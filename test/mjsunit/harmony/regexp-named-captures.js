// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-regexp-named-captures --harmony-regexp-lookbehind

// Malformed named captures.
assertThrows("/(?<>a)/u", SyntaxError);  // Empty name.
assertThrows("/(?<aa)/u", SyntaxError);  // Unterminated name.
assertThrows("/(?<42a>a)/u", SyntaxError);  // Name starting with digits.
assertThrows("/(?<:a>a)/u", SyntaxError);  // Name starting with invalid char.
assertThrows("/(?<a:>a)/u", SyntaxError);  // Name containing with invalid char.
assertThrows("/(?<a>a)(?<a>a)/u", SyntaxError);  // Duplicate name.
assertThrows("/(?<a>a)(?<b>b)(?<a>a)/u", SyntaxError);  // Duplicate name.
assertThrows("/\\k<a>/u", SyntaxError);  // Invalid reference.
assertThrows("/\\k<a/u", SyntaxError);  // Unterminated reference.
assertThrows("/\\k/u", SyntaxError);  // Lone \k.
assertThrows("/(?<a>.)\\k/u", SyntaxError);  // Lone \k.
assertThrows("/(?<a>.)\\k<a/u", SyntaxError);  // Unterminated reference.
assertThrows("/(?<a>.)\\k<b>/u", SyntaxError);  // Invalid reference.
assertThrows("/(?<a>a)\\k<ab>/u", SyntaxError);  // Invalid reference.
assertThrows("/(?<ab>a)\\k<a>/u", SyntaxError);  // Invalid reference.
assertThrows("/\\k<a>(?<ab>a)/u", SyntaxError);  // Invalid reference.
assertThrows("/(?<a>\\a)/u", SyntaxError);  // Identity escape in capture.

// Behavior in non-unicode mode.
assertThrows("/(?<>a)/", SyntaxError);
assertThrows("/(?<aa)/", SyntaxError);
assertThrows("/(?<42a>a)/", SyntaxError);
assertThrows("/(?<:a>a)/", SyntaxError);
assertThrows("/(?<a:>a)/", SyntaxError);
assertThrows("/(?<a>a)(?<a>a)/", SyntaxError);
assertThrows("/(?<a>a)(?<b>b)(?<a>a)/", SyntaxError);
assertTrue(/\k<a>/.test("k<a>"));
assertTrue(/\k<4>/.test("k<4>"));
assertTrue(/\k<a/.test("k<a"));
assertTrue(/\k/.test("k"));
assertThrows("/(?<a>.)\\k/", SyntaxError);
assertThrows("/(?<a>.)\\k<a/", SyntaxError);
assertThrows("/(?<a>.)\\k<b>/", SyntaxError);
assertThrows("/(?<a>a)\\k<ab>/", SyntaxError);
assertThrows("/(?<ab>a)\\k<a>/", SyntaxError);
assertThrows("/\\k<a>(?<ab>a)/", SyntaxError);
assertThrows("/\\k<a(?<a>a)/", SyntaxError);
assertTrue(/(?<a>\a)/.test("a"));

assertEquals(["k<a>"], "xxxk<a>xxx".match(/\k<a>/));
assertEquals(["k<a"], "xxxk<a>xxx".match(/\k<a/));

assertEquals({a: "a", b: "b", c: "c"},
             /(?<a>.)(?<b>.)(?<c>.)\k<c>\k<b>\k<a>/.exec("abccba").groups);

// A couple of corner cases around '\k' as named back-references vs. identity
// escapes.
assertTrue(/\k<a>(?<=>)a/.test("k<a>a"));
assertTrue(/\k<a>(?<!a)a/.test("k<a>a"));
assertTrue(/\k<a>(<a>x)/.test("k<a><a>x"));
assertTrue(/\k<a>(?<a>x)/.test("x"));
assertThrows("/\\k<a>(?<b>x)/", SyntaxError);
assertThrows("/\\k<a(?<a>.)/", SyntaxError);
assertThrows("/\\k(?<a>.)/", SyntaxError);

// Basic named groups.
assertEquals(["a", "a"], "bab".match(/(?<a>a)/u));
assertEquals(["a", "a"], "bab".match(/(?<a42>a)/u));
assertEquals(["a", "a"], "bab".match(/(?<_>a)/u));
assertEquals(["a", "a"], "bab".match(/(?<$>a)/u));
assertEquals(["bab", "a"], "bab".match(/.(?<$>a)./u));
assertEquals(["bab", "a", "b"], "bab".match(/.(?<a>a)(.)/u));
assertEquals(["bab", "a", "b"], "bab".match(/.(?<a>a)(?<b>.)/u));
assertEquals(["bab", "ab"], "bab".match(/.(?<a>\w\w)/u));
assertEquals(["bab", "bab"], "bab".match(/(?<a>\w\w\w)/u));
assertEquals(["bab", "ba", "b"], "bab".match(/(?<a>\w\w)(?<b>\w)/u));

assertEquals(["a", "a"], "bab".match(/(?<a>a)/));
assertEquals(["a", "a"], "bab".match(/(?<a42>a)/));
assertEquals(["a", "a"], "bab".match(/(?<_>a)/));
assertEquals(["a", "a"], "bab".match(/(?<$>a)/));
assertEquals(["bab", "a"], "bab".match(/.(?<$>a)./));
assertEquals(["bab", "a", "b"], "bab".match(/.(?<a>a)(.)/));
assertEquals(["bab", "a", "b"], "bab".match(/.(?<a>a)(?<b>.)/));
assertEquals(["bab", "ab"], "bab".match(/.(?<a>\w\w)/));
assertEquals(["bab", "bab"], "bab".match(/(?<a>\w\w\w)/));
assertEquals(["bab", "ba", "b"], "bab".match(/(?<a>\w\w)(?<b>\w)/));

assertEquals("bab".match(/(a)/u), "bab".match(/(?<a>a)/u));
assertEquals("bab".match(/(a)/u), "bab".match(/(?<a42>a)/u));
assertEquals("bab".match(/(a)/u), "bab".match(/(?<_>a)/u));
assertEquals("bab".match(/(a)/u), "bab".match(/(?<$>a)/u));
assertEquals("bab".match(/.(a)./u), "bab".match(/.(?<$>a)./u));
assertEquals("bab".match(/.(a)(.)/u), "bab".match(/.(?<a>a)(.)/u));
assertEquals("bab".match(/.(a)(.)/u), "bab".match(/.(?<a>a)(?<b>.)/u));
assertEquals("bab".match(/.(\w\w)/u), "bab".match(/.(?<a>\w\w)/u));
assertEquals("bab".match(/(\w\w\w)/u), "bab".match(/(?<a>\w\w\w)/u));
assertEquals("bab".match(/(\w\w)(\w)/u), "bab".match(/(?<a>\w\w)(?<b>\w)/u));

assertEquals(["bab", "b"], "bab".match(/(?<b>b).\1/u));
assertEquals(["baba", "b", "a"], "baba".match(/(.)(?<a>a)\1\2/u));
assertEquals(["baba", "b", "a", "b", "a"],
    "baba".match(/(.)(?<a>a)(?<b>\1)(\2)/u));
assertEquals(["<a", "<"], "<a".match(/(?<lt><)a/u));
assertEquals([">a", ">"], ">a".match(/(?<gt>>)a/u));

// Named references.
assertEquals(["bab", "b"], "bab".match(/(?<b>.).\k<b>/u));
assertNull("baa".match(/(?<b>.).\k<b>/u));

// Nested groups.
assertEquals(["bab", "bab", "ab", "b"], "bab".match(/(?<a>.(?<b>.(?<c>.)))/u));
assertEquals({a: "bab", b: "ab", c: "b"},
             "bab".match(/(?<a>.(?<b>.(?<c>.)))/u).groups);

// Reference inside group.
assertEquals(["bab", "b"], "bab".match(/(?<a>\k<a>\w)../u));
assertEquals({a: "b"}, "bab".match(/(?<a>\k<a>\w)../u).groups);

// Reference before group.
assertEquals(["bab", "b"], "bab".match(/\k<a>(?<a>b)\w\k<a>/u));
assertEquals({a: "b"}, "bab".match(/\k<a>(?<a>b)\w\k<a>/u).groups);
assertEquals(["bab", "b", "a"], "bab".match(/(?<b>b)\k<a>(?<a>a)\k<b>/u));
assertEquals({a: "a", b: "b"},
             "bab".match(/(?<b>b)\k<a>(?<a>a)\k<b>/u).groups);

assertEquals(["bab", "b"], "bab".match(/\k<a>(?<a>b)\w\k<a>/));
assertEquals(["bab", "b", "a"], "bab".match(/(?<b>b)\k<a>(?<a>a)\k<b>/));

// Reference properties.
assertEquals("a", /(?<a>a)(?<b>b)\k<a>/u.exec("aba").groups.a);
assertEquals("b", /(?<a>a)(?<b>b)\k<a>/u.exec("aba").groups.b);
assertEquals(undefined, /(?<a>a)(?<b>b)\k<a>/u.exec("aba").groups.c);
assertEquals(undefined, /(?<a>a)(?<b>b)\k<a>|(?<c>c)/u.exec("aba").groups.c);

// Unicode names.
assertEquals("a", /(?<π>a)/u.exec("bab").groups.π);
assertEquals("a", /(?<\u{03C0}>a)/u.exec("bab").groups.π);
assertEquals("a", /(?<π>a)/u.exec("bab").groups.\u03C0);
assertEquals("a", /(?<\u{03C0}>a)/u.exec("bab").groups.\u03C0);
assertEquals("a", /(?<$>a)/u.exec("bab").groups.$);
assertEquals("a", /(?<_>a)/u.exec("bab").groups._);
assertEquals("a", /(?<$𐒤>a)/u.exec("bab").groups.$𐒤);
assertEquals("a", /(?<_\u200C>a)/u.exec("bab").groups._\u200C);
assertEquals("a", /(?<_\u200D>a)/u.exec("bab").groups._\u200D);
assertEquals("a", /(?<ಠ_ಠ>a)/u.exec("bab").groups.ಠ_ಠ);
assertThrows('/(?<❤>a)/u', SyntaxError);
assertThrows('/(?<𐒤>a)/u', SyntaxError);  // ID_Continue but not ID_Start.

assertEquals("a", /(?<π>a)/.exec("bab").groups.π);
assertEquals("a", /(?<$>a)/.exec("bab").groups.$);
assertEquals("a", /(?<_>a)/.exec("bab").groups._);
assertEquals("a", /(?<$𐒤>a)/.exec("bab").groups.$𐒤);
assertEquals("a", /(?<ಠ_ಠ>a)/.exec("bab").groups.ಠ_ಠ);
assertThrows('/(?<❤>a)/', SyntaxError);
assertThrows('/(?<𐒤>a)/', SyntaxError);  // ID_Continue but not ID_Start.

// Interaction with lookbehind assertions.
assertEquals(["f", "c"], "abcdef".match(/(?<=(?<a>\w){3})f/u));
assertEquals({a: "c"}, "abcdef".match(/(?<=(?<a>\w){3})f/u).groups);
assertEquals({a: "b"}, "abcdef".match(/(?<=(?<a>\w){4})f/u).groups);
assertEquals({a: "a"}, "abcdef".match(/(?<=(?<a>\w)+)f/u).groups);
assertNull("abcdef".match(/(?<=(?<a>\w){6})f/u));

assertEquals(["f", ""], "abcdef".match(/((?<=\w{3}))f/u));
assertEquals(["f", ""], "abcdef".match(/(?<a>(?<=\w{3}))f/u));

assertEquals(["f", undefined], "abcdef".match(/(?<!(?<a>\d){3})f/u));
assertNull("abcdef".match(/(?<!(?<a>\D){3})f/u));

assertEquals(["f", undefined], "abcdef".match(/(?<!(?<a>\D){3})f|f/u));
assertEquals(["f", undefined], "abcdef".match(/(?<a>(?<!\D{3}))f|f/u));

// Properties created on result.groups.
assertEquals(["fst", "snd"],
             Object.getOwnPropertyNames(
                 /(?<fst>.)|(?<snd>.)/u.exec("abcd").groups));

// The '__proto__' property on the groups object.
assertEquals(undefined, /(?<a>.)/u.exec("a").groups.__proto__);
assertEquals("a", /(?<__proto__>a)/u.exec("a").groups.__proto__);

// Backslash as ID_Start and ID_Continue (v8:5868).
assertThrows("/(?<\\>.)/", SyntaxError);   // '\' misclassified as ID_Start.
assertThrows("/(?<a\\>.)/", SyntaxError);  // '\' misclassified as ID_Continue.

// Backreference before the group (exercises the capture mini-parser).
assertThrows("/\\1(?:.)/u", SyntaxError);
assertThrows("/\\1(?<=a)./u", SyntaxError);
assertThrows("/\\1(?<!a)./u", SyntaxError);
assertEquals(["a", "a"], /\1(?<a>.)/u.exec("abcd"));

// @@replace with a callable replacement argument (no named captures).
{
  let result = "abcd".replace(/(.)(.)/u, (match, fst, snd, offset, str) => {
    assertEquals("ab", match);
    assertEquals("a", fst);
    assertEquals("b", snd);
    assertEquals(0, offset);
    assertEquals("abcd", str);
    return `${snd}${fst}`;
  });
  assertEquals("bacd", result);

  assertEquals("undefinedbcd", "abcd".replace(/(.)|(.)/u,
      (match, fst, snd, offset, str) => snd));
}

// @@replace with a callable replacement argument (global, named captures).
{
  let i = 0;
  let result = "abcd".replace(/(?<fst>.)(?<snd>.)/gu,
      (match, fst, snd, offset, str, groups) => {
    if (i == 0) {
      assertEquals("ab", match);
      assertEquals("a", groups.fst);
      assertEquals("b", groups.snd);
      assertEquals("a", fst);
      assertEquals("b", snd);
      assertEquals(0, offset);
      assertEquals("abcd", str);
    } else if (i == 1) {
      assertEquals("cd", match);
      assertEquals("c", groups.fst);
      assertEquals("d", groups.snd);
      assertEquals("c", fst);
      assertEquals("d", snd);
      assertEquals(2, offset);
      assertEquals("abcd", str);
    } else {
      assertUnreachable();
    }
    i++;
    return `${groups.snd}${groups.fst}`;
  });
  assertEquals("badc", result);

  assertEquals("undefinedundefinedundefinedundefined",
      "abcd".replace(/(?<fst>.)|(?<snd>.)/gu,
            (match, fst, snd, offset, str, groups) => groups.snd));
}

// @@replace with a callable replacement argument (non-global, named captures).
{
  let result = "abcd".replace(/(?<fst>.)(?<snd>.)/u,
      (match, fst, snd, offset, str, groups) => {
    assertEquals("ab", match);
    assertEquals("a", groups.fst);
    assertEquals("b", groups.snd);
    assertEquals("a", fst);
    assertEquals("b", snd);
    assertEquals(0, offset);
    assertEquals("abcd", str);
    return `${groups.snd}${groups.fst}`;
  });
  assertEquals("bacd", result);

  assertEquals("undefinedbcd",
      "abcd".replace(/(?<fst>.)|(?<snd>.)/u,
            (match, fst, snd, offset, str, groups) => groups.snd));
}

function toSlowMode(re) {
  re.exec = (str) => RegExp.prototype.exec.call(re, str);
  return re;
}

// @@replace with a callable replacement argument (slow, global,
// named captures).
{
  let i = 0;
  let re = toSlowMode(/(?<fst>.)(?<snd>.)/gu);
  let result = "abcd".replace(re, (match, fst, snd, offset, str, groups) => {
    if (i == 0) {
      assertEquals("ab", match);
      assertEquals("a", groups.fst);
      assertEquals("b", groups.snd);
      assertEquals("a", fst);
      assertEquals("b", snd);
      assertEquals(0, offset);
      assertEquals("abcd", str);
    } else if (i == 1) {
      assertEquals("cd", match);
      assertEquals("c", groups.fst);
      assertEquals("d", groups.snd);
      assertEquals("c", fst);
      assertEquals("d", snd);
      assertEquals(2, offset);
      assertEquals("abcd", str);
    } else {
      assertUnreachable();
    }
    i++;
    return `${groups.snd}${groups.fst}`;
  });
  assertEquals("badc", result);

  assertEquals("undefinedundefinedundefinedundefined",
      "abcd".replace(toSlowMode(/(?<fst>.)|(?<snd>.)/gu),
            (match, fst, snd, offset, str, groups) => groups.snd));
}

// @@replace with a callable replacement argument (slow, non-global,
// named captures).
{
  let re = toSlowMode(/(?<fst>.)(?<snd>.)/u);
  let result = "abcd".replace(re, (match, fst, snd, offset, str, groups) => {
    assertEquals("ab", match);
    assertEquals("a", groups.fst);
    assertEquals("b", groups.snd);
    assertEquals("a", fst);
    assertEquals("b", snd);
    assertEquals(0, offset);
    assertEquals("abcd", str);
    return `${groups.snd}${groups.fst}`;
  });
  assertEquals("bacd", result);

  assertEquals("undefinedbcd",
      "abcd".replace(toSlowMode(/(?<fst>.)|(?<snd>.)/u),
            (match, fst, snd, offset, str, groups) => groups.snd));
}

// @@replace with a string replacement argument (no named captures).
{
  let re = /(.)(.)|(x)/u;
  assertEquals("$<snd>$<fst>cd", "abcd".replace(re, "$<snd>$<fst>"));
  assertEquals("bacd", "abcd".replace(re, "$2$1"));
  assertEquals("cd", "abcd".replace(re, "$3"));
  assertEquals("$<sndcd", "abcd".replace(re, "$<snd"));
  assertEquals("$<42a>cd", "abcd".replace(re, "$<42$1>"));
  assertEquals("$<fth>cd", "abcd".replace(re, "$<fth>"));
  assertEquals("$<a>cd", "abcd".replace(re, "$<$1>"));
}

// @@replace with a string replacement argument (global, named captures).
{
  let re = /(?<fst>.)(?<snd>.)|(?<thd>x)/gu;
  assertEquals("badc", "abcd".replace(re, "$<snd>$<fst>"));
  assertEquals("badc", "abcd".replace(re, "$2$1"));
  assertEquals("", "abcd".replace(re, "$<thd>"));
  assertThrows(() => "abcd".replace(re, "$<snd"), SyntaxError);
  assertThrows(() => "abcd".replace(re, "$<42$1>"), SyntaxError);
  assertThrows(() => "abcd".replace(re, "$<fth>"), SyntaxError);
  assertThrows(() => "abcd".replace(re, "$<$1>"), SyntaxError);
}

// @@replace with a string replacement argument (non-global, named captures).
{
  let re = /(?<fst>.)(?<snd>.)|(?<thd>x)/u;
  assertEquals("bacd", "abcd".replace(re, "$<snd>$<fst>"));
  assertEquals("bacd", "abcd".replace(re, "$2$1"));
  assertEquals("cd", "abcd".replace(re, "$<thd>"));
  assertThrows(() => "abcd".replace(re, "$<snd"), SyntaxError);
  assertThrows(() => "abcd".replace(re, "$<42$1>"), SyntaxError);
  assertThrows(() => "abcd".replace(re, "$<fth>"), SyntaxError);
  assertThrows(() => "abcd".replace(re, "$<$1>"), SyntaxError);
}

// @@replace with a string replacement argument (slow, global, named captures).
{
  let re = toSlowMode(/(?<fst>.)(?<snd>.)|(?<thd>x)/gu);
  assertEquals("badc", "abcd".replace(re, "$<snd>$<fst>"));
  assertEquals("badc", "abcd".replace(re, "$2$1"));
  assertEquals("", "abcd".replace(re, "$<thd>"));
  assertThrows(() => "abcd".replace(re, "$<snd"), SyntaxError);
  assertThrows(() => "abcd".replace(re, "$<42$1>"), SyntaxError);
  assertThrows(() => "abcd".replace(re, "$<fth>"), SyntaxError);
  assertThrows(() => "abcd".replace(re, "$<$1>"), SyntaxError);
}

// @@replace with a string replacement argument (slow, non-global,
// named captures).
{
  let re = toSlowMode(/(?<fst>.)(?<snd>.)|(?<thd>x)/u);
  assertEquals("bacd", "abcd".replace(re, "$<snd>$<fst>"));
  assertEquals("bacd", "abcd".replace(re, "$2$1"));
  assertEquals("cd", "abcd".replace(re, "$<thd>"));
  assertThrows(() => "abcd".replace(re, "$<snd"), SyntaxError);
  assertThrows(() => "abcd".replace(re, "$<42$1>"), SyntaxError);
  assertThrows(() => "abcd".replace(re, "$<fth>"), SyntaxError);
  assertThrows(() => "abcd".replace(re, "$<$1>"), SyntaxError);
}
