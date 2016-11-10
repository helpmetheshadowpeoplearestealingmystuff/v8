// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var v = 0;
function foo(a) {
  v = a;
}

foo(1);
foo(2);
%OptimizeFunctionOnNextCall(foo);
foo(3);
assertEquals(3, v);

Object.freeze(this);

foo(4);
foo(5);
%OptimizeFunctionOnNextCall(foo);
foo(6);
assertEquals(3, v);
