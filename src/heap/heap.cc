// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/heap.h"

#include <atomic>
#include <cinttypes>
#include <iomanip>
#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "include/v8-locker.h"
#include "src/api/api-inl.h"
#include "src/base/bits.h"
#include "src/base/flags.h"
#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/base/once.h"
#include "src/base/optional.h"
#include "src/base/platform/memory.h"
#include "src/base/platform/mutex.h"
#include "src/base/utils/random-number-generator.h"
#include "src/builtins/accessors.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/compilation-cache.h"
#include "src/common/assert-scope.h"
#include "src/common/globals.h"
#include "src/compiler-dispatcher/optimizing-compile-dispatcher.h"
#include "src/debug/debug.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/execution/embedder-state.h"
#include "src/execution/isolate-utils-inl.h"
#include "src/execution/microtask-queue.h"
#include "src/execution/v8threads.h"
#include "src/execution/vm-state-inl.h"
#include "src/flags/flags.h"
#include "src/handles/global-handles-inl.h"
#include "src/handles/traced-handles.h"
#include "src/heap/allocation-observer.h"
#include "src/heap/array-buffer-sweeper.h"
#include "src/heap/base/stack.h"
#include "src/heap/base/worklist.h"
#include "src/heap/basic-memory-chunk.h"
#include "src/heap/code-range.h"
#include "src/heap/code-stats.h"
#include "src/heap/collection-barrier.h"
#include "src/heap/combined-heap.h"
#include "src/heap/concurrent-allocator.h"
#include "src/heap/concurrent-marking.h"
#include "src/heap/cppgc-js/cpp-heap.h"
#include "src/heap/ephemeron-remembered-set.h"
#include "src/heap/evacuation-verifier-inl.h"
#include "src/heap/finalization-registry-cleanup-task.h"
#include "src/heap/gc-idle-time-handler.h"
#include "src/heap/gc-tracer-inl.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/heap-allocator.h"
#include "src/heap/heap-controller.h"
#include "src/heap/heap-layout-tracer.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/heap/incremental-marking-inl.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/large-spaces.h"
#include "src/heap/local-heap.h"
#include "src/heap/mark-compact-inl.h"
#include "src/heap/mark-compact.h"
#include "src/heap/marking-barrier-inl.h"
#include "src/heap/marking-barrier.h"
#include "src/heap/marking-state-inl.h"
#include "src/heap/marking-state.h"
#include "src/heap/memory-balancer.h"
#include "src/heap/memory-chunk-inl.h"
#include "src/heap/memory-chunk-layout.h"
#include "src/heap/memory-measurement.h"
#include "src/heap/memory-reducer.h"
#include "src/heap/minor-gc-job.h"
#include "src/heap/minor-mark-sweep.h"
#include "src/heap/new-spaces.h"
#include "src/heap/object-lock.h"
#include "src/heap/object-stats.h"
#include "src/heap/objects-visiting-inl.h"
#include "src/heap/objects-visiting.h"
#include "src/heap/paged-spaces-inl.h"
#include "src/heap/parked-scope.h"
#include "src/heap/pretenuring-handler.h"
#include "src/heap/read-only-heap.h"
#include "src/heap/remembered-set.h"
#include "src/heap/safepoint.h"
#include "src/heap/scavenger-inl.h"
#include "src/heap/stress-scavenge-observer.h"
#include "src/heap/sweeper.h"
#include "src/heap/zapping.h"
#include "src/init/bootstrapper.h"
#include "src/init/v8.h"
#include "src/interpreter/interpreter.h"
#include "src/logging/log.h"
#include "src/logging/runtime-call-stats-scope.h"
#include "src/numbers/conversions.h"
#include "src/objects/data-handler.h"
#include "src/objects/feedback-vector.h"
#include "src/objects/free-space-inl.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/hash-table.h"
#include "src/objects/instance-type.h"
#include "src/objects/maybe-object.h"
#include "src/objects/objects.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/slots-atomic-inl.h"
#include "src/objects/slots-inl.h"
#include "src/objects/visitors.h"
#include "src/regexp/regexp.h"
#include "src/snapshot/embedded/embedded-data.h"
#include "src/snapshot/serializer-deserializer.h"
#include "src/snapshot/snapshot.h"
#include "src/strings/string-stream.h"
#include "src/strings/unicode-decoder.h"
#include "src/strings/unicode-inl.h"
#include "src/tracing/trace-event.h"
#include "src/utils/utils-inl.h"
#include "src/utils/utils.h"

#ifdef V8_ENABLE_CONSERVATIVE_STACK_SCANNING
#include "src/heap/conservative-stack-visitor.h"
#endif  // V8_ENABLE_CONSERVATIVE_STACK_SCANNING

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#ifdef V8_ENABLE_THIRD_PARTY_HEAP
Isolate* Heap::GetIsolateFromWritableObject(HeapObject object) {
  return reinterpret_cast<Isolate*>(
      third_party_heap::Heap::GetIsolate(object.address()));
}
#endif

// These are outside the Heap class so they can be forward-declared
// in heap-write-barrier-inl.h.
bool Heap_PageFlagsAreConsistent(HeapObject object) {
  return Heap::PageFlagsAreConsistent(object);
}

void Heap_CombinedGenerationalAndSharedBarrierSlow(HeapObject object,
                                                   Address slot,
                                                   HeapObject value) {
  Heap::CombinedGenerationalAndSharedBarrierSlow(object, slot, value);
}

void Heap_CombinedGenerationalAndSharedEphemeronBarrierSlow(
    EphemeronHashTable table, Address slot, HeapObject value) {
  Heap::CombinedGenerationalAndSharedEphemeronBarrierSlow(table, slot, value);
}

void Heap_GenerationalBarrierForCodeSlow(InstructionStream host,
                                         RelocInfo* rinfo, HeapObject object) {
  Heap::GenerationalBarrierForCodeSlow(host, rinfo, object);
}

void Heap::SetConstructStubCreateDeoptPCOffset(int pc_offset) {
  DCHECK_EQ(Smi::zero(), construct_stub_create_deopt_pc_offset());
  set_construct_stub_create_deopt_pc_offset(Smi::FromInt(pc_offset));
}

void Heap::SetConstructStubInvokeDeoptPCOffset(int pc_offset) {
  DCHECK_EQ(Smi::zero(), construct_stub_invoke_deopt_pc_offset());
  set_construct_stub_invoke_deopt_pc_offset(Smi::FromInt(pc_offset));
}

void Heap::SetInterpreterEntryReturnPCOffset(int pc_offset) {
  DCHECK_EQ(Smi::zero(), interpreter_entry_return_pc_offset());
  set_interpreter_entry_return_pc_offset(Smi::FromInt(pc_offset));
}

void Heap::SetSerializedObjects(FixedArray objects) {
  DCHECK(isolate()->serializer_enabled());
  set_serialized_objects(objects);
}

void Heap::SetSerializedGlobalProxySizes(FixedArray sizes) {
  DCHECK(isolate()->serializer_enabled());
  set_serialized_global_proxy_sizes(sizes);
}

void Heap::SetBasicBlockProfilingData(Handle<ArrayList> list) {
  set_basic_block_profiling_data(*list);
}

class ScheduleMinorGCTaskObserver : public AllocationObserver {
 public:
  explicit ScheduleMinorGCTaskObserver(Heap* heap)
      : AllocationObserver(kNotUsingFixedStepSize), heap_(heap) {
    // Register GC callback for all atomic pause types.
    heap_->main_thread_local_heap()->AddGCEpilogueCallback(
        &GCEpilogueCallback, this,
        static_cast<GCType>(GCType::kGCTypeScavenge |
                            GCType::kGCTypeMinorMarkSweep |
                            GCType::kGCTypeMarkSweepCompact));
    AddToNewSpace();
  }
  ~ScheduleMinorGCTaskObserver() override {
    RemoveFromNewSpace();
    heap_->main_thread_local_heap()->RemoveGCEpilogueCallback(
        &GCEpilogueCallback, this);
  }

  intptr_t GetNextStepSize() final {
    size_t new_space_threshold =
        MinorGCJob::YoungGenerationTaskTriggerSize(heap_);
    size_t new_space_size = heap_->new_space()->Size();
    if (new_space_size < new_space_threshold) {
      return new_space_threshold - new_space_size;
    }
    // Force a step on next allocation.
    return 1;
  }

  void Step(int, Address, size_t) final {
    StepImpl();
    // Remove this observer. It will be re-added after a GC.
    DCHECK(was_added_to_space_);
    heap_->new_space()->RemoveAllocationObserver(this);
    was_added_to_space_ = false;
  }

 protected:
  static void GCEpilogueCallback(LocalIsolate*, GCType, GCCallbackFlags,
                                 void* observer) {
    reinterpret_cast<ScheduleMinorGCTaskObserver*>(observer)
        ->RemoveFromNewSpace();
    reinterpret_cast<ScheduleMinorGCTaskObserver*>(observer)->AddToNewSpace();
  }

  void AddToNewSpace() {
    DCHECK(!was_added_to_space_);
    heap_->new_space()->AddAllocationObserver(this);
    was_added_to_space_ = true;
  }

  void RemoveFromNewSpace() {
    if (!was_added_to_space_) return;
    heap_->new_space()->RemoveAllocationObserver(this);
    was_added_to_space_ = false;
  }

  virtual void StepImpl() { heap_->ScheduleMinorGCTaskIfNeeded(); }
  Heap* heap_;
  bool was_added_to_space_ = false;
};

class MinorMSIncrementalMarkingTaskObserver final
    : public ScheduleMinorGCTaskObserver {
 public:
  explicit MinorMSIncrementalMarkingTaskObserver(Heap* heap)
      : ScheduleMinorGCTaskObserver(heap) {}

 protected:
  void StepImpl() final {
    if (v8_flags.concurrent_minor_ms_marking) {
      if (heap_->incremental_marking()->IsMinorMarking()) {
        heap_->concurrent_marking()->RescheduleJobIfNeeded(
            GarbageCollector::MINOR_MARK_SWEEPER);
      }
    }

    heap_->StartMinorMSIncrementalMarkingIfNeeded();

    ScheduleMinorGCTaskObserver::StepImpl();
  }
};

Heap::Heap()
    : isolate_(isolate()),
      heap_allocator_(this),
      memory_pressure_level_(MemoryPressureLevel::kNone),
      safepoint_(std::make_unique<IsolateSafepoint>(this)),
      external_string_table_(this),
      allocation_type_for_in_place_internalizable_strings_(
          isolate()->OwnsStringTables() ? AllocationType::kOld
                                        : AllocationType::kSharedOld),
      marking_state_(isolate_),
      non_atomic_marking_state_(isolate_),
      pretenuring_handler_(this) {
  // Ensure old_generation_size_ is a multiple of kPageSize.
  DCHECK_EQ(0, max_old_generation_size() & (Page::kPageSize - 1));

  max_regular_code_object_size_ = MemoryChunkLayout::MaxRegularCodeObjectSize();

  set_native_contexts_list(Smi::zero());
  set_allocation_sites_list(Smi::zero());
  set_dirty_js_finalization_registries_list(Smi::zero());
  set_dirty_js_finalization_registries_list_tail(Smi::zero());

  // Put a dummy entry in the remembered pages so we can find the list the
  // minidump even if there are no real unmapped pages.
  RememberUnmappedPage(kNullAddress, false);
}

Heap::~Heap() = default;

size_t Heap::MaxReserved() const {
  const size_t kMaxNewLargeObjectSpaceSize = max_semi_space_size_;
  return static_cast<size_t>(
      (v8_flags.minor_ms ? 1 : 2) * max_semi_space_size_ +
      kMaxNewLargeObjectSpaceSize + max_old_generation_size());
}

size_t Heap::YoungGenerationSizeFromOldGenerationSize(size_t old_generation) {
  // Compute the semi space size and cap it.
  bool is_low_memory = old_generation <= kOldGenerationLowMemory;
  size_t semi_space;
  if (v8_flags.minor_ms && !is_low_memory) {
    semi_space = DefaultMaxSemiSpaceSize();
  } else {
    size_t ratio = is_low_memory ? OldGenerationToSemiSpaceRatioLowMemory()
                                 : OldGenerationToSemiSpaceRatio();
    semi_space = old_generation / ratio;
    semi_space = std::min({semi_space, DefaultMaxSemiSpaceSize()});
    semi_space = std::max({semi_space, DefaultMinSemiSpaceSize()});
    semi_space = RoundUp(semi_space, Page::kPageSize);
  }
  return YoungGenerationSizeFromSemiSpaceSize(semi_space);
}

size_t Heap::HeapSizeFromPhysicalMemory(uint64_t physical_memory) {
  // Compute the old generation size and cap it.
  uint64_t old_generation = physical_memory /
                            kPhysicalMemoryToOldGenerationRatio *
                            kHeapLimitMultiplier;
  old_generation =
      std::min(old_generation,
               static_cast<uint64_t>(MaxOldGenerationSize(physical_memory)));
  old_generation =
      std::max({old_generation, static_cast<uint64_t>(V8HeapTrait::kMinSize)});
  old_generation = RoundUp(old_generation, Page::kPageSize);

  size_t young_generation = YoungGenerationSizeFromOldGenerationSize(
      static_cast<size_t>(old_generation));
  return static_cast<size_t>(old_generation) + young_generation;
}

void Heap::GenerationSizesFromHeapSize(size_t heap_size,
                                       size_t* young_generation_size,
                                       size_t* old_generation_size) {
  // Initialize values for the case when the given heap size is too small.
  *young_generation_size = 0;
  *old_generation_size = 0;
  // Binary search for the largest old generation size that fits to the given
  // heap limit considering the correspondingly sized young generation.
  size_t lower = 0, upper = heap_size;
  while (lower + 1 < upper) {
    size_t old_generation = lower + (upper - lower) / 2;
    size_t young_generation =
        YoungGenerationSizeFromOldGenerationSize(old_generation);
    if (old_generation + young_generation <= heap_size) {
      // This size configuration fits into the given heap limit.
      *young_generation_size = young_generation;
      *old_generation_size = old_generation;
      lower = old_generation;
    } else {
      upper = old_generation;
    }
  }
}

size_t Heap::MinYoungGenerationSize() {
  return YoungGenerationSizeFromSemiSpaceSize(DefaultMinSemiSpaceSize());
}

size_t Heap::MinOldGenerationSize() {
  size_t paged_space_count =
      LAST_GROWABLE_PAGED_SPACE - FIRST_GROWABLE_PAGED_SPACE + 1;
  return paged_space_count * Page::kPageSize;
}

size_t Heap::AllocatorLimitOnMaxOldGenerationSize() {
#ifdef V8_COMPRESS_POINTERS
  // Isolate and the young generation are also allocated on the heap.
  return kPtrComprCageReservationSize -
         YoungGenerationSizeFromSemiSpaceSize(DefaultMaxSemiSpaceSize()) -
         RoundUp(sizeof(Isolate), size_t{1} << kPageSizeBits);
#else
  return std::numeric_limits<size_t>::max();
#endif
}

size_t Heap::MaxOldGenerationSize(uint64_t physical_memory) {
  size_t max_size = V8HeapTrait::kMaxSize;
  // Increase the heap size from 2GB to 4GB for 64-bit systems with physical
  // memory at least 16GB. The theshold is set to 15GB to accomodate for some
  // memory being reserved by the hardware.
  constexpr bool x64_bit = Heap::kHeapLimitMultiplier >= 2;
  if (v8_flags.huge_max_old_generation_size && x64_bit &&
      (physical_memory / GB) >= 15) {
    DCHECK_EQ(max_size / GB, 2u);
    max_size *= 2;
  }
  return std::min(max_size, AllocatorLimitOnMaxOldGenerationSize());
}

namespace {
int NumberOfSemiSpaces() { return v8_flags.minor_ms ? 1 : 2; }
}  // namespace

size_t Heap::YoungGenerationSizeFromSemiSpaceSize(size_t semi_space_size) {
  return semi_space_size *
         (NumberOfSemiSpaces() + kNewLargeObjectSpaceToSemiSpaceRatio);
}

size_t Heap::SemiSpaceSizeFromYoungGenerationSize(
    size_t young_generation_size) {
  return young_generation_size /
         (NumberOfSemiSpaces() + kNewLargeObjectSpaceToSemiSpaceRatio);
}

size_t Heap::Capacity() {
  if (!HasBeenSetUp()) return 0;

  if (v8_flags.enable_third_party_heap) return tp_heap_->Capacity();

  return NewSpaceCapacity() + OldGenerationCapacity();
}

size_t Heap::OldGenerationCapacity() const {
  if (!HasBeenSetUp()) return 0;
  PagedSpaceIterator spaces(this);
  size_t total = 0;
  for (PagedSpace* space = spaces.Next(); space != nullptr;
       space = spaces.Next()) {
    total += space->Capacity();
  }
  if (shared_lo_space_) {
    total += shared_lo_space_->SizeOfObjects();
  }
  return total + lo_space_->SizeOfObjects() + code_lo_space_->SizeOfObjects();
}

size_t Heap::CommittedOldGenerationMemory() {
  if (!HasBeenSetUp()) return 0;

  PagedSpaceIterator spaces(this);
  size_t total = 0;
  for (PagedSpace* space = spaces.Next(); space != nullptr;
       space = spaces.Next()) {
    total += space->CommittedMemory();
  }
  if (shared_lo_space_) {
    total += shared_lo_space_->Size();
  }
  return total + lo_space_->Size() + code_lo_space_->Size();
}

size_t Heap::CommittedMemoryOfUnmapper() {
  if (!HasBeenSetUp()) return 0;

  return memory_allocator()->unmapper()->CommittedBufferedMemory();
}

size_t Heap::CommittedMemory() {
  if (!HasBeenSetUp()) return 0;

  size_t new_space_committed = new_space_ ? new_space_->CommittedMemory() : 0;
  size_t new_lo_space_committed = new_lo_space_ ? new_lo_space_->Size() : 0;

  return new_space_committed + new_lo_space_committed +
         CommittedOldGenerationMemory();
}

size_t Heap::CommittedPhysicalMemory() {
  if (!HasBeenSetUp()) return 0;

  size_t total = 0;
  for (SpaceIterator it(this); it.HasNext();) {
    total += it.Next()->CommittedPhysicalMemory();
  }

  return total;
}

size_t Heap::CommittedMemoryExecutable() {
  if (!HasBeenSetUp()) return 0;

  return static_cast<size_t>(memory_allocator()->SizeExecutable());
}

void Heap::UpdateMaximumCommitted() {
  if (!HasBeenSetUp()) return;

  const size_t current_committed_memory = CommittedMemory();
  if (current_committed_memory > maximum_committed_) {
    maximum_committed_ = current_committed_memory;
  }
}

size_t Heap::Available() {
  if (!HasBeenSetUp()) return 0;

  size_t total = 0;

  for (SpaceIterator it(this); it.HasNext();) {
    total += it.Next()->Available();
  }

  total += memory_allocator()->Available();
  return total;
}

bool Heap::CanExpandOldGeneration(size_t size) const {
  if (force_oom_ || force_gc_on_next_allocation_) return false;
  if (OldGenerationCapacity() + size > max_old_generation_size()) return false;
  // The OldGenerationCapacity does not account compaction spaces used
  // during evacuation. Ensure that expanding the old generation does push
  // the total allocated memory size over the maximum heap size.
  return memory_allocator()->Size() + size <= MaxReserved();
}

namespace {
bool IsIsolateDeserializationActive(LocalHeap* local_heap) {
  return local_heap && !local_heap->heap()->deserialization_complete();
}
}  // anonymous namespace

bool Heap::CanExpandOldGenerationBackground(LocalHeap* local_heap,
                                            size_t size) {
  if (force_oom_) return false;

  // When the heap is tearing down, then GC requests from background threads
  // are not served and the threads are allowed to expand the heap to avoid OOM.
  return gc_state() == TEAR_DOWN || IsMainThreadParked(local_heap) ||
         IsIsolateDeserializationActive(local_heap) ||
         memory_allocator()->Size() + size <= MaxReserved();
}

bool Heap::CanPromoteYoungAndExpandOldGeneration(size_t size) const {
  size_t new_space_capacity = NewSpaceTargetCapacity();
  size_t new_lo_space_capacity = new_lo_space_ ? new_lo_space_->Size() : 0;

  // Over-estimate the new space size using capacity to allow some slack.
  return CanExpandOldGeneration(size + new_space_capacity +
                                new_lo_space_capacity);
}

bool Heap::HasBeenSetUp() const {
  // We will always have an old space when the heap is set up.
  return old_space_ != nullptr;
}

GarbageCollector Heap::SelectGarbageCollector(AllocationSpace space,
                                              GarbageCollectionReason gc_reason,
                                              const char** reason) const {
  if (gc_reason == GarbageCollectionReason::kFinalizeMinorMS) {
    DCHECK(new_space());
    *reason = "finalize MinorMS";
    return GarbageCollector::MINOR_MARK_SWEEPER;
  }

  // Is global GC requested?
  if (space != NEW_SPACE && space != NEW_LO_SPACE) {
    isolate_->counters()->gc_compactor_caused_by_request()->Increment();
    *reason = "GC in old space requested";
    return GarbageCollector::MARK_COMPACTOR;
  }

  if (v8_flags.gc_global || ShouldStressCompaction() || !new_space()) {
    *reason = "GC in old space forced by flags";
    return GarbageCollector::MARK_COMPACTOR;
  }

  if (incremental_marking()->IsMajorMarking() &&
      incremental_marking()->IsMajorMarkingComplete() &&
      AllocationLimitOvershotByLargeMargin()) {
    *reason = "Incremental marking needs finalization";
    return GarbageCollector::MARK_COMPACTOR;
  }

  if (v8_flags.separate_gc_phases && incremental_marking()->IsMajorMarking()) {
    // TODO(v8:12503): Remove previous condition when flag gets removed.
    *reason = "Incremental marking forced finalization";
    return GarbageCollector::MARK_COMPACTOR;
  }

  if (!CanPromoteYoungAndExpandOldGeneration(0)) {
    isolate_->counters()
        ->gc_compactor_caused_by_oldspace_exhaustion()
        ->Increment();
    *reason = "scavenge might not succeed";
    return GarbageCollector::MARK_COMPACTOR;
  }

  DCHECK(!v8_flags.single_generation);
  DCHECK(!v8_flags.gc_global);
  // Default
  *reason = nullptr;
  return YoungGenerationCollector();
}

void Heap::SetGCState(HeapState state) {
  gc_state_.store(state, std::memory_order_relaxed);
}

bool Heap::IsGCWithStack() const {
  return embedder_stack_state_ ==
         cppgc::EmbedderStackState::kMayContainHeapPointers;
}

bool Heap::CanShortcutStringsDuringGC(GarbageCollector collector) const {
  if (!v8_flags.shortcut_strings_with_stack && IsGCWithStack()) return false;

  switch (collector) {
    case GarbageCollector::MINOR_MARK_SWEEPER:
      if (!v8_flags.minor_ms_shortcut_strings) return false;

      DCHECK(!incremental_marking()->IsMajorMarking());

      // Minor MC cannot short cut strings during concurrent marking.
      if (incremental_marking()->IsMinorMarking()) return false;

      // Minor MC uses static roots to check for strings to shortcut.
      if (!V8_STATIC_ROOTS_BOOL) return false;

      break;
    case GarbageCollector::SCAVENGER:
      // Scavenger cannot short cut strings during incremental marking.
      if (incremental_marking()->IsMajorMarking()) return false;

      if (isolate()->has_shared_space() &&
          !isolate()->is_shared_space_isolate() &&
          isolate()
              ->shared_space_isolate()
              ->heap()
              ->incremental_marking()
              ->IsMarking())
        return false;

      break;
    default:
      UNREACHABLE();
  }

  return true;
}

void Heap::PrintShortHeapStatistics() {
  if (!v8_flags.trace_gc_verbose) return;
  PrintIsolate(isolate_,
               "Memory allocator,       used: %6zu KB,"
               " available: %6zu KB\n",
               memory_allocator()->Size() / KB,
               memory_allocator()->Available() / KB);
  PrintIsolate(isolate_,
               "Read-only space,        used: %6zu KB"
               ", available: %6zu KB"
               ", committed: %6zu KB\n",
               read_only_space_->Size() / KB, size_t{0},
               read_only_space_->CommittedMemory() / KB);
  PrintIsolate(isolate_,
               "New space,              used: %6zu KB"
               ", available: %6zu KB%s"
               ", committed: %6zu KB\n",
               NewSpaceSize() / KB, new_space_->Available() / KB,
               (v8_flags.minor_ms && minor_sweeping_in_progress()) ? "*" : "",
               new_space_->CommittedMemory() / KB);
  PrintIsolate(isolate_,
               "New large object space, used: %6zu KB"
               ", available: %6zu KB"
               ", committed: %6zu KB\n",
               new_lo_space_->SizeOfObjects() / KB,
               new_lo_space_->Available() / KB,
               new_lo_space_->CommittedMemory() / KB);
  PrintIsolate(isolate_,
               "Old space,              used: %6zu KB"
               ", available: %6zu KB%s"
               ", committed: %6zu KB\n",
               old_space_->SizeOfObjects() / KB, old_space_->Available() / KB,
               sweeping_in_progress() ? "*" : "",
               old_space_->CommittedMemory() / KB);
  PrintIsolate(isolate_,
               "Code space,             used: %6zu KB"
               ", available: %6zu KB%s"
               ", committed: %6zu KB\n",
               code_space_->SizeOfObjects() / KB, code_space_->Available() / KB,
               major_sweeping_in_progress() ? "*" : "",
               code_space_->CommittedMemory() / KB);
  PrintIsolate(isolate_,
               "Large object space,     used: %6zu KB"
               ", available: %6zu KB"
               ", committed: %6zu KB\n",
               lo_space_->SizeOfObjects() / KB, lo_space_->Available() / KB,
               lo_space_->CommittedMemory() / KB);
  PrintIsolate(isolate_,
               "Code large object space,     used: %6zu KB"
               ", available: %6zu KB"
               ", committed: %6zu KB\n",
               code_lo_space_->SizeOfObjects() / KB,
               code_lo_space_->Available() / KB,
               code_lo_space_->CommittedMemory() / KB);
  ReadOnlySpace* const ro_space = read_only_space_;
  PrintIsolate(isolate_,
               "All spaces,             used: %6zu KB"
               ", available: %6zu KB%s"
               ", committed: %6zu KB\n",
               (this->SizeOfObjects() + ro_space->Size()) / KB,
               (this->Available()) / KB, sweeping_in_progress() ? "*" : "",
               (this->CommittedMemory() + ro_space->CommittedMemory()) / KB);
  PrintIsolate(isolate_,
               "Unmapper buffering %zu chunks of committed: %6zu KB\n",
               memory_allocator()->unmapper()->NumberOfCommittedChunks(),
               CommittedMemoryOfUnmapper() / KB);
  PrintIsolate(isolate_, "External memory reported: %6" PRId64 " KB\n",
               external_memory_.total() / KB);
  PrintIsolate(isolate_, "Backing store memory: %6" PRIu64 " KB\n",
               backing_store_bytes() / KB);
  PrintIsolate(isolate_, "External memory global %zu KB\n",
               external_memory_callback_() / KB);
  PrintIsolate(isolate_, "Total time spent in GC  : %.1f ms\n",
               total_gc_time_ms_);
  if (sweeping_in_progress()) {
    PrintIsolate(isolate_,
                 "(*) Sweeping is still in progress, making available sizes "
                 "inaccurate.\n");
  }
}

void Heap::PrintFreeListsStats() {
  DCHECK(v8_flags.trace_gc_freelists);

  if (v8_flags.trace_gc_freelists_verbose) {
    PrintIsolate(isolate_,
                 "Freelists statistics per Page: "
                 "[category: length || total free bytes]\n");
  }

  std::vector<int> categories_lengths(
      old_space()->free_list()->number_of_categories(), 0);
  std::vector<size_t> categories_sums(
      old_space()->free_list()->number_of_categories(), 0);
  unsigned int pageCnt = 0;

  // This loops computes freelists lengths and sum.
  // If v8_flags.trace_gc_freelists_verbose is enabled, it also prints
  // the stats of each FreeListCategory of each Page.
  for (Page* page : *old_space()) {
    std::ostringstream out_str;

    if (v8_flags.trace_gc_freelists_verbose) {
      out_str << "Page " << std::setw(4) << pageCnt;
    }

    for (int cat = kFirstCategory;
         cat <= old_space()->free_list()->last_category(); cat++) {
      FreeListCategory* free_list =
          page->free_list_category(static_cast<FreeListCategoryType>(cat));
      int length = free_list->FreeListLength();
      size_t sum = free_list->SumFreeList();

      if (v8_flags.trace_gc_freelists_verbose) {
        out_str << "[" << cat << ": " << std::setw(4) << length << " || "
                << std::setw(6) << sum << " ]"
                << (cat == old_space()->free_list()->last_category() ? "\n"
                                                                     : ", ");
      }
      categories_lengths[cat] += length;
      categories_sums[cat] += sum;
    }

    if (v8_flags.trace_gc_freelists_verbose) {
      PrintIsolate(isolate_, "%s", out_str.str().c_str());
    }

    pageCnt++;
  }

  // Print statistics about old_space (pages, free/wasted/used memory...).
  PrintIsolate(
      isolate_,
      "%d pages. Free space: %.1f MB (waste: %.2f). "
      "Usage: %.1f/%.1f (MB) -> %.2f%%.\n",
      pageCnt, static_cast<double>(old_space_->Available()) / MB,
      static_cast<double>(old_space_->Waste()) / MB,
      static_cast<double>(old_space_->Size()) / MB,
      static_cast<double>(old_space_->Capacity()) / MB,
      static_cast<double>(old_space_->Size()) / old_space_->Capacity() * 100);

  // Print global statistics of each FreeListCategory (length & sum).
  PrintIsolate(isolate_,
               "FreeLists global statistics: "
               "[category: length || total free KB]\n");
  std::ostringstream out_str;
  for (int cat = kFirstCategory;
       cat <= old_space()->free_list()->last_category(); cat++) {
    out_str << "[" << cat << ": " << categories_lengths[cat] << " || "
            << std::fixed << std::setprecision(2)
            << static_cast<double>(categories_sums[cat]) / KB << " KB]"
            << (cat == old_space()->free_list()->last_category() ? "\n" : ", ");
  }
  PrintIsolate(isolate_, "%s", out_str.str().c_str());
}

void Heap::DumpJSONHeapStatistics(std::stringstream& stream) {
  HeapStatistics stats;
  reinterpret_cast<v8::Isolate*>(isolate())->GetHeapStatistics(&stats);

// clang-format off
#define DICT(s) "{" << s << "}"
#define LIST(s) "[" << s << "]"
#define QUOTE(s) "\"" << s << "\""
#define MEMBER(s) QUOTE(s) << ":"

  auto SpaceStatistics = [this](int space_index) {
    HeapSpaceStatistics space_stats;
    reinterpret_cast<v8::Isolate*>(isolate())->GetHeapSpaceStatistics(
        &space_stats, space_index);
    std::stringstream stream;
    stream << DICT(
      MEMBER("name")
        << QUOTE(ToString(
              static_cast<AllocationSpace>(space_index)))
        << ","
      MEMBER("size") << space_stats.space_size() << ","
      MEMBER("used_size") << space_stats.space_used_size() << ","
      MEMBER("available_size") << space_stats.space_available_size() << ","
      MEMBER("physical_size") << space_stats.physical_space_size());
    return stream.str();
  };

  stream << DICT(
    MEMBER("isolate") << QUOTE(reinterpret_cast<void*>(isolate())) << ","
    MEMBER("id") << gc_count() << ","
    MEMBER("time_ms") << isolate()->time_millis_since_init() << ","
    MEMBER("total_heap_size") << stats.total_heap_size() << ","
    MEMBER("total_heap_size_executable")
      << stats.total_heap_size_executable() << ","
    MEMBER("total_physical_size") << stats.total_physical_size() << ","
    MEMBER("total_available_size") << stats.total_available_size() << ","
    MEMBER("used_heap_size") << stats.used_heap_size() << ","
    MEMBER("heap_size_limit") << stats.heap_size_limit() << ","
    MEMBER("malloced_memory") << stats.malloced_memory() << ","
    MEMBER("external_memory") << stats.external_memory() << ","
    MEMBER("peak_malloced_memory") << stats.peak_malloced_memory() << ","
    MEMBER("spaces") << LIST(
      SpaceStatistics(RO_SPACE)      << "," <<
      SpaceStatistics(NEW_SPACE)     << "," <<
      SpaceStatistics(OLD_SPACE)     << "," <<
      SpaceStatistics(CODE_SPACE)    << "," <<
      SpaceStatistics(LO_SPACE)      << "," <<
      SpaceStatistics(CODE_LO_SPACE) << "," <<
      SpaceStatistics(NEW_LO_SPACE)));

#undef DICT
#undef LIST
#undef QUOTE
#undef MEMBER
  // clang-format on
}

void Heap::ReportStatisticsAfterGC() {
  for (int i = 0; i < static_cast<int>(v8::Isolate::kUseCounterFeatureCount);
       ++i) {
    isolate()->CountUsage(static_cast<v8::Isolate::UseCounterFeature>(i),
                          deferred_counters_[i]);
    deferred_counters_[i] = 0;
  }
}

class Heap::AllocationTrackerForDebugging final
    : public HeapObjectAllocationTracker {
 public:
  static bool IsNeeded() {
    return v8_flags.verify_predictable || v8_flags.fuzzer_gc_analysis ||
           (v8_flags.trace_allocation_stack_interval > 0);
  }

  explicit AllocationTrackerForDebugging(Heap* heap) : heap_(heap) {
    CHECK(IsNeeded());
    heap_->AddHeapObjectAllocationTracker(this);
  }

  ~AllocationTrackerForDebugging() final {
    heap_->RemoveHeapObjectAllocationTracker(this);
    if (v8_flags.verify_predictable || v8_flags.fuzzer_gc_analysis) {
      PrintAllocationsHash();
    }
  }

  void AllocationEvent(Address addr, int size) final {
    if (v8_flags.verify_predictable) {
      allocations_count_.fetch_add(1, std::memory_order_relaxed);
      // Advance synthetic time by making a time request.
      heap_->MonotonicallyIncreasingTimeInMs();

      UpdateAllocationsHash(HeapObject::FromAddress(addr));
      UpdateAllocationsHash(size);

      if (allocations_count_ % v8_flags.dump_allocations_digest_at_alloc == 0) {
        PrintAllocationsHash();
      }
    } else if (v8_flags.fuzzer_gc_analysis) {
      allocations_count_.fetch_add(1, std::memory_order_relaxed);
    } else if (v8_flags.trace_allocation_stack_interval > 0) {
      allocations_count_.fetch_add(1, std::memory_order_relaxed);
      if (allocations_count_ % v8_flags.trace_allocation_stack_interval == 0) {
        heap_->isolate()->PrintStack(stdout, Isolate::kPrintStackConcise);
      }
    }
  }

  void MoveEvent(Address source, Address target, int size) final {
    if (v8_flags.verify_predictable) {
      allocations_count_.fetch_add(1, std::memory_order_relaxed);
      // Advance synthetic time by making a time request.
      heap_->MonotonicallyIncreasingTimeInMs();

      UpdateAllocationsHash(HeapObject::FromAddress(source));
      UpdateAllocationsHash(HeapObject::FromAddress(target));
      UpdateAllocationsHash(size);

      if (allocations_count_ % v8_flags.dump_allocations_digest_at_alloc == 0) {
        PrintAllocationsHash();
      }
    } else if (v8_flags.fuzzer_gc_analysis) {
      allocations_count_.fetch_add(1, std::memory_order_relaxed);
    }
  }

  void UpdateObjectSizeEvent(Address, int) final {}

 private:
  void UpdateAllocationsHash(HeapObject object) {
    Address object_address = object.address();
    MemoryChunk* memory_chunk = MemoryChunk::FromAddress(object_address);
    AllocationSpace allocation_space = memory_chunk->owner_identity();

    static_assert(kSpaceTagSize + kPageSizeBits <= 32);
    uint32_t value =
        static_cast<uint32_t>(object_address - memory_chunk->address()) |
        (static_cast<uint32_t>(allocation_space) << kPageSizeBits);

    UpdateAllocationsHash(value);
  }

  void UpdateAllocationsHash(uint32_t value) {
    const uint16_t c1 = static_cast<uint16_t>(value);
    const uint16_t c2 = static_cast<uint16_t>(value >> 16);
    raw_allocations_hash_ =
        StringHasher::AddCharacterCore(raw_allocations_hash_, c1);
    raw_allocations_hash_ =
        StringHasher::AddCharacterCore(raw_allocations_hash_, c2);
  }

  void PrintAllocationsHash() {
    uint32_t hash = StringHasher::GetHashCore(raw_allocations_hash_);
    PrintF("\n### Allocations = %zu, hash = 0x%08x\n",
           allocations_count_.load(std::memory_order_relaxed), hash);
  }

  Heap* const heap_;
  // Count of all allocations performed through C++ bottlenecks. This needs to
  // be atomic as objects are moved in parallel in the GC which counts as
  // allocations.
  std::atomic<size_t> allocations_count_{0};
  // Running hash over allocations performed.
  uint32_t raw_allocations_hash_ = 0;
};

void Heap::AddHeapObjectAllocationTracker(
    HeapObjectAllocationTracker* tracker) {
  if (allocation_trackers_.empty() && v8_flags.inline_new) {
    DisableInlineAllocation();
  }
  allocation_trackers_.push_back(tracker);
  if (allocation_trackers_.size() == 1) {
    isolate_->UpdateLogObjectRelocation();
  }
}

void Heap::RemoveHeapObjectAllocationTracker(
    HeapObjectAllocationTracker* tracker) {
  allocation_trackers_.erase(std::remove(allocation_trackers_.begin(),
                                         allocation_trackers_.end(), tracker),
                             allocation_trackers_.end());
  if (allocation_trackers_.empty()) {
    isolate_->UpdateLogObjectRelocation();
  }
  if (allocation_trackers_.empty() && v8_flags.inline_new) {
    EnableInlineAllocation();
  }
}

void Heap::AddRetainingPathTarget(Handle<HeapObject> object,
                                  RetainingPathOption option) {
  if (!v8_flags.track_retaining_path) {
    PrintF("Retaining path tracking requires --track-retaining-path\n");
  } else {
    Handle<WeakArrayList> array(retaining_path_targets(), isolate());
    int index = array->length();
    array = WeakArrayList::AddToEnd(isolate(), array,
                                    MaybeObjectHandle::Weak(object));
    set_retaining_path_targets(*array);
    DCHECK_EQ(array->length(), index + 1);
    retaining_path_target_option_[index] = option;
  }
}

bool Heap::IsRetainingPathTarget(HeapObject object,
                                 RetainingPathOption* option) {
  WeakArrayList targets = retaining_path_targets();
  int length = targets.length();
  MaybeObject object_to_check = HeapObjectReference::Weak(object);
  for (int i = 0; i < length; i++) {
    MaybeObject target = targets.Get(i);
    DCHECK(target->IsWeakOrCleared());
    if (target == object_to_check) {
      DCHECK(retaining_path_target_option_.count(i));
      *option = retaining_path_target_option_[i];
      return true;
    }
  }
  return false;
}

void Heap::PrintRetainingPath(HeapObject target, RetainingPathOption option) {
  PrintF("\n\n\n");
  PrintF("#################################################\n");
  PrintF("Retaining path for %p:\n", reinterpret_cast<void*>(target.ptr()));
  HeapObject object = target;
  std::vector<std::pair<HeapObject, bool>> retaining_path;
  base::Optional<Root> root;
  bool ephemeron = false;
  while (true) {
    retaining_path.push_back(std::make_pair(object, ephemeron));
    if (option == RetainingPathOption::kTrackEphemeronPath &&
        ephemeron_retainer_.count(object)) {
      object = ephemeron_retainer_[object];
      ephemeron = true;
    } else if (retainer_.count(object)) {
      object = retainer_[object];
      ephemeron = false;
    } else {
      if (retaining_root_.count(object)) {
        root.emplace(retaining_root_[object]);
      }
      break;
    }
  }
  int distance = static_cast<int>(retaining_path.size());
  for (auto node : retaining_path) {
    HeapObject node_object = node.first;
    bool node_ephemeron = node.second;
    PrintF("\n");
    PrintF("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
    PrintF("Distance from root %d%s: ", distance,
           node_ephemeron ? " (ephemeron)" : "");
    node_object.ShortPrint();
    PrintF("\n");
#ifdef OBJECT_PRINT
    node_object.Print();
    PrintF("\n");
#endif
    --distance;
  }
  PrintF("\n");
  PrintF("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
  PrintF("Root: %s\n",
         root.has_value() ? RootVisitor::RootName(root.value()) : "Unknown");
  PrintF("-------------------------------------------------\n");
}

void UpdateRetainersMapAfterScavenge(UnorderedHeapObjectMap<HeapObject>* map) {
  // This is only used for Scavenger.
  DCHECK(!v8_flags.minor_ms);

  UnorderedHeapObjectMap<HeapObject> updated_map;

  for (auto pair : *map) {
    HeapObject object = pair.first;
    HeapObject retainer = pair.second;

    if (Heap::InFromPage(object)) {
      MapWord map_word = object.map_word(kRelaxedLoad);
      if (!map_word.IsForwardingAddress()) continue;
      object = map_word.ToForwardingAddress(object);
    }

    if (Heap::InFromPage(retainer)) {
      MapWord map_word = retainer.map_word(kRelaxedLoad);
      if (!map_word.IsForwardingAddress()) continue;
      retainer = map_word.ToForwardingAddress(retainer);
    }

    updated_map[object] = retainer;
  }

  *map = std::move(updated_map);
}

void Heap::UpdateRetainersAfterScavenge() {
  if (!incremental_marking()->IsMarking()) return;

  // This is only used for Scavenger.
  DCHECK(!v8_flags.minor_ms);

  UpdateRetainersMapAfterScavenge(&retainer_);
  UpdateRetainersMapAfterScavenge(&ephemeron_retainer_);

  UnorderedHeapObjectMap<Root> updated_retaining_root;

  for (auto pair : retaining_root_) {
    HeapObject object = pair.first;

    if (Heap::InFromPage(object)) {
      MapWord map_word = object.map_word(kRelaxedLoad);
      if (!map_word.IsForwardingAddress()) continue;
      object = map_word.ToForwardingAddress(object);
    }

    updated_retaining_root[object] = pair.second;
  }

  retaining_root_ = std::move(updated_retaining_root);
}

void Heap::AddRetainer(HeapObject retainer, HeapObject object) {
  if (retainer_.count(object)) return;
  retainer_[object] = retainer;
  RetainingPathOption option = RetainingPathOption::kDefault;
  if (IsRetainingPathTarget(object, &option)) {
    // Check if the retaining path was already printed in
    // AddEphemeronRetainer().
    if (ephemeron_retainer_.count(object) == 0 ||
        option == RetainingPathOption::kDefault) {
      PrintRetainingPath(object, option);
    }
  }
}

void Heap::AddEphemeronRetainer(HeapObject retainer, HeapObject object) {
  if (ephemeron_retainer_.count(object)) return;
  ephemeron_retainer_[object] = retainer;
  RetainingPathOption option = RetainingPathOption::kDefault;
  if (IsRetainingPathTarget(object, &option) &&
      option == RetainingPathOption::kTrackEphemeronPath) {
    // Check if the retaining path was already printed in AddRetainer().
    if (retainer_.count(object) == 0) {
      PrintRetainingPath(object, option);
    }
  }
}

void Heap::AddRetainingRoot(Root root, HeapObject object) {
  if (retaining_root_.count(object)) return;
  retaining_root_[object] = root;
  RetainingPathOption option = RetainingPathOption::kDefault;
  if (IsRetainingPathTarget(object, &option)) {
    PrintRetainingPath(object, option);
  }
}

void Heap::IncrementDeferredCount(v8::Isolate::UseCounterFeature feature) {
  deferred_counters_[feature]++;
}

void Heap::GarbageCollectionPrologue(
    GarbageCollectionReason gc_reason,
    const v8::GCCallbackFlags gc_callback_flags) {
  TRACE_GC(tracer(), GCTracer::Scope::HEAP_PROLOGUE);

  is_current_gc_forced_ = gc_callback_flags & v8::kGCCallbackFlagForced ||
                          current_gc_flags_ & GCFlag::kForced ||
                          force_gc_on_next_allocation_;
  is_current_gc_for_heap_profiler_ =
      gc_reason == GarbageCollectionReason::kHeapProfiler;
  if (force_gc_on_next_allocation_) force_gc_on_next_allocation_ = false;

#ifdef V8_ENABLE_ALLOCATION_TIMEOUT
  heap_allocator_.UpdateAllocationTimeout();
#endif  // V8_ENABLE_ALLOCATION_TIMEOUT

  // There may be an allocation memento behind objects in new space. Upon
  // evacuation of a non-full new space (or if we are on the last page) there
  // may be uninitialized memory behind top. We fill the remainder of the page
  // with a filler.
  if (new_space()) new_space()->MakeLinearAllocationAreaIterable();

  // Reset GC statistics.
  promoted_objects_size_ = 0;
  previous_new_space_surviving_object_size_ = new_space_surviving_object_size_;
  new_space_surviving_object_size_ = 0;
  nodes_died_in_new_space_ = 0;
  nodes_copied_in_new_space_ = 0;
  nodes_promoted_ = 0;

  UpdateMaximumCommitted();

#ifdef DEBUG
  DCHECK(!AllowGarbageCollection::IsAllowed());
  DCHECK_EQ(gc_state(), NOT_IN_GC);

  if (v8_flags.gc_verbose) Print();
#endif  // DEBUG

  memory_allocator()->unmapper()->PrepareForGC();
}

void Heap::GarbageCollectionPrologueInSafepoint() {
  TRACE_GC(tracer(), GCTracer::Scope::HEAP_PROLOGUE_SAFEPOINT);
  gc_count_++;

  DCHECK_EQ(ResizeNewSpaceMode::kNone, resize_new_space_mode_);
  if (new_space_) {
    UpdateNewSpaceAllocationCounter();
    if (!v8_flags.minor_ms) {
      resize_new_space_mode_ = ShouldResizeNewSpace();
      // Pretenuring heuristics require that new space grows before pretenuring
      // feedback is processed.
      if (resize_new_space_mode_ == ResizeNewSpaceMode::kGrow) {
        ExpandNewSpaceSize();
      }
      SemiSpaceNewSpace::From(new_space_)->ResetParkedAllocationBuffers();
    }
  }
}

void Heap::UpdateNewSpaceAllocationCounter() {
  new_space_allocation_counter_ = NewSpaceAllocationCounter();
}

size_t Heap::NewSpaceAllocationCounter() {
  return new_space_allocation_counter_ +
         (new_space_ ? new_space()->AllocatedSinceLastGC() : 0);
}

size_t Heap::SizeOfObjects() {
  size_t total = 0;

  for (SpaceIterator it(this); it.HasNext();) {
    total += it.Next()->SizeOfObjects();
  }
  return total;
}

size_t Heap::TotalGlobalHandlesSize() {
  return isolate_->global_handles()->TotalSize() +
         isolate_->traced_handles()->total_size_bytes();
}

size_t Heap::UsedGlobalHandlesSize() {
  return isolate_->global_handles()->UsedSize() +
         isolate_->traced_handles()->used_size_bytes();
}

void Heap::AddAllocationObserversToAllSpaces(
    AllocationObserver* observer, AllocationObserver* new_space_observer) {
  DCHECK(observer && new_space_observer);

  for (SpaceIterator it(this); it.HasNext();) {
    Space* space = it.Next();
    if (space == new_space()) {
      space->AddAllocationObserver(new_space_observer);
    } else {
      space->AddAllocationObserver(observer);
    }
  }
}

void Heap::RemoveAllocationObserversFromAllSpaces(
    AllocationObserver* observer, AllocationObserver* new_space_observer) {
  DCHECK(observer && new_space_observer);

  for (SpaceIterator it(this); it.HasNext();) {
    Space* space = it.Next();
    if (space == new_space()) {
      space->RemoveAllocationObserver(new_space_observer);
    } else {
      space->RemoveAllocationObserver(observer);
    }
  }
}

void Heap::PublishPendingAllocations() {
  if (v8_flags.enable_third_party_heap) return;
  if (new_space_) new_space_->MarkLabStartInitialized();
  PagedSpaceIterator spaces(this);
  for (PagedSpace* space = spaces.Next(); space != nullptr;
       space = spaces.Next()) {
    space->MoveOriginalTopForward();
  }
  lo_space_->ResetPendingObject();
  if (new_lo_space_) new_lo_space_->ResetPendingObject();
  code_lo_space_->ResetPendingObject();
}

void Heap::DeoptMarkedAllocationSites() {
  // TODO(hpayer): If iterating over the allocation sites list becomes a
  // performance issue, use a cache data structure in heap instead.

  ForeachAllocationSite(allocation_sites_list(), [this](AllocationSite site) {
    if (site.deopt_dependent_code()) {
      DependentCode::MarkCodeForDeoptimization(
          isolate_, site, DependentCode::kAllocationSiteTenuringChangedGroup);
      site.set_deopt_dependent_code(false);
    }
  });

  Deoptimizer::DeoptimizeMarkedCode(isolate_);
}

static GCType GetGCTypeFromGarbageCollector(GarbageCollector collector) {
  switch (collector) {
    case GarbageCollector::MARK_COMPACTOR:
      return kGCTypeMarkSweepCompact;
    case GarbageCollector::SCAVENGER:
      return kGCTypeScavenge;
    case GarbageCollector::MINOR_MARK_SWEEPER:
      return kGCTypeMinorMarkSweep;
    default:
      UNREACHABLE();
  }
}

void Heap::GarbageCollectionEpilogueInSafepoint(GarbageCollector collector) {
  if (collector == GarbageCollector::MARK_COMPACTOR) {
    memory_pressure_level_.store(MemoryPressureLevel::kNone,
                                 std::memory_order_relaxed);

    if (v8_flags.stress_marking > 0) {
      stress_marking_percentage_ = NextStressMarkingLimit();
    }
  }

  TRACE_GC(tracer(), GCTracer::Scope::HEAP_EPILOGUE_SAFEPOINT);

  {
    // Allows handle derefs for all threads/isolates from this thread.
    AllowHandleDereferenceAllThreads allow_all_handle_derefs;
    safepoint()->IterateLocalHeaps([this, collector](LocalHeap* local_heap) {
      local_heap->InvokeGCEpilogueCallbacksInSafepoint(
          GetGCTypeFromGarbageCollector(collector), current_gc_callback_flags_);
    });

    if (isolate()->is_shared_space_isolate()) {
      isolate()->global_safepoint()->IterateClientIsolates(
          [this, collector](Isolate* client) {
            client->heap()->safepoint()->IterateLocalHeaps(
                [this, collector](LocalHeap* local_heap) {
                  local_heap->InvokeGCEpilogueCallbacksInSafepoint(
                      GetGCTypeFromGarbageCollector(collector),
                      current_gc_callback_flags_);
                });
          });
    }
  }

#define UPDATE_COUNTERS_FOR_SPACE(space)                \
  isolate_->counters()->space##_bytes_available()->Set( \
      static_cast<int>(space()->Available()));          \
  isolate_->counters()->space##_bytes_committed()->Set( \
      static_cast<int>(space()->CommittedMemory()));    \
  isolate_->counters()->space##_bytes_used()->Set(      \
      static_cast<int>(space()->SizeOfObjects()));
#define UPDATE_FRAGMENTATION_FOR_SPACE(space)                          \
  if (space()->CommittedMemory() > 0) {                                \
    isolate_->counters()->external_fragmentation_##space()->AddSample( \
        static_cast<int>(100 - (space()->SizeOfObjects() * 100.0) /    \
                                   space()->CommittedMemory()));       \
  }
#define UPDATE_COUNTERS_AND_FRAGMENTATION_FOR_SPACE(space) \
  UPDATE_COUNTERS_FOR_SPACE(space)                         \
  UPDATE_FRAGMENTATION_FOR_SPACE(space)

  if (new_space()) {
    UPDATE_COUNTERS_FOR_SPACE(new_space)
  }

  UPDATE_COUNTERS_AND_FRAGMENTATION_FOR_SPACE(old_space)
  UPDATE_COUNTERS_AND_FRAGMENTATION_FOR_SPACE(code_space)

  UPDATE_COUNTERS_AND_FRAGMENTATION_FOR_SPACE(lo_space)
#undef UPDATE_COUNTERS_FOR_SPACE
#undef UPDATE_FRAGMENTATION_FOR_SPACE
#undef UPDATE_COUNTERS_AND_FRAGMENTATION_FOR_SPACE

#ifdef DEBUG
  if (v8_flags.print_global_handles) isolate_->global_handles()->Print();
  if (v8_flags.print_handles) PrintHandles();
  if (v8_flags.check_handle_count) CheckHandleCount();
#endif

  if (new_space() && !v8_flags.minor_ms) {
    SemiSpaceNewSpace* semi_space_new_space =
        SemiSpaceNewSpace::From(new_space());
    if (heap::ShouldZapGarbage() || v8_flags.clear_free_memory) {
      semi_space_new_space->ZapUnusedMemory();
    }

    {
      TRACE_GC(tracer(), GCTracer::Scope::HEAP_EPILOGUE_REDUCE_NEW_SPACE);
      if (resize_new_space_mode_ == ResizeNewSpaceMode::kShrink) {
        ReduceNewSpaceSize();
      }
    }
    resize_new_space_mode_ = ResizeNewSpaceMode::kNone;

    semi_space_new_space->MakeAllPagesInFromSpaceIterable();
  }

  // Ensure that unmapper task isn't running during full GC. We need access to
  // those pages for accessing page flags when processing old-to-new slots.
  DCHECK_IMPLIES(collector == GarbageCollector::MARK_COMPACTOR,
                 !memory_allocator()->unmapper()->IsRunning());

  // Start concurrent unmapper tasks to free pages queued during GC.
  memory_allocator()->unmapper()->FreeQueuedChunks();

  // Remove CollectionRequested flag from main thread state, as the collection
  // was just performed.
  safepoint()->AssertActive();
  LocalHeap::ThreadState old_state =
      main_thread_local_heap()->state_.ClearCollectionRequested();

  CHECK(old_state.IsRunning());

  // Resume all threads waiting for the GC.
  collection_barrier_->ResumeThreadsAwaitingCollection();
}

void Heap::GarbageCollectionEpilogue(GarbageCollector collector) {
  TRACE_GC(tracer(), GCTracer::Scope::HEAP_EPILOGUE);
  AllowGarbageCollection for_the_rest_of_the_epilogue;

  UpdateMaximumCommitted();

  if (v8_flags.track_retaining_path &&
      collector == GarbageCollector::MARK_COMPACTOR) {
    retainer_.clear();
    ephemeron_retainer_.clear();
    retaining_root_.clear();
  }

  isolate_->counters()->alive_after_last_gc()->Set(
      static_cast<int>(SizeOfObjects()));

  if (CommittedMemory() > 0) {
    isolate_->counters()->external_fragmentation_total()->AddSample(
        static_cast<int>(100 - (SizeOfObjects() * 100.0) / CommittedMemory()));

    isolate_->counters()->heap_sample_total_committed()->AddSample(
        static_cast<int>(CommittedMemory() / KB));
    isolate_->counters()->heap_sample_total_used()->AddSample(
        static_cast<int>(SizeOfObjects() / KB));
    isolate_->counters()->heap_sample_code_space_committed()->AddSample(
        static_cast<int>(code_space()->CommittedMemory() / KB));

    isolate_->counters()->heap_sample_maximum_committed()->AddSample(
        static_cast<int>(MaximumCommittedMemory() / KB));
  }

#ifdef DEBUG
  ReportStatisticsAfterGC();
  if (v8_flags.code_stats) ReportCodeStatistics("After GC");
#endif  // DEBUG

  last_gc_time_ = MonotonicallyIncreasingTimeInMs();
}

GCCallbacksScope::GCCallbacksScope(Heap* heap) : heap_(heap) {
  heap_->gc_callbacks_depth_++;
}

GCCallbacksScope::~GCCallbacksScope() { heap_->gc_callbacks_depth_--; }

bool GCCallbacksScope::CheckReenter() const {
  return heap_->gc_callbacks_depth_ == 1;
}

void Heap::HandleGCRequest() {
  if (IsStressingScavenge() && stress_scavenge_observer_->HasRequestedGC()) {
    CollectGarbage(NEW_SPACE, GarbageCollectionReason::kTesting);
    stress_scavenge_observer_->RequestedGCDone();
  } else if (HighMemoryPressure()) {
    CheckMemoryPressure();
  } else if (CollectionRequested()) {
    CheckCollectionRequested();
  } else if (incremental_marking()->CollectionRequested()) {
    CollectAllGarbage(current_gc_flags_,
                      GarbageCollectionReason::kFinalizeMarkingViaStackGuard,
                      current_gc_callback_flags_);
  }
}

void Heap::ScheduleMinorGCTaskIfNeeded() {
  DCHECK_NOT_NULL(minor_gc_job_);
  minor_gc_job_->ScheduleTaskIfNeeded(this);
}

void Heap::StartMinorMSIncrementalMarkingIfNeeded() {
  if (v8_flags.concurrent_minor_ms_marking && !IsTearingDown() &&
      !incremental_marking()->IsMarking() &&
      incremental_marking()->CanBeStarted() && V8_LIKELY(!v8_flags.gc_global) &&
      (new_space()->Size() >=
       MinorGCJob::YoungGenerationTaskTriggerSize(this))) {
    StartIncrementalMarking(GCFlag::kNoFlags, GarbageCollectionReason::kTask,
                            kNoGCCallbackFlags,
                            GarbageCollector::MINOR_MARK_SWEEPER);
  }
}

void Heap::CollectAllGarbage(GCFlags gc_flags,
                             GarbageCollectionReason gc_reason,
                             const v8::GCCallbackFlags gc_callback_flags) {
  current_gc_flags_ = gc_flags;
  CollectGarbage(OLD_SPACE, gc_reason, gc_callback_flags);
  current_gc_flags_ = GCFlag::kNoFlags;
}

namespace {

intptr_t CompareWords(int size, HeapObject a, HeapObject b) {
  int slots = size / kTaggedSize;
  DCHECK_EQ(a.Size(), size);
  DCHECK_EQ(b.Size(), size);
  Tagged_t* slot_a = reinterpret_cast<Tagged_t*>(a.address());
  Tagged_t* slot_b = reinterpret_cast<Tagged_t*>(b.address());
  for (int i = 0; i < slots; i++) {
    if (*slot_a != *slot_b) {
      return *slot_a - *slot_b;
    }
    slot_a++;
    slot_b++;
  }
  return 0;
}

void ReportDuplicates(int size, std::vector<HeapObject>* objects) {
  if (objects->size() == 0) return;

  sort(objects->begin(), objects->end(), [size](HeapObject a, HeapObject b) {
    intptr_t c = CompareWords(size, a, b);
    if (c != 0) return c < 0;
    return a < b;
  });

  std::vector<std::pair<int, HeapObject>> duplicates;
  HeapObject current = (*objects)[0];
  int count = 1;
  for (size_t i = 1; i < objects->size(); i++) {
    if (CompareWords(size, current, (*objects)[i]) == 0) {
      count++;
    } else {
      if (count > 1) {
        duplicates.push_back(std::make_pair(count - 1, current));
      }
      count = 1;
      current = (*objects)[i];
    }
  }
  if (count > 1) {
    duplicates.push_back(std::make_pair(count - 1, current));
  }

  int threshold = v8_flags.trace_duplicate_threshold_kb * KB;

  sort(duplicates.begin(), duplicates.end());
  for (auto it = duplicates.rbegin(); it != duplicates.rend(); ++it) {
    int duplicate_bytes = it->first * size;
    if (duplicate_bytes < threshold) break;
    PrintF("%d duplicates of size %d each (%dKB)\n", it->first, size,
           duplicate_bytes / KB);
    PrintF("Sample object: ");
    it->second.Print();
    PrintF("============================\n");
  }
}
}  // anonymous namespace

void Heap::CollectAllAvailableGarbage(GarbageCollectionReason gc_reason) {
  // Min and max number of attempts for GC. The method will continue with more
  // GCs until the root set is stable.
  static constexpr int kMaxNumberOfAttempts = 7;
  static constexpr int kMinNumberOfAttempts = 2;

  // Returns the number of roots. We assume stack layout is stable but global
  // roots could change between GCs due to finalizers and weak callbacks.
  const auto num_roots = [this]() {
    size_t js_roots = 0;
    js_roots += isolate()->global_handles()->handles_count();
    js_roots += isolate()->eternal_handles()->handles_count();
    size_t cpp_roots = 0;
    if (auto* cpp_heap = CppHeap::From(cpp_heap_)) {
      cpp_roots += cpp_heap->GetStrongPersistentRegion().NodesInUse();
      cpp_roots +=
          cpp_heap->GetStrongCrossThreadPersistentRegion().NodesInUse();
    }
    return js_roots + cpp_roots;
  };

  if (gc_reason == GarbageCollectionReason::kLastResort) {
    InvokeNearHeapLimitCallback();
  }
  RCS_SCOPE(isolate(), RuntimeCallCounterId::kGC_Custom_AllAvailableGarbage);

  // The optimizing compiler may be unnecessarily holding on to memory.
  isolate()->AbortConcurrentOptimization(BlockingBehavior::kDontBlock);
  isolate()->ClearSerializerData();
  isolate()->compilation_cache()->Clear();

  current_gc_flags_ =
      GCFlag::kReduceMemoryFootprint |
      (gc_reason == GarbageCollectionReason::kLowMemoryNotification
           ? GCFlag::kForced
           : GCFlag::kNoFlags);
  for (int attempt = 0; attempt < kMaxNumberOfAttempts; attempt++) {
    const size_t roots_before = num_roots();
    CollectGarbage(OLD_SPACE, gc_reason, kNoGCCallbackFlags);
    if ((roots_before == num_roots()) &&
        ((attempt + 1) >= kMinNumberOfAttempts)) {
      break;
    }
  }
  current_gc_flags_ = GCFlag::kNoFlags;

  EagerlyFreeExternalMemory();

  if (v8_flags.trace_duplicate_threshold_kb) {
    std::map<int, std::vector<HeapObject>> objects_by_size;
    PagedSpaceIterator spaces(this);
    for (PagedSpace* space = spaces.Next(); space != nullptr;
         space = spaces.Next()) {
      PagedSpaceObjectIterator it(this, space);
      for (HeapObject obj = it.Next(); !obj.is_null(); obj = it.Next()) {
        objects_by_size[obj.Size()].push_back(obj);
      }
    }
    {
      LargeObjectSpaceObjectIterator it(lo_space());
      for (HeapObject obj = it.Next(); !obj.is_null(); obj = it.Next()) {
        objects_by_size[obj.Size()].push_back(obj);
      }
    }
    for (auto it = objects_by_size.rbegin(); it != objects_by_size.rend();
         ++it) {
      ReportDuplicates(it->first, &it->second);
    }
  }
}

void Heap::PreciseCollectAllGarbage(GCFlags gc_flags,
                                    GarbageCollectionReason gc_reason,
                                    const GCCallbackFlags gc_callback_flags) {
  if (!incremental_marking()->IsStopped()) {
    FinalizeIncrementalMarkingAtomically(gc_reason);
  }
  CollectAllGarbage(gc_flags, gc_reason, gc_callback_flags);
}

void Heap::ReportExternalMemoryPressure() {
  const GCCallbackFlags kGCCallbackFlagsForExternalMemory =
      static_cast<GCCallbackFlags>(
          kGCCallbackFlagSynchronousPhantomCallbackProcessing |
          kGCCallbackFlagCollectAllExternalMemory);
  int64_t current = external_memory_.total();
  int64_t baseline = external_memory_.low_since_mark_compact();
  int64_t limit = external_memory_.limit();
  TRACE_EVENT2(
      "devtools.timeline,v8", "V8.ExternalMemoryPressure", "external_memory_mb",
      static_cast<int>((current - baseline) / MB), "external_memory_limit_mb",
      static_cast<int>((limit - baseline) / MB));
  if (current > baseline + external_memory_hard_limit()) {
    CollectAllGarbage(
        GCFlag::kReduceMemoryFootprint,
        GarbageCollectionReason::kExternalMemoryPressure,
        static_cast<GCCallbackFlags>(kGCCallbackFlagCollectAllAvailableGarbage |
                                     kGCCallbackFlagsForExternalMemory));
    return;
  }
  if (incremental_marking()->IsStopped()) {
    if (incremental_marking()->CanBeStarted()) {
      StartIncrementalMarking(GCFlagsForIncrementalMarking(),
                              GarbageCollectionReason::kExternalMemoryPressure,
                              kGCCallbackFlagsForExternalMemory);
    } else {
      CollectAllGarbage(i::GCFlag::kNoFlags,
                        GarbageCollectionReason::kExternalMemoryPressure,
                        kGCCallbackFlagsForExternalMemory);
    }
  } else {
    // Incremental marking is turned on and has already been started.
    current_gc_callback_flags_ = static_cast<GCCallbackFlags>(
        current_gc_callback_flags_ | kGCCallbackFlagsForExternalMemory);
    incremental_marking()->AdvanceAndFinalizeIfNecessary();
  }
}

int64_t Heap::external_memory_limit() { return external_memory_.limit(); }

Heap::DevToolsTraceEventScope::DevToolsTraceEventScope(Heap* heap,
                                                       const char* event_name,
                                                       const char* event_type)
    : heap_(heap), event_name_(event_name) {
  TRACE_EVENT_BEGIN2("devtools.timeline,v8", event_name_, "usedHeapSizeBefore",
                     heap_->SizeOfObjects(), "type", event_type);
}

Heap::DevToolsTraceEventScope::~DevToolsTraceEventScope() {
  TRACE_EVENT_END1("devtools.timeline,v8", event_name_, "usedHeapSizeAfter",
                   heap_->SizeOfObjects());
}

namespace {

template <typename Callback>
void InvokeExternalCallbacks(Isolate* isolate, Callback callback) {
  AllowGarbageCollection allow_gc;
  AllowJavascriptExecution allow_js(isolate);
  // Temporary override any embedder stack state as callbacks may create
  // their own state on the stack and recursively trigger GC.
  EmbedderStackStateScope embedder_scope(
      isolate->heap(), EmbedderStackStateScope::kExplicitInvocation,
      StackState::kMayContainHeapPointers);
  VMState<EXTERNAL> callback_state(isolate);

  callback();
}

size_t GlobalMemorySizeFromV8Size(size_t v8_size) {
  const size_t kGlobalMemoryToV8Ratio = 2;
  return std::min(static_cast<uint64_t>(std::numeric_limits<size_t>::max()),
                  static_cast<uint64_t>(v8_size) * kGlobalMemoryToV8Ratio);
}

}  // anonymous namespace

void Heap::SetOldGenerationAndGlobalMaximumSize(
    size_t max_old_generation_size) {
  max_old_generation_size_.store(max_old_generation_size,
                                 std::memory_order_relaxed);
  max_global_memory_size_ = GlobalMemorySizeFromV8Size(max_old_generation_size);
}

void Heap::SetOldGenerationAndGlobalAllocationLimit(
    size_t new_old_generation_allocation_limit,
    size_t new_global_allocation_limit) {
  CHECK_GE(new_global_allocation_limit, new_old_generation_allocation_limit);
  old_generation_allocation_limit_.store(new_old_generation_allocation_limit,
                                         std::memory_order_relaxed);
  global_allocation_limit_ = new_global_allocation_limit;
  old_generation_allocation_limit_configured_ = true;
}

void Heap::ResetOldGenerationAndGlobalAllocationLimit() {
  SetOldGenerationAndGlobalAllocationLimit(
      initial_old_generation_size_,
      GlobalMemorySizeFromV8Size(initial_old_generation_size_));
  old_generation_allocation_limit_configured_ = false;
}

void Heap::CollectGarbage(AllocationSpace space,
                          GarbageCollectionReason gc_reason,
                          const v8::GCCallbackFlags gc_callback_flags) {
  if (V8_UNLIKELY(!deserialization_complete_)) {
    // During isolate initialization heap always grows. GC is only requested
    // if a new page allocation fails. In such a case we should crash with
    // an out-of-memory instead of performing GC because the prologue/epilogue
    // callbacks may see objects that are not yet deserialized.
    CHECK(always_allocate());
    FatalProcessOutOfMemory("GC during deserialization");
  }

  // CollectGarbage consists of three parts:
  // 1. The prologue part which may execute callbacks. These callbacks may
  // allocate and trigger another garbage collection.
  // 2. The main garbage collection phase.
  // 3. The epilogue part which may execute callbacks. These callbacks may
  // allocate and trigger another garbage collection

  // Part 1: Invoke all callbacks which should happen before the actual garbage
  // collection is triggered. Note that these callbacks may trigger another
  // garbage collection since they may allocate.

  DCHECK(AllowGarbageCollection::IsAllowed());

  const char* collector_reason = nullptr;
  const GarbageCollector collector =
      SelectGarbageCollector(space, gc_reason, &collector_reason);
  current_or_last_garbage_collector_ = collector;

  if (collector == GarbageCollector::MARK_COMPACTOR &&
      incremental_marking()->IsMinorMarking()) {
    CollectGarbage(NEW_SPACE, GarbageCollectionReason::kFinalizeMinorMS);
  }

  const GCType gc_type = GetGCTypeFromGarbageCollector(collector);

  // Prologue callbacks. These callbacks may trigger GC themselves and thus
  // cannot be related exactly to garbage collection cycles.
  //
  // GCTracer scopes are managed by callees.
  InvokeExternalCallbacks(isolate(), [this, gc_callback_flags, gc_type]() {
    // Ensure that all pending phantom callbacks are invoked.
    isolate()->global_handles()->InvokeSecondPassPhantomCallbacks();

    // Prologue callbacks registered with Heap.
    CallGCPrologueCallbacks(gc_type, gc_callback_flags,
                            GCTracer::Scope::HEAP_EXTERNAL_PROLOGUE);
  });

  // The main garbage collection phase.
  DisallowGarbageCollection no_gc_during_gc;

  size_t committed_memory_before = collector == GarbageCollector::MARK_COMPACTOR
                                       ? CommittedOldGenerationMemory()
                                       : 0;
  {
    tracer()->StartObservablePause();
    VMState<GC> state(isolate());
    DevToolsTraceEventScope devtools_trace_event_scope(
        this, IsYoungGenerationCollector(collector) ? "MinorGC" : "MajorGC",
        ToString(gc_reason));

    // We need a stack marker at the top of all entry points to allow
    // deterministic passes over the stack. E.g., a verifier that should only
    // find a subset of references of the marker.
    //
    // TODO(chromium:1056170): Consider adding a component that keeps track
    // of relevant GC stack regions where interesting pointers can be found.
    stack().SetMarkerToCurrentStackPosition();

    GarbageCollectionPrologue(gc_reason, gc_callback_flags);
    {
      GCTracer::RecordGCPhasesInfo record_gc_phases_info(this, collector,
                                                         gc_reason);
      base::Optional<TimedHistogramScope> histogram_timer_scope;
      base::Optional<OptionalTimedHistogramScope>
          histogram_timer_priority_scope;
      TRACE_EVENT0("v8", record_gc_phases_info.trace_event_name());
      if (record_gc_phases_info.type_timer()) {
        histogram_timer_scope.emplace(record_gc_phases_info.type_timer(),
                                      isolate_);
      }
      if (record_gc_phases_info.type_priority_timer()) {
        OptionalTimedHistogramScopeMode mode =
            isolate_->IsMemorySavingsModeActive()
                ? OptionalTimedHistogramScopeMode::DONT_TAKE_TIME
                : OptionalTimedHistogramScopeMode::TAKE_TIME;
        histogram_timer_priority_scope.emplace(
            record_gc_phases_info.type_priority_timer(), isolate_, mode);
      }

      if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) {
        tp_heap_->CollectGarbage();
      } else {
        PerformGarbageCollection(collector, gc_reason, collector_reason);
      }
      // Clear flags describing the current GC now that the current GC is
      // complete. Do this before GarbageCollectionEpilogue() since that could
      // trigger another unforced GC.
      is_current_gc_forced_ = false;
      is_current_gc_for_heap_profiler_ = false;

      if (collector == GarbageCollector::MARK_COMPACTOR ||
          collector == GarbageCollector::SCAVENGER) {
        tracer()->RecordGCPhasesHistograms(record_gc_phases_info.mode());
      }
    }

    GarbageCollectionEpilogue(collector);
    if (collector == GarbageCollector::MARK_COMPACTOR &&
        v8_flags.track_detached_contexts) {
      isolate()->CheckDetachedContextsAfterGC();
    }

    if (collector == GarbageCollector::MARK_COMPACTOR) {
      if (memory_reducer_ != nullptr) {
        memory_reducer_->NotifyMarkCompact(committed_memory_before);
      }
      if (initial_max_old_generation_size_ < max_old_generation_size() &&
          OldGenerationSizeOfObjects() <
              initial_max_old_generation_size_threshold_) {
        SetOldGenerationAndGlobalMaximumSize(initial_max_old_generation_size_);
      }
    }

    tracer()->StopAtomicPause();
    tracer()->StopObservablePause();
    tracer()->UpdateStatistics(collector);
    // Young generation cycles finish atomically. It is important that
    // StopObservablePause, UpdateStatistics and StopCycle are called in this
    // order; the latter may replace the current event with that of an
    // interrupted full cycle.
    if (IsYoungGenerationCollector(collector)) {
      tracer()->StopYoungCycleIfNeeded();
    } else {
      tracer()->StopFullCycleIfNeeded();
    }
  }

  // Epilogue callbacks. These callbacks may trigger GC themselves and thus
  // cannot be related exactly to garbage collection cycles.
  //
  // GCTracer scopes are managed by callees.
  InvokeExternalCallbacks(isolate(), [this, gc_callback_flags, gc_type]() {
    // Epilogue callbacks registered with Heap.
    CallGCEpilogueCallbacks(gc_type, gc_callback_flags,
                            GCTracer::Scope::HEAP_EXTERNAL_EPILOGUE);

    isolate()->global_handles()->PostGarbageCollectionProcessing(
        gc_callback_flags);
  });

  if (collector == GarbageCollector::MARK_COMPACTOR &&
      (gc_callback_flags & (kGCCallbackFlagForced |
                            kGCCallbackFlagCollectAllAvailableGarbage)) != 0) {
    isolate()->CountUsage(v8::Isolate::kForcedGC);
  }

  // Start incremental marking for the next cycle. We do this only for scavenger
  // to avoid a loop where mark-compact causes another mark-compact.
  if (collector == GarbageCollector::SCAVENGER) {
    DCHECK(!v8_flags.minor_ms);
    StartIncrementalMarkingIfAllocationLimitIsReached(
        GCFlagsForIncrementalMarking(),
        kGCCallbackScheduleIdleGarbageCollection);
  }

  if (!CanExpandOldGeneration(0)) {
    InvokeNearHeapLimitCallback();
    if (!CanExpandOldGeneration(0)) {
      FatalProcessOutOfMemory("Reached heap limit");
    }
  }
}

int Heap::NotifyContextDisposed(bool has_dependent_context) {
  if (!has_dependent_context) {
    tracer()->ResetSurvivalEvents();
    ResetOldGenerationAndGlobalAllocationLimit();
    if (memory_reducer_) {
      memory_reducer_->NotifyPossibleGarbage();
    }
  }
  isolate()->AbortConcurrentOptimization(BlockingBehavior::kDontBlock);
  if (!isolate()->context().is_null()) {
    RemoveDirtyFinalizationRegistriesOnContext(isolate()->raw_native_context());
    isolate()->raw_native_context().set_retained_maps(
        ReadOnlyRoots(this).empty_weak_array_list());
  }
  return ++contexts_disposed_;
}

void Heap::StartIncrementalMarking(GCFlags gc_flags,
                                   GarbageCollectionReason gc_reason,
                                   GCCallbackFlags gc_callback_flags,
                                   GarbageCollector collector) {
  DCHECK(incremental_marking()->IsStopped());

  // Delay incremental marking start while concurrent sweeping still has work.
  // This helps avoid large CompleteSweep blocks on the main thread when major
  // incremental marking should be scheduled following a minor GC.
  if (sweeper()->AreMinorSweeperTasksRunning()) return;

  if (IsYoungGenerationCollector(collector)) {
    CompleteSweepingYoung();
  } else {
    // Sweeping needs to be completed such that markbits are all cleared before
    // starting marking again.
    CompleteSweepingFull();
  }

  base::Optional<SafepointScope> safepoint_scope;

  {
    AllowGarbageCollection allow_shared_gc;
    IgnoreLocalGCRequests ignore_gc_requests(this);

    SafepointKind safepoint_kind = isolate()->is_shared_space_isolate()
                                       ? SafepointKind::kGlobal
                                       : SafepointKind::kIsolate;
    safepoint_scope.emplace(isolate(), safepoint_kind);
  }

#ifdef DEBUG
  VerifyCountersAfterSweeping();
#endif

  if (isolate()->is_shared_space_isolate()) {
    isolate()->global_safepoint()->IterateClientIsolates([](Isolate* client) {
      if (v8_flags.concurrent_marking) {
        client->heap()->concurrent_marking()->Pause();
      }
    });
  }

  // Now that sweeping is completed, we can start the next full GC cycle.
  tracer()->StartCycle(collector, gc_reason, nullptr,
                       GCTracer::MarkingType::kIncremental);

  current_gc_flags_ = gc_flags;
  current_gc_callback_flags_ = gc_callback_flags;

  incremental_marking()->Start(collector, gc_reason);

  if (isolate()->is_shared_space_isolate()) {
    isolate()->global_safepoint()->IterateClientIsolates([](Isolate* client) {
      if (v8_flags.concurrent_marking &&
          client->heap()->incremental_marking()->IsMarking()) {
        client->heap()->concurrent_marking()->Resume();
      }
    });
  }
}

namespace {
void CompleteArrayBufferSweeping(Heap* heap) {
  auto* array_buffer_sweeper = heap->array_buffer_sweeper();
  if (array_buffer_sweeper->sweeping_in_progress()) {
    auto* tracer = heap->tracer();
    GCTracer::Scope::ScopeId scope_id;

    switch (tracer->GetCurrentCollector()) {
      case GarbageCollector::MINOR_MARK_SWEEPER:
        scope_id = GCTracer::Scope::MINOR_MS_COMPLETE_SWEEP_ARRAY_BUFFERS;
        break;
      case GarbageCollector::SCAVENGER:
        scope_id = GCTracer::Scope::SCAVENGER_COMPLETE_SWEEP_ARRAY_BUFFERS;
        break;
      case GarbageCollector::MARK_COMPACTOR:
        scope_id = GCTracer::Scope::MC_COMPLETE_SWEEP_ARRAY_BUFFERS;
    }

    TRACE_GC_EPOCH(tracer, scope_id, ThreadKind::kMain);
    array_buffer_sweeper->EnsureFinished();
  }
}
}  // namespace

void Heap::CompleteSweepingFull() {
  EnsureSweepingCompleted(SweepingForcedFinalizationMode::kUnifiedHeap);

  DCHECK(!sweeping_in_progress());
  DCHECK_IMPLIES(cpp_heap(),
                 !CppHeap::From(cpp_heap())->sweeper().IsSweepingInProgress());
  DCHECK(!tracer()->IsSweepingInProgress());
}

void Heap::StartIncrementalMarkingIfAllocationLimitIsReached(
    GCFlags gc_flags, const GCCallbackFlags gc_callback_flags) {
  if (v8_flags.separate_gc_phases && gc_callbacks_depth_ > 0) {
    // Do not start incremental marking while invoking GC callbacks.
    // Heap::CollectGarbage already decided which GC is going to be invoked. In
    // case it chose a young-gen GC, starting an incremental full GC during
    // callbacks would break the separate GC phases guarantee.
    return;
  }
  if (incremental_marking()->IsStopped()) {
    switch (IncrementalMarkingLimitReached()) {
      case IncrementalMarkingLimit::kHardLimit:
        StartIncrementalMarking(
            gc_flags,
            OldGenerationSpaceAvailable() <= NewSpaceTargetCapacity()
                ? GarbageCollectionReason::kAllocationLimit
                : GarbageCollectionReason::kGlobalAllocationLimit,
            gc_callback_flags);
        break;
      case IncrementalMarkingLimit::kSoftLimit:
        incremental_marking()->incremental_marking_job()->ScheduleTask();
        break;
      case IncrementalMarkingLimit::kFallbackForEmbedderLimit:
        // This is a fallback case where no appropriate limits have been
        // configured yet.
        if (memory_reducer() != nullptr) {
          memory_reducer()->NotifyPossibleGarbage();
        }
        break;
      case IncrementalMarkingLimit::kNoLimit:
        break;
    }
  }
}

void Heap::StartIncrementalMarkingIfAllocationLimitIsReachedBackground() {
  if (incremental_marking()->IsMarking() ||
      !incremental_marking()->CanBeStarted()) {
    return;
  }

  const size_t old_generation_space_available = OldGenerationSpaceAvailable();

  if (old_generation_space_available < NewSpaceTargetCapacity()) {
    incremental_marking()->incremental_marking_job()->ScheduleTask();
  }
}

void Heap::MoveRange(HeapObject dst_object, const ObjectSlot dst_slot,
                     const ObjectSlot src_slot, int len,
                     WriteBarrierMode mode) {
  DCHECK_NE(len, 0);
  DCHECK_NE(dst_object.map(), ReadOnlyRoots(this).fixed_cow_array_map());
  const ObjectSlot dst_end(dst_slot + len);
  // Ensure no range overflow.
  DCHECK(dst_slot < dst_end);
  DCHECK(src_slot < src_slot + len);

  if ((v8_flags.concurrent_marking && incremental_marking()->IsMarking()) ||
      (v8_flags.minor_ms && sweeper()->IsIteratingPromotedPages())) {
    if (dst_slot < src_slot) {
      // Copy tagged values forward using relaxed load/stores that do not
      // involve value decompression.
      const AtomicSlot atomic_dst_end(dst_end);
      AtomicSlot dst(dst_slot);
      AtomicSlot src(src_slot);
      while (dst < atomic_dst_end) {
        *dst = *src;
        ++dst;
        ++src;
      }
    } else {
      // Copy tagged values backwards using relaxed load/stores that do not
      // involve value decompression.
      const AtomicSlot atomic_dst_begin(dst_slot);
      AtomicSlot dst(dst_slot + len - 1);
      AtomicSlot src(src_slot + len - 1);
      while (dst >= atomic_dst_begin) {
        *dst = *src;
        --dst;
        --src;
      }
    }
  } else {
    MemMove(dst_slot.ToVoidPtr(), src_slot.ToVoidPtr(), len * kTaggedSize);
  }
  if (mode == SKIP_WRITE_BARRIER) return;
  WriteBarrierForRange(dst_object, dst_slot, dst_end);
}

// Instantiate Heap::CopyRange() for ObjectSlot and MaybeObjectSlot.
template void Heap::CopyRange<ObjectSlot>(HeapObject dst_object,
                                          ObjectSlot dst_slot,
                                          ObjectSlot src_slot, int len,
                                          WriteBarrierMode mode);
template void Heap::CopyRange<MaybeObjectSlot>(HeapObject dst_object,
                                               MaybeObjectSlot dst_slot,
                                               MaybeObjectSlot src_slot,
                                               int len, WriteBarrierMode mode);

template <typename TSlot>
void Heap::CopyRange(HeapObject dst_object, const TSlot dst_slot,
                     const TSlot src_slot, int len, WriteBarrierMode mode) {
  DCHECK_NE(len, 0);

  DCHECK_NE(dst_object.map(), ReadOnlyRoots(this).fixed_cow_array_map());
  const TSlot dst_end(dst_slot + len);
  // Ensure ranges do not overlap.
  DCHECK(dst_end <= src_slot || (src_slot + len) <= dst_slot);

  if ((v8_flags.concurrent_marking && incremental_marking()->IsMarking()) ||
      (v8_flags.minor_ms && sweeper()->IsIteratingPromotedPages())) {
    // Copy tagged values using relaxed load/stores that do not involve value
    // decompression.
    const AtomicSlot atomic_dst_end(dst_end);
    AtomicSlot dst(dst_slot);
    AtomicSlot src(src_slot);
    while (dst < atomic_dst_end) {
      *dst = *src;
      ++dst;
      ++src;
    }
  } else {
    MemCopy(dst_slot.ToVoidPtr(), src_slot.ToVoidPtr(), len * kTaggedSize);
  }
  if (mode == SKIP_WRITE_BARRIER) return;
  WriteBarrierForRange(dst_object, dst_slot, dst_end);
}

bool Heap::CollectionRequested() {
  return collection_barrier_->WasGCRequested();
}

void Heap::CollectGarbageForBackground(LocalHeap* local_heap) {
  CHECK(local_heap->is_main_thread());
  CollectAllGarbage(current_gc_flags_,
                    GarbageCollectionReason::kBackgroundAllocationFailure,
                    current_gc_callback_flags_);
}

void Heap::CheckCollectionRequested() {
  if (!CollectionRequested()) return;

  CollectAllGarbage(current_gc_flags_,
                    GarbageCollectionReason::kBackgroundAllocationFailure,
                    current_gc_callback_flags_);
}

#if V8_ENABLE_WEBASSEMBLY
void Heap::EnsureWasmCanonicalRttsSize(int length) {
  HandleScope scope(isolate());

  Handle<WeakArrayList> current_rtts = handle(wasm_canonical_rtts(), isolate_);
  if (length <= current_rtts->length()) return;
  Handle<WeakArrayList> new_rtts = WeakArrayList::EnsureSpace(
      isolate(), current_rtts, length, AllocationType::kOld);
  new_rtts->set_length(length);
  set_wasm_canonical_rtts(*new_rtts);

  // Wrappers are indexed by canonical rtt length, and an additional boolean
  // storing whether the corresponding function is imported or not.
  int required_wrapper_length = 2 * length;
  Handle<WeakArrayList> current_wrappers =
      handle(js_to_wasm_wrappers(), isolate_);
  if (required_wrapper_length <= current_wrappers->length()) return;
  Handle<WeakArrayList> new_wrappers =
      WeakArrayList::EnsureSpace(isolate(), current_wrappers,
                                 required_wrapper_length, AllocationType::kOld);
  new_wrappers->set_length(required_wrapper_length);
  set_js_to_wasm_wrappers(*new_wrappers);
}
#endif

void Heap::UpdateSurvivalStatistics(int start_new_space_size) {
  if (start_new_space_size == 0) return;

  promotion_ratio_ = (static_cast<double>(promoted_objects_size_) /
                      static_cast<double>(start_new_space_size) * 100);

  if (previous_new_space_surviving_object_size_ > 0) {
    promotion_rate_ =
        (static_cast<double>(promoted_objects_size_) /
         static_cast<double>(previous_new_space_surviving_object_size_) * 100);
  } else {
    promotion_rate_ = 0;
  }

  new_space_surviving_rate_ =
      (static_cast<double>(new_space_surviving_object_size_) /
       static_cast<double>(start_new_space_size) * 100);

  double survival_rate = promotion_ratio_ + new_space_surviving_rate_;
  tracer()->AddSurvivalRatio(survival_rate);
}

namespace {

GCTracer::Scope::ScopeId CollectorScopeId(GarbageCollector collector) {
  switch (collector) {
    case GarbageCollector::MARK_COMPACTOR:
      return GCTracer::Scope::ScopeId::MARK_COMPACTOR;
    case GarbageCollector::MINOR_MARK_SWEEPER:
      return GCTracer::Scope::ScopeId::MINOR_MARK_SWEEPER;
    case GarbageCollector::SCAVENGER:
      return GCTracer::Scope::ScopeId::SCAVENGER;
  }
  UNREACHABLE();
}

void ClearStubCaches(Isolate* isolate) {
  isolate->load_stub_cache()->Clear();
  isolate->store_stub_cache()->Clear();

  if (isolate->is_shared_space_isolate()) {
    isolate->global_safepoint()->IterateClientIsolates([](Isolate* client) {
      client->load_stub_cache()->Clear();
      client->store_stub_cache()->Clear();
    });
  }
}

}  // namespace

void Heap::PerformGarbageCollection(GarbageCollector collector,
                                    GarbageCollectionReason gc_reason,
                                    const char* collector_reason) {
  DisallowJavascriptExecution no_js(isolate());

  if (IsYoungGenerationCollector(collector)) {
    CompleteSweepingYoung();
    if (v8_flags.verify_heap) {
      // If heap verification is enabled, we want to ensure that sweeping is
      // completed here, as it will be triggered from Heap::Verify anyway.
      // In this way, sweeping finalization is accounted to the corresponding
      // full GC cycle.
      CompleteSweepingFull();
    }
  } else {
    DCHECK_EQ(GarbageCollector::MARK_COMPACTOR, collector);
    CompleteSweepingFull();

    memory_allocator()->unmapper()->EnsureUnmappingCompleted();
  }

  base::Optional<SafepointScope> safepoint_scope;
  {
    AllowGarbageCollection allow_shared_gc;
    IgnoreLocalGCRequests ignore_gc_requests(this);

    SafepointKind safepoint_kind = isolate()->is_shared_space_isolate()
                                       ? SafepointKind::kGlobal
                                       : SafepointKind::kIsolate;
    safepoint_scope.emplace(isolate(), safepoint_kind);
  }

  if (!incremental_marking_->IsMarking() ||
      (collector == GarbageCollector::SCAVENGER)) {
    tracer()->StartCycle(collector, gc_reason, collector_reason,
                         GCTracer::MarkingType::kAtomic);
  }

  tracer()->StartAtomicPause();
  if ((!Heap::IsYoungGenerationCollector(collector) || v8_flags.minor_ms) &&
      incremental_marking_->IsMarking()) {
    DCHECK_IMPLIES(Heap::IsYoungGenerationCollector(collector),
                   incremental_marking_->IsMinorMarking());
    tracer()->UpdateCurrentEvent(gc_reason, collector_reason);
  }

  DCHECK(tracer()->IsConsistentWithCollector(collector));
  TRACE_GC_EPOCH(tracer(), CollectorScopeId(collector), ThreadKind::kMain);

  collection_barrier_->StopTimeToCollectionTimer();

  HeapVerifier::VerifyHeapIfEnabled(this);

  if (isolate()->is_shared_space_isolate()) {
    isolate()->global_safepoint()->IterateClientIsolates(
        [collector](Isolate* client) {
          CHECK(client->heap()->deserialization_complete());

          if (v8_flags.concurrent_marking) {
            client->heap()->concurrent_marking()->Pause();
          }

          if (collector == GarbageCollector::MARK_COMPACTOR) {
            Sweeper* const client_sweeper = client->heap()->sweeper();
            client_sweeper->ContributeAndWaitForPromotedPagesIteration();
          }
          HeapVerifier::VerifyHeapIfEnabled(client->heap());
        });
  }

  tracer()->StartInSafepoint();

  GarbageCollectionPrologueInSafepoint();

  if (new_space()) new_space()->Prologue();

  size_t start_young_generation_size =
      NewSpaceSize() + (new_lo_space() ? new_lo_space()->SizeOfObjects() : 0);

  // Make sure allocation observers are disabled until the new new space
  // capacity is set in the epilogue.
  PauseAllocationObserversScope pause_observers(this);

  size_t new_space_capacity_before_gc = NewSpaceTargetCapacity();

  if (collector == GarbageCollector::MARK_COMPACTOR) {
    MarkCompact();
  } else if (collector == GarbageCollector::MINOR_MARK_SWEEPER) {
    MinorMarkSweep();
  } else {
    DCHECK_EQ(GarbageCollector::SCAVENGER, collector);
    Scavenge();
  }

  pretenuring_handler_.ProcessPretenuringFeedback(new_space_capacity_before_gc);

  UpdateSurvivalStatistics(static_cast<int>(start_young_generation_size));
  ShrinkOldGenerationAllocationLimitIfNotConfigured();

  if (collector == GarbageCollector::SCAVENGER) {
    // Objects that died in the new space might have been accounted
    // as bytes marked ahead of schedule by the incremental marker.
    incremental_marking()->UpdateMarkedBytesAfterScavenge(
        start_young_generation_size - SurvivedYoungObjectSize());
  }

  isolate_->counters()->objs_since_last_young()->Set(0);

  isolate_->eternal_handles()->PostGarbageCollectionProcessing();

  // Update relocatables.
  Relocatable::PostGarbageCollectionProcessing(isolate_);

  if (isolate_->is_shared_space_isolate()) {
    // Allows handle derefs for all threads/isolates from this thread.
    AllowHandleDereferenceAllThreads allow_all_handle_derefs;
    isolate()->global_safepoint()->IterateClientIsolates([](Isolate* client) {
      Relocatable::PostGarbageCollectionProcessing(client);
    });
  }

  // First round weak callbacks are not supposed to allocate and trigger
  // nested GCs.
  isolate_->global_handles()->InvokeFirstPassWeakCallbacks();

  if (cpp_heap() && (collector == GarbageCollector::MARK_COMPACTOR ||
                     collector == GarbageCollector::MINOR_MARK_SWEEPER)) {
    // TraceEpilogue may trigger operations that invalidate global handles. It
    // has to be called *after* all other operations that potentially touch
    // and reset global handles. It is also still part of the main garbage
    // collection pause and thus needs to be called *before* any operation
    // that can potentially trigger recursive garbage collections.
    TRACE_GC(tracer(), GCTracer::Scope::HEAP_EMBEDDER_TRACING_EPILOGUE);
    CppHeap::From(cpp_heap())->TraceEpilogue();
  }

  RecomputeLimits(collector);

  if (collector == GarbageCollector::MARK_COMPACTOR) {
    // After every full GC the old generation allocation limit should be
    // configured.
    DCHECK(old_generation_allocation_limit_configured_);

    ClearStubCaches(isolate());
  }

  GarbageCollectionEpilogueInSafepoint(collector);

  tracer()->StopInSafepoint();

  HeapVerifier::VerifyHeapIfEnabled(this);

  if (isolate()->is_shared_space_isolate()) {
    isolate()->global_safepoint()->IterateClientIsolates([](Isolate* client) {
      HeapVerifier::VerifyHeapIfEnabled(client->heap());

      if (v8_flags.concurrent_marking &&
          client->heap()->incremental_marking()->IsMarking()) {
        client->heap()->concurrent_marking()->Resume();
      }
    });
  }
}

bool Heap::CollectGarbageShared(LocalHeap* local_heap,
                                GarbageCollectionReason gc_reason) {
  CHECK(deserialization_complete());
  DCHECK(isolate()->has_shared_space());

  Isolate* shared_space_isolate = isolate()->shared_space_isolate();
  return shared_space_isolate->heap()->CollectGarbageFromAnyThread(local_heap,
                                                                   gc_reason);
}

bool Heap::CollectGarbageFromAnyThread(LocalHeap* local_heap,
                                       GarbageCollectionReason gc_reason) {
  DCHECK(local_heap->IsRunning());

  if (isolate() == local_heap->heap()->isolate() &&
      local_heap->is_main_thread()) {
    CollectAllGarbage(current_gc_flags_, gc_reason, current_gc_callback_flags_);
    return true;
  } else {
    if (!collection_barrier_->TryRequestGC()) return false;

    const LocalHeap::ThreadState old_state =
        main_thread_local_heap()->state_.SetCollectionRequested();

    if (old_state.IsRunning()) {
      const bool performed_gc =
          collection_barrier_->AwaitCollectionBackground(local_heap);
      return performed_gc;
    } else {
      DCHECK(old_state.IsParked());
      return false;
    }
  }
}

void Heap::CompleteSweepingYoung() {
  CompleteArrayBufferSweeping(this);

  // If sweeping is in progress and there are no sweeper tasks running, finish
  // the sweeping here, to avoid having to pause and resume during the young
  // generation GC.
  FinishSweepingIfOutOfWork();

  if (v8_flags.minor_ms) {
    EnsureYoungSweepingCompleted();
  }

#if defined(CPPGC_YOUNG_GENERATION)
  // Always complete sweeping if young generation is enabled.
  if (cpp_heap()) {
    if (auto* iheap = CppHeap::From(cpp_heap());
        iheap->generational_gc_supported())
      iheap->FinishSweepingIfRunning();
  }
#endif  // defined(CPPGC_YOUNG_GENERATION)
}

void Heap::EnsureSweepingCompletedForObject(HeapObject object) {
  if (!sweeping_in_progress()) return;

  BasicMemoryChunk* basic_chunk = BasicMemoryChunk::FromHeapObject(object);
  if (basic_chunk->InReadOnlySpace()) return;

  MemoryChunk* chunk = MemoryChunk::cast(basic_chunk);
  if (chunk->SweepingDone()) return;

  // SweepingDone() is always true for large pages.
  DCHECK(!chunk->IsLargePage());

  Page* page = Page::cast(chunk);
  sweeper()->EnsurePageIsSwept(page);
}

void Heap::RecomputeLimits(GarbageCollector collector) {
  if (!((collector == GarbageCollector::MARK_COMPACTOR) ||
        (HasLowYoungGenerationAllocationRate() &&
         old_generation_allocation_limit_configured_))) {
    return;
  }

  double v8_gc_speed =
      tracer()->CombinedMarkCompactSpeedInBytesPerMillisecond();
  double v8_mutator_speed =
      tracer()->CurrentOldGenerationAllocationThroughputInBytesPerMillisecond();
  double v8_growing_factor = MemoryController<V8HeapTrait>::GrowingFactor(
      this, max_old_generation_size(), v8_gc_speed, v8_mutator_speed);
  double global_growing_factor = 0;
  double embedder_gc_speed = tracer()->EmbedderSpeedInBytesPerMillisecond();
  double embedder_speed =
      tracer()->CurrentEmbedderAllocationThroughputInBytesPerMillisecond();
  double embedder_growing_factor =
      (embedder_gc_speed > 0 && embedder_speed > 0)
          ? MemoryController<GlobalMemoryTrait>::GrowingFactor(
                this, max_global_memory_size_, embedder_gc_speed,
                embedder_speed)
          : 0;
  global_growing_factor = std::max(v8_growing_factor, embedder_growing_factor);

  size_t old_gen_size = OldGenerationSizeOfObjects();
  size_t new_space_capacity = NewSpaceTargetCapacity();
  HeapGrowingMode mode = CurrentHeapGrowingMode();

  if (collector == GarbageCollector::MARK_COMPACTOR) {
    external_memory_.ResetAfterGC();

    size_t new_old_generation_allocation_limit =
        MemoryController<V8HeapTrait>::CalculateAllocationLimit(
            this, old_gen_size, min_old_generation_size_,
            max_old_generation_size(), new_space_capacity, v8_growing_factor,
            mode);
    DCHECK_GT(global_growing_factor, 0);
    size_t new_global_allocation_limit =
        MemoryController<GlobalMemoryTrait>::CalculateAllocationLimit(
            this, GlobalSizeOfObjects(), min_global_memory_size_,
            max_global_memory_size_, new_space_capacity, global_growing_factor,
            mode);
    SetOldGenerationAndGlobalAllocationLimit(
        new_old_generation_allocation_limit, new_global_allocation_limit);
    CheckIneffectiveMarkCompact(
        old_gen_size, tracer()->AverageMarkCompactMutatorUtilization());
  } else if (HasLowYoungGenerationAllocationRate() &&
             old_generation_allocation_limit_configured_) {
    size_t new_old_generation_allocation_limit =
        MemoryController<V8HeapTrait>::CalculateAllocationLimit(
            this, old_gen_size, min_old_generation_size_,
            max_old_generation_size(), new_space_capacity, v8_growing_factor,
            mode);
    new_old_generation_allocation_limit = std::min(
        new_old_generation_allocation_limit, old_generation_allocation_limit());
    DCHECK_GT(global_growing_factor, 0);
    size_t new_global_allocation_limit =
        MemoryController<GlobalMemoryTrait>::CalculateAllocationLimit(
            this, GlobalSizeOfObjects(), min_global_memory_size_,
            max_global_memory_size_, new_space_capacity, global_growing_factor,
            mode);
    new_global_allocation_limit =
        std::min(new_global_allocation_limit, global_allocation_limit_);
    SetOldGenerationAndGlobalAllocationLimit(
        new_old_generation_allocation_limit, new_global_allocation_limit);
  }

  CHECK_EQ(max_global_memory_size_,
           GlobalMemorySizeFromV8Size(max_old_generation_size_));
  CHECK_GE(global_allocation_limit_, old_generation_allocation_limit_);

  if (v8_flags.memory_balancer) {
    mb_->UpdateExternalAllocationLimit(global_allocation_limit_ -
                                       old_generation_allocation_limit_);
    mb_->NotifyGC();
  }
}

void Heap::CallGCPrologueCallbacks(GCType gc_type, GCCallbackFlags flags,
                                   GCTracer::Scope::ScopeId scope_id) {
  if (gc_prologue_callbacks_.IsEmpty()) return;

  GCCallbacksScope scope(this);
  if (scope.CheckReenter()) {
    RCS_SCOPE(isolate(), RuntimeCallCounterId::kGCPrologueCallback);
    TRACE_GC(tracer(), scope_id);
    HandleScope handle_scope(isolate());
    gc_prologue_callbacks_.Invoke(gc_type, flags);
  }
}

void Heap::CallGCEpilogueCallbacks(GCType gc_type, GCCallbackFlags flags,
                                   GCTracer::Scope::ScopeId scope_id) {
  if (gc_epilogue_callbacks_.IsEmpty()) return;

  GCCallbacksScope scope(this);
  if (scope.CheckReenter()) {
    RCS_SCOPE(isolate(), RuntimeCallCounterId::kGCEpilogueCallback);
    TRACE_GC(tracer(), scope_id);
    HandleScope handle_scope(isolate());
    gc_epilogue_callbacks_.Invoke(gc_type, flags);
  }
}

void Heap::MarkCompact() {
  SetGCState(MARK_COMPACT);

  PROFILE(isolate_, CodeMovingGCEvent());
  CodePageHeaderModificationScope code_modification(
      "MarkCompact needs to access the marking bitmap in the page header");

  UpdateOldGenerationAllocationCounter();
  uint64_t size_of_objects_before_gc = SizeOfObjects();

  mark_compact_collector()->Prepare();

  ms_count_++;
  contexts_disposed_ = 0;

  MarkCompactPrologue();

  mark_compact_collector()->CollectGarbage();

  MarkCompactEpilogue();

  if (v8_flags.allocation_site_pretenuring) {
    EvaluateOldSpaceLocalPretenuring(size_of_objects_before_gc);
  }
  // This should be updated before PostGarbageCollectionProcessing, which
  // can cause another GC. Take into account the objects promoted during
  // GC.
  old_generation_allocation_counter_at_last_gc_ +=
      static_cast<size_t>(promoted_objects_size_);
  old_generation_size_at_last_gc_ = OldGenerationSizeOfObjects();
  global_memory_at_last_gc_ = GlobalSizeOfObjects();
}

void Heap::MinorMarkSweep() {
  DCHECK(v8_flags.minor_ms);
  CHECK_EQ(NOT_IN_GC, gc_state());
  DCHECK(new_space());
  DCHECK(!incremental_marking()->IsMajorMarking());

  TRACE_GC(tracer(), GCTracer::Scope::MINOR_MS);

  AlwaysAllocateScope always_allocate(this);

  SetGCState(MINOR_MARK_SWEEP);
  minor_mark_sweep_collector_->CollectGarbage();
  SetGCState(NOT_IN_GC);
}

void Heap::MarkCompactEpilogue() {
  TRACE_GC(tracer(), GCTracer::Scope::MC_EPILOGUE);
  SetGCState(NOT_IN_GC);

  isolate_->counters()->objs_since_last_full()->Set(0);
}

void Heap::MarkCompactPrologue() {
  TRACE_GC(tracer(), GCTracer::Scope::MC_PROLOGUE);
  isolate_->descriptor_lookup_cache()->Clear();
  RegExpResultsCache::Clear(string_split_cache());
  RegExpResultsCache::Clear(regexp_multiple_cache());

  FlushNumberStringCache();
}

void Heap::Scavenge() {
  DCHECK_NOT_NULL(new_space());
  DCHECK_IMPLIES(v8_flags.separate_gc_phases,
                 !incremental_marking()->IsMarking());

  if (v8_flags.trace_incremental_marking &&
      !incremental_marking()->IsStopped()) {
    isolate()->PrintWithTimestamp(
        "[IncrementalMarking] Scavenge during marking.\n");
  }

  TRACE_GC(tracer(), GCTracer::Scope::SCAVENGER_SCAVENGE);
  base::MutexGuard guard(relocation_mutex());
  // Young generation garbage collection is orthogonal from full GC marking. It
  // is possible that objects that are currently being processed for marking are
  // reclaimed in the young generation GC that interleaves concurrent marking.
  // Pause concurrent markers to allow processing them using
  // `UpdateMarkingWorklistAfterYoungGenGC()`.
  ConcurrentMarking::PauseScope pause_js_marking(concurrent_marking());
  CppHeap::PauseConcurrentMarkingScope pause_cpp_marking(
      CppHeap::From(cpp_heap_));
  // There are soft limits in the allocation code, designed to trigger a mark
  // sweep collection by failing allocations. There is no sense in trying to
  // trigger one during scavenge: scavenges allocation should always succeed.
  AlwaysAllocateScope scope(this);

  // Bump-pointer allocations done during scavenge are not real allocations.
  // Pause the inline allocation steps.
  IncrementalMarking::PauseBlackAllocationScope pause_black_allocation(
      incremental_marking());

  SetGCState(SCAVENGE);

  SemiSpaceNewSpace::From(new_space())->EvacuatePrologue();

  // We also flip the young generation large object space. All large objects
  // will be in the from space.
  new_lo_space()->Flip();
  new_lo_space()->ResetPendingObject();

  // Implements Cheney's copying algorithm
  scavenger_collector_->CollectGarbage();

  SetGCState(NOT_IN_GC);
}

bool Heap::ExternalStringTable::Contains(String string) {
  for (size_t i = 0; i < young_strings_.size(); ++i) {
    if (young_strings_[i] == string) return true;
  }
  for (size_t i = 0; i < old_strings_.size(); ++i) {
    if (old_strings_[i] == string) return true;
  }
  return false;
}

void Heap::UpdateExternalString(String string, size_t old_payload,
                                size_t new_payload) {
  DCHECK(string.IsExternalString());
  if (v8_flags.enable_third_party_heap) return;

  Page* page = Page::FromHeapObject(string);

  if (old_payload > new_payload) {
    page->DecrementExternalBackingStoreBytes(
        ExternalBackingStoreType::kExternalString, old_payload - new_payload);
  } else {
    page->IncrementExternalBackingStoreBytes(
        ExternalBackingStoreType::kExternalString, new_payload - old_payload);
  }
}

String Heap::UpdateYoungReferenceInExternalStringTableEntry(Heap* heap,
                                                            FullObjectSlot p) {
  // This is only used for Scavenger.
  DCHECK(!v8_flags.minor_ms);

  PtrComprCageBase cage_base(heap->isolate());
  HeapObject obj = HeapObject::cast(*p);
  MapWord first_word = obj.map_word(cage_base, kRelaxedLoad);

  String new_string;

  if (InFromPage(obj)) {
    if (!first_word.IsForwardingAddress()) {
      // Unreachable external string can be finalized.
      String string = String::cast(obj);
      if (!string.IsExternalString(cage_base)) {
        // Original external string has been internalized.
        DCHECK(string.IsThinString(cage_base));
        return String();
      }
      heap->FinalizeExternalString(string);
      return String();
    }
    new_string = String::cast(first_word.ToForwardingAddress(obj));
  } else {
    new_string = String::cast(obj);
  }

  // String is still reachable.
  if (new_string.IsThinString(cage_base)) {
    // Filtering Thin strings out of the external string table.
    return String();
  } else if (new_string.IsExternalString(cage_base)) {
    MemoryChunk::MoveExternalBackingStoreBytes(
        ExternalBackingStoreType::kExternalString,
        Page::FromAddress((*p).ptr()), Page::FromHeapObject(new_string),
        ExternalString::cast(new_string).ExternalPayloadSize());
    return new_string;
  }

  // Internalization can replace external strings with non-external strings.
  return new_string.IsExternalString(cage_base) ? new_string : String();
}

void Heap::ExternalStringTable::VerifyYoung() {
#ifdef DEBUG
  std::set<String> visited_map;
  std::map<MemoryChunk*, size_t> size_map;
  ExternalBackingStoreType type = ExternalBackingStoreType::kExternalString;
  for (size_t i = 0; i < young_strings_.size(); ++i) {
    String obj = String::cast(young_strings_[i]);
    MemoryChunk* mc = MemoryChunk::FromHeapObject(obj);
    DCHECK(mc->InYoungGeneration());
    DCHECK(heap_->InYoungGeneration(obj));
    DCHECK(!obj.IsTheHole(heap_->isolate()));
    DCHECK(obj.IsExternalString());
    // Note: we can have repeated elements in the table.
    DCHECK_EQ(0, visited_map.count(obj));
    visited_map.insert(obj);
    size_map[mc] += ExternalString::cast(obj).ExternalPayloadSize();
  }
  for (std::map<MemoryChunk*, size_t>::iterator it = size_map.begin();
       it != size_map.end(); it++)
    DCHECK_EQ(it->first->ExternalBackingStoreBytes(type), it->second);
#endif
}

void Heap::ExternalStringTable::Verify() {
#ifdef DEBUG
  std::set<String> visited_map;
  std::map<MemoryChunk*, size_t> size_map;
  ExternalBackingStoreType type = ExternalBackingStoreType::kExternalString;
  VerifyYoung();
  for (size_t i = 0; i < old_strings_.size(); ++i) {
    String obj = String::cast(old_strings_[i]);
    MemoryChunk* mc = MemoryChunk::FromHeapObject(obj);
    DCHECK(!mc->InYoungGeneration());
    DCHECK(!heap_->InYoungGeneration(obj));
    DCHECK(!obj.IsTheHole(heap_->isolate()));
    DCHECK(obj.IsExternalString());
    // Note: we can have repeated elements in the table.
    DCHECK_EQ(0, visited_map.count(obj));
    visited_map.insert(obj);
    size_map[mc] += ExternalString::cast(obj).ExternalPayloadSize();
  }
  for (std::map<MemoryChunk*, size_t>::iterator it = size_map.begin();
       it != size_map.end(); it++)
    DCHECK_EQ(it->first->ExternalBackingStoreBytes(type), it->second);
#endif
}

void Heap::ExternalStringTable::UpdateYoungReferences(
    Heap::ExternalStringTableUpdaterCallback updater_func) {
  if (young_strings_.empty()) return;

  FullObjectSlot start(young_strings_.data());
  FullObjectSlot end(young_strings_.data() + young_strings_.size());
  FullObjectSlot last = start;

  for (FullObjectSlot p = start; p < end; ++p) {
    String target = updater_func(heap_, p);

    if (target.is_null()) continue;

    DCHECK(target.IsExternalString());

    if (InYoungGeneration(target)) {
      // String is still in new space. Update the table entry.
      last.store(target);
      ++last;
    } else {
      // String got promoted. Move it to the old string list.
      old_strings_.push_back(target);
    }
  }

  DCHECK(last <= end);
  young_strings_.resize(last - start);
  if (v8_flags.verify_heap) {
    VerifyYoung();
  }
}

void Heap::ExternalStringTable::PromoteYoung() {
  old_strings_.reserve(old_strings_.size() + young_strings_.size());
  std::move(std::begin(young_strings_), std::end(young_strings_),
            std::back_inserter(old_strings_));
  young_strings_.clear();
}

void Heap::ExternalStringTable::IterateYoung(RootVisitor* v) {
  if (!young_strings_.empty()) {
    v->VisitRootPointers(
        Root::kExternalStringsTable, nullptr,
        FullObjectSlot(young_strings_.data()),
        FullObjectSlot(young_strings_.data() + young_strings_.size()));
  }
}

void Heap::ExternalStringTable::IterateAll(RootVisitor* v) {
  IterateYoung(v);
  if (!old_strings_.empty()) {
    v->VisitRootPointers(
        Root::kExternalStringsTable, nullptr,
        FullObjectSlot(old_strings_.data()),
        FullObjectSlot(old_strings_.data() + old_strings_.size()));
  }
}

void Heap::UpdateYoungReferencesInExternalStringTable(
    ExternalStringTableUpdaterCallback updater_func) {
  external_string_table_.UpdateYoungReferences(updater_func);
}

void Heap::ExternalStringTable::UpdateReferences(
    Heap::ExternalStringTableUpdaterCallback updater_func) {
  if (old_strings_.size() > 0) {
    FullObjectSlot start(old_strings_.data());
    FullObjectSlot end(old_strings_.data() + old_strings_.size());
    for (FullObjectSlot p = start; p < end; ++p)
      p.store(updater_func(heap_, p));
  }

  UpdateYoungReferences(updater_func);
}

void Heap::UpdateReferencesInExternalStringTable(
    ExternalStringTableUpdaterCallback updater_func) {
  external_string_table_.UpdateReferences(updater_func);
}

void Heap::ProcessAllWeakReferences(WeakObjectRetainer* retainer) {
  ProcessNativeContexts(retainer);
  ProcessAllocationSites(retainer);
  ProcessDirtyJSFinalizationRegistries(retainer);
}

void Heap::ProcessNativeContexts(WeakObjectRetainer* retainer) {
  Object head = VisitWeakList<Context>(this, native_contexts_list(), retainer);
  // Update the head of the list of contexts.
  set_native_contexts_list(head);
}

void Heap::ProcessAllocationSites(WeakObjectRetainer* retainer) {
  Object allocation_site_obj =
      VisitWeakList<AllocationSite>(this, allocation_sites_list(), retainer);
  set_allocation_sites_list(allocation_site_obj);
}

void Heap::ProcessDirtyJSFinalizationRegistries(WeakObjectRetainer* retainer) {
  Object head = VisitWeakList<JSFinalizationRegistry>(
      this, dirty_js_finalization_registries_list(), retainer);
  set_dirty_js_finalization_registries_list(head);
  // If the list is empty, set the tail to undefined. Otherwise the tail is set
  // by WeakListVisitor<JSFinalizationRegistry>::VisitLiveObject.
  if (head.IsUndefined(isolate())) {
    set_dirty_js_finalization_registries_list_tail(head);
  }
}

void Heap::ProcessWeakListRoots(WeakObjectRetainer* retainer) {
  set_native_contexts_list(retainer->RetainAs(native_contexts_list()));
  set_allocation_sites_list(retainer->RetainAs(allocation_sites_list()));
  set_dirty_js_finalization_registries_list(
      retainer->RetainAs(dirty_js_finalization_registries_list()));
  set_dirty_js_finalization_registries_list_tail(
      retainer->RetainAs(dirty_js_finalization_registries_list_tail()));
}

void Heap::ForeachAllocationSite(
    Object list, const std::function<void(AllocationSite)>& visitor) {
  DisallowGarbageCollection no_gc;
  Object current = list;
  while (current.IsAllocationSite()) {
    AllocationSite site = AllocationSite::cast(current);
    visitor(site);
    Object current_nested = site.nested_site();
    while (current_nested.IsAllocationSite()) {
      AllocationSite nested_site = AllocationSite::cast(current_nested);
      visitor(nested_site);
      current_nested = nested_site.nested_site();
    }
    current = site.weak_next();
  }
}

void Heap::ResetAllAllocationSitesDependentCode(AllocationType allocation) {
  DisallowGarbageCollection no_gc_scope;
  bool marked = false;

  ForeachAllocationSite(allocation_sites_list(),
                        [&marked, allocation, this](AllocationSite site) {
                          if (site.GetAllocationType() == allocation) {
                            site.ResetPretenureDecision();
                            site.set_deopt_dependent_code(true);
                            marked = true;
                            pretenuring_handler_
                                .RemoveAllocationSitePretenuringFeedback(site);
                            return;
                          }
                        });
  if (marked) isolate_->stack_guard()->RequestDeoptMarkedAllocationSites();
}

void Heap::EvaluateOldSpaceLocalPretenuring(
    uint64_t size_of_objects_before_gc) {
  uint64_t size_of_objects_after_gc = SizeOfObjects();
  double old_generation_survival_rate =
      (static_cast<double>(size_of_objects_after_gc) * 100) /
      static_cast<double>(size_of_objects_before_gc);

  if (old_generation_survival_rate < kOldSurvivalRateLowThreshold) {
    // Too many objects died in the old generation, pretenuring of wrong
    // allocation sites may be the cause for that. We have to deopt all
    // dependent code registered in the allocation sites to re-evaluate
    // our pretenuring decisions.
    ResetAllAllocationSitesDependentCode(AllocationType::kOld);
    if (v8_flags.trace_pretenuring) {
      PrintF(
          "Deopt all allocation sites dependent code due to low survival "
          "rate in the old generation %f\n",
          old_generation_survival_rate);
    }
  }
}

void Heap::VisitExternalResources(v8::ExternalResourceVisitor* visitor) {
  DisallowGarbageCollection no_gc;
  // All external strings are listed in the external string table.

  class ExternalStringTableVisitorAdapter : public RootVisitor {
   public:
    explicit ExternalStringTableVisitorAdapter(
        Isolate* isolate, v8::ExternalResourceVisitor* visitor)
        : isolate_(isolate), visitor_(visitor) {}
    void VisitRootPointers(Root root, const char* description,
                           FullObjectSlot start, FullObjectSlot end) override {
      for (FullObjectSlot p = start; p < end; ++p) {
        DCHECK((*p).IsExternalString());
        visitor_->VisitExternalString(
            Utils::ToLocal(Handle<String>(String::cast(*p), isolate_)));
      }
    }

   private:
    Isolate* isolate_;
    v8::ExternalResourceVisitor* visitor_;
  } external_string_table_visitor(isolate(), visitor);

  external_string_table_.IterateAll(&external_string_table_visitor);
}

static_assert(IsAligned(FixedDoubleArray::kHeaderSize, kDoubleAlignment));

#ifdef V8_COMPRESS_POINTERS
// TODO(ishell, v8:8875): When pointer compression is enabled the kHeaderSize
// is only kTaggedSize aligned but we can keep using unaligned access since
// both x64 and arm64 architectures (where pointer compression supported)
// allow unaligned access to doubles.
static_assert(IsAligned(ByteArray::kHeaderSize, kTaggedSize));
#else
static_assert(IsAligned(ByteArray::kHeaderSize, kDoubleAlignment));
#endif

static_assert(!USE_ALLOCATION_ALIGNMENT_BOOL ||
              (HeapNumber::kValueOffset & kDoubleAlignmentMask) == kTaggedSize);

int Heap::GetMaximumFillToAlign(AllocationAlignment alignment) {
  if (V8_COMPRESS_POINTERS_8GB_BOOL) return 0;
  switch (alignment) {
    case kTaggedAligned:
      return 0;
    case kDoubleAligned:
    case kDoubleUnaligned:
      return kDoubleSize - kTaggedSize;
    default:
      UNREACHABLE();
  }
}

// static
int Heap::GetFillToAlign(Address address, AllocationAlignment alignment) {
  if (V8_COMPRESS_POINTERS_8GB_BOOL) return 0;
  if (alignment == kDoubleAligned && (address & kDoubleAlignmentMask) != 0)
    return kTaggedSize;
  if (alignment == kDoubleUnaligned && (address & kDoubleAlignmentMask) == 0) {
    return kDoubleSize - kTaggedSize;  // No fill if double is always aligned.
  }
  return 0;
}

size_t Heap::GetCodeRangeReservedAreaSize() {
  return CodeRange::GetWritableReservedAreaSize();
}

HeapObject Heap::PrecedeWithFiller(HeapObject object, int filler_size) {
  CreateFillerObjectAt(object.address(), filler_size);
  return HeapObject::FromAddress(object.address() + filler_size);
}

HeapObject Heap::PrecedeWithFillerBackground(HeapObject object,
                                             int filler_size) {
  CreateFillerObjectAtBackground(object.address(), filler_size);
  return HeapObject::FromAddress(object.address() + filler_size);
}

HeapObject Heap::AlignWithFillerBackground(HeapObject object, int object_size,
                                           int allocation_size,
                                           AllocationAlignment alignment) {
  const int filler_size = allocation_size - object_size;
  DCHECK_LT(0, filler_size);
  const int pre_filler = GetFillToAlign(object.address(), alignment);
  if (pre_filler) {
    object = PrecedeWithFillerBackground(object, pre_filler);
  }
  DCHECK_LE(0, filler_size - pre_filler);
  const int post_filler = filler_size - pre_filler;
  if (post_filler) {
    CreateFillerObjectAtBackground(object.address() + object_size, post_filler);
  }
  return object;
}

void* Heap::AllocateExternalBackingStore(
    const std::function<void*(size_t)>& allocate, size_t byte_length) {
  if (!always_allocate() && new_space()) {
    size_t new_space_backing_store_bytes =
        new_space()->ExternalBackingStoreOverallBytes();
    if (new_space_backing_store_bytes >= 2 * DefaultMaxSemiSpaceSize() &&
        new_space_backing_store_bytes >= byte_length) {
      // Performing a young generation GC amortizes over the allocated backing
      // store bytes and may free enough external bytes for this allocation.
      CollectGarbage(NEW_SPACE,
                     GarbageCollectionReason::kExternalMemoryPressure);
    }
  }
  void* result = allocate(byte_length);
  if (result) return result;
  if (!always_allocate()) {
    for (int i = 0; i < 2; i++) {
      CollectGarbage(OLD_SPACE,
                     GarbageCollectionReason::kExternalMemoryPressure);
      result = allocate(byte_length);
      if (result) return result;
    }
    CollectAllAvailableGarbage(
        GarbageCollectionReason::kExternalMemoryPressure);
  }
  return allocate(byte_length);
}

// When old generation allocation limit is not configured (before the first full
// GC), this method shrinks the initial very large old generation size. This
// method can only shrink allocation limits but not increase it again.
void Heap::ShrinkOldGenerationAllocationLimitIfNotConfigured() {
  if (!old_generation_allocation_limit_configured_ &&
      tracer()->SurvivalEventsRecorded()) {
    const size_t minimum_growing_step =
        MemoryController<V8HeapTrait>::MinimumAllocationLimitGrowingStep(
            CurrentHeapGrowingMode());
    size_t new_old_generation_allocation_limit =
        std::max(OldGenerationSizeOfObjects() + minimum_growing_step,
                 static_cast<size_t>(
                     static_cast<double>(old_generation_allocation_limit()) *
                     (tracer()->AverageSurvivalRatio() / 100)));
    new_old_generation_allocation_limit = std::min(
        new_old_generation_allocation_limit, old_generation_allocation_limit());
    size_t new_global_allocation_limit = std::max(
        GlobalSizeOfObjects() + minimum_growing_step,
        static_cast<size_t>(static_cast<double>(global_allocation_limit_) *
                            (tracer()->AverageSurvivalRatio() / 100)));
    new_global_allocation_limit =
        std::min(new_global_allocation_limit, global_allocation_limit_);
    SetOldGenerationAndGlobalAllocationLimit(
        new_old_generation_allocation_limit, new_global_allocation_limit);
    // We need to update limits but still remain in the "not configured" state.
    // The first full GC will configure the heap.
    old_generation_allocation_limit_configured_ = false;
  }
}

void Heap::FlushNumberStringCache() {
  // Flush the number to string cache.
  int len = number_string_cache().length();
  for (int i = 0; i < len; i++) {
    number_string_cache().set_undefined(i);
  }
}

namespace {

void CreateFillerObjectAtImpl(Heap* heap, Address addr, int size,
                              ClearFreedMemoryMode clear_memory_mode) {
  if (size == 0) return;
  DCHECK_IMPLIES(V8_COMPRESS_POINTERS_8GB_BOOL,
                 IsAligned(addr, kObjectAlignment8GbHeap));
  DCHECK_IMPLIES(V8_COMPRESS_POINTERS_8GB_BOOL,
                 IsAligned(size, kObjectAlignment8GbHeap));

  CodePageMemoryModificationScope memory_modification_scope(
      BasicMemoryChunk::FromAddress(addr));

  // TODO(v8:13070): Filler sizes are irrelevant for 8GB+ heaps. Adding them
  // should be avoided in this mode.
  HeapObject filler = HeapObject::FromAddress(addr);
  ReadOnlyRoots roots(heap);
  if (size == kTaggedSize) {
    filler.set_map_after_allocation(roots.unchecked_one_pointer_filler_map(),
                                    SKIP_WRITE_BARRIER);
    // Ensure the filler map is properly initialized.
    DCHECK(filler.map(heap->isolate()).IsMap());
  } else if (size == 2 * kTaggedSize) {
    filler.set_map_after_allocation(roots.unchecked_two_pointer_filler_map(),
                                    SKIP_WRITE_BARRIER);
    if (clear_memory_mode == ClearFreedMemoryMode::kClearFreedMemory) {
      AtomicSlot slot(ObjectSlot(addr) + 1);
      *slot = static_cast<Tagged_t>(kClearedFreeMemoryValue);
    }
    // Ensure the filler map is properly initialized.
    DCHECK(filler.map(heap->isolate()).IsMap());
  } else {
    DCHECK_GT(size, 2 * kTaggedSize);
    filler.set_map_after_allocation(roots.unchecked_free_space_map(),
                                    SKIP_WRITE_BARRIER);
    FreeSpace::cast(filler).set_size(size, kRelaxedStore);
    if (clear_memory_mode == ClearFreedMemoryMode::kClearFreedMemory) {
      MemsetTagged(ObjectSlot(addr) + 2, Object(kClearedFreeMemoryValue),
                   (size / kTaggedSize) - 2);
    }

    // During bootstrapping we need to create a free space object before its
    // map is initialized. In this case we cannot access the map yet, as it
    // might be null, or not set up properly yet.
    DCHECK_IMPLIES(roots.is_initialized(RootIndex::kFreeSpaceMap),
                   filler.map(heap->isolate()).IsMap());
  }
}

#ifdef DEBUG
void VerifyNoNeedToClearSlots(Address start, Address end) {
  BasicMemoryChunk* basic_chunk = BasicMemoryChunk::FromAddress(start);
  if (basic_chunk->InReadOnlySpace()) return;
  MemoryChunk* chunk = static_cast<MemoryChunk*>(basic_chunk);
  if (chunk->InYoungGeneration()) return;
  BaseSpace* space = chunk->owner();
  space->heap()->VerifySlotRangeHasNoRecordedSlots(start, end);
}
#else
void VerifyNoNeedToClearSlots(Address start, Address end) {}
#endif  // DEBUG

}  // namespace

void Heap::CreateFillerObjectAtBackground(Address addr, int size) {
  // TODO(leszeks): Verify that no slots need to be recorded.
  // Do not verify whether slots are cleared here: the concurrent thread is not
  // allowed to access the main thread's remembered set.
  CreateFillerObjectAtRaw(addr, size,
                          ClearFreedMemoryMode::kDontClearFreedMemory,
                          ClearRecordedSlots::kNo, VerifyNoSlotsRecorded::kNo);
}

void Heap::CreateFillerObjectAtSweeper(Address addr, int size) {
  // Do not verify whether slots are cleared here: the concurrent sweeper is not
  // allowed to access the main thread's remembered set.
  CreateFillerObjectAtRaw(addr, size,
                          ClearFreedMemoryMode::kDontClearFreedMemory,
                          ClearRecordedSlots::kNo, VerifyNoSlotsRecorded::kNo);
}

void Heap::CreateFillerObjectAt(Address addr, int size,
                                ClearFreedMemoryMode clear_memory_mode) {
  CreateFillerObjectAtRaw(addr, size, clear_memory_mode,
                          ClearRecordedSlots::kNo, VerifyNoSlotsRecorded::kYes);
}

void Heap::CreateFillerObjectAtRaw(
    Address addr, int size, ClearFreedMemoryMode clear_memory_mode,
    ClearRecordedSlots clear_slots_mode,
    VerifyNoSlotsRecorded verify_no_slots_recorded) {
  // TODO(mlippautz): It would be nice to DCHECK that we never call this
  // with {addr} pointing into large object space; however we currently
  // initialize LO allocations with a filler, see
  // LargeObjectSpace::AllocateLargePage.
  if (size == 0) return;
  CreateFillerObjectAtImpl(this, addr, size, clear_memory_mode);
  if (!V8_ENABLE_THIRD_PARTY_HEAP_BOOL) {
    if (clear_slots_mode == ClearRecordedSlots::kYes) {
      ClearRecordedSlotRange(addr, addr + size);
    } else if (verify_no_slots_recorded == VerifyNoSlotsRecorded::kYes) {
      VerifyNoNeedToClearSlots(addr, addr + size);
    }
  }
}

bool Heap::CanMoveObjectStart(HeapObject object) {
  if (!v8_flags.move_object_start) return false;

  // Sampling heap profiler may have a reference to the object.
  if (isolate()->heap_profiler()->is_sampling_allocations()) return false;

  if (IsLargeObject(object)) return false;

  // Compilation jobs may have references to the object.
  if (isolate()->concurrent_recompilation_enabled() &&
      isolate()->optimizing_compile_dispatcher()->HasJobs()) {
    return false;
  }

  // Concurrent marking does not support moving object starts without snapshot
  // protocol.
  //
  // TODO(v8:13726): This can be improved via concurrently reading the contents
  // in the marker at the cost of some complexity.
  if (incremental_marking()->IsMarking()) return false;

  // Concurrent sweeper does not support moving object starts. It assumes that
  // markbits (black regions) and object starts are matching up.
  if (!Page::FromHeapObject(object)->SweepingDone()) return false;

  return true;
}

bool Heap::IsImmovable(HeapObject object) {
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL)
    return third_party_heap::Heap::IsImmovable(object);

  BasicMemoryChunk* chunk = BasicMemoryChunk::FromHeapObject(object);
  return chunk->NeverEvacuate() || IsLargeObject(object);
}

bool Heap::IsLargeObject(HeapObject object) {
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL)
    return third_party_heap::Heap::InLargeObjectSpace(object.address()) ||
           third_party_heap::Heap::InSpace(object.address(), CODE_LO_SPACE);
  return BasicMemoryChunk::FromHeapObject(object)->IsLargePage();
}

#ifdef ENABLE_SLOW_DCHECKS
namespace {

class LeftTrimmerVerifierRootVisitor : public RootVisitor {
 public:
  explicit LeftTrimmerVerifierRootVisitor(FixedArrayBase to_check)
      : to_check_(to_check) {}

  LeftTrimmerVerifierRootVisitor(const LeftTrimmerVerifierRootVisitor&) =
      delete;
  LeftTrimmerVerifierRootVisitor& operator=(
      const LeftTrimmerVerifierRootVisitor&) = delete;

  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) override {
    for (FullObjectSlot p = start; p < end; ++p) {
      // V8_EXTERNAL_CODE_SPACE specific: we might be comparing
      // InstructionStream object with non-InstructionStream object here and it
      // might produce false positives because operator== for tagged values
      // compares only lower 32 bits when pointer compression is enabled.
      DCHECK_NE((*p).ptr(), to_check_.ptr());
    }
  }

  void VisitRootPointers(Root root, const char* description,
                         OffHeapObjectSlot start,
                         OffHeapObjectSlot end) override {
    DCHECK_EQ(root, Root::kStringTable);
    // We can skip iterating the string table, it doesn't point to any fixed
    // arrays.
  }

 private:
  FixedArrayBase to_check_;
};
}  // namespace
#endif  // ENABLE_SLOW_DCHECKS

namespace {
bool MayContainRecordedSlots(HeapObject object) {
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) return false;
  // New space object do not have recorded slots.
  if (BasicMemoryChunk::FromHeapObject(object)->InYoungGeneration())
    return false;
  // Allowlist objects that definitely do not have pointers.
  if (object.IsByteArray() || object.IsFixedDoubleArray()) return false;
  // Conservatively return true for other objects.
  return true;
}
}  // namespace

void Heap::OnMoveEvent(HeapObject source, HeapObject target,
                       int size_in_bytes) {
  HeapProfiler* heap_profiler = isolate_->heap_profiler();
  if (heap_profiler->is_tracking_object_moves()) {
    heap_profiler->ObjectMoveEvent(source.address(), target.address(),
                                   size_in_bytes, /*is_embedder_object=*/false);
  }
  for (auto& tracker : allocation_trackers_) {
    tracker->MoveEvent(source.address(), target.address(), size_in_bytes);
  }
  if (target.IsSharedFunctionInfo(isolate_)) {
    LOG_CODE_EVENT(isolate_, SharedFunctionInfoMoveEvent(source.address(),
                                                         target.address()));
  } else if (target.IsNativeContext(isolate_)) {
    if (isolate_->current_embedder_state() != nullptr) {
      isolate_->current_embedder_state()->OnMoveEvent(source.address(),
                                                      target.address());
    }
    PROFILE(isolate_,
            NativeContextMoveEvent(source.address(), target.address()));
  } else if (target.IsMap(isolate_)) {
    LOG(isolate_, MapMoveEvent(Map::cast(source), Map::cast(target)));
  }
}

FixedArrayBase Heap::LeftTrimFixedArray(FixedArrayBase object,
                                        int elements_to_trim) {
  if (elements_to_trim == 0) {
    // This simplifies reasoning in the rest of the function.
    return object;
  }
  CHECK(!object.is_null());
  DCHECK(CanMoveObjectStart(object));
  // Add custom visitor to concurrent marker if new left-trimmable type
  // is added.
  DCHECK(object.IsFixedArray() || object.IsFixedDoubleArray());
  const int element_size = object.IsFixedArray() ? kTaggedSize : kDoubleSize;
  const int bytes_to_trim = elements_to_trim * element_size;
  Map map = object.map();

  // For now this trick is only applied to fixed arrays which may be in new
  // space or old space. In a large object space the object's start must
  // coincide with chunk and thus the trick is just not applicable.
  DCHECK(!IsLargeObject(object));
  DCHECK(object.map() != ReadOnlyRoots(this).fixed_cow_array_map());

  static_assert(FixedArrayBase::kMapOffset == 0);
  static_assert(FixedArrayBase::kLengthOffset == kTaggedSize);
  static_assert(FixedArrayBase::kHeaderSize == 2 * kTaggedSize);

  const int len = object.length();
  DCHECK(elements_to_trim <= len);

  // Calculate location of new array start.
  Address old_start = object.address();
  Address new_start = old_start + bytes_to_trim;

  // Technically in new space this write might be omitted (except for
  // debug mode which iterates through the heap), but to play safer
  // we still do it.
  CreateFillerObjectAtRaw(
      old_start, bytes_to_trim, ClearFreedMemoryMode::kClearFreedMemory,
      MayContainRecordedSlots(object) ? ClearRecordedSlots::kYes
                                      : ClearRecordedSlots::kNo,
      VerifyNoSlotsRecorded::kYes);

  // Initialize header of the trimmed array. Since left trimming is only
  // performed on pages which are not concurrently swept creating a filler
  // object does not require synchronization.
  RELAXED_WRITE_FIELD(object, bytes_to_trim,
                      Object(MapWord::FromMap(map).ptr()));
  RELAXED_WRITE_FIELD(object, bytes_to_trim + kTaggedSize,
                      Smi::FromInt(len - elements_to_trim));

  FixedArrayBase new_object =
      FixedArrayBase::cast(HeapObject::FromAddress(new_start));

  if (isolate()->log_object_relocation()) {
    // Notify the heap profiler of change in object layout.
    OnMoveEvent(object, new_object, new_object.Size());
  }

#ifdef ENABLE_SLOW_DCHECKS
  if (v8_flags.enable_slow_asserts) {
    // Make sure the stack or other roots (e.g., Handles) don't contain pointers
    // to the original FixedArray (which is now the filler object).
    base::Optional<IsolateSafepointScope> safepoint_scope;

    {
      AllowGarbageCollection allow_gc;
      IgnoreLocalGCRequests ignore_gc_requests(this);
      safepoint_scope.emplace(this);
    }

    LeftTrimmerVerifierRootVisitor root_visitor(object);
    ReadOnlyRoots(this).Iterate(&root_visitor);

    IterateRoots(&root_visitor,
                 base::EnumSet<SkipRoot>{SkipRoot::kConservativeStack});
  }
#endif  // ENABLE_SLOW_DCHECKS

  return new_object;
}

void Heap::RightTrimFixedArray(FixedArrayBase object, int elements_to_trim) {
  const int len = object.length();
  DCHECK_LE(elements_to_trim, len);
  DCHECK_GE(elements_to_trim, 0);

  int bytes_to_trim;
  if (object.IsByteArray()) {
    int new_size = ByteArray::SizeFor(len - elements_to_trim);
    bytes_to_trim = ByteArray::SizeFor(len) - new_size;
    DCHECK_GE(bytes_to_trim, 0);
  } else if (object.IsFixedArray()) {
    CHECK_NE(elements_to_trim, len);
    bytes_to_trim = elements_to_trim * kTaggedSize;
  } else {
    DCHECK(object.IsFixedDoubleArray());
    CHECK_NE(elements_to_trim, len);
    bytes_to_trim = elements_to_trim * kDoubleSize;
  }

  CreateFillerForArray<FixedArrayBase>(object, elements_to_trim, bytes_to_trim);
}

void Heap::RightTrimWeakFixedArray(WeakFixedArray object,
                                   int elements_to_trim) {
  // This function is safe to use only at the end of the mark compact
  // collection: When marking, we record the weak slots, and shrinking
  // invalidates them.
  DCHECK_EQ(gc_state(), MARK_COMPACT);
  CreateFillerForArray<WeakFixedArray>(object, elements_to_trim,
                                       elements_to_trim * kTaggedSize);
}

template <typename T>
void Heap::CreateFillerForArray(T object, int elements_to_trim,
                                int bytes_to_trim) {
  DCHECK(object.IsFixedArrayBase() || object.IsByteArray() ||
         object.IsWeakFixedArray());

  // For now this trick is only applied to objects in new and paged space.
  DCHECK(object.map() != ReadOnlyRoots(this).fixed_cow_array_map());

  if (bytes_to_trim == 0) {
    DCHECK_EQ(elements_to_trim, 0);
    // No need to create filler and update live bytes counters.
    return;
  }

  // Calculate location of new array end.
  int old_size = object.Size();
  Address old_end = object.address() + old_size;
  Address new_end = old_end - bytes_to_trim;

  bool clear_slots = MayContainRecordedSlots(object);

  // Technically in new space this write might be omitted (except for
  // debug mode which iterates through the heap), but to play safer
  // we still do it.
  // We do not create a filler for objects in a large object space.
  if (!IsLargeObject(object)) {
    NotifyObjectSizeChange(
        object, old_size, old_size - bytes_to_trim,
        clear_slots ? ClearRecordedSlots::kYes : ClearRecordedSlots::kNo);
    HeapObject filler = HeapObject::FromAddress(new_end);
    // Clear the mark bits of the black area that belongs now to the filler.
    // This is an optimization. The sweeper will release black fillers anyway.
    if (incremental_marking()->black_allocation() &&
        marking_state()->IsMarked(filler)) {
      Page* page = Page::FromAddress(new_end);
      page->marking_bitmap()->ClearRange<AccessMode::ATOMIC>(
          MarkingBitmap::AddressToIndex(new_end),
          MarkingBitmap::LimitAddressToIndex(new_end + bytes_to_trim));
    }
  } else if (clear_slots) {
    // Large objects are not swept, so it is not necessary to clear the
    // recorded slot.
    MemsetTagged(ObjectSlot(new_end), Object(kClearedFreeMemoryValue),
                 (old_end - new_end) / kTaggedSize);
  }

  // Initialize header of the trimmed array. We are storing the new length
  // using release store after creating a filler for the left-over space to
  // avoid races with the sweeper thread.
  object.set_length(object.length() - elements_to_trim, kReleaseStore);

  // Notify the heap object allocation tracker of change in object layout. The
  // array may not be moved during GC, and size has to be adjusted nevertheless.
  for (auto& tracker : allocation_trackers_) {
    tracker->UpdateObjectSizeEvent(object.address(), object.Size());
  }
}

void Heap::MakeHeapIterable() {
  EnsureSweepingCompleted(SweepingForcedFinalizationMode::kV8Only);

  safepoint()->IterateLocalHeaps([](LocalHeap* local_heap) {
    local_heap->MakeLinearAllocationAreaIterable();
  });

  if (isolate()->is_shared_space_isolate()) {
    isolate()->global_safepoint()->IterateSharedSpaceAndClientIsolates(
        [](Isolate* client) {
          client->heap()->MakeSharedLinearAllocationAreasIterable();
        });
  }

  PagedSpaceIterator spaces(this);
  for (PagedSpace* space = spaces.Next(); space != nullptr;
       space = spaces.Next()) {
    space->MakeLinearAllocationAreaIterable();
  }

  if (shared_space_allocator_) {
    shared_space_allocator_->MakeLinearAllocationAreaIterable();
  }
  if (new_space()) new_space()->MakeLinearAllocationAreaIterable();
}

void Heap::FreeLinearAllocationAreas() {
  FreeMainThreadLinearAllocationAreas();

  safepoint()->IterateLocalHeaps(
      [](LocalHeap* local_heap) { local_heap->FreeLinearAllocationArea(); });

  if (isolate()->is_shared_space_isolate()) {
    isolate()->global_safepoint()->IterateSharedSpaceAndClientIsolates(
        [](Isolate* client) {
          client->heap()->FreeSharedLinearAllocationAreas();
        });
  }
}

void Heap::FreeMainThreadLinearAllocationAreas() {
  PagedSpaceIterator spaces(this);
  for (PagedSpace* space = spaces.Next(); space != nullptr;
       space = spaces.Next()) {
    base::MutexGuard guard(space->mutex());
    space->FreeLinearAllocationArea();
  }

  if (shared_space_allocator_) {
    shared_space_allocator_->FreeLinearAllocationArea();
  }
  if (new_space()) new_space()->FreeLinearAllocationArea();
}

void Heap::FreeSharedLinearAllocationAreas() {
  if (!isolate()->has_shared_space()) return;
  safepoint()->IterateLocalHeaps([](LocalHeap* local_heap) {
    local_heap->FreeSharedLinearAllocationArea();
  });
  FreeMainThreadSharedLinearAllocationAreas();
}

void Heap::FreeMainThreadSharedLinearAllocationAreas() {
  if (!isolate()->has_shared_space()) return;
  shared_space_allocator_->FreeLinearAllocationArea();
  main_thread_local_heap()->FreeSharedLinearAllocationArea();
}

void Heap::MakeSharedLinearAllocationAreasIterable() {
  if (!isolate()->has_shared_space()) return;

  safepoint()->IterateLocalHeaps([](LocalHeap* local_heap) {
    local_heap->MakeSharedLinearAllocationAreaIterable();
  });

  if (shared_space_allocator_) {
    shared_space_allocator_->MakeLinearAllocationAreaIterable();
  }

  main_thread_local_heap()->MakeSharedLinearAllocationAreaIterable();
}

void Heap::MarkSharedLinearAllocationAreasBlack() {
  if (shared_space_allocator_) {
    shared_space_allocator_->MarkLinearAllocationAreaBlack();
  }
  safepoint()->IterateLocalHeaps([](LocalHeap* local_heap) {
    local_heap->MarkSharedLinearAllocationAreaBlack();
  });
  main_thread_local_heap()->MarkSharedLinearAllocationAreaBlack();
}

void Heap::UnmarkSharedLinearAllocationAreas() {
  if (shared_space_allocator_) {
    shared_space_allocator_->UnmarkLinearAllocationArea();
  }
  safepoint()->IterateLocalHeaps([](LocalHeap* local_heap) {
    local_heap->UnmarkSharedLinearAllocationArea();
  });
  main_thread_local_heap()->UnmarkSharedLinearAllocationArea();
}

namespace {

double ComputeMutatorUtilizationImpl(double mutator_speed, double gc_speed) {
  constexpr double kMinMutatorUtilization = 0.0;
  constexpr double kConservativeGcSpeedInBytesPerMillisecond = 200000;
  if (mutator_speed == 0) return kMinMutatorUtilization;
  if (gc_speed == 0) gc_speed = kConservativeGcSpeedInBytesPerMillisecond;
  // Derivation:
  // mutator_utilization = mutator_time / (mutator_time + gc_time)
  // mutator_time = 1 / mutator_speed
  // gc_time = 1 / gc_speed
  // mutator_utilization = (1 / mutator_speed) /
  //                       (1 / mutator_speed + 1 / gc_speed)
  // mutator_utilization = gc_speed / (mutator_speed + gc_speed)
  return gc_speed / (mutator_speed + gc_speed);
}

}  // namespace

double Heap::ComputeMutatorUtilization(const char* tag, double mutator_speed,
                                       double gc_speed) {
  double result = ComputeMutatorUtilizationImpl(mutator_speed, gc_speed);
  if (v8_flags.trace_mutator_utilization) {
    isolate()->PrintWithTimestamp(
        "%s mutator utilization = %.3f ("
        "mutator_speed=%.f, gc_speed=%.f)\n",
        tag, result, mutator_speed, gc_speed);
  }
  return result;
}

bool Heap::HasLowYoungGenerationAllocationRate() {
  double mu = ComputeMutatorUtilization(
      "Young generation",
      tracer()->NewSpaceAllocationThroughputInBytesPerMillisecond(),
      tracer()->ScavengeSpeedInBytesPerMillisecond(kForSurvivedObjects));
  constexpr double kHighMutatorUtilization = 0.993;
  return mu > kHighMutatorUtilization;
}

bool Heap::HasLowOldGenerationAllocationRate() {
  double mu = ComputeMutatorUtilization(
      "Old generation",
      tracer()->OldGenerationAllocationThroughputInBytesPerMillisecond(),
      tracer()->CombinedMarkCompactSpeedInBytesPerMillisecond());
  const double kHighMutatorUtilization = 0.993;
  return mu > kHighMutatorUtilization;
}

bool Heap::HasLowEmbedderAllocationRate() {
  double mu = ComputeMutatorUtilization(
      "Embedder",
      tracer()->CurrentEmbedderAllocationThroughputInBytesPerMillisecond(),
      tracer()->EmbedderSpeedInBytesPerMillisecond());
  const double kHighMutatorUtilization = 0.993;
  return mu > kHighMutatorUtilization;
}

bool Heap::HasLowAllocationRate() {
  return HasLowYoungGenerationAllocationRate() &&
         HasLowOldGenerationAllocationRate() && HasLowEmbedderAllocationRate();
}

bool Heap::IsIneffectiveMarkCompact(size_t old_generation_size,
                                    double mutator_utilization) {
  const double kHighHeapPercentage = 0.8;
  const double kLowMutatorUtilization = 0.4;
  return old_generation_size >=
             kHighHeapPercentage * max_old_generation_size() &&
         mutator_utilization < kLowMutatorUtilization;
}

void Heap::CheckIneffectiveMarkCompact(size_t old_generation_size,
                                       double mutator_utilization) {
  const int kMaxConsecutiveIneffectiveMarkCompacts = 4;
  if (!v8_flags.detect_ineffective_gcs_near_heap_limit) return;
  if (!IsIneffectiveMarkCompact(old_generation_size, mutator_utilization)) {
    consecutive_ineffective_mark_compacts_ = 0;
    return;
  }
  ++consecutive_ineffective_mark_compacts_;
  if (consecutive_ineffective_mark_compacts_ ==
      kMaxConsecutiveIneffectiveMarkCompacts) {
    if (InvokeNearHeapLimitCallback()) {
      // The callback increased the heap limit.
      consecutive_ineffective_mark_compacts_ = 0;
      return;
    }
    FatalProcessOutOfMemory("Ineffective mark-compacts near heap limit");
  }
}

bool Heap::HasHighFragmentation() {
  const size_t used = OldGenerationSizeOfObjects();
  const size_t committed = CommittedOldGenerationMemory();

  // Background thread allocation could result in committed memory being less
  // than used memory in some situations.
  if (committed < used) return false;

  constexpr size_t kSlack = 16 * MB;

  // Fragmentation is high if committed > 2 * used + kSlack.
  // Rewrite the expression to avoid overflow.
  return committed - used > used + kSlack;
}

bool Heap::ShouldOptimizeForMemoryUsage() {
  const size_t kOldGenerationSlack = max_old_generation_size() / 8;
  return v8_flags.optimize_for_size || isolate()->IsIsolateInBackground() ||
         isolate()->IsMemorySavingsModeActive() || HighMemoryPressure() ||
         !CanExpandOldGeneration(kOldGenerationSlack);
}

void Heap::ActivateMemoryReducerIfNeeded() {
  // Activate memory reducer when switching to background if
  // - there was no mark compact since the start.
  // - the committed memory can be potentially reduced.
  // 2 pages for the old, code, and map space + 1 page for new space.
  if (memory_reducer_ == nullptr) return;
  const int kMinCommittedMemory = 7 * Page::kPageSize;
  if (ms_count_ == 0 && CommittedMemory() > kMinCommittedMemory &&
      isolate()->IsIsolateInBackground()) {
    memory_reducer_->NotifyPossibleGarbage();
  }
}

Heap::ResizeNewSpaceMode Heap::ShouldResizeNewSpace() {
  if (ShouldReduceMemory()) {
    return (v8_flags.predictable) ? ResizeNewSpaceMode::kNone
                                  : ResizeNewSpaceMode::kShrink;
  }

  static const size_t kLowAllocationThroughput = 1000;
  const double allocation_throughput =
      tracer_->CurrentAllocationThroughputInBytesPerMillisecond();
  const bool should_shrink = !v8_flags.predictable &&
                             (allocation_throughput != 0) &&
                             (allocation_throughput < kLowAllocationThroughput);

  const bool should_grow =
      (new_space_->TotalCapacity() < new_space_->MaximumCapacity()) &&
      (survived_since_last_expansion_ > new_space_->TotalCapacity());

  if (should_grow) survived_since_last_expansion_ = 0;

  if (should_grow == should_shrink) return ResizeNewSpaceMode::kNone;
  return should_grow ? ResizeNewSpaceMode::kGrow : ResizeNewSpaceMode::kShrink;
}

void Heap::ExpandNewSpaceSize() {
  // Grow the size of new space if there is room to grow, and enough data
  // has survived scavenge since the last expansion.
  new_space_->Grow();
  new_lo_space()->SetCapacity(new_space()->TotalCapacity());
}

void Heap::ReduceNewSpaceSize() {
  // MinorMS shrinks new space as part of sweeping.
  if (!v8_flags.minor_ms) {
    SemiSpaceNewSpace::From(new_space())->Shrink();
  } else {
    paged_new_space()->FinishShrinking();
  }
  new_lo_space_->SetCapacity(new_space()->TotalCapacity());
}

size_t Heap::NewSpaceSize() { return new_space() ? new_space()->Size() : 0; }

size_t Heap::NewSpaceCapacity() const {
  return new_space() ? new_space()->Capacity() : 0;
}

size_t Heap::NewSpaceTargetCapacity() const {
  return new_space() ? new_space()->TotalCapacity() : 0;
}

void Heap::FinalizeIncrementalMarkingAtomically(
    GarbageCollectionReason gc_reason) {
  DCHECK(!incremental_marking()->IsStopped());
  CollectAllGarbage(current_gc_flags_, gc_reason, current_gc_callback_flags_);
}

void Heap::InvokeIncrementalMarkingPrologueCallbacks() {
  AllowGarbageCollection allow_allocation;
  VMState<EXTERNAL> state(isolate_);
  CallGCPrologueCallbacks(kGCTypeIncrementalMarking, kNoGCCallbackFlags,
                          GCTracer::Scope::MC_INCREMENTAL_EXTERNAL_PROLOGUE);
}

void Heap::InvokeIncrementalMarkingEpilogueCallbacks() {
  AllowGarbageCollection allow_allocation;
  VMState<EXTERNAL> state(isolate_);
  CallGCEpilogueCallbacks(kGCTypeIncrementalMarking, kNoGCCallbackFlags,
                          GCTracer::Scope::MC_INCREMENTAL_EXTERNAL_EPILOGUE);
}

namespace {
thread_local Address pending_layout_change_object_address = kNullAddress;
}  // namespace

void Heap::NotifyObjectLayoutChange(
    HeapObject object, const DisallowGarbageCollection&,
    InvalidateRecordedSlots invalidate_recorded_slots, int new_size) {
  if (invalidate_recorded_slots == InvalidateRecordedSlots::kYes) {
    const bool may_contain_recorded_slots = MayContainRecordedSlots(object);
    MemoryChunk* const chunk = MemoryChunk::FromHeapObject(object);
    // Do not remove the recorded slot in the map word as this one can never be
    // invalidated.
    const Address clear_range_start = object.address() + kTaggedSize;
    // Only slots in the range of the new object size (which is potentially
    // smaller than the original one) can be invalidated. Clearing of recorded
    // slots up to the original object size even conflicts with concurrent
    // sweeping.
    const Address clear_range_end = object.address() + new_size;

    if (incremental_marking()->IsMarking()) {
      ExclusiveObjectLock::Lock(object);
      DCHECK_EQ(pending_layout_change_object_address, kNullAddress);
      pending_layout_change_object_address = object.address();
      if (may_contain_recorded_slots && incremental_marking()->IsCompacting()) {
        RememberedSet<OLD_TO_OLD>::RemoveRange(
            chunk, clear_range_start, clear_range_end,
            SlotSet::EmptyBucketMode::KEEP_EMPTY_BUCKETS);
      }
    }

    if (may_contain_recorded_slots) {
      RememberedSet<OLD_TO_NEW>::RemoveRange(
          chunk, clear_range_start, clear_range_end,
          SlotSet::EmptyBucketMode::KEEP_EMPTY_BUCKETS);
      RememberedSet<OLD_TO_NEW_BACKGROUND>::RemoveRange(
          chunk, clear_range_start, clear_range_end,
          SlotSet::EmptyBucketMode::KEEP_EMPTY_BUCKETS);
      RememberedSet<OLD_TO_SHARED>::RemoveRange(
          chunk, clear_range_start, clear_range_end,
          SlotSet::EmptyBucketMode::KEEP_EMPTY_BUCKETS);
    }
  }
#ifdef VERIFY_HEAP
  if (v8_flags.verify_heap) {
    HeapVerifier::SetPendingLayoutChangeObject(this, object);
  }
#endif
}

// static
void Heap::NotifyObjectLayoutChangeDone(HeapObject object) {
  if (pending_layout_change_object_address != kNullAddress) {
    DCHECK_EQ(pending_layout_change_object_address, object.address());
    ExclusiveObjectLock::Unlock(object);
    pending_layout_change_object_address = kNullAddress;
  }
}

void Heap::NotifyObjectSizeChange(HeapObject object, int old_size, int new_size,
                                  ClearRecordedSlots clear_recorded_slots) {
  old_size = ALIGN_TO_ALLOCATION_ALIGNMENT(old_size);
  new_size = ALIGN_TO_ALLOCATION_ALIGNMENT(new_size);
  DCHECK_LE(new_size, old_size);
  DCHECK(!IsLargeObject(object));
  if (new_size == old_size) return;

  const bool is_main_thread = LocalHeap::Current() == nullptr;

  DCHECK_IMPLIES(!is_main_thread,
                 clear_recorded_slots == ClearRecordedSlots::kNo);

  const auto verify_no_slots_recorded =
      is_main_thread ? VerifyNoSlotsRecorded::kYes : VerifyNoSlotsRecorded::kNo;

  const auto clear_memory_mode = ClearFreedMemoryMode::kDontClearFreedMemory;

  const Address filler = object.address() + new_size;
  const int filler_size = old_size - new_size;
  CreateFillerObjectAtRaw(filler, filler_size, clear_memory_mode,
                          clear_recorded_slots, verify_no_slots_recorded);
}

GCIdleTimeHeapState Heap::ComputeHeapState() {
  GCIdleTimeHeapState heap_state;
  heap_state.size_of_objects = static_cast<size_t>(SizeOfObjects());
  heap_state.incremental_marking_stopped = incremental_marking()->IsStopped();
  return heap_state;
}

bool Heap::PerformIdleTimeAction(GCIdleTimeAction action,
                                 GCIdleTimeHeapState heap_state,
                                 double deadline_in_ms) {
  bool result = false;
  switch (action) {
    case GCIdleTimeAction::kDone:
      result = true;
      break;
    case GCIdleTimeAction::kIncrementalStep: {
      incremental_marking()->AdvanceAndFinalizeIfComplete();
      result = incremental_marking()->IsStopped();
      break;
    }
  }

  return result;
}

void Heap::IdleNotificationEpilogue(GCIdleTimeAction action,
                                    GCIdleTimeHeapState heap_state,
                                    double start_ms, double deadline_in_ms) {
  const double idle_time_in_ms = deadline_in_ms - start_ms;
  const double deadline_difference =
      deadline_in_ms - MonotonicallyIncreasingTimeInMs();

  if (v8_flags.trace_idle_notification) {
    isolate_->PrintWithTimestamp(
        "Idle notification: requested idle time %.2f ms, used idle time %.2f "
        "ms, deadline usage %.2f ms [",
        idle_time_in_ms, idle_time_in_ms - deadline_difference,
        deadline_difference);
    switch (action) {
      case GCIdleTimeAction::kDone:
        PrintF("done");
        break;
      case GCIdleTimeAction::kIncrementalStep:
        PrintF("incremental step");
        break;
    }
    PrintF("]");
    if (v8_flags.trace_idle_notification_verbose) {
      PrintF("[");
      heap_state.Print();
      PrintF("]");
    }
    PrintF("\n");
  }
}

double Heap::MonotonicallyIncreasingTimeInMs() const {
  return V8::GetCurrentPlatform()->MonotonicallyIncreasingTime() *
         static_cast<double>(base::Time::kMillisecondsPerSecond);
}

#if DEBUG
void Heap::VerifyNewSpaceTop() {
  if (!new_space()) return;
  new_space()->VerifyTop();
}
#endif  // DEBUG

bool Heap::IdleNotification(int idle_time_in_ms) {
  return IdleNotification(
      V8::GetCurrentPlatform()->MonotonicallyIncreasingTime() +
      (static_cast<double>(idle_time_in_ms) /
       static_cast<double>(base::Time::kMillisecondsPerSecond)));
}

bool Heap::IdleNotification(double deadline_in_seconds) {
  CHECK(HasBeenSetUp());
  double deadline_in_ms =
      deadline_in_seconds *
      static_cast<double>(base::Time::kMillisecondsPerSecond);
  NestedTimedHistogramScope idle_notification_scope(
      isolate_->counters()->gc_idle_notification());
  TRACE_EVENT0("v8", "V8.GCIdleNotification");
  double start_ms = MonotonicallyIncreasingTimeInMs();
  double idle_time_in_ms = deadline_in_ms - start_ms;

  tracer()->SampleAllocation(start_ms, NewSpaceAllocationCounter(),
                             OldGenerationAllocationCounter(),
                             EmbedderAllocationCounter());

  GCIdleTimeHeapState heap_state = ComputeHeapState();
  GCIdleTimeAction action =
      gc_idle_time_handler_->Compute(idle_time_in_ms, heap_state);
  bool result = PerformIdleTimeAction(action, heap_state, deadline_in_ms);
  IdleNotificationEpilogue(action, heap_state, start_ms, deadline_in_ms);
  return result;
}

class MemoryPressureInterruptTask : public CancelableTask {
 public:
  explicit MemoryPressureInterruptTask(Heap* heap)
      : CancelableTask(heap->isolate()), heap_(heap) {}

  ~MemoryPressureInterruptTask() override = default;
  MemoryPressureInterruptTask(const MemoryPressureInterruptTask&) = delete;
  MemoryPressureInterruptTask& operator=(const MemoryPressureInterruptTask&) =
      delete;

 private:
  // v8::internal::CancelableTask overrides.
  void RunInternal() override { heap_->CheckMemoryPressure(); }

  Heap* heap_;
};

void Heap::CheckMemoryPressure() {
  if (HighMemoryPressure()) {
    // The optimizing compiler may be unnecessarily holding on to memory.
    isolate()->AbortConcurrentOptimization(BlockingBehavior::kDontBlock);
  }
  // Reset the memory pressure level to avoid recursive GCs triggered by
  // CheckMemoryPressure from AdjustAmountOfExternalMemory called by
  // the finalizers.
  MemoryPressureLevel memory_pressure_level = memory_pressure_level_.exchange(
      MemoryPressureLevel::kNone, std::memory_order_relaxed);
  if (memory_pressure_level == MemoryPressureLevel::kCritical) {
    TRACE_EVENT0("devtools.timeline,v8", "V8.CheckMemoryPressure");
    CollectGarbageOnMemoryPressure();
  } else if (memory_pressure_level == MemoryPressureLevel::kModerate) {
    if (v8_flags.incremental_marking && incremental_marking()->IsStopped()) {
      TRACE_EVENT0("devtools.timeline,v8", "V8.CheckMemoryPressure");
      StartIncrementalMarking(GCFlag::kReduceMemoryFootprint,
                              GarbageCollectionReason::kMemoryPressure);
    }
  }
}

void Heap::CollectGarbageOnMemoryPressure() {
  const int kGarbageThresholdInBytes = 8 * MB;
  const double kGarbageThresholdAsFractionOfTotalMemory = 0.1;
  // This constant is the maximum response time in RAIL performance model.
  const double kMaxMemoryPressurePauseMs = 100;

  double start = MonotonicallyIncreasingTimeInMs();
  CollectAllGarbage(GCFlag::kReduceMemoryFootprint,
                    GarbageCollectionReason::kMemoryPressure,
                    kGCCallbackFlagCollectAllAvailableGarbage);
  EagerlyFreeExternalMemory();
  double end = MonotonicallyIncreasingTimeInMs();

  // Estimate how much memory we can free.
  int64_t potential_garbage =
      (CommittedMemory() - SizeOfObjects()) + external_memory_.total();
  // If we can potentially free large amount of memory, then start GC right
  // away instead of waiting for memory reducer.
  if (potential_garbage >= kGarbageThresholdInBytes &&
      potential_garbage >=
          CommittedMemory() * kGarbageThresholdAsFractionOfTotalMemory) {
    // If we spent less than half of the time budget, then perform full GC
    // Otherwise, start incremental marking.
    if (end - start < kMaxMemoryPressurePauseMs / 2) {
      CollectAllGarbage(GCFlag::kReduceMemoryFootprint,
                        GarbageCollectionReason::kMemoryPressure,
                        kGCCallbackFlagCollectAllAvailableGarbage);
    } else {
      if (v8_flags.incremental_marking && incremental_marking()->IsStopped()) {
        StartIncrementalMarking(GCFlag::kReduceMemoryFootprint,
                                GarbageCollectionReason::kMemoryPressure);
      }
    }
  }
}

void Heap::MemoryPressureNotification(MemoryPressureLevel level,
                                      bool is_isolate_locked) {
  TRACE_EVENT1("devtools.timeline,v8", "V8.MemoryPressureNotification", "level",
               static_cast<int>(level));
  MemoryPressureLevel previous =
      memory_pressure_level_.exchange(level, std::memory_order_relaxed);
  if ((previous != MemoryPressureLevel::kCritical &&
       level == MemoryPressureLevel::kCritical) ||
      (previous == MemoryPressureLevel::kNone &&
       level == MemoryPressureLevel::kModerate)) {
    if (is_isolate_locked) {
      CheckMemoryPressure();
    } else {
      ExecutionAccess access(isolate());
      isolate()->stack_guard()->RequestGC();
      auto taskrunner = V8::GetCurrentPlatform()->GetForegroundTaskRunner(
          reinterpret_cast<v8::Isolate*>(isolate()));
      taskrunner->PostTask(std::make_unique<MemoryPressureInterruptTask>(this));
    }
  }
}

void Heap::EagerlyFreeExternalMemory() {
  CompleteArrayBufferSweeping(this);
  memory_allocator()->unmapper()->EnsureUnmappingCompleted();
}

void Heap::AddNearHeapLimitCallback(v8::NearHeapLimitCallback callback,
                                    void* data) {
  const size_t kMaxCallbacks = 100;
  CHECK_LT(near_heap_limit_callbacks_.size(), kMaxCallbacks);
  for (auto callback_data : near_heap_limit_callbacks_) {
    CHECK_NE(callback_data.first, callback);
  }
  near_heap_limit_callbacks_.push_back(std::make_pair(callback, data));
}

void Heap::RemoveNearHeapLimitCallback(v8::NearHeapLimitCallback callback,
                                       size_t heap_limit) {
  for (size_t i = 0; i < near_heap_limit_callbacks_.size(); i++) {
    if (near_heap_limit_callbacks_[i].first == callback) {
      near_heap_limit_callbacks_.erase(near_heap_limit_callbacks_.begin() + i);
      if (heap_limit) {
        RestoreHeapLimit(heap_limit);
      }
      return;
    }
  }
  UNREACHABLE();
}

void Heap::AppendArrayBufferExtension(JSArrayBuffer object,
                                      ArrayBufferExtension* extension) {
  // ArrayBufferSweeper is managing all counters and updating Heap counters.
  array_buffer_sweeper_->Append(object, extension);
}

void Heap::DetachArrayBufferExtension(JSArrayBuffer object,
                                      ArrayBufferExtension* extension) {
  // ArrayBufferSweeper is managing all counters and updating Heap counters.
  return array_buffer_sweeper_->Detach(object, extension);
}

void Heap::AutomaticallyRestoreInitialHeapLimit(double threshold_percent) {
  initial_max_old_generation_size_threshold_ =
      initial_max_old_generation_size_ * threshold_percent;
}

bool Heap::InvokeNearHeapLimitCallback() {
  if (near_heap_limit_callbacks_.size() > 0) {
    AllowGarbageCollection allow_gc;
    TRACE_GC(tracer(), GCTracer::Scope::HEAP_EXTERNAL_NEAR_HEAP_LIMIT);
    VMState<EXTERNAL> callback_state(isolate());
    HandleScope scope(isolate());
    v8::NearHeapLimitCallback callback =
        near_heap_limit_callbacks_.back().first;
    void* data = near_heap_limit_callbacks_.back().second;
    size_t heap_limit = callback(data, max_old_generation_size(),
                                 initial_max_old_generation_size_);
    if (heap_limit > max_old_generation_size()) {
      SetOldGenerationAndGlobalMaximumSize(
          std::min(heap_limit, AllocatorLimitOnMaxOldGenerationSize()));
      return true;
    }
  }
  return false;
}

bool Heap::MeasureMemory(std::unique_ptr<v8::MeasureMemoryDelegate> delegate,
                         v8::MeasureMemoryExecution execution) {
  HandleScope handle_scope(isolate());
  std::vector<Handle<NativeContext>> contexts = FindAllNativeContexts();
  std::vector<Handle<NativeContext>> to_measure;
  for (auto& current : contexts) {
    if (delegate->ShouldMeasure(v8::Utils::ToLocal(current))) {
      to_measure.push_back(current);
    }
  }
  return memory_measurement_->EnqueueRequest(std::move(delegate), execution,
                                             to_measure);
}

std::unique_ptr<v8::MeasureMemoryDelegate> Heap::MeasureMemoryDelegate(
    Handle<NativeContext> context, Handle<JSPromise> promise,
    v8::MeasureMemoryMode mode) {
  return i::MemoryMeasurement::DefaultDelegate(isolate_, context, promise,
                                               mode);
}

void Heap::CollectCodeStatistics() {
  TRACE_EVENT0("v8", "Heap::CollectCodeStatistics");
  IgnoreLocalGCRequests ignore_gc_requests(this);
  IsolateSafepointScope safepoint_scope(this);
  MakeHeapIterable();
  CodeStatistics::ResetCodeAndMetadataStatistics(isolate());
  // We do not look for code in new space, or map space.  If code
  // somehow ends up in those spaces, we would miss it here.
  CodeStatistics::CollectCodeStatistics(code_space_, isolate());
  CodeStatistics::CollectCodeStatistics(old_space_, isolate());
  CodeStatistics::CollectCodeStatistics(code_lo_space_, isolate());
}

#ifdef DEBUG

void Heap::Print() {
  if (!HasBeenSetUp()) return;
  isolate()->PrintStack(stdout);

  for (SpaceIterator it(this); it.HasNext();) {
    it.Next()->Print();
  }
}

void Heap::ReportCodeStatistics(const char* title) {
  PrintF("###### Code Stats (%s) ######\n", title);
  CollectCodeStatistics();
  CodeStatistics::ReportCodeStatistics(isolate());
}

#endif  // DEBUG

bool Heap::Contains(HeapObject value) const {
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) {
    return true;
  }
  if (ReadOnlyHeap::Contains(value)) {
    return false;
  }
  if (memory_allocator()->IsOutsideAllocatedSpace(value.address())) {
    return false;
  }

  if (!HasBeenSetUp()) return false;

  return (new_space_ && new_space_->Contains(value)) ||
         old_space_->Contains(value) || code_space_->Contains(value) ||
         (shared_space_ && shared_space_->Contains(value)) ||
         lo_space_->Contains(value) || code_lo_space_->Contains(value) ||
         (new_lo_space_ && new_lo_space_->Contains(value)) ||
         (shared_lo_space_ && shared_lo_space_->Contains(value));
}

bool Heap::ContainsCode(HeapObject value) const {
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) {
    return true;
  }
  // TODO(v8:11880): support external code space.
  if (memory_allocator()->IsOutsideAllocatedSpace(value.address())) {
    return false;
  }
  return HasBeenSetUp() &&
         (code_space_->Contains(value) || code_lo_space_->Contains(value));
}

bool Heap::SharedHeapContains(HeapObject value) const {
  if (shared_allocation_space_) {
    if (shared_allocation_space_->Contains(value)) return true;
    if (shared_lo_allocation_space_->Contains(value)) return true;
  }

  return false;
}

bool Heap::MustBeInSharedOldSpace(HeapObject value) {
  if (isolate()->OwnsStringTables()) return false;
  if (ReadOnlyHeap::Contains(value)) return false;
  if (Heap::InYoungGeneration(value)) return false;
  if (value.IsExternalString()) return false;
  if (value.IsInternalizedString()) return true;
  return false;
}

bool Heap::InSpace(HeapObject value, AllocationSpace space) const {
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL)
    return third_party_heap::Heap::InSpace(value.address(), space);
  if (memory_allocator()->IsOutsideAllocatedSpace(value.address())) {
    return false;
  }
  if (!HasBeenSetUp()) return false;

  switch (space) {
    case NEW_SPACE:
      return new_space_->Contains(value);
    case OLD_SPACE:
      return old_space_->Contains(value);
    case CODE_SPACE:
      return code_space_->Contains(value);
    case SHARED_SPACE:
      return shared_space_->Contains(value);
    case LO_SPACE:
      return lo_space_->Contains(value);
    case CODE_LO_SPACE:
      return code_lo_space_->Contains(value);
    case NEW_LO_SPACE:
      return new_lo_space_->Contains(value);
    case SHARED_LO_SPACE:
      return shared_lo_space_->Contains(value);
    case RO_SPACE:
      return ReadOnlyHeap::Contains(value);
  }
  UNREACHABLE();
}

bool Heap::InSpaceSlow(Address addr, AllocationSpace space) const {
  if (memory_allocator()->IsOutsideAllocatedSpace(addr)) {
    return false;
  }
  if (!HasBeenSetUp()) return false;

  switch (space) {
    case NEW_SPACE:
      return new_space_->ContainsSlow(addr);
    case OLD_SPACE:
      return old_space_->ContainsSlow(addr);
    case CODE_SPACE:
      return code_space_->ContainsSlow(addr);
    case SHARED_SPACE:
      return shared_space_->ContainsSlow(addr);
    case LO_SPACE:
      return lo_space_->ContainsSlow(addr);
    case CODE_LO_SPACE:
      return code_lo_space_->ContainsSlow(addr);
    case NEW_LO_SPACE:
      return new_lo_space_->ContainsSlow(addr);
    case SHARED_LO_SPACE:
      return shared_lo_space_->ContainsSlow(addr);
    case RO_SPACE:
      return read_only_space_->ContainsSlow(addr);
  }
  UNREACHABLE();
}

bool Heap::IsValidAllocationSpace(AllocationSpace space) {
  switch (space) {
    case NEW_SPACE:
    case OLD_SPACE:
    case CODE_SPACE:
    case SHARED_SPACE:
    case LO_SPACE:
    case NEW_LO_SPACE:
    case CODE_LO_SPACE:
    case SHARED_LO_SPACE:
    case RO_SPACE:
      return true;
    default:
      return false;
  }
}

#ifdef DEBUG
void Heap::VerifyCountersAfterSweeping() {
  MakeHeapIterable();
  PagedSpaceIterator spaces(this);
  for (PagedSpace* space = spaces.Next(); space != nullptr;
       space = spaces.Next()) {
    space->VerifyCountersAfterSweeping(this);
  }
}

void Heap::VerifyCountersBeforeConcurrentSweeping(GarbageCollector collector) {
  if (v8_flags.minor_ms && new_space()) {
    PagedSpaceBase* space = paged_new_space()->paged_space();
    space->RefillFreeList();
    space->VerifyCountersBeforeConcurrentSweeping();
  }
  if (collector != GarbageCollector::MARK_COMPACTOR) return;
  PagedSpaceIterator spaces(this);
  for (PagedSpace* space = spaces.Next(); space != nullptr;
       space = spaces.Next()) {
    // We need to refine the counters on pages that are already swept and have
    // not been moved over to the actual space. Otherwise, the AccountingStats
    // are just an over approximation.
    space->RefillFreeList();
    space->VerifyCountersBeforeConcurrentSweeping();
  }
}

void Heap::VerifyCommittedPhysicalMemory() {
  PagedSpaceIterator spaces(this);
  for (PagedSpace* space = spaces.Next(); space != nullptr;
       space = spaces.Next()) {
    space->VerifyCommittedPhysicalMemory();
  }
  if (v8_flags.minor_ms && new_space()) {
    paged_new_space()->paged_space()->VerifyCommittedPhysicalMemory();
  }
}
#endif  // DEBUG

void Heap::IterateWeakRoots(RootVisitor* v, base::EnumSet<SkipRoot> options) {
  DCHECK(!options.contains(SkipRoot::kWeak));

  if (!options.contains(SkipRoot::kOldGeneration) &&
      !options.contains(SkipRoot::kUnserializable) &&
      isolate()->OwnsStringTables()) {
    // Do not visit for the following reasons.
    // - Serialization, since the string table is custom serialized.
    // - If we are skipping old generation, since all internalized strings
    //   are in old space.
    // - If the string table is shared and this is not the shared heap,
    //   since all internalized strings are in the shared heap.
    isolate()->string_table()->IterateElements(v);
  }
  v->Synchronize(VisitorSynchronization::kStringTable);
  if (!options.contains(SkipRoot::kExternalStringTable) &&
      !options.contains(SkipRoot::kUnserializable)) {
    // Scavenge collections have special processing for this.
    // Do not visit for serialization, since the external string table will
    // be populated from scratch upon deserialization.
    external_string_table_.IterateAll(v);
  }
  v->Synchronize(VisitorSynchronization::kExternalStringsTable);
}

void Heap::IterateSmiRoots(RootVisitor* v) {
  // Acquire execution access since we are going to read stack limit values.
  ExecutionAccess access(isolate());
  v->VisitRootPointers(Root::kSmiRootList, nullptr,
                       roots_table().smi_roots_begin(),
                       roots_table().smi_roots_end());
  v->Synchronize(VisitorSynchronization::kSmiRootList);
}

// We cannot avoid stale handles to left-trimmed objects, but can only make
// sure all handles still needed are updated. Filter out a stale pointer
// and clear the slot to allow post processing of handles (needed because
// the sweeper might actually free the underlying page).
class ClearStaleLeftTrimmedHandlesVisitor : public RootVisitor {
 public:
  explicit ClearStaleLeftTrimmedHandlesVisitor(Heap* heap)
      : heap_(heap)
#if V8_COMPRESS_POINTERS
        ,
        cage_base_(heap->isolate())
#endif  // V8_COMPRESS_POINTERS
  {
    USE(heap_);
  }

  void VisitRootPointer(Root root, const char* description,
                        FullObjectSlot p) override {
    FixHandle(p);
  }

  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) override {
    for (FullObjectSlot p = start; p < end; ++p) {
      FixHandle(p);
    }
  }

  // The pointer compression cage base value used for decompression of all
  // tagged values except references to InstructionStream objects.
  PtrComprCageBase cage_base() const {
#if V8_COMPRESS_POINTERS
    return cage_base_;
#else
    return PtrComprCageBase{};
#endif  // V8_COMPRESS_POINTERS
  }

 private:
  inline void FixHandle(FullObjectSlot p) {
    if (!(*p).IsHeapObject()) return;
    HeapObject current = HeapObject::cast(*p);
    if (!current.map_word(cage_base(), kRelaxedLoad).IsForwardingAddress() &&
        current.IsFreeSpaceOrFiller(cage_base())) {
#ifdef DEBUG
      // We need to find a FixedArrayBase map after walking the fillers.
      while (
          !current.map_word(cage_base(), kRelaxedLoad).IsForwardingAddress() &&
          current.IsFreeSpaceOrFiller(cage_base())) {
        Address next = current.ptr();
        if (current.map(cage_base()) ==
            ReadOnlyRoots(heap_).one_pointer_filler_map()) {
          next += kTaggedSize;
        } else if (current.map(cage_base()) ==
                   ReadOnlyRoots(heap_).two_pointer_filler_map()) {
          next += 2 * kTaggedSize;
        } else {
          next += current.Size();
        }
        current = HeapObject::cast(Object(next));
      }
      DCHECK(
          current.map_word(cage_base(), kRelaxedLoad).IsForwardingAddress() ||
          current.IsFixedArrayBase(cage_base()));
#endif  // DEBUG
      p.store(Smi::zero());
    }
  }

  Heap* heap_;

#if V8_COMPRESS_POINTERS
  const PtrComprCageBase cage_base_;
#endif  // V8_COMPRESS_POINTERS
};

void Heap::IterateRoots(RootVisitor* v, base::EnumSet<SkipRoot> options,
                        IterateRootsMode roots_mode) {
  v->VisitRootPointers(Root::kStrongRootList, nullptr,
                       roots_table().strong_roots_begin(),
                       roots_table().strong_roots_end());
  v->Synchronize(VisitorSynchronization::kStrongRootList);

  isolate_->bootstrapper()->Iterate(v);
  v->Synchronize(VisitorSynchronization::kBootstrapper);
  Relocatable::Iterate(isolate_, v);
  v->Synchronize(VisitorSynchronization::kRelocatable);
  isolate_->debug()->Iterate(v);
  v->Synchronize(VisitorSynchronization::kDebug);

  isolate_->compilation_cache()->Iterate(v);
  v->Synchronize(VisitorSynchronization::kCompilationCache);

  const bool skip_iterate_builtins =
      options.contains(SkipRoot::kOldGeneration) ||
      (Builtins::kCodeObjectsAreInROSpace &&
       options.contains(SkipRoot::kReadOnlyBuiltins));
  if (!skip_iterate_builtins) {
    IterateBuiltins(v);
    v->Synchronize(VisitorSynchronization::kBuiltins);
  }

  // Iterate over pointers being held by inactive threads.
  isolate_->thread_manager()->Iterate(v);
  v->Synchronize(VisitorSynchronization::kThreadManager);

  // Visitors in this block only run when not serializing. These include:
  //
  // - Thread-local and stack.
  // - Handles.
  // - Microtasks.
  // - The startup object cache.
  //
  // When creating real startup snapshot, these areas are expected to be empty.
  // It is also possible to create a snapshot of a *running* isolate for testing
  // purposes. In this case, these areas are likely not empty and will simply be
  // skipped.
  //
  // The general guideline for adding visitors to this section vs. adding them
  // above is that non-transient heap state is always visited, transient heap
  // state is visited only when not serializing.
  if (!options.contains(SkipRoot::kUnserializable)) {
    if (!options.contains(SkipRoot::kTracedHandles)) {
      if (options.contains(SkipRoot::kOldGeneration)) {
        isolate_->traced_handles()->IterateYoungRoots(v);
      } else {
        isolate_->traced_handles()->Iterate(v);
      }
    }

    if (!options.contains(SkipRoot::kGlobalHandles)) {
      if (options.contains(SkipRoot::kWeak)) {
        if (options.contains(SkipRoot::kOldGeneration)) {
          // Skip handles that are either weak or old.
          isolate_->global_handles()->IterateYoungStrongAndDependentRoots(v);
          isolate_->traced_handles()->IterateYoungRoots(v);
        } else {
          // Skip handles that are weak.
          isolate_->global_handles()->IterateStrongRoots(v);
        }
      } else {
        if (options.contains(SkipRoot::kOldGeneration)) {
          UNREACHABLE();
        } else {
          // Do not skip any handles.
          isolate_->global_handles()->IterateAllRoots(v);
        }
      }
    }
    v->Synchronize(VisitorSynchronization::kGlobalHandles);

    if (!options.contains(SkipRoot::kStack)) {
      IterateStackRoots(v);
      if (!options.contains(SkipRoot::kConservativeStack)) {
        ScanStackMode stack_mode = options.contains(SkipRoot::kTopOfStack)
                                       ? ScanStackMode::kFromMarker
                                       : ScanStackMode::kComplete;
        IterateConservativeStackRoots(v, stack_mode, roots_mode);
      }
      v->Synchronize(VisitorSynchronization::kStackRoots);
    }

    // Iterate over main thread handles in handle scopes.
    if (!options.contains(SkipRoot::kMainThreadHandles)) {
      // Clear main thread handles with stale references to left-trimmed
      // objects. The GC would crash on such stale references.
      ClearStaleLeftTrimmedHandlesVisitor left_trim_visitor(this);
      isolate_->handle_scope_implementer()->Iterate(&left_trim_visitor);
      isolate_->handle_scope_implementer()->Iterate(v);
    }
    // Iterate local handles for all local heaps.
    safepoint_->Iterate(v);
    // Iterates all persistent handles.
    isolate_->persistent_handles_list()->Iterate(v, isolate_);
    v->Synchronize(VisitorSynchronization::kHandleScope);

    if (options.contains(SkipRoot::kOldGeneration)) {
      isolate_->eternal_handles()->IterateYoungRoots(v);
    } else {
      isolate_->eternal_handles()->IterateAllRoots(v);
    }
    v->Synchronize(VisitorSynchronization::kEternalHandles);

    // Iterate over pending Microtasks stored in MicrotaskQueues.
    MicrotaskQueue* default_microtask_queue =
        isolate_->default_microtask_queue();
    if (default_microtask_queue) {
      MicrotaskQueue* microtask_queue = default_microtask_queue;
      do {
        microtask_queue->IterateMicrotasks(v);
        microtask_queue = microtask_queue->next();
      } while (microtask_queue != default_microtask_queue);
    }
    v->Synchronize(VisitorSynchronization::kMicroTasks);

    // Iterate over other strong roots (currently only identity maps and
    // deoptimization entries).
    for (StrongRootsEntry* current = strong_roots_head_; current;
         current = current->next) {
      v->VisitRootPointers(Root::kStrongRoots, current->label, current->start,
                           current->end);
    }
    v->Synchronize(VisitorSynchronization::kStrongRoots);

    // Iterate over the startup and shared heap object caches unless
    // serializing or deserializing.
    SerializerDeserializer::IterateStartupObjectCache(isolate_, v);
    v->Synchronize(VisitorSynchronization::kStartupObjectCache);

    // Iterate over shared heap object cache when the isolate owns this data
    // structure. Isolates which own the shared heap object cache are:
    //   * Shared isolate
    //   * Shared space/main isolate
    //   * All isolates which do not use the shared heap feature.
    //
    // However, worker/client isolates do not own the shared heap object cache
    // and should not iterate it.
    if (isolate_->is_shared_space_isolate() || !isolate_->has_shared_space()) {
      SerializerDeserializer::IterateSharedHeapObjectCache(isolate_, v);
      v->Synchronize(VisitorSynchronization::kSharedHeapObjectCache);
    }
  }

  if (!options.contains(SkipRoot::kWeak)) {
    IterateWeakRoots(v, options);
  }
}

void Heap::IterateRootsIncludingClients(RootVisitor* v,
                                        base::EnumSet<SkipRoot> options) {
  IterateRoots(v, options, IterateRootsMode::kMainIsolate);

  if (isolate()->is_shared_space_isolate()) {
    ClientRootVisitor<> client_root_visitor(v);
    // For client isolates, use the stack marker to conservatively scan the
    // stack.
    options.Add(SkipRoot::kTopOfStack);
    isolate()->global_safepoint()->IterateClientIsolates(
        [v = &client_root_visitor, options](Isolate* client) {
          client->heap()->IterateRoots(v, options,
                                       IterateRootsMode::kClientIsolate);
        });
  }
}

void Heap::IterateWeakGlobalHandles(RootVisitor* v) {
  isolate_->global_handles()->IterateWeakRoots(v);
  isolate_->traced_handles()->Iterate(v);
}

void Heap::IterateBuiltins(RootVisitor* v) {
  Builtins* builtins = isolate()->builtins();
  for (Builtin builtin = Builtins::kFirst; builtin <= Builtins::kLast;
       ++builtin) {
    const char* name = Builtins::name(builtin);
    v->VisitRootPointer(Root::kBuiltins, name, builtins->builtin_slot(builtin));
  }

  for (Builtin builtin = Builtins::kFirst; builtin <= Builtins::kLastTier0;
       ++builtin) {
    v->VisitRootPointer(Root::kBuiltins, Builtins::name(builtin),
                        builtins->builtin_tier0_slot(builtin));
  }

  // The entry table doesn't need to be updated since all builtins are embedded.
  static_assert(Builtins::AllBuiltinsAreIsolateIndependent());
}

void Heap::IterateStackRoots(RootVisitor* v) { isolate_->Iterate(v); }

void Heap::IterateConservativeStackRoots(RootVisitor* v,
                                         ScanStackMode stack_mode,
                                         IterateRootsMode roots_mode) {
#ifdef V8_ENABLE_CONSERVATIVE_STACK_SCANNING
  if (!IsGCWithStack()) return;

  // In case of a shared GC, we're interested in the main isolate for CSS.
  Isolate* main_isolate = roots_mode == IterateRootsMode::kClientIsolate
                              ? isolate()->shared_space_isolate()
                              : isolate();

  ConservativeStackVisitor stack_visitor(main_isolate, v);
  if (stack_mode == ScanStackMode::kComplete) {
    stack().IteratePointers(&stack_visitor);
  } else {
    stack().IteratePointersUntilMarker(&stack_visitor);
  }
#endif  // V8_ENABLE_CONSERVATIVE_STACK_SCANNING
}

// static
size_t Heap::DefaultMinSemiSpaceSize() {
#if ENABLE_HUGEPAGE
  static constexpr size_t kMinSemiSpaceSize =
      kHugePageSize * kPointerMultiplier;
#else
  static constexpr size_t kMinSemiSpaceSize = 512 * KB * kPointerMultiplier;
#endif
  static_assert(kMinSemiSpaceSize % (1 << kPageSizeBits) == 0);

  return kMinSemiSpaceSize;
}

// static
size_t Heap::DefaultMaxSemiSpaceSize() {
#if ENABLE_HUGEPAGE
  static constexpr size_t kMaxSemiSpaceCapacityBaseUnit =
      kHugePageSize * 2 * kPointerMultiplier;
#else
  static constexpr size_t kMaxSemiSpaceCapacityBaseUnit =
      MB * kPointerMultiplier;
#endif
  static_assert(kMaxSemiSpaceCapacityBaseUnit % (1 << kPageSizeBits) == 0);

  size_t max_semi_space_size =
      (v8_flags.minor_ms ? v8_flags.minor_ms_max_new_space_capacity_mb
                         : v8_flags.scavenger_max_new_space_capacity_mb) *
      kMaxSemiSpaceCapacityBaseUnit;
  DCHECK_EQ(0, max_semi_space_size % (1 << kPageSizeBits));
  return max_semi_space_size;
}

// static
size_t Heap::OldGenerationToSemiSpaceRatio() {
  DCHECK(!v8_flags.minor_ms);
  static constexpr size_t kOldGenerationToSemiSpaceRatio =
      128 * kHeapLimitMultiplier / kPointerMultiplier;
  return kOldGenerationToSemiSpaceRatio;
}

// static
size_t Heap::OldGenerationToSemiSpaceRatioLowMemory() {
  static constexpr size_t kOldGenerationToSemiSpaceRatioLowMemory =
      256 * kHeapLimitMultiplier / kPointerMultiplier;
  return kOldGenerationToSemiSpaceRatioLowMemory / (v8_flags.minor_ms ? 2 : 1);
}

void Heap::ConfigureHeap(const v8::ResourceConstraints& constraints) {
  // Initialize max_semi_space_size_.
  {
    max_semi_space_size_ = DefaultMaxSemiSpaceSize();
    if (constraints.max_young_generation_size_in_bytes() > 0) {
      max_semi_space_size_ = SemiSpaceSizeFromYoungGenerationSize(
          constraints.max_young_generation_size_in_bytes());
    }
    if (v8_flags.max_semi_space_size > 0) {
      max_semi_space_size_ =
          static_cast<size_t>(v8_flags.max_semi_space_size) * MB;
    } else if (v8_flags.max_heap_size > 0) {
      size_t max_heap_size = static_cast<size_t>(v8_flags.max_heap_size) * MB;
      size_t young_generation_size, old_generation_size;
      if (v8_flags.max_old_space_size > 0) {
        old_generation_size =
            static_cast<size_t>(v8_flags.max_old_space_size) * MB;
        young_generation_size = max_heap_size > old_generation_size
                                    ? max_heap_size - old_generation_size
                                    : 0;
      } else {
        GenerationSizesFromHeapSize(max_heap_size, &young_generation_size,
                                    &old_generation_size);
      }
      max_semi_space_size_ =
          SemiSpaceSizeFromYoungGenerationSize(young_generation_size);
    }
    if (v8_flags.stress_compaction) {
      // This will cause more frequent GCs when stressing.
      max_semi_space_size_ = MB;
    }
    if (!v8_flags.minor_ms) {
      // TODO(dinfuehr): Rounding to a power of 2 is technically no longer
      // needed but yields best performance on Pixel2.
      max_semi_space_size_ =
          static_cast<size_t>(base::bits::RoundUpToPowerOfTwo64(
              static_cast<uint64_t>(max_semi_space_size_)));
    }
    max_semi_space_size_ =
        std::max(max_semi_space_size_, DefaultMinSemiSpaceSize());
    max_semi_space_size_ = RoundDown<Page::kPageSize>(max_semi_space_size_);
  }

  // Initialize max_old_generation_size_ and max_global_memory_.
  {
    size_t max_old_generation_size = 700ul * (kSystemPointerSize / 4) * MB;
    if (constraints.max_old_generation_size_in_bytes() > 0) {
      max_old_generation_size = constraints.max_old_generation_size_in_bytes();
    }
    if (v8_flags.max_old_space_size > 0) {
      max_old_generation_size =
          static_cast<size_t>(v8_flags.max_old_space_size) * MB;
    } else if (v8_flags.max_heap_size > 0) {
      size_t max_heap_size = static_cast<size_t>(v8_flags.max_heap_size) * MB;
      size_t young_generation_size =
          YoungGenerationSizeFromSemiSpaceSize(max_semi_space_size_);
      max_old_generation_size = max_heap_size > young_generation_size
                                    ? max_heap_size - young_generation_size
                                    : 0;
    }
    max_old_generation_size =
        std::max(max_old_generation_size, MinOldGenerationSize());
    max_old_generation_size = std::min(max_old_generation_size,
                                       AllocatorLimitOnMaxOldGenerationSize());
    max_old_generation_size =
        RoundDown<Page::kPageSize>(max_old_generation_size);

    SetOldGenerationAndGlobalMaximumSize(max_old_generation_size);
  }

  CHECK_IMPLIES(
      v8_flags.max_heap_size > 0,
      v8_flags.max_semi_space_size == 0 || v8_flags.max_old_space_size == 0);

  // Initialize initial_semispace_size_.
  {
    initial_semispace_size_ = DefaultMinSemiSpaceSize();
    if (max_semi_space_size_ == DefaultMaxSemiSpaceSize()) {
      // Start with at least 1*MB semi-space on machines with a lot of memory.
      initial_semispace_size_ =
          std::max(initial_semispace_size_, static_cast<size_t>(1 * MB));
    }
    if (constraints.initial_young_generation_size_in_bytes() > 0) {
      initial_semispace_size_ = SemiSpaceSizeFromYoungGenerationSize(
          constraints.initial_young_generation_size_in_bytes());
    }
    if (v8_flags.initial_heap_size > 0) {
      size_t young_generation, old_generation;
      Heap::GenerationSizesFromHeapSize(
          static_cast<size_t>(v8_flags.initial_heap_size) * MB,
          &young_generation, &old_generation);
      initial_semispace_size_ =
          SemiSpaceSizeFromYoungGenerationSize(young_generation);
    }
    if (v8_flags.min_semi_space_size > 0) {
      initial_semispace_size_ =
          static_cast<size_t>(v8_flags.min_semi_space_size) * MB;
    }
    initial_semispace_size_ =
        std::min(initial_semispace_size_, max_semi_space_size_);
    initial_semispace_size_ =
        RoundDown<Page::kPageSize>(initial_semispace_size_);
  }

  if (v8_flags.lazy_new_space_shrinking) {
    initial_semispace_size_ = max_semi_space_size_;
  }

  // Initialize initial_old_space_size_.
  {
    initial_old_generation_size_ = kMaxInitialOldGenerationSize;
    if (constraints.initial_old_generation_size_in_bytes() > 0) {
      initial_old_generation_size_ =
          constraints.initial_old_generation_size_in_bytes();
      old_generation_allocation_limit_configured_ = true;
    }
    if (v8_flags.initial_heap_size > 0) {
      size_t initial_heap_size =
          static_cast<size_t>(v8_flags.initial_heap_size) * MB;
      size_t young_generation_size =
          YoungGenerationSizeFromSemiSpaceSize(initial_semispace_size_);
      initial_old_generation_size_ =
          initial_heap_size > young_generation_size
              ? initial_heap_size - young_generation_size
              : 0;
      old_generation_allocation_limit_configured_ = true;
    }
    if (v8_flags.initial_old_space_size > 0) {
      initial_old_generation_size_ =
          static_cast<size_t>(v8_flags.initial_old_space_size) * MB;
      old_generation_allocation_limit_configured_ = true;
    }
    initial_old_generation_size_ =
        std::min(initial_old_generation_size_, max_old_generation_size() / 2);
    initial_old_generation_size_ =
        RoundDown<Page::kPageSize>(initial_old_generation_size_);
  }

  if (old_generation_allocation_limit_configured_) {
    // If the embedder pre-configures the initial old generation size,
    // then allow V8 to skip full GCs below that threshold.
    min_old_generation_size_ = initial_old_generation_size_;
    min_global_memory_size_ =
        GlobalMemorySizeFromV8Size(min_old_generation_size_);
  }

  if (v8_flags.semi_space_growth_factor < 2) {
    v8_flags.semi_space_growth_factor = 2;
  }

  initial_max_old_generation_size_ = max_old_generation_size();
  ResetOldGenerationAndGlobalAllocationLimit();

  // We rely on being able to allocate new arrays in paged spaces.
  DCHECK(kMaxRegularHeapObjectSize >=
         (JSArray::kHeaderSize +
          FixedArray::SizeFor(JSArray::kInitialMaxFastElementArray) +
          ALIGN_TO_ALLOCATION_ALIGNMENT(AllocationMemento::kSize)));

  code_range_size_ = constraints.code_range_size_in_bytes();

  configured_ = true;
}

void Heap::AddToRingBuffer(const char* string) {
  size_t first_part =
      std::min(strlen(string), kTraceRingBufferSize - ring_buffer_end_);
  memcpy(trace_ring_buffer_ + ring_buffer_end_, string, first_part);
  ring_buffer_end_ += first_part;
  if (first_part < strlen(string)) {
    ring_buffer_full_ = true;
    size_t second_part = strlen(string) - first_part;
    memcpy(trace_ring_buffer_, string + first_part, second_part);
    ring_buffer_end_ = second_part;
  }
}

void Heap::GetFromRingBuffer(char* buffer) {
  size_t copied = 0;
  if (ring_buffer_full_) {
    copied = kTraceRingBufferSize - ring_buffer_end_;
    memcpy(buffer, trace_ring_buffer_ + ring_buffer_end_, copied);
  }
  memcpy(buffer + copied, trace_ring_buffer_, ring_buffer_end_);
}

void Heap::ConfigureHeapDefault() {
  v8::ResourceConstraints constraints;
  ConfigureHeap(constraints);
}

void Heap::RecordStats(HeapStats* stats, bool take_snapshot) {
  *stats->start_marker = HeapStats::kStartMarker;
  *stats->end_marker = HeapStats::kEndMarker;
  *stats->ro_space_size = read_only_space_->Size();
  *stats->ro_space_capacity = read_only_space_->Capacity();
  *stats->new_space_size = NewSpaceSize();
  *stats->new_space_capacity = NewSpaceCapacity();
  *stats->old_space_size = old_space_->SizeOfObjects();
  *stats->old_space_capacity = old_space_->Capacity();
  *stats->code_space_size = code_space_->SizeOfObjects();
  *stats->code_space_capacity = code_space_->Capacity();
  *stats->map_space_size = 0;
  *stats->map_space_capacity = 0;
  *stats->lo_space_size = lo_space_->Size();
  *stats->code_lo_space_size = code_lo_space_->Size();
  isolate_->global_handles()->RecordStats(stats);
  *stats->memory_allocator_size = memory_allocator()->Size();
  *stats->memory_allocator_capacity =
      memory_allocator()->Size() + memory_allocator()->Available();
  *stats->os_error = base::OS::GetLastError();
  // TODO(leszeks): Include the string table in both current and peak usage.
  *stats->malloced_memory = isolate_->allocator()->GetCurrentMemoryUsage();
  *stats->malloced_peak_memory = isolate_->allocator()->GetMaxMemoryUsage();
  if (take_snapshot) {
    HeapObjectIterator iterator(this);
    for (HeapObject obj = iterator.Next(); !obj.is_null();
         obj = iterator.Next()) {
      InstanceType type = obj.map().instance_type();
      DCHECK(0 <= type && type <= LAST_TYPE);
      stats->objects_per_type[type]++;
      stats->size_per_type[type] += obj.Size();
    }
  }
  if (stats->last_few_messages != nullptr)
    GetFromRingBuffer(stats->last_few_messages);
}

size_t Heap::OldGenerationSizeOfObjects() const {
  PagedSpaceIterator spaces(this);
  size_t total = 0;
  for (PagedSpace* space = spaces.Next(); space != nullptr;
       space = spaces.Next()) {
    total += space->SizeOfObjects();
  }
  if (shared_lo_space_) {
    total += shared_lo_space_->SizeOfObjects();
  }
  return total + lo_space_->SizeOfObjects() + code_lo_space_->SizeOfObjects();
}

size_t Heap::EmbedderSizeOfObjects() const {
  return cpp_heap_ ? CppHeap::From(cpp_heap_)->used_size() : 0;
}

size_t Heap::GlobalSizeOfObjects() const {
  return OldGenerationSizeOfObjects() + EmbedderSizeOfObjects();
}

uint64_t Heap::AllocatedExternalMemorySinceMarkCompact() const {
  return external_memory_.AllocatedSinceMarkCompact();
}

bool Heap::AllocationLimitOvershotByLargeMargin() const {
  // This guards against too eager finalization in small heaps.
  // The number is chosen based on v8.browsing_mobile on Nexus 7v2.
  constexpr size_t kMarginForSmallHeaps = 32u * MB;

  uint64_t size_now =
      OldGenerationSizeOfObjects() + AllocatedExternalMemorySinceMarkCompact();

  const size_t v8_overshoot = old_generation_allocation_limit() < size_now
                                  ? size_now - old_generation_allocation_limit()
                                  : 0;
  const size_t global_overshoot =
      global_allocation_limit_ < GlobalSizeOfObjects()
          ? GlobalSizeOfObjects() - global_allocation_limit_
          : 0;

  // Bail out if the V8 and global sizes are still below their respective
  // limits.
  if (v8_overshoot == 0 && global_overshoot == 0) {
    return false;
  }

  // Overshoot margin is 50% of allocation limit or half-way to the max heap
  // with special handling of small heaps.
  const size_t v8_margin = std::min(
      std::max(old_generation_allocation_limit() / 2, kMarginForSmallHeaps),
      (max_old_generation_size() - old_generation_allocation_limit()) / 2);
  const size_t global_margin =
      std::min(std::max(global_allocation_limit_ / 2, kMarginForSmallHeaps),
               (max_global_memory_size_ - global_allocation_limit_) / 2);

  return v8_overshoot >= v8_margin || global_overshoot >= global_margin;
}

bool Heap::ShouldOptimizeForLoadTime() {
  return isolate()->rail_mode() == PERFORMANCE_LOAD &&
         !AllocationLimitOvershotByLargeMargin() &&
         MonotonicallyIncreasingTimeInMs() <
             isolate()->LoadStartTimeMs() + kMaxLoadTimeMs;
}

// This predicate is called when an old generation space cannot allocated from
// the free list and is about to add a new page. Returning false will cause a
// major GC. It happens when the old generation allocation limit is reached and
// - either we need to optimize for memory usage,
// - or the incremental marking is not in progress and we cannot start it.
bool Heap::ShouldExpandOldGenerationOnSlowAllocation(LocalHeap* local_heap,
                                                     AllocationOrigin origin) {
  if (always_allocate() || OldGenerationSpaceAvailable() > 0) return true;
  // We reached the old generation allocation limit.

  // Allocations in the GC should always succeed if possible.
  if (origin == AllocationOrigin::kGC) return true;

  // Background threads need to be allowed to allocate without GC after teardown
  // was initiated.
  if (gc_state() == TEAR_DOWN) return true;

  // If main thread is parked, it can't perform the GC. Fix the deadlock by
  // allowing the allocation.
  if (IsMainThreadParked(local_heap)) return true;

  // If allocating isolate is deserialized at the moment then always allow
  // allocation.
  if (IsIsolateDeserializationActive(local_heap)) return true;

  // Make it more likely that retry of allocation on background thread succeeds
  if (IsRetryOfFailedAllocation(local_heap)) return true;

  // Background thread requested GC, allocation should fail
  if (CollectionRequested()) return false;

  if (ShouldOptimizeForMemoryUsage()) return false;

  if (ShouldOptimizeForLoadTime()) return true;

  if (IsMajorMarkingComplete(local_heap)) {
    return !AllocationLimitOvershotByLargeMargin();
  }

  if (incremental_marking()->IsStopped() &&
      IncrementalMarkingLimitReached() == IncrementalMarkingLimit::kNoLimit) {
    // We cannot start incremental marking.
    return false;
  }
  return true;
}

bool Heap::IsRetryOfFailedAllocation(LocalHeap* local_heap) {
  if (!local_heap) return false;
  return local_heap->allocation_failed_;
}

bool Heap::IsMainThreadParked(LocalHeap* local_heap) {
  if (!local_heap) return false;
  return local_heap->main_thread_parked_;
}

bool Heap::IsMajorMarkingComplete(LocalHeap* local_heap) {
  // Only check this on the main thread.
  if (!local_heap || !local_heap->is_main_thread()) return false;

  // But also ignore main threads of client isolates.
  if (local_heap->heap() != this) {
    DCHECK(isolate()->is_shared_space_isolate());
    return false;
  }

  return incremental_marking()->IsMajorMarkingComplete();
}

Heap::HeapGrowingMode Heap::CurrentHeapGrowingMode() {
  if (ShouldReduceMemory() || v8_flags.stress_compaction) {
    return Heap::HeapGrowingMode::kMinimal;
  }

  if (ShouldOptimizeForMemoryUsage()) {
    return Heap::HeapGrowingMode::kConservative;
  }

  if (memory_reducer() != nullptr && memory_reducer()->ShouldGrowHeapSlowly()) {
    return Heap::HeapGrowingMode::kSlow;
  }

  return Heap::HeapGrowingMode::kDefault;
}

base::Optional<size_t> Heap::GlobalMemoryAvailable() {
  size_t global_size = GlobalSizeOfObjects();

  if (global_size < global_allocation_limit_)
    return global_allocation_limit_ - global_size;

  return 0;
}

double Heap::PercentToOldGenerationLimit() {
  double size_at_gc = old_generation_size_at_last_gc_;
  double size_now =
      OldGenerationSizeOfObjects() + AllocatedExternalMemorySinceMarkCompact();
  double current_bytes = size_now - size_at_gc;
  double total_bytes = old_generation_allocation_limit() - size_at_gc;
  return total_bytes > 0 ? (current_bytes / total_bytes) * 100.0 : 0;
}

double Heap::PercentToGlobalMemoryLimit() {
  double size_at_gc = old_generation_size_at_last_gc_;
  double size_now =
      OldGenerationSizeOfObjects() + AllocatedExternalMemorySinceMarkCompact();
  double current_bytes = size_now - size_at_gc;
  double total_bytes = old_generation_allocation_limit() - size_at_gc;
  return total_bytes > 0 ? (current_bytes / total_bytes) * 100.0 : 0;
}

// - kNoLimit means that either incremental marking is disabled or it is too
// early to start incremental marking.
// - kSoftLimit means that incremental marking should be started soon.
// - kHardLimit means that incremental marking should be started immediately.
// - kFallbackForEmbedderLimit means that incremental marking should be
// started as soon as the embedder does not allocate with high throughput
// anymore.
Heap::IncrementalMarkingLimit Heap::IncrementalMarkingLimitReached() {
  // InstructionStream using an AlwaysAllocateScope assumes that the GC state
  // does not change; that implies that no marking steps must be performed.
  if (!incremental_marking()->CanBeStarted() || always_allocate()) {
    // Incremental marking is disabled or it is too early to start.
    return IncrementalMarkingLimit::kNoLimit;
  }
  if (v8_flags.stress_incremental_marking) {
    return IncrementalMarkingLimit::kHardLimit;
  }
  if (incremental_marking()->IsBelowActivationThresholds()) {
    // Incremental marking is disabled or it is too early to start.
    return IncrementalMarkingLimit::kNoLimit;
  }
  if (ShouldStressCompaction() || HighMemoryPressure()) {
    // If there is high memory pressure or stress testing is enabled, then
    // start marking immediately.
    return IncrementalMarkingLimit::kHardLimit;
  }

  if (v8_flags.stress_marking > 0) {
    int current_percent = static_cast<int>(
        std::max(PercentToOldGenerationLimit(), PercentToGlobalMemoryLimit()));
    if (current_percent > 0) {
      if (v8_flags.trace_stress_marking) {
        isolate()->PrintWithTimestamp(
            "[IncrementalMarking] %d%% of the memory limit reached\n",
            current_percent);
      }
      if (v8_flags.fuzzer_gc_analysis) {
        // Skips values >=100% since they already trigger marking.
        if (current_percent < 100) {
          max_marking_limit_reached_ =
              std::max<double>(max_marking_limit_reached_, current_percent);
        }
      } else if (current_percent >= stress_marking_percentage_) {
        return IncrementalMarkingLimit::kHardLimit;
      }
    }
  }

  if (v8_flags.incremental_marking_soft_trigger > 0 ||
      v8_flags.incremental_marking_hard_trigger > 0) {
    int current_percent = static_cast<int>(
        std::max(PercentToOldGenerationLimit(), PercentToGlobalMemoryLimit()));
    if (current_percent > v8_flags.incremental_marking_hard_trigger &&
        v8_flags.incremental_marking_hard_trigger > 0) {
      return IncrementalMarkingLimit::kHardLimit;
    }
    if (current_percent > v8_flags.incremental_marking_soft_trigger &&
        v8_flags.incremental_marking_soft_trigger > 0) {
      return IncrementalMarkingLimit::kSoftLimit;
    }
    return IncrementalMarkingLimit::kNoLimit;
  }

  size_t old_generation_space_available = OldGenerationSpaceAvailable();
  const base::Optional<size_t> global_memory_available =
      GlobalMemoryAvailable();

  if (old_generation_space_available > NewSpaceTargetCapacity() &&
      (!global_memory_available ||
       global_memory_available > NewSpaceTargetCapacity())) {
    if (cpp_heap() && !old_generation_allocation_limit_configured_ &&
        gc_count_ == 0) {
      // At this point the embedder memory is above the activation
      // threshold. No GC happened so far and it's thus unlikely to get a
      // configured heap any time soon. Start a memory reducer in this case
      // which will wait until the allocation rate is low to trigger garbage
      // collection.
      return IncrementalMarkingLimit::kFallbackForEmbedderLimit;
    }
    return IncrementalMarkingLimit::kNoLimit;
  }
  if (ShouldOptimizeForMemoryUsage()) {
    return IncrementalMarkingLimit::kHardLimit;
  }
  if (ShouldOptimizeForLoadTime()) {
    return IncrementalMarkingLimit::kNoLimit;
  }
  if (old_generation_space_available == 0) {
    return IncrementalMarkingLimit::kHardLimit;
  }
  if (global_memory_available && *global_memory_available == 0) {
    return IncrementalMarkingLimit::kHardLimit;
  }
  return IncrementalMarkingLimit::kSoftLimit;
}

bool Heap::ShouldStressCompaction() const {
  return v8_flags.stress_compaction && (gc_count_ & 1) != 0;
}

void Heap::EnableInlineAllocation() { inline_allocation_enabled_ = true; }

void Heap::DisableInlineAllocation() {
  inline_allocation_enabled_ = false;
  FreeMainThreadLinearAllocationAreas();
}

void Heap::SetUp(LocalHeap* main_thread_local_heap) {
  DCHECK_NULL(main_thread_local_heap_);
  main_thread_local_heap_ = main_thread_local_heap;

#ifdef V8_ENABLE_ALLOCATION_TIMEOUT
  heap_allocator_.UpdateAllocationTimeout();
#endif  // V8_ENABLE_ALLOCATION_TIMEOUT

#ifdef V8_ENABLE_THIRD_PARTY_HEAP
  tp_heap_ = third_party_heap::Heap::New(isolate());
#endif

  // Initialize heap spaces and initial maps and objects.
  //
  // If the heap is not yet configured (e.g. through the API), configure it.
  // Configuration is based on the flags new-space-size (really the semispace
  // size) and old-space-size if set or the initial values of semispace_size_
  // and old_generation_size_ otherwise.
  if (!configured_) ConfigureHeapDefault();

  mmap_region_base_ =
      reinterpret_cast<uintptr_t>(v8::internal::GetRandomMmapAddr()) &
      ~kMmapRegionMask;

  v8::PageAllocator* code_page_allocator;
  if (isolate_->RequiresCodeRange() || code_range_size_ != 0) {
    const size_t requested_size =
        code_range_size_ == 0 ? kMaximalCodeRangeSize : code_range_size_;
    // When a target requires the code range feature, we put all code objects in
    // a contiguous range of virtual address space, so that they can call each
    // other with near calls.
#ifdef V8_COMPRESS_POINTERS_IN_SHARED_CAGE
    // When sharing a pointer cage among Isolates, also share the
    // CodeRange. isolate_->page_allocator() is the process-wide pointer
    // compression cage's PageAllocator.
    code_range_ = CodeRange::EnsureProcessWideCodeRange(
        isolate_->page_allocator(), requested_size);
#else
    code_range_ = std::make_unique<CodeRange>();
    if (!code_range_->InitReservation(isolate_->page_allocator(),
                                      requested_size)) {
      V8::FatalProcessOutOfMemory(
          isolate_, "Failed to reserve virtual memory for CodeRange");
    }
#endif  // V8_COMPRESS_POINTERS_IN_SHARED_CAGE

    LOG(isolate_,
        NewEvent("CodeRange",
                 reinterpret_cast<void*>(code_range_->reservation()->address()),
                 code_range_size_));

    isolate_->AddCodeRange(code_range_->reservation()->region().begin(),
                           code_range_->reservation()->region().size());
    code_page_allocator = code_range_->page_allocator();
  } else {
    code_page_allocator = isolate_->page_allocator();
  }

  collection_barrier_.reset(new CollectionBarrier(this));

  // Set up memory allocator.
  memory_allocator_.reset(
      new MemoryAllocator(isolate_, code_page_allocator, MaxReserved()));

  sweeper_.reset(new Sweeper(this));

  mark_compact_collector_.reset(new MarkCompactCollector(this));

  scavenger_collector_.reset(new ScavengerCollector(this));
  minor_mark_sweep_collector_.reset(new MinorMarkSweepCollector(this));
  ephemeron_remembered_set_.reset(new EphemeronRememberedSet());

  incremental_marking_.reset(
      new IncrementalMarking(this, mark_compact_collector_->weak_objects()));

  if (v8_flags.concurrent_marking || v8_flags.parallel_marking) {
    concurrent_marking_.reset(
        new ConcurrentMarking(this, mark_compact_collector_->weak_objects()));
  } else {
    concurrent_marking_.reset(new ConcurrentMarking(this, nullptr));
  }

  // Set up layout tracing callback.
  if (V8_UNLIKELY(v8_flags.trace_gc_heap_layout)) {
    v8::GCType gc_type = kGCTypeMarkSweepCompact;
    if (V8_UNLIKELY(!v8_flags.trace_gc_heap_layout_ignore_minor_gc)) {
      gc_type = static_cast<v8::GCType>(gc_type | kGCTypeScavenge |
                                        kGCTypeMinorMarkSweep);
    }
    AddGCPrologueCallback(HeapLayoutTracer::GCProloguePrintHeapLayout, gc_type,
                          nullptr);
    AddGCEpilogueCallback(HeapLayoutTracer::GCEpiloguePrintHeapLayout, gc_type,
                          nullptr);
  }
  if (v8_flags.memory_balancer) {
    mb_.reset(new MemoryBalancer(this));
  }
}

void Heap::SetUpFromReadOnlyHeap(ReadOnlyHeap* ro_heap) {
  DCHECK_NOT_NULL(ro_heap);
  DCHECK_IMPLIES(read_only_space_ != nullptr,
                 read_only_space_ == ro_heap->read_only_space());
  DCHECK_NULL(space_[RO_SPACE].get());
  read_only_space_ = ro_heap->read_only_space();
  heap_allocator_.SetReadOnlySpace(read_only_space_);
}

void Heap::ReplaceReadOnlySpace(SharedReadOnlySpace* space) {
  CHECK(V8_SHARED_RO_HEAP_BOOL);
  if (read_only_space_) {
    read_only_space_->TearDown(memory_allocator());
    delete read_only_space_;
  }

  read_only_space_ = space;
  heap_allocator_.SetReadOnlySpace(read_only_space_);
}

class StressConcurrentAllocationObserver : public AllocationObserver {
 public:
  explicit StressConcurrentAllocationObserver(Heap* heap)
      : AllocationObserver(1024), heap_(heap) {}

  void Step(int bytes_allocated, Address, size_t) override {
    DCHECK(heap_->deserialization_complete());
    if (v8_flags.stress_concurrent_allocation) {
      // Only schedule task if --stress-concurrent-allocation is enabled. This
      // allows tests to disable flag even when Isolate was already initialized.
      StressConcurrentAllocatorTask::Schedule(heap_->isolate());
    }
    heap_->RemoveAllocationObserversFromAllSpaces(this, this);
    heap_->need_to_remove_stress_concurrent_allocation_observer_ = false;
  }

 private:
  Heap* heap_;
};

namespace {

size_t ReturnNull() { return 0; }

}  // namespace

void Heap::SetUpSpaces(LinearAllocationArea& new_allocation_info,
                       LinearAllocationArea& old_allocation_info) {
  // Ensure SetUpFromReadOnlySpace has been ran.
  DCHECK_NOT_NULL(read_only_space_);
  if (!v8_flags.single_generation) {
    if (v8_flags.minor_ms) {
      space_[NEW_SPACE] = std::make_unique<PagedNewSpace>(
          this, initial_semispace_size_, max_semi_space_size_,
          new_allocation_info);
    } else {
      space_[NEW_SPACE] = std::make_unique<SemiSpaceNewSpace>(
          this, initial_semispace_size_, max_semi_space_size_,
          new_allocation_info);
    }
    new_space_ = static_cast<NewSpace*>(space_[NEW_SPACE].get());

    space_[NEW_LO_SPACE] =
        std::make_unique<NewLargeObjectSpace>(this, NewSpaceCapacity());
    new_lo_space_ =
        static_cast<NewLargeObjectSpace*>(space_[NEW_LO_SPACE].get());
  }

  space_[OLD_SPACE] = std::make_unique<OldSpace>(this, old_allocation_info);
  old_space_ = static_cast<OldSpace*>(space_[OLD_SPACE].get());

  space_[CODE_SPACE] = std::make_unique<CodeSpace>(this);
  code_space_ = static_cast<CodeSpace*>(space_[CODE_SPACE].get());

  if (isolate()->is_shared_space_isolate()) {
    space_[SHARED_SPACE] = std::make_unique<SharedSpace>(this);
    shared_space_ = static_cast<SharedSpace*>(space_[SHARED_SPACE].get());
  }

  space_[LO_SPACE] = std::make_unique<OldLargeObjectSpace>(this);
  lo_space_ = static_cast<OldLargeObjectSpace*>(space_[LO_SPACE].get());

  space_[CODE_LO_SPACE] = std::make_unique<CodeLargeObjectSpace>(this);
  code_lo_space_ =
      static_cast<CodeLargeObjectSpace*>(space_[CODE_LO_SPACE].get());

  if (isolate()->is_shared_space_isolate()) {
    space_[SHARED_LO_SPACE] = std::make_unique<SharedLargeObjectSpace>(this);
    shared_lo_space_ =
        static_cast<SharedLargeObjectSpace*>(space_[SHARED_LO_SPACE].get());
  }

  for (int i = 0; i < static_cast<int>(v8::Isolate::kUseCounterFeatureCount);
       i++) {
    deferred_counters_[i] = 0;
  }

  tracer_.reset(new GCTracer(this));
  array_buffer_sweeper_.reset(new ArrayBufferSweeper(this));
  gc_idle_time_handler_.reset(new GCIdleTimeHandler());
  memory_measurement_.reset(new MemoryMeasurement(isolate()));
  if (v8_flags.memory_reducer) memory_reducer_.reset(new MemoryReducer(this));
  if (V8_UNLIKELY(TracingFlags::is_gc_stats_enabled())) {
    live_object_stats_.reset(new ObjectStats(this));
    dead_object_stats_.reset(new ObjectStats(this));
  }
  if (Heap::AllocationTrackerForDebugging::IsNeeded()) {
    allocation_tracker_for_debugging_ =
        std::make_unique<Heap::AllocationTrackerForDebugging>(this);
  }

  LOG(isolate_, IntPtrTEvent("heap-capacity", Capacity()));
  LOG(isolate_, IntPtrTEvent("heap-available", Available()));

  if (new_space()) {
    minor_gc_job_.reset(new MinorGCJob());
    if (v8_flags.minor_ms && v8_flags.concurrent_minor_ms_marking) {
      // TODO(v8:13012): Atomic MinorMS should not use ScavengeJob. Instead, we
      // should schedule MinorMS tasks at a soft limit, which are used by atomic
      // MinorMS, and to finalize concurrent MinorMS. The condition
      // v8_flags.concurrent_minor_ms_marking can then be changed to
      // v8_flags.minor_ms (here and at the RemoveAllocationObserver call site).
      minor_gc_task_observer_.reset(
          new MinorMSIncrementalMarkingTaskObserver(this));
    } else {
      // ScavengeJob is used by atomic MinorMS and Scavenger.
      minor_gc_task_observer_.reset(new ScheduleMinorGCTaskObserver(this));
    }
  }

  SetGetExternallyAllocatedMemoryInBytesCallback(ReturnNull);

  if (v8_flags.stress_marking > 0) {
    stress_marking_percentage_ = NextStressMarkingLimit();
  }
  if (IsStressingScavenge()) {
    stress_scavenge_observer_ = new StressScavengeObserver(this);
    new_space()->AddAllocationObserver(stress_scavenge_observer_);
  }

  write_protect_code_memory_ = v8_flags.write_protect_code_memory;
#if V8_HEAP_USE_PKU_JIT_WRITE_PROTECT
  if (RwxMemoryWriteScope::IsSupported()) {
    // If PKU machinery is available then use it instead of conventional
    // mprotect.
    write_protect_code_memory_ = false;
  }
#endif  // V8_HEAP_USE_PKU_JIT_WRITE_PROTECT

  if (isolate()->has_shared_space()) {
    Heap* heap = isolate()->shared_space_isolate()->heap();

    shared_space_allocator_ = std::make_unique<ConcurrentAllocator>(
        main_thread_local_heap(), heap->shared_space_,
        ConcurrentAllocator::Context::kNotGC);

    shared_allocation_space_ = heap->shared_space_;
    shared_lo_allocation_space_ = heap->shared_lo_space_;
  }

  main_thread_local_heap()->SetUpMainThread();
  heap_allocator_.Setup();
}

void Heap::InitializeHashSeed() {
  DCHECK(!deserialization_complete_);
  uint64_t new_hash_seed;
  if (v8_flags.hash_seed == 0) {
    int64_t rnd = isolate()->random_number_generator()->NextInt64();
    new_hash_seed = static_cast<uint64_t>(rnd);
  } else {
    new_hash_seed = static_cast<uint64_t>(v8_flags.hash_seed);
  }
  ReadOnlyRoots(this).hash_seed()->copy_in(
      0, reinterpret_cast<uint8_t*>(&new_hash_seed), kInt64Size);
}

// static
void Heap::InitializeOncePerProcess() {
#ifdef V8_ENABLE_ALLOCATION_TIMEOUT
  HeapAllocator::InitializeOncePerProcess();
#endif
  MemoryAllocator::InitializeOncePerProcess();
  if (v8_flags.predictable) {
    ::heap::base::WorklistBase::EnforcePredictableOrder();
  }
}

void Heap::PrintMaxMarkingLimitReached() {
  PrintF("\n### Maximum marking limit reached = %.02lf\n",
         max_marking_limit_reached_);
}

void Heap::PrintMaxNewSpaceSizeReached() {
  PrintF("\n### Maximum new space size reached = %.02lf\n",
         stress_scavenge_observer_->MaxNewSpaceSizeReached());
}

int Heap::NextStressMarkingLimit() {
  return isolate()->fuzzer_rng()->NextInt(v8_flags.stress_marking + 1);
}

void Heap::WeakenDescriptorArrays(
    GlobalHandleVector<DescriptorArray> strong_descriptor_arrays) {
  if (incremental_marking()->IsMajorMarking()) {
    // During incremental/concurrent marking regular DescriptorArray objects are
    // treated with custom weakness. This weakness depends on
    // DescriptorArray::raw_gc_state() which is not set up properly upon
    // deserialization. The strong arrays are transitioned to weak ones at the
    // end of the GC.
    mark_compact_collector()->RecordStrongDescriptorArraysForWeakening(
        std::move(strong_descriptor_arrays));
    return;
  }

  // No GC is running, weaken the arrays right away.
  DisallowGarbageCollection no_gc;
  Map descriptor_array_map = ReadOnlyRoots(isolate()).descriptor_array_map();
  for (auto it = strong_descriptor_arrays.begin();
       it != strong_descriptor_arrays.end(); ++it) {
    DescriptorArray array = it.raw();
    DCHECK(array.IsStrongDescriptorArray());
    array.set_map_safe_transition_no_write_barrier(descriptor_array_map);
    DCHECK_EQ(array.raw_gc_state(kRelaxedLoad), 0);
  }
}

void Heap::NotifyDeserializationComplete() {
  PagedSpaceIterator spaces(this);
  for (PagedSpace* s = spaces.Next(); s != nullptr; s = spaces.Next()) {
    // Shared space is used concurrently and cannot be shrunk.
    if (s->identity() == SHARED_SPACE) continue;
    if (isolate()->snapshot_available()) s->ShrinkImmortalImmovablePages();
#ifdef DEBUG
    // All pages right after bootstrapping must be marked as never-evacuate.
    for (Page* p : *s) {
      DCHECK(p->NeverEvacuate());
    }
#endif  // DEBUG
  }

  if (v8_flags.stress_concurrent_allocation) {
    stress_concurrent_allocation_observer_.reset(
        new StressConcurrentAllocationObserver(this));
    AddAllocationObserversToAllSpaces(
        stress_concurrent_allocation_observer_.get(),
        stress_concurrent_allocation_observer_.get());
    need_to_remove_stress_concurrent_allocation_observer_ = true;
  }

  // Deserialization will never create objects in new space.
  DCHECK_IMPLIES(new_space(), new_space()->Size() == 0);
  DCHECK_IMPLIES(new_lo_space(), new_lo_space()->Size() == 0);

  deserialization_complete_ = true;
}

void Heap::NotifyBootstrapComplete() {
  // This function is invoked for each native context creation. We are
  // interested only in the first native context.
  if (old_generation_capacity_after_bootstrap_ == 0) {
    old_generation_capacity_after_bootstrap_ = OldGenerationCapacity();
  }
}

void Heap::NotifyOldGenerationExpansion(AllocationSpace space,
                                        MemoryChunk* chunk) {
  // Do the same thing we would have done for background expansion.
  NotifyOldGenerationExpansionBackground(space, chunk);

  const size_t kMemoryReducerActivationThreshold = 1 * MB;
  if (memory_reducer() != nullptr && old_generation_capacity_after_bootstrap_ &&
      ms_count_ == 0 &&
      OldGenerationCapacity() >= old_generation_capacity_after_bootstrap_ +
                                     kMemoryReducerActivationThreshold &&
      v8_flags.memory_reducer_for_small_heaps) {
    memory_reducer()->NotifyPossibleGarbage();
  }
}

void Heap::NotifyOldGenerationExpansionBackground(AllocationSpace space,
                                                  MemoryChunk* chunk) {
  // Pages created during bootstrapping may contain immortal immovable objects.
  if (!deserialization_complete()) {
    DCHECK_NE(NEW_SPACE, chunk->owner()->identity());
    chunk->MarkNeverEvacuate();
  }
  if (space == CODE_SPACE || space == CODE_LO_SPACE) {
    isolate()->AddCodeMemoryChunk(chunk);
  }
}

void Heap::SetEmbedderRootsHandler(EmbedderRootsHandler* handler) {
  embedder_roots_handler_ = handler;
}

EmbedderRootsHandler* Heap::GetEmbedderRootsHandler() const {
  return embedder_roots_handler_;
}

void Heap::AttachCppHeap(v8::CppHeap* cpp_heap) {
  CHECK(!incremental_marking()->IsMarking());
  CppHeap::From(cpp_heap)->AttachIsolate(isolate());
  cpp_heap_ = cpp_heap;
}

void Heap::DetachCppHeap() {
  CppHeap::From(cpp_heap_)->DetachIsolate();
  cpp_heap_ = nullptr;
}

const cppgc::EmbedderStackState* Heap::overriden_stack_state() const {
  const auto* cpp_heap = CppHeap::From(cpp_heap_);
  return cpp_heap ? cpp_heap->override_stack_state() : nullptr;
}

void Heap::SetStackStart(void* stack_start) {
  stack().SetStackStart(stack_start);
}

::heap::base::Stack& Heap::stack() { return isolate_->stack(); }

void Heap::StartTearDown() {
  // Finish any ongoing sweeping to avoid stray background tasks still accessing
  // the heap during teardown.
  CompleteSweepingFull();

  memory_allocator()->unmapper()->EnsureUnmappingCompleted();

  if (v8_flags.concurrent_marking) {
    concurrent_marking()->Pause();
  }

  SetGCState(TEAR_DOWN);

  // Background threads may allocate and block until GC is performed. However
  // this might never happen when the main thread tries to quit and doesn't
  // process the event queue anymore. Avoid this deadlock by allowing all
  // allocations after tear down was requested to make sure all background
  // threads finish.
  collection_barrier_->NotifyShutdownRequested();

  // Main thread isn't going to allocate anymore.
  main_thread_local_heap()->FreeLinearAllocationArea();

  FreeMainThreadSharedLinearAllocationAreas();
}

void Heap::TearDownWithSharedHeap() {
  DCHECK_EQ(gc_state(), TEAR_DOWN);

  // Assert that there are no background threads left and no executable memory
  // chunks are unprotected.
  safepoint()->AssertMainThreadIsOnlyThread();

  // Now that all threads are stopped, verify the heap before tearing down the
  // heap/isolate.
  HeapVerifier::VerifyHeapIfEnabled(this);

  // Might use the external pointer which might be in the shared heap.
  external_string_table_.TearDown();

  // Publish shared object worklist for the main thread if incremental marking
  // is enabled for the shared heap.
  main_thread_local_heap()->marking_barrier()->PublishSharedIfNeeded();
}

void Heap::TearDown() {
  DCHECK_EQ(gc_state(), TEAR_DOWN);

  // Assert that there are no background threads left and no executable memory
  // chunks are unprotected.
  safepoint()->AssertMainThreadIsOnlyThread();

  DCHECK(concurrent_marking()->IsStopped());

  // It's too late for Heap::Verify() here, as parts of the Isolate are
  // already gone by the time this is called.

  UpdateMaximumCommitted();

  if (v8_flags.fuzzer_gc_analysis) {
    if (v8_flags.stress_marking > 0) {
      PrintMaxMarkingLimitReached();
    }
    if (IsStressingScavenge()) {
      PrintMaxNewSpaceSizeReached();
    }
  }

  minor_gc_task_observer_.reset();
  minor_gc_job_.reset();

  if (need_to_remove_stress_concurrent_allocation_observer_) {
    RemoveAllocationObserversFromAllSpaces(
        stress_concurrent_allocation_observer_.get(),
        stress_concurrent_allocation_observer_.get());
  }
  stress_concurrent_allocation_observer_.reset();

  if (IsStressingScavenge()) {
    new_space()->RemoveAllocationObserver(stress_scavenge_observer_);
    delete stress_scavenge_observer_;
    stress_scavenge_observer_ = nullptr;
  }

  if (mark_compact_collector_) {
    mark_compact_collector_->TearDown();
    mark_compact_collector_.reset();
  }

  if (minor_mark_sweep_collector_) {
    minor_mark_sweep_collector_->TearDown();
    minor_mark_sweep_collector_.reset();
  }

  sweeper_->TearDown();
  sweeper_.reset();

  scavenger_collector_.reset();
  array_buffer_sweeper_.reset();
  incremental_marking_.reset();
  concurrent_marking_.reset();

  gc_idle_time_handler_.reset();
  memory_measurement_.reset();
  allocation_tracker_for_debugging_.reset();
  ephemeron_remembered_set_.reset();

  if (memory_reducer_ != nullptr) {
    memory_reducer_->TearDown();
    memory_reducer_.reset();
  }

  live_object_stats_.reset();
  dead_object_stats_.reset();

  embedder_roots_handler_ = nullptr;

  if (cpp_heap_) {
    CppHeap::From(cpp_heap_)->DetachIsolate();
    cpp_heap_ = nullptr;
  }

  tracer_.reset();

  pretenuring_handler_.reset();

  shared_space_allocator_.reset();

  {
    CodePageHeaderModificationScope rwx_write_scope(
        "Deletion of CODE_SPACE and CODE_LO_SPACE requires write access to "
        "Code page headers");
    for (int i = FIRST_MUTABLE_SPACE; i <= LAST_MUTABLE_SPACE; i++) {
      space_[i].reset();
    }
  }

  isolate()->read_only_heap()->OnHeapTearDown(this);
  read_only_space_ = nullptr;

  memory_allocator()->TearDown();

  StrongRootsEntry* next = nullptr;
  for (StrongRootsEntry* current = strong_roots_head_; current;
       current = next) {
    next = current->next;
    delete current;
  }
  strong_roots_head_ = nullptr;

  memory_allocator_.reset();
}

void Heap::AddGCPrologueCallback(v8::Isolate::GCCallbackWithData callback,
                                 GCType gc_type, void* data) {
  gc_prologue_callbacks_.Add(
      callback, reinterpret_cast<v8::Isolate*>(isolate()), gc_type, data);
}

void Heap::RemoveGCPrologueCallback(v8::Isolate::GCCallbackWithData callback,
                                    void* data) {
  gc_prologue_callbacks_.Remove(callback, data);
}

void Heap::AddGCEpilogueCallback(v8::Isolate::GCCallbackWithData callback,
                                 GCType gc_type, void* data) {
  gc_epilogue_callbacks_.Add(
      callback, reinterpret_cast<v8::Isolate*>(isolate()), gc_type, data);
}

void Heap::RemoveGCEpilogueCallback(v8::Isolate::GCCallbackWithData callback,
                                    void* data) {
  gc_epilogue_callbacks_.Remove(callback, data);
}

namespace {
Handle<WeakArrayList> CompactWeakArrayList(Heap* heap,
                                           Handle<WeakArrayList> array,
                                           AllocationType allocation) {
  if (array->length() == 0) {
    return array;
  }
  int new_length = array->CountLiveWeakReferences();
  if (new_length == array->length()) {
    return array;
  }

  Handle<WeakArrayList> new_array = WeakArrayList::EnsureSpace(
      heap->isolate(),
      handle(ReadOnlyRoots(heap).empty_weak_array_list(), heap->isolate()),
      new_length, allocation);
  // Allocation might have caused GC and turned some of the elements into
  // cleared weak heap objects. Count the number of live references again and
  // fill in the new array.
  int copy_to = 0;
  for (int i = 0; i < array->length(); i++) {
    MaybeObject element = array->Get(i);
    if (element->IsCleared()) continue;
    new_array->Set(copy_to++, element);
  }
  new_array->set_length(copy_to);
  return new_array;
}

}  // anonymous namespace

void Heap::CompactWeakArrayLists() {
  // Find known PrototypeUsers and compact them.
  std::vector<Handle<PrototypeInfo>> prototype_infos;
  {
    HeapObjectIterator iterator(this);
    for (HeapObject o = iterator.Next(); !o.is_null(); o = iterator.Next()) {
      if (o.IsPrototypeInfo()) {
        PrototypeInfo prototype_info = PrototypeInfo::cast(o);
        if (prototype_info.prototype_users().IsWeakArrayList()) {
          prototype_infos.emplace_back(handle(prototype_info, isolate()));
        }
      }
    }
  }
  for (auto& prototype_info : prototype_infos) {
    Handle<WeakArrayList> array(
        WeakArrayList::cast(prototype_info->prototype_users()), isolate());
    DCHECK(InOldSpace(*array) ||
           *array == ReadOnlyRoots(this).empty_weak_array_list());
    WeakArrayList new_array = PrototypeUsers::Compact(
        array, this, JSObject::PrototypeRegistryCompactionCallback,
        AllocationType::kOld);
    prototype_info->set_prototype_users(new_array);
  }

  // Find known WeakArrayLists and compact them.
  Handle<WeakArrayList> scripts(script_list(), isolate());
  DCHECK_IMPLIES(!V8_ENABLE_THIRD_PARTY_HEAP_BOOL, InOldSpace(*scripts));
  scripts = CompactWeakArrayList(this, scripts, AllocationType::kOld);
  set_script_list(*scripts);
}

void Heap::AddRetainedMap(Handle<NativeContext> context, Handle<Map> map) {
  DCHECK(!map->InAnySharedSpace());

  if (map->is_in_retained_map_list()) {
    return;
  }

  Handle<WeakArrayList> array(WeakArrayList::cast(context->retained_maps()),
                              isolate());
  if (array->IsFull()) {
    CompactRetainedMaps(*array);
  }
  array =
      WeakArrayList::AddToEnd(isolate(), array, MaybeObjectHandle::Weak(map),
                              Smi::FromInt(v8_flags.retain_maps_for_n_gc));
  if (*array != context->retained_maps()) {
    context->set_retained_maps(*array);
  }
  map->set_is_in_retained_map_list(true);
}

void Heap::CompactRetainedMaps(WeakArrayList retained_maps) {
  int length = retained_maps.length();
  int new_length = 0;
  // This loop compacts the array by removing cleared weak cells.
  for (int i = 0; i < length; i += 2) {
    MaybeObject maybe_object = retained_maps.Get(i);
    if (maybe_object->IsCleared()) {
      continue;
    }

    DCHECK(maybe_object->IsWeak());

    MaybeObject age = retained_maps.Get(i + 1);
    DCHECK(age->IsSmi());
    if (i != new_length) {
      retained_maps.Set(new_length, maybe_object);
      retained_maps.Set(new_length + 1, age);
    }
    new_length += 2;
  }
  HeapObject undefined = ReadOnlyRoots(this).undefined_value();
  for (int i = new_length; i < length; i++) {
    retained_maps.Set(i, HeapObjectReference::Strong(undefined));
  }
  if (new_length != length) retained_maps.set_length(new_length);
}

void Heap::FatalProcessOutOfMemory(const char* location) {
  V8::FatalProcessOutOfMemory(isolate(), location, V8::kHeapOOM);
}

#ifdef DEBUG

class PrintHandleVisitor : public RootVisitor {
 public:
  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) override {
    for (FullObjectSlot p = start; p < end; ++p)
      PrintF("  handle %p to %p\n", p.ToVoidPtr(),
             reinterpret_cast<void*>((*p).ptr()));
  }
};

void Heap::PrintHandles() {
  PrintF("Handles:\n");
  PrintHandleVisitor v;
  isolate_->handle_scope_implementer()->Iterate(&v);
}

#endif

class CheckHandleCountVisitor : public RootVisitor {
 public:
  CheckHandleCountVisitor() : handle_count_(0) {}
  ~CheckHandleCountVisitor() override {
    CHECK_GT(HandleScope::kCheckHandleThreshold, handle_count_);
  }
  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) override {
    handle_count_ += end - start;
  }

 private:
  ptrdiff_t handle_count_;
};

void Heap::CheckHandleCount() {
  CheckHandleCountVisitor v;
  isolate_->handle_scope_implementer()->Iterate(&v);
}

void Heap::ClearRecordedSlot(HeapObject object, ObjectSlot slot) {
#ifndef V8_DISABLE_WRITE_BARRIERS
  DCHECK(!IsLargeObject(object));
  Page* page = Page::FromAddress(slot.address());
  if (!page->InYoungGeneration()) {
    DCHECK_EQ(page->owner_identity(), OLD_SPACE);

    // We only need to remove that slot when sweeping is still in progress.
    // Because in that case, a concurrent sweeper could find that memory and
    // reuse it for subsequent allocations. The runtime could install another
    // property at this slot but without unboxed doubles this will always be a
    // tagged pointer.
    if (!page->SweepingDone()) {
      // No need to update old-to-old here since that remembered set is gone
      // after a full GC and not re-recorded until sweeping is finished.
      RememberedSet<OLD_TO_NEW>::Remove(page, slot.address());
      RememberedSet<OLD_TO_NEW_BACKGROUND>::Remove(page, slot.address());
      RememberedSet<OLD_TO_SHARED>::Remove(page, slot.address());
    }
  }
#endif
}

// static
int Heap::InsertIntoRememberedSetFromCode(MemoryChunk* chunk, Address slot) {
  // This is called during runtime by a builtin, therefore it is run in the main
  // thread.
  DCHECK_NULL(LocalHeap::Current());
  RememberedSet<OLD_TO_NEW>::Insert<AccessMode::NON_ATOMIC>(chunk, slot);
  return 0;
}

#ifdef DEBUG
void Heap::VerifySlotRangeHasNoRecordedSlots(Address start, Address end) {
#ifndef V8_DISABLE_WRITE_BARRIERS
  Page* page = Page::FromAddress(start);
  RememberedSet<OLD_TO_NEW>::CheckNoneInRange(page, start, end);
  RememberedSet<OLD_TO_NEW_BACKGROUND>::CheckNoneInRange(page, start, end);
  RememberedSet<OLD_TO_SHARED>::CheckNoneInRange(page, start, end);
#endif
}
#endif

void Heap::ClearRecordedSlotRange(Address start, Address end) {
#ifndef V8_DISABLE_WRITE_BARRIERS
  Page* page = Page::FromAddress(start);
  DCHECK(!page->IsLargePage());
  if (!page->InYoungGeneration()) {
    // This method will be invoked on objects in shared space for
    // internalization and string forwarding during GC.
    DCHECK(page->owner_identity() == OLD_SPACE ||
           page->owner_identity() == SHARED_SPACE);

    if (!page->SweepingDone()) {
      RememberedSet<OLD_TO_NEW>::RemoveRange(page, start, end,
                                             SlotSet::KEEP_EMPTY_BUCKETS);
      RememberedSet<OLD_TO_NEW_BACKGROUND>::RemoveRange(
          page, start, end, SlotSet::KEEP_EMPTY_BUCKETS);
      RememberedSet<OLD_TO_SHARED>::RemoveRange(page, start, end,
                                                SlotSet::KEEP_EMPTY_BUCKETS);
    }
  }
#endif
}

PagedSpace* PagedSpaceIterator::Next() {
  DCHECK_GE(counter_, FIRST_GROWABLE_PAGED_SPACE);
  while (counter_ <= LAST_GROWABLE_PAGED_SPACE) {
    PagedSpace* space = heap_->paged_space(counter_++);
    if (space) return space;
  }
  return nullptr;
}

class HeapObjectsFilter {
 public:
  virtual ~HeapObjectsFilter() = default;
  virtual bool SkipObject(HeapObject object) = 0;
};

class UnreachableObjectsFilter : public HeapObjectsFilter {
 public:
  explicit UnreachableObjectsFilter(Heap* heap) : heap_(heap) {
    MarkReachableObjects();
  }

  ~UnreachableObjectsFilter() override {
    for (auto it : reachable_) {
      delete it.second;
      it.second = nullptr;
    }
  }

  bool SkipObject(HeapObject object) override {
    if (object.IsFreeSpaceOrFiller()) return true;
    Address chunk = object.ptr() & ~kLogicalChunkAlignmentMask;
    if (reachable_.count(chunk) == 0) return true;
    return reachable_[chunk]->count(object) == 0;
  }

 private:
  bool MarkAsReachable(HeapObject object) {
    Address chunk = object.ptr() & ~kLogicalChunkAlignmentMask;
    if (reachable_.count(chunk) == 0) {
      reachable_[chunk] = new std::unordered_set<HeapObject, Object::Hasher>();
    }
    if (reachable_[chunk]->count(object)) return false;
    reachable_[chunk]->insert(object);
    return true;
  }

  static constexpr intptr_t kLogicalChunkAlignment =
      (static_cast<uintptr_t>(1) << kPageSizeBits);

  static constexpr intptr_t kLogicalChunkAlignmentMask =
      kLogicalChunkAlignment - 1;

  class MarkingVisitor : public ObjectVisitorWithCageBases, public RootVisitor {
   public:
    explicit MarkingVisitor(UnreachableObjectsFilter* filter)
        : ObjectVisitorWithCageBases(filter->heap_), filter_(filter) {}

    void VisitMapPointer(HeapObject object) override {
      MarkHeapObject(Map::unchecked_cast(object.map(cage_base())));
    }
    void VisitPointers(HeapObject host, ObjectSlot start,
                       ObjectSlot end) override {
      MarkPointers(MaybeObjectSlot(start), MaybeObjectSlot(end));
    }

    void VisitPointers(HeapObject host, MaybeObjectSlot start,
                       MaybeObjectSlot end) final {
      MarkPointers(start, end);
    }

    void VisitInstructionStreamPointer(Code host,
                                       InstructionStreamSlot slot) override {
      Object maybe_code = slot.load(code_cage_base());
      HeapObject heap_object;
      if (maybe_code.GetHeapObject(&heap_object)) {
        MarkHeapObject(heap_object);
      }
    }

    void VisitCodeTarget(InstructionStream host, RelocInfo* rinfo) final {
      InstructionStream target =
          InstructionStream::FromTargetAddress(rinfo->target_address());
      MarkHeapObject(target);
    }
    void VisitEmbeddedPointer(InstructionStream host, RelocInfo* rinfo) final {
      MarkHeapObject(rinfo->target_object(cage_base()));
    }

    void VisitRootPointers(Root root, const char* description,
                           FullObjectSlot start, FullObjectSlot end) override {
      MarkPointersImpl(start, end);
    }
    void VisitRootPointers(Root root, const char* description,
                           OffHeapObjectSlot start,
                           OffHeapObjectSlot end) override {
      MarkPointersImpl(start, end);
    }

    void TransitiveClosure() {
      while (!marking_stack_.empty()) {
        HeapObject obj = marking_stack_.back();
        marking_stack_.pop_back();
        obj.Iterate(cage_base(), this);
      }
    }

   private:
    void MarkPointers(MaybeObjectSlot start, MaybeObjectSlot end) {
      MarkPointersImpl(start, end);
    }

    template <typename TSlot>
    V8_INLINE void MarkPointersImpl(TSlot start, TSlot end) {
      // Treat weak references as strong.
      for (TSlot p = start; p < end; ++p) {
        typename TSlot::TObject object = p.load(cage_base());
        HeapObject heap_object;
        if (object.GetHeapObject(&heap_object)) {
          MarkHeapObject(heap_object);
        }
      }
    }

    V8_INLINE void MarkHeapObject(HeapObject heap_object) {
      if (filter_->MarkAsReachable(heap_object)) {
        marking_stack_.push_back(heap_object);
      }
    }

    UnreachableObjectsFilter* filter_;
    std::vector<HeapObject> marking_stack_;
  };

  friend class MarkingVisitor;

  void MarkReachableObjects() {
    MarkingVisitor visitor(this);
    heap_->IterateRoots(&visitor, {});
    visitor.TransitiveClosure();
  }

  Heap* heap_;
  DISALLOW_GARBAGE_COLLECTION(no_gc_)
  std::unordered_map<Address, std::unordered_set<HeapObject, Object::Hasher>*>
      reachable_;
};

HeapObjectIterator::HeapObjectIterator(
    Heap* heap, HeapObjectIterator::HeapObjectsFiltering filtering)
    : heap_(heap),
      safepoint_scope_(std::make_unique<SafepointScope>(
          heap->isolate(), heap->isolate()->is_shared_space_isolate()
                               ? SafepointKind::kGlobal
                               : SafepointKind::kIsolate)),
      filtering_(filtering),
      filter_(nullptr),
      space_iterator_(nullptr),
      object_iterator_(nullptr) {
  heap_->MakeHeapIterable();
  // Start the iteration.
  space_iterator_ = new SpaceIterator(heap_);
  switch (filtering_) {
    case kFilterUnreachable:
      filter_ = new UnreachableObjectsFilter(heap_);
      break;
    default:
      break;
  }
  CHECK(space_iterator_->HasNext());
  object_iterator_ = space_iterator_->Next()->GetObjectIterator(heap_);
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) heap_->tp_heap_->ResetIterator();
}

HeapObjectIterator::~HeapObjectIterator() {
#ifdef DEBUG
  // Assert that in filtering mode we have iterated through all
  // objects. Otherwise, heap will be left in an inconsistent state.
  if (!V8_ENABLE_THIRD_PARTY_HEAP_BOOL && filtering_ != kNoFiltering) {
    DCHECK_NULL(object_iterator_);
  }
#endif
  delete space_iterator_;
  delete filter_;
}

HeapObject HeapObjectIterator::Next() {
  if (filter_ == nullptr) return NextObject();

  HeapObject obj = NextObject();
  while (!obj.is_null() && (filter_->SkipObject(obj))) obj = NextObject();
  return obj;
}

HeapObject HeapObjectIterator::NextObject() {
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) return heap_->tp_heap_->NextObject();
  // No iterator means we are done.
  if (object_iterator_.get() == nullptr) return HeapObject();

  HeapObject obj = object_iterator_.get()->Next();
  if (!obj.is_null()) {
    // If the current iterator has more objects we are fine.
    return obj;
  } else {
    // Go though the spaces looking for one that has objects.
    while (space_iterator_->HasNext()) {
      object_iterator_ = space_iterator_->Next()->GetObjectIterator(heap_);
      obj = object_iterator_.get()->Next();
      if (!obj.is_null()) {
        return obj;
      }
    }
  }
  // Done with the last space.
  object_iterator_.reset(nullptr);
  return HeapObject();
}

void Heap::UpdateTotalGCTime(double duration) {
  if (v8_flags.trace_gc_verbose) {
    total_gc_time_ms_ += duration;
  }
}

void Heap::ExternalStringTable::CleanUpYoung() {
  int last = 0;
  Isolate* isolate = heap_->isolate();
  for (size_t i = 0; i < young_strings_.size(); ++i) {
    Object o = young_strings_[i];
    if (o.IsTheHole(isolate)) {
      continue;
    }
    // The real external string is already in one of these vectors and was or
    // will be processed. Re-processing it will add a duplicate to the vector.
    if (o.IsThinString()) continue;
    DCHECK(o.IsExternalString());
    if (InYoungGeneration(o)) {
      young_strings_[last++] = o;
    } else {
      old_strings_.push_back(o);
    }
  }
  young_strings_.resize(last);
}

void Heap::ExternalStringTable::CleanUpAll() {
  CleanUpYoung();
  int last = 0;
  Isolate* isolate = heap_->isolate();
  for (size_t i = 0; i < old_strings_.size(); ++i) {
    Object o = old_strings_[i];
    if (o.IsTheHole(isolate)) {
      continue;
    }
    // The real external string is already in one of these vectors and was or
    // will be processed. Re-processing it will add a duplicate to the vector.
    if (o.IsThinString()) continue;
    DCHECK(o.IsExternalString());
    DCHECK(!InYoungGeneration(o));
    old_strings_[last++] = o;
  }
  old_strings_.resize(last);
  if (v8_flags.verify_heap && !v8_flags.enable_third_party_heap) {
    Verify();
  }
}

void Heap::ExternalStringTable::TearDown() {
  for (size_t i = 0; i < young_strings_.size(); ++i) {
    Object o = young_strings_[i];
    // Dont finalize thin strings.
    if (o.IsThinString()) continue;
    heap_->FinalizeExternalString(ExternalString::cast(o));
  }
  young_strings_.clear();
  for (size_t i = 0; i < old_strings_.size(); ++i) {
    Object o = old_strings_[i];
    // Dont finalize thin strings.
    if (o.IsThinString()) continue;
    heap_->FinalizeExternalString(ExternalString::cast(o));
  }
  old_strings_.clear();
}

void Heap::RememberUnmappedPage(Address page, bool compacted) {
  // Tag the page pointer to make it findable in the dump file.
  if (compacted) {
    page ^= 0xC1EAD & (Page::kPageSize - 1);  // Cleared.
  } else {
    page ^= 0x1D1ED & (Page::kPageSize - 1);  // I died.
  }
  remembered_unmapped_pages_[remembered_unmapped_pages_index_] = page;
  remembered_unmapped_pages_index_++;
  remembered_unmapped_pages_index_ %= kRememberedUnmappedPages;
}

size_t Heap::YoungArrayBufferBytes() {
  return array_buffer_sweeper()->YoungBytes();
}

size_t Heap::OldArrayBufferBytes() {
  return array_buffer_sweeper()->OldBytes();
}

StrongRootsEntry* Heap::RegisterStrongRoots(const char* label,
                                            FullObjectSlot start,
                                            FullObjectSlot end) {
  // We're either on the main thread, or in a background thread with an active
  // local heap.
  DCHECK(isolate()->CurrentLocalHeap()->IsRunning());

  base::MutexGuard guard(&strong_roots_mutex_);

  StrongRootsEntry* entry = new StrongRootsEntry(label);
  entry->start = start;
  entry->end = end;
  entry->prev = nullptr;
  entry->next = strong_roots_head_;

  if (strong_roots_head_) {
    DCHECK_NULL(strong_roots_head_->prev);
    strong_roots_head_->prev = entry;
  }
  strong_roots_head_ = entry;

  return entry;
}

void Heap::UpdateStrongRoots(StrongRootsEntry* entry, FullObjectSlot start,
                             FullObjectSlot end) {
  entry->start = start;
  entry->end = end;
}

void Heap::UnregisterStrongRoots(StrongRootsEntry* entry) {
  // We're either on the main thread, or in a background thread with an active
  // local heap.
  DCHECK(isolate()->CurrentLocalHeap()->IsRunning());

  base::MutexGuard guard(&strong_roots_mutex_);

  StrongRootsEntry* prev = entry->prev;
  StrongRootsEntry* next = entry->next;

  if (prev) prev->next = next;
  if (next) next->prev = prev;

  if (strong_roots_head_ == entry) {
    DCHECK_NULL(prev);
    strong_roots_head_ = next;
  }

  delete entry;
}

void Heap::SetBuiltinsConstantsTable(FixedArray cache) {
  set_builtins_constants_table(cache);
}

void Heap::SetDetachedContexts(WeakArrayList detached_contexts) {
  set_detached_contexts(detached_contexts);
}

void Heap::PostFinalizationRegistryCleanupTaskIfNeeded() {
  // Only one cleanup task is posted at a time.
  if (!HasDirtyJSFinalizationRegistries() ||
      is_finalization_registry_cleanup_task_posted_) {
    return;
  }
  auto taskrunner = V8::GetCurrentPlatform()->GetForegroundTaskRunner(
      reinterpret_cast<v8::Isolate*>(isolate()));
  auto task = std::make_unique<FinalizationRegistryCleanupTask>(this);
  taskrunner->PostNonNestableTask(std::move(task));
  is_finalization_registry_cleanup_task_posted_ = true;
}

void Heap::EnqueueDirtyJSFinalizationRegistry(
    JSFinalizationRegistry finalization_registry,
    std::function<void(HeapObject object, ObjectSlot slot, Object target)>
        gc_notify_updated_slot) {
  // Add a FinalizationRegistry to the tail of the dirty list.
  DCHECK(!HasDirtyJSFinalizationRegistries() ||
         dirty_js_finalization_registries_list().IsJSFinalizationRegistry());
  DCHECK(finalization_registry.next_dirty().IsUndefined(isolate()));
  DCHECK(!finalization_registry.scheduled_for_cleanup());
  finalization_registry.set_scheduled_for_cleanup(true);
  if (dirty_js_finalization_registries_list_tail().IsUndefined(isolate())) {
    DCHECK(dirty_js_finalization_registries_list().IsUndefined(isolate()));
    set_dirty_js_finalization_registries_list(finalization_registry);
    // dirty_js_finalization_registries_list_ is rescanned by
    // ProcessWeakListRoots.
  } else {
    JSFinalizationRegistry tail = JSFinalizationRegistry::cast(
        dirty_js_finalization_registries_list_tail());
    tail.set_next_dirty(finalization_registry);
    gc_notify_updated_slot(
        tail, tail.RawField(JSFinalizationRegistry::kNextDirtyOffset),
        finalization_registry);
  }
  set_dirty_js_finalization_registries_list_tail(finalization_registry);
  // dirty_js_finalization_registries_list_tail_ is rescanned by
  // ProcessWeakListRoots.
}

MaybeHandle<JSFinalizationRegistry> Heap::DequeueDirtyJSFinalizationRegistry() {
  // Take a FinalizationRegistry from the head of the dirty list for fairness.
  if (HasDirtyJSFinalizationRegistries()) {
    Handle<JSFinalizationRegistry> head(
        JSFinalizationRegistry::cast(dirty_js_finalization_registries_list()),
        isolate());
    set_dirty_js_finalization_registries_list(head->next_dirty());
    head->set_next_dirty(ReadOnlyRoots(this).undefined_value());
    if (*head == dirty_js_finalization_registries_list_tail()) {
      set_dirty_js_finalization_registries_list_tail(
          ReadOnlyRoots(this).undefined_value());
    }
    return head;
  }
  return {};
}

void Heap::RemoveDirtyFinalizationRegistriesOnContext(NativeContext context) {
  DisallowGarbageCollection no_gc;

  Isolate* isolate = this->isolate();
  Object prev = ReadOnlyRoots(isolate).undefined_value();
  Object current = dirty_js_finalization_registries_list();
  while (!current.IsUndefined(isolate)) {
    JSFinalizationRegistry finalization_registry =
        JSFinalizationRegistry::cast(current);
    if (finalization_registry.native_context() == context) {
      if (prev.IsUndefined(isolate)) {
        set_dirty_js_finalization_registries_list(
            finalization_registry.next_dirty());
      } else {
        JSFinalizationRegistry::cast(prev).set_next_dirty(
            finalization_registry.next_dirty());
      }
      finalization_registry.set_scheduled_for_cleanup(false);
      current = finalization_registry.next_dirty();
      finalization_registry.set_next_dirty(
          ReadOnlyRoots(isolate).undefined_value());
    } else {
      prev = current;
      current = finalization_registry.next_dirty();
    }
  }
  set_dirty_js_finalization_registries_list_tail(prev);
}

void Heap::KeepDuringJob(Handle<HeapObject> target) {
  DCHECK(weak_refs_keep_during_job().IsUndefined() ||
         weak_refs_keep_during_job().IsOrderedHashSet());
  Handle<OrderedHashSet> table;
  if (weak_refs_keep_during_job().IsUndefined(isolate())) {
    table = isolate()->factory()->NewOrderedHashSet();
  } else {
    table =
        handle(OrderedHashSet::cast(weak_refs_keep_during_job()), isolate());
  }
  table = OrderedHashSet::Add(isolate(), table, target).ToHandleChecked();
  set_weak_refs_keep_during_job(*table);
}

void Heap::ClearKeptObjects() {
  set_weak_refs_keep_during_job(ReadOnlyRoots(isolate()).undefined_value());
}

size_t Heap::NumberOfTrackedHeapObjectTypes() {
  return ObjectStats::OBJECT_STATS_COUNT;
}

size_t Heap::ObjectCountAtLastGC(size_t index) {
  if (live_object_stats_ == nullptr || index >= ObjectStats::OBJECT_STATS_COUNT)
    return 0;
  return live_object_stats_->object_count_last_gc(index);
}

size_t Heap::ObjectSizeAtLastGC(size_t index) {
  if (live_object_stats_ == nullptr || index >= ObjectStats::OBJECT_STATS_COUNT)
    return 0;
  return live_object_stats_->object_size_last_gc(index);
}

bool Heap::GetObjectTypeName(size_t index, const char** object_type,
                             const char** object_sub_type) {
  if (index >= ObjectStats::OBJECT_STATS_COUNT) return false;

  switch (static_cast<int>(index)) {
#define COMPARE_AND_RETURN_NAME(name) \
  case name:                          \
    *object_type = #name;             \
    *object_sub_type = "";            \
    return true;
    INSTANCE_TYPE_LIST(COMPARE_AND_RETURN_NAME)
#undef COMPARE_AND_RETURN_NAME

#define COMPARE_AND_RETURN_NAME(name)                       \
  case ObjectStats::FIRST_VIRTUAL_TYPE + ObjectStats::name: \
    *object_type = #name;                                   \
    *object_sub_type = "";                                  \
    return true;
    VIRTUAL_INSTANCE_TYPE_LIST(COMPARE_AND_RETURN_NAME)
#undef COMPARE_AND_RETURN_NAME
  }
  return false;
}

size_t Heap::NumberOfNativeContexts() {
  int result = 0;
  Object context = native_contexts_list();
  while (!context.IsUndefined(isolate())) {
    ++result;
    Context native_context = Context::cast(context);
    context = native_context.next_context_link();
  }
  return result;
}

std::vector<Handle<NativeContext>> Heap::FindAllNativeContexts() {
  std::vector<Handle<NativeContext>> result;
  Object context = native_contexts_list();
  while (!context.IsUndefined(isolate())) {
    NativeContext native_context = NativeContext::cast(context);
    result.push_back(handle(native_context, isolate()));
    context = native_context.next_context_link();
  }
  return result;
}

std::vector<WeakArrayList> Heap::FindAllRetainedMaps() {
  std::vector<WeakArrayList> result;
  Object context = native_contexts_list();
  while (!context.IsUndefined(isolate())) {
    NativeContext native_context = NativeContext::cast(context);
    result.push_back(WeakArrayList::cast(native_context.retained_maps()));
    context = native_context.next_context_link();
  }
  return result;
}

size_t Heap::NumberOfDetachedContexts() {
  // The detached_contexts() array has two entries per detached context.
  return detached_contexts().length() / 2;
}

bool Heap::AllowedToBeMigrated(Map map, HeapObject obj, AllocationSpace dst) {
  // Object migration is governed by the following rules:
  //
  // 1) Objects in new-space can be migrated to the old space
  //    that matches their target space or they stay in new-space.
  // 2) Objects in old-space stay in the same space when migrating.
  // 3) Fillers (two or more words) can migrate due to left-trimming of
  //    fixed arrays in new-space or old space.
  // 4) Fillers (one word) can never migrate, they are skipped by
  //    incremental marking explicitly to prevent invalid pattern.
  //
  // Since this function is used for debugging only, we do not place
  // asserts here, but check everything explicitly.
  if (map == ReadOnlyRoots(this).one_pointer_filler_map()) return false;
  InstanceType type = map.instance_type();
  MemoryChunk* chunk = MemoryChunk::FromHeapObject(obj);
  AllocationSpace src = chunk->owner_identity();
  switch (src) {
    case NEW_SPACE:
      return dst == NEW_SPACE || dst == OLD_SPACE;
    case OLD_SPACE:
      return dst == OLD_SPACE;
    case CODE_SPACE:
      return dst == CODE_SPACE && type == INSTRUCTION_STREAM_TYPE;
    case SHARED_SPACE:
      return dst == SHARED_SPACE;
    case LO_SPACE:
    case CODE_LO_SPACE:
    case NEW_LO_SPACE:
    case SHARED_LO_SPACE:
    case RO_SPACE:
      return false;
  }
  UNREACHABLE();
}

size_t Heap::EmbedderAllocationCounter() const {
  return cpp_heap_ ? CppHeap::From(cpp_heap_)->allocated_size() : 0;
}

void Heap::CreateObjectStats() {
  if (V8_LIKELY(!TracingFlags::is_gc_stats_enabled())) return;
  if (!live_object_stats_) {
    live_object_stats_.reset(new ObjectStats(this));
  }
  if (!dead_object_stats_) {
    dead_object_stats_.reset(new ObjectStats(this));
  }
}

Map Heap::GcSafeMapOfHeapObject(HeapObject object) {
  PtrComprCageBase cage_base(isolate());
  MapWord map_word = object.map_word(cage_base, kRelaxedLoad);
  if (map_word.IsForwardingAddress()) {
    return map_word.ToForwardingAddress(object).map(cage_base);
  }
  return map_word.ToMap();
}

GcSafeCode Heap::GcSafeGetCodeFromInstructionStream(
    HeapObject instruction_stream, Address inner_pointer) {
  InstructionStream istream =
      InstructionStream::unchecked_cast(instruction_stream);
  DCHECK(!istream.is_null());
  DCHECK(GcSafeInstructionStreamContains(istream, inner_pointer));
  return GcSafeCode::unchecked_cast(istream.raw_code(kAcquireLoad));
}

bool Heap::GcSafeInstructionStreamContains(InstructionStream istream,
                                           Address addr) {
  Map map = GcSafeMapOfHeapObject(istream);
  DCHECK_EQ(map, ReadOnlyRoots(this).instruction_stream_map());

  Builtin builtin_lookup_result =
      OffHeapInstructionStream::TryLookupCode(isolate(), addr);
  if (Builtins::IsBuiltinId(builtin_lookup_result)) {
    // Builtins don't have InstructionStream objects.
    DCHECK(!Builtins::IsBuiltinId(istream.code(kAcquireLoad).builtin_id()));
    return false;
  }

  Address start = istream.address();
  Address end = start + istream.SizeFromMap(map);
  return start <= addr && addr < end;
}

base::Optional<InstructionStream>
Heap::GcSafeTryFindInstructionStreamForInnerPointer(Address inner_pointer) {
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) {
    Address start = tp_heap_->GetObjectFromInnerPointer(inner_pointer);
    return InstructionStream::unchecked_cast(HeapObject::FromAddress(start));
  }

  base::Optional<Address> start =
      ThreadIsolation::StartOfJitAllocationAt(inner_pointer);
  if (start.has_value()) {
    return InstructionStream::unchecked_cast(HeapObject::FromAddress(*start));
  }

  return {};
}

base::Optional<GcSafeCode> Heap::GcSafeTryFindCodeForInnerPointer(
    Address inner_pointer) {
  Builtin maybe_builtin =
      OffHeapInstructionStream::TryLookupCode(isolate(), inner_pointer);
  if (Builtins::IsBuiltinId(maybe_builtin)) {
    return GcSafeCode::cast(isolate()->builtins()->code(maybe_builtin));
  }

  base::Optional<InstructionStream> maybe_istream =
      GcSafeTryFindInstructionStreamForInnerPointer(inner_pointer);
  if (!maybe_istream) return {};

  return GcSafeGetCodeFromInstructionStream(*maybe_istream, inner_pointer);
}

Code Heap::FindCodeForInnerPointer(Address inner_pointer) {
  return GcSafeFindCodeForInnerPointer(inner_pointer).UnsafeCastToCode();
}

GcSafeCode Heap::GcSafeFindCodeForInnerPointer(Address inner_pointer) {
  base::Optional<GcSafeCode> maybe_code =
      GcSafeTryFindCodeForInnerPointer(inner_pointer);
  // Callers expect that the code object is found.
  CHECK(maybe_code.has_value());
  return GcSafeCode::unchecked_cast(maybe_code.value());
}

base::Optional<Code> Heap::TryFindCodeForInnerPointerForPrinting(
    Address inner_pointer) {
  if (InSpaceSlow(inner_pointer, i::CODE_SPACE) ||
      InSpaceSlow(inner_pointer, i::CODE_LO_SPACE) ||
      i::OffHeapInstructionStream::PcIsOffHeap(isolate(), inner_pointer)) {
    base::Optional<GcSafeCode> maybe_code =
        GcSafeTryFindCodeForInnerPointer(inner_pointer);
    if (maybe_code.has_value()) {
      return maybe_code->UnsafeCastToCode();
    }
  }
  return {};
}

void Heap::CombinedGenerationalAndSharedBarrierSlow(HeapObject object,
                                                    Address slot,
                                                    HeapObject value) {
  MemoryChunk* value_chunk = MemoryChunk::FromHeapObject(value);

  if (value_chunk->InYoungGeneration()) {
    Heap::GenerationalBarrierSlow(object, slot, value);

  } else {
    DCHECK(value_chunk->InWritableSharedSpace());
    DCHECK(!object.InWritableSharedSpace());
    Heap::SharedHeapBarrierSlow(object, slot);
  }
}

void Heap::CombinedGenerationalAndSharedEphemeronBarrierSlow(
    EphemeronHashTable table, Address slot, HeapObject value) {
  MemoryChunk* value_chunk = MemoryChunk::FromHeapObject(value);

  if (value_chunk->InYoungGeneration()) {
    MemoryChunk* table_chunk = MemoryChunk::FromHeapObject(table);
    table_chunk->heap()->RecordEphemeronKeyWrite(table, slot);

  } else {
    DCHECK(value_chunk->InWritableSharedSpace());
    DCHECK(!table.InWritableSharedSpace());
    Heap::SharedHeapBarrierSlow(table, slot);
  }
}

void Heap::GenerationalBarrierSlow(HeapObject object, Address slot,
                                   HeapObject value) {
  MemoryChunk* chunk = MemoryChunk::FromHeapObject(object);
  if (LocalHeap::Current() == nullptr) {
    RememberedSet<OLD_TO_NEW>::Insert<AccessMode::NON_ATOMIC>(chunk, slot);
  } else {
    RememberedSet<OLD_TO_NEW_BACKGROUND>::Insert<AccessMode::ATOMIC>(chunk,
                                                                     slot);
  }
}

void Heap::SharedHeapBarrierSlow(HeapObject object, Address slot) {
  MemoryChunk* chunk = MemoryChunk::FromHeapObject(object);
  DCHECK(!chunk->InWritableSharedSpace());
  RememberedSet<OLD_TO_SHARED>::Insert<AccessMode::ATOMIC>(chunk, slot);
}

void Heap::RecordEphemeronKeyWrite(EphemeronHashTable table, Address slot) {
  ephemeron_remembered_set_->RecordEphemeronKeyWrite(table, slot);
}

void Heap::EphemeronKeyWriteBarrierFromCode(Address raw_object,
                                            Address key_slot_address,
                                            Isolate* isolate) {
  EphemeronHashTable table = EphemeronHashTable::cast(Object(raw_object));
  ObjectSlot key_slot(key_slot_address);
  CombinedEphemeronWriteBarrier(table, key_slot, *key_slot,
                                UPDATE_WRITE_BARRIER);
}

enum RangeWriteBarrierMode {
  kDoGenerationalOrShared = 1 << 0,
  kDoMarking = 1 << 1,
  kDoEvacuationSlotRecording = 1 << 2,
};

template <int kModeMask, typename TSlot>
void Heap::WriteBarrierForRangeImpl(MemoryChunk* source_page, HeapObject object,
                                    TSlot start_slot, TSlot end_slot) {
  // At least one of generational or marking write barrier should be requested.
  static_assert(kModeMask & (kDoGenerationalOrShared | kDoMarking));
  // kDoEvacuationSlotRecording implies kDoMarking.
  static_assert(!(kModeMask & kDoEvacuationSlotRecording) ||
                (kModeMask & kDoMarking));

  MarkingBarrier* marking_barrier = nullptr;

  if (kModeMask & kDoMarking) {
    marking_barrier = WriteBarrier::CurrentMarkingBarrier(object);
  }

  MarkCompactCollector* collector = this->mark_compact_collector();

  for (TSlot slot = start_slot; slot < end_slot; ++slot) {
    typename TSlot::TObject value = *slot;
    HeapObject value_heap_object;
    if (!value.GetHeapObject(&value_heap_object)) continue;

    if (kModeMask & kDoGenerationalOrShared) {
      if (Heap::InYoungGeneration(value_heap_object)) {
        RememberedSet<OLD_TO_NEW>::Insert<AccessMode::NON_ATOMIC>(
            source_page, slot.address());
      } else if (value_heap_object.InWritableSharedSpace()) {
        RememberedSet<OLD_TO_SHARED>::Insert<AccessMode::ATOMIC>(
            source_page, slot.address());
      }
    }

    if (kModeMask & kDoMarking) {
      marking_barrier->MarkValue(object, value_heap_object);
      if (kModeMask & kDoEvacuationSlotRecording) {
        collector->RecordSlot(source_page, HeapObjectSlot(slot),
                              value_heap_object);
      }
    }
  }
}

// Instantiate Heap::WriteBarrierForRange() for ObjectSlot and MaybeObjectSlot.
template void Heap::WriteBarrierForRange<ObjectSlot>(HeapObject object,
                                                     ObjectSlot start_slot,
                                                     ObjectSlot end_slot);
template void Heap::WriteBarrierForRange<MaybeObjectSlot>(
    HeapObject object, MaybeObjectSlot start_slot, MaybeObjectSlot end_slot);

template <typename TSlot>
void Heap::WriteBarrierForRange(HeapObject object, TSlot start_slot,
                                TSlot end_slot) {
  if (v8_flags.disable_write_barriers) return;
  MemoryChunk* source_page = MemoryChunk::FromHeapObject(object);
  base::Flags<RangeWriteBarrierMode> mode;

  if (!source_page->InYoungGeneration() &&
      !source_page->InWritableSharedSpace()) {
    mode |= kDoGenerationalOrShared;
  }

  if (incremental_marking()->IsMarking()) {
    mode |= kDoMarking;
    if (!source_page->ShouldSkipEvacuationSlotRecording()) {
      mode |= kDoEvacuationSlotRecording;
    }
  }

  switch (mode) {
    // Nothing to be done.
    case 0:
      return;

    // Generational only.
    case kDoGenerationalOrShared:
      return WriteBarrierForRangeImpl<kDoGenerationalOrShared>(
          source_page, object, start_slot, end_slot);
    // Marking, no evacuation slot recording.
    case kDoMarking:
      return WriteBarrierForRangeImpl<kDoMarking>(source_page, object,
                                                  start_slot, end_slot);
    // Marking with evacuation slot recording.
    case kDoMarking | kDoEvacuationSlotRecording:
      return WriteBarrierForRangeImpl<kDoMarking | kDoEvacuationSlotRecording>(
          source_page, object, start_slot, end_slot);

    // Generational and marking, no evacuation slot recording.
    case kDoGenerationalOrShared | kDoMarking:
      return WriteBarrierForRangeImpl<kDoGenerationalOrShared | kDoMarking>(
          source_page, object, start_slot, end_slot);

    // Generational and marking with evacuation slot recording.
    case kDoGenerationalOrShared | kDoMarking | kDoEvacuationSlotRecording:
      return WriteBarrierForRangeImpl<kDoGenerationalOrShared | kDoMarking |
                                      kDoEvacuationSlotRecording>(
          source_page, object, start_slot, end_slot);

    default:
      UNREACHABLE();
  }
}

void Heap::GenerationalBarrierForCodeSlow(InstructionStream host,
                                          RelocInfo* rinfo, HeapObject object) {
  DCHECK(InYoungGeneration(object));
  const MarkCompactCollector::RecordRelocSlotInfo info =
      MarkCompactCollector::ProcessRelocInfo(host, rinfo, object);

  base::MutexGuard write_scope(info.memory_chunk->mutex());
  RememberedSet<OLD_TO_NEW>::InsertTyped(info.memory_chunk, info.slot_type,
                                         info.offset);
}

bool Heap::PageFlagsAreConsistent(HeapObject object) {
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) {
    return true;
  }
  BasicMemoryChunk* chunk = BasicMemoryChunk::FromHeapObject(object);
  heap_internals::MemoryChunk* slim_chunk =
      heap_internals::MemoryChunk::FromHeapObject(object);

  // Slim chunk flags consistency.
  CHECK_EQ(chunk->InYoungGeneration(), slim_chunk->InYoungGeneration());
  CHECK_EQ(chunk->IsFlagSet(MemoryChunk::INCREMENTAL_MARKING),
           slim_chunk->IsMarking());

  AllocationSpace identity = chunk->owner()->identity();

  // Generation consistency.
  CHECK_EQ(identity == NEW_SPACE || identity == NEW_LO_SPACE,
           slim_chunk->InYoungGeneration());
  // Read-only consistency.
  CHECK_EQ(chunk->InReadOnlySpace(), slim_chunk->InReadOnlySpace());

  // Marking consistency.
  if (chunk->IsWritable()) {
    // RO_SPACE can be shared between heaps, so we can't use RO_SPACE objects to
    // find a heap. The exception is when the ReadOnlySpace is writeable, during
    // bootstrapping, so explicitly allow this case.
    Heap* heap = Heap::FromWritableHeapObject(object);
    CHECK_EQ(slim_chunk->IsMarking(), heap->incremental_marking()->IsMarking());
  } else {
    // Non-writable RO_SPACE must never have marking flag set.
    CHECK(!slim_chunk->IsMarking());
  }
  return true;
}

#ifdef DEBUG
void Heap::IncrementObjectCounters() {
  isolate_->counters()->objs_since_last_full()->Increment();
  isolate_->counters()->objs_since_last_young()->Increment();
}
#endif  // DEBUG

bool Heap::IsStressingScavenge() {
  return v8_flags.stress_scavenge > 0 && new_space();
}

void Heap::SetIsMarkingFlag(bool value) {
  isolate()->isolate_data()->is_marking_flag_ = value;
}

uint8_t* Heap::IsMarkingFlagAddress() {
  return &isolate()->isolate_data()->is_marking_flag_;
}

void Heap::SetIsMinorMarkingFlag(bool value) {
  isolate()->isolate_data()->is_minor_marking_flag_ = value;
}

uint8_t* Heap::IsMinorMarkingFlagAddress() {
  return &isolate()->isolate_data()->is_minor_marking_flag_;
}

// StrongRootBlocks are allocated as a block of addresses, prefixed with a
// StrongRootsEntry pointer:
//
//   | StrongRootsEntry*
//   | Address 1
//   | ...
//   | Address N
//
// The allocate method registers the range "Address 1" to "Address N" with the
// heap as a strong root array, saves that entry in StrongRootsEntry*, and
// returns a pointer to Address 1.
Address* StrongRootBlockAllocator::allocate(size_t n) {
  void* block = base::Malloc(sizeof(StrongRootsEntry*) + n * sizeof(Address));

  StrongRootsEntry** header = reinterpret_cast<StrongRootsEntry**>(block);
  Address* ret = reinterpret_cast<Address*>(reinterpret_cast<char*>(block) +
                                            sizeof(StrongRootsEntry*));

  memset(ret, kNullAddress, n * sizeof(Address));
  *header = heap_->RegisterStrongRoots(
      "StrongRootBlockAllocator", FullObjectSlot(ret), FullObjectSlot(ret + n));

  return ret;
}

void StrongRootBlockAllocator::deallocate(Address* p, size_t n) noexcept {
  // The allocate method returns a pointer to Address 1, so the deallocate
  // method has to offset that pointer back by sizeof(StrongRootsEntry*).
  void* block = reinterpret_cast<char*>(p) - sizeof(StrongRootsEntry*);
  StrongRootsEntry** header = reinterpret_cast<StrongRootsEntry**>(block);

  heap_->UnregisterStrongRoots(*header);

  base::Free(block);
}

#ifdef V8_ENABLE_ALLOCATION_TIMEOUT
void Heap::set_allocation_timeout(int allocation_timeout) {
  heap_allocator_.SetAllocationTimeout(allocation_timeout);
}
#endif  // V8_ENABLE_ALLOCATION_TIMEOUT

void Heap::FinishSweepingIfOutOfWork() {
  if (sweeper()->major_sweeping_in_progress() && v8_flags.concurrent_sweeping &&
      !sweeper()->AreMajorSweeperTasksRunning()) {
    // At this point we know that all concurrent sweeping tasks have run
    // out of work and quit: all pages are swept. The main thread still needs
    // to complete sweeping though.
    EnsureSweepingCompleted(SweepingForcedFinalizationMode::kV8Only);
  }
  if (cpp_heap()) {
    // Ensure that sweeping is also completed for the C++ managed heap, if one
    // exists and it's out of work.
    CppHeap::From(cpp_heap())->FinishSweepingIfOutOfWork();
  }
}

void Heap::EnsureSweepingCompleted(SweepingForcedFinalizationMode mode) {
  CompleteArrayBufferSweeping(this);

  if (sweeper()->sweeping_in_progress()) {
    sweeper()->EnsureMajorCompleted();

    if (v8_flags.minor_ms && new_space()) {
      TRACE_GC_EPOCH(tracer(), GCTracer::Scope::MINOR_MS_COMPLETE_SWEEPING,
                     ThreadKind::kMain);
      paged_new_space()->paged_space()->RefillFreeList();
    }

    TRACE_GC_EPOCH(tracer(), GCTracer::Scope::MC_COMPLETE_SWEEPING,
                   ThreadKind::kMain);
    old_space()->RefillFreeList();
    {
      CodePageHeaderModificationScope rwx_write_scope(
          "Updating per-page stats stored in page headers requires write "
          "access to Code page headers");
      code_space()->RefillFreeList();
    }
    if (shared_space()) {
      shared_space()->RefillFreeList();
    }

    tracer()->NotifyFullSweepingCompleted();

#ifdef VERIFY_HEAP
    if (v8_flags.verify_heap) {
      EvacuationVerifier verifier(this);
      verifier.Run();
    }
#endif
  }

  if (mode == SweepingForcedFinalizationMode::kUnifiedHeap && cpp_heap()) {
    // Ensure that sweeping is also completed for the C++ managed heap, if one
    // exists.
    CppHeap::From(cpp_heap())->FinishSweepingIfRunning();
    DCHECK(!CppHeap::From(cpp_heap())->sweeper().IsSweepingInProgress());
  }

  DCHECK_IMPLIES(
      mode == SweepingForcedFinalizationMode::kUnifiedHeap || !cpp_heap(),
      !tracer()->IsSweepingInProgress());
}

void Heap::EnsureYoungSweepingCompleted() {
  if (!sweeper()->minor_sweeping_in_progress()) return;

  TRACE_GC_EPOCH(tracer(), GCTracer::Scope::MINOR_MS_COMPLETE_SWEEPING,
                 ThreadKind::kMain);

  sweeper()->EnsureMinorCompleted();
  paged_new_space()->paged_space()->RefillFreeList();
  old_space()->RefillFreeList();

  tracer()->NotifyYoungSweepingCompleted();
}

void Heap::DrainSweepingWorklistForSpace(AllocationSpace space) {
  if (!sweeper()->sweeping_in_progress_for_space(space)) return;
  sweeper()->DrainSweepingWorklistForSpace(space);
}

EmbedderStackStateScope::EmbedderStackStateScope(Heap* heap, Origin origin,
                                                 StackState stack_state)
    : heap_(heap), old_stack_state_(heap_->embedder_stack_state_) {
  if (origin == kImplicitThroughTask && heap->overriden_stack_state()) {
    stack_state = *heap->overriden_stack_state();
  }

  heap_->embedder_stack_state_ = stack_state;
}

// static
EmbedderStackStateScope EmbedderStackStateScope::ExplicitScopeForTesting(
    Heap* heap, StackState stack_state) {
  return EmbedderStackStateScope(heap, Origin::kExplicitInvocation,
                                 stack_state);
}

EmbedderStackStateScope::~EmbedderStackStateScope() {
  heap_->embedder_stack_state_ = old_stack_state_;
}

CppClassNamesAsHeapObjectNameScope::CppClassNamesAsHeapObjectNameScope(
    v8::CppHeap* heap)
    : scope_(std::make_unique<cppgc::internal::ClassNameAsHeapObjectNameScope>(
          *CppHeap::From(heap))) {}

CppClassNamesAsHeapObjectNameScope::~CppClassNamesAsHeapObjectNameScope() =
    default;

#if V8_HEAP_USE_PTHREAD_JIT_WRITE_PROTECT || V8_HEAP_USE_PKU_JIT_WRITE_PROTECT

CodePageMemoryModificationScopeForDebugging::
    CodePageMemoryModificationScopeForDebugging(Heap* heap,
                                                VirtualMemory* reservation,
                                                base::AddressRegion region)
    : rwx_write_scope_("Write access for zapping.") {
#if !defined(DEBUG) && !defined(VERIFY_HEAP)
  UNREACHABLE();
#endif
}

CodePageMemoryModificationScopeForDebugging::
    CodePageMemoryModificationScopeForDebugging(BasicMemoryChunk* chunk)
    : rwx_write_scope_("Write access for zapping.") {
#if !defined(DEBUG) && !defined(VERIFY_HEAP)
  UNREACHABLE();
#endif
}

CodePageMemoryModificationScopeForDebugging::
    ~CodePageMemoryModificationScopeForDebugging() {}

#else  // V8_HEAP_USE_PTHREAD_JIT_WRITE_PROTECT ||
       // V8_HEAP_USE_PKU_JIT_WRITE_PROTECT

CodePageMemoryModificationScopeForDebugging::
    CodePageMemoryModificationScopeForDebugging(Heap* heap,
                                                VirtualMemory* reservation,
                                                base::AddressRegion region) {
#if !defined(DEBUG) && !defined(VERIFY_HEAP)
  UNREACHABLE();
#else
  if (heap->write_protect_code_memory()) {
    reservation_ = reservation;
    region_.emplace(region);
    CHECK(reservation_->SetPermissions(
        region_->begin(), region_->size(),
        MemoryChunk::GetCodeModificationPermission()));
  }
#endif
}

CodePageMemoryModificationScopeForDebugging::
    CodePageMemoryModificationScopeForDebugging(BasicMemoryChunk* chunk)
    : memory_modification_scope_(chunk) {
#if !defined(DEBUG) && !defined(VERIFY_HEAP)
  UNREACHABLE();
#endif
}

CodePageMemoryModificationScopeForDebugging::
    ~CodePageMemoryModificationScopeForDebugging() {
  if (reservation_) {
    DCHECK(region_.has_value());
    CHECK(reservation_->SetPermissions(
        region_->begin(), region_->size(),
        v8_flags.jitless ? PageAllocator::Permission::kRead
                         : PageAllocator::Permission::kReadExecute));
  }
}

#endif

}  // namespace internal
}  // namespace v8
