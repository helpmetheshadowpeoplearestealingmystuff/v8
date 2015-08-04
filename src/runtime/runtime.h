// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_RUNTIME_RUNTIME_H_
#define V8_RUNTIME_RUNTIME_H_

#include "src/allocation.h"
#include "src/objects.h"
#include "src/zone.h"

namespace v8 {
namespace internal {

// * Each intrinsic is consistently exposed in JavaScript via 2 names:
//    * %#name, which is always a runtime call.
//    * %_#name, which can be inlined or just a runtime call, the compiler in
//      question decides.
//
// * IntrinsicTypes are Runtime::RUNTIME and Runtime::INLINE, respectively.
//
// * IDs are Runtime::k##name and Runtime::kInline##name, respectively.
//
// * All intrinsics have a C++ implementation Runtime_##name.
//
// * Each compiler has an explicit list of intrisics it supports, falling back
//   to a simple runtime call if necessary.


// Entries have the form F(name, number of arguments, number of values):

#define FOR_EACH_INTRINSIC_ARRAY(F)         \
  F(FinishArrayPrototypeSetup, 1, 1)        \
  F(SpecialArrayFunctions, 0, 1)            \
  F(TransitionElementsKind, 2, 1)           \
  F(PushIfAbsent, 2, 1)                     \
  F(ArrayConcat, 1, 1)                      \
  F(RemoveArrayHoles, 2, 1)                 \
  F(MoveArrayContents, 2, 1)                \
  F(EstimateNumberOfElements, 1, 1)         \
  F(GetArrayKeys, 2, 1)                     \
  F(ArrayConstructor, -1, 1)                \
  F(ArrayConstructorWithSubclassing, -1, 1) \
  F(InternalArrayConstructor, -1, 1)        \
  F(NormalizeElements, 1, 1)                \
  F(GrowArrayElements, 2, 1)                \
  F(HasComplexElements, 1, 1)               \
  F(IsArray, 1, 1)                          \
  F(HasCachedArrayIndex, 1, 1)              \
  F(GetCachedArrayIndex, 1, 1)              \
  F(FixedArrayGet, 2, 1)                    \
  F(FixedArraySet, 3, 1)                    \
  F(FastOneByteArrayJoin, 2, 1)


#define FOR_EACH_INTRINSIC_ATOMICS(F) \
  F(AtomicsCompareExchange, 4, 1)     \
  F(AtomicsLoad, 2, 1)                \
  F(AtomicsStore, 3, 1)               \
  F(AtomicsAdd, 3, 1)                 \
  F(AtomicsSub, 3, 1)                 \
  F(AtomicsAnd, 3, 1)                 \
  F(AtomicsOr, 3, 1)                  \
  F(AtomicsXor, 3, 1)                 \
  F(AtomicsExchange, 3, 1)            \
  F(AtomicsIsLockFree, 1, 1)


#define FOR_EACH_INTRINSIC_FUTEX(F)  \
  F(AtomicsFutexWait, 4, 1)          \
  F(AtomicsFutexWake, 3, 1)          \
  F(AtomicsFutexWakeOrRequeue, 5, 1) \
  F(AtomicsFutexNumWaitersForTesting, 2, 1)


#define FOR_EACH_INTRINSIC_CLASSES(F)         \
  F(ThrowNonMethodError, 0, 1)                \
  F(ThrowUnsupportedSuperError, 0, 1)         \
  F(ThrowConstructorNonCallableError, 0, 1)   \
  F(ThrowArrayNotSubclassableError, 0, 1)     \
  F(ThrowStaticPrototypeError, 0, 1)          \
  F(ThrowIfStaticPrototype, 1, 1)             \
  F(ToMethod, 2, 1)                           \
  F(HomeObjectSymbol, 0, 1)                   \
  F(DefineClass, 6, 1)                        \
  F(DefineClassStrong, 6, 1)                  \
  F(FinalizeClassDefinition, 2, 1)            \
  F(FinalizeClassDefinitionStrong, 2, 1)      \
  F(DefineClassMethod, 3, 1)                  \
  F(ClassGetSourceCode, 1, 1)                 \
  F(LoadFromSuper, 4, 1)                      \
  F(LoadKeyedFromSuper, 4, 1)                 \
  F(StoreToSuper_Strict, 4, 1)                \
  F(StoreToSuper_Sloppy, 4, 1)                \
  F(StoreKeyedToSuper_Strict, 4, 1)           \
  F(StoreKeyedToSuper_Sloppy, 4, 1)           \
  F(HandleStepInForDerivedConstructors, 1, 1) \
  F(DefaultConstructorCallSuper, 2, 1)


#define FOR_EACH_INTRINSIC_COLLECTIONS(F) \
  F(StringGetRawHashField, 1, 1)          \
  F(TheHole, 0, 1)                        \
  F(JSCollectionGetTable, 1, 1)           \
  F(GenericHash, 1, 1)                    \
  F(SetInitialize, 1, 1)                  \
  F(SetGrow, 1, 1)                        \
  F(SetShrink, 1, 1)                      \
  F(SetClear, 1, 1)                       \
  F(SetIteratorInitialize, 3, 1)          \
  F(SetIteratorClone, 1, 1)               \
  F(SetIteratorNext, 2, 1)                \
  F(SetIteratorDetails, 1, 1)             \
  F(MapInitialize, 1, 1)                  \
  F(MapShrink, 1, 1)                      \
  F(MapClear, 1, 1)                       \
  F(MapGrow, 1, 1)                        \
  F(MapIteratorInitialize, 3, 1)          \
  F(MapIteratorClone, 1, 1)               \
  F(MapIteratorDetails, 1, 1)             \
  F(GetWeakMapEntries, 2, 1)              \
  F(MapIteratorNext, 2, 1)                \
  F(WeakCollectionInitialize, 1, 1)       \
  F(WeakCollectionGet, 3, 1)              \
  F(WeakCollectionHas, 3, 1)              \
  F(WeakCollectionDelete, 3, 1)           \
  F(WeakCollectionSet, 4, 1)              \
  F(GetWeakSetValues, 2, 1)               \
  F(ObservationWeakMapCreate, 0, 1)


#define FOR_EACH_INTRINSIC_COMPILER(F)  \
  F(CompileLazy, 1, 1)                  \
  F(CompileOptimized, 2, 1)             \
  F(NotifyStubFailure, 0, 1)            \
  F(NotifyDeoptimized, 1, 1)            \
  F(CompileForOnStackReplacement, 1, 1) \
  F(TryInstallOptimizedCode, 1, 1)      \
  F(CompileString, 2, 1)                \
  F(ResolvePossiblyDirectEval, 5, 1)


#define FOR_EACH_INTRINSIC_DATE(F) \
  F(DateMakeDay, 2, 1)             \
  F(DateSetValue, 3, 1)            \
  F(IsDate, 1, 1)                  \
  F(ThrowNotDateError, 0, 1)       \
  F(DateCurrentTime, 0, 1)         \
  F(DateParseString, 2, 1)         \
  F(DateLocalTimezone, 1, 1)       \
  F(DateToUTC, 1, 1)               \
  F(DateCacheVersion, 0, 1)        \
  F(DateField, 2 /* date object, field index */, 1)


#define FOR_EACH_INTRINSIC_DEBUG(F)            \
  F(HandleDebuggerStatement, 0, 1)             \
  F(DebugBreak, 0, 1)                          \
  F(SetDebugEventListener, 2, 1)               \
  F(ScheduleBreak, 0, 1)                       \
  F(DebugGetInternalProperties, 1, 1)          \
  F(DebugGetPropertyDetails, 2, 1)             \
  F(DebugGetProperty, 2, 1)                    \
  F(DebugPropertyTypeFromDetails, 1, 1)        \
  F(DebugPropertyAttributesFromDetails, 1, 1)  \
  F(DebugPropertyIndexFromDetails, 1, 1)       \
  F(DebugNamedInterceptorPropertyValue, 2, 1)  \
  F(DebugIndexedInterceptorElementValue, 2, 1) \
  F(CheckExecutionState, 1, 1)                 \
  F(GetFrameCount, 1, 1)                       \
  F(GetFrameDetails, 2, 1)                     \
  F(GetScopeCount, 2, 1)                       \
  F(GetStepInPositions, 2, 1)                  \
  F(GetScopeDetails, 4, 1)                     \
  F(GetAllScopesDetails, 4, 1)                 \
  F(GetFunctionScopeCount, 1, 1)               \
  F(GetFunctionScopeDetails, 2, 1)             \
  F(SetScopeVariableValue, 6, 1)               \
  F(DebugPrintScopes, 0, 1)                    \
  F(GetThreadCount, 1, 1)                      \
  F(GetThreadDetails, 2, 1)                    \
  F(SetDisableBreak, 1, 1)                     \
  F(GetBreakLocations, 2, 1)                   \
  F(SetFunctionBreakPoint, 3, 1)               \
  F(SetScriptBreakPoint, 4, 1)                 \
  F(ClearBreakPoint, 1, 1)                     \
  F(ChangeBreakOnException, 2, 1)              \
  F(IsBreakOnException, 1, 1)                  \
  F(PrepareStep, 4, 1)                         \
  F(ClearStepping, 0, 1)                       \
  F(DebugEvaluate, 6, 1)                       \
  F(DebugEvaluateGlobal, 4, 1)                 \
  F(DebugGetLoadedScripts, 0, 1)               \
  F(DebugReferencedBy, 3, 1)                   \
  F(DebugConstructedBy, 2, 1)                  \
  F(DebugGetPrototype, 1, 1)                   \
  F(DebugSetScriptSource, 2, 1)                \
  F(FunctionGetInferredName, 1, 1)             \
  F(GetFunctionCodePositionFromSource, 2, 1)   \
  F(ExecuteInDebugContext, 1, 1)               \
  F(GetDebugContext, 0, 1)                     \
  F(CollectGarbage, 1, 1)                      \
  F(GetHeapUsage, 0, 1)                        \
  F(GetScript, 1, 1)                           \
  F(DebugCallbackSupportsStepping, 1, 1)       \
  F(DebugPrepareStepInIfStepping, 1, 1)        \
  F(DebugPushPromise, 2, 1)                    \
  F(DebugPopPromise, 0, 1)                     \
  F(DebugPromiseEvent, 1, 1)                   \
  F(DebugAsyncTaskEvent, 1, 1)                 \
  F(DebugIsActive, 0, 1)                       \
  F(DebugBreakInOptimizedCode, 0, 1)


#define FOR_EACH_INTRINSIC_FORIN(F) \
  F(ForInDone, 2, 1)                \
  F(ForInFilter, 2, 1)              \
  F(ForInNext, 4, 1)                \
  F(ForInStep, 1, 1)


#define FOR_EACH_INTRINSIC_FUNCTION(F)                      \
  F(IsSloppyModeFunction, 1, 1)                             \
  F(FunctionGetName, 1, 1)                                  \
  F(FunctionSetName, 2, 1)                                  \
  F(FunctionNameShouldPrintAsAnonymous, 1, 1)               \
  F(FunctionMarkNameShouldPrintAsAnonymous, 1, 1)           \
  F(FunctionIsArrow, 1, 1)                                  \
  F(FunctionIsConciseMethod, 1, 1)                          \
  F(FunctionRemovePrototype, 1, 1)                          \
  F(FunctionGetScript, 1, 1)                                \
  F(FunctionGetSourceCode, 1, 1)                            \
  F(FunctionGetScriptSourcePosition, 1, 1)                  \
  F(FunctionGetPositionForOffset, 2, 1)                     \
  F(FunctionSetInstanceClassName, 2, 1)                     \
  F(FunctionSetLength, 2, 1)                                \
  F(FunctionSetPrototype, 2, 1)                             \
  F(FunctionIsAPIFunction, 1, 1)                            \
  F(FunctionIsBuiltin, 1, 1)                                \
  F(SetCode, 2, 1)                                          \
  F(SetNativeFlag, 1, 1)                                    \
  F(ThrowStrongModeTooFewArguments, 0, 1)                   \
  F(IsConstructor, 1, 1)                                    \
  F(SetForceInlineFlag, 1, 1)                               \
  F(FunctionBindArguments, 4, 1)                            \
  F(BoundFunctionGetBindings, 1, 1)                         \
  F(NewObjectFromBound, 1, 1)                               \
  F(Call, -1 /* >= 2 */, 1)                                 \
  F(Apply, 5, 1)                                            \
  F(GetFunctionDelegate, 1, 1)                              \
  F(GetConstructorDelegate, 1, 1)                           \
  F(GetOriginalConstructor, 0, 1)                           \
  F(CallFunction, -1 /* receiver + n args + function */, 1) \
  F(IsConstructCall, 0, 1)                                  \
  F(IsFunction, 1, 1)


#define FOR_EACH_INTRINSIC_GENERATOR(F) \
  F(CreateJSGeneratorObject, 0, 1)      \
  F(SuspendJSGeneratorObject, -1, 1)    \
  F(ResumeJSGeneratorObject, 3, 1)      \
  F(GeneratorClose, 1, 1)               \
  F(GeneratorGetFunction, 1, 1)         \
  F(GeneratorGetContext, 1, 1)          \
  F(GeneratorGetReceiver, 1, 1)         \
  F(GeneratorGetContinuation, 1, 1)     \
  F(GeneratorGetSourcePosition, 1, 1)   \
  F(FunctionIsGenerator, 1, 1)          \
  F(GeneratorNext, 2, 1)                \
  F(GeneratorThrow, 2, 1)


#ifdef V8_I18N_SUPPORT
#define FOR_EACH_INTRINSIC_I18N(F)           \
  F(CanonicalizeLanguageTag, 1, 1)           \
  F(AvailableLocalesOf, 1, 1)                \
  F(GetDefaultICULocale, 0, 1)               \
  F(GetLanguageTagVariants, 1, 1)            \
  F(IsInitializedIntlObject, 1, 1)           \
  F(IsInitializedIntlObjectOfType, 2, 1)     \
  F(MarkAsInitializedIntlObjectOfType, 3, 1) \
  F(GetImplFromInitializedIntlObject, 1, 1)  \
  F(CreateDateTimeFormat, 3, 1)              \
  F(InternalDateFormat, 2, 1)                \
  F(InternalDateParse, 2, 1)                 \
  F(CreateNumberFormat, 3, 1)                \
  F(InternalNumberFormat, 2, 1)              \
  F(InternalNumberParse, 2, 1)               \
  F(CreateCollator, 3, 1)                    \
  F(InternalCompare, 3, 1)                   \
  F(StringNormalize, 2, 1)                   \
  F(CreateBreakIterator, 3, 1)               \
  F(BreakIteratorAdoptText, 2, 1)            \
  F(BreakIteratorFirst, 1, 1)                \
  F(BreakIteratorNext, 1, 1)                 \
  F(BreakIteratorCurrent, 1, 1)              \
  F(BreakIteratorBreakType, 1, 1)
#else
#define FOR_EACH_INTRINSIC_I18N(F)
#endif


#define FOR_EACH_INTRINSIC_INTERNAL(F)        \
  F(CheckIsBootstrapping, 0, 1)               \
  F(Throw, 1, 1)                              \
  F(ReThrow, 1, 1)                            \
  F(UnwindAndFindExceptionHandler, 0, 1)      \
  F(PromoteScheduledException, 0, 1)          \
  F(ThrowReferenceError, 1, 1)                \
  F(NewTypeError, 2, 1)                       \
  F(NewSyntaxError, 2, 1)                     \
  F(NewReferenceError, 2, 1)                  \
  F(ThrowIteratorResultNotAnObject, 1, 1)     \
  F(ThrowStrongModeImplicitConversion, 0, 1)  \
  F(PromiseRejectEvent, 3, 1)                 \
  F(PromiseRevokeReject, 1, 1)                \
  F(PromiseHasHandlerSymbol, 0, 1)            \
  F(StackGuard, 0, 1)                         \
  F(Interrupt, 0, 1)                          \
  F(AllocateInNewSpace, 1, 1)                 \
  F(AllocateInTargetSpace, 2, 1)              \
  F(CollectStackTrace, 2, 1)                  \
  F(RenderCallSite, 0, 1)                     \
  F(MessageGetStartPosition, 1, 1)            \
  F(MessageGetScript, 1, 1)                   \
  F(FormatMessageString, 4, 1)                \
  F(CallSiteGetFileNameRT, 3, 1)              \
  F(CallSiteGetFunctionNameRT, 3, 1)          \
  F(CallSiteGetScriptNameOrSourceUrlRT, 3, 1) \
  F(CallSiteGetMethodNameRT, 3, 1)            \
  F(CallSiteGetLineNumberRT, 3, 1)            \
  F(CallSiteGetColumnNumberRT, 3, 1)          \
  F(CallSiteIsNativeRT, 3, 1)                 \
  F(CallSiteIsToplevelRT, 3, 1)               \
  F(CallSiteIsEvalRT, 3, 1)                   \
  F(CallSiteIsConstructorRT, 3, 1)            \
  F(IS_VAR, 1, 1)                             \
  F(IncrementStatsCounter, 1, 1)              \
  F(Likely, 1, 1)                             \
  F(Unlikely, 1, 1)                           \
  F(HarmonyToString, 0, 1)                    \
  F(GetTypeFeedbackVector, 1, 1)              \
  F(GetCallerJSFunction, 0, 1)                \
  F(GetCodeStubExportsObject, 0, 1)


#define FOR_EACH_INTRINSIC_JSON(F) \
  F(QuoteJSONString, 1, 1)         \
  F(BasicJSONStringify, 1, 1)      \
  F(ParseJson, 1, 1)


#define FOR_EACH_INTRINSIC_LITERALS(F)   \
  F(CreateObjectLiteral, 4, 1)           \
  F(CreateArrayLiteral, 4, 1)            \
  F(CreateArrayLiteralStubBailout, 3, 1) \
  F(StoreArrayLiteralElement, 5, 1)


#define FOR_EACH_INTRINSIC_LIVEEDIT(F)              \
  F(LiveEditFindSharedFunctionInfosForScript, 1, 1) \
  F(LiveEditGatherCompileInfo, 2, 1)                \
  F(LiveEditReplaceScript, 3, 1)                    \
  F(LiveEditFunctionSourceUpdated, 1, 1)            \
  F(LiveEditReplaceFunctionCode, 2, 1)              \
  F(LiveEditFunctionSetScript, 2, 1)                \
  F(LiveEditReplaceRefToNestedFunction, 3, 1)       \
  F(LiveEditPatchFunctionPositions, 2, 1)           \
  F(LiveEditCheckAndDropActivations, 2, 1)          \
  F(LiveEditCompareStrings, 2, 1)                   \
  F(LiveEditRestartFrame, 2, 1)


#define FOR_EACH_INTRINSIC_MATHS(F) \
  F(MathAcos, 1, 1)                 \
  F(MathAsin, 1, 1)                 \
  F(MathAtan, 1, 1)                 \
  F(MathLogRT, 1, 1)                \
  F(DoubleHi, 1, 1)                 \
  F(DoubleLo, 1, 1)                 \
  F(ConstructDouble, 2, 1)          \
  F(RemPiO2, 2, 1)                  \
  F(MathAtan2, 2, 1)                \
  F(MathExpRT, 1, 1)                \
  F(MathClz32, 1, 1)                \
  F(MathFloor, 1, 1)                \
  F(MathPowSlow, 2, 1)              \
  F(MathPowRT, 2, 1)                \
  F(RoundNumber, 1, 1)              \
  F(MathSqrt, 1, 1)                 \
  F(MathFround, 1, 1)               \
  F(MathPow, 2, 1)                  \
  F(IsMinusZero, 1, 1)


#define FOR_EACH_INTRINSIC_NUMBERS(F)  \
  F(NumberToRadixString, 2, 1)         \
  F(NumberToFixed, 2, 1)               \
  F(NumberToExponential, 2, 1)         \
  F(NumberToPrecision, 2, 1)           \
  F(IsValidSmi, 1, 1)                  \
  F(StringToNumber, 1, 1)              \
  F(StringParseInt, 2, 1)              \
  F(StringParseFloat, 1, 1)            \
  F(NumberToStringRT, 1, 1)            \
  F(NumberToStringSkipCache, 1, 1)     \
  F(NumberToInteger, 1, 1)             \
  F(NumberToIntegerMapMinusZero, 1, 1) \
  F(NumberToJSUint32, 1, 1)            \
  F(NumberToJSInt32, 1, 1)             \
  F(NumberToSmi, 1, 1)                 \
  F(NumberAdd, 2, 1)                   \
  F(NumberSub, 2, 1)                   \
  F(NumberMul, 2, 1)                   \
  F(NumberUnaryMinus, 1, 1)            \
  F(NumberDiv, 2, 1)                   \
  F(NumberMod, 2, 1)                   \
  F(NumberImul, 2, 1)                  \
  F(NumberOr, 2, 1)                    \
  F(NumberAnd, 2, 1)                   \
  F(NumberXor, 2, 1)                   \
  F(NumberShl, 2, 1)                   \
  F(NumberShr, 2, 1)                   \
  F(NumberSar, 2, 1)                   \
  F(NumberEquals, 2, 1)                \
  F(NumberCompare, 3, 1)               \
  F(SmiLexicographicCompare, 2, 1)     \
  F(MaxSmi, 0, 1)                      \
  F(NumberToString, 1, 1)              \
  F(IsSmi, 1, 1)                       \
  F(IsNonNegativeSmi, 1, 1)            \
  F(GetRootNaN, 0, 1)


#define FOR_EACH_INTRINSIC_OBJECT(F)                 \
  F(GetPrototype, 1, 1)                              \
  F(InternalSetPrototype, 2, 1)                      \
  F(SetPrototype, 2, 1)                              \
  F(IsInPrototypeChain, 2, 1)                        \
  F(GetOwnProperty, 2, 1)                            \
  F(PreventExtensions, 1, 1)                         \
  F(IsExtensible, 1, 1)                              \
  F(OptimizeObjectForAddingMultipleProperties, 2, 1) \
  F(ObjectFreeze, 1, 1)                              \
  F(ObjectSeal, 1, 1)                                \
  F(GetProperty, 2, 1)                               \
  F(GetPropertyStrong, 2, 1)                         \
  F(KeyedGetProperty, 2, 1)                          \
  F(KeyedGetPropertyStrong, 2, 1)                    \
  F(LoadGlobalViaContext, 1, 1)                      \
  F(StoreGlobalViaContext_Sloppy, 2, 1)              \
  F(StoreGlobalViaContext_Strict, 2, 1)              \
  F(AddNamedProperty, 4, 1)                          \
  F(SetProperty, 4, 1)                               \
  F(AddElement, 3, 1)                                \
  F(AppendElement, 2, 1)                             \
  F(DeleteProperty, 3, 1)                            \
  F(HasOwnProperty, 2, 1)                            \
  F(HasProperty, 2, 1)                               \
  F(HasElement, 2, 1)                                \
  F(IsPropertyEnumerable, 2, 1)                      \
  F(GetPropertyNames, 1, 1)                          \
  F(GetPropertyNamesFast, 1, 1)                      \
  F(GetOwnPropertyNames, 2, 1)                       \
  F(GetOwnElementNames, 1, 1)                        \
  F(GetInterceptorInfo, 1, 1)                        \
  F(GetNamedInterceptorPropertyNames, 1, 1)          \
  F(GetIndexedInterceptorElementNames, 1, 1)         \
  F(OwnKeys, 1, 1)                                   \
  F(ToFastProperties, 1, 1)                          \
  F(ToBool, 1, 1)                                    \
  F(NewStringWrapper, 1, 1)                          \
  F(AllocateHeapNumber, 0, 1)                        \
  F(NewObject, 2, 1)                                 \
  F(NewObjectWithAllocationSite, 3, 1)               \
  F(FinalizeInstanceSize, 1, 1)                      \
  F(GlobalProxy, 1, 1)                               \
  F(LookupAccessor, 3, 1)                            \
  F(LoadMutableDouble, 2, 1)                         \
  F(TryMigrateInstance, 1, 1)                        \
  F(IsJSGlobalProxy, 1, 1)                           \
  F(DefineAccessorPropertyUnchecked, 5, 1)           \
  F(DefineDataPropertyUnchecked, 4, 1)               \
  F(GetDataProperty, 2, 1)                           \
  F(HasFastPackedElements, 1, 1)                     \
  F(ValueOf, 1, 1)                                   \
  F(SetValueOf, 2, 1)                                \
  F(JSValueGetValue, 1, 1)                           \
  F(HeapObjectGetMap, 1, 1)                          \
  F(MapGetInstanceType, 1, 1)                        \
  F(ObjectEquals, 2, 1)                              \
  F(IsObject, 1, 1)                                  \
  F(IsUndetectableObject, 1, 1)                      \
  F(IsSpecObject, 1, 1)                              \
  F(IsStrong, 1, 1)                                  \
  F(ClassOf, 1, 1)                                   \
  F(DefineGetterPropertyUnchecked, 4, 1)             \
  F(DefineSetterPropertyUnchecked, 4, 1)             \
  F(ToObject, 1, 1)


#define FOR_EACH_INTRINSIC_OBSERVE(F)            \
  F(IsObserved, 1, 1)                            \
  F(SetIsObserved, 1, 1)                         \
  F(EnqueueMicrotask, 1, 1)                      \
  F(RunMicrotasks, 0, 1)                         \
  F(DeliverObservationChangeRecords, 2, 1)       \
  F(GetObservationState, 0, 1)                   \
  F(ObserverObjectAndRecordHaveSameOrigin, 3, 1) \
  F(ObjectWasCreatedInCurrentOrigin, 1, 1)       \
  F(GetObjectContextObjectObserve, 1, 1)         \
  F(GetObjectContextObjectGetNotifier, 1, 1)     \
  F(GetObjectContextNotifierPerformChange, 1, 1)


#define FOR_EACH_INTRINSIC_PROXY(F) \
  F(CreateJSProxy, 2, 1)            \
  F(CreateJSFunctionProxy, 4, 1)    \
  F(IsJSProxy, 1, 1)                \
  F(IsJSFunctionProxy, 1, 1)        \
  F(GetHandler, 1, 1)               \
  F(GetCallTrap, 1, 1)              \
  F(GetConstructTrap, 1, 1)         \
  F(Fix, 1, 1)


#define FOR_EACH_INTRINSIC_REGEXP(F)           \
  F(StringReplaceGlobalRegExpWithString, 4, 1) \
  F(StringSplit, 3, 1)                         \
  F(RegExpExec, 4, 1)                          \
  F(RegExpConstructResultRT, 3, 1)             \
  F(RegExpConstructResult, 3, 1)               \
  F(RegExpInitializeAndCompile, 3, 1)          \
  F(MaterializeRegExpLiteral, 4, 1)            \
  F(RegExpExecMultiple, 4, 1)                  \
  F(RegExpExecReThrow, 4, 1)                   \
  F(IsRegExp, 1, 1)


#define FOR_EACH_INTRINSIC_SCOPES(F)                         \
  F(ThrowConstAssignError, 0, 1)                             \
  F(DeclareGlobals, 2, 1)                                    \
  F(InitializeVarGlobal, 3, 1)                               \
  F(InitializeConstGlobal, 2, 1)                             \
  F(DeclareLookupSlot, 2, 1)                                 \
  F(DeclareReadOnlyLookupSlot, 2, 1)                         \
  F(InitializeLegacyConstLookupSlot, 3, 1)                   \
  F(NewArguments, 1, 1) /* TODO(turbofan): Only temporary */ \
  F(NewSloppyArguments, 3, 1)                                \
  F(NewStrictArguments, 3, 1)                                \
  F(NewRestParam, 4, 1)                                      \
  F(NewRestParamSlow, 2, 1)                                  \
  F(NewClosureFromStubFailure, 1, 1)                         \
  F(NewClosure, 3, 1)                                        \
  F(NewScriptContext, 2, 1)                                  \
  F(NewFunctionContext, 1, 1)                                \
  F(PushWithContext, 2, 1)                                   \
  F(PushCatchContext, 3, 1)                                  \
  F(PushBlockContext, 2, 1)                                  \
  F(IsJSModule, 1, 1)                                        \
  F(PushModuleContext, 2, 1)                                 \
  F(DeclareModules, 1, 1)                                    \
  F(DeleteLookupSlot, 2, 1)                                  \
  F(StoreLookupSlot, 4, 1)                                   \
  F(GetArgumentsProperty, 1, 1)                              \
  F(ArgumentsLength, 0, 1)                                   \
  F(Arguments, 1, 1)


#define FOR_EACH_INTRINSIC_SIMD(F)    \
  F(IsSimdValue, 1, 1)                \
  F(SimdToObject, 1, 1)               \
  F(SimdEquals, 2, 1)                 \
  F(SimdSameValue, 2, 1)              \
  F(SimdSameValueZero, 2, 1)          \
  F(CreateFloat32x4, 4, 1)            \
  F(CreateInt32x4, 4, 1)              \
  F(CreateBool32x4, 4, 1)             \
  F(CreateInt16x8, 8, 1)              \
  F(CreateBool16x8, 8, 1)             \
  F(CreateInt8x16, 16, 1)             \
  F(CreateBool8x16, 16, 1)            \
  F(Float32x4Check, 1, 1)             \
  F(Int32x4Check, 1, 1)               \
  F(Bool32x4Check, 1, 1)              \
  F(Int16x8Check, 1, 1)               \
  F(Bool16x8Check, 1, 1)              \
  F(Int8x16Check, 1, 1)               \
  F(Bool8x16Check, 1, 1)              \
  F(Float32x4ExtractLane, 2, 1)       \
  F(Int32x4ExtractLane, 2, 1)         \
  F(Bool32x4ExtractLane, 2, 1)        \
  F(Int16x8ExtractLane, 2, 1)         \
  F(Int16x8UnsignedExtractLane, 2, 1) \
  F(Bool16x8ExtractLane, 2, 1)        \
  F(Int8x16ExtractLane, 2, 1)         \
  F(Int8x16UnsignedExtractLane, 2, 1) \
  F(Bool8x16ExtractLane, 2, 1)        \
  F(Float32x4ReplaceLane, 3, 1)       \
  F(Int32x4ReplaceLane, 3, 1)         \
  F(Bool32x4ReplaceLane, 3, 1)        \
  F(Int16x8ReplaceLane, 3, 1)         \
  F(Bool16x8ReplaceLane, 3, 1)        \
  F(Int8x16ReplaceLane, 3, 1)         \
  F(Bool8x16ReplaceLane, 3, 1)


#define FOR_EACH_INTRINSIC_STRINGS(F)           \
  F(StringReplaceOneCharWithString, 3, 1)       \
  F(StringIndexOf, 3, 1)                        \
  F(StringLastIndexOf, 3, 1)                    \
  F(StringLocaleCompare, 2, 1)                  \
  F(SubStringRT, 3, 1)                          \
  F(SubString, 3, 1)                            \
  F(StringAddRT, 2, 1)                          \
  F(StringAdd, 2, 1)                            \
  F(InternalizeString, 1, 1)                    \
  F(StringMatch, 3, 1)                          \
  F(StringCharCodeAtRT, 2, 1)                   \
  F(CharFromCode, 1, 1)                         \
  F(StringCompareRT, 2, 1)                      \
  F(StringCompare, 2, 1)                        \
  F(StringBuilderConcat, 3, 1)                  \
  F(StringBuilderJoin, 3, 1)                    \
  F(SparseJoinWithSeparator, 3, 1)              \
  F(StringToArray, 2, 1)                        \
  F(StringToLowerCase, 1, 1)                    \
  F(StringToUpperCase, 1, 1)                    \
  F(StringTrim, 3, 1)                           \
  F(TruncateString, 2, 1)                       \
  F(NewString, 2, 1)                            \
  F(NewConsString, 4, 1)                        \
  F(StringEquals, 2, 1)                         \
  F(FlattenString, 1, 1)                        \
  F(StringCharFromCode, 1, 1)                   \
  F(StringCharAt, 2, 1)                         \
  F(OneByteSeqStringGetChar, 2, 1)              \
  F(OneByteSeqStringSetChar, 3, 1)              \
  F(TwoByteSeqStringGetChar, 2, 1)              \
  F(TwoByteSeqStringSetChar, 3, 1)              \
  F(StringCharCodeAt, 2, 1)                     \
  F(IsStringWrapperSafeForDefaultValueOf, 1, 1) \
  F(StringGetLength, 1, 1)


#define FOR_EACH_INTRINSIC_SYMBOL(F) \
  F(CreateSymbol, 1, 1)              \
  F(CreatePrivateSymbol, 1, 1)       \
  F(CreateGlobalPrivateSymbol, 1, 1) \
  F(SymbolDescription, 1, 1)         \
  F(SymbolRegistry, 0, 1)            \
  F(SymbolIsPrivate, 1, 1)


#define FOR_EACH_INTRINSIC_TEST(F)            \
  F(DeoptimizeFunction, 1, 1)                 \
  F(DeoptimizeNow, 0, 1)                      \
  F(RunningInSimulator, 0, 1)                 \
  F(IsConcurrentRecompilationSupported, 0, 1) \
  F(OptimizeFunctionOnNextCall, -1, 1)        \
  F(OptimizeOsr, -1, 1)                       \
  F(NeverOptimizeFunction, 1, 1)              \
  F(GetOptimizationStatus, -1, 1)             \
  F(UnblockConcurrentRecompilation, 0, 1)     \
  F(GetOptimizationCount, 1, 1)               \
  F(GetUndetectable, 0, 1)                    \
  F(ClearFunctionTypeFeedback, 1, 1)          \
  F(NotifyContextDisposed, 0, 1)              \
  F(SetAllocationTimeout, -1 /* 2 || 3 */, 1) \
  F(DebugPrint, 1, 1)                         \
  F(DebugTrace, 0, 1)                         \
  F(GlobalPrint, 1, 1)                        \
  F(SystemBreak, 0, 1)                        \
  F(SetFlags, 1, 1)                           \
  F(Abort, 1, 1)                              \
  F(AbortJS, 1, 1)                            \
  F(NativeScriptsCount, 0, 1)                 \
  F(GetV8Version, 0, 1)                       \
  F(DisassembleFunction, 1, 1)                \
  F(TraceEnter, 0, 1)                         \
  F(TraceExit, 1, 1)                          \
  F(HaveSameMap, 2, 1)                        \
  F(HasFastSmiElements, 1, 1)                 \
  F(HasFastObjectElements, 1, 1)              \
  F(HasFastSmiOrObjectElements, 1, 1)         \
  F(HasFastDoubleElements, 1, 1)              \
  F(HasFastHoleyElements, 1, 1)               \
  F(HasDictionaryElements, 1, 1)              \
  F(HasSloppyArgumentsElements, 1, 1)         \
  F(HasFixedTypedArrayElements, 1, 1)         \
  F(HasFastProperties, 1, 1)                  \
  F(HasFixedUint8Elements, 1, 1)              \
  F(HasFixedInt8Elements, 1, 1)               \
  F(HasFixedUint16Elements, 1, 1)             \
  F(HasFixedInt16Elements, 1, 1)              \
  F(HasFixedUint32Elements, 1, 1)             \
  F(HasFixedInt32Elements, 1, 1)              \
  F(HasFixedFloat32Elements, 1, 1)            \
  F(HasFixedFloat64Elements, 1, 1)            \
  F(HasFixedUint8ClampedElements, 1, 1)


#define FOR_EACH_INTRINSIC_TYPEDARRAY(F)     \
  F(ArrayBufferInitialize, 3, 1)             \
  F(ArrayBufferGetByteLength, 1, 1)          \
  F(ArrayBufferSliceImpl, 3, 1)              \
  F(ArrayBufferIsView, 1, 1)                 \
  F(ArrayBufferNeuter, 1, 1)                 \
  F(TypedArrayInitialize, 6, 1)              \
  F(TypedArrayInitializeFromArrayLike, 4, 1) \
  F(ArrayBufferViewGetByteLength, 1, 1)      \
  F(ArrayBufferViewGetByteOffset, 1, 1)      \
  F(TypedArrayGetLength, 1, 1)               \
  F(DataViewGetBuffer, 1, 1)                 \
  F(TypedArrayGetBuffer, 1, 1)               \
  F(TypedArraySetFastCases, 3, 1)            \
  F(TypedArrayMaxSizeInHeap, 0, 1)           \
  F(IsTypedArray, 1, 1)                      \
  F(IsSharedTypedArray, 1, 1)                \
  F(IsSharedIntegerTypedArray, 1, 1)         \
  F(DataViewInitialize, 4, 1)                \
  F(DataViewGetUint8, 3, 1)                  \
  F(DataViewGetInt8, 3, 1)                   \
  F(DataViewGetUint16, 3, 1)                 \
  F(DataViewGetInt16, 3, 1)                  \
  F(DataViewGetUint32, 3, 1)                 \
  F(DataViewGetInt32, 3, 1)                  \
  F(DataViewGetFloat32, 3, 1)                \
  F(DataViewGetFloat64, 3, 1)                \
  F(DataViewSetUint8, 4, 1)                  \
  F(DataViewSetInt8, 4, 1)                   \
  F(DataViewSetUint16, 4, 1)                 \
  F(DataViewSetInt16, 4, 1)                  \
  F(DataViewSetUint32, 4, 1)                 \
  F(DataViewSetInt32, 4, 1)                  \
  F(DataViewSetFloat32, 4, 1)                \
  F(DataViewSetFloat64, 4, 1)


#define FOR_EACH_INTRINSIC_URI(F) \
  F(URIEscape, 1, 1)              \
  F(URIUnescape, 1, 1)


#define FOR_EACH_INTRINSIC_RETURN_PAIR(F) \
  F(LoadLookupSlot, 2, 2)                 \
  F(LoadLookupSlotNoReferenceError, 2, 2)


// Most intrinsics are implemented in the runtime/ directory, but ICs are
// implemented in ic.cc for now.
#define FOR_EACH_INTRINSIC_IC(F)             \
  F(LoadIC_Miss, 3, 1)                       \
  F(KeyedLoadIC_Miss, 3, 1)                  \
  F(CallIC_Miss, 3, 1)                       \
  F(CallIC_Customization_Miss, 3, 1)         \
  F(StoreIC_Miss, 3, 1)                      \
  F(StoreIC_Slow, 3, 1)                      \
  F(KeyedStoreIC_Miss, 3, 1)                 \
  F(KeyedStoreIC_Slow, 3, 1)                 \
  F(StoreCallbackProperty, 5, 1)             \
  F(LoadPropertyWithInterceptorOnly, 3, 1)   \
  F(LoadPropertyWithInterceptor, 3, 1)       \
  F(LoadElementWithInterceptor, 2, 1)        \
  F(StorePropertyWithInterceptor, 3, 1)      \
  F(CompareIC_Miss, 3, 1)                    \
  F(BinaryOpIC_Miss, 2, 1)                   \
  F(CompareNilIC_Miss, 1, 1)                 \
  F(Unreachable, 0, 1)                       \
  F(ToBooleanIC_Miss, 1, 1)                  \
  F(KeyedLoadIC_MissFromStubFailure, 4, 1)   \
  F(KeyedStoreIC_MissFromStubFailure, 3, 1)  \
  F(StoreIC_MissFromStubFailure, 3, 1)       \
  F(ElementsTransitionAndStoreIC_Miss, 4, 1) \
  F(BinaryOpIC_MissWithAllocationSite, 3, 1) \
  F(LoadIC_MissFromStubFailure, 0, 1)


#define FOR_EACH_INTRINSIC_RETURN_OBJECT(F) \
  FOR_EACH_INTRINSIC_IC(F)                  \
  FOR_EACH_INTRINSIC_ARRAY(F)               \
  FOR_EACH_INTRINSIC_ATOMICS(F)             \
  FOR_EACH_INTRINSIC_CLASSES(F)             \
  FOR_EACH_INTRINSIC_COLLECTIONS(F)         \
  FOR_EACH_INTRINSIC_COMPILER(F)            \
  FOR_EACH_INTRINSIC_DATE(F)                \
  FOR_EACH_INTRINSIC_DEBUG(F)               \
  FOR_EACH_INTRINSIC_FORIN(F)               \
  FOR_EACH_INTRINSIC_FUNCTION(F)            \
  FOR_EACH_INTRINSIC_FUTEX(F)               \
  FOR_EACH_INTRINSIC_GENERATOR(F)           \
  FOR_EACH_INTRINSIC_I18N(F)                \
  FOR_EACH_INTRINSIC_INTERNAL(F)            \
  FOR_EACH_INTRINSIC_JSON(F)                \
  FOR_EACH_INTRINSIC_LITERALS(F)            \
  FOR_EACH_INTRINSIC_LIVEEDIT(F)            \
  FOR_EACH_INTRINSIC_MATHS(F)               \
  FOR_EACH_INTRINSIC_NUMBERS(F)             \
  FOR_EACH_INTRINSIC_OBJECT(F)              \
  FOR_EACH_INTRINSIC_OBSERVE(F)             \
  FOR_EACH_INTRINSIC_PROXY(F)               \
  FOR_EACH_INTRINSIC_REGEXP(F)              \
  FOR_EACH_INTRINSIC_SCOPES(F)              \
  FOR_EACH_INTRINSIC_SIMD(F)                \
  FOR_EACH_INTRINSIC_STRINGS(F)             \
  FOR_EACH_INTRINSIC_SYMBOL(F)              \
  FOR_EACH_INTRINSIC_TEST(F)                \
  FOR_EACH_INTRINSIC_TYPEDARRAY(F)          \
  FOR_EACH_INTRINSIC_URI(F)

// FOR_EACH_INTRINSIC defines the list of all intrinsics, coming in 2 flavors,
// either returning an object or a pair.
#define FOR_EACH_INTRINSIC(F)       \
  FOR_EACH_INTRINSIC_RETURN_PAIR(F) \
  FOR_EACH_INTRINSIC_RETURN_OBJECT(F)


#define F(name, nargs, ressize)                                 \
  Object* Runtime_##name(int args_length, Object** args_object, \
                         Isolate* isolate);
FOR_EACH_INTRINSIC_RETURN_OBJECT(F)
#undef F

//---------------------------------------------------------------------------
// Runtime provides access to all C++ runtime functions.

class RuntimeState {
 public:
  unibrow::Mapping<unibrow::ToUppercase, 128>* to_upper_mapping() {
    return &to_upper_mapping_;
  }
  unibrow::Mapping<unibrow::ToLowercase, 128>* to_lower_mapping() {
    return &to_lower_mapping_;
  }

 private:
  RuntimeState() {}
  unibrow::Mapping<unibrow::ToUppercase, 128> to_upper_mapping_;
  unibrow::Mapping<unibrow::ToLowercase, 128> to_lower_mapping_;

  friend class Isolate;
  friend class Runtime;

  DISALLOW_COPY_AND_ASSIGN(RuntimeState);
};


class JavaScriptFrameIterator;  // Forward declaration.


class Runtime : public AllStatic {
 public:
  enum FunctionId {
#define F(name, nargs, ressize) k##name,
#define I(name, nargs, ressize) kInline##name,
    FOR_EACH_INTRINSIC(F) FOR_EACH_INTRINSIC(I)
#undef I
#undef F
        kNumFunctions,
  };

  enum IntrinsicType { RUNTIME, INLINE };

  // Intrinsic function descriptor.
  struct Function {
    FunctionId function_id;
    IntrinsicType intrinsic_type;
    // The JS name of the function.
    const char* name;

    // The C++ (native) entry point.  NULL if the function is inlined.
    byte* entry;

    // The number of arguments expected. nargs is -1 if the function takes
    // a variable number of arguments.
    int nargs;
    // Size of result.  Most functions return a single pointer, size 1.
    int result_size;
  };

  static const int kNotFound = -1;

  // Add internalized strings for all the intrinsic function names to a
  // StringDictionary.
  static void InitializeIntrinsicFunctionNames(Isolate* isolate,
                                               Handle<NameDictionary> dict);

  // Get the intrinsic function with the given name, which must be internalized.
  static const Function* FunctionForName(Handle<String> name);

  // Get the intrinsic function with the given FunctionId.
  static const Function* FunctionForId(FunctionId id);

  // Get the intrinsic function with the given function entry address.
  static const Function* FunctionForEntry(Address ref);

  MUST_USE_RESULT static MaybeHandle<Object> DeleteObjectProperty(
      Isolate* isolate, Handle<JSReceiver> receiver, Handle<Object> key,
      LanguageMode language_mode);

  MUST_USE_RESULT static MaybeHandle<Object> SetObjectProperty(
      Isolate* isolate, Handle<Object> object, Handle<Object> key,
      Handle<Object> value, LanguageMode language_mode);

  MUST_USE_RESULT static MaybeHandle<Object> GetObjectProperty(
      Isolate* isolate, Handle<Object> object, Handle<Object> key,
      LanguageMode language_mode = SLOPPY);

  MUST_USE_RESULT static MaybeHandle<Object> KeyedGetObjectProperty(
      Isolate* isolate, Handle<Object> receiver_obj, Handle<Object> key_obj,
      LanguageMode language_mode);

  MUST_USE_RESULT static MaybeHandle<Object> GetPrototype(
      Isolate* isolate, Handle<Object> object);

  MUST_USE_RESULT static MaybeHandle<Name> ToName(Isolate* isolate,
                                                  Handle<Object> key);

  static void SetupArrayBuffer(Isolate* isolate,
                               Handle<JSArrayBuffer> array_buffer,
                               bool is_external, void* data,
                               size_t allocated_length,
                               SharedFlag shared = SharedFlag::kNotShared);

  static bool SetupArrayBufferAllocatingData(
      Isolate* isolate, Handle<JSArrayBuffer> array_buffer,
      size_t allocated_length, bool initialize = true,
      SharedFlag shared = SharedFlag::kNotShared);

  static void NeuterArrayBuffer(Handle<JSArrayBuffer> array_buffer);

  enum TypedArrayId {
    // arrayIds below should be synchromized with typedarray.js natives.
    ARRAY_ID_UINT8 = 1,
    ARRAY_ID_INT8 = 2,
    ARRAY_ID_UINT16 = 3,
    ARRAY_ID_INT16 = 4,
    ARRAY_ID_UINT32 = 5,
    ARRAY_ID_INT32 = 6,
    ARRAY_ID_FLOAT32 = 7,
    ARRAY_ID_FLOAT64 = 8,
    ARRAY_ID_UINT8_CLAMPED = 9,
    ARRAY_ID_FIRST = ARRAY_ID_UINT8,
    ARRAY_ID_LAST = ARRAY_ID_UINT8_CLAMPED
  };

  static void ArrayIdToTypeAndSize(int array_id, ExternalArrayType* type,
                                   ElementsKind* fixed_elements_kind,
                                   size_t* element_size);

  // Used in runtime.cc and hydrogen's VisitArrayLiteral.
  MUST_USE_RESULT static MaybeHandle<Object> CreateArrayLiteralBoilerplate(
      Isolate* isolate, Handle<FixedArray> literals,
      Handle<FixedArray> elements, bool is_strong);


  static void JSMapInitialize(Isolate* isolate, Handle<JSMap> map);
  static void JSMapClear(Isolate* isolate, Handle<JSMap> map);
  static void JSSetInitialize(Isolate* isolate, Handle<JSSet> set);
  static void JSSetClear(Isolate* isolate, Handle<JSSet> set);

  static void WeakCollectionInitialize(
      Isolate* isolate, Handle<JSWeakCollection> weak_collection);
  static void WeakCollectionSet(Handle<JSWeakCollection> weak_collection,
                                Handle<Object> key, Handle<Object> value,
                                int32_t hash);
  static bool WeakCollectionDelete(Handle<JSWeakCollection> weak_collection,
                                   Handle<Object> key);
  static bool WeakCollectionDelete(Handle<JSWeakCollection> weak_collection,
                                   Handle<Object> key, int32_t hash);

  static MaybeHandle<JSArray> GetInternalProperties(Isolate* isolate,
                                                    Handle<Object>);

  static bool AtomicIsLockFree(uint32_t size);
};


std::ostream& operator<<(std::ostream&, Runtime::FunctionId);

//---------------------------------------------------------------------------
// Constants used by interface to runtime functions.

class AllocateDoubleAlignFlag : public BitField<bool, 0, 1> {};
class AllocateTargetSpace : public BitField<AllocationSpace, 1, 3> {};

class DeclareGlobalsEvalFlag : public BitField<bool, 0, 1> {};
class DeclareGlobalsNativeFlag : public BitField<bool, 1, 1> {};
STATIC_ASSERT(LANGUAGE_END == 3);
class DeclareGlobalsLanguageMode : public BitField<LanguageMode, 2, 2> {};

//---------------------------------------------------------------------------
// Inline functions

// Assume that 32-bit architectures don't have 64-bit atomic ops.
// TODO(binji): can we do better here?
#if V8_TARGET_ARCH_64_BIT && V8_HOST_ARCH_64_BIT

#define ATOMICS_REQUIRE_LOCK_64_BIT 0

inline bool Runtime::AtomicIsLockFree(uint32_t size) {
  return size == 1 || size == 2 || size == 4 || size == 8;
}

#else

#define ATOMICS_REQUIRE_LOCK_64_BIT 1

inline bool Runtime::AtomicIsLockFree(uint32_t size) {
  return size == 1 || size == 2 || size == 4;
}

#endif

}  // namespace internal
}  // namespace v8

#endif  // V8_RUNTIME_RUNTIME_H_
