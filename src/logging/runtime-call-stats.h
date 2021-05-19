// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LOGGING_RUNTIME_CALL_STATS_H_
#define V8_LOGGING_RUNTIME_CALL_STATS_H_

#include "include/v8.h"

#ifdef V8_RUNTIME_CALL_STATS

#include "src/base/atomic-utils.h"
#include "src/base/optional.h"
#include "src/base/platform/elapsed-timer.h"
#include "src/base/platform/time.h"
#include "src/builtins/builtins-definitions.h"
#include "src/debug/debug-interface.h"
#include "src/execution/thread-id.h"
#include "src/init/heap-symbols.h"
#include "src/logging/tracing-flags.h"
#include "src/runtime/runtime.h"
#include "src/tracing/traced-value.h"
#include "src/tracing/tracing-category-observer.h"

#endif  // V8_RUNTIME_CALL_STATS

namespace v8 {
namespace internal {

#ifdef V8_RUNTIME_CALL_STATS

#define RCS_SCOPE(...) \
  v8::internal::RuntimeCallTimerScope rcs_timer_scope(__VA_ARGS__)

class RuntimeCallCounter final {
 public:
  RuntimeCallCounter() : RuntimeCallCounter(nullptr) {}
  explicit RuntimeCallCounter(const char* name)
      : name_(name), count_(0), time_(0) {}
  V8_NOINLINE void Reset();
  V8_NOINLINE void Dump(v8::tracing::TracedValue* value);
  void Add(RuntimeCallCounter* other);

  const char* name() const { return name_; }
  int64_t count() const { return count_; }
  base::TimeDelta time() const {
    return base::TimeDelta::FromMicroseconds(time_);
  }
  void Increment() { count_++; }
  void Add(base::TimeDelta delta) { time_ += delta.InMicroseconds(); }

 private:
  friend class RuntimeCallStats;

  const char* name_;
  int64_t count_;
  // Stored as int64_t so that its initialization can be deferred.
  int64_t time_;
};

// RuntimeCallTimer is used to keep track of the stack of currently active
// timers used for properly measuring the own time of a RuntimeCallCounter.
class RuntimeCallTimer final {
 public:
  RuntimeCallCounter* counter() { return counter_; }
  void set_counter(RuntimeCallCounter* counter) { counter_ = counter; }
  RuntimeCallTimer* parent() const { return parent_.Value(); }
  void set_parent(RuntimeCallTimer* timer) { parent_.SetValue(timer); }
  const char* name() const { return counter_->name(); }

  inline bool IsStarted() const { return start_ticks_ != base::TimeTicks(); }

  inline void Start(RuntimeCallCounter* counter, RuntimeCallTimer* parent) {
    DCHECK(!IsStarted());
    counter_ = counter;
    parent_.SetValue(parent);
    if (TracingFlags::runtime_stats.load(std::memory_order_relaxed) ==
        v8::tracing::TracingCategoryObserver::ENABLED_BY_SAMPLING) {
      return;
    }
    base::TimeTicks now = RuntimeCallTimer::Now();
    if (parent) parent->Pause(now);
    Resume(now);
    DCHECK(IsStarted());
  }

  void Snapshot();

  inline RuntimeCallTimer* Stop() {
    if (!IsStarted()) return parent();
    base::TimeTicks now = RuntimeCallTimer::Now();
    Pause(now);
    counter_->Increment();
    CommitTimeToCounter();

    RuntimeCallTimer* parent_timer = parent();
    if (parent_timer) {
      parent_timer->Resume(now);
    }
    return parent_timer;
  }

  // Make the time source configurable for testing purposes.
  V8_EXPORT_PRIVATE static base::TimeTicks (*Now)();

  // Helper to switch over to CPU time.
  static base::TimeTicks NowCPUTime();

 private:
  inline void Pause(base::TimeTicks now) {
    DCHECK(IsStarted());
    elapsed_ += (now - start_ticks_);
    start_ticks_ = base::TimeTicks();
  }

  inline void Resume(base::TimeTicks now) {
    DCHECK(!IsStarted());
    start_ticks_ = now;
  }

  inline void CommitTimeToCounter() {
    counter_->Add(elapsed_);
    elapsed_ = base::TimeDelta();
  }

  RuntimeCallCounter* counter_ = nullptr;
  base::AtomicValue<RuntimeCallTimer*> parent_;
  base::TimeTicks start_ticks_;
  base::TimeDelta elapsed_;
};

#define FOR_EACH_GC_COUNTER(V) \
  TRACER_SCOPES(V)             \
  TRACER_BACKGROUND_SCOPES(V)

#define FOR_EACH_API_COUNTER(V)                            \
  V(AccessorPair_New)                                      \
  V(ArrayBuffer_Cast)                                      \
  V(ArrayBuffer_Detach)                                    \
  V(ArrayBuffer_New)                                       \
  V(ArrayBuffer_NewBackingStore)                           \
  V(ArrayBuffer_BackingStore_Reallocate)                   \
  V(Array_CloneElementAt)                                  \
  V(Array_New)                                             \
  V(BigInt64Array_New)                                     \
  V(BigInt_NewFromWords)                                   \
  V(BigIntObject_BigIntValue)                              \
  V(BigIntObject_New)                                      \
  V(BigUint64Array_New)                                    \
  V(BooleanObject_BooleanValue)                            \
  V(BooleanObject_New)                                     \
  V(Context_New)                                           \
  V(Context_NewRemoteContext)                              \
  V(DataView_New)                                          \
  V(Date_New)                                              \
  V(Date_NumberValue)                                      \
  V(Debug_Call)                                            \
  V(debug_GetPrivateMembers)                               \
  V(Error_New)                                             \
  V(External_New)                                          \
  V(Float32Array_New)                                      \
  V(Float64Array_New)                                      \
  V(Function_Call)                                         \
  V(Function_New)                                          \
  V(Function_FunctionProtoToString)                        \
  V(Function_NewInstance)                                  \
  V(FunctionTemplate_GetFunction)                          \
  V(FunctionTemplate_New)                                  \
  V(FunctionTemplate_NewRemoteInstance)                    \
  V(FunctionTemplate_NewWithCache)                         \
  V(FunctionTemplate_NewWithFastHandler)                   \
  V(Int16Array_New)                                        \
  V(Int32Array_New)                                        \
  V(Int8Array_New)                                         \
  V(Isolate_DateTimeConfigurationChangeNotification)       \
  V(Isolate_LocaleConfigurationChangeNotification)         \
  V(JSON_Parse)                                            \
  V(JSON_Stringify)                                        \
  V(Map_AsArray)                                           \
  V(Map_Clear)                                             \
  V(Map_Delete)                                            \
  V(Map_Get)                                               \
  V(Map_Has)                                               \
  V(Map_New)                                               \
  V(Map_Set)                                               \
  V(Message_GetEndColumn)                                  \
  V(Message_GetLineNumber)                                 \
  V(Message_GetSourceLine)                                 \
  V(Message_GetStartColumn)                                \
  V(Module_Evaluate)                                       \
  V(Module_InstantiateModule)                              \
  V(Module_SetSyntheticModuleExport)                       \
  V(NumberObject_New)                                      \
  V(NumberObject_NumberValue)                              \
  V(Object_CallAsConstructor)                              \
  V(Object_CallAsFunction)                                 \
  V(Object_CreateDataProperty)                             \
  V(Object_DefineOwnProperty)                              \
  V(Object_DefineProperty)                                 \
  V(Object_Delete)                                         \
  V(Object_DeleteProperty)                                 \
  V(Object_ForceSet)                                       \
  V(Object_Get)                                            \
  V(Object_GetOwnPropertyDescriptor)                       \
  V(Object_GetOwnPropertyNames)                            \
  V(Object_GetPropertyAttributes)                          \
  V(Object_GetPropertyNames)                               \
  V(Object_GetRealNamedProperty)                           \
  V(Object_GetRealNamedPropertyAttributes)                 \
  V(Object_GetRealNamedPropertyAttributesInPrototypeChain) \
  V(Object_GetRealNamedPropertyInPrototypeChain)           \
  V(Object_Has)                                            \
  V(Object_HasOwnProperty)                                 \
  V(Object_HasRealIndexedProperty)                         \
  V(Object_HasRealNamedCallbackProperty)                   \
  V(Object_HasRealNamedProperty)                           \
  V(Object_IsCodeLike)                                     \
  V(Object_New)                                            \
  V(Object_ObjectProtoToString)                            \
  V(Object_Set)                                            \
  V(Object_SetAccessor)                                    \
  V(Object_SetIntegrityLevel)                              \
  V(Object_SetPrivate)                                     \
  V(Object_SetPrototype)                                   \
  V(ObjectTemplate_New)                                    \
  V(ObjectTemplate_NewInstance)                            \
  V(Object_ToArrayIndex)                                   \
  V(Object_ToBigInt)                                       \
  V(Object_ToDetailString)                                 \
  V(Object_ToInt32)                                        \
  V(Object_ToInteger)                                      \
  V(Object_ToNumber)                                       \
  V(Object_ToObject)                                       \
  V(Object_ToString)                                       \
  V(Object_ToUint32)                                       \
  V(Persistent_New)                                        \
  V(Private_New)                                           \
  V(Promise_Catch)                                         \
  V(Promise_Chain)                                         \
  V(Promise_HasRejectHandler)                              \
  V(Promise_Resolver_New)                                  \
  V(Promise_Resolver_Reject)                               \
  V(Promise_Resolver_Resolve)                              \
  V(Promise_Result)                                        \
  V(Promise_Status)                                        \
  V(Promise_Then)                                          \
  V(Proxy_New)                                             \
  V(RangeError_New)                                        \
  V(ReferenceError_New)                                    \
  V(RegExp_Exec)                                           \
  V(RegExp_New)                                            \
  V(ScriptCompiler_Compile)                                \
  V(ScriptCompiler_CompileFunctionInContext)               \
  V(ScriptCompiler_CompileUnbound)                         \
  V(Script_Run)                                            \
  V(Set_Add)                                               \
  V(Set_AsArray)                                           \
  V(Set_Clear)                                             \
  V(Set_Delete)                                            \
  V(Set_Has)                                               \
  V(Set_New)                                               \
  V(SharedArrayBuffer_New)                                 \
  V(SharedArrayBuffer_NewBackingStore)                     \
  V(String_Concat)                                         \
  V(String_NewExternalOneByte)                             \
  V(String_NewExternalTwoByte)                             \
  V(String_NewFromOneByte)                                 \
  V(String_NewFromTwoByte)                                 \
  V(String_NewFromUtf8)                                    \
  V(String_NewFromUtf8Literal)                             \
  V(StringObject_New)                                      \
  V(StringObject_StringValue)                              \
  V(String_Write)                                          \
  V(String_WriteUtf8)                                      \
  V(Symbol_New)                                            \
  V(SymbolObject_New)                                      \
  V(SymbolObject_SymbolValue)                              \
  V(SyntaxError_New)                                       \
  V(TracedGlobal_New)                                      \
  V(TryCatch_StackTrace)                                   \
  V(TypeError_New)                                         \
  V(Uint16Array_New)                                       \
  V(Uint32Array_New)                                       \
  V(Uint8Array_New)                                        \
  V(Uint8ClampedArray_New)                                 \
  V(UnboundScript_GetId)                                   \
  V(UnboundScript_GetLineNumber)                           \
  V(UnboundScript_GetName)                                 \
  V(UnboundScript_GetSourceMappingURL)                     \
  V(UnboundScript_GetSourceURL)                            \
  V(ValueDeserializer_ReadHeader)                          \
  V(ValueDeserializer_ReadValue)                           \
  V(ValueSerializer_WriteValue)                            \
  V(Value_Equals)                                          \
  V(Value_InstanceOf)                                      \
  V(Value_Int32Value)                                      \
  V(Value_IntegerValue)                                    \
  V(Value_NumberValue)                                     \
  V(Value_TypeOf)                                          \
  V(Value_Uint32Value)                                     \
  V(WasmCompileError_New)                                  \
  V(WasmLinkError_New)                                     \
  V(WasmRuntimeError_New)                                  \
  V(WeakMap_Get)                                           \
  V(WeakMap_New)                                           \
  V(WeakMap_Set)

#define ADD_THREAD_SPECIFIC_COUNTER(V, Prefix, Suffix) \
  V(Prefix##Suffix)                                    \
  V(Prefix##Background##Suffix)

#define FOR_EACH_THREAD_SPECIFIC_COUNTER(V)                                 \
  ADD_THREAD_SPECIFIC_COUNTER(V, Compile, Analyse)                          \
  ADD_THREAD_SPECIFIC_COUNTER(V, Compile, Eval)                             \
  ADD_THREAD_SPECIFIC_COUNTER(V, Compile, Function)                         \
  ADD_THREAD_SPECIFIC_COUNTER(V, Compile, Ignition)                         \
  ADD_THREAD_SPECIFIC_COUNTER(V, Compile, IgnitionFinalization)             \
  ADD_THREAD_SPECIFIC_COUNTER(V, Compile, RewriteReturnResult)              \
  ADD_THREAD_SPECIFIC_COUNTER(V, Compile, ScopeAnalysis)                    \
  ADD_THREAD_SPECIFIC_COUNTER(V, Compile, Script)                           \
                                                                            \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, AllocateFPRegisters)             \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, AllocateGeneralRegisters)        \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, AssembleCode)                    \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, AssignSpillSlots)                \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, BuildLiveRangeBundles)           \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, BuildLiveRanges)                 \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, BytecodeGraphBuilder)            \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, CommitAssignment)                \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, ConnectRanges)                   \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, ControlFlowOptimization)         \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, CSAEarlyOptimization)            \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, CSAOptimization)                 \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, DecideSpillingMode)              \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, DecompressionOptimization)       \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, EarlyOptimization)               \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, EffectLinearization)             \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, EscapeAnalysis)                  \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, FinalizeCode)                    \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, FrameElision)                    \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, GenericLowering)                 \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, Inlining)                        \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, JumpThreading)                   \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, MidTierPopulateReferenceMaps)    \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, MidTierRegisterAllocator)        \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, MidTierRegisterOutputDefinition) \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, MidTierSpillSlotAllocator)       \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, LateOptimization)                \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, LoadElimination)                 \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, LocateSpillSlots)                \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, LoopExitElimination)             \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, LoopPeeling)                     \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, MachineOperatorOptimization)     \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, MeetRegisterConstraints)         \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, MemoryOptimization)              \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, OptimizeMoves)                   \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, PopulatePointerMaps)             \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, PrintGraph)                      \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, ResolveControlFlow)              \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, ResolvePhis)                     \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize,                                  \
                              ScheduledEffectControlLinearization)          \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, ScheduledMachineLowering)        \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, Scheduling)                      \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, SelectInstructions)              \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, SimplifiedLowering)              \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, StoreStoreElimination)           \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, TypeAssertions)                  \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, TypedLowering)                   \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, Typer)                           \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, Untyper)                         \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, VerifyGraph)                     \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, WasmBaseOptimization)            \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, WasmInlining)                    \
  ADD_THREAD_SPECIFIC_COUNTER(V, Optimize, WasmLoopUnrolling)               \
                                                                            \
  ADD_THREAD_SPECIFIC_COUNTER(V, Parse, ArrowFunctionLiteral)               \
  ADD_THREAD_SPECIFIC_COUNTER(V, Parse, FunctionLiteral)                    \
  ADD_THREAD_SPECIFIC_COUNTER(V, Parse, Program)                            \
  ADD_THREAD_SPECIFIC_COUNTER(V, PreParse, ArrowFunctionLiteral)            \
  ADD_THREAD_SPECIFIC_COUNTER(V, PreParse, WithVariableResolution)

#define FOR_EACH_MANUAL_COUNTER(V)             \
  V(AccessorGetterCallback)                    \
  V(AccessorSetterCallback)                    \
  V(ArrayLengthGetter)                         \
  V(ArrayLengthSetter)                         \
  V(BoundFunctionLengthGetter)                 \
  V(BoundFunctionNameGetter)                   \
  V(CodeGenerationFromStringsCallbacks)        \
  V(CompileBackgroundCompileTask)              \
  V(CompileBaseline)                           \
  V(CompileBaselineVisit)                      \
  V(CompileBaselinePreVisit)                   \
  V(CompileCollectSourcePositions)             \
  V(CompileDeserialize)                        \
  V(CompileEnqueueOnDispatcher)                \
  V(CompileFinalizeBackgroundCompileTask)      \
  V(CompileFinishNowOnDispatcher)              \
  V(CompileGetFromOptimizedCodeMap)            \
  V(CompilePublishBackgroundFinalization)      \
  V(CompileSerialize)                          \
  V(CompileWaitForDispatcher)                  \
  V(ConfigureInstance)                         \
  V(CreateApiFunction)                         \
  V(DeoptimizeCode)                            \
  V(DeserializeContext)                        \
  V(DeserializeIsolate)                        \
  V(FinalizationRegistryCleanupFromTask)       \
  V(FunctionCallback)                          \
  V(FunctionLengthGetter)                      \
  V(FunctionPrototypeGetter)                   \
  V(FunctionPrototypeSetter)                   \
  V(GC_Custom_AllAvailableGarbage)             \
  V(GC_Custom_IncrementalMarkingObserver)      \
  V(GC_Custom_SlowAllocateRaw)                 \
  V(GCEpilogueCallback)                        \
  V(GCPrologueCallback)                        \
  V(Genesis)                                   \
  V(GetCompatibleReceiver)                     \
  V(GetMoreDataCallback)                       \
  V(IndexedDefinerCallback)                    \
  V(IndexedDeleterCallback)                    \
  V(IndexedDescriptorCallback)                 \
  V(IndexedEnumeratorCallback)                 \
  V(IndexedGetterCallback)                     \
  V(IndexedQueryCallback)                      \
  V(IndexedSetterCallback)                     \
  V(InstantiateFunction)                       \
  V(InstantiateObject)                         \
  V(Invoke)                                    \
  V(InvokeApiFunction)                         \
  V(InvokeApiInterruptCallbacks)               \
  V(IsCompatibleReceiver)                      \
  V(IsCompatibleReceiverMap)                   \
  V(IsTemplateFor)                             \
  V(JS_Execution)                              \
  V(Map_SetPrototype)                          \
  V(Map_TransitionToAccessorProperty)          \
  V(Map_TransitionToDataProperty)              \
  V(MessageListenerCallback)                   \
  V(NamedDefinerCallback)                      \
  V(NamedDeleterCallback)                      \
  V(NamedDescriptorCallback)                   \
  V(NamedEnumeratorCallback)                   \
  V(NamedGetterCallback)                       \
  V(NamedQueryCallback)                        \
  V(NamedSetterCallback)                       \
  V(Object_DeleteProperty)                     \
  V(ObjectVerify)                              \
  V(OptimizeBackgroundDispatcherJob)           \
  V(OptimizeCode)                              \
  V(OptimizeConcurrentFinalize)                \
  V(OptimizeConcurrentPrepare)                 \
  V(OptimizeFinalizePipelineJob)               \
  V(OptimizeHeapBrokerInitialization)          \
  V(OptimizeNonConcurrent)                     \
  V(OptimizeSerialization)                     \
  V(OptimizeSerializeMetadata)                 \
  V(ParseEval)                                 \
  V(ParseFunction)                             \
  V(PropertyCallback)                          \
  V(PrototypeMap_TransitionToAccessorProperty) \
  V(PrototypeMap_TransitionToDataProperty)     \
  V(PrototypeObject_DeleteProperty)            \
  V(ReconfigureToDataProperty)                 \
  V(UpdateProtector)                           \
  V(StringLengthGetter)                        \
  V(TestCounter1)                              \
  V(TestCounter2)                              \
  V(TestCounter3)                              \
  V(WebSnapshotDeserialize)                    \
  V(WebSnapshotDeserialize_Contexts)           \
  V(WebSnapshotDeserialize_Exports)            \
  V(WebSnapshotDeserialize_Functions)          \
  V(WebSnapshotDeserialize_Maps)               \
  V(WebSnapshotDeserialize_Objects)            \
  V(WebSnapshotDeserialize_Strings)

#define FOR_EACH_HANDLER_COUNTER(V)               \
  V(KeyedLoadIC_KeyedLoadSloppyArgumentsStub)     \
  V(KeyedLoadIC_LoadElementDH)                    \
  V(KeyedLoadIC_LoadIndexedInterceptorStub)       \
  V(KeyedLoadIC_LoadIndexedStringDH)              \
  V(KeyedLoadIC_SlowStub)                         \
  V(KeyedStoreIC_ElementsTransitionAndStoreStub)  \
  V(KeyedStoreIC_KeyedStoreSloppyArgumentsStub)   \
  V(KeyedStoreIC_SlowStub)                        \
  V(KeyedStoreIC_StoreElementStub)                \
  V(KeyedStoreIC_StoreFastElementStub)            \
  V(LoadGlobalIC_LoadScriptContextField)          \
  V(LoadGlobalIC_SlowStub)                        \
  V(LoadIC_FunctionPrototypeStub)                 \
  V(LoadIC_HandlerCacheHit_Accessor)              \
  V(LoadIC_LoadAccessorDH)                        \
  V(LoadIC_LoadAccessorFromPrototypeDH)           \
  V(LoadIC_LoadApiGetterFromPrototypeDH)          \
  V(LoadIC_LoadCallback)                          \
  V(LoadIC_LoadConstantDH)                        \
  V(LoadIC_LoadConstantFromPrototypeDH)           \
  V(LoadIC_LoadFieldDH)                           \
  V(LoadIC_LoadFieldFromPrototypeDH)              \
  V(LoadIC_LoadGlobalDH)                          \
  V(LoadIC_LoadGlobalFromPrototypeDH)             \
  V(LoadIC_LoadIntegerIndexedExoticDH)            \
  V(LoadIC_LoadInterceptorDH)                     \
  V(LoadIC_LoadInterceptorFromPrototypeDH)        \
  V(LoadIC_LoadNativeDataPropertyDH)              \
  V(LoadIC_LoadNativeDataPropertyFromPrototypeDH) \
  V(LoadIC_LoadNonexistentDH)                     \
  V(LoadIC_LoadNonMaskingInterceptorDH)           \
  V(LoadIC_LoadNormalDH)                          \
  V(LoadIC_LoadNormalFromPrototypeDH)             \
  V(LoadIC_NonReceiver)                           \
  V(LoadIC_SlowStub)                              \
  V(LoadIC_StringLength)                          \
  V(LoadIC_StringWrapperLength)                   \
  V(StoreGlobalIC_SlowStub)                       \
  V(StoreGlobalIC_StoreScriptContextField)        \
  V(StoreIC_HandlerCacheHit_Accessor)             \
  V(StoreIC_NonReceiver)                          \
  V(StoreIC_SlowStub)                             \
  V(StoreIC_StoreAccessorDH)                      \
  V(StoreIC_StoreAccessorOnPrototypeDH)           \
  V(StoreIC_StoreApiSetterOnPrototypeDH)          \
  V(StoreIC_StoreFieldDH)                         \
  V(StoreIC_StoreGlobalDH)                        \
  V(StoreIC_StoreGlobalTransitionDH)              \
  V(StoreIC_StoreInterceptorStub)                 \
  V(StoreIC_StoreNativeDataPropertyDH)            \
  V(StoreIC_StoreNativeDataPropertyOnPrototypeDH) \
  V(StoreIC_StoreNormalDH)                        \
  V(StoreIC_StoreTransitionDH)                    \
  V(StoreInArrayLiteralIC_SlowStub)

enum RuntimeCallCounterId {
#define CALL_RUNTIME_COUNTER(name) kGC_##name,
  FOR_EACH_GC_COUNTER(CALL_RUNTIME_COUNTER)
#undef CALL_RUNTIME_COUNTER
#define CALL_RUNTIME_COUNTER(name) k##name,
      FOR_EACH_MANUAL_COUNTER(CALL_RUNTIME_COUNTER)
#undef CALL_RUNTIME_COUNTER
#define CALL_RUNTIME_COUNTER(name, nargs, ressize) kRuntime_##name,
          FOR_EACH_INTRINSIC(CALL_RUNTIME_COUNTER)
#undef CALL_RUNTIME_COUNTER
#define CALL_BUILTIN_COUNTER(name) kBuiltin_##name,
              BUILTIN_LIST_C(CALL_BUILTIN_COUNTER)
#undef CALL_BUILTIN_COUNTER
#define CALL_BUILTIN_COUNTER(name) kAPI_##name,
                  FOR_EACH_API_COUNTER(CALL_BUILTIN_COUNTER)
#undef CALL_BUILTIN_COUNTER
#define CALL_BUILTIN_COUNTER(name) kHandler_##name,
                      FOR_EACH_HANDLER_COUNTER(CALL_BUILTIN_COUNTER)
#undef CALL_BUILTIN_COUNTER
#define THREAD_SPECIFIC_COUNTER(name) k##name,
                          FOR_EACH_THREAD_SPECIFIC_COUNTER(
                              THREAD_SPECIFIC_COUNTER)
#undef THREAD_SPECIFIC_COUNTER
                              kNumberOfCounters,
};

class RuntimeCallStats final {
 public:
  enum ThreadType { kMainIsolateThread, kWorkerThread };

  // If kExact is chosen the counter will be use as given. With kThreadSpecific,
  // if the RuntimeCallStats was created for a worker thread, then the
  // background specific version of the counter will be used instead.
  enum CounterMode { kExact, kThreadSpecific };

  explicit V8_EXPORT_PRIVATE RuntimeCallStats(ThreadType thread_type);

  // Starting measuring the time for a function. This will establish the
  // connection to the parent counter for properly calculating the own times.
  V8_EXPORT_PRIVATE void Enter(RuntimeCallTimer* timer,
                               RuntimeCallCounterId counter_id);

  // Leave a scope for a measured runtime function. This will properly add
  // the time delta to the current_counter and subtract the delta from its
  // parent.
  V8_EXPORT_PRIVATE void Leave(RuntimeCallTimer* timer);

  // Set counter id for the innermost measurement. It can be used to refine
  // event kind when a runtime entry counter is too generic.
  V8_EXPORT_PRIVATE void CorrectCurrentCounterId(
      RuntimeCallCounterId counter_id, CounterMode mode = kExact);

  V8_EXPORT_PRIVATE void Reset();
  // Add all entries from another stats object.
  void Add(RuntimeCallStats* other);
  V8_EXPORT_PRIVATE void Print(std::ostream& os);
  V8_EXPORT_PRIVATE void Print();
  V8_NOINLINE void Dump(v8::tracing::TracedValue* value);

  V8_EXPORT_PRIVATE void EnumerateCounters(
      debug::RuntimeCallCounterCallback callback);

  ThreadId thread_id() const { return thread_id_; }
  RuntimeCallTimer* current_timer() { return current_timer_.Value(); }
  RuntimeCallCounter* current_counter() { return current_counter_.Value(); }
  bool InUse() { return in_use_; }
  bool IsCalledOnTheSameThread();

  V8_EXPORT_PRIVATE bool IsBackgroundThreadSpecificVariant(
      RuntimeCallCounterId id);
  V8_EXPORT_PRIVATE bool HasThreadSpecificCounterVariants(
      RuntimeCallCounterId id);

  // This should only be called for counters with a dual Background variant. If
  // on the main thread, this just returns the counter. If on a worker thread,
  // it returns Background variant of the counter.
  RuntimeCallCounterId CounterIdForThread(RuntimeCallCounterId id) {
    DCHECK(HasThreadSpecificCounterVariants(id));
    // All thread specific counters are laid out with the main thread variant
    // first followed by the background variant.
    return thread_type_ == kWorkerThread
               ? static_cast<RuntimeCallCounterId>(id + 1)
               : id;
  }

  bool IsCounterAppropriateForThread(RuntimeCallCounterId id) {
    // TODO(delphick): We should add background-only counters and ensure that
    // all counters (not just the thread-specific variants) are only invoked on
    // the correct thread.
    if (!HasThreadSpecificCounterVariants(id)) return true;
    return IsBackgroundThreadSpecificVariant(id) ==
           (thread_type_ == kWorkerThread);
  }

  static const int kNumberOfCounters =
      static_cast<int>(RuntimeCallCounterId::kNumberOfCounters);
  RuntimeCallCounter* GetCounter(RuntimeCallCounterId counter_id) {
    return &counters_[static_cast<int>(counter_id)];
  }
  RuntimeCallCounter* GetCounter(int counter_id) {
    return &counters_[counter_id];
  }

 private:
  // Top of a stack of active timers.
  base::AtomicValue<RuntimeCallTimer*> current_timer_;
  // Active counter object associated with current timer.
  base::AtomicValue<RuntimeCallCounter*> current_counter_;
  // Used to track nested tracing scopes.
  bool in_use_;
  ThreadType thread_type_;
  ThreadId thread_id_;
  RuntimeCallCounter counters_[kNumberOfCounters];
};

class WorkerThreadRuntimeCallStats final {
 public:
  WorkerThreadRuntimeCallStats();
  ~WorkerThreadRuntimeCallStats();

  // Returns the TLS key associated with this WorkerThreadRuntimeCallStats.
  base::Thread::LocalStorageKey GetKey();

  // Returns a new worker thread runtime call stats table managed by this
  // WorkerThreadRuntimeCallStats.
  RuntimeCallStats* NewTable();

  // Adds the counters from the worker thread tables to |main_call_stats|.
  void AddToMainTable(RuntimeCallStats* main_call_stats);

 private:
  base::Mutex mutex_;
  std::vector<std::unique_ptr<RuntimeCallStats>> tables_;
  base::Optional<base::Thread::LocalStorageKey> tls_key_;
  // Since this is for creating worker thread runtime-call stats, record the
  // main thread ID to ensure we never create a worker RCS table for the main
  // thread.
  ThreadId isolate_thread_id_;
};

// Creating a WorkerThreadRuntimeCallStatsScope will provide a thread-local
// runtime call stats table, and will dump the table to an immediate trace event
// when it is destroyed.
class V8_NODISCARD WorkerThreadRuntimeCallStatsScope final {
 public:
  explicit WorkerThreadRuntimeCallStatsScope(
      WorkerThreadRuntimeCallStats* off_thread_stats);
  ~WorkerThreadRuntimeCallStatsScope();

  RuntimeCallStats* Get() const { return table_; }

 private:
  RuntimeCallStats* table_;
};

#define CHANGE_CURRENT_RUNTIME_COUNTER(runtime_call_stats, counter_id) \
  do {                                                                 \
    if (V8_UNLIKELY(TracingFlags::is_runtime_stats_enabled()) &&       \
        runtime_call_stats) {                                          \
      runtime_call_stats->CorrectCurrentCounterId(counter_id);         \
    }                                                                  \
  } while (false)

#define TRACE_HANDLER_STATS(isolate, counter_name) \
  CHANGE_CURRENT_RUNTIME_COUNTER(                  \
      isolate->counters()->runtime_call_stats(),   \
      RuntimeCallCounterId::kHandler_##counter_name)

// A RuntimeCallTimerScopes wraps around a RuntimeCallTimer to measure the
// the time of C++ scope.
class V8_NODISCARD RuntimeCallTimerScope {
 public:
  inline RuntimeCallTimerScope(Isolate* isolate,
                               RuntimeCallCounterId counter_id);
  inline RuntimeCallTimerScope(RuntimeCallStats* stats,
                               RuntimeCallCounterId counter_id,
                               RuntimeCallStats::CounterMode mode =
                                   RuntimeCallStats::CounterMode::kExact) {
    if (V8_LIKELY(!TracingFlags::is_runtime_stats_enabled() ||
                  stats == nullptr)) {
      return;
    }
    stats_ = stats;
    if (mode == RuntimeCallStats::CounterMode::kThreadSpecific) {
      counter_id = stats->CounterIdForThread(counter_id);
    }

    DCHECK(stats->IsCounterAppropriateForThread(counter_id));
    stats_->Enter(&timer_, counter_id);
  }

  inline ~RuntimeCallTimerScope() {
    if (V8_UNLIKELY(stats_ != nullptr)) {
      stats_->Leave(&timer_);
    }
  }

  RuntimeCallTimerScope(const RuntimeCallTimerScope&) = delete;
  RuntimeCallTimerScope& operator=(const RuntimeCallTimerScope&) = delete;

 private:
  RuntimeCallStats* stats_ = nullptr;
  RuntimeCallTimer timer_;
};

#else  // RUNTIME_CALL_STATS

#define RCS_SCOPE(...)
#define TRACE_HANDLER_STATS(...)
#define CHANGE_CURRENT_RUNTIME_COUNTER(...)

// Create dummy types to limit code changes
class WorkerThreadRuntimeCallStats {};

class RuntimeCallStats {
 public:
  enum ThreadType { kMainIsolateThread, kWorkerThread };
  explicit V8_EXPORT_PRIVATE RuntimeCallStats(ThreadType thread_type) {}
};

class WorkerThreadRuntimeCallStatsScope {
 public:
  explicit WorkerThreadRuntimeCallStatsScope(
      WorkerThreadRuntimeCallStats* off_thread_stats) {}
  RuntimeCallStats* Get() const { return nullptr; }
};

#endif  // RUNTIME_CALL_STATS

}  // namespace internal
}  // namespace v8

#endif  // V8_LOGGING_RUNTIME_CALL_STATS_H_
