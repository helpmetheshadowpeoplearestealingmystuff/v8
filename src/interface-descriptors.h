// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CALL_INTERFACE_DESCRIPTOR_H_
#define V8_CALL_INTERFACE_DESCRIPTOR_H_

#include <memory>

#include "src/assembler.h"
#include "src/macro-assembler.h"

namespace v8 {
namespace internal {

class PlatformInterfaceDescriptor;

#define INTERFACE_DESCRIPTOR_LIST(V)   \
  V(Void)                              \
  V(ContextOnly)                       \
  V(OnStackWith1Args)                  \
  V(OnStackWith2Args)                  \
  V(OnStackWith3Args)                  \
  V(OnStackWith4Args)                  \
  V(OnStackWith5Args)                  \
  V(OnStackWith6Args)                  \
  V(OnStackWith7Args)                  \
  V(Load)                              \
  V(LoadWithVector)                    \
  V(LoadGlobal)                        \
  V(LoadGlobalWithVector)              \
  V(Store)                             \
  V(StoreWithVector)                   \
  V(StoreTransition)                   \
  V(VectorStoreTransition)             \
  V(VarArgFunction)                    \
  V(FastNewClosure)                    \
  V(FastNewFunctionContext)            \
  V(FastNewObject)                     \
  V(FastNewRestParameter)              \
  V(FastNewSloppyArguments)            \
  V(FastNewStrictArguments)            \
  V(TypeConversion)                    \
  V(Typeof)                            \
  V(FastCloneRegExp)                   \
  V(FastCloneShallowArray)             \
  V(FastCloneShallowObject)            \
  V(CreateAllocationSite)              \
  V(CreateWeakCell)                    \
  V(CallFunction)                      \
  V(CallFunctionWithFeedback)          \
  V(CallFunctionWithFeedbackAndVector) \
  V(CallConstruct)                     \
  V(CallTrampoline)                    \
  V(ConstructStub)                     \
  V(ConstructTrampoline)               \
  V(RegExpConstructResult)             \
  V(CopyFastSmiOrObjectElements)       \
  V(TransitionElementsKind)            \
  V(AllocateHeapNumber)                \
  V(AllocateFloat32x4)                 \
  V(AllocateInt32x4)                   \
  V(AllocateUint32x4)                  \
  V(AllocateBool32x4)                  \
  V(AllocateInt16x8)                   \
  V(AllocateUint16x8)                  \
  V(AllocateBool16x8)                  \
  V(AllocateInt8x16)                   \
  V(AllocateUint8x16)                  \
  V(AllocateBool8x16)                  \
  V(ArrayNoArgumentConstructor)        \
  V(ArraySingleArgumentConstructor)    \
  V(ArrayNArgumentsConstructor)        \
  V(Compare)                           \
  V(BinaryOp)                          \
  V(BinaryOpWithAllocationSite)        \
  V(BinaryOpWithVector)                \
  V(CountOp)                           \
  V(StringAdd)                         \
  V(StringCompare)                     \
  V(Keyed)                             \
  V(Named)                             \
  V(HasProperty)                       \
  V(ForInFilter)                       \
  V(GetProperty)                       \
  V(CallHandler)                       \
  V(ArgumentAdaptor)                   \
  V(ApiCallbackWith0Args)              \
  V(ApiCallbackWith1Args)              \
  V(ApiCallbackWith2Args)              \
  V(ApiCallbackWith3Args)              \
  V(ApiCallbackWith4Args)              \
  V(ApiCallbackWith5Args)              \
  V(ApiCallbackWith6Args)              \
  V(ApiCallbackWith7Args)              \
  V(ApiGetter)                         \
  V(StoreGlobalViaContext)             \
  V(MathPowTagged)                     \
  V(MathPowInteger)                    \
  V(GrowArrayElements)                 \
  V(InterpreterDispatch)               \
  V(InterpreterPushArgsAndCall)        \
  V(InterpreterPushArgsAndConstruct)   \
  V(InterpreterCEntry)                 \
  V(ResumeGenerator)

class CallInterfaceDescriptorData {
 public:
  CallInterfaceDescriptorData() : register_param_count_(-1), param_count_(-1) {}

  // A copy of the passed in registers and param_representations is made
  // and owned by the CallInterfaceDescriptorData.

  void InitializePlatformSpecific(
      int register_parameter_count, const Register* registers,
      PlatformInterfaceDescriptor* platform_descriptor = NULL);

  // if machine_types is null, then an array of size
  // (register_parameter_count + extra_parameter_count) will be created
  // with MachineType::AnyTagged() for each member.
  //
  // if machine_types is not null, then it should be of the size
  // register_parameter_count. Those members of the parameter array
  // will be initialized from {machine_types}, and the rest initialized
  // to MachineType::AnyTagged().
  void InitializePlatformIndependent(int parameter_count,
                                     int extra_parameter_count,
                                     const MachineType* machine_types);

  bool IsInitialized() const {
    return register_param_count_ >= 0 && param_count_ >= 0;
  }

  int param_count() const { return param_count_; }
  int register_param_count() const { return register_param_count_; }
  Register register_param(int index) const { return register_params_[index]; }
  Register* register_params() const { return register_params_.get(); }
  MachineType param_type(int index) const { return machine_types_[index]; }
  PlatformInterfaceDescriptor* platform_specific_descriptor() const {
    return platform_specific_descriptor_;
  }

 private:
  int register_param_count_;
  int param_count_;

  // The Register params are allocated dynamically by the
  // InterfaceDescriptor, and freed on destruction. This is because static
  // arrays of Registers cause creation of runtime static initializers
  // which we don't want.
  std::unique_ptr<Register[]> register_params_;
  std::unique_ptr<MachineType[]> machine_types_;

  PlatformInterfaceDescriptor* platform_specific_descriptor_;

  DISALLOW_COPY_AND_ASSIGN(CallInterfaceDescriptorData);
};


class CallDescriptors {
 public:
  enum Key {
#define DEF_ENUM(name) name,
    INTERFACE_DESCRIPTOR_LIST(DEF_ENUM)
#undef DEF_ENUM
    NUMBER_OF_DESCRIPTORS
  };
};


class CallInterfaceDescriptor {
 public:
  CallInterfaceDescriptor() : data_(NULL) {}
  virtual ~CallInterfaceDescriptor() {}

  CallInterfaceDescriptor(Isolate* isolate, CallDescriptors::Key key)
      : data_(isolate->call_descriptor_data(key)) {}

  int GetParameterCount() const { return data()->param_count(); }

  int GetRegisterParameterCount() const {
    return data()->register_param_count();
  }

  int GetStackParameterCount() const {
    return data()->param_count() - data()->register_param_count();
  }

  Register GetRegisterParameter(int index) const {
    return data()->register_param(index);
  }

  MachineType GetParameterType(int index) const {
    DCHECK(index < data()->param_count());
    return data()->param_type(index);
  }

  // Some platforms have extra information to associate with the descriptor.
  PlatformInterfaceDescriptor* platform_specific_descriptor() const {
    return data()->platform_specific_descriptor();
  }

  static const Register ContextRegister();

  const char* DebugName(Isolate* isolate) const;

 protected:
  const CallInterfaceDescriptorData* data() const { return data_; }

  virtual void InitializePlatformSpecific(CallInterfaceDescriptorData* data) {
    UNREACHABLE();
  }

  virtual void InitializePlatformIndependent(
      CallInterfaceDescriptorData* data) {
    data->InitializePlatformIndependent(data->register_param_count(), 0, NULL);
  }

  void Initialize(Isolate* isolate, CallDescriptors::Key key) {
    if (!data()->IsInitialized()) {
      // We should only initialize descriptors on the isolate's main thread.
      DCHECK(ThreadId::Current().Equals(isolate->thread_id()));
      CallInterfaceDescriptorData* d = isolate->call_descriptor_data(key);
      DCHECK(d == data());  // d should be a modifiable pointer to data().
      InitializePlatformSpecific(d);
      InitializePlatformIndependent(d);
    }
  }

  // Initializes |data| using the platform dependent default set of registers.
  // It is intended to be used for TurboFan stubs when particular set of
  // registers does not matter.
  static void DefaultInitializePlatformSpecific(
      CallInterfaceDescriptorData* data, int register_parameter_count);

 private:
  const CallInterfaceDescriptorData* data_;
};

#define DECLARE_DESCRIPTOR_WITH_BASE(name, base)           \
 public:                                                   \
  explicit name(Isolate* isolate) : base(isolate, key()) { \
    Initialize(isolate, key());                            \
  }                                                        \
  static inline CallDescriptors::Key key();

#define DECLARE_DEFAULT_DESCRIPTOR(name, base, parameter_count)            \
  DECLARE_DESCRIPTOR_WITH_BASE(name, base)                                 \
 protected:                                                                \
  void InitializePlatformSpecific(CallInterfaceDescriptorData* data)       \
      override {                                                           \
    DefaultInitializePlatformSpecific(data, parameter_count);              \
  }                                                                        \
  name(Isolate* isolate, CallDescriptors::Key key) : base(isolate, key) {} \
                                                                           \
 public:

#define DECLARE_DESCRIPTOR(name, base)                                         \
  DECLARE_DESCRIPTOR_WITH_BASE(name, base)                                     \
 protected:                                                                    \
  void InitializePlatformSpecific(CallInterfaceDescriptorData* data) override; \
  name(Isolate* isolate, CallDescriptors::Key key) : base(isolate, key) {}     \
                                                                               \
 public:

#define DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(name, base)        \
  DECLARE_DESCRIPTOR(name, base)                                        \
 protected:                                                             \
  void InitializePlatformIndependent(CallInterfaceDescriptorData* data) \
      override;                                                         \
                                                                        \
 public:

#define DECLARE_DESCRIPTOR_WITH_BASE_AND_FUNCTION_TYPE_ARG(name, base, arg) \
  DECLARE_DESCRIPTOR_WITH_BASE(name, base)                                  \
 protected:                                                                 \
  int extra_args() const override { return arg; }                           \
                                                                            \
 public:

#define DEFINE_PARAMETERS(...)                          \
  enum ParameterIndices {                               \
    __VA_ARGS__,                                        \
                                                        \
    kParameterCount,                                    \
    kContext = kParameterCount /* implicit parameter */ \
  };

class VoidDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(VoidDescriptor, CallInterfaceDescriptor)
};

class ContextOnlyDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(ContextOnlyDescriptor, CallInterfaceDescriptor)
};

// The OnStackWith*ArgsDescriptors have a lot of boilerplate. The superclass
// OnStackArgsDescriptorBase is not meant to be instantiated directly and has no
// public constructors to ensure this is so.contains all the logic, and the
//
// Use OnStackArgsDescriptorBase::ForArgs(isolate, parameter_count) to
// instantiate a descriptor with the number of args.
class OnStackArgsDescriptorBase : public CallInterfaceDescriptor {
 public:
  static CallInterfaceDescriptor ForArgs(Isolate* isolate, int parameter_count);

 protected:
  virtual int extra_args() const { return 0; }
  OnStackArgsDescriptorBase(Isolate* isolate, CallDescriptors::Key key)
      : CallInterfaceDescriptor(isolate, key) {}
  void InitializePlatformSpecific(CallInterfaceDescriptorData* data) override;
  void InitializePlatformIndependent(
      CallInterfaceDescriptorData* data) override;
};

class OnStackWith1ArgsDescriptor : public OnStackArgsDescriptorBase {
 public:
  DECLARE_DESCRIPTOR_WITH_BASE_AND_FUNCTION_TYPE_ARG(OnStackWith1ArgsDescriptor,
                                                     OnStackArgsDescriptorBase,
                                                     1)
};

class OnStackWith2ArgsDescriptor : public OnStackArgsDescriptorBase {
 public:
  DECLARE_DESCRIPTOR_WITH_BASE_AND_FUNCTION_TYPE_ARG(OnStackWith2ArgsDescriptor,
                                                     OnStackArgsDescriptorBase,
                                                     2)
};

class OnStackWith3ArgsDescriptor : public OnStackArgsDescriptorBase {
 public:
  DECLARE_DESCRIPTOR_WITH_BASE_AND_FUNCTION_TYPE_ARG(OnStackWith3ArgsDescriptor,
                                                     OnStackArgsDescriptorBase,
                                                     3)
};

class OnStackWith4ArgsDescriptor : public OnStackArgsDescriptorBase {
 public:
  DECLARE_DESCRIPTOR_WITH_BASE_AND_FUNCTION_TYPE_ARG(OnStackWith4ArgsDescriptor,
                                                     OnStackArgsDescriptorBase,
                                                     4)
};

class OnStackWith5ArgsDescriptor : public OnStackArgsDescriptorBase {
 public:
  DECLARE_DESCRIPTOR_WITH_BASE_AND_FUNCTION_TYPE_ARG(OnStackWith5ArgsDescriptor,
                                                     OnStackArgsDescriptorBase,
                                                     5)
};

class OnStackWith6ArgsDescriptor : public OnStackArgsDescriptorBase {
 public:
  DECLARE_DESCRIPTOR_WITH_BASE_AND_FUNCTION_TYPE_ARG(OnStackWith6ArgsDescriptor,
                                                     OnStackArgsDescriptorBase,
                                                     6)
};

class OnStackWith7ArgsDescriptor : public OnStackArgsDescriptorBase {
 public:
  DECLARE_DESCRIPTOR_WITH_BASE_AND_FUNCTION_TYPE_ARG(OnStackWith7ArgsDescriptor,
                                                     OnStackArgsDescriptorBase,
                                                     7)
};

// LoadDescriptor is used by all stubs that implement Load/KeyedLoad ICs.
class LoadDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kReceiver, kName, kSlot)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(LoadDescriptor,
                                               CallInterfaceDescriptor)

  static const Register ReceiverRegister();
  static const Register NameRegister();
  static const Register SlotRegister();
};

class LoadGlobalDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kSlot)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(LoadGlobalDescriptor,
                                               CallInterfaceDescriptor)

  static const Register SlotRegister() {
    return LoadDescriptor::SlotRegister();
  }
};

class StoreDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kReceiver, kName, kValue, kSlot)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(StoreDescriptor,
                                               CallInterfaceDescriptor)

  static const Register ReceiverRegister();
  static const Register NameRegister();
  static const Register ValueRegister();
  static const Register SlotRegister();
};


class StoreTransitionDescriptor : public StoreDescriptor {
 public:
  DEFINE_PARAMETERS(kReceiver, kName, kValue, kMap)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(StoreTransitionDescriptor,
                                               StoreDescriptor)

  static const Register MapRegister();
};


class VectorStoreTransitionDescriptor : public StoreDescriptor {
 public:
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(VectorStoreTransitionDescriptor,
                                               StoreDescriptor)

  // TODO(ishell): use DEFINE_PARAMETERS macro here
  // Extends StoreDescriptor with Map parameter.
  enum ParameterIndices {
    kReceiver = 0,
    kName = 1,
    kValue = 2,

    kMap = 3,

    kSlot = 4,  // not present on ia32.
    kVirtualSlotVector = 4,

    kVector = 5
  };

  static const Register MapRegister();
  static const Register SlotRegister();
  static const Register VectorRegister();
};

class StoreWithVectorDescriptor : public StoreDescriptor {
 public:
  DEFINE_PARAMETERS(kReceiver, kName, kValue, kSlot, kVector)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(StoreWithVectorDescriptor,
                                               StoreDescriptor)

  static const Register VectorRegister();
};

class LoadWithVectorDescriptor : public LoadDescriptor {
 public:
  DEFINE_PARAMETERS(kReceiver, kName, kSlot, kVector)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(LoadWithVectorDescriptor,
                                               LoadDescriptor)

  static const Register VectorRegister();
};

class LoadGlobalWithVectorDescriptor : public LoadGlobalDescriptor {
 public:
  DEFINE_PARAMETERS(kSlot, kVector)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(LoadGlobalWithVectorDescriptor,
                                               LoadGlobalDescriptor)

  static const Register VectorRegister() {
    return LoadWithVectorDescriptor::VectorRegister();
  }
};

class FastNewClosureDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(FastNewClosureDescriptor, CallInterfaceDescriptor)
};

class FastNewFunctionContextDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kFunction, kSlots)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(FastNewFunctionContextDescriptor,
                                               CallInterfaceDescriptor)

  static const Register FunctionRegister();
  static const Register SlotsRegister();
};

class FastNewObjectDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(FastNewObjectDescriptor, CallInterfaceDescriptor)
};

class FastNewRestParameterDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(FastNewRestParameterDescriptor, CallInterfaceDescriptor)
};

class FastNewSloppyArgumentsDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(FastNewSloppyArgumentsDescriptor,
                     CallInterfaceDescriptor)
};

class FastNewStrictArgumentsDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(FastNewStrictArgumentsDescriptor,
                     CallInterfaceDescriptor)
};

class TypeConversionDescriptor final : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kArgument)
  DECLARE_DESCRIPTOR(TypeConversionDescriptor, CallInterfaceDescriptor)

  static const Register ArgumentRegister();
};

class HasPropertyDescriptor final : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kKey, kObject)
  DECLARE_DEFAULT_DESCRIPTOR(HasPropertyDescriptor, CallInterfaceDescriptor,
                             kParameterCount)
};

class ForInFilterDescriptor final : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kKey, kObject)
  DECLARE_DEFAULT_DESCRIPTOR(ForInFilterDescriptor, CallInterfaceDescriptor,
                             kParameterCount)
};

class GetPropertyDescriptor final : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kObject, kKey)
  DECLARE_DEFAULT_DESCRIPTOR(GetPropertyDescriptor, CallInterfaceDescriptor,
                             kParameterCount)
};

class TypeofDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kObject)
  DECLARE_DESCRIPTOR(TypeofDescriptor, CallInterfaceDescriptor)
};


class FastCloneRegExpDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kClosure, kLiteralIndex, kPattern, kFlags)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(FastCloneRegExpDescriptor,
                                               CallInterfaceDescriptor)
};


class FastCloneShallowArrayDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kClosure, kLiteralIndex, kConstantElements)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(FastCloneShallowArrayDescriptor,
                                               CallInterfaceDescriptor)
};


class FastCloneShallowObjectDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(FastCloneShallowObjectDescriptor, CallInterfaceDescriptor)
};


class CreateAllocationSiteDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kVector, kSlot)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(CreateAllocationSiteDescriptor,
                                               CallInterfaceDescriptor)
};


class CreateWeakCellDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kVector, kSlot, kValue)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(CreateWeakCellDescriptor,
                                               CallInterfaceDescriptor)
};


class CallTrampolineDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kFunction, kActualArgumentsCount)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(CallTrampolineDescriptor,
                                               CallInterfaceDescriptor)
};


class ConstructStubDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kFunction, kNewTarget, kActualArgumentsCount,
                    kAllocationSite)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(ConstructStubDescriptor,
                                               CallInterfaceDescriptor)
};


class ConstructTrampolineDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kFunction, kNewTarget, kActualArgumentsCount)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(ConstructTrampolineDescriptor,
                                               CallInterfaceDescriptor)
};


class CallFunctionDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(CallFunctionDescriptor, CallInterfaceDescriptor)
};


class CallFunctionWithFeedbackDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kFunction, kSlot)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(
      CallFunctionWithFeedbackDescriptor, CallInterfaceDescriptor)
};


class CallFunctionWithFeedbackAndVectorDescriptor
    : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kFunction, kSlot, kVector)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(
      CallFunctionWithFeedbackAndVectorDescriptor, CallInterfaceDescriptor)
};


class CallConstructDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(CallConstructDescriptor, CallInterfaceDescriptor)
};


class RegExpConstructResultDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kLength, kIndex, kInput)
  DECLARE_DESCRIPTOR(RegExpConstructResultDescriptor, CallInterfaceDescriptor)
};


class StoreGlobalViaContextDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kSlot, kValue)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(StoreGlobalViaContextDescriptor,
                                               CallInterfaceDescriptor)

  static const Register SlotRegister();
  static const Register ValueRegister();
};

class CopyFastSmiOrObjectElementsDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kObject)
  DECLARE_DEFAULT_DESCRIPTOR(CopyFastSmiOrObjectElementsDescriptor,
                             CallInterfaceDescriptor, kParameterCount)
};

class TransitionElementsKindDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kObject, kMap)
  DECLARE_DESCRIPTOR(TransitionElementsKindDescriptor, CallInterfaceDescriptor)
};


class AllocateHeapNumberDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(AllocateHeapNumberDescriptor, CallInterfaceDescriptor)
};

#define SIMD128_ALLOC_DESC(TYPE, Type, type, lane_count, lane_type)         \
  class Allocate##Type##Descriptor : public CallInterfaceDescriptor {       \
   public:                                                                  \
    DECLARE_DESCRIPTOR(Allocate##Type##Descriptor, CallInterfaceDescriptor) \
  };
SIMD128_TYPES(SIMD128_ALLOC_DESC)
#undef SIMD128_ALLOC_DESC

class ArrayNoArgumentConstructorDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kFunction, kAllocationSite, kActualArgumentsCount,
                    kFunctionParameter)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(
      ArrayNoArgumentConstructorDescriptor, CallInterfaceDescriptor)
};

class ArraySingleArgumentConstructorDescriptor
    : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kFunction, kAllocationSite, kActualArgumentsCount,
                    kFunctionParameter, kArraySizeSmiParameter)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(
      ArraySingleArgumentConstructorDescriptor, CallInterfaceDescriptor)
};

class ArrayNArgumentsConstructorDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kFunction, kAllocationSite, kActualArgumentsCount)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(
      ArrayNArgumentsConstructorDescriptor, CallInterfaceDescriptor)
};


class CompareDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kLeft, kRight)
  DECLARE_DESCRIPTOR(CompareDescriptor, CallInterfaceDescriptor)
};


class BinaryOpDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kLeft, kRight)
  DECLARE_DESCRIPTOR(BinaryOpDescriptor, CallInterfaceDescriptor)
};


class BinaryOpWithAllocationSiteDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kAllocationSite, kLeft, kRight)
  DECLARE_DESCRIPTOR(BinaryOpWithAllocationSiteDescriptor,
                     CallInterfaceDescriptor)
};

class BinaryOpWithVectorDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kLeft, kRight, kSlot, kVector)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(BinaryOpWithVectorDescriptor,
                                               CallInterfaceDescriptor)
};

class CountOpDescriptor final : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(CountOpDescriptor, CallInterfaceDescriptor)
};

class StringAddDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kLeft, kRight)
  DECLARE_DESCRIPTOR(StringAddDescriptor, CallInterfaceDescriptor)
};


class StringCompareDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kLeft, kRight)
  DECLARE_DESCRIPTOR(StringCompareDescriptor, CallInterfaceDescriptor)

  static const Register LeftRegister();
  static const Register RightRegister();
};

// TODO(ishell): not used, remove.
class KeyedDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(KeyedDescriptor, CallInterfaceDescriptor)
};

// TODO(ishell): not used, remove
class NamedDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(NamedDescriptor, CallInterfaceDescriptor)
};

// TODO(ishell): not used, remove.
class CallHandlerDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(CallHandlerDescriptor, CallInterfaceDescriptor)
};


class ArgumentAdaptorDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kFunction, kNewTarget, kActualArgumentsCount,
                    kExpectedArgumentsCount)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(ArgumentAdaptorDescriptor,
                                               CallInterfaceDescriptor)
};

// The ApiCallback*Descriptors have a lot of boilerplate. The superclass
// ApiCallbackDescriptorBase contains all the logic, and the
// ApiCallbackWith*ArgsDescriptor merely instantiate these with a
// parameter for the number of args.
//
// The base class is not meant to be instantiated directly and has no
// public constructors to ensure this is so.
//
// The simplest usage for all the ApiCallback*Descriptors is probably
//   ApiCallbackDescriptorBase::ForArgs(isolate, argc)
//
class ApiCallbackDescriptorBase : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kFunction, kCallData, kHolder, kApiFunctionAddress)
  static CallInterfaceDescriptor ForArgs(Isolate* isolate, int argc);

 protected:
  virtual int extra_args() const { return 0; }
  ApiCallbackDescriptorBase(Isolate* isolate, CallDescriptors::Key key)
      : CallInterfaceDescriptor(isolate, key) {}
  void InitializePlatformSpecific(CallInterfaceDescriptorData* data) override;
  void InitializePlatformIndependent(
      CallInterfaceDescriptorData* data) override;
};

class ApiCallbackWith0ArgsDescriptor : public ApiCallbackDescriptorBase {
 public:
  DECLARE_DESCRIPTOR_WITH_BASE_AND_FUNCTION_TYPE_ARG(
      ApiCallbackWith0ArgsDescriptor, ApiCallbackDescriptorBase, 0)
};

class ApiCallbackWith1ArgsDescriptor : public ApiCallbackDescriptorBase {
 public:
  DECLARE_DESCRIPTOR_WITH_BASE_AND_FUNCTION_TYPE_ARG(
      ApiCallbackWith1ArgsDescriptor, ApiCallbackDescriptorBase, 1)
};

class ApiCallbackWith2ArgsDescriptor : public ApiCallbackDescriptorBase {
 public:
  DECLARE_DESCRIPTOR_WITH_BASE_AND_FUNCTION_TYPE_ARG(
      ApiCallbackWith2ArgsDescriptor, ApiCallbackDescriptorBase, 2)
};

class ApiCallbackWith3ArgsDescriptor : public ApiCallbackDescriptorBase {
 public:
  DECLARE_DESCRIPTOR_WITH_BASE_AND_FUNCTION_TYPE_ARG(
      ApiCallbackWith3ArgsDescriptor, ApiCallbackDescriptorBase, 3)
};

class ApiCallbackWith4ArgsDescriptor : public ApiCallbackDescriptorBase {
 public:
  DECLARE_DESCRIPTOR_WITH_BASE_AND_FUNCTION_TYPE_ARG(
      ApiCallbackWith4ArgsDescriptor, ApiCallbackDescriptorBase, 4)
};

class ApiCallbackWith5ArgsDescriptor : public ApiCallbackDescriptorBase {
 public:
  DECLARE_DESCRIPTOR_WITH_BASE_AND_FUNCTION_TYPE_ARG(
      ApiCallbackWith5ArgsDescriptor, ApiCallbackDescriptorBase, 5)
};

class ApiCallbackWith6ArgsDescriptor : public ApiCallbackDescriptorBase {
 public:
  DECLARE_DESCRIPTOR_WITH_BASE_AND_FUNCTION_TYPE_ARG(
      ApiCallbackWith6ArgsDescriptor, ApiCallbackDescriptorBase, 6)
};

class ApiCallbackWith7ArgsDescriptor : public ApiCallbackDescriptorBase {
 public:
  DECLARE_DESCRIPTOR_WITH_BASE_AND_FUNCTION_TYPE_ARG(
      ApiCallbackWith7ArgsDescriptor, ApiCallbackDescriptorBase, 7)
};


class ApiGetterDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kReceiver, kHolder, kCallback)
  DECLARE_DESCRIPTOR(ApiGetterDescriptor, CallInterfaceDescriptor)

  static const Register ReceiverRegister();
  static const Register HolderRegister();
  static const Register CallbackRegister();
};

class MathPowTaggedDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kExponent)
  DECLARE_DESCRIPTOR(MathPowTaggedDescriptor, CallInterfaceDescriptor)

  static const Register exponent();
};

class MathPowIntegerDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kExponent)
  DECLARE_DESCRIPTOR(MathPowIntegerDescriptor, CallInterfaceDescriptor)

  static const Register exponent();
};

class VarArgFunctionDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kActualArgumentsCount)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(VarArgFunctionDescriptor,
                                               CallInterfaceDescriptor)
};

// TODO(turbofan): We should probably rename this to GrowFastElementsDescriptor.
class GrowArrayElementsDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kObject, kKey)
  DECLARE_DESCRIPTOR(GrowArrayElementsDescriptor, CallInterfaceDescriptor)

  static const Register ObjectRegister();
  static const Register KeyRegister();
};

class InterpreterDispatchDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kAccumulator, kBytecodeOffset, kBytecodeArray,
                    kDispatchTable)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(InterpreterDispatchDescriptor,
                                               CallInterfaceDescriptor)
};

class InterpreterPushArgsAndCallDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(InterpreterPushArgsAndCallDescriptor,
                     CallInterfaceDescriptor)
};


class InterpreterPushArgsAndConstructDescriptor
    : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(InterpreterPushArgsAndConstructDescriptor,
                     CallInterfaceDescriptor)
};


class InterpreterCEntryDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(InterpreterCEntryDescriptor, CallInterfaceDescriptor)
};

class ResumeGeneratorDescriptor final : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(ResumeGeneratorDescriptor, CallInterfaceDescriptor)
};

#undef DECLARE_DESCRIPTOR_WITH_BASE
#undef DECLARE_DESCRIPTOR
#undef DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE
#undef DECLARE_DESCRIPTOR_WITH_BASE_AND_FUNCTION_TYPE_ARG
#undef DEFINE_PARAMETERS

// We define the association between CallDescriptors::Key and the specialized
// descriptor here to reduce boilerplate and mistakes.
#define DEF_KEY(name) \
  CallDescriptors::Key name##Descriptor::key() { return CallDescriptors::name; }
INTERFACE_DESCRIPTOR_LIST(DEF_KEY)
#undef DEF_KEY
}  // namespace internal
}  // namespace v8


#if V8_TARGET_ARCH_ARM64
#include "src/arm64/interface-descriptors-arm64.h"
#elif V8_TARGET_ARCH_ARM
#include "src/arm/interface-descriptors-arm.h"
#endif

#endif  // V8_CALL_INTERFACE_DESCRIPTOR_H_
