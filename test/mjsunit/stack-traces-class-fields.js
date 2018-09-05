// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-class-fields

// Utility function for testing that the expected strings occur
// in the stack trace produced when running the given function.
function testTrace(name, fun, expected, unexpected) {
  var threw = false;
  try {
    fun();
  } catch (e) {
    for (var i = 0; i < expected.length; i++) {
      assertTrue(
        e.stack.indexOf(expected[i]) != -1,
        name + " doesn't contain expected[" + i + "] stack = " + e.stack
      );
    }
    if (unexpected) {
      for (var i = 0; i < unexpected.length; i++) {
        assertEquals(
          e.stack.indexOf(unexpected[i]),
          -1,
          name + " contains unexpected[" + i + "]"
        );
      }
    }
    threw = true;
  }
  assertTrue(threw, name + " didn't throw");
}

function thrower() {
  FAIL;
}

function testClassConstruction() {
  class X {
    static x = thrower();
  }
}

// ReferenceError: FAIL is not defined
//     at thrower
//     at Function.<anonymous>
//     at testClassConstruction
//     at testTrace
testTrace("during class construction", testClassConstruction, [
  "thrower",
  "Function.<anonymous>"
]);

function testClassConstruction2() {
  class X {
    [thrower()];
  }
}

// ReferenceError: FAIL is not defined
//    at thrower
//    at testClassConstruction2
//    at testTrace
testTrace("during class construction2", testClassConstruction2, [
  "thrower"
]);

function testClassInstantiation() {
  class X {
    x = thrower();
  }

  new X();
}

// ReferenceError: FAIL is not defined
//     at thrower
//     at X.<anonymous>
//     at new X
//     at testClassInstantiation
//     at testTrace
testTrace("during class instantiation", testClassInstantiation, [
  "thrower",
  "X.<anonymous>",
  "new X"
]);

function testClassInstantiationWithSuper() {
  class Base {}

  class X extends Base {
    x = thrower();
  }

  new X();
}

// ReferenceError: FAIL is not defined
//     at thrower
//     at X.<anonymous>
//     at new X
//     at testClassInstantiation
//     at testTrace
testTrace(
  "during class instantiation with super",
  testClassInstantiationWithSuper,
  ["thrower", "X.<anonymous>", "new X"],
  ["Base"]
);

function testClassInstantiationWithSuper2() {
  class Base {}

  class X extends Base {
    constructor() {
      super();
    }
    x = thrower();
  }

  new X();
}

// ReferenceError: FAIL is not defined
//     at thrower
//     at X.<anonymous>
//     at new X
//     at testClassInstantiation
//     at testTrace
testTrace(
  "during class instantiation with super2",
  testClassInstantiationWithSuper2,
  ["thrower", "X.<anonymous>", "new X"],
  ["Base"]
);

function testClassInstantiationWithSuper3() {
  class Base {
    x = thrower();
  }

  class X extends Base {
    constructor() {
      super();
    }
  }

  new X();
}

// ReferenceError: FAIL is not defined
//     at thrower
//     at X.<anonymous>
//     at new Base
//     at new X
//     at testClassInstantiationWithSuper3
//     at testTrace
testTrace(
  "during class instantiation with super3",
  testClassInstantiationWithSuper3,
  ["thrower", "X.<anonymous>", "new Base", "new X"]
);

function testClassFieldCall() {
  class X {
    x = thrower;
  }

  let x = new X();
  x.x();
}

// ReferenceError: FAIL is not defined
//     at X.thrower [as x]
//     at testClassFieldCall
//     at testTrace
testTrace(
  "during class field call",
  testClassFieldCall,
  ["X.thrower"],
  ["anonymous"]
);

function testStaticClassFieldCall() {
  class X {
    static x = thrower;
  }

  X.x();
}

// ReferenceError: FAIL is not defined
//     at Function.thrower [as x]
//     at testStaticClassFieldCall
//     at testTrace
testTrace(
  "during static class field call",
  testStaticClassFieldCall,
  ["Function.thrower"],
  ["anonymous"]
);

function testClassFieldCallWithFNI() {
  class X {
    x = function() {
      FAIL;
    };
  }

  let x = new X();
  x.x();
}

// ReferenceError: FAIL is not defined
//     at X.x
//     at testClassFieldCallWithFNI
//     at testTrace
testTrace(
  "during class field call with FNI",
  testClassFieldCallWithFNI,
  ["X.x"],
  ["anonymous"]
);

function testStaticClassFieldCallWithFNI() {
  class X {
    static x = function() {
      FAIL;
    };
  }

  X.x();
}

// ReferenceError: FAIL is not defined
//     at Function.x
//     at testStaticClassFieldCallWithFNI
//     at testTrace
testTrace(
  "during static class field call with FNI",
  testStaticClassFieldCallWithFNI,
  ["Function.x"],
  ["anonymous"]
);
