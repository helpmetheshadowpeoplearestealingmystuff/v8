// Copyright 2006-2008 Google Inc. All Rights Reserved.
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

#include "macro-assembler.h"
#include "mark-compact.h"
#include "platform.h"

namespace v8 { namespace internal {

#ifdef DEBUG
DECLARE_bool(heap_stats);
DEFINE_bool(collect_heap_spill_statistics, false,
            "report heap spill statistics along with heap_stats "
            "(requires heap_stats)");
#endif

#ifdef ENABLE_LOGGING_AND_PROFILING
DECLARE_bool(log_gc);
#endif

// For paged spaces, top and limit should always be in the same page and top
// should not be greater than limit.
#define ASSERT_PAGED_ALLOCATION_INFO(info)       \
  ASSERT((Page::FromAllocationTop((info).top) == \
          Page::FromAllocationTop((info).limit)) \
      &&((info).top <= (info).limit))


// For contiguous spaces, top should be in the space (or at the end) and limit
// should be the end of the space.
#define ASSERT_SEMISPACE_ALLOCATION_INFO(info, space) \
  ASSERT((space)->low() <= (info).top                 \
         && (info).top <= (space)->high()             \
         && (info).limit == (space)->high())

// ----------------------------------------------------------------------------
// SpaceIterator

SpaceIterator::SpaceIterator() : current_space_(NEW_SPACE), iterator_(NULL) {
  // SpaceIterator depends on AllocationSpace enumeration starts with NEW_SPACE.
  ASSERT(NEW_SPACE == 0);
}


SpaceIterator::~SpaceIterator() {
  // Delete active iterator if any.
  if (iterator_ != NULL) delete iterator_;
}


bool SpaceIterator::has_next() {
  // Iterate until no more spaces.
  return current_space_ != LAST_SPACE;
}


ObjectIterator* SpaceIterator::next() {
  if (iterator_ != NULL) {
    delete iterator_;
    iterator_ = NULL;
    // Move to the next space
    current_space_++;
    if (current_space_ > LAST_SPACE) {
      return NULL;
    }
  }

  // Return iterator for the new current space.
  return CreateIterator();
}


// Create an iterator for the space to iterate.
ObjectIterator* SpaceIterator::CreateIterator() {
  ASSERT(iterator_ == NULL);

  switch (current_space_) {
    case NEW_SPACE:
      iterator_ = new SemiSpaceIterator(Heap::new_space());
      break;
    case OLD_SPACE:
      iterator_ = new HeapObjectIterator(Heap::old_space());
      break;
    case CODE_SPACE:
      iterator_ = new HeapObjectIterator(Heap::code_space());
      break;
    case MAP_SPACE:
      iterator_ = new HeapObjectIterator(Heap::map_space());
      break;
    case LO_SPACE:
      iterator_ = new LargeObjectIterator(Heap::lo_space());
      break;
  }

  // Return the newly allocated iterator;
  ASSERT(iterator_ != NULL);
  return iterator_;
}


// ----------------------------------------------------------------------------
// HeapObjectIterator

HeapObjectIterator::HeapObjectIterator(PagedSpace* space) {
  Initialize(space->bottom(), space->top(), NULL);
}


HeapObjectIterator::HeapObjectIterator(PagedSpace* space,
                                       HeapObjectCallback size_func) {
  Initialize(space->bottom(), space->top(), size_func);
}


HeapObjectIterator::HeapObjectIterator(PagedSpace* space, Address start) {
  Initialize(start, space->top(), NULL);
}


HeapObjectIterator::HeapObjectIterator(PagedSpace* space, Address start,
                                       HeapObjectCallback size_func) {
  Initialize(start, space->top(), size_func);
}


void HeapObjectIterator::Initialize(Address cur, Address end,
                                    HeapObjectCallback size_f) {
  cur_addr_ = cur;
  end_addr_ = end;
  end_page_ = Page::FromAllocationTop(end);
  size_func_ = size_f;
  Page* p = Page::FromAllocationTop(cur_addr_);
  cur_limit_ = (p == end_page_) ? end_addr_ : p->AllocationTop();

#ifdef DEBUG
  Verify();
#endif
}


bool HeapObjectIterator::HasNextInNextPage() {
  if (cur_addr_ == end_addr_) return false;

  Page* cur_page = Page::FromAllocationTop(cur_addr_);
  cur_page = cur_page->next_page();
  ASSERT(cur_page->is_valid());

  cur_addr_ = cur_page->ObjectAreaStart();
  cur_limit_ = (cur_page == end_page_) ? end_addr_ : cur_page->AllocationTop();

  ASSERT(cur_addr_ < cur_limit_);
#ifdef DEBUG
  Verify();
#endif
  return true;
}


#ifdef DEBUG
void HeapObjectIterator::Verify() {
  Page* p = Page::FromAllocationTop(cur_addr_);
  ASSERT(p == Page::FromAllocationTop(cur_limit_));
  ASSERT(p->Offset(cur_addr_) <= p->Offset(cur_limit_));
}
#endif


// -----------------------------------------------------------------------------
// PageIterator

PageIterator::PageIterator(PagedSpace* space, Mode mode) {
  cur_page_ = space->first_page_;
  switch (mode) {
    case PAGES_IN_USE:
      stop_page_ = space->AllocationTopPage()->next_page();
      break;
    case PAGES_USED_BY_MC:
      stop_page_ = space->MCRelocationTopPage()->next_page();
      break;
    case ALL_PAGES:
      stop_page_ = Page::FromAddress(NULL);
      break;
    default:
      UNREACHABLE();
  }
}


// -----------------------------------------------------------------------------
// Page

#ifdef DEBUG
Page::RSetState Page::rset_state_ = Page::IN_USE;
#endif

// -----------------------------------------------------------------------------
// MemoryAllocator
//
int MemoryAllocator::capacity_   = 0;
int MemoryAllocator::size_       = 0;

VirtualMemory* MemoryAllocator::initial_chunk_ = NULL;

// 270 is an estimate based on the static default heap size of a pair of 256K
// semispaces and a 64M old generation.
const int kEstimatedNumberOfChunks = 270;
List<MemoryAllocator::ChunkInfo> MemoryAllocator::chunks_(
    kEstimatedNumberOfChunks);
List<int> MemoryAllocator::free_chunk_ids_(kEstimatedNumberOfChunks);
int MemoryAllocator::max_nof_chunks_ = 0;
int MemoryAllocator::top_ = 0;


void MemoryAllocator::Push(int free_chunk_id) {
  ASSERT(max_nof_chunks_ > 0);
  ASSERT(top_ < max_nof_chunks_);
  free_chunk_ids_[top_++] = free_chunk_id;
}


int MemoryAllocator::Pop() {
  ASSERT(top_ > 0);
  return free_chunk_ids_[--top_];
}


bool MemoryAllocator::Setup(int capacity) {
  capacity_ = RoundUp(capacity, Page::kPageSize);

  // Over-estimate the size of chunks_ array.  It assumes the expansion of old
  // space is always in the unit of a chunk (kChunkSize) except the last
  // expansion.
  //
  // Due to alignment, allocated space might be one page less than required
  // number (kPagesPerChunk) of pages for old spaces.
  //
  // Reserve two chunk ids for semispaces, one for map space and one for old
  // space.
  max_nof_chunks_ = (capacity_ / (kChunkSize - Page::kPageSize)) + 4;
  if (max_nof_chunks_ > kMaxNofChunks) return false;

  size_ = 0;
  ChunkInfo info;  // uninitialized element.
  for (int i = max_nof_chunks_ - 1; i >= 0; i--) {
    chunks_.Add(info);
    free_chunk_ids_.Add(i);
  }
  top_ = max_nof_chunks_;
  return true;
}


void MemoryAllocator::TearDown() {
  for (int i = 0; i < max_nof_chunks_; i++) {
    if (chunks_[i].address() != NULL) DeleteChunk(i);
  }
  chunks_.Clear();
  free_chunk_ids_.Clear();

  if (initial_chunk_ != NULL) {
    LOG(DeleteEvent("InitialChunk", initial_chunk_->address()));
    delete initial_chunk_;
    initial_chunk_ = NULL;
  }

  ASSERT(top_ == max_nof_chunks_);  // all chunks are free
  top_ = 0;
  capacity_ = 0;
  size_ = 0;
  max_nof_chunks_ = 0;
}


void* MemoryAllocator::AllocateRawMemory(const size_t requested,
                                         size_t* allocated) {
  if (size_ + static_cast<int>(requested) > capacity_) return NULL;

  void* mem = OS::Allocate(requested, allocated);
  int alloced = *allocated;
  size_ += alloced;
  Counters::memory_allocated.Increment(alloced);
  return mem;
}


void MemoryAllocator::FreeRawMemory(void* mem, size_t length) {
  OS::Free(mem, length);
  Counters::memory_allocated.Decrement(length);
  size_ -= length;
  ASSERT(size_ >= 0);
}


void* MemoryAllocator::ReserveInitialChunk(const size_t requested) {
  ASSERT(initial_chunk_ == NULL);

  initial_chunk_ = new VirtualMemory(requested);
  CHECK(initial_chunk_ != NULL);
  if (!initial_chunk_->IsReserved()) {
    delete initial_chunk_;
    initial_chunk_ = NULL;
    return NULL;
  }

  // We are sure that we have mapped a block of requested addresses.
  ASSERT(initial_chunk_->size() == requested);
  LOG(NewEvent("InitialChunk", initial_chunk_->address(), requested));
  size_ += requested;
  return initial_chunk_->address();
}


static int PagesInChunk(Address start, size_t size) {
  // The first page starts on the first page-aligned address from start onward
  // and the last page ends on the last page-aligned address before
  // start+size.  Page::kPageSize is a power of two so we can divide by
  // shifting.
  return (RoundDown(start + size, Page::kPageSize)
          - RoundUp(start, Page::kPageSize)) >> Page::kPageSizeBits;
}


Page* MemoryAllocator::AllocatePages(int requested_pages, int* allocated_pages,
                                     PagedSpace* owner) {
  if (requested_pages <= 0) return Page::FromAddress(NULL);
  size_t chunk_size = requested_pages * Page::kPageSize;

  // There is not enough space to guarantee the desired number pages can be
  // allocated.
  if (size_ + static_cast<int>(chunk_size) > capacity_) {
    // Request as many pages as we can.
    chunk_size = capacity_ - size_;
    requested_pages = chunk_size >> Page::kPageSizeBits;

    if (requested_pages <= 0) return Page::FromAddress(NULL);
  }

  void* chunk = AllocateRawMemory(chunk_size, &chunk_size);
  if (chunk == NULL) return Page::FromAddress(NULL);
  LOG(NewEvent("PagedChunk", chunk, chunk_size));

  *allocated_pages = PagesInChunk(static_cast<Address>(chunk), chunk_size);
  if (*allocated_pages == 0) {
    FreeRawMemory(chunk, chunk_size);
    LOG(DeleteEvent("PagedChunk", chunk));
    return Page::FromAddress(NULL);
  }

  int chunk_id = Pop();
  chunks_[chunk_id].init(static_cast<Address>(chunk), chunk_size, owner);

  return InitializePagesInChunk(chunk_id, *allocated_pages, owner);
}


Page* MemoryAllocator::CommitPages(Address start, size_t size,
                                   PagedSpace* owner, int* num_pages) {
  ASSERT(start != NULL);
  *num_pages = PagesInChunk(start, size);
  ASSERT(*num_pages > 0);
  ASSERT(initial_chunk_ != NULL);
  ASSERT(initial_chunk_->address() <= start);
  ASSERT(start + size <= reinterpret_cast<Address>(initial_chunk_->address())
                             + initial_chunk_->size());

  if (!initial_chunk_->Commit(start, size)) {
    return Page::FromAddress(NULL);
  }
  Counters::memory_allocated.Increment(size);

  // So long as we correctly overestimated the number of chunks we should not
  // run out of chunk ids.
  CHECK(!OutOfChunkIds());
  int chunk_id = Pop();
  chunks_[chunk_id].init(start, size, owner);
  return InitializePagesInChunk(chunk_id, *num_pages, owner);
}


bool MemoryAllocator::CommitBlock(Address start, size_t size) {
  ASSERT(start != NULL);
  ASSERT(size > 0);
  ASSERT(initial_chunk_ != NULL);
  ASSERT(initial_chunk_->address() <= start);
  ASSERT(start + size <= reinterpret_cast<Address>(initial_chunk_->address())
                             + initial_chunk_->size());

  if (!initial_chunk_->Commit(start, size)) return false;
  Counters::memory_allocated.Increment(size);
  return true;
}


Page* MemoryAllocator::InitializePagesInChunk(int chunk_id, int pages_in_chunk,
                                              PagedSpace* owner) {
  ASSERT(IsValidChunk(chunk_id));
  ASSERT(pages_in_chunk > 0);

  Address chunk_start = chunks_[chunk_id].address();

  Address low = RoundUp(chunk_start, Page::kPageSize);

#ifdef DEBUG
  size_t chunk_size = chunks_[chunk_id].size();
  Address high = RoundDown(chunk_start + chunk_size, Page::kPageSize);
  ASSERT(pages_in_chunk <=
        ((OffsetFrom(high) - OffsetFrom(low)) / Page::kPageSize));
#endif

  Address page_addr = low;
  for (int i = 0; i < pages_in_chunk; i++) {
    Page* p = Page::FromAddress(page_addr);
    p->opaque_header = OffsetFrom(page_addr + Page::kPageSize) | chunk_id;
    p->is_normal_page = 1;
    page_addr += Page::kPageSize;
  }

  // Set the next page of the last page to 0.
  Page* last_page = Page::FromAddress(page_addr - Page::kPageSize);
  last_page->opaque_header = OffsetFrom(0) | chunk_id;

  return Page::FromAddress(low);
}


Page* MemoryAllocator::FreePages(Page* p) {
  if (!p->is_valid()) return p;

  // Find the first page in the same chunk as 'p'
  Page* first_page = FindFirstPageInSameChunk(p);
  Page* page_to_return = Page::FromAddress(NULL);

  if (p != first_page) {
    // Find the last page in the same chunk as 'prev'.
    Page* last_page = FindLastPageInSameChunk(p);
    first_page = GetNextPage(last_page);  // first page in next chunk

    // set the next_page of last_page to NULL
    SetNextPage(last_page, Page::FromAddress(NULL));
    page_to_return = p;  // return 'p' when exiting
  }

  while (first_page->is_valid()) {
    int chunk_id = GetChunkId(first_page);
    ASSERT(IsValidChunk(chunk_id));

    // Find the first page of the next chunk before deleting this chunk.
    first_page = GetNextPage(FindLastPageInSameChunk(first_page));

    // Free the current chunk.
    DeleteChunk(chunk_id);
  }

  return page_to_return;
}


void MemoryAllocator::DeleteChunk(int chunk_id) {
  ASSERT(IsValidChunk(chunk_id));

  ChunkInfo& c = chunks_[chunk_id];

  // We cannot free a chunk contained in the initial chunk because it was not
  // allocated with AllocateRawMemory.  Instead we uncommit the virtual
  // memory.
  bool in_initial_chunk = false;
  if (initial_chunk_ != NULL) {
    Address start = static_cast<Address>(initial_chunk_->address());
    Address end = start + initial_chunk_->size();
    in_initial_chunk = (start <= c.address()) && (c.address() < end);
  }

  if (in_initial_chunk) {
    // TODO(1240712): VirtualMemory::Uncommit has a return value which
    // is ignored here.
    initial_chunk_->Uncommit(c.address(), c.size());
    Counters::memory_allocated.Decrement(c.size());
  } else {
    LOG(DeleteEvent("PagedChunk", c.address()));
    FreeRawMemory(c.address(), c.size());
  }
  c.init(NULL, 0, NULL);
  Push(chunk_id);
}


Page* MemoryAllocator::FindFirstPageInSameChunk(Page* p) {
  int chunk_id = GetChunkId(p);
  ASSERT(IsValidChunk(chunk_id));

  Address low = RoundUp(chunks_[chunk_id].address(), Page::kPageSize);
  return Page::FromAddress(low);
}


Page* MemoryAllocator::FindLastPageInSameChunk(Page* p) {
  int chunk_id = GetChunkId(p);
  ASSERT(IsValidChunk(chunk_id));

  Address chunk_start = chunks_[chunk_id].address();
  size_t chunk_size = chunks_[chunk_id].size();

  Address high = RoundDown(chunk_start + chunk_size, Page::kPageSize);
  ASSERT(chunk_start <= p->address() && p->address() < high);

  return Page::FromAddress(high - Page::kPageSize);
}


#ifdef DEBUG
void MemoryAllocator::ReportStatistics() {
  float pct = static_cast<float>(capacity_ - size_) / capacity_;
  PrintF("  capacity: %d, used: %d, available: %%%d\n\n",
         capacity_, size_, static_cast<int>(pct*100));
}
#endif


// -----------------------------------------------------------------------------
// PagedSpace implementation

PagedSpace::PagedSpace(int max_capacity, AllocationSpace id) {
  ASSERT(id == OLD_SPACE || id == CODE_SPACE || id == MAP_SPACE);
  max_capacity_ = (RoundDown(max_capacity, Page::kPageSize) / Page::kPageSize)
                  * Page::kObjectAreaSize;
  identity_ = id;
  accounting_stats_.Clear();

  allocation_mode_ = LINEAR;

  allocation_info_.top = NULL;
  allocation_info_.limit = NULL;

  mc_forwarding_info_.top = NULL;
  mc_forwarding_info_.limit = NULL;
}


bool PagedSpace::Setup(Address start, size_t size) {
  if (HasBeenSetup()) return false;

  int num_pages = 0;
  // Try to use the virtual memory range passed to us.  If it is too small to
  // contain at least one page, ignore it and allocate instead.
  if (PagesInChunk(start, size) > 0) {
    first_page_ = MemoryAllocator::CommitPages(start, size, this, &num_pages);
  } else {
    int requested_pages = Min(MemoryAllocator::kPagesPerChunk,
                              max_capacity_ / Page::kObjectAreaSize);
    first_page_ =
        MemoryAllocator::AllocatePages(requested_pages, &num_pages, this);
    if (!first_page_->is_valid()) return false;
  }

  // We are sure that the first page is valid and that we have at least one
  // page.
  ASSERT(first_page_->is_valid());
  ASSERT(num_pages > 0);
  accounting_stats_.ExpandSpace(num_pages * Page::kObjectAreaSize);
  ASSERT(Capacity() <= max_capacity_);

  for (Page* p = first_page_; p->is_valid(); p = p->next_page()) {
    p->ClearRSet();
  }

  // Use first_page_ for allocation.
  SetAllocationInfo(&allocation_info_, first_page_);

  return true;
}


bool PagedSpace::HasBeenSetup() {
  return (Capacity() > 0);
}


void PagedSpace::TearDown() {
  first_page_ = MemoryAllocator::FreePages(first_page_);
  ASSERT(!first_page_->is_valid());

  accounting_stats_.Clear();
}


void PagedSpace::ClearRSet() {
  PageIterator it(this, PageIterator::ALL_PAGES);
  while (it.has_next()) {
    it.next()->ClearRSet();
  }
}


Object* PagedSpace::FindObject(Address addr) {
#ifdef DEBUG
  // Note: this function can only be called before or after mark-compact GC
  // because it accesses map pointers.
  ASSERT(!MarkCompactCollector::in_use());
#endif

  if (!Contains(addr)) return Failure::Exception();

  Page* p = Page::FromAddress(addr);
  Address cur = p->ObjectAreaStart();
  Address end = p->AllocationTop();
  while (cur < end) {
    HeapObject* obj = HeapObject::FromAddress(cur);
    Address next = cur + obj->Size();
    if ((cur <= addr) && (addr < next)) return obj;
    cur = next;
  }

  return Failure::Exception();
}


void PagedSpace::SetAllocationInfo(AllocationInfo* alloc_info, Page* p) {
  alloc_info->top = p->ObjectAreaStart();
  alloc_info->limit = p->ObjectAreaEnd();
  ASSERT_PAGED_ALLOCATION_INFO(*alloc_info);
}


void PagedSpace::MCResetRelocationInfo() {
  // Set page indexes.
  int i = 0;
  PageIterator it(this, PageIterator::ALL_PAGES);
  while (it.has_next()) {
    Page* p = it.next();
    p->mc_page_index = i++;
  }

  // Set mc_forwarding_info_ to the first page in the space.
  SetAllocationInfo(&mc_forwarding_info_, first_page_);
  // All the bytes in the space are 'available'.  We will rediscover
  // allocated and wasted bytes during GC.
  accounting_stats_.Reset();
}


void PagedSpace::SetLinearAllocationOnly(bool linear_only) {
  if (linear_only) {
    // Note that the free_list is not cleared. If we switch back to
    // FREE_LIST mode it will be available for use. Resetting it
    // requires correct accounting for the wasted bytes.
    allocation_mode_ = LINEAR_ONLY;
  } else {
    ASSERT(allocation_mode_ == LINEAR_ONLY);
    allocation_mode_ = LINEAR;
  }
}


int PagedSpace::MCSpaceOffsetForAddress(Address addr) {
#ifdef DEBUG
  // The Contains function considers the address at the beginning of a
  // page in the page, MCSpaceOffsetForAddress considers it is in the
  // previous page.
  if (Page::IsAlignedToPageSize(addr)) {
    ASSERT(Contains(addr - kPointerSize));
  } else {
    ASSERT(Contains(addr));
  }
#endif

  // If addr is at the end of a page, it belongs to previous page
  Page* p = Page::IsAlignedToPageSize(addr)
            ? Page::FromAllocationTop(addr)
            : Page::FromAddress(addr);
  int index = p->mc_page_index;
  return (index * Page::kPageSize) + p->Offset(addr);
}


bool PagedSpace::Expand(Page* last_page) {
  ASSERT(max_capacity_ % Page::kObjectAreaSize == 0);
  ASSERT(Capacity() % Page::kObjectAreaSize == 0);

  if (Capacity() == max_capacity_) return false;

  ASSERT(Capacity() < max_capacity_);
  // Last page must be valid and its next page is invalid.
  ASSERT(last_page->is_valid() && !last_page->next_page()->is_valid());

  int available_pages = (max_capacity_ - Capacity()) / Page::kObjectAreaSize;
  if (available_pages <= 0) return false;

  int desired_pages = Min(available_pages, MemoryAllocator::kPagesPerChunk);
  Page* p = MemoryAllocator::AllocatePages(desired_pages, &desired_pages, this);
  if (!p->is_valid()) return false;

  accounting_stats_.ExpandSpace(desired_pages * Page::kObjectAreaSize);
  ASSERT(Capacity() <= max_capacity_);

  MemoryAllocator::SetNextPage(last_page, p);

  // Clear remembered set of new pages.
  while (p->is_valid()) {
    p->ClearRSet();
    p = p->next_page();
  }

  return true;
}


#ifdef DEBUG
int PagedSpace::CountTotalPages() {
  int count = 0;
  for (Page* p = first_page_; p->is_valid(); p = p->next_page()) {
    count++;
  }
  return count;
}
#endif


void PagedSpace::Shrink() {
  // Release half of free pages.
  Page* top_page = AllocationTopPage();
  ASSERT(top_page->is_valid());

  // Loop over the pages from the top page to the end of the space to count
  // the number of pages to keep and find the last page to keep.
  int free_pages = 0;
  int pages_to_keep = 0;  // Of the free pages.
  Page* last_page_to_keep = top_page;
  Page* current_page = top_page->next_page();
  // Loop over the pages to the end of the space.
  while (current_page->is_valid()) {
    // Keep every odd-numbered page, one page for every two in the space.
    if ((free_pages & 0x1) == 1) {
      pages_to_keep++;
      last_page_to_keep = last_page_to_keep->next_page();
    }
    free_pages++;
    current_page = current_page->next_page();
  }

  // Free pages after last_page_to_keep, and adjust the next_page link.
  Page* p = MemoryAllocator::FreePages(last_page_to_keep->next_page());
  MemoryAllocator::SetNextPage(last_page_to_keep, p);

  // Since pages are only freed in whole chunks, we may have kept more than
  // pages_to_keep.
  while (p->is_valid()) {
    pages_to_keep++;
    p = p->next_page();
  }

  // The difference between free_pages and pages_to_keep is the number of
  // pages actually freed.
  ASSERT(pages_to_keep <= free_pages);
  int bytes_freed = (free_pages - pages_to_keep) * Page::kObjectAreaSize;
  accounting_stats_.ShrinkSpace(bytes_freed);

  ASSERT(Capacity() == CountTotalPages() * Page::kObjectAreaSize);
}


bool PagedSpace::EnsureCapacity(int capacity) {
  if (Capacity() >= capacity) return true;

  // Start from the allocation top and loop to the last page in the space.
  Page* last_page = AllocationTopPage();
  Page* next_page = last_page->next_page();
  while (next_page->is_valid()) {
    last_page = MemoryAllocator::FindLastPageInSameChunk(next_page);
    next_page = last_page->next_page();
  }

  // Expand the space until it has the required capacity or expansion fails.
  do {
    if (!Expand(last_page)) return false;
    ASSERT(last_page->next_page()->is_valid());
    last_page =
        MemoryAllocator::FindLastPageInSameChunk(last_page->next_page());
  } while (Capacity() < capacity);

  return true;
}


#ifdef DEBUG
void PagedSpace::Print() { }
#endif


// -----------------------------------------------------------------------------
// NewSpace implementation

NewSpace::NewSpace(int initial_semispace_capacity,
                   int maximum_semispace_capacity) {
  ASSERT(initial_semispace_capacity <= maximum_semispace_capacity);
  ASSERT(IsPowerOf2(maximum_semispace_capacity));
  maximum_capacity_ = maximum_semispace_capacity;
  capacity_ = initial_semispace_capacity;
  to_space_ = new SemiSpace(capacity_, maximum_capacity_);
  from_space_ = new SemiSpace(capacity_, maximum_capacity_);

  // Allocate and setup the histogram arrays if necessary.
#if defined(DEBUG) || defined(ENABLE_LOGGING_AND_PROFILING)
  allocated_histogram_ = NewArray<HistogramInfo>(LAST_TYPE + 1);
  promoted_histogram_ = NewArray<HistogramInfo>(LAST_TYPE + 1);

#define SET_NAME(name) allocated_histogram_[name].set_name(#name); \
                       promoted_histogram_[name].set_name(#name);
  INSTANCE_TYPE_LIST(SET_NAME)
#undef SET_NAME
#endif
}


bool NewSpace::Setup(Address start, int size) {
  ASSERT(size == 2 * maximum_capacity_);
  ASSERT(IsAddressAligned(start, size, 0));

  if (to_space_ == NULL
      || !to_space_->Setup(start, maximum_capacity_)) {
    return false;
  }
  if (from_space_ == NULL
      || !from_space_->Setup(start + maximum_capacity_, maximum_capacity_)) {
    return false;
  }

  start_ = start;
  address_mask_ = ~(size - 1);
  object_mask_ = address_mask_ | kHeapObjectTag;
  object_expected_ = reinterpret_cast<uint32_t>(start) | kHeapObjectTag;

  allocation_info_.top = to_space_->low();
  allocation_info_.limit = to_space_->high();
  mc_forwarding_info_.top = NULL;
  mc_forwarding_info_.limit = NULL;

  ASSERT_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);
  return true;
}


void NewSpace::TearDown() {
#if defined(DEBUG) || defined(ENABLE_LOGGING_AND_PROFILING)
  if (allocated_histogram_) {
    DeleteArray(allocated_histogram_);
    allocated_histogram_ = NULL;
  }
  if (promoted_histogram_) {
    DeleteArray(promoted_histogram_);
    promoted_histogram_ = NULL;
  }
#endif

  start_ = NULL;
  capacity_ = 0;
  allocation_info_.top = NULL;
  allocation_info_.limit = NULL;
  mc_forwarding_info_.top = NULL;
  mc_forwarding_info_.limit = NULL;

  if (to_space_ != NULL) {
    to_space_->TearDown();
    delete to_space_;
    to_space_ = NULL;
  }

  if (from_space_ != NULL) {
    from_space_->TearDown();
    delete from_space_;
    from_space_ = NULL;
  }
}


void NewSpace::Flip() {
  SemiSpace* tmp = from_space_;
  from_space_ = to_space_;
  to_space_ = tmp;
}


bool NewSpace::Double() {
  ASSERT(capacity_ <= maximum_capacity_ / 2);
  // TODO(1240712): Failure to double the from space can result in
  // semispaces of different sizes.  In the event of that failure, the
  // to space doubling should be rolled back before returning false.
  if (!to_space_->Double() || !from_space_->Double()) return false;
  capacity_ *= 2;
  allocation_info_.limit = to_space_->high();
  ASSERT_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);
  return true;
}


void NewSpace::ResetAllocationInfo() {
  allocation_info_.top = to_space_->low();
  allocation_info_.limit = to_space_->high();
  ASSERT_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);
}


void NewSpace::MCResetRelocationInfo() {
  mc_forwarding_info_.top = from_space_->low();
  mc_forwarding_info_.limit = from_space_->high();
  ASSERT_SEMISPACE_ALLOCATION_INFO(mc_forwarding_info_, from_space_);
}


void NewSpace::MCCommitRelocationInfo() {
  // Assumes that the spaces have been flipped so that mc_forwarding_info_ is
  // valid allocation info for the to space.
  allocation_info_.top = mc_forwarding_info_.top;
  allocation_info_.limit = to_space_->high();
  ASSERT_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);
}


#ifdef DEBUG
// We do not use the SemispaceIterator because verification doesn't assume
// that it works (it depends on the invariants we are checking).
void NewSpace::Verify() {
  // The allocation pointer should be in the space or at the very end.
  ASSERT_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);

  // There should be objects packed in from the low address up to the
  // allocation pointer.
  Address current = to_space_->low();
  while (current < top()) {
    HeapObject* object = HeapObject::FromAddress(current);

    // The first word should be a map, and we expect all map pointers to
    // be in map space.
    Map* map = object->map();
    ASSERT(map->IsMap());
    ASSERT(Heap::map_space()->Contains(map));

    // The object should not be code or a map.
    ASSERT(!object->IsMap());
    ASSERT(!object->IsCode());

    // The object itself should look OK.
    object->Verify();

    // All the interior pointers should be contained in the heap.
    VerifyPointersVisitor visitor;
    int size = object->Size();
    object->IterateBody(map->instance_type(), size, &visitor);

    current += size;
  }

  // The allocation pointer should not be in the middle of an object.
  ASSERT(current == top());
}
#endif


// -----------------------------------------------------------------------------
// SemiSpace implementation

SemiSpace::SemiSpace(int initial_capacity, int maximum_capacity)
    : capacity_(initial_capacity), maximum_capacity_(maximum_capacity),
      start_(NULL), age_mark_(NULL) {
}


bool SemiSpace::Setup(Address start, int size) {
  ASSERT(size == maximum_capacity_);
  if (!MemoryAllocator::CommitBlock(start, capacity_)) return false;

  start_ = start;
  address_mask_ = ~(size - 1);
  object_mask_ = address_mask_ | kHeapObjectTag;
  object_expected_ = reinterpret_cast<uint32_t>(start) | kHeapObjectTag;

  age_mark_ = start_;
  return true;
}


void SemiSpace::TearDown() {
  start_ = NULL;
  capacity_ = 0;
}


bool SemiSpace::Double() {
  if (!MemoryAllocator::CommitBlock(high(), capacity_)) return false;
  capacity_ *= 2;
  return true;
}


#ifdef DEBUG
void SemiSpace::Print() { }
#endif


// -----------------------------------------------------------------------------
// SemiSpaceIterator implementation.
SemiSpaceIterator::SemiSpaceIterator(NewSpace* space) {
  Initialize(space, space->bottom(), space->top(), NULL);
}


SemiSpaceIterator::SemiSpaceIterator(NewSpace* space,
                                     HeapObjectCallback size_func) {
  Initialize(space, space->bottom(), space->top(), size_func);
}


SemiSpaceIterator::SemiSpaceIterator(NewSpace* space, Address start) {
  Initialize(space, start, space->top(), NULL);
}


void SemiSpaceIterator::Initialize(NewSpace* space, Address start,
                                   Address end,
                                   HeapObjectCallback size_func) {
  ASSERT(space->ToSpaceContains(start));
  ASSERT(space->ToSpaceLow() <= end
         && end <= space->ToSpaceHigh());
  space_ = space->to_space_;
  current_ = start;
  limit_ = end;
  size_func_ = size_func;
}


#ifdef DEBUG
// A static array of histogram info for each type.
static HistogramInfo heap_histograms[LAST_TYPE+1];
static JSObject::SpillInformation js_spill_information;

// heap_histograms is shared, always clear it before using it.
static void ClearHistograms() {
  // We reset the name each time, though it hasn't changed.
#define DEF_TYPE_NAME(name) heap_histograms[name].set_name(#name);
  INSTANCE_TYPE_LIST(DEF_TYPE_NAME)
#undef DEF_TYPE_NAME

#define CLEAR_HISTOGRAM(name) heap_histograms[name].clear();
  INSTANCE_TYPE_LIST(CLEAR_HISTOGRAM)
#undef CLEAR_HISTOGRAM

  js_spill_information.Clear();
}


static int code_kind_statistics[Code::NUMBER_OF_KINDS];


static void ClearCodeKindStatistics() {
  for (int i = 0; i < Code::NUMBER_OF_KINDS; i++) {
    code_kind_statistics[i] = 0;
  }
}


static void ReportCodeKindStatistics() {
  const char* table[Code::NUMBER_OF_KINDS];

#define CASE(name)                            \
  case Code::name: table[Code::name] = #name; \
  break

  for (int i = 0; i < Code::NUMBER_OF_KINDS; i++) {
    switch (static_cast<Code::Kind>(i)) {
      CASE(FUNCTION);
      CASE(STUB);
      CASE(BUILTIN);
      CASE(LOAD_IC);
      CASE(KEYED_LOAD_IC);
      CASE(STORE_IC);
      CASE(KEYED_STORE_IC);
      CASE(CALL_IC);
    }
  }

#undef CASE

  PrintF("\n   Code kind histograms: \n");
  for (int i = 0; i < Code::NUMBER_OF_KINDS; i++) {
    if (code_kind_statistics[i] > 0) {
      PrintF("     %-20s: %10d bytes\n", table[i], code_kind_statistics[i]);
    }
  }
  PrintF("\n");
}


static int CollectHistogramInfo(HeapObject* obj) {
  InstanceType type = obj->map()->instance_type();
  ASSERT(0 <= type && type <= LAST_TYPE);
  ASSERT(heap_histograms[type].name() != NULL);
  heap_histograms[type].increment_number(1);
  heap_histograms[type].increment_bytes(obj->Size());

  if (FLAG_collect_heap_spill_statistics && obj->IsJSObject()) {
    JSObject::cast(obj)->IncrementSpillStatistics(&js_spill_information);
  }

  return obj->Size();
}


static void ReportHistogram(bool print_spill) {
  PrintF("\n  Object Histogram:\n");
  for (int i = 0; i <= LAST_TYPE; i++) {
    if (heap_histograms[i].number() > 0) {
      PrintF("    %-33s%10d (%10d bytes)\n",
             heap_histograms[i].name(),
             heap_histograms[i].number(),
             heap_histograms[i].bytes());
    }
  }
  PrintF("\n");

  // Summarize string types.
  int string_number = 0;
  int string_bytes = 0;
#define INCREMENT(type, size, name)                  \
    string_number += heap_histograms[type].number(); \
    string_bytes += heap_histograms[type].bytes();
  STRING_TYPE_LIST(INCREMENT)
#undef INCREMENT
  if (string_number > 0) {
    PrintF("    %-33s%10d (%10d bytes)\n\n", "STRING_TYPE", string_number,
           string_bytes);
  }

  if (FLAG_collect_heap_spill_statistics && print_spill) {
    js_spill_information.Print();
  }
}
#endif  // DEBUG


// Support for statistics gathering for --heap-stats and --log-gc.
#if defined(DEBUG) || defined(ENABLE_LOGGING_AND_PROFILING)
void NewSpace::ClearHistograms() {
  for (int i = 0; i <= LAST_TYPE; i++) {
    allocated_histogram_[i].clear();
    promoted_histogram_[i].clear();
  }
}

// Because the copying collector does not touch garbage objects, we iterate
// the new space before a collection to get a histogram of allocated objects.
// This only happens (1) when compiled with DEBUG and the --heap-stats flag is
// set, or when compiled with ENABLE_LOGGING_AND_PROFILING and the --log-gc
// flag is set.
void NewSpace::CollectStatistics() {
  ClearHistograms();
  SemiSpaceIterator it(this);
  while (it.has_next()) RecordAllocation(it.next());
}


#ifdef ENABLE_LOGGING_AND_PROFILING
static void DoReportStatistics(HistogramInfo* info, const char* description) {
  LOG(HeapSampleBeginEvent("NewSpace", description));
  // Lump all the string types together.
  int string_number = 0;
  int string_bytes = 0;
#define INCREMENT(type, size, name)       \
    string_number += info[type].number(); \
    string_bytes += info[type].bytes();
  STRING_TYPE_LIST(INCREMENT)
#undef INCREMENT
  if (string_number > 0) {
    LOG(HeapSampleItemEvent("STRING_TYPE", string_number, string_bytes));
  }

  // Then do the other types.
  for (int i = FIRST_NONSTRING_TYPE; i <= LAST_TYPE; ++i) {
    if (info[i].number() > 0) {
      LOG(HeapSampleItemEvent(info[i].name(), info[i].number(),
                              info[i].bytes()));
    }
  }
  LOG(HeapSampleEndEvent("NewSpace", description));
}
#endif  // ENABLE_LOGGING_AND_PROFILING


void NewSpace::ReportStatistics() {
#ifdef DEBUG
  if (FLAG_heap_stats) {
    float pct = static_cast<float>(Available()) / Capacity();
    PrintF("  capacity: %d, available: %d, %%%d\n",
           Capacity(), Available(), static_cast<int>(pct*100));
    PrintF("\n  Object Histogram:\n");
    for (int i = 0; i <= LAST_TYPE; i++) {
      if (allocated_histogram_[i].number() > 0) {
        PrintF("    %-33s%10d (%10d bytes)\n",
               allocated_histogram_[i].name(),
               allocated_histogram_[i].number(),
               allocated_histogram_[i].bytes());
      }
    }
    PrintF("\n");
  }
#endif  // DEBUG

#ifdef ENABLE_LOGGING_AND_PROFILING
  if (FLAG_log_gc) {
    DoReportStatistics(allocated_histogram_, "allocated");
    DoReportStatistics(promoted_histogram_, "promoted");
  }
#endif  // ENABLE_LOGGING_AND_PROFILING
}


void NewSpace::RecordAllocation(HeapObject* obj) {
  InstanceType type = obj->map()->instance_type();
  ASSERT(0 <= type && type <= LAST_TYPE);
  allocated_histogram_[type].increment_number(1);
  allocated_histogram_[type].increment_bytes(obj->Size());
}


void NewSpace::RecordPromotion(HeapObject* obj) {
  InstanceType type = obj->map()->instance_type();
  ASSERT(0 <= type && type <= LAST_TYPE);
  promoted_histogram_[type].increment_number(1);
  promoted_histogram_[type].increment_bytes(obj->Size());
}
#endif  // defined(DEBUG) || defined(ENABLE_LOGGING_AND_PROFILING)


// -----------------------------------------------------------------------------
// Free lists for old object spaces implementation

void FreeListNode::set_size(int size_in_bytes) {
  ASSERT(size_in_bytes > 0);
  ASSERT(IsAligned(size_in_bytes, kPointerSize));

  // We write a map and possibly size information to the block.  If the block
  // is big enough to be a ByteArray with at least one extra word (the next
  // pointer), we set its map to be the byte array map and its size to an
  // appropriate array length for the desired size from HeapObject::Size().
  // If the block is too small (eg, one or two words), to hold both a size
  // field and a next pointer, we give it a filler map that gives it the
  // correct size.
  if (size_in_bytes > Array::kHeaderSize) {
    set_map(Heap::byte_array_map());
    ByteArray::cast(this)->set_length(ByteArray::LengthFor(size_in_bytes));
  } else if (size_in_bytes == kPointerSize) {
    set_map(Heap::one_word_filler_map());
  } else if (size_in_bytes == 2 * kPointerSize) {
    set_map(Heap::two_word_filler_map());
  } else {
    UNREACHABLE();
  }
}


Address FreeListNode::next() {
  ASSERT(map() == Heap::byte_array_map());
  return Memory::Address_at(address() + kNextOffset);
}


void FreeListNode::set_next(Address next) {
  ASSERT(map() == Heap::byte_array_map());
  Memory::Address_at(address() + kNextOffset) = next;
}


OldSpaceFreeList::OldSpaceFreeList(AllocationSpace owner) : owner_(owner) {
  Reset();
}


void OldSpaceFreeList::Reset() {
  available_ = 0;
  for (int i = 0; i < kFreeListsLength; i++) {
    free_[i].head_node_ = NULL;
  }
  needs_rebuild_ = false;
  finger_ = kHead;
  free_[kHead].next_size_ = kEnd;
}


void OldSpaceFreeList::RebuildSizeList() {
  ASSERT(needs_rebuild_);
  int cur = kHead;
  for (int i = cur + 1; i < kFreeListsLength; i++) {
    if (free_[i].head_node_ != NULL) {
      free_[cur].next_size_ = i;
      cur = i;
    }
  }
  free_[cur].next_size_ = kEnd;
  needs_rebuild_ = false;
}


int OldSpaceFreeList::Free(Address start, int size_in_bytes) {
#ifdef DEBUG
  for (int i = 0; i < size_in_bytes; i += kPointerSize) {
    Memory::Address_at(start + i) = kZapValue;
  }
#endif
  FreeListNode* node = FreeListNode::FromAddress(start);
  node->set_size(size_in_bytes);

  // Early return to drop too-small blocks on the floor (one or two word
  // blocks cannot hold a map pointer, a size field, and a pointer to the
  // next block in the free list).
  if (size_in_bytes < kMinBlockSize) {
    return size_in_bytes;
  }

  // Insert other blocks at the head of an exact free list.
  int index = size_in_bytes >> kPointerSizeLog2;
  node->set_next(free_[index].head_node_);
  free_[index].head_node_ = node->address();
  available_ += size_in_bytes;
  needs_rebuild_ = true;
  return 0;
}


Object* OldSpaceFreeList::Allocate(int size_in_bytes, int* wasted_bytes) {
  ASSERT(0 < size_in_bytes);
  ASSERT(size_in_bytes <= kMaxBlockSize);
  ASSERT(IsAligned(size_in_bytes, kPointerSize));

  if (needs_rebuild_) RebuildSizeList();
  int index = size_in_bytes >> kPointerSizeLog2;
  // Check for a perfect fit.
  if (free_[index].head_node_ != NULL) {
    FreeListNode* node = FreeListNode::FromAddress(free_[index].head_node_);
    // If this was the last block of its size, remove the size.
    if ((free_[index].head_node_ = node->next()) == NULL) RemoveSize(index);
    available_ -= size_in_bytes;
    *wasted_bytes = 0;
    return node;
  }
  // Search the size list for the best fit.
  int prev = finger_ < index ? finger_ : kHead;
  int cur = FindSize(index, &prev);
  ASSERT(index < cur);
  if (cur == kEnd) {
    // No large enough size in list.
    *wasted_bytes = 0;
    return Failure::RetryAfterGC(size_in_bytes, owner_);
  }
  int rem = cur - index;
  int rem_bytes = rem << kPointerSizeLog2;
  FreeListNode* cur_node = FreeListNode::FromAddress(free_[cur].head_node_);
  FreeListNode* rem_node = FreeListNode::FromAddress(free_[cur].head_node_ +
                                                     size_in_bytes);
  // Distinguish the cases prev < rem < cur and rem <= prev < cur
  // to avoid many redundant tests and calls to Insert/RemoveSize.
  if (prev < rem) {
    // Simple case: insert rem between prev and cur.
    finger_ = prev;
    free_[prev].next_size_ = rem;
    // If this was the last block of size cur, remove the size.
    if ((free_[cur].head_node_ = cur_node->next()) == NULL) {
      free_[rem].next_size_ = free_[cur].next_size_;
    } else {
      free_[rem].next_size_ = cur;
    }
    // Add the remainder block.
    rem_node->set_size(rem_bytes);
    rem_node->set_next(free_[rem].head_node_);
    free_[rem].head_node_ = rem_node->address();
  } else {
    // If this was the last block of size cur, remove the size.
    if ((free_[cur].head_node_ = cur_node->next()) == NULL) {
      finger_ = prev;
      free_[prev].next_size_ = free_[cur].next_size_;
    }
    if (rem_bytes < kMinBlockSize) {
      // Too-small remainder is wasted.
      rem_node->set_size(rem_bytes);
      available_ -= size_in_bytes + rem_bytes;
      *wasted_bytes = rem_bytes;
      return cur_node;
    }
    // Add the remainder block and, if needed, insert its size.
    rem_node->set_size(rem_bytes);
    rem_node->set_next(free_[rem].head_node_);
    free_[rem].head_node_ = rem_node->address();
    if (rem_node->next() == NULL) InsertSize(rem);
  }
  available_ -= size_in_bytes;
  *wasted_bytes = 0;
  return cur_node;
}


MapSpaceFreeList::MapSpaceFreeList() {
  Reset();
}


void MapSpaceFreeList::Reset() {
  available_ = 0;
  head_ = NULL;
}


void MapSpaceFreeList::Free(Address start) {
#ifdef DEBUG
  for (int i = 0; i < Map::kSize; i += kPointerSize) {
    Memory::Address_at(start + i) = kZapValue;
  }
#endif
  FreeListNode* node = FreeListNode::FromAddress(start);
  node->set_size(Map::kSize);
  node->set_next(head_);
  head_ = node->address();
  available_ += Map::kSize;
}


Object* MapSpaceFreeList::Allocate() {
  if (head_ == NULL) {
    return Failure::RetryAfterGC(Map::kSize, MAP_SPACE);
  }

  FreeListNode* node = FreeListNode::FromAddress(head_);
  head_ = node->next();
  available_ -= Map::kSize;
  return node;
}


// -----------------------------------------------------------------------------
// OldSpace implementation

void OldSpace::PrepareForMarkCompact(bool will_compact) {
  if (will_compact) {
    // Reset relocation info.  During a compacting collection, everything in
    // the space is considered 'available' and we will rediscover live data
    // and waste during the collection.
    MCResetRelocationInfo();
    mc_end_of_relocation_ = bottom();
    ASSERT(Available() == Capacity());
  } else {
    // During a non-compacting collection, everything below the linear
    // allocation pointer is considered allocated (everything above is
    // available) and we will rediscover available and wasted bytes during
    // the collection.
    accounting_stats_.AllocateBytes(free_list_.available());
    accounting_stats_.FillWastedBytes(Waste());
  }

  // Clear the free list and switch to linear allocation if we are in FREE_LIST
  free_list_.Reset();
  if (allocation_mode_ == FREE_LIST) allocation_mode_ = LINEAR;
}


void OldSpace::MCAdjustRelocationEnd(Address address, int size_in_bytes) {
  ASSERT(Contains(address));
  Address current_top = mc_end_of_relocation_;
  Page* current_page = Page::FromAllocationTop(current_top);

  // No more objects relocated to this page?  Move to the next.
  ASSERT(current_top <= current_page->mc_relocation_top);
  if (current_top == current_page->mc_relocation_top) {
    // The space should already be properly expanded.
    Page* next_page = current_page->next_page();
    CHECK(next_page->is_valid());
    mc_end_of_relocation_ = next_page->ObjectAreaStart();
  }
  ASSERT(mc_end_of_relocation_ == address);
  mc_end_of_relocation_ += size_in_bytes;
}


void OldSpace::MCCommitRelocationInfo() {
  // Update fast allocation info.
  allocation_info_.top = mc_forwarding_info_.top;
  allocation_info_.limit = mc_forwarding_info_.limit;
  ASSERT_PAGED_ALLOCATION_INFO(allocation_info_);

  // The space is compacted and we haven't yet built free lists or
  // wasted any space.
  ASSERT(Waste() == 0);
  ASSERT(AvailableFree() == 0);

  // Build the free list for the space.
  int computed_size = 0;
  PageIterator it(this, PageIterator::PAGES_USED_BY_MC);
  while (it.has_next()) {
    Page* p = it.next();
    // Space below the relocation pointer is allocated.
    computed_size += p->mc_relocation_top - p->ObjectAreaStart();
    if (it.has_next()) {
      // Free the space at the top of the page.  We cannot use
      // p->mc_relocation_top after the call to Free (because Free will clear
      // remembered set bits).
      int extra_size = p->ObjectAreaEnd() - p->mc_relocation_top;
      if (extra_size > 0) {
        int wasted_bytes = free_list_.Free(p->mc_relocation_top, extra_size);
        // The bytes we have just "freed" to add to the free list were
        // already accounted as available.
        accounting_stats_.WasteBytes(wasted_bytes);
      }
    }
  }

  // Make sure the computed size - based on the used portion of the pages in
  // use - matches the size obtained while computing forwarding addresses.
  ASSERT(computed_size == Size());
}


Object* OldSpace::AllocateRawInternal(int size_in_bytes,
                                      AllocationInfo* alloc_info) {
  ASSERT(HasBeenSetup());

  if (allocation_mode_ == LINEAR_ONLY || allocation_mode_ == LINEAR) {
    // Try linear allocation in the current page.
    Address cur_top = alloc_info->top;
    Address new_top = cur_top + size_in_bytes;
    if (new_top <= alloc_info->limit) {
      Object* obj = HeapObject::FromAddress(cur_top);
      alloc_info->top = new_top;
      ASSERT_PAGED_ALLOCATION_INFO(*alloc_info);

      accounting_stats_.AllocateBytes(size_in_bytes);
      ASSERT(Size() <= Capacity());
      return obj;
    }
  } else {
    // For now we should not try free list allocation during m-c relocation.
    ASSERT(alloc_info == &allocation_info_);
    int wasted_bytes;
    Object* object = free_list_.Allocate(size_in_bytes, &wasted_bytes);
    accounting_stats_.WasteBytes(wasted_bytes);
    if (!object->IsFailure()) {
      accounting_stats_.AllocateBytes(size_in_bytes);
      return object;
    }
  }
  // Fast allocation failed.
  return SlowAllocateRaw(size_in_bytes, alloc_info);
}


// Slow cases for AllocateRawInternal.  In linear allocation mode, try
// to allocate in the next page in the space.  If there are no more
// pages, switch to free-list allocation if permitted, otherwise try
// to grow the space.  In free-list allocation mode, try to grow the
// space and switch to linear allocation.
Object* OldSpace::SlowAllocateRaw(int size_in_bytes,
                                  AllocationInfo* alloc_info) {
  if (allocation_mode_ == LINEAR_ONLY || allocation_mode_ == LINEAR) {
    Page* top_page = TopPageOf(*alloc_info);
    // Until we implement free-list allocation during global gc, we have two
    // cases: one for normal allocation and one for m-c relocation allocation.
    if (alloc_info == &allocation_info_) {  // Normal allocation.
      int free_size = top_page->ObjectAreaEnd() - alloc_info->top;
      // Add the extra space at the top of this page to the free list.
      if (free_size > 0) {
        int wasted_bytes = free_list_.Free(alloc_info->top, free_size);
        accounting_stats_.WasteBytes(wasted_bytes);
        alloc_info->top += free_size;
        ASSERT_PAGED_ALLOCATION_INFO(*alloc_info);
      }

      // Move to the next page in this space if there is one; switch
      // to free-list allocation, if we can; try to expand the space otherwise
      if (top_page->next_page()->is_valid()) {
        SetAllocationInfo(alloc_info, top_page->next_page());
      } else if (allocation_mode_ == LINEAR) {
        allocation_mode_ = FREE_LIST;
      } else if (Expand(top_page)) {
        ASSERT(top_page->next_page()->is_valid());
        SetAllocationInfo(alloc_info, top_page->next_page());
      } else {
        return Failure::RetryAfterGC(size_in_bytes, identity());
      }
    } else {  // Allocation during m-c relocation.
      // During m-c 'allocation' while computing forwarding addresses, we do
      // not yet add blocks to the free list because they still contain live
      // objects.  We also cache the m-c forwarding allocation pointer in the
      // current page.

      // If there are no more pages try to expand the space.  This can only
      // happen when promoting objects from the new space.
      if (!top_page->next_page()->is_valid()) {
        if (!Expand(top_page)) {
          return Failure::RetryAfterGC(size_in_bytes, identity());
        }
      }

      // Move to the next page.
      ASSERT(top_page->next_page()->is_valid());
      top_page->mc_relocation_top = alloc_info->top;
      SetAllocationInfo(alloc_info, top_page->next_page());
    }
  } else {  // Free-list allocation.
    // We failed to allocate from the free list; try to expand the space and
    // switch back to linear allocation.
    ASSERT(alloc_info == &allocation_info_);
    Page* top_page = TopPageOf(*alloc_info);
    if (!top_page->next_page()->is_valid()) {
      if (!Expand(top_page)) {
        return Failure::RetryAfterGC(size_in_bytes, identity());
      }
    }

    // We surely have more pages, move to the next page and switch to linear
    // allocation.
    ASSERT(top_page->next_page()->is_valid());
    SetAllocationInfo(alloc_info, top_page->next_page());
    ASSERT(allocation_mode_ == FREE_LIST);
    allocation_mode_ = LINEAR;
  }

  // Perform the allocation.
  return AllocateRawInternal(size_in_bytes, alloc_info);
}


#ifdef DEBUG
// We do not assume that the PageIterator works, because it depends on the
// invariants we are checking during verification.
void OldSpace::Verify() {
  // The allocation pointer should be valid, and it should be in a page in the
  // space.
  ASSERT_PAGED_ALLOCATION_INFO(allocation_info_);
  Page* top_page = Page::FromAllocationTop(allocation_info_.top);
  ASSERT(MemoryAllocator::IsPageInSpace(top_page, this));

  // Loop over all the pages.
  bool above_allocation_top = false;
  Page* current_page = first_page_;
  while (current_page->is_valid()) {
    if (above_allocation_top) {
      // We don't care what's above the allocation top.
    } else {
      // Unless this is the last page in the space containing allocated
      // objects, the allocation top should be at the object area end.
      Address top = current_page->AllocationTop();
      if (current_page == top_page) {
        ASSERT(top == allocation_info_.top);
        // The next page will be above the allocation top.
        above_allocation_top = true;
      } else {
        ASSERT(top == current_page->ObjectAreaEnd());
      }

      // It should be packed with objects from the bottom to the top.
      Address current = current_page->ObjectAreaStart();
      while (current < top) {
        HeapObject* object = HeapObject::FromAddress(current);

        // The first word should be a map, and we expect all map pointers to
        // be in map space.
        Map* map = object->map();
        ASSERT(map->IsMap());
        ASSERT(Heap::map_space()->Contains(map));

        // The object should not be a map.
        ASSERT(!object->IsMap());

        // The object itself should look OK.
        // This is blocked by bug #1006953.
        // object->Verify();

        // All the interior pointers should be contained in the heap and have
        // their remembered set bits set if they point to new space.  Code
        // objects do not have remembered set bits that we care about.
        VerifyPointersAndRSetVisitor rset_visitor;
        VerifyPointersVisitor no_rset_visitor;
        int size = object->Size();
        if (object->IsCode()) {
          Code::cast(object)->ConvertICTargetsFromAddressToObject();
          object->IterateBody(map->instance_type(), size, &no_rset_visitor);
          Code::cast(object)->ConvertICTargetsFromObjectToAddress();
        } else {
          object->IterateBody(map->instance_type(), size, &rset_visitor);
        }

        current += size;
      }

      // The allocation pointer should not be in the middle of an object.
      ASSERT(current == top);
    }

    current_page = current_page->next_page();
  }
}


struct CommentStatistic {
  const char* comment;
  int size;
  int count;
  void Clear() {
    comment = NULL;
    size = 0;
    count = 0;
  }
};


// must be small, since an iteration is used for lookup
const int kMaxComments = 64;
static CommentStatistic comments_statistics[kMaxComments+1];


void PagedSpace::ReportCodeStatistics() {
  ReportCodeKindStatistics();
  PrintF("Code comment statistics (\"   [ comment-txt   :    size/   "
         "count  (average)\"):\n");
  for (int i = 0; i <= kMaxComments; i++) {
    const CommentStatistic& cs = comments_statistics[i];
    if (cs.size > 0) {
      PrintF("   %-30s: %10d/%6d     (%d)\n", cs.comment, cs.size, cs.count,
             cs.size/cs.count);
    }
  }
  PrintF("\n");
}


void PagedSpace::ResetCodeStatistics() {
  ClearCodeKindStatistics();
  for (int i = 0; i < kMaxComments; i++) comments_statistics[i].Clear();
  comments_statistics[kMaxComments].comment = "Unknown";
  comments_statistics[kMaxComments].size = 0;
  comments_statistics[kMaxComments].count = 0;
}


// Adds comment to 'comment_statistics' table. Performance OK sa long as
// 'kMaxComments' is small
static void EnterComment(const char* comment, int delta) {
  // Do not count empty comments
  if (delta <= 0) return;
  CommentStatistic* cs = &comments_statistics[kMaxComments];
  // Search for a free or matching entry in 'comments_statistics': 'cs'
  // points to result.
  for (int i = 0; i < kMaxComments; i++) {
    if (comments_statistics[i].comment == NULL) {
      cs = &comments_statistics[i];
      cs->comment = comment;
      break;
    } else if (strcmp(comments_statistics[i].comment, comment) == 0) {
      cs = &comments_statistics[i];
      break;
    }
  }
  // Update entry for 'comment'
  cs->size += delta;
  cs->count += 1;
}


// Call for each nested comment start (start marked with '[ xxx', end marked
// with ']'.  RelocIterator 'it' must point to a comment reloc info.
static void CollectCommentStatistics(RelocIterator* it) {
  ASSERT(!it->done());
  ASSERT(it->rinfo()->rmode() == comment);
  const char* tmp = reinterpret_cast<const char*>(it->rinfo()->data());
  if (tmp[0] != '[') {
    // Not a nested comment; skip
    return;
  }

  // Search for end of nested comment or a new nested comment
  const char* const comment_txt =
      reinterpret_cast<const char*>(it->rinfo()->data());
  const byte* prev_pc = it->rinfo()->pc();
  int flat_delta = 0;
  it->next();
  while (true) {
    // All nested comments must be terminated properly, and therefore exit
    // from loop.
    ASSERT(!it->done());
    if (it->rinfo()->rmode() == comment) {
      const char* const txt =
          reinterpret_cast<const char*>(it->rinfo()->data());
      flat_delta += it->rinfo()->pc() - prev_pc;
      if (txt[0] == ']') break;  // End of nested  comment
      // A new comment
      CollectCommentStatistics(it);
      // Skip code that was covered with previous comment
      prev_pc = it->rinfo()->pc();
    }
    it->next();
  }
  EnterComment(comment_txt, flat_delta);
}


// Collects code size statistics:
// - by code kind
// - by code comment
void PagedSpace::CollectCodeStatistics() {
  HeapObjectIterator obj_it(this);
  while (obj_it.has_next()) {
    HeapObject* obj = obj_it.next();
    if (obj->IsCode()) {
      Code* code = Code::cast(obj);
      code_kind_statistics[code->kind()] += code->Size();
      RelocIterator it(code);
      int delta = 0;
      const byte* prev_pc = code->instruction_start();
      while (!it.done()) {
        if (it.rinfo()->rmode() == comment) {
          delta += it.rinfo()->pc() - prev_pc;
          CollectCommentStatistics(&it);
          prev_pc = it.rinfo()->pc();
        }
        it.next();
      }

      ASSERT(code->instruction_start() <= prev_pc &&
             prev_pc <= code->relocation_start());
      delta += code->relocation_start() - prev_pc;
      EnterComment("NoComment", delta);
    }
  }
}


void OldSpace::ReportStatistics() {
  int pct = Available() * 100 / Capacity();
  PrintF("  capacity: %d, waste: %d, available: %d, %%%d\n",
         Capacity(), Waste(), Available(), pct);

  // Report remembered set statistics.
  int rset_marked_pointers = 0;
  int rset_marked_arrays = 0;
  int rset_marked_array_elements = 0;
  int cross_gen_pointers = 0;
  int cross_gen_array_elements = 0;

  PageIterator page_it(this, PageIterator::PAGES_IN_USE);
  while (page_it.has_next()) {
    Page* p = page_it.next();

    for (Address rset_addr = p->RSetStart();
         rset_addr < p->RSetEnd();
         rset_addr += kIntSize) {
      int rset = Memory::int_at(rset_addr);
      if (rset != 0) {
        // Bits were set
        int intoff = rset_addr - p->address();
        int bitoff = 0;
        for (; bitoff < kBitsPerInt; ++bitoff) {
          if ((rset & (1 << bitoff)) != 0) {
            int bitpos = intoff*kBitsPerByte + bitoff;
            Address slot = p->OffsetToAddress(bitpos << kObjectAlignmentBits);
            Object** obj = reinterpret_cast<Object**>(slot);
            if (*obj == Heap::fixed_array_map()) {
              rset_marked_arrays++;
              FixedArray* fa = FixedArray::cast(HeapObject::FromAddress(slot));

              rset_marked_array_elements += fa->length();
              // Manually inline FixedArray::IterateBody
              Address elm_start = slot + FixedArray::kHeaderSize;
              Address elm_stop = elm_start + fa->length() * kPointerSize;
              for (Address elm_addr = elm_start;
                   elm_addr < elm_stop; elm_addr += kPointerSize) {
                // Filter non-heap-object pointers
                Object** elm_p = reinterpret_cast<Object**>(elm_addr);
                if (Heap::InNewSpace(*elm_p))
                  cross_gen_array_elements++;
              }
            } else {
              rset_marked_pointers++;
              if (Heap::InNewSpace(*obj))
                cross_gen_pointers++;
            }
          }
        }
      }
    }
  }

  pct = rset_marked_pointers == 0 ?
        0 : cross_gen_pointers * 100 / rset_marked_pointers;
  PrintF("  rset-marked pointers %d, to-new-space %d (%%%d)\n",
            rset_marked_pointers, cross_gen_pointers, pct);
  PrintF("  rset_marked arrays %d, ", rset_marked_arrays);
  PrintF("  elements %d, ", rset_marked_array_elements);
  pct = rset_marked_array_elements == 0 ? 0
           : cross_gen_array_elements * 100 / rset_marked_array_elements;
  PrintF("  pointers to new space %d (%%%d)\n", cross_gen_array_elements, pct);
  PrintF("  total rset-marked bits %d\n",
            (rset_marked_pointers + rset_marked_arrays));
  pct = (rset_marked_pointers + rset_marked_array_elements) == 0 ? 0
        : (cross_gen_pointers + cross_gen_array_elements) * 100 /
          (rset_marked_pointers + rset_marked_array_elements);
  PrintF("  total rset pointers %d, true cross generation ones %d (%%%d)\n",
         (rset_marked_pointers + rset_marked_array_elements),
         (cross_gen_pointers + cross_gen_array_elements),
         pct);

  ClearHistograms();
  HeapObjectIterator obj_it(this);
  while (obj_it.has_next()) { CollectHistogramInfo(obj_it.next()); }
  ReportHistogram(true);
}


// Dump the range of remembered set words between [start, end) corresponding
// to the pointers starting at object_p.  The allocation_top is an object
// pointer which should not be read past.  This is important for large object
// pages, where some bits in the remembered set range do not correspond to
// allocated addresses.
static void PrintRSetRange(Address start, Address end, Object** object_p,
                           Address allocation_top) {
  Address rset_address = start;

  // If the range starts on on odd numbered word (eg, for large object extra
  // remembered set ranges), print some spaces.
  if ((reinterpret_cast<uint32_t>(start) / kIntSize) % 2 == 1) {
    PrintF("                                    ");
  }

  // Loop over all the words in the range.
  while (rset_address < end) {
    uint32_t rset_word = Memory::uint32_at(rset_address);
    int bit_position = 0;

    // Loop over all the bits in the word.
    while (bit_position < kBitsPerInt) {
      if (object_p == reinterpret_cast<Object**>(allocation_top)) {
        // Print a bar at the allocation pointer.
        PrintF("|");
      } else if (object_p > reinterpret_cast<Object**>(allocation_top)) {
        // Do not dereference object_p past the allocation pointer.
        PrintF("#");
      } else if ((rset_word & (1 << bit_position)) == 0) {
        // Print a dot for zero bits.
        PrintF(".");
      } else if (Heap::InNewSpace(*object_p)) {
        // Print an X for one bits for pointers to new space.
        PrintF("X");
      } else {
        // Print a circle for one bits for pointers to old space.
        PrintF("o");
      }

      // Print a space after every 8th bit except the last.
      if (bit_position % 8 == 7 && bit_position != (kBitsPerInt - 1)) {
        PrintF(" ");
      }

      // Advance to next bit.
      bit_position++;
      object_p++;
    }

    // Print a newline after every odd numbered word, otherwise a space.
    if ((reinterpret_cast<uint32_t>(rset_address) / kIntSize) % 2 == 1) {
      PrintF("\n");
    } else {
      PrintF(" ");
    }

    // Advance to next remembered set word.
    rset_address += kIntSize;
  }
}


void PagedSpace::DoPrintRSet(const char* space_name) {
  PageIterator it(this, PageIterator::PAGES_IN_USE);
  while (it.has_next()) {
    Page* p = it.next();
    PrintF("%s page 0x%x:\n", space_name, p);
    PrintRSetRange(p->RSetStart(), p->RSetEnd(),
                   reinterpret_cast<Object**>(p->ObjectAreaStart()),
                   p->AllocationTop());
    PrintF("\n");
  }
}


void OldSpace::PrintRSet() { DoPrintRSet("old"); }
#endif

// -----------------------------------------------------------------------------
// MapSpace implementation

void MapSpace::PrepareForMarkCompact(bool will_compact) {
  if (will_compact) {
    // Reset relocation info.
    MCResetRelocationInfo();

    // Initialize map index entry.
    int page_count = 0;
    PageIterator it(this, PageIterator::ALL_PAGES);
    while (it.has_next()) {
      ASSERT_MAP_PAGE_INDEX(page_count);

      Page* p = it.next();
      ASSERT(p->mc_page_index == page_count);

      page_addresses_[page_count++] = p->address();
    }

    // During a compacting collection, everything in the space is considered
    // 'available' (set by the call to MCResetRelocationInfo) and we will
    // rediscover live and wasted bytes during the collection.
    ASSERT(Available() == Capacity());
  } else {
    // During a non-compacting collection, everything below the linear
    // allocation pointer except wasted top-of-page blocks is considered
    // allocated and we will rediscover available bytes during the
    // collection.
    accounting_stats_.AllocateBytes(free_list_.available());
  }

  // Clear the free list and switch to linear allocation if not already
  // required.
  free_list_.Reset();
  if (allocation_mode_ != LINEAR_ONLY) allocation_mode_ = LINEAR;
}


void MapSpace::MCCommitRelocationInfo() {
  // Update fast allocation info.
  allocation_info_.top = mc_forwarding_info_.top;
  allocation_info_.limit = mc_forwarding_info_.limit;
  ASSERT_PAGED_ALLOCATION_INFO(allocation_info_);

  // The space is compacted and we haven't yet wasted any space.
  ASSERT(Waste() == 0);

  // Update allocation_top of each page in use and compute waste.
  int computed_size = 0;
  PageIterator it(this, PageIterator::PAGES_USED_BY_MC);
  while (it.has_next()) {
    Page* page = it.next();
    Address page_top = page->AllocationTop();
    computed_size += page_top - page->ObjectAreaStart();
    if (it.has_next()) {
      accounting_stats_.WasteBytes(page->ObjectAreaEnd() - page_top);
    }
  }

  // Make sure the computed size - based on the used portion of the
  // pages in use - matches the size we adjust during allocation.
  ASSERT(computed_size == Size());
}


Object* MapSpace::AllocateRawInternal(int size_in_bytes,
                                      AllocationInfo* alloc_info) {
  ASSERT(HasBeenSetup());
  // When doing free-list allocation, we implicitly assume that we always
  // allocate a map-sized block.
  ASSERT(size_in_bytes == Map::kSize);

  if (allocation_mode_ == LINEAR_ONLY || allocation_mode_ == LINEAR) {
    // Try linear allocation in the current page.
    Address cur_top = alloc_info->top;
    Address new_top = cur_top + size_in_bytes;
    if (new_top <= alloc_info->limit) {
      Object* obj = HeapObject::FromAddress(cur_top);
      alloc_info->top = new_top;
      ASSERT_PAGED_ALLOCATION_INFO(*alloc_info);

      accounting_stats_.AllocateBytes(size_in_bytes);
      return obj;
    }
  } else {
    // We should not do free list allocation during m-c compaction.
    ASSERT(alloc_info == &allocation_info_);
    Object* object = free_list_.Allocate();
    if (!object->IsFailure()) {
      accounting_stats_.AllocateBytes(size_in_bytes);
      return object;
    }
  }
  // Fast allocation failed.
  return SlowAllocateRaw(size_in_bytes, alloc_info);
}


// Slow case for AllocateRawInternal.  In linear allocation mode, try to
// allocate in the next page in the space.  If there are no more pages, switch
// to free-list allocation.  In free-list allocation mode, try to grow the
// space and switch to linear allocation.
Object* MapSpace::SlowAllocateRaw(int size_in_bytes,
                                  AllocationInfo* alloc_info) {
  if (allocation_mode_ == LINEAR_ONLY || allocation_mode_ == LINEAR) {
    Page* top_page = TopPageOf(*alloc_info);

    // We do not do free-list allocation during compacting GCs.
    if (alloc_info == &mc_forwarding_info_) {
      // We expect to always have more pages, because the map space cannot
      // grow during GC.  Move to the next page.
      CHECK(top_page->next_page()->is_valid());
      top_page->mc_relocation_top = alloc_info->top;
      SetAllocationInfo(alloc_info, top_page->next_page());
    } else {  // Normal allocation.
      // Move to the next page in this space (counting the top-of-page block
      // as waste) if there is one, otherwise switch to free-list allocation if
      // permitted, otherwise try to expand the heap
      if (top_page->next_page()->is_valid() ||
          (allocation_mode_ == LINEAR_ONLY && Expand(top_page))) {
        int free_size = top_page->ObjectAreaEnd() - alloc_info->top;
        ASSERT(free_size == kPageExtra);
        accounting_stats_.WasteBytes(free_size);
        SetAllocationInfo(alloc_info, top_page->next_page());
      } else if (allocation_mode_ == LINEAR) {
        allocation_mode_ = FREE_LIST;
      } else {
        return Failure::RetryAfterGC(size_in_bytes, MAP_SPACE);
      }
    }
  } else {  // Free-list allocation.
    ASSERT(alloc_info == &allocation_info_);
    // We failed to allocate from the free list (ie, it must be empty) so try
    // to expand the space and switch back to linear allocation.
    Page* top_page = TopPageOf(*alloc_info);
    if (!top_page->next_page()->is_valid()) {
      if (!Expand(top_page)) {
        return Failure::RetryAfterGC(size_in_bytes, MAP_SPACE);
      }
    }

    // We have more pages now so we can move to the next and switch to linear
    // allocation.
    ASSERT(top_page->next_page()->is_valid());
    int free_size = top_page->ObjectAreaEnd() - alloc_info->top;
    ASSERT(free_size == kPageExtra);
    accounting_stats_.WasteBytes(free_size);
    SetAllocationInfo(alloc_info, top_page->next_page());
    ASSERT(allocation_mode_ == FREE_LIST);
    allocation_mode_ = LINEAR;
  }

  // Perform the allocation.
  return AllocateRawInternal(size_in_bytes, alloc_info);
}


#ifdef DEBUG
// We do not assume that the PageIterator works, because it depends on the
// invariants we are checking during verification.
void MapSpace::Verify() {
  // The allocation pointer should be valid, and it should be in a page in the
  // space.
  ASSERT_PAGED_ALLOCATION_INFO(allocation_info_);
  Page* top_page = Page::FromAllocationTop(allocation_info_.top);
  ASSERT(MemoryAllocator::IsPageInSpace(top_page, this));

  // Loop over all the pages.
  bool above_allocation_top = false;
  Page* current_page = first_page_;
  while (current_page->is_valid()) {
    if (above_allocation_top) {
      // We don't care what's above the allocation top.
    } else {
      // Unless this is the last page in the space containing allocated
      // objects, the allocation top should be at a constant offset from the
      // object area end.
      Address top = current_page->AllocationTop();
      if (current_page == top_page) {
        ASSERT(top == allocation_info_.top);
        // The next page will be above the allocation top.
        above_allocation_top = true;
      } else {
        ASSERT(top == current_page->ObjectAreaEnd() - kPageExtra);
      }

      // It should be packed with objects from the bottom to the top.
      Address current = current_page->ObjectAreaStart();
      while (current < top) {
        HeapObject* object = HeapObject::FromAddress(current);

        // The first word should be a map, and we expect all map pointers to
        // be in map space.
        Map* map = object->map();
        ASSERT(map->IsMap());
        ASSERT(Heap::map_space()->Contains(map));

        // The object should be a map or a byte array.
        ASSERT(object->IsMap() || object->IsByteArray());

        // The object itself should look OK.
        // This is blocked by bug #1006953.
        // object->Verify();

        // All the interior pointers should be contained in the heap and
        // have their remembered set bits set if they point to new space.
        VerifyPointersAndRSetVisitor visitor;
        int size = object->Size();
        object->IterateBody(map->instance_type(), size, &visitor);

        current += size;
      }

      // The allocation pointer should not be in the middle of an object.
      ASSERT(current == top);
    }

    current_page = current_page->next_page();
  }
}


void MapSpace::ReportStatistics() {
  int pct = Available() * 100 / Capacity();
  PrintF("  capacity: %d, waste: %d, available: %d, %%%d\n",
         Capacity(), Waste(), Available(), pct);

  // Report remembered set statistics.
  int rset_marked_pointers = 0;
  int cross_gen_pointers = 0;

  PageIterator page_it(this, PageIterator::PAGES_IN_USE);
  while (page_it.has_next()) {
    Page* p = page_it.next();

    for (Address rset_addr = p->RSetStart();
         rset_addr < p->RSetEnd();
         rset_addr += kIntSize) {
      int rset = Memory::int_at(rset_addr);
      if (rset != 0) {
        // Bits were set
        int intoff = rset_addr - p->address();
        int bitoff = 0;
        for (; bitoff < kBitsPerInt; ++bitoff) {
          if ((rset & (1 << bitoff)) != 0) {
            int bitpos = intoff*kBitsPerByte + bitoff;
            Address slot = p->OffsetToAddress(bitpos << kObjectAlignmentBits);
            Object** obj = reinterpret_cast<Object**>(slot);
            rset_marked_pointers++;
            if (Heap::InNewSpace(*obj))
              cross_gen_pointers++;
          }
        }
      }
    }
  }

  pct = rset_marked_pointers == 0 ?
          0 : cross_gen_pointers * 100 / rset_marked_pointers;
  PrintF("  rset-marked pointers %d, to-new-space %d (%%%d)\n",
            rset_marked_pointers, cross_gen_pointers, pct);

  ClearHistograms();
  HeapObjectIterator obj_it(this);
  while (obj_it.has_next()) { CollectHistogramInfo(obj_it.next()); }
  ReportHistogram(false);
}


void MapSpace::PrintRSet() { DoPrintRSet("map"); }
#endif


// -----------------------------------------------------------------------------
// LargeObjectIterator

LargeObjectIterator::LargeObjectIterator(LargeObjectSpace* space) {
  current_ = space->first_chunk_;
  size_func_ = NULL;
}


LargeObjectIterator::LargeObjectIterator(LargeObjectSpace* space,
                                         HeapObjectCallback size_func) {
  current_ = space->first_chunk_;
  size_func_ = size_func;
}


HeapObject* LargeObjectIterator::next() {
  ASSERT(has_next());
  HeapObject* object = current_->GetObject();
  current_ = current_->next();
  return object;
}


// -----------------------------------------------------------------------------
// LargeObjectChunk

LargeObjectChunk* LargeObjectChunk::New(int size_in_bytes,
                                        size_t* chunk_size) {
  size_t requested = ChunkSizeFor(size_in_bytes);
  void* mem = MemoryAllocator::AllocateRawMemory(requested, chunk_size);
  if (mem == NULL) return NULL;
  LOG(NewEvent("LargeObjectChunk", mem, *chunk_size));
  if (*chunk_size < requested) {
    MemoryAllocator::FreeRawMemory(mem, *chunk_size);
    LOG(DeleteEvent("LargeObjectChunk", mem));
    return NULL;
  }
  return reinterpret_cast<LargeObjectChunk*>(mem);
}


int LargeObjectChunk::ChunkSizeFor(int size_in_bytes) {
  int os_alignment = OS::AllocateAlignment();
  if (os_alignment < Page::kPageSize)
    size_in_bytes += (Page::kPageSize - os_alignment);
  return size_in_bytes + Page::kObjectStartOffset;
}

// -----------------------------------------------------------------------------
// LargeObjectSpace

LargeObjectSpace::LargeObjectSpace()
    : first_chunk_(NULL),
      size_(0),
      page_count_(0) {}


bool LargeObjectSpace::Setup() {
  first_chunk_ = NULL;
  size_ = 0;
  page_count_ = 0;
  return true;
}


void LargeObjectSpace::TearDown() {
  while (first_chunk_ != NULL) {
    LargeObjectChunk* chunk = first_chunk_;
    first_chunk_ = first_chunk_->next();
    LOG(DeleteEvent("LargeObjectChunk", chunk->address()));
    MemoryAllocator::FreeRawMemory(chunk->address(), chunk->size());
  }

  size_ = 0;
  page_count_ = 0;
}


Object* LargeObjectSpace::AllocateRawInternal(int requested_size,
                                              int object_size) {
  ASSERT(0 < object_size && object_size <= requested_size);
  size_t chunk_size;
  LargeObjectChunk* chunk =
      LargeObjectChunk::New(requested_size, &chunk_size);
  if (chunk == NULL) {
    return Failure::RetryAfterGC(requested_size, LO_SPACE);
  }

  size_ += chunk_size;
  page_count_++;
  chunk->set_next(first_chunk_);
  chunk->set_size(chunk_size);
  first_chunk_ = chunk;

  // Set the object address and size in the page header and clear its
  // remembered set.
  Page* page = Page::FromAddress(RoundUp(chunk->address(), Page::kPageSize));
  Address object_address = page->ObjectAreaStart();
  // Clear the low order bit of the second word in the page to flag it as a
  // large object page.  If the chunk_size happened to be written there, its
  // low order bit should already be clear.
  ASSERT((chunk_size & 0x1) == 0);
  page->is_normal_page &= ~0x1;
  page->ClearRSet();
  int extra_bytes = requested_size - object_size;
  if (extra_bytes > 0) {
    // The extra memory for the remembered set should be cleared.
    memset(object_address + object_size, 0, extra_bytes);
  }

  return HeapObject::FromAddress(object_address);
}


Object* LargeObjectSpace::AllocateRaw(int size_in_bytes) {
  ASSERT(0 < size_in_bytes);
  return AllocateRawInternal(size_in_bytes, size_in_bytes);
}


Object* LargeObjectSpace::AllocateRawFixedArray(int size_in_bytes) {
  int extra_rset_bytes = ExtraRSetBytesFor(size_in_bytes);
  return AllocateRawInternal(size_in_bytes + extra_rset_bytes, size_in_bytes);
}


// GC support
Object* LargeObjectSpace::FindObject(Address a) {
  for (LargeObjectChunk* chunk = first_chunk_;
       chunk != NULL;
       chunk = chunk->next()) {
    Address chunk_address = chunk->address();
    if (chunk_address <= a && a < chunk_address + chunk->size()) {
      return chunk->GetObject();
    }
  }
  return Failure::Exception();
}


void LargeObjectSpace::ClearRSet() {
  ASSERT(Page::is_rset_in_use());

  LargeObjectIterator it(this);
  while (it.has_next()) {
    HeapObject* object = it.next();
    // We only have code, sequential strings, or fixed arrays in large
    // object space, and only fixed arrays need remembered set support.
    if (object->IsFixedArray()) {
      // Clear the normal remembered set region of the page;
      Page* page = Page::FromAddress(object->address());
      page->ClearRSet();

      // Clear the extra remembered set.
      int size = object->Size();
      int extra_rset_bytes = ExtraRSetBytesFor(size);
      memset(object->address() + size, 0, extra_rset_bytes);
    }
  }
}


void LargeObjectSpace::IterateRSet(ObjectSlotCallback copy_object_func) {
  ASSERT(Page::is_rset_in_use());

  LargeObjectIterator it(this);
  while (it.has_next()) {
    // We only have code, sequential strings, or fixed arrays in large
    // object space, and only fixed arrays can possibly contain pointers to
    // the young generation.
    HeapObject* object = it.next();
    if (object->IsFixedArray()) {
      // Iterate the normal page remembered set range.
      Page* page = Page::FromAddress(object->address());
      Address object_end = object->address() + object->Size();
      Heap::IterateRSetRange(page->ObjectAreaStart(),
                             Min(page->ObjectAreaEnd(), object_end),
                             page->RSetStart(),
                             copy_object_func);

      // Iterate the extra array elements.
      if (object_end > page->ObjectAreaEnd()) {
        Heap::IterateRSetRange(page->ObjectAreaEnd(), object_end,
                               object_end, copy_object_func);
      }
    }
  }
}


void LargeObjectSpace::FreeUnmarkedObjects() {
  LargeObjectChunk* previous = NULL;
  LargeObjectChunk* current = first_chunk_;
  while (current != NULL) {
    HeapObject* object = current->GetObject();
    if (is_marked(object)) {
      clear_mark(object);
      previous = current;
      current = current->next();
    } else {
      Address chunk_address = current->address();
      size_t chunk_size = current->size();

      // Cut the chunk out from the chunk list.
      current = current->next();
      if (previous == NULL) {
        first_chunk_ = current;
      } else {
        previous->set_next(current);
      }

      // Free the chunk.
      if (object->IsCode()) {
        LOG(CodeDeleteEvent(object->address()));
      }
      size_ -= chunk_size;
      page_count_--;
      MemoryAllocator::FreeRawMemory(chunk_address, chunk_size);
      LOG(DeleteEvent("LargeObjectChunk", chunk_address));
    }
  }
}


bool LargeObjectSpace::Contains(HeapObject* object) {
  Address address = object->address();
  Page* page = Page::FromAddress(address);

  SLOW_ASSERT(!page->IsLargeObjectPage()
              || !FindObject(address)->IsFailure());

  return page->IsLargeObjectPage();
}


#ifdef DEBUG
// We do not assume that the large object iterator works, because it depends
// on the invariants we are checking during verification.
void LargeObjectSpace::Verify() {
  for (LargeObjectChunk* chunk = first_chunk_;
       chunk != NULL;
       chunk = chunk->next()) {
    // Each chunk contains an object that starts at the large object page's
    // object area start.
    HeapObject* object = chunk->GetObject();
    Page* page = Page::FromAddress(object->address());
    ASSERT(object->address() == page->ObjectAreaStart());

    // The first word should be a map, and we expect all map pointers to be
    // in map space.
    Map* map = object->map();
    ASSERT(map->IsMap());
    ASSERT(Heap::map_space()->Contains(map));

    // We have only code, sequential strings, fixed arrays, and byte arrays
    // in large object space.
    ASSERT(object->IsCode() || object->IsSeqString()
           || object->IsFixedArray() || object->IsByteArray());

    // The object itself should look OK.
    // This is blocked by bug #1006953.
    // object->Verify();

    // Byte arrays and strings don't have interior pointers.
    if (object->IsCode()) {
      VerifyPointersVisitor code_visitor;
      Code::cast(object)->ConvertICTargetsFromAddressToObject();
      object->IterateBody(map->instance_type(),
                          object->Size(),
                          &code_visitor);
      Code::cast(object)->ConvertICTargetsFromObjectToAddress();
    } else if (object->IsFixedArray()) {
      // We loop over fixed arrays ourselves, rather then using the visitor,
      // because the visitor doesn't support the start/offset iteration
      // needed for IsRSetSet.
      FixedArray* array = FixedArray::cast(object);
      for (int j = 0; j < array->length(); j++) {
        Object* element = array->get(j);
        if (element->IsHeapObject()) {
          HeapObject* element_object = HeapObject::cast(element);
          ASSERT(Heap::Contains(element_object));
          ASSERT(element_object->map()->IsMap());
          if (Heap::InNewSpace(element_object)) {
            ASSERT(Page::IsRSetSet(object->address(),
                                   FixedArray::kHeaderSize + j * kPointerSize));
          }
        }
      }
    }
  }
}


void LargeObjectSpace::Print() {
  LargeObjectIterator it(this);
  while (it.has_next()) {
    it.next()->Print();
  }
}


void LargeObjectSpace::ReportStatistics() {
  PrintF("  size: %d\n", size_);
  int num_objects = 0;
  ClearHistograms();
  LargeObjectIterator it(this);
  while (it.has_next()) {
    num_objects++;
    CollectHistogramInfo(it.next());
  }

  PrintF("  number of objects %d\n", num_objects);
  if (num_objects > 0) ReportHistogram(false);
}


void LargeObjectSpace::CollectCodeStatistics() {
  LargeObjectIterator obj_it(this);
  while (obj_it.has_next()) {
    HeapObject* obj = obj_it.next();
    if (obj->IsCode()) {
      Code* code = Code::cast(obj);
      code_kind_statistics[code->kind()] += code->Size();
    }
  }
}


void LargeObjectSpace::PrintRSet() {
  LargeObjectIterator it(this);
  while (it.has_next()) {
    HeapObject* object = it.next();
    if (object->IsFixedArray()) {
      Page* page = Page::FromAddress(object->address());

      Address allocation_top = object->address() + object->Size();
      PrintF("large page 0x%x:\n", page);
      PrintRSetRange(page->RSetStart(), page->RSetEnd(),
                     reinterpret_cast<Object**>(object->address()),
                     allocation_top);
      int extra_array_bytes = object->Size() - Page::kObjectAreaSize;
      int extra_rset_bits = RoundUp(extra_array_bytes / kPointerSize,
                                    kBitsPerInt);
      PrintF("------------------------------------------------------------"
             "-----------\n");
      PrintRSetRange(allocation_top,
                     allocation_top + extra_rset_bits / kBitsPerByte,
                     reinterpret_cast<Object**>(object->address()
                                                + Page::kObjectAreaSize),
                     allocation_top);
      PrintF("\n");
    }
  }
}
#endif  // DEBUG

} }  // namespace v8::internal
