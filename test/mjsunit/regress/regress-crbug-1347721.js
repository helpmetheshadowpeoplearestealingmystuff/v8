// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-rab-gsab

for (let i = 0; i < 10000; i++) {
  const rab = new ArrayBuffer(1632, {"maxByteLength": 9943683});
  const ta1 = new Uint32Array(rab);
  const ta2 = new Float32Array(rab);
  rab.resize(2004);
  ta1["set"](ta2);
}
