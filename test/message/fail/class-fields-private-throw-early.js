// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-private-fields

class X {
  constructor() {
    this.#x = 1;
  }
}
