// Copyright 2010 the V8 project authors. All rights reserved.
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

#include "gc-extension.h"

namespace v8 {
namespace internal {

const char* const GCExtension::kSource = "native function gc();";


v8::Handle<v8::FunctionTemplate> GCExtension::GetNativeFunction(
    v8::Handle<v8::String> str) {
  return v8::FunctionTemplate::New(GCExtension::GC);
}


v8::Handle<v8::Value> GCExtension::GC(const v8::Arguments& args) {
  bool compact = false;
  // All allocation spaces other than NEW_SPACE have the same effect.
  if (args.Length() >= 1 && args[0]->IsBoolean()) {
    compact = args[0]->BooleanValue();
  }
  HEAP->CollectAllGarbage(compact);
  return v8::Undefined();
}


void GCExtension::Register() {
  static GCExtension gc_extension;
  static v8::DeclareExtension gc_extension_declaration(&gc_extension);
}

} }  // namespace v8::internal
