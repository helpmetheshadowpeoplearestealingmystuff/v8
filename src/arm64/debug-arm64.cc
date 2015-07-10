// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_ARM64

#include "src/codegen.h"
#include "src/debug.h"

namespace v8 {
namespace internal {


#define __ ACCESS_MASM(masm)


void BreakLocation::SetDebugBreakAtReturn() {
  // Patch the code emitted by FullCodeGenerator::EmitReturnSequence, changing
  // the return from JS function sequence from
  //   mov sp, fp
  //   ldp fp, lr, [sp] #16
  //   lrd ip0, [pc, #(3 * kInstructionSize)]
  //   add sp, sp, ip0
  //   ret
  //   <number of paramters ...
  //    ... plus one (64 bits)>
  // to a call to the debug break return code.
  //   ldr ip0, [pc, #(3 * kInstructionSize)]
  //   blr ip0
  //   hlt kHltBadCode    @ code should not return, catch if it does.
  //   <debug break return code ...
  //    ... entry point address (64 bits)>

  // The patching code must not overflow the space occupied by the return
  // sequence.
  STATIC_ASSERT(Assembler::kJSReturnSequenceInstructions >= 5);
  PatchingAssembler patcher(reinterpret_cast<Instruction*>(pc()), 5);
  byte* entry =
      debug_info_->GetIsolate()->builtins()->Return_DebugBreak()->entry();

  // The first instruction of a patched return sequence must be a load literal
  // loading the address of the debug break return code.
  patcher.ldr_pcrel(ip0, (3 * kInstructionSize) >> kLoadLiteralScaleLog2);
  // TODO(all): check the following is correct.
  // The debug break return code will push a frame and call statically compiled
  // code. By using blr, even though control will not return after the branch,
  // this call site will be registered in the frame (lr being saved as the pc
  // of the next instruction to execute for this frame). The debugger can now
  // iterate on the frames to find call to debug break return code.
  patcher.blr(ip0);
  patcher.hlt(kHltBadCode);
  patcher.dc64(reinterpret_cast<int64_t>(entry));
}


void BreakLocation::SetDebugBreakAtSlot() {
  // Patch the code emitted by DebugCodegen::GenerateSlots, changing the debug
  // break slot code from
  //   mov x0, x0    @ nop DEBUG_BREAK_NOP
  //   mov x0, x0    @ nop DEBUG_BREAK_NOP
  //   mov x0, x0    @ nop DEBUG_BREAK_NOP
  //   mov x0, x0    @ nop DEBUG_BREAK_NOP
  // to a call to the debug slot code.
  //   ldr ip0, [pc, #(2 * kInstructionSize)]
  //   blr ip0
  //   <debug break slot code ...
  //    ... entry point address (64 bits)>

  // TODO(all): consider adding a hlt instruction after the blr as we don't
  // expect control to return here. This implies increasing
  // kDebugBreakSlotInstructions to 5 instructions.

  // The patching code must not overflow the space occupied by the return
  // sequence.
  STATIC_ASSERT(Assembler::kDebugBreakSlotInstructions >= 4);
  PatchingAssembler patcher(reinterpret_cast<Instruction*>(pc()), 4);
  byte* entry =
      debug_info_->GetIsolate()->builtins()->Slot_DebugBreak()->entry();

  // The first instruction of a patched debug break slot must be a load literal
  // loading the address of the debug break slot code.
  patcher.ldr_pcrel(ip0, (2 * kInstructionSize) >> kLoadLiteralScaleLog2);
  // TODO(all): check the following is correct.
  // The debug break slot code will push a frame and call statically compiled
  // code. By using blr, event hough control will not return after the branch,
  // this call site will be registered in the frame (lr being saved as the pc
  // of the next instruction to execute for this frame). The debugger can now
  // iterate on the frames to find call to debug break slot code.
  patcher.blr(ip0);
  patcher.dc64(reinterpret_cast<int64_t>(entry));
}


static void Generate_DebugBreakCallHelper(MacroAssembler* masm,
                                          RegList object_regs) {
  Register scratch = x10;
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Load padding words on stack.
    __ Mov(scratch, Smi::FromInt(LiveEdit::kFramePaddingValue));
    __ PushMultipleTimes(scratch, LiveEdit::kFramePaddingInitialSize);
    __ Mov(scratch, Smi::FromInt(LiveEdit::kFramePaddingInitialSize));
    __ Push(scratch);

    // Any live values (object_regs and non_object_regs) in caller-saved
    // registers (or lr) need to be stored on the stack so that their values are
    // safely preserved for a call into C code.
    //
    // Also:
    //  * object_regs may be modified during the C code by the garbage
    //    collector. Every object register must be a valid tagged pointer or
    //    SMI.
    //
    //  * non_object_regs will be converted to SMIs so that the garbage
    //    collector doesn't try to interpret them as pointers.
    //
    // TODO(jbramley): Why can't this handle callee-saved registers?
    DCHECK((~kCallerSaved.list() & object_regs) == 0);
    DCHECK((scratch.Bit() & object_regs) == 0);
    DCHECK((masm->TmpList()->list() & object_regs) == 0);
    STATIC_ASSERT(kSmiValueSize == 32);

    if (object_regs != 0) {
      __ PushXRegList(object_regs);
    }

#ifdef DEBUG
    __ RecordComment("// Calling from debug break to runtime - come in - over");
#endif
    __ Mov(x0, 0);  // No arguments.
    __ Mov(x1, ExternalReference::debug_break(masm->isolate()));

    CEntryStub stub(masm->isolate(), 1);
    __ CallStub(&stub);

    // Restore the register values from the expression stack.
    if (object_regs != 0) {
      __ PopXRegList(object_regs);
    }

    // Don't bother removing padding bytes pushed on the stack
    // as the frame is going to be restored right away.

    // Leave the internal frame.
  }

  // Now that the break point has been handled, resume normal execution by
  // jumping to the target address intended by the caller and that was
  // overwritten by the address of DebugBreakXXX.
  ExternalReference after_break_target =
      ExternalReference::debug_after_break_target_address(masm->isolate());
  __ Mov(scratch, after_break_target);
  __ Ldr(scratch, MemOperand(scratch));
  __ Br(scratch);
}


void DebugCodegen::GenerateReturnDebugBreak(MacroAssembler* masm) {
  // In places other than IC call sites it is expected that r0 is TOS which
  // is an object - this is not generally the case so this should be used with
  // care.
  Generate_DebugBreakCallHelper(masm, x0.Bit());
}


void DebugCodegen::GenerateSlot(MacroAssembler* masm) {
  // Generate enough nop's to make space for a call instruction. Avoid emitting
  // the constant pool in the debug break slot code.
  InstructionAccurateScope scope(masm, Assembler::kDebugBreakSlotInstructions);

  for (int i = 0; i < Assembler::kDebugBreakSlotInstructions; i++) {
    __ nop(Assembler::DEBUG_BREAK_NOP);
  }
}


void DebugCodegen::GenerateSlotDebugBreak(MacroAssembler* masm) {
  // In the places where a debug break slot is inserted no registers can contain
  // object pointers.
  Generate_DebugBreakCallHelper(masm, 0);
}


void DebugCodegen::GeneratePlainReturnLiveEdit(MacroAssembler* masm) {
  __ Ret();
}


void DebugCodegen::GenerateFrameDropperLiveEdit(MacroAssembler* masm) {
  ExternalReference restarter_frame_function_slot =
      ExternalReference::debug_restarter_frame_function_pointer_address(
          masm->isolate());
  UseScratchRegisterScope temps(masm);
  Register scratch = temps.AcquireX();

  __ Mov(scratch, restarter_frame_function_slot);
  __ Str(xzr, MemOperand(scratch));

  // We do not know our frame height, but set sp based on fp.
  __ Sub(masm->StackPointer(), fp, kPointerSize);
  __ AssertStackConsistency();

  __ Pop(x1, fp, lr);  // Function, Frame, Return address.

  // Load context from the function.
  __ Ldr(cp, FieldMemOperand(x1, JSFunction::kContextOffset));

  // Get function code.
  __ Ldr(scratch, FieldMemOperand(x1, JSFunction::kSharedFunctionInfoOffset));
  __ Ldr(scratch, FieldMemOperand(scratch, SharedFunctionInfo::kCodeOffset));
  __ Add(scratch, scratch, Code::kHeaderSize - kHeapObjectTag);

  // Re-run JSFunction, x1 is function, cp is context.
  __ Br(scratch);
}


const bool LiveEdit::kFrameDropperSupported = true;

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM64
