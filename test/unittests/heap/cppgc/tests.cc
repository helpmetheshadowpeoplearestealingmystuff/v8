// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/heap/cppgc/tests.h"

#include <memory>

#include "src/heap/cppgc/object-allocator.h"
#include "test/unittests/heap/cppgc/test-platform.h"

#if !CPPGC_IS_STANDALONE
#include "include/v8-initialization.h"
#endif  // !CPPGC_IS_STANDALONE

namespace cppgc {
namespace internal {
namespace testing {

// static
std::shared_ptr<TestPlatform> TestWithPlatform::platform_;

// static
void TestWithPlatform::SetUpTestSuite() {
  platform_ = std::make_shared<TestPlatform>(
      std::make_unique<DelegatingTracingController>());

#if !CPPGC_IS_STANDALONE
  // For non-standalone builds, we need to initialize V8's platform so that it
  // can be looked-up by trace-event.h.
  v8::V8::InitializePlatform(platform_->GetV8Platform());
#ifdef V8_ENABLE_SANDBOX
  CHECK(v8::V8::InitializeSandbox());
#endif  // V8_ENABLE_SANDBOX
  v8::V8::Initialize();
#endif  // !CPPGC_IS_STANDALONE
}

// static
void TestWithPlatform::TearDownTestSuite() {
#if !CPPGC_IS_STANDALONE
  v8::V8::Dispose();
  v8::V8::DisposePlatform();
#endif  // !CPPGC_IS_STANDALONE
  platform_.reset();
}

TestWithHeap::TestWithHeap()
    : heap_(Heap::Create(platform_)),
      allocation_handle_(heap_->GetAllocationHandle()) {}

TestWithHeap::~TestWithHeap() {
#if defined(CPPGC_CAGED_HEAP)
  CagedHeap::Instance().ResetForTesting();
#endif  // defined(CPPGC_CAGED_HEAP)
}

void TestWithHeap::ResetLinearAllocationBuffers() {
  Heap::From(GetHeap())->object_allocator().ResetLinearAllocationBuffers();
}

TestSupportingAllocationOnly::TestSupportingAllocationOnly()
    : no_gc_scope_(GetHeap()->GetHeapHandle()) {}

}  // namespace testing
}  // namespace internal
}  // namespace cppgc
