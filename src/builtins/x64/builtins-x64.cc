// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_X64

#include "src/api-arguments.h"
#include "src/base/adapters.h"
#include "src/code-factory.h"
#include "src/counters.h"
#include "src/deoptimizer.h"
#include "src/frame-constants.h"
#include "src/frames.h"
#include "src/objects-inl.h"
#include "src/objects/debug-objects.h"
#include "src/objects/js-generator.h"
#include "src/objects/smi.h"
#include "src/register-configuration.h"
#include "src/wasm/wasm-linkage.h"
#include "src/wasm/wasm-objects.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void Builtins::Generate_Adaptor(MacroAssembler* masm, Address address,
                                ExitFrameType exit_frame_type) {
  __ LoadAddress(kJavaScriptCallExtraArg1Register,
                 ExternalReference::Create(address));
  if (exit_frame_type == BUILTIN_EXIT) {
    __ Jump(BUILTIN_CODE(masm->isolate(), AdaptorWithBuiltinExitFrame),
            RelocInfo::CODE_TARGET);
  } else {
    DCHECK(exit_frame_type == EXIT);
    __ Jump(BUILTIN_CODE(masm->isolate(), AdaptorWithExitFrame),
            RelocInfo::CODE_TARGET);
  }
}

static void GenerateTailCallToReturnedCode(MacroAssembler* masm,
                                           Runtime::FunctionId function_id) {
  // ----------- S t a t e -------------
  //  -- rax : argument count (preserved for callee)
  //  -- rdx : new target (preserved for callee)
  //  -- rdi : target function (preserved for callee)
  // -----------------------------------
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Push the number of arguments to the callee.
    __ SmiTag(rax, rax);
    __ Push(rax);
    // Push a copy of the target function and the new target.
    __ Push(rdi);
    __ Push(rdx);
    // Function is also the parameter to the runtime call.
    __ Push(rdi);

    __ CallRuntime(function_id, 1);
    __ movp(rcx, rax);

    // Restore target function and new target.
    __ Pop(rdx);
    __ Pop(rdi);
    __ Pop(rax);
    __ SmiUntag(rax, rax);
  }
  static_assert(kJavaScriptCallCodeStartRegister == rcx, "ABI mismatch");
  __ leap(rcx, FieldOperand(rcx, Code::kHeaderSize));
  __ jmp(rcx);
}

namespace {

void Generate_JSBuiltinsConstructStubHelper(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax: number of arguments
  //  -- rdi: constructor function
  //  -- rdx: new target
  //  -- rsi: context
  // -----------------------------------

  // Enter a construct frame.
  {
    FrameScope scope(masm, StackFrame::CONSTRUCT);

    // Preserve the incoming parameters on the stack.
    __ SmiTag(rcx, rax);
    __ Push(rsi);
    __ Push(rcx);

    // The receiver for the builtin/api call.
    __ PushRoot(RootIndex::kTheHoleValue);

    // Set up pointer to last argument.
    __ leap(rbx, Operand(rbp, StandardFrameConstants::kCallerSPOffset));

    // Copy arguments and receiver to the expression stack.
    Label loop, entry;
    __ movp(rcx, rax);
    // ----------- S t a t e -------------
    //  --                rax: number of arguments (untagged)
    //  --                rdi: constructor function
    //  --                rdx: new target
    //  --                rbx: pointer to last argument
    //  --                rcx: counter
    //  -- sp[0*kPointerSize]: the hole (receiver)
    //  -- sp[1*kPointerSize]: number of arguments (tagged)
    //  -- sp[2*kPointerSize]: context
    // -----------------------------------
    __ jmp(&entry);
    __ bind(&loop);
    __ Push(Operand(rbx, rcx, times_pointer_size, 0));
    __ bind(&entry);
    __ decp(rcx);
    __ j(greater_equal, &loop, Label::kNear);

    // Call the function.
    // rax: number of arguments (untagged)
    // rdi: constructor function
    // rdx: new target
    ParameterCount actual(rax);
    __ InvokeFunction(rdi, rdx, actual, CALL_FUNCTION);

    // Restore context from the frame.
    __ movp(rsi, Operand(rbp, ConstructFrameConstants::kContextOffset));
    // Restore smi-tagged arguments count from the frame.
    __ movp(rbx, Operand(rbp, ConstructFrameConstants::kLengthOffset));

    // Leave construct frame.
  }

  // Remove caller arguments from the stack and return.
  __ PopReturnAddressTo(rcx);
  SmiIndex index = masm->SmiToIndex(rbx, rbx, kPointerSizeLog2);
  __ leap(rsp, Operand(rsp, index.reg, index.scale, 1 * kPointerSize));
  __ PushReturnAddressFrom(rcx);

  __ ret(0);
}

void Generate_StackOverflowCheck(
    MacroAssembler* masm, Register num_args, Register scratch,
    Label* stack_overflow,
    Label::Distance stack_overflow_distance = Label::kFar) {
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  __ LoadRoot(kScratchRegister, RootIndex::kRealStackLimit);
  __ movp(scratch, rsp);
  // Make scratch the space we have left. The stack might already be overflowed
  // here which will cause scratch to become negative.
  __ subp(scratch, kScratchRegister);
  __ sarp(scratch, Immediate(kPointerSizeLog2));
  // Check if the arguments will overflow the stack.
  __ cmpp(scratch, num_args);
  // Signed comparison.
  __ j(less_equal, stack_overflow, stack_overflow_distance);
}

}  // namespace

// The construct stub for ES5 constructor functions and ES6 class constructors.
void Builtins::Generate_JSConstructStubGeneric(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax: number of arguments (untagged)
  //  -- rdi: constructor function
  //  -- rdx: new target
  //  -- rsi: context
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  // Enter a construct frame.
  {
    FrameScope scope(masm, StackFrame::CONSTRUCT);
    Label post_instantiation_deopt_entry, not_create_implicit_receiver;

    // Preserve the incoming parameters on the stack.
    __ SmiTag(rcx, rax);
    __ Push(rsi);
    __ Push(rcx);
    __ Push(rdi);
    __ PushRoot(RootIndex::kTheHoleValue);
    __ Push(rdx);

    // ----------- S t a t e -------------
    //  --         sp[0*kPointerSize]: new target
    //  --         sp[1*kPointerSize]: padding
    //  -- rdi and sp[2*kPointerSize]: constructor function
    //  --         sp[3*kPointerSize]: argument count
    //  --         sp[4*kPointerSize]: context
    // -----------------------------------

    __ movp(rbx, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
    __ testl(FieldOperand(rbx, SharedFunctionInfo::kFlagsOffset),
             Immediate(SharedFunctionInfo::IsDerivedConstructorBit::kMask));
    __ j(not_zero, &not_create_implicit_receiver, Label::kNear);

    // If not derived class constructor: Allocate the new receiver object.
    __ IncrementCounter(masm->isolate()->counters()->constructed_objects(), 1);
    __ Call(BUILTIN_CODE(masm->isolate(), FastNewObject),
            RelocInfo::CODE_TARGET);
    __ jmp(&post_instantiation_deopt_entry, Label::kNear);

    // Else: use TheHoleValue as receiver for constructor call
    __ bind(&not_create_implicit_receiver);
    __ LoadRoot(rax, RootIndex::kTheHoleValue);

    // ----------- S t a t e -------------
    //  -- rax                          implicit receiver
    //  -- Slot 4 / sp[0*kPointerSize]  new target
    //  -- Slot 3 / sp[1*kPointerSize]  padding
    //  -- Slot 2 / sp[2*kPointerSize]  constructor function
    //  -- Slot 1 / sp[3*kPointerSize]  number of arguments (tagged)
    //  -- Slot 0 / sp[4*kPointerSize]  context
    // -----------------------------------
    // Deoptimizer enters here.
    masm->isolate()->heap()->SetConstructStubCreateDeoptPCOffset(
        masm->pc_offset());
    __ bind(&post_instantiation_deopt_entry);

    // Restore new target.
    __ Pop(rdx);

    // Push the allocated receiver to the stack. We need two copies
    // because we may have to return the original one and the calling
    // conventions dictate that the called function pops the receiver.
    __ Push(rax);
    __ Push(rax);

    // ----------- S t a t e -------------
    //  -- sp[0*kPointerSize]  implicit receiver
    //  -- sp[1*kPointerSize]  implicit receiver
    //  -- sp[2*kPointerSize]  padding
    //  -- sp[3*kPointerSize]  constructor function
    //  -- sp[4*kPointerSize]  number of arguments (tagged)
    //  -- sp[5*kPointerSize]  context
    // -----------------------------------

    // Restore constructor function and argument count.
    __ movp(rdi, Operand(rbp, ConstructFrameConstants::kConstructorOffset));
    __ SmiUntag(rax, Operand(rbp, ConstructFrameConstants::kLengthOffset));

    // Set up pointer to last argument.
    __ leap(rbx, Operand(rbp, StandardFrameConstants::kCallerSPOffset));

    // Check if we have enough stack space to push all arguments.
    // Argument count in rax. Clobbers rcx.
    Label enough_stack_space, stack_overflow;
    Generate_StackOverflowCheck(masm, rax, rcx, &stack_overflow, Label::kNear);
    __ jmp(&enough_stack_space, Label::kNear);

    __ bind(&stack_overflow);
    // Restore context from the frame.
    __ movp(rsi, Operand(rbp, ConstructFrameConstants::kContextOffset));
    __ CallRuntime(Runtime::kThrowStackOverflow);
    // This should be unreachable.
    __ int3();

    __ bind(&enough_stack_space);

    // Copy arguments and receiver to the expression stack.
    Label loop, entry;
    __ movp(rcx, rax);
    // ----------- S t a t e -------------
    //  --                        rax: number of arguments (untagged)
    //  --                        rdx: new target
    //  --                        rbx: pointer to last argument
    //  --                        rcx: counter (tagged)
    //  --         sp[0*kPointerSize]: implicit receiver
    //  --         sp[1*kPointerSize]: implicit receiver
    //  --         sp[2*kPointerSize]: padding
    //  -- rdi and sp[3*kPointerSize]: constructor function
    //  --         sp[4*kPointerSize]: number of arguments (tagged)
    //  --         sp[5*kPointerSize]: context
    // -----------------------------------
    __ jmp(&entry, Label::kNear);
    __ bind(&loop);
    __ Push(Operand(rbx, rcx, times_pointer_size, 0));
    __ bind(&entry);
    __ decp(rcx);
    __ j(greater_equal, &loop, Label::kNear);

    // Call the function.
    ParameterCount actual(rax);
    __ InvokeFunction(rdi, rdx, actual, CALL_FUNCTION);

    // ----------- S t a t e -------------
    //  -- rax                 constructor result
    //  -- sp[0*kPointerSize]  implicit receiver
    //  -- sp[1*kPointerSize]  padding
    //  -- sp[2*kPointerSize]  constructor function
    //  -- sp[3*kPointerSize]  number of arguments
    //  -- sp[4*kPointerSize]  context
    // -----------------------------------

    // Store offset of return address for deoptimizer.
    masm->isolate()->heap()->SetConstructStubInvokeDeoptPCOffset(
        masm->pc_offset());

    // Restore context from the frame.
    __ movp(rsi, Operand(rbp, ConstructFrameConstants::kContextOffset));

    // If the result is an object (in the ECMA sense), we should get rid
    // of the receiver and use the result; see ECMA-262 section 13.2.2-7
    // on page 74.
    Label use_receiver, do_throw, leave_frame;

    // If the result is undefined, we jump out to using the implicit receiver.
    __ JumpIfRoot(rax, RootIndex::kUndefinedValue, &use_receiver, Label::kNear);

    // Otherwise we do a smi check and fall through to check if the return value
    // is a valid receiver.

    // If the result is a smi, it is *not* an object in the ECMA sense.
    __ JumpIfSmi(rax, &use_receiver, Label::kNear);

    // If the type of the result (stored in its map) is less than
    // FIRST_JS_RECEIVER_TYPE, it is not an object in the ECMA sense.
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    __ CmpObjectType(rax, FIRST_JS_RECEIVER_TYPE, rcx);
    __ j(above_equal, &leave_frame, Label::kNear);
    __ jmp(&use_receiver, Label::kNear);

    __ bind(&do_throw);
    __ CallRuntime(Runtime::kThrowConstructorReturnedNonObject);

    // Throw away the result of the constructor invocation and use the
    // on-stack receiver as the result.
    __ bind(&use_receiver);
    __ movp(rax, Operand(rsp, 0 * kPointerSize));
    __ JumpIfRoot(rax, RootIndex::kTheHoleValue, &do_throw, Label::kNear);

    __ bind(&leave_frame);
    // Restore the arguments count.
    __ movp(rbx, Operand(rbp, ConstructFrameConstants::kLengthOffset));
    // Leave construct frame.
  }
  // Remove caller arguments from the stack and return.
  __ PopReturnAddressTo(rcx);
  SmiIndex index = masm->SmiToIndex(rbx, rbx, kPointerSizeLog2);
  __ leap(rsp, Operand(rsp, index.reg, index.scale, 1 * kPointerSize));
  __ PushReturnAddressFrom(rcx);
  __ ret(0);
}

void Builtins::Generate_JSBuiltinsConstructStub(MacroAssembler* masm) {
  Generate_JSBuiltinsConstructStubHelper(masm);
}

void Builtins::Generate_ConstructedNonConstructable(MacroAssembler* masm) {
  FrameScope scope(masm, StackFrame::INTERNAL);
  __ Push(rdi);
  __ CallRuntime(Runtime::kThrowConstructedNonConstructable);
}

namespace {

// Called with the native C calling convention. The corresponding function
// signature is:
//
//  using JSEntryFunction = GeneratedCode<Object*(
//      Object * new_target, Object * target, Object * receiver, int argc,
//      Object*** args, Address root_register_value)>;
void Generate_JSEntryVariant(MacroAssembler* masm, StackFrame::Type type,
                             Builtins::Name entry_trampoline) {
  Label invoke, handler_entry, exit;
  Label not_outermost_js, not_outermost_js_2;

  {  // NOLINT. Scope block confuses linter.
    NoRootArrayScope uninitialized_root_register(masm);
    // Set up frame.
    __ pushq(rbp);
    __ movp(rbp, rsp);

    // Push the stack frame type.
    __ Push(Immediate(StackFrame::TypeToMarker(type)));
    // Reserve a slot for the context. It is filled after the root register has
    // been set up.
    __ subp(rsp, Immediate(kPointerSize));
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
    // On Win64 XMM6-XMM15 are callee-save.
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
    STATIC_ASSERT(EntryFrameConstants::kCalleeSaveXMMRegisters == 10);
    STATIC_ASSERT(EntryFrameConstants::kXMMRegistersBlockSize ==
                  EntryFrameConstants::kXMMRegisterSize *
                      EntryFrameConstants::kCalleeSaveXMMRegisters);
#endif

#ifdef _WIN64
    // Initialize the root register.
    // C calling convention. The sixth argument is passed on the stack.
    __ movp(kRootRegister,
            Operand(rbp, EntryFrameConstants::kRootRegisterValueOffset));
#else
    // Initialize the root register.
    // C calling convention. The sixth argument is passed in r9.
    __ movp(kRootRegister, r9);
#endif
  }

  // Save copies of the top frame descriptor on the stack.
  ExternalReference c_entry_fp = ExternalReference::Create(
      IsolateAddressId::kCEntryFPAddress, masm->isolate());
  {
    Operand c_entry_fp_operand = masm->ExternalReferenceAsOperand(c_entry_fp);
    __ Push(c_entry_fp_operand);
  }

  // Store the context address in the previously-reserved slot.
  ExternalReference context_address = ExternalReference::Create(
      IsolateAddressId::kContextAddress, masm->isolate());
  __ Load(kScratchRegister, context_address);
  static constexpr int kOffsetToContextSlot = -2 * kPointerSize;
  __ movp(Operand(rbp, kOffsetToContextSlot), kScratchRegister);

  // If this is the outermost JS call, set js_entry_sp value.
  ExternalReference js_entry_sp = ExternalReference::Create(
      IsolateAddressId::kJSEntrySPAddress, masm->isolate());
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

  // Store the current pc as the handler offset. It's used later to create the
  // handler table.
  masm->isolate()->builtins()->SetJSEntryHandlerOffset(handler_entry.pos());

  // Caught exception: Store result (exception) in the pending exception
  // field in the JSEnv and return a failure sentinel.
  ExternalReference pending_exception = ExternalReference::Create(
      IsolateAddressId::kPendingExceptionAddress, masm->isolate());
  __ Store(pending_exception, rax);
  __ LoadRoot(rax, RootIndex::kException);
  __ jmp(&exit);

  // Invoke: Link this frame into the handler chain.
  __ bind(&invoke);
  __ PushStackHandler();

  // Invoke the function by calling through JS entry trampoline builtin and
  // pop the faked function when we return.
  Handle<Code> trampoline_code =
      masm->isolate()->builtins()->builtin_handle(entry_trampoline);
  __ Call(trampoline_code, RelocInfo::CODE_TARGET);

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

}  // namespace

void Builtins::Generate_JSEntry(MacroAssembler* masm) {
  Generate_JSEntryVariant(masm, StackFrame::ENTRY,
                          Builtins::kJSEntryTrampoline);
}

void Builtins::Generate_JSConstructEntry(MacroAssembler* masm) {
  Generate_JSEntryVariant(masm, StackFrame::CONSTRUCT_ENTRY,
                          Builtins::kJSConstructEntryTrampoline);
}

void Builtins::Generate_JSRunMicrotasksEntry(MacroAssembler* masm) {
  Generate_JSEntryVariant(masm, StackFrame::ENTRY, Builtins::kRunMicrotasks);
}

static void Generate_JSEntryTrampolineHelper(MacroAssembler* masm,
                                             bool is_construct) {
  // Expects five C++ function parameters.
  // - Object* new_target
  // - JSFunction function
  // - Object* receiver
  // - int argc
  // - Object*** argv
  // (see Handle::Invoke in execution.cc).

  // Open a C++ scope for the FrameScope.
  {
// Platform specific argument handling. After this, the stack contains
// an internal frame and the pushed function and receiver, and
// register rax and rbx holds the argument count and argument array,
// while rdi holds the function pointer, rsi the context, and rdx the
// new.target.

#ifdef _WIN64
    // MSVC parameters in:
    // rcx        : new_target
    // rdx        : function
    // r8         : receiver
    // r9         : argc
    // [rsp+0x20] : argv

    // Enter an internal frame.
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Setup the context (we need to use the caller context from the isolate).
    ExternalReference context_address = ExternalReference::Create(
        IsolateAddressId::kContextAddress, masm->isolate());
    __ movp(rsi, masm->ExternalReferenceAsOperand(context_address));

    // Push the function and the receiver onto the stack.
    __ Push(rdx);
    __ Push(r8);

    // Load the number of arguments and setup pointer to the arguments.
    __ movp(rax, r9);
    // Load the previous frame pointer to access C argument on stack
    __ movp(kScratchRegister, Operand(rbp, 0));
    __ movp(rbx, Operand(kScratchRegister, EntryFrameConstants::kArgvOffset));
    // Load the function pointer into rdi.
    __ movp(rdi, rdx);
    // Load the new.target into rdx.
    __ movp(rdx, rcx);
#else   // _WIN64
    // GCC parameters in:
    // rdi : new_target
    // rsi : function
    // rdx : receiver
    // rcx : argc
    // r8  : argv

    __ movp(r11, rdi);
    __ movp(rdi, rsi);
    // rdi : function
    // r11 : new_target

    // Clear the context before we push it when entering the internal frame.
    __ Set(rsi, 0);

    // Enter an internal frame.
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Setup the context (we need to use the caller context from the isolate).
    ExternalReference context_address = ExternalReference::Create(
        IsolateAddressId::kContextAddress, masm->isolate());
    __ movp(rsi, masm->ExternalReferenceAsOperand(context_address));

    // Push the function and receiver onto the stack.
    __ Push(rdi);
    __ Push(rdx);

    // Load the number of arguments and setup pointer to the arguments.
    __ movp(rax, rcx);
    __ movp(rbx, r8);

    // Load the new.target into rdx.
    __ movp(rdx, r11);
#endif  // _WIN64

    // Current stack contents:
    // [rsp + 2 * kPointerSize ... ] : Internal frame
    // [rsp + kPointerSize]          : function
    // [rsp]                         : receiver
    // Current register contents:
    // rax : argc
    // rbx : argv
    // rsi : context
    // rdi : function
    // rdx : new.target

    // Check if we have enough stack space to push all arguments.
    // Argument count in rax. Clobbers rcx.
    Label enough_stack_space, stack_overflow;
    Generate_StackOverflowCheck(masm, rax, rcx, &stack_overflow, Label::kNear);
    __ jmp(&enough_stack_space, Label::kNear);

    __ bind(&stack_overflow);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    // This should be unreachable.
    __ int3();

    __ bind(&enough_stack_space);

    // Copy arguments to the stack in a loop.
    // Register rbx points to array of pointers to handle locations.
    // Push the values of these handles.
    Label loop, entry;
    __ Set(rcx, 0);  // Set loop variable to 0.
    __ jmp(&entry, Label::kNear);
    __ bind(&loop);
    __ movp(kScratchRegister, Operand(rbx, rcx, times_pointer_size, 0));
    __ Push(Operand(kScratchRegister, 0));  // dereference handle
    __ addp(rcx, Immediate(1));
    __ bind(&entry);
    __ cmpp(rcx, rax);
    __ j(not_equal, &loop, Label::kNear);

    // Invoke the builtin code.
    Handle<Code> builtin = is_construct
                               ? BUILTIN_CODE(masm->isolate(), Construct)
                               : masm->isolate()->builtins()->Call();
    __ Call(builtin, RelocInfo::CODE_TARGET);

    // Exit the internal frame. Notice that this also removes the empty
    // context and the function left on the stack by the code
    // invocation.
  }

  __ ret(0);
}

void Builtins::Generate_JSEntryTrampoline(MacroAssembler* masm) {
  Generate_JSEntryTrampolineHelper(masm, false);
}

void Builtins::Generate_JSConstructEntryTrampoline(MacroAssembler* masm) {
  Generate_JSEntryTrampolineHelper(masm, true);
}

static void GetSharedFunctionInfoBytecode(MacroAssembler* masm,
                                          Register sfi_data,
                                          Register scratch1) {
  Label done;

  __ CmpObjectType(sfi_data, INTERPRETER_DATA_TYPE, scratch1);
  __ j(not_equal, &done, Label::kNear);
  __ movp(sfi_data,
          FieldOperand(sfi_data, InterpreterData::kBytecodeArrayOffset));

  __ bind(&done);
}

// static
void Builtins::Generate_ResumeGeneratorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : the value to pass to the generator
  //  -- rdx    : the JSGeneratorObject to resume
  //  -- rsp[0] : return address
  // -----------------------------------
  __ AssertGeneratorObject(rdx);

  // Store input value into generator object.
  __ movp(FieldOperand(rdx, JSGeneratorObject::kInputOrDebugPosOffset), rax);
  __ RecordWriteField(rdx, JSGeneratorObject::kInputOrDebugPosOffset, rax, rcx,
                      kDontSaveFPRegs);

  // Load suspended function and context.
  __ movp(rdi, FieldOperand(rdx, JSGeneratorObject::kFunctionOffset));
  __ movp(rsi, FieldOperand(rdi, JSFunction::kContextOffset));

  // Flood function if we are stepping.
  Label prepare_step_in_if_stepping, prepare_step_in_suspended_generator;
  Label stepping_prepared;
  ExternalReference debug_hook =
      ExternalReference::debug_hook_on_function_call_address(masm->isolate());
  Operand debug_hook_operand = masm->ExternalReferenceAsOperand(debug_hook);
  __ cmpb(debug_hook_operand, Immediate(0));
  __ j(not_equal, &prepare_step_in_if_stepping);

  // Flood function if we need to continue stepping in the suspended generator.
  ExternalReference debug_suspended_generator =
      ExternalReference::debug_suspended_generator_address(masm->isolate());
  Operand debug_suspended_generator_operand =
      masm->ExternalReferenceAsOperand(debug_suspended_generator);
  __ cmpp(rdx, debug_suspended_generator_operand);
  __ j(equal, &prepare_step_in_suspended_generator);
  __ bind(&stepping_prepared);

  // Check the stack for overflow. We are not trying to catch interruptions
  // (i.e. debug break and preemption) here, so check the "real stack limit".
  Label stack_overflow;
  __ CompareRoot(rsp, RootIndex::kRealStackLimit);
  __ j(below, &stack_overflow);

  // Pop return address.
  __ PopReturnAddressTo(rax);

  // Push receiver.
  __ Push(FieldOperand(rdx, JSGeneratorObject::kReceiverOffset));

  // ----------- S t a t e -------------
  //  -- rax    : return address
  //  -- rdx    : the JSGeneratorObject to resume
  //  -- rdi    : generator function
  //  -- rsi    : generator context
  //  -- rsp[0] : generator receiver
  // -----------------------------------

  // Copy the function arguments from the generator object's register file.
  __ movp(rcx, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
  __ movzxwq(
      rcx, FieldOperand(rcx, SharedFunctionInfo::kFormalParameterCountOffset));

  __ movp(rbx,
          FieldOperand(rdx, JSGeneratorObject::kParametersAndRegistersOffset));

  {
    Label done_loop, loop;
    __ Set(r9, 0);

    __ bind(&loop);
    __ cmpl(r9, rcx);
    __ j(greater_equal, &done_loop, Label::kNear);
    __ Push(FieldOperand(rbx, r9, times_pointer_size, FixedArray::kHeaderSize));
    __ addl(r9, Immediate(1));
    __ jmp(&loop);

    __ bind(&done_loop);
  }

  // Underlying function needs to have bytecode available.
  if (FLAG_debug_code) {
    __ movp(rcx, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
    __ movp(rcx, FieldOperand(rcx, SharedFunctionInfo::kFunctionDataOffset));
    GetSharedFunctionInfoBytecode(masm, rcx, kScratchRegister);
    __ CmpObjectType(rcx, BYTECODE_ARRAY_TYPE, rcx);
    __ Assert(equal, AbortReason::kMissingBytecodeArray);
  }

  // Resume (Ignition/TurboFan) generator object.
  {
    __ PushReturnAddressFrom(rax);
    __ movp(rax, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
    __ movzxwq(rax, FieldOperand(
                        rax, SharedFunctionInfo::kFormalParameterCountOffset));
    // We abuse new.target both to indicate that this is a resume call and to
    // pass in the generator object.  In ordinary calls, new.target is always
    // undefined because generator functions are non-constructable.
    static_assert(kJavaScriptCallCodeStartRegister == rcx, "ABI mismatch");
    __ movp(rcx, FieldOperand(rdi, JSFunction::kCodeOffset));
    __ addp(rcx, Immediate(Code::kHeaderSize - kHeapObjectTag));
    __ jmp(rcx);
  }

  __ bind(&prepare_step_in_if_stepping);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(rdx);
    __ Push(rdi);
    // Push hole as receiver since we do not use it for stepping.
    __ PushRoot(RootIndex::kTheHoleValue);
    __ CallRuntime(Runtime::kDebugOnFunctionCall);
    __ Pop(rdx);
    __ movp(rdi, FieldOperand(rdx, JSGeneratorObject::kFunctionOffset));
  }
  __ jmp(&stepping_prepared);

  __ bind(&prepare_step_in_suspended_generator);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(rdx);
    __ CallRuntime(Runtime::kDebugPrepareStepInSuspendedGenerator);
    __ Pop(rdx);
    __ movp(rdi, FieldOperand(rdx, JSGeneratorObject::kFunctionOffset));
  }
  __ jmp(&stepping_prepared);

  __ bind(&stack_overflow);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ int3();  // This should be unreachable.
  }
}

// TODO(juliana): if we remove the code below then we don't need all
// the parameters.
static void ReplaceClosureCodeWithOptimizedCode(
    MacroAssembler* masm, Register optimized_code, Register closure,
    Register scratch1, Register scratch2, Register scratch3) {

  // Store the optimized code in the closure.
  __ movp(FieldOperand(closure, JSFunction::kCodeOffset), optimized_code);
  __ movp(scratch1, optimized_code);  // Write barrier clobbers scratch1 below.
  __ RecordWriteField(closure, JSFunction::kCodeOffset, scratch1, scratch2,
                      kDontSaveFPRegs, OMIT_REMEMBERED_SET, OMIT_SMI_CHECK);
}

static void LeaveInterpreterFrame(MacroAssembler* masm, Register scratch1,
                                  Register scratch2) {
  Register args_count = scratch1;
  Register return_pc = scratch2;

  // Get the arguments + receiver count.
  __ movp(args_count,
          Operand(rbp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ movl(args_count,
          FieldOperand(args_count, BytecodeArray::kParameterSizeOffset));

  // Leave the frame (also dropping the register file).
  __ leave();

  // Drop receiver + arguments.
  __ PopReturnAddressTo(return_pc);
  __ addp(rsp, args_count);
  __ PushReturnAddressFrom(return_pc);
}

// Tail-call |function_id| if |smi_entry| == |marker|
static void TailCallRuntimeIfMarkerEquals(MacroAssembler* masm,
                                          Register smi_entry,
                                          OptimizationMarker marker,
                                          Runtime::FunctionId function_id) {
  Label no_match;
  __ SmiCompare(smi_entry, Smi::FromEnum(marker));
  __ j(not_equal, &no_match);
  GenerateTailCallToReturnedCode(masm, function_id);
  __ bind(&no_match);
}

static void MaybeTailCallOptimizedCodeSlot(MacroAssembler* masm,
                                           Register feedback_vector,
                                           Register scratch1, Register scratch2,
                                           Register scratch3) {
  // ----------- S t a t e -------------
  //  -- rax : argument count (preserved for callee if needed, and caller)
  //  -- rdx : new target (preserved for callee if needed, and caller)
  //  -- rdi : target function (preserved for callee if needed, and caller)
  //  -- feedback vector (preserved for caller if needed)
  // -----------------------------------
  DCHECK(!AreAliased(feedback_vector, rax, rdx, rdi, scratch1, scratch2,
                     scratch3));

  Label optimized_code_slot_is_weak_ref, fallthrough;

  Register closure = rdi;
  Register optimized_code_entry = scratch1;

  __ movp(optimized_code_entry,
          FieldOperand(feedback_vector, FeedbackVector::kOptimizedCodeOffset));

  // Check if the code entry is a Smi. If yes, we interpret it as an
  // optimisation marker. Otherwise, interpret it as a weak reference to a code
  // object.
  __ JumpIfNotSmi(optimized_code_entry, &optimized_code_slot_is_weak_ref);

  {
    // Optimized code slot is a Smi optimization marker.

    // Fall through if no optimization trigger.
    __ SmiCompare(optimized_code_entry,
                  Smi::FromEnum(OptimizationMarker::kNone));
    __ j(equal, &fallthrough);

    // TODO(v8:8394): The logging of first execution will break if
    // feedback vectors are not allocated. We need to find a different way of
    // logging these events if required.
    TailCallRuntimeIfMarkerEquals(masm, optimized_code_entry,
                                  OptimizationMarker::kLogFirstExecution,
                                  Runtime::kFunctionFirstExecution);
    TailCallRuntimeIfMarkerEquals(masm, optimized_code_entry,
                                  OptimizationMarker::kCompileOptimized,
                                  Runtime::kCompileOptimized_NotConcurrent);
    TailCallRuntimeIfMarkerEquals(
        masm, optimized_code_entry,
        OptimizationMarker::kCompileOptimizedConcurrent,
        Runtime::kCompileOptimized_Concurrent);

    {
      // Otherwise, the marker is InOptimizationQueue, so fall through hoping
      // that an interrupt will eventually update the slot with optimized code.
      if (FLAG_debug_code) {
        __ SmiCompare(optimized_code_entry,
                      Smi::FromEnum(OptimizationMarker::kInOptimizationQueue));
        __ Assert(equal, AbortReason::kExpectedOptimizationSentinel);
      }
      __ jmp(&fallthrough);
    }
  }

  {
    // Optimized code slot is a weak reference.
    __ bind(&optimized_code_slot_is_weak_ref);

    __ LoadWeakValue(optimized_code_entry, &fallthrough);

    // Check if the optimized code is marked for deopt. If it is, call the
    // runtime to clear it.
    Label found_deoptimized_code;
    __ movp(scratch2,
            FieldOperand(optimized_code_entry, Code::kCodeDataContainerOffset));
    __ testl(
        FieldOperand(scratch2, CodeDataContainer::kKindSpecificFlagsOffset),
        Immediate(1 << Code::kMarkedForDeoptimizationBit));
    __ j(not_zero, &found_deoptimized_code);

    // Optimized code is good, get it into the closure and link the closure into
    // the optimized functions list, then tail call the optimized code.
    // The feedback vector is no longer used, so re-use it as a scratch
    // register.
    ReplaceClosureCodeWithOptimizedCode(masm, optimized_code_entry, closure,
                                        scratch2, scratch3, feedback_vector);
    static_assert(kJavaScriptCallCodeStartRegister == rcx, "ABI mismatch");
    __ Move(rcx, optimized_code_entry);
    __ addp(rcx, Immediate(Code::kHeaderSize - kHeapObjectTag));
    __ jmp(rcx);

    // Optimized code slot contains deoptimized code, evict it and re-enter the
    // closure's code.
    __ bind(&found_deoptimized_code);
    GenerateTailCallToReturnedCode(masm, Runtime::kEvictOptimizedCodeSlot);
  }

  // Fall-through if the optimized code cell is clear and there is no
  // optimization marker.
  __ bind(&fallthrough);
}

// Advance the current bytecode offset. This simulates what all bytecode
// handlers do upon completion of the underlying operation. Will bail out to a
// label if the bytecode (without prefix) is a return bytecode.
static void AdvanceBytecodeOffsetOrReturn(MacroAssembler* masm,
                                          Register bytecode_array,
                                          Register bytecode_offset,
                                          Register bytecode, Register scratch1,
                                          Label* if_return) {
  Register bytecode_size_table = scratch1;
  DCHECK(!AreAliased(bytecode_array, bytecode_offset, bytecode_size_table,
                     bytecode));

  __ Move(bytecode_size_table,
          ExternalReference::bytecode_size_table_address());

  // Check if the bytecode is a Wide or ExtraWide prefix bytecode.
  Label process_bytecode, extra_wide;
  STATIC_ASSERT(0 == static_cast<int>(interpreter::Bytecode::kWide));
  STATIC_ASSERT(1 == static_cast<int>(interpreter::Bytecode::kExtraWide));
  STATIC_ASSERT(2 == static_cast<int>(interpreter::Bytecode::kDebugBreakWide));
  STATIC_ASSERT(3 ==
                static_cast<int>(interpreter::Bytecode::kDebugBreakExtraWide));
  __ cmpb(bytecode, Immediate(0x3));
  __ j(above, &process_bytecode, Label::kNear);
  __ testb(bytecode, Immediate(0x1));
  __ j(not_equal, &extra_wide, Label::kNear);

  // Load the next bytecode and update table to the wide scaled table.
  __ incl(bytecode_offset);
  __ movzxbp(bytecode, Operand(bytecode_array, bytecode_offset, times_1, 0));
  __ addp(bytecode_size_table,
          Immediate(kIntSize * interpreter::Bytecodes::kBytecodeCount));
  __ jmp(&process_bytecode, Label::kNear);

  __ bind(&extra_wide);
  // Load the next bytecode and update table to the extra wide scaled table.
  __ incl(bytecode_offset);
  __ movzxbp(bytecode, Operand(bytecode_array, bytecode_offset, times_1, 0));
  __ addp(bytecode_size_table,
          Immediate(2 * kIntSize * interpreter::Bytecodes::kBytecodeCount));

  __ bind(&process_bytecode);

// Bailout to the return label if this is a return bytecode.
#define JUMP_IF_EQUAL(NAME)                                             \
  __ cmpb(bytecode,                                                     \
          Immediate(static_cast<int>(interpreter::Bytecode::k##NAME))); \
  __ j(equal, if_return, Label::kFar);
  RETURN_BYTECODE_LIST(JUMP_IF_EQUAL)
#undef JUMP_IF_EQUAL

  // Otherwise, load the size of the current bytecode and advance the offset.
  __ addl(bytecode_offset, Operand(bytecode_size_table, bytecode, times_4, 0));
}

// Generate code for entering a JS function with the interpreter.
// On entry to the function the receiver and arguments have been pushed on the
// stack left to right.  The actual argument count matches the formal parameter
// count expected by the function.
//
// The live registers are:
//   o rdi: the JS function object being called
//   o rdx: the incoming new target or generator object
//   o rsi: our context
//   o rbp: the caller's frame pointer
//   o rsp: stack pointer (pointing to return address)
//
// The function builds an interpreter frame.  See InterpreterFrameConstants in
// frames.h for its layout.
void Builtins::Generate_InterpreterEntryTrampoline(MacroAssembler* masm) {
  Register closure = rdi;
  Register feedback_vector = rbx;

  // Get the bytecode array from the function object and load it into
  // kInterpreterBytecodeArrayRegister.
  __ movp(rax, FieldOperand(closure, JSFunction::kSharedFunctionInfoOffset));
  __ movp(kInterpreterBytecodeArrayRegister,
          FieldOperand(rax, SharedFunctionInfo::kFunctionDataOffset));
  GetSharedFunctionInfoBytecode(masm, kInterpreterBytecodeArrayRegister,
                                kScratchRegister);

  // The bytecode array could have been flushed from the shared function info,
  // if so, call into CompileLazy.
  Label compile_lazy;
  __ CmpObjectType(kInterpreterBytecodeArrayRegister, BYTECODE_ARRAY_TYPE, rax);
  __ j(not_equal, &compile_lazy);

  // Load the feedback vector from the closure.
  __ movp(feedback_vector,
          FieldOperand(closure, JSFunction::kFeedbackCellOffset));
  __ movp(feedback_vector, FieldOperand(feedback_vector, Cell::kValueOffset));

  Label push_stack_frame;
  // Check if feedback vector is valid. If valid, check for optimized code
  // and update invocation count. Otherwise, setup the stack frame.
  __ JumpIfRoot(feedback_vector, RootIndex::kUndefinedValue, &push_stack_frame);

  // Read off the optimized code slot in the feedback vector, and if there
  // is optimized code or an optimization marker, call that instead.
  MaybeTailCallOptimizedCodeSlot(masm, feedback_vector, rcx, r14, r15);

  // Increment invocation count for the function.
  __ incl(
      FieldOperand(feedback_vector, FeedbackVector::kInvocationCountOffset));

  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done below).
  __ bind(&push_stack_frame);
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ pushq(rbp);  // Caller's frame pointer.
  __ movp(rbp, rsp);
  __ Push(rsi);  // Callee's context.
  __ Push(rdi);  // Callee's JS function.

  // Reset code age.
  __ movb(FieldOperand(kInterpreterBytecodeArrayRegister,
                       BytecodeArray::kBytecodeAgeOffset),
          Immediate(BytecodeArray::kNoAgeBytecodeAge));

  // Load initial bytecode offset.
  __ movp(kInterpreterBytecodeOffsetRegister,
          Immediate(BytecodeArray::kHeaderSize - kHeapObjectTag));

  // Push bytecode array and Smi tagged bytecode offset.
  __ Push(kInterpreterBytecodeArrayRegister);
  __ SmiTag(rcx, kInterpreterBytecodeOffsetRegister);
  __ Push(rcx);

  // Allocate the local and temporary register file on the stack.
  {
    // Load frame size from the BytecodeArray object.
    __ movl(rcx, FieldOperand(kInterpreterBytecodeArrayRegister,
                              BytecodeArray::kFrameSizeOffset));

    // Do a stack check to ensure we don't go over the limit.
    Label ok;
    __ movp(rax, rsp);
    __ subp(rax, rcx);
    __ CompareRoot(rax, RootIndex::kRealStackLimit);
    __ j(above_equal, &ok, Label::kNear);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ bind(&ok);

    // If ok, push undefined as the initial value for all register file entries.
    Label loop_header;
    Label loop_check;
    __ LoadRoot(rax, RootIndex::kUndefinedValue);
    __ j(always, &loop_check, Label::kNear);
    __ bind(&loop_header);
    // TODO(rmcilroy): Consider doing more than one push per loop iteration.
    __ Push(rax);
    // Continue loop if not done.
    __ bind(&loop_check);
    __ subp(rcx, Immediate(kPointerSize));
    __ j(greater_equal, &loop_header, Label::kNear);
  }

  // If the bytecode array has a valid incoming new target or generator object
  // register, initialize it with incoming value which was passed in rdx.
  Label no_incoming_new_target_or_generator_register;
  __ movsxlq(
      rax,
      FieldOperand(kInterpreterBytecodeArrayRegister,
                   BytecodeArray::kIncomingNewTargetOrGeneratorRegisterOffset));
  __ testl(rax, rax);
  __ j(zero, &no_incoming_new_target_or_generator_register, Label::kNear);
  __ movp(Operand(rbp, rax, times_pointer_size, 0), rdx);
  __ bind(&no_incoming_new_target_or_generator_register);

  // Load accumulator with undefined.
  __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kUndefinedValue);

  // Load the dispatch table into a register and dispatch to the bytecode
  // handler at the current bytecode offset.
  Label do_dispatch;
  __ bind(&do_dispatch);
  __ Move(
      kInterpreterDispatchTableRegister,
      ExternalReference::interpreter_dispatch_table_address(masm->isolate()));
  __ movzxbp(r11, Operand(kInterpreterBytecodeArrayRegister,
                          kInterpreterBytecodeOffsetRegister, times_1, 0));
  __ movp(
      kJavaScriptCallCodeStartRegister,
      Operand(kInterpreterDispatchTableRegister, r11, times_pointer_size, 0));
  __ call(kJavaScriptCallCodeStartRegister);
  masm->isolate()->heap()->SetInterpreterEntryReturnPCOffset(masm->pc_offset());

  // Any returns to the entry trampoline are either due to the return bytecode
  // or the interpreter tail calling a builtin and then a dispatch.

  // Get bytecode array and bytecode offset from the stack frame.
  __ movp(kInterpreterBytecodeArrayRegister,
          Operand(rbp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ movp(kInterpreterBytecodeOffsetRegister,
          Operand(rbp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister,
              kInterpreterBytecodeOffsetRegister);

  // Either return, or advance to the next bytecode and dispatch.
  Label do_return;
  __ movzxbp(rbx, Operand(kInterpreterBytecodeArrayRegister,
                          kInterpreterBytecodeOffsetRegister, times_1, 0));
  AdvanceBytecodeOffsetOrReturn(masm, kInterpreterBytecodeArrayRegister,
                                kInterpreterBytecodeOffsetRegister, rbx, rcx,
                                &do_return);
  __ jmp(&do_dispatch);

  __ bind(&do_return);
  // The return value is in rax.
  LeaveInterpreterFrame(masm, rbx, rcx);
  __ ret(0);

  __ bind(&compile_lazy);
  GenerateTailCallToReturnedCode(masm, Runtime::kCompileLazy);
  __ int3();  // Should not return.
}

static void Generate_InterpreterPushArgs(MacroAssembler* masm,
                                         Register num_args,
                                         Register start_address,
                                         Register scratch) {
  // Find the address of the last argument.
  __ Move(scratch, num_args);
  __ shlp(scratch, Immediate(kPointerSizeLog2));
  __ negp(scratch);
  __ addp(scratch, start_address);

  // Push the arguments.
  Label loop_header, loop_check;
  __ j(always, &loop_check, Label::kNear);
  __ bind(&loop_header);
  __ Push(Operand(start_address, 0));
  __ subp(start_address, Immediate(kPointerSize));
  __ bind(&loop_check);
  __ cmpp(start_address, scratch);
  __ j(greater, &loop_header, Label::kNear);
}

// static
void Builtins::Generate_InterpreterPushArgsThenCallImpl(
    MacroAssembler* masm, ConvertReceiverMode receiver_mode,
    InterpreterPushArgsMode mode) {
  DCHECK(mode != InterpreterPushArgsMode::kArrayFunction);
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rbx : the address of the first argument to be pushed. Subsequent
  //           arguments should be consecutive above this, in the same order as
  //           they are to be pushed onto the stack.
  //  -- rdi : the target to call (can be any Object).
  // -----------------------------------
  Label stack_overflow;

  // Number of values to be pushed.
  __ leal(rcx, Operand(rax, 1));  // Add one for receiver.

  // Add a stack check before pushing arguments.
  Generate_StackOverflowCheck(masm, rcx, rdx, &stack_overflow);

  // Pop return address to allow tail-call after pushing arguments.
  __ PopReturnAddressTo(kScratchRegister);

  // Push "undefined" as the receiver arg if we need to.
  if (receiver_mode == ConvertReceiverMode::kNullOrUndefined) {
    __ PushRoot(RootIndex::kUndefinedValue);
    __ decl(rcx);  // Subtract one for receiver.
  }

  // rbx and rdx will be modified.
  Generate_InterpreterPushArgs(masm, rcx, rbx, rdx);

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    __ Pop(rbx);                 // Pass the spread in a register
    __ decl(rax);                // Subtract one for spread
  }

  // Call the target.
  __ PushReturnAddressFrom(kScratchRegister);  // Re-push return address.

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    __ Jump(BUILTIN_CODE(masm->isolate(), CallWithSpread),
            RelocInfo::CODE_TARGET);
  } else {
    __ Jump(masm->isolate()->builtins()->Call(receiver_mode),
            RelocInfo::CODE_TARGET);
  }

  // Throw stack overflow exception.
  __ bind(&stack_overflow);
  {
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    // This should be unreachable.
    __ int3();
  }
}

// static
void Builtins::Generate_InterpreterPushArgsThenConstructImpl(
    MacroAssembler* masm, InterpreterPushArgsMode mode) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdx : the new target (either the same as the constructor or
  //           the JSFunction on which new was invoked initially)
  //  -- rdi : the constructor to call (can be any Object)
  //  -- rbx : the allocation site feedback if available, undefined otherwise
  //  -- rcx : the address of the first argument to be pushed. Subsequent
  //           arguments should be consecutive above this, in the same order as
  //           they are to be pushed onto the stack.
  // -----------------------------------
  Label stack_overflow;

  // Add a stack check before pushing arguments.
  Generate_StackOverflowCheck(masm, rax, r8, &stack_overflow);

  // Pop return address to allow tail-call after pushing arguments.
  __ PopReturnAddressTo(kScratchRegister);

  // Push slot for the receiver to be constructed.
  __ Push(Immediate(0));

  // rcx and r8 will be modified.
  Generate_InterpreterPushArgs(masm, rax, rcx, r8);

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    __ Pop(rbx);                 // Pass the spread in a register
    __ decl(rax);                // Subtract one for spread

    // Push return address in preparation for the tail-call.
    __ PushReturnAddressFrom(kScratchRegister);
  } else {
    __ PushReturnAddressFrom(kScratchRegister);
    __ AssertUndefinedOrAllocationSite(rbx);
  }

  if (mode == InterpreterPushArgsMode::kArrayFunction) {
    // Tail call to the array construct stub (still in the caller
    // context at this point).
    __ AssertFunction(rdi);
    // Jump to the constructor function (rax, rbx, rdx passed on).
    Handle<Code> code = BUILTIN_CODE(masm->isolate(), ArrayConstructorImpl);
    __ Jump(code, RelocInfo::CODE_TARGET);
  } else if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    // Call the constructor (rax, rdx, rdi passed on).
    __ Jump(BUILTIN_CODE(masm->isolate(), ConstructWithSpread),
            RelocInfo::CODE_TARGET);
  } else {
    DCHECK_EQ(InterpreterPushArgsMode::kOther, mode);
    // Call the constructor (rax, rdx, rdi passed on).
    __ Jump(BUILTIN_CODE(masm->isolate(), Construct), RelocInfo::CODE_TARGET);
  }

  // Throw stack overflow exception.
  __ bind(&stack_overflow);
  {
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    // This should be unreachable.
    __ int3();
  }
}

static void Generate_InterpreterEnterBytecode(MacroAssembler* masm) {
  // Set the return address to the correct point in the interpreter entry
  // trampoline.
  Label builtin_trampoline, trampoline_loaded;
  Smi interpreter_entry_return_pc_offset(
      masm->isolate()->heap()->interpreter_entry_return_pc_offset());
  DCHECK_NE(interpreter_entry_return_pc_offset, Smi::kZero);

  // If the SFI function_data is an InterpreterData, the function will have a
  // custom copy of the interpreter entry trampoline for profiling. If so,
  // get the custom trampoline, otherwise grab the entry address of the global
  // trampoline.
  __ movp(rbx, Operand(rbp, StandardFrameConstants::kFunctionOffset));
  __ movp(rbx, FieldOperand(rbx, JSFunction::kSharedFunctionInfoOffset));
  __ movp(rbx, FieldOperand(rbx, SharedFunctionInfo::kFunctionDataOffset));
  __ CmpObjectType(rbx, INTERPRETER_DATA_TYPE, kScratchRegister);
  __ j(not_equal, &builtin_trampoline, Label::kNear);

  __ movp(rbx,
          FieldOperand(rbx, InterpreterData::kInterpreterTrampolineOffset));
  __ addp(rbx, Immediate(Code::kHeaderSize - kHeapObjectTag));
  __ jmp(&trampoline_loaded, Label::kNear);

  __ bind(&builtin_trampoline);
  __ movp(rbx,
          __ ExternalReferenceAsOperand(
              ExternalReference::
                  address_of_interpreter_entry_trampoline_instruction_start(
                      masm->isolate()),
              kScratchRegister));

  __ bind(&trampoline_loaded);
  __ addp(rbx, Immediate(interpreter_entry_return_pc_offset->value()));
  __ Push(rbx);

  // Initialize dispatch table register.
  __ Move(
      kInterpreterDispatchTableRegister,
      ExternalReference::interpreter_dispatch_table_address(masm->isolate()));

  // Get the bytecode array pointer from the frame.
  __ movp(kInterpreterBytecodeArrayRegister,
          Operand(rbp, InterpreterFrameConstants::kBytecodeArrayFromFp));

  if (FLAG_debug_code) {
    // Check function data field is actually a BytecodeArray object.
    __ AssertNotSmi(kInterpreterBytecodeArrayRegister);
    __ CmpObjectType(kInterpreterBytecodeArrayRegister, BYTECODE_ARRAY_TYPE,
                     rbx);
    __ Assert(
        equal,
        AbortReason::kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
  }

  // Get the target bytecode offset from the frame.
  __ movp(kInterpreterBytecodeOffsetRegister,
          Operand(rbp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister,
              kInterpreterBytecodeOffsetRegister);

  // Dispatch to the target bytecode.
  __ movzxbp(r11, Operand(kInterpreterBytecodeArrayRegister,
                          kInterpreterBytecodeOffsetRegister, times_1, 0));
  __ movp(
      kJavaScriptCallCodeStartRegister,
      Operand(kInterpreterDispatchTableRegister, r11, times_pointer_size, 0));
  __ jmp(kJavaScriptCallCodeStartRegister);
}

void Builtins::Generate_InterpreterEnterBytecodeAdvance(MacroAssembler* masm) {
  // Get bytecode array and bytecode offset from the stack frame.
  __ movp(kInterpreterBytecodeArrayRegister,
          Operand(rbp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ movp(kInterpreterBytecodeOffsetRegister,
          Operand(rbp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister,
              kInterpreterBytecodeOffsetRegister);

  // Load the current bytecode.
  __ movzxbp(rbx, Operand(kInterpreterBytecodeArrayRegister,
                          kInterpreterBytecodeOffsetRegister, times_1, 0));

  // Advance to the next bytecode.
  Label if_return;
  AdvanceBytecodeOffsetOrReturn(masm, kInterpreterBytecodeArrayRegister,
                                kInterpreterBytecodeOffsetRegister, rbx, rcx,
                                &if_return);

  // Convert new bytecode offset to a Smi and save in the stackframe.
  __ SmiTag(rbx, kInterpreterBytecodeOffsetRegister);
  __ movp(Operand(rbp, InterpreterFrameConstants::kBytecodeOffsetFromFp), rbx);

  Generate_InterpreterEnterBytecode(masm);

  // We should never take the if_return path.
  __ bind(&if_return);
  __ Abort(AbortReason::kInvalidBytecodeAdvance);
}

void Builtins::Generate_InterpreterEnterBytecodeDispatch(MacroAssembler* masm) {
  Generate_InterpreterEnterBytecode(masm);
}

void Builtins::Generate_InstantiateAsmJs(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : argument count (preserved for callee)
  //  -- rdx : new target (preserved for callee)
  //  -- rdi : target function (preserved for callee)
  // -----------------------------------
  Label failed;
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Preserve argument count for later compare.
    __ movp(rcx, rax);
    // Push the number of arguments to the callee.
    __ SmiTag(rax, rax);
    __ Push(rax);
    // Push a copy of the target function and the new target.
    __ Push(rdi);
    __ Push(rdx);

    // The function.
    __ Push(rdi);
    // Copy arguments from caller (stdlib, foreign, heap).
    Label args_done;
    for (int j = 0; j < 4; ++j) {
      Label over;
      if (j < 3) {
        __ cmpp(rcx, Immediate(j));
        __ j(not_equal, &over, Label::kNear);
      }
      for (int i = j - 1; i >= 0; --i) {
        __ Push(Operand(
            rbp, StandardFrameConstants::kCallerSPOffset + i * kPointerSize));
      }
      for (int i = 0; i < 3 - j; ++i) {
        __ PushRoot(RootIndex::kUndefinedValue);
      }
      if (j < 3) {
        __ jmp(&args_done, Label::kNear);
        __ bind(&over);
      }
    }
    __ bind(&args_done);

    // Call runtime, on success unwind frame, and parent frame.
    __ CallRuntime(Runtime::kInstantiateAsmJs, 4);
    // A smi 0 is returned on failure, an object on success.
    __ JumpIfSmi(rax, &failed, Label::kNear);

    __ Drop(2);
    __ Pop(rcx);
    __ SmiUntag(rcx, rcx);
    scope.GenerateLeaveFrame();

    __ PopReturnAddressTo(rbx);
    __ incp(rcx);
    __ leap(rsp, Operand(rsp, rcx, times_pointer_size, 0));
    __ PushReturnAddressFrom(rbx);
    __ ret(0);

    __ bind(&failed);
    // Restore target function and new target.
    __ Pop(rdx);
    __ Pop(rdi);
    __ Pop(rax);
    __ SmiUntag(rax, rax);
  }
  // On failure, tail call back to regular js by re-calling the function
  // which has be reset to the compile lazy builtin.
  __ movp(rcx, FieldOperand(rdi, JSFunction::kCodeOffset));
  __ addp(rcx, Immediate(Code::kHeaderSize - kHeapObjectTag));
  __ jmp(rcx);
}

namespace {
void Generate_ContinueToBuiltinHelper(MacroAssembler* masm,
                                      bool java_script_builtin,
                                      bool with_result) {
  const RegisterConfiguration* config(RegisterConfiguration::Default());
  int allocatable_register_count = config->num_allocatable_general_registers();
  if (with_result) {
    // Overwrite the hole inserted by the deoptimizer with the return value from
    // the LAZY deopt point.
    __ movq(Operand(rsp,
                    config->num_allocatable_general_registers() * kPointerSize +
                        BuiltinContinuationFrameConstants::kFixedFrameSize),
            rax);
  }
  for (int i = allocatable_register_count - 1; i >= 0; --i) {
    int code = config->GetAllocatableGeneralCode(i);
    __ popq(Register::from_code(code));
    if (java_script_builtin && code == kJavaScriptCallArgCountRegister.code()) {
      __ SmiUntag(Register::from_code(code), Register::from_code(code));
    }
  }
  __ movq(
      rbp,
      Operand(rsp, BuiltinContinuationFrameConstants::kFixedFrameSizeFromFp));
  const int offsetToPC =
      BuiltinContinuationFrameConstants::kFixedFrameSizeFromFp - kPointerSize;
  __ popq(Operand(rsp, offsetToPC));
  __ Drop(offsetToPC / kPointerSize);
  __ addq(Operand(rsp, 0), Immediate(Code::kHeaderSize - kHeapObjectTag));
  __ Ret();
}
}  // namespace

void Builtins::Generate_ContinueToCodeStubBuiltin(MacroAssembler* masm) {
  Generate_ContinueToBuiltinHelper(masm, false, false);
}

void Builtins::Generate_ContinueToCodeStubBuiltinWithResult(
    MacroAssembler* masm) {
  Generate_ContinueToBuiltinHelper(masm, false, true);
}

void Builtins::Generate_ContinueToJavaScriptBuiltin(MacroAssembler* masm) {
  Generate_ContinueToBuiltinHelper(masm, true, false);
}

void Builtins::Generate_ContinueToJavaScriptBuiltinWithResult(
    MacroAssembler* masm) {
  Generate_ContinueToBuiltinHelper(masm, true, true);
}

void Builtins::Generate_NotifyDeoptimized(MacroAssembler* masm) {
  // Enter an internal frame.
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kNotifyDeoptimized);
    // Tear down internal frame.
  }

  DCHECK_EQ(kInterpreterAccumulatorRegister.code(), rax.code());
  __ movp(rax, Operand(rsp, kPCOnStackSize));
  __ ret(1 * kPointerSize);  // Remove rax.
}

// static
void Builtins::Generate_FunctionPrototypeApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax     : argc
  //  -- rsp[0]  : return address
  //  -- rsp[8]  : argArray
  //  -- rsp[16] : thisArg
  //  -- rsp[24] : receiver
  // -----------------------------------

  // 1. Load receiver into rdi, argArray into rbx (if present), remove all
  // arguments from the stack (including the receiver), and push thisArg (if
  // present) instead.
  {
    Label no_arg_array, no_this_arg;
    StackArgumentsAccessor args(rsp, rax);
    __ LoadRoot(rdx, RootIndex::kUndefinedValue);
    __ movp(rbx, rdx);
    __ movp(rdi, args.GetReceiverOperand());
    __ testp(rax, rax);
    __ j(zero, &no_this_arg, Label::kNear);
    {
      __ movp(rdx, args.GetArgumentOperand(1));
      __ cmpp(rax, Immediate(1));
      __ j(equal, &no_arg_array, Label::kNear);
      __ movp(rbx, args.GetArgumentOperand(2));
      __ bind(&no_arg_array);
    }
    __ bind(&no_this_arg);
    __ PopReturnAddressTo(rcx);
    __ leap(rsp, Operand(rsp, rax, times_pointer_size, kPointerSize));
    __ Push(rdx);
    __ PushReturnAddressFrom(rcx);
  }

  // ----------- S t a t e -------------
  //  -- rbx     : argArray
  //  -- rdi     : receiver
  //  -- rsp[0]  : return address
  //  -- rsp[8]  : thisArg
  // -----------------------------------

  // 2. We don't need to check explicitly for callable receiver here,
  // since that's the first thing the Call/CallWithArrayLike builtins
  // will do.

  // 3. Tail call with no arguments if argArray is null or undefined.
  Label no_arguments;
  __ JumpIfRoot(rbx, RootIndex::kNullValue, &no_arguments, Label::kNear);
  __ JumpIfRoot(rbx, RootIndex::kUndefinedValue, &no_arguments, Label::kNear);

  // 4a. Apply the receiver to the given argArray.
  __ Jump(BUILTIN_CODE(masm->isolate(), CallWithArrayLike),
          RelocInfo::CODE_TARGET);

  // 4b. The argArray is either null or undefined, so we tail call without any
  // arguments to the receiver. Since we did not create a frame for
  // Function.prototype.apply() yet, we use a normal Call builtin here.
  __ bind(&no_arguments);
  {
    __ Set(rax, 0);
    __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
  }
}

// static
void Builtins::Generate_FunctionPrototypeCall(MacroAssembler* masm) {
  // Stack Layout:
  // rsp[0]           : Return address
  // rsp[8]           : Argument n
  // rsp[16]          : Argument n-1
  //  ...
  // rsp[8 * n]       : Argument 1
  // rsp[8 * (n + 1)] : Receiver (callable to call)
  //
  // rax contains the number of arguments, n, not counting the receiver.
  //
  // 1. Make sure we have at least one argument.
  {
    Label done;
    __ testp(rax, rax);
    __ j(not_zero, &done, Label::kNear);
    __ PopReturnAddressTo(rbx);
    __ PushRoot(RootIndex::kUndefinedValue);
    __ PushReturnAddressFrom(rbx);
    __ incp(rax);
    __ bind(&done);
  }

  // 2. Get the callable to call (passed as receiver) from the stack.
  {
    StackArgumentsAccessor args(rsp, rax);
    __ movp(rdi, args.GetReceiverOperand());
  }

  // 3. Shift arguments and return address one slot down on the stack
  //    (overwriting the original receiver).  Adjust argument count to make
  //    the original first argument the new receiver.
  {
    Label loop;
    __ movp(rcx, rax);
    StackArgumentsAccessor args(rsp, rcx);
    __ bind(&loop);
    __ movp(rbx, args.GetArgumentOperand(1));
    __ movp(args.GetArgumentOperand(0), rbx);
    __ decp(rcx);
    __ j(not_zero, &loop);              // While non-zero.
    __ DropUnderReturnAddress(1, rbx);  // Drop one slot under return address.
    __ decp(rax);  // One fewer argument (first argument is new receiver).
  }

  // 4. Call the callable.
  // Since we did not create a frame for Function.prototype.call() yet,
  // we use a normal Call builtin here.
  __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
}

void Builtins::Generate_ReflectApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax     : argc
  //  -- rsp[0]  : return address
  //  -- rsp[8]  : argumentsList
  //  -- rsp[16] : thisArgument
  //  -- rsp[24] : target
  //  -- rsp[32] : receiver
  // -----------------------------------

  // 1. Load target into rdi (if present), argumentsList into rbx (if present),
  // remove all arguments from the stack (including the receiver), and push
  // thisArgument (if present) instead.
  {
    Label done;
    StackArgumentsAccessor args(rsp, rax);
    __ LoadRoot(rdi, RootIndex::kUndefinedValue);
    __ movp(rdx, rdi);
    __ movp(rbx, rdi);
    __ cmpp(rax, Immediate(1));
    __ j(below, &done, Label::kNear);
    __ movp(rdi, args.GetArgumentOperand(1));  // target
    __ j(equal, &done, Label::kNear);
    __ movp(rdx, args.GetArgumentOperand(2));  // thisArgument
    __ cmpp(rax, Immediate(3));
    __ j(below, &done, Label::kNear);
    __ movp(rbx, args.GetArgumentOperand(3));  // argumentsList
    __ bind(&done);
    __ PopReturnAddressTo(rcx);
    __ leap(rsp, Operand(rsp, rax, times_pointer_size, kPointerSize));
    __ Push(rdx);
    __ PushReturnAddressFrom(rcx);
  }

  // ----------- S t a t e -------------
  //  -- rbx     : argumentsList
  //  -- rdi     : target
  //  -- rsp[0]  : return address
  //  -- rsp[8]  : thisArgument
  // -----------------------------------

  // 2. We don't need to check explicitly for callable target here,
  // since that's the first thing the Call/CallWithArrayLike builtins
  // will do.

  // 3. Apply the target to the given argumentsList.
  __ Jump(BUILTIN_CODE(masm->isolate(), CallWithArrayLike),
          RelocInfo::CODE_TARGET);
}

void Builtins::Generate_ReflectConstruct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax     : argc
  //  -- rsp[0]  : return address
  //  -- rsp[8]  : new.target (optional)
  //  -- rsp[16] : argumentsList
  //  -- rsp[24] : target
  //  -- rsp[32] : receiver
  // -----------------------------------

  // 1. Load target into rdi (if present), argumentsList into rbx (if present),
  // new.target into rdx (if present, otherwise use target), remove all
  // arguments from the stack (including the receiver), and push thisArgument
  // (if present) instead.
  {
    Label done;
    StackArgumentsAccessor args(rsp, rax);
    __ LoadRoot(rdi, RootIndex::kUndefinedValue);
    __ movp(rdx, rdi);
    __ movp(rbx, rdi);
    __ cmpp(rax, Immediate(1));
    __ j(below, &done, Label::kNear);
    __ movp(rdi, args.GetArgumentOperand(1));  // target
    __ movp(rdx, rdi);                         // new.target defaults to target
    __ j(equal, &done, Label::kNear);
    __ movp(rbx, args.GetArgumentOperand(2));  // argumentsList
    __ cmpp(rax, Immediate(3));
    __ j(below, &done, Label::kNear);
    __ movp(rdx, args.GetArgumentOperand(3));  // new.target
    __ bind(&done);
    __ PopReturnAddressTo(rcx);
    __ leap(rsp, Operand(rsp, rax, times_pointer_size, kPointerSize));
    __ PushRoot(RootIndex::kUndefinedValue);
    __ PushReturnAddressFrom(rcx);
  }

  // ----------- S t a t e -------------
  //  -- rbx     : argumentsList
  //  -- rdx     : new.target
  //  -- rdi     : target
  //  -- rsp[0]  : return address
  //  -- rsp[8]  : receiver (undefined)
  // -----------------------------------

  // 2. We don't need to check explicitly for constructor target here,
  // since that's the first thing the Construct/ConstructWithArrayLike
  // builtins will do.

  // 3. We don't need to check explicitly for constructor new.target here,
  // since that's the second thing the Construct/ConstructWithArrayLike
  // builtins will do.

  // 4. Construct the target with the given new.target and argumentsList.
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructWithArrayLike),
          RelocInfo::CODE_TARGET);
}

void Builtins::Generate_InternalArrayConstructor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : argc
  //  -- rsp[0] : return address
  //  -- rsp[8] : last argument
  // -----------------------------------
  Label generic_array_code;

  if (FLAG_debug_code) {
    // Initial map for the builtin InternalArray functions should be maps.
    __ movp(rbx, FieldOperand(rdi, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a nullptr and a Smi.
    STATIC_ASSERT(kSmiTag == 0);
    Condition not_smi = NegateCondition(masm->CheckSmi(rbx));
    __ Check(not_smi,
             AbortReason::kUnexpectedInitialMapForInternalArrayFunction);
    __ CmpObjectType(rbx, MAP_TYPE, rcx);
    __ Check(equal, AbortReason::kUnexpectedInitialMapForInternalArrayFunction);
  }

  // Run the native code for the InternalArray function called as a normal
  // function.
  __ Jump(BUILTIN_CODE(masm->isolate(), InternalArrayConstructorImpl),
          RelocInfo::CODE_TARGET);
}

static void EnterArgumentsAdaptorFrame(MacroAssembler* masm) {
  __ pushq(rbp);
  __ movp(rbp, rsp);

  // Store the arguments adaptor context sentinel.
  __ Push(Immediate(StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR)));

  // Push the function on the stack.
  __ Push(rdi);

  // Preserve the number of arguments on the stack. Must preserve rax,
  // rbx and rcx because these registers are used when copying the
  // arguments and the receiver.
  __ SmiTag(r8, rax);
  __ Push(r8);

  __ Push(Immediate(0));  // Padding.
}

static void LeaveArgumentsAdaptorFrame(MacroAssembler* masm) {
  // Retrieve the number of arguments from the stack. Number is a Smi.
  __ movp(rbx, Operand(rbp, ArgumentsAdaptorFrameConstants::kLengthOffset));

  // Leave the frame.
  __ movp(rsp, rbp);
  __ popq(rbp);

  // Remove caller arguments from the stack.
  __ PopReturnAddressTo(rcx);
  SmiIndex index = masm->SmiToIndex(rbx, rbx, kPointerSizeLog2);
  __ leap(rsp, Operand(rsp, index.reg, index.scale, 1 * kPointerSize));
  __ PushReturnAddressFrom(rcx);
}

void Builtins::Generate_ArgumentsAdaptorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : actual number of arguments
  //  -- rbx : expected number of arguments
  //  -- rdx : new target (passed through to callee)
  //  -- rdi : function (passed through to callee)
  // -----------------------------------

  Label invoke, dont_adapt_arguments, stack_overflow, enough, too_few;
  __ cmpp(rbx, Immediate(SharedFunctionInfo::kDontAdaptArgumentsSentinel));
  __ j(equal, &dont_adapt_arguments);
  __ cmpp(rax, rbx);
  __ j(less, &too_few);

  {  // Enough parameters: Actual >= expected.
    __ bind(&enough);
    EnterArgumentsAdaptorFrame(masm);
    // The registers rcx and r8 will be modified. The register rbx is only read.
    Generate_StackOverflowCheck(masm, rbx, rcx, &stack_overflow);

    // Copy receiver and all expected arguments.
    const int offset = StandardFrameConstants::kCallerSPOffset;
    __ leap(rax, Operand(rbp, rax, times_pointer_size, offset));
    __ Set(r8, -1);  // account for receiver

    Label copy;
    __ bind(&copy);
    __ incp(r8);
    __ Push(Operand(rax, 0));
    __ subp(rax, Immediate(kPointerSize));
    __ cmpp(r8, rbx);
    __ j(less, &copy);
    __ jmp(&invoke);
  }

  {  // Too few parameters: Actual < expected.
    __ bind(&too_few);

    EnterArgumentsAdaptorFrame(masm);
    // The registers rcx and r8 will be modified. The register rbx is only read.
    Generate_StackOverflowCheck(masm, rbx, rcx, &stack_overflow);

    // Copy receiver and all actual arguments.
    const int offset = StandardFrameConstants::kCallerSPOffset;
    __ leap(rdi, Operand(rbp, rax, times_pointer_size, offset));
    __ Set(r8, -1);  // account for receiver

    Label copy;
    __ bind(&copy);
    __ incp(r8);
    __ Push(Operand(rdi, 0));
    __ subp(rdi, Immediate(kPointerSize));
    __ cmpp(r8, rax);
    __ j(less, &copy);

    // Fill remaining expected arguments with undefined values.
    Label fill;
    __ LoadRoot(kScratchRegister, RootIndex::kUndefinedValue);
    __ bind(&fill);
    __ incp(r8);
    __ Push(kScratchRegister);
    __ cmpp(r8, rbx);
    __ j(less, &fill);

    // Restore function pointer.
    __ movp(rdi, Operand(rbp, ArgumentsAdaptorFrameConstants::kFunctionOffset));
  }

  // Call the entry point.
  __ bind(&invoke);
  __ movp(rax, rbx);
  // rax : expected number of arguments
  // rdx : new target (passed through to callee)
  // rdi : function (passed through to callee)
  static_assert(kJavaScriptCallCodeStartRegister == rcx, "ABI mismatch");
  __ movp(rcx, FieldOperand(rdi, JSFunction::kCodeOffset));
  __ addp(rcx, Immediate(Code::kHeaderSize - kHeapObjectTag));
  __ call(rcx);

  // Store offset of return address for deoptimizer.
  masm->isolate()->heap()->SetArgumentsAdaptorDeoptPCOffset(masm->pc_offset());

  // Leave frame and return.
  LeaveArgumentsAdaptorFrame(masm);
  __ ret(0);

  // -------------------------------------------
  // Dont adapt arguments.
  // -------------------------------------------
  __ bind(&dont_adapt_arguments);
  static_assert(kJavaScriptCallCodeStartRegister == rcx, "ABI mismatch");
  __ movp(rcx, FieldOperand(rdi, JSFunction::kCodeOffset));
  __ addp(rcx, Immediate(Code::kHeaderSize - kHeapObjectTag));
  __ jmp(rcx);

  __ bind(&stack_overflow);
  {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ int3();
  }
}

// static
void Builtins::Generate_CallOrConstructVarargs(MacroAssembler* masm,
                                               Handle<Code> code) {
  // ----------- S t a t e -------------
  //  -- rdi    : target
  //  -- rax    : number of parameters on the stack (not including the receiver)
  //  -- rbx    : arguments list (a FixedArray)
  //  -- rcx    : len (number of elements to push from args)
  //  -- rdx    : new.target (for [[Construct]])
  //  -- rsp[0] : return address
  // -----------------------------------
  if (masm->emit_debug_code()) {
    // Allow rbx to be a FixedArray, or a FixedDoubleArray if rcx == 0.
    Label ok, fail;
    __ AssertNotSmi(rbx);
    Register map = r9;
    __ movp(map, FieldOperand(rbx, HeapObject::kMapOffset));
    __ CmpInstanceType(map, FIXED_ARRAY_TYPE);
    __ j(equal, &ok);
    __ CmpInstanceType(map, FIXED_DOUBLE_ARRAY_TYPE);
    __ j(not_equal, &fail);
    __ cmpl(rcx, Immediate(0));
    __ j(equal, &ok);
    // Fall through.
    __ bind(&fail);
    __ Abort(AbortReason::kOperandIsNotAFixedArray);

    __ bind(&ok);
  }

  Label stack_overflow;
  Generate_StackOverflowCheck(masm, rcx, r8, &stack_overflow, Label::kNear);

  // Push additional arguments onto the stack.
  {
    __ PopReturnAddressTo(r8);
    __ Set(r9, 0);
    Label done, push, loop;
    __ bind(&loop);
    __ cmpl(r9, rcx);
    __ j(equal, &done, Label::kNear);
    // Turn the hole into undefined as we go.
    __ movp(r11,
            FieldOperand(rbx, r9, times_pointer_size, FixedArray::kHeaderSize));
    __ CompareRoot(r11, RootIndex::kTheHoleValue);
    __ j(not_equal, &push, Label::kNear);
    __ LoadRoot(r11, RootIndex::kUndefinedValue);
    __ bind(&push);
    __ Push(r11);
    __ incl(r9);
    __ jmp(&loop);
    __ bind(&done);
    __ PushReturnAddressFrom(r8);
    __ addq(rax, r9);
  }

  // Tail-call to the actual Call or Construct builtin.
  __ Jump(code, RelocInfo::CODE_TARGET);

  __ bind(&stack_overflow);
  __ TailCallRuntime(Runtime::kThrowStackOverflow);
}

// static
void Builtins::Generate_CallOrConstructForwardVarargs(MacroAssembler* masm,
                                                      CallOrConstructMode mode,
                                                      Handle<Code> code) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdx : the new target (for [[Construct]] calls)
  //  -- rdi : the target to call (can be any Object)
  //  -- rcx : start index (to support rest parameters)
  // -----------------------------------

  // Check if new.target has a [[Construct]] internal method.
  if (mode == CallOrConstructMode::kConstruct) {
    Label new_target_constructor, new_target_not_constructor;
    __ JumpIfSmi(rdx, &new_target_not_constructor, Label::kNear);
    __ movp(rbx, FieldOperand(rdx, HeapObject::kMapOffset));
    __ testb(FieldOperand(rbx, Map::kBitFieldOffset),
             Immediate(Map::IsConstructorBit::kMask));
    __ j(not_zero, &new_target_constructor, Label::kNear);
    __ bind(&new_target_not_constructor);
    {
      FrameScope scope(masm, StackFrame::MANUAL);
      __ EnterFrame(StackFrame::INTERNAL);
      __ Push(rdx);
      __ CallRuntime(Runtime::kThrowNotConstructor);
    }
    __ bind(&new_target_constructor);
  }

  // Check if we have an arguments adaptor frame below the function frame.
  Label arguments_adaptor, arguments_done;
  __ movp(rbx, Operand(rbp, StandardFrameConstants::kCallerFPOffset));
  __ cmpp(Operand(rbx, CommonFrameConstants::kContextOrFrameTypeOffset),
          Immediate(StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR)));
  __ j(equal, &arguments_adaptor, Label::kNear);
  {
    __ movp(r8, Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
    __ movp(r8, FieldOperand(r8, JSFunction::kSharedFunctionInfoOffset));
    __ movzxwq(
        r8, FieldOperand(r8, SharedFunctionInfo::kFormalParameterCountOffset));
    __ movp(rbx, rbp);
  }
  __ jmp(&arguments_done, Label::kNear);
  __ bind(&arguments_adaptor);
  {
    __ SmiUntag(r8,
                Operand(rbx, ArgumentsAdaptorFrameConstants::kLengthOffset));
  }
  __ bind(&arguments_done);

  Label stack_done, stack_overflow;
  __ subl(r8, rcx);
  __ j(less_equal, &stack_done);
  {
    // Check for stack overflow.
    Generate_StackOverflowCheck(masm, r8, rcx, &stack_overflow, Label::kNear);

    // Forward the arguments from the caller frame.
    {
      Label loop;
      __ addl(rax, r8);
      __ PopReturnAddressTo(rcx);
      __ bind(&loop);
      {
        StackArgumentsAccessor args(rbx, r8, ARGUMENTS_DONT_CONTAIN_RECEIVER);
        __ Push(args.GetArgumentOperand(0));
        __ decl(r8);
        __ j(not_zero, &loop);
      }
      __ PushReturnAddressFrom(rcx);
    }
  }
  __ jmp(&stack_done, Label::kNear);
  __ bind(&stack_overflow);
  __ TailCallRuntime(Runtime::kThrowStackOverflow);
  __ bind(&stack_done);

  // Tail-call to the {code} handler.
  __ Jump(code, RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_CallFunction(MacroAssembler* masm,
                                     ConvertReceiverMode mode) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdi : the function to call (checked to be a JSFunction)
  // -----------------------------------
  StackArgumentsAccessor args(rsp, rax);
  __ AssertFunction(rdi);

  // ES6 section 9.2.1 [[Call]] ( thisArgument, argumentsList)
  // Check that the function is not a "classConstructor".
  Label class_constructor;
  __ movp(rdx, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
  __ testl(FieldOperand(rdx, SharedFunctionInfo::kFlagsOffset),
           Immediate(SharedFunctionInfo::IsClassConstructorBit::kMask));
  __ j(not_zero, &class_constructor);

  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdx : the shared function info.
  //  -- rdi : the function to call (checked to be a JSFunction)
  // -----------------------------------

  // Enter the context of the function; ToObject has to run in the function
  // context, and we also need to take the global proxy from the function
  // context in case of conversion.
  __ movp(rsi, FieldOperand(rdi, JSFunction::kContextOffset));
  // We need to convert the receiver for non-native sloppy mode functions.
  Label done_convert;
  __ testl(FieldOperand(rdx, SharedFunctionInfo::kFlagsOffset),
           Immediate(SharedFunctionInfo::IsNativeBit::kMask |
                     SharedFunctionInfo::IsStrictBit::kMask));
  __ j(not_zero, &done_convert);
  {
    // ----------- S t a t e -------------
    //  -- rax : the number of arguments (not including the receiver)
    //  -- rdx : the shared function info.
    //  -- rdi : the function to call (checked to be a JSFunction)
    //  -- rsi : the function context.
    // -----------------------------------

    if (mode == ConvertReceiverMode::kNullOrUndefined) {
      // Patch receiver to global proxy.
      __ LoadGlobalProxy(rcx);
    } else {
      Label convert_to_object, convert_receiver;
      __ movp(rcx, args.GetReceiverOperand());
      __ JumpIfSmi(rcx, &convert_to_object, Label::kNear);
      STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
      __ CmpObjectType(rcx, FIRST_JS_RECEIVER_TYPE, rbx);
      __ j(above_equal, &done_convert);
      if (mode != ConvertReceiverMode::kNotNullOrUndefined) {
        Label convert_global_proxy;
        __ JumpIfRoot(rcx, RootIndex::kUndefinedValue, &convert_global_proxy,
                      Label::kNear);
        __ JumpIfNotRoot(rcx, RootIndex::kNullValue, &convert_to_object,
                         Label::kNear);
        __ bind(&convert_global_proxy);
        {
          // Patch receiver to global proxy.
          __ LoadGlobalProxy(rcx);
        }
        __ jmp(&convert_receiver);
      }
      __ bind(&convert_to_object);
      {
        // Convert receiver using ToObject.
        // TODO(bmeurer): Inline the allocation here to avoid building the frame
        // in the fast case? (fall back to AllocateInNewSpace?)
        FrameScope scope(masm, StackFrame::INTERNAL);
        __ SmiTag(rax, rax);
        __ Push(rax);
        __ Push(rdi);
        __ movp(rax, rcx);
        __ Push(rsi);
        __ Call(BUILTIN_CODE(masm->isolate(), ToObject),
                RelocInfo::CODE_TARGET);
        __ Pop(rsi);
        __ movp(rcx, rax);
        __ Pop(rdi);
        __ Pop(rax);
        __ SmiUntag(rax, rax);
      }
      __ movp(rdx, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
      __ bind(&convert_receiver);
    }
    __ movp(args.GetReceiverOperand(), rcx);
  }
  __ bind(&done_convert);

  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdx : the shared function info.
  //  -- rdi : the function to call (checked to be a JSFunction)
  //  -- rsi : the function context.
  // -----------------------------------

  __ movzxwq(
      rbx, FieldOperand(rdx, SharedFunctionInfo::kFormalParameterCountOffset));
  ParameterCount actual(rax);
  ParameterCount expected(rbx);

  __ InvokeFunctionCode(rdi, no_reg, expected, actual, JUMP_FUNCTION);

  // The function is a "classConstructor", need to raise an exception.
  __ bind(&class_constructor);
  {
    FrameScope frame(masm, StackFrame::INTERNAL);
    __ Push(rdi);
    __ CallRuntime(Runtime::kThrowConstructorNonCallableError);
  }
}

namespace {

void Generate_PushBoundArguments(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdx : new.target (only in case of [[Construct]])
  //  -- rdi : target (checked to be a JSBoundFunction)
  // -----------------------------------

  // Load [[BoundArguments]] into rcx and length of that into rbx.
  Label no_bound_arguments;
  __ movp(rcx, FieldOperand(rdi, JSBoundFunction::kBoundArgumentsOffset));
  __ SmiUntag(rbx, FieldOperand(rcx, FixedArray::kLengthOffset));
  __ testl(rbx, rbx);
  __ j(zero, &no_bound_arguments);
  {
    // ----------- S t a t e -------------
    //  -- rax : the number of arguments (not including the receiver)
    //  -- rdx : new.target (only in case of [[Construct]])
    //  -- rdi : target (checked to be a JSBoundFunction)
    //  -- rcx : the [[BoundArguments]] (implemented as FixedArray)
    //  -- rbx : the number of [[BoundArguments]] (checked to be non-zero)
    // -----------------------------------

    // Reserve stack space for the [[BoundArguments]].
    {
      Label done;
      __ leap(kScratchRegister, Operand(rbx, times_pointer_size, 0));
      __ subp(rsp, kScratchRegister);
      // Check the stack for overflow. We are not trying to catch interruptions
      // (i.e. debug break and preemption) here, so check the "real stack
      // limit".
      __ CompareRoot(rsp, RootIndex::kRealStackLimit);
      __ j(above_equal, &done, Label::kNear);
      // Restore the stack pointer.
      __ leap(rsp, Operand(rsp, rbx, times_pointer_size, 0));
      {
        FrameScope scope(masm, StackFrame::MANUAL);
        __ EnterFrame(StackFrame::INTERNAL);
        __ CallRuntime(Runtime::kThrowStackOverflow);
      }
      __ bind(&done);
    }

    // Adjust effective number of arguments to include return address.
    __ incl(rax);

    // Relocate arguments and return address down the stack.
    {
      Label loop;
      __ Set(rcx, 0);
      __ leap(rbx, Operand(rsp, rbx, times_pointer_size, 0));
      __ bind(&loop);
      __ movp(kScratchRegister, Operand(rbx, rcx, times_pointer_size, 0));
      __ movp(Operand(rsp, rcx, times_pointer_size, 0), kScratchRegister);
      __ incl(rcx);
      __ cmpl(rcx, rax);
      __ j(less, &loop);
    }

    // Copy [[BoundArguments]] to the stack (below the arguments).
    {
      Label loop;
      __ movp(rcx, FieldOperand(rdi, JSBoundFunction::kBoundArgumentsOffset));
      __ SmiUntag(rbx, FieldOperand(rcx, FixedArray::kLengthOffset));
      __ bind(&loop);
      __ decl(rbx);
      __ movp(kScratchRegister, FieldOperand(rcx, rbx, times_pointer_size,
                                             FixedArray::kHeaderSize));
      __ movp(Operand(rsp, rax, times_pointer_size, 0), kScratchRegister);
      __ leal(rax, Operand(rax, 1));
      __ j(greater, &loop);
    }

    // Adjust effective number of arguments (rax contains the number of
    // arguments from the call plus return address plus the number of
    // [[BoundArguments]]), so we need to subtract one for the return address.
    __ decl(rax);
  }
  __ bind(&no_bound_arguments);
}

}  // namespace

// static
void Builtins::Generate_CallBoundFunctionImpl(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdi : the function to call (checked to be a JSBoundFunction)
  // -----------------------------------
  __ AssertBoundFunction(rdi);

  // Patch the receiver to [[BoundThis]].
  StackArgumentsAccessor args(rsp, rax);
  __ movp(rbx, FieldOperand(rdi, JSBoundFunction::kBoundThisOffset));
  __ movp(args.GetReceiverOperand(), rbx);

  // Push the [[BoundArguments]] onto the stack.
  Generate_PushBoundArguments(masm);

  // Call the [[BoundTargetFunction]] via the Call builtin.
  __ movp(rdi, FieldOperand(rdi, JSBoundFunction::kBoundTargetFunctionOffset));
  __ Jump(BUILTIN_CODE(masm->isolate(), Call_ReceiverIsAny),
          RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_Call(MacroAssembler* masm, ConvertReceiverMode mode) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdi : the target to call (can be any Object)
  // -----------------------------------
  StackArgumentsAccessor args(rsp, rax);

  Label non_callable;
  __ JumpIfSmi(rdi, &non_callable);
  __ CmpObjectType(rdi, JS_FUNCTION_TYPE, rcx);
  __ Jump(masm->isolate()->builtins()->CallFunction(mode),
          RelocInfo::CODE_TARGET, equal);

  __ CmpInstanceType(rcx, JS_BOUND_FUNCTION_TYPE);
  __ Jump(BUILTIN_CODE(masm->isolate(), CallBoundFunction),
          RelocInfo::CODE_TARGET, equal);

  // Check if target has a [[Call]] internal method.
  __ testb(FieldOperand(rcx, Map::kBitFieldOffset),
           Immediate(Map::IsCallableBit::kMask));
  __ j(zero, &non_callable, Label::kNear);

  // Check if target is a proxy and call CallProxy external builtin
  __ CmpInstanceType(rcx, JS_PROXY_TYPE);
  __ Jump(BUILTIN_CODE(masm->isolate(), CallProxy), RelocInfo::CODE_TARGET,
          equal);

  // 2. Call to something else, which might have a [[Call]] internal method (if
  // not we raise an exception).

  // Overwrite the original receiver with the (original) target.
  __ movp(args.GetReceiverOperand(), rdi);
  // Let the "call_as_function_delegate" take care of the rest.
  __ LoadNativeContextSlot(Context::CALL_AS_FUNCTION_DELEGATE_INDEX, rdi);
  __ Jump(masm->isolate()->builtins()->CallFunction(
              ConvertReceiverMode::kNotNullOrUndefined),
          RelocInfo::CODE_TARGET);

  // 3. Call to something that is not callable.
  __ bind(&non_callable);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(rdi);
    __ CallRuntime(Runtime::kThrowCalledNonCallable);
  }
}

// static
void Builtins::Generate_ConstructFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdx : the new target (checked to be a constructor)
  //  -- rdi : the constructor to call (checked to be a JSFunction)
  // -----------------------------------
  __ AssertConstructor(rdi);
  __ AssertFunction(rdi);

  // Calling convention for function specific ConstructStubs require
  // rbx to contain either an AllocationSite or undefined.
  __ LoadRoot(rbx, RootIndex::kUndefinedValue);

  // Jump to JSBuiltinsConstructStub or JSConstructStubGeneric.
  __ movp(rcx, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
  __ testl(FieldOperand(rcx, SharedFunctionInfo::kFlagsOffset),
           Immediate(SharedFunctionInfo::ConstructAsBuiltinBit::kMask));
  __ Jump(BUILTIN_CODE(masm->isolate(), JSBuiltinsConstructStub),
          RelocInfo::CODE_TARGET, not_zero);

  __ Jump(BUILTIN_CODE(masm->isolate(), JSConstructStubGeneric),
          RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_ConstructBoundFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdx : the new target (checked to be a constructor)
  //  -- rdi : the constructor to call (checked to be a JSBoundFunction)
  // -----------------------------------
  __ AssertConstructor(rdi);
  __ AssertBoundFunction(rdi);

  // Push the [[BoundArguments]] onto the stack.
  Generate_PushBoundArguments(masm);

  // Patch new.target to [[BoundTargetFunction]] if new.target equals target.
  {
    Label done;
    __ cmpp(rdi, rdx);
    __ j(not_equal, &done, Label::kNear);
    __ movp(rdx,
            FieldOperand(rdi, JSBoundFunction::kBoundTargetFunctionOffset));
    __ bind(&done);
  }

  // Construct the [[BoundTargetFunction]] via the Construct builtin.
  __ movp(rdi, FieldOperand(rdi, JSBoundFunction::kBoundTargetFunctionOffset));
  __ Jump(BUILTIN_CODE(masm->isolate(), Construct), RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_Construct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdx : the new target (either the same as the constructor or
  //           the JSFunction on which new was invoked initially)
  //  -- rdi : the constructor to call (can be any Object)
  // -----------------------------------
  StackArgumentsAccessor args(rsp, rax);

  // Check if target is a Smi.
  Label non_constructor;
  __ JumpIfSmi(rdi, &non_constructor);

  // Check if target has a [[Construct]] internal method.
  __ movq(rcx, FieldOperand(rdi, HeapObject::kMapOffset));
  __ testb(FieldOperand(rcx, Map::kBitFieldOffset),
           Immediate(Map::IsConstructorBit::kMask));
  __ j(zero, &non_constructor);

  // Dispatch based on instance type.
  __ CmpInstanceType(rcx, JS_FUNCTION_TYPE);
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructFunction),
          RelocInfo::CODE_TARGET, equal);

  // Only dispatch to bound functions after checking whether they are
  // constructors.
  __ CmpInstanceType(rcx, JS_BOUND_FUNCTION_TYPE);
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructBoundFunction),
          RelocInfo::CODE_TARGET, equal);

  // Only dispatch to proxies after checking whether they are constructors.
  __ CmpInstanceType(rcx, JS_PROXY_TYPE);
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructProxy), RelocInfo::CODE_TARGET,
          equal);

  // Called Construct on an exotic Object with a [[Construct]] internal method.
  {
    // Overwrite the original receiver with the (original) target.
    __ movp(args.GetReceiverOperand(), rdi);
    // Let the "call_as_constructor_delegate" take care of the rest.
    __ LoadNativeContextSlot(Context::CALL_AS_CONSTRUCTOR_DELEGATE_INDEX, rdi);
    __ Jump(masm->isolate()->builtins()->CallFunction(),
            RelocInfo::CODE_TARGET);
  }

  // Called Construct on an Object that doesn't have a [[Construct]] internal
  // method.
  __ bind(&non_constructor);
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructedNonConstructable),
          RelocInfo::CODE_TARGET);
}

void Builtins::Generate_InterpreterOnStackReplacement(MacroAssembler* masm) {
  // Lookup the function in the JavaScript frame.
  __ movp(rax, Operand(rbp, StandardFrameConstants::kCallerFPOffset));
  __ movp(rax, Operand(rax, JavaScriptFrameConstants::kFunctionOffset));

  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Pass function as argument.
    __ Push(rax);
    __ CallRuntime(Runtime::kCompileForOnStackReplacement);
  }

  Label skip;
  // If the code object is null, just return to the caller.
  __ testp(rax, rax);
  __ j(not_equal, &skip, Label::kNear);
  __ ret(0);

  __ bind(&skip);

  // Drop the handler frame that is be sitting on top of the actual
  // JavaScript frame. This is the case then OSR is triggered from bytecode.
  __ leave();

  // Load deoptimization data from the code object.
  __ movp(rbx, Operand(rax, Code::kDeoptimizationDataOffset - kHeapObjectTag));

  // Load the OSR entrypoint offset from the deoptimization data.
  __ SmiUntag(rbx, Operand(rbx, FixedArray::OffsetOfElementAt(
                                    DeoptimizationData::kOsrPcOffsetIndex) -
                                    kHeapObjectTag));

  // Compute the target address = code_obj + header_size + osr_offset
  __ leap(rax, Operand(rax, rbx, times_1, Code::kHeaderSize - kHeapObjectTag));

  // Overwrite the return address on the stack.
  __ movq(StackOperandForReturnAddress(0), rax);

  // And "return" to the OSR entry point of the function.
  __ ret(0);
}

void Builtins::Generate_WasmCompileLazy(MacroAssembler* masm) {
  // The function index was pushed to the stack by the caller as int32.
  __ Pop(r11);
  // Convert to Smi for the runtime call.
  __ SmiTag(r11, r11);
  {
    HardAbortScope hard_abort(masm);  // Avoid calls to Abort.
    FrameScope scope(masm, StackFrame::WASM_COMPILE_LAZY);

    // Save all parameter registers (see wasm-linkage.cc). They might be
    // overwritten in the runtime call below. We don't have any callee-saved
    // registers in wasm, so no need to store anything else.
    static_assert(WasmCompileLazyFrameConstants::kNumberOfSavedGpParamRegs ==
                      arraysize(wasm::kGpParamRegisters),
                  "frame size mismatch");
    for (Register reg : wasm::kGpParamRegisters) {
      __ Push(reg);
    }
    static_assert(WasmCompileLazyFrameConstants::kNumberOfSavedFpParamRegs ==
                      arraysize(wasm::kFpParamRegisters),
                  "frame size mismatch");
    __ subp(rsp, Immediate(kSimd128Size * arraysize(wasm::kFpParamRegisters)));
    int offset = 0;
    for (DoubleRegister reg : wasm::kFpParamRegisters) {
      __ movdqu(Operand(rsp, offset), reg);
      offset += kSimd128Size;
    }

    // Push the WASM instance as an explicit argument to WasmCompileLazy.
    __ Push(kWasmInstanceRegister);
    // Push the function index as second argument.
    __ Push(r11);
    // Load the correct CEntry builtin from the instance object.
    __ movp(rcx, FieldOperand(kWasmInstanceRegister,
                              WasmInstanceObject::kCEntryStubOffset));
    // Initialize the JavaScript context with 0. CEntry will use it to
    // set the current context on the isolate.
    __ Move(kContextRegister, Smi::zero());
    __ CallRuntimeWithCEntry(Runtime::kWasmCompileLazy, rcx);
    // The entrypoint address is the return value.
    __ movq(r11, kReturnRegister0);

    // Restore registers.
    for (DoubleRegister reg : base::Reversed(wasm::kFpParamRegisters)) {
      offset -= kSimd128Size;
      __ movdqu(reg, Operand(rsp, offset));
    }
    DCHECK_EQ(0, offset);
    __ addp(rsp, Immediate(kSimd128Size * arraysize(wasm::kFpParamRegisters)));
    for (Register reg : base::Reversed(wasm::kGpParamRegisters)) {
      __ Pop(reg);
    }
  }
  // Finally, jump to the entrypoint.
  __ jmp(r11);
}

void Builtins::Generate_CEntry(MacroAssembler* masm, int result_size,
                               SaveFPRegsMode save_doubles, ArgvMode argv_mode,
                               bool builtin_exit_frame) {
  // rax: number of arguments including receiver
  // rbx: pointer to C function  (C callee-saved)
  // rbp: frame pointer of calling JS frame (restored after C call)
  // rsp: stack pointer  (restored after C call)
  // rsi: current context (restored)
  //
  // If argv_mode == kArgvInRegister:
  // r15: pointer to the first argument

#ifdef _WIN64
  // Windows 64-bit ABI passes arguments in rcx, rdx, r8, r9. It requires the
  // stack to be aligned to 16 bytes. It only allows a single-word to be
  // returned in register rax. Larger return sizes must be written to an address
  // passed as a hidden first argument.
  const Register kCCallArg0 = rcx;
  const Register kCCallArg1 = rdx;
  const Register kCCallArg2 = r8;
  const Register kCCallArg3 = r9;
  const int kArgExtraStackSpace = 2;
  const int kMaxRegisterResultSize = 1;
#else
  // GCC / Clang passes arguments in rdi, rsi, rdx, rcx, r8, r9. Simple results
  // are returned in rax, and a struct of two pointers are returned in rax+rdx.
  // Larger return sizes must be written to an address passed as a hidden first
  // argument.
  const Register kCCallArg0 = rdi;
  const Register kCCallArg1 = rsi;
  const Register kCCallArg2 = rdx;
  const Register kCCallArg3 = rcx;
  const int kArgExtraStackSpace = 0;
  const int kMaxRegisterResultSize = 2;
#endif  // _WIN64

  // Enter the exit frame that transitions from JavaScript to C++.
  int arg_stack_space =
      kArgExtraStackSpace +
      (result_size <= kMaxRegisterResultSize ? 0 : result_size);
  if (argv_mode == kArgvInRegister) {
    DCHECK(save_doubles == kDontSaveFPRegs);
    DCHECK(!builtin_exit_frame);
    __ EnterApiExitFrame(arg_stack_space);
    // Move argc into r14 (argv is already in r15).
    __ movp(r14, rax);
  } else {
    __ EnterExitFrame(
        arg_stack_space, save_doubles == kSaveFPRegs,
        builtin_exit_frame ? StackFrame::BUILTIN_EXIT : StackFrame::EXIT);
  }

  // rbx: pointer to builtin function  (C callee-saved).
  // rbp: frame pointer of exit frame  (restored after C call).
  // rsp: stack pointer (restored after C call).
  // r14: number of arguments including receiver (C callee-saved).
  // r15: argv pointer (C callee-saved).

  // Check stack alignment.
  if (FLAG_debug_code) {
    __ CheckStackAlignment();
  }

  // Call C function. The arguments object will be created by stubs declared by
  // DECLARE_RUNTIME_FUNCTION().
  if (result_size <= kMaxRegisterResultSize) {
    // Pass a pointer to the Arguments object as the first argument.
    // Return result in single register (rax), or a register pair (rax, rdx).
    __ movp(kCCallArg0, r14);  // argc.
    __ movp(kCCallArg1, r15);  // argv.
    __ Move(kCCallArg2, ExternalReference::isolate_address(masm->isolate()));
  } else {
    DCHECK_LE(result_size, 2);
    // Pass a pointer to the result location as the first argument.
    __ leap(kCCallArg0, StackSpaceOperand(kArgExtraStackSpace));
    // Pass a pointer to the Arguments object as the second argument.
    __ movp(kCCallArg1, r14);  // argc.
    __ movp(kCCallArg2, r15);  // argv.
    __ Move(kCCallArg3, ExternalReference::isolate_address(masm->isolate()));
  }
  __ call(rbx);

  if (result_size > kMaxRegisterResultSize) {
    // Read result values stored on stack. Result is stored
    // above the the two Arguments object slots on Win64.
    DCHECK_LE(result_size, 2);
    __ movq(kReturnRegister0, StackSpaceOperand(kArgExtraStackSpace + 0));
    __ movq(kReturnRegister1, StackSpaceOperand(kArgExtraStackSpace + 1));
  }
  // Result is in rax or rdx:rax - do not destroy these registers!

  // Check result for exception sentinel.
  Label exception_returned;
  __ CompareRoot(rax, RootIndex::kException);
  __ j(equal, &exception_returned);

  // Check that there is no pending exception, otherwise we
  // should have returned the exception sentinel.
  if (FLAG_debug_code) {
    Label okay;
    __ LoadRoot(r14, RootIndex::kTheHoleValue);
    ExternalReference pending_exception_address = ExternalReference::Create(
        IsolateAddressId::kPendingExceptionAddress, masm->isolate());
    Operand pending_exception_operand =
        masm->ExternalReferenceAsOperand(pending_exception_address);
    __ cmpp(r14, pending_exception_operand);
    __ j(equal, &okay, Label::kNear);
    __ int3();
    __ bind(&okay);
  }

  // Exit the JavaScript to C++ exit frame.
  __ LeaveExitFrame(save_doubles == kSaveFPRegs, argv_mode == kArgvOnStack);
  __ ret(0);

  // Handling of exception.
  __ bind(&exception_returned);

  ExternalReference pending_handler_context_address = ExternalReference::Create(
      IsolateAddressId::kPendingHandlerContextAddress, masm->isolate());
  ExternalReference pending_handler_entrypoint_address =
      ExternalReference::Create(
          IsolateAddressId::kPendingHandlerEntrypointAddress, masm->isolate());
  ExternalReference pending_handler_fp_address = ExternalReference::Create(
      IsolateAddressId::kPendingHandlerFPAddress, masm->isolate());
  ExternalReference pending_handler_sp_address = ExternalReference::Create(
      IsolateAddressId::kPendingHandlerSPAddress, masm->isolate());

  // Ask the runtime for help to determine the handler. This will set rax to
  // contain the current pending exception, don't clobber it.
  ExternalReference find_handler =
      ExternalReference::Create(Runtime::kUnwindAndFindExceptionHandler);
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ movp(arg_reg_1, Immediate(0));  // argc.
    __ movp(arg_reg_2, Immediate(0));  // argv.
    __ Move(arg_reg_3, ExternalReference::isolate_address(masm->isolate()));
    __ PrepareCallCFunction(3);
    __ CallCFunction(find_handler, 3);
  }
  // Retrieve the handler context, SP and FP.
  __ movp(rsi,
          masm->ExternalReferenceAsOperand(pending_handler_context_address));
  __ movp(rsp, masm->ExternalReferenceAsOperand(pending_handler_sp_address));
  __ movp(rbp, masm->ExternalReferenceAsOperand(pending_handler_fp_address));

  // If the handler is a JS frame, restore the context to the frame. Note that
  // the context will be set to (rsi == 0) for non-JS frames.
  Label skip;
  __ testp(rsi, rsi);
  __ j(zero, &skip, Label::kNear);
  __ movp(Operand(rbp, StandardFrameConstants::kContextOffset), rsi);
  __ bind(&skip);

  // Reset the masking register. This is done independent of the underlying
  // feature flag {FLAG_untrusted_code_mitigations} to make the snapshot work
  // with both configurations. It is safe to always do this, because the
  // underlying register is caller-saved and can be arbitrarily clobbered.
  __ ResetSpeculationPoisonRegister();

  // Compute the handler entry address and jump to it.
  __ movp(rdi,
          masm->ExternalReferenceAsOperand(pending_handler_entrypoint_address));
  __ jmp(rdi);
}

void Builtins::Generate_DoubleToI(MacroAssembler* masm) {
  Label check_negative, process_64_bits, done;

  // Account for return address and saved regs.
  const int kArgumentOffset = 4 * kRegisterSize;

  MemOperand mantissa_operand(MemOperand(rsp, kArgumentOffset));
  MemOperand exponent_operand(
      MemOperand(rsp, kArgumentOffset + kDoubleSize / 2));

  // The result is returned on the stack.
  MemOperand return_operand = mantissa_operand;

  Register scratch1 = rbx;

  // Since we must use rcx for shifts below, use some other register (rax)
  // to calculate the result if ecx is the requested return register.
  Register result_reg = rax;
  // Save ecx if it isn't the return register and therefore volatile, or if it
  // is the return register, then save the temp register we use in its stead
  // for the result.
  Register save_reg = rax;
  __ pushq(rcx);
  __ pushq(scratch1);
  __ pushq(save_reg);

  __ movl(scratch1, mantissa_operand);
  __ Movsd(kScratchDoubleReg, mantissa_operand);
  __ movl(rcx, exponent_operand);

  __ andl(rcx, Immediate(HeapNumber::kExponentMask));
  __ shrl(rcx, Immediate(HeapNumber::kExponentShift));
  __ leal(result_reg, MemOperand(rcx, -HeapNumber::kExponentBias));
  __ cmpl(result_reg, Immediate(HeapNumber::kMantissaBits));
  __ j(below, &process_64_bits, Label::kNear);

  // Result is entirely in lower 32-bits of mantissa
  int delta = HeapNumber::kExponentBias + Double::kPhysicalSignificandSize;
  __ subl(rcx, Immediate(delta));
  __ xorl(result_reg, result_reg);
  __ cmpl(rcx, Immediate(31));
  __ j(above, &done, Label::kNear);
  __ shll_cl(scratch1);
  __ jmp(&check_negative, Label::kNear);

  __ bind(&process_64_bits);
  __ Cvttsd2siq(result_reg, kScratchDoubleReg);
  __ jmp(&done, Label::kNear);

  // If the double was negative, negate the integer result.
  __ bind(&check_negative);
  __ movl(result_reg, scratch1);
  __ negl(result_reg);
  __ cmpl(exponent_operand, Immediate(0));
  __ cmovl(greater, result_reg, scratch1);

  // Restore registers
  __ bind(&done);
  __ movl(return_operand, result_reg);
  __ popq(save_reg);
  __ popq(scratch1);
  __ popq(rcx);
  __ ret(0);
}

void Builtins::Generate_MathPowInternal(MacroAssembler* masm) {
  const Register exponent = rdx;
  const Register scratch = rcx;
  const XMMRegister double_result = xmm3;
  const XMMRegister double_base = xmm2;
  const XMMRegister double_exponent = xmm1;
  const XMMRegister double_scratch = xmm4;

  Label call_runtime, done, exponent_not_smi, int_exponent;

  // Save 1 in double_result - we need this several times later on.
  __ movp(scratch, Immediate(1));
  __ Cvtlsi2sd(double_result, scratch);

  Label fast_power, try_arithmetic_simplification;
  // Detect integer exponents stored as double.
  __ DoubleToI(exponent, double_exponent, double_scratch,
               &try_arithmetic_simplification, &try_arithmetic_simplification);
  __ jmp(&int_exponent);

  __ bind(&try_arithmetic_simplification);
  __ Cvttsd2si(exponent, double_exponent);
  // Skip to runtime if possibly NaN (indicated by the indefinite integer).
  __ cmpl(exponent, Immediate(0x1));
  __ j(overflow, &call_runtime);

  // Using FPU instructions to calculate power.
  Label fast_power_failed;
  __ bind(&fast_power);
  __ fnclex();  // Clear flags to catch exceptions later.
  // Transfer (B)ase and (E)xponent onto the FPU register stack.
  __ subp(rsp, Immediate(kDoubleSize));
  __ Movsd(Operand(rsp, 0), double_exponent);
  __ fld_d(Operand(rsp, 0));  // E
  __ Movsd(Operand(rsp, 0), double_base);
  __ fld_d(Operand(rsp, 0));  // B, E

  // Exponent is in st(1) and base is in st(0)
  // B ^ E = (2^(E * log2(B)) - 1) + 1 = (2^X - 1) + 1 for X = E * log2(B)
  // FYL2X calculates st(1) * log2(st(0))
  __ fyl2x();    // X
  __ fld(0);     // X, X
  __ frndint();  // rnd(X), X
  __ fsub(1);    // rnd(X), X-rnd(X)
  __ fxch(1);    // X - rnd(X), rnd(X)
  // F2XM1 calculates 2^st(0) - 1 for -1 < st(0) < 1
  __ f2xm1();   // 2^(X-rnd(X)) - 1, rnd(X)
  __ fld1();    // 1, 2^(X-rnd(X)) - 1, rnd(X)
  __ faddp(1);  // 2^(X-rnd(X)), rnd(X)
  // FSCALE calculates st(0) * 2^st(1)
  __ fscale();  // 2^X, rnd(X)
  __ fstp(1);
  // Bail out to runtime in case of exceptions in the status word.
  __ fnstsw_ax();
  __ testb(rax, Immediate(0x5F));  // Check for all but precision exception.
  __ j(not_zero, &fast_power_failed, Label::kNear);
  __ fstp_d(Operand(rsp, 0));
  __ Movsd(double_result, Operand(rsp, 0));
  __ addp(rsp, Immediate(kDoubleSize));
  __ jmp(&done);

  __ bind(&fast_power_failed);
  __ fninit();
  __ addp(rsp, Immediate(kDoubleSize));
  __ jmp(&call_runtime);

  // Calculate power with integer exponent.
  __ bind(&int_exponent);
  const XMMRegister double_scratch2 = double_exponent;
  // Back up exponent as we need to check if exponent is negative later.
  __ movp(scratch, exponent);                // Back up exponent.
  __ Movsd(double_scratch, double_base);     // Back up base.
  __ Movsd(double_scratch2, double_result);  // Load double_exponent with 1.

  // Get absolute value of exponent.
  Label no_neg, while_true, while_false;
  __ testl(scratch, scratch);
  __ j(positive, &no_neg, Label::kNear);
  __ negl(scratch);
  __ bind(&no_neg);

  __ j(zero, &while_false, Label::kNear);
  __ shrl(scratch, Immediate(1));
  // Above condition means CF==0 && ZF==0.  This means that the
  // bit that has been shifted out is 0 and the result is not 0.
  __ j(above, &while_true, Label::kNear);
  __ Movsd(double_result, double_scratch);
  __ j(zero, &while_false, Label::kNear);

  __ bind(&while_true);
  __ shrl(scratch, Immediate(1));
  __ Mulsd(double_scratch, double_scratch);
  __ j(above, &while_true, Label::kNear);
  __ Mulsd(double_result, double_scratch);
  __ j(not_zero, &while_true);

  __ bind(&while_false);
  // If the exponent is negative, return 1/result.
  __ testl(exponent, exponent);
  __ j(greater, &done);
  __ Divsd(double_scratch2, double_result);
  __ Movsd(double_result, double_scratch2);
  // Test whether result is zero.  Bail out to check for subnormal result.
  // Due to subnormals, x^-y == (1/x)^y does not hold in all cases.
  __ Xorpd(double_scratch2, double_scratch2);
  __ Ucomisd(double_scratch2, double_result);
  // double_exponent aliased as double_scratch2 has already been overwritten
  // and may not have contained the exponent value in the first place when the
  // input was a smi.  We reset it with exponent value before bailing out.
  __ j(not_equal, &done);
  __ Cvtlsi2sd(double_exponent, exponent);

  // Returning or bailing out.
  __ bind(&call_runtime);
  // Move base to the correct argument register.  Exponent is already in xmm1.
  __ Movsd(xmm0, double_base);
  DCHECK(double_exponent == xmm1);
  {
    AllowExternalCallThatCantCauseGC scope(masm);
    __ PrepareCallCFunction(2);
    __ CallCFunction(ExternalReference::power_double_double_function(), 2);
  }
  // Return value is in xmm0.
  __ Movsd(double_result, xmm0);

  __ bind(&done);
  __ ret(0);
}

void Builtins::Generate_InternalArrayConstructorImpl(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : argc
  //  -- rdi    : constructor
  //  -- rsp[0] : return address
  //  -- rsp[8] : last argument
  // -----------------------------------

  if (FLAG_debug_code) {
    // The array construct code is only set for the global and natives
    // builtin Array functions which always have maps.

    // Initial map for the builtin Array function should be a map.
    __ movp(rcx, FieldOperand(rdi, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a nullptr and a Smi.
    STATIC_ASSERT(kSmiTag == 0);
    Condition not_smi = NegateCondition(masm->CheckSmi(rcx));
    __ Check(not_smi, AbortReason::kUnexpectedInitialMapForArrayFunction);
    __ CmpObjectType(rcx, MAP_TYPE, rcx);
    __ Check(equal, AbortReason::kUnexpectedInitialMapForArrayFunction);

    // Figure out the right elements kind
    __ movp(rcx, FieldOperand(rdi, JSFunction::kPrototypeOrInitialMapOffset));

    // Load the map's "bit field 2" into |result|. We only need the first byte,
    // but the following masking takes care of that anyway.
    __ movzxbp(rcx, FieldOperand(rcx, Map::kBitField2Offset));
    // Retrieve elements_kind from bit field 2.
    __ DecodeField<Map::ElementsKindBits>(rcx);

    // Initial elements kind should be packed elements.
    __ cmpl(rcx, Immediate(PACKED_ELEMENTS));
    __ Assert(equal, AbortReason::kInvalidElementsKindForInternalPackedArray);

    // No arguments should be passed.
    __ testp(rax, rax);
    __ Assert(zero, AbortReason::kWrongNumberOfArgumentsForInternalPackedArray);
  }

  __ Jump(
      BUILTIN_CODE(masm->isolate(), InternalArrayNoArgumentConstructor_Packed),
      RelocInfo::CODE_TARGET);
}

namespace {

int Offset(ExternalReference ref0, ExternalReference ref1) {
  int64_t offset = (ref0.address() - ref1.address());
  // Check that fits into int.
  DCHECK(static_cast<int>(offset) == offset);
  return static_cast<int>(offset);
}

// Calls an API function.  Allocates HandleScope, extracts returned value
// from handle and propagates exceptions.  Clobbers r14, r15, rbx and
// caller-save registers.  Restores context.  On return removes
// stack_space * kPointerSize (GCed).
void CallApiFunctionAndReturn(MacroAssembler* masm, Register function_address,
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
    DCHECK_EQ(stack_space, 0);
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

  if (stack_space_operand == nullptr) {
    DCHECK_NE(stack_space, 0);
    __ ret(stack_space * kPointerSize);
  } else {
    DCHECK_EQ(stack_space, 0);
    __ PopReturnAddressTo(rcx);
    __ addq(rsp, rbx);
    __ jmp(rcx);
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

}  // namespace

// TODO(jgruber): Instead of explicitly setting up implicit_args_ on the stack
// in CallApiCallback, we could use the calling convention to set up the stack
// correctly in the first place.
//
// TODO(jgruber): I suspect that most of CallApiCallback could be implemented
// as a C++ trampoline, vastly simplifying the assembly implementation.

void Builtins::Generate_CallApiCallback(MacroAssembler* masm) {
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

void Builtins::Generate_CallApiGetter(MacroAssembler* masm) {
  Register name_arg = arg_reg_1;
  Register accessor_info_arg = arg_reg_2;
  Register getter_arg = arg_reg_3;
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
  __ PushAddress(ExternalReference::isolate_address(masm->isolate()));
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

void Builtins::Generate_DirectCEntry(MacroAssembler* masm) {
  __ int3();  // Unused on this architecture.
}

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_X64
