// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_PLATFORM_H_
#define INCLUDE_CPPGC_PLATFORM_H_

#include <memory>

#include "v8-platform.h"  // NOLINT(build/include_directory)
#include "v8config.h"     // NOLINT(build/include_directory)

namespace cppgc {

// TODO(v8:10346): Create separate includes for concepts that are not
// V8-specific.
using IdleTask = v8::IdleTask;
using JobHandle = v8::JobHandle;
using JobDelegate = v8::JobDelegate;
using JobTask = v8::JobTask;
using PageAllocator = v8::PageAllocator;
using Task = v8::Task;
using TaskPriority = v8::TaskPriority;
using TaskRunner = v8::TaskRunner;
using TracingController = v8::TracingController;

/**
 * Platform interface used by Heap. Contains allocators and executors.
 */
class V8_EXPORT Platform {
 public:
  static constexpr bool StackAddressesSmallerThanHeapAddresses();

  virtual ~Platform() = default;

  /**
   * Returns the allocator used by cppgc to allocate its heap and various
   * support structures.
   */
  virtual PageAllocator* GetPageAllocator() = 0;

  /**
   * Monotonically increasing time in seconds from an arbitrary fixed point in
   * the past. This function is expected to return at least
   * millisecond-precision values. For this reason,
   * it is recommended that the fixed point be no further in the past than
   * the epoch.
   **/
  virtual double MonotonicallyIncreasingTime() = 0;

  /**
   * Foreground task runner that should be used by a Heap.
   */
  virtual std::shared_ptr<TaskRunner> GetForegroundTaskRunner() {
    return nullptr;
  }

  /**
   * Posts `job_task` to run in parallel. Returns a `JobHandle` associated with
   * the `Job`, which can be joined or canceled.
   * This avoids degenerate cases:
   * - Calling `CallOnWorkerThread()` for each work item, causing significant
   *   overhead.
   * - Fixed number of `CallOnWorkerThread()` calls that split the work and
   *   might run for a long time. This is problematic when many components post
   *   "num cores" tasks and all expect to use all the cores. In these cases,
   *   the scheduler lacks context to be fair to multiple same-priority requests
   *   and/or ability to request lower priority work to yield when high priority
   *   work comes in.
   * A canonical implementation of `job_task` looks like:
   * \code
   * class MyJobTask : public JobTask {
   *  public:
   *   MyJobTask(...) : worker_queue_(...) {}
   *   // JobTask implementation.
   *   void Run(JobDelegate* delegate) override {
   *     while (!delegate->ShouldYield()) {
   *       // Smallest unit of work.
   *       auto work_item = worker_queue_.TakeWorkItem(); // Thread safe.
   *       if (!work_item) return;
   *       ProcessWork(work_item);
   *     }
   *   }
   *
   *   size_t GetMaxConcurrency() const override {
   *     return worker_queue_.GetSize(); // Thread safe.
   *   }
   * };
   *
   * // ...
   * auto handle = PostJob(TaskPriority::kUserVisible,
   *                       std::make_unique<MyJobTask>(...));
   * handle->Join();
   * \endcode
   *
   * `PostJob()` and methods of the returned JobHandle/JobDelegate, must never
   * be called while holding a lock that could be acquired by `JobTask::Run()`
   * or `JobTask::GetMaxConcurrency()` -- that could result in a deadlock. This
   * is because (1) `JobTask::GetMaxConcurrency()` may be invoked while holding
   * internal lock (A), hence `JobTask::GetMaxConcurrency()` can only use a lock
   * (B) if that lock is *never* held while calling back into `JobHandle` from
   * any thread (A=>B/B=>A deadlock) and (2) `JobTask::Run()` or
   * `JobTask::GetMaxConcurrency()` may be invoked synchronously from
   * `JobHandle` (B=>JobHandle::foo=>B deadlock).
   *
   * A sufficient `PostJob()` implementation that uses the default Job provided
   * in libplatform looks like:
   * \code
   * std::unique_ptr<JobHandle> PostJob(
   *     TaskPriority priority, std::unique_ptr<JobTask> job_task) override {
   *   return std::make_unique<DefaultJobHandle>(
   *       std::make_shared<DefaultJobState>(
   *           this, std::move(job_task), kNumThreads));
   * }
   * \endcode
   */
  virtual std::unique_ptr<JobHandle> PostJob(
      TaskPriority priority, std::unique_ptr<JobTask> job_task) {
    return nullptr;
  }

  /**
   * Returns an instance of a `TracingController`. This must be non-nullptr. The
   * default implementation returns an empty `TracingController` that consumes
   * trace data without effect.
   */
  virtual TracingController* GetTracingController();
};

/**
 * Process-global initialization of the garbage collector. Must be called before
 * creating a Heap.
 *
 * Can be called multiple times when paired with `ShutdownProcess()`.
 *
 * \param page_allocator The allocator used for maintaining meta data. Must not
 *   change between multiple calls to InitializeProcess.
 */
V8_EXPORT void InitializeProcess(PageAllocator* page_allocator);

/**
 * Must be called after destroying the last used heap. Some process-global
 * metadata may not be returned and reused upon a subsequent
 * `InitializeProcess()` call.
 */
V8_EXPORT void ShutdownProcess();

namespace internal {

V8_EXPORT void Abort();

}  // namespace internal

// static
constexpr bool Platform::StackAddressesSmallerThanHeapAddresses() {
#if V8_OS_WIN || V8_OS_FUCHSIA || V8_OS_MACOSX
  return false;
#else
  return true;
#endif  // V8_OS_WIN || V8_OS_FUCHSIA || V8_OS_MACOSX
}

}  // namespace cppgc

#endif  // INCLUDE_CPPGC_PLATFORM_H_
