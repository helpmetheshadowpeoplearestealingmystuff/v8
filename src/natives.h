// Copyright 2006-2008 the V8 project authors. All rights reserved.
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

#ifndef V8_NATIVES_H_
#define V8_NATIVES_H_

namespace v8 { namespace internal {

typedef bool (*NativeSourceCallback)(Vector<const char> name,
                                     Vector<const char> source,
                                     int index);

enum NativeType {
  CORE, D8
};

template <NativeType type>
class NativesCollection {
 public:
  // Number of built-in scripts.
  static int GetBuiltinsCount();
  // Number of delayed/lazy loading scripts.
  static int GetDelayCount();

  // These are used to access built-in scripts.
  // The delayed script has an index in the interval [0, GetDelayCount()).
  // The non-delayed script has an index in the interval
  // [GetDelayCount(), GetNativesCount()).
  static int GetIndex(const char* name);
  static Vector<const char> GetScriptSource(int index);
  static Vector<const char> GetScriptName(int index);
};

typedef NativesCollection<CORE> Natives;

} }  // namespace v8::internal

#endif  // V8_NATIVES_H_
