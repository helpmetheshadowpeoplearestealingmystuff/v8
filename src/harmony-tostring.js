// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

// This file relies on the fact that the following declaration has been made
// in runtime.js and symbol.js:
// var $Object = global.Object;

DefaultObjectToString = ObjectToStringHarmony;
// ES6 draft 08-24-14, section 19.1.3.6
function ObjectToStringHarmony() {
  if (IS_UNDEFINED(this) && !IS_UNDETECTABLE(this)) return "[object Undefined]";
  if (IS_NULL(this)) return "[object Null]";
  var O = ToObject(this);
  var builtinTag = %_ClassOf(O);
  var tag = O[symbolToStringTag];
  if (!IS_STRING(tag)) {
    tag = builtinTag;
  }
  return "[object " + tag + "]";
}

function HarmonyToStringExtendSymbolPrototype() {
  %CheckIsBootstrapping();

  InstallConstants(global.Symbol, $Array(
    // TODO(dslomov, caitp): Move to symbol.js when shipping
   "toStringTag", symbolToStringTag
  ));
}

HarmonyToStringExtendSymbolPrototype();
