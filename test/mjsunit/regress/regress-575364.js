// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags:  --expose-wasm

function f() {
  "use asm";

}
assertThrows(function() { WASM.asmCompileRun(f.toString()); });
