// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function TestThrowBeforeLengthToPrimitive() {
  // From 22.2.4.5 TypedArray ( buffer [ , byteOffset [ , length ] ] ), check
  // that step 7:
  //    If offset modulo elementSize ≠ 0, throw a RangeError exception.
  // happens before step 11:
  //    Let newLength be ? ToIndex(length).
  var expected = ["offset.toPrimitive"];
  var actual = [];
  var offset = {};
  offset[Symbol.toPrimitive] = function() {
    actual.push("offset.toPrimitive");
    return 1;
  };

  var length = {};
  length[Symbol.toPrimitive] = function() {
    actual.push("length.toPrimitive");
    return 1;
  };

  var buffer = new ArrayBuffer(16);

  assertThrows(function() {
    new Uint32Array(buffer, offset, length)
  }, RangeError);
  assertEquals(expected, actual);
})();
