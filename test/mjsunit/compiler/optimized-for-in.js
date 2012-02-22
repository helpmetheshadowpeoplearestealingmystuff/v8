// Copyright 2012 the V8 project authors. All rights reserved.
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

// Flags: --allow-natives-syntax

// Test for-in support in Crankshaft.  For simplicity this tests assumes certain
// fixed iteration order for properties and will have to be adjusted if V8
// stops following insertion order.


function a(t) {
  var result = [];
  for (var i in t) {
    result.push(i + t[i]);
  }
  return result.join('');
}

// Check that we correctly deoptimize on map check.
function b(t) {
  var result = [];
  for (var i in t) {
    result.push(i + t[i]);
    delete t[i];
  }
  return result.join('');
}

// Check that we correctly deoptimize during preparation step.
function c(t) {
  var result = [];
  for (var i in t) {
    result.push(i + t[i]);
  }
  return result.join('');
}

// Check that we deoptimize to the place after side effect in the right state.
function d(t) {
  var result = [];
  var o;
  for (var i in (o = t())) {
    result.push(i + o[i]);
  }
  return result.join('');
}

// Check that we correctly deoptimize on map check inserted for fused load.
function e(t) {
  var result = [];
  for (var i in t) {
    delete t[i];
    t[i] = i;
    result.push(i + t[i]);
  }
  return result.join('');
}

// Nested for-in loops.
function f(t) {
  var result = [];
  for (var i in t) {
    for (var j in t) {
      result.push(i + j + t[i] + t[j]);
    }
  }
  return result.join('');
}

// Deoptimization from the inner for-in loop.
function g(t) {
  var result = [];
  for (var i in t) {
    for (var j in t) {
      result.push(i + j + t[i] + t[j]);
      var v = t[i];
      delete t[i];
      t[i] = v;
    }
  }
  return result.join('');
}


// Break from the inner for-in loop.
function h(t, deopt) {
  var result = [];
  for (var i in t) {
    for (var j in t) {
      result.push(i + j + t[i] + t[j]);
      break;
    }
  }
  deopt.deopt;
  return result.join('');
}

// Continue in the inner loop.
function j(t, deopt) {
  var result = [];
  for (var i in t) {
    for (var j in t) {
      result.push(i + j + t[i] + t[j]);
      continue;
    }
  }
  deopt.deopt;
  return result.join('');
}

// Continue of the outer loop.
function k(t, deopt) {
  var result = [];
  outer: for (var i in t) {
    for (var j in t) {
      result.push(i + j + t[i] + t[j]);
      continue outer;
    }
  }
  deopt.deopt;
  return result.join('');
}

// Break of the outer loop.
function l(t, deopt) {
  var result = [];
  outer: for (var i in t) {
    for (var j in t) {
      result.push(i + j + t[i] + t[j]);
      break outer;
    }
  }
  deopt.deopt;
  return result.join('');
}

// Test deoptimization from inlined frame (currently it is not inlined).
function m0(t, deopt) {
  for (var i in t) {
    for (var j in t) {
      deopt.deopt;
      return i + j + t[i] + t[j];
    }
  }
}

function m(t, deopt) {
  return m0(t, deopt);
}


function tryFunction(s, mkT, f) {
  var d = {deopt: false};
  assertEquals(s, f(mkT(), d));
  assertEquals(s, f(mkT(), d));
  assertEquals(s, f(mkT(), d));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(s, f(mkT(), d));
  assertEquals(s, f(mkT(), {}));
}

var s = "a1b2c3d4";
function mkTable() { return { a: "1", b: "2", c: "3", d: "4" }; }


tryFunction(s, mkTable, a);
tryFunction(s, mkTable, b);
tryFunction("0a1b2c3d", function () { return "abcd"; }, c);
tryFunction("0a1b2c3d", function () {
  var cnt = false;
  return function () {
    cnt = true;
    return "abcd";
  }
}, d);
tryFunction("aabbccdd", mkTable, e);

function mkSmallTable() { return { a: "1", b: "2" }; }

tryFunction("aa11ab12ba21bb22", mkSmallTable, f);
tryFunction("aa11ab12bb22ba21", mkSmallTable, g);
tryFunction("aa11ba21", mkSmallTable, h);
tryFunction("aa11ab12ba21bb22", mkSmallTable, j);
tryFunction("aa11ba21", mkSmallTable, h);
tryFunction("aa11ba21", mkSmallTable, k);
tryFunction("aa11", mkSmallTable, l);
tryFunction("aa11", mkSmallTable, m);

// Test handling of null.
tryFunction("", function () {
  return function () { return null; }
}, function (t) {
  for (var i in t()) { return i; }
  return "";
});

// Test smis.
tryFunction("", function () {
  return function () { return 11; }
}, function (t) {
  for (var i in t()) { return i; }
  return "";
});

// Test LoadFieldByIndex for out of object properties.
function O() { this.a = 1; }
for (var i = 0; i < 10; i++) new O();
tryFunction("a1b2c3d4e5f6", function () {
  var o = new O();
  o.b = 2;
  o.c = 3;
  o.d = 4;
  o.e = 5;
  o.f = 6;
  return o;
}, function (t) {
  var r = [];
  for (var i in t) r.push(i + t[i]);
  return r.join('');
});
