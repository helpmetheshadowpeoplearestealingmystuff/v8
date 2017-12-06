// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DATA_HANDLER_H_
#define V8_DATA_HANDLER_H_

#include "src/objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// DataHandler is a base class for load and store handlers that can't be
// encoded in one Smi. Kind of a handler can be deduced from instance type.
class DataHandler : public Struct {
 public:
  // [smi_handler]: A Smi which encodes a handler or Code object (we still
  // use code handlers for accessing lexical environment variables, but soon
  // only smi handlers will remain). See LoadHandler and StoreHandler for
  // details about encoding.
  DECL_ACCESSORS(smi_handler, Object)

  // [validity_cell]: A validity Cell that guards prototype chain modifications.
  DECL_ACCESSORS(validity_cell, Object)

  // [data1,2]: These are optional general-purpose fields whose content and
  // presence depends on the handler kind.
  DECL_ACCESSORS(data1, Object)
  DECL_ACCESSORS(data2, Object)

// Layout description.
#define DATA_HANDLER_FIELDS(V)         \
  V(kSmiHandlerOffset, kPointerSize)   \
  V(kValidityCellOffset, kPointerSize) \
  V(kData1Offset, kPointerSize)        \
  V(kSizeWithData1, 0)                 \
  V(kData2Offset, kPointerSize)        \
  V(kSizeWithData2, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(HeapObject::kHeaderSize, DATA_HANDLER_FIELDS)
#undef DATA_HANDLER_FIELDS

  DECL_CAST(DataHandler)

  DECL_VERIFIER(DataHandler)
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_DATA_HANDLER_H_
