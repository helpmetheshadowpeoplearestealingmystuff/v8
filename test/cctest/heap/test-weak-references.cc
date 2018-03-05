// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api.h"
#include "src/assembler-inl.h"
#include "src/factory.h"
#include "src/isolate.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-utils.h"

namespace v8 {
namespace internal {
namespace heap {

Handle<FeedbackVector> CreateFeedbackVectorForTest(
    v8::Isolate* isolate, Factory* factory,
    PretenureFlag pretenure_flag = NOT_TENURED) {
  v8::Local<v8::Script> script =
      v8::Script::Compile(isolate->GetCurrentContext(),
                          v8::String::NewFromUtf8(isolate, "function foo() {}",
                                                  v8::NewStringType::kNormal)
                              .ToLocalChecked())
          .ToLocalChecked();
  Handle<Object> obj = v8::Utils::OpenHandle(*script);
  Handle<SharedFunctionInfo> shared_function =
      Handle<SharedFunctionInfo>(JSFunction::cast(*obj)->shared());
  Handle<FeedbackVector> fv =
      factory->NewFeedbackVector(shared_function, pretenure_flag);
  return fv;
}

TEST(WeakReferencesBasic) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();
  HandleScope outer_scope(isolate);

  Handle<FeedbackVector> fv =
      CreateFeedbackVectorForTest(CcTest::isolate(), factory);
  CHECK(heap->InNewSpace(*fv));

  MaybeObject* code_object = fv->optimized_code_weak_or_smi();
  CHECK(code_object->IsSmi());
  CcTest::CollectAllGarbage();
  CHECK(heap->InNewSpace(*fv));
  CHECK_EQ(code_object, fv->optimized_code_weak_or_smi());

  {
    HandleScope inner_scope(isolate);

    // Create a new Code.
    Assembler assm(isolate, nullptr, 0);
    assm.nop();  // supported on all architectures
    CodeDesc desc;
    assm.GetCode(isolate, &desc);
    Handle<Code> code =
        isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());
    CHECK(code->IsCode());

    fv->set_optimized_code_weak_or_smi(HeapObjectReference::Weak(*code));
    HeapObject* code_heap_object;
    CHECK(
        fv->optimized_code_weak_or_smi()->ToWeakHeapObject(&code_heap_object));
    CHECK_EQ(*code, code_heap_object);

    CcTest::CollectAllGarbage();

    CHECK(
        fv->optimized_code_weak_or_smi()->ToWeakHeapObject(&code_heap_object));
    CHECK_EQ(*code, code_heap_object);
  }  // code will go out of scope.

  CcTest::CollectAllGarbage();
  CHECK(fv->optimized_code_weak_or_smi()->IsClearedWeakHeapObject());
}

TEST(WeakReferencesOldToOld) {
  // Like WeakReferencesBasic, but the updated weak slot is in the old space,
  // and referring to an old space object.
  ManualGCScope manual_gc_scope;
  FLAG_manual_evacuation_candidates_selection = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();

  HandleScope outer_scope(isolate);
  Handle<FeedbackVector> fv =
      CreateFeedbackVectorForTest(CcTest::isolate(), factory, TENURED);
  CHECK(heap->InOldSpace(*fv));

  // Create a new FixedArray which the FeedbackVector will point to.
  Handle<FixedArray> fixed_array = factory->NewFixedArray(1, TENURED);
  CHECK(heap->InOldSpace(*fixed_array));
  fv->set_optimized_code_weak_or_smi(HeapObjectReference::Weak(*fixed_array));

  Page* page_before_gc = Page::FromAddress(fixed_array->address());
  heap::ForceEvacuationCandidate(page_before_gc);
  CcTest::CollectAllGarbage();
  CHECK(heap->InOldSpace(*fixed_array));

  HeapObject* heap_object;
  CHECK(fv->optimized_code_weak_or_smi()->ToWeakHeapObject(&heap_object));
  CHECK_EQ(heap_object, *fixed_array);
}

TEST(WeakReferencesOldToNew) {
  // Like WeakReferencesBasic, but the updated weak slot is in the old space,
  // and referring to an new space object.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();

  HandleScope outer_scope(isolate);
  Handle<FeedbackVector> fv =
      CreateFeedbackVectorForTest(CcTest::isolate(), factory, TENURED);
  CHECK(heap->InOldSpace(*fv));

  // Create a new FixedArray which the FeedbackVector will point to.
  Handle<FixedArray> fixed_array = factory->NewFixedArray(1);
  CHECK(heap->InNewSpace(*fixed_array));
  fv->set_optimized_code_weak_or_smi(HeapObjectReference::Weak(*fixed_array));

  CcTest::CollectAllGarbage();

  HeapObject* heap_object;
  CHECK(fv->optimized_code_weak_or_smi()->ToWeakHeapObject(&heap_object));
  CHECK_EQ(heap_object, *fixed_array);
}

TEST(WeakReferencesOldToNewScavenged) {
  // Like WeakReferencesBasic, but the updated weak slot is in the old space,
  // and referring to an new space object, which is then scavenged.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();

  HandleScope outer_scope(isolate);
  Handle<FeedbackVector> fv =
      CreateFeedbackVectorForTest(CcTest::isolate(), factory, TENURED);
  CHECK(heap->InOldSpace(*fv));

  // Create a new FixedArray which the FeedbackVector will point to.
  Handle<FixedArray> fixed_array = factory->NewFixedArray(1);
  CHECK(heap->InNewSpace(*fixed_array));
  fv->set_optimized_code_weak_or_smi(HeapObjectReference::Weak(*fixed_array));

  CcTest::CollectGarbage(NEW_SPACE);

  HeapObject* heap_object;
  CHECK(fv->optimized_code_weak_or_smi()->ToWeakHeapObject(&heap_object));
  CHECK_EQ(heap_object, *fixed_array);
}

TEST(WeakReferencesOldToCleared) {
  // Like WeakReferencesBasic, but the updated weak slot is in the old space,
  // and is cleared.
  ManualGCScope manual_gc_scope;
  FLAG_manual_evacuation_candidates_selection = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();

  HandleScope outer_scope(isolate);
  Handle<FeedbackVector> fv =
      CreateFeedbackVectorForTest(CcTest::isolate(), factory, TENURED);
  CHECK(heap->InOldSpace(*fv));
  fv->set_optimized_code_weak_or_smi(HeapObjectReference::ClearedValue());

  CcTest::CollectAllGarbage();
  CHECK(fv->optimized_code_weak_or_smi()->IsClearedWeakHeapObject());
}

TEST(ObjectMovesBeforeClearingWeakField) {
  if (!FLAG_incremental_marking) {
    return;
  }
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();

  HandleScope outer_scope(isolate);
  Handle<FeedbackVector> fv =
      CreateFeedbackVectorForTest(CcTest::isolate(), factory);
  CHECK(heap->InNewSpace(*fv));
  FeedbackVector* fv_location = *fv;
  {
    HandleScope inner_scope(isolate);
    // Create a new FixedArray which the FeedbackVector will point to.
    Handle<FixedArray> fixed_array = factory->NewFixedArray(1);
    CHECK(heap->InNewSpace(*fixed_array));
    fv->set_optimized_code_weak_or_smi(HeapObjectReference::Weak(*fixed_array));
    // inner_scope will go out of scope, so when marking the next time,
    // *fixed_array will stay white.
  }

  // Do marking steps; this will store *fv into the list for later processing
  // (since it points to a white object).
  SimulateIncrementalMarking(heap, true);

  // Scavenger will move *fv.
  CcTest::CollectGarbage(NEW_SPACE);
  FeedbackVector* new_fv_location = *fv;
  CHECK_NE(fv_location, new_fv_location);
  CHECK(fv->optimized_code_weak_or_smi()->IsWeakHeapObject());

  // Now we try to clear *fv.
  CcTest::CollectAllGarbage();
  CHECK(fv->optimized_code_weak_or_smi()->IsClearedWeakHeapObject());
}

TEST(ObjectWithWeakReferencePromoted) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();

  HandleScope outer_scope(isolate);
  Handle<FeedbackVector> fv =
      CreateFeedbackVectorForTest(CcTest::isolate(), factory);
  CHECK(heap->InNewSpace(*fv));

  // Create a new FixedArray which the FeedbackVector will point to.
  Handle<FixedArray> fixed_array = factory->NewFixedArray(1);
  CHECK(heap->InNewSpace(*fixed_array));
  fv->set_optimized_code_weak_or_smi(HeapObjectReference::Weak(*fixed_array));

  CcTest::CollectGarbage(NEW_SPACE);
  CcTest::CollectGarbage(NEW_SPACE);
  CHECK(heap->InOldSpace(*fv));
  CHECK(heap->InOldSpace(*fixed_array));

  HeapObject* heap_object;
  CHECK(fv->optimized_code_weak_or_smi()->ToWeakHeapObject(&heap_object));
  CHECK_EQ(heap_object, *fixed_array);
}

TEST(ObjectWithClearedWeakReferencePromoted) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();

  HandleScope outer_scope(isolate);
  Handle<FeedbackVector> fv =
      CreateFeedbackVectorForTest(CcTest::isolate(), factory);
  CHECK(heap->InNewSpace(*fv));

  fv->set_optimized_code_weak_or_smi(HeapObjectReference::ClearedValue());

  CcTest::CollectGarbage(NEW_SPACE);
  CHECK(heap->InNewSpace(*fv));
  CHECK(fv->optimized_code_weak_or_smi()->IsClearedWeakHeapObject());

  CcTest::CollectGarbage(NEW_SPACE);
  CHECK(heap->InOldSpace(*fv));
  CHECK(fv->optimized_code_weak_or_smi()->IsClearedWeakHeapObject());

  CcTest::CollectAllGarbage();
  CHECK(fv->optimized_code_weak_or_smi()->IsClearedWeakHeapObject());
}

TEST(WeakReferenceWriteBarrier) {
  if (!FLAG_incremental_marking) {
    return;
  }

  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();

  HandleScope outer_scope(isolate);
  Handle<FeedbackVector> fv =
      CreateFeedbackVectorForTest(CcTest::isolate(), factory);
  CHECK(heap->InNewSpace(*fv));

  {
    HandleScope inner_scope(isolate);

    // Create a new FixedArray which the FeedbackVector will point to.
    Handle<FixedArray> fixed_array1 = factory->NewFixedArray(1);
    CHECK(heap->InNewSpace(*fixed_array1));
    fv->set_optimized_code_weak_or_smi(
        HeapObjectReference::Weak(*fixed_array1));

    SimulateIncrementalMarking(heap, true);

    Handle<FixedArray> fixed_array2 = factory->NewFixedArray(1);
    CHECK(heap->InNewSpace(*fixed_array2));
    // This write will trigger the write barrier.
    fv->set_optimized_code_weak_or_smi(
        HeapObjectReference::Weak(*fixed_array2));
  }

  CcTest::CollectAllGarbage();

  // Check that the write barrier treated the weak reference as strong.
  CHECK(fv->optimized_code_weak_or_smi()->IsWeakHeapObject());
}

}  // namespace heap
}  // namespace internal
}  // namespace v8
