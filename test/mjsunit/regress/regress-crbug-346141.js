// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-symbols

var s = Symbol()
var o = {}
o[s] = 2
o[""] = 3
Object.getOwnPropertySymbols(o)
