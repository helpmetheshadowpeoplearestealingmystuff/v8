// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-templates

var num = 5;
var str = "str";
function fn() { return "result"; }
var obj = {
  num: num,
  str: str,
  fn: function() { return "result"; }
};

(function testBasicExpressions() {
  assertEquals("foo 5 bar", `foo ${num} bar`);
  assertEquals("foo str bar", `foo ${str} bar`);
  assertEquals("foo [object Object] bar", `foo ${obj} bar`);
  assertEquals("foo result bar", `foo ${fn()} bar`);
  assertEquals("foo 5 bar", `foo ${obj.num} bar`);
  assertEquals("foo str bar", `foo ${obj.str} bar`);
  assertEquals("foo result bar", `foo ${obj.fn()} bar`);
})();

(function testExpressionsContainingTemplates() {
  assertEquals("foo bar 5", `foo ${`bar ${num}`}`);
})();

(function testMultilineTemplates() {
  assertEquals("foo\n    bar\n    baz", `foo
    bar
    baz`);

  assertEquals("foo\n  bar\n  baz", eval("`foo\r\n  bar\r  baz`"));
})();

(function testLineContinuation() {
  assertEquals("\n", `\

`);
})();

(function testTaggedTemplates() {
  var calls = 0;
  (function(s) {
    calls++;
  })`test`;
  assertEquals(1, calls);

  calls = 0;
  // assert tag is invoked in right context
  obj = {
    fn: function() {
      calls++;
      assertEquals(obj, this);
    }
  };

  obj.fn`test`;
  assertEquals(1, calls);

  calls = 0;
  // Simple templates only have a callSiteObj
  (function(s) {
    calls++;
    assertEquals(1, arguments.length);
  })`test`;
  assertEquals(1, calls);

  // Templates containing expressions have the values of evaluated expressions
  calls = 0;
  (function(site, n, s, o, f, r) {
    calls++;
    assertEquals(6, arguments.length);
    assertEquals("number", typeof n);
    assertEquals("string", typeof s);
    assertEquals("object", typeof o);
    assertEquals("function", typeof f);
    assertEquals("result", r);
  })`${num}${str}${obj}${fn}${fn()}`;
  assertEquals(1, calls);

  // The TV and TRV of NoSubstitutionTemplate :: `` is the empty code unit
  // sequence.
  calls = 0;
  (function(s) {
    calls++;
    assertEquals(1, s.length);
    assertEquals(1, s.raw.length);
    assertEquals("", s[0]);

    // Failure: expected <""> found <"foo  barfoo  barfoo foo foo foo testtest">
    assertEquals("", s.raw[0]);
  })``;
  assertEquals(1, calls);

  // The TV and TRV of TemplateHead :: `${ is the empty code unit sequence.
  calls = 0;
  (function(s) {
    calls++;
    assertEquals(2, s.length);
    assertEquals(2, s.raw.length);
    assertEquals("", s[0]);
    assertEquals("", s.raw[0]);
  })`${1}`;
  assertEquals(1, calls);

  // The TV and TRV of TemplateMiddle :: }${ is the empty code unit sequence.
  calls = 0;
  (function(s) {
    calls++;
    assertEquals(3, s.length);
    assertEquals(3, s.raw.length);
    assertEquals("", s[1]);
    assertEquals("", s.raw[1]);
  })`${1}${2}`;
  assertEquals(1, calls);

  // The TV and TRV of TemplateTail :: }` is the empty code unit sequence.
  calls = 0;
  (function(s) {
    calls++;
    assertEquals(2, s.length);
    assertEquals(2, s.raw.length);
    assertEquals("", s[1]);
    assertEquals("", s.raw[1]);
  })`${1}`;
  assertEquals(1, calls);

  // The TV of NoSubstitutionTemplate :: ` TemplateCharacters ` is the TV of
  // TemplateCharacters.
  calls = 0;
  (function(s) { calls++; assertEquals("foo", s[0]); })`foo`;
  assertEquals(1, calls);

  // The TV of TemplateHead :: ` TemplateCharacters ${ is the TV of
  // TemplateCharacters.
  calls = 0;
  (function(s) { calls++; assertEquals("foo", s[0]); })`foo${1}`;
  assertEquals(1, calls);

  // The TV of TemplateMiddle :: } TemplateCharacters ${ is the TV of
  // TemplateCharacters.
  calls = 0;
  (function(s) { calls++; assertEquals("foo", s[1]); })`${1}foo${2}`;
  assertEquals(1, calls);

  // The TV of TemplateTail :: } TemplateCharacters ` is the TV of
  // TemplateCharacters.
  calls = 0;
  (function(s) { calls++; assertEquals("foo", s[1]); })`${1}foo`;
  assertEquals(1, calls);

  // The TV of TemplateCharacters :: TemplateCharacter is the TV of
  // TemplateCharacter.
  calls = 0;
  (function(s) { calls++; assertEquals("f", s[0]); })`f`;
  assertEquals(1, calls);

  // The TV of TemplateCharacter :: $ is the code unit value 0x0024.
  calls = 0;
  (function(s) { calls++; assertEquals("$", s[0]); })`$`;
  assertEquals(1, calls);

  // The TV of TemplateCharacter :: \ EscapeSequence is the CV of
  // EscapeSequence.
  calls = 0;
  (function(s) { calls++; assertEquals("안녕", s[0]); })`\uc548\uB155`;
  (function(s) { calls++; assertEquals("\xff", s[0]); })`\xff`;
  (function(s) { calls++; assertEquals("\n", s[0]); })`\n`;
  assertEquals(3, calls);

  // The TV of TemplateCharacter :: LineContinuation is the TV of
  // LineContinuation. The TV of LineContinuation :: \ LineTerminatorSequence is
  // the empty code unit sequence.
  calls = 0;
  (function(s) { calls++; assertEquals("", s[0]); })`\
`;
  assertEquals(1, calls);

  // The TRV of NoSubstitutionTemplate :: ` TemplateCharacters ` is the TRV of
  // TemplateCharacters.
  calls = 0;
  (function(s) { calls++; assertEquals("test", s.raw[0]); })`test`;
  assertEquals(1, calls);

  // The TRV of TemplateHead :: ` TemplateCharacters ${ is the TRV of
  // TemplateCharacters.
  calls = 0;
  (function(s) { calls++; assertEquals("test", s.raw[0]); })`test${1}`;
  assertEquals(1, calls);

  // The TRV of TemplateMiddle :: } TemplateCharacters ${ is the TRV of
  // TemplateCharacters.
  calls = 0;
  (function(s) { calls++; assertEquals("test", s.raw[1]); })`${1}test${2}`;
  assertEquals(1, calls);

  // The TRV of TemplateTail :: } TemplateCharacters ` is the TRV of
  // TemplateCharacters.
  calls = 0;
  (function(s) { calls++; assertEquals("test", s.raw[1]); })`${1}test`;
  assertEquals(1, calls);

  // The TRV of TemplateCharacters :: TemplateCharacter is the TRV of
  // TemplateCharacter.
  calls = 0;
  (function(s) { calls++; assertEquals("f", s.raw[0]); })`f`;
  assertEquals(1, calls);

  // The TRV of TemplateCharacter :: $ is the code unit value 0x0024.
  calls = 0;
  (function(s) { calls++; assertEquals("\u0024", s.raw[0]); })`$`;
  assertEquals(1, calls);

  // The TRV of EscapeSequence :: 0 is the code unit value 0x0030.
  calls = 0;
  (function(s) { calls++; assertEquals("\u005C\u0030", s.raw[0]); })`\0`;
  assertEquals(1, calls);

  // The TRV of TemplateCharacter :: \ EscapeSequence is the sequence consisting
  // of the code unit value 0x005C followed by the code units of TRV of
  // EscapeSequence.

  //   The TRV of EscapeSequence :: HexEscapeSequence is the TRV of the
  //   HexEscapeSequence.
  calls = 0;
  (function(s) { calls++; assertEquals("\u005Cxff", s.raw[0]); })`\xff`;
  assertEquals(1, calls);

  //   The TRV of EscapeSequence :: UnicodeEscapeSequence is the TRV of the
  //   UnicodeEscapeSequence.
  calls = 0;
  (function(s) { calls++; assertEquals("\u005Cuc548", s.raw[0]); })`\uc548`;
  assertEquals(1, calls);

  //   The TRV of CharacterEscapeSequence :: SingleEscapeCharacter is the TRV of
  //   the SingleEscapeCharacter.
  calls = 0;
  (function(s) { calls++; assertEquals("\u005C\u0027", s.raw[0]); })`\'`;
  (function(s) { calls++; assertEquals("\u005C\u0022", s.raw[0]); })`\"`;
  (function(s) { calls++; assertEquals("\u005C\u005C", s.raw[0]); })`\\`;
  (function(s) { calls++; assertEquals("\u005Cb", s.raw[0]); })`\b`;
  (function(s) { calls++; assertEquals("\u005Cf", s.raw[0]); })`\f`;
  (function(s) { calls++; assertEquals("\u005Cn", s.raw[0]); })`\n`;
  (function(s) { calls++; assertEquals("\u005Cr", s.raw[0]); })`\r`;
  (function(s) { calls++; assertEquals("\u005Ct", s.raw[0]); })`\t`;
  (function(s) { calls++; assertEquals("\u005Cv", s.raw[0]); })`\v`;
  (function(s) { calls++; assertEquals("\u005C`", s.raw[0]); })`\``;
  assertEquals(10, calls);

  //   The TRV of CharacterEscapeSequence :: NonEscapeCharacter is the CV of the
  //   NonEscapeCharacter.
  calls = 0;
  (function(s) { calls++; assertEquals("\u005Cx", s.raw[0]); })`\x`;
  assertEquals(1, calls);

  // The TRV of LineTerminatorSequence :: <LF> is the code unit value 0x000A.
  // The TRV of LineTerminatorSequence :: <CR> is the code unit value 0x000A.
  // The TRV of LineTerminatorSequence :: <CR><LF> is the sequence consisting of
  // the code unit value 0x000A.
  calls = 0;
  function testRawLineNormalization(cs) {
    calls++;
    assertEquals(cs.raw[0], "\n\n\n");
    assertEquals(cs.raw[1], "\n\n\n");
  }
  eval("testRawLineNormalization`\r\n\n\r${1}\r\n\n\r`");
  assertEquals(1, calls);

  // The TRV of LineContinuation :: \ LineTerminatorSequence is the sequence
  // consisting of the code unit value 0x005C followed by the code units of TRV
  // of LineTerminatorSequence.
  calls = 0;
  function testRawLineContinuation(cs) {
    calls++;
    assertEquals(cs.raw[0], "\u005C\n\u005C\n\u005C\n");
    assertEquals(cs.raw[1], "\u005C\n\u005C\n\u005C\n");
  }
  eval("testRawLineContinuation`\\\r\n\\\n\\\r${1}\\\r\n\\\n\\\r`");
  assertEquals(1, calls);
})();


(function testCallSiteObj() {
  var calls = 0;
  function tag(cs) {
    calls++;
    assertTrue(cs.hasOwnProperty("raw"));
    assertTrue(Object.isFrozen(cs));
    assertTrue(Object.isFrozen(cs.raw));
    var raw = Object.getOwnPropertyDescriptor(cs, "raw");
    assertFalse(raw.writable);
    assertFalse(raw.configurable);
    assertFalse(raw.enumerable);
    assertEquals(Array.prototype, Object.getPrototypeOf(cs.raw));
    assertTrue(Array.isArray(cs.raw));
    assertEquals(Array.prototype, Object.getPrototypeOf(cs));
    assertTrue(Array.isArray(cs));

    var cooked0 = Object.getOwnPropertyDescriptor(cs, "0");
    assertFalse(cooked0.writable);
    assertFalse(cooked0.configurable);
    assertTrue(cooked0.enumerable);

    var raw0 = Object.getOwnPropertyDescriptor(cs.raw, "0");
    assertFalse(cooked0.writable);
    assertFalse(cooked0.configurable);
    assertTrue(cooked0.enumerable);

    var length = Object.getOwnPropertyDescriptor(cs, "length");
    assertFalse(length.writable);
    assertFalse(length.configurable);
    assertFalse(length.enumerable);

    length = Object.getOwnPropertyDescriptor(cs.raw, "length");
    assertFalse(length.writable);
    assertFalse(length.configurable);
    assertFalse(length.enumerable);
  }
  tag`${1}`;
  assertEquals(1, calls);
})();


(function testUTF16ByteOrderMark() {
  assertEquals("\uFEFFtest", `\uFEFFtest`);
  assertEquals("\uFEFFtest", eval("`\uFEFFtest`"));
})();


(function testStringRawAsTagFn() {
  assertEquals("\\u0065\\`\\r\\r\\n\\ntestcheck",
               String.raw`\u0065\`\r\r\n\n${"test"}check`);
  assertEquals("\\\n\\\n\\\n", eval("String.raw`\\\r\\\r\n\\\n`"));
  assertEquals("", String.raw``);
})();


(function testExtendedArrayPrototype() {
  Object.defineProperty(Array.prototype, 0, {
    set: function() {
      assertUnreachable();
    }
  });
  function tag(){}
  tag`a${1}b`;
})();
