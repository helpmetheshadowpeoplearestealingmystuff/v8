// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_LOONG64_ASSEMBLER_LOONG64_INL_H_
#define V8_CODEGEN_LOONG64_ASSEMBLER_LOONG64_INL_H_

#include "src/codegen/assembler.h"
#include "src/codegen/flush-instruction-cache.h"
#include "src/codegen/loong64/assembler-loong64.h"
#include "src/debug/debug.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

bool CpuFeatures::SupportsOptimizer() { return IsSupported(FPU); }

// -----------------------------------------------------------------------------
// Operand and MemOperand.

bool Operand::is_reg() const { return rm_.is_valid(); }

int64_t Operand::immediate() const {
  DCHECK(!is_reg());
  DCHECK(!IsHeapObjectRequest());
  return value_.immediate;
}

// -----------------------------------------------------------------------------
// RelocInfo.

void RelocInfo::apply(intptr_t delta) {
  if (IsInternalReference(rmode_)) {
    // Absolute code pointer inside code object moves with the code object.
    Assembler::RelocateInternalReference(rmode_, pc_, delta);
  } else {
    DCHECK(IsRelativeCodeTarget(rmode_));
    Assembler::RelocateRelativeReference(rmode_, pc_, delta);
  }
}

Address RelocInfo::target_address() {
  DCHECK(IsCodeTargetMode(rmode_) || IsRuntimeEntry(rmode_) ||
         IsWasmCall(rmode_));
  return Assembler::target_address_at(pc_, constant_pool_);
}

Address RelocInfo::target_address_address() {
  DCHECK(HasTargetAddressAddress());
  // Read the address of the word containing the target_address in an
  // instruction stream.
  // The only architecture-independent user of this function is the serializer.
  // The serializer uses it to find out how many raw bytes of instruction to
  // output before the next target.
  // For an instruction like LUI/ORI where the target bits are mixed into the
  // instruction bits, the size of the target will be zero, indicating that the
  // serializer should not step forward in memory after a target is resolved
  // and written. In this case the target_address_address function should
  // return the end of the instructions to be patched, allowing the
  // deserializer to deserialize the instructions as raw bytes and put them in
  // place, ready to be patched with the target. After jump optimization,
  // that is the address of the instruction that follows J/JAL/JR/JALR
  // instruction.
  return pc_ + Assembler::kInstructionsFor64BitConstant * kInstrSize;
}

Address RelocInfo::constant_pool_entry_address() { UNREACHABLE(); }

int RelocInfo::target_address_size() { return Assembler::kSpecialTargetSize; }

void Assembler::deserialization_set_special_target_at(
    Address instruction_payload, Code code, Address target) {
  set_target_address_at(instruction_payload,
                        !code.is_null() ? code.constant_pool() : kNullAddress,
                        target);
}

int Assembler::deserialization_special_target_size(
    Address instruction_payload) {
  return kSpecialTargetSize;
}

void Assembler::deserialization_set_target_internal_reference_at(
    Address pc, Address target, RelocInfo::Mode mode) {
  WriteUnalignedValue<Address>(pc, target);
}

HeapObject RelocInfo::target_object(PtrComprCageBase cage_base) {
  DCHECK(IsCodeTarget(rmode_) || IsFullEmbeddedObject(rmode_) ||
         IsDataEmbeddedObject(rmode_));
  if (IsDataEmbeddedObject(rmode_)) {
    return HeapObject::cast(Object(ReadUnalignedValue<Address>(pc_)));
  }
  return HeapObject::cast(
      Object(Assembler::target_address_at(pc_, constant_pool_)));
}

Handle<HeapObject> RelocInfo::target_object_handle(Assembler* origin) {
  if (IsDataEmbeddedObject(rmode_)) {
    return Handle<HeapObject>::cast(ReadUnalignedValue<Handle<Object>>(pc_));
  } else if (IsCodeTarget(rmode_) || IsFullEmbeddedObject(rmode_)) {
    return Handle<HeapObject>(reinterpret_cast<Address*>(
        Assembler::target_address_at(pc_, constant_pool_)));
  } else {
    DCHECK(IsRelativeCodeTarget(rmode_));
    return origin->relative_code_target_object_handle_at(pc_);
  }
}

void RelocInfo::set_target_object(Heap* heap, HeapObject target,
                                  WriteBarrierMode write_barrier_mode,
                                  ICacheFlushMode icache_flush_mode) {
  DCHECK(IsCodeTarget(rmode_) || IsFullEmbeddedObject(rmode_) ||
         IsDataEmbeddedObject(rmode_));
  if (IsDataEmbeddedObject(rmode_)) {
    WriteUnalignedValue(pc_, target.ptr());
    // No need to flush icache since no instructions were changed.
  } else {
    Assembler::set_target_address_at(pc_, constant_pool_, target.ptr(),
                                     icache_flush_mode);
  }
  if (write_barrier_mode == UPDATE_WRITE_BARRIER && !host().is_null() &&
      !FLAG_disable_write_barriers) {
    WriteBarrierForCode(host(), this, target);
  }
}

Address RelocInfo::target_external_reference() {
  DCHECK(rmode_ == EXTERNAL_REFERENCE);
  return Assembler::target_address_at(pc_, constant_pool_);
}

void RelocInfo::set_target_external_reference(
    Address target, ICacheFlushMode icache_flush_mode) {
  DCHECK(rmode_ == RelocInfo::EXTERNAL_REFERENCE);
  Assembler::set_target_address_at(pc_, constant_pool_, target,
                                   icache_flush_mode);
}

Address RelocInfo::target_internal_reference() {
  if (rmode_ == INTERNAL_REFERENCE) {
    return Memory<Address>(pc_);
  } else {
    UNREACHABLE();
  }
}

Address RelocInfo::target_internal_reference_address() {
  DCHECK(rmode_ == INTERNAL_REFERENCE);
  return pc_;
}

Handle<Code> Assembler::relative_code_target_object_handle_at(
    Address pc) const {
  Instr instr = Assembler::instr_at(pc);
  int32_t code_target_index = instr & kImm26Mask;
  code_target_index = ((code_target_index & 0x3ff) << 22 >> 6) |
                      ((code_target_index >> 10) & kImm16Mask);
  return GetCodeTarget(code_target_index);
}

Address RelocInfo::target_runtime_entry(Assembler* origin) {
  DCHECK(IsRuntimeEntry(rmode_));
  return target_address();
}

void RelocInfo::set_target_runtime_entry(Address target,
                                         WriteBarrierMode write_barrier_mode,
                                         ICacheFlushMode icache_flush_mode) {
  DCHECK(IsRuntimeEntry(rmode_));
  if (target_address() != target)
    set_target_address(target, write_barrier_mode, icache_flush_mode);
}

Address RelocInfo::target_off_heap_target() {
  DCHECK(IsOffHeapTarget(rmode_));
  return Assembler::target_address_at(pc_, constant_pool_);
}

void RelocInfo::WipeOut() {
  DCHECK(IsFullEmbeddedObject(rmode_) || IsCodeTarget(rmode_) ||
         IsRuntimeEntry(rmode_) || IsExternalReference(rmode_) ||
         IsInternalReference(rmode_) || IsOffHeapTarget(rmode_));
  if (IsInternalReference(rmode_)) {
    Memory<Address>(pc_) = kNullAddress;
  } else {
    Assembler::set_target_address_at(pc_, constant_pool_, kNullAddress);
  }
}

// -----------------------------------------------------------------------------
// Assembler.

void Assembler::CheckBuffer() {
  if (buffer_space() <= kGap) {
    GrowBuffer();
  }
}

void Assembler::EmitHelper(Instr x) {
  *reinterpret_cast<Instr*>(pc_) = x;
  pc_ += kInstrSize;
  CheckTrampolinePoolQuick();
}

template <>
inline void Assembler::EmitHelper(uint8_t x);

template <typename T>
void Assembler::EmitHelper(T x) {
  *reinterpret_cast<T*>(pc_) = x;
  pc_ += sizeof(x);
  CheckTrampolinePoolQuick();
}

template <>
void Assembler::EmitHelper(uint8_t x) {
  *reinterpret_cast<uint8_t*>(pc_) = x;
  pc_ += sizeof(x);
  if (reinterpret_cast<intptr_t>(pc_) % kInstrSize == 0) {
    CheckTrampolinePoolQuick();
  }
}

void Assembler::emit(Instr x) {
  if (!is_buffer_growth_blocked()) {
    CheckBuffer();
  }
  EmitHelper(x);
}

void Assembler::emit(uint64_t data) {
  //  CheckForEmitInForbiddenSlot();
  if (!is_buffer_growth_blocked()) {
    CheckBuffer();
  }
  EmitHelper(data);
}

EnsureSpace::EnsureSpace(Assembler* assembler) { assembler->CheckBuffer(); }

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_LOONG64_ASSEMBLER_LOONG64_INL_H_
