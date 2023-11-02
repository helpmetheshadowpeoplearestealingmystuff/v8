// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --experimental-wasm-exnref --turboshaft-wasm
// Flags: --no-liftoff

// This file is for the most parts a direct port of
// test/mjsunit/wasm/exceptions-rethrow.js using the new exception handling
// proposal.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");
d8.file.execute("test/mjsunit/wasm/exceptions-utils.js");

// Test that rethrow expressions can target catch blocks.
(function TestRethrowInCatch() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addTag(kSig_v_v);
  builder.addFunction("rethrow0", kSig_v_v)
      .addBody([
        kExprBlock, kExnRefCode,
          kExprTryTable, kWasmVoid, 1,
          kCatchRef, except, 0,
            kExprThrow, except,
          kExprEnd,
          kExprBr, 1,
        kExprEnd,
        kExprThrowRef,
  ]).exportFunc();
  builder.addFunction("rethrow1", kSig_i_i)
      .addBody([
        kExprTry, kWasmI32,
          kExprThrow, except,
        kExprCatch, except,
          kExprLocalGet, 0,
          kExprI32Eqz,
          kExprIf, kWasmVoid,
            kExprRethrow, 1,
          kExprEnd,
          kExprI32Const, 23,
        kExprEnd
  ]).exportFunc();
  let instance = builder.instantiate();

  assertWasmThrows(instance, except, [], () => instance.exports.rethrow0());
  assertWasmThrows(instance, except, [], () => instance.exports.rethrow1(0));
  assertEquals(23, instance.exports.rethrow1(1));
})();

// Test that an exception being rethrow can be caught by another local catch
// block in the same function without ever unwinding the activation.
(function TestRethrowRecatch() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addTag(kSig_v_v);
  let sig_v_e = builder.addType(makeSig([kWasmExnRef], []));
  builder.addFunction("rethrow_recatch", kSig_i_i)
      .addBody([
        kExprBlock, kExnRefCode,
          kExprTryTable, kWasmVoid, 1,
          kCatchRef, except, 0,
            kExprThrow, except,
          kExprEnd,
          kExprUnreachable,
        kExprEnd,
        kExprBlock, sig_v_e,
          kExprTryTable, sig_v_e, 1,
          kCatchNoRef, except, 0,
            kExprLocalGet, 0,
            kExprI32Eqz,
            kExprIf, sig_v_e,
              kExprThrowRef,
            kExprElse,
              kExprDrop,
            kExprEnd,
            kExprI32Const, 42,
            kExprReturn,
          kExprEnd,
        kExprEnd,
        kExprI32Const, 23,
  ]).exportFunc();
  let instance = builder.instantiate();

  assertEquals(23, instance.exports.rethrow_recatch(0));
  assertEquals(42, instance.exports.rethrow_recatch(1));
})();
