// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_AST_MODULES_H_
#define V8_AST_MODULES_H_

#include "src/parsing/scanner.h"  // Only for Scanner::Location.
#include "src/pending-compilation-error-handler.h"
#include "src/zone-containers.h"

namespace v8 {
namespace internal {


class AstRawString;


class ModuleDescriptor : public ZoneObject {
 public:
  explicit ModuleDescriptor(Zone* zone)
      : special_exports_(1, zone),
        special_imports_(1, zone),
        regular_exports_(zone),
        regular_imports_(zone) {}

  // import x from "foo.js";
  // import {x} from "foo.js";
  // import {x as y} from "foo.js";
  void AddImport(
    const AstRawString* import_name, const AstRawString* local_name,
    const AstRawString* module_request, const Scanner::Location loc,
    Zone* zone);

  // import * as x from "foo.js";
  void AddStarImport(
    const AstRawString* local_name, const AstRawString* module_request,
    const Scanner::Location loc, Zone* zone);

  // import "foo.js";
  // import {} from "foo.js";
  // export {} from "foo.js";  (sic!)
  void AddEmptyImport(
      const AstRawString* module_request, const Scanner::Location loc,
      Zone* zone);

  // export {x};
  // export {x as y};
  // export VariableStatement
  // export Declaration
  // export default ...
  void AddExport(
    const AstRawString* local_name, const AstRawString* export_name,
    const Scanner::Location loc, Zone* zone);

  // export {x} from "foo.js";
  // export {x as y} from "foo.js";
  void AddExport(
    const AstRawString* export_name, const AstRawString* import_name,
    const AstRawString* module_request, const Scanner::Location loc,
    Zone* zone);

  // export * from "foo.js";
  void AddStarExport(
    const AstRawString* module_request, const Scanner::Location loc,
    Zone* zone);

  // Check if module is well-formed and report error if not.
  // Also canonicalize indirect exports.
  bool Validate(ModuleScope* module_scope,
                PendingCompilationErrorHandler* error_handler, Zone* zone);

  struct Entry : public ZoneObject {
    const Scanner::Location location;
    const AstRawString* export_name;
    const AstRawString* local_name;
    const AstRawString* import_name;
    const AstRawString* module_request;

    explicit Entry(Scanner::Location loc)
        : location(loc),
          export_name(nullptr),
          local_name(nullptr),
          import_name(nullptr),
          module_request(nullptr) {}
  };

  // Empty imports and namespace imports.
  const ZoneList<const Entry*>& special_imports() const {
    return special_imports_;
  }

  // All the remaining imports, indexed by local name.
  const ZoneMap<const AstRawString*, const Entry*>& regular_imports() const {
    return regular_imports_;
  }

  // Star exports and explicitly indirect exports.
  const ZoneList<const Entry*>& special_exports() const {
    return special_exports_;
  }

  // All the remaining exports, indexed by local name.
  const ZoneMultimap<const AstRawString*, Entry*>& regular_exports() const {
    return regular_exports_;
  }

 private:
  // TODO(neis): Use STL datastructure instead of ZoneList?
  ZoneList<const Entry*> special_exports_;
  ZoneList<const Entry*> special_imports_;
  ZoneMultimap<const AstRawString*, Entry*> regular_exports_;
  ZoneMap<const AstRawString*, const Entry*> regular_imports_;

  // If there are multiple export entries with the same export name, return one
  // of them.  Otherwise return nullptr.
  const Entry* FindDuplicateExport(Zone* zone) const;

  // Find any implicitly indirect exports and make them explicit.
  //
  // An explicitly indirect export is an export entry arising from an export
  // statement of the following form:
  //   export {a as c} from "X";
  // An implicitly indirect export corresponds to
  //   export {b as c};
  // in the presence of an import statement of the form
  //   import {a as b} from "X";
  // This function finds such implicitly indirect export entries and rewrites
  // them by filling in the import name and module request, as well as nulling
  // out the local name.  Effectively, it turns
  //   import {a as b} from "X"; export {b as c};
  // into:
  //   import {a as b} from "X"; export {a as c} from "X";
  // (The import entry is never deleted.)
  void MakeIndirectExportsExplicit(Zone* zone);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_AST_MODULES_H_
