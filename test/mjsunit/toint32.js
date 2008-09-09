// Copyright 2008 the V8 project authors. All rights reserved.
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

function toInt32(x) {
  return x | 0;
}

assertEquals(0, toInt32(Infinity));
assertEquals(0, toInt32(-Infinity));
assertEquals(0, toInt32(NaN));
assertEquals(0, toInt32(0.0));
assertEquals(0, toInt32(-0.0));

assertEquals(0, toInt32(Number.MIN_VALUE));
assertEquals(0, toInt32(-Number.MIN_VALUE));
assertEquals(0, toInt32(0.1));
assertEquals(0, toInt32(-0.1));
assertEquals(1, toInt32(1));
assertEquals(1, toInt32(1.1));
assertEquals(-1, toInt32(-1));

assertEquals(2147483647, toInt32(2147483647));
assertEquals(-2147483648, toInt32(2147483648));
assertEquals(-2147483647, toInt32(2147483649));

assertEquals(-1, toInt32(4294967295));
assertEquals(0, toInt32(4294967296));
assertEquals(1, toInt32(4294967297));

assertEquals(-2147483647, toInt32(-2147483647));
assertEquals(-2147483648, toInt32(-2147483648));
assertEquals(2147483647, toInt32(-2147483649));

assertEquals(1, toInt32(-4294967295));
assertEquals(0, toInt32(-4294967296));
assertEquals(-1, toInt32(-4294967297));

assertEquals(-2147483648, toInt32(2147483648.25));
assertEquals(-2147483648, toInt32(2147483648.5));
assertEquals(-2147483648, toInt32(2147483648.75));
assertEquals(-1, toInt32(4294967295.25));
assertEquals(-1, toInt32(4294967295.5));
assertEquals(-1, toInt32(4294967295.75));
assertEquals(-1294967296, toInt32(3000000000.25));
assertEquals(-1294967296, toInt32(3000000000.5));
assertEquals(-1294967296, toInt32(3000000000.75));

assertEquals(-2147483648, toInt32(-2147483648.25));
assertEquals(-2147483648, toInt32(-2147483648.5));
assertEquals(-2147483648, toInt32(-2147483648.75));
assertEquals(1, toInt32(-4294967295.25));
assertEquals(1, toInt32(-4294967295.5));
assertEquals(1, toInt32(-4294967295.75));
assertEquals(1294967296, toInt32(-3000000000.25));
assertEquals(1294967296, toInt32(-3000000000.5));
assertEquals(1294967296, toInt32(-3000000000.75));
