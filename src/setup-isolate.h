// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace v8 {
namespace internal {

class Builtins;
class Code;
class Isolate;

namespace interpreter {
class Interpreter;
}  // namespace interpreter

// This class is an abstraction layer around initialization of components
// that are either deserialized from the snapshot or generated from scratch.
// Currently this includes builtins and interpreter bytecode handlers.
// There are two implementations to choose from at link time:
// - setup-isolate-deserialize.cc: always loads things from snapshot.
// - setup-isolate-full.cc: loads from snapshot or bootstraps from scratch,
//                          controlled by the |create_heap_objects| flag.
// For testing, the implementation in setup-isolate-for-tests.cc can be chosen
// to force the behavior of setup-isolate-full.cc at runtime.
//
// The actual implementations of generation of builtins and handlers is in
// setup-builtins-internal.cc and setup-interpreter-internal.cc, and is
// linked in by the latter two Delegate implementations.
class SetupIsolateDelegate {
 public:
  SetupIsolateDelegate() {}
  virtual ~SetupIsolateDelegate() {}

  virtual void SetupBuiltins(Isolate* isolate, bool create_heap_objects);

  virtual void SetupInterpreter(interpreter::Interpreter* interpreter,
                                bool create_heap_objects);

 protected:
  static void SetupBuiltinsInternal(Isolate* isolate);
  static void AddBuiltin(Builtins* builtins, int index, Code* code);
  static void PopulateWithPlaceholders(Isolate* isolate);
  static void ReplacePlaceholders(Isolate* isolate);
};

}  // namespace internal
}  // namespace v8
