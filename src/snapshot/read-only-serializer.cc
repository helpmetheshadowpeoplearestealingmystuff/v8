// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/read-only-serializer.h"

#include "src/api/api.h"
#include "src/diagnostics/code-tracer.h"
#include "src/execution/v8threads.h"
#include "src/handles/global-handles.h"
#include "src/heap/read-only-heap.h"
#include "src/objects/objects-inl.h"
#include "src/objects/slots.h"
#include "src/snapshot/startup-serializer.h"

namespace v8 {
namespace internal {

ReadOnlySerializer::ReadOnlySerializer(Isolate* isolate,
                                       Snapshot::SerializerFlags flags)
    : RootsSerializer(isolate, flags, RootIndex::kFirstReadOnlyRoot) {
  STATIC_ASSERT(RootIndex::kFirstReadOnlyRoot == RootIndex::kFirstRoot);
  allocator()->UseCustomChunkSize(FLAG_serialization_chunk_size);
}

ReadOnlySerializer::~ReadOnlySerializer() {
  OutputStatistics("ReadOnlySerializer");
}

void ReadOnlySerializer::SerializeObject(HeapObject obj) {
  CHECK(ReadOnlyHeap::Contains(obj));
  CHECK_IMPLIES(obj.IsString(), obj.IsInternalizedString());

  if (SerializeHotObject(obj)) return;
  if (IsRootAndHasBeenSerialized(obj) && SerializeRoot(obj)) {
    return;
  }
  if (SerializeBackReference(obj)) return;

  CheckRehashability(obj);

  // Object has not yet been serialized.  Serialize it here.
  ObjectSerializer object_serializer(this, obj, &sink_);
  object_serializer.Serialize();
#ifdef DEBUG
  serialized_objects_.insert(obj);
#endif
}

void ReadOnlySerializer::SerializeReadOnlyRoots() {
  // No active threads.
  CHECK_NULL(isolate()->thread_manager()->FirstThreadStateInUse());
  // No active or weak handles.
  CHECK_IMPLIES(!allow_active_isolate_for_testing(),
                isolate()->handle_scope_implementer()->blocks()->empty());

  ReadOnlyRoots(isolate()).Iterate(this);
}

void ReadOnlySerializer::FinalizeSerialization() {
  // This comes right after serialization of the other snapshots, where we
  // add entries to the read-only object cache. Add one entry with 'undefined'
  // to terminate the read-only object cache.
  Object undefined = ReadOnlyRoots(isolate()).undefined_value();
  VisitRootPointer(Root::kReadOnlyObjectCache, nullptr,
                   FullObjectSlot(&undefined));
  SerializeDeferredObjects();
  Pad();

#ifdef DEBUG
  // Check that every object on read-only heap is reachable (and was
  // serialized).
  ReadOnlyHeapObjectIterator iterator(isolate()->read_only_heap());
  for (HeapObject object = iterator.Next(); !object.is_null();
       object = iterator.Next()) {
    CHECK(serialized_objects_.count(object));
  }
#endif
}

bool ReadOnlySerializer::MustBeDeferred(HeapObject object) {
  if (root_has_been_serialized(RootIndex::kFreeSpaceMap) &&
      root_has_been_serialized(RootIndex::kOnePointerFillerMap) &&
      root_has_been_serialized(RootIndex::kTwoPointerFillerMap)) {
    // All required root objects are serialized, so any aligned objects can
    // be saved without problems.
    return false;
  }
  // Defer objects with special alignment requirements until the filler roots
  // are serialized.
  return HeapObject::RequiredAlignment(object.map()) != kWordAligned;
}

bool ReadOnlySerializer::SerializeUsingReadOnlyObjectCache(
    SnapshotByteSink* sink, HeapObject obj) {
  if (!ReadOnlyHeap::Contains(obj)) return false;

  // Get the cache index and serialize it into the read-only snapshot if
  // necessary.
  int cache_index = SerializeInObjectCache(obj);

  // Writing out the cache entry into the calling serializer's sink.
  sink->Put(kReadOnlyObjectCache, "ReadOnlyObjectCache");
  sink->PutInt(cache_index, "read_only_object_cache_index");

  return true;
}

}  // namespace internal
}  // namespace v8
