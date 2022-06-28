// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-gc

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
builder.addFunction("leak", kSig_l_v)
  .exportFunc()
  .addLocals(kWasmI64, 10)
  .addBody([
    kExprLocalGet, 0,
    kExprLocalGet, 0,
    kExprLocalGet, 0,
    kExprLocalGet, 0,
    kExprLocalGet, 0,
    kExprBlock, kWasmVoid,
      kExprLoop, kWasmVoid,
        kExprRefNull, kAnyRefCode,
        kExprBrOnNonNull, 0,
        kExprBr, 1,
      kExprEnd,  // loop
    kExprEnd,  // block
    kExprDrop,
    kExprDrop,
    kExprDrop,
    kExprDrop,
  ]);

let loop_type = builder.addType(makeSig([kWasmFuncRef], []));
builder.addFunction("crash", kSig_v_v).exportFunc().addBody([
  kExprRefFunc, 0,
  kExprLoop, loop_type,
    kExprRefFunc, 0,
    kGCPrefix, kExprBrOnNonFunc, 0,
    kExprDrop,
    kExprDrop,
  kExprEnd,  // loop
]);

let array_type = builder.addArray(wasmOptRefType(kSig_i_i), true);
builder.addFunction("array", kSig_l_v).exportFunc()
  .addLocals(kWasmI64, 10)
  .addBody([
    kExprLocalGet, 0,
    kExprLocalGet, 0,
    kExprLocalGet, 0,
    kExprLocalGet, 0,
    kExprLocalGet, 0,
    kExprI32Const, 0,
    kGCPrefix, kExprArrayNewDefault, array_type,
    kExprDrop,
    kExprDrop,
    kExprDrop,
    kExprDrop,
    kExprDrop,
])

let instance = builder.instantiate();
let result = instance.exports.leak();
assertEquals(0n, result);
result = instance.exports.array();
assertEquals(0n, result);
instance.exports.crash();
