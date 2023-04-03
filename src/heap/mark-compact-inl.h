// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MARK_COMPACT_INL_H_
#define V8_HEAP_MARK_COMPACT_INL_H_

#include "src/base/bits.h"
#include "src/base/build_config.h"
#include "src/codegen/assembler-inl.h"
#include "src/heap/heap-inl.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/index-generator.h"
#include "src/heap/mark-compact.h"
#include "src/heap/marking-state-inl.h"
#include "src/heap/marking-worklist-inl.h"
#include "src/heap/marking-worklist.h"
#include "src/heap/objects-visiting-inl.h"
#include "src/heap/remembered-set-inl.h"
#include "src/objects/js-collection-inl.h"
#include "src/objects/js-weak-refs-inl.h"
#include "src/objects/slots-inl.h"
#include "src/objects/transitions.h"

namespace v8 {
namespace internal {

void MarkCompactCollector::MarkObject(HeapObject host, HeapObject obj) {
  DCHECK(ReadOnlyHeap::Contains(obj) || heap()->Contains(obj));
  if (marking_state()->TryMark(obj)) {
    local_marking_worklists()->Push(obj);
    if (V8_UNLIKELY(v8_flags.track_retaining_path)) {
      heap_->AddRetainer(host, obj);
    }
  }
}

void MarkCompactCollector::MarkRootObject(Root root, HeapObject obj) {
  DCHECK(ReadOnlyHeap::Contains(obj) || heap()->Contains(obj));
  if (marking_state()->TryMark(obj)) {
    local_marking_worklists()->Push(obj);
    if (V8_UNLIKELY(v8_flags.track_retaining_path)) {
      heap_->AddRetainingRoot(root, obj);
    }
  }
}

void MinorMarkCompactCollector::MarkRootObject(HeapObject heap_object) {
  if (Heap::InYoungGeneration(heap_object) &&
      non_atomic_marking_state()->TryMark(heap_object)) {
    // Maps won't change in the atomic pause, so the map can be read without
    // atomics.
    Map map = Map::cast(*heap_object.map_slot());
    if (Map::ObjectFieldsFrom(map.visitor_id()) == ObjectFields::kDataOnly) {
      const int size = heap_object.SizeFromMap(map);
      marking_state()->IncrementLiveBytes(
          MemoryChunk::cast(BasicMemoryChunk::FromHeapObject(heap_object)),
          ALIGN_TO_ALLOCATION_ALIGNMENT(size));
    } else {
      local_marking_worklists_->Push(heap_object);
    }
  }
}

// static
void MarkCompactCollector::RecordSlot(HeapObject object, ObjectSlot slot,
                                      HeapObject target) {
  RecordSlot(object, HeapObjectSlot(slot), target);
}

// static
void MarkCompactCollector::RecordSlot(HeapObject object, HeapObjectSlot slot,
                                      HeapObject target) {
  MemoryChunk* source_page = MemoryChunk::FromHeapObject(object);
  if (!source_page->ShouldSkipEvacuationSlotRecording()) {
    RecordSlot(source_page, slot, target);
  }
}

// static
void MarkCompactCollector::RecordSlot(MemoryChunk* source_page,
                                      HeapObjectSlot slot, HeapObject target) {
  BasicMemoryChunk* target_page = BasicMemoryChunk::FromHeapObject(target);
  if (target_page->IsEvacuationCandidate()) {
    if (target_page->IsFlagSet(MemoryChunk::IS_EXECUTABLE)) {
      RememberedSet<OLD_TO_CODE>::Insert<AccessMode::ATOMIC>(source_page,
                                                             slot.address());
    } else {
      RememberedSet<OLD_TO_OLD>::Insert<AccessMode::ATOMIC>(source_page,
                                                            slot.address());
    }
  }
}

void MarkCompactCollector::AddTransitionArray(TransitionArray array) {
  local_weak_objects()->transition_arrays_local.Push(array);
}

bool MarkCompactCollector::ShouldMarkObject(HeapObject object) const {
  if (object.InReadOnlySpace()) return false;
  if (V8_LIKELY(!uses_shared_heap_)) return true;
  if (is_shared_space_isolate_) return true;
  return !object.InAnySharedSpace();
}

template <typename MarkingState>
template <typename TSlot>
void MainMarkingVisitor<MarkingState>::RecordSlot(HeapObject object, TSlot slot,
                                                  HeapObject target) {
  MarkCompactCollector::RecordSlot(object, slot, target);
}

template <typename MarkingState>
void MainMarkingVisitor<MarkingState>::RecordRelocSlot(RelocInfo* rinfo,
                                                       HeapObject target) {
  MarkCompactCollector::RecordRelocSlot(rinfo, target);
}

Isolate* CollectorBase::isolate() { return heap()->isolate(); }

template <typename TSlot>
void YoungGenerationMainMarkingVisitor::VisitPointersImpl(HeapObject host,
                                                          TSlot start,
                                                          TSlot end) {
  for (TSlot slot = start; slot < end; ++slot) {
    typename TSlot::TObject target = *slot;
    VisitObjectImpl(target);
  }
}

V8_INLINE void YoungGenerationMarkingState::IncrementLiveBytes(
    MemoryChunk* chunk, intptr_t by) {
  DCHECK_IMPLIES(V8_COMPRESS_POINTERS_8GB_BOOL,
                 IsAligned(by, kObjectAlignment8GbHeap));
  const size_t hash =
      (reinterpret_cast<size_t>(chunk) >> kPageSizeBits) & kEntriesMask;
  auto& entry = live_bytes_data_[hash];
  if (entry.first && entry.first != chunk) {
    entry.first->live_byte_count_.fetch_add(entry.second,
                                            std::memory_order_relaxed);
    entry.first = chunk;
    entry.second = 0;
  } else {
    entry.first = chunk;
  }
  entry.second += by;
}

YoungGenerationMarkingState::~YoungGenerationMarkingState() {
  for (auto& pair : live_bytes_data_) {
    if (pair.first) {
      pair.first->live_byte_count_.fetch_add(pair.second,
                                             std::memory_order_relaxed);
    }
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MARK_COMPACT_INL_H_
