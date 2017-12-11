// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbo-inline-array-builtins --opt
// Flags: --no-always-opt

// Unknown field access leads to soft-deopt unrelated to find, should still
// lead to correct result.
(() => {
  const a = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
             20, 21, 22, 23, 24, 25];
  let result = 0;
  function eagerDeoptInCalled(deopt) {
    return a.find((v, i) => {
      if (i === 13 && deopt) {
        a.abc = 25;
      }
      result += v;
      return v === 20;
    });
  }
  eagerDeoptInCalled();
  eagerDeoptInCalled();
  %OptimizeFunctionOnNextCall(eagerDeoptInCalled);
  eagerDeoptInCalled();
  assertEquals(20, eagerDeoptInCalled(true));
  eagerDeoptInCalled();
  assertEquals(1050, result);
})();

// Length change detected during loop, must cause properly handled eager deopt.
(() => {
  let called_values;
  function eagerDeoptInCalled(deopt) {
    const a = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
    called_values = [];
    return a.find((v,i) => {
      called_values.push(v);
      a.length = (i === 5 && deopt) ? 8 : 10;
      return v === 9;
    });
  }
  assertEquals(9, eagerDeoptInCalled());
  assertArrayEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], called_values);
  eagerDeoptInCalled();
  %OptimizeFunctionOnNextCall(eagerDeoptInCalled);
  assertEquals(9, eagerDeoptInCalled());
  assertEquals(undefined, eagerDeoptInCalled(true));
  assertArrayEquals([1, 2, 3, 4, 5, 6, 7, 8, undefined, undefined],
                    called_values);
  eagerDeoptInCalled();
})();

// Lazy deopt from a callback that changes the input array. Deopt in a callback
// execution that returns true.
(() => {
  const a = [1, 2, 3, 4, 5];
  function lazyChanger(deopt) {
    return a.find((v, i) => {
      if (i === 3 && deopt) {
        a[3] = 100;
        %DeoptimizeNow();
       }
      return v > 3;
    });
  }
  assertEquals(4, lazyChanger());
  lazyChanger();
  %OptimizeFunctionOnNextCall(lazyChanger);
  assertEquals(4, lazyChanger(true));
  assertEquals(100, lazyChanger());
})();

// Lazy deopt from a callback that changes the input array. Deopt in a callback
// execution that returns false.
(() => {
  const a = [1, 2, 3, 4, 5];
  function lazyChanger(deopt) {
    return a.find((v, i) => {
      if (i === 2 && deopt) {
        a[3] = 100;
        %DeoptimizeNow();
       }
      return v > 3;
    });
  }
  assertEquals(4, lazyChanger());
  lazyChanger();
  %OptimizeFunctionOnNextCall(lazyChanger);
  assertEquals(100, lazyChanger(true));
  assertEquals(100, lazyChanger());
})();

// Escape analyzed array
(() => {
  let result = 0;
  function eagerDeoptInCalled(deopt) {
    const a_noescape = [0, 1, 2, 3, 4, 5];
    a_noescape.find((v, i) => {
      result += v | 0;
      if (i === 13 && deopt) {
        a_noescape.length = 25;
      }
      return false;
    });
  }
  eagerDeoptInCalled();
  eagerDeoptInCalled();
  %OptimizeFunctionOnNextCall(eagerDeoptInCalled);
  eagerDeoptInCalled();
  eagerDeoptInCalled(true);
  eagerDeoptInCalled();
  assertEquals(75, result);
})();

// Lazy deopt from runtime call from inlined callback function.
(() => {
  const a = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
             20, 21, 22, 23, 24, 25];
  let result = 0;
  function lazyDeopt(deopt) {
    a.find((v, i) => {
      result += i;
      if (i === 13 && deopt) {
        %DeoptimizeNow();
      }
      return false;
    });
  }
  lazyDeopt();
  lazyDeopt();
  %OptimizeFunctionOnNextCall(lazyDeopt);
  lazyDeopt();
  lazyDeopt(true);
  lazyDeopt();
  assertEquals(1500, result);
})();

// Lazy deopt from runtime call from non-inline callback function.
(() => {
  const a = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
             20, 21, 22, 23, 24, 25];
  let result = 0;
  function lazyDeopt(deopt) {
    function callback(v, i) {
      result += i;
      if (i === 13 && deopt) {
        %DeoptimizeNow();
      }
      return false;
    }
    %NeverOptimizeFunction(callback);
    a.find(callback);
  }
  lazyDeopt();
  lazyDeopt();
  %OptimizeFunctionOnNextCall(lazyDeopt);
  lazyDeopt();
  lazyDeopt(true);
  lazyDeopt();
  assertEquals(1500, result);
})();

// Call to a.find is done inside a try-catch block and the callback function
// being called actually throws.
(() => {
  const a = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
             20, 21, 22, 23, 24, 25];
  let caught = false;
  function lazyDeopt(deopt) {
    try {
      a.find((v, i) => {
        if (i === 1 && deopt) {
          throw("a");
        }
        return false;
      });
    } catch (e) {
      caught = true;
    }
  }
  lazyDeopt();
  lazyDeopt();
  %OptimizeFunctionOnNextCall(lazyDeopt);
  lazyDeopt();
  assertDoesNotThrow(() => lazyDeopt(true));
  assertTrue(caught);
  lazyDeopt();
})();

// Call to a.find is done inside a try-catch block and the callback function
// being called actually throws, but the callback is not inlined.
(() => {
  let a = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
  let caught = false;
  function lazyDeopt(deopt) {
    function callback(v, i) {
      if (i === 1 && deopt) {
        throw("a");
      }
      return false;
    }
    %NeverOptimizeFunction(callback);
    try {
      a.find(callback);
    } catch (e) {
      caught = true;
    }
  }
  lazyDeopt();
  lazyDeopt();
  %OptimizeFunctionOnNextCall(lazyDeopt);
  lazyDeopt();
  assertDoesNotThrow(() => lazyDeopt(true));
  assertTrue(caught);
  lazyDeopt();
})();

// Call to a.find is done inside a try-catch block and the callback function
// being called throws into a deoptimized caller function.
(function TestThrowIntoDeoptimizedOuter() {
  const a = [1, 2, 3, 4];
  function lazyDeopt(deopt) {
    function callback(v, i) {
      if (i === 1 && deopt) {
        %DeoptimizeFunction(lazyDeopt);
        throw "some exception";
      }
      return v === 3;
    }
    %NeverOptimizeFunction(callback);
    let result = 0;
    try {
      result = a.find(callback);
    } catch (e) {
      assertEquals("some exception", e);
      result = "nope";
    }
    return result;
  }
  assertEquals(3, lazyDeopt(false));
  assertEquals(3, lazyDeopt(false));
  assertEquals("nope", lazyDeopt(true));
  assertEquals("nope", lazyDeopt(true));
  %OptimizeFunctionOnNextCall(lazyDeopt);
  assertEquals(3, lazyDeopt(false));
  assertEquals("nope", lazyDeopt(true));
})();

// An error generated inside the callback includes find in it's
// stack trace.
(() => {
  const re = /Array\.find/;
  function lazyDeopt(deopt) {
    const b = [1, 2, 3];
    let result = 0;
    b.find((v, i) => {
      result += v;
      if (i === 1) {
        const e = new Error();
        assertTrue(re.exec(e.stack) !== null);
      }
      return false;
    });
  }
  lazyDeopt();
  lazyDeopt();
  %OptimizeFunctionOnNextCall(lazyDeopt);
  lazyDeopt();
})();

// An error generated inside a non-inlined callback function also
// includes find in it's stack trace.
(() => {
  const re = /Array\.find/;
  function lazyDeopt(deopt) {
    const b = [1, 2, 3];
    let did_assert_error = false;
    let result = 0;
    function callback(v, i) {
      result += v;
      if (i === 1) {
        const e = new Error();
        assertTrue(re.exec(e.stack) !== null);
        did_assert_error = true;
      }
      return false;
    }
    %NeverOptimizeFunction(callback);
    b.find(callback);
    return did_assert_error;
  }
  lazyDeopt();
  lazyDeopt();
  %OptimizeFunctionOnNextCall(lazyDeopt);
  assertTrue(lazyDeopt());
})();

// An error generated inside a recently deoptimized callback function
// includes find in it's stack trace.
(() => {
  const re = /Array\.find/;
  function lazyDeopt(deopt) {
    const b = [1, 2, 3];
    let did_assert_error = false;
    let result = 0;
    b.find((v, i) => {
      result += v;
      if (i === 1) {
        %DeoptimizeNow();
      } else if (i === 2) {
        const e = new Error();
        assertTrue(re.exec(e.stack) !== null);
        did_assert_error = true;
      }
      return false;
    });
    return did_assert_error;
  }
  lazyDeopt();
  lazyDeopt();
  %OptimizeFunctionOnNextCall(lazyDeopt);
  assertTrue(lazyDeopt());
})();

// Verify that various exception edges are handled appropriately.
// The thrown Error object should always indicate it was created from
// a find call stack.
(() => {
  const re = /Array\.find/;
  const a = [1, 2, 3];
  let result = 0;
  function lazyDeopt() {
    a.find((v, i) => {
      result += i;
      if (i === 1) {
        %DeoptimizeFunction(lazyDeopt);
        throw new Error();
      }
      return false;
    });
  }
  assertThrows(() => lazyDeopt());
  assertThrows(() => lazyDeopt());
  try {
    lazyDeopt();
  } catch (e) {
    assertTrue(re.exec(e.stack) !== null);
  }
  %OptimizeFunctionOnNextCall(lazyDeopt);
  try {
    lazyDeopt();
  } catch (e) {
    assertTrue(re.exec(e.stack) !== null);
  }
})();

// Messing with the Array prototype causes deoptimization.
(() => {
  const a = [1, 2, 3];
  let result = 0;
  function prototypeChanged() {
    a.find((v, i) => {
      result += v;
      return false;
    });
  }
  prototypeChanged();
  prototypeChanged();
  %OptimizeFunctionOnNextCall(prototypeChanged);
  prototypeChanged();
  a.constructor = {};
  prototypeChanged();
  assertUnoptimized(prototypeChanged);
  assertEquals(24, result);
})();

// Verify holes are replaced with undefined.
(() => {
  const a = [1, 2, , 3, 4];
  function withHoles() {
    const callback_values = [];
    a.find(v => {
      callback_values.push(v);
      return false;
    });
    return callback_values;
  }
  withHoles();
  withHoles();
  %OptimizeFunctionOnNextCall(withHoles);
  assertArrayEquals([1, 2, undefined, 3, 4], withHoles());
})();
