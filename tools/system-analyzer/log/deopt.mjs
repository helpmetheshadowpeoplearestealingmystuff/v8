// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import { LogEntry } from './log.mjs';

export class DeoptLogEntry extends LogEntry {
  constructor(type, time) {
    super(type, time);
  }
}
