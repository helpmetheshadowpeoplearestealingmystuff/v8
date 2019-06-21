// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "src/init/v8.h"
#include "test/cctest/cctest.h"

#include "src/heap/heap.h"
#include "src/objects/objects-inl.h"
#include "src/objects/objects.h"

namespace v8 {
namespace internal {

void TestArrayBufferViewContents(LocalContext& env, bool should_use_buffer) {
  v8::Local<v8::Object> obj_a = v8::Local<v8::Object>::Cast(
      env->Global()
          ->Get(env->GetIsolate()->GetCurrentContext(), v8_str("a"))
          .ToLocalChecked());
  CHECK(obj_a->IsArrayBufferView());
  v8::Local<v8::ArrayBufferView> array_buffer_view =
      v8::Local<v8::ArrayBufferView>::Cast(obj_a);
  CHECK_EQ(array_buffer_view->HasBuffer(), should_use_buffer);
  unsigned char contents[4] = {23, 23, 23, 23};
  CHECK_EQ(sizeof(contents),
           array_buffer_view->CopyContents(contents, sizeof(contents)));
  CHECK_EQ(array_buffer_view->HasBuffer(), should_use_buffer);
  for (size_t i = 0; i < sizeof(contents); ++i) {
    CHECK_EQ(i, contents[i]);
  }
}


TEST(CopyContentsTypedArray) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  CompileRun(
      "var a = new Uint8Array(4);"
      "a[0] = 0;"
      "a[1] = 1;"
      "a[2] = 2;"
      "a[3] = 3;");
  TestArrayBufferViewContents(env, false);
}


TEST(CopyContentsArray) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  CompileRun("var a = new Uint8Array([0, 1, 2, 3]);");
  TestArrayBufferViewContents(env, false);
}


TEST(CopyContentsView) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  CompileRun(
      "var b = new ArrayBuffer(6);"
      "var c = new Uint8Array(b);"
      "c[0] = -1;"
      "c[1] = -1;"
      "c[2] = 0;"
      "c[3] = 1;"
      "c[4] = 2;"
      "c[5] = 3;"
      "var a = new DataView(b, 2);");
  TestArrayBufferViewContents(env, true);
}


TEST(AllocateNotExternal) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  void* memory = reinterpret_cast<Isolate*>(env->GetIsolate())
                     ->array_buffer_allocator()
                     ->Allocate(1024);
  v8::Local<v8::ArrayBuffer> buffer =
      v8::ArrayBuffer::New(env->GetIsolate(), memory, 1024,
                           v8::ArrayBufferCreationMode::kInternalized);
  CHECK(!buffer->IsExternal());
  CHECK_EQ(memory, buffer->GetContents().Data());
}

void TestSpeciesProtector(char* code,
                          bool invalidates_species_protector = true) {
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  std::string typed_array_constructors[] = {
#define TYPED_ARRAY_CTOR(Type, type, TYPE, ctype) #Type "Array",

      TYPED_ARRAYS(TYPED_ARRAY_CTOR)
#undef TYPED_ARRAY_CTOR
  };

  for (auto& constructor : typed_array_constructors) {
    v8::Isolate* isolate = v8::Isolate::New(create_params);
    isolate->Enter();
    {
      LocalContext context(isolate);
      v8::HandleScope scope(isolate);
      v8::TryCatch try_catch(isolate);

      CompileRun(("let x = new " + constructor + "();").c_str());
      CompileRun(("let constructor = " + constructor + ";").c_str());
      v8::Local<v8::Value> constructor_obj = CompileRun(constructor.c_str());
      CHECK_EQ(constructor_obj, CompileRun("x.slice().constructor"));
      CHECK_EQ(constructor_obj, CompileRun("x.subarray().constructor"));
      CHECK_EQ(constructor_obj, CompileRun("x.map(()=>{}).constructor"));
      std::string decl = "class MyTypedArray extends " + constructor + " { }";
      CompileRun(decl.c_str());

      v8::internal::Isolate* i_isolate =
          reinterpret_cast<v8::internal::Isolate*>(isolate);
      CHECK(i_isolate->IsTypedArraySpeciesLookupChainIntact());
      CompileRun(code);
      if (invalidates_species_protector) {
        CHECK(!i_isolate->IsTypedArraySpeciesLookupChainIntact());
      } else {
        CHECK(i_isolate->IsTypedArraySpeciesLookupChainIntact());
      }

      v8::Local<v8::Value> my_typed_array = CompileRun("MyTypedArray");
      CHECK_EQ(my_typed_array, CompileRun("x.slice().constructor"));
      CHECK_EQ(my_typed_array, CompileRun("x.subarray().constructor"));
      CHECK_EQ(my_typed_array, CompileRun("x.map(()=>{}).constructor"));
    }
    isolate->Exit();
    isolate->Dispose();
  }
}

UNINITIALIZED_TEST(SpeciesConstructor) {
  char code[] = "x.constructor = MyTypedArray";
  TestSpeciesProtector(code);
}

UNINITIALIZED_TEST(SpeciesConstructorAccessor) {
  char code[] =
      "Object.defineProperty(x, 'constructor',{get() {return MyTypedArray;}})";
  TestSpeciesProtector(code);
}

UNINITIALIZED_TEST(SpeciesModified) {
  char code[] =
      "Object.defineProperty(constructor, Symbol.species, "
      "{value:MyTypedArray})";
  TestSpeciesProtector(code);
}

UNINITIALIZED_TEST(SpeciesParentConstructor) {
  char code[] = "constructor.prototype.constructor = MyTypedArray";
  TestSpeciesProtector(code);
}

UNINITIALIZED_TEST(SpeciesProto) {
  char code[] = "x.__proto__ = MyTypedArray.prototype";
  TestSpeciesProtector(code, false);
}

}  // namespace internal
}  // namespace v8
