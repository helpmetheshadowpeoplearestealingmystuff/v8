// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "include/v8-metrics.h"
#include "src/api/api-inl.h"
#include "src/wasm/wasm-module-builder.h"
#include "test/cctest/cctest.h"
#include "test/common/wasm/flag-utils.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"
#include "test/common/wasm/wasm-module-runner.h"

namespace v8 {
namespace internal {
namespace wasm {

namespace {

class MockPlatform final : public TestPlatform {
 public:
  MockPlatform() : task_runner_(std::make_shared<MockTaskRunner>()) {
    // Now that it's completely constructed, make this the current platform.
    i::V8::SetPlatformForTesting(this);
  }

  ~MockPlatform() override {
    for (auto* job_handle : job_handles_) job_handle->ResetPlatform();
  }

  std::unique_ptr<v8::JobHandle> PostJob(
      v8::TaskPriority priority,
      std::unique_ptr<v8::JobTask> job_task) override {
    auto orig_job_handle = TestPlatform::PostJob(priority, std::move(job_task));
    auto job_handle =
        std::make_unique<MockJobHandle>(std::move(orig_job_handle), this);
    job_handles_.insert(job_handle.get());
    return job_handle;
  }

  std::shared_ptr<TaskRunner> GetForegroundTaskRunner(
      v8::Isolate* isolate) override {
    return task_runner_;
  }

  void CallOnWorkerThread(std::unique_ptr<v8::Task> task) override {
    task_runner_->PostTask(std::move(task));
  }

  bool IdleTasksEnabled(v8::Isolate* isolate) override { return false; }

  void ExecuteTasks() {
    for (auto* job_handle : job_handles_) {
      if (job_handle->IsRunning()) job_handle->Join();
    }
    task_runner_->ExecuteTasks();
  }

 private:
  class MockTaskRunner final : public TaskRunner {
   public:
    void PostTask(std::unique_ptr<v8::Task> task) override {
      base::MutexGuard lock_scope(&tasks_lock_);
      tasks_.push(std::move(task));
    }

    void PostDelayedTask(std::unique_ptr<Task> task,
                         double delay_in_seconds) override {
      base::MutexGuard lock_scope(&tasks_lock_);
      tasks_.push(std::move(task));
    }

    void PostIdleTask(std::unique_ptr<IdleTask> task) override {
      UNREACHABLE();
    }

    bool IdleTasksEnabled() override { return false; }

    void ExecuteTasks() {
      std::queue<std::unique_ptr<v8::Task>> tasks;
      {
        base::MutexGuard lock_scope(&tasks_lock_);
        tasks.swap(tasks_);
      }
      while (!tasks.empty()) {
        std::unique_ptr<Task> task = std::move(tasks.front());
        tasks.pop();
        task->Run();
      }
    }

   private:
    base::Mutex tasks_lock_;
    // We do not execute tasks concurrently, so we only need one list of tasks.
    std::queue<std::unique_ptr<v8::Task>> tasks_;
  };

  class MockJobHandle : public JobHandle {
   public:
    explicit MockJobHandle(std::unique_ptr<JobHandle> orig_handle,
                           MockPlatform* platform)
        : orig_handle_(std::move(orig_handle)), platform_(platform) {}

    ~MockJobHandle() {
      if (platform_) platform_->job_handles_.erase(this);
    }

    void ResetPlatform() { platform_ = nullptr; }

    void NotifyConcurrencyIncrease() override {
      orig_handle_->NotifyConcurrencyIncrease();
    }
    void Join() override { orig_handle_->Join(); }
    void Cancel() override { orig_handle_->Cancel(); }
    bool IsRunning() override { return orig_handle_->IsRunning(); }
    bool IsCompleted() override { return orig_handle_->IsCompleted(); }

   private:
    std::unique_ptr<JobHandle> orig_handle_;
    MockPlatform* platform_;
  };

  std::shared_ptr<MockTaskRunner> task_runner_;
  std::unordered_set<MockJobHandle*> job_handles_;
};

enum class CompilationStatus {
  kPending,
  kFinished,
  kFailed,
};

class TestInstantiateResolver : public InstantiationResultResolver {
 public:
  TestInstantiateResolver(CompilationStatus* status, std::string* error_message)
      : status_(status), error_message_(error_message) {}

  void OnInstantiationSucceeded(
      i::Handle<i::WasmInstanceObject> instance) override {
    *status_ = CompilationStatus::kFinished;
  }

  void OnInstantiationFailed(i::Handle<i::Object> error_reason) override {
    *status_ = CompilationStatus::kFailed;
    Handle<String> str =
        Object::ToString(CcTest::i_isolate(), error_reason).ToHandleChecked();
    error_message_->assign(str->ToCString().get());
  }

 private:
  CompilationStatus* const status_;
  std::string* const error_message_;
};

class TestCompileResolver : public CompilationResultResolver {
 public:
  TestCompileResolver(CompilationStatus* status, std::string* error_message,
                      Isolate* isolate,
                      std::shared_ptr<NativeModule>* native_module)
      : status_(status),
        error_message_(error_message),
        isolate_(isolate),
        native_module_(native_module) {}

  void OnCompilationSucceeded(i::Handle<i::WasmModuleObject> module) override {
    if (!module.is_null()) {
      *native_module_ = module->shared_native_module();
      isolate_->wasm_engine()->AsyncInstantiate(
          isolate_,
          std::make_unique<TestInstantiateResolver>(status_, error_message_),
          module, MaybeHandle<JSReceiver>());
    }
  }

  void OnCompilationFailed(i::Handle<i::Object> error_reason) override {
    *status_ = CompilationStatus::kFailed;
    Handle<String> str =
        Object::ToString(CcTest::i_isolate(), error_reason).ToHandleChecked();
    error_message_->assign(str->ToCString().get());
  }

 private:
  CompilationStatus* const status_;
  std::string* const error_message_;
  Isolate* isolate_;
  std::shared_ptr<NativeModule>* const native_module_;
};

}  // namespace

#define COMPILE_TEST(name)                                                  \
  void RunCompile_##name();                                                 \
  TEST(Sync##name) {                                                        \
    i::FlagScope<bool> sync_scope(&i::FLAG_wasm_async_compilation, false);  \
    RunCompile_##name();                                                    \
  }                                                                         \
                                                                            \
  TEST(Async##name) { RunCompile_##name(); }                                \
                                                                            \
  TEST(Streaming##name) {                                                   \
    i::FlagScope<bool> streaming_scope(&i::FLAG_wasm_test_streaming, true); \
    RunCompile_##name();                                                    \
  }                                                                         \
  void RunCompile_##name()

class MetricsRecorder : public v8::metrics::Recorder {
 public:
  std::vector<v8::metrics::WasmModuleDecoded> module_decoded_;
  std::vector<v8::metrics::WasmModuleCompiled> module_compiled_;
  std::vector<v8::metrics::WasmModuleInstantiated> module_instantiated_;
  std::vector<v8::metrics::WasmModuleTieredUp> module_tiered_up_;

  void AddMainThreadEvent(const v8::metrics::WasmModuleDecoded& event,
                          v8::metrics::Recorder::ContextId id) override {
    CHECK(!id.IsEmpty());
    module_decoded_.emplace_back(event);
  }
  void AddMainThreadEvent(const v8::metrics::WasmModuleCompiled& event,
                          v8::metrics::Recorder::ContextId id) override {
    CHECK(!id.IsEmpty());
    module_compiled_.emplace_back(event);
  }
  void AddMainThreadEvent(const v8::metrics::WasmModuleInstantiated& event,
                          v8::metrics::Recorder::ContextId id) override {
    CHECK(!id.IsEmpty());
    module_instantiated_.emplace_back(event);
  }
  void AddMainThreadEvent(const v8::metrics::WasmModuleTieredUp& event,
                          v8::metrics::Recorder::ContextId id) override {
    CHECK(!id.IsEmpty());
    module_tiered_up_.emplace_back(event);
  }
};

COMPILE_TEST(TestEventMetrics) {
  MockPlatform platform;
  Isolate* isolate = CcTest::InitIsolateOnce();
  CHECK_EQ(V8::GetCurrentPlatform(), &platform);
  HandleScope scope(isolate);
  testing::SetupIsolateForWasmModule(isolate);
  std::shared_ptr<MetricsRecorder> recorder =
      std::make_shared<MetricsRecorder>();
  CcTest::isolate()->SetMetricsRecorder(recorder);

  TestSignatures sigs;
  v8::internal::AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);

  WasmModuleBuilder* builder = zone.New<WasmModuleBuilder>(&zone);
  WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());
  f->builder()->AddExport(CStrVector("main"), f);
  byte code[] = {WASM_I32V_2(0)};
  f->EmitCode(code, sizeof(code));
  f->Emit(kExprEnd);
  ZoneBuffer buffer(&zone);
  builder->WriteTo(&buffer);

  auto enabled_features = WasmFeatures::FromIsolate(isolate);
  CompilationStatus status = CompilationStatus::kPending;
  std::string error_message;
  std::shared_ptr<NativeModule> native_module;
  isolate->wasm_engine()->AsyncCompile(
      isolate, enabled_features,
      std::make_shared<TestCompileResolver>(&status, &error_message, isolate,
                                            &native_module),
      ModuleWireBytes(buffer.begin(), buffer.end()), true,
      "CompileAndInstantiateWasmModuleForTesting");

  // Finish compilation tasks.
  while (status == CompilationStatus::kPending) {
    platform.ExecuteTasks();
  }
  platform.ExecuteTasks();  // Complete pending tasks beyond compilation.
  CHECK_EQ(CompilationStatus::kFinished, status);

  CHECK_EQ(1, recorder->module_decoded_.size());
  CHECK(recorder->module_decoded_.back().success);
  CHECK_EQ(i::FLAG_wasm_async_compilation,
           recorder->module_decoded_.back().async);
  CHECK_EQ(i::FLAG_wasm_test_streaming,
           recorder->module_decoded_.back().streamed);
  CHECK_EQ(buffer.size(),
           recorder->module_decoded_.back().module_size_in_bytes);
  CHECK_EQ(1, recorder->module_decoded_.back().function_count);
  CHECK_LE(0, recorder->module_decoded_.back().wall_clock_time_in_us);

  CHECK_EQ(1, recorder->module_compiled_.size());
  CHECK(recorder->module_compiled_.back().success);
  CHECK_EQ(i::FLAG_wasm_async_compilation,
           recorder->module_compiled_.back().async);
  CHECK_EQ(i::FLAG_wasm_test_streaming,
           recorder->module_compiled_.back().streamed);
  CHECK(!recorder->module_compiled_.back().cached);
  CHECK(!recorder->module_compiled_.back().deserialized);
  CHECK(!recorder->module_compiled_.back().lazy);
  CHECK_LT(0, recorder->module_compiled_.back().code_size_in_bytes);
  CHECK_EQ(native_module->liftoff_code_size(),
           recorder->module_compiled_.back().code_size_in_bytes);
  CHECK_GE(native_module->generated_code_size(),
           recorder->module_compiled_.back().code_size_in_bytes);
  CHECK_EQ(0, recorder->module_compiled_.back().liftoff_bailout_count);
  CHECK_LE(0, recorder->module_compiled_.back().wall_clock_time_in_us);

  CHECK_EQ(1, recorder->module_instantiated_.size());
  CHECK(recorder->module_instantiated_.back().success);
  // We currently don't support true async instantiation.
  CHECK(!recorder->module_instantiated_.back().async);
  CHECK_EQ(0, recorder->module_instantiated_.back().imported_function_count);
  CHECK_LE(0, recorder->module_instantiated_.back().wall_clock_time_in_us);

  CHECK_EQ(1, recorder->module_tiered_up_.size());
  CHECK(!recorder->module_tiered_up_.back().lazy);
  CHECK_LT(0, recorder->module_tiered_up_.back().code_size_in_bytes);
  CHECK_EQ(native_module->turbofan_code_size(),
           recorder->module_tiered_up_.back().code_size_in_bytes);
  CHECK_GE(native_module->generated_code_size(),
           recorder->module_tiered_up_.back().code_size_in_bytes);
  CHECK_GE(native_module->committed_code_space(),
           recorder->module_tiered_up_.back().code_size_in_bytes);
  CHECK_LE(0, recorder->module_tiered_up_.back().wall_clock_time_in_us);
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
