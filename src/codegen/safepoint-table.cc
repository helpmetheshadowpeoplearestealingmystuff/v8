// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/safepoint-table.h"

#include "src/codegen/assembler-inl.h"
#include "src/codegen/macro-assembler.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/diagnostics/disasm.h"
#include "src/execution/frames-inl.h"
#include "src/utils/ostreams.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/wasm-code-manager.h"
#endif  // V8_ENABLE_WEBASSEMBLY

namespace v8 {
namespace internal {

SafepointTable::SafepointTable(Isolate* isolate, Address pc, Code code)
    : SafepointTable(code.InstructionStart(isolate, pc),
                     code.SafepointTableAddress()) {}

#if V8_ENABLE_WEBASSEMBLY
SafepointTable::SafepointTable(const wasm::WasmCode* code)
    : SafepointTable(
          code->instruction_start(),
          code->instruction_start() + code->safepoint_table_offset()) {}
#endif  // V8_ENABLE_WEBASSEMBLY

SafepointTable::SafepointTable(Address instruction_start,
                               Address safepoint_table_address)
    : instruction_start_(instruction_start),
      safepoint_table_address_(safepoint_table_address),
      length_(base::Memory<int>(safepoint_table_address + kLengthOffset)),
      entry_configuration_(base::Memory<uint32_t>(safepoint_table_address +
                                                  kEntryConfigurationOffset)) {}

int SafepointTable::find_return_pc(int pc_offset) {
  for (int i = 0; i < length(); i++) {
    SafepointEntry entry = GetEntry(i);
    if (entry.trampoline_pc() == pc_offset || entry.pc() == pc_offset) {
      return entry.pc();
    }
  }
  UNREACHABLE();
}

SafepointEntry SafepointTable::FindEntry(Address pc) const {
  int pc_offset = static_cast<int>(pc - instruction_start_);
  // We use kMaxUInt32 as sentinel value, so check that we don't hit that.
  DCHECK_NE(kMaxUInt32, pc_offset);
  CHECK_LT(0, length_);
  // A single entry with pc == -1 covers all call sites in the function.
  if (length_ == 1 && GetEntry(0).pc() == -1) return GetEntry(0);
  for (int i = 0; i < length_; i++) {
    // TODO(kasperl): Replace the linear search with binary search.
    SafepointEntry entry = GetEntry(i);
    if (entry.pc() == pc_offset || entry.trampoline_pc() == pc_offset) {
      return entry;
    }
  }
  UNREACHABLE();
}

void SafepointTable::PrintEntry(int index, std::ostream& os) const {
  for (uint8_t bits : GetEntry(index).tagged_slots()) {
    for (int bit = 0; bit < kBitsPerByte; ++bit) {
      os << ((bits >> bit) & 1);
    }
  }
}

Safepoint SafepointTableBuilder::DefineSafepoint(Assembler* assembler) {
  entries_.push_back(EntryBuilder(zone_, assembler->pc_offset_for_safepoint()));
  EntryBuilder& new_entry = entries_.back();
  return Safepoint(new_entry.stack_indexes, &new_entry.register_indexes);
}

int SafepointTableBuilder::UpdateDeoptimizationInfo(int pc, int trampoline,
                                                    int start,
                                                    int deopt_index) {
  DCHECK_NE(SafepointEntry::kNoTrampolinePC, trampoline);
  DCHECK_NE(SafepointEntry::kNoDeoptIndex, deopt_index);
  auto it = entries_.Find(start);
  DCHECK(std::any_of(it, entries_.end(),
                     [pc](auto& entry) { return entry.pc == pc; }));
  int index = start;
  while (it->pc != pc) ++it, ++index;
  it->trampoline = trampoline;
  it->deopt_index = deopt_index;
  return index;
}

void SafepointTableBuilder::Emit(Assembler* assembler, int tagged_slots_size) {
#ifdef DEBUG
  int last_pc = -1;
  int last_trampoline = -1;
  for (const EntryBuilder& entry : entries_) {
    // Entries are ordered by PC.
    DCHECK_LT(last_pc, entry.pc);
    last_pc = entry.pc;
    // Trampoline PCs are increasing, and larger than regular PCs.
    if (entry.trampoline != SafepointEntry::kNoTrampolinePC) {
      DCHECK_LT(last_trampoline, entry.trampoline);
      DCHECK_LT(entries_.back().pc, entry.trampoline);
      last_trampoline = entry.trampoline;
    }
    // An entry either has trampoline and deopt index, or none of the two.
    DCHECK_EQ(entry.trampoline == SafepointEntry::kNoTrampolinePC,
              entry.deopt_index == SafepointEntry::kNoDeoptIndex);
  }
#endif  // DEBUG

  RemoveDuplicates();
  TrimEntries(&tagged_slots_size);

#if V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_ARM64
  // We cannot emit a const pool within the safepoint table.
  Assembler::BlockConstPoolScope block_const_pool(assembler);
#endif

  // Make sure the safepoint table is properly aligned. Pad with nops.
  assembler->Align(Code::kMetadataAlignment);
  assembler->RecordComment(";;; Safepoint table.");
  offset_ = assembler->pc_offset();

  // Compute the number of bytes for tagged slots per safepoint entry.
  int tagged_slots_bytes =
      RoundUp(tagged_slots_size, kBitsPerByte) >> kBitsPerByteLog2;
  bool has_deopt_data =
      std::any_of(entries_.begin(), entries_.end(), [](auto& entry) {
        return entry.deopt_index != SafepointEntry::kNoDeoptIndex;
      });
  bool has_register_indexes =
      std::any_of(entries_.begin(), entries_.end(),
                  [](auto& entry) { return entry.register_indexes != 0; });

  uint32_t entry_configuration =
      SafepointTable::TaggedSlotsBytesField::encode(tagged_slots_bytes) |
      SafepointTable::HasDeoptDataField::encode(has_deopt_data) |
      SafepointTable::HasRegisterIndexesField::encode(has_register_indexes);

  // Emit the table header.
  STATIC_ASSERT(SafepointTable::kLengthOffset == 0 * kIntSize);
  STATIC_ASSERT(SafepointTable::kEntryConfigurationOffset == 1 * kIntSize);
  STATIC_ASSERT(SafepointTable::kHeaderSize == 2 * kIntSize);
  int length = static_cast<int>(entries_.size());
  assembler->dd(length);
  assembler->dd(entry_configuration);

  // Emit entries, sorted by pc offsets.
  for (const EntryBuilder& entry : entries_) {
    assembler->dd(entry.pc);
    if (has_deopt_data) {
      assembler->dd(entry.deopt_index);
      assembler->dd(entry.trampoline);
    }
    if (has_register_indexes) {
      assembler->dd(entry.register_indexes);
    }
  }

  // Emit bitmaps of tagged stack slots.
  ZoneVector<uint8_t> bits(tagged_slots_bytes, 0, zone_);
  for (const EntryBuilder& entry : entries_) {
    std::fill(bits.begin(), bits.end(), 0);

    // Run through the indexes and build a bitmap.
    for (int idx : *entry.stack_indexes) {
      DCHECK_GT(tagged_slots_size, idx);
      int index = tagged_slots_size - 1 - idx;
      int byte_index = index >> kBitsPerByteLog2;
      int bit_index = index & (kBitsPerByte - 1);
      bits[byte_index] |= (1u << bit_index);
    }

    // Emit the bitmap for the current entry.
    for (uint8_t byte : bits) assembler->db(byte);
  }
}

void SafepointTableBuilder::RemoveDuplicates() {
  // If the table contains more than one entry, and all entries are identical
  // (except for the pc), replace the whole table by a single entry with pc =
  // -1. This especially compacts the table for wasm code without tagged
  // pointers and without deoptimization info.

  if (entries_.size() < 2) return;

  // Check that all entries (1, size] are identical to entry 0.
  const EntryBuilder& first_entry = entries_.front();
  auto is_identical_except_for_pc = [](const EntryBuilder& entry1,
                                       const EntryBuilder& entry2) {
    if (entry1.deopt_index != entry2.deopt_index) return false;
    DCHECK_EQ(entry1.trampoline, entry2.trampoline);

    ZoneChunkList<int>* indexes1 = entry1.stack_indexes;
    ZoneChunkList<int>* indexes2 = entry2.stack_indexes;
    if (indexes1->size() != indexes2->size()) return false;
    if (!std::equal(indexes1->begin(), indexes1->end(), indexes2->begin())) {
      return false;
    }

    if (entry1.register_indexes != entry2.register_indexes) return false;

    return true;
  };

  if (std::all_of(entries_.Find(1), entries_.end(),
                  [&](const EntryBuilder& entry) {
                    return is_identical_except_for_pc(first_entry, entry);
                  })) {
    // All entries were identical. Rewind the list to just one
    // entry, and set the pc to -1.
    entries_.Rewind(1);
    entries_.front().pc = -1;
  }
}

void SafepointTableBuilder::TrimEntries(int* tagged_slots_size) {
  int min_index = *tagged_slots_size;
  if (min_index == 0) return;  // Early exit: nothing to trim.

  for (auto& entry : entries_) {
    for (int idx : *entry.stack_indexes) {
      DCHECK_GT(*tagged_slots_size, idx);  // Validity check.
      if (idx >= min_index) continue;
      if (idx == 0) return;  // Early exit: nothing to trim.
      min_index = idx;
    }
  }

  DCHECK_LT(0, min_index);
  *tagged_slots_size -= min_index;
  for (auto& entry : entries_) {
    for (int& idx : *entry.stack_indexes) {
      idx -= min_index;
    }
  }
}

}  // namespace internal
}  // namespace v8
