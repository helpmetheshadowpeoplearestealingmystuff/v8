// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TEST_FEEDBACK_VECTOR_H_
#define V8_TEST_FEEDBACK_VECTOR_H_

#include "src/feedback-vector.h"
#include "src/objects.h"
#include "src/objects/shared-function-info.h"

namespace v8 {
namespace internal {

// Helper class that allows to write tests in a slot size independent manner.
// Use helper.slot(X) to get X'th slot identifier.
class FeedbackVectorHelper {
 public:
  explicit FeedbackVectorHelper(Handle<FeedbackVector> vector)
      : vector_(vector) {
    int slot_count = vector->length();
    slots_.reserve(slot_count);
    FeedbackMetadataIterator iter(vector->metadata());
    while (iter.HasNext()) {
      FeedbackSlot slot = iter.Next();
      slots_.push_back(slot);
    }
  }

  Handle<FeedbackVector> vector() { return vector_; }

  // Returns slot identifier by numerical index.
  FeedbackSlot slot(int index) const { return slots_[index]; }

  // Returns the number of slots in the feedback vector.
  int slot_count() const { return static_cast<int>(slots_.size()); }

 private:
  Handle<FeedbackVector> vector_;
  std::vector<FeedbackSlot> slots_;
};

template <typename Spec>
Handle<FeedbackVector> NewFeedbackVector(Isolate* isolate, Spec* spec) {
  Handle<FeedbackMetadata> metadata = FeedbackMetadata::New(isolate, spec);
  Handle<SharedFunctionInfo> shared =
      isolate->factory()->NewSharedFunctionInfoForBuiltin(
          isolate->factory()->empty_string(), Builtins::kIllegal);
  shared->set_feedback_metadata(*metadata);
  return FeedbackVector::New(isolate, shared);
}

template <typename Spec>
Handle<FeedbackMetadata> NewFeedbackMetadata(Isolate* isolate, Spec* spec) {
  return FeedbackMetadata::New(isolate, spec);
}

}  // namespace internal
}  // namespace v8

#endif
