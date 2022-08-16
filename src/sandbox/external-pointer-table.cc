// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/sandbox/external-pointer-table.h"

#include <algorithm>

#include "src/execution/isolate.h"
#include "src/logging/counters.h"
#include "src/sandbox/external-pointer-table-inl.h"

#ifdef V8_COMPRESS_POINTERS

namespace v8 {
namespace internal {

static_assert(sizeof(ExternalPointerTable) == ExternalPointerTable::kSize);

uint32_t ExternalPointerTable::SweepAndCompact(Isolate* isolate) {
  // There must not be any entry allocations while the table is being swept as
  // that would not be safe. Set the freelist head to this special marker value
  // to better catch any violation of this requirement.
  uint32_t old_freelist_head = base::Relaxed_Load(&freelist_head_);
  base::Release_Store(&freelist_head_, kTableIsCurrentlySweepingMarker);

  // Keep track of the last block (identified by the index of its first entry)
  // that has live entries. Used to decommit empty blocks at the end.
  DCHECK_GE(capacity(), kEntriesPerBlock);
  const uint32_t last_block = capacity() - kEntriesPerBlock;
  uint32_t last_in_use_block = last_block;

  // When compacting, we can compute the number of unused blocks at the end of
  // the table and skip those during sweeping.
  if (IsCompacting()) {
    DCHECK(IsAligned(start_of_evacuation_area_, kEntriesPerBlock));

    TableCompactionOutcome outcome;
    if (start_of_evacuation_area_ == kTableCompactionAbortedMarker) {
      // Compaction was aborted during marking because the freelist grew to
      // short. This is not great because there is now no guarantee that any
      // blocks will be completely emtpy and so the entire table needs to be
      // swept.
      outcome = TableCompactionOutcome::kAbortedDuringMarking;
    } else if (!old_freelist_head ||
               old_freelist_head > start_of_evacuation_area_) {
      // In this case, marking finished successfully, but the application
      // afterwards allocated entries inside the area that is being compacted.
      // In this case, we can still compute how many blocks at the end of the
      // table are now empty.
      if (old_freelist_head) {
        last_in_use_block = RoundDown(old_freelist_head, kEntriesPerBlock);
      }
      outcome = TableCompactionOutcome::kPartialSuccess;
    } else {
      // Marking was successful so the entire area that we are compacting is now
      // free.
      last_in_use_block = start_of_evacuation_area_ - kEntriesPerBlock;
      outcome = TableCompactionOutcome::kSuccess;
    }
    isolate->counters()->external_pointer_table_compaction_outcome()->AddSample(
        static_cast<int>(outcome));
  }

  // Sweep top to bottom and rebuild the freelist from newly dead and
  // previously freed entries while also clearing the marking bit on live
  // entries and resolving evacuation entries table when compacting the table.
  // This way, the freelist ends up sorted by index which already makes the
  // table somewhat self-compacting and is required for the compaction
  // algorithm so that evacuated entries are evacuated to the start of the
  // table. This method must run either on the mutator thread or while the
  // mutator is stopped.
  uint32_t current_freelist_size = 0;
  uint32_t current_freelist_head = 0;

  // Skip the special null entry. This also guarantees that the first block
  // will never be decommitted.
  DCHECK_GE(capacity(), 1);
  uint32_t table_end = last_in_use_block + kEntriesPerBlock;
  DCHECK(IsAligned(table_end, kEntriesPerBlock));
  for (uint32_t i = table_end - 1; i > 0; i--) {
    // No other threads are active during sweep, so there is no need to use
    // atomic operations here.
    Address entry = load(i);
    if (is_evacuation_entry(entry)) {
      // Resolve the evacuation entry: take the pointer to the handle from the
      // evacuation entry, copy the entry to its new location, and finally
      // update the handle to point to the new entry.
      Address evacuation_entry = load(i);
      ExternalPointerHandle* handle_location =
          reinterpret_cast<ExternalPointerHandle*>(
              extract_handle_location_from_evacuation_entry(evacuation_entry));

      ExternalPointerHandle old_handle = *handle_location;
      ExternalPointerHandle new_handle = index_to_handle(i);
      DCHECK_GE(handle_to_index(old_handle), start_of_evacuation_area_);
      DCHECK_LT(handle_to_index(new_handle), start_of_evacuation_area_);

      Address entry_to_evacuate = load(handle_to_index(old_handle));
      store(i, clear_mark_bit(entry_to_evacuate));
      *handle_location = new_handle;

#ifdef DEBUG
      // In debug builds, clobber the old entry so that any sharing of table
      // entries is easily detected. Shared entries would require write
      // barriers, so we'd like to avoid them. See the compaction algorithm
      // explanation in external-pointer-table.h for more details.
      constexpr Address kClobberedEntryMarker = static_cast<Address>(-1);
      DCHECK_NE(entry_to_evacuate, kClobberedEntryMarker);
      store(handle_to_index(old_handle), kClobberedEntryMarker);
#endif  //  DEBUG

      // While we know that the old entry is now free, we don't add it to (the
      // start of) the freelist because that would immediately cause new
      // fragmentation when the next entry is allocated. Instead, we assume
      // that the blocks out of which entries are evacuated will all be
      // decommitted anyway after this loop, which is usually the case unless
      // compaction was already aborted during marking.
    } else if (!is_marked(entry)) {
      current_freelist_size++;
      Address entry =
          make_freelist_entry(current_freelist_head, current_freelist_size);
      store(i, entry);
      current_freelist_head = i;
    } else {
      store(i, clear_mark_bit(entry));
    }

    if (last_in_use_block == i) {
      // Finished iterating over the last in-use block. Now see if it is
      // empty.
      if (current_freelist_size == kEntriesPerBlock) {
        // Block is completely empty, so mark it for decommitting.
        last_in_use_block -= kEntriesPerBlock;
        // Freelist is now empty again.
        current_freelist_head = 0;
        current_freelist_size = 0;
      }
    }
  }

  // Decommit all blocks at the end of the table that are not used anymore.
  if (last_in_use_block != last_block) {
    uint32_t new_capacity = last_in_use_block + kEntriesPerBlock;
    DCHECK_LT(new_capacity, capacity());
    Address new_table_end = buffer_ + new_capacity * sizeof(Address);
    uint32_t bytes_to_decommit = (capacity() - new_capacity) * sizeof(Address);
    set_capacity(new_capacity);

    VirtualAddressSpace* root_space = GetPlatformVirtualAddressSpace();
    // The pages may contain stale pointers which could be abused by an
    // attacker if they are still accessible, so use Decommit here which
    // guarantees that the pages become inaccessible and will be zeroed out.
    CHECK(root_space->DecommitPages(new_table_end, bytes_to_decommit));
  }

  if (IsCompacting()) {
    StopCompacting();
  }

  base::Release_Store(&freelist_head_, current_freelist_head);

  uint32_t num_active_entries = capacity() - current_freelist_size;
  isolate->counters()->external_pointers_count()->AddSample(num_active_entries);
  return num_active_entries;
}

void ExternalPointerTable::StartCompactingIfNeeded() {
  // This method may be executed while other threads allocate entries from the
  // freelist or even grow the table, thereby increasing the capacity. In that
  // case, this method may use incorrect data to determine if table compaction
  // is necessary. That's fine however since in the worst case, compaction will
  // simply be aborted right away if the freelist became too small.
  uint32_t freelist_size = FreelistSize();
  uint32_t current_capacity = capacity();

  // Current (somewhat arbitrary) heuristic: need compacting if the table is
  // more than 1MB in size and is at least 10% empty.
  uint32_t table_size = current_capacity * kSystemPointerSize;
  double free_ratio = static_cast<double>(freelist_size) /
                      static_cast<double>(current_capacity);
  bool should_compact = (table_size >= 1 * MB) && (free_ratio >= 0.10);

  if (should_compact) {
    uint32_t num_entries_to_free = freelist_size / 2;
    num_entries_to_free = RoundDown(num_entries_to_free, kBlockSize);
    DCHECK_GT(num_entries_to_free, 0);
    // A non-zero value for this member indicates that compaction is running.
    start_of_evacuation_area_ = current_capacity - num_entries_to_free;
  }
}

void ExternalPointerTable::StopCompacting() {
  DCHECK(IsCompacting());
  start_of_evacuation_area_ = 0;
}

uint32_t ExternalPointerTable::Grow() {
  // Freelist should be empty.
  DCHECK_EQ(0, freelist_head_);
  // Mutex must be held when calling this method.
  mutex_->AssertHeld();

  // Grow the table by one block.
  uint32_t old_capacity = capacity();
  uint32_t new_capacity = old_capacity + kEntriesPerBlock;
  CHECK_LE(new_capacity, kMaxExternalPointers);

  // Failure likely means OOM. TODO(saelo) handle this.
  VirtualAddressSpace* root_space = GetPlatformVirtualAddressSpace();
  DCHECK(IsAligned(kBlockSize, root_space->page_size()));
  CHECK(root_space->SetPagePermissions(buffer_ + old_capacity * sizeof(Address),
                                       kBlockSize,
                                       PagePermissions::kReadWrite));
  set_capacity(new_capacity);

  // Build freelist bottom to top, which might be more cache friendly.
  uint32_t start = std::max<uint32_t>(old_capacity, 1);  // Skip entry zero
  uint32_t last = new_capacity - 1;
  uint32_t current_freelist_size = 1;
  for (uint32_t i = start; i < last; i++) {
    uint32_t next_entry = i + 1;
    store(i, make_freelist_entry(next_entry, current_freelist_size++));
  }
  store(last, make_freelist_entry(0, current_freelist_size));

  // This must be a release store to prevent reordering of the preceeding
  // stores to the freelist from being reordered past this store. See
  // Allocate() for more details.
  base::Release_Store(&freelist_head_, start);
  return start;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_COMPRESS_POINTERS
