// Copyright 2009 the V8 project authors. All rights reserved.
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

// Tests loading of properties across eval calls.

var x = 1;
function global_function() { return 'global'; }
const const_uninitialized;
const const_initialized = function() { return "const_global"; }

// Test loading across an eval call that does not shadow variables.
function testNoShadowing() {
  var y = 2;
  function local_function() { return 'local'; }
  const local_const_uninitialized;
  const local_const_initialized = function() { return "const_local"; }
  function f() {
    eval('1');
    assertEquals(1, x);
    try { typeof(asdf); } catch(e) { assertUnreachable(); }
    assertEquals(2, y);
    assertEquals('global', global_function());
    assertEquals('local', local_function());
    try {
      const_uninitialized();
      assertUnreachable();
    } catch(e) {
      // Ignore.
    }
    assertEquals('const_global', const_initialized());
    try {
      local_const_uninitialized();
      assertUnreachable();
    } catch(e) {
      // Ignore.
    }
    assertEquals('const_local', local_const_initialized());
    function g() {
      assertEquals(1, x);
      try { typeof(asdf); } catch(e) { assertUnreachable(); }
      assertEquals(2, y);
      assertEquals('global', global_function());
      assertEquals('local', local_function());
      try {
        const_uninitialized();
        assertUnreachable();
      } catch(e) {
        // Ignore.
      }
      assertEquals('const_global', const_initialized());
      try {
        local_const_uninitialized();
        assertUnreachable();
      } catch(e) {
        // Ignore.
      }
      assertEquals('const_local', local_const_initialized());
    }
    g();
  }
  f();
}

testNoShadowing();

// Test loading across eval calls that do not shadow variables.
function testNoShadowing2() {
  var y = 2;
  function local_function() { return 'local'; }
  eval('1');
  function f() {
    eval('1');
    assertEquals(1, x);
    assertEquals(2, y);
    assertEquals('global', global_function());
    assertEquals('local', local_function());
    function g() {
      assertEquals(1, x);
      assertEquals(2, y);
      assertEquals('global', global_function());
      assertEquals('local', local_function());
    }
    g();
  }
  f();
}

testNoShadowing2();

// Test loading across an eval call that shadows variables.
function testShadowing() {
  var y = 2;
  function local_function() { return 'local'; }
  function f() {
    eval('var x = 3; var y = 4;');
    assertEquals(3, x);
    assertEquals(4, y);
    eval('function local_function() { return "new_local"; }');
    eval('function global_function() { return "new_nonglobal"; }');
    assertEquals('new_nonglobal', global_function());
    assertEquals('new_local', local_function());
    function g() {
      assertEquals(3, x);
      assertEquals(4, y);
      assertEquals('new_nonglobal', global_function());
      assertEquals('new_local', local_function());
    }
    g();
  }
  f();
}

testShadowing();
