// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_ARRAY_BUFFER_TRACKER_INL_H_
#define V8_HEAP_ARRAY_BUFFER_TRACKER_INL_H_

#include "src/heap/array-buffer-tracker.h"
#include "src/heap/heap-inl.h"
#include "src/heap/spaces-inl.h"
#include "src/numbers/conversions-inl.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/objects.h"

#define TRACE_BS(...) /* redefine for tracing output */

namespace v8 {
namespace internal {

void ArrayBufferTracker::RegisterNew(
    Heap* heap, JSArrayBuffer buffer,
    std::shared_ptr<BackingStore> backing_store) {
  if (!backing_store) return;

  // ArrayBuffer tracking works only for small objects.
  DCHECK(!heap->IsLargeObject(buffer));
  DCHECK_EQ(backing_store->buffer_start(), buffer.backing_store());

  const size_t length = buffer.byte_length();
  Page* page = Page::FromHeapObject(buffer);
  {
    base::MutexGuard guard(page->mutex());
    LocalArrayBufferTracker* tracker = page->local_tracker();
    if (tracker == nullptr) {
      page->AllocateLocalTracker();
      tracker = page->local_tracker();
    }
    DCHECK_NOT_NULL(tracker);
    tracker->Add(buffer, std::move(backing_store));
  }

  // TODO(wez): Remove backing-store from external memory accounting.
  // We may go over the limit of externally allocated memory here. We call the
  // api function to trigger a GC in this case.
  reinterpret_cast<v8::Isolate*>(heap->isolate())
      ->AdjustAmountOfExternalAllocatedMemory(length);
}

std::shared_ptr<BackingStore> ArrayBufferTracker::Unregister(
    Heap* heap, JSArrayBuffer buffer) {
  std::shared_ptr<BackingStore> backing_store;

  const size_t length = buffer.byte_length();
  Page* page = Page::FromHeapObject(buffer);
  {
    base::MutexGuard guard(page->mutex());
    LocalArrayBufferTracker* tracker = page->local_tracker();
    DCHECK_NOT_NULL(tracker);
    backing_store = tracker->Remove(buffer);
  }

  // TODO(wez): Remove backing-store from external memory accounting.
  heap->update_external_memory(-static_cast<intptr_t>(length));
  return backing_store;
}

std::shared_ptr<BackingStore> ArrayBufferTracker::Lookup(Heap* heap,
                                                         JSArrayBuffer buffer) {
  std::shared_ptr<BackingStore> backing_store;

  if (buffer.backing_store() == nullptr) return backing_store;

  Page* page = Page::FromHeapObject(buffer);
  base::MutexGuard guard(page->mutex());
  LocalArrayBufferTracker* tracker = page->local_tracker();
  DCHECK_NOT_NULL(tracker);
  return tracker->Lookup(buffer);
}

template <typename Callback>
void LocalArrayBufferTracker::Free(Callback should_free) {
  size_t freed_memory = 0;
  for (TrackingData::iterator it = array_buffers_.begin();
       it != array_buffers_.end();) {
    // Unchecked cast because the map might already be dead at this point.
    JSArrayBuffer buffer = JSArrayBuffer::unchecked_cast(it->first);
    const size_t length = buffer.byte_length();

    if (should_free(buffer)) {
      // Destroy the shared pointer, (perhaps) freeing the backing store.
      TRACE_BS("ABT:free bs=%p mem=%p (%zu bytes)\n", it->second.get(),
               it->second->buffer_start(), it->second->byte_length());
      it = array_buffers_.erase(it);
      freed_memory += length;
    } else {
      ++it;
    }
  }
  if (freed_memory > 0) {
    page_->DecrementExternalBackingStoreBytes(
        ExternalBackingStoreType::kArrayBuffer, freed_memory);

    // TODO(wez): Remove backing-store from external memory accounting.
    page_->heap()->update_external_memory_concurrently_freed(
        static_cast<intptr_t>(freed_memory));
  }
}

template <typename MarkingState>
void ArrayBufferTracker::FreeDead(Page* page, MarkingState* marking_state) {
  // Callers need to ensure having the page lock.
  LocalArrayBufferTracker* tracker = page->local_tracker();
  if (tracker == nullptr) return;
  tracker->Free([marking_state](JSArrayBuffer buffer) {
    return marking_state->IsWhite(buffer);
  });
  if (tracker->IsEmpty()) {
    page->ReleaseLocalTracker();
  }
}

void LocalArrayBufferTracker::Add(JSArrayBuffer buffer,
                                  std::shared_ptr<BackingStore> backing_store) {
  page_->IncrementExternalBackingStoreBytes(
      ExternalBackingStoreType::kArrayBuffer, buffer.byte_length());

  AddInternal(buffer, std::move(backing_store));
}

void LocalArrayBufferTracker::AddInternal(
    JSArrayBuffer buffer, std::shared_ptr<BackingStore> backing_store) {
  auto ret = array_buffers_.insert({buffer, std::move(backing_store)});
  USE(ret);
  // Check that we indeed inserted a new value and did not overwrite an existing
  // one (which would be a bug).
  DCHECK(ret.second);
}

std::shared_ptr<BackingStore> LocalArrayBufferTracker::Remove(
    JSArrayBuffer buffer) {
  TrackingData::iterator it = array_buffers_.find(buffer);

  // Check that we indeed find a key to remove.
  DCHECK(it != array_buffers_.end());

  // Steal the underlying shared pointer before erasing the entry.
  std::shared_ptr<BackingStore> backing_store = std::move(it->second);

  TRACE_BS("ABT:remove bs=%p mem=%p (%zu bytes)\n", backing_store.get(),
           backing_store->buffer_start(), backing_store->byte_length());

  // Erase the entry.
  array_buffers_.erase(it);

  // Update accounting.
  page_->DecrementExternalBackingStoreBytes(
      ExternalBackingStoreType::kArrayBuffer, buffer.byte_length());

  return backing_store;
}

std::shared_ptr<BackingStore> LocalArrayBufferTracker::Lookup(
    JSArrayBuffer buffer) {
  TrackingData::iterator it = array_buffers_.find(buffer);
  if (it != array_buffers_.end()) {
    return it->second;
  }
  return std::shared_ptr<BackingStore>();
}

#undef TRACE_BS

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_ARRAY_BUFFER_TRACKER_INL_H_
