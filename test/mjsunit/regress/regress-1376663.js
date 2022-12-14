// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

function __f_0() {
  onmessage = function(e) {
    import("./does_not_exist.js").then();
    while(true) {
    }
  }
}
function __f_1() {
}
let sab = new SharedArrayBuffer();
let w1 = new Worker(__f_0, {type: 'function'});
w1.postMessage({sab: sab});
let w2 = new Worker(__f_1, {type: 'function'});
