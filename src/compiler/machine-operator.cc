// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/machine-operator.h"
#include <type_traits>

#include "src/compiler/opcodes.h"
#include "src/compiler/operator.h"

namespace v8 {
namespace internal {
namespace compiler {

bool operator==(StoreRepresentation lhs, StoreRepresentation rhs) {
  return lhs.representation() == rhs.representation() &&
         lhs.write_barrier_kind() == rhs.write_barrier_kind();
}


bool operator!=(StoreRepresentation lhs, StoreRepresentation rhs) {
  return !(lhs == rhs);
}


size_t hash_value(StoreRepresentation rep) {
  return base::hash_combine(rep.representation(), rep.write_barrier_kind());
}


std::ostream& operator<<(std::ostream& os, StoreRepresentation rep) {
  return os << rep.representation() << ", " << rep.write_barrier_kind();
}

size_t hash_value(MemoryAccessKind kind) { return static_cast<size_t>(kind); }

std::ostream& operator<<(std::ostream& os, MemoryAccessKind kind) {
  switch (kind) {
    case MemoryAccessKind::kNormal:
      return os << "kNormal";
    case MemoryAccessKind::kUnaligned:
      return os << "kUnaligned";
    case MemoryAccessKind::kProtected:
      return os << "kProtected";
  }
  UNREACHABLE();
}

size_t hash_value(LoadTransformation rep) { return static_cast<size_t>(rep); }

std::ostream& operator<<(std::ostream& os, LoadTransformation rep) {
  switch (rep) {
    case LoadTransformation::kS128Load8Splat:
      return os << "kS128Load8Splat";
    case LoadTransformation::kS128Load16Splat:
      return os << "kS128Load16Splat";
    case LoadTransformation::kS128Load32Splat:
      return os << "kS128Load32Splat";
    case LoadTransformation::kS128Load64Splat:
      return os << "kS128Load64Splat";
    case LoadTransformation::kS128Load8x8S:
      return os << "kS128Load8x8S";
    case LoadTransformation::kS128Load8x8U:
      return os << "kS128Load8x8U";
    case LoadTransformation::kS128Load16x4S:
      return os << "kS128Load16x4S";
    case LoadTransformation::kS128Load16x4U:
      return os << "kS128Load16x4U";
    case LoadTransformation::kS128Load32x2S:
      return os << "kS128Load32x2S";
    case LoadTransformation::kS128Load32x2U:
      return os << "kS128Load32x2U";
    case LoadTransformation::kS128Load32Zero:
      return os << "kS128Load32Zero";
    case LoadTransformation::kS128Load64Zero:
      return os << "kS128Load64Zero";
  }
  UNREACHABLE();
}

size_t hash_value(LoadTransformParameters params) {
  return base::hash_combine(params.kind, params.transformation);
}

std::ostream& operator<<(std::ostream& os, LoadTransformParameters params) {
  return os << "(" << params.kind << " " << params.transformation << ")";
}

LoadTransformParameters const& LoadTransformParametersOf(Operator const* op) {
  DCHECK_EQ(IrOpcode::kLoadTransform, op->opcode());
  return OpParameter<LoadTransformParameters>(op);
}

bool operator==(LoadTransformParameters lhs, LoadTransformParameters rhs) {
  return lhs.transformation == rhs.transformation && lhs.kind == rhs.kind;
}

bool operator!=(LoadTransformParameters lhs, LoadTransformParameters rhs) {
  return !(lhs == rhs);
}

size_t hash_value(LoadLaneParameters params) {
  return base::hash_combine(params.kind, params.rep, params.laneidx);
}

std::ostream& operator<<(std::ostream& os, LoadLaneParameters params) {
  return os << "(" << params.kind << " " << params.rep << " " << params.laneidx
            << ")";
}

LoadLaneParameters const& LoadLaneParametersOf(Operator const* op) {
  DCHECK_EQ(IrOpcode::kLoadLane, op->opcode());
  return OpParameter<LoadLaneParameters>(op);
}

bool operator==(LoadLaneParameters lhs, LoadLaneParameters rhs) {
  return lhs.kind == rhs.kind && lhs.rep == rhs.rep &&
         lhs.laneidx == rhs.laneidx;
}

LoadRepresentation LoadRepresentationOf(Operator const* op) {
  DCHECK(IrOpcode::kLoad == op->opcode() ||
         IrOpcode::kProtectedLoad == op->opcode() ||
         IrOpcode::kWord32AtomicLoad == op->opcode() ||
         IrOpcode::kWord64AtomicLoad == op->opcode() ||
         IrOpcode::kWord32AtomicPairLoad == op->opcode() ||
         IrOpcode::kPoisonedLoad == op->opcode() ||
         IrOpcode::kUnalignedLoad == op->opcode());
  return OpParameter<LoadRepresentation>(op);
}

StoreRepresentation const& StoreRepresentationOf(Operator const* op) {
  DCHECK(IrOpcode::kStore == op->opcode() ||
         IrOpcode::kProtectedStore == op->opcode());
  return OpParameter<StoreRepresentation>(op);
}

UnalignedStoreRepresentation const& UnalignedStoreRepresentationOf(
    Operator const* op) {
  DCHECK_EQ(IrOpcode::kUnalignedStore, op->opcode());
  return OpParameter<UnalignedStoreRepresentation>(op);
}

size_t hash_value(StoreLaneParameters params) {
  return base::hash_combine(params.kind, params.rep, params.laneidx);
}

std::ostream& operator<<(std::ostream& os, StoreLaneParameters params) {
  return os << "(" << params.kind << " " << params.rep << " " << params.laneidx
            << ")";
}

StoreLaneParameters const& StoreLaneParametersOf(Operator const* op) {
  DCHECK_EQ(IrOpcode::kStoreLane, op->opcode());
  return OpParameter<StoreLaneParameters>(op);
}

bool operator==(StoreLaneParameters lhs, StoreLaneParameters rhs) {
  return lhs.kind == rhs.kind && lhs.rep == rhs.rep &&
         lhs.laneidx == rhs.laneidx;
}

bool operator==(StackSlotRepresentation lhs, StackSlotRepresentation rhs) {
  return lhs.size() == rhs.size() && lhs.alignment() == rhs.alignment();
}

bool operator!=(StackSlotRepresentation lhs, StackSlotRepresentation rhs) {
  return !(lhs == rhs);
}

size_t hash_value(StackSlotRepresentation rep) {
  return base::hash_combine(rep.size(), rep.alignment());
}

std::ostream& operator<<(std::ostream& os, StackSlotRepresentation rep) {
  return os << rep.size() << ", " << rep.alignment();
}

StackSlotRepresentation const& StackSlotRepresentationOf(Operator const* op) {
  DCHECK_EQ(IrOpcode::kStackSlot, op->opcode());
  return OpParameter<StackSlotRepresentation>(op);
}

MachineRepresentation AtomicStoreRepresentationOf(Operator const* op) {
  DCHECK(IrOpcode::kWord32AtomicStore == op->opcode() ||
         IrOpcode::kWord64AtomicStore == op->opcode());
  return OpParameter<MachineRepresentation>(op);
}

MachineType AtomicOpType(Operator const* op) {
  return OpParameter<MachineType>(op);
}

size_t hash_value(ShiftKind kind) { return static_cast<size_t>(kind); }
V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os, ShiftKind kind) {
  switch (kind) {
    case ShiftKind::kNormal:
      return os << "Normal";
    case ShiftKind::kShiftOutZeros:
      return os << "ShiftOutZeros";
  }
}

ShiftKind ShiftKindOf(Operator const* op) {
  DCHECK(IrOpcode::kWord32Sar == op->opcode() ||
         IrOpcode::kWord64Sar == op->opcode());
  return OpParameter<ShiftKind>(op);
}

// The format is:
// V(Name, properties, value_input_count, control_input_count, output_count)
#define PURE_BINARY_OP_LIST_32(V)                                           \
  V(Word32And, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)    \
  V(Word32Or, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)     \
  V(Word32Xor, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)    \
  V(Word32Shl, Operator::kNoProperties, 2, 0, 1)                            \
  V(Word32Shr, Operator::kNoProperties, 2, 0, 1)                            \
  V(Word32Ror, Operator::kNoProperties, 2, 0, 1)                            \
  V(Word32Equal, Operator::kCommutative, 2, 0, 1)                           \
  V(Int32Add, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)     \
  V(Int32Sub, Operator::kNoProperties, 2, 0, 1)                             \
  V(Int32Mul, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)     \
  V(Int32MulHigh, Operator::kAssociative | Operator::kCommutative, 2, 0, 1) \
  V(Int32Div, Operator::kNoProperties, 2, 1, 1)                             \
  V(Int32Mod, Operator::kNoProperties, 2, 1, 1)                             \
  V(Int32LessThan, Operator::kNoProperties, 2, 0, 1)                        \
  V(Int32LessThanOrEqual, Operator::kNoProperties, 2, 0, 1)                 \
  V(Uint32Div, Operator::kNoProperties, 2, 1, 1)                            \
  V(Uint32LessThan, Operator::kNoProperties, 2, 0, 1)                       \
  V(Uint32LessThanOrEqual, Operator::kNoProperties, 2, 0, 1)                \
  V(Uint32Mod, Operator::kNoProperties, 2, 1, 1)                            \
  V(Uint32MulHigh, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)

// The format is:
// V(Name, properties, value_input_count, control_input_count, output_count)
#define PURE_BINARY_OP_LIST_64(V)                                        \
  V(Word64And, Operator::kAssociative | Operator::kCommutative, 2, 0, 1) \
  V(Word64Or, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)  \
  V(Word64Xor, Operator::kAssociative | Operator::kCommutative, 2, 0, 1) \
  V(Word64Shl, Operator::kNoProperties, 2, 0, 1)                         \
  V(Word64Shr, Operator::kNoProperties, 2, 0, 1)                         \
  V(Word64Ror, Operator::kNoProperties, 2, 0, 1)                         \
  V(Word64Equal, Operator::kCommutative, 2, 0, 1)                        \
  V(Int64Add, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)  \
  V(Int64Sub, Operator::kNoProperties, 2, 0, 1)                          \
  V(Int64Mul, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)  \
  V(Int64Div, Operator::kNoProperties, 2, 1, 1)                          \
  V(Int64Mod, Operator::kNoProperties, 2, 1, 1)                          \
  V(Int64LessThan, Operator::kNoProperties, 2, 0, 1)                     \
  V(Int64LessThanOrEqual, Operator::kNoProperties, 2, 0, 1)              \
  V(Uint64Div, Operator::kNoProperties, 2, 1, 1)                         \
  V(Uint64Mod, Operator::kNoProperties, 2, 1, 1)                         \
  V(Uint64LessThan, Operator::kNoProperties, 2, 0, 1)                    \
  V(Uint64LessThanOrEqual, Operator::kNoProperties, 2, 0, 1)

// The format is:
// V(Name, properties, value_input_count, control_input_count, output_count)
#define MACHINE_PURE_OP_LIST(V)                                            \
  PURE_BINARY_OP_LIST_32(V)                                                \
  PURE_BINARY_OP_LIST_64(V)                                                \
  V(Word32Clz, Operator::kNoProperties, 1, 0, 1)                           \
  V(Word64Clz, Operator::kNoProperties, 1, 0, 1)                           \
  V(Word32ReverseBytes, Operator::kNoProperties, 1, 0, 1)                  \
  V(Word64ReverseBytes, Operator::kNoProperties, 1, 0, 1)                  \
  V(Simd128ReverseBytes, Operator::kNoProperties, 1, 0, 1)                 \
  V(BitcastTaggedToWordForTagAndSmiBits, Operator::kNoProperties, 1, 0, 1) \
  V(BitcastWordToTaggedSigned, Operator::kNoProperties, 1, 0, 1)           \
  V(TruncateFloat64ToWord32, Operator::kNoProperties, 1, 0, 1)             \
  V(ChangeFloat32ToFloat64, Operator::kNoProperties, 1, 0, 1)              \
  V(ChangeFloat64ToInt32, Operator::kNoProperties, 1, 0, 1)                \
  V(ChangeFloat64ToInt64, Operator::kNoProperties, 1, 0, 1)                \
  V(ChangeFloat64ToUint32, Operator::kNoProperties, 1, 0, 1)               \
  V(ChangeFloat64ToUint64, Operator::kNoProperties, 1, 0, 1)               \
  V(TruncateFloat64ToUint32, Operator::kNoProperties, 1, 0, 1)             \
  V(TryTruncateFloat32ToInt64, Operator::kNoProperties, 1, 0, 2)           \
  V(TryTruncateFloat64ToInt64, Operator::kNoProperties, 1, 0, 2)           \
  V(TryTruncateFloat32ToUint64, Operator::kNoProperties, 1, 0, 2)          \
  V(TryTruncateFloat64ToUint64, Operator::kNoProperties, 1, 0, 2)          \
  V(ChangeInt32ToFloat64, Operator::kNoProperties, 1, 0, 1)                \
  V(ChangeInt64ToFloat64, Operator::kNoProperties, 1, 0, 1)                \
  V(Float64SilenceNaN, Operator::kNoProperties, 1, 0, 1)                   \
  V(RoundFloat64ToInt32, Operator::kNoProperties, 1, 0, 1)                 \
  V(RoundInt32ToFloat32, Operator::kNoProperties, 1, 0, 1)                 \
  V(RoundInt64ToFloat32, Operator::kNoProperties, 1, 0, 1)                 \
  V(RoundInt64ToFloat64, Operator::kNoProperties, 1, 0, 1)                 \
  V(RoundUint32ToFloat32, Operator::kNoProperties, 1, 0, 1)                \
  V(RoundUint64ToFloat32, Operator::kNoProperties, 1, 0, 1)                \
  V(RoundUint64ToFloat64, Operator::kNoProperties, 1, 0, 1)                \
  V(BitcastWord32ToWord64, Operator::kNoProperties, 1, 0, 1)               \
  V(ChangeInt32ToInt64, Operator::kNoProperties, 1, 0, 1)                  \
  V(ChangeUint32ToFloat64, Operator::kNoProperties, 1, 0, 1)               \
  V(ChangeUint32ToUint64, Operator::kNoProperties, 1, 0, 1)                \
  V(TruncateFloat64ToFloat32, Operator::kNoProperties, 1, 0, 1)            \
  V(TruncateInt64ToInt32, Operator::kNoProperties, 1, 0, 1)                \
  V(BitcastFloat32ToInt32, Operator::kNoProperties, 1, 0, 1)               \
  V(BitcastFloat64ToInt64, Operator::kNoProperties, 1, 0, 1)               \
  V(BitcastInt32ToFloat32, Operator::kNoProperties, 1, 0, 1)               \
  V(BitcastInt64ToFloat64, Operator::kNoProperties, 1, 0, 1)               \
  V(SignExtendWord8ToInt32, Operator::kNoProperties, 1, 0, 1)              \
  V(SignExtendWord16ToInt32, Operator::kNoProperties, 1, 0, 1)             \
  V(SignExtendWord8ToInt64, Operator::kNoProperties, 1, 0, 1)              \
  V(SignExtendWord16ToInt64, Operator::kNoProperties, 1, 0, 1)             \
  V(SignExtendWord32ToInt64, Operator::kNoProperties, 1, 0, 1)             \
  V(Float32Abs, Operator::kNoProperties, 1, 0, 1)                          \
  V(Float32Add, Operator::kCommutative, 2, 0, 1)                           \
  V(Float32Sub, Operator::kNoProperties, 2, 0, 1)                          \
  V(Float32Mul, Operator::kCommutative, 2, 0, 1)                           \
  V(Float32Div, Operator::kNoProperties, 2, 0, 1)                          \
  V(Float32Neg, Operator::kNoProperties, 1, 0, 1)                          \
  V(Float32Sqrt, Operator::kNoProperties, 1, 0, 1)                         \
  V(Float32Max, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)  \
  V(Float32Min, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)  \
  V(Float64Abs, Operator::kNoProperties, 1, 0, 1)                          \
  V(Float64Acos, Operator::kNoProperties, 1, 0, 1)                         \
  V(Float64Acosh, Operator::kNoProperties, 1, 0, 1)                        \
  V(Float64Asin, Operator::kNoProperties, 1, 0, 1)                         \
  V(Float64Asinh, Operator::kNoProperties, 1, 0, 1)                        \
  V(Float64Atan, Operator::kNoProperties, 1, 0, 1)                         \
  V(Float64Atan2, Operator::kNoProperties, 2, 0, 1)                        \
  V(Float64Atanh, Operator::kNoProperties, 1, 0, 1)                        \
  V(Float64Cbrt, Operator::kNoProperties, 1, 0, 1)                         \
  V(Float64Cos, Operator::kNoProperties, 1, 0, 1)                          \
  V(Float64Cosh, Operator::kNoProperties, 1, 0, 1)                         \
  V(Float64Exp, Operator::kNoProperties, 1, 0, 1)                          \
  V(Float64Expm1, Operator::kNoProperties, 1, 0, 1)                        \
  V(Float64Log, Operator::kNoProperties, 1, 0, 1)                          \
  V(Float64Log1p, Operator::kNoProperties, 1, 0, 1)                        \
  V(Float64Log2, Operator::kNoProperties, 1, 0, 1)                         \
  V(Float64Log10, Operator::kNoProperties, 1, 0, 1)                        \
  V(Float64Max, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)  \
  V(Float64Min, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)  \
  V(Float64Neg, Operator::kNoProperties, 1, 0, 1)                          \
  V(Float64Add, Operator::kCommutative, 2, 0, 1)                           \
  V(Float64Sub, Operator::kNoProperties, 2, 0, 1)                          \
  V(Float64Mul, Operator::kCommutative, 2, 0, 1)                           \
  V(Float64Div, Operator::kNoProperties, 2, 0, 1)                          \
  V(Float64Mod, Operator::kNoProperties, 2, 0, 1)                          \
  V(Float64Pow, Operator::kNoProperties, 2, 0, 1)                          \
  V(Float64Sin, Operator::kNoProperties, 1, 0, 1)                          \
  V(Float64Sinh, Operator::kNoProperties, 1, 0, 1)                         \
  V(Float64Sqrt, Operator::kNoProperties, 1, 0, 1)                         \
  V(Float64Tan, Operator::kNoProperties, 1, 0, 1)                          \
  V(Float64Tanh, Operator::kNoProperties, 1, 0, 1)                         \
  V(Float32Equal, Operator::kCommutative, 2, 0, 1)                         \
  V(Float32LessThan, Operator::kNoProperties, 2, 0, 1)                     \
  V(Float32LessThanOrEqual, Operator::kNoProperties, 2, 0, 1)              \
  V(Float64Equal, Operator::kCommutative, 2, 0, 1)                         \
  V(Float64LessThan, Operator::kNoProperties, 2, 0, 1)                     \
  V(Float64LessThanOrEqual, Operator::kNoProperties, 2, 0, 1)              \
  V(Float64ExtractLowWord32, Operator::kNoProperties, 1, 0, 1)             \
  V(Float64ExtractHighWord32, Operator::kNoProperties, 1, 0, 1)            \
  V(Float64InsertLowWord32, Operator::kNoProperties, 2, 0, 1)              \
  V(Float64InsertHighWord32, Operator::kNoProperties, 2, 0, 1)             \
  V(LoadStackCheckOffset, Operator::kNoProperties, 0, 0, 1)                \
  V(LoadFramePointer, Operator::kNoProperties, 0, 0, 1)                    \
  V(LoadParentFramePointer, Operator::kNoProperties, 0, 0, 1)              \
  V(Int32PairAdd, Operator::kNoProperties, 4, 0, 2)                        \
  V(Int32PairSub, Operator::kNoProperties, 4, 0, 2)                        \
  V(Int32PairMul, Operator::kNoProperties, 4, 0, 2)                        \
  V(Word32PairShl, Operator::kNoProperties, 3, 0, 2)                       \
  V(Word32PairShr, Operator::kNoProperties, 3, 0, 2)                       \
  V(Word32PairSar, Operator::kNoProperties, 3, 0, 2)                       \
  V(F64x2Splat, Operator::kNoProperties, 1, 0, 1)                          \
  V(F64x2Abs, Operator::kNoProperties, 1, 0, 1)                            \
  V(F64x2Neg, Operator::kNoProperties, 1, 0, 1)                            \
  V(F64x2Sqrt, Operator::kNoProperties, 1, 0, 1)                           \
  V(F64x2Add, Operator::kCommutative, 2, 0, 1)                             \
  V(F64x2Sub, Operator::kNoProperties, 2, 0, 1)                            \
  V(F64x2Mul, Operator::kCommutative, 2, 0, 1)                             \
  V(F64x2Div, Operator::kNoProperties, 2, 0, 1)                            \
  V(F64x2Min, Operator::kCommutative, 2, 0, 1)                             \
  V(F64x2Max, Operator::kCommutative, 2, 0, 1)                             \
  V(F64x2Eq, Operator::kCommutative, 2, 0, 1)                              \
  V(F64x2Ne, Operator::kCommutative, 2, 0, 1)                              \
  V(F64x2Lt, Operator::kNoProperties, 2, 0, 1)                             \
  V(F64x2Le, Operator::kNoProperties, 2, 0, 1)                             \
  V(F64x2Qfma, Operator::kNoProperties, 3, 0, 1)                           \
  V(F64x2Qfms, Operator::kNoProperties, 3, 0, 1)                           \
  V(F64x2Pmin, Operator::kNoProperties, 2, 0, 1)                           \
  V(F64x2Pmax, Operator::kNoProperties, 2, 0, 1)                           \
  V(F64x2Ceil, Operator::kNoProperties, 1, 0, 1)                           \
  V(F64x2Floor, Operator::kNoProperties, 1, 0, 1)                          \
  V(F64x2Trunc, Operator::kNoProperties, 1, 0, 1)                          \
  V(F64x2NearestInt, Operator::kNoProperties, 1, 0, 1)                     \
  V(F64x2ConvertLowI32x4S, Operator::kNoProperties, 1, 0, 1)               \
  V(F64x2ConvertLowI32x4U, Operator::kNoProperties, 1, 0, 1)               \
  V(F64x2PromoteLowF32x4, Operator::kNoProperties, 1, 0, 1)                \
  V(F32x4Splat, Operator::kNoProperties, 1, 0, 1)                          \
  V(F32x4SConvertI32x4, Operator::kNoProperties, 1, 0, 1)                  \
  V(F32x4UConvertI32x4, Operator::kNoProperties, 1, 0, 1)                  \
  V(F32x4Abs, Operator::kNoProperties, 1, 0, 1)                            \
  V(F32x4Neg, Operator::kNoProperties, 1, 0, 1)                            \
  V(F32x4Sqrt, Operator::kNoProperties, 1, 0, 1)                           \
  V(F32x4RecipApprox, Operator::kNoProperties, 1, 0, 1)                    \
  V(F32x4RecipSqrtApprox, Operator::kNoProperties, 1, 0, 1)                \
  V(F32x4Add, Operator::kCommutative, 2, 0, 1)                             \
  V(F32x4AddHoriz, Operator::kNoProperties, 2, 0, 1)                       \
  V(F32x4Sub, Operator::kNoProperties, 2, 0, 1)                            \
  V(F32x4Mul, Operator::kCommutative, 2, 0, 1)                             \
  V(F32x4Div, Operator::kNoProperties, 2, 0, 1)                            \
  V(F32x4Min, Operator::kCommutative, 2, 0, 1)                             \
  V(F32x4Max, Operator::kCommutative, 2, 0, 1)                             \
  V(F32x4Eq, Operator::kCommutative, 2, 0, 1)                              \
  V(F32x4Ne, Operator::kCommutative, 2, 0, 1)                              \
  V(F32x4Lt, Operator::kNoProperties, 2, 0, 1)                             \
  V(F32x4Le, Operator::kNoProperties, 2, 0, 1)                             \
  V(F32x4Qfma, Operator::kNoProperties, 3, 0, 1)                           \
  V(F32x4Qfms, Operator::kNoProperties, 3, 0, 1)                           \
  V(F32x4Pmin, Operator::kNoProperties, 2, 0, 1)                           \
  V(F32x4Pmax, Operator::kNoProperties, 2, 0, 1)                           \
  V(F32x4Ceil, Operator::kNoProperties, 1, 0, 1)                           \
  V(F32x4Floor, Operator::kNoProperties, 1, 0, 1)                          \
  V(F32x4Trunc, Operator::kNoProperties, 1, 0, 1)                          \
  V(F32x4NearestInt, Operator::kNoProperties, 1, 0, 1)                     \
  V(F32x4DemoteF64x2Zero, Operator::kNoProperties, 1, 0, 1)                \
  V(I64x2Splat, Operator::kNoProperties, 1, 0, 1)                          \
  V(I64x2SplatI32Pair, Operator::kNoProperties, 2, 0, 1)                   \
  V(I64x2Neg, Operator::kNoProperties, 1, 0, 1)                            \
  V(I64x2SConvertI32x4Low, Operator::kNoProperties, 1, 0, 1)               \
  V(I64x2SConvertI32x4High, Operator::kNoProperties, 1, 0, 1)              \
  V(I64x2UConvertI32x4Low, Operator::kNoProperties, 1, 0, 1)               \
  V(I64x2UConvertI32x4High, Operator::kNoProperties, 1, 0, 1)              \
  V(I64x2BitMask, Operator::kNoProperties, 1, 0, 1)                        \
  V(I64x2Shl, Operator::kNoProperties, 2, 0, 1)                            \
  V(I64x2ShrS, Operator::kNoProperties, 2, 0, 1)                           \
  V(I64x2Add, Operator::kCommutative, 2, 0, 1)                             \
  V(I64x2Sub, Operator::kNoProperties, 2, 0, 1)                            \
  V(I64x2Mul, Operator::kCommutative, 2, 0, 1)                             \
  V(I64x2Eq, Operator::kCommutative, 2, 0, 1)                              \
  V(I64x2ShrU, Operator::kNoProperties, 2, 0, 1)                           \
  V(I64x2ExtMulLowI32x4S, Operator::kCommutative, 2, 0, 1)                 \
  V(I64x2ExtMulHighI32x4S, Operator::kCommutative, 2, 0, 1)                \
  V(I64x2ExtMulLowI32x4U, Operator::kCommutative, 2, 0, 1)                 \
  V(I64x2ExtMulHighI32x4U, Operator::kCommutative, 2, 0, 1)                \
  V(I64x2SignSelect, Operator::kNoProperties, 3, 0, 1)                     \
  V(I32x4Splat, Operator::kNoProperties, 1, 0, 1)                          \
  V(I32x4SConvertF32x4, Operator::kNoProperties, 1, 0, 1)                  \
  V(I32x4SConvertI16x8Low, Operator::kNoProperties, 1, 0, 1)               \
  V(I32x4SConvertI16x8High, Operator::kNoProperties, 1, 0, 1)              \
  V(I32x4Neg, Operator::kNoProperties, 1, 0, 1)                            \
  V(I32x4Shl, Operator::kNoProperties, 2, 0, 1)                            \
  V(I32x4ShrS, Operator::kNoProperties, 2, 0, 1)                           \
  V(I32x4Add, Operator::kCommutative, 2, 0, 1)                             \
  V(I32x4AddHoriz, Operator::kNoProperties, 2, 0, 1)                       \
  V(I32x4Sub, Operator::kNoProperties, 2, 0, 1)                            \
  V(I32x4Mul, Operator::kCommutative, 2, 0, 1)                             \
  V(I32x4MinS, Operator::kCommutative, 2, 0, 1)                            \
  V(I32x4MaxS, Operator::kCommutative, 2, 0, 1)                            \
  V(I32x4Eq, Operator::kCommutative, 2, 0, 1)                              \
  V(I32x4Ne, Operator::kCommutative, 2, 0, 1)                              \
  V(I32x4GtS, Operator::kNoProperties, 2, 0, 1)                            \
  V(I32x4GeS, Operator::kNoProperties, 2, 0, 1)                            \
  V(I32x4UConvertF32x4, Operator::kNoProperties, 1, 0, 1)                  \
  V(I32x4UConvertI16x8Low, Operator::kNoProperties, 1, 0, 1)               \
  V(I32x4UConvertI16x8High, Operator::kNoProperties, 1, 0, 1)              \
  V(I32x4ShrU, Operator::kNoProperties, 2, 0, 1)                           \
  V(I32x4MinU, Operator::kCommutative, 2, 0, 1)                            \
  V(I32x4MaxU, Operator::kCommutative, 2, 0, 1)                            \
  V(I32x4GtU, Operator::kNoProperties, 2, 0, 1)                            \
  V(I32x4GeU, Operator::kNoProperties, 2, 0, 1)                            \
  V(I32x4Abs, Operator::kNoProperties, 1, 0, 1)                            \
  V(I32x4BitMask, Operator::kNoProperties, 1, 0, 1)                        \
  V(I32x4DotI16x8S, Operator::kCommutative, 2, 0, 1)                       \
  V(I32x4ExtMulLowI16x8S, Operator::kCommutative, 2, 0, 1)                 \
  V(I32x4ExtMulHighI16x8S, Operator::kCommutative, 2, 0, 1)                \
  V(I32x4ExtMulLowI16x8U, Operator::kCommutative, 2, 0, 1)                 \
  V(I32x4ExtMulHighI16x8U, Operator::kCommutative, 2, 0, 1)                \
  V(I32x4SignSelect, Operator::kNoProperties, 3, 0, 1)                     \
  V(I32x4ExtAddPairwiseI16x8S, Operator::kNoProperties, 1, 0, 1)           \
  V(I32x4ExtAddPairwiseI16x8U, Operator::kNoProperties, 1, 0, 1)           \
  V(I32x4TruncSatF64x2SZero, Operator::kNoProperties, 1, 0, 1)             \
  V(I32x4TruncSatF64x2UZero, Operator::kNoProperties, 1, 0, 1)             \
  V(I16x8Splat, Operator::kNoProperties, 1, 0, 1)                          \
  V(I16x8SConvertI8x16Low, Operator::kNoProperties, 1, 0, 1)               \
  V(I16x8SConvertI8x16High, Operator::kNoProperties, 1, 0, 1)              \
  V(I16x8Neg, Operator::kNoProperties, 1, 0, 1)                            \
  V(I16x8Shl, Operator::kNoProperties, 2, 0, 1)                            \
  V(I16x8ShrS, Operator::kNoProperties, 2, 0, 1)                           \
  V(I16x8SConvertI32x4, Operator::kNoProperties, 2, 0, 1)                  \
  V(I16x8Add, Operator::kCommutative, 2, 0, 1)                             \
  V(I16x8AddSatS, Operator::kCommutative, 2, 0, 1)                         \
  V(I16x8AddHoriz, Operator::kNoProperties, 2, 0, 1)                       \
  V(I16x8Sub, Operator::kNoProperties, 2, 0, 1)                            \
  V(I16x8SubSatS, Operator::kNoProperties, 2, 0, 1)                        \
  V(I16x8Mul, Operator::kCommutative, 2, 0, 1)                             \
  V(I16x8MinS, Operator::kCommutative, 2, 0, 1)                            \
  V(I16x8MaxS, Operator::kCommutative, 2, 0, 1)                            \
  V(I16x8Eq, Operator::kCommutative, 2, 0, 1)                              \
  V(I16x8Ne, Operator::kCommutative, 2, 0, 1)                              \
  V(I16x8GtS, Operator::kNoProperties, 2, 0, 1)                            \
  V(I16x8GeS, Operator::kNoProperties, 2, 0, 1)                            \
  V(I16x8UConvertI8x16Low, Operator::kNoProperties, 1, 0, 1)               \
  V(I16x8UConvertI8x16High, Operator::kNoProperties, 1, 0, 1)              \
  V(I16x8ShrU, Operator::kNoProperties, 2, 0, 1)                           \
  V(I16x8UConvertI32x4, Operator::kNoProperties, 2, 0, 1)                  \
  V(I16x8AddSatU, Operator::kCommutative, 2, 0, 1)                         \
  V(I16x8SubSatU, Operator::kNoProperties, 2, 0, 1)                        \
  V(I16x8MinU, Operator::kCommutative, 2, 0, 1)                            \
  V(I16x8MaxU, Operator::kCommutative, 2, 0, 1)                            \
  V(I16x8GtU, Operator::kNoProperties, 2, 0, 1)                            \
  V(I16x8GeU, Operator::kNoProperties, 2, 0, 1)                            \
  V(I16x8RoundingAverageU, Operator::kCommutative, 2, 0, 1)                \
  V(I16x8Q15MulRSatS, Operator::kCommutative, 2, 0, 1)                     \
  V(I16x8Abs, Operator::kNoProperties, 1, 0, 1)                            \
  V(I16x8BitMask, Operator::kNoProperties, 1, 0, 1)                        \
  V(I16x8ExtMulLowI8x16S, Operator::kCommutative, 2, 0, 1)                 \
  V(I16x8ExtMulHighI8x16S, Operator::kCommutative, 2, 0, 1)                \
  V(I16x8ExtMulLowI8x16U, Operator::kCommutative, 2, 0, 1)                 \
  V(I16x8ExtMulHighI8x16U, Operator::kCommutative, 2, 0, 1)                \
  V(I16x8SignSelect, Operator::kNoProperties, 3, 0, 1)                     \
  V(I16x8ExtAddPairwiseI8x16S, Operator::kNoProperties, 1, 0, 1)           \
  V(I16x8ExtAddPairwiseI8x16U, Operator::kNoProperties, 1, 0, 1)           \
  V(I8x16Splat, Operator::kNoProperties, 1, 0, 1)                          \
  V(I8x16Neg, Operator::kNoProperties, 1, 0, 1)                            \
  V(I8x16Shl, Operator::kNoProperties, 2, 0, 1)                            \
  V(I8x16ShrS, Operator::kNoProperties, 2, 0, 1)                           \
  V(I8x16SConvertI16x8, Operator::kNoProperties, 2, 0, 1)                  \
  V(I8x16Add, Operator::kCommutative, 2, 0, 1)                             \
  V(I8x16AddSatS, Operator::kCommutative, 2, 0, 1)                         \
  V(I8x16Sub, Operator::kNoProperties, 2, 0, 1)                            \
  V(I8x16SubSatS, Operator::kNoProperties, 2, 0, 1)                        \
  V(I8x16Mul, Operator::kCommutative, 2, 0, 1)                             \
  V(I8x16MinS, Operator::kCommutative, 2, 0, 1)                            \
  V(I8x16MaxS, Operator::kCommutative, 2, 0, 1)                            \
  V(I8x16Eq, Operator::kCommutative, 2, 0, 1)                              \
  V(I8x16Ne, Operator::kCommutative, 2, 0, 1)                              \
  V(I8x16GtS, Operator::kNoProperties, 2, 0, 1)                            \
  V(I8x16GeS, Operator::kNoProperties, 2, 0, 1)                            \
  V(I8x16ShrU, Operator::kNoProperties, 2, 0, 1)                           \
  V(I8x16UConvertI16x8, Operator::kNoProperties, 2, 0, 1)                  \
  V(I8x16AddSatU, Operator::kCommutative, 2, 0, 1)                         \
  V(I8x16SubSatU, Operator::kNoProperties, 2, 0, 1)                        \
  V(I8x16MinU, Operator::kCommutative, 2, 0, 1)                            \
  V(I8x16MaxU, Operator::kCommutative, 2, 0, 1)                            \
  V(I8x16GtU, Operator::kNoProperties, 2, 0, 1)                            \
  V(I8x16GeU, Operator::kNoProperties, 2, 0, 1)                            \
  V(I8x16RoundingAverageU, Operator::kCommutative, 2, 0, 1)                \
  V(I8x16Popcnt, Operator::kNoProperties, 1, 0, 1)                         \
  V(I8x16Abs, Operator::kNoProperties, 1, 0, 1)                            \
  V(I8x16BitMask, Operator::kNoProperties, 1, 0, 1)                        \
  V(I8x16SignSelect, Operator::kNoProperties, 3, 0, 1)                     \
  V(S128Load, Operator::kNoProperties, 2, 0, 1)                            \
  V(S128Store, Operator::kNoProperties, 3, 0, 1)                           \
  V(S128Zero, Operator::kNoProperties, 0, 0, 1)                            \
  V(S128And, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)     \
  V(S128Or, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)      \
  V(S128Xor, Operator::kAssociative | Operator::kCommutative, 2, 0, 1)     \
  V(S128Not, Operator::kNoProperties, 1, 0, 1)                             \
  V(S128Select, Operator::kNoProperties, 3, 0, 1)                          \
  V(S128AndNot, Operator::kNoProperties, 2, 0, 1)                          \
  V(V32x4AnyTrue, Operator::kNoProperties, 1, 0, 1)                        \
  V(V32x4AllTrue, Operator::kNoProperties, 1, 0, 1)                        \
  V(V16x8AnyTrue, Operator::kNoProperties, 1, 0, 1)                        \
  V(V16x8AllTrue, Operator::kNoProperties, 1, 0, 1)                        \
  V(V8x16AnyTrue, Operator::kNoProperties, 1, 0, 1)                        \
  V(V8x16AllTrue, Operator::kNoProperties, 1, 0, 1)                        \
  V(I8x16Swizzle, Operator::kNoProperties, 2, 0, 1)

// The format is:
// V(Name, properties, value_input_count, control_input_count, output_count)
#define PURE_OPTIONAL_OP_LIST(V)                            \
  V(Word32Ctz, Operator::kNoProperties, 1, 0, 1)            \
  V(Word64Ctz, Operator::kNoProperties, 1, 0, 1)            \
  V(Word32Rol, Operator::kNoProperties, 2, 0, 1)            \
  V(Word64Rol, Operator::kNoProperties, 2, 0, 1)            \
  V(Word32ReverseBits, Operator::kNoProperties, 1, 0, 1)    \
  V(Word64ReverseBits, Operator::kNoProperties, 1, 0, 1)    \
  V(Int32AbsWithOverflow, Operator::kNoProperties, 1, 0, 2) \
  V(Int64AbsWithOverflow, Operator::kNoProperties, 1, 0, 2) \
  V(Word32Popcnt, Operator::kNoProperties, 1, 0, 1)         \
  V(Word64Popcnt, Operator::kNoProperties, 1, 0, 1)         \
  V(Float32RoundDown, Operator::kNoProperties, 1, 0, 1)     \
  V(Float64RoundDown, Operator::kNoProperties, 1, 0, 1)     \
  V(Float32RoundUp, Operator::kNoProperties, 1, 0, 1)       \
  V(Float64RoundUp, Operator::kNoProperties, 1, 0, 1)       \
  V(Float32RoundTruncate, Operator::kNoProperties, 1, 0, 1) \
  V(Float64RoundTruncate, Operator::kNoProperties, 1, 0, 1) \
  V(Float64RoundTiesAway, Operator::kNoProperties, 1, 0, 1) \
  V(Float32RoundTiesEven, Operator::kNoProperties, 1, 0, 1) \
  V(Float64RoundTiesEven, Operator::kNoProperties, 1, 0, 1)

// The format is:
// V(Name, properties, value_input_count, control_input_count, output_count)
#define OVERFLOW_OP_LIST(V)                                                \
  V(Int32AddWithOverflow, Operator::kAssociative | Operator::kCommutative) \
  V(Int32SubWithOverflow, Operator::kNoProperties)                         \
  V(Int32MulWithOverflow, Operator::kAssociative | Operator::kCommutative) \
  V(Int64AddWithOverflow, Operator::kAssociative | Operator::kCommutative) \
  V(Int64SubWithOverflow, Operator::kNoProperties)

#define MACHINE_TYPE_LIST(V) \
  V(Float32)                 \
  V(Float64)                 \
  V(Simd128)                 \
  V(Int8)                    \
  V(Uint8)                   \
  V(Int16)                   \
  V(Uint16)                  \
  V(Int32)                   \
  V(Uint32)                  \
  V(Int64)                   \
  V(Uint64)                  \
  V(Pointer)                 \
  V(TaggedSigned)            \
  V(TaggedPointer)           \
  V(AnyTagged)               \
  V(CompressedPointer)       \
  V(AnyCompressed)

#define MACHINE_REPRESENTATION_LIST(V) \
  V(kFloat32)                          \
  V(kFloat64)                          \
  V(kSimd128)                          \
  V(kWord8)                            \
  V(kWord16)                           \
  V(kWord32)                           \
  V(kWord64)                           \
  V(kTaggedSigned)                     \
  V(kTaggedPointer)                    \
  V(kTagged)                           \
  V(kCompressedPointer)                \
  V(kCompressed)

#define LOAD_TRANSFORM_LIST(V) \
  V(S128Load8Splat)            \
  V(S128Load16Splat)           \
  V(S128Load32Splat)           \
  V(S128Load64Splat)           \
  V(S128Load8x8S)              \
  V(S128Load8x8U)              \
  V(S128Load16x4S)             \
  V(S128Load16x4U)             \
  V(S128Load32x2S)             \
  V(S128Load32x2U)             \
  V(S128Load32Zero)            \
  V(S128Load64Zero)

#define ATOMIC_U32_TYPE_LIST(V) \
  V(Uint8)                      \
  V(Uint16)                     \
  V(Uint32)

#define ATOMIC_TYPE_LIST(V) \
  ATOMIC_U32_TYPE_LIST(V)   \
  V(Int8)                   \
  V(Int16)                  \
  V(Int32)

#define ATOMIC_U64_TYPE_LIST(V) \
  ATOMIC_U32_TYPE_LIST(V)       \
  V(Uint64)

#define ATOMIC_REPRESENTATION_LIST(V) \
  V(kWord8)                           \
  V(kWord16)                          \
  V(kWord32)

#define ATOMIC64_REPRESENTATION_LIST(V) \
  ATOMIC_REPRESENTATION_LIST(V)         \
  V(kWord64)

#define SIMD_LANE_OP_LIST(V) \
  V(F64x2, 2)                \
  V(F32x4, 4)                \
  V(I64x2, 2)                \
  V(I32x4, 4)                \
  V(I16x8, 8)                \
  V(I8x16, 16)

#define SIMD_I64x2_LANES(V) V(0) V(1)

#define SIMD_I32x4_LANES(V) SIMD_I64x2_LANES(V) V(2) V(3)

#define SIMD_I16x8_LANES(V) SIMD_I32x4_LANES(V) V(4) V(5) V(6) V(7)

#define SIMD_I8x16_LANES(V) \
  SIMD_I16x8_LANES(V) V(8) V(9) V(10) V(11) V(12) V(13) V(14) V(15)

#define STACK_SLOT_CACHED_SIZES_ALIGNMENTS_LIST(V) \
  V(4, 0) V(8, 0) V(16, 0) V(4, 4) V(8, 8) V(16, 16)

template <IrOpcode::Value op, int value_input_count, int effect_input_count,
          int control_input_count, int value_output_count,
          int effect_output_count, int control_output_count>
struct CachedOperator : public Operator {
  CachedOperator(Operator::Properties properties, const char* mnemonic)
      : Operator(op, properties, mnemonic, value_input_count,
                 effect_input_count, control_input_count, value_output_count,
                 effect_output_count, control_output_count) {}
};

template <IrOpcode::Value op, int value_input_count, int control_input_count,
          int value_output_count>
struct CachedPureOperator : public Operator {
  CachedPureOperator(Operator::Properties properties, const char* mnemonic)
      : Operator(op, Operator::kPure | properties, mnemonic, value_input_count,
                 0, control_input_count, value_output_count, 0, 0) {}
};

template <class Op>
const Operator* GetCachedOperator() {
  STATIC_ASSERT(std::is_trivially_destructible<Op>::value);
  static const Op op;
  return &op;
}

template <class Op>
const Operator* GetCachedOperator(Operator::Properties properties,
                                  const char* mnemonic) {
#ifdef DEBUG
  static Operator::Properties const initial_properties = properties;
  static const char* const initial_mnemonic = mnemonic;
  DCHECK_EQ(properties, initial_properties);
  DCHECK_EQ(mnemonic, initial_mnemonic);
#endif
  STATIC_ASSERT(std::is_trivially_destructible<Op>::value);
  static const Op op(properties, mnemonic);
  return &op;
}

struct StackSlotOperator : public Operator1<StackSlotRepresentation> {
  explicit StackSlotOperator(int size, int alignment)
      : Operator1(IrOpcode::kStackSlot, Operator::kNoDeopt | Operator::kNoThrow,
                  "StackSlot", 0, 0, 0, 1, 0, 0,
                  StackSlotRepresentation(size, alignment)) {}
};

template <int size, int alignment>
struct CachedStackSlotOperator : StackSlotOperator {
  CachedStackSlotOperator() : StackSlotOperator(size, alignment) {}
};

#define PURE(Name, properties, value_input_count, control_input_count,         \
             output_count)                                                     \
  const OptionalOperator MachineOperatorBuilder::Name() {                      \
    return OptionalOperator(                                                   \
        flags_ & k##Name,                                                      \
        GetCachedOperator<                                                     \
            CachedPureOperator<IrOpcode::k##Name, value_input_count,           \
                               control_input_count, output_count>>(properties, \
                                                                   #Name));    \
  }
PURE_OPTIONAL_OP_LIST(PURE)
#undef PURE

#define OVERFLOW_OP(Name, properties)                                     \
  const Operator* MachineOperatorBuilder::Name() {                        \
    return GetCachedOperator<                                             \
        CachedOperator<IrOpcode::k##Name, 2, 0, 1, 2, 0, 0>>(             \
        Operator::kEliminatable | Operator::kNoRead | properties, #Name); \
  }
OVERFLOW_OP_LIST(OVERFLOW_OP)
#undef OVERFLOW_OP

template <ShiftKind kind>
struct Word32SarOperator : Operator1<ShiftKind> {
  Word32SarOperator()
      : Operator1(IrOpcode::kWord32Sar, Operator::kPure, "Word32Sar", 2, 0, 0,
                  1, 0, 0, kind) {}
};

const Operator* MachineOperatorBuilder::Word32Sar(ShiftKind kind) {
  switch (kind) {
    case ShiftKind::kNormal:
      return GetCachedOperator<Word32SarOperator<ShiftKind::kNormal>>();
    case ShiftKind::kShiftOutZeros:
      return GetCachedOperator<Word32SarOperator<ShiftKind::kShiftOutZeros>>();
  }
}

template <ShiftKind kind>
struct Word64SarOperator : Operator1<ShiftKind> {
  Word64SarOperator()
      : Operator1(IrOpcode::kWord64Sar, Operator::kPure, "Word64Sar", 2, 0, 0,
                  1, 0, 0, kind) {}
};

const Operator* MachineOperatorBuilder::Word64Sar(ShiftKind kind) {
  switch (kind) {
    case ShiftKind::kNormal:
      return GetCachedOperator<Word64SarOperator<ShiftKind::kNormal>>();
    case ShiftKind::kShiftOutZeros:
      return GetCachedOperator<Word64SarOperator<ShiftKind::kShiftOutZeros>>();
  }
}

template <MachineRepresentation rep, MachineSemantic sem>
struct LoadOperator : public Operator1<LoadRepresentation> {
  LoadOperator()
      : Operator1(IrOpcode::kLoad, Operator::kEliminatable, "Load", 2, 1, 1, 1,
                  1, 0, LoadRepresentation(rep, sem)) {}
};

template <MachineRepresentation rep, MachineSemantic sem>
struct PoisonedLoadOperator : public Operator1<LoadRepresentation> {
  PoisonedLoadOperator()
      : Operator1(IrOpcode::kPoisonedLoad, Operator::kEliminatable,
                  "PoisonedLoad", 2, 1, 1, 1, 1, 0,
                  LoadRepresentation(rep, sem)) {}
};

template <MachineRepresentation rep, MachineSemantic sem>
struct UnalignedLoadOperator : public Operator1<LoadRepresentation> {
  UnalignedLoadOperator()
      : Operator1(IrOpcode::kUnalignedLoad, Operator::kEliminatable,
                  "UnalignedLoad", 2, 1, 1, 1, 1, 0,
                  LoadRepresentation(rep, sem)) {}
};

template <MachineRepresentation rep, MachineSemantic sem>
struct ProtectedLoadOperator : public Operator1<LoadRepresentation> {
  ProtectedLoadOperator()
      : Operator1(IrOpcode::kProtectedLoad,
                  Operator::kNoDeopt | Operator::kNoThrow, "ProtectedLoad", 2,
                  1, 1, 1, 1, 0, LoadRepresentation(rep, sem)) {}
};

template <MemoryAccessKind kind, LoadTransformation type>
struct LoadTransformOperator : public Operator1<LoadTransformParameters> {
  LoadTransformOperator()
      : Operator1(IrOpcode::kLoadTransform,
                  kind == MemoryAccessKind::kProtected
                      ? Operator::kNoDeopt | Operator::kNoThrow
                      : Operator::kEliminatable,
                  "LoadTransform", 2, 1, 1, 1, 1, 0,
                  LoadTransformParameters{kind, type}) {}
};

template <MemoryAccessKind kind, MachineRepresentation rep, MachineSemantic sem,
          uint8_t laneidx>
struct LoadLaneOperator : public Operator1<LoadLaneParameters> {
  LoadLaneOperator()
      : Operator1(
            IrOpcode::kLoadLane,
            kind == MemoryAccessKind::kProtected
                ? Operator::kNoDeopt | Operator::kNoThrow
                : Operator::kEliminatable,
            "LoadLane", 3, 1, 1, 1, 1, 0,
            LoadLaneParameters{kind, LoadRepresentation(rep, sem), laneidx}) {}
};

template <MachineRepresentation rep, WriteBarrierKind write_barrier_kind>
struct StoreOperator : public Operator1<StoreRepresentation> {
  StoreOperator()
      : Operator1(IrOpcode::kStore,
                  Operator::kNoDeopt | Operator::kNoRead | Operator::kNoThrow,
                  "Store", 3, 1, 1, 0, 1, 0,
                  StoreRepresentation(rep, write_barrier_kind)) {}
};

template <MachineRepresentation rep>
struct UnalignedStoreOperator : public Operator1<UnalignedStoreRepresentation> {
  UnalignedStoreOperator()
      : Operator1(IrOpcode::kUnalignedStore,
                  Operator::kNoDeopt | Operator::kNoRead | Operator::kNoThrow,
                  "UnalignedStore", 3, 1, 1, 0, 1, 0, rep) {}
};

template <MachineRepresentation rep>
struct ProtectedStoreOperator : public Operator1<StoreRepresentation> {
  ProtectedStoreOperator()
      : Operator1(IrOpcode::kProtectedStore,
                  Operator::kNoDeopt | Operator::kNoRead | Operator::kNoThrow,
                  "Store", 3, 1, 1, 0, 1, 0,
                  StoreRepresentation(rep, kNoWriteBarrier)) {}
};

template <MemoryAccessKind kind, MachineRepresentation rep, uint8_t laneidx>
struct StoreLaneOperator : public Operator1<StoreLaneParameters> {
  StoreLaneOperator()
      : Operator1(IrOpcode::kStoreLane,
                  Operator::kNoDeopt | Operator::kNoRead | Operator::kNoThrow,
                  "StoreLane", 3, 1, 1, 0, 1, 0,
                  StoreLaneParameters{kind, rep, laneidx}) {}
};

template <MachineRepresentation rep, MachineSemantic sem>
struct Word32AtomicLoadOperator : public Operator1<LoadRepresentation> {
  Word32AtomicLoadOperator()
      : Operator1(IrOpcode::kWord32AtomicLoad, Operator::kEliminatable,
                  "Word32AtomicLoad", 2, 1, 1, 1, 1, 0, MachineType(rep, sem)) {
  }
};

template <MachineRepresentation rep, MachineSemantic sem>
struct Word64AtomicLoadOperator : public Operator1<LoadRepresentation> {
  Word64AtomicLoadOperator()
      : Operator1(IrOpcode::kWord64AtomicLoad, Operator::kEliminatable,
                  "Word64AtomicLoad", 2, 1, 1, 1, 1, 0, MachineType(rep, sem)) {
  }
};

template <MachineRepresentation rep>
struct Word32AtomicStoreOperator : public Operator1<MachineRepresentation> {
  Word32AtomicStoreOperator()
      : Operator1(IrOpcode::kWord32AtomicStore,
                  Operator::kNoDeopt | Operator::kNoRead | Operator::kNoThrow,
                  "Word32AtomicStore", 3, 1, 1, 0, 1, 0, rep) {}
};

template <MachineRepresentation rep>
struct Word64AtomicStoreOperator : public Operator1<MachineRepresentation> {
  Word64AtomicStoreOperator()
      : Operator1(IrOpcode::kWord64AtomicStore,
                  Operator::kNoDeopt | Operator::kNoRead | Operator::kNoThrow,
                  "Word64AtomicStore", 3, 1, 1, 0, 1, 0, rep) {}
};

#define ATOMIC_OP(op)                                                         \
  template <MachineRepresentation rep, MachineSemantic sem>                   \
  struct op##Operator : public Operator1<MachineType> {                       \
    op##Operator()                                                            \
        : Operator1(IrOpcode::k##op, Operator::kNoDeopt | Operator::kNoThrow, \
                    #op, 3, 1, 1, 1, 1, 0, MachineType(rep, sem)) {}          \
  };
ATOMIC_OP(Word32AtomicAdd)
ATOMIC_OP(Word32AtomicSub)
ATOMIC_OP(Word32AtomicAnd)
ATOMIC_OP(Word32AtomicOr)
ATOMIC_OP(Word32AtomicXor)
ATOMIC_OP(Word32AtomicExchange)
ATOMIC_OP(Word64AtomicAdd)
ATOMIC_OP(Word64AtomicSub)
ATOMIC_OP(Word64AtomicAnd)
ATOMIC_OP(Word64AtomicOr)
ATOMIC_OP(Word64AtomicXor)
ATOMIC_OP(Word64AtomicExchange)
#undef ATOMIC_OP

template <MachineRepresentation rep, MachineSemantic sem>
struct Word32AtomicCompareExchangeOperator : public Operator1<MachineType> {
  Word32AtomicCompareExchangeOperator()
      : Operator1(IrOpcode::kWord32AtomicCompareExchange,
                  Operator::kNoDeopt | Operator::kNoThrow,
                  "Word32AtomicCompareExchange", 4, 1, 1, 1, 1, 0,
                  MachineType(rep, sem)) {}
};

template <MachineRepresentation rep, MachineSemantic sem>
struct Word64AtomicCompareExchangeOperator : public Operator1<MachineType> {
  Word64AtomicCompareExchangeOperator()
      : Operator1(IrOpcode::kWord64AtomicCompareExchange,
                  Operator::kNoDeopt | Operator::kNoThrow,
                  "Word64AtomicCompareExchange", 4, 1, 1, 1, 1, 0,
                  MachineType(rep, sem)) {}
};

struct Word32AtomicPairLoadOperator : public Operator {
  Word32AtomicPairLoadOperator()
      : Operator(IrOpcode::kWord32AtomicPairLoad,
                 Operator::kNoDeopt | Operator::kNoThrow,
                 "Word32AtomicPairLoad", 2, 1, 1, 2, 1, 0) {}
};

struct Word32AtomicPairStoreOperator : public Operator {
  Word32AtomicPairStoreOperator()
      : Operator(IrOpcode::kWord32AtomicPairStore,
                 Operator::kNoDeopt | Operator::kNoThrow,
                 "Word32AtomicPairStore", 4, 1, 1, 0, 1, 0) {}
};

#define ATOMIC_PAIR_OP(op)                                      \
  struct Word32AtomicPair##op##Operator : public Operator {     \
    Word32AtomicPair##op##Operator()                            \
        : Operator(IrOpcode::kWord32AtomicPair##op,             \
                   Operator::kNoDeopt | Operator::kNoThrow,     \
                   "Word32AtomicPair" #op, 4, 1, 1, 2, 1, 0) {} \
  };
ATOMIC_PAIR_OP(Add)
ATOMIC_PAIR_OP(Sub)
ATOMIC_PAIR_OP(And)
ATOMIC_PAIR_OP(Or)
ATOMIC_PAIR_OP(Xor)
ATOMIC_PAIR_OP(Exchange)
#undef ATOMIC_PAIR_OP

struct Word32AtomicPairCompareExchangeOperator : public Operator {
  Word32AtomicPairCompareExchangeOperator()
      : Operator(IrOpcode::kWord32AtomicPairCompareExchange,
                 Operator::kNoDeopt | Operator::kNoThrow,
                 "Word32AtomicPairCompareExchange", 6, 1, 1, 2, 1, 0) {}
};

struct MemoryBarrierOperator : public Operator {
  MemoryBarrierOperator()
      : Operator(IrOpcode::kMemoryBarrier,
                 Operator::kNoDeopt | Operator::kNoThrow, "MemoryBarrier", 0, 1,
                 1, 0, 1, 0) {}
};

// The {BitcastWordToTagged} operator must not be marked as pure (especially
// not idempotent), because otherwise the splitting logic in the Scheduler
// might decide to split these operators, thus potentially creating live
// ranges of allocation top across calls or other things that might allocate.
// See https://bugs.chromium.org/p/v8/issues/detail?id=6059 for more details.
struct BitcastWordToTaggedOperator : public Operator {
  BitcastWordToTaggedOperator()
      : Operator(IrOpcode::kBitcastWordToTagged,
                 Operator::kEliminatable | Operator::kNoWrite,
                 "BitcastWordToTagged", 1, 1, 1, 1, 1, 0) {}
};

struct BitcastTaggedToWordOperator : public Operator {
  BitcastTaggedToWordOperator()
      : Operator(IrOpcode::kBitcastTaggedToWord,
                 Operator::kEliminatable | Operator::kNoWrite,
                 "BitcastTaggedToWord", 1, 1, 1, 1, 1, 0) {}
};

struct BitcastMaybeObjectToWordOperator : public Operator {
  BitcastMaybeObjectToWordOperator()
      : Operator(IrOpcode::kBitcastTaggedToWord,
                 Operator::kEliminatable | Operator::kNoWrite,
                 "BitcastMaybeObjectToWord", 1, 1, 1, 1, 1, 0) {}
};

struct TaggedPoisonOnSpeculationOperator : public Operator {
  TaggedPoisonOnSpeculationOperator()
      : Operator(IrOpcode::kTaggedPoisonOnSpeculation,
                 Operator::kEliminatable | Operator::kNoWrite,
                 "TaggedPoisonOnSpeculation", 1, 1, 1, 1, 1, 0) {}
};

struct Word32PoisonOnSpeculationOperator : public Operator {
  Word32PoisonOnSpeculationOperator()
      : Operator(IrOpcode::kWord32PoisonOnSpeculation,
                 Operator::kEliminatable | Operator::kNoWrite,
                 "Word32PoisonOnSpeculation", 1, 1, 1, 1, 1, 0) {}
};

struct Word64PoisonOnSpeculationOperator : public Operator {
  Word64PoisonOnSpeculationOperator()
      : Operator(IrOpcode::kWord64PoisonOnSpeculation,
                 Operator::kEliminatable | Operator::kNoWrite,
                 "Word64PoisonOnSpeculation", 1, 1, 1, 1, 1, 0) {}
};

struct AbortCSAAssertOperator : public Operator {
  AbortCSAAssertOperator()
      : Operator(IrOpcode::kAbortCSAAssert, Operator::kNoThrow,
                 "AbortCSAAssert", 1, 1, 1, 0, 1, 0) {}
};

struct DebugBreakOperator : public Operator {
  DebugBreakOperator()
      : Operator(IrOpcode::kDebugBreak, Operator::kNoThrow, "DebugBreak", 0, 1,
                 1, 0, 1, 0) {}
};

struct UnsafePointerAddOperator : public Operator {
  UnsafePointerAddOperator()
      : Operator(IrOpcode::kUnsafePointerAdd, Operator::kKontrol,
                 "UnsafePointerAdd", 2, 1, 1, 1, 1, 0) {}
};

template <StackCheckKind kind>
struct StackPointerGreaterThanOperator : public Operator1<StackCheckKind> {
  StackPointerGreaterThanOperator()
      : Operator1(IrOpcode::kStackPointerGreaterThan, Operator::kEliminatable,
                  "StackPointerGreaterThan", 1, 1, 0, 1, 1, 0, kind) {}
};

struct CommentOperator : public Operator1<const char*> {
  explicit CommentOperator(const char* msg)
      : Operator1(IrOpcode::kComment, Operator::kNoThrow | Operator::kNoWrite,
                  "Comment", 0, 1, 1, 0, 1, 0, msg) {}
};

MachineOperatorBuilder::MachineOperatorBuilder(
    Zone* zone, MachineRepresentation word, Flags flags,
    AlignmentRequirements alignmentRequirements)
    : zone_(zone),
      word_(word),
      flags_(flags),
      alignment_requirements_(alignmentRequirements) {
  DCHECK(word == MachineRepresentation::kWord32 ||
         word == MachineRepresentation::kWord64);
}

const Operator* MachineOperatorBuilder::UnalignedLoad(LoadRepresentation rep) {
#define LOAD(Type)                                                  \
  if (rep == MachineType::Type()) {                                 \
    return GetCachedOperator<                                       \
        UnalignedLoadOperator<MachineType::Type().representation(), \
                              MachineType::Type().semantic()>>();   \
  }
  MACHINE_TYPE_LIST(LOAD)
#undef LOAD
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::UnalignedStore(
    UnalignedStoreRepresentation rep) {
  switch (rep) {
#define STORE(kRep)                 \
  case MachineRepresentation::kRep: \
    return GetCachedOperator<       \
        UnalignedStoreOperator<MachineRepresentation::kRep>>();
    MACHINE_REPRESENTATION_LIST(STORE)
#undef STORE
    case MachineRepresentation::kBit:
    case MachineRepresentation::kNone:
      break;
  }
  UNREACHABLE();
}

template <TruncateKind kind>
struct TruncateFloat32ToUint32Operator : Operator1<TruncateKind> {
  TruncateFloat32ToUint32Operator()
      : Operator1(IrOpcode::kTruncateFloat32ToUint32, Operator::kPure,
                  "TruncateFloat32ToUint32", 1, 0, 0, 1, 0, 0, kind) {}
};

const Operator* MachineOperatorBuilder::TruncateFloat32ToUint32(
    TruncateKind kind) {
  switch (kind) {
    case TruncateKind::kArchitectureDefault:
      return GetCachedOperator<TruncateFloat32ToUint32Operator<
          TruncateKind::kArchitectureDefault>>();
    case TruncateKind::kSetOverflowToMin:
      return GetCachedOperator<
          TruncateFloat32ToUint32Operator<TruncateKind::kSetOverflowToMin>>();
  }
}

template <TruncateKind kind>
struct TruncateFloat32ToInt32Operator : Operator1<TruncateKind> {
  TruncateFloat32ToInt32Operator()
      : Operator1(IrOpcode::kTruncateFloat32ToInt32, Operator::kPure,
                  "TruncateFloat32ToInt32", 1, 0, 0, 1, 0, 0, kind) {}
};

const Operator* MachineOperatorBuilder::TruncateFloat32ToInt32(
    TruncateKind kind) {
  switch (kind) {
    case TruncateKind::kArchitectureDefault:
      return GetCachedOperator<
          TruncateFloat32ToInt32Operator<TruncateKind::kArchitectureDefault>>();
    case TruncateKind::kSetOverflowToMin:
      return GetCachedOperator<
          TruncateFloat32ToInt32Operator<TruncateKind::kSetOverflowToMin>>();
  }
}

template <TruncateKind kind>
struct TruncateFloat64ToInt64Operator : Operator1<TruncateKind> {
  TruncateFloat64ToInt64Operator()
      : Operator1(IrOpcode::kTruncateFloat64ToInt64, Operator::kPure,
                  "TruncateFloat64ToInt64", 1, 0, 0, 1, 0, 0, kind) {}
};

const Operator* MachineOperatorBuilder::TruncateFloat64ToInt64(
    TruncateKind kind) {
  switch (kind) {
    case TruncateKind::kArchitectureDefault:
      return GetCachedOperator<
          TruncateFloat64ToInt64Operator<TruncateKind::kArchitectureDefault>>();
    case TruncateKind::kSetOverflowToMin:
      return GetCachedOperator<
          TruncateFloat64ToInt64Operator<TruncateKind::kSetOverflowToMin>>();
  }
}

size_t hash_value(TruncateKind kind) { return static_cast<size_t>(kind); }

std::ostream& operator<<(std::ostream& os, TruncateKind kind) {
  switch (kind) {
    case TruncateKind::kArchitectureDefault:
      return os << "kArchitectureDefault";
    case TruncateKind::kSetOverflowToMin:
      return os << "kSetOverflowToMin";
  }
}

#define PURE(Name, properties, value_input_count, control_input_count,     \
             output_count)                                                 \
  const Operator* MachineOperatorBuilder::Name() {                         \
    return GetCachedOperator<                                              \
        CachedPureOperator<IrOpcode::k##Name, value_input_count,           \
                           control_input_count, output_count>>(properties, \
                                                               #Name);     \
  }
MACHINE_PURE_OP_LIST(PURE)
#undef PURE

const Operator* MachineOperatorBuilder::PrefetchTemporal() {
  return GetCachedOperator<
      CachedOperator<IrOpcode::kPrefetchTemporal, 2, 1, 1, 0, 1, 0>>(
      Operator::kNoDeopt | Operator::kNoThrow, "PrefetchTemporal");
}

const Operator* MachineOperatorBuilder::PrefetchNonTemporal() {
  return GetCachedOperator<
      CachedOperator<IrOpcode::kPrefetchNonTemporal, 2, 1, 1, 0, 1, 0>>(
      Operator::kNoDeopt | Operator::kNoThrow, "PrefetchNonTemporal");
}

const Operator* MachineOperatorBuilder::Load(LoadRepresentation rep) {
#define LOAD(Type)                                         \
  if (rep == MachineType::Type()) {                        \
    return GetCachedOperator<                              \
        LoadOperator<MachineType::Type().representation(), \
                     MachineType::Type().semantic()>>();   \
  }
  MACHINE_TYPE_LIST(LOAD)
#undef LOAD
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::PoisonedLoad(LoadRepresentation rep) {
#define LOAD(Type)                                                 \
  if (rep == MachineType::Type()) {                                \
    return GetCachedOperator<                                      \
        PoisonedLoadOperator<MachineType::Type().representation(), \
                             MachineType::Type().semantic()>>();   \
  }
  MACHINE_TYPE_LIST(LOAD)
#undef LOAD
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::ProtectedLoad(LoadRepresentation rep) {
#define LOAD(Type)                                                  \
  if (rep == MachineType::Type()) {                                 \
    return GetCachedOperator<                                       \
        ProtectedLoadOperator<MachineType::Type().representation(), \
                              MachineType::Type().semantic()>>();   \
  }
  MACHINE_TYPE_LIST(LOAD)
#undef LOAD
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::LoadTransform(
    MemoryAccessKind kind, LoadTransformation transform) {
#define LOAD_TRANSFORM_KIND(TYPE, KIND)                             \
  if (kind == MemoryAccessKind::k##KIND &&                          \
      transform == LoadTransformation::k##TYPE) {                   \
    return GetCachedOperator<LoadTransformOperator<                 \
        MemoryAccessKind::k##KIND, LoadTransformation::k##TYPE>>(); \
  }
#define LOAD_TRANSFORM(TYPE)           \
  LOAD_TRANSFORM_KIND(TYPE, Normal)    \
  LOAD_TRANSFORM_KIND(TYPE, Unaligned) \
  LOAD_TRANSFORM_KIND(TYPE, Protected)

  LOAD_TRANSFORM_LIST(LOAD_TRANSFORM)
#undef LOAD_TRANSFORM
#undef LOAD_TRANSFORM_KIND
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::LoadLane(MemoryAccessKind kind,
                                                 LoadRepresentation rep,
                                                 uint8_t laneidx) {
#define LOAD_LANE_KIND(TYPE, KIND, LANEIDX)                              \
  if (kind == MemoryAccessKind::k##KIND && rep == MachineType::TYPE() && \
      laneidx == LANEIDX) {                                              \
    return GetCachedOperator<LoadLaneOperator<                           \
        MemoryAccessKind::k##KIND, MachineType::TYPE().representation(), \
        MachineType::TYPE().semantic(), LANEIDX>>();                     \
  }

#define LOAD_LANE_T(T, LANE)         \
  LOAD_LANE_KIND(T, Normal, LANE)    \
  LOAD_LANE_KIND(T, Unaligned, LANE) \
  LOAD_LANE_KIND(T, Protected, LANE)

#define LOAD_LANE_INT8(LANE) LOAD_LANE_T(Int8, LANE)
#define LOAD_LANE_INT16(LANE) LOAD_LANE_T(Int16, LANE)
#define LOAD_LANE_INT32(LANE) LOAD_LANE_T(Int32, LANE)
#define LOAD_LANE_INT64(LANE) LOAD_LANE_T(Int64, LANE)

  // Semicolons unnecessary, but helps formatting.
  SIMD_I8x16_LANES(LOAD_LANE_INT8);
  SIMD_I16x8_LANES(LOAD_LANE_INT16);
  SIMD_I32x4_LANES(LOAD_LANE_INT32);
  SIMD_I64x2_LANES(LOAD_LANE_INT64);
#undef LOAD_LANE_INT8
#undef LOAD_LANE_INT16
#undef LOAD_LANE_INT32
#undef LOAD_LANE_INT64
#undef LOAD_LANE_KIND
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::StoreLane(MemoryAccessKind kind,
                                                  MachineRepresentation rep,
                                                  uint8_t laneidx) {
#define STORE_LANE_KIND(REP, KIND, LANEIDX)                                 \
  if (kind == MemoryAccessKind::k##KIND &&                                  \
      rep == MachineRepresentation::REP && laneidx == LANEIDX) {            \
    return GetCachedOperator<StoreLaneOperator<                             \
        MemoryAccessKind::k##KIND, MachineRepresentation::REP, LANEIDX>>(); \
  }

#define STORE_LANE_T(T, LANE)         \
  STORE_LANE_KIND(T, Normal, LANE)    \
  STORE_LANE_KIND(T, Unaligned, LANE) \
  STORE_LANE_KIND(T, Protected, LANE)

#define STORE_LANE_WORD8(LANE) STORE_LANE_T(kWord8, LANE)
#define STORE_LANE_WORD16(LANE) STORE_LANE_T(kWord16, LANE)
#define STORE_LANE_WORD32(LANE) STORE_LANE_T(kWord32, LANE)
#define STORE_LANE_WORD64(LANE) STORE_LANE_T(kWord64, LANE)

  // Semicolons unnecessary, but helps formatting.
  SIMD_I8x16_LANES(STORE_LANE_WORD8);
  SIMD_I16x8_LANES(STORE_LANE_WORD16);
  SIMD_I32x4_LANES(STORE_LANE_WORD32);
  SIMD_I64x2_LANES(STORE_LANE_WORD64);
#undef STORE_LANE_WORD8
#undef STORE_LANE_WORD16
#undef STORE_LANE_WORD32
#undef STORE_LANE_WORD64
#undef STORE_LANE_KIND
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::StackSlot(int size, int alignment) {
  DCHECK_LE(0, size);
  DCHECK(alignment == 0 || alignment == 4 || alignment == 8 || alignment == 16);
#define CASE_CACHED_SIZE(Size, Alignment)                                 \
  if (size == Size && alignment == Alignment) {                           \
    return GetCachedOperator<CachedStackSlotOperator<Size, Alignment>>(); \
  }

  STACK_SLOT_CACHED_SIZES_ALIGNMENTS_LIST(CASE_CACHED_SIZE)

#undef CASE_CACHED_SIZE
  return zone_->New<StackSlotOperator>(size, alignment);
}

const Operator* MachineOperatorBuilder::StackSlot(MachineRepresentation rep,
                                                  int alignment) {
  return StackSlot(1 << ElementSizeLog2Of(rep), alignment);
}

const Operator* MachineOperatorBuilder::Store(StoreRepresentation store_rep) {
  switch (store_rep.representation()) {
#define STORE(kRep)                                                           \
  case MachineRepresentation::kRep:                                           \
    switch (store_rep.write_barrier_kind()) {                                 \
      case kNoWriteBarrier:                                                   \
        return GetCachedOperator<                                             \
            StoreOperator<MachineRepresentation::kRep, kNoWriteBarrier>>();   \
      case kAssertNoWriteBarrier:                                             \
        return GetCachedOperator<StoreOperator<MachineRepresentation::kRep,   \
                                               kAssertNoWriteBarrier>>();     \
      case kMapWriteBarrier:                                                  \
        return GetCachedOperator<                                             \
            StoreOperator<MachineRepresentation::kRep, kMapWriteBarrier>>();  \
      case kPointerWriteBarrier:                                              \
        return GetCachedOperator<StoreOperator<MachineRepresentation::kRep,   \
                                               kPointerWriteBarrier>>();      \
      case kEphemeronKeyWriteBarrier:                                         \
        return GetCachedOperator<StoreOperator<MachineRepresentation::kRep,   \
                                               kEphemeronKeyWriteBarrier>>(); \
      case kFullWriteBarrier:                                                 \
        return GetCachedOperator<                                             \
            StoreOperator<MachineRepresentation::kRep, kFullWriteBarrier>>(); \
    }                                                                         \
    break;
    MACHINE_REPRESENTATION_LIST(STORE)
#undef STORE
    case MachineRepresentation::kBit:
    case MachineRepresentation::kNone:
      break;
  }
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::ProtectedStore(
    MachineRepresentation rep) {
  switch (rep) {
#define STORE(kRep)                                             \
  case MachineRepresentation::kRep:                             \
    return GetCachedOperator<                                   \
        ProtectedStoreOperator<MachineRepresentation::kRep>>(); \
    break;
    MACHINE_REPRESENTATION_LIST(STORE)
#undef STORE
    case MachineRepresentation::kBit:
    case MachineRepresentation::kNone:
      break;
  }
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::UnsafePointerAdd() {
  return GetCachedOperator<UnsafePointerAddOperator>();
}

const Operator* MachineOperatorBuilder::StackPointerGreaterThan(
    StackCheckKind kind) {
  switch (kind) {
    case StackCheckKind::kJSFunctionEntry:
      return GetCachedOperator<
          StackPointerGreaterThanOperator<StackCheckKind::kJSFunctionEntry>>();
    case StackCheckKind::kJSIterationBody:
      return GetCachedOperator<
          StackPointerGreaterThanOperator<StackCheckKind::kJSIterationBody>>();
    case StackCheckKind::kCodeStubAssembler:
      return GetCachedOperator<StackPointerGreaterThanOperator<
          StackCheckKind::kCodeStubAssembler>>();
    case StackCheckKind::kWasm:
      return GetCachedOperator<
          StackPointerGreaterThanOperator<StackCheckKind::kWasm>>();
  }
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::BitcastWordToTagged() {
  return GetCachedOperator<BitcastWordToTaggedOperator>();
}

const Operator* MachineOperatorBuilder::BitcastTaggedToWord() {
  return GetCachedOperator<BitcastTaggedToWordOperator>();
}

const Operator* MachineOperatorBuilder::BitcastMaybeObjectToWord() {
  return GetCachedOperator<BitcastMaybeObjectToWordOperator>();
}

const Operator* MachineOperatorBuilder::AbortCSAAssert() {
  return GetCachedOperator<AbortCSAAssertOperator>();
}

const Operator* MachineOperatorBuilder::DebugBreak() {
  return GetCachedOperator<DebugBreakOperator>();
}

const Operator* MachineOperatorBuilder::Comment(const char* msg) {
  return zone_->New<CommentOperator>(msg);
}

const Operator* MachineOperatorBuilder::MemBarrier() {
  return GetCachedOperator<MemoryBarrierOperator>();
}

const Operator* MachineOperatorBuilder::Word32AtomicLoad(
    LoadRepresentation rep) {
#define LOAD(Type)                                                     \
  if (rep == MachineType::Type()) {                                    \
    return GetCachedOperator<                                          \
        Word32AtomicLoadOperator<MachineType::Type().representation(), \
                                 MachineType::Type().semantic()>>();   \
  }
  ATOMIC_TYPE_LIST(LOAD)
#undef LOAD
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::Word32AtomicStore(
    MachineRepresentation rep) {
#define STORE(kRep)                                                \
  if (rep == MachineRepresentation::kRep) {                        \
    return GetCachedOperator<                                      \
        Word32AtomicStoreOperator<MachineRepresentation::kRep>>(); \
  }
  ATOMIC_REPRESENTATION_LIST(STORE)
#undef STORE
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::Word32AtomicExchange(MachineType type) {
#define EXCHANGE(Type)                                                     \
  if (type == MachineType::Type()) {                                       \
    return GetCachedOperator<                                              \
        Word32AtomicExchangeOperator<MachineType::Type().representation(), \
                                     MachineType::Type().semantic()>>();   \
  }
  ATOMIC_TYPE_LIST(EXCHANGE)
#undef EXCHANGE
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::Word32AtomicCompareExchange(
    MachineType type) {
#define COMPARE_EXCHANGE(Type)                                    \
  if (type == MachineType::Type()) {                              \
    return GetCachedOperator<Word32AtomicCompareExchangeOperator< \
        MachineType::Type().representation(),                     \
        MachineType::Type().semantic()>>();                       \
  }
  ATOMIC_TYPE_LIST(COMPARE_EXCHANGE)
#undef COMPARE_EXCHANGE
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::Word32AtomicAdd(MachineType type) {
#define ADD(Type)                                                     \
  if (type == MachineType::Type()) {                                  \
    return GetCachedOperator<                                         \
        Word32AtomicAddOperator<MachineType::Type().representation(), \
                                MachineType::Type().semantic()>>();   \
  }
  ATOMIC_TYPE_LIST(ADD)
#undef ADD
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::Word32AtomicSub(MachineType type) {
#define SUB(Type)                                                     \
  if (type == MachineType::Type()) {                                  \
    return GetCachedOperator<                                         \
        Word32AtomicSubOperator<MachineType::Type().representation(), \
                                MachineType::Type().semantic()>>();   \
  }
  ATOMIC_TYPE_LIST(SUB)
#undef SUB
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::Word32AtomicAnd(MachineType type) {
#define AND(Type)                                                     \
  if (type == MachineType::Type()) {                                  \
    return GetCachedOperator<                                         \
        Word32AtomicAndOperator<MachineType::Type().representation(), \
                                MachineType::Type().semantic()>>();   \
  }
  ATOMIC_TYPE_LIST(AND)
#undef AND
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::Word32AtomicOr(MachineType type) {
#define OR(Type)                                                     \
  if (type == MachineType::Type()) {                                 \
    return GetCachedOperator<                                        \
        Word32AtomicOrOperator<MachineType::Type().representation(), \
                               MachineType::Type().semantic()>>();   \
  }
  ATOMIC_TYPE_LIST(OR)
#undef OR
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::Word32AtomicXor(MachineType type) {
#define XOR(Type)                                                     \
  if (type == MachineType::Type()) {                                  \
    return GetCachedOperator<                                         \
        Word32AtomicXorOperator<MachineType::Type().representation(), \
                                MachineType::Type().semantic()>>();   \
  }
  ATOMIC_TYPE_LIST(XOR)
#undef XOR
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::Word64AtomicLoad(
    LoadRepresentation rep) {
#define LOAD(Type)                                                     \
  if (rep == MachineType::Type()) {                                    \
    return GetCachedOperator<                                          \
        Word64AtomicLoadOperator<MachineType::Type().representation(), \
                                 MachineType::Type().semantic()>>();   \
  }
  ATOMIC_U64_TYPE_LIST(LOAD)
#undef LOAD
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::Word64AtomicStore(
    MachineRepresentation rep) {
#define STORE(kRep)                                                \
  if (rep == MachineRepresentation::kRep) {                        \
    return GetCachedOperator<                                      \
        Word64AtomicStoreOperator<MachineRepresentation::kRep>>(); \
  }
  ATOMIC64_REPRESENTATION_LIST(STORE)
#undef STORE
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::Word64AtomicAdd(MachineType type) {
#define ADD(Type)                                                     \
  if (type == MachineType::Type()) {                                  \
    return GetCachedOperator<                                         \
        Word64AtomicAddOperator<MachineType::Type().representation(), \
                                MachineType::Type().semantic()>>();   \
  }
  ATOMIC_U64_TYPE_LIST(ADD)
#undef ADD
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::Word64AtomicSub(MachineType type) {
#define SUB(Type)                                                     \
  if (type == MachineType::Type()) {                                  \
    return GetCachedOperator<                                         \
        Word64AtomicSubOperator<MachineType::Type().representation(), \
                                MachineType::Type().semantic()>>();   \
  }
  ATOMIC_U64_TYPE_LIST(SUB)
#undef SUB
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::Word64AtomicAnd(MachineType type) {
#define AND(Type)                                                     \
  if (type == MachineType::Type()) {                                  \
    return GetCachedOperator<                                         \
        Word64AtomicAndOperator<MachineType::Type().representation(), \
                                MachineType::Type().semantic()>>();   \
  }
  ATOMIC_U64_TYPE_LIST(AND)
#undef AND
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::Word64AtomicOr(MachineType type) {
#define OR(Type)                                                     \
  if (type == MachineType::Type()) {                                 \
    return GetCachedOperator<                                        \
        Word64AtomicOrOperator<MachineType::Type().representation(), \
                               MachineType::Type().semantic()>>();   \
  }
  ATOMIC_U64_TYPE_LIST(OR)
#undef OR
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::Word64AtomicXor(MachineType type) {
#define XOR(Type)                                                     \
  if (type == MachineType::Type()) {                                  \
    return GetCachedOperator<                                         \
        Word64AtomicXorOperator<MachineType::Type().representation(), \
                                MachineType::Type().semantic()>>();   \
  }
  ATOMIC_U64_TYPE_LIST(XOR)
#undef XOR
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::Word64AtomicExchange(MachineType type) {
#define EXCHANGE(Type)                                                     \
  if (type == MachineType::Type()) {                                       \
    return GetCachedOperator<                                              \
        Word64AtomicExchangeOperator<MachineType::Type().representation(), \
                                     MachineType::Type().semantic()>>();   \
  }
  ATOMIC_U64_TYPE_LIST(EXCHANGE)
#undef EXCHANGE
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::Word64AtomicCompareExchange(
    MachineType type) {
#define COMPARE_EXCHANGE(Type)                                    \
  if (type == MachineType::Type()) {                              \
    return GetCachedOperator<Word64AtomicCompareExchangeOperator< \
        MachineType::Type().representation(),                     \
        MachineType::Type().semantic()>>();                       \
  }
  ATOMIC_U64_TYPE_LIST(COMPARE_EXCHANGE)
#undef COMPARE_EXCHANGE
  UNREACHABLE();
}

const Operator* MachineOperatorBuilder::Word32AtomicPairLoad() {
  return GetCachedOperator<Word32AtomicPairLoadOperator>();
}

const Operator* MachineOperatorBuilder::Word32AtomicPairStore() {
  return GetCachedOperator<Word32AtomicPairStoreOperator>();
}

const Operator* MachineOperatorBuilder::Word32AtomicPairAdd() {
  return GetCachedOperator<Word32AtomicPairAddOperator>();
}

const Operator* MachineOperatorBuilder::Word32AtomicPairSub() {
  return GetCachedOperator<Word32AtomicPairSubOperator>();
}

const Operator* MachineOperatorBuilder::Word32AtomicPairAnd() {
  return GetCachedOperator<Word32AtomicPairAndOperator>();
}

const Operator* MachineOperatorBuilder::Word32AtomicPairOr() {
  return GetCachedOperator<Word32AtomicPairOrOperator>();
}

const Operator* MachineOperatorBuilder::Word32AtomicPairXor() {
  return GetCachedOperator<Word32AtomicPairXorOperator>();
}

const Operator* MachineOperatorBuilder::Word32AtomicPairExchange() {
  return GetCachedOperator<Word32AtomicPairExchangeOperator>();
}

const Operator* MachineOperatorBuilder::Word32AtomicPairCompareExchange() {
  return GetCachedOperator<Word32AtomicPairCompareExchangeOperator>();
}

const Operator* MachineOperatorBuilder::TaggedPoisonOnSpeculation() {
  return GetCachedOperator<TaggedPoisonOnSpeculationOperator>();
}

const Operator* MachineOperatorBuilder::Word32PoisonOnSpeculation() {
  return GetCachedOperator<Word32PoisonOnSpeculationOperator>();
}

const Operator* MachineOperatorBuilder::Word64PoisonOnSpeculation() {
  return GetCachedOperator<Word64PoisonOnSpeculationOperator>();
}

#define EXTRACT_LANE_OP(Type, Sign, lane_count)                      \
  const Operator* MachineOperatorBuilder::Type##ExtractLane##Sign(   \
      int32_t lane_index) {                                          \
    DCHECK(0 <= lane_index && lane_index < lane_count);              \
    return zone_->New<Operator1<int32_t>>(                           \
        IrOpcode::k##Type##ExtractLane##Sign, Operator::kPure,       \
        "" #Type "ExtractLane" #Sign, 1, 0, 0, 1, 0, 0, lane_index); \
  }
EXTRACT_LANE_OP(F64x2, , 2)
EXTRACT_LANE_OP(F32x4, , 4)
EXTRACT_LANE_OP(I64x2, , 2)
EXTRACT_LANE_OP(I32x4, , 4)
EXTRACT_LANE_OP(I16x8, U, 8)
EXTRACT_LANE_OP(I16x8, S, 8)
EXTRACT_LANE_OP(I8x16, U, 16)
EXTRACT_LANE_OP(I8x16, S, 16)
#undef EXTRACT_LANE_OP

#define REPLACE_LANE_OP(Type, lane_count)                                     \
  const Operator* MachineOperatorBuilder::Type##ReplaceLane(                  \
      int32_t lane_index) {                                                   \
    DCHECK(0 <= lane_index && lane_index < lane_count);                       \
    return zone_->New<Operator1<int32_t>>(IrOpcode::k##Type##ReplaceLane,     \
                                          Operator::kPure, "Replace lane", 2, \
                                          0, 0, 1, 0, 0, lane_index);         \
  }
SIMD_LANE_OP_LIST(REPLACE_LANE_OP)
#undef REPLACE_LANE_OP

const Operator* MachineOperatorBuilder::I64x2ReplaceLaneI32Pair(
    int32_t lane_index) {
  DCHECK(0 <= lane_index && lane_index < 2);
  return zone_->New<Operator1<int32_t>>(IrOpcode::kI64x2ReplaceLaneI32Pair,
                                        Operator::kPure, "Replace lane", 3, 0,
                                        0, 1, 0, 0, lane_index);
}

bool operator==(S128ImmediateParameter const& lhs,
                S128ImmediateParameter const& rhs) {
  return (lhs.immediate() == rhs.immediate());
}

bool operator!=(S128ImmediateParameter const& lhs,
                S128ImmediateParameter const& rhs) {
  return !(lhs == rhs);
}

size_t hash_value(S128ImmediateParameter const& p) {
  return base::hash_range(p.immediate().begin(), p.immediate().end());
}

std::ostream& operator<<(std::ostream& os, S128ImmediateParameter const& p) {
  for (int i = 0; i < 16; i++) {
    const char* separator = (i < 15) ? "," : "";
    os << static_cast<uint32_t>(p[i]) << separator;
  }
  return os;
}

S128ImmediateParameter const& S128ImmediateParameterOf(Operator const* op) {
  DCHECK(IrOpcode::kI8x16Shuffle == op->opcode() ||
         IrOpcode::kS128Const == op->opcode());
  return OpParameter<S128ImmediateParameter>(op);
}

const Operator* MachineOperatorBuilder::S128Const(const uint8_t value[16]) {
  return zone_->New<Operator1<S128ImmediateParameter>>(
      IrOpcode::kS128Const, Operator::kPure, "Immediate", 0, 0, 0, 1, 0, 0,
      S128ImmediateParameter(value));
}

const Operator* MachineOperatorBuilder::I8x16Shuffle(
    const uint8_t shuffle[16]) {
  return zone_->New<Operator1<S128ImmediateParameter>>(
      IrOpcode::kI8x16Shuffle, Operator::kPure, "Shuffle", 2, 0, 0, 1, 0, 0,
      S128ImmediateParameter(shuffle));
}

StackCheckKind StackCheckKindOf(Operator const* op) {
  DCHECK_EQ(IrOpcode::kStackPointerGreaterThan, op->opcode());
  return OpParameter<StackCheckKind>(op);
}

#undef PURE_BINARY_OP_LIST_32
#undef PURE_BINARY_OP_LIST_64
#undef MACHINE_PURE_OP_LIST
#undef PURE_OPTIONAL_OP_LIST
#undef OVERFLOW_OP_LIST
#undef MACHINE_TYPE_LIST
#undef MACHINE_REPRESENTATION_LIST
#undef ATOMIC_TYPE_LIST
#undef ATOMIC_U64_TYPE_LIST
#undef ATOMIC_U32_TYPE_LIST
#undef ATOMIC_REPRESENTATION_LIST
#undef ATOMIC64_REPRESENTATION_LIST
#undef SIMD_LANE_OP_LIST
#undef STACK_SLOT_CACHED_SIZES_ALIGNMENTS_LIST
#undef LOAD_TRANSFORM_LIST

}  // namespace compiler
}  // namespace internal
}  // namespace v8
