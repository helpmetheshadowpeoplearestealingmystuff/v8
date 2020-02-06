// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_FACTORY_BASE_INL_H_
#define V8_HEAP_FACTORY_BASE_INL_H_

#include "src/heap/factory-base.h"

#include "src/handles/handle-for.h"
#include "src/numbers/conversions.h"
#include "src/objects/smi.h"
#include "src/roots/roots.h"

namespace v8 {
namespace internal {

template <typename Impl>
HandleFor<Impl, Oddball> FactoryBase<Impl>::ToBoolean(bool value) {
  return value ? impl()->true_value() : impl()->false_value();
}

template <typename Impl>
template <AllocationType allocation>
HandleFor<Impl, Object> FactoryBase<Impl>::NewNumber(double value) {
  // Materialize as a SMI if possible.
  int32_t int_value;
  if (DoubleToSmiInteger(value, &int_value)) {
    return handle(Smi::FromInt(int_value), isolate());
  }
  return NewHeapNumber<allocation>(value);
}

template <typename Impl>
template <AllocationType allocation>
HandleFor<Impl, Object> FactoryBase<Impl>::NewNumberFromInt(int32_t value) {
  if (Smi::IsValid(value)) return handle(Smi::FromInt(value), isolate());
  // Bypass NewNumber to avoid various redundant checks.
  return NewHeapNumber<allocation>(FastI2D(value));
}

template <typename Impl>
template <AllocationType allocation>
HandleFor<Impl, Object> FactoryBase<Impl>::NewNumberFromUint(uint32_t value) {
  int32_t int32v = static_cast<int32_t>(value);
  if (int32v >= 0 && Smi::IsValid(int32v)) {
    return handle(Smi::FromInt(int32v), isolate());
  }
  return NewHeapNumber<allocation>(FastUI2D(value));
}

template <typename Impl>
template <AllocationType allocation>
HandleFor<Impl, Object> FactoryBase<Impl>::NewNumberFromSize(size_t value) {
  // We can't use Smi::IsValid() here because that operates on a signed
  // intptr_t, and casting from size_t could create a bogus sign bit.
  if (value <= static_cast<size_t>(Smi::kMaxValue)) {
    return handle(Smi::FromIntptr(static_cast<intptr_t>(value)), isolate());
  }
  return NewHeapNumber<allocation>(static_cast<double>(value));
}

template <typename Impl>
template <AllocationType allocation>
HandleFor<Impl, Object> FactoryBase<Impl>::NewNumberFromInt64(int64_t value) {
  if (value <= std::numeric_limits<int32_t>::max() &&
      value >= std::numeric_limits<int32_t>::min() &&
      Smi::IsValid(static_cast<int32_t>(value))) {
    return handle(Smi::FromInt(static_cast<int32_t>(value)), isolate());
  }
  return NewHeapNumber<allocation>(static_cast<double>(value));
}

template <typename Impl>
template <AllocationType allocation>
HandleFor<Impl, HeapNumber> FactoryBase<Impl>::NewHeapNumber(double value) {
  HandleFor<Impl, HeapNumber> heap_number = NewHeapNumber<allocation>();
  heap_number->set_value(value);
  return heap_number;
}

template <typename Impl>
template <AllocationType allocation>
HandleFor<Impl, HeapNumber> FactoryBase<Impl>::NewHeapNumberFromBits(
    uint64_t bits) {
  HandleFor<Impl, HeapNumber> heap_number = NewHeapNumber<allocation>();
  heap_number->set_value_as_bits(bits);
  return heap_number;
}

template <typename Impl>
template <AllocationType allocation>
HandleFor<Impl, HeapNumber> FactoryBase<Impl>::NewHeapNumberWithHoleNaN() {
  return NewHeapNumberFromBits<allocation>(kHoleNanInt64);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_FACTORY_BASE_INL_H_
