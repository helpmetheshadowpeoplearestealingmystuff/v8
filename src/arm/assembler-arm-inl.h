// Copyright (c) 1994-2006 Sun Microsystems Inc.
// All Rights Reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// - Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// - Redistribution in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the
// distribution.
//
// - Neither the name of Sun Microsystems or the names of contributors may
// be used to endorse or promote products derived from this software without
// specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.

// The original source code covered by the above license above has been modified
// significantly by Google Inc.
// Copyright 2006-2008 the V8 project authors. All rights reserved.

#ifndef V8_ARM_ASSEMBLER_ARM_INL_H_
#define V8_ARM_ASSEMBLER_ARM_INL_H_

#include "arm/assembler-arm.h"
#include "cpu.h"


namespace v8 {
namespace internal {

Condition NegateCondition(Condition cc) {
  ASSERT(cc != al);
  return static_cast<Condition>(cc ^ ne);
}


void RelocInfo::apply(int delta) {
  if (RelocInfo::IsInternalReference(rmode_)) {
    // absolute code pointer inside code object moves with the code object.
    int32_t* p = reinterpret_cast<int32_t*>(pc_);
    *p += delta;  // relocate entry
  }
  // We do not use pc relative addressing on ARM, so there is
  // nothing else to do.
}


Address RelocInfo::target_address() {
  ASSERT(IsCodeTarget(rmode_) || rmode_ == RUNTIME_ENTRY);
  return Assembler::target_address_at(pc_);
}


Address RelocInfo::target_address_address() {
  ASSERT(IsCodeTarget(rmode_) || rmode_ == RUNTIME_ENTRY);
  return reinterpret_cast<Address>(Assembler::target_address_address_at(pc_));
}


void RelocInfo::set_target_address(Address target) {
  ASSERT(IsCodeTarget(rmode_) || rmode_ == RUNTIME_ENTRY);
  Assembler::set_target_address_at(pc_, target);
}


Object* RelocInfo::target_object() {
  ASSERT(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  return reinterpret_cast<Object*>(Assembler::target_address_at(pc_));
}


Object** RelocInfo::target_object_address() {
  ASSERT(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  return reinterpret_cast<Object**>(Assembler::target_address_address_at(pc_));
}


void RelocInfo::set_target_object(Object* target) {
  ASSERT(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  Assembler::set_target_address_at(pc_, reinterpret_cast<Address>(target));
}


Address* RelocInfo::target_reference_address() {
  ASSERT(rmode_ == EXTERNAL_REFERENCE);
  return reinterpret_cast<Address*>(Assembler::target_address_address_at(pc_));
}


Address RelocInfo::call_address() {
  ASSERT(IsCallInstruction());
  UNIMPLEMENTED();
  return NULL;
}


void RelocInfo::set_call_address(Address target) {
  ASSERT(IsCallInstruction());
  UNIMPLEMENTED();
}


Object* RelocInfo::call_object() {
  ASSERT(IsCallInstruction());
  UNIMPLEMENTED();
  return NULL;
}


Object** RelocInfo::call_object_address() {
  ASSERT(IsCallInstruction());
  UNIMPLEMENTED();
  return NULL;
}


void RelocInfo::set_call_object(Object* target) {
  ASSERT(IsCallInstruction());
  UNIMPLEMENTED();
}


bool RelocInfo::IsCallInstruction() {
  UNIMPLEMENTED();
  return false;
}


Operand::Operand(int32_t immediate, RelocInfo::Mode rmode)  {
  rm_ = no_reg;
  imm32_ = immediate;
  rmode_ = rmode;
}


Operand::Operand(const char* s) {
  rm_ = no_reg;
  imm32_ = reinterpret_cast<int32_t>(s);
  rmode_ = RelocInfo::EMBEDDED_STRING;
}


Operand::Operand(const ExternalReference& f)  {
  rm_ = no_reg;
  imm32_ = reinterpret_cast<int32_t>(f.address());
  rmode_ = RelocInfo::EXTERNAL_REFERENCE;
}


Operand::Operand(Object** opp) {
  rm_ = no_reg;
  imm32_ = reinterpret_cast<int32_t>(opp);
  rmode_ = RelocInfo::NONE;
}


Operand::Operand(Context** cpp) {
  rm_ = no_reg;
  imm32_ = reinterpret_cast<int32_t>(cpp);
  rmode_ = RelocInfo::NONE;
}


Operand::Operand(Smi* value) {
  rm_ = no_reg;
  imm32_ =  reinterpret_cast<intptr_t>(value);
  rmode_ = RelocInfo::NONE;
}


Operand::Operand(Register rm) {
  rm_ = rm;
  rs_ = no_reg;
  shift_op_ = LSL;
  shift_imm_ = 0;
}


bool Operand::is_reg() const {
  return rm_.is_valid() &&
         rs_.is(no_reg) &&
         shift_op_ == LSL &&
         shift_imm_ == 0;
}


void Assembler::CheckBuffer() {
  if (buffer_space() <= kGap) {
    GrowBuffer();
  }
  if (pc_offset() > next_buffer_check_) {
    CheckConstPool(false, true);
  }
}


void Assembler::emit(Instr x) {
  CheckBuffer();
  *reinterpret_cast<Instr*>(pc_) = x;
  pc_ += kInstrSize;
}


Address Assembler::target_address_address_at(Address pc) {
  Instr instr = Memory::int32_at(pc);
  // Verify that the instruction at pc is a ldr<cond> <Rd>, [pc +/- offset_12].
  ASSERT((instr & 0x0f7f0000) == 0x051f0000);
  int offset = instr & 0xfff;  // offset_12 is unsigned
  if ((instr & (1 << 23)) == 0) offset = -offset;  // U bit defines offset sign
  // Verify that the constant pool comes after the instruction referencing it.
  ASSERT(offset >= -4);
  return pc + offset + 8;
}


Address Assembler::target_address_at(Address pc) {
  return Memory::Address_at(target_address_address_at(pc));
}


void Assembler::set_target_address_at(Address pc, Address target) {
  Memory::Address_at(target_address_address_at(pc)) = target;
  // Intuitively, we would think it is necessary to flush the instruction cache
  // after patching a target address in the code as follows:
  //   CPU::FlushICache(pc, sizeof(target));
  // However, on ARM, no instruction was actually patched by the assignment
  // above; the target address is not part of an instruction, it is patched in
  // the constant pool and is read via a data access; the instruction accessing
  // this address in the constant pool remains unchanged.
}

} }  // namespace v8::internal

#endif  // V8_ARM_ASSEMBLER_ARM_INL_H_
