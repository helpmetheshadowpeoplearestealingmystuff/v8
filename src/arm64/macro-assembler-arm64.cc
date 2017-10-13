// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_ARM64

#include "src/assembler.h"
#include "src/base/bits.h"
#include "src/base/division-by-constant.h"
#include "src/bootstrapper.h"
#include "src/callable.h"
#include "src/codegen.h"
#include "src/debug/debug.h"
#include "src/external-reference-table.h"
#include "src/frame-constants.h"
#include "src/frames-inl.h"
#include "src/heap/heap-inl.h"
#include "src/register-configuration.h"
#include "src/runtime/runtime.h"

#include "src/arm64/macro-assembler-arm64-inl.h"
#include "src/arm64/macro-assembler-arm64.h"  // Cannot be the first include

namespace v8 {
namespace internal {

MacroAssembler::MacroAssembler(Isolate* isolate, byte* buffer,
                               unsigned buffer_size,
                               CodeObjectRequired create_code_object)
    : TurboAssembler(isolate, buffer, buffer_size, create_code_object) {}

CPURegList TurboAssembler::DefaultTmpList() { return CPURegList(ip0, ip1); }

CPURegList TurboAssembler::DefaultFPTmpList() {
  return CPURegList(fp_scratch1, fp_scratch2);
}

TurboAssembler::TurboAssembler(Isolate* isolate, void* buffer, int buffer_size,
                               CodeObjectRequired create_code_object)
    : Assembler(isolate, buffer, buffer_size),
      isolate_(isolate),
#if DEBUG
      allow_macro_instructions_(true),
#endif
      tmp_list_(DefaultTmpList()),
      fptmp_list_(DefaultFPTmpList()),
      sp_(jssp),
      use_real_aborts_(true) {
  if (create_code_object == CodeObjectRequired::kYes) {
    code_object_ =
        Handle<HeapObject>::New(isolate->heap()->undefined_value(), isolate);
  }
}

int TurboAssembler::RequiredStackSizeForCallerSaved(SaveFPRegsMode fp_mode,
                                                    Register exclusion1,
                                                    Register exclusion2,
                                                    Register exclusion3) const {
  int bytes = 0;
  auto list = kCallerSaved;
  list.Remove(exclusion1, exclusion2, exclusion3);
  bytes += list.Count() * kXRegSizeInBits / 8;

  if (fp_mode == kSaveFPRegs) {
    bytes += kCallerSavedV.Count() * kDRegSizeInBits / 8;
  }
  return bytes;
}

int TurboAssembler::PushCallerSaved(SaveFPRegsMode fp_mode, Register exclusion1,
                                    Register exclusion2, Register exclusion3) {
  int bytes = 0;
  auto list = kCallerSaved;
  list.Remove(exclusion1, exclusion2, exclusion3);
  PushCPURegList(list);
  bytes += list.Count() * kXRegSizeInBits / 8;

  if (fp_mode == kSaveFPRegs) {
    PushCPURegList(kCallerSavedV);
    bytes += kCallerSavedV.Count() * kDRegSizeInBits / 8;
  }
  return bytes;
}

int TurboAssembler::PopCallerSaved(SaveFPRegsMode fp_mode, Register exclusion1,
                                   Register exclusion2, Register exclusion3) {
  int bytes = 0;
  if (fp_mode == kSaveFPRegs) {
    PopCPURegList(kCallerSavedV);
    bytes += kCallerSavedV.Count() * kDRegSizeInBits / 8;
  }

  auto list = kCallerSaved;
  list.Remove(exclusion1, exclusion2, exclusion3);
  PopCPURegList(list);
  bytes += list.Count() * kXRegSizeInBits / 8;

  return bytes;
}

void TurboAssembler::LogicalMacro(const Register& rd, const Register& rn,
                                  const Operand& operand, LogicalOp op) {
  UseScratchRegisterScope temps(this);

  if (operand.NeedsRelocation(this)) {
    Register temp = temps.AcquireX();
    Ldr(temp, operand.immediate());
    Logical(rd, rn, temp, op);

  } else if (operand.IsImmediate()) {
    int64_t immediate = operand.ImmediateValue();
    unsigned reg_size = rd.SizeInBits();

    // If the operation is NOT, invert the operation and immediate.
    if ((op & NOT) == NOT) {
      op = static_cast<LogicalOp>(op & ~NOT);
      immediate = ~immediate;
    }

    // Ignore the top 32 bits of an immediate if we're moving to a W register.
    if (rd.Is32Bits()) {
      // Check that the top 32 bits are consistent.
      DCHECK(((immediate >> kWRegSizeInBits) == 0) ||
             ((immediate >> kWRegSizeInBits) == -1));
      immediate &= kWRegMask;
    }

    DCHECK(rd.Is64Bits() || is_uint32(immediate));

    // Special cases for all set or all clear immediates.
    if (immediate == 0) {
      switch (op) {
        case AND:
          Mov(rd, 0);
          return;
        case ORR:  // Fall through.
        case EOR:
          Mov(rd, rn);
          return;
        case ANDS:  // Fall through.
        case BICS:
          break;
        default:
          UNREACHABLE();
      }
    } else if ((rd.Is64Bits() && (immediate == -1L)) ||
               (rd.Is32Bits() && (immediate == 0xffffffffL))) {
      switch (op) {
        case AND:
          Mov(rd, rn);
          return;
        case ORR:
          Mov(rd, immediate);
          return;
        case EOR:
          Mvn(rd, rn);
          return;
        case ANDS:  // Fall through.
        case BICS:
          break;
        default:
          UNREACHABLE();
      }
    }

    unsigned n, imm_s, imm_r;
    if (IsImmLogical(immediate, reg_size, &n, &imm_s, &imm_r)) {
      // Immediate can be encoded in the instruction.
      LogicalImmediate(rd, rn, n, imm_s, imm_r, op);
    } else {
      // Immediate can't be encoded: synthesize using move immediate.
      Register temp = temps.AcquireSameSizeAs(rn);

      // If the left-hand input is the stack pointer, we can't pre-shift the
      // immediate, as the encoding won't allow the subsequent post shift.
      PreShiftImmMode mode = rn.Is(csp) ? kNoShift : kAnyShift;
      Operand imm_operand = MoveImmediateForShiftedOp(temp, immediate, mode);

      if (rd.Is(csp)) {
        // If rd is the stack pointer we cannot use it as the destination
        // register so we use the temp register as an intermediate again.
        Logical(temp, rn, imm_operand, op);
        Mov(csp, temp);
        AssertStackConsistency();
      } else {
        Logical(rd, rn, imm_operand, op);
      }
    }

  } else if (operand.IsExtendedRegister()) {
    DCHECK(operand.reg().SizeInBits() <= rd.SizeInBits());
    // Add/sub extended supports shift <= 4. We want to support exactly the
    // same modes here.
    DCHECK(operand.shift_amount() <= 4);
    DCHECK(operand.reg().Is64Bits() ||
           ((operand.extend() != UXTX) && (operand.extend() != SXTX)));
    Register temp = temps.AcquireSameSizeAs(rn);
    EmitExtendShift(temp, operand.reg(), operand.extend(),
                    operand.shift_amount());
    Logical(rd, rn, temp, op);

  } else {
    // The operand can be encoded in the instruction.
    DCHECK(operand.IsShiftedRegister());
    Logical(rd, rn, operand, op);
  }
}

void TurboAssembler::Mov(const Register& rd, uint64_t imm) {
  DCHECK(allow_macro_instructions());
  DCHECK(is_uint32(imm) || is_int32(imm) || rd.Is64Bits());
  DCHECK(!rd.IsZero());

  // TODO(all) extend to support more immediates.
  //
  // Immediates on Aarch64 can be produced using an initial value, and zero to
  // three move keep operations.
  //
  // Initial values can be generated with:
  //  1. 64-bit move zero (movz).
  //  2. 32-bit move inverted (movn).
  //  3. 64-bit move inverted.
  //  4. 32-bit orr immediate.
  //  5. 64-bit orr immediate.
  // Move-keep may then be used to modify each of the 16-bit half-words.
  //
  // The code below supports all five initial value generators, and
  // applying move-keep operations to move-zero and move-inverted initial
  // values.

  // Try to move the immediate in one instruction, and if that fails, switch to
  // using multiple instructions.
  if (!TryOneInstrMoveImmediate(rd, imm)) {
    unsigned reg_size = rd.SizeInBits();

    // Generic immediate case. Imm will be represented by
    //   [imm3, imm2, imm1, imm0], where each imm is 16 bits.
    // A move-zero or move-inverted is generated for the first non-zero or
    // non-0xffff immX, and a move-keep for subsequent non-zero immX.

    uint64_t ignored_halfword = 0;
    bool invert_move = false;
    // If the number of 0xffff halfwords is greater than the number of 0x0000
    // halfwords, it's more efficient to use move-inverted.
    if (CountClearHalfWords(~imm, reg_size) >
        CountClearHalfWords(imm, reg_size)) {
      ignored_halfword = 0xffffL;
      invert_move = true;
    }

    // Mov instructions can't move immediate values into the stack pointer, so
    // set up a temporary register, if needed.
    UseScratchRegisterScope temps(this);
    Register temp = rd.IsSP() ? temps.AcquireSameSizeAs(rd) : rd;

    // Iterate through the halfwords. Use movn/movz for the first non-ignored
    // halfword, and movk for subsequent halfwords.
    DCHECK((reg_size % 16) == 0);
    bool first_mov_done = false;
    for (int i = 0; i < (rd.SizeInBits() / 16); i++) {
      uint64_t imm16 = (imm >> (16 * i)) & 0xffffL;
      if (imm16 != ignored_halfword) {
        if (!first_mov_done) {
          if (invert_move) {
            movn(temp, (~imm16) & 0xffffL, 16 * i);
          } else {
            movz(temp, imm16, 16 * i);
          }
          first_mov_done = true;
        } else {
          // Construct a wider constant.
          movk(temp, imm16, 16 * i);
        }
      }
    }
    DCHECK(first_mov_done);

    // Move the temporary if the original destination register was the stack
    // pointer.
    if (rd.IsSP()) {
      mov(rd, temp);
      AssertStackConsistency();
    }
  }
}

void TurboAssembler::Mov(const Register& rd, const Operand& operand,
                         DiscardMoveMode discard_mode) {
  DCHECK(allow_macro_instructions());
  DCHECK(!rd.IsZero());

  // Provide a swap register for instructions that need to write into the
  // system stack pointer (and can't do this inherently).
  UseScratchRegisterScope temps(this);
  Register dst = (rd.IsSP()) ? temps.AcquireSameSizeAs(rd) : rd;

  if (operand.NeedsRelocation(this)) {
    Ldr(dst, operand);

  } else if (operand.IsImmediate()) {
    // Call the macro assembler for generic immediates.
    Mov(dst, operand.ImmediateValue());

  } else if (operand.IsShiftedRegister() && (operand.shift_amount() != 0)) {
    // Emit a shift instruction if moving a shifted register. This operation
    // could also be achieved using an orr instruction (like orn used by Mvn),
    // but using a shift instruction makes the disassembly clearer.
    EmitShift(dst, operand.reg(), operand.shift(), operand.shift_amount());

  } else if (operand.IsExtendedRegister()) {
    // Emit an extend instruction if moving an extended register. This handles
    // extend with post-shift operations, too.
    EmitExtendShift(dst, operand.reg(), operand.extend(),
                    operand.shift_amount());

  } else {
    // Otherwise, emit a register move only if the registers are distinct, or
    // if they are not X registers.
    //
    // Note that mov(w0, w0) is not a no-op because it clears the top word of
    // x0. A flag is provided (kDiscardForSameWReg) if a move between the same W
    // registers is not required to clear the top word of the X register. In
    // this case, the instruction is discarded.
    //
    // If csp is an operand, add #0 is emitted, otherwise, orr #0.
    if (!rd.Is(operand.reg()) || (rd.Is32Bits() &&
                                  (discard_mode == kDontDiscardForSameWReg))) {
      Assembler::mov(rd, operand.reg());
    }
    // This case can handle writes into the system stack pointer directly.
    dst = rd;
  }

  // Copy the result to the system stack pointer.
  if (!dst.Is(rd)) {
    DCHECK(rd.IsSP());
    Assembler::mov(rd, dst);
  }
}

void TurboAssembler::Movi16bitHelper(const VRegister& vd, uint64_t imm) {
  DCHECK(is_uint16(imm));
  int byte1 = (imm & 0xff);
  int byte2 = ((imm >> 8) & 0xff);
  if (byte1 == byte2) {
    movi(vd.Is64Bits() ? vd.V8B() : vd.V16B(), byte1);
  } else if (byte1 == 0) {
    movi(vd, byte2, LSL, 8);
  } else if (byte2 == 0) {
    movi(vd, byte1);
  } else if (byte1 == 0xff) {
    mvni(vd, ~byte2 & 0xff, LSL, 8);
  } else if (byte2 == 0xff) {
    mvni(vd, ~byte1 & 0xff);
  } else {
    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireW();
    movz(temp, imm);
    dup(vd, temp);
  }
}

void TurboAssembler::Movi32bitHelper(const VRegister& vd, uint64_t imm) {
  DCHECK(is_uint32(imm));

  uint8_t bytes[sizeof(imm)];
  memcpy(bytes, &imm, sizeof(imm));

  // All bytes are either 0x00 or 0xff.
  {
    bool all0orff = true;
    for (int i = 0; i < 4; ++i) {
      if ((bytes[i] != 0) && (bytes[i] != 0xff)) {
        all0orff = false;
        break;
      }
    }

    if (all0orff == true) {
      movi(vd.Is64Bits() ? vd.V1D() : vd.V2D(), ((imm << 32) | imm));
      return;
    }
  }

  // Of the 4 bytes, only one byte is non-zero.
  for (int i = 0; i < 4; i++) {
    if ((imm & (0xff << (i * 8))) == imm) {
      movi(vd, bytes[i], LSL, i * 8);
      return;
    }
  }

  // Of the 4 bytes, only one byte is not 0xff.
  for (int i = 0; i < 4; i++) {
    uint32_t mask = ~(0xff << (i * 8));
    if ((imm & mask) == mask) {
      mvni(vd, ~bytes[i] & 0xff, LSL, i * 8);
      return;
    }
  }

  // Immediate is of the form 0x00MMFFFF.
  if ((imm & 0xff00ffff) == 0x0000ffff) {
    movi(vd, bytes[2], MSL, 16);
    return;
  }

  // Immediate is of the form 0x0000MMFF.
  if ((imm & 0xffff00ff) == 0x000000ff) {
    movi(vd, bytes[1], MSL, 8);
    return;
  }

  // Immediate is of the form 0xFFMM0000.
  if ((imm & 0xff00ffff) == 0xff000000) {
    mvni(vd, ~bytes[2] & 0xff, MSL, 16);
    return;
  }
  // Immediate is of the form 0xFFFFMM00.
  if ((imm & 0xffff00ff) == 0xffff0000) {
    mvni(vd, ~bytes[1] & 0xff, MSL, 8);
    return;
  }

  // Top and bottom 16-bits are equal.
  if (((imm >> 16) & 0xffff) == (imm & 0xffff)) {
    Movi16bitHelper(vd.Is64Bits() ? vd.V4H() : vd.V8H(), imm & 0xffff);
    return;
  }

  // Default case.
  {
    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireW();
    Mov(temp, imm);
    dup(vd, temp);
  }
}

void TurboAssembler::Movi64bitHelper(const VRegister& vd, uint64_t imm) {
  // All bytes are either 0x00 or 0xff.
  {
    bool all0orff = true;
    for (int i = 0; i < 8; ++i) {
      int byteval = (imm >> (i * 8)) & 0xff;
      if (byteval != 0 && byteval != 0xff) {
        all0orff = false;
        break;
      }
    }
    if (all0orff == true) {
      movi(vd, imm);
      return;
    }
  }

  // Top and bottom 32-bits are equal.
  if (((imm >> 32) & 0xffffffff) == (imm & 0xffffffff)) {
    Movi32bitHelper(vd.Is64Bits() ? vd.V2S() : vd.V4S(), imm & 0xffffffff);
    return;
  }

  // Default case.
  {
    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireX();
    Mov(temp, imm);
    if (vd.Is1D()) {
      mov(vd.D(), 0, temp);
    } else {
      dup(vd.V2D(), temp);
    }
  }
}

void TurboAssembler::Movi(const VRegister& vd, uint64_t imm, Shift shift,
                          int shift_amount) {
  DCHECK(allow_macro_instructions());
  if (shift_amount != 0 || shift != LSL) {
    movi(vd, imm, shift, shift_amount);
  } else if (vd.Is8B() || vd.Is16B()) {
    // 8-bit immediate.
    DCHECK(is_uint8(imm));
    movi(vd, imm);
  } else if (vd.Is4H() || vd.Is8H()) {
    // 16-bit immediate.
    Movi16bitHelper(vd, imm);
  } else if (vd.Is2S() || vd.Is4S()) {
    // 32-bit immediate.
    Movi32bitHelper(vd, imm);
  } else {
    // 64-bit immediate.
    Movi64bitHelper(vd, imm);
  }
}

void TurboAssembler::Movi(const VRegister& vd, uint64_t hi, uint64_t lo) {
  // TODO(all): Move 128-bit values in a more efficient way.
  DCHECK(vd.Is128Bits());
  UseScratchRegisterScope temps(this);
  Movi(vd.V2D(), lo);
  Register temp = temps.AcquireX();
  Mov(temp, hi);
  Ins(vd.V2D(), 1, temp);
}

void TurboAssembler::Mvn(const Register& rd, const Operand& operand) {
  DCHECK(allow_macro_instructions());

  if (operand.NeedsRelocation(this)) {
    Ldr(rd, operand.immediate());
    mvn(rd, rd);

  } else if (operand.IsImmediate()) {
    // Call the macro assembler for generic immediates.
    Mov(rd, ~operand.ImmediateValue());

  } else if (operand.IsExtendedRegister()) {
    // Emit two instructions for the extend case. This differs from Mov, as
    // the extend and invert can't be achieved in one instruction.
    EmitExtendShift(rd, operand.reg(), operand.extend(),
                    operand.shift_amount());
    mvn(rd, rd);

  } else {
    mvn(rd, operand);
  }
}

unsigned TurboAssembler::CountClearHalfWords(uint64_t imm, unsigned reg_size) {
  DCHECK((reg_size % 8) == 0);
  int count = 0;
  for (unsigned i = 0; i < (reg_size / 16); i++) {
    if ((imm & 0xffff) == 0) {
      count++;
    }
    imm >>= 16;
  }
  return count;
}


// The movz instruction can generate immediates containing an arbitrary 16-bit
// half-word, with remaining bits clear, eg. 0x00001234, 0x0000123400000000.
bool TurboAssembler::IsImmMovz(uint64_t imm, unsigned reg_size) {
  DCHECK((reg_size == kXRegSizeInBits) || (reg_size == kWRegSizeInBits));
  return CountClearHalfWords(imm, reg_size) >= ((reg_size / 16) - 1);
}


// The movn instruction can generate immediates containing an arbitrary 16-bit
// half-word, with remaining bits set, eg. 0xffff1234, 0xffff1234ffffffff.
bool TurboAssembler::IsImmMovn(uint64_t imm, unsigned reg_size) {
  return IsImmMovz(~imm, reg_size);
}

void TurboAssembler::ConditionalCompareMacro(const Register& rn,
                                             const Operand& operand,
                                             StatusFlags nzcv, Condition cond,
                                             ConditionalCompareOp op) {
  DCHECK((cond != al) && (cond != nv));
  if (operand.NeedsRelocation(this)) {
    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireX();
    Ldr(temp, operand.immediate());
    ConditionalCompareMacro(rn, temp, nzcv, cond, op);

  } else if ((operand.IsShiftedRegister() && (operand.shift_amount() == 0)) ||
             (operand.IsImmediate() &&
              IsImmConditionalCompare(operand.ImmediateValue()))) {
    // The immediate can be encoded in the instruction, or the operand is an
    // unshifted register: call the assembler.
    ConditionalCompare(rn, operand, nzcv, cond, op);

  } else {
    // The operand isn't directly supported by the instruction: perform the
    // operation on a temporary register.
    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireSameSizeAs(rn);
    Mov(temp, operand);
    ConditionalCompare(rn, temp, nzcv, cond, op);
  }
}


void MacroAssembler::Csel(const Register& rd,
                          const Register& rn,
                          const Operand& operand,
                          Condition cond) {
  DCHECK(allow_macro_instructions());
  DCHECK(!rd.IsZero());
  DCHECK((cond != al) && (cond != nv));
  if (operand.IsImmediate()) {
    // Immediate argument. Handle special cases of 0, 1 and -1 using zero
    // register.
    int64_t imm = operand.ImmediateValue();
    Register zr = AppropriateZeroRegFor(rn);
    if (imm == 0) {
      csel(rd, rn, zr, cond);
    } else if (imm == 1) {
      csinc(rd, rn, zr, cond);
    } else if (imm == -1) {
      csinv(rd, rn, zr, cond);
    } else {
      UseScratchRegisterScope temps(this);
      Register temp = temps.AcquireSameSizeAs(rn);
      Mov(temp, imm);
      csel(rd, rn, temp, cond);
    }
  } else if (operand.IsShiftedRegister() && (operand.shift_amount() == 0)) {
    // Unshifted register argument.
    csel(rd, rn, operand.reg(), cond);
  } else {
    // All other arguments.
    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireSameSizeAs(rn);
    Mov(temp, operand);
    csel(rd, rn, temp, cond);
  }
}

bool TurboAssembler::TryOneInstrMoveImmediate(const Register& dst,
                                              int64_t imm) {
  unsigned n, imm_s, imm_r;
  int reg_size = dst.SizeInBits();
  if (IsImmMovz(imm, reg_size) && !dst.IsSP()) {
    // Immediate can be represented in a move zero instruction. Movz can't write
    // to the stack pointer.
    movz(dst, imm);
    return true;
  } else if (IsImmMovn(imm, reg_size) && !dst.IsSP()) {
    // Immediate can be represented in a move not instruction. Movn can't write
    // to the stack pointer.
    movn(dst, dst.Is64Bits() ? ~imm : (~imm & kWRegMask));
    return true;
  } else if (IsImmLogical(imm, reg_size, &n, &imm_s, &imm_r)) {
    // Immediate can be represented in a logical orr instruction.
    LogicalImmediate(dst, AppropriateZeroRegFor(dst), n, imm_s, imm_r, ORR);
    return true;
  }
  return false;
}

Operand TurboAssembler::MoveImmediateForShiftedOp(const Register& dst,
                                                  int64_t imm,
                                                  PreShiftImmMode mode) {
  int reg_size = dst.SizeInBits();
  // Encode the immediate in a single move instruction, if possible.
  if (TryOneInstrMoveImmediate(dst, imm)) {
    // The move was successful; nothing to do here.
  } else {
    // Pre-shift the immediate to the least-significant bits of the register.
    int shift_low = CountTrailingZeros(imm, reg_size);
    if (mode == kLimitShiftForSP) {
      // When applied to the stack pointer, the subsequent arithmetic operation
      // can use the extend form to shift left by a maximum of four bits. Right
      // shifts are not allowed, so we filter them out later before the new
      // immediate is tested.
      shift_low = std::min(shift_low, 4);
    }
    int64_t imm_low = imm >> shift_low;

    // Pre-shift the immediate to the most-significant bits of the register. We
    // insert set bits in the least-significant bits, as this creates a
    // different immediate that may be encodable using movn or orr-immediate.
    // If this new immediate is encodable, the set bits will be eliminated by
    // the post shift on the following instruction.
    int shift_high = CountLeadingZeros(imm, reg_size);
    int64_t imm_high = (imm << shift_high) | ((INT64_C(1) << shift_high) - 1);

    if ((mode != kNoShift) && TryOneInstrMoveImmediate(dst, imm_low)) {
      // The new immediate has been moved into the destination's low bits:
      // return a new leftward-shifting operand.
      return Operand(dst, LSL, shift_low);
    } else if ((mode == kAnyShift) && TryOneInstrMoveImmediate(dst, imm_high)) {
      // The new immediate has been moved into the destination's high bits:
      // return a new rightward-shifting operand.
      return Operand(dst, LSR, shift_high);
    } else {
      // Use the generic move operation to set up the immediate.
      Mov(dst, imm);
    }
  }
  return Operand(dst);
}

void TurboAssembler::AddSubMacro(const Register& rd, const Register& rn,
                                 const Operand& operand, FlagsUpdate S,
                                 AddSubOp op) {
  if (operand.IsZero() && rd.Is(rn) && rd.Is64Bits() && rn.Is64Bits() &&
      !operand.NeedsRelocation(this) && (S == LeaveFlags)) {
    // The instruction would be a nop. Avoid generating useless code.
    return;
  }

  if (operand.NeedsRelocation(this)) {
    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireX();
    Ldr(temp, operand.immediate());
    AddSubMacro(rd, rn, temp, S, op);
  } else if ((operand.IsImmediate() &&
              !IsImmAddSub(operand.ImmediateValue()))      ||
             (rn.IsZero() && !operand.IsShiftedRegister()) ||
             (operand.IsShiftedRegister() && (operand.shift() == ROR))) {
    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireSameSizeAs(rn);
    if (operand.IsImmediate()) {
      PreShiftImmMode mode = kAnyShift;

      // If the destination or source register is the stack pointer, we can
      // only pre-shift the immediate right by values supported in the add/sub
      // extend encoding.
      if (rd.Is(csp)) {
        // If the destination is SP and flags will be set, we can't pre-shift
        // the immediate at all.
        mode = (S == SetFlags) ? kNoShift : kLimitShiftForSP;
      } else if (rn.Is(csp)) {
        mode = kLimitShiftForSP;
      }

      Operand imm_operand =
          MoveImmediateForShiftedOp(temp, operand.ImmediateValue(), mode);
      AddSub(rd, rn, imm_operand, S, op);
    } else {
      Mov(temp, operand);
      AddSub(rd, rn, temp, S, op);
    }
  } else {
    AddSub(rd, rn, operand, S, op);
  }
}

void TurboAssembler::AddSubWithCarryMacro(const Register& rd,
                                          const Register& rn,
                                          const Operand& operand, FlagsUpdate S,
                                          AddSubWithCarryOp op) {
  DCHECK(rd.SizeInBits() == rn.SizeInBits());
  UseScratchRegisterScope temps(this);

  if (operand.NeedsRelocation(this)) {
    Register temp = temps.AcquireX();
    Ldr(temp, operand.immediate());
    AddSubWithCarryMacro(rd, rn, temp, S, op);

  } else if (operand.IsImmediate() ||
             (operand.IsShiftedRegister() && (operand.shift() == ROR))) {
    // Add/sub with carry (immediate or ROR shifted register.)
    Register temp = temps.AcquireSameSizeAs(rn);
    Mov(temp, operand);
    AddSubWithCarry(rd, rn, temp, S, op);

  } else if (operand.IsShiftedRegister() && (operand.shift_amount() != 0)) {
    // Add/sub with carry (shifted register).
    DCHECK(operand.reg().SizeInBits() == rd.SizeInBits());
    DCHECK(operand.shift() != ROR);
    DCHECK(is_uintn(operand.shift_amount(),
          rd.SizeInBits() == kXRegSizeInBits ? kXRegSizeInBitsLog2
                                             : kWRegSizeInBitsLog2));
    Register temp = temps.AcquireSameSizeAs(rn);
    EmitShift(temp, operand.reg(), operand.shift(), operand.shift_amount());
    AddSubWithCarry(rd, rn, temp, S, op);

  } else if (operand.IsExtendedRegister()) {
    // Add/sub with carry (extended register).
    DCHECK(operand.reg().SizeInBits() <= rd.SizeInBits());
    // Add/sub extended supports a shift <= 4. We want to support exactly the
    // same modes.
    DCHECK(operand.shift_amount() <= 4);
    DCHECK(operand.reg().Is64Bits() ||
           ((operand.extend() != UXTX) && (operand.extend() != SXTX)));
    Register temp = temps.AcquireSameSizeAs(rn);
    EmitExtendShift(temp, operand.reg(), operand.extend(),
                    operand.shift_amount());
    AddSubWithCarry(rd, rn, temp, S, op);

  } else {
    // The addressing mode is directly supported by the instruction.
    AddSubWithCarry(rd, rn, operand, S, op);
  }
}

void TurboAssembler::LoadStoreMacro(const CPURegister& rt,
                                    const MemOperand& addr, LoadStoreOp op) {
  int64_t offset = addr.offset();
  unsigned size = CalcLSDataSize(op);

  // Check if an immediate offset fits in the immediate field of the
  // appropriate instruction. If not, emit two instructions to perform
  // the operation.
  if (addr.IsImmediateOffset() && !IsImmLSScaled(offset, size) &&
      !IsImmLSUnscaled(offset)) {
    // Immediate offset that can't be encoded using unsigned or unscaled
    // addressing modes.
    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireSameSizeAs(addr.base());
    Mov(temp, addr.offset());
    LoadStore(rt, MemOperand(addr.base(), temp), op);
  } else if (addr.IsPostIndex() && !IsImmLSUnscaled(offset)) {
    // Post-index beyond unscaled addressing range.
    LoadStore(rt, MemOperand(addr.base()), op);
    add(addr.base(), addr.base(), offset);
  } else if (addr.IsPreIndex() && !IsImmLSUnscaled(offset)) {
    // Pre-index beyond unscaled addressing range.
    add(addr.base(), addr.base(), offset);
    LoadStore(rt, MemOperand(addr.base()), op);
  } else {
    // Encodable in one load/store instruction.
    LoadStore(rt, addr, op);
  }
}

void TurboAssembler::LoadStorePairMacro(const CPURegister& rt,
                                        const CPURegister& rt2,
                                        const MemOperand& addr,
                                        LoadStorePairOp op) {
  // TODO(all): Should we support register offset for load-store-pair?
  DCHECK(!addr.IsRegisterOffset());

  int64_t offset = addr.offset();
  unsigned size = CalcLSPairDataSize(op);

  // Check if the offset fits in the immediate field of the appropriate
  // instruction. If not, emit two instructions to perform the operation.
  if (IsImmLSPair(offset, size)) {
    // Encodable in one load/store pair instruction.
    LoadStorePair(rt, rt2, addr, op);
  } else {
    Register base = addr.base();
    if (addr.IsImmediateOffset()) {
      UseScratchRegisterScope temps(this);
      Register temp = temps.AcquireSameSizeAs(base);
      Add(temp, base, offset);
      LoadStorePair(rt, rt2, MemOperand(temp), op);
    } else if (addr.IsPostIndex()) {
      LoadStorePair(rt, rt2, MemOperand(base), op);
      Add(base, base, offset);
    } else {
      DCHECK(addr.IsPreIndex());
      Add(base, base, offset);
      LoadStorePair(rt, rt2, MemOperand(base), op);
    }
  }
}

bool TurboAssembler::NeedExtraInstructionsOrRegisterBranch(
    Label* label, ImmBranchType b_type) {
  bool need_longer_range = false;
  // There are two situations in which we care about the offset being out of
  // range:
  //  - The label is bound but too far away.
  //  - The label is not bound but linked, and the previous branch
  //    instruction in the chain is too far away.
  if (label->is_bound() || label->is_linked()) {
    need_longer_range =
      !Instruction::IsValidImmPCOffset(b_type, label->pos() - pc_offset());
  }
  if (!need_longer_range && !label->is_bound()) {
    int max_reachable_pc = pc_offset() + Instruction::ImmBranchRange(b_type);
    unresolved_branches_.insert(
        std::pair<int, FarBranchInfo>(max_reachable_pc,
                                      FarBranchInfo(pc_offset(), label)));
    // Also maintain the next pool check.
    next_veneer_pool_check_ =
      Min(next_veneer_pool_check_,
          max_reachable_pc - kVeneerDistanceCheckMargin);
  }
  return need_longer_range;
}

void TurboAssembler::Adr(const Register& rd, Label* label, AdrHint hint) {
  DCHECK(allow_macro_instructions());
  DCHECK(!rd.IsZero());

  if (hint == kAdrNear) {
    adr(rd, label);
    return;
  }

  DCHECK(hint == kAdrFar);
  if (label->is_bound()) {
    int label_offset = label->pos() - pc_offset();
    if (Instruction::IsValidPCRelOffset(label_offset)) {
      adr(rd, label);
    } else {
      DCHECK(label_offset <= 0);
      int min_adr_offset = -(1 << (Instruction::ImmPCRelRangeBitwidth - 1));
      adr(rd, min_adr_offset);
      Add(rd, rd, label_offset - min_adr_offset);
    }
  } else {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.AcquireX();

    InstructionAccurateScope scope(
        this, PatchingAssembler::kAdrFarPatchableNInstrs);
    adr(rd, label);
    for (int i = 0; i < PatchingAssembler::kAdrFarPatchableNNops; ++i) {
      nop(ADR_FAR_NOP);
    }
    movz(scratch, 0);
  }
}

void TurboAssembler::B(Label* label, BranchType type, Register reg, int bit) {
  DCHECK((reg.Is(NoReg) || type >= kBranchTypeFirstUsingReg) &&
         (bit == -1 || type >= kBranchTypeFirstUsingBit));
  if (kBranchTypeFirstCondition <= type && type <= kBranchTypeLastCondition) {
    B(static_cast<Condition>(type), label);
  } else {
    switch (type) {
      case always:        B(label);              break;
      case never:         break;
      case reg_zero:      Cbz(reg, label);       break;
      case reg_not_zero:  Cbnz(reg, label);      break;
      case reg_bit_clear: Tbz(reg, bit, label);  break;
      case reg_bit_set:   Tbnz(reg, bit, label); break;
      default:
        UNREACHABLE();
    }
  }
}

void TurboAssembler::B(Label* label, Condition cond) {
  DCHECK(allow_macro_instructions());
  DCHECK((cond != al) && (cond != nv));

  Label done;
  bool need_extra_instructions =
    NeedExtraInstructionsOrRegisterBranch(label, CondBranchType);

  if (need_extra_instructions) {
    b(&done, NegateCondition(cond));
    B(label);
  } else {
    b(label, cond);
  }
  bind(&done);
}

void TurboAssembler::Tbnz(const Register& rt, unsigned bit_pos, Label* label) {
  DCHECK(allow_macro_instructions());

  Label done;
  bool need_extra_instructions =
    NeedExtraInstructionsOrRegisterBranch(label, TestBranchType);

  if (need_extra_instructions) {
    tbz(rt, bit_pos, &done);
    B(label);
  } else {
    tbnz(rt, bit_pos, label);
  }
  bind(&done);
}

void TurboAssembler::Tbz(const Register& rt, unsigned bit_pos, Label* label) {
  DCHECK(allow_macro_instructions());

  Label done;
  bool need_extra_instructions =
    NeedExtraInstructionsOrRegisterBranch(label, TestBranchType);

  if (need_extra_instructions) {
    tbnz(rt, bit_pos, &done);
    B(label);
  } else {
    tbz(rt, bit_pos, label);
  }
  bind(&done);
}

void TurboAssembler::Cbnz(const Register& rt, Label* label) {
  DCHECK(allow_macro_instructions());

  Label done;
  bool need_extra_instructions =
    NeedExtraInstructionsOrRegisterBranch(label, CompareBranchType);

  if (need_extra_instructions) {
    cbz(rt, &done);
    B(label);
  } else {
    cbnz(rt, label);
  }
  bind(&done);
}

void TurboAssembler::Cbz(const Register& rt, Label* label) {
  DCHECK(allow_macro_instructions());

  Label done;
  bool need_extra_instructions =
    NeedExtraInstructionsOrRegisterBranch(label, CompareBranchType);

  if (need_extra_instructions) {
    cbnz(rt, &done);
    B(label);
  } else {
    cbz(rt, label);
  }
  bind(&done);
}


// Pseudo-instructions.

void TurboAssembler::Abs(const Register& rd, const Register& rm,
                         Label* is_not_representable, Label* is_representable) {
  DCHECK(allow_macro_instructions());
  DCHECK(AreSameSizeAndType(rd, rm));

  Cmp(rm, 1);
  Cneg(rd, rm, lt);

  // If the comparison sets the v flag, the input was the smallest value
  // representable by rm, and the mathematical result of abs(rm) is not
  // representable using two's complement.
  if ((is_not_representable != NULL) && (is_representable != NULL)) {
    B(is_not_representable, vs);
    B(is_representable);
  } else if (is_not_representable != NULL) {
    B(is_not_representable, vs);
  } else if (is_representable != NULL) {
    B(is_representable, vc);
  }
}


// Abstracted stack operations.

void TurboAssembler::Push(const CPURegister& src0, const CPURegister& src1,
                          const CPURegister& src2, const CPURegister& src3) {
  DCHECK(AreSameSizeAndType(src0, src1, src2, src3));

  int count = 1 + src1.IsValid() + src2.IsValid() + src3.IsValid();
  int size = src0.SizeInBytes();

  PushPreamble(count, size);
  PushHelper(count, size, src0, src1, src2, src3);
}

void TurboAssembler::Push(const CPURegister& src0, const CPURegister& src1,
                          const CPURegister& src2, const CPURegister& src3,
                          const CPURegister& src4, const CPURegister& src5,
                          const CPURegister& src6, const CPURegister& src7) {
  DCHECK(AreSameSizeAndType(src0, src1, src2, src3, src4, src5, src6, src7));

  int count = 5 + src5.IsValid() + src6.IsValid() + src6.IsValid();
  int size = src0.SizeInBytes();

  PushPreamble(count, size);
  PushHelper(4, size, src0, src1, src2, src3);
  PushHelper(count - 4, size, src4, src5, src6, src7);
}

void TurboAssembler::Pop(const CPURegister& dst0, const CPURegister& dst1,
                         const CPURegister& dst2, const CPURegister& dst3) {
  // It is not valid to pop into the same register more than once in one
  // instruction, not even into the zero register.
  DCHECK(!AreAliased(dst0, dst1, dst2, dst3));
  DCHECK(AreSameSizeAndType(dst0, dst1, dst2, dst3));
  DCHECK(dst0.IsValid());

  int count = 1 + dst1.IsValid() + dst2.IsValid() + dst3.IsValid();
  int size = dst0.SizeInBytes();

  PopHelper(count, size, dst0, dst1, dst2, dst3);
  PopPostamble(count, size);
}

void TurboAssembler::Pop(const CPURegister& dst0, const CPURegister& dst1,
                         const CPURegister& dst2, const CPURegister& dst3,
                         const CPURegister& dst4, const CPURegister& dst5,
                         const CPURegister& dst6, const CPURegister& dst7) {
  // It is not valid to pop into the same register more than once in one
  // instruction, not even into the zero register.
  DCHECK(!AreAliased(dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7));
  DCHECK(AreSameSizeAndType(dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7));
  DCHECK(dst0.IsValid());

  int count = 5 + dst5.IsValid() + dst6.IsValid() + dst7.IsValid();
  int size = dst0.SizeInBytes();

  PopHelper(4, size, dst0, dst1, dst2, dst3);
  PopHelper(count - 4, size, dst4, dst5, dst6, dst7);
  PopPostamble(count, size);
}

void TurboAssembler::Push(const Register& src0, const VRegister& src1) {
  int size = src0.SizeInBytes() + src1.SizeInBytes();

  PushPreamble(size);
  // Reserve room for src0 and push src1.
  str(src1, MemOperand(StackPointer(), -size, PreIndex));
  // Fill the gap with src0.
  str(src0, MemOperand(StackPointer(), src1.SizeInBytes()));
}


void MacroAssembler::PushPopQueue::PushQueued(
    PreambleDirective preamble_directive) {
  if (queued_.empty()) return;

  if (preamble_directive == WITH_PREAMBLE) {
    masm_->PushPreamble(size_);
  }

  size_t count = queued_.size();
  size_t index = 0;
  while (index < count) {
    // PushHelper can only handle registers with the same size and type, and it
    // can handle only four at a time. Batch them up accordingly.
    CPURegister batch[4] = {NoReg, NoReg, NoReg, NoReg};
    int batch_index = 0;
    do {
      batch[batch_index++] = queued_[index++];
    } while ((batch_index < 4) && (index < count) &&
             batch[0].IsSameSizeAndType(queued_[index]));

    masm_->PushHelper(batch_index, batch[0].SizeInBytes(),
                      batch[0], batch[1], batch[2], batch[3]);
  }

  queued_.clear();
}


void MacroAssembler::PushPopQueue::PopQueued() {
  if (queued_.empty()) return;

  size_t count = queued_.size();
  size_t index = 0;
  while (index < count) {
    // PopHelper can only handle registers with the same size and type, and it
    // can handle only four at a time. Batch them up accordingly.
    CPURegister batch[4] = {NoReg, NoReg, NoReg, NoReg};
    int batch_index = 0;
    do {
      batch[batch_index++] = queued_[index++];
    } while ((batch_index < 4) && (index < count) &&
             batch[0].IsSameSizeAndType(queued_[index]));

    masm_->PopHelper(batch_index, batch[0].SizeInBytes(),
                     batch[0], batch[1], batch[2], batch[3]);
  }

  masm_->PopPostamble(size_);
  queued_.clear();
}

void TurboAssembler::PushCPURegList(CPURegList registers) {
  int size = registers.RegisterSizeInBytes();

  PushPreamble(registers.Count(), size);
  // Push up to four registers at a time because if the current stack pointer is
  // csp and reg_size is 32, registers must be pushed in blocks of four in order
  // to maintain the 16-byte alignment for csp.
  while (!registers.IsEmpty()) {
    int count_before = registers.Count();
    const CPURegister& src0 = registers.PopHighestIndex();
    const CPURegister& src1 = registers.PopHighestIndex();
    const CPURegister& src2 = registers.PopHighestIndex();
    const CPURegister& src3 = registers.PopHighestIndex();
    int count = count_before - registers.Count();
    PushHelper(count, size, src0, src1, src2, src3);
  }
}

void TurboAssembler::PopCPURegList(CPURegList registers) {
  int size = registers.RegisterSizeInBytes();

  // Pop up to four registers at a time because if the current stack pointer is
  // csp and reg_size is 32, registers must be pushed in blocks of four in
  // order to maintain the 16-byte alignment for csp.
  while (!registers.IsEmpty()) {
    int count_before = registers.Count();
    const CPURegister& dst0 = registers.PopLowestIndex();
    const CPURegister& dst1 = registers.PopLowestIndex();
    const CPURegister& dst2 = registers.PopLowestIndex();
    const CPURegister& dst3 = registers.PopLowestIndex();
    int count = count_before - registers.Count();
    PopHelper(count, size, dst0, dst1, dst2, dst3);
  }
  PopPostamble(registers.Count(), size);
}

void MacroAssembler::PushMultipleTimes(CPURegister src, Register count) {
  PushPreamble(Operand(count, UXTW, WhichPowerOf2(src.SizeInBytes())));

  UseScratchRegisterScope temps(this);
  Register temp = temps.AcquireSameSizeAs(count);

  if (FLAG_optimize_for_size) {
    Label loop, done;

    Subs(temp, count, 1);
    B(mi, &done);

    // Push all registers individually, to save code size.
    Bind(&loop);
    Subs(temp, temp, 1);
    PushHelper(1, src.SizeInBytes(), src, NoReg, NoReg, NoReg);
    B(pl, &loop);

    Bind(&done);
  } else {
    Label loop, leftover2, leftover1, done;

    Subs(temp, count, 4);
    B(mi, &leftover2);

    // Push groups of four first.
    Bind(&loop);
    Subs(temp, temp, 4);
    PushHelper(4, src.SizeInBytes(), src, src, src, src);
    B(pl, &loop);

    // Push groups of two.
    Bind(&leftover2);
    Tbz(count, 1, &leftover1);
    PushHelper(2, src.SizeInBytes(), src, src, NoReg, NoReg);

    // Push the last one (if required).
    Bind(&leftover1);
    Tbz(count, 0, &done);
    PushHelper(1, src.SizeInBytes(), src, NoReg, NoReg, NoReg);

    Bind(&done);
  }
}

void TurboAssembler::PushHelper(int count, int size, const CPURegister& src0,
                                const CPURegister& src1,
                                const CPURegister& src2,
                                const CPURegister& src3) {
  // Ensure that we don't unintentially modify scratch or debug registers.
  InstructionAccurateScope scope(this);

  DCHECK(AreSameSizeAndType(src0, src1, src2, src3));
  DCHECK(size == src0.SizeInBytes());

  // When pushing multiple registers, the store order is chosen such that
  // Push(a, b) is equivalent to Push(a) followed by Push(b).
  switch (count) {
    case 1:
      DCHECK(src1.IsNone() && src2.IsNone() && src3.IsNone());
      str(src0, MemOperand(StackPointer(), -1 * size, PreIndex));
      break;
    case 2:
      DCHECK(src2.IsNone() && src3.IsNone());
      stp(src1, src0, MemOperand(StackPointer(), -2 * size, PreIndex));
      break;
    case 3:
      DCHECK(src3.IsNone());
      stp(src2, src1, MemOperand(StackPointer(), -3 * size, PreIndex));
      str(src0, MemOperand(StackPointer(), 2 * size));
      break;
    case 4:
      // Skip over 4 * size, then fill in the gap. This allows four W registers
      // to be pushed using csp, whilst maintaining 16-byte alignment for csp
      // at all times.
      stp(src3, src2, MemOperand(StackPointer(), -4 * size, PreIndex));
      stp(src1, src0, MemOperand(StackPointer(), 2 * size));
      break;
    default:
      UNREACHABLE();
  }
}

void TurboAssembler::PopHelper(int count, int size, const CPURegister& dst0,
                               const CPURegister& dst1, const CPURegister& dst2,
                               const CPURegister& dst3) {
  // Ensure that we don't unintentially modify scratch or debug registers.
  InstructionAccurateScope scope(this);

  DCHECK(AreSameSizeAndType(dst0, dst1, dst2, dst3));
  DCHECK(size == dst0.SizeInBytes());

  // When popping multiple registers, the load order is chosen such that
  // Pop(a, b) is equivalent to Pop(a) followed by Pop(b).
  switch (count) {
    case 1:
      DCHECK(dst1.IsNone() && dst2.IsNone() && dst3.IsNone());
      ldr(dst0, MemOperand(StackPointer(), 1 * size, PostIndex));
      break;
    case 2:
      DCHECK(dst2.IsNone() && dst3.IsNone());
      ldp(dst0, dst1, MemOperand(StackPointer(), 2 * size, PostIndex));
      break;
    case 3:
      DCHECK(dst3.IsNone());
      ldr(dst2, MemOperand(StackPointer(), 2 * size));
      ldp(dst0, dst1, MemOperand(StackPointer(), 3 * size, PostIndex));
      break;
    case 4:
      // Load the higher addresses first, then load the lower addresses and
      // skip the whole block in the second instruction. This allows four W
      // registers to be popped using csp, whilst maintaining 16-byte alignment
      // for csp at all times.
      ldp(dst2, dst3, MemOperand(StackPointer(), 2 * size));
      ldp(dst0, dst1, MemOperand(StackPointer(), 4 * size, PostIndex));
      break;
    default:
      UNREACHABLE();
  }
}

void TurboAssembler::PushPreamble(Operand total_size) {
  if (total_size.IsZero()) return;

  if (csp.Is(StackPointer())) {
    // If the current stack pointer is csp, then it must be aligned to 16 bytes
    // on entry and the total size of the specified registers must also be a
    // multiple of 16 bytes.
    if (total_size.IsImmediate()) {
      DCHECK((total_size.ImmediateValue() % 16) == 0);
    }

    // Don't check access size for non-immediate sizes. It's difficult to do
    // well, and it will be caught by hardware (or the simulator) anyway.
  } else {
    // Even if the current stack pointer is not the system stack pointer (csp),
    // the system stack pointer will still be modified in order to comply with
    // ABI rules about accessing memory below the system stack pointer.
    BumpSystemStackPointer(total_size);
  }
}

void TurboAssembler::PopPostamble(Operand total_size) {
  if (total_size.IsZero()) return;

  if (csp.Is(StackPointer())) {
    // If the current stack pointer is csp, then it must be aligned to 16 bytes
    // on entry and the total size of the specified registers must also be a
    // multiple of 16 bytes.
    if (total_size.IsImmediate()) {
      DCHECK((total_size.ImmediateValue() % 16) == 0);
    }

    // Don't check access size for non-immediate sizes. It's difficult to do
    // well, and it will be caught by hardware (or the simulator) anyway.
  } else if (emit_debug_code()) {
    // It is safe to leave csp where it is when unwinding the JavaScript stack,
    // but if we keep it matching StackPointer, the simulator can detect memory
    // accesses in the now-free part of the stack.
    SyncSystemStackPointer();
  }
}

void TurboAssembler::PushPreamble(int count, int size) {
  PushPreamble(count * size);
}
void TurboAssembler::PopPostamble(int count, int size) {
  PopPostamble(count * size);
}

void TurboAssembler::Poke(const CPURegister& src, const Operand& offset) {
  if (offset.IsImmediate()) {
    DCHECK(offset.ImmediateValue() >= 0);
  } else if (emit_debug_code()) {
    Cmp(xzr, offset);
    Check(le, kStackAccessBelowStackPointer);
  }

  Str(src, MemOperand(StackPointer(), offset));
}


void MacroAssembler::Peek(const CPURegister& dst, const Operand& offset) {
  if (offset.IsImmediate()) {
    DCHECK(offset.ImmediateValue() >= 0);
  } else if (emit_debug_code()) {
    Cmp(xzr, offset);
    Check(le, kStackAccessBelowStackPointer);
  }

  Ldr(dst, MemOperand(StackPointer(), offset));
}

void TurboAssembler::PokePair(const CPURegister& src1, const CPURegister& src2,
                              int offset) {
  DCHECK(AreSameSizeAndType(src1, src2));
  DCHECK((offset >= 0) && ((offset % src1.SizeInBytes()) == 0));
  Stp(src1, src2, MemOperand(StackPointer(), offset));
}


void MacroAssembler::PeekPair(const CPURegister& dst1,
                              const CPURegister& dst2,
                              int offset) {
  DCHECK(AreSameSizeAndType(dst1, dst2));
  DCHECK((offset >= 0) && ((offset % dst1.SizeInBytes()) == 0));
  Ldp(dst1, dst2, MemOperand(StackPointer(), offset));
}


void MacroAssembler::PushCalleeSavedRegisters() {
  // Ensure that the macro-assembler doesn't use any scratch registers.
  InstructionAccurateScope scope(this);

  // This method must not be called unless the current stack pointer is the
  // system stack pointer (csp).
  DCHECK(csp.Is(StackPointer()));

  MemOperand tos(csp, -2 * static_cast<int>(kXRegSize), PreIndex);

  stp(d14, d15, tos);
  stp(d12, d13, tos);
  stp(d10, d11, tos);
  stp(d8, d9, tos);

  stp(x29, x30, tos);
  stp(x27, x28, tos);    // x28 = jssp
  stp(x25, x26, tos);
  stp(x23, x24, tos);
  stp(x21, x22, tos);
  stp(x19, x20, tos);
}


void MacroAssembler::PopCalleeSavedRegisters() {
  // Ensure that the macro-assembler doesn't use any scratch registers.
  InstructionAccurateScope scope(this);

  // This method must not be called unless the current stack pointer is the
  // system stack pointer (csp).
  DCHECK(csp.Is(StackPointer()));

  MemOperand tos(csp, 2 * kXRegSize, PostIndex);

  ldp(x19, x20, tos);
  ldp(x21, x22, tos);
  ldp(x23, x24, tos);
  ldp(x25, x26, tos);
  ldp(x27, x28, tos);    // x28 = jssp
  ldp(x29, x30, tos);

  ldp(d8, d9, tos);
  ldp(d10, d11, tos);
  ldp(d12, d13, tos);
  ldp(d14, d15, tos);
}

void TurboAssembler::AssertStackConsistency() {
  // Avoid emitting code when !use_real_abort() since non-real aborts cause too
  // much code to be generated.
  if (emit_debug_code() && use_real_aborts()) {
    if (csp.Is(StackPointer())) {
      // Always check the alignment of csp if ALWAYS_ALIGN_CSP is true.  We
      // can't check the alignment of csp without using a scratch register (or
      // clobbering the flags), but the processor (or simulator) will abort if
      // it is not properly aligned during a load.
      ldr(xzr, MemOperand(csp, 0));
    }
    if (FLAG_enable_slow_asserts && !csp.Is(StackPointer())) {
      Label ok;
      // Check that csp <= StackPointer(), preserving all registers and NZCV.
      sub(StackPointer(), csp, StackPointer());
      cbz(StackPointer(), &ok);                 // Ok if csp == StackPointer().
      tbnz(StackPointer(), kXSignBit, &ok);     // Ok if csp < StackPointer().

      // Avoid generating AssertStackConsistency checks for the Push in Abort.
      { DontEmitDebugCodeScope dont_emit_debug_code_scope(this);
        // Restore StackPointer().
        sub(StackPointer(), csp, StackPointer());
        Abort(kTheCurrentStackPointerIsBelowCsp);
      }

      bind(&ok);
      // Restore StackPointer().
      sub(StackPointer(), csp, StackPointer());
    }
  }
}

void TurboAssembler::AssertCspAligned() {
  if (emit_debug_code() && use_real_aborts()) {
    // TODO(titzer): use a real assert for alignment check?
    UseScratchRegisterScope scope(this);
    Register temp = scope.AcquireX();
    ldr(temp, MemOperand(csp));
  }
}

void TurboAssembler::CopySlots(int dst, Register src, Register slot_count) {
  DCHECK(!src.IsZero());
  UseScratchRegisterScope scope(this);
  Register dst_reg = scope.AcquireX();
  Add(dst_reg, StackPointer(), dst << kPointerSizeLog2);
  Add(src, StackPointer(), Operand(src, LSL, kPointerSizeLog2));
  CopyDoubleWords(dst_reg, src, slot_count);
}

void TurboAssembler::CopySlots(Register dst, Register src,
                               Register slot_count) {
  DCHECK(!dst.IsZero() && !src.IsZero());
  Add(dst, StackPointer(), Operand(dst, LSL, kPointerSizeLog2));
  Add(src, StackPointer(), Operand(src, LSL, kPointerSizeLog2));
  CopyDoubleWords(dst, src, slot_count);
}

void TurboAssembler::CopyDoubleWords(Register dst, Register src,
                                     Register count) {
  if (emit_debug_code()) {
    // Copy requires dst < src || (dst - src) >= count.
    Label dst_below_src;
    Subs(dst, dst, src);
    B(lt, &dst_below_src);
    Cmp(dst, count);
    Check(ge, kOffsetOutOfRange);
    Bind(&dst_below_src);
    Add(dst, dst, src);
  }

  static_assert(kPointerSize == kDRegSize,
                "pointers must be the same size as doubles");
  UseScratchRegisterScope scope(this);
  VRegister temp0 = scope.AcquireD();
  VRegister temp1 = scope.AcquireD();

  Label pairs, done;

  Tbz(count, 0, &pairs);
  Ldr(temp0, MemOperand(src, kPointerSize, PostIndex));
  Sub(count, count, 1);
  Str(temp0, MemOperand(dst, kPointerSize, PostIndex));

  Bind(&pairs);
  Cbz(count, &done);
  Ldp(temp0, temp1, MemOperand(src, 2 * kPointerSize, PostIndex));
  Sub(count, count, 2);
  Stp(temp0, temp1, MemOperand(dst, 2 * kPointerSize, PostIndex));
  B(&pairs);

  // TODO(all): large copies may benefit from using temporary Q registers
  // to copy four double words per iteration.

  Bind(&done);
}

void TurboAssembler::AssertFPCRState(Register fpcr) {
  if (emit_debug_code()) {
    Label unexpected_mode, done;
    UseScratchRegisterScope temps(this);
    if (fpcr.IsNone()) {
      fpcr = temps.AcquireX();
      Mrs(fpcr, FPCR);
    }

    // Settings left to their default values:
    //   - Assert that flush-to-zero is not set.
    Tbnz(fpcr, FZ_offset, &unexpected_mode);
    //   - Assert that the rounding mode is nearest-with-ties-to-even.
    STATIC_ASSERT(FPTieEven == 0);
    Tst(fpcr, RMode_mask);
    B(eq, &done);

    Bind(&unexpected_mode);
    Abort(kUnexpectedFPCRMode);

    Bind(&done);
  }
}

void TurboAssembler::CanonicalizeNaN(const VRegister& dst,
                                     const VRegister& src) {
  AssertFPCRState();

  // Subtracting 0.0 preserves all inputs except for signalling NaNs, which
  // become quiet NaNs. We use fsub rather than fadd because fsub preserves -0.0
  // inputs: -0.0 + 0.0 = 0.0, but -0.0 - 0.0 = -0.0.
  Fsub(dst, src, fp_zero);
}

void TurboAssembler::LoadRoot(CPURegister destination,
                              Heap::RootListIndex index) {
  // TODO(jbramley): Most root values are constants, and can be synthesized
  // without a load. Refer to the ARM back end for details.
  Ldr(destination, MemOperand(root, index << kPointerSizeLog2));
}


void MacroAssembler::LoadObject(Register result, Handle<Object> object) {
  AllowDeferredHandleDereference heap_object_check;
  if (object->IsHeapObject()) {
    Move(result, Handle<HeapObject>::cast(object));
  } else {
    Mov(result, Operand(Smi::cast(*object)));
  }
}

void TurboAssembler::Move(Register dst, Register src) { Mov(dst, src); }
void TurboAssembler::Move(Register dst, Handle<HeapObject> x) { Mov(dst, x); }
void TurboAssembler::Move(Register dst, Smi* src) { Mov(dst, src); }

void MacroAssembler::LoadInstanceDescriptors(Register map,
                                             Register descriptors) {
  Ldr(descriptors, FieldMemOperand(map, Map::kDescriptorsOffset));
}

void MacroAssembler::LoadAccessor(Register dst, Register holder,
                                  int accessor_index,
                                  AccessorComponent accessor) {
  Ldr(dst, FieldMemOperand(holder, HeapObject::kMapOffset));
  LoadInstanceDescriptors(dst, dst);
  Ldr(dst,
      FieldMemOperand(dst, DescriptorArray::GetValueOffset(accessor_index)));
  int offset = accessor == ACCESSOR_GETTER ? AccessorPair::kGetterOffset
                                           : AccessorPair::kSetterOffset;
  Ldr(dst, FieldMemOperand(dst, offset));
}

void MacroAssembler::InNewSpace(Register object,
                                Condition cond,
                                Label* branch) {
  DCHECK(cond == eq || cond == ne);
  UseScratchRegisterScope temps(this);
  CheckPageFlag(object, temps.AcquireSameSizeAs(object),
                MemoryChunk::kIsInNewSpaceMask, cond, branch);
}

void TurboAssembler::AssertSmi(Register object, BailoutReason reason) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    Tst(object, kSmiTagMask);
    Check(eq, reason);
  }
}


void MacroAssembler::AssertNotSmi(Register object, BailoutReason reason) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    Tst(object, kSmiTagMask);
    Check(ne, reason);
  }
}

void MacroAssembler::AssertFixedArray(Register object) {
  if (emit_debug_code()) {
    AssertNotSmi(object, kOperandIsASmiAndNotAFixedArray);

    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireX();

    CompareObjectType(object, temp, temp, FIXED_ARRAY_TYPE);
    Check(eq, kOperandIsNotAFixedArray);
  }
}

void MacroAssembler::AssertFunction(Register object) {
  if (emit_debug_code()) {
    AssertNotSmi(object, kOperandIsASmiAndNotAFunction);

    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireX();

    CompareObjectType(object, temp, temp, JS_FUNCTION_TYPE);
    Check(eq, kOperandIsNotAFunction);
  }
}


void MacroAssembler::AssertBoundFunction(Register object) {
  if (emit_debug_code()) {
    AssertNotSmi(object, kOperandIsASmiAndNotABoundFunction);

    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireX();

    CompareObjectType(object, temp, temp, JS_BOUND_FUNCTION_TYPE);
    Check(eq, kOperandIsNotABoundFunction);
  }
}

void MacroAssembler::AssertGeneratorObject(Register object) {
  if (!emit_debug_code()) return;
  AssertNotSmi(object, kOperandIsASmiAndNotAGeneratorObject);

  // Load map
  UseScratchRegisterScope temps(this);
  Register temp = temps.AcquireX();
  Ldr(temp, FieldMemOperand(object, HeapObject::kMapOffset));

  // Load instance type
  Ldrb(temp, FieldMemOperand(temp, Map::kInstanceTypeOffset));

  Label do_check;
  // Check if JSGeneratorObject
  Cmp(temp, JS_GENERATOR_OBJECT_TYPE);
  B(eq, &do_check);

  // Check if JSAsyncGeneratorObject
  Cmp(temp, JS_ASYNC_GENERATOR_OBJECT_TYPE);

  bind(&do_check);
  // Restore generator object to register and perform assertion
  Check(eq, kOperandIsNotAGeneratorObject);
}

void MacroAssembler::AssertUndefinedOrAllocationSite(Register object,
                                                     Register scratch) {
  if (emit_debug_code()) {
    Label done_checking;
    AssertNotSmi(object);
    JumpIfRoot(object, Heap::kUndefinedValueRootIndex, &done_checking);
    Ldr(scratch, FieldMemOperand(object, HeapObject::kMapOffset));
    CompareRoot(scratch, Heap::kAllocationSiteMapRootIndex);
    Assert(eq, kExpectedUndefinedOrCell);
    Bind(&done_checking);
  }
}

void TurboAssembler::AssertPositiveOrZero(Register value) {
  if (emit_debug_code()) {
    Label done;
    int sign_bit = value.Is64Bits() ? kXSignBit : kWSignBit;
    Tbz(value, sign_bit, &done);
    Abort(kUnexpectedNegativeValue);
    Bind(&done);
  }
}

void TurboAssembler::CallStubDelayed(CodeStub* stub) {
  DCHECK(AllowThisStubCall(stub));  // Stub calls are not allowed in some stubs.
  BlockPoolsScope scope(this);
#ifdef DEBUG
  Label start_call;
  Bind(&start_call);
#endif
  UseScratchRegisterScope temps(this);
  Register temp = temps.AcquireX();
  Ldr(temp, Operand::EmbeddedCode(stub));
  Blr(temp);
#ifdef DEBUG
  AssertSizeOfCodeGeneratedSince(&start_call, kCallSizeWithRelocation);
#endif
}

void MacroAssembler::CallStub(CodeStub* stub) {
  DCHECK(AllowThisStubCall(stub));  // Stub calls are not allowed in some stubs.
  Call(stub->GetCode(), RelocInfo::CODE_TARGET);
}

void MacroAssembler::TailCallStub(CodeStub* stub) {
  Jump(stub->GetCode(), RelocInfo::CODE_TARGET);
}

void TurboAssembler::CallRuntimeDelayed(Zone* zone, Runtime::FunctionId fid,
                                        SaveFPRegsMode save_doubles) {
  const Runtime::Function* f = Runtime::FunctionForId(fid);
  // TODO(1236192): Most runtime routines don't need the number of
  // arguments passed in because it is constant. At some point we
  // should remove this need and make the runtime routine entry code
  // smarter.
  Mov(x0, f->nargs);
  Mov(x1, ExternalReference(f, isolate()));
  CallStubDelayed(new (zone) CEntryStub(nullptr, 1, save_doubles));
}

void MacroAssembler::CallRuntime(const Runtime::Function* f,
                                 int num_arguments,
                                 SaveFPRegsMode save_doubles) {
  // All arguments must be on the stack before this function is called.
  // x0 holds the return value after the call.

  // Check that the number of arguments matches what the function expects.
  // If f->nargs is -1, the function can accept a variable number of arguments.
  CHECK(f->nargs < 0 || f->nargs == num_arguments);

  // Place the necessary arguments.
  Mov(x0, num_arguments);
  Mov(x1, ExternalReference(f, isolate()));

  CEntryStub stub(isolate(), 1, save_doubles);
  CallStub(&stub);
}

void MacroAssembler::JumpToExternalReference(const ExternalReference& builtin,
                                             bool builtin_exit_frame) {
  Mov(x1, builtin);
  CEntryStub stub(isolate(), 1, kDontSaveFPRegs, kArgvOnStack,
                  builtin_exit_frame);
  Jump(stub.GetCode(), RelocInfo::CODE_TARGET);
}

void MacroAssembler::TailCallRuntime(Runtime::FunctionId fid) {
  const Runtime::Function* function = Runtime::FunctionForId(fid);
  DCHECK_EQ(1, function->result_size);
  if (function->nargs >= 0) {
    // TODO(1236192): Most runtime routines don't need the number of
    // arguments passed in because it is constant. At some point we
    // should remove this need and make the runtime routine entry code
    // smarter.
    Mov(x0, function->nargs);
  }
  JumpToExternalReference(ExternalReference(fid, isolate()));
}

int TurboAssembler::ActivationFrameAlignment() {
#if V8_HOST_ARCH_ARM64
  // Running on the real platform. Use the alignment as mandated by the local
  // environment.
  // Note: This will break if we ever start generating snapshots on one ARM
  // platform for another ARM platform with a different alignment.
  return base::OS::ActivationFrameAlignment();
#else  // V8_HOST_ARCH_ARM64
  // If we are using the simulator then we should always align to the expected
  // alignment. As the simulator is used to generate snapshots we do not know
  // if the target platform will need alignment, so this is controlled from a
  // flag.
  return FLAG_sim_stack_alignment;
#endif  // V8_HOST_ARCH_ARM64
}

void TurboAssembler::CallCFunction(ExternalReference function,
                                   int num_of_reg_args) {
  CallCFunction(function, num_of_reg_args, 0);
}

void TurboAssembler::CallCFunction(ExternalReference function,
                                   int num_of_reg_args,
                                   int num_of_double_args) {
  UseScratchRegisterScope temps(this);
  Register temp = temps.AcquireX();
  Mov(temp, function);
  CallCFunction(temp, num_of_reg_args, num_of_double_args);
}

static const int kRegisterPassedArguments = 8;

void TurboAssembler::CallCFunction(Register function, int num_of_reg_args,
                                   int num_of_double_args) {
  DCHECK_LE(num_of_reg_args + num_of_double_args, kMaxCParameters);
  DCHECK(has_frame());

  // If we're passing doubles, we're limited to the following prototypes
  // (defined by ExternalReference::Type):
  //  BUILTIN_COMPARE_CALL:  int f(double, double)
  //  BUILTIN_FP_FP_CALL:    double f(double, double)
  //  BUILTIN_FP_CALL:       double f(double)
  //  BUILTIN_FP_INT_CALL:   double f(double, int)
  if (num_of_double_args > 0) {
    DCHECK(num_of_reg_args <= 1);
    DCHECK((num_of_double_args + num_of_reg_args) <= 2);
  }

  // We rely on the frame alignment being 16 bytes, which means we never need
  // to align the CSP by an unknown number of bytes and we always know the delta
  // between the stack pointer and the frame pointer.
  DCHECK(ActivationFrameAlignment() == 16);

  // If the stack pointer is not csp, we need to derive an aligned csp from the
  // current stack pointer.
  const Register old_stack_pointer = StackPointer();
  if (!csp.Is(old_stack_pointer)) {
    AssertStackConsistency();

    int sp_alignment = ActivationFrameAlignment();
    // The current stack pointer is a callee saved register, and is preserved
    // across the call.
    DCHECK(kCalleeSaved.IncludesAliasOf(old_stack_pointer));

    // If more than eight arguments are passed to the function, we expect the
    // ninth argument onwards to have been placed on the csp-based stack
    // already. We assume csp already points to the last stack-passed argument
    // in that case.
    // Otherwise, align and synchronize the system stack pointer with jssp.
    if (num_of_reg_args <= kRegisterPassedArguments) {
      Bic(csp, old_stack_pointer, sp_alignment - 1);
    }
    SetStackPointer(csp);
  }

  // Call directly. The function called cannot cause a GC, or allow preemption,
  // so the return address in the link register stays correct.
  Call(function);

  if (csp.Is(old_stack_pointer)) {
    if (num_of_reg_args > kRegisterPassedArguments) {
      // Drop the register passed arguments.
      int claim_slots = RoundUp(num_of_reg_args - kRegisterPassedArguments, 2);
      Drop(claim_slots);
    }
  } else {
    DCHECK(jssp.Is(old_stack_pointer));
    if (emit_debug_code()) {
      UseScratchRegisterScope temps(this);
      Register temp = temps.AcquireX();

      if (num_of_reg_args > kRegisterPassedArguments) {
        // We don't need to drop stack arguments, as the stack pointer will be
        // jssp when returning from this function. However, in debug builds, we
        // can check that jssp is as expected.
        int claim_slots =
            RoundUp(num_of_reg_args - kRegisterPassedArguments, 2);

        // Check jssp matches the previous value on the stack.
        Ldr(temp, MemOperand(csp, claim_slots * kPointerSize));
        Cmp(jssp, temp);
        Check(eq, kTheStackWasCorruptedByMacroAssemblerCall);
      } else {
        // Because the stack pointer must be aligned on a 16-byte boundary, the
        // aligned csp can be up to 12 bytes below the jssp. This is the case
        // where we only pushed one W register on top of an aligned jssp.
        Sub(temp, csp, old_stack_pointer);
        // We want temp <= 0 && temp >= -12.
        Cmp(temp, 0);
        Ccmp(temp, -12, NFlag, le);
        Check(ge, kTheStackWasCorruptedByMacroAssemblerCall);
      }
    }
    SetStackPointer(old_stack_pointer);
  }
}

void TurboAssembler::Jump(Register target) { Br(target); }

void TurboAssembler::Jump(intptr_t target, RelocInfo::Mode rmode,
                          Condition cond) {
  if (cond == nv) return;
  UseScratchRegisterScope temps(this);
  Register temp = temps.AcquireX();
  Label done;
  if (cond != al) B(NegateCondition(cond), &done);
  Mov(temp, Operand(target, rmode));
  Br(temp);
  Bind(&done);
}

void TurboAssembler::Jump(Address target, RelocInfo::Mode rmode,
                          Condition cond) {
  DCHECK(!RelocInfo::IsCodeTarget(rmode));
  Jump(reinterpret_cast<intptr_t>(target), rmode, cond);
}

void TurboAssembler::Jump(Handle<Code> code, RelocInfo::Mode rmode,
                          Condition cond) {
  DCHECK(RelocInfo::IsCodeTarget(rmode));
  Jump(reinterpret_cast<intptr_t>(code.address()), rmode, cond);
}

void TurboAssembler::Call(Register target) {
  BlockPoolsScope scope(this);
#ifdef DEBUG
  Label start_call;
  Bind(&start_call);
#endif

  Blr(target);

#ifdef DEBUG
  AssertSizeOfCodeGeneratedSince(&start_call, CallSize(target));
#endif
}

void TurboAssembler::Call(Label* target) {
  BlockPoolsScope scope(this);
#ifdef DEBUG
  Label start_call;
  Bind(&start_call);
#endif

  Bl(target);

#ifdef DEBUG
  AssertSizeOfCodeGeneratedSince(&start_call, CallSize(target));
#endif
}

// TurboAssembler::CallSize is sensitive to changes in this function, as it
// requires to know how many instructions are used to branch to the target.
void TurboAssembler::Call(Address target, RelocInfo::Mode rmode) {
  BlockPoolsScope scope(this);
#ifdef DEBUG
  Label start_call;
  Bind(&start_call);
#endif

  // Addresses always have 64 bits, so we shouldn't encounter NONE32.
  DCHECK(rmode != RelocInfo::NONE32);

  UseScratchRegisterScope temps(this);
  Register temp = temps.AcquireX();

  if (rmode == RelocInfo::NONE64) {
    // Addresses are 48 bits so we never need to load the upper 16 bits.
    uint64_t imm = reinterpret_cast<uint64_t>(target);
    // If we don't use ARM tagged addresses, the 16 higher bits must be 0.
    DCHECK(((imm >> 48) & 0xffff) == 0);
    movz(temp, (imm >> 0) & 0xffff, 0);
    movk(temp, (imm >> 16) & 0xffff, 16);
    movk(temp, (imm >> 32) & 0xffff, 32);
  } else {
    Ldr(temp, Immediate(reinterpret_cast<intptr_t>(target), rmode));
  }
  Blr(temp);
#ifdef DEBUG
  AssertSizeOfCodeGeneratedSince(&start_call, CallSize(target, rmode));
#endif
}

void TurboAssembler::Call(Handle<Code> code, RelocInfo::Mode rmode) {
#ifdef DEBUG
  Label start_call;
  Bind(&start_call);
#endif

  Call(code.address(), rmode);

#ifdef DEBUG
  // Check the size of the code generated.
  AssertSizeOfCodeGeneratedSince(&start_call, CallSize(code, rmode));
#endif
}

int TurboAssembler::CallSize(Register target) {
  USE(target);
  return kInstructionSize;
}

int TurboAssembler::CallSize(Label* target) {
  USE(target);
  return kInstructionSize;
}

int TurboAssembler::CallSize(Address target, RelocInfo::Mode rmode) {
  USE(target);

  // Addresses always have 64 bits, so we shouldn't encounter NONE32.
  DCHECK(rmode != RelocInfo::NONE32);

  if (rmode == RelocInfo::NONE64) {
    return kCallSizeWithoutRelocation;
  } else {
    return kCallSizeWithRelocation;
  }
}

int TurboAssembler::CallSize(Handle<Code> code, RelocInfo::Mode rmode) {
  USE(code);

  // Addresses always have 64 bits, so we shouldn't encounter NONE32.
  DCHECK(rmode != RelocInfo::NONE32);

  if (rmode == RelocInfo::NONE64) {
    return kCallSizeWithoutRelocation;
  } else {
    return kCallSizeWithRelocation;
  }
}


void MacroAssembler::JumpIfHeapNumber(Register object, Label* on_heap_number,
                                      SmiCheckType smi_check_type) {
  Label on_not_heap_number;

  if (smi_check_type == DO_SMI_CHECK) {
    JumpIfSmi(object, &on_not_heap_number);
  }

  AssertNotSmi(object);

  UseScratchRegisterScope temps(this);
  Register temp = temps.AcquireX();
  Ldr(temp, FieldMemOperand(object, HeapObject::kMapOffset));
  JumpIfRoot(temp, Heap::kHeapNumberMapRootIndex, on_heap_number);

  Bind(&on_not_heap_number);
}


void MacroAssembler::JumpIfNotHeapNumber(Register object,
                                         Label* on_not_heap_number,
                                         SmiCheckType smi_check_type) {
  if (smi_check_type == DO_SMI_CHECK) {
    JumpIfSmi(object, on_not_heap_number);
  }

  AssertNotSmi(object);

  UseScratchRegisterScope temps(this);
  Register temp = temps.AcquireX();
  Ldr(temp, FieldMemOperand(object, HeapObject::kMapOffset));
  JumpIfNotRoot(temp, Heap::kHeapNumberMapRootIndex, on_not_heap_number);
}

void MacroAssembler::TryRepresentDoubleAsInt(Register as_int, VRegister value,
                                             VRegister scratch_d,
                                             Label* on_successful_conversion,
                                             Label* on_failed_conversion) {
  // Convert to an int and back again, then compare with the original value.
  Fcvtzs(as_int, value);
  Scvtf(scratch_d, as_int);
  Fcmp(value, scratch_d);

  if (on_successful_conversion) {
    B(on_successful_conversion, eq);
  }
  if (on_failed_conversion) {
    B(on_failed_conversion, ne);
  }
}

void TurboAssembler::PrepareForTailCall(const ParameterCount& callee_args_count,
                                        Register caller_args_count_reg,
                                        Register scratch0, Register scratch1) {
#if DEBUG
  if (callee_args_count.is_reg()) {
    DCHECK(!AreAliased(callee_args_count.reg(), caller_args_count_reg, scratch0,
                       scratch1));
  } else {
    DCHECK(!AreAliased(caller_args_count_reg, scratch0, scratch1));
  }
#endif

  // Calculate the end of destination area where we will put the arguments
  // after we drop current frame. We add kPointerSize to count the receiver
  // argument which is not included into formal parameters count.
  Register dst_reg = scratch0;
  add(dst_reg, fp, Operand(caller_args_count_reg, LSL, kPointerSizeLog2));
  add(dst_reg, dst_reg,
      Operand(StandardFrameConstants::kCallerSPOffset + kPointerSize));

  Register src_reg = caller_args_count_reg;
  // Calculate the end of source area. +kPointerSize is for the receiver.
  if (callee_args_count.is_reg()) {
    add(src_reg, jssp, Operand(callee_args_count.reg(), LSL, kPointerSizeLog2));
    add(src_reg, src_reg, Operand(kPointerSize));
  } else {
    add(src_reg, jssp,
        Operand((callee_args_count.immediate() + 1) * kPointerSize));
  }

  if (FLAG_debug_code) {
    Cmp(src_reg, dst_reg);
    Check(lo, kStackAccessBelowStackPointer);
  }

  // Restore caller's frame pointer and return address now as they will be
  // overwritten by the copying loop.
  Ldr(lr, MemOperand(fp, StandardFrameConstants::kCallerPCOffset));
  Ldr(fp, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));

  // Now copy callee arguments to the caller frame going backwards to avoid
  // callee arguments corruption (source and destination areas could overlap).

  // Both src_reg and dst_reg are pointing to the word after the one to copy,
  // so they must be pre-decremented in the loop.
  Register tmp_reg = scratch1;
  Label loop, entry;
  B(&entry);
  bind(&loop);
  Ldr(tmp_reg, MemOperand(src_reg, -kPointerSize, PreIndex));
  Str(tmp_reg, MemOperand(dst_reg, -kPointerSize, PreIndex));
  bind(&entry);
  Cmp(jssp, src_reg);
  B(ne, &loop);

  // Leave current frame.
  Mov(jssp, dst_reg);
  SetStackPointer(jssp);
  AssertStackConsistency();
}

void MacroAssembler::InvokePrologue(const ParameterCount& expected,
                                    const ParameterCount& actual, Label* done,
                                    InvokeFlag flag,
                                    bool* definitely_mismatches) {
  bool definitely_matches = false;
  *definitely_mismatches = false;
  Label regular_invoke;

  // Check whether the expected and actual arguments count match. If not,
  // setup registers according to contract with ArgumentsAdaptorTrampoline:
  //  x0: actual arguments count.
  //  x1: function (passed through to callee).
  //  x2: expected arguments count.

  // The code below is made a lot easier because the calling code already sets
  // up actual and expected registers according to the contract if values are
  // passed in registers.
  DCHECK(actual.is_immediate() || actual.reg().is(x0));
  DCHECK(expected.is_immediate() || expected.reg().is(x2));

  if (expected.is_immediate()) {
    DCHECK(actual.is_immediate());
    Mov(x0, actual.immediate());
    if (expected.immediate() == actual.immediate()) {
      definitely_matches = true;

    } else {
      if (expected.immediate() ==
          SharedFunctionInfo::kDontAdaptArgumentsSentinel) {
        // Don't worry about adapting arguments for builtins that
        // don't want that done. Skip adaption code by making it look
        // like we have a match between expected and actual number of
        // arguments.
        definitely_matches = true;
      } else {
        *definitely_mismatches = true;
        // Set up x2 for the argument adaptor.
        Mov(x2, expected.immediate());
      }
    }

  } else {  // expected is a register.
    Operand actual_op = actual.is_immediate() ? Operand(actual.immediate())
                                              : Operand(actual.reg());
    Mov(x0, actual_op);
    // If actual == expected perform a regular invocation.
    Cmp(expected.reg(), actual_op);
    B(eq, &regular_invoke);
  }

  // If the argument counts may mismatch, generate a call to the argument
  // adaptor.
  if (!definitely_matches) {
    Handle<Code> adaptor = BUILTIN_CODE(isolate(), ArgumentsAdaptorTrampoline);
    if (flag == CALL_FUNCTION) {
      Call(adaptor);
      if (!*definitely_mismatches) {
        // If the arg counts don't match, no extra code is emitted by
        // MAsm::InvokeFunctionCode and we can just fall through.
        B(done);
      }
    } else {
      Jump(adaptor, RelocInfo::CODE_TARGET);
    }
  }
  Bind(&regular_invoke);
}

void MacroAssembler::CheckDebugHook(Register fun, Register new_target,
                                    const ParameterCount& expected,
                                    const ParameterCount& actual) {
  Label skip_hook;
  ExternalReference debug_hook_active =
      ExternalReference::debug_hook_on_function_call_address(isolate());
  Mov(x4, Operand(debug_hook_active));
  Ldrsb(x4, MemOperand(x4));
  CompareAndBranch(x4, Operand(0), eq, &skip_hook);
  {
    FrameScope frame(this,
                     has_frame() ? StackFrame::NONE : StackFrame::INTERNAL);
    if (expected.is_reg()) {
      SmiTag(expected.reg());
      Push(expected.reg());
    }
    if (actual.is_reg()) {
      SmiTag(actual.reg());
      Push(actual.reg());
    }
    if (new_target.is_valid()) {
      Push(new_target);
    }
    Push(fun);
    Push(fun);
    CallRuntime(Runtime::kDebugOnFunctionCall);
    Pop(fun);
    if (new_target.is_valid()) {
      Pop(new_target);
    }
    if (actual.is_reg()) {
      Pop(actual.reg());
      SmiUntag(actual.reg());
    }
    if (expected.is_reg()) {
      Pop(expected.reg());
      SmiUntag(expected.reg());
    }
  }
  bind(&skip_hook);
}

void MacroAssembler::InvokeFunctionCode(Register function, Register new_target,
                                        const ParameterCount& expected,
                                        const ParameterCount& actual,
                                        InvokeFlag flag) {
  // You can't call a function without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());
  DCHECK(function.is(x1));
  DCHECK_IMPLIES(new_target.is_valid(), new_target.is(x3));

  // On function call, call into the debugger if necessary.
  CheckDebugHook(function, new_target, expected, actual);

  // Clear the new.target register if not given.
  if (!new_target.is_valid()) {
    LoadRoot(x3, Heap::kUndefinedValueRootIndex);
  }

  Label done;
  bool definitely_mismatches = false;
  InvokePrologue(expected, actual, &done, flag, &definitely_mismatches);

  // If we are certain that actual != expected, then we know InvokePrologue will
  // have handled the call through the argument adaptor mechanism.
  // The called function expects the call kind in x5.
  if (!definitely_mismatches) {
    // We call indirectly through the code field in the function to
    // allow recompilation to take effect without changing any of the
    // call sites.
    Register code = x4;
    Ldr(code, FieldMemOperand(function, JSFunction::kCodeOffset));
    Add(code, code, Operand(Code::kHeaderSize - kHeapObjectTag));
    if (flag == CALL_FUNCTION) {
      Call(code);
    } else {
      DCHECK(flag == JUMP_FUNCTION);
      Jump(code);
    }
  }

  // Continue here if InvokePrologue does handle the invocation due to
  // mismatched parameter counts.
  Bind(&done);
}

void MacroAssembler::InvokeFunction(Register function, Register new_target,
                                    const ParameterCount& actual,
                                    InvokeFlag flag) {
  // You can't call a function without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());

  // Contract with called JS functions requires that function is passed in x1.
  // (See FullCodeGenerator::Generate().)
  DCHECK(function.is(x1));

  Register expected_reg = x2;

  Ldr(cp, FieldMemOperand(function, JSFunction::kContextOffset));
  // The number of arguments is stored as an int32_t, and -1 is a marker
  // (SharedFunctionInfo::kDontAdaptArgumentsSentinel), so we need sign
  // extension to correctly handle it.
  Ldr(expected_reg, FieldMemOperand(function,
                                    JSFunction::kSharedFunctionInfoOffset));
  Ldrsw(expected_reg,
        FieldMemOperand(expected_reg,
                        SharedFunctionInfo::kFormalParameterCountOffset));

  ParameterCount expected(expected_reg);
  InvokeFunctionCode(function, new_target, expected, actual, flag);
}

void MacroAssembler::InvokeFunction(Register function,
                                    const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    InvokeFlag flag) {
  // You can't call a function without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());

  // Contract with called JS functions requires that function is passed in x1.
  // (See FullCodeGenerator::Generate().)
  DCHECK(function.Is(x1));

  // Set up the context.
  Ldr(cp, FieldMemOperand(function, JSFunction::kContextOffset));

  InvokeFunctionCode(function, no_reg, expected, actual, flag);
}

void MacroAssembler::InvokeFunction(Handle<JSFunction> function,
                                    const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    InvokeFlag flag) {
  // Contract with called JS functions requires that function is passed in x1.
  // (See FullCodeGenerator::Generate().)
  LoadObject(x1, function);
  InvokeFunction(x1, expected, actual, flag);
}

void TurboAssembler::TryConvertDoubleToInt64(Register result,
                                             DoubleRegister double_input,
                                             Label* done) {
  // Try to convert with an FPU convert instruction. It's trivial to compute
  // the modulo operation on an integer register so we convert to a 64-bit
  // integer.
  //
  // Fcvtzs will saturate to INT64_MIN (0x800...00) or INT64_MAX (0x7ff...ff)
  // when the double is out of range. NaNs and infinities will be converted to 0
  // (as ECMA-262 requires).
  Fcvtzs(result.X(), double_input);

  // The values INT64_MIN (0x800...00) or INT64_MAX (0x7ff...ff) are not
  // representable using a double, so if the result is one of those then we know
  // that saturation occurred, and we need to manually handle the conversion.
  //
  // It is easy to detect INT64_MIN and INT64_MAX because adding or subtracting
  // 1 will cause signed overflow.
  Cmp(result.X(), 1);
  Ccmp(result.X(), -1, VFlag, vc);

  B(vc, done);
}

void TurboAssembler::TruncateDoubleToIDelayed(Zone* zone, Register result,
                                              DoubleRegister double_input) {
  Label done;

  // Try to convert the double to an int64. If successful, the bottom 32 bits
  // contain our truncated int32 result.
  TryConvertDoubleToInt64(result, double_input, &done);

  const Register old_stack_pointer = StackPointer();
  if (csp.Is(old_stack_pointer)) {
    // This currently only happens during compiler-unittest. If it arises
    // during regular code generation the DoubleToI stub should be updated to
    // cope with csp and have an extra parameter indicating which stack pointer
    // it should use.
    Push(jssp, xzr);  // Push xzr to maintain csp required 16-bytes alignment.
    Mov(jssp, csp);
    SetStackPointer(jssp);
  }

  // If we fell through then inline version didn't succeed - call stub instead.
  Push(lr, double_input);

  auto stub = new (zone) DoubleToIStub(nullptr, jssp, result, 0,
                                       true,   // is_truncating
                                       true);  // skip_fastpath
  // DoubleToIStub preserves any registers it needs to clobber.
  CallStubDelayed(stub);

  DCHECK_EQ(xzr.SizeInBytes(), double_input.SizeInBytes());
  Pop(xzr, lr);  // xzr to drop the double input on the stack.

  if (csp.Is(old_stack_pointer)) {
    Mov(csp, jssp);
    SetStackPointer(csp);
    AssertStackConsistency();
    Pop(xzr, jssp);
  }

  Bind(&done);
  // Keep our invariant that the upper 32 bits are zero.
  Uxtw(result.W(), result.W());
}

void TurboAssembler::Prologue() {
  Push(lr, fp, cp, x1);
  Add(fp, jssp, StandardFrameConstants::kFixedFrameSizeFromFp);
}

void TurboAssembler::EnterFrame(StackFrame::Type type) {
  UseScratchRegisterScope temps(this);
  Register type_reg = temps.AcquireX();
  Register code_reg = temps.AcquireX();

  if (type == StackFrame::INTERNAL) {
    DCHECK(jssp.Is(StackPointer()));
    Mov(type_reg, StackFrame::TypeToMarker(type));
    Mov(code_reg, Operand(CodeObject()));
    Push(lr, fp, type_reg, code_reg);
    Add(fp, jssp, InternalFrameConstants::kFixedFrameSizeFromFp);
    // jssp[4] : lr
    // jssp[3] : fp
    // jssp[1] : type
    // jssp[0] : [code object]
  } else if (type == StackFrame::WASM_COMPILED) {
    DCHECK(csp.Is(StackPointer()));
    Mov(type_reg, StackFrame::TypeToMarker(type));
    Push(lr, fp);
    Mov(fp, csp);
    Push(type_reg, padreg);
    // csp[3] : lr
    // csp[2] : fp
    // csp[1] : type
    // csp[0] : for alignment
  } else {
    DCHECK(jssp.Is(StackPointer()));
    Mov(type_reg, StackFrame::TypeToMarker(type));
    Push(lr, fp, type_reg);
    Add(fp, jssp, TypedFrameConstants::kFixedFrameSizeFromFp);
    // jssp[2] : lr
    // jssp[1] : fp
    // jssp[0] : type
  }
}

void TurboAssembler::LeaveFrame(StackFrame::Type type) {
  if (type == StackFrame::WASM_COMPILED) {
    DCHECK(csp.Is(StackPointer()));
    Mov(csp, fp);
    AssertStackConsistency();
    Pop(fp, lr);
  } else {
    DCHECK(jssp.Is(StackPointer()));
    // Drop the execution stack down to the frame pointer and restore
    // the caller frame pointer and return address.
    Mov(jssp, fp);
    AssertStackConsistency();
    Pop(fp, lr);
  }
}


void MacroAssembler::ExitFramePreserveFPRegs() {
  DCHECK_EQ(kCallerSavedV.Count() % 2, 0);
  PushCPURegList(kCallerSavedV);
}


void MacroAssembler::ExitFrameRestoreFPRegs() {
  // Read the registers from the stack without popping them. The stack pointer
  // will be reset as part of the unwinding process.
  CPURegList saved_fp_regs = kCallerSavedV;
  DCHECK(saved_fp_regs.Count() % 2 == 0);

  int offset = ExitFrameConstants::kLastExitFrameField;
  while (!saved_fp_regs.IsEmpty()) {
    const CPURegister& dst0 = saved_fp_regs.PopHighestIndex();
    const CPURegister& dst1 = saved_fp_regs.PopHighestIndex();
    offset -= 2 * kDRegSize;
    Ldp(dst1, dst0, MemOperand(fp, offset));
  }
}

void MacroAssembler::EnterExitFrame(bool save_doubles, const Register& scratch,
                                    int extra_space,
                                    StackFrame::Type frame_type) {
  DCHECK(jssp.Is(StackPointer()));
  DCHECK(frame_type == StackFrame::EXIT ||
         frame_type == StackFrame::BUILTIN_EXIT);

  // Set up the new stack frame.
  Push(lr, fp);
  Mov(fp, StackPointer());
  Mov(scratch, StackFrame::TypeToMarker(frame_type));
  Push(scratch, xzr);
  Mov(scratch, Operand(CodeObject()));
  Push(scratch, padreg);
  //          fp[8]: CallerPC (lr)
  //    fp -> fp[0]: CallerFP (old fp)
  //          fp[-8]: STUB marker
  //          fp[-16]: Space reserved for SPOffset.
  //          fp[-24]: CodeObject()
  //  jssp -> fp[-32]: padding
  STATIC_ASSERT((2 * kPointerSize) == ExitFrameConstants::kCallerSPOffset);
  STATIC_ASSERT((1 * kPointerSize) == ExitFrameConstants::kCallerPCOffset);
  STATIC_ASSERT((0 * kPointerSize) == ExitFrameConstants::kCallerFPOffset);
  STATIC_ASSERT((-2 * kPointerSize) == ExitFrameConstants::kSPOffset);
  STATIC_ASSERT((-3 * kPointerSize) == ExitFrameConstants::kCodeOffset);
  STATIC_ASSERT((-4 * kPointerSize) == ExitFrameConstants::kPaddingOffset);

  // Save the frame pointer and context pointer in the top frame.
  Mov(scratch, Operand(ExternalReference(IsolateAddressId::kCEntryFPAddress,
                                         isolate())));
  Str(fp, MemOperand(scratch));
  Mov(scratch,
      Operand(ExternalReference(IsolateAddressId::kContextAddress, isolate())));
  Str(cp, MemOperand(scratch));

  STATIC_ASSERT((-4 * kPointerSize) == ExitFrameConstants::kLastExitFrameField);
  if (save_doubles) {
    ExitFramePreserveFPRegs();
  }

  // Round the number of space we need to claim to a multiple of two.
  int slots_to_claim = RoundUp(extra_space + 1, 2);

  // Reserve space for the return address and for user requested memory.
  // We do this before aligning to make sure that we end up correctly
  // aligned with the minimum of wasted space.
  Claim(slots_to_claim, kXRegSize);
  //         fp[8]: CallerPC (lr)
  //   fp -> fp[0]: CallerFP (old fp)
  //         fp[-8]: STUB marker
  //         fp[-16]: Space reserved for SPOffset.
  //         fp[-24]: CodeObject()
  //         fp[-24 - fp_size]: Saved doubles (if save_doubles is true).
  //         jssp[8]: Extra space reserved for caller (if extra_space != 0).
  // jssp -> jssp[0]: Space reserved for the return address.

  // Align and synchronize the system stack pointer with jssp.
  AlignAndSetCSPForFrame();
  DCHECK(csp.Is(StackPointer()));

  //         fp[8]: CallerPC (lr)
  //   fp -> fp[0]: CallerFP (old fp)
  //         fp[-8]: STUB marker
  //         fp[-16]: Space reserved for SPOffset.
  //         fp[-24]: CodeObject()
  //         fp[-24 - fp_size]: Saved doubles (if save_doubles is true).
  //         csp[8]: Memory reserved for the caller if extra_space != 0.
  //                 Alignment padding, if necessary.
  //  csp -> csp[0]: Space reserved for the return address.

  // ExitFrame::GetStateForFramePointer expects to find the return address at
  // the memory address immediately below the pointer stored in SPOffset.
  // It is not safe to derive much else from SPOffset, because the size of the
  // padding can vary.
  Add(scratch, csp, kXRegSize);
  Str(scratch, MemOperand(fp, ExitFrameConstants::kSPOffset));
}


// Leave the current exit frame.
void MacroAssembler::LeaveExitFrame(bool restore_doubles,
                                    const Register& scratch,
                                    bool restore_context) {
  DCHECK(csp.Is(StackPointer()));

  if (restore_doubles) {
    ExitFrameRestoreFPRegs();
  }

  // Restore the context pointer from the top frame.
  if (restore_context) {
    Mov(scratch, Operand(ExternalReference(IsolateAddressId::kContextAddress,
                                           isolate())));
    Ldr(cp, MemOperand(scratch));
  }

  if (emit_debug_code()) {
    // Also emit debug code to clear the cp in the top frame.
    Mov(scratch, Operand(ExternalReference(IsolateAddressId::kContextAddress,
                                           isolate())));
    Str(xzr, MemOperand(scratch));
  }
  // Clear the frame pointer from the top frame.
  Mov(scratch, Operand(ExternalReference(IsolateAddressId::kCEntryFPAddress,
                                         isolate())));
  Str(xzr, MemOperand(scratch));

  // Pop the exit frame.
  //         fp[8]: CallerPC (lr)
  //   fp -> fp[0]: CallerFP (old fp)
  //         fp[...]: The rest of the frame.
  Mov(jssp, fp);
  SetStackPointer(jssp);
  AssertStackConsistency();
  Pop(fp, lr);
}


void MacroAssembler::IncrementCounter(StatsCounter* counter, int value,
                                      Register scratch1, Register scratch2) {
  DCHECK(value != 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    Mov(scratch2, ExternalReference(counter));
    Ldr(scratch1.W(), MemOperand(scratch2));
    Add(scratch1.W(), scratch1.W(), value);
    Str(scratch1.W(), MemOperand(scratch2));
  }
}


void MacroAssembler::DecrementCounter(StatsCounter* counter, int value,
                                      Register scratch1, Register scratch2) {
  IncrementCounter(counter, -value, scratch1, scratch2);
}

void MacroAssembler::MaybeDropFrames() {
  // Check whether we need to drop frames to restart a function on the stack.
  ExternalReference restart_fp =
      ExternalReference::debug_restart_fp_address(isolate());
  Mov(x1, Operand(restart_fp));
  Ldr(x1, MemOperand(x1));
  Tst(x1, x1);
  Jump(BUILTIN_CODE(isolate(), FrameDropperTrampoline), RelocInfo::CODE_TARGET,
       ne);
}

void MacroAssembler::JumpIfObjectType(Register object,
                                      Register map,
                                      Register type_reg,
                                      InstanceType type,
                                      Label* if_cond_pass,
                                      Condition cond) {
  CompareObjectType(object, map, type_reg, type);
  B(cond, if_cond_pass);
}


// Sets condition flags based on comparison, and returns type in type_reg.
void MacroAssembler::CompareObjectType(Register object,
                                       Register map,
                                       Register type_reg,
                                       InstanceType type) {
  Ldr(map, FieldMemOperand(object, HeapObject::kMapOffset));
  CompareInstanceType(map, type_reg, type);
}


// Sets condition flags based on comparison, and returns type in type_reg.
void MacroAssembler::CompareInstanceType(Register map,
                                         Register type_reg,
                                         InstanceType type) {
  Ldrb(type_reg, FieldMemOperand(map, Map::kInstanceTypeOffset));
  Cmp(type_reg, type);
}


void MacroAssembler::GetWeakValue(Register value, Handle<WeakCell> cell) {
  Mov(value, Operand(cell));
  Ldr(value, FieldMemOperand(value, WeakCell::kValueOffset));
}


void MacroAssembler::LoadWeakValue(Register value, Handle<WeakCell> cell,
                                   Label* miss) {
  GetWeakValue(value, cell);
  JumpIfSmi(value, miss);
}

void MacroAssembler::LoadElementsKindFromMap(Register result, Register map) {
  // Load the map's "bit field 2".
  Ldrb(result, FieldMemOperand(map, Map::kBitField2Offset));
  // Retrieve elements_kind from bit field 2.
  DecodeField<Map::ElementsKindBits>(result);
}

void MacroAssembler::GetMapConstructor(Register result, Register map,
                                       Register temp, Register temp2) {
  Label done, loop;
  Ldr(result, FieldMemOperand(map, Map::kConstructorOrBackPointerOffset));
  Bind(&loop);
  JumpIfSmi(result, &done);
  CompareObjectType(result, temp, temp2, MAP_TYPE);
  B(ne, &done);
  Ldr(result, FieldMemOperand(result, Map::kConstructorOrBackPointerOffset));
  B(&loop);
  Bind(&done);
}

void MacroAssembler::CompareRoot(const Register& obj,
                                 Heap::RootListIndex index) {
  UseScratchRegisterScope temps(this);
  Register temp = temps.AcquireX();
  DCHECK(!AreAliased(obj, temp));
  LoadRoot(temp, index);
  Cmp(obj, temp);
}


void MacroAssembler::JumpIfRoot(const Register& obj,
                                Heap::RootListIndex index,
                                Label* if_equal) {
  CompareRoot(obj, index);
  B(eq, if_equal);
}


void MacroAssembler::JumpIfNotRoot(const Register& obj,
                                   Heap::RootListIndex index,
                                   Label* if_not_equal) {
  CompareRoot(obj, index);
  B(ne, if_not_equal);
}


void MacroAssembler::CompareAndSplit(const Register& lhs,
                                     const Operand& rhs,
                                     Condition cond,
                                     Label* if_true,
                                     Label* if_false,
                                     Label* fall_through) {
  if ((if_true == if_false) && (if_false == fall_through)) {
    // Fall through.
  } else if (if_true == if_false) {
    B(if_true);
  } else if (if_false == fall_through) {
    CompareAndBranch(lhs, rhs, cond, if_true);
  } else if (if_true == fall_through) {
    CompareAndBranch(lhs, rhs, NegateCondition(cond), if_false);
  } else {
    CompareAndBranch(lhs, rhs, cond, if_true);
    B(if_false);
  }
}


void MacroAssembler::TestAndSplit(const Register& reg,
                                  uint64_t bit_pattern,
                                  Label* if_all_clear,
                                  Label* if_any_set,
                                  Label* fall_through) {
  if ((if_all_clear == if_any_set) && (if_any_set == fall_through)) {
    // Fall through.
  } else if (if_all_clear == if_any_set) {
    B(if_all_clear);
  } else if (if_all_clear == fall_through) {
    TestAndBranchIfAnySet(reg, bit_pattern, if_any_set);
  } else if (if_any_set == fall_through) {
    TestAndBranchIfAllClear(reg, bit_pattern, if_all_clear);
  } else {
    TestAndBranchIfAnySet(reg, bit_pattern, if_any_set);
    B(if_all_clear);
  }
}

bool TurboAssembler::AllowThisStubCall(CodeStub* stub) {
  return has_frame() || !stub->SometimesSetsUpAFrame();
}

void MacroAssembler::RememberedSetHelper(Register object,  // For debug tests.
                                         Register address, Register scratch1,
                                         SaveFPRegsMode fp_mode) {
  DCHECK(!AreAliased(object, address, scratch1));
  Label done, store_buffer_overflow;
  if (emit_debug_code()) {
    Label ok;
    JumpIfNotInNewSpace(object, &ok);
    Abort(kRememberedSetPointerInNewSpace);
    bind(&ok);
  }
  UseScratchRegisterScope temps(this);
  Register scratch2 = temps.AcquireX();

  // Load store buffer top.
  Mov(scratch2, ExternalReference::store_buffer_top(isolate()));
  Ldr(scratch1, MemOperand(scratch2));
  // Store pointer to buffer and increment buffer top.
  Str(address, MemOperand(scratch1, kPointerSize, PostIndex));
  // Write back new top of buffer.
  Str(scratch1, MemOperand(scratch2));
  // Call stub on end of buffer.
  // Check for end of buffer.
  Tst(scratch1, StoreBuffer::kStoreBufferMask);
  B(eq, &store_buffer_overflow);
  Ret();

  Bind(&store_buffer_overflow);
  Push(lr);
  StoreBufferOverflowStub store_buffer_overflow_stub(isolate(), fp_mode);
  CallStub(&store_buffer_overflow_stub);
  Pop(lr);

  Bind(&done);
  Ret();
}


void MacroAssembler::PopSafepointRegisters() {
  const int num_unsaved = kNumSafepointRegisters - kNumSafepointSavedRegisters;
  DCHECK_GE(num_unsaved, 0);
  DCHECK_EQ(num_unsaved % 2, 0);
  DCHECK_EQ(kSafepointSavedRegisters % 2, 0);
  PopXRegList(kSafepointSavedRegisters);
  Drop(num_unsaved);
}


void MacroAssembler::PushSafepointRegisters() {
  // Safepoints expect a block of kNumSafepointRegisters values on the stack, so
  // adjust the stack for unsaved registers.
  const int num_unsaved = kNumSafepointRegisters - kNumSafepointSavedRegisters;
  DCHECK_GE(num_unsaved, 0);
  DCHECK_EQ(num_unsaved % 2, 0);
  DCHECK_EQ(kSafepointSavedRegisters % 2, 0);
  Claim(num_unsaved);
  PushXRegList(kSafepointSavedRegisters);
}

int MacroAssembler::SafepointRegisterStackIndex(int reg_code) {
  // Make sure the safepoint registers list is what we expect.
  DCHECK(CPURegList::GetSafepointSavedRegisters().list() == 0x6ffcffff);

  // Safepoint registers are stored contiguously on the stack, but not all the
  // registers are saved. The following registers are excluded:
  //  - x16 and x17 (ip0 and ip1) because they shouldn't be preserved outside of
  //    the macro assembler.
  //  - x28 (jssp) because JS stack pointer doesn't need to be included in
  //    safepoint registers.
  //  - x31 (csp) because the system stack pointer doesn't need to be included
  //    in safepoint registers.
  //
  // This function implements the mapping of register code to index into the
  // safepoint register slots.
  if ((reg_code >= 0) && (reg_code <= 15)) {
    return reg_code;
  } else if ((reg_code >= 18) && (reg_code <= 27)) {
    // Skip ip0 and ip1.
    return reg_code - 2;
  } else if ((reg_code == 29) || (reg_code == 30)) {
    // Also skip jssp.
    return reg_code - 3;
  } else {
    // This register has no safepoint register slot.
    UNREACHABLE();
  }
}

void MacroAssembler::CheckPageFlag(const Register& object,
                                   const Register& scratch, int mask,
                                   Condition cc, Label* condition_met) {
  And(scratch, object, ~Page::kPageAlignmentMask);
  Ldr(scratch, MemOperand(scratch, MemoryChunk::kFlagsOffset));
  if (cc == eq) {
    TestAndBranchIfAnySet(scratch, mask, condition_met);
  } else {
    TestAndBranchIfAllClear(scratch, mask, condition_met);
  }
}

void TurboAssembler::CheckPageFlagSet(const Register& object,
                                      const Register& scratch, int mask,
                                      Label* if_any_set) {
  And(scratch, object, ~Page::kPageAlignmentMask);
  Ldr(scratch, MemOperand(scratch, MemoryChunk::kFlagsOffset));
  TestAndBranchIfAnySet(scratch, mask, if_any_set);
}

void TurboAssembler::CheckPageFlagClear(const Register& object,
                                        const Register& scratch, int mask,
                                        Label* if_all_clear) {
  And(scratch, object, ~Page::kPageAlignmentMask);
  Ldr(scratch, MemOperand(scratch, MemoryChunk::kFlagsOffset));
  TestAndBranchIfAllClear(scratch, mask, if_all_clear);
}

void MacroAssembler::RecordWriteField(Register object, int offset,
                                      Register value, Register scratch,
                                      LinkRegisterStatus lr_status,
                                      SaveFPRegsMode save_fp,
                                      RememberedSetAction remembered_set_action,
                                      SmiCheck smi_check) {
  // First, check if a write barrier is even needed. The tests below
  // catch stores of Smis.
  Label done;

  // Skip the barrier if writing a smi.
  if (smi_check == INLINE_SMI_CHECK) {
    JumpIfSmi(value, &done);
  }

  // Although the object register is tagged, the offset is relative to the start
  // of the object, so offset must be a multiple of kPointerSize.
  DCHECK(IsAligned(offset, kPointerSize));

  Add(scratch, object, offset - kHeapObjectTag);
  if (emit_debug_code()) {
    Label ok;
    Tst(scratch, kPointerSize - 1);
    B(eq, &ok);
    Abort(kUnalignedCellInWriteBarrier);
    Bind(&ok);
  }

  RecordWrite(object, scratch, value, lr_status, save_fp, remembered_set_action,
              OMIT_SMI_CHECK);

  Bind(&done);

  // Clobber clobbered input registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    Mov(value, Operand(bit_cast<int64_t>(kZapValue + 4)));
    Mov(scratch, Operand(bit_cast<int64_t>(kZapValue + 8)));
  }
}

void TurboAssembler::SaveRegisters(RegList registers) {
  DCHECK(NumRegs(registers) > 0);
  CPURegList regs(lr);
  for (int i = 0; i < Register::kNumRegisters; ++i) {
    if ((registers >> i) & 1u) {
      regs.Combine(Register::XRegFromCode(i));
    }
  }

  PushCPURegList(regs);
}

void TurboAssembler::RestoreRegisters(RegList registers) {
  DCHECK(NumRegs(registers) > 0);
  CPURegList regs(lr);
  for (int i = 0; i < Register::kNumRegisters; ++i) {
    if ((registers >> i) & 1u) {
      regs.Combine(Register::XRegFromCode(i));
    }
  }

  PopCPURegList(regs);
}

void TurboAssembler::CallRecordWriteStub(
    Register object, Register address,
    RememberedSetAction remembered_set_action, SaveFPRegsMode fp_mode) {
  // TODO(albertnetymk): For now we ignore remembered_set_action and fp_mode,
  // i.e. always emit remember set and save FP registers in RecordWriteStub. If
  // large performance regression is observed, we should use these values to
  // avoid unnecessary work.

  Callable const callable =
      Builtins::CallableFor(isolate(), Builtins::kRecordWrite);
  RegList registers = callable.descriptor().allocatable_registers();

  SaveRegisters(registers);

  Register object_parameter(callable.descriptor().GetRegisterParameter(
      RecordWriteDescriptor::kObject));
  Register slot_parameter(
      callable.descriptor().GetRegisterParameter(RecordWriteDescriptor::kSlot));
  Register isolate_parameter(callable.descriptor().GetRegisterParameter(
      RecordWriteDescriptor::kIsolate));
  Register remembered_set_parameter(callable.descriptor().GetRegisterParameter(
      RecordWriteDescriptor::kRememberedSet));
  Register fp_mode_parameter(callable.descriptor().GetRegisterParameter(
      RecordWriteDescriptor::kFPMode));

  Push(object);
  Push(address);

  Pop(slot_parameter);
  Pop(object_parameter);

  Mov(isolate_parameter, ExternalReference::isolate_address(isolate()));
  Move(remembered_set_parameter, Smi::FromEnum(remembered_set_action));
  Move(fp_mode_parameter, Smi::FromEnum(fp_mode));
  Call(callable.code(), RelocInfo::CODE_TARGET);

  RestoreRegisters(registers);
}

// Will clobber: object, address, value.
// If lr_status is kLRHasBeenSaved, lr will also be clobbered.
//
// The register 'object' contains a heap object pointer. The heap object tag is
// shifted away.
void MacroAssembler::RecordWrite(Register object, Register address,
                                 Register value, LinkRegisterStatus lr_status,
                                 SaveFPRegsMode fp_mode,
                                 RememberedSetAction remembered_set_action,
                                 SmiCheck smi_check) {
  ASM_LOCATION_IN_ASSEMBLER("MacroAssembler::RecordWrite");
  DCHECK(!AreAliased(object, value));

  if (emit_debug_code()) {
    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireX();

    Ldr(temp, MemOperand(address));
    Cmp(temp, value);
    Check(eq, kWrongAddressOrValuePassedToRecordWrite);
  }

  // First, check if a write barrier is even needed. The tests below
  // catch stores of smis and stores into the young generation.
  Label done;

  if (smi_check == INLINE_SMI_CHECK) {
    DCHECK_EQ(0, kSmiTag);
    JumpIfSmi(value, &done);
  }

  CheckPageFlagClear(value,
                     value,  // Used as scratch.
                     MemoryChunk::kPointersToHereAreInterestingMask, &done);
  CheckPageFlagClear(object,
                     value,  // Used as scratch.
                     MemoryChunk::kPointersFromHereAreInterestingMask,
                     &done);

  // Record the actual write.
  if (lr_status == kLRHasNotBeenSaved) {
    Push(padreg, lr);
  }
#ifdef V8_CSA_WRITE_BARRIER
  CallRecordWriteStub(object, address, remembered_set_action, fp_mode);
#else
  RecordWriteStub stub(isolate(), object, value, address, remembered_set_action,
                       fp_mode);
  CallStub(&stub);
#endif
  if (lr_status == kLRHasNotBeenSaved) {
    Pop(lr, padreg);
  }

  Bind(&done);

  // Count number of write barriers in generated code.
  isolate()->counters()->write_barriers_static()->Increment();
  IncrementCounter(isolate()->counters()->write_barriers_dynamic(), 1, address,
                   value);

  // Clobber clobbered registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    Mov(address, Operand(bit_cast<int64_t>(kZapValue + 12)));
    Mov(value, Operand(bit_cast<int64_t>(kZapValue + 16)));
  }
}


void MacroAssembler::AssertHasValidColor(const Register& reg) {
  if (emit_debug_code()) {
    // The bit sequence is backward. The first character in the string
    // represents the least significant bit.
    DCHECK(strcmp(Marking::kImpossibleBitPattern, "01") == 0);

    Label color_is_valid;
    Tbnz(reg, 0, &color_is_valid);
    Tbz(reg, 1, &color_is_valid);
    Abort(kUnexpectedColorFound);
    Bind(&color_is_valid);
  }
}


void MacroAssembler::GetMarkBits(Register addr_reg,
                                 Register bitmap_reg,
                                 Register shift_reg) {
  DCHECK(!AreAliased(addr_reg, bitmap_reg, shift_reg));
  DCHECK(addr_reg.Is64Bits() && bitmap_reg.Is64Bits() && shift_reg.Is64Bits());
  // addr_reg is divided into fields:
  // |63        page base        20|19    high      8|7   shift   3|2  0|
  // 'high' gives the index of the cell holding color bits for the object.
  // 'shift' gives the offset in the cell for this object's color.
  const int kShiftBits = kPointerSizeLog2 + Bitmap::kBitsPerCellLog2;
  UseScratchRegisterScope temps(this);
  Register temp = temps.AcquireX();
  Ubfx(temp, addr_reg, kShiftBits, kPageSizeBits - kShiftBits);
  Bic(bitmap_reg, addr_reg, Page::kPageAlignmentMask);
  Add(bitmap_reg, bitmap_reg, Operand(temp, LSL, Bitmap::kBytesPerCellLog2));
  // bitmap_reg:
  // |63        page base        20|19 zeros 15|14      high      3|2  0|
  Ubfx(shift_reg, addr_reg, kPointerSizeLog2, Bitmap::kBitsPerCellLog2);
}


void MacroAssembler::HasColor(Register object,
                              Register bitmap_scratch,
                              Register shift_scratch,
                              Label* has_color,
                              int first_bit,
                              int second_bit) {
  // See mark-compact.h for color definitions.
  DCHECK(!AreAliased(object, bitmap_scratch, shift_scratch));

  GetMarkBits(object, bitmap_scratch, shift_scratch);
  Ldr(bitmap_scratch, MemOperand(bitmap_scratch, MemoryChunk::kHeaderSize));
  // Shift the bitmap down to get the color of the object in bits [1:0].
  Lsr(bitmap_scratch, bitmap_scratch, shift_scratch);

  AssertHasValidColor(bitmap_scratch);

  // These bit sequences are backwards. The first character in the string
  // represents the least significant bit.
  DCHECK(strcmp(Marking::kWhiteBitPattern, "00") == 0);
  DCHECK(strcmp(Marking::kBlackBitPattern, "11") == 0);
  DCHECK(strcmp(Marking::kGreyBitPattern, "10") == 0);

  // Check for the color.
  if (first_bit == 0) {
    // Checking for white.
    DCHECK(second_bit == 0);
    // We only need to test the first bit.
    Tbz(bitmap_scratch, 0, has_color);
  } else {
    Label other_color;
    // Checking for grey or black.
    Tbz(bitmap_scratch, 0, &other_color);
    if (second_bit == 0) {
      Tbz(bitmap_scratch, 1, has_color);
    } else {
      Tbnz(bitmap_scratch, 1, has_color);
    }
    Bind(&other_color);
  }

  // Fall through if it does not have the right color.
}


void MacroAssembler::JumpIfBlack(Register object,
                                 Register scratch0,
                                 Register scratch1,
                                 Label* on_black) {
  DCHECK(strcmp(Marking::kBlackBitPattern, "11") == 0);
  HasColor(object, scratch0, scratch1, on_black, 1, 1);  // kBlackBitPattern.
}

void MacroAssembler::JumpIfWhite(Register value, Register bitmap_scratch,
                                 Register shift_scratch, Register load_scratch,
                                 Register length_scratch,
                                 Label* value_is_white) {
  DCHECK(!AreAliased(
      value, bitmap_scratch, shift_scratch, load_scratch, length_scratch));

  // These bit sequences are backwards. The first character in the string
  // represents the least significant bit.
  DCHECK(strcmp(Marking::kWhiteBitPattern, "00") == 0);
  DCHECK(strcmp(Marking::kBlackBitPattern, "11") == 0);
  DCHECK(strcmp(Marking::kGreyBitPattern, "10") == 0);

  GetMarkBits(value, bitmap_scratch, shift_scratch);
  Ldr(load_scratch, MemOperand(bitmap_scratch, MemoryChunk::kHeaderSize));
  Lsr(load_scratch, load_scratch, shift_scratch);

  AssertHasValidColor(load_scratch);

  // If the value is black or grey we don't need to do anything.
  // Since both black and grey have a 1 in the first position and white does
  // not have a 1 there we only need to check one bit.
  Tbz(load_scratch, 0, value_is_white);
}

void TurboAssembler::Assert(Condition cond, BailoutReason reason) {
  if (emit_debug_code()) {
    Check(cond, reason);
  }
}

void MacroAssembler::AssertRegisterIsRoot(Register reg,
                                          Heap::RootListIndex index,
                                          BailoutReason reason) {
  if (emit_debug_code()) {
    CompareRoot(reg, index);
    Check(eq, reason);
  }
}

void TurboAssembler::Check(Condition cond, BailoutReason reason) {
  Label ok;
  B(cond, &ok);
  Abort(reason);
  // Will not return here.
  Bind(&ok);
}

void TurboAssembler::Abort(BailoutReason reason) {
#ifdef DEBUG
  RecordComment("Abort message: ");
  RecordComment(GetBailoutReason(reason));

  if (FLAG_trap_on_abort) {
    Brk(0);
    return;
  }
#endif

  // Abort is used in some contexts where csp is the stack pointer. In order to
  // simplify the CallRuntime code, make sure that jssp is the stack pointer.
  // There is no risk of register corruption here because Abort doesn't return.
  Register old_stack_pointer = StackPointer();
  SetStackPointer(jssp);
  Mov(jssp, old_stack_pointer);

  // We need some scratch registers for the MacroAssembler, so make sure we have
  // some. This is safe here because Abort never returns.
  RegList old_tmp_list = TmpList()->list();
  TmpList()->Combine(MacroAssembler::DefaultTmpList());

  if (use_real_aborts()) {
    // Avoid infinite recursion; Push contains some assertions that use Abort.
    NoUseRealAbortsScope no_real_aborts(this);

    Move(x1, Smi::FromInt(static_cast<int>(reason)));

    if (!has_frame_) {
      // We don't actually want to generate a pile of code for this, so just
      // claim there is a stack frame, without generating one.
      FrameScope scope(this, StackFrame::NONE);
      Call(BUILTIN_CODE(isolate(), Abort), RelocInfo::CODE_TARGET);
    } else {
      Call(BUILTIN_CODE(isolate(), Abort), RelocInfo::CODE_TARGET);
    }
  } else {
    // Load the string to pass to Printf.
    Label msg_address;
    Adr(x0, &msg_address);

    // Call Printf directly to report the error.
    CallPrintf();

    // We need a way to stop execution on both the simulator and real hardware,
    // and Unreachable() is the best option.
    Unreachable();

    // Emit the message string directly in the instruction stream.
    {
      BlockPoolsScope scope(this);
      Bind(&msg_address);
      EmitStringData(GetBailoutReason(reason));
    }
  }

  SetStackPointer(old_stack_pointer);
  TmpList()->set_list(old_tmp_list);
}

void MacroAssembler::LoadNativeContextSlot(int index, Register dst) {
  Ldr(dst, NativeContextMemOperand());
  Ldr(dst, ContextMemOperand(dst, index));
}


// This is the main Printf implementation. All other Printf variants call
// PrintfNoPreserve after setting up one or more PreserveRegisterScopes.
void MacroAssembler::PrintfNoPreserve(const char * format,
                                      const CPURegister& arg0,
                                      const CPURegister& arg1,
                                      const CPURegister& arg2,
                                      const CPURegister& arg3) {
  // We cannot handle a caller-saved stack pointer. It doesn't make much sense
  // in most cases anyway, so this restriction shouldn't be too serious.
  DCHECK(!kCallerSaved.IncludesAliasOf(StackPointer()));

  // The provided arguments, and their proper procedure-call standard registers.
  CPURegister args[kPrintfMaxArgCount] = {arg0, arg1, arg2, arg3};
  CPURegister pcs[kPrintfMaxArgCount] = {NoReg, NoReg, NoReg, NoReg};

  int arg_count = kPrintfMaxArgCount;

  // The PCS varargs registers for printf. Note that x0 is used for the printf
  // format string.
  static const CPURegList kPCSVarargs =
      CPURegList(CPURegister::kRegister, kXRegSizeInBits, 1, arg_count);
  static const CPURegList kPCSVarargsFP =
      CPURegList(CPURegister::kVRegister, kDRegSizeInBits, 0, arg_count - 1);

  // We can use caller-saved registers as scratch values, except for the
  // arguments and the PCS registers where they might need to go.
  CPURegList tmp_list = kCallerSaved;
  tmp_list.Remove(x0);      // Used to pass the format string.
  tmp_list.Remove(kPCSVarargs);
  tmp_list.Remove(arg0, arg1, arg2, arg3);

  CPURegList fp_tmp_list = kCallerSavedV;
  fp_tmp_list.Remove(kPCSVarargsFP);
  fp_tmp_list.Remove(arg0, arg1, arg2, arg3);

  // Override the MacroAssembler's scratch register list. The lists will be
  // reset automatically at the end of the UseScratchRegisterScope.
  UseScratchRegisterScope temps(this);
  TmpList()->set_list(tmp_list.list());
  FPTmpList()->set_list(fp_tmp_list.list());

  // Copies of the printf vararg registers that we can pop from.
  CPURegList pcs_varargs = kPCSVarargs;
  CPURegList pcs_varargs_fp = kPCSVarargsFP;

  // Place the arguments. There are lots of clever tricks and optimizations we
  // could use here, but Printf is a debug tool so instead we just try to keep
  // it simple: Move each input that isn't already in the right place to a
  // scratch register, then move everything back.
  for (unsigned i = 0; i < kPrintfMaxArgCount; i++) {
    // Work out the proper PCS register for this argument.
    if (args[i].IsRegister()) {
      pcs[i] = pcs_varargs.PopLowestIndex().X();
      // We might only need a W register here. We need to know the size of the
      // argument so we can properly encode it for the simulator call.
      if (args[i].Is32Bits()) pcs[i] = pcs[i].W();
    } else if (args[i].IsVRegister()) {
      // In C, floats are always cast to doubles for varargs calls.
      pcs[i] = pcs_varargs_fp.PopLowestIndex().D();
    } else {
      DCHECK(args[i].IsNone());
      arg_count = i;
      break;
    }

    // If the argument is already in the right place, leave it where it is.
    if (args[i].Aliases(pcs[i])) continue;

    // Otherwise, if the argument is in a PCS argument register, allocate an
    // appropriate scratch register and then move it out of the way.
    if (kPCSVarargs.IncludesAliasOf(args[i]) ||
        kPCSVarargsFP.IncludesAliasOf(args[i])) {
      if (args[i].IsRegister()) {
        Register old_arg = args[i].Reg();
        Register new_arg = temps.AcquireSameSizeAs(old_arg);
        Mov(new_arg, old_arg);
        args[i] = new_arg;
      } else {
        VRegister old_arg = args[i].VReg();
        VRegister new_arg = temps.AcquireSameSizeAs(old_arg);
        Fmov(new_arg, old_arg);
        args[i] = new_arg;
      }
    }
  }

  // Do a second pass to move values into their final positions and perform any
  // conversions that may be required.
  for (int i = 0; i < arg_count; i++) {
    DCHECK(pcs[i].type() == args[i].type());
    if (pcs[i].IsRegister()) {
      Mov(pcs[i].Reg(), args[i].Reg(), kDiscardForSameWReg);
    } else {
      DCHECK(pcs[i].IsVRegister());
      if (pcs[i].SizeInBytes() == args[i].SizeInBytes()) {
        Fmov(pcs[i].VReg(), args[i].VReg());
      } else {
        Fcvt(pcs[i].VReg(), args[i].VReg());
      }
    }
  }

  // Load the format string into x0, as per the procedure-call standard.
  //
  // To make the code as portable as possible, the format string is encoded
  // directly in the instruction stream. It might be cleaner to encode it in a
  // literal pool, but since Printf is usually used for debugging, it is
  // beneficial for it to be minimally dependent on other features.
  Label format_address;
  Adr(x0, &format_address);

  // Emit the format string directly in the instruction stream.
  { BlockPoolsScope scope(this);
    Label after_data;
    B(&after_data);
    Bind(&format_address);
    EmitStringData(format);
    Unreachable();
    Bind(&after_data);
  }

  // We don't pass any arguments on the stack, but we still need to align the C
  // stack pointer to a 16-byte boundary for PCS compliance.
  if (!csp.Is(StackPointer())) {
    Bic(csp, StackPointer(), 0xf);
  }

  CallPrintf(arg_count, pcs);
}

void TurboAssembler::CallPrintf(int arg_count, const CPURegister* args) {
// A call to printf needs special handling for the simulator, since the system
// printf function will use a different instruction set and the procedure-call
// standard will not be compatible.
#ifdef USE_SIMULATOR
  { InstructionAccurateScope scope(this, kPrintfLength / kInstructionSize);
    hlt(kImmExceptionIsPrintf);
    dc32(arg_count);          // kPrintfArgCountOffset

    // Determine the argument pattern.
    uint32_t arg_pattern_list = 0;
    for (int i = 0; i < arg_count; i++) {
      uint32_t arg_pattern;
      if (args[i].IsRegister()) {
        arg_pattern = args[i].Is32Bits() ? kPrintfArgW : kPrintfArgX;
      } else {
        DCHECK(args[i].Is64Bits());
        arg_pattern = kPrintfArgD;
      }
      DCHECK(arg_pattern < (1 << kPrintfArgPatternBits));
      arg_pattern_list |= (arg_pattern << (kPrintfArgPatternBits * i));
    }
    dc32(arg_pattern_list);   // kPrintfArgPatternListOffset
  }
#else
  Call(FUNCTION_ADDR(printf), RelocInfo::EXTERNAL_REFERENCE);
#endif
}


void MacroAssembler::Printf(const char * format,
                            CPURegister arg0,
                            CPURegister arg1,
                            CPURegister arg2,
                            CPURegister arg3) {
  // We can only print sp if it is the current stack pointer.
  if (!csp.Is(StackPointer())) {
    DCHECK(!csp.Aliases(arg0));
    DCHECK(!csp.Aliases(arg1));
    DCHECK(!csp.Aliases(arg2));
    DCHECK(!csp.Aliases(arg3));
  }

  // Printf is expected to preserve all registers, so make sure that none are
  // available as scratch registers until we've preserved them.
  RegList old_tmp_list = TmpList()->list();
  RegList old_fp_tmp_list = FPTmpList()->list();
  TmpList()->set_list(0);
  FPTmpList()->set_list(0);

  // Preserve all caller-saved registers as well as NZCV.
  // If csp is the stack pointer, PushCPURegList asserts that the size of each
  // list is a multiple of 16 bytes.
  PushCPURegList(kCallerSaved);
  PushCPURegList(kCallerSavedV);

  // We can use caller-saved registers as scratch values (except for argN).
  CPURegList tmp_list = kCallerSaved;
  CPURegList fp_tmp_list = kCallerSavedV;
  tmp_list.Remove(arg0, arg1, arg2, arg3);
  fp_tmp_list.Remove(arg0, arg1, arg2, arg3);
  TmpList()->set_list(tmp_list.list());
  FPTmpList()->set_list(fp_tmp_list.list());

  { UseScratchRegisterScope temps(this);
    // If any of the arguments are the current stack pointer, allocate a new
    // register for them, and adjust the value to compensate for pushing the
    // caller-saved registers.
    bool arg0_sp = StackPointer().Aliases(arg0);
    bool arg1_sp = StackPointer().Aliases(arg1);
    bool arg2_sp = StackPointer().Aliases(arg2);
    bool arg3_sp = StackPointer().Aliases(arg3);
    if (arg0_sp || arg1_sp || arg2_sp || arg3_sp) {
      // Allocate a register to hold the original stack pointer value, to pass
      // to PrintfNoPreserve as an argument.
      Register arg_sp = temps.AcquireX();
      Add(arg_sp, StackPointer(),
          kCallerSaved.TotalSizeInBytes() + kCallerSavedV.TotalSizeInBytes());
      if (arg0_sp) arg0 = Register::Create(arg_sp.code(), arg0.SizeInBits());
      if (arg1_sp) arg1 = Register::Create(arg_sp.code(), arg1.SizeInBits());
      if (arg2_sp) arg2 = Register::Create(arg_sp.code(), arg2.SizeInBits());
      if (arg3_sp) arg3 = Register::Create(arg_sp.code(), arg3.SizeInBits());
    }

    // Preserve NZCV.
    { UseScratchRegisterScope temps(this);
      Register tmp = temps.AcquireX();
      Mrs(tmp, NZCV);
      Push(tmp, xzr);
    }

    PrintfNoPreserve(format, arg0, arg1, arg2, arg3);

    // Restore NZCV.
    { UseScratchRegisterScope temps(this);
      Register tmp = temps.AcquireX();
      Pop(xzr, tmp);
      Msr(NZCV, tmp);
    }
  }

  PopCPURegList(kCallerSavedV);
  PopCPURegList(kCallerSaved);

  TmpList()->set_list(old_tmp_list);
  FPTmpList()->set_list(old_fp_tmp_list);
}

UseScratchRegisterScope::~UseScratchRegisterScope() {
  available_->set_list(old_available_);
  availablefp_->set_list(old_availablefp_);
}


Register UseScratchRegisterScope::AcquireSameSizeAs(const Register& reg) {
  int code = AcquireNextAvailable(available_).code();
  return Register::Create(code, reg.SizeInBits());
}

VRegister UseScratchRegisterScope::AcquireSameSizeAs(const VRegister& reg) {
  int code = AcquireNextAvailable(availablefp_).code();
  return VRegister::Create(code, reg.SizeInBits());
}


CPURegister UseScratchRegisterScope::AcquireNextAvailable(
    CPURegList* available) {
  CHECK(!available->IsEmpty());
  CPURegister result = available->PopLowestIndex();
  DCHECK(!AreAliased(result, xzr, csp));
  return result;
}


MemOperand ContextMemOperand(Register context, int index) {
  return MemOperand(context, Context::SlotOffset(index));
}

MemOperand NativeContextMemOperand() {
  return ContextMemOperand(cp, Context::NATIVE_CONTEXT_INDEX);
}

#define __ masm->

void InlineSmiCheckInfo::Emit(MacroAssembler* masm, const Register& reg,
                              const Label* smi_check) {
  Assembler::BlockPoolsScope scope(masm);
  if (reg.IsValid()) {
    DCHECK(smi_check->is_bound());
    DCHECK(reg.Is64Bits());

    // Encode the register (x0-x30) in the lowest 5 bits, then the offset to
    // 'check' in the other bits. The possible offset is limited in that we
    // use BitField to pack the data, and the underlying data type is a
    // uint32_t.
    uint32_t delta =
        static_cast<uint32_t>(__ InstructionsGeneratedSince(smi_check));
    __ InlineData(RegisterBits::encode(reg.code()) | DeltaBits::encode(delta));
  } else {
    DCHECK(!smi_check->is_bound());

    // An offset of 0 indicates that there is no patch site.
    __ InlineData(0);
  }
}

InlineSmiCheckInfo::InlineSmiCheckInfo(Address info)
    : reg_(NoReg), smi_check_delta_(0), smi_check_(NULL) {
  InstructionSequence* inline_data = InstructionSequence::At(info);
  DCHECK(inline_data->IsInlineData());
  if (inline_data->IsInlineData()) {
    uint64_t payload = inline_data->InlineData();
    // We use BitField to decode the payload, and BitField can only handle
    // 32-bit values.
    DCHECK(is_uint32(payload));
    if (payload != 0) {
      uint32_t payload32 = static_cast<uint32_t>(payload);
      int reg_code = RegisterBits::decode(payload32);
      reg_ = Register::XRegFromCode(reg_code);
      smi_check_delta_ = DeltaBits::decode(payload32);
      DCHECK_NE(0, smi_check_delta_);
      smi_check_ = inline_data->preceding(smi_check_delta_);
    }
  }
}


#undef __


}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM64
