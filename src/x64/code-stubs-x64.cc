// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_X64

#include "src/api-arguments-inl.h"
#include "src/bootstrapper.h"
#include "src/code-stubs.h"
#include "src/counters.h"
#include "src/double.h"
#include "src/frame-constants.h"
#include "src/frames.h"
#include "src/ic/ic.h"
#include "src/ic/stub-cache.h"
#include "src/isolate.h"
#include "src/macro-assembler.h"
#include "src/objects-inl.h"
#include "src/objects/api-callbacks.h"
#include "src/objects/regexp-match-info.h"
#include "src/regexp/jsregexp.h"
#include "src/regexp/regexp-macro-assembler.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void JSEntryStub::Generate(MacroAssembler* masm) {
  Label invoke, handler_entry, exit;
  Label not_outermost_js, not_outermost_js_2;

  {  // NOLINT. Scope block confuses linter.
    NoRootArrayScope uninitialized_root_register(masm);
    // Set up frame.
    __ pushq(rbp);
    __ movp(rbp, rsp);

    // Push the stack frame type.
    __ Push(Immediate(StackFrame::TypeToMarker(type())));  // context slot
    ExternalReference context_address =
        ExternalReference::Create(IsolateAddressId::kContextAddress, isolate());
    __ Load(kScratchRegister, context_address);
    __ Push(kScratchRegister);  // context
    // Save callee-saved registers (X64/X32/Win64 calling conventions).
    __ pushq(r12);
    __ pushq(r13);
    __ pushq(r14);
    __ pushq(r15);
#ifdef _WIN64
    __ pushq(rdi);  // Only callee save in Win64 ABI, argument in AMD64 ABI.
    __ pushq(rsi);  // Only callee save in Win64 ABI, argument in AMD64 ABI.
#endif
    __ pushq(rbx);

#ifdef _WIN64
    // On Win64 XMM6-XMM15 are callee-save
    __ subp(rsp, Immediate(EntryFrameConstants::kXMMRegistersBlockSize));
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 0), xmm6);
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 1), xmm7);
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 2), xmm8);
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 3), xmm9);
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 4), xmm10);
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 5), xmm11);
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 6), xmm12);
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 7), xmm13);
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 8), xmm14);
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 9), xmm15);
#endif

    __ InitializeRootRegister();
  }

  // Save copies of the top frame descriptor on the stack.
  ExternalReference c_entry_fp =
      ExternalReference::Create(IsolateAddressId::kCEntryFPAddress, isolate());
  {
    Operand c_entry_fp_operand = masm->ExternalReferenceAsOperand(c_entry_fp);
    __ Push(c_entry_fp_operand);
  }

  // If this is the outermost JS call, set js_entry_sp value.
  ExternalReference js_entry_sp =
      ExternalReference::Create(IsolateAddressId::kJSEntrySPAddress, isolate());
  __ Load(rax, js_entry_sp);
  __ testp(rax, rax);
  __ j(not_zero, &not_outermost_js);
  __ Push(Immediate(StackFrame::OUTERMOST_JSENTRY_FRAME));
  __ movp(rax, rbp);
  __ Store(js_entry_sp, rax);
  Label cont;
  __ jmp(&cont);
  __ bind(&not_outermost_js);
  __ Push(Immediate(StackFrame::INNER_JSENTRY_FRAME));
  __ bind(&cont);

  // Jump to a faked try block that does the invoke, with a faked catch
  // block that sets the pending exception.
  __ jmp(&invoke);
  __ bind(&handler_entry);
  handler_offset_ = handler_entry.pos();
  // Caught exception: Store result (exception) in the pending exception
  // field in the JSEnv and return a failure sentinel.
  ExternalReference pending_exception = ExternalReference::Create(
      IsolateAddressId::kPendingExceptionAddress, isolate());
  __ Store(pending_exception, rax);
  __ LoadRoot(rax, RootIndex::kException);
  __ jmp(&exit);

  // Invoke: Link this frame into the handler chain.
  __ bind(&invoke);
  __ PushStackHandler();

  // Invoke the function by calling through JS entry trampoline builtin and
  // pop the faked function when we return. We load the address from an
  // external reference instead of inlining the call target address directly
  // in the code, because the builtin stubs may not have been generated yet
  // at the time this code is generated.
  __ Call(EntryTrampoline(), RelocInfo::CODE_TARGET);

  // Unlink this frame from the handler chain.
  __ PopStackHandler();

  __ bind(&exit);
  // Check if the current stack frame is marked as the outermost JS frame.
  __ Pop(rbx);
  __ cmpp(rbx, Immediate(StackFrame::OUTERMOST_JSENTRY_FRAME));
  __ j(not_equal, &not_outermost_js_2);
  __ Move(kScratchRegister, js_entry_sp);
  __ movp(Operand(kScratchRegister, 0), Immediate(0));
  __ bind(&not_outermost_js_2);

  // Restore the top frame descriptor from the stack.
  {
    Operand c_entry_fp_operand = masm->ExternalReferenceAsOperand(c_entry_fp);
    __ Pop(c_entry_fp_operand);
  }

  // Restore callee-saved registers (X64 conventions).
#ifdef _WIN64
  // On Win64 XMM6-XMM15 are callee-save
  __ movdqu(xmm6, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 0));
  __ movdqu(xmm7, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 1));
  __ movdqu(xmm8, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 2));
  __ movdqu(xmm9, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 3));
  __ movdqu(xmm10, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 4));
  __ movdqu(xmm11, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 5));
  __ movdqu(xmm12, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 6));
  __ movdqu(xmm13, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 7));
  __ movdqu(xmm14, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 8));
  __ movdqu(xmm15, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 9));
  __ addp(rsp, Immediate(EntryFrameConstants::kXMMRegistersBlockSize));
#endif

  __ popq(rbx);
#ifdef _WIN64
  // Callee save on in Win64 ABI, arguments/volatile in AMD64 ABI.
  __ popq(rsi);
  __ popq(rdi);
#endif
  __ popq(r15);
  __ popq(r14);
  __ popq(r13);
  __ popq(r12);
  __ addp(rsp, Immediate(2 * kPointerSize));  // remove markers

  // Restore frame pointer and return.
  __ popq(rbp);
  __ ret(0);
}

static int Offset(ExternalReference ref0, ExternalReference ref1) {
  int64_t offset = (ref0.address() - ref1.address());
  // Check that fits into int.
  DCHECK(static_cast<int>(offset) == offset);
  return static_cast<int>(offset);
}

// Calls an API function.  Allocates HandleScope, extracts returned value
// from handle and propagates exceptions.  Clobbers r14, r15, rbx and
// caller-save registers.  Restores context.  On return removes
// stack_space * kPointerSize (GCed).
static void CallApiFunctionAndReturn(MacroAssembler* masm,
                                     Register function_address,
                                     ExternalReference thunk_ref,
                                     Register thunk_last_arg, int stack_space,
                                     Operand* stack_space_operand,
                                     Operand return_value_operand) {
  Label prologue;
  Label promote_scheduled_exception;
  Label delete_allocated_handles;
  Label leave_exit_frame;
  Label write_back;

  Isolate* isolate = masm->isolate();
  Factory* factory = isolate->factory();
  ExternalReference next_address =
      ExternalReference::handle_scope_next_address(isolate);
  const int kNextOffset = 0;
  const int kLimitOffset = Offset(
      ExternalReference::handle_scope_limit_address(isolate), next_address);
  const int kLevelOffset = Offset(
      ExternalReference::handle_scope_level_address(isolate), next_address);
  ExternalReference scheduled_exception_address =
      ExternalReference::scheduled_exception_address(isolate);

  DCHECK(rdx == function_address || r8 == function_address);
  // Allocate HandleScope in callee-save registers.
  Register prev_next_address_reg = r14;
  Register prev_limit_reg = rbx;
  Register base_reg = r15;
  __ Move(base_reg, next_address);
  __ movp(prev_next_address_reg, Operand(base_reg, kNextOffset));
  __ movp(prev_limit_reg, Operand(base_reg, kLimitOffset));
  __ addl(Operand(base_reg, kLevelOffset), Immediate(1));

  if (FLAG_log_timer_events) {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ PushSafepointRegisters();
    __ PrepareCallCFunction(1);
    __ LoadAddress(arg_reg_1, ExternalReference::isolate_address(isolate));
    __ CallCFunction(ExternalReference::log_enter_external_function(), 1);
    __ PopSafepointRegisters();
  }

  Label profiler_disabled;
  Label end_profiler_check;
  __ Move(rax, ExternalReference::is_profiling_address(isolate));
  __ cmpb(Operand(rax, 0), Immediate(0));
  __ j(zero, &profiler_disabled);

  // Third parameter is the address of the actual getter function.
  __ Move(thunk_last_arg, function_address);
  __ Move(rax, thunk_ref);
  __ jmp(&end_profiler_check);

  __ bind(&profiler_disabled);
  // Call the api function!
  __ Move(rax, function_address);

  __ bind(&end_profiler_check);

  // Call the api function!
  __ call(rax);

  if (FLAG_log_timer_events) {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ PushSafepointRegisters();
    __ PrepareCallCFunction(1);
    __ LoadAddress(arg_reg_1, ExternalReference::isolate_address(isolate));
    __ CallCFunction(ExternalReference::log_leave_external_function(), 1);
    __ PopSafepointRegisters();
  }

  // Load the value from ReturnValue
  __ movp(rax, return_value_operand);
  __ bind(&prologue);

  // No more valid handles (the result handle was the last one). Restore
  // previous handle scope.
  __ subl(Operand(base_reg, kLevelOffset), Immediate(1));
  __ movp(Operand(base_reg, kNextOffset), prev_next_address_reg);
  __ cmpp(prev_limit_reg, Operand(base_reg, kLimitOffset));
  __ j(not_equal, &delete_allocated_handles);

  // Leave the API exit frame.
  __ bind(&leave_exit_frame);
  if (stack_space_operand != nullptr) {
    __ movp(rbx, *stack_space_operand);
  }
  __ LeaveApiExitFrame();

  // Check if the function scheduled an exception.
  __ Move(rdi, scheduled_exception_address);
  __ Cmp(Operand(rdi, 0), factory->the_hole_value());
  __ j(not_equal, &promote_scheduled_exception);

#if DEBUG
  // Check if the function returned a valid JavaScript value.
  Label ok;
  Register return_value = rax;
  Register map = rcx;

  __ JumpIfSmi(return_value, &ok, Label::kNear);
  __ movp(map, FieldOperand(return_value, HeapObject::kMapOffset));

  __ CmpInstanceType(map, LAST_NAME_TYPE);
  __ j(below_equal, &ok, Label::kNear);

  __ CmpInstanceType(map, FIRST_JS_RECEIVER_TYPE);
  __ j(above_equal, &ok, Label::kNear);

  __ CompareRoot(map, RootIndex::kHeapNumberMap);
  __ j(equal, &ok, Label::kNear);

  __ CompareRoot(return_value, RootIndex::kUndefinedValue);
  __ j(equal, &ok, Label::kNear);

  __ CompareRoot(return_value, RootIndex::kTrueValue);
  __ j(equal, &ok, Label::kNear);

  __ CompareRoot(return_value, RootIndex::kFalseValue);
  __ j(equal, &ok, Label::kNear);

  __ CompareRoot(return_value, RootIndex::kNullValue);
  __ j(equal, &ok, Label::kNear);

  __ Abort(AbortReason::kAPICallReturnedInvalidObject);

  __ bind(&ok);
#endif

  if (stack_space_operand != nullptr) {
    DCHECK_EQ(stack_space, 0);
    __ PopReturnAddressTo(rcx);
    __ addq(rsp, rbx);
    __ jmp(rcx);
  } else {
    __ ret(stack_space * kPointerSize);
  }

  // Re-throw by promoting a scheduled exception.
  __ bind(&promote_scheduled_exception);
  __ TailCallRuntime(Runtime::kPromoteScheduledException);

  // HandleScope limit has changed. Delete allocated extensions.
  __ bind(&delete_allocated_handles);
  __ movp(Operand(base_reg, kLimitOffset), prev_limit_reg);
  __ movp(prev_limit_reg, rax);
  __ LoadAddress(arg_reg_1, ExternalReference::isolate_address(isolate));
  __ LoadAddress(rax, ExternalReference::delete_handle_scope_extensions());
  __ call(rax);
  __ movp(rax, prev_limit_reg);
  __ jmp(&leave_exit_frame);
}

// TODO(jgruber): Instead of explicitly setting up implicit_args_ on the stack
// in CallApiCallback, we could use the calling convention to set up the stack
// correctly in the first place.
//
// TODO(jgruber): I suspect that most of CallApiCallback could be implemented
// as a C++ trampoline, vastly simplifying the assembly implementation.

void CallApiCallbackStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rsi                 : kTargetContext
  //  -- rdx                 : kApiFunctionAddress
  //  -- rcx                 : kArgc
  //  --
  //  -- rsp[0]              : return address
  //  -- rsp[8]              : last argument
  //  -- ...
  //  -- rsp[argc * 8]       : first argument
  //  -- rsp[(argc + 1) * 8] : receiver
  //  -- rsp[(argc + 2) * 8] : kHolder
  //  -- rsp[(argc + 3) * 8] : kCallData
  // -----------------------------------

  Register api_function_address = rdx;
  Register argc = rcx;

  DCHECK(!AreAliased(api_function_address, argc, kScratchRegister));

  // Stack offsets (without argc).
  static constexpr int kReceiverOffset = kPointerSize;
  static constexpr int kHolderOffset = kReceiverOffset + kPointerSize;
  static constexpr int kCallDataOffset = kHolderOffset + kPointerSize;

  // Extra stack arguments are: the receiver, kHolder, kCallData.
  static constexpr int kExtraStackArgumentCount = 3;

  typedef FunctionCallbackArguments FCA;

  STATIC_ASSERT(FCA::kArgsLength == 6);
  STATIC_ASSERT(FCA::kNewTargetIndex == 5);
  STATIC_ASSERT(FCA::kDataIndex == 4);
  STATIC_ASSERT(FCA::kReturnValueOffset == 3);
  STATIC_ASSERT(FCA::kReturnValueDefaultValueIndex == 2);
  STATIC_ASSERT(FCA::kIsolateIndex == 1);
  STATIC_ASSERT(FCA::kHolderIndex == 0);

  // Set up FunctionCallbackInfo's implicit_args on the stack as follows:
  //
  // Current state:
  //   rsp[0]: return address
  //
  // Target state:
  //   rsp[0 * kPointerSize]: return address
  //   rsp[1 * kPointerSize]: kHolder
  //   rsp[2 * kPointerSize]: kIsolate
  //   rsp[3 * kPointerSize]: undefined (kReturnValueDefaultValue)
  //   rsp[4 * kPointerSize]: undefined (kReturnValue)
  //   rsp[5 * kPointerSize]: kData
  //   rsp[6 * kPointerSize]: undefined (kNewTarget)

  // Reserve space on the stack.
  __ subp(rsp, Immediate(FCA::kArgsLength * kPointerSize));

  // Return address (the old stack location is overwritten later on).
  __ movp(kScratchRegister, Operand(rsp, FCA::kArgsLength * kPointerSize));
  __ movp(Operand(rsp, 0 * kPointerSize), kScratchRegister);

  // kHolder.
  __ movp(kScratchRegister,
          Operand(rsp, argc, times_pointer_size,
                  FCA::kArgsLength * kPointerSize + kHolderOffset));
  __ movp(Operand(rsp, 1 * kPointerSize), kScratchRegister);

  // kIsolate.
  __ Move(kScratchRegister,
          ExternalReference::isolate_address(masm->isolate()));
  __ movp(Operand(rsp, 2 * kPointerSize), kScratchRegister);

  // kReturnValueDefaultValue, kReturnValue, and kNewTarget.
  __ LoadRoot(kScratchRegister, RootIndex::kUndefinedValue);
  __ movp(Operand(rsp, 3 * kPointerSize), kScratchRegister);
  __ movp(Operand(rsp, 4 * kPointerSize), kScratchRegister);
  __ movp(Operand(rsp, 6 * kPointerSize), kScratchRegister);

  // kData.
  __ movp(kScratchRegister,
          Operand(rsp, argc, times_pointer_size,
                  FCA::kArgsLength * kPointerSize + kCallDataOffset));
  __ movp(Operand(rsp, 5 * kPointerSize), kScratchRegister);

  // Keep a pointer to kHolder (= implicit_args) in a scratch register.
  // We use it below to set up the FunctionCallbackInfo object.
  Register scratch = rbx;
  __ leap(scratch, Operand(rsp, 1 * kPointerSize));

  // Allocate the v8::Arguments structure in the arguments' space since
  // it's not controlled by GC.
  static constexpr int kApiStackSpace = 4;
  __ EnterApiExitFrame(kApiStackSpace);

  // FunctionCallbackInfo::implicit_args_ (points at kHolder as set up above).
  __ movp(StackSpaceOperand(0), scratch);

  // FunctionCallbackInfo::values_ (points at the first varargs argument passed
  // on the stack).
  __ leap(scratch, Operand(scratch, argc, times_pointer_size,
                           (FCA::kArgsLength - 1) * kPointerSize));
  __ movp(StackSpaceOperand(1), scratch);

  // FunctionCallbackInfo::length_.
  __ movp(StackSpaceOperand(2), argc);

  // We also store the number of bytes to drop from the stack after returning
  // from the API function here.
  __ leaq(
      kScratchRegister,
      Operand(argc, times_pointer_size,
              (FCA::kArgsLength + kExtraStackArgumentCount) * kPointerSize));
  __ movp(StackSpaceOperand(3), kScratchRegister);

  Register arguments_arg = arg_reg_1;
  Register callback_arg = arg_reg_2;

  // It's okay if api_function_address == callback_arg
  // but not arguments_arg
  DCHECK(api_function_address != arguments_arg);

  // v8::InvocationCallback's argument.
  __ leap(arguments_arg, StackSpaceOperand(0));

  ExternalReference thunk_ref = ExternalReference::invoke_function_callback();

  // There are two stack slots above the arguments we constructed on the stack:
  // the stored ebp (pushed by EnterApiExitFrame), and the return address.
  static constexpr int kStackSlotsAboveFCA = 2;
  Operand return_value_operand(
      rbp, (kStackSlotsAboveFCA + FCA::kReturnValueOffset) * kPointerSize);

  static constexpr int kUseStackSpaceOperand = 0;
  Operand stack_space_operand = StackSpaceOperand(3);
  CallApiFunctionAndReturn(masm, api_function_address, thunk_ref, callback_arg,
                           kUseStackSpaceOperand, &stack_space_operand,
                           return_value_operand);
}


void CallApiGetterStub::Generate(MacroAssembler* masm) {
#if defined(__MINGW64__) || defined(_WIN64)
  Register getter_arg = r8;
  Register accessor_info_arg = rdx;
  Register name_arg = rcx;
#else
  Register getter_arg = rdx;
  Register accessor_info_arg = rsi;
  Register name_arg = rdi;
#endif
  Register api_function_address = r8;
  Register receiver = ApiGetterDescriptor::ReceiverRegister();
  Register holder = ApiGetterDescriptor::HolderRegister();
  Register callback = ApiGetterDescriptor::CallbackRegister();
  Register scratch = rax;
  DCHECK(!AreAliased(receiver, holder, callback, scratch));

  // Build v8::PropertyCallbackInfo::args_ array on the stack and push property
  // name below the exit frame to make GC aware of them.
  STATIC_ASSERT(PropertyCallbackArguments::kShouldThrowOnErrorIndex == 0);
  STATIC_ASSERT(PropertyCallbackArguments::kHolderIndex == 1);
  STATIC_ASSERT(PropertyCallbackArguments::kIsolateIndex == 2);
  STATIC_ASSERT(PropertyCallbackArguments::kReturnValueDefaultValueIndex == 3);
  STATIC_ASSERT(PropertyCallbackArguments::kReturnValueOffset == 4);
  STATIC_ASSERT(PropertyCallbackArguments::kDataIndex == 5);
  STATIC_ASSERT(PropertyCallbackArguments::kThisIndex == 6);
  STATIC_ASSERT(PropertyCallbackArguments::kArgsLength == 7);

  // Insert additional parameters into the stack frame above return address.
  __ PopReturnAddressTo(scratch);
  __ Push(receiver);
  __ Push(FieldOperand(callback, AccessorInfo::kDataOffset));
  __ LoadRoot(kScratchRegister, RootIndex::kUndefinedValue);
  __ Push(kScratchRegister);  // return value
  __ Push(kScratchRegister);  // return value default
  __ PushAddress(ExternalReference::isolate_address(isolate()));
  __ Push(holder);
  __ Push(Smi::zero());  // should_throw_on_error -> false
  __ Push(FieldOperand(callback, AccessorInfo::kNameOffset));
  __ PushReturnAddressFrom(scratch);

  // v8::PropertyCallbackInfo::args_ array and name handle.
  const int kStackUnwindSpace = PropertyCallbackArguments::kArgsLength + 1;

  // Allocate v8::PropertyCallbackInfo in non-GCed stack space.
  const int kArgStackSpace = 1;

  // Load address of v8::PropertyAccessorInfo::args_ array.
  __ leap(scratch, Operand(rsp, 2 * kPointerSize));

  __ EnterApiExitFrame(kArgStackSpace);

  // Create v8::PropertyCallbackInfo object on the stack and initialize
  // it's args_ field.
  Operand info_object = StackSpaceOperand(0);
  __ movp(info_object, scratch);

  __ leap(name_arg, Operand(scratch, -kPointerSize));
  // The context register (rsi) has been saved in EnterApiExitFrame and
  // could be used to pass arguments.
  __ leap(accessor_info_arg, info_object);

  ExternalReference thunk_ref =
      ExternalReference::invoke_accessor_getter_callback();

  // It's okay if api_function_address == getter_arg
  // but not accessor_info_arg or name_arg
  DCHECK(api_function_address != accessor_info_arg);
  DCHECK(api_function_address != name_arg);
  __ movp(scratch, FieldOperand(callback, AccessorInfo::kJsGetterOffset));
  __ movp(api_function_address,
          FieldOperand(scratch, Foreign::kForeignAddressOffset));

  // +3 is to skip prolog, return address and name handle.
  Operand return_value_operand(
      rbp, (PropertyCallbackArguments::kReturnValueOffset + 3) * kPointerSize);
  Operand* const kUseStackSpaceConstant = nullptr;
  CallApiFunctionAndReturn(masm, api_function_address, thunk_ref, getter_arg,
                           kStackUnwindSpace, kUseStackSpaceConstant,
                           return_value_operand);
}

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_X64
