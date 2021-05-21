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
  94: "BASELINE_DATA_TYPE",
  95: "BREAK_POINT_TYPE",
  96: "BREAK_POINT_INFO_TYPE",
  97: "CACHED_TEMPLATE_OBJECT_TYPE",
  98: "CALL_HANDLER_INFO_TYPE",
  99: "CLASS_POSITIONS_TYPE",
  100: "DEBUG_INFO_TYPE",
  101: "ENUM_CACHE_TYPE",
  102: "FEEDBACK_CELL_TYPE",
  103: "FUNCTION_TEMPLATE_RARE_DATA_TYPE",
  104: "INTERCEPTOR_INFO_TYPE",
  105: "INTERPRETER_DATA_TYPE",
  106: "MODULE_REQUEST_TYPE",
  107: "PROMISE_CAPABILITY_TYPE",
  108: "PROMISE_REACTION_TYPE",
  109: "PROPERTY_DESCRIPTOR_OBJECT_TYPE",
  110: "PROTOTYPE_INFO_TYPE",
  111: "REG_EXP_BOILERPLATE_DESCRIPTION_TYPE",
  112: "SCRIPT_TYPE",
  113: "SOURCE_TEXT_MODULE_INFO_ENTRY_TYPE",
  114: "STACK_FRAME_INFO_TYPE",
  115: "TEMPLATE_OBJECT_DESCRIPTION_TYPE",
  116: "TUPLE2_TYPE",
  117: "WASM_EXCEPTION_TAG_TYPE",
  118: "WASM_INDIRECT_FUNCTION_TABLE_TYPE",
  119: "FIXED_ARRAY_TYPE",
  120: "HASH_TABLE_TYPE",
  121: "EPHEMERON_HASH_TABLE_TYPE",
  122: "GLOBAL_DICTIONARY_TYPE",
  123: "NAME_DICTIONARY_TYPE",
  124: "NUMBER_DICTIONARY_TYPE",
  125: "ORDERED_HASH_MAP_TYPE",
  126: "ORDERED_HASH_SET_TYPE",
  127: "ORDERED_NAME_DICTIONARY_TYPE",
  128: "SIMPLE_NUMBER_DICTIONARY_TYPE",
  129: "CLOSURE_FEEDBACK_CELL_ARRAY_TYPE",
  130: "OBJECT_BOILERPLATE_DESCRIPTION_TYPE",
  131: "SCRIPT_CONTEXT_TABLE_TYPE",
  132: "BYTE_ARRAY_TYPE",
  133: "BYTECODE_ARRAY_TYPE",
  134: "FIXED_DOUBLE_ARRAY_TYPE",
  135: "INTERNAL_CLASS_WITH_SMI_ELEMENTS_TYPE",
  136: "SLOPPY_ARGUMENTS_ELEMENTS_TYPE",
  137: "AWAIT_CONTEXT_TYPE",
  138: "BLOCK_CONTEXT_TYPE",
  139: "CATCH_CONTEXT_TYPE",
  140: "DEBUG_EVALUATE_CONTEXT_TYPE",
  141: "EVAL_CONTEXT_TYPE",
  142: "FUNCTION_CONTEXT_TYPE",
  143: "MODULE_CONTEXT_TYPE",
  144: "NATIVE_CONTEXT_TYPE",
  145: "SCRIPT_CONTEXT_TYPE",
  146: "WITH_CONTEXT_TYPE",
  147: "EXPORTED_SUB_CLASS_BASE_TYPE",
  148: "EXPORTED_SUB_CLASS_TYPE",
  149: "EXPORTED_SUB_CLASS2_TYPE",
  150: "SMALL_ORDERED_HASH_MAP_TYPE",
  151: "SMALL_ORDERED_HASH_SET_TYPE",
  152: "SMALL_ORDERED_NAME_DICTIONARY_TYPE",
  153: "DESCRIPTOR_ARRAY_TYPE",
  154: "STRONG_DESCRIPTOR_ARRAY_TYPE",
  155: "SOURCE_TEXT_MODULE_TYPE",
  156: "SYNTHETIC_MODULE_TYPE",
  157: "UNCOMPILED_DATA_WITH_PREPARSE_DATA_TYPE",
  158: "UNCOMPILED_DATA_WITHOUT_PREPARSE_DATA_TYPE",
  159: "WASM_ARRAY_TYPE",
  160: "WASM_STRUCT_TYPE",
  161: "WEAK_FIXED_ARRAY_TYPE",
  162: "TRANSITION_ARRAY_TYPE",
  163: "CELL_TYPE",
  164: "CODE_TYPE",
  165: "CODE_DATA_CONTAINER_TYPE",
  166: "COVERAGE_INFO_TYPE",
  167: "EMBEDDER_DATA_ARRAY_TYPE",
  168: "FEEDBACK_METADATA_TYPE",
  169: "FEEDBACK_VECTOR_TYPE",
  170: "FILLER_TYPE",
  171: "FREE_SPACE_TYPE",
  172: "INTERNAL_CLASS_TYPE",
  173: "INTERNAL_CLASS_WITH_STRUCT_ELEMENTS_TYPE",
  174: "MAP_TYPE",
  175: "MEGA_DOM_HANDLER_TYPE",
  176: "ON_HEAP_BASIC_BLOCK_PROFILER_DATA_TYPE",
  177: "PREPARSE_DATA_TYPE",
  178: "PROPERTY_ARRAY_TYPE",
  179: "PROPERTY_CELL_TYPE",
  180: "SCOPE_INFO_TYPE",
  181: "SHARED_FUNCTION_INFO_TYPE",
  182: "SMI_BOX_TYPE",
  183: "SMI_PAIR_TYPE",
  184: "SORT_STATE_TYPE",
  185: "SWISS_NAME_DICTIONARY_TYPE",
  186: "WEAK_ARRAY_LIST_TYPE",
  187: "WEAK_CELL_TYPE",
  188: "JS_PROXY_TYPE",
  1057: "JS_OBJECT_TYPE",
  189: "JS_GLOBAL_OBJECT_TYPE",
  190: "JS_GLOBAL_PROXY_TYPE",
  191: "JS_MODULE_NAMESPACE_TYPE",
  1040: "JS_SPECIAL_API_OBJECT_TYPE",
  1041: "JS_PRIMITIVE_WRAPPER_TYPE",
  1042: "JS_ARRAY_ITERATOR_PROTOTYPE_TYPE",
  1043: "JS_ITERATOR_PROTOTYPE_TYPE",
  1044: "JS_MAP_ITERATOR_PROTOTYPE_TYPE",
  1045: "JS_OBJECT_PROTOTYPE_TYPE",
  1046: "JS_PROMISE_PROTOTYPE_TYPE",
  1047: "JS_REG_EXP_PROTOTYPE_TYPE",
  1048: "JS_SET_ITERATOR_PROTOTYPE_TYPE",
  1049: "JS_SET_PROTOTYPE_TYPE",
  1050: "JS_STRING_ITERATOR_PROTOTYPE_TYPE",
  1051: "JS_TYPED_ARRAY_PROTOTYPE_TYPE",
  1052: "JS_GENERATOR_OBJECT_TYPE",
  1053: "JS_ASYNC_FUNCTION_OBJECT_TYPE",
  1054: "JS_ASYNC_GENERATOR_OBJECT_TYPE",
  1055: "JS_ARGUMENTS_OBJECT_TYPE",
  1056: "JS_API_OBJECT_TYPE",
  1058: "JS_BOUND_FUNCTION_TYPE",
  1059: "JS_FUNCTION_TYPE",
  1060: "BIGINT64_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  1061: "BIGUINT64_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  1062: "FLOAT32_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  1063: "FLOAT64_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  1064: "INT16_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  1065: "INT32_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  1066: "INT8_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  1067: "UINT16_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  1068: "UINT32_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  1069: "UINT8_CLAMPED_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  1070: "UINT8_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  1071: "JS_ARRAY_CONSTRUCTOR_TYPE",
  1072: "JS_PROMISE_CONSTRUCTOR_TYPE",
  1073: "JS_REG_EXP_CONSTRUCTOR_TYPE",
  1074: "JS_MAP_KEY_ITERATOR_TYPE",
  1075: "JS_MAP_KEY_VALUE_ITERATOR_TYPE",
  1076: "JS_MAP_VALUE_ITERATOR_TYPE",
  1077: "JS_SET_KEY_VALUE_ITERATOR_TYPE",
  1078: "JS_SET_VALUE_ITERATOR_TYPE",
  1079: "JS_DATA_VIEW_TYPE",
  1080: "JS_TYPED_ARRAY_TYPE",
  1081: "JS_MAP_TYPE",
  1082: "JS_SET_TYPE",
  1083: "JS_WEAK_MAP_TYPE",
  1084: "JS_WEAK_SET_TYPE",
  1085: "JS_ARRAY_TYPE",
  1086: "JS_ARRAY_BUFFER_TYPE",
  1087: "JS_ARRAY_ITERATOR_TYPE",
  1088: "JS_ASYNC_FROM_SYNC_ITERATOR_TYPE",
  1089: "JS_COLLATOR_TYPE",
  1090: "JS_CONTEXT_EXTENSION_OBJECT_TYPE",
  1091: "JS_DATE_TYPE",
  1092: "JS_DATE_TIME_FORMAT_TYPE",
  1093: "JS_DISPLAY_NAMES_TYPE",
  1094: "JS_ERROR_TYPE",
  1095: "JS_FINALIZATION_REGISTRY_TYPE",
  1096: "JS_LIST_FORMAT_TYPE",
  1097: "JS_LOCALE_TYPE",
  1098: "JS_MESSAGE_OBJECT_TYPE",
  1099: "JS_NUMBER_FORMAT_TYPE",
  1100: "JS_PLURAL_RULES_TYPE",
  1101: "JS_PROMISE_TYPE",
  1102: "JS_REG_EXP_TYPE",
  1103: "JS_REG_EXP_STRING_ITERATOR_TYPE",
  1104: "JS_RELATIVE_TIME_FORMAT_TYPE",
  1105: "JS_SEGMENT_ITERATOR_TYPE",
  1106: "JS_SEGMENTER_TYPE",
  1107: "JS_SEGMENTS_TYPE",
  1108: "JS_STRING_ITERATOR_TYPE",
  1109: "JS_V8_BREAK_ITERATOR_TYPE",
  1110: "JS_WEAK_REF_TYPE",
  1111: "WASM_EXCEPTION_OBJECT_TYPE",
  1112: "WASM_GLOBAL_OBJECT_TYPE",
  1113: "WASM_INSTANCE_OBJECT_TYPE",
  1114: "WASM_MEMORY_OBJECT_TYPE",
  1115: "WASM_MODULE_OBJECT_TYPE",
  1116: "WASM_TABLE_OBJECT_TYPE",
  1117: "WASM_VALUE_OBJECT_TYPE",
}

# List of known V8 maps.
KNOWN_MAPS = {
  ("read_only_space", 0x02119): (174, "MetaMap"),
  ("read_only_space", 0x02141): (67, "NullMap"),
  ("read_only_space", 0x02169): (154, "StrongDescriptorArrayMap"),
  ("read_only_space", 0x02191): (161, "WeakFixedArrayMap"),
  ("read_only_space", 0x021d1): (101, "EnumCacheMap"),
  ("read_only_space", 0x02205): (119, "FixedArrayMap"),
  ("read_only_space", 0x02251): (8, "OneByteInternalizedStringMap"),
  ("read_only_space", 0x0229d): (171, "FreeSpaceMap"),
  ("read_only_space", 0x022c5): (170, "OnePointerFillerMap"),
  ("read_only_space", 0x022ed): (170, "TwoPointerFillerMap"),
  ("read_only_space", 0x02315): (67, "UninitializedMap"),
  ("read_only_space", 0x0238d): (67, "UndefinedMap"),
  ("read_only_space", 0x023d1): (66, "HeapNumberMap"),
  ("read_only_space", 0x02405): (67, "TheHoleMap"),
  ("read_only_space", 0x02465): (67, "BooleanMap"),
  ("read_only_space", 0x02509): (132, "ByteArrayMap"),
  ("read_only_space", 0x02531): (119, "FixedCOWArrayMap"),
  ("read_only_space", 0x02559): (120, "HashTableMap"),
  ("read_only_space", 0x02581): (64, "SymbolMap"),
  ("read_only_space", 0x025a9): (40, "OneByteStringMap"),
  ("read_only_space", 0x025d1): (180, "ScopeInfoMap"),
  ("read_only_space", 0x025f9): (181, "SharedFunctionInfoMap"),
  ("read_only_space", 0x02621): (164, "CodeMap"),
  ("read_only_space", 0x02649): (163, "CellMap"),
  ("read_only_space", 0x02671): (179, "GlobalPropertyCellMap"),
  ("read_only_space", 0x02699): (70, "ForeignMap"),
  ("read_only_space", 0x026c1): (162, "TransitionArrayMap"),
  ("read_only_space", 0x026e9): (45, "ThinOneByteStringMap"),
  ("read_only_space", 0x02711): (169, "FeedbackVectorMap"),
  ("read_only_space", 0x02749): (67, "ArgumentsMarkerMap"),
  ("read_only_space", 0x027a9): (67, "ExceptionMap"),
  ("read_only_space", 0x02805): (67, "TerminationExceptionMap"),
  ("read_only_space", 0x0286d): (67, "OptimizedOutMap"),
  ("read_only_space", 0x028cd): (67, "StaleRegisterMap"),
  ("read_only_space", 0x0292d): (131, "ScriptContextTableMap"),
  ("read_only_space", 0x02955): (129, "ClosureFeedbackCellArrayMap"),
  ("read_only_space", 0x0297d): (168, "FeedbackMetadataArrayMap"),
  ("read_only_space", 0x029a5): (119, "ArrayListMap"),
  ("read_only_space", 0x029cd): (65, "BigIntMap"),
  ("read_only_space", 0x029f5): (130, "ObjectBoilerplateDescriptionMap"),
  ("read_only_space", 0x02a1d): (133, "BytecodeArrayMap"),
  ("read_only_space", 0x02a45): (165, "CodeDataContainerMap"),
  ("read_only_space", 0x02a6d): (166, "CoverageInfoMap"),
  ("read_only_space", 0x02a95): (134, "FixedDoubleArrayMap"),
  ("read_only_space", 0x02abd): (122, "GlobalDictionaryMap"),
  ("read_only_space", 0x02ae5): (102, "ManyClosuresCellMap"),
  ("read_only_space", 0x02b0d): (175, "MegaDomHandlerMap"),
  ("read_only_space", 0x02b35): (119, "ModuleInfoMap"),
  ("read_only_space", 0x02b5d): (123, "NameDictionaryMap"),
  ("read_only_space", 0x02b85): (102, "NoClosuresCellMap"),
  ("read_only_space", 0x02bad): (124, "NumberDictionaryMap"),
  ("read_only_space", 0x02bd5): (102, "OneClosureCellMap"),
  ("read_only_space", 0x02bfd): (125, "OrderedHashMapMap"),
  ("read_only_space", 0x02c25): (126, "OrderedHashSetMap"),
  ("read_only_space", 0x02c4d): (127, "OrderedNameDictionaryMap"),
  ("read_only_space", 0x02c75): (177, "PreparseDataMap"),
  ("read_only_space", 0x02c9d): (178, "PropertyArrayMap"),
  ("read_only_space", 0x02cc5): (98, "SideEffectCallHandlerInfoMap"),
  ("read_only_space", 0x02ced): (98, "SideEffectFreeCallHandlerInfoMap"),
  ("read_only_space", 0x02d15): (98, "NextCallSideEffectFreeCallHandlerInfoMap"),
  ("read_only_space", 0x02d3d): (128, "SimpleNumberDictionaryMap"),
  ("read_only_space", 0x02d65): (150, "SmallOrderedHashMapMap"),
  ("read_only_space", 0x02d8d): (151, "SmallOrderedHashSetMap"),
  ("read_only_space", 0x02db5): (152, "SmallOrderedNameDictionaryMap"),
  ("read_only_space", 0x02ddd): (155, "SourceTextModuleMap"),
  ("read_only_space", 0x02e05): (185, "SwissNameDictionaryMap"),
  ("read_only_space", 0x02e2d): (156, "SyntheticModuleMap"),
  ("read_only_space", 0x02e55): (72, "WasmCapiFunctionDataMap"),
  ("read_only_space", 0x02e7d): (73, "WasmExportedFunctionDataMap"),
  ("read_only_space", 0x02ea5): (74, "WasmJSFunctionDataMap"),
  ("read_only_space", 0x02ecd): (75, "WasmTypeInfoMap"),
  ("read_only_space", 0x02ef5): (186, "WeakArrayListMap"),
  ("read_only_space", 0x02f1d): (121, "EphemeronHashTableMap"),
  ("read_only_space", 0x02f45): (167, "EmbedderDataArrayMap"),
  ("read_only_space", 0x02f6d): (187, "WeakCellMap"),
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
  ("read_only_space", 0x03359): (104, "InterceptorInfoMap"),
  ("read_only_space", 0x05661): (76, "PromiseFulfillReactionJobTaskMap"),
  ("read_only_space", 0x05689): (77, "PromiseRejectReactionJobTaskMap"),
  ("read_only_space", 0x056b1): (78, "CallableTaskMap"),
  ("read_only_space", 0x056d9): (79, "CallbackTaskMap"),
  ("read_only_space", 0x05701): (80, "PromiseResolveThenableJobTaskMap"),
  ("read_only_space", 0x05729): (83, "FunctionTemplateInfoMap"),
  ("read_only_space", 0x05751): (84, "ObjectTemplateInfoMap"),
  ("read_only_space", 0x05779): (85, "AccessCheckInfoMap"),
  ("read_only_space", 0x057a1): (86, "AccessorInfoMap"),
  ("read_only_space", 0x057c9): (87, "AccessorPairMap"),
  ("read_only_space", 0x057f1): (88, "AliasedArgumentsEntryMap"),
  ("read_only_space", 0x05819): (89, "AllocationMementoMap"),
  ("read_only_space", 0x05841): (92, "AsmWasmDataMap"),
  ("read_only_space", 0x05869): (93, "AsyncGeneratorRequestMap"),
  ("read_only_space", 0x05891): (94, "BaselineDataMap"),
  ("read_only_space", 0x058b9): (95, "BreakPointMap"),
  ("read_only_space", 0x058e1): (96, "BreakPointInfoMap"),
  ("read_only_space", 0x05909): (97, "CachedTemplateObjectMap"),
  ("read_only_space", 0x05931): (99, "ClassPositionsMap"),
  ("read_only_space", 0x05959): (100, "DebugInfoMap"),
  ("read_only_space", 0x05981): (103, "FunctionTemplateRareDataMap"),
  ("read_only_space", 0x059a9): (105, "InterpreterDataMap"),
  ("read_only_space", 0x059d1): (106, "ModuleRequestMap"),
  ("read_only_space", 0x059f9): (107, "PromiseCapabilityMap"),
  ("read_only_space", 0x05a21): (108, "PromiseReactionMap"),
  ("read_only_space", 0x05a49): (109, "PropertyDescriptorObjectMap"),
  ("read_only_space", 0x05a71): (110, "PrototypeInfoMap"),
  ("read_only_space", 0x05a99): (111, "RegExpBoilerplateDescriptionMap"),
  ("read_only_space", 0x05ac1): (112, "ScriptMap"),
  ("read_only_space", 0x05ae9): (113, "SourceTextModuleInfoEntryMap"),
  ("read_only_space", 0x05b11): (114, "StackFrameInfoMap"),
  ("read_only_space", 0x05b39): (115, "TemplateObjectDescriptionMap"),
  ("read_only_space", 0x05b61): (116, "Tuple2Map"),
  ("read_only_space", 0x05b89): (117, "WasmExceptionTagMap"),
  ("read_only_space", 0x05bb1): (118, "WasmIndirectFunctionTableMap"),
  ("read_only_space", 0x05bd9): (136, "SloppyArgumentsElementsMap"),
  ("read_only_space", 0x05c01): (153, "DescriptorArrayMap"),
  ("read_only_space", 0x05c29): (158, "UncompiledDataWithoutPreparseDataMap"),
  ("read_only_space", 0x05c51): (157, "UncompiledDataWithPreparseDataMap"),
  ("read_only_space", 0x05c79): (176, "OnHeapBasicBlockProfilerDataMap"),
  ("read_only_space", 0x05ca1): (172, "InternalClassMap"),
  ("read_only_space", 0x05cc9): (183, "SmiPairMap"),
  ("read_only_space", 0x05cf1): (182, "SmiBoxMap"),
  ("read_only_space", 0x05d19): (147, "ExportedSubClassBaseMap"),
  ("read_only_space", 0x05d41): (148, "ExportedSubClassMap"),
  ("read_only_space", 0x05d69): (68, "AbstractInternalClassSubclass1Map"),
  ("read_only_space", 0x05d91): (69, "AbstractInternalClassSubclass2Map"),
  ("read_only_space", 0x05db9): (135, "InternalClassWithSmiElementsMap"),
  ("read_only_space", 0x05de1): (173, "InternalClassWithStructElementsMap"),
  ("read_only_space", 0x05e09): (149, "ExportedSubClass2Map"),
  ("read_only_space", 0x05e31): (184, "SortStateMap"),
  ("read_only_space", 0x05e59): (90, "AllocationSiteWithWeakNextMap"),
  ("read_only_space", 0x05e81): (90, "AllocationSiteWithoutWeakNextMap"),
  ("read_only_space", 0x05ea9): (81, "LoadHandler1Map"),
  ("read_only_space", 0x05ed1): (81, "LoadHandler2Map"),
  ("read_only_space", 0x05ef9): (81, "LoadHandler3Map"),
  ("read_only_space", 0x05f21): (82, "StoreHandler0Map"),
  ("read_only_space", 0x05f49): (82, "StoreHandler1Map"),
  ("read_only_space", 0x05f71): (82, "StoreHandler2Map"),
  ("read_only_space", 0x05f99): (82, "StoreHandler3Map"),
  ("map_space", 0x02119): (1057, "ExternalMap"),
  ("map_space", 0x02141): (1098, "JSMessageObjectMap"),
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
  ("old_space", 0x02119): "ArgumentsIteratorAccessor",
  ("old_space", 0x0215d): "ArrayLengthAccessor",
  ("old_space", 0x021a1): "BoundFunctionLengthAccessor",
  ("old_space", 0x021e5): "BoundFunctionNameAccessor",
  ("old_space", 0x02229): "ErrorStackAccessor",
  ("old_space", 0x0226d): "FunctionArgumentsAccessor",
  ("old_space", 0x022b1): "FunctionCallerAccessor",
  ("old_space", 0x022f5): "FunctionNameAccessor",
  ("old_space", 0x02339): "FunctionLengthAccessor",
  ("old_space", 0x0237d): "FunctionPrototypeAccessor",
  ("old_space", 0x023c1): "StringLengthAccessor",
  ("old_space", 0x02405): "InvalidPrototypeValidityCell",
  ("old_space", 0x0240d): "EmptyScript",
  ("old_space", 0x0244d): "ManyClosuresCell",
  ("old_space", 0x02459): "ArrayConstructorProtector",
  ("old_space", 0x0246d): "NoElementsProtector",
  ("old_space", 0x02481): "MegaDOMProtector",
  ("old_space", 0x02495): "IsConcatSpreadableProtector",
  ("old_space", 0x024a9): "ArraySpeciesProtector",
  ("old_space", 0x024bd): "TypedArraySpeciesProtector",
  ("old_space", 0x024d1): "PromiseSpeciesProtector",
  ("old_space", 0x024e5): "RegExpSpeciesProtector",
  ("old_space", 0x024f9): "StringLengthProtector",
  ("old_space", 0x0250d): "ArrayIteratorProtector",
  ("old_space", 0x02521): "ArrayBufferDetachingProtector",
  ("old_space", 0x02535): "PromiseHookProtector",
  ("old_space", 0x02549): "PromiseResolveProtector",
  ("old_space", 0x0255d): "MapIteratorProtector",
  ("old_space", 0x02571): "PromiseThenProtector",
  ("old_space", 0x02585): "SetIteratorProtector",
  ("old_space", 0x02599): "StringIteratorProtector",
  ("old_space", 0x025ad): "SingleCharacterStringCache",
  ("old_space", 0x029b5): "StringSplitCache",
  ("old_space", 0x02dbd): "RegExpMultipleCache",
  ("old_space", 0x031c5): "BuiltinsConstantsTable",
  ("old_space", 0x035d1): "AsyncFunctionAwaitRejectSharedFun",
  ("old_space", 0x035f5): "AsyncFunctionAwaitResolveSharedFun",
  ("old_space", 0x03619): "AsyncGeneratorAwaitRejectSharedFun",
  ("old_space", 0x0363d): "AsyncGeneratorAwaitResolveSharedFun",
  ("old_space", 0x03661): "AsyncGeneratorYieldResolveSharedFun",
  ("old_space", 0x03685): "AsyncGeneratorReturnResolveSharedFun",
  ("old_space", 0x036a9): "AsyncGeneratorReturnClosedRejectSharedFun",
  ("old_space", 0x036cd): "AsyncGeneratorReturnClosedResolveSharedFun",
  ("old_space", 0x036f1): "AsyncIteratorValueUnwrapSharedFun",
  ("old_space", 0x03715): "PromiseAllResolveElementSharedFun",
  ("old_space", 0x03739): "PromiseAllSettledResolveElementSharedFun",
  ("old_space", 0x0375d): "PromiseAllSettledRejectElementSharedFun",
  ("old_space", 0x03781): "PromiseAnyRejectElementSharedFun",
  ("old_space", 0x037a5): "PromiseCapabilityDefaultRejectSharedFun",
  ("old_space", 0x037c9): "PromiseCapabilityDefaultResolveSharedFun",
  ("old_space", 0x037ed): "PromiseCatchFinallySharedFun",
  ("old_space", 0x03811): "PromiseGetCapabilitiesExecutorSharedFun",
  ("old_space", 0x03835): "PromiseThenFinallySharedFun",
  ("old_space", 0x03859): "PromiseThrowerFinallySharedFun",
  ("old_space", 0x0387d): "PromiseValueThunkFinallySharedFun",
  ("old_space", 0x038a1): "ProxyRevokeSharedFun",
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
