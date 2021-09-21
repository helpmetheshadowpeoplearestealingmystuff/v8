// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/utils/allocation.h"

#include <stdlib.h>  // For free, malloc.

#include "src/base/bits.h"
#include "src/base/bounded-page-allocator.h"
#include "src/base/lazy-instance.h"
#include "src/base/logging.h"
#include "src/base/page-allocator.h"
#include "src/base/platform/platform.h"
#include "src/base/platform/wrappers.h"
#include "src/base/sanitizer/lsan-page-allocator.h"
#include "src/base/vector.h"
#include "src/flags/flags.h"
#include "src/init/v8.h"
#include "src/init/vm-cage.h"
#include "src/utils/memcopy.h"

#if V8_LIBC_BIONIC
#include <malloc.h>
#endif

namespace v8 {
namespace internal {

namespace {

void* AlignedAllocInternal(size_t size, size_t alignment) {
  void* ptr;
#if V8_OS_WIN
  ptr = _aligned_malloc(size, alignment);
#elif V8_LIBC_BIONIC
  // posix_memalign is not exposed in some Android versions, so we fall back to
  // memalign. See http://code.google.com/p/android/issues/detail?id=35391.
  ptr = memalign(alignment, size);
#elif V8_OS_STARBOARD
  ptr = SbMemoryAllocateAligned(alignment, size);
#else
  if (posix_memalign(&ptr, alignment, size)) ptr = nullptr;
#endif
  return ptr;
}

class PageAllocatorInitializer {
 public:
  PageAllocatorInitializer() {
    page_allocator_ = V8::GetCurrentPlatform()->GetPageAllocator();
    if (page_allocator_ == nullptr) {
      static base::LeakyObject<base::PageAllocator> default_page_allocator;
      page_allocator_ = default_page_allocator.get();
    }
#if defined(LEAK_SANITIZER)
    static_assert(!V8_VIRTUAL_MEMORY_CAGE_BOOL, "Not currently supported");
    static base::LeakyObject<base::LsanPageAllocator> lsan_allocator(
        page_allocator_);
    page_allocator_ = lsan_allocator.get();
#endif
  }

  PageAllocator* page_allocator() const { return page_allocator_; }

#ifdef V8_VIRTUAL_MEMORY_CAGE
  PageAllocator* data_cage_page_allocator() const {
    return data_cage_page_allocator_;
  }
#endif

  void SetPageAllocatorForTesting(PageAllocator* allocator) {
    page_allocator_ = allocator;
  }

 private:
  PageAllocator* page_allocator_;
#ifdef V8_VIRTUAL_MEMORY_CAGE
  PageAllocator* data_cage_page_allocator_;
#endif
};

DEFINE_LAZY_LEAKY_OBJECT_GETTER(PageAllocatorInitializer,
                                GetPageAllocatorInitializer)

// We will attempt allocation this many times. After each failure, we call
// OnCriticalMemoryPressure to try to free some memory.
const int kAllocationTries = 2;

}  // namespace

v8::PageAllocator* GetPlatformPageAllocator() {
  DCHECK_NOT_NULL(GetPageAllocatorInitializer()->page_allocator());
  return GetPageAllocatorInitializer()->page_allocator();
}

#ifdef V8_VIRTUAL_MEMORY_CAGE
v8::PageAllocator* GetVirtualMemoryCagePageAllocator() {
  // TODO(chromium:1218005) remove this code once the cage is no longer
  // optional.
  if (GetProcessWideVirtualMemoryCage()->is_disabled()) {
    return GetPlatformPageAllocator();
  } else {
    CHECK(GetProcessWideVirtualMemoryCage()->is_initialized());
    return GetProcessWideVirtualMemoryCage()->page_allocator();
  }
}
#endif

v8::PageAllocator* SetPlatformPageAllocatorForTesting(
    v8::PageAllocator* new_page_allocator) {
  v8::PageAllocator* old_page_allocator = GetPlatformPageAllocator();
  GetPageAllocatorInitializer()->SetPageAllocatorForTesting(new_page_allocator);
  return old_page_allocator;
}

void* Malloced::operator new(size_t size) {
  void* result = AllocWithRetry(size);
  if (result == nullptr) {
    V8::FatalProcessOutOfMemory(nullptr, "Malloced operator new");
  }
  return result;
}

void Malloced::operator delete(void* p) { base::Free(p); }

char* StrDup(const char* str) {
  size_t length = strlen(str);
  char* result = NewArray<char>(length + 1);
  MemCopy(result, str, length);
  result[length] = '\0';
  return result;
}

char* StrNDup(const char* str, size_t n) {
  size_t length = strlen(str);
  if (n < length) length = n;
  char* result = NewArray<char>(length + 1);
  MemCopy(result, str, length);
  result[length] = '\0';
  return result;
}

void* AllocWithRetry(size_t size, MallocFn malloc_fn) {
  void* result = nullptr;
  for (int i = 0; i < kAllocationTries; ++i) {
    result = malloc_fn(size);
    if (result != nullptr) break;
    if (!OnCriticalMemoryPressure(size)) break;
  }
  return result;
}

void* AlignedAlloc(size_t size, size_t alignment) {
  DCHECK_LE(alignof(void*), alignment);
  DCHECK(base::bits::IsPowerOfTwo(alignment));
  void* result = nullptr;
  for (int i = 0; i < kAllocationTries; ++i) {
    result = AlignedAllocInternal(size, alignment);
    if (result != nullptr) break;
    if (!OnCriticalMemoryPressure(size + alignment)) break;
  }
  if (result == nullptr) {
    V8::FatalProcessOutOfMemory(nullptr, "AlignedAlloc");
  }
  return result;
}

void AlignedFree(void* ptr) {
#if V8_OS_WIN
  _aligned_free(ptr);
#elif V8_LIBC_BIONIC
  // Using free is not correct in general, but for V8_LIBC_BIONIC it is.
  base::Free(ptr);
#elif V8_OS_STARBOARD
  SbMemoryFreeAligned(ptr);
#else
  base::Free(ptr);
#endif
}

size_t AllocatePageSize() {
  return GetPlatformPageAllocator()->AllocatePageSize();
}

size_t CommitPageSize() { return GetPlatformPageAllocator()->CommitPageSize(); }

void SetRandomMmapSeed(int64_t seed) {
  GetPlatformPageAllocator()->SetRandomMmapSeed(seed);
}

void* GetRandomMmapAddr() {
  return GetPlatformPageAllocator()->GetRandomMmapAddr();
}

void* AllocatePages(v8::PageAllocator* page_allocator, void* hint, size_t size,
                    size_t alignment, PageAllocator::Permission access) {
  DCHECK_NOT_NULL(page_allocator);
  DCHECK_EQ(hint, AlignedAddress(hint, alignment));
  DCHECK(IsAligned(size, page_allocator->AllocatePageSize()));
  if (FLAG_randomize_all_allocations) {
    hint = page_allocator->GetRandomMmapAddr();
  }
  void* result = nullptr;
  for (int i = 0; i < kAllocationTries; ++i) {
    result = page_allocator->AllocatePages(hint, size, alignment, access);
    if (result != nullptr) break;
    size_t request_size = size + alignment - page_allocator->AllocatePageSize();
    if (!OnCriticalMemoryPressure(request_size)) break;
  }
  return result;
}

bool FreePages(v8::PageAllocator* page_allocator, void* address,
               const size_t size) {
  DCHECK_NOT_NULL(page_allocator);
  DCHECK(IsAligned(size, page_allocator->AllocatePageSize()));
  return page_allocator->FreePages(address, size);
}

bool ReleasePages(v8::PageAllocator* page_allocator, void* address, size_t size,
                  size_t new_size) {
  DCHECK_NOT_NULL(page_allocator);
  DCHECK_LT(new_size, size);
  DCHECK(IsAligned(new_size, page_allocator->CommitPageSize()));
  return page_allocator->ReleasePages(address, size, new_size);
}

bool SetPermissions(v8::PageAllocator* page_allocator, void* address,
                    size_t size, PageAllocator::Permission access) {
  DCHECK_NOT_NULL(page_allocator);
  return page_allocator->SetPermissions(address, size, access);
}

bool OnCriticalMemoryPressure(size_t length) {
  // TODO(bbudge) Rework retry logic once embedders implement the more
  // informative overload.
  if (!V8::GetCurrentPlatform()->OnCriticalMemoryPressure(length)) {
    V8::GetCurrentPlatform()->OnCriticalMemoryPressure();
  }
  return true;
}

VirtualMemory::VirtualMemory() = default;

VirtualMemory::VirtualMemory(v8::PageAllocator* page_allocator, size_t size,
                             void* hint, size_t alignment, JitPermission jit)
    : page_allocator_(page_allocator) {
  DCHECK_NOT_NULL(page_allocator);
  DCHECK(IsAligned(size, page_allocator_->CommitPageSize()));
  size_t page_size = page_allocator_->AllocatePageSize();
  alignment = RoundUp(alignment, page_size);
  PageAllocator::Permission permissions =
      jit == kMapAsJittable ? PageAllocator::kNoAccessWillJitLater
                            : PageAllocator::kNoAccess;
  Address address = reinterpret_cast<Address>(AllocatePages(
      page_allocator_, hint, RoundUp(size, page_size), alignment, permissions));
  if (address != kNullAddress) {
    DCHECK(IsAligned(address, alignment));
    region_ = base::AddressRegion(address, size);
  }
}

VirtualMemory::~VirtualMemory() {
  if (IsReserved()) {
    Free();
  }
}

void VirtualMemory::Reset() {
  page_allocator_ = nullptr;
  region_ = base::AddressRegion();
}

bool VirtualMemory::SetPermissions(Address address, size_t size,
                                   PageAllocator::Permission access) {
  CHECK(InVM(address, size));
  bool result =
      v8::internal::SetPermissions(page_allocator_, address, size, access);
  DCHECK(result);
  return result;
}

size_t VirtualMemory::Release(Address free_start) {
  DCHECK(IsReserved());
  DCHECK(IsAligned(free_start, page_allocator_->CommitPageSize()));
  // Notice: Order is important here. The VirtualMemory object might live
  // inside the allocated region.

  const size_t old_size = region_.size();
  const size_t free_size = old_size - (free_start - region_.begin());
  CHECK(InVM(free_start, free_size));
  region_.set_size(old_size - free_size);
  CHECK(ReleasePages(page_allocator_, reinterpret_cast<void*>(region_.begin()),
                     old_size, region_.size()));
  return free_size;
}

void VirtualMemory::Free() {
  DCHECK(IsReserved());
  // Notice: Order is important here. The VirtualMemory object might live
  // inside the allocated region.
  v8::PageAllocator* page_allocator = page_allocator_;
  base::AddressRegion region = region_;
  Reset();
  // FreePages expects size to be aligned to allocation granularity however
  // ReleasePages may leave size at only commit granularity. Align it here.
  CHECK(FreePages(page_allocator, reinterpret_cast<void*>(region.begin()),
                  RoundUp(region.size(), page_allocator->AllocatePageSize())));
}

void VirtualMemory::FreeReadOnly() {
  DCHECK(IsReserved());
  // The only difference to Free is that it doesn't call Reset which would write
  // to the VirtualMemory object.
  v8::PageAllocator* page_allocator = page_allocator_;
  base::AddressRegion region = region_;

  // FreePages expects size to be aligned to allocation granularity however
  // ReleasePages may leave size at only commit granularity. Align it here.
  CHECK(FreePages(page_allocator, reinterpret_cast<void*>(region.begin()),
                  RoundUp(region.size(), page_allocator->AllocatePageSize())));
}

VirtualMemoryCage::VirtualMemoryCage() = default;

VirtualMemoryCage::~VirtualMemoryCage() { Free(); }

VirtualMemoryCage::VirtualMemoryCage(VirtualMemoryCage&& other) V8_NOEXCEPT {
  *this = std::move(other);
}

VirtualMemoryCage& VirtualMemoryCage::operator=(VirtualMemoryCage&& other)
    V8_NOEXCEPT {
  page_allocator_ = std::move(other.page_allocator_);
  reservation_ = std::move(other.reservation_);
  return *this;
}

namespace {
inline Address VirtualMemoryCageStart(
    Address reservation_start,
    const VirtualMemoryCage::ReservationParams& params) {
  return RoundUp(reservation_start + params.base_bias_size,
                 params.base_alignment) -
         params.base_bias_size;
}
}  // namespace

bool VirtualMemoryCage::InitReservation(
    const ReservationParams& params, base::AddressRegion existing_reservation) {
  DCHECK(!reservation_.IsReserved());

  const size_t allocate_page_size = params.page_allocator->AllocatePageSize();
  CHECK(IsAligned(params.reservation_size, allocate_page_size));
  CHECK(params.base_alignment == ReservationParams::kAnyBaseAlignment ||
        (IsAligned(params.base_alignment, allocate_page_size) &&
         IsAligned(params.base_bias_size, allocate_page_size)));
  CHECK_LE(params.base_bias_size, params.reservation_size);

  Address hint = RoundDown(params.requested_start_hint,
                           RoundUp(params.base_alignment, allocate_page_size)) -
                 RoundUp(params.base_bias_size, allocate_page_size);

  if (!existing_reservation.is_empty()) {
    CHECK_EQ(existing_reservation.size(), params.reservation_size);
    CHECK(params.base_alignment == ReservationParams::kAnyBaseAlignment ||
          IsAligned(existing_reservation.begin(), params.base_alignment));
    reservation_ =
        VirtualMemory(params.page_allocator, existing_reservation.begin(),
                      existing_reservation.size());
    base_ = reservation_.address() + params.base_bias_size;
  } else if (params.base_alignment == ReservationParams::kAnyBaseAlignment) {
    // When the base doesn't need to be aligned, the virtual memory reservation
    // fails only due to OOM.
    VirtualMemory reservation(params.page_allocator, params.reservation_size,
                              reinterpret_cast<void*>(hint));
    if (!reservation.IsReserved()) return false;

    reservation_ = std::move(reservation);
    base_ = reservation_.address() + params.base_bias_size;
    CHECK_EQ(reservation_.size(), params.reservation_size);
  } else {
    // Otherwise, we need to try harder by first overreserving
    // in hopes of finding a correctly aligned address within the larger
    // reservation.
    const int kMaxAttempts = 4;
    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
      // Reserve a region of twice the size so that there is an aligned address
      // within it that's usable as the cage base.
      VirtualMemory padded_reservation(params.page_allocator,
                                       params.reservation_size * 2,
                                       reinterpret_cast<void*>(hint));
      if (!padded_reservation.IsReserved()) return false;

      // Find properly aligned sub-region inside the reservation.
      Address address =
          VirtualMemoryCageStart(padded_reservation.address(), params);
      CHECK(padded_reservation.InVM(address, params.reservation_size));

#if defined(V8_OS_FUCHSIA)
      // Fuchsia does not respect given hints so as a workaround we will use
      // overreserved address space region instead of trying to re-reserve
      // a subregion.
      bool overreserve = true;
#else
      // For the last attempt use the overreserved region to avoid an OOM crash.
      // This case can happen if there are many isolates being created in
      // parallel that race for reserving the regions.
      bool overreserve = (attempt == kMaxAttempts - 1);
#endif

      if (overreserve) {
        if (padded_reservation.InVM(address, params.reservation_size)) {
          reservation_ = std::move(padded_reservation);
          base_ = address + params.base_bias_size;
          break;
        }
      } else {
        // Now free the padded reservation and immediately try to reserve an
        // exact region at aligned address. We have to do this dancing because
        // the reservation address requirement is more complex than just a
        // certain alignment and not all operating systems support freeing parts
        // of reserved address space regions.
        padded_reservation.Free();

        VirtualMemory reservation(params.page_allocator,
                                  params.reservation_size,
                                  reinterpret_cast<void*>(address));
        if (!reservation.IsReserved()) return false;

        // The reservation could still be somewhere else but we can accept it
        // if it has the required alignment.
        Address address = VirtualMemoryCageStart(reservation.address(), params);
        if (reservation.address() == address) {
          reservation_ = std::move(reservation);
          base_ = address + params.base_bias_size;
          CHECK_EQ(reservation_.size(), params.reservation_size);
          break;
        }
      }
    }
  }
  CHECK_NE(base_, kNullAddress);
  CHECK(IsAligned(base_, params.base_alignment));

  const Address allocatable_base = RoundUp(base_, params.page_size);
  const size_t allocatable_size =
      RoundDown(params.reservation_size - (allocatable_base - base_) -
                    params.base_bias_size,
                params.page_size);
  page_allocator_ = std::make_unique<base::BoundedPageAllocator>(
      params.page_allocator, allocatable_base, allocatable_size,
      params.page_size);
  return true;
}

void VirtualMemoryCage::Free() {
  if (IsReserved()) {
    base_ = kNullAddress;
    page_allocator_.reset();
    reservation_.Free();
  }
}

}  // namespace internal
}  // namespace v8
