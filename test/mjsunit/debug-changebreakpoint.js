// Copyright 2008 Google Inc. All Rights Reserved.
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

// Simple function which stores the last debug event.
listenerComplete = false;
exception = false;

var base_request = '"seq":0,"type":"request","command":"changebreakpoint"'

function safeEval(code) {
  try {
    return eval('(' + code + ')');
  } catch (e) {
    assertEquals(void 0, e);
    return undefined;
  }
}

function testArguments(dcp, arguments, success) {
  var request = '{' + base_request + ',"arguments":' + arguments + '}'
  var json_response = dcp.processDebugJSONRequest(request);
  var response = safeEval(json_response);
  if (success) {
    assertTrue(response.success, json_response);
  } else {
    assertFalse(response.success, json_response);
  }
}

function listener(event, exec_state, event_data, data) {
  try {
  if (event == Debug.DebugEvent.Break) {
    // Get the debug command processor.
    var dcp = exec_state.debugCommandProcessor();

    // Test some illegal clearbreakpoint requests.
    var request = '{' + base_request + '}'
    var response = safeEval(dcp.processDebugJSONRequest(request));
    assertFalse(response.success);

    testArguments(dcp, '{}', false);
    testArguments(dcp, '{"breakpoint":0,"condition":"false"}', false);
    // TODO(1241036) change this to 2 when break points have been restructured.
    testArguments(dcp, '{"breakpoint":3,"condition":"false"}', false);
    testArguments(dcp, '{"breakpoint":"xx","condition":"false"}', false);

    // Test some legal clearbreakpoint requests.
    testArguments(dcp, '{"breakpoint":1}', true);
    testArguments(dcp, '{"breakpoint":1,"enabled":"true"}', true);
    testArguments(dcp, '{"breakpoint":1,"enabled":"false"}', true);
    testArguments(dcp, '{"breakpoint":1,"condition":"1==2"}', true);
    testArguments(dcp, '{"breakpoint":1,"condition":"false"}', true);
    testArguments(dcp, '{"breakpoint":1,"ignoreCount":7}', true);
    testArguments(dcp, '{"breakpoint":1,"ignoreCount":0}', true);
    testArguments(
        dcp,
        '{"breakpoint":1,"enabled":"true","condition":"false","ignoreCount":0}',
        true);

    // Indicate that all was processed.
    listenerComplete = true;
  }
  } catch (e) {
    exception = e
  };
};

// Add the debug event listener.
Debug.addListener(listener);

function g() {};

// Set a break point and call to invoke the debug event listener.
bp = Debug.setBreakPoint(g, 0, 0);
assertEquals(1, bp);
g();

// Make sure that the debug event listener vas invoked.
assertTrue(listenerComplete, "listener did not run to completion");
assertFalse(exception, "exception in listener")
