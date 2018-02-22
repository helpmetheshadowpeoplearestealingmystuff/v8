// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Extensions to mjsunit and other test harnesses added between harness and
// fuzzing code.

try {
  // Scope for utility functions.
  (function() {
    // Same as in mjsunit.js.
    function classOf(object) {
      // Argument must not be null or undefined.
      var string = Object.prototype.toString.call(object);
      // String has format [object <ClassName>].
      return string.substring(8, string.length - 1);
    }

    // Override prettyPrinted with a version that also recusively prints object
    // properties (with a depth of 3).
    let origPrettyPrinted = this.prettyPrinted;
    this.prettyPrinted = function prettyPrinted(value, depth=3) {
      if (depth == 0) {
        return "...";
      }
      switch (typeof value) {
        case "object":
          if (value === null) return "null";
          var objectClass = classOf(value);
          switch (objectClass) {
            case "Object":
              var name = value.constructor.name;
              if (!name)
                name = "Object";
              return name + "{" + Object.keys(value).map(function(key, index) {
                return (
                    prettyPrinted(key, depth - 1) +
                    ": " +
                    prettyPrinted(value[key], depth - 1)
                );
              }).join(",")  + "}";
          }
      }
      // Fall through to original version for all other types.
      return origPrettyPrinted(value);
    }
  })();
} catch(e) { }
