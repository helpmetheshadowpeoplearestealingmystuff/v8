// Copyright 2010 the V8 project authors. All rights reserved.
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

var exception = null;  // Exception in debug event listener.

function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.AfterCompile) {
      assertEquals(Debug.ScriptCompilationType.Eval,
                   event_data.script().compilationType(),
                  'Wrong compilationType');
      var evalFromScript = event_data.script().evalFromScript();
      assertTrue(!!evalFromScript, ' evalFromScript ');
      assertFalse(evalFromScript.isUndefined(), 'evalFromScript.isUndefined()');
      assertTrue(/debug-compile-event-newfunction.js$/.test(
                     evalFromScript.name()),
                 'Wrong eval from script name.');

      var evalFromLocation = event_data.script().evalFromLocation();
      assertTrue(!!evalFromLocation, 'evalFromLocation is undefined');
      assertEquals(63, evalFromLocation.line);

      // Check that the event can be serialized without exceptions.
      var json = event_data.toJSONProtocol();
    }
  } catch (e) {
    exception = e
  }
};


// Add the debug event listener.
Debug.setListener(listener);

// Create a function from its body text. It will lead to an eval.
var f = new Function('arg1', 'return arg1 + 1;');
// TODO(titzer): Assignment only needed because source positions are borked.

assertNull(exception, "exception in listener");

Debug.setListener(null);
