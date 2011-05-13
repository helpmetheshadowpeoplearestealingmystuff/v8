// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <stdlib.h>

#include "v8.h"

#include "ast.h"
#include "bootstrapper.h"
#include "codegen.h"
#include "compilation-cache.h"
#include "debug.h"
#include "deoptimizer.h"
#include "heap-profiler.h"
#include "hydrogen.h"
#include "isolate.h"
#include "lithium-allocator.h"
#include "log.h"
#include "messages.h"
#include "regexp-stack.h"
#include "runtime-profiler.h"
#include "scanner.h"
#include "scopeinfo.h"
#include "serialize.h"
#include "simulator.h"
#include "spaces.h"
#include "stub-cache.h"
#include "version.h"
#include "vm-state-inl.h"


namespace v8 {
namespace internal {

Atomic32 ThreadId::highest_thread_id_ = 0;

int ThreadId::AllocateThreadId() {
  int new_id = NoBarrier_AtomicIncrement(&highest_thread_id_, 1);
  return new_id;
}


int ThreadId::GetCurrentThreadId() {
  int thread_id = Thread::GetThreadLocalInt(Isolate::thread_id_key_);
  if (thread_id == 0) {
    thread_id = AllocateThreadId();
    Thread::SetThreadLocalInt(Isolate::thread_id_key_, thread_id);
  }
  return thread_id;
}


ThreadLocalTop::ThreadLocalTop() {
  InitializeInternal();
}


void ThreadLocalTop::InitializeInternal() {
  c_entry_fp_ = 0;
  handler_ = 0;
#ifdef USE_SIMULATOR
  simulator_ = NULL;
#endif
#ifdef ENABLE_LOGGING_AND_PROFILING
  js_entry_sp_ = NULL;
  external_callback_ = NULL;
#endif
#ifdef ENABLE_VMSTATE_TRACKING
  current_vm_state_ = EXTERNAL;
#endif
  try_catch_handler_address_ = NULL;
  context_ = NULL;
  thread_id_ = ThreadId::Invalid();
  external_caught_exception_ = false;
  failed_access_check_callback_ = NULL;
  save_context_ = NULL;
  catcher_ = NULL;
}


void ThreadLocalTop::Initialize() {
  InitializeInternal();
#ifdef USE_SIMULATOR
#ifdef V8_TARGET_ARCH_ARM
  simulator_ = Simulator::current(isolate_);
#elif V8_TARGET_ARCH_MIPS
  simulator_ = Simulator::current(isolate_);
#endif
#endif
  thread_id_ = ThreadId::Current();
}


v8::TryCatch* ThreadLocalTop::TryCatchHandler() {
  return TRY_CATCH_FROM_ADDRESS(try_catch_handler_address());
}


// Create a dummy thread that will wait forever on a semaphore. The only
// purpose for this thread is to have some stack area to save essential data
// into for use by a stacks only core dump (aka minidump).
class PreallocatedMemoryThread: public Thread {
 public:
  char* data() {
    if (data_ready_semaphore_ != NULL) {
      // Initial access is guarded until the data has been published.
      data_ready_semaphore_->Wait();
      delete data_ready_semaphore_;
      data_ready_semaphore_ = NULL;
    }
    return data_;
  }

  unsigned length() {
    if (data_ready_semaphore_ != NULL) {
      // Initial access is guarded until the data has been published.
      data_ready_semaphore_->Wait();
      delete data_ready_semaphore_;
      data_ready_semaphore_ = NULL;
    }
    return length_;
  }

  // Stop the PreallocatedMemoryThread and release its resources.
  void StopThread() {
    keep_running_ = false;
    wait_for_ever_semaphore_->Signal();

    // Wait for the thread to terminate.
    Join();

    if (data_ready_semaphore_ != NULL) {
      delete data_ready_semaphore_;
      data_ready_semaphore_ = NULL;
    }

    delete wait_for_ever_semaphore_;
    wait_for_ever_semaphore_ = NULL;
  }

 protected:
  // When the thread starts running it will allocate a fixed number of bytes
  // on the stack and publish the location of this memory for others to use.
  void Run() {
    EmbeddedVector<char, 15 * 1024> local_buffer;

    // Initialize the buffer with a known good value.
    OS::StrNCpy(local_buffer, "Trace data was not generated.\n",
                local_buffer.length());

    // Publish the local buffer and signal its availability.
    data_ = local_buffer.start();
    length_ = local_buffer.length();
    data_ready_semaphore_->Signal();

    while (keep_running_) {
      // This thread will wait here until the end of time.
      wait_for_ever_semaphore_->Wait();
    }

    // Make sure we access the buffer after the wait to remove all possibility
    // of it being optimized away.
    OS::StrNCpy(local_buffer, "PreallocatedMemoryThread shutting down.\n",
                local_buffer.length());
  }


 private:
  explicit PreallocatedMemoryThread(Isolate* isolate)
      : Thread(isolate, "v8:PreallocMem"),
        keep_running_(true),
        wait_for_ever_semaphore_(OS::CreateSemaphore(0)),
        data_ready_semaphore_(OS::CreateSemaphore(0)),
        data_(NULL),
        length_(0) {
  }

  // Used to make sure that the thread keeps looping even for spurious wakeups.
  bool keep_running_;

  // This semaphore is used by the PreallocatedMemoryThread to wait for ever.
  Semaphore* wait_for_ever_semaphore_;
  // Semaphore to signal that the data has been initialized.
  Semaphore* data_ready_semaphore_;

  // Location and size of the preallocated memory block.
  char* data_;
  unsigned length_;

  friend class Isolate;

  DISALLOW_COPY_AND_ASSIGN(PreallocatedMemoryThread);
};


void Isolate::PreallocatedMemoryThreadStart() {
  if (preallocated_memory_thread_ != NULL) return;
  preallocated_memory_thread_ = new PreallocatedMemoryThread(this);
  preallocated_memory_thread_->Start();
}


void Isolate::PreallocatedMemoryThreadStop() {
  if (preallocated_memory_thread_ == NULL) return;
  preallocated_memory_thread_->StopThread();
  // Done with the thread entirely.
  delete preallocated_memory_thread_;
  preallocated_memory_thread_ = NULL;
}


void Isolate::PreallocatedStorageInit(size_t size) {
  ASSERT(free_list_.next_ == &free_list_);
  ASSERT(free_list_.previous_ == &free_list_);
  PreallocatedStorage* free_chunk =
      reinterpret_cast<PreallocatedStorage*>(new char[size]);
  free_list_.next_ = free_list_.previous_ = free_chunk;
  free_chunk->next_ = free_chunk->previous_ = &free_list_;
  free_chunk->size_ = size - sizeof(PreallocatedStorage);
  preallocated_storage_preallocated_ = true;
}


void* Isolate::PreallocatedStorageNew(size_t size) {
  if (!preallocated_storage_preallocated_) {
    return FreeStoreAllocationPolicy::New(size);
  }
  ASSERT(free_list_.next_ != &free_list_);
  ASSERT(free_list_.previous_ != &free_list_);

  size = (size + kPointerSize - 1) & ~(kPointerSize - 1);
  // Search for exact fit.
  for (PreallocatedStorage* storage = free_list_.next_;
       storage != &free_list_;
       storage = storage->next_) {
    if (storage->size_ == size) {
      storage->Unlink();
      storage->LinkTo(&in_use_list_);
      return reinterpret_cast<void*>(storage + 1);
    }
  }
  // Search for first fit.
  for (PreallocatedStorage* storage = free_list_.next_;
       storage != &free_list_;
       storage = storage->next_) {
    if (storage->size_ >= size + sizeof(PreallocatedStorage)) {
      storage->Unlink();
      storage->LinkTo(&in_use_list_);
      PreallocatedStorage* left_over =
          reinterpret_cast<PreallocatedStorage*>(
              reinterpret_cast<char*>(storage + 1) + size);
      left_over->size_ = storage->size_ - size - sizeof(PreallocatedStorage);
      ASSERT(size + left_over->size_ + sizeof(PreallocatedStorage) ==
             storage->size_);
      storage->size_ = size;
      left_over->LinkTo(&free_list_);
      return reinterpret_cast<void*>(storage + 1);
    }
  }
  // Allocation failure.
  ASSERT(false);
  return NULL;
}


// We don't attempt to coalesce.
void Isolate::PreallocatedStorageDelete(void* p) {
  if (p == NULL) {
    return;
  }
  if (!preallocated_storage_preallocated_) {
    FreeStoreAllocationPolicy::Delete(p);
    return;
  }
  PreallocatedStorage* storage = reinterpret_cast<PreallocatedStorage*>(p) - 1;
  ASSERT(storage->next_->previous_ == storage);
  ASSERT(storage->previous_->next_ == storage);
  storage->Unlink();
  storage->LinkTo(&free_list_);
}


Isolate* Isolate::default_isolate_ = NULL;
Thread::LocalStorageKey Isolate::isolate_key_;
Thread::LocalStorageKey Isolate::thread_id_key_;
Thread::LocalStorageKey Isolate::per_isolate_thread_data_key_;
Mutex* Isolate::process_wide_mutex_ = OS::CreateMutex();
Isolate::ThreadDataTable* Isolate::thread_data_table_ = NULL;


class IsolateInitializer {
 public:
  IsolateInitializer() {
    Isolate::EnsureDefaultIsolate();
  }
};

static IsolateInitializer* EnsureDefaultIsolateAllocated() {
  // TODO(isolates): Use the system threading API to do this once?
  static IsolateInitializer static_initializer;
  return &static_initializer;
}

// This variable only needed to trigger static intialization.
static IsolateInitializer* static_initializer = EnsureDefaultIsolateAllocated();





Isolate::PerIsolateThreadData* Isolate::AllocatePerIsolateThreadData(
    ThreadId thread_id) {
  ASSERT(!thread_id.Equals(ThreadId::Invalid()));
  PerIsolateThreadData* per_thread = new PerIsolateThreadData(this, thread_id);
  {
    ScopedLock lock(process_wide_mutex_);
    ASSERT(thread_data_table_->Lookup(this, thread_id) == NULL);
    thread_data_table_->Insert(per_thread);
    ASSERT(thread_data_table_->Lookup(this, thread_id) == per_thread);
  }
  return per_thread;
}


Isolate::PerIsolateThreadData*
    Isolate::FindOrAllocatePerThreadDataForThisThread() {
  ThreadId thread_id = ThreadId::Current();
  PerIsolateThreadData* per_thread = NULL;
  {
    ScopedLock lock(process_wide_mutex_);
    per_thread = thread_data_table_->Lookup(this, thread_id);
    if (per_thread == NULL) {
      per_thread = AllocatePerIsolateThreadData(thread_id);
    }
  }
  return per_thread;
}


Isolate::PerIsolateThreadData* Isolate::FindPerThreadDataForThisThread() {
  ThreadId thread_id = ThreadId::Current();
  PerIsolateThreadData* per_thread = NULL;
  {
    ScopedLock lock(process_wide_mutex_);
    per_thread = thread_data_table_->Lookup(this, thread_id);
  }
  return per_thread;
}


void Isolate::EnsureDefaultIsolate() {
  ScopedLock lock(process_wide_mutex_);
  if (default_isolate_ == NULL) {
    isolate_key_ = Thread::CreateThreadLocalKey();
    thread_id_key_ = Thread::CreateThreadLocalKey();
    per_isolate_thread_data_key_ = Thread::CreateThreadLocalKey();
    thread_data_table_ = new Isolate::ThreadDataTable();
    default_isolate_ = new Isolate();
  }
  // Can't use SetIsolateThreadLocals(default_isolate_, NULL) here
  // becase a non-null thread data may be already set.
  if (Thread::GetThreadLocal(isolate_key_) == NULL) {
    Thread::SetThreadLocal(isolate_key_, default_isolate_);
  }
  CHECK(default_isolate_->PreInit());
}


#ifdef ENABLE_DEBUGGER_SUPPORT
Debugger* Isolate::GetDefaultIsolateDebugger() {
  EnsureDefaultIsolate();
  return default_isolate_->debugger();
}
#endif


StackGuard* Isolate::GetDefaultIsolateStackGuard() {
  EnsureDefaultIsolate();
  return default_isolate_->stack_guard();
}


void Isolate::EnterDefaultIsolate() {
  EnsureDefaultIsolate();
  ASSERT(default_isolate_ != NULL);

  PerIsolateThreadData* data = CurrentPerIsolateThreadData();
  // If not yet in default isolate - enter it.
  if (data == NULL || data->isolate() != default_isolate_) {
    default_isolate_->Enter();
  }
}


Isolate* Isolate::GetDefaultIsolateForLocking() {
  EnsureDefaultIsolate();
  return default_isolate_;
}


Address Isolate::get_address_from_id(Isolate::AddressId id) {
  return isolate_addresses_[id];
}


char* Isolate::Iterate(ObjectVisitor* v, char* thread_storage) {
  ThreadLocalTop* thread = reinterpret_cast<ThreadLocalTop*>(thread_storage);
  Iterate(v, thread);
  return thread_storage + sizeof(ThreadLocalTop);
}


void Isolate::IterateThread(ThreadVisitor* v) {
  v->VisitThread(this, thread_local_top());
}


void Isolate::IterateThread(ThreadVisitor* v, char* t) {
  ThreadLocalTop* thread = reinterpret_cast<ThreadLocalTop*>(t);
  v->VisitThread(this, thread);
}


void Isolate::Iterate(ObjectVisitor* v, ThreadLocalTop* thread) {
  // Visit the roots from the top for a given thread.
  Object* pending;
  // The pending exception can sometimes be a failure.  We can't show
  // that to the GC, which only understands objects.
  if (thread->pending_exception_->ToObject(&pending)) {
    v->VisitPointer(&pending);
    thread->pending_exception_ = pending;  // In case GC updated it.
  }
  v->VisitPointer(&(thread->pending_message_obj_));
  v->VisitPointer(BitCast<Object**>(&(thread->pending_message_script_)));
  v->VisitPointer(BitCast<Object**>(&(thread->context_)));
  Object* scheduled;
  if (thread->scheduled_exception_->ToObject(&scheduled)) {
    v->VisitPointer(&scheduled);
    thread->scheduled_exception_ = scheduled;
  }

  for (v8::TryCatch* block = thread->TryCatchHandler();
       block != NULL;
       block = TRY_CATCH_FROM_ADDRESS(block->next_)) {
    v->VisitPointer(BitCast<Object**>(&(block->exception_)));
    v->VisitPointer(BitCast<Object**>(&(block->message_)));
  }

  // Iterate over pointers on native execution stack.
  for (StackFrameIterator it(this, thread); !it.done(); it.Advance()) {
    it.frame()->Iterate(v);
  }
}


void Isolate::Iterate(ObjectVisitor* v) {
  ThreadLocalTop* current_t = thread_local_top();
  Iterate(v, current_t);
}


void Isolate::RegisterTryCatchHandler(v8::TryCatch* that) {
  // The ARM simulator has a separate JS stack.  We therefore register
  // the C++ try catch handler with the simulator and get back an
  // address that can be used for comparisons with addresses into the
  // JS stack.  When running without the simulator, the address
  // returned will be the address of the C++ try catch handler itself.
  Address address = reinterpret_cast<Address>(
      SimulatorStack::RegisterCTryCatch(reinterpret_cast<uintptr_t>(that)));
  thread_local_top()->set_try_catch_handler_address(address);
}


void Isolate::UnregisterTryCatchHandler(v8::TryCatch* that) {
  ASSERT(thread_local_top()->TryCatchHandler() == that);
  thread_local_top()->set_try_catch_handler_address(
      reinterpret_cast<Address>(that->next_));
  thread_local_top()->catcher_ = NULL;
  SimulatorStack::UnregisterCTryCatch();
}


Handle<String> Isolate::StackTraceString() {
  if (stack_trace_nesting_level_ == 0) {
    stack_trace_nesting_level_++;
    HeapStringAllocator allocator;
    StringStream::ClearMentionedObjectCache();
    StringStream accumulator(&allocator);
    incomplete_message_ = &accumulator;
    PrintStack(&accumulator);
    Handle<String> stack_trace = accumulator.ToString();
    incomplete_message_ = NULL;
    stack_trace_nesting_level_ = 0;
    return stack_trace;
  } else if (stack_trace_nesting_level_ == 1) {
    stack_trace_nesting_level_++;
    OS::PrintError(
      "\n\nAttempt to print stack while printing stack (double fault)\n");
    OS::PrintError(
      "If you are lucky you may find a partial stack dump on stdout.\n\n");
    incomplete_message_->OutputToStdOut();
    return factory()->empty_symbol();
  } else {
    OS::Abort();
    // Unreachable
    return factory()->empty_symbol();
  }
}


Handle<JSArray> Isolate::CaptureCurrentStackTrace(
    int frame_limit, StackTrace::StackTraceOptions options) {
  // Ensure no negative values.
  int limit = Max(frame_limit, 0);
  Handle<JSArray> stack_trace = factory()->NewJSArray(frame_limit);

  Handle<String> column_key = factory()->LookupAsciiSymbol("column");
  Handle<String> line_key = factory()->LookupAsciiSymbol("lineNumber");
  Handle<String> script_key = factory()->LookupAsciiSymbol("scriptName");
  Handle<String> name_or_source_url_key =
      factory()->LookupAsciiSymbol("nameOrSourceURL");
  Handle<String> script_name_or_source_url_key =
      factory()->LookupAsciiSymbol("scriptNameOrSourceURL");
  Handle<String> function_key = factory()->LookupAsciiSymbol("functionName");
  Handle<String> eval_key = factory()->LookupAsciiSymbol("isEval");
  Handle<String> constructor_key =
      factory()->LookupAsciiSymbol("isConstructor");

  StackTraceFrameIterator it(this);
  int frames_seen = 0;
  while (!it.done() && (frames_seen < limit)) {
    JavaScriptFrame* frame = it.frame();
    // Set initial size to the maximum inlining level + 1 for the outermost
    // function.
    List<FrameSummary> frames(Compiler::kMaxInliningLevels + 1);
    frame->Summarize(&frames);
    for (int i = frames.length() - 1; i >= 0 && frames_seen < limit; i--) {
      // Create a JSObject to hold the information for the StackFrame.
      Handle<JSObject> stackFrame = factory()->NewJSObject(object_function());

      Handle<JSFunction> fun = frames[i].function();
      Handle<Script> script(Script::cast(fun->shared()->script()));

      if (options & StackTrace::kLineNumber) {
        int script_line_offset = script->line_offset()->value();
        int position = frames[i].code()->SourcePosition(frames[i].pc());
        int line_number = GetScriptLineNumber(script, position);
        // line_number is already shifted by the script_line_offset.
        int relative_line_number = line_number - script_line_offset;
        if (options & StackTrace::kColumnOffset && relative_line_number >= 0) {
          Handle<FixedArray> line_ends(FixedArray::cast(script->line_ends()));
          int start = (relative_line_number == 0) ? 0 :
              Smi::cast(line_ends->get(relative_line_number - 1))->value() + 1;
          int column_offset = position - start;
          if (relative_line_number == 0) {
            // For the case where the code is on the same line as the script
            // tag.
            column_offset += script->column_offset()->value();
          }
          SetLocalPropertyNoThrow(stackFrame, column_key,
                                  Handle<Smi>(Smi::FromInt(column_offset + 1)));
        }
        SetLocalPropertyNoThrow(stackFrame, line_key,
                                Handle<Smi>(Smi::FromInt(line_number + 1)));
      }

      if (options & StackTrace::kScriptName) {
        Handle<Object> script_name(script->name(), this);
        SetLocalPropertyNoThrow(stackFrame, script_key, script_name);
      }

      if (options & StackTrace::kScriptNameOrSourceURL) {
        Handle<Object> script_name(script->name(), this);
        Handle<JSValue> script_wrapper = GetScriptWrapper(script);
        Handle<Object> property = GetProperty(script_wrapper,
                                              name_or_source_url_key);
        ASSERT(property->IsJSFunction());
        Handle<JSFunction> method = Handle<JSFunction>::cast(property);
        bool caught_exception;
        Handle<Object> result = Execution::TryCall(method, script_wrapper, 0,
                                                   NULL, &caught_exception);
        if (caught_exception) {
          result = factory()->undefined_value();
        }
        SetLocalPropertyNoThrow(stackFrame, script_name_or_source_url_key,
                                result);
      }

      if (options & StackTrace::kFunctionName) {
        Handle<Object> fun_name(fun->shared()->name(), this);
        if (fun_name->ToBoolean()->IsFalse()) {
          fun_name = Handle<Object>(fun->shared()->inferred_name(), this);
        }
        SetLocalPropertyNoThrow(stackFrame, function_key, fun_name);
      }

      if (options & StackTrace::kIsEval) {
        int type = Smi::cast(script->compilation_type())->value();
        Handle<Object> is_eval = (type == Script::COMPILATION_TYPE_EVAL) ?
            factory()->true_value() : factory()->false_value();
        SetLocalPropertyNoThrow(stackFrame, eval_key, is_eval);
      }

      if (options & StackTrace::kIsConstructor) {
        Handle<Object> is_constructor = (frames[i].is_constructor()) ?
            factory()->true_value() : factory()->false_value();
        SetLocalPropertyNoThrow(stackFrame, constructor_key, is_constructor);
      }

      FixedArray::cast(stack_trace->elements())->set(frames_seen, *stackFrame);
      frames_seen++;
    }
    it.Advance();
  }

  stack_trace->set_length(Smi::FromInt(frames_seen));
  return stack_trace;
}


void Isolate::PrintStack() {
  if (stack_trace_nesting_level_ == 0) {
    stack_trace_nesting_level_++;

    StringAllocator* allocator;
    if (preallocated_message_space_ == NULL) {
      allocator = new HeapStringAllocator();
    } else {
      allocator = preallocated_message_space_;
    }

    StringStream::ClearMentionedObjectCache();
    StringStream accumulator(allocator);
    incomplete_message_ = &accumulator;
    PrintStack(&accumulator);
    accumulator.OutputToStdOut();
    accumulator.Log();
    incomplete_message_ = NULL;
    stack_trace_nesting_level_ = 0;
    if (preallocated_message_space_ == NULL) {
      // Remove the HeapStringAllocator created above.
      delete allocator;
    }
  } else if (stack_trace_nesting_level_ == 1) {
    stack_trace_nesting_level_++;
    OS::PrintError(
      "\n\nAttempt to print stack while printing stack (double fault)\n");
    OS::PrintError(
      "If you are lucky you may find a partial stack dump on stdout.\n\n");
    incomplete_message_->OutputToStdOut();
  }
}


static void PrintFrames(StringStream* accumulator,
                        StackFrame::PrintMode mode) {
  StackFrameIterator it;
  for (int i = 0; !it.done(); it.Advance()) {
    it.frame()->Print(accumulator, mode, i++);
  }
}


void Isolate::PrintStack(StringStream* accumulator) {
  if (!IsInitialized()) {
    accumulator->Add(
        "\n==== Stack trace is not available ==========================\n\n");
    accumulator->Add(
        "\n==== Isolate for the thread is not initialized =============\n\n");
    return;
  }
  // The MentionedObjectCache is not GC-proof at the moment.
  AssertNoAllocation nogc;
  ASSERT(StringStream::IsMentionedObjectCacheClear());

  // Avoid printing anything if there are no frames.
  if (c_entry_fp(thread_local_top()) == 0) return;

  accumulator->Add(
      "\n==== Stack trace ============================================\n\n");
  PrintFrames(accumulator, StackFrame::OVERVIEW);

  accumulator->Add(
      "\n==== Details ================================================\n\n");
  PrintFrames(accumulator, StackFrame::DETAILS);

  accumulator->PrintMentionedObjectCache();
  accumulator->Add("=====================\n\n");
}


void Isolate::SetFailedAccessCheckCallback(
    v8::FailedAccessCheckCallback callback) {
  thread_local_top()->failed_access_check_callback_ = callback;
}


void Isolate::ReportFailedAccessCheck(JSObject* receiver, v8::AccessType type) {
  if (!thread_local_top()->failed_access_check_callback_) return;

  ASSERT(receiver->IsAccessCheckNeeded());
  ASSERT(context());

  // Get the data object from access check info.
  JSFunction* constructor = JSFunction::cast(receiver->map()->constructor());
  if (!constructor->shared()->IsApiFunction()) return;
  Object* data_obj =
      constructor->shared()->get_api_func_data()->access_check_info();
  if (data_obj == heap_.undefined_value()) return;

  HandleScope scope;
  Handle<JSObject> receiver_handle(receiver);
  Handle<Object> data(AccessCheckInfo::cast(data_obj)->data());
  thread_local_top()->failed_access_check_callback_(
    v8::Utils::ToLocal(receiver_handle),
    type,
    v8::Utils::ToLocal(data));
}


enum MayAccessDecision {
  YES, NO, UNKNOWN
};


static MayAccessDecision MayAccessPreCheck(Isolate* isolate,
                                           JSObject* receiver,
                                           v8::AccessType type) {
  // During bootstrapping, callback functions are not enabled yet.
  if (isolate->bootstrapper()->IsActive()) return YES;

  if (receiver->IsJSGlobalProxy()) {
    Object* receiver_context = JSGlobalProxy::cast(receiver)->context();
    if (!receiver_context->IsContext()) return NO;

    // Get the global context of current top context.
    // avoid using Isolate::global_context() because it uses Handle.
    Context* global_context = isolate->context()->global()->global_context();
    if (receiver_context == global_context) return YES;

    if (Context::cast(receiver_context)->security_token() ==
        global_context->security_token())
      return YES;
  }

  return UNKNOWN;
}


bool Isolate::MayNamedAccess(JSObject* receiver, Object* key,
                             v8::AccessType type) {
  ASSERT(receiver->IsAccessCheckNeeded());

  // The callers of this method are not expecting a GC.
  AssertNoAllocation no_gc;

  // Skip checks for hidden properties access.  Note, we do not
  // require existence of a context in this case.
  if (key == heap_.hidden_symbol()) return true;

  // Check for compatibility between the security tokens in the
  // current lexical context and the accessed object.
  ASSERT(context());

  MayAccessDecision decision = MayAccessPreCheck(this, receiver, type);
  if (decision != UNKNOWN) return decision == YES;

  // Get named access check callback
  JSFunction* constructor = JSFunction::cast(receiver->map()->constructor());
  if (!constructor->shared()->IsApiFunction()) return false;

  Object* data_obj =
     constructor->shared()->get_api_func_data()->access_check_info();
  if (data_obj == heap_.undefined_value()) return false;

  Object* fun_obj = AccessCheckInfo::cast(data_obj)->named_callback();
  v8::NamedSecurityCallback callback =
      v8::ToCData<v8::NamedSecurityCallback>(fun_obj);

  if (!callback) return false;

  HandleScope scope(this);
  Handle<JSObject> receiver_handle(receiver, this);
  Handle<Object> key_handle(key, this);
  Handle<Object> data(AccessCheckInfo::cast(data_obj)->data(), this);
  LOG(this, ApiNamedSecurityCheck(key));
  bool result = false;
  {
    // Leaving JavaScript.
    VMState state(this, EXTERNAL);
    result = callback(v8::Utils::ToLocal(receiver_handle),
                      v8::Utils::ToLocal(key_handle),
                      type,
                      v8::Utils::ToLocal(data));
  }
  return result;
}


bool Isolate::MayIndexedAccess(JSObject* receiver,
                               uint32_t index,
                               v8::AccessType type) {
  ASSERT(receiver->IsAccessCheckNeeded());
  // Check for compatibility between the security tokens in the
  // current lexical context and the accessed object.
  ASSERT(context());

  MayAccessDecision decision = MayAccessPreCheck(this, receiver, type);
  if (decision != UNKNOWN) return decision == YES;

  // Get indexed access check callback
  JSFunction* constructor = JSFunction::cast(receiver->map()->constructor());
  if (!constructor->shared()->IsApiFunction()) return false;

  Object* data_obj =
      constructor->shared()->get_api_func_data()->access_check_info();
  if (data_obj == heap_.undefined_value()) return false;

  Object* fun_obj = AccessCheckInfo::cast(data_obj)->indexed_callback();
  v8::IndexedSecurityCallback callback =
      v8::ToCData<v8::IndexedSecurityCallback>(fun_obj);

  if (!callback) return false;

  HandleScope scope(this);
  Handle<JSObject> receiver_handle(receiver, this);
  Handle<Object> data(AccessCheckInfo::cast(data_obj)->data(), this);
  LOG(this, ApiIndexedSecurityCheck(index));
  bool result = false;
  {
    // Leaving JavaScript.
    VMState state(this, EXTERNAL);
    result = callback(v8::Utils::ToLocal(receiver_handle),
                      index,
                      type,
                      v8::Utils::ToLocal(data));
  }
  return result;
}


const char* const Isolate::kStackOverflowMessage =
  "Uncaught RangeError: Maximum call stack size exceeded";


Failure* Isolate::StackOverflow() {
  HandleScope scope;
  Handle<String> key = factory()->stack_overflow_symbol();
  Handle<JSObject> boilerplate =
      Handle<JSObject>::cast(GetProperty(js_builtins_object(), key));
  Handle<Object> exception = Copy(boilerplate);
  // TODO(1240995): To avoid having to call JavaScript code to compute
  // the message for stack overflow exceptions which is very likely to
  // double fault with another stack overflow exception, we use a
  // precomputed message.
  DoThrow(*exception, NULL);
  return Failure::Exception();
}


Failure* Isolate::TerminateExecution() {
  DoThrow(heap_.termination_exception(), NULL);
  return Failure::Exception();
}


Failure* Isolate::Throw(Object* exception, MessageLocation* location) {
  DoThrow(exception, location);
  return Failure::Exception();
}


Failure* Isolate::ReThrow(MaybeObject* exception, MessageLocation* location) {
  bool can_be_caught_externally = false;
  ShouldReportException(&can_be_caught_externally,
                        is_catchable_by_javascript(exception));
  thread_local_top()->catcher_ = can_be_caught_externally ?
      try_catch_handler() : NULL;

  // Set the exception being re-thrown.
  set_pending_exception(exception);
  return Failure::Exception();
}


Failure* Isolate::ThrowIllegalOperation() {
  return Throw(heap_.illegal_access_symbol());
}


void Isolate::ScheduleThrow(Object* exception) {
  // When scheduling a throw we first throw the exception to get the
  // error reporting if it is uncaught before rescheduling it.
  Throw(exception);
  thread_local_top()->scheduled_exception_ = pending_exception();
  thread_local_top()->external_caught_exception_ = false;
  clear_pending_exception();
}


Failure* Isolate::PromoteScheduledException() {
  MaybeObject* thrown = scheduled_exception();
  clear_scheduled_exception();
  // Re-throw the exception to avoid getting repeated error reporting.
  return ReThrow(thrown);
}


void Isolate::PrintCurrentStackTrace(FILE* out) {
  StackTraceFrameIterator it(this);
  while (!it.done()) {
    HandleScope scope;
    // Find code position if recorded in relocation info.
    JavaScriptFrame* frame = it.frame();
    int pos = frame->LookupCode()->SourcePosition(frame->pc());
    Handle<Object> pos_obj(Smi::FromInt(pos));
    // Fetch function and receiver.
    Handle<JSFunction> fun(JSFunction::cast(frame->function()));
    Handle<Object> recv(frame->receiver());
    // Advance to the next JavaScript frame and determine if the
    // current frame is the top-level frame.
    it.Advance();
    Handle<Object> is_top_level = it.done()
        ? factory()->true_value()
        : factory()->false_value();
    // Generate and print stack trace line.
    Handle<String> line =
        Execution::GetStackTraceLine(recv, fun, pos_obj, is_top_level);
    if (line->length() > 0) {
      line->PrintOn(out);
      fprintf(out, "\n");
    }
  }
}


void Isolate::ComputeLocation(MessageLocation* target) {
  *target = MessageLocation(Handle<Script>(heap_.empty_script()), -1, -1);
  StackTraceFrameIterator it(this);
  if (!it.done()) {
    JavaScriptFrame* frame = it.frame();
    JSFunction* fun = JSFunction::cast(frame->function());
    Object* script = fun->shared()->script();
    if (script->IsScript() &&
        !(Script::cast(script)->source()->IsUndefined())) {
      int pos = frame->LookupCode()->SourcePosition(frame->pc());
      // Compute the location from the function and the reloc info.
      Handle<Script> casted_script(Script::cast(script));
      *target = MessageLocation(casted_script, pos, pos + 1);
    }
  }
}


bool Isolate::ShouldReportException(bool* can_be_caught_externally,
                                    bool catchable_by_javascript) {
  // Find the top-most try-catch handler.
  StackHandler* handler =
      StackHandler::FromAddress(Isolate::handler(thread_local_top()));
  while (handler != NULL && !handler->is_try_catch()) {
    handler = handler->next();
  }

  // Get the address of the external handler so we can compare the address to
  // determine which one is closer to the top of the stack.
  Address external_handler_address =
      thread_local_top()->try_catch_handler_address();

  // The exception has been externally caught if and only if there is
  // an external handler which is on top of the top-most try-catch
  // handler.
  *can_be_caught_externally = external_handler_address != NULL &&
      (handler == NULL || handler->address() > external_handler_address ||
       !catchable_by_javascript);

  if (*can_be_caught_externally) {
    // Only report the exception if the external handler is verbose.
    return try_catch_handler()->is_verbose_;
  } else {
    // Report the exception if it isn't caught by JavaScript code.
    return handler == NULL;
  }
}


void Isolate::DoThrow(MaybeObject* exception, MessageLocation* location) {
  ASSERT(!has_pending_exception());

  HandleScope scope;
  Object* exception_object = Smi::FromInt(0);
  bool is_object = exception->ToObject(&exception_object);
  Handle<Object> exception_handle(exception_object);

  // Determine reporting and whether the exception is caught externally.
  bool catchable_by_javascript = is_catchable_by_javascript(exception);
  // Only real objects can be caught by JS.
  ASSERT(!catchable_by_javascript || is_object);
  bool can_be_caught_externally = false;
  bool should_report_exception =
      ShouldReportException(&can_be_caught_externally, catchable_by_javascript);
  bool report_exception = catchable_by_javascript && should_report_exception;

#ifdef ENABLE_DEBUGGER_SUPPORT
  // Notify debugger of exception.
  if (catchable_by_javascript) {
    debugger_->OnException(exception_handle, report_exception);
  }
#endif

  // Generate the message.
  Handle<Object> message_obj;
  MessageLocation potential_computed_location;
  bool try_catch_needs_message =
      can_be_caught_externally &&
      try_catch_handler()->capture_message_;
  if (report_exception || try_catch_needs_message) {
    if (location == NULL) {
      // If no location was specified we use a computed one instead
      ComputeLocation(&potential_computed_location);
      location = &potential_computed_location;
    }
    if (!bootstrapper()->IsActive()) {
      // It's not safe to try to make message objects or collect stack
      // traces while the bootstrapper is active since the infrastructure
      // may not have been properly initialized.
      Handle<String> stack_trace;
      if (FLAG_trace_exception) stack_trace = StackTraceString();
      Handle<JSArray> stack_trace_object;
      if (report_exception && capture_stack_trace_for_uncaught_exceptions_) {
          stack_trace_object = CaptureCurrentStackTrace(
              stack_trace_for_uncaught_exceptions_frame_limit_,
              stack_trace_for_uncaught_exceptions_options_);
      }
      ASSERT(is_object);  // Can't use the handle unless there's a real object.
      message_obj = MessageHandler::MakeMessageObject("uncaught_exception",
          location, HandleVector<Object>(&exception_handle, 1), stack_trace,
          stack_trace_object);
    }
  }

  // Save the message for reporting if the the exception remains uncaught.
  thread_local_top()->has_pending_message_ = report_exception;
  if (!message_obj.is_null()) {
    thread_local_top()->pending_message_obj_ = *message_obj;
    if (location != NULL) {
      thread_local_top()->pending_message_script_ = *location->script();
      thread_local_top()->pending_message_start_pos_ = location->start_pos();
      thread_local_top()->pending_message_end_pos_ = location->end_pos();
    }
  }

  // Do not forget to clean catcher_ if currently thrown exception cannot
  // be caught.  If necessary, ReThrow will update the catcher.
  thread_local_top()->catcher_ = can_be_caught_externally ?
      try_catch_handler() : NULL;

  // NOTE: Notifying the debugger or generating the message
  // may have caused new exceptions. For now, we just ignore
  // that and set the pending exception to the original one.
  if (is_object) {
    set_pending_exception(*exception_handle);
  } else {
    // Failures are not on the heap so they neither need nor work with handles.
    ASSERT(exception_handle->IsFailure());
    set_pending_exception(exception);
  }
}


bool Isolate::IsExternallyCaught() {
  ASSERT(has_pending_exception());

  if ((thread_local_top()->catcher_ == NULL) ||
      (try_catch_handler() != thread_local_top()->catcher_)) {
    // When throwing the exception, we found no v8::TryCatch
    // which should care about this exception.
    return false;
  }

  if (!is_catchable_by_javascript(pending_exception())) {
    return true;
  }

  // Get the address of the external handler so we can compare the address to
  // determine which one is closer to the top of the stack.
  Address external_handler_address =
      thread_local_top()->try_catch_handler_address();
  ASSERT(external_handler_address != NULL);

  // The exception has been externally caught if and only if there is
  // an external handler which is on top of the top-most try-finally
  // handler.
  // There should be no try-catch blocks as they would prohibit us from
  // finding external catcher in the first place (see catcher_ check above).
  //
  // Note, that finally clause would rethrow an exception unless it's
  // aborted by jumps in control flow like return, break, etc. and we'll
  // have another chances to set proper v8::TryCatch.
  StackHandler* handler =
      StackHandler::FromAddress(Isolate::handler(thread_local_top()));
  while (handler != NULL && handler->address() < external_handler_address) {
    ASSERT(!handler->is_try_catch());
    if (handler->is_try_finally()) return false;

    handler = handler->next();
  }

  return true;
}


void Isolate::ReportPendingMessages() {
  ASSERT(has_pending_exception());
  PropagatePendingExceptionToExternalTryCatch();

  // If the pending exception is OutOfMemoryException set out_of_memory in
  // the global context.  Note: We have to mark the global context here
  // since the GenerateThrowOutOfMemory stub cannot make a RuntimeCall to
  // set it.
  HandleScope scope;
  if (thread_local_top_.pending_exception_ == Failure::OutOfMemoryException()) {
    context()->mark_out_of_memory();
  } else if (thread_local_top_.pending_exception_ ==
             heap()->termination_exception()) {
    // Do nothing: if needed, the exception has been already propagated to
    // v8::TryCatch.
  } else {
    if (thread_local_top_.has_pending_message_) {
      thread_local_top_.has_pending_message_ = false;
      if (!thread_local_top_.pending_message_obj_->IsTheHole()) {
        HandleScope scope;
        Handle<Object> message_obj(thread_local_top_.pending_message_obj_);
        if (thread_local_top_.pending_message_script_ != NULL) {
          Handle<Script> script(thread_local_top_.pending_message_script_);
          int start_pos = thread_local_top_.pending_message_start_pos_;
          int end_pos = thread_local_top_.pending_message_end_pos_;
          MessageLocation location(script, start_pos, end_pos);
          MessageHandler::ReportMessage(this, &location, message_obj);
        } else {
          MessageHandler::ReportMessage(this, NULL, message_obj);
        }
      }
    }
  }
  clear_pending_message();
}


void Isolate::TraceException(bool flag) {
  FLAG_trace_exception = flag;  // TODO(isolates): This is an unfortunate use.
}


bool Isolate::OptionalRescheduleException(bool is_bottom_call) {
  ASSERT(has_pending_exception());
  PropagatePendingExceptionToExternalTryCatch();

  // Allways reschedule out of memory exceptions.
  if (!is_out_of_memory()) {
    bool is_termination_exception =
        pending_exception() == heap_.termination_exception();

    // Do not reschedule the exception if this is the bottom call.
    bool clear_exception = is_bottom_call;

    if (is_termination_exception) {
      if (is_bottom_call) {
        thread_local_top()->external_caught_exception_ = false;
        clear_pending_exception();
        return false;
      }
    } else if (thread_local_top()->external_caught_exception_) {
      // If the exception is externally caught, clear it if there are no
      // JavaScript frames on the way to the C++ frame that has the
      // external handler.
      ASSERT(thread_local_top()->try_catch_handler_address() != NULL);
      Address external_handler_address =
          thread_local_top()->try_catch_handler_address();
      JavaScriptFrameIterator it;
      if (it.done() || (it.frame()->sp() > external_handler_address)) {
        clear_exception = true;
      }
    }

    // Clear the exception if needed.
    if (clear_exception) {
      thread_local_top()->external_caught_exception_ = false;
      clear_pending_exception();
      return false;
    }
  }

  // Reschedule the exception.
  thread_local_top()->scheduled_exception_ = pending_exception();
  clear_pending_exception();
  return true;
}


void Isolate::SetCaptureStackTraceForUncaughtExceptions(
      bool capture,
      int frame_limit,
      StackTrace::StackTraceOptions options) {
  capture_stack_trace_for_uncaught_exceptions_ = capture;
  stack_trace_for_uncaught_exceptions_frame_limit_ = frame_limit;
  stack_trace_for_uncaught_exceptions_options_ = options;
}


bool Isolate::is_out_of_memory() {
  if (has_pending_exception()) {
    MaybeObject* e = pending_exception();
    if (e->IsFailure() && Failure::cast(e)->IsOutOfMemoryException()) {
      return true;
    }
  }
  if (has_scheduled_exception()) {
    MaybeObject* e = scheduled_exception();
    if (e->IsFailure() && Failure::cast(e)->IsOutOfMemoryException()) {
      return true;
    }
  }
  return false;
}


Handle<Context> Isolate::global_context() {
  GlobalObject* global = thread_local_top()->context_->global();
  return Handle<Context>(global->global_context());
}


Handle<Context> Isolate::GetCallingGlobalContext() {
  JavaScriptFrameIterator it;
#ifdef ENABLE_DEBUGGER_SUPPORT
  if (debug_->InDebugger()) {
    while (!it.done()) {
      JavaScriptFrame* frame = it.frame();
      Context* context = Context::cast(frame->context());
      if (context->global_context() == *debug_->debug_context()) {
        it.Advance();
      } else {
        break;
      }
    }
  }
#endif  // ENABLE_DEBUGGER_SUPPORT
  if (it.done()) return Handle<Context>::null();
  JavaScriptFrame* frame = it.frame();
  Context* context = Context::cast(frame->context());
  return Handle<Context>(context->global_context());
}


char* Isolate::ArchiveThread(char* to) {
  if (RuntimeProfiler::IsEnabled() && current_vm_state() == JS) {
    RuntimeProfiler::IsolateExitedJS(this);
  }
  memcpy(to, reinterpret_cast<char*>(thread_local_top()),
         sizeof(ThreadLocalTop));
  InitializeThreadLocal();
  return to + sizeof(ThreadLocalTop);
}


char* Isolate::RestoreThread(char* from) {
  memcpy(reinterpret_cast<char*>(thread_local_top()), from,
         sizeof(ThreadLocalTop));
  // This might be just paranoia, but it seems to be needed in case a
  // thread_local_top_ is restored on a separate OS thread.
#ifdef USE_SIMULATOR
#ifdef V8_TARGET_ARCH_ARM
  thread_local_top()->simulator_ = Simulator::current(this);
#elif V8_TARGET_ARCH_MIPS
  thread_local_top()->simulator_ = Simulator::current(this);
#endif
#endif
  if (RuntimeProfiler::IsEnabled() && current_vm_state() == JS) {
    RuntimeProfiler::IsolateEnteredJS(this);
  }
  return from + sizeof(ThreadLocalTop);
}


Isolate::ThreadDataTable::ThreadDataTable()
    : list_(NULL) {
}


Isolate::PerIsolateThreadData*
    Isolate::ThreadDataTable::Lookup(Isolate* isolate,
                                     ThreadId thread_id) {
  for (PerIsolateThreadData* data = list_; data != NULL; data = data->next_) {
    if (data->Matches(isolate, thread_id)) return data;
  }
  return NULL;
}


void Isolate::ThreadDataTable::Insert(Isolate::PerIsolateThreadData* data) {
  if (list_ != NULL) list_->prev_ = data;
  data->next_ = list_;
  list_ = data;
}


void Isolate::ThreadDataTable::Remove(PerIsolateThreadData* data) {
  if (list_ == data) list_ = data->next_;
  if (data->next_ != NULL) data->next_->prev_ = data->prev_;
  if (data->prev_ != NULL) data->prev_->next_ = data->next_;
}


void Isolate::ThreadDataTable::Remove(Isolate* isolate,
                                      ThreadId thread_id) {
  PerIsolateThreadData* data = Lookup(isolate, thread_id);
  if (data != NULL) {
    Remove(data);
  }
}


#ifdef DEBUG
#define TRACE_ISOLATE(tag)                                              \
  do {                                                                  \
    if (FLAG_trace_isolates) {                                          \
      PrintF("Isolate %p " #tag "\n", reinterpret_cast<void*>(this));   \
    }                                                                   \
  } while (false)
#else
#define TRACE_ISOLATE(tag)
#endif


Isolate::Isolate()
    : state_(UNINITIALIZED),
      entry_stack_(NULL),
      stack_trace_nesting_level_(0),
      incomplete_message_(NULL),
      preallocated_memory_thread_(NULL),
      preallocated_message_space_(NULL),
      bootstrapper_(NULL),
      runtime_profiler_(NULL),
      compilation_cache_(NULL),
      counters_(new Counters()),
      code_range_(NULL),
      break_access_(OS::CreateMutex()),
      logger_(new Logger()),
      stats_table_(new StatsTable()),
      stub_cache_(NULL),
      deoptimizer_data_(NULL),
      capture_stack_trace_for_uncaught_exceptions_(false),
      stack_trace_for_uncaught_exceptions_frame_limit_(0),
      stack_trace_for_uncaught_exceptions_options_(StackTrace::kOverview),
      transcendental_cache_(NULL),
      memory_allocator_(NULL),
      keyed_lookup_cache_(NULL),
      context_slot_cache_(NULL),
      descriptor_lookup_cache_(NULL),
      handle_scope_implementer_(NULL),
      unicode_cache_(NULL),
      in_use_list_(0),
      free_list_(0),
      preallocated_storage_preallocated_(false),
      pc_to_code_cache_(NULL),
      write_input_buffer_(NULL),
      global_handles_(NULL),
      context_switcher_(NULL),
      thread_manager_(NULL),
      ast_sentinels_(NULL),
      string_tracker_(NULL),
      regexp_stack_(NULL),
      frame_element_constant_list_(0),
      result_constant_list_(0) {
  TRACE_ISOLATE(constructor);

  memset(isolate_addresses_, 0,
      sizeof(isolate_addresses_[0]) * (k_isolate_address_count + 1));

  heap_.isolate_ = this;
  zone_.isolate_ = this;
  stack_guard_.isolate_ = this;

  // ThreadManager is initialized early to support locking an isolate
  // before it is entered.
  thread_manager_ = new ThreadManager();
  thread_manager_->isolate_ = this;

#if defined(V8_TARGET_ARCH_ARM) && !defined(__arm__) || \
    defined(V8_TARGET_ARCH_MIPS) && !defined(__mips__)
  simulator_initialized_ = false;
  simulator_i_cache_ = NULL;
  simulator_redirection_ = NULL;
#endif

#ifdef DEBUG
  // heap_histograms_ initializes itself.
  memset(&js_spill_information_, 0, sizeof(js_spill_information_));
  memset(code_kind_statistics_, 0,
         sizeof(code_kind_statistics_[0]) * Code::NUMBER_OF_KINDS);
#endif

#ifdef ENABLE_DEBUGGER_SUPPORT
  debug_ = NULL;
  debugger_ = NULL;
#endif

#ifdef ENABLE_LOGGING_AND_PROFILING
  producer_heap_profile_ = NULL;
#endif

  handle_scope_data_.Initialize();

#define ISOLATE_INIT_EXECUTE(type, name, initial_value)                        \
  name##_ = (initial_value);
  ISOLATE_INIT_LIST(ISOLATE_INIT_EXECUTE)
#undef ISOLATE_INIT_EXECUTE

#define ISOLATE_INIT_ARRAY_EXECUTE(type, name, length)                         \
  memset(name##_, 0, sizeof(type) * length);
  ISOLATE_INIT_ARRAY_LIST(ISOLATE_INIT_ARRAY_EXECUTE)
#undef ISOLATE_INIT_ARRAY_EXECUTE
}

void Isolate::TearDown() {
  TRACE_ISOLATE(tear_down);

  // Temporarily set this isolate as current so that various parts of
  // the isolate can access it in their destructors without having a
  // direct pointer. We don't use Enter/Exit here to avoid
  // initializing the thread data.
  PerIsolateThreadData* saved_data = CurrentPerIsolateThreadData();
  Isolate* saved_isolate = UncheckedCurrent();
  SetIsolateThreadLocals(this, NULL);

  Deinit();

  if (!IsDefaultIsolate()) {
    delete this;
  }

  // Restore the previous current isolate.
  SetIsolateThreadLocals(saved_isolate, saved_data);
}


void Isolate::Deinit() {
  if (state_ == INITIALIZED) {
    TRACE_ISOLATE(deinit);

    if (FLAG_hydrogen_stats) HStatistics::Instance()->Print();

    // We must stop the logger before we tear down other components.
    logger_->EnsureTickerStopped();

    delete deoptimizer_data_;
    deoptimizer_data_ = NULL;
    if (FLAG_preemption) {
      v8::Locker locker;
      v8::Locker::StopPreemption();
    }
    builtins_.TearDown();
    bootstrapper_->TearDown();

    // Remove the external reference to the preallocated stack memory.
    delete preallocated_message_space_;
    preallocated_message_space_ = NULL;
    PreallocatedMemoryThreadStop();

    HeapProfiler::TearDown();
    CpuProfiler::TearDown();
    if (runtime_profiler_ != NULL) {
      runtime_profiler_->TearDown();
      delete runtime_profiler_;
      runtime_profiler_ = NULL;
    }
    heap_.TearDown();
    logger_->TearDown();

    // The default isolate is re-initializable due to legacy API.
    state_ = PREINITIALIZED;
  }
}


void Isolate::SetIsolateThreadLocals(Isolate* isolate,
                                     PerIsolateThreadData* data) {
  Thread::SetThreadLocal(isolate_key_, isolate);
  Thread::SetThreadLocal(per_isolate_thread_data_key_, data);
}


Isolate::~Isolate() {
  TRACE_ISOLATE(destructor);

#ifdef ENABLE_LOGGING_AND_PROFILING
  delete producer_heap_profile_;
  producer_heap_profile_ = NULL;
#endif

  delete unicode_cache_;
  unicode_cache_ = NULL;

  delete regexp_stack_;
  regexp_stack_ = NULL;

  delete ast_sentinels_;
  ast_sentinels_ = NULL;

  delete descriptor_lookup_cache_;
  descriptor_lookup_cache_ = NULL;
  delete context_slot_cache_;
  context_slot_cache_ = NULL;
  delete keyed_lookup_cache_;
  keyed_lookup_cache_ = NULL;

  delete transcendental_cache_;
  transcendental_cache_ = NULL;
  delete stub_cache_;
  stub_cache_ = NULL;
  delete stats_table_;
  stats_table_ = NULL;

  delete logger_;
  logger_ = NULL;

  delete counters_;
  counters_ = NULL;

  delete handle_scope_implementer_;
  handle_scope_implementer_ = NULL;
  delete break_access_;
  break_access_ = NULL;

  delete compilation_cache_;
  compilation_cache_ = NULL;
  delete bootstrapper_;
  bootstrapper_ = NULL;
  delete pc_to_code_cache_;
  pc_to_code_cache_ = NULL;
  delete write_input_buffer_;
  write_input_buffer_ = NULL;

  delete context_switcher_;
  context_switcher_ = NULL;
  delete thread_manager_;
  thread_manager_ = NULL;

  delete string_tracker_;
  string_tracker_ = NULL;

  delete memory_allocator_;
  memory_allocator_ = NULL;
  delete code_range_;
  code_range_ = NULL;
  delete global_handles_;
  global_handles_ = NULL;

#ifdef ENABLE_DEBUGGER_SUPPORT
  delete debugger_;
  debugger_ = NULL;
  delete debug_;
  debug_ = NULL;
#endif
}


bool Isolate::PreInit() {
  if (state_ != UNINITIALIZED) return true;

  TRACE_ISOLATE(preinit);

  ASSERT(Isolate::Current() == this);
#ifdef ENABLE_DEBUGGER_SUPPORT
  debug_ = new Debug(this);
  debugger_ = new Debugger();
  debugger_->isolate_ = this;
#endif

  memory_allocator_ = new MemoryAllocator();
  memory_allocator_->isolate_ = this;
  code_range_ = new CodeRange();
  code_range_->isolate_ = this;

  // Safe after setting Heap::isolate_, initializing StackGuard and
  // ensuring that Isolate::Current() == this.
  heap_.SetStackLimits();

#ifdef DEBUG
  DisallowAllocationFailure disallow_allocation_failure;
#endif

#define C(name) isolate_addresses_[Isolate::k_##name] =                        \
    reinterpret_cast<Address>(name());
  ISOLATE_ADDRESS_LIST(C)
  ISOLATE_ADDRESS_LIST_PROF(C)
#undef C

  string_tracker_ = new StringTracker();
  string_tracker_->isolate_ = this;
  compilation_cache_ = new CompilationCache(this);
  transcendental_cache_ = new TranscendentalCache();
  keyed_lookup_cache_ = new KeyedLookupCache();
  context_slot_cache_ = new ContextSlotCache();
  descriptor_lookup_cache_ = new DescriptorLookupCache();
  unicode_cache_ = new UnicodeCache();
  pc_to_code_cache_ = new PcToCodeCache(this);
  write_input_buffer_ = new StringInputBuffer();
  global_handles_ = new GlobalHandles(this);
  bootstrapper_ = new Bootstrapper();
  handle_scope_implementer_ = new HandleScopeImplementer(this);
  stub_cache_ = new StubCache(this);
  ast_sentinels_ = new AstSentinels();
  regexp_stack_ = new RegExpStack();
  regexp_stack_->isolate_ = this;

#ifdef ENABLE_LOGGING_AND_PROFILING
  producer_heap_profile_ = new ProducerHeapProfile();
  producer_heap_profile_->isolate_ = this;
#endif

  state_ = PREINITIALIZED;
  return true;
}


void Isolate::InitializeThreadLocal() {
  thread_local_top_.isolate_ = this;
  thread_local_top_.Initialize();
  clear_pending_exception();
  clear_pending_message();
  clear_scheduled_exception();
}


void Isolate::PropagatePendingExceptionToExternalTryCatch() {
  ASSERT(has_pending_exception());

  bool external_caught = IsExternallyCaught();
  thread_local_top_.external_caught_exception_ = external_caught;

  if (!external_caught) return;

  if (thread_local_top_.pending_exception_ == Failure::OutOfMemoryException()) {
    // Do not propagate OOM exception: we should kill VM asap.
  } else if (thread_local_top_.pending_exception_ ==
             heap()->termination_exception()) {
    try_catch_handler()->can_continue_ = false;
    try_catch_handler()->exception_ = heap()->null_value();
  } else {
    // At this point all non-object (failure) exceptions have
    // been dealt with so this shouldn't fail.
    ASSERT(!pending_exception()->IsFailure());
    try_catch_handler()->can_continue_ = true;
    try_catch_handler()->exception_ = pending_exception();
    if (!thread_local_top_.pending_message_obj_->IsTheHole()) {
      try_catch_handler()->message_ = thread_local_top_.pending_message_obj_;
    }
  }
}


bool Isolate::Init(Deserializer* des) {
  ASSERT(state_ != INITIALIZED);

  TRACE_ISOLATE(init);

  bool create_heap_objects = des == NULL;

#ifdef DEBUG
  // The initialization process does not handle memory exhaustion.
  DisallowAllocationFailure disallow_allocation_failure;
#endif

  if (state_ == UNINITIALIZED && !PreInit()) return false;

  // Enable logging before setting up the heap
  logger_->Setup();

  CpuProfiler::Setup();
  HeapProfiler::Setup();

  // Initialize other runtime facilities
#if defined(USE_SIMULATOR)
#if defined(V8_TARGET_ARCH_ARM) || defined(V8_TARGET_ARCH_MIPS)
  Simulator::Initialize(this);
#endif
#endif

  { // NOLINT
    // Ensure that the thread has a valid stack guard.  The v8::Locker object
    // will ensure this too, but we don't have to use lockers if we are only
    // using one thread.
    ExecutionAccess lock(this);
    stack_guard_.InitThread(lock);
  }

  // Setup the object heap
  ASSERT(!heap_.HasBeenSetup());
  if (!heap_.Setup(create_heap_objects)) {
    V8::SetFatalError();
    return false;
  }

  bootstrapper_->Initialize(create_heap_objects);
  builtins_.Setup(create_heap_objects);

  InitializeThreadLocal();

  // Only preallocate on the first initialization.
  if (FLAG_preallocate_message_memory && preallocated_message_space_ == NULL) {
    // Start the thread which will set aside some memory.
    PreallocatedMemoryThreadStart();
    preallocated_message_space_ =
        new NoAllocationStringAllocator(
            preallocated_memory_thread_->data(),
            preallocated_memory_thread_->length());
    PreallocatedStorageInit(preallocated_memory_thread_->length() / 4);
  }

  if (FLAG_preemption) {
    v8::Locker locker;
    v8::Locker::StartPreemption(100);
  }

#ifdef ENABLE_DEBUGGER_SUPPORT
  debug_->Setup(create_heap_objects);
#endif
  stub_cache_->Initialize(create_heap_objects);

  // If we are deserializing, read the state into the now-empty heap.
  if (des != NULL) {
    des->Deserialize();
    stub_cache_->Clear();
  }

  // Deserializing may put strange things in the root array's copy of the
  // stack guard.
  heap_.SetStackLimits();

  deoptimizer_data_ = new DeoptimizerData;
  runtime_profiler_ = new RuntimeProfiler(this);
  runtime_profiler_->Setup();

  // If we are deserializing, log non-function code objects and compiled
  // functions found in the snapshot.
  if (des != NULL && (FLAG_log_code || FLAG_ll_prof)) {
    HandleScope scope;
    LOG(this, LogCodeObjects());
    LOG(this, LogCompiledFunctions());
  }

  state_ = INITIALIZED;
  return true;
}


void Isolate::Enter() {
  Isolate* current_isolate = NULL;
  PerIsolateThreadData* current_data = CurrentPerIsolateThreadData();
  if (current_data != NULL) {
    current_isolate = current_data->isolate_;
    ASSERT(current_isolate != NULL);
    if (current_isolate == this) {
      ASSERT(Current() == this);
      ASSERT(entry_stack_ != NULL);
      ASSERT(entry_stack_->previous_thread_data == NULL ||
             entry_stack_->previous_thread_data->thread_id().Equals(
                 ThreadId::Current()));
      // Same thread re-enters the isolate, no need to re-init anything.
      entry_stack_->entry_count++;
      return;
    }
  }

  // Threads can have default isolate set into TLS as Current but not yet have
  // PerIsolateThreadData for it, as it requires more advanced phase of the
  // initialization. For example, a thread might be the one that system used for
  // static initializers - in this case the default isolate is set in TLS but
  // the thread did not yet Enter the isolate. If PerisolateThreadData is not
  // there, use the isolate set in TLS.
  if (current_isolate == NULL) {
    current_isolate = Isolate::UncheckedCurrent();
  }

  PerIsolateThreadData* data = FindOrAllocatePerThreadDataForThisThread();
  ASSERT(data != NULL);
  ASSERT(data->isolate_ == this);

  EntryStackItem* item = new EntryStackItem(current_data,
                                            current_isolate,
                                            entry_stack_);
  entry_stack_ = item;

  SetIsolateThreadLocals(this, data);

  CHECK(PreInit());

  // In case it's the first time some thread enters the isolate.
  set_thread_id(data->thread_id());
}


void Isolate::Exit() {
  ASSERT(entry_stack_ != NULL);
  ASSERT(entry_stack_->previous_thread_data == NULL ||
         entry_stack_->previous_thread_data->thread_id().Equals(
             ThreadId::Current()));

  if (--entry_stack_->entry_count > 0) return;

  ASSERT(CurrentPerIsolateThreadData() != NULL);
  ASSERT(CurrentPerIsolateThreadData()->isolate_ == this);

  // Pop the stack.
  EntryStackItem* item = entry_stack_;
  entry_stack_ = item->previous_item;

  PerIsolateThreadData* previous_thread_data = item->previous_thread_data;
  Isolate* previous_isolate = item->previous_isolate;

  delete item;

  // Reinit the current thread for the isolate it was running before this one.
  SetIsolateThreadLocals(previous_isolate, previous_thread_data);
}


void Isolate::ResetEagerOptimizingData() {
  compilation_cache_->ResetEagerOptimizingData();
}


#ifdef DEBUG
#define ISOLATE_FIELD_OFFSET(type, name, ignored)                       \
const intptr_t Isolate::name##_debug_offset_ = OFFSET_OF(Isolate, name##_);
ISOLATE_INIT_LIST(ISOLATE_FIELD_OFFSET)
ISOLATE_INIT_ARRAY_LIST(ISOLATE_FIELD_OFFSET)
#undef ISOLATE_FIELD_OFFSET
#endif

} }  // namespace v8::internal
