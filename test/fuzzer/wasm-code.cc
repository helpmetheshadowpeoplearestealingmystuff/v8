// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "include/v8.h"
#include "src/isolate.h"
#include "src/wasm/encoder.h"
#include "src/wasm/wasm-interpreter.h"
#include "src/wasm/wasm-js.h"
#include "src/wasm/wasm-module.h"
#include "test/cctest/wasm/test-signatures.h"
#include "test/cctest/wasm/wasm-module-runner.h"
#include "test/fuzzer/fuzzer-support.h"

using namespace v8::internal::wasm;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  v8_fuzzer::FuzzerSupport* support = v8_fuzzer::FuzzerSupport::Get();
  v8::Isolate* isolate = support->GetIsolate();
  v8::internal::Isolate* i_isolate =
      reinterpret_cast<v8::internal::Isolate*>(isolate);

  // Clear any pending exceptions from a prior run.
  if (i_isolate->has_pending_exception()) {
    i_isolate->clear_pending_exception();
  }

  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(support->GetContext());
  v8::TryCatch try_catch(isolate);

  v8::base::AccountingAllocator allocator;
  v8::internal::Zone zone(&allocator);

  TestSignatures sigs;

  WasmModuleBuilder builder(&zone);

  uint16_t f1_index = builder.AddFunction();
  WasmFunctionBuilder* f = builder.FunctionAt(f1_index);
  f->SetSignature(sigs.i_iii());
  f->EmitCode(data, static_cast<uint32_t>(size));
  f->SetExported();
  f->SetName("main", 4);

  ZoneBuffer buffer(&zone);
  builder.WriteTo(buffer);

  v8::internal::WasmJs::SetupIsolateForWasm(i_isolate);

  v8::internal::HandleScope scope(i_isolate);

  ErrorThrower interpreter_thrower(i_isolate, "Interpreter");
  std::unique_ptr<const WasmModule> module(testing::DecodeWasmModuleForTesting(
      i_isolate, &zone, interpreter_thrower, buffer.begin(), buffer.end(),
      v8::internal::wasm::ModuleOrigin::kWasmOrigin));

  if (module == nullptr) {
    return 0;
  }
  int32_t result_interpreted;
  {
    WasmVal args[] = {WasmVal(1), WasmVal(2), WasmVal(3)};
    result_interpreted = testing::InterpretWasmModule(
        i_isolate, interpreter_thrower, module.get(), 0, args);
  }

  ErrorThrower compiler_thrower(i_isolate, "Compiler");
  v8::internal::Handle<v8::internal::JSObject> instance =
      testing::InstantiateModuleForTesting(i_isolate, compiler_thrower,
                                           module.get());

  if (!interpreter_thrower.error()) {
    CHECK(!instance.is_null());
  } else {
    return 0;
  }
  int32_t result_compiled;
  {
    v8::internal::Handle<v8::internal::Object> arguments[] = {
        v8::internal::handle(v8::internal::Smi::FromInt(1), i_isolate)};
    result_compiled = testing::CallWasmFunctionForTesting(
        i_isolate, instance, compiler_thrower, "main", arraysize(arguments),
        arguments, v8::internal::wasm::ModuleOrigin::kWasmOrigin);
  }
  if (result_interpreted == 0xdeadbeef) {
    CHECK(i_isolate->has_pending_exception());
    i_isolate->clear_pending_exception();
  } else {
    CHECK_EQ(result_interpreted, result_compiled);
  }
  return 0;
}
