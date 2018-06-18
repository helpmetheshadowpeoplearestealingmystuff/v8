// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interface-descriptors.h"

namespace v8 {
namespace internal {


void CallInterfaceDescriptorData::InitializePlatformSpecific(
    int register_parameter_count, const Register* registers,
    PlatformInterfaceDescriptor* platform_descriptor) {
  platform_specific_descriptor_ = platform_descriptor;
  register_param_count_ = register_parameter_count;

  // InterfaceDescriptor owns a copy of the registers array.
  register_params_ = NewArray<Register>(register_parameter_count, no_reg);
  for (int i = 0; i < register_parameter_count; i++) {
    register_params_[i] = registers[i];
  }
}

void CallInterfaceDescriptorData::InitializePlatformIndependent(
    int parameter_count, int extra_parameter_count,
    const MachineType* machine_types) {
  // InterfaceDescriptor owns a copy of the MachineType array.
  // We only care about parameters, not receiver and result.
  param_count_ = parameter_count + extra_parameter_count;
  machine_types_ = NewArray<MachineType>(param_count_);
  for (int i = 0; i < param_count_; i++) {
    if (machine_types == nullptr || i >= parameter_count) {
      machine_types_[i] = MachineType::AnyTagged();
    } else {
      machine_types_[i] = machine_types[i];
    }
  }
}

void CallInterfaceDescriptorData::Reset() {
  delete[] machine_types_;
  machine_types_ = nullptr;
  delete[] register_params_;
  register_params_ = nullptr;
}

// static
CallInterfaceDescriptorData
    CallDescriptors::call_descriptor_data_[NUMBER_OF_DESCRIPTORS];

void CallDescriptors::InitializeOncePerProcess() {
#define INTERFACE_DESCRIPTOR(name, ...) \
  name##Descriptor().Initialize(&call_descriptor_data_[CallDescriptors::name]);
  INTERFACE_DESCRIPTOR_LIST(INTERFACE_DESCRIPTOR)
#undef INTERFACE_DESCRIPTOR
}

void CallDescriptors::TearDown() {
  for (CallInterfaceDescriptorData& data : call_descriptor_data_) {
    data.Reset();
  }
}

void CallInterfaceDescriptor::JSDefaultInitializePlatformSpecific(
    CallInterfaceDescriptorData* data, int non_js_register_parameter_count) {
  DCHECK_LE(static_cast<unsigned>(non_js_register_parameter_count), 1);

  // 3 is for kTarget, kNewTarget and kActualArgumentsCount
  int register_parameter_count = 3 + non_js_register_parameter_count;

  DCHECK(!AreAliased(
      kJavaScriptCallTargetRegister, kJavaScriptCallNewTargetRegister,
      kJavaScriptCallArgCountRegister, kJavaScriptCallExtraArg1Register));

  const Register default_js_stub_registers[] = {
      kJavaScriptCallTargetRegister, kJavaScriptCallNewTargetRegister,
      kJavaScriptCallArgCountRegister, kJavaScriptCallExtraArg1Register};

  CHECK_LE(static_cast<size_t>(register_parameter_count),
           arraysize(default_js_stub_registers));
  data->InitializePlatformSpecific(register_parameter_count,
                                   default_js_stub_registers);
}

const char* CallInterfaceDescriptor::DebugName() const {
  CallDescriptors::Key key = CallDescriptors::GetKey(data_);
  switch (key) {
#define DEF_CASE(name, ...)   \
  case CallDescriptors::name: \
    return #name " Descriptor";
    INTERFACE_DESCRIPTOR_LIST(DEF_CASE)
#undef DEF_CASE
    case CallDescriptors::NUMBER_OF_DESCRIPTORS:
      break;
  }
  return "";
}


void VoidDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  data->InitializePlatformSpecific(0, nullptr);
}

void AllocateDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {kAllocateSizeRegister};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void AllocateDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  MachineType machine_types[] = {MachineType::Int32()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void FastNewFunctionContextDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  MachineType machine_types[] = {MachineType::AnyTagged(),
                                 MachineType::Int32()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void FastNewFunctionContextDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ScopeInfoRegister(), SlotsRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void FastNewObjectDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {TargetRegister(), NewTargetRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

const Register FastNewObjectDescriptor::TargetRegister() {
  return kJSFunctionRegister;
}

const Register FastNewObjectDescriptor::NewTargetRegister() {
  return kJavaScriptCallNewTargetRegister;
}

void RecordWriteDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  MachineType machine_types[] = {MachineType::TaggedPointer(),
                                 MachineType::Pointer(), MachineType::Pointer(),
                                 MachineType::TaggedSigned(),
                                 MachineType::TaggedSigned()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void LoadDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kReceiver, kName, kSlot
  MachineType machine_types[] = {MachineType::AnyTagged(),
                                 MachineType::AnyTagged(),
                                 MachineType::TaggedSigned()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void LoadDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ReceiverRegister(), NameRegister(), SlotRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void LoadGlobalDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kName, kSlot
  MachineType machine_types[] = {MachineType::AnyTagged(),
                                 MachineType::TaggedSigned()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void LoadGlobalDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {NameRegister(), SlotRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void LoadGlobalWithVectorDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kName, kSlot, kVector
  MachineType machine_types[] = {MachineType::AnyTagged(),
                                 MachineType::TaggedSigned(),
                                 MachineType::AnyTagged()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void LoadGlobalWithVectorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {NameRegister(), SlotRegister(), VectorRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void StoreGlobalDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kName, kValue, kSlot
  MachineType machine_types[] = {MachineType::AnyTagged(),
                                 MachineType::AnyTagged(),
                                 MachineType::TaggedSigned()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void StoreGlobalDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {NameRegister(), ValueRegister(), SlotRegister()};

  int len = arraysize(registers) - kStackArgumentsCount;
  data->InitializePlatformSpecific(len, registers);
}

void StoreGlobalWithVectorDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kName, kValue, kSlot, kVector
  MachineType machine_types[] = {
      MachineType::AnyTagged(), MachineType::AnyTagged(),
      MachineType::TaggedSigned(), MachineType::AnyTagged()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void StoreGlobalWithVectorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {NameRegister(), ValueRegister(), SlotRegister(),
                          VectorRegister()};
  int len = arraysize(registers) - kStackArgumentsCount;
  data->InitializePlatformSpecific(len, registers);
}

void StoreDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kReceiver, kName, kValue, kSlot
  MachineType machine_types[] = {
      MachineType::AnyTagged(), MachineType::AnyTagged(),
      MachineType::AnyTagged(), MachineType::TaggedSigned()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void StoreDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ReceiverRegister(), NameRegister(), ValueRegister(),
                          SlotRegister()};

  int len = arraysize(registers) - kStackArgumentsCount;
  data->InitializePlatformSpecific(len, registers);
}

void StoreTransitionDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      ReceiverRegister(), NameRegister(), MapRegister(),
      ValueRegister(),    SlotRegister(), VectorRegister(),
  };
  int len = arraysize(registers) - kStackArgumentsCount;
  data->InitializePlatformSpecific(len, registers);
}

void StoreTransitionDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kReceiver, kName, kMap, kValue, kSlot, kVector
  MachineType machine_types[] = {
      MachineType::AnyTagged(),    MachineType::AnyTagged(),
      MachineType::AnyTagged(),    MachineType::AnyTagged(),
      MachineType::TaggedSigned(), MachineType::AnyTagged()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void StringAtDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kReceiver, kPosition
  // TODO(turbofan): Allow builtins to return untagged values.
  MachineType machine_types[] = {MachineType::AnyTagged(),
                                 MachineType::IntPtr()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void StringAtDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  DefaultInitializePlatformSpecific(data, kParameterCount);
}

void StringSubstringDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kString, kFrom, kTo
  // TODO(turbofan): Allow builtins to return untagged values.
  MachineType machine_types[] = {MachineType::AnyTagged(),
                                 MachineType::IntPtr(), MachineType::IntPtr()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void StringSubstringDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  DefaultInitializePlatformSpecific(data, kParameterCount);
}

void TypeConversionDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ArgumentRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void TypeConversionStackParameterDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  data->InitializePlatformSpecific(0, nullptr);
}

void TypeConversionStackParameterDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  data->InitializePlatformIndependent(data->register_param_count(), 1, nullptr);
}

void LoadWithVectorDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kReceiver, kName, kSlot, kVector
  MachineType machine_types[] = {
      MachineType::AnyTagged(), MachineType::AnyTagged(),
      MachineType::TaggedSigned(), MachineType::AnyTagged()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void LoadWithVectorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ReceiverRegister(), NameRegister(), SlotRegister(),
                          VectorRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void StoreWithVectorDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kReceiver, kName, kValue, kSlot, kVector
  MachineType machine_types[] = {
      MachineType::AnyTagged(), MachineType::AnyTagged(),
      MachineType::AnyTagged(), MachineType::TaggedSigned(),
      MachineType::AnyTagged()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void StoreWithVectorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ReceiverRegister(), NameRegister(), ValueRegister(),
                          SlotRegister(), VectorRegister()};
  int len = arraysize(registers) - kStackArgumentsCount;
  data->InitializePlatformSpecific(len, registers);
}

const Register ApiGetterDescriptor::ReceiverRegister() {
  return LoadDescriptor::ReceiverRegister();
}

void ApiGetterDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ReceiverRegister(), HolderRegister(),
                          CallbackRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ContextOnlyDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  data->InitializePlatformSpecific(0, nullptr);
}

void GrowArrayElementsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ObjectRegister(), KeyRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void NewArgumentsElementsDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kFrame, kLength, kMappedCount
  MachineType const kMachineTypes[] = {MachineType::Pointer(),
                                       MachineType::TaggedSigned(),
                                       MachineType::TaggedSigned()};
  data->InitializePlatformIndependent(arraysize(kMachineTypes), 0,
                                      kMachineTypes);
}

void NewArgumentsElementsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  DefaultInitializePlatformSpecific(data, 3);
}

void CallTrampolineDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kFunction, kActualArgumentsCount
  MachineType machine_types[] = {MachineType::AnyTagged(),
                                 MachineType::Int32()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void CallVarargsDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kTarget, kActualArgumentsCount, kArgumentsList, kArgumentsLength
  MachineType machine_types[] = {MachineType::AnyTagged(), MachineType::Int32(),
                                 MachineType::AnyTagged(),
                                 MachineType::Int32()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void CallForwardVarargsDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kTarget, kActualArgumentsCount, kStartIndex
  MachineType machine_types[] = {MachineType::AnyTagged(), MachineType::Int32(),
                                 MachineType::Int32()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void CallWithSpreadDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kTarget, kArgumentsCount, kArgumentsList
  MachineType machine_types[] = {MachineType::AnyTagged(), MachineType::Int32(),
                                 MachineType::AnyTagged()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void CallWithArrayLikeDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kTarget, kArgumentsList
  MachineType machine_types[] = {MachineType::AnyTagged(),
                                 MachineType::AnyTagged()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void ConstructVarargsDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kTarget, kNewTarget, kActualArgumentsCount, kArgumentsList,
  // kArgumentsLength
  MachineType machine_types[] = {
      MachineType::AnyTagged(), MachineType::AnyTagged(), MachineType::Int32(),
      MachineType::AnyTagged(), MachineType::Int32()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void ConstructForwardVarargsDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kTarget, kNewTarget, kActualArgumentsCount, kStartIndex
  MachineType machine_types[] = {MachineType::AnyTagged(),
                                 MachineType::AnyTagged(), MachineType::Int32(),
                                 MachineType::Int32()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void ConstructWithSpreadDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kTarget, kNewTarget, kArgumentsCount, kSpread
  MachineType machine_types[] = {MachineType::AnyTagged(),
                                 MachineType::AnyTagged(), MachineType::Int32(),
                                 MachineType::AnyTagged()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void ConstructWithArrayLikeDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kTarget, kNewTarget, kArgumentsList
  MachineType machine_types[] = {MachineType::AnyTagged(),
                                 MachineType::AnyTagged(),
                                 MachineType::AnyTagged()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void ConstructStubDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kFunction, kNewTarget, kActualArgumentsCount, kAllocationSite
  MachineType machine_types[] = {MachineType::AnyTagged(),
                                 MachineType::AnyTagged(), MachineType::Int32(),
                                 MachineType::AnyTagged()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void ArrayNoArgumentConstructorDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kFunction, kAllocationSite, kActualArgumentsCount, kFunctionParameter
  MachineType machine_types[] = {MachineType::AnyTagged(),
                                 MachineType::AnyTagged(), MachineType::Int32(),
                                 MachineType::AnyTagged()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void ArrayNoArgumentConstructorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // This descriptor must use the same set of registers as the
  // ArrayNArgumentsConstructorDescriptor.
  ArrayNArgumentsConstructorDescriptor::InitializePlatformSpecific(data);
}

void ArraySingleArgumentConstructorDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kFunction, kAllocationSite, kActualArgumentsCount, kFunctionParameter,
  // kArraySizeSmiParameter
  MachineType machine_types[] = {
      MachineType::AnyTagged(), MachineType::AnyTagged(), MachineType::Int32(),
      MachineType::AnyTagged(), MachineType::AnyTagged()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void ArraySingleArgumentConstructorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // This descriptor must use the same set of registers as the
  // ArrayNArgumentsConstructorDescriptor.
  ArrayNArgumentsConstructorDescriptor::InitializePlatformSpecific(data);
}

void ArrayNArgumentsConstructorDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kFunction, kAllocationSite, kActualArgumentsCount
  MachineType machine_types[] = {
      MachineType::AnyTagged(), MachineType::AnyTagged(), MachineType::Int32()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void ArrayNArgumentsConstructorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // Keep the arguments on the same registers as they were in
  // ArrayConstructorDescriptor to avoid unnecessary register moves.
  // kFunction, kAllocationSite, kActualArgumentsCount
  Register registers[] = {kJavaScriptCallTargetRegister,
                          kJavaScriptCallExtraArg1Register,
                          kJavaScriptCallArgCountRegister};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ArgumentAdaptorDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kFunction, kNewTarget, kActualArgumentsCount, kExpectedArgumentsCount
  MachineType machine_types[] = {MachineType::TaggedPointer(),
                                 MachineType::AnyTagged(), MachineType::Int32(),
                                 MachineType::Int32()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void ApiCallbackDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kTargetContext, kCallData, kHolder, kApiFunctionAddress
  MachineType machine_types[] = {
      MachineType::AnyTagged(), MachineType::AnyTagged(),
      MachineType::AnyTagged(), MachineType::Pointer()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void InterpreterDispatchDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kAccumulator, kBytecodeOffset, kBytecodeArray, kDispatchTable
  MachineType machine_types[] = {
      MachineType::AnyTagged(), MachineType::IntPtr(), MachineType::AnyTagged(),
      MachineType::IntPtr()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void InterpreterPushArgsThenCallDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kNumberOfArguments, kFirstArgument, kFunction
  MachineType machine_types[] = {MachineType::Int32(), MachineType::Pointer(),
                                 MachineType::AnyTagged()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void InterpreterPushArgsThenConstructDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kNumberOfArguments, kNewTarget, kConstructor, kFeedbackElement,
  // kFirstArgument
  MachineType machine_types[] = {
      MachineType::Int32(), MachineType::AnyTagged(), MachineType::AnyTagged(),
      MachineType::AnyTagged(), MachineType::Pointer()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void InterpreterCEntryDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kNumberOfArguments, kFirstArgument, kFunctionEntry
  MachineType machine_types[] = {MachineType::Int32(), MachineType::Pointer(),
                                 MachineType::Pointer()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void FrameDropperTrampolineDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // New FP value.
  MachineType machine_types[] = {MachineType::Pointer()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

}  // namespace internal
}  // namespace v8
