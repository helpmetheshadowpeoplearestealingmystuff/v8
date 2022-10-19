// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_ISOLATE_H_
#define V8_EXECUTION_ISOLATE_H_

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <queue>
#include <unordered_map>
#include <vector>

#include "include/v8-context.h"
#include "include/v8-internal.h"
#include "include/v8-isolate.h"
#include "include/v8-metrics.h"
#include "include/v8-snapshot.h"
#include "src/base/macros.h"
#include "src/base/platform/mutex.h"
#include "src/base/platform/platform-posix.h"
#include "src/builtins/builtins.h"
#include "src/common/globals.h"
#include "src/debug/interface-types.h"
#include "src/execution/execution.h"
#include "src/execution/futex-emulation.h"
#include "src/execution/isolate-data.h"
#include "src/execution/messages.h"
#include "src/execution/shared-mutex-guard-if-off-thread.h"
#include "src/execution/stack-guard.h"
#include "src/handles/handles.h"
#include "src/heap/factory.h"
#include "src/heap/heap.h"
#include "src/heap/read-only-heap.h"
#include "src/init/isolate-allocator.h"
#include "src/objects/code.h"
#include "src/objects/contexts.h"
#include "src/objects/debug-objects.h"
#include "src/objects/js-objects.h"
#include "src/runtime/runtime.h"
#include "src/sandbox/external-pointer.h"
#include "src/utils/allocation.h"

#ifdef DEBUG
#include "src/runtime/runtime-utils.h"
#endif

#ifdef V8_INTL_SUPPORT
#include "unicode/uversion.h"  // Define U_ICU_NAMESPACE.
namespace U_ICU_NAMESPACE {
class UMemory;
}  // namespace U_ICU_NAMESPACE
#endif  // V8_INTL_SUPPORT

#if USE_SIMULATOR
#include "src/execution/encoded-c-signature.h"
namespace v8 {
namespace internal {
class SimulatorData;
}
}  // namespace v8
#endif

namespace v8_inspector {
class V8Inspector;
}  // namespace v8_inspector

namespace v8 {

class EmbedderState;

namespace base {
class RandomNumberGenerator;
}  // namespace base

namespace bigint {
class Processor;
}

namespace debug {
class ConsoleDelegate;
class AsyncEventDelegate;
}  // namespace debug

namespace internal {

void DefaultWasmAsyncResolvePromiseCallback(
    v8::Isolate* isolate, v8::Local<v8::Context> context,
    v8::Local<v8::Promise::Resolver> resolver,
    v8::Local<v8::Value> compilation_result, WasmAsyncSuccess success);

namespace heap {
class HeapTester;
}  // namespace heap

namespace maglev {
class MaglevConcurrentDispatcher;
}  // namespace maglev

class AddressToIndexHashMap;
class AstStringConstants;
class Bootstrapper;
class BuiltinsConstantsTableBuilder;
class CancelableTaskManager;
class Logger;
class CodeTracer;
class CommonFrame;
class CompilationCache;
class CompilationStatistics;
class Counters;
class Debug;
class Deoptimizer;
class DescriptorLookupCache;
class EmbeddedFileWriterInterface;
class EternalHandles;
class GlobalHandles;
class GlobalSafepoint;
class HandleScopeImplementer;
class HeapObjectToIndexHashMap;
class HeapProfiler;
class InnerPointerToCodeCache;
class LazyCompileDispatcher;
class LocalIsolate;
class V8FileLogger;
class MaterializedObjectStore;
class Microtask;
class MicrotaskQueue;
class OptimizingCompileDispatcher;
class PersistentHandles;
class PersistentHandlesList;
class ReadOnlyArtifacts;
class RegExpStack;
class RootVisitor;
class SetupIsolateDelegate;
class Simulator;
class SnapshotData;
class StringForwardingTable;
class StringTable;
class StubCache;
class ThreadManager;
class ThreadState;
class ThreadVisitor;  // Defined in v8threads.h
class TieringManager;
class TracingCpuProfilerImpl;
class UnicodeCache;
struct ManagedPtrDestructor;

template <StateTag Tag>
class VMState;

namespace baseline {
class BaselineBatchCompiler;
}  // namespace baseline

namespace interpreter {
class Interpreter;
}  // namespace interpreter

namespace compiler {
class NodeObserver;
class PerIsolateCompilerCache;
}  // namespace compiler

namespace win64_unwindinfo {
class BuiltinUnwindInfo;
}  // namespace win64_unwindinfo

namespace metrics {
class Recorder;
}  // namespace metrics

namespace wasm {
class StackMemory;
}

#define RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate) \
  do {                                                 \
    Isolate* __isolate__ = (isolate);                  \
    DCHECK(!__isolate__->has_pending_exception());     \
    if (__isolate__->has_scheduled_exception()) {      \
      return __isolate__->PromoteScheduledException(); \
    }                                                  \
  } while (false)

#define RETURN_FAILURE_IF_SCHEDULED_EXCEPTION_DETECTOR(isolate, detector) \
  do {                                                                    \
    Isolate* __isolate__ = (isolate);                                     \
    DCHECK(!__isolate__->has_pending_exception());                        \
    if (__isolate__->has_scheduled_exception()) {                         \
      detector.AcceptSideEffects();                                       \
      return __isolate__->PromoteScheduledException();                    \
    }                                                                     \
  } while (false)

// Macros for MaybeHandle.

#define RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, value) \
  do {                                                      \
    Isolate* __isolate__ = (isolate);                       \
    DCHECK(!__isolate__->has_pending_exception());          \
    if (__isolate__->has_scheduled_exception()) {           \
      __isolate__->PromoteScheduledException();             \
      return value;                                         \
    }                                                       \
  } while (false)

#define RETURN_VALUE_IF_SCHEDULED_EXCEPTION_DETECTOR(isolate, detector, value) \
  RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate,                                 \
                                      (detector.AcceptSideEffects(), value))

#define RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, T) \
  RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, MaybeHandle<T>())

#define ASSIGN_RETURN_ON_SCHEDULED_EXCEPTION_VALUE(isolate, dst, call, value) \
  do {                                                                        \
    Isolate* __isolate__ = (isolate);                                         \
    if (!(call).ToLocal(&dst)) {                                              \
      DCHECK(__isolate__->has_scheduled_exception());                         \
      __isolate__->PromoteScheduledException();                               \
      return value;                                                           \
    }                                                                         \
  } while (false)

#define RETURN_ON_SCHEDULED_EXCEPTION_VALUE(isolate, call, value) \
  do {                                                            \
    Isolate* __isolate__ = (isolate);                             \
    if ((call).IsNothing()) {                                     \
      DCHECK(__isolate__->has_scheduled_exception());             \
      __isolate__->PromoteScheduledException();                   \
      return value;                                               \
    }                                                             \
  } while (false)

/**
 * RETURN_RESULT_OR_FAILURE is used in functions with return type Object (such
 * as "RUNTIME_FUNCTION(...) {...}" or "BUILTIN(...) {...}" ) to return either
 * the contents of a MaybeHandle<X>, or the "exception" sentinel value.
 * Example usage:
 *
 * RUNTIME_FUNCTION(Runtime_Func) {
 *   ...
 *   RETURN_RESULT_OR_FAILURE(
 *       isolate,
 *       FunctionWithReturnTypeMaybeHandleX(...));
 * }
 *
 * If inside a function with return type MaybeHandle<X> use RETURN_ON_EXCEPTION
 * instead.
 * If inside a function with return type Handle<X>, or Maybe<X> use
 * RETURN_ON_EXCEPTION_VALUE instead.
 */
#define RETURN_RESULT_OR_FAILURE(isolate, call)      \
  do {                                               \
    Handle<Object> __result__;                       \
    Isolate* __isolate__ = (isolate);                \
    if (!(call).ToHandle(&__result__)) {             \
      DCHECK(__isolate__->has_pending_exception());  \
      return ReadOnlyRoots(__isolate__).exception(); \
    }                                                \
    DCHECK(!__isolate__->has_pending_exception());   \
    return *__result__;                              \
  } while (false)

#define ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, dst, call, value) \
  do {                                                              \
    if (!(call).ToHandle(&dst)) {                                   \
      DCHECK((isolate)->has_pending_exception());                   \
      return value;                                                 \
    }                                                               \
  } while (false)

#define ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, dst, call)                \
  do {                                                                        \
    auto* __isolate__ = (isolate);                                            \
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(__isolate__, dst, call,                  \
                                     ReadOnlyRoots(__isolate__).exception()); \
  } while (false)

#define ASSIGN_RETURN_ON_EXCEPTION(isolate, dst, call, T) \
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, dst, call, MaybeHandle<T>())

#define THROW_NEW_ERROR(isolate, call, T)                                \
  do {                                                                   \
    auto* __isolate__ = (isolate);                                       \
    return __isolate__->template Throw<T>(__isolate__->factory()->call); \
  } while (false)

#define THROW_NEW_ERROR_RETURN_FAILURE(isolate, call)         \
  do {                                                        \
    auto* __isolate__ = (isolate);                            \
    return __isolate__->Throw(*__isolate__->factory()->call); \
  } while (false)

#define THROW_NEW_ERROR_RETURN_VALUE(isolate, call, value) \
  do {                                                     \
    auto* __isolate__ = (isolate);                         \
    __isolate__->Throw(*__isolate__->factory()->call);     \
    return value;                                          \
  } while (false)

/**
 * RETURN_ON_EXCEPTION_VALUE conditionally returns the given value when the
 * given MaybeHandle is empty. It is typically used in functions with return
 * type Maybe<X> or Handle<X>. Example usage:
 *
 * Handle<X> Func() {
 *   ...
 *   RETURN_ON_EXCEPTION_VALUE(
 *       isolate,
 *       FunctionWithReturnTypeMaybeHandleX(...),
 *       Handle<X>());
 *   // code to handle non exception
 *   ...
 * }
 *
 * Maybe<bool> Func() {
 *   ..
 *   RETURN_ON_EXCEPTION_VALUE(
 *       isolate,
 *       FunctionWithReturnTypeMaybeHandleX(...),
 *       Nothing<bool>);
 *   // code to handle non exception
 *   return Just(true);
 * }
 *
 * If inside a function with return type MaybeHandle<X>, use RETURN_ON_EXCEPTION
 * instead.
 * If inside a function with return type Object, use
 * RETURN_FAILURE_ON_EXCEPTION instead.
 */
#define RETURN_ON_EXCEPTION_VALUE(isolate, call, value) \
  do {                                                  \
    if ((call).is_null()) {                             \
      DCHECK((isolate)->has_pending_exception());       \
      return value;                                     \
    }                                                   \
  } while (false)

/**
 * RETURN_FAILURE_ON_EXCEPTION conditionally returns the "exception" sentinel if
 * the given MaybeHandle is empty; so it can only be used in functions with
 * return type Object, such as RUNTIME_FUNCTION(...) {...} or BUILTIN(...)
 * {...}. Example usage:
 *
 * RUNTIME_FUNCTION(Runtime_Func) {
 *   ...
 *   RETURN_FAILURE_ON_EXCEPTION(
 *       isolate,
 *       FunctionWithReturnTypeMaybeHandleX(...));
 *   // code to handle non exception
 *   ...
 * }
 *
 * If inside a function with return type MaybeHandle<X>, use RETURN_ON_EXCEPTION
 * instead.
 * If inside a function with return type Maybe<X> or Handle<X>, use
 * RETURN_ON_EXCEPTION_VALUE instead.
 */
#define RETURN_FAILURE_ON_EXCEPTION(isolate, call)                     \
  do {                                                                 \
    Isolate* __isolate__ = (isolate);                                  \
    RETURN_ON_EXCEPTION_VALUE(__isolate__, call,                       \
                              ReadOnlyRoots(__isolate__).exception()); \
  } while (false);

/**
 * RETURN_ON_EXCEPTION conditionally returns an empty MaybeHandle<T> if the
 * given MaybeHandle is empty. Use it to return immediately from a function with
 * return type MaybeHandle when an exception was thrown. Example usage:
 *
 * MaybeHandle<X> Func() {
 *   ...
 *   RETURN_ON_EXCEPTION(
 *       isolate,
 *       FunctionWithReturnTypeMaybeHandleY(...),
 *       X);
 *   // code to handle non exception
 *   ...
 * }
 *
 * If inside a function with return type Object, use
 * RETURN_FAILURE_ON_EXCEPTION instead.
 * If inside a function with return type
 * Maybe<X> or Handle<X>, use RETURN_ON_EXCEPTION_VALUE instead.
 */
#define RETURN_ON_EXCEPTION(isolate, call, T) \
  RETURN_ON_EXCEPTION_VALUE(isolate, call, MaybeHandle<T>())

#define RETURN_FAILURE(isolate, should_throw, call) \
  do {                                              \
    if ((should_throw) == kDontThrow) {             \
      return Just(false);                           \
    } else {                                        \
      isolate->Throw(*isolate->factory()->call);    \
      return Nothing<bool>();                       \
    }                                               \
  } while (false)

#define MAYBE_RETURN(call, value)         \
  do {                                    \
    if ((call).IsNothing()) return value; \
  } while (false)

#define MAYBE_RETURN_NULL(call) MAYBE_RETURN(call, MaybeHandle<Object>())

#define MAYBE_RETURN_ON_EXCEPTION_VALUE(isolate, call, value) \
  do {                                                        \
    if ((call).IsNothing()) {                                 \
      DCHECK((isolate)->has_pending_exception());             \
      return value;                                           \
    }                                                         \
  } while (false)

#define MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, dst, call, value) \
  do {                                                                    \
    if (!(call).To(&dst)) {                                               \
      DCHECK((isolate)->has_pending_exception());                         \
      return value;                                                       \
    }                                                                     \
  } while (false)

#define MAYBE_ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, dst, call) \
  do {                                                               \
    Isolate* __isolate__ = (isolate);                                \
    if (!(call).To(&dst)) {                                          \
      DCHECK(__isolate__->has_pending_exception());                  \
      return ReadOnlyRoots(__isolate__).exception();                 \
    }                                                                \
  } while (false)

#define FOR_WITH_HANDLE_SCOPE(isolate, loop_var_type, init, loop_var,      \
                              limit_check, increment, body)                \
  do {                                                                     \
    loop_var_type init;                                                    \
    loop_var_type for_with_handle_limit = loop_var;                        \
    Isolate* for_with_handle_isolate = isolate;                            \
    while (limit_check) {                                                  \
      for_with_handle_limit += 1024;                                       \
      HandleScope loop_scope(for_with_handle_isolate);                     \
      for (; limit_check && loop_var < for_with_handle_limit; increment) { \
        body                                                               \
      }                                                                    \
    }                                                                      \
  } while (false)

#define WHILE_WITH_HANDLE_SCOPE(isolate, limit_check, body)                  \
  do {                                                                       \
    Isolate* for_with_handle_isolate = isolate;                              \
    while (limit_check) {                                                    \
      HandleScope loop_scope(for_with_handle_isolate);                       \
      for (int for_with_handle_it = 0;                                       \
           limit_check && for_with_handle_it < 1024; ++for_with_handle_it) { \
        body                                                                 \
      }                                                                      \
    }                                                                        \
  } while (false)

#define FIELD_ACCESSOR(type, name)                \
  inline void set_##name(type v) { name##_ = v; } \
  inline type name() const { return name##_; }

// Controls for manual embedded blob lifecycle management, used by tests and
// mksnapshot.
V8_EXPORT_PRIVATE void DisableEmbeddedBlobRefcounting();
V8_EXPORT_PRIVATE void FreeCurrentEmbeddedBlob();

#ifdef DEBUG

#define ISOLATE_INIT_DEBUG_ARRAY_LIST(V)               \
  V(CommentStatistic, paged_space_comments_statistics, \
    CommentStatistic::kMaxComments + 1)                \
  V(int, code_kind_statistics, kCodeKindCount)
#else

#define ISOLATE_INIT_DEBUG_ARRAY_LIST(V)

#endif

#define ISOLATE_INIT_ARRAY_LIST(V)                                             \
  /* SerializerDeserializer state. */                                          \
  V(int32_t, jsregexp_static_offsets_vector, kJSRegexpStaticOffsetsVectorSize) \
  V(int, bad_char_shift_table, kUC16AlphabetSize)                              \
  V(int, good_suffix_shift_table, (kBMMaxShift + 1))                           \
  V(int, suffix_table, (kBMMaxShift + 1))                                      \
  ISOLATE_INIT_DEBUG_ARRAY_LIST(V)

using DebugObjectCache = std::vector<Handle<HeapObject>>;

#define ISOLATE_INIT_LIST(V)                                                  \
  /* Assembler state. */                                                      \
  V(FatalErrorCallback, exception_behavior, nullptr)                          \
  V(OOMErrorCallback, oom_behavior, nullptr)                                  \
  V(LogEventCallback, event_logger, nullptr)                                  \
  V(AllowCodeGenerationFromStringsCallback, allow_code_gen_callback, nullptr) \
  V(ModifyCodeGenerationFromStringsCallback, modify_code_gen_callback,        \
    nullptr)                                                                  \
  V(ModifyCodeGenerationFromStringsCallback2, modify_code_gen_callback2,      \
    nullptr)                                                                  \
  V(AllowWasmCodeGenerationCallback, allow_wasm_code_gen_callback, nullptr)   \
  V(ExtensionCallback, wasm_module_callback, &NoExtension)                    \
  V(ExtensionCallback, wasm_instance_callback, &NoExtension)                  \
  V(SharedArrayBufferConstructorEnabledCallback,                              \
    sharedarraybuffer_constructor_enabled_callback, nullptr)                  \
  V(WasmStreamingCallback, wasm_streaming_callback, nullptr)                  \
  V(WasmAsyncResolvePromiseCallback, wasm_async_resolve_promise_callback,     \
    DefaultWasmAsyncResolvePromiseCallback)                                   \
  V(WasmLoadSourceMapCallback, wasm_load_source_map_callback, nullptr)        \
  V(WasmSimdEnabledCallback, wasm_simd_enabled_callback, nullptr)             \
  V(WasmExceptionsEnabledCallback, wasm_exceptions_enabled_callback, nullptr) \
  /* State for Relocatable. */                                                \
  V(Relocatable*, relocatable_top, nullptr)                                   \
  V(DebugObjectCache*, string_stream_debug_object_cache, nullptr)             \
  V(Object, string_stream_current_security_token, Object())                   \
  V(const intptr_t*, api_external_references, nullptr)                        \
  V(AddressToIndexHashMap*, external_reference_map, nullptr)                  \
  V(HeapObjectToIndexHashMap*, root_index_map, nullptr)                       \
  V(MicrotaskQueue*, default_microtask_queue, nullptr)                        \
  V(CodeTracer*, code_tracer, nullptr)                                        \
  V(PromiseRejectCallback, promise_reject_callback, nullptr)                  \
  V(const v8::StartupData*, snapshot_blob, nullptr)                           \
  V(int, code_and_metadata_size, 0)                                           \
  V(int, bytecode_and_metadata_size, 0)                                       \
  V(int, external_script_source_size, 0)                                      \
  /* Number of CPU profilers running on the isolate. */                       \
  V(size_t, num_cpu_profilers, 0)                                             \
  /* true if a trace is being formatted through Error.prepareStackTrace. */   \
  V(bool, formatting_stack_trace, false)                                      \
  /* Perform side effect checks on function call and API callbacks. */        \
  V(DebugInfo::ExecutionMode, debug_execution_mode, DebugInfo::kBreakpoints)  \
  V(bool, disable_bytecode_flushing, false)                                   \
  V(int, last_console_context_id, 0)                                          \
  V(v8_inspector::V8Inspector*, inspector, nullptr)                           \
  V(bool, next_v8_call_is_safe_for_termination, false)                        \
  V(bool, only_terminate_in_safe_scope, false)                                \
  V(int, embedder_wrapper_type_index, -1)                                     \
  V(int, embedder_wrapper_object_index, -1)                                   \
  V(compiler::NodeObserver*, node_observer, nullptr)                          \
  /* Used in combination with --script-run-delay-once */                      \
  V(bool, did_run_script_delay, false)                                        \
  V(bool, javascript_execution_assert, true)                                  \
  V(bool, javascript_execution_throws, true)                                  \
  V(bool, javascript_execution_dump, true)                                    \
  V(uint32_t, javascript_execution_counter, 0)                                \
  V(bool, deoptimization_assert, true)                                        \
  V(bool, compilation_assert, true)                                           \
  V(bool, no_exception_assert, true)

#define THREAD_LOCAL_TOP_ACCESSOR(type, name)                         \
  inline void set_##name(type v) { thread_local_top()->name##_ = v; } \
  inline type name() const { return thread_local_top()->name##_; }

#define THREAD_LOCAL_TOP_ADDRESS(type, name) \
  type* name##_address() { return &thread_local_top()->name##_; }

// HiddenFactory exists so Isolate can privately inherit from it without making
// Factory's members available to Isolate directly.
class V8_EXPORT_PRIVATE HiddenFactory : private Factory {};

class V8_EXPORT_PRIVATE Isolate final : private HiddenFactory {
  // These forward declarations are required to make the friend declarations in
  // PerIsolateThreadData work on some older versions of gcc.
  class ThreadDataTable;
  class EntryStackItem;

 public:
  Isolate(const Isolate&) = delete;
  Isolate& operator=(const Isolate&) = delete;

  using HandleScopeType = HandleScope;
  void* operator new(size_t) = delete;
  void operator delete(void*) = delete;

  // A thread has a PerIsolateThreadData instance for each isolate that it has
  // entered. That instance is allocated when the isolate is initially entered
  // and reused on subsequent entries.
  class PerIsolateThreadData {
   public:
    PerIsolateThreadData(Isolate* isolate, ThreadId thread_id)
        : isolate_(isolate),
          thread_id_(thread_id),
          stack_limit_(0),
          thread_state_(nullptr)
#if USE_SIMULATOR
          ,
          simulator_(nullptr)
#endif
    {
    }
    ~PerIsolateThreadData();
    PerIsolateThreadData(const PerIsolateThreadData&) = delete;
    PerIsolateThreadData& operator=(const PerIsolateThreadData&) = delete;
    Isolate* isolate() const { return isolate_; }
    ThreadId thread_id() const { return thread_id_; }

    FIELD_ACCESSOR(uintptr_t, stack_limit)
    FIELD_ACCESSOR(ThreadState*, thread_state)

#if USE_SIMULATOR
    FIELD_ACCESSOR(Simulator*, simulator)
#endif

    bool Matches(Isolate* isolate, ThreadId thread_id) const {
      return isolate_ == isolate && thread_id_ == thread_id;
    }

   private:
    Isolate* isolate_;
    ThreadId thread_id_;
    uintptr_t stack_limit_;
    ThreadState* thread_state_;

#if USE_SIMULATOR
    Simulator* simulator_;
#endif

    friend class Isolate;
    friend class ThreadDataTable;
    friend class EntryStackItem;
  };

  static void InitializeOncePerProcess();

  // Creates Isolate object. Must be used instead of constructing Isolate with
  // new operator.
  static Isolate* New();

  // Deletes Isolate object. Must be used instead of delete operator.
  // Destroys the non-default isolates.
  // Sets default isolate into "has_been_disposed" state rather then destroying,
  // for legacy API reasons.
  static void Delete(Isolate* isolate);

  void SetUpFromReadOnlyArtifacts(std::shared_ptr<ReadOnlyArtifacts> artifacts,
                                  ReadOnlyHeap* ro_heap);
  void set_read_only_heap(ReadOnlyHeap* ro_heap) { read_only_heap_ = ro_heap; }

  // Page allocator that must be used for allocating V8 heap pages.
  v8::PageAllocator* page_allocator() const;

  // Returns the PerIsolateThreadData for the current thread (or nullptr if one
  // is not currently set).
  V8_INLINE static PerIsolateThreadData* CurrentPerIsolateThreadData();

  // Returns the isolate inside which the current thread is running or nullptr.
  V8_INLINE static Isolate* TryGetCurrent();

  // Returns the isolate inside which the current thread is running.
  V8_INLINE static Isolate* Current();

  inline bool IsCurrent() const;

  // Usually called by Init(), but can be called early e.g. to allow
  // testing components that require logging but not the whole
  // isolate.
  //
  // Safe to call more than once.
  void InitializeLoggingAndCounters();
  bool InitializeCounters();  // Returns false if already initialized.

  bool InitWithoutSnapshot();
  bool InitWithSnapshot(SnapshotData* startup_snapshot_data,
                        SnapshotData* read_only_snapshot_data,
                        SnapshotData* shared_heap_snapshot_data,
                        bool can_rehash);

  // True if at least one thread Enter'ed this isolate.
  bool IsInUse() { return entry_stack_ != nullptr; }

  void ReleaseSharedPtrs();

  void ClearSerializerData();

  void UpdateLogObjectRelocation();

  // Initializes the current thread to run this Isolate.
  // Not thread-safe. Multiple threads should not Enter/Exit the same isolate
  // at the same time, this should be prevented using external locking.
  void Enter();

  // Exits the current thread. The previosuly entered Isolate is restored
  // for the thread.
  // Not thread-safe. Multiple threads should not Enter/Exit the same isolate
  // at the same time, this should be prevented using external locking.
  void Exit();

  // Find the PerThread for this particular (isolate, thread) combination.
  // If one does not yet exist, allocate a new one.
  PerIsolateThreadData* FindOrAllocatePerThreadDataForThisThread();

  // Find the PerThread for this particular (isolate, thread) combination
  // If one does not yet exist, return null.
  PerIsolateThreadData* FindPerThreadDataForThisThread();

  // Find the PerThread for given (isolate, thread) combination
  // If one does not yet exist, return null.
  PerIsolateThreadData* FindPerThreadDataForThread(ThreadId thread_id);

  // Discard the PerThread for this particular (isolate, thread) combination
  // If one does not yet exist, no-op.
  void DiscardPerThreadDataForThisThread();

  // Mutex for serializing access to break control structures.
  base::RecursiveMutex* break_access() { return &break_access_; }

  // Shared mutex for allowing thread-safe concurrent reads of FeedbackVectors.
  base::SharedMutex* feedback_vector_access() {
    return &feedback_vector_access_;
  }

  // Shared mutex for allowing thread-safe concurrent reads of
  // InternalizedStrings.
  base::SharedMutex* internalized_string_access() {
    return &internalized_string_access_;
  }

  // Shared mutex for allowing thread-safe concurrent reads of TransitionArrays
  // of kind kFullTransitionArray.
  base::SharedMutex* full_transition_array_access() {
    return &full_transition_array_access_;
  }

  // Shared mutex for allowing thread-safe concurrent reads of
  // SharedFunctionInfos.
  base::SharedMutex* shared_function_info_access() {
    return &shared_function_info_access_;
  }

  // Protects (most) map update operations, see also MapUpdater.
  base::SharedMutex* map_updater_access() { return &map_updater_access_; }

  // Protects JSObject boilerplate migrations (i.e. calls to MigrateInstance on
  // boilerplate objects; elements kind transitions are *not* protected).
  // Note this lock interacts with `map_updater_access` as follows
  //
  // - boilerplate migrations may trigger map updates.
  // - if so, `boilerplate_migration_access` is locked before
  //   `map_updater_access`.
  // - backgrounds threads must use the same lock order to avoid deadlocks.
  base::SharedMutex* boilerplate_migration_access() {
    return &boilerplate_migration_access_;
  }

  // The isolate's string table.
  StringTable* string_table() const { return string_table_.get(); }
  StringForwardingTable* string_forwarding_table() const {
    return string_forwarding_table_.get();
  }

  Address get_address_from_id(IsolateAddressId id);

  // Access to top context (where the current function object was created).
  Context context() const { return thread_local_top()->context_; }
  inline void set_context(Context context);
  Context* context_address() { return &thread_local_top()->context_; }

  // Access to current thread id.
  inline void set_thread_id(ThreadId id) {
    thread_local_top()->thread_id_.store(id, std::memory_order_relaxed);
  }
  inline ThreadId thread_id() const {
    return thread_local_top()->thread_id_.load(std::memory_order_relaxed);
  }

  void InstallConditionalFeatures(Handle<Context> context);

  bool IsSharedArrayBufferConstructorEnabled(Handle<Context> context);

  bool IsWasmSimdEnabled(Handle<Context> context);
  bool AreWasmExceptionsEnabled(Handle<Context> context);

  THREAD_LOCAL_TOP_ADDRESS(Context, pending_handler_context)
  THREAD_LOCAL_TOP_ADDRESS(Address, pending_handler_entrypoint)
  THREAD_LOCAL_TOP_ADDRESS(Address, pending_handler_constant_pool)
  THREAD_LOCAL_TOP_ADDRESS(Address, pending_handler_fp)
  THREAD_LOCAL_TOP_ADDRESS(Address, pending_handler_sp)
  THREAD_LOCAL_TOP_ADDRESS(uintptr_t, num_frames_above_pending_handler)

  THREAD_LOCAL_TOP_ACCESSOR(bool, external_caught_exception)

  v8::TryCatch* try_catch_handler() {
    return thread_local_top()->try_catch_handler_;
  }

  THREAD_LOCAL_TOP_ADDRESS(bool, external_caught_exception)

  // Interface to pending exception.
  THREAD_LOCAL_TOP_ADDRESS(Object, pending_exception)
  inline Object pending_exception();
  inline void set_pending_exception(Object exception_obj);
  inline void clear_pending_exception();
  inline bool has_pending_exception();

  THREAD_LOCAL_TOP_ADDRESS(Object, pending_message)
  inline void clear_pending_message();
  inline Object pending_message();
  inline bool has_pending_message();
  inline void set_pending_message(Object message_obj);

  THREAD_LOCAL_TOP_ADDRESS(Object, scheduled_exception)
  inline Object scheduled_exception();
  inline bool has_scheduled_exception();
  inline void clear_scheduled_exception();
  inline void set_scheduled_exception(Object exception);

#ifdef DEBUG
  inline Object VerifyBuiltinsResult(Object result);
  inline ObjectPair VerifyBuiltinsResult(ObjectPair pair);
#endif

  enum class ExceptionHandlerType {
    kJavaScriptHandler,
    kExternalTryCatch,
    kNone
  };

  ExceptionHandlerType TopExceptionHandlerType(Object exception);

  inline bool is_catchable_by_javascript(Object exception);
  inline bool is_catchable_by_wasm(Object exception);
  inline bool is_execution_terminating();
  inline bool is_execution_termination_pending();

  // JS execution stack (see frames.h).
  static Address c_entry_fp(ThreadLocalTop* thread) {
    return thread->c_entry_fp_;
  }
  static Address handler(ThreadLocalTop* thread) { return thread->handler_; }
  Address c_function() { return thread_local_top()->c_function_; }

  inline Address* c_entry_fp_address() {
    return &thread_local_top()->c_entry_fp_;
  }
  static uint32_t c_entry_fp_offset() {
    return static_cast<uint32_t>(
        OFFSET_OF(Isolate, thread_local_top()->c_entry_fp_) -
        isolate_root_bias());
  }
  inline Address* handler_address() { return &thread_local_top()->handler_; }
  inline Address* c_function_address() {
    return &thread_local_top()->c_function_;
  }

#if defined(DEBUG) || defined(VERIFY_HEAP)
  // Count the number of active deserializers, so that the heap verifier knows
  // whether there is currently an active deserialization happening.
  //
  // This is needed as the verifier currently doesn't support verifying objects
  // which are partially deserialized.
  //
  // TODO(leszeks): Make the verifier a bit more deserialization compatible.
  void RegisterDeserializerStarted() { ++num_active_deserializers_; }
  void RegisterDeserializerFinished() {
    CHECK_GE(--num_active_deserializers_, 0);
  }
  bool has_active_deserializer() const {
    return num_active_deserializers_.load(std::memory_order_acquire) > 0;
  }
#else
  void RegisterDeserializerStarted() {}
  void RegisterDeserializerFinished() {}
  bool has_active_deserializer() const { UNREACHABLE(); }
#endif

  // Bottom JS entry.
  Address js_entry_sp() { return thread_local_top()->js_entry_sp_; }
  inline Address* js_entry_sp_address() {
    return &thread_local_top()->js_entry_sp_;
  }

  std::vector<MemoryRange>* GetCodePages() const;

  void SetCodePages(std::vector<MemoryRange>* new_code_pages);

  // Returns the global object of the current context. It could be
  // a builtin object, or a JS global object.
  inline Handle<JSGlobalObject> global_object();

  // Returns the global proxy object of the current context.
  inline Handle<JSGlobalProxy> global_proxy();

  static int ArchiveSpacePerThread() { return sizeof(ThreadLocalTop); }
  void FreeThreadResources() { thread_local_top()->Free(); }

  // This method is called by the api after operations that may throw
  // exceptions.  If an exception was thrown and not handled by an external
  // handler the exception is scheduled to be rethrown when we return to running
  // JavaScript code.  If an exception is scheduled true is returned.
  bool OptionalRescheduleException(bool clear_exception);

  // Push and pop a promise and the current try-catch handler.
  void PushPromise(Handle<JSObject> promise);
  void PopPromise();
  bool IsPromiseStackEmpty() const;

  // Return the relevant Promise that a throw/rejection pertains to, based
  // on the contents of the Promise stack
  Handle<Object> GetPromiseOnStackOnThrow();

  // Heuristically guess whether a Promise is handled by user catch handler
  bool PromiseHasUserDefinedRejectHandler(Handle<JSPromise> promise);

  class V8_NODISCARD ExceptionScope {
   public:
    // Scope currently can only be used for regular exceptions,
    // not termination exception.
    inline explicit ExceptionScope(Isolate* isolate);
    inline ~ExceptionScope();

   private:
    Isolate* isolate_;
    Handle<Object> pending_exception_;
  };

  void SetCaptureStackTraceForUncaughtExceptions(
      bool capture, int frame_limit, StackTrace::StackTraceOptions options);
  bool get_capture_stack_trace_for_uncaught_exceptions() const;

  void SetAbortOnUncaughtExceptionCallback(
      v8::Isolate::AbortOnUncaughtExceptionCallback callback);

  enum PrintStackMode { kPrintStackConcise, kPrintStackVerbose };
  void PrintCurrentStackTrace(std::ostream& out);
  void PrintStack(StringStream* accumulator,
                  PrintStackMode mode = kPrintStackVerbose);
  void PrintStack(FILE* out, PrintStackMode mode = kPrintStackVerbose);
  Handle<String> StackTraceString();
  // Stores a stack trace in a stack-allocated temporary buffer which will
  // end up in the minidump for debugging purposes.
  V8_NOINLINE void PushStackTraceAndDie(void* ptr1 = nullptr,
                                        void* ptr2 = nullptr,
                                        void* ptr3 = nullptr,
                                        void* ptr4 = nullptr);
  // Similar to the above but without collecting the stack trace.
  V8_NOINLINE void PushParamsAndDie(void* ptr1 = nullptr, void* ptr2 = nullptr,
                                    void* ptr3 = nullptr, void* ptr4 = nullptr,
                                    void* ptr5 = nullptr, void* ptr6 = nullptr);
  Handle<FixedArray> CaptureDetailedStackTrace(
      int limit, StackTrace::StackTraceOptions options);
  MaybeHandle<JSObject> CaptureAndSetErrorStack(Handle<JSObject> error_object,
                                                FrameSkipMode mode,
                                                Handle<Object> caller);
  Handle<FixedArray> GetDetailedStackTrace(Handle<JSReceiver> error_object);
  Handle<FixedArray> GetSimpleStackTrace(Handle<JSReceiver> error_object);
  // Walks the JS stack to find the first frame with a script name or
  // source URL. The inspected frames are the same as for the detailed stack
  // trace.
  Handle<String> CurrentScriptNameOrSourceURL();

  Address GetAbstractPC(int* line, int* column);

  // Returns if the given context may access the given global object. If
  // the result is false, the pending exception is guaranteed to be
  // set.
  bool MayAccess(Handle<Context> accessing_context, Handle<JSObject> receiver);

  void SetFailedAccessCheckCallback(v8::FailedAccessCheckCallback callback);
  void ReportFailedAccessCheck(Handle<JSObject> receiver);

  // Exception throwing support. The caller should use the result
  // of Throw() as its return value.
  Object Throw(Object exception) { return ThrowInternal(exception, nullptr); }
  Object ThrowAt(Handle<JSObject> exception, MessageLocation* location);
  Object ThrowIllegalOperation();

  template <typename T>
  V8_WARN_UNUSED_RESULT MaybeHandle<T> Throw(Handle<Object> exception) {
    Throw(*exception);
    return MaybeHandle<T>();
  }

  template <typename T>
  V8_WARN_UNUSED_RESULT MaybeHandle<T> ThrowAt(Handle<JSObject> exception,
                                               MessageLocation* location) {
    ThrowAt(exception, location);
    return MaybeHandle<T>();
  }

  void FatalProcessOutOfHeapMemory(const char* location) {
    heap()->FatalProcessOutOfMemory(location);
  }

  void set_console_delegate(debug::ConsoleDelegate* delegate) {
    console_delegate_ = delegate;
  }
  debug::ConsoleDelegate* console_delegate() { return console_delegate_; }

  void set_async_event_delegate(debug::AsyncEventDelegate* delegate) {
    async_event_delegate_ = delegate;
    PromiseHookStateUpdated();
  }

  // Async function and promise instrumentation support.
  void OnAsyncFunctionSuspended(Handle<JSPromise> promise,
                                Handle<JSPromise> parent);
  void OnPromiseThen(Handle<JSPromise> promise);
  void OnPromiseBefore(Handle<JSPromise> promise);
  void OnPromiseAfter(Handle<JSPromise> promise);
  void OnTerminationDuringRunMicrotasks();

  // Re-throw an exception.  This involves no error reporting since error
  // reporting was handled when the exception was thrown originally.
  // The first overload doesn't set the corresponding pending message, which
  // has to be set separately or be guaranteed to not have changed.
  Object ReThrow(Object exception);
  Object ReThrow(Object exception, Object message);

  // Find the correct handler for the current pending exception. This also
  // clears and returns the current pending exception.
  Object UnwindAndFindHandler();

  // Tries to predict whether an exception will be caught. Note that this can
  // only produce an estimate, because it is undecidable whether a finally
  // clause will consume or re-throw an exception.
  enum CatchType {
    NOT_CAUGHT,
    CAUGHT_BY_JAVASCRIPT,
    CAUGHT_BY_EXTERNAL,
    CAUGHT_BY_PROMISE,
    CAUGHT_BY_ASYNC_AWAIT
  };
  CatchType PredictExceptionCatcher();

  void ScheduleThrow(Object exception);
  // Re-set pending message, script and positions reported to the TryCatch
  // back to the TLS for re-use when rethrowing.
  void RestorePendingMessageFromTryCatch(v8::TryCatch* handler);
  // Un-schedule an exception that was caught by a TryCatch handler.
  void CancelScheduledExceptionFromTryCatch(v8::TryCatch* handler);
  void ReportPendingMessages();

  // Promote a scheduled exception to pending. Asserts has_scheduled_exception.
  Object PromoteScheduledException();

  // Attempts to compute the current source location, storing the
  // result in the target out parameter. The source location is attached to a
  // Message object as the location which should be shown to the user. It's
  // typically the top-most meaningful location on the stack.
  bool ComputeLocation(MessageLocation* target);
  bool ComputeLocationFromException(MessageLocation* target,
                                    Handle<Object> exception);
  bool ComputeLocationFromSimpleStackTrace(MessageLocation* target,
                                           Handle<Object> exception);
  bool ComputeLocationFromDetailedStackTrace(MessageLocation* target,
                                             Handle<Object> exception);

  Handle<JSMessageObject> CreateMessage(Handle<Object> exception,
                                        MessageLocation* location);
  Handle<JSMessageObject> CreateMessageOrAbort(Handle<Object> exception,
                                               MessageLocation* location);
  // Similar to Isolate::CreateMessage but DOESN'T inspect the JS stack and
  // only looks at the "detailed stack trace" as the "simple stack trace" might
  // have already been stringified.
  Handle<JSMessageObject> CreateMessageFromException(Handle<Object> exception);

  // Out of resource exception helpers.
  Object StackOverflow();
  Object TerminateExecution();
  void CancelTerminateExecution();

  void RequestInterrupt(InterruptCallback callback, void* data);
  void InvokeApiInterruptCallbacks();

  // Administration
  void Iterate(RootVisitor* v);
  void Iterate(RootVisitor* v, ThreadLocalTop* t);
  char* Iterate(RootVisitor* v, char* t);
  void IterateThread(ThreadVisitor* v, char* t);

  // Returns the current native context.
  inline Handle<NativeContext> native_context();
  inline NativeContext raw_native_context();

  Handle<Context> GetIncumbentContext();

  void RegisterTryCatchHandler(v8::TryCatch* that);
  void UnregisterTryCatchHandler(v8::TryCatch* that);

  char* ArchiveThread(char* to);
  char* RestoreThread(char* from);

  static const int kUC16AlphabetSize = 256;  // See StringSearchBase.
  static const int kBMMaxShift = 250;        // See StringSearchBase.

  // Accessors.
#define GLOBAL_ACCESSOR(type, name, initialvalue)                \
  inline type name() const {                                     \
    DCHECK(OFFSET_OF(Isolate, name##_) == name##_debug_offset_); \
    return name##_;                                              \
  }                                                              \
  inline void set_##name(type value) {                           \
    DCHECK(OFFSET_OF(Isolate, name##_) == name##_debug_offset_); \
    name##_ = value;                                             \
  }
  ISOLATE_INIT_LIST(GLOBAL_ACCESSOR)
#undef GLOBAL_ACCESSOR

  void SetDetailedSourcePositionsForProfiling(bool value) {
    if (value) {
      CollectSourcePositionsForAllBytecodeArrays();
    }
    detailed_source_positions_for_profiling_ = value;
  }

  bool detailed_source_positions_for_profiling() const {
    return detailed_source_positions_for_profiling_;
  }

#define GLOBAL_ARRAY_ACCESSOR(type, name, length)                \
  inline type* name() {                                          \
    DCHECK(OFFSET_OF(Isolate, name##_) == name##_debug_offset_); \
    return &(name##_)[0];                                        \
  }
  ISOLATE_INIT_ARRAY_LIST(GLOBAL_ARRAY_ACCESSOR)
#undef GLOBAL_ARRAY_ACCESSOR

#define NATIVE_CONTEXT_FIELD_ACCESSOR(index, type, name) \
  inline Handle<type> name();                            \
  inline bool is_##name(type value);
  NATIVE_CONTEXT_FIELDS(NATIVE_CONTEXT_FIELD_ACCESSOR)
#undef NATIVE_CONTEXT_FIELD_ACCESSOR

  Bootstrapper* bootstrapper() { return bootstrapper_; }
  // Use for updating counters on a foreground thread.
  Counters* counters() { return async_counters().get(); }
  // Use for updating counters on a background thread.
  const std::shared_ptr<Counters>& async_counters() {
    // Make sure InitializeCounters() has been called.
    DCHECK_NOT_NULL(async_counters_.get());
    return async_counters_;
  }
  const std::shared_ptr<metrics::Recorder>& metrics_recorder() {
    return metrics_recorder_;
  }
  TieringManager* tiering_manager() { return tiering_manager_; }
  CompilationCache* compilation_cache() { return compilation_cache_; }
  V8FileLogger* v8_file_logger() {
    // Call InitializeLoggingAndCounters() if logging is needed before
    // the isolate is fully initialized.
    DCHECK_NOT_NULL(v8_file_logger_);
    return v8_file_logger_;
  }
  StackGuard* stack_guard() { return isolate_data()->stack_guard(); }
  Heap* heap() { return &heap_; }
  const Heap* heap() const { return &heap_; }
  ReadOnlyHeap* read_only_heap() const { return read_only_heap_; }
  static Isolate* FromHeap(const Heap* heap) {
    return reinterpret_cast<Isolate*>(reinterpret_cast<Address>(heap) -
                                      OFFSET_OF(Isolate, heap_));
  }

  const IsolateData* isolate_data() const { return &isolate_data_; }
  IsolateData* isolate_data() { return &isolate_data_; }

  // When pointer compression is on, this is the base address of the pointer
  // compression cage, and the kPtrComprCageBaseRegister is set to this
  // value. When pointer compression is off, this is always kNullAddress.
  Address cage_base() const {
    DCHECK_IMPLIES(!COMPRESS_POINTERS_IN_ISOLATE_CAGE_BOOL &&
                       !COMPRESS_POINTERS_IN_SHARED_CAGE_BOOL,
                   isolate_data()->cage_base() == kNullAddress);
    return isolate_data()->cage_base();
  }

  // When pointer compression and external code space are on, this is the base
  // address of the cage where the code space is allocated. Otherwise, it
  // defaults to cage_base().
  Address code_cage_base() const {
#ifdef V8_EXTERNAL_CODE_SPACE
    return code_cage_base_;
#else
    return cage_base();
#endif  // V8_EXTERNAL_CODE_SPACE
  }

  // When pointer compression is on, the PtrComprCage used by this
  // Isolate. Otherwise nullptr.
  VirtualMemoryCage* GetPtrComprCage() {
    return isolate_allocator_->GetPtrComprCage();
  }
  const VirtualMemoryCage* GetPtrComprCage() const {
    return isolate_allocator_->GetPtrComprCage();
  }
  VirtualMemoryCage* GetPtrComprCodeCageForTesting();

  // Generated code can embed this address to get access to the isolate-specific
  // data (for example, roots, external references, builtins, etc.).
  // The kRootRegister is set to this value.
  Address isolate_root() const { return isolate_data()->isolate_root(); }
  static size_t isolate_root_bias() {
    return OFFSET_OF(Isolate, isolate_data_) + IsolateData::kIsolateRootBias;
  }
  static Isolate* FromRootAddress(Address isolate_root) {
    return reinterpret_cast<Isolate*>(isolate_root - isolate_root_bias());
  }

  RootsTable& roots_table() { return isolate_data()->roots(); }
  const RootsTable& roots_table() const { return isolate_data()->roots(); }

  // A sub-region of the Isolate object that has "predictable" layout which
  // depends only on the pointer size and therefore it's guaranteed that there
  // will be no compatibility issues because of different compilers used for
  // snapshot generator and actual V8 code.
  // Thus, kRootRegister may be used to address any location that falls into
  // this region.
  // See IsolateData::AssertPredictableLayout() for details.
  base::AddressRegion root_register_addressable_region() const {
    return base::AddressRegion(reinterpret_cast<Address>(&isolate_data_),
                               sizeof(IsolateData));
  }

  Object root(RootIndex index) const { return Object(roots_table()[index]); }

  Handle<Object> root_handle(RootIndex index) {
    return Handle<Object>(&roots_table()[index]);
  }

  ExternalReferenceTable* external_reference_table() {
    DCHECK(isolate_data()->external_reference_table()->is_initialized());
    return isolate_data()->external_reference_table();
  }

  Address* builtin_entry_table() { return isolate_data_.builtin_entry_table(); }
  V8_INLINE Address* builtin_table() { return isolate_data_.builtin_table(); }
  V8_INLINE Address* builtin_tier0_table() {
    return isolate_data_.builtin_tier0_table();
  }

  bool IsBuiltinTableHandleLocation(Address* handle_location);

  StubCache* load_stub_cache() const { return load_stub_cache_; }
  StubCache* store_stub_cache() const { return store_stub_cache_; }
  Deoptimizer* GetAndClearCurrentDeoptimizer() {
    Deoptimizer* result = current_deoptimizer_;
    CHECK_NOT_NULL(result);
    current_deoptimizer_ = nullptr;
    return result;
  }
  void set_current_deoptimizer(Deoptimizer* deoptimizer) {
    DCHECK_NULL(current_deoptimizer_);
    DCHECK_NOT_NULL(deoptimizer);
    current_deoptimizer_ = deoptimizer;
  }
  bool deoptimizer_lazy_throw() const { return deoptimizer_lazy_throw_; }
  void set_deoptimizer_lazy_throw(bool value) {
    deoptimizer_lazy_throw_ = value;
  }
  void InitializeThreadLocal();
  ThreadLocalTop* thread_local_top() {
    return &isolate_data_.thread_local_top_;
  }
  ThreadLocalTop const* thread_local_top() const {
    return &isolate_data_.thread_local_top_;
  }

  static uint32_t thread_in_wasm_flag_address_offset() {
    // For WebAssembly trap handlers there is a flag in thread-local storage
    // which indicates that the executing thread executes WebAssembly code. To
    // access this flag directly from generated code, we store a pointer to the
    // flag in ThreadLocalTop in thread_in_wasm_flag_address_. This function
    // here returns the offset of that member from {isolate_root()}.
    return static_cast<uint32_t>(
        OFFSET_OF(Isolate, thread_local_top()->thread_in_wasm_flag_address_) -
        isolate_root_bias());
  }

  THREAD_LOCAL_TOP_ADDRESS(Address, thread_in_wasm_flag_address)

  MaterializedObjectStore* materialized_object_store() const {
    return materialized_object_store_;
  }

  DescriptorLookupCache* descriptor_lookup_cache() const {
    return descriptor_lookup_cache_;
  }

  V8_INLINE HandleScopeData* handle_scope_data() { return &handle_scope_data_; }

  HandleScopeImplementer* handle_scope_implementer() const {
    DCHECK(handle_scope_implementer_);
    return handle_scope_implementer_;
  }

  UnicodeCache* unicode_cache() const { return unicode_cache_; }

  InnerPointerToCodeCache* inner_pointer_to_code_cache() {
    return inner_pointer_to_code_cache_;
  }

  GlobalHandles* global_handles() const { return global_handles_; }

  EternalHandles* eternal_handles() const { return eternal_handles_; }

  ThreadManager* thread_manager() const { return thread_manager_; }

  bigint::Processor* bigint_processor() { return bigint_processor_; }

#ifndef V8_INTL_SUPPORT
  unibrow::Mapping<unibrow::Ecma262UnCanonicalize>* jsregexp_uncanonicalize() {
    return &jsregexp_uncanonicalize_;
  }

  unibrow::Mapping<unibrow::CanonicalizationRange>* jsregexp_canonrange() {
    return &jsregexp_canonrange_;
  }

  unibrow::Mapping<unibrow::Ecma262Canonicalize>*
  regexp_macro_assembler_canonicalize() {
    return &regexp_macro_assembler_canonicalize_;
  }
#endif  // !V8_INTL_SUPPORT

  RuntimeState* runtime_state() { return &runtime_state_; }

  Builtins* builtins() { return &builtins_; }

  RegExpStack* regexp_stack() const { return regexp_stack_; }

  size_t total_regexp_code_generated() const {
    return total_regexp_code_generated_;
  }
  void IncreaseTotalRegexpCodeGenerated(Handle<HeapObject> code);

  std::vector<int>* regexp_indices() { return &regexp_indices_; }

  Debug* debug() const { return debug_; }

  bool is_profiling() const {
    return isolate_data_.is_profiling_.load(std::memory_order_relaxed);
  }

  void SetIsProfiling(bool enabled) {
    if (enabled) {
      CollectSourcePositionsForAllBytecodeArrays();
    }
    isolate_data_.is_profiling_.store(enabled, std::memory_order_relaxed);
    UpdateLogObjectRelocation();
  }

  Logger* logger() const { return logger_; }
  HeapProfiler* heap_profiler() const { return heap_profiler_; }

#ifdef DEBUG
  static size_t non_disposed_isolates() { return non_disposed_isolates_; }
#endif

  v8::internal::Factory* factory() {
    // Upcast to the privately inherited base-class using c-style casts to avoid
    // undefined behavior (as static_cast cannot cast across private bases).
    return (v8::internal::Factory*)this;
  }

  static const int kJSRegexpStaticOffsetsVectorSize = 128;

  THREAD_LOCAL_TOP_ACCESSOR(ExternalCallbackScope*, external_callback_scope)

  THREAD_LOCAL_TOP_ACCESSOR(StateTag, current_vm_state)
  THREAD_LOCAL_TOP_ACCESSOR(EmbedderState*, current_embedder_state)

  void SetData(uint32_t slot, void* data) {
    DCHECK_LT(slot, Internals::kNumIsolateDataSlots);
    isolate_data_.embedder_data_[slot] = data;
  }
  void* GetData(uint32_t slot) const {
    DCHECK_LT(slot, Internals::kNumIsolateDataSlots);
    return isolate_data_.embedder_data_[slot];
  }

  bool serializer_enabled() const { return serializer_enabled_; }

  void enable_serializer() { serializer_enabled_ = true; }

  bool snapshot_available() const {
    return snapshot_blob_ != nullptr && snapshot_blob_->raw_size != 0;
  }

  bool IsDead() const { return has_fatal_error_; }
  void SignalFatalError() { has_fatal_error_ = true; }

  bool use_optimizer();

  bool initialized_from_snapshot() { return initialized_from_snapshot_; }

  bool NeedsSourcePositionsForProfiling() const;

  bool NeedsDetailedOptimizedCodeLineInfo() const;

  bool is_best_effort_code_coverage() const {
    return code_coverage_mode() == debug::CoverageMode::kBestEffort;
  }

  bool is_precise_count_code_coverage() const {
    return code_coverage_mode() == debug::CoverageMode::kPreciseCount;
  }

  bool is_precise_binary_code_coverage() const {
    return code_coverage_mode() == debug::CoverageMode::kPreciseBinary;
  }

  bool is_block_count_code_coverage() const {
    return code_coverage_mode() == debug::CoverageMode::kBlockCount;
  }

  bool is_block_binary_code_coverage() const {
    return code_coverage_mode() == debug::CoverageMode::kBlockBinary;
  }

  bool is_block_code_coverage() const {
    return is_block_count_code_coverage() || is_block_binary_code_coverage();
  }

  bool is_binary_code_coverage() const {
    return is_precise_binary_code_coverage() || is_block_binary_code_coverage();
  }

  bool is_count_code_coverage() const {
    return is_precise_count_code_coverage() || is_block_count_code_coverage();
  }

  // Collect feedback vectors with data for code coverage or type profile.
  // Reset the list, when both code coverage and type profile are not
  // needed anymore. This keeps many feedback vectors alive, but code
  // coverage or type profile are used for debugging only and increase in
  // memory usage is expected.
  void SetFeedbackVectorsForProfilingTools(Object value);

  void MaybeInitializeVectorListFromHeap();

  double time_millis_since_init() const {
    return heap_.MonotonicallyIncreasingTimeInMs() - time_millis_at_init_;
  }

  DateCache* date_cache() const { return date_cache_; }

  void set_date_cache(DateCache* date_cache);

#ifdef V8_INTL_SUPPORT

  const std::string& DefaultLocale();

  void ResetDefaultLocale();

  void set_default_locale(const std::string& locale) {
    DCHECK_EQ(default_locale_.length(), 0);
    default_locale_ = locale;
  }

  enum class ICUObjectCacheType{
      kDefaultCollator, kDefaultNumberFormat, kDefaultSimpleDateFormat,
      kDefaultSimpleDateFormatForTime, kDefaultSimpleDateFormatForDate};
  static constexpr int kICUObjectCacheTypeCount = 5;

  icu::UMemory* get_cached_icu_object(ICUObjectCacheType cache_type,
                                      Handle<Object> locales);
  void set_icu_object_in_cache(ICUObjectCacheType cache_type,
                               Handle<Object> locales,
                               std::shared_ptr<icu::UMemory> obj);
  void clear_cached_icu_object(ICUObjectCacheType cache_type);
  void clear_cached_icu_objects();

#endif  // V8_INTL_SUPPORT

  enum class KnownPrototype { kNone, kObject, kArray, kString };

  KnownPrototype IsArrayOrObjectOrStringPrototype(Object object);

  // On intent to set an element in object, make sure that appropriate
  // notifications occur if the set is on the elements of the array or
  // object prototype. Also ensure that changes to prototype chain between
  // Array and Object fire notifications.
  void UpdateNoElementsProtectorOnSetElement(Handle<JSObject> object);
  void UpdateNoElementsProtectorOnSetLength(Handle<JSObject> object) {
    UpdateNoElementsProtectorOnSetElement(object);
  }
  void UpdateNoElementsProtectorOnSetPrototype(Handle<JSObject> object) {
    UpdateNoElementsProtectorOnSetElement(object);
  }
  void UpdateTypedArraySpeciesLookupChainProtectorOnSetPrototype(
      Handle<JSObject> object);
  void UpdateNoElementsProtectorOnNormalizeElements(Handle<JSObject> object) {
    UpdateNoElementsProtectorOnSetElement(object);
  }

  // Returns true if array is the initial array prototype in any native context.
  inline bool IsAnyInitialArrayPrototype(JSArray array);

  std::unique_ptr<PersistentHandles> NewPersistentHandles();

  PersistentHandlesList* persistent_handles_list() const {
    return persistent_handles_list_.get();
  }

#ifdef DEBUG
  bool IsDeferredHandle(Address* location);
#endif  // DEBUG

  baseline::BaselineBatchCompiler* baseline_batch_compiler() const {
    DCHECK_NOT_NULL(baseline_batch_compiler_);
    return baseline_batch_compiler_;
  }

#ifdef V8_ENABLE_MAGLEV
  maglev::MaglevConcurrentDispatcher* maglev_concurrent_dispatcher() {
    DCHECK_NOT_NULL(maglev_concurrent_dispatcher_);
    return maglev_concurrent_dispatcher_;
  }
#endif  // V8_ENABLE_MAGLEV

  bool concurrent_recompilation_enabled() {
    // Thread is only available with flag enabled.
    DCHECK(optimizing_compile_dispatcher_ == nullptr ||
           v8_flags.concurrent_recompilation);
    return optimizing_compile_dispatcher_ != nullptr;
  }

  OptimizingCompileDispatcher* optimizing_compile_dispatcher() {
    DCHECK_NOT_NULL(optimizing_compile_dispatcher_);
    return optimizing_compile_dispatcher_;
  }
  // Flushes all pending concurrent optimzation jobs from the optimizing
  // compile dispatcher's queue.
  void AbortConcurrentOptimization(BlockingBehavior blocking_behavior);

  int id() const { return id_; }

  bool was_locker_ever_used() const {
    return was_locker_ever_used_.load(std::memory_order_relaxed);
  }
  void set_was_locker_ever_used() {
    was_locker_ever_used_.store(true, std::memory_order_relaxed);
  }

  std::shared_ptr<CompilationStatistics> GetTurboStatistics();
  CodeTracer* GetCodeTracer();

  void DumpAndResetStats();

  void* stress_deopt_count_address() { return &stress_deopt_count_; }

  void set_force_slow_path(bool v) { force_slow_path_ = v; }
  bool force_slow_path() const { return force_slow_path_; }
  bool* force_slow_path_address() { return &force_slow_path_; }

  bool jitless() const { return jitless_; }

  DebugInfo::ExecutionMode* debug_execution_mode_address() {
    return &debug_execution_mode_;
  }

  base::RandomNumberGenerator* random_number_generator();

  base::RandomNumberGenerator* fuzzer_rng();

  // Generates a random number that is non-zero when masked
  // with the provided mask.
  int GenerateIdentityHash(uint32_t mask);

  // Given an address occupied by a live code object, return that object.
  CodeLookupResult FindCodeObject(Address a);

  int NextOptimizationId() {
    int id = next_optimization_id_++;
    if (!Smi::IsValid(next_optimization_id_)) {
      next_optimization_id_ = 0;
    }
    return id;
  }

  // https://github.com/tc39/proposal-top-level-await/pull/159
  // TODO(syg): Update to actual spec link once merged.
  //
  // According to the spec, modules that depend on async modules (i.e. modules
  // with top-level await) must be evaluated in order in which their
  // [[AsyncEvaluating]] flags were set to true. V8 tracks this global total
  // order with next_module_async_evaluating_ordinal_. Each module that sets its
  // [[AsyncEvaluating]] to true grabs the next ordinal.
  unsigned NextModuleAsyncEvaluatingOrdinal() {
    unsigned ordinal = next_module_async_evaluating_ordinal_++;
    CHECK_LT(ordinal, kMaxModuleAsyncEvaluatingOrdinal);
    return ordinal;
  }

  inline void DidFinishModuleAsyncEvaluation(unsigned ordinal);

  void AddNearHeapLimitCallback(v8::NearHeapLimitCallback, void* data);
  void RemoveNearHeapLimitCallback(v8::NearHeapLimitCallback callback,
                                   size_t heap_limit);
  void AddCallCompletedCallback(CallCompletedCallback callback);
  void RemoveCallCompletedCallback(CallCompletedCallback callback);
  void FireCallCompletedCallback(MicrotaskQueue* microtask_queue) {
    if (!thread_local_top()->CallDepthIsZero()) return;
    FireCallCompletedCallbackInternal(microtask_queue);
  }

  void AddBeforeCallEnteredCallback(BeforeCallEnteredCallback callback);
  void RemoveBeforeCallEnteredCallback(BeforeCallEnteredCallback callback);
  inline void FireBeforeCallEnteredCallback();

  void SetPromiseRejectCallback(PromiseRejectCallback callback);
  void ReportPromiseReject(Handle<JSPromise> promise, Handle<Object> value,
                           v8::PromiseRejectEvent event);

  void SetTerminationOnExternalTryCatch();

  Handle<Symbol> SymbolFor(RootIndex dictionary_index, Handle<String> name,
                           bool private_symbol);

  void SetUseCounterCallback(v8::Isolate::UseCounterCallback callback);
  void CountUsage(v8::Isolate::UseCounterFeature feature);
  void CountUsage(v8::Isolate::UseCounterFeature feature, int count);

  static std::string GetTurboCfgFileName(Isolate* isolate);

  int GetNextScriptId();

#if V8_SFI_HAS_UNIQUE_ID
  int GetNextUniqueSharedFunctionInfoId() {
    int current_id = next_unique_sfi_id_.load(std::memory_order_relaxed);
    int next_id;
    do {
      if (current_id >= Smi::kMaxValue) {
        next_id = 0;
      } else {
        next_id = current_id + 1;
      }
    } while (!next_unique_sfi_id_.compare_exchange_weak(
        current_id, next_id, std::memory_order_relaxed));
    return current_id;
  }
#endif

#ifdef V8_ENABLE_JAVASCRIPT_PROMISE_HOOKS
  void SetHasContextPromiseHooks(bool context_promise_hook) {
    promise_hook_flags_ = PromiseHookFields::HasContextPromiseHook::update(
        promise_hook_flags_, context_promise_hook);
    PromiseHookStateUpdated();
  }
#endif  // V8_ENABLE_JAVASCRIPT_PROMISE_HOOKS

  bool HasContextPromiseHooks() const {
    return PromiseHookFields::HasContextPromiseHook::decode(
        promise_hook_flags_);
  }

  Address promise_hook_flags_address() {
    return reinterpret_cast<Address>(&promise_hook_flags_);
  }

  Address promise_hook_address() {
    return reinterpret_cast<Address>(&promise_hook_);
  }

  Address async_event_delegate_address() {
    return reinterpret_cast<Address>(&async_event_delegate_);
  }

  Address javascript_execution_assert_address() {
    return reinterpret_cast<Address>(&javascript_execution_assert_);
  }

  void IncrementJavascriptExecutionCounter() {
    javascript_execution_counter_++;
  }

  Address handle_scope_implementer_address() {
    return reinterpret_cast<Address>(&handle_scope_implementer_);
  }

  void SetAtomicsWaitCallback(v8::Isolate::AtomicsWaitCallback callback,
                              void* data);
  void RunAtomicsWaitCallback(v8::Isolate::AtomicsWaitEvent event,
                              Handle<JSArrayBuffer> array_buffer,
                              size_t offset_in_bytes, int64_t value,
                              double timeout_in_ms,
                              AtomicsWaitWakeHandle* stop_handle);

  void SetPromiseHook(PromiseHook hook);
  void RunPromiseHook(PromiseHookType type, Handle<JSPromise> promise,
                      Handle<Object> parent);
  void RunAllPromiseHooks(PromiseHookType type, Handle<JSPromise> promise,
                          Handle<Object> parent);
  void UpdatePromiseHookProtector();
  void PromiseHookStateUpdated();

  void AddDetachedContext(Handle<Context> context);
  void CheckDetachedContextsAfterGC();

  // Detach the environment from its outer global object.
  void DetachGlobal(Handle<Context> env);

  std::vector<Object>* startup_object_cache() { return &startup_object_cache_; }

  // When there is a shared space (i.e. when this is a client Isolate), the
  // shared heap object cache holds objects in shared among Isolates. Otherwise
  // this object cache is per-Isolate like the startup object cache.
  std::vector<Object>* shared_heap_object_cache() {
    if (has_shared_heap()) {
      return &shared_heap_isolate()->shared_heap_object_cache_;
    }
    return &shared_heap_object_cache_;
  }

  bool IsGeneratingEmbeddedBuiltins() const {
    return builtins_constants_table_builder() != nullptr;
  }

  BuiltinsConstantsTableBuilder* builtins_constants_table_builder() const {
    return builtins_constants_table_builder_;
  }

  // Hashes bits of the Isolate that are relevant for embedded builtins. In
  // particular, the embedded blob requires builtin Code object layout and the
  // builtins constants table to remain unchanged from build-time.
  size_t HashIsolateForEmbeddedBlob();

  static const uint8_t* CurrentEmbeddedBlobCode();
  static uint32_t CurrentEmbeddedBlobCodeSize();
  static const uint8_t* CurrentEmbeddedBlobData();
  static uint32_t CurrentEmbeddedBlobDataSize();
  static bool CurrentEmbeddedBlobIsBinaryEmbedded();

  // These always return the same result as static methods above, but don't
  // access the global atomic variable (and thus *might be* slightly faster).
  const uint8_t* embedded_blob_code() const;
  uint32_t embedded_blob_code_size() const;
  const uint8_t* embedded_blob_data() const;
  uint32_t embedded_blob_data_size() const;

  // Returns true if short bultin calls optimization is enabled for the Isolate.
  bool is_short_builtin_calls_enabled() const {
    return V8_SHORT_BUILTIN_CALLS_BOOL && is_short_builtin_calls_enabled_;
  }

  // Returns a region from which it's possible to make pc-relative (short)
  // calls/jumps to embedded builtins or empty region if there's no embedded
  // blob or if pc-relative calls are not supported.
  static base::AddressRegion GetShortBuiltinsCallRegion();

  void set_array_buffer_allocator(v8::ArrayBuffer::Allocator* allocator) {
    array_buffer_allocator_ = allocator;
  }
  v8::ArrayBuffer::Allocator* array_buffer_allocator() const {
    return array_buffer_allocator_;
  }

  void set_array_buffer_allocator_shared(
      std::shared_ptr<v8::ArrayBuffer::Allocator> allocator) {
    array_buffer_allocator_shared_ = std::move(allocator);
  }
  std::shared_ptr<v8::ArrayBuffer::Allocator> array_buffer_allocator_shared()
      const {
    return array_buffer_allocator_shared_;
  }

  FutexWaitListNode* futex_wait_list_node() { return &futex_wait_list_node_; }

  CancelableTaskManager* cancelable_task_manager() {
    return cancelable_task_manager_;
  }

  const AstStringConstants* ast_string_constants() const {
    return ast_string_constants_;
  }

  interpreter::Interpreter* interpreter() const { return interpreter_; }

  compiler::PerIsolateCompilerCache* compiler_cache() const {
    return compiler_cache_;
  }
  void set_compiler_utils(compiler::PerIsolateCompilerCache* cache,
                          Zone* zone) {
    compiler_cache_ = cache;
    compiler_zone_ = zone;
  }

  AccountingAllocator* allocator() { return allocator_; }

  LazyCompileDispatcher* lazy_compile_dispatcher() const {
    return lazy_compile_dispatcher_.get();
  }

  bool IsInAnyContext(Object object, uint32_t index);

  void ClearKeptObjects();

  void SetHostImportModuleDynamicallyCallback(
      HostImportModuleDynamicallyWithImportAssertionsCallback callback);
  void SetHostImportModuleDynamicallyCallback(
      HostImportModuleDynamicallyCallback callback);
  MaybeHandle<JSPromise> RunHostImportModuleDynamicallyCallback(
      MaybeHandle<Script> maybe_referrer, Handle<Object> specifier,
      MaybeHandle<Object> maybe_import_assertions_argument);

  void SetHostInitializeImportMetaObjectCallback(
      HostInitializeImportMetaObjectCallback callback);
  MaybeHandle<JSObject> RunHostInitializeImportMetaObjectCallback(
      Handle<SourceTextModule> module);

  void SetHostCreateShadowRealmContextCallback(
      HostCreateShadowRealmContextCallback callback);
  MaybeHandle<NativeContext> RunHostCreateShadowRealmContextCallback();

  void RegisterEmbeddedFileWriter(EmbeddedFileWriterInterface* writer) {
    embedded_file_writer_ = writer;
  }

  int LookupOrAddExternallyCompiledFilename(const char* filename);
  const char* GetExternallyCompiledFilename(int index) const;
  int GetExternallyCompiledFilenameCount() const;
  // PrepareBuiltinSourcePositionMap is necessary in order to preserve the
  // builtin source positions before the corresponding code objects are
  // replaced with trampolines. Those source positions are used to
  // annotate the builtin blob with debugging information.
  void PrepareBuiltinSourcePositionMap();

  // Store the position of the labels that will be used in the list of allowed
  // return addresses.
  void PrepareBuiltinLabelInfoMap();

#if defined(V8_OS_WIN64)
  void SetBuiltinUnwindData(
      Builtin builtin,
      const win64_unwindinfo::BuiltinUnwindInfo& unwinding_info);
#endif  // V8_OS_WIN64

  void SetPrepareStackTraceCallback(PrepareStackTraceCallback callback);
  MaybeHandle<Object> RunPrepareStackTraceCallback(Handle<Context>,
                                                   Handle<JSObject> Error,
                                                   Handle<JSArray> sites);
  bool HasPrepareStackTraceCallback() const;

  void SetAddCrashKeyCallback(AddCrashKeyCallback callback);
  void AddCrashKey(CrashKeyId id, const std::string& value) {
    if (add_crash_key_callback_) {
      add_crash_key_callback_(id, value);
    }
  }

  void SetRAILMode(RAILMode rail_mode);

  RAILMode rail_mode() { return rail_mode_.load(); }

  void set_code_coverage_mode(debug::CoverageMode coverage_mode) {
    code_coverage_mode_.store(coverage_mode, std::memory_order_relaxed);
  }
  debug::CoverageMode code_coverage_mode() const {
    return code_coverage_mode_.load(std::memory_order_relaxed);
  }

  double LoadStartTimeMs();

  void UpdateLoadStartTime();

  void IsolateInForegroundNotification();

  void IsolateInBackgroundNotification();

  bool IsIsolateInBackground() { return is_isolate_in_background_; }

  void EnableMemorySavingsMode() { memory_savings_mode_active_ = true; }

  void DisableMemorySavingsMode() { memory_savings_mode_active_ = false; }

  bool IsMemorySavingsModeActive() { return memory_savings_mode_active_; }

  PRINTF_FORMAT(2, 3) void PrintWithTimestamp(const char* format, ...);

  void set_allow_atomics_wait(bool set) { allow_atomics_wait_ = set; }
  bool allow_atomics_wait() { return allow_atomics_wait_; }

  // Register a finalizer to be called at isolate teardown.
  void RegisterManagedPtrDestructor(ManagedPtrDestructor* finalizer);

  // Removes a previously-registered shared object finalizer.
  void UnregisterManagedPtrDestructor(ManagedPtrDestructor* finalizer);

  size_t elements_deletion_counter() { return elements_deletion_counter_; }
  void set_elements_deletion_counter(size_t value) {
    elements_deletion_counter_ = value;
  }

#if V8_ENABLE_WEBASSEMBLY
  void AddSharedWasmMemory(Handle<WasmMemoryObject> memory_object);
#endif  // V8_ENABLE_WEBASSEMBLY

  const v8::Context::BackupIncumbentScope* top_backup_incumbent_scope() const {
    return top_backup_incumbent_scope_;
  }
  void set_top_backup_incumbent_scope(
      const v8::Context::BackupIncumbentScope* top_backup_incumbent_scope) {
    top_backup_incumbent_scope_ = top_backup_incumbent_scope;
  }

  void SetIdle(bool is_idle);

  // Changing various modes can cause differences in generated bytecode which
  // interferes with lazy source positions, so this should be called immediately
  // before such a mode change to ensure that this cannot happen.
  void CollectSourcePositionsForAllBytecodeArrays();

  void AddCodeMemoryChunk(MemoryChunk* chunk);
  void RemoveCodeMemoryChunk(MemoryChunk* chunk);
  void AddCodeRange(Address begin, size_t length_in_bytes);

  bool RequiresCodeRange() const;

  static Address load_from_stack_count_address(const char* function_name);
  static Address store_to_stack_count_address(const char* function_name);

  v8::metrics::Recorder::ContextId GetOrRegisterRecorderContextId(
      Handle<NativeContext> context);
  MaybeLocal<v8::Context> GetContextFromRecorderContextId(
      v8::metrics::Recorder::ContextId id);

  void UpdateLongTaskStats();
  v8::metrics::LongTaskStats* GetCurrentLongTaskStats();

  LocalIsolate* main_thread_local_isolate() {
    return main_thread_local_isolate_.get();
  }

  Isolate* AsIsolate() { return this; }
  LocalIsolate* AsLocalIsolate() { return main_thread_local_isolate(); }

  LocalHeap* main_thread_local_heap();
  LocalHeap* CurrentLocalHeap();

#ifdef V8_COMPRESS_POINTERS
  ExternalPointerTable& external_pointer_table() {
    return isolate_data_.external_pointer_table_;
  }

  const ExternalPointerTable& external_pointer_table() const {
    return isolate_data_.external_pointer_table_;
  }

  Address external_pointer_table_address() {
    return reinterpret_cast<Address>(&isolate_data_.external_pointer_table_);
  }

  ExternalPointerTable& shared_external_pointer_table() {
    return *isolate_data_.shared_external_pointer_table_;
  }

  const ExternalPointerTable& shared_external_pointer_table() const {
    return *isolate_data_.shared_external_pointer_table_;
  }

  Address shared_external_pointer_table_address_address() {
    return reinterpret_cast<Address>(
        &isolate_data_.shared_external_pointer_table_);
  }

  ExternalPointerHandle* GetWaiterQueueNodeExternalPointerHandleLocation() {
    return &waiter_queue_node_external_pointer_handle_;
  }

  ExternalPointerHandle GetOrCreateWaiterQueueNodeExternalPointer();
#endif  // V8_COMPRESS_POINTERS

  struct PromiseHookFields {
    using HasContextPromiseHook = base::BitField<bool, 0, 1>;
    using HasIsolatePromiseHook = HasContextPromiseHook::Next<bool, 1>;
    using HasAsyncEventDelegate = HasIsolatePromiseHook::Next<bool, 1>;
    using IsDebugActive = HasAsyncEventDelegate::Next<bool, 1>;
  };

  bool is_shared() const { return is_shared_; }
  Isolate* shared_isolate() const {
    DCHECK(attached_to_shared_isolate_);
    return shared_isolate_;
  }

  bool is_shared_space_isolate() const { return is_shared_space_isolate_; }
  Isolate* shared_space_isolate() const {
    return shared_space_isolate_.value();
  }

  void set_shared_isolate(Isolate* shared_isolate) {
    DCHECK(shared_isolate->is_shared());
    DCHECK_NULL(shared_isolate_);
    DCHECK(!attached_to_shared_isolate_);
    DCHECK(!v8_flags.shared_space);
    shared_isolate_ = shared_isolate;
    owns_shareable_data_ = false;
  }

  // Returns true when this isolate supports allocation in shared spaces.
  bool has_shared_heap() const {
    return v8_flags.shared_space ? shared_space_isolate() : shared_isolate();
  }

  // Returns the isolate that owns the shared spaces.
  Isolate* shared_heap_isolate() const {
    DCHECK(has_shared_heap());
    Isolate* isolate =
        v8_flags.shared_space ? shared_space_isolate() : shared_isolate();
    DCHECK_NOT_NULL(isolate);
    return isolate;
  }

  bool is_shared_heap_isolate() const {
    return is_shared() || is_shared_space_isolate();
  }

  GlobalSafepoint* global_safepoint() const { return global_safepoint_.get(); }

  bool owns_shareable_data() { return owns_shareable_data_; }

  bool log_object_relocation() const { return log_object_relocation_; }

  // TODO(pthier): Unify with owns_shareable_data() once the flag
  // --shared-string-table is removed.
  bool OwnsStringTables() {
    return !v8_flags.shared_string_table || is_shared() ||
           is_shared_space_isolate();
  }

#if USE_SIMULATOR
  SimulatorData* simulator_data() { return simulator_data_; }
#endif

#ifdef V8_ENABLE_WEBASSEMBLY
  wasm::StackMemory*& wasm_stacks() { return wasm_stacks_; }
#endif

  // Access to the global "locals block list cache". Caches outer-stack
  // allocated variables per ScopeInfo for debug-evaluate.
  // We also store a strong reference to the outer ScopeInfo to keep all
  // blocklists along a scope chain alive.
  void LocalsBlockListCacheSet(Handle<ScopeInfo> scope_info,
                               Handle<ScopeInfo> outer_scope_info,
                               Handle<StringSet> locals_blocklist);
  // Returns either `TheHole` or `StringSet`.
  Object LocalsBlockListCacheGet(Handle<ScopeInfo> scope_info);

 private:
  explicit Isolate(std::unique_ptr<IsolateAllocator> isolate_allocator,
                   bool is_shared);
  ~Isolate();

  static Isolate* Allocate(bool is_shared);

  bool Init(SnapshotData* startup_snapshot_data,
            SnapshotData* read_only_snapshot_data,
            SnapshotData* shared_heap_snapshot_data, bool can_rehash);

  void CheckIsolateLayout();

  void InitializeCodeRanges();
  void AddCodeMemoryRange(MemoryRange range);

  static void RemoveContextIdCallback(const v8::WeakCallbackInfo<void>& data);

  void FireCallCompletedCallbackInternal(MicrotaskQueue* microtask_queue);

  class ThreadDataTable {
   public:
    ThreadDataTable() = default;

    PerIsolateThreadData* Lookup(ThreadId thread_id);
    void Insert(PerIsolateThreadData* data);
    void Remove(PerIsolateThreadData* data);
    void RemoveAllThreads();

   private:
    struct Hasher {
      std::size_t operator()(const ThreadId& t) const {
        return std::hash<int>()(t.ToInteger());
      }
    };

    std::unordered_map<ThreadId, PerIsolateThreadData*, Hasher> table_;
  };

  // These items form a stack synchronously with threads Enter'ing and Exit'ing
  // the Isolate. The top of the stack points to a thread which is currently
  // running the Isolate. When the stack is empty, the Isolate is considered
  // not entered by any thread and can be Disposed.
  // If the same thread enters the Isolate more than once, the entry_count_
  // is incremented rather then a new item pushed to the stack.
  class EntryStackItem {
   public:
    EntryStackItem(PerIsolateThreadData* previous_thread_data,
                   Isolate* previous_isolate, EntryStackItem* previous_item)
        : entry_count(1),
          previous_thread_data(previous_thread_data),
          previous_isolate(previous_isolate),
          previous_item(previous_item) {}
    EntryStackItem(const EntryStackItem&) = delete;
    EntryStackItem& operator=(const EntryStackItem&) = delete;

    int entry_count;
    PerIsolateThreadData* previous_thread_data;
    Isolate* previous_isolate;
    EntryStackItem* previous_item;
  };

  // When a feature flag that requires the shared heap is passed, a shared
  // isolate is created to hold the shared allocations. The shared isolate is
  // created by the first isolate to be created in the process, which is
  // considered the main isolate and owns the lifetime of the shared
  // isolate. The main isolate deletes the shared isolate when it itself is
  // deleted.
  static base::LazyMutex process_wide_shared_isolate_mutex_;
  static Isolate* process_wide_shared_isolate_;

  static Isolate* GetProcessWideSharedIsolate(bool* created_shared_isolate);
  static void DeleteProcessWideSharedIsolate();

  static Isolate* process_wide_shared_space_isolate_;

  void Deinit();

  static void SetIsolateThreadLocals(Isolate* isolate,
                                     PerIsolateThreadData* data);

  void FillCache();

  // Propagate pending exception message to the v8::TryCatch.
  // If there is no external try-catch or message was successfully propagated,
  // then return true.
  bool PropagatePendingExceptionToExternalTryCatch(
      ExceptionHandlerType top_handler);

  bool HasIsolatePromiseHooks() const {
    return PromiseHookFields::HasIsolatePromiseHook::decode(
        promise_hook_flags_);
  }

  bool HasAsyncEventDelegate() const {
    return PromiseHookFields::HasAsyncEventDelegate::decode(
        promise_hook_flags_);
  }

  const char* RAILModeName(RAILMode rail_mode) const {
    switch (rail_mode) {
      case PERFORMANCE_RESPONSE:
        return "RESPONSE";
      case PERFORMANCE_ANIMATION:
        return "ANIMATION";
      case PERFORMANCE_IDLE:
        return "IDLE";
      case PERFORMANCE_LOAD:
        return "LOAD";
    }
    return "";
  }

  void AddCrashKeysForIsolateAndHeapPointers();

  // Returns the Exception sentinel.
  Object ThrowInternal(Object exception, MessageLocation* location);

  // These methods add/remove the isolate to/from the list of clients in the
  // shared isolate. Isolates in the client list need to participate in a global
  // safepoint.
  void AttachToSharedIsolate();
  void DetachFromSharedIsolate();

  void AttachToSharedSpaceIsolate(Isolate* shared_space_isolate);
  void DetachFromSharedSpaceIsolate();

  // This class contains a collection of data accessible from both C++ runtime
  // and compiled code (including assembly stubs, builtins, interpreter bytecode
  // handlers and optimized code).
  IsolateData isolate_data_;

  // Set to true if this isolate is used as shared heap. This field must be set
  // before Heap is constructed, as Heap's constructor consults it.
  const bool is_shared_;

  // Set to true if this isolate is used as main isolate with a shared space.
  bool is_shared_space_isolate_{false};

  std::unique_ptr<IsolateAllocator> isolate_allocator_;
  Heap heap_;
  ReadOnlyHeap* read_only_heap_ = nullptr;
  std::shared_ptr<ReadOnlyArtifacts> artifacts_;
  std::shared_ptr<StringTable> string_table_;
  std::shared_ptr<StringForwardingTable> string_forwarding_table_;

  const int id_;
  EntryStackItem* entry_stack_ = nullptr;
  int stack_trace_nesting_level_ = 0;
  std::atomic<bool> was_locker_ever_used_{false};
  StringStream* incomplete_message_ = nullptr;
  Address isolate_addresses_[kIsolateAddressCount + 1] = {};
  Bootstrapper* bootstrapper_ = nullptr;
  TieringManager* tiering_manager_ = nullptr;
  CompilationCache* compilation_cache_ = nullptr;
  std::shared_ptr<Counters> async_counters_;
  base::RecursiveMutex break_access_;
  base::SharedMutex feedback_vector_access_;
  base::SharedMutex internalized_string_access_;
  base::SharedMutex full_transition_array_access_;
  base::SharedMutex shared_function_info_access_;
  base::SharedMutex map_updater_access_;
  base::SharedMutex boilerplate_migration_access_;
  V8FileLogger* v8_file_logger_ = nullptr;
  StubCache* load_stub_cache_ = nullptr;
  StubCache* store_stub_cache_ = nullptr;
  Deoptimizer* current_deoptimizer_ = nullptr;
  bool deoptimizer_lazy_throw_ = false;
  MaterializedObjectStore* materialized_object_store_ = nullptr;
  bool capture_stack_trace_for_uncaught_exceptions_ = false;
  int stack_trace_for_uncaught_exceptions_frame_limit_ = 0;
  StackTrace::StackTraceOptions stack_trace_for_uncaught_exceptions_options_ =
      StackTrace::kOverview;
  DescriptorLookupCache* descriptor_lookup_cache_ = nullptr;
  HandleScopeData handle_scope_data_;
  HandleScopeImplementer* handle_scope_implementer_ = nullptr;
  UnicodeCache* unicode_cache_ = nullptr;
  AccountingAllocator* allocator_ = nullptr;
  InnerPointerToCodeCache* inner_pointer_to_code_cache_ = nullptr;
  GlobalHandles* global_handles_ = nullptr;
  EternalHandles* eternal_handles_ = nullptr;
  ThreadManager* thread_manager_ = nullptr;
  bigint::Processor* bigint_processor_ = nullptr;
  RuntimeState runtime_state_;
  Builtins builtins_;
  SetupIsolateDelegate* setup_delegate_ = nullptr;
#if defined(DEBUG) || defined(VERIFY_HEAP)
  std::atomic<int> num_active_deserializers_;
#endif
#ifndef V8_INTL_SUPPORT
  unibrow::Mapping<unibrow::Ecma262UnCanonicalize> jsregexp_uncanonicalize_;
  unibrow::Mapping<unibrow::CanonicalizationRange> jsregexp_canonrange_;
  unibrow::Mapping<unibrow::Ecma262Canonicalize>
      regexp_macro_assembler_canonicalize_;
#endif  // !V8_INTL_SUPPORT
  RegExpStack* regexp_stack_ = nullptr;
  std::vector<int> regexp_indices_;
  DateCache* date_cache_ = nullptr;
  base::RandomNumberGenerator* random_number_generator_ = nullptr;
  base::RandomNumberGenerator* fuzzer_rng_ = nullptr;
  std::atomic<RAILMode> rail_mode_;
  v8::Isolate::AtomicsWaitCallback atomics_wait_callback_ = nullptr;
  void* atomics_wait_callback_data_ = nullptr;
  PromiseHook promise_hook_ = nullptr;
  HostImportModuleDynamicallyCallback host_import_module_dynamically_callback_ =
      nullptr;
  HostImportModuleDynamicallyWithImportAssertionsCallback
      host_import_module_dynamically_with_import_assertions_callback_ = nullptr;
  std::atomic<debug::CoverageMode> code_coverage_mode_{
      debug::CoverageMode::kBestEffort};

  // Helper function for RunHostImportModuleDynamicallyCallback.
  // Unpacks import assertions, if present, from the second argument to dynamic
  // import() and returns them in a FixedArray, sorted by code point order of
  // the keys, in the form [key1, value1, key2, value2, ...]. Returns an empty
  // MaybeHandle if an error was thrown.  In this case, the host callback should
  // not be called and instead the caller should use the pending exception to
  // reject the import() call's Promise.
  MaybeHandle<FixedArray> GetImportAssertionsFromArgument(
      MaybeHandle<Object> maybe_import_assertions_argument);

  HostInitializeImportMetaObjectCallback
      host_initialize_import_meta_object_callback_ = nullptr;
  HostCreateShadowRealmContextCallback
      host_create_shadow_realm_context_callback_ = nullptr;

  base::Mutex rail_mutex_;
  double load_start_time_ms_ = 0;

#ifdef V8_INTL_SUPPORT
  std::string default_locale_;

  // The cache stores the most recently accessed {locales,obj} pair for each
  // cache type.
  struct ICUObjectCacheEntry {
    std::string locales;
    std::shared_ptr<icu::UMemory> obj;

    ICUObjectCacheEntry() = default;
    ICUObjectCacheEntry(std::string locales, std::shared_ptr<icu::UMemory> obj)
        : locales(locales), obj(std::move(obj)) {}
  };

  ICUObjectCacheEntry icu_object_cache_[kICUObjectCacheTypeCount];
#endif  // V8_INTL_SUPPORT

  // Whether the isolate has been created for snapshotting.
  bool serializer_enabled_ = false;

  // True if fatal error has been signaled for this isolate.
  bool has_fatal_error_ = false;

  // True if this isolate was initialized from a snapshot.
  bool initialized_from_snapshot_ = false;

  // True if short bultin calls optimization is enabled.
  bool is_short_builtin_calls_enabled_ = false;

  // True if the isolate is in background. This flag is used
  // to prioritize between memory usage and latency.
  bool is_isolate_in_background_ = false;

  // True if the isolate is in memory savings mode. This flag is used to
  // favor memory over runtime performance.
  bool memory_savings_mode_active_ = false;

  // Indicates whether the isolate owns shareable data.
  // Only false for client isolates attached to a shared isolate.
  bool owns_shareable_data_ = true;

  // True if this isolate is attached to a shared isolate, and this isolate is
  // the main isolate in the process and owns the lifetime of the shared
  // isolate.
  bool owns_shared_isolate_ = false;

  bool log_object_relocation_ = false;

#ifdef V8_EXTERNAL_CODE_SPACE
  // Base address of the pointer compression cage containing external code
  // space, when external code space is enabled.
  Address code_cage_base_ = 0;
#endif

  // Time stamp at initialization.
  double time_millis_at_init_ = 0;

#ifdef DEBUG
  static std::atomic<size_t> non_disposed_isolates_;

  JSObject::SpillInformation js_spill_information_;
#endif

  Debug* debug_ = nullptr;
  HeapProfiler* heap_profiler_ = nullptr;
  Logger* logger_ = nullptr;

  const AstStringConstants* ast_string_constants_ = nullptr;

  interpreter::Interpreter* interpreter_ = nullptr;

  compiler::PerIsolateCompilerCache* compiler_cache_ = nullptr;
  // The following zone is for compiler-related objects that should live
  // through all compilations (and thus all JSHeapBroker instances).
  Zone* compiler_zone_ = nullptr;

  std::unique_ptr<LazyCompileDispatcher> lazy_compile_dispatcher_;
  baseline::BaselineBatchCompiler* baseline_batch_compiler_ = nullptr;
#ifdef V8_ENABLE_MAGLEV
  maglev::MaglevConcurrentDispatcher* maglev_concurrent_dispatcher_ = nullptr;
#endif  // V8_ENABLE_MAGLEV

  using InterruptEntry = std::pair<InterruptCallback, void*>;
  std::queue<InterruptEntry> api_interrupts_queue_;

#define GLOBAL_BACKING_STORE(type, name, initialvalue) type name##_;
  ISOLATE_INIT_LIST(GLOBAL_BACKING_STORE)
#undef GLOBAL_BACKING_STORE

#define GLOBAL_ARRAY_BACKING_STORE(type, name, length) type name##_[length];
  ISOLATE_INIT_ARRAY_LIST(GLOBAL_ARRAY_BACKING_STORE)
#undef GLOBAL_ARRAY_BACKING_STORE

#ifdef DEBUG
  // This class is huge and has a number of fields controlled by
  // preprocessor defines. Make sure the offsets of these fields agree
  // between compilation units.
#define ISOLATE_FIELD_OFFSET(type, name, ignored) \
  static const intptr_t name##_debug_offset_;
  ISOLATE_INIT_LIST(ISOLATE_FIELD_OFFSET)
  ISOLATE_INIT_ARRAY_LIST(ISOLATE_FIELD_OFFSET)
#undef ISOLATE_FIELD_OFFSET
#endif

  bool detailed_source_positions_for_profiling_;

  OptimizingCompileDispatcher* optimizing_compile_dispatcher_ = nullptr;

  std::unique_ptr<PersistentHandlesList> persistent_handles_list_;

  // Counts deopt points if deopt_every_n_times is enabled.
  unsigned int stress_deopt_count_ = 0;

  bool force_slow_path_ = false;

  bool initialized_ = false;
  bool jitless_ = false;

  int next_optimization_id_ = 0;

#if V8_SFI_HAS_UNIQUE_ID
  std::atomic<int> next_unique_sfi_id_;
#endif

  unsigned next_module_async_evaluating_ordinal_;

  // Vector of callbacks before a Call starts execution.
  std::vector<BeforeCallEnteredCallback> before_call_entered_callbacks_;

  // Vector of callbacks when a Call completes.
  std::vector<CallCompletedCallback> call_completed_callbacks_;

  v8::Isolate::UseCounterCallback use_counter_callback_ = nullptr;

  std::shared_ptr<CompilationStatistics> turbo_statistics_;
  std::shared_ptr<metrics::Recorder> metrics_recorder_;
  uintptr_t last_recorder_context_id_ = 0;
  std::unordered_map<uintptr_t, v8::Global<v8::Context>>
      recorder_context_id_map_;

  size_t last_long_task_stats_counter_ = 0;
  v8::metrics::LongTaskStats long_task_stats_;

  std::vector<Object> startup_object_cache_;

  // When sharing data among Isolates (e.g. v8_flags.shared_string_table), only
  // the shared Isolate populates this and client Isolates reference that copy.
  //
  // Otherwise this is populated for all Isolates.
  std::vector<Object> shared_heap_object_cache_;

  // Used during builtins compilation to build the builtins constants table,
  // which is stored on the root list prior to serialization.
  BuiltinsConstantsTableBuilder* builtins_constants_table_builder_ = nullptr;

  void InitializeDefaultEmbeddedBlob();
  void CreateAndSetEmbeddedBlob();
  void MaybeRemapEmbeddedBuiltinsIntoCodeRange();
  void TearDownEmbeddedBlob();
  void SetEmbeddedBlob(const uint8_t* code, uint32_t code_size,
                       const uint8_t* data, uint32_t data_size);
  void ClearEmbeddedBlob();

  const uint8_t* embedded_blob_code_ = nullptr;
  uint32_t embedded_blob_code_size_ = 0;
  const uint8_t* embedded_blob_data_ = nullptr;
  uint32_t embedded_blob_data_size_ = 0;

  v8::ArrayBuffer::Allocator* array_buffer_allocator_ = nullptr;
  std::shared_ptr<v8::ArrayBuffer::Allocator> array_buffer_allocator_shared_;

  FutexWaitListNode futex_wait_list_node_;

  CancelableTaskManager* cancelable_task_manager_ = nullptr;

  debug::ConsoleDelegate* console_delegate_ = nullptr;

  debug::AsyncEventDelegate* async_event_delegate_ = nullptr;
  uint32_t promise_hook_flags_ = 0;
  int async_task_count_ = 0;

  std::unique_ptr<LocalIsolate> main_thread_local_isolate_;

  v8::Isolate::AbortOnUncaughtExceptionCallback
      abort_on_uncaught_exception_callback_ = nullptr;

  bool allow_atomics_wait_ = true;

  base::Mutex managed_ptr_destructors_mutex_;
  ManagedPtrDestructor* managed_ptr_destructors_head_ = nullptr;

  size_t total_regexp_code_generated_ = 0;

  size_t elements_deletion_counter_ = 0;

  std::unique_ptr<TracingCpuProfilerImpl> tracing_cpu_profiler_;

  EmbeddedFileWriterInterface* embedded_file_writer_ = nullptr;

  // The top entry of the v8::Context::BackupIncumbentScope stack.
  const v8::Context::BackupIncumbentScope* top_backup_incumbent_scope_ =
      nullptr;

  PrepareStackTraceCallback prepare_stack_trace_callback_ = nullptr;

  // TODO(kenton@cloudflare.com): This mutex can be removed if
  // thread_data_table_ is always accessed under the isolate lock. I do not
  // know if this is the case, so I'm preserving it for now.
  base::Mutex thread_data_table_mutex_;
  ThreadDataTable thread_data_table_;

  // Stores the shared isolate for this client isolate. nullptr for shared
  // isolates or when no shared isolate is used.
  //
  // When non-null, it is identical to process_wide_shared_isolate_.
  Isolate* shared_isolate_ = nullptr;

  // Stores the isolate containing the shared space.
  base::Optional<Isolate*> shared_space_isolate_;

#ifdef V8_COMPRESS_POINTERS
  // The external pointer handle to the Isolate's main thread's WaiterQueueNode.
  // It is used to wait for JS-exposed mutex or condition variable.
  ExternalPointerHandle waiter_queue_node_external_pointer_handle_ =
      kNullExternalPointerHandle;
#endif

#if DEBUG
  // Set to true once during isolate initialization right when attaching to the
  // shared isolate. If there was no shared isolate given it will still be set
  // to true. After this point invocations of shared_isolate() are valid.
  bool attached_to_shared_isolate_ = false;
#endif  // DEBUG

  // Used to track and safepoint all client isolates attached to this shared
  // isolate.
  std::unique_ptr<GlobalSafepoint> global_safepoint_;
  // Client isolates list managed by GlobalSafepoint.
  Isolate* global_safepoint_prev_client_isolate_ = nullptr;
  Isolate* global_safepoint_next_client_isolate_ = nullptr;

  // A signal-safe vector of heap pages containing code. Used with the
  // v8::Unwinder API.
  std::atomic<std::vector<MemoryRange>*> code_pages_{nullptr};
  std::vector<MemoryRange> code_pages_buffer1_;
  std::vector<MemoryRange> code_pages_buffer2_;
  // The mutex only guards adding pages, the retrieval is signal safe.
  base::Mutex code_pages_mutex_;

#ifdef V8_ENABLE_WEBASSEMBLY
  wasm::StackMemory* wasm_stacks_;
#endif

  // Enables the host application to provide a mechanism for recording a
  // predefined set of data as crash keys to be used in postmortem debugging
  // in case of a crash.
  AddCrashKeyCallback add_crash_key_callback_ = nullptr;

  // Delete new/delete operators to ensure that Isolate::New() and
  // Isolate::Delete() are used for Isolate creation and deletion.
  void* operator new(size_t, void* ptr) { return ptr; }

#if USE_SIMULATOR
  SimulatorData* simulator_data_ = nullptr;
#endif

  friend class heap::HeapTester;
  friend class GlobalSafepoint;
  friend class TestSerializer;
  friend class SharedHeapNoClientsTest;
};

// The current entered Isolate and its thread data. Do not access these
// directly! Use Isolate::Current and Isolate::CurrentPerIsolateThreadData.
//
// These are outside the Isolate class with extern storage because in clang-cl,
// thread_local is incompatible with dllexport linkage caused by
// V8_EXPORT_PRIVATE being applied to Isolate.
extern thread_local Isolate::PerIsolateThreadData*
    g_current_per_isolate_thread_data_ V8_CONSTINIT;
extern thread_local Isolate* g_current_isolate_ V8_CONSTINIT;

#undef FIELD_ACCESSOR
#undef THREAD_LOCAL_TOP_ACCESSOR

// SaveContext scopes save the current context on the Isolate on creation, and
// restore it on destruction.
class V8_EXPORT_PRIVATE SaveContext {
 public:
  explicit SaveContext(Isolate* isolate);

  ~SaveContext();

  Handle<Context> context() { return context_; }

  // Returns true if this save context is below a given JavaScript frame.
  bool IsBelowFrame(CommonFrame* frame);

 private:
  Isolate* const isolate_;
  Handle<Context> context_;
  Address c_entry_fp_;
};

// Like SaveContext, but also switches the Context to a new one in the
// constructor.
class V8_EXPORT_PRIVATE SaveAndSwitchContext : public SaveContext {
 public:
  SaveAndSwitchContext(Isolate* isolate, Context new_context);
};

// A scope which sets the given isolate's context to null for its lifetime to
// ensure that code does not make assumptions on a context being available.
class V8_NODISCARD NullContextScope : public SaveAndSwitchContext {
 public:
  explicit NullContextScope(Isolate* isolate)
      : SaveAndSwitchContext(isolate, Context()) {}
};

class AssertNoContextChange {
#ifdef DEBUG
 public:
  explicit AssertNoContextChange(Isolate* isolate);
  ~AssertNoContextChange() { DCHECK(isolate_->context() == *context_); }

 private:
  Isolate* isolate_;
  Handle<Context> context_;
#else
 public:
  explicit AssertNoContextChange(Isolate* isolate) {}
#endif
};

class ExecutionAccess {
 public:
  explicit ExecutionAccess(Isolate* isolate) : isolate_(isolate) {
    Lock(isolate);
  }
  ~ExecutionAccess() { Unlock(isolate_); }

  static void Lock(Isolate* isolate) { isolate->break_access()->Lock(); }
  static void Unlock(Isolate* isolate) { isolate->break_access()->Unlock(); }

  static bool TryLock(Isolate* isolate) {
    return isolate->break_access()->TryLock();
  }

 private:
  Isolate* isolate_;
};

// Support for checking for stack-overflows.
class StackLimitCheck {
 public:
  explicit StackLimitCheck(Isolate* isolate) : isolate_(isolate) {}

  // Use this to check for stack-overflows in C++ code.
  bool HasOverflowed() const {
    StackGuard* stack_guard = isolate_->stack_guard();
    return GetCurrentStackPosition() < stack_guard->real_climit();
  }
  static bool HasOverflowed(LocalIsolate* local_isolate);

  // Use this to check for interrupt request in C++ code.
  V8_INLINE bool InterruptRequested() {
    StackGuard* stack_guard = isolate_->stack_guard();
    return GetCurrentStackPosition() < stack_guard->climit();
  }

  // Handle interripts if InterruptRequested was true.
  // Returns true if any interrupt (overflow or termination) was handled, in
  // which case the caller should prevent further JS execution.
  V8_EXPORT_PRIVATE bool HandleInterrupt(Isolate* isolate);

  // Use this to check for stack-overflow when entering runtime from JS code.
  bool JsHasOverflowed(uintptr_t gap = 0) const;

 private:
  Isolate* isolate_;
};

// This macro may be used in context that disallows JS execution.
// That is why it checks only for a stack overflow and termination.
#define STACK_CHECK(isolate, result_value)        \
  do {                                            \
    StackLimitCheck stack_check(isolate);         \
    if (stack_check.InterruptRequested()) {       \
      if (stack_check.HandleInterrupt(isolate)) { \
        return result_value;                      \
      }                                           \
    }                                             \
  } while (false)

class StackTraceFailureMessage {
 public:
  enum StackTraceMode { kIncludeStackTrace, kDontIncludeStackTrace };

  explicit StackTraceFailureMessage(Isolate* isolate, StackTraceMode mode,
                                    void* ptr1 = nullptr, void* ptr2 = nullptr,
                                    void* ptr3 = nullptr, void* ptr4 = nullptr,
                                    void* ptr5 = nullptr, void* ptr6 = nullptr);

  V8_NOINLINE void Print() volatile;

  static const uintptr_t kStartMarker = 0xdecade30;
  static const uintptr_t kEndMarker = 0xdecade31;
  static const int kStacktraceBufferSize = 32 * KB;

  uintptr_t start_marker_ = kStartMarker;
  void* isolate_;
  void* ptr1_;
  void* ptr2_;
  void* ptr3_;
  void* ptr4_;
  void* ptr5_;
  void* ptr6_;
  void* code_objects_[4];
  char js_stack_trace_[kStacktraceBufferSize];
  uintptr_t end_marker_ = kEndMarker;
};

template <base::MutexSharedType kIsShared>
class V8_NODISCARD SharedMutexGuardIfOffThread<Isolate, kIsShared> final {
 public:
  SharedMutexGuardIfOffThread(base::SharedMutex* mutex, Isolate* isolate) {
    DCHECK_NOT_NULL(mutex);
    DCHECK_NOT_NULL(isolate);
    DCHECK_EQ(ThreadId::Current(), isolate->thread_id());
  }

  SharedMutexGuardIfOffThread(const SharedMutexGuardIfOffThread&) = delete;
  SharedMutexGuardIfOffThread& operator=(const SharedMutexGuardIfOffThread&) =
      delete;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_ISOLATE_H_
