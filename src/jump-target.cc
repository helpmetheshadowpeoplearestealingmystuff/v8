// Copyright 2009 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#include "codegen-inl.h"
#include "jump-target-inl.h"
#include "register-allocator-inl.h"

namespace v8 { namespace internal {

// -------------------------------------------------------------------------
// JumpTarget implementation.

bool JumpTarget::compiling_deferred_code_ = false;


void JumpTarget::Unuse() {
  reaching_frames_.Clear();
  merge_labels_.Clear();
  entry_frame_ = NULL;
  entry_label_.Unuse();
}


void JumpTarget::ComputeEntryFrame(int mergable_elements) {
  // Given: a collection of frames reaching by forward CFG edges and
  // the directionality of the block.  Compute: an entry frame for the
  // block.

  Counters::compute_entry_frame.Increment();
#ifdef DEBUG
  if (compiling_deferred_code_) {
    ASSERT(reaching_frames_.length() > 1);
    VirtualFrame* frame = reaching_frames_[0];
    bool all_identical = true;
    for (int i = 1; i < reaching_frames_.length(); i++) {
      if (!frame->Equals(reaching_frames_[i])) {
        all_identical = false;
        break;
      }
    }
    ASSERT(!all_identical || all_identical);
  }
#endif

  // Choose an initial frame.
  VirtualFrame* initial_frame = reaching_frames_[0];

  // A list of pointers to frame elements in the entry frame.  NULL
  // indicates that the element has not yet been determined.
  int length = initial_frame->elements_.length();
  ZoneList<FrameElement*> elements(length);

  // Convert the number of mergable elements (counted from the top
  // down) to a frame high-water mark (counted from the bottom up).
  // Elements strictly above the high-water index will be mergable in
  // entry frames for bidirectional jump targets.
  int high_water_mark = (mergable_elements == kAllElements)
      ? VirtualFrame::kIllegalIndex  // All frame indices are above this.
      : length - mergable_elements - 1;  // Top index if m_e == 0.

  // Initially populate the list of elements based on the initial
  // frame.
  for (int i = 0; i < length; i++) {
    FrameElement element = initial_frame->elements_[i];
    // We do not allow copies or constants in bidirectional frames.  All
    // elements above the water mark on bidirectional frames have
    // unknown static types.
    if (direction_ == BIDIRECTIONAL && i > high_water_mark) {
      if (element.is_constant() || element.is_copy()) {
        elements.Add(NULL);
        continue;
      }
      // It's safe to change the static type on the initial frame
      // element, see comment in JumpTarget::Combine.
      initial_frame->elements_[i].set_static_type(StaticType::unknown());
    }
    elements.Add(&initial_frame->elements_[i]);
  }

  // Compute elements based on the other reaching frames.
  if (reaching_frames_.length() > 1) {
    for (int i = 0; i < length; i++) {
      FrameElement* element = elements[i];
      for (int j = 1; j < reaching_frames_.length(); j++) {
        // Element computation is monotonic: new information will not
        // change our decision about undetermined or invalid elements.
        if (element == NULL || !element->is_valid()) break;

        element = element->Combine(&reaching_frames_[j]->elements_[i]);
      }
      elements[i] = element;
    }
  }

  // Build the new frame.  A freshly allocated frame has memory elements
  // for the parameters and some platform-dependent elements (e.g.,
  // return address).  Replace those first.
  entry_frame_ = new VirtualFrame();
  int index = 0;
  for (; index < entry_frame_->elements_.length(); index++) {
    FrameElement* target = elements[index];
    // If the element is determined, set it now.  Count registers.  Mark
    // elements as copied exactly when they have a copy.  Undetermined
    // elements are initially recorded as if in memory.
    if (target != NULL) {
      entry_frame_->elements_[index] = *target;
      InitializeEntryElement(index, target);
    }
  }
  // Then fill in the rest of the frame with new elements.
  for (; index < length; index++) {
    FrameElement* target = elements[index];
    if (target == NULL) {
      entry_frame_->elements_.Add(FrameElement::MemoryElement());
    } else {
      entry_frame_->elements_.Add(*target);
      InitializeEntryElement(index, target);
    }
  }

  // Allocate any still-undetermined frame elements to registers or
  // memory, from the top down.
  for (int i = length - 1; i >= 0; i--) {
    if (elements[i] == NULL) {
      // Loop over all the reaching frames to check whether the element
      // is synced on all frames, to count the registers it occupies,
      // and to compute a merged static type.
      bool is_synced = true;
      RegisterFile candidate_registers;
      int best_count = kMinInt;
      int best_reg_code = no_reg.code_;

      StaticType type;  // Initially invalid.
      if (direction_ != BIDIRECTIONAL || i < high_water_mark) {
        type = reaching_frames_[0]->elements_[i].static_type();
      }

      for (int j = 0; j < reaching_frames_.length(); j++) {
        FrameElement element = reaching_frames_[j]->elements_[i];
        is_synced = is_synced && element.is_synced();
        if (element.is_register() && !entry_frame_->is_used(element.reg())) {
          // Count the register occurrence and remember it if better
          // than the previous best.
          candidate_registers.Use(element.reg());
          if (candidate_registers.count(element.reg()) > best_count) {
            best_count = candidate_registers.count(element.reg());
            best_reg_code = element.reg().code();
          }
        }
        type = type.merge(element.static_type());
      }

      // If the value is synced on all frames, put it in memory.  This
      // costs nothing at the merge code but will incur a
      // memory-to-register move when the value is needed later.
      if (is_synced) {
        // Already recorded as a memory element.
        entry_frame_->elements_[i].set_static_type(type);
        continue;
      }

      // Try to put it in a register.  If there was no best choice
      // consider any free register.
      if (best_reg_code == no_reg.code_) {
        for (int j = 0; j < kNumRegisters; j++) {
          if (!entry_frame_->is_used(j) && !RegisterAllocator::IsReserved(j)) {
            best_reg_code = j;
            break;
          }
        }
      }

      if (best_reg_code == no_reg.code_) {
        // If there was no register found, the element is already
        // recorded as in memory.
        entry_frame_->elements_[i].set_static_type(type);
      } else {
        // If there was a register choice, use it.  Preserve the copied
        // flag on the element.  Set the static type as computed.
        bool is_copied = entry_frame_->elements_[i].is_copied();
        Register reg = { best_reg_code };
        entry_frame_->elements_[i] =
            FrameElement::RegisterElement(reg,
                                          FrameElement::NOT_SYNCED);
        if (is_copied) entry_frame_->elements_[i].set_copied();
        entry_frame_->elements_[i].set_static_type(type);
        entry_frame_->register_locations_[best_reg_code] = i;
      }
    }
  }

  // The stack pointer is at the highest synced element or the base of
  // the expression stack.
  int stack_pointer = length - 1;
  while (stack_pointer >= entry_frame_->expression_base_index() &&
         !entry_frame_->elements_[stack_pointer].is_synced()) {
    stack_pointer--;
  }
  entry_frame_->stack_pointer_ = stack_pointer;
}


void JumpTarget::Jump() {
  DoJump();
}


void JumpTarget::Jump(Result* arg) {
  ASSERT(cgen()->has_valid_frame());

  cgen()->frame()->Push(arg);
  DoJump();
}


void JumpTarget::Jump(Result* arg0, Result* arg1) {
  ASSERT(cgen()->has_valid_frame());

  cgen()->frame()->Push(arg0);
  cgen()->frame()->Push(arg1);
  DoJump();
}


void JumpTarget::Jump(Result* arg0, Result* arg1, Result* arg2) {
  ASSERT(cgen()->has_valid_frame());

  cgen()->frame()->Push(arg0);
  cgen()->frame()->Push(arg1);
  cgen()->frame()->Push(arg2);
  DoJump();
}


void JumpTarget::Branch(Condition cc, Hint hint) {
  DoBranch(cc, hint);
}


#ifdef DEBUG
#define DECLARE_ARGCHECK_VARS(name)                                \
  Result::Type name##_type = name->type();                         \
  Register name##_reg = name->is_register() ? name->reg() : no_reg

#define ASSERT_ARGCHECK(name)                                \
  ASSERT(name->type() == name##_type);                       \
  ASSERT(!name->is_register() || name->reg().is(name##_reg))

#else
#define DECLARE_ARGCHECK_VARS(name) do {} while (false)

#define ASSERT_ARGCHECK(name) do {} while (false)
#endif

void JumpTarget::Branch(Condition cc, Result* arg, Hint hint) {
  ASSERT(cgen()->has_valid_frame());

  // We want to check that non-frame registers at the call site stay in
  // the same registers on the fall-through branch.
  DECLARE_ARGCHECK_VARS(arg);

  cgen()->frame()->Push(arg);
  DoBranch(cc, hint);
  *arg = cgen()->frame()->Pop();

  ASSERT_ARGCHECK(arg);
}


void JumpTarget::Branch(Condition cc, Result* arg0, Result* arg1, Hint hint) {
  ASSERT(cgen()->frame() != NULL);

  // We want to check that non-frame registers at the call site stay in
  // the same registers on the fall-through branch.
  DECLARE_ARGCHECK_VARS(arg0);
  DECLARE_ARGCHECK_VARS(arg1);

  cgen()->frame()->Push(arg0);
  cgen()->frame()->Push(arg1);
  DoBranch(cc, hint);
  *arg1 = cgen()->frame()->Pop();
  *arg0 = cgen()->frame()->Pop();

  ASSERT_ARGCHECK(arg0);
  ASSERT_ARGCHECK(arg1);
}


void JumpTarget::Branch(Condition cc,
                        Result* arg0,
                        Result* arg1,
                        Result* arg2,
                        Hint hint) {
  ASSERT(cgen()->frame() != NULL);

  // We want to check that non-frame registers at the call site stay in
  // the same registers on the fall-through branch.
  DECLARE_ARGCHECK_VARS(arg0);
  DECLARE_ARGCHECK_VARS(arg1);
  DECLARE_ARGCHECK_VARS(arg2);

  cgen()->frame()->Push(arg0);
  cgen()->frame()->Push(arg1);
  cgen()->frame()->Push(arg2);
  DoBranch(cc, hint);
  *arg2 = cgen()->frame()->Pop();
  *arg1 = cgen()->frame()->Pop();
  *arg0 = cgen()->frame()->Pop();

  ASSERT_ARGCHECK(arg0);
  ASSERT_ARGCHECK(arg1);
  ASSERT_ARGCHECK(arg2);
}


void JumpTarget::Branch(Condition cc,
                        Result* arg0,
                        Result* arg1,
                        Result* arg2,
                        Result* arg3,
                        Hint hint) {
  ASSERT(cgen()->frame() != NULL);

  // We want to check that non-frame registers at the call site stay in
  // the same registers on the fall-through branch.
  DECLARE_ARGCHECK_VARS(arg0);
  DECLARE_ARGCHECK_VARS(arg1);
  DECLARE_ARGCHECK_VARS(arg2);
  DECLARE_ARGCHECK_VARS(arg3);

  cgen()->frame()->Push(arg0);
  cgen()->frame()->Push(arg1);
  cgen()->frame()->Push(arg2);
  cgen()->frame()->Push(arg3);
  DoBranch(cc, hint);
  *arg3 = cgen()->frame()->Pop();
  *arg2 = cgen()->frame()->Pop();
  *arg1 = cgen()->frame()->Pop();
  *arg0 = cgen()->frame()->Pop();

  ASSERT_ARGCHECK(arg0);
  ASSERT_ARGCHECK(arg1);
  ASSERT_ARGCHECK(arg2);
  ASSERT_ARGCHECK(arg3);
}


void BreakTarget::Branch(Condition cc, Result* arg, Hint hint) {
  ASSERT(cgen()->has_valid_frame());

  int count = cgen()->frame()->height() - expected_height_;
  if (count > 0) {
    // We negate and branch here rather than using DoBranch's negate
    // and branch.  This gives us a hook to remove statement state
    // from the frame.
    JumpTarget fall_through;
    // Branch to fall through will not negate, because it is a
    // forward-only target.
    fall_through.Branch(NegateCondition(cc), NegateHint(hint));
    Jump(arg);  // May emit merge code here.
    fall_through.Bind();
  } else {
    DECLARE_ARGCHECK_VARS(arg);
    cgen()->frame()->Push(arg);
    DoBranch(cc, hint);
    *arg = cgen()->frame()->Pop();
    ASSERT_ARGCHECK(arg);
  }
}

#undef DECLARE_ARGCHECK_VARS
#undef ASSERT_ARGCHECK


void JumpTarget::Bind(int mergable_elements) {
  DoBind(mergable_elements);
}


void JumpTarget::Bind(Result* arg, int mergable_elements) {
  if (cgen()->has_valid_frame()) {
    cgen()->frame()->Push(arg);
  }
  DoBind(mergable_elements);
  *arg = cgen()->frame()->Pop();
}


void JumpTarget::Bind(Result* arg0, Result* arg1, int mergable_elements) {
  if (cgen()->has_valid_frame()) {
    cgen()->frame()->Push(arg0);
    cgen()->frame()->Push(arg1);
  }
  DoBind(mergable_elements);
  *arg1 = cgen()->frame()->Pop();
  *arg0 = cgen()->frame()->Pop();
}


void JumpTarget::Bind(Result* arg0,
                      Result* arg1,
                      Result* arg2,
                      int mergable_elements) {
  if (cgen()->has_valid_frame()) {
    cgen()->frame()->Push(arg0);
    cgen()->frame()->Push(arg1);
    cgen()->frame()->Push(arg2);
  }
  DoBind(mergable_elements);
  *arg2 = cgen()->frame()->Pop();
  *arg1 = cgen()->frame()->Pop();
  *arg0 = cgen()->frame()->Pop();
}


void JumpTarget::Bind(Result* arg0,
                      Result* arg1,
                      Result* arg2,
                      Result* arg3,
                      int mergable_elements) {
  if (cgen()->has_valid_frame()) {
    cgen()->frame()->Push(arg0);
    cgen()->frame()->Push(arg1);
    cgen()->frame()->Push(arg2);
    cgen()->frame()->Push(arg3);
  }
  DoBind(mergable_elements);
  *arg3 = cgen()->frame()->Pop();
  *arg2 = cgen()->frame()->Pop();
  *arg1 = cgen()->frame()->Pop();
  *arg0 = cgen()->frame()->Pop();
}


void JumpTarget::AddReachingFrame(VirtualFrame* frame) {
  ASSERT(reaching_frames_.length() == merge_labels_.length());
  ASSERT(entry_frame_ == NULL);
  Label fresh;
  merge_labels_.Add(fresh);
  reaching_frames_.Add(frame);
}


// -------------------------------------------------------------------------
// BreakTarget implementation.

void BreakTarget::set_direction(Directionality direction) {
  JumpTarget::set_direction(direction);
  ASSERT(cgen()->has_valid_frame());
  expected_height_ = cgen()->frame()->height();
}


void BreakTarget::CopyTo(BreakTarget* destination) {
  ASSERT(destination != NULL);
  destination->direction_ = direction_;
  destination->reaching_frames_.Rewind(0);
  destination->reaching_frames_.AddAll(reaching_frames_);
  destination->merge_labels_.Rewind(0);
  destination->merge_labels_.AddAll(merge_labels_);
  destination->entry_frame_ = entry_frame_;
  destination->entry_label_ = entry_label_;
  destination->expected_height_ = expected_height_;
}


void BreakTarget::Jump() {
  ASSERT(cgen()->has_valid_frame());

  // Drop leftover statement state from the frame before merging.
  cgen()->frame()->ForgetElements(cgen()->frame()->height() - expected_height_);
  DoJump();
}


void BreakTarget::Jump(Result* arg) {
  ASSERT(cgen()->has_valid_frame());

  // Drop leftover statement state from the frame before merging.
  cgen()->frame()->ForgetElements(cgen()->frame()->height() - expected_height_);
  cgen()->frame()->Push(arg);
  DoJump();
}


void BreakTarget::Branch(Condition cc, Hint hint) {
  ASSERT(cgen()->has_valid_frame());

  int count = cgen()->frame()->height() - expected_height_;
  if (count > 0) {
    // We negate and branch here rather than using DoBranch's negate
    // and branch.  This gives us a hook to remove statement state
    // from the frame.
    JumpTarget fall_through;
    // Branch to fall through will not negate, because it is a
    // forward-only target.
    fall_through.Branch(NegateCondition(cc), NegateHint(hint));
    Jump();  // May emit merge code here.
    fall_through.Bind();
  } else {
    DoBranch(cc, hint);
  }
}


void BreakTarget::Bind(int mergable_elements) {
#ifdef DEBUG
  // All the forward-reaching frames should have been adjusted at the
  // jumps to this target.
  for (int i = 0; i < reaching_frames_.length(); i++) {
    ASSERT(reaching_frames_[i] == NULL ||
           reaching_frames_[i]->height() == expected_height_);
  }
#endif
  // Drop leftover statement state from the frame before merging, even
  // on the fall through.  This is so we can bind the return target
  // with state on the frame.
  if (cgen()->has_valid_frame()) {
    int count = cgen()->frame()->height() - expected_height_;
    cgen()->frame()->ForgetElements(count);
  }
  DoBind(mergable_elements);
}


void BreakTarget::Bind(Result* arg, int mergable_elements) {
#ifdef DEBUG
  // All the forward-reaching frames should have been adjusted at the
  // jumps to this target.
  for (int i = 0; i < reaching_frames_.length(); i++) {
    ASSERT(reaching_frames_[i] == NULL ||
           reaching_frames_[i]->height() == expected_height_ + 1);
  }
#endif
  // Drop leftover statement state from the frame before merging, even
  // on the fall through.  This is so we can bind the return target
  // with state on the frame.
  if (cgen()->has_valid_frame()) {
    int count = cgen()->frame()->height() - expected_height_;
    cgen()->frame()->ForgetElements(count);
    cgen()->frame()->Push(arg);
  }
  DoBind(mergable_elements);
  *arg = cgen()->frame()->Pop();
}


// -------------------------------------------------------------------------
// ShadowTarget implementation.

ShadowTarget::ShadowTarget(BreakTarget* shadowed) {
  ASSERT(shadowed != NULL);
  other_target_ = shadowed;

#ifdef DEBUG
  is_shadowing_ = true;
#endif
  // While shadowing this shadow target saves the state of the original.
  shadowed->CopyTo(this);

  // The original's state is reset.
  shadowed->Unuse();
  ASSERT(cgen()->has_valid_frame());
  shadowed->set_expected_height(cgen()->frame()->height());
}


void ShadowTarget::StopShadowing() {
  ASSERT(is_shadowing_);

  // The states of this target, which was shadowed, and the original
  // target, which was shadowing, are swapped.
  BreakTarget temp;
  other_target_->CopyTo(&temp);
  CopyTo(other_target_);
  temp.CopyTo(this);
  temp.Unuse();

#ifdef DEBUG
  is_shadowing_ = false;
#endif
}


} }  // namespace v8::internal
