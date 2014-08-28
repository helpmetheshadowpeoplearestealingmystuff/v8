// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_MACHINE_TYPE_H_
#define V8_COMPILER_MACHINE_TYPE_H_

#include "src/globals.h"

namespace v8 {
namespace internal {

class OStream;

namespace compiler {

// Machine-level types and representations.
// TODO(titzer): Use the real type system instead of MachineType.
enum MachineType {
  // Representations.
  kRepBit = 1 << 0,
  kRepWord8 = 1 << 1,
  kRepWord16 = 1 << 2,
  kRepWord32 = 1 << 3,
  kRepWord64 = 1 << 4,
  kRepFloat32 = 1 << 5,
  kRepFloat64 = 1 << 6,
  kRepTagged = 1 << 7,

  // Types.
  kTypeBool = 1 << 8,
  kTypeInt32 = 1 << 9,
  kTypeUint32 = 1 << 10,
  kTypeInt64 = 1 << 11,
  kTypeUint64 = 1 << 12,
  kTypeNumber = 1 << 13,
  kTypeAny = 1 << 14
};

OStream& operator<<(OStream& os, const MachineType& type);

typedef uint16_t MachineTypeUnion;

// Globally useful machine types and constants.
const MachineTypeUnion kRepMask = kRepBit | kRepWord8 | kRepWord16 |
                                  kRepWord32 | kRepWord64 | kRepFloat32 |
                                  kRepFloat64 | kRepTagged;
const MachineTypeUnion kTypeMask = kTypeBool | kTypeInt32 | kTypeUint32 |
                                   kTypeInt64 | kTypeUint64 | kTypeNumber |
                                   kTypeAny;

const MachineType kMachNone = static_cast<MachineType>(0);
const MachineType kMachFloat32 =
    static_cast<MachineType>(kRepFloat32 | kTypeNumber);
const MachineType kMachFloat64 =
    static_cast<MachineType>(kRepFloat64 | kTypeNumber);
const MachineType kMachInt8 = static_cast<MachineType>(kRepWord8 | kTypeInt32);
const MachineType kMachUint8 =
    static_cast<MachineType>(kRepWord8 | kTypeUint32);
const MachineType kMachInt16 =
    static_cast<MachineType>(kRepWord16 | kTypeInt32);
const MachineType kMachUint16 =
    static_cast<MachineType>(kRepWord16 | kTypeUint32);
const MachineType kMachInt32 =
    static_cast<MachineType>(kRepWord32 | kTypeInt32);
const MachineType kMachUint32 =
    static_cast<MachineType>(kRepWord32 | kTypeUint32);
const MachineType kMachInt64 =
    static_cast<MachineType>(kRepWord64 | kTypeInt64);
const MachineType kMachUint64 =
    static_cast<MachineType>(kRepWord64 | kTypeUint64);
const MachineType kMachPtr = kPointerSize == 4 ? kRepWord32 : kRepWord64;
const MachineType kMachAnyTagged =
    static_cast<MachineType>(kRepTagged | kTypeAny);

// Gets only the type of the given type.
inline MachineType TypeOf(MachineType machine_type) {
  int result = machine_type & kTypeMask;
  return static_cast<MachineType>(result);
}

// Gets only the representation of the given type.
inline MachineType RepresentationOf(MachineType machine_type) {
  int result = machine_type & kRepMask;
  CHECK(IsPowerOf2(result));
  return static_cast<MachineType>(result);
}

// Gets the element size in bytes of the machine type.
inline int ElementSizeOf(MachineType machine_type) {
  switch (RepresentationOf(machine_type)) {
    case kRepBit:
    case kRepWord8:
      return 1;
    case kRepWord16:
      return 2;
    case kRepWord32:
    case kRepFloat32:
      return 4;
    case kRepWord64:
    case kRepFloat64:
      return 8;
    case kRepTagged:
      return kPointerSize;
    default:
      UNREACHABLE();
      return kPointerSize;
  }
}

// Describes the inputs and outputs of a function or call in terms of machine
// types.
class MachineSignature {
 public:
  MachineSignature(uint8_t return_count, uint16_t param_count,
                   MachineType* reps)
      : return_count_(return_count), param_count_(param_count), reps_(reps) {}

  int GetReturnCount() const { return return_count_; }
  int GetParamCount() const { return param_count_; }

  MachineType GetParameterType(int index) const {
    DCHECK(index >= 0 && index < param_count_);
    return reps_[return_count_ + index];
  }

  MachineType GetReturnType(int index = 0) const {
    DCHECK(index >= 0 && index < return_count_);
    return reps_[index];
  }

 protected:
  uint8_t return_count_;
  uint16_t param_count_;
  MachineType* reps_;
};
}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_MACHINE_TYPE_H_
