// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-gen.h"
#include "src/code-stub-assembler.h"
#include "src/objects-inl.h"
#include "src/wasm/wasm-opcodes.h"

namespace v8 {
namespace internal {

typedef compiler::Node Node;

TF_BUILTIN(WasmArgumentsAdaptor, CodeStubAssembler) {
  TNode<Object> context =
      UncheckedCast<Object>(Parameter(Descriptor::kContext));
  TNode<Object> function =
      UncheckedCast<Object>(Parameter(Descriptor::kFunction));
  TNode<Object> new_target =
      UncheckedCast<Object>(Parameter(Descriptor::kNewTarget));
  TNode<Object> argc1 =
      UncheckedCast<Object>(Parameter(Descriptor::kActualArgumentsCount));
  TNode<Object> argc2 =
      UncheckedCast<Object>(Parameter(Descriptor::kExpectedArgumentsCount));
  TNode<Object> instance = UncheckedCast<Object>(
      LoadFromParentFrame(WasmCompiledFrameConstants::kWasmInstanceOffset));
  TNode<IntPtrT> roots = UncheckedCast<IntPtrT>(
      Load(MachineType::Pointer(), instance,
           IntPtrConstant(WasmInstanceObject::kRootsArrayAddressOffset -
                          kHeapObjectTag)));
  TNode<Code> target = UncheckedCast<Code>(Load(
      MachineType::TaggedPointer(), roots,
      IntPtrConstant(Heap::roots_to_builtins_offset() +
                     Builtins::kArgumentsAdaptorTrampoline * kPointerSize)));
  TailCallStub(ArgumentAdaptorDescriptor{}, target, context, function,
               new_target, argc1, argc2);
}

TF_BUILTIN(WasmCallJavaScript, CodeStubAssembler) {
  TNode<Object> context =
      UncheckedCast<Object>(Parameter(Descriptor::kContext));
  TNode<Object> function =
      UncheckedCast<Object>(Parameter(Descriptor::kFunction));
  TNode<Object> argc =
      UncheckedCast<Object>(Parameter(Descriptor::kActualArgumentsCount));
  TNode<Object> instance = UncheckedCast<Object>(
      LoadFromParentFrame(WasmCompiledFrameConstants::kWasmInstanceOffset));
  TNode<IntPtrT> roots = UncheckedCast<IntPtrT>(
      Load(MachineType::Pointer(), instance,
           IntPtrConstant(WasmInstanceObject::kRootsArrayAddressOffset -
                          kHeapObjectTag)));
  TNode<Code> target = UncheckedCast<Code>(
      Load(MachineType::TaggedPointer(), roots,
           IntPtrConstant(Heap::roots_to_builtins_offset() +
                          Builtins::kCall_ReceiverIsAny * kPointerSize)));
  TailCallStub(CallTrampolineDescriptor{}, target, context, function, argc);
}

TF_BUILTIN(WasmStackGuard, CodeStubAssembler) {
  TNode<Object> instance = UncheckedCast<Object>(
      LoadFromParentFrame(WasmCompiledFrameConstants::kWasmInstanceOffset));
  TNode<Code> centry = UncheckedCast<Code>(Load(
      MachineType::AnyTagged(), instance,
      IntPtrConstant(WasmInstanceObject::kCEntryStubOffset - kHeapObjectTag)));
  TailCallRuntimeWithCEntry(Runtime::kWasmStackGuard, centry,
                            NoContextConstant());
}

#define DECLARE_ENUM(name)                                                     \
  TF_BUILTIN(ThrowWasm##name, CodeStubAssembler) {                             \
    TNode<Object> instance = UncheckedCast<Object>(                            \
        LoadFromParentFrame(WasmCompiledFrameConstants::kWasmInstanceOffset)); \
    TNode<Code> centry = UncheckedCast<Code>(                                  \
        Load(MachineType::AnyTagged(), instance,                               \
             IntPtrConstant(WasmInstanceObject::kCEntryStubOffset -            \
                            kHeapObjectTag)));                                 \
    int message_id = wasm::WasmOpcodes::TrapReasonToMessageId(wasm::k##name);  \
    TailCallRuntimeWithCEntry(Runtime::kThrowWasmError, centry,                \
                              NoContextConstant(), SmiConstant(message_id));   \
  }
FOREACH_WASM_TRAPREASON(DECLARE_ENUM)
#undef DECLARE_ENUM

}  // namespace internal
}  // namespace v8
