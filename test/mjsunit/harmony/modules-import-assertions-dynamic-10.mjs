// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --harmony-import-assertions

var result1;
var result2;
import('modules-skip-1.json', { get assert() { throw 'bad \'assert\' getter!'; } }).then(
    () => assertUnreachable('Should have failed due to throwing getter'),
    error => result1 = error);
import('modules-skip-1.json', { assert: { get assertionKey() { throw 'bad \'assertionKey\' getter!'; } } }).then(
    () => assertUnreachable('Should have failed due to throwing getter'),
    error => result2 = error);

%PerformMicrotaskCheckpoint();

assertEquals('bad \'assert\' getter!', result1);
assertEquals('bad \'assertionKey\' getter!', result2);