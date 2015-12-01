// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-proxies

(function testBasicFunctionality() {
   var target = {
     target_one: 1,
     property: "value"
   };

   var handler = {handler:1};

   var proxy = new Proxy(target, handler);
   assertEquals("value", proxy.property);
   assertEquals(undefined, proxy.nothing);
   assertEquals(undefined, proxy.handler);

   handler.get = function() { return "value 2" };
   assertEquals("value 2", proxy.property);
   assertEquals("value 2", proxy.nothing);
   assertEquals("value 2", proxy.handler);

   var handler2 = new Proxy({get: function() { return "value 3" }},{});
   var proxy2 = new Proxy(target, handler2);
   assertEquals("value 3", proxy2.property);
   assertEquals("value 3", proxy2.nothing);
   assertEquals("value 3", proxy2.handler);
})();

(function testThrowOnGettingTrap() {
  var handler = new Proxy({}, {get: function(){ throw Error() }});
  var proxy = new Proxy({}, handler);
  assertThrows("proxy.property", Error);
})();

(function testFallback() {
  var target = {property:"value"};
  var proxy = new Proxy(target, {});
  assertEquals("value", proxy.property);
  assertEquals(undefined, proxy.property2);
})();

(function testFallbackUndefinedTrap() {
  var handler = new Proxy({}, {get: function(){ return undefined }});
  var target = {property:"value"};
  var proxy = new Proxy(target, handler);
  assertEquals("value", proxy.property);
  assertEquals(undefined, proxy.property2);
})();

(function testFailingInvariant() {
  var target = {};
  var handler = { get: function(){ return "value" }}
  var proxy = new Proxy(target, handler);
  assertEquals("value", proxy.property);
  assertEquals("value", proxy.key);
  assertEquals("value", proxy.key2);
  assertEquals("value", proxy.key3);

  // Define a non-configurable, non-writeable property on the target for
  // which the handler will return a different value.
  Object.defineProperty(target, "key", {
    configurable: false,
    writable: false,
    value: "different value"
  });
  assertEquals("value", proxy.property);
  assertThrows(function(){ proxy.key }, TypeError);
  assertEquals("value", proxy.key2);
  assertEquals("value", proxy.key3);

  // Define a non-configurable getter on the target for which the handler
  // will return a value, according to the spec we do not throw.
  Object.defineProperty(target, "key2", {
    configurable: false,
    get: function() { return "different value" }
  });
  assertEquals("value", proxy.property);
  assertThrows(function(){ proxy.key }, TypeError);
  assertEquals("value", proxy.key2);
  assertEquals("value", proxy.key3);

  // Define a non-configurable setter without a corresponding getter on the
  // target for which the handler will return a value.
  Object.defineProperty(target, "key3", {
    configurable: false,
    set: function() { }
  });
  assertEquals("value", proxy.property);
  assertThrows(function(){ proxy.key }, TypeError);
  assertEquals("value", proxy.key2);
  assertThrows(function(){ proxy.key3 }, TypeError);
})();
