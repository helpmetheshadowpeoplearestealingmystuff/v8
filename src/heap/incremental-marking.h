// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_INCREMENTAL_MARKING_H_
#define V8_HEAP_INCREMENTAL_MARKING_H_

#include "src/cancelable-task.h"
#include "src/execution.h"
#include "src/heap/heap.h"
#include "src/heap/incremental-marking-job.h"
#include "src/heap/mark-compact.h"
#include "src/heap/spaces.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

// Forward declarations.
class MarkBit;
class PagedSpace;

class IncrementalMarking {
 public:
  enum State { STOPPED, SWEEPING, MARKING, COMPLETE };

  enum CompletionAction { GC_VIA_STACK_GUARD, NO_GC_VIA_STACK_GUARD };

  enum ForceMarkingAction { FORCE_MARKING, DO_NOT_FORCE_MARKING };

  enum ForceCompletionAction { FORCE_COMPLETION, DO_NOT_FORCE_COMPLETION };

  enum GCRequestType { NONE, COMPLETE_MARKING, FINALIZATION };

  struct StepActions {
    StepActions(CompletionAction complete_action_,
                ForceMarkingAction force_marking_,
                ForceCompletionAction force_completion_)
        : completion_action(complete_action_),
          force_marking(force_marking_),
          force_completion(force_completion_) {}

    CompletionAction completion_action;
    ForceMarkingAction force_marking;
    ForceCompletionAction force_completion;
  };

  static StepActions IdleStepActions();

  explicit IncrementalMarking(Heap* heap);

  static void Initialize();

  State state() {
    DCHECK(state_ == STOPPED || FLAG_incremental_marking);
    return state_;
  }

  bool should_hurry() { return should_hurry_; }
  void set_should_hurry(bool val) { should_hurry_ = val; }

  bool finalize_marking_completed() const {
    return finalize_marking_completed_;
  }

  void SetWeakClosureWasOverApproximatedForTesting(bool val) {
    finalize_marking_completed_ = val;
  }

  inline bool IsStopped() { return state() == STOPPED; }

  inline bool IsSweeping() { return state() == SWEEPING; }

  INLINE(bool IsMarking()) { return state() >= MARKING; }

  inline bool IsMarkingIncomplete() { return state() == MARKING; }

  inline bool IsComplete() { return state() == COMPLETE; }

  inline bool IsReadyToOverApproximateWeakClosure() const {
    return request_type_ == FINALIZATION && !finalize_marking_completed_;
  }

  GCRequestType request_type() const { return request_type_; }

  void reset_request_type() { request_type_ = NONE; }

  bool CanBeActivated();

  bool ShouldActivateEvenWithoutIdleNotification();

  bool WasActivated();

  void Start(const char* reason = nullptr);

  void FinalizeIncrementally();

  void UpdateMarkingDequeAfterScavenge();

  void Hurry();

  void Finalize();

  void Stop();

  void FinalizeMarking(CompletionAction action);

  void MarkingComplete(CompletionAction action);

  void Epilogue();

  // Performs incremental marking steps until deadline_in_ms is reached. It
  // returns the remaining time that cannot be used for incremental marking
  // anymore because a single step would exceed the deadline.
  double AdvanceIncrementalMarking(double deadline_in_ms,
                                   StepActions step_actions);

  // It's hard to know how much work the incremental marker should do to make
  // progress in the face of the mutator creating new work for it.  We start
  // of at a moderate rate of work and gradually increase the speed of the
  // incremental marker until it completes.
  // Do some marking every time this much memory has been allocated or that many
  // heavy (color-checking) write barriers have been invoked.
  static const intptr_t kAllocatedThreshold = 65536;
  static const intptr_t kWriteBarriersInvokedThreshold = 32768;
  // Cap the write barrier counter to avoid overflows.
  static const intptr_t kMaxWriteBarrierCounter = 1 << 20;
  // These constants are arbitrary and chosen based on benchmarks.
  static const intptr_t kBytesToMarkPerAllocatedByte = 3;
  static const intptr_t kBytesToMarkPerWriteBarrier = 10;
  static const intptr_t kMinIncrementalStepDurationInMs = 1;

  // This is the upper bound for how many times we allow finalization of
  // incremental marking to be postponed.
  static const size_t kMaxIdleMarkingDelayCounter = 3;

  void FinalizeSweeping();

  void OldSpaceStep(intptr_t allocated);

  intptr_t Step(intptr_t allocated, CompletionAction action,
                ForceMarkingAction marking = DO_NOT_FORCE_MARKING,
                ForceCompletionAction completion = FORCE_COMPLETION);

  inline void RestartIfNotMarking();

  static void RecordWriteFromCode(HeapObject* obj, Object** slot,
                                  Isolate* isolate);

  static void RecordWriteOfCodeEntryFromCode(JSFunction* host, Object** slot,
                                             Isolate* isolate);

  // Record a slot for compaction.  Returns false for objects that are
  // guaranteed to be rescanned or not guaranteed to survive.
  //
  // No slots in white objects should be recorded, as some slots are typed and
  // cannot be interpreted correctly if the underlying object does not survive
  // the incremental cycle (stays white).
  INLINE(bool BaseRecordWrite(HeapObject* obj, Object* value));
  INLINE(void RecordWrite(HeapObject* obj, Object** slot, Object* value));
  INLINE(void RecordWriteIntoCode(Code* host, RelocInfo* rinfo, Object* value));
  INLINE(void RecordWriteOfCodeEntry(JSFunction* host, Object** slot,
                                     Code* value));


  void RecordWriteSlow(HeapObject* obj, Object** slot, Object* value);
  void RecordWriteIntoCodeSlow(Code* host, RelocInfo* rinfo, Object* value);
  void RecordWriteOfCodeEntrySlow(JSFunction* host, Object** slot, Code* value);
  void RecordCodeTargetPatch(Code* host, Address pc, HeapObject* value);
  void RecordCodeTargetPatch(Address pc, HeapObject* value);

  void WhiteToGreyAndPush(HeapObject* obj, MarkBit mark_bit);

  inline void SetOldSpacePageFlags(MemoryChunk* chunk) {
    SetOldSpacePageFlags(chunk, IsMarking(), IsCompacting());
  }

  inline void SetNewSpacePageFlags(Page* chunk) {
    SetNewSpacePageFlags(chunk, IsMarking());
  }

  bool IsCompacting() { return IsMarking() && is_compacting_; }

  void ActivateGeneratedStub(Code* stub);

  void NotifyIncompleteScanOfObject(int unscanned_bytes) {
    unscanned_bytes_of_large_object_ = unscanned_bytes;
  }

  void ClearIdleMarkingDelayCounter();

  bool IsIdleMarkingDelayCounterLimitReached();

  static void MarkGrey(Heap* heap, HeapObject* object);

  static void MarkBlack(HeapObject* object, int size);

  static void TransferMark(Heap* heap, Address old_start, Address new_start);

  // Returns true if the color transfer requires live bytes updating.
  INLINE(static bool TransferColor(HeapObject* from, HeapObject* to,
                                   int size)) {
    MarkBit from_mark_bit = ObjectMarking::MarkBitFrom(from);
    MarkBit to_mark_bit = ObjectMarking::MarkBitFrom(to);

    if (Marking::IsBlack(to_mark_bit)) {
      DCHECK(to->GetHeap()->incremental_marking()->black_allocation());
      return false;
    }

    DCHECK(Marking::IsWhite(to_mark_bit));
    if (from_mark_bit.Get()) {
      to_mark_bit.Set();
      if (from_mark_bit.Next().Get()) {
        to_mark_bit.Next().Set();
        return true;
      }
    }
    return false;
  }

  void IterateBlackObject(HeapObject* object);

  Heap* heap() const { return heap_; }

  IncrementalMarkingJob* incremental_marking_job() {
    return &incremental_marking_job_;
  }

  bool black_allocation() { return black_allocation_; }

  void StartBlackAllocationForTesting() { StartBlackAllocation(); }

 private:
  class Observer : public AllocationObserver {
   public:
    Observer(IncrementalMarking& incremental_marking, intptr_t step_size)
        : AllocationObserver(step_size),
          incremental_marking_(incremental_marking) {}

    void Step(int bytes_allocated, Address, size_t) override {
      incremental_marking_.Step(bytes_allocated,
                                IncrementalMarking::GC_VIA_STACK_GUARD);
    }

   private:
    IncrementalMarking& incremental_marking_;
  };

  int64_t SpaceLeftInOldSpace();

  void ResetStepCounters();

  void StartMarking();

  void StartBlackAllocation();
  void FinishBlackAllocation();

  void MarkRoots();
  void MarkObjectGroups();
  void ProcessWeakCells();
  // Retain dying maps for <FLAG_retain_maps_for_n_gc> garbage collections to
  // increase chances of reusing of map transition tree in future.
  void RetainMaps();

  void ActivateIncrementalWriteBarrier(PagedSpace* space);
  static void ActivateIncrementalWriteBarrier(NewSpace* space);
  void ActivateIncrementalWriteBarrier();

  static void DeactivateIncrementalWriteBarrierForSpace(PagedSpace* space);
  static void DeactivateIncrementalWriteBarrierForSpace(NewSpace* space);
  void DeactivateIncrementalWriteBarrier();

  static void SetOldSpacePageFlags(MemoryChunk* chunk, bool is_marking,
                                   bool is_compacting);

  static void SetNewSpacePageFlags(MemoryChunk* chunk, bool is_marking);

  INLINE(void ProcessMarkingDeque());

  INLINE(intptr_t ProcessMarkingDeque(
      intptr_t bytes_to_process,
      ForceCompletionAction completion = DO_NOT_FORCE_COMPLETION));

  INLINE(void VisitObject(Map* map, HeapObject* obj, int size));

  void IncrementIdleMarkingDelayCounter();

  Heap* heap_;

  Observer observer_;

  State state_;
  bool is_compacting_;

  bool should_hurry_;
  intptr_t allocated_;
  intptr_t write_barriers_invoked_since_last_step_;
  size_t idle_marking_delay_counter_;

  int unscanned_bytes_of_large_object_;

  bool was_activated_;

  bool black_allocation_;

  bool finalize_marking_completed_;

  int incremental_marking_finalization_rounds_;

  GCRequestType request_type_;

  IncrementalMarkingJob incremental_marking_job_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(IncrementalMarking);
};
}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_INCREMENTAL_MARKING_H_
