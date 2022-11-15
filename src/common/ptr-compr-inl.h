// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMMON_PTR_COMPR_INL_H_
#define V8_COMMON_PTR_COMPR_INL_H_

#include "include/v8-internal.h"
#include "src/execution/isolate.h"
#include "src/execution/local-isolate-inl.h"

namespace v8 {
namespace internal {

#ifdef V8_COMPRESS_POINTERS

PtrComprCageBase::PtrComprCageBase(const Isolate* isolate)
    : address_(isolate->cage_base()) {}
PtrComprCageBase::PtrComprCageBase(const LocalIsolate* isolate)
    : address_(isolate->cage_base()) {}

//
// V8HeapCompressionScheme
//

// static
Address V8HeapCompressionScheme::GetPtrComprCageBaseAddress(
    Address on_heap_addr) {
  return RoundDown<kPtrComprCageBaseAlignment>(on_heap_addr);
}

// static
Address V8HeapCompressionScheme::GetPtrComprCageBaseAddress(
    PtrComprCageBase cage_base) {
  Address base = cage_base.address();
  base = reinterpret_cast<Address>(V8_ASSUME_ALIGNED(
      reinterpret_cast<void*>(base), kPtrComprCageBaseAlignment));
  return base;
}

// static
Tagged_t V8HeapCompressionScheme::CompressTagged(Address tagged) {
  return static_cast<Tagged_t>(static_cast<uint32_t>(tagged));
}

// static
Address V8HeapCompressionScheme::DecompressTaggedSigned(Tagged_t raw_value) {
  // For runtime code the upper 32-bits of the Smi value do not matter.
  return static_cast<Address>(raw_value);
}

// static
template <typename TOnHeapAddress>
Address V8HeapCompressionScheme::DecompressTaggedPointer(
    TOnHeapAddress on_heap_addr, Tagged_t raw_value) {
  return GetPtrComprCageBaseAddress(on_heap_addr) +
         static_cast<Address>(raw_value);
}

// static
template <typename TOnHeapAddress>
Address V8HeapCompressionScheme::DecompressTaggedAny(
    TOnHeapAddress on_heap_addr, Tagged_t raw_value) {
  return DecompressTaggedPointer(on_heap_addr, raw_value);
}

// static
template <typename ProcessPointerCallback>
void V8HeapCompressionScheme::ProcessIntermediatePointers(
    PtrComprCageBase cage_base, Address raw_value,
    ProcessPointerCallback callback) {
  // If pointer compression is enabled, we may have random compressed pointers
  // on the stack that may be used for subsequent operations.
  // Extract, decompress and trace both halfwords.
  Address decompressed_low = V8HeapCompressionScheme::DecompressTaggedPointer(
      cage_base, static_cast<Tagged_t>(raw_value));
  callback(decompressed_low);
  Address decompressed_high = V8HeapCompressionScheme::DecompressTaggedPointer(
      cage_base,
      static_cast<Tagged_t>(raw_value >> (sizeof(Tagged_t) * CHAR_BIT)));
  callback(decompressed_high);
}

#ifdef V8_EXTERNAL_CODE_SPACE

//
// ExternalCodeCompressionScheme
//

// static
Address ExternalCodeCompressionScheme::PrepareCageBaseAddress(
    Address on_heap_addr) {
  return RoundDown<kMinExpectedOSPageSize>(on_heap_addr);
}

// static
Address ExternalCodeCompressionScheme::GetPtrComprCageBaseAddress(
    PtrComprCageBase cage_base) {
  Address base = cage_base.address();
  base = reinterpret_cast<Address>(
      V8_ASSUME_ALIGNED(reinterpret_cast<void*>(base), kMinExpectedOSPageSize));
  return base;
}

// static
Tagged_t ExternalCodeCompressionScheme::CompressTagged(Address tagged) {
  return static_cast<Tagged_t>(static_cast<uint32_t>(tagged));
}

// static
Address ExternalCodeCompressionScheme::DecompressTaggedSigned(
    Tagged_t raw_value) {
  // For runtime code the upper 32-bits of the Smi value do not matter.
  return static_cast<Address>(raw_value);
}

// static
template <typename TOnHeapAddress>
Address ExternalCodeCompressionScheme::DecompressTaggedPointer(
    TOnHeapAddress on_heap_addr, Tagged_t raw_value) {
  Address cage_base = GetPtrComprCageBaseAddress(on_heap_addr);
  Address diff = static_cast<Address>(static_cast<uint32_t>(raw_value)) -
                 static_cast<Address>(static_cast<uint32_t>(cage_base));
  // The cage base value was chosen such that it's less or equal than any
  // pointer in the cage, thus if we got a negative diff then it means that
  // the decompressed value is off by 4GB.
  if (static_cast<intptr_t>(diff) < 0) {
    diff += size_t{4} * GB;
  }
  DCHECK(is_uint32(diff));
  return cage_base + diff;
}

// static
template <typename TOnHeapAddress>
Address ExternalCodeCompressionScheme::DecompressTaggedAny(
    TOnHeapAddress on_heap_addr, Tagged_t raw_value) {
  if (HAS_SMI_TAG(raw_value)) return DecompressTaggedSigned(raw_value);
  return DecompressTaggedPointer(on_heap_addr, raw_value);
}

#endif  // V8_EXTERNAL_CODE_SPACE

//
// Misc functions.
//

V8_INLINE PtrComprCageBase
GetPtrComprCageBaseFromOnHeapAddress(Address address) {
  return PtrComprCageBase(
      V8HeapCompressionScheme::GetPtrComprCageBaseAddress(address));
}

#else

//
// V8HeapCompressionScheme
//

// static
Address V8HeapCompressionScheme::GetPtrComprCageBaseAddress(
    Address on_heap_addr) {
  UNREACHABLE();
}

// static
Tagged_t V8HeapCompressionScheme::CompressTagged(Address tagged) {
  UNREACHABLE();
}

// static
Address V8HeapCompressionScheme::DecompressTaggedSigned(Tagged_t raw_value) {
  UNREACHABLE();
}

template <typename TOnHeapAddress>
Address V8HeapCompressionScheme::DecompressTaggedPointer(
    TOnHeapAddress on_heap_addr, Tagged_t raw_value) {
  UNREACHABLE();
}

// static
template <typename TOnHeapAddress>
Address V8HeapCompressionScheme::DecompressTaggedAny(
    TOnHeapAddress on_heap_addr, Tagged_t raw_value) {
  UNREACHABLE();
}

// static
template <typename ProcessPointerCallback>
void V8HeapCompressionScheme::ProcessIntermediatePointers(
    PtrComprCageBase cage_base, Address raw_value,
    ProcessPointerCallback callback) {
  UNREACHABLE();
}

//
// Misc functions.
//

V8_INLINE constexpr PtrComprCageBase GetPtrComprCageBaseFromOnHeapAddress(
    Address address) {
  return PtrComprCageBase();
}

#endif  // V8_COMPRESS_POINTERS

V8_INLINE PtrComprCageBase GetPtrComprCageBase(HeapObject object) {
  return GetPtrComprCageBaseFromOnHeapAddress(object.ptr());
}

}  // namespace internal
}  // namespace v8

#endif  // V8_COMMON_PTR_COMPR_INL_H_
