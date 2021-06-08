// Use of this source code is governed by a BSD-style license that can be
// Copyright 2021 the V8 project authors. All rights reserved.
// found in the LICENSE file.

#ifndef V8_BASELINE_X64_BASELINE_COMPILER_X64_INL_H_
#define V8_BASELINE_X64_BASELINE_COMPILER_X64_INL_H_

#include "src/base/macros.h"
#include "src/baseline/baseline-compiler.h"
#include "src/codegen/interface-descriptors.h"

namespace v8 {
namespace internal {
namespace baseline {

#define __ basm_.

void BaselineCompiler::Prologue() {
  DCHECK_EQ(kJSFunctionRegister, kJavaScriptCallTargetRegister);
  int max_frame_size = bytecode_->frame_size() + max_call_args_;
  CallBuiltin<Builtin::kBaselineOutOfLinePrologue>(
      kContextRegister, kJSFunctionRegister, kJavaScriptCallArgCountRegister,
      max_frame_size, kJavaScriptCallNewTargetRegister, bytecode_);

  PrologueFillFrame();
}

void BaselineCompiler::PrologueFillFrame() {
  __ RecordComment("[ Fill frame");
  // Inlined register frame fill
  interpreter::Register new_target_or_generator_register =
      bytecode_->incoming_new_target_or_generator_register();
  if (FLAG_debug_code) {
    __ masm()->Cmp(kInterpreterAccumulatorRegister,
                   isolate_->factory()->undefined_value());
    __ masm()->Assert(equal, AbortReason::kUnexpectedValue);
  }
  int register_count = bytecode_->register_count();
  // Magic value
  const int kLoopUnrollSize = 8;
  const int new_target_index = new_target_or_generator_register.index();
  const bool has_new_target = new_target_index != kMaxInt;
  if (has_new_target) {
    DCHECK_LE(new_target_index, register_count);
    for (int i = 0; i < new_target_index; i++) {
      __ Push(kInterpreterAccumulatorRegister);
    }
    // Push new_target_or_generator.
    __ Push(kJavaScriptCallNewTargetRegister);
    register_count -= new_target_index + 1;
  }
  if (register_count < 2 * kLoopUnrollSize) {
    // If the frame is small enough, just unroll the frame fill completely.
    for (int i = 0; i < register_count; ++i) {
      __ Push(kInterpreterAccumulatorRegister);
    }
  } else {
    // Extract the first few registers to round to the unroll size.
    int first_registers = register_count % kLoopUnrollSize;
    for (int i = 0; i < first_registers; ++i) {
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
    for (int i = 0; i < kLoopUnrollSize; ++i) {
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

}  // namespace baseline
}  // namespace internal
}  // namespace v8

#endif  // V8_BASELINE_X64_BASELINE_COMPILER_X64_INL_H_
