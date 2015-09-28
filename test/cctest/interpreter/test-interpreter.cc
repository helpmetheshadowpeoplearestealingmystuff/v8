// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/execution.h"
#include "src/handles.h"
#include "src/interpreter/bytecode-array-builder.h"
#include "src/interpreter/interpreter.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {
namespace interpreter {


static MaybeHandle<Object> CallInterpreter(Isolate* isolate,
                                           Handle<JSFunction> function) {
  return Execution::Call(isolate, function,
                         isolate->factory()->undefined_value(), 0, nullptr);
}


template <class... A>
static MaybeHandle<Object> CallInterpreter(Isolate* isolate,
                                           Handle<JSFunction> function,
                                           A... args) {
  Handle<Object> argv[] = { args... };
  return Execution::Call(isolate, function,
                         isolate->factory()->undefined_value(), sizeof...(args),
                         argv);
}


template <class... A>
class InterpreterCallable {
 public:
  InterpreterCallable(Isolate* isolate, Handle<JSFunction> function)
      : isolate_(isolate), function_(function) {}
  virtual ~InterpreterCallable() {}

  MaybeHandle<Object> operator()(A... args) {
    return CallInterpreter(isolate_, function_, args...);
  }

 private:
  Isolate* isolate_;
  Handle<JSFunction> function_;
};


static const char* kFunctionName = "f";


class InterpreterTester {
 public:
  InterpreterTester(Isolate* isolate, const char* source,
                    MaybeHandle<BytecodeArray> bytecode,
                    MaybeHandle<TypeFeedbackVector> feedback_vector)
      : isolate_(isolate),
        source_(source),
        bytecode_(bytecode),
        feedback_vector_(feedback_vector) {
    i::FLAG_vector_stores = true;
    i::FLAG_ignition = true;
    i::FLAG_always_opt = false;
    // Set ignition filter flag via SetFlagsFromString to avoid double-free
    // (or potential leak with StrDup() based on ownership confusion).
    ScopedVector<char> ignition_filter(64);
    SNPrintF(ignition_filter, "--ignition-filter=%s", kFunctionName);
    FlagList::SetFlagsFromString(ignition_filter.start(),
                                 ignition_filter.length());
    // Ensure handler table is generated.
    isolate->interpreter()->Initialize();
  }

  InterpreterTester(Isolate* isolate, Handle<BytecodeArray> bytecode,
                    MaybeHandle<TypeFeedbackVector> feedback_vector =
                        MaybeHandle<TypeFeedbackVector>())
      : InterpreterTester(isolate, nullptr, bytecode, feedback_vector) {}


  InterpreterTester(Isolate* isolate, const char* source)
      : InterpreterTester(isolate, source, MaybeHandle<BytecodeArray>(),
                          MaybeHandle<TypeFeedbackVector>()) {}

  virtual ~InterpreterTester() {}

  template <class... A>
  InterpreterCallable<A...> GetCallable() {
    return InterpreterCallable<A...>(isolate_, GetBytecodeFunction<A...>());
  }

  static Handle<Object> NewObject(const char* script) {
    return v8::Utils::OpenHandle(*CompileRun(script));
  }

  static Handle<String> GetName(Isolate* isolate, const char* name) {
    Handle<String> result = isolate->factory()->NewStringFromAsciiChecked(name);
    return isolate->factory()->string_table()->LookupString(isolate, result);
  }

  static std::string function_name() {
    return std::string(kFunctionName);
  }

 private:
  Isolate* isolate_;
  const char* source_;
  MaybeHandle<BytecodeArray> bytecode_;
  MaybeHandle<TypeFeedbackVector> feedback_vector_;

  template <class... A>
  Handle<JSFunction> GetBytecodeFunction() {
    Handle<JSFunction> function;
    if (source_) {
      CompileRun(source_);
      Local<Function> api_function =
          Local<Function>::Cast(CcTest::global()->Get(v8_str(kFunctionName)));
      function = v8::Utils::OpenHandle(*api_function);
    } else {
      int arg_count = sizeof...(A);
      std::string source("(function " + function_name() + "(");
      for (int i = 0; i < arg_count; i++) {
        source += i == 0 ? "a" : ", a";
      }
      source += "){})";
      function = v8::Utils::OpenHandle(
          *v8::Handle<v8::Function>::Cast(CompileRun(source.c_str())));
      function->ReplaceCode(
          *isolate_->builtins()->InterpreterEntryTrampoline());
    }

    if (!bytecode_.is_null()) {
      function->shared()->set_function_data(*bytecode_.ToHandleChecked());
    }
    if (!feedback_vector_.is_null()) {
      function->shared()->set_feedback_vector(
          *feedback_vector_.ToHandleChecked());
    }
    return function;
  }

  DISALLOW_COPY_AND_ASSIGN(InterpreterTester);
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

using v8::internal::BytecodeArray;
using v8::internal::Handle;
using v8::internal::Object;
using v8::internal::Runtime;
using v8::internal::Smi;
using v8::internal::Token;
using namespace v8::internal::interpreter;

TEST(InterpreterReturn) {
  HandleAndZoneScope handles;
  Handle<Object> undefined_value =
      handles.main_isolate()->factory()->undefined_value();

  BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
  builder.set_locals_count(0);
  builder.set_parameter_count(1);
  builder.Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

  InterpreterTester tester(handles.main_isolate(), bytecode_array);
  auto callable = tester.GetCallable<>();
  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK(return_val.is_identical_to(undefined_value));
}


TEST(InterpreterLoadUndefined) {
  HandleAndZoneScope handles;
  Handle<Object> undefined_value =
      handles.main_isolate()->factory()->undefined_value();

  BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
  builder.set_locals_count(0);
  builder.set_parameter_count(1);
  builder.LoadUndefined().Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

  InterpreterTester tester(handles.main_isolate(), bytecode_array);
  auto callable = tester.GetCallable<>();
  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK(return_val.is_identical_to(undefined_value));
}


TEST(InterpreterLoadNull) {
  HandleAndZoneScope handles;
  Handle<Object> null_value = handles.main_isolate()->factory()->null_value();

  BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
  builder.set_locals_count(0);
  builder.set_parameter_count(1);
  builder.LoadNull().Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

  InterpreterTester tester(handles.main_isolate(), bytecode_array);
  auto callable = tester.GetCallable<>();
  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK(return_val.is_identical_to(null_value));
}


TEST(InterpreterLoadTheHole) {
  HandleAndZoneScope handles;
  Handle<Object> the_hole_value =
      handles.main_isolate()->factory()->the_hole_value();

  BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
  builder.set_locals_count(0);
  builder.set_parameter_count(1);
  builder.LoadTheHole().Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

  InterpreterTester tester(handles.main_isolate(), bytecode_array);
  auto callable = tester.GetCallable<>();
  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK(return_val.is_identical_to(the_hole_value));
}


TEST(InterpreterLoadTrue) {
  HandleAndZoneScope handles;
  Handle<Object> true_value = handles.main_isolate()->factory()->true_value();

  BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
  builder.set_locals_count(0);
  builder.set_parameter_count(1);
  builder.LoadTrue().Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

  InterpreterTester tester(handles.main_isolate(), bytecode_array);
  auto callable = tester.GetCallable<>();
  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK(return_val.is_identical_to(true_value));
}


TEST(InterpreterLoadFalse) {
  HandleAndZoneScope handles;
  Handle<Object> false_value = handles.main_isolate()->factory()->false_value();

  BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
  builder.set_locals_count(0);
  builder.set_parameter_count(1);
  builder.LoadFalse().Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

  InterpreterTester tester(handles.main_isolate(), bytecode_array);
  auto callable = tester.GetCallable<>();
  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK(return_val.is_identical_to(false_value));
}


TEST(InterpreterLoadLiteral) {
  HandleAndZoneScope handles;
  i::Factory* factory = handles.main_isolate()->factory();

  // Small Smis.
  for (int i = -128; i < 128; i++) {
    BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
    builder.set_locals_count(0);
    builder.set_parameter_count(1);
    builder.LoadLiteral(Smi::FromInt(i)).Return();
    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

    InterpreterTester tester(handles.main_isolate(), bytecode_array);
    auto callable = tester.GetCallable<>();
    Handle<Object> return_val = callable().ToHandleChecked();
    CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(i));
  }

  // Large Smis.
  {
    BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
    builder.set_locals_count(0);
    builder.set_parameter_count(1);
    builder.LoadLiteral(Smi::FromInt(0x12345678)).Return();
    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

    InterpreterTester tester(handles.main_isolate(), bytecode_array);
    auto callable = tester.GetCallable<>();
    Handle<Object> return_val = callable().ToHandleChecked();
    CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(0x12345678));
  }

  // Heap numbers.
  {
    BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
    builder.set_locals_count(0);
    builder.set_parameter_count(1);
    builder.LoadLiteral(factory->NewHeapNumber(-2.1e19)).Return();
    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

    InterpreterTester tester(handles.main_isolate(), bytecode_array);
    auto callable = tester.GetCallable<>();
    Handle<Object> return_val = callable().ToHandleChecked();
    CHECK_EQ(i::HeapNumber::cast(*return_val)->value(), -2.1e19);
  }

  // Strings.
  {
    BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
    builder.set_locals_count(0);
    builder.set_parameter_count(1);
    Handle<i::String> string = factory->NewStringFromAsciiChecked("String");
    builder.LoadLiteral(string).Return();
    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

    InterpreterTester tester(handles.main_isolate(), bytecode_array);
    auto callable = tester.GetCallable<>();
    Handle<Object> return_val = callable().ToHandleChecked();
    CHECK(i::String::cast(*return_val)->Equals(*string));
  }
}


TEST(InterpreterLoadStoreRegisters) {
  HandleAndZoneScope handles;
  Handle<Object> true_value = handles.main_isolate()->factory()->true_value();
  for (int i = 0; i <= Register::kMaxRegisterIndex; i++) {
    BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
    builder.set_locals_count(i + 1);
    builder.set_parameter_count(1);
    Register reg(i);
    builder.LoadTrue()
        .StoreAccumulatorInRegister(reg)
        .LoadFalse()
        .LoadAccumulatorWithRegister(reg)
        .Return();
    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

    InterpreterTester tester(handles.main_isolate(), bytecode_array);
    auto callable = tester.GetCallable<>();
    Handle<Object> return_val = callable().ToHandleChecked();
    CHECK(return_val.is_identical_to(true_value));
  }
}


TEST(InterpreterAdd) {
  HandleAndZoneScope handles;
  // TODO(rmcilroy): Do add tests for heap numbers and strings once we support
  // them.
  BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
  builder.set_locals_count(1);
  builder.set_parameter_count(1);
  Register reg(0);
  builder.LoadLiteral(Smi::FromInt(1))
      .StoreAccumulatorInRegister(reg)
      .LoadLiteral(Smi::FromInt(2))
      .BinaryOperation(Token::Value::ADD, reg)
      .Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

  InterpreterTester tester(handles.main_isolate(), bytecode_array);
  auto callable = tester.GetCallable<>();
  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(3));
}


TEST(InterpreterSub) {
  HandleAndZoneScope handles;
  // TODO(rmcilroy): Do add tests for heap numbers once we support them.
  BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
  builder.set_locals_count(1);
  builder.set_parameter_count(1);
  Register reg(0);
  builder.LoadLiteral(Smi::FromInt(5))
      .StoreAccumulatorInRegister(reg)
      .LoadLiteral(Smi::FromInt(31))
      .BinaryOperation(Token::Value::SUB, reg)
      .Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

  InterpreterTester tester(handles.main_isolate(), bytecode_array);
  auto callable = tester.GetCallable<>();
  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(-26));
}


TEST(InterpreterMul) {
  HandleAndZoneScope handles;
  // TODO(rmcilroy): Do add tests for heap numbers once we support them.
  BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
  builder.set_locals_count(1);
  builder.set_parameter_count(1);
  Register reg(0);
  builder.LoadLiteral(Smi::FromInt(111))
      .StoreAccumulatorInRegister(reg)
      .LoadLiteral(Smi::FromInt(6))
      .BinaryOperation(Token::Value::MUL, reg)
      .Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

  InterpreterTester tester(handles.main_isolate(), bytecode_array);
  auto callable = tester.GetCallable<>();
  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(666));
}


TEST(InterpreterDiv) {
  HandleAndZoneScope handles;
  // TODO(rmcilroy): Do add tests for heap numbers once we support them.
  BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
  builder.set_locals_count(1);
  builder.set_parameter_count(1);
  Register reg(0);
  builder.LoadLiteral(Smi::FromInt(-20))
      .StoreAccumulatorInRegister(reg)
      .LoadLiteral(Smi::FromInt(5))
      .BinaryOperation(Token::Value::DIV, reg)
      .Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

  InterpreterTester tester(handles.main_isolate(), bytecode_array);
  auto callable = tester.GetCallable<>();
  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(-4));
}


TEST(InterpreterMod) {
  HandleAndZoneScope handles;
  // TODO(rmcilroy): Do add tests for heap numbers once we support them.
  BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
  builder.set_locals_count(1);
  builder.set_parameter_count(1);
  Register reg(0);
  builder.LoadLiteral(Smi::FromInt(121))
      .StoreAccumulatorInRegister(reg)
      .LoadLiteral(Smi::FromInt(100))
      .BinaryOperation(Token::Value::MOD, reg)
      .Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

  InterpreterTester tester(handles.main_isolate(), bytecode_array);
  auto callable = tester.GetCallable<>();
  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(21));
}


TEST(InterpreterParameter1) {
  HandleAndZoneScope handles;
  BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
  builder.set_locals_count(0);
  builder.set_parameter_count(1);
  builder.LoadAccumulatorWithRegister(builder.Parameter(0)).Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

  InterpreterTester tester(handles.main_isolate(), bytecode_array);
  auto callable = tester.GetCallable<Handle<Object>>();

  // Check for heap objects.
  Handle<Object> true_value = handles.main_isolate()->factory()->true_value();
  Handle<Object> return_val = callable(true_value).ToHandleChecked();
  CHECK(return_val.is_identical_to(true_value));

  // Check for Smis.
  return_val = callable(Handle<Smi>(Smi::FromInt(3), handles.main_isolate()))
                   .ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(3));
}


TEST(InterpreterParameter8) {
  HandleAndZoneScope handles;
  BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
  builder.set_locals_count(0);
  builder.set_parameter_count(8);
  builder.LoadAccumulatorWithRegister(builder.Parameter(0))
      .BinaryOperation(Token::Value::ADD, builder.Parameter(1))
      .BinaryOperation(Token::Value::ADD, builder.Parameter(2))
      .BinaryOperation(Token::Value::ADD, builder.Parameter(3))
      .BinaryOperation(Token::Value::ADD, builder.Parameter(4))
      .BinaryOperation(Token::Value::ADD, builder.Parameter(5))
      .BinaryOperation(Token::Value::ADD, builder.Parameter(6))
      .BinaryOperation(Token::Value::ADD, builder.Parameter(7))
      .Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

  InterpreterTester tester(handles.main_isolate(), bytecode_array);
  typedef Handle<Object> H;
  auto callable = tester.GetCallable<H, H, H, H, H, H, H, H>();

  Handle<Smi> arg1 = Handle<Smi>(Smi::FromInt(1), handles.main_isolate());
  Handle<Smi> arg2 = Handle<Smi>(Smi::FromInt(2), handles.main_isolate());
  Handle<Smi> arg3 = Handle<Smi>(Smi::FromInt(3), handles.main_isolate());
  Handle<Smi> arg4 = Handle<Smi>(Smi::FromInt(4), handles.main_isolate());
  Handle<Smi> arg5 = Handle<Smi>(Smi::FromInt(5), handles.main_isolate());
  Handle<Smi> arg6 = Handle<Smi>(Smi::FromInt(6), handles.main_isolate());
  Handle<Smi> arg7 = Handle<Smi>(Smi::FromInt(7), handles.main_isolate());
  Handle<Smi> arg8 = Handle<Smi>(Smi::FromInt(8), handles.main_isolate());
  // Check for Smis.
  Handle<Object> return_val =
      callable(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)
          .ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(36));
}


TEST(InterpreterLoadGlobal) {
  HandleAndZoneScope handles;

  // Test loading a global.
  std::string source(
      "var global = 321;\n"
      "function " + InterpreterTester::function_name() + "() {\n"
      "  return global;\n"
      "}");
  InterpreterTester tester(handles.main_isolate(), source.c_str());
  auto callable = tester.GetCallable<>();

  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(321));
}


TEST(InterpreterCallGlobal) {
  HandleAndZoneScope handles;

  // Test calling a global function.
  std::string source(
      "function g_add(a, b) { return a + b; }\n"
      "function " + InterpreterTester::function_name() + "() {\n"
      "  return g_add(5, 10);\n"
      "}");
  InterpreterTester tester(handles.main_isolate(), source.c_str());
  auto callable = tester.GetCallable<>();

  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(15));
}


TEST(InterpreterLoadNamedProperty) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  i::Factory* factory = isolate->factory();

  i::FeedbackVectorSlotKind ic_kinds[] = {i::FeedbackVectorSlotKind::LOAD_IC};
  i::StaticFeedbackVectorSpec feedback_spec(0, 1, ic_kinds);
  Handle<i::TypeFeedbackVector> vector =
      factory->NewTypeFeedbackVector(&feedback_spec);

  Handle<i::String> name = factory->NewStringFromAsciiChecked("val");
  name = factory->string_table()->LookupString(isolate, name);

  BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
  builder.set_locals_count(0);
  builder.set_parameter_count(1);
  builder.LoadLiteral(name)
      .LoadNamedProperty(builder.Parameter(0), vector->first_ic_slot_index(),
                         i::SLOPPY)
      .Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

  InterpreterTester tester(handles.main_isolate(), bytecode_array, vector);
  auto callable = tester.GetCallable<Handle<Object>>();

  Handle<Object> object = InterpreterTester::NewObject("({ val : 123 })");
  // Test IC miss.
  Handle<Object> return_val = callable(object).ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(123));

  // Test transition to monomorphic IC.
  return_val = callable(object).ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(123));

  // Test transition to polymorphic IC.
  Handle<Object> object2 =
      InterpreterTester::NewObject("({ val : 456, other : 123 })");
  return_val = callable(object2).ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(456));

  // Test transition to megamorphic IC.
  Handle<Object> object3 =
      InterpreterTester::NewObject("({ val : 789, val2 : 123 })");
  callable(object3).ToHandleChecked();
  Handle<Object> object4 =
      InterpreterTester::NewObject("({ val : 789, val3 : 123 })");
  callable(object4).ToHandleChecked();
  Handle<Object> object5 =
      InterpreterTester::NewObject("({ val : 789, val4 : 123 })");
  return_val = callable(object5).ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(789));
}


TEST(InterpreterLoadKeyedProperty) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  i::Factory* factory = isolate->factory();

  i::FeedbackVectorSlotKind ic_kinds[] = {
      i::FeedbackVectorSlotKind::KEYED_LOAD_IC};
  i::StaticFeedbackVectorSpec feedback_spec(0, 1, ic_kinds);
  Handle<i::TypeFeedbackVector> vector =
      factory->NewTypeFeedbackVector(&feedback_spec);

  Handle<i::String> key = factory->NewStringFromAsciiChecked("key");
  key = factory->string_table()->LookupString(isolate, key);

  BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
  builder.set_locals_count(1);
  builder.set_parameter_count(1);
  builder.LoadLiteral(key)
      .LoadKeyedProperty(builder.Parameter(0), vector->first_ic_slot_index(),
                         i::SLOPPY)
      .Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

  InterpreterTester tester(handles.main_isolate(), bytecode_array, vector);
  auto callable = tester.GetCallable<Handle<Object>>();

  Handle<Object> object = InterpreterTester::NewObject("({ key : 123 })");
  // Test IC miss.
  Handle<Object> return_val = callable(object).ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(123));

  // Test transition to monomorphic IC.
  return_val = callable(object).ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(123));

  // Test transition to megamorphic IC.
  Handle<Object> object3 =
      InterpreterTester::NewObject("({ key : 789, val2 : 123 })");
  return_val = callable(object3).ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(789));
}


TEST(InterpreterStoreNamedProperty) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  i::Factory* factory = isolate->factory();

  i::FeedbackVectorSlotKind ic_kinds[] = {i::FeedbackVectorSlotKind::STORE_IC};
  i::StaticFeedbackVectorSpec feedback_spec(0, 1, ic_kinds);
  Handle<i::TypeFeedbackVector> vector =
      factory->NewTypeFeedbackVector(&feedback_spec);

  Handle<i::String> name = factory->NewStringFromAsciiChecked("val");
  name = factory->string_table()->LookupString(isolate, name);

  BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
  builder.set_locals_count(1);
  builder.set_parameter_count(1);
  builder.LoadLiteral(name)
      .StoreAccumulatorInRegister(Register(0))
      .LoadLiteral(Smi::FromInt(999))
      .StoreNamedProperty(builder.Parameter(0), Register(0),
                          vector->first_ic_slot_index(), i::SLOPPY)
      .Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

  InterpreterTester tester(isolate, bytecode_array, vector);
  auto callable = tester.GetCallable<Handle<Object>>();
  Handle<Object> object = InterpreterTester::NewObject("({ val : 123 })");
  // Test IC miss.
  Handle<Object> result;
  callable(object).ToHandleChecked();
  CHECK(Runtime::GetObjectProperty(isolate, object, name).ToHandle(&result));
  CHECK_EQ(Smi::cast(*result), Smi::FromInt(999));

  // Test transition to monomorphic IC.
  callable(object).ToHandleChecked();
  CHECK(Runtime::GetObjectProperty(isolate, object, name).ToHandle(&result));
  CHECK_EQ(Smi::cast(*result), Smi::FromInt(999));

  // Test transition to polymorphic IC.
  Handle<Object> object2 =
      InterpreterTester::NewObject("({ val : 456, other : 123 })");
  callable(object2).ToHandleChecked();
  CHECK(Runtime::GetObjectProperty(isolate, object2, name).ToHandle(&result));
  CHECK_EQ(Smi::cast(*result), Smi::FromInt(999));

  // Test transition to megamorphic IC.
  Handle<Object> object3 =
      InterpreterTester::NewObject("({ val : 789, val2 : 123 })");
  callable(object3).ToHandleChecked();
  Handle<Object> object4 =
      InterpreterTester::NewObject("({ val : 789, val3 : 123 })");
  callable(object4).ToHandleChecked();
  Handle<Object> object5 =
      InterpreterTester::NewObject("({ val : 789, val4 : 123 })");
  callable(object5).ToHandleChecked();
  CHECK(Runtime::GetObjectProperty(isolate, object5, name).ToHandle(&result));
  CHECK_EQ(Smi::cast(*result), Smi::FromInt(999));
}


TEST(InterpreterStoreKeyedProperty) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  i::Factory* factory = isolate->factory();

  i::FeedbackVectorSlotKind ic_kinds[] = {
      i::FeedbackVectorSlotKind::KEYED_STORE_IC};
  i::StaticFeedbackVectorSpec feedback_spec(0, 1, ic_kinds);
  Handle<i::TypeFeedbackVector> vector =
      factory->NewTypeFeedbackVector(&feedback_spec);

  Handle<i::String> name = factory->NewStringFromAsciiChecked("val");
  name = factory->string_table()->LookupString(isolate, name);

  BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
  builder.set_locals_count(1);
  builder.set_parameter_count(1);
  builder.LoadLiteral(name)
      .StoreAccumulatorInRegister(Register(0))
      .LoadLiteral(Smi::FromInt(999))
      .StoreKeyedProperty(builder.Parameter(0), Register(0),
                          vector->first_ic_slot_index(), i::SLOPPY)
      .Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

  InterpreterTester tester(isolate, bytecode_array, vector);
  auto callable = tester.GetCallable<Handle<Object>>();
  Handle<Object> object = InterpreterTester::NewObject("({ val : 123 })");
  // Test IC miss.
  Handle<Object> result;
  callable(object).ToHandleChecked();
  CHECK(Runtime::GetObjectProperty(isolate, object, name).ToHandle(&result));
  CHECK_EQ(Smi::cast(*result), Smi::FromInt(999));

  // Test transition to monomorphic IC.
  callable(object).ToHandleChecked();
  CHECK(Runtime::GetObjectProperty(isolate, object, name).ToHandle(&result));
  CHECK_EQ(Smi::cast(*result), Smi::FromInt(999));

  // Test transition to megamorphic IC.
  Handle<Object> object2 =
      InterpreterTester::NewObject("({ val : 456, other : 123 })");
  callable(object2).ToHandleChecked();
  CHECK(Runtime::GetObjectProperty(isolate, object2, name).ToHandle(&result));
  CHECK_EQ(Smi::cast(*result), Smi::FromInt(999));
}


TEST(InterpreterCall) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  i::Factory* factory = isolate->factory();

  i::FeedbackVectorSlotKind ic_kinds[] = {i::FeedbackVectorSlotKind::LOAD_IC};
  i::StaticFeedbackVectorSpec feedback_spec(0, 1, ic_kinds);
  Handle<i::TypeFeedbackVector> vector =
      factory->NewTypeFeedbackVector(&feedback_spec);

  Handle<i::String> name = factory->NewStringFromAsciiChecked("func");
  name = factory->string_table()->LookupString(isolate, name);

  // Check with no args.
  {
    BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
    builder.set_locals_count(1);
    builder.set_parameter_count(1);
    builder.LoadLiteral(name)
        .LoadNamedProperty(builder.Parameter(0), vector->first_ic_slot_index(),
                           i::SLOPPY)
        .StoreAccumulatorInRegister(Register(0))
        .Call(Register(0), builder.Parameter(0), 0)
        .Return();
    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

    InterpreterTester tester(handles.main_isolate(), bytecode_array, vector);
    auto callable = tester.GetCallable<Handle<Object>>();

    Handle<Object> object = InterpreterTester::NewObject(
        "new (function Obj() { this.func = function() { return 0x265; }})()");
    Handle<Object> return_val = callable(object).ToHandleChecked();
    CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(0x265));
  }

  // Check that receiver is passed properly.
  {
    BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
    builder.set_locals_count(1);
    builder.set_parameter_count(1);
    builder.LoadLiteral(name)
        .LoadNamedProperty(builder.Parameter(0), vector->first_ic_slot_index(),
                           i::SLOPPY)
        .StoreAccumulatorInRegister(Register(0))
        .Call(Register(0), builder.Parameter(0), 0)
        .Return();
    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

    InterpreterTester tester(handles.main_isolate(), bytecode_array, vector);
    auto callable = tester.GetCallable<Handle<Object>>();

    Handle<Object> object = InterpreterTester::NewObject(
        "new (function Obj() {"
        "  this.val = 1234;"
        "  this.func = function() { return this.val; };"
        "})()");
    Handle<Object> return_val = callable(object).ToHandleChecked();
    CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(1234));
  }

  // Check with two parameters (+ receiver).
  {
    BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
    builder.set_locals_count(4);
    builder.set_parameter_count(1);
    builder.LoadLiteral(name)
        .LoadNamedProperty(builder.Parameter(0), vector->first_ic_slot_index(),
                           i::SLOPPY)
        .StoreAccumulatorInRegister(Register(0))
        .LoadAccumulatorWithRegister(builder.Parameter(0))
        .StoreAccumulatorInRegister(Register(1))
        .LoadLiteral(Smi::FromInt(51))
        .StoreAccumulatorInRegister(Register(2))
        .LoadLiteral(Smi::FromInt(11))
        .StoreAccumulatorInRegister(Register(3))
        .Call(Register(0), Register(1), 2)
        .Return();
    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

    InterpreterTester tester(handles.main_isolate(), bytecode_array, vector);
    auto callable = tester.GetCallable<Handle<Object>>();

    Handle<Object> object = InterpreterTester::NewObject(
        "new (function Obj() { "
        "  this.func = function(a, b) { return a - b; }"
        "})()");
    Handle<Object> return_val = callable(object).ToHandleChecked();
    CHECK(return_val->SameValue(Smi::FromInt(40)));
  }

  // Check with 10 parameters (+ receiver).
  {
    BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
    builder.set_locals_count(12);
    builder.set_parameter_count(1);
    builder.LoadLiteral(name)
        .LoadNamedProperty(builder.Parameter(0), vector->first_ic_slot_index(),
                           i::SLOPPY)
        .StoreAccumulatorInRegister(Register(0))
        .LoadAccumulatorWithRegister(builder.Parameter(0))
        .StoreAccumulatorInRegister(Register(1))
        .LoadLiteral(factory->NewStringFromAsciiChecked("a"))
        .StoreAccumulatorInRegister(Register(2))
        .LoadLiteral(factory->NewStringFromAsciiChecked("b"))
        .StoreAccumulatorInRegister(Register(3))
        .LoadLiteral(factory->NewStringFromAsciiChecked("c"))
        .StoreAccumulatorInRegister(Register(4))
        .LoadLiteral(factory->NewStringFromAsciiChecked("d"))
        .StoreAccumulatorInRegister(Register(5))
        .LoadLiteral(factory->NewStringFromAsciiChecked("e"))
        .StoreAccumulatorInRegister(Register(6))
        .LoadLiteral(factory->NewStringFromAsciiChecked("f"))
        .StoreAccumulatorInRegister(Register(7))
        .LoadLiteral(factory->NewStringFromAsciiChecked("g"))
        .StoreAccumulatorInRegister(Register(8))
        .LoadLiteral(factory->NewStringFromAsciiChecked("h"))
        .StoreAccumulatorInRegister(Register(9))
        .LoadLiteral(factory->NewStringFromAsciiChecked("i"))
        .StoreAccumulatorInRegister(Register(10))
        .LoadLiteral(factory->NewStringFromAsciiChecked("j"))
        .StoreAccumulatorInRegister(Register(11))
        .Call(Register(0), Register(1), 10)
        .Return();
    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();

    InterpreterTester tester(handles.main_isolate(), bytecode_array, vector);
    auto callable = tester.GetCallable<Handle<Object>>();

    Handle<Object> object = InterpreterTester::NewObject(
        "new (function Obj() { "
        "  this.prefix = \"prefix_\";"
        "  this.func = function(a, b, c, d, e, f, g, h, i, j) {"
        "      return this.prefix + a + b + c + d + e + f + g + h + i + j;"
        "  }"
        "})()");
    Handle<Object> return_val = callable(object).ToHandleChecked();
    Handle<i::String> expected =
        factory->NewStringFromAsciiChecked("prefix_abcdefghij");
    CHECK(i::String::cast(*return_val)->Equals(*expected));
  }
}


static BytecodeArrayBuilder& SetRegister(BytecodeArrayBuilder& builder,
                                         Register reg, int value,
                                         Register scratch) {
  return builder.StoreAccumulatorInRegister(scratch)
      .LoadLiteral(Smi::FromInt(value))
      .StoreAccumulatorInRegister(reg)
      .LoadAccumulatorWithRegister(scratch);
}


static BytecodeArrayBuilder& IncrementRegister(BytecodeArrayBuilder& builder,
                                               Register reg, int value,
                                               Register scratch) {
  return builder.StoreAccumulatorInRegister(scratch)
      .LoadLiteral(Smi::FromInt(value))
      .BinaryOperation(Token::Value::ADD, reg)
      .StoreAccumulatorInRegister(reg)
      .LoadAccumulatorWithRegister(scratch);
}


TEST(InterpreterJumps) {
  HandleAndZoneScope handles;
  BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
  builder.set_locals_count(2);
  builder.set_parameter_count(0);
  Register reg(0), scratch(1);
  BytecodeLabel label[3];

  builder.LoadLiteral(Smi::FromInt(0))
      .StoreAccumulatorInRegister(reg)
      .Jump(&label[1]);
  SetRegister(builder, reg, 1024, scratch).Bind(&label[0]);
  IncrementRegister(builder, reg, 1, scratch).Jump(&label[2]);
  SetRegister(builder, reg, 2048, scratch).Bind(&label[1]);
  IncrementRegister(builder, reg, 2, scratch).Jump(&label[0]);
  SetRegister(builder, reg, 4096, scratch).Bind(&label[2]);
  IncrementRegister(builder, reg, 4, scratch)
      .LoadAccumulatorWithRegister(reg)
      .Return();

  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();
  InterpreterTester tester(handles.main_isolate(), bytecode_array);
  auto callable = tester.GetCallable<>();
  Handle<Object> return_value = callable().ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_value)->value(), 7);
}


TEST(InterpreterConditionalJumps) {
  HandleAndZoneScope handles;
  BytecodeArrayBuilder builder(handles.main_isolate(), handles.main_zone());
  builder.set_locals_count(2);
  builder.set_parameter_count(0);
  Register reg(0), scratch(1);
  BytecodeLabel label[2];
  BytecodeLabel done, done1;

  builder.LoadLiteral(Smi::FromInt(0))
      .StoreAccumulatorInRegister(reg)
      .LoadFalse()
      .JumpIfFalse(&label[0]);
  IncrementRegister(builder, reg, 1024, scratch)
      .Bind(&label[0])
      .LoadTrue()
      .JumpIfFalse(&done);
  IncrementRegister(builder, reg, 1, scratch).LoadTrue().JumpIfTrue(&label[1]);
  IncrementRegister(builder, reg, 2048, scratch).Bind(&label[1]);
  IncrementRegister(builder, reg, 2, scratch).LoadFalse().JumpIfTrue(&done1);
  IncrementRegister(builder, reg, 4, scratch)
      .LoadAccumulatorWithRegister(reg)
      .Bind(&done)
      .Bind(&done1)
      .Return();

  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray();
  InterpreterTester tester(handles.main_isolate(), bytecode_array);
  auto callable = tester.GetCallable<>();
  Handle<Object> return_value = callable().ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_value)->value(), 7);
}
