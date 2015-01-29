// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/zone.h"

#include <cstring>

#ifdef V8_USE_ADDRESS_SANITIZER
#include <sanitizer/asan_interface.h>
#endif  // V8_USE_ADDRESS_SANITIZER

namespace v8 {
namespace internal {

namespace {

#if V8_USE_ADDRESS_SANITIZER

const int kASanRedzoneBytes = 24;  // Must be a multiple of 8.

#else

#define ASAN_POISON_MEMORY_REGION(start, size) \
  do {                                         \
    USE(start);                                \
    USE(size);                                 \
  } while (false)

#define ASAN_UNPOISON_MEMORY_REGION(start, size) \
  do {                                           \
    USE(start);                                  \
    USE(size);                                   \
  } while (false)

const int kASanRedzoneBytes = 0;

#endif  // V8_USE_ADDRESS_SANITIZER

}  // namespace


// Segments represent chunks of memory: They have starting address
// (encoded in the this pointer) and a size in bytes. Segments are
// chained together forming a LIFO structure with the newest segment
// available as segment_head_. Segments are allocated using malloc()
// and de-allocated using free().

class Segment {
 public:
  void Initialize(Segment* next, int size) {
    next_ = next;
    size_ = size;
  }

  Segment* next() const { return next_; }
  void clear_next() { next_ = nullptr; }

  int size() const { return size_; }
  int capacity() const { return size_ - sizeof(Segment); }

  Address start() const { return address(sizeof(Segment)); }
  Address end() const { return address(size_); }

 private:
  // Computes the address of the nth byte in this segment.
  Address address(int n) const {
    return Address(this) + n;
  }

  Segment* next_;
  int size_;
};


Zone::Zone()
    : allocation_size_(0),
      segment_bytes_allocated_(0),
      position_(0),
      limit_(0),
      segment_head_(nullptr) {}


Zone::~Zone() {
  DeleteAll();
  DeleteKeptSegment();

  DCHECK(segment_bytes_allocated_ == 0);
}


void* Zone::New(int size) {
  // Round up the requested size to fit the alignment.
  size = RoundUp(size, kAlignment);

  // If the allocation size is divisible by 8 then we return an 8-byte aligned
  // address.
  if (kPointerSize == 4 && kAlignment == 4) {
    position_ += ((~size) & 4) & (reinterpret_cast<intptr_t>(position_) & 4);
  } else {
    DCHECK(kAlignment >= kPointerSize);
  }

  // Check if the requested size is available without expanding.
  Address result = position_;

  const int size_with_redzone = size + kASanRedzoneBytes;
  if (size_with_redzone > limit_ - position_) {
    result = NewExpand(size_with_redzone);
  } else {
    position_ += size_with_redzone;
  }

  Address redzone_position = result + size;
  DCHECK(redzone_position + kASanRedzoneBytes == position_);
  ASAN_POISON_MEMORY_REGION(redzone_position, kASanRedzoneBytes);

  // Check that the result has the proper alignment and return it.
  DCHECK(IsAddressAligned(result, kAlignment, 0));
  allocation_size_ += size;
  return reinterpret_cast<void*>(result);
}


void Zone::DeleteAll() {
#ifdef DEBUG
  // Constant byte value used for zapping dead memory in debug mode.
  static const unsigned char kZapDeadByte = 0xcd;
#endif

  // Find a segment with a suitable size to keep around.
  Segment* keep = nullptr;
  // Traverse the chained list of segments, zapping (in debug mode)
  // and freeing every segment except the one we wish to keep.
  for (Segment* current = segment_head_; current;) {
    Segment* next = current->next();
    if (!keep && current->size() <= kMaximumKeptSegmentSize) {
      // Unlink the segment we wish to keep from the list.
      keep = current;
      keep->clear_next();
    } else {
      int size = current->size();
#ifdef DEBUG
      // Un-poison first so the zapping doesn't trigger ASan complaints.
      ASAN_UNPOISON_MEMORY_REGION(current, size);
      // Zap the entire current segment (including the header).
      memset(current, kZapDeadByte, size);
#endif
      DeleteSegment(current, size);
    }
    current = next;
  }

  // If we have found a segment we want to keep, we must recompute the
  // variables 'position' and 'limit' to prepare for future allocate
  // attempts. Otherwise, we must clear the position and limit to
  // force a new segment to be allocated on demand.
  if (keep) {
    Address start = keep->start();
    position_ = RoundUp(start, kAlignment);
    limit_ = keep->end();
    // Un-poison so we can re-use the segment later.
    ASAN_UNPOISON_MEMORY_REGION(start, keep->capacity());
#ifdef DEBUG
    // Zap the contents of the kept segment (but not the header).
    memset(start, kZapDeadByte, keep->capacity());
#endif
  } else {
    position_ = limit_ = 0;
  }

  allocation_size_ = 0;
  // Update the head segment to be the kept segment (if any).
  segment_head_ = keep;
}


void Zone::DeleteKeptSegment() {
#ifdef DEBUG
  // Constant byte value used for zapping dead memory in debug mode.
  static const unsigned char kZapDeadByte = 0xcd;
#endif

  DCHECK(segment_head_ == nullptr || segment_head_->next() == nullptr);
  if (segment_head_ != nullptr) {
    int size = segment_head_->size();
#ifdef DEBUG
    // Un-poison first so the zapping doesn't trigger ASan complaints.
    ASAN_UNPOISON_MEMORY_REGION(segment_head_, size);
    // Zap the entire kept segment (including the header).
    memset(segment_head_, kZapDeadByte, size);
#endif
    DeleteSegment(segment_head_, size);
    segment_head_ = nullptr;
  }

  DCHECK(segment_bytes_allocated_ == 0);
}


// Creates a new segment, sets it size, and pushes it to the front
// of the segment chain. Returns the new segment.
Segment* Zone::NewSegment(int size) {
  Segment* result = reinterpret_cast<Segment*>(Malloced::New(size));
  segment_bytes_allocated_ += size;
  if (result != nullptr) {
    result->Initialize(segment_head_, size);
    segment_head_ = result;
  }
  return result;
}


// Deletes the given segment. Does not touch the segment chain.
void Zone::DeleteSegment(Segment* segment, int size) {
  segment_bytes_allocated_ -= size;
  Malloced::Delete(segment);
}


Address Zone::NewExpand(int size) {
  // Make sure the requested size is already properly aligned and that
  // there isn't enough room in the Zone to satisfy the request.
  DCHECK(size == RoundDown(size, kAlignment));
  DCHECK(size > limit_ - position_);

  // Compute the new segment size. We use a 'high water mark'
  // strategy, where we increase the segment size every time we expand
  // except that we employ a maximum segment size when we delete. This
  // is to avoid excessive malloc() and free() overhead.
  Segment* head = segment_head_;
  const size_t old_size = (head == nullptr) ? 0 : head->size();
  static const size_t kSegmentOverhead = sizeof(Segment) + kAlignment;
  const size_t new_size_no_overhead = size + (old_size << 1);
  size_t new_size = kSegmentOverhead + new_size_no_overhead;
  const size_t min_new_size = kSegmentOverhead + static_cast<size_t>(size);
  // Guard against integer overflow.
  if (new_size_no_overhead < static_cast<size_t>(size) ||
      new_size < static_cast<size_t>(kSegmentOverhead)) {
    FatalProcessOutOfMemory("Zone");
    return nullptr;
  }
  if (new_size < static_cast<size_t>(kMinimumSegmentSize)) {
    new_size = kMinimumSegmentSize;
  } else if (new_size > static_cast<size_t>(kMaximumSegmentSize)) {
    // Limit the size of new segments to avoid growing the segment size
    // exponentially, thus putting pressure on contiguous virtual address space.
    // All the while making sure to allocate a segment large enough to hold the
    // requested size.
    new_size = Max(min_new_size, static_cast<size_t>(kMaximumSegmentSize));
  }
  if (new_size > INT_MAX) {
    FatalProcessOutOfMemory("Zone");
    return nullptr;
  }
  Segment* segment = NewSegment(static_cast<int>(new_size));
  if (segment == nullptr) {
    FatalProcessOutOfMemory("Zone");
    return nullptr;
  }

  // Recompute 'top' and 'limit' based on the new segment.
  Address result = RoundUp(segment->start(), kAlignment);
  position_ = result + size;
  // Check for address overflow.
  // (Should not happen since the segment is guaranteed to accomodate
  // size bytes + header and alignment padding)
  DCHECK_GE(reinterpret_cast<uintptr_t>(position_),
            reinterpret_cast<uintptr_t>(result));
  limit_ = segment->end();
  DCHECK(position_ <= limit_);
  return result;
}

}  // namespace internal
}  // namespace v8
