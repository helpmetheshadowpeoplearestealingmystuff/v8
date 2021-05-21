// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbo-optimize-apply --opt

// These tests do not work well if this script is run more than once (e.g.
// --stress-opt); after a few runs the whole function is immediately compiled
// and assertions would fail. We prevent re-runs.
// Flags: --nostress-opt --no-always-opt

// Tests for optimization of CallWithSpread and CallWithArrayLike.
// This test is in a separate file because it invalidates protectors.

// Test with holey array with default values.
(function () {
  "use strict";

  // Setting value to be retrieved in place of hole.
  Array.prototype[1] = 'x';

  var sum_js3_got_interpreted = true;
  function sum_js3(a, b, c) {
    sum_js3_got_interpreted = %IsBeingInterpreted();
    return a + b + c;
  }
  function foo(x, y) {
    return sum_js3.apply(null, [x, , y]);
  }

  %PrepareFunctionForOptimization(sum_js3);
  %PrepareFunctionForOptimization(foo);
  assertEquals('AxB', foo('A', 'B'));
  assertTrue(sum_js3_got_interpreted);

  // The protector should be invalidated, which prevents inlining.
  %OptimizeFunctionOnNextCall(foo);
  assertEquals('AxB', foo('A', 'B'));
  assertTrue(sum_js3_got_interpreted);
  assertOptimized(foo);
})();
