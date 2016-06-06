// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function __f_71(stdlib, buffer) {
  "use asm";
  var __v_22 = new stdlib.Float64Array(buffer);
  function __f_26() {
    __v_22 = __v_22;
  }
  return {__f_26: __f_26};
}

assertThrows(function() { Wasm.instantiateModuleFromAsm( __f_71.toString()); });
