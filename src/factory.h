// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_FACTORY_H_
#define V8_FACTORY_H_

#include "globals.h"
#include "handles.h"
#include "heap.h"

namespace v8 {
namespace internal {

// Interface for handle based allocation.

class Factory {
 public:
  // Allocate a new uninitialized fixed array.
  Handle<FixedArray> NewFixedArray(
      int size,
      PretenureFlag pretenure = NOT_TENURED);

  // Allocate a new fixed array with non-existing entries (the hole).
  Handle<FixedArray> NewFixedArrayWithHoles(
      int size,
      PretenureFlag pretenure = NOT_TENURED);

  // Allocate a new uninitialized fixed double array.
  Handle<FixedArray> NewFixedDoubleArray(
      int size,
      PretenureFlag pretenure = NOT_TENURED);

  Handle<NumberDictionary> NewNumberDictionary(int at_least_space_for);

  Handle<StringDictionary> NewStringDictionary(int at_least_space_for);

  Handle<ObjectHashTable> NewObjectHashTable(int at_least_space_for);

  Handle<DescriptorArray> NewDescriptorArray(int number_of_descriptors);
  Handle<DeoptimizationInputData> NewDeoptimizationInputData(
      int deopt_entry_count,
      PretenureFlag pretenure);
  Handle<DeoptimizationOutputData> NewDeoptimizationOutputData(
      int deopt_entry_count,
      PretenureFlag pretenure);

  Handle<String> LookupSymbol(Vector<const char> str);
  Handle<String> LookupSymbol(Handle<String> str);
  Handle<String> LookupAsciiSymbol(Vector<const char> str);
  Handle<String> LookupAsciiSymbol(Handle<SeqAsciiString>,
                                   int from,
                                   int length);
  Handle<String> LookupTwoByteSymbol(Vector<const uc16> str);
  Handle<String> LookupAsciiSymbol(const char* str) {
    return LookupSymbol(CStrVector(str));
  }


  // String creation functions.  Most of the string creation functions take
  // a Heap::PretenureFlag argument to optionally request that they be
  // allocated in the old generation.  The pretenure flag defaults to
  // DONT_TENURE.
  //
  // Creates a new String object.  There are two String encodings: ASCII and
  // two byte.  One should choose between the three string factory functions
  // based on the encoding of the string buffer that the string is
  // initialized from.
  //   - ...FromAscii initializes the string from a buffer that is ASCII
  //     encoded (it does not check that the buffer is ASCII encoded) and
  //     the result will be ASCII encoded.
  //   - ...FromUtf8 initializes the string from a buffer that is UTF-8
  //     encoded.  If the characters are all single-byte characters, the
  //     result will be ASCII encoded, otherwise it will converted to two
  //     byte.
  //   - ...FromTwoByte initializes the string from a buffer that is two
  //     byte encoded.  If the characters are all single-byte characters,
  //     the result will be converted to ASCII, otherwise it will be left as
  //     two byte.
  //
  // ASCII strings are pretenured when used as keys in the SourceCodeCache.
  Handle<String> NewStringFromAscii(
      Vector<const char> str,
      PretenureFlag pretenure = NOT_TENURED);

  // UTF8 strings are pretenured when used for regexp literal patterns and
  // flags in the parser.
  Handle<String> NewStringFromUtf8(
      Vector<const char> str,
      PretenureFlag pretenure = NOT_TENURED);

  Handle<String> NewStringFromTwoByte(
      Vector<const uc16> str,
      PretenureFlag pretenure = NOT_TENURED);

  // Allocates and partially initializes an ASCII or TwoByte String. The
  // characters of the string are uninitialized. Currently used in regexp code
  // only, where they are pretenured.
  Handle<SeqAsciiString> NewRawAsciiString(
      int length,
      PretenureFlag pretenure = NOT_TENURED);
  Handle<SeqTwoByteString> NewRawTwoByteString(
      int length,
      PretenureFlag pretenure = NOT_TENURED);

  // Create a new cons string object which consists of a pair of strings.
  Handle<String> NewConsString(Handle<String> first,
                               Handle<String> second);

  // Create a new string object which holds a substring of a string.
  Handle<String> NewSubString(Handle<String> str,
                              int begin,
                              int end);

  // Create a new string object which holds a proper substring of a string.
  Handle<String> NewProperSubString(Handle<String> str,
                                    int begin,
                                    int end);

  // Creates a new external String object.  There are two String encodings
  // in the system: ASCII and two byte.  Unlike other String types, it does
  // not make sense to have a UTF-8 factory function for external strings,
  // because we cannot change the underlying buffer.
  Handle<String> NewExternalStringFromAscii(
      ExternalAsciiString::Resource* resource);
  Handle<String> NewExternalStringFromTwoByte(
      ExternalTwoByteString::Resource* resource);

  // Create a global (but otherwise uninitialized) context.
  Handle<Context> NewGlobalContext();

  // Create a function context.
  Handle<Context> NewFunctionContext(int length,
                                     Handle<JSFunction> function);

  // Create a catch context.
  Handle<Context> NewCatchContext(Handle<JSFunction> function,
                                  Handle<Context> previous,
                                  Handle<String> name,
                                  Handle<Object> thrown_object);

  // Create a 'with' context.
  Handle<Context> NewWithContext(Handle<JSFunction> function,
                                 Handle<Context> previous,
                                 Handle<JSObject> extension);

  // Return the Symbol matching the passed in string.
  Handle<String> SymbolFromString(Handle<String> value);

  // Allocate a new struct.  The struct is pretenured (allocated directly in
  // the old generation).
  Handle<Struct> NewStruct(InstanceType type);

  Handle<AccessorInfo> NewAccessorInfo();

  Handle<Script> NewScript(Handle<String> source);

  // Foreign objects are pretenured when allocated by the bootstrapper.
  Handle<Foreign> NewForeign(Address addr,
                             PretenureFlag pretenure = NOT_TENURED);

  // Allocate a new foreign object.  The foreign is pretenured (allocated
  // directly in the old generation).
  Handle<Foreign> NewForeign(const AccessorDescriptor* foreign);

  Handle<ByteArray> NewByteArray(int length,
                                 PretenureFlag pretenure = NOT_TENURED);

  Handle<ExternalArray> NewExternalArray(
      int length,
      ExternalArrayType array_type,
      void* external_pointer,
      PretenureFlag pretenure = NOT_TENURED);

  Handle<JSGlobalPropertyCell> NewJSGlobalPropertyCell(
      Handle<Object> value);

  Handle<Map> NewMap(InstanceType type, int instance_size);

  Handle<JSObject> NewFunctionPrototype(Handle<JSFunction> function);

  Handle<Map> CopyMapDropDescriptors(Handle<Map> map);

  // Copy the map adding more inobject properties if possible without
  // overflowing the instance size.
  Handle<Map> CopyMap(Handle<Map> map, int extra_inobject_props);

  Handle<Map> CopyMapDropTransitions(Handle<Map> map);

  Handle<Map> GetFastElementsMap(Handle<Map> map);

  Handle<Map> GetSlowElementsMap(Handle<Map> map);

  Handle<Map> GetExternalArrayElementsMap(Handle<Map> map,
                                          ExternalArrayType array_type,
                                          bool safe_to_add_transition);

  Handle<FixedArray> CopyFixedArray(Handle<FixedArray> array);

  // Numbers (eg, literals) are pretenured by the parser.
  Handle<Object> NewNumber(double value,
                           PretenureFlag pretenure = NOT_TENURED);

  Handle<Object> NewNumberFromInt(int value);
  Handle<Object> NewNumberFromUint(uint32_t value);

  // These objects are used by the api to create env-independent data
  // structures in the heap.
  Handle<JSObject> NewNeanderObject();

  Handle<JSObject> NewArgumentsObject(Handle<Object> callee, int length);

  // JS objects are pretenured when allocated by the bootstrapper and
  // runtime.
  Handle<JSObject> NewJSObject(Handle<JSFunction> constructor,
                               PretenureFlag pretenure = NOT_TENURED);

  // Global objects are pretenured.
  Handle<GlobalObject> NewGlobalObject(Handle<JSFunction> constructor);

  // JS objects are pretenured when allocated by the bootstrapper and
  // runtime.
  Handle<JSObject> NewJSObjectFromMap(Handle<Map> map);

  // JS arrays are pretenured when allocated by the parser.
  Handle<JSArray> NewJSArray(int capacity,
                             PretenureFlag pretenure = NOT_TENURED);

  Handle<JSArray> NewJSArrayWithElements(
      Handle<FixedArray> elements,
      PretenureFlag pretenure = NOT_TENURED);

  Handle<JSProxy> NewJSProxy(Handle<Object> handler, Handle<Object> prototype);

  // Change the type of the argument into a regular JS object and reinitialize.
  void BecomeJSObject(Handle<JSProxy> object);

  Handle<JSFunction> NewFunction(Handle<String> name,
                                 Handle<Object> prototype);

  Handle<JSFunction> NewFunctionWithoutPrototype(
      Handle<String> name,
      StrictModeFlag strict_mode);

  Handle<JSFunction> NewFunction(Handle<Object> super, bool is_global);

  Handle<JSFunction> BaseNewFunctionFromSharedFunctionInfo(
      Handle<SharedFunctionInfo> function_info,
      Handle<Map> function_map,
      PretenureFlag pretenure);

  Handle<JSFunction> NewFunctionFromSharedFunctionInfo(
      Handle<SharedFunctionInfo> function_info,
      Handle<Context> context,
      PretenureFlag pretenure = TENURED);

  Handle<Code> NewCode(const CodeDesc& desc,
                       Code::Flags flags,
                       Handle<Object> self_reference,
                       bool immovable = false);

  Handle<Code> CopyCode(Handle<Code> code);

  Handle<Code> CopyCode(Handle<Code> code, Vector<byte> reloc_info);

  Handle<Object> ToObject(Handle<Object> object);
  Handle<Object> ToObject(Handle<Object> object,
                          Handle<Context> global_context);

  // Interface for creating error objects.

  Handle<Object> NewError(const char* maker, const char* type,
                          Handle<JSArray> args);
  Handle<Object> NewError(const char* maker, const char* type,
                          Vector< Handle<Object> > args);
  Handle<Object> NewError(const char* type,
                          Vector< Handle<Object> > args);
  Handle<Object> NewError(Handle<String> message);
  Handle<Object> NewError(const char* constructor,
                          Handle<String> message);

  Handle<Object> NewTypeError(const char* type,
                              Vector< Handle<Object> > args);
  Handle<Object> NewTypeError(Handle<String> message);

  Handle<Object> NewRangeError(const char* type,
                               Vector< Handle<Object> > args);
  Handle<Object> NewRangeError(Handle<String> message);

  Handle<Object> NewSyntaxError(const char* type, Handle<JSArray> args);
  Handle<Object> NewSyntaxError(Handle<String> message);

  Handle<Object> NewReferenceError(const char* type,
                                   Vector< Handle<Object> > args);
  Handle<Object> NewReferenceError(Handle<String> message);

  Handle<Object> NewEvalError(const char* type,
                              Vector< Handle<Object> > args);


  Handle<JSFunction> NewFunction(Handle<String> name,
                                 InstanceType type,
                                 int instance_size,
                                 Handle<Code> code,
                                 bool force_initial_map);

  Handle<JSFunction> NewFunction(Handle<Map> function_map,
      Handle<SharedFunctionInfo> shared, Handle<Object> prototype);


  Handle<JSFunction> NewFunctionWithPrototype(Handle<String> name,
                                              InstanceType type,
                                              int instance_size,
                                              Handle<JSObject> prototype,
                                              Handle<Code> code,
                                              bool force_initial_map);

  Handle<JSFunction> NewFunctionWithoutPrototype(Handle<String> name,
                                                 Handle<Code> code);

  Handle<DescriptorArray> CopyAppendForeignDescriptor(
      Handle<DescriptorArray> array,
      Handle<String> key,
      Handle<Object> value,
      PropertyAttributes attributes);

  Handle<String> NumberToString(Handle<Object> number);

  enum ApiInstanceType {
    JavaScriptObject,
    InnerGlobalObject,
    OuterGlobalObject
  };

  Handle<JSFunction> CreateApiFunction(
      Handle<FunctionTemplateInfo> data,
      ApiInstanceType type = JavaScriptObject);

  Handle<JSFunction> InstallMembers(Handle<JSFunction> function);

  // Installs interceptors on the instance.  'desc' is a function template,
  // and instance is an object instance created by the function of this
  // function template.
  void ConfigureInstance(Handle<FunctionTemplateInfo> desc,
                         Handle<JSObject> instance,
                         bool* pending_exception);

#define ROOT_ACCESSOR(type, name, camel_name)                                  \
  inline Handle<type> name() {                                                 \
    return Handle<type>(BitCast<type**>(                                       \
        &isolate()->heap()->roots_[Heap::k##camel_name##RootIndex]));          \
  }
  ROOT_LIST(ROOT_ACCESSOR)
#undef ROOT_ACCESSOR_ACCESSOR

#define SYMBOL_ACCESSOR(name, str)                                             \
  inline Handle<String> name() {                                               \
    return Handle<String>(BitCast<String**>(                                   \
        &isolate()->heap()->roots_[Heap::k##name##RootIndex]));                \
  }
  SYMBOL_LIST(SYMBOL_ACCESSOR)
#undef SYMBOL_ACCESSOR

  Handle<String> hidden_symbol() {
    return Handle<String>(&isolate()->heap()->hidden_symbol_);
  }

  Handle<SharedFunctionInfo> NewSharedFunctionInfo(
      Handle<String> name,
      int number_of_literals,
      Handle<Code> code,
      Handle<SerializedScopeInfo> scope_info);
  Handle<SharedFunctionInfo> NewSharedFunctionInfo(Handle<String> name);

  Handle<JSMessageObject> NewJSMessageObject(
      Handle<String> type,
      Handle<JSArray> arguments,
      int start_position,
      int end_position,
      Handle<Object> script,
      Handle<Object> stack_trace,
      Handle<Object> stack_frames);

  Handle<NumberDictionary> DictionaryAtNumberPut(
      Handle<NumberDictionary>,
      uint32_t key,
      Handle<Object> value);

#ifdef ENABLE_DEBUGGER_SUPPORT
  Handle<DebugInfo> NewDebugInfo(Handle<SharedFunctionInfo> shared);
#endif

  // Return a map using the map cache in the global context.
  // The key the an ordered set of property names.
  Handle<Map> ObjectLiteralMapFromCache(Handle<Context> context,
                                        Handle<FixedArray> keys);

  // Creates a new FixedArray that holds the data associated with the
  // atom regexp and stores it in the regexp.
  void SetRegExpAtomData(Handle<JSRegExp> regexp,
                         JSRegExp::Type type,
                         Handle<String> source,
                         JSRegExp::Flags flags,
                         Handle<Object> match_pattern);

  // Creates a new FixedArray that holds the data associated with the
  // irregexp regexp and stores it in the regexp.
  void SetRegExpIrregexpData(Handle<JSRegExp> regexp,
                             JSRegExp::Type type,
                             Handle<String> source,
                             JSRegExp::Flags flags,
                             int capture_count);

 private:
  Isolate* isolate() { return reinterpret_cast<Isolate*>(this); }

  Handle<JSFunction> NewFunctionHelper(Handle<String> name,
                                       Handle<Object> prototype);

  Handle<JSFunction> NewFunctionWithoutPrototypeHelper(
      Handle<String> name,
      StrictModeFlag strict_mode);

  Handle<DescriptorArray> CopyAppendCallbackDescriptors(
      Handle<DescriptorArray> array,
      Handle<Object> descriptors);

  // Create a new map cache.
  Handle<MapCache> NewMapCache(int at_least_space_for);

  // Update the map cache in the global context with (keys, map)
  Handle<MapCache> AddToMapCache(Handle<Context> context,
                                 Handle<FixedArray> keys,
                                 Handle<Map> map);
};


} }  // namespace v8::internal

#endif  // V8_FACTORY_H_
