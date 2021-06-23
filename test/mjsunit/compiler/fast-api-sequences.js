// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file excercises sequences support for  fast API calls.

// Flags: --turbo-fast-api-calls --allow-natives-syntax --opt
// --always-opt is disabled because we rely on particular feedback for
// optimizing to the fastest path.
// Flags: --no-always-opt
// The test relies on optimizing/deoptimizing at predictable moments, so
// it's not suitable for deoptimization fuzzing.
// Flags: --deopt-every-n-times=0

const fast_c_api = new d8.test.FastCAPI();

// ----------- add_all_sequence -----------
// `add_all_sequence` has the following signature:
// double add_all_sequence(bool /*should_fallback*/, Local<Array>)

const max_safe_float = 2**24 - 1;
const add_all_result_full = -42 + 45 +
  Number.MIN_SAFE_INTEGER + Number.MAX_SAFE_INTEGER +
  max_safe_float * 0.5 + Math.PI;
const full_array = [-42, 45,
  Number.MIN_SAFE_INTEGER, Number.MAX_SAFE_INTEGER,
  max_safe_float * 0.5, Math.PI];

function add_all_sequence_smi(arg) {
  return fast_c_api.add_all_sequence(false /* should_fallback */, arg);
}

%PrepareFunctionForOptimization(add_all_sequence_smi);
assertEquals(3, add_all_sequence_smi([-42, 45]));
%OptimizeFunctionOnNextCall(add_all_sequence_smi);

function add_all_sequence_full(arg) {
  return fast_c_api.add_all_sequence(false /* should_fallback */, arg);
}

%PrepareFunctionForOptimization(add_all_sequence_full);
if (fast_c_api.supports_fp_params) {
  assertEquals(add_all_result_full, add_all_sequence_full(full_array));
} else {
  assertEquals(3, add_all_sequence_smi([-42, 45]));
}
%OptimizeFunctionOnNextCall(add_all_sequence_full);

if (fast_c_api.supports_fp_params) {
  // Test that regular call hits the fast path.
  fast_c_api.reset_counts();
  assertEquals(add_all_result_full, add_all_sequence_full(full_array));
  assertOptimized(add_all_sequence_full);
  assertEquals(1, fast_c_api.fast_call_count());
  assertEquals(0, fast_c_api.slow_call_count());
} else {
  // Smi only test - regular call hits the fast path.
  fast_c_api.reset_counts();
  assertEquals(3, add_all_sequence_smi([-42, 45]));
  assertOptimized(add_all_sequence_smi);
  assertEquals(1, fast_c_api.fast_call_count());
  assertEquals(0, fast_c_api.slow_call_count());
}

function add_all_sequence_mismatch(arg) {
  return fast_c_api.add_all_sequence(false /*should_fallback*/,
    arg);
}

%PrepareFunctionForOptimization(add_all_sequence_mismatch);
assertThrows(() => add_all_sequence_mismatch());
%OptimizeFunctionOnNextCall(add_all_sequence_mismatch);

// Test that passing non-array arguments falls down the slow path.
fast_c_api.reset_counts();
assertThrows(() => add_all_sequence_mismatch(42));
assertOptimized(add_all_sequence_mismatch);
assertEquals(0, fast_c_api.fast_call_count());
assertEquals(1, fast_c_api.slow_call_count());

fast_c_api.reset_counts();
assertThrows(() => add_all_sequence_mismatch({}));
assertOptimized(add_all_sequence_mismatch);
assertEquals(0, fast_c_api.fast_call_count());
assertEquals(1, fast_c_api.slow_call_count());

fast_c_api.reset_counts();
assertThrows(() => add_all_sequence_mismatch('string'));
assertOptimized(add_all_sequence_mismatch);
assertEquals(0, fast_c_api.fast_call_count());
assertEquals(1, fast_c_api.slow_call_count());

fast_c_api.reset_counts();
assertThrows(() => add_all_sequence_mismatch(Symbol()));
assertOptimized(add_all_sequence_mismatch);
assertEquals(0, fast_c_api.fast_call_count());
assertEquals(1, fast_c_api.slow_call_count());
