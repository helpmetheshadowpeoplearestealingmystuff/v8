// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_FACTORY_H_
#define V8_HEAP_FACTORY_H_

// Clients of this interface shouldn't depend on lots of heap internals.
// Do not include anything from src/heap here!
#include "src/base/strings.h"
#include "src/base/vector.h"
#include "src/baseline/baseline.h"
#include "src/builtins/builtins.h"
#include "src/common/globals.h"
#include "src/execution/messages.h"
#include "src/handles/handles.h"
#include "src/handles/maybe-handles.h"
#include "src/heap/factory-base.h"
#include "src/heap/heap.h"
#include "src/objects/code.h"
#include "src/objects/dictionary.h"
#include "src/objects/js-array.h"
#include "src/objects/js-regexp.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/string.h"

namespace unibrow {
enum class Utf8Variant : uint8_t;
}

namespace v8 {
namespace internal {

// Forward declarations.
class AliasedArgumentsEntry;
class ObjectBoilerplateDescription;
class BasicBlockProfilerData;
class BreakPoint;
class BreakPointInfo;
class CallableTask;
class CallbackTask;
class CallHandlerInfo;
class CallSiteInfo;
class Expression;
class EmbedderDataArray;
class ArrayBoilerplateDescription;
class CoverageInfo;
class DebugInfo;
class EnumCache;
class FreshlyAllocatedBigInt;
class Isolate;
class JSArrayBufferView;
class JSDataView;
class JSGeneratorObject;
class JSMap;
class JSMapIterator;
class JSModuleNamespace;
class JSPromise;
class JSProxy;
class JSSet;
class JSSetIterator;
class JSTypedArray;
class JSWeakMap;
class LoadHandler;
class NativeContext;
class PromiseResolveThenableJobTask;
class RegExpMatchInfo;
class ScriptContextTable;
class SourceTextModule;
class StackFrameInfo;
class StringSet;
class StoreHandler;
class SyntheticModule;
class TemplateObjectDescription;
class WasmCapiFunctionData;
class WasmExportedFunctionData;
class WasmJSFunctionData;
class WeakCell;
#if V8_ENABLE_WEBASSEMBLY
namespace wasm {
class ArrayType;
class StructType;
struct WasmElemSegment;
class WasmValue;
enum class OnResume : int;
enum Suspend : bool;
enum Promise : bool;
}  // namespace wasm
#endif

enum class SharedFlag : uint8_t;
enum class InitializedFlag : uint8_t;

enum FunctionMode {
  kWithNameBit = 1 << 0,
  kWithWritablePrototypeBit = 1 << 1,
  kWithReadonlyPrototypeBit = 1 << 2,
  kWithPrototypeBits = kWithWritablePrototypeBit | kWithReadonlyPrototypeBit,

  // Without prototype.
  FUNCTION_WITHOUT_PROTOTYPE = 0,
  METHOD_WITH_NAME = kWithNameBit,

  // With writable prototype.
  FUNCTION_WITH_WRITEABLE_PROTOTYPE = kWithWritablePrototypeBit,
  FUNCTION_WITH_NAME_AND_WRITEABLE_PROTOTYPE =
      kWithWritablePrototypeBit | kWithNameBit,

  // With readonly prototype.
  FUNCTION_WITH_READONLY_PROTOTYPE = kWithReadonlyPrototypeBit,
  FUNCTION_WITH_NAME_AND_READONLY_PROTOTYPE =
      kWithReadonlyPrototypeBit | kWithNameBit,
};

// Interface for handle based allocation.
class V8_EXPORT_PRIVATE Factory : public FactoryBase<Factory> {
 public:
  inline ReadOnlyRoots read_only_roots() const;

  Handle<Oddball> NewOddball(Handle<Map> map, const char* to_string,
                             Handle<Object> to_number, const char* type_of,
                             byte kind);

  // Marks self references within code generation.
  Handle<Oddball> NewSelfReferenceMarker();

  // Marks references to a function's basic-block usage counters array during
  // code generation.
  Handle<Oddball> NewBasicBlockCountersMarker();

  // Allocates a property array initialized with undefined values.
  Handle<PropertyArray> NewPropertyArray(
      int length, AllocationType allocation = AllocationType::kYoung);
  // Tries allocating a fixed array initialized with undefined values.
  // In case of an allocation failure (OOM) an empty handle is returned.
  // The caller has to manually signal an
  // v8::internal::Heap::FatalProcessOutOfMemory typically by calling
  // NewFixedArray as a fallback.
  V8_WARN_UNUSED_RESULT
  MaybeHandle<FixedArray> TryNewFixedArray(
      int length, AllocationType allocation = AllocationType::kYoung);

  // Allocates a closure feedback cell array whose feedback cells are
  // initialized with undefined values.
  Handle<ClosureFeedbackCellArray> NewClosureFeedbackCellArray(int num_slots);

  // Allocates a feedback vector whose slots are initialized with undefined
  // values.
  Handle<FeedbackVector> NewFeedbackVector(
      Handle<SharedFunctionInfo> shared,
      Handle<ClosureFeedbackCellArray> closure_feedback_cell_array,
      Handle<FeedbackCell> parent_feedback_cell);

  // Allocates a clean embedder data array with given capacity.
  Handle<EmbedderDataArray> NewEmbedderDataArray(int length);

  // Allocate a new fixed double array with hole values.
  Handle<FixedArrayBase> NewFixedDoubleArrayWithHoles(int size);

  // Allocates a NameDictionary with an internal capacity calculated such that
  // |at_least_space_for| entries can be added without reallocating.
  Handle<NameDictionary> NewNameDictionary(int at_least_space_for);

  // Allocates an OrderedNameDictionary of the given capacity. This guarantees
  // that |capacity| entries can be added without reallocating.
  Handle<OrderedNameDictionary> NewOrderedNameDictionary(
      int capacity = OrderedNameDictionary::kInitialCapacity);

  Handle<OrderedHashSet> NewOrderedHashSet();
  Handle<OrderedHashMap> NewOrderedHashMap();
  Handle<SmallOrderedHashSet> NewSmallOrderedHashSet(
      int capacity = kSmallOrderedHashSetMinCapacity,
      AllocationType allocation = AllocationType::kYoung);
  Handle<SmallOrderedHashMap> NewSmallOrderedHashMap(
      int capacity = kSmallOrderedHashMapMinCapacity,
      AllocationType allocation = AllocationType::kYoung);
  Handle<SmallOrderedNameDictionary> NewSmallOrderedNameDictionary(
      int capacity = kSmallOrderedHashMapMinCapacity,
      AllocationType allocation = AllocationType::kYoung);

  Handle<SwissNameDictionary> CreateCanonicalEmptySwissNameDictionary();

  // Create a new PrototypeInfo struct.
  Handle<PrototypeInfo> NewPrototypeInfo();

  // Create a new EnumCache struct.
  Handle<EnumCache> NewEnumCache(
      Handle<FixedArray> keys, Handle<FixedArray> indices,
      AllocationType allocation = AllocationType::kOld);

  // Create a new Tuple2 struct.
  Handle<Tuple2> NewTuple2(Handle<Object> value1, Handle<Object> value2,
                           AllocationType allocation);

  // Create a new PropertyDescriptorObject struct.
  Handle<PropertyDescriptorObject> NewPropertyDescriptorObject();

  // Finds the internalized copy for string in the string table.
  // If not found, a new string is added to the table and returned.
  Handle<String> InternalizeUtf8String(const base::Vector<const char>& str);
  Handle<String> InternalizeUtf8String(const char* str) {
    return InternalizeUtf8String(base::CStrVector(str));
  }

  // Import InternalizeString overloads from base class.
  using FactoryBase::InternalizeString;

  Handle<String> InternalizeString(base::Vector<const char> str,
                                   bool convert_encoding = false) {
    return InternalizeString(base::Vector<const uint8_t>::cast(str));
  }

  template <typename SeqString>
  Handle<String> InternalizeString(Handle<SeqString>, int from, int length,
                                   bool convert_encoding = false);

  // Internalized strings are created in the old generation (data space).
  inline Handle<String> InternalizeString(Handle<String> string);

  inline Handle<Name> InternalizeName(Handle<Name> name);

  // String creation functions.  Most of the string creation functions take
  // an AllocationType argument to optionally request that they be
  // allocated in the old generation. Otherwise the default is
  // AllocationType::kYoung.
  //
  // Creates a new String object.  There are two String encodings: one-byte and
  // two-byte.  One should choose between the three string factory functions
  // based on the encoding of the string buffer that the string is
  // initialized from.
  //   - ...FromOneByte (defined in FactoryBase) initializes the string from a
  //     buffer that is Latin1 encoded (it does not check that the buffer is
  //     Latin1 encoded) and the result will be Latin1 encoded.
  //   - ...FromUtf8 initializes the string from a buffer that is UTF-8
  //     encoded.  If the characters are all ASCII characters, the result
  //     will be Latin1 encoded, otherwise it will converted to two-byte.
  //   - ...FromTwoByte initializes the string from a buffer that is two-byte
  //     encoded.  If the characters are all Latin1 characters, the result
  //     will be converted to Latin1, otherwise it will be left as two-byte.
  //
  // One-byte strings are pretenured when used as keys in the SourceCodeCache.
  template <size_t N>
  inline Handle<String> NewStringFromStaticChars(
      const char (&str)[N],
      AllocationType allocation = AllocationType::kYoung) {
    DCHECK_EQ(N, strlen(str) + 1);
    return NewStringFromOneByte(base::StaticOneByteVector(str), allocation)
        .ToHandleChecked();
  }

  inline Handle<String> NewStringFromAsciiChecked(
      const char* str, AllocationType allocation = AllocationType::kYoung) {
    return NewStringFromOneByte(base::OneByteVector(str), allocation)
        .ToHandleChecked();
  }

  // UTF8 strings are pretenured when used for regexp literal patterns and
  // flags in the parser.
  V8_WARN_UNUSED_RESULT MaybeHandle<String> NewStringFromUtf8(
      const base::Vector<const char>& str,
      AllocationType allocation = AllocationType::kYoung);
  V8_WARN_UNUSED_RESULT MaybeHandle<String> NewStringFromUtf8(
      const base::Vector<const uint8_t>& str, unibrow::Utf8Variant utf8_variant,
      AllocationType allocation = AllocationType::kYoung);

#if V8_ENABLE_WEBASSEMBLY
  V8_WARN_UNUSED_RESULT MaybeHandle<String> NewStringFromUtf8(
      Handle<WasmArray> array, uint32_t begin, uint32_t end,
      unibrow::Utf8Variant utf8_variant,
      AllocationType allocation = AllocationType::kYoung);

  V8_WARN_UNUSED_RESULT MaybeHandle<String> NewStringFromUtf8(
      Handle<ByteArray> array, uint32_t start, uint32_t end,
      unibrow::Utf8Variant utf8_variant,
      AllocationType allocation = AllocationType::kYoung);

  V8_WARN_UNUSED_RESULT MaybeHandle<String> NewStringFromUtf16(
      Handle<WasmArray> array, uint32_t start, uint32_t end,
      AllocationType allocation = AllocationType::kYoung);
#endif  // V8_ENABLE_WEBASSEMBLY

  V8_WARN_UNUSED_RESULT MaybeHandle<String> NewStringFromUtf8SubString(
      Handle<SeqOneByteString> str, int begin, int end,
      AllocationType allocation = AllocationType::kYoung);

  V8_WARN_UNUSED_RESULT MaybeHandle<String> NewStringFromTwoByte(
      const base::Vector<const base::uc16>& str,
      AllocationType allocation = AllocationType::kYoung);

  V8_WARN_UNUSED_RESULT MaybeHandle<String> NewStringFromTwoByte(
      const ZoneVector<base::uc16>* str,
      AllocationType allocation = AllocationType::kYoung);

#if V8_ENABLE_WEBASSEMBLY
  // Usually the two-byte encodings are in the native endianness, but for
  // WebAssembly linear memory, they are explicitly little-endian.
  V8_WARN_UNUSED_RESULT MaybeHandle<String> NewStringFromTwoByteLittleEndian(
      const base::Vector<const base::uc16>& str,
      AllocationType allocation = AllocationType::kYoung);
#endif  // V8_ENABLE_WEBASSEMBLY

  Handle<JSStringIterator> NewJSStringIterator(Handle<String> string);

  Handle<String> NewInternalizedStringImpl(Handle<String> string, int len,
                                           uint32_t hash_field);

  // Compute the internalization strategy for the input string.
  //
  // Old-generation sequential strings can be internalized by mutating their map
  // and return kInPlace, along with the matching internalized string map for
  // string stored in internalized_map.
  //
  // Internalized strings return kAlreadyTransitioned.
  //
  // All other strings are internalized by flattening and copying and return
  // kCopy.
  V8_WARN_UNUSED_RESULT StringTransitionStrategy
  ComputeInternalizationStrategyForString(Handle<String> string,
                                          MaybeHandle<Map>* internalized_map);

  // Creates an internalized copy of an external string. |string| must be
  // of type StringClass.
  template <class StringClass>
  Handle<StringClass> InternalizeExternalString(Handle<String> string);

  // Compute the sharing strategy for the input string.
  //
  // Old-generation sequential and thin strings can be shared by mutating their
  // map and return kInPlace, along with the matching shared string map for the
  // string stored in shared_map.
  //
  // Already-shared strings return kAlreadyTransitioned.
  //
  // All other strings are shared by flattening and copying into a sequential
  // string then sharing that sequential string, and return kCopy.
  V8_WARN_UNUSED_RESULT StringTransitionStrategy
  ComputeSharingStrategyForString(Handle<String> string,
                                  MaybeHandle<Map>* shared_map);

  // Create or lookup a single character string made up of a utf16 surrogate
  // pair.
  Handle<String> NewSurrogatePairString(uint16_t lead, uint16_t trail);

  // Create a new string object which holds a proper substring of a string.
  Handle<String> NewProperSubString(Handle<String> str, int begin, int end);

  // Create a new string object which holds a substring of a string.
  inline Handle<String> NewSubString(Handle<String> str, int begin, int end);

  // Creates a new external String object.  There are two String encodings
  // in the system: one-byte and two-byte.  Unlike other String types, it does
  // not make sense to have a UTF-8 factory function for external strings,
  // because we cannot change the underlying buffer.  Note that these strings
  // are backed by a string resource that resides outside the V8 heap.
  V8_WARN_UNUSED_RESULT MaybeHandle<String> NewExternalStringFromOneByte(
      const ExternalOneByteString::Resource* resource);
  V8_WARN_UNUSED_RESULT MaybeHandle<String> NewExternalStringFromTwoByte(
      const ExternalTwoByteString::Resource* resource);

  // Create a symbol in old or read-only space.
  Handle<Symbol> NewSymbol(AllocationType allocation = AllocationType::kOld);
  Handle<Symbol> NewPrivateSymbol(
      AllocationType allocation = AllocationType::kOld);
  Handle<Symbol> NewPrivateNameSymbol(Handle<String> name);

  // Create a global (but otherwise uninitialized) context.
  Handle<NativeContext> NewNativeContext();

  // Create a script context.
  Handle<Context> NewScriptContext(Handle<NativeContext> outer,
                                   Handle<ScopeInfo> scope_info);

  // Create an empty script context table.
  Handle<ScriptContextTable> NewScriptContextTable();

  // Create a module context.
  Handle<Context> NewModuleContext(Handle<SourceTextModule> module,
                                   Handle<NativeContext> outer,
                                   Handle<ScopeInfo> scope_info);

  // Create a function or eval context.
  Handle<Context> NewFunctionContext(Handle<Context> outer,
                                     Handle<ScopeInfo> scope_info);

  // Create a catch context.
  Handle<Context> NewCatchContext(Handle<Context> previous,
                                  Handle<ScopeInfo> scope_info,
                                  Handle<Object> thrown_object);

  // Create a 'with' context.
  Handle<Context> NewWithContext(Handle<Context> previous,
                                 Handle<ScopeInfo> scope_info,
                                 Handle<JSReceiver> extension);

  Handle<Context> NewDebugEvaluateContext(Handle<Context> previous,
                                          Handle<ScopeInfo> scope_info,
                                          Handle<JSReceiver> extension,
                                          Handle<Context> wrapped);

  // Create a block context.
  Handle<Context> NewBlockContext(Handle<Context> previous,
                                  Handle<ScopeInfo> scope_info);

  // Create a context that's used by builtin functions.
  //
  // These are similar to function context but don't have a previous
  // context or any scope info. These are used to store spec defined
  // context values.
  Handle<Context> NewBuiltinContext(Handle<NativeContext> native_context,
                                    int length);

  Handle<AliasedArgumentsEntry> NewAliasedArgumentsEntry(
      int aliased_context_slot);

  Handle<AccessorInfo> NewAccessorInfo();

  Handle<ErrorStackData> NewErrorStackData(
      Handle<Object> call_site_infos_or_formatted_stack,
      Handle<Object> limit_or_stack_frame_infos);

  Handle<Script> CloneScript(Handle<Script> script);

  Handle<BreakPointInfo> NewBreakPointInfo(int source_position);
  Handle<BreakPoint> NewBreakPoint(int id, Handle<String> condition);

  Handle<CallSiteInfo> NewCallSiteInfo(Handle<Object> receiver_or_instance,
                                       Handle<Object> function,
                                       Handle<HeapObject> code_object,
                                       int code_offset_or_source_position,
                                       int flags,
                                       Handle<FixedArray> parameters);
  Handle<StackFrameInfo> NewStackFrameInfo(
      Handle<HeapObject> shared_or_script,
      int bytecode_offset_or_source_position, Handle<String> function_name,
      bool is_constructor);

  Handle<PromiseOnStack> NewPromiseOnStack(Handle<Object> prev,
                                           Handle<JSObject> promise);

  // Allocate various microtasks.
  Handle<CallableTask> NewCallableTask(Handle<JSReceiver> callable,
                                       Handle<Context> context);
  Handle<CallbackTask> NewCallbackTask(Handle<Foreign> callback,
                                       Handle<Foreign> data);
  Handle<PromiseResolveThenableJobTask> NewPromiseResolveThenableJobTask(
      Handle<JSPromise> promise_to_resolve, Handle<JSReceiver> thenable,
      Handle<JSReceiver> then, Handle<Context> context);

  // Foreign objects are pretenured when allocated by the bootstrapper.
  Handle<Foreign> NewForeign(
      Address addr, AllocationType allocation_type = AllocationType::kYoung);

  Handle<Cell> NewCell(Smi value);
  Handle<Cell> NewCell();

  Handle<PropertyCell> NewPropertyCell(
      Handle<Name> name, PropertyDetails details, Handle<Object> value,
      AllocationType allocation = AllocationType::kOld);
  Handle<PropertyCell> NewProtector();

  Handle<FeedbackCell> NewNoClosuresCell(Handle<HeapObject> value);
  Handle<FeedbackCell> NewOneClosureCell(Handle<HeapObject> value);
  Handle<FeedbackCell> NewManyClosuresCell(Handle<HeapObject> value);

  Handle<TransitionArray> NewTransitionArray(int number_of_transitions,
                                             int slack = 0);

  // Allocate a tenured AllocationSite. Its payload is null.
  Handle<AllocationSite> NewAllocationSite(bool with_weak_next);

  // Allocates and initializes a new Map.
  Handle<Map> NewMap(InstanceType type, int instance_size,
                     ElementsKind elements_kind = TERMINAL_FAST_ELEMENTS_KIND,
                     int inobject_properties = 0,
                     AllocationType allocation_type = AllocationType::kMap);
  // Initializes the fields of a newly created Map using roots from the
  // passed-in Heap. Exposed for tests and heap setup; other code should just
  // call NewMap which takes care of it.
  Map InitializeMap(Map map, InstanceType type, int instance_size,
                    ElementsKind elements_kind, int inobject_properties,
                    Heap* roots);

  // Allocate a block of memory of the given AllocationType (filled with a
  // filler). Used as a fall-back for generated code when the space is full.
  Handle<HeapObject> NewFillerObject(
      int size, AllocationAlignment alignment, AllocationType allocation,
      AllocationOrigin origin = AllocationOrigin::kRuntime);

  Handle<JSObject> NewFunctionPrototype(Handle<JSFunction> function);

  // Returns a deep copy of the JavaScript object.
  // Properties and elements are copied too.
  Handle<JSObject> CopyJSObject(Handle<JSObject> object);
  // Same as above, but also takes an AllocationSite to be appended in an
  // AllocationMemento.
  Handle<JSObject> CopyJSObjectWithAllocationSite(Handle<JSObject> object,
                                                  Handle<AllocationSite> site);

  Handle<FixedArray> CopyFixedArrayWithMap(Handle<FixedArray> array,
                                           Handle<Map> map);

  Handle<FixedArray> CopyFixedArrayAndGrow(
      Handle<FixedArray> array, int grow_by,
      AllocationType allocation = AllocationType::kYoung);

  Handle<WeakArrayList> NewWeakArrayList(
      int capacity, AllocationType allocation = AllocationType::kYoung);

  Handle<WeakFixedArray> CopyWeakFixedArrayAndGrow(Handle<WeakFixedArray> array,
                                                   int grow_by);

  Handle<WeakArrayList> CopyWeakArrayListAndGrow(
      Handle<WeakArrayList> array, int grow_by,
      AllocationType allocation = AllocationType::kYoung);

  Handle<WeakArrayList> CompactWeakArrayList(
      Handle<WeakArrayList> array, int new_capacity,
      AllocationType allocation = AllocationType::kYoung);

  Handle<PropertyArray> CopyPropertyArrayAndGrow(Handle<PropertyArray> array,
                                                 int grow_by);

  Handle<FixedArray> CopyFixedArrayUpTo(
      Handle<FixedArray> array, int new_len,
      AllocationType allocation = AllocationType::kYoung);

  Handle<FixedArray> CopyFixedArray(Handle<FixedArray> array);

  Handle<FixedDoubleArray> CopyFixedDoubleArray(Handle<FixedDoubleArray> array);

  // Creates a new HeapNumber in read-only space if possible otherwise old
  // space.
  Handle<HeapNumber> NewHeapNumberForCodeAssembler(double value);

  Handle<JSObject> NewArgumentsObject(Handle<JSFunction> callee, int length);

  // Allocates and initializes a new JavaScript object based on a
  // constructor.
  // JS objects are pretenured when allocated by the bootstrapper and
  // runtime.
  Handle<JSObject> NewJSObject(
      Handle<JSFunction> constructor,
      AllocationType allocation = AllocationType::kYoung);
  // JSObject without a prototype.
  Handle<JSObject> NewJSObjectWithNullProto();
  // JSObject without a prototype, in dictionary mode.
  Handle<JSObject> NewSlowJSObjectWithNullProto();

  // Global objects are pretenured and initialized based on a constructor.
  Handle<JSGlobalObject> NewJSGlobalObject(Handle<JSFunction> constructor);

  // Allocates and initializes a new JavaScript object based on a map.
  // Passing an allocation site means that a memento will be created that
  // points to the site.
  // JS objects are pretenured when allocated by the bootstrapper and
  // runtime.
  Handle<JSObject> NewJSObjectFromMap(
      Handle<Map> map, AllocationType allocation = AllocationType::kYoung,
      Handle<AllocationSite> allocation_site = Handle<AllocationSite>::null());
  // Like NewJSObjectFromMap, but includes allocating a properties dictionary.
  Handle<JSObject> NewSlowJSObjectFromMap(
      Handle<Map> map,
      int number_of_slow_properties = NameDictionary::kInitialCapacity,
      AllocationType allocation = AllocationType::kYoung,
      Handle<AllocationSite> allocation_site = Handle<AllocationSite>::null());
  // Calls NewJSObjectFromMap or NewSlowJSObjectFromMap depending on whether the
  // map is a dictionary map.
  inline Handle<JSObject> NewFastOrSlowJSObjectFromMap(
      Handle<Map> map,
      int number_of_slow_properties = NameDictionary::kInitialCapacity,
      AllocationType allocation = AllocationType::kYoung,
      Handle<AllocationSite> allocation_site = Handle<AllocationSite>::null());
  // Allocates and initializes a new JavaScript object with the given
  // {prototype} and {properties}. The newly created object will be
  // in dictionary properties mode. The {elements} can either be the
  // empty fixed array, in which case the resulting object will have
  // fast elements, or a NumberDictionary, in which case the resulting
  // object will have dictionary elements.
  Handle<JSObject> NewSlowJSObjectWithPropertiesAndElements(
      Handle<HeapObject> prototype, Handle<HeapObject> properties,
      Handle<FixedArrayBase> elements);

  // JS arrays are pretenured when allocated by the parser.

  // Create a JSArray with a specified length and elements initialized
  // according to the specified mode.
  Handle<JSArray> NewJSArray(
      ElementsKind elements_kind, int length, int capacity,
      ArrayStorageAllocationMode mode = DONT_INITIALIZE_ARRAY_ELEMENTS,
      AllocationType allocation = AllocationType::kYoung);

  Handle<JSArray> NewJSArray(
      int capacity, ElementsKind elements_kind = TERMINAL_FAST_ELEMENTS_KIND,
      AllocationType allocation = AllocationType::kYoung) {
    if (capacity != 0) {
      elements_kind = GetHoleyElementsKind(elements_kind);
    }
    return NewJSArray(elements_kind, 0, capacity,
                      INITIALIZE_ARRAY_ELEMENTS_WITH_HOLE, allocation);
  }

  // Create a JSArray with the given elements.
  Handle<JSArray> NewJSArrayWithElements(
      Handle<FixedArrayBase> elements, ElementsKind elements_kind, int length,
      AllocationType allocation = AllocationType::kYoung);

  inline Handle<JSArray> NewJSArrayWithElements(
      Handle<FixedArrayBase> elements,
      ElementsKind elements_kind = TERMINAL_FAST_ELEMENTS_KIND,
      AllocationType allocation = AllocationType::kYoung);

  Handle<JSArray> NewJSArrayForTemplateLiteralArray(
      Handle<FixedArray> cooked_strings, Handle<FixedArray> raw_strings,
      int function_literal_id, int slot_id);

  void NewJSArrayStorage(
      Handle<JSArray> array, int length, int capacity,
      ArrayStorageAllocationMode mode = DONT_INITIALIZE_ARRAY_ELEMENTS);

  Handle<JSWeakMap> NewJSWeakMap();

  Handle<JSGeneratorObject> NewJSGeneratorObject(Handle<JSFunction> function);

  Handle<JSModuleNamespace> NewJSModuleNamespace();

  Handle<JSWrappedFunction> NewJSWrappedFunction(
      Handle<NativeContext> creation_context, Handle<Object> target);

#if V8_ENABLE_WEBASSEMBLY
  Handle<WasmTypeInfo> NewWasmTypeInfo(Address type_address,
                                       Handle<Map> opt_parent,
                                       int instance_size_bytes,
                                       Handle<WasmInstanceObject> instance,
                                       uint32_t type_index);
  Handle<WasmInternalFunction> NewWasmInternalFunction(Address opt_call_target,
                                                       Handle<HeapObject> ref,
                                                       Handle<Map> rtt);
  Handle<WasmCapiFunctionData> NewWasmCapiFunctionData(
      Address call_target, Handle<Foreign> embedder_data,
      Handle<Code> wrapper_code, Handle<Map> rtt,
      Handle<PodArray<wasm::ValueType>> serialized_sig);
  Handle<WasmExportedFunctionData> NewWasmExportedFunctionData(
      Handle<Code> export_wrapper, Handle<WasmInstanceObject> instance,
      Address call_target, Handle<Object> ref, int func_index,
      const wasm::FunctionSig* sig, uint32_t canonical_type_index,
      int wrapper_budget, Handle<Map> rtt, wasm::Promise promise);
  Handle<WasmApiFunctionRef> NewWasmApiFunctionRef(
      Handle<JSReceiver> callable, wasm::Suspend suspend,
      Handle<WasmInstanceObject> instance);
  // {opt_call_target} is kNullAddress for JavaScript functions, and
  // non-null for exported Wasm functions.
  Handle<WasmJSFunctionData> NewWasmJSFunctionData(
      Address opt_call_target, Handle<JSReceiver> callable, int return_count,
      int parameter_count, Handle<PodArray<wasm::ValueType>> serialized_sig,
      Handle<Code> wrapper_code, Handle<Map> rtt, wasm::Suspend suspend,
      wasm::Promise promise);
  Handle<WasmResumeData> NewWasmResumeData(
      Handle<WasmSuspenderObject> suspender, wasm::OnResume on_resume);
  Handle<WasmStruct> NewWasmStruct(const wasm::StructType* type,
                                   wasm::WasmValue* args, Handle<Map> map);
  Handle<WasmArray> NewWasmArray(const wasm::ArrayType* type, uint32_t length,
                                 wasm::WasmValue initial_value,
                                 Handle<Map> map);
  Handle<WasmArray> NewWasmArrayFromElements(
      const wasm::ArrayType* type, const std::vector<wasm::WasmValue>& elements,
      Handle<Map> map);
  Handle<WasmArray> NewWasmArrayFromMemory(uint32_t length, Handle<Map> map,
                                           Address source);
  // Returns a handle to a WasmArray if successful, or a Smi containing a
  // {MessageTemplate} if computing the array's elements leads to an error.
  Handle<Object> NewWasmArrayFromElementSegment(
      Handle<WasmInstanceObject> instance, const wasm::WasmElemSegment* segment,
      uint32_t start_offset, uint32_t length, Handle<Map> map);
  Handle<WasmContinuationObject> NewWasmContinuationObject(
      Address jmpbuf, Handle<Foreign> managed_stack, Handle<HeapObject> parent,
      AllocationType allocation = AllocationType::kYoung);

  Handle<SharedFunctionInfo> NewSharedFunctionInfoForWasmExportedFunction(
      Handle<String> name, Handle<WasmExportedFunctionData> data);
  Handle<SharedFunctionInfo> NewSharedFunctionInfoForWasmJSFunction(
      Handle<String> name, Handle<WasmJSFunctionData> data);
  Handle<SharedFunctionInfo> NewSharedFunctionInfoForWasmResume(
      Handle<WasmResumeData> data);
  Handle<SharedFunctionInfo> NewSharedFunctionInfoForWasmCapiFunction(
      Handle<WasmCapiFunctionData> data);
#endif  // V8_ENABLE_WEBASSEMBLY

  Handle<SourceTextModule> NewSourceTextModule(Handle<SharedFunctionInfo> code);
  Handle<SyntheticModule> NewSyntheticModule(
      Handle<String> module_name, Handle<FixedArray> export_names,
      v8::Module::SyntheticModuleEvaluationSteps evaluation_steps);

  Handle<JSArrayBuffer> NewJSArrayBuffer(
      std::shared_ptr<BackingStore> backing_store,
      AllocationType allocation = AllocationType::kYoung);

  MaybeHandle<JSArrayBuffer> NewJSArrayBufferAndBackingStore(
      size_t byte_length, InitializedFlag initialized,
      AllocationType allocation = AllocationType::kYoung);

  MaybeHandle<JSArrayBuffer> NewJSArrayBufferAndBackingStore(
      size_t byte_length, size_t max_byte_length, InitializedFlag initialized,
      ResizableFlag resizable = ResizableFlag::kNotResizable,
      AllocationType allocation = AllocationType::kYoung);

  Handle<JSArrayBuffer> NewJSSharedArrayBuffer(
      std::shared_ptr<BackingStore> backing_store);

  static void TypeAndSizeForElementsKind(ElementsKind kind,
                                         ExternalArrayType* array_type,
                                         size_t* element_size);

  // Creates a new JSTypedArray with the specified buffer.
  Handle<JSTypedArray> NewJSTypedArray(ExternalArrayType type,
                                       Handle<JSArrayBuffer> buffer,
                                       size_t byte_offset, size_t length,
                                       bool is_length_tracking = false);

  Handle<JSDataViewOrRabGsabDataView> NewJSDataViewOrRabGsabDataView(
      Handle<JSArrayBuffer> buffer, size_t byte_offset, size_t byte_length,
      bool is_length_tracking = false);

  Handle<JSIteratorResult> NewJSIteratorResult(Handle<Object> value, bool done);
  Handle<JSAsyncFromSyncIterator> NewJSAsyncFromSyncIterator(
      Handle<JSReceiver> sync_iterator, Handle<Object> next);

  Handle<JSMap> NewJSMap();
  Handle<JSSet> NewJSSet();

  // Allocates a bound function.
  MaybeHandle<JSBoundFunction> NewJSBoundFunction(
      Handle<JSReceiver> target_function, Handle<Object> bound_this,
      base::Vector<Handle<Object>> bound_args);

  // Allocates a Harmony proxy.
  Handle<JSProxy> NewJSProxy(Handle<JSReceiver> target,
                             Handle<JSReceiver> handler);

  // Reinitialize an JSGlobalProxy based on a constructor.  The object
  // must have the same size as objects allocated using the
  // constructor.  The object is reinitialized and behaves as an
  // object that has been freshly allocated using the constructor.
  void ReinitializeJSGlobalProxy(Handle<JSGlobalProxy> global,
                                 Handle<JSFunction> constructor);

  Handle<JSGlobalProxy> NewUninitializedJSGlobalProxy(int size);

  // For testing only. Creates a sloppy function without code.
  Handle<JSFunction> NewFunctionForTesting(Handle<String> name);

  // Create an External object for V8's external API.
  Handle<JSObject> NewExternal(void* value);

  Handle<DeoptimizationLiteralArray> NewDeoptimizationLiteralArray(int length);

  // Allocates a new code object and initializes it as the trampoline to the
  // given off-heap entry point.
  Handle<Code> NewOffHeapTrampolineFor(Handle<Code> code,
                                       Address off_heap_entry);

  Handle<BytecodeArray> CopyBytecodeArray(Handle<BytecodeArray>);

  // Interface for creating error objects.
  Handle<JSObject> NewError(Handle<JSFunction> constructor,
                            Handle<String> message);

  Handle<Object> NewInvalidStringLengthError();

  inline Handle<Object> NewURIError();

  Handle<JSObject> NewError(Handle<JSFunction> constructor,
                            MessageTemplate template_index,
                            Handle<Object> arg0 = Handle<Object>(),
                            Handle<Object> arg1 = Handle<Object>(),
                            Handle<Object> arg2 = Handle<Object>());

#define DECLARE_ERROR(NAME)                                          \
  Handle<JSObject> New##NAME(MessageTemplate template_index,         \
                             Handle<Object> arg0 = Handle<Object>(), \
                             Handle<Object> arg1 = Handle<Object>(), \
                             Handle<Object> arg2 = Handle<Object>());
  DECLARE_ERROR(Error)
  DECLARE_ERROR(EvalError)
  DECLARE_ERROR(RangeError)
  DECLARE_ERROR(ReferenceError)
  DECLARE_ERROR(SyntaxError)
  DECLARE_ERROR(TypeError)
  DECLARE_ERROR(WasmCompileError)
  DECLARE_ERROR(WasmLinkError)
  DECLARE_ERROR(WasmRuntimeError)
  DECLARE_ERROR(WasmExceptionError)
#undef DECLARE_ERROR

  Handle<String> SizeToString(size_t value, bool check_cache = true);
  inline Handle<String> Uint32ToString(uint32_t value,
                                       bool check_cache = true) {
    return SizeToString(value, check_cache);
  }

#define ROOT_ACCESSOR(Type, name, CamelName) inline Handle<Type> name();
  MUTABLE_ROOT_LIST(ROOT_ACCESSOR)
#undef ROOT_ACCESSOR

  // Allocates a new SharedFunctionInfo object.
  Handle<SharedFunctionInfo> NewSharedFunctionInfoForApiFunction(
      MaybeHandle<String> maybe_name,
      Handle<FunctionTemplateInfo> function_template_info, FunctionKind kind);

  Handle<SharedFunctionInfo> NewSharedFunctionInfoForBuiltin(
      MaybeHandle<String> name, Builtin builtin,
      FunctionKind kind = FunctionKind::kNormalFunction);

  static bool IsFunctionModeWithPrototype(FunctionMode function_mode) {
    return (function_mode & kWithPrototypeBits) != 0;
  }

  static bool IsFunctionModeWithWritablePrototype(FunctionMode function_mode) {
    return (function_mode & kWithWritablePrototypeBit) != 0;
  }

  static bool IsFunctionModeWithName(FunctionMode function_mode) {
    return (function_mode & kWithNameBit) != 0;
  }

  Handle<Map> CreateSloppyFunctionMap(
      FunctionMode function_mode, MaybeHandle<JSFunction> maybe_empty_function);

  Handle<Map> CreateStrictFunctionMap(FunctionMode function_mode,
                                      Handle<JSFunction> empty_function);

  Handle<Map> CreateClassFunctionMap(Handle<JSFunction> empty_function);

  // Allocates a new JSMessageObject object.
  Handle<JSMessageObject> NewJSMessageObject(
      MessageTemplate message, Handle<Object> argument, int start_position,
      int end_position, Handle<SharedFunctionInfo> shared_info,
      int bytecode_offset, Handle<Script> script, Handle<Object> stack_frames);

  Handle<DebugInfo> NewDebugInfo(Handle<SharedFunctionInfo> shared);

  // Return a map for given number of properties using the map cache in the
  // native context.
  Handle<Map> ObjectLiteralMapFromCache(Handle<NativeContext> native_context,
                                        int number_of_properties);

  Handle<LoadHandler> NewLoadHandler(
      int data_count, AllocationType allocation = AllocationType::kOld);
  Handle<StoreHandler> NewStoreHandler(int data_count);
  Handle<MegaDomHandler> NewMegaDomHandler(MaybeObjectHandle accessor,
                                           MaybeObjectHandle context);
  Handle<RegExpMatchInfo> NewRegExpMatchInfo();

  // Creates a new FixedArray that holds the data associated with the
  // atom regexp and stores it in the regexp.
  void SetRegExpAtomData(Handle<JSRegExp> regexp, Handle<String> source,
                         JSRegExp::Flags flags, Handle<Object> match_pattern);

  // Creates a new FixedArray that holds the data associated with the
  // irregexp regexp and stores it in the regexp.
  void SetRegExpIrregexpData(Handle<JSRegExp> regexp, Handle<String> source,
                             JSRegExp::Flags flags, int capture_count,
                             uint32_t backtrack_limit);

  // Creates a new FixedArray that holds the data associated with the
  // experimental regexp and stores it in the regexp.
  void SetRegExpExperimentalData(Handle<JSRegExp> regexp, Handle<String> source,
                                 JSRegExp::Flags flags, int capture_count);

  // Returns the value for a known global constant (a property of the global
  // object which is neither configurable nor writable) like 'undefined'.
  // Returns a null handle when the given name is unknown.
  Handle<Object> GlobalConstantFor(Handle<Name> name);

  // Converts the given ToPrimitive hint to it's string representation.
  Handle<String> ToPrimitiveHintString(ToPrimitiveHint hint);

  Handle<JSPromise> NewJSPromiseWithoutHook();
  Handle<JSPromise> NewJSPromise();

  Handle<CallHandlerInfo> NewCallHandlerInfo(bool has_no_side_effect = false);

  HeapObject NewForTest(Handle<Map> map, AllocationType allocation) {
    return New(map, allocation);
  }

  Handle<JSSharedStruct> NewJSSharedStruct(Handle<JSFunction> constructor);

  Handle<JSSharedArray> NewJSSharedArray(Handle<JSFunction> constructor,
                                         int length);

  Handle<JSAtomicsMutex> NewJSAtomicsMutex();

  Handle<JSAtomicsCondition> NewJSAtomicsCondition();

  // Helper class for creating JSFunction objects.
  class V8_EXPORT_PRIVATE JSFunctionBuilder final {
   public:
    JSFunctionBuilder(Isolate* isolate, Handle<SharedFunctionInfo> sfi,
                      Handle<Context> context);

    V8_WARN_UNUSED_RESULT Handle<JSFunction> Build();

    JSFunctionBuilder& set_map(Handle<Map> v) {
      maybe_map_ = v;
      return *this;
    }
    JSFunctionBuilder& set_allocation_type(AllocationType v) {
      allocation_type_ = v;
      return *this;
    }
    JSFunctionBuilder& set_feedback_cell(Handle<FeedbackCell> v) {
      maybe_feedback_cell_ = v;
      return *this;
    }

   private:
    void PrepareMap();
    void PrepareFeedbackCell();

    V8_WARN_UNUSED_RESULT Handle<JSFunction> BuildRaw(Handle<Code> code);

    Isolate* const isolate_;
    Handle<SharedFunctionInfo> sfi_;
    Handle<Context> context_;
    MaybeHandle<Map> maybe_map_;
    MaybeHandle<FeedbackCell> maybe_feedback_cell_;
    AllocationType allocation_type_ = AllocationType::kOld;

    friend class Factory;
  };

  // Allows creation of InstructionStream objects. It provides two build
  // methods, one of which tries to gracefully handle allocation failure.
  class V8_EXPORT_PRIVATE CodeBuilder final {
   public:
    CodeBuilder(Isolate* isolate, const CodeDesc& desc, CodeKind kind);

    // TODO(victorgomes): Remove Isolate dependency from CodeBuilder.
    CodeBuilder(LocalIsolate* local_isolate, const CodeDesc& desc,
                CodeKind kind);

    // Builds a new code object (fully initialized). All header fields of the
    // associated InstructionStream are immutable and the InstructionStream
    // object is write protected.
    V8_WARN_UNUSED_RESULT Handle<Code> Build();
    // Like Build, builds a new code object. May return an empty handle if the
    // allocation fails.
    V8_WARN_UNUSED_RESULT MaybeHandle<Code> TryBuild();

    // Sets the self-reference object in which a reference to the code object is
    // stored. This allows generated code to reference its own InstructionStream
    // object by using this handle.
    CodeBuilder& set_self_reference(Handle<Object> self_reference) {
      DCHECK(!self_reference.is_null());
      self_reference_ = self_reference;
      return *this;
    }

    CodeBuilder& set_builtin(Builtin builtin) {
      DCHECK_IMPLIES(builtin != Builtin::kNoBuiltinId,
                     !CodeKindIsJSFunction(kind_));
      builtin_ = builtin;
      return *this;
    }

    CodeBuilder& set_inlined_bytecode_size(uint32_t size) {
      DCHECK_IMPLIES(size != 0, CodeKindIsOptimizedJSFunction(kind_));
      inlined_bytecode_size_ = size;
      return *this;
    }

    CodeBuilder& set_osr_offset(BytecodeOffset offset) {
      DCHECK_IMPLIES(!offset.IsNone(), CodeKindCanOSR(kind_));
      osr_offset_ = offset;
      return *this;
    }

    CodeBuilder& set_source_position_table(Handle<ByteArray> table) {
      DCHECK_NE(kind_, CodeKind::BASELINE);
      DCHECK(!table.is_null());
      position_table_ = table;
      return *this;
    }

    CodeBuilder& set_bytecode_offset_table(Handle<ByteArray> table) {
      DCHECK_EQ(kind_, CodeKind::BASELINE);
      DCHECK(!table.is_null());
      position_table_ = table;
      return *this;
    }

    CodeBuilder& set_deoptimization_data(
        Handle<DeoptimizationData> deopt_data) {
      DCHECK_NE(kind_, CodeKind::BASELINE);
      DCHECK(!deopt_data.is_null());
      deoptimization_data_ = deopt_data;
      return *this;
    }

    inline CodeBuilder& set_interpreter_data(
        Handle<HeapObject> interpreter_data);

    CodeBuilder& set_is_turbofanned() {
      DCHECK(!CodeKindIsUnoptimizedJSFunction(kind_));
      is_turbofanned_ = true;
      return *this;
    }

    CodeBuilder& set_kind_specific_flags(int32_t flags) {
      kind_specific_flags_ = flags;
      return *this;
    }

    CodeBuilder& set_stack_slots(int stack_slots) {
      stack_slots_ = stack_slots;
      return *this;
    }

    CodeBuilder& set_profiler_data(BasicBlockProfilerData* profiler_data) {
      profiler_data_ = profiler_data;
      return *this;
    }

    inline bool CompiledWithConcurrentBaseline() const;

   private:
    MaybeHandle<Code> BuildInternal(bool retry_allocation_or_fail);
    MaybeHandle<InstructionStream> AllocateInstructionStream(
        bool retry_allocation_or_fail);
    MaybeHandle<InstructionStream> AllocateConcurrentSparkplugInstructionStream(
        bool retry_allocation_or_fail);

    Isolate* const isolate_;
    LocalIsolate* local_isolate_;
    const CodeDesc& code_desc_;
    const CodeKind kind_;

    MaybeHandle<Object> self_reference_;
    Builtin builtin_ = Builtin::kNoBuiltinId;
    uint32_t inlined_bytecode_size_ = 0;
    BytecodeOffset osr_offset_ = BytecodeOffset::None();
    int32_t kind_specific_flags_ = 0;
    // Either source_position_table for non-baseline code
    // or bytecode_offset_table for baseline code.
    Handle<ByteArray> position_table_;
    Handle<DeoptimizationData> deoptimization_data_ =
        DeoptimizationData::Empty(isolate_);
    Handle<HeapObject> interpreter_data_;
    BasicBlockProfilerData* profiler_data_ = nullptr;
    bool is_turbofanned_ = false;
    int stack_slots_ = 0;
  };

 private:
  friend class FactoryBase<Factory>;

  // ------
  // Customization points for FactoryBase
  HeapObject AllocateRaw(int size, AllocationType allocation,
                         AllocationAlignment alignment = kTaggedAligned);

  Isolate* isolate() const {
    // Downcast to the privately inherited sub-class using c-style casts to
    // avoid undefined behavior (as static_cast cannot cast across private
    // bases).
    // NOLINTNEXTLINE (google-readability-casting)
    return (Isolate*)this;  // NOLINT(readability/casting)
  }

  // This is the real Isolate that will be used for allocating and accessing
  // external pointer entries when the sandbox is enabled.
  Isolate* isolate_for_sandbox() const {
#ifdef V8_ENABLE_SANDBOX
    return isolate();
#else
    return nullptr;
#endif  // V8_ENABLE_SANDBOX
  }

  V8_INLINE HeapAllocator* allocator() const;

  bool CanAllocateInReadOnlySpace();
  bool EmptyStringRootIsInitialized();
  AllocationType AllocationTypeForInPlaceInternalizableString();

  void AddToScriptList(Handle<Script> shared);
  // ------

  HeapObject AllocateRawWithAllocationSite(
      Handle<Map> map, AllocationType allocation,
      Handle<AllocationSite> allocation_site);

  Handle<JSArrayBufferView> NewJSArrayBufferView(
      Handle<Map> map, Handle<FixedArrayBase> elements,
      Handle<JSArrayBuffer> buffer, size_t byte_offset, size_t byte_length);

  Symbol NewSymbolInternal(AllocationType allocation = AllocationType::kOld);

  // Allocates new context with given map, sets length and initializes the
  // after-header part with uninitialized values and leaves the context header
  // uninitialized.
  Context NewContextInternal(Handle<Map> map, int size,
                             int variadic_part_length,
                             AllocationType allocation);

  template <typename T>
  Handle<T> AllocateSmallOrderedHashTable(Handle<Map> map, int capacity,
                                          AllocationType allocation);

  // Creates a heap object based on the map. The fields of the heap object are
  // not initialized, it's the responsibility of the caller to do that.
  HeapObject New(Handle<Map> map, AllocationType allocation);

  template <typename T>
  Handle<T> CopyArrayWithMap(Handle<T> src, Handle<Map> map);
  template <typename T>
  Handle<T> CopyArrayAndGrow(Handle<T> src, int grow_by,
                             AllocationType allocation);

  MaybeHandle<String> NewStringFromTwoByte(const base::uc16* string, int length,
                                           AllocationType allocation);

  // Functions to get the hash of a number for the number_string_cache.
  int NumberToStringCacheHash(Smi number);
  int NumberToStringCacheHash(double number);

  // Attempt to find the number in a small cache.  If we finds it, return
  // the string representation of the number.  Otherwise return undefined.
  V8_INLINE Handle<Object> NumberToStringCacheGet(Object number, int hash);

  // Update the cache with a new number-string pair.
  V8_INLINE void NumberToStringCacheSet(Handle<Object> number, int hash,
                                        Handle<String> js_string);

  // Creates a new JSArray with the given backing storage. Performs no
  // verification of the backing storage because it may not yet be filled.
  Handle<JSArray> NewJSArrayWithUnverifiedElements(
      Handle<FixedArrayBase> elements, ElementsKind elements_kind, int length,
      AllocationType allocation = AllocationType::kYoung);
  Handle<JSArray> NewJSArrayWithUnverifiedElements(
      Handle<Map> map, Handle<FixedArrayBase> elements, int length,
      AllocationType allocation = AllocationType::kYoung);

  // Creates the backing storage for a JSArray. This handle must be discarded
  // before returning the JSArray reference to code outside Factory, which might
  // decide to left-trim the backing store. To avoid unnecessary HandleScopes,
  // this method requires capacity greater than zero.
  Handle<FixedArrayBase> NewJSArrayStorage(
      ElementsKind elements_kind, int capacity,
      ArrayStorageAllocationMode mode = DONT_INITIALIZE_ARRAY_ELEMENTS);

  void InitializeAllocationMemento(AllocationMemento memento,
                                   AllocationSite allocation_site);

  // Initializes a JSObject based on its map.
  void InitializeJSObjectFromMap(JSObject obj, Object properties, Map map);
  // Initializes JSObject body starting at given offset.
  void InitializeJSObjectBody(JSObject obj, Map map, int start_offset);

  Handle<WeakArrayList> NewUninitializedWeakArrayList(
      int capacity, AllocationType allocation = AllocationType::kYoung);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_FACTORY_H_
