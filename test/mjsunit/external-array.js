// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Flags: --allow-natives-syntax --expose-gc

// This is a regression test for overlapping key and value registers.
function f(a) {
  a[0] = 0;
  a[1] = 0;
}

var a = new Int32Array(2);
for (var i = 0; i < 5; i++) {
  f(a);
}
%OptimizeFunctionOnNextCall(f);
f(a);

assertEquals(0, a[0]);
assertEquals(0, a[1]);

// Test the correct behavior of the |BYTES_PER_ELEMENT| property (which is
// "constant", but not read-only).
a = new Int32Array(2);
assertEquals(4, a.BYTES_PER_ELEMENT);
a.BYTES_PER_ELEMENT = 42;
assertEquals(42, a.BYTES_PER_ELEMENT);
a = new Uint8Array(2);
assertEquals(1, a.BYTES_PER_ELEMENT);
a = new Int16Array(2);
assertEquals(2, a.BYTES_PER_ELEMENT);

// Test Float64Arrays.
function get(a, index) {
  return a[index];
}
function set(a, index, value) {
  a[index] = value;
}

var array = new Float64Array(2);
for (var i = 0; i < 5; i++) {
  set(array, 0, 2.5);
  assertEquals(2.5, array[0]);
}
%OptimizeFunctionOnNextCall(set);
set(array, 0, 2.5);
assertEquals(2.5, array[0]);
set(array, 1, 3.5);
assertEquals(3.5, array[1]);
for (var i = 0; i < 5; i++) {
  assertEquals(2.5, get(array, 0));
  assertEquals(3.5, array[1]);
}
%OptimizeFunctionOnNextCall(get);
assertEquals(2.5, get(array, 0));
assertEquals(3.5, get(array, 1));

// Test loads and stores.
types = [Int8Array, Uint8Array, Int16Array, Uint16Array, Int32Array,
         Uint32Array, PixelArray, Float32Array, Float64Array];

const kElementCount = 40;

function test_load(array, sum) {
  for (var i = 0; i < kElementCount; i++) {
    sum += array[i];
  }
  return sum;
}

function test_load_const_key(array, sum) {
  sum += array[0];
  sum += array[1];
  sum += array[2];
  return sum;
}

function test_store(array, sum) {
  for (var i = 0; i < kElementCount; i++) {
    sum += array[i] = i+1;
  }
  return sum;
}

function test_store_const_key(array, sum) {
  sum += array[0] = 1;
  sum += array[1] = 2;
  sum += array[2] = 3;
  return sum;
}

function run_test(test_func, array, expected_sum_per_run) {
  for (var i = 0; i < 5; i++) test_func(array, 0);
  %OptimizeFunctionOnNextCall(test_func);
  const kRuns = 10;
  var sum = 0;
  for (var i = 0; i < kRuns; i++) {
    sum = test_func(array, sum);
  }
  assertEquals(sum, expected_sum_per_run * kRuns);
  %DeoptimizeFunction(test_func);
  gc();  // Makes V8 forget about type information for test_func.
}

for (var t = 0; t < types.length; t++) {
  var type = types[t];
  var a = new type(kElementCount);
  for (var i = 0; i < kElementCount; i++) {
    a[i] = i;
  }
  
  // Run test functions defined above.
  run_test(test_load, a, 780);
  run_test(test_load_const_key, a, 3);
  run_test(test_store, a, 820);
  run_test(test_store_const_key, a, 6);
  
  // Test the correct behavior of the |length| property (which is read-only).
  assertEquals(kElementCount, a.length);
  a.length = 2;
  assertEquals(kElementCount, a.length);
  assertTrue(delete a.length);
  a.length = 2
  assertEquals(2, a.length);
}
