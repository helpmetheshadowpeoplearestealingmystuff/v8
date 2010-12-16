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

// Test overflow checks in optimized code.
function testMul(a, b) {
  a *= 2;
  b *= 2;
  if (a < 1 && b < 1) {
    return a * b;
  }
}

for (var i=0; i<10000000; i++) testMul(0,0);
assertEquals(4611686018427388000, testMul(-0x40000000, -0x40000000));

function testAdd(a, b) {
  a *= 2;
  b *= 2;
  if (a < 1 && b < 1) {
    return a + b;
  }
}

for (var i=0; i<10000000; i++) testAdd(0,0);
assertEquals(-4294967296, testAdd(-0x40000000, -0x40000000));


function testSub(a, b) {
  a *= 2;
  b *= 2;
  if (b == 2) {print(a); print(b);}
  if (a < 1 && b < 3) {
    return a - b;
  }
}

for (var i=0; i<10000000; i++) testSub(0,0);
assertEquals(-2147483650, testSub(-0x40000000, 1));
