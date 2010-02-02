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

assertEquals(0, Math.round(0));
assertEquals(-0, Math.round(-0));
assertEquals(Infinity, Math.round(Infinity));
assertEquals(-Infinity, Math.round(-Infinity));
assertNaN(Math.round(NaN));

assertEquals(1, Math.round(0.5));
assertEquals(1, Math.round(0.7));
assertEquals(1, Math.round(1));
assertEquals(1, Math.round(1.1));
assertEquals(1, Math.round(1.49999));
assertEquals(1/-0, 1/Math.round(-0.5));  // Test for -0 result.
assertEquals(-1, Math.round(-0.5000000000000001));
assertEquals(-1, Math.round(-0.7));
assertEquals(-1, Math.round(-1));
assertEquals(-1, Math.round(-1.1));
assertEquals(-1, Math.round(-1.49999));
assertEquals(-1, Math.round(-1.5));

assertEquals(9007199254740990, Math.round(9007199254740990));
assertEquals(9007199254740991, Math.round(9007199254740991));
assertEquals(-9007199254740990, Math.round(-9007199254740990));
assertEquals(-9007199254740991, Math.round(-9007199254740991));
assertEquals(Number.MAX_VALUE, Math.round(Number.MAX_VALUE));
assertEquals(-Number.MAX_VALUE, Math.round(-Number.MAX_VALUE));
