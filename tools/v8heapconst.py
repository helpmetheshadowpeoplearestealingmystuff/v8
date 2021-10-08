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
  68: "ABSTRACT_INTERNAL_CLASS_SUBCLASS1_TYPE",
  69: "ABSTRACT_INTERNAL_CLASS_SUBCLASS2_TYPE",
  70: "FOREIGN_TYPE",
  71: "WASM_FUNCTION_DATA_TYPE",
  72: "WASM_CAPI_FUNCTION_DATA_TYPE",
  73: "WASM_EXPORTED_FUNCTION_DATA_TYPE",
  74: "WASM_JS_FUNCTION_DATA_TYPE",
  75: "WASM_TYPE_INFO_TYPE",
  76: "PROMISE_FULFILL_REACTION_JOB_TASK_TYPE",
  77: "PROMISE_REJECT_REACTION_JOB_TASK_TYPE",
  78: "CALLABLE_TASK_TYPE",
  79: "CALLBACK_TASK_TYPE",
  80: "PROMISE_RESOLVE_THENABLE_JOB_TASK_TYPE",
  81: "LOAD_HANDLER_TYPE",
  82: "STORE_HANDLER_TYPE",
  83: "FUNCTION_TEMPLATE_INFO_TYPE",
  84: "OBJECT_TEMPLATE_INFO_TYPE",
  85: "ACCESS_CHECK_INFO_TYPE",
  86: "ACCESSOR_INFO_TYPE",
  87: "ACCESSOR_PAIR_TYPE",
  88: "ALIASED_ARGUMENTS_ENTRY_TYPE",
  89: "ALLOCATION_MEMENTO_TYPE",
  90: "ALLOCATION_SITE_TYPE",
  91: "ARRAY_BOILERPLATE_DESCRIPTION_TYPE",
  92: "ASM_WASM_DATA_TYPE",
  93: "ASYNC_GENERATOR_REQUEST_TYPE",
  94: "BREAK_POINT_TYPE",
  95: "BREAK_POINT_INFO_TYPE",
  96: "CACHED_TEMPLATE_OBJECT_TYPE",
  97: "CALL_HANDLER_INFO_TYPE",
  98: "CLASS_POSITIONS_TYPE",
  99: "DEBUG_INFO_TYPE",
  100: "ENUM_CACHE_TYPE",
  101: "FEEDBACK_CELL_TYPE",
  102: "FUNCTION_TEMPLATE_RARE_DATA_TYPE",
  103: "INTERCEPTOR_INFO_TYPE",
  104: "INTERPRETER_DATA_TYPE",
  105: "MODULE_REQUEST_TYPE",
  106: "PROMISE_CAPABILITY_TYPE",
  107: "PROMISE_REACTION_TYPE",
  108: "PROPERTY_DESCRIPTOR_OBJECT_TYPE",
  109: "PROTOTYPE_INFO_TYPE",
  110: "REG_EXP_BOILERPLATE_DESCRIPTION_TYPE",
  111: "SCRIPT_TYPE",
  112: "SOURCE_TEXT_MODULE_INFO_ENTRY_TYPE",
  113: "STACK_FRAME_INFO_TYPE",
  114: "TEMPLATE_OBJECT_DESCRIPTION_TYPE",
  115: "TUPLE2_TYPE",
  116: "WASM_EXCEPTION_TAG_TYPE",
  117: "WASM_INDIRECT_FUNCTION_TABLE_TYPE",
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
  128: "CLOSURE_FEEDBACK_CELL_ARRAY_TYPE",
  129: "OBJECT_BOILERPLATE_DESCRIPTION_TYPE",
  130: "SCRIPT_CONTEXT_TABLE_TYPE",
  131: "BYTE_ARRAY_TYPE",
  132: "BYTECODE_ARRAY_TYPE",
  133: "FIXED_DOUBLE_ARRAY_TYPE",
  134: "INTERNAL_CLASS_WITH_SMI_ELEMENTS_TYPE",
  135: "SLOPPY_ARGUMENTS_ELEMENTS_TYPE",
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
  146: "EXPORTED_SUB_CLASS_BASE_TYPE",
  147: "EXPORTED_SUB_CLASS_TYPE",
  148: "EXPORTED_SUB_CLASS2_TYPE",
  149: "SMALL_ORDERED_HASH_MAP_TYPE",
  150: "SMALL_ORDERED_HASH_SET_TYPE",
  151: "SMALL_ORDERED_NAME_DICTIONARY_TYPE",
  152: "DESCRIPTOR_ARRAY_TYPE",
  153: "STRONG_DESCRIPTOR_ARRAY_TYPE",
  154: "SOURCE_TEXT_MODULE_TYPE",
  155: "SYNTHETIC_MODULE_TYPE",
  156: "UNCOMPILED_DATA_WITH_PREPARSE_DATA_TYPE",
  157: "UNCOMPILED_DATA_WITHOUT_PREPARSE_DATA_TYPE",
  158: "WEAK_FIXED_ARRAY_TYPE",
  159: "TRANSITION_ARRAY_TYPE",
  160: "CALL_REF_DATA_TYPE",
  161: "CELL_TYPE",
  162: "CODE_TYPE",
  163: "CODE_DATA_CONTAINER_TYPE",
  164: "COVERAGE_INFO_TYPE",
  165: "EMBEDDER_DATA_ARRAY_TYPE",
  166: "FEEDBACK_METADATA_TYPE",
  167: "FEEDBACK_VECTOR_TYPE",
  168: "FILLER_TYPE",
  169: "FREE_SPACE_TYPE",
  170: "INTERNAL_CLASS_TYPE",
  171: "INTERNAL_CLASS_WITH_STRUCT_ELEMENTS_TYPE",
  172: "MAP_TYPE",
  173: "MEGA_DOM_HANDLER_TYPE",
  174: "ON_HEAP_BASIC_BLOCK_PROFILER_DATA_TYPE",
  175: "PREPARSE_DATA_TYPE",
  176: "PROPERTY_ARRAY_TYPE",
  177: "PROPERTY_CELL_TYPE",
  178: "SCOPE_INFO_TYPE",
  179: "SHARED_FUNCTION_INFO_TYPE",
  180: "SMI_BOX_TYPE",
  181: "SMI_PAIR_TYPE",
  182: "SORT_STATE_TYPE",
  183: "SWISS_NAME_DICTIONARY_TYPE",
  184: "WEAK_ARRAY_LIST_TYPE",
  185: "WEAK_CELL_TYPE",
  186: "WASM_ARRAY_TYPE",
  187: "WASM_STRUCT_TYPE",
  188: "JS_PROXY_TYPE",
  1057: "JS_OBJECT_TYPE",
  189: "JS_GLOBAL_OBJECT_TYPE",
  190: "JS_GLOBAL_PROXY_TYPE",
  191: "JS_MODULE_NAMESPACE_TYPE",
  1040: "JS_SPECIAL_API_OBJECT_TYPE",
  1041: "JS_PRIMITIVE_WRAPPER_TYPE",
  1058: "JS_API_OBJECT_TYPE",
  2058: "JS_LAST_DUMMY_API_OBJECT_TYPE",
  2059: "JS_BOUND_FUNCTION_TYPE",
  2060: "JS_FUNCTION_TYPE",
  2061: "BIGINT64_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2062: "BIGUINT64_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2063: "FLOAT32_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2064: "FLOAT64_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2065: "INT16_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2066: "INT32_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2067: "INT8_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2068: "UINT16_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2069: "UINT32_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2070: "UINT8_CLAMPED_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2071: "UINT8_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2072: "JS_ARRAY_CONSTRUCTOR_TYPE",
  2073: "JS_PROMISE_CONSTRUCTOR_TYPE",
  2074: "JS_REG_EXP_CONSTRUCTOR_TYPE",
  2075: "JS_CLASS_CONSTRUCTOR_TYPE",
  2076: "JS_ARRAY_ITERATOR_PROTOTYPE_TYPE",
  2077: "JS_ITERATOR_PROTOTYPE_TYPE",
  2078: "JS_MAP_ITERATOR_PROTOTYPE_TYPE",
  2079: "JS_OBJECT_PROTOTYPE_TYPE",
  2080: "JS_PROMISE_PROTOTYPE_TYPE",
  2081: "JS_REG_EXP_PROTOTYPE_TYPE",
  2082: "JS_SET_ITERATOR_PROTOTYPE_TYPE",
  2083: "JS_SET_PROTOTYPE_TYPE",
  2084: "JS_STRING_ITERATOR_PROTOTYPE_TYPE",
  2085: "JS_TYPED_ARRAY_PROTOTYPE_TYPE",
  2086: "JS_MAP_KEY_ITERATOR_TYPE",
  2087: "JS_MAP_KEY_VALUE_ITERATOR_TYPE",
  2088: "JS_MAP_VALUE_ITERATOR_TYPE",
  2089: "JS_SET_KEY_VALUE_ITERATOR_TYPE",
  2090: "JS_SET_VALUE_ITERATOR_TYPE",
  2091: "JS_GENERATOR_OBJECT_TYPE",
  2092: "JS_ASYNC_FUNCTION_OBJECT_TYPE",
  2093: "JS_ASYNC_GENERATOR_OBJECT_TYPE",
  2094: "JS_DATA_VIEW_TYPE",
  2095: "JS_TYPED_ARRAY_TYPE",
  2096: "JS_MAP_TYPE",
  2097: "JS_SET_TYPE",
  2098: "JS_WEAK_MAP_TYPE",
  2099: "JS_WEAK_SET_TYPE",
  2100: "JS_ARGUMENTS_OBJECT_TYPE",
  2101: "JS_ARRAY_TYPE",
  2102: "JS_ARRAY_BUFFER_TYPE",
  2103: "JS_ARRAY_ITERATOR_TYPE",
  2104: "JS_ASYNC_FROM_SYNC_ITERATOR_TYPE",
  2105: "JS_COLLATOR_TYPE",
  2106: "JS_CONTEXT_EXTENSION_OBJECT_TYPE",
  2107: "JS_DATE_TYPE",
  2108: "JS_DATE_TIME_FORMAT_TYPE",
  2109: "JS_DISPLAY_NAMES_TYPE",
  2110: "JS_ERROR_TYPE",
  2111: "JS_FINALIZATION_REGISTRY_TYPE",
  2112: "JS_LIST_FORMAT_TYPE",
  2113: "JS_LOCALE_TYPE",
  2114: "JS_MESSAGE_OBJECT_TYPE",
  2115: "JS_NUMBER_FORMAT_TYPE",
  2116: "JS_PLURAL_RULES_TYPE",
  2117: "JS_PROMISE_TYPE",
  2118: "JS_REG_EXP_TYPE",
  2119: "JS_REG_EXP_STRING_ITERATOR_TYPE",
  2120: "JS_RELATIVE_TIME_FORMAT_TYPE",
  2121: "JS_SEGMENT_ITERATOR_TYPE",
  2122: "JS_SEGMENTER_TYPE",
  2123: "JS_SEGMENTS_TYPE",
  2124: "JS_STRING_ITERATOR_TYPE",
  2125: "JS_TEMPORAL_CALENDAR_TYPE",
  2126: "JS_TEMPORAL_DURATION_TYPE",
  2127: "JS_TEMPORAL_INSTANT_TYPE",
  2128: "JS_TEMPORAL_PLAIN_DATE_TYPE",
  2129: "JS_TEMPORAL_PLAIN_DATE_TIME_TYPE",
  2130: "JS_TEMPORAL_PLAIN_MONTH_DAY_TYPE",
  2131: "JS_TEMPORAL_PLAIN_TIME_TYPE",
  2132: "JS_TEMPORAL_PLAIN_YEAR_MONTH_TYPE",
  2133: "JS_TEMPORAL_TIME_ZONE_TYPE",
  2134: "JS_TEMPORAL_ZONED_DATE_TIME_TYPE",
  2135: "JS_V8_BREAK_ITERATOR_TYPE",
  2136: "JS_WEAK_REF_TYPE",
  2137: "WASM_GLOBAL_OBJECT_TYPE",
  2138: "WASM_INSTANCE_OBJECT_TYPE",
  2139: "WASM_MEMORY_OBJECT_TYPE",
  2140: "WASM_MODULE_OBJECT_TYPE",
  2141: "WASM_TABLE_OBJECT_TYPE",
  2142: "WASM_TAG_OBJECT_TYPE",
  2143: "WASM_VALUE_OBJECT_TYPE",
}

# List of known V8 maps.
KNOWN_MAPS = {
  ("read_only_space", 0x02119): (172, "MetaMap"),
  ("read_only_space", 0x02141): (67, "NullMap"),
  ("read_only_space", 0x02169): (153, "StrongDescriptorArrayMap"),
  ("read_only_space", 0x02191): (158, "WeakFixedArrayMap"),
  ("read_only_space", 0x021d1): (100, "EnumCacheMap"),
  ("read_only_space", 0x02205): (118, "FixedArrayMap"),
  ("read_only_space", 0x02251): (8, "OneByteInternalizedStringMap"),
  ("read_only_space", 0x0229d): (169, "FreeSpaceMap"),
  ("read_only_space", 0x022c5): (168, "OnePointerFillerMap"),
  ("read_only_space", 0x022ed): (168, "TwoPointerFillerMap"),
  ("read_only_space", 0x02315): (67, "UninitializedMap"),
  ("read_only_space", 0x0238d): (67, "UndefinedMap"),
  ("read_only_space", 0x023d1): (66, "HeapNumberMap"),
  ("read_only_space", 0x02405): (67, "TheHoleMap"),
  ("read_only_space", 0x02465): (67, "BooleanMap"),
  ("read_only_space", 0x02509): (131, "ByteArrayMap"),
  ("read_only_space", 0x02531): (118, "FixedCOWArrayMap"),
  ("read_only_space", 0x02559): (119, "HashTableMap"),
  ("read_only_space", 0x02581): (64, "SymbolMap"),
  ("read_only_space", 0x025a9): (40, "OneByteStringMap"),
  ("read_only_space", 0x025d1): (178, "ScopeInfoMap"),
  ("read_only_space", 0x025f9): (179, "SharedFunctionInfoMap"),
  ("read_only_space", 0x02621): (162, "CodeMap"),
  ("read_only_space", 0x02649): (161, "CellMap"),
  ("read_only_space", 0x02671): (177, "GlobalPropertyCellMap"),
  ("read_only_space", 0x02699): (70, "ForeignMap"),
  ("read_only_space", 0x026c1): (159, "TransitionArrayMap"),
  ("read_only_space", 0x026e9): (45, "ThinOneByteStringMap"),
  ("read_only_space", 0x02711): (167, "FeedbackVectorMap"),
  ("read_only_space", 0x02749): (67, "ArgumentsMarkerMap"),
  ("read_only_space", 0x027a9): (67, "ExceptionMap"),
  ("read_only_space", 0x02805): (67, "TerminationExceptionMap"),
  ("read_only_space", 0x0286d): (67, "OptimizedOutMap"),
  ("read_only_space", 0x028cd): (67, "StaleRegisterMap"),
  ("read_only_space", 0x0292d): (130, "ScriptContextTableMap"),
  ("read_only_space", 0x02955): (128, "ClosureFeedbackCellArrayMap"),
  ("read_only_space", 0x0297d): (166, "FeedbackMetadataArrayMap"),
  ("read_only_space", 0x029a5): (118, "ArrayListMap"),
  ("read_only_space", 0x029cd): (65, "BigIntMap"),
  ("read_only_space", 0x029f5): (129, "ObjectBoilerplateDescriptionMap"),
  ("read_only_space", 0x02a1d): (132, "BytecodeArrayMap"),
  ("read_only_space", 0x02a45): (163, "CodeDataContainerMap"),
  ("read_only_space", 0x02a6d): (164, "CoverageInfoMap"),
  ("read_only_space", 0x02a95): (133, "FixedDoubleArrayMap"),
  ("read_only_space", 0x02abd): (121, "GlobalDictionaryMap"),
  ("read_only_space", 0x02ae5): (101, "ManyClosuresCellMap"),
  ("read_only_space", 0x02b0d): (173, "MegaDomHandlerMap"),
  ("read_only_space", 0x02b35): (118, "ModuleInfoMap"),
  ("read_only_space", 0x02b5d): (122, "NameDictionaryMap"),
  ("read_only_space", 0x02b85): (101, "NoClosuresCellMap"),
  ("read_only_space", 0x02bad): (123, "NumberDictionaryMap"),
  ("read_only_space", 0x02bd5): (101, "OneClosureCellMap"),
  ("read_only_space", 0x02bfd): (124, "OrderedHashMapMap"),
  ("read_only_space", 0x02c25): (125, "OrderedHashSetMap"),
  ("read_only_space", 0x02c4d): (126, "OrderedNameDictionaryMap"),
  ("read_only_space", 0x02c75): (175, "PreparseDataMap"),
  ("read_only_space", 0x02c9d): (176, "PropertyArrayMap"),
  ("read_only_space", 0x02cc5): (97, "SideEffectCallHandlerInfoMap"),
  ("read_only_space", 0x02ced): (97, "SideEffectFreeCallHandlerInfoMap"),
  ("read_only_space", 0x02d15): (97, "NextCallSideEffectFreeCallHandlerInfoMap"),
  ("read_only_space", 0x02d3d): (127, "SimpleNumberDictionaryMap"),
  ("read_only_space", 0x02d65): (149, "SmallOrderedHashMapMap"),
  ("read_only_space", 0x02d8d): (150, "SmallOrderedHashSetMap"),
  ("read_only_space", 0x02db5): (151, "SmallOrderedNameDictionaryMap"),
  ("read_only_space", 0x02ddd): (154, "SourceTextModuleMap"),
  ("read_only_space", 0x02e05): (183, "SwissNameDictionaryMap"),
  ("read_only_space", 0x02e2d): (155, "SyntheticModuleMap"),
  ("read_only_space", 0x02e55): (72, "WasmCapiFunctionDataMap"),
  ("read_only_space", 0x02e7d): (73, "WasmExportedFunctionDataMap"),
  ("read_only_space", 0x02ea5): (74, "WasmJSFunctionDataMap"),
  ("read_only_space", 0x02ecd): (75, "WasmTypeInfoMap"),
  ("read_only_space", 0x02ef5): (184, "WeakArrayListMap"),
  ("read_only_space", 0x02f1d): (120, "EphemeronHashTableMap"),
  ("read_only_space", 0x02f45): (165, "EmbedderDataArrayMap"),
  ("read_only_space", 0x02f6d): (185, "WeakCellMap"),
  ("read_only_space", 0x02f95): (32, "StringMap"),
  ("read_only_space", 0x02fbd): (41, "ConsOneByteStringMap"),
  ("read_only_space", 0x02fe5): (33, "ConsStringMap"),
  ("read_only_space", 0x0300d): (37, "ThinStringMap"),
  ("read_only_space", 0x03035): (35, "SlicedStringMap"),
  ("read_only_space", 0x0305d): (43, "SlicedOneByteStringMap"),
  ("read_only_space", 0x03085): (34, "ExternalStringMap"),
  ("read_only_space", 0x030ad): (42, "ExternalOneByteStringMap"),
  ("read_only_space", 0x030d5): (50, "UncachedExternalStringMap"),
  ("read_only_space", 0x030fd): (0, "InternalizedStringMap"),
  ("read_only_space", 0x03125): (2, "ExternalInternalizedStringMap"),
  ("read_only_space", 0x0314d): (10, "ExternalOneByteInternalizedStringMap"),
  ("read_only_space", 0x03175): (18, "UncachedExternalInternalizedStringMap"),
  ("read_only_space", 0x0319d): (26, "UncachedExternalOneByteInternalizedStringMap"),
  ("read_only_space", 0x031c5): (58, "UncachedExternalOneByteStringMap"),
  ("read_only_space", 0x031ed): (67, "SelfReferenceMarkerMap"),
  ("read_only_space", 0x03215): (67, "BasicBlockCountersMarkerMap"),
  ("read_only_space", 0x03259): (91, "ArrayBoilerplateDescriptionMap"),
  ("read_only_space", 0x03359): (103, "InterceptorInfoMap"),
  ("read_only_space", 0x05bf1): (76, "PromiseFulfillReactionJobTaskMap"),
  ("read_only_space", 0x05c19): (77, "PromiseRejectReactionJobTaskMap"),
  ("read_only_space", 0x05c41): (78, "CallableTaskMap"),
  ("read_only_space", 0x05c69): (79, "CallbackTaskMap"),
  ("read_only_space", 0x05c91): (80, "PromiseResolveThenableJobTaskMap"),
  ("read_only_space", 0x05cb9): (83, "FunctionTemplateInfoMap"),
  ("read_only_space", 0x05ce1): (84, "ObjectTemplateInfoMap"),
  ("read_only_space", 0x05d09): (85, "AccessCheckInfoMap"),
  ("read_only_space", 0x05d31): (86, "AccessorInfoMap"),
  ("read_only_space", 0x05d59): (87, "AccessorPairMap"),
  ("read_only_space", 0x05d81): (88, "AliasedArgumentsEntryMap"),
  ("read_only_space", 0x05da9): (89, "AllocationMementoMap"),
  ("read_only_space", 0x05dd1): (92, "AsmWasmDataMap"),
  ("read_only_space", 0x05df9): (93, "AsyncGeneratorRequestMap"),
  ("read_only_space", 0x05e21): (94, "BreakPointMap"),
  ("read_only_space", 0x05e49): (95, "BreakPointInfoMap"),
  ("read_only_space", 0x05e71): (96, "CachedTemplateObjectMap"),
  ("read_only_space", 0x05e99): (98, "ClassPositionsMap"),
  ("read_only_space", 0x05ec1): (99, "DebugInfoMap"),
  ("read_only_space", 0x05ee9): (102, "FunctionTemplateRareDataMap"),
  ("read_only_space", 0x05f11): (104, "InterpreterDataMap"),
  ("read_only_space", 0x05f39): (105, "ModuleRequestMap"),
  ("read_only_space", 0x05f61): (106, "PromiseCapabilityMap"),
  ("read_only_space", 0x05f89): (107, "PromiseReactionMap"),
  ("read_only_space", 0x05fb1): (108, "PropertyDescriptorObjectMap"),
  ("read_only_space", 0x05fd9): (109, "PrototypeInfoMap"),
  ("read_only_space", 0x06001): (110, "RegExpBoilerplateDescriptionMap"),
  ("read_only_space", 0x06029): (111, "ScriptMap"),
  ("read_only_space", 0x06051): (112, "SourceTextModuleInfoEntryMap"),
  ("read_only_space", 0x06079): (113, "StackFrameInfoMap"),
  ("read_only_space", 0x060a1): (114, "TemplateObjectDescriptionMap"),
  ("read_only_space", 0x060c9): (115, "Tuple2Map"),
  ("read_only_space", 0x060f1): (116, "WasmExceptionTagMap"),
  ("read_only_space", 0x06119): (117, "WasmIndirectFunctionTableMap"),
  ("read_only_space", 0x06141): (135, "SloppyArgumentsElementsMap"),
  ("read_only_space", 0x06169): (152, "DescriptorArrayMap"),
  ("read_only_space", 0x06191): (157, "UncompiledDataWithoutPreparseDataMap"),
  ("read_only_space", 0x061b9): (156, "UncompiledDataWithPreparseDataMap"),
  ("read_only_space", 0x061e1): (174, "OnHeapBasicBlockProfilerDataMap"),
  ("read_only_space", 0x06209): (170, "InternalClassMap"),
  ("read_only_space", 0x06231): (181, "SmiPairMap"),
  ("read_only_space", 0x06259): (180, "SmiBoxMap"),
  ("read_only_space", 0x06281): (146, "ExportedSubClassBaseMap"),
  ("read_only_space", 0x062a9): (147, "ExportedSubClassMap"),
  ("read_only_space", 0x062d1): (68, "AbstractInternalClassSubclass1Map"),
  ("read_only_space", 0x062f9): (69, "AbstractInternalClassSubclass2Map"),
  ("read_only_space", 0x06321): (134, "InternalClassWithSmiElementsMap"),
  ("read_only_space", 0x06349): (171, "InternalClassWithStructElementsMap"),
  ("read_only_space", 0x06371): (148, "ExportedSubClass2Map"),
  ("read_only_space", 0x06399): (182, "SortStateMap"),
  ("read_only_space", 0x063c1): (160, "CallRefDataMap"),
  ("read_only_space", 0x063e9): (90, "AllocationSiteWithWeakNextMap"),
  ("read_only_space", 0x06411): (90, "AllocationSiteWithoutWeakNextMap"),
  ("read_only_space", 0x06439): (81, "LoadHandler1Map"),
  ("read_only_space", 0x06461): (81, "LoadHandler2Map"),
  ("read_only_space", 0x06489): (81, "LoadHandler3Map"),
  ("read_only_space", 0x064b1): (82, "StoreHandler0Map"),
  ("read_only_space", 0x064d9): (82, "StoreHandler1Map"),
  ("read_only_space", 0x06501): (82, "StoreHandler2Map"),
  ("read_only_space", 0x06529): (82, "StoreHandler3Map"),
  ("map_space", 0x02119): (1057, "ExternalMap"),
  ("map_space", 0x02141): (2114, "JSMessageObjectMap"),
}

# List of known V8 objects.
KNOWN_OBJECTS = {
  ("read_only_space", 0x021b9): "EmptyWeakFixedArray",
  ("read_only_space", 0x021c1): "EmptyDescriptorArray",
  ("read_only_space", 0x021f9): "EmptyEnumCache",
  ("read_only_space", 0x0222d): "EmptyFixedArray",
  ("read_only_space", 0x02235): "NullValue",
  ("read_only_space", 0x0233d): "UninitializedValue",
  ("read_only_space", 0x023b5): "UndefinedValue",
  ("read_only_space", 0x023f9): "NanValue",
  ("read_only_space", 0x0242d): "TheHoleValue",
  ("read_only_space", 0x02459): "HoleNanValue",
  ("read_only_space", 0x0248d): "TrueValue",
  ("read_only_space", 0x024cd): "FalseValue",
  ("read_only_space", 0x024fd): "empty_string",
  ("read_only_space", 0x02739): "EmptyScopeInfo",
  ("read_only_space", 0x02771): "ArgumentsMarker",
  ("read_only_space", 0x027d1): "Exception",
  ("read_only_space", 0x0282d): "TerminationException",
  ("read_only_space", 0x02895): "OptimizedOut",
  ("read_only_space", 0x028f5): "StaleRegister",
  ("read_only_space", 0x0323d): "EmptyPropertyArray",
  ("read_only_space", 0x03245): "EmptyByteArray",
  ("read_only_space", 0x0324d): "EmptyObjectBoilerplateDescription",
  ("read_only_space", 0x03281): "EmptyArrayBoilerplateDescription",
  ("read_only_space", 0x0328d): "EmptyClosureFeedbackCellArray",
  ("read_only_space", 0x03295): "EmptySlowElementDictionary",
  ("read_only_space", 0x032b9): "EmptyOrderedHashMap",
  ("read_only_space", 0x032cd): "EmptyOrderedHashSet",
  ("read_only_space", 0x032e1): "EmptyFeedbackMetadata",
  ("read_only_space", 0x032ed): "EmptyPropertyDictionary",
  ("read_only_space", 0x03315): "EmptyOrderedPropertyDictionary",
  ("read_only_space", 0x0332d): "EmptySwissPropertyDictionary",
  ("read_only_space", 0x03381): "NoOpInterceptorInfo",
  ("read_only_space", 0x033a9): "EmptyWeakArrayList",
  ("read_only_space", 0x033b5): "InfinityValue",
  ("read_only_space", 0x033c1): "MinusZeroValue",
  ("read_only_space", 0x033cd): "MinusInfinityValue",
  ("read_only_space", 0x033d9): "SelfReferenceMarker",
  ("read_only_space", 0x03419): "BasicBlockCountersMarker",
  ("read_only_space", 0x0345d): "OffHeapTrampolineRelocationInfo",
  ("read_only_space", 0x03469): "TrampolineTrivialCodeDataContainer",
  ("read_only_space", 0x03475): "TrampolinePromiseRejectionCodeDataContainer",
  ("read_only_space", 0x03481): "GlobalThisBindingScopeInfo",
  ("read_only_space", 0x034b5): "EmptyFunctionScopeInfo",
  ("read_only_space", 0x034d9): "NativeScopeInfo",
  ("read_only_space", 0x034f1): "HashSeed",
  ("old_space", 0x04aa1): "ArgumentsIteratorAccessor",
  ("old_space", 0x04ae5): "ArrayLengthAccessor",
  ("old_space", 0x04b29): "BoundFunctionLengthAccessor",
  ("old_space", 0x04b6d): "BoundFunctionNameAccessor",
  ("old_space", 0x04bb1): "ErrorStackAccessor",
  ("old_space", 0x04bf5): "FunctionArgumentsAccessor",
  ("old_space", 0x04c39): "FunctionCallerAccessor",
  ("old_space", 0x04c7d): "FunctionNameAccessor",
  ("old_space", 0x04cc1): "FunctionLengthAccessor",
  ("old_space", 0x04d05): "FunctionPrototypeAccessor",
  ("old_space", 0x04d49): "StringLengthAccessor",
  ("old_space", 0x04d8d): "InvalidPrototypeValidityCell",
  ("old_space", 0x04d95): "EmptyScript",
  ("old_space", 0x04dd5): "ManyClosuresCell",
  ("old_space", 0x04de1): "ArrayConstructorProtector",
  ("old_space", 0x04df5): "NoElementsProtector",
  ("old_space", 0x04e09): "MegaDOMProtector",
  ("old_space", 0x04e1d): "IsConcatSpreadableProtector",
  ("old_space", 0x04e31): "ArraySpeciesProtector",
  ("old_space", 0x04e45): "TypedArraySpeciesProtector",
  ("old_space", 0x04e59): "PromiseSpeciesProtector",
  ("old_space", 0x04e6d): "RegExpSpeciesProtector",
  ("old_space", 0x04e81): "StringLengthProtector",
  ("old_space", 0x04e95): "ArrayIteratorProtector",
  ("old_space", 0x04ea9): "ArrayBufferDetachingProtector",
  ("old_space", 0x04ebd): "PromiseHookProtector",
  ("old_space", 0x04ed1): "PromiseResolveProtector",
  ("old_space", 0x04ee5): "MapIteratorProtector",
  ("old_space", 0x04ef9): "PromiseThenProtector",
  ("old_space", 0x04f0d): "SetIteratorProtector",
  ("old_space", 0x04f21): "StringIteratorProtector",
  ("old_space", 0x04f35): "SingleCharacterStringCache",
  ("old_space", 0x0533d): "StringSplitCache",
  ("old_space", 0x05745): "RegExpMultipleCache",
  ("old_space", 0x05b4d): "BuiltinsConstantsTable",
  ("old_space", 0x05f75): "AsyncFunctionAwaitRejectSharedFun",
  ("old_space", 0x05f99): "AsyncFunctionAwaitResolveSharedFun",
  ("old_space", 0x05fbd): "AsyncGeneratorAwaitRejectSharedFun",
  ("old_space", 0x05fe1): "AsyncGeneratorAwaitResolveSharedFun",
  ("old_space", 0x06005): "AsyncGeneratorYieldResolveSharedFun",
  ("old_space", 0x06029): "AsyncGeneratorReturnResolveSharedFun",
  ("old_space", 0x0604d): "AsyncGeneratorReturnClosedRejectSharedFun",
  ("old_space", 0x06071): "AsyncGeneratorReturnClosedResolveSharedFun",
  ("old_space", 0x06095): "AsyncIteratorValueUnwrapSharedFun",
  ("old_space", 0x060b9): "PromiseAllResolveElementSharedFun",
  ("old_space", 0x060dd): "PromiseAllSettledResolveElementSharedFun",
  ("old_space", 0x06101): "PromiseAllSettledRejectElementSharedFun",
  ("old_space", 0x06125): "PromiseAnyRejectElementSharedFun",
  ("old_space", 0x06149): "PromiseCapabilityDefaultRejectSharedFun",
  ("old_space", 0x0616d): "PromiseCapabilityDefaultResolveSharedFun",
  ("old_space", 0x06191): "PromiseCatchFinallySharedFun",
  ("old_space", 0x061b5): "PromiseGetCapabilitiesExecutorSharedFun",
  ("old_space", 0x061d9): "PromiseThenFinallySharedFun",
  ("old_space", 0x061fd): "PromiseThrowerFinallySharedFun",
  ("old_space", 0x06221): "PromiseValueThunkFinallySharedFun",
  ("old_space", 0x06245): "ProxyRevokeSharedFun",
}

# Lower 32 bits of first page addresses for various heap spaces.
HEAP_FIRST_PAGES = {
  0x080c0000: "old_space",
  0x08100000: "map_space",
  0x08000000: "read_only_space",
}

# List of known V8 Frame Markers.
FRAME_MARKERS = (
  "ENTRY",
  "CONSTRUCT_ENTRY",
  "EXIT",
  "WASM",
  "WASM_TO_JS",
  "JS_TO_WASM",
  "WASM_DEBUG_BREAK",
  "C_WASM_ENTRY",
  "WASM_EXIT",
  "WASM_COMPILE_LAZY",
  "INTERPRETED",
  "BASELINE",
  "OPTIMIZED",
  "STUB",
  "BUILTIN_CONTINUATION",
  "JAVA_SCRIPT_BUILTIN_CONTINUATION",
  "JAVA_SCRIPT_BUILTIN_CONTINUATION_WITH_CATCH",
  "INTERNAL",
  "CONSTRUCT",
  "BUILTIN",
  "BUILTIN_EXIT",
  "NATIVE",
)

# This set of constants is generated from a shipping build.
