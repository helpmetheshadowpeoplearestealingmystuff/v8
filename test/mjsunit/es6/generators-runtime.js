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

// Test aspects of the generator runtime.

// See:
// http://people.mozilla.org/~jorendorff/es6-draft.html#sec-generatorfunction-objects

function f() { "use strict"; }
function* g() { yield 1; }
var GeneratorFunctionPrototype = Object.getPrototypeOf(g);
var GeneratorFunction = GeneratorFunctionPrototype.constructor;
var GeneratorObjectPrototype = GeneratorFunctionPrototype.prototype;

// A generator function should have the same set of properties as any
// other function.
function TestGeneratorFunctionInstance() {
  var f_own_property_names = Object.getOwnPropertyNames(f);
  var g_own_property_names = Object.getOwnPropertyNames(g);

  f_own_property_names.sort();
  g_own_property_names.sort();

  assertArrayEquals(f_own_property_names, g_own_property_names);
  var i;
  for (i = 0; i < f_own_property_names.length; i++) {
    var prop = f_own_property_names[i];
    var f_desc = Object.getOwnPropertyDescriptor(f, prop);
    var g_desc = Object.getOwnPropertyDescriptor(g, prop);
    if (prop === "prototype") {
      // ES6 draft 03-17-2015 section 25.2.2.2
      assertFalse(g_desc.writable, prop);
      assertFalse(g_desc.enumerable, prop);
      assertFalse(g_desc.configurable, prop);
    } else {
      assertEquals(f_desc.configurable, g_desc.configurable, prop);
      assertEquals(f_desc.writable, g_desc.writable, prop);
      assertEquals(f_desc.enumerable, g_desc.enumerable, prop);
    }
  }
}
TestGeneratorFunctionInstance();


// Generators have an additional object interposed in the chain between
// themselves and Function.prototype.
function TestGeneratorFunctionPrototype() {
  // Sanity check.
  assertSame(Object.getPrototypeOf(f), Function.prototype);
  assertFalse(GeneratorFunctionPrototype === Function.prototype);
  assertSame(Function.prototype,
             Object.getPrototypeOf(GeneratorFunctionPrototype));
  assertSame(GeneratorFunctionPrototype,
             Object.getPrototypeOf(function* () {}));
  assertSame("object", typeof GeneratorFunctionPrototype);

  var constructor_desc = Object.getOwnPropertyDescriptor(
      GeneratorFunctionPrototype, "constructor");
  assertTrue(constructor_desc !== undefined);
  assertSame(GeneratorFunction, constructor_desc.value);
  assertFalse(constructor_desc.writable);
  assertFalse(constructor_desc.enumerable);
  assertTrue(constructor_desc.configurable);

  var prototype_desc = Object.getOwnPropertyDescriptor(
      GeneratorFunctionPrototype, "prototype");
  assertTrue(prototype_desc !== undefined);
  assertSame(GeneratorObjectPrototype, prototype_desc.value);
  assertFalse(prototype_desc.writable);
  assertFalse(prototype_desc.enumerable);
  assertTrue(prototype_desc.configurable);
}
TestGeneratorFunctionPrototype();


// Functions that we associate with generator objects are actually defined by
// a common prototype.
function TestGeneratorObjectPrototype() {
  assertSame(Object.prototype,
             Object.getPrototypeOf(GeneratorObjectPrototype));
  assertSame(GeneratorObjectPrototype,
             Object.getPrototypeOf((function*(){yield 1}).prototype));

  var expected_property_names = ["next", "throw", "constructor"];
  var found_property_names =
      Object.getOwnPropertyNames(GeneratorObjectPrototype);

  expected_property_names.sort();
  found_property_names.sort();

  assertArrayEquals(expected_property_names, found_property_names);

  iterator_desc = Object.getOwnPropertyDescriptor(GeneratorObjectPrototype,
      Symbol.iterator);
  assertTrue(iterator_desc !== undefined);
  assertFalse(iterator_desc.writable);
  assertFalse(iterator_desc.enumerable);
  assertFalse(iterator_desc.configurable);

  // The generator object's "iterator" function is just the identity.
  assertSame(iterator_desc.value.call(42), 42);
}
TestGeneratorObjectPrototype();


// This tests the object that would be called "GeneratorFunction", if it were
// like "Function".
function TestGeneratorFunction() {
  assertSame(GeneratorFunctionPrototype, GeneratorFunction.prototype);
  assertTrue(g instanceof GeneratorFunction);

  assertSame(Function, Object.getPrototypeOf(GeneratorFunction));
  assertTrue(g instanceof Function);

  assertEquals("function* g() { yield 1; }", g.toString());

  // Not all functions are generators.
  assertTrue(f instanceof Function);  // Sanity check.
  assertTrue(!(f instanceof GeneratorFunction));

  assertTrue((new GeneratorFunction()) instanceof GeneratorFunction);
  assertTrue(GeneratorFunction() instanceof GeneratorFunction);
}
TestGeneratorFunction();


function TestPerGeneratorPrototype() {
  assertTrue((function*(){}).prototype !== (function*(){}).prototype);
  assertTrue((function*(){}).prototype !== g.prototype);
  assertSame(GeneratorObjectPrototype, Object.getPrototypeOf(g.prototype));
  assertTrue(!(g.prototype instanceof Function));
  assertSame(typeof (g.prototype), "object");

  assertArrayEquals([], Object.getOwnPropertyNames(g.prototype));
}
TestPerGeneratorPrototype();
