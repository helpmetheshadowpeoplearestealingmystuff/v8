# Copyright 2013 the V8 project authors. All rights reserved.
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of Google Inc. nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# This file is automatically generated from the V8 source and should not
# be modified manually, run 'make grokdump' instead to update this file.

# List of known V8 instance types.
INSTANCE_TYPES = {
  64: "STRING_TYPE",
  68: "ASCII_STRING_TYPE",
  65: "CONS_STRING_TYPE",
  69: "CONS_ASCII_STRING_TYPE",
  67: "SLICED_STRING_TYPE",
  66: "EXTERNAL_STRING_TYPE",
  70: "EXTERNAL_ASCII_STRING_TYPE",
  74: "EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE",
  82: "SHORT_EXTERNAL_STRING_TYPE",
  86: "SHORT_EXTERNAL_ASCII_STRING_TYPE",
  90: "SHORT_EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE",
  0: "INTERNALIZED_STRING_TYPE",
  4: "ASCII_INTERNALIZED_STRING_TYPE",
  1: "CONS_INTERNALIZED_STRING_TYPE",
  5: "CONS_ASCII_INTERNALIZED_STRING_TYPE",
  2: "EXTERNAL_INTERNALIZED_STRING_TYPE",
  6: "EXTERNAL_ASCII_INTERNALIZED_STRING_TYPE",
  10: "EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE",
  18: "SHORT_EXTERNAL_INTERNALIZED_STRING_TYPE",
  22: "SHORT_EXTERNAL_ASCII_INTERNALIZED_STRING_TYPE",
  26: "SHORT_EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE",
  128: "SYMBOL_TYPE",
  129: "MAP_TYPE",
  130: "CODE_TYPE",
  131: "ODDBALL_TYPE",
  132: "CELL_TYPE",
  133: "PROPERTY_CELL_TYPE",
  134: "HEAP_NUMBER_TYPE",
  135: "FOREIGN_TYPE",
  136: "BYTE_ARRAY_TYPE",
  137: "FREE_SPACE_TYPE",
  138: "EXTERNAL_BYTE_ARRAY_TYPE",
  139: "EXTERNAL_UNSIGNED_BYTE_ARRAY_TYPE",
  140: "EXTERNAL_SHORT_ARRAY_TYPE",
  141: "EXTERNAL_UNSIGNED_SHORT_ARRAY_TYPE",
  142: "EXTERNAL_INT_ARRAY_TYPE",
  143: "EXTERNAL_UNSIGNED_INT_ARRAY_TYPE",
  144: "EXTERNAL_FLOAT_ARRAY_TYPE",
  145: "EXTERNAL_DOUBLE_ARRAY_TYPE",
  146: "EXTERNAL_PIXEL_ARRAY_TYPE",
  148: "FILLER_TYPE",
  149: "DECLARED_ACCESSOR_DESCRIPTOR_TYPE",
  150: "DECLARED_ACCESSOR_INFO_TYPE",
  151: "EXECUTABLE_ACCESSOR_INFO_TYPE",
  152: "ACCESSOR_PAIR_TYPE",
  153: "ACCESS_CHECK_INFO_TYPE",
  154: "INTERCEPTOR_INFO_TYPE",
  155: "CALL_HANDLER_INFO_TYPE",
  156: "FUNCTION_TEMPLATE_INFO_TYPE",
  157: "OBJECT_TEMPLATE_INFO_TYPE",
  158: "SIGNATURE_INFO_TYPE",
  159: "TYPE_SWITCH_INFO_TYPE",
  161: "ALLOCATION_MEMENTO_TYPE",
  160: "ALLOCATION_SITE_TYPE",
  162: "SCRIPT_TYPE",
  163: "CODE_CACHE_TYPE",
  164: "POLYMORPHIC_CODE_CACHE_TYPE",
  165: "TYPE_FEEDBACK_INFO_TYPE",
  166: "ALIASED_ARGUMENTS_ENTRY_TYPE",
  167: "BOX_TYPE",
  170: "FIXED_ARRAY_TYPE",
  147: "FIXED_DOUBLE_ARRAY_TYPE",
  171: "SHARED_FUNCTION_INFO_TYPE",
  172: "JS_MESSAGE_OBJECT_TYPE",
  175: "JS_VALUE_TYPE",
  176: "JS_DATE_TYPE",
  177: "JS_OBJECT_TYPE",
  178: "JS_CONTEXT_EXTENSION_OBJECT_TYPE",
  179: "JS_GENERATOR_OBJECT_TYPE",
  180: "JS_MODULE_TYPE",
  181: "JS_GLOBAL_OBJECT_TYPE",
  182: "JS_BUILTINS_OBJECT_TYPE",
  183: "JS_GLOBAL_PROXY_TYPE",
  184: "JS_ARRAY_TYPE",
  185: "JS_ARRAY_BUFFER_TYPE",
  186: "JS_TYPED_ARRAY_TYPE",
  187: "JS_DATA_VIEW_TYPE",
  174: "JS_PROXY_TYPE",
  190: "JS_WEAK_MAP_TYPE",
  191: "JS_WEAK_SET_TYPE",
  192: "JS_REGEXP_TYPE",
  193: "JS_FUNCTION_TYPE",
  173: "JS_FUNCTION_PROXY_TYPE",
  168: "DEBUG_INFO_TYPE",
  169: "BREAK_POINT_INFO_TYPE",
}

# List of known V8 maps.
KNOWN_MAPS = {
  0x08081: (136, "ByteArrayMap"),
  0x080a9: (129, "MetaMap"),
  0x080d1: (131, "OddballMap"),
  0x080f9: (4, "AsciiInternalizedStringMap"),
  0x08121: (170, "FixedArrayMap"),
  0x08149: (134, "HeapNumberMap"),
  0x08171: (137, "FreeSpaceMap"),
  0x08199: (148, "OnePointerFillerMap"),
  0x081c1: (148, "TwoPointerFillerMap"),
  0x081e9: (132, "CellMap"),
  0x08211: (133, "GlobalPropertyCellMap"),
  0x08239: (171, "SharedFunctionInfoMap"),
  0x08261: (170, "NativeContextMap"),
  0x08289: (130, "CodeMap"),
  0x082b1: (170, "ScopeInfoMap"),
  0x082d9: (170, "FixedCOWArrayMap"),
  0x08301: (147, "FixedDoubleArrayMap"),
  0x08329: (170, "HashTableMap"),
  0x08351: (128, "SymbolMap"),
  0x08379: (64, "StringMap"),
  0x083a1: (68, "AsciiStringMap"),
  0x083c9: (65, "ConsStringMap"),
  0x083f1: (69, "ConsAsciiStringMap"),
  0x08419: (67, "SlicedStringMap"),
  0x08441: (71, "SlicedAsciiStringMap"),
  0x08469: (66, "ExternalStringMap"),
  0x08491: (74, "ExternalStringWithOneByteDataMap"),
  0x084b9: (70, "ExternalAsciiStringMap"),
  0x084e1: (82, "ShortExternalStringMap"),
  0x08509: (90, "ShortExternalStringWithOneByteDataMap"),
  0x08531: (0, "InternalizedStringMap"),
  0x08559: (1, "ConsInternalizedStringMap"),
  0x08581: (5, "ConsAsciiInternalizedStringMap"),
  0x085a9: (2, "ExternalInternalizedStringMap"),
  0x085d1: (10, "ExternalInternalizedStringWithOneByteDataMap"),
  0x085f9: (6, "ExternalAsciiInternalizedStringMap"),
  0x08621: (18, "ShortExternalInternalizedStringMap"),
  0x08649: (26, "ShortExternalInternalizedStringWithOneByteDataMap"),
  0x08671: (22, "ShortExternalAsciiInternalizedStringMap"),
  0x08699: (86, "ShortExternalAsciiStringMap"),
  0x086c1: (64, "UndetectableStringMap"),
  0x086e9: (68, "UndetectableAsciiStringMap"),
  0x08711: (138, "ExternalByteArrayMap"),
  0x08739: (139, "ExternalUnsignedByteArrayMap"),
  0x08761: (140, "ExternalShortArrayMap"),
  0x08789: (141, "ExternalUnsignedShortArrayMap"),
  0x087b1: (142, "ExternalIntArrayMap"),
  0x087d9: (143, "ExternalUnsignedIntArrayMap"),
  0x08801: (144, "ExternalFloatArrayMap"),
  0x08829: (145, "ExternalDoubleArrayMap"),
  0x08851: (146, "ExternalPixelArrayMap"),
  0x08879: (170, "NonStrictArgumentsElementsMap"),
  0x088a1: (170, "FunctionContextMap"),
  0x088c9: (170, "CatchContextMap"),
  0x088f1: (170, "WithContextMap"),
  0x08919: (170, "BlockContextMap"),
  0x08941: (170, "ModuleContextMap"),
  0x08969: (170, "GlobalContextMap"),
  0x08991: (172, "JSMessageObjectMap"),
  0x089b9: (135, "ForeignMap"),
  0x089e1: (177, "NeanderMap"),
  0x08a09: (161, "AllocationMementoMap"),
  0x08a31: (160, "AllocationSiteMap"),
  0x08a59: (164, "PolymorphicCodeCacheMap"),
  0x08a81: (162, "ScriptMap"),
  0x08ad1: (177, "ExternalMap"),
  0x08af9: (167, "BoxMap"),
  0x08b21: (149, "DeclaredAccessorDescriptorMap"),
  0x08b49: (150, "DeclaredAccessorInfoMap"),
  0x08b71: (151, "ExecutableAccessorInfoMap"),
  0x08b99: (152, "AccessorPairMap"),
  0x08bc1: (153, "AccessCheckInfoMap"),
  0x08be9: (154, "InterceptorInfoMap"),
  0x08c11: (155, "CallHandlerInfoMap"),
  0x08c39: (156, "FunctionTemplateInfoMap"),
  0x08c61: (157, "ObjectTemplateInfoMap"),
  0x08c89: (158, "SignatureInfoMap"),
  0x08cb1: (159, "TypeSwitchInfoMap"),
  0x08cd9: (163, "CodeCacheMap"),
  0x08d01: (165, "TypeFeedbackInfoMap"),
  0x08d29: (166, "AliasedArgumentsEntryMap"),
  0x08d51: (168, "DebugInfoMap"),
  0x08d79: (169, "BreakPointInfoMap"),
}

# List of known V8 objects.
KNOWN_OBJECTS = {
  ("OLD_POINTER_SPACE", 0x08081): "NullValue",
  ("OLD_POINTER_SPACE", 0x08091): "UndefinedValue",
  ("OLD_POINTER_SPACE", 0x080a1): "InstanceofCacheMap",
  ("OLD_POINTER_SPACE", 0x080b1): "TrueValue",
  ("OLD_POINTER_SPACE", 0x080c1): "FalseValue",
  ("OLD_POINTER_SPACE", 0x080d1): "UninitializedValue",
  ("OLD_POINTER_SPACE", 0x080e1): "NoInterceptorResultSentinel",
  ("OLD_POINTER_SPACE", 0x080f1): "ArgumentsMarker",
  ("OLD_POINTER_SPACE", 0x08101): "NumberStringCache",
  ("OLD_POINTER_SPACE", 0x08909): "SingleCharacterStringCache",
  ("OLD_POINTER_SPACE", 0x08d11): "StringSplitCache",
  ("OLD_POINTER_SPACE", 0x09119): "RegExpMultipleCache",
  ("OLD_POINTER_SPACE", 0x09521): "TerminationException",
  ("OLD_POINTER_SPACE", 0x09531): "MessageListeners",
  ("OLD_POINTER_SPACE", 0x0954d): "CodeStubs",
  ("OLD_POINTER_SPACE", 0x0a9d9): "NonMonomorphicCache",
  ("OLD_POINTER_SPACE", 0x0afed): "PolymorphicCodeCache",
  ("OLD_POINTER_SPACE", 0x0aff5): "NativesSourceCache",
  ("OLD_POINTER_SPACE", 0x0b035): "EmptyScript",
  ("OLD_POINTER_SPACE", 0x0b06d): "IntrinsicFunctionNames",
  ("OLD_POINTER_SPACE", 0x0e089): "ObservationState",
  ("OLD_POINTER_SPACE", 0x0e095): "FrozenSymbol",
  ("OLD_POINTER_SPACE", 0x0e0a1): "ElementsTransitionSymbol",
  ("OLD_POINTER_SPACE", 0x0e0ad): "EmptySlowElementDictionary",
  ("OLD_POINTER_SPACE", 0x0e249): "ObservedSymbol",
  ("OLD_POINTER_SPACE", 0x27631): "StringTable",
  ("OLD_DATA_SPACE", 0x08099): "EmptyDescriptorArray",
  ("OLD_DATA_SPACE", 0x080a1): "EmptyFixedArray",
  ("OLD_DATA_SPACE", 0x080a9): "NanValue",
  ("OLD_DATA_SPACE", 0x08141): "EmptyByteArray",
  ("OLD_DATA_SPACE", 0x08269): "EmptyExternalByteArray",
  ("OLD_DATA_SPACE", 0x08275): "EmptyExternalUnsignedByteArray",
  ("OLD_DATA_SPACE", 0x08281): "EmptyExternalShortArray",
  ("OLD_DATA_SPACE", 0x0828d): "EmptyExternalUnsignedShortArray",
  ("OLD_DATA_SPACE", 0x08299): "EmptyExternalIntArray",
  ("OLD_DATA_SPACE", 0x082a5): "EmptyExternalUnsignedIntArray",
  ("OLD_DATA_SPACE", 0x082b1): "EmptyExternalFloatArray",
  ("OLD_DATA_SPACE", 0x082bd): "EmptyExternalDoubleArray",
  ("OLD_DATA_SPACE", 0x082c9): "EmptyExternalPixelArray",
  ("OLD_DATA_SPACE", 0x082d5): "InfinityValue",
  ("OLD_DATA_SPACE", 0x082e1): "MinusZeroValue",
  ("CODE_SPACE", 0x0e801): "JsConstructEntryCode",
  ("CODE_SPACE", 0x176e1): "JsEntryCode",
}
