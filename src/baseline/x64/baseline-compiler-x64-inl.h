// Use of this source code is governed by a BSD-style license that can be
// Copyright 2021 the V8 project authors. All rights reserved.
// found in the LICENSE file.

#ifndef V8_BASELINE_X64_BASELINE_COMPILER_X64_INL_H_
#define V8_BASELINE_X64_BASELINE_COMPILER_X64_INL_H_

#include "src/base/macros.h"
#include "src/baseline/baseline-compiler.h"
#include "src/codegen/interface-descriptors.h"
#include "src/codegen/x64/register-x64.h"
#include "src/objects/code-inl.h"

namespace v8 {
namespace internal {
namespace baseline {

namespace detail {

// TODO(v8:11429,verwaest): For now this avoids using kScratchRegister(==r10)
// since the macro-assembler doesn't use this scope and will conflict.
static constexpr Register kScratchRegisters[] = {r8, r9, r11, r12, r14, r15};
static constexpr int kNumScratchRegisters = arraysize(kScratchRegisters);

}  // namespace detail

// TODO(v8:11429): Move BaselineAssembler to baseline-assembler-<arch>-inl.h
class BaselineAssembler::ScratchRegisterScope {
 public:
  explicit ScratchRegisterScope(BaselineAssembler* assembler)
      : assembler_(assembler),
        prev_scope_(assembler->scratch_register_scope_),
        registers_used_(prev_scope_ == nullptr ? 0
                                               : prev_scope_->registers_used_) {
    assembler_->scratch_register_scope_ = this;
  }
  ~ScratchRegisterScope() { assembler_->scratch_register_scope_ = prev_scope_; }

  Register AcquireScratch() {
    DCHECK_LT(registers_used_, detail::kNumScratchRegisters);
    return detail::kScratchRegisters[registers_used_++];
  }

 private:
  BaselineAssembler* assembler_;
  ScratchRegisterScope* prev_scope_;
  int registers_used_;
};

// TODO(v8:11429,leszeks): Unify condition names in the MacroAssembler.
enum class Condition : uint8_t {
  kEqual = equal,
  kNotEqual = not_equal,

  kLessThan = less,
  kGreaterThan = greater,
  kLessThanEqual = less_equal,
  kGreaterThanEqual = greater_equal,

  kUnsignedLessThan = below,
  kUnsignedGreaterThan = above,
  kUnsignedLessThanEqual = below_equal,
  kUnsignedGreaterThanEqual = above_equal,

  kOverflow = overflow,
  kNoOverflow = no_overflow,

  kZero = zero,
  kNotZero = not_zero,
};

internal::Condition AsMasmCondition(Condition cond) {
  return static_cast<internal::Condition>(cond);
}

namespace detail {

#define __ masm_->

#ifdef DEBUG
bool Clobbers(Register target, MemOperand op) {
  return op.AddressUsesRegister(target);
}
#endif

}  // namespace detail

MemOperand BaselineAssembler::RegisterFrameOperand(
    interpreter::Register interpreter_register) {
  return MemOperand(rbp, interpreter_register.ToOperand() * kSystemPointerSize);
}
MemOperand BaselineAssembler::FeedbackVectorOperand() {
  return MemOperand(rbp, BaselineFrameConstants::kFeedbackVectorFromFp);
}

void BaselineAssembler::Jump(Label* target, Label::Distance distance) {
  __ jmp(target, distance);
}
void BaselineAssembler::JumpIf(Condition cc, Label* target,
                               Label::Distance distance) {
  __ j(AsMasmCondition(cc), target, distance);
}
void BaselineAssembler::JumpIfRoot(Register value, RootIndex index,
                                   Label* target, Label::Distance distance) {
  __ JumpIfRoot(value, index, target, distance);
}
void BaselineAssembler::JumpIfNotRoot(Register value, RootIndex index,
                                      Label* target, Label::Distance distance) {
  __ JumpIfNotRoot(value, index, target, distance);
}
void BaselineAssembler::JumpIfSmi(Register value, Label* target,
                                  Label::Distance distance) {
  __ JumpIfSmi(value, target, distance);
}
void BaselineAssembler::JumpIfNotSmi(Register value, Label* target,
                                     Label::Distance distance) {
  __ JumpIfNotSmi(value, target, distance);
}

void BaselineAssembler::CallBuiltin(Builtins::Name builtin) {
  __ RecordCommentForOffHeapTrampoline(builtin);
  __ Call(__ EntryFromBuiltinIndexAsOperand(builtin));
  if (FLAG_code_comments) __ RecordComment("]");
}

void BaselineAssembler::TailCallBuiltin(Builtins::Name builtin) {
  __ RecordCommentForOffHeapTrampoline(builtin);
  __ Jump(__ EntryFromBuiltinIndexAsOperand(builtin));
  if (FLAG_code_comments) __ RecordComment("]");
}

void BaselineAssembler::Test(Register value, int mask) {
  if ((mask & 0xff) == mask) {
    __ testb(value, Immediate(mask));
  } else {
    __ testl(value, Immediate(mask));
  }
}

void BaselineAssembler::CmpObjectType(Register object,
                                      InstanceType instance_type,
                                      Register map) {
  __ CmpObjectType(object, instance_type, map);
}
void BaselineAssembler::CmpInstanceType(Register value,
                                        InstanceType instance_type) {
  __ CmpInstanceType(value, instance_type);
}
void BaselineAssembler::Cmp(Register value, Smi smi) { __ Cmp(value, smi); }
void BaselineAssembler::ComparePointer(Register value, MemOperand operand) {
  __ cmpq(value, operand);
}
void BaselineAssembler::SmiCompare(Register lhs, Register rhs) {
  __ SmiCompare(lhs, rhs);
}
// cmp_tagged
void BaselineAssembler::CompareTagged(Register value, MemOperand operand) {
  __ cmp_tagged(value, operand);
}
void BaselineAssembler::CompareTagged(MemOperand operand, Register value) {
  __ cmp_tagged(operand, value);
}
void BaselineAssembler::CompareByte(Register value, int32_t byte) {
  __ cmpb(value, Immediate(byte));
}

void BaselineAssembler::Move(interpreter::Register output, Register source) {
  return __ movq(RegisterFrameOperand(output), source);
}
void BaselineAssembler::Move(Register output, TaggedIndex value) {
  __ Move(output, value);
}
void BaselineAssembler::Move(MemOperand output, Register source) {
  __ movq(output, source);
}
void BaselineAssembler::Move(Register output, ExternalReference reference) {
  __ Move(output, reference);
}
void BaselineAssembler::Move(Register output, Handle<HeapObject> value) {
  __ Move(output, value);
}
void BaselineAssembler::Move(Register output, int32_t value) {
  __ Move(output, Immediate(value));
}
void BaselineAssembler::MoveMaybeSmi(Register output, Register source) {
  __ mov_tagged(output, source);
}
void BaselineAssembler::MoveSmi(Register output, Register source) {
  __ mov_tagged(output, source);
}

namespace detail {
void PushSingle(MacroAssembler* masm, RootIndex source) {
  masm->PushRoot(source);
}
void PushSingle(MacroAssembler* masm, Register reg) { masm->Push(reg); }
void PushSingle(MacroAssembler* masm, TaggedIndex value) { masm->Push(value); }
void PushSingle(MacroAssembler* masm, Smi value) { masm->Push(value); }
void PushSingle(MacroAssembler* masm, Handle<HeapObject> object) {
  masm->Push(object);
}
void PushSingle(MacroAssembler* masm, int32_t immediate) {
  masm->Push(Immediate(immediate));
}
void PushSingle(MacroAssembler* masm, MemOperand operand) {
  masm->Push(operand);
}
void PushSingle(MacroAssembler* masm, interpreter::Register source) {
  return PushSingle(masm, BaselineAssembler::RegisterFrameOperand(source));
}

template <typename Arg>
struct PushHelper {
  static int Push(BaselineAssembler* basm, Arg arg) {
    PushSingle(basm->masm(), arg);
    return 1;
  }
  static int PushReverse(BaselineAssembler* basm, Arg arg) {
    return Push(basm, arg);
  }
};

template <>
struct PushHelper<interpreter::RegisterList> {
  static int Push(BaselineAssembler* basm, interpreter::RegisterList list) {
    for (int reg_index = 0; reg_index < list.register_count(); ++reg_index) {
      PushSingle(basm->masm(), list[reg_index]);
    }
    return list.register_count();
  }
  static int PushReverse(BaselineAssembler* basm,
                         interpreter::RegisterList list) {
    for (int reg_index = list.register_count() - 1; reg_index >= 0;
         --reg_index) {
      PushSingle(basm->masm(), list[reg_index]);
    }
    return list.register_count();
  }
};

template <typename... Args>
struct PushAllHelper;
template <>
struct PushAllHelper<> {
  static int Push(BaselineAssembler* masm) { return 0; }
  static int PushReverse(BaselineAssembler* masm) { return 0; }
};
template <typename Arg, typename... Args>
struct PushAllHelper<Arg, Args...> {
  static int Push(BaselineAssembler* masm, Arg arg, Args... args) {
    int nargs = PushHelper<Arg>::Push(masm, arg);
    return nargs + PushAllHelper<Args...>::Push(masm, args...);
  }
  static int PushReverse(BaselineAssembler* masm, Arg arg, Args... args) {
    int nargs = PushAllHelper<Args...>::PushReverse(masm, args...);
    return nargs + PushHelper<Arg>::PushReverse(masm, arg);
  }
};

}  // namespace detail

template <typename... T>
int BaselineAssembler::Push(T... vals) {
  return detail::PushAllHelper<T...>::Push(this, vals...);
}

template <typename... T>
void BaselineAssembler::PushReverse(T... vals) {
  detail::PushAllHelper<T...>::PushReverse(this, vals...);
}

template <typename... T>
void BaselineAssembler::Pop(T... registers) {
  ITERATE_PACK(__ Pop(registers));
}

void BaselineAssembler::LoadTaggedPointerField(Register output, Register source,
                                               int offset) {
  __ LoadTaggedPointerField(output, FieldOperand(source, offset));
}
void BaselineAssembler::LoadTaggedSignedField(Register output, Register source,
                                              int offset) {
  __ LoadTaggedSignedField(output, FieldOperand(source, offset));
}
void BaselineAssembler::LoadTaggedAnyField(Register output, Register source,
                                           int offset) {
  __ LoadAnyTaggedField(output, FieldOperand(source, offset));
}
void BaselineAssembler::LoadByteField(Register output, Register source,
                                      int offset) {
  __ movb(output, FieldOperand(source, offset));
}
void BaselineAssembler::StoreTaggedSignedField(Register target, int offset,
                                               Smi value) {
  __ StoreTaggedSignedField(FieldOperand(target, offset), value);
}
void BaselineAssembler::StoreTaggedFieldWithWriteBarrier(Register target,
                                                         int offset,

                                                         Register value) {
  BaselineAssembler::ScratchRegisterScope scratch_scope(this);
  Register scratch = scratch_scope.AcquireScratch();
  DCHECK_NE(target, scratch);
  DCHECK_NE(value, scratch);
  __ StoreTaggedField(FieldOperand(target, offset), value);
  __ RecordWriteField(target, offset, value, scratch, kDontSaveFPRegs);
}
void BaselineAssembler::StoreTaggedFieldNoWriteBarrier(Register target,
                                                       int offset,
                                                       Register value) {
  __ StoreTaggedField(FieldOperand(target, offset), value);
}

void BaselineAssembler::AddToInterruptBudget(int32_t weight) {
  ScratchRegisterScope scratch_scope(this);
  Register feedback_cell = scratch_scope.AcquireScratch();
  LoadFunction(feedback_cell);
  LoadTaggedPointerField(feedback_cell, feedback_cell,
                         JSFunction::kFeedbackCellOffset);
  __ addl(FieldOperand(feedback_cell, FeedbackCell::kInterruptBudgetOffset),
          Immediate(weight));
}

void BaselineAssembler::AddToInterruptBudget(Register weight) {
  ScratchRegisterScope scratch_scope(this);
  Register feedback_cell = scratch_scope.AcquireScratch();
  LoadFunction(feedback_cell);
  LoadTaggedPointerField(feedback_cell, feedback_cell,
                         JSFunction::kFeedbackCellOffset);
  __ addl(FieldOperand(feedback_cell, FeedbackCell::kInterruptBudgetOffset),
          weight);
}

void BaselineAssembler::AddSmi(Register lhs, Smi rhs) {
  if (rhs.value() == 0) return;
  if (SmiValuesAre31Bits()) {
    __ addl(lhs, Immediate(rhs));
  } else {
    ScratchRegisterScope scratch_scope(this);
    Register rhs_reg = scratch_scope.AcquireScratch();
    __ Move(rhs_reg, rhs);
    __ addq(lhs, rhs_reg);
  }
}

void BaselineAssembler::Switch(Register reg, int case_value_base,
                               Label** labels, int num_labels) {
  ScratchRegisterScope scope(this);
  Register table = scope.AcquireScratch();
  Label fallthrough, jump_table;
  if (case_value_base > 0) {
    __ subq(reg, Immediate(case_value_base));
  }
  __ cmpq(reg, Immediate(num_labels));
  __ j(above_equal, &fallthrough);
  __ leaq(table, MemOperand(&jump_table));
  __ jmp(MemOperand(table, reg, times_8, 0));
  // Emit the jump table inline, under the assumption that it's not too big.
  __ Align(kSystemPointerSize);
  __ bind(&jump_table);
  for (int i = 0; i < num_labels; ++i) {
    __ dq(labels[i]);
  }
  __ bind(&fallthrough);
}

#undef __

#define __ basm_.

void BaselineCompiler::Prologue() {
  __ Move(kInterpreterBytecodeArrayRegister, bytecode_);
  DCHECK_EQ(kJSFunctionRegister, kJavaScriptCallTargetRegister);
  CallBuiltin(Builtins::kBaselineOutOfLinePrologue, kContextRegister,
              kJSFunctionRegister, kJavaScriptCallArgCountRegister,
              kInterpreterBytecodeArrayRegister);

  PrologueFillFrame();
}

void BaselineCompiler::PrologueFillFrame() {
  __ RecordComment("[ Fill frame");
  // Inlined register frame fill
  interpreter::Register new_target_or_generator_register =
      bytecode_->incoming_new_target_or_generator_register();
  __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kUndefinedValue);
  int register_count = bytecode_->register_count();
  // Magic value
  const int kLoopUnrollSize = 8;
  const int new_target_index = new_target_or_generator_register.index();
  const bool has_new_target = new_target_index != kMaxInt;
  int i = 0;
  if (has_new_target) {
    DCHECK_LE(new_target_index, register_count);
    for (; i < new_target_index; i++) {
      __ Push(kInterpreterAccumulatorRegister);
    }
    // Push new_target_or_generator.
    __ Push(kJavaScriptCallNewTargetRegister);
    i++;
  }
  if (register_count < 2 * kLoopUnrollSize) {
    // If the frame is small enough, just unroll the frame fill completely.
    for (; i < register_count; ++i) {
      __ Push(kInterpreterAccumulatorRegister);
    }
  } else {
    register_count -= i;
    i = 0;
    // Extract the first few registers to round to the unroll size.
    int first_registers = register_count % kLoopUnrollSize;
    for (; i < first_registers; ++i) {
      __ Push(kInterpreterAccumulatorRegister);
    }
    BaselineAssembler::ScratchRegisterScope scope(&basm_);
    Register scratch = scope.AcquireScratch();
    __ Move(scratch, register_count / kLoopUnrollSize);
    // We enter the loop unconditionally, so make sure we need to loop at least
    // once.
    DCHECK_GT(register_count / kLoopUnrollSize, 0);
    Label loop;
    __ Bind(&loop);
    for (int j = 0; j < kLoopUnrollSize; ++j) {
      __ Push(kInterpreterAccumulatorRegister);
    }
    __ masm()->decl(scratch);
    __ JumpIf(Condition::kGreaterThan, &loop);
  }
  __ RecordComment("]");
}

void BaselineCompiler::VerifyFrameSize() {
  __ Move(kScratchRegister, rsp);
  __ masm()->addq(kScratchRegister,
                  Immediate(InterpreterFrameConstants::kFixedFrameSizeFromFp +
                            bytecode_->frame_size()));
  __ masm()->cmpq(kScratchRegister, rbp);
  __ masm()->Assert(equal, AbortReason::kUnexpectedStackPointer);
}

#undef __

#define __ basm.

void BaselineAssembler::EmitReturn(MacroAssembler* masm) {
  BaselineAssembler basm(masm);

  Register weight = BaselineLeaveFrameDescriptor::WeightRegister();
  Register params_size = BaselineLeaveFrameDescriptor::ParamsSizeRegister();

  __ RecordComment("[ Update Interrupt Budget");
  __ AddToInterruptBudget(weight);

  // Use compare flags set by add
  // TODO(v8:11429,leszeks): This might be trickier cross-arch.
  Label skip_interrupt_label;
  __ JumpIf(Condition::kGreaterThanEqual, &skip_interrupt_label);
  {
    __ masm()->SmiTag(params_size);
    __ Push(params_size, kInterpreterAccumulatorRegister);

    __ LoadContext(kContextRegister);
    __ Push(MemOperand(rbp, InterpreterFrameConstants::kFunctionOffset));
    __ CallRuntime(Runtime::kBytecodeBudgetInterruptFromBytecode, 1);

    __ Pop(kInterpreterAccumulatorRegister, params_size);
    __ masm()->SmiUntag(params_size);
  }
  __ RecordComment("]");

  __ Bind(&skip_interrupt_label);

  BaselineAssembler::ScratchRegisterScope scope(&basm);
  Register scratch = scope.AcquireScratch();

  Register actual_params_size = scratch;
  // Compute the size of the actual parameters + receiver (in bytes).
  __ masm()->movq(actual_params_size,
                  MemOperand(rbp, StandardFrameConstants::kArgCOffset));

  // If actual is bigger than formal, then we should use it to free up the stack
  // arguments.
  Label corrected_args_count;
  __ masm()->cmpq(params_size, actual_params_size);
  __ JumpIf(Condition::kGreaterThanEqual, &corrected_args_count, Label::kNear);
  __ masm()->movq(params_size, actual_params_size);
  __ Bind(&corrected_args_count);

  // Leave the frame (also dropping the register file).
  __ masm()->LeaveFrame(StackFrame::MANUAL);

  // Drop receiver + arguments.
  Register return_pc = scratch;
  __ masm()->PopReturnAddressTo(return_pc);
  __ masm()->leaq(rsp, MemOperand(rsp, params_size, times_system_pointer_size,
                                  kSystemPointerSize));
  __ masm()->PushReturnAddressFrom(return_pc);
  __ masm()->Ret();
}

#undef __

}  // namespace baseline
}  // namespace internal
}  // namespace v8

#endif  // V8_BASELINE_X64_BASELINE_COMPILER_X64_INL_H_
