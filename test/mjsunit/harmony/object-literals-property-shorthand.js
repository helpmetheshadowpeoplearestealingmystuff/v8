// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-object-literals


(function TestBasics() {
  var x = 1;
  var object = {x};
  assertEquals(1, object.x);
})();


(function TestDescriptor() {
  var x = 1;
  var object = {x};
  var descr = Object.getOwnPropertyDescriptor(object, 'x');
  assertEquals(1, descr.value);
  assertTrue(descr.enumerable);
  assertTrue(descr.writable);
  assertTrue(descr.configurable);
})();


(function TestNotDefined() {
  'use strict';
  assertThrows(function() {
    return {notDefined};
  }, ReferenceError);
})();


(function TestLet() {
  var let = 1;
  var object = {let};
  assertEquals(1, object.let);
})();


(function TestYieldInFunction() {
  var yield = 1;
  var object = {yield};
  assertEquals(1, object.yield);
})();


(function TestToString() {
  function f(x) { return {x}; }
  assertEquals('function f(x) { return {x}; }', f.toString());
})();
