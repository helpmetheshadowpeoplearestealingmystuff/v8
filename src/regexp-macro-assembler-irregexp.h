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

#ifndef V8_REGEXP_MACRO_ASSEMBLER_IRREGEXP_H_
#define V8_REGEXP_MACRO_ASSEMBLER_IRREGEXP_H_

namespace v8 { namespace internal {


class RegExpMacroAssemblerIrregexp: public RegExpMacroAssembler {
 public:
  // Create an assembler. Instructions and relocation information are emitted
  // into a buffer, with the instructions starting from the beginning and the
  // relocation information starting from the end of the buffer. See CodeDesc
  // for a detailed comment on the layout (globals.h).
  //
  // If the provided buffer is NULL, the assembler allocates and grows its own
  // buffer, and buffer_size determines the initial buffer size. The buffer is
  // owned by the assembler and deallocated upon destruction of the assembler.
  //
  // If the provided buffer is not NULL, the assembler uses the provided buffer
  // for code generation and assumes its size to be buffer_size. If the buffer
  // is too small, a fatal error occurs. No deallocation of the buffer is done
  // upon destruction of the assembler.
  explicit RegExpMacroAssemblerIrregexp(Vector<byte>);
  virtual ~RegExpMacroAssemblerIrregexp();
  virtual void Bind(Label* label);
  virtual void EmitOrLink(Label* label);
  virtual void AdvanceCurrentPosition(int by);  // Signed cp change.
  virtual void PopCurrentPosition();
  virtual void PushCurrentPosition();
  virtual void Backtrack();
  virtual void GoTo(Label* label);
  virtual void PushBacktrack(Label* label);
  virtual void Succeed();
  virtual void Fail();
  virtual void PopRegister(int register_index);
  virtual void PushRegister(int register_index);
  virtual void AdvanceRegister(int reg, int by);  // r[reg] += by.
  virtual void SetRegister(int register_index, int to);
  virtual void WriteCurrentPositionToRegister(int reg);
  virtual void ReadCurrentPositionFromRegister(int reg);
  virtual void WriteStackPointerToRegister(int reg);
  virtual void ReadStackPointerFromRegister(int reg);
  virtual void LoadCurrentCharacter(int cp_offset, Label* on_end_of_input);
  virtual void CheckCharacterLT(uc16 limit, Label* on_less);
  virtual void CheckCharacterGT(uc16 limit, Label* on_greater);
  virtual void CheckCharacter(uc16 c, Label* on_equal);
  virtual void CheckNotCharacter(uc16 c, Label* on_not_equal);
  virtual void CheckNotCharacterAfterOr(uc16 c, uc16 mask, Label* on_not_equal);
  virtual void CheckNotCharacterAfterMinusOr(uc16 c,
                                             uc16 mask,
                                             Label* on_not_equal);
  virtual void CheckNotBackReference(int start_reg, Label* on_no_match);
  virtual void CheckNotBackReferenceIgnoreCase(int start_reg,
                                               Label* on_no_match);
  virtual void CheckNotRegistersEqual(int reg1, int reg2, Label* on_not_equal);
  virtual void CheckCharacters(Vector<const uc16> str,
                               int cp_offset,
                               Label* on_failure);
  virtual void CheckCurrentPosition(int register_index, Label* on_equal);
  virtual void CheckBitmap(uc16 start, Label* bitmap, Label* on_zero);
  virtual void DispatchHalfNibbleMap(uc16 start,
                                     Label* half_nibble_map,
                                     const Vector<Label*>& destinations);
  virtual void DispatchByteMap(uc16 start,
                               Label* byte_map,
                               const Vector<Label*>& destinations);
  virtual void DispatchHighByteMap(byte start,
                                   Label* byte_map,
                                   const Vector<Label*>& destinations);
  virtual void IfRegisterLT(int register_index, int comparand, Label* if_lt);
  virtual void IfRegisterGE(int register_index, int comparand, Label* if_ge);

  virtual IrregexpImplementation Implementation();
  virtual Handle<Object> GetCode();
 private:
  void Expand();
  // Code and bitmap emission.
  inline void Emit32(uint32_t x);
  inline void Emit16(uint32_t x);
  inline void Emit(uint32_t x);
  // Bytecode buffer.
  int length();
  void Copy(Address a);



  // The buffer into which code and relocation info are generated.
  Vector<byte> buffer_;
  // The program counter.
  int pc_;
  // True if the assembler owns the buffer, false if buffer is external.
  bool own_buffer_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(RegExpMacroAssemblerIrregexp);
};

} }  // namespace v8::internal

#endif  // V8_REGEXP_MACRO_ASSEMBLER_IRREGEXP_H_
