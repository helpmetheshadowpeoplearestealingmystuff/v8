// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

// Test that NumberModulus with Number feedback works if only in the
// end SimplifiedLowering figures out that the inputs to this operation
// are actually Unsigned32.
(function() {
  // We need a separately polluted % with NumberOrOddball feedback.
  function bar(x) { return x % 2; }
  bar(undefined);  // The % feedback is now NumberOrOddball.

  // Now just use the gadget above in a way that only after RETYPE
  // in SimplifiedLowering we find out that the `x` is actually in
  // Unsigned32 range (based on taking the SignedSmall feedback on
  // the + operator).
  function foo(x) {
    x = (x >>> 0) + 1;
    return bar(x) | 0;
  }

  assertEquals(0, foo(1));
  assertEquals(1, foo(2));
  assertEquals(0, foo(3));
  assertEquals(1, foo(4));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0, foo(1));
  assertEquals(1, foo(2));
  assertEquals(0, foo(3));
  assertEquals(1, foo(4));
  assertOptimized(foo);
})();

// Test that NumberModulus with Number feedback works if only in the
// end SimplifiedLowering figures out that the inputs to this operation
// are actually Signed32.
(function() {
  // We need a separately polluted % with NumberOrOddball feedback.
  function bar(x) { return x % 2; }
  bar(undefined);  // The % feedback is now NumberOrOddball.

  // Now just use the gadget above in a way that only after RETYPE
  // in SimplifiedLowering we find out that the `x` is actually in
  // Signed32 range (based on taking the SignedSmall feedback on
  // the + operator).
  function foo(x) {
    x = (x | 0) + 1;
    return bar(x) | 0;
  }

  assertEquals(0, foo(1));
  assertEquals(1, foo(2));
  assertEquals(0, foo(3));
  assertEquals(1, foo(4));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0, foo(1));
  assertEquals(1, foo(2));
  assertEquals(0, foo(3));
  assertEquals(1, foo(4));
  assertOptimized(foo);
})();

// Test that SpeculativeNumberModulus with Number feedback works if
// only in the end SimplifiedLowering figures out that the inputs to
// this operation are actually Unsigned32.
(function() {
  // We need to use an object literal here to make sure that the
  // SpeculativeNumberModulus is not turned into a NumberModulus
  // early during JSTypedLowering.
  function bar(x) { return {x}.x % 2; }
  bar(undefined);  // The % feedback is now NumberOrOddball.

  // Now just use the gadget above in a way that only after RETYPE
  // in SimplifiedLowering we find out that the `x` is actually in
  // Unsigned32 range (based on taking the SignedSmall feedback on
  // the + operator).
  function foo(x) {
    x = (x >>> 0) + 1;
    return bar(x) | 0;
  }

  assertEquals(0, foo(1));
  assertEquals(1, foo(2));
  assertEquals(0, foo(3));
  assertEquals(1, foo(4));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0, foo(1));
  assertEquals(1, foo(2));
  assertEquals(0, foo(3));
  assertEquals(1, foo(4));
  assertOptimized(foo);
})();

// Test that SpeculativeNumberModulus with Number feedback works if
// only in the end SimplifiedLowering figures out that the inputs to
// this operation are actually Signed32.
(function() {
  // We need to use an object literal here to make sure that the
  // SpeculativeNumberModulus is not turned into a NumberModulus
  // early during JSTypedLowering.
  function bar(x) { return {x}.x % 2; }
  bar(undefined);  // The % feedback is now NumberOrOddball.

  // Now just use the gadget above in a way that only after RETYPE
  // in SimplifiedLowering we find out that the `x` is actually in
  // Signed32 range (based on taking the SignedSmall feedback on
  // the + operator).
  function foo(x) {
    x = (x | 0) + 1;
    return bar(x) | 0;
  }

  assertEquals(0, foo(1));
  assertEquals(1, foo(2));
  assertEquals(0, foo(3));
  assertEquals(1, foo(4));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0, foo(1));
  assertEquals(1, foo(2));
  assertEquals(0, foo(3));
  assertEquals(1, foo(4));
  assertOptimized(foo);
})();

// Test that NumberModulus works in the case where TurboFan
// can infer that the output is Signed32 \/ MinusZero, and
// there's a truncation on the result that identifies zeros
// (via the SpeculativeNumberEqual).
(function() {
  // We need a separately polluted % with NumberOrOddball feedback.
  function bar(x) { return x % 2; }
  bar(undefined);  // The % feedback is now NumberOrOddball.

  // Now we just use the gadget above on an `x` that is known
  // to be in Signed32 range and compare it to 0, which passes
  // a truncation that identifies zeros.
  function foo(x) {
    if (bar(x | 0) == 0) return 0;
    return 1;
  }

  assertEquals(0, foo(2));
  assertEquals(1, foo(1));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0, foo(2));
  assertEquals(1, foo(1));
  assertOptimized(foo);

  // Now `foo` should stay optimized even if `x % 2` would
  // produce -0, aka when we pass a negative value for `x`.
  assertEquals(0, foo(-2));
  assertEquals(1, foo(-1));
  assertOptimized(foo);
})();
