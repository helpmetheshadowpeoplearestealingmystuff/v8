// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --expose-gc

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

function makeFFI(func, t) {
  var builder = new WasmModuleBuilder();

  var sig_index = builder.addSignature([t,t,t,t,t,t,t,t,t,t,t]);
  builder.addImport("func", sig_index);
  // Try to create a frame with lots of spilled values and parameters
  // on the stack to try to catch GC bugs in the reference maps for
  // the different parts of the stack.
  builder.addFunction("main", sig_index)
    .addBody([
      kExprGetLocal, 0,         // --
      kExprGetLocal, 1,         // --
      kExprGetLocal, 2,         // --
      kExprGetLocal, 3,         // --
      kExprGetLocal, 4,         // --
      kExprGetLocal, 5,         // --
      kExprGetLocal, 6,         // --
      kExprGetLocal, 7,         // --
      kExprGetLocal, 8,         // --
      kExprGetLocal, 9,         // --
      kExprCallImport, 10, 0,   // --
      kExprGetLocal, 0,         // --
      kExprGetLocal, 1,         // --
      kExprGetLocal, 2,         // --
      kExprGetLocal, 3,         // --
      kExprGetLocal, 4,         // --
      kExprGetLocal, 5,         // --
      kExprGetLocal, 6,         // --
      kExprGetLocal, 7,         // --
      kExprGetLocal, 8,         // --
      kExprGetLocal, 9,         // --
      kExprCallImport, 10, 0    // --
    ])                          // --
    .exportFunc();

  return builder.instantiate({func: func}).exports.main;
}


function print10(a, b, c, d, e, f, g, h, i) {
  print(a + ",", b + ",", c + ",", d + ",", e + ",", f + ",", g + ",", h + ",", i);
  gc();
  print(a + ",", b + ",", c + ",", d + ",", e + ",", f + ",", g + ",", h + ",", i);
}

(function I32Test() {
  var main = makeFFI(print10, kAstI32);
  for (var i = 1; i < 0xFFFFFFF; i <<= 2) {
    main(i - 1, i, i + 2, i + 3, i + 4, i + 5, i + 6, i + 7, i + 8);
  }
})();

(function F32Test() {
  var main = makeFFI(print10, kAstF32);
  for (var i = 1; i < 2e+30; i *= -157) {
    main(i - 1, i, i + 2, i + 3, i + 4, i + 5, i + 6, i + 7, i + 8);
  }
})();

(function I32Test() {
  var main = makeFFI(print10, kAstF64);
  for (var i = 1; i < 2e+80; i *= -1137) {
    main(i - 1, i, i + 2, i + 3, i + 4, i + 5, i + 6, i + 7, i + 8);
  }
})();
