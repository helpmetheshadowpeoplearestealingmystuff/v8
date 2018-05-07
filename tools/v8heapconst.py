# Copyright 2017 the V8 project authors. All rights reserved.
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
  158: "ALLOCATION_SITE_TYPE",
  159: "ASYNC_GENERATOR_REQUEST_TYPE",
  160: "DEBUG_INFO_TYPE",
  161: "FUNCTION_TEMPLATE_INFO_TYPE",
  162: "INTERCEPTOR_INFO_TYPE",
  163: "INTERPRETER_DATA_TYPE",
  164: "MODULE_INFO_ENTRY_TYPE",
  165: "MODULE_TYPE",
  166: "OBJECT_TEMPLATE_INFO_TYPE",
  167: "PROMISE_CAPABILITY_TYPE",
  168: "PROMISE_REACTION_TYPE",
  169: "PROTOTYPE_INFO_TYPE",
  170: "SCRIPT_TYPE",
  171: "STACK_FRAME_INFO_TYPE",
  172: "TUPLE2_TYPE",
  173: "TUPLE3_TYPE",
  174: "WASM_COMPILED_MODULE_TYPE",
  175: "WASM_DEBUG_INFO_TYPE",
  176: "WASM_EXPORTED_FUNCTION_DATA_TYPE",
  177: "WASM_SHARED_MODULE_DATA_TYPE",
  178: "CALLABLE_TASK_TYPE",
  179: "CALLBACK_TASK_TYPE",
  180: "PROMISE_FULFILL_REACTION_JOB_TASK_TYPE",
  181: "PROMISE_REJECT_REACTION_JOB_TASK_TYPE",
  182: "PROMISE_RESOLVE_THENABLE_JOB_TASK_TYPE",
  183: "FIXED_ARRAY_TYPE",
  184: "BOILERPLATE_DESCRIPTION_TYPE",
  185: "DESCRIPTOR_ARRAY_TYPE",
  186: "HASH_TABLE_TYPE",
  187: "SCOPE_INFO_TYPE",
  188: "BLOCK_CONTEXT_TYPE",
  189: "CATCH_CONTEXT_TYPE",
  190: "DEBUG_EVALUATE_CONTEXT_TYPE",
  191: "EVAL_CONTEXT_TYPE",
  192: "FUNCTION_CONTEXT_TYPE",
  193: "MODULE_CONTEXT_TYPE",
  194: "NATIVE_CONTEXT_TYPE",
  195: "SCRIPT_CONTEXT_TYPE",
  196: "WITH_CONTEXT_TYPE",
  197: "WEAK_FIXED_ARRAY_TYPE",
  198: "TRANSITION_ARRAY_TYPE",
  199: "CALL_HANDLER_INFO_TYPE",
  200: "CELL_TYPE",
  201: "CODE_DATA_CONTAINER_TYPE",
  202: "FEEDBACK_CELL_TYPE",
  203: "FEEDBACK_VECTOR_TYPE",
  204: "LOAD_HANDLER_TYPE",
  205: "PROPERTY_ARRAY_TYPE",
  206: "PROPERTY_CELL_TYPE",
  207: "SHARED_FUNCTION_INFO_TYPE",
  208: "SMALL_ORDERED_HASH_MAP_TYPE",
  209: "SMALL_ORDERED_HASH_SET_TYPE",
  210: "STORE_HANDLER_TYPE",
  211: "WEAK_CELL_TYPE",
  212: "WEAK_ARRAY_LIST_TYPE",
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
  1084: "WASM_GLOBAL_TYPE",
  1085: "WASM_INSTANCE_TYPE",
  1086: "WASM_MEMORY_TYPE",
  1087: "WASM_MODULE_TYPE",
  1088: "WASM_TABLE_TYPE",
  1089: "JS_BOUND_FUNCTION_TYPE",
  1090: "JS_FUNCTION_TYPE",
}

# List of known V8 maps.
KNOWN_MAPS = {
  ("RO_SPACE", 0x02201): (138, "FreeSpaceMap"),
  ("RO_SPACE", 0x02259): (132, "MetaMap"),
  ("RO_SPACE", 0x022e1): (131, "NullMap"),
  ("RO_SPACE", 0x02359): (185, "DescriptorArrayMap"),
  ("RO_SPACE", 0x023c1): (183, "FixedArrayMap"),
  ("RO_SPACE", 0x02429): (211, "WeakCellMap"),
  ("RO_SPACE", 0x024d1): (152, "OnePointerFillerMap"),
  ("RO_SPACE", 0x02539): (152, "TwoPointerFillerMap"),
  ("RO_SPACE", 0x025a1): (131, "UninitializedMap"),
  ("RO_SPACE", 0x02631): (8, "OneByteInternalizedStringMap"),
  ("RO_SPACE", 0x026f1): (131, "UndefinedMap"),
  ("RO_SPACE", 0x02769): (129, "HeapNumberMap"),
  ("RO_SPACE", 0x02801): (131, "TheHoleMap"),
  ("RO_SPACE", 0x028c9): (131, "BooleanMap"),
  ("RO_SPACE", 0x029d9): (136, "ByteArrayMap"),
  ("RO_SPACE", 0x02a41): (183, "FixedCOWArrayMap"),
  ("RO_SPACE", 0x02aa9): (186, "HashTableMap"),
  ("RO_SPACE", 0x02b11): (128, "SymbolMap"),
  ("RO_SPACE", 0x02b79): (72, "OneByteStringMap"),
  ("RO_SPACE", 0x02be1): (187, "ScopeInfoMap"),
  ("RO_SPACE", 0x02c49): (207, "SharedFunctionInfoMap"),
  ("RO_SPACE", 0x02cb1): (133, "CodeMap"),
  ("RO_SPACE", 0x02d19): (192, "FunctionContextMap"),
  ("RO_SPACE", 0x02d81): (200, "CellMap"),
  ("RO_SPACE", 0x02de9): (206, "GlobalPropertyCellMap"),
  ("RO_SPACE", 0x02e51): (135, "ForeignMap"),
  ("RO_SPACE", 0x02eb9): (198, "TransitionArrayMap"),
  ("RO_SPACE", 0x02f21): (203, "FeedbackVectorMap"),
  ("RO_SPACE", 0x02f89): (131, "ArgumentsMarkerMap"),
  ("RO_SPACE", 0x03019): (131, "ExceptionMap"),
  ("RO_SPACE", 0x030a9): (131, "TerminationExceptionMap"),
  ("RO_SPACE", 0x03141): (131, "OptimizedOutMap"),
  ("RO_SPACE", 0x031d1): (131, "StaleRegisterMap"),
  ("RO_SPACE", 0x03261): (194, "NativeContextMap"),
  ("RO_SPACE", 0x032c9): (193, "ModuleContextMap"),
  ("RO_SPACE", 0x03331): (191, "EvalContextMap"),
  ("RO_SPACE", 0x03399): (195, "ScriptContextMap"),
  ("RO_SPACE", 0x03401): (188, "BlockContextMap"),
  ("RO_SPACE", 0x03469): (189, "CatchContextMap"),
  ("RO_SPACE", 0x034d1): (196, "WithContextMap"),
  ("RO_SPACE", 0x03539): (190, "DebugEvaluateContextMap"),
  ("RO_SPACE", 0x035a1): (183, "ScriptContextTableMap"),
  ("RO_SPACE", 0x03609): (151, "FeedbackMetadataArrayMap"),
  ("RO_SPACE", 0x03671): (183, "ArrayListMap"),
  ("RO_SPACE", 0x036d9): (130, "BigIntMap"),
  ("RO_SPACE", 0x03741): (184, "BoilerplateDescriptionMap"),
  ("RO_SPACE", 0x037a9): (137, "BytecodeArrayMap"),
  ("RO_SPACE", 0x03811): (201, "CodeDataContainerMap"),
  ("RO_SPACE", 0x03879): (150, "FixedDoubleArrayMap"),
  ("RO_SPACE", 0x038e1): (186, "GlobalDictionaryMap"),
  ("RO_SPACE", 0x03949): (202, "ManyClosuresCellMap"),
  ("RO_SPACE", 0x039b1): (183, "ModuleInfoMap"),
  ("RO_SPACE", 0x03a19): (134, "MutableHeapNumberMap"),
  ("RO_SPACE", 0x03a81): (186, "NameDictionaryMap"),
  ("RO_SPACE", 0x03ae9): (202, "NoClosuresCellMap"),
  ("RO_SPACE", 0x03b51): (186, "NumberDictionaryMap"),
  ("RO_SPACE", 0x03bb9): (202, "OneClosureCellMap"),
  ("RO_SPACE", 0x03c21): (186, "OrderedHashMapMap"),
  ("RO_SPACE", 0x03c89): (186, "OrderedHashSetMap"),
  ("RO_SPACE", 0x03cf1): (205, "PropertyArrayMap"),
  ("RO_SPACE", 0x03d59): (199, "SideEffectCallHandlerInfoMap"),
  ("RO_SPACE", 0x03dc1): (199, "SideEffectFreeCallHandlerInfoMap"),
  ("RO_SPACE", 0x03e29): (199, "NextCallSideEffectFreeCallHandlerInfoMap"),
  ("RO_SPACE", 0x03e91): (186, "SimpleNumberDictionaryMap"),
  ("RO_SPACE", 0x03ef9): (183, "SloppyArgumentsElementsMap"),
  ("RO_SPACE", 0x03f61): (208, "SmallOrderedHashMapMap"),
  ("RO_SPACE", 0x03fc9): (209, "SmallOrderedHashSetMap"),
  ("RO_SPACE", 0x04031): (186, "StringTableMap"),
  ("RO_SPACE", 0x04099): (197, "WeakFixedArrayMap"),
  ("RO_SPACE", 0x04101): (212, "WeakArrayListMap"),
  ("RO_SPACE", 0x04169): (106, "NativeSourceStringMap"),
  ("RO_SPACE", 0x041d1): (64, "StringMap"),
  ("RO_SPACE", 0x04239): (73, "ConsOneByteStringMap"),
  ("RO_SPACE", 0x042a1): (65, "ConsStringMap"),
  ("RO_SPACE", 0x04309): (77, "ThinOneByteStringMap"),
  ("RO_SPACE", 0x04371): (69, "ThinStringMap"),
  ("RO_SPACE", 0x043d9): (67, "SlicedStringMap"),
  ("RO_SPACE", 0x04441): (75, "SlicedOneByteStringMap"),
  ("RO_SPACE", 0x044a9): (66, "ExternalStringMap"),
  ("RO_SPACE", 0x04511): (82, "ExternalStringWithOneByteDataMap"),
  ("RO_SPACE", 0x04579): (74, "ExternalOneByteStringMap"),
  ("RO_SPACE", 0x045e1): (98, "ShortExternalStringMap"),
  ("RO_SPACE", 0x04649): (114, "ShortExternalStringWithOneByteDataMap"),
  ("RO_SPACE", 0x046b1): (0, "InternalizedStringMap"),
  ("RO_SPACE", 0x04719): (2, "ExternalInternalizedStringMap"),
  ("RO_SPACE", 0x04781): (18, "ExternalInternalizedStringWithOneByteDataMap"),
  ("RO_SPACE", 0x047e9): (10, "ExternalOneByteInternalizedStringMap"),
  ("RO_SPACE", 0x04851): (34, "ShortExternalInternalizedStringMap"),
  ("RO_SPACE", 0x048b9): (50, "ShortExternalInternalizedStringWithOneByteDataMap"),
  ("RO_SPACE", 0x04921): (42, "ShortExternalOneByteInternalizedStringMap"),
  ("RO_SPACE", 0x04989): (106, "ShortExternalOneByteStringMap"),
  ("RO_SPACE", 0x049f1): (140, "FixedUint8ArrayMap"),
  ("RO_SPACE", 0x04a59): (139, "FixedInt8ArrayMap"),
  ("RO_SPACE", 0x04ac1): (142, "FixedUint16ArrayMap"),
  ("RO_SPACE", 0x04b29): (141, "FixedInt16ArrayMap"),
  ("RO_SPACE", 0x04b91): (144, "FixedUint32ArrayMap"),
  ("RO_SPACE", 0x04bf9): (143, "FixedInt32ArrayMap"),
  ("RO_SPACE", 0x04c61): (145, "FixedFloat32ArrayMap"),
  ("RO_SPACE", 0x04cc9): (146, "FixedFloat64ArrayMap"),
  ("RO_SPACE", 0x04d31): (147, "FixedUint8ClampedArrayMap"),
  ("RO_SPACE", 0x04d99): (149, "FixedBigUint64ArrayMap"),
  ("RO_SPACE", 0x04e01): (148, "FixedBigInt64ArrayMap"),
  ("RO_SPACE", 0x04e69): (131, "SelfReferenceMarkerMap"),
  ("RO_SPACE", 0x04ee9): (172, "Tuple2Map"),
  ("RO_SPACE", 0x04f61): (170, "ScriptMap"),
  ("RO_SPACE", 0x04fc9): (162, "InterceptorInfoMap"),
  ("RO_SPACE", 0x08ec1): (154, "AccessorInfoMap"),
  ("RO_SPACE", 0x090d1): (153, "AccessCheckInfoMap"),
  ("RO_SPACE", 0x09139): (155, "AccessorPairMap"),
  ("RO_SPACE", 0x091a1): (156, "AliasedArgumentsEntryMap"),
  ("RO_SPACE", 0x09209): (157, "AllocationMementoMap"),
  ("RO_SPACE", 0x09271): (158, "AllocationSiteMap"),
  ("RO_SPACE", 0x092d9): (159, "AsyncGeneratorRequestMap"),
  ("RO_SPACE", 0x09341): (160, "DebugInfoMap"),
  ("RO_SPACE", 0x093a9): (161, "FunctionTemplateInfoMap"),
  ("RO_SPACE", 0x09411): (163, "InterpreterDataMap"),
  ("RO_SPACE", 0x09479): (164, "ModuleInfoEntryMap"),
  ("RO_SPACE", 0x094e1): (165, "ModuleMap"),
  ("RO_SPACE", 0x09549): (166, "ObjectTemplateInfoMap"),
  ("RO_SPACE", 0x095b1): (167, "PromiseCapabilityMap"),
  ("RO_SPACE", 0x09619): (168, "PromiseReactionMap"),
  ("RO_SPACE", 0x09681): (169, "PrototypeInfoMap"),
  ("RO_SPACE", 0x096e9): (171, "StackFrameInfoMap"),
  ("RO_SPACE", 0x09751): (173, "Tuple3Map"),
  ("RO_SPACE", 0x097b9): (174, "WasmCompiledModuleMap"),
  ("RO_SPACE", 0x09821): (175, "WasmDebugInfoMap"),
  ("RO_SPACE", 0x09889): (176, "WasmExportedFunctionDataMap"),
  ("RO_SPACE", 0x098f1): (177, "WasmSharedModuleDataMap"),
  ("RO_SPACE", 0x09959): (178, "CallableTaskMap"),
  ("RO_SPACE", 0x099c1): (179, "CallbackTaskMap"),
  ("RO_SPACE", 0x09a29): (180, "PromiseFulfillReactionJobTaskMap"),
  ("RO_SPACE", 0x09a91): (181, "PromiseRejectReactionJobTaskMap"),
  ("RO_SPACE", 0x09af9): (182, "PromiseResolveThenableJobTaskMap"),
  ("MAP_SPACE", 0x02201): (1057, "ExternalMap"),
  ("MAP_SPACE", 0x02259): (1072, "JSMessageObjectMap"),
}

# List of known V8 objects.
KNOWN_OBJECTS = {
  ("RO_SPACE", 0x022b1): "NullValue",
  ("RO_SPACE", 0x02339): "EmptyDescriptorArray",
  ("RO_SPACE", 0x023b1): "EmptyFixedArray",
  ("RO_SPACE", 0x026c1): "UndefinedValue",
  ("RO_SPACE", 0x02759): "NanValue",
  ("RO_SPACE", 0x027d1): "TheHoleValue",
  ("RO_SPACE", 0x02889): "HoleNanValue",
  ("RO_SPACE", 0x02899): "TrueValue",
  ("RO_SPACE", 0x02971): "FalseValue",
  ("RO_SPACE", 0x029c1): "empty_string",
  ("RO_SPACE", 0x04f51): "EmptyByteArray",
  ("OLD_SPACE", 0x02201): "UninitializedValue",
  ("OLD_SPACE", 0x02231): "EmptyScopeInfo",
  ("OLD_SPACE", 0x02241): "ArgumentsMarker",
  ("OLD_SPACE", 0x02271): "Exception",
  ("OLD_SPACE", 0x022a1): "TerminationException",
  ("OLD_SPACE", 0x022d1): "OptimizedOut",
  ("OLD_SPACE", 0x02301): "StaleRegister",
  ("OLD_SPACE", 0x02361): "EmptyFixedUint8Array",
  ("OLD_SPACE", 0x02381): "EmptyFixedInt8Array",
  ("OLD_SPACE", 0x023a1): "EmptyFixedUint16Array",
  ("OLD_SPACE", 0x023c1): "EmptyFixedInt16Array",
  ("OLD_SPACE", 0x023e1): "EmptyFixedUint32Array",
  ("OLD_SPACE", 0x02401): "EmptyFixedInt32Array",
  ("OLD_SPACE", 0x02421): "EmptyFixedFloat32Array",
  ("OLD_SPACE", 0x02441): "EmptyFixedFloat64Array",
  ("OLD_SPACE", 0x02461): "EmptyFixedUint8ClampedArray",
  ("OLD_SPACE", 0x024c1): "EmptyScript",
  ("OLD_SPACE", 0x02559): "ManyClosuresCell",
  ("OLD_SPACE", 0x02569): "EmptySloppyArgumentsElements",
  ("OLD_SPACE", 0x02589): "EmptySlowElementDictionary",
  ("OLD_SPACE", 0x025d1): "EmptyOrderedHashMap",
  ("OLD_SPACE", 0x025f9): "EmptyOrderedHashSet",
  ("OLD_SPACE", 0x02631): "EmptyPropertyCell",
  ("OLD_SPACE", 0x02659): "EmptyWeakCell",
  ("OLD_SPACE", 0x026e1): "NoElementsProtector",
  ("OLD_SPACE", 0x02709): "IsConcatSpreadableProtector",
  ("OLD_SPACE", 0x02719): "ArraySpeciesProtector",
  ("OLD_SPACE", 0x02741): "TypedArraySpeciesProtector",
  ("OLD_SPACE", 0x02769): "PromiseSpeciesProtector",
  ("OLD_SPACE", 0x02791): "StringLengthProtector",
  ("OLD_SPACE", 0x027a1): "ArrayIteratorProtector",
  ("OLD_SPACE", 0x027c9): "ArrayBufferNeuteringProtector",
  ("OLD_SPACE", 0x02851): "InfinityValue",
  ("OLD_SPACE", 0x02861): "MinusZeroValue",
  ("OLD_SPACE", 0x02871): "MinusInfinityValue",
  ("OLD_SPACE", 0x02881): "SelfReferenceMarker",
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
