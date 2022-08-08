// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_INTERNAL_CAGED_HEAP_H_
#define INCLUDE_CPPGC_INTERNAL_CAGED_HEAP_H_

#include <climits>
#include <cstddef>

#include "cppgc/internal/api-constants.h"
#include "cppgc/internal/base-page-handle.h"
#include "v8config.h"  // NOLINT(build/include_directory)

#if defined(CPPGC_CAGED_HEAP)

namespace cppgc {
namespace internal {

class V8_EXPORT CagedHeapBase {
 public:
  V8_INLINE static uintptr_t OffsetFromAddress(const void* address) {
    return reinterpret_cast<uintptr_t>(address) &
           (api_constants::kCagedHeapReservationAlignment - 1);
  }

  V8_INLINE static bool IsWithinCage(const void* address) {
    CPPGC_DCHECK(g_heap_base_);
    return (reinterpret_cast<uintptr_t>(address) &
            ~(api_constants::kCagedHeapReservationAlignment - 1)) ==
           g_heap_base_;
  }

  V8_INLINE static bool AreWithinCage(const void* addr1, const void* addr2) {
    static constexpr size_t kHalfWordShift = sizeof(uint32_t) * CHAR_BIT - 1;
    static_assert((static_cast<size_t>(1) << kHalfWordShift) ==
                  api_constants::kCagedHeapReservationSize);
    CPPGC_DCHECK(g_heap_base_);
    return !(((reinterpret_cast<uintptr_t>(addr1) ^ g_heap_base_) |
              (reinterpret_cast<uintptr_t>(addr2) ^ g_heap_base_)) >>
             kHalfWordShift);
  }

  V8_INLINE static bool IsWithinNormalPageReservation(void* address) {
    return (reinterpret_cast<uintptr_t>(address) - g_heap_base_) <
           api_constants::kCagedHeapNormalPageReservationSize;
  }

  V8_INLINE static bool IsWithinLargePageReservation(const void* ptr) {
    CPPGC_DCHECK(g_heap_base_);
    auto uptr = reinterpret_cast<uintptr_t>(ptr);
    return (uptr >= g_heap_base_ +
                        api_constants::kCagedHeapNormalPageReservationSize) &&
           (uptr < g_heap_base_ + api_constants::kCagedHeapReservationSize);
  }

  V8_INLINE static uintptr_t GetBase() { return g_heap_base_; }

  V8_INLINE static BasePageHandle& LookupPageFromInnerPointer(void* ptr) {
    if (V8_LIKELY(IsWithinNormalPageReservation(ptr)))
      return *BasePageHandle::FromPayload(ptr);
    else
      return LookupLargePageFromInnerPointer(ptr);
  }

 private:
  friend class CagedHeap;

  static BasePageHandle& LookupLargePageFromInnerPointer(void* address);

  static uintptr_t g_heap_base_;
};

}  // namespace internal
}  // namespace cppgc

#endif  // defined(CPPGC_CAGED_HEAP)

#endif  // INCLUDE_CPPGC_INTERNAL_CAGED_HEAP_H_
