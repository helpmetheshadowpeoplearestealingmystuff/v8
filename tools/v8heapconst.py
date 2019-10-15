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
  80: "TUPLE2_TYPE",
  81: "TUPLE3_TYPE",
  82: "ACCESS_CHECK_INFO_TYPE",
  83: "ACCESSOR_INFO_TYPE",
  84: "ACCESSOR_PAIR_TYPE",
  85: "ALIASED_ARGUMENTS_ENTRY_TYPE",
  86: "ALLOCATION_MEMENTO_TYPE",
  87: "ALLOCATION_SITE_TYPE",
  88: "ARRAY_BOILERPLATE_DESCRIPTION_TYPE",
  89: "ASM_WASM_DATA_TYPE",
  90: "ASYNC_GENERATOR_REQUEST_TYPE",
  91: "CALL_HANDLER_INFO_TYPE",
  92: "CLASS_POSITIONS_TYPE",
  93: "DEBUG_INFO_TYPE",
  94: "ENUM_CACHE_TYPE",
  95: "FEEDBACK_CELL_TYPE",
  96: "FUNCTION_TEMPLATE_RARE_DATA_TYPE",
  97: "INTERCEPTOR_INFO_TYPE",
  98: "INTERNAL_CLASS_TYPE",
  99: "INTERPRETER_DATA_TYPE",
  100: "PROMISE_CAPABILITY_TYPE",
  101: "PROMISE_REACTION_TYPE",
  102: "PROTOTYPE_INFO_TYPE",
  103: "SCRIPT_TYPE",
  104: "SMI_BOX_TYPE",
  105: "SMI_PAIR_TYPE",
  106: "SORT_STATE_TYPE",
  107: "SOURCE_POSITION_TABLE_WITH_FRAME_CACHE_TYPE",
  108: "SOURCE_TEXT_MODULE_INFO_ENTRY_TYPE",
  109: "STACK_FRAME_INFO_TYPE",
  110: "STACK_TRACE_FRAME_TYPE",
  111: "TEMPLATE_OBJECT_DESCRIPTION_TYPE",
  112: "WASM_CAPI_FUNCTION_DATA_TYPE",
  113: "WASM_DEBUG_INFO_TYPE",
  114: "WASM_EXCEPTION_TAG_TYPE",
  115: "WASM_EXPORTED_FUNCTION_DATA_TYPE",
  116: "WASM_INDIRECT_FUNCTION_TABLE_TYPE",
  117: "WASM_JS_FUNCTION_DATA_TYPE",
  118: "FIXED_ARRAY_TYPE",
  119: "HASH_TABLE_TYPE",
  120: "EPHEMERON_HASH_TABLE_TYPE",
  121: "GLOBAL_DICTIONARY_TYPE",
  122: "NAME_DICTIONARY_TYPE",
  123: "NUMBER_DICTIONARY_TYPE",
  124: "ORDERED_HASH_MAP_TYPE",
  125: "ORDERED_HASH_SET_TYPE",
  126: "ORDERED_NAME_DICTIONARY_TYPE",
  127: "SIMPLE_NUMBER_DICTIONARY_TYPE",
  128: "STRING_TABLE_TYPE",
  129: "CLOSURE_FEEDBACK_CELL_ARRAY_TYPE",
  130: "OBJECT_BOILERPLATE_DESCRIPTION_TYPE",
  131: "SCOPE_INFO_TYPE",
  132: "SCRIPT_CONTEXT_TABLE_TYPE",
  133: "BYTE_ARRAY_TYPE",
  134: "BYTECODE_ARRAY_TYPE",
  135: "FIXED_DOUBLE_ARRAY_TYPE",
  136: "AWAIT_CONTEXT_TYPE",
  137: "BLOCK_CONTEXT_TYPE",
  138: "CATCH_CONTEXT_TYPE",
  139: "DEBUG_EVALUATE_CONTEXT_TYPE",
  140: "EVAL_CONTEXT_TYPE",
  141: "FUNCTION_CONTEXT_TYPE",
  142: "MODULE_CONTEXT_TYPE",
  143: "NATIVE_CONTEXT_TYPE",
  144: "SCRIPT_CONTEXT_TYPE",
  145: "WITH_CONTEXT_TYPE",
  146: "SMALL_ORDERED_HASH_MAP_TYPE",
  147: "SMALL_ORDERED_HASH_SET_TYPE",
  148: "SMALL_ORDERED_NAME_DICTIONARY_TYPE",
  149: "UNCOMPILED_DATA_WITH_PREPARSE_DATA_TYPE",
  150: "UNCOMPILED_DATA_WITHOUT_PREPARSE_DATA_TYPE",
  151: "WEAK_FIXED_ARRAY_TYPE",
  152: "TRANSITION_ARRAY_TYPE",
  153: "CELL_TYPE",
  154: "CODE_TYPE",
  155: "CODE_DATA_CONTAINER_TYPE",
  156: "DESCRIPTOR_ARRAY_TYPE",
  157: "EMBEDDER_DATA_ARRAY_TYPE",
  158: "FEEDBACK_METADATA_TYPE",
  159: "FEEDBACK_VECTOR_TYPE",
  160: "FILLER_TYPE",
  161: "FREE_SPACE_TYPE",
  162: "MAP_TYPE",
  163: "PREPARSE_DATA_TYPE",
  164: "PROPERTY_ARRAY_TYPE",
  165: "PROPERTY_CELL_TYPE",
  166: "SHARED_FUNCTION_INFO_TYPE",
  167: "WEAK_ARRAY_LIST_TYPE",
  168: "WEAK_CELL_TYPE",
  169: "JS_PROXY_TYPE",
  1057: "JS_OBJECT_TYPE",
  170: "JS_GLOBAL_OBJECT_TYPE",
  171: "JS_GLOBAL_PROXY_TYPE",
  172: "JS_MODULE_NAMESPACE_TYPE",
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
  1067: "JS_ERROR_TYPE",
  1068: "JS_FINALIZATION_GROUP_TYPE",
  1069: "JS_FINALIZATION_GROUP_CLEANUP_ITERATOR_TYPE",
  1070: "JS_LIST_FORMAT_TYPE",
  1071: "JS_LOCALE_TYPE",
  1072: "JS_MESSAGE_OBJECT_TYPE",
  1073: "JS_NUMBER_FORMAT_TYPE",
  1074: "JS_PLURAL_RULES_TYPE",
  1075: "JS_PROMISE_TYPE",
  1076: "JS_REG_EXP_TYPE",
  1077: "JS_REG_EXP_STRING_ITERATOR_TYPE",
  1078: "JS_RELATIVE_TIME_FORMAT_TYPE",
  1079: "JS_SEGMENT_ITERATOR_TYPE",
  1080: "JS_SEGMENTER_TYPE",
  1081: "JS_STRING_ITERATOR_TYPE",
  1082: "JS_V8_BREAK_ITERATOR_TYPE",
  1083: "JS_WEAK_REF_TYPE",
  1084: "WASM_EXCEPTION_OBJECT_TYPE",
  1085: "WASM_GLOBAL_OBJECT_TYPE",
  1086: "WASM_INSTANCE_OBJECT_TYPE",
  1087: "WASM_MEMORY_OBJECT_TYPE",
  1088: "WASM_MODULE_OBJECT_TYPE",
  1089: "WASM_TABLE_OBJECT_TYPE",
  1090: "JS_BOUND_FUNCTION_TYPE",
  1091: "JS_FUNCTION_TYPE",
}

# List of known V8 maps.
KNOWN_MAPS = {
  ("read_only_space", 0x00121): (161, "FreeSpaceMap"),
  ("read_only_space", 0x00171): (162, "MetaMap"),
  ("read_only_space", 0x001f1): (67, "NullMap"),
  ("read_only_space", 0x00259): (156, "DescriptorArrayMap"),
  ("read_only_space", 0x002b9): (151, "WeakFixedArrayMap"),
  ("read_only_space", 0x00309): (160, "OnePointerFillerMap"),
  ("read_only_space", 0x00359): (160, "TwoPointerFillerMap"),
  ("read_only_space", 0x003d9): (67, "UninitializedMap"),
  ("read_only_space", 0x00449): (8, "OneByteInternalizedStringMap"),
  ("read_only_space", 0x004e9): (67, "UndefinedMap"),
  ("read_only_space", 0x00549): (66, "HeapNumberMap"),
  ("read_only_space", 0x005c9): (67, "TheHoleMap"),
  ("read_only_space", 0x00671): (67, "BooleanMap"),
  ("read_only_space", 0x00749): (133, "ByteArrayMap"),
  ("read_only_space", 0x00799): (118, "FixedArrayMap"),
  ("read_only_space", 0x007e9): (118, "FixedCOWArrayMap"),
  ("read_only_space", 0x00839): (119, "HashTableMap"),
  ("read_only_space", 0x00889): (64, "SymbolMap"),
  ("read_only_space", 0x008d9): (40, "OneByteStringMap"),
  ("read_only_space", 0x00929): (131, "ScopeInfoMap"),
  ("read_only_space", 0x00979): (166, "SharedFunctionInfoMap"),
  ("read_only_space", 0x009c9): (154, "CodeMap"),
  ("read_only_space", 0x00a19): (153, "CellMap"),
  ("read_only_space", 0x00a69): (165, "GlobalPropertyCellMap"),
  ("read_only_space", 0x00ab9): (70, "ForeignMap"),
  ("read_only_space", 0x00b09): (152, "TransitionArrayMap"),
  ("read_only_space", 0x00b59): (45, "ThinOneByteStringMap"),
  ("read_only_space", 0x00ba9): (159, "FeedbackVectorMap"),
  ("read_only_space", 0x00c49): (67, "ArgumentsMarkerMap"),
  ("read_only_space", 0x00ce9): (67, "ExceptionMap"),
  ("read_only_space", 0x00d89): (67, "TerminationExceptionMap"),
  ("read_only_space", 0x00e31): (67, "OptimizedOutMap"),
  ("read_only_space", 0x00ed1): (67, "StaleRegisterMap"),
  ("read_only_space", 0x00f41): (132, "ScriptContextTableMap"),
  ("read_only_space", 0x00f91): (129, "ClosureFeedbackCellArrayMap"),
  ("read_only_space", 0x00fe1): (158, "FeedbackMetadataArrayMap"),
  ("read_only_space", 0x01031): (118, "ArrayListMap"),
  ("read_only_space", 0x01081): (65, "BigIntMap"),
  ("read_only_space", 0x010d1): (130, "ObjectBoilerplateDescriptionMap"),
  ("read_only_space", 0x01121): (134, "BytecodeArrayMap"),
  ("read_only_space", 0x01171): (155, "CodeDataContainerMap"),
  ("read_only_space", 0x011c1): (135, "FixedDoubleArrayMap"),
  ("read_only_space", 0x01211): (121, "GlobalDictionaryMap"),
  ("read_only_space", 0x01261): (95, "ManyClosuresCellMap"),
  ("read_only_space", 0x012b1): (118, "ModuleInfoMap"),
  ("read_only_space", 0x01301): (122, "NameDictionaryMap"),
  ("read_only_space", 0x01351): (95, "NoClosuresCellMap"),
  ("read_only_space", 0x013a1): (123, "NumberDictionaryMap"),
  ("read_only_space", 0x013f1): (95, "OneClosureCellMap"),
  ("read_only_space", 0x01441): (124, "OrderedHashMapMap"),
  ("read_only_space", 0x01491): (125, "OrderedHashSetMap"),
  ("read_only_space", 0x014e1): (126, "OrderedNameDictionaryMap"),
  ("read_only_space", 0x01531): (163, "PreparseDataMap"),
  ("read_only_space", 0x01581): (164, "PropertyArrayMap"),
  ("read_only_space", 0x015d1): (91, "SideEffectCallHandlerInfoMap"),
  ("read_only_space", 0x01621): (91, "SideEffectFreeCallHandlerInfoMap"),
  ("read_only_space", 0x01671): (91, "NextCallSideEffectFreeCallHandlerInfoMap"),
  ("read_only_space", 0x016c1): (127, "SimpleNumberDictionaryMap"),
  ("read_only_space", 0x01711): (118, "SloppyArgumentsElementsMap"),
  ("read_only_space", 0x01761): (146, "SmallOrderedHashMapMap"),
  ("read_only_space", 0x017b1): (147, "SmallOrderedHashSetMap"),
  ("read_only_space", 0x01801): (148, "SmallOrderedNameDictionaryMap"),
  ("read_only_space", 0x01851): (68, "SourceTextModuleMap"),
  ("read_only_space", 0x018a1): (128, "StringTableMap"),
  ("read_only_space", 0x018f1): (69, "SyntheticModuleMap"),
  ("read_only_space", 0x01941): (150, "UncompiledDataWithoutPreparseDataMap"),
  ("read_only_space", 0x01991): (149, "UncompiledDataWithPreparseDataMap"),
  ("read_only_space", 0x019e1): (167, "WeakArrayListMap"),
  ("read_only_space", 0x01a31): (120, "EphemeronHashTableMap"),
  ("read_only_space", 0x01a81): (157, "EmbedderDataArrayMap"),
  ("read_only_space", 0x01ad1): (168, "WeakCellMap"),
  ("read_only_space", 0x01b21): (58, "NativeSourceStringMap"),
  ("read_only_space", 0x01b71): (32, "StringMap"),
  ("read_only_space", 0x01bc1): (41, "ConsOneByteStringMap"),
  ("read_only_space", 0x01c11): (33, "ConsStringMap"),
  ("read_only_space", 0x01c61): (37, "ThinStringMap"),
  ("read_only_space", 0x01cb1): (35, "SlicedStringMap"),
  ("read_only_space", 0x01d01): (43, "SlicedOneByteStringMap"),
  ("read_only_space", 0x01d51): (34, "ExternalStringMap"),
  ("read_only_space", 0x01da1): (42, "ExternalOneByteStringMap"),
  ("read_only_space", 0x01df1): (50, "UncachedExternalStringMap"),
  ("read_only_space", 0x01e41): (0, "InternalizedStringMap"),
  ("read_only_space", 0x01e91): (2, "ExternalInternalizedStringMap"),
  ("read_only_space", 0x01ee1): (10, "ExternalOneByteInternalizedStringMap"),
  ("read_only_space", 0x01f31): (18, "UncachedExternalInternalizedStringMap"),
  ("read_only_space", 0x01f81): (26, "UncachedExternalOneByteInternalizedStringMap"),
  ("read_only_space", 0x01fd1): (58, "UncachedExternalOneByteStringMap"),
  ("read_only_space", 0x02021): (67, "SelfReferenceMarkerMap"),
  ("read_only_space", 0x02089): (94, "EnumCacheMap"),
  ("read_only_space", 0x02129): (88, "ArrayBoilerplateDescriptionMap"),
  ("read_only_space", 0x02319): (97, "InterceptorInfoMap"),
  ("read_only_space", 0x04c59): (71, "PromiseFulfillReactionJobTaskMap"),
  ("read_only_space", 0x04ca9): (72, "PromiseRejectReactionJobTaskMap"),
  ("read_only_space", 0x04cf9): (73, "CallableTaskMap"),
  ("read_only_space", 0x04d49): (74, "CallbackTaskMap"),
  ("read_only_space", 0x04d99): (75, "PromiseResolveThenableJobTaskMap"),
  ("read_only_space", 0x04de9): (78, "FunctionTemplateInfoMap"),
  ("read_only_space", 0x04e39): (79, "ObjectTemplateInfoMap"),
  ("read_only_space", 0x04e89): (80, "Tuple2Map"),
  ("read_only_space", 0x04ed9): (81, "Tuple3Map"),
  ("read_only_space", 0x04f29): (82, "AccessCheckInfoMap"),
  ("read_only_space", 0x04f79): (83, "AccessorInfoMap"),
  ("read_only_space", 0x04fc9): (84, "AccessorPairMap"),
  ("read_only_space", 0x05019): (85, "AliasedArgumentsEntryMap"),
  ("read_only_space", 0x05069): (86, "AllocationMementoMap"),
  ("read_only_space", 0x050b9): (89, "AsmWasmDataMap"),
  ("read_only_space", 0x05109): (90, "AsyncGeneratorRequestMap"),
  ("read_only_space", 0x05159): (92, "ClassPositionsMap"),
  ("read_only_space", 0x051a9): (93, "DebugInfoMap"),
  ("read_only_space", 0x051f9): (96, "FunctionTemplateRareDataMap"),
  ("read_only_space", 0x05249): (99, "InterpreterDataMap"),
  ("read_only_space", 0x05299): (100, "PromiseCapabilityMap"),
  ("read_only_space", 0x052e9): (101, "PromiseReactionMap"),
  ("read_only_space", 0x05339): (102, "PrototypeInfoMap"),
  ("read_only_space", 0x05389): (103, "ScriptMap"),
  ("read_only_space", 0x053d9): (107, "SourcePositionTableWithFrameCacheMap"),
  ("read_only_space", 0x05429): (108, "SourceTextModuleInfoEntryMap"),
  ("read_only_space", 0x05479): (109, "StackFrameInfoMap"),
  ("read_only_space", 0x054c9): (110, "StackTraceFrameMap"),
  ("read_only_space", 0x05519): (111, "TemplateObjectDescriptionMap"),
  ("read_only_space", 0x05569): (112, "WasmCapiFunctionDataMap"),
  ("read_only_space", 0x055b9): (113, "WasmDebugInfoMap"),
  ("read_only_space", 0x05609): (114, "WasmExceptionTagMap"),
  ("read_only_space", 0x05659): (115, "WasmExportedFunctionDataMap"),
  ("read_only_space", 0x056a9): (116, "WasmIndirectFunctionTableMap"),
  ("read_only_space", 0x056f9): (117, "WasmJSFunctionDataMap"),
  ("read_only_space", 0x05749): (98, "InternalClassMap"),
  ("read_only_space", 0x05799): (105, "SmiPairMap"),
  ("read_only_space", 0x057e9): (104, "SmiBoxMap"),
  ("read_only_space", 0x05839): (106, "SortStateMap"),
  ("read_only_space", 0x05889): (87, "AllocationSiteWithWeakNextMap"),
  ("read_only_space", 0x058d9): (87, "AllocationSiteWithoutWeakNextMap"),
  ("read_only_space", 0x05929): (76, "LoadHandler1Map"),
  ("read_only_space", 0x05979): (76, "LoadHandler2Map"),
  ("read_only_space", 0x059c9): (76, "LoadHandler3Map"),
  ("read_only_space", 0x05a19): (77, "StoreHandler0Map"),
  ("read_only_space", 0x05a69): (77, "StoreHandler1Map"),
  ("read_only_space", 0x05ab9): (77, "StoreHandler2Map"),
  ("read_only_space", 0x05b09): (77, "StoreHandler3Map"),
  ("map_space", 0x00121): (1057, "ExternalMap"),
  ("map_space", 0x00171): (1072, "JSMessageObjectMap"),
}

# List of known V8 objects.
KNOWN_OBJECTS = {
  ("read_only_space", 0x001c1): "NullValue",
  ("read_only_space", 0x00241): "EmptyDescriptorArray",
  ("read_only_space", 0x002a9): "EmptyWeakFixedArray",
  ("read_only_space", 0x003a9): "UninitializedValue",
  ("read_only_space", 0x004b9): "UndefinedValue",
  ("read_only_space", 0x00539): "NanValue",
  ("read_only_space", 0x00599): "TheHoleValue",
  ("read_only_space", 0x00631): "HoleNanValue",
  ("read_only_space", 0x00641): "TrueValue",
  ("read_only_space", 0x006f1): "FalseValue",
  ("read_only_space", 0x00739): "empty_string",
  ("read_only_space", 0x00bf9): "EmptyScopeInfo",
  ("read_only_space", 0x00c09): "EmptyFixedArray",
  ("read_only_space", 0x00c19): "ArgumentsMarker",
  ("read_only_space", 0x00cb9): "Exception",
  ("read_only_space", 0x00d59): "TerminationException",
  ("read_only_space", 0x00e01): "OptimizedOut",
  ("read_only_space", 0x00ea1): "StaleRegister",
  ("read_only_space", 0x02071): "EmptyEnumCache",
  ("read_only_space", 0x020d9): "EmptyPropertyArray",
  ("read_only_space", 0x020e9): "EmptyByteArray",
  ("read_only_space", 0x020f9): "EmptyObjectBoilerplateDescription",
  ("read_only_space", 0x02111): "EmptyArrayBoilerplateDescription",
  ("read_only_space", 0x02179): "EmptyClosureFeedbackCellArray",
  ("read_only_space", 0x02189): "EmptySloppyArgumentsElements",
  ("read_only_space", 0x021a9): "EmptySlowElementDictionary",
  ("read_only_space", 0x021f1): "EmptyOrderedHashMap",
  ("read_only_space", 0x02219): "EmptyOrderedHashSet",
  ("read_only_space", 0x02241): "EmptyFeedbackMetadata",
  ("read_only_space", 0x02251): "EmptyPropertyCell",
  ("read_only_space", 0x02279): "EmptyPropertyDictionary",
  ("read_only_space", 0x022c9): "NoOpInterceptorInfo",
  ("read_only_space", 0x02369): "EmptyWeakArrayList",
  ("read_only_space", 0x02381): "InfinityValue",
  ("read_only_space", 0x02391): "MinusZeroValue",
  ("read_only_space", 0x023a1): "MinusInfinityValue",
  ("read_only_space", 0x023b1): "SelfReferenceMarker",
  ("read_only_space", 0x02409): "OffHeapTrampolineRelocationInfo",
  ("read_only_space", 0x02421): "TrampolineTrivialCodeDataContainer",
  ("read_only_space", 0x02439): "TrampolinePromiseRejectionCodeDataContainer",
  ("read_only_space", 0x02451): "GlobalThisBindingScopeInfo",
  ("read_only_space", 0x024b9): "EmptyFunctionScopeInfo",
  ("read_only_space", 0x02509): "HashSeed",
  ("old_space", 0x00121): "ArgumentsIteratorAccessor",
  ("old_space", 0x00191): "ArrayLengthAccessor",
  ("old_space", 0x00201): "BoundFunctionLengthAccessor",
  ("old_space", 0x00271): "BoundFunctionNameAccessor",
  ("old_space", 0x002e1): "ErrorStackAccessor",
  ("old_space", 0x00351): "FunctionArgumentsAccessor",
  ("old_space", 0x003c1): "FunctionCallerAccessor",
  ("old_space", 0x00431): "FunctionNameAccessor",
  ("old_space", 0x004a1): "FunctionLengthAccessor",
  ("old_space", 0x00511): "FunctionPrototypeAccessor",
  ("old_space", 0x00581): "RegExpResultIndicesAccessor",
  ("old_space", 0x005f1): "StringLengthAccessor",
  ("old_space", 0x00661): "InvalidPrototypeValidityCell",
  ("old_space", 0x00671): "EmptyScript",
  ("old_space", 0x006f1): "ManyClosuresCell",
  ("old_space", 0x00709): "ArrayConstructorProtector",
  ("old_space", 0x00731): "NoElementsProtector",
  ("old_space", 0x00759): "IsConcatSpreadableProtector",
  ("old_space", 0x00781): "ArraySpeciesProtector",
  ("old_space", 0x007a9): "TypedArraySpeciesProtector",
  ("old_space", 0x007d1): "PromiseSpeciesProtector",
  ("old_space", 0x007f9): "StringLengthProtector",
  ("old_space", 0x00821): "ArrayIteratorProtector",
  ("old_space", 0x00849): "ArrayBufferDetachingProtector",
  ("old_space", 0x00871): "PromiseHookProtector",
  ("old_space", 0x00899): "PromiseResolveProtector",
  ("old_space", 0x008c1): "MapIteratorProtector",
  ("old_space", 0x008e9): "PromiseThenProtector",
  ("old_space", 0x00911): "SetIteratorProtector",
  ("old_space", 0x00939): "StringIteratorProtector",
  ("old_space", 0x00961): "SingleCharacterStringCache",
  ("old_space", 0x01171): "StringSplitCache",
  ("old_space", 0x01981): "RegExpMultipleCache",
  ("old_space", 0x02191): "BuiltinsConstantsTable",
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
