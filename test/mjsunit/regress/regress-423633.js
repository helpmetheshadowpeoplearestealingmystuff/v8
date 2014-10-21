// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Object.defineProperty(Array.prototype, '0', {
  get: function() { return false; },
});
var a = [1, 2, 3];
assertEquals(a, a.slice());
assertEquals([3], a.splice(2, 1));
