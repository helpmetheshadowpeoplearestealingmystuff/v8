# Copyright 2019 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

# This file is automatically generated by mkgrokdump and should not
# be modified manually.

# List of known V8 instance types.
INSTANCE_TYPES = {
  0: "INTERNALIZED_STRING_TYPE",
  2: "EXTERNAL_INTERNALIZED_STRING_TYPE",
  8: "ONE_BYTE_INTERNALIZED_STRING_TYPE",
  10: "EXTERNAL_ONE_BYTE_INTERNALIZED_STRING_TYPE",
  18: "UNCACHED_EXTERNAL_INTERNALIZED_STRING_TYPE",
  26: "UNCACHED_EXTERNAL_ONE_BYTE_INTERNALIZED_STRING_TYPE",
  32: "STRING_TYPE",
  33: "CONS_STRING_TYPE",
  34: "EXTERNAL_STRING_TYPE",
  35: "SLICED_STRING_TYPE",
  37: "THIN_STRING_TYPE",
  40: "ONE_BYTE_STRING_TYPE",
  41: "CONS_ONE_BYTE_STRING_TYPE",
  42: "EXTERNAL_ONE_BYTE_STRING_TYPE",
  43: "SLICED_ONE_BYTE_STRING_TYPE",
  45: "THIN_ONE_BYTE_STRING_TYPE",
  50: "UNCACHED_EXTERNAL_STRING_TYPE",
  58: "UNCACHED_EXTERNAL_ONE_BYTE_STRING_TYPE",
  64: "SYMBOL_TYPE",
  65: "BIG_INT_BASE_TYPE",
  66: "HEAP_NUMBER_TYPE",
  67: "ODDBALL_TYPE",
  68: "SOURCE_TEXT_MODULE_TYPE",
  69: "SYNTHETIC_MODULE_TYPE",
  70: "FOREIGN_TYPE",
  71: "PROMISE_FULFILL_REACTION_JOB_TASK_TYPE",
  72: "PROMISE_REJECT_REACTION_JOB_TASK_TYPE",
  73: "CALLABLE_TASK_TYPE",
  74: "CALLBACK_TASK_TYPE",
  75: "PROMISE_RESOLVE_THENABLE_JOB_TASK_TYPE",
  76: "LOAD_HANDLER_TYPE",
  77: "STORE_HANDLER_TYPE",
  78: "FUNCTION_TEMPLATE_INFO_TYPE",
  79: "OBJECT_TEMPLATE_INFO_TYPE",
  80: "ACCESS_CHECK_INFO_TYPE",
  81: "ACCESSOR_INFO_TYPE",
  82: "ACCESSOR_PAIR_TYPE",
  83: "ALIASED_ARGUMENTS_ENTRY_TYPE",
  84: "ALLOCATION_MEMENTO_TYPE",
  85: "ALLOCATION_SITE_TYPE",
  86: "ARRAY_BOILERPLATE_DESCRIPTION_TYPE",
  87: "ASM_WASM_DATA_TYPE",
  88: "ASYNC_GENERATOR_REQUEST_TYPE",
  89: "BREAK_POINT_TYPE",
  90: "BREAK_POINT_INFO_TYPE",
  91: "CACHED_TEMPLATE_OBJECT_TYPE",
  92: "CALL_HANDLER_INFO_TYPE",
  93: "CLASS_POSITIONS_TYPE",
  94: "DEBUG_INFO_TYPE",
  95: "ENUM_CACHE_TYPE",
  96: "FEEDBACK_CELL_TYPE",
  97: "FUNCTION_TEMPLATE_RARE_DATA_TYPE",
  98: "INTERCEPTOR_INFO_TYPE",
  99: "INTERNAL_CLASS_TYPE",
  100: "INTERPRETER_DATA_TYPE",
  101: "PROMISE_CAPABILITY_TYPE",
  102: "PROMISE_REACTION_TYPE",
  103: "PROPERTY_DESCRIPTOR_OBJECT_TYPE",
  104: "PROTOTYPE_INFO_TYPE",
  105: "SCRIPT_TYPE",
  106: "SMI_BOX_TYPE",
  107: "SMI_PAIR_TYPE",
  108: "SORT_STATE_TYPE",
  109: "SOURCE_TEXT_MODULE_INFO_ENTRY_TYPE",
  110: "STACK_FRAME_INFO_TYPE",
  111: "STACK_TRACE_FRAME_TYPE",
  112: "TEMPLATE_OBJECT_DESCRIPTION_TYPE",
  113: "TUPLE2_TYPE",
  114: "WASM_CAPI_FUNCTION_DATA_TYPE",
  115: "WASM_DEBUG_INFO_TYPE",
  116: "WASM_EXCEPTION_TAG_TYPE",
  117: "WASM_EXPORTED_FUNCTION_DATA_TYPE",
  118: "WASM_INDIRECT_FUNCTION_TABLE_TYPE",
  119: "WASM_JS_FUNCTION_DATA_TYPE",
  120: "FIXED_ARRAY_TYPE",
  121: "HASH_TABLE_TYPE",
  122: "EPHEMERON_HASH_TABLE_TYPE",
  123: "GLOBAL_DICTIONARY_TYPE",
  124: "NAME_DICTIONARY_TYPE",
  125: "NUMBER_DICTIONARY_TYPE",
  126: "ORDERED_HASH_MAP_TYPE",
  127: "ORDERED_HASH_SET_TYPE",
  128: "ORDERED_NAME_DICTIONARY_TYPE",
  129: "SIMPLE_NUMBER_DICTIONARY_TYPE",
  130: "STRING_TABLE_TYPE",
  131: "CLOSURE_FEEDBACK_CELL_ARRAY_TYPE",
  132: "OBJECT_BOILERPLATE_DESCRIPTION_TYPE",
  133: "SCOPE_INFO_TYPE",
  134: "SCRIPT_CONTEXT_TABLE_TYPE",
  135: "BYTE_ARRAY_TYPE",
  136: "BYTECODE_ARRAY_TYPE",
  137: "FIXED_DOUBLE_ARRAY_TYPE",
  138: "AWAIT_CONTEXT_TYPE",
  139: "BLOCK_CONTEXT_TYPE",
  140: "CATCH_CONTEXT_TYPE",
  141: "DEBUG_EVALUATE_CONTEXT_TYPE",
  142: "EVAL_CONTEXT_TYPE",
  143: "FUNCTION_CONTEXT_TYPE",
  144: "MODULE_CONTEXT_TYPE",
  145: "NATIVE_CONTEXT_TYPE",
  146: "SCRIPT_CONTEXT_TYPE",
  147: "WITH_CONTEXT_TYPE",
  148: "SMALL_ORDERED_HASH_MAP_TYPE",
  149: "SMALL_ORDERED_HASH_SET_TYPE",
  150: "SMALL_ORDERED_NAME_DICTIONARY_TYPE",
  151: "UNCOMPILED_DATA_WITH_PREPARSE_DATA_TYPE",
  152: "UNCOMPILED_DATA_WITHOUT_PREPARSE_DATA_TYPE",
  153: "WEAK_FIXED_ARRAY_TYPE",
  154: "TRANSITION_ARRAY_TYPE",
  155: "CELL_TYPE",
  156: "CODE_TYPE",
  157: "CODE_DATA_CONTAINER_TYPE",
  158: "DESCRIPTOR_ARRAY_TYPE",
  159: "EMBEDDER_DATA_ARRAY_TYPE",
  160: "FEEDBACK_METADATA_TYPE",
  161: "FEEDBACK_VECTOR_TYPE",
  162: "FILLER_TYPE",
  163: "FREE_SPACE_TYPE",
  164: "MAP_TYPE",
  165: "PREPARSE_DATA_TYPE",
  166: "PROPERTY_ARRAY_TYPE",
  167: "PROPERTY_CELL_TYPE",
  168: "SHARED_FUNCTION_INFO_TYPE",
  169: "WEAK_ARRAY_LIST_TYPE",
  170: "WEAK_CELL_TYPE",
  171: "JS_PROXY_TYPE",
  1057: "JS_OBJECT_TYPE",
  172: "JS_GLOBAL_OBJECT_TYPE",
  173: "JS_GLOBAL_PROXY_TYPE",
  174: "JS_MODULE_NAMESPACE_TYPE",
  1040: "JS_SPECIAL_API_OBJECT_TYPE",
  1041: "JS_PRIMITIVE_WRAPPER_TYPE",
  1042: "JS_MAP_KEY_ITERATOR_TYPE",
  1043: "JS_MAP_KEY_VALUE_ITERATOR_TYPE",
  1044: "JS_MAP_VALUE_ITERATOR_TYPE",
  1045: "JS_SET_KEY_VALUE_ITERATOR_TYPE",
  1046: "JS_SET_VALUE_ITERATOR_TYPE",
  1047: "JS_GENERATOR_OBJECT_TYPE",
  1048: "JS_ASYNC_FUNCTION_OBJECT_TYPE",
  1049: "JS_ASYNC_GENERATOR_OBJECT_TYPE",
  1050: "JS_DATA_VIEW_TYPE",
  1051: "JS_TYPED_ARRAY_TYPE",
  1052: "JS_MAP_TYPE",
  1053: "JS_SET_TYPE",
  1054: "JS_WEAK_MAP_TYPE",
  1055: "JS_WEAK_SET_TYPE",
  1056: "JS_API_OBJECT_TYPE",
  1058: "JS_ARGUMENTS_OBJECT_TYPE",
  1059: "JS_ARRAY_TYPE",
  1060: "JS_ARRAY_BUFFER_TYPE",
  1061: "JS_ARRAY_ITERATOR_TYPE",
  1062: "JS_ASYNC_FROM_SYNC_ITERATOR_TYPE",
  1063: "JS_COLLATOR_TYPE",
  1064: "JS_CONTEXT_EXTENSION_OBJECT_TYPE",
  1065: "JS_DATE_TYPE",
  1066: "JS_DATE_TIME_FORMAT_TYPE",
  1067: "JS_DISPLAY_NAMES_TYPE",
  1068: "JS_ERROR_TYPE",
  1069: "JS_FINALIZATION_GROUP_TYPE",
  1070: "JS_FINALIZATION_GROUP_CLEANUP_ITERATOR_TYPE",
  1071: "JS_LIST_FORMAT_TYPE",
  1072: "JS_LOCALE_TYPE",
  1073: "JS_MESSAGE_OBJECT_TYPE",
  1074: "JS_NUMBER_FORMAT_TYPE",
  1075: "JS_PLURAL_RULES_TYPE",
  1076: "JS_PROMISE_TYPE",
  1077: "JS_REG_EXP_TYPE",
  1078: "JS_REG_EXP_STRING_ITERATOR_TYPE",
  1079: "JS_RELATIVE_TIME_FORMAT_TYPE",
  1080: "JS_SEGMENT_ITERATOR_TYPE",
  1081: "JS_SEGMENTER_TYPE",
  1082: "JS_STRING_ITERATOR_TYPE",
  1083: "JS_V8_BREAK_ITERATOR_TYPE",
  1084: "JS_WEAK_REF_TYPE",
  1085: "WASM_EXCEPTION_OBJECT_TYPE",
  1086: "WASM_GLOBAL_OBJECT_TYPE",
  1087: "WASM_INSTANCE_OBJECT_TYPE",
  1088: "WASM_MEMORY_OBJECT_TYPE",
  1089: "WASM_MODULE_OBJECT_TYPE",
  1090: "WASM_TABLE_OBJECT_TYPE",
  1091: "JS_BOUND_FUNCTION_TYPE",
  1092: "JS_FUNCTION_TYPE",
}

# List of known V8 maps.
KNOWN_MAPS = {
  ("read_only_space", 0x00121): (163, "FreeSpaceMap"),
  ("read_only_space", 0x00149): (164, "MetaMap"),
  ("read_only_space", 0x0018d): (67, "NullMap"),
  ("read_only_space", 0x001c5): (158, "DescriptorArrayMap"),
  ("read_only_space", 0x001f5): (153, "WeakFixedArrayMap"),
  ("read_only_space", 0x0021d): (162, "OnePointerFillerMap"),
  ("read_only_space", 0x00245): (162, "TwoPointerFillerMap"),
  ("read_only_space", 0x00289): (67, "UninitializedMap"),
  ("read_only_space", 0x002cd): (8, "OneByteInternalizedStringMap"),
  ("read_only_space", 0x00329): (67, "UndefinedMap"),
  ("read_only_space", 0x0035d): (66, "HeapNumberMap"),
  ("read_only_space", 0x003a1): (67, "TheHoleMap"),
  ("read_only_space", 0x00401): (67, "BooleanMap"),
  ("read_only_space", 0x00489): (135, "ByteArrayMap"),
  ("read_only_space", 0x004b1): (120, "FixedArrayMap"),
  ("read_only_space", 0x004d9): (120, "FixedCOWArrayMap"),
  ("read_only_space", 0x00501): (121, "HashTableMap"),
  ("read_only_space", 0x00529): (64, "SymbolMap"),
  ("read_only_space", 0x00551): (40, "OneByteStringMap"),
  ("read_only_space", 0x00579): (133, "ScopeInfoMap"),
  ("read_only_space", 0x005a1): (168, "SharedFunctionInfoMap"),
  ("read_only_space", 0x005c9): (156, "CodeMap"),
  ("read_only_space", 0x005f1): (155, "CellMap"),
  ("read_only_space", 0x00619): (167, "GlobalPropertyCellMap"),
  ("read_only_space", 0x00641): (70, "ForeignMap"),
  ("read_only_space", 0x00669): (154, "TransitionArrayMap"),
  ("read_only_space", 0x00691): (45, "ThinOneByteStringMap"),
  ("read_only_space", 0x006b9): (161, "FeedbackVectorMap"),
  ("read_only_space", 0x0070d): (67, "ArgumentsMarkerMap"),
  ("read_only_space", 0x0076d): (67, "ExceptionMap"),
  ("read_only_space", 0x007c9): (67, "TerminationExceptionMap"),
  ("read_only_space", 0x00831): (67, "OptimizedOutMap"),
  ("read_only_space", 0x00891): (67, "StaleRegisterMap"),
  ("read_only_space", 0x008d5): (134, "ScriptContextTableMap"),
  ("read_only_space", 0x008fd): (131, "ClosureFeedbackCellArrayMap"),
  ("read_only_space", 0x00925): (160, "FeedbackMetadataArrayMap"),
  ("read_only_space", 0x0094d): (120, "ArrayListMap"),
  ("read_only_space", 0x00975): (65, "BigIntMap"),
  ("read_only_space", 0x0099d): (132, "ObjectBoilerplateDescriptionMap"),
  ("read_only_space", 0x009c5): (136, "BytecodeArrayMap"),
  ("read_only_space", 0x009ed): (157, "CodeDataContainerMap"),
  ("read_only_space", 0x00a15): (137, "FixedDoubleArrayMap"),
  ("read_only_space", 0x00a3d): (123, "GlobalDictionaryMap"),
  ("read_only_space", 0x00a65): (96, "ManyClosuresCellMap"),
  ("read_only_space", 0x00a8d): (120, "ModuleInfoMap"),
  ("read_only_space", 0x00ab5): (124, "NameDictionaryMap"),
  ("read_only_space", 0x00add): (96, "NoClosuresCellMap"),
  ("read_only_space", 0x00b05): (125, "NumberDictionaryMap"),
  ("read_only_space", 0x00b2d): (96, "OneClosureCellMap"),
  ("read_only_space", 0x00b55): (126, "OrderedHashMapMap"),
  ("read_only_space", 0x00b7d): (127, "OrderedHashSetMap"),
  ("read_only_space", 0x00ba5): (128, "OrderedNameDictionaryMap"),
  ("read_only_space", 0x00bcd): (165, "PreparseDataMap"),
  ("read_only_space", 0x00bf5): (166, "PropertyArrayMap"),
  ("read_only_space", 0x00c1d): (92, "SideEffectCallHandlerInfoMap"),
  ("read_only_space", 0x00c45): (92, "SideEffectFreeCallHandlerInfoMap"),
  ("read_only_space", 0x00c6d): (92, "NextCallSideEffectFreeCallHandlerInfoMap"),
  ("read_only_space", 0x00c95): (129, "SimpleNumberDictionaryMap"),
  ("read_only_space", 0x00cbd): (120, "SloppyArgumentsElementsMap"),
  ("read_only_space", 0x00ce5): (148, "SmallOrderedHashMapMap"),
  ("read_only_space", 0x00d0d): (149, "SmallOrderedHashSetMap"),
  ("read_only_space", 0x00d35): (150, "SmallOrderedNameDictionaryMap"),
  ("read_only_space", 0x00d5d): (68, "SourceTextModuleMap"),
  ("read_only_space", 0x00d85): (130, "StringTableMap"),
  ("read_only_space", 0x00dad): (69, "SyntheticModuleMap"),
  ("read_only_space", 0x00dd5): (152, "UncompiledDataWithoutPreparseDataMap"),
  ("read_only_space", 0x00dfd): (151, "UncompiledDataWithPreparseDataMap"),
  ("read_only_space", 0x00e25): (169, "WeakArrayListMap"),
  ("read_only_space", 0x00e4d): (122, "EphemeronHashTableMap"),
  ("read_only_space", 0x00e75): (159, "EmbedderDataArrayMap"),
  ("read_only_space", 0x00e9d): (170, "WeakCellMap"),
  ("read_only_space", 0x00ec5): (32, "StringMap"),
  ("read_only_space", 0x00eed): (41, "ConsOneByteStringMap"),
  ("read_only_space", 0x00f15): (33, "ConsStringMap"),
  ("read_only_space", 0x00f3d): (37, "ThinStringMap"),
  ("read_only_space", 0x00f65): (35, "SlicedStringMap"),
  ("read_only_space", 0x00f8d): (43, "SlicedOneByteStringMap"),
  ("read_only_space", 0x00fb5): (34, "ExternalStringMap"),
  ("read_only_space", 0x00fdd): (42, "ExternalOneByteStringMap"),
  ("read_only_space", 0x01005): (50, "UncachedExternalStringMap"),
  ("read_only_space", 0x0102d): (0, "InternalizedStringMap"),
  ("read_only_space", 0x01055): (2, "ExternalInternalizedStringMap"),
  ("read_only_space", 0x0107d): (10, "ExternalOneByteInternalizedStringMap"),
  ("read_only_space", 0x010a5): (18, "UncachedExternalInternalizedStringMap"),
  ("read_only_space", 0x010cd): (26, "UncachedExternalOneByteInternalizedStringMap"),
  ("read_only_space", 0x010f5): (58, "UncachedExternalOneByteStringMap"),
  ("read_only_space", 0x0111d): (67, "SelfReferenceMarkerMap"),
  ("read_only_space", 0x01151): (95, "EnumCacheMap"),
  ("read_only_space", 0x011a1): (86, "ArrayBoilerplateDescriptionMap"),
  ("read_only_space", 0x0129d): (98, "InterceptorInfoMap"),
  ("read_only_space", 0x03299): (71, "PromiseFulfillReactionJobTaskMap"),
  ("read_only_space", 0x032c1): (72, "PromiseRejectReactionJobTaskMap"),
  ("read_only_space", 0x032e9): (73, "CallableTaskMap"),
  ("read_only_space", 0x03311): (74, "CallbackTaskMap"),
  ("read_only_space", 0x03339): (75, "PromiseResolveThenableJobTaskMap"),
  ("read_only_space", 0x03361): (78, "FunctionTemplateInfoMap"),
  ("read_only_space", 0x03389): (79, "ObjectTemplateInfoMap"),
  ("read_only_space", 0x033b1): (80, "AccessCheckInfoMap"),
  ("read_only_space", 0x033d9): (81, "AccessorInfoMap"),
  ("read_only_space", 0x03401): (82, "AccessorPairMap"),
  ("read_only_space", 0x03429): (83, "AliasedArgumentsEntryMap"),
  ("read_only_space", 0x03451): (84, "AllocationMementoMap"),
  ("read_only_space", 0x03479): (87, "AsmWasmDataMap"),
  ("read_only_space", 0x034a1): (88, "AsyncGeneratorRequestMap"),
  ("read_only_space", 0x034c9): (89, "BreakPointMap"),
  ("read_only_space", 0x034f1): (90, "BreakPointInfoMap"),
  ("read_only_space", 0x03519): (91, "CachedTemplateObjectMap"),
  ("read_only_space", 0x03541): (93, "ClassPositionsMap"),
  ("read_only_space", 0x03569): (94, "DebugInfoMap"),
  ("read_only_space", 0x03591): (97, "FunctionTemplateRareDataMap"),
  ("read_only_space", 0x035b9): (100, "InterpreterDataMap"),
  ("read_only_space", 0x035e1): (101, "PromiseCapabilityMap"),
  ("read_only_space", 0x03609): (102, "PromiseReactionMap"),
  ("read_only_space", 0x03631): (103, "PropertyDescriptorObjectMap"),
  ("read_only_space", 0x03659): (104, "PrototypeInfoMap"),
  ("read_only_space", 0x03681): (105, "ScriptMap"),
  ("read_only_space", 0x036a9): (109, "SourceTextModuleInfoEntryMap"),
  ("read_only_space", 0x036d1): (110, "StackFrameInfoMap"),
  ("read_only_space", 0x036f9): (111, "StackTraceFrameMap"),
  ("read_only_space", 0x03721): (112, "TemplateObjectDescriptionMap"),
  ("read_only_space", 0x03749): (113, "Tuple2Map"),
  ("read_only_space", 0x03771): (114, "WasmCapiFunctionDataMap"),
  ("read_only_space", 0x03799): (115, "WasmDebugInfoMap"),
  ("read_only_space", 0x037c1): (116, "WasmExceptionTagMap"),
  ("read_only_space", 0x037e9): (117, "WasmExportedFunctionDataMap"),
  ("read_only_space", 0x03811): (118, "WasmIndirectFunctionTableMap"),
  ("read_only_space", 0x03839): (119, "WasmJSFunctionDataMap"),
  ("read_only_space", 0x03861): (99, "InternalClassMap"),
  ("read_only_space", 0x03889): (107, "SmiPairMap"),
  ("read_only_space", 0x038b1): (106, "SmiBoxMap"),
  ("read_only_space", 0x038d9): (108, "SortStateMap"),
  ("read_only_space", 0x03901): (85, "AllocationSiteWithWeakNextMap"),
  ("read_only_space", 0x03929): (85, "AllocationSiteWithoutWeakNextMap"),
  ("read_only_space", 0x03951): (76, "LoadHandler1Map"),
  ("read_only_space", 0x03979): (76, "LoadHandler2Map"),
  ("read_only_space", 0x039a1): (76, "LoadHandler3Map"),
  ("read_only_space", 0x039c9): (77, "StoreHandler0Map"),
  ("read_only_space", 0x039f1): (77, "StoreHandler1Map"),
  ("read_only_space", 0x03a19): (77, "StoreHandler2Map"),
  ("read_only_space", 0x03a41): (77, "StoreHandler3Map"),
  ("map_space", 0x00121): (1057, "ExternalMap"),
  ("map_space", 0x00149): (1073, "JSMessageObjectMap"),
}

# List of known V8 objects.
KNOWN_OBJECTS = {
  ("read_only_space", 0x00171): "NullValue",
  ("read_only_space", 0x001b5): "EmptyDescriptorArray",
  ("read_only_space", 0x001ed): "EmptyWeakFixedArray",
  ("read_only_space", 0x0026d): "UninitializedValue",
  ("read_only_space", 0x0030d): "UndefinedValue",
  ("read_only_space", 0x00351): "NanValue",
  ("read_only_space", 0x00385): "TheHoleValue",
  ("read_only_space", 0x003d9): "HoleNanValue",
  ("read_only_space", 0x003e5): "TrueValue",
  ("read_only_space", 0x0044d): "FalseValue",
  ("read_only_space", 0x0047d): "empty_string",
  ("read_only_space", 0x006e1): "EmptyScopeInfo",
  ("read_only_space", 0x006e9): "EmptyFixedArray",
  ("read_only_space", 0x006f1): "ArgumentsMarker",
  ("read_only_space", 0x00751): "Exception",
  ("read_only_space", 0x007ad): "TerminationException",
  ("read_only_space", 0x00815): "OptimizedOut",
  ("read_only_space", 0x00875): "StaleRegister",
  ("read_only_space", 0x01145): "EmptyEnumCache",
  ("read_only_space", 0x01179): "EmptyPropertyArray",
  ("read_only_space", 0x01181): "EmptyByteArray",
  ("read_only_space", 0x01189): "EmptyObjectBoilerplateDescription",
  ("read_only_space", 0x01195): "EmptyArrayBoilerplateDescription",
  ("read_only_space", 0x011c9): "EmptyClosureFeedbackCellArray",
  ("read_only_space", 0x011d1): "EmptySloppyArgumentsElements",
  ("read_only_space", 0x011e1): "EmptySlowElementDictionary",
  ("read_only_space", 0x01205): "EmptyOrderedHashMap",
  ("read_only_space", 0x01219): "EmptyOrderedHashSet",
  ("read_only_space", 0x0122d): "EmptyFeedbackMetadata",
  ("read_only_space", 0x01239): "EmptyPropertyCell",
  ("read_only_space", 0x0124d): "EmptyPropertyDictionary",
  ("read_only_space", 0x01275): "NoOpInterceptorInfo",
  ("read_only_space", 0x012c5): "EmptyWeakArrayList",
  ("read_only_space", 0x012d1): "InfinityValue",
  ("read_only_space", 0x012dd): "MinusZeroValue",
  ("read_only_space", 0x012e9): "MinusInfinityValue",
  ("read_only_space", 0x012f5): "SelfReferenceMarker",
  ("read_only_space", 0x01335): "OffHeapTrampolineRelocationInfo",
  ("read_only_space", 0x01341): "TrampolineTrivialCodeDataContainer",
  ("read_only_space", 0x0134d): "TrampolinePromiseRejectionCodeDataContainer",
  ("read_only_space", 0x01359): "GlobalThisBindingScopeInfo",
  ("read_only_space", 0x01391): "EmptyFunctionScopeInfo",
  ("read_only_space", 0x013b9): "NativeScopeInfo",
  ("read_only_space", 0x013d5): "HashSeed",
  ("old_space", 0x00121): "ArgumentsIteratorAccessor",
  ("old_space", 0x00165): "ArrayLengthAccessor",
  ("old_space", 0x001a9): "BoundFunctionLengthAccessor",
  ("old_space", 0x001ed): "BoundFunctionNameAccessor",
  ("old_space", 0x00231): "ErrorStackAccessor",
  ("old_space", 0x00275): "FunctionArgumentsAccessor",
  ("old_space", 0x002b9): "FunctionCallerAccessor",
  ("old_space", 0x002fd): "FunctionNameAccessor",
  ("old_space", 0x00341): "FunctionLengthAccessor",
  ("old_space", 0x00385): "FunctionPrototypeAccessor",
  ("old_space", 0x003c9): "RegExpResultIndicesAccessor",
  ("old_space", 0x0040d): "StringLengthAccessor",
  ("old_space", 0x00451): "InvalidPrototypeValidityCell",
  ("old_space", 0x00459): "EmptyScript",
  ("old_space", 0x00499): "ManyClosuresCell",
  ("old_space", 0x004a5): "ArrayConstructorProtector",
  ("old_space", 0x004b9): "NoElementsProtector",
  ("old_space", 0x004cd): "IsConcatSpreadableProtector",
  ("old_space", 0x004e1): "ArraySpeciesProtector",
  ("old_space", 0x004f5): "TypedArraySpeciesProtector",
  ("old_space", 0x00509): "PromiseSpeciesProtector",
  ("old_space", 0x0051d): "StringLengthProtector",
  ("old_space", 0x00531): "ArrayIteratorProtector",
  ("old_space", 0x00545): "ArrayBufferDetachingProtector",
  ("old_space", 0x00559): "PromiseHookProtector",
  ("old_space", 0x0056d): "PromiseResolveProtector",
  ("old_space", 0x00581): "MapIteratorProtector",
  ("old_space", 0x00595): "PromiseThenProtector",
  ("old_space", 0x005a9): "SetIteratorProtector",
  ("old_space", 0x005bd): "StringIteratorProtector",
  ("old_space", 0x005d1): "SingleCharacterStringCache",
  ("old_space", 0x009d9): "StringSplitCache",
  ("old_space", 0x00de1): "RegExpMultipleCache",
  ("old_space", 0x011e9): "BuiltinsConstantsTable",
}

# Lower 32 bits of first page addresses for various heap spaces.
HEAP_FIRST_PAGES = {
  0x08100000: "old_space",
  0x08140000: "map_space",
  0x08040000: "read_only_space",
}

# List of known V8 Frame Markers.
FRAME_MARKERS = (
  "ENTRY",
  "CONSTRUCT_ENTRY",
  "EXIT",
  "OPTIMIZED",
  "WASM_COMPILED",
  "WASM_TO_JS",
  "JS_TO_WASM",
  "WASM_INTERPRETER_ENTRY",
  "C_WASM_ENTRY",
  "WASM_EXIT",
  "WASM_COMPILE_LAZY",
  "INTERPRETED",
  "STUB",
  "BUILTIN_CONTINUATION",
  "JAVA_SCRIPT_BUILTIN_CONTINUATION",
  "JAVA_SCRIPT_BUILTIN_CONTINUATION_WITH_CATCH",
  "INTERNAL",
  "CONSTRUCT",
  "ARGUMENTS_ADAPTOR",
  "BUILTIN",
  "BUILTIN_EXIT",
  "NATIVE",
)

# This set of constants is generated from a shipping build.
