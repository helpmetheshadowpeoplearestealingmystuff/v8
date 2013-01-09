// Copyright 2013 the V8 project authors. All rights reserved.
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

assertEquals(String.fromCharCode(97, 220, 256), 'a' + '\u00DC' + '\u0100');
assertEquals(String.fromCharCode(97, 220, 256), 'a\u00DC\u0100');

assertEquals(0x80, JSON.stringify("\x80").charCodeAt(1));

assertEquals(['a', 'b', '\xdc'], ['b', '\xdc', 'a'].sort());

assertEquals(['\xfc\xdc', '\xfc'], new RegExp('(\xdc)\\1', 'i').exec('\xfc\xdc'));
// Same test but for all values in Latin-1 range.
var total_lo = 0;
for (var i = 0; i < 0xff; i++) {
  var base = String.fromCharCode(i);
  var escaped = base;
  if (base == '(' || base == ')' || base == '*' || base == '+' ||
      base == '?' || base == '[' || base == ']' || base == '\\' ||
      base == '$' || base == '^' || base == '|') {
    escaped = '\\' + base;
  }
  var lo = String.fromCharCode(i + 0x20);
  base_result = new RegExp('(' + escaped + ')\\1', 'i').exec(base + base);
  assertEquals( base_result, [base + base, base]);
  lo_result = new RegExp('(' + escaped + ')\\1', 'i').exec(base + lo);
  if (base.toLowerCase() == lo) {
    assertEquals([base + lo, base], lo_result);
    total_lo++;
  } else {
    assertEquals(null, lo_result);
  }
}
// Should have hit the branch for the following char codes:
// [A-Z], [192-222] but not 215
assertEquals((90-65+1)+(222-192-1+1), total_lo);
