// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/common/globals.h"
#include "src/execution/isolate-inl.h"
#include "src/heap/heap-inl.h"
#include "test/cctest/cctest.h"

#ifdef V8_COMPRESS_POINTERS

namespace v8 {
namespace internal {

UNINITIALIZED_TEST(PtrComprCageAndIsolateRoot) {
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();

  v8::Isolate* isolate1 = v8::Isolate::New(create_params);
  Isolate* i_isolate1 = reinterpret_cast<Isolate*>(isolate1);
  v8::Isolate* isolate2 = v8::Isolate::New(create_params);
  Isolate* i_isolate2 = reinterpret_cast<Isolate*>(isolate2);

#ifdef V8_COMPRESS_POINTERS_IN_ISOLATE_CAGE
  CHECK_EQ(i_isolate1->isolate_root(), i_isolate1->cage_base());
  CHECK_EQ(i_isolate2->isolate_root(), i_isolate2->cage_base());
  CHECK_NE(i_isolate1->cage_base(), i_isolate2->cage_base());
#endif

#ifdef V8_COMPRESS_POINTERS_IN_SHARED_CAGE
  CHECK_NE(i_isolate1->isolate_root(), i_isolate1->cage_base());
  CHECK_NE(i_isolate2->isolate_root(), i_isolate2->cage_base());
  CHECK_NE(i_isolate1->isolate_root(), i_isolate2->isolate_root());
  CHECK_EQ(i_isolate1->cage_base(), i_isolate2->cage_base());
#endif

  isolate1->Dispose();
  isolate2->Dispose();
}

UNINITIALIZED_TEST(PtrComprCageCodeRange) {
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();

  v8::Isolate* isolate = v8::Isolate::New(create_params);
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);

  VirtualMemoryCage* cage = i_isolate->GetPtrComprCage();
  if (i_isolate->RequiresCodeRange()) {
    CHECK(!i_isolate->heap()->code_region().is_empty());
    CHECK(cage->reservation()->InVM(i_isolate->heap()->code_region().begin(),
                                    i_isolate->heap()->code_region().size()));
  }

  isolate->Dispose();
}

#ifdef V8_COMPRESS_POINTERS_IN_SHARED_CAGE
UNINITIALIZED_TEST(SharedPtrComprCage) {
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();

  v8::Isolate* isolate1 = v8::Isolate::New(create_params);
  Isolate* i_isolate1 = reinterpret_cast<Isolate*>(isolate1);
  v8::Isolate* isolate2 = v8::Isolate::New(create_params);
  Isolate* i_isolate2 = reinterpret_cast<Isolate*>(isolate2);

  Factory* factory1 = i_isolate1->factory();
  Factory* factory2 = i_isolate2->factory();

  {
    HandleScope scope1(i_isolate1);
    HandleScope scope2(i_isolate2);

    Handle<FixedArray> isolate1_object = factory1->NewFixedArray(100);
    Handle<FixedArray> isolate2_object = factory2->NewFixedArray(100);

    CHECK_EQ(GetPtrComprCageBase(*isolate1_object),
             GetPtrComprCageBase(*isolate2_object));
  }

  isolate1->Dispose();
  isolate2->Dispose();
}

UNINITIALIZED_TEST(SharedPtrComprCageCodeRange) {
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();

  v8::Isolate* isolate1 = v8::Isolate::New(create_params);
  Isolate* i_isolate1 = reinterpret_cast<Isolate*>(isolate1);
  v8::Isolate* isolate2 = v8::Isolate::New(create_params);
  Isolate* i_isolate2 = reinterpret_cast<Isolate*>(isolate2);

  if (i_isolate1->RequiresCodeRange() || i_isolate2->RequiresCodeRange()) {
    CHECK_EQ(i_isolate1->heap()->code_region(),
             i_isolate2->heap()->code_region());
  }

  isolate1->Dispose();
  isolate2->Dispose();
}

UNINITIALIZED_TEST(SharedPtrComprCageRemappedBuiltinsJitlessFalseToTrue) {
  // Testing that toggling jitless from false to true use the same re-embedded
  // builtins. Toggling jitless from false to true with shared pointer
  // compression cage is not supported.

  if (!V8_SHORT_BUILTIN_CALLS_BOOL) return;
  FLAG_short_builtin_calls = true;
  FLAG_jitless = false;

  constexpr uint64_t kMemoryGB = 4;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  create_params.constraints.ConfigureDefaults(kMemoryGB * GB, kMemoryGB * GB);

  v8::Isolate* isolate1 = v8::Isolate::New(create_params);
  Isolate* i_isolate1 = reinterpret_cast<Isolate*>(isolate1);
  v8::Isolate* isolate2 = v8::Isolate::New(create_params);
  Isolate* i_isolate2 = reinterpret_cast<Isolate*>(isolate2);

  CHECK_EQ(i_isolate1->embedded_blob_code(), i_isolate2->embedded_blob_code());
  CodeRange* shared_code_range = CodeRange::GetProcessWideCodeRange().get();
  if (shared_code_range &&
      shared_code_range->embedded_blob_code_copy() != nullptr) {
    CHECK_EQ(shared_code_range->embedded_blob_code_copy(),
             i_isolate1->embedded_blob_code());
    CHECK_EQ(shared_code_range->embedded_blob_code_copy(),
             i_isolate2->embedded_blob_code());
  }

  FLAG_jitless = true;
  v8::Isolate* isolate3 = v8::Isolate::New(create_params);
  Isolate* i_isolate3 = reinterpret_cast<Isolate*>(isolate3);
  if (shared_code_range &&
      shared_code_range->embedded_blob_code_copy() != nullptr) {
    CHECK_EQ(shared_code_range->embedded_blob_code_copy(),
             i_isolate3->embedded_blob_code());
  }

  isolate1->Dispose();
  isolate2->Dispose();
  isolate3->Dispose();
}

namespace {
constexpr int kIsolatesToAllocate = 25;

class IsolateAllocatingThread final : public v8::base::Thread {
 public:
  IsolateAllocatingThread()
      : v8::base::Thread(base::Thread::Options("IsolateAllocatingThread")) {}

  void Run() override {
    std::vector<v8::Isolate*> isolates;
    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = CcTest::array_buffer_allocator();

    for (int i = 0; i < kIsolatesToAllocate; i++) {
      isolates.push_back(v8::Isolate::New(create_params));
    }

    for (auto* isolate : isolates) {
      isolate->Dispose();
    }
  }
};
}  // namespace

UNINITIALIZED_TEST(SharedPtrComprCageRace) {
  // Make a bunch of Isolates concurrently as a smoke test against races during
  // initialization and de-initialization.

  std::vector<std::unique_ptr<IsolateAllocatingThread>> threads;
  constexpr int kThreads = 10;

  for (int i = 0; i < kThreads; i++) {
    auto thread = std::make_unique<IsolateAllocatingThread>();
    CHECK(thread->Start());
    threads.push_back(std::move(thread));
  }

  for (auto& thread : threads) {
    thread->Join();
  }
}

#ifdef V8_SHARED_RO_HEAP
UNINITIALIZED_TEST(SharedPtrComprCageImpliesSharedReadOnlyHeap) {
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();

  v8::Isolate* isolate1 = v8::Isolate::New(create_params);
  Isolate* i_isolate1 = reinterpret_cast<Isolate*>(isolate1);
  v8::Isolate* isolate2 = v8::Isolate::New(create_params);
  Isolate* i_isolate2 = reinterpret_cast<Isolate*>(isolate2);

  CHECK_EQ(i_isolate1->read_only_heap(), i_isolate2->read_only_heap());

  // Spot check that some read-only roots are the same.
  CHECK_EQ(ReadOnlyRoots(i_isolate1).the_hole_value(),
           ReadOnlyRoots(i_isolate2).the_hole_value());
  CHECK_EQ(ReadOnlyRoots(i_isolate1).code_map(),
           ReadOnlyRoots(i_isolate2).code_map());
  CHECK_EQ(ReadOnlyRoots(i_isolate1).exception(),
           ReadOnlyRoots(i_isolate2).exception());

  isolate1->Dispose();
  isolate2->Dispose();
}
#endif  // V8_SHARED_RO_HEAP
#endif  // V8_COMPRESS_POINTERS_IN_SHARED_CAGE

}  // namespace internal
}  // namespace v8

#endif  // V8_COMPRESS_POINTERS
