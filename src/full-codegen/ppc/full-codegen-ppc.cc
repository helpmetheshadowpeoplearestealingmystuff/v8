// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_PPC

#include "src/ast/compile-time-value.h"
#include "src/ast/scopes.h"
#include "src/builtins/builtins-constructor.h"
#include "src/code-factory.h"
#include "src/code-stubs.h"
#include "src/codegen.h"
#include "src/compilation-info.h"
#include "src/compiler.h"
#include "src/debug/debug.h"
#include "src/full-codegen/full-codegen.h"
#include "src/ic/ic.h"

#include "src/ppc/code-stubs-ppc.h"
#include "src/ppc/macro-assembler-ppc.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm())

// A patch site is a location in the code which it is possible to patch. This
// class has a number of methods to emit the code which is patchable and the
// method EmitPatchInfo to record a marker back to the patchable code. This
// marker is a cmpi rx, #yyy instruction, and x * 0x0000ffff + yyy (raw 16 bit
// immediate value is used) is the delta from the pc to the first instruction of
// the patchable code.
// See PatchInlinedSmiCode in ic-ppc.cc for the code that patches it
class JumpPatchSite BASE_EMBEDDED {
 public:
  explicit JumpPatchSite(MacroAssembler* masm) : masm_(masm) {
#ifdef DEBUG
    info_emitted_ = false;
#endif
  }

  ~JumpPatchSite() { DCHECK(patch_site_.is_bound() == info_emitted_); }

  // When initially emitting this ensure that a jump is always generated to skip
  // the inlined smi code.
  void EmitJumpIfNotSmi(Register reg, Label* target) {
    DCHECK(!patch_site_.is_bound() && !info_emitted_);
    Assembler::BlockTrampolinePoolScope block_trampoline_pool(masm_);
    __ bind(&patch_site_);
    __ cmp(reg, reg, cr0);
    __ beq(target, cr0);  // Always taken before patched.
  }

  // When initially emitting this ensure that a jump is never generated to skip
  // the inlined smi code.
  void EmitJumpIfSmi(Register reg, Label* target) {
    Assembler::BlockTrampolinePoolScope block_trampoline_pool(masm_);
    DCHECK(!patch_site_.is_bound() && !info_emitted_);
    __ bind(&patch_site_);
    __ cmp(reg, reg, cr0);
    __ bne(target, cr0);  // Never taken before patched.
  }

  void EmitPatchInfo() {
    if (patch_site_.is_bound()) {
      int delta_to_patch_site = masm_->InstructionsGeneratedSince(&patch_site_);
      Register reg;
      // I believe this is using reg as the high bits of of the offset
      reg.set_code(delta_to_patch_site / kOff16Mask);
      __ cmpi(reg, Operand(delta_to_patch_site % kOff16Mask));
#ifdef DEBUG
      info_emitted_ = true;
#endif
    } else {
      __ nop();  // Signals no inlined code.
    }
  }

 private:
  MacroAssembler* masm() { return masm_; }
  MacroAssembler* masm_;
  Label patch_site_;
#ifdef DEBUG
  bool info_emitted_;
#endif
};


// Generate code for a JS function.  On entry to the function the receiver
// and arguments have been pushed on the stack left to right.  The actual
// argument count matches the formal parameter count expected by the
// function.
//
// The live registers are:
//   o r4: the JS function object being called (i.e., ourselves)
//   o r6: the new target value
//   o cp: our context
//   o fp: our caller's frame pointer (aka r31)
//   o sp: stack pointer
//   o lr: return address
//   o ip: our own function entry (required by the prologue)
//
// The function builds a JS frame.  Please see JavaScriptFrameConstants in
// frames-ppc.h for its layout.
void FullCodeGenerator::Generate() {
  CompilationInfo* info = info_;
  profiling_counter_ = isolate()->factory()->NewCell(
      Handle<Smi>(Smi::FromInt(FLAG_interrupt_budget), isolate()));
  SetFunctionPosition(literal());
  Comment cmnt(masm_, "[ function compiled by full code generator");

  ProfileEntryHookStub::MaybeCallEntryHook(masm_);

  if (FLAG_debug_code && info->ExpectsJSReceiverAsReceiver()) {
    int receiver_offset = info->scope()->num_parameters() * kPointerSize;
    __ LoadP(r5, MemOperand(sp, receiver_offset), r0);
    __ AssertNotSmi(r5);
    __ CompareObjectType(r5, r5, no_reg, FIRST_JS_RECEIVER_TYPE);
    __ Assert(ge, kSloppyFunctionExpectsJSReceiverReceiver);
  }

  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done below).
  FrameScope frame_scope(masm_, StackFrame::MANUAL);
  int prologue_offset = masm_->pc_offset();

  if (prologue_offset) {
    // Prologue logic requires it's starting address in ip and the
    // corresponding offset from the function entry.
    prologue_offset += Instruction::kInstrSize;
    __ addi(ip, ip, Operand(prologue_offset));
  }
  info->set_prologue_offset(prologue_offset);
  __ Prologue(info->GeneratePreagedPrologue(), ip, prologue_offset);

  // Increment invocation count for the function.
  {
    Comment cmnt(masm_, "[ Increment invocation count");
    __ LoadP(r7, FieldMemOperand(r4, JSFunction::kFeedbackVectorOffset));
    __ LoadP(r8, FieldMemOperand(r7, TypeFeedbackVector::kInvocationCountIndex *
                                             kPointerSize +
                                         TypeFeedbackVector::kHeaderSize));
    __ AddSmiLiteral(r8, r8, Smi::FromInt(1), r0);
    __ StoreP(r8,
              FieldMemOperand(
                  r7, TypeFeedbackVector::kInvocationCountIndex * kPointerSize +
                          TypeFeedbackVector::kHeaderSize),
              r0);
  }

  {
    Comment cmnt(masm_, "[ Allocate locals");
    int locals_count = info->scope()->num_stack_slots();
    OperandStackDepthIncrement(locals_count);
    if (locals_count > 0) {
      if (locals_count >= 128) {
        Label ok;
        __ Add(ip, sp, -(locals_count * kPointerSize), r0);
        __ LoadRoot(r5, Heap::kRealStackLimitRootIndex);
        __ cmpl(ip, r5);
        __ bc_short(ge, &ok);
        __ CallRuntime(Runtime::kThrowStackOverflow);
        __ bind(&ok);
      }
      __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
      int kMaxPushes = FLAG_optimize_for_size ? 4 : 32;
      if (locals_count >= kMaxPushes) {
        int loop_iterations = locals_count / kMaxPushes;
        __ mov(r5, Operand(loop_iterations));
        __ mtctr(r5);
        Label loop_header;
        __ bind(&loop_header);
        // Do pushes.
        for (int i = 0; i < kMaxPushes; i++) {
          __ push(ip);
        }
        // Continue loop if not done.
        __ bdnz(&loop_header);
      }
      int remaining = locals_count % kMaxPushes;
      // Emit the remaining pushes.
      for (int i = 0; i < remaining; i++) {
        __ push(ip);
      }
    }
  }

  bool function_in_register_r4 = true;

  // Possibly allocate a local context.
  if (info->scope()->NeedsContext()) {
    // Argument to NewContext is the function, which is still in r4.
    Comment cmnt(masm_, "[ Allocate context");
    bool need_write_barrier = true;
    int slots = info->scope()->num_heap_slots() - Context::MIN_CONTEXT_SLOTS;
    if (info->scope()->is_script_scope()) {
      __ push(r4);
      __ Push(info->scope()->scope_info());
      __ CallRuntime(Runtime::kNewScriptContext);
      PrepareForBailoutForId(BailoutId::ScriptContext(),
                             BailoutState::TOS_REGISTER);
      // The new target value is not used, clobbering is safe.
      DCHECK_NULL(info->scope()->new_target_var());
    } else {
      if (info->scope()->new_target_var() != nullptr) {
        __ push(r6);  // Preserve new target.
      }
      if (slots <=
          ConstructorBuiltinsAssembler::MaximumFunctionContextSlots()) {
        Callable callable = CodeFactory::FastNewFunctionContext(
            isolate(), info->scope()->scope_type());
        __ mov(FastNewFunctionContextDescriptor::SlotsRegister(),
               Operand(slots));
        __ Call(callable.code(), RelocInfo::CODE_TARGET);
        // Result of the FastNewFunctionContext builtin is always in new space.
        need_write_barrier = false;
      } else {
        __ push(r4);
        __ Push(Smi::FromInt(info->scope()->scope_type()));
        __ CallRuntime(Runtime::kNewFunctionContext);
      }
      if (info->scope()->new_target_var() != nullptr) {
        __ pop(r6);  // Preserve new target.
      }
    }
    function_in_register_r4 = false;
    // Context is returned in r3.  It replaces the context passed to us.
    // It's saved in the stack and kept live in cp.
    __ mr(cp, r3);
    __ StoreP(r3, MemOperand(fp, StandardFrameConstants::kContextOffset));
    // Copy any necessary parameters into the context.
    int num_parameters = info->scope()->num_parameters();
    int first_parameter = info->scope()->has_this_declaration() ? -1 : 0;
    for (int i = first_parameter; i < num_parameters; i++) {
      Variable* var =
          (i == -1) ? info->scope()->receiver() : info->scope()->parameter(i);
      if (var->IsContextSlot()) {
        int parameter_offset = StandardFrameConstants::kCallerSPOffset +
                               (num_parameters - 1 - i) * kPointerSize;
        // Load parameter from stack.
        __ LoadP(r3, MemOperand(fp, parameter_offset), r0);
        // Store it in the context.
        MemOperand target = ContextMemOperand(cp, var->index());
        __ StoreP(r3, target, r0);

        // Update the write barrier.
        if (need_write_barrier) {
          __ RecordWriteContextSlot(cp, target.offset(), r3, r5,
                                    kLRHasBeenSaved, kDontSaveFPRegs);
        } else if (FLAG_debug_code) {
          Label done;
          __ JumpIfInNewSpace(cp, r3, &done);
          __ Abort(kExpectedNewSpaceObject);
          __ bind(&done);
        }
      }
    }
  }

  // Register holding this function and new target are both trashed in case we
  // bailout here. But since that can happen only when new target is not used
  // and we allocate a context, the value of |function_in_register| is correct.
  PrepareForBailoutForId(BailoutId::FunctionContext(),
                         BailoutState::NO_REGISTERS);

  // We don't support new.target and rest parameters here.
  DCHECK_NULL(info->scope()->new_target_var());
  DCHECK_NULL(info->scope()->rest_parameter());
  DCHECK_NULL(info->scope()->this_function_var());

  Variable* arguments = info->scope()->arguments();
  if (arguments != NULL) {
    // Function uses arguments object.
    Comment cmnt(masm_, "[ Allocate arguments object");
    if (!function_in_register_r4) {
      // Load this again, if it's used by the local context below.
      __ LoadP(r4, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
    }
    if (is_strict(language_mode()) || !has_simple_parameters()) {
      FastNewStrictArgumentsStub stub(isolate());
      __ CallStub(&stub);
    } else if (literal()->has_duplicate_parameters()) {
      __ Push(r4);
      __ CallRuntime(Runtime::kNewSloppyArguments_Generic);
    } else {
      FastNewSloppyArgumentsStub stub(isolate());
      __ CallStub(&stub);
    }

    SetVar(arguments, r3, r4, r5);
  }

  if (FLAG_trace) {
    __ CallRuntime(Runtime::kTraceEnter);
  }

  // Visit the declarations and body.
  PrepareForBailoutForId(BailoutId::FunctionEntry(),
                         BailoutState::NO_REGISTERS);
  {
    Comment cmnt(masm_, "[ Declarations");
    VisitDeclarations(scope()->declarations());
  }

  // Assert that the declarations do not use ICs. Otherwise the debugger
  // won't be able to redirect a PC at an IC to the correct IC in newly
  // recompiled code.
  DCHECK_EQ(0, ic_total_count_);

  {
    Comment cmnt(masm_, "[ Stack check");
    PrepareForBailoutForId(BailoutId::Declarations(),
                           BailoutState::NO_REGISTERS);
    Label ok;
    __ LoadRoot(ip, Heap::kStackLimitRootIndex);
    __ cmpl(sp, ip);
    __ bc_short(ge, &ok);
    __ Call(isolate()->builtins()->StackCheck(), RelocInfo::CODE_TARGET);
    __ bind(&ok);
  }

  {
    Comment cmnt(masm_, "[ Body");
    DCHECK(loop_depth() == 0);
    VisitStatements(literal()->body());
    DCHECK(loop_depth() == 0);
  }

  // Always emit a 'return undefined' in case control fell off the end of
  // the body.
  {
    Comment cmnt(masm_, "[ return <undefined>;");
    __ LoadRoot(r3, Heap::kUndefinedValueRootIndex);
  }
  EmitReturnSequence();

  if (HasStackOverflow()) {
    masm_->AbortConstantPoolBuilding();
  }
}


void FullCodeGenerator::ClearAccumulator() {
  __ LoadSmiLiteral(r3, Smi::kZero);
}


void FullCodeGenerator::EmitProfilingCounterDecrement(int delta) {
  __ mov(r5, Operand(profiling_counter_));
  __ LoadP(r6, FieldMemOperand(r5, Cell::kValueOffset));
  __ SubSmiLiteral(r6, r6, Smi::FromInt(delta), r0);
  __ StoreP(r6, FieldMemOperand(r5, Cell::kValueOffset), r0);
}


void FullCodeGenerator::EmitProfilingCounterReset() {
  int reset_value = FLAG_interrupt_budget;
  __ mov(r5, Operand(profiling_counter_));
  __ LoadSmiLiteral(r6, Smi::FromInt(reset_value));
  __ StoreP(r6, FieldMemOperand(r5, Cell::kValueOffset), r0);
}


void FullCodeGenerator::EmitBackEdgeBookkeeping(IterationStatement* stmt,
                                                Label* back_edge_target) {
  Comment cmnt(masm_, "[ Back edge bookkeeping");
  Label ok;

  DCHECK(back_edge_target->is_bound());
  int distance = masm_->SizeOfCodeGeneratedSince(back_edge_target) +
                 kCodeSizeMultiplier / 2;
  int weight = Min(kMaxBackEdgeWeight, Max(1, distance / kCodeSizeMultiplier));
  EmitProfilingCounterDecrement(weight);
  {
    Assembler::BlockTrampolinePoolScope block_trampoline_pool(masm_);
    Assembler::BlockConstantPoolEntrySharingScope prevent_entry_sharing(masm_);
    // BackEdgeTable::PatchAt manipulates this sequence.
    __ cmpi(r6, Operand::Zero());
    __ bc_short(ge, &ok);
    __ Call(isolate()->builtins()->InterruptCheck(), RelocInfo::CODE_TARGET);

    // Record a mapping of this PC offset to the OSR id.  This is used to find
    // the AST id from the unoptimized code in order to use it as a key into
    // the deoptimization input data found in the optimized code.
    RecordBackEdge(stmt->OsrEntryId());
  }
  EmitProfilingCounterReset();

  __ bind(&ok);
  PrepareForBailoutForId(stmt->EntryId(), BailoutState::NO_REGISTERS);
  // Record a mapping of the OSR id to this PC.  This is used if the OSR
  // entry becomes the target of a bailout.  We don't expect it to be, but
  // we want it to work if it is.
  PrepareForBailoutForId(stmt->OsrEntryId(), BailoutState::NO_REGISTERS);
}

void FullCodeGenerator::EmitProfilingCounterHandlingForReturnSequence(
    bool is_tail_call) {
  // Pretend that the exit is a backwards jump to the entry.
  int weight = 1;
  if (info_->ShouldSelfOptimize()) {
    weight = FLAG_interrupt_budget / FLAG_self_opt_count;
  } else {
    int distance = masm_->pc_offset() + kCodeSizeMultiplier / 2;
    weight = Min(kMaxBackEdgeWeight, Max(1, distance / kCodeSizeMultiplier));
  }
  EmitProfilingCounterDecrement(weight);
  Label ok;
  __ cmpi(r6, Operand::Zero());
  __ bge(&ok);
  // Don't need to save result register if we are going to do a tail call.
  if (!is_tail_call) {
    __ push(r3);
  }
  __ Call(isolate()->builtins()->InterruptCheck(), RelocInfo::CODE_TARGET);
  if (!is_tail_call) {
    __ pop(r3);
  }
  EmitProfilingCounterReset();
  __ bind(&ok);
}

void FullCodeGenerator::EmitReturnSequence() {
  Comment cmnt(masm_, "[ Return sequence");
  if (return_label_.is_bound()) {
    __ b(&return_label_);
  } else {
    __ bind(&return_label_);
    if (FLAG_trace) {
      // Push the return value on the stack as the parameter.
      // Runtime::TraceExit returns its parameter in r3
      __ push(r3);
      __ CallRuntime(Runtime::kTraceExit);
    }
    EmitProfilingCounterHandlingForReturnSequence(false);

    // Make sure that the constant pool is not emitted inside of the return
    // sequence.
    {
      Assembler::BlockTrampolinePoolScope block_trampoline_pool(masm_);
      int32_t arg_count = info_->scope()->num_parameters() + 1;
      int32_t sp_delta = arg_count * kPointerSize;
      SetReturnPosition(literal());
      __ LeaveFrame(StackFrame::JAVA_SCRIPT, sp_delta);
      __ blr();
    }
  }
}

void FullCodeGenerator::RestoreContext() {
  __ LoadP(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
}

void FullCodeGenerator::StackValueContext::Plug(Variable* var) const {
  DCHECK(var->IsStackAllocated() || var->IsContextSlot());
  codegen()->GetVar(result_register(), var);
  codegen()->PushOperand(result_register());
}


void FullCodeGenerator::EffectContext::Plug(Heap::RootListIndex index) const {}


void FullCodeGenerator::AccumulatorValueContext::Plug(
    Heap::RootListIndex index) const {
  __ LoadRoot(result_register(), index);
}


void FullCodeGenerator::StackValueContext::Plug(
    Heap::RootListIndex index) const {
  __ LoadRoot(result_register(), index);
  codegen()->PushOperand(result_register());
}


void FullCodeGenerator::TestContext::Plug(Heap::RootListIndex index) const {
  codegen()->PrepareForBailoutBeforeSplit(condition(), true, true_label_,
                                          false_label_);
  if (index == Heap::kUndefinedValueRootIndex ||
      index == Heap::kNullValueRootIndex ||
      index == Heap::kFalseValueRootIndex) {
    if (false_label_ != fall_through_) __ b(false_label_);
  } else if (index == Heap::kTrueValueRootIndex) {
    if (true_label_ != fall_through_) __ b(true_label_);
  } else {
    __ LoadRoot(result_register(), index);
    codegen()->DoTest(this);
  }
}


void FullCodeGenerator::EffectContext::Plug(Handle<Object> lit) const {}


void FullCodeGenerator::AccumulatorValueContext::Plug(
    Handle<Object> lit) const {
  __ mov(result_register(), Operand(lit));
}


void FullCodeGenerator::StackValueContext::Plug(Handle<Object> lit) const {
  // Immediates cannot be pushed directly.
  __ mov(result_register(), Operand(lit));
  codegen()->PushOperand(result_register());
}


void FullCodeGenerator::TestContext::Plug(Handle<Object> lit) const {
  codegen()->PrepareForBailoutBeforeSplit(condition(), true, true_label_,
                                          false_label_);
  DCHECK(lit->IsNullOrUndefined(isolate()) || !lit->IsUndetectable());
  if (lit->IsNullOrUndefined(isolate()) || lit->IsFalse(isolate())) {
    if (false_label_ != fall_through_) __ b(false_label_);
  } else if (lit->IsTrue(isolate()) || lit->IsJSObject()) {
    if (true_label_ != fall_through_) __ b(true_label_);
  } else if (lit->IsString()) {
    if (String::cast(*lit)->length() == 0) {
      if (false_label_ != fall_through_) __ b(false_label_);
    } else {
      if (true_label_ != fall_through_) __ b(true_label_);
    }
  } else if (lit->IsSmi()) {
    if (Smi::cast(*lit)->value() == 0) {
      if (false_label_ != fall_through_) __ b(false_label_);
    } else {
      if (true_label_ != fall_through_) __ b(true_label_);
    }
  } else {
    // For simplicity we always test the accumulator register.
    __ mov(result_register(), Operand(lit));
    codegen()->DoTest(this);
  }
}


void FullCodeGenerator::StackValueContext::DropAndPlug(int count,
                                                       Register reg) const {
  DCHECK(count > 0);
  if (count > 1) codegen()->DropOperands(count - 1);
  __ StoreP(reg, MemOperand(sp, 0));
}


void FullCodeGenerator::EffectContext::Plug(Label* materialize_true,
                                            Label* materialize_false) const {
  DCHECK(materialize_true == materialize_false);
  __ bind(materialize_true);
}


void FullCodeGenerator::AccumulatorValueContext::Plug(
    Label* materialize_true, Label* materialize_false) const {
  Label done;
  __ bind(materialize_true);
  __ LoadRoot(result_register(), Heap::kTrueValueRootIndex);
  __ b(&done);
  __ bind(materialize_false);
  __ LoadRoot(result_register(), Heap::kFalseValueRootIndex);
  __ bind(&done);
}


void FullCodeGenerator::StackValueContext::Plug(
    Label* materialize_true, Label* materialize_false) const {
  Label done;
  __ bind(materialize_true);
  __ LoadRoot(ip, Heap::kTrueValueRootIndex);
  __ b(&done);
  __ bind(materialize_false);
  __ LoadRoot(ip, Heap::kFalseValueRootIndex);
  __ bind(&done);
  codegen()->PushOperand(ip);
}


void FullCodeGenerator::TestContext::Plug(Label* materialize_true,
                                          Label* materialize_false) const {
  DCHECK(materialize_true == true_label_);
  DCHECK(materialize_false == false_label_);
}


void FullCodeGenerator::AccumulatorValueContext::Plug(bool flag) const {
  Heap::RootListIndex value_root_index =
      flag ? Heap::kTrueValueRootIndex : Heap::kFalseValueRootIndex;
  __ LoadRoot(result_register(), value_root_index);
}


void FullCodeGenerator::StackValueContext::Plug(bool flag) const {
  Heap::RootListIndex value_root_index =
      flag ? Heap::kTrueValueRootIndex : Heap::kFalseValueRootIndex;
  __ LoadRoot(ip, value_root_index);
  codegen()->PushOperand(ip);
}


void FullCodeGenerator::TestContext::Plug(bool flag) const {
  codegen()->PrepareForBailoutBeforeSplit(condition(), true, true_label_,
                                          false_label_);
  if (flag) {
    if (true_label_ != fall_through_) __ b(true_label_);
  } else {
    if (false_label_ != fall_through_) __ b(false_label_);
  }
}


void FullCodeGenerator::DoTest(Expression* condition, Label* if_true,
                               Label* if_false, Label* fall_through) {
  Handle<Code> ic = ToBooleanICStub::GetUninitialized(isolate());
  CallIC(ic, condition->test_id());
  __ CompareRoot(result_register(), Heap::kTrueValueRootIndex);
  Split(eq, if_true, if_false, fall_through);
}


void FullCodeGenerator::Split(Condition cond, Label* if_true, Label* if_false,
                              Label* fall_through, CRegister cr) {
  if (if_false == fall_through) {
    __ b(cond, if_true, cr);
  } else if (if_true == fall_through) {
    __ b(NegateCondition(cond), if_false, cr);
  } else {
    __ b(cond, if_true, cr);
    __ b(if_false);
  }
}


MemOperand FullCodeGenerator::StackOperand(Variable* var) {
  DCHECK(var->IsStackAllocated());
  // Offset is negative because higher indexes are at lower addresses.
  int offset = -var->index() * kPointerSize;
  // Adjust by a (parameter or local) base offset.
  if (var->IsParameter()) {
    offset += (info_->scope()->num_parameters() + 1) * kPointerSize;
  } else {
    offset += JavaScriptFrameConstants::kLocal0Offset;
  }
  return MemOperand(fp, offset);
}


MemOperand FullCodeGenerator::VarOperand(Variable* var, Register scratch) {
  DCHECK(var->IsContextSlot() || var->IsStackAllocated());
  if (var->IsContextSlot()) {
    int context_chain_length = scope()->ContextChainLength(var->scope());
    __ LoadContext(scratch, context_chain_length);
    return ContextMemOperand(scratch, var->index());
  } else {
    return StackOperand(var);
  }
}


void FullCodeGenerator::GetVar(Register dest, Variable* var) {
  // Use destination as scratch.
  MemOperand location = VarOperand(var, dest);
  __ LoadP(dest, location, r0);
}


void FullCodeGenerator::SetVar(Variable* var, Register src, Register scratch0,
                               Register scratch1) {
  DCHECK(var->IsContextSlot() || var->IsStackAllocated());
  DCHECK(!scratch0.is(src));
  DCHECK(!scratch0.is(scratch1));
  DCHECK(!scratch1.is(src));
  MemOperand location = VarOperand(var, scratch0);
  __ StoreP(src, location, r0);

  // Emit the write barrier code if the location is in the heap.
  if (var->IsContextSlot()) {
    __ RecordWriteContextSlot(scratch0, location.offset(), src, scratch1,
                              kLRHasBeenSaved, kDontSaveFPRegs);
  }
}


void FullCodeGenerator::PrepareForBailoutBeforeSplit(Expression* expr,
                                                     bool should_normalize,
                                                     Label* if_true,
                                                     Label* if_false) {
  // Only prepare for bailouts before splits if we're in a test
  // context. Otherwise, we let the Visit function deal with the
  // preparation to avoid preparing with the same AST id twice.
  if (!context()->IsTest()) return;

  Label skip;
  if (should_normalize) __ b(&skip);
  PrepareForBailout(expr, BailoutState::TOS_REGISTER);
  if (should_normalize) {
    __ LoadRoot(ip, Heap::kTrueValueRootIndex);
    __ cmp(r3, ip);
    Split(eq, if_true, if_false, NULL);
    __ bind(&skip);
  }
}


void FullCodeGenerator::EmitDebugCheckDeclarationContext(Variable* variable) {
  // The variable in the declaration always resides in the current function
  // context.
  DCHECK_EQ(0, scope()->ContextChainLength(variable->scope()));
  if (FLAG_debug_code) {
    // Check that we're not inside a with or catch context.
    __ LoadP(r4, FieldMemOperand(cp, HeapObject::kMapOffset));
    __ CompareRoot(r4, Heap::kWithContextMapRootIndex);
    __ Check(ne, kDeclarationInWithContext);
    __ CompareRoot(r4, Heap::kCatchContextMapRootIndex);
    __ Check(ne, kDeclarationInCatchContext);
  }
}


void FullCodeGenerator::VisitVariableDeclaration(
    VariableDeclaration* declaration) {
  VariableProxy* proxy = declaration->proxy();
  Variable* variable = proxy->var();
  switch (variable->location()) {
    case VariableLocation::UNALLOCATED: {
      DCHECK(!variable->binding_needs_init());
      globals_->Add(variable->name(), zone());
      FeedbackVectorSlot slot = proxy->VariableFeedbackSlot();
      DCHECK(!slot.IsInvalid());
      globals_->Add(handle(Smi::FromInt(slot.ToInt()), isolate()), zone());
      globals_->Add(isolate()->factory()->undefined_value(), zone());
      globals_->Add(isolate()->factory()->undefined_value(), zone());
      break;
    }
    case VariableLocation::PARAMETER:
    case VariableLocation::LOCAL:
      if (variable->binding_needs_init()) {
        Comment cmnt(masm_, "[ VariableDeclaration");
        __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
        __ StoreP(ip, StackOperand(variable));
      }
      break;

    case VariableLocation::CONTEXT:
      if (variable->binding_needs_init()) {
        Comment cmnt(masm_, "[ VariableDeclaration");
        EmitDebugCheckDeclarationContext(variable);
        __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
        __ StoreP(ip, ContextMemOperand(cp, variable->index()), r0);
        // No write barrier since the_hole_value is in old space.
        PrepareForBailoutForId(proxy->id(), BailoutState::NO_REGISTERS);
      }
      break;

    case VariableLocation::LOOKUP:
    case VariableLocation::MODULE:
      UNREACHABLE();
  }
}


void FullCodeGenerator::VisitFunctionDeclaration(
    FunctionDeclaration* declaration) {
  VariableProxy* proxy = declaration->proxy();
  Variable* variable = proxy->var();
  switch (variable->location()) {
    case VariableLocation::UNALLOCATED: {
      globals_->Add(variable->name(), zone());
      FeedbackVectorSlot slot = proxy->VariableFeedbackSlot();
      DCHECK(!slot.IsInvalid());
      globals_->Add(handle(Smi::FromInt(slot.ToInt()), isolate()), zone());

      // We need the slot where the literals array lives, too.
      slot = declaration->fun()->LiteralFeedbackSlot();
      DCHECK(!slot.IsInvalid());
      globals_->Add(handle(Smi::FromInt(slot.ToInt()), isolate()), zone());

      Handle<SharedFunctionInfo> function =
          Compiler::GetSharedFunctionInfo(declaration->fun(), script(), info_);
      // Check for stack-overflow exception.
      if (function.is_null()) return SetStackOverflow();
      globals_->Add(function, zone());
      break;
    }

    case VariableLocation::PARAMETER:
    case VariableLocation::LOCAL: {
      Comment cmnt(masm_, "[ FunctionDeclaration");
      VisitForAccumulatorValue(declaration->fun());
      __ StoreP(result_register(), StackOperand(variable));
      break;
    }

    case VariableLocation::CONTEXT: {
      Comment cmnt(masm_, "[ FunctionDeclaration");
      EmitDebugCheckDeclarationContext(variable);
      VisitForAccumulatorValue(declaration->fun());
      __ StoreP(result_register(), ContextMemOperand(cp, variable->index()),
                r0);
      int offset = Context::SlotOffset(variable->index());
      // We know that we have written a function, which is not a smi.
      __ RecordWriteContextSlot(cp, offset, result_register(), r5,
                                kLRHasBeenSaved, kDontSaveFPRegs,
                                EMIT_REMEMBERED_SET, OMIT_SMI_CHECK);
      PrepareForBailoutForId(proxy->id(), BailoutState::NO_REGISTERS);
      break;
    }

    case VariableLocation::LOOKUP:
    case VariableLocation::MODULE:
      UNREACHABLE();
  }
}


void FullCodeGenerator::DeclareGlobals(Handle<FixedArray> pairs) {
  // Call the runtime to declare the globals.
  __ mov(r4, Operand(pairs));
  __ LoadSmiLiteral(r3, Smi::FromInt(DeclareGlobalsFlags()));
  __ EmitLoadTypeFeedbackVector(r5);
  __ Push(r4, r3, r5);
  __ CallRuntime(Runtime::kDeclareGlobals);
  // Return value is ignored.
}


void FullCodeGenerator::VisitSwitchStatement(SwitchStatement* stmt) {
  Comment cmnt(masm_, "[ SwitchStatement");
  Breakable nested_statement(this, stmt);
  SetStatementPosition(stmt);

  // Keep the switch value on the stack until a case matches.
  VisitForStackValue(stmt->tag());
  PrepareForBailoutForId(stmt->EntryId(), BailoutState::NO_REGISTERS);

  ZoneList<CaseClause*>* clauses = stmt->cases();
  CaseClause* default_clause = NULL;  // Can occur anywhere in the list.

  Label next_test;  // Recycled for each test.
  // Compile all the tests with branches to their bodies.
  for (int i = 0; i < clauses->length(); i++) {
    CaseClause* clause = clauses->at(i);
    clause->body_target()->Unuse();

    // The default is not a test, but remember it as final fall through.
    if (clause->is_default()) {
      default_clause = clause;
      continue;
    }

    Comment cmnt(masm_, "[ Case comparison");
    __ bind(&next_test);
    next_test.Unuse();

    // Compile the label expression.
    VisitForAccumulatorValue(clause->label());

    // Perform the comparison as if via '==='.
    __ LoadP(r4, MemOperand(sp, 0));  // Switch value.
    bool inline_smi_code = ShouldInlineSmiCase(Token::EQ_STRICT);
    JumpPatchSite patch_site(masm_);
    if (inline_smi_code) {
      Label slow_case;
      __ orx(r5, r4, r3);
      patch_site.EmitJumpIfNotSmi(r5, &slow_case);

      __ cmp(r4, r3);
      __ bne(&next_test);
      __ Drop(1);  // Switch value is no longer needed.
      __ b(clause->body_target());
      __ bind(&slow_case);
    }

    // Record position before stub call for type feedback.
    SetExpressionPosition(clause);
    Handle<Code> ic =
        CodeFactory::CompareIC(isolate(), Token::EQ_STRICT).code();
    CallIC(ic, clause->CompareId());
    patch_site.EmitPatchInfo();

    Label skip;
    __ b(&skip);
    PrepareForBailout(clause, BailoutState::TOS_REGISTER);
    __ LoadRoot(ip, Heap::kTrueValueRootIndex);
    __ cmp(r3, ip);
    __ bne(&next_test);
    __ Drop(1);
    __ b(clause->body_target());
    __ bind(&skip);

    __ cmpi(r3, Operand::Zero());
    __ bne(&next_test);
    __ Drop(1);  // Switch value is no longer needed.
    __ b(clause->body_target());
  }

  // Discard the test value and jump to the default if present, otherwise to
  // the end of the statement.
  __ bind(&next_test);
  DropOperands(1);  // Switch value is no longer needed.
  if (default_clause == NULL) {
    __ b(nested_statement.break_label());
  } else {
    __ b(default_clause->body_target());
  }

  // Compile all the case bodies.
  for (int i = 0; i < clauses->length(); i++) {
    Comment cmnt(masm_, "[ Case body");
    CaseClause* clause = clauses->at(i);
    __ bind(clause->body_target());
    PrepareForBailoutForId(clause->EntryId(), BailoutState::NO_REGISTERS);
    VisitStatements(clause->statements());
  }

  __ bind(nested_statement.break_label());
  PrepareForBailoutForId(stmt->ExitId(), BailoutState::NO_REGISTERS);
}


void FullCodeGenerator::VisitForInStatement(ForInStatement* stmt) {
  Comment cmnt(masm_, "[ ForInStatement");
  SetStatementPosition(stmt, SKIP_BREAK);

  FeedbackVectorSlot slot = stmt->ForInFeedbackSlot();

  // Get the object to enumerate over.
  SetExpressionAsStatementPosition(stmt->enumerable());
  VisitForAccumulatorValue(stmt->enumerable());
  OperandStackDepthIncrement(5);

  Label loop, exit;
  Iteration loop_statement(this, stmt);
  increment_loop_depth();

  // If the object is null or undefined, skip over the loop, otherwise convert
  // it to a JS receiver.  See ECMA-262 version 5, section 12.6.4.
  Label convert, done_convert;
  __ JumpIfSmi(r3, &convert);
  __ CompareObjectType(r3, r4, r4, FIRST_JS_RECEIVER_TYPE);
  __ bge(&done_convert);
  __ CompareRoot(r3, Heap::kNullValueRootIndex);
  __ beq(&exit);
  __ CompareRoot(r3, Heap::kUndefinedValueRootIndex);
  __ beq(&exit);
  __ bind(&convert);
  __ Call(isolate()->builtins()->ToObject(), RelocInfo::CODE_TARGET);
  RestoreContext();
  __ bind(&done_convert);
  PrepareForBailoutForId(stmt->ToObjectId(), BailoutState::TOS_REGISTER);
  __ push(r3);

  // Check cache validity in generated code. If we cannot guarantee cache
  // validity, call the runtime system to check cache validity or get the
  // property names in a fixed array. Note: Proxies never have an enum cache,
  // so will always take the slow path.
  Label call_runtime;
  __ CheckEnumCache(&call_runtime);

  // The enum cache is valid.  Load the map of the object being
  // iterated over and use the cache for the iteration.
  Label use_cache;
  __ LoadP(r3, FieldMemOperand(r3, HeapObject::kMapOffset));
  __ b(&use_cache);

  // Get the set of properties to enumerate.
  __ bind(&call_runtime);
  __ push(r3);  // Duplicate the enumerable object on the stack.
  __ CallRuntime(Runtime::kForInEnumerate);
  PrepareForBailoutForId(stmt->EnumId(), BailoutState::TOS_REGISTER);

  // If we got a map from the runtime call, we can do a fast
  // modification check. Otherwise, we got a fixed array, and we have
  // to do a slow check.
  Label fixed_array;
  __ LoadP(r5, FieldMemOperand(r3, HeapObject::kMapOffset));
  __ LoadRoot(ip, Heap::kMetaMapRootIndex);
  __ cmp(r5, ip);
  __ bne(&fixed_array);

  // We got a map in register r3. Get the enumeration cache from it.
  Label no_descriptors;
  __ bind(&use_cache);

  __ EnumLength(r4, r3);
  __ CmpSmiLiteral(r4, Smi::kZero, r0);
  __ beq(&no_descriptors);

  __ LoadInstanceDescriptors(r3, r5);
  __ LoadP(r5, FieldMemOperand(r5, DescriptorArray::kEnumCacheOffset));
  __ LoadP(r5,
           FieldMemOperand(r5, DescriptorArray::kEnumCacheBridgeCacheOffset));

  // Set up the four remaining stack slots.
  __ push(r3);  // Map.
  __ LoadSmiLiteral(r3, Smi::kZero);
  // Push enumeration cache, enumeration cache length (as smi) and zero.
  __ Push(r5, r4, r3);
  __ b(&loop);

  __ bind(&no_descriptors);
  __ Drop(1);
  __ b(&exit);

  // We got a fixed array in register r3. Iterate through that.
  __ bind(&fixed_array);

  __ LoadSmiLiteral(r4, Smi::FromInt(1));  // Smi(1) indicates slow check
  __ Push(r4, r3);  // Smi and array
  __ LoadP(r4, FieldMemOperand(r3, FixedArray::kLengthOffset));
  __ Push(r4);  // Fixed array length (as smi).
  PrepareForBailoutForId(stmt->PrepareId(), BailoutState::NO_REGISTERS);
  __ LoadSmiLiteral(r3, Smi::kZero);
  __ Push(r3);  // Initial index.

  // Generate code for doing the condition check.
  __ bind(&loop);
  SetExpressionAsStatementPosition(stmt->each());

  // Load the current count to r3, load the length to r4.
  __ LoadP(r3, MemOperand(sp, 0 * kPointerSize));
  __ LoadP(r4, MemOperand(sp, 1 * kPointerSize));
  __ cmpl(r3, r4);  // Compare to the array length.
  __ bge(loop_statement.break_label());

  // Get the current entry of the array into register r6.
  __ LoadP(r5, MemOperand(sp, 2 * kPointerSize));
  __ addi(r5, r5, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ SmiToPtrArrayOffset(r6, r3);
  __ LoadPX(r6, MemOperand(r6, r5));

  // Get the expected map from the stack or a smi in the
  // permanent slow case into register r5.
  __ LoadP(r5, MemOperand(sp, 3 * kPointerSize));

  // Check if the expected map still matches that of the enumerable.
  // If not, we may have to filter the key.
  Label update_each;
  __ LoadP(r4, MemOperand(sp, 4 * kPointerSize));
  __ LoadP(r7, FieldMemOperand(r4, HeapObject::kMapOffset));
  __ cmp(r7, r5);
  __ beq(&update_each);

  // We need to filter the key, record slow-path here.
  int const vector_index = SmiFromSlot(slot)->value();
  __ EmitLoadTypeFeedbackVector(r3);
  __ mov(r5, Operand(TypeFeedbackVector::MegamorphicSentinel(isolate())));
  __ StoreP(
      r5, FieldMemOperand(r3, FixedArray::OffsetOfElementAt(vector_index)), r0);

  // Convert the entry to a string or (smi) 0 if it isn't a property
  // any more. If the property has been removed while iterating, we
  // just skip it.
  __ Push(r4, r6);  // Enumerable and current entry.
  __ CallRuntime(Runtime::kForInFilter);
  PrepareForBailoutForId(stmt->FilterId(), BailoutState::TOS_REGISTER);
  __ mr(r6, r3);
  __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
  __ cmp(r3, r0);
  __ beq(loop_statement.continue_label());

  // Update the 'each' property or variable from the possibly filtered
  // entry in register r6.
  __ bind(&update_each);
  __ mr(result_register(), r6);
  // Perform the assignment as if via '='.
  {
    EffectContext context(this);
    EmitAssignment(stmt->each(), stmt->EachFeedbackSlot());
    PrepareForBailoutForId(stmt->AssignmentId(), BailoutState::NO_REGISTERS);
  }

  // Both Crankshaft and Turbofan expect BodyId to be right before stmt->body().
  PrepareForBailoutForId(stmt->BodyId(), BailoutState::NO_REGISTERS);
  // Generate code for the body of the loop.
  Visit(stmt->body());

  // Generate code for the going to the next element by incrementing
  // the index (smi) stored on top of the stack.
  __ bind(loop_statement.continue_label());
  PrepareForBailoutForId(stmt->IncrementId(), BailoutState::NO_REGISTERS);
  __ pop(r3);
  __ AddSmiLiteral(r3, r3, Smi::FromInt(1), r0);
  __ push(r3);

  EmitBackEdgeBookkeeping(stmt, &loop);
  __ b(&loop);

  // Remove the pointers stored on the stack.
  __ bind(loop_statement.break_label());
  DropOperands(5);

  // Exit and decrement the loop depth.
  PrepareForBailoutForId(stmt->ExitId(), BailoutState::NO_REGISTERS);
  __ bind(&exit);
  decrement_loop_depth();
}


void FullCodeGenerator::EmitSetHomeObject(Expression* initializer, int offset,
                                          FeedbackVectorSlot slot) {
  DCHECK(NeedsHomeObject(initializer));
  __ LoadP(StoreDescriptor::ReceiverRegister(), MemOperand(sp));
  __ LoadP(StoreDescriptor::ValueRegister(),
           MemOperand(sp, offset * kPointerSize));
  CallStoreIC(slot, isolate()->factory()->home_object_symbol());
}


void FullCodeGenerator::EmitSetHomeObjectAccumulator(Expression* initializer,
                                                     int offset,
                                                     FeedbackVectorSlot slot) {
  DCHECK(NeedsHomeObject(initializer));
  __ Move(StoreDescriptor::ReceiverRegister(), r3);
  __ LoadP(StoreDescriptor::ValueRegister(),
           MemOperand(sp, offset * kPointerSize));
  CallStoreIC(slot, isolate()->factory()->home_object_symbol());
}

void FullCodeGenerator::EmitVariableLoad(VariableProxy* proxy,
                                         TypeofMode typeof_mode) {
  // Record position before possible IC call.
  SetExpressionPosition(proxy);
  PrepareForBailoutForId(proxy->BeforeId(), BailoutState::NO_REGISTERS);
  Variable* var = proxy->var();

  // Two cases: global variables and all other types of variables.
  switch (var->location()) {
    case VariableLocation::UNALLOCATED: {
      Comment cmnt(masm_, "[ Global variable");
      EmitGlobalVariableLoad(proxy, typeof_mode);
      context()->Plug(r3);
      break;
    }

    case VariableLocation::PARAMETER:
    case VariableLocation::LOCAL:
    case VariableLocation::CONTEXT: {
      DCHECK_EQ(NOT_INSIDE_TYPEOF, typeof_mode);
      Comment cmnt(masm_, var->IsContextSlot() ? "[ Context variable"
                                               : "[ Stack variable");
      if (proxy->hole_check_mode() == HoleCheckMode::kRequired) {
        // Throw a reference error when using an uninitialized let/const
        // binding in harmony mode.
        Label done;
        GetVar(r3, var);
        __ CompareRoot(r3, Heap::kTheHoleValueRootIndex);
        __ bne(&done);
        __ mov(r3, Operand(var->name()));
        __ push(r3);
        __ CallRuntime(Runtime::kThrowReferenceError);
        __ bind(&done);
        context()->Plug(r3);
        break;
      }
      context()->Plug(var);
      break;
    }

    case VariableLocation::LOOKUP:
    case VariableLocation::MODULE:
      UNREACHABLE();
  }
}


void FullCodeGenerator::EmitAccessor(ObjectLiteralProperty* property) {
  Expression* expression = (property == NULL) ? NULL : property->value();
  if (expression == NULL) {
    __ LoadRoot(r4, Heap::kNullValueRootIndex);
    PushOperand(r4);
  } else {
    VisitForStackValue(expression);
    if (NeedsHomeObject(expression)) {
      DCHECK(property->kind() == ObjectLiteral::Property::GETTER ||
             property->kind() == ObjectLiteral::Property::SETTER);
      int offset = property->kind() == ObjectLiteral::Property::GETTER ? 2 : 3;
      EmitSetHomeObject(expression, offset, property->GetSlot());
    }
  }
}


void FullCodeGenerator::VisitObjectLiteral(ObjectLiteral* expr) {
  Comment cmnt(masm_, "[ ObjectLiteral");

  Handle<BoilerplateDescription> constant_properties =
      expr->GetOrBuildConstantProperties(isolate());
  __ LoadP(r6, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ LoadSmiLiteral(r5, SmiFromSlot(expr->literal_slot()));
  __ mov(r4, Operand(constant_properties));
  int flags = expr->ComputeFlags();
  __ LoadSmiLiteral(r3, Smi::FromInt(flags));
  if (MustCreateObjectLiteralWithRuntime(expr)) {
    __ Push(r6, r5, r4, r3);
    __ CallRuntime(Runtime::kCreateObjectLiteral);
  } else {
    Callable callable = CodeFactory::FastCloneShallowObject(
        isolate(), expr->properties_count());
    __ Call(callable.code(), RelocInfo::CODE_TARGET);
    RestoreContext();
  }
  PrepareForBailoutForId(expr->CreateLiteralId(), BailoutState::TOS_REGISTER);

  // If result_saved is true the result is on top of the stack.  If
  // result_saved is false the result is in r3.
  bool result_saved = false;

  AccessorTable accessor_table(zone());
  for (int i = 0; i < expr->properties()->length(); i++) {
    ObjectLiteral::Property* property = expr->properties()->at(i);
    DCHECK(!property->is_computed_name());
    if (property->IsCompileTimeValue()) continue;

    Literal* key = property->key()->AsLiteral();
    Expression* value = property->value();
    if (!result_saved) {
      PushOperand(r3);  // Save result on stack
      result_saved = true;
    }
    switch (property->kind()) {
      case ObjectLiteral::Property::SPREAD:
      case ObjectLiteral::Property::CONSTANT:
        UNREACHABLE();
      case ObjectLiteral::Property::MATERIALIZED_LITERAL:
        DCHECK(!CompileTimeValue::IsCompileTimeValue(property->value()));
      // Fall through.
      case ObjectLiteral::Property::COMPUTED:
        // It is safe to use [[Put]] here because the boilerplate already
        // contains computed properties with an uninitialized value.
        if (key->IsStringLiteral()) {
          DCHECK(key->IsPropertyName());
          if (property->emit_store()) {
            VisitForAccumulatorValue(value);
            DCHECK(StoreDescriptor::ValueRegister().is(r3));
            __ LoadP(StoreDescriptor::ReceiverRegister(), MemOperand(sp));
            CallStoreIC(property->GetSlot(0), key->value());
            PrepareForBailoutForId(key->id(), BailoutState::NO_REGISTERS);

            if (NeedsHomeObject(value)) {
              EmitSetHomeObjectAccumulator(value, 0, property->GetSlot(1));
            }
          } else {
            VisitForEffect(value);
          }
          break;
        }
        // Duplicate receiver on stack.
        __ LoadP(r3, MemOperand(sp));
        PushOperand(r3);
        VisitForStackValue(key);
        VisitForStackValue(value);
        if (property->emit_store()) {
          if (NeedsHomeObject(value)) {
            EmitSetHomeObject(value, 2, property->GetSlot());
          }
          __ LoadSmiLiteral(r3, Smi::FromInt(SLOPPY));  // PropertyAttributes
          PushOperand(r3);
          CallRuntimeWithOperands(Runtime::kSetProperty);
        } else {
          DropOperands(3);
        }
        break;
      case ObjectLiteral::Property::PROTOTYPE:
        // Duplicate receiver on stack.
        __ LoadP(r3, MemOperand(sp));
        PushOperand(r3);
        VisitForStackValue(value);
        DCHECK(property->emit_store());
        CallRuntimeWithOperands(Runtime::kInternalSetPrototype);
        PrepareForBailoutForId(expr->GetIdForPropertySet(i),
                               BailoutState::NO_REGISTERS);
        break;
      case ObjectLiteral::Property::GETTER:
        if (property->emit_store()) {
          AccessorTable::Iterator it = accessor_table.lookup(key);
          it->second->bailout_id = expr->GetIdForPropertySet(i);
          it->second->getter = property;
        }
        break;
      case ObjectLiteral::Property::SETTER:
        if (property->emit_store()) {
          AccessorTable::Iterator it = accessor_table.lookup(key);
          it->second->bailout_id = expr->GetIdForPropertySet(i);
          it->second->setter = property;
        }
        break;
    }
  }

  // Emit code to define accessors, using only a single call to the runtime for
  // each pair of corresponding getters and setters.
  for (AccessorTable::Iterator it = accessor_table.begin();
       it != accessor_table.end(); ++it) {
    __ LoadP(r3, MemOperand(sp));  // Duplicate receiver.
    PushOperand(r3);
    VisitForStackValue(it->first);
    EmitAccessor(it->second->getter);
    EmitAccessor(it->second->setter);
    __ LoadSmiLiteral(r3, Smi::FromInt(NONE));
    PushOperand(r3);
    CallRuntimeWithOperands(Runtime::kDefineAccessorPropertyUnchecked);
    PrepareForBailoutForId(it->second->bailout_id, BailoutState::NO_REGISTERS);
  }

  if (result_saved) {
    context()->PlugTOS();
  } else {
    context()->Plug(r3);
  }
}


void FullCodeGenerator::VisitArrayLiteral(ArrayLiteral* expr) {
  Comment cmnt(masm_, "[ ArrayLiteral");

  Handle<ConstantElementsPair> constant_elements =
      expr->GetOrBuildConstantElements(isolate());
  bool has_fast_elements =
      IsFastObjectElementsKind(expr->constant_elements_kind());

  AllocationSiteMode allocation_site_mode = TRACK_ALLOCATION_SITE;
  if (has_fast_elements && !FLAG_allocation_site_pretenuring) {
    // If the only customer of allocation sites is transitioning, then
    // we can turn it off if we don't have anywhere else to transition to.
    allocation_site_mode = DONT_TRACK_ALLOCATION_SITE;
  }

  __ LoadP(r6, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ LoadSmiLiteral(r5, SmiFromSlot(expr->literal_slot()));
  __ mov(r4, Operand(constant_elements));
  if (MustCreateArrayLiteralWithRuntime(expr)) {
    __ LoadSmiLiteral(r3, Smi::FromInt(expr->ComputeFlags()));
    __ Push(r6, r5, r4, r3);
    __ CallRuntime(Runtime::kCreateArrayLiteral);
  } else {
    Callable callable =
        CodeFactory::FastCloneShallowArray(isolate(), allocation_site_mode);
    __ Call(callable.code(), RelocInfo::CODE_TARGET);
    RestoreContext();
  }
  PrepareForBailoutForId(expr->CreateLiteralId(), BailoutState::TOS_REGISTER);

  bool result_saved = false;  // Is the result saved to the stack?
  ZoneList<Expression*>* subexprs = expr->values();
  int length = subexprs->length();

  // Emit code to evaluate all the non-constant subexpressions and to store
  // them into the newly cloned array.
  for (int array_index = 0; array_index < length; array_index++) {
    Expression* subexpr = subexprs->at(array_index);
    DCHECK(!subexpr->IsSpread());
    // If the subexpression is a literal or a simple materialized literal it
    // is already set in the cloned array.
    if (CompileTimeValue::IsCompileTimeValue(subexpr)) continue;

    if (!result_saved) {
      PushOperand(r3);
      result_saved = true;
    }
    VisitForAccumulatorValue(subexpr);

    __ LoadSmiLiteral(StoreDescriptor::NameRegister(),
                      Smi::FromInt(array_index));
    __ LoadP(StoreDescriptor::ReceiverRegister(), MemOperand(sp, 0));
    CallKeyedStoreIC(expr->LiteralFeedbackSlot());

    PrepareForBailoutForId(expr->GetIdForElement(array_index),
                           BailoutState::NO_REGISTERS);
  }

  if (result_saved) {
    context()->PlugTOS();
  } else {
    context()->Plug(r3);
  }
}


void FullCodeGenerator::VisitAssignment(Assignment* expr) {
  DCHECK(expr->target()->IsValidReferenceExpressionOrThis());

  Comment cmnt(masm_, "[ Assignment");

  Property* property = expr->target()->AsProperty();
  LhsKind assign_type = Property::GetAssignType(property);

  // Evaluate LHS expression.
  switch (assign_type) {
    case VARIABLE:
      // Nothing to do here.
      break;
    case NAMED_PROPERTY:
      if (expr->is_compound()) {
        // We need the receiver both on the stack and in the register.
        VisitForStackValue(property->obj());
        __ LoadP(LoadDescriptor::ReceiverRegister(), MemOperand(sp, 0));
      } else {
        VisitForStackValue(property->obj());
      }
      break;
    case KEYED_PROPERTY:
      if (expr->is_compound()) {
        VisitForStackValue(property->obj());
        VisitForStackValue(property->key());
        __ LoadP(LoadDescriptor::ReceiverRegister(),
                 MemOperand(sp, 1 * kPointerSize));
        __ LoadP(LoadDescriptor::NameRegister(), MemOperand(sp, 0));
      } else {
        VisitForStackValue(property->obj());
        VisitForStackValue(property->key());
      }
      break;
    case NAMED_SUPER_PROPERTY:
    case KEYED_SUPER_PROPERTY:
      UNREACHABLE();
      break;
  }

  // For compound assignments we need another deoptimization point after the
  // variable/property load.
  if (expr->is_compound()) {
    {
      AccumulatorValueContext context(this);
      switch (assign_type) {
        case VARIABLE:
          EmitVariableLoad(expr->target()->AsVariableProxy());
          PrepareForBailout(expr->target(), BailoutState::TOS_REGISTER);
          break;
        case NAMED_PROPERTY:
          EmitNamedPropertyLoad(property);
          PrepareForBailoutForId(property->LoadId(),
                                 BailoutState::TOS_REGISTER);
          break;
        case KEYED_PROPERTY:
          EmitKeyedPropertyLoad(property);
          PrepareForBailoutForId(property->LoadId(),
                                 BailoutState::TOS_REGISTER);
          break;
        case NAMED_SUPER_PROPERTY:
        case KEYED_SUPER_PROPERTY:
          UNREACHABLE();
          break;
      }
    }

    Token::Value op = expr->binary_op();
    PushOperand(r3);  // Left operand goes on the stack.
    VisitForAccumulatorValue(expr->value());

    AccumulatorValueContext context(this);
    if (ShouldInlineSmiCase(op)) {
      EmitInlineSmiBinaryOp(expr->binary_operation(), op, expr->target(),
                            expr->value());
    } else {
      EmitBinaryOp(expr->binary_operation(), op);
    }

    // Deoptimization point in case the binary operation may have side effects.
    PrepareForBailout(expr->binary_operation(), BailoutState::TOS_REGISTER);
  } else {
    VisitForAccumulatorValue(expr->value());
  }

  SetExpressionPosition(expr);

  // Store the value.
  switch (assign_type) {
    case VARIABLE: {
      VariableProxy* proxy = expr->target()->AsVariableProxy();
      EmitVariableAssignment(proxy->var(), expr->op(), expr->AssignmentSlot(),
                             proxy->hole_check_mode());
      PrepareForBailoutForId(expr->AssignmentId(), BailoutState::TOS_REGISTER);
      context()->Plug(r3);
      break;
    }
    case NAMED_PROPERTY:
      EmitNamedPropertyAssignment(expr);
      break;
    case KEYED_PROPERTY:
      EmitKeyedPropertyAssignment(expr);
      break;
    case NAMED_SUPER_PROPERTY:
    case KEYED_SUPER_PROPERTY:
      UNREACHABLE();
      break;
  }
}


void FullCodeGenerator::VisitYield(Yield* expr) {
  // Resumable functions are not supported.
  UNREACHABLE();
}

void FullCodeGenerator::PushOperands(Register reg1, Register reg2) {
  OperandStackDepthIncrement(2);
  __ Push(reg1, reg2);
}

void FullCodeGenerator::PushOperands(Register reg1, Register reg2,
                                     Register reg3) {
  OperandStackDepthIncrement(3);
  __ Push(reg1, reg2, reg3);
}

void FullCodeGenerator::PushOperands(Register reg1, Register reg2,
                                     Register reg3, Register reg4) {
  OperandStackDepthIncrement(4);
  __ Push(reg1, reg2, reg3, reg4);
}

void FullCodeGenerator::PopOperands(Register reg1, Register reg2) {
  OperandStackDepthDecrement(2);
  __ Pop(reg1, reg2);
}

void FullCodeGenerator::EmitOperandStackDepthCheck() {
  if (FLAG_debug_code) {
    int expected_diff = StandardFrameConstants::kFixedFrameSizeFromFp +
                        operand_stack_depth_ * kPointerSize;
    __ sub(r3, fp, sp);
    __ mov(ip, Operand(expected_diff));
    __ cmp(r3, ip);
    __ Assert(eq, kUnexpectedStackDepth);
  }
}

void FullCodeGenerator::EmitCreateIteratorResult(bool done) {
  Label allocate, done_allocate;

  __ Allocate(JSIteratorResult::kSize, r3, r5, r6, &allocate,
              NO_ALLOCATION_FLAGS);
  __ b(&done_allocate);

  __ bind(&allocate);
  __ Push(Smi::FromInt(JSIteratorResult::kSize));
  __ CallRuntime(Runtime::kAllocateInNewSpace);

  __ bind(&done_allocate);
  __ LoadNativeContextSlot(Context::ITERATOR_RESULT_MAP_INDEX, r4);
  PopOperand(r5);
  __ LoadRoot(r6,
              done ? Heap::kTrueValueRootIndex : Heap::kFalseValueRootIndex);
  __ LoadRoot(r7, Heap::kEmptyFixedArrayRootIndex);
  __ StoreP(r4, FieldMemOperand(r3, HeapObject::kMapOffset), r0);
  __ StoreP(r7, FieldMemOperand(r3, JSObject::kPropertiesOffset), r0);
  __ StoreP(r7, FieldMemOperand(r3, JSObject::kElementsOffset), r0);
  __ StoreP(r5, FieldMemOperand(r3, JSIteratorResult::kValueOffset), r0);
  __ StoreP(r6, FieldMemOperand(r3, JSIteratorResult::kDoneOffset), r0);
}


void FullCodeGenerator::EmitInlineSmiBinaryOp(BinaryOperation* expr,
                                              Token::Value op,
                                              Expression* left_expr,
                                              Expression* right_expr) {
  Label done, smi_case, stub_call;

  Register scratch1 = r5;
  Register scratch2 = r6;

  // Get the arguments.
  Register left = r4;
  Register right = r3;
  PopOperand(left);

  // Perform combined smi check on both operands.
  __ orx(scratch1, left, right);
  STATIC_ASSERT(kSmiTag == 0);
  JumpPatchSite patch_site(masm_);
  patch_site.EmitJumpIfSmi(scratch1, &smi_case);

  __ bind(&stub_call);
  Handle<Code> code = CodeFactory::BinaryOpIC(isolate(), op).code();
  CallIC(code, expr->BinaryOperationFeedbackId());
  patch_site.EmitPatchInfo();
  __ b(&done);

  __ bind(&smi_case);
  // Smi case. This code works the same way as the smi-smi case in the type
  // recording binary operation stub.
  switch (op) {
    case Token::SAR:
      __ GetLeastBitsFromSmi(scratch1, right, 5);
      __ ShiftRightArith(right, left, scratch1);
      __ ClearRightImm(right, right, Operand(kSmiTagSize + kSmiShiftSize));
      break;
    case Token::SHL: {
      __ GetLeastBitsFromSmi(scratch2, right, 5);
#if V8_TARGET_ARCH_PPC64
      __ ShiftLeft_(right, left, scratch2);
#else
      __ SmiUntag(scratch1, left);
      __ ShiftLeft_(scratch1, scratch1, scratch2);
      // Check that the *signed* result fits in a smi
      __ JumpIfNotSmiCandidate(scratch1, scratch2, &stub_call);
      __ SmiTag(right, scratch1);
#endif
      break;
    }
    case Token::SHR: {
      __ SmiUntag(scratch1, left);
      __ GetLeastBitsFromSmi(scratch2, right, 5);
      __ srw(scratch1, scratch1, scratch2);
      // Unsigned shift is not allowed to produce a negative number.
      __ JumpIfNotUnsignedSmiCandidate(scratch1, r0, &stub_call);
      __ SmiTag(right, scratch1);
      break;
    }
    case Token::ADD: {
      __ AddAndCheckForOverflow(scratch1, left, right, scratch2, r0);
      __ BranchOnOverflow(&stub_call);
      __ mr(right, scratch1);
      break;
    }
    case Token::SUB: {
      __ SubAndCheckForOverflow(scratch1, left, right, scratch2, r0);
      __ BranchOnOverflow(&stub_call);
      __ mr(right, scratch1);
      break;
    }
    case Token::MUL: {
      Label mul_zero;
#if V8_TARGET_ARCH_PPC64
      // Remove tag from both operands.
      __ SmiUntag(ip, right);
      __ SmiUntag(r0, left);
      __ Mul(scratch1, r0, ip);
      // Check for overflowing the smi range - no overflow if higher 33 bits of
      // the result are identical.
      __ TestIfInt32(scratch1, r0);
      __ bne(&stub_call);
#else
      __ SmiUntag(ip, right);
      __ mullw(scratch1, left, ip);
      __ mulhw(scratch2, left, ip);
      // Check for overflowing the smi range - no overflow if higher 33 bits of
      // the result are identical.
      __ TestIfInt32(scratch2, scratch1, ip);
      __ bne(&stub_call);
#endif
      // Go slow on zero result to handle -0.
      __ cmpi(scratch1, Operand::Zero());
      __ beq(&mul_zero);
#if V8_TARGET_ARCH_PPC64
      __ SmiTag(right, scratch1);
#else
      __ mr(right, scratch1);
#endif
      __ b(&done);
      // We need -0 if we were multiplying a negative number with 0 to get 0.
      // We know one of them was zero.
      __ bind(&mul_zero);
      __ add(scratch2, right, left);
      __ cmpi(scratch2, Operand::Zero());
      __ blt(&stub_call);
      __ LoadSmiLiteral(right, Smi::kZero);
      break;
    }
    case Token::BIT_OR:
      __ orx(right, left, right);
      break;
    case Token::BIT_AND:
      __ and_(right, left, right);
      break;
    case Token::BIT_XOR:
      __ xor_(right, left, right);
      break;
    default:
      UNREACHABLE();
  }

  __ bind(&done);
  context()->Plug(r3);
}

void FullCodeGenerator::EmitBinaryOp(BinaryOperation* expr, Token::Value op) {
  PopOperand(r4);
  Handle<Code> code = CodeFactory::BinaryOpIC(isolate(), op).code();
  JumpPatchSite patch_site(masm_);  // unbound, signals no inlined smi code.
  CallIC(code, expr->BinaryOperationFeedbackId());
  patch_site.EmitPatchInfo();
  context()->Plug(r3);
}


void FullCodeGenerator::EmitAssignment(Expression* expr,
                                       FeedbackVectorSlot slot) {
  DCHECK(expr->IsValidReferenceExpressionOrThis());

  Property* prop = expr->AsProperty();
  LhsKind assign_type = Property::GetAssignType(prop);

  switch (assign_type) {
    case VARIABLE: {
      VariableProxy* proxy = expr->AsVariableProxy();
      EffectContext context(this);
      EmitVariableAssignment(proxy->var(), Token::ASSIGN, slot,
                             proxy->hole_check_mode());
      break;
    }
    case NAMED_PROPERTY: {
      PushOperand(r3);  // Preserve value.
      VisitForAccumulatorValue(prop->obj());
      __ Move(StoreDescriptor::ReceiverRegister(), r3);
      PopOperand(StoreDescriptor::ValueRegister());  // Restore value.
      CallStoreIC(slot, prop->key()->AsLiteral()->value());
      break;
    }
    case KEYED_PROPERTY: {
      PushOperand(r3);  // Preserve value.
      VisitForStackValue(prop->obj());
      VisitForAccumulatorValue(prop->key());
      __ Move(StoreDescriptor::NameRegister(), r3);
      PopOperands(StoreDescriptor::ValueRegister(),
                  StoreDescriptor::ReceiverRegister());
      CallKeyedStoreIC(slot);
      break;
    }
    case NAMED_SUPER_PROPERTY:
    case KEYED_SUPER_PROPERTY:
      UNREACHABLE();
      break;
  }
  context()->Plug(r3);
}


void FullCodeGenerator::EmitStoreToStackLocalOrContextSlot(
    Variable* var, MemOperand location) {
  __ StoreP(result_register(), location, r0);
  if (var->IsContextSlot()) {
    // RecordWrite may destroy all its register arguments.
    __ mr(r6, result_register());
    int offset = Context::SlotOffset(var->index());
    __ RecordWriteContextSlot(r4, offset, r6, r5, kLRHasBeenSaved,
                              kDontSaveFPRegs);
  }
}

void FullCodeGenerator::EmitVariableAssignment(Variable* var, Token::Value op,
                                               FeedbackVectorSlot slot,
                                               HoleCheckMode hole_check_mode) {
  if (var->IsUnallocated()) {
    // Global var, const, or let.
    __ LoadGlobalObject(StoreDescriptor::ReceiverRegister());
    CallStoreIC(slot, var->name());

  } else if (IsLexicalVariableMode(var->mode()) && op != Token::INIT) {
    DCHECK(!var->IsLookupSlot());
    DCHECK(var->IsStackAllocated() || var->IsContextSlot());
    MemOperand location = VarOperand(var, r4);
    // Perform an initialization check for lexically declared variables.
    if (hole_check_mode == HoleCheckMode::kRequired) {
      Label assign;
      __ LoadP(r6, location);
      __ CompareRoot(r6, Heap::kTheHoleValueRootIndex);
      __ bne(&assign);
      __ mov(r6, Operand(var->name()));
      __ push(r6);
      __ CallRuntime(Runtime::kThrowReferenceError);
      __ bind(&assign);
    }
    if (var->mode() != CONST) {
      EmitStoreToStackLocalOrContextSlot(var, location);
    } else if (var->throw_on_const_assignment(language_mode())) {
      __ CallRuntime(Runtime::kThrowConstAssignError);
    }
  } else if (var->is_this() && var->mode() == CONST && op == Token::INIT) {
    // Initializing assignment to const {this} needs a write barrier.
    DCHECK(var->IsStackAllocated() || var->IsContextSlot());
    Label uninitialized_this;
    MemOperand location = VarOperand(var, r4);
    __ LoadP(r6, location);
    __ CompareRoot(r6, Heap::kTheHoleValueRootIndex);
    __ beq(&uninitialized_this);
    __ mov(r4, Operand(var->name()));
    __ push(r4);
    __ CallRuntime(Runtime::kThrowReferenceError);
    __ bind(&uninitialized_this);
    EmitStoreToStackLocalOrContextSlot(var, location);

  } else {
    DCHECK(var->mode() != CONST || op == Token::INIT);
    DCHECK((var->IsStackAllocated() || var->IsContextSlot()));
    DCHECK(!var->IsLookupSlot());
    // Assignment to var or initializing assignment to let/const in harmony
    // mode.
    MemOperand location = VarOperand(var, r4);
    if (FLAG_debug_code && var->mode() == LET && op == Token::INIT) {
      // Check for an uninitialized let binding.
      __ LoadP(r5, location);
      __ CompareRoot(r5, Heap::kTheHoleValueRootIndex);
      __ Check(eq, kLetBindingReInitialization);
    }
    EmitStoreToStackLocalOrContextSlot(var, location);
  }
}


void FullCodeGenerator::EmitNamedPropertyAssignment(Assignment* expr) {
  // Assignment to a property, using a named store IC.
  Property* prop = expr->target()->AsProperty();
  DCHECK(prop != NULL);
  DCHECK(prop->key()->IsLiteral());

  PopOperand(StoreDescriptor::ReceiverRegister());
  CallStoreIC(expr->AssignmentSlot(), prop->key()->AsLiteral()->value());

  PrepareForBailoutForId(expr->AssignmentId(), BailoutState::TOS_REGISTER);
  context()->Plug(r3);
}


void FullCodeGenerator::EmitKeyedPropertyAssignment(Assignment* expr) {
  // Assignment to a property, using a keyed store IC.
  PopOperands(StoreDescriptor::ReceiverRegister(),
              StoreDescriptor::NameRegister());
  DCHECK(StoreDescriptor::ValueRegister().is(r3));

  CallKeyedStoreIC(expr->AssignmentSlot());

  PrepareForBailoutForId(expr->AssignmentId(), BailoutState::TOS_REGISTER);
  context()->Plug(r3);
}

// Code common for calls using the IC.
void FullCodeGenerator::EmitCallWithLoadIC(Call* expr) {
  Expression* callee = expr->expression();

  // Get the target function.
  ConvertReceiverMode convert_mode;
  if (callee->IsVariableProxy()) {
    {
      StackValueContext context(this);
      EmitVariableLoad(callee->AsVariableProxy());
      PrepareForBailout(callee, BailoutState::NO_REGISTERS);
    }
    // Push undefined as receiver. This is patched in the method prologue if it
    // is a sloppy mode method.
    __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
    PushOperand(r0);
    convert_mode = ConvertReceiverMode::kNullOrUndefined;
  } else {
    // Load the function from the receiver.
    DCHECK(callee->IsProperty());
    DCHECK(!callee->AsProperty()->IsSuperAccess());
    __ LoadP(LoadDescriptor::ReceiverRegister(), MemOperand(sp, 0));
    EmitNamedPropertyLoad(callee->AsProperty());
    PrepareForBailoutForId(callee->AsProperty()->LoadId(),
                           BailoutState::TOS_REGISTER);
    // Push the target function under the receiver.
    __ LoadP(r0, MemOperand(sp, 0));
    PushOperand(r0);
    __ StoreP(r3, MemOperand(sp, kPointerSize));
    convert_mode = ConvertReceiverMode::kNotNullOrUndefined;
  }

  EmitCall(expr, convert_mode);
}


// Code common for calls using the IC.
void FullCodeGenerator::EmitKeyedCallWithLoadIC(Call* expr, Expression* key) {
  // Load the key.
  VisitForAccumulatorValue(key);

  Expression* callee = expr->expression();

  // Load the function from the receiver.
  DCHECK(callee->IsProperty());
  __ LoadP(LoadDescriptor::ReceiverRegister(), MemOperand(sp, 0));
  __ Move(LoadDescriptor::NameRegister(), r3);
  EmitKeyedPropertyLoad(callee->AsProperty());
  PrepareForBailoutForId(callee->AsProperty()->LoadId(),
                         BailoutState::TOS_REGISTER);

  // Push the target function under the receiver.
  __ LoadP(ip, MemOperand(sp, 0));
  PushOperand(ip);
  __ StoreP(r3, MemOperand(sp, kPointerSize));

  EmitCall(expr, ConvertReceiverMode::kNotNullOrUndefined);
}


void FullCodeGenerator::EmitCall(Call* expr, ConvertReceiverMode mode) {
  // Load the arguments.
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    VisitForStackValue(args->at(i));
  }

  PrepareForBailoutForId(expr->CallId(), BailoutState::NO_REGISTERS);
  SetCallPosition(expr, expr->tail_call_mode());
  if (expr->tail_call_mode() == TailCallMode::kAllow) {
    if (FLAG_trace) {
      __ CallRuntime(Runtime::kTraceTailCall);
    }
    // Update profiling counters before the tail call since we will
    // not return to this function.
    EmitProfilingCounterHandlingForReturnSequence(true);
  }
  Handle<Code> code =
      CodeFactory::CallIC(isolate(), mode, expr->tail_call_mode()).code();
  __ LoadSmiLiteral(r6, SmiFromSlot(expr->CallFeedbackICSlot()));
  __ LoadP(r4, MemOperand(sp, (arg_count + 1) * kPointerSize), r0);
  __ mov(r3, Operand(arg_count));
  CallIC(code);
  OperandStackDepthDecrement(arg_count + 1);

  RecordJSReturnSite(expr);
  RestoreContext();
  context()->DropAndPlug(1, r3);
}

void FullCodeGenerator::VisitCallNew(CallNew* expr) {
  Comment cmnt(masm_, "[ CallNew");
  // According to ECMA-262, section 11.2.2, page 44, the function
  // expression in new calls must be evaluated before the
  // arguments.

  // Push constructor on the stack.  If it's not a function it's used as
  // receiver for CALL_NON_FUNCTION, otherwise the value on the stack is
  // ignored.
  DCHECK(!expr->expression()->IsSuperPropertyReference());
  VisitForStackValue(expr->expression());

  // Push the arguments ("left-to-right") on the stack.
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    VisitForStackValue(args->at(i));
  }

  // Call the construct call builtin that handles allocation and
  // constructor invocation.
  SetConstructCallPosition(expr);

  // Load function and argument count into r4 and r3.
  __ mov(r3, Operand(arg_count));
  __ LoadP(r4, MemOperand(sp, arg_count * kPointerSize), r0);

  // Record call targets in unoptimized code.
  __ EmitLoadTypeFeedbackVector(r5);
  __ LoadSmiLiteral(r6, SmiFromSlot(expr->CallNewFeedbackSlot()));

  CallConstructStub stub(isolate());
  CallIC(stub.GetCode());
  OperandStackDepthDecrement(arg_count + 1);
  PrepareForBailoutForId(expr->ReturnId(), BailoutState::TOS_REGISTER);
  RestoreContext();
  context()->Plug(r3);
}


void FullCodeGenerator::EmitIsSmi(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false, &if_true,
                         &if_false, &fall_through);

  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  __ TestIfSmi(r3, r0);
  Split(eq, if_true, if_false, fall_through, cr0);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsJSReceiver(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false, &if_true,
                         &if_false, &fall_through);

  __ JumpIfSmi(r3, if_false);
  __ CompareObjectType(r3, r4, r4, FIRST_JS_RECEIVER_TYPE);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(ge, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsArray(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false, &if_true,
                         &if_false, &fall_through);

  __ JumpIfSmi(r3, if_false);
  __ CompareObjectType(r3, r4, r4, JS_ARRAY_TYPE);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(eq, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsTypedArray(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false, &if_true,
                         &if_false, &fall_through);

  __ JumpIfSmi(r3, if_false);
  __ CompareObjectType(r3, r4, r4, JS_TYPED_ARRAY_TYPE);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(eq, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsJSProxy(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false, &if_true,
                         &if_false, &fall_through);

  __ JumpIfSmi(r3, if_false);
  __ CompareObjectType(r3, r4, r4, JS_PROXY_TYPE);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(eq, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitClassOf(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);
  Label done, null, function, non_function_constructor;

  VisitForAccumulatorValue(args->at(0));

  // If the object is not a JSReceiver, we return null.
  __ JumpIfSmi(r3, &null);
  STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
  __ CompareObjectType(r3, r3, r4, FIRST_JS_RECEIVER_TYPE);
  // Map is now in r3.
  __ blt(&null);

  // Return 'Function' for JSFunction and JSBoundFunction objects.
  __ cmpli(r4, Operand(FIRST_FUNCTION_TYPE));
  STATIC_ASSERT(LAST_FUNCTION_TYPE == LAST_TYPE);
  __ bge(&function);

  // Check if the constructor in the map is a JS function.
  Register instance_type = r5;
  __ GetMapConstructor(r3, r3, r4, instance_type);
  __ cmpi(instance_type, Operand(JS_FUNCTION_TYPE));
  __ bne(&non_function_constructor);

  // r3 now contains the constructor function. Grab the
  // instance class name from there.
  __ LoadP(r3, FieldMemOperand(r3, JSFunction::kSharedFunctionInfoOffset));
  __ LoadP(r3,
           FieldMemOperand(r3, SharedFunctionInfo::kInstanceClassNameOffset));
  __ b(&done);

  // Functions have class 'Function'.
  __ bind(&function);
  __ LoadRoot(r3, Heap::kFunction_stringRootIndex);
  __ b(&done);

  // Objects with a non-function constructor have class 'Object'.
  __ bind(&non_function_constructor);
  __ LoadRoot(r3, Heap::kObject_stringRootIndex);
  __ b(&done);

  // Non-JS objects have class null.
  __ bind(&null);
  __ LoadRoot(r3, Heap::kNullValueRootIndex);

  // All done.
  __ bind(&done);

  context()->Plug(r3);
}


void FullCodeGenerator::EmitStringCharCodeAt(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 2);
  VisitForStackValue(args->at(0));
  VisitForAccumulatorValue(args->at(1));

  Register object = r4;
  Register index = r3;
  Register result = r6;

  PopOperand(object);

  Label need_conversion;
  Label index_out_of_range;
  Label done;
  StringCharCodeAtGenerator generator(object, index, result, &need_conversion,
                                      &need_conversion, &index_out_of_range);
  generator.GenerateFast(masm_);
  __ b(&done);

  __ bind(&index_out_of_range);
  // When the index is out of range, the spec requires us to return
  // NaN.
  __ LoadRoot(result, Heap::kNanValueRootIndex);
  __ b(&done);

  __ bind(&need_conversion);
  // Load the undefined value into the result register, which will
  // trigger conversion.
  __ LoadRoot(result, Heap::kUndefinedValueRootIndex);
  __ b(&done);

  NopRuntimeCallHelper call_helper;
  generator.GenerateSlow(masm_, NOT_PART_OF_IC_HANDLER, call_helper);

  __ bind(&done);
  context()->Plug(result);
}


void FullCodeGenerator::EmitCall(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_LE(2, args->length());
  // Push target, receiver and arguments onto the stack.
  for (Expression* const arg : *args) {
    VisitForStackValue(arg);
  }
  PrepareForBailoutForId(expr->CallId(), BailoutState::NO_REGISTERS);
  // Move target to r4.
  int const argc = args->length() - 2;
  __ LoadP(r4, MemOperand(sp, (argc + 1) * kPointerSize));
  // Call the target.
  __ mov(r3, Operand(argc));
  __ Call(isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
  OperandStackDepthDecrement(argc + 1);
  RestoreContext();
  // Discard the function left on TOS.
  context()->DropAndPlug(1, r3);
}

void FullCodeGenerator::EmitGetSuperConstructor(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(1, args->length());
  VisitForAccumulatorValue(args->at(0));
  __ AssertFunction(r3);
  __ LoadP(r3, FieldMemOperand(r3, HeapObject::kMapOffset));
  __ LoadP(r3, FieldMemOperand(r3, Map::kPrototypeOffset));
  context()->Plug(r3);
}

void FullCodeGenerator::EmitDebugIsActive(CallRuntime* expr) {
  DCHECK(expr->arguments()->length() == 0);
  ExternalReference debug_is_active =
      ExternalReference::debug_is_active_address(isolate());
  __ mov(ip, Operand(debug_is_active));
  __ lbz(r3, MemOperand(ip));
  __ SmiTag(r3);
  context()->Plug(r3);
}


void FullCodeGenerator::EmitCreateIterResultObject(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(2, args->length());
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));

  Label runtime, done;

  __ Allocate(JSIteratorResult::kSize, r3, r5, r6, &runtime,
              NO_ALLOCATION_FLAGS);
  __ LoadNativeContextSlot(Context::ITERATOR_RESULT_MAP_INDEX, r4);
  __ Pop(r5, r6);
  __ LoadRoot(r7, Heap::kEmptyFixedArrayRootIndex);
  __ StoreP(r4, FieldMemOperand(r3, HeapObject::kMapOffset), r0);
  __ StoreP(r7, FieldMemOperand(r3, JSObject::kPropertiesOffset), r0);
  __ StoreP(r7, FieldMemOperand(r3, JSObject::kElementsOffset), r0);
  __ StoreP(r5, FieldMemOperand(r3, JSIteratorResult::kValueOffset), r0);
  __ StoreP(r6, FieldMemOperand(r3, JSIteratorResult::kDoneOffset), r0);
  STATIC_ASSERT(JSIteratorResult::kSize == 5 * kPointerSize);
  __ b(&done);

  __ bind(&runtime);
  CallRuntimeWithOperands(Runtime::kCreateIterResultObject);

  __ bind(&done);
  context()->Plug(r3);
}


void FullCodeGenerator::EmitLoadJSRuntimeFunction(CallRuntime* expr) {
  // Push function.
  __ LoadNativeContextSlot(expr->context_index(), r3);
  PushOperand(r3);

  // Push undefined as the receiver.
  __ LoadRoot(r3, Heap::kUndefinedValueRootIndex);
  PushOperand(r3);
}


void FullCodeGenerator::EmitCallJSRuntimeFunction(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();

  SetCallPosition(expr);
  __ LoadP(r4, MemOperand(sp, (arg_count + 1) * kPointerSize), r0);
  __ mov(r3, Operand(arg_count));
  __ Call(isolate()->builtins()->Call(ConvertReceiverMode::kNullOrUndefined),
          RelocInfo::CODE_TARGET);
  OperandStackDepthDecrement(arg_count + 1);
  RestoreContext();
}


void FullCodeGenerator::VisitUnaryOperation(UnaryOperation* expr) {
  switch (expr->op()) {
    case Token::DELETE: {
      Comment cmnt(masm_, "[ UnaryOperation (DELETE)");
      Property* property = expr->expression()->AsProperty();
      VariableProxy* proxy = expr->expression()->AsVariableProxy();

      if (property != NULL) {
        VisitForStackValue(property->obj());
        VisitForStackValue(property->key());
        CallRuntimeWithOperands(is_strict(language_mode())
                                    ? Runtime::kDeleteProperty_Strict
                                    : Runtime::kDeleteProperty_Sloppy);
        context()->Plug(r3);
      } else if (proxy != NULL) {
        Variable* var = proxy->var();
        // Delete of an unqualified identifier is disallowed in strict mode but
        // "delete this" is allowed.
        bool is_this = var->is_this();
        DCHECK(is_sloppy(language_mode()) || is_this);
        if (var->IsUnallocated()) {
          __ LoadGlobalObject(r5);
          __ mov(r4, Operand(var->name()));
          __ Push(r5, r4);
          __ CallRuntime(Runtime::kDeleteProperty_Sloppy);
          context()->Plug(r3);
        } else {
          DCHECK(!var->IsLookupSlot());
          DCHECK(var->IsStackAllocated() || var->IsContextSlot());
          // Result of deleting non-global, non-dynamic variables is false.
          // The subexpression does not have side effects.
          context()->Plug(is_this);
        }
      } else {
        // Result of deleting non-property, non-variable reference is true.
        // The subexpression may have side effects.
        VisitForEffect(expr->expression());
        context()->Plug(true);
      }
      break;
    }

    case Token::VOID: {
      Comment cmnt(masm_, "[ UnaryOperation (VOID)");
      VisitForEffect(expr->expression());
      context()->Plug(Heap::kUndefinedValueRootIndex);
      break;
    }

    case Token::NOT: {
      Comment cmnt(masm_, "[ UnaryOperation (NOT)");
      if (context()->IsEffect()) {
        // Unary NOT has no side effects so it's only necessary to visit the
        // subexpression.  Match the optimizing compiler by not branching.
        VisitForEffect(expr->expression());
      } else if (context()->IsTest()) {
        const TestContext* test = TestContext::cast(context());
        // The labels are swapped for the recursive call.
        VisitForControl(expr->expression(), test->false_label(),
                        test->true_label(), test->fall_through());
        context()->Plug(test->true_label(), test->false_label());
      } else {
        // We handle value contexts explicitly rather than simply visiting
        // for control and plugging the control flow into the context,
        // because we need to prepare a pair of extra administrative AST ids
        // for the optimizing compiler.
        DCHECK(context()->IsAccumulatorValue() || context()->IsStackValue());
        Label materialize_true, materialize_false, done;
        VisitForControl(expr->expression(), &materialize_false,
                        &materialize_true, &materialize_true);
        if (!context()->IsAccumulatorValue()) OperandStackDepthIncrement(1);
        __ bind(&materialize_true);
        PrepareForBailoutForId(expr->MaterializeTrueId(),
                               BailoutState::NO_REGISTERS);
        __ LoadRoot(r3, Heap::kTrueValueRootIndex);
        if (context()->IsStackValue()) __ push(r3);
        __ b(&done);
        __ bind(&materialize_false);
        PrepareForBailoutForId(expr->MaterializeFalseId(),
                               BailoutState::NO_REGISTERS);
        __ LoadRoot(r3, Heap::kFalseValueRootIndex);
        if (context()->IsStackValue()) __ push(r3);
        __ bind(&done);
      }
      break;
    }

    case Token::TYPEOF: {
      Comment cmnt(masm_, "[ UnaryOperation (TYPEOF)");
      {
        AccumulatorValueContext context(this);
        VisitForTypeofValue(expr->expression());
      }
      __ mr(r6, r3);
      __ Call(isolate()->builtins()->Typeof(), RelocInfo::CODE_TARGET);
      context()->Plug(r3);
      break;
    }

    default:
      UNREACHABLE();
  }
}


void FullCodeGenerator::VisitCountOperation(CountOperation* expr) {
  DCHECK(expr->expression()->IsValidReferenceExpressionOrThis());

  Comment cmnt(masm_, "[ CountOperation");

  Property* prop = expr->expression()->AsProperty();
  LhsKind assign_type = Property::GetAssignType(prop);

  // Evaluate expression and get value.
  if (assign_type == VARIABLE) {
    DCHECK(expr->expression()->AsVariableProxy()->var() != NULL);
    AccumulatorValueContext context(this);
    EmitVariableLoad(expr->expression()->AsVariableProxy());
  } else {
    // Reserve space for result of postfix operation.
    if (expr->is_postfix() && !context()->IsEffect()) {
      __ LoadSmiLiteral(ip, Smi::kZero);
      PushOperand(ip);
    }
    switch (assign_type) {
      case NAMED_PROPERTY: {
        // Put the object both on the stack and in the register.
        VisitForStackValue(prop->obj());
        __ LoadP(LoadDescriptor::ReceiverRegister(), MemOperand(sp, 0));
        EmitNamedPropertyLoad(prop);
        break;
      }

      case KEYED_PROPERTY: {
        VisitForStackValue(prop->obj());
        VisitForStackValue(prop->key());
        __ LoadP(LoadDescriptor::ReceiverRegister(),
                 MemOperand(sp, 1 * kPointerSize));
        __ LoadP(LoadDescriptor::NameRegister(), MemOperand(sp, 0));
        EmitKeyedPropertyLoad(prop);
        break;
      }

      case NAMED_SUPER_PROPERTY:
      case KEYED_SUPER_PROPERTY:
      case VARIABLE:
        UNREACHABLE();
    }
  }

  // We need a second deoptimization point after loading the value
  // in case evaluating the property load my have a side effect.
  if (assign_type == VARIABLE) {
    PrepareForBailout(expr->expression(), BailoutState::TOS_REGISTER);
  } else {
    PrepareForBailoutForId(prop->LoadId(), BailoutState::TOS_REGISTER);
  }

  // Inline smi case if we are in a loop.
  Label stub_call, done;
  JumpPatchSite patch_site(masm_);

  int count_value = expr->op() == Token::INC ? 1 : -1;
  if (ShouldInlineSmiCase(expr->op())) {
    Label slow;
    patch_site.EmitJumpIfNotSmi(r3, &slow);

    // Save result for postfix expressions.
    if (expr->is_postfix()) {
      if (!context()->IsEffect()) {
        // Save the result on the stack. If we have a named or keyed property
        // we store the result under the receiver that is currently on top
        // of the stack.
        switch (assign_type) {
          case VARIABLE:
            __ push(r3);
            break;
          case NAMED_PROPERTY:
            __ StoreP(r3, MemOperand(sp, kPointerSize));
            break;
          case KEYED_PROPERTY:
            __ StoreP(r3, MemOperand(sp, 2 * kPointerSize));
            break;
          case NAMED_SUPER_PROPERTY:
          case KEYED_SUPER_PROPERTY:
            UNREACHABLE();
            break;
        }
      }
    }

    Register scratch1 = r4;
    Register scratch2 = r5;
    __ LoadSmiLiteral(scratch1, Smi::FromInt(count_value));
    __ AddAndCheckForOverflow(r3, r3, scratch1, scratch2, r0);
    __ BranchOnNoOverflow(&done);
    // Call stub. Undo operation first.
    __ sub(r3, r3, scratch1);
    __ b(&stub_call);
    __ bind(&slow);
  }

  // Convert old value into a number.
  __ Call(isolate()->builtins()->ToNumber(), RelocInfo::CODE_TARGET);
  RestoreContext();
  PrepareForBailoutForId(expr->ToNumberId(), BailoutState::TOS_REGISTER);

  // Save result for postfix expressions.
  if (expr->is_postfix()) {
    if (!context()->IsEffect()) {
      // Save the result on the stack. If we have a named or keyed property
      // we store the result under the receiver that is currently on top
      // of the stack.
      switch (assign_type) {
        case VARIABLE:
          PushOperand(r3);
          break;
        case NAMED_PROPERTY:
          __ StoreP(r3, MemOperand(sp, kPointerSize));
          break;
        case KEYED_PROPERTY:
          __ StoreP(r3, MemOperand(sp, 2 * kPointerSize));
          break;
        case NAMED_SUPER_PROPERTY:
        case KEYED_SUPER_PROPERTY:
          UNREACHABLE();
          break;
      }
    }
  }

  __ bind(&stub_call);
  __ mr(r4, r3);
  __ LoadSmiLiteral(r3, Smi::FromInt(count_value));

  SetExpressionPosition(expr);

  Handle<Code> code = CodeFactory::BinaryOpIC(isolate(), Token::ADD).code();
  CallIC(code, expr->CountBinOpFeedbackId());
  patch_site.EmitPatchInfo();
  __ bind(&done);

  // Store the value returned in r3.
  switch (assign_type) {
    case VARIABLE: {
      VariableProxy* proxy = expr->expression()->AsVariableProxy();
      if (expr->is_postfix()) {
        {
          EffectContext context(this);
          EmitVariableAssignment(proxy->var(), Token::ASSIGN, expr->CountSlot(),
                                 proxy->hole_check_mode());
          PrepareForBailoutForId(expr->AssignmentId(),
                                 BailoutState::TOS_REGISTER);
          context.Plug(r3);
        }
        // For all contexts except EffectConstant We have the result on
        // top of the stack.
        if (!context()->IsEffect()) {
          context()->PlugTOS();
        }
      } else {
        EmitVariableAssignment(proxy->var(), Token::ASSIGN, expr->CountSlot(),
                               proxy->hole_check_mode());
        PrepareForBailoutForId(expr->AssignmentId(),
                               BailoutState::TOS_REGISTER);
        context()->Plug(r3);
      }
      break;
    }
    case NAMED_PROPERTY: {
      PopOperand(StoreDescriptor::ReceiverRegister());
      CallStoreIC(expr->CountSlot(), prop->key()->AsLiteral()->value());
      PrepareForBailoutForId(expr->AssignmentId(), BailoutState::TOS_REGISTER);
      if (expr->is_postfix()) {
        if (!context()->IsEffect()) {
          context()->PlugTOS();
        }
      } else {
        context()->Plug(r3);
      }
      break;
    }
    case KEYED_PROPERTY: {
      PopOperands(StoreDescriptor::ReceiverRegister(),
                  StoreDescriptor::NameRegister());
      CallKeyedStoreIC(expr->CountSlot());
      PrepareForBailoutForId(expr->AssignmentId(), BailoutState::TOS_REGISTER);
      if (expr->is_postfix()) {
        if (!context()->IsEffect()) {
          context()->PlugTOS();
        }
      } else {
        context()->Plug(r3);
      }
      break;
    }
    case NAMED_SUPER_PROPERTY:
    case KEYED_SUPER_PROPERTY:
      UNREACHABLE();
      break;
  }
}


void FullCodeGenerator::EmitLiteralCompareTypeof(Expression* expr,
                                                 Expression* sub_expr,
                                                 Handle<String> check) {
  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false, &if_true,
                         &if_false, &fall_through);

  {
    AccumulatorValueContext context(this);
    VisitForTypeofValue(sub_expr);
  }
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);

  Factory* factory = isolate()->factory();
  if (String::Equals(check, factory->number_string())) {
    __ JumpIfSmi(r3, if_true);
    __ LoadP(r3, FieldMemOperand(r3, HeapObject::kMapOffset));
    __ LoadRoot(ip, Heap::kHeapNumberMapRootIndex);
    __ cmp(r3, ip);
    Split(eq, if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->string_string())) {
    __ JumpIfSmi(r3, if_false);
    __ CompareObjectType(r3, r3, r4, FIRST_NONSTRING_TYPE);
    Split(lt, if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->symbol_string())) {
    __ JumpIfSmi(r3, if_false);
    __ CompareObjectType(r3, r3, r4, SYMBOL_TYPE);
    Split(eq, if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->boolean_string())) {
    __ CompareRoot(r3, Heap::kTrueValueRootIndex);
    __ beq(if_true);
    __ CompareRoot(r3, Heap::kFalseValueRootIndex);
    Split(eq, if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->undefined_string())) {
    __ CompareRoot(r3, Heap::kNullValueRootIndex);
    __ beq(if_false);
    __ JumpIfSmi(r3, if_false);
    // Check for undetectable objects => true.
    __ LoadP(r3, FieldMemOperand(r3, HeapObject::kMapOffset));
    __ lbz(r4, FieldMemOperand(r3, Map::kBitFieldOffset));
    __ andi(r0, r4, Operand(1 << Map::kIsUndetectable));
    Split(ne, if_true, if_false, fall_through, cr0);

  } else if (String::Equals(check, factory->function_string())) {
    __ JumpIfSmi(r3, if_false);
    __ LoadP(r3, FieldMemOperand(r3, HeapObject::kMapOffset));
    __ lbz(r4, FieldMemOperand(r3, Map::kBitFieldOffset));
    __ andi(r4, r4,
            Operand((1 << Map::kIsCallable) | (1 << Map::kIsUndetectable)));
    __ cmpi(r4, Operand(1 << Map::kIsCallable));
    Split(eq, if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->object_string())) {
    __ JumpIfSmi(r3, if_false);
    __ CompareRoot(r3, Heap::kNullValueRootIndex);
    __ beq(if_true);
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    __ CompareObjectType(r3, r3, r4, FIRST_JS_RECEIVER_TYPE);
    __ blt(if_false);
    // Check for callable or undetectable objects => false.
    __ lbz(r4, FieldMemOperand(r3, Map::kBitFieldOffset));
    __ andi(r0, r4,
            Operand((1 << Map::kIsCallable) | (1 << Map::kIsUndetectable)));
    Split(eq, if_true, if_false, fall_through, cr0);
// clang-format off
#define SIMD128_TYPE(TYPE, Type, type, lane_count, lane_type)   \
  } else if (String::Equals(check, factory->type##_string())) { \
    __ JumpIfSmi(r3, if_false);                                 \
    __ LoadP(r3, FieldMemOperand(r3, HeapObject::kMapOffset));    \
    __ CompareRoot(r3, Heap::k##Type##MapRootIndex);            \
    Split(eq, if_true, if_false, fall_through);
  SIMD128_TYPES(SIMD128_TYPE)
#undef SIMD128_TYPE
    // clang-format on
  } else {
    if (if_false != fall_through) __ b(if_false);
  }
  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::VisitCompareOperation(CompareOperation* expr) {
  Comment cmnt(masm_, "[ CompareOperation");

  // First we try a fast inlined version of the compare when one of
  // the operands is a literal.
  if (TryLiteralCompare(expr)) return;

  // Always perform the comparison for its control flow.  Pack the result
  // into the expression's context after the comparison is performed.
  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false, &if_true,
                         &if_false, &fall_through);

  Token::Value op = expr->op();
  VisitForStackValue(expr->left());
  switch (op) {
    case Token::IN:
      VisitForStackValue(expr->right());
      SetExpressionPosition(expr);
      EmitHasProperty();
      PrepareForBailoutBeforeSplit(expr, false, NULL, NULL);
      __ CompareRoot(r3, Heap::kTrueValueRootIndex);
      Split(eq, if_true, if_false, fall_through);
      break;

    case Token::INSTANCEOF: {
      VisitForAccumulatorValue(expr->right());
      SetExpressionPosition(expr);
      PopOperand(r4);
      __ Call(isolate()->builtins()->InstanceOf(), RelocInfo::CODE_TARGET);
      PrepareForBailoutBeforeSplit(expr, false, NULL, NULL);
      __ CompareRoot(r3, Heap::kTrueValueRootIndex);
      Split(eq, if_true, if_false, fall_through);
      break;
    }

    default: {
      VisitForAccumulatorValue(expr->right());
      SetExpressionPosition(expr);
      Condition cond = CompareIC::ComputeCondition(op);
      PopOperand(r4);

      bool inline_smi_code = ShouldInlineSmiCase(op);
      JumpPatchSite patch_site(masm_);
      if (inline_smi_code) {
        Label slow_case;
        __ orx(r5, r3, r4);
        patch_site.EmitJumpIfNotSmi(r5, &slow_case);
        __ cmp(r4, r3);
        Split(cond, if_true, if_false, NULL);
        __ bind(&slow_case);
      }

      Handle<Code> ic = CodeFactory::CompareIC(isolate(), op).code();
      CallIC(ic, expr->CompareOperationFeedbackId());
      patch_site.EmitPatchInfo();
      PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
      __ cmpi(r3, Operand::Zero());
      Split(cond, if_true, if_false, fall_through);
    }
  }

  // Convert the result of the comparison into one expected for this
  // expression's context.
  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitLiteralCompareNil(CompareOperation* expr,
                                              Expression* sub_expr,
                                              NilValue nil) {
  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false, &if_true,
                         &if_false, &fall_through);

  VisitForAccumulatorValue(sub_expr);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  if (expr->op() == Token::EQ_STRICT) {
    Heap::RootListIndex nil_value = nil == kNullValue
                                        ? Heap::kNullValueRootIndex
                                        : Heap::kUndefinedValueRootIndex;
    __ LoadRoot(r4, nil_value);
    __ cmp(r3, r4);
    Split(eq, if_true, if_false, fall_through);
  } else {
    __ JumpIfSmi(r3, if_false);
    __ LoadP(r3, FieldMemOperand(r3, HeapObject::kMapOffset));
    __ lbz(r4, FieldMemOperand(r3, Map::kBitFieldOffset));
    __ andi(r0, r4, Operand(1 << Map::kIsUndetectable));
    Split(ne, if_true, if_false, fall_through, cr0);
  }
  context()->Plug(if_true, if_false);
}


Register FullCodeGenerator::result_register() { return r3; }


Register FullCodeGenerator::context_register() { return cp; }

void FullCodeGenerator::LoadFromFrameField(int frame_offset, Register value) {
  DCHECK_EQ(static_cast<int>(POINTER_SIZE_ALIGN(frame_offset)), frame_offset);
  __ LoadP(value, MemOperand(fp, frame_offset), r0);
}

void FullCodeGenerator::StoreToFrameField(int frame_offset, Register value) {
  DCHECK_EQ(static_cast<int>(POINTER_SIZE_ALIGN(frame_offset)), frame_offset);
  __ StoreP(value, MemOperand(fp, frame_offset), r0);
}


void FullCodeGenerator::LoadContextField(Register dst, int context_index) {
  __ LoadP(dst, ContextMemOperand(cp, context_index), r0);
}


void FullCodeGenerator::PushFunctionArgumentForContextAllocation() {
  DeclarationScope* closure_scope = scope()->GetClosureScope();
  if (closure_scope->is_script_scope() ||
      closure_scope->is_module_scope()) {
    // Contexts nested in the native context have a canonical empty function
    // as their closure, not the anonymous closure containing the global
    // code.
    __ LoadNativeContextSlot(Context::CLOSURE_INDEX, ip);
  } else if (closure_scope->is_eval_scope()) {
    // Contexts created by a call to eval have the same closure as the
    // context calling eval, not the anonymous closure containing the eval
    // code.  Fetch it from the context.
    __ LoadP(ip, ContextMemOperand(cp, Context::CLOSURE_INDEX));
  } else {
    DCHECK(closure_scope->is_function_scope());
    __ LoadP(ip, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  }
  PushOperand(ip);
}


#undef __


void BackEdgeTable::PatchAt(Code* unoptimized_code, Address pc,
                            BackEdgeState target_state,
                            Code* replacement_code) {
  Address mov_address = Assembler::target_address_from_return_address(pc);
  Address cmp_address = mov_address - 2 * Assembler::kInstrSize;
  Isolate* isolate = unoptimized_code->GetIsolate();
  CodePatcher patcher(isolate, cmp_address, 1);

  switch (target_state) {
    case INTERRUPT: {
      //  <decrement profiling counter>
      //         cmpi    r6, 0
      //         bge     <ok>            ;; not changed
      //         mov     r12, <interrupt stub address>
      //         mtlr    r12
      //         blrl
      //  <reset profiling counter>
      //  ok-label
      patcher.masm()->cmpi(r6, Operand::Zero());
      break;
    }
    case ON_STACK_REPLACEMENT:
      //  <decrement profiling counter>
      //         crset
      //         bge     <ok>            ;; not changed
      //         mov     r12, <on-stack replacement address>
      //         mtlr    r12
      //         blrl
      //  <reset profiling counter>
      //  ok-label ----- pc_after points here

      // Set the LT bit such that bge is a NOP
      patcher.masm()->crset(Assembler::encode_crbit(cr7, CR_LT));
      break;
  }

  // Replace the stack check address in the mov sequence with the
  // entry address of the replacement code.
  Assembler::set_target_address_at(isolate, mov_address, unoptimized_code,
                                   replacement_code->entry());

  unoptimized_code->GetHeap()->incremental_marking()->RecordCodeTargetPatch(
      unoptimized_code, mov_address, replacement_code);
}


BackEdgeTable::BackEdgeState BackEdgeTable::GetBackEdgeState(
    Isolate* isolate, Code* unoptimized_code, Address pc) {
  Address mov_address = Assembler::target_address_from_return_address(pc);
  Address cmp_address = mov_address - 2 * Assembler::kInstrSize;
#ifdef DEBUG
  Address interrupt_address =
      Assembler::target_address_at(mov_address, unoptimized_code);
#endif

  if (Assembler::IsCmpImmediate(Assembler::instr_at(cmp_address))) {
    DCHECK(interrupt_address == isolate->builtins()->InterruptCheck()->entry());
    return INTERRUPT;
  }

  DCHECK(Assembler::IsCrSet(Assembler::instr_at(cmp_address)));

  DCHECK(interrupt_address ==
         isolate->builtins()->OnStackReplacement()->entry());
  return ON_STACK_REPLACEMENT;
}
}  // namespace internal
}  // namespace v8
#endif  // V8_TARGET_ARCH_PPC
