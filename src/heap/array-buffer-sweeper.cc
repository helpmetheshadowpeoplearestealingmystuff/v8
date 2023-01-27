// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/array-buffer-sweeper.h"

#include <atomic>
#include <memory>

#include "src/heap/gc-tracer-inl.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap.h"
#include "src/heap/remembered-set.h"
#include "src/objects/js-array-buffer.h"
#include "src/tasks/cancelable-task.h"
#include "src/tasks/task-utils.h"

namespace v8 {
namespace internal {

void ArrayBufferList::Append(ArrayBufferExtension* extension) {
  if (head_ == nullptr) {
    DCHECK_NULL(tail_);
    head_ = tail_ = extension;
  } else {
    tail_->set_next(extension);
    tail_ = extension;
  }

  const size_t accounting_length = extension->accounting_length();
  DCHECK_GE(bytes_ + accounting_length, bytes_);
  bytes_ += accounting_length;
  extension->set_next(nullptr);
}

void ArrayBufferList::Append(ArrayBufferList* list) {
  if (head_ == nullptr) {
    DCHECK_NULL(tail_);
    head_ = list->head_;
    tail_ = list->tail_;
  } else if (list->head_) {
    DCHECK_NOT_NULL(list->tail_);
    tail_->set_next(list->head_);
    tail_ = list->tail_;
  } else {
    DCHECK_NULL(list->tail_);
  }

  bytes_ += list->ApproximateBytes();
  *list = ArrayBufferList();
}

bool ArrayBufferList::ContainsSlow(ArrayBufferExtension* extension) const {
  for (ArrayBufferExtension* current = head_; current;
       current = current->next()) {
    if (current == extension) return true;
  }
  return false;
}

size_t ArrayBufferList::BytesSlow() const {
  ArrayBufferExtension* current = head_;
  size_t sum = 0;
  while (current) {
    sum += current->accounting_length();
    current = current->next();
  }
  DCHECK_GE(sum, ApproximateBytes());
  return sum;
}

bool ArrayBufferList::IsEmpty() const {
  DCHECK_IMPLIES(head_, tail_);
  DCHECK_IMPLIES(!head_, bytes_ == 0);
  return head_ == nullptr;
}

struct ArrayBufferSweeper::SweepingJob final {
  SweepingJob(ArrayBufferList young, ArrayBufferList old, SweepingType type)
      : state_(SweepingState::kInProgress),
        young_(std::move(young)),
        old_(std::move(old)),
        type_(type) {}

  void Sweep();
  void SweepYoung();
  void SweepFull();
  ArrayBufferList SweepListFull(ArrayBufferList* list);

 private:
  CancelableTaskManager::Id id_ = CancelableTaskManager::kInvalidTaskId;
  std::atomic<SweepingState> state_;
  ArrayBufferList young_;
  ArrayBufferList old_;
  const SweepingType type_;
  std::atomic<size_t> freed_bytes_{0};

  friend class ArrayBufferSweeper;
};

ArrayBufferSweeper::ArrayBufferSweeper(Heap* heap)
    : heap_(heap), local_sweeper_(heap_->sweeper()) {}

ArrayBufferSweeper::~ArrayBufferSweeper() {
  EnsureFinished();
  ReleaseAll(&old_);
  ReleaseAll(&young_);
}

void ArrayBufferSweeper::EnsureFinished() {
  if (!sweeping_in_progress()) return;

  TryAbortResult abort_result =
      heap_->isolate()->cancelable_task_manager()->TryAbort(job_->id_);

  switch (abort_result) {
    case TryAbortResult::kTaskAborted:
      // Task has not run, so we need to run it synchronously here.
      job_->Sweep();
      break;
    case TryAbortResult::kTaskRemoved:
      // Task was removed, but did actually run, just ensure we are in the right
      // state.
      CHECK_EQ(SweepingState::kDone, job_->state_);
      break;
    case TryAbortResult::kTaskRunning: {
      // Task is running. Wait until task is finished with its work.
      base::MutexGuard guard(&sweeping_mutex_);
      while (job_->state_ != SweepingState::kDone) {
        job_finished_.Wait(&sweeping_mutex_);
      }
      break;
    }
  }

  Finalize();
  DCHECK_LE(heap_->backing_store_bytes(), SIZE_MAX);
  DCHECK(!sweeping_in_progress());
}

void ArrayBufferSweeper::FinishIfDone() {
  if (sweeping_in_progress()) {
    DCHECK(job_);
    if (job_->state_ == SweepingState::kDone) {
      Finalize();
    }
  }
}

void ArrayBufferSweeper::RequestSweep(SweepingType type) {
  DCHECK(!sweeping_in_progress());
  DCHECK(local_sweeper_.IsEmpty());

  if (young_.IsEmpty() && (old_.IsEmpty() || type == SweepingType::kYoung))
    return;

  Prepare(type);
  if (!heap_->IsTearingDown() && !heap_->ShouldReduceMemory() &&
      v8_flags.concurrent_array_buffer_sweeping) {
    auto task = MakeCancelableTask(heap_->isolate(), [this, type] {
      GCTracer::Scope::ScopeId scope_id =
          type == SweepingType::kYoung
              ? GCTracer::Scope::BACKGROUND_YOUNG_ARRAY_BUFFER_SWEEP
              : GCTracer::Scope::BACKGROUND_FULL_ARRAY_BUFFER_SWEEP;
      TRACE_GC_EPOCH(heap_->tracer(), scope_id, ThreadKind::kBackground);
      local_sweeper_.ContributeAndWaitForPromotedPagesIteration();
      base::MutexGuard guard(&sweeping_mutex_);
      job_->Sweep();
      job_finished_.NotifyAll();
    });
    job_->id_ = task->id();
    V8::GetCurrentPlatform()->CallOnWorkerThread(std::move(task));
  } else {
    local_sweeper_.ContributeAndWaitForPromotedPagesIteration();
    job_->Sweep();
    Finalize();
  }
}

void ArrayBufferSweeper::Prepare(SweepingType type) {
  DCHECK(!sweeping_in_progress());
  switch (type) {
    case SweepingType::kYoung: {
      job_ = std::make_unique<SweepingJob>(std::move(young_), ArrayBufferList(),
                                           type);
      young_ = ArrayBufferList();
    } break;
    case SweepingType::kFull: {
      job_ = std::make_unique<SweepingJob>(std::move(young_), std::move(old_),
                                           type);
      young_ = ArrayBufferList();
      old_ = ArrayBufferList();
    } break;
  }
  DCHECK(sweeping_in_progress());
}

void ArrayBufferSweeper::Finalize() {
  DCHECK(sweeping_in_progress());
  CHECK_EQ(job_->state_, SweepingState::kDone);
  young_.Append(&job_->young_);
  old_.Append(&job_->old_);
  const size_t freed_bytes =
      job_->freed_bytes_.exchange(0, std::memory_order_relaxed);
  DecrementExternalMemoryCounters(freed_bytes);

  local_sweeper_.Finalize();

  job_.reset();
  DCHECK(!sweeping_in_progress());
}

void ArrayBufferSweeper::ReleaseAll(ArrayBufferList* list) {
  ArrayBufferExtension* current = list->head_;
  while (current) {
    ArrayBufferExtension* next = current->next();
    delete current;
    current = next;
  }
  *list = ArrayBufferList();
}

void ArrayBufferSweeper::Append(JSArrayBuffer object,
                                ArrayBufferExtension* extension) {
  size_t bytes = extension->accounting_length();

  FinishIfDone();

  if (Heap::InYoungGeneration(object)) {
    young_.Append(extension);
  } else {
    old_.Append(extension);
  }

  IncrementExternalMemoryCounters(bytes);
}

void ArrayBufferSweeper::Detach(JSArrayBuffer object,
                                ArrayBufferExtension* extension) {
  size_t bytes = extension->ClearAccountingLength();

  // We cannot free the extension eagerly here, since extensions are tracked in
  // a singly linked list. The next GC will remove it automatically.

  FinishIfDone();

  if (!sweeping_in_progress()) {
    // If concurrent sweeping isn't running at the moment, we can also adjust
    // the respective bytes in the corresponding ArraybufferLists as they are
    // only approximate.
    if (Heap::InYoungGeneration(object)) {
      DCHECK_GE(young_.bytes_, bytes);
      young_.bytes_ -= bytes;
    } else {
      DCHECK_GE(old_.bytes_, bytes);
      old_.bytes_ -= bytes;
    }
  }

  DecrementExternalMemoryCounters(bytes);
}

void ArrayBufferSweeper::IncrementExternalMemoryCounters(size_t bytes) {
  if (bytes == 0) return;
  heap_->IncrementExternalBackingStoreBytes(
      ExternalBackingStoreType::kArrayBuffer, bytes);
  reinterpret_cast<v8::Isolate*>(heap_->isolate())
      ->AdjustAmountOfExternalAllocatedMemory(static_cast<int64_t>(bytes));
}

void ArrayBufferSweeper::DecrementExternalMemoryCounters(size_t bytes) {
  if (bytes == 0) return;
  heap_->DecrementExternalBackingStoreBytes(
      ExternalBackingStoreType::kArrayBuffer, bytes);
  // Unlike IncrementExternalMemoryCounters we don't use
  // AdjustAmountOfExternalAllocatedMemory such that we never start a GC here.
  heap_->update_external_memory(-static_cast<int64_t>(bytes));
}

void ArrayBufferSweeper::SweepingJob::Sweep() {
  CHECK_EQ(state_, SweepingState::kInProgress);
  switch (type_) {
    case SweepingType::kYoung:
      SweepYoung();
      break;
    case SweepingType::kFull:
      SweepFull();
      break;
  }
  state_ = SweepingState::kDone;
}

void ArrayBufferSweeper::SweepingJob::SweepFull() {
  DCHECK_EQ(SweepingType::kFull, type_);
  ArrayBufferList promoted = SweepListFull(&young_);
  ArrayBufferList survived = SweepListFull(&old_);

  old_ = std::move(promoted);
  old_.Append(&survived);
}

ArrayBufferList ArrayBufferSweeper::SweepingJob::SweepListFull(
    ArrayBufferList* list) {
  ArrayBufferExtension* current = list->head_;
  ArrayBufferList survivor_list;

  while (current) {
    ArrayBufferExtension* next = current->next();

    if (!current->IsMarked()) {
      const size_t bytes = current->accounting_length();
      delete current;
      if (bytes) freed_bytes_.fetch_add(bytes, std::memory_order_relaxed);
    } else {
      current->Unmark();
      survivor_list.Append(current);
    }

    current = next;
  }

  *list = ArrayBufferList();
  return survivor_list;
}

void ArrayBufferSweeper::SweepingJob::SweepYoung() {
  DCHECK_EQ(SweepingType::kYoung, type_);
  ArrayBufferExtension* current = young_.head_;

  ArrayBufferList new_young;
  ArrayBufferList new_old;

  while (current) {
    ArrayBufferExtension* next = current->next();

    if (!current->IsYoungMarked()) {
      size_t bytes = current->accounting_length();
      delete current;
      if (bytes) freed_bytes_.fetch_add(bytes, std::memory_order_relaxed);
    } else if (current->IsYoungPromoted()) {
      current->YoungUnmark();
      new_old.Append(current);
    } else {
      current->YoungUnmark();
      new_young.Append(current);
    }

    current = next;
  }

  old_ = new_old;
  young_ = new_young;
}

}  // namespace internal
}  // namespace v8
