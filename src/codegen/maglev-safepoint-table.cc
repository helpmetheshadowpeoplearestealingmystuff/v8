// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/maglev-safepoint-table.h"

#include <iomanip>

#include "src/codegen/macro-assembler.h"
#include "src/objects/code-inl.h"

namespace v8 {
namespace internal {

MaglevSafepointTable::MaglevSafepointTable(Isolate* isolate, Address pc,
                                           Code code)
    : MaglevSafepointTable(code.InstructionStart(isolate, pc),
                           code.SafepointTableAddress()) {
  DCHECK(code.is_maglevved());
}

MaglevSafepointTable::MaglevSafepointTable(Address instruction_start,
                                           Address safepoint_table_address)
    : instruction_start_(instruction_start),
      safepoint_table_address_(safepoint_table_address),
      length_(base::Memory<int>(safepoint_table_address + kLengthOffset)),
      entry_configuration_(base::Memory<uint32_t>(safepoint_table_address +
                                                  kEntryConfigurationOffset)),
      num_tagged_slots_(base::Memory<uint32_t>(safepoint_table_address +
                                               kNumTaggedSlotsOffset)),
      num_untagged_slots_(base::Memory<uint32_t>(safepoint_table_address +
                                                 kNumUntaggedSlotsOffset)) {}

int MaglevSafepointTable::find_return_pc(int pc_offset) {
  for (int i = 0; i < length(); i++) {
    MaglevSafepointEntry entry = GetEntry(i);
    if (entry.trampoline_pc() == pc_offset || entry.pc() == pc_offset) {
      return entry.pc();
    }
  }
  UNREACHABLE();
}

MaglevSafepointEntry MaglevSafepointTable::FindEntry(Address pc) const {
  int pc_offset = static_cast<int>(pc - instruction_start_);

  // Check if the PC is pointing at a trampoline.
  if (has_deopt_data()) {
    int candidate = -1;
    for (int i = 0; i < length_; ++i) {
      int trampoline_pc = GetEntry(i).trampoline_pc();
      if (trampoline_pc != -1 && trampoline_pc <= pc_offset) candidate = i;
      if (trampoline_pc > pc_offset) break;
    }
    if (candidate != -1) return GetEntry(candidate);
  }

  for (int i = 0; i < length_; ++i) {
    MaglevSafepointEntry entry = GetEntry(i);
    if (i == length_ - 1 || GetEntry(i + 1).pc() > pc_offset) {
      DCHECK_LE(entry.pc(), pc_offset);
      return entry;
    }
  }
  UNREACHABLE();
}

void MaglevSafepointTable::Print(std::ostream& os) const {
  os << "Safepoints (entries = " << length_ << ", byte size = " << byte_size()
     << ", tagged slots = " << num_tagged_slots_
     << ", untagged slots = " << num_untagged_slots_ << ")\n";

  for (int index = 0; index < length_; index++) {
    MaglevSafepointEntry entry = GetEntry(index);
    os << reinterpret_cast<const void*>(instruction_start_ + entry.pc()) << " "
       << std::setw(6) << std::hex << entry.pc() << std::dec;

    os << "  num pushed registers: "
       << static_cast<int>(entry.num_pushed_registers());

    if (entry.tagged_register_indexes() != 0) {
      os << "  registers: ";
      uint32_t register_bits = entry.tagged_register_indexes();
      int bits = 32 - base::bits::CountLeadingZeros32(register_bits);
      for (int j = bits - 1; j >= 0; --j) {
        os << ((register_bits >> j) & 1);
      }
    }

    if (entry.has_deoptimization_index()) {
      os << "  deopt " << std::setw(6) << entry.deoptimization_index()
         << " trampoline: " << std::setw(6) << std::hex
         << entry.trampoline_pc();
    }
    os << "\n";
  }
}

MaglevSafepointTableBuilder::Safepoint
MaglevSafepointTableBuilder::DefineSafepoint(Assembler* assembler) {
  entries_.push_back(EntryBuilder(assembler->pc_offset_for_safepoint()));
  return MaglevSafepointTableBuilder::Safepoint(&entries_.back());
}

int MaglevSafepointTableBuilder::UpdateDeoptimizationInfo(int pc,
                                                          int trampoline,
                                                          int start,
                                                          int deopt_index) {
  DCHECK_NE(MaglevSafepointEntry::kNoTrampolinePC, trampoline);
  DCHECK_NE(MaglevSafepointEntry::kNoDeoptIndex, deopt_index);
  auto it = entries_.Find(start);
  DCHECK(std::any_of(it, entries_.end(),
                     [pc](auto& entry) { return entry.pc == pc; }));
  int index = start;
  while (it->pc != pc) ++it, ++index;
  it->trampoline = trampoline;
  it->deopt_index = deopt_index;
  return index;
}

void MaglevSafepointTableBuilder::Emit(Assembler* assembler) {
#ifdef DEBUG
  int last_pc = -1;
  int last_trampoline = -1;
  for (const EntryBuilder& entry : entries_) {
    // Entries are ordered by PC.
    DCHECK_LT(last_pc, entry.pc);
    last_pc = entry.pc;
    // Trampoline PCs are increasing, and larger than regular PCs.
    if (entry.trampoline != MaglevSafepointEntry::kNoTrampolinePC) {
      DCHECK_LT(last_trampoline, entry.trampoline);
      DCHECK_LT(entries_.back().pc, entry.trampoline);
      last_trampoline = entry.trampoline;
    }
    // An entry either has trampoline and deopt index, or none of the two.
    DCHECK_EQ(entry.trampoline == MaglevSafepointEntry::kNoTrampolinePC,
              entry.deopt_index == MaglevSafepointEntry::kNoDeoptIndex);
  }
#endif  // DEBUG

  RemoveDuplicates();

#if V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_ARM64
  // We cannot emit a const pool within the safepoint table.
  Assembler::BlockConstPoolScope block_const_pool(assembler);
#endif

  // Make sure the safepoint table is properly aligned. Pad with nops.
  assembler->Align(Code::kMetadataAlignment);
  assembler->RecordComment(";;; Maglev safepoint table.");
  set_safepoint_table_offset(assembler->pc_offset());

  // Compute the required sizes of the fields.
  int used_register_indexes = 0;
  static_assert(MaglevSafepointEntry::kNoTrampolinePC == -1);
  int max_pc = MaglevSafepointEntry::kNoTrampolinePC;
  static_assert(MaglevSafepointEntry::kNoDeoptIndex == -1);
  int max_deopt_index = MaglevSafepointEntry::kNoDeoptIndex;
  for (const EntryBuilder& entry : entries_) {
    used_register_indexes |= entry.tagged_register_indexes;
    max_pc = std::max(max_pc, std::max(entry.pc, entry.trampoline));
    max_deopt_index = std::max(max_deopt_index, entry.deopt_index);
  }

  // Derive the bytes and bools for the entry configuration from the values.
  auto value_to_bytes = [](int value) {
    DCHECK_LE(0, value);
    if (value == 0) return 0;
    if (value <= 0xff) return 1;
    if (value <= 0xffff) return 2;
    if (value <= 0xffffff) return 3;
    return 4;
  };
  bool has_deopt_data = max_deopt_index != -1;
  int register_indexes_size = value_to_bytes(used_register_indexes);
  // Add 1 so all values (including kNoDeoptIndex and kNoTrampolinePC) are
  // non-negative.
  static_assert(MaglevSafepointEntry::kNoDeoptIndex == -1);
  static_assert(MaglevSafepointEntry::kNoTrampolinePC == -1);
  int pc_size = value_to_bytes(max_pc + 1);
  int deopt_index_size = value_to_bytes(max_deopt_index + 1);

  // Add a CHECK to ensure we never overflow the space in the bitfield, even for
  // huge functions which might not be covered by tests.
  CHECK(MaglevSafepointTable::RegisterIndexesSizeField::is_valid(
      register_indexes_size));
  CHECK(MaglevSafepointTable::PcSizeField::is_valid(pc_size));
  CHECK(MaglevSafepointTable::DeoptIndexSizeField::is_valid(deopt_index_size));

  uint32_t entry_configuration =
      MaglevSafepointTable::HasDeoptDataField::encode(has_deopt_data) |
      MaglevSafepointTable::RegisterIndexesSizeField::encode(
          register_indexes_size) |
      MaglevSafepointTable::PcSizeField::encode(pc_size) |
      MaglevSafepointTable::DeoptIndexSizeField::encode(deopt_index_size);

  // Emit the table header.
  static_assert(MaglevSafepointTable::kLengthOffset == 0 * kIntSize);
  static_assert(MaglevSafepointTable::kEntryConfigurationOffset ==
                1 * kIntSize);
  static_assert(MaglevSafepointTable::kNumTaggedSlotsOffset == 2 * kIntSize);
  static_assert(MaglevSafepointTable::kNumUntaggedSlotsOffset == 3 * kIntSize);
  static_assert(MaglevSafepointTable::kHeaderSize == 4 * kIntSize);
  int length = static_cast<int>(entries_.size());
  assembler->dd(length);
  assembler->dd(entry_configuration);
  assembler->dd(num_tagged_slots_);
  assembler->dd(num_untagged_slots_);

  auto emit_bytes = [assembler](int value, int bytes) {
    DCHECK_LE(0, value);
    for (; bytes > 0; --bytes, value >>= 8) assembler->db(value);
    DCHECK_EQ(0, value);
  };
  // Emit entries, sorted by pc offsets.
  for (const EntryBuilder& entry : entries_) {
    emit_bytes(entry.pc, pc_size);
    if (has_deopt_data) {
      // Add 1 so all values (including kNoDeoptIndex and kNoTrampolinePC) are
      // non-negative.
      static_assert(MaglevSafepointEntry::kNoDeoptIndex == -1);
      static_assert(MaglevSafepointEntry::kNoTrampolinePC == -1);
      emit_bytes(entry.deopt_index + 1, deopt_index_size);
      emit_bytes(entry.trampoline + 1, pc_size);
    }
    assembler->db(entry.num_pushed_registers);
    emit_bytes(entry.tagged_register_indexes, register_indexes_size);
  }
}

void MaglevSafepointTableBuilder::RemoveDuplicates() {
  // Remove any duplicate entries, i.e. succeeding entries that are identical
  // except for the PC. During lookup, we will find the first entry whose PC is
  // not larger than the PC at hand, and find the first non-duplicate.

  if (entries_.size() < 2) return;

  auto is_identical_except_for_pc = [](const EntryBuilder& entry1,
                                       const EntryBuilder& entry2) {
    if (entry1.deopt_index != entry2.deopt_index) return false;
    DCHECK_EQ(entry1.trampoline, entry2.trampoline);
    return entry1.num_pushed_registers == entry2.num_pushed_registers &&
           entry1.tagged_register_indexes == entry2.tagged_register_indexes;
  };

  auto remaining_it = entries_.begin();
  size_t remaining = 0;

  for (auto it = entries_.begin(), end = entries_.end(); it != end;
       ++remaining_it, ++remaining) {
    if (remaining_it != it) *remaining_it = *it;
    // Merge identical entries.
    do {
      ++it;
    } while (it != end && is_identical_except_for_pc(*it, *remaining_it));
  }

  entries_.Rewind(remaining);
}

}  // namespace internal
}  // namespace v8
