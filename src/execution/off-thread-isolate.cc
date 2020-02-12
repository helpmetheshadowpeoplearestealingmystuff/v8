// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/off-thread-isolate.h"

#include "src/execution/isolate.h"
#include "src/logging/off-thread-logger.h"

namespace v8 {
namespace internal {

OffThreadIsolate::OffThreadIsolate(Isolate* isolate)
    : HiddenOffThreadFactory(isolate),
      isolate_(isolate),
      logger_(new OffThreadLogger()) {}
OffThreadIsolate::~OffThreadIsolate() { delete logger_; }

int OffThreadIsolate::GetNextScriptId() { return isolate_->GetNextScriptId(); }

bool OffThreadIsolate::NeedsSourcePositionsForProfiling() {
  // TODO(leszeks): Figure out if it makes sense to check this asynchronously.
  return isolate_->NeedsSourcePositionsForProfiling();
}

bool OffThreadIsolate::is_collecting_type_profile() {
  // TODO(leszeks): Figure out if it makes sense to check this asynchronously.
  return isolate_->is_collecting_type_profile();
}

}  // namespace internal
}  // namespace v8
