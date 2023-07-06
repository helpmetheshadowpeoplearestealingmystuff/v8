// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MARKING_STATE_INL_H_
#define V8_HEAP_MARKING_STATE_INL_H_

#include "src/heap/marking-inl.h"
#include "src/heap/marking-state.h"
#include "src/heap/memory-chunk.h"

namespace v8 {
namespace internal {

template <typename ConcreteState, AccessMode access_mode>
bool MarkingStateBase<ConcreteState, access_mode>::IsMarked(
    const HeapObject obj) const {
  return MarkBit::From(obj).template Get<access_mode>();
}

template <typename ConcreteState, AccessMode access_mode>
bool MarkingStateBase<ConcreteState, access_mode>::IsUnmarked(
    const HeapObject obj) const {
  return !IsMarked(obj);
}

template <typename ConcreteState, AccessMode access_mode>
bool MarkingStateBase<ConcreteState, access_mode>::TryMark(HeapObject obj) {
  return MarkBit::From(obj).template Set<access_mode>();
}

template <typename ConcreteState, AccessMode access_mode>
bool MarkingStateBase<ConcreteState, access_mode>::TryMarkAndAccountLiveBytes(
    HeapObject obj) {
  if (TryMark(obj)) {
    MemoryChunk::FromHeapObject(obj)->IncrementLiveBytesAtomically(
        ALIGN_TO_ALLOCATION_ALIGNMENT(obj.Size(cage_base())));
    return true;
  }
  return false;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MARKING_STATE_INL_H_
