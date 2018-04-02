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
  160: "CONTEXT_EXTENSION_TYPE",
  161: "DEBUG_INFO_TYPE",
  162: "FUNCTION_TEMPLATE_INFO_TYPE",
  163: "INTERCEPTOR_INFO_TYPE",
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
  176: "WASM_SHARED_MODULE_DATA_TYPE",
  177: "CALLABLE_TASK_TYPE",
  178: "CALLBACK_TASK_TYPE",
  179: "PROMISE_FULFILL_REACTION_JOB_TASK_TYPE",
  180: "PROMISE_REJECT_REACTION_JOB_TASK_TYPE",
  181: "PROMISE_RESOLVE_THENABLE_JOB_TASK_TYPE",
  182: "FIXED_ARRAY_TYPE",
  183: "BOILERPLATE_DESCRIPTION_TYPE",
  184: "DESCRIPTOR_ARRAY_TYPE",
  185: "HASH_TABLE_TYPE",
  186: "SCOPE_INFO_TYPE",
  187: "TRANSITION_ARRAY_TYPE",
  188: "BLOCK_CONTEXT_TYPE",
  189: "CATCH_CONTEXT_TYPE",
  190: "DEBUG_EVALUATE_CONTEXT_TYPE",
  191: "EVAL_CONTEXT_TYPE",
  192: "FUNCTION_CONTEXT_TYPE",
  193: "MODULE_CONTEXT_TYPE",
  194: "NATIVE_CONTEXT_TYPE",
  195: "SCRIPT_CONTEXT_TYPE",
  196: "WITH_CONTEXT_TYPE",
  197: "CALL_HANDLER_INFO_TYPE",
  198: "CELL_TYPE",
  199: "CODE_DATA_CONTAINER_TYPE",
  200: "FEEDBACK_CELL_TYPE",
  201: "FEEDBACK_VECTOR_TYPE",
  202: "LOAD_HANDLER_TYPE",
  203: "PROPERTY_ARRAY_TYPE",
  204: "PROPERTY_CELL_TYPE",
  205: "SHARED_FUNCTION_INFO_TYPE",
  206: "SMALL_ORDERED_HASH_MAP_TYPE",
  207: "SMALL_ORDERED_HASH_SET_TYPE",
  208: "STORE_HANDLER_TYPE",
  209: "WEAK_CELL_TYPE",
  210: "WEAK_FIXED_ARRAY_TYPE",
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
  1075: "JS_SET_TYPE",
  1076: "JS_SET_KEY_VALUE_ITERATOR_TYPE",
  1077: "JS_SET_VALUE_ITERATOR_TYPE",
  1078: "JS_STRING_ITERATOR_TYPE",
  1079: "JS_WEAK_MAP_TYPE",
  1080: "JS_WEAK_SET_TYPE",
  1081: "JS_TYPED_ARRAY_TYPE",
  1082: "JS_DATA_VIEW_TYPE",
  1083: "WASM_INSTANCE_TYPE",
  1084: "WASM_MEMORY_TYPE",
  1085: "WASM_MODULE_TYPE",
  1086: "WASM_TABLE_TYPE",
  1087: "JS_BOUND_FUNCTION_TYPE",
  1088: "JS_FUNCTION_TYPE",
}

# List of known V8 maps.
KNOWN_MAPS = {
  ("MAP_SPACE", 0x02201): (138, "FreeSpaceMap"),
  ("MAP_SPACE", 0x02259): (132, "MetaMap"),
  ("MAP_SPACE", 0x022b1): (131, "NullMap"),
  ("MAP_SPACE", 0x02309): (184, "DescriptorArrayMap"),
  ("MAP_SPACE", 0x02361): (182, "FixedArrayMap"),
  ("MAP_SPACE", 0x023b9): (152, "OnePointerFillerMap"),
  ("MAP_SPACE", 0x02411): (152, "TwoPointerFillerMap"),
  ("MAP_SPACE", 0x02469): (131, "UninitializedMap"),
  ("MAP_SPACE", 0x024c1): (8, "OneByteInternalizedStringMap"),
  ("MAP_SPACE", 0x02519): (131, "UndefinedMap"),
  ("MAP_SPACE", 0x02571): (129, "HeapNumberMap"),
  ("MAP_SPACE", 0x025c9): (131, "TheHoleMap"),
  ("MAP_SPACE", 0x02621): (131, "BooleanMap"),
  ("MAP_SPACE", 0x02679): (136, "ByteArrayMap"),
  ("MAP_SPACE", 0x026d1): (182, "FixedCOWArrayMap"),
  ("MAP_SPACE", 0x02729): (185, "HashTableMap"),
  ("MAP_SPACE", 0x02781): (128, "SymbolMap"),
  ("MAP_SPACE", 0x027d9): (72, "OneByteStringMap"),
  ("MAP_SPACE", 0x02831): (186, "ScopeInfoMap"),
  ("MAP_SPACE", 0x02889): (205, "SharedFunctionInfoMap"),
  ("MAP_SPACE", 0x028e1): (133, "CodeMap"),
  ("MAP_SPACE", 0x02939): (192, "FunctionContextMap"),
  ("MAP_SPACE", 0x02991): (198, "CellMap"),
  ("MAP_SPACE", 0x029e9): (209, "WeakCellMap"),
  ("MAP_SPACE", 0x02a41): (204, "GlobalPropertyCellMap"),
  ("MAP_SPACE", 0x02a99): (135, "ForeignMap"),
  ("MAP_SPACE", 0x02af1): (187, "TransitionArrayMap"),
  ("MAP_SPACE", 0x02b49): (201, "FeedbackVectorMap"),
  ("MAP_SPACE", 0x02ba1): (131, "ArgumentsMarkerMap"),
  ("MAP_SPACE", 0x02bf9): (131, "ExceptionMap"),
  ("MAP_SPACE", 0x02c51): (131, "TerminationExceptionMap"),
  ("MAP_SPACE", 0x02ca9): (131, "OptimizedOutMap"),
  ("MAP_SPACE", 0x02d01): (131, "StaleRegisterMap"),
  ("MAP_SPACE", 0x02d59): (194, "NativeContextMap"),
  ("MAP_SPACE", 0x02db1): (193, "ModuleContextMap"),
  ("MAP_SPACE", 0x02e09): (191, "EvalContextMap"),
  ("MAP_SPACE", 0x02e61): (195, "ScriptContextMap"),
  ("MAP_SPACE", 0x02eb9): (188, "BlockContextMap"),
  ("MAP_SPACE", 0x02f11): (189, "CatchContextMap"),
  ("MAP_SPACE", 0x02f69): (196, "WithContextMap"),
  ("MAP_SPACE", 0x02fc1): (190, "DebugEvaluateContextMap"),
  ("MAP_SPACE", 0x03019): (182, "ScriptContextTableMap"),
  ("MAP_SPACE", 0x03071): (151, "FeedbackMetadataArrayMap"),
  ("MAP_SPACE", 0x030c9): (182, "ArrayListMap"),
  ("MAP_SPACE", 0x03121): (130, "BigIntMap"),
  ("MAP_SPACE", 0x03179): (183, "BoilerplateDescriptionMap"),
  ("MAP_SPACE", 0x031d1): (137, "BytecodeArrayMap"),
  ("MAP_SPACE", 0x03229): (199, "CodeDataContainerMap"),
  ("MAP_SPACE", 0x03281): (1057, "ExternalMap"),
  ("MAP_SPACE", 0x032d9): (150, "FixedDoubleArrayMap"),
  ("MAP_SPACE", 0x03331): (185, "GlobalDictionaryMap"),
  ("MAP_SPACE", 0x03389): (200, "ManyClosuresCellMap"),
  ("MAP_SPACE", 0x033e1): (1072, "JSMessageObjectMap"),
  ("MAP_SPACE", 0x03439): (182, "ModuleInfoMap"),
  ("MAP_SPACE", 0x03491): (134, "MutableHeapNumberMap"),
  ("MAP_SPACE", 0x034e9): (185, "NameDictionaryMap"),
  ("MAP_SPACE", 0x03541): (200, "NoClosuresCellMap"),
  ("MAP_SPACE", 0x03599): (185, "NumberDictionaryMap"),
  ("MAP_SPACE", 0x035f1): (200, "OneClosureCellMap"),
  ("MAP_SPACE", 0x03649): (185, "OrderedHashMapMap"),
  ("MAP_SPACE", 0x036a1): (185, "OrderedHashSetMap"),
  ("MAP_SPACE", 0x036f9): (203, "PropertyArrayMap"),
  ("MAP_SPACE", 0x03751): (197, "SideEffectCallHandlerInfoMap"),
  ("MAP_SPACE", 0x037a9): (197, "SideEffectFreeCallHandlerInfoMap"),
  ("MAP_SPACE", 0x03801): (185, "SimpleNumberDictionaryMap"),
  ("MAP_SPACE", 0x03859): (182, "SloppyArgumentsElementsMap"),
  ("MAP_SPACE", 0x038b1): (206, "SmallOrderedHashMapMap"),
  ("MAP_SPACE", 0x03909): (207, "SmallOrderedHashSetMap"),
  ("MAP_SPACE", 0x03961): (185, "StringTableMap"),
  ("MAP_SPACE", 0x039b9): (210, "WeakFixedArrayMap"),
  ("MAP_SPACE", 0x03a11): (106, "NativeSourceStringMap"),
  ("MAP_SPACE", 0x03a69): (64, "StringMap"),
  ("MAP_SPACE", 0x03ac1): (73, "ConsOneByteStringMap"),
  ("MAP_SPACE", 0x03b19): (65, "ConsStringMap"),
  ("MAP_SPACE", 0x03b71): (77, "ThinOneByteStringMap"),
  ("MAP_SPACE", 0x03bc9): (69, "ThinStringMap"),
  ("MAP_SPACE", 0x03c21): (67, "SlicedStringMap"),
  ("MAP_SPACE", 0x03c79): (75, "SlicedOneByteStringMap"),
  ("MAP_SPACE", 0x03cd1): (66, "ExternalStringMap"),
  ("MAP_SPACE", 0x03d29): (82, "ExternalStringWithOneByteDataMap"),
  ("MAP_SPACE", 0x03d81): (74, "ExternalOneByteStringMap"),
  ("MAP_SPACE", 0x03dd9): (98, "ShortExternalStringMap"),
  ("MAP_SPACE", 0x03e31): (114, "ShortExternalStringWithOneByteDataMap"),
  ("MAP_SPACE", 0x03e89): (0, "InternalizedStringMap"),
  ("MAP_SPACE", 0x03ee1): (2, "ExternalInternalizedStringMap"),
  ("MAP_SPACE", 0x03f39): (18, "ExternalInternalizedStringWithOneByteDataMap"),
  ("MAP_SPACE", 0x03f91): (10, "ExternalOneByteInternalizedStringMap"),
  ("MAP_SPACE", 0x03fe9): (34, "ShortExternalInternalizedStringMap"),
  ("MAP_SPACE", 0x04041): (50, "ShortExternalInternalizedStringWithOneByteDataMap"),
  ("MAP_SPACE", 0x04099): (42, "ShortExternalOneByteInternalizedStringMap"),
  ("MAP_SPACE", 0x040f1): (106, "ShortExternalOneByteStringMap"),
  ("MAP_SPACE", 0x04149): (140, "FixedUint8ArrayMap"),
  ("MAP_SPACE", 0x041a1): (139, "FixedInt8ArrayMap"),
  ("MAP_SPACE", 0x041f9): (142, "FixedUint16ArrayMap"),
  ("MAP_SPACE", 0x04251): (141, "FixedInt16ArrayMap"),
  ("MAP_SPACE", 0x042a9): (144, "FixedUint32ArrayMap"),
  ("MAP_SPACE", 0x04301): (143, "FixedInt32ArrayMap"),
  ("MAP_SPACE", 0x04359): (145, "FixedFloat32ArrayMap"),
  ("MAP_SPACE", 0x043b1): (146, "FixedFloat64ArrayMap"),
  ("MAP_SPACE", 0x04409): (147, "FixedUint8ClampedArrayMap"),
  ("MAP_SPACE", 0x04461): (149, "FixedBigUint64ArrayMap"),
  ("MAP_SPACE", 0x044b9): (148, "FixedBigInt64ArrayMap"),
  ("MAP_SPACE", 0x04511): (172, "Tuple2Map"),
  ("MAP_SPACE", 0x04569): (170, "ScriptMap"),
  ("MAP_SPACE", 0x045c1): (163, "InterceptorInfoMap"),
  ("MAP_SPACE", 0x04619): (154, "AccessorInfoMap"),
  ("MAP_SPACE", 0x04671): (153, "AccessCheckInfoMap"),
  ("MAP_SPACE", 0x046c9): (155, "AccessorPairMap"),
  ("MAP_SPACE", 0x04721): (156, "AliasedArgumentsEntryMap"),
  ("MAP_SPACE", 0x04779): (157, "AllocationMementoMap"),
  ("MAP_SPACE", 0x047d1): (158, "AllocationSiteMap"),
  ("MAP_SPACE", 0x04829): (159, "AsyncGeneratorRequestMap"),
  ("MAP_SPACE", 0x04881): (160, "ContextExtensionMap"),
  ("MAP_SPACE", 0x048d9): (161, "DebugInfoMap"),
  ("MAP_SPACE", 0x04931): (162, "FunctionTemplateInfoMap"),
  ("MAP_SPACE", 0x04989): (164, "ModuleInfoEntryMap"),
  ("MAP_SPACE", 0x049e1): (165, "ModuleMap"),
  ("MAP_SPACE", 0x04a39): (166, "ObjectTemplateInfoMap"),
  ("MAP_SPACE", 0x04a91): (167, "PromiseCapabilityMap"),
  ("MAP_SPACE", 0x04ae9): (168, "PromiseReactionMap"),
  ("MAP_SPACE", 0x04b41): (169, "PrototypeInfoMap"),
  ("MAP_SPACE", 0x04b99): (171, "StackFrameInfoMap"),
  ("MAP_SPACE", 0x04bf1): (173, "Tuple3Map"),
  ("MAP_SPACE", 0x04c49): (174, "WasmCompiledModuleMap"),
  ("MAP_SPACE", 0x04ca1): (175, "WasmDebugInfoMap"),
  ("MAP_SPACE", 0x04cf9): (176, "WasmSharedModuleDataMap"),
  ("MAP_SPACE", 0x04d51): (177, "CallableTaskMap"),
  ("MAP_SPACE", 0x04da9): (178, "CallbackTaskMap"),
  ("MAP_SPACE", 0x04e01): (179, "PromiseFulfillReactionJobTaskMap"),
  ("MAP_SPACE", 0x04e59): (180, "PromiseRejectReactionJobTaskMap"),
  ("MAP_SPACE", 0x04eb1): (181, "PromiseResolveThenableJobTaskMap"),
}

# List of known V8 objects.
KNOWN_OBJECTS = {
  ("OLD_SPACE", 0x02201): "NullValue",
  ("OLD_SPACE", 0x02231): "EmptyDescriptorArray",
  ("OLD_SPACE", 0x02251): "EmptyFixedArray",
  ("OLD_SPACE", 0x02261): "UninitializedValue",
  ("OLD_SPACE", 0x022e1): "UndefinedValue",
  ("OLD_SPACE", 0x02311): "NanValue",
  ("OLD_SPACE", 0x02321): "TheHoleValue",
  ("OLD_SPACE", 0x02371): "HoleNanValue",
  ("OLD_SPACE", 0x02381): "TrueValue",
  ("OLD_SPACE", 0x023f1): "FalseValue",
  ("OLD_SPACE", 0x02441): "empty_string",
  ("OLD_SPACE", 0x02459): "EmptyScopeInfo",
  ("OLD_SPACE", 0x02469): "ArgumentsMarker",
  ("OLD_SPACE", 0x024c1): "Exception",
  ("OLD_SPACE", 0x02519): "TerminationException",
  ("OLD_SPACE", 0x02579): "OptimizedOut",
  ("OLD_SPACE", 0x025d1): "StaleRegister",
  ("OLD_SPACE", 0x02661): "EmptyByteArray",
  ("OLD_SPACE", 0x02681): "EmptyFixedUint8Array",
  ("OLD_SPACE", 0x026a1): "EmptyFixedInt8Array",
  ("OLD_SPACE", 0x026c1): "EmptyFixedUint16Array",
  ("OLD_SPACE", 0x026e1): "EmptyFixedInt16Array",
  ("OLD_SPACE", 0x02701): "EmptyFixedUint32Array",
  ("OLD_SPACE", 0x02721): "EmptyFixedInt32Array",
  ("OLD_SPACE", 0x02741): "EmptyFixedFloat32Array",
  ("OLD_SPACE", 0x02761): "EmptyFixedFloat64Array",
  ("OLD_SPACE", 0x02781): "EmptyFixedUint8ClampedArray",
  ("OLD_SPACE", 0x027e1): "EmptyScript",
  ("OLD_SPACE", 0x02879): "ManyClosuresCell",
  ("OLD_SPACE", 0x02889): "EmptySloppyArgumentsElements",
  ("OLD_SPACE", 0x028a9): "EmptySlowElementDictionary",
  ("OLD_SPACE", 0x028f1): "EmptyOrderedHashMap",
  ("OLD_SPACE", 0x02919): "EmptyOrderedHashSet",
  ("OLD_SPACE", 0x02951): "EmptyPropertyCell",
  ("OLD_SPACE", 0x02979): "EmptyWeakCell",
  ("OLD_SPACE", 0x029e9): "NoElementsProtector",
  ("OLD_SPACE", 0x02a11): "IsConcatSpreadableProtector",
  ("OLD_SPACE", 0x02a21): "SpeciesProtector",
  ("OLD_SPACE", 0x02a49): "StringLengthProtector",
  ("OLD_SPACE", 0x02a59): "ArrayIteratorProtector",
  ("OLD_SPACE", 0x02a81): "ArrayBufferNeuteringProtector",
  ("OLD_SPACE", 0x02b09): "InfinityValue",
  ("OLD_SPACE", 0x02b19): "MinusZeroValue",
  ("OLD_SPACE", 0x02b29): "MinusInfinityValue",
}

# List of known V8 Frame Markers.
FRAME_MARKERS = (
  "ENTRY",
  "CONSTRUCT_ENTRY",
  "EXIT",
  "OPTIMIZED",
  "WASM_COMPILED",
  "WASM_TO_JS",
  "WASM_TO_WASM",
  "JS_TO_WASM",
  "WASM_INTERPRETER_ENTRY",
  "C_WASM_ENTRY",
  "INTERPRETED",
  "STUB",
  "BUILTIN_CONTINUATION",
  "JAVA_SCRIPT_BUILTIN_CONTINUATION",
  "INTERNAL",
  "CONSTRUCT",
  "ARGUMENTS_ADAPTOR",
  "BUILTIN",
  "BUILTIN_EXIT",
  "NATIVE",
)

# This set of constants is generated from a shipping build.
