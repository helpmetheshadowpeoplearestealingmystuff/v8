// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/heap-write-barrier.h"

#include "src/heap/heap-write-barrier-inl.h"
#include "src/heap/marking-barrier.h"
#include "src/objects/descriptor-array.h"
#include "src/objects/maybe-object.h"
#include "src/objects/slots-inl.h"
#include "src/objects/slots.h"

namespace v8 {
namespace internal {

void WriteBarrier::MarkingSlow(Heap* heap, HeapObject host, HeapObjectSlot slot,
                               HeapObject value) {
  heap->marking_barrier()->Write(host, slot, value);
}

void WriteBarrier::MarkingSlow(Heap* heap, Code host, RelocInfo* reloc_info,
                               HeapObject value) {
  heap->marking_barrier()->Write(host, reloc_info, value);
}

void WriteBarrier::MarkingSlow(Heap* heap, JSArrayBuffer host,
                               ArrayBufferExtension* extension) {
  heap->marking_barrier()->Write(host, extension);
}

void WriteBarrier::MarkingSlow(Heap* heap, Map host,
                               DescriptorArray descriptor_array,
                               int number_of_own_descriptors) {
  heap->marking_barrier()->Write(host, descriptor_array,
                                 number_of_own_descriptors);
}

int WriteBarrier::MarkingFromCode(Address raw_host, Address raw_slot) {
  HeapObject host = HeapObject::cast(Object(raw_host));
  MaybeObjectSlot slot(raw_slot);
  WriteBarrier::Marking(host, slot, *slot);
  // Called by RecordWriteCodeStubAssembler, which doesnt accept void type
  return 0;
}

}  // namespace internal
}  // namespace v8
