// Copyright 2008 the V8 project authors. All rights reserved.
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

// Flags: --expose-debug-as debug
// Get the Debug object exposed from the debug context global object.
Debug = debug.Debug

// Simple function which collects a simple call graph.
var call_graph = "";
function listener(event, exec_state, event_data, data) {
  if (event == Debug.DebugEvent.Break)
  {
    call_graph += exec_state.frame().func().name();
    exec_state.prepareStep();
  }
};

// Add the debug event listener.
Debug.addListener(listener);

// Test debug event for constructor.
function a() {
  new c();
}

function b() {
  x = 1;
  new c();
}

function c() {
  this.x = 1;
  d();
}

function d() {
}

// Break point stops on "new c()" and steps into c.
Debug.setBreakPoint(a, 1);
call_graph = "";
a();
Debug.clearStepping();  // Clear stepping as the listener leaves it on.
assertEquals("accdca", call_graph);

// Break point stops on "x = 1" and steps to "new c()" and then into c.
Debug.setBreakPoint(b, 1);
call_graph = "";
b();
Debug.clearStepping();  // Clear stepping as the listener leaves it on.
assertEquals("bbccdcb", call_graph);

// Get rid of the debug event listener.
Debug.removeListener(listener);