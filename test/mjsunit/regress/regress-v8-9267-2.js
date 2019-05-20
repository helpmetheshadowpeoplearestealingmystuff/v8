// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function bar(a) {
  return Object.defineProperty(a, 'x', {get() { return 1; }});
}

function foo() {
  return {};
}

%NeverOptimizeFunction(bar);
%PrepareFunctionForOptimization(foo);
bar(foo());
const a = bar(foo());
%OptimizeFunctionOnNextCall(foo);
const b = bar(foo());

assertTrue(%HaveSameMap(a, b));
