#!/usr/bin/env python3
# Copyright 2019 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

# This file is automatically generated by mkgrokdump and should not
# be modified manually.

# List of known V8 instance types.
# yapf: disable

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
  50: "UNCACHED_EXTERNAL_STRING_TYPE",
  58: "UNCACHED_EXTERNAL_ONE_BYTE_STRING_TYPE",
  96: "SHARED_STRING_TYPE",
  98: "SHARED_EXTERNAL_STRING_TYPE",
  104: "SHARED_ONE_BYTE_STRING_TYPE",
  106: "SHARED_EXTERNAL_ONE_BYTE_STRING_TYPE",
  114: "SHARED_UNCACHED_EXTERNAL_STRING_TYPE",
  122: "SHARED_UNCACHED_EXTERNAL_ONE_BYTE_STRING_TYPE",
  128: "SYMBOL_TYPE",
  129: "BIG_INT_BASE_TYPE",
  130: "HEAP_NUMBER_TYPE",
  131: "ODDBALL_TYPE",
  132: "PROMISE_FULFILL_REACTION_JOB_TASK_TYPE",
  133: "PROMISE_REJECT_REACTION_JOB_TASK_TYPE",
  134: "CALLABLE_TASK_TYPE",
  135: "CALLBACK_TASK_TYPE",
  136: "PROMISE_RESOLVE_THENABLE_JOB_TASK_TYPE",
  137: "LOAD_HANDLER_TYPE",
  138: "STORE_HANDLER_TYPE",
  139: "FUNCTION_TEMPLATE_INFO_TYPE",
  140: "OBJECT_TEMPLATE_INFO_TYPE",
  141: "ACCESS_CHECK_INFO_TYPE",
  142: "ACCESSOR_PAIR_TYPE",
  143: "ALIASED_ARGUMENTS_ENTRY_TYPE",
  144: "ALLOCATION_MEMENTO_TYPE",
  145: "ALLOCATION_SITE_TYPE",
  146: "ARRAY_BOILERPLATE_DESCRIPTION_TYPE",
  147: "ASM_WASM_DATA_TYPE",
  148: "ASYNC_GENERATOR_REQUEST_TYPE",
  149: "BREAK_POINT_TYPE",
  150: "BREAK_POINT_INFO_TYPE",
  151: "CALL_SITE_INFO_TYPE",
  152: "CLASS_POSITIONS_TYPE",
  153: "DEBUG_INFO_TYPE",
  154: "ENUM_CACHE_TYPE",
  155: "ERROR_STACK_DATA_TYPE",
  156: "FEEDBACK_CELL_TYPE",
  157: "FUNCTION_TEMPLATE_RARE_DATA_TYPE",
  158: "INTERCEPTOR_INFO_TYPE",
  159: "INTERPRETER_DATA_TYPE",
  160: "MODULE_REQUEST_TYPE",
  161: "PROMISE_CAPABILITY_TYPE",
  162: "PROMISE_ON_STACK_TYPE",
  163: "PROMISE_REACTION_TYPE",
  164: "PROPERTY_DESCRIPTOR_OBJECT_TYPE",
  165: "PROTOTYPE_INFO_TYPE",
  166: "REG_EXP_BOILERPLATE_DESCRIPTION_TYPE",
  167: "SCRIPT_TYPE",
  168: "SCRIPT_OR_MODULE_TYPE",
  169: "SOURCE_TEXT_MODULE_INFO_ENTRY_TYPE",
  170: "STACK_FRAME_INFO_TYPE",
  171: "TEMPLATE_OBJECT_DESCRIPTION_TYPE",
  172: "TUPLE2_TYPE",
  173: "WASM_EXCEPTION_TAG_TYPE",
  174: "WASM_INDIRECT_FUNCTION_TABLE_TYPE",
  175: "FIXED_ARRAY_TYPE",
  176: "HASH_TABLE_TYPE",
  177: "EPHEMERON_HASH_TABLE_TYPE",
  178: "GLOBAL_DICTIONARY_TYPE",
  179: "NAME_DICTIONARY_TYPE",
  180: "NAME_TO_INDEX_HASH_TABLE_TYPE",
  181: "NUMBER_DICTIONARY_TYPE",
  182: "ORDERED_HASH_MAP_TYPE",
  183: "ORDERED_HASH_SET_TYPE",
  184: "ORDERED_NAME_DICTIONARY_TYPE",
  185: "REGISTERED_SYMBOL_TABLE_TYPE",
  186: "SIMPLE_NUMBER_DICTIONARY_TYPE",
  187: "CLOSURE_FEEDBACK_CELL_ARRAY_TYPE",
  188: "OBJECT_BOILERPLATE_DESCRIPTION_TYPE",
  189: "SCRIPT_CONTEXT_TABLE_TYPE",
  190: "BYTE_ARRAY_TYPE",
  191: "BYTECODE_ARRAY_TYPE",
  192: "FIXED_DOUBLE_ARRAY_TYPE",
  193: "INTERNAL_CLASS_WITH_SMI_ELEMENTS_TYPE",
  194: "SLOPPY_ARGUMENTS_ELEMENTS_TYPE",
  195: "TURBOSHAFT_FLOAT64_TYPE_TYPE",
  196: "TURBOSHAFT_FLOAT64_RANGE_TYPE_TYPE",
  197: "TURBOSHAFT_FLOAT64_SET_TYPE_TYPE",
  198: "TURBOSHAFT_WORD32_TYPE_TYPE",
  199: "TURBOSHAFT_WORD32_RANGE_TYPE_TYPE",
  200: "TURBOSHAFT_WORD32_SET_TYPE_TYPE",
  201: "TURBOSHAFT_WORD64_TYPE_TYPE",
  202: "TURBOSHAFT_WORD64_RANGE_TYPE_TYPE",
  203: "TURBOSHAFT_WORD64_SET_TYPE_TYPE",
  204: "FOREIGN_TYPE",
  205: "AWAIT_CONTEXT_TYPE",
  206: "BLOCK_CONTEXT_TYPE",
  207: "CATCH_CONTEXT_TYPE",
  208: "DEBUG_EVALUATE_CONTEXT_TYPE",
  209: "EVAL_CONTEXT_TYPE",
  210: "FUNCTION_CONTEXT_TYPE",
  211: "MODULE_CONTEXT_TYPE",
  212: "NATIVE_CONTEXT_TYPE",
  213: "SCRIPT_CONTEXT_TYPE",
  214: "WITH_CONTEXT_TYPE",
  215: "TURBOFAN_BITSET_TYPE_TYPE",
  216: "TURBOFAN_HEAP_CONSTANT_TYPE_TYPE",
  217: "TURBOFAN_OTHER_NUMBER_CONSTANT_TYPE_TYPE",
  218: "TURBOFAN_RANGE_TYPE_TYPE",
  219: "TURBOFAN_UNION_TYPE_TYPE",
  220: "UNCOMPILED_DATA_WITH_PREPARSE_DATA_TYPE",
  221: "UNCOMPILED_DATA_WITH_PREPARSE_DATA_AND_JOB_TYPE",
  222: "UNCOMPILED_DATA_WITHOUT_PREPARSE_DATA_TYPE",
  223: "UNCOMPILED_DATA_WITHOUT_PREPARSE_DATA_WITH_JOB_TYPE",
  224: "WASM_FUNCTION_DATA_TYPE",
  225: "WASM_CAPI_FUNCTION_DATA_TYPE",
  226: "WASM_EXPORTED_FUNCTION_DATA_TYPE",
  227: "WASM_JS_FUNCTION_DATA_TYPE",
  228: "EXPORTED_SUB_CLASS_BASE_TYPE",
  229: "EXPORTED_SUB_CLASS_TYPE",
  230: "EXPORTED_SUB_CLASS2_TYPE",
  231: "SMALL_ORDERED_HASH_MAP_TYPE",
  232: "SMALL_ORDERED_HASH_SET_TYPE",
  233: "SMALL_ORDERED_NAME_DICTIONARY_TYPE",
  234: "ABSTRACT_INTERNAL_CLASS_SUBCLASS1_TYPE",
  235: "ABSTRACT_INTERNAL_CLASS_SUBCLASS2_TYPE",
  236: "DESCRIPTOR_ARRAY_TYPE",
  237: "STRONG_DESCRIPTOR_ARRAY_TYPE",
  238: "SOURCE_TEXT_MODULE_TYPE",
  239: "SYNTHETIC_MODULE_TYPE",
  240: "WEAK_FIXED_ARRAY_TYPE",
  241: "TRANSITION_ARRAY_TYPE",
  242: "ACCESSOR_INFO_TYPE",
  243: "CALL_HANDLER_INFO_TYPE",
  244: "CELL_TYPE",
  245: "CODE_TYPE",
  246: "COVERAGE_INFO_TYPE",
  247: "EMBEDDER_DATA_ARRAY_TYPE",
  248: "FEEDBACK_METADATA_TYPE",
  249: "FEEDBACK_VECTOR_TYPE",
  250: "FILLER_TYPE",
  251: "FREE_SPACE_TYPE",
  252: "INSTRUCTION_STREAM_TYPE",
  253: "INTERNAL_CLASS_TYPE",
  254: "INTERNAL_CLASS_WITH_STRUCT_ELEMENTS_TYPE",
  255: "MAP_TYPE",
  256: "MEGA_DOM_HANDLER_TYPE",
  257: "ON_HEAP_BASIC_BLOCK_PROFILER_DATA_TYPE",
  258: "PREPARSE_DATA_TYPE",
  259: "PROPERTY_ARRAY_TYPE",
  260: "PROPERTY_CELL_TYPE",
  261: "SCOPE_INFO_TYPE",
  262: "SHARED_FUNCTION_INFO_TYPE",
  263: "SMI_BOX_TYPE",
  264: "SMI_PAIR_TYPE",
  265: "SORT_STATE_TYPE",
  266: "SWISS_NAME_DICTIONARY_TYPE",
  267: "WASM_API_FUNCTION_REF_TYPE",
  268: "WASM_CONTINUATION_OBJECT_TYPE",
  269: "WASM_INTERNAL_FUNCTION_TYPE",
  270: "WASM_NULL_TYPE",
  271: "WASM_RESUME_DATA_TYPE",
  272: "WASM_STRING_VIEW_ITER_TYPE",
  273: "WASM_TYPE_INFO_TYPE",
  274: "WEAK_ARRAY_LIST_TYPE",
  275: "WEAK_CELL_TYPE",
  276: "WASM_ARRAY_TYPE",
  277: "WASM_STRUCT_TYPE",
  278: "JS_PROXY_TYPE",
  1057: "JS_OBJECT_TYPE",
  279: "JS_GLOBAL_OBJECT_TYPE",
  280: "JS_GLOBAL_PROXY_TYPE",
  281: "JS_MODULE_NAMESPACE_TYPE",
  1040: "JS_SPECIAL_API_OBJECT_TYPE",
  1041: "JS_PRIMITIVE_WRAPPER_TYPE",
  1058: "JS_API_OBJECT_TYPE",
  2058: "JS_LAST_DUMMY_API_OBJECT_TYPE",
  2059: "JS_DATA_VIEW_TYPE",
  2060: "JS_RAB_GSAB_DATA_VIEW_TYPE",
  2061: "JS_TYPED_ARRAY_TYPE",
  2062: "JS_ARRAY_BUFFER_TYPE",
  2063: "JS_PROMISE_TYPE",
  2064: "JS_BOUND_FUNCTION_TYPE",
  2065: "JS_WRAPPED_FUNCTION_TYPE",
  2066: "JS_FUNCTION_TYPE",
  2067: "BIGINT64_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2068: "BIGUINT64_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2069: "FLOAT32_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2070: "FLOAT64_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2071: "INT16_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2072: "INT32_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2073: "INT8_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2074: "UINT16_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2075: "UINT32_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2076: "UINT8_CLAMPED_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2077: "UINT8_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2078: "JS_ARRAY_CONSTRUCTOR_TYPE",
  2079: "JS_PROMISE_CONSTRUCTOR_TYPE",
  2080: "JS_REG_EXP_CONSTRUCTOR_TYPE",
  2081: "JS_CLASS_CONSTRUCTOR_TYPE",
  2082: "JS_ARRAY_ITERATOR_PROTOTYPE_TYPE",
  2083: "JS_ITERATOR_PROTOTYPE_TYPE",
  2084: "JS_MAP_ITERATOR_PROTOTYPE_TYPE",
  2085: "JS_OBJECT_PROTOTYPE_TYPE",
  2086: "JS_PROMISE_PROTOTYPE_TYPE",
  2087: "JS_REG_EXP_PROTOTYPE_TYPE",
  2088: "JS_SET_ITERATOR_PROTOTYPE_TYPE",
  2089: "JS_SET_PROTOTYPE_TYPE",
  2090: "JS_STRING_ITERATOR_PROTOTYPE_TYPE",
  2091: "JS_TYPED_ARRAY_PROTOTYPE_TYPE",
  2092: "JS_MAP_KEY_ITERATOR_TYPE",
  2093: "JS_MAP_KEY_VALUE_ITERATOR_TYPE",
  2094: "JS_MAP_VALUE_ITERATOR_TYPE",
  2095: "JS_SET_KEY_VALUE_ITERATOR_TYPE",
  2096: "JS_SET_VALUE_ITERATOR_TYPE",
  2097: "JS_ITERATOR_DROP_HELPER_TYPE",
  2098: "JS_ITERATOR_FILTER_HELPER_TYPE",
  2099: "JS_ITERATOR_FLAT_MAP_HELPER_TYPE",
  2100: "JS_ITERATOR_MAP_HELPER_TYPE",
  2101: "JS_ITERATOR_TAKE_HELPER_TYPE",
  2102: "JS_ATOMICS_CONDITION_TYPE",
  2103: "JS_ATOMICS_MUTEX_TYPE",
  2104: "JS_SHARED_ARRAY_TYPE",
  2105: "JS_SHARED_STRUCT_TYPE",
  2106: "JS_GENERATOR_OBJECT_TYPE",
  2107: "JS_ASYNC_FUNCTION_OBJECT_TYPE",
  2108: "JS_ASYNC_GENERATOR_OBJECT_TYPE",
  2109: "JS_MAP_TYPE",
  2110: "JS_SET_TYPE",
  2111: "JS_WEAK_MAP_TYPE",
  2112: "JS_WEAK_SET_TYPE",
  2113: "JS_ARGUMENTS_OBJECT_TYPE",
  2114: "JS_ARRAY_TYPE",
  2115: "JS_ARRAY_ITERATOR_TYPE",
  2116: "JS_ASYNC_FROM_SYNC_ITERATOR_TYPE",
  2117: "JS_COLLATOR_TYPE",
  2118: "JS_CONTEXT_EXTENSION_OBJECT_TYPE",
  2119: "JS_DATE_TYPE",
  2120: "JS_DATE_TIME_FORMAT_TYPE",
  2121: "JS_DISPLAY_NAMES_TYPE",
  2122: "JS_DURATION_FORMAT_TYPE",
  2123: "JS_ERROR_TYPE",
  2124: "JS_EXTERNAL_OBJECT_TYPE",
  2125: "JS_FINALIZATION_REGISTRY_TYPE",
  2126: "JS_LIST_FORMAT_TYPE",
  2127: "JS_LOCALE_TYPE",
  2128: "JS_MESSAGE_OBJECT_TYPE",
  2129: "JS_NUMBER_FORMAT_TYPE",
  2130: "JS_PLURAL_RULES_TYPE",
  2131: "JS_RAW_JSON_TYPE",
  2132: "JS_REG_EXP_TYPE",
  2133: "JS_REG_EXP_STRING_ITERATOR_TYPE",
  2134: "JS_RELATIVE_TIME_FORMAT_TYPE",
  2135: "JS_SEGMENT_ITERATOR_TYPE",
  2136: "JS_SEGMENTER_TYPE",
  2137: "JS_SEGMENTS_TYPE",
  2138: "JS_SHADOW_REALM_TYPE",
  2139: "JS_STRING_ITERATOR_TYPE",
  2140: "JS_TEMPORAL_CALENDAR_TYPE",
  2141: "JS_TEMPORAL_DURATION_TYPE",
  2142: "JS_TEMPORAL_INSTANT_TYPE",
  2143: "JS_TEMPORAL_PLAIN_DATE_TYPE",
  2144: "JS_TEMPORAL_PLAIN_DATE_TIME_TYPE",
  2145: "JS_TEMPORAL_PLAIN_MONTH_DAY_TYPE",
  2146: "JS_TEMPORAL_PLAIN_TIME_TYPE",
  2147: "JS_TEMPORAL_PLAIN_YEAR_MONTH_TYPE",
  2148: "JS_TEMPORAL_TIME_ZONE_TYPE",
  2149: "JS_TEMPORAL_ZONED_DATE_TIME_TYPE",
  2150: "JS_V8_BREAK_ITERATOR_TYPE",
  2151: "JS_VALID_ITERATOR_WRAPPER_TYPE",
  2152: "JS_WEAK_REF_TYPE",
  2153: "WASM_EXCEPTION_PACKAGE_TYPE",
  2154: "WASM_GLOBAL_OBJECT_TYPE",
  2155: "WASM_INSTANCE_OBJECT_TYPE",
  2156: "WASM_MEMORY_OBJECT_TYPE",
  2157: "WASM_MODULE_OBJECT_TYPE",
  2158: "WASM_SUSPENDER_OBJECT_TYPE",
  2159: "WASM_TABLE_OBJECT_TYPE",
  2160: "WASM_TAG_OBJECT_TYPE",
  2161: "WASM_VALUE_OBJECT_TYPE",
}

# List of known V8 maps.
KNOWN_MAPS = {
    ("read_only_space", 0x00061): (255, "MetaMap"),
    ("read_only_space", 0x00089): (175, "FixedArrayMap"),
    ("read_only_space", 0x000b1): (240, "WeakFixedArrayMap"),
    ("read_only_space", 0x000d9): (274, "WeakArrayListMap"),
    ("read_only_space", 0x00101): (175, "FixedCOWArrayMap"),
    ("read_only_space", 0x00129): (236, "DescriptorArrayMap"),
    ("read_only_space", 0x00151): (131, "UndefinedMap"),
    ("read_only_space", 0x00179): (131, "NullMap"),
    ("read_only_space", 0x001a1): (131, "TheHoleMap"),
    ("read_only_space", 0x001c9): (151, "CallSiteInfoMap"),
    ("read_only_space", 0x001f1): (154, "EnumCacheMap"),
    ("read_only_space", 0x002a5): (261, "ScopeInfoMap"),
    ("read_only_space", 0x002cd): (175, "ModuleInfoMap"),
    ("read_only_space", 0x002f5): (187, "ClosureFeedbackCellArrayMap"),
    ("read_only_space", 0x0031d): (249, "FeedbackVectorMap"),
    ("read_only_space", 0x00345): (130, "HeapNumberMap"),
    ("read_only_space", 0x0036d): (204, "ForeignMap"),
    ("read_only_space", 0x00395): (256, "MegaDomHandlerMap"),
    ("read_only_space", 0x003bd): (131, "BooleanMap"),
    ("read_only_space", 0x003e5): (131, "UninitializedMap"),
    ("read_only_space", 0x0040d): (131, "ArgumentsMarkerMap"),
    ("read_only_space", 0x00435): (131, "ExceptionMap"),
    ("read_only_space", 0x0045d): (131, "TerminationExceptionMap"),
    ("read_only_space", 0x00485): (131, "OptimizedOutMap"),
    ("read_only_space", 0x004ad): (131, "StaleRegisterMap"),
    ("read_only_space", 0x004d5): (131, "SelfReferenceMarkerMap"),
    ("read_only_space", 0x004fd): (131, "BasicBlockCountersMarkerMap"),
    ("read_only_space", 0x00525): (129, "BigIntMap"),
    ("read_only_space", 0x0054d): (128, "SymbolMap"),
    ("read_only_space", 0x00575): (32, "StringMap"),
    ("read_only_space", 0x0059d): (40, "OneByteStringMap"),
    ("read_only_space", 0x005c5): (33, "ConsStringMap"),
    ("read_only_space", 0x005ed): (41, "ConsOneByteStringMap"),
    ("read_only_space", 0x00615): (35, "SlicedStringMap"),
    ("read_only_space", 0x0063d): (43, "SlicedOneByteStringMap"),
    ("read_only_space", 0x00665): (34, "ExternalStringMap"),
    ("read_only_space", 0x0068d): (42, "ExternalOneByteStringMap"),
    ("read_only_space", 0x006b5): (50, "UncachedExternalStringMap"),
    ("read_only_space", 0x006dd): (58, "UncachedExternalOneByteStringMap"),
    ("read_only_space", 0x00705): (98, "SharedExternalStringMap"),
    ("read_only_space", 0x0072d): (106, "SharedExternalOneByteStringMap"),
    ("read_only_space", 0x00755): (114, "SharedUncachedExternalStringMap"),
    ("read_only_space", 0x0077d): (122, "SharedUncachedExternalOneByteStringMap"),
    ("read_only_space", 0x007a5): (2, "ExternalInternalizedStringMap"),
    ("read_only_space", 0x007cd): (10, "ExternalOneByteInternalizedStringMap"),
    ("read_only_space", 0x007f5): (18, "UncachedExternalInternalizedStringMap"),
    ("read_only_space", 0x0081d): (26, "UncachedExternalOneByteInternalizedStringMap"),
    ("read_only_space", 0x00845): (0, "InternalizedStringMap"),
    ("read_only_space", 0x0086d): (8, "OneByteInternalizedStringMap"),
    ("read_only_space", 0x00895): (37, "ThinStringMap"),
    ("read_only_space", 0x008bd): (96, "SharedStringMap"),
    ("read_only_space", 0x008e5): (104, "SharedOneByteStringMap"),
    ("read_only_space", 0x0090d): (192, "FixedDoubleArrayMap"),
    ("read_only_space", 0x00935): (248, "FeedbackMetadataArrayMap"),
    ("read_only_space", 0x0095d): (190, "ByteArrayMap"),
    ("read_only_space", 0x00985): (191, "BytecodeArrayMap"),
    ("read_only_space", 0x009ad): (251, "FreeSpaceMap"),
    ("read_only_space", 0x009d5): (259, "PropertyArrayMap"),
    ("read_only_space", 0x009fd): (231, "SmallOrderedHashMapMap"),
    ("read_only_space", 0x00a25): (232, "SmallOrderedHashSetMap"),
    ("read_only_space", 0x00a4d): (233, "SmallOrderedNameDictionaryMap"),
    ("read_only_space", 0x00a75): (252, "InstructionStreamMap"),
    ("read_only_space", 0x00a9d): (244, "CellMap"),
    ("read_only_space", 0x00acd): (260, "GlobalPropertyCellMap"),
    ("read_only_space", 0x00af5): (250, "OnePointerFillerMap"),
    ("read_only_space", 0x00b1d): (250, "TwoPointerFillerMap"),
    ("read_only_space", 0x00b45): (156, "NoClosuresCellMap"),
    ("read_only_space", 0x00b6d): (156, "OneClosureCellMap"),
    ("read_only_space", 0x00b95): (156, "ManyClosuresCellMap"),
    ("read_only_space", 0x00bbd): (241, "TransitionArrayMap"),
    ("read_only_space", 0x00be5): (176, "HashTableMap"),
    ("read_only_space", 0x00c0d): (184, "OrderedNameDictionaryMap"),
    ("read_only_space", 0x00c35): (179, "NameDictionaryMap"),
    ("read_only_space", 0x00c5d): (266, "SwissNameDictionaryMap"),
    ("read_only_space", 0x00c85): (178, "GlobalDictionaryMap"),
    ("read_only_space", 0x00cad): (181, "NumberDictionaryMap"),
    ("read_only_space", 0x00cd5): (185, "RegisteredSymbolTableMap"),
    ("read_only_space", 0x00cfd): (175, "ArrayListMap"),
    ("read_only_space", 0x00d25): (242, "AccessorInfoMap"),
    ("read_only_space", 0x00d4d): (258, "PreparseDataMap"),
    ("read_only_space", 0x00d75): (262, "SharedFunctionInfoMap"),
    ("read_only_space", 0x00d9d): (245, "CodeMap"),
    ("read_only_space", 0x00ff9): (132, "PromiseFulfillReactionJobTaskMap"),
    ("read_only_space", 0x01021): (133, "PromiseRejectReactionJobTaskMap"),
    ("read_only_space", 0x01049): (134, "CallableTaskMap"),
    ("read_only_space", 0x01071): (135, "CallbackTaskMap"),
    ("read_only_space", 0x01099): (136, "PromiseResolveThenableJobTaskMap"),
    ("read_only_space", 0x010c1): (139, "FunctionTemplateInfoMap"),
    ("read_only_space", 0x010e9): (140, "ObjectTemplateInfoMap"),
    ("read_only_space", 0x01111): (141, "AccessCheckInfoMap"),
    ("read_only_space", 0x01139): (142, "AccessorPairMap"),
    ("read_only_space", 0x01161): (143, "AliasedArgumentsEntryMap"),
    ("read_only_space", 0x01189): (144, "AllocationMementoMap"),
    ("read_only_space", 0x011b1): (146, "ArrayBoilerplateDescriptionMap"),
    ("read_only_space", 0x011d9): (147, "AsmWasmDataMap"),
    ("read_only_space", 0x01201): (148, "AsyncGeneratorRequestMap"),
    ("read_only_space", 0x01229): (149, "BreakPointMap"),
    ("read_only_space", 0x01251): (150, "BreakPointInfoMap"),
    ("read_only_space", 0x01279): (152, "ClassPositionsMap"),
    ("read_only_space", 0x012a1): (153, "DebugInfoMap"),
    ("read_only_space", 0x012c9): (155, "ErrorStackDataMap"),
    ("read_only_space", 0x012f1): (157, "FunctionTemplateRareDataMap"),
    ("read_only_space", 0x01319): (158, "InterceptorInfoMap"),
    ("read_only_space", 0x01341): (159, "InterpreterDataMap"),
    ("read_only_space", 0x01369): (160, "ModuleRequestMap"),
    ("read_only_space", 0x01391): (161, "PromiseCapabilityMap"),
    ("read_only_space", 0x013b9): (162, "PromiseOnStackMap"),
    ("read_only_space", 0x013e1): (163, "PromiseReactionMap"),
    ("read_only_space", 0x01409): (164, "PropertyDescriptorObjectMap"),
    ("read_only_space", 0x01431): (165, "PrototypeInfoMap"),
    ("read_only_space", 0x01459): (166, "RegExpBoilerplateDescriptionMap"),
    ("read_only_space", 0x01481): (167, "ScriptMap"),
    ("read_only_space", 0x014a9): (168, "ScriptOrModuleMap"),
    ("read_only_space", 0x014d1): (169, "SourceTextModuleInfoEntryMap"),
    ("read_only_space", 0x014f9): (170, "StackFrameInfoMap"),
    ("read_only_space", 0x01521): (171, "TemplateObjectDescriptionMap"),
    ("read_only_space", 0x01549): (172, "Tuple2Map"),
    ("read_only_space", 0x01571): (173, "WasmExceptionTagMap"),
    ("read_only_space", 0x01599): (174, "WasmIndirectFunctionTableMap"),
    ("read_only_space", 0x015c1): (145, "AllocationSiteWithWeakNextMap"),
    ("read_only_space", 0x015e9): (145, "AllocationSiteWithoutWeakNextMap"),
    ("read_only_space", 0x01611): (137, "LoadHandler1Map"),
    ("read_only_space", 0x01639): (137, "LoadHandler2Map"),
    ("read_only_space", 0x01661): (137, "LoadHandler3Map"),
    ("read_only_space", 0x01689): (138, "StoreHandler0Map"),
    ("read_only_space", 0x016b1): (138, "StoreHandler1Map"),
    ("read_only_space", 0x016d9): (138, "StoreHandler2Map"),
    ("read_only_space", 0x01701): (138, "StoreHandler3Map"),
    ("read_only_space", 0x01729): (222, "UncompiledDataWithoutPreparseDataMap"),
    ("read_only_space", 0x01751): (220, "UncompiledDataWithPreparseDataMap"),
    ("read_only_space", 0x01779): (223, "UncompiledDataWithoutPreparseDataWithJobMap"),
    ("read_only_space", 0x017a1): (221, "UncompiledDataWithPreparseDataAndJobMap"),
    ("read_only_space", 0x017c9): (257, "OnHeapBasicBlockProfilerDataMap"),
    ("read_only_space", 0x017f1): (215, "TurbofanBitsetTypeMap"),
    ("read_only_space", 0x01819): (219, "TurbofanUnionTypeMap"),
    ("read_only_space", 0x01841): (218, "TurbofanRangeTypeMap"),
    ("read_only_space", 0x01869): (216, "TurbofanHeapConstantTypeMap"),
    ("read_only_space", 0x01891): (217, "TurbofanOtherNumberConstantTypeMap"),
    ("read_only_space", 0x018b9): (198, "TurboshaftWord32TypeMap"),
    ("read_only_space", 0x018e1): (199, "TurboshaftWord32RangeTypeMap"),
    ("read_only_space", 0x01909): (201, "TurboshaftWord64TypeMap"),
    ("read_only_space", 0x01931): (202, "TurboshaftWord64RangeTypeMap"),
    ("read_only_space", 0x01959): (195, "TurboshaftFloat64TypeMap"),
    ("read_only_space", 0x01981): (196, "TurboshaftFloat64RangeTypeMap"),
    ("read_only_space", 0x019a9): (253, "InternalClassMap"),
    ("read_only_space", 0x019d1): (264, "SmiPairMap"),
    ("read_only_space", 0x019f9): (263, "SmiBoxMap"),
    ("read_only_space", 0x01a21): (228, "ExportedSubClassBaseMap"),
    ("read_only_space", 0x01a49): (229, "ExportedSubClassMap"),
    ("read_only_space", 0x01a71): (234, "AbstractInternalClassSubclass1Map"),
    ("read_only_space", 0x01a99): (235, "AbstractInternalClassSubclass2Map"),
    ("read_only_space", 0x01ac1): (230, "ExportedSubClass2Map"),
    ("read_only_space", 0x01ae9): (265, "SortStateMap"),
    ("read_only_space", 0x01b11): (272, "WasmStringViewIterMap"),
    ("read_only_space", 0x01b39): (194, "SloppyArgumentsElementsMap"),
    ("read_only_space", 0x01b61): (237, "StrongDescriptorArrayMap"),
    ("read_only_space", 0x01b89): (200, "TurboshaftWord32SetTypeMap"),
    ("read_only_space", 0x01bb1): (203, "TurboshaftWord64SetTypeMap"),
    ("read_only_space", 0x01bd9): (197, "TurboshaftFloat64SetTypeMap"),
    ("read_only_space", 0x01c01): (193, "InternalClassWithSmiElementsMap"),
    ("read_only_space", 0x01c29): (254, "InternalClassWithStructElementsMap"),
    ("read_only_space", 0x01c51): (182, "OrderedHashMapMap"),
    ("read_only_space", 0x01c79): (183, "OrderedHashSetMap"),
    ("read_only_space", 0x01ca1): (186, "SimpleNumberDictionaryMap"),
    ("read_only_space", 0x01cc9): (180, "NameToIndexHashTableMap"),
    ("read_only_space", 0x01cf1): (247, "EmbedderDataArrayMap"),
    ("read_only_space", 0x01d19): (177, "EphemeronHashTableMap"),
    ("read_only_space", 0x01d41): (189, "ScriptContextTableMap"),
    ("read_only_space", 0x01d69): (188, "ObjectBoilerplateDescriptionMap"),
    ("read_only_space", 0x01d91): (246, "CoverageInfoMap"),
    ("read_only_space", 0x01db9): (243, "SideEffectCallHandlerInfoMap"),
    ("read_only_space", 0x01de1): (243, "SideEffectFreeCallHandlerInfoMap"),
    ("read_only_space", 0x01e09): (238, "SourceTextModuleMap"),
    ("read_only_space", 0x01e31): (239, "SyntheticModuleMap"),
    ("read_only_space", 0x01e59): (267, "WasmApiFunctionRefMap"),
    ("read_only_space", 0x01e81): (225, "WasmCapiFunctionDataMap"),
    ("read_only_space", 0x01ea9): (226, "WasmExportedFunctionDataMap"),
    ("read_only_space", 0x01ed1): (269, "WasmInternalFunctionMap"),
    ("read_only_space", 0x01ef9): (227, "WasmJSFunctionDataMap"),
    ("read_only_space", 0x01f21): (271, "WasmResumeDataMap"),
    ("read_only_space", 0x01f49): (273, "WasmTypeInfoMap"),
    ("read_only_space", 0x01f71): (268, "WasmContinuationObjectMap"),
    ("read_only_space", 0x01f99): (270, "WasmNullMap"),
    ("read_only_space", 0x01fc1): (275, "WeakCellMap"),
    ("old_space", 0x043bd): (2124, "ExternalMap"),
    ("old_space", 0x043e5): (2128, "JSMessageObjectMap"),
}

# List of known V8 objects.
KNOWN_OBJECTS = {
  ("read_only_space", 0x00219): "EmptyFixedArray",
  ("read_only_space", 0x00221): "EmptyWeakFixedArray",
  ("read_only_space", 0x00229): "EmptyWeakArrayList",
  ("read_only_space", 0x00235): "NullValue",
  ("read_only_space", 0x00251): "UndefinedValue",
  ("read_only_space", 0x0026d): "TheHoleValue",
  ("read_only_space", 0x00289): "EmptyEnumCache",
  ("read_only_space", 0x00295): "EmptyDescriptorArray",
  ("read_only_space", 0x00ac5): "InvalidPrototypeValidityCell",
  ("read_only_space", 0x00dc5): "TrueValue",
  ("read_only_space", 0x00de1): "FalseValue",
  ("read_only_space", 0x00dfd): "HashSeed",
  ("read_only_space", 0x00e0d): "empty_string",
  ("read_only_space", 0x00efd): "EmptyPropertyDictionary",
  ("read_only_space", 0x00f29): "EmptyOrderedPropertyDictionary",
  ("read_only_space", 0x00f4d): "EmptySwissPropertyDictionary",
  ("read_only_space", 0x00f6d): "EmptyByteArray",
  ("read_only_space", 0x00f75): "EmptyScopeInfo",
  ("read_only_space", 0x00f85): "EmptyPropertyArray",
  ("read_only_space", 0x00f8d): "MinusZeroValue",
  ("read_only_space", 0x00f99): "NanValue",
  ("read_only_space", 0x00fa5): "HoleNanValue",
  ("read_only_space", 0x00fb1): "InfinityValue",
  ("read_only_space", 0x00fbd): "MinusInfinityValue",
  ("read_only_space", 0x00fc9): "MaxSafeInteger",
  ("read_only_space", 0x00fd5): "MaxUInt32",
  ("read_only_space", 0x00fe1): "SmiMinValue",
  ("read_only_space", 0x00fed): "SmiMaxValuePlusOne",
  ("read_only_space", 0x01fe9): "NoOpInterceptorInfo",
  ("read_only_space", 0x02011): "EmptyArrayList",
  ("read_only_space", 0x0201d): "EmptyObjectBoilerplateDescription",
  ("read_only_space", 0x02029): "EmptyArrayBoilerplateDescription",
  ("read_only_space", 0x02035): "EmptyClosureFeedbackCellArray",
  ("read_only_space", 0x0203d): "SingleCharacterStringTable",
  ("read_only_space", 0x05acd): "UninitializedValue",
  ("read_only_space", 0x05b05): "ArgumentsMarker",
  ("read_only_space", 0x05b3d): "TerminationException",
  ("read_only_space", 0x05b7d): "Exception",
  ("read_only_space", 0x05b99): "OptimizedOut",
  ("read_only_space", 0x05bd1): "StaleRegister",
  ("read_only_space", 0x05c09): "SelfReferenceMarker",
  ("read_only_space", 0x05c49): "BasicBlockCountersMarker",
  ("read_only_space", 0x06111): "EmptySlowElementDictionary",
  ("read_only_space", 0x06135): "EmptySymbolTable",
  ("read_only_space", 0x06151): "EmptyOrderedHashMap",
  ("read_only_space", 0x06165): "EmptyOrderedHashSet",
  ("read_only_space", 0x06179): "EmptyFeedbackMetadata",
  ("read_only_space", 0x06185): "GlobalThisBindingScopeInfo",
  ("read_only_space", 0x061a5): "EmptyFunctionScopeInfo",
  ("read_only_space", 0x061c9): "NativeScopeInfo",
  ("read_only_space", 0x061e1): "ShadowRealmScopeInfo",
  ("read_only_space", 0x0fffd): "WasmNull",
  ("old_space", 0x0426d): "ArgumentsIteratorAccessor",
  ("old_space", 0x04285): "ArrayLengthAccessor",
  ("old_space", 0x0429d): "BoundFunctionLengthAccessor",
  ("old_space", 0x042b5): "BoundFunctionNameAccessor",
  ("old_space", 0x042cd): "ErrorStackAccessor",
  ("old_space", 0x042e5): "FunctionArgumentsAccessor",
  ("old_space", 0x042fd): "FunctionCallerAccessor",
  ("old_space", 0x04315): "FunctionNameAccessor",
  ("old_space", 0x0432d): "FunctionLengthAccessor",
  ("old_space", 0x04345): "FunctionPrototypeAccessor",
  ("old_space", 0x0435d): "StringLengthAccessor",
  ("old_space", 0x04375): "ValueUnavailableAccessor",
  ("old_space", 0x0438d): "WrappedFunctionLengthAccessor",
  ("old_space", 0x043a5): "WrappedFunctionNameAccessor",
  ("old_space", 0x043bd): "ExternalMap",
  ("old_space", 0x043e5): "JSMessageObjectMap",
  ("old_space", 0x0440d): "EmptyScript",
  ("old_space", 0x04455): "ManyClosuresCell",
  ("old_space", 0x04461): "ArrayConstructorProtector",
  ("old_space", 0x04475): "NoElementsProtector",
  ("old_space", 0x04489): "MegaDOMProtector",
  ("old_space", 0x0449d): "IsConcatSpreadableProtector",
  ("old_space", 0x044b1): "ArraySpeciesProtector",
  ("old_space", 0x044c5): "TypedArraySpeciesProtector",
  ("old_space", 0x044d9): "PromiseSpeciesProtector",
  ("old_space", 0x044ed): "RegExpSpeciesProtector",
  ("old_space", 0x04501): "StringLengthProtector",
  ("old_space", 0x04515): "ArrayIteratorProtector",
  ("old_space", 0x04529): "ArrayBufferDetachingProtector",
  ("old_space", 0x0453d): "PromiseHookProtector",
  ("old_space", 0x04551): "PromiseResolveProtector",
  ("old_space", 0x04565): "MapIteratorProtector",
  ("old_space", 0x04579): "PromiseThenProtector",
  ("old_space", 0x0458d): "SetIteratorProtector",
  ("old_space", 0x045a1): "StringIteratorProtector",
  ("old_space", 0x045b5): "NumberStringPrototypeNoReplaceProtector",
  ("old_space", 0x045c9): "StringSplitCache",
  ("old_space", 0x049d1): "RegExpMultipleCache",
  ("old_space", 0x04dd9): "BuiltinsConstantsTable",
  ("old_space", 0x054a9): "AsyncFunctionAwaitRejectSharedFun",
  ("old_space", 0x054cd): "AsyncFunctionAwaitResolveSharedFun",
  ("old_space", 0x054f1): "AsyncGeneratorAwaitRejectSharedFun",
  ("old_space", 0x05515): "AsyncGeneratorAwaitResolveSharedFun",
  ("old_space", 0x05539): "AsyncGeneratorYieldWithAwaitResolveSharedFun",
  ("old_space", 0x0555d): "AsyncGeneratorReturnResolveSharedFun",
  ("old_space", 0x05581): "AsyncGeneratorReturnClosedRejectSharedFun",
  ("old_space", 0x055a5): "AsyncGeneratorReturnClosedResolveSharedFun",
  ("old_space", 0x055c9): "AsyncIteratorValueUnwrapSharedFun",
  ("old_space", 0x055ed): "PromiseAllResolveElementSharedFun",
  ("old_space", 0x05611): "PromiseAllSettledResolveElementSharedFun",
  ("old_space", 0x05635): "PromiseAllSettledRejectElementSharedFun",
  ("old_space", 0x05659): "PromiseAnyRejectElementSharedFun",
  ("old_space", 0x0567d): "PromiseCapabilityDefaultRejectSharedFun",
  ("old_space", 0x056a1): "PromiseCapabilityDefaultResolveSharedFun",
  ("old_space", 0x056c5): "PromiseCatchFinallySharedFun",
  ("old_space", 0x056e9): "PromiseGetCapabilitiesExecutorSharedFun",
  ("old_space", 0x0570d): "PromiseThenFinallySharedFun",
  ("old_space", 0x05731): "PromiseThrowerFinallySharedFun",
  ("old_space", 0x05755): "PromiseValueThunkFinallySharedFun",
  ("old_space", 0x05779): "ProxyRevokeSharedFun",
  ("old_space", 0x0579d): "ShadowRealmImportValueFulfilledSFI",
  ("old_space", 0x057c1): "SourceTextModuleExecuteAsyncModuleFulfilledSFI",
  ("old_space", 0x057e5): "SourceTextModuleExecuteAsyncModuleRejectedSFI",
}

# Lower 32 bits of first page addresses for various heap spaces.
HEAP_FIRST_PAGES = {
  0x00140000: "old_space",
  0x00000000: "read_only_space",
}

# List of known V8 Frame Markers.
FRAME_MARKERS = (
  "ENTRY",
  "CONSTRUCT_ENTRY",
  "EXIT",
  "WASM",
  "WASM_TO_JS",
  "WASM_TO_JS_FUNCTION",
  "JS_TO_WASM",
  "STACK_SWITCH",
  "WASM_DEBUG_BREAK",
  "C_WASM_ENTRY",
  "WASM_EXIT",
  "WASM_LIFTOFF_SETUP",
  "INTERPRETED",
  "BASELINE",
  "MAGLEV",
  "TURBOFAN",
  "STUB",
  "TURBOFAN_STUB_WITH_CONTEXT",
  "BUILTIN_CONTINUATION",
  "JAVA_SCRIPT_BUILTIN_CONTINUATION",
  "JAVA_SCRIPT_BUILTIN_CONTINUATION_WITH_CATCH",
  "INTERNAL",
  "CONSTRUCT",
  "BUILTIN",
  "BUILTIN_EXIT",
  "NATIVE",
  "IRREGEXP",
)

# This set of constants is generated from a shipping build.
