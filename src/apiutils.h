// Copyright 2009 the V8 project authors. All rights reserved.
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

#ifndef V8_APIUTILS_H_
#define V8_APIUTILS_H_

namespace v8 {
class ImplementationUtilities {
 public:
  static v8::Handle<v8::Primitive> Undefined();
  static v8::Handle<v8::Primitive> Null();
  static v8::Handle<v8::Boolean> True();
  static v8::Handle<v8::Boolean> False();

  static int GetNameCount(ExtensionConfiguration* that) {
    return that->name_count_;
  }

  static const char** GetNames(ExtensionConfiguration* that) {
    return that->names_;
  }

  // Packs additional parameters for the NewArguments function. |implicit_args|
  // is a pointer to the last element of 3-elements array controlled by GC.
  static void PrepareArgumentsData(internal::Object** implicit_args,
                                   internal::Object* data,
                                   internal::JSFunction* callee,
                                   internal::Object* holder) {
    implicit_args[v8::Arguments::kDataIndex] = data;
    implicit_args[v8::Arguments::kCalleeIndex] = callee;
    implicit_args[v8::Arguments::kHolderIndex] = holder;
  }

  static v8::Arguments NewArguments(internal::Object** implicit_args,
                                    internal::Object** argv, int argc,
                                    bool is_construct_call) {
    ASSERT(implicit_args[v8::Arguments::kCalleeIndex]->IsJSFunction());
    ASSERT(implicit_args[v8::Arguments::kHolderIndex]->IsHeapObject());

    return v8::Arguments(implicit_args, argv, argc, is_construct_call);
  }

  // Introduce an alias for the handle scope data to allow non-friends
  // to access the HandleScope data.
  typedef v8::HandleScope::Data HandleScopeData;

  static HandleScopeData* CurrentHandleScope();

#ifdef DEBUG
  static void ZapHandleRange(internal::Object** begin, internal::Object** end);
#endif
};

}  // namespace v8

#endif  // V8_APIUTILS_H_
