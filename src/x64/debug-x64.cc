// Copyright 2009 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


#include "v8.h"

#include "codegen-inl.h"
#include "debug.h"


namespace v8 {
namespace internal {

#ifdef ENABLE_DEBUGGER_SUPPORT

bool Debug::IsDebugBreakAtReturn(v8::internal::RelocInfo* rinfo) {
  ASSERT(RelocInfo::IsJSReturn(rinfo->rmode()));
  // 11th byte of patch is 0x49 (REX.WB byte of computed jump/call to r10),
  // 11th byte of JS return is 0xCC (int3).
  ASSERT(*(rinfo->pc() + 10) == 0x49 || *(rinfo->pc() + 10) == 0xCC);
  return (*(rinfo->pc() + 10) != 0xCC);
}

#define __ ACCESS_MASM(masm)

static void Generate_DebugBreakCallHelper(MacroAssembler* masm,
                                          RegList pointer_regs,
                                          bool convert_call_to_jmp) {
  // Save the content of all general purpose registers in memory. This copy in
  // memory is later pushed onto the JS expression stack for the fake JS frame
  // generated and also to the C frame generated on top of that. In the JS
  // frame ONLY the registers containing pointers will be pushed on the
  // expression stack. This causes the GC to update these pointers so that
  // they will have the correct value when returning from the debugger.
  __ SaveRegistersToMemory(kJSCallerSaved);

  // Enter an internal frame.
  __ EnterInternalFrame();

  // Store the registers containing object pointers on the expression stack to
  // make sure that these are correctly updated during GC.
  __ PushRegistersFromMemory(pointer_regs);

#ifdef DEBUG
  __ RecordComment("// Calling from debug break to runtime - come in - over");
#endif
  __ xor_(rax, rax);  // No arguments (argc == 0).
  __ movq(rbx, ExternalReference::debug_break());

  CEntryDebugBreakStub ceb;
  __ CallStub(&ceb);

  // Restore the register values containing object pointers from the expression
  // stack in the reverse order as they where pushed.
  __ PopRegistersToMemory(pointer_regs);

  // Get rid of the internal frame.
  __ LeaveInternalFrame();

  // If this call did not replace a call but patched other code then there will
  // be an unwanted return address left on the stack. Here we get rid of that.
  if (convert_call_to_jmp) {
    __ pop(rax);
  }

  // Finally restore all registers.
  __ RestoreRegistersFromMemory(kJSCallerSaved);

  // Now that the break point has been handled, resume normal execution by
  // jumping to the target address intended by the caller and that was
  // overwritten by the address of DebugBreakXXX.
  ExternalReference after_break_target =
      ExternalReference(Debug_Address::AfterBreakTarget());
  __ movq(kScratchRegister, after_break_target);
  __ jmp(Operand(kScratchRegister, 0));
}


void Debug::GenerateCallICDebugBreak(MacroAssembler* masm) {
  // Register state for keyed IC call call (from ic-x64.cc)
  // ----------- S t a t e -------------
  //  -- rax: number of arguments
  // -----------------------------------
  // The number of arguments in rax is not smi encoded.
  Generate_DebugBreakCallHelper(masm, 0, false);
}


void Debug::GenerateConstructCallDebugBreak(MacroAssembler* masm) {
  // Register state just before return from JS function (from codegen-x64.cc).
  // rax is the actual number of arguments not encoded as a smi, see comment
  // above IC call.
  // ----------- S t a t e -------------
  //  -- rax: number of arguments
  // -----------------------------------
  // The number of arguments in rax is not smi encoded.
  Generate_DebugBreakCallHelper(masm, 0, false);
}


void Debug::GenerateKeyedLoadICDebugBreak(MacroAssembler* masm) {
  // Register state for keyed IC load call (from ic-x64.cc).
  // ----------- S t a t e -------------
  //  No registers used on entry.
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, 0, false);
}


void Debug::GenerateKeyedStoreICDebugBreak(MacroAssembler* masm) {
  // Register state for keyed IC load call (from ic-x64.cc).
  // ----------- S t a t e -------------
  //  -- rax    : value
  // -----------------------------------
  // Register rax contains an object that needs to be pushed on the
  // expression stack of the fake JS frame.
  Generate_DebugBreakCallHelper(masm, rax.bit(), false);
}


void Debug::GenerateLoadICDebugBreak(MacroAssembler* masm) {
  // Register state for IC load call (from ic-x64.cc).
  // ----------- S t a t e -------------
  //  -- rcx    : name
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, rcx.bit(), false);
}


void Debug::GenerateReturnDebugBreak(MacroAssembler* masm) {
  // Register state just before return from JS function (from codegen-x64.cc).
  // ----------- S t a t e -------------
  //  -- rax: return value
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, rax.bit(), true);
}


void Debug::GenerateReturnDebugBreakEntry(MacroAssembler* masm) {
  // OK to clobber rbx as we are returning from a JS function through the code
  // generated by CodeGenerator::GenerateReturnSequence()
  ExternalReference debug_break_return =
      ExternalReference(Debug_Address::DebugBreakReturn());
  __ movq(rbx, debug_break_return);
  __ movq(rbx, Operand(rbx, 0));
  __ addq(rbx, Immediate(Code::kHeaderSize - kHeapObjectTag));
  __ jmp(rbx);
}


void Debug::GenerateStoreICDebugBreak(MacroAssembler* masm) {
  // REgister state for IC store call (from ic-x64.cc).
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rcx    : name
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, rax.bit() | rcx.bit(), false);
}


void Debug::GenerateStubNoRegistersDebugBreak(MacroAssembler* masm) {
  // Register state for stub CallFunction (from CallFunctionStub in ic-x64.cc).
  // ----------- S t a t e -------------
  //  No registers used on entry.
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, 0, false);
}


#undef __


void BreakLocationIterator::ClearDebugBreakAtReturn() {
  rinfo()->PatchCode(original_rinfo()->pc(),
                     Debug::kX64JSReturnSequenceLength);
}


bool BreakLocationIterator::IsDebugBreakAtReturn()  {
  return Debug::IsDebugBreakAtReturn(rinfo());
}


void BreakLocationIterator::SetDebugBreakAtReturn()  {
  ASSERT(Debug::kX64JSReturnSequenceLength >= Debug::kX64CallInstructionLength);
  rinfo()->PatchCodeWithCall(Debug::debug_break_return_entry()->entry(),
      Debug::kX64JSReturnSequenceLength - Debug::kX64CallInstructionLength);
}

#endif  // ENABLE_DEBUGGER_SUPPORT

} }  // namespace v8::internal
