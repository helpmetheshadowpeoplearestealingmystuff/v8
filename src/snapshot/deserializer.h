// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_DESERIALIZER_H_
#define V8_SNAPSHOT_DESERIALIZER_H_

#include "src/heap/heap.h"
#include "src/objects.h"
#include "src/snapshot/serializer-common.h"
#include "src/snapshot/snapshot-source-sink.h"

namespace v8 {
namespace internal {

class Heap;

// A Deserializer reads a snapshot and reconstructs the Object graph it defines.
class Deserializer : public SerializerDeserializer {
 public:
  // Create a deserializer from a snapshot byte source.
  template <class Data>
  explicit Deserializer(Data* data)
      : isolate_(NULL),
        source_(data->Payload()),
        magic_number_(data->GetMagicNumber()),
        external_reference_table_(NULL),
        deserialized_large_objects_(0),
        deserializing_user_code_(false),
        next_alignment_(kWordAligned) {
    DecodeReservation(data->Reservations());
  }

  ~Deserializer() override;

  // Deserialize the snapshot into an empty heap.
  void Deserialize(Isolate* isolate);

  // Deserialize a single object and the objects reachable from it.
  MaybeHandle<Object> DeserializePartial(Isolate* isolate,
                                         Handle<JSGlobalProxy> global_proxy);

  // Deserialize a shared function info. Fail gracefully.
  MaybeHandle<SharedFunctionInfo> DeserializeCode(Isolate* isolate);

  // Pass a vector of externally-provided objects referenced by the snapshot.
  // The ownership to its backing store is handed over as well.
  void SetAttachedObjects(Vector<Handle<Object> > attached_objects) {
    attached_objects_ = attached_objects;
  }

 private:
  void VisitPointers(Object** start, Object** end) override;

  void Synchronize(VisitorSynchronization::SyncTag tag) override;

  void VisitRuntimeEntry(RelocInfo* rinfo) override { UNREACHABLE(); }

  void Initialize(Isolate* isolate);

  bool deserializing_user_code() { return deserializing_user_code_; }

  void DecodeReservation(Vector<const SerializedData::Reservation> res);

  bool ReserveSpace();

  void UnalignedCopy(Object** dest, Object** src) {
    memcpy(dest, src, sizeof(*src));
  }

  void SetAlignment(byte data) {
    DCHECK_EQ(kWordAligned, next_alignment_);
    int alignment = data - (kAlignmentPrefix - 1);
    DCHECK_LE(kWordAligned, alignment);
    DCHECK_LE(alignment, kSimd128Unaligned);
    next_alignment_ = static_cast<AllocationAlignment>(alignment);
  }

  void DeserializeDeferredObjects();

  void FlushICacheForNewIsolate();
  void FlushICacheForNewCodeObjects();

  void CommitPostProcessedObjects(Isolate* isolate);

  // Fills in some heap data in an area from start to end (non-inclusive).  The
  // space id is used for the write barrier.  The object_address is the address
  // of the object we are writing into, or NULL if we are not writing into an
  // object, i.e. if we are writing a series of tagged values that are not on
  // the heap. Return false if the object content has been deferred.
  bool ReadData(Object** start, Object** end, int space,
                Address object_address);
  void ReadObject(int space_number, Object** write_back);
  Address Allocate(int space_index, int size);

  // Special handling for serialized code like hooking up internalized strings.
  HeapObject* PostProcessNewObject(HeapObject* obj, int space);

  // This returns the address of an object that has been described in the
  // snapshot by chunk index and offset.
  HeapObject* GetBackReferencedObject(int space);

  Object** CopyInNativesSource(Vector<const char> source_vector,
                               Object** current);

  // Cached current isolate.
  Isolate* isolate_;

  // Objects from the attached object descriptions in the serialized user code.
  Vector<Handle<Object> > attached_objects_;

  SnapshotByteSource source_;
  uint32_t magic_number_;

  // The address of the next object that will be allocated in each space.
  // Each space has a number of chunks reserved by the GC, with each chunk
  // fitting into a page. Deserialized objects are allocated into the
  // current chunk of the target space by bumping up high water mark.
  Heap::Reservation reservations_[kNumberOfSpaces];
  uint32_t current_chunk_[kNumberOfPreallocatedSpaces];
  Address high_water_[kNumberOfPreallocatedSpaces];

  ExternalReferenceTable* external_reference_table_;

  List<HeapObject*> deserialized_large_objects_;
  List<Code*> new_code_objects_;
  List<Handle<String> > new_internalized_strings_;
  List<Handle<Script> > new_scripts_;

  bool deserializing_user_code_;

  AllocationAlignment next_alignment_;

  DISALLOW_COPY_AND_ASSIGN(Deserializer);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_DESERIALIZER_H_
