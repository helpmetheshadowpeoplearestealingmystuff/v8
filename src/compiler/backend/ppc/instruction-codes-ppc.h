// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_BACKEND_PPC_INSTRUCTION_CODES_PPC_H_
#define V8_COMPILER_BACKEND_PPC_INSTRUCTION_CODES_PPC_H_

namespace v8 {
namespace internal {
namespace compiler {

// PPC-specific opcodes that specify which assembly sequence to emit.
// Most opcodes specify a single instruction.
#define TARGET_ARCH_OPCODE_LIST(V)   \
  V(PPC_Peek)                        \
  V(PPC_Sync)                        \
  V(PPC_And)                         \
  V(PPC_AndComplement)               \
  V(PPC_Or)                          \
  V(PPC_OrComplement)                \
  V(PPC_Xor)                         \
  V(PPC_ShiftLeft32)                 \
  V(PPC_ShiftLeft64)                 \
  V(PPC_ShiftLeftPair)               \
  V(PPC_ShiftRight32)                \
  V(PPC_ShiftRight64)                \
  V(PPC_ShiftRightPair)              \
  V(PPC_ShiftRightAlg32)             \
  V(PPC_ShiftRightAlg64)             \
  V(PPC_ShiftRightAlgPair)           \
  V(PPC_RotRight32)                  \
  V(PPC_RotRight64)                  \
  V(PPC_Not)                         \
  V(PPC_RotLeftAndMask32)            \
  V(PPC_RotLeftAndClear64)           \
  V(PPC_RotLeftAndClearLeft64)       \
  V(PPC_RotLeftAndClearRight64)      \
  V(PPC_Add32)                       \
  V(PPC_Add64)                       \
  V(PPC_AddWithOverflow32)           \
  V(PPC_AddPair)                     \
  V(PPC_AddDouble)                   \
  V(PPC_Sub)                         \
  V(PPC_SubWithOverflow32)           \
  V(PPC_SubPair)                     \
  V(PPC_SubDouble)                   \
  V(PPC_Mul32)                       \
  V(PPC_Mul32WithHigh32)             \
  V(PPC_Mul64)                       \
  V(PPC_MulHigh32)                   \
  V(PPC_MulHighU32)                  \
  V(PPC_MulPair)                     \
  V(PPC_MulDouble)                   \
  V(PPC_Div32)                       \
  V(PPC_Div64)                       \
  V(PPC_DivU32)                      \
  V(PPC_DivU64)                      \
  V(PPC_DivDouble)                   \
  V(PPC_Mod32)                       \
  V(PPC_Mod64)                       \
  V(PPC_ModU32)                      \
  V(PPC_ModU64)                      \
  V(PPC_ModDouble)                   \
  V(PPC_Neg)                         \
  V(PPC_NegDouble)                   \
  V(PPC_SqrtDouble)                  \
  V(PPC_FloorDouble)                 \
  V(PPC_CeilDouble)                  \
  V(PPC_TruncateDouble)              \
  V(PPC_RoundDouble)                 \
  V(PPC_MaxDouble)                   \
  V(PPC_MinDouble)                   \
  V(PPC_AbsDouble)                   \
  V(PPC_Cntlz32)                     \
  V(PPC_Cntlz64)                     \
  V(PPC_Popcnt32)                    \
  V(PPC_Popcnt64)                    \
  V(PPC_Cmp32)                       \
  V(PPC_Cmp64)                       \
  V(PPC_CmpDouble)                   \
  V(PPC_Tst32)                       \
  V(PPC_Tst64)                       \
  V(PPC_Push)                        \
  V(PPC_PushFrame)                   \
  V(PPC_StoreToStackSlot)            \
  V(PPC_ExtendSignWord8)             \
  V(PPC_ExtendSignWord16)            \
  V(PPC_ExtendSignWord32)            \
  V(PPC_Uint32ToUint64)              \
  V(PPC_Int64ToInt32)                \
  V(PPC_Int64ToFloat32)              \
  V(PPC_Int64ToDouble)               \
  V(PPC_Uint64ToFloat32)             \
  V(PPC_Uint64ToDouble)              \
  V(PPC_Int32ToFloat32)              \
  V(PPC_Int32ToDouble)               \
  V(PPC_Uint32ToFloat32)             \
  V(PPC_Uint32ToDouble)              \
  V(PPC_Float32ToDouble)             \
  V(PPC_Float64SilenceNaN)           \
  V(PPC_DoubleToInt32)               \
  V(PPC_DoubleToUint32)              \
  V(PPC_DoubleToInt64)               \
  V(PPC_DoubleToUint64)              \
  V(PPC_DoubleToFloat32)             \
  V(PPC_DoubleExtractLowWord32)      \
  V(PPC_DoubleExtractHighWord32)     \
  V(PPC_DoubleInsertLowWord32)       \
  V(PPC_DoubleInsertHighWord32)      \
  V(PPC_DoubleConstruct)             \
  V(PPC_BitcastInt32ToFloat32)       \
  V(PPC_BitcastFloat32ToInt32)       \
  V(PPC_BitcastInt64ToDouble)        \
  V(PPC_BitcastDoubleToInt64)        \
  V(PPC_LoadWordS8)                  \
  V(PPC_LoadWordU8)                  \
  V(PPC_LoadWordS16)                 \
  V(PPC_LoadWordU16)                 \
  V(PPC_LoadWordS32)                 \
  V(PPC_LoadWordU32)                 \
  V(PPC_LoadWord64)                  \
  V(PPC_LoadFloat32)                 \
  V(PPC_LoadDouble)                  \
  V(PPC_LoadSimd128)                 \
  V(PPC_StoreWord8)                  \
  V(PPC_StoreWord16)                 \
  V(PPC_StoreWord32)                 \
  V(PPC_StoreWord64)                 \
  V(PPC_StoreFloat32)                \
  V(PPC_StoreDouble)                 \
  V(PPC_StoreSimd128)                \
  V(PPC_ByteRev32)                   \
  V(PPC_ByteRev64)                   \
  V(PPC_CompressSigned)              \
  V(PPC_CompressPointer)             \
  V(PPC_CompressAny)                 \
  V(PPC_AtomicStoreUint8)            \
  V(PPC_AtomicStoreUint16)           \
  V(PPC_AtomicStoreWord32)           \
  V(PPC_AtomicStoreWord64)           \
  V(PPC_AtomicLoadUint8)             \
  V(PPC_AtomicLoadUint16)            \
  V(PPC_AtomicLoadWord32)            \
  V(PPC_AtomicLoadWord64)            \
  V(PPC_AtomicExchangeUint8)         \
  V(PPC_AtomicExchangeUint16)        \
  V(PPC_AtomicExchangeWord32)        \
  V(PPC_AtomicExchangeWord64)        \
  V(PPC_AtomicCompareExchangeUint8)  \
  V(PPC_AtomicCompareExchangeUint16) \
  V(PPC_AtomicCompareExchangeWord32) \
  V(PPC_AtomicCompareExchangeWord64) \
  V(PPC_AtomicAddUint8)              \
  V(PPC_AtomicAddUint16)             \
  V(PPC_AtomicAddUint32)             \
  V(PPC_AtomicAddUint64)             \
  V(PPC_AtomicAddInt8)               \
  V(PPC_AtomicAddInt16)              \
  V(PPC_AtomicAddInt32)              \
  V(PPC_AtomicAddInt64)              \
  V(PPC_AtomicSubUint8)              \
  V(PPC_AtomicSubUint16)             \
  V(PPC_AtomicSubUint32)             \
  V(PPC_AtomicSubUint64)             \
  V(PPC_AtomicSubInt8)               \
  V(PPC_AtomicSubInt16)              \
  V(PPC_AtomicSubInt32)              \
  V(PPC_AtomicSubInt64)              \
  V(PPC_AtomicAndUint8)              \
  V(PPC_AtomicAndUint16)             \
  V(PPC_AtomicAndUint32)             \
  V(PPC_AtomicAndUint64)             \
  V(PPC_AtomicAndInt8)               \
  V(PPC_AtomicAndInt16)              \
  V(PPC_AtomicAndInt32)              \
  V(PPC_AtomicAndInt64)              \
  V(PPC_AtomicOrUint8)               \
  V(PPC_AtomicOrUint16)              \
  V(PPC_AtomicOrUint32)              \
  V(PPC_AtomicOrUint64)              \
  V(PPC_AtomicOrInt8)                \
  V(PPC_AtomicOrInt16)               \
  V(PPC_AtomicOrInt32)               \
  V(PPC_AtomicOrInt64)               \
  V(PPC_AtomicXorUint8)              \
  V(PPC_AtomicXorUint16)             \
  V(PPC_AtomicXorUint32)             \
  V(PPC_AtomicXorUint64)             \
  V(PPC_AtomicXorInt8)               \
  V(PPC_AtomicXorInt16)              \
  V(PPC_AtomicXorInt32)              \
  V(PPC_AtomicXorInt64)              \
  V(PPC_F64x2Splat)                  \
  V(PPC_F64x2ExtractLane)            \
  V(PPC_F64x2ReplaceLane)            \
  V(PPC_F64x2Add)                    \
  V(PPC_F64x2Sub)                    \
  V(PPC_F64x2Mul)                    \
  V(PPC_F64x2Eq)                     \
  V(PPC_F64x2Ne)                     \
  V(PPC_F64x2Le)                     \
  V(PPC_F64x2Lt)                     \
  V(PPC_F64x2Abs)                    \
  V(PPC_F64x2Neg)                    \
  V(PPC_F64x2Sqrt)                   \
  V(PPC_F32x4Splat)                  \
  V(PPC_F32x4ExtractLane)            \
  V(PPC_F32x4ReplaceLane)            \
  V(PPC_F32x4Add)                    \
  V(PPC_F32x4AddHoriz)               \
  V(PPC_F32x4Sub)                    \
  V(PPC_F32x4Mul)                    \
  V(PPC_F32x4Eq)                     \
  V(PPC_F32x4Ne)                     \
  V(PPC_F32x4Lt)                     \
  V(PPC_F32x4Le)                     \
  V(PPC_F32x4Abs)                    \
  V(PPC_F32x4Neg)                    \
  V(PPC_F32x4RecipApprox)            \
  V(PPC_F32x4RecipSqrtApprox)        \
  V(PPC_F32x4Sqrt)                   \
  V(PPC_F32x4SConvertI32x4)          \
  V(PPC_F32x4UConvertI32x4)          \
  V(PPC_I64x2Splat)                  \
  V(PPC_I64x2ExtractLane)            \
  V(PPC_I64x2ReplaceLane)            \
  V(PPC_I64x2Add)                    \
  V(PPC_I64x2Sub)                    \
  V(PPC_I64x2Mul)                    \
  V(PPC_I64x2MinS)                   \
  V(PPC_I64x2MinU)                   \
  V(PPC_I64x2MaxS)                   \
  V(PPC_I64x2MaxU)                   \
  V(PPC_I64x2Eq)                     \
  V(PPC_I64x2Ne)                     \
  V(PPC_I64x2GtS)                    \
  V(PPC_I64x2GtU)                    \
  V(PPC_I64x2GeU)                    \
  V(PPC_I64x2GeS)                    \
  V(PPC_I64x2Shl)                    \
  V(PPC_I64x2ShrS)                   \
  V(PPC_I64x2ShrU)                   \
  V(PPC_I64x2Neg)                    \
  V(PPC_I32x4Splat)                  \
  V(PPC_I32x4ExtractLane)            \
  V(PPC_I32x4ReplaceLane)            \
  V(PPC_I32x4Add)                    \
  V(PPC_I32x4AddHoriz)               \
  V(PPC_I32x4Sub)                    \
  V(PPC_I32x4Mul)                    \
  V(PPC_I32x4MinS)                   \
  V(PPC_I32x4MinU)                   \
  V(PPC_I32x4MaxS)                   \
  V(PPC_I32x4MaxU)                   \
  V(PPC_I32x4Eq)                     \
  V(PPC_I32x4Ne)                     \
  V(PPC_I32x4GtS)                    \
  V(PPC_I32x4GeS)                    \
  V(PPC_I32x4GtU)                    \
  V(PPC_I32x4GeU)                    \
  V(PPC_I32x4Shl)                    \
  V(PPC_I32x4ShrS)                   \
  V(PPC_I32x4ShrU)                   \
  V(PPC_I32x4Neg)                    \
  V(PPC_I32x4Abs)                    \
  V(PPC_I32x4SConvertF32x4)          \
  V(PPC_I32x4UConvertF32x4)          \
  V(PPC_I32x4SConvertI16x8Low)       \
  V(PPC_I32x4SConvertI16x8High)      \
  V(PPC_I32x4UConvertI16x8Low)       \
  V(PPC_I32x4UConvertI16x8High)      \
  V(PPC_I16x8Splat)                  \
  V(PPC_I16x8ExtractLaneU)           \
  V(PPC_I16x8ExtractLaneS)           \
  V(PPC_I16x8ReplaceLane)            \
  V(PPC_I16x8Add)                    \
  V(PPC_I16x8AddHoriz)               \
  V(PPC_I16x8Sub)                    \
  V(PPC_I16x8Mul)                    \
  V(PPC_I16x8MinS)                   \
  V(PPC_I16x8MinU)                   \
  V(PPC_I16x8MaxS)                   \
  V(PPC_I16x8MaxU)                   \
  V(PPC_I16x8Eq)                     \
  V(PPC_I16x8Ne)                     \
  V(PPC_I16x8GtS)                    \
  V(PPC_I16x8GeS)                    \
  V(PPC_I16x8GtU)                    \
  V(PPC_I16x8GeU)                    \
  V(PPC_I16x8Shl)                    \
  V(PPC_I16x8ShrS)                   \
  V(PPC_I16x8ShrU)                   \
  V(PPC_I16x8Neg)                    \
  V(PPC_I16x8Abs)                    \
  V(PPC_I16x8SConvertI32x4)          \
  V(PPC_I16x8UConvertI32x4)          \
  V(PPC_I16x8SConvertI8x16Low)       \
  V(PPC_I16x8SConvertI8x16High)      \
  V(PPC_I16x8UConvertI8x16Low)       \
  V(PPC_I16x8UConvertI8x16High)      \
  V(PPC_I8x16Splat)                  \
  V(PPC_I8x16ExtractLaneU)           \
  V(PPC_I8x16ExtractLaneS)           \
  V(PPC_I8x16ReplaceLane)            \
  V(PPC_I8x16Add)                    \
  V(PPC_I8x16Sub)                    \
  V(PPC_I8x16Mul)                    \
  V(PPC_I8x16MinS)                   \
  V(PPC_I8x16MinU)                   \
  V(PPC_I8x16MaxS)                   \
  V(PPC_I8x16MaxU)                   \
  V(PPC_I8x16Eq)                     \
  V(PPC_I8x16Ne)                     \
  V(PPC_I8x16GtS)                    \
  V(PPC_I8x16GeS)                    \
  V(PPC_I8x16GtU)                    \
  V(PPC_I8x16GeU)                    \
  V(PPC_I8x16Shl)                    \
  V(PPC_I8x16ShrS)                   \
  V(PPC_I8x16ShrU)                   \
  V(PPC_I8x16Neg)                    \
  V(PPC_I8x16Abs)                    \
  V(PPC_I8x16SConvertI16x8)          \
  V(PPC_I8x16UConvertI16x8)          \
  V(PPC_S8x16Shuffle)                \
  V(PPC_V64x2AnyTrue)                \
  V(PPC_V32x4AnyTrue)                \
  V(PPC_V16x8AnyTrue)                \
  V(PPC_V8x16AnyTrue)                \
  V(PPC_V64x2AllTrue)                \
  V(PPC_V32x4AllTrue)                \
  V(PPC_V16x8AllTrue)                \
  V(PPC_V8x16AllTrue)                \
  V(PPC_S128And)                     \
  V(PPC_S128Or)                      \
  V(PPC_S128Xor)                     \
  V(PPC_S128Zero)                    \
  V(PPC_S128Not)                     \
  V(PPC_S128Select)                  \
  V(PPC_StoreCompressTagged)         \
  V(PPC_LoadDecompressTaggedSigned)  \
  V(PPC_LoadDecompressTaggedPointer) \
  V(PPC_LoadDecompressAnyTagged)

// Addressing modes represent the "shape" of inputs to an instruction.
// Many instructions support multiple addressing modes. Addressing modes
// are encoded into the InstructionCode of the instruction and tell the
// code generator after register allocation which assembler method to call.
//
// We use the following local notation for addressing modes:
//
// R = register
// O = register or stack slot
// D = double register
// I = immediate (handle, external, int32)
// MRI = [register + immediate]
// MRR = [register + register]
#define TARGET_ADDRESSING_MODE_LIST(V) \
  V(MRI) /* [%r0 + K] */               \
  V(MRR) /* [%r0 + %r1] */

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_BACKEND_PPC_INSTRUCTION_CODES_PPC_H_
