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
  71: "WASM_TYPE_INFO_TYPE",
  72: "PROMISE_FULFILL_REACTION_JOB_TASK_TYPE",
  73: "PROMISE_REJECT_REACTION_JOB_TASK_TYPE",
  74: "CALLABLE_TASK_TYPE",
  75: "CALLBACK_TASK_TYPE",
  76: "PROMISE_RESOLVE_THENABLE_JOB_TASK_TYPE",
  77: "LOAD_HANDLER_TYPE",
  78: "STORE_HANDLER_TYPE",
  79: "FUNCTION_TEMPLATE_INFO_TYPE",
  80: "OBJECT_TEMPLATE_INFO_TYPE",
  81: "ACCESS_CHECK_INFO_TYPE",
  82: "ACCESSOR_INFO_TYPE",
  83: "ACCESSOR_PAIR_TYPE",
  84: "ALIASED_ARGUMENTS_ENTRY_TYPE",
  85: "ALLOCATION_MEMENTO_TYPE",
  86: "ALLOCATION_SITE_TYPE",
  87: "ARRAY_BOILERPLATE_DESCRIPTION_TYPE",
  88: "ASM_WASM_DATA_TYPE",
  89: "ASYNC_GENERATOR_REQUEST_TYPE",
  90: "BREAK_POINT_TYPE",
  91: "BREAK_POINT_INFO_TYPE",
  92: "CACHED_TEMPLATE_OBJECT_TYPE",
  93: "CALL_HANDLER_INFO_TYPE",
  94: "CLASS_POSITIONS_TYPE",
  95: "DEBUG_INFO_TYPE",
  96: "ENUM_CACHE_TYPE",
  97: "FEEDBACK_CELL_TYPE",
  98: "FUNCTION_TEMPLATE_RARE_DATA_TYPE",
  99: "INTERCEPTOR_INFO_TYPE",
  100: "INTERPRETER_DATA_TYPE",
  101: "PROMISE_CAPABILITY_TYPE",
  102: "PROMISE_REACTION_TYPE",
  103: "PROPERTY_DESCRIPTOR_OBJECT_TYPE",
  104: "PROTOTYPE_INFO_TYPE",
  105: "SCRIPT_TYPE",
  106: "SOURCE_TEXT_MODULE_INFO_ENTRY_TYPE",
  107: "STACK_FRAME_INFO_TYPE",
  108: "STACK_TRACE_FRAME_TYPE",
  109: "TEMPLATE_OBJECT_DESCRIPTION_TYPE",
  110: "TUPLE2_TYPE",
  111: "WASM_CAPI_FUNCTION_DATA_TYPE",
  112: "WASM_EXCEPTION_TAG_TYPE",
  113: "WASM_EXPORTED_FUNCTION_DATA_TYPE",
  114: "WASM_INDIRECT_FUNCTION_TABLE_TYPE",
  115: "WASM_JS_FUNCTION_DATA_TYPE",
  116: "WASM_VALUE_TYPE",
  117: "FIXED_ARRAY_TYPE",
  118: "HASH_TABLE_TYPE",
  119: "EPHEMERON_HASH_TABLE_TYPE",
  120: "GLOBAL_DICTIONARY_TYPE",
  121: "NAME_DICTIONARY_TYPE",
  122: "NUMBER_DICTIONARY_TYPE",
  123: "ORDERED_HASH_MAP_TYPE",
  124: "ORDERED_HASH_SET_TYPE",
  125: "ORDERED_NAME_DICTIONARY_TYPE",
  126: "SIMPLE_NUMBER_DICTIONARY_TYPE",
  127: "STRING_TABLE_TYPE",
  128: "CLOSURE_FEEDBACK_CELL_ARRAY_TYPE",
  129: "OBJECT_BOILERPLATE_DESCRIPTION_TYPE",
  130: "SCOPE_INFO_TYPE",
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
  153: "SOURCE_TEXT_MODULE_TYPE",
  154: "SYNTHETIC_MODULE_TYPE",
  155: "UNCOMPILED_DATA_WITH_PREPARSE_DATA_TYPE",
  156: "UNCOMPILED_DATA_WITHOUT_PREPARSE_DATA_TYPE",
  157: "WEAK_FIXED_ARRAY_TYPE",
  158: "TRANSITION_ARRAY_TYPE",
  159: "CELL_TYPE",
  160: "CODE_TYPE",
  161: "CODE_DATA_CONTAINER_TYPE",
  162: "COVERAGE_INFO_TYPE",
  163: "DESCRIPTOR_ARRAY_TYPE",
  164: "EMBEDDER_DATA_ARRAY_TYPE",
  165: "FEEDBACK_METADATA_TYPE",
  166: "FEEDBACK_VECTOR_TYPE",
  167: "FILLER_TYPE",
  168: "FREE_SPACE_TYPE",
  169: "INTERNAL_CLASS_TYPE",
  170: "INTERNAL_CLASS_WITH_STRUCT_ELEMENTS_TYPE",
  171: "MAP_TYPE",
  172: "ON_HEAP_BASIC_BLOCK_PROFILER_DATA_TYPE",
  173: "PREPARSE_DATA_TYPE",
  174: "PROPERTY_ARRAY_TYPE",
  175: "PROPERTY_CELL_TYPE",
  176: "SHARED_FUNCTION_INFO_TYPE",
  177: "SMI_BOX_TYPE",
  178: "SMI_PAIR_TYPE",
  179: "SORT_STATE_TYPE",
  180: "WASM_ARRAY_TYPE",
  181: "WASM_STRUCT_TYPE",
  182: "WEAK_ARRAY_LIST_TYPE",
  183: "WEAK_CELL_TYPE",
  184: "JS_PROXY_TYPE",
  1057: "JS_OBJECT_TYPE",
  185: "JS_GLOBAL_OBJECT_TYPE",
  186: "JS_GLOBAL_PROXY_TYPE",
  187: "JS_MODULE_NAMESPACE_TYPE",
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
  1069: "JS_FINALIZATION_REGISTRY_TYPE",
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
  ("read_only_space", 0x00121): (168, "FreeSpaceMap"),
  ("read_only_space", 0x00149): (171, "MetaMap"),
  ("read_only_space", 0x0018d): (67, "NullMap"),
  ("read_only_space", 0x001c5): (163, "DescriptorArrayMap"),
  ("read_only_space", 0x001f5): (157, "WeakFixedArrayMap"),
  ("read_only_space", 0x0021d): (167, "OnePointerFillerMap"),
  ("read_only_space", 0x00245): (167, "TwoPointerFillerMap"),
  ("read_only_space", 0x00289): (67, "UninitializedMap"),
  ("read_only_space", 0x002cd): (8, "OneByteInternalizedStringMap"),
  ("read_only_space", 0x00329): (67, "UndefinedMap"),
  ("read_only_space", 0x0035d): (66, "HeapNumberMap"),
  ("read_only_space", 0x003a1): (67, "TheHoleMap"),
  ("read_only_space", 0x00401): (67, "BooleanMap"),
  ("read_only_space", 0x00489): (132, "ByteArrayMap"),
  ("read_only_space", 0x004b1): (117, "FixedArrayMap"),
  ("read_only_space", 0x004d9): (117, "FixedCOWArrayMap"),
  ("read_only_space", 0x00501): (118, "HashTableMap"),
  ("read_only_space", 0x00529): (64, "SymbolMap"),
  ("read_only_space", 0x00551): (40, "OneByteStringMap"),
  ("read_only_space", 0x00579): (130, "ScopeInfoMap"),
  ("read_only_space", 0x005a1): (176, "SharedFunctionInfoMap"),
  ("read_only_space", 0x005c9): (160, "CodeMap"),
  ("read_only_space", 0x005f1): (159, "CellMap"),
  ("read_only_space", 0x00619): (175, "GlobalPropertyCellMap"),
  ("read_only_space", 0x00641): (70, "ForeignMap"),
  ("read_only_space", 0x00669): (158, "TransitionArrayMap"),
  ("read_only_space", 0x00691): (45, "ThinOneByteStringMap"),
  ("read_only_space", 0x006b9): (166, "FeedbackVectorMap"),
  ("read_only_space", 0x0070d): (67, "ArgumentsMarkerMap"),
  ("read_only_space", 0x0076d): (67, "ExceptionMap"),
  ("read_only_space", 0x007c9): (67, "TerminationExceptionMap"),
  ("read_only_space", 0x00831): (67, "OptimizedOutMap"),
  ("read_only_space", 0x00891): (67, "StaleRegisterMap"),
  ("read_only_space", 0x008d5): (131, "ScriptContextTableMap"),
  ("read_only_space", 0x008fd): (128, "ClosureFeedbackCellArrayMap"),
  ("read_only_space", 0x00925): (165, "FeedbackMetadataArrayMap"),
  ("read_only_space", 0x0094d): (117, "ArrayListMap"),
  ("read_only_space", 0x00975): (65, "BigIntMap"),
  ("read_only_space", 0x0099d): (129, "ObjectBoilerplateDescriptionMap"),
  ("read_only_space", 0x009c5): (133, "BytecodeArrayMap"),
  ("read_only_space", 0x009ed): (161, "CodeDataContainerMap"),
  ("read_only_space", 0x00a15): (162, "CoverageInfoMap"),
  ("read_only_space", 0x00a3d): (134, "FixedDoubleArrayMap"),
  ("read_only_space", 0x00a65): (120, "GlobalDictionaryMap"),
  ("read_only_space", 0x00a8d): (97, "ManyClosuresCellMap"),
  ("read_only_space", 0x00ab5): (117, "ModuleInfoMap"),
  ("read_only_space", 0x00add): (121, "NameDictionaryMap"),
  ("read_only_space", 0x00b05): (97, "NoClosuresCellMap"),
  ("read_only_space", 0x00b2d): (122, "NumberDictionaryMap"),
  ("read_only_space", 0x00b55): (97, "OneClosureCellMap"),
  ("read_only_space", 0x00b7d): (123, "OrderedHashMapMap"),
  ("read_only_space", 0x00ba5): (124, "OrderedHashSetMap"),
  ("read_only_space", 0x00bcd): (125, "OrderedNameDictionaryMap"),
  ("read_only_space", 0x00bf5): (173, "PreparseDataMap"),
  ("read_only_space", 0x00c1d): (174, "PropertyArrayMap"),
  ("read_only_space", 0x00c45): (93, "SideEffectCallHandlerInfoMap"),
  ("read_only_space", 0x00c6d): (93, "SideEffectFreeCallHandlerInfoMap"),
  ("read_only_space", 0x00c95): (93, "NextCallSideEffectFreeCallHandlerInfoMap"),
  ("read_only_space", 0x00cbd): (126, "SimpleNumberDictionaryMap"),
  ("read_only_space", 0x00ce5): (150, "SmallOrderedHashMapMap"),
  ("read_only_space", 0x00d0d): (151, "SmallOrderedHashSetMap"),
  ("read_only_space", 0x00d35): (152, "SmallOrderedNameDictionaryMap"),
  ("read_only_space", 0x00d5d): (153, "SourceTextModuleMap"),
  ("read_only_space", 0x00d85): (127, "StringTableMap"),
  ("read_only_space", 0x00dad): (154, "SyntheticModuleMap"),
  ("read_only_space", 0x00dd5): (156, "UncompiledDataWithoutPreparseDataMap"),
  ("read_only_space", 0x00dfd): (155, "UncompiledDataWithPreparseDataMap"),
  ("read_only_space", 0x00e25): (71, "WasmTypeInfoMap"),
  ("read_only_space", 0x00e4d): (182, "WeakArrayListMap"),
  ("read_only_space", 0x00e75): (119, "EphemeronHashTableMap"),
  ("read_only_space", 0x00e9d): (164, "EmbedderDataArrayMap"),
  ("read_only_space", 0x00ec5): (183, "WeakCellMap"),
  ("read_only_space", 0x00eed): (32, "StringMap"),
  ("read_only_space", 0x00f15): (41, "ConsOneByteStringMap"),
  ("read_only_space", 0x00f3d): (33, "ConsStringMap"),
  ("read_only_space", 0x00f65): (37, "ThinStringMap"),
  ("read_only_space", 0x00f8d): (35, "SlicedStringMap"),
  ("read_only_space", 0x00fb5): (43, "SlicedOneByteStringMap"),
  ("read_only_space", 0x00fdd): (34, "ExternalStringMap"),
  ("read_only_space", 0x01005): (42, "ExternalOneByteStringMap"),
  ("read_only_space", 0x0102d): (50, "UncachedExternalStringMap"),
  ("read_only_space", 0x01055): (0, "InternalizedStringMap"),
  ("read_only_space", 0x0107d): (2, "ExternalInternalizedStringMap"),
  ("read_only_space", 0x010a5): (10, "ExternalOneByteInternalizedStringMap"),
  ("read_only_space", 0x010cd): (18, "UncachedExternalInternalizedStringMap"),
  ("read_only_space", 0x010f5): (26, "UncachedExternalOneByteInternalizedStringMap"),
  ("read_only_space", 0x0111d): (58, "UncachedExternalOneByteStringMap"),
  ("read_only_space", 0x01145): (67, "SelfReferenceMarkerMap"),
  ("read_only_space", 0x0116d): (67, "BasicBlockCountersMarkerMap"),
  ("read_only_space", 0x011a1): (96, "EnumCacheMap"),
  ("read_only_space", 0x011f1): (87, "ArrayBoilerplateDescriptionMap"),
  ("read_only_space", 0x012dd): (99, "InterceptorInfoMap"),
  ("read_only_space", 0x03391): (72, "PromiseFulfillReactionJobTaskMap"),
  ("read_only_space", 0x033b9): (73, "PromiseRejectReactionJobTaskMap"),
  ("read_only_space", 0x033e1): (74, "CallableTaskMap"),
  ("read_only_space", 0x03409): (75, "CallbackTaskMap"),
  ("read_only_space", 0x03431): (76, "PromiseResolveThenableJobTaskMap"),
  ("read_only_space", 0x03459): (79, "FunctionTemplateInfoMap"),
  ("read_only_space", 0x03481): (80, "ObjectTemplateInfoMap"),
  ("read_only_space", 0x034a9): (81, "AccessCheckInfoMap"),
  ("read_only_space", 0x034d1): (82, "AccessorInfoMap"),
  ("read_only_space", 0x034f9): (83, "AccessorPairMap"),
  ("read_only_space", 0x03521): (84, "AliasedArgumentsEntryMap"),
  ("read_only_space", 0x03549): (85, "AllocationMementoMap"),
  ("read_only_space", 0x03571): (88, "AsmWasmDataMap"),
  ("read_only_space", 0x03599): (89, "AsyncGeneratorRequestMap"),
  ("read_only_space", 0x035c1): (90, "BreakPointMap"),
  ("read_only_space", 0x035e9): (91, "BreakPointInfoMap"),
  ("read_only_space", 0x03611): (92, "CachedTemplateObjectMap"),
  ("read_only_space", 0x03639): (94, "ClassPositionsMap"),
  ("read_only_space", 0x03661): (95, "DebugInfoMap"),
  ("read_only_space", 0x03689): (98, "FunctionTemplateRareDataMap"),
  ("read_only_space", 0x036b1): (100, "InterpreterDataMap"),
  ("read_only_space", 0x036d9): (101, "PromiseCapabilityMap"),
  ("read_only_space", 0x03701): (102, "PromiseReactionMap"),
  ("read_only_space", 0x03729): (103, "PropertyDescriptorObjectMap"),
  ("read_only_space", 0x03751): (104, "PrototypeInfoMap"),
  ("read_only_space", 0x03779): (105, "ScriptMap"),
  ("read_only_space", 0x037a1): (106, "SourceTextModuleInfoEntryMap"),
  ("read_only_space", 0x037c9): (107, "StackFrameInfoMap"),
  ("read_only_space", 0x037f1): (108, "StackTraceFrameMap"),
  ("read_only_space", 0x03819): (109, "TemplateObjectDescriptionMap"),
  ("read_only_space", 0x03841): (110, "Tuple2Map"),
  ("read_only_space", 0x03869): (111, "WasmCapiFunctionDataMap"),
  ("read_only_space", 0x03891): (112, "WasmExceptionTagMap"),
  ("read_only_space", 0x038b9): (113, "WasmExportedFunctionDataMap"),
  ("read_only_space", 0x038e1): (114, "WasmIndirectFunctionTableMap"),
  ("read_only_space", 0x03909): (115, "WasmJSFunctionDataMap"),
  ("read_only_space", 0x03931): (116, "WasmValueMap"),
  ("read_only_space", 0x03959): (136, "SloppyArgumentsElementsMap"),
  ("read_only_space", 0x03981): (172, "OnHeapBasicBlockProfilerDataMap"),
  ("read_only_space", 0x039a9): (169, "InternalClassMap"),
  ("read_only_space", 0x039d1): (178, "SmiPairMap"),
  ("read_only_space", 0x039f9): (177, "SmiBoxMap"),
  ("read_only_space", 0x03a21): (147, "ExportedSubClassBaseMap"),
  ("read_only_space", 0x03a49): (148, "ExportedSubClassMap"),
  ("read_only_space", 0x03a71): (68, "AbstractInternalClassSubclass1Map"),
  ("read_only_space", 0x03a99): (69, "AbstractInternalClassSubclass2Map"),
  ("read_only_space", 0x03ac1): (135, "InternalClassWithSmiElementsMap"),
  ("read_only_space", 0x03ae9): (170, "InternalClassWithStructElementsMap"),
  ("read_only_space", 0x03b11): (149, "ExportedSubClass2Map"),
  ("read_only_space", 0x03b39): (179, "SortStateMap"),
  ("read_only_space", 0x03b61): (86, "AllocationSiteWithWeakNextMap"),
  ("read_only_space", 0x03b89): (86, "AllocationSiteWithoutWeakNextMap"),
  ("read_only_space", 0x03bb1): (77, "LoadHandler1Map"),
  ("read_only_space", 0x03bd9): (77, "LoadHandler2Map"),
  ("read_only_space", 0x03c01): (77, "LoadHandler3Map"),
  ("read_only_space", 0x03c29): (78, "StoreHandler0Map"),
  ("read_only_space", 0x03c51): (78, "StoreHandler1Map"),
  ("read_only_space", 0x03c79): (78, "StoreHandler2Map"),
  ("read_only_space", 0x03ca1): (78, "StoreHandler3Map"),
  ("map_space", 0x00121): (1057, "ExternalMap"),
  ("map_space", 0x00149): (1072, "JSMessageObjectMap"),
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
  ("read_only_space", 0x01195): "EmptyEnumCache",
  ("read_only_space", 0x011c9): "EmptyPropertyArray",
  ("read_only_space", 0x011d1): "EmptyByteArray",
  ("read_only_space", 0x011d9): "EmptyObjectBoilerplateDescription",
  ("read_only_space", 0x011e5): "EmptyArrayBoilerplateDescription",
  ("read_only_space", 0x01219): "EmptyClosureFeedbackCellArray",
  ("read_only_space", 0x01221): "EmptySlowElementDictionary",
  ("read_only_space", 0x01245): "EmptyOrderedHashMap",
  ("read_only_space", 0x01259): "EmptyOrderedHashSet",
  ("read_only_space", 0x0126d): "EmptyFeedbackMetadata",
  ("read_only_space", 0x01279): "EmptyPropertyCell",
  ("read_only_space", 0x0128d): "EmptyPropertyDictionary",
  ("read_only_space", 0x012b5): "NoOpInterceptorInfo",
  ("read_only_space", 0x01305): "EmptyWeakArrayList",
  ("read_only_space", 0x01311): "InfinityValue",
  ("read_only_space", 0x0131d): "MinusZeroValue",
  ("read_only_space", 0x01329): "MinusInfinityValue",
  ("read_only_space", 0x01335): "SelfReferenceMarker",
  ("read_only_space", 0x01375): "BasicBlockCountersMarker",
  ("read_only_space", 0x013b9): "OffHeapTrampolineRelocationInfo",
  ("read_only_space", 0x013c5): "TrampolineTrivialCodeDataContainer",
  ("read_only_space", 0x013d1): "TrampolinePromiseRejectionCodeDataContainer",
  ("read_only_space", 0x013dd): "GlobalThisBindingScopeInfo",
  ("read_only_space", 0x01415): "EmptyFunctionScopeInfo",
  ("read_only_space", 0x0143d): "NativeScopeInfo",
  ("read_only_space", 0x01459): "HashSeed",
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
  ("old_space", 0x0051d): "RegExpSpeciesProtector",
  ("old_space", 0x00531): "StringLengthProtector",
  ("old_space", 0x00545): "ArrayIteratorProtector",
  ("old_space", 0x00559): "ArrayBufferDetachingProtector",
  ("old_space", 0x0056d): "PromiseHookProtector",
  ("old_space", 0x00581): "PromiseResolveProtector",
  ("old_space", 0x00595): "MapIteratorProtector",
  ("old_space", 0x005a9): "PromiseThenProtector",
  ("old_space", 0x005bd): "SetIteratorProtector",
  ("old_space", 0x005d1): "StringIteratorProtector",
  ("old_space", 0x005e5): "SingleCharacterStringCache",
  ("old_space", 0x009ed): "StringSplitCache",
  ("old_space", 0x00df5): "RegExpMultipleCache",
  ("old_space", 0x011fd): "BuiltinsConstantsTable",
  ("old_space", 0x015a5): "AsyncFunctionAwaitRejectSharedFun",
  ("old_space", 0x015cd): "AsyncFunctionAwaitResolveSharedFun",
  ("old_space", 0x015f5): "AsyncGeneratorAwaitRejectSharedFun",
  ("old_space", 0x0161d): "AsyncGeneratorAwaitResolveSharedFun",
  ("old_space", 0x01645): "AsyncGeneratorYieldResolveSharedFun",
  ("old_space", 0x0166d): "AsyncGeneratorReturnResolveSharedFun",
  ("old_space", 0x01695): "AsyncGeneratorReturnClosedRejectSharedFun",
  ("old_space", 0x016bd): "AsyncGeneratorReturnClosedResolveSharedFun",
  ("old_space", 0x016e5): "AsyncIteratorValueUnwrapSharedFun",
  ("old_space", 0x0170d): "PromiseAllResolveElementSharedFun",
  ("old_space", 0x01735): "PromiseAllSettledResolveElementSharedFun",
  ("old_space", 0x0175d): "PromiseAllSettledRejectElementSharedFun",
  ("old_space", 0x01785): "PromiseAnyRejectElementSharedFun",
  ("old_space", 0x017ad): "PromiseCapabilityDefaultRejectSharedFun",
  ("old_space", 0x017d5): "PromiseCapabilityDefaultResolveSharedFun",
  ("old_space", 0x017fd): "PromiseCatchFinallySharedFun",
  ("old_space", 0x01825): "PromiseGetCapabilitiesExecutorSharedFun",
  ("old_space", 0x0184d): "PromiseThenFinallySharedFun",
  ("old_space", 0x01875): "PromiseThrowerFinallySharedFun",
  ("old_space", 0x0189d): "PromiseValueThunkFinallySharedFun",
  ("old_space", 0x018c5): "ProxyRevokeSharedFun",
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
  "WASM",
  "WASM_TO_JS",
  "JS_TO_WASM",
  "WASM_DEBUG_BREAK",
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
