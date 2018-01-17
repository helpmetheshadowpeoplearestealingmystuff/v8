// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

var kV8MaxWasmMemoryPages = 32767;  // ~ 2 GiB
var kSpecMaxWasmMemoryPages = 65536;  // 4 GiB

(function testMemorySizeZero() {
  print("testMemorySizeZero()");
  var builder = new WasmModuleBuilder();
  builder.addMemory(0, 0, false);
  builder.addFunction("memory_size", kSig_i_v)
         .addBody([kExprMemorySize, kMemoryZero])
         .exportFunc();
  var module = builder.instantiate();
  assertEquals(0, module.exports.memory_size());
})();

(function testMemorySizeNonZero() {
  print("testMemorySizeNonZero()");
  var builder = new WasmModuleBuilder();
  var size = 11;
  builder.addMemory(size, size, false);
  builder.addFunction("memory_size", kSig_i_v)
         .addBody([kExprMemorySize, kMemoryZero])
         .exportFunc();
  var module = builder.instantiate();
  assertEquals(size, module.exports.memory_size());
})();

(function testMemorySizeSpecMaxOk() {
  print("testMemorySizeV8Max()");
  var builder = new WasmModuleBuilder();
  builder.addMemory(1, kSpecMaxWasmMemoryPages, true);
  builder.addFunction("memory_size", kSig_i_v)
         .addBody([kExprMemorySize, kMemoryZero])
         .exportFunc();
  var module = builder.instantiate();
  assertEquals(1, module.exports.memory_size());
})();

(function testMemorySizeV8MaxPlus1Throws() {
  print("testMemorySizeV8MaxPlus1Throws()");
  var builder = new WasmModuleBuilder();
  builder.addMemory(kV8MaxWasmMemoryPages + 1,
                    kV8MaxWasmMemoryPages + 1, false);
  builder.addFunction("memory_size", kSig_i_v)
         .addBody([kExprMemorySize, kMemoryZero])
         .exportFunc();
  assertThrows(() => builder.instantiate());
})();

(function testMemorySpecMaxOk() {
  print("testMemoryInitialMaxOk()");
  var mem = new WebAssembly.Memory(
      {initial: 1, maximum: kSpecMaxWasmMemoryPages});
  assertThrows(() => builder.instantiate());
})();

(function testMemoryInitialMaxPlus1Throws() {
  print("testMemoryInitialMaxOk()");
  assertThrows(() => new WebAssembly.Memory(
      {initial: kV8WasmMaxMemoryPages + 1}));
})();
