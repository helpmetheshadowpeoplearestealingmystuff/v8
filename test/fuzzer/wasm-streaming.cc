// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "include/v8-isolate.h"
#include "src/api/api-inl.h"
#include "src/flags/flags.h"
#include "src/libplatform/default-platform.h"
#include "src/wasm/streaming-decoder.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-objects-inl.h"
#include "test/fuzzer/fuzzer-support.h"
#include "test/fuzzer/wasm-fuzzer-common.h"

namespace v8::internal::wasm {

// Some properties of the compilation result to check. Extend if needed.
struct CompilationResult {
  MOVE_ONLY_WITH_DEFAULT_CONSTRUCTORS(CompilationResult);

  bool failed = false;
  base::OwnedVector<const char> error_message;

  // If successful:
  uint32_t imported_functions = 0;
  uint32_t declared_functions = 0;

  static CompilationResult ForFailure(base::Vector<const char> error_message) {
    return {true, base::OwnedVector<const char>::Of(error_message)};
  }

  static CompilationResult ForSuccess(const WasmModule* module) {
    return {false,
            {},
            module->num_imported_functions,
            module->num_declared_functions};
  }
};

class TestResolver : public CompilationResultResolver {
 public:
  TestResolver(i::Isolate* isolate) : isolate_(isolate) {}

  void OnCompilationSucceeded(i::Handle<i::WasmModuleObject> module) override {
    done_ = true;
    native_module_ = module->shared_native_module();
  }

  void OnCompilationFailed(i::Handle<i::Object> error_reason) override {
    done_ = true;
    failed_ = true;
    Handle<String> str =
        Object::ToString(isolate_, error_reason).ToHandleChecked();
    error_message_.assign(str->ToCString().get());
  }

  bool done() const { return done_; }

  bool failed() const { return failed_; }

  const std::shared_ptr<NativeModule>& native_module() const {
    return native_module_;
  }

  const std::string& error_message() const { return error_message_; }

 private:
  i::Isolate* isolate_;
  bool done_ = false;
  bool failed_ = false;
  std::string error_message_;
  std::shared_ptr<NativeModule> native_module_;
};

CompilationResult CompileStreaming(v8_fuzzer::FuzzerSupport* support,
                                   WasmFeatures enabled_features,
                                   base::Vector<const uint8_t> data,
                                   uint8_t config) {
  v8::Isolate* isolate = support->GetIsolate();
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);

  CompilationResult result;
  std::weak_ptr<NativeModule> weak_native_module;
  {
    HandleScope handle_scope{i_isolate};
    auto resolver = std::make_shared<TestResolver>(i_isolate);
    Handle<Context> context = v8::Utils::OpenHandle(*support->GetContext());
    std::shared_ptr<StreamingDecoder> stream =
        GetWasmEngine()->StartStreamingCompilation(
            i_isolate, enabled_features, context, "wasm-streaming-fuzzer",
            resolver);

    if (data.size() > 0) {
      size_t split = config % data.size();
      stream->OnBytesReceived(data.SubVector(0, split));
      stream->OnBytesReceived(data.SubVectorFrom(split));
    }
    stream->Finish();

    // Wait for the promise to resolve or reject.
    while (!resolver->done()) {
      support->PumpMessageLoop(platform::MessageLoopBehavior::kWaitForWork);
      isolate->PerformMicrotaskCheckpoint();
    }

    if (resolver->failed()) {
      return CompilationResult::ForFailure(
          base::VectorOf(resolver->error_message()));
    }

    result = CompilationResult::ForSuccess(resolver->native_module()->module());
    weak_native_module = resolver->native_module();
  }
  // Collect garbage until the native module is collected. This ensures that we
  // recompile the module for sync compilation instead of taking it from the
  // cache.
  // If this turns out to be too slow, we could try to explicitly clear the
  // cache, but we have to be careful not to break other internal assumptions
  // then (because we have several identical modules / scripts).
  while (weak_native_module.lock()) {
    isolate->RequestGarbageCollectionForTesting(
        v8::Isolate::kFullGarbageCollection);
  }
  return result;
}

CompilationResult CompileSync(Isolate* isolate, WasmFeatures enabled_features,
                              base::Vector<const uint8_t> data) {
  ErrorThrower thrower{isolate, "wasm-streaming-fuzzer"};
  Handle<WasmModuleObject> module_object;
  CompilationResult result;
  if (!GetWasmEngine()
           ->SyncCompile(isolate, enabled_features, &thrower,
                         ModuleWireBytes{data})
           .ToHandle(&module_object)) {
    auto result =
        CompilationResult::ForFailure(base::CStrVector(thrower.error_msg()));
    thrower.Reset();
    return result;
  }
  return CompilationResult::ForSuccess(module_object->module());
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 1) return 0;

  v8_fuzzer::FuzzerSupport* support = v8_fuzzer::FuzzerSupport::Get();
  v8::Isolate* isolate = support->GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<v8::internal::Isolate*>(isolate);

  v8::Isolate::Scope isolate_scope(isolate);
  i::HandleScope handle_scope(i_isolate);
  v8::Context::Scope context_scope(support->GetContext());

  // We explicitly enable staged WebAssembly features here to increase fuzzer
  // coverage. For libfuzzer fuzzers it is not possible that the fuzzer enables
  // the flag by itself.
  fuzzer::OneTimeEnableStagedWasmFeatures(isolate);

  WasmFeatures enabled_features = i::wasm::WasmFeatures::FromIsolate(i_isolate);

  base::Vector<const uint8_t> data_vec{data, size - 1};
  uint8_t config = data[size - 1];

  CompilationResult streaming_result =
      CompileStreaming(support, enabled_features, data_vec, config);

  CompilationResult sync_result =
      CompileSync(i_isolate, enabled_features, data_vec);

  if (streaming_result.failed != sync_result.failed) {
    const char* error_msg = streaming_result.failed
                                ? streaming_result.error_message.begin()
                                : sync_result.error_message.begin();
    FATAL(
        "Streaming compilation did%s fail, sync compilation did%s. "
        "Error message: %s\n",
        streaming_result.failed ? "" : " not", sync_result.failed ? "" : " not",
        error_msg);
  }
  // TODO(12922): Enable this test later, after other bugs are flushed out.
  // if (strcmp(streaming_result.error_message.begin(),
  //            sync_result.error_message.begin()) != 0) {
  //   FATAL("Error messages differ: %s / %s\n",
  //         streaming_result.error_message.begin(),
  //         sync_result.error_message.begin());
  // }
  CHECK_EQ(streaming_result.imported_functions, sync_result.imported_functions);
  CHECK_EQ(streaming_result.declared_functions, sync_result.declared_functions);

  // We should not leave pending exceptions behind.
  DCHECK(!i_isolate->has_pending_exception());

  return 0;
}

}  // namespace v8::internal::wasm
