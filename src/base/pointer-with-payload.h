// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_POINTER_WITH_PAYLOAD_H_
#define V8_BASE_POINTER_WITH_PAYLOAD_H_

#include <cstdint>
#include <type_traits>

#include "src/base/logging.h"

namespace v8 {
namespace base {

template <typename PointerType>
struct PointerWithPayloadTraits {
  static constexpr int kAvailableBits =
      alignof(PointerType) >= 8 ? 3 : alignof(PointerType) >= 4 ? 2 : 1;
};

// Assume void* has the same payloads as void**, under the assumption that it's
// used for classes that contain at least one pointer.
template <>
struct PointerWithPayloadTraits<void> : public PointerWithPayloadTraits<void*> {
};

// PointerWithPayload combines a PointerType* an a small PayloadType into
// one. The bits of the storage type get packed into the lower bits of the
// pointer that are free due to alignment. The user needs to specify how many
// bits are needed to store the PayloadType, allowing Types that by default are
// larger to be stored.
//
// Example:
//   PointerWithPayload<int *, bool, 1> data_and_flag;
//
//   Here we store a bool that needs 1 bit of storage state into the lower bits
//   of int *, which points to some int data;
template <typename PointerType, typename PayloadType, int NumPayloadBits>
class PointerWithPayload {
 public:
  PointerWithPayload() = default;

  explicit PointerWithPayload(PointerType* pointer)
      : pointer_with_payload_(reinterpret_cast<uintptr_t>(pointer)) {
    DCHECK_EQ(reinterpret_cast<uintptr_t>(pointer) & kPayloadMask, 0);
    DCHECK_EQ(GetPointer(), pointer);
    DCHECK_EQ(GetPayload(), static_cast<PayloadType>(0));
  }

  explicit PointerWithPayload(PayloadType payload)
      : pointer_with_payload_(static_cast<uintptr_t>(payload)) {
    DCHECK_EQ(GetPointer(), nullptr);
    DCHECK_EQ(GetPayload(), payload);
  }

  PointerWithPayload(PointerType* pointer, PayloadType payload) {
    Update(pointer, payload);
  }

  PointerWithPayload(const PointerWithPayload& other) V8_NOEXCEPT = default;

  PointerWithPayload& operator=(const PointerWithPayload& other)
      V8_NOEXCEPT = default;

  V8_INLINE PointerType* GetPointer() const {
    return reinterpret_cast<PointerType*>(pointer_with_payload_ & kPointerMask);
  }

  // An optimized version of GetPointer for when we know the payload value.
  V8_INLINE PointerType* GetPointerWithKnownPayload(PayloadType payload) const {
    DCHECK_EQ(GetPayload(), payload);
    return reinterpret_cast<PointerType*>(pointer_with_payload_ -
                                          static_cast<uintptr_t>(payload));
  }

  V8_INLINE PointerType* operator->() const { return GetPointer(); }

  V8_INLINE void Update(PointerType* new_pointer, PayloadType new_payload) {
    DCHECK_EQ(reinterpret_cast<uintptr_t>(new_pointer) & kPayloadMask, 0);
    pointer_with_payload_ = reinterpret_cast<uintptr_t>(new_pointer) |
                            static_cast<uintptr_t>(new_payload);
    DCHECK_EQ(GetPayload(), new_payload);
    DCHECK_EQ(GetPointer(), new_pointer);
  }

  V8_INLINE void SetPointer(PointerType* new_pointer) {
    DCHECK_EQ(reinterpret_cast<uintptr_t>(new_pointer) & kPayloadMask, 0);
    pointer_with_payload_ = reinterpret_cast<uintptr_t>(new_pointer) |
                            (pointer_with_payload_ & kPayloadMask);
    DCHECK_EQ(GetPointer(), new_pointer);
  }

  V8_INLINE PayloadType GetPayload() const {
    return static_cast<PayloadType>(pointer_with_payload_ & kPayloadMask);
  }

  V8_INLINE void SetPayload(PayloadType new_payload) {
    uintptr_t new_payload_ptr = static_cast<uintptr_t>(new_payload);
    DCHECK_EQ(new_payload_ptr & kPayloadMask, new_payload_ptr);
    pointer_with_payload_ =
        (pointer_with_payload_ & kPointerMask) | new_payload_ptr;
    DCHECK_EQ(GetPayload(), new_payload);
  }

 private:
  static constexpr int kAvailableBits = PointerWithPayloadTraits<
      typename std::remove_const<PointerType>::type>::kAvailableBits;
  static_assert(
      kAvailableBits >= NumPayloadBits,
      "Ptr does not have sufficient alignment for the selected amount of "
      "storage bits. Override PointerWithPayloadTraits to guarantee available "
      "bits manually.");

  static constexpr uintptr_t kPayloadMask =
      (uintptr_t{1} << NumPayloadBits) - 1;
  static constexpr uintptr_t kPointerMask = ~kPayloadMask;

  uintptr_t pointer_with_payload_ = 0;

  friend bool operator==(const PointerWithPayload& p1,
                         const PointerWithPayload& p2) {
    return p1.pointer_with_payload_ == p2.pointer_with_payload_;
  }
  friend bool operator!=(const PointerWithPayload& p1,
                         const PointerWithPayload& p2) {
    return !(p1 == p2);
  }
};

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_POINTER_WITH_PAYLOAD_H_
