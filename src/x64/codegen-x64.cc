// Copyright 2011 the V8 project authors. All rights reserved.
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

#if defined(V8_TARGET_ARCH_X64)

#include "codegen.h"
#include "macro-assembler.h"

namespace v8 {
namespace internal {

// -------------------------------------------------------------------------
// Platform-specific RuntimeCallHelper functions.

void StubRuntimeCallHelper::BeforeCall(MacroAssembler* masm) const {
  masm->EnterFrame(StackFrame::INTERNAL);
  ASSERT(!masm->has_frame());
  masm->set_has_frame(true);
}


void StubRuntimeCallHelper::AfterCall(MacroAssembler* masm) const {
  masm->LeaveFrame(StackFrame::INTERNAL);
  ASSERT(masm->has_frame());
  masm->set_has_frame(false);
}


#define __ masm.

#ifdef _WIN64
typedef double (*ModuloFunction)(double, double);
// Define custom fmod implementation.
ModuloFunction CreateModuloFunction() {
  size_t actual_size;
  byte* buffer = static_cast<byte*>(OS::Allocate(Assembler::kMinimalBufferSize,
                                                 &actual_size,
                                                 true));
  CHECK(buffer);
  Assembler masm(NULL, buffer, static_cast<int>(actual_size));
  // Generated code is put into a fixed, unmovable, buffer, and not into
  // the V8 heap. We can't, and don't, refer to any relocatable addresses
  // (e.g. the JavaScript nan-object).

  // Windows 64 ABI passes double arguments in xmm0, xmm1 and
  // returns result in xmm0.
  // Argument backing space is allocated on the stack above
  // the return address.

  // Compute x mod y.
  // Load y and x (use argument backing store as temporary storage).
  __ movsd(Operand(rsp, kPointerSize * 2), xmm1);
  __ movsd(Operand(rsp, kPointerSize), xmm0);
  __ fld_d(Operand(rsp, kPointerSize * 2));
  __ fld_d(Operand(rsp, kPointerSize));

  // Clear exception flags before operation.
  {
    Label no_exceptions;
    __ fwait();
    __ fnstsw_ax();
    // Clear if Illegal Operand or Zero Division exceptions are set.
    __ testb(rax, Immediate(5));
    __ j(zero, &no_exceptions);
    __ fnclex();
    __ bind(&no_exceptions);
  }

  // Compute st(0) % st(1)
  {
    Label partial_remainder_loop;
    __ bind(&partial_remainder_loop);
    __ fprem();
    __ fwait();
    __ fnstsw_ax();
    __ testl(rax, Immediate(0x400 /* C2 */));
    // If C2 is set, computation only has partial result. Loop to
    // continue computation.
    __ j(not_zero, &partial_remainder_loop);
  }

  Label valid_result;
  Label return_result;
  // If Invalid Operand or Zero Division exceptions are set,
  // return NaN.
  __ testb(rax, Immediate(5));
  __ j(zero, &valid_result);
  __ fstp(0);  // Drop result in st(0).
  int64_t kNaNValue = V8_INT64_C(0x7ff8000000000000);
  __ movq(rcx, kNaNValue, RelocInfo::NONE);
  __ movq(Operand(rsp, kPointerSize), rcx);
  __ movsd(xmm0, Operand(rsp, kPointerSize));
  __ jmp(&return_result);

  // If result is valid, return that.
  __ bind(&valid_result);
  __ fstp_d(Operand(rsp, kPointerSize));
  __ movsd(xmm0, Operand(rsp, kPointerSize));

  // Clean up FPU stack and exceptions and return xmm0
  __ bind(&return_result);
  __ fstp(0);  // Unload y.

  Label clear_exceptions;
  __ testb(rax, Immediate(0x3f /* Any Exception*/));
  __ j(not_zero, &clear_exceptions);
  __ ret(0);
  __ bind(&clear_exceptions);
  __ fnclex();
  __ ret(0);

  CodeDesc desc;
  masm.GetCode(&desc);
  OS::ProtectCode(buffer, actual_size);
  // Call the function from C++ through this pointer.
  return FUNCTION_CAST<ModuloFunction>(buffer);
}

#endif

#undef __

// -------------------------------------------------------------------------
// Code generators

#define __ ACCESS_MASM(masm)

void ElementsTransitionGenerator::GenerateSmiOnlyToObject(
    MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rbx    : target map
  //  -- rcx    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  // Set transitioned map.
  __ movq(FieldOperand(rdx, HeapObject::kMapOffset), rbx);
  __ RecordWriteField(rdx,
                      HeapObject::kMapOffset,
                      rbx,
                      rdi,
                      kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
}


void ElementsTransitionGenerator::GenerateSmiOnlyToDouble(
    MacroAssembler* masm, Label* fail) {
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rbx    : target map
  //  -- rcx    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  // The fail label is not actually used since we do not allocate.

  // Set transitioned map.
  __ movq(FieldOperand(rdx, HeapObject::kMapOffset), rbx);
  __ RecordWriteField(rdx,
                      HeapObject::kMapOffset,
                      rbx,
                      rdi,
                      kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  // Set backing store's map
  __ movq(r8, FieldOperand(rdx, JSObject::kElementsOffset));
  __ LoadRoot(rdi, Heap::kFixedDoubleArrayMapRootIndex);
  __ movq(FieldOperand(r8, HeapObject::kMapOffset), rdi);

  // Convert smis to doubles and holes to hole NaNs.  Since FixedArray and
  // FixedDoubleArray do not differ in size, we do not allocate a new array.
  STATIC_ASSERT(FixedDoubleArray::kLengthOffset == FixedArray::kLengthOffset);
  STATIC_ASSERT(FixedDoubleArray::kHeaderSize == FixedArray::kHeaderSize);
  __ SmiToInteger32(r9, FieldOperand(r8, FixedDoubleArray::kLengthOffset));
  // r8 : elements array
  // r9 : elements array length
  Label loop, entry, convert_hole;
  __ movq(r15, BitCast<int64_t, uint64_t>(kHoleNanInt64), RelocInfo::NONE);
  // r15: the-hole NaN
  __ jmp(&entry);
  __ bind(&loop);
  __ decq(r9);
  __ movq(rbx,
          FieldOperand(r8, r9, times_8, FixedArray::kHeaderSize));
  // r9 : current element's index
  // rbx: current element (smi-tagged)
  __ JumpIfNotSmi(rbx, &convert_hole);
  __ SmiToInteger32(rbx, rbx);
  __ cvtlsi2sd(xmm0, rbx);
  __ movsd(FieldOperand(r8, r9, times_8, FixedDoubleArray::kHeaderSize),
           xmm0);
  __ jmp(&entry);
  __ bind(&convert_hole);
  __ movq(FieldOperand(r8, r9, times_8, FixedDoubleArray::kHeaderSize), r15);
  __ bind(&entry);
  __ testq(r9, r9);
  __ j(not_zero, &loop);
}


void ElementsTransitionGenerator::GenerateDoubleToObject(
    MacroAssembler* masm, Label* fail) {
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rbx    : target map
  //  -- rcx    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  Label loop, entry, convert_hole, gc_required;
  __ push(rax);

  __ movq(r8, FieldOperand(rdx, JSObject::kElementsOffset));
  __ SmiToInteger32(r9, FieldOperand(r8, FixedDoubleArray::kLengthOffset));
  // r8 : source FixedDoubleArray
  // r9 : number of elements
  __ lea(rdi, Operand(r9, times_pointer_size, FixedArray::kHeaderSize));
  __ AllocateInNewSpace(rdi, rax, r14, r15, &gc_required, TAG_OBJECT);
  // rax: destination FixedArray
  __ LoadRoot(rdi, Heap::kFixedArrayMapRootIndex);
  __ movq(FieldOperand(rax, HeapObject::kMapOffset), rdi);
  __ Integer32ToSmi(r14, r9);
  __ movq(FieldOperand(rax, FixedArray::kLengthOffset), r14);

  // Prepare for conversion loop.
  __ movq(rsi, BitCast<int64_t, uint64_t>(kHoleNanInt64), RelocInfo::NONE);
  __ LoadRoot(rdi, Heap::kTheHoleValueRootIndex);
  // rsi: the-hole NaN
  // rdi: pointer to the-hole
  __ jmp(&entry);

  // Call into runtime if GC is required.
  __ bind(&gc_required);
  __ pop(rax);
  __ movq(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));
  __ jmp(fail);

  // Box doubles into heap numbers.
  __ bind(&loop);
  __ decq(r9);
  __ movq(r14, FieldOperand(r8,
                            r9,
                            times_pointer_size,
                            FixedDoubleArray::kHeaderSize));
  // r9 : current element's index
  // r14: current element
  __ cmpq(r14, rsi);
  __ j(equal, &convert_hole);

  // Non-hole double, copy value into a heap number.
  __ AllocateHeapNumber(r11, r15, &gc_required);
  // r11: new heap number
  __ movq(FieldOperand(r11, HeapNumber::kValueOffset), r14);
  __ movq(FieldOperand(rax,
                       r9,
                       times_pointer_size,
                       FixedArray::kHeaderSize),
          r11);
  __ movq(r15, r9);
  __ RecordWriteArray(rax,
                      r11,
                      r15,
                      kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  __ jmp(&entry, Label::kNear);

  // Replace the-hole NaN with the-hole pointer.
  __ bind(&convert_hole);
  __ movq(FieldOperand(rax,
                       r9,
                       times_pointer_size,
                       FixedArray::kHeaderSize),
          rdi);

  __ bind(&entry);
  __ testq(r9, r9);
  __ j(not_zero, &loop);

  // Set transitioned map.
  __ movq(FieldOperand(rdx, HeapObject::kMapOffset), rbx);
  __ RecordWriteField(rdx,
                      HeapObject::kMapOffset,
                      rbx,
                      rdi,
                      kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  // Replace receiver's backing store with newly created and filled FixedArray.
  __ movq(FieldOperand(rdx, JSObject::kElementsOffset), rax);
  __ RecordWriteField(rdx,
                      JSObject::kElementsOffset,
                      rax,
                      rdi,
                      kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  __ pop(rax);
  __ movq(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));
}

#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_X64
