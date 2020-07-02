// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api/api-inl.h"
#include "src/codegen/assembler-inl.h"
#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/heap/heap-inl.h"
#include "src/objects/smi.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-tester.h"
#include "test/cctest/heap/heap-utils.h"

namespace v8 {
namespace internal {
namespace heap {

Handle<LoadHandler> CreateLoadHandlerForTest(
    Factory* factory, AllocationType allocation = AllocationType::kYoung) {
  Handle<LoadHandler> result = factory->NewLoadHandler(1, allocation);
  result->set_smi_handler(Smi::zero());
  result->set_validity_cell(Smi::zero());
  result->set_data1(MaybeObject::FromSmi(Smi::zero()));
  return result;
}

TEST(WeakReferencesBasic) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  HandleScope outer_scope(isolate);

  Handle<LoadHandler> lh = CreateLoadHandlerForTest(factory);

  if (!FLAG_single_generation) CHECK(Heap::InYoungGeneration(*lh));

  MaybeObject code_object = lh->data1();
  CHECK(code_object->IsSmi());
  CcTest::CollectAllGarbage();
  CHECK(FLAG_always_promote_young_mc ? !Heap::InYoungGeneration(*lh)
                                     : Heap::InYoungGeneration(*lh));
  CHECK_EQ(code_object, lh->data1());

  {
    HandleScope inner_scope(isolate);

    // Create a new Code.
    Assembler assm(AssemblerOptions{});
    assm.nop();  // supported on all architectures
    CodeDesc desc;
    assm.GetCode(isolate, &desc);
    Handle<Code> code = Factory::CodeBuilder(isolate, desc, Code::STUB).Build();
    CHECK(code->IsCode());

    lh->set_data1(HeapObjectReference::Weak(*code));
    HeapObject code_heap_object;
    CHECK(lh->data1()->GetHeapObjectIfWeak(&code_heap_object));
    CHECK_EQ(*code, code_heap_object);

    CcTest::CollectAllGarbage();

    CHECK(lh->data1()->GetHeapObjectIfWeak(&code_heap_object));
    CHECK_EQ(*code, code_heap_object);
  }  // code will go out of scope.

  CcTest::CollectAllGarbage();
  CHECK(lh->data1()->IsCleared());
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
  Handle<LoadHandler> lh =
      CreateLoadHandlerForTest(factory, AllocationType::kOld);
  CHECK(heap->InOldSpace(*lh));

  // Create a new FixedArray which the LoadHandler will point to.
  Handle<FixedArray> fixed_array =
      factory->NewFixedArray(1, AllocationType::kOld);
  CHECK(heap->InOldSpace(*fixed_array));
  lh->set_data1(HeapObjectReference::Weak(*fixed_array));

  Page* page_before_gc = Page::FromHeapObject(*fixed_array);
  heap::ForceEvacuationCandidate(page_before_gc);
  CcTest::CollectAllGarbage();
  CHECK(heap->InOldSpace(*fixed_array));

  HeapObject heap_object;
  CHECK(lh->data1()->GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(heap_object, *fixed_array);
}

TEST(WeakReferencesOldToNew) {
  // Like WeakReferencesBasic, but the updated weak slot is in the old space,
  // and referring to an new space object.
  if (FLAG_single_generation) return;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();

  HandleScope outer_scope(isolate);
  Handle<LoadHandler> lh =
      CreateLoadHandlerForTest(factory, AllocationType::kOld);
  CHECK(heap->InOldSpace(*lh));

  // Create a new FixedArray which the LoadHandler will point to.
  Handle<FixedArray> fixed_array = factory->NewFixedArray(1);
  CHECK(Heap::InYoungGeneration(*fixed_array));
  lh->set_data1(HeapObjectReference::Weak(*fixed_array));

  CcTest::CollectAllGarbage();

  HeapObject heap_object;
  CHECK(lh->data1()->GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(heap_object, *fixed_array);
}

TEST(WeakReferencesOldToNewScavenged) {
  if (FLAG_single_generation) return;
  // Like WeakReferencesBasic, but the updated weak slot is in the old space,
  // and referring to an new space object, which is then scavenged.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();

  HandleScope outer_scope(isolate);
  Handle<LoadHandler> lh =
      CreateLoadHandlerForTest(factory, AllocationType::kOld);
  CHECK(heap->InOldSpace(*lh));

  // Create a new FixedArray which the LoadHandler will point to.
  Handle<FixedArray> fixed_array = factory->NewFixedArray(1);
  CHECK(Heap::InYoungGeneration(*fixed_array));
  lh->set_data1(HeapObjectReference::Weak(*fixed_array));

  CcTest::CollectGarbage(NEW_SPACE);

  HeapObject heap_object;
  CHECK(lh->data1()->GetHeapObjectIfWeak(&heap_object));
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
  Handle<LoadHandler> lh =
      CreateLoadHandlerForTest(factory, AllocationType::kOld);
  CHECK(heap->InOldSpace(*lh));
  lh->set_data1(HeapObjectReference::ClearedValue(isolate));

  CcTest::CollectAllGarbage();
  CHECK(lh->data1()->IsCleared());
}

TEST(ObjectMovesBeforeClearingWeakField) {
  if (!FLAG_incremental_marking || FLAG_single_generation) {
    return;
  }
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();

  HandleScope outer_scope(isolate);
  Handle<LoadHandler> lh = CreateLoadHandlerForTest(factory);
  CHECK(InCorrectGeneration(*lh));
  LoadHandler lh_location = *lh;
  {
    HandleScope inner_scope(isolate);
    // Create a new FixedArray which the LoadHandler will point to.
    Handle<FixedArray> fixed_array = factory->NewFixedArray(1);
    CHECK(Heap::InYoungGeneration(*fixed_array));
    lh->set_data1(HeapObjectReference::Weak(*fixed_array));
    // inner_scope will go out of scope, so when marking the next time,
    // *fixed_array will stay white.
  }

  // Do marking steps; this will store *lh into the list for later processing
  // (since it points to a white object).
  SimulateIncrementalMarking(heap, true);

  // Scavenger will move *lh.
  CcTest::CollectGarbage(NEW_SPACE);
  LoadHandler new_lh_location = *lh;
  CHECK_NE(lh_location, new_lh_location);
  CHECK(lh->data1()->IsWeak());

  // Now we try to clear *lh.
  CcTest::CollectAllGarbage();
  CHECK(lh->data1()->IsCleared());
}

TEST(ObjectWithWeakFieldDies) {
  if (!FLAG_incremental_marking) {
    return;
  }
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();

  {
    HandleScope outer_scope(isolate);
    Handle<LoadHandler> lh = CreateLoadHandlerForTest(factory);
    CHECK(InCorrectGeneration(*lh));
    {
      HandleScope inner_scope(isolate);
      // Create a new FixedArray which the LoadHandler will point to.
      Handle<FixedArray> fixed_array = factory->NewFixedArray(1);
      CHECK(InCorrectGeneration(*fixed_array));
      lh->set_data1(HeapObjectReference::Weak(*fixed_array));
      // inner_scope will go out of scope, so when marking the next time,
      // *fixed_array will stay white.
    }

    // Do marking steps; this will store *lh into the list for later processing
    // (since it points to a white object).
    SimulateIncrementalMarking(heap, true);
  }  // outer_scope goes out of scope

  // lh will die
  CcTest::CollectGarbage(NEW_SPACE);

  // This used to crash when processing the dead weak reference.
  CcTest::CollectAllGarbage();
}

TEST(ObjectWithWeakReferencePromoted) {
  if (FLAG_single_generation) return;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();

  HandleScope outer_scope(isolate);
  Handle<LoadHandler> lh = CreateLoadHandlerForTest(factory);
  CHECK(Heap::InYoungGeneration(*lh));

  // Create a new FixedArray which the LoadHandler will point to.
  Handle<FixedArray> fixed_array = factory->NewFixedArray(1);
  CHECK(Heap::InYoungGeneration(*fixed_array));
  lh->set_data1(HeapObjectReference::Weak(*fixed_array));

  CcTest::CollectGarbage(NEW_SPACE);
  CcTest::CollectGarbage(NEW_SPACE);
  CHECK(heap->InOldSpace(*lh));
  CHECK(heap->InOldSpace(*fixed_array));

  HeapObject heap_object;
  CHECK(lh->data1()->GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(heap_object, *fixed_array);
}

TEST(ObjectWithClearedWeakReferencePromoted) {
  if (FLAG_single_generation) return;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();

  HandleScope outer_scope(isolate);
  Handle<LoadHandler> lh = CreateLoadHandlerForTest(factory);
  CHECK(Heap::InYoungGeneration(*lh));

  lh->set_data1(HeapObjectReference::ClearedValue(isolate));

  CcTest::CollectGarbage(NEW_SPACE);
  CHECK(Heap::InYoungGeneration(*lh));
  CHECK(lh->data1()->IsCleared());

  CcTest::CollectGarbage(NEW_SPACE);
  CHECK(heap->InOldSpace(*lh));
  CHECK(lh->data1()->IsCleared());

  CcTest::CollectAllGarbage();
  CHECK(lh->data1()->IsCleared());
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
  Handle<LoadHandler> lh = CreateLoadHandlerForTest(factory);
  CHECK(InCorrectGeneration(*lh));

  {
    HandleScope inner_scope(isolate);

    // Create a new FixedArray which the LoadHandler will point to.
    Handle<FixedArray> fixed_array1 = factory->NewFixedArray(1);
    CHECK(InCorrectGeneration(*fixed_array1));
    lh->set_data1(HeapObjectReference::Weak(*fixed_array1));

    SimulateIncrementalMarking(heap, true);

    Handle<FixedArray> fixed_array2 = factory->NewFixedArray(1);
    CHECK(InCorrectGeneration(*fixed_array2));
    // This write will trigger the write barrier.
    lh->set_data1(HeapObjectReference::Weak(*fixed_array2));
  }

  CcTest::CollectAllGarbage();

  // Check that the write barrier treated the weak reference as strong.
  CHECK(lh->data1()->IsWeak());
}

TEST(EmptyWeakArray) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  HandleScope outer_scope(isolate);

  Handle<WeakFixedArray> array = factory->empty_weak_fixed_array();
  CHECK(array->IsWeakFixedArray());
  CHECK(!array->IsFixedArray());
  CHECK_EQ(array->length(), 0);
}

TEST(WeakArraysBasic) {
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();
  HandleScope outer_scope(isolate);

  const int length = 4;
  Handle<WeakFixedArray> array = factory->NewWeakFixedArray(length);
  CHECK(array->IsWeakFixedArray());
  CHECK(!array->IsFixedArray());
  CHECK_EQ(array->length(), length);

  if (!FLAG_single_generation) CHECK(Heap::InYoungGeneration(*array));

  for (int i = 0; i < length; ++i) {
    HeapObject heap_object;
    CHECK(array->Get(i)->GetHeapObjectIfStrong(&heap_object));
    CHECK_EQ(heap_object, ReadOnlyRoots(heap).undefined_value());
  }

  Handle<HeapObject> saved;
  {
    HandleScope inner_scope(isolate);
    Handle<FixedArray> index0 = factory->NewFixedArray(1);
    index0->set(0, Smi::FromInt(2016));
    Handle<FixedArray> index1 = factory->NewFixedArray(1);
    index1->set(0, Smi::FromInt(2017));

    Handle<FixedArray> index2 = factory->NewFixedArray(1);
    index2->set(0, Smi::FromInt(2018));
    Handle<FixedArray> index3 = factory->NewFixedArray(1);
    index3->set(0, Smi::FromInt(2019));

    array->Set(0, HeapObjectReference::Weak(*index0));
    array->Set(1, HeapObjectReference::Weak(*index1));
    array->Set(2, HeapObjectReference::Strong(*index2));
    array->Set(3, HeapObjectReference::Weak(*index3));
    saved = inner_scope.CloseAndEscape(index1);
  }  // inner_scope goes out of scope.

  // The references are only cleared by the mark-compact (scavenger treats weak
  // references as strong). Thus we need to GC until the array reaches old
  // space.

  // TODO(marja): update this when/if we do handle weak references in the new
  // space.
  CcTest::CollectGarbage(NEW_SPACE);
  HeapObject heap_object;
  CHECK(array->Get(0)->GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(Smi::cast(FixedArray::cast(heap_object).get(0)).value(), 2016);
  CHECK(array->Get(1)->GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(Smi::cast(FixedArray::cast(heap_object).get(0)).value(), 2017);
  CHECK(array->Get(2)->GetHeapObjectIfStrong(&heap_object));
  CHECK_EQ(Smi::cast(FixedArray::cast(heap_object).get(0)).value(), 2018);
  CHECK(array->Get(3)->GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(Smi::cast(FixedArray::cast(heap_object).get(0)).value(), 2019);

  CcTest::CollectAllGarbage();
  CHECK(heap->InOldSpace(*array));
  CHECK(array->Get(0)->IsCleared());
  CHECK(array->Get(1)->GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(Smi::cast(FixedArray::cast(heap_object).get(0)).value(), 2017);
  CHECK(array->Get(2)->GetHeapObjectIfStrong(&heap_object));
  CHECK_EQ(Smi::cast(FixedArray::cast(heap_object).get(0)).value(), 2018);
  CHECK(array->Get(3)->IsCleared());
}

TEST(WeakArrayListBasic) {
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();
  HandleScope outer_scope(isolate);

  Handle<WeakArrayList> array(ReadOnlyRoots(heap).empty_weak_array_list(),
                              isolate);
  CHECK(array->IsWeakArrayList());
  CHECK(!array->IsFixedArray());
  CHECK(!array->IsWeakFixedArray());
  CHECK_EQ(array->length(), 0);

  Handle<FixedArray> index2 = factory->NewFixedArray(1);
  index2->set(0, Smi::FromInt(2017));

  Handle<HeapObject> saved;
  {
    HandleScope inner_scope(isolate);
    Handle<FixedArray> index0 = factory->NewFixedArray(1);
    index0->set(0, Smi::FromInt(2016));
    Handle<FixedArray> index4 = factory->NewFixedArray(1);
    index4->set(0, Smi::FromInt(2018));
    Handle<FixedArray> index6 = factory->NewFixedArray(1);
    index6->set(0, Smi::FromInt(2019));

    array = WeakArrayList::AddToEnd(isolate, array,
                                    MaybeObjectHandle::Weak(index0));
    array = WeakArrayList::AddToEnd(
        isolate, array, MaybeObjectHandle(Smi::FromInt(1), isolate));
    CHECK_EQ(array->length(), 2);

    array = WeakArrayList::AddToEnd(isolate, array,
                                    MaybeObjectHandle::Weak(index2));
    array = WeakArrayList::AddToEnd(
        isolate, array, MaybeObjectHandle(Smi::FromInt(3), isolate));
    CHECK_EQ(array->length(), 4);

    array = WeakArrayList::AddToEnd(isolate, array,
                                    MaybeObjectHandle::Weak(index4));
    array = WeakArrayList::AddToEnd(
        isolate, array, MaybeObjectHandle(Smi::FromInt(5), isolate));
    CHECK_EQ(array->length(), 6);

    array = WeakArrayList::AddToEnd(isolate, array,
                                    MaybeObjectHandle::Weak(index6));
    array = WeakArrayList::AddToEnd(
        isolate, array, MaybeObjectHandle(Smi::FromInt(7), isolate));
    CHECK_EQ(array->length(), 8);

    CHECK(InCorrectGeneration(*array));

    CHECK_EQ(array->Get(0), HeapObjectReference::Weak(*index0));
    CHECK_EQ(array->Get(1).ToSmi().value(), 1);

    CHECK_EQ(array->Get(2), HeapObjectReference::Weak(*index2));
    CHECK_EQ(array->Get(3).ToSmi().value(), 3);

    CHECK_EQ(array->Get(4), HeapObjectReference::Weak(*index4));
    CHECK_EQ(array->Get(5).ToSmi().value(), 5);

    CHECK_EQ(array->Get(6), HeapObjectReference::Weak(*index6));
    array = inner_scope.CloseAndEscape(array);
  }  // inner_scope goes out of scope.

  // The references are only cleared by the mark-compact (scavenger treats weak
  // references as strong). Thus we need to GC until the array reaches old
  // space.

  // TODO(marja): update this when/if we do handle weak references in the new
  // space.
  CcTest::CollectGarbage(NEW_SPACE);
  HeapObject heap_object;
  CHECK_EQ(array->length(), 8);
  CHECK(array->Get(0)->GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(Smi::cast(FixedArray::cast(heap_object).get(0)).value(), 2016);
  CHECK_EQ(array->Get(1).ToSmi().value(), 1);

  CHECK(array->Get(2)->GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(Smi::cast(FixedArray::cast(heap_object).get(0)).value(), 2017);
  CHECK_EQ(array->Get(3).ToSmi().value(), 3);

  CHECK(array->Get(4)->GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(Smi::cast(FixedArray::cast(heap_object).get(0)).value(), 2018);
  CHECK_EQ(array->Get(5).ToSmi().value(), 5);

  CHECK(array->Get(6)->GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(Smi::cast(FixedArray::cast(heap_object).get(0)).value(), 2019);
  CHECK_EQ(array->Get(7).ToSmi().value(), 7);

  CcTest::CollectAllGarbage();
  CHECK(heap->InOldSpace(*array));
  CHECK_EQ(array->length(), 8);
  CHECK(array->Get(0)->IsCleared());
  CHECK_EQ(array->Get(1).ToSmi().value(), 1);

  CHECK(array->Get(2)->GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(Smi::cast(FixedArray::cast(heap_object).get(0)).value(), 2017);
  CHECK_EQ(array->Get(3).ToSmi().value(), 3);

  CHECK(array->Get(4)->IsCleared());
  CHECK_EQ(array->Get(5).ToSmi().value(), 5);

  CHECK(array->Get(6)->IsCleared());
  CHECK_EQ(array->Get(7).ToSmi().value(), 7);
}

TEST(WeakArrayListRemove) {
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();
  HandleScope outer_scope(isolate);

  Handle<WeakArrayList> array(ReadOnlyRoots(heap).empty_weak_array_list(),
                              isolate);

  Handle<FixedArray> elem0 = factory->NewFixedArray(1);
  Handle<FixedArray> elem1 = factory->NewFixedArray(1);
  Handle<FixedArray> elem2 = factory->NewFixedArray(1);

  array =
      WeakArrayList::AddToEnd(isolate, array, MaybeObjectHandle::Weak(elem0));
  array =
      WeakArrayList::AddToEnd(isolate, array, MaybeObjectHandle::Weak(elem1));
  array =
      WeakArrayList::AddToEnd(isolate, array, MaybeObjectHandle::Weak(elem2));

  CHECK_EQ(array->length(), 3);
  CHECK_EQ(array->Get(0), HeapObjectReference::Weak(*elem0));
  CHECK_EQ(array->Get(1), HeapObjectReference::Weak(*elem1));
  CHECK_EQ(array->Get(2), HeapObjectReference::Weak(*elem2));

  CHECK(array->RemoveOne(MaybeObjectHandle::Weak(elem1)));

  CHECK_EQ(array->length(), 2);
  CHECK_EQ(array->Get(0), HeapObjectReference::Weak(*elem0));
  CHECK_EQ(array->Get(1), HeapObjectReference::Weak(*elem2));

  CHECK(!array->RemoveOne(MaybeObjectHandle::Weak(elem1)));

  CHECK_EQ(array->length(), 2);
  CHECK_EQ(array->Get(0), HeapObjectReference::Weak(*elem0));
  CHECK_EQ(array->Get(1), HeapObjectReference::Weak(*elem2));

  CHECK(array->RemoveOne(MaybeObjectHandle::Weak(elem0)));

  CHECK_EQ(array->length(), 1);
  CHECK_EQ(array->Get(0), HeapObjectReference::Weak(*elem2));

  CHECK(array->RemoveOne(MaybeObjectHandle::Weak(elem2)));

  CHECK_EQ(array->length(), 0);
}

TEST(Regress7768) {
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_turbo_inlining = false;
  if (!FLAG_incremental_marking) {
    return;
  }
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  HandleScope outer_scope(isolate);
  // Create an optimized code which will contain a weak reference to another
  // function ("f"). The weak reference is the only reference to the function.
  CompileRun(
      "function myfunc(f) { f(); } "
      "%PrepareFunctionForOptimization(myfunc); "
      "(function wrapper() { "
      "   function f() {}; myfunc(f); myfunc(f); "
      "   %OptimizeFunctionOnNextCall(myfunc); myfunc(f); "
      "   %ClearFunctionFeedback(wrapper);"
      "})(); "
      "%ClearFunctionFeedback(myfunc);");

  // Do marking steps; this will store the objects pointed by myfunc for later
  // processing.
  SimulateIncrementalMarking(heap, true);

  // Deoptimize the code; now the pointers inside it will be replaced with
  // undefined, and the weak_objects_in_code is the only place pointing to the
  // function f.
  CompileRun("%DeoptimizeFunction(myfunc);");

  // The object pointed to by the weak reference won't be scavenged.
  CcTest::CollectGarbage(NEW_SPACE);

  // Make sure the memory where it's stored is invalidated, so that we'll crash
  // if we try to access it.
  HeapTester::UncommitFromSpace(heap);

  // This used to crash when processing the dead weak reference.
  CcTest::CollectAllGarbage();
}

TEST(PrototypeUsersBasic) {
  CcTest::InitializeVM();
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();
  HandleScope outer_scope(isolate);

  Handle<WeakArrayList> array(ReadOnlyRoots(heap).empty_weak_array_list(),
                              isolate);

  // Add some objects into the array.
  int index = -1;
  {
    Handle<Map> map = factory->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
    array = PrototypeUsers::Add(isolate, array, map, &index);
    CHECK_EQ(array->length(), index + 1);
  }
  CHECK_EQ(index, 1);

  int empty_index = index;
  PrototypeUsers::MarkSlotEmpty(*array, empty_index);

  // Even though we have an empty slot, we still add to the end.
  int last_index = index;
  int old_capacity = array->capacity();
  while (!array->IsFull()) {
    Handle<Map> map = factory->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
    array = PrototypeUsers::Add(isolate, array, map, &index);
    CHECK_EQ(index, last_index + 1);
    CHECK_EQ(array->length(), index + 1);
    last_index = index;
  }

  // The next addition will fill the empty slot.
  {
    Handle<Map> map = factory->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
    array = PrototypeUsers::Add(isolate, array, map, &index);
  }
  CHECK_EQ(index, empty_index);

  // The next addition will make the arrow grow again.
  {
    Handle<Map> map = factory->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
    array = PrototypeUsers::Add(isolate, array, map, &index);
    CHECK_EQ(array->length(), index + 1);
    last_index = index;
  }
  CHECK_GT(array->capacity(), old_capacity);

  // Make multiple slots empty.
  int empty_index1 = 1;
  int empty_index2 = 2;
  PrototypeUsers::MarkSlotEmpty(*array, empty_index1);
  PrototypeUsers::MarkSlotEmpty(*array, empty_index2);

  // Fill the array (still adding to the end)
  old_capacity = array->capacity();
  while (!array->IsFull()) {
    Handle<Map> map = factory->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
    array = PrototypeUsers::Add(isolate, array, map, &index);
    CHECK_EQ(index, last_index + 1);
    CHECK_EQ(array->length(), index + 1);
    last_index = index;
  }

  // Make sure we use the empty slots in (reverse) order.
  {
    Handle<Map> map = factory->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
    array = PrototypeUsers::Add(isolate, array, map, &index);
  }
  CHECK_EQ(index, empty_index2);

  {
    Handle<Map> map = factory->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
    array = PrototypeUsers::Add(isolate, array, map, &index);
  }
  CHECK_EQ(index, empty_index1);
}

namespace {

HeapObject saved_heap_object;

static void TestCompactCallback(HeapObject value, int old_index,
                                int new_index) {
  saved_heap_object = value;
  CHECK_EQ(old_index, 2);
  CHECK_EQ(new_index, 1);
}

}  // namespace

TEST(PrototypeUsersCompacted) {
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();
  HandleScope outer_scope(isolate);

  Handle<WeakArrayList> array(ReadOnlyRoots(heap).empty_weak_array_list(),
                              isolate);

  // Add some objects into the array.
  int index = -1;
  Handle<Map> map_cleared_by_user =
      factory->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
  array = PrototypeUsers::Add(isolate, array, map_cleared_by_user, &index);
  CHECK_EQ(index, 1);
  Handle<Map> live_map = factory->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
  array = PrototypeUsers::Add(isolate, array, live_map, &index);
  CHECK_EQ(index, 2);
  {
    HandleScope inner_scope(isolate);
    Handle<Map> soon_dead_map =
        factory->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
    array = PrototypeUsers::Add(isolate, array, soon_dead_map, &index);
    CHECK_EQ(index, 3);

    array = inner_scope.CloseAndEscape(array);
  }

  PrototypeUsers::MarkSlotEmpty(*array, 1);
  CcTest::CollectAllGarbage();
  CHECK(array->Get(3)->IsCleared());

  CHECK_EQ(array->length(), 3 + PrototypeUsers::kFirstIndex);
  WeakArrayList new_array =
      PrototypeUsers::Compact(array, heap, TestCompactCallback);
  CHECK_EQ(new_array.length(), 1 + PrototypeUsers::kFirstIndex);
  CHECK_EQ(saved_heap_object, *live_map);
}

}  // namespace heap
}  // namespace internal
}  // namespace v8
