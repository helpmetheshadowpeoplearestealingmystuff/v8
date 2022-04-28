// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbofan --turbo-inlining

function main() {
  function vul(x0, x1, ...args) {
    const res = Reflect.construct(Array,args,vul);
    let local_1;
    let local_2;
    let local_3;
    let local_4;
    let local_5;
    return res;
  }

  %PrepareFunctionForOptimization(vul);
  return vul(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);
}

%PrepareFunctionForOptimization(main);
main();
const unoptimized_result = main();
%OptimizeFunctionOnNextCall(main);
const optimized_result = main();

assertEquals(unoptimized_result, optimized_result);
