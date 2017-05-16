// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/inspector/task-runner.h"

#include "test/inspector/inspector-impl.h"

#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>  // NOLINT
#endif               // !defined(_WIN32) && !defined(_WIN64)

namespace {

const int kTaskRunnerIndex = 2;
const int kContextGroupIdIndex = 3;

void ReportUncaughtException(v8::Isolate* isolate,
                             const v8::TryCatch& try_catch) {
  CHECK(try_catch.HasCaught());
  v8::HandleScope handle_scope(isolate);
  std::string message = *v8::String::Utf8Value(try_catch.Message()->Get());
  fprintf(stderr, "Unhandle exception: %s\n", message.data());
}

v8::internal::Vector<uint16_t> ToVector(v8::Local<v8::String> str) {
  v8::internal::Vector<uint16_t> buffer =
      v8::internal::Vector<uint16_t>::New(str->Length());
  str->Write(buffer.start(), 0, str->Length());
  return buffer;
}

}  //  namespace

TaskRunner::TaskRunner(TaskRunner::SetupGlobalTasks setup_global_tasks,
                       bool catch_exceptions,
                       v8::base::Semaphore* ready_semaphore,
                       v8::StartupData* startup_data)
    : Thread(Options("Task Runner")),
      setup_global_tasks_(std::move(setup_global_tasks)),
      startup_data_(startup_data),
      catch_exceptions_(catch_exceptions),
      ready_semaphore_(ready_semaphore),
      isolate_(nullptr),
      process_queue_semaphore_(0),
      nested_loop_count_(0) {
  Start();
}

TaskRunner::~TaskRunner() { Join(); }

void TaskRunner::InitializeIsolate() {
  v8::Isolate::CreateParams params;
  params.array_buffer_allocator =
      v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  params.snapshot_blob = startup_data_;
  isolate_ = v8::Isolate::New(params);
  isolate_->SetMicrotasksPolicy(v8::MicrotasksPolicy::kScoped);
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);
  NewContextGroup(setup_global_tasks_);
  if (ready_semaphore_) ready_semaphore_->Signal();
}

v8::Local<v8::Context> TaskRunner::NewContextGroup(
    const TaskRunner::SetupGlobalTasks& setup_global_tasks) {
  v8::Local<v8::ObjectTemplate> global_template =
      v8::ObjectTemplate::New(isolate_);
  for (auto it = setup_global_tasks.begin(); it != setup_global_tasks.end();
       ++it) {
    (*it)->Run(isolate_, global_template);
  }
  v8::Local<v8::Context> context =
      v8::Context::New(isolate_, nullptr, global_template);
  context->SetAlignedPointerInEmbedderData(kTaskRunnerIndex, this);
  intptr_t context_group_id = ++last_context_group_id_;
  // Should be 2-byte aligned.
  context->SetAlignedPointerInEmbedderData(
      kContextGroupIdIndex, reinterpret_cast<void*>(context_group_id * 2));
  contexts_[context_group_id].Reset(isolate_, context);
  return context;
}

v8::Local<v8::Context> TaskRunner::GetContext(int context_group_id) {
  return contexts_[context_group_id].Get(isolate_);
}

int TaskRunner::GetContextGroupId(v8::Local<v8::Context> context) {
  return static_cast<int>(
      reinterpret_cast<intptr_t>(
          context->GetAlignedPointerFromEmbedderData(kContextGroupIdIndex)) /
      2);
}

void TaskRunner::Run() {
  InitializeIsolate();
  RunMessageLoop(false);
}

void TaskRunner::RunMessageLoop(bool only_protocol) {
  int loop_number = ++nested_loop_count_;
  while (nested_loop_count_ == loop_number && !is_terminated_.Value()) {
    TaskRunner::Task* task = GetNext(only_protocol);
    if (!task) return;
    v8::Isolate::Scope isolate_scope(isolate_);
    if (catch_exceptions_) {
      v8::TryCatch try_catch(isolate_);
      task->RunOnTaskRunner(this);
      delete task;
      if (try_catch.HasCaught()) {
        ReportUncaughtException(isolate_, try_catch);
        fflush(stdout);
        fflush(stderr);
        _exit(0);
      }
    } else {
      task->RunOnTaskRunner(this);
      delete task;
    }
  }
}

void TaskRunner::QuitMessageLoop() {
  DCHECK(nested_loop_count_ > 0);
  --nested_loop_count_;
}

void TaskRunner::Append(Task* task) {
  queue_.Enqueue(task);
  process_queue_semaphore_.Signal();
}

void TaskRunner::Terminate() {
  is_terminated_.Increment(1);
  process_queue_semaphore_.Signal();
}

void TaskRunner::RegisterModule(v8::internal::Vector<uint16_t> name,
                                v8::Local<v8::Module> module) {
  modules_[name] = v8::Global<v8::Module>(isolate_, module);
}

v8::MaybeLocal<v8::Module> TaskRunner::ModuleResolveCallback(
    v8::Local<v8::Context> context, v8::Local<v8::String> specifier,
    v8::Local<v8::Module> referrer) {
  std::string str = *v8::String::Utf8Value(specifier);
  TaskRunner* runner = TaskRunner::FromContext(context);
  return runner->modules_[ToVector(specifier)].Get(runner->isolate_);
}

TaskRunner::Task* TaskRunner::GetNext(bool only_protocol) {
  for (;;) {
    if (is_terminated_.Value()) return nullptr;
    if (only_protocol) {
      Task* task = nullptr;
      if (queue_.Dequeue(&task)) {
        if (task->is_inspector_task()) return task;
        deffered_queue_.Enqueue(task);
      }
    } else {
      Task* task = nullptr;
      if (deffered_queue_.Dequeue(&task)) return task;
      if (queue_.Dequeue(&task)) return task;
    }
    process_queue_semaphore_.Wait();
  }
  return nullptr;
}

TaskRunner* TaskRunner::FromContext(v8::Local<v8::Context> context) {
  return static_cast<TaskRunner*>(
      context->GetAlignedPointerFromEmbedderData(kTaskRunnerIndex));
}

AsyncTask::AsyncTask(const char* task_name,
                     v8_inspector::V8Inspector* inspector)
    : inspector_(task_name ? inspector : nullptr) {
  if (inspector_) {
    inspector_->asyncTaskScheduled(
        v8_inspector::StringView(reinterpret_cast<const uint8_t*>(task_name),
                                 strlen(task_name)),
        this, false);
  }
}

void AsyncTask::Run() {
  if (inspector_) inspector_->asyncTaskStarted(this);
  AsyncRun();
  if (inspector_) inspector_->asyncTaskFinished(this);
}

ExecuteStringTask::ExecuteStringTask(
    const v8::internal::Vector<uint16_t>& expression,
    v8::Local<v8::String> name, v8::Local<v8::Integer> line_offset,
    v8::Local<v8::Integer> column_offset, v8::Local<v8::Boolean> is_module,
    const char* task_name, v8_inspector::V8Inspector* inspector)
    : AsyncTask(task_name, inspector),
      expression_(expression),
      name_(ToVector(name)),
      line_offset_(line_offset.As<v8::Int32>()->Value()),
      column_offset_(column_offset.As<v8::Int32>()->Value()),
      is_module_(is_module->Value()) {}

ExecuteStringTask::ExecuteStringTask(
    const v8::internal::Vector<const char>& expression)
    : AsyncTask(nullptr, nullptr), expression_utf8_(expression) {}

void ExecuteStringTask::AsyncRun() {
  v8::MicrotasksScope microtasks_scope(isolate(),
                                       v8::MicrotasksScope::kRunMicrotasks);
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = default_context();
  v8::Context::Scope context_scope(context);

  v8::Local<v8::String> name =
      v8::String::NewFromTwoByte(isolate(), name_.start(),
                                 v8::NewStringType::kNormal, name_.length())
          .ToLocalChecked();
  v8::Local<v8::Integer> line_offset =
      v8::Integer::New(isolate(), line_offset_);
  v8::Local<v8::Integer> column_offset =
      v8::Integer::New(isolate(), column_offset_);

  v8::ScriptOrigin origin(
      name, line_offset, column_offset,
      /* resource_is_shared_cross_origin */ v8::Local<v8::Boolean>(),
      /* script_id */ v8::Local<v8::Integer>(),
      /* source_map_url */ v8::Local<v8::Value>(),
      /* resource_is_opaque */ v8::Local<v8::Boolean>(),
      /* is_wasm */ v8::Local<v8::Boolean>(),
      v8::Boolean::New(isolate(), is_module_));
  v8::Local<v8::String> source;
  if (expression_.length()) {
    source = v8::String::NewFromTwoByte(isolate(), expression_.start(),
                                        v8::NewStringType::kNormal,
                                        expression_.length())
                 .ToLocalChecked();
  } else {
    source = v8::String::NewFromUtf8(isolate(), expression_utf8_.start(),
                                     v8::NewStringType::kNormal,
                                     expression_utf8_.length())
                 .ToLocalChecked();
  }

  v8::ScriptCompiler::Source scriptSource(source, origin);
  if (!is_module_) {
    v8::Local<v8::Script> script;
    if (!v8::ScriptCompiler::Compile(context, &scriptSource).ToLocal(&script))
      return;
    v8::MaybeLocal<v8::Value> result;
    result = script->Run(context);
  } else {
    v8::Local<v8::Module> module;
    if (!v8::ScriptCompiler::CompileModule(isolate(), &scriptSource)
             .ToLocal(&module)) {
      return;
    }
    if (!module->Instantiate(context, &TaskRunner::ModuleResolveCallback))
      return;
    v8::Local<v8::Value> result;
    if (!module->Evaluate(context).ToLocal(&result)) return;
    TaskRunner* runner = TaskRunner::FromContext(context);
    runner->RegisterModule(name_, module);
  }
}
