// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ROOTS_H_
#define V8_ROOTS_H_

#include "src/accessors.h"
#include "src/globals.h"
#include "src/heap-symbols.h"
#include "src/objects-definitions.h"
#include "src/objects/slots.h"

namespace v8 {
namespace internal {

// Forward declarations.
enum ElementsKind : uint8_t;
class FixedTypedArrayBase;
template <typename T>
class Handle;
class Heap;
class Isolate;
class Map;
class PropertyCell;
class String;
class Symbol;
class RootVisitor;

// Defines all the read-only roots in Heap.
#define STRONG_READ_ONLY_ROOT_LIST(V)                                          \
  /* Cluster the most popular ones in a few cache lines here at the top.    */ \
  /* The first 32 entries are most often used in the startup snapshot and   */ \
  /* can use a shorter representation in the serialization format.          */ \
  V(Map, free_space_map, FreeSpaceMap)                                         \
  V(Map, one_pointer_filler_map, OnePointerFillerMap)                          \
  V(Map, two_pointer_filler_map, TwoPointerFillerMap)                          \
  V(Oddball*, uninitialized_value, UninitializedValue)                         \
  V(Oddball*, undefined_value, UndefinedValue)                                 \
  V(Oddball*, the_hole_value, TheHoleValue)                                    \
  V(Oddball*, null_value, NullValue)                                           \
  V(Oddball*, true_value, TrueValue)                                           \
  V(Oddball*, false_value, FalseValue)                                         \
  V(String, empty_string, empty_string)                                        \
  V(Map, meta_map, MetaMap)                                                    \
  V(Map, byte_array_map, ByteArrayMap)                                         \
  V(Map, fixed_array_map, FixedArrayMap)                                       \
  V(Map, fixed_cow_array_map, FixedCOWArrayMap)                                \
  V(Map, hash_table_map, HashTableMap)                                         \
  V(Map, symbol_map, SymbolMap)                                                \
  V(Map, one_byte_string_map, OneByteStringMap)                                \
  V(Map, one_byte_internalized_string_map, OneByteInternalizedStringMap)       \
  V(Map, scope_info_map, ScopeInfoMap)                                         \
  V(Map, shared_function_info_map, SharedFunctionInfoMap)                      \
  V(Map, code_map, CodeMap)                                                    \
  V(Map, function_context_map, FunctionContextMap)                             \
  V(Map, cell_map, CellMap)                                                    \
  V(Map, global_property_cell_map, GlobalPropertyCellMap)                      \
  V(Map, foreign_map, ForeignMap)                                              \
  V(Map, heap_number_map, HeapNumberMap)                                       \
  V(Map, transition_array_map, TransitionArrayMap)                             \
  V(Map, feedback_vector_map, FeedbackVectorMap)                               \
  V(ScopeInfo, empty_scope_info, EmptyScopeInfo)                               \
  V(FixedArray, empty_fixed_array, EmptyFixedArray)                            \
  V(DescriptorArray, empty_descriptor_array, EmptyDescriptorArray)             \
  /* Entries beyond the first 32                                            */ \
  /* Oddballs */                                                               \
  V(Oddball*, arguments_marker, ArgumentsMarker)                               \
  V(Oddball*, exception, Exception)                                            \
  V(Oddball*, termination_exception, TerminationException)                     \
  V(Oddball*, optimized_out, OptimizedOut)                                     \
  V(Oddball*, stale_register, StaleRegister)                                   \
  /* Context maps */                                                           \
  V(Map, native_context_map, NativeContextMap)                                 \
  V(Map, module_context_map, ModuleContextMap)                                 \
  V(Map, eval_context_map, EvalContextMap)                                     \
  V(Map, script_context_map, ScriptContextMap)                                 \
  V(Map, await_context_map, AwaitContextMap)                                   \
  V(Map, block_context_map, BlockContextMap)                                   \
  V(Map, catch_context_map, CatchContextMap)                                   \
  V(Map, with_context_map, WithContextMap)                                     \
  V(Map, debug_evaluate_context_map, DebugEvaluateContextMap)                  \
  V(Map, script_context_table_map, ScriptContextTableMap)                      \
  /* Maps */                                                                   \
  V(Map, feedback_metadata_map, FeedbackMetadataArrayMap)                      \
  V(Map, array_list_map, ArrayListMap)                                         \
  V(Map, bigint_map, BigIntMap)                                                \
  V(Map, object_boilerplate_description_map, ObjectBoilerplateDescriptionMap)  \
  V(Map, bytecode_array_map, BytecodeArrayMap)                                 \
  V(Map, code_data_container_map, CodeDataContainerMap)                        \
  V(Map, descriptor_array_map, DescriptorArrayMap)                             \
  V(Map, fixed_double_array_map, FixedDoubleArrayMap)                          \
  V(Map, global_dictionary_map, GlobalDictionaryMap)                           \
  V(Map, many_closures_cell_map, ManyClosuresCellMap)                          \
  V(Map, module_info_map, ModuleInfoMap)                                       \
  V(Map, mutable_heap_number_map, MutableHeapNumberMap)                        \
  V(Map, name_dictionary_map, NameDictionaryMap)                               \
  V(Map, no_closures_cell_map, NoClosuresCellMap)                              \
  V(Map, no_feedback_cell_map, NoFeedbackCellMap)                              \
  V(Map, number_dictionary_map, NumberDictionaryMap)                           \
  V(Map, one_closure_cell_map, OneClosureCellMap)                              \
  V(Map, ordered_hash_map_map, OrderedHashMapMap)                              \
  V(Map, ordered_hash_set_map, OrderedHashSetMap)                              \
  V(Map, ordered_name_dictionary_map, OrderedNameDictionaryMap)                \
  V(Map, pre_parsed_scope_data_map, PreParsedScopeDataMap)                     \
  V(Map, property_array_map, PropertyArrayMap)                                 \
  V(Map, side_effect_call_handler_info_map, SideEffectCallHandlerInfoMap)      \
  V(Map, side_effect_free_call_handler_info_map,                               \
    SideEffectFreeCallHandlerInfoMap)                                          \
  V(Map, next_call_side_effect_free_call_handler_info_map,                     \
    NextCallSideEffectFreeCallHandlerInfoMap)                                  \
  V(Map, simple_number_dictionary_map, SimpleNumberDictionaryMap)              \
  V(Map, sloppy_arguments_elements_map, SloppyArgumentsElementsMap)            \
  V(Map, small_ordered_hash_map_map, SmallOrderedHashMapMap)                   \
  V(Map, small_ordered_hash_set_map, SmallOrderedHashSetMap)                   \
  V(Map, small_ordered_name_dictionary_map, SmallOrderedNameDictionaryMap)     \
  V(Map, string_table_map, StringTableMap)                                     \
  V(Map, uncompiled_data_without_pre_parsed_scope_map,                         \
    UncompiledDataWithoutPreParsedScopeMap)                                    \
  V(Map, uncompiled_data_with_pre_parsed_scope_map,                            \
    UncompiledDataWithPreParsedScopeMap)                                       \
  V(Map, weak_fixed_array_map, WeakFixedArrayMap)                              \
  V(Map, weak_array_list_map, WeakArrayListMap)                                \
  V(Map, ephemeron_hash_table_map, EphemeronHashTableMap)                      \
  V(Map, embedder_data_array_map, EmbedderDataArrayMap)                        \
  /* String maps */                                                            \
  V(Map, native_source_string_map, NativeSourceStringMap)                      \
  V(Map, string_map, StringMap)                                                \
  V(Map, cons_one_byte_string_map, ConsOneByteStringMap)                       \
  V(Map, cons_string_map, ConsStringMap)                                       \
  V(Map, thin_one_byte_string_map, ThinOneByteStringMap)                       \
  V(Map, thin_string_map, ThinStringMap)                                       \
  V(Map, sliced_string_map, SlicedStringMap)                                   \
  V(Map, sliced_one_byte_string_map, SlicedOneByteStringMap)                   \
  V(Map, external_string_map, ExternalStringMap)                               \
  V(Map, external_string_with_one_byte_data_map,                               \
    ExternalStringWithOneByteDataMap)                                          \
  V(Map, external_one_byte_string_map, ExternalOneByteStringMap)               \
  V(Map, uncached_external_string_map, UncachedExternalStringMap)              \
  V(Map, uncached_external_string_with_one_byte_data_map,                      \
    UncachedExternalStringWithOneByteDataMap)                                  \
  V(Map, internalized_string_map, InternalizedStringMap)                       \
  V(Map, external_internalized_string_map, ExternalInternalizedStringMap)      \
  V(Map, external_internalized_string_with_one_byte_data_map,                  \
    ExternalInternalizedStringWithOneByteDataMap)                              \
  V(Map, external_one_byte_internalized_string_map,                            \
    ExternalOneByteInternalizedStringMap)                                      \
  V(Map, uncached_external_internalized_string_map,                            \
    UncachedExternalInternalizedStringMap)                                     \
  V(Map, uncached_external_internalized_string_with_one_byte_data_map,         \
    UncachedExternalInternalizedStringWithOneByteDataMap)                      \
  V(Map, uncached_external_one_byte_internalized_string_map,                   \
    UncachedExternalOneByteInternalizedStringMap)                              \
  V(Map, uncached_external_one_byte_string_map,                                \
    UncachedExternalOneByteStringMap)                                          \
  /* Array element maps */                                                     \
  V(Map, fixed_uint8_array_map, FixedUint8ArrayMap)                            \
  V(Map, fixed_int8_array_map, FixedInt8ArrayMap)                              \
  V(Map, fixed_uint16_array_map, FixedUint16ArrayMap)                          \
  V(Map, fixed_int16_array_map, FixedInt16ArrayMap)                            \
  V(Map, fixed_uint32_array_map, FixedUint32ArrayMap)                          \
  V(Map, fixed_int32_array_map, FixedInt32ArrayMap)                            \
  V(Map, fixed_float32_array_map, FixedFloat32ArrayMap)                        \
  V(Map, fixed_float64_array_map, FixedFloat64ArrayMap)                        \
  V(Map, fixed_uint8_clamped_array_map, FixedUint8ClampedArrayMap)             \
  V(Map, fixed_biguint64_array_map, FixedBigUint64ArrayMap)                    \
  V(Map, fixed_bigint64_array_map, FixedBigInt64ArrayMap)                      \
  /* Oddball maps */                                                           \
  V(Map, undefined_map, UndefinedMap)                                          \
  V(Map, the_hole_map, TheHoleMap)                                             \
  V(Map, null_map, NullMap)                                                    \
  V(Map, boolean_map, BooleanMap)                                              \
  V(Map, uninitialized_map, UninitializedMap)                                  \
  V(Map, arguments_marker_map, ArgumentsMarkerMap)                             \
  V(Map, exception_map, ExceptionMap)                                          \
  V(Map, termination_exception_map, TerminationExceptionMap)                   \
  V(Map, optimized_out_map, OptimizedOutMap)                                   \
  V(Map, stale_register_map, StaleRegisterMap)                                 \
  V(Map, self_reference_marker_map, SelfReferenceMarkerMap)                    \
  /* Canonical empty values */                                                 \
  V(EnumCache*, empty_enum_cache, EmptyEnumCache)                              \
  V(PropertyArray, empty_property_array, EmptyPropertyArray)                   \
  V(ByteArray, empty_byte_array, EmptyByteArray)                               \
  V(ObjectBoilerplateDescription, empty_object_boilerplate_description,        \
    EmptyObjectBoilerplateDescription)                                         \
  V(ArrayBoilerplateDescription*, empty_array_boilerplate_description,         \
    EmptyArrayBoilerplateDescription)                                          \
  V(FixedTypedArrayBase, empty_fixed_uint8_array, EmptyFixedUint8Array)        \
  V(FixedTypedArrayBase, empty_fixed_int8_array, EmptyFixedInt8Array)          \
  V(FixedTypedArrayBase, empty_fixed_uint16_array, EmptyFixedUint16Array)      \
  V(FixedTypedArrayBase, empty_fixed_int16_array, EmptyFixedInt16Array)        \
  V(FixedTypedArrayBase, empty_fixed_uint32_array, EmptyFixedUint32Array)      \
  V(FixedTypedArrayBase, empty_fixed_int32_array, EmptyFixedInt32Array)        \
  V(FixedTypedArrayBase, empty_fixed_float32_array, EmptyFixedFloat32Array)    \
  V(FixedTypedArrayBase, empty_fixed_float64_array, EmptyFixedFloat64Array)    \
  V(FixedTypedArrayBase, empty_fixed_uint8_clamped_array,                      \
    EmptyFixedUint8ClampedArray)                                               \
  V(FixedTypedArrayBase, empty_fixed_biguint64_array,                          \
    EmptyFixedBigUint64Array)                                                  \
  V(FixedTypedArrayBase, empty_fixed_bigint64_array, EmptyFixedBigInt64Array)  \
  V(FixedArray, empty_sloppy_arguments_elements, EmptySloppyArgumentsElements) \
  V(NumberDictionary, empty_slow_element_dictionary,                           \
    EmptySlowElementDictionary)                                                \
  V(FixedArray, empty_ordered_hash_map, EmptyOrderedHashMap)                   \
  V(FixedArray, empty_ordered_hash_set, EmptyOrderedHashSet)                   \
  V(FeedbackMetadata, empty_feedback_metadata, EmptyFeedbackMetadata)          \
  V(PropertyCell*, empty_property_cell, EmptyPropertyCell)                     \
  V(NameDictionary, empty_property_dictionary, EmptyPropertyDictionary)        \
  V(InterceptorInfo*, noop_interceptor_info, NoOpInterceptorInfo)              \
  V(WeakFixedArray, empty_weak_fixed_array, EmptyWeakFixedArray)               \
  V(WeakArrayList, empty_weak_array_list, EmptyWeakArrayList)                  \
  /* Special numbers */                                                        \
  V(HeapNumber*, nan_value, NanValue)                                          \
  V(HeapNumber*, hole_nan_value, HoleNanValue)                                 \
  V(HeapNumber*, infinity_value, InfinityValue)                                \
  V(HeapNumber*, minus_zero_value, MinusZeroValue)                             \
  V(HeapNumber*, minus_infinity_value, MinusInfinityValue)                     \
  /* Marker for self-references during code-generation */                      \
  V(HeapObject*, self_reference_marker, SelfReferenceMarker)                   \
  /* Canonical trampoline RelocInfo */                                         \
  V(ByteArray, off_heap_trampoline_relocation_info,                            \
    OffHeapTrampolineRelocationInfo)                                           \
  /* Hash seed */                                                              \
  V(ByteArray, hash_seed, HashSeed)

// Mutable roots that are known to be immortal immovable, for which we can
// safely skip write barriers.
#define STRONG_MUTABLE_IMMOVABLE_ROOT_LIST(V)                                 \
  ACCESSOR_INFO_ROOT_LIST(V)                                                  \
  /* Maps */                                                                  \
  V(Map, external_map, ExternalMap)                                           \
  V(Map, message_object_map, JSMessageObjectMap)                              \
  /* Canonical empty values */                                                \
  V(Script*, empty_script, EmptyScript)                                       \
  V(FeedbackCell*, many_closures_cell, ManyClosuresCell)                      \
  V(FeedbackCell*, no_feedback_cell, NoFeedbackCell)                          \
  V(Cell*, invalid_prototype_validity_cell, InvalidPrototypeValidityCell)     \
  /* Protectors */                                                            \
  V(Cell*, array_constructor_protector, ArrayConstructorProtector)            \
  V(PropertyCell*, no_elements_protector, NoElementsProtector)                \
  V(Cell*, is_concat_spreadable_protector, IsConcatSpreadableProtector)       \
  V(PropertyCell*, array_species_protector, ArraySpeciesProtector)            \
  V(PropertyCell*, typed_array_species_protector, TypedArraySpeciesProtector) \
  V(PropertyCell*, regexp_species_protector, RegExpSpeciesProtector)          \
  V(PropertyCell*, promise_species_protector, PromiseSpeciesProtector)        \
  V(Cell*, string_length_protector, StringLengthProtector)                    \
  V(PropertyCell*, array_iterator_protector, ArrayIteratorProtector)          \
  V(PropertyCell*, array_buffer_neutering_protector,                          \
    ArrayBufferNeuteringProtector)                                            \
  V(PropertyCell*, promise_hook_protector, PromiseHookProtector)              \
  V(Cell*, promise_resolve_protector, PromiseResolveProtector)                \
  V(PropertyCell*, map_iterator_protector, MapIteratorProtector)              \
  V(PropertyCell*, promise_then_protector, PromiseThenProtector)              \
  V(PropertyCell*, set_iterator_protector, SetIteratorProtector)              \
  V(PropertyCell*, string_iterator_protector, StringIteratorProtector)        \
  /* Caches */                                                                \
  V(FixedArray, single_character_string_cache, SingleCharacterStringCache)    \
  V(FixedArray, string_split_cache, StringSplitCache)                         \
  V(FixedArray, regexp_multiple_cache, RegExpMultipleCache)                   \
  /* Indirection lists for isolate-independent builtins */                    \
  V(FixedArray, builtins_constants_table, BuiltinsConstantsTable)             \
  /* JS Entries */                                                            \
  V(Code, js_entry_code, JsEntryCode)                                         \
  V(Code, js_construct_entry_code, JsConstructEntryCode)                      \
  V(Code, js_run_microtasks_entry_code, JsRunMicrotasksEntryCode)

// These root references can be updated by the mutator.
#define STRONG_MUTABLE_MOVABLE_ROOT_LIST(V)                                \
  /* Caches */                                                             \
  V(FixedArray, number_string_cache, NumberStringCache)                    \
  /* Lists and dictionaries */                                             \
  V(NameDictionary, public_symbol_table, PublicSymbolTable)                \
  V(NameDictionary, api_symbol_table, ApiSymbolTable)                      \
  V(NameDictionary, api_private_symbol_table, ApiPrivateSymbolTable)       \
  V(WeakArrayList, script_list, ScriptList)                                \
  V(SimpleNumberDictionary, code_stubs, CodeStubs)                         \
  V(FixedArray, materialized_objects, MaterializedObjects)                 \
  V(WeakArrayList, detached_contexts, DetachedContexts)                    \
  V(WeakArrayList, retaining_path_targets, RetainingPathTargets)           \
  V(WeakArrayList, retained_maps, RetainedMaps)                            \
  /* Feedback vectors that we need for code coverage or type profile */    \
  V(Object*, feedback_vectors_for_profiling_tools,                         \
    FeedbackVectorsForProfilingTools)                                      \
  V(WeakArrayList, noscript_shared_function_infos,                         \
    NoScriptSharedFunctionInfos)                                           \
  V(FixedArray, serialized_objects, SerializedObjects)                     \
  V(FixedArray, serialized_global_proxy_sizes, SerializedGlobalProxySizes) \
  V(TemplateList, message_listeners, MessageListeners)                     \
  /* Support for async stack traces */                                     \
  V(HeapObject*, current_microtask, CurrentMicrotask)                      \
  /* JSWeakFactory objects which need cleanup */                           \
  V(Object*, dirty_js_weak_factories, DirtyJSWeakFactories)                \
  /* KeepDuringJob set for JS WeakRefs */                                  \
  V(HeapObject*, weak_refs_keep_during_job, WeakRefsKeepDuringJob)         \
  V(HeapObject*, interpreter_entry_trampoline_for_profiling,               \
    InterpreterEntryTrampolineForProfiling)

// Entries in this list are limited to Smis and are not visited during GC.
#define SMI_ROOT_LIST(V)                                                       \
  V(Smi, stack_limit, StackLimit)                                              \
  V(Smi, real_stack_limit, RealStackLimit)                                     \
  V(Smi, last_script_id, LastScriptId)                                         \
  V(Smi, last_debugging_id, LastDebuggingId)                                   \
  /* To distinguish the function templates, so that we can find them in the */ \
  /* function cache of the native context. */                                  \
  V(Smi, next_template_serial_number, NextTemplateSerialNumber)                \
  V(Smi, arguments_adaptor_deopt_pc_offset, ArgumentsAdaptorDeoptPCOffset)     \
  V(Smi, construct_stub_create_deopt_pc_offset,                                \
    ConstructStubCreateDeoptPCOffset)                                          \
  V(Smi, construct_stub_invoke_deopt_pc_offset,                                \
    ConstructStubInvokeDeoptPCOffset)                                          \
  V(Smi, interpreter_entry_return_pc_offset, InterpreterEntryReturnPCOffset)

// Adapts one INTERNALIZED_STRING_LIST_GENERATOR entry to
// the ROOT_LIST-compatible entry
#define INTERNALIZED_STRING_LIST_ADAPTER(V, name, ...) V(String, name, name)

// Produces (String, name, CamelCase) entries
#define INTERNALIZED_STRING_ROOT_LIST(V) \
  INTERNALIZED_STRING_LIST_GENERATOR(INTERNALIZED_STRING_LIST_ADAPTER, V)

// Adapts one XXX_SYMBOL_LIST_GENERATOR entry to the ROOT_LIST-compatible entry
#define SYMBOL_ROOT_LIST_ADAPTER(V, name, ...) V(Symbol, name, name)

// Produces (Symbol, name, CamelCase) entries
#define PRIVATE_SYMBOL_ROOT_LIST(V) \
  PRIVATE_SYMBOL_LIST_GENERATOR(SYMBOL_ROOT_LIST_ADAPTER, V)
#define PUBLIC_SYMBOL_ROOT_LIST(V) \
  PUBLIC_SYMBOL_LIST_GENERATOR(SYMBOL_ROOT_LIST_ADAPTER, V)
#define WELL_KNOWN_SYMBOL_ROOT_LIST(V) \
  WELL_KNOWN_SYMBOL_LIST_GENERATOR(SYMBOL_ROOT_LIST_ADAPTER, V)

// Adapts one ACCESSOR_INFO_LIST_GENERATOR entry to the ROOT_LIST-compatible
// entry
#define ACCESSOR_INFO_ROOT_LIST_ADAPTER(V, name, CamelName, ...) \
  V(AccessorInfo*, name##_accessor, CamelName##Accessor)

// Produces (AccessorInfo*, name, CamelCase) entries
#define ACCESSOR_INFO_ROOT_LIST(V) \
  ACCESSOR_INFO_LIST_GENERATOR(ACCESSOR_INFO_ROOT_LIST_ADAPTER, V)

#define READ_ONLY_ROOT_LIST(V)     \
  STRONG_READ_ONLY_ROOT_LIST(V)    \
  INTERNALIZED_STRING_ROOT_LIST(V) \
  PRIVATE_SYMBOL_ROOT_LIST(V)      \
  PUBLIC_SYMBOL_ROOT_LIST(V)       \
  WELL_KNOWN_SYMBOL_ROOT_LIST(V)   \
  STRUCT_MAPS_LIST(V)              \
  ALLOCATION_SITE_MAPS_LIST(V)     \
  DATA_HANDLER_MAPS_LIST(V)

#define MUTABLE_ROOT_LIST(V)                \
  STRONG_MUTABLE_IMMOVABLE_ROOT_LIST(V)     \
  STRONG_MUTABLE_MOVABLE_ROOT_LIST(V)       \
  V(StringTable, string_table, StringTable) \
  SMI_ROOT_LIST(V)

#define ROOT_LIST(V)     \
  READ_ONLY_ROOT_LIST(V) \
  MUTABLE_ROOT_LIST(V)

// Declare all the root indices.  This defines the root list order.
// clang-format off
enum class RootIndex : uint16_t {
#define DECL(type, name, CamelName) k##CamelName,
  ROOT_LIST(DECL)
#undef DECL

  kRootListLength,

  // Helper aliases for inclusive regions of root indices.
  kFirstRoot = 0,
  kLastRoot = kRootListLength - 1,

#define ROOT(...) +1
  kReadOnlyRootsCount = 0 READ_ONLY_ROOT_LIST(ROOT),
  kImmortalImmovableRootsCount =
      kReadOnlyRootsCount STRONG_MUTABLE_IMMOVABLE_ROOT_LIST(ROOT),
#undef ROOT
  kFirstReadOnlyRoot = kFirstRoot,
  kLastReadOnlyRoot = kFirstReadOnlyRoot + kReadOnlyRootsCount - 1,

  // The strong roots visited by the garbage collector (not including read-only
  // roots).
  kFirstStrongRoot = kLastReadOnlyRoot + 1,
  // (kStringTable is not a strong root).
  kLastStrongRoot = kStringTable - 1,

  // All of the strong roots plus the read-only roots.
  kFirstStrongOrReadOnlyRoot = kFirstRoot,
  kLastStrongOrReadOnlyRoot = kLastStrongRoot,

  // All immortal immovable roots including read only ones.
  kFirstImmortalImmovableRoot = kFirstReadOnlyRoot,
  kLastImmortalImmovableRoot =
      kFirstImmortalImmovableRoot + kImmortalImmovableRootsCount - 1,

  kFirstSmiRoot = kStringTable + 1,
  kLastSmiRoot = kLastRoot
};
// clang-format on

// Represents a storage of V8 heap roots.
class RootsTable {
 public:
  static constexpr size_t kEntriesCount =
      static_cast<size_t>(RootIndex::kRootListLength);

  RootsTable() : roots_{} {}

  inline bool IsRootHandleLocation(Address* handle_location,
                                   RootIndex* index) const;

  template <typename T>
  bool IsRootHandle(Handle<T> handle, RootIndex* index) const;

  Object* const& operator[](RootIndex root_index) const {
    size_t index = static_cast<size_t>(root_index);
    DCHECK_LT(index, kEntriesCount);
    return roots_[index];
  }

  static const char* name(RootIndex root_index) {
    size_t index = static_cast<size_t>(root_index);
    DCHECK_LT(index, kEntriesCount);
    return root_names_[index];
  }

  static constexpr int offset_of(RootIndex root_index) {
    return static_cast<int>(root_index) * kPointerSize;
  }

  static RootIndex RootIndexForFixedTypedArray(ExternalArrayType array_type);
  static RootIndex RootIndexForFixedTypedArray(ElementsKind elements_kind);
  static RootIndex RootIndexForEmptyFixedTypedArray(ElementsKind elements_kind);

  // Immortal immovable root objects are allocated in OLD space and GC never
  // moves them and the root table entries are guaranteed to not be modified
  // after initialization. Note, however, that contents of those root objects
  // that are allocated in writable space can still be modified after
  // initialization.
  // Generated code can treat direct references to these roots as constants.
  static constexpr bool IsImmortalImmovable(RootIndex root_index) {
    STATIC_ASSERT(static_cast<int>(RootIndex::kFirstImmortalImmovableRoot) ==
                  0);
    return static_cast<unsigned>(root_index) <=
           static_cast<unsigned>(RootIndex::kLastImmortalImmovableRoot);
  }

 private:
  ObjectSlot begin() {
    return ObjectSlot(&roots_[static_cast<size_t>(RootIndex::kFirstRoot)]);
  }
  ObjectSlot end() {
    return ObjectSlot(&roots_[static_cast<size_t>(RootIndex::kLastRoot) + 1]);
  }

  // Used for iterating over all of the read-only and mutable strong roots.
  ObjectSlot strong_or_read_only_roots_begin() {
    STATIC_ASSERT(static_cast<size_t>(RootIndex::kLastReadOnlyRoot) ==
                  static_cast<size_t>(RootIndex::kFirstStrongRoot) - 1);
    return ObjectSlot(
        &roots_[static_cast<size_t>(RootIndex::kFirstStrongOrReadOnlyRoot)]);
  }
  ObjectSlot strong_or_read_only_roots_end() {
    return ObjectSlot(
        &roots_[static_cast<size_t>(RootIndex::kLastStrongOrReadOnlyRoot) + 1]);
  }

  // The read-only, strong and Smi roots as defined by these accessors are all
  // disjoint.
  ObjectSlot read_only_roots_begin() {
    return ObjectSlot(
        &roots_[static_cast<size_t>(RootIndex::kFirstReadOnlyRoot)]);
  }
  ObjectSlot read_only_roots_end() {
    return ObjectSlot(
        &roots_[static_cast<size_t>(RootIndex::kLastReadOnlyRoot) + 1]);
  }

  ObjectSlot strong_roots_begin() {
    return ObjectSlot(
        &roots_[static_cast<size_t>(RootIndex::kFirstStrongRoot)]);
  }
  ObjectSlot strong_roots_end() {
    return ObjectSlot(
        &roots_[static_cast<size_t>(RootIndex::kLastStrongRoot) + 1]);
  }

  ObjectSlot smi_roots_begin() {
    return ObjectSlot(&roots_[static_cast<size_t>(RootIndex::kFirstSmiRoot)]);
  }
  ObjectSlot smi_roots_end() {
    return ObjectSlot(
        &roots_[static_cast<size_t>(RootIndex::kLastSmiRoot) + 1]);
  }

  Object*& operator[](RootIndex root_index) {
    size_t index = static_cast<size_t>(root_index);
    DCHECK_LT(index, kEntriesCount);
    return roots_[index];
  }

  Object* roots_[kEntriesCount];
  static const char* root_names_[kEntriesCount];

  friend class Isolate;
  friend class Heap;
  friend class Factory;
  friend class ReadOnlyRoots;
  friend class RootsSerializer;
};

class ReadOnlyRoots {
 public:
  V8_INLINE explicit ReadOnlyRoots(Heap* heap);
  V8_INLINE explicit ReadOnlyRoots(Isolate* isolate);

// TODO(jkummerow): Drop std::remove_pointer after the migration to ObjectPtr.
#define ROOT_ACCESSOR(Type, name, CamelName) \
  V8_INLINE class Type name() const;         \
  V8_INLINE Handle<std::remove_pointer<Type>::type> name##_handle() const;

  READ_ONLY_ROOT_LIST(ROOT_ACCESSOR)
#undef ROOT_ACCESSOR

  V8_INLINE Map MapForFixedTypedArray(ExternalArrayType array_type);
  V8_INLINE Map MapForFixedTypedArray(ElementsKind elements_kind);
  V8_INLINE FixedTypedArrayBase EmptyFixedTypedArrayForMap(const Map map);

  // Iterate over all the read-only roots. This is not necessary for garbage
  // collection and is usually only performed as part of (de)serialization or
  // heap verification.
  void Iterate(RootVisitor* visitor);

 private:
  RootsTable& roots_table_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_ROOTS_H_
