// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_INCREMENTAL_MARKING_H_
#define V8_HEAP_INCREMENTAL_MARKING_H_

#include <cstdint>

#include "src/base/logging.h"
#include "src/base/platform/mutex.h"
#include "src/base/platform/time.h"
#include "src/common/globals.h"
#include "src/heap/heap.h"
#include "src/heap/incremental-marking-job.h"
#include "src/heap/mark-compact.h"
#include "src/tasks/cancelable-task.h"

namespace v8 {
namespace internal {

class HeapObject;
class MarkBit;
class Map;
class Object;
class PagedSpace;

// Describes in which context IncrementalMarking::Step() is used in. This
// information is used when marking finishes and for marking progress
// heuristics.
enum class StepOrigin {
  // The caller of Step() is not allowed to complete marking right away. A task
  // is scheduled to complete the GC. When the task isn't
  // run soon enough, the stack guard mechanism will be used.
  kV8,

  // The caller of Step() will complete marking by running the GC right
  // afterwards.
  kTask
};

class V8_EXPORT_PRIVATE IncrementalMarking final {
 public:
  class V8_NODISCARD PauseBlackAllocationScope {
   public:
    explicit PauseBlackAllocationScope(IncrementalMarking* marking)
        : marking_(marking) {
      if (marking_->black_allocation()) {
        paused_ = true;
        marking_->PauseBlackAllocation();
      }
    }

    ~PauseBlackAllocationScope() {
      if (paused_) {
        marking_->StartBlackAllocation();
      }
    }

   private:
    IncrementalMarking* marking_;
    bool paused_ = false;
  };

  // It's hard to know how much work the incremental marker should do to make
  // progress in the face of the mutator creating new work for it.  We start
  // of at a moderate rate of work and gradually increase the speed of the
  // incremental marker until it completes.
  // Do some marking every time this much memory has been allocated or that many
  // heavy (color-checking) write barriers have been invoked.
  static const size_t kYoungGenerationAllocatedThreshold = 64 * KB;
  static const size_t kOldGenerationAllocatedThreshold = 256 * KB;
  static const size_t kMinStepSizeInBytes = 64 * KB;

  static constexpr v8::base::TimeDelta kMaxStepSizeOnTask =
      v8::base::TimeDelta::FromMilliseconds(1);
  static constexpr v8::base::TimeDelta kMaxStepSizeOnAllocation =
      v8::base::TimeDelta::FromMilliseconds(5);

#ifndef DEBUG
  static constexpr size_t kV8ActivationThreshold = 8 * MB;
  static constexpr size_t kEmbedderActivationThreshold = 8 * MB;
#else
  static constexpr size_t kV8ActivationThreshold = 0;
  static constexpr size_t kEmbedderActivationThreshold = 0;
#endif

  V8_INLINE void TransferColor(HeapObject from, HeapObject to);

  IncrementalMarking(Heap* heap, WeakObjects* weak_objects);

  IncrementalMarking(const IncrementalMarking&) = delete;
  IncrementalMarking& operator=(const IncrementalMarking&) = delete;

  MarkingMode marking_mode() const { return marking_mode_; }

  bool IsStopped() const { return !IsMarking(); }
  bool IsMarking() const { return marking_mode_ != MarkingMode::kNoMarking; }
  bool IsMajorMarkingComplete() const {
    return IsMajorMarking() && ShouldFinalize();
  }

  bool CollectionRequested() const {
    return collection_requested_via_stack_guard_;
  }

  bool ShouldFinalize() const;

  bool CanBeStarted() const;

  void Start(GarbageCollector garbage_collector,
             GarbageCollectionReason gc_reason);
  // Returns true if incremental marking was running and false otherwise.
  bool Stop();

  void UpdateMarkingWorklistAfterScavenge();
  void UpdateMarkedBytesAfterScavenge(size_t dead_bytes_in_new_space);

  // Performs incremental marking step and finalizes marking if complete.
  void AdvanceAndFinalizeIfComplete();

  // Performs incremental marking step and finalizes marking if the stack guard
  // was already armed. If marking is complete but the stack guard wasn't armed
  // yet, a finalization task is scheduled.
  void AdvanceAndFinalizeIfNecessary();

  // Performs incremental marking step and schedules job for finalization if
  // marking completes.
  void AdvanceOnAllocation();

  bool IsAheadOfSchedule() const;

  void MarkBlackBackground(HeapObject obj, int object_size);

  bool IsCompacting() { return IsMarking() && is_compacting_; }

  Heap* heap() const { return heap_; }
  Isolate* isolate() const;

  IncrementalMarkingJob* incremental_marking_job() const {
    return incremental_marking_job_.get();
  }

  bool black_allocation() { return black_allocation_; }

  bool IsBelowActivationThresholds() const;

  void IncrementLiveBytesBackground(MemoryChunk* chunk, intptr_t by) {
    base::MutexGuard guard(&background_live_bytes_mutex_);
    background_live_bytes_[chunk] += by;
  }

  bool IsMinorMarking() const {
    return marking_mode_ == MarkingMode::kMinorMarking;
  }
  bool IsMajorMarking() const {
    return marking_mode_ == MarkingMode::kMajorMarking;
  }

  void MarkRootsForTesting();

  // Performs incremental marking step for unit tests.
  void AdvanceForTesting(v8::base::TimeDelta max_duration,
                         size_t max_bytes_to_mark = SIZE_MAX);

 private:
  class IncrementalMarkingRootMarkingVisitor;

  class Observer : public AllocationObserver {
   public:
    Observer(IncrementalMarking* incremental_marking, intptr_t step_size)
        : AllocationObserver(step_size),
          incremental_marking_(incremental_marking) {}

    void Step(int bytes_allocated, Address, size_t) override;

   private:
    IncrementalMarking* incremental_marking_;
  };

  void StartMarkingMajor();
  void StartMarkingMinor();

  void StartBlackAllocation();
  void PauseBlackAllocation();
  void FinishBlackAllocation();

  void MarkRoots();
  // Returns true if the function succeeds in transitioning the object
  // from white to grey.
  bool WhiteToGreyAndPush(HeapObject obj);
  void PublishWriteBarrierWorklists();

  // Fetches marked byte counters from the concurrent marker.
  void FetchBytesMarkedConcurrently();
  size_t GetScheduledBytes(StepOrigin step_origin);

  bool ShouldWaitForTask();
  bool TryInitializeTaskTimeout();
  double CurrentTimeToMarkingTask() const;

  void EmbedderStep(double expected_duration_ms, double* duration_ms);
  void Step(v8::base::TimeDelta max_duration, size_t max_bytes_to_process,
            StepOrigin step_origin);

  size_t OldGenerationSizeOfObjects() const;

  MarkingState* marking_state() { return marking_state_; }
  MarkingWorklists::Local* local_marking_worklists() const {
    return current_local_marking_worklists_;
  }

  Heap* const heap_;
  MarkCompactCollector* const major_collector_;
  MinorMarkSweepCollector* const minor_collector_;
  WeakObjects* weak_objects_;
  MarkingWorklists::Local* current_local_marking_worklists_ = nullptr;
  MarkingState* const marking_state_;
  double start_time_ms_ = 0.0;
  size_t main_thread_marked_bytes_ = 0;
  // A sample of concurrent_marking()->TotalMarkedBytes() at the last
  // incremental marking step.
  size_t bytes_marked_concurrently_ = 0;
  MarkingMode marking_mode_ = MarkingMode::kNoMarking;

  bool is_compacting_ = false;
  bool black_allocation_ = false;
  bool completion_task_scheduled_ = false;
  double completion_task_timeout_ = 0.0;
  bool collection_requested_via_stack_guard_ = false;
  std::unique_ptr<IncrementalMarkingJob> incremental_marking_job_;
  Observer new_generation_observer_;
  Observer old_generation_observer_;
  base::Mutex background_live_bytes_mutex_;
  std::unordered_map<MemoryChunk*, intptr_t> background_live_bytes_;
  std::unique_ptr<::heap::base::IncrementalMarkingSchedule> schedule_;

  friend class IncrementalMarkingJob;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_INCREMENTAL_MARKING_H_
