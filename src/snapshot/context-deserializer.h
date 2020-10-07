// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_CONTEXT_DESERIALIZER_H_
#define V8_SNAPSHOT_CONTEXT_DESERIALIZER_H_

#include "src/snapshot/deserializer.h"
#include "src/snapshot/snapshot-data.h"
#include "src/snapshot/snapshot.h"

namespace v8 {
namespace internal {

class Context;
class Isolate;

// Deserializes the context-dependent object graph rooted at a given object.
// The ContextDeserializer is not expected to deserialize any code objects.
class V8_EXPORT_PRIVATE ContextDeserializer final : public Deserializer {
 public:
  static MaybeHandle<Context> DeserializeContext(
      Isolate* isolate, const SnapshotData* data, bool can_rehash,
      Handle<JSGlobalProxy> global_proxy,
      v8::DeserializeEmbedderFieldsCallback embedder_fields_deserializer);

 private:
  explicit ContextDeserializer(Isolate* isolate, const SnapshotData* data,
                               bool can_rehash)
      : Deserializer(isolate, data->Payload(), data->GetMagicNumber(), false,
                     can_rehash) {}

  // Deserialize a single object and the objects reachable from it.
  MaybeHandle<Object> Deserialize(
      Isolate* isolate, Handle<JSGlobalProxy> global_proxy,
      v8::DeserializeEmbedderFieldsCallback embedder_fields_deserializer);

  void DeserializeEmbedderFields(
      v8::DeserializeEmbedderFieldsCallback embedder_fields_deserializer);

  void SetupOffHeapArrayBufferBackingStores();
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_CONTEXT_DESERIALIZER_H_
