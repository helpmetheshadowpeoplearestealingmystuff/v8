// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_MEMORY_H_
#define V8_WASM_WASM_MEMORY_H_

#include <unordered_map>

#include "src/base/platform/mutex.h"
#include "src/flags.h"
#include "src/handles.h"
#include "src/objects/js-array.h"

namespace v8 {
namespace internal {
namespace wasm {

class WasmMemoryTracker {
 public:
  WasmMemoryTracker() {}
  ~WasmMemoryTracker();

  // ReserveAddressSpace attempts to increase the reserved address space counter
  // to determine whether there is enough headroom to allocate another guarded
  // Wasm memory. Returns true if successful (meaning it is okay to go ahead and
  // allocate the buffer), false otherwise.
  bool ReserveAddressSpace(size_t num_bytes);

  void RegisterAllocation(void* allocation_base, size_t allocation_length,
                          void* buffer_start, size_t buffer_length);

  struct AllocationData {
    void* const allocation_base = nullptr;
    size_t const allocation_length = 0;
    void* const buffer_start = nullptr;
    size_t const buffer_length = 0;

   private:
    AllocationData(void* allocation_base, size_t allocation_length,
                   void* buffer_start, size_t buffer_length)
        : allocation_base(allocation_base),
          allocation_length(allocation_length),
          buffer_start(buffer_start),
          buffer_length(buffer_length) {
      DCHECK_LE(reinterpret_cast<uintptr_t>(allocation_base),
                reinterpret_cast<uintptr_t>(buffer_start));
      DCHECK_GE(
          reinterpret_cast<uintptr_t>(allocation_base) + allocation_length,
          reinterpret_cast<uintptr_t>(buffer_start));
      DCHECK_GE(
          reinterpret_cast<uintptr_t>(allocation_base) + allocation_length,
          reinterpret_cast<uintptr_t>(buffer_start) + buffer_length);
    }

    friend WasmMemoryTracker;
  };

  // Decreases the amount of reserved address space
  void ReleaseReservation(size_t num_bytes);

  // Removes an allocation from the tracker
  AllocationData ReleaseAllocation(const void* buffer_start);

  bool IsWasmMemory(const void* buffer_start);

  // Returns a pointer to a Wasm buffer's allocation data, or nullptr if the
  // buffer is not tracked.
  const AllocationData* FindAllocationData(const void* buffer_start);

 private:
  // Clients use a two-part process. First they "reserve" the address space,
  // which signifies an intent to actually allocate it. This determines whether
  // doing the allocation would put us over our limit. Once there is a
  // reservation, clients can do the allocation and register the result.
  //
  // We should always have:
  // allocated_address_space_ <= reserved_address_space_ <= kAddressSpaceLimit
  std::atomic_size_t reserved_address_space_{0};

  // Used to protect access to the allocated address space counter and
  // allocation map. This is needed because Wasm memories can be freed on
  // another thread by the ArrayBufferTracker.
  base::Mutex mutex_;

  size_t allocated_address_space_{0};

  // Track Wasm memory allocation information. This is keyed by the start of the
  // buffer, rather than by the start of the allocation.
  std::unordered_map<const void*, AllocationData> allocations_;

  DISALLOW_COPY_AND_ASSIGN(WasmMemoryTracker);
};

Handle<JSArrayBuffer> NewArrayBuffer(
    Isolate*, size_t size, bool require_guard_regions,
    SharedFlag shared = SharedFlag::kNotShared);

Handle<JSArrayBuffer> SetupArrayBuffer(
    Isolate*, void* backing_store, size_t size, bool is_external,
    SharedFlag shared = SharedFlag::kNotShared);

void DetachMemoryBuffer(Isolate* isolate, Handle<JSArrayBuffer> buffer,
                        bool free_memory);

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_MEMORY_H_
