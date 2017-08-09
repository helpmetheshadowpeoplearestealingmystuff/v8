// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Proxy get trap doesn't fail when the value returned by it
// is a number.

function __getProperties(obj, type) {
  if(typeof obj === "undefined" || obj === null) {
    return [];
  }

  let properties = [];
  for(let name of Object.getOwnPropertyNames(obj)) {
    properties.push(name);
  }
  return properties;
}

var number = 1;

(function testFailingInvariant() {
  var obj = {};
  var handler = {
    get: function() {}
  };
  var proxy = new Proxy(obj, handler);
  Object.defineProperty(handler, 'get', {
    get: function() {
      return number;
    }
  });
  assertThrows(function(){ proxy.property; }, TypeError);
})();
