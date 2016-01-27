// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_STORE_BUFFER_INL_H_
#define V8_STORE_BUFFER_INL_H_

#include "src/heap/heap.h"
#include "src/heap/spaces-inl.h"
#include "src/heap/store-buffer.h"

namespace v8 {
namespace internal {

void StoreBuffer::Mark(Address addr) {
  DCHECK(!heap_->code_space()->Contains(addr));
  Address* top = reinterpret_cast<Address*>(heap_->store_buffer_top());
  *top++ = addr;
  heap_->set_store_buffer_top(reinterpret_cast<Smi*>(top));
  if ((reinterpret_cast<uintptr_t>(top) & kStoreBufferOverflowBit) != 0) {
    DCHECK(top == limit_);
    Compact();
  } else {
    DCHECK(top < limit_);
  }
}


void StoreBuffer::EnterDirectlyIntoStoreBuffer(Address addr) {
  if (store_buffer_rebuilding_enabled_) {
    SLOW_DCHECK(!heap_->code_space()->Contains(addr) &&
                !heap_->new_space()->Contains(addr));
    Address* top = old_top_;
    *top++ = addr;
    old_top_ = top;
    old_buffer_is_sorted_ = false;
    old_buffer_is_filtered_ = false;
    if (top >= old_limit_) {
      DCHECK(callback_ != NULL);
      (*callback_)(heap_, MemoryChunk::FromAnyPointerAddress(heap_, addr),
                   kStoreBufferFullEvent);
    }
  }
}

void LocalStoreBuffer::Record(Address addr) {
  if (top_->is_full()) top_ = new Node(top_);
  top_->buffer[top_->count++] = addr;
}

void LocalStoreBuffer::Process(StoreBuffer* store_buffer) {
  Node* current = top_;
  while (current != nullptr) {
    for (int i = 0; i < current->count; i++) {
      store_buffer->Mark(current->buffer[i]);
    }
    current = current->next;
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_STORE_BUFFER_INL_H_
