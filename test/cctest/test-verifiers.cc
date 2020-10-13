// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These tests check that Torque-generated verifier functions crash the process
// when encountering data that doesn't fit the Torque type definitions.

#include "src/api/api-inl.h"
#include "src/objects/descriptor-array.h"
#include "src/objects/map-inl.h"
#include "test/cctest/cctest.h"
#include "torque-generated/class-verifiers.h"

namespace v8 {
namespace internal {

// Defines a pair of tests with similar code. The goal is to test that a
// specific action causes a failure, but that everything else in the test case
// succeeds. The general pattern should be:
//
// TEST_PAIR(Something) {
//   do_setup_steps_that_always_succeed();
//   if (should_fail) {
//     do_the_step_that_fails();
//   }
//   do_teardown_steps_that_always_succeed();
// }
//
// A corresponding entry in cctest.status specifies that all Fail* tests in this
// file must fail.
#define TEST_PAIR(Name)               \
  static void Name(bool should_fail); \
  TEST(Pass##Name) { Name(false); }   \
  TEST(Fail##Name) { Name(true); }    \
  static void Name(bool should_fail)

#ifdef VERIFY_HEAP

TEST_PAIR(TestWrongTypeInNormalField) {
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  v8::HandleScope scope(isolate);
  v8::Local<v8::Value> v = CompileRun("({a: 3, b: 4})");
  Handle<JSObject> o = Handle<JSObject>::cast(v8::Utils::OpenHandle(*v));
  Handle<Object> original_elements(
      TaggedField<Object>::load(*o, JSObject::kElementsOffset), i_isolate);
  CHECK(original_elements->IsFixedArrayBase());

  // There must be no GC (and therefore no verifiers running) until we can
  // restore the modified data.
  DisallowHeapAllocation no_gc;

  // Elements must be FixedArrayBase according to the Torque definition, so a
  // JSObject should cause a failure.
  TaggedField<Object>::store(*o, JSObject::kElementsOffset, *o);
  if (should_fail) {
    TorqueGeneratedClassVerifiers::JSObjectVerify(*o, i_isolate);
  }

  // Put back the original value in case verifiers run on test shutdown.
  TaggedField<Object>::store(*o, JSObject::kElementsOffset, *original_elements);
}

TEST_PAIR(TestWrongStrongTypeInIndexedStructField) {
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  v8::HandleScope scope(isolate);
  v8::Local<v8::Value> v = CompileRun("({a: 3, b: 4})");
  Handle<Object> o = v8::Utils::OpenHandle(*v);
  Handle<Map> map(Handle<HeapObject>::cast(o)->map(), i_isolate);
  Handle<DescriptorArray> descriptors(map->instance_descriptors(kRelaxedLoad),
                                      i_isolate);
  int offset = DescriptorArray::OffsetOfDescriptorAt(1) +
               DescriptorArray::kEntryKeyOffset;
  Handle<Object> original_key(TaggedField<Object>::load(*descriptors, offset),
                              i_isolate);
  CHECK(original_key->IsString());

  // There must be no GC (and therefore no verifiers running) until we can
  // restore the modified data.
  DisallowHeapAllocation no_gc;

  // Key must be Name|Undefined according to the Torque definition, so a
  // JSObject should cause a failure.
  TaggedField<Object>::store(*descriptors, offset, *o);
  if (should_fail) {
    TorqueGeneratedClassVerifiers::DescriptorArrayVerify(*descriptors,
                                                         i_isolate);
  }

  // Put back the original value in case verifiers run on test shutdown.
  TaggedField<Object>::store(*descriptors, offset, *original_key);
}

TEST_PAIR(TestWrongWeakTypeInIndexedStructField) {
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  v8::HandleScope scope(isolate);
  v8::Local<v8::Value> v = CompileRun("({a: 3, b: 4})");
  Handle<Object> o = v8::Utils::OpenHandle(*v);
  Handle<Map> map(Handle<HeapObject>::cast(o)->map(), i_isolate);
  Handle<DescriptorArray> descriptors(map->instance_descriptors(kRelaxedLoad),
                                      i_isolate);
  int offset = DescriptorArray::OffsetOfDescriptorAt(0) +
               DescriptorArray::kEntryValueOffset;
  Handle<Object> original_value(TaggedField<Object>::load(*descriptors, offset),
                                i_isolate);

  // There must be no GC (and therefore no verifiers running) until we can
  // restore the modified data.
  DisallowHeapAllocation no_gc;

  // Value can be JSAny, which includes JSObject, and it can be Weak<Map>, but
  // it can't be Weak<JSObject>.
  TaggedField<Object>::store(*descriptors, offset, *o);
  TorqueGeneratedClassVerifiers::DescriptorArrayVerify(*descriptors, i_isolate);
  MaybeObject weak = MaybeObject::MakeWeak(MaybeObject::FromObject(*o));
  TaggedField<MaybeObject>::store(*descriptors, offset, weak);
  if (should_fail) {
    TorqueGeneratedClassVerifiers::DescriptorArrayVerify(*descriptors,
                                                         i_isolate);
  }

  // Put back the original value in case verifiers run on test shutdown.
  TaggedField<Object>::store(*descriptors, offset, *original_value);
}

#endif  // VERIFY_HEAP

#undef TEST_PAIR

}  // namespace internal
}  // namespace v8
