// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-stress-opt

function foo(x,y) {
  if (x < y) {
    return 42;
  }
  return 24;
}

%PrepareFunctionForOptimization(foo);
assertEquals(42, foo(1,2));
assertEquals(24, foo(6,2));
%OptimizeMaglevOnNextCall(foo);
assertEquals(42, foo(1,2));
assertEquals(24, foo(6,2));


function bar(x,y) {
  if (!(x >= y)) {
    return 42;
  }
  return 24;
}

%PrepareFunctionForOptimization(bar);
assertEquals(42, bar(1,2));
assertEquals(24, bar(6,2));
%OptimizeMaglevOnNextCall(bar);
assertEquals(42, bar(1,2));
assertEquals(24, bar(6,2));
