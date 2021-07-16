// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/init/bootstrapper.h"

#include "src/api/api-inl.h"
#include "src/api/api-natives.h"
#include "src/base/hashmap.h"
#include "src/base/ieee754.h"
#include "src/builtins/accessors.h"
#include "src/codegen/compiler.h"
#include "src/common/globals.h"
#include "src/debug/debug.h"
#include "src/execution/isolate-inl.h"
#include "src/execution/microtask-queue.h"
#include "src/execution/protectors.h"
#include "src/extensions/cputracemark-extension.h"
#include "src/extensions/externalize-string-extension.h"
#include "src/extensions/gc-extension.h"
#include "src/extensions/ignition-statistics-extension.h"
#include "src/extensions/statistics-extension.h"
#include "src/extensions/trigger-failure-extension.h"
#include "src/logging/runtime-call-stats-scope.h"
#include "src/objects/objects.h"
#ifdef ENABLE_VTUNE_TRACEMARK
#include "src/extensions/vtunedomain-support-extension.h"
#endif  // ENABLE_VTUNE_TRACEMARK
#include "src/heap/heap-inl.h"
#include "src/logging/counters.h"
#include "src/logging/log.h"
#include "src/numbers/math-random.h"
#include "src/objects/api-callbacks.h"
#include "src/objects/arguments.h"
#include "src/objects/function-kind.h"
#include "src/objects/hash-table-inl.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/intl-objects.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/js-array-inl.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/js-break-iterator.h"
#include "src/objects/js-collator.h"
#include "src/objects/js-date-time-format.h"
#include "src/objects/js-display-names.h"
#include "src/objects/js-list-format.h"
#include "src/objects/js-locale.h"
#include "src/objects/js-number-format.h"
#include "src/objects/js-plural-rules.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/js-regexp-string-iterator.h"
#include "src/objects/js-regexp.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/js-relative-time-format.h"
#include "src/objects/js-segment-iterator.h"
#include "src/objects/js-segmenter.h"
#include "src/objects/js-segments.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/js-weak-refs.h"
#include "src/objects/ordered-hash-table.h"
#include "src/objects/property-cell.h"
#include "src/objects/slots-inl.h"
#include "src/objects/swiss-name-dictionary-inl.h"
#include "src/objects/templates.h"
#include "src/snapshot/snapshot.h"
#include "src/zone/zone-hashmap.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/wasm-js.h"
#endif  // V8_ENABLE_WEBASSEMBLY

namespace v8 {
namespace internal {

void SourceCodeCache::Initialize(Isolate* isolate, bool create_heap_objects) {
  cache_ = create_heap_objects ? ReadOnlyRoots(isolate).empty_fixed_array()
                               : FixedArray();
}

void SourceCodeCache::Iterate(RootVisitor* v) {
  v->VisitRootPointer(Root::kExtensions, nullptr, FullObjectSlot(&cache_));
}

bool SourceCodeCache::Lookup(Isolate* isolate, base::Vector<const char> name,
                             Handle<SharedFunctionInfo>* handle) {
  for (int i = 0; i < cache_.length(); i += 2) {
    SeqOneByteString str = SeqOneByteString::cast(cache_.get(i));
    if (str.IsOneByteEqualTo(name)) {
      *handle = Handle<SharedFunctionInfo>(
          SharedFunctionInfo::cast(cache_.get(i + 1)), isolate);
      return true;
    }
  }
  return false;
}

void SourceCodeCache::Add(Isolate* isolate, base::Vector<const char> name,
                          Handle<SharedFunctionInfo> shared) {
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);
  int length = cache_.length();
  Handle<FixedArray> new_array =
      factory->NewFixedArray(length + 2, AllocationType::kOld);
  cache_.CopyTo(0, *new_array, 0, cache_.length());
  cache_ = *new_array;
  Handle<String> str =
      factory
          ->NewStringFromOneByte(base::Vector<const uint8_t>::cast(name),
                                 AllocationType::kOld)
          .ToHandleChecked();
  DCHECK(!str.is_null());
  cache_.set(length, *str);
  cache_.set(length + 1, *shared);
  Script::cast(shared->script()).set_type(type_);
}

Bootstrapper::Bootstrapper(Isolate* isolate)
    : isolate_(isolate),
      nesting_(0),
      extensions_cache_(Script::TYPE_EXTENSION) {}

void Bootstrapper::Initialize(bool create_heap_objects) {
  extensions_cache_.Initialize(isolate_, create_heap_objects);
}

static const char* GCFunctionName() {
  bool flag_given =
      FLAG_expose_gc_as != nullptr && strlen(FLAG_expose_gc_as) != 0;
  return flag_given ? FLAG_expose_gc_as : "gc";
}

static bool isValidCpuTraceMarkFunctionName() {
  return FLAG_expose_cputracemark_as != nullptr &&
         strlen(FLAG_expose_cputracemark_as) != 0;
}

void Bootstrapper::InitializeOncePerProcess() {
  v8::RegisterExtension(std::make_unique<GCExtension>(GCFunctionName()));
  v8::RegisterExtension(std::make_unique<ExternalizeStringExtension>());
  v8::RegisterExtension(std::make_unique<StatisticsExtension>());
  v8::RegisterExtension(std::make_unique<TriggerFailureExtension>());
  v8::RegisterExtension(std::make_unique<IgnitionStatisticsExtension>());
  if (isValidCpuTraceMarkFunctionName()) {
    v8::RegisterExtension(
        std::make_unique<CpuTraceMarkExtension>(FLAG_expose_cputracemark_as));
  }
#ifdef ENABLE_VTUNE_TRACEMARK
  v8::RegisterExtension(
      std::make_unique<VTuneDomainSupportExtension>("vtunedomainmark"));
#endif  // ENABLE_VTUNE_TRACEMARK
}

void Bootstrapper::TearDown() {
  extensions_cache_.Initialize(isolate_, false);  // Yes, symmetrical
}

class Genesis {
 public:
  Genesis(Isolate* isolate, MaybeHandle<JSGlobalProxy> maybe_global_proxy,
          v8::Local<v8::ObjectTemplate> global_proxy_template,
          size_t context_snapshot_index,
          v8::DeserializeEmbedderFieldsCallback embedder_fields_deserializer,
          v8::MicrotaskQueue* microtask_queue);
  Genesis(Isolate* isolate, MaybeHandle<JSGlobalProxy> maybe_global_proxy,
          v8::Local<v8::ObjectTemplate> global_proxy_template);
  ~Genesis() = default;

  Isolate* isolate() const { return isolate_; }
  Factory* factory() const { return isolate_->factory(); }
  Builtins* builtins() const { return isolate_->builtins(); }
  Heap* heap() const { return isolate_->heap(); }

  Handle<Context> result() { return result_; }

  Handle<JSGlobalProxy> global_proxy() { return global_proxy_; }

 private:
  Handle<NativeContext> native_context() { return native_context_; }

  // Creates some basic objects. Used for creating a context from scratch.
  void CreateRoots();
  // Creates the empty function.  Used for creating a context from scratch.
  Handle<JSFunction> CreateEmptyFunction();
  // Returns the %ThrowTypeError% intrinsic function.
  // See ES#sec-%throwtypeerror% for details.
  Handle<JSFunction> GetThrowTypeErrorIntrinsic();

  void CreateSloppyModeFunctionMaps(Handle<JSFunction> empty);
  void CreateStrictModeFunctionMaps(Handle<JSFunction> empty);
  void CreateObjectFunction(Handle<JSFunction> empty);
  void CreateIteratorMaps(Handle<JSFunction> empty);
  void CreateAsyncIteratorMaps(Handle<JSFunction> empty);
  void CreateAsyncFunctionMaps(Handle<JSFunction> empty);
  void CreateJSProxyMaps();

  // Make the "arguments" and "caller" properties throw a TypeError on access.
  void AddRestrictedFunctionProperties(Handle<JSFunction> empty);

  // Creates the global objects using the global proxy and the template passed
  // in through the API.  We call this regardless of whether we are building a
  // context from scratch or using a deserialized one from the context snapshot
  // but in the latter case we don't use the objects it produces directly, as
  // we have to use the deserialized ones that are linked together with the
  // rest of the context snapshot. At the end we link the global proxy and the
  // context to each other.
  Handle<JSGlobalObject> CreateNewGlobals(
      v8::Local<v8::ObjectTemplate> global_proxy_template,
      Handle<JSGlobalProxy> global_proxy);
  // Similarly, we want to use the global that has been created by the templates
  // passed through the API.  The global from the snapshot is detached from the
  // other objects in the snapshot.
  void HookUpGlobalObject(Handle<JSGlobalObject> global_object);
  // Hooks the given global proxy into the context in the case we do not
  // replace the global object from the deserialized native context.
  void HookUpGlobalProxy(Handle<JSGlobalProxy> global_proxy);
  // The native context has a ScriptContextTable that store declarative bindings
  // made in script scopes.  Add a "this" binding to that table pointing to the
  // global proxy.
  void InstallGlobalThisBinding();
  // New context initialization.  Used for creating a context from scratch.
  void InitializeGlobal(Handle<JSGlobalObject> global_object,
                        Handle<JSFunction> empty_function);
  void InitializeExperimentalGlobal();
  void InitializeIteratorFunctions();
  void InitializeCallSiteBuiltins();

#define DECLARE_FEATURE_INITIALIZATION(id, descr) void InitializeGlobal_##id();

  HARMONY_INPROGRESS(DECLARE_FEATURE_INITIALIZATION)
  HARMONY_STAGED(DECLARE_FEATURE_INITIALIZATION)
  HARMONY_SHIPPING(DECLARE_FEATURE_INITIALIZATION)
#undef DECLARE_FEATURE_INITIALIZATION
  void InitializeGlobal_regexp_linear_flag();

  enum ArrayBufferKind {
    ARRAY_BUFFER,
    SHARED_ARRAY_BUFFER,
    RESIZABLE_ARRAY_BUFFER,
    GROWABLE_SHARED_ARRAY_BUFFER
  };
  Handle<JSFunction> CreateArrayBuffer(Handle<String> name,
                                       ArrayBufferKind array_buffer_kind);

  bool InstallABunchOfRandomThings();
  bool InstallExtrasBindings();

  Handle<JSFunction> InstallTypedArray(const char* name,
                                       ElementsKind elements_kind,
                                       InstanceType type,
                                       int rab_gsab_initial_map_index);
  void InitializeMapCaches();

  enum ExtensionTraversalState { UNVISITED, VISITED, INSTALLED };

  class ExtensionStates {
   public:
    ExtensionStates();
    ExtensionStates(const ExtensionStates&) = delete;
    ExtensionStates& operator=(const ExtensionStates&) = delete;
    ExtensionTraversalState get_state(RegisteredExtension* extension);
    void set_state(RegisteredExtension* extension,
                   ExtensionTraversalState state);

   private:
    base::HashMap map_;
  };

  // Used both for deserialized and from-scratch contexts to add the extensions
  // provided.
  static bool InstallExtensions(Isolate* isolate,
                                Handle<Context> native_context,
                                v8::ExtensionConfiguration* extensions);
  static bool InstallAutoExtensions(Isolate* isolate,
                                    ExtensionStates* extension_states);
  static bool InstallRequestedExtensions(Isolate* isolate,
                                         v8::ExtensionConfiguration* extensions,
                                         ExtensionStates* extension_states);
  static bool InstallExtension(Isolate* isolate, const char* name,
                               ExtensionStates* extension_states);
  static bool InstallExtension(Isolate* isolate,
                               v8::RegisteredExtension* current,
                               ExtensionStates* extension_states);
  static bool InstallSpecialObjects(Isolate* isolate,
                                    Handle<Context> native_context);
  bool ConfigureApiObject(Handle<JSObject> object,
                          Handle<ObjectTemplateInfo> object_template);
  bool ConfigureGlobalObjects(
      v8::Local<v8::ObjectTemplate> global_proxy_template);

  // Migrates all properties from the 'from' object to the 'to'
  // object and overrides the prototype in 'to' with the one from
  // 'from'.
  void TransferObject(Handle<JSObject> from, Handle<JSObject> to);
  void TransferNamedProperties(Handle<JSObject> from, Handle<JSObject> to);
  void TransferIndexedProperties(Handle<JSObject> from, Handle<JSObject> to);

  Handle<Map> CreateInitialMapForArraySubclass(int size,
                                               int inobject_properties);

  static bool CompileExtension(Isolate* isolate, v8::Extension* extension);

  Isolate* isolate_;
  Handle<Context> result_;
  Handle<NativeContext> native_context_;
  Handle<JSGlobalProxy> global_proxy_;

  // %ThrowTypeError%. See ES#sec-%throwtypeerror% for details.
  Handle<JSFunction> restricted_properties_thrower_;

  BootstrapperActive active_;
  friend class Bootstrapper;
};

void Bootstrapper::Iterate(RootVisitor* v) {
  extensions_cache_.Iterate(v);
  v->Synchronize(VisitorSynchronization::kExtensions);
}

Handle<Context> Bootstrapper::CreateEnvironment(
    MaybeHandle<JSGlobalProxy> maybe_global_proxy,
    v8::Local<v8::ObjectTemplate> global_proxy_template,
    v8::ExtensionConfiguration* extensions, size_t context_snapshot_index,
    v8::DeserializeEmbedderFieldsCallback embedder_fields_deserializer,
    v8::MicrotaskQueue* microtask_queue) {
  HandleScope scope(isolate_);
  Handle<Context> env;
  {
    Genesis genesis(isolate_, maybe_global_proxy, global_proxy_template,
                    context_snapshot_index, embedder_fields_deserializer,
                    microtask_queue);
    env = genesis.result();
    if (env.is_null() || !InstallExtensions(env, extensions)) {
      return Handle<Context>();
    }
  }
  LogAllMaps();
  isolate_->heap()->NotifyBootstrapComplete();
  return scope.CloseAndEscape(env);
}

Handle<JSGlobalProxy> Bootstrapper::NewRemoteContext(
    MaybeHandle<JSGlobalProxy> maybe_global_proxy,
    v8::Local<v8::ObjectTemplate> global_proxy_template) {
  HandleScope scope(isolate_);
  Handle<JSGlobalProxy> global_proxy;
  {
    Genesis genesis(isolate_, maybe_global_proxy, global_proxy_template);
    global_proxy = genesis.global_proxy();
    if (global_proxy.is_null()) return Handle<JSGlobalProxy>();
  }
  LogAllMaps();
  return scope.CloseAndEscape(global_proxy);
}

void Bootstrapper::LogAllMaps() {
  if (!FLAG_log_maps || isolate_->initialized_from_snapshot()) return;
  // Log all created Map objects that are on the heap. For snapshots the Map
  // logging happens during deserialization in order to avoid printing Maps
  // multiple times during partial deserialization.
  LOG(isolate_, LogAllMaps());
}

void Bootstrapper::DetachGlobal(Handle<Context> env) {
  isolate_->counters()->errors_thrown_per_context()->AddSample(
      env->native_context().GetErrorsThrown());

  ReadOnlyRoots roots(isolate_);
  Handle<JSGlobalProxy> global_proxy(env->global_proxy(), isolate_);
  global_proxy->set_native_context(roots.null_value());
  // NOTE: Turbofan's JSNativeContextSpecialization depends on DetachGlobal
  // causing a map change.
  JSObject::ForceSetPrototype(isolate_, global_proxy,
                              isolate_->factory()->null_value());
  global_proxy->map().SetConstructor(roots.null_value());
  if (FLAG_track_detached_contexts) {
    isolate_->AddDetachedContext(env);
  }
  DCHECK(global_proxy->IsDetached());

  env->native_context().set_microtask_queue(isolate_, nullptr);
}

namespace {

#ifdef DEBUG
bool IsFunctionMapOrSpecialBuiltin(Handle<Map> map, Builtin builtin,
                                   Handle<Context> context) {
  // During bootstrapping some of these maps could be not created yet.
  return ((*map == context->get(Context::STRICT_FUNCTION_MAP_INDEX)) ||
          (*map == context->get(
                       Context::STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX)) ||
          (*map ==
           context->get(
               Context::STRICT_FUNCTION_WITH_READONLY_PROTOTYPE_MAP_INDEX)) ||
          // Check if it's a creation of an empty or Proxy function during
          // bootstrapping.
          (builtin == Builtin::kEmptyFunction ||
           builtin == Builtin::kProxyConstructor));
}
#endif  // DEBUG

V8_NOINLINE Handle<JSFunction> CreateFunctionForBuiltin(Isolate* isolate,
                                                        Handle<String> name,
                                                        Handle<Map> map,
                                                        Builtin builtin) {
  Factory* factory = isolate->factory();
  Handle<NativeContext> context(isolate->native_context());
  DCHECK(IsFunctionMapOrSpecialBuiltin(map, builtin, context));

  Handle<SharedFunctionInfo> info =
      factory->NewSharedFunctionInfoForBuiltin(name, builtin);
  info->set_language_mode(LanguageMode::kStrict);

  return Factory::JSFunctionBuilder{isolate, info, context}
      .set_map(map)
      .Build();
}

V8_NOINLINE Handle<JSFunction> CreateFunctionForBuiltinWithPrototype(
    Isolate* isolate, Handle<String> name, Builtin builtin,
    Handle<HeapObject> prototype, InstanceType type, int instance_size,
    int inobject_properties, MutableMode prototype_mutability) {
  Factory* factory = isolate->factory();
  Handle<NativeContext> context(isolate->native_context());
  Handle<Map> map =
      prototype_mutability == MUTABLE
          ? isolate->strict_function_map()
          : isolate->strict_function_with_readonly_prototype_map();
  DCHECK(IsFunctionMapOrSpecialBuiltin(map, builtin, context));

  Handle<SharedFunctionInfo> info =
      factory->NewSharedFunctionInfoForBuiltin(name, builtin);
  info->set_language_mode(LanguageMode::kStrict);
  info->set_expected_nof_properties(inobject_properties);

  Handle<JSFunction> result =
      Factory::JSFunctionBuilder{isolate, info, context}.set_map(map).Build();

  ElementsKind elements_kind;
  switch (type) {
    case JS_ARRAY_TYPE:
      elements_kind = PACKED_SMI_ELEMENTS;
      break;
    case JS_ARGUMENTS_OBJECT_TYPE:
      elements_kind = PACKED_ELEMENTS;
      break;
    default:
      elements_kind = TERMINAL_FAST_ELEMENTS_KIND;
      break;
  }
  Handle<Map> initial_map =
      factory->NewMap(type, instance_size, elements_kind, inobject_properties);
  // TODO(littledan): Why do we have this is_generator test when
  // NewFunctionPrototype already handles finding an appropriately
  // shared prototype?
  if (!IsResumableFunction(info->kind()) && prototype->IsTheHole(isolate)) {
    prototype = factory->NewFunctionPrototype(result);
  }
  JSFunction::SetInitialMap(isolate, result, initial_map, prototype);

  return result;
}

V8_NOINLINE Handle<JSFunction> CreateFunctionForBuiltinWithoutPrototype(
    Isolate* isolate, Handle<String> name, Builtin builtin) {
  Factory* factory = isolate->factory();
  Handle<NativeContext> context(isolate->native_context());
  Handle<Map> map = isolate->strict_function_without_prototype_map();
  DCHECK(IsFunctionMapOrSpecialBuiltin(map, builtin, context));

  Handle<SharedFunctionInfo> info =
      factory->NewSharedFunctionInfoForBuiltin(name, builtin);
  info->set_language_mode(LanguageMode::kStrict);

  return Factory::JSFunctionBuilder{isolate, info, context}
      .set_map(map)
      .Build();
}

V8_NOINLINE Handle<JSFunction> CreateFunction(
    Isolate* isolate, Handle<String> name, InstanceType type, int instance_size,
    int inobject_properties, Handle<HeapObject> prototype, Builtin builtin) {
  DCHECK(Builtins::HasJSLinkage(builtin));

  Handle<JSFunction> result = CreateFunctionForBuiltinWithPrototype(
      isolate, name, builtin, prototype, type, instance_size,
      inobject_properties, IMMUTABLE);

  // Make the JSFunction's prototype object fast.
  JSObject::MakePrototypesFast(handle(result->prototype(), isolate),
                               kStartAtReceiver, isolate);

  // Make the resulting JSFunction object fast.
  JSObject::MakePrototypesFast(result, kStartAtReceiver, isolate);
  result->shared().set_native(true);
  return result;
}

V8_NOINLINE Handle<JSFunction> CreateFunction(
    Isolate* isolate, const char* name, InstanceType type, int instance_size,
    int inobject_properties, Handle<HeapObject> prototype, Builtin builtin) {
  return CreateFunction(isolate,
                        isolate->factory()->InternalizeUtf8String(name), type,
                        instance_size, inobject_properties, prototype, builtin);
}

V8_NOINLINE Handle<JSFunction> InstallFunction(
    Isolate* isolate, Handle<JSObject> target, Handle<String> name,
    InstanceType type, int instance_size, int inobject_properties,
    Handle<HeapObject> prototype, Builtin call) {
  DCHECK(Builtins::HasJSLinkage(call));
  Handle<JSFunction> function = CreateFunction(
      isolate, name, type, instance_size, inobject_properties, prototype, call);
  JSObject::AddProperty(isolate, target, name, function, DONT_ENUM);
  return function;
}

V8_NOINLINE Handle<JSFunction> InstallFunction(
    Isolate* isolate, Handle<JSObject> target, const char* name,
    InstanceType type, int instance_size, int inobject_properties,
    Handle<HeapObject> prototype, Builtin call) {
  return InstallFunction(isolate, target,
                         isolate->factory()->InternalizeUtf8String(name), type,
                         instance_size, inobject_properties, prototype, call);
}

// This installs an instance type (|constructor_type|) on the constructor map
// which will be used for protector cell checks -- this is separate from |type|
// which is used to set the instance type of the object created by this
// constructor. If protector cell checks are not required, continue to use the
// default JS_FUNCTION_TYPE by directly calling InstallFunction.
V8_NOINLINE Handle<JSFunction> InstallConstructor(
    Isolate* isolate, Handle<JSObject> target, const char* name,
    InstanceType type, int instance_size, int inobject_properties,
    Handle<HeapObject> prototype, Builtin call, InstanceType constructor_type) {
  Handle<JSFunction> function = InstallFunction(
      isolate, target, isolate->factory()->InternalizeUtf8String(name), type,
      instance_size, inobject_properties, prototype, call);
  DCHECK(InstanceTypeChecker::IsJSFunction(constructor_type));
  function->map().set_instance_type(constructor_type);
  return function;
}

V8_NOINLINE Handle<JSFunction> SimpleCreateFunction(Isolate* isolate,
                                                    Handle<String> name,
                                                    Builtin call, int len,
                                                    bool adapt) {
  DCHECK(Builtins::HasJSLinkage(call));
  name = String::Flatten(isolate, name, AllocationType::kOld);
  Handle<JSFunction> fun =
      CreateFunctionForBuiltinWithoutPrototype(isolate, name, call);
  // Make the resulting JSFunction object fast.
  JSObject::MakePrototypesFast(fun, kStartAtReceiver, isolate);
  fun->shared().set_native(true);

  if (adapt) {
    fun->shared().set_internal_formal_parameter_count(len);
  } else {
    fun->shared().DontAdaptArguments();
  }
  fun->shared().set_length(len);
  return fun;
}

V8_NOINLINE Handle<JSFunction> InstallFunctionWithBuiltinId(
    Isolate* isolate, Handle<JSObject> base, const char* name, Builtin call,
    int len, bool adapt) {
  Handle<String> internalized_name =
      isolate->factory()->InternalizeUtf8String(name);
  Handle<JSFunction> fun =
      SimpleCreateFunction(isolate, internalized_name, call, len, adapt);
  JSObject::AddProperty(isolate, base, internalized_name, fun, DONT_ENUM);
  return fun;
}

V8_NOINLINE Handle<JSFunction> SimpleInstallFunction(
    Isolate* isolate, Handle<JSObject> base, const char* name, Builtin call,
    int len, bool adapt, PropertyAttributes attrs = DONT_ENUM) {
  // Although function name does not have to be internalized the property name
  // will be internalized during property addition anyway, so do it here now.
  Handle<String> internalized_name =
      isolate->factory()->InternalizeUtf8String(name);
  Handle<JSFunction> fun =
      SimpleCreateFunction(isolate, internalized_name, call, len, adapt);
  JSObject::AddProperty(isolate, base, internalized_name, fun, attrs);
  return fun;
}

V8_NOINLINE Handle<JSFunction> InstallFunctionAtSymbol(
    Isolate* isolate, Handle<JSObject> base, Handle<Symbol> symbol,
    const char* symbol_string, Builtin call, int len, bool adapt,
    PropertyAttributes attrs = DONT_ENUM) {
  Handle<String> internalized_symbol =
      isolate->factory()->InternalizeUtf8String(symbol_string);
  Handle<JSFunction> fun =
      SimpleCreateFunction(isolate, internalized_symbol, call, len, adapt);
  JSObject::AddProperty(isolate, base, symbol, fun, attrs);
  return fun;
}

V8_NOINLINE void SimpleInstallGetterSetter(Isolate* isolate,
                                           Handle<JSObject> base,
                                           Handle<String> name,
                                           Builtin call_getter,
                                           Builtin call_setter) {
  Handle<String> getter_name =
      Name::ToFunctionName(isolate, name, isolate->factory()->get_string())
          .ToHandleChecked();
  Handle<JSFunction> getter =
      SimpleCreateFunction(isolate, getter_name, call_getter, 0, true);

  Handle<String> setter_name =
      Name::ToFunctionName(isolate, name, isolate->factory()->set_string())
          .ToHandleChecked();
  Handle<JSFunction> setter =
      SimpleCreateFunction(isolate, setter_name, call_setter, 1, true);

  JSObject::DefineAccessor(base, name, getter, setter, DONT_ENUM).Check();
}

void SimpleInstallGetterSetter(Isolate* isolate, Handle<JSObject> base,
                               const char* name, Builtin call_getter,
                               Builtin call_setter) {
  SimpleInstallGetterSetter(isolate, base,
                            isolate->factory()->InternalizeUtf8String(name),
                            call_getter, call_setter);
}

V8_NOINLINE Handle<JSFunction> SimpleInstallGetter(Isolate* isolate,
                                                   Handle<JSObject> base,
                                                   Handle<Name> name,
                                                   Handle<Name> property_name,
                                                   Builtin call, bool adapt) {
  Handle<String> getter_name =
      Name::ToFunctionName(isolate, name, isolate->factory()->get_string())
          .ToHandleChecked();
  Handle<JSFunction> getter =
      SimpleCreateFunction(isolate, getter_name, call, 0, adapt);

  Handle<Object> setter = isolate->factory()->undefined_value();

  JSObject::DefineAccessor(base, property_name, getter, setter, DONT_ENUM)
      .Check();

  return getter;
}

V8_NOINLINE Handle<JSFunction> SimpleInstallGetter(Isolate* isolate,
                                                   Handle<JSObject> base,
                                                   Handle<Name> name,
                                                   Builtin call, bool adapt) {
  return SimpleInstallGetter(isolate, base, name, name, call, adapt);
}

V8_NOINLINE void InstallConstant(Isolate* isolate, Handle<JSObject> holder,
                                 const char* name, Handle<Object> value) {
  JSObject::AddProperty(
      isolate, holder, isolate->factory()->InternalizeUtf8String(name), value,
      static_cast<PropertyAttributes>(DONT_DELETE | DONT_ENUM | READ_ONLY));
}

V8_NOINLINE void InstallTrueValuedProperty(Isolate* isolate,
                                           Handle<JSObject> holder,
                                           const char* name) {
  JSObject::AddProperty(isolate, holder,
                        isolate->factory()->InternalizeUtf8String(name),
                        isolate->factory()->true_value(), NONE);
}

V8_NOINLINE void InstallSpeciesGetter(Isolate* isolate,
                                      Handle<JSFunction> constructor) {
  Factory* factory = isolate->factory();
  // TODO(adamk): We should be able to share a SharedFunctionInfo
  // between all these JSFunctins.
  SimpleInstallGetter(isolate, constructor, factory->symbol_species_string(),
                      factory->species_symbol(), Builtin::kReturnReceiver,
                      true);
}

V8_NOINLINE void InstallToStringTag(Isolate* isolate, Handle<JSObject> holder,
                                    Handle<String> value) {
  JSObject::AddProperty(isolate, holder,
                        isolate->factory()->to_string_tag_symbol(), value,
                        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));
}

void InstallToStringTag(Isolate* isolate, Handle<JSObject> holder,
                        const char* value) {
  InstallToStringTag(isolate, holder,
                     isolate->factory()->InternalizeUtf8String(value));
}

}  // namespace

Handle<JSFunction> Genesis::CreateEmptyFunction() {
  // Allocate the function map first and then patch the prototype later.
  Handle<Map> empty_function_map = factory()->CreateSloppyFunctionMap(
      FUNCTION_WITHOUT_PROTOTYPE, MaybeHandle<JSFunction>());
  empty_function_map->set_is_prototype_map(true);
  DCHECK(!empty_function_map->is_dictionary_map());

  // Allocate the empty function as the prototype for function according to
  // ES#sec-properties-of-the-function-prototype-object
  Handle<JSFunction> empty_function =
      CreateFunctionForBuiltin(isolate(), factory()->empty_string(),
                               empty_function_map, Builtin::kEmptyFunction);
  native_context()->set_empty_function(*empty_function);

  // --- E m p t y ---
  Handle<String> source = factory()->NewStringFromStaticChars("() {}");
  Handle<Script> script = factory()->NewScript(source);
  script->set_type(Script::TYPE_NATIVE);
  Handle<WeakFixedArray> infos = factory()->NewWeakFixedArray(2);
  script->set_shared_function_infos(*infos);
  empty_function->shared().set_raw_scope_info(
      ReadOnlyRoots(isolate()).empty_function_scope_info());
  empty_function->shared().DontAdaptArguments();
  empty_function->shared().SetScript(ReadOnlyRoots(isolate()), *script, 1);

  return empty_function;
}

void Genesis::CreateSloppyModeFunctionMaps(Handle<JSFunction> empty) {
  Factory* factory = isolate_->factory();
  Handle<Map> map;

  //
  // Allocate maps for sloppy functions without prototype.
  //
  map = factory->CreateSloppyFunctionMap(FUNCTION_WITHOUT_PROTOTYPE, empty);
  native_context()->set_sloppy_function_without_prototype_map(*map);

  //
  // Allocate maps for sloppy functions with readonly prototype.
  //
  map =
      factory->CreateSloppyFunctionMap(FUNCTION_WITH_READONLY_PROTOTYPE, empty);
  native_context()->set_sloppy_function_with_readonly_prototype_map(*map);

  //
  // Allocate maps for sloppy functions with writable prototype.
  //
  map = factory->CreateSloppyFunctionMap(FUNCTION_WITH_WRITEABLE_PROTOTYPE,
                                         empty);
  native_context()->set_sloppy_function_map(*map);

  map = factory->CreateSloppyFunctionMap(
      FUNCTION_WITH_NAME_AND_WRITEABLE_PROTOTYPE, empty);
  native_context()->set_sloppy_function_with_name_map(*map);
}

Handle<JSFunction> Genesis::GetThrowTypeErrorIntrinsic() {
  if (!restricted_properties_thrower_.is_null()) {
    return restricted_properties_thrower_;
  }
  Handle<String> name = factory()->empty_string();
  Handle<JSFunction> function = CreateFunctionForBuiltinWithoutPrototype(
      isolate(), name, Builtin::kStrictPoisonPillThrower);
  function->shared().DontAdaptArguments();

  // %ThrowTypeError% must have a name property with an empty string value. Per
  // spec, ThrowTypeError's name is non-configurable, unlike ordinary functions'
  // name property. To redefine it to be non-configurable, use
  // SetOwnPropertyIgnoreAttributes.
  JSObject::SetOwnPropertyIgnoreAttributes(
      function, factory()->name_string(), factory()->empty_string(),
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY))
      .Assert();

  // length needs to be non configurable.
  Handle<Object> value(Smi::FromInt(function->length()), isolate());
  JSObject::SetOwnPropertyIgnoreAttributes(
      function, factory()->length_string(), value,
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY))
      .Assert();

  if (JSObject::PreventExtensions(function, kThrowOnError).IsNothing()) {
    DCHECK(false);
  }

  JSObject::MigrateSlowToFast(function, 0, "Bootstrapping");

  restricted_properties_thrower_ = function;
  return function;
}

void Genesis::CreateStrictModeFunctionMaps(Handle<JSFunction> empty) {
  Factory* factory = isolate_->factory();
  Handle<Map> map;

  //
  // Allocate maps for strict functions without prototype.
  //
  map = factory->CreateStrictFunctionMap(FUNCTION_WITHOUT_PROTOTYPE, empty);
  native_context()->set_strict_function_without_prototype_map(*map);

  map = factory->CreateStrictFunctionMap(METHOD_WITH_NAME, empty);
  native_context()->set_method_with_name_map(*map);

  //
  // Allocate maps for strict functions with writable prototype.
  //
  map = factory->CreateStrictFunctionMap(FUNCTION_WITH_WRITEABLE_PROTOTYPE,
                                         empty);
  native_context()->set_strict_function_map(*map);

  map = factory->CreateStrictFunctionMap(
      FUNCTION_WITH_NAME_AND_WRITEABLE_PROTOTYPE, empty);
  native_context()->set_strict_function_with_name_map(*map);

  //
  // Allocate maps for strict functions with readonly prototype.
  //
  map =
      factory->CreateStrictFunctionMap(FUNCTION_WITH_READONLY_PROTOTYPE, empty);
  native_context()->set_strict_function_with_readonly_prototype_map(*map);

  //
  // Allocate map for class functions.
  //
  map = factory->CreateClassFunctionMap(empty);
  native_context()->set_class_function_map(*map);

  // Now that the strict mode function map is available, set up the
  // restricted "arguments" and "caller" getters.
  AddRestrictedFunctionProperties(empty);
}

void Genesis::CreateObjectFunction(Handle<JSFunction> empty_function) {
  Factory* factory = isolate_->factory();

  // --- O b j e c t ---
  int inobject_properties = JSObject::kInitialGlobalObjectUnusedPropertiesCount;
  int instance_size = JSObject::kHeaderSize + kTaggedSize * inobject_properties;

  Handle<JSFunction> object_fun = CreateFunction(
      isolate_, factory->Object_string(), JS_OBJECT_TYPE, instance_size,
      inobject_properties, factory->null_value(), Builtin::kObjectConstructor);
  object_fun->shared().set_length(1);
  object_fun->shared().DontAdaptArguments();
  native_context()->set_object_function(*object_fun);

  {
    // Finish setting up Object function's initial map.
    Map initial_map = object_fun->initial_map();
    initial_map.set_elements_kind(HOLEY_ELEMENTS);
  }

  // Allocate a new prototype for the object function.
  Handle<JSObject> object_function_prototype =
      factory->NewFunctionPrototype(object_fun);

  Handle<Map> map =
      Map::Copy(isolate(), handle(object_function_prototype->map(), isolate()),
                "EmptyObjectPrototype");
  map->set_is_prototype_map(true);
  // Ban re-setting Object.prototype.__proto__ to prevent Proxy security bug
  map->set_is_immutable_proto(true);
  object_function_prototype->set_map(*map);

  // Complete setting up empty function.
  {
    Handle<Map> empty_function_map(empty_function->map(), isolate_);
    Map::SetPrototype(isolate(), empty_function_map, object_function_prototype);
  }

  native_context()->set_initial_object_prototype(*object_function_prototype);
  JSFunction::SetPrototype(object_fun, object_function_prototype);
  object_function_prototype->map().set_instance_type(JS_OBJECT_PROTOTYPE_TYPE);
  {
    // Set up slow map for Object.create(null) instances without in-object
    // properties.
    Handle<Map> map(object_fun->initial_map(), isolate_);
    map = Map::CopyInitialMapNormalized(isolate(), map);
    Map::SetPrototype(isolate(), map, factory->null_value());
    native_context()->set_slow_object_with_null_prototype_map(*map);

    // Set up slow map for literals with too many properties.
    map = Map::Copy(isolate(), map, "slow_object_with_object_prototype_map");
    Map::SetPrototype(isolate(), map, object_function_prototype);
    native_context()->set_slow_object_with_object_prototype_map(*map);
  }
}

namespace {

Handle<Map> CreateNonConstructorMap(Isolate* isolate, Handle<Map> source_map,
                                    Handle<JSObject> prototype,
                                    const char* reason) {
  Handle<Map> map = Map::Copy(isolate, source_map, reason);
  // Ensure the resulting map has prototype slot (it is necessary for storing
  // inital map even when the prototype property is not required).
  if (!map->has_prototype_slot()) {
    // Re-set the unused property fields after changing the instance size.
    int unused_property_fields = map->UnusedPropertyFields();
    map->set_instance_size(map->instance_size() + kTaggedSize);
    // The prototype slot shifts the in-object properties area by one slot.
    map->SetInObjectPropertiesStartInWords(
        map->GetInObjectPropertiesStartInWords() + 1);
    map->set_has_prototype_slot(true);
    map->SetInObjectUnusedPropertyFields(unused_property_fields);
  }
  map->set_is_constructor(false);
  Map::SetPrototype(isolate, map, prototype);
  return map;
}

}  // namespace

void Genesis::CreateIteratorMaps(Handle<JSFunction> empty) {
  // Create iterator-related meta-objects.
  Handle<JSObject> iterator_prototype = factory()->NewJSObject(
      isolate()->object_function(), AllocationType::kOld);

  InstallFunctionAtSymbol(isolate(), iterator_prototype,
                          factory()->iterator_symbol(), "[Symbol.iterator]",
                          Builtin::kReturnReceiver, 0, true);
  native_context()->set_initial_iterator_prototype(*iterator_prototype);
  CHECK_NE(iterator_prototype->map().ptr(),
           isolate_->initial_object_prototype()->map().ptr());
  iterator_prototype->map().set_instance_type(JS_ITERATOR_PROTOTYPE_TYPE);

  Handle<JSObject> generator_object_prototype = factory()->NewJSObject(
      isolate()->object_function(), AllocationType::kOld);
  native_context()->set_initial_generator_prototype(
      *generator_object_prototype);
  JSObject::ForceSetPrototype(isolate(), generator_object_prototype,
                              iterator_prototype);
  Handle<JSObject> generator_function_prototype = factory()->NewJSObject(
      isolate()->object_function(), AllocationType::kOld);
  JSObject::ForceSetPrototype(isolate(), generator_function_prototype, empty);

  InstallToStringTag(isolate(), generator_function_prototype,
                     "GeneratorFunction");
  JSObject::AddProperty(isolate(), generator_function_prototype,
                        factory()->prototype_string(),
                        generator_object_prototype,
                        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

  JSObject::AddProperty(isolate(), generator_object_prototype,
                        factory()->constructor_string(),
                        generator_function_prototype,
                        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));
  InstallToStringTag(isolate(), generator_object_prototype, "Generator");
  SimpleInstallFunction(isolate(), generator_object_prototype, "next",
                        Builtin::kGeneratorPrototypeNext, 1, false);
  SimpleInstallFunction(isolate(), generator_object_prototype, "return",
                        Builtin::kGeneratorPrototypeReturn, 1, false);
  SimpleInstallFunction(isolate(), generator_object_prototype, "throw",
                        Builtin::kGeneratorPrototypeThrow, 1, false);

  // Internal version of generator_prototype_next, flagged as non-native such
  // that it doesn't show up in Error traces.
  Handle<JSFunction> generator_next_internal =
      SimpleCreateFunction(isolate(), factory()->next_string(),
                           Builtin::kGeneratorPrototypeNext, 1, false);
  generator_next_internal->shared().set_native(false);
  native_context()->set_generator_next_internal(*generator_next_internal);

  // Internal version of async module functions, flagged as non-native such
  // that they don't show up in Error traces.
  {
    Handle<JSFunction> async_module_evaluate_internal =
        SimpleCreateFunction(isolate(), factory()->next_string(),
                             Builtin::kAsyncModuleEvaluate, 1, false);
    async_module_evaluate_internal->shared().set_native(false);
    native_context()->set_async_module_evaluate_internal(
        *async_module_evaluate_internal);

    Handle<JSFunction> call_async_module_fulfilled =
        SimpleCreateFunction(isolate(), factory()->empty_string(),
                             Builtin::kCallAsyncModuleFulfilled, 1, false);
    call_async_module_fulfilled->shared().set_native(false);
    native_context()->set_call_async_module_fulfilled(
        *call_async_module_fulfilled);

    Handle<JSFunction> call_async_module_rejected =
        SimpleCreateFunction(isolate(), factory()->empty_string(),
                             Builtin::kCallAsyncModuleRejected, 1, false);
    call_async_module_rejected->shared().set_native(false);
    native_context()->set_call_async_module_rejected(
        *call_async_module_rejected);
  }

  // Create maps for generator functions and their prototypes.  Store those
  // maps in the native context. The "prototype" property descriptor is
  // writable, non-enumerable, and non-configurable (as per ES6 draft
  // 04-14-15, section 25.2.4.3).
  // Generator functions do not have "caller" or "arguments" accessors.
  Handle<Map> map;
  map = CreateNonConstructorMap(isolate(), isolate()->strict_function_map(),
                                generator_function_prototype,
                                "GeneratorFunction");
  native_context()->set_generator_function_map(*map);

  map = CreateNonConstructorMap(
      isolate(), isolate()->strict_function_with_name_map(),
      generator_function_prototype, "GeneratorFunction with name");
  native_context()->set_generator_function_with_name_map(*map);

  Handle<JSFunction> object_function(native_context()->object_function(),
                                     isolate());
  Handle<Map> generator_object_prototype_map = Map::Create(isolate(), 0);
  Map::SetPrototype(isolate(), generator_object_prototype_map,
                    generator_object_prototype);
  native_context()->set_generator_object_prototype_map(
      *generator_object_prototype_map);
}

void Genesis::CreateAsyncIteratorMaps(Handle<JSFunction> empty) {
  // %AsyncIteratorPrototype%
  // proposal-async-iteration/#sec-asynciteratorprototype
  Handle<JSObject> async_iterator_prototype = factory()->NewJSObject(
      isolate()->object_function(), AllocationType::kOld);

  InstallFunctionAtSymbol(
      isolate(), async_iterator_prototype, factory()->async_iterator_symbol(),
      "[Symbol.asyncIterator]", Builtin::kReturnReceiver, 0, true);
  native_context()->set_initial_async_iterator_prototype(
      *async_iterator_prototype);

  // %AsyncFromSyncIteratorPrototype%
  // proposal-async-iteration/#sec-%asyncfromsynciteratorprototype%-object
  Handle<JSObject> async_from_sync_iterator_prototype = factory()->NewJSObject(
      isolate()->object_function(), AllocationType::kOld);
  SimpleInstallFunction(isolate(), async_from_sync_iterator_prototype, "next",
                        Builtin::kAsyncFromSyncIteratorPrototypeNext, 1, false);
  SimpleInstallFunction(isolate(), async_from_sync_iterator_prototype, "return",
                        Builtin::kAsyncFromSyncIteratorPrototypeReturn, 1,
                        false);
  SimpleInstallFunction(isolate(), async_from_sync_iterator_prototype, "throw",
                        Builtin::kAsyncFromSyncIteratorPrototypeThrow, 1,
                        false);

  InstallToStringTag(isolate(), async_from_sync_iterator_prototype,
                     "Async-from-Sync Iterator");

  JSObject::ForceSetPrototype(isolate(), async_from_sync_iterator_prototype,
                              async_iterator_prototype);

  Handle<Map> async_from_sync_iterator_map = factory()->NewMap(
      JS_ASYNC_FROM_SYNC_ITERATOR_TYPE, JSAsyncFromSyncIterator::kHeaderSize);
  Map::SetPrototype(isolate(), async_from_sync_iterator_map,
                    async_from_sync_iterator_prototype);
  native_context()->set_async_from_sync_iterator_map(
      *async_from_sync_iterator_map);

  // Async Generators
  Handle<JSObject> async_generator_object_prototype = factory()->NewJSObject(
      isolate()->object_function(), AllocationType::kOld);
  Handle<JSObject> async_generator_function_prototype = factory()->NewJSObject(
      isolate()->object_function(), AllocationType::kOld);

  // %AsyncGenerator% / %AsyncGeneratorFunction%.prototype
  JSObject::ForceSetPrototype(isolate(), async_generator_function_prototype,
                              empty);

  // The value of AsyncGeneratorFunction.prototype.prototype is the
  //     %AsyncGeneratorPrototype% intrinsic object.
  // This property has the attributes
  //     { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: true }.
  JSObject::AddProperty(isolate(), async_generator_function_prototype,
                        factory()->prototype_string(),
                        async_generator_object_prototype,
                        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));
  JSObject::AddProperty(isolate(), async_generator_object_prototype,
                        factory()->constructor_string(),
                        async_generator_function_prototype,
                        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));
  InstallToStringTag(isolate(), async_generator_function_prototype,
                     "AsyncGeneratorFunction");

  // %AsyncGeneratorPrototype%
  JSObject::ForceSetPrototype(isolate(), async_generator_object_prototype,
                              async_iterator_prototype);
  native_context()->set_initial_async_generator_prototype(
      *async_generator_object_prototype);

  InstallToStringTag(isolate(), async_generator_object_prototype,
                     "AsyncGenerator");
  SimpleInstallFunction(isolate(), async_generator_object_prototype, "next",
                        Builtin::kAsyncGeneratorPrototypeNext, 1, false);
  SimpleInstallFunction(isolate(), async_generator_object_prototype, "return",
                        Builtin::kAsyncGeneratorPrototypeReturn, 1, false);
  SimpleInstallFunction(isolate(), async_generator_object_prototype, "throw",
                        Builtin::kAsyncGeneratorPrototypeThrow, 1, false);

  // Create maps for generator functions and their prototypes.  Store those
  // maps in the native context. The "prototype" property descriptor is
  // writable, non-enumerable, and non-configurable (as per ES6 draft
  // 04-14-15, section 25.2.4.3).
  // Async Generator functions do not have "caller" or "arguments" accessors.
  Handle<Map> map;
  map = CreateNonConstructorMap(isolate(), isolate()->strict_function_map(),
                                async_generator_function_prototype,
                                "AsyncGeneratorFunction");
  native_context()->set_async_generator_function_map(*map);

  map = CreateNonConstructorMap(
      isolate(), isolate()->strict_function_with_name_map(),
      async_generator_function_prototype, "AsyncGeneratorFunction with name");
  native_context()->set_async_generator_function_with_name_map(*map);

  Handle<JSFunction> object_function(native_context()->object_function(),
                                     isolate());
  Handle<Map> async_generator_object_prototype_map = Map::Create(isolate(), 0);
  Map::SetPrototype(isolate(), async_generator_object_prototype_map,
                    async_generator_object_prototype);
  native_context()->set_async_generator_object_prototype_map(
      *async_generator_object_prototype_map);
}

void Genesis::CreateAsyncFunctionMaps(Handle<JSFunction> empty) {
  // %AsyncFunctionPrototype% intrinsic
  Handle<JSObject> async_function_prototype = factory()->NewJSObject(
      isolate()->object_function(), AllocationType::kOld);
  JSObject::ForceSetPrototype(isolate(), async_function_prototype, empty);

  InstallToStringTag(isolate(), async_function_prototype, "AsyncFunction");

  Handle<Map> map =
      Map::Copy(isolate(), isolate()->strict_function_without_prototype_map(),
                "AsyncFunction");
  Map::SetPrototype(isolate(), map, async_function_prototype);
  native_context()->set_async_function_map(*map);

  map = Map::Copy(isolate(), isolate()->method_with_name_map(),
                  "AsyncFunction with name");
  Map::SetPrototype(isolate(), map, async_function_prototype);
  native_context()->set_async_function_with_name_map(*map);
}

void Genesis::CreateJSProxyMaps() {
  // Allocate maps for all Proxy types.
  // Next to the default proxy, we need maps indicating callable and
  // constructable proxies.
  Handle<Map> proxy_map = factory()->NewMap(JS_PROXY_TYPE, JSProxy::kSize,
                                            TERMINAL_FAST_ELEMENTS_KIND);
  proxy_map->set_is_dictionary_map(true);
  proxy_map->set_may_have_interesting_symbols(true);
  native_context()->set_proxy_map(*proxy_map);

  Handle<Map> proxy_callable_map =
      Map::Copy(isolate_, proxy_map, "callable Proxy");
  proxy_callable_map->set_is_callable(true);
  native_context()->set_proxy_callable_map(*proxy_callable_map);
  proxy_callable_map->SetConstructor(native_context()->function_function());

  Handle<Map> proxy_constructor_map =
      Map::Copy(isolate_, proxy_callable_map, "constructor Proxy");
  proxy_constructor_map->set_is_constructor(true);
  native_context()->set_proxy_constructor_map(*proxy_constructor_map);

  {
    Handle<Map> map =
        factory()->NewMap(JS_OBJECT_TYPE, JSProxyRevocableResult::kSize,
                          TERMINAL_FAST_ELEMENTS_KIND, 2);
    Map::EnsureDescriptorSlack(isolate_, map, 2);

    {  // proxy
      Descriptor d = Descriptor::DataField(isolate(), factory()->proxy_string(),
                                           JSProxyRevocableResult::kProxyIndex,
                                           NONE, Representation::Tagged());
      map->AppendDescriptor(isolate(), &d);
    }
    {  // revoke
      Descriptor d = Descriptor::DataField(
          isolate(), factory()->revoke_string(),
          JSProxyRevocableResult::kRevokeIndex, NONE, Representation::Tagged());
      map->AppendDescriptor(isolate(), &d);
    }

    Map::SetPrototype(isolate(), map, isolate()->initial_object_prototype());
    map->SetConstructor(native_context()->object_function());

    native_context()->set_proxy_revocable_result_map(*map);
  }
}

namespace {
void ReplaceAccessors(Isolate* isolate, Handle<Map> map, Handle<String> name,
                      PropertyAttributes attributes,
                      Handle<AccessorPair> accessor_pair) {
  DescriptorArray descriptors = map->instance_descriptors(isolate);
  InternalIndex entry = descriptors.SearchWithCache(isolate, *name, *map);
  Descriptor d = Descriptor::AccessorConstant(name, accessor_pair, attributes);
  descriptors.Replace(entry, &d);
}
}  // namespace

void Genesis::AddRestrictedFunctionProperties(Handle<JSFunction> empty) {
  PropertyAttributes rw_attribs = static_cast<PropertyAttributes>(DONT_ENUM);
  Handle<JSFunction> thrower = GetThrowTypeErrorIntrinsic();
  Handle<AccessorPair> accessors = factory()->NewAccessorPair();
  accessors->set_getter(*thrower);
  accessors->set_setter(*thrower);

  Handle<Map> map(empty->map(), isolate());
  ReplaceAccessors(isolate(), map, factory()->arguments_string(), rw_attribs,
                   accessors);
  ReplaceAccessors(isolate(), map, factory()->caller_string(), rw_attribs,
                   accessors);
}

static void AddToWeakNativeContextList(Isolate* isolate, Context context) {
  DCHECK(context.IsNativeContext());
  Heap* heap = isolate->heap();
#ifdef DEBUG
  {
    DCHECK(context.next_context_link().IsUndefined(isolate));
    // Check that context is not in the list yet.
    for (Object current = heap->native_contexts_list();
         !current.IsUndefined(isolate);
         current = Context::cast(current).next_context_link()) {
      DCHECK(current != context);
    }
  }
#endif
  context.set(Context::NEXT_CONTEXT_LINK, heap->native_contexts_list(),
              UPDATE_WEAK_WRITE_BARRIER);
  heap->set_native_contexts_list(context);
}

void Genesis::CreateRoots() {
  // Allocate the native context FixedArray first and then patch the
  // closure and extension object later (we need the empty function
  // and the global object, but in order to create those, we need the
  // native context).
  native_context_ = factory()->NewNativeContext();

  AddToWeakNativeContextList(isolate(), *native_context());
  isolate()->set_context(*native_context());

  // Allocate the message listeners object.
  {
    Handle<TemplateList> list = TemplateList::New(isolate(), 1);
    native_context()->set_message_listeners(*list);
  }
}

void Genesis::InstallGlobalThisBinding() {
  Handle<ScriptContextTable> script_contexts(
      native_context()->script_context_table(), isolate());
  Handle<ScopeInfo> scope_info =
      ReadOnlyRoots(isolate()).global_this_binding_scope_info_handle();
  Handle<Context> context =
      factory()->NewScriptContext(native_context(), scope_info);

  // Go ahead and hook it up while we're at it.
  int slot = scope_info->ReceiverContextSlotIndex();
  DCHECK_EQ(slot, Context::MIN_CONTEXT_SLOTS);
  context->set(slot, native_context()->global_proxy());

  Handle<ScriptContextTable> new_script_contexts =
      ScriptContextTable::Extend(script_contexts, context);
  native_context()->set_script_context_table(*new_script_contexts);
}

Handle<JSGlobalObject> Genesis::CreateNewGlobals(
    v8::Local<v8::ObjectTemplate> global_proxy_template,
    Handle<JSGlobalProxy> global_proxy) {
  // The argument global_proxy_template aka data is an ObjectTemplateInfo.
  // It has a constructor pointer that points at global_constructor which is a
  // FunctionTemplateInfo.
  // The global_proxy_constructor is used to (re)initialize the
  // global_proxy. The global_proxy_constructor also has a prototype_template
  // pointer that points at js_global_object_template which is an
  // ObjectTemplateInfo.
  // That in turn has a constructor pointer that points at
  // js_global_object_constructor which is a FunctionTemplateInfo.
  // js_global_object_constructor is used to make js_global_object_function
  // js_global_object_function is used to make the new global_object.
  //
  // --- G l o b a l ---
  // Step 1: Create a fresh JSGlobalObject.
  Handle<JSFunction> js_global_object_function;
  Handle<ObjectTemplateInfo> js_global_object_template;
  if (!global_proxy_template.IsEmpty()) {
    // Get prototype template of the global_proxy_template.
    Handle<ObjectTemplateInfo> data =
        v8::Utils::OpenHandle(*global_proxy_template);
    Handle<FunctionTemplateInfo> global_constructor =
        Handle<FunctionTemplateInfo>(
            FunctionTemplateInfo::cast(data->constructor()), isolate());
    Handle<Object> proto_template(global_constructor->GetPrototypeTemplate(),
                                  isolate());
    if (!proto_template->IsUndefined(isolate())) {
      js_global_object_template =
          Handle<ObjectTemplateInfo>::cast(proto_template);
    }
  }

  if (js_global_object_template.is_null()) {
    Handle<String> name = factory()->empty_string();
    Handle<JSObject> prototype =
        factory()->NewFunctionPrototype(isolate()->object_function());
    js_global_object_function = CreateFunctionForBuiltinWithPrototype(
        isolate(), name, Builtin::kIllegal, prototype, JS_GLOBAL_OBJECT_TYPE,
        JSGlobalObject::kHeaderSize, 0, MUTABLE);
#ifdef DEBUG
    LookupIterator it(isolate(), prototype, factory()->constructor_string(),
                      LookupIterator::OWN_SKIP_INTERCEPTOR);
    Handle<Object> value = Object::GetProperty(&it).ToHandleChecked();
    DCHECK(it.IsFound());
    DCHECK_EQ(*isolate()->object_function(), *value);
#endif
  } else {
    Handle<FunctionTemplateInfo> js_global_object_constructor(
        FunctionTemplateInfo::cast(js_global_object_template->constructor()),
        isolate());
    js_global_object_function = ApiNatives::CreateApiFunction(
        isolate(), isolate()->native_context(), js_global_object_constructor,
        factory()->the_hole_value(), JS_GLOBAL_OBJECT_TYPE);
  }

  js_global_object_function->initial_map().set_is_prototype_map(true);
  js_global_object_function->initial_map().set_is_dictionary_map(true);
  js_global_object_function->initial_map().set_may_have_interesting_symbols(
      true);
  Handle<JSGlobalObject> global_object =
      factory()->NewJSGlobalObject(js_global_object_function);

  // Step 2: (re)initialize the global proxy object.
  Handle<JSFunction> global_proxy_function;
  if (global_proxy_template.IsEmpty()) {
    Handle<String> name = factory()->empty_string();
    global_proxy_function = CreateFunctionForBuiltinWithPrototype(
        isolate(), name, Builtin::kIllegal, factory()->the_hole_value(),
        JS_GLOBAL_PROXY_TYPE, JSGlobalProxy::SizeWithEmbedderFields(0), 0,
        MUTABLE);
  } else {
    Handle<ObjectTemplateInfo> data =
        v8::Utils::OpenHandle(*global_proxy_template);
    Handle<FunctionTemplateInfo> global_constructor(
        FunctionTemplateInfo::cast(data->constructor()), isolate());
    global_proxy_function = ApiNatives::CreateApiFunction(
        isolate(), isolate()->native_context(), global_constructor,
        factory()->the_hole_value(), JS_GLOBAL_PROXY_TYPE);
  }
  global_proxy_function->initial_map().set_is_access_check_needed(true);
  global_proxy_function->initial_map().set_may_have_interesting_symbols(true);
  native_context()->set_global_proxy_function(*global_proxy_function);

  // Set global_proxy.__proto__ to js_global after ConfigureGlobalObjects
  // Return the global proxy.

  factory()->ReinitializeJSGlobalProxy(global_proxy, global_proxy_function);

  // Set the native context for the global object.
  global_object->set_native_context(*native_context());
  global_object->set_global_proxy(*global_proxy);
  // Set the native context of the global proxy.
  global_proxy->set_native_context(*native_context());
  // Set the global proxy of the native context. If the native context has been
  // deserialized, the global proxy is already correctly set up by the
  // deserializer. Otherwise it's undefined.
  DCHECK(native_context()
             ->get(Context::GLOBAL_PROXY_INDEX)
             .IsUndefined(isolate()) ||
         native_context()->global_proxy_object() == *global_proxy);
  native_context()->set_global_proxy_object(*global_proxy);

  return global_object;
}

void Genesis::HookUpGlobalProxy(Handle<JSGlobalProxy> global_proxy) {
  // Re-initialize the global proxy with the global proxy function from the
  // snapshot, and then set up the link to the native context.
  Handle<JSFunction> global_proxy_function(
      native_context()->global_proxy_function(), isolate());
  factory()->ReinitializeJSGlobalProxy(global_proxy, global_proxy_function);
  Handle<JSObject> global_object(
      JSObject::cast(native_context()->global_object()), isolate());
  JSObject::ForceSetPrototype(isolate(), global_proxy, global_object);
  global_proxy->set_native_context(*native_context());
  DCHECK(native_context()->global_proxy() == *global_proxy);
}

void Genesis::HookUpGlobalObject(Handle<JSGlobalObject> global_object) {
  Handle<JSGlobalObject> global_object_from_snapshot(
      JSGlobalObject::cast(native_context()->extension()), isolate());
  native_context()->set_extension(*global_object);
  native_context()->set_security_token(*global_object);

  TransferNamedProperties(global_object_from_snapshot, global_object);
  if (global_object_from_snapshot->HasDictionaryElements()) {
    JSObject::NormalizeElements(global_object);
  }
  DCHECK_EQ(global_object_from_snapshot->GetElementsKind(),
            global_object->GetElementsKind());
  TransferIndexedProperties(global_object_from_snapshot, global_object);
}

static void InstallWithIntrinsicDefaultProto(Isolate* isolate,
                                             Handle<JSFunction> function,
                                             int context_index) {
  Handle<Smi> index(Smi::FromInt(context_index), isolate);
  JSObject::AddProperty(isolate, function,
                        isolate->factory()->native_context_index_symbol(),
                        index, NONE);
  isolate->native_context()->set(context_index, *function, UPDATE_WRITE_BARRIER,
                                 kReleaseStore);
}

static void InstallError(Isolate* isolate, Handle<JSObject> global,
                         Handle<String> name, int context_index,
                         Builtin error_constructor = Builtin::kErrorConstructor,
                         int error_function_length = 1,
                         int in_object_properties = 2) {
  Factory* factory = isolate->factory();

  if (FLAG_harmony_error_cause) {
    in_object_properties += 1;
  }

  // Most Error objects consist of a message and a stack trace.
  // Reserve two in-object properties for these.
  const int kErrorObjectSize =
      JSObject::kHeaderSize + in_object_properties * kTaggedSize;
  Handle<JSFunction> error_fun = InstallFunction(
      isolate, global, name, JS_ERROR_TYPE, kErrorObjectSize,
      in_object_properties, factory->the_hole_value(), error_constructor);
  error_fun->shared().DontAdaptArguments();
  error_fun->shared().set_length(error_function_length);

  if (context_index == Context::ERROR_FUNCTION_INDEX) {
    SimpleInstallFunction(isolate, error_fun, "captureStackTrace",
                          Builtin::kErrorCaptureStackTrace, 2, false);
  }

  InstallWithIntrinsicDefaultProto(isolate, error_fun, context_index);

  {
    // Setup %XXXErrorPrototype%.
    Handle<JSObject> prototype(JSObject::cast(error_fun->instance_prototype()),
                               isolate);

    JSObject::AddProperty(isolate, prototype, factory->name_string(), name,
                          DONT_ENUM);
    JSObject::AddProperty(isolate, prototype, factory->message_string(),
                          factory->empty_string(), DONT_ENUM);

    if (FLAG_harmony_error_cause) {
      JSObject::AddProperty(isolate, prototype, factory->cause_string(),
                            factory->undefined_value(), DONT_ENUM);
    }

    if (context_index == Context::ERROR_FUNCTION_INDEX) {
      Handle<JSFunction> to_string_fun =
          SimpleInstallFunction(isolate, prototype, "toString",
                                Builtin::kErrorPrototypeToString, 0, true);
      isolate->native_context()->set_error_to_string(*to_string_fun);
      isolate->native_context()->set_initial_error_prototype(*prototype);
    } else {
      Handle<JSFunction> global_error = isolate->error_function();
      CHECK(JSReceiver::SetPrototype(error_fun, global_error, false,
                                     kThrowOnError)
                .FromMaybe(false));
      CHECK(JSReceiver::SetPrototype(prototype,
                                     handle(global_error->prototype(), isolate),
                                     false, kThrowOnError)
                .FromMaybe(false));
    }
  }

  Handle<Map> initial_map(error_fun->initial_map(), isolate);
  Map::EnsureDescriptorSlack(isolate, initial_map, 1);

  {
    Handle<AccessorInfo> info = factory->error_stack_accessor();
    Descriptor d = Descriptor::AccessorConstant(handle(info->name(), isolate),
                                                info, DONT_ENUM);
    initial_map->AppendDescriptor(isolate, &d);
  }
}

// This is only called if we are not using snapshots.  The equivalent
// work in the snapshot case is done in HookUpGlobalObject.
void Genesis::InitializeGlobal(Handle<JSGlobalObject> global_object,
                               Handle<JSFunction> empty_function) {
  // --- N a t i v e   C o n t e x t ---
  // Set extension and global object.
  native_context()->set_extension(*global_object);
  // Security setup: Set the security token of the native context to the global
  // object. This makes the security check between two different contexts fail
  // by default even in case of global object reinitialization.
  native_context()->set_security_token(*global_object);

  Factory* factory = isolate_->factory();

  {  // -- C o n t e x t
    Handle<Map> map =
        factory->NewMap(FUNCTION_CONTEXT_TYPE, kVariableSizeSentinel);
    map->set_native_context(*native_context());
    native_context()->set_function_context_map(*map);

    map = factory->NewMap(CATCH_CONTEXT_TYPE, kVariableSizeSentinel);
    map->set_native_context(*native_context());
    native_context()->set_catch_context_map(*map);

    map = factory->NewMap(WITH_CONTEXT_TYPE, kVariableSizeSentinel);
    map->set_native_context(*native_context());
    native_context()->set_with_context_map(*map);

    map = factory->NewMap(DEBUG_EVALUATE_CONTEXT_TYPE, kVariableSizeSentinel);
    map->set_native_context(*native_context());
    native_context()->set_debug_evaluate_context_map(*map);

    map = factory->NewMap(BLOCK_CONTEXT_TYPE, kVariableSizeSentinel);
    map->set_native_context(*native_context());
    native_context()->set_block_context_map(*map);

    map = factory->NewMap(MODULE_CONTEXT_TYPE, kVariableSizeSentinel);
    map->set_native_context(*native_context());
    native_context()->set_module_context_map(*map);

    map = factory->NewMap(AWAIT_CONTEXT_TYPE, kVariableSizeSentinel);
    map->set_native_context(*native_context());
    native_context()->set_await_context_map(*map);

    map = factory->NewMap(SCRIPT_CONTEXT_TYPE, kVariableSizeSentinel);
    map->set_native_context(*native_context());
    native_context()->set_script_context_map(*map);

    map = factory->NewMap(EVAL_CONTEXT_TYPE, kVariableSizeSentinel);
    map->set_native_context(*native_context());
    native_context()->set_eval_context_map(*map);

    Handle<ScriptContextTable> script_context_table =
        factory->NewScriptContextTable();
    native_context()->set_script_context_table(*script_context_table);
    InstallGlobalThisBinding();
  }

  {  // --- O b j e c t ---
    Handle<String> object_name = factory->Object_string();
    Handle<JSFunction> object_function = isolate_->object_function();
    JSObject::AddProperty(isolate_, global_object, object_name, object_function,
                          DONT_ENUM);

    SimpleInstallFunction(isolate_, object_function, "assign",
                          Builtin::kObjectAssign, 2, false);
    SimpleInstallFunction(isolate_, object_function, "getOwnPropertyDescriptor",
                          Builtin::kObjectGetOwnPropertyDescriptor, 2, false);
    SimpleInstallFunction(isolate_, object_function,
                          "getOwnPropertyDescriptors",
                          Builtin::kObjectGetOwnPropertyDescriptors, 1, false);
    SimpleInstallFunction(isolate_, object_function, "getOwnPropertyNames",
                          Builtin::kObjectGetOwnPropertyNames, 1, true);
    SimpleInstallFunction(isolate_, object_function, "getOwnPropertySymbols",
                          Builtin::kObjectGetOwnPropertySymbols, 1, false);
    SimpleInstallFunction(isolate_, object_function, "is", Builtin::kObjectIs,
                          2, true);
    SimpleInstallFunction(isolate_, object_function, "preventExtensions",
                          Builtin::kObjectPreventExtensions, 1, true);
    SimpleInstallFunction(isolate_, object_function, "seal",
                          Builtin::kObjectSeal, 1, false);

    Handle<JSFunction> object_create = SimpleInstallFunction(
        isolate_, object_function, "create", Builtin::kObjectCreate, 2, false);
    native_context()->set_object_create(*object_create);

    SimpleInstallFunction(isolate_, object_function, "defineProperties",
                          Builtin::kObjectDefineProperties, 2, true);

    SimpleInstallFunction(isolate_, object_function, "defineProperty",
                          Builtin::kObjectDefineProperty, 3, true);

    SimpleInstallFunction(isolate_, object_function, "freeze",
                          Builtin::kObjectFreeze, 1, false);

    SimpleInstallFunction(isolate_, object_function, "getPrototypeOf",
                          Builtin::kObjectGetPrototypeOf, 1, true);
    SimpleInstallFunction(isolate_, object_function, "setPrototypeOf",
                          Builtin::kObjectSetPrototypeOf, 2, true);

    SimpleInstallFunction(isolate_, object_function, "isExtensible",
                          Builtin::kObjectIsExtensible, 1, true);
    SimpleInstallFunction(isolate_, object_function, "isFrozen",
                          Builtin::kObjectIsFrozen, 1, false);

    SimpleInstallFunction(isolate_, object_function, "isSealed",
                          Builtin::kObjectIsSealed, 1, false);

    SimpleInstallFunction(isolate_, object_function, "keys",
                          Builtin::kObjectKeys, 1, true);
    SimpleInstallFunction(isolate_, object_function, "entries",
                          Builtin::kObjectEntries, 1, true);
    SimpleInstallFunction(isolate_, object_function, "fromEntries",
                          Builtin::kObjectFromEntries, 1, false);
    SimpleInstallFunction(isolate_, object_function, "values",
                          Builtin::kObjectValues, 1, true);

    SimpleInstallFunction(isolate_, isolate_->initial_object_prototype(),
                          "__defineGetter__", Builtin::kObjectDefineGetter, 2,
                          true);
    SimpleInstallFunction(isolate_, isolate_->initial_object_prototype(),
                          "__defineSetter__", Builtin::kObjectDefineSetter, 2,
                          true);
    SimpleInstallFunction(isolate_, isolate_->initial_object_prototype(),
                          "hasOwnProperty",
                          Builtin::kObjectPrototypeHasOwnProperty, 1, true);
    SimpleInstallFunction(isolate_, isolate_->initial_object_prototype(),
                          "__lookupGetter__", Builtin::kObjectLookupGetter, 1,
                          true);
    SimpleInstallFunction(isolate_, isolate_->initial_object_prototype(),
                          "__lookupSetter__", Builtin::kObjectLookupSetter, 1,
                          true);
    SimpleInstallFunction(isolate_, isolate_->initial_object_prototype(),
                          "isPrototypeOf",
                          Builtin::kObjectPrototypeIsPrototypeOf, 1, true);
    SimpleInstallFunction(
        isolate_, isolate_->initial_object_prototype(), "propertyIsEnumerable",
        Builtin::kObjectPrototypePropertyIsEnumerable, 1, false);
    Handle<JSFunction> object_to_string = SimpleInstallFunction(
        isolate_, isolate_->initial_object_prototype(), "toString",
        Builtin::kObjectPrototypeToString, 0, true);
    native_context()->set_object_to_string(*object_to_string);
    Handle<JSFunction> object_value_of = SimpleInstallFunction(
        isolate_, isolate_->initial_object_prototype(), "valueOf",
        Builtin::kObjectPrototypeValueOf, 0, true);
    native_context()->set_object_value_of_function(*object_value_of);

    SimpleInstallGetterSetter(
        isolate_, isolate_->initial_object_prototype(), factory->proto_string(),
        Builtin::kObjectPrototypeGetProto, Builtin::kObjectPrototypeSetProto);

    SimpleInstallFunction(isolate_, isolate_->initial_object_prototype(),
                          "toLocaleString",
                          Builtin::kObjectPrototypeToLocaleString, 0, true);
  }

  Handle<JSObject> global(native_context()->global_object(), isolate());

  {  // --- F u n c t i o n ---
    Handle<JSFunction> prototype = empty_function;
    Handle<JSFunction> function_fun =
        InstallFunction(isolate_, global, "Function", JS_FUNCTION_TYPE,
                        JSFunction::kSizeWithPrototype, 0, prototype,
                        Builtin::kFunctionConstructor);
    // Function instances are sloppy by default.
    function_fun->set_prototype_or_initial_map(*isolate_->sloppy_function_map(),
                                               kReleaseStore);
    function_fun->shared().DontAdaptArguments();
    function_fun->shared().set_length(1);
    InstallWithIntrinsicDefaultProto(isolate_, function_fun,
                                     Context::FUNCTION_FUNCTION_INDEX);

    // Setup the methods on the %FunctionPrototype%.
    JSObject::AddProperty(isolate_, prototype, factory->constructor_string(),
                          function_fun, DONT_ENUM);
    Handle<JSFunction> function_prototype_apply =
        SimpleInstallFunction(isolate_, prototype, "apply",
                              Builtin::kFunctionPrototypeApply, 2, false);
    native_context()->set_function_prototype_apply(*function_prototype_apply);
    SimpleInstallFunction(isolate_, prototype, "bind",
                          Builtin::kFastFunctionPrototypeBind, 1, false);
    SimpleInstallFunction(isolate_, prototype, "call",
                          Builtin::kFunctionPrototypeCall, 1, false);
    Handle<JSFunction> function_to_string =
        SimpleInstallFunction(isolate_, prototype, "toString",
                              Builtin::kFunctionPrototypeToString, 0, false);
    native_context()->set_function_to_string(*function_to_string);

    // Install the @@hasInstance function.
    Handle<JSFunction> has_instance = InstallFunctionAtSymbol(
        isolate_, prototype, factory->has_instance_symbol(),
        "[Symbol.hasInstance]", Builtin::kFunctionPrototypeHasInstance, 1, true,
        static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY));
    native_context()->set_function_has_instance(*has_instance);

    // Complete setting up function maps.
    {
      isolate_->sloppy_function_map()->SetConstructor(*function_fun);
      isolate_->sloppy_function_with_name_map()->SetConstructor(*function_fun);
      isolate_->sloppy_function_with_readonly_prototype_map()->SetConstructor(
          *function_fun);

      isolate_->strict_function_map()->SetConstructor(*function_fun);
      isolate_->strict_function_with_name_map()->SetConstructor(*function_fun);
      isolate_->strict_function_with_readonly_prototype_map()->SetConstructor(
          *function_fun);

      isolate_->class_function_map()->SetConstructor(*function_fun);
    }
  }

  Handle<JSFunction> array_prototype_to_string_fun;
  {  // --- A r r a y ---
    Handle<JSFunction> array_function = InstallConstructor(
        isolate_, global, "Array", JS_ARRAY_TYPE, JSArray::kHeaderSize, 0,
        isolate_->initial_object_prototype(), Builtin::kArrayConstructor,
        JS_ARRAY_CONSTRUCTOR_TYPE);
    array_function->shared().DontAdaptArguments();

    // This seems a bit hackish, but we need to make sure Array.length
    // is 1.
    array_function->shared().set_length(1);

    Handle<Map> initial_map(array_function->initial_map(), isolate());

    // This assert protects an optimization in
    // HGraphBuilder::JSArrayBuilder::EmitMapCode()
    DCHECK(initial_map->elements_kind() == GetInitialFastElementsKind());
    Map::EnsureDescriptorSlack(isolate_, initial_map, 1);

    PropertyAttributes attribs =
        static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE);

    STATIC_ASSERT(JSArray::kLengthDescriptorIndex == 0);
    {  // Add length.
      Descriptor d = Descriptor::AccessorConstant(
          factory->length_string(), factory->array_length_accessor(), attribs);
      initial_map->AppendDescriptor(isolate(), &d);
    }

    InstallWithIntrinsicDefaultProto(isolate_, array_function,
                                     Context::ARRAY_FUNCTION_INDEX);
    InstallSpeciesGetter(isolate_, array_function);

    // Cache the array maps, needed by ArrayConstructorStub
    CacheInitialJSArrayMaps(isolate_, native_context(), initial_map);

    // Set up %ArrayPrototype%.
    // The %ArrayPrototype% has TERMINAL_FAST_ELEMENTS_KIND in order to ensure
    // that constant functions stay constant after turning prototype to setup
    // mode and back.
    Handle<JSArray> proto = factory->NewJSArray(0, TERMINAL_FAST_ELEMENTS_KIND,
                                                AllocationType::kOld);
    JSFunction::SetPrototype(array_function, proto);
    native_context()->set_initial_array_prototype(*proto);

    SimpleInstallFunction(isolate_, array_function, "isArray",
                          Builtin::kArrayIsArray, 1, true);
    SimpleInstallFunction(isolate_, array_function, "from", Builtin::kArrayFrom,
                          1, false);
    SimpleInstallFunction(isolate_, array_function, "of", Builtin::kArrayOf, 0,
                          false);

    JSObject::AddProperty(isolate_, proto, factory->constructor_string(),
                          array_function, DONT_ENUM);

    SimpleInstallFunction(isolate_, proto, "concat",
                          Builtin::kArrayPrototypeConcat, 1, false);
    SimpleInstallFunction(isolate_, proto, "copyWithin",
                          Builtin::kArrayPrototypeCopyWithin, 2, false);
    SimpleInstallFunction(isolate_, proto, "fill", Builtin::kArrayPrototypeFill,
                          1, false);
    SimpleInstallFunction(isolate_, proto, "find", Builtin::kArrayPrototypeFind,
                          1, false);
    SimpleInstallFunction(isolate_, proto, "findIndex",
                          Builtin::kArrayPrototypeFindIndex, 1, false);
    SimpleInstallFunction(isolate_, proto, "lastIndexOf",
                          Builtin::kArrayPrototypeLastIndexOf, 1, false);
    SimpleInstallFunction(isolate_, proto, "pop", Builtin::kArrayPrototypePop,
                          0, false);
    SimpleInstallFunction(isolate_, proto, "push", Builtin::kArrayPrototypePush,
                          1, false);
    SimpleInstallFunction(isolate_, proto, "reverse",
                          Builtin::kArrayPrototypeReverse, 0, false);
    SimpleInstallFunction(isolate_, proto, "shift",
                          Builtin::kArrayPrototypeShift, 0, false);
    SimpleInstallFunction(isolate_, proto, "unshift",
                          Builtin::kArrayPrototypeUnshift, 1, false);
    SimpleInstallFunction(isolate_, proto, "slice",
                          Builtin::kArrayPrototypeSlice, 2, false);
    SimpleInstallFunction(isolate_, proto, "sort", Builtin::kArrayPrototypeSort,
                          1, false);
    SimpleInstallFunction(isolate_, proto, "splice",
                          Builtin::kArrayPrototypeSplice, 2, false);
    SimpleInstallFunction(isolate_, proto, "includes", Builtin::kArrayIncludes,
                          1, false);
    SimpleInstallFunction(isolate_, proto, "indexOf", Builtin::kArrayIndexOf, 1,
                          false);
    SimpleInstallFunction(isolate_, proto, "join", Builtin::kArrayPrototypeJoin,
                          1, false);

    {  // Set up iterator-related properties.
      Handle<JSFunction> keys = InstallFunctionWithBuiltinId(
          isolate_, proto, "keys", Builtin::kArrayPrototypeKeys, 0, true);
      native_context()->set_array_keys_iterator(*keys);

      Handle<JSFunction> entries = InstallFunctionWithBuiltinId(
          isolate_, proto, "entries", Builtin::kArrayPrototypeEntries, 0, true);
      native_context()->set_array_entries_iterator(*entries);

      Handle<JSFunction> values = InstallFunctionWithBuiltinId(
          isolate_, proto, "values", Builtin::kArrayPrototypeValues, 0, true);
      JSObject::AddProperty(isolate_, proto, factory->iterator_symbol(), values,
                            DONT_ENUM);
      native_context()->set_array_values_iterator(*values);
    }

    Handle<JSFunction> for_each_fun = SimpleInstallFunction(
        isolate_, proto, "forEach", Builtin::kArrayForEach, 1, false);
    native_context()->set_array_for_each_iterator(*for_each_fun);
    SimpleInstallFunction(isolate_, proto, "filter", Builtin::kArrayFilter, 1,
                          false);
    SimpleInstallFunction(isolate_, proto, "flat", Builtin::kArrayPrototypeFlat,
                          0, false);
    SimpleInstallFunction(isolate_, proto, "flatMap",
                          Builtin::kArrayPrototypeFlatMap, 1, false);
    SimpleInstallFunction(isolate_, proto, "map", Builtin::kArrayMap, 1, false);
    SimpleInstallFunction(isolate_, proto, "every", Builtin::kArrayEvery, 1,
                          false);
    SimpleInstallFunction(isolate_, proto, "some", Builtin::kArraySome, 1,
                          false);
    SimpleInstallFunction(isolate_, proto, "reduce", Builtin::kArrayReduce, 1,
                          false);
    SimpleInstallFunction(isolate_, proto, "reduceRight",
                          Builtin::kArrayReduceRight, 1, false);
    SimpleInstallFunction(isolate_, proto, "toLocaleString",
                          Builtin::kArrayPrototypeToLocaleString, 0, false);
    array_prototype_to_string_fun =
        SimpleInstallFunction(isolate_, proto, "toString",
                              Builtin::kArrayPrototypeToString, 0, false);

    Handle<JSObject> unscopables = factory->NewJSObjectWithNullProto();
    InstallTrueValuedProperty(isolate_, unscopables, "copyWithin");
    InstallTrueValuedProperty(isolate_, unscopables, "entries");
    InstallTrueValuedProperty(isolate_, unscopables, "fill");
    InstallTrueValuedProperty(isolate_, unscopables, "find");
    InstallTrueValuedProperty(isolate_, unscopables, "findIndex");
    InstallTrueValuedProperty(isolate_, unscopables, "flat");
    InstallTrueValuedProperty(isolate_, unscopables, "flatMap");
    InstallTrueValuedProperty(isolate_, unscopables, "includes");
    InstallTrueValuedProperty(isolate_, unscopables, "keys");
    InstallTrueValuedProperty(isolate_, unscopables, "values");
    JSObject::MigrateSlowToFast(unscopables, 0, "Bootstrapping");
    JSObject::AddProperty(
        isolate_, proto, factory->unscopables_symbol(), unscopables,
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

    Handle<Map> map(proto->map(), isolate_);
    Map::SetShouldBeFastPrototypeMap(map, true, isolate_);
  }

  {  // --- A r r a y I t e r a t o r ---
    Handle<JSObject> iterator_prototype(
        native_context()->initial_iterator_prototype(), isolate());

    Handle<JSObject> array_iterator_prototype =
        factory->NewJSObject(isolate_->object_function(), AllocationType::kOld);
    JSObject::ForceSetPrototype(isolate(), array_iterator_prototype,
                                iterator_prototype);
    CHECK_NE(array_iterator_prototype->map().ptr(),
             isolate_->initial_object_prototype()->map().ptr());
    array_iterator_prototype->map().set_instance_type(
        JS_ARRAY_ITERATOR_PROTOTYPE_TYPE);

    InstallToStringTag(isolate_, array_iterator_prototype,
                       factory->ArrayIterator_string());

    InstallFunctionWithBuiltinId(isolate_, array_iterator_prototype, "next",
                                 Builtin::kArrayIteratorPrototypeNext, 0, true);

    Handle<JSFunction> array_iterator_function =
        CreateFunction(isolate_, factory->ArrayIterator_string(),
                       JS_ARRAY_ITERATOR_TYPE, JSArrayIterator::kHeaderSize, 0,
                       array_iterator_prototype, Builtin::kIllegal);
    array_iterator_function->shared().set_native(false);

    native_context()->set_initial_array_iterator_map(
        array_iterator_function->initial_map());
    native_context()->set_initial_array_iterator_prototype(
        *array_iterator_prototype);
  }

  {  // --- N u m b e r ---
    Handle<JSFunction> number_fun = InstallFunction(
        isolate_, global, "Number", JS_PRIMITIVE_WRAPPER_TYPE,
        JSPrimitiveWrapper::kHeaderSize, 0,
        isolate_->initial_object_prototype(), Builtin::kNumberConstructor);
    number_fun->shared().DontAdaptArguments();
    number_fun->shared().set_length(1);
    InstallWithIntrinsicDefaultProto(isolate_, number_fun,
                                     Context::NUMBER_FUNCTION_INDEX);

    // Create the %NumberPrototype%
    Handle<JSPrimitiveWrapper> prototype = Handle<JSPrimitiveWrapper>::cast(
        factory->NewJSObject(number_fun, AllocationType::kOld));
    prototype->set_value(Smi::zero());
    JSFunction::SetPrototype(number_fun, prototype);

    // Install the "constructor" property on the {prototype}.
    JSObject::AddProperty(isolate_, prototype, factory->constructor_string(),
                          number_fun, DONT_ENUM);

    // Install the Number.prototype methods.
    SimpleInstallFunction(isolate_, prototype, "toExponential",
                          Builtin::kNumberPrototypeToExponential, 1, false);
    SimpleInstallFunction(isolate_, prototype, "toFixed",
                          Builtin::kNumberPrototypeToFixed, 1, false);
    SimpleInstallFunction(isolate_, prototype, "toPrecision",
                          Builtin::kNumberPrototypeToPrecision, 1, false);
    SimpleInstallFunction(isolate_, prototype, "toString",
                          Builtin::kNumberPrototypeToString, 1, false);
    SimpleInstallFunction(isolate_, prototype, "valueOf",
                          Builtin::kNumberPrototypeValueOf, 0, true);

    SimpleInstallFunction(isolate_, prototype, "toLocaleString",
                          Builtin::kNumberPrototypeToLocaleString, 0, false);

    // Install the Number functions.
    SimpleInstallFunction(isolate_, number_fun, "isFinite",
                          Builtin::kNumberIsFinite, 1, true);
    SimpleInstallFunction(isolate_, number_fun, "isInteger",
                          Builtin::kNumberIsInteger, 1, true);
    SimpleInstallFunction(isolate_, number_fun, "isNaN", Builtin::kNumberIsNaN,
                          1, true);
    SimpleInstallFunction(isolate_, number_fun, "isSafeInteger",
                          Builtin::kNumberIsSafeInteger, 1, true);

    // Install Number.parseFloat and Global.parseFloat.
    Handle<JSFunction> parse_float_fun =
        SimpleInstallFunction(isolate_, number_fun, "parseFloat",
                              Builtin::kNumberParseFloat, 1, true);
    JSObject::AddProperty(isolate_, global_object, "parseFloat",
                          parse_float_fun, DONT_ENUM);

    // Install Number.parseInt and Global.parseInt.
    Handle<JSFunction> parse_int_fun = SimpleInstallFunction(
        isolate_, number_fun, "parseInt", Builtin::kNumberParseInt, 2, true);
    JSObject::AddProperty(isolate_, global_object, "parseInt", parse_int_fun,
                          DONT_ENUM);

    // Install Number constants
    const double kMaxValue = 1.7976931348623157e+308;
    const double kMinValue = 5e-324;
    const double kMinSafeInteger = -kMaxSafeInteger;
    const double kEPS = 2.220446049250313e-16;

    InstallConstant(isolate_, number_fun, "MAX_VALUE",
                    factory->NewNumber(kMaxValue));
    InstallConstant(isolate_, number_fun, "MIN_VALUE",
                    factory->NewNumber(kMinValue));
    InstallConstant(isolate_, number_fun, "NaN", factory->nan_value());
    InstallConstant(isolate_, number_fun, "NEGATIVE_INFINITY",
                    factory->NewNumber(-V8_INFINITY));
    InstallConstant(isolate_, number_fun, "POSITIVE_INFINITY",
                    factory->infinity_value());
    InstallConstant(isolate_, number_fun, "MAX_SAFE_INTEGER",
                    factory->NewNumber(kMaxSafeInteger));
    InstallConstant(isolate_, number_fun, "MIN_SAFE_INTEGER",
                    factory->NewNumber(kMinSafeInteger));
    InstallConstant(isolate_, number_fun, "EPSILON", factory->NewNumber(kEPS));

    InstallConstant(isolate_, global, "Infinity", factory->infinity_value());
    InstallConstant(isolate_, global, "NaN", factory->nan_value());
    InstallConstant(isolate_, global, "undefined", factory->undefined_value());
  }

  {  // --- B o o l e a n ---
    Handle<JSFunction> boolean_fun = InstallFunction(
        isolate_, global, "Boolean", JS_PRIMITIVE_WRAPPER_TYPE,
        JSPrimitiveWrapper::kHeaderSize, 0,
        isolate_->initial_object_prototype(), Builtin::kBooleanConstructor);
    boolean_fun->shared().DontAdaptArguments();
    boolean_fun->shared().set_length(1);
    InstallWithIntrinsicDefaultProto(isolate_, boolean_fun,
                                     Context::BOOLEAN_FUNCTION_INDEX);

    // Create the %BooleanPrototype%
    Handle<JSPrimitiveWrapper> prototype = Handle<JSPrimitiveWrapper>::cast(
        factory->NewJSObject(boolean_fun, AllocationType::kOld));
    prototype->set_value(ReadOnlyRoots(isolate_).false_value());
    JSFunction::SetPrototype(boolean_fun, prototype);

    // Install the "constructor" property on the {prototype}.
    JSObject::AddProperty(isolate_, prototype, factory->constructor_string(),
                          boolean_fun, DONT_ENUM);

    // Install the Boolean.prototype methods.
    SimpleInstallFunction(isolate_, prototype, "toString",
                          Builtin::kBooleanPrototypeToString, 0, true);
    SimpleInstallFunction(isolate_, prototype, "valueOf",
                          Builtin::kBooleanPrototypeValueOf, 0, true);
  }

  {  // --- S t r i n g ---
    Handle<JSFunction> string_fun = InstallFunction(
        isolate_, global, "String", JS_PRIMITIVE_WRAPPER_TYPE,
        JSPrimitiveWrapper::kHeaderSize, 0,
        isolate_->initial_object_prototype(), Builtin::kStringConstructor);
    string_fun->shared().DontAdaptArguments();
    string_fun->shared().set_length(1);
    InstallWithIntrinsicDefaultProto(isolate_, string_fun,
                                     Context::STRING_FUNCTION_INDEX);

    Handle<Map> string_map = Handle<Map>(
        native_context()->string_function().initial_map(), isolate());
    string_map->set_elements_kind(FAST_STRING_WRAPPER_ELEMENTS);
    Map::EnsureDescriptorSlack(isolate_, string_map, 1);

    PropertyAttributes attribs =
        static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);

    {  // Add length.
      Descriptor d = Descriptor::AccessorConstant(
          factory->length_string(), factory->string_length_accessor(), attribs);
      string_map->AppendDescriptor(isolate(), &d);
    }

    // Install the String.fromCharCode function.
    SimpleInstallFunction(isolate_, string_fun, "fromCharCode",
                          Builtin::kStringFromCharCode, 1, false);

    // Install the String.fromCodePoint function.
    SimpleInstallFunction(isolate_, string_fun, "fromCodePoint",
                          Builtin::kStringFromCodePoint, 1, false);

    // Install the String.raw function.
    SimpleInstallFunction(isolate_, string_fun, "raw", Builtin::kStringRaw, 1,
                          false);

    // Create the %StringPrototype%
    Handle<JSPrimitiveWrapper> prototype = Handle<JSPrimitiveWrapper>::cast(
        factory->NewJSObject(string_fun, AllocationType::kOld));
    prototype->set_value(ReadOnlyRoots(isolate_).empty_string());
    JSFunction::SetPrototype(string_fun, prototype);
    native_context()->set_initial_string_prototype(*prototype);

    // Install the "constructor" property on the {prototype}.
    JSObject::AddProperty(isolate_, prototype, factory->constructor_string(),
                          string_fun, DONT_ENUM);

    // Install the String.prototype methods.
    SimpleInstallFunction(isolate_, prototype, "anchor",
                          Builtin::kStringPrototypeAnchor, 1, false);
    SimpleInstallFunction(isolate_, prototype, "big",
                          Builtin::kStringPrototypeBig, 0, false);
    SimpleInstallFunction(isolate_, prototype, "blink",
                          Builtin::kStringPrototypeBlink, 0, false);
    SimpleInstallFunction(isolate_, prototype, "bold",
                          Builtin::kStringPrototypeBold, 0, false);
    SimpleInstallFunction(isolate_, prototype, "charAt",
                          Builtin::kStringPrototypeCharAt, 1, true);
    SimpleInstallFunction(isolate_, prototype, "charCodeAt",
                          Builtin::kStringPrototypeCharCodeAt, 1, true);
    SimpleInstallFunction(isolate_, prototype, "codePointAt",
                          Builtin::kStringPrototypeCodePointAt, 1, true);
    SimpleInstallFunction(isolate_, prototype, "concat",
                          Builtin::kStringPrototypeConcat, 1, false);
    SimpleInstallFunction(isolate_, prototype, "endsWith",
                          Builtin::kStringPrototypeEndsWith, 1, false);
    SimpleInstallFunction(isolate_, prototype, "fontcolor",
                          Builtin::kStringPrototypeFontcolor, 1, false);
    SimpleInstallFunction(isolate_, prototype, "fontsize",
                          Builtin::kStringPrototypeFontsize, 1, false);
    SimpleInstallFunction(isolate_, prototype, "fixed",
                          Builtin::kStringPrototypeFixed, 0, false);
    SimpleInstallFunction(isolate_, prototype, "includes",
                          Builtin::kStringPrototypeIncludes, 1, false);
    SimpleInstallFunction(isolate_, prototype, "indexOf",
                          Builtin::kStringPrototypeIndexOf, 1, false);
    SimpleInstallFunction(isolate_, prototype, "italics",
                          Builtin::kStringPrototypeItalics, 0, false);
    SimpleInstallFunction(isolate_, prototype, "lastIndexOf",
                          Builtin::kStringPrototypeLastIndexOf, 1, false);
    SimpleInstallFunction(isolate_, prototype, "link",
                          Builtin::kStringPrototypeLink, 1, false);
#ifdef V8_INTL_SUPPORT
    SimpleInstallFunction(isolate_, prototype, "localeCompare",
                          Builtin::kStringPrototypeLocaleCompare, 1, false);
#else
    SimpleInstallFunction(isolate_, prototype, "localeCompare",
                          Builtin::kStringPrototypeLocaleCompare, 1, true);
#endif  // V8_INTL_SUPPORT
    SimpleInstallFunction(isolate_, prototype, "match",
                          Builtin::kStringPrototypeMatch, 1, true);
    SimpleInstallFunction(isolate_, prototype, "matchAll",
                          Builtin::kStringPrototypeMatchAll, 1, true);
#ifdef V8_INTL_SUPPORT
    SimpleInstallFunction(isolate_, prototype, "normalize",
                          Builtin::kStringPrototypeNormalizeIntl, 0, false);
#else
    SimpleInstallFunction(isolate_, prototype, "normalize",
                          Builtin::kStringPrototypeNormalize, 0, false);
#endif  // V8_INTL_SUPPORT
    SimpleInstallFunction(isolate_, prototype, "padEnd",
                          Builtin::kStringPrototypePadEnd, 1, false);
    SimpleInstallFunction(isolate_, prototype, "padStart",
                          Builtin::kStringPrototypePadStart, 1, false);
    SimpleInstallFunction(isolate_, prototype, "repeat",
                          Builtin::kStringPrototypeRepeat, 1, true);
    SimpleInstallFunction(isolate_, prototype, "replace",
                          Builtin::kStringPrototypeReplace, 2, true);
    SimpleInstallFunction(isolate(), prototype, "replaceAll",
                          Builtin::kStringPrototypeReplaceAll, 2, true);
    SimpleInstallFunction(isolate_, prototype, "search",
                          Builtin::kStringPrototypeSearch, 1, true);
    SimpleInstallFunction(isolate_, prototype, "slice",
                          Builtin::kStringPrototypeSlice, 2, false);
    SimpleInstallFunction(isolate_, prototype, "small",
                          Builtin::kStringPrototypeSmall, 0, false);
    SimpleInstallFunction(isolate_, prototype, "split",
                          Builtin::kStringPrototypeSplit, 2, false);
    SimpleInstallFunction(isolate_, prototype, "strike",
                          Builtin::kStringPrototypeStrike, 0, false);
    SimpleInstallFunction(isolate_, prototype, "sub",
                          Builtin::kStringPrototypeSub, 0, false);
    SimpleInstallFunction(isolate_, prototype, "substr",
                          Builtin::kStringPrototypeSubstr, 2, false);
    SimpleInstallFunction(isolate_, prototype, "substring",
                          Builtin::kStringPrototypeSubstring, 2, false);
    SimpleInstallFunction(isolate_, prototype, "sup",
                          Builtin::kStringPrototypeSup, 0, false);
    SimpleInstallFunction(isolate_, prototype, "startsWith",
                          Builtin::kStringPrototypeStartsWith, 1, false);
    SimpleInstallFunction(isolate_, prototype, "toString",
                          Builtin::kStringPrototypeToString, 0, true);
    SimpleInstallFunction(isolate_, prototype, "trim",
                          Builtin::kStringPrototypeTrim, 0, false);

    // Install `String.prototype.trimStart` with `trimLeft` alias.
    Handle<JSFunction> trim_start_fun =
        SimpleInstallFunction(isolate_, prototype, "trimStart",
                              Builtin::kStringPrototypeTrimStart, 0, false);
    JSObject::AddProperty(isolate_, prototype, "trimLeft", trim_start_fun,
                          DONT_ENUM);

    // Install `String.prototype.trimEnd` with `trimRight` alias.
    Handle<JSFunction> trim_end_fun =
        SimpleInstallFunction(isolate_, prototype, "trimEnd",
                              Builtin::kStringPrototypeTrimEnd, 0, false);
    JSObject::AddProperty(isolate_, prototype, "trimRight", trim_end_fun,
                          DONT_ENUM);

    SimpleInstallFunction(isolate_, prototype, "toLocaleLowerCase",
                          Builtin::kStringPrototypeToLocaleLowerCase, 0, false);
    SimpleInstallFunction(isolate_, prototype, "toLocaleUpperCase",
                          Builtin::kStringPrototypeToLocaleUpperCase, 0, false);
#ifdef V8_INTL_SUPPORT
    SimpleInstallFunction(isolate_, prototype, "toLowerCase",
                          Builtin::kStringPrototypeToLowerCaseIntl, 0, true);
    SimpleInstallFunction(isolate_, prototype, "toUpperCase",
                          Builtin::kStringPrototypeToUpperCaseIntl, 0, false);
#else
    SimpleInstallFunction(isolate_, prototype, "toLowerCase",
                          Builtin::kStringPrototypeToLowerCase, 0, false);
    SimpleInstallFunction(isolate_, prototype, "toUpperCase",
                          Builtin::kStringPrototypeToUpperCase, 0, false);
#endif
    SimpleInstallFunction(isolate_, prototype, "valueOf",
                          Builtin::kStringPrototypeValueOf, 0, true);

    InstallFunctionAtSymbol(
        isolate_, prototype, factory->iterator_symbol(), "[Symbol.iterator]",
        Builtin::kStringPrototypeIterator, 0, true, DONT_ENUM);
  }

  {  // --- S t r i n g I t e r a t o r ---
    Handle<JSObject> iterator_prototype(
        native_context()->initial_iterator_prototype(), isolate());

    Handle<JSObject> string_iterator_prototype =
        factory->NewJSObject(isolate_->object_function(), AllocationType::kOld);
    JSObject::ForceSetPrototype(isolate(), string_iterator_prototype,
                                iterator_prototype);
    CHECK_NE(string_iterator_prototype->map().ptr(),
             isolate_->initial_object_prototype()->map().ptr());
    string_iterator_prototype->map().set_instance_type(
        JS_STRING_ITERATOR_PROTOTYPE_TYPE);
    InstallToStringTag(isolate_, string_iterator_prototype, "String Iterator");

    InstallFunctionWithBuiltinId(isolate_, string_iterator_prototype, "next",
                                 Builtin::kStringIteratorPrototypeNext, 0,
                                 true);

    Handle<JSFunction> string_iterator_function = CreateFunction(
        isolate_, factory->InternalizeUtf8String("StringIterator"),
        JS_STRING_ITERATOR_TYPE, JSStringIterator::kHeaderSize, 0,
        string_iterator_prototype, Builtin::kIllegal);
    string_iterator_function->shared().set_native(false);
    native_context()->set_initial_string_iterator_map(
        string_iterator_function->initial_map());
    native_context()->set_initial_string_iterator_prototype(
        *string_iterator_prototype);
  }

  {  // --- S y m b o l ---
    Handle<JSFunction> symbol_fun =
        InstallFunction(isolate_, global, "Symbol", JS_PRIMITIVE_WRAPPER_TYPE,
                        JSPrimitiveWrapper::kHeaderSize, 0,
                        factory->the_hole_value(), Builtin::kSymbolConstructor);
    symbol_fun->shared().set_length(0);
    symbol_fun->shared().DontAdaptArguments();
    native_context()->set_symbol_function(*symbol_fun);

    // Install the Symbol.for and Symbol.keyFor functions.
    SimpleInstallFunction(isolate_, symbol_fun, "for", Builtin::kSymbolFor, 1,
                          false);
    SimpleInstallFunction(isolate_, symbol_fun, "keyFor",
                          Builtin::kSymbolKeyFor, 1, false);

    // Install well-known symbols.
    InstallConstant(isolate_, symbol_fun, "asyncIterator",
                    factory->async_iterator_symbol());
    InstallConstant(isolate_, symbol_fun, "hasInstance",
                    factory->has_instance_symbol());
    InstallConstant(isolate_, symbol_fun, "isConcatSpreadable",
                    factory->is_concat_spreadable_symbol());
    InstallConstant(isolate_, symbol_fun, "iterator",
                    factory->iterator_symbol());
    InstallConstant(isolate_, symbol_fun, "match", factory->match_symbol());
    InstallConstant(isolate_, symbol_fun, "matchAll",
                    factory->match_all_symbol());
    InstallConstant(isolate_, symbol_fun, "replace", factory->replace_symbol());
    InstallConstant(isolate_, symbol_fun, "search", factory->search_symbol());
    InstallConstant(isolate_, symbol_fun, "species", factory->species_symbol());
    InstallConstant(isolate_, symbol_fun, "split", factory->split_symbol());
    InstallConstant(isolate_, symbol_fun, "toPrimitive",
                    factory->to_primitive_symbol());
    InstallConstant(isolate_, symbol_fun, "toStringTag",
                    factory->to_string_tag_symbol());
    InstallConstant(isolate_, symbol_fun, "unscopables",
                    factory->unscopables_symbol());

    // Setup %SymbolPrototype%.
    Handle<JSObject> prototype(JSObject::cast(symbol_fun->instance_prototype()),
                               isolate());

    InstallToStringTag(isolate_, prototype, "Symbol");

    // Install the Symbol.prototype methods.
    InstallFunctionWithBuiltinId(isolate_, prototype, "toString",
                                 Builtin::kSymbolPrototypeToString, 0, true);
    InstallFunctionWithBuiltinId(isolate_, prototype, "valueOf",
                                 Builtin::kSymbolPrototypeValueOf, 0, true);

    // Install the Symbol.prototype.description getter.
    SimpleInstallGetter(isolate_, prototype,
                        factory->InternalizeUtf8String("description"),
                        Builtin::kSymbolPrototypeDescriptionGetter, true);

    // Install the @@toPrimitive function.
    InstallFunctionAtSymbol(
        isolate_, prototype, factory->to_primitive_symbol(),
        "[Symbol.toPrimitive]", Builtin::kSymbolPrototypeToPrimitive, 1, true,
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));
  }

  {  // --- D a t e ---
    Handle<JSFunction> date_fun = InstallFunction(
        isolate_, global, "Date", JS_DATE_TYPE, JSDate::kHeaderSize, 0,
        factory->the_hole_value(), Builtin::kDateConstructor);
    InstallWithIntrinsicDefaultProto(isolate_, date_fun,
                                     Context::DATE_FUNCTION_INDEX);
    date_fun->shared().set_length(7);
    date_fun->shared().DontAdaptArguments();

    // Install the Date.now, Date.parse and Date.UTC functions.
    SimpleInstallFunction(isolate_, date_fun, "now", Builtin::kDateNow, 0,
                          false);
    SimpleInstallFunction(isolate_, date_fun, "parse", Builtin::kDateParse, 1,
                          false);
    SimpleInstallFunction(isolate_, date_fun, "UTC", Builtin::kDateUTC, 7,
                          false);

    // Setup %DatePrototype%.
    Handle<JSObject> prototype(JSObject::cast(date_fun->instance_prototype()),
                               isolate());

    // Install the Date.prototype methods.
    SimpleInstallFunction(isolate_, prototype, "toString",
                          Builtin::kDatePrototypeToString, 0, false);
    SimpleInstallFunction(isolate_, prototype, "toDateString",
                          Builtin::kDatePrototypeToDateString, 0, false);
    SimpleInstallFunction(isolate_, prototype, "toTimeString",
                          Builtin::kDatePrototypeToTimeString, 0, false);
    SimpleInstallFunction(isolate_, prototype, "toISOString",
                          Builtin::kDatePrototypeToISOString, 0, false);
    Handle<JSFunction> to_utc_string =
        SimpleInstallFunction(isolate_, prototype, "toUTCString",
                              Builtin::kDatePrototypeToUTCString, 0, false);
    JSObject::AddProperty(isolate_, prototype, "toGMTString", to_utc_string,
                          DONT_ENUM);
    SimpleInstallFunction(isolate_, prototype, "getDate",
                          Builtin::kDatePrototypeGetDate, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setDate",
                          Builtin::kDatePrototypeSetDate, 1, false);
    SimpleInstallFunction(isolate_, prototype, "getDay",
                          Builtin::kDatePrototypeGetDay, 0, true);
    SimpleInstallFunction(isolate_, prototype, "getFullYear",
                          Builtin::kDatePrototypeGetFullYear, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setFullYear",
                          Builtin::kDatePrototypeSetFullYear, 3, false);
    SimpleInstallFunction(isolate_, prototype, "getHours",
                          Builtin::kDatePrototypeGetHours, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setHours",
                          Builtin::kDatePrototypeSetHours, 4, false);
    SimpleInstallFunction(isolate_, prototype, "getMilliseconds",
                          Builtin::kDatePrototypeGetMilliseconds, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setMilliseconds",
                          Builtin::kDatePrototypeSetMilliseconds, 1, false);
    SimpleInstallFunction(isolate_, prototype, "getMinutes",
                          Builtin::kDatePrototypeGetMinutes, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setMinutes",
                          Builtin::kDatePrototypeSetMinutes, 3, false);
    SimpleInstallFunction(isolate_, prototype, "getMonth",
                          Builtin::kDatePrototypeGetMonth, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setMonth",
                          Builtin::kDatePrototypeSetMonth, 2, false);
    SimpleInstallFunction(isolate_, prototype, "getSeconds",
                          Builtin::kDatePrototypeGetSeconds, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setSeconds",
                          Builtin::kDatePrototypeSetSeconds, 2, false);
    SimpleInstallFunction(isolate_, prototype, "getTime",
                          Builtin::kDatePrototypeGetTime, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setTime",
                          Builtin::kDatePrototypeSetTime, 1, false);
    SimpleInstallFunction(isolate_, prototype, "getTimezoneOffset",
                          Builtin::kDatePrototypeGetTimezoneOffset, 0, true);
    SimpleInstallFunction(isolate_, prototype, "getUTCDate",
                          Builtin::kDatePrototypeGetUTCDate, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setUTCDate",
                          Builtin::kDatePrototypeSetUTCDate, 1, false);
    SimpleInstallFunction(isolate_, prototype, "getUTCDay",
                          Builtin::kDatePrototypeGetUTCDay, 0, true);
    SimpleInstallFunction(isolate_, prototype, "getUTCFullYear",
                          Builtin::kDatePrototypeGetUTCFullYear, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setUTCFullYear",
                          Builtin::kDatePrototypeSetUTCFullYear, 3, false);
    SimpleInstallFunction(isolate_, prototype, "getUTCHours",
                          Builtin::kDatePrototypeGetUTCHours, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setUTCHours",
                          Builtin::kDatePrototypeSetUTCHours, 4, false);
    SimpleInstallFunction(isolate_, prototype, "getUTCMilliseconds",
                          Builtin::kDatePrototypeGetUTCMilliseconds, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setUTCMilliseconds",
                          Builtin::kDatePrototypeSetUTCMilliseconds, 1, false);
    SimpleInstallFunction(isolate_, prototype, "getUTCMinutes",
                          Builtin::kDatePrototypeGetUTCMinutes, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setUTCMinutes",
                          Builtin::kDatePrototypeSetUTCMinutes, 3, false);
    SimpleInstallFunction(isolate_, prototype, "getUTCMonth",
                          Builtin::kDatePrototypeGetUTCMonth, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setUTCMonth",
                          Builtin::kDatePrototypeSetUTCMonth, 2, false);
    SimpleInstallFunction(isolate_, prototype, "getUTCSeconds",
                          Builtin::kDatePrototypeGetUTCSeconds, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setUTCSeconds",
                          Builtin::kDatePrototypeSetUTCSeconds, 2, false);
    SimpleInstallFunction(isolate_, prototype, "valueOf",
                          Builtin::kDatePrototypeValueOf, 0, true);
    SimpleInstallFunction(isolate_, prototype, "getYear",
                          Builtin::kDatePrototypeGetYear, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setYear",
                          Builtin::kDatePrototypeSetYear, 1, false);
    SimpleInstallFunction(isolate_, prototype, "toJSON",
                          Builtin::kDatePrototypeToJson, 1, false);

#ifdef V8_INTL_SUPPORT
    SimpleInstallFunction(isolate_, prototype, "toLocaleString",
                          Builtin::kDatePrototypeToLocaleString, 0, false);
    SimpleInstallFunction(isolate_, prototype, "toLocaleDateString",
                          Builtin::kDatePrototypeToLocaleDateString, 0, false);
    SimpleInstallFunction(isolate_, prototype, "toLocaleTimeString",
                          Builtin::kDatePrototypeToLocaleTimeString, 0, false);
#else
    // Install Intl fallback functions.
    SimpleInstallFunction(isolate_, prototype, "toLocaleString",
                          Builtin::kDatePrototypeToString, 0, false);
    SimpleInstallFunction(isolate_, prototype, "toLocaleDateString",
                          Builtin::kDatePrototypeToDateString, 0, false);
    SimpleInstallFunction(isolate_, prototype, "toLocaleTimeString",
                          Builtin::kDatePrototypeToTimeString, 0, false);
#endif  // V8_INTL_SUPPORT

    // Install the @@toPrimitive function.
    InstallFunctionAtSymbol(
        isolate_, prototype, factory->to_primitive_symbol(),
        "[Symbol.toPrimitive]", Builtin::kDatePrototypeToPrimitive, 1, true,
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));
  }

  {  // -- P r o m i s e
    Handle<JSFunction> promise_fun = InstallConstructor(
        isolate_, global, "Promise", JS_PROMISE_TYPE,
        JSPromise::kSizeWithEmbedderFields, 0, factory->the_hole_value(),
        Builtin::kPromiseConstructor, JS_PROMISE_CONSTRUCTOR_TYPE);
    InstallWithIntrinsicDefaultProto(isolate_, promise_fun,
                                     Context::PROMISE_FUNCTION_INDEX);

    Handle<SharedFunctionInfo> shared(promise_fun->shared(), isolate_);
    shared->set_internal_formal_parameter_count(1);
    shared->set_length(1);

    InstallSpeciesGetter(isolate_, promise_fun);

    Handle<JSFunction> promise_all = InstallFunctionWithBuiltinId(
        isolate_, promise_fun, "all", Builtin::kPromiseAll, 1, true);
    native_context()->set_promise_all(*promise_all);

    InstallFunctionWithBuiltinId(isolate_, promise_fun, "allSettled",
                                 Builtin::kPromiseAllSettled, 1, true);

    Handle<JSFunction> promise_any = InstallFunctionWithBuiltinId(
        isolate_, promise_fun, "any", Builtin::kPromiseAny, 1, true);
    native_context()->set_promise_any(*promise_any);

    InstallFunctionWithBuiltinId(isolate_, promise_fun, "race",
                                 Builtin::kPromiseRace, 1, true);

    InstallFunctionWithBuiltinId(isolate_, promise_fun, "resolve",
                                 Builtin::kPromiseResolveTrampoline, 1, true);

    InstallFunctionWithBuiltinId(isolate_, promise_fun, "reject",
                                 Builtin::kPromiseReject, 1, true);

    // Setup %PromisePrototype%.
    Handle<JSObject> prototype(
        JSObject::cast(promise_fun->instance_prototype()), isolate());
    native_context()->set_promise_prototype(*prototype);

    InstallToStringTag(isolate_, prototype, factory->Promise_string());

    Handle<JSFunction> promise_then = InstallFunctionWithBuiltinId(
        isolate_, prototype, "then", Builtin::kPromisePrototypeThen, 2, true);
    native_context()->set_promise_then(*promise_then);

    InstallFunctionWithBuiltinId(isolate_, prototype, "catch",
                                 Builtin::kPromisePrototypeCatch, 1, true);

    InstallFunctionWithBuiltinId(isolate_, prototype, "finally",
                                 Builtin::kPromisePrototypeFinally, 1, true);

    DCHECK(promise_fun->HasFastProperties());

    Handle<Map> prototype_map(prototype->map(), isolate());
    Map::SetShouldBeFastPrototypeMap(prototype_map, true, isolate_);
    CHECK_NE(prototype->map().ptr(),
             isolate_->initial_object_prototype()->map().ptr());
    prototype->map().set_instance_type(JS_PROMISE_PROTOTYPE_TYPE);

    DCHECK(promise_fun->HasFastProperties());
  }

  {  // -- R e g E x p
    // Builtin functions for RegExp.prototype.
    Handle<JSFunction> regexp_fun = InstallConstructor(
        isolate_, global, "RegExp", JS_REG_EXP_TYPE,
        JSRegExp::kHeaderSize + JSRegExp::kInObjectFieldCount * kTaggedSize,
        JSRegExp::kInObjectFieldCount, factory->the_hole_value(),
        Builtin::kRegExpConstructor, JS_REG_EXP_CONSTRUCTOR_TYPE);
    InstallWithIntrinsicDefaultProto(isolate_, regexp_fun,
                                     Context::REGEXP_FUNCTION_INDEX);
    Handle<SharedFunctionInfo> shared(regexp_fun->shared(), isolate_);
    shared->set_internal_formal_parameter_count(2);
    shared->set_length(2);

    {
      // Setup %RegExpPrototype%.
      Handle<JSObject> prototype(
          JSObject::cast(regexp_fun->instance_prototype()), isolate());
      native_context()->set_regexp_prototype(*prototype);

      {
        Handle<JSFunction> fun =
            SimpleInstallFunction(isolate_, prototype, "exec",
                                  Builtin::kRegExpPrototypeExec, 1, true);
        native_context()->set_regexp_exec_function(*fun);
        DCHECK_EQ(JSRegExp::kExecFunctionDescriptorIndex,
                  prototype->map().LastAdded().as_int());
      }

      SimpleInstallGetter(isolate_, prototype, factory->dotAll_string(),
                          Builtin::kRegExpPrototypeDotAllGetter, true);
      SimpleInstallGetter(isolate_, prototype, factory->flags_string(),
                          Builtin::kRegExpPrototypeFlagsGetter, true);
      SimpleInstallGetter(isolate_, prototype, factory->global_string(),
                          Builtin::kRegExpPrototypeGlobalGetter, true);
      SimpleInstallGetter(isolate_, prototype, factory->ignoreCase_string(),
                          Builtin::kRegExpPrototypeIgnoreCaseGetter, true);
      SimpleInstallGetter(isolate_, prototype, factory->multiline_string(),
                          Builtin::kRegExpPrototypeMultilineGetter, true);
      SimpleInstallGetter(isolate_, prototype, factory->source_string(),
                          Builtin::kRegExpPrototypeSourceGetter, true);
      SimpleInstallGetter(isolate_, prototype, factory->sticky_string(),
                          Builtin::kRegExpPrototypeStickyGetter, true);
      SimpleInstallGetter(isolate_, prototype, factory->unicode_string(),
                          Builtin::kRegExpPrototypeUnicodeGetter, true);

      SimpleInstallFunction(isolate_, prototype, "compile",
                            Builtin::kRegExpPrototypeCompile, 2, true);
      SimpleInstallFunction(isolate_, prototype, "toString",
                            Builtin::kRegExpPrototypeToString, 0, false);
      SimpleInstallFunction(isolate_, prototype, "test",
                            Builtin::kRegExpPrototypeTest, 1, true);

      {
        Handle<JSFunction> fun = InstallFunctionAtSymbol(
            isolate_, prototype, factory->match_symbol(), "[Symbol.match]",
            Builtin::kRegExpPrototypeMatch, 1, true);
        native_context()->set_regexp_match_function(*fun);
        DCHECK_EQ(JSRegExp::kSymbolMatchFunctionDescriptorIndex,
                  prototype->map().LastAdded().as_int());
      }

      {
        Handle<JSFunction> fun = InstallFunctionAtSymbol(
            isolate_, prototype, factory->match_all_symbol(),
            "[Symbol.matchAll]", Builtin::kRegExpPrototypeMatchAll, 1, true);
        native_context()->set_regexp_match_all_function(*fun);
        DCHECK_EQ(JSRegExp::kSymbolMatchAllFunctionDescriptorIndex,
                  prototype->map().LastAdded().as_int());
      }

      {
        Handle<JSFunction> fun = InstallFunctionAtSymbol(
            isolate_, prototype, factory->replace_symbol(), "[Symbol.replace]",
            Builtin::kRegExpPrototypeReplace, 2, false);
        native_context()->set_regexp_replace_function(*fun);
        DCHECK_EQ(JSRegExp::kSymbolReplaceFunctionDescriptorIndex,
                  prototype->map().LastAdded().as_int());
      }

      {
        Handle<JSFunction> fun = InstallFunctionAtSymbol(
            isolate_, prototype, factory->search_symbol(), "[Symbol.search]",
            Builtin::kRegExpPrototypeSearch, 1, true);
        native_context()->set_regexp_search_function(*fun);
        DCHECK_EQ(JSRegExp::kSymbolSearchFunctionDescriptorIndex,
                  prototype->map().LastAdded().as_int());
      }

      {
        Handle<JSFunction> fun = InstallFunctionAtSymbol(
            isolate_, prototype, factory->split_symbol(), "[Symbol.split]",
            Builtin::kRegExpPrototypeSplit, 2, false);
        native_context()->set_regexp_split_function(*fun);
        DCHECK_EQ(JSRegExp::kSymbolSplitFunctionDescriptorIndex,
                  prototype->map().LastAdded().as_int());
      }

      Handle<Map> prototype_map(prototype->map(), isolate());
      Map::SetShouldBeFastPrototypeMap(prototype_map, true, isolate_);
      CHECK_NE((*prototype_map).ptr(),
               isolate_->initial_object_prototype()->map().ptr());
      prototype_map->set_instance_type(JS_REG_EXP_PROTOTYPE_TYPE);

      // Store the initial RegExp.prototype map. This is used in fast-path
      // checks. Do not alter the prototype after this point.
      native_context()->set_regexp_prototype_map(*prototype_map);
    }

    {
      // RegExp getters and setters.

      InstallSpeciesGetter(isolate_, regexp_fun);

      // Static properties set by a successful match.

      SimpleInstallGetterSetter(isolate_, regexp_fun, factory->input_string(),
                                Builtin::kRegExpInputGetter,
                                Builtin::kRegExpInputSetter);
      SimpleInstallGetterSetter(isolate_, regexp_fun, "$_",
                                Builtin::kRegExpInputGetter,
                                Builtin::kRegExpInputSetter);

      SimpleInstallGetterSetter(isolate_, regexp_fun, "lastMatch",
                                Builtin::kRegExpLastMatchGetter,
                                Builtin::kEmptyFunction);
      SimpleInstallGetterSetter(isolate_, regexp_fun, "$&",
                                Builtin::kRegExpLastMatchGetter,
                                Builtin::kEmptyFunction);

      SimpleInstallGetterSetter(isolate_, regexp_fun, "lastParen",
                                Builtin::kRegExpLastParenGetter,
                                Builtin::kEmptyFunction);
      SimpleInstallGetterSetter(isolate_, regexp_fun, "$+",
                                Builtin::kRegExpLastParenGetter,
                                Builtin::kEmptyFunction);

      SimpleInstallGetterSetter(isolate_, regexp_fun, "leftContext",
                                Builtin::kRegExpLeftContextGetter,
                                Builtin::kEmptyFunction);
      SimpleInstallGetterSetter(isolate_, regexp_fun, "$`",
                                Builtin::kRegExpLeftContextGetter,
                                Builtin::kEmptyFunction);

      SimpleInstallGetterSetter(isolate_, regexp_fun, "rightContext",
                                Builtin::kRegExpRightContextGetter,
                                Builtin::kEmptyFunction);
      SimpleInstallGetterSetter(isolate_, regexp_fun, "$'",
                                Builtin::kRegExpRightContextGetter,
                                Builtin::kEmptyFunction);

#define INSTALL_CAPTURE_GETTER(i)                               \
  SimpleInstallGetterSetter(isolate_, regexp_fun, "$" #i,       \
                            Builtin::kRegExpCapture##i##Getter, \
                            Builtin::kEmptyFunction)
      INSTALL_CAPTURE_GETTER(1);
      INSTALL_CAPTURE_GETTER(2);
      INSTALL_CAPTURE_GETTER(3);
      INSTALL_CAPTURE_GETTER(4);
      INSTALL_CAPTURE_GETTER(5);
      INSTALL_CAPTURE_GETTER(6);
      INSTALL_CAPTURE_GETTER(7);
      INSTALL_CAPTURE_GETTER(8);
      INSTALL_CAPTURE_GETTER(9);
#undef INSTALL_CAPTURE_GETTER
    }

    DCHECK(regexp_fun->has_initial_map());
    Handle<Map> initial_map(regexp_fun->initial_map(), isolate());

    DCHECK_EQ(1, initial_map->GetInObjectProperties());

    Map::EnsureDescriptorSlack(isolate_, initial_map, 1);

    // ECMA-262, section 15.10.7.5.
    PropertyAttributes writable =
        static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE);
    Descriptor d = Descriptor::DataField(isolate(), factory->lastIndex_string(),
                                         JSRegExp::kLastIndexFieldIndex,
                                         writable, Representation::Tagged());
    initial_map->AppendDescriptor(isolate(), &d);

    // Create the last match info.
    Handle<RegExpMatchInfo> last_match_info = factory->NewRegExpMatchInfo();
    native_context()->set_regexp_last_match_info(*last_match_info);

    // Install the species protector cell.
    Handle<PropertyCell> cell = factory->NewProtector();
    native_context()->set_regexp_species_protector(*cell);

    DCHECK(regexp_fun->HasFastProperties());
  }

  {  // --- R e g E x p S t r i n g  I t e r a t o r ---
    Handle<JSObject> iterator_prototype(
        native_context()->initial_iterator_prototype(), isolate());

    Handle<JSObject> regexp_string_iterator_prototype = factory->NewJSObject(
        isolate()->object_function(), AllocationType::kOld);
    JSObject::ForceSetPrototype(isolate(), regexp_string_iterator_prototype,
                                iterator_prototype);

    InstallToStringTag(isolate(), regexp_string_iterator_prototype,
                       "RegExp String Iterator");

    SimpleInstallFunction(isolate(), regexp_string_iterator_prototype, "next",
                          Builtin::kRegExpStringIteratorPrototypeNext, 0, true);

    Handle<JSFunction> regexp_string_iterator_function = CreateFunction(
        isolate(), "RegExpStringIterator", JS_REG_EXP_STRING_ITERATOR_TYPE,
        JSRegExpStringIterator::kHeaderSize, 0,
        regexp_string_iterator_prototype, Builtin::kIllegal);
    regexp_string_iterator_function->shared().set_native(false);
    native_context()->set_initial_regexp_string_iterator_prototype_map(
        regexp_string_iterator_function->initial_map());
  }

  // -- E r r o r
  InstallError(isolate_, global, factory->Error_string(),
               Context::ERROR_FUNCTION_INDEX);

  // -- A g g r e g a t e E r r o r
  InstallError(isolate_, global, factory->AggregateError_string(),
               Context::AGGREGATE_ERROR_FUNCTION_INDEX,
               Builtin::kAggregateErrorConstructor, 2, 2);

  // -- E v a l E r r o r
  InstallError(isolate_, global, factory->EvalError_string(),
               Context::EVAL_ERROR_FUNCTION_INDEX);

  // -- R a n g e E r r o r
  InstallError(isolate_, global, factory->RangeError_string(),
               Context::RANGE_ERROR_FUNCTION_INDEX);

  // -- R e f e r e n c e E r r o r
  InstallError(isolate_, global, factory->ReferenceError_string(),
               Context::REFERENCE_ERROR_FUNCTION_INDEX);

  // -- S y n t a x E r r o r
  InstallError(isolate_, global, factory->SyntaxError_string(),
               Context::SYNTAX_ERROR_FUNCTION_INDEX);

  // -- T y p e E r r o r
  InstallError(isolate_, global, factory->TypeError_string(),
               Context::TYPE_ERROR_FUNCTION_INDEX);

  // -- U R I E r r o r
  InstallError(isolate_, global, factory->URIError_string(),
               Context::URI_ERROR_FUNCTION_INDEX);

  {  // -- C o m p i l e E r r o r
    Handle<JSObject> dummy = factory->NewJSObject(isolate_->object_function());
    InstallError(isolate_, dummy, factory->CompileError_string(),
                 Context::WASM_COMPILE_ERROR_FUNCTION_INDEX);

    // -- L i n k E r r o r
    InstallError(isolate_, dummy, factory->LinkError_string(),
                 Context::WASM_LINK_ERROR_FUNCTION_INDEX);

    // -- R u n t i m e E r r o r
    InstallError(isolate_, dummy, factory->RuntimeError_string(),
                 Context::WASM_RUNTIME_ERROR_FUNCTION_INDEX);
  }

  // Initialize the embedder data slot.
  // TODO(ishell): microtask queue pointer will be moved from native context
  // to the embedder data array so we don't need an empty embedder data array.
  Handle<EmbedderDataArray> embedder_data = factory->NewEmbedderDataArray(0);
  native_context()->set_embedder_data(*embedder_data);

  {  // -- g l o b a l T h i s
    Handle<JSGlobalProxy> global_proxy(native_context()->global_proxy(),
                                       isolate_);
    JSObject::AddProperty(isolate_, global, factory->globalThis_string(),
                          global_proxy, DONT_ENUM);
  }

  {  // -- J S O N
    Handle<JSObject> json_object =
        factory->NewJSObject(isolate_->object_function(), AllocationType::kOld);
    JSObject::AddProperty(isolate_, global, "JSON", json_object, DONT_ENUM);
    SimpleInstallFunction(isolate_, json_object, "parse", Builtin::kJsonParse,
                          2, false);
    SimpleInstallFunction(isolate_, json_object, "stringify",
                          Builtin::kJsonStringify, 3, true);
    InstallToStringTag(isolate_, json_object, "JSON");
  }

  {  // -- M a t h
    Handle<JSObject> math =
        factory->NewJSObject(isolate_->object_function(), AllocationType::kOld);
    JSObject::AddProperty(isolate_, global, "Math", math, DONT_ENUM);
    SimpleInstallFunction(isolate_, math, "abs", Builtin::kMathAbs, 1, true);
    SimpleInstallFunction(isolate_, math, "acos", Builtin::kMathAcos, 1, true);
    SimpleInstallFunction(isolate_, math, "acosh", Builtin::kMathAcosh, 1,
                          true);
    SimpleInstallFunction(isolate_, math, "asin", Builtin::kMathAsin, 1, true);
    SimpleInstallFunction(isolate_, math, "asinh", Builtin::kMathAsinh, 1,
                          true);
    SimpleInstallFunction(isolate_, math, "atan", Builtin::kMathAtan, 1, true);
    SimpleInstallFunction(isolate_, math, "atanh", Builtin::kMathAtanh, 1,
                          true);
    SimpleInstallFunction(isolate_, math, "atan2", Builtin::kMathAtan2, 2,
                          true);
    SimpleInstallFunction(isolate_, math, "ceil", Builtin::kMathCeil, 1, true);
    SimpleInstallFunction(isolate_, math, "cbrt", Builtin::kMathCbrt, 1, true);
    SimpleInstallFunction(isolate_, math, "expm1", Builtin::kMathExpm1, 1,
                          true);
    SimpleInstallFunction(isolate_, math, "clz32", Builtin::kMathClz32, 1,
                          true);
    SimpleInstallFunction(isolate_, math, "cos", Builtin::kMathCos, 1, true);
    SimpleInstallFunction(isolate_, math, "cosh", Builtin::kMathCosh, 1, true);
    SimpleInstallFunction(isolate_, math, "exp", Builtin::kMathExp, 1, true);
    Handle<JSFunction> math_floor = SimpleInstallFunction(
        isolate_, math, "floor", Builtin::kMathFloor, 1, true);
    native_context()->set_math_floor(*math_floor);
    SimpleInstallFunction(isolate_, math, "fround", Builtin::kMathFround, 1,
                          true);
    SimpleInstallFunction(isolate_, math, "hypot", Builtin::kMathHypot, 2,
                          false);
    SimpleInstallFunction(isolate_, math, "imul", Builtin::kMathImul, 2, true);
    SimpleInstallFunction(isolate_, math, "log", Builtin::kMathLog, 1, true);
    SimpleInstallFunction(isolate_, math, "log1p", Builtin::kMathLog1p, 1,
                          true);
    SimpleInstallFunction(isolate_, math, "log2", Builtin::kMathLog2, 1, true);
    SimpleInstallFunction(isolate_, math, "log10", Builtin::kMathLog10, 1,
                          true);
    SimpleInstallFunction(isolate_, math, "max", Builtin::kMathMax, 2, false);
    SimpleInstallFunction(isolate_, math, "min", Builtin::kMathMin, 2, false);
    Handle<JSFunction> math_pow = SimpleInstallFunction(
        isolate_, math, "pow", Builtin::kMathPow, 2, true);
    native_context()->set_math_pow(*math_pow);
    SimpleInstallFunction(isolate_, math, "random", Builtin::kMathRandom, 0,
                          true);
    SimpleInstallFunction(isolate_, math, "round", Builtin::kMathRound, 1,
                          true);
    SimpleInstallFunction(isolate_, math, "sign", Builtin::kMathSign, 1, true);
    SimpleInstallFunction(isolate_, math, "sin", Builtin::kMathSin, 1, true);
    SimpleInstallFunction(isolate_, math, "sinh", Builtin::kMathSinh, 1, true);
    SimpleInstallFunction(isolate_, math, "sqrt", Builtin::kMathSqrt, 1, true);
    SimpleInstallFunction(isolate_, math, "tan", Builtin::kMathTan, 1, true);
    SimpleInstallFunction(isolate_, math, "tanh", Builtin::kMathTanh, 1, true);
    SimpleInstallFunction(isolate_, math, "trunc", Builtin::kMathTrunc, 1,
                          true);

    // Install math constants.
    double const kE = base::ieee754::exp(1.0);
    double const kPI = 3.1415926535897932;
    InstallConstant(isolate_, math, "E", factory->NewNumber(kE));
    InstallConstant(isolate_, math, "LN10",
                    factory->NewNumber(base::ieee754::log(10.0)));
    InstallConstant(isolate_, math, "LN2",
                    factory->NewNumber(base::ieee754::log(2.0)));
    InstallConstant(isolate_, math, "LOG10E",
                    factory->NewNumber(base::ieee754::log10(kE)));
    InstallConstant(isolate_, math, "LOG2E",
                    factory->NewNumber(base::ieee754::log2(kE)));
    InstallConstant(isolate_, math, "PI", factory->NewNumber(kPI));
    InstallConstant(isolate_, math, "SQRT1_2",
                    factory->NewNumber(std::sqrt(0.5)));
    InstallConstant(isolate_, math, "SQRT2",
                    factory->NewNumber(std::sqrt(2.0)));
    InstallToStringTag(isolate_, math, "Math");
  }

  {  // -- C o n s o l e
    Handle<String> name = factory->InternalizeUtf8String("console");

    Handle<NativeContext> context(isolate()->native_context());
    Handle<SharedFunctionInfo> info =
        factory->NewSharedFunctionInfoForBuiltin(name, Builtin::kIllegal);
    info->set_language_mode(LanguageMode::kStrict);

    Handle<JSFunction> cons =
        Factory::JSFunctionBuilder{isolate(), info, context}.Build();
    Handle<JSObject> empty = factory->NewJSObject(isolate_->object_function());
    JSFunction::SetPrototype(cons, empty);

    Handle<JSObject> console = factory->NewJSObject(cons, AllocationType::kOld);
    DCHECK(console->IsJSObject());
    JSObject::AddProperty(isolate_, global, name, console, DONT_ENUM);
    SimpleInstallFunction(isolate_, console, "debug", Builtin::kConsoleDebug, 0,
                          false, NONE);
    SimpleInstallFunction(isolate_, console, "error", Builtin::kConsoleError, 0,
                          false, NONE);
    SimpleInstallFunction(isolate_, console, "info", Builtin::kConsoleInfo, 0,
                          false, NONE);
    SimpleInstallFunction(isolate_, console, "log", Builtin::kConsoleLog, 0,
                          false, NONE);
    SimpleInstallFunction(isolate_, console, "warn", Builtin::kConsoleWarn, 0,
                          false, NONE);
    SimpleInstallFunction(isolate_, console, "dir", Builtin::kConsoleDir, 0,
                          false, NONE);
    SimpleInstallFunction(isolate_, console, "dirxml", Builtin::kConsoleDirXml,
                          0, false, NONE);
    SimpleInstallFunction(isolate_, console, "table", Builtin::kConsoleTable, 0,
                          false, NONE);
    SimpleInstallFunction(isolate_, console, "trace", Builtin::kConsoleTrace, 0,
                          false, NONE);
    SimpleInstallFunction(isolate_, console, "group", Builtin::kConsoleGroup, 0,
                          false, NONE);
    SimpleInstallFunction(isolate_, console, "groupCollapsed",
                          Builtin::kConsoleGroupCollapsed, 0, false, NONE);
    SimpleInstallFunction(isolate_, console, "groupEnd",
                          Builtin::kConsoleGroupEnd, 0, false, NONE);
    SimpleInstallFunction(isolate_, console, "clear", Builtin::kConsoleClear, 0,
                          false, NONE);
    SimpleInstallFunction(isolate_, console, "count", Builtin::kConsoleCount, 0,
                          false, NONE);
    SimpleInstallFunction(isolate_, console, "countReset",
                          Builtin::kConsoleCountReset, 0, false, NONE);
    SimpleInstallFunction(isolate_, console, "assert",
                          Builtin::kFastConsoleAssert, 0, false, NONE);
    SimpleInstallFunction(isolate_, console, "profile",
                          Builtin::kConsoleProfile, 0, false, NONE);
    SimpleInstallFunction(isolate_, console, "profileEnd",
                          Builtin::kConsoleProfileEnd, 0, false, NONE);
    SimpleInstallFunction(isolate_, console, "time", Builtin::kConsoleTime, 0,
                          false, NONE);
    SimpleInstallFunction(isolate_, console, "timeLog",
                          Builtin::kConsoleTimeLog, 0, false, NONE);
    SimpleInstallFunction(isolate_, console, "timeEnd",
                          Builtin::kConsoleTimeEnd, 0, false, NONE);
    SimpleInstallFunction(isolate_, console, "timeStamp",
                          Builtin::kConsoleTimeStamp, 0, false, NONE);
    SimpleInstallFunction(isolate_, console, "context",
                          Builtin::kConsoleContext, 1, true, NONE);
    InstallToStringTag(isolate_, console, "Object");
  }

#ifdef V8_INTL_SUPPORT
  {  // -- I n t l
    Handle<JSObject> intl =
        factory->NewJSObject(isolate_->object_function(), AllocationType::kOld);
    JSObject::AddProperty(isolate_, global, "Intl", intl, DONT_ENUM);

    // ecma402 #sec-Intl-toStringTag
    // The initial value of the @@toStringTag property is the string value
    // *"Intl"*.
    InstallToStringTag(isolate_, intl, "Intl");

    SimpleInstallFunction(isolate(), intl, "getCanonicalLocales",
                          Builtin::kIntlGetCanonicalLocales, 1, false);

    {  // -- D a t e T i m e F o r m a t
      Handle<JSFunction> date_time_format_constructor = InstallFunction(
          isolate_, intl, "DateTimeFormat", JS_DATE_TIME_FORMAT_TYPE,
          JSDateTimeFormat::kHeaderSize, 0, factory->the_hole_value(),
          Builtin::kDateTimeFormatConstructor);
      date_time_format_constructor->shared().set_length(0);
      date_time_format_constructor->shared().DontAdaptArguments();
      InstallWithIntrinsicDefaultProto(
          isolate_, date_time_format_constructor,
          Context::INTL_DATE_TIME_FORMAT_FUNCTION_INDEX);

      SimpleInstallFunction(
          isolate(), date_time_format_constructor, "supportedLocalesOf",
          Builtin::kDateTimeFormatSupportedLocalesOf, 1, false);

      Handle<JSObject> prototype(
          JSObject::cast(date_time_format_constructor->prototype()), isolate_);

      InstallToStringTag(isolate_, prototype, "Intl.DateTimeFormat");

      SimpleInstallFunction(isolate_, prototype, "resolvedOptions",
                            Builtin::kDateTimeFormatPrototypeResolvedOptions, 0,
                            false);

      SimpleInstallFunction(isolate_, prototype, "formatToParts",
                            Builtin::kDateTimeFormatPrototypeFormatToParts, 1,
                            false);

      SimpleInstallGetter(isolate_, prototype, factory->format_string(),
                          Builtin::kDateTimeFormatPrototypeFormat, false);

      SimpleInstallFunction(isolate_, prototype, "formatRange",
                            Builtin::kDateTimeFormatPrototypeFormatRange, 2,
                            false);
      SimpleInstallFunction(isolate_, prototype, "formatRangeToParts",
                            Builtin::kDateTimeFormatPrototypeFormatRangeToParts,
                            2, false);
    }

    {  // -- N u m b e r F o r m a t
      Handle<JSFunction> number_format_constructor = InstallFunction(
          isolate_, intl, "NumberFormat", JS_NUMBER_FORMAT_TYPE,
          JSNumberFormat::kHeaderSize, 0, factory->the_hole_value(),
          Builtin::kNumberFormatConstructor);
      number_format_constructor->shared().set_length(0);
      number_format_constructor->shared().DontAdaptArguments();
      InstallWithIntrinsicDefaultProto(
          isolate_, number_format_constructor,
          Context::INTL_NUMBER_FORMAT_FUNCTION_INDEX);

      SimpleInstallFunction(isolate(), number_format_constructor,
                            "supportedLocalesOf",
                            Builtin::kNumberFormatSupportedLocalesOf, 1, false);

      Handle<JSObject> prototype(
          JSObject::cast(number_format_constructor->prototype()), isolate_);

      InstallToStringTag(isolate_, prototype, "Intl.NumberFormat");

      SimpleInstallFunction(isolate_, prototype, "resolvedOptions",
                            Builtin::kNumberFormatPrototypeResolvedOptions, 0,
                            false);

      SimpleInstallFunction(isolate_, prototype, "formatToParts",
                            Builtin::kNumberFormatPrototypeFormatToParts, 1,
                            false);
      SimpleInstallGetter(isolate_, prototype, factory->format_string(),
                          Builtin::kNumberFormatPrototypeFormatNumber, false);
    }

    {  // -- C o l l a t o r
      Handle<JSFunction> collator_constructor = InstallFunction(
          isolate_, intl, "Collator", JS_COLLATOR_TYPE, JSCollator::kHeaderSize,
          0, factory->the_hole_value(), Builtin::kCollatorConstructor);
      collator_constructor->shared().DontAdaptArguments();
      InstallWithIntrinsicDefaultProto(isolate_, collator_constructor,
                                       Context::INTL_COLLATOR_FUNCTION_INDEX);

      SimpleInstallFunction(isolate(), collator_constructor,
                            "supportedLocalesOf",
                            Builtin::kCollatorSupportedLocalesOf, 1, false);

      Handle<JSObject> prototype(
          JSObject::cast(collator_constructor->prototype()), isolate_);

      InstallToStringTag(isolate_, prototype, "Intl.Collator");

      SimpleInstallFunction(isolate_, prototype, "resolvedOptions",
                            Builtin::kCollatorPrototypeResolvedOptions, 0,
                            false);

      SimpleInstallGetter(isolate_, prototype, factory->compare_string(),
                          Builtin::kCollatorPrototypeCompare, false);
    }

    {  // -- V 8 B r e a k I t e r a t o r
      Handle<JSFunction> v8_break_iterator_constructor = InstallFunction(
          isolate_, intl, "v8BreakIterator", JS_V8_BREAK_ITERATOR_TYPE,
          JSV8BreakIterator::kHeaderSize, 0, factory->the_hole_value(),
          Builtin::kV8BreakIteratorConstructor);
      v8_break_iterator_constructor->shared().DontAdaptArguments();

      SimpleInstallFunction(
          isolate_, v8_break_iterator_constructor, "supportedLocalesOf",
          Builtin::kV8BreakIteratorSupportedLocalesOf, 1, false);

      Handle<JSObject> prototype(
          JSObject::cast(v8_break_iterator_constructor->prototype()), isolate_);

      InstallToStringTag(isolate_, prototype, factory->Object_string());

      SimpleInstallFunction(isolate_, prototype, "resolvedOptions",
                            Builtin::kV8BreakIteratorPrototypeResolvedOptions,
                            0, false);

      SimpleInstallGetter(isolate_, prototype, factory->adoptText_string(),
                          Builtin::kV8BreakIteratorPrototypeAdoptText, false);

      SimpleInstallGetter(isolate_, prototype, factory->first_string(),
                          Builtin::kV8BreakIteratorPrototypeFirst, false);

      SimpleInstallGetter(isolate_, prototype, factory->next_string(),
                          Builtin::kV8BreakIteratorPrototypeNext, false);

      SimpleInstallGetter(isolate_, prototype, factory->current_string(),
                          Builtin::kV8BreakIteratorPrototypeCurrent, false);

      SimpleInstallGetter(isolate_, prototype, factory->breakType_string(),
                          Builtin::kV8BreakIteratorPrototypeBreakType, false);
    }

    {  // -- P l u r a l R u l e s
      Handle<JSFunction> plural_rules_constructor = InstallFunction(
          isolate_, intl, "PluralRules", JS_PLURAL_RULES_TYPE,
          JSPluralRules::kHeaderSize, 0, factory->the_hole_value(),
          Builtin::kPluralRulesConstructor);
      plural_rules_constructor->shared().DontAdaptArguments();
      InstallWithIntrinsicDefaultProto(
          isolate_, plural_rules_constructor,
          Context::INTL_PLURAL_RULES_FUNCTION_INDEX);

      SimpleInstallFunction(isolate(), plural_rules_constructor,
                            "supportedLocalesOf",
                            Builtin::kPluralRulesSupportedLocalesOf, 1, false);

      Handle<JSObject> prototype(
          JSObject::cast(plural_rules_constructor->prototype()), isolate_);

      InstallToStringTag(isolate_, prototype, "Intl.PluralRules");

      SimpleInstallFunction(isolate_, prototype, "resolvedOptions",
                            Builtin::kPluralRulesPrototypeResolvedOptions, 0,
                            false);

      SimpleInstallFunction(isolate_, prototype, "select",
                            Builtin::kPluralRulesPrototypeSelect, 1, false);
    }

    {  // -- R e l a t i v e T i m e F o r m a t
      Handle<JSFunction> relative_time_format_fun = InstallFunction(
          isolate(), intl, "RelativeTimeFormat", JS_RELATIVE_TIME_FORMAT_TYPE,
          JSRelativeTimeFormat::kHeaderSize, 0, factory->the_hole_value(),
          Builtin::kRelativeTimeFormatConstructor);
      relative_time_format_fun->shared().set_length(0);
      relative_time_format_fun->shared().DontAdaptArguments();
      InstallWithIntrinsicDefaultProto(
          isolate_, relative_time_format_fun,
          Context::INTL_RELATIVE_TIME_FORMAT_FUNCTION_INDEX);

      SimpleInstallFunction(
          isolate(), relative_time_format_fun, "supportedLocalesOf",
          Builtin::kRelativeTimeFormatSupportedLocalesOf, 1, false);

      // Setup %RelativeTimeFormatPrototype%.
      Handle<JSObject> prototype(
          JSObject::cast(relative_time_format_fun->instance_prototype()),
          isolate());

      InstallToStringTag(isolate(), prototype, "Intl.RelativeTimeFormat");

      SimpleInstallFunction(
          isolate(), prototype, "resolvedOptions",
          Builtin::kRelativeTimeFormatPrototypeResolvedOptions, 0, false);
      SimpleInstallFunction(isolate(), prototype, "format",
                            Builtin::kRelativeTimeFormatPrototypeFormat, 2,
                            false);
      SimpleInstallFunction(isolate(), prototype, "formatToParts",
                            Builtin::kRelativeTimeFormatPrototypeFormatToParts,
                            2, false);
    }

    {  // -- L i s t F o r m a t
      Handle<JSFunction> list_format_fun = InstallFunction(
          isolate(), intl, "ListFormat", JS_LIST_FORMAT_TYPE,
          JSListFormat::kHeaderSize, 0, factory->the_hole_value(),
          Builtin::kListFormatConstructor);
      list_format_fun->shared().set_length(0);
      list_format_fun->shared().DontAdaptArguments();
      InstallWithIntrinsicDefaultProto(
          isolate_, list_format_fun, Context::INTL_LIST_FORMAT_FUNCTION_INDEX);

      SimpleInstallFunction(isolate(), list_format_fun, "supportedLocalesOf",
                            Builtin::kListFormatSupportedLocalesOf, 1, false);

      // Setup %ListFormatPrototype%.
      Handle<JSObject> prototype(
          JSObject::cast(list_format_fun->instance_prototype()), isolate());

      InstallToStringTag(isolate(), prototype, "Intl.ListFormat");

      SimpleInstallFunction(isolate(), prototype, "resolvedOptions",
                            Builtin::kListFormatPrototypeResolvedOptions, 0,
                            false);
      SimpleInstallFunction(isolate(), prototype, "format",
                            Builtin::kListFormatPrototypeFormat, 1, false);
      SimpleInstallFunction(isolate(), prototype, "formatToParts",
                            Builtin::kListFormatPrototypeFormatToParts, 1,
                            false);
    }

    {  // -- L o c a l e
      Handle<JSFunction> locale_fun = InstallFunction(
          isolate(), intl, "Locale", JS_LOCALE_TYPE, JSLocale::kHeaderSize, 0,
          factory->the_hole_value(), Builtin::kLocaleConstructor);
      InstallWithIntrinsicDefaultProto(isolate(), locale_fun,
                                       Context::INTL_LOCALE_FUNCTION_INDEX);
      locale_fun->shared().set_length(1);
      locale_fun->shared().DontAdaptArguments();

      // Setup %LocalePrototype%.
      Handle<JSObject> prototype(
          JSObject::cast(locale_fun->instance_prototype()), isolate());

      InstallToStringTag(isolate(), prototype, "Intl.Locale");

      SimpleInstallFunction(isolate(), prototype, "toString",
                            Builtin::kLocalePrototypeToString, 0, false);
      SimpleInstallFunction(isolate(), prototype, "maximize",
                            Builtin::kLocalePrototypeMaximize, 0, false);
      SimpleInstallFunction(isolate(), prototype, "minimize",
                            Builtin::kLocalePrototypeMinimize, 0, false);
      // Base locale getters.
      SimpleInstallGetter(isolate(), prototype, factory->language_string(),
                          Builtin::kLocalePrototypeLanguage, true);
      SimpleInstallGetter(isolate(), prototype, factory->script_string(),
                          Builtin::kLocalePrototypeScript, true);
      SimpleInstallGetter(isolate(), prototype, factory->region_string(),
                          Builtin::kLocalePrototypeRegion, true);
      SimpleInstallGetter(isolate(), prototype, factory->baseName_string(),
                          Builtin::kLocalePrototypeBaseName, true);
      // Unicode extension getters.
      SimpleInstallGetter(isolate(), prototype, factory->calendar_string(),
                          Builtin::kLocalePrototypeCalendar, true);
      SimpleInstallGetter(isolate(), prototype, factory->caseFirst_string(),
                          Builtin::kLocalePrototypeCaseFirst, true);
      SimpleInstallGetter(isolate(), prototype, factory->collation_string(),
                          Builtin::kLocalePrototypeCollation, true);
      SimpleInstallGetter(isolate(), prototype, factory->hourCycle_string(),
                          Builtin::kLocalePrototypeHourCycle, true);
      SimpleInstallGetter(isolate(), prototype, factory->numeric_string(),
                          Builtin::kLocalePrototypeNumeric, true);
      SimpleInstallGetter(isolate(), prototype,
                          factory->numberingSystem_string(),
                          Builtin::kLocalePrototypeNumberingSystem, true);
    }

    {  // -- D i s p l a y N a m e s
      Handle<JSFunction> display_names_fun = InstallFunction(
          isolate(), intl, "DisplayNames", JS_DISPLAY_NAMES_TYPE,
          JSDisplayNames::kHeaderSize, 0, factory->the_hole_value(),
          Builtin::kDisplayNamesConstructor);
      display_names_fun->shared().set_length(2);
      display_names_fun->shared().DontAdaptArguments();
      InstallWithIntrinsicDefaultProto(
          isolate(), display_names_fun,
          Context::INTL_DISPLAY_NAMES_FUNCTION_INDEX);

      SimpleInstallFunction(isolate(), display_names_fun, "supportedLocalesOf",
                            Builtin::kDisplayNamesSupportedLocalesOf, 1, false);

      {
        // Setup %DisplayNamesPrototype%.
        Handle<JSObject> prototype(
            JSObject::cast(display_names_fun->instance_prototype()), isolate());

        InstallToStringTag(isolate(), prototype, "Intl.DisplayNames");

        SimpleInstallFunction(isolate(), prototype, "resolvedOptions",
                              Builtin::kDisplayNamesPrototypeResolvedOptions, 0,
                              false);

        SimpleInstallFunction(isolate(), prototype, "of",
                              Builtin::kDisplayNamesPrototypeOf, 1, false);
      }
    }

    {  // -- S e g m e n t e r
      Handle<JSFunction> segmenter_fun = InstallFunction(
          isolate(), intl, "Segmenter", JS_SEGMENTER_TYPE,
          JSSegmenter::kHeaderSize, 0, factory->the_hole_value(),
          Builtin::kSegmenterConstructor);
      segmenter_fun->shared().set_length(0);
      segmenter_fun->shared().DontAdaptArguments();
      InstallWithIntrinsicDefaultProto(isolate_, segmenter_fun,
                                       Context::INTL_SEGMENTER_FUNCTION_INDEX);
      SimpleInstallFunction(isolate(), segmenter_fun, "supportedLocalesOf",
                            Builtin::kSegmenterSupportedLocalesOf, 1, false);
      {
        // Setup %SegmenterPrototype%.
        Handle<JSObject> prototype(
            JSObject::cast(segmenter_fun->instance_prototype()), isolate());
        // #sec-intl.segmenter.prototype-@@tostringtag
        //
        // Intl.Segmenter.prototype [ @@toStringTag ]
        //
        // The initial value of the @@toStringTag property is the String value
        // "Intl.Segmenter".
        InstallToStringTag(isolate(), prototype, "Intl.Segmenter");
        SimpleInstallFunction(isolate(), prototype, "resolvedOptions",
                              Builtin::kSegmenterPrototypeResolvedOptions, 0,
                              false);
        SimpleInstallFunction(isolate(), prototype, "segment",
                              Builtin::kSegmenterPrototypeSegment, 1, false);
      }
      {
        // Setup %SegmentsPrototype%.
        Handle<JSObject> prototype = factory->NewJSObject(
            isolate()->object_function(), AllocationType::kOld);
        Handle<String> name_string =
            Name::ToFunctionName(isolate(), factory->Segments_string())
                .ToHandleChecked();
        Handle<JSFunction> segments_fun = CreateFunction(
            isolate(), name_string, JS_SEGMENTS_TYPE, JSSegments::kHeaderSize,
            0, prototype, Builtin::kIllegal);
        segments_fun->shared().set_native(false);
        segments_fun->shared().set_length(0);
        segments_fun->shared().DontAdaptArguments();
        SimpleInstallFunction(isolate(), prototype, "containing",
                              Builtin::kSegmentsPrototypeContaining, 1, false);
        InstallFunctionAtSymbol(isolate_, prototype, factory->iterator_symbol(),
                                "[Symbol.iterator]",
                                Builtin::kSegmentsPrototypeIterator, 0, true,
                                DONT_ENUM);
        Handle<Map> segments_map(segments_fun->initial_map(), isolate());
        native_context()->set_intl_segments_map(*segments_map);
      }
      {
        // Setup %SegmentIteratorPrototype%.
        Handle<JSObject> iterator_prototype(
            native_context()->initial_iterator_prototype(), isolate());
        Handle<JSObject> prototype = factory->NewJSObject(
            isolate()->object_function(), AllocationType::kOld);
        JSObject::ForceSetPrototype(isolate(), prototype, iterator_prototype);
        // #sec-%segmentiteratorprototype%.@@tostringtag
        //
        // %SegmentIteratorPrototype% [ @@toStringTag ]
        //
        // The initial value of the @@toStringTag property is the String value
        // "Segmenter String Iterator".
        InstallToStringTag(isolate(), prototype, "Segmenter String Iterator");
        SimpleInstallFunction(isolate(), prototype, "next",
                              Builtin::kSegmentIteratorPrototypeNext, 0, false);
        // Setup SegmentIterator constructor.
        Handle<String> name_string =
            Name::ToFunctionName(isolate(), factory->SegmentIterator_string())
                .ToHandleChecked();
        Handle<JSFunction> segment_iterator_fun = CreateFunction(
            isolate(), name_string, JS_SEGMENT_ITERATOR_TYPE,
            JSSegmentIterator::kHeaderSize, 0, prototype, Builtin::kIllegal);
        segment_iterator_fun->shared().set_native(false);
        Handle<Map> segment_iterator_map(segment_iterator_fun->initial_map(),
                                         isolate());
        native_context()->set_intl_segment_iterator_map(*segment_iterator_map);
      }
    }
  }
#endif  // V8_INTL_SUPPORT

  {  // -- A r r a y B u f f e r
    Handle<String> name = factory->ArrayBuffer_string();
    Handle<JSFunction> array_buffer_fun = CreateArrayBuffer(name, ARRAY_BUFFER);
    JSObject::AddProperty(isolate_, global, name, array_buffer_fun, DONT_ENUM);
    InstallWithIntrinsicDefaultProto(isolate_, array_buffer_fun,
                                     Context::ARRAY_BUFFER_FUN_INDEX);
    InstallSpeciesGetter(isolate_, array_buffer_fun);

    Handle<JSFunction> array_buffer_noinit_fun = SimpleCreateFunction(
        isolate_,
        factory->InternalizeUtf8String(
            "arrayBufferConstructor_DoNotInitialize"),
        Builtin::kArrayBufferConstructor_DoNotInitialize, 1, false);
    native_context()->set_array_buffer_noinit_fun(*array_buffer_noinit_fun);
  }

  {  // -- S h a r e d A r r a y B u f f e r
    Handle<String> name = factory->SharedArrayBuffer_string();
    Handle<JSFunction> shared_array_buffer_fun =
        CreateArrayBuffer(name, SHARED_ARRAY_BUFFER);
    InstallWithIntrinsicDefaultProto(isolate_, shared_array_buffer_fun,
                                     Context::SHARED_ARRAY_BUFFER_FUN_INDEX);
    InstallSpeciesGetter(isolate_, shared_array_buffer_fun);
  }

  {  // R e s i z a b l e A r r a y B u f f e r
    Handle<String> name = factory->ResizableArrayBuffer_string();
    Handle<JSFunction> resizable_array_buffer_fun =
        CreateArrayBuffer(name, RESIZABLE_ARRAY_BUFFER);
    InstallWithIntrinsicDefaultProto(isolate_, resizable_array_buffer_fun,
                                     Context::RESIZABLE_ARRAY_BUFFER_FUN_INDEX);
    InstallSpeciesGetter(isolate_, resizable_array_buffer_fun);
  }

  {  // G r o w a b l e S h a r e d A r r a y B u f f e r
    Handle<String> name = factory->GrowableSharedArrayBuffer_string();
    Handle<JSFunction> growable_shared_array_buffer_fun =
        CreateArrayBuffer(name, GROWABLE_SHARED_ARRAY_BUFFER);
    InstallWithIntrinsicDefaultProto(
        isolate_, growable_shared_array_buffer_fun,
        Context::GROWABLE_SHARED_ARRAY_BUFFER_FUN_INDEX);
    InstallSpeciesGetter(isolate_, growable_shared_array_buffer_fun);
  }

  {  // -- A t o m i c s
    Handle<JSObject> atomics_object =
        factory->NewJSObject(isolate_->object_function(), AllocationType::kOld);
    native_context()->set_atomics_object(*atomics_object);

    SimpleInstallFunction(isolate_, atomics_object, "load",
                          Builtin::kAtomicsLoad, 2, true);
    SimpleInstallFunction(isolate_, atomics_object, "store",
                          Builtin::kAtomicsStore, 3, true);
    SimpleInstallFunction(isolate_, atomics_object, "add", Builtin::kAtomicsAdd,
                          3, true);
    SimpleInstallFunction(isolate_, atomics_object, "sub", Builtin::kAtomicsSub,
                          3, true);
    SimpleInstallFunction(isolate_, atomics_object, "and", Builtin::kAtomicsAnd,
                          3, true);
    SimpleInstallFunction(isolate_, atomics_object, "or", Builtin::kAtomicsOr,
                          3, true);
    SimpleInstallFunction(isolate_, atomics_object, "xor", Builtin::kAtomicsXor,
                          3, true);
    SimpleInstallFunction(isolate_, atomics_object, "exchange",
                          Builtin::kAtomicsExchange, 3, true);
    SimpleInstallFunction(isolate_, atomics_object, "compareExchange",
                          Builtin::kAtomicsCompareExchange, 4, true);
    SimpleInstallFunction(isolate_, atomics_object, "isLockFree",
                          Builtin::kAtomicsIsLockFree, 1, true);
    SimpleInstallFunction(isolate_, atomics_object, "wait",
                          Builtin::kAtomicsWait, 4, true);
    SimpleInstallFunction(isolate(), atomics_object, "waitAsync",
                          Builtin::kAtomicsWaitAsync, 4, true);
    SimpleInstallFunction(isolate_, atomics_object, "notify",
                          Builtin::kAtomicsNotify, 3, true);
  }

  {  // -- T y p e d A r r a y
    Handle<JSFunction> typed_array_fun = CreateFunction(
        isolate_, factory->InternalizeUtf8String("TypedArray"),
        JS_TYPED_ARRAY_TYPE, JSTypedArray::kHeaderSize, 0,
        factory->the_hole_value(), Builtin::kTypedArrayBaseConstructor);
    typed_array_fun->shared().set_native(false);
    typed_array_fun->shared().set_length(0);
    InstallSpeciesGetter(isolate_, typed_array_fun);
    native_context()->set_typed_array_function(*typed_array_fun);

    SimpleInstallFunction(isolate_, typed_array_fun, "of",
                          Builtin::kTypedArrayOf, 0, false);
    SimpleInstallFunction(isolate_, typed_array_fun, "from",
                          Builtin::kTypedArrayFrom, 1, false);

    // Setup %TypedArrayPrototype%.
    Handle<JSObject> prototype(
        JSObject::cast(typed_array_fun->instance_prototype()), isolate());
    native_context()->set_typed_array_prototype(*prototype);

    // Install the "buffer", "byteOffset", "byteLength", "length"
    // and @@toStringTag getters on the {prototype}.
    SimpleInstallGetter(isolate_, prototype, factory->buffer_string(),
                        Builtin::kTypedArrayPrototypeBuffer, false);
    SimpleInstallGetter(isolate_, prototype, factory->byte_length_string(),
                        Builtin::kTypedArrayPrototypeByteLength, true);
    SimpleInstallGetter(isolate_, prototype, factory->byte_offset_string(),
                        Builtin::kTypedArrayPrototypeByteOffset, true);
    SimpleInstallGetter(isolate_, prototype, factory->length_string(),
                        Builtin::kTypedArrayPrototypeLength, true);
    SimpleInstallGetter(isolate_, prototype, factory->to_string_tag_symbol(),
                        Builtin::kTypedArrayPrototypeToStringTag, true);

    // Install "keys", "values" and "entries" methods on the {prototype}.
    InstallFunctionWithBuiltinId(isolate_, prototype, "entries",
                                 Builtin::kTypedArrayPrototypeEntries, 0, true);

    InstallFunctionWithBuiltinId(isolate_, prototype, "keys",
                                 Builtin::kTypedArrayPrototypeKeys, 0, true);

    Handle<JSFunction> values = InstallFunctionWithBuiltinId(
        isolate_, prototype, "values", Builtin::kTypedArrayPrototypeValues, 0,
        true);
    JSObject::AddProperty(isolate_, prototype, factory->iterator_symbol(),
                          values, DONT_ENUM);

    // TODO(caitp): alphasort accessors/methods
    SimpleInstallFunction(isolate_, prototype, "copyWithin",
                          Builtin::kTypedArrayPrototypeCopyWithin, 2, false);
    SimpleInstallFunction(isolate_, prototype, "every",
                          Builtin::kTypedArrayPrototypeEvery, 1, false);
    SimpleInstallFunction(isolate_, prototype, "fill",
                          Builtin::kTypedArrayPrototypeFill, 1, false);
    SimpleInstallFunction(isolate_, prototype, "filter",
                          Builtin::kTypedArrayPrototypeFilter, 1, false);
    SimpleInstallFunction(isolate_, prototype, "find",
                          Builtin::kTypedArrayPrototypeFind, 1, false);
    SimpleInstallFunction(isolate_, prototype, "findIndex",
                          Builtin::kTypedArrayPrototypeFindIndex, 1, false);
    SimpleInstallFunction(isolate_, prototype, "forEach",
                          Builtin::kTypedArrayPrototypeForEach, 1, false);
    SimpleInstallFunction(isolate_, prototype, "includes",
                          Builtin::kTypedArrayPrototypeIncludes, 1, false);
    SimpleInstallFunction(isolate_, prototype, "indexOf",
                          Builtin::kTypedArrayPrototypeIndexOf, 1, false);
    SimpleInstallFunction(isolate_, prototype, "join",
                          Builtin::kTypedArrayPrototypeJoin, 1, false);
    SimpleInstallFunction(isolate_, prototype, "lastIndexOf",
                          Builtin::kTypedArrayPrototypeLastIndexOf, 1, false);
    SimpleInstallFunction(isolate_, prototype, "map",
                          Builtin::kTypedArrayPrototypeMap, 1, false);
    SimpleInstallFunction(isolate_, prototype, "reverse",
                          Builtin::kTypedArrayPrototypeReverse, 0, false);
    SimpleInstallFunction(isolate_, prototype, "reduce",
                          Builtin::kTypedArrayPrototypeReduce, 1, false);
    SimpleInstallFunction(isolate_, prototype, "reduceRight",
                          Builtin::kTypedArrayPrototypeReduceRight, 1, false);
    SimpleInstallFunction(isolate_, prototype, "set",
                          Builtin::kTypedArrayPrototypeSet, 1, false);
    SimpleInstallFunction(isolate_, prototype, "slice",
                          Builtin::kTypedArrayPrototypeSlice, 2, false);
    SimpleInstallFunction(isolate_, prototype, "some",
                          Builtin::kTypedArrayPrototypeSome, 1, false);
    SimpleInstallFunction(isolate_, prototype, "sort",
                          Builtin::kTypedArrayPrototypeSort, 1, false);
    SimpleInstallFunction(isolate_, prototype, "subarray",
                          Builtin::kTypedArrayPrototypeSubArray, 2, false);
    SimpleInstallFunction(isolate_, prototype, "toLocaleString",
                          Builtin::kTypedArrayPrototypeToLocaleString, 0,
                          false);
    JSObject::AddProperty(isolate_, prototype, factory->toString_string(),
                          array_prototype_to_string_fun, DONT_ENUM);
  }

  {// -- T y p e d A r r a y s
#define INSTALL_TYPED_ARRAY(Type, type, TYPE, ctype)                         \
  {                                                                          \
    Handle<JSFunction> fun = InstallTypedArray(                              \
        #Type "Array", TYPE##_ELEMENTS, TYPE##_TYPED_ARRAY_CONSTRUCTOR_TYPE, \
        Context::RAB_GSAB_##TYPE##_ARRAY_MAP_INDEX);                         \
    InstallWithIntrinsicDefaultProto(isolate_, fun,                          \
                                     Context::TYPE##_ARRAY_FUN_INDEX);       \
  }
  TYPED_ARRAYS(INSTALL_TYPED_ARRAY)
#undef INSTALL_TYPED_ARRAY
  }

  {  // -- D a t a V i e w
    Handle<JSFunction> data_view_fun = InstallFunction(
        isolate_, global, "DataView", JS_DATA_VIEW_TYPE,
        JSDataView::kSizeWithEmbedderFields, 0, factory->the_hole_value(),
        Builtin::kDataViewConstructor);
    InstallWithIntrinsicDefaultProto(isolate_, data_view_fun,
                                     Context::DATA_VIEW_FUN_INDEX);
    data_view_fun->shared().set_length(1);
    data_view_fun->shared().DontAdaptArguments();

    // Setup %DataViewPrototype%.
    Handle<JSObject> prototype(
        JSObject::cast(data_view_fun->instance_prototype()), isolate());

    InstallToStringTag(isolate_, prototype, "DataView");

    // Install the "buffer", "byteOffset" and "byteLength" getters
    // on the {prototype}.
    SimpleInstallGetter(isolate_, prototype, factory->buffer_string(),
                        Builtin::kDataViewPrototypeGetBuffer, false);
    SimpleInstallGetter(isolate_, prototype, factory->byte_length_string(),
                        Builtin::kDataViewPrototypeGetByteLength, false);
    SimpleInstallGetter(isolate_, prototype, factory->byte_offset_string(),
                        Builtin::kDataViewPrototypeGetByteOffset, false);

    SimpleInstallFunction(isolate_, prototype, "getInt8",
                          Builtin::kDataViewPrototypeGetInt8, 1, false);
    SimpleInstallFunction(isolate_, prototype, "setInt8",
                          Builtin::kDataViewPrototypeSetInt8, 2, false);
    SimpleInstallFunction(isolate_, prototype, "getUint8",
                          Builtin::kDataViewPrototypeGetUint8, 1, false);
    SimpleInstallFunction(isolate_, prototype, "setUint8",
                          Builtin::kDataViewPrototypeSetUint8, 2, false);
    SimpleInstallFunction(isolate_, prototype, "getInt16",
                          Builtin::kDataViewPrototypeGetInt16, 1, false);
    SimpleInstallFunction(isolate_, prototype, "setInt16",
                          Builtin::kDataViewPrototypeSetInt16, 2, false);
    SimpleInstallFunction(isolate_, prototype, "getUint16",
                          Builtin::kDataViewPrototypeGetUint16, 1, false);
    SimpleInstallFunction(isolate_, prototype, "setUint16",
                          Builtin::kDataViewPrototypeSetUint16, 2, false);
    SimpleInstallFunction(isolate_, prototype, "getInt32",
                          Builtin::kDataViewPrototypeGetInt32, 1, false);
    SimpleInstallFunction(isolate_, prototype, "setInt32",
                          Builtin::kDataViewPrototypeSetInt32, 2, false);
    SimpleInstallFunction(isolate_, prototype, "getUint32",
                          Builtin::kDataViewPrototypeGetUint32, 1, false);
    SimpleInstallFunction(isolate_, prototype, "setUint32",
                          Builtin::kDataViewPrototypeSetUint32, 2, false);
    SimpleInstallFunction(isolate_, prototype, "getFloat32",
                          Builtin::kDataViewPrototypeGetFloat32, 1, false);
    SimpleInstallFunction(isolate_, prototype, "setFloat32",
                          Builtin::kDataViewPrototypeSetFloat32, 2, false);
    SimpleInstallFunction(isolate_, prototype, "getFloat64",
                          Builtin::kDataViewPrototypeGetFloat64, 1, false);
    SimpleInstallFunction(isolate_, prototype, "setFloat64",
                          Builtin::kDataViewPrototypeSetFloat64, 2, false);
    SimpleInstallFunction(isolate_, prototype, "getBigInt64",
                          Builtin::kDataViewPrototypeGetBigInt64, 1, false);
    SimpleInstallFunction(isolate_, prototype, "setBigInt64",
                          Builtin::kDataViewPrototypeSetBigInt64, 2, false);
    SimpleInstallFunction(isolate_, prototype, "getBigUint64",
                          Builtin::kDataViewPrototypeGetBigUint64, 1, false);
    SimpleInstallFunction(isolate_, prototype, "setBigUint64",
                          Builtin::kDataViewPrototypeSetBigUint64, 2, false);
  }

  {  // -- M a p
    Handle<JSFunction> js_map_fun = InstallFunction(
        isolate_, global, "Map", JS_MAP_TYPE, JSMap::kHeaderSize, 0,
        factory->the_hole_value(), Builtin::kMapConstructor);
    InstallWithIntrinsicDefaultProto(isolate_, js_map_fun,
                                     Context::JS_MAP_FUN_INDEX);

    Handle<SharedFunctionInfo> shared(js_map_fun->shared(), isolate_);
    shared->DontAdaptArguments();
    shared->set_length(0);

    // Setup %MapPrototype%.
    Handle<JSObject> prototype(JSObject::cast(js_map_fun->instance_prototype()),
                               isolate());

    InstallToStringTag(isolate_, prototype, factory->Map_string());

    Handle<JSFunction> map_get = SimpleInstallFunction(
        isolate_, prototype, "get", Builtin::kMapPrototypeGet, 1, true);
    native_context()->set_map_get(*map_get);

    Handle<JSFunction> map_set = SimpleInstallFunction(
        isolate_, prototype, "set", Builtin::kMapPrototypeSet, 2, true);
    // Check that index of "set" function in JSCollection is correct.
    DCHECK_EQ(JSCollection::kAddFunctionDescriptorIndex,
              prototype->map().LastAdded().as_int());
    native_context()->set_map_set(*map_set);

    Handle<JSFunction> map_has = SimpleInstallFunction(
        isolate_, prototype, "has", Builtin::kMapPrototypeHas, 1, true);
    native_context()->set_map_has(*map_has);

    Handle<JSFunction> map_delete = SimpleInstallFunction(
        isolate_, prototype, "delete", Builtin::kMapPrototypeDelete, 1, true);
    native_context()->set_map_delete(*map_delete);

    SimpleInstallFunction(isolate_, prototype, "clear",
                          Builtin::kMapPrototypeClear, 0, true);
    Handle<JSFunction> entries = SimpleInstallFunction(
        isolate_, prototype, "entries", Builtin::kMapPrototypeEntries, 0, true);
    JSObject::AddProperty(isolate_, prototype, factory->iterator_symbol(),
                          entries, DONT_ENUM);
    SimpleInstallFunction(isolate_, prototype, "forEach",
                          Builtin::kMapPrototypeForEach, 1, false);
    SimpleInstallFunction(isolate_, prototype, "keys",
                          Builtin::kMapPrototypeKeys, 0, true);
    SimpleInstallGetter(isolate_, prototype,
                        factory->InternalizeUtf8String("size"),
                        Builtin::kMapPrototypeGetSize, true);
    SimpleInstallFunction(isolate_, prototype, "values",
                          Builtin::kMapPrototypeValues, 0, true);

    native_context()->set_initial_map_prototype_map(prototype->map());

    InstallSpeciesGetter(isolate_, js_map_fun);

    DCHECK(js_map_fun->HasFastProperties());

    native_context()->set_js_map_map(js_map_fun->initial_map());
  }

  {  // -- B i g I n t
    Handle<JSFunction> bigint_fun =
        InstallFunction(isolate_, global, "BigInt", JS_PRIMITIVE_WRAPPER_TYPE,
                        JSPrimitiveWrapper::kHeaderSize, 0,
                        factory->the_hole_value(), Builtin::kBigIntConstructor);
    bigint_fun->shared().DontAdaptArguments();
    bigint_fun->shared().set_length(1);
    InstallWithIntrinsicDefaultProto(isolate_, bigint_fun,
                                     Context::BIGINT_FUNCTION_INDEX);

    // Install the properties of the BigInt constructor.
    // asUintN(bits, bigint)
    SimpleInstallFunction(isolate_, bigint_fun, "asUintN",
                          Builtin::kBigIntAsUintN, 2, false);
    // asIntN(bits, bigint)
    SimpleInstallFunction(isolate_, bigint_fun, "asIntN",
                          Builtin::kBigIntAsIntN, 2, false);

    // Set up the %BigIntPrototype%.
    Handle<JSObject> prototype(JSObject::cast(bigint_fun->instance_prototype()),
                               isolate_);
    JSFunction::SetPrototype(bigint_fun, prototype);

    // Install the properties of the BigInt.prototype.
    // "constructor" is created implicitly by InstallFunction() above.
    // toLocaleString([reserved1 [, reserved2]])
    SimpleInstallFunction(isolate_, prototype, "toLocaleString",
                          Builtin::kBigIntPrototypeToLocaleString, 0, false);
    // toString([radix])
    SimpleInstallFunction(isolate_, prototype, "toString",
                          Builtin::kBigIntPrototypeToString, 0, false);
    // valueOf()
    SimpleInstallFunction(isolate_, prototype, "valueOf",
                          Builtin::kBigIntPrototypeValueOf, 0, false);
    // @@toStringTag
    InstallToStringTag(isolate_, prototype, factory->BigInt_string());
  }

  {  // -- S e t
    Handle<JSFunction> js_set_fun = InstallFunction(
        isolate_, global, "Set", JS_SET_TYPE, JSSet::kHeaderSize, 0,
        factory->the_hole_value(), Builtin::kSetConstructor);
    InstallWithIntrinsicDefaultProto(isolate_, js_set_fun,
                                     Context::JS_SET_FUN_INDEX);

    Handle<SharedFunctionInfo> shared(js_set_fun->shared(), isolate_);
    shared->DontAdaptArguments();
    shared->set_length(0);

    // Setup %SetPrototype%.
    Handle<JSObject> prototype(JSObject::cast(js_set_fun->instance_prototype()),
                               isolate());

    InstallToStringTag(isolate_, prototype, factory->Set_string());

    Handle<JSFunction> set_has = SimpleInstallFunction(
        isolate_, prototype, "has", Builtin::kSetPrototypeHas, 1, true);
    native_context()->set_set_has(*set_has);

    Handle<JSFunction> set_add = SimpleInstallFunction(
        isolate_, prototype, "add", Builtin::kSetPrototypeAdd, 1, true);
    // Check that index of "add" function in JSCollection is correct.
    DCHECK_EQ(JSCollection::kAddFunctionDescriptorIndex,
              prototype->map().LastAdded().as_int());
    native_context()->set_set_add(*set_add);

    Handle<JSFunction> set_delete = SimpleInstallFunction(
        isolate_, prototype, "delete", Builtin::kSetPrototypeDelete, 1, true);
    native_context()->set_set_delete(*set_delete);

    SimpleInstallFunction(isolate_, prototype, "clear",
                          Builtin::kSetPrototypeClear, 0, true);
    SimpleInstallFunction(isolate_, prototype, "entries",
                          Builtin::kSetPrototypeEntries, 0, true);
    SimpleInstallFunction(isolate_, prototype, "forEach",
                          Builtin::kSetPrototypeForEach, 1, false);
    SimpleInstallGetter(isolate_, prototype,
                        factory->InternalizeUtf8String("size"),
                        Builtin::kSetPrototypeGetSize, true);
    Handle<JSFunction> values = SimpleInstallFunction(
        isolate_, prototype, "values", Builtin::kSetPrototypeValues, 0, true);
    JSObject::AddProperty(isolate_, prototype, factory->keys_string(), values,
                          DONT_ENUM);
    JSObject::AddProperty(isolate_, prototype, factory->iterator_symbol(),
                          values, DONT_ENUM);

    native_context()->set_initial_set_prototype_map(prototype->map());
    native_context()->set_initial_set_prototype(*prototype);

    InstallSpeciesGetter(isolate_, js_set_fun);

    DCHECK(js_set_fun->HasFastProperties());

    native_context()->set_js_set_map(js_set_fun->initial_map());
    CHECK_NE(prototype->map().ptr(),
             isolate_->initial_object_prototype()->map().ptr());
    prototype->map().set_instance_type(JS_SET_PROTOTYPE_TYPE);
  }

  {  // -- J S M o d u l e N a m e s p a c e
    Handle<Map> map = factory->NewMap(
        JS_MODULE_NAMESPACE_TYPE, JSModuleNamespace::kSize,
        TERMINAL_FAST_ELEMENTS_KIND, JSModuleNamespace::kInObjectFieldCount);
    map->SetConstructor(native_context()->object_function());
    Map::SetPrototype(isolate(), map, isolate_->factory()->null_value());
    Map::EnsureDescriptorSlack(isolate_, map, 1);
    native_context()->set_js_module_namespace_map(*map);

    {  // Install @@toStringTag.
      PropertyAttributes attribs =
          static_cast<PropertyAttributes>(DONT_DELETE | DONT_ENUM | READ_ONLY);
      Descriptor d =
          Descriptor::DataField(isolate(), factory->to_string_tag_symbol(),
                                JSModuleNamespace::kToStringTagFieldIndex,
                                attribs, Representation::Tagged());
      map->AppendDescriptor(isolate(), &d);
    }
  }

  {  // -- I t e r a t o r R e s u l t
    // Setup the map for IterResultObjects created from builtins in such a
    // way that it's exactly the same map as the one produced by object
    // literals in the form `{value, done}`. This way we have better sharing
    // of maps (i.e. less polymorphism) and also make it possible to hit the
    // fast-paths in various builtins (i.e. promises and collections) with
    // user defined iterators.
    Handle<Map> map = factory->ObjectLiteralMapFromCache(native_context(), 2);

    // value
    map = Map::CopyWithField(isolate(), map, factory->value_string(),
                             FieldType::Any(isolate()), NONE,
                             PropertyConstness::kConst,
                             Representation::Tagged(), INSERT_TRANSITION)
              .ToHandleChecked();

    // done
    map = Map::CopyWithField(isolate(), map, factory->done_string(),
                             FieldType::Any(isolate()), NONE,
                             PropertyConstness::kConst,
                             Representation::HeapObject(), INSERT_TRANSITION)
              .ToHandleChecked();

    native_context()->set_iterator_result_map(*map);
  }

  {  // -- W e a k M a p
    Handle<JSFunction> cons = InstallFunction(
        isolate_, global, "WeakMap", JS_WEAK_MAP_TYPE, JSWeakMap::kHeaderSize,
        0, factory->the_hole_value(), Builtin::kWeakMapConstructor);
    InstallWithIntrinsicDefaultProto(isolate_, cons,
                                     Context::JS_WEAK_MAP_FUN_INDEX);

    Handle<SharedFunctionInfo> shared(cons->shared(), isolate_);
    shared->DontAdaptArguments();
    shared->set_length(0);

    // Setup %WeakMapPrototype%.
    Handle<JSObject> prototype(JSObject::cast(cons->instance_prototype()),
                               isolate());

    Handle<JSFunction> weakmap_delete =
        SimpleInstallFunction(isolate_, prototype, "delete",
                              Builtin::kWeakMapPrototypeDelete, 1, true);
    native_context()->set_weakmap_delete(*weakmap_delete);

    Handle<JSFunction> weakmap_get = SimpleInstallFunction(
        isolate_, prototype, "get", Builtin::kWeakMapGet, 1, true);
    native_context()->set_weakmap_get(*weakmap_get);

    Handle<JSFunction> weakmap_set = SimpleInstallFunction(
        isolate_, prototype, "set", Builtin::kWeakMapPrototypeSet, 2, true);
    // Check that index of "set" function in JSWeakCollection is correct.
    DCHECK_EQ(JSWeakCollection::kAddFunctionDescriptorIndex,
              prototype->map().LastAdded().as_int());

    native_context()->set_weakmap_set(*weakmap_set);
    SimpleInstallFunction(isolate_, prototype, "has",
                          Builtin::kWeakMapPrototypeHas, 1, true);

    InstallToStringTag(isolate_, prototype, "WeakMap");

    native_context()->set_initial_weakmap_prototype_map(prototype->map());
  }

  {  // -- W e a k S e t
    Handle<JSFunction> cons = InstallFunction(
        isolate_, global, "WeakSet", JS_WEAK_SET_TYPE, JSWeakSet::kHeaderSize,
        0, factory->the_hole_value(), Builtin::kWeakSetConstructor);
    InstallWithIntrinsicDefaultProto(isolate_, cons,
                                     Context::JS_WEAK_SET_FUN_INDEX);

    Handle<SharedFunctionInfo> shared(cons->shared(), isolate_);
    shared->DontAdaptArguments();
    shared->set_length(0);

    // Setup %WeakSetPrototype%.
    Handle<JSObject> prototype(JSObject::cast(cons->instance_prototype()),
                               isolate());

    SimpleInstallFunction(isolate_, prototype, "delete",
                          Builtin::kWeakSetPrototypeDelete, 1, true);
    SimpleInstallFunction(isolate_, prototype, "has",
                          Builtin::kWeakSetPrototypeHas, 1, true);

    Handle<JSFunction> weakset_add = SimpleInstallFunction(
        isolate_, prototype, "add", Builtin::kWeakSetPrototypeAdd, 1, true);
    // Check that index of "add" function in JSWeakCollection is correct.
    DCHECK_EQ(JSWeakCollection::kAddFunctionDescriptorIndex,
              prototype->map().LastAdded().as_int());

    native_context()->set_weakset_add(*weakset_add);

    InstallToStringTag(isolate_, prototype,
                       factory->InternalizeUtf8String("WeakSet"));

    native_context()->set_initial_weakset_prototype_map(prototype->map());
  }

  {  // -- P r o x y
    CreateJSProxyMaps();
    // Proxy function map has prototype slot for storing initial map but does
    // not have a prototype property.
    Handle<Map> proxy_function_map = Map::Copy(
        isolate_, isolate_->strict_function_without_prototype_map(), "Proxy");
    proxy_function_map->set_is_constructor(true);

    Handle<String> name = factory->Proxy_string();
    Handle<JSFunction> proxy_function = CreateFunctionForBuiltin(
        isolate(), name, proxy_function_map, Builtin::kProxyConstructor);

    isolate_->proxy_map()->SetConstructor(*proxy_function);

    proxy_function->shared().set_internal_formal_parameter_count(2);
    proxy_function->shared().set_length(2);

    native_context()->set_proxy_function(*proxy_function);
    JSObject::AddProperty(isolate_, global, name, proxy_function, DONT_ENUM);

    DCHECK(!proxy_function->has_prototype_property());

    SimpleInstallFunction(isolate_, proxy_function, "revocable",
                          Builtin::kProxyRevocable, 2, true);
  }

  {  // -- R e f l e c t
    Handle<String> reflect_string = factory->InternalizeUtf8String("Reflect");
    Handle<JSObject> reflect =
        factory->NewJSObject(isolate_->object_function(), AllocationType::kOld);
    JSObject::AddProperty(isolate_, global, reflect_string, reflect, DONT_ENUM);
    InstallToStringTag(isolate_, reflect, reflect_string);

    SimpleInstallFunction(isolate_, reflect, "defineProperty",
                          Builtin::kReflectDefineProperty, 3, true);

    SimpleInstallFunction(isolate_, reflect, "deleteProperty",
                          Builtin::kReflectDeleteProperty, 2, true);

    Handle<JSFunction> apply = SimpleInstallFunction(
        isolate_, reflect, "apply", Builtin::kReflectApply, 3, false);
    native_context()->set_reflect_apply(*apply);

    Handle<JSFunction> construct = SimpleInstallFunction(
        isolate_, reflect, "construct", Builtin::kReflectConstruct, 2, false);
    native_context()->set_reflect_construct(*construct);

    SimpleInstallFunction(isolate_, reflect, "get", Builtin::kReflectGet, 2,
                          false);
    SimpleInstallFunction(isolate_, reflect, "getOwnPropertyDescriptor",
                          Builtin::kReflectGetOwnPropertyDescriptor, 2, true);
    SimpleInstallFunction(isolate_, reflect, "getPrototypeOf",
                          Builtin::kReflectGetPrototypeOf, 1, true);
    SimpleInstallFunction(isolate_, reflect, "has", Builtin::kReflectHas, 2,
                          true);
    SimpleInstallFunction(isolate_, reflect, "isExtensible",
                          Builtin::kReflectIsExtensible, 1, true);
    SimpleInstallFunction(isolate_, reflect, "ownKeys",
                          Builtin::kReflectOwnKeys, 1, true);
    SimpleInstallFunction(isolate_, reflect, "preventExtensions",
                          Builtin::kReflectPreventExtensions, 1, true);
    SimpleInstallFunction(isolate_, reflect, "set", Builtin::kReflectSet, 3,
                          false);
    SimpleInstallFunction(isolate_, reflect, "setPrototypeOf",
                          Builtin::kReflectSetPrototypeOf, 2, true);
  }

  {  // --- B o u n d F u n c t i o n
    Handle<Map> map =
        factory->NewMap(JS_BOUND_FUNCTION_TYPE, JSBoundFunction::kHeaderSize,
                        TERMINAL_FAST_ELEMENTS_KIND, 0);
    map->SetConstructor(native_context()->object_function());
    map->set_is_callable(true);
    Map::SetPrototype(isolate(), map, empty_function);

    PropertyAttributes roc_attribs =
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY);
    Map::EnsureDescriptorSlack(isolate_, map, 2);

    {  // length
      STATIC_ASSERT(JSFunctionOrBoundFunction::kLengthDescriptorIndex == 0);
      Descriptor d = Descriptor::AccessorConstant(
          factory->length_string(), factory->bound_function_length_accessor(),
          roc_attribs);
      map->AppendDescriptor(isolate(), &d);
    }

    {  // name
      STATIC_ASSERT(JSFunctionOrBoundFunction::kNameDescriptorIndex == 1);
      Descriptor d = Descriptor::AccessorConstant(
          factory->name_string(), factory->bound_function_name_accessor(),
          roc_attribs);
      map->AppendDescriptor(isolate(), &d);
    }
    native_context()->set_bound_function_without_constructor_map(*map);

    map = Map::Copy(isolate_, map, "IsConstructor");
    map->set_is_constructor(true);
    native_context()->set_bound_function_with_constructor_map(*map);
  }

  {  // -- F i n a l i z a t i o n R e g i s t r y
    Handle<JSFunction> finalization_registry_fun = InstallFunction(
        isolate_, global, factory->FinalizationRegistry_string(),
        JS_FINALIZATION_REGISTRY_TYPE, JSFinalizationRegistry::kHeaderSize, 0,
        factory->the_hole_value(), Builtin::kFinalizationRegistryConstructor);
    InstallWithIntrinsicDefaultProto(
        isolate_, finalization_registry_fun,
        Context::JS_FINALIZATION_REGISTRY_FUNCTION_INDEX);

    finalization_registry_fun->shared().DontAdaptArguments();
    finalization_registry_fun->shared().set_length(1);

    Handle<JSObject> finalization_registry_prototype(
        JSObject::cast(finalization_registry_fun->instance_prototype()),
        isolate());

    InstallToStringTag(isolate_, finalization_registry_prototype,
                       factory->FinalizationRegistry_string());

    SimpleInstallFunction(isolate_, finalization_registry_prototype, "register",
                          Builtin::kFinalizationRegistryRegister, 2, false);

    SimpleInstallFunction(isolate_, finalization_registry_prototype,
                          "unregister",
                          Builtin::kFinalizationRegistryUnregister, 1, false);

    // The cleanupSome function is created but not exposed, as it is used
    // internally by InvokeFinalizationRegistryCleanupFromTask.
    //
    // It is exposed by FLAG_harmony_weak_refs_with_cleanup_some.
    Handle<JSFunction> cleanup_some_fun = SimpleCreateFunction(
        isolate_, factory->InternalizeUtf8String("cleanupSome"),
        Builtin::kFinalizationRegistryPrototypeCleanupSome, 0, false);
    native_context()->set_finalization_registry_cleanup_some(*cleanup_some_fun);
  }

  {  // -- W e a k R e f
    Handle<JSFunction> weak_ref_fun = InstallFunction(
        isolate_, global, "WeakRef", JS_WEAK_REF_TYPE, JSWeakRef::kHeaderSize,
        0, factory->the_hole_value(), Builtin::kWeakRefConstructor);
    InstallWithIntrinsicDefaultProto(isolate_, weak_ref_fun,
                                     Context::JS_WEAK_REF_FUNCTION_INDEX);

    weak_ref_fun->shared().DontAdaptArguments();
    weak_ref_fun->shared().set_length(1);

    Handle<JSObject> weak_ref_prototype(
        JSObject::cast(weak_ref_fun->instance_prototype()), isolate());

    InstallToStringTag(isolate_, weak_ref_prototype, factory->WeakRef_string());

    SimpleInstallFunction(isolate_, weak_ref_prototype, "deref",
                          Builtin::kWeakRefDeref, 0, true);
  }

  {  // --- sloppy arguments map
    Handle<String> arguments_string = factory->Arguments_string();
    Handle<JSFunction> function = CreateFunctionForBuiltinWithPrototype(
        isolate(), arguments_string, Builtin::kIllegal,
        isolate()->initial_object_prototype(), JS_ARGUMENTS_OBJECT_TYPE,
        JSSloppyArgumentsObject::kSize, 2, MUTABLE);
    Handle<Map> map(function->initial_map(), isolate());

    // Create the descriptor array for the arguments object.
    Map::EnsureDescriptorSlack(isolate_, map, 2);

    {  // length
      Descriptor d =
          Descriptor::DataField(isolate(), factory->length_string(),
                                JSSloppyArgumentsObject::kLengthIndex,
                                DONT_ENUM, Representation::Tagged());
      map->AppendDescriptor(isolate(), &d);
    }
    {  // callee
      Descriptor d =
          Descriptor::DataField(isolate(), factory->callee_string(),
                                JSSloppyArgumentsObject::kCalleeIndex,
                                DONT_ENUM, Representation::Tagged());
      map->AppendDescriptor(isolate(), &d);
    }
    // @@iterator method is added later.

    native_context()->set_sloppy_arguments_map(*map);

    DCHECK(!map->is_dictionary_map());
    DCHECK(IsObjectElementsKind(map->elements_kind()));
  }

  {  // --- fast and slow aliased arguments map
    Handle<Map> map = isolate_->sloppy_arguments_map();
    map = Map::Copy(isolate_, map, "FastAliasedArguments");
    map->set_elements_kind(FAST_SLOPPY_ARGUMENTS_ELEMENTS);
    DCHECK_EQ(2, map->GetInObjectProperties());
    native_context()->set_fast_aliased_arguments_map(*map);

    map = Map::Copy(isolate_, map, "SlowAliasedArguments");
    map->set_elements_kind(SLOW_SLOPPY_ARGUMENTS_ELEMENTS);
    DCHECK_EQ(2, map->GetInObjectProperties());
    native_context()->set_slow_aliased_arguments_map(*map);
  }

  {  // --- strict mode arguments map
    const PropertyAttributes attributes =
        static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);

    // Create the ThrowTypeError function.
    Handle<AccessorPair> callee = factory->NewAccessorPair();

    Handle<JSFunction> poison = GetThrowTypeErrorIntrinsic();

    // Install the ThrowTypeError function.
    callee->set_getter(*poison);
    callee->set_setter(*poison);

    // Create the map. Allocate one in-object field for length.
    Handle<Map> map =
        factory->NewMap(JS_ARGUMENTS_OBJECT_TYPE,
                        JSStrictArgumentsObject::kSize, PACKED_ELEMENTS, 1);
    // Create the descriptor array for the arguments object.
    Map::EnsureDescriptorSlack(isolate_, map, 2);

    {  // length
      Descriptor d =
          Descriptor::DataField(isolate(), factory->length_string(),
                                JSStrictArgumentsObject::kLengthIndex,
                                DONT_ENUM, Representation::Tagged());
      map->AppendDescriptor(isolate(), &d);
    }
    {  // callee
      Descriptor d = Descriptor::AccessorConstant(factory->callee_string(),
                                                  callee, attributes);
      map->AppendDescriptor(isolate(), &d);
    }
    // @@iterator method is added later.

    DCHECK_EQ(native_context()->object_function().prototype(),
              *isolate_->initial_object_prototype());
    Map::SetPrototype(isolate(), map, isolate_->initial_object_prototype());

    // Copy constructor from the sloppy arguments boilerplate.
    map->SetConstructor(
        native_context()->sloppy_arguments_map().GetConstructor());

    native_context()->set_strict_arguments_map(*map);

    DCHECK(!map->is_dictionary_map());
    DCHECK(IsObjectElementsKind(map->elements_kind()));
  }

  {  // --- context extension
    // Create a function for the context extension objects.
    Handle<JSFunction> context_extension_fun = CreateFunction(
        isolate_, factory->empty_string(), JS_CONTEXT_EXTENSION_OBJECT_TYPE,
        JSObject::kHeaderSize, 0, factory->the_hole_value(), Builtin::kIllegal);
    native_context()->set_context_extension_function(*context_extension_fun);
  }

  {
    // Set up the call-as-function delegate.
    Handle<JSFunction> delegate =
        SimpleCreateFunction(isolate_, factory->empty_string(),
                             Builtin::kHandleApiCallAsFunction, 0, false);
    native_context()->set_call_as_function_delegate(*delegate);
  }

  {
    // Set up the call-as-constructor delegate.
    Handle<JSFunction> delegate =
        SimpleCreateFunction(isolate_, factory->empty_string(),
                             Builtin::kHandleApiCallAsConstructor, 0, false);
    native_context()->set_call_as_constructor_delegate(*delegate);
  }
}

Handle<JSFunction> Genesis::InstallTypedArray(const char* name,
                                              ElementsKind elements_kind,
                                              InstanceType type,
                                              int rab_gsab_initial_map_index) {
  Handle<JSObject> global =
      Handle<JSObject>(native_context()->global_object(), isolate());

  Handle<JSObject> typed_array_prototype = isolate()->typed_array_prototype();
  Handle<JSFunction> typed_array_function = isolate()->typed_array_function();

  Handle<JSFunction> result = InstallConstructor(
      isolate(), global, name, JS_TYPED_ARRAY_TYPE,
      JSTypedArray::kSizeWithEmbedderFields, 0, factory()->the_hole_value(),
      Builtin::kTypedArrayConstructor, type);
  result->initial_map().set_elements_kind(elements_kind);

  result->shared().DontAdaptArguments();
  result->shared().set_length(3);

  CHECK(JSObject::SetPrototype(result, typed_array_function, false, kDontThrow)
            .FromJust());

  Handle<Smi> bytes_per_element(
      Smi::FromInt(1 << ElementsKindToShiftSize(elements_kind)), isolate());

  InstallConstant(isolate(), result, "BYTES_PER_ELEMENT", bytes_per_element);

  // Setup prototype object.
  DCHECK(result->prototype().IsJSObject());
  Handle<JSObject> prototype(JSObject::cast(result->prototype()), isolate());

  CHECK(JSObject::SetPrototype(prototype, typed_array_prototype, false,
                               kDontThrow)
            .FromJust());

  CHECK_NE(prototype->map().ptr(),
           isolate_->initial_object_prototype()->map().ptr());
  prototype->map().set_instance_type(JS_TYPED_ARRAY_PROTOTYPE_TYPE);

  InstallConstant(isolate(), prototype, "BYTES_PER_ELEMENT", bytes_per_element);

  // RAB / GSAB backed TypedArrays don't have separate constructors, but they
  // have their own maps. Create the corresponding map here.
  Handle<Map> rab_gsab_initial_map = factory()->NewMap(
      JS_TYPED_ARRAY_TYPE, JSTypedArray::kSizeWithEmbedderFields,
      GetCorrespondingRabGsabElementsKind(elements_kind), 0);
  native_context()->set(rab_gsab_initial_map_index, *rab_gsab_initial_map,
                        UPDATE_WRITE_BARRIER, kReleaseStore);
  Map::SetPrototype(isolate(), rab_gsab_initial_map, prototype);

  return result;
}

void Genesis::InitializeExperimentalGlobal() {
#define FEATURE_INITIALIZE_GLOBAL(id, descr) InitializeGlobal_##id();

  // Initialize features from more mature to less mature, because less mature
  // features may depend on more mature features having been initialized
  // already.
  HARMONY_SHIPPING(FEATURE_INITIALIZE_GLOBAL)
  HARMONY_STAGED(FEATURE_INITIALIZE_GLOBAL)
  HARMONY_INPROGRESS(FEATURE_INITIALIZE_GLOBAL)
#undef FEATURE_INITIALIZE_GLOBAL
  InitializeGlobal_regexp_linear_flag();
}

bool Genesis::CompileExtension(Isolate* isolate, v8::Extension* extension) {
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);
  Handle<SharedFunctionInfo> function_info;

  Handle<String> source =
      isolate->factory()
          ->NewExternalStringFromOneByte(extension->source())
          .ToHandleChecked();
  DCHECK(source->IsOneByteRepresentation());

  // If we can't find the function in the cache, we compile a new
  // function and insert it into the cache.
  base::Vector<const char> name = base::CStrVector(extension->name());
  SourceCodeCache* cache = isolate->bootstrapper()->extensions_cache();
  Handle<Context> context(isolate->context(), isolate);
  DCHECK(context->IsNativeContext());

  if (!cache->Lookup(isolate, name, &function_info)) {
    Handle<String> script_name =
        factory->NewStringFromUtf8(name).ToHandleChecked();
    MaybeHandle<SharedFunctionInfo> maybe_function_info =
        Compiler::GetSharedFunctionInfoForScript(
            isolate, source, Compiler::ScriptDetails(script_name),
            ScriptOriginOptions(), extension, nullptr,
            ScriptCompiler::kNoCompileOptions,
            ScriptCompiler::kNoCacheBecauseV8Extension, EXTENSION_CODE);
    if (!maybe_function_info.ToHandle(&function_info)) return false;
    cache->Add(isolate, name, function_info);
  }

  // Set up the function context. Conceptually, we should clone the
  // function before overwriting the context but since we're in a
  // single-threaded environment it is not strictly necessary.
  Handle<JSFunction> fun =
      Factory::JSFunctionBuilder{isolate, function_info, context}.Build();

  // Call function using either the runtime object or the global
  // object as the receiver. Provide no parameters.
  Handle<Object> receiver = isolate->global_object();
  return !Execution::TryCall(isolate, fun, receiver, 0, nullptr,
                             Execution::MessageHandling::kKeepPending, nullptr)
              .is_null();
}

void Genesis::InitializeIteratorFunctions() {
  Isolate* isolate = isolate_;
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);
  Handle<NativeContext> native_context = isolate->native_context();
  Handle<JSObject> iterator_prototype(
      native_context->initial_iterator_prototype(), isolate);

  {  // -- G e n e r a t o r
    PrototypeIterator iter(isolate, native_context->generator_function_map());
    Handle<JSObject> generator_function_prototype(iter.GetCurrent<JSObject>(),
                                                  isolate);
    Handle<JSFunction> generator_function_function = CreateFunction(
        isolate, "GeneratorFunction", JS_FUNCTION_TYPE,
        JSFunction::kSizeWithPrototype, 0, generator_function_prototype,
        Builtin::kGeneratorFunctionConstructor);
    generator_function_function->set_prototype_or_initial_map(
        native_context->generator_function_map(), kReleaseStore);
    generator_function_function->shared().DontAdaptArguments();
    generator_function_function->shared().set_length(1);
    InstallWithIntrinsicDefaultProto(
        isolate, generator_function_function,
        Context::GENERATOR_FUNCTION_FUNCTION_INDEX);

    JSObject::ForceSetPrototype(isolate, generator_function_function,
                                isolate->function_function());
    JSObject::AddProperty(
        isolate, generator_function_prototype, factory->constructor_string(),
        generator_function_function,
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

    native_context->generator_function_map().SetConstructor(
        *generator_function_function);
  }

  {  // -- A s y n c G e n e r a t o r
    PrototypeIterator iter(isolate,
                           native_context->async_generator_function_map());
    Handle<JSObject> async_generator_function_prototype(
        iter.GetCurrent<JSObject>(), isolate);

    Handle<JSFunction> async_generator_function_function = CreateFunction(
        isolate, "AsyncGeneratorFunction", JS_FUNCTION_TYPE,
        JSFunction::kSizeWithPrototype, 0, async_generator_function_prototype,
        Builtin::kAsyncGeneratorFunctionConstructor);
    async_generator_function_function->set_prototype_or_initial_map(
        native_context->async_generator_function_map(), kReleaseStore);
    async_generator_function_function->shared().DontAdaptArguments();
    async_generator_function_function->shared().set_length(1);
    InstallWithIntrinsicDefaultProto(
        isolate, async_generator_function_function,
        Context::ASYNC_GENERATOR_FUNCTION_FUNCTION_INDEX);

    JSObject::ForceSetPrototype(isolate, async_generator_function_function,
                                isolate->function_function());

    JSObject::AddProperty(
        isolate, async_generator_function_prototype,
        factory->constructor_string(), async_generator_function_function,
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

    native_context->async_generator_function_map().SetConstructor(
        *async_generator_function_function);
  }

  {  // -- S e t I t e r a t o r
    // Setup %SetIteratorPrototype%.
    Handle<JSObject> prototype =
        factory->NewJSObject(isolate->object_function(), AllocationType::kOld);
    JSObject::ForceSetPrototype(isolate, prototype, iterator_prototype);

    InstallToStringTag(isolate, prototype, factory->SetIterator_string());

    // Install the next function on the {prototype}.
    InstallFunctionWithBuiltinId(isolate, prototype, "next",
                                 Builtin::kSetIteratorPrototypeNext, 0, true);
    native_context->set_initial_set_iterator_prototype(*prototype);
    CHECK_NE(prototype->map().ptr(),
             isolate_->initial_object_prototype()->map().ptr());
    prototype->map().set_instance_type(JS_SET_ITERATOR_PROTOTYPE_TYPE);

    // Setup SetIterator constructor.
    Handle<JSFunction> set_iterator_function = CreateFunction(
        isolate, "SetIterator", JS_SET_VALUE_ITERATOR_TYPE,
        JSSetIterator::kHeaderSize, 0, prototype, Builtin::kIllegal);
    set_iterator_function->shared().set_native(false);

    Handle<Map> set_value_iterator_map(set_iterator_function->initial_map(),
                                       isolate);
    native_context->set_set_value_iterator_map(*set_value_iterator_map);

    Handle<Map> set_key_value_iterator_map = Map::Copy(
        isolate, set_value_iterator_map, "JS_SET_KEY_VALUE_ITERATOR_TYPE");
    set_key_value_iterator_map->set_instance_type(
        JS_SET_KEY_VALUE_ITERATOR_TYPE);
    native_context->set_set_key_value_iterator_map(*set_key_value_iterator_map);
  }

  {  // -- M a p I t e r a t o r
    // Setup %MapIteratorPrototype%.
    Handle<JSObject> prototype =
        factory->NewJSObject(isolate->object_function(), AllocationType::kOld);
    JSObject::ForceSetPrototype(isolate, prototype, iterator_prototype);

    InstallToStringTag(isolate, prototype, factory->MapIterator_string());

    // Install the next function on the {prototype}.
    InstallFunctionWithBuiltinId(isolate, prototype, "next",
                                 Builtin::kMapIteratorPrototypeNext, 0, true);
    native_context->set_initial_map_iterator_prototype(*prototype);
    CHECK_NE(prototype->map().ptr(),
             isolate_->initial_object_prototype()->map().ptr());
    prototype->map().set_instance_type(JS_MAP_ITERATOR_PROTOTYPE_TYPE);

    // Setup MapIterator constructor.
    Handle<JSFunction> map_iterator_function = CreateFunction(
        isolate, "MapIterator", JS_MAP_KEY_ITERATOR_TYPE,
        JSMapIterator::kHeaderSize, 0, prototype, Builtin::kIllegal);
    map_iterator_function->shared().set_native(false);

    Handle<Map> map_key_iterator_map(map_iterator_function->initial_map(),
                                     isolate);
    native_context->set_map_key_iterator_map(*map_key_iterator_map);

    Handle<Map> map_key_value_iterator_map = Map::Copy(
        isolate, map_key_iterator_map, "JS_MAP_KEY_VALUE_ITERATOR_TYPE");
    map_key_value_iterator_map->set_instance_type(
        JS_MAP_KEY_VALUE_ITERATOR_TYPE);
    native_context->set_map_key_value_iterator_map(*map_key_value_iterator_map);

    Handle<Map> map_value_iterator_map =
        Map::Copy(isolate, map_key_iterator_map, "JS_MAP_VALUE_ITERATOR_TYPE");
    map_value_iterator_map->set_instance_type(JS_MAP_VALUE_ITERATOR_TYPE);
    native_context->set_map_value_iterator_map(*map_value_iterator_map);
  }

  {  // -- A s y n c F u n c t i o n
    // Builtin functions for AsyncFunction.
    PrototypeIterator iter(isolate, native_context->async_function_map());
    Handle<JSObject> async_function_prototype(iter.GetCurrent<JSObject>(),
                                              isolate);

    Handle<JSFunction> async_function_constructor = CreateFunction(
        isolate, "AsyncFunction", JS_FUNCTION_TYPE,
        JSFunction::kSizeWithPrototype, 0, async_function_prototype,
        Builtin::kAsyncFunctionConstructor);
    async_function_constructor->set_prototype_or_initial_map(
        native_context->async_function_map(), kReleaseStore);
    async_function_constructor->shared().DontAdaptArguments();
    async_function_constructor->shared().set_length(1);
    native_context->set_async_function_constructor(*async_function_constructor);
    JSObject::ForceSetPrototype(isolate, async_function_constructor,
                                isolate->function_function());

    JSObject::AddProperty(
        isolate, async_function_prototype, factory->constructor_string(),
        async_function_constructor,
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

    JSFunction::SetPrototype(async_function_constructor,
                             async_function_prototype);

    // Async functions don't have a prototype, but they use generator objects
    // under the hood to model the suspend/resume (in await). Instead of using
    // the "prototype" / initial_map machinery (like for (async) generators),
    // there's one global (per native context) map here that is used for the
    // async function generator objects. These objects never escape to user
    // JavaScript anyways.
    Handle<Map> async_function_object_map = factory->NewMap(
        JS_ASYNC_FUNCTION_OBJECT_TYPE, JSAsyncFunctionObject::kHeaderSize);
    native_context->set_async_function_object_map(*async_function_object_map);
  }
}

void Genesis::InitializeCallSiteBuiltins() {
  Factory* factory = isolate()->factory();
  HandleScope scope(isolate());
  // -- C a l l S i t e
  // Builtin functions for CallSite.

  // CallSites are a special case; the constructor is for our private use
  // only, therefore we set it up as a builtin that throws. Internally, we use
  // CallSiteUtils::Construct to create CallSite objects.

  Handle<JSFunction> callsite_fun = CreateFunction(
      isolate(), "CallSite", JS_OBJECT_TYPE, JSObject::kHeaderSize, 0,
      factory->the_hole_value(), Builtin::kUnsupportedThrower);
  callsite_fun->shared().DontAdaptArguments();
  isolate()->native_context()->set_callsite_function(*callsite_fun);

  // Setup CallSite.prototype.
  Handle<JSObject> prototype(JSObject::cast(callsite_fun->instance_prototype()),
                             isolate());

  struct FunctionInfo {
    const char* name;
    Builtin id;
  };

  FunctionInfo infos[] = {
      {"getColumnNumber", Builtin::kCallSitePrototypeGetColumnNumber},
      {"getEnclosingColumnNumber",
       Builtin::kCallSitePrototypeGetEnclosingColumnNumber},
      {"getEnclosingLineNumber",
       Builtin::kCallSitePrototypeGetEnclosingLineNumber},
      {"getEvalOrigin", Builtin::kCallSitePrototypeGetEvalOrigin},
      {"getFileName", Builtin::kCallSitePrototypeGetFileName},
      {"getFunction", Builtin::kCallSitePrototypeGetFunction},
      {"getFunctionName", Builtin::kCallSitePrototypeGetFunctionName},
      {"getLineNumber", Builtin::kCallSitePrototypeGetLineNumber},
      {"getMethodName", Builtin::kCallSitePrototypeGetMethodName},
      {"getPosition", Builtin::kCallSitePrototypeGetPosition},
      {"getPromiseIndex", Builtin::kCallSitePrototypeGetPromiseIndex},
      {"getScriptNameOrSourceURL",
       Builtin::kCallSitePrototypeGetScriptNameOrSourceURL},
      {"getThis", Builtin::kCallSitePrototypeGetThis},
      {"getTypeName", Builtin::kCallSitePrototypeGetTypeName},
      {"isAsync", Builtin::kCallSitePrototypeIsAsync},
      {"isConstructor", Builtin::kCallSitePrototypeIsConstructor},
      {"isEval", Builtin::kCallSitePrototypeIsEval},
      {"isNative", Builtin::kCallSitePrototypeIsNative},
      {"isPromiseAll", Builtin::kCallSitePrototypeIsPromiseAll},
      {"isToplevel", Builtin::kCallSitePrototypeIsToplevel},
      {"toString", Builtin::kCallSitePrototypeToString}};

  PropertyAttributes attrs =
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);

  Handle<JSFunction> fun;
  for (const FunctionInfo& info : infos) {
    SimpleInstallFunction(isolate(), prototype, info.name, info.id, 0, true,
                          attrs);
  }
}

#define EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(id) \
  void Genesis::InitializeGlobal_##id() {}

EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_regexp_sequence)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_top_level_await)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_import_assertions)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_private_brand_checks)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_class_static_blocks)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_error_cause)

#ifdef V8_INTL_SUPPORT
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_intl_best_fit_matcher)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_intl_displaynames_v2)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_intl_dateformat_day_period)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_intl_more_timezone)
#endif  // V8_INTL_SUPPORT

#undef EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE

void Genesis::InitializeGlobal_harmony_object_has_own() {
  if (!FLAG_harmony_object_has_own) return;

  Handle<JSFunction> object_function = isolate_->object_function();
  SimpleInstallFunction(isolate_, object_function, "hasOwn",
                        Builtin::kObjectHasOwn, 2, true);
}

void Genesis::InitializeGlobal_harmony_sharedarraybuffer() {
  if (!FLAG_harmony_sharedarraybuffer ||
      FLAG_enable_sharedarraybuffer_per_context) {
    return;
  }

  Handle<JSGlobalObject> global(native_context()->global_object(), isolate());

  JSObject::AddProperty(isolate_, global, "SharedArrayBuffer",
                        isolate()->shared_array_buffer_fun(), DONT_ENUM);
}

void Genesis::InitializeGlobal_harmony_atomics() {
  if (!FLAG_harmony_atomics) return;

  Handle<JSGlobalObject> global(native_context()->global_object(), isolate());

  JSObject::AddProperty(isolate_, global, "Atomics",
                        isolate()->atomics_object(), DONT_ENUM);
  InstallToStringTag(isolate_, isolate()->atomics_object(), "Atomics");
}

void Genesis::InitializeGlobal_harmony_weak_refs_with_cleanup_some() {
  if (!FLAG_harmony_weak_refs_with_cleanup_some) return;

  Handle<JSFunction> finalization_registry_fun =
      isolate()->js_finalization_registry_fun();
  Handle<JSObject> finalization_registry_prototype(
      JSObject::cast(finalization_registry_fun->instance_prototype()),
      isolate());

  JSObject::AddProperty(isolate(), finalization_registry_prototype,
                        factory()->InternalizeUtf8String("cleanupSome"),
                        isolate()->finalization_registry_cleanup_some(),
                        DONT_ENUM);
}

void Genesis::InitializeGlobal_harmony_regexp_match_indices() {
  if (!FLAG_harmony_regexp_match_indices) return;

  Handle<Map> source_map(native_context()->regexp_result_map(), isolate());
  Handle<Map> initial_map =
      Map::Copy(isolate(), source_map, "JSRegExpResult with indices");
  initial_map->set_instance_size(JSRegExpResultWithIndices::kSize);
  DCHECK_EQ(initial_map->GetInObjectProperties(),
            JSRegExpResultWithIndices::kInObjectPropertyCount);

  // indices descriptor
  {
    Descriptor d =
        Descriptor::DataField(isolate(), factory()->indices_string(),
                              JSRegExpResultWithIndices::kIndicesIndex, NONE,
                              Representation::Tagged());
    Map::EnsureDescriptorSlack(isolate(), initial_map, 1);
    initial_map->AppendDescriptor(isolate(), &d);
  }

  native_context()->set_regexp_result_with_indices_map(*initial_map);

  Handle<JSObject> prototype(native_context()->regexp_prototype(), isolate());
  SimpleInstallGetter(isolate(), prototype, factory()->has_indices_string(),
                      Builtin::kRegExpPrototypeHasIndicesGetter, true);

  // Store regexp prototype map again after change.
  native_context()->set_regexp_prototype_map(prototype->map());
}

void Genesis::InitializeGlobal_regexp_linear_flag() {
  if (!FLAG_enable_experimental_regexp_engine) return;

  Handle<JSFunction> regexp_fun(native_context()->regexp_function(), isolate());
  Handle<JSObject> regexp_prototype(
      JSObject::cast(regexp_fun->instance_prototype()), isolate());
  SimpleInstallGetter(isolate(), regexp_prototype,
                      isolate()->factory()->linear_string(),
                      Builtin::kRegExpPrototypeLinearGetter, true);

  // Store regexp prototype map again after change.
  native_context()->set_regexp_prototype_map(regexp_prototype->map());
}

void Genesis::InitializeGlobal_harmony_relative_indexing_methods() {
  if (!FLAG_harmony_relative_indexing_methods) return;

  {
    Handle<JSFunction> array_function(native_context()->array_function(),
                                      isolate());
    Handle<JSObject> array_prototype(
        JSObject::cast(array_function->instance_prototype()), isolate());

    SimpleInstallFunction(isolate(), array_prototype, "at",
                          Builtin::kArrayPrototypeAt, 1, true);

    Handle<JSObject> unscopables = Handle<JSObject>::cast(
        JSReceiver::GetProperty(isolate(), array_prototype,
                                factory()->unscopables_symbol())
            .ToHandleChecked());
    InstallTrueValuedProperty(isolate(), unscopables, "at");
  }

  {
    Handle<JSFunction> string_function(native_context()->string_function(),
                                       isolate());
    Handle<JSObject> string_prototype(
        JSObject::cast(string_function->instance_prototype()), isolate());

    SimpleInstallFunction(isolate(), string_prototype, "at",
                          Builtin::kStringPrototypeAt, 1, true);
  }

  {
    Handle<JSFunction> typed_array_function(
        native_context()->typed_array_function(), isolate());
    Handle<JSObject> typed_array_prototype(
        JSObject::cast(typed_array_function->instance_prototype()), isolate());

    SimpleInstallFunction(isolate(), typed_array_prototype, "at",
                          Builtin::kTypedArrayPrototypeAt, 1, true);
  }
}

#ifdef V8_INTL_SUPPORT

void Genesis::InitializeGlobal_harmony_intl_locale_info() {
  if (!FLAG_harmony_intl_locale_info) return;
  Handle<JSObject> prototype(
      JSObject::cast(native_context()->intl_locale_function().prototype()),
      isolate_);
  SimpleInstallGetter(isolate(), prototype, factory()->calendars_string(),
                      Builtin::kLocalePrototypeCalendars, true);
  SimpleInstallGetter(isolate(), prototype, factory()->collations_string(),
                      Builtin::kLocalePrototypeCollations, true);
  SimpleInstallGetter(isolate(), prototype, factory()->hourCycles_string(),
                      Builtin::kLocalePrototypeHourCycles, true);
  SimpleInstallGetter(isolate(), prototype,
                      factory()->numberingSystems_string(),
                      Builtin::kLocalePrototypeNumberingSystems, true);
  SimpleInstallGetter(isolate(), prototype, factory()->textInfo_string(),
                      Builtin::kLocalePrototypeTextInfo, true);
  SimpleInstallGetter(isolate(), prototype, factory()->timeZones_string(),
                      Builtin::kLocalePrototypeTimeZones, true);
  SimpleInstallGetter(isolate(), prototype, factory()->weekInfo_string(),
                      Builtin::kLocalePrototypeWeekInfo, true);
}

#endif  // V8_INTL_SUPPORT

void Genesis::InitializeGlobal_harmony_rab_gsab() {
  if (!FLAG_harmony_rab_gsab) return;

  Handle<JSGlobalObject> global(native_context()->global_object(), isolate());

  JSObject::AddProperty(isolate_, global, "ResizableArrayBuffer",
                        isolate()->resizable_array_buffer_fun(), DONT_ENUM);

  JSObject::AddProperty(isolate_, global, "GrowableSharedArrayBuffer",
                        isolate()->growable_shared_array_buffer_fun(),
                        DONT_ENUM);
}

Handle<JSFunction> Genesis::CreateArrayBuffer(
    Handle<String> name, ArrayBufferKind array_buffer_kind) {
  // Create the %ArrayBufferPrototype%
  // Setup the {prototype} with the given {name} for @@toStringTag.
  Handle<JSObject> prototype = factory()->NewJSObject(
      isolate()->object_function(), AllocationType::kOld);
  InstallToStringTag(isolate(), prototype, name);

  // Allocate the constructor with the given {prototype}.
  Handle<JSFunction> array_buffer_fun =
      CreateFunction(isolate(), name, JS_ARRAY_BUFFER_TYPE,
                     JSArrayBuffer::kSizeWithEmbedderFields, 0, prototype,
                     Builtin::kArrayBufferConstructor);
  array_buffer_fun->shared().DontAdaptArguments();
  array_buffer_fun->shared().set_length(1);

  // Install the "constructor" property on the {prototype}.
  JSObject::AddProperty(isolate(), prototype, factory()->constructor_string(),
                        array_buffer_fun, DONT_ENUM);

  switch (array_buffer_kind) {
    case ARRAY_BUFFER:
      InstallFunctionWithBuiltinId(isolate(), array_buffer_fun, "isView",
                                   Builtin::kArrayBufferIsView, 1, true);

      // Install the "byteLength" getter on the {prototype}.
      SimpleInstallGetter(isolate(), prototype, factory()->byte_length_string(),
                          Builtin::kArrayBufferPrototypeGetByteLength, false);

      SimpleInstallFunction(isolate(), prototype, "slice",
                            Builtin::kArrayBufferPrototypeSlice, 2, true);
      break;

    case SHARED_ARRAY_BUFFER:
      // Install the "byteLength" getter on the {prototype}.
      SimpleInstallGetter(isolate(), prototype, factory()->byte_length_string(),
                          Builtin::kSharedArrayBufferPrototypeGetByteLength,
                          false);

      SimpleInstallFunction(isolate(), prototype, "slice",
                            Builtin::kSharedArrayBufferPrototypeSlice, 2, true);
      break;
    case RESIZABLE_ARRAY_BUFFER:
      SimpleInstallGetter(isolate(), prototype, factory()->byte_length_string(),
                          Builtin::kResizableArrayBufferPrototypeGetByteLength,
                          false);
      SimpleInstallGetter(
          isolate(), prototype, factory()->max_byte_length_string(),
          Builtin::kResizableArrayBufferPrototypeGetMaxByteLength, false);
      SimpleInstallFunction(isolate(), prototype, "resize",
                            Builtin::kResizableArrayBufferPrototypeResize, 1,
                            true);
      break;
    case GROWABLE_SHARED_ARRAY_BUFFER:
      SimpleInstallGetter(
          isolate(), prototype, factory()->byte_length_string(),
          Builtin::kGrowableSharedArrayBufferPrototypeGetByteLength, true);
      SimpleInstallGetter(
          isolate(), prototype, factory()->max_byte_length_string(),
          Builtin::kGrowableSharedArrayBufferPrototypeGetMaxByteLength, false);
      SimpleInstallFunction(isolate(), prototype, "grow",
                            Builtin::kGrowableSharedArrayBufferPrototypeGrow, 1,
                            true);
      break;
  }

  return array_buffer_fun;
}

// TODO(jgruber): Refactor this into some kind of meaningful organization. There
// is likely no reason remaining for these objects to be installed here. For
// example, global object setup done in this function could likely move to
// InitializeGlobal.
bool Genesis::InstallABunchOfRandomThings() {
  HandleScope scope(isolate());

  auto fast_template_instantiations_cache =
      isolate()->factory()->NewFixedArrayWithHoles(
          TemplateInfo::kFastTemplateInstantiationsCacheSize);
  native_context()->set_fast_template_instantiations_cache(
      *fast_template_instantiations_cache);

  auto slow_template_instantiations_cache = SimpleNumberDictionary::New(
      isolate(), ApiNatives::kInitialFunctionCacheSize);
  native_context()->set_slow_template_instantiations_cache(
      *slow_template_instantiations_cache);

  auto wasm_debug_maps = isolate()->factory()->empty_fixed_array();
  native_context()->set_wasm_debug_maps(*wasm_debug_maps);

  // Store the map for the %ObjectPrototype% after the natives has been compiled
  // and the Object function has been set up.
  {
    Handle<JSFunction> object_function(native_context()->object_function(),
                                       isolate());
    DCHECK(JSObject::cast(object_function->initial_map().prototype())
               .HasFastProperties());
    native_context()->set_object_function_prototype_map(
        HeapObject::cast(object_function->initial_map().prototype()).map());
  }

  // Store the map for the %StringPrototype% after the natives has been compiled
  // and the String function has been set up.
  Handle<JSFunction> string_function(native_context()->string_function(),
                                     isolate());
  JSObject string_function_prototype =
      JSObject::cast(string_function->initial_map().prototype());
  DCHECK(string_function_prototype.HasFastProperties());
  native_context()->set_string_function_prototype_map(
      string_function_prototype.map());

  Handle<JSGlobalObject> global_object =
      handle(native_context()->global_object(), isolate());

  // Install Global.decodeURI.
  InstallFunctionWithBuiltinId(isolate(), global_object, "decodeURI",
                               Builtin::kGlobalDecodeURI, 1, false);

  // Install Global.decodeURIComponent.
  InstallFunctionWithBuiltinId(isolate(), global_object, "decodeURIComponent",
                               Builtin::kGlobalDecodeURIComponent, 1, false);

  // Install Global.encodeURI.
  InstallFunctionWithBuiltinId(isolate(), global_object, "encodeURI",
                               Builtin::kGlobalEncodeURI, 1, false);

  // Install Global.encodeURIComponent.
  InstallFunctionWithBuiltinId(isolate(), global_object, "encodeURIComponent",
                               Builtin::kGlobalEncodeURIComponent, 1, false);

  // Install Global.escape.
  InstallFunctionWithBuiltinId(isolate(), global_object, "escape",
                               Builtin::kGlobalEscape, 1, false);

  // Install Global.unescape.
  InstallFunctionWithBuiltinId(isolate(), global_object, "unescape",
                               Builtin::kGlobalUnescape, 1, false);

  // Install Global.eval.
  {
    Handle<JSFunction> eval = SimpleInstallFunction(
        isolate(), global_object, "eval", Builtin::kGlobalEval, 1, false);
    native_context()->set_global_eval_fun(*eval);
  }

  // Install Global.isFinite
  InstallFunctionWithBuiltinId(isolate(), global_object, "isFinite",
                               Builtin::kGlobalIsFinite, 1, true);

  // Install Global.isNaN
  InstallFunctionWithBuiltinId(isolate(), global_object, "isNaN",
                               Builtin::kGlobalIsNaN, 1, true);

  // Install Array builtin functions.
  {
    Handle<JSFunction> array_constructor(native_context()->array_function(),
                                         isolate());
    Handle<JSArray> proto(JSArray::cast(array_constructor->prototype()),
                          isolate());

    // Verification of important array prototype properties.
    Object length = proto->length();
    CHECK(length.IsSmi());
    CHECK_EQ(Smi::ToInt(length), 0);
    CHECK(proto->HasSmiOrObjectElements());
    // This is necessary to enable fast checks for absence of elements
    // on Array.prototype and below.
    proto->set_elements(ReadOnlyRoots(heap()).empty_fixed_array());
  }

  // Create a map for accessor property descriptors (a variant of JSObject
  // that predefines four properties get, set, configurable and enumerable).
  {
    // AccessorPropertyDescriptor initial map.
    Handle<Map> map =
        factory()->NewMap(JS_OBJECT_TYPE, JSAccessorPropertyDescriptor::kSize,
                          TERMINAL_FAST_ELEMENTS_KIND, 4);
    // Create the descriptor array for the property descriptor object.
    Map::EnsureDescriptorSlack(isolate(), map, 4);

    {  // get
      Descriptor d =
          Descriptor::DataField(isolate(), factory()->get_string(),
                                JSAccessorPropertyDescriptor::kGetIndex, NONE,
                                Representation::Tagged());
      map->AppendDescriptor(isolate(), &d);
    }
    {  // set
      Descriptor d =
          Descriptor::DataField(isolate(), factory()->set_string(),
                                JSAccessorPropertyDescriptor::kSetIndex, NONE,
                                Representation::Tagged());
      map->AppendDescriptor(isolate(), &d);
    }
    {  // enumerable
      Descriptor d =
          Descriptor::DataField(isolate(), factory()->enumerable_string(),
                                JSAccessorPropertyDescriptor::kEnumerableIndex,
                                NONE, Representation::Tagged());
      map->AppendDescriptor(isolate(), &d);
    }
    {  // configurable
      Descriptor d = Descriptor::DataField(
          isolate(), factory()->configurable_string(),
          JSAccessorPropertyDescriptor::kConfigurableIndex, NONE,
          Representation::Tagged());
      map->AppendDescriptor(isolate(), &d);
    }

    Map::SetPrototype(isolate(), map, isolate()->initial_object_prototype());
    map->SetConstructor(native_context()->object_function());

    native_context()->set_accessor_property_descriptor_map(*map);
  }

  // Create a map for data property descriptors (a variant of JSObject
  // that predefines four properties value, writable, configurable and
  // enumerable).
  {
    // DataPropertyDescriptor initial map.
    Handle<Map> map =
        factory()->NewMap(JS_OBJECT_TYPE, JSDataPropertyDescriptor::kSize,
                          TERMINAL_FAST_ELEMENTS_KIND, 4);
    // Create the descriptor array for the property descriptor object.
    Map::EnsureDescriptorSlack(isolate(), map, 4);

    {  // value
      Descriptor d =
          Descriptor::DataField(isolate(), factory()->value_string(),
                                JSDataPropertyDescriptor::kValueIndex, NONE,
                                Representation::Tagged());
      map->AppendDescriptor(isolate(), &d);
    }
    {  // writable
      Descriptor d =
          Descriptor::DataField(isolate(), factory()->writable_string(),
                                JSDataPropertyDescriptor::kWritableIndex, NONE,
                                Representation::Tagged());
      map->AppendDescriptor(isolate(), &d);
    }
    {  // enumerable
      Descriptor d =
          Descriptor::DataField(isolate(), factory()->enumerable_string(),
                                JSDataPropertyDescriptor::kEnumerableIndex,
                                NONE, Representation::Tagged());
      map->AppendDescriptor(isolate(), &d);
    }
    {  // configurable
      Descriptor d =
          Descriptor::DataField(isolate(), factory()->configurable_string(),
                                JSDataPropertyDescriptor::kConfigurableIndex,
                                NONE, Representation::Tagged());
      map->AppendDescriptor(isolate(), &d);
    }

    Map::SetPrototype(isolate(), map, isolate()->initial_object_prototype());
    map->SetConstructor(native_context()->object_function());

    native_context()->set_data_property_descriptor_map(*map);
  }

  // Create a constructor for RegExp results (a variant of Array that
  // predefines the properties index, input, and groups).
  {
    // JSRegExpResult initial map.
    // Add additional slack to the initial map in case regexp_match_indices
    // are enabled to account for the additional descriptor.
    Handle<Map> initial_map = CreateInitialMapForArraySubclass(
        JSRegExpResult::kSize, JSRegExpResult::kInObjectPropertyCount);

    // index descriptor.
    {
      Descriptor d = Descriptor::DataField(isolate(), factory()->index_string(),
                                           JSRegExpResult::kIndexIndex, NONE,
                                           Representation::Tagged());
      initial_map->AppendDescriptor(isolate(), &d);
    }

    // input descriptor.
    {
      Descriptor d = Descriptor::DataField(isolate(), factory()->input_string(),
                                           JSRegExpResult::kInputIndex, NONE,
                                           Representation::Tagged());
      initial_map->AppendDescriptor(isolate(), &d);
    }

    // groups descriptor.
    {
      Descriptor d = Descriptor::DataField(
          isolate(), factory()->groups_string(), JSRegExpResult::kGroupsIndex,
          NONE, Representation::Tagged());
      initial_map->AppendDescriptor(isolate(), &d);
    }

    // Private internal only fields. All of the remaining fields have special
    // symbols to prevent their use in Javascript.
    {
      PropertyAttributes attribs = DONT_ENUM;

      // names descriptor.
      {
        Descriptor d = Descriptor::DataField(
            isolate(), factory()->regexp_result_names_symbol(),
            JSRegExpResult::kNamesIndex, attribs, Representation::Tagged());
        initial_map->AppendDescriptor(isolate(), &d);
      }

      // regexp_input_index descriptor.
      {
        Descriptor d = Descriptor::DataField(
            isolate(), factory()->regexp_result_regexp_input_symbol(),
            JSRegExpResult::kRegExpInputIndex, attribs,
            Representation::Tagged());
        initial_map->AppendDescriptor(isolate(), &d);
      }

      // regexp_last_index descriptor.
      {
        Descriptor d = Descriptor::DataField(
            isolate(), factory()->regexp_result_regexp_last_index_symbol(),
            JSRegExpResult::kRegExpLastIndex, attribs,
            Representation::Tagged());
        initial_map->AppendDescriptor(isolate(), &d);
      }
    }

    native_context()->set_regexp_result_map(*initial_map);
  }

  // Create a constructor for JSRegExpResultIndices (a variant of Array that
  // predefines the groups property).
  {
    // JSRegExpResultIndices initial map.
    Handle<Map> initial_map = CreateInitialMapForArraySubclass(
        JSRegExpResultIndices::kSize,
        JSRegExpResultIndices::kInObjectPropertyCount);

    // groups descriptor.
    {
      Descriptor d = Descriptor::DataField(
          isolate(), factory()->groups_string(),
          JSRegExpResultIndices::kGroupsIndex, NONE, Representation::Tagged());
      initial_map->AppendDescriptor(isolate(), &d);
      DCHECK_EQ(initial_map->LastAdded().as_int(),
                JSRegExpResultIndices::kGroupsDescriptorIndex);
    }

    native_context()->set_regexp_result_indices_map(*initial_map);
  }

  // Add @@iterator method to the arguments object maps.
  {
    PropertyAttributes attribs = DONT_ENUM;
    Handle<AccessorInfo> arguments_iterator =
        factory()->arguments_iterator_accessor();
    {
      Descriptor d = Descriptor::AccessorConstant(factory()->iterator_symbol(),
                                                  arguments_iterator, attribs);
      Handle<Map> map(native_context()->sloppy_arguments_map(), isolate());
      Map::EnsureDescriptorSlack(isolate(), map, 1);
      map->AppendDescriptor(isolate(), &d);
    }
    {
      Descriptor d = Descriptor::AccessorConstant(factory()->iterator_symbol(),
                                                  arguments_iterator, attribs);
      Handle<Map> map(native_context()->fast_aliased_arguments_map(),
                      isolate());
      Map::EnsureDescriptorSlack(isolate(), map, 1);
      map->AppendDescriptor(isolate(), &d);
    }
    {
      Descriptor d = Descriptor::AccessorConstant(factory()->iterator_symbol(),
                                                  arguments_iterator, attribs);
      Handle<Map> map(native_context()->slow_aliased_arguments_map(),
                      isolate());
      Map::EnsureDescriptorSlack(isolate(), map, 1);
      map->AppendDescriptor(isolate(), &d);
    }
    {
      Descriptor d = Descriptor::AccessorConstant(factory()->iterator_symbol(),
                                                  arguments_iterator, attribs);
      Handle<Map> map(native_context()->strict_arguments_map(), isolate());
      Map::EnsureDescriptorSlack(isolate(), map, 1);
      map->AppendDescriptor(isolate(), &d);
    }
  }
  {
    Handle<OrderedHashSet> promises =
        OrderedHashSet::Allocate(isolate(), 0).ToHandleChecked();
    native_context()->set_atomics_waitasync_promises(*promises);
  }

  return true;
}

bool Genesis::InstallExtrasBindings() {
  HandleScope scope(isolate());

  Handle<JSObject> extras_binding = factory()->NewJSObjectWithNullProto();

  // binding.isTraceCategoryEnabled(category)
  SimpleInstallFunction(isolate(), extras_binding, "isTraceCategoryEnabled",
                        Builtin::kIsTraceCategoryEnabled, 1, true);

  // binding.trace(phase, category, name, id, data)
  SimpleInstallFunction(isolate(), extras_binding, "trace", Builtin::kTrace, 5,
                        true);

  native_context()->set_extras_binding_object(*extras_binding);

  return true;
}

void Genesis::InitializeMapCaches() {
  {
    Handle<NormalizedMapCache> cache = NormalizedMapCache::New(isolate());
    native_context()->set_normalized_map_cache(*cache);
  }

  {
    Handle<WeakFixedArray> cache = factory()->NewWeakFixedArray(
        JSObject::kMapCacheSize, AllocationType::kOld);

    DisallowGarbageCollection no_gc;
    native_context()->set_map_cache(*cache);
    Map initial = native_context()->object_function().initial_map();
    cache->Set(0, HeapObjectReference::Weak(initial), SKIP_WRITE_BARRIER);
    cache->Set(initial.GetInObjectProperties(),
               HeapObjectReference::Weak(initial), SKIP_WRITE_BARRIER);
  }
}

bool Bootstrapper::InstallExtensions(Handle<Context> native_context,
                                     v8::ExtensionConfiguration* extensions) {
  // Don't install extensions into the snapshot.
  if (isolate_->serializer_enabled()) return true;
  BootstrapperActive active(this);
  SaveAndSwitchContext saved_context(isolate_, *native_context);
  return Genesis::InstallExtensions(isolate_, native_context, extensions) &&
         Genesis::InstallSpecialObjects(isolate_, native_context);
}

bool Genesis::InstallSpecialObjects(Isolate* isolate,
                                    Handle<Context> native_context) {
  HandleScope scope(isolate);

  Handle<JSObject> Error = isolate->error_function();
  Handle<String> name = isolate->factory()->stackTraceLimit_string();
  Handle<Smi> stack_trace_limit(Smi::FromInt(FLAG_stack_trace_limit), isolate);
  JSObject::AddProperty(isolate, Error, name, stack_trace_limit, NONE);

#if V8_ENABLE_WEBASSEMBLY
  if (FLAG_expose_wasm) {
    // Install the internal data structures into the isolate and expose on
    // the global object.
    WasmJs::Install(isolate, true);
  } else if (FLAG_validate_asm) {
    // Install the internal data structures only; these are needed for asm.js
    // translated to Wasm to work correctly.
    WasmJs::Install(isolate, false);
  }
#endif  // V8_ENABLE_WEBASSEMBLY

  return true;
}

static uint32_t Hash(RegisteredExtension* extension) {
  return v8::internal::ComputePointerHash(extension);
}

Genesis::ExtensionStates::ExtensionStates() : map_(8) {}

Genesis::ExtensionTraversalState Genesis::ExtensionStates::get_state(
    RegisteredExtension* extension) {
  base::HashMap::Entry* entry = map_.Lookup(extension, Hash(extension));
  if (entry == nullptr) {
    return UNVISITED;
  }
  return static_cast<ExtensionTraversalState>(
      reinterpret_cast<intptr_t>(entry->value));
}

void Genesis::ExtensionStates::set_state(RegisteredExtension* extension,
                                         ExtensionTraversalState state) {
  map_.LookupOrInsert(extension, Hash(extension))->value =
      reinterpret_cast<void*>(static_cast<intptr_t>(state));
}

bool Genesis::InstallExtensions(Isolate* isolate,
                                Handle<Context> native_context,
                                v8::ExtensionConfiguration* extensions) {
  ExtensionStates extension_states;  // All extensions have state UNVISITED.
  return InstallAutoExtensions(isolate, &extension_states) &&
         (!FLAG_expose_gc ||
          InstallExtension(isolate, "v8/gc", &extension_states)) &&
         (!FLAG_expose_externalize_string ||
          InstallExtension(isolate, "v8/externalize", &extension_states)) &&
         (!TracingFlags::is_gc_stats_enabled() ||
          InstallExtension(isolate, "v8/statistics", &extension_states)) &&
         (!FLAG_expose_trigger_failure ||
          InstallExtension(isolate, "v8/trigger-failure", &extension_states)) &&
         (!FLAG_expose_ignition_statistics ||
          InstallExtension(isolate, "v8/ignition-statistics",
                           &extension_states)) &&
         (!isValidCpuTraceMarkFunctionName() ||
          InstallExtension(isolate, "v8/cpumark", &extension_states)) &&
#ifdef ENABLE_VTUNE_TRACEMARK
         (!FLAG_enable_vtune_domain_support ||
          InstallExtension(isolate, "v8/vtunedomain", &extension_states)) &&
#endif  // ENABLE_VTUNE_TRACEMARK
         InstallRequestedExtensions(isolate, extensions, &extension_states);
}

bool Genesis::InstallAutoExtensions(Isolate* isolate,
                                    ExtensionStates* extension_states) {
  for (v8::RegisteredExtension* it = v8::RegisteredExtension::first_extension();
       it != nullptr; it = it->next()) {
    if (it->extension()->auto_enable() &&
        !InstallExtension(isolate, it, extension_states)) {
      return false;
    }
  }
  return true;
}

bool Genesis::InstallRequestedExtensions(Isolate* isolate,
                                         v8::ExtensionConfiguration* extensions,
                                         ExtensionStates* extension_states) {
  for (const char** it = extensions->begin(); it != extensions->end(); ++it) {
    if (!InstallExtension(isolate, *it, extension_states)) return false;
  }
  return true;
}

// Installs a named extension.  This methods is unoptimized and does
// not scale well if we want to support a large number of extensions.
bool Genesis::InstallExtension(Isolate* isolate, const char* name,
                               ExtensionStates* extension_states) {
  for (v8::RegisteredExtension* it = v8::RegisteredExtension::first_extension();
       it != nullptr; it = it->next()) {
    if (strcmp(name, it->extension()->name()) == 0) {
      return InstallExtension(isolate, it, extension_states);
    }
  }
  return Utils::ApiCheck(false, "v8::Context::New()",
                         "Cannot find required extension");
}

bool Genesis::InstallExtension(Isolate* isolate,
                               v8::RegisteredExtension* current,
                               ExtensionStates* extension_states) {
  HandleScope scope(isolate);

  if (extension_states->get_state(current) == INSTALLED) return true;
  // The current node has already been visited so there must be a
  // cycle in the dependency graph; fail.
  if (!Utils::ApiCheck(extension_states->get_state(current) != VISITED,
                       "v8::Context::New()", "Circular extension dependency")) {
    return false;
  }
  DCHECK(extension_states->get_state(current) == UNVISITED);
  extension_states->set_state(current, VISITED);
  v8::Extension* extension = current->extension();
  // Install the extension's dependencies
  for (int i = 0; i < extension->dependency_count(); i++) {
    if (!InstallExtension(isolate, extension->dependencies()[i],
                          extension_states)) {
      return false;
    }
  }
  bool result = CompileExtension(isolate, extension);
  if (!result) {
    // If this failed, it either threw an exception, or the isolate is
    // terminating.
    DCHECK(isolate->has_pending_exception() ||
           (isolate->has_scheduled_exception() &&
            isolate->scheduled_exception() ==
                ReadOnlyRoots(isolate).termination_exception()));
    // We print out the name of the extension that fail to install.
    // When an error is thrown during bootstrapping we automatically print
    // the line number at which this happened to the console in the isolate
    // error throwing functionality.
    base::OS::PrintError("Error installing extension '%s'.\n",
                         current->extension()->name());
    isolate->clear_pending_exception();
  }
  extension_states->set_state(current, INSTALLED);
  return result;
}

bool Genesis::ConfigureGlobalObjects(
    v8::Local<v8::ObjectTemplate> global_proxy_template) {
  Handle<JSObject> global_proxy(native_context()->global_proxy(), isolate());
  Handle<JSObject> global_object(native_context()->global_object(), isolate());

  if (!global_proxy_template.IsEmpty()) {
    // Configure the global proxy object.
    Handle<ObjectTemplateInfo> global_proxy_data =
        v8::Utils::OpenHandle(*global_proxy_template);
    if (!ConfigureApiObject(global_proxy, global_proxy_data)) return false;

    // Configure the global object.
    Handle<FunctionTemplateInfo> proxy_constructor(
        FunctionTemplateInfo::cast(global_proxy_data->constructor()),
        isolate());
    if (!proxy_constructor->GetPrototypeTemplate().IsUndefined(isolate())) {
      Handle<ObjectTemplateInfo> global_object_data(
          ObjectTemplateInfo::cast(proxy_constructor->GetPrototypeTemplate()),
          isolate());
      if (!ConfigureApiObject(global_object, global_object_data)) return false;
    }
  }

  JSObject::ForceSetPrototype(isolate(), global_proxy, global_object);

  native_context()->set_array_buffer_map(
      native_context()->array_buffer_fun().initial_map());

  return true;
}

bool Genesis::ConfigureApiObject(Handle<JSObject> object,
                                 Handle<ObjectTemplateInfo> object_template) {
  DCHECK(!object_template.is_null());
  DCHECK(FunctionTemplateInfo::cast(object_template->constructor())
             .IsTemplateFor(object->map()));

  MaybeHandle<JSObject> maybe_obj =
      ApiNatives::InstantiateObject(object->GetIsolate(), object_template);
  Handle<JSObject> instantiated_template;
  if (!maybe_obj.ToHandle(&instantiated_template)) {
    DCHECK(isolate()->has_pending_exception());
    isolate()->clear_pending_exception();
    return false;
  }
  TransferObject(instantiated_template, object);
  return true;
}

static bool PropertyAlreadyExists(Isolate* isolate, Handle<JSObject> to,
                                  Handle<Name> key) {
  LookupIterator it(isolate, to, key, LookupIterator::OWN_SKIP_INTERCEPTOR);
  CHECK_NE(LookupIterator::ACCESS_CHECK, it.state());
  return it.IsFound();
}

void Genesis::TransferNamedProperties(Handle<JSObject> from,
                                      Handle<JSObject> to) {
  // If JSObject::AddProperty asserts due to already existing property,
  // it is likely due to both global objects sharing property name(s).
  // Merging those two global objects is impossible.
  // The global template must not create properties that already exist
  // in the snapshotted global object.
  if (from->HasFastProperties()) {
    Handle<DescriptorArray> descs = Handle<DescriptorArray>(
        from->map().instance_descriptors(isolate()), isolate());
    for (InternalIndex i : from->map().IterateOwnDescriptors()) {
      PropertyDetails details = descs->GetDetails(i);
      if (details.location() == kField) {
        if (details.kind() == kData) {
          HandleScope inner(isolate());
          Handle<Name> key = Handle<Name>(descs->GetKey(i), isolate());
          // If the property is already there we skip it.
          if (PropertyAlreadyExists(isolate(), to, key)) continue;
          FieldIndex index = FieldIndex::ForDescriptor(from->map(), i);
          Handle<Object> value =
              JSObject::FastPropertyAt(from, details.representation(), index);
          JSObject::AddProperty(isolate(), to, key, value,
                                details.attributes());
        } else {
          DCHECK_EQ(kAccessor, details.kind());
          UNREACHABLE();
        }

      } else {
        DCHECK_EQ(kDescriptor, details.location());
        DCHECK_EQ(kAccessor, details.kind());
        Handle<Name> key(descs->GetKey(i), isolate());
        // If the property is already there we skip it.
        if (PropertyAlreadyExists(isolate(), to, key)) continue;
        HandleScope inner(isolate());
        DCHECK(!to->HasFastProperties());
        // Add to dictionary.
        Handle<Object> value(descs->GetStrongValue(i), isolate());
        PropertyDetails d(kAccessor, details.attributes(),
                          PropertyCellType::kMutable);
        JSObject::SetNormalizedProperty(to, key, value, d);
      }
    }
  } else if (from->IsJSGlobalObject()) {
    // Copy all keys and values in enumeration order.
    Handle<GlobalDictionary> properties(
        JSGlobalObject::cast(*from).global_dictionary(kAcquireLoad), isolate());
    Handle<FixedArray> indices =
        GlobalDictionary::IterationIndices(isolate(), properties);
    for (int i = 0; i < indices->length(); i++) {
      InternalIndex index(Smi::ToInt(indices->get(i)));
      Handle<PropertyCell> cell(properties->CellAt(index), isolate());
      Handle<Name> key(cell->name(), isolate());
      // If the property is already there we skip it.
      if (PropertyAlreadyExists(isolate(), to, key)) continue;
      // Set the property.
      Handle<Object> value(cell->value(), isolate());
      if (value->IsTheHole(isolate())) continue;
      PropertyDetails details = cell->property_details();
      if (details.kind() != kData) continue;
      JSObject::AddProperty(isolate(), to, key, value, details.attributes());
    }

  } else if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
    // Copy all keys and values in enumeration order.
    Handle<SwissNameDictionary> properties = Handle<SwissNameDictionary>(
        from->property_dictionary_swiss(), isolate());
    ReadOnlyRoots roots(isolate());
    for (InternalIndex entry : properties->IterateEntriesOrdered()) {
      Object raw_key;
      if (!properties->ToKey(roots, entry, &raw_key)) continue;

      DCHECK(raw_key.IsName());
      Handle<Name> key(Name::cast(raw_key), isolate());
      // If the property is already there we skip it.
      if (PropertyAlreadyExists(isolate(), to, key)) continue;
      // Set the property.
      Handle<Object> value =
          Handle<Object>(properties->ValueAt(entry), isolate());
      DCHECK(!value->IsCell());
      DCHECK(!value->IsTheHole(isolate()));
      PropertyDetails details = properties->DetailsAt(entry);
      DCHECK_EQ(kData, details.kind());
      JSObject::AddProperty(isolate(), to, key, value, details.attributes());
    }
  } else {
    // Copy all keys and values in enumeration order.
    Handle<NameDictionary> properties =
        Handle<NameDictionary>(from->property_dictionary(), isolate());
    Handle<FixedArray> key_indices =
        NameDictionary::IterationIndices(isolate(), properties);
    ReadOnlyRoots roots(isolate());
    for (int i = 0; i < key_indices->length(); i++) {
      InternalIndex key_index(Smi::ToInt(key_indices->get(i)));
      Object raw_key = properties->KeyAt(key_index);
      DCHECK(properties->IsKey(roots, raw_key));
      DCHECK(raw_key.IsName());
      Handle<Name> key(Name::cast(raw_key), isolate());
      // If the property is already there we skip it.
      if (PropertyAlreadyExists(isolate(), to, key)) continue;
      // Set the property.
      Handle<Object> value =
          Handle<Object>(properties->ValueAt(key_index), isolate());
      DCHECK(!value->IsCell());
      DCHECK(!value->IsTheHole(isolate()));
      PropertyDetails details = properties->DetailsAt(key_index);
      DCHECK_EQ(kData, details.kind());
      JSObject::AddProperty(isolate(), to, key, value, details.attributes());
    }
  }
}

void Genesis::TransferIndexedProperties(Handle<JSObject> from,
                                        Handle<JSObject> to) {
  // Cloning the elements array is sufficient.
  Handle<FixedArray> from_elements =
      Handle<FixedArray>(FixedArray::cast(from->elements()), isolate());
  Handle<FixedArray> to_elements = factory()->CopyFixedArray(from_elements);
  to->set_elements(*to_elements);
}

void Genesis::TransferObject(Handle<JSObject> from, Handle<JSObject> to) {
  HandleScope outer(isolate());

  DCHECK(!from->IsJSArray());
  DCHECK(!to->IsJSArray());

  TransferNamedProperties(from, to);
  TransferIndexedProperties(from, to);

  // Transfer the prototype (new map is needed).
  Handle<HeapObject> proto(from->map().prototype(), isolate());
  JSObject::ForceSetPrototype(isolate(), to, proto);
}

Handle<Map> Genesis::CreateInitialMapForArraySubclass(int size,
                                                      int inobject_properties) {
  // Find global.Array.prototype to inherit from.
  Handle<JSFunction> array_constructor(native_context()->array_function(),
                                       isolate());
  Handle<JSObject> array_prototype(native_context()->initial_array_prototype(),
                                   isolate());

  // Add initial map.
  Handle<Map> initial_map = factory()->NewMap(
      JS_ARRAY_TYPE, size, TERMINAL_FAST_ELEMENTS_KIND, inobject_properties);
  initial_map->SetConstructor(*array_constructor);

  // Set prototype on map.
  initial_map->set_has_non_instance_prototype(false);
  Map::SetPrototype(isolate(), initial_map, array_prototype);

  // Update map with length accessor from Array.
  static constexpr int kTheLengthAccessor = 1;
  Map::EnsureDescriptorSlack(isolate(), initial_map,
                             inobject_properties + kTheLengthAccessor);

  // length descriptor.
  {
    JSFunction array_function = native_context()->array_function();
    Handle<DescriptorArray> array_descriptors(
        array_function.initial_map().instance_descriptors(isolate()),
        isolate());
    Handle<String> length = factory()->length_string();
    InternalIndex old = array_descriptors->SearchWithCache(
        isolate(), *length, array_function.initial_map());
    DCHECK(old.is_found());
    Descriptor d = Descriptor::AccessorConstant(
        length, handle(array_descriptors->GetStrongValue(old), isolate()),
        array_descriptors->GetDetails(old).attributes());
    initial_map->AppendDescriptor(isolate(), &d);
  }
  return initial_map;
}

Genesis::Genesis(
    Isolate* isolate, MaybeHandle<JSGlobalProxy> maybe_global_proxy,
    v8::Local<v8::ObjectTemplate> global_proxy_template,
    size_t context_snapshot_index,
    v8::DeserializeEmbedderFieldsCallback embedder_fields_deserializer,
    v8::MicrotaskQueue* microtask_queue)
    : isolate_(isolate), active_(isolate->bootstrapper()) {
  RCS_SCOPE(isolate, RuntimeCallCounterId::kGenesis);
  result_ = Handle<Context>::null();
  global_proxy_ = Handle<JSGlobalProxy>::null();

  // Before creating the roots we must save the context and restore it
  // on all function exits.
  SaveContext saved_context(isolate);

  // The deserializer needs to hook up references to the global proxy.
  // Create an uninitialized global proxy now if we don't have one
  // and initialize it later in CreateNewGlobals.
  Handle<JSGlobalProxy> global_proxy;
  if (!maybe_global_proxy.ToHandle(&global_proxy)) {
    int instance_size = 0;
    if (context_snapshot_index > 0) {
      // The global proxy function to reinitialize this global proxy is in the
      // context that is yet to be deserialized. We need to prepare a global
      // proxy of the correct size.
      Object size = isolate->heap()->serialized_global_proxy_sizes().get(
          static_cast<int>(context_snapshot_index) - 1);
      instance_size = Smi::ToInt(size);
    } else {
      instance_size = JSGlobalProxy::SizeWithEmbedderFields(
          global_proxy_template.IsEmpty()
              ? 0
              : global_proxy_template->InternalFieldCount());
    }
    global_proxy =
        isolate->factory()->NewUninitializedJSGlobalProxy(instance_size);
  }

  // We can only de-serialize a context if the isolate was initialized from
  // a snapshot. Otherwise we have to build the context from scratch.
  // Also create a context from scratch to expose natives, if required by flag.
  DCHECK(native_context_.is_null());
  if (isolate->initialized_from_snapshot()) {
    Handle<Context> context;
    if (Snapshot::NewContextFromSnapshot(isolate, global_proxy,
                                         context_snapshot_index,
                                         embedder_fields_deserializer)
            .ToHandle(&context)) {
      native_context_ = Handle<NativeContext>::cast(context);
    }
  }

  if (!native_context().is_null()) {
    AddToWeakNativeContextList(isolate, *native_context());
    isolate->set_context(*native_context());
    isolate->counters()->contexts_created_by_snapshot()->Increment();

    if (context_snapshot_index == 0) {
      Handle<JSGlobalObject> global_object =
          CreateNewGlobals(global_proxy_template, global_proxy);
      HookUpGlobalObject(global_object);

      if (!ConfigureGlobalObjects(global_proxy_template)) return;
    } else {
      // The global proxy needs to be integrated into the native context.
      HookUpGlobalProxy(global_proxy);
    }
    DCHECK(!global_proxy->IsDetachedFrom(native_context()->global_object()));
  } else {
    DCHECK(native_context().is_null());

    base::ElapsedTimer timer;
    if (FLAG_profile_deserialization) timer.Start();
    DCHECK_EQ(0u, context_snapshot_index);
    // We get here if there was no context snapshot.
    CreateRoots();
    MathRandom::InitializeContext(isolate, native_context());
    Handle<JSFunction> empty_function = CreateEmptyFunction();
    CreateSloppyModeFunctionMaps(empty_function);
    CreateStrictModeFunctionMaps(empty_function);
    CreateObjectFunction(empty_function);
    CreateIteratorMaps(empty_function);
    CreateAsyncIteratorMaps(empty_function);
    CreateAsyncFunctionMaps(empty_function);
    Handle<JSGlobalObject> global_object =
        CreateNewGlobals(global_proxy_template, global_proxy);
    InitializeMapCaches();
    InitializeGlobal(global_object, empty_function);
    InitializeIteratorFunctions();
    InitializeCallSiteBuiltins();

    if (!InstallABunchOfRandomThings()) return;
    if (!InstallExtrasBindings()) return;
    if (!ConfigureGlobalObjects(global_proxy_template)) return;

    isolate->counters()->contexts_created_from_scratch()->Increment();

    if (FLAG_profile_deserialization) {
      double ms = timer.Elapsed().InMillisecondsF();
      PrintF("[Initializing context from scratch took %0.3f ms]\n", ms);
    }
  }

  // TODO(v8:10391): The reason is that the NativeContext::microtask_queue
  // serialization is not actually supported, and therefore the field is
  // serialized as raw data instead of being serialized as ExternalReference.
  // As a result, when V8 heap sandbox is enabled, the external pointer entry
  // is not allocated for microtask queue field during deserialization, so we
  // allocate it manually here.
  native_context()->AllocateExternalPointerEntries(isolate);

  native_context()->set_microtask_queue(
      isolate, microtask_queue ? static_cast<MicrotaskQueue*>(microtask_queue)
                               : isolate->default_microtask_queue());

  // Install experimental natives. Do not include them into the
  // snapshot as we should be able to turn them off at runtime. Re-installing
  // them after they have already been deserialized would also fail.
  if (!isolate->serializer_enabled()) {
    InitializeExperimentalGlobal();

    // Store String.prototype's map again in case it has been changed by
    // experimental natives.
    Handle<JSFunction> string_function(native_context()->string_function(),
                                       isolate);
    JSObject string_function_prototype =
        JSObject::cast(string_function->initial_map().prototype());
    DCHECK(string_function_prototype.HasFastProperties());
    native_context()->set_string_function_prototype_map(
        string_function_prototype.map());
  }

  if (FLAG_disallow_code_generation_from_strings) {
    native_context()->set_allow_code_gen_from_strings(
        ReadOnlyRoots(isolate).false_value());
  }

  // We created new functions, which may require debug instrumentation.
  if (isolate->debug()->is_active()) {
    isolate->debug()->InstallDebugBreakTrampoline();
  }

  native_context()->ResetErrorsThrown();
  result_ = native_context();
}

Genesis::Genesis(Isolate* isolate,
                 MaybeHandle<JSGlobalProxy> maybe_global_proxy,
                 v8::Local<v8::ObjectTemplate> global_proxy_template)
    : isolate_(isolate), active_(isolate->bootstrapper()) {
  result_ = Handle<Context>::null();
  global_proxy_ = Handle<JSGlobalProxy>::null();

  // Before creating the roots we must save the context and restore it
  // on all function exits.
  SaveContext saved_context(isolate);

  const int proxy_size = JSGlobalProxy::SizeWithEmbedderFields(
      global_proxy_template->InternalFieldCount());

  Handle<JSGlobalProxy> global_proxy;
  if (!maybe_global_proxy.ToHandle(&global_proxy)) {
    global_proxy = factory()->NewUninitializedJSGlobalProxy(proxy_size);
  }

  // Create a remote object as the global object.
  Handle<ObjectTemplateInfo> global_proxy_data =
      Utils::OpenHandle(*global_proxy_template);
  Handle<FunctionTemplateInfo> global_constructor(
      FunctionTemplateInfo::cast(global_proxy_data->constructor()), isolate);

  Handle<ObjectTemplateInfo> global_object_template(
      ObjectTemplateInfo::cast(global_constructor->GetPrototypeTemplate()),
      isolate);
  Handle<JSObject> global_object =
      ApiNatives::InstantiateRemoteObject(global_object_template)
          .ToHandleChecked();

  // (Re)initialize the global proxy object.
  DCHECK_EQ(global_proxy_data->embedder_field_count(),
            global_proxy_template->InternalFieldCount());
  Handle<Map> global_proxy_map = isolate->factory()->NewMap(
      JS_GLOBAL_PROXY_TYPE, proxy_size, TERMINAL_FAST_ELEMENTS_KIND);
  global_proxy_map->set_is_access_check_needed(true);
  global_proxy_map->set_may_have_interesting_symbols(true);

  // A remote global proxy has no native context.
  global_proxy->set_native_context(ReadOnlyRoots(heap()).null_value());

  // Configure the hidden prototype chain of the global proxy.
  JSObject::ForceSetPrototype(isolate, global_proxy, global_object);
  global_proxy->map().SetConstructor(*global_constructor);

  global_proxy_ = global_proxy;
}

// Support for thread preemption.

// Reserve space for statics needing saving and restoring.
int Bootstrapper::ArchiveSpacePerThread() { return sizeof(NestingCounterType); }

// Archive statics that are thread-local.
char* Bootstrapper::ArchiveState(char* to) {
  *reinterpret_cast<NestingCounterType*>(to) = nesting_;
  nesting_ = 0;
  return to + sizeof(NestingCounterType);
}

// Restore statics that are thread-local.
char* Bootstrapper::RestoreState(char* from) {
  nesting_ = *reinterpret_cast<NestingCounterType*>(from);
  return from + sizeof(NestingCounterType);
}

// Called when the top-level V8 mutex is destroyed.
void Bootstrapper::FreeThreadResources() { DCHECK(!IsActive()); }

}  // namespace internal
}  // namespace v8
