// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/compilation-manager.h"

#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace wasm {

void CompilationManager::StartAsyncCompileJob(
    Isolate* isolate, std::unique_ptr<byte[]> bytes_copy, size_t length,
    Handle<Context> context, Handle<JSPromise> promise) {
  std::shared_ptr<AsyncCompileJob> job(new AsyncCompileJob(
      isolate, std::move(bytes_copy), length, context, promise));
  jobs_.insert({job.get(), job});
  job->Start();
}

std::shared_ptr<AsyncCompileJob> CompilationManager::RemoveJob(
    AsyncCompileJob* job) {
  auto item = jobs_.find(job);
  DCHECK(item != jobs_.end());
  std::shared_ptr<AsyncCompileJob> result = item->second;
  jobs_.erase(item);
  return result;
}

void CompilationManager::TearDown() { jobs_.clear(); }

}  // namespace wasm
}  // namespace internal
}  // namespace v8
