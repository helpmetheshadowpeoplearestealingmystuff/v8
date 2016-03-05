// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

load('test/mjsunit/wasm/wasm-constants.js');
load('test/mjsunit/wasm/wasm-module-builder.js');

var debug = (arguments[0] == "debug");

(function BasicTest() {
    var module = new WasmModuleBuilder();
    module.addMemory(1, 2, false);
    module.addFunction("foo", [kAstI32])
        .addBody([kExprI8Const, 11])
        .exportAs("blarg");

    var buffer = module.toBuffer(debug);
    var instance = _WASMEXP_.instantiateModule(buffer);
    assertEquals(11, instance.exports.blarg());
})();

(function ImportTest() {
    var module = new WasmModuleBuilder();
    var index = module.addImport("print", [kAstStmt, kAstI32]);
    module.addFunction("foo", [kAstStmt])
        .addBody([kExprCallImport, index, kExprI8Const, 13])
        .exportAs("main");

    var buffer = module.toBuffer(debug);
    var instance = _WASMEXP_.instantiateModule(buffer, {print: print});
    print("should print 13! ");
    instance.exports.main();
})();

(function LocalsTest() {
    var module = new WasmModuleBuilder();
    module.addFunction(undefined, [kAstI32, kAstI32])
        .addLocals({i32_count: 1})
        .addBody([kExprSetLocal, 1, kExprGetLocal, 0])
        .exportAs("main");

    var buffer = module.toBuffer(debug);
    var instance = _WASMEXP_.instantiateModule(buffer);
    assertEquals(19, instance.exports.main(19));
    assertEquals(27777, instance.exports.main(27777));
})();

(function CallTest() {
    var module = new WasmModuleBuilder();
    module.addFunction("add", [kAstI32, kAstI32, kAstI32])
        .addBody([kExprI32Add, kExprGetLocal, 0, kExprGetLocal, 1]);
    module.addFunction("main", [kAstI32, kAstI32, kAstI32])
        .addBody([kExprCallFunction, 0, kExprGetLocal, 0, kExprGetLocal, 1])
        .exportAs("main");

    var instance = module.instantiate();
    assertEquals(44, instance.exports.main(11, 33));
    assertEquals(7777, instance.exports.main(2222, 5555));
})();

(function IndirectCallTest() {
    var module = new WasmModuleBuilder();
    module.addFunction("add", [kAstI32, kAstI32, kAstI32])
        .addBody([kExprI32Add, kExprGetLocal, 0, kExprGetLocal, 1]);
    module.addFunction("main", [kAstI32, kAstI32, kAstI32, kAstI32])
        .addBody([kExprCallIndirect, 0, kExprGetLocal,
                  0, kExprGetLocal, 1, kExprGetLocal, 2])
        .exportAs("main");
    module.appendToFunctionTable([0]);

    var instance = module.instantiate();
    assertEquals(44, instance.exports.main(0, 11, 33));
    assertEquals(7777, instance.exports.main(0, 2222, 5555));
    assertThrows(function() { instance.exports.main(1, 1, 1); });
})();

(function DataSegmentTest() {
    var module = new WasmModuleBuilder();
    module.addMemory(1, 1, false);
    module.addFunction("load", [kAstI32, kAstI32])
        .addBody([kExprI32LoadMem, 0, kExprGetLocal, 0])
        .exportAs("load");
    module.addDataSegment(0, [9, 9, 9, 9], true);

    var buffer = module.toBuffer(debug);
    var instance = _WASMEXP_.instantiateModule(buffer);
    assertEquals(151587081, instance.exports.load(0));
})();
