// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_RUNTIME_RUNTIME_H_
#define V8_RUNTIME_RUNTIME_H_

#include <memory>

#include "include/v8-maybe.h"
#include "src/base/bit-field.h"
#include "src/base/platform/time.h"
#include "src/common/globals.h"
#include "src/handles/handles.h"
#include "src/objects/elements-kind.h"
#include "src/strings/unicode.h"
#include "src/utils/allocation.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

// * Each intrinsic is exposed in JavaScript via:
//    * %#name, which is always a runtime call.
//    * (optionally) %_#name, which can be inlined or just a runtime call, the
//      compiler in question decides.
//
// * IntrinsicTypes are Runtime::RUNTIME and Runtime::INLINE, respectively.
//
// * IDs are Runtime::k##name and Runtime::kInline##name, respectively.
//
// * All intrinsics have a C++ implementation Runtime_##name.
//
// * Each compiler has an explicit list of intrisics it supports, falling back
//   to a simple runtime call if necessary.

// Entries have the form F(name, number of arguments, number of return values):
// A variable number of arguments is specified by a -1, additional restrictions
// are specified by inline comments. To declare only the runtime version (no
// inline), use the F macro below. To declare the runtime version and the inline
// version simultaneously, use the I macro below.

#define FOR_EACH_INTRINSIC_ARRAY(F, I) \
  F(ArrayIncludes_Slow, 3, 1)          \
  F(ArrayIndexOf, 3, 1)                \
  F(ArrayIsArray, 1, 1)                \
  F(ArraySpeciesConstructor, 1, 1)     \
  F(GrowArrayElements, 2, 1)           \
  F(IsArray, 1, 1)                     \
  F(NewArray, -1 /* >= 3 */, 1)        \
  F(NormalizeElements, 1, 1)           \
  F(TransitionElementsKind, 2, 1)      \
  F(TransitionElementsKindWithKind, 2, 1)

#define FOR_EACH_INTRINSIC_ATOMICS(F, I)               \
  F(AtomicsLoad64, 2, 1)                               \
  F(AtomicsStore64, 3, 1)                              \
  F(AtomicsAdd, 3, 1)                                  \
  F(AtomicsAnd, 3, 1)                                  \
  F(AtomicsCompareExchange, 4, 1)                      \
  F(AtomicsExchange, 3, 1)                             \
  F(AtomicsNumWaitersForTesting, 2, 1)                 \
  F(AtomicsNumAsyncWaitersForTesting, 0, 1)            \
  F(AtomicsNumUnresolvedAsyncPromisesForTesting, 2, 1) \
  F(AtomicsOr, 3, 1)                                   \
  F(AtomicsSub, 3, 1)                                  \
  F(AtomicsXor, 3, 1)                                  \
  F(SetAllowAtomicsWait, 1, 1)                         \
  F(AtomicsLoadSharedStructField, 2, 1)                \
  F(AtomicsStoreSharedStructField, 3, 1)               \
  F(AtomicsExchangeSharedStructField, 3, 1)

#define FOR_EACH_INTRINSIC_BIGINT(F, I) \
  F(BigIntBinaryOp, 3, 1)               \
  F(BigIntCompareToBigInt, 3, 1)        \
  F(BigIntCompareToNumber, 3, 1)        \
  F(BigIntCompareToString, 3, 1)        \
  F(BigIntEqualToBigInt, 2, 1)          \
  F(BigIntEqualToNumber, 2, 1)          \
  F(BigIntEqualToString, 2, 1)          \
  F(BigIntMaxLengthBits, 0, 1)          \
  F(BigIntToBoolean, 1, 1)              \
  F(BigIntToNumber, 1, 1)               \
  F(BigIntUnaryOp, 2, 1)                \
  F(ToBigInt, 1, 1)

#define FOR_EACH_INTRINSIC_CLASSES(F, I)    \
  F(DefineClass, -1 /* >= 3 */, 1)          \
  F(LoadFromSuper, 3, 1)                    \
  F(LoadKeyedFromSuper, 3, 1)               \
  F(StoreKeyedToSuper, 4, 1)                \
  F(StoreToSuper, 4, 1)                     \
  F(ThrowConstructorNonCallableError, 1, 1) \
  F(ThrowNotSuperConstructor, 2, 1)         \
  F(ThrowStaticPrototypeError, 0, 1)        \
  F(ThrowSuperAlreadyCalledError, 0, 1)     \
  F(ThrowSuperNotCalled, 0, 1)              \
  F(ThrowUnsupportedSuperError, 0, 1)

#define FOR_EACH_INTRINSIC_COLLECTIONS(F, I) \
  F(MapGrow, 1, 1)                           \
  F(MapShrink, 1, 1)                         \
  F(SetGrow, 1, 1)                           \
  F(SetShrink, 1, 1)                         \
  F(TheHole, 0, 1)                           \
  F(WeakCollectionDelete, 3, 1)              \
  F(WeakCollectionSet, 4, 1)

#define FOR_EACH_INTRINSIC_COMPILER(F, I) \
  F(CompileOptimizedOSR, 0, 1)            \
  F(TraceOptimizedOSREntry, 0, 1)         \
  F(CompileLazy, 1, 1)                    \
  F(CompileBaseline, 1, 1)                \
  F(CompileMaglev_Concurrent, 1, 1)       \
  F(CompileMaglev_Synchronous, 1, 1)      \
  F(CompileTurbofan_Concurrent, 1, 1)     \
  F(CompileTurbofan_Synchronous, 1, 1)    \
  F(InstallBaselineCode, 1, 1)            \
  F(HealOptimizedCodeSlot, 1, 1)          \
  F(InstantiateAsmJs, 4, 1)               \
  F(NotifyDeoptimized, 0, 1)              \
  F(ObserveNode, 1, 1)                    \
  F(ResolvePossiblyDirectEval, 6, 1)      \
  F(VerifyType, 1, 1)

#define FOR_EACH_INTRINSIC_DATE(F, I) F(DateCurrentTime, 0, 1)

#define FOR_EACH_INTRINSIC_DEBUG(F, I)          \
  F(ClearStepping, 0, 1)                        \
  F(CollectGarbage, 1, 1)                       \
  F(DebugAsyncFunctionSuspended, 4, 1)          \
  F(DebugBreakAtEntry, 1, 1)                    \
  F(DebugCollectCoverage, 0, 1)                 \
  F(DebugGetLoadedScriptIds, 0, 1)              \
  F(DebugOnFunctionCall, 2, 1)                  \
  F(DebugPopPromise, 0, 1)                      \
  F(DebugPrepareStepInSuspendedGenerator, 0, 1) \
  F(DebugPromiseThen, 1, 1)                     \
  F(DebugPushPromise, 1, 1)                     \
  F(DebugToggleBlockCoverage, 1, 1)             \
  F(DebugTogglePreciseCoverage, 1, 1)           \
  F(FunctionGetInferredName, 1, 1)              \
  F(GetBreakLocations, 1, 1)                    \
  F(GetGeneratorScopeCount, 1, 1)               \
  F(GetGeneratorScopeDetails, 2, 1)             \
  F(HandleDebuggerStatement, 0, 1)              \
  F(IsBreakOnException, 1, 1)                   \
  F(LiveEditPatchScript, 2, 1)                  \
  F(ProfileCreateSnapshotDataBlob, 0, 1)        \
  F(ScheduleBreak, 0, 1)                        \
  F(ScriptLocationFromLine2, 4, 1)              \
  F(SetGeneratorScopeVariableValue, 4, 1)       \
  I(IncBlockCounter, 2, 1)

#define FOR_EACH_INTRINSIC_FORIN(F, I) \
  F(ForInEnumerate, 1, 1)              \
  F(ForInHasProperty, 2, 1)

#ifdef V8_TRACE_UNOPTIMIZED
#define FOR_EACH_INTRINSIC_TRACE_UNOPTIMIZED(F, I) \
  F(TraceUnoptimizedBytecodeEntry, 3, 1)           \
  F(TraceUnoptimizedBytecodeExit, 3, 1)
#else
#define FOR_EACH_INTRINSIC_TRACE_UNOPTIMIZED(F, I)
#endif

#ifdef V8_TRACE_FEEDBACK_UPDATES
#define FOR_EACH_INTRINSIC_TRACE_FEEDBACK(F, I) F(TraceUpdateFeedback, 3, 1)
#else
#define FOR_EACH_INTRINSIC_TRACE_FEEDBACK(F, I)
#endif

#define FOR_EACH_INTRINSIC_TRACE(F, I)       \
  FOR_EACH_INTRINSIC_TRACE_UNOPTIMIZED(F, I) \
  FOR_EACH_INTRINSIC_TRACE_FEEDBACK(F, I)

#define FOR_EACH_INTRINSIC_FUNCTION(F, I)  \
  F(Call, -1 /* >= 2 */, 1)                \
  F(FunctionGetScriptSource, 1, 1)         \
  F(FunctionGetScriptId, 1, 1)             \
  F(FunctionGetScriptSourcePosition, 1, 1) \
  F(FunctionGetSourceCode, 1, 1)           \
  F(FunctionIsAPIFunction, 1, 1)           \
  F(IsFunction, 1, 1)

#define FOR_EACH_INTRINSIC_GENERATOR(F, I)    \
  I(AsyncFunctionAwaitCaught, 2, 1)           \
  I(AsyncFunctionAwaitUncaught, 2, 1)         \
  I(AsyncFunctionEnter, 2, 1)                 \
  I(AsyncFunctionReject, 2, 1)                \
  I(AsyncFunctionResolve, 2, 1)               \
  I(AsyncGeneratorAwaitCaught, 2, 1)          \
  I(AsyncGeneratorAwaitUncaught, 2, 1)        \
  F(AsyncGeneratorHasCatchHandlerForPC, 1, 1) \
  I(AsyncGeneratorReject, 2, 1)               \
  I(AsyncGeneratorResolve, 3, 1)              \
  I(AsyncGeneratorYield, 3, 1)                \
  I(CreateJSGeneratorObject, 2, 1)            \
  I(GeneratorClose, 1, 1)                     \
  F(GeneratorGetFunction, 1, 1)               \
  I(GeneratorGetResumeMode, 1, 1)

#ifdef V8_INTL_SUPPORT
#define FOR_EACH_INTRINSIC_INTL(F, I) \
  F(FormatList, 2, 1)                 \
  F(FormatListToParts, 2, 1)          \
  F(StringToLowerCaseIntl, 1, 1)      \
  F(StringToUpperCaseIntl, 1, 1)  // End of macro.
#else
#define FOR_EACH_INTRINSIC_INTL(F, I)
#endif  // V8_INTL_SUPPORT

#define FOR_EACH_INTRINSIC_INTERNAL(F, I)            \
  F(AccessCheck, 1, 1)                               \
  F(AllocateByteArray, 1, 1)                         \
  F(AllocateInYoungGeneration, 2, 1)                 \
  F(AllocateInOldGeneration, 2, 1)                   \
  F(AllocateSeqOneByteString, 1, 1)                  \
  F(AllocateSeqTwoByteString, 1, 1)                  \
  F(AllowDynamicFunction, 1, 1)                      \
  I(CreateAsyncFromSyncIterator, 1, 1)               \
  F(CreateListFromArrayLike, 1, 1)                   \
  F(DoubleToStringWithRadix, 2, 1)                   \
  F(FatalProcessOutOfMemoryInAllocateRaw, 0, 1)      \
  F(FatalProcessOutOfMemoryInvalidArrayLength, 0, 1) \
  F(GetAndResetRuntimeCallStats, -1 /* <= 2 */, 1)   \
  F(GetTemplateObject, 3, 1)                         \
  F(IncrementUseCounter, 1, 1)                       \
  F(BytecodeBudgetInterrupt, 1, 1)                   \
  F(BytecodeBudgetInterruptWithStackCheck, 1, 1)     \
  F(NewError, 2, 1)                                  \
  F(NewForeign, 0, 1)                                \
  F(NewReferenceError, 2, 1)                         \
  F(NewSyntaxError, 2, 1)                            \
  F(NewTypeError, -1 /* [1, 4] */, 1)                \
  F(OrdinaryHasInstance, 2, 1)                       \
  F(PromoteScheduledException, 0, 1)                 \
  F(ReportMessageFromMicrotask, 1, 1)                \
  F(ReThrow, 1, 1)                                   \
  F(ReThrowWithMessage, 2, 1)                        \
  F(RunMicrotaskCallback, 2, 1)                      \
  F(PerformMicrotaskCheckpoint, 0, 1)                \
  F(SharedValueBarrierSlow, 1, 1)                    \
  F(StackGuard, 0, 1)                                \
  F(StackGuardWithGap, 1, 1)                         \
  F(Throw, 1, 1)                                     \
  F(ThrowApplyNonFunction, 1, 1)                     \
  F(ThrowCalledNonCallable, 1, 1)                    \
  F(ThrowConstructedNonConstructable, 1, 1)          \
  F(ThrowConstructorReturnedNonObject, 0, 1)         \
  F(ThrowInvalidStringLength, 0, 1)                  \
  F(ThrowInvalidTypedArrayAlignment, 2, 1)           \
  F(ThrowIteratorError, 1, 1)                        \
  F(ThrowSpreadArgError, 2, 1)                       \
  F(ThrowIteratorResultNotAnObject, 1, 1)            \
  F(ThrowNoAccess, 0, 1)                             \
  F(ThrowNotConstructor, 1, 1)                       \
  F(ThrowPatternAssignmentNonCoercible, 1, 1)        \
  F(ThrowRangeError, -1 /* >= 1 */, 1)               \
  F(ThrowReferenceError, 1, 1)                       \
  F(ThrowAccessedUninitializedVariable, 1, 1)        \
  F(ThrowStackOverflow, 0, 1)                        \
  F(ThrowSymbolAsyncIteratorInvalid, 0, 1)           \
  F(ThrowSymbolIteratorInvalid, 0, 1)                \
  F(ThrowThrowMethodMissing, 0, 1)                   \
  F(ThrowTypeError, -1 /* >= 1 */, 1)                \
  F(ThrowTypeErrorIfStrict, -1 /* >= 1 */, 1)        \
  F(Typeof, 1, 1)                                    \
  F(UnwindAndFindExceptionHandler, 0, 1)

#define FOR_EACH_INTRINSIC_LITERALS(F, I)           \
  F(CreateArrayLiteral, 4, 1)                       \
  F(CreateArrayLiteralWithoutAllocationSite, 2, 1)  \
  F(CreateObjectLiteral, 4, 1)                      \
  F(CreateObjectLiteralWithoutAllocationSite, 2, 1) \
  F(CreateRegExpLiteral, 4, 1)

#define FOR_EACH_INTRINSIC_MODULE(F, I)    \
  F(DynamicImportCall, -1 /* [2, 3] */, 1) \
  I(GetImportMetaObject, 0, 1)             \
  F(GetModuleNamespace, 1, 1)              \
  F(GetModuleNamespaceExport, 2, 1)

#define FOR_EACH_INTRINSIC_NUMBERS(F, I) \
  F(ArrayBufferMaxByteLength, 0, 1)      \
  F(GetHoleNaNLower, 0, 1)               \
  F(GetHoleNaNUpper, 0, 1)               \
  F(IsSmi, 1, 1)                         \
  F(MaxSmi, 0, 1)                        \
  F(NumberToStringSlow, 1, 1)            \
  F(StringParseFloat, 1, 1)              \
  F(StringParseInt, 2, 1)                \
  F(StringToNumber, 1, 1)                \
  F(TypedArrayMaxLength, 0, 1)

#define FOR_EACH_INTRINSIC_OBJECT(F, I)                                \
  F(AddDictionaryProperty, 3, 1)                                       \
  F(AddPrivateBrand, 4, 1)                                             \
  F(AllocateHeapNumber, 0, 1)                                          \
  F(CollectTypeProfile, 3, 1)                                          \
  F(CompleteInobjectSlackTrackingForMap, 1, 1)                         \
  I(CopyDataProperties, 2, 1)                                          \
  I(CopyDataPropertiesWithExcludedPropertiesOnStack, -1 /* >= 1 */, 1) \
  I(CreateDataProperty, 3, 1)                                          \
  I(CreateIterResultObject, 2, 1)                                      \
  F(CreatePrivateAccessors, 2, 1)                                      \
  F(DefineAccessorPropertyUnchecked, 5, 1)                             \
  F(DefineKeyedOwnPropertyInLiteral, 6, 1)                             \
  F(DefineGetterPropertyUnchecked, 4, 1)                               \
  F(DefineSetterPropertyUnchecked, 4, 1)                               \
  F(DeleteProperty, 3, 1)                                              \
  F(GetDerivedMap, 2, 1)                                               \
  F(GetFunctionName, 1, 1)                                             \
  F(GetOwnPropertyDescriptor, 2, 1)                                    \
  F(GetOwnPropertyKeys, 2, 1)                                          \
  F(GetProperty, -1 /* [2, 3] */, 1)                                   \
  F(HasFastPackedElements, 1, 1)                                       \
  F(HasInPrototypeChain, 2, 1)                                         \
  F(HasProperty, 2, 1)                                                 \
  F(InternalSetPrototype, 2, 1)                                        \
  F(IsJSReceiver, 1, 1)                                                \
  F(JSReceiverPreventExtensionsDontThrow, 1, 1)                        \
  F(JSReceiverPreventExtensionsThrow, 1, 1)                            \
  F(JSReceiverGetPrototypeOf, 1, 1)                                    \
  F(JSReceiverSetPrototypeOfDontThrow, 2, 1)                           \
  F(JSReceiverSetPrototypeOfThrow, 2, 1)                               \
  F(LoadPrivateGetter, 1, 1)                                           \
  F(LoadPrivateSetter, 1, 1)                                           \
  F(NewObject, 2, 1)                                                   \
  F(ObjectCreate, 2, 1)                                                \
  F(ObjectEntries, 1, 1)                                               \
  F(ObjectEntriesSkipFastPath, 1, 1)                                   \
  F(ObjectGetOwnPropertyNames, 1, 1)                                   \
  F(ObjectGetOwnPropertyNamesTryFast, 1, 1)                            \
  F(ObjectHasOwnProperty, 2, 1)                                        \
  F(ObjectIsExtensible, 1, 1)                                          \
  F(ObjectKeys, 1, 1)                                                  \
  F(ObjectValues, 1, 1)                                                \
  F(ObjectValuesSkipFastPath, 1, 1)                                    \
  F(OptimizeObjectForAddingMultipleProperties, 2, 1)                   \
  F(SetDataProperties, 2, 1)                                           \
  F(SetKeyedProperty, 3, 1)                                            \
  F(DefineObjectOwnProperty, 3, 1)                                     \
  F(SetNamedProperty, 3, 1)                                            \
  F(SetOwnPropertyIgnoreAttributes, 4, 1)                              \
  F(DefineKeyedOwnPropertyInLiteral_Simple, 3, 1)                      \
  F(ShrinkNameDictionary, 1, 1)                                        \
  F(ShrinkSwissNameDictionary, 1, 1)                                   \
  F(ToFastProperties, 1, 1)                                            \
  F(ToLength, 1, 1)                                                    \
  F(ToName, 1, 1)                                                      \
  F(ToNumber, 1, 1)                                                    \
  F(ToNumeric, 1, 1)                                                   \
  F(ToObject, 1, 1)                                                    \
  F(ToString, 1, 1)                                                    \
  F(TryMigrateInstance, 1, 1)                                          \
  F(SwissTableAdd, 4, 1)                                               \
  F(SwissTableAllocate, 1, 1)                                          \
  F(SwissTableDelete, 2, 1)                                            \
  F(SwissTableDetailsAt, 2, 1)                                         \
  F(SwissTableElementsCount, 1, 1)                                     \
  F(SwissTableEquals, 2, 1)                                            \
  F(SwissTableFindEntry, 2, 1)                                         \
  F(SwissTableUpdate, 4, 1)                                            \
  F(SwissTableValueAt, 2, 1)                                           \
  F(SwissTableKeyAt, 2, 1)

#define FOR_EACH_INTRINSIC_OPERATORS(F, I) \
  F(Add, 2, 1)                             \
  F(Equal, 2, 1)                           \
  F(GreaterThan, 2, 1)                     \
  F(GreaterThanOrEqual, 2, 1)              \
  F(LessThan, 2, 1)                        \
  F(LessThanOrEqual, 2, 1)                 \
  F(NotEqual, 2, 1)                        \
  F(StrictEqual, 2, 1)                     \
  F(StrictNotEqual, 2, 1)                  \
  F(ReferenceEqual, 2, 1)

#define FOR_EACH_INTRINSIC_PROMISE(F, I) \
  F(EnqueueMicrotask, 1, 1)              \
  F(PromiseHookAfter, 1, 1)              \
  F(PromiseHookBefore, 1, 1)             \
  F(PromiseHookInit, 2, 1)               \
  F(PromiseRejectEventFromStack, 2, 1)   \
  F(PromiseRevokeReject, 1, 1)           \
  F(PromiseStatus, 1, 1)                 \
  F(RejectPromise, 3, 1)                 \
  F(ResolvePromise, 2, 1)                \
  F(PromiseRejectAfterResolved, 2, 1)    \
  F(PromiseResolveAfterResolved, 2, 1)   \
  F(ConstructAggregateErrorHelper, 4, 1) \
  F(ConstructInternalAggregateErrorHelper, -1 /* <= 5*/, 1)

#define FOR_EACH_INTRINSIC_PROXY(F, I) \
  F(CheckProxyGetSetTrapResult, 2, 1)  \
  F(CheckProxyHasTrapResult, 2, 1)     \
  F(CheckProxyDeleteTrapResult, 2, 1)  \
  F(GetPropertyWithReceiver, 3, 1)     \
  F(IsJSProxy, 1, 1)                   \
  F(JSProxyGetHandler, 1, 1)           \
  F(JSProxyGetTarget, 1, 1)            \
  F(SetPropertyWithReceiver, 4, 1)

#define FOR_EACH_INTRINSIC_REGEXP(F, I)                          \
  F(IsRegExp, 1, 1)                                              \
  F(RegExpBuildIndices, 3, 1)                                    \
  F(RegExpExec, 4, 1)                                            \
  F(RegExpExecTreatMatchAtEndAsFailure, 4, 1)                    \
  F(RegExpExperimentalOneshotExec, 4, 1)                         \
  F(RegExpExperimentalOneshotExecTreatMatchAtEndAsFailure, 4, 1) \
  F(RegExpExecMultiple, 4, 1)                                    \
  F(RegExpInitializeAndCompile, 3, 1)                            \
  F(RegExpReplaceRT, 3, 1)                                       \
  F(RegExpSplit, 3, 1)                                           \
  F(RegExpStringFromFlags, 1, 1)                                 \
  F(StringReplaceNonGlobalRegExpWithFunction, 3, 1)              \
  F(StringSplit, 3, 1)

#define FOR_EACH_INTRINSIC_SCOPES(F, I)            \
  F(DeclareEvalFunction, 2, 1)                     \
  F(DeclareEvalVar, 1, 1)                          \
  F(DeclareGlobals, 2, 1)                          \
  F(DeclareModuleExports, 2, 1)                    \
  F(DeleteLookupSlot, 1, 1)                        \
  F(LoadLookupSlot, 1, 1)                          \
  F(LoadLookupSlotInsideTypeof, 1, 1)              \
                                                   \
  F(NewClosure, 2, 1)                              \
  F(NewClosure_Tenured, 2, 1)                      \
  F(NewFunctionContext, 1, 1)                      \
  F(NewRestParameter, 1, 1)                        \
  F(NewSloppyArguments, 1, 1)                      \
  F(NewStrictArguments, 1, 1)                      \
  F(PushBlockContext, 1, 1)                        \
  F(PushCatchContext, 2, 1)                        \
  F(PushWithContext, 2, 1)                         \
  F(StoreGlobalNoHoleCheckForReplLetOrConst, 2, 1) \
  F(StoreLookupSlot_Sloppy, 2, 1)                  \
  F(StoreLookupSlot_SloppyHoisting, 2, 1)          \
  F(StoreLookupSlot_Strict, 2, 1)                  \
  F(ThrowConstAssignError, 0, 1)

#define FOR_EACH_INTRINSIC_SHADOW_REALM(F, I) \
  F(ShadowRealmWrappedFunctionCreate, 2, 1)   \
  F(ShadowRealmImportValue, 1, 1)

#define FOR_EACH_INTRINSIC_STRINGS(F, I)  \
  F(FlattenString, 1, 1)                  \
  F(GetSubstitution, 5, 1)                \
  F(InternalizeString, 1, 1)              \
  F(StringAdd, 2, 1)                      \
  F(StringBuilderConcat, 3, 1)            \
  F(StringCharCodeAt, 2, 1)               \
  F(StringEqual, 2, 1)                    \
  F(StringEscapeQuotes, 1, 1)             \
  F(StringGreaterThan, 2, 1)              \
  F(StringGreaterThanOrEqual, 2, 1)       \
  F(StringLastIndexOf, 2, 1)              \
  F(StringLessThan, 2, 1)                 \
  F(StringLessThanOrEqual, 2, 1)          \
  F(StringMaxLength, 0, 1)                \
  F(StringReplaceOneCharWithString, 3, 1) \
  F(StringSubstring, 3, 1)                \
  F(StringToArray, 2, 1)

#define FOR_EACH_INTRINSIC_SYMBOL(F, I)    \
  F(CreatePrivateNameSymbol, 1, 1)         \
  F(CreatePrivateBrandSymbol, 1, 1)        \
  F(CreatePrivateSymbol, -1 /* <= 1 */, 1) \
  F(SymbolDescriptiveString, 1, 1)         \
  F(SymbolIsPrivate, 1, 1)

#define FOR_EACH_INTRINSIC_TEMPORAL(F, I) \
  F(IsInvalidTemporalCalendarField, 2, 1)

#define FOR_EACH_INTRINSIC_TEST(F, I)         \
  F(Abort, 1, 1)                              \
  F(AbortCSADcheck, 1, 1)                     \
  F(AbortJS, 1, 1)                            \
  F(ActiveTierIsMaglev, 1, 1)                 \
  F(ArrayIteratorProtector, 0, 1)             \
  F(ArraySpeciesProtector, 0, 1)              \
  F(BaselineOsr, -1, 1)                       \
  F(BenchMaglev, 2, 1)                        \
  F(ClearFunctionFeedback, 1, 1)              \
  F(ClearMegamorphicStubCache, 0, 1)          \
  F(CompleteInobjectSlackTracking, 1, 1)      \
  F(ConstructConsString, 2, 1)                \
  F(ConstructDouble, 2, 1)                    \
  F(ConstructInternalizedString, 1, 1)        \
  F(ConstructSlicedString, 2, 1)              \
  F(ConstructThinString, 1, 1)                \
  F(DebugPrint, 1, 1)                         \
  F(DebugPrintPtr, 1, 1)                      \
  F(DebugTrace, 0, 1)                         \
  F(DebugTrackRetainingPath, -1, 1)           \
  F(DeoptimizeFunction, 1, 1)                 \
  F(DisableOptimizationFinalization, 0, 1)    \
  F(DisallowCodegenFromStrings, 1, 1)         \
  F(DisassembleFunction, 1, 1)                \
  F(EnableCodeLoggingForTesting, 0, 1)        \
  F(EnsureFeedbackVectorForFunction, 1, 1)    \
  F(FinalizeOptimization, 0, 1)               \
  F(GetCallable, 0, 1)                        \
  F(GetInitializerFunction, 1, 1)             \
  F(GetOptimizationStatus, 1, 1)              \
  F(GetUndetectable, 0, 1)                    \
  F(GlobalPrint, 1, 1)                        \
  F(HasDictionaryElements, 1, 1)              \
  F(HasDoubleElements, 1, 1)                  \
  F(HasElementsInALargeObjectSpace, 1, 1)     \
  F(HasFastElements, 1, 1)                    \
  F(HasFastProperties, 1, 1)                  \
  F(HasFixedBigInt64Elements, 1, 1)           \
  F(HasFixedBigUint64Elements, 1, 1)          \
  F(HasFixedFloat32Elements, 1, 1)            \
  F(HasFixedFloat64Elements, 1, 1)            \
  F(HasFixedInt16Elements, 1, 1)              \
  F(HasFixedInt32Elements, 1, 1)              \
  F(HasFixedInt8Elements, 1, 1)               \
  F(HasFixedUint16Elements, 1, 1)             \
  F(HasFixedUint32Elements, 1, 1)             \
  F(HasFixedUint8ClampedElements, 1, 1)       \
  F(HasFixedUint8Elements, 1, 1)              \
  F(HasHoleyElements, 1, 1)                   \
  F(HasObjectElements, 1, 1)                  \
  F(HasOwnConstDataProperty, 2, 1)            \
  F(HasPackedElements, 1, 1)                  \
  F(HasSloppyArgumentsElements, 1, 1)         \
  F(HasSmiElements, 1, 1)                     \
  F(HasSmiOrObjectElements, 1, 1)             \
  F(HaveSameMap, 2, 1)                        \
  F(HeapObjectVerify, 1, 1)                   \
  F(ICsAreEnabled, 0, 1)                      \
  F(InLargeObjectSpace, 1, 1)                 \
  F(InYoungGeneration, 1, 1)                  \
  F(Is64Bit, 0, 1)                            \
  F(IsAtomicsWaitAllowed, 0, 1)               \
  F(IsBeingInterpreted, 0, 1)                 \
  F(IsConcatSpreadableProtector, 0, 1)        \
  F(IsConcurrentRecompilationSupported, 0, 1) \
  F(IsDictPropertyConstTrackingEnabled, 0, 1) \
  F(IsInternalizedString, 1, 1)               \
  F(IsSameHeapObject, 2, 1)                   \
  F(IsSharedString, 1, 1)                     \
  F(MapIteratorProtector, 0, 1)               \
  F(NeverOptimizeFunction, 1, 1)              \
  F(NewRegExpWithBacktrackLimit, 3, 1)        \
  F(NotifyContextDisposed, 0, 1)              \
  F(OptimizeMaglevOnNextCall, 1, 1)           \
  F(OptimizeFunctionOnNextCall, -1, 1)        \
  F(OptimizeOsr, -1, 1)                       \
  F(PrepareFunctionForOptimization, -1, 1)    \
  F(PretenureAllocationSite, 1, 1)            \
  F(PrintWithNameForAssert, 2, 1)             \
  F(PromiseSpeciesProtector, 0, 1)            \
  F(RegExpSpeciesProtector, 0, 1)             \
  F(RegexpHasBytecode, 2, 1)                  \
  F(RegexpHasNativeCode, 2, 1)                \
  F(RegexpIsUnmodified, 1, 1)                 \
  F(RegexpTypeTag, 1, 1)                      \
  F(RunningInSimulator, 0, 1)                 \
  F(RuntimeEvaluateREPL, 1, 1)                \
  F(ScheduleGCInStackCheck, 0, 1)             \
  F(SerializeDeserializeNow, 0, 1)            \
  F(SetAllocationTimeout, -1 /* 2 || 3 */, 1) \
  F(SetForceSlowPath, 1, 1)                   \
  F(SetIteratorProtector, 0, 1)               \
  F(SharedGC, 0, 1)                           \
  F(SimulateNewspaceFull, 0, 1)               \
  F(StringIteratorProtector, 0, 1)            \
  F(SystemBreak, 0, 1)                        \
  F(TakeHeapSnapshot, -1, 1)                  \
  F(TraceEnter, 0, 1)                         \
  F(TraceExit, 1, 1)                          \
  F(TurbofanStaticAssert, 1, 1)               \
  F(TypedArraySpeciesProtector, 0, 1)         \
  F(WaitForBackgroundOptimization, 0, 1)      \
  I(DeoptimizeNow, 0, 1)

#define FOR_EACH_INTRINSIC_TYPEDARRAY(F, I)    \
  F(ArrayBufferDetach, 1, 1)                   \
  F(GrowableSharedArrayBufferByteLength, 1, 1) \
  F(TypedArrayCopyElements, 3, 1)              \
  F(TypedArrayGetBuffer, 1, 1)                 \
  F(TypedArraySet, 2, 1)                       \
  F(TypedArraySortFast, 1, 1)

#define FOR_EACH_INTRINSIC_WASM(F, I) \
  F(ThrowWasmError, 1, 1)             \
  F(ThrowWasmStackOverflow, 0, 1)     \
  F(WasmI32AtomicWait, 4, 1)          \
  F(WasmI64AtomicWait, 5, 1)          \
  F(WasmAtomicNotify, 3, 1)           \
  F(WasmMemoryGrow, 2, 1)             \
  F(WasmStackGuard, 0, 1)             \
  F(WasmThrow, 2, 1)                  \
  F(WasmReThrow, 1, 1)                \
  F(WasmThrowJSTypeError, 0, 1)       \
  F(WasmRefFunc, 1, 1)                \
  F(WasmFunctionTableGet, 3, 1)       \
  F(WasmFunctionTableSet, 4, 1)       \
  F(WasmTableInit, 6, 1)              \
  F(WasmTableCopy, 6, 1)              \
  F(WasmTableGrow, 3, 1)              \
  F(WasmTableFill, 5, 1)              \
  F(WasmIsValidRefValue, 3, 1)        \
  F(WasmCompileLazy, 2, 1)            \
  F(WasmCompileWrapper, 2, 1)         \
  F(WasmTriggerTierUp, 1, 1)          \
  F(WasmDebugBreak, 0, 1)             \
  F(WasmArrayCopy, 5, 1)              \
  F(WasmArrayInitFromSegment, 5, 1)   \
  F(WasmAllocateContinuation, 1, 1)   \
  F(WasmSyncStackLimit, 0, 1)         \
  F(WasmCreateResumePromise, 2, 1)    \
  F(WasmStringNewWtf8, 5, 1)          \
  F(WasmStringNewWtf16, 4, 1)         \
  F(WasmStringConst, 2, 1)            \
  F(WasmStringMeasureUtf8, 1, 1)      \
  F(WasmStringMeasureWtf8, 1, 1)      \
  F(WasmStringEncodeWtf8, 5, 1)       \
  F(WasmStringEncodeWtf16, 6, 1)

#define FOR_EACH_INTRINSIC_WASM_TEST(F, I) \
  F(DeserializeWasmModule, 2, 1)           \
  F(DisallowWasmCodegen, 1, 1)             \
  F(FreezeWasmLazyCompilation, 1, 1)       \
  F(GetWasmExceptionTagId, 2, 1)           \
  F(GetWasmExceptionValues, 1, 1)          \
  F(GetWasmRecoveredTrapCount, 0, 1)       \
  F(IsAsmWasmCode, 1, 1)                   \
  F(IsLiftoffFunction, 1, 1)               \
  F(IsTurboFanFunction, 1, 1)              \
  F(IsThreadInWasm, 0, 1)                  \
  F(IsWasmCode, 1, 1)                      \
  F(IsWasmTrapHandlerEnabled, 0, 1)        \
  F(SerializeWasmModule, 1, 1)             \
  F(SetWasmCompileControls, 2, 1)          \
  F(SetWasmInstantiateControls, 0, 1)      \
  F(WasmGetNumberOfInstances, 1, 1)        \
  F(WasmNumCodeSpaces, 1, 1)               \
  F(WasmTierDown, 0, 1)                    \
  F(WasmTierUp, 0, 1)                      \
  F(WasmTierUpFunction, 2, 1)              \
  F(WasmTraceEnter, 0, 1)                  \
  F(WasmTraceExit, 1, 1)                   \
  F(WasmTraceMemory, 1, 1)

#define FOR_EACH_INTRINSIC_WEAKREF(F, I)                             \
  F(JSFinalizationRegistryRegisterWeakCellWithUnregisterToken, 4, 1) \
  F(JSWeakRefAddToKeptObjects, 1, 1)                                 \
  F(ShrinkFinalizationRegistryUnregisterTokenMap, 1, 1)

#define FOR_EACH_INTRINSIC_RETURN_PAIR_IMPL(F, I) \
  F(DebugBreakOnBytecode, 1, 2)                   \
  F(LoadLookupSlotForCall, 1, 2)

// Most intrinsics are implemented in the runtime/ directory, but ICs are
// implemented in ic.cc for now.
#define FOR_EACH_INTRINSIC_IC(F, I)          \
  F(ElementsTransitionAndStoreIC_Miss, 6, 1) \
  F(KeyedLoadIC_Miss, 4, 1)                  \
  F(KeyedStoreIC_Miss, 5, 1)                 \
  F(DefineKeyedOwnIC_Miss, 5, 1)             \
  F(StoreInArrayLiteralIC_Miss, 5, 1)        \
  F(DefineNamedOwnIC_Slow, 3, 1)             \
  F(KeyedStoreIC_Slow, 3, 1)                 \
  F(DefineKeyedOwnIC_Slow, 3, 1)             \
  F(LoadElementWithInterceptor, 2, 1)        \
  F(LoadGlobalIC_Miss, 4, 1)                 \
  F(LoadGlobalIC_Slow, 3, 1)                 \
  F(LoadIC_Miss, 4, 1)                       \
  F(LoadNoFeedbackIC_Miss, 4, 1)             \
  F(LoadWithReceiverIC_Miss, 5, 1)           \
  F(LoadWithReceiverNoFeedbackIC_Miss, 3, 1) \
  F(LoadPropertyWithInterceptor, 5, 1)       \
  F(StoreCallbackProperty, 5, 1)             \
  F(StoreGlobalIC_Miss, 4, 1)                \
  F(StoreGlobalICNoFeedback_Miss, 2, 1)      \
  F(StoreGlobalIC_Slow, 5, 1)                \
  F(StoreIC_Miss, 5, 1)                      \
  F(DefineNamedOwnIC_Miss, 5, 1)             \
  F(StoreInArrayLiteralIC_Slow, 5, 1)        \
  F(StorePropertyWithInterceptor, 5, 1)      \
  F(CloneObjectIC_Miss, 4, 1)                \
  F(KeyedHasIC_Miss, 4, 1)                   \
  F(HasElementWithInterceptor, 2, 1)

#define FOR_EACH_INTRINSIC_RETURN_OBJECT_IMPL(F, I) \
  FOR_EACH_INTRINSIC_ARRAY(F, I)                    \
  FOR_EACH_INTRINSIC_ATOMICS(F, I)                  \
  FOR_EACH_INTRINSIC_BIGINT(F, I)                   \
  FOR_EACH_INTRINSIC_CLASSES(F, I)                  \
  FOR_EACH_INTRINSIC_COLLECTIONS(F, I)              \
  FOR_EACH_INTRINSIC_COMPILER(F, I)                 \
  FOR_EACH_INTRINSIC_DATE(F, I)                     \
  FOR_EACH_INTRINSIC_DEBUG(F, I)                    \
  FOR_EACH_INTRINSIC_FORIN(F, I)                    \
  FOR_EACH_INTRINSIC_FUNCTION(F, I)                 \
  FOR_EACH_INTRINSIC_GENERATOR(F, I)                \
  FOR_EACH_INTRINSIC_IC(F, I)                       \
  FOR_EACH_INTRINSIC_INTERNAL(F, I)                 \
  FOR_EACH_INTRINSIC_TRACE(F, I)                    \
  FOR_EACH_INTRINSIC_INTL(F, I)                     \
  FOR_EACH_INTRINSIC_LITERALS(F, I)                 \
  FOR_EACH_INTRINSIC_MODULE(F, I)                   \
  FOR_EACH_INTRINSIC_NUMBERS(F, I)                  \
  FOR_EACH_INTRINSIC_OBJECT(F, I)                   \
  FOR_EACH_INTRINSIC_OPERATORS(F, I)                \
  FOR_EACH_INTRINSIC_PROMISE(F, I)                  \
  FOR_EACH_INTRINSIC_PROXY(F, I)                    \
  FOR_EACH_INTRINSIC_REGEXP(F, I)                   \
  FOR_EACH_INTRINSIC_SCOPES(F, I)                   \
  FOR_EACH_INTRINSIC_SHADOW_REALM(F, I)             \
  FOR_EACH_INTRINSIC_STRINGS(F, I)                  \
  FOR_EACH_INTRINSIC_SYMBOL(F, I)                   \
  FOR_EACH_INTRINSIC_TEMPORAL(F, I)                 \
  FOR_EACH_INTRINSIC_TEST(F, I)                     \
  FOR_EACH_INTRINSIC_TYPEDARRAY(F, I)               \
  IF_WASM(FOR_EACH_INTRINSIC_WASM, F, I)            \
  IF_WASM(FOR_EACH_INTRINSIC_WASM_TEST, F, I)       \
  FOR_EACH_INTRINSIC_WEAKREF(F, I)

// Defines the list of all intrinsics, coming in 2 flavors, either returning an
// object or a pair.
#define FOR_EACH_INTRINSIC_IMPL(F, I)       \
  FOR_EACH_INTRINSIC_RETURN_PAIR_IMPL(F, I) \
  FOR_EACH_INTRINSIC_RETURN_OBJECT_IMPL(F, I)

#define FOR_EACH_INTRINSIC_RETURN_OBJECT(F) \
  FOR_EACH_INTRINSIC_RETURN_OBJECT_IMPL(F, F)

#define FOR_EACH_INTRINSIC_RETURN_PAIR(F) \
  FOR_EACH_INTRINSIC_RETURN_PAIR_IMPL(F, F)

// The list of all intrinsics, including those that have inline versions, but
// not the inline versions themselves.
#define FOR_EACH_INTRINSIC(F) FOR_EACH_INTRINSIC_IMPL(F, F)

// The list of all inline intrinsics only.
#define FOR_EACH_INLINE_INTRINSIC(I) FOR_EACH_INTRINSIC_IMPL(NOTHING, I)

#define F(name, nargs, ressize)                                 \
  Address Runtime_##name(int args_length, Address* args_object, \
                         Isolate* isolate);
FOR_EACH_INTRINSIC_RETURN_OBJECT(F)
#undef F

//---------------------------------------------------------------------------
// Runtime provides access to all C++ runtime functions.

class Runtime : public AllStatic {
 public:
  enum FunctionId : int32_t {
#define F(name, nargs, ressize) k##name,
#define I(name, nargs, ressize) kInline##name,
    FOR_EACH_INTRINSIC(F) FOR_EACH_INLINE_INTRINSIC(I)
#undef I
#undef F
        kNumFunctions,
  };

  static constexpr int kNumInlineFunctions =
#define COUNT(...) +1
      FOR_EACH_INLINE_INTRINSIC(COUNT);
#undef COUNT

  enum IntrinsicType { RUNTIME, INLINE };

  // Intrinsic function descriptor.
  struct Function {
    FunctionId function_id;
    IntrinsicType intrinsic_type;
    // The JS name of the function.
    const char* name;

    // For RUNTIME functions, this is the C++ entry point.
    // For INLINE functions this is the C++ entry point of the fall back.
    Address entry;

    // The number of arguments expected. nargs is -1 if the function takes
    // a variable number of arguments.
    int8_t nargs;
    // Size of result.  Most functions return a single pointer, size 1.
    int8_t result_size;
  };

  static const int kNotFound = -1;

  // Checks whether the runtime function with the given {id} depends on the
  // "current context", i.e. because it does scoped lookups, or whether it's
  // fine to just pass any context within the same "native context".
  static bool NeedsExactContext(FunctionId id);

  // Checks whether the runtime function with the given {id} never returns
  // to it's caller normally, i.e. whether it'll always raise an exception.
  // More specifically: The C++ implementation returns the Heap::exception
  // sentinel, always.
  static bool IsNonReturning(FunctionId id);

  // Check if a runtime function with the given {id} may trigger a heap
  // allocation.
  static bool MayAllocate(FunctionId id);

  // Check if a runtime function with the given {id} is allowlisted for
  // using it with fuzzers.
  static bool IsAllowListedForFuzzing(FunctionId id);

  // Get the intrinsic function with the given name.
  static const Function* FunctionForName(const unsigned char* name, int length);

  // Get the intrinsic function with the given FunctionId.
  V8_EXPORT_PRIVATE static const Function* FunctionForId(FunctionId id);

  // Get the intrinsic function with the given function entry address.
  static const Function* FunctionForEntry(Address ref);

  // Get the runtime intrinsic function table.
  static const Function* RuntimeFunctionTable(Isolate* isolate);

  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static Maybe<bool>
  DeleteObjectProperty(Isolate* isolate, Handle<JSReceiver> receiver,
                       Handle<Object> key, LanguageMode language_mode);

  // Perform a property store on object. If the key is a private name (i.e. this
  // is a private field assignment), this method throws if the private field
  // does not exist on object.
  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static MaybeHandle<Object>
  SetObjectProperty(Isolate* isolate, Handle<Object> object, Handle<Object> key,
                    Handle<Object> value, StoreOrigin store_origin,
                    Maybe<ShouldThrow> should_throw = Nothing<ShouldThrow>());

  // Defines a property on object. If the key is a private name (i.e. this is a
  // private field definition), this method throws if the field already exists
  // on object.
  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static MaybeHandle<Object>
  DefineObjectOwnProperty(Isolate* isolate, Handle<Object> object,
                          Handle<Object> key, Handle<Object> value,
                          StoreOrigin store_origin);

  // When "receiver" is not passed, it defaults to "lookup_start_object".
  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static MaybeHandle<Object>
  GetObjectProperty(Isolate* isolate, Handle<Object> lookup_start_object,
                    Handle<Object> key,
                    Handle<Object> receiver = Handle<Object>(),
                    bool* is_found = nullptr);

  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> HasProperty(
      Isolate* isolate, Handle<Object> object, Handle<Object> key);

  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static MaybeHandle<JSArray>
  GetInternalProperties(Isolate* isolate, Handle<Object>);

  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> ThrowIteratorError(
      Isolate* isolate, Handle<Object> object);
};

class RuntimeState {
 public:
  RuntimeState(const RuntimeState&) = delete;
  RuntimeState& operator=(const RuntimeState&) = delete;
#ifndef V8_INTL_SUPPORT
  unibrow::Mapping<unibrow::ToUppercase, 128>* to_upper_mapping() {
    return &to_upper_mapping_;
  }
  unibrow::Mapping<unibrow::ToLowercase, 128>* to_lower_mapping() {
    return &to_lower_mapping_;
  }
#endif

  Runtime::Function* redirected_intrinsic_functions() {
    return redirected_intrinsic_functions_.get();
  }

  void set_redirected_intrinsic_functions(
      Runtime::Function* redirected_intrinsic_functions) {
    redirected_intrinsic_functions_.reset(redirected_intrinsic_functions);
  }

 private:
  RuntimeState() = default;
#ifndef V8_INTL_SUPPORT
  unibrow::Mapping<unibrow::ToUppercase, 128> to_upper_mapping_;
  unibrow::Mapping<unibrow::ToLowercase, 128> to_lower_mapping_;
#endif

  std::unique_ptr<Runtime::Function[]> redirected_intrinsic_functions_;

  friend class Isolate;
  friend class Runtime;
};

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream&, Runtime::FunctionId);

//---------------------------------------------------------------------------
// Constants used by interface to runtime functions.

using AllocateDoubleAlignFlag = base::BitField<bool, 0, 1>;

using AllowLargeObjectAllocationFlag = base::BitField<bool, 1, 1>;

// A set of bits returned by Runtime_GetOptimizationStatus.
// These bits must be in sync with bits defined in test/mjsunit/mjsunit.js
enum class OptimizationStatus {
  kIsFunction = 1 << 0,
  kNeverOptimize = 1 << 1,
  kAlwaysOptimize = 1 << 2,
  kMaybeDeopted = 1 << 3,
  kOptimized = 1 << 4,
  kMaglevved = 1 << 5,
  kTurboFanned = 1 << 6,
  kInterpreted = 1 << 7,
  kMarkedForOptimization = 1 << 8,
  kMarkedForConcurrentOptimization = 1 << 9,
  kOptimizingConcurrently = 1 << 10,
  kIsExecuting = 1 << 11,
  kTopmostFrameIsTurboFanned = 1 << 12,
  kLiteMode = 1 << 13,
  kMarkedForDeoptimization = 1 << 14,
  kBaseline = 1 << 15,
  kTopmostFrameIsInterpreted = 1 << 16,
  kTopmostFrameIsBaseline = 1 << 17,
};

}  // namespace internal
}  // namespace v8

#endif  // V8_RUNTIME_RUNTIME_H_
