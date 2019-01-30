// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

load("test/mjsunit/wasm/wasm-module-builder.js");

var debug = false;

function SimpleDataSegmentTest(offset) {
  print("SimpleDataSegmentTest(" + offset + ")...");
  var builder = new WasmModuleBuilder();
  builder.addMemory(1, 1, false);
  builder.addFunction("load", kSig_i_i)
    .addBody([kExprGetLocal, 0, kExprI32LoadMem, 0, 0])
    .exportAs("load");
  builder.addDataSegment(offset, [9, 9, 9, 9]);

  var buffer = builder.toBuffer(debug);
  var instance = new WebAssembly.Instance(new WebAssembly.Module(buffer));
  for (var i = offset - 20; i < offset + 20; i += 4) {
    if (i < 0) continue;
    var expected = (i == offset) ? 151587081 : 0;
    assertEquals(expected, instance.exports.load(i));
  }
}

SimpleDataSegmentTest(0);
SimpleDataSegmentTest(4);
SimpleDataSegmentTest(12);
SimpleDataSegmentTest(1064);

function GlobalImportedInitTest(pad) {
  print("GlobaleImportedInitTest(" + pad + ")...");
  var builder = new WasmModuleBuilder();
  builder.addMemory(1, 1, false);

  var g = builder.addImportedGlobal("mod", "offset", kWasmI32);

  while (pad-- > 0) builder.addGlobal(kWasmI32);  // pad

  builder.addFunction("load", kSig_i_i)
    .addBody([kExprGetLocal, 0, kExprI32LoadMem, 0, 0])
    .exportAs("load");
  builder.addDataSegment(g.index, [5, 5, 5, 5], true);

  var buffer = builder.toBuffer(debug);
  var module = new WebAssembly.Module(buffer);

  for (var offset of [0, 12, 192, 1024]) {
    var instance = new WebAssembly.Instance(module, {mod: {offset: offset}});
    for (var i = offset - 20; i < offset + 20; i += 4) {
      if (i < 0) continue;
      var expected = i == offset ? 84215045 : 0;
      assertEquals(expected, instance.exports.load(i));
    }
  }
}

GlobalImportedInitTest(0);
GlobalImportedInitTest(1);
GlobalImportedInitTest(4);
