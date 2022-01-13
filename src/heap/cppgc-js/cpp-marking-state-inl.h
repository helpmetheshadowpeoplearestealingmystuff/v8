// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_JS_CPP_MARKING_STATE_INL_H_
#define V8_HEAP_CPPGC_JS_CPP_MARKING_STATE_INL_H_

#include "src/heap/cppgc-js/cpp-marking-state.h"
#include "src/heap/embedder-tracing-inl.h"
#include "src/objects/embedder-data-slot.h"

namespace v8 {
namespace internal {

void CppMarkingState::MarkAndPush(const EmbedderDataSlot& type_slot,
                                  const EmbedderDataSlot& instance_slot) {
  LocalEmbedderHeapTracer::WrapperInfo info;
  if (LocalEmbedderHeapTracer::ExtractWrappableInfo(
          isolate_, wrapper_descriptor_, type_slot, instance_slot, &info)) {
    marking_state_.MarkAndPush(
        cppgc::internal::HeapObjectHeader::FromObject(info.second));
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_CPPGC_JS_CPP_MARKING_STATE_INL_H_
