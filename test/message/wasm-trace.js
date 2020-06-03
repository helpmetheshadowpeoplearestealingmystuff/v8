// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --trace-wasm --no-wasm-tier-up --liftoff --no-stress-opt

load('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
let kRet23Function = builder.addFunction('ret_23', kSig_i_v)
                         .addBody([kExprI32Const, 23])
                         .exportFunc()
                         .index;
let kRet57Function = builder.addFunction('ret_57', kSig_l_v)
                         .addBody([kExprI64Const, 57])
                         .exportFunc()
                         .index;
let kRet0Function = builder.addFunction('ret_0', kSig_f_v)
                        .addBody(wasmF32Const(0))
                        .exportFunc()
                        .index;
let kRet1Function = builder.addFunction('ret_1', kSig_d_v)
                        .addBody(wasmF64Const(1))
                        .exportFunc()
                        .index;
builder.addFunction('main', kSig_v_v)
    .addBody([
      kExprCallFunction, kRet23Function, kExprDrop,  // -
      kExprCallFunction, kRet57Function, kExprDrop,  // -
      kExprCallFunction, kRet0Function, kExprDrop,   // -
      kExprCallFunction, kRet1Function, kExprDrop    // -
    ])
    .exportAs('main');

let instance = builder.instantiate();
instance.exports.main();
