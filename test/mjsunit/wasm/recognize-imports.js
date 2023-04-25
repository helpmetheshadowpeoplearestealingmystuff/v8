// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-stringref --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

function CheckStackTrace(thrower, reference, topmost_wasm_func) {
  let reference_exception;
  let actual_exception;
  try {
    thrower();
    assertUnreachable();
  } catch (e) {
    actual_exception = e;
  }
  try {
    reference();
    assertUnreachable();
  } catch (e) {
    reference_exception = e;
  }
  assertInstanceof(actual_exception, reference_exception.constructor);
  let actual_stack = actual_exception.stack.split('\n');
  let reference_stack = reference_exception.stack.split('\n');
  assertEquals(reference_stack[0], actual_stack[0]);
  assertEquals(reference_stack[1], actual_stack[1]);
  assertTrue(
      actual_stack[2].startsWith(`    at ${topmost_wasm_func} (wasm://wasm/`));
}

let builder = new WasmModuleBuilder();
let sig_w_w = makeSig([kWasmStringRef], [kWasmStringRef]);
let toLowerCase = builder.addImport("m", "toLowerCase", sig_w_w);

builder.addFunction('call_tolower', sig_w_w).exportFunc().addBody([
  kExprLocalGet, 0,
  kExprCallFunction, toLowerCase,
]);

let module = builder.toModule();

let recognizable = Function.prototype.call.bind(String.prototype.toLowerCase);
let recognizable_imports = { m: { toLowerCase: recognizable } };

let instance1 = new WebAssembly.Instance(module, recognizable_imports);
let call_tolower = instance1.exports.call_tolower;
assertEquals("abc", call_tolower("ABC"));
%WasmTierUpFunction(call_tolower);
assertEquals("abc", call_tolower("ABC"));

// Null should be handled correctly (by throwing the same TypeError that
// JavaScript would throw).
CheckStackTrace(
    () => call_tolower(null), () => String.prototype.toLowerCase.call(null),
    'call_tolower');

// Creating a second instance with identical imports should not cause
// recompilation.
console.log("Second instance.");
let instance2 = new WebAssembly.Instance(module, recognizable_imports);
assertEquals("def", instance2.exports.call_tolower("DEF"));

// Creating a third instance with different imports must not reuse the
// existing optimized code.
console.log("Third instance.");
let other_imports = { m: { toLowerCase: () => "foo" } };
let instance3 = new WebAssembly.Instance(module, other_imports);
assertEquals("foo", instance3.exports.call_tolower("GHI"));
assertEquals("def", instance2.exports.call_tolower("DEF"));
assertEquals("abc", instance1.exports.call_tolower("ABC"));

(function TestIntToString() {
  console.log("Testing IntToString");
  let builder = new WasmModuleBuilder();
  let sig_w_ii = makeSig([kWasmI32, kWasmI32], [kWasmStringRef]);
  let intToString = builder.addImport("m", "intToString", sig_w_ii);
  builder.addFunction('call_inttostring', sig_w_ii).exportFunc().addBody([
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kExprCallFunction, intToString,
  ]);
  let func = Function.prototype.call.bind(Number.prototype.toString);
  let instance = builder.instantiate({ m: { intToString: func } });
  let call_inttostring = instance.exports.call_inttostring;
  %WasmTierUpFunction(call_inttostring);
  assertEquals("42", call_inttostring(42, 10));
  assertEquals("-123", call_inttostring(-123, 10));
  assertEquals("2a", call_inttostring(42, 16));
  assertEquals("2147483647", call_inttostring(2147483647, 10));
  assertEquals("-2147483648", call_inttostring(-2147483648, 10));
  CheckStackTrace(
      () => call_inttostring(1, 99), () => func(1, 99), 'call_inttostring');
})();
