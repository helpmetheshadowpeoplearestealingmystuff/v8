// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_FOREIGN_H_
#define V8_OBJECTS_FOREIGN_H_

#include "src/objects/heap-object.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// Foreign describes objects pointing from JavaScript to C structures.
class Foreign : public HeapObjectPtr {
 public:
  // [address]: field containing the address.
  inline Address foreign_address();

  static inline bool IsNormalized(Object* object);

  DECL_CAST2(Foreign)

  // Dispatched behavior.
  DECL_PRINTER(Foreign)
  DECL_VERIFIER(Foreign)

  // Layout description.

  static const int kForeignAddressOffset = HeapObject::kHeaderSize;
  static const int kSize = kForeignAddressOffset + kPointerSize;

  STATIC_ASSERT(kForeignAddressOffset == Internals::kForeignAddressOffset);

  class BodyDescriptor;

 private:
  friend class Factory;
  friend class SerializerDeserializer;
  friend class StartupSerializer;

  inline void set_foreign_address(Address value);

  OBJECT_CONSTRUCTORS(Foreign, HeapObjectPtr);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_FOREIGN_H_
