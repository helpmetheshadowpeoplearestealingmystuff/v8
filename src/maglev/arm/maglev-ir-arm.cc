// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/logging.h"
#include "src/codegen/arm/assembler-arm.h"
#include "src/codegen/arm/register-arm.h"
#include "src/maglev/arm/maglev-assembler-arm-inl.h"
#include "src/maglev/maglev-assembler-inl.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-ir-inl.h"
#include "src/maglev/maglev-ir.h"

namespace v8 {
namespace internal {
namespace maglev {

#define __ masm->

#define MAGLEV_NODE_NOT_IMPLEMENTED(Node)                    \
  do {                                                       \
    PrintF("Maglev: Node not yet implemented'" #Node "'\n"); \
    masm->set_failed(true);                                  \
  } while (false)

void Int32NegateWithOverflow::SetValueLocationConstraints() {
  UseRegister(value_input());
  DefineAsRegister(this);
}

void Int32NegateWithOverflow::GenerateCode(MaglevAssembler* masm,
                                           const ProcessingState& state) {
  Register value = ToRegister(value_input());
  Register out = ToRegister(result());

  // Deopt when result would be -0.
  __ cmp(value, Operand(0));
  __ EmitEagerDeoptIf(eq, DeoptimizeReason::kOverflow, this);

  __ rsb(out, value, Operand(0), SetCC);
  // Output register must not be a register input into the eager deopt info.
  DCHECK_REGLIST_EMPTY(RegList{out} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(vs, DeoptimizeReason::kOverflow, this);
}

void Int32IncrementWithOverflow::SetValueLocationConstraints() {
  UseRegister(value_input());
  DefineAsRegister(this);
}

void Int32IncrementWithOverflow::GenerateCode(MaglevAssembler* masm,
                                              const ProcessingState& state) {
  Register value = ToRegister(value_input());
  Register out = ToRegister(result());
  __ add(out, value, Operand(1), SetCC);
  // Output register must not be a register input into the eager deopt info.
  DCHECK_REGLIST_EMPTY(RegList{out} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(vs, DeoptimizeReason::kOverflow, this);
}

void Int32DecrementWithOverflow::SetValueLocationConstraints() {
  UseRegister(value_input());
  DefineAsRegister(this);
}

void Int32DecrementWithOverflow::GenerateCode(MaglevAssembler* masm,
                                              const ProcessingState& state) {
  Register value = ToRegister(value_input());
  Register out = ToRegister(result());
  __ sub(out, value, Operand(1), SetCC);
  // Output register must not be a register input into the eager deopt info.
  DCHECK_REGLIST_EMPTY(RegList{out} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(vs, DeoptimizeReason::kOverflow, this);
}

int BuiltinStringFromCharCode::MaxCallStackArgs() const {
  return AllocateDescriptor::GetStackParameterCount();
}
void BuiltinStringFromCharCode::SetValueLocationConstraints() {
  if (code_input().node()->Is<Int32Constant>()) {
    UseAny(code_input());
  } else {
    UseRegister(code_input());
  }
  set_temporaries_needed(2);
  DefineAsRegister(this);
}
void BuiltinStringFromCharCode::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();
  Register result_string = ToRegister(result());
  if (Int32Constant* constant = code_input().node()->TryCast<Int32Constant>()) {
    int32_t char_code = constant->value();
    if (0 <= char_code && char_code < String::kMaxOneByteCharCode) {
      __ LoadSingleCharacterString(result_string, char_code);
    } else {
      // Ensure that {result_string} never aliases {scratch}, otherwise the
      // store will fail.
      bool reallocate_result = (scratch == result_string);
      if (reallocate_result) {
        result_string = temps.Acquire();
      }
      DCHECK(scratch != result_string);
      __ AllocateTwoByteString(register_snapshot(), result_string, 1);
      __ Move(scratch, char_code & 0xFFFF);
      __ strh(scratch,
              FieldMemOperand(result_string, SeqTwoByteString::kHeaderSize));
      if (reallocate_result) {
        __ Move(ToRegister(result()), result_string);
      }
    }
  } else {
    __ StringFromCharCode(register_snapshot(), nullptr, result_string,
                          ToRegister(code_input()), scratch);
  }
}

int BuiltinStringPrototypeCharCodeOrCodePointAt::MaxCallStackArgs() const {
  DCHECK_EQ(Runtime::FunctionForId(Runtime::kStringCharCodeAt)->nargs, 2);
  return 2;
}
void BuiltinStringPrototypeCharCodeOrCodePointAt::
    SetValueLocationConstraints() {
  UseAndClobberRegister(string_input());
  UseAndClobberRegister(index_input());
  DefineAsRegister(this);
}
void BuiltinStringPrototypeCharCodeOrCodePointAt::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
  Label done;
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();
  RegisterSnapshot save_registers = register_snapshot();
  __ StringCharCodeOrCodePointAt(mode_, save_registers, ToRegister(result()),
                                 ToRegister(string_input()),
                                 ToRegister(index_input()), scratch, &done);
  __ bind(&done);
}

void FoldedAllocation::SetValueLocationConstraints() {
  UseRegister(raw_allocation());
  DefineAsRegister(this);
}

void FoldedAllocation::GenerateCode(MaglevAssembler* masm,
                                    const ProcessingState& state) {
  __ add(ToRegister(result()), ToRegister(raw_allocation()), Operand(offset()));
}

int CheckedObjectToIndex::MaxCallStackArgs() const { return 0; }

void Int32AddWithOverflow::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(this);
}

void Int32AddWithOverflow::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  Register left = ToRegister(left_input());
  Register right = ToRegister(right_input());
  Register out = ToRegister(result());
  __ add(out, left, right, SetCC);
  // The output register shouldn't be a register input into the eager deopt
  // info.
  DCHECK_REGLIST_EMPTY(RegList{out} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(vs, DeoptimizeReason::kOverflow, this);
}

void Int32SubtractWithOverflow::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(this);
}
void Int32SubtractWithOverflow::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  Register left = ToRegister(left_input());
  Register right = ToRegister(right_input());
  Register out = ToRegister(result());
  __ sub(out, left, right, SetCC);
  // The output register shouldn't be a register input into the eager deopt
  // info.
  DCHECK_REGLIST_EMPTY(RegList{out} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(vs, DeoptimizeReason::kOverflow, this);
}

void Int32MultiplyWithOverflow::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(this);
  set_temporaries_needed(1);
}
void Int32MultiplyWithOverflow::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  Register left = ToRegister(left_input());
  Register right = ToRegister(right_input());
  Register out = ToRegister(result());

  // TODO(leszeks): peephole optimise multiplication by a constant.

  MaglevAssembler::ScratchRegisterScope temps(masm);
  bool out_alias_input = out == left || out == right;
  Register res_low = out;
  if (out_alias_input) {
    res_low = temps.Acquire();
  }
  Register res_high = temps.Acquire();
  __ smull(res_low, res_high, left, right);

  // ARM doesn't set the overflow flag for multiplication, so we need to
  // test on kNotEqual.
  __ cmp(res_high, Operand(res_low, ASR, 31));
  __ EmitEagerDeoptIf(ne, DeoptimizeReason::kOverflow, this);

  // If the result is zero, check if either lhs or rhs is negative.
  Label end;
  __ tst(res_low, res_low);
  __ b(ne, &end);
  Register temp = res_high;
  __ orr(temp, left, right, SetCC);
  // If one of them is negative, we must have a -0 result, which is non-int32,
  // so deopt.
  __ EmitEagerDeoptIf(mi, DeoptimizeReason::kOverflow, this);

  __ bind(&end);
  if (out_alias_input) {
    __ Move(out, res_low);
  }
}

void Int32DivideWithOverflow::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(this);
  if (!CpuFeatures::IsSupported(SUDIV)) {
    // We use the standard low double register and an extra one.
    set_double_temporaries_needed(1);
  }
}
void Int32DivideWithOverflow::GenerateCode(MaglevAssembler* masm,
                                           const ProcessingState& state) {
  Register left = ToRegister(left_input());
  Register right = ToRegister(right_input());
  Register out = ToRegister(result());

  // TODO(leszeks): peephole optimise division by a constant.

  // Pre-check for overflow, since idiv throws a division exception on overflow
  // rather than setting the overflow flag. Logic copied from
  // effect-control-linearizer.cc

  // Check if {right} is positive (and not zero).
  __ cmp(right, Operand(0));
  ZoneLabelRef done(masm);
  __ JumpToDeferredIf(
      le,
      [](MaglevAssembler* masm, ZoneLabelRef done, Register left,
         Register right, Int32DivideWithOverflow* node) {
        // {right} is negative or zero.

        // TODO(leszeks): Using kNotInt32 here, but in same places
        // kDivisionByZerokMinusZero/kMinusZero/kOverflow would be better. Right
        // now all eager deopts in a node have to be the same -- we should allow
        // a node to emit multiple eager deopts with different reasons.
        Label* deopt = __ GetDeoptLabel(node, DeoptimizeReason::kNotInt32);

        // Check if {right} is zero.
        // We've already done the compare and flags won't be cleared yet.
        __ JumpIf(eq, deopt);

        // Check if {left} is zero, as that would produce minus zero.
        __ tst(left, left);
        __ JumpIf(eq, deopt);

        // Check if {left} is kMinInt and {right} is -1, in which case we'd have
        // to return -kMinInt, which is not representable as Int32.
        __ cmp(left, Operand(kMinInt));
        __ JumpIf(ne, *done);
        __ cmp(right, Operand(-1));
        __ JumpIf(ne, *done);
        __ Jump(deopt);
      },
      done, left, right, this);
  __ bind(*done);

  // Perform the actual integer division.
  MaglevAssembler::ScratchRegisterScope temps(masm);
  bool out_alias_input = out == left || out == right;
  Register res = out;
  if (out_alias_input) {
    res = temps.Acquire();
  }
  if (CpuFeatures::IsSupported(SUDIV)) {
    CpuFeatureScope scope(masm, SUDIV);
    __ sdiv(res, left, right);
  } else {
    UseScratchRegisterScope temps(masm);
    LowDwVfpRegister double_right = temps.AcquireLowD();
    SwVfpRegister tmp = double_right.low();
    DwVfpRegister double_left = temps.AcquireD();
    DwVfpRegister double_res = double_left;
    __ vmov(tmp, left);
    __ vcvt_f64_s32(double_left, tmp);
    __ vmov(tmp, right);
    __ vcvt_f64_s32(double_right, tmp);
    __ vdiv(double_res, double_left, double_right);
    __ vcvt_s32_f64(tmp, double_res);
    __ vmov(res, tmp);
  }

  // Check that the remainder is zero.
  Register temp = temps.Acquire();
  __ mul(temp, res, right);
  __ tst(temp, left);
  __ EmitEagerDeoptIf(ne, DeoptimizeReason::kNotInt32, this);

  __ Move(out, res);
}

namespace {
void Uint32Mod(MaglevAssembler* masm, Register out, Register left,
               Register right) {
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register res = temps.Acquire();
  if (CpuFeatures::IsSupported(SUDIV)) {
    CpuFeatureScope scope(masm, SUDIV);
    __ udiv(res, left, right);
  } else {
    UseScratchRegisterScope temps(masm);
    LowDwVfpRegister double_right = temps.AcquireLowD();
    SwVfpRegister tmp = double_right.low();
    DwVfpRegister double_left = temps.AcquireD();
    DwVfpRegister double_res = double_left;
    __ vmov(tmp, left);
    __ vcvt_f64_s32(double_left, tmp);
    __ vmov(tmp, right);
    __ vcvt_f64_s32(double_right, tmp);
    __ vdiv(double_res, double_left, double_right);
    __ vcvt_s32_f64(tmp, double_res);
    __ vmov(res, tmp);
  }
  if (CpuFeatures::IsSupported(ARMv7)) {
    __ mls(out, res, right, left);
  } else {
    __ mul(res, res, right);
    __ sub(out, left, res);
  }
}
}  // namespace

void Int32ModulusWithOverflow::SetValueLocationConstraints() {
  UseAndClobberRegister(left_input());
  UseAndClobberRegister(right_input());
  DefineAsRegister(this);
  if (!CpuFeatures::IsSupported(SUDIV)) {
    // We use the standard low double register and an extra one.
    set_double_temporaries_needed(1);
  }
}
void Int32ModulusWithOverflow::GenerateCode(MaglevAssembler* masm,
                                            const ProcessingState& state) {
  // If AreAliased(lhs, rhs):
  //   deopt if lhs < 0  // Minus zero.
  //   0
  //
  // Using same algorithm as in EffectControlLinearizer:
  //   if rhs <= 0 then
  //     rhs = -rhs
  //     deopt if rhs == 0
  //   if lhs < 0 then
  //     let lhs_abs = -lsh in
  //     let res = lhs_abs % rhs in
  //     deopt if res == 0
  //     -res
  //   else
  //     let msk = rhs - 1 in
  //     if rhs & msk == 0 then
  //       lhs & msk
  //     else
  //       lhs % rhs

  Register lhs = ToRegister(left_input());
  Register rhs = ToRegister(right_input());
  Register out = ToRegister(result());

  static constexpr DeoptimizeReason deopt_reason =
      DeoptimizeReason::kDivisionByZero;

  if (lhs == rhs) {
    // For the modulus algorithm described above, lhs and rhs must not alias
    // each other.
    __ tst(lhs, lhs);
    // TODO(victorgomes): This ideally should be kMinusZero, but Maglev only
    // allows one deopt reason per IR.
    __ EmitEagerDeoptIf(mi, deopt_reason, this);
    __ Move(ToRegister(result()), 0);
    return;
  }

  DCHECK_NE(lhs, rhs);

  ZoneLabelRef done(masm);
  ZoneLabelRef rhs_checked(masm);
  __ cmp(rhs, Operand(0));
  __ JumpToDeferredIf(
      le,
      [](MaglevAssembler* masm, ZoneLabelRef rhs_checked, Register rhs,
         Int32ModulusWithOverflow* node) {
        __ rsb(rhs, rhs, Operand(0), SetCC);
        __ b(ne, *rhs_checked);
        __ EmitEagerDeopt(node, deopt_reason);
      },
      rhs_checked, rhs, this);
  __ bind(*rhs_checked);

  __ cmp(lhs, Operand(0));
  __ JumpToDeferredIf(
      lt,
      [](MaglevAssembler* masm, ZoneLabelRef done, Register lhs, Register rhs,
         Register out, Int32ModulusWithOverflow* node) {
        __ rsb(lhs, lhs, Operand(0));
        Uint32Mod(masm, out, lhs, rhs);
        __ rsb(out, out, Operand(0), SetCC);
        // TODO(victorgomes): This ideally should be kMinusZero, but Maglev
        // only allows one deopt reason per IR.
        __ b(ne, *done);
        __ EmitEagerDeopt(node, deopt_reason);
      },
      done, lhs, rhs, out, this);

  Label rhs_not_power_of_2;
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register mask = temps.Acquire();
  __ add(mask, rhs, Operand(-1));
  __ tst(mask, rhs);
  __ JumpIf(ne, &rhs_not_power_of_2);
  __ DebugBreak();

  // {rhs} is power of 2.
  __ and_(out, mask, lhs);
  __ Jump(*done);
  // {mask} can be reused from now on.
  temps.Include(mask);

  __ bind(&rhs_not_power_of_2);
  Uint32Mod(masm, out, lhs, rhs);
  __ bind(*done);
}

#define DEF_BITWISE_BINOP(Instruction, opcode)                   \
  void Instruction::SetValueLocationConstraints() {              \
    UseRegister(left_input());                                   \
    UseRegister(right_input());                                  \
    DefineAsRegister(this);                                      \
  }                                                              \
                                                                 \
  void Instruction::GenerateCode(MaglevAssembler* masm,          \
                                 const ProcessingState& state) { \
    Register left = ToRegister(left_input());                    \
    Register right = ToRegister(right_input());                  \
    Register out = ToRegister(result());                         \
    __ opcode(out, left, right);                                 \
  }
DEF_BITWISE_BINOP(Int32BitwiseAnd, and_)
DEF_BITWISE_BINOP(Int32BitwiseOr, orr)
DEF_BITWISE_BINOP(Int32BitwiseXor, eor)
#undef DEF_BITWISE_BINOP

#define DEF_SHIFT_BINOP(Instruction, opcode)                             \
  void Instruction::SetValueLocationConstraints() {                      \
    UseRegister(left_input());                                           \
    if (right_input().node()->Is<Int32Constant>()) {                     \
      UseAny(right_input());                                             \
    } else {                                                             \
      UseRegister(right_input());                                        \
    }                                                                    \
    DefineAsRegister(this);                                              \
  }                                                                      \
  void Instruction::GenerateCode(MaglevAssembler* masm,                  \
                                 const ProcessingState& state) {         \
    Register left = ToRegister(left_input());                            \
    Register out = ToRegister(result());                                 \
    if (Int32Constant* constant =                                        \
            right_input().node()->TryCast<Int32Constant>()) {            \
      __ opcode(out, left,                                               \
                Operand(static_cast<uint32_t>(constant->value()) & 31)); \
    } else {                                                             \
      MaglevAssembler::ScratchRegisterScope temps(masm);                 \
      Register scratch = temps.Acquire();                                \
      Register right = ToRegister(right_input());                        \
      __ and_(scratch, right, Operand(31));                              \
      __ opcode(out, left, Operand(scratch));                            \
    }                                                                    \
  }
DEF_SHIFT_BINOP(Int32ShiftLeft, lsl)
DEF_SHIFT_BINOP(Int32ShiftRight, asr)
DEF_SHIFT_BINOP(Int32ShiftRightLogical, lsr)
#undef DEF_SHIFT_BINOP

void Int32BitwiseNot::SetValueLocationConstraints() {
  UseRegister(value_input());
  DefineAsRegister(this);
}

void Int32BitwiseNot::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  Register value = ToRegister(value_input());
  Register out = ToRegister(result());
  __ mvn(out, Operand(value));
}

void Float64Add::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(this);
}

void Float64Add::GenerateCode(MaglevAssembler* masm,
                              const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(left_input());
  DoubleRegister right = ToDoubleRegister(right_input());
  DoubleRegister out = ToDoubleRegister(result());
  __ vadd(out, left, right);
}

void Float64Subtract::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(this);
}

void Float64Subtract::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(left_input());
  DoubleRegister right = ToDoubleRegister(right_input());
  DoubleRegister out = ToDoubleRegister(result());
  __ vsub(out, left, right);
}

void Float64Multiply::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(this);
}

void Float64Multiply::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(left_input());
  DoubleRegister right = ToDoubleRegister(right_input());
  DoubleRegister out = ToDoubleRegister(result());
  __ vmul(out, left, right);
}

void Float64Divide::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(this);
}

void Float64Divide::GenerateCode(MaglevAssembler* masm,
                                 const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(left_input());
  DoubleRegister right = ToDoubleRegister(right_input());
  DoubleRegister out = ToDoubleRegister(result());
  __ vdiv(out, left, right);
}

void Float64Modulus::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(this);
}
void Float64Modulus::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
  FrameScope scope(masm, StackFrame::MANUAL);
  __ PrepareCallCFunction(0, 2);
  __ MovToFloatParameters(ToDoubleRegister(left_input()),
                          ToDoubleRegister(right_input()));
  __ CallCFunction(ExternalReference::mod_two_doubles_operation(), 0, 2);
  // Move the result in the double result register.
  __ MovFromFloatResult(ToDoubleRegister(result()));
}

void Float64Negate::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void Float64Negate::GenerateCode(MaglevAssembler* masm,
                                 const ProcessingState& state) {
  DoubleRegister value = ToDoubleRegister(input());
  DoubleRegister out = ToDoubleRegister(result());
  __ vneg(out, value);
}

void Float64Round::GenerateCode(MaglevAssembler* masm,
                                const ProcessingState& state) {
  DoubleRegister in = ToDoubleRegister(input());
  DoubleRegister out = ToDoubleRegister(result());
  CpuFeatureScope scope(masm, ARMv8);
  if (kind_ == Kind::kNearest) {
    MaglevAssembler::ScratchRegisterScope temps(masm);
    DoubleRegister temp = temps.AcquireDouble();
    DoubleRegister half_one = temps.AcquireDouble();
    __ Move(temp, in);
    // vrintn rounds to even on tie, while JS expects it to round towards
    // +Infinity. Fix the difference by checking if we rounded down by exactly
    // 0.5, and if so, round to the other side.
    __ vrintn(out, in);
    __ vsub(temp, temp, out);
    __ Move(half_one, 0.5);
    __ VFPCompareAndSetFlags(temp, half_one);
    Label done;
    __ JumpIf(ne, &done, Label::kNear);
    // Fix wrong tie-to-even by adding 0.5 twice.
    __ vadd(out, out, half_one);
    __ vadd(out, out, half_one);
    __ bind(&done);
  } else if (kind_ == Kind::kCeil) {
    __ vrintp(out, in);
  } else if (kind_ == Kind::kFloor) {
    __ vrintm(out, in);
  }
}

int Float64Exponentiate::MaxCallStackArgs() const { return 0; }
void Float64Exponentiate::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(this);
}
void Float64Exponentiate::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(left_input());
  DoubleRegister right = ToDoubleRegister(right_input());
  DoubleRegister out = ToDoubleRegister(result());
  FrameScope scope(masm, StackFrame::MANUAL);
  __ PrepareCallCFunction(0, 2);
  __ MovToFloatParameters(left, right);
  __ CallCFunction(ExternalReference::ieee754_pow_function(), 0, 2);
  __ MovFromFloatResult(out);
}

int Float64Ieee754Unary::MaxCallStackArgs() const { return 0; }
void Float64Ieee754Unary::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void Float64Ieee754Unary::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  DoubleRegister value = ToDoubleRegister(input());
  DoubleRegister out = ToDoubleRegister(result());
  FrameScope scope(masm, StackFrame::MANUAL);
  __ PrepareCallCFunction(0, 1);
  __ MovToFloatParameter(value);
  __ CallCFunction(ieee_function_, 0, 1);
  __ MovFromFloatResult(out);
}

void CheckJSTypedArrayBounds::SetValueLocationConstraints() {
  UseRegister(receiver_input());
  if (ElementsKindSize(elements_kind_) == 1) {
    UseRegister(index_input());
  } else {
    UseAndClobberRegister(index_input());
  }
}
void CheckJSTypedArrayBounds::GenerateCode(MaglevAssembler* masm,
                                           const ProcessingState& state) {
  MAGLEV_NODE_NOT_IMPLEMENTED(CheckJSTypedArrayBounds);
}

int CheckJSDataViewBounds::MaxCallStackArgs() const { return 1; }
void CheckJSDataViewBounds::SetValueLocationConstraints() {
  UseRegister(receiver_input());
  UseRegister(index_input());
  set_temporaries_needed(1);
}
void CheckJSDataViewBounds::GenerateCode(MaglevAssembler* masm,
                                         const ProcessingState& state) {
  USE(element_type_);
  MAGLEV_NODE_NOT_IMPLEMENTED(CheckJSDataViewBounds);
}

void HoleyFloat64ToMaybeNanFloat64::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void HoleyFloat64ToMaybeNanFloat64::GenerateCode(MaglevAssembler* masm,
                                                 const ProcessingState& state) {
  MAGLEV_NODE_NOT_IMPLEMENTED(HoleyFloat64ToMaybeNanFloat64);
}

namespace {

enum class ReduceInterruptBudgetType { kLoop, kReturn };

void HandleInterruptsAndTiering(MaglevAssembler* masm, ZoneLabelRef done,
                                Node* node, ReduceInterruptBudgetType type,
                                Register scratch0) {
  // For loops, first check for interrupts. Don't do this for returns, as we
  // can't lazy deopt to the end of a return.
  if (type == ReduceInterruptBudgetType::kLoop) {
    Label next;
    // Here, we only care about interrupts since we've already guarded against
    // real stack overflows on function entry.
    {
      Register stack_limit = scratch0;
      __ LoadStackLimit(stack_limit, StackLimitKind::kInterruptStackLimit);
      __ cmp(sp, stack_limit);
      __ b(hi, &next);
    }

    // An interrupt has been requested and we must call into runtime to handle
    // it; since we already pay the call cost, combine with the TieringManager
    // call.
    {
      SaveRegisterStateForCall save_register_state(masm,
                                                   node->register_snapshot());
      Register function = scratch0;
      __ ldr(function, MemOperand(fp, StandardFrameConstants::kFunctionOffset));
      __ Push(function);
      // Move into kContextRegister after the load into scratch0, just in case
      // scratch0 happens to be kContextRegister.
      __ Move(kContextRegister, masm->native_context().object());
      __ CallRuntime(Runtime::kBytecodeBudgetInterruptWithStackCheck_Maglev, 1);
      save_register_state.DefineSafepointWithLazyDeopt(node->lazy_deopt_info());
    }
    __ b(*done);  // All done, continue.
    __ bind(&next);
  }

  // No pending interrupts. Call into the TieringManager if needed.
  {
    SaveRegisterStateForCall save_register_state(masm,
                                                 node->register_snapshot());
    Register function = scratch0;
    __ ldr(function, MemOperand(fp, StandardFrameConstants::kFunctionOffset));
    __ Push(function);
    // Move into kContextRegister after the load into scratch0, just in case
    // scratch0 happens to be kContextRegister.
    __ Move(kContextRegister, masm->native_context().object());
    // Note: must not cause a lazy deopt!
    __ CallRuntime(Runtime::kBytecodeBudgetInterrupt_Maglev, 1);
    save_register_state.DefineSafepoint();
  }
  __ b(*done);
}

void GenerateReduceInterruptBudget(MaglevAssembler* masm, Node* node,
                                   ReduceInterruptBudgetType type, int amount) {
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();
  Register feedback_cell = scratch;
  Register budget = temps.Acquire();
  __ ldr(feedback_cell,
         MemOperand(fp, StandardFrameConstants::kFunctionOffset));
  __ LoadTaggedField(
      feedback_cell,
      FieldMemOperand(feedback_cell, JSFunction::kFeedbackCellOffset));
  __ ldr(budget,
         FieldMemOperand(feedback_cell, FeedbackCell::kInterruptBudgetOffset));
  __ sub(budget, budget, Operand(amount));
  __ str(budget,
         FieldMemOperand(feedback_cell, FeedbackCell::kInterruptBudgetOffset));
  ZoneLabelRef done(masm);
  __ JumpToDeferredIf(lt, HandleInterruptsAndTiering, done, node, type,
                      scratch);
  __ bind(*done);
}

}  // namespace

int ReduceInterruptBudgetForLoop::MaxCallStackArgs() const { return 1; }
void ReduceInterruptBudgetForLoop::SetValueLocationConstraints() {
  set_temporaries_needed(2);
}
void ReduceInterruptBudgetForLoop::GenerateCode(MaglevAssembler* masm,
                                                const ProcessingState& state) {
  GenerateReduceInterruptBudget(masm, this, ReduceInterruptBudgetType::kLoop,
                                amount());
}

int ReduceInterruptBudgetForReturn::MaxCallStackArgs() const { return 1; }
void ReduceInterruptBudgetForReturn::SetValueLocationConstraints() {
  set_temporaries_needed(2);
}
void ReduceInterruptBudgetForReturn::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
  GenerateReduceInterruptBudget(masm, this, ReduceInterruptBudgetType::kReturn,
                                amount());
}

void LoadSignedIntDataViewElement::SetValueLocationConstraints() {
  UseRegister(object_input());
  UseRegister(index_input());
  if (is_little_endian_constant() ||
      type_ == ExternalArrayType::kExternalInt8Array) {
    UseAny(is_little_endian_input());
  } else {
    UseRegister(is_little_endian_input());
  }
  DefineAsRegister(this);
}
void LoadSignedIntDataViewElement::GenerateCode(MaglevAssembler* masm,
                                                const ProcessingState& state) {
  MAGLEV_NODE_NOT_IMPLEMENTED(LoadSignedIntDataViewElement);
}

void StoreSignedIntDataViewElement::SetValueLocationConstraints() {
  UseRegister(object_input());
  UseRegister(index_input());
  if (ExternalArrayElementSize(type_) > 1) {
    UseAndClobberRegister(value_input());
  } else {
    UseRegister(value_input());
  }
  if (is_little_endian_constant() ||
      type_ == ExternalArrayType::kExternalInt8Array) {
    UseAny(is_little_endian_input());
  } else {
    UseRegister(is_little_endian_input());
  }
}
void StoreSignedIntDataViewElement::GenerateCode(MaglevAssembler* masm,
                                                 const ProcessingState& state) {
  MAGLEV_NODE_NOT_IMPLEMENTED(StoreSignedIntDataViewElement);
}

void LoadDoubleDataViewElement::SetValueLocationConstraints() {
  UseRegister(object_input());
  UseRegister(index_input());
  if (is_little_endian_constant()) {
    UseAny(is_little_endian_input());
  } else {
    UseRegister(is_little_endian_input());
  }
  set_temporaries_needed(1);
  DefineAsRegister(this);
}
void LoadDoubleDataViewElement::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  MAGLEV_NODE_NOT_IMPLEMENTED(LoadDoubleDataViewElement);
}

void StoreDoubleDataViewElement::SetValueLocationConstraints() {
  UseRegister(object_input());
  UseRegister(index_input());
  UseRegister(value_input());
  if (is_little_endian_constant()) {
    UseAny(is_little_endian_input());
  } else {
    UseRegister(is_little_endian_input());
  }
  set_temporaries_needed(1);
}
void StoreDoubleDataViewElement::GenerateCode(MaglevAssembler* masm,
                                              const ProcessingState& state) {
  MAGLEV_NODE_NOT_IMPLEMENTED(StoreDoubleDataViewElement);
}

int FunctionEntryStackCheck::MaxCallStackArgs() const { return 1; }
void FunctionEntryStackCheck::SetValueLocationConstraints() {
  set_temporaries_needed(2);
}
void FunctionEntryStackCheck::GenerateCode(MaglevAssembler* masm,
                                           const ProcessingState& state) {
  if (!masm->code_gen_state()->needs_stack_check()) return;
  // Stack check. This folds the checks for both the interrupt stack limit
  // check and the real stack limit into one by just checking for the
  // interrupt limit. The interrupt limit is either equal to the real
  // stack limit or tighter. By ensuring we have space until that limit
  // after building the frame we can quickly precheck both at once.
  MaglevAssembler::ScratchRegisterScope temps(masm);
  const int stack_check_offset = masm->code_gen_state()->stack_check_offset();
  Register stack_cmp_reg = sp;
  if (stack_check_offset > kStackLimitSlackForDeoptimizationInBytes) {
    stack_cmp_reg = temps.Acquire();
    __ sub(stack_cmp_reg, sp, Operand(stack_check_offset));
  }
  Register interrupt_stack_limit = temps.Acquire();
  __ LoadStackLimit(interrupt_stack_limit,
                    StackLimitKind::kInterruptStackLimit);
  __ cmp(stack_cmp_reg, interrupt_stack_limit);

  ZoneLabelRef deferred_call_stack_guard_return(masm);
  __ JumpToDeferredIf(
      lo,
      [](MaglevAssembler* masm, FunctionEntryStackCheck* node,
         ZoneLabelRef done, int stack_check_offset) {
        ASM_CODE_COMMENT_STRING(masm, "Stack/interrupt call");
        {
          SaveRegisterStateForCall save_register_state(
              masm, node->register_snapshot());
          // Push the frame size
          __ Push(Smi::FromInt(stack_check_offset));
          __ CallRuntime(Runtime::kStackGuardWithGap, 1);
          save_register_state.DefineSafepointWithLazyDeopt(
              node->lazy_deopt_info());
        }
        __ b(*done);
      },
      this, deferred_call_stack_guard_return, stack_check_offset);
  __ bind(*deferred_call_stack_guard_return);
}

// ---
// Control nodes
// ---
void Return::SetValueLocationConstraints() {
  UseFixed(value_input(), kReturnRegister0);
}
void Return::GenerateCode(MaglevAssembler* masm, const ProcessingState& state) {
  DCHECK_EQ(ToRegister(value_input()), kReturnRegister0);

  // Read the formal number of parameters from the top level compilation unit
  // (i.e. the outermost, non inlined function).
  int formal_params_size =
      masm->compilation_info()->toplevel_compilation_unit()->parameter_count();

  // We're not going to continue execution, so we can use an arbitrary register
  // here instead of relying on temporaries from the register allocator.
  Register actual_params_size = r4;
  Register params_size = r8;

  // Compute the size of the actual parameters + receiver (in bytes).
  // TODO(leszeks): Consider making this an input into Return to re-use the
  // incoming argc's register (if it's still valid).
  __ ldr(actual_params_size,
         MemOperand(fp, StandardFrameConstants::kArgCOffset));

  // Leave the frame.
  __ LeaveFrame(StackFrame::MAGLEV);

  // If actual is bigger than formal, then we should use it to free up the stack
  // arguments.
  Label corrected_args_count;
  __ Move(params_size, formal_params_size);
  __ cmp(params_size, actual_params_size);
  __ b(kGreaterThanEqual, &corrected_args_count);
  __ Move(params_size, actual_params_size);
  __ bind(&corrected_args_count);

  // Drop receiver + arguments according to dynamic arguments size.
  __ DropArguments(params_size, MacroAssembler::kCountIsInteger,
                   MacroAssembler::kCountIncludesReceiver);
  __ Ret();
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
