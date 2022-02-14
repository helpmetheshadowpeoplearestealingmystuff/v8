// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/marker.h"

#include <cstddef>
#include <cstdint>
#include <memory>

#include "include/cppgc/heap-consistency.h"
#include "include/cppgc/platform.h"
#include "src/base/platform/time.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/heap-page.h"
#include "src/heap/cppgc/heap-visitor.h"
#include "src/heap/cppgc/heap.h"
#include "src/heap/cppgc/liveness-broker.h"
#include "src/heap/cppgc/marking-state.h"
#include "src/heap/cppgc/marking-visitor.h"
#include "src/heap/cppgc/process-heap.h"
#include "src/heap/cppgc/stats-collector.h"
#include "src/heap/cppgc/write-barrier.h"

#if defined(CPPGC_CAGED_HEAP)
#include "include/cppgc/internal/caged-heap-local-data.h"
#endif

namespace cppgc {
namespace internal {

namespace {

bool EnterIncrementalMarkingIfNeeded(Marker::MarkingConfig config,
                                     HeapBase& heap) {
  if (config.marking_type == Marker::MarkingConfig::MarkingType::kIncremental ||
      config.marking_type ==
          Marker::MarkingConfig::MarkingType::kIncrementalAndConcurrent) {
    WriteBarrier::IncrementalOrConcurrentMarkingFlagUpdater::Enter();
#if defined(CPPGC_CAGED_HEAP)
    heap.caged_heap().local_data().is_incremental_marking_in_progress = true;
#endif  // defined(CPPGC_CAGED_HEAP)
    return true;
  }
  return false;
}

bool ExitIncrementalMarkingIfNeeded(Marker::MarkingConfig config,
                                    HeapBase& heap) {
  if (config.marking_type == Marker::MarkingConfig::MarkingType::kIncremental ||
      config.marking_type ==
          Marker::MarkingConfig::MarkingType::kIncrementalAndConcurrent) {
    WriteBarrier::IncrementalOrConcurrentMarkingFlagUpdater::Exit();
#if defined(CPPGC_CAGED_HEAP)
    heap.caged_heap().local_data().is_incremental_marking_in_progress = false;
#endif  // defined(CPPGC_CAGED_HEAP)
    return true;
  }
  return false;
}

// Visits ranges that were recorded in the generational barrier for ranges.
void VisitRememberedObjects(HeapBase& heap, Visitor& visitor,
                            MutatorMarkingState& mutator_marking_state) {
#if defined(CPPGC_YOUNG_GENERATION)
  for (HeapObjectHeader* source_hoh : heap.remembered_source_objects()) {
    DCHECK(source_hoh);
    // The age checking in the generational barrier is imprecise, since a card
    // may have mixed young/old objects. Check here precisely if the object is
    // old.
    if (source_hoh->IsYoung()) continue;
    // The design of young generation requires collections to be executed at the
    // top level (with the guarantee that no objects are currently being in
    // construction). This can be ensured by running young GCs from safe points
    // or by reintroducing nested allocation scopes that avoid finalization.
    DCHECK(!source_hoh->template IsInConstruction<AccessMode::kNonAtomic>());

    const TraceCallback trace_callback =
        GlobalGCInfoTable::GCInfoFromIndex(source_hoh->GetGCInfoIndex()).trace;

    // Process eagerly to avoid reaccounting.
    trace_callback(&visitor, source_hoh->ObjectStart());
  }
#endif
}

// Visit remembered set that was recorded in the generational barrier.
void VisitRememberedSlots(HeapBase& heap,
                          MutatorMarkingState& mutator_marking_state) {
#if defined(CPPGC_YOUNG_GENERATION)
  for (void* slot : heap.remembered_slots()) {
    // Slot must always point to a valid, not freed object.
    auto& slot_header = BasePage::FromInnerAddress(&heap, slot)
                            ->ObjectHeaderFromInnerAddress(slot);
    // The age checking in the generational barrier is imprecise, since a card
    // may have mixed young/old objects. Check here precisely if the object is
    // old.
    if (slot_header.IsYoung()) continue;
    // The design of young generation requires collections to be executed at the
    // top level (with the guarantee that no objects are currently being in
    // construction). This can be ensured by running young GCs from safe points
    // or by reintroducing nested allocation scopes that avoid finalization.
    DCHECK(!slot_header.template IsInConstruction<AccessMode::kNonAtomic>());

    void* value = *reinterpret_cast<void**>(slot);
    // Slot could be updated to nullptr or kSentinelPointer by the mutator.
    if (value == kSentinelPointer || value == nullptr) continue;

#if DEBUG
    // Check that the slot can not point to a freed object.
    HeapObjectHeader& header =
        BasePage::FromPayload(value)->ObjectHeaderFromInnerAddress(value);
    DCHECK(!header.IsFree());
#endif

    mutator_marking_state.DynamicallyMarkAddress(static_cast<Address>(value));
  }
#endif
}

static constexpr size_t kDefaultDeadlineCheckInterval = 150u;

template <size_t kDeadlineCheckInterval = kDefaultDeadlineCheckInterval,
          typename WorklistLocal, typename Callback>
bool DrainWorklistWithBytesAndTimeDeadline(BasicMarkingState& marking_state,
                                           size_t marked_bytes_deadline,
                                           v8::base::TimeTicks time_deadline,
                                           WorklistLocal& worklist_local,
                                           Callback callback) {
  return DrainWorklistWithPredicate<kDeadlineCheckInterval>(
      [&marking_state, marked_bytes_deadline, time_deadline]() {
        return (marked_bytes_deadline <= marking_state.marked_bytes()) ||
               (time_deadline <= v8::base::TimeTicks::Now());
      },
      worklist_local, callback);
}

size_t GetNextIncrementalStepDuration(IncrementalMarkingSchedule& schedule,
                                      HeapBase& heap) {
  return schedule.GetNextIncrementalStepDuration(
      heap.stats_collector()->allocated_object_size());
}

}  // namespace

constexpr v8::base::TimeDelta MarkerBase::kMaximumIncrementalStepDuration;

MarkerBase::IncrementalMarkingTask::IncrementalMarkingTask(
    MarkerBase* marker, MarkingConfig::StackState stack_state)
    : marker_(marker),
      stack_state_(stack_state),
      handle_(Handle::NonEmptyTag{}) {}

// static
MarkerBase::IncrementalMarkingTask::Handle
MarkerBase::IncrementalMarkingTask::Post(cppgc::TaskRunner* runner,
                                         MarkerBase* marker) {
  // Incremental GC is possible only via the GCInvoker, so getting here
  // guarantees that either non-nestable tasks or conservative stack
  // scanning are supported. This is required so that the incremental
  // task can safely finalize GC if needed.
  DCHECK_IMPLIES(marker->heap().stack_support() !=
                     HeapBase::StackSupport::kSupportsConservativeStackScan,
                 runner->NonNestableTasksEnabled());
  MarkingConfig::StackState stack_state_for_task =
      runner->NonNestableTasksEnabled()
          ? MarkingConfig::StackState::kNoHeapPointers
          : MarkingConfig::StackState::kMayContainHeapPointers;
  auto task =
      std::make_unique<IncrementalMarkingTask>(marker, stack_state_for_task);
  auto handle = task->handle_;
  if (runner->NonNestableTasksEnabled()) {
    runner->PostNonNestableTask(std::move(task));
  } else {
    runner->PostTask(std::move(task));
  }
  return handle;
}

void MarkerBase::IncrementalMarkingTask::Run() {
  if (handle_.IsCanceled()) return;

  StatsCollector::EnabledScope stats_scope(marker_->heap().stats_collector(),
                                           StatsCollector::kIncrementalMark);

  if (marker_->IncrementalMarkingStep(stack_state_)) {
    // Incremental marking is done so should finalize GC.
    marker_->heap().FinalizeIncrementalGarbageCollectionIfNeeded(stack_state_);
  }
}

MarkerBase::MarkerBase(HeapBase& heap, cppgc::Platform* platform,
                       MarkingConfig config)
    : heap_(heap),
      config_(config),
      platform_(platform),
      foreground_task_runner_(platform_->GetForegroundTaskRunner()),
      mutator_marking_state_(heap, marking_worklists_,
                             heap.compactor().compaction_worklists()) {}

MarkerBase::~MarkerBase() {
  // The fixed point iteration may have found not-fully-constructed objects.
  // Such objects should have already been found through the stack scan though
  // and should thus already be marked.
  if (!marking_worklists_.not_fully_constructed_worklist()->IsEmpty()) {
#if DEBUG
    DCHECK_NE(MarkingConfig::StackState::kNoHeapPointers, config_.stack_state);
    std::unordered_set<HeapObjectHeader*> objects =
        mutator_marking_state_.not_fully_constructed_worklist().Extract();
    for (HeapObjectHeader* object : objects) DCHECK(object->IsMarked());
#else
    marking_worklists_.not_fully_constructed_worklist()->Clear();
#endif
  }

  // |discovered_ephemeron_pairs_worklist_| may still hold ephemeron pairs with
  // dead keys.
  if (!marking_worklists_.discovered_ephemeron_pairs_worklist()->IsEmpty()) {
#if DEBUG
    MarkingWorklists::EphemeronPairItem item;
    while (mutator_marking_state_.discovered_ephemeron_pairs_worklist().Pop(
        &item)) {
      DCHECK(!HeapObjectHeader::FromObject(item.key).IsMarked());
    }
#else
    marking_worklists_.discovered_ephemeron_pairs_worklist()->Clear();
#endif
  }

  marking_worklists_.weak_containers_worklist()->Clear();
}

class MarkerBase::IncrementalMarkingAllocationObserver final
    : public StatsCollector::AllocationObserver {
 public:
  static constexpr size_t kMinAllocatedBytesPerStep = 256 * kKB;

  explicit IncrementalMarkingAllocationObserver(MarkerBase& marker)
      : marker_(marker) {}

  void AllocatedObjectSizeIncreased(size_t delta) final {
    current_allocated_size_ += delta;
    if (current_allocated_size_ > kMinAllocatedBytesPerStep) {
      marker_.AdvanceMarkingOnAllocation();
      current_allocated_size_ = 0;
    }
  }

 private:
  MarkerBase& marker_;
  size_t current_allocated_size_ = 0;
};

void MarkerBase::StartMarking() {
  DCHECK(!is_marking_);
  StatsCollector::EnabledScope stats_scope(
      heap().stats_collector(),
      config_.marking_type == MarkingConfig::MarkingType::kAtomic
          ? StatsCollector::kAtomicMark
          : StatsCollector::kIncrementalMark);

  heap().stats_collector()->NotifyMarkingStarted(config_.collection_type,
                                                 config_.is_forced_gc);

  is_marking_ = true;
  if (EnterIncrementalMarkingIfNeeded(config_, heap())) {
    StatsCollector::EnabledScope inner_stats_scope(
        heap().stats_collector(), StatsCollector::kMarkIncrementalStart);

    // Performing incremental or concurrent marking.
    schedule_.NotifyIncrementalMarkingStart();
    // Scanning the stack is expensive so we only do it at the atomic pause.
    VisitRoots(MarkingConfig::StackState::kNoHeapPointers);
    ScheduleIncrementalMarkingTask();
    if (config_.marking_type ==
        MarkingConfig::MarkingType::kIncrementalAndConcurrent) {
      mutator_marking_state_.Publish();
      concurrent_marker_->Start();
      concurrent_marking_active_ = true;
    }
    incremental_marking_allocation_observer_ =
        std::make_unique<IncrementalMarkingAllocationObserver>(*this);
    heap().stats_collector()->RegisterObserver(
        incremental_marking_allocation_observer_.get());
  }
}
void MarkerBase::HandleNotFullyConstructedObjects() {
  if (config_.stack_state == MarkingConfig::StackState::kNoHeapPointers) {
    mutator_marking_state_.FlushNotFullyConstructedObjects();
  } else {
    MarkNotFullyConstructedObjects();
  }
}

void MarkerBase::EnterAtomicPause(MarkingConfig::StackState stack_state) {
  StatsCollector::EnabledScope top_stats_scope(heap().stats_collector(),
                                               StatsCollector::kAtomicMark);
  StatsCollector::EnabledScope stats_scope(heap().stats_collector(),
                                           StatsCollector::kMarkAtomicPrologue);

  if (ExitIncrementalMarkingIfNeeded(config_, heap())) {
    // Cancel remaining incremental tasks. Concurrent marking jobs are left to
    // run in parallel with the atomic pause until the mutator thread runs out
    // of work.
    incremental_marking_handle_.Cancel();
    heap().stats_collector()->UnregisterObserver(
        incremental_marking_allocation_observer_.get());
    incremental_marking_allocation_observer_.reset();
  }
  config_.stack_state = stack_state;
  config_.marking_type = MarkingConfig::MarkingType::kAtomic;
  mutator_marking_state_.set_in_atomic_pause();

  {
    // VisitRoots also resets the LABs.
    VisitRoots(config_.stack_state);
    HandleNotFullyConstructedObjects();
  }
  if (heap().marking_support() ==
      MarkingConfig::MarkingType::kIncrementalAndConcurrent) {
    // Start parallel marking.
    mutator_marking_state_.Publish();
    if (concurrent_marking_active_) {
      concurrent_marker_->NotifyIncrementalMutatorStepCompleted();
    } else {
      concurrent_marker_->Start();
      concurrent_marking_active_ = true;
    }
  }
}

void MarkerBase::LeaveAtomicPause() {
  {
    StatsCollector::EnabledScope top_stats_scope(heap().stats_collector(),
                                                 StatsCollector::kAtomicMark);
    StatsCollector::EnabledScope stats_scope(
        heap().stats_collector(), StatsCollector::kMarkAtomicEpilogue);
    DCHECK(!incremental_marking_handle_);
    heap().stats_collector()->NotifyMarkingCompleted(
        // GetOverallMarkedBytes also includes concurrently marked bytes.
        schedule_.GetOverallMarkedBytes());
    is_marking_ = false;
  }
  {
    // Weakness callbacks are forbidden from allocating objects.
    cppgc::subtle::DisallowGarbageCollectionScope disallow_gc_scope(heap_);
    ProcessWeakness();
  }
  // TODO(chromium:1056170): It would be better if the call to Unlock was
  // covered by some cppgc scope.
  g_process_mutex.Pointer()->Unlock();
  heap().SetStackStateOfPrevGC(config_.stack_state);
}

void MarkerBase::FinishMarking(MarkingConfig::StackState stack_state) {
  DCHECK(is_marking_);
  EnterAtomicPause(stack_state);
  {
    StatsCollector::EnabledScope stats_scope(heap().stats_collector(),
                                             StatsCollector::kAtomicMark);
    CHECK(AdvanceMarkingWithLimits(v8::base::TimeDelta::Max(), SIZE_MAX));
    mutator_marking_state_.Publish();
  }
  LeaveAtomicPause();
}

void MarkerBase::ProcessWeakness() {
  DCHECK_EQ(MarkingConfig::MarkingType::kAtomic, config_.marking_type);

  StatsCollector::EnabledScope stats_scope(heap().stats_collector(),
                                           StatsCollector::kAtomicWeak);

  heap().GetWeakPersistentRegion().Trace(&visitor());
  // Processing cross-thread handles requires taking the process lock.
  g_process_mutex.Get().AssertHeld();
  CHECK(visited_cross_thread_persistents_in_atomic_pause_);
  heap().GetWeakCrossThreadPersistentRegion().Trace(&visitor());

  // Call weak callbacks on objects that may now be pointing to dead objects.
  MarkingWorklists::WeakCallbackItem item;
  LivenessBroker broker = LivenessBrokerFactory::Create();
  MarkingWorklists::WeakCallbackWorklist::Local& local =
      mutator_marking_state_.weak_callback_worklist();
  while (local.Pop(&item)) {
    item.callback(broker, item.parameter);
  }

  // Weak callbacks should not add any new objects for marking.
  DCHECK(marking_worklists_.marking_worklist()->IsEmpty());
}

void MarkerBase::VisitRoots(MarkingConfig::StackState stack_state) {
  StatsCollector::EnabledScope stats_scope(heap().stats_collector(),
                                           StatsCollector::kMarkVisitRoots);

  // Reset LABs before scanning roots. LABs are cleared to allow
  // ObjectStartBitmap handling without considering LABs.
  heap().object_allocator().ResetLinearAllocationBuffers();

  {
    {
      StatsCollector::DisabledScope inner_stats_scope(
          heap().stats_collector(), StatsCollector::kMarkVisitPersistents);
      heap().GetStrongPersistentRegion().Trace(&visitor());
    }
  }

  if (stack_state != MarkingConfig::StackState::kNoHeapPointers) {
    StatsCollector::DisabledScope stack_stats_scope(
        heap().stats_collector(), StatsCollector::kMarkVisitStack);
    heap().stack()->IteratePointers(&stack_visitor());
  }
  if (config_.collection_type == MarkingConfig::CollectionType::kMinor) {
    StatsCollector::EnabledScope stats_scope(
        heap().stats_collector(), StatsCollector::kMarkVisitRememberedSets);
    VisitRememberedSlots(heap(), mutator_marking_state_);
    VisitRememberedObjects(heap(), visitor(), mutator_marking_state_);
  }
}

bool MarkerBase::VisitCrossThreadPersistentsIfNeeded() {
  if (config_.marking_type != MarkingConfig::MarkingType::kAtomic ||
      visited_cross_thread_persistents_in_atomic_pause_)
    return false;

  StatsCollector::DisabledScope inner_stats_scope(
      heap().stats_collector(),
      StatsCollector::kMarkVisitCrossThreadPersistents);
  // Lock guards against changes to {Weak}CrossThreadPersistent handles, that
  // may conflict with marking. E.g., a WeakCrossThreadPersistent may be
  // converted into a CrossThreadPersistent which requires that the handle
  // is either cleared or the object is retained.
  g_process_mutex.Pointer()->Lock();
  heap().GetStrongCrossThreadPersistentRegion().Trace(&visitor());
  visited_cross_thread_persistents_in_atomic_pause_ = true;
  return (heap().GetStrongCrossThreadPersistentRegion().NodesInUse() > 0);
}

void MarkerBase::ScheduleIncrementalMarkingTask() {
  DCHECK(platform_);
  if (!foreground_task_runner_ || incremental_marking_handle_) return;
  incremental_marking_handle_ =
      IncrementalMarkingTask::Post(foreground_task_runner_.get(), this);
}

bool MarkerBase::IncrementalMarkingStepForTesting(
    MarkingConfig::StackState stack_state) {
  return IncrementalMarkingStep(stack_state);
}

bool MarkerBase::IncrementalMarkingStep(MarkingConfig::StackState stack_state) {
  if (stack_state == MarkingConfig::StackState::kNoHeapPointers) {
    mutator_marking_state_.FlushNotFullyConstructedObjects();
  }
  config_.stack_state = stack_state;

  return AdvanceMarkingWithLimits();
}

void MarkerBase::AdvanceMarkingOnAllocation() {
  StatsCollector::EnabledScope stats_scope(heap().stats_collector(),
                                           StatsCollector::kIncrementalMark);
  StatsCollector::EnabledScope nested_scope(heap().stats_collector(),
                                            StatsCollector::kMarkOnAllocation);
  if (AdvanceMarkingWithLimits()) {
    // Schedule another incremental task for finalizing without a stack.
    ScheduleIncrementalMarkingTask();
  }
}

bool MarkerBase::CancelConcurrentMarkingIfNeeded() {
  if (config_.marking_type != MarkingConfig::MarkingType::kAtomic ||
      !concurrent_marking_active_)
    return false;

  concurrent_marker_->Cancel();
  concurrent_marking_active_ = false;
  // Concurrent markers may have pushed some "leftover" in-construction objects
  // after flushing in EnterAtomicPause.
  HandleNotFullyConstructedObjects();
  DCHECK(marking_worklists_.not_fully_constructed_worklist()->IsEmpty());
  return true;
}

bool MarkerBase::AdvanceMarkingWithLimits(v8::base::TimeDelta max_duration,
                                          size_t marked_bytes_limit) {
  bool is_done = false;
  if (!main_marking_disabled_for_testing_) {
    if (marked_bytes_limit == 0) {
      marked_bytes_limit = mutator_marking_state_.marked_bytes() +
                           GetNextIncrementalStepDuration(schedule_, heap_);
    }
    StatsCollector::EnabledScope deadline_scope(
        heap().stats_collector(),
        StatsCollector::kMarkTransitiveClosureWithDeadline, "deadline_ms",
        max_duration.InMillisecondsF());
    const auto deadline = v8::base::TimeTicks::Now() + max_duration;
    is_done = ProcessWorklistsWithDeadline(marked_bytes_limit, deadline);
    if (is_done && VisitCrossThreadPersistentsIfNeeded()) {
      // Both limits are absolute and hence can be passed along without further
      // adjustment.
      is_done = ProcessWorklistsWithDeadline(marked_bytes_limit, deadline);
    }
    if (is_done && CancelConcurrentMarkingIfNeeded()) {
      is_done = ProcessWorklistsWithDeadline(marked_bytes_limit, deadline);
    }
    schedule_.UpdateMutatorThreadMarkedBytes(
        mutator_marking_state_.marked_bytes());
  }
  mutator_marking_state_.Publish();
  if (!is_done) {
    // If marking is atomic, |is_done| should always be true.
    DCHECK_NE(MarkingConfig::MarkingType::kAtomic, config_.marking_type);
    ScheduleIncrementalMarkingTask();
    if (config_.marking_type ==
        MarkingConfig::MarkingType::kIncrementalAndConcurrent) {
      concurrent_marker_->NotifyIncrementalMutatorStepCompleted();
    }
  }
  return is_done;
}

bool MarkerBase::ProcessWorklistsWithDeadline(
    size_t marked_bytes_deadline, v8::base::TimeTicks time_deadline) {
  StatsCollector::EnabledScope stats_scope(
      heap().stats_collector(), StatsCollector::kMarkTransitiveClosure);
  bool saved_did_discover_new_ephemeron_pairs;
  do {
    mutator_marking_state_.ResetDidDiscoverNewEphemeronPairs();
    if ((config_.marking_type == MarkingConfig::MarkingType::kAtomic) ||
        schedule_.ShouldFlushEphemeronPairs()) {
      mutator_marking_state_.FlushDiscoveredEphemeronPairs();
    }

    // Bailout objects may be complicated to trace and thus might take longer
    // than other objects. Therefore we reduce the interval between deadline
    // checks to guarantee the deadline is not exceeded.
    {
      StatsCollector::EnabledScope inner_scope(
          heap().stats_collector(), StatsCollector::kMarkProcessBailOutObjects);
      if (!DrainWorklistWithBytesAndTimeDeadline<kDefaultDeadlineCheckInterval /
                                                 5>(
              mutator_marking_state_, marked_bytes_deadline, time_deadline,
              mutator_marking_state_.concurrent_marking_bailout_worklist(),
              [this](
                  const MarkingWorklists::ConcurrentMarkingBailoutItem& item) {
                mutator_marking_state_.AccountMarkedBytes(item.bailedout_size);
                item.callback(&visitor(), item.parameter);
              })) {
        return false;
      }
    }

    {
      StatsCollector::EnabledScope inner_scope(
          heap().stats_collector(),
          StatsCollector::kMarkProcessNotFullyconstructedWorklist);
      if (!DrainWorklistWithBytesAndTimeDeadline(
              mutator_marking_state_, marked_bytes_deadline, time_deadline,
              mutator_marking_state_
                  .previously_not_fully_constructed_worklist(),
              [this](HeapObjectHeader* header) {
                mutator_marking_state_.AccountMarkedBytes(*header);
                DynamicallyTraceMarkedObject<AccessMode::kNonAtomic>(visitor(),
                                                                     *header);
              })) {
        return false;
      }
    }

    {
      StatsCollector::EnabledScope inner_scope(
          heap().stats_collector(),
          StatsCollector::kMarkProcessMarkingWorklist);
      if (!DrainWorklistWithBytesAndTimeDeadline(
              mutator_marking_state_, marked_bytes_deadline, time_deadline,
              mutator_marking_state_.marking_worklist(),
              [this](const MarkingWorklists::MarkingItem& item) {
                const HeapObjectHeader& header =
                    HeapObjectHeader::FromObject(item.base_object_payload);
                DCHECK(!header.IsInConstruction<AccessMode::kNonAtomic>());
                DCHECK(header.IsMarked<AccessMode::kNonAtomic>());
                mutator_marking_state_.AccountMarkedBytes(header);
                item.callback(&visitor(), item.base_object_payload);
              })) {
        return false;
      }
    }

    {
      StatsCollector::EnabledScope inner_scope(
          heap().stats_collector(),
          StatsCollector::kMarkProcessWriteBarrierWorklist);
      if (!DrainWorklistWithBytesAndTimeDeadline(
              mutator_marking_state_, marked_bytes_deadline, time_deadline,
              mutator_marking_state_.write_barrier_worklist(),
              [this](HeapObjectHeader* header) {
                mutator_marking_state_.AccountMarkedBytes(*header);
                DynamicallyTraceMarkedObject<AccessMode::kNonAtomic>(visitor(),
                                                                     *header);
              })) {
        return false;
      }
      if (!DrainWorklistWithBytesAndTimeDeadline(
              mutator_marking_state_, marked_bytes_deadline, time_deadline,
              mutator_marking_state_.retrace_marked_objects_worklist(),
              [this](HeapObjectHeader* header) {
                // Retracing does not increment marked bytes as the object has
                // already been processed before.
                DynamicallyTraceMarkedObject<AccessMode::kNonAtomic>(visitor(),
                                                                     *header);
              })) {
        return false;
      }
    }

    saved_did_discover_new_ephemeron_pairs =
        mutator_marking_state_.DidDiscoverNewEphemeronPairs();
    {
      StatsCollector::EnabledScope inner_stats_scope(
          heap().stats_collector(), StatsCollector::kMarkProcessEphemerons);
      if (!DrainWorklistWithBytesAndTimeDeadline(
              mutator_marking_state_, marked_bytes_deadline, time_deadline,
              mutator_marking_state_.ephemeron_pairs_for_processing_worklist(),
              [this](const MarkingWorklists::EphemeronPairItem& item) {
                mutator_marking_state_.ProcessEphemeron(
                    item.key, item.value, item.value_desc, visitor());
              })) {
        return false;
      }
    }
  } while (!mutator_marking_state_.marking_worklist().IsLocalAndGlobalEmpty() ||
           saved_did_discover_new_ephemeron_pairs);
  return true;
}

void MarkerBase::MarkNotFullyConstructedObjects() {
  StatsCollector::DisabledScope stats_scope(
      heap().stats_collector(),
      StatsCollector::kMarkVisitNotFullyConstructedObjects);
  std::unordered_set<HeapObjectHeader*> objects =
      mutator_marking_state_.not_fully_constructed_worklist().Extract();
  for (HeapObjectHeader* object : objects) {
    DCHECK(object);
    // TraceConservativelyIfNeeded delegates to either in-construction or
    // fully constructed handling. Both handlers have their own marked bytes
    // accounting and markbit handling (bailout).
    conservative_visitor().TraceConservativelyIfNeeded(*object);
  }
}

void MarkerBase::ClearAllWorklistsForTesting() {
  marking_worklists_.ClearForTesting();
  auto* compaction_worklists = heap_.compactor().compaction_worklists();
  if (compaction_worklists) compaction_worklists->ClearForTesting();
}

void MarkerBase::SetMainThreadMarkingDisabledForTesting(bool value) {
  main_marking_disabled_for_testing_ = value;
}

void MarkerBase::WaitForConcurrentMarkingForTesting() {
  concurrent_marker_->JoinForTesting();
}

Marker::Marker(HeapBase& heap, cppgc::Platform* platform, MarkingConfig config)
    : MarkerBase(heap, platform, config),
      marking_visitor_(heap, mutator_marking_state_),
      conservative_marking_visitor_(heap, mutator_marking_state_,
                                    marking_visitor_) {
  concurrent_marker_ = std::make_unique<ConcurrentMarker>(
      heap_, marking_worklists_, schedule_, platform_);
}

}  // namespace internal
}  // namespace cppgc
