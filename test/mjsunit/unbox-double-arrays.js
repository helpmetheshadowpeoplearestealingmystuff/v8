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

// Test dictionary -> double elements -> dictionary elements round trip

// Flags: --allow-natives-syntax --unbox-double-arrays --expose-gc
var large_array_size = 500000;
var approx_dict_to_elements_threshold = 69000;

var name = 0;

function expected_array_value(i) {
  if ((i % 2) == 0) {
    return i;
  } else {
    return i + 0.5;
  }
}

function force_to_fast_double_array(a) {
  for (var i= 0; i < approx_dict_to_elements_threshold; ++i ) {
    a[i] = expected_array_value(i);
  }
  assertTrue(%HasFastDoubleElements(a));
}

function testOneArrayType(allocator) {
  var large_array = new allocator(500000);
  force_to_fast_double_array(large_array);
  var six = 6;

  for (var i= 0; i < approx_dict_to_elements_threshold; i += 501 ) {
    assertEquals(expected_array_value(i), large_array[i]);
  }

  // This function has a constant and won't get inlined.
  function computed_6() {
    return six;
  }

  // Multiple versions of the test function makes sure that IC/Crankshaft state
  // doesn't get reused.
  function test_various_loads(a, value_5, value_6, value_7) {
    assertTrue(%HasFastDoubleElements(a));
    assertEquals(value_5, a[5]);
    assertEquals(value_6, a[6]);
    assertEquals(value_6, a[computed_6()]); // Test non-constant key
    assertEquals(value_7, a[7]);
    assertEquals(undefined, a[large_array_size-1]);
    assertEquals(undefined, a[-1]);
    assertEquals(large_array_size, a.length);
    assertTrue(%HasFastDoubleElements(a));
  }

  function test_various_loads2(a, value_5, value_6, value_7) {
    assertTrue(%HasFastDoubleElements(a));
    assertEquals(value_5, a[5]);
    assertEquals(value_6, a[6]);
    assertEquals(value_6, a[computed_6()]); // Test non-constant key
    assertEquals(value_7, a[7]);
    assertEquals(undefined, a[large_array_size-1]);
    assertEquals(undefined, a[-1]);
    assertEquals(large_array_size, a.length);
    assertTrue(%HasFastDoubleElements(a));
  }

  function test_various_loads3(a, value_5, value_6, value_7) {
    assertTrue(%HasFastDoubleElements(a));
    assertEquals(value_5, a[5]);
    assertEquals(value_6, a[6]);
    assertEquals(value_6, a[computed_6()]); // Test non-constant key
    assertEquals(value_7, a[7]);
    assertEquals(undefined, a[large_array_size-1]);
    assertEquals(undefined, a[-1]);
    assertEquals(large_array_size, a.length);
    assertTrue(%HasFastDoubleElements(a));
  }

  function test_various_loads4(a, value_5, value_6, value_7) {
    assertTrue(%HasFastDoubleElements(a));
    assertEquals(value_5, a[5]);
    assertEquals(value_6, a[6]);
    assertEquals(value_6, a[computed_6()]); // Test non-constant key
    assertEquals(value_7, a[7]);
    assertEquals(undefined, a[large_array_size-1]);
    assertEquals(undefined, a[-1]);
    assertEquals(large_array_size, a.length);
    assertTrue(%HasFastDoubleElements(a));
  }

  function test_various_loads5(a, value_5, value_6, value_7) {
    assertTrue(%HasFastDoubleElements(a));
    assertEquals(value_5, a[5]);
    assertEquals(value_6, a[6]);
    assertEquals(value_6, a[computed_6()]); // Test non-constant key
    assertEquals(value_7, a[7]);
    assertEquals(undefined, a[large_array_size-1]);
    assertEquals(undefined, a[-1]);
    assertEquals(large_array_size, a.length);
    assertTrue(%HasFastDoubleElements(a));
  }

  function test_various_loads6(a, value_5, value_6, value_7) {
    assertTrue(%HasFastDoubleElements(a));
    assertEquals(value_5, a[5]);
    assertEquals(value_6, a[6]);
    assertEquals(value_6, a[computed_6()]); // Test non-constant key
    assertEquals(value_7, a[7]);
    assertEquals(undefined, a[large_array_size-1]);
    assertEquals(undefined, a[-1]);
    assertEquals(large_array_size, a.length);
    assertTrue(%HasFastDoubleElements(a));
  }

  function test_various_stores(a, value_5, value_6, value_7) {
    assertTrue(%HasFastDoubleElements(a));
    a[5] = value_5;
    a[computed_6()] = value_6;
    a[7] = value_7;
    assertTrue(%HasFastDoubleElements(a));
  }

  // Test double and integer values
  test_various_loads(large_array,
                     expected_array_value(5),
                     expected_array_value(6),
                     expected_array_value(7));
  test_various_loads(large_array,
                     expected_array_value(5),
                     expected_array_value(6),
                     expected_array_value(7));
  test_various_loads(large_array,
                     expected_array_value(5),
                     expected_array_value(6),
                     expected_array_value(7));
  %OptimizeFunctionOnNextCall(test_various_loads);
  test_various_loads(large_array,
                     expected_array_value(5),
                     expected_array_value(6),
                     expected_array_value(7));

  // Test NaN values
  test_various_stores(large_array, NaN, -NaN, expected_array_value(7));

  test_various_loads2(large_array,
                      NaN,
                      -NaN,
                      expected_array_value(7));
  test_various_loads2(large_array,
                      NaN,
                      -NaN,
                      expected_array_value(7));
  test_various_loads2(large_array,
                      NaN,
                      -NaN,
                      expected_array_value(7));
  %OptimizeFunctionOnNextCall(test_various_loads2);
  test_various_loads2(large_array,
                      NaN,
                      -NaN,
                      expected_array_value(7));

  // Test Infinity values
  test_various_stores(large_array,
                      Infinity,
                      -Infinity,
                      expected_array_value(7));

  test_various_loads3(large_array,
                      Infinity,
                      -Infinity,
                      expected_array_value(7));
  test_various_loads3(large_array,
                      Infinity,
                      -Infinity,
                      expected_array_value(7));
  test_various_loads3(large_array,
                      Infinity,
                      -Infinity,
                      expected_array_value(7));
  %OptimizeFunctionOnNextCall(test_various_loads3);
  test_various_loads3(large_array,
                      Infinity,
                      -Infinity,
                      expected_array_value(7));

  // Test the hole for the default runtime implementation.
  delete large_array[5];
  delete large_array[6];
  test_various_loads4(large_array,
                      undefined,
                      undefined,
                      expected_array_value(7));

  // Test the keyed load IC implementation when the value is the hole.
  test_various_stores(large_array,
                      expected_array_value(5),
                      expected_array_value(6),
                      expected_array_value(7));
  test_various_loads5(large_array,
                      expected_array_value(5),
                      expected_array_value(6),
                      expected_array_value(7));
  test_various_loads5(large_array,
                      expected_array_value(5),
                      expected_array_value(6),
                      expected_array_value(7));
  delete large_array[5];
  delete large_array[6];
  test_various_loads5(large_array,
                      undefined,
                      undefined,
                      expected_array_value(7));
  test_various_loads5(large_array,
                      undefined,
                      undefined,
                      expected_array_value(7));

  // Make sure Crankshaft code handles the hole correctly (bailout)
  test_various_stores(large_array,
                      expected_array_value(5),
                      expected_array_value(6),
                      expected_array_value(7));
  test_various_loads6(large_array,
                      expected_array_value(5),
                      expected_array_value(6),
                      expected_array_value(7));
  test_various_loads6(large_array,
                      expected_array_value(5),
                      expected_array_value(6),
                      expected_array_value(7));
  %OptimizeFunctionOnNextCall(test_various_loads6);
  test_various_loads6(large_array,
                      expected_array_value(5),
                      expected_array_value(6),
                      expected_array_value(7));

  delete large_array[5];
  delete large_array[6];
  test_various_loads6(large_array,
                      undefined,
                      undefined,
                      expected_array_value(7));

  // Test stores for non-NaN.
  %OptimizeFunctionOnNextCall(test_various_stores);
  test_various_stores(large_array,
                      expected_array_value(5),
                      expected_array_value(6),
                      expected_array_value(7));

  test_various_stores(large_array,
                      expected_array_value(5),
                      expected_array_value(6),
                      expected_array_value(7));

  test_various_loads6(large_array,
                      expected_array_value(5),
                      expected_array_value(6),
                      expected_array_value(7));

  // Test NaN behavior for stores.
  test_various_stores(large_array,
                      NaN,
                      -NaN,
                      expected_array_value(7));

  test_various_stores(large_array,
                      NaN,
                      -NaN,
                      expected_array_value(7));

  test_various_loads6(large_array,
                      NaN,
                      -NaN,
                      expected_array_value(7));

  // Test Infinity behavior for stores.
  test_various_stores(large_array,
                      Infinity,
                      -Infinity,
                      expected_array_value(7));

  test_various_stores(large_array,
                      Infinity,
                      -Infinity,
                      expected_array_value(7));

  test_various_loads6(large_array,
                      Infinity,
                      -Infinity,
                      expected_array_value(7));

  assertTrue(%GetOptimizationStatus(test_various_stores) != 2);

  // Make sure that we haven't converted from fast double.
  assertTrue(%HasFastDoubleElements(large_array));
  // Cause the array to grow beyond it's JSArray length. This will double the
  // size of the capacity and force the array into "slow" dictionary case.
  large_array[large_array_size+1] = 50;
  assertTrue(%HasDictionaryElements(large_array));
  assertEquals(50, large_array[large_array_size+1]);
  assertEquals(large_array_size+2, large_array.length);
  assertEquals(Infinity, large_array[5]);
  assertEquals(undefined, large_array[large_array_size-1]);
  assertEquals(undefined, large_array[-1]);
  assertEquals(large_array_size+2, large_array.length);

  // Test dictionary -> double elements -> fast elements.
  var large_array2 = new allocator(large_array_size);
  force_to_fast_double_array(large_array2);
  delete large_array2[5];

  // Convert back to fast elements and make sure the contents of the array are
  // unchanged.
  large_array2[25] = new Object();
  assertTrue(%HasFastElements(large_array2));
  for (var i= 0; i < approx_dict_to_elements_threshold; i += 500 ) {
    if (i != 25 && i != 5) {
      assertEquals(expected_array_value(i), large_array2[i]);
    }
  }
  assertEquals(undefined, large_array2[5])
      assertEquals(undefined, large_array2[large_array_size-1])
      assertEquals(undefined, large_array2[-1])
      assertEquals(large_array_size, large_array2.length);
}

testOneArrayType(Array);
