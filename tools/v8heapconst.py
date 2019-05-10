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
  65: "HEAP_NUMBER_TYPE",
  66: "BIGINT_TYPE",
  67: "ODDBALL_TYPE",
  68: "MAP_TYPE",
  69: "CODE_TYPE",
  70: "MUTABLE_HEAP_NUMBER_TYPE",
  71: "FOREIGN_TYPE",
  72: "BYTE_ARRAY_TYPE",
  73: "BYTECODE_ARRAY_TYPE",
  74: "FREE_SPACE_TYPE",
  75: "FIXED_INT8_ARRAY_TYPE",
  76: "FIXED_UINT8_ARRAY_TYPE",
  77: "FIXED_INT16_ARRAY_TYPE",
  78: "FIXED_UINT16_ARRAY_TYPE",
  79: "FIXED_INT32_ARRAY_TYPE",
  80: "FIXED_UINT32_ARRAY_TYPE",
  81: "FIXED_FLOAT32_ARRAY_TYPE",
  82: "FIXED_FLOAT64_ARRAY_TYPE",
  83: "FIXED_UINT8_CLAMPED_ARRAY_TYPE",
  84: "FIXED_BIGINT64_ARRAY_TYPE",
  85: "FIXED_BIGUINT64_ARRAY_TYPE",
  86: "FIXED_DOUBLE_ARRAY_TYPE",
  87: "FEEDBACK_METADATA_TYPE",
  88: "FILLER_TYPE",
  89: "ACCESS_CHECK_INFO_TYPE",
  90: "ACCESSOR_INFO_TYPE",
  91: "ACCESSOR_PAIR_TYPE",
  92: "ALIASED_ARGUMENTS_ENTRY_TYPE",
  93: "ALLOCATION_MEMENTO_TYPE",
  94: "ASM_WASM_DATA_TYPE",
  95: "ASYNC_GENERATOR_REQUEST_TYPE",
  96: "CLASS_POSITIONS_TYPE",
  97: "DEBUG_INFO_TYPE",
  98: "ENUM_CACHE_TYPE",
  99: "FUNCTION_TEMPLATE_INFO_TYPE",
  100: "FUNCTION_TEMPLATE_RARE_DATA_TYPE",
  101: "INTERCEPTOR_INFO_TYPE",
  102: "INTERPRETER_DATA_TYPE",
  103: "MODULE_INFO_ENTRY_TYPE",
  104: "MODULE_TYPE",
  105: "OBJECT_TEMPLATE_INFO_TYPE",
  106: "PROMISE_CAPABILITY_TYPE",
  107: "PROMISE_REACTION_TYPE",
  108: "PROTOTYPE_INFO_TYPE",
  109: "SCRIPT_TYPE",
  110: "SOURCE_POSITION_TABLE_WITH_FRAME_CACHE_TYPE",
  111: "STACK_FRAME_INFO_TYPE",
  112: "STACK_TRACE_FRAME_TYPE",
  113: "TUPLE2_TYPE",
  114: "TUPLE3_TYPE",
  115: "ARRAY_BOILERPLATE_DESCRIPTION_TYPE",
  116: "WASM_CAPI_FUNCTION_DATA_TYPE",
  117: "WASM_DEBUG_INFO_TYPE",
  118: "WASM_EXCEPTION_TAG_TYPE",
  119: "WASM_EXPORTED_FUNCTION_DATA_TYPE",
  120: "CALLABLE_TASK_TYPE",
  121: "CALLBACK_TASK_TYPE",
  122: "PROMISE_FULFILL_REACTION_JOB_TASK_TYPE",
  123: "PROMISE_REJECT_REACTION_JOB_TASK_TYPE",
  124: "PROMISE_RESOLVE_THENABLE_JOB_TASK_TYPE",
  125: "FINALIZATION_GROUP_CLEANUP_JOB_TASK_TYPE",
  126: "ALLOCATION_SITE_TYPE",
  127: "EMBEDDER_DATA_ARRAY_TYPE",
  128: "FIXED_ARRAY_TYPE",
  129: "OBJECT_BOILERPLATE_DESCRIPTION_TYPE",
  130: "CLOSURE_FEEDBACK_CELL_ARRAY_TYPE",
  131: "HASH_TABLE_TYPE",
  132: "ORDERED_HASH_MAP_TYPE",
  133: "ORDERED_HASH_SET_TYPE",
  134: "ORDERED_NAME_DICTIONARY_TYPE",
  135: "NAME_DICTIONARY_TYPE",
  136: "GLOBAL_DICTIONARY_TYPE",
  137: "NUMBER_DICTIONARY_TYPE",
  138: "SIMPLE_NUMBER_DICTIONARY_TYPE",
  139: "STRING_TABLE_TYPE",
  140: "EPHEMERON_HASH_TABLE_TYPE",
  141: "SCOPE_INFO_TYPE",
  142: "SCRIPT_CONTEXT_TABLE_TYPE",
  143: "AWAIT_CONTEXT_TYPE",
  144: "BLOCK_CONTEXT_TYPE",
  145: "CATCH_CONTEXT_TYPE",
  146: "DEBUG_EVALUATE_CONTEXT_TYPE",
  147: "EVAL_CONTEXT_TYPE",
  148: "FUNCTION_CONTEXT_TYPE",
  149: "MODULE_CONTEXT_TYPE",
  150: "NATIVE_CONTEXT_TYPE",
  151: "SCRIPT_CONTEXT_TYPE",
  152: "WITH_CONTEXT_TYPE",
  153: "WEAK_FIXED_ARRAY_TYPE",
  154: "TRANSITION_ARRAY_TYPE",
  155: "CALL_HANDLER_INFO_TYPE",
  156: "CELL_TYPE",
  157: "CODE_DATA_CONTAINER_TYPE",
  158: "DESCRIPTOR_ARRAY_TYPE",
  159: "FEEDBACK_CELL_TYPE",
  160: "FEEDBACK_VECTOR_TYPE",
  161: "LOAD_HANDLER_TYPE",
  162: "PREPARSE_DATA_TYPE",
  163: "PROPERTY_ARRAY_TYPE",
  164: "PROPERTY_CELL_TYPE",
  165: "SHARED_FUNCTION_INFO_TYPE",
  166: "SMALL_ORDERED_HASH_MAP_TYPE",
  167: "SMALL_ORDERED_HASH_SET_TYPE",
  168: "SMALL_ORDERED_NAME_DICTIONARY_TYPE",
  169: "STORE_HANDLER_TYPE",
  170: "UNCOMPILED_DATA_WITHOUT_PREPARSE_DATA_TYPE",
  171: "UNCOMPILED_DATA_WITH_PREPARSE_DATA_TYPE",
  172: "WEAK_ARRAY_LIST_TYPE",
  173: "WEAK_CELL_TYPE",
  1024: "JS_PROXY_TYPE",
  1025: "JS_GLOBAL_OBJECT_TYPE",
  1026: "JS_GLOBAL_PROXY_TYPE",
  1027: "JS_MODULE_NAMESPACE_TYPE",
  1040: "JS_SPECIAL_API_OBJECT_TYPE",
  1041: "JS_VALUE_TYPE",
  1056: "JS_API_OBJECT_TYPE",
  1057: "JS_OBJECT_TYPE",
  1058: "JS_ARGUMENTS_TYPE",
  1059: "JS_ARRAY_BUFFER_TYPE",
  1060: "JS_ARRAY_ITERATOR_TYPE",
  1061: "JS_ARRAY_TYPE",
  1062: "JS_ASYNC_FROM_SYNC_ITERATOR_TYPE",
  1063: "JS_ASYNC_FUNCTION_OBJECT_TYPE",
  1064: "JS_ASYNC_GENERATOR_OBJECT_TYPE",
  1065: "JS_CONTEXT_EXTENSION_OBJECT_TYPE",
  1066: "JS_DATE_TYPE",
  1067: "JS_ERROR_TYPE",
  1068: "JS_GENERATOR_OBJECT_TYPE",
  1069: "JS_MAP_TYPE",
  1070: "JS_MAP_KEY_ITERATOR_TYPE",
  1071: "JS_MAP_KEY_VALUE_ITERATOR_TYPE",
  1072: "JS_MAP_VALUE_ITERATOR_TYPE",
  1073: "JS_MESSAGE_OBJECT_TYPE",
  1074: "JS_PROMISE_TYPE",
  1075: "JS_REGEXP_TYPE",
  1076: "JS_REGEXP_STRING_ITERATOR_TYPE",
  1077: "JS_SET_TYPE",
  1078: "JS_SET_KEY_VALUE_ITERATOR_TYPE",
  1079: "JS_SET_VALUE_ITERATOR_TYPE",
  1080: "JS_STRING_ITERATOR_TYPE",
  1081: "JS_WEAK_REF_TYPE",
  1082: "JS_FINALIZATION_GROUP_CLEANUP_ITERATOR_TYPE",
  1083: "JS_FINALIZATION_GROUP_TYPE",
  1084: "JS_WEAK_MAP_TYPE",
  1085: "JS_WEAK_SET_TYPE",
  1086: "JS_TYPED_ARRAY_TYPE",
  1087: "JS_DATA_VIEW_TYPE",
  1088: "JS_INTL_V8_BREAK_ITERATOR_TYPE",
  1089: "JS_INTL_COLLATOR_TYPE",
  1090: "JS_INTL_DATE_TIME_FORMAT_TYPE",
  1091: "JS_INTL_LIST_FORMAT_TYPE",
  1092: "JS_INTL_LOCALE_TYPE",
  1093: "JS_INTL_NUMBER_FORMAT_TYPE",
  1094: "JS_INTL_PLURAL_RULES_TYPE",
  1095: "JS_INTL_RELATIVE_TIME_FORMAT_TYPE",
  1096: "JS_INTL_SEGMENT_ITERATOR_TYPE",
  1097: "JS_INTL_SEGMENTER_TYPE",
  1098: "WASM_EXCEPTION_TYPE",
  1099: "WASM_GLOBAL_TYPE",
  1100: "WASM_INSTANCE_TYPE",
  1101: "WASM_MEMORY_TYPE",
  1102: "WASM_MODULE_TYPE",
  1103: "WASM_TABLE_TYPE",
  1104: "JS_BOUND_FUNCTION_TYPE",
  1105: "JS_FUNCTION_TYPE",
}

# List of known V8 maps.
KNOWN_MAPS = {
  ("read_only_space", 0x00141): (74, "FreeSpaceMap"),
  ("read_only_space", 0x00169): (68, "MetaMap"),
  ("read_only_space", 0x001ad): (67, "NullMap"),
  ("read_only_space", 0x001e5): (158, "DescriptorArrayMap"),
  ("read_only_space", 0x00215): (153, "WeakFixedArrayMap"),
  ("read_only_space", 0x0023d): (88, "OnePointerFillerMap"),
  ("read_only_space", 0x00265): (88, "TwoPointerFillerMap"),
  ("read_only_space", 0x002a9): (67, "UninitializedMap"),
  ("read_only_space", 0x002ed): (8, "OneByteInternalizedStringMap"),
  ("read_only_space", 0x00349): (67, "UndefinedMap"),
  ("read_only_space", 0x0037d): (65, "HeapNumberMap"),
  ("read_only_space", 0x003c1): (67, "TheHoleMap"),
  ("read_only_space", 0x00421): (67, "BooleanMap"),
  ("read_only_space", 0x004a9): (72, "ByteArrayMap"),
  ("read_only_space", 0x004d1): (128, "FixedArrayMap"),
  ("read_only_space", 0x004f9): (128, "FixedCOWArrayMap"),
  ("read_only_space", 0x00521): (131, "HashTableMap"),
  ("read_only_space", 0x00549): (64, "SymbolMap"),
  ("read_only_space", 0x00571): (40, "OneByteStringMap"),
  ("read_only_space", 0x00599): (141, "ScopeInfoMap"),
  ("read_only_space", 0x005c1): (165, "SharedFunctionInfoMap"),
  ("read_only_space", 0x005e9): (69, "CodeMap"),
  ("read_only_space", 0x00611): (148, "FunctionContextMap"),
  ("read_only_space", 0x00639): (156, "CellMap"),
  ("read_only_space", 0x00661): (164, "GlobalPropertyCellMap"),
  ("read_only_space", 0x00689): (71, "ForeignMap"),
  ("read_only_space", 0x006b1): (154, "TransitionArrayMap"),
  ("read_only_space", 0x006d9): (160, "FeedbackVectorMap"),
  ("read_only_space", 0x0072d): (67, "ArgumentsMarkerMap"),
  ("read_only_space", 0x0078d): (67, "ExceptionMap"),
  ("read_only_space", 0x007e9): (67, "TerminationExceptionMap"),
  ("read_only_space", 0x00851): (67, "OptimizedOutMap"),
  ("read_only_space", 0x008b1): (67, "StaleRegisterMap"),
  ("read_only_space", 0x008f5): (150, "NativeContextMap"),
  ("read_only_space", 0x0091d): (149, "ModuleContextMap"),
  ("read_only_space", 0x00945): (147, "EvalContextMap"),
  ("read_only_space", 0x0096d): (151, "ScriptContextMap"),
  ("read_only_space", 0x00995): (143, "AwaitContextMap"),
  ("read_only_space", 0x009bd): (144, "BlockContextMap"),
  ("read_only_space", 0x009e5): (145, "CatchContextMap"),
  ("read_only_space", 0x00a0d): (152, "WithContextMap"),
  ("read_only_space", 0x00a35): (146, "DebugEvaluateContextMap"),
  ("read_only_space", 0x00a5d): (142, "ScriptContextTableMap"),
  ("read_only_space", 0x00a85): (130, "ClosureFeedbackCellArrayMap"),
  ("read_only_space", 0x00aad): (87, "FeedbackMetadataArrayMap"),
  ("read_only_space", 0x00ad5): (128, "ArrayListMap"),
  ("read_only_space", 0x00afd): (66, "BigIntMap"),
  ("read_only_space", 0x00b25): (129, "ObjectBoilerplateDescriptionMap"),
  ("read_only_space", 0x00b4d): (73, "BytecodeArrayMap"),
  ("read_only_space", 0x00b75): (157, "CodeDataContainerMap"),
  ("read_only_space", 0x00b9d): (86, "FixedDoubleArrayMap"),
  ("read_only_space", 0x00bc5): (136, "GlobalDictionaryMap"),
  ("read_only_space", 0x00bed): (159, "ManyClosuresCellMap"),
  ("read_only_space", 0x00c15): (128, "ModuleInfoMap"),
  ("read_only_space", 0x00c3d): (70, "MutableHeapNumberMap"),
  ("read_only_space", 0x00c65): (135, "NameDictionaryMap"),
  ("read_only_space", 0x00c8d): (159, "NoClosuresCellMap"),
  ("read_only_space", 0x00cb5): (137, "NumberDictionaryMap"),
  ("read_only_space", 0x00cdd): (159, "OneClosureCellMap"),
  ("read_only_space", 0x00d05): (132, "OrderedHashMapMap"),
  ("read_only_space", 0x00d2d): (133, "OrderedHashSetMap"),
  ("read_only_space", 0x00d55): (134, "OrderedNameDictionaryMap"),
  ("read_only_space", 0x00d7d): (162, "PreparseDataMap"),
  ("read_only_space", 0x00da5): (163, "PropertyArrayMap"),
  ("read_only_space", 0x00dcd): (155, "SideEffectCallHandlerInfoMap"),
  ("read_only_space", 0x00df5): (155, "SideEffectFreeCallHandlerInfoMap"),
  ("read_only_space", 0x00e1d): (155, "NextCallSideEffectFreeCallHandlerInfoMap"),
  ("read_only_space", 0x00e45): (138, "SimpleNumberDictionaryMap"),
  ("read_only_space", 0x00e6d): (128, "SloppyArgumentsElementsMap"),
  ("read_only_space", 0x00e95): (166, "SmallOrderedHashMapMap"),
  ("read_only_space", 0x00ebd): (167, "SmallOrderedHashSetMap"),
  ("read_only_space", 0x00ee5): (168, "SmallOrderedNameDictionaryMap"),
  ("read_only_space", 0x00f0d): (139, "StringTableMap"),
  ("read_only_space", 0x00f35): (170, "UncompiledDataWithoutPreparseDataMap"),
  ("read_only_space", 0x00f5d): (171, "UncompiledDataWithPreparseDataMap"),
  ("read_only_space", 0x00f85): (172, "WeakArrayListMap"),
  ("read_only_space", 0x00fad): (140, "EphemeronHashTableMap"),
  ("read_only_space", 0x00fd5): (127, "EmbedderDataArrayMap"),
  ("read_only_space", 0x00ffd): (173, "WeakCellMap"),
  ("read_only_space", 0x01025): (58, "NativeSourceStringMap"),
  ("read_only_space", 0x0104d): (32, "StringMap"),
  ("read_only_space", 0x01075): (41, "ConsOneByteStringMap"),
  ("read_only_space", 0x0109d): (33, "ConsStringMap"),
  ("read_only_space", 0x010c5): (45, "ThinOneByteStringMap"),
  ("read_only_space", 0x010ed): (37, "ThinStringMap"),
  ("read_only_space", 0x01115): (35, "SlicedStringMap"),
  ("read_only_space", 0x0113d): (43, "SlicedOneByteStringMap"),
  ("read_only_space", 0x01165): (34, "ExternalStringMap"),
  ("read_only_space", 0x0118d): (42, "ExternalOneByteStringMap"),
  ("read_only_space", 0x011b5): (50, "UncachedExternalStringMap"),
  ("read_only_space", 0x011dd): (0, "InternalizedStringMap"),
  ("read_only_space", 0x01205): (2, "ExternalInternalizedStringMap"),
  ("read_only_space", 0x0122d): (10, "ExternalOneByteInternalizedStringMap"),
  ("read_only_space", 0x01255): (18, "UncachedExternalInternalizedStringMap"),
  ("read_only_space", 0x0127d): (26, "UncachedExternalOneByteInternalizedStringMap"),
  ("read_only_space", 0x012a5): (58, "UncachedExternalOneByteStringMap"),
  ("read_only_space", 0x012cd): (76, "FixedUint8ArrayMap"),
  ("read_only_space", 0x012f5): (75, "FixedInt8ArrayMap"),
  ("read_only_space", 0x0131d): (78, "FixedUint16ArrayMap"),
  ("read_only_space", 0x01345): (77, "FixedInt16ArrayMap"),
  ("read_only_space", 0x0136d): (80, "FixedUint32ArrayMap"),
  ("read_only_space", 0x01395): (79, "FixedInt32ArrayMap"),
  ("read_only_space", 0x013bd): (81, "FixedFloat32ArrayMap"),
  ("read_only_space", 0x013e5): (82, "FixedFloat64ArrayMap"),
  ("read_only_space", 0x0140d): (83, "FixedUint8ClampedArrayMap"),
  ("read_only_space", 0x01435): (85, "FixedBigUint64ArrayMap"),
  ("read_only_space", 0x0145d): (84, "FixedBigInt64ArrayMap"),
  ("read_only_space", 0x01485): (67, "SelfReferenceMarkerMap"),
  ("read_only_space", 0x014b9): (98, "EnumCacheMap"),
  ("read_only_space", 0x01509): (115, "ArrayBoilerplateDescriptionMap"),
  ("read_only_space", 0x016e1): (101, "InterceptorInfoMap"),
  ("read_only_space", 0x03559): (89, "AccessCheckInfoMap"),
  ("read_only_space", 0x03581): (90, "AccessorInfoMap"),
  ("read_only_space", 0x035a9): (91, "AccessorPairMap"),
  ("read_only_space", 0x035d1): (92, "AliasedArgumentsEntryMap"),
  ("read_only_space", 0x035f9): (93, "AllocationMementoMap"),
  ("read_only_space", 0x03621): (94, "AsmWasmDataMap"),
  ("read_only_space", 0x03649): (95, "AsyncGeneratorRequestMap"),
  ("read_only_space", 0x03671): (96, "ClassPositionsMap"),
  ("read_only_space", 0x03699): (97, "DebugInfoMap"),
  ("read_only_space", 0x036c1): (99, "FunctionTemplateInfoMap"),
  ("read_only_space", 0x036e9): (100, "FunctionTemplateRareDataMap"),
  ("read_only_space", 0x03711): (102, "InterpreterDataMap"),
  ("read_only_space", 0x03739): (103, "ModuleInfoEntryMap"),
  ("read_only_space", 0x03761): (104, "ModuleMap"),
  ("read_only_space", 0x03789): (105, "ObjectTemplateInfoMap"),
  ("read_only_space", 0x037b1): (106, "PromiseCapabilityMap"),
  ("read_only_space", 0x037d9): (107, "PromiseReactionMap"),
  ("read_only_space", 0x03801): (108, "PrototypeInfoMap"),
  ("read_only_space", 0x03829): (109, "ScriptMap"),
  ("read_only_space", 0x03851): (110, "SourcePositionTableWithFrameCacheMap"),
  ("read_only_space", 0x03879): (111, "StackFrameInfoMap"),
  ("read_only_space", 0x038a1): (112, "StackTraceFrameMap"),
  ("read_only_space", 0x038c9): (113, "Tuple2Map"),
  ("read_only_space", 0x038f1): (114, "Tuple3Map"),
  ("read_only_space", 0x03919): (116, "WasmCapiFunctionDataMap"),
  ("read_only_space", 0x03941): (117, "WasmDebugInfoMap"),
  ("read_only_space", 0x03969): (118, "WasmExceptionTagMap"),
  ("read_only_space", 0x03991): (119, "WasmExportedFunctionDataMap"),
  ("read_only_space", 0x039b9): (120, "CallableTaskMap"),
  ("read_only_space", 0x039e1): (121, "CallbackTaskMap"),
  ("read_only_space", 0x03a09): (122, "PromiseFulfillReactionJobTaskMap"),
  ("read_only_space", 0x03a31): (123, "PromiseRejectReactionJobTaskMap"),
  ("read_only_space", 0x03a59): (124, "PromiseResolveThenableJobTaskMap"),
  ("read_only_space", 0x03a81): (125, "FinalizationGroupCleanupJobTaskMap"),
  ("read_only_space", 0x03aa9): (126, "AllocationSiteWithWeakNextMap"),
  ("read_only_space", 0x03ad1): (126, "AllocationSiteWithoutWeakNextMap"),
  ("read_only_space", 0x03af9): (161, "LoadHandler1Map"),
  ("read_only_space", 0x03b21): (161, "LoadHandler2Map"),
  ("read_only_space", 0x03b49): (161, "LoadHandler3Map"),
  ("read_only_space", 0x03b71): (169, "StoreHandler0Map"),
  ("read_only_space", 0x03b99): (169, "StoreHandler1Map"),
  ("read_only_space", 0x03bc1): (169, "StoreHandler2Map"),
  ("read_only_space", 0x03be9): (169, "StoreHandler3Map"),
  ("map_space", 0x00141): (1057, "ExternalMap"),
  ("map_space", 0x00169): (1073, "JSMessageObjectMap"),
}

# List of known V8 objects.
KNOWN_OBJECTS = {
  ("read_only_space", 0x00191): "NullValue",
  ("read_only_space", 0x001d5): "EmptyDescriptorArray",
  ("read_only_space", 0x0020d): "EmptyWeakFixedArray",
  ("read_only_space", 0x0028d): "UninitializedValue",
  ("read_only_space", 0x0032d): "UndefinedValue",
  ("read_only_space", 0x00371): "NanValue",
  ("read_only_space", 0x003a5): "TheHoleValue",
  ("read_only_space", 0x003f9): "HoleNanValue",
  ("read_only_space", 0x00405): "TrueValue",
  ("read_only_space", 0x0046d): "FalseValue",
  ("read_only_space", 0x0049d): "empty_string",
  ("read_only_space", 0x00701): "EmptyScopeInfo",
  ("read_only_space", 0x00709): "EmptyFixedArray",
  ("read_only_space", 0x00711): "ArgumentsMarker",
  ("read_only_space", 0x00771): "Exception",
  ("read_only_space", 0x007cd): "TerminationException",
  ("read_only_space", 0x00835): "OptimizedOut",
  ("read_only_space", 0x00895): "StaleRegister",
  ("read_only_space", 0x014ad): "EmptyEnumCache",
  ("read_only_space", 0x014e1): "EmptyPropertyArray",
  ("read_only_space", 0x014e9): "EmptyByteArray",
  ("read_only_space", 0x014f1): "EmptyObjectBoilerplateDescription",
  ("read_only_space", 0x014fd): "EmptyArrayBoilerplateDescription",
  ("read_only_space", 0x01531): "EmptyClosureFeedbackCellArray",
  ("read_only_space", 0x01539): "EmptyFixedUint8Array",
  ("read_only_space", 0x0154d): "EmptyFixedInt8Array",
  ("read_only_space", 0x01561): "EmptyFixedUint16Array",
  ("read_only_space", 0x01575): "EmptyFixedInt16Array",
  ("read_only_space", 0x01589): "EmptyFixedUint32Array",
  ("read_only_space", 0x0159d): "EmptyFixedInt32Array",
  ("read_only_space", 0x015b1): "EmptyFixedFloat32Array",
  ("read_only_space", 0x015c5): "EmptyFixedFloat64Array",
  ("read_only_space", 0x015d9): "EmptyFixedUint8ClampedArray",
  ("read_only_space", 0x015ed): "EmptyFixedBigUint64Array",
  ("read_only_space", 0x01601): "EmptyFixedBigInt64Array",
  ("read_only_space", 0x01615): "EmptySloppyArgumentsElements",
  ("read_only_space", 0x01625): "EmptySlowElementDictionary",
  ("read_only_space", 0x01649): "EmptyOrderedHashMap",
  ("read_only_space", 0x0165d): "EmptyOrderedHashSet",
  ("read_only_space", 0x01671): "EmptyFeedbackMetadata",
  ("read_only_space", 0x0167d): "EmptyPropertyCell",
  ("read_only_space", 0x01691): "EmptyPropertyDictionary",
  ("read_only_space", 0x016b9): "NoOpInterceptorInfo",
  ("read_only_space", 0x01709): "EmptyWeakArrayList",
  ("read_only_space", 0x01715): "InfinityValue",
  ("read_only_space", 0x01721): "MinusZeroValue",
  ("read_only_space", 0x0172d): "MinusInfinityValue",
  ("read_only_space", 0x01739): "SelfReferenceMarker",
  ("read_only_space", 0x01779): "OffHeapTrampolineRelocationInfo",
  ("read_only_space", 0x01785): "HashSeed",
  ("old_space", 0x00141): "ArgumentsIteratorAccessor",
  ("old_space", 0x00185): "ArrayLengthAccessor",
  ("old_space", 0x001c9): "BoundFunctionLengthAccessor",
  ("old_space", 0x0020d): "BoundFunctionNameAccessor",
  ("old_space", 0x00251): "ErrorStackAccessor",
  ("old_space", 0x00295): "FunctionArgumentsAccessor",
  ("old_space", 0x002d9): "FunctionCallerAccessor",
  ("old_space", 0x0031d): "FunctionNameAccessor",
  ("old_space", 0x00361): "FunctionLengthAccessor",
  ("old_space", 0x003a5): "FunctionPrototypeAccessor",
  ("old_space", 0x003e9): "StringLengthAccessor",
  ("old_space", 0x0042d): "InvalidPrototypeValidityCell",
  ("old_space", 0x00435): "EmptyScript",
  ("old_space", 0x00475): "ManyClosuresCell",
  ("old_space", 0x00481): "ArrayConstructorProtector",
  ("old_space", 0x00489): "NoElementsProtector",
  ("old_space", 0x0049d): "IsConcatSpreadableProtector",
  ("old_space", 0x004a5): "ArraySpeciesProtector",
  ("old_space", 0x004b9): "TypedArraySpeciesProtector",
  ("old_space", 0x004cd): "RegExpSpeciesProtector",
  ("old_space", 0x004e1): "PromiseSpeciesProtector",
  ("old_space", 0x004f5): "StringLengthProtector",
  ("old_space", 0x004fd): "ArrayIteratorProtector",
  ("old_space", 0x00511): "ArrayBufferDetachingProtector",
  ("old_space", 0x00525): "PromiseHookProtector",
  ("old_space", 0x00539): "PromiseResolveProtector",
  ("old_space", 0x00541): "MapIteratorProtector",
  ("old_space", 0x00555): "PromiseThenProtector",
  ("old_space", 0x00569): "SetIteratorProtector",
  ("old_space", 0x0057d): "StringIteratorProtector",
  ("old_space", 0x00591): "SingleCharacterStringCache",
  ("old_space", 0x00999): "StringSplitCache",
  ("old_space", 0x00da1): "RegExpMultipleCache",
  ("old_space", 0x011a9): "BuiltinsConstantsTable",
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
