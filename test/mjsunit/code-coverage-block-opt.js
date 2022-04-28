// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-always-turbofan --turbofan
// Flags: --no-stress-flush-code --turbo-inlining
// Files: test/mjsunit/code-coverage-utils.js

if (isNeverOptimizeLiteMode()) {
  print("Warning: skipping test that requires optimization in Lite mode.");
  testRunner.quit(0);
}

%DebugToggleBlockCoverage(true);

TestCoverage(
"optimized and inlined functions",
`
function g() { if (true) nop(); }         // 0000
function f() { g(); g(); }                // 0050
%PrepareFunctionForOptimization(f);       // 0100
f(); f(); %OptimizeFunctionOnNextCall(f); // 0150
f(); f(); f(); f(); f(); f();             // 0200
`,
[{"start":0,"end":249,"count":1},
 {"start":0,"end":33,"count":16},
 {"start":50,"end":76,"count":8}]
);

// This test is tricky: it requires a non-toplevel, optimized function.
// After initial collection, counts are cleared. Further invocation_counts
// are not collected for optimized functions, and on the next coverage
// collection we and up with an uncovered function with an uncovered parent
// but with non-trivial block coverage.
TestCoverage("Partial coverage collection",
`
!function() {                             // 0000
  function f(x) {                         // 0050
    if (x) { nop(); } else { nop(); }     // 0100
  }                                       // 0150
  %PrepareFunctionForOptimization(f);     // 0200
  f(true); f(true);                       // 0250
  %OptimizeFunctionOnNextCall(f);         // 0300
  %DebugCollectCoverage();                // 0350
  f(false);                               // 0400
}();                                      // 0450
`,
[{"start":52,"end":153,"count":1},
 {"start":111,"end":121,"count":0}]
);

%DebugToggleBlockCoverage(false);
