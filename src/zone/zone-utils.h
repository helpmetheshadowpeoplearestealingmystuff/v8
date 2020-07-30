// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ZONE_ZONE_UTILS_H_
#define V8_ZONE_ZONE_UTILS_H_

#include <algorithm>
#include <type_traits>

#include "src/utils/vector.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

template <typename T>
Vector<T> CloneVector(Zone* zone, const Vector<const T>& other) {
  int length = other.length();
  if (length == 0) return Vector<T>();

  T* data = zone->NewArray<T>(length);
  if (std::is_trivially_copyable<T>::value) {
    MemCopy(data, other.data(), length * sizeof(T));
  } else {
    std::copy(other.begin(), other.end(), data);
  }
  return Vector<T>(data, length);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_ZONE_ZONE_UTILS_H_
