// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-promise-gen.h"

#include "src/builtins/builtins-constructor-gen.h"
#include "src/builtins/builtins-iterator-gen.h"
#include "src/builtins/builtins-promise.h"
#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/codegen/code-factory.h"
#include "src/codegen/code-stub-assembler.h"
#include "src/objects/fixed-array.h"
#include "src/objects/js-objects.h"
#include "src/objects/js-promise.h"
#include "src/objects/objects-inl.h"
#include "src/objects/smi.h"

namespace v8 {
namespace internal {

using Node = compiler::Node;
using IteratorRecord = TorqueStructIteratorRecord;
using PromiseResolvingFunctions = TorqueStructPromiseResolvingFunctions;

TNode<JSPromise> PromiseBuiltinsAssembler::AllocateJSPromise(
    TNode<Context> context) {
  const TNode<NativeContext> native_context = LoadNativeContext(context);
  const TNode<JSFunction> promise_fun =
      CAST(LoadContextElement(native_context, Context::PROMISE_FUNCTION_INDEX));
  CSA_ASSERT(this, IsFunctionWithPrototypeSlotMap(LoadMap(promise_fun)));
  const TNode<Object> promise_map =
      LoadObjectField(promise_fun, JSFunction::kPrototypeOrInitialMapOffset);
  const TNode<HeapObject> promise =
      Allocate(JSPromise::kSizeWithEmbedderFields);
  StoreMapNoWriteBarrier(promise, promise_map);
  StoreObjectFieldRoot(promise, JSPromise::kPropertiesOrHashOffset,
                       RootIndex::kEmptyFixedArray);
  StoreObjectFieldRoot(promise, JSPromise::kElementsOffset,
                       RootIndex::kEmptyFixedArray);
  return CAST(promise);
}

void PromiseBuiltinsAssembler::PromiseInit(Node* promise) {
  STATIC_ASSERT(v8::Promise::kPending == 0);
  StoreObjectFieldNoWriteBarrier(promise, JSPromise::kReactionsOrResultOffset,
                                 SmiConstant(Smi::zero()));
  StoreObjectFieldNoWriteBarrier(promise, JSPromise::kFlagsOffset,
                                 SmiConstant(Smi::zero()));
  for (int offset = JSPromise::kHeaderSize;
       offset < JSPromise::kSizeWithEmbedderFields; offset += kTaggedSize) {
    StoreObjectFieldNoWriteBarrier(promise, offset, SmiConstant(Smi::zero()));
  }
}

TNode<JSPromise> PromiseBuiltinsAssembler::AllocateAndInitJSPromise(
    TNode<Context> context) {
  return AllocateAndInitJSPromise(context, UndefinedConstant());
}

TNode<JSPromise> PromiseBuiltinsAssembler::AllocateAndInitJSPromise(
    TNode<Context> context, TNode<Object> parent) {
  const TNode<JSPromise> instance = AllocateJSPromise(context);
  PromiseInit(instance);

  Label out(this);
  GotoIfNot(IsPromiseHookEnabledOrHasAsyncEventDelegate(), &out);
  CallRuntime(Runtime::kPromiseHookInit, context, instance, parent);
  Goto(&out);

  BIND(&out);
  return instance;
}

TNode<JSPromise> PromiseBuiltinsAssembler::AllocateAndSetJSPromise(
    TNode<Context> context, v8::Promise::PromiseState status,
    TNode<Object> result) {
  DCHECK_NE(Promise::kPending, status);

  const TNode<JSPromise> instance = AllocateJSPromise(context);
  StoreObjectFieldNoWriteBarrier(instance, JSPromise::kReactionsOrResultOffset,
                                 result);
  STATIC_ASSERT(JSPromise::kStatusShift == 0);
  StoreObjectFieldNoWriteBarrier(instance, JSPromise::kFlagsOffset,
                                 SmiConstant(status));
  for (int offset = JSPromise::kHeaderSize;
       offset < JSPromise::kSizeWithEmbedderFields; offset += kTaggedSize) {
    StoreObjectFieldNoWriteBarrier(instance, offset, SmiConstant(0));
  }

  Label out(this);
  GotoIfNot(IsPromiseHookEnabledOrHasAsyncEventDelegate(), &out);
  CallRuntime(Runtime::kPromiseHookInit, context, instance,
              UndefinedConstant());
  Goto(&out);

  BIND(&out);
  return instance;
}

Node* PromiseBuiltinsAssembler::CreatePromiseAllResolveElementContext(
    Node* promise_capability, Node* native_context) {
  CSA_ASSERT(this, IsNativeContext(native_context));

  // TODO(bmeurer): Manually fold this into a single allocation.
  TNode<Map> array_map = CAST(LoadContextElement(
      native_context, Context::JS_ARRAY_PACKED_ELEMENTS_MAP_INDEX));
  TNode<JSArray> values_array = AllocateJSArray(
      PACKED_ELEMENTS, array_map, IntPtrConstant(0), SmiConstant(0));

  const TNode<Context> context = AllocateSyntheticFunctionContext(
      CAST(native_context), PromiseBuiltins::kPromiseAllResolveElementLength);
  StoreContextElementNoWriteBarrier(
      context, PromiseBuiltins::kPromiseAllResolveElementRemainingSlot,
      SmiConstant(1));
  StoreContextElementNoWriteBarrier(
      context, PromiseBuiltins::kPromiseAllResolveElementCapabilitySlot,
      promise_capability);
  StoreContextElementNoWriteBarrier(
      context, PromiseBuiltins::kPromiseAllResolveElementValuesArraySlot,
      values_array);

  return context;
}

TNode<JSFunction>
PromiseBuiltinsAssembler::CreatePromiseAllResolveElementFunction(
    Node* context, TNode<Smi> index, Node* native_context, int slot_index) {
  CSA_ASSERT(this, SmiGreaterThan(index, SmiConstant(0)));
  CSA_ASSERT(this, SmiLessThanOrEqual(
                       index, SmiConstant(PropertyArray::HashField::kMax)));
  CSA_ASSERT(this, IsNativeContext(native_context));

  const TNode<Map> map = CAST(LoadContextElement(
      native_context, Context::STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX));
  const TNode<SharedFunctionInfo> resolve_info =
      CAST(LoadContextElement(native_context, slot_index));
  TNode<JSFunction> resolve =
      AllocateFunctionWithMapAndContext(map, resolve_info, CAST(context));

  STATIC_ASSERT(PropertyArray::kNoHashSentinel == 0);
  StoreObjectFieldNoWriteBarrier(resolve, JSFunction::kPropertiesOrHashOffset,
                                 index);

  return resolve;
}

TNode<Context> PromiseBuiltinsAssembler::CreatePromiseResolvingFunctionsContext(
    TNode<JSPromise> promise, TNode<Object> debug_event,
    TNode<NativeContext> native_context) {
  const TNode<Context> context = AllocateSyntheticFunctionContext(
      native_context, PromiseBuiltins::kPromiseContextLength);
  StoreContextElementNoWriteBarrier(context, PromiseBuiltins::kPromiseSlot,
                                    promise);
  StoreContextElementNoWriteBarrier(
      context, PromiseBuiltins::kAlreadyResolvedSlot, FalseConstant());
  StoreContextElementNoWriteBarrier(context, PromiseBuiltins::kDebugEventSlot,
                                    debug_event);
  return context;
}

Node* PromiseBuiltinsAssembler::PromiseHasHandler(Node* promise) {
  const TNode<Smi> flags =
      CAST(LoadObjectField(promise, JSPromise::kFlagsOffset));
  return IsSetWord(SmiUntag(flags), 1 << JSPromise::kHasHandlerBit);
}

TNode<PromiseReaction> PromiseBuiltinsAssembler::AllocatePromiseReaction(
    TNode<Object> next, TNode<HeapObject> promise_or_capability,
    TNode<HeapObject> fulfill_handler, TNode<HeapObject> reject_handler) {
  const TNode<HeapObject> reaction = Allocate(PromiseReaction::kSize);
  StoreMapNoWriteBarrier(reaction, RootIndex::kPromiseReactionMap);
  StoreObjectFieldNoWriteBarrier(reaction, PromiseReaction::kNextOffset, next);
  StoreObjectFieldNoWriteBarrier(reaction,
                                 PromiseReaction::kPromiseOrCapabilityOffset,
                                 promise_or_capability);
  StoreObjectFieldNoWriteBarrier(
      reaction, PromiseReaction::kFulfillHandlerOffset, fulfill_handler);
  StoreObjectFieldNoWriteBarrier(
      reaction, PromiseReaction::kRejectHandlerOffset, reject_handler);
  return CAST(reaction);
}

TNode<PromiseReactionJobTask>
PromiseBuiltinsAssembler::AllocatePromiseReactionJobTask(
    TNode<Map> map, TNode<Context> context, TNode<Object> argument,
    TNode<HeapObject> handler, TNode<HeapObject> promise_or_capability) {
  const TNode<HeapObject> microtask =
      Allocate(PromiseReactionJobTask::kSizeOfAllPromiseReactionJobTasks);
  StoreMapNoWriteBarrier(microtask, map);
  StoreObjectFieldNoWriteBarrier(
      microtask, PromiseReactionJobTask::kArgumentOffset, argument);
  StoreObjectFieldNoWriteBarrier(
      microtask, PromiseReactionJobTask::kContextOffset, context);
  StoreObjectFieldNoWriteBarrier(
      microtask, PromiseReactionJobTask::kHandlerOffset, handler);
  StoreObjectFieldNoWriteBarrier(
      microtask, PromiseReactionJobTask::kPromiseOrCapabilityOffset,
      promise_or_capability);
  return CAST(microtask);
}

TNode<PromiseResolveThenableJobTask>
PromiseBuiltinsAssembler::AllocatePromiseResolveThenableJobTask(
    TNode<JSPromise> promise_to_resolve, TNode<JSReceiver> then,
    TNode<JSReceiver> thenable, TNode<Context> context) {
  const TNode<HeapObject> microtask =
      Allocate(PromiseResolveThenableJobTask::kSize);
  StoreMapNoWriteBarrier(microtask,
                         RootIndex::kPromiseResolveThenableJobTaskMap);
  StoreObjectFieldNoWriteBarrier(
      microtask, PromiseResolveThenableJobTask::kContextOffset, context);
  StoreObjectFieldNoWriteBarrier(
      microtask, PromiseResolveThenableJobTask::kPromiseToResolveOffset,
      promise_to_resolve);
  StoreObjectFieldNoWriteBarrier(
      microtask, PromiseResolveThenableJobTask::kThenOffset, then);
  StoreObjectFieldNoWriteBarrier(
      microtask, PromiseResolveThenableJobTask::kThenableOffset, thenable);
  return CAST(microtask);
}

Node* PromiseBuiltinsAssembler::CallResolve(Node* native_context,
                                            Node* constructor, Node* resolve,
                                            Node* value, Label* if_exception,
                                            Variable* var_exception) {
  CSA_ASSERT(this, IsNativeContext(native_context));
  CSA_ASSERT(this, IsConstructor(constructor));
  VARIABLE(var_result, MachineRepresentation::kTagged);
  Label if_fast(this), if_slow(this, Label::kDeferred), done(this, &var_result);

  // Undefined can never be a valid value for the resolve function,
  // instead it is used as a special marker for the fast path.
  Branch(IsUndefined(resolve), &if_fast, &if_slow);

  BIND(&if_fast);
  {
    const TNode<Object> result = CallBuiltin(
        Builtins::kPromiseResolve, native_context, constructor, value);
    GotoIfException(result, if_exception, var_exception);

    var_result.Bind(result);
    Goto(&done);
  }

  BIND(&if_slow);
  {
    CSA_ASSERT(this, IsCallable(resolve));

    Node* const result = CallJS(
        CodeFactory::Call(isolate(), ConvertReceiverMode::kNotNullOrUndefined),
        native_context, resolve, constructor, value);
    GotoIfException(result, if_exception, var_exception);

    var_result.Bind(result);
    Goto(&done);
  }

  BIND(&done);
  return var_result.value();
}

void PromiseBuiltinsAssembler::BranchIfPromiseResolveLookupChainIntact(
    Node* native_context, SloppyTNode<Object> constructor, Label* if_fast,
    Label* if_slow) {
  CSA_ASSERT(this, IsNativeContext(native_context));

  GotoIfForceSlowPath(if_slow);
  TNode<Object> promise_fun =
      LoadContextElement(native_context, Context::PROMISE_FUNCTION_INDEX);
  GotoIfNot(TaggedEqual(promise_fun, constructor), if_slow);
  Branch(IsPromiseResolveProtectorCellInvalid(), if_slow, if_fast);
}

void PromiseBuiltinsAssembler::GotoIfNotPromiseResolveLookupChainIntact(
    Node* native_context, SloppyTNode<Object> constructor, Label* if_slow) {
  Label if_fast(this);
  BranchIfPromiseResolveLookupChainIntact(native_context, constructor, &if_fast,
                                          if_slow);
  BIND(&if_fast);
}

void PromiseBuiltinsAssembler::BranchIfPromiseSpeciesLookupChainIntact(
    TNode<NativeContext> native_context, TNode<Map> promise_map, Label* if_fast,
    Label* if_slow) {
  TNode<Object> promise_prototype =
      LoadContextElement(native_context, Context::PROMISE_PROTOTYPE_INDEX);
  GotoIfForceSlowPath(if_slow);
  GotoIfNot(TaggedEqual(LoadMapPrototype(promise_map), promise_prototype),
            if_slow);
  Branch(IsPromiseSpeciesProtectorCellInvalid(), if_slow, if_fast);
}

void PromiseBuiltinsAssembler::BranchIfPromiseThenLookupChainIntact(
    Node* native_context, Node* receiver_map, Label* if_fast, Label* if_slow) {
  CSA_ASSERT(this, IsMap(receiver_map));
  CSA_ASSERT(this, IsNativeContext(native_context));

  GotoIfForceSlowPath(if_slow);
  GotoIfNot(IsJSPromiseMap(receiver_map), if_slow);
  const TNode<Object> promise_prototype =
      LoadContextElement(native_context, Context::PROMISE_PROTOTYPE_INDEX);
  GotoIfNot(TaggedEqual(LoadMapPrototype(receiver_map), promise_prototype),
            if_slow);
  Branch(IsPromiseThenProtectorCellInvalid(), if_slow, if_fast);
}

void PromiseBuiltinsAssembler::BranchIfAccessCheckFailed(
    SloppyTNode<Context> context, SloppyTNode<Context> native_context,
    TNode<Object> promise_constructor, TNode<Object> executor,
    Label* if_noaccess) {
  VARIABLE(var_executor, MachineRepresentation::kTagged);
  var_executor.Bind(executor);
  Label has_access(this), call_runtime(this, Label::kDeferred);

  // If executor is a bound function, load the bound function until we've
  // reached an actual function.
  Label found_function(this), loop_over_bound_function(this, &var_executor);
  Goto(&loop_over_bound_function);
  BIND(&loop_over_bound_function);
  {
    TNode<Uint16T> executor_type = LoadInstanceType(var_executor.value());
    GotoIf(InstanceTypeEqual(executor_type, JS_FUNCTION_TYPE), &found_function);
    GotoIfNot(InstanceTypeEqual(executor_type, JS_BOUND_FUNCTION_TYPE),
              &call_runtime);
    var_executor.Bind(LoadObjectField(
        var_executor.value(), JSBoundFunction::kBoundTargetFunctionOffset));
    Goto(&loop_over_bound_function);
  }

  // Load the context from the function and compare it to the Promise
  // constructor's context. If they match, everything is fine, otherwise, bail
  // out to the runtime.
  BIND(&found_function);
  {
    TNode<Context> function_context =
        CAST(LoadObjectField(var_executor.value(), JSFunction::kContextOffset));
    TNode<NativeContext> native_function_context =
        LoadNativeContext(function_context);
    Branch(TaggedEqual(native_context, native_function_context), &has_access,
           &call_runtime);
  }

  BIND(&call_runtime);
  {
    Branch(TaggedEqual(CallRuntime(Runtime::kAllowDynamicFunction, context,
                                   promise_constructor),
                       TrueConstant()),
           &has_access, if_noaccess);
  }

  BIND(&has_access);
}

void PromiseBuiltinsAssembler::SetForwardingHandlerIfTrue(
    Node* context, Node* condition, const NodeGenerator& object) {
  Label done(this);
  GotoIfNot(condition, &done);
  SetPropertyStrict(
      CAST(context), CAST(object()),
      HeapConstant(factory()->promise_forwarding_handler_symbol()),
      TrueConstant());
  Goto(&done);
  BIND(&done);
}

void PromiseBuiltinsAssembler::SetPromiseHandledByIfTrue(
    Node* context, Node* condition, Node* promise,
    const NodeGenerator& handled_by) {
  Label done(this);
  GotoIfNot(condition, &done);
  GotoIf(TaggedIsSmi(promise), &done);
  GotoIfNot(HasInstanceType(promise, JS_PROMISE_TYPE), &done);
  SetPropertyStrict(CAST(context), CAST(promise),
                    HeapConstant(factory()->promise_handled_by_symbol()),
                    CAST(handled_by()));
  Goto(&done);
  BIND(&done);
}

TF_BUILTIN(PromiseConstructorLazyDeoptContinuation, PromiseBuiltinsAssembler) {
  TNode<Object> promise = CAST(Parameter(Descriptor::kPromise));
  Node* reject = Parameter(Descriptor::kReject);
  Node* exception = Parameter(Descriptor::kException);
  Node* const context = Parameter(Descriptor::kContext);

  Label finally(this);

  GotoIf(IsTheHole(exception), &finally);
  CallJS(CodeFactory::Call(isolate(), ConvertReceiverMode::kNotNullOrUndefined),
         context, reject, UndefinedConstant(), exception);
  Goto(&finally);

  BIND(&finally);
  Return(promise);
}

TNode<Object> PromiseBuiltinsAssembler::PerformPromiseAll(
    Node* context, Node* constructor, Node* capability,
    const IteratorRecord& iterator,
    const PromiseAllResolvingElementFunction& create_resolve_element_function,
    const PromiseAllResolvingElementFunction& create_reject_element_function,
    Label* if_exception, TVariable<Object>* var_exception) {
  IteratorBuiltinsAssembler iter_assembler(state());

  TNode<NativeContext> native_context = LoadNativeContext(context);

  // For catch prediction, don't treat the .then calls as handling it;
  // instead, recurse outwards.
  SetForwardingHandlerIfTrue(
      native_context, IsDebugActive(),
      LoadObjectField(capability, PromiseCapability::kRejectOffset));

  TNode<Context> resolve_element_context =
      Cast(CreatePromiseAllResolveElementContext(capability, native_context));

  TVARIABLE(Smi, var_index, SmiConstant(1));
  Label loop(this, &var_index), done_loop(this),
      too_many_elements(this, Label::kDeferred),
      close_iterator(this, Label::kDeferred), if_slow(this, Label::kDeferred);

  // We can skip the "resolve" lookup on {constructor} if it's the
  // Promise constructor and the Promise.resolve protector is intact,
  // as that guards the lookup path for the "resolve" property on the
  // Promise constructor.
  TVARIABLE(Object, var_promise_resolve_function, UndefinedConstant());
  GotoIfNotPromiseResolveLookupChainIntact(native_context, constructor,
                                           &if_slow);
  Goto(&loop);

  BIND(&if_slow);
  {
    // 5. Let _promiseResolve_ be ? Get(_constructor_, `"resolve"`).
    TNode<Object> resolve =
        GetProperty(native_context, constructor, factory()->resolve_string());
    GotoIfException(resolve, &close_iterator, var_exception);

    // 6. If IsCallable(_promiseResolve_) is *false*, throw a *TypeError*
    // exception.
    ThrowIfNotCallable(CAST(context), resolve, "resolve");

    var_promise_resolve_function = resolve;
    Goto(&loop);
  }

  BIND(&loop);
  {
    // Let next be IteratorStep(iteratorRecord.[[Iterator]]).
    // If next is an abrupt completion, set iteratorRecord.[[Done]] to true.
    // ReturnIfAbrupt(next).
    const TNode<Map> fast_iterator_result_map = CAST(
        LoadContextElement(native_context, Context::ITERATOR_RESULT_MAP_INDEX));
    const TNode<JSReceiver> next = iter_assembler.IteratorStep(
        native_context, iterator, &done_loop, fast_iterator_result_map,
        if_exception, var_exception);

    // Let nextValue be IteratorValue(next).
    // If nextValue is an abrupt completion, set iteratorRecord.[[Done]] to
    //     true.
    // ReturnIfAbrupt(nextValue).
    const TNode<Object> next_value = iter_assembler.IteratorValue(
        native_context, next, fast_iterator_result_map, if_exception,
        var_exception);

    // Check if we reached the limit.
    const TNode<Smi> index = var_index.value();
    GotoIf(SmiEqual(index, SmiConstant(PropertyArray::HashField::kMax)),
           &too_many_elements);

    // Set index to index + 1.
    var_index = SmiAdd(index, SmiConstant(1));

    // Set remainingElementsCount.[[Value]] to
    //     remainingElementsCount.[[Value]] + 1.
    const TNode<Smi> remaining_elements_count = CAST(LoadContextElement(
        resolve_element_context,
        PromiseBuiltins::kPromiseAllResolveElementRemainingSlot));
    StoreContextElementNoWriteBarrier(
        resolve_element_context,
        PromiseBuiltins::kPromiseAllResolveElementRemainingSlot,
        SmiAdd(remaining_elements_count, SmiConstant(1)));

    // Let resolveElement be CreateBuiltinFunction(steps,
    //                                             « [[AlreadyCalled]],
    //                                               [[Index]],
    //                                               [[Values]],
    //                                               [[Capability]],
    //                                               [[RemainingElements]] »).
    // Set resolveElement.[[AlreadyCalled]] to a Record { [[Value]]: false }.
    // Set resolveElement.[[Index]] to index.
    // Set resolveElement.[[Values]] to values.
    // Set resolveElement.[[Capability]] to resultCapability.
    // Set resolveElement.[[RemainingElements]] to remainingElementsCount.
    const TNode<Object> resolve_element_fun = create_resolve_element_function(
        resolve_element_context, index, native_context, Cast(capability));
    const TNode<Object> reject_element_fun = create_reject_element_function(
        resolve_element_context, index, native_context, Cast(capability));

    // We can skip the "resolve" lookup on the {constructor} as well as the
    // "then" lookup on the result of the "resolve" call, and immediately
    // chain continuation onto the {next_value} if:
    //
    //   (a) The {constructor} is the intrinsic %Promise% function, and
    //       looking up "resolve" on {constructor} yields the initial
    //       Promise.resolve() builtin, and
    //   (b) the promise @@species protector cell is valid, meaning that
    //       no one messed with the Symbol.species property on any
    //       intrinsic promise or on the Promise.prototype, and
    //   (c) the {next_value} is a JSPromise whose [[Prototype]] field
    //       contains the intrinsic %PromisePrototype%, and
    //   (d) we're not running with async_hooks or DevTools enabled.
    //
    // In that case we also don't need to allocate a chained promise for
    // the PromiseReaction (aka we can pass undefined to PerformPromiseThen),
    // since this is only necessary for DevTools and PromiseHooks.
    Label if_fast(this), if_slow(this);
    GotoIfNot(IsUndefined(var_promise_resolve_function.value()), &if_slow);
    GotoIf(IsPromiseHookEnabledOrDebugIsActiveOrHasAsyncEventDelegate(),
           &if_slow);
    GotoIf(IsPromiseSpeciesProtectorCellInvalid(), &if_slow);
    GotoIf(TaggedIsSmi(next_value), &if_slow);
    const TNode<Map> next_value_map = LoadMap(CAST(next_value));
    BranchIfPromiseThenLookupChainIntact(native_context, next_value_map,
                                         &if_fast, &if_slow);

    BIND(&if_fast);
    {
      // Register the PromiseReaction immediately on the {next_value}, not
      // passing any chained promise since neither async_hooks nor DevTools
      // are enabled, so there's no use of the resulting promise.
      PerformPromiseThenImpl(native_context, CAST(next_value),
                             CAST(resolve_element_fun),
                             CAST(reject_element_fun), UndefinedConstant());
      Goto(&loop);
    }

    BIND(&if_slow);
    {
      // Let nextPromise be ? Call(constructor, _promiseResolve_, « nextValue
      // »).
      Node* const next_promise = CallResolve(
          native_context, constructor, var_promise_resolve_function.value(),
          next_value, &close_iterator, var_exception);

      // Perform ? Invoke(nextPromise, "then", « resolveElement,
      //                  resultCapability.[[Reject]] »).
      const TNode<Object> then =
          GetProperty(native_context, next_promise, factory()->then_string());
      GotoIfException(then, &close_iterator, var_exception);

      Node* const then_call =
          CallJS(CodeFactory::Call(isolate(),
                                   ConvertReceiverMode::kNotNullOrUndefined),
                 native_context, then, next_promise, resolve_element_fun,
                 reject_element_fun);
      GotoIfException(then_call, &close_iterator, var_exception);

      // For catch prediction, mark that rejections here are semantically
      // handled by the combined Promise.
      SetPromiseHandledByIfTrue(
          native_context, IsDebugActive(), then_call, [=]() {
            // Load promiseCapability.[[Promise]]
            return LoadObjectField(capability,
                                   PromiseCapability::kPromiseOffset);
          });

      Goto(&loop);
    }
  }

  BIND(&too_many_elements);
  {
    // If there are too many elements (currently more than 2**21-1), raise a
    // RangeError here (which is caught directly and turned into a rejection)
    // of the resulting promise. We could gracefully handle this case as well
    // and support more than this number of elements by going to a separate
    // function and pass the larger indices via a separate context, but it
    // doesn't seem likely that we need this, and it's unclear how the rest
    // of the system deals with 2**21 live Promises anyways.
    const TNode<Object> result =
        CallRuntime(Runtime::kThrowRangeError, native_context,
                    SmiConstant(MessageTemplate::kTooManyElementsInPromiseAll));
    GotoIfException(result, &close_iterator, var_exception);
    Unreachable();
  }

  BIND(&close_iterator);
  {
    // Exception must be bound to a JS value.
    CSA_ASSERT(this, IsNotTheHole(var_exception->value()));
    iter_assembler.IteratorCloseOnException(native_context, iterator,
                                            if_exception, var_exception);
  }

  BIND(&done_loop);
  {
    Label resolve_promise(this, Label::kDeferred), return_promise(this);
    // Set iteratorRecord.[[Done]] to true.
    // Set remainingElementsCount.[[Value]] to
    //    remainingElementsCount.[[Value]] - 1.
    TNode<Smi> remaining_elements_count = CAST(LoadContextElement(
        resolve_element_context,
        PromiseBuiltins::kPromiseAllResolveElementRemainingSlot));
    remaining_elements_count = SmiSub(remaining_elements_count, SmiConstant(1));
    StoreContextElementNoWriteBarrier(
        resolve_element_context,
        PromiseBuiltins::kPromiseAllResolveElementRemainingSlot,
        remaining_elements_count);
    GotoIf(SmiEqual(remaining_elements_count, SmiConstant(0)),
           &resolve_promise);

    // Pre-allocate the backing store for the {values_array} to the desired
    // capacity here. We may already have elements here in case of some
    // fancy Thenable that calls the resolve callback immediately, so we need
    // to handle that correctly here.
    const TNode<JSArray> values_array = CAST(LoadContextElement(
        resolve_element_context,
        PromiseBuiltins::kPromiseAllResolveElementValuesArraySlot));
    const TNode<FixedArrayBase> old_elements = LoadElements(values_array);
    const TNode<Smi> old_capacity = LoadFixedArrayBaseLength(old_elements);
    const TNode<Smi> new_capacity = var_index.value();
    GotoIf(SmiGreaterThanOrEqual(old_capacity, new_capacity), &return_promise);
    const TNode<FixedArrayBase> new_elements =
        AllocateFixedArray(PACKED_ELEMENTS, new_capacity,
                           AllocationFlag::kAllowLargeObjectAllocation);
    CopyFixedArrayElements(PACKED_ELEMENTS, old_elements, PACKED_ELEMENTS,
                           new_elements, SmiConstant(0), old_capacity,
                           new_capacity, UPDATE_WRITE_BARRIER, SMI_PARAMETERS);
    StoreObjectField(values_array, JSArray::kElementsOffset, new_elements);
    Goto(&return_promise);

    // If remainingElementsCount.[[Value]] is 0, then
    //     Let valuesArray be CreateArrayFromList(values).
    //     Perform ? Call(resultCapability.[[Resolve]], undefined,
    //                    « valuesArray »).
    BIND(&resolve_promise);
    {
      const TNode<Object> resolve =
          LoadObjectField(capability, PromiseCapability::kResolveOffset);
      const TNode<Object> values_array = LoadContextElement(
          resolve_element_context,
          PromiseBuiltins::kPromiseAllResolveElementValuesArraySlot);
      Node* const resolve_call = CallJS(
          CodeFactory::Call(isolate(), ConvertReceiverMode::kNullOrUndefined),
          native_context, resolve, UndefinedConstant(), values_array);
      GotoIfException(resolve_call, if_exception, var_exception);
      Goto(&return_promise);
    }

    // Return resultCapability.[[Promise]].
    BIND(&return_promise);
  }

  const TNode<Object> promise =
      LoadObjectField(capability, PromiseCapability::kPromiseOffset);
  return promise;
}

void PromiseBuiltinsAssembler::Generate_PromiseAll(
    TNode<Context> context, TNode<Object> receiver, TNode<Object> iterable,
    const PromiseAllResolvingElementFunction& create_resolve_element_function,
    const PromiseAllResolvingElementFunction& create_reject_element_function) {
  IteratorBuiltinsAssembler iter_assembler(state());

  // Let C be the this value.
  // If Type(C) is not Object, throw a TypeError exception.
  ThrowIfNotJSReceiver(context, receiver, MessageTemplate::kCalledOnNonObject,
                       "Promise.all");

  // Let promiseCapability be ? NewPromiseCapability(C).
  // Don't fire debugEvent so that forwarding the rejection through all does not
  // trigger redundant ExceptionEvents
  const TNode<Oddball> debug_event = FalseConstant();
  const TNode<PromiseCapability> capability = CAST(CallBuiltin(
      Builtins::kNewPromiseCapability, context, receiver, debug_event));

  TVARIABLE(Object, var_exception, TheHoleConstant());
  Label reject_promise(this, &var_exception, Label::kDeferred);

  // Let iterator be GetIterator(iterable).
  // IfAbruptRejectPromise(iterator, promiseCapability).
  IteratorRecord iterator = iter_assembler.GetIterator(
      context, iterable, &reject_promise, &var_exception);

  // Let result be PerformPromiseAll(iteratorRecord, C, promiseCapability).
  // If result is an abrupt completion, then
  //   If iteratorRecord.[[Done]] is false, let result be
  //       IteratorClose(iterator, result).
  //    IfAbruptRejectPromise(result, promiseCapability).
  const TNode<Object> result = PerformPromiseAll(
      context, receiver, capability, iterator, create_resolve_element_function,
      create_reject_element_function, &reject_promise, &var_exception);

  Return(result);

  BIND(&reject_promise);
  {
    // Exception must be bound to a JS value.
    CSA_SLOW_ASSERT(this, IsNotTheHole(var_exception.value()));
    const TNode<Object> reject =
        LoadObjectField(capability, PromiseCapability::kRejectOffset);
    CallJS(CodeFactory::Call(isolate(), ConvertReceiverMode::kNullOrUndefined),
           context, reject, UndefinedConstant(), var_exception.value());

    const TNode<Object> promise =
        LoadObjectField(capability, PromiseCapability::kPromiseOffset);
    Return(promise);
  }
}

// ES#sec-promise.all
// Promise.all ( iterable )
TF_BUILTIN(PromiseAll, PromiseBuiltinsAssembler) {
  TNode<Object> receiver = Cast(Parameter(Descriptor::kReceiver));
  TNode<Context> context = Cast(Parameter(Descriptor::kContext));
  TNode<Object> iterable = Cast(Parameter(Descriptor::kIterable));
  Generate_PromiseAll(
      context, receiver, iterable,
      [this](TNode<Context> context, TNode<Smi> index,
             TNode<NativeContext> native_context,
             TNode<PromiseCapability> capability) {
        return CreatePromiseAllResolveElementFunction(
            context, index, native_context,
            Context::PROMISE_ALL_RESOLVE_ELEMENT_SHARED_FUN);
      },
      [this](TNode<Context> context, TNode<Smi> index,
             TNode<NativeContext> native_context,
             TNode<PromiseCapability> capability) {
        return LoadObjectField(capability, PromiseCapability::kRejectOffset);
      });
}

// ES#sec-promise.allsettled
// Promise.allSettled ( iterable )
TF_BUILTIN(PromiseAllSettled, PromiseBuiltinsAssembler) {
  TNode<Object> receiver = Cast(Parameter(Descriptor::kReceiver));
  TNode<Context> context = Cast(Parameter(Descriptor::kContext));
  TNode<Object> iterable = Cast(Parameter(Descriptor::kIterable));
  Generate_PromiseAll(
      context, receiver, iterable,
      [this](TNode<Context> context, TNode<Smi> index,
             TNode<NativeContext> native_context,
             TNode<PromiseCapability> capability) {
        return CreatePromiseAllResolveElementFunction(
            context, index, native_context,
            Context::PROMISE_ALL_SETTLED_RESOLVE_ELEMENT_SHARED_FUN);
      },
      [this](TNode<Context> context, TNode<Smi> index,
             TNode<NativeContext> native_context,
             TNode<PromiseCapability> capability) {
        return CreatePromiseAllResolveElementFunction(
            context, index, native_context,
            Context::PROMISE_ALL_SETTLED_REJECT_ELEMENT_SHARED_FUN);
      });
}

void PromiseBuiltinsAssembler::Generate_PromiseAllResolveElementClosure(
    TNode<Context> context, TNode<Object> value, TNode<JSFunction> function,
    const CreatePromiseAllResolveElementFunctionValue& callback) {
  Label already_called(this, Label::kDeferred), resolve_promise(this);

  // We use the {function}s context as the marker to remember whether this
  // resolve element closure was already called. It points to the resolve
  // element context (which is a FunctionContext) until it was called the
  // first time, in which case we make it point to the native context here
  // to mark this resolve element closure as done.
  GotoIf(IsNativeContext(context), &already_called);
  CSA_ASSERT(
      this,
      SmiEqual(LoadObjectField<Smi>(context, Context::kLengthOffset),
               SmiConstant(PromiseBuiltins::kPromiseAllResolveElementLength)));
  TNode<NativeContext> native_context = LoadNativeContext(context);
  StoreObjectField(function, JSFunction::kContextOffset, native_context);

  // Update the value depending on whether Promise.all or
  // Promise.allSettled is called.
  value = callback(context, native_context, value);

  // Determine the index from the {function}.
  Label unreachable(this, Label::kDeferred);
  STATIC_ASSERT(PropertyArray::kNoHashSentinel == 0);
  TNode<IntPtrT> identity_hash =
      LoadJSReceiverIdentityHash(function, &unreachable);
  CSA_ASSERT(this, IntPtrGreaterThan(identity_hash, IntPtrConstant(0)));
  TNode<IntPtrT> index = IntPtrSub(identity_hash, IntPtrConstant(1));

  // Check if we need to grow the [[ValuesArray]] to store {value} at {index}.
  TNode<JSArray> values_array = CAST(LoadContextElement(
      context, PromiseBuiltins::kPromiseAllResolveElementValuesArraySlot));
  TNode<FixedArray> elements = CAST(LoadElements(values_array));
  TNode<IntPtrT> values_length =
      LoadAndUntagObjectField(values_array, JSArray::kLengthOffset);
  Label if_inbounds(this), if_outofbounds(this), done(this);
  Branch(IntPtrLessThan(index, values_length), &if_inbounds, &if_outofbounds);

  BIND(&if_outofbounds);
  {
    // Check if we need to grow the backing store.
    TNode<IntPtrT> new_length = IntPtrAdd(index, IntPtrConstant(1));
    TNode<IntPtrT> elements_length =
        LoadAndUntagObjectField(elements, FixedArray::kLengthOffset);
    Label if_grow(this, Label::kDeferred), if_nogrow(this);
    Branch(IntPtrLessThan(index, elements_length), &if_nogrow, &if_grow);

    BIND(&if_grow);
    {
      // We need to grow the backing store to fit the {index} as well.
      TNode<IntPtrT> new_elements_length =
          IntPtrMin(CalculateNewElementsCapacity(new_length),
                    IntPtrConstant(PropertyArray::HashField::kMax + 1));
      CSA_ASSERT(this, IntPtrLessThan(index, new_elements_length));
      CSA_ASSERT(this, IntPtrLessThan(elements_length, new_elements_length));
      TNode<FixedArray> new_elements =
          CAST(AllocateFixedArray(PACKED_ELEMENTS, new_elements_length,
                                  AllocationFlag::kAllowLargeObjectAllocation));
      CopyFixedArrayElements(PACKED_ELEMENTS, elements, PACKED_ELEMENTS,
                             new_elements, elements_length,
                             new_elements_length);
      StoreFixedArrayElement(new_elements, index, value);

      // Update backing store and "length" on {values_array}.
      StoreObjectField(values_array, JSArray::kElementsOffset, new_elements);
      StoreObjectFieldNoWriteBarrier(values_array, JSArray::kLengthOffset,
                                     SmiTag(new_length));
      Goto(&done);
    }

    BIND(&if_nogrow);
    {
      // The {index} is within bounds of the {elements} backing store, so
      // just store the {value} and update the "length" of the {values_array}.
      StoreObjectFieldNoWriteBarrier(values_array, JSArray::kLengthOffset,
                                     SmiTag(new_length));
      StoreFixedArrayElement(elements, index, value);
      Goto(&done);
    }
  }

  BIND(&if_inbounds);
  {
    // The {index} is in bounds of the {values_array},
    // just store the {value} and continue.
    StoreFixedArrayElement(elements, index, value);
    Goto(&done);
  }

  BIND(&done);
  TNode<Smi> remaining_elements_count = CAST(LoadContextElement(
      context, PromiseBuiltins::kPromiseAllResolveElementRemainingSlot));
  remaining_elements_count = SmiSub(remaining_elements_count, SmiConstant(1));
  StoreContextElement(context,
                      PromiseBuiltins::kPromiseAllResolveElementRemainingSlot,
                      remaining_elements_count);
  GotoIf(SmiEqual(remaining_elements_count, SmiConstant(0)), &resolve_promise);
  Return(UndefinedConstant());

  BIND(&resolve_promise);
  TNode<PromiseCapability> capability = CAST(LoadContextElement(
      context, PromiseBuiltins::kPromiseAllResolveElementCapabilitySlot));
  TNode<Object> resolve =
      LoadObjectField(capability, PromiseCapability::kResolveOffset);
  CallJS(CodeFactory::Call(isolate(), ConvertReceiverMode::kNullOrUndefined),
         context, resolve, UndefinedConstant(), values_array);
  Return(UndefinedConstant());

  BIND(&already_called);
  Return(UndefinedConstant());

  BIND(&unreachable);
  Unreachable();
}

TF_BUILTIN(PromiseAllResolveElementClosure, PromiseBuiltinsAssembler) {
  TNode<Object> value = CAST(Parameter(Descriptor::kValue));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<JSFunction> function = CAST(Parameter(Descriptor::kJSTarget));

  Generate_PromiseAllResolveElementClosure(
      context, value, function,
      [](TNode<Object>, TNode<NativeContext>, TNode<Object> value) {
        return value;
      });
}

TF_BUILTIN(PromiseAllSettledResolveElementClosure, PromiseBuiltinsAssembler) {
  TNode<Object> value = CAST(Parameter(Descriptor::kValue));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<JSFunction> function = CAST(Parameter(Descriptor::kJSTarget));

  Generate_PromiseAllResolveElementClosure(
      context, value, function,
      [this](TNode<Context> context, TNode<NativeContext> native_context,
             TNode<Object> value) {
        // TODO(gsathya): Optimize the creation using a cached map to
        // prevent transitions here.
        // 9. Let obj be ! ObjectCreate(%ObjectPrototype%).
        TNode<HeapObject> object_function = Cast(
            LoadContextElement(native_context, Context::OBJECT_FUNCTION_INDEX));
        TNode<Map> object_function_map = Cast(LoadObjectField(
            object_function, JSFunction::kPrototypeOrInitialMapOffset));
        TNode<JSObject> obj = AllocateJSObjectFromMap(object_function_map);

        // 10. Perform ! CreateDataProperty(obj, "status", "fulfilled").
        CallBuiltin(Builtins::kFastCreateDataProperty, context, obj,
                    StringConstant("status"), StringConstant("fulfilled"));

        // 11. Perform ! CreateDataProperty(obj, "value", x).
        CallBuiltin(Builtins::kFastCreateDataProperty, context, obj,
                    StringConstant("value"), value);

        return obj;
      });
}

TF_BUILTIN(PromiseAllSettledRejectElementClosure, PromiseBuiltinsAssembler) {
  TNode<Object> value = CAST(Parameter(Descriptor::kValue));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<JSFunction> function = CAST(Parameter(Descriptor::kJSTarget));

  Generate_PromiseAllResolveElementClosure(
      context, value, function,
      [this](TNode<Context> context, TNode<NativeContext> native_context,
             TNode<Object> value) {
        // TODO(gsathya): Optimize the creation using a cached map to
        // prevent transitions here.
        // 9. Let obj be ! ObjectCreate(%ObjectPrototype%).
        TNode<HeapObject> object_function = Cast(
            LoadContextElement(native_context, Context::OBJECT_FUNCTION_INDEX));
        TNode<Map> object_function_map = Cast(LoadObjectField(
            object_function, JSFunction::kPrototypeOrInitialMapOffset));
        TNode<JSObject> obj = AllocateJSObjectFromMap(object_function_map);

        // 10. Perform ! CreateDataProperty(obj, "status", "rejected").
        CallBuiltin(Builtins::kFastCreateDataProperty, context, obj,
                    StringConstant("status"), StringConstant("rejected"));

        // 11. Perform ! CreateDataProperty(obj, "reason", x).
        CallBuiltin(Builtins::kFastCreateDataProperty, context, obj,
                    StringConstant("reason"), value);

        return obj;
      });
}

}  // namespace internal
}  // namespace v8
