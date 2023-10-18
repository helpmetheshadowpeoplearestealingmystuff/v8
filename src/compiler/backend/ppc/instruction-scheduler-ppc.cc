// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/backend/instruction-scheduler.h"

namespace v8 {
namespace internal {
namespace compiler {

bool InstructionScheduler::SchedulerSupported() { return true; }

int InstructionScheduler::GetTargetInstructionFlags(
    const Instruction* instr) const {
  switch (instr->arch_opcode()) {
    case kPPC_And:
    case kPPC_AndComplement:
    case kPPC_Or:
    case kPPC_OrComplement:
    case kPPC_Xor:
    case kPPC_ShiftLeft32:
    case kPPC_ShiftLeft64:
    case kPPC_ShiftLeftPair:
    case kPPC_ShiftRight32:
    case kPPC_ShiftRight64:
    case kPPC_ShiftRightPair:
    case kPPC_ShiftRightAlg32:
    case kPPC_ShiftRightAlg64:
    case kPPC_ShiftRightAlgPair:
    case kPPC_RotRight32:
    case kPPC_RotRight64:
    case kPPC_Not:
    case kPPC_RotLeftAndMask32:
    case kPPC_RotLeftAndClear64:
    case kPPC_RotLeftAndClearLeft64:
    case kPPC_RotLeftAndClearRight64:
    case kPPC_Add32:
    case kPPC_Add64:
    case kPPC_AddWithOverflow32:
    case kPPC_AddPair:
    case kPPC_AddDouble:
    case kPPC_Sub:
    case kPPC_SubWithOverflow32:
    case kPPC_SubPair:
    case kPPC_SubDouble:
    case kPPC_Mul32:
    case kPPC_Mul32WithHigh32:
    case kPPC_Mul64:
    case kPPC_MulHighS64:
    case kPPC_MulHighU64:
    case kPPC_MulHigh32:
    case kPPC_MulHighU32:
    case kPPC_MulPair:
    case kPPC_MulDouble:
    case kPPC_Div32:
    case kPPC_Div64:
    case kPPC_DivU32:
    case kPPC_DivU64:
    case kPPC_DivDouble:
    case kPPC_Mod32:
    case kPPC_Mod64:
    case kPPC_ModU32:
    case kPPC_ModU64:
    case kPPC_ModDouble:
    case kPPC_Neg:
    case kPPC_NegDouble:
    case kPPC_SqrtDouble:
    case kPPC_FloorDouble:
    case kPPC_CeilDouble:
    case kPPC_TruncateDouble:
    case kPPC_RoundDouble:
    case kPPC_MaxDouble:
    case kPPC_MinDouble:
    case kPPC_AbsDouble:
    case kPPC_Cntlz32:
    case kPPC_Cntlz64:
    case kPPC_Popcnt32:
    case kPPC_Popcnt64:
    case kPPC_Cmp32:
    case kPPC_Cmp64:
    case kPPC_CmpDouble:
    case kPPC_Tst32:
    case kPPC_Tst64:
    case kPPC_ExtendSignWord8:
    case kPPC_ExtendSignWord16:
    case kPPC_ExtendSignWord32:
    case kPPC_Uint32ToUint64:
    case kPPC_Int64ToInt32:
    case kPPC_Int64ToFloat32:
    case kPPC_Int64ToDouble:
    case kPPC_Uint64ToFloat32:
    case kPPC_Uint64ToDouble:
    case kPPC_Int32ToFloat32:
    case kPPC_Int32ToDouble:
    case kPPC_Uint32ToFloat32:
    case kPPC_Uint32ToDouble:
    case kPPC_Float32ToInt32:
    case kPPC_Float32ToUint32:
    case kPPC_Float32ToDouble:
    case kPPC_Float64SilenceNaN:
    case kPPC_DoubleToInt32:
    case kPPC_DoubleToUint32:
    case kPPC_DoubleToInt64:
    case kPPC_DoubleToUint64:
    case kPPC_DoubleToFloat32:
    case kPPC_DoubleExtractLowWord32:
    case kPPC_DoubleExtractHighWord32:
    case kPPC_DoubleInsertLowWord32:
    case kPPC_DoubleInsertHighWord32:
    case kPPC_DoubleConstruct:
    case kPPC_BitcastInt32ToFloat32:
    case kPPC_BitcastFloat32ToInt32:
    case kPPC_BitcastInt64ToDouble:
    case kPPC_BitcastDoubleToInt64:
    case kPPC_ByteRev32:
    case kPPC_ByteRev64:
    case kPPC_F64x2Splat:
    case kPPC_F64x2Add:
    case kPPC_F64x2Sub:
    case kPPC_F64x2Mul:
    case kPPC_F64x2Eq:
    case kPPC_F64x2Ne:
    case kPPC_F64x2Le:
    case kPPC_F64x2Lt:
    case kPPC_F64x2Abs:
    case kPPC_F64x2Neg:
    case kPPC_F64x2Sqrt:
    case kPPC_F64x2Qfma:
    case kPPC_F64x2Qfms:
    case kPPC_F64x2Div:
    case kPPC_F64x2Min:
    case kPPC_F64x2Max:
    case kPPC_F64x2Ceil:
    case kPPC_F64x2Floor:
    case kPPC_F64x2Trunc:
    case kPPC_F64x2Pmin:
    case kPPC_F64x2Pmax:
    case kPPC_F64x2ConvertLowI32x4S:
    case kPPC_F64x2ConvertLowI32x4U:
    case kPPC_F64x2PromoteLowF32x4:
    case kPPC_F32x4Splat:
    case kPPC_F32x4Add:
    case kPPC_F32x4Sub:
    case kPPC_F32x4Mul:
    case kPPC_F32x4Eq:
    case kPPC_F32x4Ne:
    case kPPC_F32x4Lt:
    case kPPC_F32x4Le:
    case kPPC_F32x4Abs:
    case kPPC_F32x4Neg:
    case kPPC_F32x4Sqrt:
    case kPPC_F32x4SConvertI32x4:
    case kPPC_F32x4UConvertI32x4:
    case kPPC_F32x4Qfma:
    case kPPC_F32x4Qfms:
    case kPPC_F32x4Div:
    case kPPC_F32x4Min:
    case kPPC_F32x4Max:
    case kPPC_F32x4Ceil:
    case kPPC_F32x4Floor:
    case kPPC_F32x4Trunc:
    case kPPC_F32x4Pmin:
    case kPPC_F32x4Pmax:
    case kPPC_F32x4DemoteF64x2Zero:
    case kPPC_I64x2Splat:
    case kPPC_I64x2Add:
    case kPPC_I64x2Sub:
    case kPPC_I64x2Mul:
    case kPPC_I64x2Eq:
    case kPPC_I64x2Ne:
    case kPPC_I64x2GtS:
    case kPPC_I64x2GeS:
    case kPPC_I64x2Shl:
    case kPPC_I64x2ShrS:
    case kPPC_I64x2ShrU:
    case kPPC_I64x2Neg:
    case kPPC_I64x2BitMask:
    case kPPC_I64x2SConvertI32x4Low:
    case kPPC_I64x2SConvertI32x4High:
    case kPPC_I64x2UConvertI32x4Low:
    case kPPC_I64x2UConvertI32x4High:
    case kPPC_I64x2ExtMulLowI32x4S:
    case kPPC_I64x2ExtMulHighI32x4S:
    case kPPC_I64x2ExtMulLowI32x4U:
    case kPPC_I64x2ExtMulHighI32x4U:
    case kPPC_I64x2Abs:
    case kPPC_I32x4Splat:
    case kPPC_I32x4Add:
    case kPPC_I32x4Sub:
    case kPPC_I32x4Mul:
    case kPPC_I32x4MinS:
    case kPPC_I32x4MinU:
    case kPPC_I32x4MaxS:
    case kPPC_I32x4MaxU:
    case kPPC_I32x4Eq:
    case kPPC_I32x4Ne:
    case kPPC_I32x4GtS:
    case kPPC_I32x4GeS:
    case kPPC_I32x4GtU:
    case kPPC_I32x4GeU:
    case kPPC_I32x4Shl:
    case kPPC_I32x4ShrS:
    case kPPC_I32x4ShrU:
    case kPPC_I32x4Neg:
    case kPPC_I32x4Abs:
    case kPPC_I32x4SConvertF32x4:
    case kPPC_I32x4UConvertF32x4:
    case kPPC_I32x4SConvertI16x8Low:
    case kPPC_I32x4SConvertI16x8High:
    case kPPC_I32x4UConvertI16x8Low:
    case kPPC_I32x4UConvertI16x8High:
    case kPPC_I32x4BitMask:
    case kPPC_I32x4DotI16x8S:
    case kPPC_I32x4ExtAddPairwiseI16x8S:
    case kPPC_I32x4ExtAddPairwiseI16x8U:
    case kPPC_I32x4ExtMulLowI16x8S:
    case kPPC_I32x4ExtMulHighI16x8S:
    case kPPC_I32x4ExtMulLowI16x8U:
    case kPPC_I32x4ExtMulHighI16x8U:
    case kPPC_I32x4TruncSatF64x2SZero:
    case kPPC_I32x4TruncSatF64x2UZero:
    case kPPC_I32x4DotI8x16AddS:
    case kPPC_I16x8Splat:
    case kPPC_I16x8Add:
    case kPPC_I16x8Sub:
    case kPPC_I16x8Mul:
    case kPPC_I16x8MinS:
    case kPPC_I16x8MinU:
    case kPPC_I16x8MaxS:
    case kPPC_I16x8MaxU:
    case kPPC_I16x8Eq:
    case kPPC_I16x8Ne:
    case kPPC_I16x8GtS:
    case kPPC_I16x8GeS:
    case kPPC_I16x8GtU:
    case kPPC_I16x8GeU:
    case kPPC_I16x8Shl:
    case kPPC_I16x8ShrS:
    case kPPC_I16x8ShrU:
    case kPPC_I16x8Neg:
    case kPPC_I16x8Abs:
    case kPPC_I16x8SConvertI32x4:
    case kPPC_I16x8UConvertI32x4:
    case kPPC_I16x8SConvertI8x16Low:
    case kPPC_I16x8SConvertI8x16High:
    case kPPC_I16x8UConvertI8x16Low:
    case kPPC_I16x8UConvertI8x16High:
    case kPPC_I16x8AddSatS:
    case kPPC_I16x8SubSatS:
    case kPPC_I16x8AddSatU:
    case kPPC_I16x8SubSatU:
    case kPPC_I16x8RoundingAverageU:
    case kPPC_I16x8BitMask:
    case kPPC_I16x8ExtAddPairwiseI8x16S:
    case kPPC_I16x8ExtAddPairwiseI8x16U:
    case kPPC_I16x8Q15MulRSatS:
    case kPPC_I16x8ExtMulLowI8x16S:
    case kPPC_I16x8ExtMulHighI8x16S:
    case kPPC_I16x8ExtMulLowI8x16U:
    case kPPC_I16x8ExtMulHighI8x16U:
    case kPPC_I16x8DotI8x16S:
    case kPPC_I8x16Splat:
    case kPPC_I8x16Add:
    case kPPC_I8x16Sub:
    case kPPC_I8x16MinS:
    case kPPC_I8x16MinU:
    case kPPC_I8x16MaxS:
    case kPPC_I8x16MaxU:
    case kPPC_I8x16Eq:
    case kPPC_I8x16Ne:
    case kPPC_I8x16GtS:
    case kPPC_I8x16GeS:
    case kPPC_I8x16GtU:
    case kPPC_I8x16GeU:
    case kPPC_I8x16Shl:
    case kPPC_I8x16ShrS:
    case kPPC_I8x16ShrU:
    case kPPC_I8x16Neg:
    case kPPC_I8x16Abs:
    case kPPC_I8x16SConvertI16x8:
    case kPPC_I8x16UConvertI16x8:
    case kPPC_I8x16AddSatS:
    case kPPC_I8x16SubSatS:
    case kPPC_I8x16AddSatU:
    case kPPC_I8x16SubSatU:
    case kPPC_I8x16RoundingAverageU:
    case kPPC_I8x16Shuffle:
    case kPPC_I8x16Swizzle:
    case kPPC_I8x16BitMask:
    case kPPC_I8x16Popcnt:
    case kPPC_I64x2AllTrue:
    case kPPC_I32x4AllTrue:
    case kPPC_I16x8AllTrue:
    case kPPC_I8x16AllTrue:
    case kPPC_V128AnyTrue:
    case kPPC_S128And:
    case kPPC_S128Or:
    case kPPC_S128Xor:
    case kPPC_S128Const:
    case kPPC_S128Zero:
    case kPPC_S128AllOnes:
    case kPPC_S128Not:
    case kPPC_S128Select:
    case kPPC_S128AndNot:
    case kPPC_FExtractLane:
    case kPPC_IExtractLane:
    case kPPC_IExtractLaneU:
    case kPPC_IExtractLaneS:
    case kPPC_FReplaceLane:
    case kPPC_IReplaceLane:
    case kPPC_LoadReverseSimd128RR:
      return kNoOpcodeFlags;

    case kPPC_LoadWordS8:
    case kPPC_LoadWordU8:
    case kPPC_LoadWordS16:
    case kPPC_LoadWordU16:
    case kPPC_LoadWordS32:
    case kPPC_LoadWordU32:
    case kPPC_LoadByteRev32:
    case kPPC_LoadWord64:
    case kPPC_LoadByteRev64:
    case kPPC_LoadFloat32:
    case kPPC_LoadDouble:
    case kPPC_LoadSimd128:
    case kPPC_Peek:
    case kPPC_LoadDecompressTaggedSigned:
    case kPPC_LoadDecompressTagged:
    case kPPC_S128Load8Splat:
    case kPPC_S128Load16Splat:
    case kPPC_S128Load32Splat:
    case kPPC_S128Load64Splat:
    case kPPC_S128Load8x8S:
    case kPPC_S128Load8x8U:
    case kPPC_S128Load16x4S:
    case kPPC_S128Load16x4U:
    case kPPC_S128Load32x2S:
    case kPPC_S128Load32x2U:
    case kPPC_S128Load32Zero:
    case kPPC_S128Load64Zero:
    case kPPC_S128Load8Lane:
    case kPPC_S128Load16Lane:
    case kPPC_S128Load32Lane:
    case kPPC_S128Load64Lane:
      return kIsLoadOperation;

    case kPPC_StoreWord8:
    case kPPC_StoreWord16:
    case kPPC_StoreWord32:
    case kPPC_StoreByteRev32:
    case kPPC_StoreWord64:
    case kPPC_StoreByteRev64:
    case kPPC_StoreFloat32:
    case kPPC_StoreDouble:
    case kPPC_StoreSimd128:
    case kPPC_StoreCompressTagged:
    case kPPC_Push:
    case kPPC_PushFrame:
    case kPPC_StoreToStackSlot:
    case kPPC_Sync:
    case kPPC_S128Store8Lane:
    case kPPC_S128Store16Lane:
    case kPPC_S128Store32Lane:
    case kPPC_S128Store64Lane:
      return kHasSideEffect;

    case kPPC_AtomicExchangeUint8:
    case kPPC_AtomicExchangeUint16:
    case kPPC_AtomicExchangeWord32:
    case kPPC_AtomicExchangeWord64:
    case kPPC_AtomicCompareExchangeUint8:
    case kPPC_AtomicCompareExchangeUint16:
    case kPPC_AtomicCompareExchangeWord32:
    case kPPC_AtomicCompareExchangeWord64:
    case kPPC_AtomicAddUint8:
    case kPPC_AtomicAddUint16:
    case kPPC_AtomicAddUint32:
    case kPPC_AtomicAddUint64:
    case kPPC_AtomicAddInt8:
    case kPPC_AtomicAddInt16:
    case kPPC_AtomicAddInt32:
    case kPPC_AtomicAddInt64:
    case kPPC_AtomicSubUint8:
    case kPPC_AtomicSubUint16:
    case kPPC_AtomicSubUint32:
    case kPPC_AtomicSubUint64:
    case kPPC_AtomicSubInt8:
    case kPPC_AtomicSubInt16:
    case kPPC_AtomicSubInt32:
    case kPPC_AtomicSubInt64:
    case kPPC_AtomicAndUint8:
    case kPPC_AtomicAndUint16:
    case kPPC_AtomicAndUint32:
    case kPPC_AtomicAndUint64:
    case kPPC_AtomicAndInt8:
    case kPPC_AtomicAndInt16:
    case kPPC_AtomicAndInt32:
    case kPPC_AtomicAndInt64:
    case kPPC_AtomicOrUint8:
    case kPPC_AtomicOrUint16:
    case kPPC_AtomicOrUint32:
    case kPPC_AtomicOrUint64:
    case kPPC_AtomicOrInt8:
    case kPPC_AtomicOrInt16:
    case kPPC_AtomicOrInt32:
    case kPPC_AtomicOrInt64:
    case kPPC_AtomicXorUint8:
    case kPPC_AtomicXorUint16:
    case kPPC_AtomicXorUint32:
    case kPPC_AtomicXorUint64:
    case kPPC_AtomicXorInt8:
    case kPPC_AtomicXorInt16:
    case kPPC_AtomicXorInt32:
    case kPPC_AtomicXorInt64:
      return kHasSideEffect;

#define CASE(Name) case k##Name:
      COMMON_ARCH_OPCODE_LIST(CASE)
#undef CASE
      // Already covered in architecture independent code.
      UNREACHABLE();
  }

  UNREACHABLE();
}

int InstructionScheduler::GetInstructionLatency(const Instruction* instr) {
  // TODO(all): Add instruction cost modeling.
  return 1;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
