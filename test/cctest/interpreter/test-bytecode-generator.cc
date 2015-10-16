// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/compiler.h"
#include "src/interpreter/bytecode-array-iterator.h"
#include "src/interpreter/bytecode-generator.h"
#include "src/interpreter/interpreter.h"
#include "test/cctest/cctest.h"
#include "test/cctest/test-feedback-vector.h"

namespace v8 {
namespace internal {
namespace interpreter {

class BytecodeGeneratorHelper {
 public:
  const char* kFunctionName = "f";

  static const int kLastParamIndex =
      -InterpreterFrameConstants::kLastParamFromRegisterPointer / kPointerSize;

  BytecodeGeneratorHelper() {
    i::FLAG_vector_stores = true;
    i::FLAG_ignition = true;
    i::FLAG_ignition_fake_try_catch = true;
    i::FLAG_ignition_filter = StrDup(kFunctionName);
    i::FLAG_always_opt = false;
    i::FLAG_allow_natives_syntax = true;
    CcTest::i_isolate()->interpreter()->Initialize();
  }

  Isolate* isolate() { return CcTest::i_isolate(); }
  Factory* factory() { return CcTest::i_isolate()->factory(); }


  Handle<BytecodeArray> MakeTopLevelBytecode(const char* source) {
    const char* old_ignition_filter = i::FLAG_ignition_filter;
    i::FLAG_ignition_filter = "*";
    Local<v8::Script> script = v8_compile(source);
    i::FLAG_ignition_filter = old_ignition_filter;
    i::Handle<i::JSFunction> js_function = v8::Utils::OpenHandle(*script);
    return handle(js_function->shared()->bytecode_array(), CcTest::i_isolate());
  }


  Handle<BytecodeArray> MakeBytecode(const char* script,
                                     const char* function_name) {
    CompileRun(script);
    Local<Function> function =
        Local<Function>::Cast(CcTest::global()->Get(v8_str(function_name)));
    i::Handle<i::JSFunction> js_function = v8::Utils::OpenHandle(*function);
    return handle(js_function->shared()->bytecode_array(), CcTest::i_isolate());
  }


  Handle<BytecodeArray> MakeBytecodeForFunctionBody(const char* body) {
    ScopedVector<char> program(1024);
    SNPrintF(program, "function %s() { %s }\n%s();", kFunctionName, body,
             kFunctionName);
    return MakeBytecode(program.start(), kFunctionName);
  }

  Handle<BytecodeArray> MakeBytecodeForFunction(const char* function) {
    ScopedVector<char> program(1024);
    SNPrintF(program, "%s\n%s();", function, kFunctionName);
    return MakeBytecode(program.start(), kFunctionName);
  }
};


// Helper macros for handcrafting bytecode sequences.
#define B(x) static_cast<uint8_t>(Bytecode::k##x)
#define U8(x) static_cast<uint8_t>((x) & 0xff)
#define R(x) static_cast<uint8_t>(-(x) & 0xff)
#define _ static_cast<uint8_t>(0x5a)
#if defined(V8_TARGET_LITTLE_ENDIAN)
#define U16(x) static_cast<uint8_t>((x) & 0xff),                    \
               static_cast<uint8_t>(((x) >> kBitsPerByte) & 0xff)
#elif defined(V8_TARGET_BIG_ENDIAN)
#define U16(x) static_cast<uint8_t>(((x) >> kBitsPerByte) & 0xff),   \
               static_cast<uint8_t>((x) & 0xff)
#else
#error Unknown byte ordering
#endif


// Structure for containing expected bytecode snippets.
template<typename T>
struct ExpectedSnippet {
  const char* code_snippet;
  int frame_size;
  int parameter_count;
  int bytecode_length;
  const uint8_t bytecode[512];
  int constant_count;
  T constants[6];
};


static void CheckConstant(int expected, Object* actual) {
  CHECK_EQ(expected, Smi::cast(actual)->value());
}


static void CheckConstant(double expected, Object* actual) {
  CHECK_EQ(expected, HeapNumber::cast(actual)->value());
}


static void CheckConstant(const char* expected, Object* actual) {
  Handle<String> expected_string =
      CcTest::i_isolate()->factory()->NewStringFromAsciiChecked(expected);
  CHECK(String::cast(actual)->Equals(*expected_string));
}


static void CheckConstant(Handle<Object> expected, Object* actual) {
  CHECK(actual == *expected || expected->StrictEquals(actual));
}


static void CheckConstant(InstanceType expected, Object* actual) {
  CHECK_EQ(expected, HeapObject::cast(actual)->map()->instance_type());
}


template <typename T>
static void CheckBytecodeArrayEqual(struct ExpectedSnippet<T> expected,
                                    Handle<BytecodeArray> actual,
                                    bool has_unknown = false) {
  CHECK_EQ(actual->frame_size(), expected.frame_size);
  CHECK_EQ(actual->parameter_count(), expected.parameter_count);
  CHECK_EQ(actual->length(), expected.bytecode_length);
  if (expected.constant_count == 0) {
    CHECK_EQ(actual->constant_pool(), CcTest::heap()->empty_fixed_array());
  } else {
    CHECK_EQ(actual->constant_pool()->length(), expected.constant_count);
    for (int i = 0; i < expected.constant_count; i++) {
      CheckConstant(expected.constants[i], actual->constant_pool()->get(i));
    }
  }

  BytecodeArrayIterator iterator(actual);
  int i = 0;
  while (!iterator.done()) {
    int bytecode_index = i++;
    Bytecode bytecode = iterator.current_bytecode();
    if (Bytecodes::ToByte(bytecode) != expected.bytecode[bytecode_index]) {
      std::ostringstream stream;
      stream << "Check failed: expected bytecode [" << bytecode_index
             << "] to be " << Bytecodes::ToString(static_cast<Bytecode>(
                                  expected.bytecode[bytecode_index]))
             << " but got " << Bytecodes::ToString(bytecode);
      FATAL(stream.str().c_str());
    }
    for (int j = 0; j < Bytecodes::NumberOfOperands(bytecode); ++j) {
      OperandType operand_type = Bytecodes::GetOperandType(bytecode, j);
      int operand_index = i;
      i += static_cast<int>(Bytecodes::SizeOfOperand(operand_type));
      uint32_t raw_operand = iterator.GetRawOperand(j, operand_type);
      if (has_unknown) {
        // Check actual bytecode array doesn't have the same byte as the
        // one we use to specify an unknown byte.
        CHECK_NE(raw_operand, _);
        if (expected.bytecode[operand_index] == _) {
          continue;
        }
      }
      uint32_t expected_operand;
      switch (Bytecodes::SizeOfOperand(operand_type)) {
        case OperandSize::kNone:
          UNREACHABLE();
          return;
        case OperandSize::kByte:
          expected_operand =
              static_cast<uint32_t>(expected.bytecode[operand_index]);
          break;
        case OperandSize::kShort:
          expected_operand = Bytecodes::ShortOperandFromBytes(
              &expected.bytecode[operand_index]);
          break;
        default:
          UNREACHABLE();
          return;
      }
      if (raw_operand != expected_operand) {
        std::ostringstream stream;
        stream << "Check failed: expected operand [" << j << "] for bytecode ["
               << bytecode_index << "] to be "
               << static_cast<unsigned int>(expected_operand) << " but got "
               << static_cast<unsigned int>(raw_operand);
        FATAL(stream.str().c_str());
      }
    }
    iterator.Advance();
  }
}


TEST(PrimitiveReturnStatements) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  ExpectedSnippet<int> snippets[] = {
      {"", 0, 1, 2, {B(LdaUndefined), B(Return)}, 0},
      {"return;", 0, 1, 2, {B(LdaUndefined), B(Return)}, 0},
      {"return null;", 0, 1, 2, {B(LdaNull), B(Return)}, 0},
      {"return true;", 0, 1, 2, {B(LdaTrue), B(Return)}, 0},
      {"return false;", 0, 1, 2, {B(LdaFalse), B(Return)}, 0},
      {"return 0;", 0, 1, 2, {B(LdaZero), B(Return)}, 0},
      {"return +1;", 0, 1, 3, {B(LdaSmi8), U8(1), B(Return)}, 0},
      {"return -1;", 0, 1, 3, {B(LdaSmi8), U8(-1), B(Return)}, 0},
      {"return +127;", 0, 1, 3, {B(LdaSmi8), U8(127), B(Return)}, 0},
      {"return -128;", 0, 1, 3, {B(LdaSmi8), U8(-128), B(Return)}, 0},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(PrimitiveExpressions) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  ExpectedSnippet<int> snippets[] = {
      {"var x = 0; return x;",
       kPointerSize,
       1,
       6,
       {B(LdaZero),     //
        B(Star), R(0),  //
        B(Ldar), R(0),  //
        B(Return)},
       0},
      {"var x = 0; return x + 3;",
       2 * kPointerSize,
       1,
       12,
       {B(LdaZero),         //
        B(Star), R(0),      //
        B(Ldar), R(0),      // Easy to spot r1 not really needed here.
        B(Star), R(1),      // Dead store.
        B(LdaSmi8), U8(3),  //
        B(Add), R(1),       //
        B(Return)},
       0},
      {"var x = 0; return x - 3;",
       2 * kPointerSize,
       1,
       12,
       {B(LdaZero),         //
        B(Star), R(0),      //
        B(Ldar), R(0),      // Easy to spot r1 not really needed here.
        B(Star), R(1),      // Dead store.
        B(LdaSmi8), U8(3),  //
        B(Sub), R(1),       //
        B(Return)},
       0},
      {"var x = 4; return x * 3;",
       2 * kPointerSize,
       1,
       13,
       {B(LdaSmi8), U8(4),  //
        B(Star), R(0),      //
        B(Ldar), R(0),      // Easy to spot r1 not really needed here.
        B(Star), R(1),      // Dead store.
        B(LdaSmi8), U8(3),  //
        B(Mul), R(1),       //
        B(Return)},
       0},
      {"var x = 4; return x / 3;",
       2 * kPointerSize,
       1,
       13,
       {B(LdaSmi8), U8(4),  //
        B(Star), R(0),      //
        B(Ldar), R(0),      // Easy to spot r1 not really needed here.
        B(Star), R(1),      // Dead store.
        B(LdaSmi8), U8(3),  //
        B(Div), R(1),       //
        B(Return)},
       0},
      {"var x = 4; return x % 3;",
       2 * kPointerSize,
       1,
       13,
       {B(LdaSmi8), U8(4),  //
        B(Star), R(0),      //
        B(Ldar), R(0),      // Easy to spot r1 not really needed here.
        B(Star), R(1),      // Dead store.
        B(LdaSmi8), U8(3),  //
        B(Mod), R(1),       //
        B(Return)},
       0},
      {"var x = 1; return x | 2;",
       2 * kPointerSize,
       1,
       13,
       {B(LdaSmi8), U8(1),   //
        B(Star), R(0),       //
        B(Ldar), R(0),       // Easy to spot r1 not really needed here.
        B(Star), R(1),       // Dead store.
        B(LdaSmi8), U8(2),   //
        B(BitwiseOr), R(1),  //
        B(Return)},
       0},
      {"var x = 1; return x ^ 2;",
       2 * kPointerSize,
       1,
       13,
       {B(LdaSmi8), U8(1),    //
        B(Star), R(0),        //
        B(Ldar), R(0),        // Easy to spot r1 not really needed here.
        B(Star), R(1),        // Dead store.
        B(LdaSmi8), U8(2),    //
        B(BitwiseXor), R(1),  //
        B(Return)},
       0},
      {"var x = 1; return x & 2;",
       2 * kPointerSize,
       1,
       13,
       {B(LdaSmi8), U8(1),    //
        B(Star), R(0),        //
        B(Ldar), R(0),        // Easy to spot r1 not really needed here.
        B(Star), R(1),        // Dead store.
        B(LdaSmi8), U8(2),    //
        B(BitwiseAnd), R(1),  //
        B(Return)},
       0},
      {"var x = 10; return x << 3;",
       2 * kPointerSize,
       1,
       13,
       {B(LdaSmi8), U8(10),  //
        B(Star), R(0),       //
        B(Ldar), R(0),       // Easy to spot r1 not really needed here.
        B(Star), R(1),       // Dead store.
        B(LdaSmi8), U8(3),   //
        B(ShiftLeft), R(1),  //
        B(Return)},
       0},
      {"var x = 10; return x >> 3;",
       2 * kPointerSize,
       1,
       13,
       {B(LdaSmi8), U8(10),   //
        B(Star), R(0),        //
        B(Ldar), R(0),        // Easy to spot r1 not really needed here.
        B(Star), R(1),        // Dead store.
        B(LdaSmi8), U8(3),    //
        B(ShiftRight), R(1),  //
        B(Return)},
       0},
      {"var x = 10; return x >>> 3;",
       2 * kPointerSize,
       1,
       13,
       {B(LdaSmi8), U8(10),          //
        B(Star), R(0),               //
        B(Ldar), R(0),               // Easy to spot r1 not really needed here.
        B(Star), R(1),               // Dead store.
        B(LdaSmi8), U8(3),           //
        B(ShiftRightLogical), R(1),  //
        B(Return)},
       0},
      {"var x = 0; return (x, 3);",
       1 * kPointerSize,
       1,
       8,
       {B(LdaZero),         //
        B(Star), R(0),      //
        B(Ldar), R(0),      //
        B(LdaSmi8), U8(3),  //
        B(Return)},
       0}};

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(LogicalExpressions) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;


  ExpectedSnippet<int> snippets[] = {
      {"var x = 0; return x || 3;",
       1 * kPointerSize,
       1,
       10,
       {B(LdaZero),                     //
        B(Star), R(0),                  //
        B(Ldar), R(0),                  //
        B(JumpIfToBooleanTrue), U8(4),  //
        B(LdaSmi8), U8(3),              //
        B(Return)},
       0},
      {"var x = 0; return x && 3;",
       1 * kPointerSize,
       1,
       10,
       {B(LdaZero),                      //
        B(Star), R(0),                   //
        B(Ldar), R(0),                   //
        B(JumpIfToBooleanFalse), U8(4),  //
        B(LdaSmi8), U8(3),               //
        B(Return)},
       0},
      {"var x = 1; var a = 2, b = 3; return x || ("
#define X "a, b, a, b, "
       X X X X X X X X X X X X X X X X X X X X X X X X X X X X X X X X X
#undef X
       "3);",
       3 * kPointerSize,
       1,
       283,
       {B(LdaSmi8), U8(1),                      //
        B(Star), R(0),                          //
        B(LdaSmi8), U8(2),                      //
        B(Star), R(1),                          //
        B(LdaSmi8), U8(3),                      //
        B(Star), R(2),                          //
        B(Ldar), R(0),                          //
        B(JumpIfToBooleanTrueConstant), U8(0),  //
#define X B(Ldar), R(1), B(Ldar), R(2), B(Ldar), R(1), B(Ldar), R(2),
        X X X X X X X X X X X X X X X X X X X X X X X X X X X X X X X X X
#undef X
        B(LdaSmi8), U8(3),                      //
        B(Return)},
       1,
       {268, 0, 0, 0}},
      {"var x = 0; var a = 2, b = 3; return x && ("
#define X "a, b, a, b, "
       X X X X X X X X X X X X X X X X X X X X X X X X X X X X X X X X X
#undef X
       "3);",
       3 * kPointerSize,
       1,
       282,
       {B(LdaZero),                              //
        B(Star), R(0),                           //
        B(LdaSmi8), U8(2),                       //
        B(Star), R(1),                           //
        B(LdaSmi8), U8(3),                       //
        B(Star), R(2),                           //
        B(Ldar), R(0),                           //
        B(JumpIfToBooleanFalseConstant), U8(0),  //
#define X B(Ldar), R(1), B(Ldar), R(2), B(Ldar), R(1), B(Ldar), R(2),
        X X X X X X X X X X X X X X X X X X X X X X X X X X X X X X X X X
#undef X
        B(LdaSmi8), U8(3),                       //
        B(Return)},
       1,
       {268, 0, 0, 0}},
      {"return 0 && 3;",
       0 * kPointerSize,
       1,
       2,
       {B(LdaZero),                    //
        B(Return)},
       0},
      {"return 1 || 3;",
       0 * kPointerSize,
       1,
       3,
       {B(LdaSmi8), U8(1),             //
        B(Return)},
       0},
      {"var x = 1; return x && 3 || 0, 1;",
       1 * kPointerSize,
       1,
       16,
       {B(LdaSmi8), U8(1),               //
        B(Star), R(0),                   //
        B(Ldar), R(0),                   //
        B(JumpIfToBooleanFalse), U8(4),  //
        B(LdaSmi8), U8(3),               //
        B(JumpIfToBooleanTrue), U8(3),   //
        B(LdaZero),                      //
        B(LdaSmi8), U8(1),               //
        B(Return)},
       0}};

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(Parameters) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  ExpectedSnippet<int> snippets[] = {
      {"function f() { return this; }",
       0, 1, 3, {B(Ldar), R(helper.kLastParamIndex), B(Return)}, 0},
      {"function f(arg1) { return arg1; }",
       0, 2, 3, {B(Ldar), R(helper.kLastParamIndex), B(Return)}, 0},
      {"function f(arg1) { return this; }",
       0, 2, 3, {B(Ldar), R(helper.kLastParamIndex - 1), B(Return)}, 0},
      {"function f(arg1, arg2, arg3, arg4, arg5, arg6, arg7) { return arg4; }",
       0, 8, 3, {B(Ldar), R(helper.kLastParamIndex - 3), B(Return)}, 0},
      {"function f(arg1, arg2, arg3, arg4, arg5, arg6, arg7) { return this; }",
       0, 8, 3, {B(Ldar), R(helper.kLastParamIndex - 7), B(Return)}, 0},
      {"function f(arg1) { arg1 = 1; }",
       0, 2, 6,
       {B(LdaSmi8), U8(1),                   //
        B(Star), R(helper.kLastParamIndex),  //
        B(LdaUndefined),                     //
        B(Return)},
       0},
      {"function f(arg1, arg2, arg3, arg4) { arg2 = 1; }",
       0, 5, 6,
       {B(LdaSmi8), U8(1),                       //
        B(Star), R(helper.kLastParamIndex - 2),  //
        B(LdaUndefined),                         //
        B(Return)},
       0},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunction(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(IntegerConstants) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  ExpectedSnippet<int> snippets[] = {
    {"return 12345678;",
     0,
     1,
     3,
     {
       B(LdaConstant), U8(0),  //
       B(Return)               //
     },
     1,
     {12345678}},
    {"var a = 1234; return 5678;",
     1 * kPointerSize,
     1,
     7,
     {
       B(LdaConstant), U8(0),  //
       B(Star), R(0),          //
       B(LdaConstant), U8(1),  //
       B(Return)               //
     },
     2,
     {1234, 5678}},
    {"var a = 1234; return 1234;",
     1 * kPointerSize,
     1,
     7,
     {
       B(LdaConstant), U8(0),  //
       B(Star), R(0),          //
       B(LdaConstant), U8(0),  //
       B(Return)               //
     },
     1,
     {1234}}};

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(HeapNumberConstants) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  ExpectedSnippet<double> snippets[] = {
    {"return 1.2;",
     0,
     1,
     3,
     {
       B(LdaConstant), U8(0),  //
       B(Return)               //
     },
     1,
     {1.2}},
    {"var a = 1.2; return 2.6;",
     1 * kPointerSize,
     1,
     7,
     {
       B(LdaConstant), U8(0),  //
       B(Star), R(0),          //
       B(LdaConstant), U8(1),  //
       B(Return)               //
     },
     2,
     {1.2, 2.6}},
    {"var a = 3.14; return 3.14;",
     1 * kPointerSize,
     1,
     7,
     {
       B(LdaConstant), U8(0),  //
       B(Star), R(0),          //
       B(LdaConstant), U8(1),  //
       B(Return)               //
     },
     2,
     {3.14, 3.14}}};
  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(StringConstants) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  ExpectedSnippet<const char*> snippets[] = {
      {"return \"This is a string\";",
       0,
       1,
       3,
       {
           B(LdaConstant), U8(0),  //
           B(Return)               //
       },
       1,
       {"This is a string"}},
      {"var a = \"First string\"; return \"Second string\";",
       1 * kPointerSize,
       1,
       7,
       {
           B(LdaConstant), U8(0),  //
           B(Star), R(0),          //
           B(LdaConstant), U8(1),  //
           B(Return)               //
       },
       2,
       {"First string", "Second string"}},
      {"var a = \"Same string\"; return \"Same string\";",
       1 * kPointerSize,
       1,
       7,
       {
           B(LdaConstant), U8(0),  //
           B(Star), R(0),          //
           B(LdaConstant), U8(0),  //
           B(Return)               //
       },
       1,
       {"Same string"}}};

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(PropertyLoads) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;
  Zone zone;

  FeedbackVectorSpec feedback_spec(&zone);
  FeedbackVectorSlot slot1 = feedback_spec.AddLoadICSlot();
  FeedbackVectorSlot slot2 = feedback_spec.AddLoadICSlot();

  Handle<i::TypeFeedbackVector> vector =
      i::NewTypeFeedbackVector(helper.isolate(), &feedback_spec);

  ExpectedSnippet<const char*> snippets[] = {
      {"function f(a) { return a.name; }\nf({name : \"test\"})",
       1 * kPointerSize,
       2,
       10,
       {
           B(Ldar), R(helper.kLastParamIndex),                  //
           B(Star), R(0),                                       //
           B(LdaConstant), U8(0),                               //
           B(LoadICSloppy), R(0), U8(vector->GetIndex(slot1)),  //
           B(Return)                                            //
       },
       1,
       {"name"}},
      {"function f(a) { return a[\"key\"]; }\nf({key : \"test\"})",
       1 * kPointerSize,
       2,
       10,
       {
           B(Ldar), R(helper.kLastParamIndex),                  //
           B(Star), R(0),                                       //
           B(LdaConstant), U8(0),                               //
           B(LoadICSloppy), R(0), U8(vector->GetIndex(slot1)),  //
           B(Return)                                            //
       },
       1,
       {"key"}},
      {"function f(a) { return a[100]; }\nf({100 : \"test\"})",
       1 * kPointerSize,
       2,
       10,
       {
           B(Ldar), R(helper.kLastParamIndex),                       //
           B(Star), R(0),                                            //
           B(LdaSmi8), U8(100),                                      //
           B(KeyedLoadICSloppy), R(0), U8(vector->GetIndex(slot1)),  //
           B(Return)                                                 //
       },
       0},
      {"function f(a, b) { return a[b]; }\nf({arg : \"test\"}, \"arg\")",
       1 * kPointerSize,
       3,
       10,
       {
           B(Ldar), R(helper.kLastParamIndex - 1),                   //
           B(Star), R(0),                                            //
           B(Ldar), R(helper.kLastParamIndex),                       //
           B(KeyedLoadICSloppy), R(0), U8(vector->GetIndex(slot1)),  //
           B(Return)                                                 //
       },
       0},
      {"function f(a) { var b = a.name; return a[-124]; }\n"
       "f({\"-124\" : \"test\", name : 123 })",
       2 * kPointerSize,
       2,
       21,
       {
           B(Ldar), R(helper.kLastParamIndex),                       //
           B(Star), R(1),                                            //
           B(LdaConstant), U8(0),                                    //
           B(LoadICSloppy), R(1), U8(vector->GetIndex(slot1)),       //
           B(Star), R(0),                                            //
           B(Ldar), R(helper.kLastParamIndex),                       //
           B(Star), R(1),                                            //
           B(LdaSmi8), U8(-124),                                     //
           B(KeyedLoadICSloppy), R(1), U8(vector->GetIndex(slot2)),  //
           B(Return)                                                 //
       },
       1,
       {"name"}},
      {"function f(a) { \"use strict\"; return a.name; }\nf({name : \"test\"})",
       1 * kPointerSize,
       2,
       12,
       {
           // TODO(rmcilroy) Avoid unnecessary LdaConstant for "use strict"
           // expression, or any other unused literal expression.
           B(LdaConstant), U8(0),                               //
           B(Ldar), R(helper.kLastParamIndex),                  //
           B(Star), R(0),                                       //
           B(LdaConstant), U8(1),                               //
           B(LoadICStrict), R(0), U8(vector->GetIndex(slot1)),  //
           B(Return)                                            //
       },
       2,
       {"use strict", "name"}},
      {"function f(a, b) { \"use strict\"; return a[b]; }\n"
       "f({arg : \"test\"}, \"arg\")",
       1 * kPointerSize,
       3,
       12,
       {
           // TODO(rmcilroy) Avoid unnecessary LdaConstant for "use strict"
           // expression, or any other unused literal expression.
           B(LdaConstant), U8(0),                                    //
           B(Ldar), R(helper.kLastParamIndex - 1),                   //
           B(Star), R(0),                                            //
           B(Ldar), R(helper.kLastParamIndex),                       //
           B(KeyedLoadICStrict), R(0), U8(vector->GetIndex(slot1)),  //
           B(Return)                                                 //
       },
       1,
       {"use strict"}}};
  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecode(snippets[i].code_snippet, helper.kFunctionName);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(PropertyStores) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;
  Zone zone;

  FeedbackVectorSpec feedback_spec(&zone);
  FeedbackVectorSlot slot1 = feedback_spec.AddStoreICSlot();
  FeedbackVectorSlot slot2 = feedback_spec.AddStoreICSlot();

  Handle<i::TypeFeedbackVector> vector =
      i::NewTypeFeedbackVector(helper.isolate(), &feedback_spec);

  ExpectedSnippet<const char*> snippets[] = {
      {"function f(a) { a.name = \"val\"; }\nf({name : \"test\"})",
       2 * kPointerSize,
       2,
       16,
       {
           B(Ldar), R(helper.kLastParamIndex),                         //
           B(Star), R(0),                                              //
           B(LdaConstant), U8(0),                                      //
           B(Star), R(1),                                              //
           B(LdaConstant), U8(1),                                      //
           B(StoreICSloppy), R(0), R(1), U8(vector->GetIndex(slot1)),  //
           B(LdaUndefined),                                            //
           B(Return)                                                   //
       },
       2,
       {"name", "val"}},
      {"function f(a) { a[\"key\"] = \"val\"; }\nf({key : \"test\"})",
       2 * kPointerSize,
       2,
       16,
       {
           B(Ldar), R(helper.kLastParamIndex),                         //
           B(Star), R(0),                                              //
           B(LdaConstant), U8(0),                                      //
           B(Star), R(1),                                              //
           B(LdaConstant), U8(1),                                      //
           B(StoreICSloppy), R(0), R(1), U8(vector->GetIndex(slot1)),  //
           B(LdaUndefined),                                            //
           B(Return)                                                   //
       },
       2,
       {"key", "val"}},
      {"function f(a) { a[100] = \"val\"; }\nf({100 : \"test\"})",
       2 * kPointerSize,
       2,
       16,
       {
           B(Ldar), R(helper.kLastParamIndex),                              //
           B(Star), R(0),                                                   //
           B(LdaSmi8), U8(100),                                             //
           B(Star), R(1),                                                   //
           B(LdaConstant), U8(0),                                           //
           B(KeyedStoreICSloppy), R(0), R(1), U8(vector->GetIndex(slot1)),  //
           B(LdaUndefined),                                                 //
           B(Return)                                                        //
       },
       1,
       {"val"}},
      {"function f(a, b) { a[b] = \"val\"; }\nf({arg : \"test\"}, \"arg\")",
       2 * kPointerSize,
       3,
       16,
       {
           B(Ldar), R(helper.kLastParamIndex - 1),                          //
           B(Star), R(0),                                                   //
           B(Ldar), R(helper.kLastParamIndex),                              //
           B(Star), R(1),                                                   //
           B(LdaConstant), U8(0),                                           //
           B(KeyedStoreICSloppy), R(0), R(1), U8(vector->GetIndex(slot1)),  //
           B(LdaUndefined),                                                 //
           B(Return)                                                        //
       },
       1,
       {"val"}},
      {"function f(a) { a.name = a[-124]; }\n"
       "f({\"-124\" : \"test\", name : 123 })",
       3 * kPointerSize,
       2,
       23,
       {
           B(Ldar), R(helper.kLastParamIndex),                         //
           B(Star), R(0),                                              //
           B(LdaConstant), U8(0),                                      //
           B(Star), R(1),                                              //
           B(Ldar), R(helper.kLastParamIndex),                         //
           B(Star), R(2),                                              //
           B(LdaSmi8), U8(-124),                                       //
           B(KeyedLoadICSloppy), R(2), U8(vector->GetIndex(slot1)),    //
           B(StoreICSloppy), R(0), R(1), U8(vector->GetIndex(slot2)),  //
           B(LdaUndefined),                                            //
           B(Return)                                                   //
       },
       1,
       {"name"}},
      {"function f(a) { \"use strict\"; a.name = \"val\"; }\n"
       "f({name : \"test\"})",
       2 * kPointerSize,
       2,
       18,
       {
           // TODO(rmcilroy) Avoid unnecessary LdaConstant for "use strict"
           // expression, or any other unused literal expression.
           B(LdaConstant), U8(0),                                      //
           B(Ldar), R(helper.kLastParamIndex),                         //
           B(Star), R(0),                                              //
           B(LdaConstant), U8(1),                                      //
           B(Star), R(1),                                              //
           B(LdaConstant), U8(2),                                      //
           B(StoreICStrict), R(0), R(1), U8(vector->GetIndex(slot1)),  //
           B(LdaUndefined),                                            //
           B(Return)                                                   //
       },
       3,
       {"use strict", "name", "val"}},
      {"function f(a, b) { \"use strict\"; a[b] = \"val\"; }\n"
       "f({arg : \"test\"}, \"arg\")",
       2 * kPointerSize,
       3,
       18,
       {
           // TODO(rmcilroy) Avoid unnecessary LdaConstant for "use strict"
           // expression, or any other unused literal expression.
           B(LdaConstant), U8(0),                                           //
           B(Ldar), R(helper.kLastParamIndex - 1),                          //
           B(Star), R(0),                                                   //
           B(Ldar), R(helper.kLastParamIndex),                              //
           B(Star), R(1),                                                   //
           B(LdaConstant), U8(1),                                           //
           B(KeyedStoreICStrict), R(0), R(1), U8(vector->GetIndex(slot1)),  //
           B(LdaUndefined),                                                 //
           B(Return)                                                        //
       },
       2,
       {"use strict", "val"}}};
  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecode(snippets[i].code_snippet, helper.kFunctionName);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


#define FUNC_ARG "new (function Obj() { this.func = function() { return; }})()"


TEST(PropertyCall) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;
  Zone zone;

  FeedbackVectorSpec feedback_spec(&zone);
  FeedbackVectorSlot slot1 = feedback_spec.AddLoadICSlot();
  FeedbackVectorSlot slot2 = feedback_spec.AddLoadICSlot();
  USE(slot1);

  Handle<i::TypeFeedbackVector> vector =
      i::NewTypeFeedbackVector(helper.isolate(), &feedback_spec);

  ExpectedSnippet<const char*> snippets[] = {
      {"function f(a) { return a.func(); }\nf(" FUNC_ARG ")",
       2 * kPointerSize,
       2,
       16,
       {
           B(Ldar), R(helper.kLastParamIndex),                  //
           B(Star), R(1),                                       //
           B(LdaConstant), U8(0),                               //
           B(LoadICSloppy), R(1), U8(vector->GetIndex(slot2)),  //
           B(Star), R(0),                                       //
           B(Call), R(0), R(1), U8(0),                          //
           B(Return)                                            //
       },
       1,
       {"func"}},
      {"function f(a, b, c) { return a.func(b, c); }\nf(" FUNC_ARG ", 1, 2)",
       4 * kPointerSize,
       4,
       24,
       {
           B(Ldar), R(helper.kLastParamIndex - 2),              //
           B(Star), R(1),                                       //
           B(LdaConstant), U8(0),                               //
           B(LoadICSloppy), R(1), U8(vector->GetIndex(slot2)),  //
           B(Star), R(0),                                       //
           B(Ldar), R(helper.kLastParamIndex - 1),              //
           B(Star), R(2),                                       //
           B(Ldar), R(helper.kLastParamIndex),                  //
           B(Star), R(3),                                       //
           B(Call), R(0), R(1), U8(2),                          //
           B(Return)                                            //
       },
       1,
       {"func"}},
      {"function f(a, b) { return a.func(b + b, b); }\nf(" FUNC_ARG ", 1)",
       4 * kPointerSize,
       3,
       30,
       {
           B(Ldar), R(helper.kLastParamIndex - 1),              //
           B(Star), R(1),                                       //
           B(LdaConstant), U8(0),                               //
           B(LoadICSloppy), R(1), U8(vector->GetIndex(slot2)),  //
           B(Star), R(0),                                       //
           B(Ldar), R(helper.kLastParamIndex),                  //
           B(Star), R(3),                                       //
           B(Ldar), R(helper.kLastParamIndex),                  //
           B(Add), R(3),                                        //
           B(Star), R(2),                                       //
           B(Ldar), R(helper.kLastParamIndex),                  //
           B(Star), R(3),                                       //
           B(Call), R(0), R(1), U8(2),                          //
           B(Return),                                           //
       },
       1,
       {"func"}}};
  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecode(snippets[i].code_snippet, helper.kFunctionName);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(LoadGlobal) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  ExpectedSnippet<int> snippets[] = {
      {
          "var a = 1;\nfunction f() { return a; }\nf()",
          0,
          1,
          3,
          {
              B(LdaGlobal), _,  //
              B(Return)         //
          },
      },
      {
          "function t() { }\nfunction f() { return t; }\nf()",
          0,
          1,
          3,
          {
              B(LdaGlobal), _,  //
              B(Return)         //
          },
      },
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecode(snippets[i].code_snippet, "f");
    CheckBytecodeArrayEqual(snippets[i], bytecode_array, true);
  }
}


TEST(StoreGlobal) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  ExpectedSnippet<InstanceType> snippets[] = {
      {
          "var a = 1;\nfunction f() { a = 2; }\nf()",
          0,
          1,
          6,
          {
              B(LdaSmi8), U8(2),      //
              B(StaGlobalSloppy), _,  //
              B(LdaUndefined),        //
              B(Return)               //
          },
      },
      {
          "var a = \"test\"; function f(b) { a = b; }\nf(\"global\")",
          0,
          2,
          6,
          {
              B(Ldar), R(helper.kLastParamIndex),  //
              B(StaGlobalSloppy), _,               //
              B(LdaUndefined),                     //
              B(Return)                            //
          },
      },
      {
          "'use strict'; var a = 1;\nfunction f() { a = 2; }\nf()",
          0,
          1,
          6,
          {
              B(LdaSmi8), U8(2),      //
              B(StaGlobalStrict), _,  //
              B(LdaUndefined),        //
              B(Return)               //
          },
      },
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecode(snippets[i].code_snippet, "f");
    CheckBytecodeArrayEqual(snippets[i], bytecode_array, true);
  }
}


TEST(CallGlobal) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  ExpectedSnippet<int> snippets[] = {
      {
          "function t() { }\nfunction f() { return t(); }\nf()",
          2 * kPointerSize,
          1,
          12,
          {
              B(LdaUndefined),             //
              B(Star), R(1),               //
              B(LdaGlobal), _,             //
              B(Star), R(0),               //
              B(Call), R(0), R(1), U8(0),  //
              B(Return)                    //
          },
      },
      {
          "function t(a, b, c) { }\nfunction f() { return t(1, 2, 3); }\nf()",
          5 * kPointerSize,
          1,
          24,
          {
              B(LdaUndefined),             //
              B(Star), R(1),               //
              B(LdaGlobal), _,             //
              B(Star), R(0),               //
              B(LdaSmi8), U8(1),           //
              B(Star), R(2),               //
              B(LdaSmi8), U8(2),           //
              B(Star), R(3),               //
              B(LdaSmi8), U8(3),           //
              B(Star), R(4),               //
              B(Call), R(0), R(1), U8(3),  //
              B(Return)                    //
          },
      },
  };

  size_t num_snippets = sizeof(snippets) / sizeof(snippets[0]);
  for (size_t i = 0; i < num_snippets; i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecode(snippets[i].code_snippet, "f");
    CheckBytecodeArrayEqual(snippets[i], bytecode_array, true);
  }
}


TEST(LoadUnallocated) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;
  Zone zone;

  int context_reg = Register::function_context().index();
  int global_index = Context::GLOBAL_OBJECT_INDEX;

  FeedbackVectorSpec feedback_spec(&zone);
  FeedbackVectorSlot slot1 = feedback_spec.AddStoreICSlot();

  Handle<i::TypeFeedbackVector> vector =
      i::NewTypeFeedbackVector(helper.isolate(), &feedback_spec);

  ExpectedSnippet<const char*> snippets[] = {
      {"a = 1;\nfunction f() { return a; }\nf()",
       1 * kPointerSize,
       1,
       11,
       {B(LdaContextSlot), R(context_reg), U8(global_index),  //
        B(Star), R(0),                                        //
        B(LdaConstant), U8(0),                                //
        B(LoadICSloppy), R(0), U8(vector->GetIndex(slot1)),   //
        B(Return)},
       1,
       {"a"}},
      {"function f() { return t; }\nt = 1;\nf()",
       1 * kPointerSize,
       1,
       11,
       {B(LdaContextSlot), R(context_reg), U8(global_index),  //
        B(Star), R(0),                                        //
        B(LdaConstant), U8(0),                                //
        B(LoadICSloppy), R(0), U8(vector->GetIndex(slot1)),   //
        B(Return)},
       1,
       {"t"}},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecode(snippets[i].code_snippet, "f");
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(StoreUnallocated) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;
  Zone zone;

  int context_reg = Register::function_context().index();
  int global_index = Context::GLOBAL_OBJECT_INDEX;

  FeedbackVectorSpec feedback_spec(&zone);
  FeedbackVectorSlot slot1 = feedback_spec.AddStoreICSlot();

  Handle<i::TypeFeedbackVector> vector =
      i::NewTypeFeedbackVector(helper.isolate(), &feedback_spec);

  ExpectedSnippet<const char*> snippets[] = {
      {"a = 1;\nfunction f() { a = 2; }\nf()",
       3 * kPointerSize,
       1,
       21,
       {B(LdaSmi8), U8(2),                                          //
        B(Star), R(0),                                              //
        B(LdaContextSlot), R(context_reg), U8(global_index),        //
        B(Star), R(1),                                              //
        B(LdaConstant), U8(0),                                      //
        B(Star), R(2),                                              //
        B(Ldar), R(0),                                              //
        B(StoreICSloppy), R(1), R(2), U8(vector->GetIndex(slot1)),  //
        B(LdaUndefined),                                            //
        B(Return)},
       1,
       {"a"}},
      {"function f() { t = 4; }\nf()\nt = 1;",
       3 * kPointerSize,
       1,
       21,
       {B(LdaSmi8), U8(4),                                          //
        B(Star), R(0),                                              //
        B(LdaContextSlot), R(context_reg), U8(global_index),        //
        B(Star), R(1),                                              //
        B(LdaConstant), U8(0),                                      //
        B(Star), R(2),                                              //
        B(Ldar), R(0),                                              //
        B(StoreICSloppy), R(1), R(2), U8(vector->GetIndex(slot1)),  //
        B(LdaUndefined),                                            //
        B(Return)},
       1,
       {"t"}},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecode(snippets[i].code_snippet, "f");
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(CallRuntime) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  ExpectedSnippet<int> snippets[] = {
      {
          "function f() { %TheHole() }\nf()",
          1 * kPointerSize,
          1,
          7,
          {
              B(CallRuntime), U16(Runtime::kTheHole), R(0), U8(0),  //
              B(LdaUndefined),                                      //
              B(Return)                                             //
          },
      },
      {
          "function f(a) { return %IsArray(a) }\nf(undefined)",
          1 * kPointerSize,
          2,
          10,
          {
              B(Ldar), R(helper.kLastParamIndex),                   //
              B(Star), R(0),                                        //
              B(CallRuntime), U16(Runtime::kIsArray), R(0), U8(1),  //
              B(Return)                                             //
          },
      },
      {
          "function f() { return %Add(1, 2) }\nf()",
          2 * kPointerSize,
          1,
          14,
          {
              B(LdaSmi8), U8(1),                                //
              B(Star), R(0),                                    //
              B(LdaSmi8), U8(2),                                //
              B(Star), R(1),                                    //
              B(CallRuntime), U16(Runtime::kAdd), R(0), U8(2),  //
              B(Return)                                         //
          },
      },
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecode(snippets[i].code_snippet, "f");
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(IfConditions) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  Handle<Object> unused = helper.factory()->undefined_value();

  ExpectedSnippet<Handle<Object>> snippets[] = {
      {"function f() { if (0) { return 1; } else { return -1; } } f()",
       0,
       1,
       14,
       {B(LdaZero),             //
        B(ToBoolean),           //
        B(JumpIfFalse), U8(7),  //
        B(LdaSmi8), U8(1),      //
        B(Return),              //
        B(Jump), U8(5),         //
        B(LdaSmi8), U8(-1),     //
        B(Return),              //
        B(LdaUndefined),        //
        B(Return)},             //
       0,
       {unused, unused, unused, unused, unused, unused}},
      {"function f() { if ('lucky') { return 1; } else { return -1; } } f();",
       0,
       1,
       15,
       {B(LdaConstant), U8(0),  //
        B(ToBoolean),           //
        B(JumpIfFalse), U8(7),  //
        B(LdaSmi8), U8(1),      //
        B(Return),              //
        B(Jump), U8(5),         //
        B(LdaSmi8), U8(-1),     //
        B(Return),              //
        B(LdaUndefined),        //
        B(Return)},             //
       1,
       {helper.factory()->NewStringFromStaticChars("lucky"), unused, unused,
        unused, unused, unused}},
      {"function f() { if (false) { return 1; } else { return -1; } } f();",
       0,
       1,
       13,
       {B(LdaFalse),            //
        B(JumpIfFalse), U8(7),  //
        B(LdaSmi8), U8(1),      //
        B(Return),              //
        B(Jump), U8(5),         //
        B(LdaSmi8), U8(-1),     //
        B(Return),              //
        B(LdaUndefined),        //
        B(Return)},             //
       0,
       {unused, unused, unused, unused, unused, unused}},
      {"function f(a) { if (a <= 0) { return 200; } else { return -200; } }"
       "f(99);",
       kPointerSize,
       2,
       19,
       {B(Ldar), R(helper.kLastParamIndex),  //
        B(Star), R(0),                       //
        B(LdaZero),                          //
        B(TestLessThanOrEqual), R(0),        //
        B(JumpIfFalse), U8(7),               //
        B(LdaConstant), U8(0),               //
        B(Return),                           //
        B(Jump), U8(5),                      //
        B(LdaConstant), U8(1),               //
        B(Return),                           //
        B(LdaUndefined),                     //
        B(Return)},                          //
       2,
       {helper.factory()->NewNumberFromInt(200),
        helper.factory()->NewNumberFromInt(-200), unused, unused, unused,
        unused}},
      {"function f(a, b) { if (a in b) { return 200; } }"
       "f('prop', { prop: 'yes'});",
       kPointerSize,
       3,
       15,
       {B(Ldar), R(helper.kLastParamIndex - 1),  //
        B(Star), R(0),                           //
        B(Ldar), R(helper.kLastParamIndex),      //
        B(TestIn), R(0),                         //
        B(JumpIfFalse), U8(5),                   //
        B(LdaConstant), U8(0),                   //
        B(Return),                               //
        B(LdaUndefined),                         //
        B(Return)},                              //
       1,
       {helper.factory()->NewNumberFromInt(200), unused, unused, unused, unused,
        unused}},
      {"function f(z) { var a = 0; var b = 0; if (a === 0.01) { "
#define X "b = a; a = b; "
       X X X X X X X X X X X X X X X X X X X X X X X X
#undef X
       " return 200; } else { return -200; } } f(0.001)",
       3 * kPointerSize,
       2,
       218,
       {B(LdaZero),                     //
        B(Star), R(0),                  //
        B(LdaZero),                     //
        B(Star), R(1),                  //
        B(Ldar), R(0),                  //
        B(Star), R(2),                  //
        B(LdaConstant), U8(0),          //
        B(TestEqualStrict), R(2),       //
        B(JumpIfFalseConstant), U8(2),  //
#define X B(Ldar), R(0), B(Star), R(1), B(Ldar), R(1), B(Star), R(0),
        X X X X X X X X X X X X X X X X X X X X X X X X
#undef X
        B(LdaConstant), U8(1),  //
        B(Return),              //
        B(Jump), U8(5),         //
        B(LdaConstant), U8(3),  //
        B(Return),              //
        B(LdaUndefined),        //
        B(Return)},             //
       4,
       {helper.factory()->NewHeapNumber(0.01),
        helper.factory()->NewNumberFromInt(200),
        helper.factory()->NewNumberFromInt(199),
        helper.factory()->NewNumberFromInt(-200),
        unused, unused}},
      {"function f(a, b) {\n"
       "  if (a == b) { return 1; }\n"
       "  if (a === b) { return 1; }\n"
       "  if (a < b) { return 1; }\n"
       "  if (a > b) { return 1; }\n"
       "  if (a <= b) { return 1; }\n"
       "  if (a >= b) { return 1; }\n"
       "  if (a in b) { return 1; }\n"
       "  if (a instanceof b) { return 1; }\n"
       "  /* if (a != b) { return 1; } */"   // TODO(oth) Ast visitor yields
       "  /* if (a !== b) { return 1; } */"  // UNARY NOT, rather than !=/!==.
       "  return 0;\n"
       "} f(1, 1);",
       kPointerSize,
       3,
       106,
       {
#define IF_CONDITION_RETURN(condition) \
  B(Ldar), R(helper.kLastParamIndex - 1), \
  B(Star), R(0),                          \
  B(Ldar), R(helper.kLastParamIndex),     \
  B(condition), R(0),                     \
  B(JumpIfFalse), U8(5),                  \
  B(LdaSmi8), U8(1),                      \
  B(Return),
           IF_CONDITION_RETURN(TestEqual)               //
           IF_CONDITION_RETURN(TestEqualStrict)         //
           IF_CONDITION_RETURN(TestLessThan)            //
           IF_CONDITION_RETURN(TestGreaterThan)         //
           IF_CONDITION_RETURN(TestLessThanOrEqual)     //
           IF_CONDITION_RETURN(TestGreaterThanOrEqual)  //
           IF_CONDITION_RETURN(TestIn)                  //
           IF_CONDITION_RETURN(TestInstanceOf)          //
#undef IF_CONDITION_RETURN
           B(LdaZero),  //
           B(Return)},  //
       0,
       {unused, unused, unused, unused, unused, unused}},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecode(snippets[i].code_snippet, helper.kFunctionName);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(DeclareGlobals) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  ExpectedSnippet<InstanceType> snippets[] = {
      {"var a = 1;",
       5 * kPointerSize,
       1,
       45,
       {
           B(Ldar), R(Register::function_closure().index()),              //
           B(Star), R(2),                                                 //
           B(LdaConstant), U8(0),                                         //
           B(Star), R(3),                                                 //
           B(CallRuntime), U16(Runtime::kNewScriptContext), R(2), U8(2),  //
           B(PushContext), R(1),                                          //
           B(LdaConstant), U8(1),                                         //
           B(Star), R(2),                                                 //
           B(LdaZero),                                                    //
           B(Star), R(3),                                                 //
           B(CallRuntime), U16(Runtime::kDeclareGlobals), R(2), U8(2),    //
           B(LdaConstant), U8(2),                                         //
           B(Star), R(2),                                                 //
           B(LdaZero),                                                    //
           B(Star), R(3),                                                 //
           B(LdaSmi8), U8(1),                                             //
           B(Star), R(4),                                                 //
           B(CallRuntime), U16(Runtime::kInitializeVarGlobal), R(2),      //
                           U8(3),                                         //
           B(LdaUndefined),                                               //
           B(Return)                                                      //
       },
       3,
       {InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE}},
      {"function f() {}",
       3 * kPointerSize,
       1,
       29,
       {
           B(Ldar), R(Register::function_closure().index()),              //
           B(Star), R(1),                                                 //
           B(LdaConstant), U8(0),                                         //
           B(Star), R(2),                                                 //
           B(CallRuntime), U16(Runtime::kNewScriptContext), R(1), U8(2),  //
           B(PushContext), R(0),                                          //
           B(LdaConstant), U8(1),                                         //
           B(Star), R(1),                                                 //
           B(LdaZero),                                                    //
           B(Star), R(2),                                                 //
           B(CallRuntime), U16(Runtime::kDeclareGlobals), R(1), U8(2),    //
           B(LdaUndefined),                                               //
           B(Return)                                                      //
       },
       2,
       {InstanceType::FIXED_ARRAY_TYPE, InstanceType::FIXED_ARRAY_TYPE}},
      {"var a = 1;\na=2;",
       5 * kPointerSize,
       1,
       52,
       {
           B(Ldar), R(Register::function_closure().index()),              //
           B(Star), R(2),                                                 //
           B(LdaConstant), U8(0),                                         //
           B(Star), R(3),                                                 //
           B(CallRuntime), U16(Runtime::kNewScriptContext), R(2), U8(2),  //
           B(PushContext), R(1),                                          //
           B(LdaConstant), U8(1),                                         //
           B(Star), R(2),                                                 //
           B(LdaZero),                                                    //
           B(Star), R(3),                                                 //
           B(CallRuntime), U16(Runtime::kDeclareGlobals), R(2), U8(2),    //
           B(LdaConstant), U8(2),                                         //
           B(Star), R(2),                                                 //
           B(LdaZero),                                                    //
           B(Star), R(3),                                                 //
           B(LdaSmi8), U8(1),                                             //
           B(Star), R(4),                                                 //
           B(CallRuntime), U16(Runtime::kInitializeVarGlobal), R(2),      //
                           U8(3),                                         //
           B(LdaSmi8), U8(2),                                             //
           B(StaGlobalSloppy), _,                                         //
           B(Star), R(0),                                                 //
           B(Ldar), R(0),                                                 //
           B(Return)                                                      //
       },
       3,
       {InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE}},
      {"function f() {}\nf();",
       4 * kPointerSize,
       1,
       43,
       {
           B(Ldar), R(Register::function_closure().index()),              //
           B(Star), R(2),                                                 //
           B(LdaConstant), U8(0),                                         //
           B(Star), R(3),                                                 //
           B(CallRuntime), U16(Runtime::kNewScriptContext), R(2), U8(2),  //
           B(PushContext), R(1),                                          //
           B(LdaConstant), U8(1),                                         //
           B(Star), R(2),                                                 //
           B(LdaZero),                                                    //
           B(Star), R(3),                                                 //
           B(CallRuntime), U16(Runtime::kDeclareGlobals), R(2), U8(2),    //
           B(LdaUndefined),                                               //
           B(Star), R(3),                                                 //
           B(LdaGlobal), _,                                               //
           B(Star), R(2),                                                 //
           B(Call), R(2), R(3), U8(0),                                    //
           B(Star), R(0),                                                 //
           B(Ldar), R(0),                                                 //
           B(Return)                                                      //
       },
       2,
       {InstanceType::FIXED_ARRAY_TYPE, InstanceType::FIXED_ARRAY_TYPE}},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeTopLevelBytecode(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array, true);
  }
}


TEST(BasicLoops) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  ExpectedSnippet<int> snippets[] = {
      {"var x = 0;"
       "var y = 1;"
       "while (x < 10) {"
       "  y = y * 10;"
       "  x = x + 1;"
       "}"
       "return y;",
       3 * kPointerSize,
       1,
       42,
       {
           B(LdaZero),              //
           B(Star), R(0),           //
           B(LdaSmi8), U8(1),       //
           B(Star), R(1),           //
           B(Jump), U8(22),         //
           B(Ldar), R(1),           //
           B(Star), R(2),           //
           B(LdaSmi8), U8(10),      //
           B(Mul), R(2),            //
           B(Star), R(1),           //
           B(Ldar), R(0),           //
           B(Star), R(2),           //
           B(LdaSmi8), U8(1),       //
           B(Add), R(2),            //
           B(Star), R(0),           //
           B(Ldar), R(0),           //
           B(Star), R(2),           //
           B(LdaSmi8), U8(10),      //
           B(TestLessThan), R(2),   //
           B(JumpIfTrue), U8(-28),  //
           B(Ldar), R(1),           //
           B(Return),               //
       },
       0},
      {"var i = 0;"
       "while(true) {"
       "  if (i < 0) continue;"
       "  if (i == 3) break;"
       "  if (i == 4) break;"
       "  if (i == 10) continue;"
       "  if (i == 5) break;"
       "  i = i + 1;"
       "}"
       "return i;",
       2 * kPointerSize,
       1,
       80,
       {
           B(LdaZero),              //
           B(Star), R(0),           //
           B(Jump), U8(71),         //
           B(Ldar), R(0),           //
           B(Star), R(1),           //
           B(LdaZero),              //
           B(TestLessThan), R(1),   //
           B(JumpIfFalse), U8(4),   //
           B(Jump), U8(60),         //
           B(Ldar), R(0),           //
           B(Star), R(1),           //
           B(LdaSmi8), U8(3),       //
           B(TestEqual), R(1),      //
           B(JumpIfFalse), U8(4),   //
           B(Jump), U8(51),         //
           B(Ldar), R(0),           //
           B(Star), R(1),           //
           B(LdaSmi8), U8(4),       //
           B(TestEqual), R(1),      //
           B(JumpIfFalse), U8(4),   //
           B(Jump), U8(39),         //
           B(Ldar), R(0),           //
           B(Star), R(1),           //
           B(LdaSmi8), U8(10),      //
           B(TestEqual), R(1),      //
           B(JumpIfFalse), U8(4),   //
           B(Jump), U8(24),         //
           B(Ldar), R(0),           //
           B(Star), R(1),           //
           B(LdaSmi8), U8(5),       //
           B(TestEqual), R(1),      //
           B(JumpIfFalse), U8(4),   //
           B(Jump), U8(15),         //
           B(Ldar), R(0),           //
           B(Star), R(1),           //
           B(LdaSmi8), U8(1),       //
           B(Add), R(1),            //
           B(Star), R(0),           //
           B(LdaTrue),              //
           B(JumpIfTrue), U8(-70),  //
           B(Ldar), R(0),           //
           B(Return)                //
       },
       0},
      {"var x = 0; var y = 1;"
       "do {"
       "  y = y * 10;"
       "  if (x == 5) break;"
       "  if (x == 6) continue;"
       "  x = x + 1;"
       "} while (x < 10);"
       "return y;",
       3 * kPointerSize,
       1,
       64,
       {
           B(LdaZero),              //
           B(Star), R(0),           //
           B(LdaSmi8), U8(1),       //
           B(Star), R(1),           //
           B(Ldar), R(1),           //
           B(Star), R(2),           //
           B(LdaSmi8), U8(10),      //
           B(Mul), R(2),            //
           B(Star), R(1),           //
           B(Ldar), R(0),           //
           B(Star), R(2),           //
           B(LdaSmi8), U8(5),       //
           B(TestEqual), R(2),      //
           B(JumpIfFalse), U8(4),   //
           B(Jump), U8(34),         //
           B(Ldar), R(0),           //
           B(Star), R(2),           //
           B(LdaSmi8), U8(6),       //
           B(TestEqual), R(2),      //
           B(JumpIfFalse), U8(4),   //
           B(Jump), U8(12),         //
           B(Ldar), R(0),           //
           B(Star), R(2),           //
           B(LdaSmi8), U8(1),       //
           B(Add), R(2),            //
           B(Star), R(0),           //
           B(Ldar), R(0),           //
           B(Star), R(2),           //
           B(LdaSmi8), U8(10),      //
           B(TestLessThan), R(2),   //
           B(JumpIfTrue), U8(-52),  //
           B(Ldar), R(1),           //
           B(Return)                //
       },
       0},
      {"var x = 0; "
       "for(;;) {"
       "  if (x == 1) break;"
       "  x = x + 1;"
       "}",
       2 * kPointerSize,
       1,
       29,
       {
           B(LdaZero),             //
           B(Star), R(0),          //
           B(Ldar), R(0),          //
           B(Star), R(1),          //
           B(LdaSmi8), U8(1),      //
           B(TestEqual), R(1),     //
           B(JumpIfFalse), U8(4),  //
           B(Jump), U8(14),        //
           B(Ldar), R(0),          //
           B(Star), R(1),          //
           B(LdaSmi8), U8(1),      //
           B(Add), R(1),           //
           B(Star), R(0),          //
           B(Jump), U8(-22),       //
           B(LdaUndefined),        //
           B(Return),              //
       },
       0},
      {"var u = 0;"
       "for(var i = 0; i < 100; i = i + 1) {"
       "   u = u + 1;"
       "   continue;"
       "}",
       3 * kPointerSize,
       1,
       42,
       {
           B(LdaZero),              //
           B(Star), R(0),           //
           B(LdaZero),              //
           B(Star), R(1),           //
           B(Jump), U8(24),         //
           B(Ldar), R(0),           //
           B(Star), R(2),           //
           B(LdaSmi8), U8(1),       //
           B(Add), R(2),            //
           B(Star), R(0),           //
           B(Jump), U8(2),          //
           B(Ldar), R(1),           //
           B(Star), R(2),           //
           B(LdaSmi8), U8(1),       //
           B(Add), R(2),            //
           B(Star), R(1),           //
           B(Ldar), R(1),           //
           B(Star), R(2),           //
           B(LdaSmi8), U8(100),     //
           B(TestLessThan), R(2),   //
           B(JumpIfTrue), U8(-30),  //
           B(LdaUndefined),         //
           B(Return),               //
       },
       0},
      {"var i = 0;"
       "while(true) {"
       "  while (i < 3) {"
       "    if (i == 2) break;"
       "    i = i + 1;"
       "  }"
       "  i = i + 1;"
       "  break;"
       "}"
       "return i;",
       2 * kPointerSize,
       1,
       57,
       {
           B(LdaZero),              //
           B(Star), R(0),           //
           B(Jump), U8(48),         //
           B(Jump), U8(24),         //
           B(Ldar), R(0),           //
           B(Star), R(1),           //
           B(LdaSmi8), U8(2),       //
           B(TestEqual), R(1),      //
           B(JumpIfFalse), U8(4),   //
           B(Jump), U8(22),         //
           B(Ldar), R(0),           //
           B(Star), R(1),           //
           B(LdaSmi8), U8(1),       //
           B(Add), R(1),            //
           B(Star), R(0),           //
           B(Ldar), R(0),           //
           B(Star), R(1),           //
           B(LdaSmi8), U8(3),       //
           B(TestLessThan), R(1),   //
           B(JumpIfTrue), U8(-30),  //
           B(Ldar), R(0),           //
           B(Star), R(1),           //
           B(LdaSmi8), U8(1),       //
           B(Add), R(1),            //
           B(Star), R(0),           //
           B(Jump), U8(5),          //
           B(LdaTrue),              //
           B(JumpIfTrue), U8(-47),  //
           B(Ldar), R(0),           //
           B(Return),               //
       },
       0},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(UnaryOperators) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  ExpectedSnippet<int> snippets[] = {
      {"var x = 0;"
       "while (x != 10) {"
       "  x = x + 10;"
       "}"
       "return x;",
       2 * kPointerSize,
       1,
       29,
       {
           B(LdaZero),              //
           B(Star), R(0),           //
           B(Jump), U8(12),         //
           B(Ldar), R(0),           //
           B(Star), R(1),           //
           B(LdaSmi8), U8(10),      //
           B(Add), R(1),            //
           B(Star), R(0),           //
           B(Ldar), R(0),           //
           B(Star), R(1),           //
           B(LdaSmi8), U8(10),      //
           B(TestEqual), R(1),      //
           B(LogicalNot),           //
           B(JumpIfTrue), U8(-19),  //
           B(Ldar), R(0),           //
           B(Return),               //
       },
       0},
      {"var x = false;"
       "do {"
       "  x = !x;"
       "} while(x == false);"
       "return x;",
       2 * kPointerSize,
       1,
       20,
       {
           B(LdaFalse),             //
           B(Star), R(0),           //
           B(Ldar), R(0),           //
           B(LogicalNot),           //
           B(Star), R(0),           //
           B(Ldar), R(0),           //
           B(Star), R(1),           //
           B(LdaFalse),             //
           B(TestEqual), R(1),      //
           B(JumpIfTrue), U8(-12),  //
           B(Ldar), R(0),           //
           B(Return),               //
       },
       0},
      {"var x = 101;"
       "return void(x * 3);",
       2 * kPointerSize,
       1,
       14,
       {
           B(LdaSmi8), U8(101),  //
           B(Star), R(0),        //
           B(Ldar), R(0),        //
           B(Star), R(1),        //
           B(LdaSmi8), U8(3),    //
           B(Mul), R(1),         //
           B(LdaUndefined),      //
           B(Return),            //
       },
       0},
      {"var x = 1234;"
       "var y = void (x * x - 1);"
       "return y;",
       4 * kPointerSize,
       1,
       24,
       {
           B(LdaConstant), U8(0),  //
           B(Star), R(0),          //
           B(Ldar), R(0),          //
           B(Star), R(3),          //
           B(Ldar), R(0),          //
           B(Mul), R(3),           //
           B(Star), R(2),          //
           B(LdaSmi8), U8(1),      //
           B(Sub), R(2),           //
           B(LdaUndefined),        //
           B(Star), R(1),          //
           B(Ldar), R(1),          //
           B(Return),              //
       },
       1,
       {1234}},
      {"var x = 13;"
       "return typeof(x);",
       1 * kPointerSize,
       1,
       8,
       {
           B(LdaSmi8), U8(13),  //
           B(Star), R(0),       //
           B(Ldar), R(0),       //
           B(TypeOf),           //
           B(Return),           //
       },
       0},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(FunctionLiterals) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  ExpectedSnippet<InstanceType> snippets[] = {
      {"return function(){ }",
       0,
       1,
       5,
       {
           B(LdaConstant), U8(0),    //
           B(CreateClosure), U8(0),  //
           B(Return)                 //
       },
       1,
       {InstanceType::SHARED_FUNCTION_INFO_TYPE}},
      {"return (function(){ })()",
       2 * kPointerSize,
       1,
       14,
       {
           B(LdaUndefined),             //
           B(Star), R(1),               //
           B(LdaConstant), U8(0),       //
           B(CreateClosure), U8(0),     //
           B(Star), R(0),               //
           B(Call), R(0), R(1), U8(0),  //
           B(Return)                    //
       },
       1,
       {InstanceType::SHARED_FUNCTION_INFO_TYPE}},
      {"return (function(x){ return x; })(1)",
       3 * kPointerSize,
       1,
       18,
       {
           B(LdaUndefined),             //
           B(Star), R(1),               //
           B(LdaConstant), U8(0),       //
           B(CreateClosure), U8(0),     //
           B(Star), R(0),               //
           B(LdaSmi8), U8(1),           //
           B(Star), R(2),               //
           B(Call), R(0), R(1), U8(1),  //
           B(Return)                    //
       },
       1,
       {InstanceType::SHARED_FUNCTION_INFO_TYPE}},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(RegExpLiterals) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;
  Zone zone;

  FeedbackVectorSpec feedback_spec(&zone);
  feedback_spec.AddLoadICSlot();
  FeedbackVectorSlot slot2 = feedback_spec.AddLoadICSlot();

  Handle<i::TypeFeedbackVector> vector =
      i::NewTypeFeedbackVector(helper.isolate(), &feedback_spec);

  ExpectedSnippet<const char*> snippets[] = {
      {"return /ab+d/;",
       1 * kPointerSize,
       1,
       10,
       {
           B(LdaConstant), U8(0),                //
           B(Star), R(0),                        //
           B(LdaConstant), U8(1),                //
           B(CreateRegExpLiteral), U8(0), R(0),  //
           B(Return),                            //
       },
       2,
       {"", "ab+d"}},
      {"return /(\\w+)\\s(\\w+)/i;",
       1 * kPointerSize,
       1,
       10,
       {
           B(LdaConstant), U8(0),                //
           B(Star), R(0),                        //
           B(LdaConstant), U8(1),                //
           B(CreateRegExpLiteral), U8(0), R(0),  //
           B(Return),                            //
       },
       2,
       {"i", "(\\w+)\\s(\\w+)"}},
      {"return /ab+d/.exec('abdd');",
       3 * kPointerSize,
       1,
       27,
       {
           B(LdaConstant), U8(0),                               //
           B(Star), R(2),                                       //
           B(LdaConstant), U8(1),                               //
           B(CreateRegExpLiteral), U8(0), R(2),                 //
           B(Star), R(1),                                       //
           B(LdaConstant), U8(2),                               //
           B(LoadICSloppy), R(1), U8(vector->GetIndex(slot2)),  //
           B(Star), R(0),                                       //
           B(LdaConstant), U8(3),                               //
           B(Star), R(2),                                       //
           B(Call), R(0), R(1), U8(1),                          //
           B(Return),                                           //
       },
       4,
       {"", "ab+d", "exec", "abdd"}},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(ArrayLiterals) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;
  Zone zone;

  FeedbackVectorSpec feedback_spec(&zone);
  FeedbackVectorSlot slot1 = feedback_spec.AddKeyedStoreICSlot();
  FeedbackVectorSlot slot2 = feedback_spec.AddKeyedStoreICSlot();
  FeedbackVectorSlot slot3 = feedback_spec.AddKeyedStoreICSlot();

  Handle<i::TypeFeedbackVector> vector =
      i::NewTypeFeedbackVector(helper.isolate(), &feedback_spec);

  int simple_flags =
      ArrayLiteral::kDisableMementos | ArrayLiteral::kShallowElements;
  int deep_elements_flags = ArrayLiteral::kDisableMementos;
  ExpectedSnippet<InstanceType> snippets[] = {
      {"return [ 1, 2 ];",
       0,
       1,
       6,
       {
           B(LdaConstant), U8(0),                           //
           B(CreateArrayLiteral), U8(0), U8(simple_flags),  //
           B(Return)                                        //
       },
       1,
       {InstanceType::FIXED_ARRAY_TYPE}},
      {"var a = 1; return [ a, a + 1 ];",
       4 * kPointerSize,
       1,
       39,
       {
           B(LdaSmi8), U8(1),                                               //
           B(Star), R(0),                                                   //
           B(LdaConstant), U8(0),                                           //
           B(CreateArrayLiteral), U8(0), U8(3),                             //
           B(Star), R(2),                                                   //
           B(LdaZero),                                                      //
           B(Star), R(1),                                                   //
           B(Ldar), R(0),                                                   //
           B(KeyedStoreICSloppy), R(2), R(1), U8(vector->GetIndex(slot1)),  //
           B(LdaSmi8), U8(1),                                               //
           B(Star), R(1),                                                   //
           B(Ldar), R(0),                                                   //
           B(Star), R(3),                                                   //
           B(LdaSmi8), U8(1),                                               //
           B(Add), R(3),                                                    //
           B(KeyedStoreICSloppy), R(2), R(1), U8(vector->GetIndex(slot1)),  //
           B(Ldar), R(2),                                                   //
           B(Return)                                                        //
       },
       1,
       {InstanceType::FIXED_ARRAY_TYPE}},
      {"return [ [ 1, 2 ], [ 3 ] ];",
       0,
       1,
       6,
       {
           B(LdaConstant), U8(0),                                  //
           B(CreateArrayLiteral), U8(2), U8(deep_elements_flags),  //
           B(Return)                                               //
       },
       1,
       {InstanceType::FIXED_ARRAY_TYPE}},
      {"var a = 1; return [ [ a, 2 ], [ a + 2 ] ];",
       6 * kPointerSize,
       1,
       71,
       {
           B(LdaSmi8), U8(1),                                               //
           B(Star), R(0),                                                   //
           B(LdaConstant), U8(0),                                           //
           B(CreateArrayLiteral), U8(2), U8(deep_elements_flags),           //
           B(Star), R(2),                                                   //
           B(LdaZero),                                                      //
           B(Star), R(1),                                                   //
           B(LdaConstant), U8(1),                                           //
           B(CreateArrayLiteral), U8(0), U8(simple_flags),                  //
           B(Star), R(4),                                                   //
           B(LdaZero),                                                      //
           B(Star), R(3),                                                   //
           B(Ldar), R(0),                                                   //
           B(KeyedStoreICSloppy), R(4), R(3), U8(vector->GetIndex(slot1)),  //
           B(Ldar), R(4),                                                   //
           B(KeyedStoreICSloppy), R(2), R(1), U8(vector->GetIndex(slot3)),  //
           B(LdaSmi8), U8(1),                                               //
           B(Star), R(1),                                                   //
           B(LdaConstant), U8(2),                                           //
           B(CreateArrayLiteral), U8(1), U8(simple_flags),                  //
           B(Star), R(4),                                                   //
           B(LdaZero),                                                      //
           B(Star), R(3),                                                   //
           B(Ldar), R(0),                                                   //
           B(Star), R(5),                                                   //
           B(LdaSmi8), U8(2),                                               //
           B(Add), R(5),                                                    //
           B(KeyedStoreICSloppy), R(4), R(3), U8(vector->GetIndex(slot2)),  //
           B(Ldar), R(4),                                                   //
           B(KeyedStoreICSloppy), R(2), R(1), U8(vector->GetIndex(slot3)),  //
           B(Ldar), R(2),                                                   //
           B(Return),                                                       //
       },
       3,
       {InstanceType::FIXED_ARRAY_TYPE, InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::FIXED_ARRAY_TYPE}},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(ObjectLiterals) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  int simple_flags = ObjectLiteral::kFastElements |
                     ObjectLiteral::kShallowProperties |
                     ObjectLiteral::kDisableMementos;
  int deep_elements_flags =
      ObjectLiteral::kFastElements | ObjectLiteral::kDisableMementos;
  ExpectedSnippet<InstanceType> snippets[] = {
      {"return { };",
       0,
       1,
       6,
       {
           B(LdaConstant), U8(0),                            //
           B(CreateObjectLiteral), U8(0), U8(simple_flags),  //
           B(Return)                                         //
       },
       1,
       {InstanceType::FIXED_ARRAY_TYPE}},
      {"return { name: 'string', val: 9.2 };",
       0,
       1,
       6,
       {
           B(LdaConstant), U8(0),                                   //
           B(CreateObjectLiteral), U8(0), U8(deep_elements_flags),  //
           B(Return)                                                //
       },
       1,
       {InstanceType::FIXED_ARRAY_TYPE}},
      {"var a = 1; return { name: 'string', val: a };",
       3 * kPointerSize,
       1,
       24,
       {
           B(LdaSmi8), U8(1),                                       //
           B(Star), R(0),                                           //
           B(LdaConstant), U8(0),                                   //
           B(CreateObjectLiteral), U8(0), U8(deep_elements_flags),  //
           B(Star), R(1),                                           //
           B(LdaConstant), U8(1),                                   //
           B(Star), R(2),                                           //
           B(Ldar), R(0),                                           //
           B(StoreICSloppy), R(1), R(2), U8(3),                     //
           B(Ldar), R(1),                                           //
           B(Return),                                               //
       },
       2,
       {InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE}},
      {"var a = 1; return { val: a, val: a + 1 };",
       4 * kPointerSize,
       1,
       32,
       {
           B(LdaSmi8), U8(1),                                       //
           B(Star), R(0),                                           //
           B(LdaConstant), U8(0),                                   //
           B(CreateObjectLiteral), U8(0), U8(deep_elements_flags),  //
           B(Star), R(1),                                           //
           B(Ldar), R(0),                                           //
           B(LdaConstant), U8(1),                                   //
           B(Star), R(2),                                           //
           B(Ldar), R(0),                                           //
           B(Star), R(3),                                           //
           B(LdaSmi8), U8(1),                                       //
           B(Add), R(3),                                            //
           B(StoreICSloppy), R(1), R(2), U8(3),                     //
           B(Ldar), R(1),                                           //
           B(Return),                                               //
       },
       2,
       {InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE}},
      {"return { func: function() { } };",
       2 * kPointerSize,
       1,
       22,
       {
           B(LdaConstant), U8(0),                                   //
           B(CreateObjectLiteral), U8(0), U8(deep_elements_flags),  //
           B(Star), R(0),                                           //
           B(LdaConstant), U8(1),                                   //
           B(Star), R(1),                                           //
           B(LdaConstant), U8(2),                                   //
           B(CreateClosure), U8(0),                                 //
           B(StoreICSloppy), R(0), R(1), U8(3),                     //
           B(Ldar), R(0),                                           //
           B(Return),                                               //
       },
       3,
       {InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE,
        InstanceType::SHARED_FUNCTION_INFO_TYPE}},
      {"return { func(a) { return a; } };",
       2 * kPointerSize,
       1,
       22,
       {
           B(LdaConstant), U8(0),                                   //
           B(CreateObjectLiteral), U8(0), U8(deep_elements_flags),  //
           B(Star), R(0),                                           //
           B(LdaConstant), U8(1),                                   //
           B(Star), R(1),                                           //
           B(LdaConstant), U8(2),                                   //
           B(CreateClosure), U8(0),                                 //
           B(StoreICSloppy), R(0), R(1), U8(3),                     //
           B(Ldar), R(0),                                           //
           B(Return),                                               //
       },
       3,
       {InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE,
        InstanceType::SHARED_FUNCTION_INFO_TYPE}},
      {"return { get a() { return 2; } };",
       5 * kPointerSize,
       1,
       31,
       {
           B(LdaConstant), U8(0),                                           //
           B(CreateObjectLiteral), U8(0), U8(deep_elements_flags),          //
           B(Star), R(0),                                                   //
           B(LdaConstant), U8(1),                                           //
           B(Star), R(1),                                                   //
           B(LdaConstant), U8(2),                                           //
           B(CreateClosure), U8(0),                                         //
           B(Star), R(2),                                                   //
           B(LdaNull),                                                      //
           B(Star), R(3),                                                   //
           B(LdaZero),                                                      //
           B(Star), R(4),                                                   //
           B(CallRuntime), U16(Runtime::kDefineAccessorPropertyUnchecked),  //
                           R(0), U8(5),                                     //
           B(Ldar), R(0),                                                   //
           B(Return),                                                       //
       },
       3,
       {InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE,
        InstanceType::SHARED_FUNCTION_INFO_TYPE}},
      {"return { get a() { return this.x; }, set a(val) { this.x = val } };",
       5 * kPointerSize,
       1,
       34,
       {
           B(LdaConstant), U8(0),                                           //
           B(CreateObjectLiteral), U8(0), U8(deep_elements_flags),          //
           B(Star), R(0),                                                   //
           B(LdaConstant), U8(1),                                           //
           B(Star), R(1),                                                   //
           B(LdaConstant), U8(2),                                           //
           B(CreateClosure), U8(0),                                         //
           B(Star), R(2),                                                   //
           B(LdaConstant), U8(3),                                           //
           B(CreateClosure), U8(0),                                         //
           B(Star), R(3),                                                   //
           B(LdaZero),                                                      //
           B(Star), R(4),                                                   //
           B(CallRuntime), U16(Runtime::kDefineAccessorPropertyUnchecked),  //
                           R(0), U8(5),                                     //
           B(Ldar), R(0),                                                   //
           B(Return),                                                       //
       },
       4,
       {InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE,
        InstanceType::SHARED_FUNCTION_INFO_TYPE,
        InstanceType::SHARED_FUNCTION_INFO_TYPE}},
      {"return { set b(val) { this.y = val } };",
       5 * kPointerSize,
       1,
       31,
       {
           B(LdaConstant), U8(0),                                           //
           B(CreateObjectLiteral), U8(0), U8(deep_elements_flags),          //
           B(Star), R(0),                                                   //
           B(LdaConstant), U8(1),                                           //
           B(Star), R(1),                                                   //
           B(LdaNull),                                                      //
           B(Star), R(2),                                                   //
           B(LdaConstant), U8(2),                                           //
           B(CreateClosure), U8(0),                                         //
           B(Star), R(3),                                                   //
           B(LdaZero),                                                      //
           B(Star), R(4),                                                   //
           B(CallRuntime), U16(Runtime::kDefineAccessorPropertyUnchecked),  //
                           R(0), U8(5),                                     //
           B(Ldar), R(0),                                                   //
           B(Return),                                                       //
       },
       3,
       {InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE,
        InstanceType::SHARED_FUNCTION_INFO_TYPE}},
      {"var a = 1; return { 1: a };",
       5 * kPointerSize,
       1,
       30,
       {
           B(LdaSmi8), U8(1),                                        //
           B(Star), R(0),                                            //
           B(LdaConstant), U8(0),                                    //
           B(CreateObjectLiteral), U8(0), U8(deep_elements_flags),   //
           B(Star), R(1),                                            //
           B(LdaSmi8), U8(1),                                        //
           B(Star), R(2),                                            //
           B(Ldar), R(0),                                            //
           B(Star), R(3),                                            //
           B(LdaZero),                                               //
           B(Star), R(4),                                            //
           B(CallRuntime), U16(Runtime::kSetProperty), R(1), U8(4),  //
           B(Ldar), R(1),                                            //
           B(Return),                                                //
       },
       1,
       {InstanceType::FIXED_ARRAY_TYPE}},
      {"return { __proto__: null }",
       2 * kPointerSize,
       1,
       18,
       {
           B(LdaConstant), U8(0),                                             //
           B(CreateObjectLiteral), U8(0), U8(simple_flags),                   //
           B(Star), R(0),                                                     //
           B(LdaNull), B(Star), R(1),                                         //
           B(CallRuntime), U16(Runtime::kInternalSetPrototype), R(0), U8(2),  //
           B(Ldar), R(0),                                                     //
           B(Return),                                                         //
       },
       1,
       {InstanceType::FIXED_ARRAY_TYPE}},
      {"var a = 'test'; return { [a]: 1 }",
       5 * kPointerSize,
       1,
       31,
       {
           B(LdaConstant), U8(0),                                             //
           B(Star), R(0),                                                     //
           B(LdaConstant), U8(1),                                             //
           B(CreateObjectLiteral), U8(0), U8(simple_flags),                   //
           B(Star), R(1),                                                     //
           B(Ldar), R(0),                                                     //
           B(ToName),                                                         //
           B(Star), R(2),                                                     //
           B(LdaSmi8), U8(1),                                                 //
           B(Star), R(3),                                                     //
           B(LdaZero),                                                        //
           B(Star), R(4),                                                     //
           B(CallRuntime), U16(Runtime::kDefineDataPropertyUnchecked), R(1),  //
                           U8(4),                                             //
           B(Ldar), R(1),                                                     //
           B(Return),                                                         //
       },
       2,
       {InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE,
        InstanceType::FIXED_ARRAY_TYPE}},
      {"var a = 'test'; return { val: a, [a]: 1 }",
       5 * kPointerSize,
       1,
       41,
       {
           B(LdaConstant), U8(0),                                             //
           B(Star), R(0),                                                     //
           B(LdaConstant), U8(1),                                             //
           B(CreateObjectLiteral), U8(0), U8(deep_elements_flags),            //
           B(Star), R(1),                                                     //
           B(LdaConstant), U8(2),                                             //
           B(Star), R(2),                                                     //
           B(Ldar), R(0),                                                     //
           B(StoreICSloppy), R(1), R(2), U8(3),                               //
           B(Ldar), R(0),                                                     //
           B(ToName),                                                         //
           B(Star), R(2),                                                     //
           B(LdaSmi8), U8(1),                                                 //
           B(Star), R(3),                                                     //
           B(LdaZero),                                                        //
           B(Star), R(4),                                                     //
           B(CallRuntime), U16(Runtime::kDefineDataPropertyUnchecked), R(1),  //
                           U8(4),                                             //
           B(Ldar), R(1),                                                     //
           B(Return),                                                         //
       },
       3,
       {InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE,
        InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE}},
      {"var a = 'test'; return { [a]: 1, __proto__: {} }",
       5 * kPointerSize,
       1,
       43,
       {
           B(LdaConstant), U8(0),                                             //
           B(Star), R(0),                                                     //
           B(LdaConstant), U8(1),                                             //
           B(CreateObjectLiteral), U8(1), U8(simple_flags),                   //
           B(Star), R(1),                                                     //
           B(Ldar), R(0),                                                     //
           B(ToName),                                                         //
           B(Star), R(2),                                                     //
           B(LdaSmi8), U8(1),                                                 //
           B(Star), R(3),                                                     //
           B(LdaZero),                                                        //
           B(Star), R(4),                                                     //
           B(CallRuntime), U16(Runtime::kDefineDataPropertyUnchecked), R(1),  //
                           U8(4),                                             //
           B(LdaConstant), U8(1),                                             //
           B(CreateObjectLiteral), U8(0), U8(13),                             //
           B(Star), R(2),                                                     //
           B(CallRuntime), U16(Runtime::kInternalSetPrototype), R(1), U8(2),  //
           B(Ldar), R(1),                                                     //
           B(Return),                                                         //
       },
       2,
       {InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE,
        InstanceType::FIXED_ARRAY_TYPE}},
      {"var n = 'name'; return { [n]: 'val', get a() { }, set a(b) {} };",
       5 * kPointerSize,
       1,
       69,
       {
           B(LdaConstant), U8(0),                                             //
           B(Star), R(0),                                                     //
           B(LdaConstant), U8(1),                                             //
           B(CreateObjectLiteral), U8(0), U8(simple_flags),                   //
           B(Star), R(1),                                                     //
           B(Ldar), R(0),                                                     //
           B(ToName),                                                         //
           B(Star), R(2),                                                     //
           B(LdaConstant), U8(2),                                             //
           B(Star), R(3),                                                     //
           B(LdaZero),                                                        //
           B(Star), R(4),                                                     //
           B(CallRuntime), U16(Runtime::kDefineDataPropertyUnchecked), R(1),  //
                           U8(4),                                             //
           B(LdaConstant), U8(3),                                             //
           B(ToName),                                                         //
           B(Star), R(2),                                                     //
           B(LdaConstant), U8(4),                                             //
           B(CreateClosure), U8(0),                                           //
           B(Star), R(3),                                                     //
           B(LdaZero),                                                        //
           B(Star), R(4),                                                     //
           B(CallRuntime), U16(Runtime::kDefineGetterPropertyUnchecked),      //
                           R(1), U8(4),                                       //
           B(LdaConstant), U8(3),                                             //
           B(ToName),                                                         //
           B(Star), R(2),                                                     //
           B(LdaConstant), U8(5),                                             //
           B(CreateClosure), U8(0),                                           //
           B(Star), R(3),                                                     //
           B(LdaZero),                                                        //
           B(Star), R(4),                                                     //
           B(CallRuntime), U16(Runtime::kDefineSetterPropertyUnchecked),      //
                           R(1), U8(4),                                       //
           B(Ldar), R(1),                                                     //
           B(Return),                                                         //
       },
       6,
       {InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE,
        InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE,
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE,
        InstanceType::SHARED_FUNCTION_INFO_TYPE,
        InstanceType::SHARED_FUNCTION_INFO_TYPE}},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(TopLevelObjectLiterals) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  int has_function_flags = ObjectLiteral::kFastElements |
                           ObjectLiteral::kHasFunction |
                           ObjectLiteral::kDisableMementos;
  ExpectedSnippet<InstanceType> snippets[] = {
      {"var a = { func: function() { } };",
       7 * kPointerSize,
       1,
       69,
       {
           B(Ldar), R(Register::function_closure().index()),                 //
           B(Star), R(2),                                                    //
           B(LdaConstant), U8(0),                                            //
           B(Star), R(3),                                                    //
           B(CallRuntime), U16(Runtime::kNewScriptContext), R(2), U8(2),     //
           B(PushContext), R(1),                                             //
           B(LdaConstant), U8(1),                                            //
           B(Star), R(2),                                                    //
           B(LdaZero),                                                       //
           B(Star), R(3),                                                    //
           B(CallRuntime), U16(Runtime::kDeclareGlobals), R(2), U8(2),       //
           B(LdaConstant), U8(2),                                            //
           B(Star), R(2),                                                    //
           B(LdaZero),                                                       //
           B(Star), R(3),                                                    //
           B(LdaConstant), U8(3),                                            //
           B(CreateObjectLiteral), U8(0), U8(has_function_flags),            //
           B(Star), R(5),                                                    //
           B(LdaConstant), U8(4),                                            //
           B(Star), R(6),                                                    //
           B(LdaConstant), U8(5),                                            //
           B(CreateClosure), U8(1),                                          //
           B(StoreICSloppy), R(5), R(6), U8(3),                              //
           B(CallRuntime), U16(Runtime::kToFastProperties), R(5), U8(1),     //
           B(Ldar), R(5),                                                    //
           B(Star), R(4),                                                    //
           B(CallRuntime), U16(Runtime::kInitializeVarGlobal), R(2), U8(3),  //
           B(LdaUndefined),                                                  //
           B(Return),                                                        //
       },
       6,
       {InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE,
        InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE,
        InstanceType::SHARED_FUNCTION_INFO_TYPE}},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeTopLevelBytecode(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(TryCatch) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  // TODO(rmcilroy): modify tests when we have real try catch support.
  ExpectedSnippet<int> snippets[] = {
      {"try { return 1; } catch(e) { return 2; }",
       1 * kPointerSize,
       1,
       5,
       {
           B(LdaSmi8), U8(1),  //
           B(Return),          //
           B(LdaUndefined),    //
           B(Return),          //
       },
       0},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(TryFinally) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  // TODO(rmcilroy): modify tests when we have real try finally support.
  ExpectedSnippet<int> snippets[] = {
      {"var a = 1; try { a = 2; } finally { a = 3; }",
       1 * kPointerSize,
       1,
       14,
       {
           B(LdaSmi8), U8(1),  //
           B(Star), R(0),      //
           B(LdaSmi8), U8(2),  //
           B(Star), R(0),      //
           B(LdaSmi8), U8(3),  //
           B(Star), R(0),      //
           B(LdaUndefined),    //
           B(Return),          //
       },
       0},
      {"var a = 1; try { a = 2; } catch(e) { a = 20 } finally { a = 3; }",
       2 * kPointerSize,
       1,
       14,
       {
           B(LdaSmi8), U8(1),  //
           B(Star), R(0),      //
           B(LdaSmi8), U8(2),  //
           B(Star), R(0),      //
           B(LdaSmi8), U8(3),  //
           B(Star), R(0),      //
           B(LdaUndefined),    //
           B(Return),          //
       },
       0},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(CallNew) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  ExpectedSnippet<InstanceType> snippets[] = {
      {"function bar() { this.value = 0; }\n"
       "function f() { return new bar(); }\n"
       "f()",
       kPointerSize,
       1,
       9,
       {
           B(LdaGlobal), _,            //
           B(Star), R(0),              //
           B(New), R(0), R(0), U8(0),  //
           B(Return),                  //
       },
       0},
      {"function bar(x) { this.value = 18; this.x = x;}\n"
       "function f() { return new bar(3); }\n"
       "f()",
       2 * kPointerSize,
       1,
       13,
       {
           B(LdaGlobal), _,            //
           B(Star), R(0),              //
           B(LdaSmi8), U8(3),          //
           B(Star), R(1),              //
           B(New), R(0), R(1), U8(1),  //
           B(Return),                  //
       },
       0},
      {"function bar(w, x, y, z) {\n"
       "  this.value = 18;\n"
       "  this.x = x;\n"
       "  this.y = y;\n"
       "  this.z = z;\n"
       "}\n"
       "function f() { return new bar(3, 4, 5); }\n"
       "f()",
       4 * kPointerSize,
       1,
       21,
       {
           B(LdaGlobal), _,            //
           B(Star), R(0),              //
           B(LdaSmi8), U8(3),          //
           B(Star), R(1),              //
           B(LdaSmi8), U8(4),          //
           B(Star), R(2),              //
           B(LdaSmi8), U8(5),          //
           B(Star), R(3),              //
           B(New), R(0), R(1), U8(3),  //
           B(Return),                  //
       },
       0}};

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecode(snippets[i].code_snippet, "f");
    CheckBytecodeArrayEqual(snippets[i], bytecode_array, true);
  }
}


TEST(ContextVariables) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  int closure = Register::function_closure().index();
  int first_context_slot = Context::MIN_CONTEXT_SLOTS;
  ExpectedSnippet<InstanceType> snippets[] = {
      {"var a; return function() { a = 1; };",
       1 * kPointerSize,
       1,
       12,
       {
           B(CallRuntime), U16(Runtime::kNewFunctionContext),  //
                           R(closure), U8(1),                  //
           B(PushContext), R(0),                               //
           B(LdaConstant), U8(0),                              //
           B(CreateClosure), U8(0),                            //
           B(Return),                                          //
       },
       1,
       {InstanceType::SHARED_FUNCTION_INFO_TYPE}},
      {"var a = 1; return function() { a = 2; };",
       1 * kPointerSize,
       1,
       17,
       {
           B(CallRuntime), U16(Runtime::kNewFunctionContext),  //
                           R(closure), U8(1),                  //
           B(PushContext), R(0),                               //
           B(LdaSmi8), U8(1),                                  //
           B(StaContextSlot), R(0), U8(first_context_slot),    //
           B(LdaConstant), U8(0),                              //
           B(CreateClosure), U8(0),                            //
           B(Return),                                          //
       },
       1,
       {InstanceType::SHARED_FUNCTION_INFO_TYPE}},
      {"var a = 1; var b = 2; return function() { a = 2; b = 3 };",
       1 * kPointerSize,
       1,
       22,
       {
           B(CallRuntime), U16(Runtime::kNewFunctionContext),    //
                           R(closure), U8(1),                    //
           B(PushContext), R(0),                                 //
           B(LdaSmi8), U8(1),                                    //
           B(StaContextSlot), R(0), U8(first_context_slot),      //
           B(LdaSmi8), U8(2),                                    //
           B(StaContextSlot), R(0), U8(first_context_slot + 1),  //
           B(LdaConstant), U8(0),                                //
           B(CreateClosure), U8(0),                              //
           B(Return),                                            //
       },
       1,
       {InstanceType::SHARED_FUNCTION_INFO_TYPE}},
      {"var a; (function() { a = 2; })(); return a;",
       3 * kPointerSize,
       1,
       24,
       {
           B(CallRuntime), U16(Runtime::kNewFunctionContext),  //
                           R(closure), U8(1),                  //
           B(PushContext), R(0),                               //
           B(LdaUndefined),                                    //
           B(Star), R(2),                                      //
           B(LdaConstant), U8(0),                              //
           B(CreateClosure), U8(0),                            //
           B(Star), R(1),                                      //
           B(Call), R(1), R(2), U8(0),                         //
           B(LdaContextSlot), R(0), U8(first_context_slot),    //
           B(Return),                                          //
       },
       1,
       {InstanceType::SHARED_FUNCTION_INFO_TYPE}},
      {"'use strict'; let a = 1; { let b = 2; return function() { a + b; }; }",
       4 * kPointerSize,
       1,
       51,
       {
           B(CallRuntime), U16(Runtime::kNewFunctionContext),             //
                           R(closure), U8(1),                             //
           B(PushContext), R(0),                                          //
           B(LdaTheHole),                                                 //
           B(StaContextSlot), R(0), U8(first_context_slot),               //
           B(LdaConstant), U8(0),                                         //
           B(LdaSmi8), U8(1),                                             //
           B(StaContextSlot), R(0), U8(first_context_slot),               //
           B(LdaConstant), U8(1),                                         //
           B(Star), R(2),                                                 //
           B(Ldar), R(closure),                                           //
           B(Star), R(3),                                                 //
           B(CallRuntime), U16(Runtime::kPushBlockContext), R(2), U8(2),  //
           B(PushContext), R(1),                                          //
           B(LdaTheHole),                                                 //
           B(StaContextSlot), R(1), U8(first_context_slot),               //
           B(LdaSmi8), U8(2),                                             //
           B(StaContextSlot), R(1), U8(first_context_slot),               //
           B(LdaConstant), U8(2),                                         //
           B(CreateClosure), U8(0),                                       //
           B(Return),                                                     //
           // TODO(rmcilroy): Dead code after this point due to return in nested
           // block - investigate eliminating this.
           B(PopContext), R(0),
           B(LdaUndefined),  //
           B(Return),        //
       },
       3,
       {InstanceType::ONE_BYTE_INTERNALIZED_STRING_TYPE,
        InstanceType::FIXED_ARRAY_TYPE,
        InstanceType::SHARED_FUNCTION_INFO_TYPE}},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunctionBody(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}


TEST(ContextParameters) {
  InitializedHandleScope handle_scope;
  BytecodeGeneratorHelper helper;

  int closure = Register::function_closure().index();
  int first_context_slot = Context::MIN_CONTEXT_SLOTS;
  ExpectedSnippet<InstanceType> snippets[] = {
      {"function f(arg1) { return function() { arg1 = 2; }; }",
       1 * kPointerSize,
       2,
       17,
       {
           B(CallRuntime), U16(Runtime::kNewFunctionContext),  //
                           R(closure), U8(1),                  //
           B(PushContext), R(0),                               //
           B(Ldar), R(helper.kLastParamIndex),                 //
           B(StaContextSlot), R(0), U8(first_context_slot),    //
           B(LdaConstant), U8(0),                              //
           B(CreateClosure), U8(0),                            //
           B(Return),                                          //
       },
       1,
       {InstanceType::SHARED_FUNCTION_INFO_TYPE}},
      {"function f(arg1) { var a = function() { arg1 = 2; }; return arg1; }",
       2 * kPointerSize,
       2,
       22,
       {
           B(CallRuntime), U16(Runtime::kNewFunctionContext),  //
                           R(closure), U8(1),                  //
           B(PushContext), R(1),                               //
           B(Ldar), R(helper.kLastParamIndex),                 //
           B(StaContextSlot), R(1), U8(first_context_slot),    //
           B(LdaConstant), U8(0),                              //
           B(CreateClosure), U8(0),                            //
           B(Star), R(0),                                      //
           B(LdaContextSlot), R(1), U8(first_context_slot),    //
           B(Return),                                          //
       },
       1,
       {InstanceType::SHARED_FUNCTION_INFO_TYPE}},
      {"function f(a1, a2, a3, a4) { return function() { a1 = a3; }; }",
       1 * kPointerSize,
       5,
       22,
       {
           B(CallRuntime), U16(Runtime::kNewFunctionContext),    //
                           R(closure), U8(1),                    //
           B(PushContext), R(0),                                 //
           B(Ldar), R(helper.kLastParamIndex - 3),               //
           B(StaContextSlot), R(0), U8(first_context_slot + 1),  //
           B(Ldar), R(helper.kLastParamIndex -1),                //
           B(StaContextSlot), R(0), U8(first_context_slot),      //
           B(LdaConstant), U8(0),                                //
           B(CreateClosure), U8(0),                              //
           B(Return),                                            //
       },
       1,
       {InstanceType::SHARED_FUNCTION_INFO_TYPE}},
      {"function f() { var self = this; return function() { self = 2; }; }",
       1 * kPointerSize,
       1,
       17,
       {
           B(CallRuntime), U16(Runtime::kNewFunctionContext),  //
                           R(closure), U8(1),                  //
           B(PushContext), R(0),                               //
           B(Ldar), R(helper.kLastParamIndex),                 //
           B(StaContextSlot), R(0), U8(first_context_slot),    //
           B(LdaConstant), U8(0),                              //
           B(CreateClosure), U8(0),                            //
           B(Return),                                          //
       },
       1,
       {InstanceType::SHARED_FUNCTION_INFO_TYPE}},
  };

  for (size_t i = 0; i < arraysize(snippets); i++) {
    Handle<BytecodeArray> bytecode_array =
        helper.MakeBytecodeForFunction(snippets[i].code_snippet);
    CheckBytecodeArrayEqual(snippets[i], bytecode_array);
  }
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
