// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_NEW_SPACES_H_
#define V8_HEAP_NEW_SPACES_H_

#include <atomic>
#include <memory>

#include "src/base/macros.h"
#include "src/base/platform/mutex.h"
#include "src/common/globals.h"
#include "src/heap/heap.h"
#include "src/heap/spaces.h"
#include "src/logging/log.h"
#include "src/objects/heap-object.h"

namespace v8 {
namespace internal {

class Heap;
class MemoryChunk;
class SemiSpaceNewSpace;

enum SemiSpaceId { kFromSpace = 0, kToSpace = 1 };

using ParkedAllocationBuffer = std::pair<int, Address>;
using ParkedAllocationBuffersVector = std::vector<ParkedAllocationBuffer>;

// -----------------------------------------------------------------------------
// SemiSpace in young generation
//
// A SemiSpace is a contiguous chunk of memory holding page-like memory chunks.
// The mark-compact collector  uses the memory of the first page in the from
// space as a marking stack when tracing live objects.
class SemiSpace final : public Space {
 public:
  using iterator = PageIterator;
  using const_iterator = ConstPageIterator;

  static void Swap(SemiSpace* from, SemiSpace* to);

  SemiSpace(Heap* heap, SemiSpaceId semispace)
      : Space(heap, NEW_SPACE, new NoFreeList()),
        current_capacity_(0),
        target_capacity_(0),
        maximum_capacity_(0),
        minimum_capacity_(0),
        age_mark_(kNullAddress),
        id_(semispace),
        current_page_(nullptr) {}

  inline bool Contains(HeapObject o) const;
  inline bool Contains(Object o) const;
  inline bool ContainsSlow(Address a) const;

  void SetUp(size_t initial_capacity, size_t maximum_capacity);
  void TearDown();

  bool Commit();
  void Uncommit();
  bool IsCommitted() const { return !memory_chunk_list_.Empty(); }

  // Grow the semispace to the new capacity.  The new capacity requested must
  // be larger than the current capacity and less than the maximum capacity.
  bool GrowTo(size_t new_capacity);

  // Shrinks the semispace to the new capacity.  The new capacity requested
  // must be more than the amount of used memory in the semispace and less
  // than the current capacity.
  void ShrinkTo(size_t new_capacity);

  bool EnsureCurrentCapacity();

  // Returns the start address of the first page of the space.
  Address space_start() const {
    DCHECK_NE(memory_chunk_list_.front(), nullptr);
    return memory_chunk_list_.front()->area_start();
  }

  Page* current_page() { return current_page_; }

  // Returns the start address of the current page of the space.
  Address page_low() const { return current_page_->area_start(); }

  // Returns one past the end address of the current page of the space.
  Address page_high() const { return current_page_->area_end(); }

  bool AdvancePage() {
    Page* next_page = current_page_->next_page();
    // We cannot expand if we reached the target capcity. Note
    // that we need to account for the next page already for this check as we
    // could potentially fill the whole page after advancing.
    if (next_page == nullptr || (current_capacity_ == target_capacity_)) {
      return false;
    }
    current_page_ = next_page;
    current_capacity_ += Page::kPageSize;
    return true;
  }

  // Resets the space to using the first page.
  void Reset();

  void RemovePage(Page* page);
  void PrependPage(Page* page);
  void MovePageToTheEnd(Page* page);

  Page* InitializePage(MemoryChunk* chunk) final;

  // Age mark accessors.
  Address age_mark() const { return age_mark_; }
  void set_age_mark(Address mark);

  // Returns the current capacity of the semispace.
  size_t current_capacity() const { return current_capacity_; }

  // Returns the target capacity of the semispace.
  size_t target_capacity() const { return target_capacity_; }

  // Returns the maximum capacity of the semispace.
  size_t maximum_capacity() const { return maximum_capacity_; }

  // Returns the initial capacity of the semispace.
  size_t minimum_capacity() const { return minimum_capacity_; }

  SemiSpaceId id() const { return id_; }

  // Approximate amount of physical memory committed for this space.
  size_t CommittedPhysicalMemory() const final;

  // If we don't have these here then SemiSpace will be abstract.  However
  // they should never be called:

  size_t Size() const final { UNREACHABLE(); }

  size_t SizeOfObjects() const final { return Size(); }

  size_t Available() const final { UNREACHABLE(); }

  Page* first_page() final {
    return reinterpret_cast<Page*>(memory_chunk_list_.front());
  }
  Page* last_page() final {
    return reinterpret_cast<Page*>(memory_chunk_list_.back());
  }

  const Page* first_page() const final {
    return reinterpret_cast<const Page*>(memory_chunk_list_.front());
  }
  const Page* last_page() const final {
    return reinterpret_cast<const Page*>(memory_chunk_list_.back());
  }

  iterator begin() { return iterator(first_page()); }
  iterator end() { return iterator(nullptr); }

  const_iterator begin() const { return const_iterator(first_page()); }
  const_iterator end() const { return const_iterator(nullptr); }

  std::unique_ptr<ObjectIterator> GetObjectIterator(Heap* heap) final;

#ifdef DEBUG
  V8_EXPORT_PRIVATE void Print() final;
  // Validate a range of of addresses in a SemiSpace.
  // The "from" address must be on a page prior to the "to" address,
  // in the linked page order, or it must be earlier on the same page.
  static void AssertValidRange(Address from, Address to);
#else
  // Do nothing.
  inline static void AssertValidRange(Address from, Address to) {}
#endif

#ifdef VERIFY_HEAP
  virtual void Verify() const;
#endif

  void AddRangeToActiveSystemPages(Address start, Address end);

 private:
  void RewindPages(int num_pages);

  // Copies the flags into the masked positions on all pages in the space.
  void FixPagesFlags(Page::MainThreadFlags flags, Page::MainThreadFlags mask);

  void IncrementCommittedPhysicalMemory(size_t increment_value);
  void DecrementCommittedPhysicalMemory(size_t decrement_value);

  // The currently committed space capacity.
  size_t current_capacity_;

  // The targetted committed space capacity.
  size_t target_capacity_;

  // The maximum capacity that can be used by this space. A space cannot grow
  // beyond that size.
  size_t maximum_capacity_;

  // The minimum capacity for the space. A space cannot shrink below this size.
  size_t minimum_capacity_;

  // Used to govern object promotion during mark-compact collection.
  Address age_mark_;

  size_t committed_physical_memory_{0};

  SemiSpaceId id_;

  Page* current_page_;

  friend class SemiSpaceNewSpace;
  friend class SemiSpaceObjectIterator;
};

// A SemiSpaceObjectIterator is an ObjectIterator that iterates over the active
// semispace of the heap's new space.  It iterates over the objects in the
// semispace from a given start address (defaulting to the bottom of the
// semispace) to the top of the semispace.  New objects allocated after the
// iterator is created are not iterated.
class SemiSpaceObjectIterator : public ObjectIterator {
 public:
  // Create an iterator over the allocated objects in the given to-space.
  explicit SemiSpaceObjectIterator(const SemiSpaceNewSpace* space);

  inline HeapObject Next() final;

 private:
  void Initialize(Address start, Address end);

  // The current iteration point.
  Address current_;
  // The end of iteration.
  Address limit_;
};

class NewSpace : NON_EXPORTED_BASE(public SpaceWithLinearArea) {
 public:
  using iterator = PageIterator;
  using const_iterator = ConstPageIterator;

  NewSpace(Heap* heap, LinearAllocationArea* allocation_info);

  inline bool Contains(Object o) const;
  inline bool Contains(HeapObject o) const;
  virtual bool ContainsSlow(Address a) const = 0;

  void ResetParkedAllocationBuffers();

#if DEBUG
  void VerifyTop() const override;
#endif  // DEBUG

  Address original_top_acquire() const {
    return original_top_.load(std::memory_order_acquire);
  }
  Address original_limit_relaxed() const {
    return original_limit_.load(std::memory_order_relaxed);
  }

  V8_WARN_UNUSED_RESULT inline AllocationResult AllocateRawSynchronized(
      int size_in_bytes, AllocationAlignment alignment,
      AllocationOrigin origin = AllocationOrigin::kRuntime);

  void MoveOriginalTopForward() {
    base::SharedMutexGuard<base::kExclusive> guard(&pending_allocation_mutex_);
    DCHECK_GE(top(), original_top_);
    DCHECK_LE(top(), original_limit_);
    original_top_.store(top(), std::memory_order_release);
  }

  void MaybeFreeUnusedLab(LinearAllocationArea info);

  base::SharedMutex* pending_allocation_mutex() {
    return &pending_allocation_mutex_;
  }

  // Creates a filler object in the linear allocation area.
  void MakeLinearAllocationAreaIterable();

  // Creates a filler object in the linear allocation area and closes it.
  void FreeLinearAllocationArea() final;

  bool IsAtMaximumCapacity() const {
    return TotalCapacity() == MaximumCapacity();
  }

  size_t ExternalBackingStoreOverallBytes() const {
    size_t result = 0;
    for (int i = 0; i < ExternalBackingStoreType::kNumTypes; i++) {
      result +=
          ExternalBackingStoreBytes(static_cast<ExternalBackingStoreType>(i));
    }
    return result;
  }

  virtual size_t Capacity() const = 0;
  virtual size_t TotalCapacity() const = 0;
  virtual size_t MaximumCapacity() const = 0;
  virtual size_t AllocatedSinceLastGC() const = 0;

  // Grow the capacity of the space.
  virtual void Grow() = 0;

  // Shrink the capacity of the space.
  virtual void Shrink() = 0;

  virtual bool ShouldBePromoted(Address) const = 0;

#ifdef VERIFY_HEAP
  virtual void Verify(Isolate* isolate) const = 0;
#endif

  virtual iterator begin() = 0;
  virtual iterator end() = 0;

  virtual const_iterator begin() const = 0;
  virtual const_iterator end() const = 0;

  virtual Address first_allocatable_address() const = 0;

  virtual void ResetLinearAllocationArea() = 0;

  virtual bool AddFreshPage() = 0;

 protected:
  static const int kAllocationBufferParkingThreshold = 4 * KB;

  base::Mutex mutex_;

  // The top and the limit at the time of setting the linear allocation area.
  // These values can be accessed by background tasks. Protected by
  // pending_allocation_mutex_.
  std::atomic<Address> original_top_;
  std::atomic<Address> original_limit_;

  // Protects original_top_ and original_limit_.
  base::SharedMutex pending_allocation_mutex_;

  ParkedAllocationBuffersVector parked_allocation_buffers_;

  bool SupportsAllocationObserver() const final { return true; }
};

// -----------------------------------------------------------------------------
// The young generation space.
//
// The new space consists of a contiguous pair of semispaces.  It simply
// forwards most functions to the appropriate semispace.

class V8_EXPORT_PRIVATE SemiSpaceNewSpace final : public NewSpace {
 public:
  static SemiSpaceNewSpace* From(NewSpace* space) {
    return static_cast<SemiSpaceNewSpace*>(space);
  }

  SemiSpaceNewSpace(Heap* heap, v8::PageAllocator* page_allocator,
                    size_t initial_semispace_capacity,
                    size_t max_semispace_capacity,
                    LinearAllocationArea* allocation_info);

  ~SemiSpaceNewSpace() final;

  bool ContainsSlow(Address a) const final;

  // Flip the pair of spaces.
  void Flip();

  // Grow the capacity of the semispaces.  Assumes that they are not at
  // their maximum capacity.
  void Grow() final;

  // Shrink the capacity of the semispaces.
  void Shrink() final;

  // Return the allocated bytes in the active semispace.
  size_t Size() const final {
    DCHECK_GE(top(), to_space_.page_low());
    return (to_space_.current_capacity() - Page::kPageSize) / Page::kPageSize *
               MemoryChunkLayout::AllocatableMemoryInDataPage() +
           static_cast<size_t>(top() - to_space_.page_low());
  }

  size_t SizeOfObjects() const final { return Size(); }

  // Return the allocatable capacity of a semispace.
  size_t Capacity() const final {
    SLOW_DCHECK(to_space_.target_capacity() == from_space_.target_capacity());
    return (to_space_.target_capacity() / Page::kPageSize) *
           MemoryChunkLayout::AllocatableMemoryInDataPage();
  }

  // Return the current size of a semispace, allocatable and non-allocatable
  // memory.
  size_t TotalCapacity() const final {
    DCHECK(to_space_.target_capacity() == from_space_.target_capacity());
    return to_space_.target_capacity();
  }

  // Committed memory for NewSpace is the committed memory of both semi-spaces
  // combined.
  size_t CommittedMemory() const final {
    return from_space_.CommittedMemory() + to_space_.CommittedMemory();
  }

  size_t MaximumCommittedMemory() const final {
    return from_space_.MaximumCommittedMemory() +
           to_space_.MaximumCommittedMemory();
  }

  // Approximate amount of physical memory committed for this space.
  size_t CommittedPhysicalMemory() const final;

  // Return the available bytes without growing.
  size_t Available() const final {
    DCHECK_GE(Capacity(), Size());
    return Capacity() - Size();
  }

  size_t ExternalBackingStoreBytes(ExternalBackingStoreType type) const final {
    if (type == ExternalBackingStoreType::kArrayBuffer)
      return heap()->YoungArrayBufferBytes();
    DCHECK_EQ(0, from_space_.ExternalBackingStoreBytes(type));
    return to_space_.ExternalBackingStoreBytes(type);
  }

  size_t AllocatedSinceLastGC() const final;

  void MovePageFromSpaceToSpace(Page* page) {
    DCHECK(page->IsFromPage());
    from_space_.RemovePage(page);
    to_space_.PrependPage(page);
  }

  bool Rebalance();

  // Return the maximum capacity of a semispace.
  size_t MaximumCapacity() const final {
    DCHECK(to_space_.maximum_capacity() == from_space_.maximum_capacity());
    return to_space_.maximum_capacity();
  }

  // Returns the initial capacity of a semispace.
  size_t InitialTotalCapacity() const {
    DCHECK(to_space_.minimum_capacity() == from_space_.minimum_capacity());
    return to_space_.minimum_capacity();
  }

#if DEBUG
  void VerifyTop() const final;
#endif  // DEBUG

  // Return the address of the first allocatable address in the active
  // semispace. This may be the address where the first object resides.
  Address first_allocatable_address() const final {
    return to_space_.space_start();
  }

  // Get the age mark of the inactive semispace.
  Address age_mark() const { return from_space_.age_mark(); }
  // Set the age mark in the active semispace.
  void set_age_mark(Address mark) { to_space_.set_age_mark(mark); }

  // Reset the allocation pointer to the beginning of the active semispace.
  void ResetLinearAllocationArea() final;

  // When inline allocation stepping is active, either because of incremental
  // marking, idle scavenge, or allocation statistics gathering, we 'interrupt'
  // inline allocation every once in a while. This is done by setting
  // allocation_info_.limit to be lower than the actual limit and and increasing
  // it in steps to guarantee that the observers are notified periodically.
  void UpdateInlineAllocationLimit(size_t size_in_bytes) final;

  // Try to switch the active semispace to a new, empty, page.
  // Returns false if this isn't possible or reasonable (i.e., there
  // are no pages, or the current page is already empty), or true
  // if successful.
  bool AddFreshPage() final;

  bool AddParkedAllocationBuffer(int size_in_bytes,
                                 AllocationAlignment alignment);

#ifdef VERIFY_HEAP
  // Verify the active semispace.
  void Verify(Isolate* isolate) const final;
#endif

#ifdef DEBUG
  // Print the active semispace.
  void Print() override { to_space_.Print(); }
#endif

  // Return whether the operation succeeded.
  bool CommitFromSpaceIfNeeded() {
    if (from_space_.IsCommitted() || from_space_.Commit()) return true;

    // Committing memory to from space failed.
    // Memory is exhausted and we will die.
    heap_->FatalProcessOutOfMemory("Committing semi space failed.");
  }

  void UncommitFromSpace() {
    if (!from_space_.IsCommitted()) return;
    from_space_.Uncommit();
  }

  bool IsFromSpaceCommitted() const { return from_space_.IsCommitted(); }

  SemiSpace* active_space() { return &to_space_; }

  Page* first_page() final { return to_space_.first_page(); }
  Page* last_page() final { return to_space_.last_page(); }

  const Page* first_page() const final { return to_space_.first_page(); }
  const Page* last_page() const final { return to_space_.last_page(); }

  iterator begin() final { return to_space_.begin(); }
  iterator end() final { return to_space_.end(); }

  const_iterator begin() const final { return to_space_.begin(); }
  const_iterator end() const final { return to_space_.end(); }

  std::unique_ptr<ObjectIterator> GetObjectIterator(Heap* heap) final;

  SemiSpace& from_space() { return from_space_; }
  const SemiSpace& from_space() const { return from_space_; }
  SemiSpace& to_space() { return to_space_; }
  const SemiSpace& to_space() const { return to_space_; }

  bool ShouldBePromoted(Address address) const final;

 private:
  // Update linear allocation area to match the current to-space page.
  void UpdateLinearAllocationArea(Address known_top = 0);

  // The semispaces.
  SemiSpace to_space_;
  SemiSpace from_space_;
  VirtualMemory reservation_;

  bool EnsureAllocation(int size_in_bytes, AllocationAlignment alignment,
                        AllocationOrigin origin,
                        int* out_max_aligned_size) final;

  friend class SemiSpaceObjectIterator;
};

// For contiguous spaces, top should be in the space (or at the end) and limit
// should be the end of the space.
#define DCHECK_SEMISPACE_ALLOCATION_INFO(info, space) \
  SLOW_DCHECK((space).page_low() <= (info)->top() &&  \
              (info)->top() <= (space).page_high() && \
              (info)->limit() <= (space).page_high())

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_NEW_SPACES_H_
