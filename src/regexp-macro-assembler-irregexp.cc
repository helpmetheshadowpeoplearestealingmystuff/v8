// Copyright 2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"
#include "ast.h"
#include "bytecodes-irregexp.h"
#include "regexp-macro-assembler.h"
#include "regexp-macro-assembler-irregexp.h"
#include "regexp-macro-assembler-irregexp-inl.h"


namespace v8 { namespace internal {


RegExpMacroAssemblerIrregexp::RegExpMacroAssemblerIrregexp(Vector<byte> buffer)
    : buffer_(buffer),
      pc_(0),
      own_buffer_(false) {
}


RegExpMacroAssemblerIrregexp::~RegExpMacroAssemblerIrregexp() {
  if (backtrack_.is_linked()) backtrack_.Unuse();
}


RegExpMacroAssemblerIrregexp::IrregexpImplementation
RegExpMacroAssemblerIrregexp::Implementation() {
  return kBytecodeImplementation;
}


void RegExpMacroAssemblerIrregexp::Bind(Label* l) {
  ASSERT(!l->is_bound());
  if (l->is_linked()) {
    int pos = l->pos();
    while (pos != 0) {
      int fixup = pos;
      pos = Load32(buffer_.start() + fixup);
      Store32(buffer_.start() + fixup, pc_);
    }
  }
  l->bind_to(pc_);
}


void RegExpMacroAssemblerIrregexp::EmitOrLink(Label* l) {
  if (l == NULL) l = &backtrack_;
  if (l->is_bound()) {
    Emit32(l->pos());
  } else {
    int pos = 0;
    if (l->is_linked()) {
      pos = l->pos();
    }
    l->link_to(pc_);
    Emit32(pos);
  }
}


void RegExpMacroAssemblerIrregexp::PopRegister(int register_index) {
  Emit(BC_POP_REGISTER);
  Emit(register_index);
}


void RegExpMacroAssemblerIrregexp::PushRegister(int register_index) {
  ASSERT(register_index >= 0);
  Emit(BC_PUSH_REGISTER);
  Emit(register_index);
}


void RegExpMacroAssemblerIrregexp::WriteCurrentPositionToRegister(
    int register_index, int cp_offset) {
  ASSERT(register_index >= 0);
  Emit(BC_SET_REGISTER_TO_CP);
  Emit(register_index);
  Emit32(cp_offset);  // Current position offset.
}


void RegExpMacroAssemblerIrregexp::ReadCurrentPositionFromRegister(
    int register_index) {
  ASSERT(register_index >= 0);
  Emit(BC_SET_CP_TO_REGISTER);
  Emit(register_index);
}


void RegExpMacroAssemblerIrregexp::WriteStackPointerToRegister(
    int register_index) {
  ASSERT(register_index >= 0);
  Emit(BC_SET_REGISTER_TO_SP);
  Emit(register_index);
}


void RegExpMacroAssemblerIrregexp::ReadStackPointerFromRegister(
    int register_index) {
  ASSERT(register_index >= 0);
  Emit(BC_SET_SP_TO_REGISTER);
  Emit(register_index);
}


void RegExpMacroAssemblerIrregexp::SetRegister(int register_index, int to) {
  ASSERT(register_index >= 0);
  Emit(BC_SET_REGISTER);
  Emit(register_index);
  Emit32(to);
}


void RegExpMacroAssemblerIrregexp::AdvanceRegister(int register_index, int by) {
  ASSERT(register_index >= 0);
  Emit(BC_ADVANCE_REGISTER);
  Emit(register_index);
  Emit32(by);
}


void RegExpMacroAssemblerIrregexp::PopCurrentPosition() {
  Emit(BC_POP_CP);
}


void RegExpMacroAssemblerIrregexp::PushCurrentPosition() {
  Emit(BC_PUSH_CP);
  Emit32(0);  // Current position offset.
}


void RegExpMacroAssemblerIrregexp::Backtrack() {
  Emit(BC_POP_BT);
}


void RegExpMacroAssemblerIrregexp::GoTo(Label* l) {
  Emit(BC_GOTO);
  EmitOrLink(l);
}


void RegExpMacroAssemblerIrregexp::PushBacktrack(Label* l) {
  Emit(BC_PUSH_BT);
  EmitOrLink(l);
}


void RegExpMacroAssemblerIrregexp::Succeed() {
  Emit(BC_SUCCEED);
}


void RegExpMacroAssemblerIrregexp::Fail() {
  Emit(BC_FAIL);
}


void RegExpMacroAssemblerIrregexp::AdvanceCurrentPosition(int by) {
  Emit(BC_ADVANCE_CP);
  Emit32(by);
}


void RegExpMacroAssemblerIrregexp::CheckGreedyLoop(
      Label* on_tos_equals_current_position) {
  Emit(BC_CHECK_GREEDY);
  EmitOrLink(on_tos_equals_current_position);
}


void RegExpMacroAssemblerIrregexp::LoadCurrentCharacter(int cp_offset,
                                                        Label* on_failure,
                                                        bool check_bounds,
                                                        int characters) {
  int bytecode;
  if (check_bounds) {
    if (characters == 4) {
      bytecode = BC_LOAD_4_CURRENT_CHARS;
    } else if (characters == 2) {
      bytecode = BC_LOAD_2_CURRENT_CHARS;
    } else {
      ASSERT(characters == 1);
      bytecode = BC_LOAD_CURRENT_CHAR;
    }
  } else {
    if (characters == 4) {
      bytecode = BC_LOAD_4_CURRENT_CHARS_UNCHECKED;
    } else if (characters == 2) {
      bytecode = BC_LOAD_2_CURRENT_CHARS_UNCHECKED;
    } else {
      ASSERT(characters == 1);
      bytecode = BC_LOAD_CURRENT_CHAR_UNCHECKED;
    }
  }
  Emit(bytecode);
  Emit32(cp_offset);
  if (check_bounds) EmitOrLink(on_failure);
}


void RegExpMacroAssemblerIrregexp::CheckCharacterLT(uc16 limit,
                                                    Label* on_less) {
  Emit(BC_CHECK_LT);
  Emit16(limit);
  EmitOrLink(on_less);
}


void RegExpMacroAssemblerIrregexp::CheckCharacterGT(uc16 limit,
                                                    Label* on_greater) {
  Emit(BC_CHECK_GT);
  Emit16(limit);
  EmitOrLink(on_greater);
}


void RegExpMacroAssemblerIrregexp::CheckCharacter(uint32_t c, Label* on_equal) {
  Emit(BC_CHECK_CHAR);
  Emit32(c);
  EmitOrLink(on_equal);
}


void RegExpMacroAssemblerIrregexp::CheckNotAtStart(Label* on_not_at_start) {
  Emit(BC_CHECK_NOT_AT_START);
  EmitOrLink(on_not_at_start);
}


void RegExpMacroAssemblerIrregexp::CheckNotCharacter(uint32_t c,
                                                     Label* on_not_equal) {
  Emit(BC_CHECK_NOT_CHAR);
  Emit32(c);
  EmitOrLink(on_not_equal);
}


void RegExpMacroAssemblerIrregexp::CheckCharacterAfterAnd(
    uint32_t c,
    uint32_t mask,
    Label* on_equal) {
  Emit(BC_AND_CHECK_CHAR);
  Emit32(c);
  Emit32(mask);
  EmitOrLink(on_equal);
}


void RegExpMacroAssemblerIrregexp::CheckNotCharacterAfterAnd(
    uint32_t c,
    uint32_t mask,
    Label* on_not_equal) {
  Emit(BC_AND_CHECK_NOT_CHAR);
  Emit32(c);
  Emit32(mask);
  EmitOrLink(on_not_equal);
}


void RegExpMacroAssemblerIrregexp::CheckNotCharacterAfterMinusAnd(
    uc16 c,
    uc16 minus,
    uc16 mask,
    Label* on_not_equal) {
  Emit(BC_MINUS_AND_CHECK_NOT_CHAR);
  Emit16(c);
  Emit16(minus);
  Emit16(mask);
  EmitOrLink(on_not_equal);
}


void RegExpMacroAssemblerIrregexp::CheckNotBackReference(int start_reg,
                                                         Label* on_not_equal) {
  Emit(BC_CHECK_NOT_BACK_REF);
  Emit(start_reg);
  EmitOrLink(on_not_equal);
}


void RegExpMacroAssemblerIrregexp::CheckNotBackReferenceIgnoreCase(
    int start_reg,
    Label* on_not_equal) {
  Emit(BC_CHECK_NOT_BACK_REF_NO_CASE);
  Emit(start_reg);
  EmitOrLink(on_not_equal);
}


void RegExpMacroAssemblerIrregexp::CheckNotRegistersEqual(int reg1,
                                                          int reg2,
                                                          Label* on_not_equal) {
  Emit(BC_CHECK_NOT_REGS_EQUAL);
  Emit(reg1);
  Emit(reg2);
  EmitOrLink(on_not_equal);
}


void RegExpMacroAssemblerIrregexp::CheckBitmap(uc16 start,
                                           Label* bitmap,
                                           Label* on_zero) {
  UNIMPLEMENTED();
}


void RegExpMacroAssemblerIrregexp::DispatchHalfNibbleMap(
    uc16 start,
    Label* half_nibble_map,
    const Vector<Label*>& table) {
  UNIMPLEMENTED();
}


void RegExpMacroAssemblerIrregexp::DispatchByteMap(
    uc16 start,
    Label* byte_map,
    const Vector<Label*>& table) {
  UNIMPLEMENTED();
}


void RegExpMacroAssemblerIrregexp::DispatchHighByteMap(
    byte start,
    Label* byte_map,
    const Vector<Label*>& table) {
  UNIMPLEMENTED();
}


void RegExpMacroAssemblerIrregexp::CheckCharacters(
  Vector<const uc16> str,
  int cp_offset,
  Label* on_failure,
  bool check_end_of_string) {
  // It is vital that this loop is backwards due to the unchecked character
  // load below.
  for (int i = str.length() - 1; i >= 0; i--) {
    if (check_end_of_string && i == str.length() - 1) {
      Emit(BC_LOAD_CURRENT_CHAR);
      Emit32(cp_offset + i);
      EmitOrLink(on_failure);
    } else {
      Emit(BC_LOAD_CURRENT_CHAR_UNCHECKED);
      Emit32(cp_offset + i);
    }
    Emit(BC_CHECK_NOT_CHAR);
    Emit32(str[i]);
    EmitOrLink(on_failure);
  }
}


void RegExpMacroAssemblerIrregexp::IfRegisterLT(int register_index,
                                                int comparand,
                                                Label* on_less_than) {
  ASSERT(comparand >= 0 && comparand <= 65535);
  Emit(BC_CHECK_REGISTER_LT);
  Emit(register_index);
  Emit16(comparand);
  EmitOrLink(on_less_than);
}


void RegExpMacroAssemblerIrregexp::IfRegisterGE(int register_index,
                                                int comparand,
                                                Label* on_greater_or_equal) {
  ASSERT(comparand >= 0 && comparand <= 65535);
  Emit(BC_CHECK_REGISTER_GE);
  Emit(register_index);
  Emit16(comparand);
  EmitOrLink(on_greater_or_equal);
}


Handle<Object> RegExpMacroAssemblerIrregexp::GetCode(Handle<String> source) {
  Bind(&backtrack_);
  Emit(BC_POP_BT);
  Handle<ByteArray> array = Factory::NewByteArray(length());
  Copy(array->GetDataStartAddress());
  return array;
}


int RegExpMacroAssemblerIrregexp::length() {
  return pc_;
}


void RegExpMacroAssemblerIrregexp::Copy(Address a) {
  memcpy(a, buffer_.start(), length());
}


void RegExpMacroAssemblerIrregexp::Expand() {
  bool old_buffer_was_our_own = own_buffer_;
  Vector<byte> old_buffer = buffer_;
  buffer_ = Vector<byte>::New(old_buffer.length() * 2);
  own_buffer_ = true;
  memcpy(buffer_.start(), old_buffer.start(), old_buffer.length());
  if (old_buffer_was_our_own) {
    old_buffer.Dispose();
  }
}


} }  // namespace v8::internal
