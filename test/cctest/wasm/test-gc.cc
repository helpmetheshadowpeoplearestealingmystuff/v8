// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "src/utils/utils.h"
#include "src/utils/vector.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/struct-types.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-opcodes.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/value-helper.h"
#include "test/cctest/wasm/wasm-run-utils.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"
#include "test/common/wasm/wasm-module-runner.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace test_gc {

using F = std::pair<ValueType, bool>;

class WasmGCTester {
 public:
  WasmGCTester()
      : flag_gc(&v8::internal::FLAG_experimental_wasm_gc, true),
        flag_reftypes(&v8::internal::FLAG_experimental_wasm_anyref, true),
        flag_typedfuns(&v8::internal::FLAG_experimental_wasm_typed_funcref,
                       true),
        allocator(),
        zone(&allocator, ZONE_NAME),
        builder(&zone),
        isolate(CcTest::InitIsolateOnce()),
        scope(isolate),
        instance(),
        thrower(isolate, "Test wasm GC") {
    testing::SetupIsolateForWasmModule(isolate);
  }

  uint32_t AddGlobal(ValueType type, bool mutability, WasmInitExpr init) {
    return builder.AddGlobal(type, mutability, init);
  }

  void DefineFunction(const char* name, FunctionSig* sig,
                      std::initializer_list<ValueType> locals,
                      std::initializer_list<byte> code) {
    WasmFunctionBuilder* fun = builder.AddFunction(sig);
    builder.AddExport(CStrVector(name), fun);
    for (ValueType local : locals) {
      fun->AddLocal(local);
    }
    fun->EmitCode(code.begin(), static_cast<uint32_t>(code.size()));
  }

  uint32_t DefineStruct(std::initializer_list<F> fields) {
    StructType::Builder type_builder(&zone,
                                     static_cast<uint32_t>(fields.size()));
    for (F field : fields) {
      type_builder.AddField(field.first, field.second);
    }
    return builder.AddStructType(type_builder.Build());
  }

  uint32_t DefineArray(ValueType element_type, bool mutability) {
    return builder.AddArrayType(new (&zone)
                                    ArrayType(element_type, mutability));
  }

  void CompileModule() {
    ZoneBuffer buffer(&zone);
    builder.WriteTo(&buffer);
    MaybeHandle<WasmInstanceObject> maybe_instance =
        testing::CompileAndInstantiateForTesting(
            isolate, &thrower, ModuleWireBytes(buffer.begin(), buffer.end()));
    if (thrower.error()) FATAL("%s", thrower.error_msg());
    instance = maybe_instance.ToHandleChecked();
  }

  void CheckResult(const char* function, int32_t expected,
                   std::initializer_list<Object> args) {
    Handle<Object>* argv = zone.NewArray<Handle<Object>>(args.size());
    int i = 0;
    for (Object arg : args) {
      argv[i++] = handle(arg, isolate);
    }
    CHECK_EQ(expected, testing::CallWasmFunctionForTesting(
                           isolate, instance, &thrower, function,
                           static_cast<uint32_t>(args.size()), argv));
  }

  // TODO(7748): This uses the JavaScript interface to retrieve the plain
  // WasmStruct. Once the JS interaction story is settled, this may well
  // need to be changed.
  MaybeHandle<Object> GetJSResult(const char* function,
                                  std::initializer_list<Object> args) {
    Handle<Object>* argv = zone.NewArray<Handle<Object>>(args.size());
    int i = 0;
    for (Object arg : args) {
      argv[i++] = handle(arg, isolate);
    }
    Handle<WasmExportedFunction> exported =
        testing::GetExportedFunction(isolate, instance, function)
            .ToHandleChecked();
    return Execution::Call(isolate, exported,
                           isolate->factory()->undefined_value(),
                           static_cast<uint32_t>(args.size()), argv);
  }

  void CheckHasThrown(const char* function,
                      std::initializer_list<Object> args) {
    TryCatch try_catch(reinterpret_cast<v8::Isolate*>(isolate));
    MaybeHandle<Object> result = GetJSResult(function, args);
    CHECK(result.is_null());
    CHECK(try_catch.HasCaught());
    isolate->clear_pending_exception();
  }

  TestSignatures sigs;

 private:
  const FlagScope<bool> flag_gc;
  const FlagScope<bool> flag_reftypes;
  const FlagScope<bool> flag_typedfuns;

  v8::internal::AccountingAllocator allocator;
  Zone zone;
  WasmModuleBuilder builder;

  Isolate* const isolate;
  const HandleScope scope;
  Handle<WasmInstanceObject> instance;
  ErrorThrower thrower;
};

// TODO(7748): Use WASM_EXEC_TEST once interpreter and liftoff are supported
TEST(WasmBasicStruct) {
  WasmGCTester tester;
  uint32_t type_index =
      tester.DefineStruct({F(kWasmI32, true), F(kWasmI32, true)});
  ValueType kRefTypes[] = {ValueType(ValueType::kRef, type_index)};
  ValueType kOptRefType = ValueType(ValueType::kOptRef, type_index);
  FunctionSig sig_q_v(1, 0, kRefTypes);

  // Test struct.new and struct.get.

  tester.DefineFunction(
      "f", tester.sigs.i_v(), {},
      {WASM_STRUCT_GET(
           type_index, 0,
           WASM_STRUCT_NEW(type_index, WASM_I32V(42), WASM_I32V(64))),
       kExprEnd});

  // Test struct.new and struct.get.
  tester.DefineFunction(
      "g", tester.sigs.i_v(), {},
      {WASM_STRUCT_GET(
           type_index, 1,
           WASM_STRUCT_NEW(type_index, WASM_I32V(42), WASM_I32V(64))),
       kExprEnd});

  // Test struct.new, returning struct references to JS.
  tester.DefineFunction(
      "h", &sig_q_v, {},
      {WASM_STRUCT_NEW(type_index, WASM_I32V(42), WASM_I32V(64)), kExprEnd});

  // Test struct.set, struct refs types in locals.
  uint32_t j_local_index = 0;
  uint32_t j_field_index = 0;
  tester.DefineFunction(
      "j", tester.sigs.i_v(), {kOptRefType},
      {WASM_SET_LOCAL(j_local_index, WASM_STRUCT_NEW(type_index, WASM_I32V(42),
                                                     WASM_I32V(64))),
       WASM_STRUCT_SET(type_index, j_field_index, WASM_GET_LOCAL(j_local_index),
                       WASM_I32V(-99)),
       WASM_STRUCT_GET(type_index, j_field_index,
                       WASM_GET_LOCAL(j_local_index)),
       kExprEnd});

  // Test struct.set, ref.as_non_null,
  // struct refs types in globals and if-results.
  uint32_t k_global_index = tester.AddGlobal(
      kOptRefType, true, WasmInitExpr(WasmInitExpr::kRefNullConst));
  uint32_t k_field_index = 0;
  tester.DefineFunction(
      "k", tester.sigs.i_v(), {},
      {WASM_SET_GLOBAL(
           k_global_index,
           WASM_STRUCT_NEW(type_index, WASM_I32V(55), WASM_I32V(66))),
       WASM_STRUCT_GET(
           type_index, k_field_index,
           WASM_REF_AS_NON_NULL(WASM_IF_ELSE_R(kOptRefType, WASM_I32V(1),
                                               WASM_GET_GLOBAL(k_global_index),
                                               WASM_REF_NULL_GC(type_index)))),
       kExprEnd});

  // Test br_on_null 1.
  uint32_t l_local_index = 0;
  tester.DefineFunction(
      "l", tester.sigs.i_v(), {kOptRefType},
      {WASM_BLOCK_I(WASM_I32V(42),
                    // Branch will be taken.
                    // 42 left on stack outside the block (not 52).
                    WASM_BR_ON_NULL(0, WASM_GET_LOCAL(l_local_index)),
                    WASM_I32V(52), WASM_BR(0)),
       kExprEnd});

  // Test br_on_null 2.
  uint32_t m_field_index = 0;
  tester.DefineFunction(
      "m", tester.sigs.i_v(), {},
      {WASM_BLOCK_I(
           WASM_I32V(42),
           WASM_STRUCT_GET(
               type_index, m_field_index,
               // Branch will not be taken.
               // 52 left on stack outside the block (not 42).
               WASM_BR_ON_NULL(0, WASM_STRUCT_NEW(type_index, WASM_I32V(52),
                                                  WASM_I32V(62)))),
           WASM_BR(0)),
       kExprEnd});

  // Test ref.eq
  uint32_t n_local_index = 0;
  tester.DefineFunction(
      "n", tester.sigs.i_v(), {kOptRefType},
      {WASM_SET_LOCAL(n_local_index, WASM_STRUCT_NEW(type_index, WASM_I32V(55),
                                                     WASM_I32V(66))),
       WASM_I32_ADD(
           WASM_I32_SHL(WASM_REF_EQ(  // true
                            WASM_GET_LOCAL(n_local_index),
                            WASM_GET_LOCAL(n_local_index)),
                        WASM_I32V(0)),
           WASM_I32_ADD(
               WASM_I32_SHL(WASM_REF_EQ(  // false
                                WASM_GET_LOCAL(n_local_index),
                                WASM_STRUCT_NEW(type_index, WASM_I32V(55),
                                                WASM_I32V(66))),
                            WASM_I32V(1)),
               WASM_I32_ADD(WASM_I32_SHL(  // false
                                WASM_REF_EQ(WASM_GET_LOCAL(n_local_index),
                                            WASM_REF_NULL_GC(type_index)),
                                WASM_I32V(2)),
                            WASM_I32_SHL(WASM_REF_EQ(  // true
                                             WASM_REF_NULL_GC(type_index),
                                             WASM_REF_NULL_GC(type_index)),
                                         WASM_I32V(3))))),
       kExprEnd});
  // Result: 0b1001

  /************************* End of test definitions *************************/

  tester.CompileModule();

  tester.CheckResult("f", 42, {});
  tester.CheckResult("g", 64, {});

  CHECK(tester.GetJSResult("h", {}).ToHandleChecked()->IsWasmStruct());
  tester.CheckResult("j", -99, {});
  tester.CheckResult("k", 55, {});
  tester.CheckResult("l", 42, {});
  tester.CheckResult("m", 52, {});
  tester.CheckResult("n", 0b1001, {});
}

TEST(WasmLetInstruction) {
  WasmGCTester tester;
  uint32_t type_index =
      tester.DefineStruct({F(kWasmI32, true), F(kWasmI32, true)});

  uint32_t let_local_index = 0;
  uint32_t let_field_index = 0;
  tester.DefineFunction(
      "let_test_1", tester.sigs.i_v(), {},
      {WASM_LET_1_I(WASM_REF_TYPE(type_index),
                    WASM_STRUCT_NEW(type_index, WASM_I32V(42), WASM_I32V(52)),
                    WASM_STRUCT_GET(type_index, let_field_index,
                                    WASM_GET_LOCAL(let_local_index))),
       kExprEnd});

  uint32_t let_2_field_index = 0;
  tester.DefineFunction(
      "let_test_2", tester.sigs.i_v(), {},
      {WASM_LET_2_I(kLocalI32, WASM_I32_ADD(WASM_I32V(42), WASM_I32V(-32)),
                    WASM_REF_TYPE(type_index),
                    WASM_STRUCT_NEW(type_index, WASM_I32V(42), WASM_I32V(52)),
                    WASM_I32_MUL(WASM_STRUCT_GET(type_index, let_2_field_index,
                                                 WASM_GET_LOCAL(1)),
                                 WASM_GET_LOCAL(0))),
       kExprEnd});

  tester.DefineFunction(
      "let_test_locals", tester.sigs.i_i(), {kWasmI32},
      {WASM_SET_LOCAL(1, WASM_I32V(100)),
       WASM_LET_2_I(
           kLocalI32, WASM_I32V(1), kLocalI32, WASM_I32V(10),
           WASM_I32_SUB(WASM_I32_ADD(WASM_GET_LOCAL(0),     // 1st let-local
                                     WASM_GET_LOCAL(2)),    // Parameter
                        WASM_I32_ADD(WASM_GET_LOCAL(1),     // 2nd let-local
                                     WASM_GET_LOCAL(3)))),  // Function local
       kExprEnd});
  // Result: (1 + 1000) - (10 + 100) = 891

  uint32_t let_erase_local_index = 0;
  tester.DefineFunction("let_test_erase", tester.sigs.i_v(), {kWasmI32},
                        {WASM_SET_LOCAL(let_erase_local_index, WASM_I32V(0)),
                         WASM_LET_1_V(kLocalI32, WASM_I32V(1), WASM_NOP),
                         WASM_GET_LOCAL(let_erase_local_index), kExprEnd});
  // The result should be 0 and not 1, as local_get(0) refers to the original
  // local.

  tester.CompileModule();

  tester.CheckResult("let_test_1", 42, {});
  tester.CheckResult("let_test_2", 420, {});
  tester.CheckResult("let_test_locals", 891, {Smi::FromInt(1000)});
  tester.CheckResult("let_test_erase", 0, {});
}

TEST(WasmBasicArray) {
  WasmGCTester tester;
  uint32_t type_index = tester.DefineArray(wasm::kWasmI32, true);
  ValueType kRefTypes[] = {ValueType(ValueType::kRef, type_index)};
  FunctionSig sig_q_v(1, 0, kRefTypes);
  ValueType kOptRefType = ValueType(ValueType::kOptRef, type_index);

  // f: a = [12, 12, 12]; a[1] = 42; return a[arg0]
  uint32_t local_index = 1;
  tester.DefineFunction(
      "f", tester.sigs.i_i(), {kOptRefType},
      {WASM_SET_LOCAL(local_index,
                      WASM_ARRAY_NEW(type_index, WASM_I32V(12), WASM_I32V(3))),
       WASM_ARRAY_SET(type_index, WASM_GET_LOCAL(local_index), WASM_I32V(1),
                      WASM_I32V(42)),
       WASM_ARRAY_GET(type_index, WASM_GET_LOCAL(local_index),
                      WASM_GET_LOCAL(0)),
       kExprEnd});

  // Reads and returns an array's length.
  tester.DefineFunction(
      "g", tester.sigs.i_v(), {},
      {WASM_ARRAY_LEN(type_index,
                      WASM_ARRAY_NEW(type_index, WASM_I32V(0), WASM_I32V(42))),
       kExprEnd});

  // Create an array of length 2, initialized to [42, 42].
  tester.DefineFunction(
      "h", &sig_q_v, {},
      {WASM_ARRAY_NEW(type_index, WASM_I32V(42), WASM_I32V(2)), kExprEnd});

  tester.CompileModule();

  tester.CheckResult("f", 12, {Smi::FromInt(0)});
  tester.CheckResult("f", 42, {Smi::FromInt(1)});
  tester.CheckResult("f", 12, {Smi::FromInt(2)});
  tester.CheckHasThrown("f", {Smi::FromInt(3)});
  tester.CheckHasThrown("f", {Smi::FromInt(-1)});
  tester.CheckResult("g", 42, {});

  MaybeHandle<Object> h_result = tester.GetJSResult("h", {});
  CHECK(h_result.ToHandleChecked()->IsWasmArray());
#if OBJECT_PRINT
  h_result.ToHandleChecked()->Print();
#endif
}

}  // namespace test_gc
}  // namespace wasm
}  // namespace internal
}  // namespace v8
