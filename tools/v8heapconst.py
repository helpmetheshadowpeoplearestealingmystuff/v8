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
  34: "UNCACHED_EXTERNAL_INTERNALIZED_STRING_TYPE",
  42: "UNCACHED_EXTERNAL_ONE_BYTE_INTERNALIZED_STRING_TYPE",
  50: "UNCACHED_EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE",
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
  98: "UNCACHED_EXTERNAL_STRING_TYPE",
  106: "UNCACHED_EXTERNAL_ONE_BYTE_STRING_TYPE",
  114: "UNCACHED_EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE",
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
  181: "MICROTASK_QUEUE_TYPE",
  182: "ALLOCATION_SITE_TYPE",
  183: "FIXED_ARRAY_TYPE",
  184: "OBJECT_BOILERPLATE_DESCRIPTION_TYPE",
  185: "HASH_TABLE_TYPE",
  186: "ORDERED_HASH_MAP_TYPE",
  187: "ORDERED_HASH_SET_TYPE",
  188: "NAME_DICTIONARY_TYPE",
  189: "GLOBAL_DICTIONARY_TYPE",
  190: "NUMBER_DICTIONARY_TYPE",
  191: "SIMPLE_NUMBER_DICTIONARY_TYPE",
  192: "STRING_TABLE_TYPE",
  193: "EPHEMERON_HASH_TABLE_TYPE",
  194: "SCOPE_INFO_TYPE",
  195: "SCRIPT_CONTEXT_TABLE_TYPE",
  196: "AWAIT_CONTEXT_TYPE",
  197: "BLOCK_CONTEXT_TYPE",
  198: "CATCH_CONTEXT_TYPE",
  199: "DEBUG_EVALUATE_CONTEXT_TYPE",
  200: "EVAL_CONTEXT_TYPE",
  201: "FUNCTION_CONTEXT_TYPE",
  202: "MODULE_CONTEXT_TYPE",
  203: "NATIVE_CONTEXT_TYPE",
  204: "SCRIPT_CONTEXT_TYPE",
  205: "WITH_CONTEXT_TYPE",
  206: "WEAK_FIXED_ARRAY_TYPE",
  207: "DESCRIPTOR_ARRAY_TYPE",
  208: "TRANSITION_ARRAY_TYPE",
  209: "CALL_HANDLER_INFO_TYPE",
  210: "CELL_TYPE",
  211: "CODE_DATA_CONTAINER_TYPE",
  212: "FEEDBACK_CELL_TYPE",
  213: "FEEDBACK_VECTOR_TYPE",
  214: "LOAD_HANDLER_TYPE",
  215: "PRE_PARSED_SCOPE_DATA_TYPE",
  216: "PROPERTY_ARRAY_TYPE",
  217: "PROPERTY_CELL_TYPE",
  218: "SHARED_FUNCTION_INFO_TYPE",
  219: "SMALL_ORDERED_HASH_MAP_TYPE",
  220: "SMALL_ORDERED_HASH_SET_TYPE",
  221: "STORE_HANDLER_TYPE",
  222: "UNCOMPILED_DATA_WITHOUT_PRE_PARSED_SCOPE_TYPE",
  223: "UNCOMPILED_DATA_WITH_PRE_PARSED_SCOPE_TYPE",
  224: "WEAK_ARRAY_LIST_TYPE",
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
  1081: "JS_WEAK_CELL_TYPE",
  1082: "JS_WEAK_FACTORY_CLEANUP_ITERATOR_TYPE",
  1083: "JS_WEAK_FACTORY_TYPE",
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
  ("RO_SPACE", 0x00139): (138, "FreeSpaceMap"),
  ("RO_SPACE", 0x00189): (132, "MetaMap"),
  ("RO_SPACE", 0x00209): (131, "NullMap"),
  ("RO_SPACE", 0x00279): (207, "DescriptorArrayMap"),
  ("RO_SPACE", 0x002d9): (206, "WeakFixedArrayMap"),
  ("RO_SPACE", 0x00329): (152, "OnePointerFillerMap"),
  ("RO_SPACE", 0x00379): (152, "TwoPointerFillerMap"),
  ("RO_SPACE", 0x003f9): (131, "UninitializedMap"),
  ("RO_SPACE", 0x00469): (8, "OneByteInternalizedStringMap"),
  ("RO_SPACE", 0x00509): (131, "UndefinedMap"),
  ("RO_SPACE", 0x00569): (129, "HeapNumberMap"),
  ("RO_SPACE", 0x005e9): (131, "TheHoleMap"),
  ("RO_SPACE", 0x00691): (131, "BooleanMap"),
  ("RO_SPACE", 0x00769): (136, "ByteArrayMap"),
  ("RO_SPACE", 0x007b9): (183, "FixedArrayMap"),
  ("RO_SPACE", 0x00809): (183, "FixedCOWArrayMap"),
  ("RO_SPACE", 0x00859): (185, "HashTableMap"),
  ("RO_SPACE", 0x008a9): (128, "SymbolMap"),
  ("RO_SPACE", 0x008f9): (72, "OneByteStringMap"),
  ("RO_SPACE", 0x00949): (194, "ScopeInfoMap"),
  ("RO_SPACE", 0x00999): (218, "SharedFunctionInfoMap"),
  ("RO_SPACE", 0x009e9): (133, "CodeMap"),
  ("RO_SPACE", 0x00a39): (201, "FunctionContextMap"),
  ("RO_SPACE", 0x00a89): (210, "CellMap"),
  ("RO_SPACE", 0x00ad9): (217, "GlobalPropertyCellMap"),
  ("RO_SPACE", 0x00b29): (135, "ForeignMap"),
  ("RO_SPACE", 0x00b79): (208, "TransitionArrayMap"),
  ("RO_SPACE", 0x00bc9): (213, "FeedbackVectorMap"),
  ("RO_SPACE", 0x00c69): (131, "ArgumentsMarkerMap"),
  ("RO_SPACE", 0x00d09): (131, "ExceptionMap"),
  ("RO_SPACE", 0x00da9): (131, "TerminationExceptionMap"),
  ("RO_SPACE", 0x00e51): (131, "OptimizedOutMap"),
  ("RO_SPACE", 0x00ef1): (131, "StaleRegisterMap"),
  ("RO_SPACE", 0x00f61): (203, "NativeContextMap"),
  ("RO_SPACE", 0x00fb1): (202, "ModuleContextMap"),
  ("RO_SPACE", 0x01001): (200, "EvalContextMap"),
  ("RO_SPACE", 0x01051): (204, "ScriptContextMap"),
  ("RO_SPACE", 0x010a1): (196, "AwaitContextMap"),
  ("RO_SPACE", 0x010f1): (197, "BlockContextMap"),
  ("RO_SPACE", 0x01141): (198, "CatchContextMap"),
  ("RO_SPACE", 0x01191): (205, "WithContextMap"),
  ("RO_SPACE", 0x011e1): (199, "DebugEvaluateContextMap"),
  ("RO_SPACE", 0x01231): (195, "ScriptContextTableMap"),
  ("RO_SPACE", 0x01281): (151, "FeedbackMetadataArrayMap"),
  ("RO_SPACE", 0x012d1): (183, "ArrayListMap"),
  ("RO_SPACE", 0x01321): (130, "BigIntMap"),
  ("RO_SPACE", 0x01371): (184, "ObjectBoilerplateDescriptionMap"),
  ("RO_SPACE", 0x013c1): (137, "BytecodeArrayMap"),
  ("RO_SPACE", 0x01411): (211, "CodeDataContainerMap"),
  ("RO_SPACE", 0x01461): (150, "FixedDoubleArrayMap"),
  ("RO_SPACE", 0x014b1): (189, "GlobalDictionaryMap"),
  ("RO_SPACE", 0x01501): (212, "ManyClosuresCellMap"),
  ("RO_SPACE", 0x01551): (183, "ModuleInfoMap"),
  ("RO_SPACE", 0x015a1): (134, "MutableHeapNumberMap"),
  ("RO_SPACE", 0x015f1): (188, "NameDictionaryMap"),
  ("RO_SPACE", 0x01641): (212, "NoClosuresCellMap"),
  ("RO_SPACE", 0x01691): (190, "NumberDictionaryMap"),
  ("RO_SPACE", 0x016e1): (212, "OneClosureCellMap"),
  ("RO_SPACE", 0x01731): (186, "OrderedHashMapMap"),
  ("RO_SPACE", 0x01781): (187, "OrderedHashSetMap"),
  ("RO_SPACE", 0x017d1): (215, "PreParsedScopeDataMap"),
  ("RO_SPACE", 0x01821): (216, "PropertyArrayMap"),
  ("RO_SPACE", 0x01871): (209, "SideEffectCallHandlerInfoMap"),
  ("RO_SPACE", 0x018c1): (209, "SideEffectFreeCallHandlerInfoMap"),
  ("RO_SPACE", 0x01911): (209, "NextCallSideEffectFreeCallHandlerInfoMap"),
  ("RO_SPACE", 0x01961): (191, "SimpleNumberDictionaryMap"),
  ("RO_SPACE", 0x019b1): (183, "SloppyArgumentsElementsMap"),
  ("RO_SPACE", 0x01a01): (219, "SmallOrderedHashMapMap"),
  ("RO_SPACE", 0x01a51): (220, "SmallOrderedHashSetMap"),
  ("RO_SPACE", 0x01aa1): (192, "StringTableMap"),
  ("RO_SPACE", 0x01af1): (222, "UncompiledDataWithoutPreParsedScopeMap"),
  ("RO_SPACE", 0x01b41): (223, "UncompiledDataWithPreParsedScopeMap"),
  ("RO_SPACE", 0x01b91): (224, "WeakArrayListMap"),
  ("RO_SPACE", 0x01be1): (193, "EphemeronHashTableMap"),
  ("RO_SPACE", 0x01c31): (106, "NativeSourceStringMap"),
  ("RO_SPACE", 0x01c81): (64, "StringMap"),
  ("RO_SPACE", 0x01cd1): (73, "ConsOneByteStringMap"),
  ("RO_SPACE", 0x01d21): (65, "ConsStringMap"),
  ("RO_SPACE", 0x01d71): (77, "ThinOneByteStringMap"),
  ("RO_SPACE", 0x01dc1): (69, "ThinStringMap"),
  ("RO_SPACE", 0x01e11): (67, "SlicedStringMap"),
  ("RO_SPACE", 0x01e61): (75, "SlicedOneByteStringMap"),
  ("RO_SPACE", 0x01eb1): (66, "ExternalStringMap"),
  ("RO_SPACE", 0x01f01): (82, "ExternalStringWithOneByteDataMap"),
  ("RO_SPACE", 0x01f51): (74, "ExternalOneByteStringMap"),
  ("RO_SPACE", 0x01fa1): (98, "UncachedExternalStringMap"),
  ("RO_SPACE", 0x01ff1): (114, "UncachedExternalStringWithOneByteDataMap"),
  ("RO_SPACE", 0x02041): (0, "InternalizedStringMap"),
  ("RO_SPACE", 0x02091): (2, "ExternalInternalizedStringMap"),
  ("RO_SPACE", 0x020e1): (18, "ExternalInternalizedStringWithOneByteDataMap"),
  ("RO_SPACE", 0x02131): (10, "ExternalOneByteInternalizedStringMap"),
  ("RO_SPACE", 0x02181): (34, "UncachedExternalInternalizedStringMap"),
  ("RO_SPACE", 0x021d1): (50, "UncachedExternalInternalizedStringWithOneByteDataMap"),
  ("RO_SPACE", 0x02221): (42, "UncachedExternalOneByteInternalizedStringMap"),
  ("RO_SPACE", 0x02271): (106, "UncachedExternalOneByteStringMap"),
  ("RO_SPACE", 0x022c1): (140, "FixedUint8ArrayMap"),
  ("RO_SPACE", 0x02311): (139, "FixedInt8ArrayMap"),
  ("RO_SPACE", 0x02361): (142, "FixedUint16ArrayMap"),
  ("RO_SPACE", 0x023b1): (141, "FixedInt16ArrayMap"),
  ("RO_SPACE", 0x02401): (144, "FixedUint32ArrayMap"),
  ("RO_SPACE", 0x02451): (143, "FixedInt32ArrayMap"),
  ("RO_SPACE", 0x024a1): (145, "FixedFloat32ArrayMap"),
  ("RO_SPACE", 0x024f1): (146, "FixedFloat64ArrayMap"),
  ("RO_SPACE", 0x02541): (147, "FixedUint8ClampedArrayMap"),
  ("RO_SPACE", 0x02591): (149, "FixedBigUint64ArrayMap"),
  ("RO_SPACE", 0x025e1): (148, "FixedBigInt64ArrayMap"),
  ("RO_SPACE", 0x02631): (131, "SelfReferenceMarkerMap"),
  ("RO_SPACE", 0x02699): (171, "Tuple2Map"),
  ("RO_SPACE", 0x02739): (173, "ArrayBoilerplateDescriptionMap"),
  ("RO_SPACE", 0x02a79): (161, "InterceptorInfoMap"),
  ("RO_SPACE", 0x05079): (153, "AccessCheckInfoMap"),
  ("RO_SPACE", 0x050c9): (154, "AccessorInfoMap"),
  ("RO_SPACE", 0x05119): (155, "AccessorPairMap"),
  ("RO_SPACE", 0x05169): (156, "AliasedArgumentsEntryMap"),
  ("RO_SPACE", 0x051b9): (157, "AllocationMementoMap"),
  ("RO_SPACE", 0x05209): (158, "AsyncGeneratorRequestMap"),
  ("RO_SPACE", 0x05259): (159, "DebugInfoMap"),
  ("RO_SPACE", 0x052a9): (160, "FunctionTemplateInfoMap"),
  ("RO_SPACE", 0x052f9): (162, "InterpreterDataMap"),
  ("RO_SPACE", 0x05349): (163, "ModuleInfoEntryMap"),
  ("RO_SPACE", 0x05399): (164, "ModuleMap"),
  ("RO_SPACE", 0x053e9): (165, "ObjectTemplateInfoMap"),
  ("RO_SPACE", 0x05439): (166, "PromiseCapabilityMap"),
  ("RO_SPACE", 0x05489): (167, "PromiseReactionMap"),
  ("RO_SPACE", 0x054d9): (168, "PrototypeInfoMap"),
  ("RO_SPACE", 0x05529): (169, "ScriptMap"),
  ("RO_SPACE", 0x05579): (170, "StackFrameInfoMap"),
  ("RO_SPACE", 0x055c9): (172, "Tuple3Map"),
  ("RO_SPACE", 0x05619): (174, "WasmDebugInfoMap"),
  ("RO_SPACE", 0x05669): (175, "WasmExportedFunctionDataMap"),
  ("RO_SPACE", 0x056b9): (176, "CallableTaskMap"),
  ("RO_SPACE", 0x05709): (177, "CallbackTaskMap"),
  ("RO_SPACE", 0x05759): (178, "PromiseFulfillReactionJobTaskMap"),
  ("RO_SPACE", 0x057a9): (179, "PromiseRejectReactionJobTaskMap"),
  ("RO_SPACE", 0x057f9): (180, "PromiseResolveThenableJobTaskMap"),
  ("RO_SPACE", 0x05849): (181, "MicrotaskQueueMap"),
  ("RO_SPACE", 0x05899): (182, "AllocationSiteWithWeakNextMap"),
  ("RO_SPACE", 0x058e9): (182, "AllocationSiteWithoutWeakNextMap"),
  ("RO_SPACE", 0x05939): (214, "LoadHandler1Map"),
  ("RO_SPACE", 0x05989): (214, "LoadHandler2Map"),
  ("RO_SPACE", 0x059d9): (214, "LoadHandler3Map"),
  ("RO_SPACE", 0x05a29): (221, "StoreHandler0Map"),
  ("RO_SPACE", 0x05a79): (221, "StoreHandler1Map"),
  ("RO_SPACE", 0x05ac9): (221, "StoreHandler2Map"),
  ("RO_SPACE", 0x05b19): (221, "StoreHandler3Map"),
  ("MAP_SPACE", 0x00139): (1057, "ExternalMap"),
  ("MAP_SPACE", 0x00189): (1073, "JSMessageObjectMap"),
}

# List of known V8 objects.
KNOWN_OBJECTS = {
  ("RO_SPACE", 0x001d9): "NullValue",
  ("RO_SPACE", 0x00259): "EmptyDescriptorArray",
  ("RO_SPACE", 0x002c9): "EmptyWeakFixedArray",
  ("RO_SPACE", 0x003c9): "UninitializedValue",
  ("RO_SPACE", 0x004d9): "UndefinedValue",
  ("RO_SPACE", 0x00559): "NanValue",
  ("RO_SPACE", 0x005b9): "TheHoleValue",
  ("RO_SPACE", 0x00651): "HoleNanValue",
  ("RO_SPACE", 0x00661): "TrueValue",
  ("RO_SPACE", 0x00711): "FalseValue",
  ("RO_SPACE", 0x00759): "empty_string",
  ("RO_SPACE", 0x00c19): "EmptyScopeInfo",
  ("RO_SPACE", 0x00c29): "EmptyFixedArray",
  ("RO_SPACE", 0x00c39): "ArgumentsMarker",
  ("RO_SPACE", 0x00cd9): "Exception",
  ("RO_SPACE", 0x00d79): "TerminationException",
  ("RO_SPACE", 0x00e21): "OptimizedOut",
  ("RO_SPACE", 0x00ec1): "StaleRegister",
  ("RO_SPACE", 0x02681): "EmptyEnumCache",
  ("RO_SPACE", 0x026e9): "EmptyPropertyArray",
  ("RO_SPACE", 0x026f9): "EmptyByteArray",
  ("RO_SPACE", 0x02709): "EmptyObjectBoilerplateDescription",
  ("RO_SPACE", 0x02721): "EmptyArrayBoilerplateDescription",
  ("RO_SPACE", 0x02789): "EmptyFixedUint8Array",
  ("RO_SPACE", 0x027a9): "EmptyFixedInt8Array",
  ("RO_SPACE", 0x027c9): "EmptyFixedUint16Array",
  ("RO_SPACE", 0x027e9): "EmptyFixedInt16Array",
  ("RO_SPACE", 0x02809): "EmptyFixedUint32Array",
  ("RO_SPACE", 0x02829): "EmptyFixedInt32Array",
  ("RO_SPACE", 0x02849): "EmptyFixedFloat32Array",
  ("RO_SPACE", 0x02869): "EmptyFixedFloat64Array",
  ("RO_SPACE", 0x02889): "EmptyFixedUint8ClampedArray",
  ("RO_SPACE", 0x028a9): "EmptyFixedBigUint64Array",
  ("RO_SPACE", 0x028c9): "EmptyFixedBigInt64Array",
  ("RO_SPACE", 0x028e9): "EmptySloppyArgumentsElements",
  ("RO_SPACE", 0x02909): "EmptySlowElementDictionary",
  ("RO_SPACE", 0x02951): "EmptyOrderedHashMap",
  ("RO_SPACE", 0x02979): "EmptyOrderedHashSet",
  ("RO_SPACE", 0x029a1): "EmptyFeedbackMetadata",
  ("RO_SPACE", 0x029b1): "EmptyPropertyCell",
  ("RO_SPACE", 0x029d9): "EmptyPropertyDictionary",
  ("RO_SPACE", 0x02a29): "NoOpInterceptorInfo",
  ("RO_SPACE", 0x02ac9): "EmptyWeakArrayList",
  ("RO_SPACE", 0x02ae1): "InfinityValue",
  ("RO_SPACE", 0x02af1): "MinusZeroValue",
  ("RO_SPACE", 0x02b01): "MinusInfinityValue",
  ("RO_SPACE", 0x02b11): "SelfReferenceMarker",
  ("OLD_SPACE", 0x00139): "ArgumentsIteratorAccessor",
  ("OLD_SPACE", 0x001a9): "ArrayLengthAccessor",
  ("OLD_SPACE", 0x00219): "BoundFunctionLengthAccessor",
  ("OLD_SPACE", 0x00289): "BoundFunctionNameAccessor",
  ("OLD_SPACE", 0x002f9): "ErrorStackAccessor",
  ("OLD_SPACE", 0x00369): "FunctionArgumentsAccessor",
  ("OLD_SPACE", 0x003d9): "FunctionCallerAccessor",
  ("OLD_SPACE", 0x00449): "FunctionNameAccessor",
  ("OLD_SPACE", 0x004b9): "FunctionLengthAccessor",
  ("OLD_SPACE", 0x00529): "FunctionPrototypeAccessor",
  ("OLD_SPACE", 0x00599): "StringLengthAccessor",
  ("OLD_SPACE", 0x00609): "InvalidPrototypeValidityCell",
  ("OLD_SPACE", 0x00619): "EmptyScript",
  ("OLD_SPACE", 0x00699): "ManyClosuresCell",
  ("OLD_SPACE", 0x006a9): "ArrayConstructorProtector",
  ("OLD_SPACE", 0x006b9): "NoElementsProtector",
  ("OLD_SPACE", 0x006e1): "IsConcatSpreadableProtector",
  ("OLD_SPACE", 0x006f1): "ArraySpeciesProtector",
  ("OLD_SPACE", 0x00719): "TypedArraySpeciesProtector",
  ("OLD_SPACE", 0x00741): "PromiseSpeciesProtector",
  ("OLD_SPACE", 0x00769): "StringLengthProtector",
  ("OLD_SPACE", 0x00779): "ArrayIteratorProtector",
  ("OLD_SPACE", 0x007a1): "ArrayBufferNeuteringProtector",
  ("OLD_SPACE", 0x007c9): "PromiseHookProtector",
  ("OLD_SPACE", 0x007f1): "PromiseResolveProtector",
  ("OLD_SPACE", 0x00801): "MapIteratorProtector",
  ("OLD_SPACE", 0x00829): "PromiseThenProtector",
  ("OLD_SPACE", 0x00851): "SetIteratorProtector",
  ("OLD_SPACE", 0x00879): "StringIteratorProtector",
  ("OLD_SPACE", 0x008a1): "SingleCharacterStringCache",
  ("OLD_SPACE", 0x010b1): "StringSplitCache",
  ("OLD_SPACE", 0x018c1): "RegExpMultipleCache",
  ("OLD_SPACE", 0x020d1): "DefaultMicrotaskQueue",
  ("OLD_SPACE", 0x020e9): "BuiltinsConstantsTable",
  ("OLD_SPACE", 0x02a11): "HashSeed",
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
