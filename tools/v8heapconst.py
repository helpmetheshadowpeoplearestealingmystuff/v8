# Copyright 2018 the V8 project authors. All rights reserved.
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
  18: "EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE",
  34: "SHORT_EXTERNAL_INTERNALIZED_STRING_TYPE",
  42: "SHORT_EXTERNAL_ONE_BYTE_INTERNALIZED_STRING_TYPE",
  50: "SHORT_EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE",
  64: "STRING_TYPE",
  65: "CONS_STRING_TYPE",
  66: "EXTERNAL_STRING_TYPE",
  67: "SLICED_STRING_TYPE",
  69: "THIN_STRING_TYPE",
  72: "ONE_BYTE_STRING_TYPE",
  73: "CONS_ONE_BYTE_STRING_TYPE",
  74: "EXTERNAL_ONE_BYTE_STRING_TYPE",
  75: "SLICED_ONE_BYTE_STRING_TYPE",
  77: "THIN_ONE_BYTE_STRING_TYPE",
  82: "EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE",
  98: "SHORT_EXTERNAL_STRING_TYPE",
  106: "SHORT_EXTERNAL_ONE_BYTE_STRING_TYPE",
  114: "SHORT_EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE",
  128: "SYMBOL_TYPE",
  129: "HEAP_NUMBER_TYPE",
  130: "BIGINT_TYPE",
  131: "ODDBALL_TYPE",
  132: "MAP_TYPE",
  133: "CODE_TYPE",
  134: "MUTABLE_HEAP_NUMBER_TYPE",
  135: "FOREIGN_TYPE",
  136: "BYTE_ARRAY_TYPE",
  137: "BYTECODE_ARRAY_TYPE",
  138: "FREE_SPACE_TYPE",
  139: "FIXED_INT8_ARRAY_TYPE",
  140: "FIXED_UINT8_ARRAY_TYPE",
  141: "FIXED_INT16_ARRAY_TYPE",
  142: "FIXED_UINT16_ARRAY_TYPE",
  143: "FIXED_INT32_ARRAY_TYPE",
  144: "FIXED_UINT32_ARRAY_TYPE",
  145: "FIXED_FLOAT32_ARRAY_TYPE",
  146: "FIXED_FLOAT64_ARRAY_TYPE",
  147: "FIXED_UINT8_CLAMPED_ARRAY_TYPE",
  148: "FIXED_BIGINT64_ARRAY_TYPE",
  149: "FIXED_BIGUINT64_ARRAY_TYPE",
  150: "FIXED_DOUBLE_ARRAY_TYPE",
  151: "FEEDBACK_METADATA_TYPE",
  152: "FILLER_TYPE",
  153: "ACCESS_CHECK_INFO_TYPE",
  154: "ACCESSOR_INFO_TYPE",
  155: "ACCESSOR_PAIR_TYPE",
  156: "ALIASED_ARGUMENTS_ENTRY_TYPE",
  157: "ALLOCATION_MEMENTO_TYPE",
  158: "ASYNC_GENERATOR_REQUEST_TYPE",
  159: "DEBUG_INFO_TYPE",
  160: "FUNCTION_TEMPLATE_INFO_TYPE",
  161: "INTERCEPTOR_INFO_TYPE",
  162: "INTERPRETER_DATA_TYPE",
  163: "MODULE_INFO_ENTRY_TYPE",
  164: "MODULE_TYPE",
  165: "OBJECT_TEMPLATE_INFO_TYPE",
  166: "PROMISE_CAPABILITY_TYPE",
  167: "PROMISE_REACTION_TYPE",
  168: "PROTOTYPE_INFO_TYPE",
  169: "SCRIPT_TYPE",
  170: "STACK_FRAME_INFO_TYPE",
  171: "TUPLE2_TYPE",
  172: "TUPLE3_TYPE",
  173: "ARRAY_BOILERPLATE_DESCRIPTION_TYPE",
  174: "WASM_DEBUG_INFO_TYPE",
  175: "WASM_EXPORTED_FUNCTION_DATA_TYPE",
  176: "CALLABLE_TASK_TYPE",
  177: "CALLBACK_TASK_TYPE",
  178: "PROMISE_FULFILL_REACTION_JOB_TASK_TYPE",
  179: "PROMISE_REJECT_REACTION_JOB_TASK_TYPE",
  180: "PROMISE_RESOLVE_THENABLE_JOB_TASK_TYPE",
  181: "ALLOCATION_SITE_TYPE",
  182: "FIXED_ARRAY_TYPE",
  183: "OBJECT_BOILERPLATE_DESCRIPTION_TYPE",
  184: "HASH_TABLE_TYPE",
  185: "ORDERED_HASH_MAP_TYPE",
  186: "ORDERED_HASH_SET_TYPE",
  187: "NAME_DICTIONARY_TYPE",
  188: "GLOBAL_DICTIONARY_TYPE",
  189: "NUMBER_DICTIONARY_TYPE",
  190: "SIMPLE_NUMBER_DICTIONARY_TYPE",
  191: "STRING_TABLE_TYPE",
  192: "EPHEMERON_HASH_TABLE_TYPE",
  193: "SCOPE_INFO_TYPE",
  194: "SCRIPT_CONTEXT_TABLE_TYPE",
  195: "BLOCK_CONTEXT_TYPE",
  196: "CATCH_CONTEXT_TYPE",
  197: "DEBUG_EVALUATE_CONTEXT_TYPE",
  198: "EVAL_CONTEXT_TYPE",
  199: "FUNCTION_CONTEXT_TYPE",
  200: "MODULE_CONTEXT_TYPE",
  201: "NATIVE_CONTEXT_TYPE",
  202: "SCRIPT_CONTEXT_TYPE",
  203: "WITH_CONTEXT_TYPE",
  204: "WEAK_FIXED_ARRAY_TYPE",
  205: "DESCRIPTOR_ARRAY_TYPE",
  206: "TRANSITION_ARRAY_TYPE",
  207: "CALL_HANDLER_INFO_TYPE",
  208: "CELL_TYPE",
  209: "CODE_DATA_CONTAINER_TYPE",
  210: "FEEDBACK_CELL_TYPE",
  211: "FEEDBACK_VECTOR_TYPE",
  212: "LOAD_HANDLER_TYPE",
  213: "PRE_PARSED_SCOPE_DATA_TYPE",
  214: "PROPERTY_ARRAY_TYPE",
  215: "PROPERTY_CELL_TYPE",
  216: "SHARED_FUNCTION_INFO_TYPE",
  217: "SMALL_ORDERED_HASH_MAP_TYPE",
  218: "SMALL_ORDERED_HASH_SET_TYPE",
  219: "STORE_HANDLER_TYPE",
  220: "UNCOMPILED_DATA_WITHOUT_PRE_PARSED_SCOPE_TYPE",
  221: "UNCOMPILED_DATA_WITH_PRE_PARSED_SCOPE_TYPE",
  222: "WEAK_ARRAY_LIST_TYPE",
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
  1063: "JS_ASYNC_GENERATOR_OBJECT_TYPE",
  1064: "JS_CONTEXT_EXTENSION_OBJECT_TYPE",
  1065: "JS_DATE_TYPE",
  1066: "JS_ERROR_TYPE",
  1067: "JS_GENERATOR_OBJECT_TYPE",
  1068: "JS_MAP_TYPE",
  1069: "JS_MAP_KEY_ITERATOR_TYPE",
  1070: "JS_MAP_KEY_VALUE_ITERATOR_TYPE",
  1071: "JS_MAP_VALUE_ITERATOR_TYPE",
  1072: "JS_MESSAGE_OBJECT_TYPE",
  1073: "JS_PROMISE_TYPE",
  1074: "JS_REGEXP_TYPE",
  1075: "JS_REGEXP_STRING_ITERATOR_TYPE",
  1076: "JS_SET_TYPE",
  1077: "JS_SET_KEY_VALUE_ITERATOR_TYPE",
  1078: "JS_SET_VALUE_ITERATOR_TYPE",
  1079: "JS_STRING_ITERATOR_TYPE",
  1080: "JS_WEAK_MAP_TYPE",
  1081: "JS_WEAK_SET_TYPE",
  1082: "JS_TYPED_ARRAY_TYPE",
  1083: "JS_DATA_VIEW_TYPE",
  1084: "JS_INTL_COLLATOR_TYPE",
  1085: "JS_INTL_LIST_FORMAT_TYPE",
  1086: "JS_INTL_LOCALE_TYPE",
  1087: "JS_INTL_PLURAL_RULES_TYPE",
  1088: "JS_INTL_RELATIVE_TIME_FORMAT_TYPE",
  1089: "WASM_GLOBAL_TYPE",
  1090: "WASM_INSTANCE_TYPE",
  1091: "WASM_MEMORY_TYPE",
  1092: "WASM_MODULE_TYPE",
  1093: "WASM_TABLE_TYPE",
  1094: "JS_BOUND_FUNCTION_TYPE",
  1095: "JS_FUNCTION_TYPE",
}

# List of known V8 maps.
KNOWN_MAPS = {
  ("RO_SPACE", 0x02201): (138, "FreeSpaceMap"),
  ("RO_SPACE", 0x02251): (132, "MetaMap"),
  ("RO_SPACE", 0x022d1): (131, "NullMap"),
  ("RO_SPACE", 0x02341): (205, "DescriptorArrayMap"),
  ("RO_SPACE", 0x023a1): (204, "WeakFixedArrayMap"),
  ("RO_SPACE", 0x023f1): (152, "OnePointerFillerMap"),
  ("RO_SPACE", 0x02441): (152, "TwoPointerFillerMap"),
  ("RO_SPACE", 0x024c1): (131, "UninitializedMap"),
  ("RO_SPACE", 0x02539): (8, "OneByteInternalizedStringMap"),
  ("RO_SPACE", 0x025e1): (131, "UndefinedMap"),
  ("RO_SPACE", 0x02641): (129, "HeapNumberMap"),
  ("RO_SPACE", 0x026c1): (131, "TheHoleMap"),
  ("RO_SPACE", 0x02771): (131, "BooleanMap"),
  ("RO_SPACE", 0x02869): (136, "ByteArrayMap"),
  ("RO_SPACE", 0x028b9): (182, "FixedArrayMap"),
  ("RO_SPACE", 0x02909): (182, "FixedCOWArrayMap"),
  ("RO_SPACE", 0x02959): (184, "HashTableMap"),
  ("RO_SPACE", 0x029a9): (128, "SymbolMap"),
  ("RO_SPACE", 0x029f9): (72, "OneByteStringMap"),
  ("RO_SPACE", 0x02a49): (193, "ScopeInfoMap"),
  ("RO_SPACE", 0x02a99): (216, "SharedFunctionInfoMap"),
  ("RO_SPACE", 0x02ae9): (133, "CodeMap"),
  ("RO_SPACE", 0x02b39): (199, "FunctionContextMap"),
  ("RO_SPACE", 0x02b89): (208, "CellMap"),
  ("RO_SPACE", 0x02bd9): (215, "GlobalPropertyCellMap"),
  ("RO_SPACE", 0x02c29): (135, "ForeignMap"),
  ("RO_SPACE", 0x02c79): (206, "TransitionArrayMap"),
  ("RO_SPACE", 0x02cc9): (211, "FeedbackVectorMap"),
  ("RO_SPACE", 0x02d69): (131, "ArgumentsMarkerMap"),
  ("RO_SPACE", 0x02e11): (131, "ExceptionMap"),
  ("RO_SPACE", 0x02eb9): (131, "TerminationExceptionMap"),
  ("RO_SPACE", 0x02f69): (131, "OptimizedOutMap"),
  ("RO_SPACE", 0x03011): (131, "StaleRegisterMap"),
  ("RO_SPACE", 0x03089): (201, "NativeContextMap"),
  ("RO_SPACE", 0x030d9): (200, "ModuleContextMap"),
  ("RO_SPACE", 0x03129): (198, "EvalContextMap"),
  ("RO_SPACE", 0x03179): (202, "ScriptContextMap"),
  ("RO_SPACE", 0x031c9): (195, "BlockContextMap"),
  ("RO_SPACE", 0x03219): (196, "CatchContextMap"),
  ("RO_SPACE", 0x03269): (203, "WithContextMap"),
  ("RO_SPACE", 0x032b9): (197, "DebugEvaluateContextMap"),
  ("RO_SPACE", 0x03309): (194, "ScriptContextTableMap"),
  ("RO_SPACE", 0x03359): (151, "FeedbackMetadataArrayMap"),
  ("RO_SPACE", 0x033a9): (182, "ArrayListMap"),
  ("RO_SPACE", 0x033f9): (130, "BigIntMap"),
  ("RO_SPACE", 0x03449): (183, "ObjectBoilerplateDescriptionMap"),
  ("RO_SPACE", 0x03499): (137, "BytecodeArrayMap"),
  ("RO_SPACE", 0x034e9): (209, "CodeDataContainerMap"),
  ("RO_SPACE", 0x03539): (150, "FixedDoubleArrayMap"),
  ("RO_SPACE", 0x03589): (188, "GlobalDictionaryMap"),
  ("RO_SPACE", 0x035d9): (210, "ManyClosuresCellMap"),
  ("RO_SPACE", 0x03629): (182, "ModuleInfoMap"),
  ("RO_SPACE", 0x03679): (134, "MutableHeapNumberMap"),
  ("RO_SPACE", 0x036c9): (187, "NameDictionaryMap"),
  ("RO_SPACE", 0x03719): (210, "NoClosuresCellMap"),
  ("RO_SPACE", 0x03769): (189, "NumberDictionaryMap"),
  ("RO_SPACE", 0x037b9): (210, "OneClosureCellMap"),
  ("RO_SPACE", 0x03809): (185, "OrderedHashMapMap"),
  ("RO_SPACE", 0x03859): (186, "OrderedHashSetMap"),
  ("RO_SPACE", 0x038a9): (213, "PreParsedScopeDataMap"),
  ("RO_SPACE", 0x038f9): (214, "PropertyArrayMap"),
  ("RO_SPACE", 0x03949): (207, "SideEffectCallHandlerInfoMap"),
  ("RO_SPACE", 0x03999): (207, "SideEffectFreeCallHandlerInfoMap"),
  ("RO_SPACE", 0x039e9): (207, "NextCallSideEffectFreeCallHandlerInfoMap"),
  ("RO_SPACE", 0x03a39): (190, "SimpleNumberDictionaryMap"),
  ("RO_SPACE", 0x03a89): (182, "SloppyArgumentsElementsMap"),
  ("RO_SPACE", 0x03ad9): (217, "SmallOrderedHashMapMap"),
  ("RO_SPACE", 0x03b29): (218, "SmallOrderedHashSetMap"),
  ("RO_SPACE", 0x03b79): (191, "StringTableMap"),
  ("RO_SPACE", 0x03bc9): (220, "UncompiledDataWithoutPreParsedScopeMap"),
  ("RO_SPACE", 0x03c19): (221, "UncompiledDataWithPreParsedScopeMap"),
  ("RO_SPACE", 0x03c69): (222, "WeakArrayListMap"),
  ("RO_SPACE", 0x03cb9): (192, "EphemeronHashTableMap"),
  ("RO_SPACE", 0x03d09): (106, "NativeSourceStringMap"),
  ("RO_SPACE", 0x03d59): (64, "StringMap"),
  ("RO_SPACE", 0x03da9): (73, "ConsOneByteStringMap"),
  ("RO_SPACE", 0x03df9): (65, "ConsStringMap"),
  ("RO_SPACE", 0x03e49): (77, "ThinOneByteStringMap"),
  ("RO_SPACE", 0x03e99): (69, "ThinStringMap"),
  ("RO_SPACE", 0x03ee9): (67, "SlicedStringMap"),
  ("RO_SPACE", 0x03f39): (75, "SlicedOneByteStringMap"),
  ("RO_SPACE", 0x03f89): (66, "ExternalStringMap"),
  ("RO_SPACE", 0x03fd9): (82, "ExternalStringWithOneByteDataMap"),
  ("RO_SPACE", 0x04029): (74, "ExternalOneByteStringMap"),
  ("RO_SPACE", 0x04079): (98, "ShortExternalStringMap"),
  ("RO_SPACE", 0x040c9): (114, "ShortExternalStringWithOneByteDataMap"),
  ("RO_SPACE", 0x04119): (0, "InternalizedStringMap"),
  ("RO_SPACE", 0x04169): (2, "ExternalInternalizedStringMap"),
  ("RO_SPACE", 0x041b9): (18, "ExternalInternalizedStringWithOneByteDataMap"),
  ("RO_SPACE", 0x04209): (10, "ExternalOneByteInternalizedStringMap"),
  ("RO_SPACE", 0x04259): (34, "ShortExternalInternalizedStringMap"),
  ("RO_SPACE", 0x042a9): (50, "ShortExternalInternalizedStringWithOneByteDataMap"),
  ("RO_SPACE", 0x042f9): (42, "ShortExternalOneByteInternalizedStringMap"),
  ("RO_SPACE", 0x04349): (106, "ShortExternalOneByteStringMap"),
  ("RO_SPACE", 0x04399): (140, "FixedUint8ArrayMap"),
  ("RO_SPACE", 0x043e9): (139, "FixedInt8ArrayMap"),
  ("RO_SPACE", 0x04439): (142, "FixedUint16ArrayMap"),
  ("RO_SPACE", 0x04489): (141, "FixedInt16ArrayMap"),
  ("RO_SPACE", 0x044d9): (144, "FixedUint32ArrayMap"),
  ("RO_SPACE", 0x04529): (143, "FixedInt32ArrayMap"),
  ("RO_SPACE", 0x04579): (145, "FixedFloat32ArrayMap"),
  ("RO_SPACE", 0x045c9): (146, "FixedFloat64ArrayMap"),
  ("RO_SPACE", 0x04619): (147, "FixedUint8ClampedArrayMap"),
  ("RO_SPACE", 0x04669): (149, "FixedBigUint64ArrayMap"),
  ("RO_SPACE", 0x046b9): (148, "FixedBigInt64ArrayMap"),
  ("RO_SPACE", 0x04709): (131, "SelfReferenceMarkerMap"),
  ("RO_SPACE", 0x04771): (171, "Tuple2Map"),
  ("RO_SPACE", 0x04a99): (161, "InterceptorInfoMap"),
  ("RO_SPACE", 0x04b91): (169, "ScriptMap"),
  ("RO_SPACE", 0x09a39): (154, "AccessorInfoMap"),
  ("RO_SPACE", 0x09a89): (153, "AccessCheckInfoMap"),
  ("RO_SPACE", 0x09ad9): (155, "AccessorPairMap"),
  ("RO_SPACE", 0x09b29): (156, "AliasedArgumentsEntryMap"),
  ("RO_SPACE", 0x09b79): (157, "AllocationMementoMap"),
  ("RO_SPACE", 0x09bc9): (158, "AsyncGeneratorRequestMap"),
  ("RO_SPACE", 0x09c19): (159, "DebugInfoMap"),
  ("RO_SPACE", 0x09c69): (160, "FunctionTemplateInfoMap"),
  ("RO_SPACE", 0x09cb9): (162, "InterpreterDataMap"),
  ("RO_SPACE", 0x09d09): (163, "ModuleInfoEntryMap"),
  ("RO_SPACE", 0x09d59): (164, "ModuleMap"),
  ("RO_SPACE", 0x09da9): (165, "ObjectTemplateInfoMap"),
  ("RO_SPACE", 0x09df9): (166, "PromiseCapabilityMap"),
  ("RO_SPACE", 0x09e49): (167, "PromiseReactionMap"),
  ("RO_SPACE", 0x09e99): (168, "PrototypeInfoMap"),
  ("RO_SPACE", 0x09ee9): (170, "StackFrameInfoMap"),
  ("RO_SPACE", 0x09f39): (172, "Tuple3Map"),
  ("RO_SPACE", 0x09f89): (173, "ArrayBoilerplateDescriptionMap"),
  ("RO_SPACE", 0x09fd9): (174, "WasmDebugInfoMap"),
  ("RO_SPACE", 0x0a029): (175, "WasmExportedFunctionDataMap"),
  ("RO_SPACE", 0x0a079): (176, "CallableTaskMap"),
  ("RO_SPACE", 0x0a0c9): (177, "CallbackTaskMap"),
  ("RO_SPACE", 0x0a119): (178, "PromiseFulfillReactionJobTaskMap"),
  ("RO_SPACE", 0x0a169): (179, "PromiseRejectReactionJobTaskMap"),
  ("RO_SPACE", 0x0a1b9): (180, "PromiseResolveThenableJobTaskMap"),
  ("RO_SPACE", 0x0a209): (181, "AllocationSiteMap"),
  ("RO_SPACE", 0x0a259): (181, "AllocationSiteMap"),
  ("MAP_SPACE", 0x02201): (1057, "ExternalMap"),
  ("MAP_SPACE", 0x02251): (1072, "JSMessageObjectMap"),
}

# List of known V8 objects.
KNOWN_OBJECTS = {
  ("RO_SPACE", 0x022a1): "NullValue",
  ("RO_SPACE", 0x02321): "EmptyDescriptorArray",
  ("RO_SPACE", 0x02491): "UninitializedValue",
  ("RO_SPACE", 0x025b1): "UndefinedValue",
  ("RO_SPACE", 0x02631): "NanValue",
  ("RO_SPACE", 0x02691): "TheHoleValue",
  ("RO_SPACE", 0x02731): "HoleNanValue",
  ("RO_SPACE", 0x02741): "TrueValue",
  ("RO_SPACE", 0x02801): "FalseValue",
  ("RO_SPACE", 0x02851): "empty_string",
  ("RO_SPACE", 0x02d19): "EmptyScopeInfo",
  ("RO_SPACE", 0x02d29): "EmptyFixedArray",
  ("RO_SPACE", 0x02d39): "ArgumentsMarker",
  ("RO_SPACE", 0x02de1): "Exception",
  ("RO_SPACE", 0x02e89): "TerminationException",
  ("RO_SPACE", 0x02f39): "OptimizedOut",
  ("RO_SPACE", 0x02fe1): "StaleRegister",
  ("RO_SPACE", 0x047d1): "EmptyByteArray",
  ("RO_SPACE", 0x047f9): "EmptyFixedUint8Array",
  ("RO_SPACE", 0x04819): "EmptyFixedInt8Array",
  ("RO_SPACE", 0x04839): "EmptyFixedUint16Array",
  ("RO_SPACE", 0x04859): "EmptyFixedInt16Array",
  ("RO_SPACE", 0x04879): "EmptyFixedUint32Array",
  ("RO_SPACE", 0x04899): "EmptyFixedInt32Array",
  ("RO_SPACE", 0x048b9): "EmptyFixedFloat32Array",
  ("RO_SPACE", 0x048d9): "EmptyFixedFloat64Array",
  ("RO_SPACE", 0x048f9): "EmptyFixedUint8ClampedArray",
  ("RO_SPACE", 0x04959): "EmptySloppyArgumentsElements",
  ("RO_SPACE", 0x04979): "EmptySlowElementDictionary",
  ("RO_SPACE", 0x049c1): "EmptyOrderedHashMap",
  ("RO_SPACE", 0x049e9): "EmptyOrderedHashSet",
  ("RO_SPACE", 0x04a21): "EmptyPropertyCell",
  ("RO_SPACE", 0x04b01): "InfinityValue",
  ("RO_SPACE", 0x04b11): "MinusZeroValue",
  ("RO_SPACE", 0x04b21): "MinusInfinityValue",
  ("RO_SPACE", 0x04b31): "SelfReferenceMarker",
  ("OLD_SPACE", 0x02211): "EmptyScript",
  ("OLD_SPACE", 0x02291): "ManyClosuresCell",
  ("OLD_SPACE", 0x022b1): "NoElementsProtector",
  ("OLD_SPACE", 0x022d9): "IsConcatSpreadableProtector",
  ("OLD_SPACE", 0x022e9): "ArraySpeciesProtector",
  ("OLD_SPACE", 0x02311): "TypedArraySpeciesProtector",
  ("OLD_SPACE", 0x02339): "PromiseSpeciesProtector",
  ("OLD_SPACE", 0x02361): "StringLengthProtector",
  ("OLD_SPACE", 0x02371): "ArrayIteratorProtector",
  ("OLD_SPACE", 0x02399): "ArrayBufferNeuteringProtector",
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
