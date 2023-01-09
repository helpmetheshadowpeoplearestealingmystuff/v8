// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_EMBEDDER_TRACING_H_
#define V8_HEAP_EMBEDDER_TRACING_H_

#include <atomic>

#include "include/v8-cppgc.h"
#include "src/execution/isolate.h"
#include "src/heap/cppgc-js/cpp-heap.h"

namespace v8 {
namespace internal {

class Heap;
class JSObject;

class V8_EXPORT_PRIVATE LocalEmbedderHeapTracer final {
 public:
  enum class CollectionType : uint8_t {
    kMinor,
    kMajor,
  };

  explicit LocalEmbedderHeapTracer(Isolate* isolate) : isolate_(isolate) {}

  ~LocalEmbedderHeapTracer() {
    // CppHeap is not detached from Isolate here. Detaching is done explicitly
    // on Isolate/Heap/CppHeap destruction.
  }

  bool InUse() const { return cpp_heap_; }

  void SetCppHeap(CppHeap* cpp_heap);
  void PrepareForTrace(CollectionType type);
  void TracePrologue();
  void TraceEpilogue();
  void EnterFinalPause();
  bool IsRemoteTracingDone();

  cppgc::EmbedderStackState embedder_stack_state() const {
    return embedder_stack_state_;
  }

 private:
  CppHeap* cpp_heap() {
    DCHECK_NOT_NULL(cpp_heap_);
    DCHECK_IMPLIES(isolate_, cpp_heap_ == isolate_->heap()->cpp_heap());
    return cpp_heap_;
  }

  Isolate* const isolate_;
  CppHeap* cpp_heap_ = nullptr;

  cppgc::EmbedderStackState embedder_stack_state_ =
      cppgc::EmbedderStackState::kMayContainHeapPointers;

  friend class EmbedderStackStateScope;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_EMBEDDER_TRACING_H_
