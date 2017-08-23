// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The common functionality when building with or without snapshots.

#include "src/snapshot/snapshot.h"

#include "src/api.h"
#include "src/base/platform/platform.h"
#include "src/objects-inl.h"
#include "src/snapshot/partial-deserializer.h"
#include "src/snapshot/snapshot-source-sink.h"
#include "src/snapshot/startup-deserializer.h"
#include "src/version.h"

namespace v8 {
namespace internal {

#ifdef DEBUG
bool Snapshot::SnapshotIsValid(v8::StartupData* snapshot_blob) {
  return Snapshot::ExtractNumContexts(snapshot_blob) > 0;
}
#endif  // DEBUG

bool Snapshot::HasContextSnapshot(Isolate* isolate, size_t index) {
  // Do not use snapshots if the isolate is used to create snapshots.
  const v8::StartupData* blob = isolate->snapshot_blob();
  if (blob == nullptr) return false;
  if (blob->data == nullptr) return false;
  size_t num_contexts = static_cast<size_t>(ExtractNumContexts(blob));
  return index < num_contexts;
}

bool Snapshot::Initialize(Isolate* isolate) {
  if (!isolate->snapshot_available()) return false;
  base::ElapsedTimer timer;
  if (FLAG_profile_deserialization) timer.Start();

  const v8::StartupData* blob = isolate->snapshot_blob();
  Vector<const byte> startup_data = ExtractStartupData(blob);
  SnapshotData snapshot_data(startup_data);
  StartupDeserializer deserializer(&snapshot_data);
  deserializer.SetRehashability(ExtractRehashability(blob));
  bool success = isolate->Init(&deserializer);
  if (FLAG_profile_deserialization) {
    double ms = timer.Elapsed().InMillisecondsF();
    int bytes = startup_data.length();
    PrintF("[Deserializing isolate (%d bytes) took %0.3f ms]\n", bytes, ms);
  }
  return success;
}

MaybeHandle<Context> Snapshot::NewContextFromSnapshot(
    Isolate* isolate, Handle<JSGlobalProxy> global_proxy, size_t context_index,
    v8::DeserializeEmbedderFieldsCallback embedder_fields_deserializer) {
  if (!isolate->snapshot_available()) return Handle<Context>();
  base::ElapsedTimer timer;
  if (FLAG_profile_deserialization) timer.Start();

  const v8::StartupData* blob = isolate->snapshot_blob();
  bool can_rehash = ExtractRehashability(blob);
  Vector<const byte> context_data =
      ExtractContextData(blob, static_cast<int>(context_index));
  SnapshotData snapshot_data(context_data);

  MaybeHandle<Context> maybe_result = PartialDeserializer::DeserializeContext(
      isolate, &snapshot_data, can_rehash, global_proxy,
      embedder_fields_deserializer);

  Handle<Context> result;
  if (!maybe_result.ToHandle(&result)) return MaybeHandle<Context>();

  if (FLAG_profile_deserialization) {
    double ms = timer.Elapsed().InMillisecondsF();
    int bytes = context_data.length();
    PrintF("[Deserializing context #%zu (%d bytes) took %0.3f ms]\n",
           context_index, bytes, ms);
  }
  return result;
}

void ProfileDeserialization(
    const SnapshotData* startup_snapshot,
    const std::vector<SnapshotData*>& context_snapshots) {
  if (FLAG_profile_deserialization) {
    int startup_total = 0;
    PrintF("Deserialization will reserve:\n");
    for (const auto& reservation : startup_snapshot->Reservations()) {
      startup_total += reservation.chunk_size();
    }
    PrintF("%10d bytes per isolate\n", startup_total);
    for (size_t i = 0; i < context_snapshots.size(); i++) {
      int context_total = 0;
      for (const auto& reservation : context_snapshots[i]->Reservations()) {
        context_total += reservation.chunk_size();
      }
      PrintF("%10d bytes per context #%zu\n", context_total, i);
    }
  }
}

v8::StartupData Snapshot::CreateSnapshotBlob(
    const SnapshotData* startup_snapshot,
    const std::vector<SnapshotData*>& context_snapshots, bool can_be_rehashed) {
  int num_contexts = static_cast<int>(context_snapshots.size());
  int startup_snapshot_offset = StartupSnapshotOffset(num_contexts);
  int total_length = startup_snapshot_offset;
  total_length += startup_snapshot->RawData().length();
  for (const auto context_snapshot : context_snapshots) {
    total_length += context_snapshot->RawData().length();
  }

  ProfileDeserialization(startup_snapshot, context_snapshots);

  char* data = new char[total_length];
  memcpy(data + kNumberOfContextsOffset, &num_contexts, kInt32Size);
  int rehashability = can_be_rehashed ? 1 : 0;
  memcpy(data + kRehashabilityOffset, &rehashability, kInt32Size);
  int payload_offset = StartupSnapshotOffset(num_contexts);
  int payload_length = startup_snapshot->RawData().length();
  memcpy(data + payload_offset, startup_snapshot->RawData().start(),
         payload_length);
  if (FLAG_profile_deserialization) {
    PrintF("Snapshot blob consists of:\n%10d bytes for startup\n",
           payload_length);
  }
  payload_offset += payload_length;
  for (int i = 0; i < num_contexts; i++) {
    memcpy(data + ContextSnapshotOffsetOffset(i), &payload_offset, kInt32Size);
    SnapshotData* context_snapshot = context_snapshots[i];
    payload_length = context_snapshot->RawData().length();
    memcpy(data + payload_offset, context_snapshot->RawData().start(),
           payload_length);
    if (FLAG_profile_deserialization) {
      PrintF("%10d bytes for context #%d\n", payload_length, i);
    }
    payload_offset += payload_length;
  }

  v8::StartupData result = {data, total_length};
  return result;
}

int Snapshot::ExtractNumContexts(const v8::StartupData* data) {
  CHECK_LT(kNumberOfContextsOffset, data->raw_size);
  int num_contexts;
  memcpy(&num_contexts, data->data + kNumberOfContextsOffset, kInt32Size);
  return num_contexts;
}

bool Snapshot::ExtractRehashability(const v8::StartupData* data) {
  CHECK_LT(kRehashabilityOffset, data->raw_size);
  int rehashability;
  memcpy(&rehashability, data->data + kRehashabilityOffset, kInt32Size);
  return rehashability != 0;
}

Vector<const byte> Snapshot::ExtractStartupData(const v8::StartupData* data) {
  int num_contexts = ExtractNumContexts(data);
  int startup_offset = StartupSnapshotOffset(num_contexts);
  CHECK_LT(startup_offset, data->raw_size);
  int first_context_offset;
  memcpy(&first_context_offset, data->data + ContextSnapshotOffsetOffset(0),
         kInt32Size);
  CHECK_LT(first_context_offset, data->raw_size);
  int startup_length = first_context_offset - startup_offset;
  const byte* startup_data =
      reinterpret_cast<const byte*>(data->data + startup_offset);
  return Vector<const byte>(startup_data, startup_length);
}

Vector<const byte> Snapshot::ExtractContextData(const v8::StartupData* data,
                                                int index) {
  int num_contexts = ExtractNumContexts(data);
  CHECK_LT(index, num_contexts);

  int context_offset;
  memcpy(&context_offset, data->data + ContextSnapshotOffsetOffset(index),
         kInt32Size);
  int next_context_offset;
  if (index == num_contexts - 1) {
    next_context_offset = data->raw_size;
  } else {
    memcpy(&next_context_offset,
           data->data + ContextSnapshotOffsetOffset(index + 1), kInt32Size);
    CHECK_LT(next_context_offset, data->raw_size);
  }

  const byte* context_data =
      reinterpret_cast<const byte*>(data->data + context_offset);
  int context_length = next_context_offset - context_offset;
  return Vector<const byte>(context_data, context_length);
}

SnapshotData::SnapshotData(const Serializer* serializer) {
  DisallowHeapAllocation no_gc;
  std::vector<Reservation> reservations;
  serializer->EncodeReservations(&reservations);
  const std::vector<byte>* payload = serializer->sink()->data();

  // Calculate sizes.
  int reservation_size = static_cast<int>(reservations.size()) * kInt32Size;
  int size = kHeaderSize + reservation_size + static_cast<int>(payload->size());

  // Allocate backing store and create result data.
  AllocateData(size);

  // Set header values.
  SetMagicNumber(serializer->isolate());
  SetHeaderValue(kVersionHashOffset, Version::Hash());
  SetHeaderValue(kNumReservationsOffset, static_cast<int>(reservations.size()));
  SetHeaderValue(kPayloadLengthOffset, static_cast<int>(payload->size()));

  // Copy reservation chunk sizes.
  CopyBytes(data_ + kHeaderSize, reinterpret_cast<byte*>(reservations.data()),
            reservation_size);

  // Copy serialized data.
  CopyBytes(data_ + kHeaderSize + reservation_size, payload->data(),
            static_cast<size_t>(payload->size()));
}

bool SnapshotData::IsSane() {
  return GetHeaderValue(kVersionHashOffset) == Version::Hash();
}

Vector<const SerializedData::Reservation> SnapshotData::Reservations() const {
  return Vector<const Reservation>(
      reinterpret_cast<const Reservation*>(data_ + kHeaderSize),
      GetHeaderValue(kNumReservationsOffset));
}

Vector<const byte> SnapshotData::Payload() const {
  int reservations_size = GetHeaderValue(kNumReservationsOffset) * kInt32Size;
  const byte* payload = data_ + kHeaderSize + reservations_size;
  int length = GetHeaderValue(kPayloadLengthOffset);
  DCHECK_EQ(data_ + size_, payload + length);
  return Vector<const byte>(payload, length);
}

}  // namespace internal
}  // namespace v8
