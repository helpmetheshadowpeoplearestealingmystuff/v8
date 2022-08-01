// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_JS_CPP_MARKING_STATE_H_
#define V8_HEAP_CPPGC_JS_CPP_MARKING_STATE_H_

#include <memory>

#include "include/v8-cppgc.h"
#include "src/heap/cppgc/marking-state.h"
#include "src/heap/cppgc/marking-worklists.h"
#include "src/objects/map.h"

namespace v8 {
namespace internal {

class JSObject;
class EmbedderDataSlot;

class CppMarkingState {
 public:
  using EmbedderDataSnapshot = void*;

  CppMarkingState(Isolate* isolate, const WrapperDescriptor& wrapper_descriptor,
                  cppgc::internal::MarkingStateBase& main_thread_marking_state)
      : isolate_(isolate),
        wrapper_descriptor_(wrapper_descriptor),
        owned_marking_state_(nullptr),
        marking_state_(main_thread_marking_state) {}

  CppMarkingState(Isolate* isolate, const WrapperDescriptor& wrapper_descriptor,
                  std::unique_ptr<cppgc::internal::MarkingStateBase>
                      concurrent_marking_state)
      : isolate_(isolate),
        wrapper_descriptor_(wrapper_descriptor),
        owned_marking_state_(std::move(concurrent_marking_state)),
        marking_state_(*owned_marking_state_) {}
  CppMarkingState(const CppMarkingState&) = delete;
  CppMarkingState& operator=(const CppMarkingState&) = delete;

  void Publish() { marking_state_.Publish(); }

  inline bool ExtractEmbedderDataSnapshot(Map, JSObject, EmbedderDataSnapshot&);

  inline void MarkAndPush(const EmbedderDataSnapshot&);

  bool IsLocalEmpty() {
    return marking_state_.marking_worklist().IsLocalEmpty();
  }

 private:
  Isolate* const isolate_;
  const WrapperDescriptor& wrapper_descriptor_;

  std::unique_ptr<cppgc::internal::MarkingStateBase> owned_marking_state_;
  cppgc::internal::MarkingStateBase& marking_state_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_CPPGC_JS_CPP_MARKING_STATE_H_
