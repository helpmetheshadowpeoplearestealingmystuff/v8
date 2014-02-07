// Copyright 2014 the V8 project authors. All rights reserved.
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

var whitespaces = [
  // Whitespaces defined in ECMA-262 5.1, 7.2
  0x0009,  // Tab              TAB
  0x000B,  // Vertical Tab     VT
  0x000C,  // Form Feed        FF
  0x0020,  // Space            SP
  0x00A0,  // No-break space   NBSP
  0xFEFF,  // Byte Order Mark  BOM
  // Unicode whitespaces
  0x000A,  // Line Feed        LF
  0x000D,  // Carriage Return  CR
  0x0085,  // Next Line        NEL
  0x1680,  // Ogham Space Mark
  0x180E,  // Mongolian Vowel Separator
  0x2000,  // EN QUAD
  0x2001,  // EM QUAD
  0x2002,  // EN SPACE
  0x2003,  // EM SPACE
  0x2004,  // THREE-PER-EM SPACE
  0x2005,  // FOUR-PER-EM SPACE
  0x2006,  // SIX-PER-EM SPACE
  0x2007,  // FIGURE SPACE
  0x2008,  // PUNCTUATION SPACE
  0x2009,  // THIN SPACE
  0x200A,  // HAIR SPACE
  0x2028,  // LINE SEPARATOR
  0x2029,  // PARAGRAPH SEPARATOR
  0x202F,  // NARROW NO-BREAK SPACE
  0x205F,  // MEDIUM MATHEMATICAL SPACE
  0x3000,  // IDEOGRAPHIC SPACE
];

// Add single twobyte char to force twobyte representation.
// Interestingly, snowman is not "white" space :)
var twobyte = "\u2603";
var onebyte = "\u007E";
var twobytespace = "\u2000";
var onebytespace = "\u0020";

function is_whitespace(c) {
  return whitespaces.indexOf(c.charCodeAt(0)) > -1;
}

function test_regexp(str) {
  var pos_match = str.match(/\s/);
  var neg_match = str.match(/\S/);
  var test_char = str[0];
  var postfix = str[1];
  if (is_whitespace(test_char)) {
    assertEquals(test_char, pos_match[0]);
    assertEquals(postfix, neg_match[0]);
  } else {
    assertEquals(test_char, neg_match[0]);
    assertNull(pos_match);
  }
}

function test_trim(c, infix) {
  var str = c + c + c + infix + c;
  if (is_whitespace(c)) {
    assertEquals(infix, str.trim());
  } else {
    assertEquals(str, str.trim());
  }
}

function test_parseInt(c, postfix) {
  // Skip if prefix is a digit.
  if (c >= "0" && c <= 9) return;
  var str = c + c + "123" + postfix;
  if (is_whitespace(c)) {
    assertEquals(123, parseInt(str));
  } else {
    assertEquals(NaN, parseInt(str));
  }
}

function test_eval(c, content) {
  if (!is_whitespace(c)) return;
  var str = c + c + "'" + content + "'" + c + c;
  assertEquals(content, eval(str));
}

function test_stringtonumber(c, postfix) {
  // Skip if prefix is a digit.
  if (c >= "0" && c <= 9) return;
  var result = 1 + Number(c + "123" + c + postfix);
  if (is_whitespace(c)) {
    assertEquals(124, result);
  } else {
    assertEquals(NaN, result);
  }
}

for (var i = 0; i < 0x10000; i++) {
  c = String.fromCharCode(i);
  test_regexp(c + onebyte);
  test_regexp(c + twobyte);
  test_trim(c, onebyte + "trim");
  test_trim(c, twobyte + "trim");
  test_parseInt(c, onebyte);
  test_parseInt(c, twobyte);
  test_eval(c, onebyte);
  test_eval(c, twobyte);
  test_stringtonumber(c, onebytespace);
  test_stringtonumber(c, twobytespace);
}
