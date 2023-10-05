// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/paged-spaces.h"

#include <atomic>

#include "src/base/logging.h"
#include "src/base/optional.h"
#include "src/base/platform/mutex.h"
#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/execution/vm-state-inl.h"
#include "src/flags/flags.h"
#include "src/heap/allocation-observer.h"
#include "src/heap/array-buffer-sweeper.h"
#include "src/heap/free-list-inl.h"
#include "src/heap/gc-tracer-inl.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/heap.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/marking-state-inl.h"
#include "src/heap/memory-allocator.h"
#include "src/heap/memory-chunk-inl.h"
#include "src/heap/memory-chunk-layout.h"
#include "src/heap/page-inl.h"
#include "src/heap/paged-spaces-inl.h"
#include "src/heap/read-only-heap.h"
#include "src/heap/safepoint.h"
#include "src/heap/spaces.h"
#include "src/heap/sweeper.h"
#include "src/logging/runtime-call-stats-scope.h"
#include "src/objects/string.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {

// ----------------------------------------------------------------------------
// PagedSpaceObjectIterator

PagedSpaceObjectIterator::PagedSpaceObjectIterator(Heap* heap,
                                                   const PagedSpaceBase* space)
    : space_(space),
      page_range_(space->first_page(), nullptr),
      current_page_(page_range_.begin()) {
  heap->MakeHeapIterable();
  USE(space_);
}

// We have hit the end of the page and should advance to the next block of
// objects.  This happens at the end of the page.
bool PagedSpaceObjectIterator::AdvanceToNextPage() {
  if (current_page_ == page_range_.end()) return false;
  const Page* cur_page = *(current_page_++);
  HeapObjectRange heap_objects(cur_page);
  cur_ = heap_objects.begin();
  end_ = heap_objects.end();
  return true;
}

// ----------------------------------------------------------------------------
// PagedSpaceBase implementation

Page* PagedSpaceBase::InitializePage(MemoryChunk* chunk) {
  Page* page = static_cast<Page*>(chunk);
  DCHECK_EQ(
      MemoryChunkLayout::AllocatableMemoryInMemoryChunk(page->owner_identity()),
      page->area_size());
  // Make sure that categories are initialized before freeing the area.
  page->ResetAllocationStatistics();
  page->SetOldGenerationPageFlags(
      heap()->incremental_marking()->marking_mode());
  page->AllocateFreeListCategories();
  page->InitializeFreeListCategories();
  page->list_node().Initialize();
  page->InitializationMemoryFence();
  return page;
}

PagedSpaceBase::PagedSpaceBase(
    Heap* heap, AllocationSpace space, Executability executable,
    std::unique_ptr<FreeList> free_list,
    MainAllocator::AllocatorPolicyKind allocator_policy,
    CompactionSpaceKind compaction_space_kind,
    MainAllocator::SupportsExtendingLAB supports_extending_lab,
    LinearAllocationArea& allocation_info)
    : SpaceWithLinearArea(heap, space, std::move(free_list), allocator_policy,
                          compaction_space_kind, supports_extending_lab,
                          allocation_info),
      executable_(executable),
      compaction_space_kind_(compaction_space_kind) {
  area_size_ = MemoryChunkLayout::AllocatableMemoryInMemoryChunk(space);
  accounting_stats_.Clear();
}

PagedSpaceBase::PagedSpaceBase(Heap* heap, AllocationSpace space,
                               Executability executable,
                               std::unique_ptr<FreeList> free_list,
                               CompactionSpaceKind compaction_space_kind,
                               MainAllocator* allocator)
    : SpaceWithLinearArea(heap, space, std::move(free_list),
                          compaction_space_kind, allocator),
      executable_(executable),
      compaction_space_kind_(compaction_space_kind) {
  area_size_ = MemoryChunkLayout::AllocatableMemoryInMemoryChunk(space);
  accounting_stats_.Clear();
}

PagedSpaceBase::PagedSpaceBase(
    Heap* heap, AllocationSpace space, Executability executable,
    std::unique_ptr<FreeList> free_list,
    MainAllocator::AllocatorPolicyKind allocator_policy,
    CompactionSpaceKind compaction_space_kind,
    MainAllocator::SupportsExtendingLAB supports_extending_lab)
    : SpaceWithLinearArea(heap, space, std::move(free_list), allocator_policy,
                          compaction_space_kind, supports_extending_lab),
      executable_(executable),
      compaction_space_kind_(compaction_space_kind) {
  area_size_ = MemoryChunkLayout::AllocatableMemoryInMemoryChunk(space);
  accounting_stats_.Clear();
}

void PagedSpaceBase::TearDown() {
  while (!memory_chunk_list_.Empty()) {
    MemoryChunk* chunk = memory_chunk_list_.front();
    memory_chunk_list_.Remove(chunk);
    heap()->memory_allocator()->Free(MemoryAllocator::FreeMode::kImmediately,
                                     chunk);
  }
  accounting_stats_.Clear();
}

void PagedSpaceBase::MergeCompactionSpace(CompactionSpace* other) {
  base::MutexGuard guard(mutex());

  DCHECK_NE(NEW_SPACE, identity());
  DCHECK_NE(NEW_SPACE, other->identity());
  DCHECK(identity() == other->identity());

  // Unmerged fields:
  //   area_size_
  other->FreeLinearAllocationArea();

  // The linear allocation area of {other} should be destroyed now.
  DCHECK_EQ(kNullAddress, other->allocator_->top());
  DCHECK_EQ(kNullAddress, other->allocator_->limit());

  // Move over pages.
  for (auto it = other->begin(); it != other->end();) {
    Page* p = *(it++);

    // Ensure that pages are initialized before objects on it are discovered by
    // concurrent markers.
    p->InitializationMemoryFence();

    // Relinking requires the category to be unlinked.
    other->RemovePage(p);
    AddPage(p);
    DCHECK_IMPLIES(
        !p->IsFlagSet(Page::NEVER_ALLOCATE_ON_PAGE),
        p->AvailableInFreeList() == p->AvailableInFreeListFromAllocatedBytes());

    // TODO(leszeks): Here we should allocation step, but:
    //   1. Allocation groups are currently not handled properly by the sampling
    //      allocation profiler, and
    //   2. Observers might try to take the space lock, which isn't reentrant.
    // We'll have to come up with a better solution for allocation stepping
    // before shipping, which will likely be using LocalHeap.
  }
  for (auto p : other->GetNewPages()) {
    heap()->NotifyOldGenerationExpansion(identity(), p);
  }

  DCHECK_EQ(0u, other->Size());
  DCHECK_EQ(0u, other->Capacity());
}

size_t PagedSpaceBase::CommittedPhysicalMemory() const {
  if (!base::OS::HasLazyCommits()) {
    DCHECK_EQ(0, committed_physical_memory());
    return CommittedMemory();
  }
  CodePageHeaderModificationScope rwx_write_scope(
      "Updating high water mark for Code pages requires write access to "
      "the Code page headers");
  return committed_physical_memory();
}

void PagedSpaceBase::IncrementCommittedPhysicalMemory(size_t increment_value) {
  if (!base::OS::HasLazyCommits() || increment_value == 0) return;
  size_t old_value = committed_physical_memory_.fetch_add(
      increment_value, std::memory_order_relaxed);
  USE(old_value);
  DCHECK_LT(old_value, old_value + increment_value);
}

void PagedSpaceBase::DecrementCommittedPhysicalMemory(size_t decrement_value) {
  if (!base::OS::HasLazyCommits() || decrement_value == 0) return;
  size_t old_value = committed_physical_memory_.fetch_sub(
      decrement_value, std::memory_order_relaxed);
  USE(old_value);
  DCHECK_GT(old_value, old_value - decrement_value);
}

#if DEBUG
void PagedSpaceBase::VerifyCommittedPhysicalMemory() const {
  heap()->safepoint()->AssertActive();
  size_t size = 0;
  for (const Page* page : *this) {
    DCHECK(page->SweepingDone());
    size += page->CommittedPhysicalMemory();
  }
  // Ensure that the space's counter matches the sum of all page counters.
  DCHECK_EQ(size, CommittedPhysicalMemory());
}
#endif  // DEBUG

bool PagedSpaceBase::ContainsSlow(Address addr) const {
  Page* p = Page::FromAddress(addr);
  for (const Page* page : *this) {
    if (page == p) return true;
  }
  return false;
}

void PagedSpaceBase::RefineAllocatedBytesAfterSweeping(Page* page) {
  CHECK(page->SweepingDone());
  // The live_byte on the page was accounted in the space allocated
  // bytes counter. After sweeping allocated_bytes() contains the
  // accurate live byte count on the page.
  size_t old_counter = page->live_bytes();
  size_t new_counter = page->allocated_bytes();
  DCHECK_GE(old_counter, new_counter);
  if (old_counter > new_counter) {
    size_t counter_diff = old_counter - new_counter;
    if (identity() == NEW_SPACE) size_at_last_gc_ -= counter_diff;
    DecreaseAllocatedBytes(counter_diff, page);
  }
  page->SetLiveBytes(0);
}

Page* PagedSpaceBase::RemovePageSafe(int size_in_bytes) {
  base::MutexGuard guard(mutex());
  Page* page = free_list()->GetPageForSize(size_in_bytes);
  if (!page) return nullptr;
  RemovePage(page);
  return page;
}

void PagedSpaceBase::AddPageImpl(Page* page) {
  DCHECK_NOT_NULL(page);
  CHECK(page->SweepingDone());
  page->set_owner(this);
  DCHECK_IMPLIES(identity() == NEW_SPACE, page->IsFlagSet(Page::TO_PAGE));
  DCHECK_IMPLIES(identity() != NEW_SPACE, !page->IsFlagSet(Page::TO_PAGE));
  memory_chunk_list_.PushBack(page);
  AccountCommitted(page->size());
  IncreaseCapacity(page->area_size());
  IncreaseAllocatedBytes(page->allocated_bytes(), page);
  ForAll<ExternalBackingStoreType>(
      [this, page](ExternalBackingStoreType type, int index) {
        IncrementExternalBackingStoreBytes(
            type, page->ExternalBackingStoreBytes(type));
      });
  IncrementCommittedPhysicalMemory(page->CommittedPhysicalMemory());
}

size_t PagedSpaceBase::AddPage(Page* page) {
  AddPageImpl(page);
  return RelinkFreeListCategories(page);
}

void PagedSpaceBase::RemovePage(Page* page) {
  CHECK(page->SweepingDone());
  DCHECK_IMPLIES(identity() == NEW_SPACE, page->IsFlagSet(Page::TO_PAGE));
  memory_chunk_list_.Remove(page);
  UnlinkFreeListCategories(page);
  // Pages are only removed from new space when they are promoted to old space
  // during a GC. This happens after sweeping as started and the allocation
  // counters have been reset.
  DCHECK_IMPLIES(identity() == NEW_SPACE,
                 heap()->gc_state() != Heap::NOT_IN_GC);
  if (identity() == NEW_SPACE) {
    page->ReleaseFreeListCategories();
  } else {
    DecreaseAllocatedBytes(page->allocated_bytes(), page);
    free_list()->decrease_wasted_bytes(page->wasted_memory());
  }
  DecreaseCapacity(page->area_size());
  AccountUncommitted(page->size());
  ForAll<ExternalBackingStoreType>(
      [this, page](ExternalBackingStoreType type, int index) {
        DecrementExternalBackingStoreBytes(
            type, page->ExternalBackingStoreBytes(type));
      });
  DecrementCommittedPhysicalMemory(page->CommittedPhysicalMemory());
}

size_t PagedSpaceBase::ShrinkPageToHighWaterMark(Page* page) {
  size_t unused = page->ShrinkToHighWaterMark();
  accounting_stats_.DecreaseCapacity(static_cast<intptr_t>(unused));
  AccountUncommitted(unused);
  return unused;
}

void PagedSpaceBase::ResetFreeList() {
  for (Page* page : *this) {
    free_list_->EvictFreeListItems(page);
  }
  DCHECK(free_list_->IsEmpty());
  DCHECK_EQ(0, free_list_->Available());
}

void PagedSpaceBase::ShrinkImmortalImmovablePages() {
  base::Optional<CodePageHeaderModificationScope> optional_scope;
  if (identity() == CODE_SPACE) {
    optional_scope.emplace(
        "ShrinkImmortalImmovablePages writes to the page header.");
  }
  DCHECK(!heap()->deserialization_complete());
  ResetFreeList();
  for (Page* page : *this) {
    DCHECK(page->IsFlagSet(Page::NEVER_EVACUATE));
    ShrinkPageToHighWaterMark(page);
  }
}

Page* PagedSpaceBase::TryExpandImpl(
    MemoryAllocator::AllocationMode allocation_mode) {
  base::MutexGuard expansion_guard(heap_->heap_expansion_mutex());
  const size_t accounted_size =
      MemoryChunkLayout::AllocatableMemoryInMemoryChunk(identity());
  if (identity() != NEW_SPACE && !is_compaction_space() &&
      !heap()->IsOldGenerationExpansionAllowed(accounted_size,
                                               expansion_guard)) {
    return nullptr;
  }
  Page* page = heap()->memory_allocator()->AllocatePage(allocation_mode, this,
                                                        executable());
  if (page == nullptr) return nullptr;
  DCHECK_EQ(page->area_size(), accounted_size);
  ConcurrentAllocationMutex guard(this);
  AddPage(page);
  Free(page->area_start(), page->area_size(),
       SpaceAccountingMode::kSpaceAccounted);
  return page;
}

base::Optional<std::pair<Address, size_t>> PagedSpaceBase::TryExpandBackground(
    size_t size_in_bytes) {
  DCHECK_NE(NEW_SPACE, identity());
  base::MutexGuard expansion_guard(heap_->heap_expansion_mutex());
  const size_t accounted_size =
      MemoryChunkLayout::AllocatableMemoryInMemoryChunk(identity());
  if (!heap()->IsOldGenerationExpansionAllowed(accounted_size,
                                               expansion_guard)) {
    return {};
  }
  Page* page = heap()->memory_allocator()->AllocatePage(
      MemoryAllocator::AllocationMode::kRegular, this, executable());
  if (page == nullptr) return {};
  DCHECK_EQ(page->area_size(), accounted_size);
  base::MutexGuard lock(&space_mutex_);
  AddPage(page);
  heap()->NotifyOldGenerationExpansionBackground(identity(), page);
  Address object_start = page->area_start();
  CHECK_LE(size_in_bytes, page->area_size());
  Free(page->area_start() + size_in_bytes, page->area_size() - size_in_bytes,
       SpaceAccountingMode::kSpaceAccounted);
  AddRangeToActiveSystemPages(page, object_start, object_start + size_in_bytes);
  return std::make_pair(object_start, size_in_bytes);
}

int PagedSpaceBase::CountTotalPages() const {
  int count = 0;
  for (const Page* page : *this) {
    count++;
    USE(page);
  }
  return count;
}

void PagedSpaceBase::SetLinearAllocationArea(Address top, Address limit,
                                             Address end) {
  allocator_->ResetLab(top, limit, end);
  if (top != kNullAddress && top != limit) {
    Page* page = Page::FromAllocationAreaAddress(top);
    if ((identity() != NEW_SPACE) &&
        heap()->incremental_marking()->black_allocation()) {
      page->CreateBlackArea(top, limit);
    }
  }
}

void PagedSpaceBase::DecreaseLimit(Address new_limit) {
  Address old_limit = allocator_->limit();
  DCHECK_LE(allocator_->top(), new_limit);
  DCHECK_GE(old_limit, new_limit);
  if (new_limit != old_limit) {
    base::Optional<CodePageHeaderModificationScope> optional_scope;
    if (identity() == CODE_SPACE) {
      optional_scope.emplace("DecreaseLimit writes to the page header.");
    }

    ConcurrentAllocationMutex guard(this);
    Address old_max_limit = allocator_->original_limit_relaxed();
    if (!allocator_->supports_extending_lab()) {
      DCHECK_EQ(old_max_limit, old_limit);
      allocator_->ResetLab(allocator_->top(), new_limit, new_limit);
      Free(new_limit, old_max_limit - new_limit,
           SpaceAccountingMode::kSpaceAccounted);
    } else {
      allocator_->ExtendLAB(new_limit);
      heap()->CreateFillerObjectAt(new_limit,
                                   static_cast<int>(old_max_limit - new_limit));
    }
    if (heap()->incremental_marking()->black_allocation() &&
        identity() != NEW_SPACE) {
      Page::FromAllocationAreaAddress(new_limit)->DestroyBlackArea(new_limit,
                                                                   old_limit);
    }
  }
}

size_t PagedSpaceBase::Available() const {
  ConcurrentAllocationMutex guard(this);
  return free_list_->Available();
}

void PagedSpaceBase::FreeLinearAllocationArea() {
  // Mark the old linear allocation area with a free space map so it can be
  // skipped when scanning the heap.
  Address current_top = allocator_->top();
  Address current_limit = allocator_->limit();
  if (current_top == kNullAddress) {
    DCHECK_EQ(kNullAddress, current_limit);
    return;
  }
  Address current_max_limit = allocator_->original_limit_relaxed();
  DCHECK_IMPLIES(!allocator_->supports_extending_lab(),
                 current_max_limit == current_limit);

  allocator_->AdvanceAllocationObservers();

  base::Optional<CodePageHeaderModificationScope> optional_scope;
  if (identity() == CODE_SPACE) {
    optional_scope.emplace(
        "FreeLinearAllocationArea writes to the page header.");
  }

  if (identity() != NEW_SPACE && current_top != current_limit &&
      heap()->incremental_marking()->black_allocation()) {
    Page::FromAddress(current_top)
        ->DestroyBlackArea(current_top, current_limit);
  }

  allocator_->ResetLab(kNullAddress, kNullAddress, kNullAddress);
  DCHECK_GE(current_limit, current_top);

  DCHECK_IMPLIES(current_limit - current_top >= 2 * kTaggedSize,
                 heap()->marking_state()->IsUnmarked(
                     HeapObject::FromAddress(current_top)));
  Free(current_top, current_max_limit - current_top,
       SpaceAccountingMode::kSpaceAccounted);
}

void PagedSpaceBase::ReleasePage(Page* page) {
  ReleasePageImpl(page, MemoryAllocator::FreeMode::kConcurrently);
}

void PagedSpaceBase::ReleasePageImpl(Page* page,
                                     MemoryAllocator::FreeMode free_mode) {
  DCHECK(page->SweepingDone());
  DCHECK_EQ(0, page->live_bytes());
  DCHECK_EQ(page->owner(), this);

  DCHECK_IMPLIES(identity() == NEW_SPACE, page->IsFlagSet(Page::TO_PAGE));

  memory_chunk_list().Remove(page);

  free_list_->EvictFreeListItems(page);

  if (identity() == CODE_SPACE) {
    heap()->isolate()->RemoveCodeMemoryChunk(page);
  }

  AccountUncommitted(page->size());
  DecrementCommittedPhysicalMemory(page->CommittedPhysicalMemory());
  accounting_stats_.DecreaseCapacity(page->area_size());
  heap()->memory_allocator()->Free(free_mode, page);
}

void PagedSpaceBase::SetReadable() {
  DCHECK(identity() == CODE_SPACE);
  for (Page* page : *this) {
    DCHECK(heap()->memory_allocator()->IsMemoryChunkExecutable(page));
    page->SetReadable();
  }
}

void PagedSpaceBase::SetReadAndExecutable() {
  DCHECK(identity() == CODE_SPACE);
  for (Page* page : *this) {
    DCHECK(heap()->memory_allocator()->IsMemoryChunkExecutable(page));
    page->SetReadAndExecutable();
  }
}

std::unique_ptr<ObjectIterator> PagedSpaceBase::GetObjectIterator(Heap* heap) {
  return std::unique_ptr<ObjectIterator>(
      new PagedSpaceObjectIterator(heap, this));
}

bool PagedSpaceBase::TryAllocationFromFreeListMain(size_t size_in_bytes,
                                                   AllocationOrigin origin) {
  ConcurrentAllocationMutex guard(this);
  DCHECK(IsAligned(size_in_bytes, kTaggedSize));
  DCHECK_LE(allocator_->top(), allocator_->limit());
#ifdef DEBUG
  if (allocator_->top() != allocator_->limit()) {
    DCHECK_EQ(Page::FromAddress(allocator_->top()),
              Page::FromAddress(allocator_->limit() - 1));
  }
#endif
  // Don't free list allocate if there is linear space available.
  DCHECK_LT(static_cast<size_t>(allocator_->limit() - allocator_->top()),
            size_in_bytes);

  // Mark the old linear allocation area with a free space map so it can be
  // skipped when scanning the heap.  This also puts it back in the free list
  // if it is big enough.
  FreeLinearAllocationArea();

  size_t new_node_size = 0;
  Tagged<FreeSpace> new_node =
      free_list_->Allocate(size_in_bytes, &new_node_size, origin);
  if (new_node.is_null()) return false;
  DCHECK_GE(new_node_size, size_in_bytes);

  // The old-space-step might have finished sweeping and restarted marking.
  // Verify that it did not turn the page of the new node into an evacuation
  // candidate.
  DCHECK(!MarkCompactCollector::IsOnEvacuationCandidate(new_node));

  // Memory in the linear allocation area is counted as allocated.  We may free
  // a little of this again immediately - see below.
  Page* page = Page::FromHeapObject(new_node);
  IncreaseAllocatedBytes(new_node_size, page);

  DCHECK_EQ(allocator_->allocation_info().start(),
            allocator_->allocation_info().top());
  Address start = new_node.address();
  Address end = new_node.address() + new_node_size;
  Address limit = allocator_->ComputeLimit(start, end, size_in_bytes);
  DCHECK_LE(limit, end);
  DCHECK_LE(size_in_bytes, limit - start);
  if (limit != end) {
    if (!allocator_->supports_extending_lab()) {
      Free(limit, end - limit, SpaceAccountingMode::kSpaceAccounted);
      end = limit;
    } else {
      DCHECK(heap()->IsMainThread());
      heap()->CreateFillerObjectAt(limit, static_cast<int>(end - limit));
    }
  }
  SetLinearAllocationArea(start, limit, end);
  AddRangeToActiveSystemPages(page, start, limit);

  return true;
}

#ifdef DEBUG
void PagedSpaceBase::Print() {}
#endif

#ifdef VERIFY_HEAP
void PagedSpaceBase::Verify(Isolate* isolate,
                            SpaceVerificationVisitor* visitor) const {
  CHECK_IMPLIES(identity() != NEW_SPACE, size_at_last_gc_ == 0);

  bool allocation_pointer_found_in_space =
      (allocator_->allocation_info().top() ==
       allocator_->allocation_info().limit());
  size_t external_space_bytes[static_cast<int>(
      ExternalBackingStoreType::kNumValues)] = {0};
  PtrComprCageBase cage_base(isolate);
  for (const Page* page : *this) {
    size_t external_page_bytes[static_cast<int>(
        ExternalBackingStoreType::kNumValues)] = {0};

    CHECK_EQ(page->owner(), this);
    CHECK_IMPLIES(identity() != NEW_SPACE, page->AllocatedLabSize() == 0);
    visitor->VerifyPage(page);

    if (page ==
        Page::FromAllocationAreaAddress(allocator_->allocation_info().top())) {
      allocation_pointer_found_in_space = true;
    }
    CHECK(page->SweepingDone());
    Address end_of_previous_object = page->area_start();
    Address top = page->area_end();

    for (Tagged<HeapObject> object : HeapObjectRange(page)) {
      CHECK(end_of_previous_object <= object.address());

      // Invoke verification method for each object.
      visitor->VerifyObject(object);

      // All the interior pointers should be contained in the heap.
      int size = object->Size(cage_base);
      CHECK(object.address() + size <= top);
      end_of_previous_object = object.address() + size;

      if (IsExternalString(object, cage_base)) {
        Tagged<ExternalString> external_string = ExternalString::cast(object);
        size_t payload_size = external_string->ExternalPayloadSize();
        external_page_bytes[static_cast<int>(
            ExternalBackingStoreType::kExternalString)] += payload_size;
      }
    }
    ForAll<ExternalBackingStoreType>(
        [page, external_page_bytes, &external_space_bytes](
            ExternalBackingStoreType type, int index) {
          CHECK_EQ(external_page_bytes[index],
                   page->ExternalBackingStoreBytes(type));
          external_space_bytes[index] += external_page_bytes[index];
        });

    visitor->VerifyPageDone(page);
  }
  ForAll<ExternalBackingStoreType>(
      [this, external_space_bytes](ExternalBackingStoreType type, int index) {
        if (type == ExternalBackingStoreType::kArrayBuffer) {
          return;
        }
        CHECK_EQ(external_space_bytes[index], ExternalBackingStoreBytes(type));
      });
  CHECK(allocation_pointer_found_in_space);

  if (!v8_flags.concurrent_array_buffer_sweeping) {
    if (identity() == OLD_SPACE) {
      size_t bytes = heap()->array_buffer_sweeper()->old().BytesSlow();
      CHECK_EQ(bytes, ExternalBackingStoreBytes(
                          ExternalBackingStoreType::kArrayBuffer));
    } else if (identity() == NEW_SPACE) {
      DCHECK(v8_flags.minor_ms);
      size_t bytes = heap()->array_buffer_sweeper()->young().BytesSlow();
      CHECK_EQ(bytes, ExternalBackingStoreBytes(
                          ExternalBackingStoreType::kArrayBuffer));
    }
  }

#ifdef DEBUG
  VerifyCountersAfterSweeping(isolate->heap());
#endif
}

void PagedSpaceBase::VerifyLiveBytes() const {
  MarkingState* marking_state = heap()->marking_state();
  PtrComprCageBase cage_base(heap()->isolate());
  for (const Page* page : *this) {
    CHECK(page->SweepingDone());
    int black_size = 0;
    for (Tagged<HeapObject> object : HeapObjectRange(page)) {
      // All the interior pointers should be contained in the heap.
      if (marking_state->IsMarked(object)) {
        black_size += object->Size(cage_base);
      }
    }
    CHECK_LE(black_size, page->live_bytes());
  }
}
#endif  // VERIFY_HEAP

#ifdef DEBUG
void PagedSpaceBase::VerifyCountersAfterSweeping(Heap* heap) const {
  size_t total_capacity = 0;
  size_t total_allocated = 0;
  PtrComprCageBase cage_base(heap->isolate());
  for (const Page* page : *this) {
    DCHECK(page->SweepingDone());
    total_capacity += page->area_size();
    size_t real_allocated = 0;
    for (Tagged<HeapObject> object : HeapObjectRange(page)) {
      if (!IsFreeSpaceOrFiller(object)) {
        real_allocated +=
            ALIGN_TO_ALLOCATION_ALIGNMENT(object->Size(cage_base));
      }
    }
    total_allocated += page->allocated_bytes();
    // The real size can be smaller than the accounted size if array trimming,
    // object slack tracking happened after sweeping.
    DCHECK_LE(real_allocated, accounting_stats_.AllocatedOnPage(page));
    DCHECK_EQ(page->allocated_bytes(), accounting_stats_.AllocatedOnPage(page));
  }
  DCHECK_EQ(total_capacity, accounting_stats_.Capacity());
  DCHECK_EQ(total_allocated, accounting_stats_.Size());
}

void PagedSpaceBase::VerifyCountersBeforeConcurrentSweeping() const {
  size_t total_capacity = 0;
  size_t total_allocated = 0;
  for (const Page* page : *this) {
    size_t page_allocated =
        page->SweepingDone() ? page->allocated_bytes() : page->live_bytes();
    total_capacity += page->area_size();
    total_allocated += page_allocated;
    DCHECK_EQ(page_allocated, accounting_stats_.AllocatedOnPage(page));
  }
  DCHECK_EQ(total_capacity, accounting_stats_.Capacity());
  DCHECK_EQ(total_allocated, accounting_stats_.Size());
}
#endif

void PagedSpaceBase::UpdateInlineAllocationLimit() {
  // Ensure there are no unaccounted allocations.
  DCHECK_EQ(allocator_->allocation_info().start(),
            allocator_->allocation_info().top());

  Address new_limit =
      allocator_->ComputeLimit(allocator_->top(), allocator_->limit(), 0);
  DCHECK_LE(allocator_->top(), new_limit);
  DCHECK_LE(new_limit, allocator_->limit());
  DecreaseLimit(new_limit);
}

bool PagedSpaceBase::EnsureAllocation(int size_in_bytes,
                                      AllocationAlignment alignment,
                                      AllocationOrigin origin,
                                      int* out_max_aligned_size) {
  if (!is_compaction_space() &&
      !((identity() == NEW_SPACE) && heap_->ShouldOptimizeForLoadTime())) {
    // Start incremental marking before the actual allocation, this allows the
    // allocation function to mark the object black when incremental marking is
    // running.
    heap()->StartIncrementalMarkingIfAllocationLimitIsReached(
        heap()->GCFlagsForIncrementalMarking(),
        kGCCallbackScheduleIdleGarbageCollection);
  }
  if (identity() == NEW_SPACE && heap()->incremental_marking()->IsStopped()) {
    heap()->StartMinorMSIncrementalMarkingIfNeeded();
  }

  // We don't know exactly how much filler we need to align until space is
  // allocated, so assume the worst case.
  size_in_bytes += Heap::GetMaximumFillToAlign(alignment);
  if (out_max_aligned_size) {
    *out_max_aligned_size = size_in_bytes;
  }
  if (allocator_->allocation_info().top() + size_in_bytes <=
      allocator_->allocation_info().limit()) {
    return true;
  }
  return RefillLabMain(size_in_bytes, origin);
}

bool PagedSpaceBase::RefillLabMain(int size_in_bytes, AllocationOrigin origin) {
  VMState<GC> state(heap()->isolate());
  RCS_SCOPE(heap()->isolate(),
            RuntimeCallCounterId::kGC_Custom_SlowAllocateRaw);
  return RawRefillLabMain(size_in_bytes, origin);
}

bool PagedSpaceBase::TryExpand(int size_in_bytes, AllocationOrigin origin) {
  DCHECK_NE(NEW_SPACE, identity());
  base::Optional<CodePageHeaderModificationScope> optional_scope;
  if (identity() == CODE_SPACE) {
    optional_scope.emplace("TryExpand writes to the page header.");
  }
  Page* page = TryExpandImpl(MemoryAllocator::AllocationMode::kRegular);
  if (!page) return false;
  if (!is_compaction_space() && identity() != NEW_SPACE) {
    heap()->NotifyOldGenerationExpansion(identity(), page);
  }
  return TryAllocationFromFreeListMain(static_cast<size_t>(size_in_bytes),
                                       origin);
}

bool PagedSpaceBase::TryExtendLAB(int size_in_bytes) {
  Address current_top = allocator_->top();
  if (current_top == kNullAddress) return false;
  Address current_limit = allocator_->limit();
  Address max_limit = allocator_->original_limit_relaxed();
  if (current_top + size_in_bytes > max_limit) {
    return false;
  }
  DCHECK(allocator_->supports_extending_lab());
  allocator_->AdvanceAllocationObservers();
  Address new_limit =
      allocator_->ComputeLimit(current_top, max_limit, size_in_bytes);
  allocator_->ExtendLAB(new_limit);
  DCHECK(heap()->IsMainThread());
  heap()->CreateFillerObjectAt(new_limit,
                               static_cast<int>(max_limit - new_limit));
  Page* page = Page::FromAddress(current_top);
  // No need to create a black allocation area since new space doesn't use
  // black allocation.
  DCHECK_EQ(NEW_SPACE, identity());
  AddRangeToActiveSystemPages(page, current_limit, new_limit);
  return true;
}

bool PagedSpaceBase::RawRefillLabMain(int size_in_bytes,
                                      AllocationOrigin origin) {
  // Allocation in this space has failed.
  DCHECK_GE(size_in_bytes, 0);

  if (TryExtendLAB(size_in_bytes)) return true;

  static constexpr int kMaxPagesToSweep = 1;

  if (TryAllocationFromFreeListMain(size_in_bytes, origin)) return true;

  const bool is_main_thread =
      heap()->IsMainThread() || heap()->IsSharedMainThread();
  const auto sweeping_scope_kind =
      is_main_thread ? ThreadKind::kMain : ThreadKind::kBackground;
  const auto sweeping_scope_id =
      heap()->sweeper()->GetTracingScope(identity(), is_main_thread);
  // Sweeping is still in progress.
  if (heap()->sweeping_in_progress()) {
    // First try to refill the free-list, concurrent sweeper threads
    // may have freed some objects in the meantime.
    if (heap()->sweeper()->ShouldRefillFreelistForSpace(identity())) {
      {
        TRACE_GC_EPOCH_WITH_FLOW(
            heap()->tracer(), sweeping_scope_id, sweeping_scope_kind,
            heap()->sweeper()->GetTraceIdForFlowEvent(sweeping_scope_id),
            TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
        RefillFreeList();
      }

      // Retry the free list allocation.
      if (TryAllocationFromFreeListMain(static_cast<size_t>(size_in_bytes),
                                        origin))
        return true;
    }

    if (ContributeToSweepingMain(size_in_bytes, kMaxPagesToSweep, size_in_bytes,
                                 origin, sweeping_scope_id,
                                 sweeping_scope_kind))
      return true;
  }

  if (is_compaction_space()) {
    DCHECK_NE(NEW_SPACE, identity());
    // The main thread may have acquired all swept pages. Try to steal from
    // it. This can only happen during young generation evacuation.
    PagedSpaceBase* main_space = heap()->paged_space(identity());
    Page* page = main_space->RemovePageSafe(size_in_bytes);
    if (page != nullptr) {
      AddPage(page);
      if (TryAllocationFromFreeListMain(static_cast<size_t>(size_in_bytes),
                                        origin))
        return true;
    }
  }

  if (identity() != NEW_SPACE &&
      heap()->ShouldExpandOldGenerationOnSlowAllocation(
          heap()->main_thread_local_heap(), origin) &&
      heap()->CanExpandOldGeneration(AreaSize())) {
    if (TryExpand(size_in_bytes, origin)) {
      return true;
    }
  }

  // Try sweeping all pages.
  if (ContributeToSweepingMain(0, 0, size_in_bytes, origin, sweeping_scope_id,
                               sweeping_scope_kind))
    return true;

  if (identity() != NEW_SPACE && heap()->gc_state() != Heap::NOT_IN_GC &&
      !heap()->force_oom()) {
    // Avoid OOM crash in the GC in order to invoke NearHeapLimitCallback after
    // GC and give it a chance to increase the heap limit.
    return TryExpand(size_in_bytes, origin);
  }
  return false;
}

bool PagedSpaceBase::ContributeToSweepingMain(
    int required_freed_bytes, int max_pages, int size_in_bytes,
    AllocationOrigin origin, GCTracer::Scope::ScopeId sweeping_scope_id,
    ThreadKind sweeping_scope_kind) {
  if (!heap()->sweeping_in_progress_for_space(identity())) return false;
  if (!(identity() == NEW_SPACE
            ? heap()->sweeper()->AreMinorSweeperTasksRunning()
            : heap()->sweeper()->AreMajorSweeperTasksRunning()) &&
      heap()->sweeper()->IsSweepingDoneForSpace(identity()))
    return false;

  TRACE_GC_EPOCH_WITH_FLOW(
      heap()->tracer(), sweeping_scope_id, sweeping_scope_kind,
      heap()->sweeper()->GetTraceIdForFlowEvent(sweeping_scope_id),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  // Cleanup invalidated old-to-new refs for compaction space in the
  // final atomic pause.
  Sweeper::SweepingMode sweeping_mode =
      is_compaction_space() ? Sweeper::SweepingMode::kEagerDuringGC
                            : Sweeper::SweepingMode::kLazyOrConcurrent;

  heap()->sweeper()->ParallelSweepSpace(identity(), sweeping_mode,
                                        required_freed_bytes, max_pages);
  RefillFreeList();
  return TryAllocationFromFreeListMain(size_in_bytes, origin);
}

void PagedSpaceBase::AddRangeToActiveSystemPages(Page* page, Address start,
                                                 Address end) {
  DCHECK_LE(page->address(), start);
  DCHECK_LT(start, end);
  DCHECK_LE(end, page->address() + Page::kPageSize);

  const size_t added_pages = page->active_system_pages()->Add(
      start - page->address(), end - page->address(),
      MemoryAllocator::GetCommitPageSizeBits());

  IncrementCommittedPhysicalMemory(added_pages *
                                   MemoryAllocator::GetCommitPageSize());
}

void PagedSpaceBase::ReduceActiveSystemPages(
    Page* page, ActiveSystemPages active_system_pages) {
  const size_t reduced_pages =
      page->active_system_pages()->Reduce(active_system_pages);
  DecrementCommittedPhysicalMemory(reduced_pages *
                                   MemoryAllocator::GetCommitPageSize());
}

void PagedSpaceBase::UnlinkFreeListCategories(Page* page) {
  DCHECK_EQ(this, page->owner());
  page->ForAllFreeListCategories([this](FreeListCategory* category) {
    free_list()->RemoveCategory(category);
  });
}

size_t PagedSpaceBase::RelinkFreeListCategories(Page* page) {
  DCHECK_EQ(this, page->owner());
  size_t added = 0;
  page->ForAllFreeListCategories([this, &added](FreeListCategory* category) {
    added += category->available();
    category->Relink(free_list());
  });
  free_list()->increase_wasted_bytes(page->wasted_memory());

  DCHECK_IMPLIES(!page->IsFlagSet(Page::NEVER_ALLOCATE_ON_PAGE),
                 page->AvailableInFreeList() ==
                     page->AvailableInFreeListFromAllocatedBytes());
  return added;
}

namespace {

void DropFreeListCategories(Page* page, FreeList* free_list) {
  size_t previously_available = 0;
  page->ForAllFreeListCategories(
      [free_list, &previously_available](FreeListCategory* category) {
        previously_available += category->available();
        category->Reset(free_list);
      });
  page->add_wasted_memory(previously_available);
}

}  // namespace

void PagedSpaceBase::RefillFreeList() {
  // Any PagedSpace might invoke RefillFreeList.
  DCHECK(identity() == OLD_SPACE || identity() == CODE_SPACE ||
         identity() == SHARED_SPACE || identity() == NEW_SPACE ||
         identity() == TRUSTED_SPACE);
  DCHECK_IMPLIES(
      identity() == NEW_SPACE,
      heap_->IsMainThread() || (heap_->IsSharedMainThread() &&
                                !heap_->isolate()->is_shared_space_isolate()));
  DCHECK(!is_compaction_space());

  for (Page* p : heap()->sweeper()->GetAllSweptPagesSafe(this)) {
    // We regularly sweep NEVER_ALLOCATE_ON_PAGE pages. We drop the freelist
    // entries here to make them unavailable for allocations.
    if (p->IsFlagSet(Page::NEVER_ALLOCATE_ON_PAGE)) {
      DropFreeListCategories(p, free_list());
    }

    ConcurrentAllocationMutex guard(this);
    DCHECK_EQ(this, p->owner());
    RefineAllocatedBytesAfterSweeping(p);
    RelinkFreeListCategories(p);
  }
}

// -----------------------------------------------------------------------------
// CompactionSpace implementation

Page* CompactionSpace::TryExpandImpl(
    MemoryAllocator::AllocationMode allocation_mode) {
  DCHECK_NE(NEW_SPACE, identity());
  Page* page = PagedSpaceBase::TryExpandImpl(allocation_mode);
  new_pages_.push_back(page);
  return page;
}

void CompactionSpace::RefillFreeList() {
  DCHECK_NE(NEW_SPACE, identity());

  Sweeper* sweeper = heap()->sweeper();
  size_t added = 0;
  Page* p = nullptr;
  while ((added <= kCompactionMemoryWanted) &&
         (p = sweeper->GetSweptPageSafe(this))) {
    // We regularly sweep NEVER_ALLOCATE_ON_PAGE pages. We drop the freelist
    // entries here to make them unavailable for allocations.
    if (p->IsFlagSet(Page::NEVER_ALLOCATE_ON_PAGE)) {
      DropFreeListCategories(p, free_list());
    }

    // Only during compaction pages can actually change ownership. This is
    // safe because there exists no other competing action on the page links
    // during compaction.
    DCHECK_NE(this, p->owner());
    PagedSpace* owner = static_cast<PagedSpace*>(p->owner());
    base::MutexGuard guard(owner->mutex());
    owner->RefineAllocatedBytesAfterSweeping(p);
    owner->RemovePage(p);
    added += AddPage(p);
    added += p->wasted_memory();
  }
}

bool CompactionSpace::RefillLabMain(int size_in_bytes,
                                    AllocationOrigin origin) {
  return RawRefillLabMain(size_in_bytes, origin);
}

// -----------------------------------------------------------------------------
// OldSpace implementation

void OldSpace::AddPromotedPage(Page* page) {
  if (v8_flags.minor_ms) {
    // Reset the page's allocated bytes. The page will be swept and the
    // allocated bytes will be updated to match the live bytes.
    DCHECK_EQ(page->area_size(), page->allocated_bytes());
    page->DecreaseAllocatedBytes(page->area_size());
  }
  AddPageImpl(page);
  if (!v8_flags.minor_ms) {
    RelinkFreeListCategories(page);
  }
}

}  // namespace internal
}  // namespace v8
