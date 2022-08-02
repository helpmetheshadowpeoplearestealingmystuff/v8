// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_LOONG64_FRAME_CONSTANTS_LOONG64_H_
#define V8_EXECUTION_LOONG64_FRAME_CONSTANTS_LOONG64_H_

#include "src/base/bits.h"
#include "src/base/macros.h"
#include "src/codegen/register.h"
#include "src/execution/frame-constants.h"

namespace v8 {
namespace internal {

class EntryFrameConstants : public AllStatic {
 public:
  // This is the offset to where JSEntry pushes the current value of
  // Isolate::c_entry_fp onto the stack.
  static constexpr int kCallerFPOffset = -3 * kSystemPointerSize;
};

class WasmCompileLazyFrameConstants : public TypedFrameConstants {
 public:
  // Number of gp parameters, without the instance.
  static constexpr int kNumberOfSavedGpParamRegs = 6;
  static constexpr int kNumberOfSavedFpParamRegs = 8;
  static constexpr int kNumberOfSavedAllParamRegs = 14;

  // On loong64, spilled registers are implicitly sorted backwards by number.
  // We spill:
  //   a0: param0 = instance
  //   a2, a3, a4, a5, a6, a7: param1, param2, ..., param6
  // in the following FP-relative order: [f7, f6, f5, f4, f3, f2, f1, f0].
  static constexpr int kInstanceSpillOffset =
      TYPED_FRAME_PUSHED_VALUE_OFFSET(6);

  static constexpr int kParameterSpillsOffset[] = {
      TYPED_FRAME_PUSHED_VALUE_OFFSET(5), TYPED_FRAME_PUSHED_VALUE_OFFSET(4),
      TYPED_FRAME_PUSHED_VALUE_OFFSET(3), TYPED_FRAME_PUSHED_VALUE_OFFSET(2),
      TYPED_FRAME_PUSHED_VALUE_OFFSET(1), TYPED_FRAME_PUSHED_VALUE_OFFSET(0)};

  // SP-relative.
  static constexpr int kWasmInstanceOffset = 2 * kSystemPointerSize;
  static constexpr int kFunctionIndexOffset = 1 * kSystemPointerSize;
  static constexpr int kNativeModuleOffset = 0;
};

// Frame constructed by the {WasmDebugBreak} builtin.
// After pushing the frame type marker, the builtin pushes all Liftoff cache
// registers (see liftoff-assembler-defs.h).
class WasmDebugBreakFrameConstants : public TypedFrameConstants {
 public:
  // {a0 ... a7, t0 ... t5, s0, s1, s2, s5, s7, s8}
  static constexpr RegList kPushedGpRegs = {a0, a1, a2, a3, a4, a5, a6,
                                            a7, t0, t1, t2, t3, t4, t5,
                                            s0, s1, s2, s5, s7, s8};
  // {f0, f1, f2, ... f27, f28}
  static constexpr DoubleRegList kPushedFpRegs = {
      f0,  f1,  f2,  f3,  f4,  f5,  f6,  f7,  f8,  f9,  f10, f11, f12, f13, f14,
      f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28};

  static constexpr int kNumPushedGpRegisters = kPushedGpRegs.Count();
  static constexpr int kNumPushedFpRegisters = kPushedFpRegs.Count();

  static constexpr int kLastPushedGpRegisterOffset =
      -kFixedFrameSizeFromFp - kNumPushedGpRegisters * kSystemPointerSize;
  static constexpr int kLastPushedFpRegisterOffset =
      kLastPushedGpRegisterOffset - kNumPushedFpRegisters * kDoubleSize;

  // Offsets are fp-relative.
  static int GetPushedGpRegisterOffset(int reg_code) {
    DCHECK_NE(0, kPushedGpRegs.bits() & (1 << reg_code));
    uint32_t lower_regs =
        kPushedGpRegs.bits() & ((uint32_t{1} << reg_code) - 1);
    return kLastPushedGpRegisterOffset +
           base::bits::CountPopulation(lower_regs) * kSystemPointerSize;
  }

  static int GetPushedFpRegisterOffset(int reg_code) {
    DCHECK_NE(0, kPushedFpRegs.bits() & (1 << reg_code));
    uint32_t lower_regs =
        kPushedFpRegs.bits() & ((uint32_t{1} << reg_code) - 1);
    return kLastPushedFpRegisterOffset +
           base::bits::CountPopulation(lower_regs) * kDoubleSize;
  }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_LOONG64_FRAME_CONSTANTS_LOONG64_H_
