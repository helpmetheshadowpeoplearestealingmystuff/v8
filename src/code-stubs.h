// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODE_STUBS_H_
#define V8_CODE_STUBS_H_

#include "src/allocation.h"
#include "src/assembler.h"
#include "src/codegen.h"
#include "src/globals.h"
#include "src/ic/ic.h"
#include "src/interface-descriptors.h"
#include "src/macro-assembler.h"
#include "src/ostreams.h"

namespace v8 {
namespace internal {

// List of code stubs used on all platforms.
#define CODE_STUB_LIST_ALL_PLATFORMS(V)     \
  /* PlatformCodeStubs */                   \
  V(ArgumentsAccess)                        \
  V(ArrayConstructor)                       \
  V(BinaryOpICWithAllocationSite)           \
  V(CallApiFunction)                        \
  V(CallApiGetter)                          \
  V(CallConstruct)                          \
  V(CallFunction)                           \
  V(CallIC)                                 \
  V(CallIC_Array)                           \
  V(CEntry)                                 \
  V(CompareIC)                              \
  V(DoubleToI)                              \
  V(FunctionPrototype)                      \
  V(Instanceof)                             \
  V(InternalArrayConstructor)               \
  V(JSEntry)                                \
  V(KeyedLoadICTrampoline)                  \
  V(LoadICTrampoline)                       \
  V(MathPow)                                \
  V(ProfileEntryHook)                       \
  V(RecordWrite)                            \
  V(RegExpExec)                             \
  V(StoreArrayLiteralElement)               \
  V(StoreBufferOverflow)                    \
  V(StoreElement)                           \
  V(StringCompare)                          \
  V(StubFailureTrampoline)                  \
  V(SubString)                              \
  /* HydrogenCodeStubs */                   \
  V(ArrayNArgumentsConstructor)             \
  V(ArrayNoArgumentConstructor)             \
  V(ArraySingleArgumentConstructor)         \
  V(BinaryOpIC)                             \
  V(BinaryOpWithAllocationSite)             \
  V(CompareNilIC)                           \
  V(CreateAllocationSite)                   \
  V(ElementsTransitionAndStore)             \
  V(FastCloneShallowArray)                  \
  V(FastCloneShallowObject)                 \
  V(FastNewClosure)                         \
  V(FastNewContext)                         \
  V(InternalArrayNArgumentsConstructor)     \
  V(InternalArrayNoArgumentConstructor)     \
  V(InternalArraySingleArgumentConstructor) \
  V(KeyedLoadGeneric)                       \
  V(LoadDictionaryElement)                  \
  V(LoadFastElement)                        \
  V(NameDictionaryLookup)                   \
  V(NumberToString)                         \
  V(RegExpConstructResult)                  \
  V(StoreFastElement)                       \
  V(StringAdd)                              \
  V(ToBoolean)                              \
  V(ToNumber)                               \
  V(TransitionElementsKind)                 \
  V(VectorKeyedLoad)                        \
  V(VectorLoad)                             \
  /* IC Handler stubs */                    \
  V(LoadConstant)                           \
  V(LoadField)                              \
  V(StoreField)                             \
  V(StoreGlobal)                            \
  V(StringLength)

// List of code stubs only used on ARM 32 bits platforms.
#if V8_TARGET_ARCH_ARM
#define CODE_STUB_LIST_ARM(V) \
  V(DirectCEntry)             \
  V(WriteInt32ToHeapNumber)

#else
#define CODE_STUB_LIST_ARM(V)
#endif

// List of code stubs only used on ARM 64 bits platforms.
#if V8_TARGET_ARCH_ARM64
#define CODE_STUB_LIST_ARM64(V) \
  V(DirectCEntry)               \
  V(RestoreRegistersState)      \
  V(StoreRegistersState)

#else
#define CODE_STUB_LIST_ARM64(V)
#endif

// List of code stubs only used on MIPS platforms.
#if V8_TARGET_ARCH_MIPS
#define CODE_STUB_LIST_MIPS(V)  \
  V(DirectCEntry)               \
  V(RestoreRegistersState)      \
  V(StoreRegistersState)        \
  V(WriteInt32ToHeapNumber)
#elif V8_TARGET_ARCH_MIPS64
#define CODE_STUB_LIST_MIPS(V)  \
  V(DirectCEntry)               \
  V(RestoreRegistersState)      \
  V(StoreRegistersState)        \
  V(WriteInt32ToHeapNumber)
#else
#define CODE_STUB_LIST_MIPS(V)
#endif

// Combined list of code stubs.
#define CODE_STUB_LIST(V)            \
  CODE_STUB_LIST_ALL_PLATFORMS(V)    \
  CODE_STUB_LIST_ARM(V)              \
  CODE_STUB_LIST_ARM64(V)           \
  CODE_STUB_LIST_MIPS(V)

// Stub is base classes of all stubs.
class CodeStub BASE_EMBEDDED {
 public:
  enum Major {
    UninitializedMajorKey = 0,
#define DEF_ENUM(name) name,
    CODE_STUB_LIST(DEF_ENUM)
#undef DEF_ENUM
    NoCache,  // marker for stubs that do custom caching
    NUMBER_OF_IDS
  };

  // Retrieve the code for the stub. Generate the code if needed.
  Handle<Code> GetCode();

  // Retrieve the code for the stub, make and return a copy of the code.
  Handle<Code> GetCodeCopy(const Code::FindAndReplacePattern& pattern);

  static Major MajorKeyFromKey(uint32_t key) {
    return static_cast<Major>(MajorKeyBits::decode(key));
  }
  static uint32_t MinorKeyFromKey(uint32_t key) {
    return MinorKeyBits::decode(key);
  }

  // Gets the major key from a code object that is a code stub or binary op IC.
  static Major GetMajorKey(Code* code_stub) {
    return MajorKeyFromKey(code_stub->stub_key());
  }

  static uint32_t NoCacheKey() { return MajorKeyBits::encode(NoCache); }

  static const char* MajorName(Major major_key, bool allow_unknown_keys);

  explicit CodeStub(Isolate* isolate) : minor_key_(0), isolate_(isolate) {}
  virtual ~CodeStub() {}

  static void GenerateStubsAheadOfTime(Isolate* isolate);
  static void GenerateFPStubs(Isolate* isolate);

  // Some stubs put untagged junk on the stack that cannot be scanned by the
  // GC.  This means that we must be statically sure that no GC can occur while
  // they are running.  If that is the case they should override this to return
  // true, which will cause an assertion if we try to call something that can
  // GC or if we try to put a stack frame on top of the junk, which would not
  // result in a traversable stack.
  virtual bool SometimesSetsUpAFrame() { return true; }

  // Lookup the code in the (possibly custom) cache.
  bool FindCodeInCache(Code** code_out);

  // Returns information for computing the number key.
  virtual Major MajorKey() const = 0;
  uint32_t MinorKey() const { return minor_key_; }

  virtual InlineCacheState GetICState() const { return UNINITIALIZED; }
  virtual ExtraICState GetExtraICState() const { return kNoExtraICState; }
  virtual Code::StubType GetStubType() {
    return Code::NORMAL;
  }

  friend OStream& operator<<(OStream& os, const CodeStub& s) {
    s.PrintName(os);
    return os;
  }

  Isolate* isolate() const { return isolate_; }

 protected:
  explicit CodeStub(uint32_t key) : minor_key_(MinorKeyFromKey(key)) {}

  // Generates the assembler code for the stub.
  virtual Handle<Code> GenerateCode() = 0;

  // Returns whether the code generated for this stub needs to be allocated as
  // a fixed (non-moveable) code object.
  virtual bool NeedsImmovableCode() { return false; }

  virtual void PrintName(OStream& os) const;        // NOLINT
  virtual void PrintBaseName(OStream& os) const;    // NOLINT
  virtual void PrintState(OStream& os) const { ; }  // NOLINT

  // Computes the key based on major and minor.
  uint32_t GetKey() {
    DCHECK(static_cast<int>(MajorKey()) < NUMBER_OF_IDS);
    return MinorKeyBits::encode(MinorKey()) | MajorKeyBits::encode(MajorKey());
  }

  uint32_t minor_key_;

 private:
  // Perform bookkeeping required after code generation when stub code is
  // initially generated.
  void RecordCodeGeneration(Handle<Code> code);

  // Finish the code object after it has been generated.
  virtual void FinishCode(Handle<Code> code) { }

  // Activate newly generated stub. Is called after
  // registering stub in the stub cache.
  virtual void Activate(Code* code) { }

  // BinaryOpStub needs to override this.
  virtual Code::Kind GetCodeKind() const;

  // Add the code to a specialized cache, specific to an individual
  // stub type. Please note, this method must add the code object to a
  // roots object, otherwise we will remove the code during GC.
  virtual void AddToSpecialCache(Handle<Code> new_object) { }

  // Find code in a specialized cache, work is delegated to the specific stub.
  virtual bool FindCodeInSpecialCache(Code** code_out) {
    return false;
  }

  // If a stub uses a special cache override this.
  virtual bool UseSpecialCache() { return false; }

  STATIC_ASSERT(NUMBER_OF_IDS < (1 << kStubMajorKeyBits));
  class MajorKeyBits: public BitField<uint32_t, 0, kStubMajorKeyBits> {};
  class MinorKeyBits: public BitField<uint32_t,
      kStubMajorKeyBits, kStubMinorKeyBits> {};  // NOLINT

  friend class BreakPointIterator;

  Isolate* isolate_;
};


class PlatformCodeStub : public CodeStub {
 public:
  explicit PlatformCodeStub(Isolate* isolate) : CodeStub(isolate) { }

  // Retrieve the code for the stub. Generate the code if needed.
  virtual Handle<Code> GenerateCode() OVERRIDE;

  virtual Code::Kind GetCodeKind() const { return Code::STUB; }

 protected:
  // Generates the assembler code for the stub.
  virtual void Generate(MacroAssembler* masm) = 0;

  explicit PlatformCodeStub(uint32_t key) : CodeStub(key) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PlatformCodeStub);
};


enum StubFunctionMode { NOT_JS_FUNCTION_STUB_MODE, JS_FUNCTION_STUB_MODE };
enum HandlerArgumentsMode { DONT_PASS_ARGUMENTS, PASS_ARGUMENTS };


class CodeStubInterfaceDescriptor {
 public:
  CodeStubInterfaceDescriptor();

  void Initialize(CodeStub::Major major,
                  CallInterfaceDescriptor call_descriptor,
                  Address deoptimization_handler = NULL,
                  int hint_stack_parameter_count = -1,
                  StubFunctionMode function_mode = NOT_JS_FUNCTION_STUB_MODE);
  void Initialize(CodeStub::Major major,
                  CallInterfaceDescriptor call_descriptor,
                  Register stack_parameter_count,
                  Address deoptimization_handler = NULL,
                  int hint_stack_parameter_count = -1,
                  StubFunctionMode function_mode = NOT_JS_FUNCTION_STUB_MODE,
                  HandlerArgumentsMode handler_mode = DONT_PASS_ARGUMENTS);

  void SetMissHandler(ExternalReference handler) {
    miss_handler_ = handler;
    has_miss_handler_ = true;
    // Our miss handler infrastructure doesn't currently support
    // variable stack parameter counts.
    DCHECK(!stack_parameter_count_.is_valid());
  }

  bool IsInitialized() const { return call_descriptor_.IsInitialized(); }

  CallInterfaceDescriptor call_descriptor() const { return call_descriptor_; }

  int GetEnvironmentLength() const {
    return call_descriptor().GetEnvironmentLength();
  }

  int GetRegisterParameterCount() const {
    return call_descriptor().GetRegisterParameterCount();
  }

  Register GetParameterRegister(int index) const {
    return call_descriptor().GetParameterRegister(index);
  }

  Representation GetParameterRepresentation(int index) const {
    return call_descriptor().GetParameterRepresentation(index);
  }

  int GetEnvironmentParameterCount() const {
    return call_descriptor().GetEnvironmentParameterCount();
  }

  Register GetEnvironmentParameterRegister(int index) const {
    return call_descriptor().GetEnvironmentParameterRegister(index);
  }

  Representation GetEnvironmentParameterRepresentation(int index) const {
    return call_descriptor().GetEnvironmentParameterRepresentation(index);
  }

  ExternalReference miss_handler() const {
    DCHECK(has_miss_handler_);
    return miss_handler_;
  }

  bool has_miss_handler() const {
    return has_miss_handler_;
  }

  bool IsEnvironmentParameterCountRegister(int index) const {
    return call_descriptor().GetEnvironmentParameterRegister(index).is(
        stack_parameter_count_);
  }

  int GetHandlerParameterCount() const {
    int params = call_descriptor().GetEnvironmentParameterCount();
    if (handler_arguments_mode_ == PASS_ARGUMENTS) {
      params += 1;
    }
    return params;
  }

  int hint_stack_parameter_count() const { return hint_stack_parameter_count_; }
  Register stack_parameter_count() const { return stack_parameter_count_; }
  StubFunctionMode function_mode() const { return function_mode_; }
  Address deoptimization_handler() const { return deoptimization_handler_; }
  CodeStub::Major MajorKey() const { return major_; }

 private:
  CallInterfaceDescriptor call_descriptor_;
  Register stack_parameter_count_;
  // If hint_stack_parameter_count_ > 0, the code stub can optimize the
  // return sequence. Default value is -1, which means it is ignored.
  int hint_stack_parameter_count_;
  StubFunctionMode function_mode_;

  Address deoptimization_handler_;
  HandlerArgumentsMode handler_arguments_mode_;

  ExternalReference miss_handler_;
  bool has_miss_handler_;
  CodeStub::Major major_;
};


class HydrogenCodeStub : public CodeStub {
 public:
  enum InitializationState {
    UNINITIALIZED,
    INITIALIZED
  };

  explicit HydrogenCodeStub(Isolate* isolate,
                            InitializationState state = INITIALIZED)
      : CodeStub(isolate) {
    minor_key_ = IsMissBits::encode(state == UNINITIALIZED);
  }

  virtual Code::Kind GetCodeKind() const { return Code::STUB; }

  CodeStubInterfaceDescriptor* GetInterfaceDescriptor() {
    return isolate()->code_stub_interface_descriptor(MajorKey());
  }

  template<class SubClass>
  static Handle<Code> GetUninitialized(Isolate* isolate) {
    SubClass::GenerateAheadOfTime(isolate);
    return SubClass().GetCode(isolate);
  }

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) = 0;

  // Retrieve the code for the stub. Generate the code if needed.
  virtual Handle<Code> GenerateCode() = 0;

  bool IsUninitialized() const { return IsMissBits::decode(minor_key_); }

  Handle<Code> GenerateLightweightMissCode();

  template<class StateType>
  void TraceTransition(StateType from, StateType to);

 protected:
  void set_sub_minor_key(uint32_t key) {
    minor_key_ = SubMinorKeyBits::update(minor_key_, key);
  }

  uint32_t sub_minor_key() const { return SubMinorKeyBits::decode(minor_key_); }

  static const int kSubMinorKeyBits = kStubMinorKeyBits - 1;

 private:
  class IsMissBits : public BitField<bool, kSubMinorKeyBits, 1> {};
  class SubMinorKeyBits : public BitField<int, 0, kSubMinorKeyBits> {};

  void GenerateLightweightMiss(MacroAssembler* masm);

  DISALLOW_COPY_AND_ASSIGN(HydrogenCodeStub);
};


// Helper interface to prepare to/restore after making runtime calls.
class RuntimeCallHelper {
 public:
  virtual ~RuntimeCallHelper() {}

  virtual void BeforeCall(MacroAssembler* masm) const = 0;

  virtual void AfterCall(MacroAssembler* masm) const = 0;

 protected:
  RuntimeCallHelper() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(RuntimeCallHelper);
};


} }  // namespace v8::internal

#if V8_TARGET_ARCH_IA32
#include "src/ia32/code-stubs-ia32.h"
#elif V8_TARGET_ARCH_X64
#include "src/x64/code-stubs-x64.h"
#elif V8_TARGET_ARCH_ARM64
#include "src/arm64/code-stubs-arm64.h"
#elif V8_TARGET_ARCH_ARM
#include "src/arm/code-stubs-arm.h"
#elif V8_TARGET_ARCH_MIPS
#include "src/mips/code-stubs-mips.h"
#elif V8_TARGET_ARCH_MIPS64
#include "src/mips64/code-stubs-mips64.h"
#elif V8_TARGET_ARCH_X87
#include "src/x87/code-stubs-x87.h"
#else
#error Unsupported target architecture.
#endif

namespace v8 {
namespace internal {


// RuntimeCallHelper implementation used in stubs: enters/leaves a
// newly created internal frame before/after the runtime call.
class StubRuntimeCallHelper : public RuntimeCallHelper {
 public:
  StubRuntimeCallHelper() {}

  virtual void BeforeCall(MacroAssembler* masm) const;

  virtual void AfterCall(MacroAssembler* masm) const;
};


// Trivial RuntimeCallHelper implementation.
class NopRuntimeCallHelper : public RuntimeCallHelper {
 public:
  NopRuntimeCallHelper() {}

  virtual void BeforeCall(MacroAssembler* masm) const {}

  virtual void AfterCall(MacroAssembler* masm) const {}
};


class ToNumberStub: public HydrogenCodeStub {
 public:
  explicit ToNumberStub(Isolate* isolate) : HydrogenCodeStub(isolate) { }

  virtual Handle<Code> GenerateCode() OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) OVERRIDE;

  static void InstallDescriptors(Isolate* isolate) {
    ToNumberStub stub(isolate);
    stub.InitializeInterfaceDescriptor(
        isolate->code_stub_interface_descriptor(CodeStub::ToNumber));
  }

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ToNumberStub);
};


class NumberToStringStub FINAL : public HydrogenCodeStub {
 public:
  explicit NumberToStringStub(Isolate* isolate) : HydrogenCodeStub(isolate) {}

  virtual Handle<Code> GenerateCode() OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) OVERRIDE;

  static void InstallDescriptors(Isolate* isolate);

  // Parameters accessed via CodeStubGraphBuilder::GetParameter()
  static const int kNumber = 0;

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(NumberToStringStub);
};


class FastNewClosureStub : public HydrogenCodeStub {
 public:
  FastNewClosureStub(Isolate* isolate, StrictMode strict_mode,
                     bool is_generator)
      : HydrogenCodeStub(isolate) {
    set_sub_minor_key(StrictModeBits::encode(strict_mode) |
                      IsGeneratorBits::encode(is_generator));
  }

  virtual Handle<Code> GenerateCode() OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) OVERRIDE;

  static void InstallDescriptors(Isolate* isolate);

  StrictMode strict_mode() const {
    return StrictModeBits::decode(sub_minor_key());
  }

  bool is_generator() const { return IsGeneratorBits::decode(sub_minor_key()); }

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  class StrictModeBits : public BitField<StrictMode, 0, 1> {};
  class IsGeneratorBits : public BitField<bool, 1, 1> {};

  DISALLOW_COPY_AND_ASSIGN(FastNewClosureStub);
};


class FastNewContextStub FINAL : public HydrogenCodeStub {
 public:
  static const int kMaximumSlots = 64;

  FastNewContextStub(Isolate* isolate, int slots) : HydrogenCodeStub(isolate) {
    DCHECK(slots > 0 && slots <= kMaximumSlots);
    set_sub_minor_key(SlotsBits::encode(slots));
  }

  virtual Handle<Code> GenerateCode() OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) OVERRIDE;

  static void InstallDescriptors(Isolate* isolate);

  int slots() const { return SlotsBits::decode(sub_minor_key()); }

  // Parameters accessed via CodeStubGraphBuilder::GetParameter()
  static const int kFunction = 0;

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  class SlotsBits : public BitField<int, 0, 8> {};

  DISALLOW_COPY_AND_ASSIGN(FastNewContextStub);
};


class FastCloneShallowArrayStub : public HydrogenCodeStub {
 public:
  FastCloneShallowArrayStub(Isolate* isolate,
                            AllocationSiteMode allocation_site_mode)
      : HydrogenCodeStub(isolate) {
    set_sub_minor_key(AllocationSiteModeBits::encode(allocation_site_mode));
  }

  AllocationSiteMode allocation_site_mode() const {
    return AllocationSiteModeBits::decode(sub_minor_key());
  }

  virtual Handle<Code> GenerateCode();

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) OVERRIDE;

  static void InstallDescriptors(Isolate* isolate);

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  class AllocationSiteModeBits: public BitField<AllocationSiteMode, 0, 1> {};

  DISALLOW_COPY_AND_ASSIGN(FastCloneShallowArrayStub);
};


class FastCloneShallowObjectStub : public HydrogenCodeStub {
 public:
  // Maximum number of properties in copied object.
  static const int kMaximumClonedProperties = 6;

  FastCloneShallowObjectStub(Isolate* isolate, int length)
      : HydrogenCodeStub(isolate) {
    DCHECK_GE(length, 0);
    DCHECK_LE(length, kMaximumClonedProperties);
    set_sub_minor_key(LengthBits::encode(length));
  }

  int length() const { return LengthBits::decode(sub_minor_key()); }

  virtual Handle<Code> GenerateCode() OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) OVERRIDE;

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  class LengthBits : public BitField<int, 0, 4> {};

  DISALLOW_COPY_AND_ASSIGN(FastCloneShallowObjectStub);
};


class CreateAllocationSiteStub : public HydrogenCodeStub {
 public:
  explicit CreateAllocationSiteStub(Isolate* isolate)
      : HydrogenCodeStub(isolate) { }

  virtual Handle<Code> GenerateCode() OVERRIDE;

  static void GenerateAheadOfTime(Isolate* isolate);

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) OVERRIDE;

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(CreateAllocationSiteStub);
};


class InstanceofStub: public PlatformCodeStub {
 public:
  enum Flags {
    kNoFlags = 0,
    kArgsInRegisters = 1 << 0,
    kCallSiteInlineCheck = 1 << 1,
    kReturnTrueFalseObject = 1 << 2
  };

  InstanceofStub(Isolate* isolate, Flags flags) : PlatformCodeStub(isolate) {
    minor_key_ = FlagBits::encode(flags);
  }

  void Generate(MacroAssembler* masm);

  static Register left() { return InstanceofDescriptor::left(); }
  static Register right() { return InstanceofDescriptor::right(); }

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor);

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  Flags flags() const { return FlagBits::decode(minor_key_); }

  bool HasArgsInRegisters() const { return (flags() & kArgsInRegisters) != 0; }

  bool HasCallSiteInlineCheck() const {
    return (flags() & kCallSiteInlineCheck) != 0;
  }

  bool ReturnTrueFalseObject() const {
    return (flags() & kReturnTrueFalseObject) != 0;
  }

  virtual void PrintName(OStream& os) const OVERRIDE;  // NOLINT

  class FlagBits : public BitField<Flags, 0, 3> {};

  DISALLOW_COPY_AND_ASSIGN(InstanceofStub);
};


enum AllocationSiteOverrideMode {
  DONT_OVERRIDE,
  DISABLE_ALLOCATION_SITES,
  LAST_ALLOCATION_SITE_OVERRIDE_MODE = DISABLE_ALLOCATION_SITES
};


class ArrayConstructorStub: public PlatformCodeStub {
 public:
  enum ArgumentCountKey { ANY, NONE, ONE, MORE_THAN_ONE };
  ArrayConstructorStub(Isolate* isolate, int argument_count);

  explicit ArrayConstructorStub(Isolate* isolate);

  void Generate(MacroAssembler* masm);

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  ArgumentCountKey argument_count() const {
    return ArgumentCountBits::decode(minor_key_);
  }

  void GenerateDispatchToArrayStub(MacroAssembler* masm,
                                   AllocationSiteOverrideMode mode);

  virtual void PrintName(OStream& os) const OVERRIDE;  // NOLINT

  class ArgumentCountBits : public BitField<ArgumentCountKey, 0, 2> {};

  DISALLOW_COPY_AND_ASSIGN(ArrayConstructorStub);
};


class InternalArrayConstructorStub: public PlatformCodeStub {
 public:
  explicit InternalArrayConstructorStub(Isolate* isolate);

  void Generate(MacroAssembler* masm);

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  void GenerateCase(MacroAssembler* masm, ElementsKind kind);

  DISALLOW_COPY_AND_ASSIGN(InternalArrayConstructorStub);
};


class MathPowStub: public PlatformCodeStub {
 public:
  enum ExponentType { INTEGER, DOUBLE, TAGGED, ON_STACK };

  MathPowStub(Isolate* isolate, ExponentType exponent_type)
      : PlatformCodeStub(isolate) {
    minor_key_ = ExponentTypeBits::encode(exponent_type);
  }

  virtual void Generate(MacroAssembler* masm);

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  ExponentType exponent_type() const {
    return ExponentTypeBits::decode(minor_key_);
  }

  class ExponentTypeBits : public BitField<ExponentType, 0, 2> {};

  DISALLOW_COPY_AND_ASSIGN(MathPowStub);
};


class CallICStub: public PlatformCodeStub {
 public:
  CallICStub(Isolate* isolate, const CallIC::State& state)
      : PlatformCodeStub(isolate) {
    minor_key_ = state.GetExtraICState();
  }

  static int ExtractArgcFromMinorKey(int minor_key) {
    CallIC::State state(static_cast<ExtraICState>(minor_key));
    return state.arg_count();
  }

  virtual void Generate(MacroAssembler* masm);

  virtual Code::Kind GetCodeKind() const OVERRIDE { return Code::CALL_IC; }

  virtual InlineCacheState GetICState() const OVERRIDE { return DEFAULT; }

  virtual ExtraICState GetExtraICState() const FINAL OVERRIDE {
    return static_cast<ExtraICState>(minor_key_);
  }

 protected:
  bool CallAsMethod() const { return state().call_type() == CallIC::METHOD; }

  int arg_count() const { return state().arg_count(); }

  CallIC::State state() const {
    return CallIC::State(static_cast<ExtraICState>(minor_key_));
  }

  // Code generation helpers.
  void GenerateMiss(MacroAssembler* masm, IC::UtilityId id);

 private:
  virtual inline Major MajorKey() const OVERRIDE;

  virtual void PrintState(OStream& os) const OVERRIDE;  // NOLINT

  DISALLOW_COPY_AND_ASSIGN(CallICStub);
};


class CallIC_ArrayStub: public CallICStub {
 public:
  CallIC_ArrayStub(Isolate* isolate, const CallIC::State& state_in)
      : CallICStub(isolate, state_in) {}

  virtual void Generate(MacroAssembler* masm);

  virtual InlineCacheState GetICState() const FINAL OVERRIDE {
    return MONOMORPHIC;
  }

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  virtual void PrintState(OStream& os) const OVERRIDE;  // NOLINT

  DISALLOW_COPY_AND_ASSIGN(CallIC_ArrayStub);
};


// TODO(verwaest): Translate to hydrogen code stub.
class FunctionPrototypeStub : public PlatformCodeStub {
 public:
  explicit FunctionPrototypeStub(Isolate* isolate)
      : PlatformCodeStub(isolate) {}
  virtual void Generate(MacroAssembler* masm);
  virtual Code::Kind GetCodeKind() const { return Code::HANDLER; }

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(FunctionPrototypeStub);
};


class HandlerStub : public HydrogenCodeStub {
 public:
  virtual Code::Kind GetCodeKind() const { return Code::HANDLER; }
  virtual ExtraICState GetExtraICState() const { return kind(); }
  virtual InlineCacheState GetICState() const { return MONOMORPHIC; }

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) OVERRIDE;

 protected:
  explicit HandlerStub(Isolate* isolate) : HydrogenCodeStub(isolate) {}
  virtual Code::Kind kind() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(HandlerStub);
};


class LoadFieldStub: public HandlerStub {
 public:
  LoadFieldStub(Isolate* isolate, FieldIndex index) : HandlerStub(isolate) {
    int property_index_key = index.GetFieldAccessStubKey();
    set_sub_minor_key(LoadFieldByIndexBits::encode(property_index_key));
  }

  virtual Handle<Code> GenerateCode() OVERRIDE;

  FieldIndex index() const {
    int property_index_key = LoadFieldByIndexBits::decode(sub_minor_key());
    return FieldIndex::FromFieldAccessStubKey(property_index_key);
  }

 protected:
  explicit LoadFieldStub(Isolate* isolate);
  virtual Code::Kind kind() const { return Code::LOAD_IC; }
  virtual Code::StubType GetStubType() { return Code::FAST; }

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  class LoadFieldByIndexBits : public BitField<int, 0, 13> {};

  DISALLOW_COPY_AND_ASSIGN(LoadFieldStub);
};


class LoadConstantStub : public HandlerStub {
 public:
  LoadConstantStub(Isolate* isolate, int constant_index)
      : HandlerStub(isolate) {
    set_sub_minor_key(ConstantIndexBits::encode(constant_index));
  }

  virtual Handle<Code> GenerateCode() OVERRIDE;

  int constant_index() const {
    return ConstantIndexBits::decode(sub_minor_key());
  }

 protected:
  explicit LoadConstantStub(Isolate* isolate);
  virtual Code::Kind kind() const { return Code::LOAD_IC; }
  virtual Code::StubType GetStubType() { return Code::FAST; }

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  class ConstantIndexBits : public BitField<int, 0, kSubMinorKeyBits> {};

  DISALLOW_COPY_AND_ASSIGN(LoadConstantStub);
};


class StringLengthStub: public HandlerStub {
 public:
  explicit StringLengthStub(Isolate* isolate) : HandlerStub(isolate) {}
  virtual Handle<Code> GenerateCode() OVERRIDE;

 protected:
  virtual Code::Kind kind() const { return Code::LOAD_IC; }
  virtual Code::StubType GetStubType() { return Code::FAST; }

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(StringLengthStub);
};


class StoreFieldStub : public HandlerStub {
 public:
  StoreFieldStub(Isolate* isolate, FieldIndex index,
                 Representation representation)
      : HandlerStub(isolate) {
    int property_index_key = index.GetFieldAccessStubKey();
    uint8_t repr = PropertyDetails::EncodeRepresentation(representation);
    set_sub_minor_key(StoreFieldByIndexBits::encode(property_index_key) |
                      RepresentationBits::encode(repr));
  }

  virtual Handle<Code> GenerateCode() OVERRIDE;

  FieldIndex index() const {
    int property_index_key = StoreFieldByIndexBits::decode(sub_minor_key());
    return FieldIndex::FromFieldAccessStubKey(property_index_key);
  }

  Representation representation() {
    uint8_t repr = RepresentationBits::decode(sub_minor_key());
    return PropertyDetails::DecodeRepresentation(repr);
  }

  static void InstallDescriptors(Isolate* isolate);

 protected:
  explicit StoreFieldStub(Isolate* isolate);
  virtual Code::Kind kind() const { return Code::STORE_IC; }
  virtual Code::StubType GetStubType() { return Code::FAST; }

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  class StoreFieldByIndexBits : public BitField<int, 0, 13> {};
  class RepresentationBits : public BitField<uint8_t, 13, 4> {};

  DISALLOW_COPY_AND_ASSIGN(StoreFieldStub);
};


class StoreGlobalStub : public HandlerStub {
 public:
  StoreGlobalStub(Isolate* isolate, bool is_constant, bool check_global)
      : HandlerStub(isolate) {
    set_sub_minor_key(IsConstantBits::encode(is_constant) |
                      CheckGlobalBits::encode(check_global));
  }

  static Handle<HeapObject> global_placeholder(Isolate* isolate) {
    return isolate->factory()->uninitialized_value();
  }

  Handle<Code> GetCodeCopyFromTemplate(Handle<GlobalObject> global,
                                       Handle<PropertyCell> cell) {
    if (check_global()) {
      Code::FindAndReplacePattern pattern;
      pattern.Add(Handle<Map>(global_placeholder(isolate())->map()), global);
      pattern.Add(isolate()->factory()->meta_map(), Handle<Map>(global->map()));
      pattern.Add(isolate()->factory()->global_property_cell_map(), cell);
      return CodeStub::GetCodeCopy(pattern);
    } else {
      Code::FindAndReplacePattern pattern;
      pattern.Add(isolate()->factory()->global_property_cell_map(), cell);
      return CodeStub::GetCodeCopy(pattern);
    }
  }

  virtual Code::Kind kind() const { return Code::STORE_IC; }

  virtual Handle<Code> GenerateCode() OVERRIDE;

  bool is_constant() const { return IsConstantBits::decode(sub_minor_key()); }

  bool check_global() const { return CheckGlobalBits::decode(sub_minor_key()); }

  void set_is_constant(bool value) {
    set_sub_minor_key(IsConstantBits::update(sub_minor_key(), value));
  }

  Representation representation() {
    return Representation::FromKind(
        RepresentationBits::decode(sub_minor_key()));
  }

  void set_representation(Representation r) {
    set_sub_minor_key(RepresentationBits::update(sub_minor_key(), r.kind()));
  }

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  class IsConstantBits: public BitField<bool, 0, 1> {};
  class RepresentationBits: public BitField<Representation::Kind, 1, 8> {};
  class CheckGlobalBits: public BitField<bool, 9, 1> {};

  DISALLOW_COPY_AND_ASSIGN(StoreGlobalStub);
};


class CallApiFunctionStub : public PlatformCodeStub {
 public:
  CallApiFunctionStub(Isolate* isolate,
                      bool is_store,
                      bool call_data_undefined,
                      int argc) : PlatformCodeStub(isolate) {
    minor_key_ = IsStoreBits::encode(is_store) |
                 CallDataUndefinedBits::encode(call_data_undefined) |
                 ArgumentBits::encode(argc);
    DCHECK(!is_store || argc == 1);
  }

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  virtual void Generate(MacroAssembler* masm) OVERRIDE;

  bool is_store() const { return IsStoreBits::decode(minor_key_); }
  bool call_data_undefined() const {
    return CallDataUndefinedBits::decode(minor_key_);
  }
  int argc() const { return ArgumentBits::decode(minor_key_); }

  class IsStoreBits: public BitField<bool, 0, 1> {};
  class CallDataUndefinedBits: public BitField<bool, 1, 1> {};
  class ArgumentBits: public BitField<int, 2, Code::kArgumentsBits> {};
  STATIC_ASSERT(Code::kArgumentsBits + 2 <= kStubMinorKeyBits);

  DISALLOW_COPY_AND_ASSIGN(CallApiFunctionStub);
};


class CallApiGetterStub : public PlatformCodeStub {
 public:
  explicit CallApiGetterStub(Isolate* isolate) : PlatformCodeStub(isolate) {}

 private:
  virtual void Generate(MacroAssembler* masm) OVERRIDE;
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(CallApiGetterStub);
};


class BinaryOpICStub : public HydrogenCodeStub {
 public:
  BinaryOpICStub(Isolate* isolate, Token::Value op,
                 OverwriteMode mode = NO_OVERWRITE)
      : HydrogenCodeStub(isolate, UNINITIALIZED) {
    BinaryOpIC::State state(isolate, op, mode);
    set_sub_minor_key(state.GetExtraICState());
  }

  explicit BinaryOpICStub(Isolate* isolate, const BinaryOpIC::State& state)
      : HydrogenCodeStub(isolate) {
    set_sub_minor_key(state.GetExtraICState());
  }

  static void GenerateAheadOfTime(Isolate* isolate);

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) OVERRIDE;

  static void InstallDescriptors(Isolate* isolate);

  virtual Code::Kind GetCodeKind() const OVERRIDE {
    return Code::BINARY_OP_IC;
  }

  virtual InlineCacheState GetICState() const FINAL OVERRIDE {
    return state().GetICState();
  }

  virtual ExtraICState GetExtraICState() const FINAL OVERRIDE {
    return static_cast<ExtraICState>(sub_minor_key());
  }

  virtual Handle<Code> GenerateCode() OVERRIDE;

  BinaryOpIC::State state() const {
    return BinaryOpIC::State(isolate(), GetExtraICState());
  }

  virtual void PrintState(OStream& os) const FINAL OVERRIDE;  // NOLINT

  // Parameters accessed via CodeStubGraphBuilder::GetParameter()
  static const int kLeft = 0;
  static const int kRight = 1;

 private:
  virtual inline Major MajorKey() const OVERRIDE;

  static void GenerateAheadOfTime(Isolate* isolate,
                                  const BinaryOpIC::State& state);

  DISALLOW_COPY_AND_ASSIGN(BinaryOpICStub);
};


// TODO(bmeurer): Merge this into the BinaryOpICStub once we have proper tail
// call support for stubs in Hydrogen.
class BinaryOpICWithAllocationSiteStub FINAL : public PlatformCodeStub {
 public:
  BinaryOpICWithAllocationSiteStub(Isolate* isolate,
                                   const BinaryOpIC::State& state)
      : PlatformCodeStub(isolate) {
    minor_key_ = state.GetExtraICState();
  }

  static void GenerateAheadOfTime(Isolate* isolate);

  Handle<Code> GetCodeCopyFromTemplate(Handle<AllocationSite> allocation_site) {
    Code::FindAndReplacePattern pattern;
    pattern.Add(isolate()->factory()->undefined_map(), allocation_site);
    return CodeStub::GetCodeCopy(pattern);
  }

  virtual Code::Kind GetCodeKind() const OVERRIDE {
    return Code::BINARY_OP_IC;
  }

  virtual InlineCacheState GetICState() const OVERRIDE {
    return state().GetICState();
  }

  virtual ExtraICState GetExtraICState() const OVERRIDE {
    return static_cast<ExtraICState>(minor_key_);
  }

  virtual void Generate(MacroAssembler* masm) OVERRIDE;

  virtual void PrintState(OStream& os) const OVERRIDE;  // NOLINT

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  BinaryOpIC::State state() const {
    return BinaryOpIC::State(isolate(), static_cast<ExtraICState>(minor_key_));
  }

  static void GenerateAheadOfTime(Isolate* isolate,
                                  const BinaryOpIC::State& state);

  DISALLOW_COPY_AND_ASSIGN(BinaryOpICWithAllocationSiteStub);
};


class BinaryOpWithAllocationSiteStub FINAL : public BinaryOpICStub {
 public:
  BinaryOpWithAllocationSiteStub(Isolate* isolate,
                                 Token::Value op,
                                 OverwriteMode mode)
      : BinaryOpICStub(isolate, op, mode) {}

  BinaryOpWithAllocationSiteStub(Isolate* isolate,
                                 const BinaryOpIC::State& state)
      : BinaryOpICStub(isolate, state) {}

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) OVERRIDE;

  static void InstallDescriptors(Isolate* isolate);

  virtual Code::Kind GetCodeKind() const FINAL OVERRIDE {
    return Code::STUB;
  }

  virtual Handle<Code> GenerateCode() OVERRIDE;

  virtual inline Major MajorKey() const FINAL OVERRIDE;

  // Parameters accessed via CodeStubGraphBuilder::GetParameter()
  static const int kAllocationSite = 0;
  static const int kLeft = 1;
  static const int kRight = 2;
};


enum StringAddFlags {
  // Omit both parameter checks.
  STRING_ADD_CHECK_NONE = 0,
  // Check left parameter.
  STRING_ADD_CHECK_LEFT = 1 << 0,
  // Check right parameter.
  STRING_ADD_CHECK_RIGHT = 1 << 1,
  // Check both parameters.
  STRING_ADD_CHECK_BOTH = STRING_ADD_CHECK_LEFT | STRING_ADD_CHECK_RIGHT
};


class StringAddStub FINAL : public HydrogenCodeStub {
 public:
  StringAddStub(Isolate* isolate, StringAddFlags flags,
                PretenureFlag pretenure_flag)
      : HydrogenCodeStub(isolate) {
    set_sub_minor_key(StringAddFlagsBits::encode(flags) |
                      PretenureFlagBits::encode(pretenure_flag));
  }

  StringAddFlags flags() const {
    return StringAddFlagsBits::decode(sub_minor_key());
  }

  PretenureFlag pretenure_flag() const {
    return PretenureFlagBits::decode(sub_minor_key());
  }

  virtual Handle<Code> GenerateCode() OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) OVERRIDE;

  static void InstallDescriptors(Isolate* isolate);

  // Parameters accessed via CodeStubGraphBuilder::GetParameter()
  static const int kLeft = 0;
  static const int kRight = 1;

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  class StringAddFlagsBits: public BitField<StringAddFlags, 0, 2> {};
  class PretenureFlagBits: public BitField<PretenureFlag, 2, 1> {};

  virtual void PrintBaseName(OStream& os) const OVERRIDE;  // NOLINT

  DISALLOW_COPY_AND_ASSIGN(StringAddStub);
};


class CompareICStub : public PlatformCodeStub {
 public:
  CompareICStub(Isolate* isolate, Token::Value op, CompareIC::State left,
                CompareIC::State right, CompareIC::State state)
      : PlatformCodeStub(isolate) {
    DCHECK(Token::IsCompareOp(op));
    minor_key_ = OpBits::encode(op - Token::EQ) | LeftStateBits::encode(left) |
                 RightStateBits::encode(right) | StateBits::encode(state);
  }

  virtual void Generate(MacroAssembler* masm);

  void set_known_map(Handle<Map> map) { known_map_ = map; }

  explicit CompareICStub(uint32_t stub_key) : PlatformCodeStub(stub_key) {
    DCHECK_EQ(MajorKeyFromKey(stub_key), MajorKey());
  }

  virtual InlineCacheState GetICState() const;

  Token::Value op() const {
    return static_cast<Token::Value>(Token::EQ + OpBits::decode(minor_key_));
  }

  CompareIC::State left() const { return LeftStateBits::decode(minor_key_); }
  CompareIC::State right() const { return RightStateBits::decode(minor_key_); }
  CompareIC::State state() const { return StateBits::decode(minor_key_); }

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  virtual Code::Kind GetCodeKind() const { return Code::COMPARE_IC; }

  void GenerateSmis(MacroAssembler* masm);
  void GenerateNumbers(MacroAssembler* masm);
  void GenerateInternalizedStrings(MacroAssembler* masm);
  void GenerateStrings(MacroAssembler* masm);
  void GenerateUniqueNames(MacroAssembler* masm);
  void GenerateObjects(MacroAssembler* masm);
  void GenerateMiss(MacroAssembler* masm);
  void GenerateKnownObjects(MacroAssembler* masm);
  void GenerateGeneric(MacroAssembler* masm);

  bool strict() const { return op() == Token::EQ_STRICT; }
  Condition GetCondition() const { return CompareIC::ComputeCondition(op()); }

  virtual void AddToSpecialCache(Handle<Code> new_object);
  virtual bool FindCodeInSpecialCache(Code** code_out);
  virtual bool UseSpecialCache() { return state() == CompareIC::KNOWN_OBJECT; }

  class OpBits : public BitField<int, 0, 3> {};
  class LeftStateBits : public BitField<CompareIC::State, 3, 4> {};
  class RightStateBits : public BitField<CompareIC::State, 7, 4> {};
  class StateBits : public BitField<CompareIC::State, 11, 4> {};

  Handle<Map> known_map_;

  DISALLOW_COPY_AND_ASSIGN(CompareICStub);
};


class CompareNilICStub : public HydrogenCodeStub  {
 public:
  Type* GetType(Zone* zone, Handle<Map> map = Handle<Map>());
  Type* GetInputType(Zone* zone, Handle<Map> map);

  CompareNilICStub(Isolate* isolate, NilValue nil) : HydrogenCodeStub(isolate) {
    set_sub_minor_key(NilValueBits::encode(nil));
  }

  CompareNilICStub(Isolate* isolate, ExtraICState ic_state,
                   InitializationState init_state = INITIALIZED)
      : HydrogenCodeStub(isolate, init_state) {
    set_sub_minor_key(ic_state);
  }

  static Handle<Code> GetUninitialized(Isolate* isolate,
                                       NilValue nil) {
    return CompareNilICStub(isolate, nil, UNINITIALIZED).GetCode();
  }

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) OVERRIDE;

  static void InstallDescriptors(Isolate* isolate) {
    CompareNilICStub compare_stub(isolate, kNullValue, UNINITIALIZED);
    compare_stub.InitializeInterfaceDescriptor(
        isolate->code_stub_interface_descriptor(CodeStub::CompareNilIC));
  }

  virtual InlineCacheState GetICState() const {
    State state = this->state();
    if (state.Contains(GENERIC)) {
      return MEGAMORPHIC;
    } else if (state.Contains(MONOMORPHIC_MAP)) {
      return MONOMORPHIC;
    } else {
      return PREMONOMORPHIC;
    }
  }

  virtual Code::Kind GetCodeKind() const { return Code::COMPARE_NIL_IC; }

  virtual Handle<Code> GenerateCode() OVERRIDE;

  virtual ExtraICState GetExtraICState() const { return sub_minor_key(); }

  void UpdateStatus(Handle<Object> object);

  bool IsMonomorphic() const { return state().Contains(MONOMORPHIC_MAP); }

  NilValue nil_value() const { return NilValueBits::decode(sub_minor_key()); }

  void ClearState() {
    set_sub_minor_key(TypesBits::update(sub_minor_key(), 0));
  }

  virtual void PrintState(OStream& os) const OVERRIDE;     // NOLINT
  virtual void PrintBaseName(OStream& os) const OVERRIDE;  // NOLINT

 private:
  CompareNilICStub(Isolate* isolate, NilValue nil,
                   InitializationState init_state)
      : HydrogenCodeStub(isolate, init_state) {
    set_sub_minor_key(NilValueBits::encode(nil));
  }

  virtual inline Major MajorKey() const FINAL OVERRIDE;

  enum CompareNilType {
    UNDEFINED,
    NULL_TYPE,
    MONOMORPHIC_MAP,
    GENERIC,
    NUMBER_OF_TYPES
  };

  // At most 6 different types can be distinguished, because the Code object
  // only has room for a single byte to hold a set and there are two more
  // boolean flags we need to store. :-P
  STATIC_ASSERT(NUMBER_OF_TYPES <= 6);

  class State : public EnumSet<CompareNilType, byte> {
   public:
    State() : EnumSet<CompareNilType, byte>(0) { }
    explicit State(byte bits) : EnumSet<CompareNilType, byte>(bits) { }
  };
  friend OStream& operator<<(OStream& os, const State& s);

  State state() const { return State(TypesBits::decode(sub_minor_key())); }

  class NilValueBits : public BitField<NilValue, 0, 1> {};
  class TypesBits : public BitField<byte, 1, NUMBER_OF_TYPES> {};

  friend class CompareNilIC;

  DISALLOW_COPY_AND_ASSIGN(CompareNilICStub);
};


OStream& operator<<(OStream& os, const CompareNilICStub::State& s);


class CEntryStub : public PlatformCodeStub {
 public:
  CEntryStub(Isolate* isolate, int result_size,
             SaveFPRegsMode save_doubles = kDontSaveFPRegs)
      : PlatformCodeStub(isolate) {
    minor_key_ = SaveDoublesBits::encode(save_doubles == kSaveFPRegs);
    DCHECK(result_size == 1 || result_size == 2);
#ifdef _WIN64
    minor_key_ = ResultSizeBits::update(minor_key_, result_size);
#endif  // _WIN64
  }

  void Generate(MacroAssembler* masm);

  // The version of this stub that doesn't save doubles is generated ahead of
  // time, so it's OK to call it from other stubs that can't cope with GC during
  // their code generation.  On machines that always have gp registers (x64) we
  // can generate both variants ahead of time.
  static void GenerateAheadOfTime(Isolate* isolate);

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  bool save_doubles() const { return SaveDoublesBits::decode(minor_key_); }
#ifdef _WIN64
  int result_size() const { return ResultSizeBits::decode(minor_key_); }
#endif  // _WIN64

  bool NeedsImmovableCode();

  class SaveDoublesBits : public BitField<bool, 0, 1> {};
  class ResultSizeBits : public BitField<int, 1, 3> {};

  DISALLOW_COPY_AND_ASSIGN(CEntryStub);
};


class JSEntryStub : public PlatformCodeStub {
 public:
  explicit JSEntryStub(Isolate* isolate) : PlatformCodeStub(isolate) { }

  void Generate(MacroAssembler* masm) { GenerateBody(masm, false); }

 protected:
  void GenerateBody(MacroAssembler* masm, bool is_construct);

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  virtual void FinishCode(Handle<Code> code);

  int handler_offset_;

  DISALLOW_COPY_AND_ASSIGN(JSEntryStub);
};


class JSConstructEntryStub : public JSEntryStub {
 public:
  explicit JSConstructEntryStub(Isolate* isolate) : JSEntryStub(isolate) {
    minor_key_ = 1;
  }

  void Generate(MacroAssembler* masm) { GenerateBody(masm, true); }

 private:
  virtual void PrintName(OStream& os) const OVERRIDE {  // NOLINT
    os << "JSConstructEntryStub";
  }

  DISALLOW_COPY_AND_ASSIGN(JSConstructEntryStub);
};


class ArgumentsAccessStub: public PlatformCodeStub {
 public:
  enum Type {
    READ_ELEMENT,
    NEW_SLOPPY_FAST,
    NEW_SLOPPY_SLOW,
    NEW_STRICT
  };

  ArgumentsAccessStub(Isolate* isolate, Type type) : PlatformCodeStub(isolate) {
    minor_key_ = TypeBits::encode(type);
  }

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  Type type() const { return TypeBits::decode(minor_key_); }

  void Generate(MacroAssembler* masm);
  void GenerateReadElement(MacroAssembler* masm);
  void GenerateNewStrict(MacroAssembler* masm);
  void GenerateNewSloppyFast(MacroAssembler* masm);
  void GenerateNewSloppySlow(MacroAssembler* masm);

  virtual void PrintName(OStream& os) const OVERRIDE;  // NOLINT

  class TypeBits : public BitField<Type, 0, 2> {};

  DISALLOW_COPY_AND_ASSIGN(ArgumentsAccessStub);
};


class RegExpExecStub: public PlatformCodeStub {
 public:
  explicit RegExpExecStub(Isolate* isolate) : PlatformCodeStub(isolate) { }

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  void Generate(MacroAssembler* masm);

  DISALLOW_COPY_AND_ASSIGN(RegExpExecStub);
};


class RegExpConstructResultStub FINAL : public HydrogenCodeStub {
 public:
  explicit RegExpConstructResultStub(Isolate* isolate)
      : HydrogenCodeStub(isolate) { }

  virtual Handle<Code> GenerateCode() OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) OVERRIDE;

  virtual inline Major MajorKey() const FINAL OVERRIDE;

  static void InstallDescriptors(Isolate* isolate);

  // Parameters accessed via CodeStubGraphBuilder::GetParameter()
  static const int kLength = 0;
  static const int kIndex = 1;
  static const int kInput = 2;

 private:
  DISALLOW_COPY_AND_ASSIGN(RegExpConstructResultStub);
};


class CallFunctionStub: public PlatformCodeStub {
 public:
  CallFunctionStub(Isolate* isolate, int argc, CallFunctionFlags flags)
      : PlatformCodeStub(isolate) {
    DCHECK(argc >= 0 && argc <= Code::kMaxArguments);
    minor_key_ = ArgcBits::encode(argc) | FlagBits::encode(flags);
  }

  void Generate(MacroAssembler* masm);

  static int ExtractArgcFromMinorKey(int minor_key) {
    return ArgcBits::decode(minor_key);
  }

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor);

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  int argc() const { return ArgcBits::decode(minor_key_); }
  int flags() const { return FlagBits::decode(minor_key_); }

  bool CallAsMethod() const {
    return flags() == CALL_AS_METHOD || flags() == WRAP_AND_CALL;
  }

  bool NeedsChecks() const { return flags() != WRAP_AND_CALL; }

  virtual void PrintName(OStream& os) const OVERRIDE;  // NOLINT

  // Minor key encoding in 32 bits with Bitfield <Type, shift, size>.
  class FlagBits : public BitField<CallFunctionFlags, 0, 2> {};
  class ArgcBits : public BitField<unsigned, 2, Code::kArgumentsBits> {};
  STATIC_ASSERT(Code::kArgumentsBits + 2 <= kStubMinorKeyBits);

  DISALLOW_COPY_AND_ASSIGN(CallFunctionStub);
};


class CallConstructStub: public PlatformCodeStub {
 public:
  CallConstructStub(Isolate* isolate, CallConstructorFlags flags)
      : PlatformCodeStub(isolate) {
    minor_key_ = FlagBits::encode(flags);
  }

  void Generate(MacroAssembler* masm);

  virtual void FinishCode(Handle<Code> code) {
    code->set_has_function_cache(RecordCallTarget());
  }

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor);

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  CallConstructorFlags flags() const { return FlagBits::decode(minor_key_); }

  bool RecordCallTarget() const {
    return (flags() & RECORD_CONSTRUCTOR_TARGET) != 0;
  }

  virtual void PrintName(OStream& os) const OVERRIDE;  // NOLINT

  class FlagBits : public BitField<CallConstructorFlags, 0, 1> {};

  DISALLOW_COPY_AND_ASSIGN(CallConstructStub);
};


enum StringIndexFlags {
  // Accepts smis or heap numbers.
  STRING_INDEX_IS_NUMBER,

  // Accepts smis or heap numbers that are valid array indices
  // (ECMA-262 15.4). Invalid indices are reported as being out of
  // range.
  STRING_INDEX_IS_ARRAY_INDEX
};


// Generates code implementing String.prototype.charCodeAt.
//
// Only supports the case when the receiver is a string and the index
// is a number (smi or heap number) that is a valid index into the
// string. Additional index constraints are specified by the
// flags. Otherwise, bails out to the provided labels.
//
// Register usage: |object| may be changed to another string in a way
// that doesn't affect charCodeAt/charAt semantics, |index| is
// preserved, |scratch| and |result| are clobbered.
class StringCharCodeAtGenerator {
 public:
  StringCharCodeAtGenerator(Register object,
                            Register index,
                            Register result,
                            Label* receiver_not_string,
                            Label* index_not_number,
                            Label* index_out_of_range,
                            StringIndexFlags index_flags)
      : object_(object),
        index_(index),
        result_(result),
        receiver_not_string_(receiver_not_string),
        index_not_number_(index_not_number),
        index_out_of_range_(index_out_of_range),
        index_flags_(index_flags) {
    DCHECK(!result_.is(object_));
    DCHECK(!result_.is(index_));
  }

  // Generates the fast case code. On the fallthrough path |result|
  // register contains the result.
  void GenerateFast(MacroAssembler* masm);

  // Generates the slow case code. Must not be naturally
  // reachable. Expected to be put after a ret instruction (e.g., in
  // deferred code). Always jumps back to the fast case.
  void GenerateSlow(MacroAssembler* masm,
                    const RuntimeCallHelper& call_helper);

  // Skip handling slow case and directly jump to bailout.
  void SkipSlow(MacroAssembler* masm, Label* bailout) {
    masm->bind(&index_not_smi_);
    masm->bind(&call_runtime_);
    masm->jmp(bailout);
  }

 private:
  Register object_;
  Register index_;
  Register result_;

  Label* receiver_not_string_;
  Label* index_not_number_;
  Label* index_out_of_range_;

  StringIndexFlags index_flags_;

  Label call_runtime_;
  Label index_not_smi_;
  Label got_smi_index_;
  Label exit_;

  DISALLOW_COPY_AND_ASSIGN(StringCharCodeAtGenerator);
};


// Generates code for creating a one-char string from a char code.
class StringCharFromCodeGenerator {
 public:
  StringCharFromCodeGenerator(Register code,
                              Register result)
      : code_(code),
        result_(result) {
    DCHECK(!code_.is(result_));
  }

  // Generates the fast case code. On the fallthrough path |result|
  // register contains the result.
  void GenerateFast(MacroAssembler* masm);

  // Generates the slow case code. Must not be naturally
  // reachable. Expected to be put after a ret instruction (e.g., in
  // deferred code). Always jumps back to the fast case.
  void GenerateSlow(MacroAssembler* masm,
                    const RuntimeCallHelper& call_helper);

  // Skip handling slow case and directly jump to bailout.
  void SkipSlow(MacroAssembler* masm, Label* bailout) {
    masm->bind(&slow_case_);
    masm->jmp(bailout);
  }

 private:
  Register code_;
  Register result_;

  Label slow_case_;
  Label exit_;

  DISALLOW_COPY_AND_ASSIGN(StringCharFromCodeGenerator);
};


// Generates code implementing String.prototype.charAt.
//
// Only supports the case when the receiver is a string and the index
// is a number (smi or heap number) that is a valid index into the
// string. Additional index constraints are specified by the
// flags. Otherwise, bails out to the provided labels.
//
// Register usage: |object| may be changed to another string in a way
// that doesn't affect charCodeAt/charAt semantics, |index| is
// preserved, |scratch1|, |scratch2|, and |result| are clobbered.
class StringCharAtGenerator {
 public:
  StringCharAtGenerator(Register object,
                        Register index,
                        Register scratch,
                        Register result,
                        Label* receiver_not_string,
                        Label* index_not_number,
                        Label* index_out_of_range,
                        StringIndexFlags index_flags)
      : char_code_at_generator_(object,
                                index,
                                scratch,
                                receiver_not_string,
                                index_not_number,
                                index_out_of_range,
                                index_flags),
        char_from_code_generator_(scratch, result) {}

  // Generates the fast case code. On the fallthrough path |result|
  // register contains the result.
  void GenerateFast(MacroAssembler* masm) {
    char_code_at_generator_.GenerateFast(masm);
    char_from_code_generator_.GenerateFast(masm);
  }

  // Generates the slow case code. Must not be naturally
  // reachable. Expected to be put after a ret instruction (e.g., in
  // deferred code). Always jumps back to the fast case.
  void GenerateSlow(MacroAssembler* masm,
                    const RuntimeCallHelper& call_helper) {
    char_code_at_generator_.GenerateSlow(masm, call_helper);
    char_from_code_generator_.GenerateSlow(masm, call_helper);
  }

  // Skip handling slow case and directly jump to bailout.
  void SkipSlow(MacroAssembler* masm, Label* bailout) {
    char_code_at_generator_.SkipSlow(masm, bailout);
    char_from_code_generator_.SkipSlow(masm, bailout);
  }

 private:
  StringCharCodeAtGenerator char_code_at_generator_;
  StringCharFromCodeGenerator char_from_code_generator_;

  DISALLOW_COPY_AND_ASSIGN(StringCharAtGenerator);
};


class LoadDictionaryElementStub : public HydrogenCodeStub {
 public:
  explicit LoadDictionaryElementStub(Isolate* isolate)
      : HydrogenCodeStub(isolate) {}

  virtual Handle<Code> GenerateCode() OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) OVERRIDE;

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(LoadDictionaryElementStub);
};


class KeyedLoadGenericStub : public HydrogenCodeStub {
 public:
  explicit KeyedLoadGenericStub(Isolate* isolate) : HydrogenCodeStub(isolate) {}

  virtual Handle<Code> GenerateCode() OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) OVERRIDE;

  static void InstallDescriptors(Isolate* isolate);

  virtual Code::Kind GetCodeKind() const { return Code::KEYED_LOAD_IC; }
  virtual InlineCacheState GetICState() const { return GENERIC; }

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(KeyedLoadGenericStub);
};


class LoadICTrampolineStub : public PlatformCodeStub {
 public:
  LoadICTrampolineStub(Isolate* isolate, const LoadIC::State& state)
      : PlatformCodeStub(isolate) {
    minor_key_ = state.GetExtraICState();
  }

  virtual Code::Kind GetCodeKind() const OVERRIDE { return Code::LOAD_IC; }

  virtual InlineCacheState GetICState() const FINAL OVERRIDE {
    return GENERIC;
  }

  virtual ExtraICState GetExtraICState() const FINAL OVERRIDE {
    return static_cast<ExtraICState>(minor_key_);
  }

  virtual inline Major MajorKey() const OVERRIDE;

 private:
  LoadIC::State state() const {
    return LoadIC::State(static_cast<ExtraICState>(minor_key_));
  }

  virtual void Generate(MacroAssembler* masm);

  DISALLOW_COPY_AND_ASSIGN(LoadICTrampolineStub);
};


class KeyedLoadICTrampolineStub : public LoadICTrampolineStub {
 public:
  explicit KeyedLoadICTrampolineStub(Isolate* isolate)
      : LoadICTrampolineStub(isolate, LoadIC::State(0)) {}

  virtual Code::Kind GetCodeKind() const OVERRIDE {
    return Code::KEYED_LOAD_IC;
  }

  virtual inline Major MajorKey() const FINAL OVERRIDE;

 private:
  virtual void Generate(MacroAssembler* masm);

  DISALLOW_COPY_AND_ASSIGN(KeyedLoadICTrampolineStub);
};


class VectorLoadStub : public HydrogenCodeStub {
 public:
  explicit VectorLoadStub(Isolate* isolate, const LoadIC::State& state)
      : HydrogenCodeStub(isolate) {
    set_sub_minor_key(state.GetExtraICState());
  }

  virtual Handle<Code> GenerateCode() OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) OVERRIDE;

  static void InstallDescriptors(Isolate* isolate);

  virtual Code::Kind GetCodeKind() const OVERRIDE { return Code::LOAD_IC; }

  virtual InlineCacheState GetICState() const FINAL OVERRIDE {
    return GENERIC;
  }

  virtual ExtraICState GetExtraICState() const FINAL OVERRIDE {
    return static_cast<ExtraICState>(sub_minor_key());
  }

 private:
  virtual inline Major MajorKey() const OVERRIDE;

  LoadIC::State state() const { return LoadIC::State(GetExtraICState()); }

  DISALLOW_COPY_AND_ASSIGN(VectorLoadStub);
};


class VectorKeyedLoadStub : public VectorLoadStub {
 public:
  explicit VectorKeyedLoadStub(Isolate* isolate)
      : VectorLoadStub(isolate, LoadIC::State(0)) {}

  virtual Handle<Code> GenerateCode() OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) OVERRIDE;

  static void InstallDescriptors(Isolate* isolate);

  virtual Code::Kind GetCodeKind() const OVERRIDE {
    return Code::KEYED_LOAD_IC;
  }

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(VectorKeyedLoadStub);
};


class DoubleToIStub : public PlatformCodeStub {
 public:
  DoubleToIStub(Isolate* isolate, Register source, Register destination,
                int offset, bool is_truncating, bool skip_fastpath = false)
      : PlatformCodeStub(isolate) {
    minor_key_ = SourceRegisterBits::encode(source.code()) |
                 DestinationRegisterBits::encode(destination.code()) |
                 OffsetBits::encode(offset) |
                 IsTruncatingBits::encode(is_truncating) |
                 SkipFastPathBits::encode(skip_fastpath) |
                 SSE3Bits::encode(CpuFeatures::IsSupported(SSE3) ? 1 : 0);
  }

  void Generate(MacroAssembler* masm);

  virtual bool SometimesSetsUpAFrame() { return false; }

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  Register source() const {
    return Register::from_code(SourceRegisterBits::decode(minor_key_));
  }
  Register destination() const {
    return Register::from_code(DestinationRegisterBits::decode(minor_key_));
  }
  bool is_truncating() const { return IsTruncatingBits::decode(minor_key_); }
  bool skip_fastpath() const { return SkipFastPathBits::decode(minor_key_); }
  int offset() const { return OffsetBits::decode(minor_key_); }

  static const int kBitsPerRegisterNumber = 6;
  STATIC_ASSERT((1L << kBitsPerRegisterNumber) >= Register::kNumRegisters);
  class SourceRegisterBits:
      public BitField<int, 0, kBitsPerRegisterNumber> {};  // NOLINT
  class DestinationRegisterBits:
      public BitField<int, kBitsPerRegisterNumber,
        kBitsPerRegisterNumber> {};  // NOLINT
  class IsTruncatingBits:
      public BitField<bool, 2 * kBitsPerRegisterNumber, 1> {};  // NOLINT
  class OffsetBits:
      public BitField<int, 2 * kBitsPerRegisterNumber + 1, 3> {};  // NOLINT
  class SkipFastPathBits:
      public BitField<int, 2 * kBitsPerRegisterNumber + 4, 1> {};  // NOLINT
  class SSE3Bits:
      public BitField<int, 2 * kBitsPerRegisterNumber + 5, 1> {};  // NOLINT

  DISALLOW_COPY_AND_ASSIGN(DoubleToIStub);
};


class LoadFastElementStub : public HydrogenCodeStub {
 public:
  LoadFastElementStub(Isolate* isolate, bool is_js_array,
                      ElementsKind elements_kind)
      : HydrogenCodeStub(isolate) {
    set_sub_minor_key(ElementsKindBits::encode(elements_kind) |
                      IsJSArrayBits::encode(is_js_array));
  }

  bool is_js_array() const { return IsJSArrayBits::decode(sub_minor_key()); }

  ElementsKind elements_kind() const {
    return ElementsKindBits::decode(sub_minor_key());
  }

  virtual Handle<Code> GenerateCode() OVERRIDE;

  static void InstallDescriptors(Isolate* isolate);

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) OVERRIDE;

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  class ElementsKindBits: public BitField<ElementsKind, 0, 8> {};
  class IsJSArrayBits: public BitField<bool, 8, 1> {};

  DISALLOW_COPY_AND_ASSIGN(LoadFastElementStub);
};


class StoreFastElementStub : public HydrogenCodeStub {
 public:
  StoreFastElementStub(Isolate* isolate, bool is_js_array,
                       ElementsKind elements_kind, KeyedAccessStoreMode mode)
      : HydrogenCodeStub(isolate) {
    set_sub_minor_key(ElementsKindBits::encode(elements_kind) |
                      IsJSArrayBits::encode(is_js_array) |
                      StoreModeBits::encode(mode));
  }

  bool is_js_array() const { return IsJSArrayBits::decode(sub_minor_key()); }

  ElementsKind elements_kind() const {
    return ElementsKindBits::decode(sub_minor_key());
  }

  KeyedAccessStoreMode store_mode() const {
    return StoreModeBits::decode(sub_minor_key());
  }

  virtual Handle<Code> GenerateCode() OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) OVERRIDE;

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  class ElementsKindBits: public BitField<ElementsKind,      0, 8> {};
  class StoreModeBits: public BitField<KeyedAccessStoreMode, 8, 4> {};
  class IsJSArrayBits: public BitField<bool,                12, 1> {};

  DISALLOW_COPY_AND_ASSIGN(StoreFastElementStub);
};


class TransitionElementsKindStub : public HydrogenCodeStub {
 public:
  TransitionElementsKindStub(Isolate* isolate,
                             ElementsKind from_kind,
                             ElementsKind to_kind,
                             bool is_js_array) : HydrogenCodeStub(isolate) {
    set_sub_minor_key(FromKindBits::encode(from_kind) |
                      ToKindBits::encode(to_kind) |
                      IsJSArrayBits::encode(is_js_array));
  }

  ElementsKind from_kind() const {
    return FromKindBits::decode(sub_minor_key());
  }

  ElementsKind to_kind() const { return ToKindBits::decode(sub_minor_key()); }

  bool is_js_array() const { return IsJSArrayBits::decode(sub_minor_key()); }

  virtual Handle<Code> GenerateCode() OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) OVERRIDE;

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  class FromKindBits: public BitField<ElementsKind, 8, 8> {};
  class ToKindBits: public BitField<ElementsKind, 0, 8> {};
  class IsJSArrayBits: public BitField<bool, 16, 1> {};

  DISALLOW_COPY_AND_ASSIGN(TransitionElementsKindStub);
};


class ArrayConstructorStubBase : public HydrogenCodeStub {
 public:
  ArrayConstructorStubBase(Isolate* isolate,
                           ElementsKind kind,
                           AllocationSiteOverrideMode override_mode)
      : HydrogenCodeStub(isolate) {
    // It only makes sense to override local allocation site behavior
    // if there is a difference between the global allocation site policy
    // for an ElementsKind and the desired usage of the stub.
    DCHECK(override_mode != DISABLE_ALLOCATION_SITES ||
           AllocationSite::GetMode(kind) == TRACK_ALLOCATION_SITE);
    set_sub_minor_key(ElementsKindBits::encode(kind) |
                      AllocationSiteOverrideModeBits::encode(override_mode));
  }

  ElementsKind elements_kind() const {
    return ElementsKindBits::decode(sub_minor_key());
  }

  AllocationSiteOverrideMode override_mode() const {
    return AllocationSiteOverrideModeBits::decode(sub_minor_key());
  }

  static void GenerateStubsAheadOfTime(Isolate* isolate);
  static void InstallDescriptors(Isolate* isolate);

  // Parameters accessed via CodeStubGraphBuilder::GetParameter()
  static const int kConstructor = 0;
  static const int kAllocationSite = 1;

 protected:
  OStream& BasePrintName(OStream& os, const char* name) const;  // NOLINT

 private:
  // Ensure data fits within available bits.
  STATIC_ASSERT(LAST_ALLOCATION_SITE_OVERRIDE_MODE == 1);

  class ElementsKindBits: public BitField<ElementsKind, 0, 8> {};
  class AllocationSiteOverrideModeBits: public
      BitField<AllocationSiteOverrideMode, 8, 1> {};  // NOLINT

  DISALLOW_COPY_AND_ASSIGN(ArrayConstructorStubBase);
};


class ArrayNoArgumentConstructorStub : public ArrayConstructorStubBase {
 public:
  ArrayNoArgumentConstructorStub(
      Isolate* isolate,
      ElementsKind kind,
      AllocationSiteOverrideMode override_mode = DONT_OVERRIDE)
      : ArrayConstructorStubBase(isolate, kind, override_mode) {
  }

  virtual Handle<Code> GenerateCode() OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) OVERRIDE;

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  virtual void PrintName(OStream& os) const OVERRIDE {  // NOLINT
    BasePrintName(os, "ArrayNoArgumentConstructorStub");
  }

  DISALLOW_COPY_AND_ASSIGN(ArrayNoArgumentConstructorStub);
};


class ArraySingleArgumentConstructorStub : public ArrayConstructorStubBase {
 public:
  ArraySingleArgumentConstructorStub(
      Isolate* isolate,
      ElementsKind kind,
      AllocationSiteOverrideMode override_mode = DONT_OVERRIDE)
      : ArrayConstructorStubBase(isolate, kind, override_mode) {
  }

  virtual Handle<Code> GenerateCode() OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) OVERRIDE;

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  virtual void PrintName(OStream& os) const {  // NOLINT
    BasePrintName(os, "ArraySingleArgumentConstructorStub");
  }

  DISALLOW_COPY_AND_ASSIGN(ArraySingleArgumentConstructorStub);
};


class ArrayNArgumentsConstructorStub : public ArrayConstructorStubBase {
 public:
  ArrayNArgumentsConstructorStub(
      Isolate* isolate,
      ElementsKind kind,
      AllocationSiteOverrideMode override_mode = DONT_OVERRIDE)
      : ArrayConstructorStubBase(isolate, kind, override_mode) {
  }

  virtual Handle<Code> GenerateCode() OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) OVERRIDE;

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  virtual void PrintName(OStream& os) const {  // NOLINT
    BasePrintName(os, "ArrayNArgumentsConstructorStub");
  }

  DISALLOW_COPY_AND_ASSIGN(ArrayNArgumentsConstructorStub);
};


class InternalArrayConstructorStubBase : public HydrogenCodeStub {
 public:
  InternalArrayConstructorStubBase(Isolate* isolate, ElementsKind kind)
      : HydrogenCodeStub(isolate) {
    set_sub_minor_key(ElementsKindBits::encode(kind));
  }

  static void GenerateStubsAheadOfTime(Isolate* isolate);
  static void InstallDescriptors(Isolate* isolate);

  // Parameters accessed via CodeStubGraphBuilder::GetParameter()
  static const int kConstructor = 0;

  ElementsKind elements_kind() const {
    return ElementsKindBits::decode(sub_minor_key());
  }

 private:
  class ElementsKindBits : public BitField<ElementsKind, 0, 8> {};

  DISALLOW_COPY_AND_ASSIGN(InternalArrayConstructorStubBase);
};


class InternalArrayNoArgumentConstructorStub : public
    InternalArrayConstructorStubBase {
 public:
  InternalArrayNoArgumentConstructorStub(Isolate* isolate,
                                         ElementsKind kind)
      : InternalArrayConstructorStubBase(isolate, kind) { }

  virtual Handle<Code> GenerateCode() OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) OVERRIDE;

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(InternalArrayNoArgumentConstructorStub);
};


class InternalArraySingleArgumentConstructorStub : public
    InternalArrayConstructorStubBase {
 public:
  InternalArraySingleArgumentConstructorStub(Isolate* isolate,
                                             ElementsKind kind)
      : InternalArrayConstructorStubBase(isolate, kind) { }

  virtual Handle<Code> GenerateCode() OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) OVERRIDE;

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(InternalArraySingleArgumentConstructorStub);
};


class InternalArrayNArgumentsConstructorStub : public
    InternalArrayConstructorStubBase {
 public:
  InternalArrayNArgumentsConstructorStub(Isolate* isolate, ElementsKind kind)
      : InternalArrayConstructorStubBase(isolate, kind) { }

  virtual Handle<Code> GenerateCode() OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) OVERRIDE;

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(InternalArrayNArgumentsConstructorStub);
};


class StoreElementStub : public PlatformCodeStub {
 public:
  StoreElementStub(Isolate* isolate, ElementsKind elements_kind)
      : PlatformCodeStub(isolate) {
    minor_key_ = ElementsKindBits::encode(elements_kind);
  }

  void Generate(MacroAssembler* masm);

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  ElementsKind elements_kind() const {
    return ElementsKindBits::decode(minor_key_);
  }

  class ElementsKindBits : public BitField<ElementsKind, 0, 8> {};

  DISALLOW_COPY_AND_ASSIGN(StoreElementStub);
};


class ToBooleanStub: public HydrogenCodeStub {
 public:
  enum Type {
    UNDEFINED,
    BOOLEAN,
    NULL_TYPE,
    SMI,
    SPEC_OBJECT,
    STRING,
    SYMBOL,
    HEAP_NUMBER,
    NUMBER_OF_TYPES
  };

  enum ResultMode {
    RESULT_AS_SMI,             // For Smi(1) on truthy value, Smi(0) otherwise.
    RESULT_AS_ODDBALL,         // For {true} on truthy value, {false} otherwise.
    RESULT_AS_INVERSE_ODDBALL  // For {false} on truthy value, {true} otherwise.
  };

  // At most 8 different types can be distinguished, because the Code object
  // only has room for a single byte to hold a set of these types. :-P
  STATIC_ASSERT(NUMBER_OF_TYPES <= 8);

  class Types : public EnumSet<Type, byte> {
   public:
    Types() : EnumSet<Type, byte>(0) {}
    explicit Types(byte bits) : EnumSet<Type, byte>(bits) {}

    byte ToByte() const { return ToIntegral(); }
    bool UpdateStatus(Handle<Object> object);
    bool NeedsMap() const;
    bool CanBeUndetectable() const;
    bool IsGeneric() const { return ToIntegral() == Generic().ToIntegral(); }

    static Types Generic() { return Types((1 << NUMBER_OF_TYPES) - 1); }
  };

  ToBooleanStub(Isolate* isolate, ResultMode mode, Types types = Types())
      : HydrogenCodeStub(isolate) {
    set_sub_minor_key(TypesBits::encode(types.ToByte()) |
                      ResultModeBits::encode(mode));
  }

  ToBooleanStub(Isolate* isolate, ExtraICState state)
      : HydrogenCodeStub(isolate) {
    set_sub_minor_key(TypesBits::encode(static_cast<byte>(state)) |
                      ResultModeBits::encode(RESULT_AS_SMI));
  }

  bool UpdateStatus(Handle<Object> object);
  Types types() const { return Types(TypesBits::decode(sub_minor_key())); }
  ResultMode mode() const { return ResultModeBits::decode(sub_minor_key()); }

  virtual Handle<Code> GenerateCode() OVERRIDE;
  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) OVERRIDE;

  virtual Code::Kind GetCodeKind() const { return Code::TO_BOOLEAN_IC; }
  virtual void PrintState(OStream& os) const OVERRIDE;  // NOLINT

  virtual bool SometimesSetsUpAFrame() { return false; }

  static void InstallDescriptors(Isolate* isolate) {
    ToBooleanStub stub(isolate, RESULT_AS_SMI);
    stub.InitializeInterfaceDescriptor(
        isolate->code_stub_interface_descriptor(CodeStub::ToBoolean));
  }

  static Handle<Code> GetUninitialized(Isolate* isolate) {
    return ToBooleanStub(isolate, UNINITIALIZED).GetCode();
  }

  virtual ExtraICState GetExtraICState() const { return types().ToIntegral(); }

  virtual InlineCacheState GetICState() const {
    if (types().IsEmpty()) {
      return ::v8::internal::UNINITIALIZED;
    } else {
      return MONOMORPHIC;
    }
  }

 private:
  ToBooleanStub(Isolate* isolate, InitializationState init_state)
      : HydrogenCodeStub(isolate, init_state) {
    set_sub_minor_key(ResultModeBits::encode(RESULT_AS_SMI));
  }

  virtual inline Major MajorKey() const FINAL OVERRIDE;

  class TypesBits : public BitField<byte, 0, NUMBER_OF_TYPES> {};
  class ResultModeBits : public BitField<ResultMode, NUMBER_OF_TYPES, 2> {};

  DISALLOW_COPY_AND_ASSIGN(ToBooleanStub);
};


OStream& operator<<(OStream& os, const ToBooleanStub::Types& t);


class ElementsTransitionAndStoreStub : public HydrogenCodeStub {
 public:
  ElementsTransitionAndStoreStub(Isolate* isolate, ElementsKind from_kind,
                                 ElementsKind to_kind, bool is_jsarray,
                                 KeyedAccessStoreMode store_mode)
      : HydrogenCodeStub(isolate) {
    set_sub_minor_key(FromBits::encode(from_kind) | ToBits::encode(to_kind) |
                      IsJSArrayBits::encode(is_jsarray) |
                      StoreModeBits::encode(store_mode));
  }

  ElementsKind from_kind() const { return FromBits::decode(sub_minor_key()); }
  ElementsKind to_kind() const { return ToBits::decode(sub_minor_key()); }
  bool is_jsarray() const { return IsJSArrayBits::decode(sub_minor_key()); }
  KeyedAccessStoreMode store_mode() const {
    return StoreModeBits::decode(sub_minor_key());
  }

  virtual Handle<Code> GenerateCode() OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) OVERRIDE;

  // Parameters accessed via CodeStubGraphBuilder::GetParameter()
  enum ParameterIndices {
    kValueIndex,
    kMapIndex,
    kKeyIndex,
    kObjectIndex,
    kParameterCount
  };

  static const Register ValueRegister() {
    return ElementTransitionAndStoreDescriptor::ValueRegister();
  }
  static const Register MapRegister() {
    return ElementTransitionAndStoreDescriptor::MapRegister();
  }
  static const Register KeyRegister() {
    return ElementTransitionAndStoreDescriptor::NameRegister();
  }
  static const Register ObjectRegister() {
    return ElementTransitionAndStoreDescriptor::ReceiverRegister();
  }

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  class FromBits : public BitField<ElementsKind, 0, 8> {};
  class ToBits : public BitField<ElementsKind, 8, 8> {};
  class IsJSArrayBits : public BitField<bool, 16, 1> {};
  class StoreModeBits : public BitField<KeyedAccessStoreMode, 17, 4> {};

  DISALLOW_COPY_AND_ASSIGN(ElementsTransitionAndStoreStub);
};


class StoreArrayLiteralElementStub : public PlatformCodeStub {
 public:
  explicit StoreArrayLiteralElementStub(Isolate* isolate)
      : PlatformCodeStub(isolate) { }

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  void Generate(MacroAssembler* masm);

  DISALLOW_COPY_AND_ASSIGN(StoreArrayLiteralElementStub);
};


class StubFailureTrampolineStub : public PlatformCodeStub {
 public:
  StubFailureTrampolineStub(Isolate* isolate, StubFunctionMode function_mode)
      : PlatformCodeStub(isolate) {
    minor_key_ = FunctionModeField::encode(function_mode);
  }

  static void GenerateAheadOfTime(Isolate* isolate);

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  StubFunctionMode function_mode() const {
    return FunctionModeField::decode(minor_key_);
  }

  void Generate(MacroAssembler* masm);

  class FunctionModeField : public BitField<StubFunctionMode, 0, 1> {};

  DISALLOW_COPY_AND_ASSIGN(StubFailureTrampolineStub);
};


class ProfileEntryHookStub : public PlatformCodeStub {
 public:
  explicit ProfileEntryHookStub(Isolate* isolate) : PlatformCodeStub(isolate) {}

  // The profile entry hook function is not allowed to cause a GC.
  virtual bool SometimesSetsUpAFrame() { return false; }

  // Generates a call to the entry hook if it's enabled.
  static void MaybeCallEntryHook(MacroAssembler* masm);

 private:
  static void EntryHookTrampoline(intptr_t function,
                                  intptr_t stack_pointer,
                                  Isolate* isolate);

  virtual inline Major MajorKey() const FINAL OVERRIDE;

  void Generate(MacroAssembler* masm);

  DISALLOW_COPY_AND_ASSIGN(ProfileEntryHookStub);
};


class StoreBufferOverflowStub : public PlatformCodeStub {
 public:
  StoreBufferOverflowStub(Isolate* isolate, SaveFPRegsMode save_fp)
      : PlatformCodeStub(isolate) {
    minor_key_ = SaveDoublesBits::encode(save_fp == kSaveFPRegs);
  }

  void Generate(MacroAssembler* masm);

  static void GenerateFixedRegStubsAheadOfTime(Isolate* isolate);
  virtual bool SometimesSetsUpAFrame() { return false; }

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  bool save_doubles() const { return SaveDoublesBits::decode(minor_key_); }

  class SaveDoublesBits : public BitField<bool, 0, 1> {};

  DISALLOW_COPY_AND_ASSIGN(StoreBufferOverflowStub);
};


class SubStringStub : public PlatformCodeStub {
 public:
  explicit SubStringStub(Isolate* isolate) : PlatformCodeStub(isolate) {}

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  void Generate(MacroAssembler* masm);

  DISALLOW_COPY_AND_ASSIGN(SubStringStub);
};


class StringCompareStub : public PlatformCodeStub {
 public:
  explicit StringCompareStub(Isolate* isolate) : PlatformCodeStub(isolate) {}

 private:
  virtual inline Major MajorKey() const FINAL OVERRIDE;

  virtual void Generate(MacroAssembler* masm);

  DISALLOW_COPY_AND_ASSIGN(StringCompareStub);
};


#define DEFINE_MAJOR_KEY(NAME) \
  CodeStub::Major NAME##Stub::MajorKey() const { return NAME; }

CODE_STUB_LIST(DEFINE_MAJOR_KEY)

#undef DEFINE_MAJOR_KEY
} }  // namespace v8::internal

#endif  // V8_CODE_STUBS_H_
