// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_BUILTIN_SERIALIZER_H_
#define V8_SNAPSHOT_BUILTIN_SERIALIZER_H_

#include "src/snapshot/serializer.h"

namespace v8 {
namespace internal {

class StartupSerializer;

// Responsible for serializing all builtin objects during startup snapshot
// creation.  Builtins are serialized into a dedicated area of the snapshot.
// See snapshot.h for documentation of the snapshot layout.
class BuiltinSerializer : public Serializer {
 public:
  BuiltinSerializer(Isolate* isolate, StartupSerializer* startup_serializer);
  ~BuiltinSerializer() override;

  void SerializeBuiltins();

 private:
  void VisitRootPointers(Root root, Object** start, Object** end) override;

  void SerializeBuiltin(Code* code);
  void SerializeObject(HeapObject* o, HowToCode how_to_code,
                       WhereToPoint where_to_point, int skip) override;

  StartupSerializer* startup_serializer_;

  DISALLOW_COPY_AND_ASSIGN(BuiltinSerializer);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_BUILTIN_SERIALIZER_H_
