// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {

using EnumIndexOverflowTest = TestWithNativeContextAndZone;

TEST_F(EnumIndexOverflowTest, GlobalObject) {
  Handle<GlobalDictionary> dictionary(
      isolate()->global_object()->global_dictionary(), isolate());
  dictionary->set_next_enumeration_index(
      PropertyDetails::DictionaryStorageField::kMax);
  Handle<Object> value(Smi::FromInt(static_cast<int>(42)), isolate());
  Handle<Name> name = factory()->InternalizeUtf8String("eeeee");
  JSObject::AddProperty(isolate(), isolate()->global_object(), name, value,
                        NONE);
}

}  // namespace internal
}  // namespace v8
