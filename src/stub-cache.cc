// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/api.h"
#include "src/arguments.h"
#include "src/ast.h"
#include "src/code-stubs.h"
#include "src/cpu-profiler.h"
#include "src/gdb-jit.h"
#include "src/ic-inl.h"
#include "src/stub-cache.h"
#include "src/type-info.h"
#include "src/vm-state-inl.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------
// StubCache implementation.


StubCache::StubCache(Isolate* isolate)
    : isolate_(isolate) { }


void StubCache::Initialize() {
  ASSERT(IsPowerOf2(kPrimaryTableSize));
  ASSERT(IsPowerOf2(kSecondaryTableSize));
  Clear();
}


static Code::Flags CommonStubCacheChecks(Name* name, Map* map,
                                         Code::Flags flags, Heap* heap) {
  flags = Code::RemoveTypeAndHolderFromFlags(flags);

  // Validate that the name does not move on scavenge, and that we
  // can use identity checks instead of structural equality checks.
  ASSERT(!heap->InNewSpace(name));
  ASSERT(name->IsUniqueName());

  // The state bits are not important to the hash function because the stub
  // cache only contains handlers. Make sure that the bits are the least
  // significant so they will be the ones masked out.
  ASSERT_EQ(Code::HANDLER, Code::ExtractKindFromFlags(flags));
  STATIC_ASSERT((Code::ICStateField::kMask & 1) == 1);

  // Make sure that the code type and cache holder are not included in the hash.
  ASSERT(Code::ExtractTypeFromFlags(flags) == 0);
  ASSERT(Code::ExtractCacheHolderFromFlags(flags) == 0);

  return flags;
}


Code* StubCache::Set(Name* name, Map* map, Code* code) {
  Code::Flags flags =
      CommonStubCacheChecks(name, map, code->flags(), isolate()->heap());

  // Compute the primary entry.
  int primary_offset = PrimaryOffset(name, flags, map);
  Entry* primary = entry(primary_, primary_offset);
  Code* old_code = primary->value;

  // If the primary entry has useful data in it, we retire it to the
  // secondary cache before overwriting it.
  if (old_code != isolate_->builtins()->builtin(Builtins::kIllegal)) {
    Map* old_map = primary->map;
    Code::Flags old_flags =
        Code::RemoveTypeAndHolderFromFlags(old_code->flags());
    int seed = PrimaryOffset(primary->key, old_flags, old_map);
    int secondary_offset = SecondaryOffset(primary->key, old_flags, seed);
    Entry* secondary = entry(secondary_, secondary_offset);
    *secondary = *primary;
  }

  // Update primary cache.
  primary->key = name;
  primary->value = code;
  primary->map = map;
  isolate()->counters()->megamorphic_stub_cache_updates()->Increment();
  return code;
}


Code* StubCache::Get(Name* name, Map* map, Code::Flags flags) {
  flags = CommonStubCacheChecks(name, map, flags, isolate()->heap());
  int primary_offset = PrimaryOffset(name, flags, map);
  Entry* primary = entry(primary_, primary_offset);
  if (primary->key == name && primary->map == map) {
    return primary->value;
  }
  int secondary_offset = SecondaryOffset(name, flags, primary_offset);
  Entry* secondary = entry(secondary_, secondary_offset);
  if (secondary->key == name && secondary->map == map) {
    return secondary->value;
  }
  return NULL;
}


Handle<Code> PropertyICCompiler::Find(Handle<Name> name,
                                      Handle<Map> stub_holder, Code::Kind kind,
                                      ExtraICState extra_state,
                                      CacheHolderFlag cache_holder) {
  Code::Flags flags = Code::ComputeMonomorphicFlags(
      kind, extra_state, cache_holder);
  Handle<Object> probe(stub_holder->FindInCodeCache(*name, flags),
                       name->GetIsolate());
  if (probe->IsCode()) return Handle<Code>::cast(probe);
  return Handle<Code>::null();
}


Handle<Code> PropertyHandlerCompiler::Find(Handle<Name> name,
                                           Handle<Map> stub_holder,
                                           Code::Kind kind,
                                           CacheHolderFlag cache_holder,
                                           Code::StubType type) {
  Code::Flags flags = Code::ComputeHandlerFlags(kind, type, cache_holder);

  Handle<Object> probe(stub_holder->FindInCodeCache(*name, flags),
                       name->GetIsolate());
  if (probe->IsCode()) return Handle<Code>::cast(probe);
  return Handle<Code>::null();
}


Handle<Code> PropertyICCompiler::ComputeMonomorphic(
    Code::Kind kind, Handle<Name> name, Handle<HeapType> type,
    Handle<Code> handler, ExtraICState extra_ic_state) {
  Isolate* isolate = name->GetIsolate();
  if (handler.is_identical_to(isolate->builtins()->LoadIC_Normal()) ||
      handler.is_identical_to(isolate->builtins()->StoreIC_Normal())) {
    name = isolate->factory()->normal_ic_symbol();
  }

  CacheHolderFlag flag;
  Handle<Map> stub_holder = IC::GetICCacheHolder(*type, isolate, &flag);

  Handle<Code> ic;
  // There are multiple string maps that all use the same prototype. That
  // prototype cannot hold multiple handlers, one for each of the string maps,
  // for a single name. Hence, turn off caching of the IC.
  bool can_be_cached = !type->Is(HeapType::String());
  if (can_be_cached) {
    ic = Find(name, stub_holder, kind, extra_ic_state, flag);
    if (!ic.is_null()) return ic;
  }

#ifdef DEBUG
  if (kind == Code::KEYED_STORE_IC) {
    ASSERT(STANDARD_STORE ==
           KeyedStoreIC::GetKeyedAccessStoreMode(extra_ic_state));
  }
#endif

  PropertyICCompiler ic_compiler(isolate, kind, extra_ic_state, flag);
  ic = ic_compiler.CompileMonomorphic(type, handler, name, PROPERTY);

  if (can_be_cached) Map::UpdateCodeCache(stub_holder, name, ic);
  return ic;
}


Handle<Code> NamedLoadHandlerCompiler::ComputeLoadNonexistent(
    Handle<Name> name, Handle<HeapType> type) {
  Isolate* isolate = name->GetIsolate();
  Handle<Map> receiver_map = IC::TypeToMap(*type, isolate);
  if (receiver_map->prototype()->IsNull()) {
    // TODO(jkummerow/verwaest): If there is no prototype and the property
    // is nonexistent, introduce a builtin to handle this (fast properties
    // -> return undefined, dictionary properties -> do negative lookup).
    return Handle<Code>();
  }
  CacheHolderFlag flag;
  Handle<Map> stub_holder_map =
      IC::GetHandlerCacheHolder(*type, false, isolate, &flag);

  // If no dictionary mode objects are present in the prototype chain, the load
  // nonexistent IC stub can be shared for all names for a given map and we use
  // the empty string for the map cache in that case. If there are dictionary
  // mode objects involved, we need to do negative lookups in the stub and
  // therefore the stub will be specific to the name.
  Handle<Name> cache_name =
      receiver_map->is_dictionary_map()
          ? name
          : Handle<Name>::cast(isolate->factory()->nonexistent_symbol());
  Handle<Map> current_map = stub_holder_map;
  Handle<JSObject> last(JSObject::cast(receiver_map->prototype()));
  while (true) {
    if (current_map->is_dictionary_map()) cache_name = name;
    if (current_map->prototype()->IsNull()) break;
    last = handle(JSObject::cast(current_map->prototype()));
    current_map = handle(last->map());
  }
  // Compile the stub that is either shared for all names or
  // name specific if there are global objects involved.
  Handle<Code> handler = PropertyHandlerCompiler::Find(
      cache_name, stub_holder_map, Code::LOAD_IC, flag, Code::FAST);
  if (!handler.is_null()) return handler;

  NamedLoadHandlerCompiler compiler(isolate, type, last, flag);
  handler = compiler.CompileLoadNonexistent(cache_name);
  Map::UpdateCodeCache(stub_holder_map, cache_name, handler);
  return handler;
}


Handle<Code> PropertyICCompiler::ComputeKeyedLoadMonomorphic(
    Handle<Map> receiver_map) {
  Isolate* isolate = receiver_map->GetIsolate();
  Code::Flags flags = Code::ComputeMonomorphicFlags(Code::KEYED_LOAD_IC);
  Handle<Name> name = isolate->factory()->KeyedLoadMonomorphic_string();

  Handle<Object> probe(receiver_map->FindInCodeCache(*name, flags), isolate);
  if (probe->IsCode()) return Handle<Code>::cast(probe);

  ElementsKind elements_kind = receiver_map->elements_kind();
  Handle<Code> stub;
  if (receiver_map->has_fast_elements() ||
      receiver_map->has_external_array_elements() ||
      receiver_map->has_fixed_typed_array_elements()) {
    stub = LoadFastElementStub(isolate,
                               receiver_map->instance_type() == JS_ARRAY_TYPE,
                               elements_kind).GetCode();
  } else {
    stub = FLAG_compiled_keyed_dictionary_loads
               ? LoadDictionaryElementStub(isolate).GetCode()
               : LoadDictionaryElementPlatformStub(isolate).GetCode();
  }
  PropertyICCompiler compiler(isolate, Code::KEYED_LOAD_IC);
  Handle<Code> code =
      compiler.CompileMonomorphic(HeapType::Class(receiver_map, isolate), stub,
                                  isolate->factory()->empty_string(), ELEMENT);

  Map::UpdateCodeCache(receiver_map, name, code);
  return code;
}


Handle<Code> PropertyICCompiler::ComputeKeyedStoreMonomorphic(
    Handle<Map> receiver_map, StrictMode strict_mode,
    KeyedAccessStoreMode store_mode) {
  Isolate* isolate = receiver_map->GetIsolate();
  ExtraICState extra_state =
      KeyedStoreIC::ComputeExtraICState(strict_mode, store_mode);
  Code::Flags flags =
      Code::ComputeMonomorphicFlags(Code::KEYED_STORE_IC, extra_state);

  ASSERT(store_mode == STANDARD_STORE ||
         store_mode == STORE_AND_GROW_NO_TRANSITION ||
         store_mode == STORE_NO_TRANSITION_IGNORE_OUT_OF_BOUNDS ||
         store_mode == STORE_NO_TRANSITION_HANDLE_COW);

  Handle<String> name = isolate->factory()->KeyedStoreMonomorphic_string();
  Handle<Object> probe(receiver_map->FindInCodeCache(*name, flags), isolate);
  if (probe->IsCode()) return Handle<Code>::cast(probe);

  PropertyICCompiler compiler(isolate, Code::KEYED_STORE_IC, extra_state);
  Handle<Code> code =
      compiler.CompileKeyedStoreMonomorphic(receiver_map, store_mode);

  Map::UpdateCodeCache(receiver_map, name, code);
  ASSERT(KeyedStoreIC::GetKeyedAccessStoreMode(code->extra_ic_state())
         == store_mode);
  return code;
}


#define CALL_LOGGER_TAG(kind, type) (Logger::KEYED_##type)

static void FillCache(Isolate* isolate, Handle<Code> code) {
  Handle<UnseededNumberDictionary> dictionary =
      UnseededNumberDictionary::Set(isolate->factory()->non_monomorphic_cache(),
                                    code->flags(),
                                    code);
  isolate->heap()->public_set_non_monomorphic_cache(*dictionary);
}


Code* PropertyICCompiler::FindPreMonomorphic(Isolate* isolate, Code::Kind kind,
                                             ExtraICState state) {
  Code::Flags flags = Code::ComputeFlags(kind, PREMONOMORPHIC, state);
  UnseededNumberDictionary* dictionary =
      isolate->heap()->non_monomorphic_cache();
  int entry = dictionary->FindEntry(isolate, flags);
  ASSERT(entry != -1);
  Object* code = dictionary->ValueAt(entry);
  // This might be called during the marking phase of the collector
  // hence the unchecked cast.
  return reinterpret_cast<Code*>(code);
}


Handle<Code> PropertyICCompiler::ComputeLoad(Isolate* isolate,
                                             InlineCacheState ic_state,
                                             ExtraICState extra_state) {
  Code::Flags flags = Code::ComputeFlags(Code::LOAD_IC, ic_state, extra_state);
  Handle<UnseededNumberDictionary> cache =
      isolate->factory()->non_monomorphic_cache();
  int entry = cache->FindEntry(isolate, flags);
  if (entry != -1) return Handle<Code>(Code::cast(cache->ValueAt(entry)));

  PropertyICCompiler compiler(isolate, Code::LOAD_IC);
  Handle<Code> code;
  if (ic_state == UNINITIALIZED) {
    code = compiler.CompileLoadInitialize(flags);
  } else if (ic_state == PREMONOMORPHIC) {
    code = compiler.CompileLoadPreMonomorphic(flags);
  } else if (ic_state == MEGAMORPHIC) {
    code = compiler.CompileLoadMegamorphic(flags);
  } else {
    UNREACHABLE();
  }
  FillCache(isolate, code);
  return code;
}


Handle<Code> PropertyICCompiler::ComputeStore(Isolate* isolate,
                                              InlineCacheState ic_state,
                                              ExtraICState extra_state) {
  Code::Flags flags = Code::ComputeFlags(Code::STORE_IC, ic_state, extra_state);
  Handle<UnseededNumberDictionary> cache =
      isolate->factory()->non_monomorphic_cache();
  int entry = cache->FindEntry(isolate, flags);
  if (entry != -1) return Handle<Code>(Code::cast(cache->ValueAt(entry)));

  PropertyICCompiler compiler(isolate, Code::STORE_IC);
  Handle<Code> code;
  if (ic_state == UNINITIALIZED) {
    code = compiler.CompileStoreInitialize(flags);
  } else if (ic_state == PREMONOMORPHIC) {
    code = compiler.CompileStorePreMonomorphic(flags);
  } else if (ic_state == GENERIC) {
    code = compiler.CompileStoreGeneric(flags);
  } else if (ic_state == MEGAMORPHIC) {
    code = compiler.CompileStoreMegamorphic(flags);
  } else {
    UNREACHABLE();
  }

  FillCache(isolate, code);
  return code;
}


Handle<Code> PropertyICCompiler::ComputeCompareNil(Handle<Map> receiver_map,
                                                   CompareNilICStub* stub) {
  Isolate* isolate = receiver_map->GetIsolate();
  Handle<String> name(isolate->heap()->empty_string());
  if (!receiver_map->is_shared()) {
    Handle<Code> cached_ic =
        Find(name, receiver_map, Code::COMPARE_NIL_IC, stub->GetExtraICState());
    if (!cached_ic.is_null()) return cached_ic;
  }

  Code::FindAndReplacePattern pattern;
  pattern.Add(isolate->factory()->meta_map(), receiver_map);
  Handle<Code> ic = stub->GetCodeCopy(pattern);

  if (!receiver_map->is_shared()) {
    Map::UpdateCodeCache(receiver_map, name, ic);
  }

  return ic;
}


// TODO(verwaest): Change this method so it takes in a TypeHandleList.
Handle<Code> PropertyICCompiler::ComputeKeyedLoadPolymorphic(
    MapHandleList* receiver_maps) {
  Isolate* isolate = receiver_maps->at(0)->GetIsolate();
  Code::Flags flags = Code::ComputeFlags(Code::KEYED_LOAD_IC, POLYMORPHIC);
  Handle<PolymorphicCodeCache> cache =
      isolate->factory()->polymorphic_code_cache();
  Handle<Object> probe = cache->Lookup(receiver_maps, flags);
  if (probe->IsCode()) return Handle<Code>::cast(probe);

  TypeHandleList types(receiver_maps->length());
  for (int i = 0; i < receiver_maps->length(); i++) {
    types.Add(HeapType::Class(receiver_maps->at(i), isolate));
  }
  CodeHandleList handlers(receiver_maps->length());
  ElementHandlerCompiler compiler(isolate);
  compiler.CompileElementHandlers(receiver_maps, &handlers);
  PropertyICCompiler ic_compiler(isolate, Code::KEYED_LOAD_IC);
  Handle<Code> code = ic_compiler.CompilePolymorphic(
      &types, &handlers, isolate->factory()->empty_string(), Code::NORMAL,
      ELEMENT);

  isolate->counters()->keyed_load_polymorphic_stubs()->Increment();

  PolymorphicCodeCache::Update(cache, receiver_maps, flags, code);
  return code;
}


Handle<Code> PropertyICCompiler::ComputePolymorphic(
    Code::Kind kind, TypeHandleList* types, CodeHandleList* handlers,
    int valid_types, Handle<Name> name, ExtraICState extra_ic_state) {
  Handle<Code> handler = handlers->at(0);
  Code::StubType type = valid_types == 1 ? handler->type() : Code::NORMAL;
  ASSERT(kind == Code::LOAD_IC || kind == Code::STORE_IC);
  PropertyICCompiler ic_compiler(name->GetIsolate(), kind, extra_ic_state);
  return ic_compiler.CompilePolymorphic(types, handlers, name, type, PROPERTY);
}


Handle<Code> PropertyICCompiler::ComputeKeyedStorePolymorphic(
    MapHandleList* receiver_maps, KeyedAccessStoreMode store_mode,
    StrictMode strict_mode) {
  Isolate* isolate = receiver_maps->at(0)->GetIsolate();
  ASSERT(store_mode == STANDARD_STORE ||
         store_mode == STORE_AND_GROW_NO_TRANSITION ||
         store_mode == STORE_NO_TRANSITION_IGNORE_OUT_OF_BOUNDS ||
         store_mode == STORE_NO_TRANSITION_HANDLE_COW);
  Handle<PolymorphicCodeCache> cache =
      isolate->factory()->polymorphic_code_cache();
  ExtraICState extra_state = KeyedStoreIC::ComputeExtraICState(
      strict_mode, store_mode);
  Code::Flags flags =
      Code::ComputeFlags(Code::KEYED_STORE_IC, POLYMORPHIC, extra_state);
  Handle<Object> probe = cache->Lookup(receiver_maps, flags);
  if (probe->IsCode()) return Handle<Code>::cast(probe);

  PropertyICCompiler compiler(isolate, Code::KEYED_STORE_IC, extra_state);
  Handle<Code> code =
      compiler.CompileKeyedStorePolymorphic(receiver_maps, store_mode);
  PolymorphicCodeCache::Update(cache, receiver_maps, flags, code);
  return code;
}


void StubCache::Clear() {
  Code* empty = isolate_->builtins()->builtin(Builtins::kIllegal);
  for (int i = 0; i < kPrimaryTableSize; i++) {
    primary_[i].key = isolate()->heap()->empty_string();
    primary_[i].map = NULL;
    primary_[i].value = empty;
  }
  for (int j = 0; j < kSecondaryTableSize; j++) {
    secondary_[j].key = isolate()->heap()->empty_string();
    secondary_[j].map = NULL;
    secondary_[j].value = empty;
  }
}


void StubCache::CollectMatchingMaps(SmallMapList* types,
                                    Handle<Name> name,
                                    Code::Flags flags,
                                    Handle<Context> native_context,
                                    Zone* zone) {
  for (int i = 0; i < kPrimaryTableSize; i++) {
    if (primary_[i].key == *name) {
      Map* map = primary_[i].map;
      // Map can be NULL, if the stub is constant function call
      // with a primitive receiver.
      if (map == NULL) continue;

      int offset = PrimaryOffset(*name, flags, map);
      if (entry(primary_, offset) == &primary_[i] &&
          !TypeFeedbackOracle::CanRetainOtherContext(map, *native_context)) {
        types->AddMapIfMissing(Handle<Map>(map), zone);
      }
    }
  }

  for (int i = 0; i < kSecondaryTableSize; i++) {
    if (secondary_[i].key == *name) {
      Map* map = secondary_[i].map;
      // Map can be NULL, if the stub is constant function call
      // with a primitive receiver.
      if (map == NULL) continue;

      // Lookup in primary table and skip duplicates.
      int primary_offset = PrimaryOffset(*name, flags, map);

      // Lookup in secondary table and add matches.
      int offset = SecondaryOffset(*name, flags, primary_offset);
      if (entry(secondary_, offset) == &secondary_[i] &&
          !TypeFeedbackOracle::CanRetainOtherContext(map, *native_context)) {
        types->AddMapIfMissing(Handle<Map>(map), zone);
      }
    }
  }
}


// ------------------------------------------------------------------------
// StubCompiler implementation.


RUNTIME_FUNCTION(StoreCallbackProperty) {
  Handle<JSObject> receiver = args.at<JSObject>(0);
  Handle<JSObject> holder = args.at<JSObject>(1);
  Handle<ExecutableAccessorInfo> callback = args.at<ExecutableAccessorInfo>(2);
  Handle<Name> name = args.at<Name>(3);
  Handle<Object> value = args.at<Object>(4);
  HandleScope scope(isolate);

  ASSERT(callback->IsCompatibleReceiver(*receiver));

  Address setter_address = v8::ToCData<Address>(callback->setter());
  v8::AccessorSetterCallback fun =
      FUNCTION_CAST<v8::AccessorSetterCallback>(setter_address);
  ASSERT(fun != NULL);

  // TODO(rossberg): Support symbols in the API.
  if (name->IsSymbol()) return *value;
  Handle<String> str = Handle<String>::cast(name);

  LOG(isolate, ApiNamedPropertyAccess("store", *receiver, *name));
  PropertyCallbackArguments custom_args(isolate, callback->data(), *receiver,
                                        *holder);
  custom_args.Call(fun, v8::Utils::ToLocal(str), v8::Utils::ToLocal(value));
  RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
  return *value;
}


/**
 * Attempts to load a property with an interceptor (which must be present),
 * but doesn't search the prototype chain.
 *
 * Returns |Heap::no_interceptor_result_sentinel()| if interceptor doesn't
 * provide any value for the given name.
 */
RUNTIME_FUNCTION(LoadPropertyWithInterceptorOnly) {
  ASSERT(args.length() == NamedLoadHandlerCompiler::kInterceptorArgsLength);
  Handle<Name> name_handle =
      args.at<Name>(NamedLoadHandlerCompiler::kInterceptorArgsNameIndex);
  Handle<InterceptorInfo> interceptor_info = args.at<InterceptorInfo>(
      NamedLoadHandlerCompiler::kInterceptorArgsInfoIndex);

  // TODO(rossberg): Support symbols in the API.
  if (name_handle->IsSymbol())
    return isolate->heap()->no_interceptor_result_sentinel();
  Handle<String> name = Handle<String>::cast(name_handle);

  Address getter_address = v8::ToCData<Address>(interceptor_info->getter());
  v8::NamedPropertyGetterCallback getter =
      FUNCTION_CAST<v8::NamedPropertyGetterCallback>(getter_address);
  ASSERT(getter != NULL);

  Handle<JSObject> receiver =
      args.at<JSObject>(NamedLoadHandlerCompiler::kInterceptorArgsThisIndex);
  Handle<JSObject> holder =
      args.at<JSObject>(NamedLoadHandlerCompiler::kInterceptorArgsHolderIndex);
  PropertyCallbackArguments callback_args(
      isolate, interceptor_info->data(), *receiver, *holder);
  {
    // Use the interceptor getter.
    HandleScope scope(isolate);
    v8::Handle<v8::Value> r =
        callback_args.Call(getter, v8::Utils::ToLocal(name));
    RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
    if (!r.IsEmpty()) {
      Handle<Object> result = v8::Utils::OpenHandle(*r);
      result->VerifyApiCallResultType();
      return *v8::Utils::OpenHandle(*r);
    }
  }

  return isolate->heap()->no_interceptor_result_sentinel();
}


static Object* ThrowReferenceError(Isolate* isolate, Name* name) {
  // If the load is non-contextual, just return the undefined result.
  // Note that both keyed and non-keyed loads may end up here.
  HandleScope scope(isolate);
  LoadIC ic(IC::NO_EXTRA_FRAME, isolate);
  if (ic.contextual_mode() != CONTEXTUAL) {
    return isolate->heap()->undefined_value();
  }

  // Throw a reference error.
  Handle<Name> name_handle(name);
  Handle<Object> error =
      isolate->factory()->NewReferenceError("not_defined",
                                            HandleVector(&name_handle, 1));
  return isolate->Throw(*error);
}


/**
 * Loads a property with an interceptor performing post interceptor
 * lookup if interceptor failed.
 */
RUNTIME_FUNCTION(LoadPropertyWithInterceptor) {
  HandleScope scope(isolate);
  ASSERT(args.length() == NamedLoadHandlerCompiler::kInterceptorArgsLength);
  Handle<Name> name =
      args.at<Name>(NamedLoadHandlerCompiler::kInterceptorArgsNameIndex);
  Handle<JSObject> receiver =
      args.at<JSObject>(NamedLoadHandlerCompiler::kInterceptorArgsThisIndex);
  Handle<JSObject> holder =
      args.at<JSObject>(NamedLoadHandlerCompiler::kInterceptorArgsHolderIndex);

  Handle<Object> result;
  LookupIterator it(receiver, name, holder);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, JSObject::GetProperty(&it));

  if (it.IsFound()) return *result;

  return ThrowReferenceError(isolate, Name::cast(args[0]));
}


RUNTIME_FUNCTION(StorePropertyWithInterceptor) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  StoreIC ic(IC::NO_EXTRA_FRAME, isolate);
  Handle<JSObject> receiver = args.at<JSObject>(0);
  Handle<Name> name = args.at<Name>(1);
  Handle<Object> value = args.at<Object>(2);
#ifdef DEBUG
  if (receiver->IsJSGlobalProxy()) {
    PrototypeIterator iter(isolate, receiver);
    ASSERT(iter.IsAtEnd() ||
           Handle<JSGlobalObject>::cast(PrototypeIterator::GetCurrent(iter))
               ->HasNamedInterceptor());
  } else {
    ASSERT(receiver->HasNamedInterceptor());
  }
#endif
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      JSObject::SetProperty(receiver, name, value, ic.strict_mode()));
  return *result;
}


RUNTIME_FUNCTION(LoadElementWithInterceptor) {
  HandleScope scope(isolate);
  Handle<JSObject> receiver = args.at<JSObject>(0);
  ASSERT(args.smi_at(1) >= 0);
  uint32_t index = args.smi_at(1);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      JSObject::GetElementWithInterceptor(receiver, receiver, index));
  return *result;
}


Handle<Code> PropertyICCompiler::CompileLoadInitialize(Code::Flags flags) {
  LoadIC::GenerateInitialize(masm());
  Handle<Code> code = GetCodeWithFlags(flags, "CompileLoadInitialize");
  PROFILE(isolate(),
          CodeCreateEvent(Logger::LOAD_INITIALIZE_TAG, *code, 0));
  return code;
}


Handle<Code> PropertyICCompiler::CompileLoadPreMonomorphic(Code::Flags flags) {
  LoadIC::GeneratePreMonomorphic(masm());
  Handle<Code> code = GetCodeWithFlags(flags, "CompileLoadPreMonomorphic");
  PROFILE(isolate(),
          CodeCreateEvent(Logger::LOAD_PREMONOMORPHIC_TAG, *code, 0));
  return code;
}


Handle<Code> PropertyICCompiler::CompileLoadMegamorphic(Code::Flags flags) {
  LoadIC::GenerateMegamorphic(masm());
  Handle<Code> code = GetCodeWithFlags(flags, "CompileLoadMegamorphic");
  PROFILE(isolate(),
          CodeCreateEvent(Logger::LOAD_MEGAMORPHIC_TAG, *code, 0));
  return code;
}


Handle<Code> PropertyICCompiler::CompileStoreInitialize(Code::Flags flags) {
  StoreIC::GenerateInitialize(masm());
  Handle<Code> code = GetCodeWithFlags(flags, "CompileStoreInitialize");
  PROFILE(isolate(),
          CodeCreateEvent(Logger::STORE_INITIALIZE_TAG, *code, 0));
  return code;
}


Handle<Code> PropertyICCompiler::CompileStorePreMonomorphic(Code::Flags flags) {
  StoreIC::GeneratePreMonomorphic(masm());
  Handle<Code> code = GetCodeWithFlags(flags, "CompileStorePreMonomorphic");
  PROFILE(isolate(),
          CodeCreateEvent(Logger::STORE_PREMONOMORPHIC_TAG, *code, 0));
  return code;
}


Handle<Code> PropertyICCompiler::CompileStoreGeneric(Code::Flags flags) {
  ExtraICState extra_state = Code::ExtractExtraICStateFromFlags(flags);
  StrictMode strict_mode = StoreIC::GetStrictMode(extra_state);
  StoreIC::GenerateRuntimeSetProperty(masm(), strict_mode);
  Handle<Code> code = GetCodeWithFlags(flags, "CompileStoreGeneric");
  PROFILE(isolate(),
          CodeCreateEvent(Logger::STORE_GENERIC_TAG, *code, 0));
  return code;
}


Handle<Code> PropertyICCompiler::CompileStoreMegamorphic(Code::Flags flags) {
  StoreIC::GenerateMegamorphic(masm());
  Handle<Code> code = GetCodeWithFlags(flags, "CompileStoreMegamorphic");
  PROFILE(isolate(),
          CodeCreateEvent(Logger::STORE_MEGAMORPHIC_TAG, *code, 0));
  return code;
}


#undef CALL_LOGGER_TAG


Handle<Code> PropertyAccessCompiler::GetCodeWithFlags(Code::Flags flags,
                                                      const char* name) {
  // Create code object in the heap.
  CodeDesc desc;
  masm()->GetCode(&desc);
  Handle<Code> code = factory()->NewCode(desc, flags, masm()->CodeObject());
  if (code->IsCodeStubOrIC()) code->set_stub_key(CodeStub::NoCacheKey());
#ifdef ENABLE_DISASSEMBLER
  if (FLAG_print_code_stubs) {
    OFStream os(stdout);
    code->Disassemble(name, os);
  }
#endif
  return code;
}


Handle<Code> PropertyAccessCompiler::GetCodeWithFlags(Code::Flags flags,
                                                      Handle<Name> name) {
  return (FLAG_print_code_stubs && !name.is_null() && name->IsString())
      ? GetCodeWithFlags(flags, Handle<String>::cast(name)->ToCString().get())
      : GetCodeWithFlags(flags, NULL);
}


#define __ ACCESS_MASM(masm())


Register NamedLoadHandlerCompiler::FrontendHeader(Register object_reg,
                                                  Handle<Name> name,
                                                  Label* miss) {
  PrototypeCheckType check_type = CHECK_ALL_MAPS;
  int function_index = -1;
  if (type()->Is(HeapType::String())) {
    function_index = Context::STRING_FUNCTION_INDEX;
  } else if (type()->Is(HeapType::Symbol())) {
    function_index = Context::SYMBOL_FUNCTION_INDEX;
  } else if (type()->Is(HeapType::Number())) {
    function_index = Context::NUMBER_FUNCTION_INDEX;
  } else if (type()->Is(HeapType::Boolean())) {
    function_index = Context::BOOLEAN_FUNCTION_INDEX;
  } else {
    check_type = SKIP_RECEIVER;
  }

  if (check_type == CHECK_ALL_MAPS) {
    GenerateDirectLoadGlobalFunctionPrototype(
        masm(), function_index, scratch1(), miss);
    Object* function = isolate()->native_context()->get(function_index);
    Object* prototype = JSFunction::cast(function)->instance_prototype();
    set_type_for_object(handle(prototype, isolate()));
    object_reg = scratch1();
  }

  // Check that the maps starting from the prototype haven't changed.
  return CheckPrototypes(object_reg, scratch1(), scratch2(), scratch3(), name,
                         miss, check_type);
}


// Frontend for store uses the name register. It has to be restored before a
// miss.
Register NamedStoreHandlerCompiler::FrontendHeader(Register object_reg,
                                                   Handle<Name> name,
                                                   Label* miss) {
  return CheckPrototypes(object_reg, this->name(), scratch1(), scratch2(), name,
                         miss, SKIP_RECEIVER);
}


bool PropertyICCompiler::IncludesNumberType(TypeHandleList* types) {
  for (int i = 0; i < types->length(); ++i) {
    if (types->at(i)->Is(HeapType::Number())) return true;
  }
  return false;
}


Register PropertyHandlerCompiler::Frontend(Register object_reg,
                                           Handle<Name> name) {
  Label miss;
  Register reg = FrontendHeader(object_reg, name, &miss);
  FrontendFooter(name, &miss);
  return reg;
}


void NamedLoadHandlerCompiler::NonexistentFrontend(Handle<Name> name) {
  Label miss;

  Register holder_reg;
  Handle<Map> last_map;
  if (holder().is_null()) {
    holder_reg = receiver();
    last_map = IC::TypeToMap(*type(), isolate());
    // If |type| has null as its prototype, |holder()| is
    // Handle<JSObject>::null().
    ASSERT(last_map->prototype() == isolate()->heap()->null_value());
  } else {
    holder_reg = FrontendHeader(receiver(), name, &miss);
    last_map = handle(holder()->map());
  }

  if (last_map->is_dictionary_map() && !last_map->IsJSGlobalObjectMap()) {
    if (!name->IsUniqueName()) {
      ASSERT(name->IsString());
      name = factory()->InternalizeString(Handle<String>::cast(name));
    }
    ASSERT(holder().is_null() ||
           holder()->property_dictionary()->FindEntry(name) ==
               NameDictionary::kNotFound);
    GenerateDictionaryNegativeLookup(masm(), &miss, holder_reg, name,
                                     scratch2(), scratch3());
  }

  // If the last object in the prototype chain is a global object,
  // check that the global property cell is empty.
  if (last_map->IsJSGlobalObjectMap()) {
    Handle<JSGlobalObject> global =
        holder().is_null()
            ? Handle<JSGlobalObject>::cast(type()->AsConstant()->Value())
            : Handle<JSGlobalObject>::cast(holder());
    GenerateCheckPropertyCell(masm(), global, name, scratch2(), &miss);
  }

  FrontendFooter(name, &miss);
}


Handle<Code> NamedLoadHandlerCompiler::CompileLoadField(
    Handle<Name> name, FieldIndex field, Representation representation) {
  Register reg = Frontend(receiver(), name);
  GenerateLoadField(reg, field, representation);
  return GetCode(kind(), Code::FAST, name);
}


Handle<Code> NamedLoadHandlerCompiler::CompileLoadConstant(
    Handle<Name> name, Handle<Object> value) {
  Frontend(receiver(), name);
  GenerateLoadConstant(value);
  return GetCode(kind(), Code::FAST, name);
}


Handle<Code> NamedLoadHandlerCompiler::CompileLoadCallback(
    Handle<Name> name, Handle<ExecutableAccessorInfo> callback) {
  Register reg = CallbackFrontend(receiver(), name, callback);
  GenerateLoadCallback(reg, callback);
  return GetCode(kind(), Code::FAST, name);
}


Handle<Code> NamedLoadHandlerCompiler::CompileLoadCallback(
    Handle<Name> name, const CallOptimization& call_optimization) {
  ASSERT(call_optimization.is_simple_api_call());
  Handle<JSFunction> callback = call_optimization.constant_function();
  CallbackFrontend(receiver(), name, callback);
  Handle<Map> receiver_map = IC::TypeToMap(*type(), isolate());
  GenerateFastApiCall(
      masm(), call_optimization, receiver_map,
      receiver(), scratch1(), false, 0, NULL);
  return GetCode(kind(), Code::FAST, name);
}


Handle<Code> NamedLoadHandlerCompiler::CompileLoadInterceptor(
    Handle<Name> name) {
  // Perform a lookup after the interceptor.
  LookupResult lookup(isolate());
  holder()->LookupOwnRealNamedProperty(name, &lookup);
  if (!lookup.IsFound()) {
    PrototypeIterator iter(holder()->GetIsolate(), holder());
    if (!iter.IsAtEnd()) {
      PrototypeIterator::GetCurrent(iter)->Lookup(name, &lookup);
    }
  }

  Register reg = Frontend(receiver(), name);
  // TODO(368): Compile in the whole chain: all the interceptors in
  // prototypes and ultimate answer.
  GenerateLoadInterceptor(reg, &lookup, name);
  return GetCode(kind(), Code::FAST, name);
}


void NamedLoadHandlerCompiler::GenerateLoadPostInterceptor(
    Register interceptor_reg, Handle<Name> name, LookupResult* lookup) {
  Handle<JSObject> real_named_property_holder(lookup->holder());
  if (lookup->IsField()) {
    FieldIndex field = lookup->GetFieldIndex();
    if (holder().is_identical_to(real_named_property_holder)) {
      GenerateLoadField(interceptor_reg, field, lookup->representation());
    } else {
      set_type_for_object(holder());
      set_holder(real_named_property_holder);
      Register reg = Frontend(interceptor_reg, name);
      GenerateLoadField(reg, field, lookup->representation());
    }
  } else {
    // We found CALLBACKS property in prototype chain of interceptor's holder.
    ASSERT(lookup->type() == CALLBACKS);
    Handle<ExecutableAccessorInfo> callback(
        ExecutableAccessorInfo::cast(lookup->GetCallbackObject()));
    ASSERT(callback->getter() != NULL);

    set_type_for_object(holder());
    set_holder(real_named_property_holder);
    Register reg = CallbackFrontend(interceptor_reg, name, callback);
    GenerateLoadCallback(reg, callback);
  }
}


Handle<Code> PropertyICCompiler::CompileMonomorphic(Handle<HeapType> type,
                                                    Handle<Code> handler,
                                                    Handle<Name> name,
                                                    IcCheckType check) {
  TypeHandleList types(1);
  CodeHandleList handlers(1);
  types.Add(type);
  handlers.Add(handler);
  Code::StubType stub_type = handler->type();
  return CompilePolymorphic(&types, &handlers, name, stub_type, check);
}


Handle<Code> NamedLoadHandlerCompiler::CompileLoadViaGetter(
    Handle<Name> name, Handle<JSFunction> getter) {
  Frontend(receiver(), name);
  GenerateLoadViaGetter(masm(), type(), receiver(), getter);
  return GetCode(kind(), Code::FAST, name);
}


// TODO(verwaest): Cleanup. holder() is actually the receiver.
Handle<Code> NamedStoreHandlerCompiler::CompileStoreTransition(
    LookupResult* lookup, Handle<Map> transition, Handle<Name> name) {
  Label miss, slow;

  // Ensure no transitions to deprecated maps are followed.
  __ CheckMapDeprecated(transition, scratch1(), &miss);

  // Check that we are allowed to write this.
  bool is_nonexistent = holder()->map() == transition->GetBackPointer();
  if (is_nonexistent) {
    // Find the top object.
    Handle<JSObject> last;
    PrototypeIterator iter(isolate(), holder());
    while (!iter.IsAtEnd()) {
      last = Handle<JSObject>::cast(PrototypeIterator::GetCurrent(iter));
      iter.Advance();
    }
    if (!last.is_null()) set_holder(last);
  }

  Register holder_reg = FrontendHeader(receiver(), name, &miss);

  // If no property was found, and the holder (the last object in the
  // prototype chain) is in slow mode, we need to do a negative lookup on the
  // holder.
  if (is_nonexistent) {
    GenerateNegativeHolderLookup(masm(), holder(), holder_reg, name, &miss);
  }

  GenerateStoreTransition(masm(),
                          lookup,
                          transition,
                          name,
                          receiver(), this->name(), value(),
                          scratch1(), scratch2(), scratch3(),
                          &miss,
                          &slow);

  // Handle store cache miss.
  GenerateRestoreName(masm(), &miss, name);
  TailCallBuiltin(masm(), MissBuiltin(kind()));

  GenerateRestoreName(masm(), &slow, name);
  TailCallBuiltin(masm(), SlowBuiltin(kind()));
  return GetCode(kind(), Code::FAST, name);
}


Handle<Code> NamedStoreHandlerCompiler::CompileStoreField(LookupResult* lookup,
                                                          Handle<Name> name) {
  Label miss;

  FrontendHeader(receiver(), name, &miss);

  // Generate store field code.
  GenerateStoreField(masm(), holder(), lookup, receiver(), this->name(),
                     value(), scratch1(), scratch2(), &miss);

  // Handle store cache miss.
  __ bind(&miss);
  TailCallBuiltin(masm(), MissBuiltin(kind()));
  return GetCode(kind(), Code::FAST, name);
}


Handle<Code> NamedStoreHandlerCompiler::CompileStoreArrayLength(
    LookupResult* lookup, Handle<Name> name) {
  // This accepts as a receiver anything JSArray::SetElementsLength accepts
  // (currently anything except for external arrays which means anything with
  // elements of FixedArray type).  Value must be a number, but only smis are
  // accepted as the most common case.
  Label miss;

  // Check that value is a smi.
  __ JumpIfNotSmi(value(), &miss);

  // Generate tail call to StoreIC_ArrayLength.
  GenerateStoreArrayLength();

  // Handle miss case.
  __ bind(&miss);
  TailCallBuiltin(masm(), MissBuiltin(kind()));
  return GetCode(kind(), Code::FAST, name);
}


Handle<Code> NamedStoreHandlerCompiler::CompileStoreViaSetter(
    Handle<JSObject> object, Handle<Name> name, Handle<JSFunction> setter) {
  Frontend(receiver(), name);
  GenerateStoreViaSetter(masm(), type(), receiver(), setter);

  return GetCode(kind(), Code::FAST, name);
}


Handle<Code> NamedStoreHandlerCompiler::CompileStoreCallback(
    Handle<JSObject> object, Handle<Name> name,
    const CallOptimization& call_optimization) {
  Frontend(receiver(), name);
  Register values[] = { value() };
  GenerateFastApiCall(
      masm(), call_optimization, handle(object->map()),
      receiver(), scratch1(), true, 1, values);
  return GetCode(kind(), Code::FAST, name);
}


Handle<Code> PropertyICCompiler::CompileKeyedStoreMonomorphic(
    Handle<Map> receiver_map, KeyedAccessStoreMode store_mode) {
  ElementsKind elements_kind = receiver_map->elements_kind();
  bool is_jsarray = receiver_map->instance_type() == JS_ARRAY_TYPE;
  Handle<Code> stub;
  if (receiver_map->has_fast_elements() ||
      receiver_map->has_external_array_elements() ||
      receiver_map->has_fixed_typed_array_elements()) {
    stub = StoreFastElementStub(isolate(), is_jsarray, elements_kind,
                                store_mode).GetCode();
  } else {
    stub = StoreElementStub(isolate(), is_jsarray, elements_kind, store_mode)
               .GetCode();
  }

  __ DispatchMap(receiver(), scratch1(), receiver_map, stub, DO_SMI_CHECK);

  TailCallBuiltin(masm(), Builtins::kKeyedStoreIC_Miss);

  return GetCode(kind(), Code::NORMAL, factory()->empty_string());
}


#undef __


void PropertyAccessCompiler::TailCallBuiltin(MacroAssembler* masm,
                                             Builtins::Name name) {
  Handle<Code> code(masm->isolate()->builtins()->builtin(name));
  GenerateTailCall(masm, code);
}


Register* PropertyAccessCompiler::GetCallingConvention(Code::Kind kind) {
  if (kind == Code::LOAD_IC || kind == Code::KEYED_LOAD_IC) {
    return load_calling_convention();
  }
  ASSERT(kind == Code::STORE_IC || kind == Code::KEYED_STORE_IC);
  return store_calling_convention();
}


Handle<Code> PropertyICCompiler::GetCode(Code::Kind kind, Code::StubType type,
                                         Handle<Name> name,
                                         InlineCacheState state) {
  Code::Flags flags =
      Code::ComputeFlags(kind, state, extra_ic_state_, type, cache_holder());
  Handle<Code> code = GetCodeWithFlags(flags, name);
  IC::RegisterWeakMapDependency(code);
  PROFILE(isolate(), CodeCreateEvent(log_kind(code), *code, *name));
  return code;
}


Handle<Code> PropertyHandlerCompiler::GetCode(Code::Kind kind,
                                              Code::StubType type,
                                              Handle<Name> name) {
  Code::Flags flags = Code::ComputeHandlerFlags(kind, type, cache_holder());
  Handle<Code> code = GetCodeWithFlags(flags, name);
  PROFILE(isolate(), CodeCreateEvent(Logger::STUB_TAG, *code, *name));
  return code;
}


void ElementHandlerCompiler::CompileElementHandlers(
    MapHandleList* receiver_maps, CodeHandleList* handlers) {
  for (int i = 0; i < receiver_maps->length(); ++i) {
    Handle<Map> receiver_map = receiver_maps->at(i);
    Handle<Code> cached_stub;

    if ((receiver_map->instance_type() & kNotStringTag) == 0) {
      cached_stub = isolate()->builtins()->KeyedLoadIC_String();
    } else if (receiver_map->instance_type() < FIRST_JS_RECEIVER_TYPE) {
      cached_stub = isolate()->builtins()->KeyedLoadIC_Slow();
    } else {
      bool is_js_array = receiver_map->instance_type() == JS_ARRAY_TYPE;
      ElementsKind elements_kind = receiver_map->elements_kind();

      if (IsFastElementsKind(elements_kind) ||
          IsExternalArrayElementsKind(elements_kind) ||
          IsFixedTypedArrayElementsKind(elements_kind)) {
        cached_stub = LoadFastElementStub(isolate(), is_js_array, elements_kind)
                          .GetCode();
      } else if (elements_kind == SLOPPY_ARGUMENTS_ELEMENTS) {
        cached_stub = isolate()->builtins()->KeyedLoadIC_SloppyArguments();
      } else {
        ASSERT(elements_kind == DICTIONARY_ELEMENTS);
        cached_stub = LoadDictionaryElementStub(isolate()).GetCode();
      }
    }

    handlers->Add(cached_stub);
  }
}


Handle<Code> PropertyICCompiler::CompileKeyedStorePolymorphic(
    MapHandleList* receiver_maps, KeyedAccessStoreMode store_mode) {
  // Collect MONOMORPHIC stubs for all |receiver_maps|.
  CodeHandleList handlers(receiver_maps->length());
  MapHandleList transitioned_maps(receiver_maps->length());
  for (int i = 0; i < receiver_maps->length(); ++i) {
    Handle<Map> receiver_map(receiver_maps->at(i));
    Handle<Code> cached_stub;
    Handle<Map> transitioned_map =
        receiver_map->FindTransitionedMap(receiver_maps);

    // TODO(mvstanton): The code below is doing pessimistic elements
    // transitions. I would like to stop doing that and rely on Allocation Site
    // Tracking to do a better job of ensuring the data types are what they need
    // to be. Not all the elements are in place yet, pessimistic elements
    // transitions are still important for performance.
    bool is_js_array = receiver_map->instance_type() == JS_ARRAY_TYPE;
    ElementsKind elements_kind = receiver_map->elements_kind();
    if (!transitioned_map.is_null()) {
      cached_stub =
          ElementsTransitionAndStoreStub(isolate(), elements_kind,
                                         transitioned_map->elements_kind(),
                                         is_js_array, store_mode).GetCode();
    } else if (receiver_map->instance_type() < FIRST_JS_RECEIVER_TYPE) {
      cached_stub = isolate()->builtins()->KeyedStoreIC_Slow();
    } else {
      if (receiver_map->has_fast_elements() ||
          receiver_map->has_external_array_elements() ||
          receiver_map->has_fixed_typed_array_elements()) {
        cached_stub = StoreFastElementStub(isolate(), is_js_array,
                                           elements_kind, store_mode).GetCode();
      } else {
        cached_stub = StoreElementStub(isolate(), is_js_array, elements_kind,
                                       store_mode).GetCode();
      }
    }
    ASSERT(!cached_stub.is_null());
    handlers.Add(cached_stub);
    transitioned_maps.Add(transitioned_map);
  }

  Handle<Code> code = CompileKeyedStorePolymorphic(receiver_maps, &handlers,
                                                   &transitioned_maps);
  isolate()->counters()->keyed_store_polymorphic_stubs()->Increment();
  PROFILE(isolate(), CodeCreateEvent(log_kind(code), *code, 0));
  return code;
}


void ElementHandlerCompiler::GenerateStoreDictionaryElement(
    MacroAssembler* masm) {
  KeyedStoreIC::GenerateSlow(masm);
}


CallOptimization::CallOptimization(LookupResult* lookup) {
  if (lookup->IsFound() &&
      lookup->IsCacheable() &&
      lookup->IsConstantFunction()) {
    // We only optimize constant function calls.
    Initialize(Handle<JSFunction>(lookup->GetConstantFunction()));
  } else {
    Initialize(Handle<JSFunction>::null());
  }
}


CallOptimization::CallOptimization(Handle<JSFunction> function) {
  Initialize(function);
}


Handle<JSObject> CallOptimization::LookupHolderOfExpectedType(
    Handle<Map> object_map,
    HolderLookup* holder_lookup) const {
  ASSERT(is_simple_api_call());
  if (!object_map->IsJSObjectMap()) {
    *holder_lookup = kHolderNotFound;
    return Handle<JSObject>::null();
  }
  if (expected_receiver_type_.is_null() ||
      expected_receiver_type_->IsTemplateFor(*object_map)) {
    *holder_lookup = kHolderIsReceiver;
    return Handle<JSObject>::null();
  }
  while (true) {
    if (!object_map->prototype()->IsJSObject()) break;
    Handle<JSObject> prototype(JSObject::cast(object_map->prototype()));
    if (!prototype->map()->is_hidden_prototype()) break;
    object_map = handle(prototype->map());
    if (expected_receiver_type_->IsTemplateFor(*object_map)) {
      *holder_lookup = kHolderFound;
      return prototype;
    }
  }
  *holder_lookup = kHolderNotFound;
  return Handle<JSObject>::null();
}


bool CallOptimization::IsCompatibleReceiver(Handle<Object> receiver,
                                            Handle<JSObject> holder) const {
  ASSERT(is_simple_api_call());
  if (!receiver->IsJSObject()) return false;
  Handle<Map> map(JSObject::cast(*receiver)->map());
  HolderLookup holder_lookup;
  Handle<JSObject> api_holder =
      LookupHolderOfExpectedType(map, &holder_lookup);
  switch (holder_lookup) {
    case kHolderNotFound:
      return false;
    case kHolderIsReceiver:
      return true;
    case kHolderFound:
      if (api_holder.is_identical_to(holder)) return true;
      // Check if holder is in prototype chain of api_holder.
      {
        JSObject* object = *api_holder;
        while (true) {
          Object* prototype = object->map()->prototype();
          if (!prototype->IsJSObject()) return false;
          if (prototype == *holder) return true;
          object = JSObject::cast(prototype);
        }
      }
      break;
  }
  UNREACHABLE();
  return false;
}


void CallOptimization::Initialize(Handle<JSFunction> function) {
  constant_function_ = Handle<JSFunction>::null();
  is_simple_api_call_ = false;
  expected_receiver_type_ = Handle<FunctionTemplateInfo>::null();
  api_call_info_ = Handle<CallHandlerInfo>::null();

  if (function.is_null() || !function->is_compiled()) return;

  constant_function_ = function;
  AnalyzePossibleApiFunction(function);
}


void CallOptimization::AnalyzePossibleApiFunction(Handle<JSFunction> function) {
  if (!function->shared()->IsApiFunction()) return;
  Handle<FunctionTemplateInfo> info(function->shared()->get_api_func_data());

  // Require a C++ callback.
  if (info->call_code()->IsUndefined()) return;
  api_call_info_ =
      Handle<CallHandlerInfo>(CallHandlerInfo::cast(info->call_code()));

  // Accept signatures that either have no restrictions at all or
  // only have restrictions on the receiver.
  if (!info->signature()->IsUndefined()) {
    Handle<SignatureInfo> signature =
        Handle<SignatureInfo>(SignatureInfo::cast(info->signature()));
    if (!signature->args()->IsUndefined()) return;
    if (!signature->receiver()->IsUndefined()) {
      expected_receiver_type_ =
          Handle<FunctionTemplateInfo>(
              FunctionTemplateInfo::cast(signature->receiver()));
    }
  }

  is_simple_api_call_ = true;
}


} }  // namespace v8::internal
