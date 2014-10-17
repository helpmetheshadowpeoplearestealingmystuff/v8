// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


load('base.js');
load('forof.js');


var success = true;

function PrintResult(name, result) {
  print(name + '-Iterators(Score): ' + result);
}


function PrintError(name, error) {
  PrintResult(name, error);
  success = false;
}


BenchmarkSuite.config.doWarmup = undefined;
BenchmarkSuite.config.doDeterministic = undefined;

BenchmarkSuite.RunSuites({ NotifyResult: PrintResult,
                           NotifyError: PrintError });
