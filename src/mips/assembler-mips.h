// Copyright (c) 1994-2006 Sun Microsystems Inc.
// All Rights Reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// - Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// - Redistribution in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// - Neither the name of Sun Microsystems or the names of contributors may
// be used to endorse or promote products derived from this software without
// specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
// IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// The original source code covered by the above license above has been
// modified significantly by Google Inc.
// Copyright 2012 the V8 project authors. All rights reserved.


#ifndef V8_MIPS_ASSEMBLER_MIPS_H_
#define V8_MIPS_ASSEMBLER_MIPS_H_

#include <stdio.h>

#include <set>

#include "src/assembler.h"
#include "src/external-reference.h"
#include "src/label.h"
#include "src/mips/constants-mips.h"
#include "src/objects/smi.h"

namespace v8 {
namespace internal {

// clang-format off
#define GENERAL_REGISTERS(V)                              \
  V(zero_reg)  V(at)  V(v0)  V(v1)  V(a0)  V(a1)  V(a2)  V(a3)  \
  V(t0)  V(t1)  V(t2)  V(t3)  V(t4)  V(t5)  V(t6)  V(t7)  \
  V(s0)  V(s1)  V(s2)  V(s3)  V(s4)  V(s5)  V(s6)  V(s7)  V(t8)  V(t9) \
  V(k0)  V(k1)  V(gp)  V(sp)  V(fp)  V(ra)

#define ALLOCATABLE_GENERAL_REGISTERS(V) \
  V(a0)  V(a1)  V(a2)  V(a3) \
  V(t0)  V(t1)  V(t2)  V(t3)  V(t4)  V(t5)  V(t6) V(s7) \
  V(v0)  V(v1)

#define DOUBLE_REGISTERS(V)                               \
  V(f0)  V(f1)  V(f2)  V(f3)  V(f4)  V(f5)  V(f6)  V(f7)  \
  V(f8)  V(f9)  V(f10) V(f11) V(f12) V(f13) V(f14) V(f15) \
  V(f16) V(f17) V(f18) V(f19) V(f20) V(f21) V(f22) V(f23) \
  V(f24) V(f25) V(f26) V(f27) V(f28) V(f29) V(f30) V(f31)

#define FLOAT_REGISTERS DOUBLE_REGISTERS
#define SIMD128_REGISTERS(V)                              \
  V(w0)  V(w1)  V(w2)  V(w3)  V(w4)  V(w5)  V(w6)  V(w7)  \
  V(w8)  V(w9)  V(w10) V(w11) V(w12) V(w13) V(w14) V(w15) \
  V(w16) V(w17) V(w18) V(w19) V(w20) V(w21) V(w22) V(w23) \
  V(w24) V(w25) V(w26) V(w27) V(w28) V(w29) V(w30) V(w31)

#define ALLOCATABLE_DOUBLE_REGISTERS(V)                   \
  V(f0)  V(f2)  V(f4)  V(f6)  V(f8)  V(f10) V(f12) V(f14) \
  V(f16) V(f18) V(f20) V(f22) V(f24)
// clang-format on

// Register lists.
// Note that the bit values must match those used in actual instruction
// encoding.
const int kNumRegs = 32;

const RegList kJSCallerSaved = 1 << 2 |   // v0
                               1 << 3 |   // v1
                               1 << 4 |   // a0
                               1 << 5 |   // a1
                               1 << 6 |   // a2
                               1 << 7 |   // a3
                               1 << 8 |   // t0
                               1 << 9 |   // t1
                               1 << 10 |  // t2
                               1 << 11 |  // t3
                               1 << 12 |  // t4
                               1 << 13 |  // t5
                               1 << 14 |  // t6
                               1 << 15;   // t7

const int kNumJSCallerSaved = 14;

// Callee-saved registers preserved when switching from C to JavaScript.
const RegList kCalleeSaved = 1 << 16 |  // s0
                             1 << 17 |  // s1
                             1 << 18 |  // s2
                             1 << 19 |  // s3
                             1 << 20 |  // s4
                             1 << 21 |  // s5
                             1 << 22 |  // s6 (roots in Javascript code)
                             1 << 23 |  // s7 (cp in Javascript code)
                             1 << 30;   // fp/s8

const int kNumCalleeSaved = 9;

const RegList kCalleeSavedFPU = 1 << 20 |  // f20
                                1 << 22 |  // f22
                                1 << 24 |  // f24
                                1 << 26 |  // f26
                                1 << 28 |  // f28
                                1 << 30;   // f30

const int kNumCalleeSavedFPU = 6;

const RegList kCallerSavedFPU = 1 << 0 |   // f0
                                1 << 2 |   // f2
                                1 << 4 |   // f4
                                1 << 6 |   // f6
                                1 << 8 |   // f8
                                1 << 10 |  // f10
                                1 << 12 |  // f12
                                1 << 14 |  // f14
                                1 << 16 |  // f16
                                1 << 18;   // f18

// Number of registers for which space is reserved in safepoints. Must be a
// multiple of 8.
const int kNumSafepointRegisters = 24;

// Define the list of registers actually saved at safepoints.
// Note that the number of saved registers may be smaller than the reserved
// space, i.e. kNumSafepointSavedRegisters <= kNumSafepointRegisters.
const RegList kSafepointSavedRegisters = kJSCallerSaved | kCalleeSaved;
const int kNumSafepointSavedRegisters = kNumJSCallerSaved + kNumCalleeSaved;

const int kUndefIndex = -1;
// Map with indexes on stack that corresponds to codes of saved registers.
const int kSafepointRegisterStackIndexMap[kNumRegs] = {kUndefIndex,  // zero_reg
                                                       kUndefIndex,  // at
                                                       0,            // v0
                                                       1,            // v1
                                                       2,            // a0
                                                       3,            // a1
                                                       4,            // a2
                                                       5,            // a3
                                                       6,            // t0
                                                       7,            // t1
                                                       8,            // t2
                                                       9,            // t3
                                                       10,           // t4
                                                       11,           // t5
                                                       12,           // t6
                                                       13,           // t7
                                                       14,           // s0
                                                       15,           // s1
                                                       16,           // s2
                                                       17,           // s3
                                                       18,           // s4
                                                       19,           // s5
                                                       20,           // s6
                                                       21,           // s7
                                                       kUndefIndex,  // t8
                                                       kUndefIndex,  // t9
                                                       kUndefIndex,  // k0
                                                       kUndefIndex,  // k1
                                                       kUndefIndex,  // gp
                                                       kUndefIndex,  // sp
                                                       22,           // fp
                                                       kUndefIndex};

// CPU Registers.
//
// 1) We would prefer to use an enum, but enum values are assignment-
// compatible with int, which has caused code-generation bugs.
//
// 2) We would prefer to use a class instead of a struct but we don't like
// the register initialization to depend on the particular initialization
// order (which appears to be different on OS X, Linux, and Windows for the
// installed versions of C++ we tried). Using a struct permits C-style
// "initialization". Also, the Register objects cannot be const as this
// forces initialization stubs in MSVC, making us dependent on initialization
// order.
//
// 3) By not using an enum, we are possibly preventing the compiler from
// doing certain constant folds, which may significantly reduce the
// code generated for some assembly instructions (because they boil down
// to a few constants). If this is a problem, we could change the code
// such that we use an enum in optimized mode, and the struct in debug
// mode. This way we get the compile-time error checking in debug mode
// and best performance in optimized code.


// -----------------------------------------------------------------------------
// Implementation of Register and FPURegister.

enum RegisterCode {
#define REGISTER_CODE(R) kRegCode_##R,
  GENERAL_REGISTERS(REGISTER_CODE)
#undef REGISTER_CODE
      kRegAfterLast
};

class Register : public RegisterBase<Register, kRegAfterLast> {
 public:
#if defined(V8_TARGET_LITTLE_ENDIAN)
  static constexpr int kMantissaOffset = 0;
  static constexpr int kExponentOffset = 4;
#elif defined(V8_TARGET_BIG_ENDIAN)
  static constexpr int kMantissaOffset = 4;
  static constexpr int kExponentOffset = 0;
#else
#error Unknown endianness
#endif

 private:
  friend class RegisterBase;
  explicit constexpr Register(int code) : RegisterBase(code) {}
};

// s7: context register
// s3: scratch register
// s4: scratch register 2
#define DECLARE_REGISTER(R) \
  constexpr Register R = Register::from_code<kRegCode_##R>();
GENERAL_REGISTERS(DECLARE_REGISTER)
#undef DECLARE_REGISTER
constexpr Register no_reg = Register::no_reg();

int ToNumber(Register reg);

Register ToRegister(int num);

constexpr bool kPadArguments = false;
constexpr bool kSimpleFPAliasing = true;
constexpr bool kSimdMaskRegisters = false;

enum DoubleRegisterCode {
#define REGISTER_CODE(R) kDoubleCode_##R,
  DOUBLE_REGISTERS(REGISTER_CODE)
#undef REGISTER_CODE
      kDoubleAfterLast
};

// Coprocessor register.
class FPURegister : public RegisterBase<FPURegister, kDoubleAfterLast> {
 public:
  FPURegister low() const {
    // Find low reg of a Double-reg pair, which is the reg itself.
    DCHECK_EQ(code() % 2, 0);  // Specified Double reg must be even.
    return FPURegister::from_code(code());
  }
  FPURegister high() const {
    // Find high reg of a Doubel-reg pair, which is reg + 1.
    DCHECK_EQ(code() % 2, 0);  // Specified Double reg must be even.
    return FPURegister::from_code(code() + 1);
  }

 private:
  friend class RegisterBase;
  explicit constexpr FPURegister(int code) : RegisterBase(code) {}
};

enum MSARegisterCode {
#define REGISTER_CODE(R) kMsaCode_##R,
  SIMD128_REGISTERS(REGISTER_CODE)
#undef REGISTER_CODE
      kMsaAfterLast
};

// MIPS SIMD (MSA) register
class MSARegister : public RegisterBase<MSARegister, kMsaAfterLast> {
  friend class RegisterBase;
  explicit constexpr MSARegister(int code) : RegisterBase(code) {}
};

// A few double registers are reserved: one as a scratch register and one to
// hold 0.0.
//  f28: 0.0
//  f30: scratch register.

// V8 now supports the O32 ABI, and the FPU Registers are organized as 32
// 32-bit registers, f0 through f31. When used as 'double' they are used
// in pairs, starting with the even numbered register. So a double operation
// on f0 really uses f0 and f1.
// (Modern mips hardware also supports 32 64-bit registers, via setting
// (priviledged) Status Register FR bit to 1. This is used by the N32 ABI,
// but it is not in common use. Someday we will want to support this in v8.)

// For O32 ABI, Floats and Doubles refer to same set of 32 32-bit registers.
typedef FPURegister FloatRegister;

typedef FPURegister DoubleRegister;

#define DECLARE_DOUBLE_REGISTER(R) \
  constexpr DoubleRegister R = DoubleRegister::from_code<kDoubleCode_##R>();
DOUBLE_REGISTERS(DECLARE_DOUBLE_REGISTER)
#undef DECLARE_DOUBLE_REGISTER

constexpr DoubleRegister no_dreg = DoubleRegister::no_reg();

// SIMD registers.
typedef MSARegister Simd128Register;

#define DECLARE_SIMD128_REGISTER(R) \
  constexpr Simd128Register R = Simd128Register::from_code<kMsaCode_##R>();
SIMD128_REGISTERS(DECLARE_SIMD128_REGISTER)
#undef DECLARE_SIMD128_REGISTER

const Simd128Register no_msareg = Simd128Register::no_reg();

// Register aliases.
// cp is assumed to be a callee saved register.
constexpr Register kRootRegister = s6;
constexpr Register cp = s7;
constexpr Register kScratchReg = s3;
constexpr Register kScratchReg2 = s4;
constexpr DoubleRegister kScratchDoubleReg = f30;
constexpr DoubleRegister kDoubleRegZero = f28;
// Used on mips32r6 for compare operations.
constexpr DoubleRegister kDoubleCompareReg = f26;
// MSA zero and scratch regs must have the same numbers as FPU zero and scratch
constexpr Simd128Register kSimd128RegZero = w28;
constexpr Simd128Register kSimd128ScratchReg = w30;

// FPU (coprocessor 1) control registers.
// Currently only FCSR (#31) is implemented.
struct FPUControlRegister {
  bool is_valid() const { return reg_code == kFCSRRegister; }
  bool is(FPUControlRegister creg) const { return reg_code == creg.reg_code; }
  int code() const {
    DCHECK(is_valid());
    return reg_code;
  }
  int bit() const {
    DCHECK(is_valid());
    return 1 << reg_code;
  }
  void setcode(int f) {
    reg_code = f;
    DCHECK(is_valid());
  }
  // Unfortunately we can't make this private in a struct.
  int reg_code;
};

constexpr FPUControlRegister no_fpucreg = {kInvalidFPUControlRegister};
constexpr FPUControlRegister FCSR = {kFCSRRegister};

// MSA control registers
struct MSAControlRegister {
  bool is_valid() const {
    return (reg_code == kMSAIRRegister) || (reg_code == kMSACSRRegister);
  }
  bool is(MSAControlRegister creg) const { return reg_code == creg.reg_code; }
  int code() const {
    DCHECK(is_valid());
    return reg_code;
  }
  int bit() const {
    DCHECK(is_valid());
    return 1 << reg_code;
  }
  void setcode(int f) {
    reg_code = f;
    DCHECK(is_valid());
  }
  // Unfortunately we can't make this private in a struct.
  int reg_code;
};

constexpr MSAControlRegister no_msacreg = {kInvalidMSAControlRegister};
constexpr MSAControlRegister MSAIR = {kMSAIRRegister};
constexpr MSAControlRegister MSACSR = {kMSACSRRegister};

// Allow programmer to use Branch Delay Slot of Branches, Jumps, Calls.
enum BranchDelaySlot { USE_DELAY_SLOT, PROTECT };

// -----------------------------------------------------------------------------
// Machine instruction Operands.

// Class Operand represents a shifter operand in data processing instructions.
class Operand {
 public:
  // Immediate.
  V8_INLINE explicit Operand(int32_t immediate,
                             RelocInfo::Mode rmode = RelocInfo::NONE)
      : rm_(no_reg), rmode_(rmode) {
    value_.immediate = immediate;
  }
  V8_INLINE explicit Operand(const ExternalReference& f)
      : rm_(no_reg), rmode_(RelocInfo::EXTERNAL_REFERENCE) {
    value_.immediate = static_cast<int32_t>(f.address());
  }
  V8_INLINE explicit Operand(const char* s);
  explicit Operand(Handle<HeapObject> handle);
  V8_INLINE explicit Operand(Smi value) : rm_(no_reg), rmode_(RelocInfo::NONE) {
    value_.immediate = static_cast<intptr_t>(value.ptr());
  }

  static Operand EmbeddedNumber(double number);  // Smi or HeapNumber.
  static Operand EmbeddedCode(CodeStub* stub);
  static Operand EmbeddedStringConstant(const StringConstantBase* str);

  // Register.
  V8_INLINE explicit Operand(Register rm) : rm_(rm) {}

  // Return true if this is a register operand.
  V8_INLINE bool is_reg() const;

  inline int32_t immediate() const;

  bool IsImmediate() const { return !rm_.is_valid(); }

  HeapObjectRequest heap_object_request() const {
    DCHECK(IsHeapObjectRequest());
    return value_.heap_object_request;
  }

  bool IsHeapObjectRequest() const {
    DCHECK_IMPLIES(is_heap_object_request_, IsImmediate());
    DCHECK_IMPLIES(is_heap_object_request_,
                   rmode_ == RelocInfo::EMBEDDED_OBJECT ||
                       rmode_ == RelocInfo::CODE_TARGET);
    return is_heap_object_request_;
  }

  Register rm() const { return rm_; }

  RelocInfo::Mode rmode() const { return rmode_; }

 private:
  Register rm_;
  union Value {
    Value() {}
    HeapObjectRequest heap_object_request;  // if is_heap_object_request_
    int32_t immediate;                      // otherwise
  } value_;                                 // valid if rm_ == no_reg
  bool is_heap_object_request_ = false;
  RelocInfo::Mode rmode_;

  friend class Assembler;
  // friend class MacroAssembler;
};


// On MIPS we have only one addressing mode with base_reg + offset.
// Class MemOperand represents a memory operand in load and store instructions.
class MemOperand : public Operand {
 public:
  // Immediate value attached to offset.
  enum OffsetAddend {
    offset_minus_one = -1,
    offset_zero = 0
  };

  explicit MemOperand(Register rn, int32_t offset = 0);
  explicit MemOperand(Register rn, int32_t unit, int32_t multiplier,
                      OffsetAddend offset_addend = offset_zero);
  int32_t offset() const { return offset_; }

  bool OffsetIsInt16Encodable() const {
    return is_int16(offset_);
  }

 private:
  int32_t offset_;

  friend class Assembler;
};

class V8_EXPORT_PRIVATE Assembler : public AssemblerBase {
 public:
  // Create an assembler. Instructions and relocation information are emitted
  // into a buffer, with the instructions starting from the beginning and the
  // relocation information starting from the end of the buffer. See CodeDesc
  // for a detailed comment on the layout (globals.h).
  //
  // If the provided buffer is nullptr, the assembler allocates and grows its
  // own buffer, and buffer_size determines the initial buffer size. The buffer
  // is owned by the assembler and deallocated upon destruction of the
  // assembler.
  //
  // If the provided buffer is not nullptr, the assembler uses the provided
  // buffer for code generation and assumes its size to be buffer_size. If the
  // buffer is too small, a fatal error occurs. No deallocation of the buffer is
  // done upon destruction of the assembler.
  Assembler(const AssemblerOptions& options, void* buffer, int buffer_size);
  virtual ~Assembler() { }

  // GetCode emits any pending (non-emitted) code and fills the descriptor
  // desc. GetCode() is idempotent; it returns the same result if no other
  // Assembler functions are invoked in between GetCode() calls.
  void GetCode(Isolate* isolate, CodeDesc* desc);

  // Label operations & relative jumps (PPUM Appendix D).
  //
  // Takes a branch opcode (cc) and a label (L) and generates
  // either a backward branch or a forward branch and links it
  // to the label fixup chain. Usage:
  //
  // Label L;    // unbound label
  // j(cc, &L);  // forward branch to unbound label
  // bind(&L);   // bind label to the current pc
  // j(cc, &L);  // backward branch to bound label
  // bind(&L);   // illegal: a label may be bound only once
  //
  // Note: The same Label can be used for forward and backward branches
  // but it may be bound only once.
  void bind(Label* L);  // Binds an unbound label L to current code position.

  enum OffsetSize : int { kOffset26 = 26, kOffset21 = 21, kOffset16 = 16 };

  // Determines if Label is bound and near enough so that branch instruction
  // can be used to reach it, instead of jump instruction.
  bool is_near(Label* L);
  bool is_near(Label* L, OffsetSize bits);
  bool is_near_branch(Label* L);
  inline bool is_near_pre_r6(Label* L) {
    DCHECK(!IsMipsArchVariant(kMips32r6));
    return pc_offset() - L->pos() < kMaxBranchOffset - 4 * kInstrSize;
  }
  inline bool is_near_r6(Label* L) {
    DCHECK(IsMipsArchVariant(kMips32r6));
    return pc_offset() - L->pos() < kMaxCompactBranchOffset - 4 * kInstrSize;
  }

  int BranchOffset(Instr instr);

  // Returns the branch offset to the given label from the current code
  // position. Links the label to the current position if it is still unbound.
  // Manages the jump elimination optimization if the second parameter is true.
  int32_t branch_offset_helper(Label* L, OffsetSize bits);
  inline int32_t branch_offset(Label* L) {
    return branch_offset_helper(L, OffsetSize::kOffset16);
  }
  inline int32_t branch_offset21(Label* L) {
    return branch_offset_helper(L, OffsetSize::kOffset21);
  }
  inline int32_t branch_offset26(Label* L) {
    return branch_offset_helper(L, OffsetSize::kOffset26);
  }
  inline int32_t shifted_branch_offset(Label* L) {
    return branch_offset(L) >> 2;
  }
  inline int32_t shifted_branch_offset21(Label* L) {
    return branch_offset21(L) >> 2;
  }
  inline int32_t shifted_branch_offset26(Label* L) {
    return branch_offset26(L) >> 2;
  }
  uint32_t jump_address(Label* L);
  uint32_t branch_long_offset(Label* L);

  // Puts a labels target address at the given position.
  // The high 8 bits are set to zero.
  void label_at_put(Label* L, int at_offset);

  // Read/Modify the code target address in the branch/call instruction at pc.
  // The isolate argument is unused (and may be nullptr) when skipping flushing.
  static Address target_address_at(Address pc);
  V8_INLINE static void set_target_address_at(
      Address pc, Address target,
      ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED) {
    set_target_value_at(pc, static_cast<uint32_t>(target), icache_flush_mode);
  }
  // On MIPS there is no Constant Pool so we skip that parameter.
  V8_INLINE static Address target_address_at(Address pc,
                                             Address constant_pool) {
    return target_address_at(pc);
  }
  V8_INLINE static void set_target_address_at(
      Address pc, Address constant_pool, Address target,
      ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED) {
    set_target_address_at(pc, target, icache_flush_mode);
  }

  static void set_target_value_at(
      Address pc, uint32_t target,
      ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED);

  // Return the code target address at a call site from the return address
  // of that call in the instruction stream.
  inline static Address target_address_from_return_address(Address pc);

  static void QuietNaN(HeapObject* nan);

  // This sets the branch destination (which gets loaded at the call address).
  // This is for calls and branches within generated code.  The serializer
  // has already deserialized the lui/ori instructions etc.
  inline static void deserialization_set_special_target_at(
      Address instruction_payload, Code code, Address target);

  // Get the size of the special target encoded at 'instruction_payload'.
  inline static int deserialization_special_target_size(
      Address instruction_payload);

  // This sets the internal reference at the pc.
  inline static void deserialization_set_target_internal_reference_at(
      Address pc, Address target,
      RelocInfo::Mode mode = RelocInfo::INTERNAL_REFERENCE);

  // Difference between address of current opcode and target address offset.
  static constexpr int kBranchPCOffset = kInstrSize;

  // Difference between address of current opcode and target address offset,
  // when we are generatinga sequence of instructions for long relative PC
  // branches. It is distance between address of the first instruction in
  // the jump sequence, and the value that ra gets after calling nal().
  static constexpr int kLongBranchPCOffset = 3 * kInstrSize;

  // Adjust ra register in branch delay slot of bal instruction in order to skip
  // instructions not needed after optimization of PIC in
  // TurboAssembler::BranchAndLink method.
  static constexpr int kOptimizedBranchAndLinkLongReturnOffset = 3 * kInstrSize;

  // Offset of target relative address in calls/jumps for builtins. It is
  // distance between instruction that is placed just after calling
  // RecordRelocInfo, and the value that ra gets aftr calling nal().
  static constexpr int kRelativeJumpForBuiltinsOffset = 1 * kInstrSize;
  // Relative target address of jumps for builtins when we use lui, ori, dsll,
  // ori sequence when loading address that cannot fit into 32 bits.
  static constexpr int kRelativeCallForBuiltinsOffset = 3 * kInstrSize;

  // Here we are patching the address in the LUI/ORI instruction pair.
  // These values are used in the serialization process and must be zero for
  // MIPS platform, as Code, Embedded Object or External-reference pointers
  // are split across two consecutive instructions and don't exist separately
  // in the code, so the serializer should not step forwards in memory after
  // a target is resolved and written.

  static constexpr int kSpecialTargetSize = 0;

  // Number of consecutive instructions used to store 32bit constant. This
  // constant is used in RelocInfo::target_address_address() function to tell
  // serializer address of the instruction that follows LUI/ORI instruction
  // pair.
  static constexpr int kInstructionsFor32BitConstant = 2;

  // Distance between the instruction referring to the address of the call
  // target and the return address.
#ifdef _MIPS_ARCH_MIPS32R6
  static constexpr int kCallTargetAddressOffset = 2 * kInstrSize;
#else
  static constexpr int kCallTargetAddressOffset = 4 * kInstrSize;
#endif

  // Max offset for instructions with 16-bit offset field
  static constexpr int kMaxBranchOffset = (1 << (18 - 1)) - 1;

  // Max offset for compact branch instructions with 26-bit offset field
  static constexpr int kMaxCompactBranchOffset = (1 << (28 - 1)) - 1;

  static constexpr int kTrampolineSlotsSize =
      IsMipsArchVariant(kMips32r6) ? 2 * kInstrSize : 7 * kInstrSize;

  RegList* GetScratchRegisterList() { return &scratch_register_list_; }

  // ---------------------------------------------------------------------------
  // Code generation.

  // Insert the smallest number of nop instructions
  // possible to align the pc offset to a multiple
  // of m. m must be a power of 2 (>= 4).
  void Align(int m);
  // Insert the smallest number of zero bytes possible to align the pc offset
  // to a mulitple of m. m must be a power of 2 (>= 2).
  void DataAlign(int m);
  // Aligns code to something that's optimal for a jump target for the platform.
  void CodeTargetAlign();

  // Different nop operations are used by the code generator to detect certain
  // states of the generated code.
  enum NopMarkerTypes {
    NON_MARKING_NOP = 0,
    DEBUG_BREAK_NOP,
    // IC markers.
    PROPERTY_ACCESS_INLINED,
    PROPERTY_ACCESS_INLINED_CONTEXT,
    PROPERTY_ACCESS_INLINED_CONTEXT_DONT_DELETE,
    // Helper values.
    LAST_CODE_MARKER,
    FIRST_IC_MARKER = PROPERTY_ACCESS_INLINED,
  };

  // Type == 0 is the default non-marking nop. For mips this is a
  // sll(zero_reg, zero_reg, 0). We use rt_reg == at for non-zero
  // marking, to avoid conflict with ssnop and ehb instructions.
  void nop(unsigned int type = 0) {
    DCHECK_LT(type, 32);
    Register nop_rt_reg = (type == 0) ? zero_reg : at;
    sll(zero_reg, nop_rt_reg, type, true);
  }


  // --------Branch-and-jump-instructions----------
  // We don't use likely variant of instructions.
  void b(int16_t offset);
  inline void b(Label* L) { b(shifted_branch_offset(L)); }
  void bal(int16_t offset);
  inline void bal(Label* L) { bal(shifted_branch_offset(L)); }
  void bc(int32_t offset);
  inline void bc(Label* L) { bc(shifted_branch_offset26(L)); }
  void balc(int32_t offset);
  inline void balc(Label* L) { balc(shifted_branch_offset26(L)); }

  void beq(Register rs, Register rt, int16_t offset);
  inline void beq(Register rs, Register rt, Label* L) {
    beq(rs, rt, shifted_branch_offset(L));
  }
  void bgez(Register rs, int16_t offset);
  void bgezc(Register rt, int16_t offset);
  inline void bgezc(Register rt, Label* L) {
    bgezc(rt, shifted_branch_offset(L));
  }
  void bgeuc(Register rs, Register rt, int16_t offset);
  inline void bgeuc(Register rs, Register rt, Label* L) {
    bgeuc(rs, rt, shifted_branch_offset(L));
  }
  void bgec(Register rs, Register rt, int16_t offset);
  inline void bgec(Register rs, Register rt, Label* L) {
    bgec(rs, rt, shifted_branch_offset(L));
  }
  void bgezal(Register rs, int16_t offset);
  void bgezalc(Register rt, int16_t offset);
  inline void bgezalc(Register rt, Label* L) {
    bgezalc(rt, shifted_branch_offset(L));
  }
  void bgezall(Register rs, int16_t offset);
  inline void bgezall(Register rs, Label* L) {
    bgezall(rs, branch_offset(L) >> 2);
  }
  void bgtz(Register rs, int16_t offset);
  void bgtzc(Register rt, int16_t offset);
  inline void bgtzc(Register rt, Label* L) {
    bgtzc(rt, shifted_branch_offset(L));
  }
  void blez(Register rs, int16_t offset);
  void blezc(Register rt, int16_t offset);
  inline void blezc(Register rt, Label* L) {
    blezc(rt, shifted_branch_offset(L));
  }
  void bltz(Register rs, int16_t offset);
  void bltzc(Register rt, int16_t offset);
  inline void bltzc(Register rt, Label* L) {
    bltzc(rt, shifted_branch_offset(L));
  }
  void bltuc(Register rs, Register rt, int16_t offset);
  inline void bltuc(Register rs, Register rt, Label* L) {
    bltuc(rs, rt, shifted_branch_offset(L));
  }
  void bltc(Register rs, Register rt, int16_t offset);
  inline void bltc(Register rs, Register rt, Label* L) {
    bltc(rs, rt, shifted_branch_offset(L));
  }
  void bltzal(Register rs, int16_t offset);
  void nal() { bltzal(zero_reg, 0); }
  void blezalc(Register rt, int16_t offset);
  inline void blezalc(Register rt, Label* L) {
    blezalc(rt, shifted_branch_offset(L));
  }
  void bltzalc(Register rt, int16_t offset);
  inline void bltzalc(Register rt, Label* L) {
    bltzalc(rt, shifted_branch_offset(L));
  }
  void bgtzalc(Register rt, int16_t offset);
  inline void bgtzalc(Register rt, Label* L) {
    bgtzalc(rt, shifted_branch_offset(L));
  }
  void beqzalc(Register rt, int16_t offset);
  inline void beqzalc(Register rt, Label* L) {
    beqzalc(rt, shifted_branch_offset(L));
  }
  void beqc(Register rs, Register rt, int16_t offset);
  inline void beqc(Register rs, Register rt, Label* L) {
    beqc(rs, rt, shifted_branch_offset(L));
  }
  void beqzc(Register rs, int32_t offset);
  inline void beqzc(Register rs, Label* L) {
    beqzc(rs, shifted_branch_offset21(L));
  }
  void bnezalc(Register rt, int16_t offset);
  inline void bnezalc(Register rt, Label* L) {
    bnezalc(rt, shifted_branch_offset(L));
  }
  void bnec(Register rs, Register rt, int16_t offset);
  inline void bnec(Register rs, Register rt, Label* L) {
    bnec(rs, rt, shifted_branch_offset(L));
  }
  void bnezc(Register rt, int32_t offset);
  inline void bnezc(Register rt, Label* L) {
    bnezc(rt, shifted_branch_offset21(L));
  }
  void bne(Register rs, Register rt, int16_t offset);
  inline void bne(Register rs, Register rt, Label* L) {
    bne(rs, rt, shifted_branch_offset(L));
  }
  void bovc(Register rs, Register rt, int16_t offset);
  inline void bovc(Register rs, Register rt, Label* L) {
    bovc(rs, rt, shifted_branch_offset(L));
  }
  void bnvc(Register rs, Register rt, int16_t offset);
  inline void bnvc(Register rs, Register rt, Label* L) {
    bnvc(rs, rt, shifted_branch_offset(L));
  }

  // Never use the int16_t b(l)cond version with a branch offset
  // instead of using the Label* version.

  // Jump targets must be in the current 256 MB-aligned region. i.e. 28 bits.
  void j(int32_t target);
  void jal(int32_t target);
  void jalr(Register rs, Register rd = ra);
  void jr(Register target);
  void jic(Register rt, int16_t offset);
  void jialc(Register rt, int16_t offset);


  // -------Data-processing-instructions---------

  // Arithmetic.
  void addu(Register rd, Register rs, Register rt);
  void subu(Register rd, Register rs, Register rt);
  void mult(Register rs, Register rt);
  void multu(Register rs, Register rt);
  void div(Register rs, Register rt);
  void divu(Register rs, Register rt);
  void div(Register rd, Register rs, Register rt);
  void divu(Register rd, Register rs, Register rt);
  void mod(Register rd, Register rs, Register rt);
  void modu(Register rd, Register rs, Register rt);
  void mul(Register rd, Register rs, Register rt);
  void muh(Register rd, Register rs, Register rt);
  void mulu(Register rd, Register rs, Register rt);
  void muhu(Register rd, Register rs, Register rt);

  void addiu(Register rd, Register rs, int32_t j);

  // Logical.
  void and_(Register rd, Register rs, Register rt);
  void or_(Register rd, Register rs, Register rt);
  void xor_(Register rd, Register rs, Register rt);
  void nor(Register rd, Register rs, Register rt);

  void andi(Register rd, Register rs, int32_t j);
  void ori(Register rd, Register rs, int32_t j);
  void xori(Register rd, Register rs, int32_t j);
  void lui(Register rd, int32_t j);
  void aui(Register rs, Register rt, int32_t j);

  // Shifts.
  // Please note: sll(zero_reg, zero_reg, x) instructions are reserved as nop
  // and may cause problems in normal code. coming_from_nop makes sure this
  // doesn't happen.
  void sll(Register rd, Register rt, uint16_t sa, bool coming_from_nop = false);
  void sllv(Register rd, Register rt, Register rs);
  void srl(Register rd, Register rt, uint16_t sa);
  void srlv(Register rd, Register rt, Register rs);
  void sra(Register rt, Register rd, uint16_t sa);
  void srav(Register rt, Register rd, Register rs);
  void rotr(Register rd, Register rt, uint16_t sa);
  void rotrv(Register rd, Register rt, Register rs);

  // ------------Memory-instructions-------------

  void lb(Register rd, const MemOperand& rs);
  void lbu(Register rd, const MemOperand& rs);
  void lh(Register rd, const MemOperand& rs);
  void lhu(Register rd, const MemOperand& rs);
  void lw(Register rd, const MemOperand& rs);
  void lwl(Register rd, const MemOperand& rs);
  void lwr(Register rd, const MemOperand& rs);
  void sb(Register rd, const MemOperand& rs);
  void sh(Register rd, const MemOperand& rs);
  void sw(Register rd, const MemOperand& rs);
  void swl(Register rd, const MemOperand& rs);
  void swr(Register rd, const MemOperand& rs);

  // ----------Atomic instructions--------------

  void ll(Register rd, const MemOperand& rs);
  void sc(Register rd, const MemOperand& rs);
  void llx(Register rd, const MemOperand& rs);
  void scx(Register rd, const MemOperand& rs);

  // ---------PC-Relative-instructions-----------

  void addiupc(Register rs, int32_t imm19);
  void lwpc(Register rs, int32_t offset19);
  void auipc(Register rs, int16_t imm16);
  void aluipc(Register rs, int16_t imm16);


  // ----------------Prefetch--------------------

  void pref(int32_t hint, const MemOperand& rs);


  // -------------Misc-instructions--------------

  // Break / Trap instructions.
  void break_(uint32_t code, bool break_as_stop = false);
  void stop(const char* msg, uint32_t code = kMaxStopCode);
  void tge(Register rs, Register rt, uint16_t code);
  void tgeu(Register rs, Register rt, uint16_t code);
  void tlt(Register rs, Register rt, uint16_t code);
  void tltu(Register rs, Register rt, uint16_t code);
  void teq(Register rs, Register rt, uint16_t code);
  void tne(Register rs, Register rt, uint16_t code);

  // Memory barrier instruction.
  void sync();

  // Move from HI/LO register.
  void mfhi(Register rd);
  void mflo(Register rd);

  // Set on less than.
  void slt(Register rd, Register rs, Register rt);
  void sltu(Register rd, Register rs, Register rt);
  void slti(Register rd, Register rs, int32_t j);
  void sltiu(Register rd, Register rs, int32_t j);

  // Conditional move.
  void movz(Register rd, Register rs, Register rt);
  void movn(Register rd, Register rs, Register rt);
  void movt(Register rd, Register rs, uint16_t cc = 0);
  void movf(Register rd, Register rs, uint16_t cc = 0);

  void sel(SecondaryField fmt, FPURegister fd, FPURegister fs, FPURegister ft);
  void sel_s(FPURegister fd, FPURegister fs, FPURegister ft);
  void sel_d(FPURegister fd, FPURegister fs, FPURegister ft);
  void seleqz(Register rd, Register rs, Register rt);
  void seleqz(SecondaryField fmt, FPURegister fd, FPURegister fs,
              FPURegister ft);
  void selnez(Register rd, Register rs, Register rt);
  void selnez(SecondaryField fmt, FPURegister fd, FPURegister fs,
              FPURegister ft);
  void seleqz_d(FPURegister fd, FPURegister fs, FPURegister ft);
  void seleqz_s(FPURegister fd, FPURegister fs, FPURegister ft);
  void selnez_d(FPURegister fd, FPURegister fs, FPURegister ft);
  void selnez_s(FPURegister fd, FPURegister fs, FPURegister ft);

  void movz_s(FPURegister fd, FPURegister fs, Register rt);
  void movz_d(FPURegister fd, FPURegister fs, Register rt);
  void movt_s(FPURegister fd, FPURegister fs, uint16_t cc = 0);
  void movt_d(FPURegister fd, FPURegister fs, uint16_t cc = 0);
  void movf_s(FPURegister fd, FPURegister fs, uint16_t cc = 0);
  void movf_d(FPURegister fd, FPURegister fs, uint16_t cc = 0);
  void movn_s(FPURegister fd, FPURegister fs, Register rt);
  void movn_d(FPURegister fd, FPURegister fs, Register rt);
  // Bit twiddling.
  void clz(Register rd, Register rs);
  void ins_(Register rt, Register rs, uint16_t pos, uint16_t size);
  void ext_(Register rt, Register rs, uint16_t pos, uint16_t size);
  void bitswap(Register rd, Register rt);
  void align(Register rd, Register rs, Register rt, uint8_t bp);

  void wsbh(Register rd, Register rt);
  void seh(Register rd, Register rt);
  void seb(Register rd, Register rt);

  // --------Coprocessor-instructions----------------

  // Load, store, and move.
  void lwc1(FPURegister fd, const MemOperand& src);
  void swc1(FPURegister fs, const MemOperand& dst);

  void mtc1(Register rt, FPURegister fs);
  void mthc1(Register rt, FPURegister fs);

  void mfc1(Register rt, FPURegister fs);
  void mfhc1(Register rt, FPURegister fs);

  void ctc1(Register rt, FPUControlRegister fs);
  void cfc1(Register rt, FPUControlRegister fs);

  // Arithmetic.
  void add_s(FPURegister fd, FPURegister fs, FPURegister ft);
  void add_d(FPURegister fd, FPURegister fs, FPURegister ft);
  void sub_s(FPURegister fd, FPURegister fs, FPURegister ft);
  void sub_d(FPURegister fd, FPURegister fs, FPURegister ft);
  void mul_s(FPURegister fd, FPURegister fs, FPURegister ft);
  void mul_d(FPURegister fd, FPURegister fs, FPURegister ft);
  void madd_s(FPURegister fd, FPURegister fr, FPURegister fs, FPURegister ft);
  void madd_d(FPURegister fd, FPURegister fr, FPURegister fs, FPURegister ft);
  void msub_s(FPURegister fd, FPURegister fr, FPURegister fs, FPURegister ft);
  void msub_d(FPURegister fd, FPURegister fr, FPURegister fs, FPURegister ft);
  void maddf_s(FPURegister fd, FPURegister fs, FPURegister ft);
  void maddf_d(FPURegister fd, FPURegister fs, FPURegister ft);
  void msubf_s(FPURegister fd, FPURegister fs, FPURegister ft);
  void msubf_d(FPURegister fd, FPURegister fs, FPURegister ft);
  void div_s(FPURegister fd, FPURegister fs, FPURegister ft);
  void div_d(FPURegister fd, FPURegister fs, FPURegister ft);
  void abs_s(FPURegister fd, FPURegister fs);
  void abs_d(FPURegister fd, FPURegister fs);
  void mov_d(FPURegister fd, FPURegister fs);
  void mov_s(FPURegister fd, FPURegister fs);
  void neg_s(FPURegister fd, FPURegister fs);
  void neg_d(FPURegister fd, FPURegister fs);
  void sqrt_s(FPURegister fd, FPURegister fs);
  void sqrt_d(FPURegister fd, FPURegister fs);
  void rsqrt_s(FPURegister fd, FPURegister fs);
  void rsqrt_d(FPURegister fd, FPURegister fs);
  void recip_d(FPURegister fd, FPURegister fs);
  void recip_s(FPURegister fd, FPURegister fs);

  // Conversion.
  void cvt_w_s(FPURegister fd, FPURegister fs);
  void cvt_w_d(FPURegister fd, FPURegister fs);
  void trunc_w_s(FPURegister fd, FPURegister fs);
  void trunc_w_d(FPURegister fd, FPURegister fs);
  void round_w_s(FPURegister fd, FPURegister fs);
  void round_w_d(FPURegister fd, FPURegister fs);
  void floor_w_s(FPURegister fd, FPURegister fs);
  void floor_w_d(FPURegister fd, FPURegister fs);
  void ceil_w_s(FPURegister fd, FPURegister fs);
  void ceil_w_d(FPURegister fd, FPURegister fs);
  void rint_s(FPURegister fd, FPURegister fs);
  void rint_d(FPURegister fd, FPURegister fs);
  void rint(SecondaryField fmt, FPURegister fd, FPURegister fs);

  void cvt_l_s(FPURegister fd, FPURegister fs);
  void cvt_l_d(FPURegister fd, FPURegister fs);
  void trunc_l_s(FPURegister fd, FPURegister fs);
  void trunc_l_d(FPURegister fd, FPURegister fs);
  void round_l_s(FPURegister fd, FPURegister fs);
  void round_l_d(FPURegister fd, FPURegister fs);
  void floor_l_s(FPURegister fd, FPURegister fs);
  void floor_l_d(FPURegister fd, FPURegister fs);
  void ceil_l_s(FPURegister fd, FPURegister fs);
  void ceil_l_d(FPURegister fd, FPURegister fs);

  void class_s(FPURegister fd, FPURegister fs);
  void class_d(FPURegister fd, FPURegister fs);

  void min(SecondaryField fmt, FPURegister fd, FPURegister fs, FPURegister ft);
  void mina(SecondaryField fmt, FPURegister fd, FPURegister fs, FPURegister ft);
  void max(SecondaryField fmt, FPURegister fd, FPURegister fs, FPURegister ft);
  void maxa(SecondaryField fmt, FPURegister fd, FPURegister fs, FPURegister ft);
  void min_s(FPURegister fd, FPURegister fs, FPURegister ft);
  void min_d(FPURegister fd, FPURegister fs, FPURegister ft);
  void max_s(FPURegister fd, FPURegister fs, FPURegister ft);
  void max_d(FPURegister fd, FPURegister fs, FPURegister ft);
  void mina_s(FPURegister fd, FPURegister fs, FPURegister ft);
  void mina_d(FPURegister fd, FPURegister fs, FPURegister ft);
  void maxa_s(FPURegister fd, FPURegister fs, FPURegister ft);
  void maxa_d(FPURegister fd, FPURegister fs, FPURegister ft);

  void cvt_s_w(FPURegister fd, FPURegister fs);
  void cvt_s_l(FPURegister fd, FPURegister fs);
  void cvt_s_d(FPURegister fd, FPURegister fs);

  void cvt_d_w(FPURegister fd, FPURegister fs);
  void cvt_d_l(FPURegister fd, FPURegister fs);
  void cvt_d_s(FPURegister fd, FPURegister fs);

  // Conditions and branches for MIPSr6.
  void cmp(FPUCondition cond, SecondaryField fmt,
         FPURegister fd, FPURegister ft, FPURegister fs);
  void cmp_s(FPUCondition cond, FPURegister fd, FPURegister fs, FPURegister ft);
  void cmp_d(FPUCondition cond, FPURegister fd, FPURegister fs, FPURegister ft);

  void bc1eqz(int16_t offset, FPURegister ft);
  inline void bc1eqz(Label* L, FPURegister ft) {
    bc1eqz(shifted_branch_offset(L), ft);
  }
  void bc1nez(int16_t offset, FPURegister ft);
  inline void bc1nez(Label* L, FPURegister ft) {
    bc1nez(shifted_branch_offset(L), ft);
  }

  // Conditions and branches for non MIPSr6.
  void c(FPUCondition cond, SecondaryField fmt,
         FPURegister ft, FPURegister fs, uint16_t cc = 0);
  void c_s(FPUCondition cond, FPURegister ft, FPURegister fs, uint16_t cc = 0);
  void c_d(FPUCondition cond, FPURegister ft, FPURegister fs, uint16_t cc = 0);

  void bc1f(int16_t offset, uint16_t cc = 0);
  inline void bc1f(Label* L, uint16_t cc = 0) {
    bc1f(shifted_branch_offset(L), cc);
  }
  void bc1t(int16_t offset, uint16_t cc = 0);
  inline void bc1t(Label* L, uint16_t cc = 0) {
    bc1t(shifted_branch_offset(L), cc);
  }
  void fcmp(FPURegister src1, const double src2, FPUCondition cond);

  // MSA instructions
  void bz_v(MSARegister wt, int16_t offset);
  inline void bz_v(MSARegister wt, Label* L) {
    bz_v(wt, shifted_branch_offset(L));
  }
  void bz_b(MSARegister wt, int16_t offset);
  inline void bz_b(MSARegister wt, Label* L) {
    bz_b(wt, shifted_branch_offset(L));
  }
  void bz_h(MSARegister wt, int16_t offset);
  inline void bz_h(MSARegister wt, Label* L) {
    bz_h(wt, shifted_branch_offset(L));
  }
  void bz_w(MSARegister wt, int16_t offset);
  inline void bz_w(MSARegister wt, Label* L) {
    bz_w(wt, shifted_branch_offset(L));
  }
  void bz_d(MSARegister wt, int16_t offset);
  inline void bz_d(MSARegister wt, Label* L) {
    bz_d(wt, shifted_branch_offset(L));
  }
  void bnz_v(MSARegister wt, int16_t offset);
  inline void bnz_v(MSARegister wt, Label* L) {
    bnz_v(wt, shifted_branch_offset(L));
  }
  void bnz_b(MSARegister wt, int16_t offset);
  inline void bnz_b(MSARegister wt, Label* L) {
    bnz_b(wt, shifted_branch_offset(L));
  }
  void bnz_h(MSARegister wt, int16_t offset);
  inline void bnz_h(MSARegister wt, Label* L) {
    bnz_h(wt, shifted_branch_offset(L));
  }
  void bnz_w(MSARegister wt, int16_t offset);
  inline void bnz_w(MSARegister wt, Label* L) {
    bnz_w(wt, shifted_branch_offset(L));
  }
  void bnz_d(MSARegister wt, int16_t offset);
  inline void bnz_d(MSARegister wt, Label* L) {
    bnz_d(wt, shifted_branch_offset(L));
  }

  void ld_b(MSARegister wd, const MemOperand& rs);
  void ld_h(MSARegister wd, const MemOperand& rs);
  void ld_w(MSARegister wd, const MemOperand& rs);
  void ld_d(MSARegister wd, const MemOperand& rs);
  void st_b(MSARegister wd, const MemOperand& rs);
  void st_h(MSARegister wd, const MemOperand& rs);
  void st_w(MSARegister wd, const MemOperand& rs);
  void st_d(MSARegister wd, const MemOperand& rs);

  void ldi_b(MSARegister wd, int32_t imm10);
  void ldi_h(MSARegister wd, int32_t imm10);
  void ldi_w(MSARegister wd, int32_t imm10);
  void ldi_d(MSARegister wd, int32_t imm10);

  void addvi_b(MSARegister wd, MSARegister ws, uint32_t imm5);
  void addvi_h(MSARegister wd, MSARegister ws, uint32_t imm5);
  void addvi_w(MSARegister wd, MSARegister ws, uint32_t imm5);
  void addvi_d(MSARegister wd, MSARegister ws, uint32_t imm5);
  void subvi_b(MSARegister wd, MSARegister ws, uint32_t imm5);
  void subvi_h(MSARegister wd, MSARegister ws, uint32_t imm5);
  void subvi_w(MSARegister wd, MSARegister ws, uint32_t imm5);
  void subvi_d(MSARegister wd, MSARegister ws, uint32_t imm5);
  void maxi_s_b(MSARegister wd, MSARegister ws, uint32_t imm5);
  void maxi_s_h(MSARegister wd, MSARegister ws, uint32_t imm5);
  void maxi_s_w(MSARegister wd, MSARegister ws, uint32_t imm5);
  void maxi_s_d(MSARegister wd, MSARegister ws, uint32_t imm5);
  void maxi_u_b(MSARegister wd, MSARegister ws, uint32_t imm5);
  void maxi_u_h(MSARegister wd, MSARegister ws, uint32_t imm5);
  void maxi_u_w(MSARegister wd, MSARegister ws, uint32_t imm5);
  void maxi_u_d(MSARegister wd, MSARegister ws, uint32_t imm5);
  void mini_s_b(MSARegister wd, MSARegister ws, uint32_t imm5);
  void mini_s_h(MSARegister wd, MSARegister ws, uint32_t imm5);
  void mini_s_w(MSARegister wd, MSARegister ws, uint32_t imm5);
  void mini_s_d(MSARegister wd, MSARegister ws, uint32_t imm5);
  void mini_u_b(MSARegister wd, MSARegister ws, uint32_t imm5);
  void mini_u_h(MSARegister wd, MSARegister ws, uint32_t imm5);
  void mini_u_w(MSARegister wd, MSARegister ws, uint32_t imm5);
  void mini_u_d(MSARegister wd, MSARegister ws, uint32_t imm5);
  void ceqi_b(MSARegister wd, MSARegister ws, uint32_t imm5);
  void ceqi_h(MSARegister wd, MSARegister ws, uint32_t imm5);
  void ceqi_w(MSARegister wd, MSARegister ws, uint32_t imm5);
  void ceqi_d(MSARegister wd, MSARegister ws, uint32_t imm5);
  void clti_s_b(MSARegister wd, MSARegister ws, uint32_t imm5);
  void clti_s_h(MSARegister wd, MSARegister ws, uint32_t imm5);
  void clti_s_w(MSARegister wd, MSARegister ws, uint32_t imm5);
  void clti_s_d(MSARegister wd, MSARegister ws, uint32_t imm5);
  void clti_u_b(MSARegister wd, MSARegister ws, uint32_t imm5);
  void clti_u_h(MSARegister wd, MSARegister ws, uint32_t imm5);
  void clti_u_w(MSARegister wd, MSARegister ws, uint32_t imm5);
  void clti_u_d(MSARegister wd, MSARegister ws, uint32_t imm5);
  void clei_s_b(MSARegister wd, MSARegister ws, uint32_t imm5);
  void clei_s_h(MSARegister wd, MSARegister ws, uint32_t imm5);
  void clei_s_w(MSARegister wd, MSARegister ws, uint32_t imm5);
  void clei_s_d(MSARegister wd, MSARegister ws, uint32_t imm5);
  void clei_u_b(MSARegister wd, MSARegister ws, uint32_t imm5);
  void clei_u_h(MSARegister wd, MSARegister ws, uint32_t imm5);
  void clei_u_w(MSARegister wd, MSARegister ws, uint32_t imm5);
  void clei_u_d(MSARegister wd, MSARegister ws, uint32_t imm5);

  void andi_b(MSARegister wd, MSARegister ws, uint32_t imm8);
  void ori_b(MSARegister wd, MSARegister ws, uint32_t imm8);
  void nori_b(MSARegister wd, MSARegister ws, uint32_t imm8);
  void xori_b(MSARegister wd, MSARegister ws, uint32_t imm8);
  void bmnzi_b(MSARegister wd, MSARegister ws, uint32_t imm8);
  void bmzi_b(MSARegister wd, MSARegister ws, uint32_t imm8);
  void bseli_b(MSARegister wd, MSARegister ws, uint32_t imm8);
  void shf_b(MSARegister wd, MSARegister ws, uint32_t imm8);
  void shf_h(MSARegister wd, MSARegister ws, uint32_t imm8);
  void shf_w(MSARegister wd, MSARegister ws, uint32_t imm8);

  void and_v(MSARegister wd, MSARegister ws, MSARegister wt);
  void or_v(MSARegister wd, MSARegister ws, MSARegister wt);
  void nor_v(MSARegister wd, MSARegister ws, MSARegister wt);
  void xor_v(MSARegister wd, MSARegister ws, MSARegister wt);
  void bmnz_v(MSARegister wd, MSARegister ws, MSARegister wt);
  void bmz_v(MSARegister wd, MSARegister ws, MSARegister wt);
  void bsel_v(MSARegister wd, MSARegister ws, MSARegister wt);

  void fill_b(MSARegister wd, Register rs);
  void fill_h(MSARegister wd, Register rs);
  void fill_w(MSARegister wd, Register rs);
  void pcnt_b(MSARegister wd, MSARegister ws);
  void pcnt_h(MSARegister wd, MSARegister ws);
  void pcnt_w(MSARegister wd, MSARegister ws);
  void pcnt_d(MSARegister wd, MSARegister ws);
  void nloc_b(MSARegister wd, MSARegister ws);
  void nloc_h(MSARegister wd, MSARegister ws);
  void nloc_w(MSARegister wd, MSARegister ws);
  void nloc_d(MSARegister wd, MSARegister ws);
  void nlzc_b(MSARegister wd, MSARegister ws);
  void nlzc_h(MSARegister wd, MSARegister ws);
  void nlzc_w(MSARegister wd, MSARegister ws);
  void nlzc_d(MSARegister wd, MSARegister ws);

  void fclass_w(MSARegister wd, MSARegister ws);
  void fclass_d(MSARegister wd, MSARegister ws);
  void ftrunc_s_w(MSARegister wd, MSARegister ws);
  void ftrunc_s_d(MSARegister wd, MSARegister ws);
  void ftrunc_u_w(MSARegister wd, MSARegister ws);
  void ftrunc_u_d(MSARegister wd, MSARegister ws);
  void fsqrt_w(MSARegister wd, MSARegister ws);
  void fsqrt_d(MSARegister wd, MSARegister ws);
  void frsqrt_w(MSARegister wd, MSARegister ws);
  void frsqrt_d(MSARegister wd, MSARegister ws);
  void frcp_w(MSARegister wd, MSARegister ws);
  void frcp_d(MSARegister wd, MSARegister ws);
  void frint_w(MSARegister wd, MSARegister ws);
  void frint_d(MSARegister wd, MSARegister ws);
  void flog2_w(MSARegister wd, MSARegister ws);
  void flog2_d(MSARegister wd, MSARegister ws);
  void fexupl_w(MSARegister wd, MSARegister ws);
  void fexupl_d(MSARegister wd, MSARegister ws);
  void fexupr_w(MSARegister wd, MSARegister ws);
  void fexupr_d(MSARegister wd, MSARegister ws);
  void ffql_w(MSARegister wd, MSARegister ws);
  void ffql_d(MSARegister wd, MSARegister ws);
  void ffqr_w(MSARegister wd, MSARegister ws);
  void ffqr_d(MSARegister wd, MSARegister ws);
  void ftint_s_w(MSARegister wd, MSARegister ws);
  void ftint_s_d(MSARegister wd, MSARegister ws);
  void ftint_u_w(MSARegister wd, MSARegister ws);
  void ftint_u_d(MSARegister wd, MSARegister ws);
  void ffint_s_w(MSARegister wd, MSARegister ws);
  void ffint_s_d(MSARegister wd, MSARegister ws);
  void ffint_u_w(MSARegister wd, MSARegister ws);
  void ffint_u_d(MSARegister wd, MSARegister ws);

  void sll_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void sll_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void sll_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void sll_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void sra_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void sra_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void sra_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void sra_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void srl_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void srl_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void srl_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void srl_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void bclr_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void bclr_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void bclr_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void bclr_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void bset_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void bset_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void bset_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void bset_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void bneg_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void bneg_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void bneg_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void bneg_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void binsl_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void binsl_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void binsl_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void binsl_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void binsr_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void binsr_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void binsr_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void binsr_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void addv_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void addv_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void addv_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void addv_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void subv_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void subv_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void subv_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void subv_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void max_s_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void max_s_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void max_s_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void max_s_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void max_u_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void max_u_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void max_u_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void max_u_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void min_s_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void min_s_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void min_s_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void min_s_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void min_u_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void min_u_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void min_u_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void min_u_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void max_a_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void max_a_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void max_a_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void max_a_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void min_a_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void min_a_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void min_a_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void min_a_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void ceq_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void ceq_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void ceq_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void ceq_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void clt_s_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void clt_s_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void clt_s_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void clt_s_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void clt_u_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void clt_u_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void clt_u_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void clt_u_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void cle_s_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void cle_s_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void cle_s_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void cle_s_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void cle_u_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void cle_u_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void cle_u_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void cle_u_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void add_a_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void add_a_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void add_a_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void add_a_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void adds_a_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void adds_a_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void adds_a_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void adds_a_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void adds_s_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void adds_s_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void adds_s_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void adds_s_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void adds_u_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void adds_u_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void adds_u_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void adds_u_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void ave_s_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void ave_s_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void ave_s_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void ave_s_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void ave_u_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void ave_u_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void ave_u_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void ave_u_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void aver_s_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void aver_s_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void aver_s_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void aver_s_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void aver_u_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void aver_u_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void aver_u_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void aver_u_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void subs_s_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void subs_s_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void subs_s_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void subs_s_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void subs_u_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void subs_u_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void subs_u_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void subs_u_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void subsus_u_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void subsus_u_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void subsus_u_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void subsus_u_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void subsus_s_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void subsus_s_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void subsus_s_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void subsus_s_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void subsuu_u_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void subsuu_u_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void subsuu_u_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void subsuu_u_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void subsuu_s_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void subsuu_s_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void subsuu_s_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void subsuu_s_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void asub_s_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void asub_s_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void asub_s_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void asub_s_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void asub_u_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void asub_u_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void asub_u_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void asub_u_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void mulv_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void mulv_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void mulv_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void mulv_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void maddv_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void maddv_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void maddv_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void maddv_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void msubv_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void msubv_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void msubv_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void msubv_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void div_s_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void div_s_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void div_s_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void div_s_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void div_u_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void div_u_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void div_u_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void div_u_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void mod_s_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void mod_s_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void mod_s_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void mod_s_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void mod_u_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void mod_u_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void mod_u_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void mod_u_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void dotp_s_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void dotp_s_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void dotp_s_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void dotp_s_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void dotp_u_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void dotp_u_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void dotp_u_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void dotp_u_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void dpadd_s_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void dpadd_s_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void dpadd_s_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void dpadd_s_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void dpadd_u_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void dpadd_u_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void dpadd_u_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void dpadd_u_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void dpsub_s_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void dpsub_s_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void dpsub_s_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void dpsub_s_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void dpsub_u_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void dpsub_u_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void dpsub_u_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void dpsub_u_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void sld_b(MSARegister wd, MSARegister ws, Register rt);
  void sld_h(MSARegister wd, MSARegister ws, Register rt);
  void sld_w(MSARegister wd, MSARegister ws, Register rt);
  void sld_d(MSARegister wd, MSARegister ws, Register rt);
  void splat_b(MSARegister wd, MSARegister ws, Register rt);
  void splat_h(MSARegister wd, MSARegister ws, Register rt);
  void splat_w(MSARegister wd, MSARegister ws, Register rt);
  void splat_d(MSARegister wd, MSARegister ws, Register rt);
  void pckev_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void pckev_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void pckev_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void pckev_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void pckod_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void pckod_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void pckod_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void pckod_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void ilvl_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void ilvl_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void ilvl_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void ilvl_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void ilvr_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void ilvr_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void ilvr_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void ilvr_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void ilvev_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void ilvev_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void ilvev_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void ilvev_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void ilvod_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void ilvod_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void ilvod_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void ilvod_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void vshf_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void vshf_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void vshf_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void vshf_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void srar_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void srar_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void srar_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void srar_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void srlr_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void srlr_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void srlr_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void srlr_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void hadd_s_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void hadd_s_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void hadd_s_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void hadd_s_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void hadd_u_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void hadd_u_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void hadd_u_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void hadd_u_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void hsub_s_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void hsub_s_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void hsub_s_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void hsub_s_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void hsub_u_b(MSARegister wd, MSARegister ws, MSARegister wt);
  void hsub_u_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void hsub_u_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void hsub_u_d(MSARegister wd, MSARegister ws, MSARegister wt);

  void fcaf_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void fcaf_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void fcun_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void fcun_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void fceq_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void fceq_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void fcueq_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void fcueq_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void fclt_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void fclt_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void fcult_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void fcult_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void fcle_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void fcle_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void fcule_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void fcule_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void fsaf_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void fsaf_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void fsun_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void fsun_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void fseq_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void fseq_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void fsueq_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void fsueq_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void fslt_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void fslt_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void fsult_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void fsult_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void fsle_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void fsle_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void fsule_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void fsule_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void fadd_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void fadd_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void fsub_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void fsub_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void fmul_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void fmul_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void fdiv_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void fdiv_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void fmadd_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void fmadd_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void fmsub_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void fmsub_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void fexp2_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void fexp2_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void fexdo_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void fexdo_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void ftq_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void ftq_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void fmin_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void fmin_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void fmin_a_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void fmin_a_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void fmax_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void fmax_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void fmax_a_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void fmax_a_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void fcor_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void fcor_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void fcune_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void fcune_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void fcne_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void fcne_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void mul_q_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void mul_q_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void madd_q_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void madd_q_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void msub_q_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void msub_q_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void fsor_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void fsor_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void fsune_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void fsune_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void fsne_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void fsne_d(MSARegister wd, MSARegister ws, MSARegister wt);
  void mulr_q_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void mulr_q_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void maddr_q_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void maddr_q_w(MSARegister wd, MSARegister ws, MSARegister wt);
  void msubr_q_h(MSARegister wd, MSARegister ws, MSARegister wt);
  void msubr_q_w(MSARegister wd, MSARegister ws, MSARegister wt);

  void sldi_b(MSARegister wd, MSARegister ws, uint32_t n);
  void sldi_h(MSARegister wd, MSARegister ws, uint32_t n);
  void sldi_w(MSARegister wd, MSARegister ws, uint32_t n);
  void sldi_d(MSARegister wd, MSARegister ws, uint32_t n);
  void splati_b(MSARegister wd, MSARegister ws, uint32_t n);
  void splati_h(MSARegister wd, MSARegister ws, uint32_t n);
  void splati_w(MSARegister wd, MSARegister ws, uint32_t n);
  void splati_d(MSARegister wd, MSARegister ws, uint32_t n);
  void copy_s_b(Register rd, MSARegister ws, uint32_t n);
  void copy_s_h(Register rd, MSARegister ws, uint32_t n);
  void copy_s_w(Register rd, MSARegister ws, uint32_t n);
  void copy_u_b(Register rd, MSARegister ws, uint32_t n);
  void copy_u_h(Register rd, MSARegister ws, uint32_t n);
  void copy_u_w(Register rd, MSARegister ws, uint32_t n);
  void insert_b(MSARegister wd, uint32_t n, Register rs);
  void insert_h(MSARegister wd, uint32_t n, Register rs);
  void insert_w(MSARegister wd, uint32_t n, Register rs);
  void insve_b(MSARegister wd, uint32_t n, MSARegister ws);
  void insve_h(MSARegister wd, uint32_t n, MSARegister ws);
  void insve_w(MSARegister wd, uint32_t n, MSARegister ws);
  void insve_d(MSARegister wd, uint32_t n, MSARegister ws);
  void move_v(MSARegister wd, MSARegister ws);
  void ctcmsa(MSAControlRegister cd, Register rs);
  void cfcmsa(Register rd, MSAControlRegister cs);

  void slli_b(MSARegister wd, MSARegister ws, uint32_t m);
  void slli_h(MSARegister wd, MSARegister ws, uint32_t m);
  void slli_w(MSARegister wd, MSARegister ws, uint32_t m);
  void slli_d(MSARegister wd, MSARegister ws, uint32_t m);
  void srai_b(MSARegister wd, MSARegister ws, uint32_t m);
  void srai_h(MSARegister wd, MSARegister ws, uint32_t m);
  void srai_w(MSARegister wd, MSARegister ws, uint32_t m);
  void srai_d(MSARegister wd, MSARegister ws, uint32_t m);
  void srli_b(MSARegister wd, MSARegister ws, uint32_t m);
  void srli_h(MSARegister wd, MSARegister ws, uint32_t m);
  void srli_w(MSARegister wd, MSARegister ws, uint32_t m);
  void srli_d(MSARegister wd, MSARegister ws, uint32_t m);
  void bclri_b(MSARegister wd, MSARegister ws, uint32_t m);
  void bclri_h(MSARegister wd, MSARegister ws, uint32_t m);
  void bclri_w(MSARegister wd, MSARegister ws, uint32_t m);
  void bclri_d(MSARegister wd, MSARegister ws, uint32_t m);
  void bseti_b(MSARegister wd, MSARegister ws, uint32_t m);
  void bseti_h(MSARegister wd, MSARegister ws, uint32_t m);
  void bseti_w(MSARegister wd, MSARegister ws, uint32_t m);
  void bseti_d(MSARegister wd, MSARegister ws, uint32_t m);
  void bnegi_b(MSARegister wd, MSARegister ws, uint32_t m);
  void bnegi_h(MSARegister wd, MSARegister ws, uint32_t m);
  void bnegi_w(MSARegister wd, MSARegister ws, uint32_t m);
  void bnegi_d(MSARegister wd, MSARegister ws, uint32_t m);
  void binsli_b(MSARegister wd, MSARegister ws, uint32_t m);
  void binsli_h(MSARegister wd, MSARegister ws, uint32_t m);
  void binsli_w(MSARegister wd, MSARegister ws, uint32_t m);
  void binsli_d(MSARegister wd, MSARegister ws, uint32_t m);
  void binsri_b(MSARegister wd, MSARegister ws, uint32_t m);
  void binsri_h(MSARegister wd, MSARegister ws, uint32_t m);
  void binsri_w(MSARegister wd, MSARegister ws, uint32_t m);
  void binsri_d(MSARegister wd, MSARegister ws, uint32_t m);
  void sat_s_b(MSARegister wd, MSARegister ws, uint32_t m);
  void sat_s_h(MSARegister wd, MSARegister ws, uint32_t m);
  void sat_s_w(MSARegister wd, MSARegister ws, uint32_t m);
  void sat_s_d(MSARegister wd, MSARegister ws, uint32_t m);
  void sat_u_b(MSARegister wd, MSARegister ws, uint32_t m);
  void sat_u_h(MSARegister wd, MSARegister ws, uint32_t m);
  void sat_u_w(MSARegister wd, MSARegister ws, uint32_t m);
  void sat_u_d(MSARegister wd, MSARegister ws, uint32_t m);
  void srari_b(MSARegister wd, MSARegister ws, uint32_t m);
  void srari_h(MSARegister wd, MSARegister ws, uint32_t m);
  void srari_w(MSARegister wd, MSARegister ws, uint32_t m);
  void srari_d(MSARegister wd, MSARegister ws, uint32_t m);
  void srlri_b(MSARegister wd, MSARegister ws, uint32_t m);
  void srlri_h(MSARegister wd, MSARegister ws, uint32_t m);
  void srlri_w(MSARegister wd, MSARegister ws, uint32_t m);
  void srlri_d(MSARegister wd, MSARegister ws, uint32_t m);

  // Check the code size generated from label to here.
  int SizeOfCodeGeneratedSince(Label* label) {
    return pc_offset() - label->pos();
  }

  // Check the number of instructions generated from label to here.
  int InstructionsGeneratedSince(Label* label) {
    return SizeOfCodeGeneratedSince(label) / kInstrSize;
  }

  // Class for scoping postponing the trampoline pool generation.
  class BlockTrampolinePoolScope {
   public:
    explicit BlockTrampolinePoolScope(Assembler* assem) : assem_(assem) {
      assem_->StartBlockTrampolinePool();
    }
    ~BlockTrampolinePoolScope() {
      assem_->EndBlockTrampolinePool();
    }

   private:
    Assembler* assem_;

    DISALLOW_IMPLICIT_CONSTRUCTORS(BlockTrampolinePoolScope);
  };

  // Class for postponing the assembly buffer growth. Typically used for
  // sequences of instructions that must be emitted as a unit, before
  // buffer growth (and relocation) can occur.
  // This blocking scope is not nestable.
  class BlockGrowBufferScope {
   public:
    explicit BlockGrowBufferScope(Assembler* assem) : assem_(assem) {
      assem_->StartBlockGrowBuffer();
    }
    ~BlockGrowBufferScope() {
      assem_->EndBlockGrowBuffer();
    }

   private:
    Assembler* assem_;

    DISALLOW_IMPLICIT_CONSTRUCTORS(BlockGrowBufferScope);
  };

  // Record a comment relocation entry that can be used by a disassembler.
  // Use --code-comments to enable.
  void RecordComment(const char* msg);

  // Record a deoptimization reason that can be used by a log or cpu profiler.
  // Use --trace-deopt to enable.
  void RecordDeoptReason(DeoptimizeReason reason, SourcePosition position,
                         int id);

  static int RelocateInternalReference(RelocInfo::Mode rmode, Address pc,
                                       intptr_t pc_delta);

  static void RelocateRelativeReference(RelocInfo::Mode rmode, Address pc,
                                        intptr_t pc_delta);

  // Writes a single byte or word of data in the code stream.  Used for
  // inline tables, e.g., jump-tables.
  void db(uint8_t data);
  void dd(uint32_t data);
  void dq(uint64_t data);
  void dp(uintptr_t data) { dd(data); }
  void dd(Label* label);

  // Postpone the generation of the trampoline pool for the specified number of
  // instructions.
  void BlockTrampolinePoolFor(int instructions);

  // Check if there is less than kGap bytes available in the buffer.
  // If this is the case, we need to grow the buffer before emitting
  // an instruction or relocation information.
  inline bool overflow() const { return pc_ >= reloc_info_writer.pos() - kGap; }

  // Get the number of bytes available in the buffer.
  inline int available_space() const { return reloc_info_writer.pos() - pc_; }

  // Read/patch instructions.
  static Instr instr_at(Address pc) { return *reinterpret_cast<Instr*>(pc); }
  static void instr_at_put(Address pc, Instr instr) {
    *reinterpret_cast<Instr*>(pc) = instr;
  }
  Instr instr_at(int pos) { return *reinterpret_cast<Instr*>(buffer_ + pos); }
  void instr_at_put(int pos, Instr instr) {
    *reinterpret_cast<Instr*>(buffer_ + pos) = instr;
  }

  // Check if an instruction is a branch of some kind.
  static bool IsBranch(Instr instr);
  static bool IsMsaBranch(Instr instr);
  static bool IsBc(Instr instr);
  static bool IsNal(Instr instr);
  static bool IsBzc(Instr instr);
  static bool IsBeq(Instr instr);
  static bool IsBne(Instr instr);
  static bool IsBeqzc(Instr instr);
  static bool IsBnezc(Instr instr);
  static bool IsBeqc(Instr instr);
  static bool IsBnec(Instr instr);
  static bool IsJicOrJialc(Instr instr);
  static bool IsMov(Instr instr, Register rd, Register rs);

  static bool IsJump(Instr instr);
  static bool IsJ(Instr instr);
  static bool IsLui(Instr instr);
  static bool IsOri(Instr instr);
  static bool IsAddu(Instr instr, Register rd, Register rs, Register rt);

  static bool IsJal(Instr instr);
  static bool IsJr(Instr instr);
  static bool IsJalr(Instr instr);

  static bool IsNop(Instr instr, unsigned int type);
  static bool IsPop(Instr instr);
  static bool IsPush(Instr instr);
  static bool IsLwRegFpOffset(Instr instr);
  static bool IsSwRegFpOffset(Instr instr);
  static bool IsLwRegFpNegOffset(Instr instr);
  static bool IsSwRegFpNegOffset(Instr instr);

  static Register GetRtReg(Instr instr);
  static Register GetRsReg(Instr instr);
  static Register GetRdReg(Instr instr);

  static uint32_t GetRt(Instr instr);
  static uint32_t GetRtField(Instr instr);
  static uint32_t GetRs(Instr instr);
  static uint32_t GetRsField(Instr instr);
  static uint32_t GetRd(Instr instr);
  static uint32_t GetRdField(Instr instr);
  static uint32_t GetSa(Instr instr);
  static uint32_t GetSaField(Instr instr);
  static uint32_t GetOpcodeField(Instr instr);
  static uint32_t GetFunction(Instr instr);
  static uint32_t GetFunctionField(Instr instr);
  static uint32_t GetImmediate16(Instr instr);
  static uint32_t GetLabelConst(Instr instr);

  static int32_t GetBranchOffset(Instr instr);
  static bool IsLw(Instr instr);
  static int16_t GetLwOffset(Instr instr);
  static int16_t GetJicOrJialcOffset(Instr instr);
  static int16_t GetLuiOffset(Instr instr);
  static Instr SetLwOffset(Instr instr, int16_t offset);

  static bool IsSw(Instr instr);
  static Instr SetSwOffset(Instr instr, int16_t offset);
  static bool IsAddImmediate(Instr instr);
  static Instr SetAddImmediateOffset(Instr instr, int16_t offset);
  static uint32_t CreateTargetAddress(Instr instr_lui, Instr instr_jic);
  static void UnpackTargetAddress(uint32_t address, int16_t& lui_offset,
                                  int16_t& jic_offset);
  static void UnpackTargetAddressUnsigned(uint32_t address,
                                          uint32_t& lui_offset,
                                          uint32_t& jic_offset);

  static bool IsAndImmediate(Instr instr);
  static bool IsEmittedConstant(Instr instr);

  void CheckTrampolinePool();

  bool IsPrevInstrCompactBranch() { return prev_instr_compact_branch_; }
  static bool IsCompactBranchSupported() {
    return IsMipsArchVariant(kMips32r6);
  }

  // Get the code target object for a pc-relative call or jump.
  V8_INLINE Handle<Code> relative_code_target_object_handle_at(
      Address pc_) const;

  inline int UnboundLabelsCount() { return unbound_labels_count_; }

 protected:
  // Load Scaled Address instruction.
  void lsa(Register rd, Register rt, Register rs, uint8_t sa);

  // Readable constants for base and offset adjustment helper, these indicate if
  // aside from offset, another value like offset + 4 should fit into int16.
  enum class OffsetAccessType : bool {
    SINGLE_ACCESS = false,
    TWO_ACCESSES = true
  };

  // Helper function for memory load/store using base register and offset.
  void AdjustBaseAndOffset(
      MemOperand& src,
      OffsetAccessType access_type = OffsetAccessType::SINGLE_ACCESS,
      int second_access_add_to_offset = 4);

  int32_t buffer_space() const { return reloc_info_writer.pos() - pc_; }

  // Decode branch instruction at pos and return branch target pos.
  int target_at(int pos, bool is_internal);

  // Patch branch instruction at pos to branch to given branch target pos.
  void target_at_put(int pos, int target_pos, bool is_internal);

  // Say if we need to relocate with this mode.
  bool MustUseReg(RelocInfo::Mode rmode);

  // Record reloc info for current pc_.
  void RecordRelocInfo(RelocInfo::Mode rmode, intptr_t data = 0);

  // Read 32-bit immediate from lui, ori pair that is used to load immediate.
  static int32_t GetLuiOriImmediate(Instr instr1, Instr instr2);

  // Block the emission of the trampoline pool before pc_offset.
  void BlockTrampolinePoolBefore(int pc_offset) {
    if (no_trampoline_pool_before_ < pc_offset)
      no_trampoline_pool_before_ = pc_offset;
  }

  void StartBlockTrampolinePool() {
    trampoline_pool_blocked_nesting_++;
  }

  void EndBlockTrampolinePool() {
    trampoline_pool_blocked_nesting_--;
    if (trampoline_pool_blocked_nesting_ == 0) {
      CheckTrampolinePoolQuick(1);
    }
  }

  bool is_trampoline_pool_blocked() const {
    return trampoline_pool_blocked_nesting_ > 0;
  }

  bool has_exception() const {
    return internal_trampoline_exception_;
  }

  bool is_trampoline_emitted() const {
    return trampoline_emitted_;
  }

  // Temporarily block automatic assembly buffer growth.
  void StartBlockGrowBuffer() {
    DCHECK(!block_buffer_growth_);
    block_buffer_growth_ = true;
  }

  void EndBlockGrowBuffer() {
    DCHECK(block_buffer_growth_);
    block_buffer_growth_ = false;
  }

  bool is_buffer_growth_blocked() const {
    return block_buffer_growth_;
  }

  void EmitForbiddenSlotInstruction() {
    if (IsPrevInstrCompactBranch()) {
      nop();
    }
  }

  inline void CheckTrampolinePoolQuick(int extra_instructions = 0) {
    if (pc_offset() >= next_buffer_check_ - extra_instructions * kInstrSize) {
      CheckTrampolinePool();
    }
  }

  inline void CheckBuffer();

  RegList scratch_register_list_;

  // Generate common instruction sequence.
  void GenPCRelativeJump(Register tf, Register ts, int32_t imm32,
                         RelocInfo::Mode rmode, BranchDelaySlot bdslot);
  void GenPCRelativeJumpAndLink(Register t, int32_t imm32,
                                RelocInfo::Mode rmode, BranchDelaySlot bdslot);

 private:
  // Avoid overflows for displacements etc.
  static const int kMaximalBufferSize = 512 * MB;

  inline static void set_target_internal_reference_encoded_at(Address pc,
                                                              Address target);

  // Buffer size and constant pool distance are checked together at regular
  // intervals of kBufferCheckInterval emitted bytes.
  static constexpr int kBufferCheckInterval = 1 * KB / 2;

  // Code generation.
  // The relocation writer's position is at least kGap bytes below the end of
  // the generated instructions. This is so that multi-instruction sequences do
  // not have to check for overflow. The same is true for writes of large
  // relocation info entries.
  static constexpr int kGap = 32;

  // Repeated checking whether the trampoline pool should be emitted is rather
  // expensive. By default we only check again once a number of instructions
  // has been generated.
  static constexpr int kCheckConstIntervalInst = 32;
  static constexpr int kCheckConstInterval =
      kCheckConstIntervalInst * kInstrSize;

  int next_buffer_check_;  // pc offset of next buffer check.

  // Emission of the trampoline pool may be blocked in some code sequences.
  int trampoline_pool_blocked_nesting_;  // Block emission if this is not zero.
  int no_trampoline_pool_before_;  // Block emission before this pc offset.

  // Keep track of the last emitted pool to guarantee a maximal distance.
  int last_trampoline_pool_end_;  // pc offset of the end of the last pool.

  // Automatic growth of the assembly buffer may be blocked for some sequences.
  bool block_buffer_growth_;  // Block growth when true.

  // Relocation information generation.
  // Each relocation is encoded as a variable size value.
  static constexpr int kMaxRelocSize = RelocInfoWriter::kMaxSize;
  RelocInfoWriter reloc_info_writer;

  // The bound position, before this we cannot do instruction elimination.
  int last_bound_pos_;

  // Readable constants for compact branch handling in emit()
  enum class CompactBranchType : bool { NO = false, COMPACT_BRANCH = true };

  // Code emission.
  void GrowBuffer();
  inline void emit(Instr x,
                   CompactBranchType is_compact_branch = CompactBranchType::NO);
  inline void emit(uint64_t x);
  inline void CheckForEmitInForbiddenSlot();
  template <typename T>
  inline void EmitHelper(T x);
  inline void EmitHelper(Instr x, CompactBranchType is_compact_branch);

  // Instruction generation.
  // We have 3 different kind of encoding layout on MIPS.
  // However due to many different types of objects encoded in the same fields
  // we have quite a few aliases for each mode.
  // Using the same structure to refer to Register and FPURegister would spare a
  // few aliases, but mixing both does not look clean to me.
  // Anyway we could surely implement this differently.

  void GenInstrRegister(Opcode opcode, Register rs, Register rt, Register rd,
                        uint16_t sa = 0, SecondaryField func = nullptrSF);

  void GenInstrRegister(Opcode opcode,
                        Register rs,
                        Register rt,
                        uint16_t msb,
                        uint16_t lsb,
                        SecondaryField func);

  void GenInstrRegister(Opcode opcode, SecondaryField fmt, FPURegister ft,
                        FPURegister fs, FPURegister fd,
                        SecondaryField func = nullptrSF);

  void GenInstrRegister(Opcode opcode, FPURegister fr, FPURegister ft,
                        FPURegister fs, FPURegister fd,
                        SecondaryField func = nullptrSF);

  void GenInstrRegister(Opcode opcode, SecondaryField fmt, Register rt,
                        FPURegister fs, FPURegister fd,
                        SecondaryField func = nullptrSF);

  void GenInstrRegister(Opcode opcode, SecondaryField fmt, Register rt,
                        FPUControlRegister fs, SecondaryField func = nullptrSF);

  void GenInstrImmediate(
      Opcode opcode, Register rs, Register rt, int32_t j,
      CompactBranchType is_compact_branch = CompactBranchType::NO);
  void GenInstrImmediate(
      Opcode opcode, Register rs, SecondaryField SF, int32_t j,
      CompactBranchType is_compact_branch = CompactBranchType::NO);
  void GenInstrImmediate(
      Opcode opcode, Register r1, FPURegister r2, int32_t j,
      CompactBranchType is_compact_branch = CompactBranchType::NO);
  void GenInstrImmediate(Opcode opcode, Register base, Register rt,
                         int32_t offset9, int bit6, SecondaryField func);
  void GenInstrImmediate(
      Opcode opcode, Register rs, int32_t offset21,
      CompactBranchType is_compact_branch = CompactBranchType::NO);
  void GenInstrImmediate(Opcode opcode, Register rs, uint32_t offset21);
  void GenInstrImmediate(
      Opcode opcode, int32_t offset26,
      CompactBranchType is_compact_branch = CompactBranchType::NO);


  void GenInstrJump(Opcode opcode,
                     uint32_t address);

  // MSA
  void GenInstrMsaI8(SecondaryField operation, uint32_t imm8, MSARegister ws,
                     MSARegister wd);

  void GenInstrMsaI5(SecondaryField operation, SecondaryField df, int32_t imm5,
                     MSARegister ws, MSARegister wd);

  void GenInstrMsaBit(SecondaryField operation, SecondaryField df, uint32_t m,
                      MSARegister ws, MSARegister wd);

  void GenInstrMsaI10(SecondaryField operation, SecondaryField df,
                      int32_t imm10, MSARegister wd);

  template <typename RegType>
  void GenInstrMsa3R(SecondaryField operation, SecondaryField df, RegType t,
                     MSARegister ws, MSARegister wd);

  template <typename DstType, typename SrcType>
  void GenInstrMsaElm(SecondaryField operation, SecondaryField df, uint32_t n,
                      SrcType src, DstType dst);

  void GenInstrMsa3RF(SecondaryField operation, uint32_t df, MSARegister wt,
                      MSARegister ws, MSARegister wd);

  void GenInstrMsaVec(SecondaryField operation, MSARegister wt, MSARegister ws,
                      MSARegister wd);

  void GenInstrMsaMI10(SecondaryField operation, int32_t s10, Register rs,
                       MSARegister wd);

  void GenInstrMsa2R(SecondaryField operation, SecondaryField df,
                     MSARegister ws, MSARegister wd);

  void GenInstrMsa2RF(SecondaryField operation, SecondaryField df,
                      MSARegister ws, MSARegister wd);

  void GenInstrMsaBranch(SecondaryField operation, MSARegister wt,
                         int32_t offset16);

  inline bool is_valid_msa_df_m(SecondaryField bit_df, uint32_t m) {
    switch (bit_df) {
      case BIT_DF_b:
        return is_uint3(m);
      case BIT_DF_h:
        return is_uint4(m);
      case BIT_DF_w:
        return is_uint5(m);
      case BIT_DF_d:
        return is_uint6(m);
      default:
        return false;
    }
  }

  inline bool is_valid_msa_df_n(SecondaryField elm_df, uint32_t n) {
    switch (elm_df) {
      case ELM_DF_B:
        return is_uint4(n);
      case ELM_DF_H:
        return is_uint3(n);
      case ELM_DF_W:
        return is_uint2(n);
      case ELM_DF_D:
        return is_uint1(n);
      default:
        return false;
    }
  }

  // Labels.
  void print(const Label* L);
  void bind_to(Label* L, int pos);
  void next(Label* L, bool is_internal);

  // Patching lui/ori pair which is commonly used for loading constants.
  static void PatchLuiOriImmediate(Address pc, int32_t imm, Instr instr1,
                                   Address offset_lui, Instr instr2,
                                   Address offset_ori);
  void PatchLuiOriImmediate(int pc, int32_t imm, Instr instr1,
                            Address offset_lui, Instr instr2,
                            Address offset_ori);

  // One trampoline consists of:
  // - space for trampoline slots,
  // - space for labels.
  //
  // Space for trampoline slots is equal to slot_count * 2 * kInstrSize.
  // Space for trampoline slots precedes space for labels. Each label is of one
  // instruction size, so total amount for labels is equal to
  // label_count *  kInstrSize.
  class Trampoline {
   public:
    Trampoline() {
      start_ = 0;
      next_slot_ = 0;
      free_slot_count_ = 0;
      end_ = 0;
    }
    Trampoline(int start, int slot_count) {
      start_ = start;
      next_slot_ = start;
      free_slot_count_ = slot_count;
      end_ = start + slot_count * kTrampolineSlotsSize;
    }
    int start() {
      return start_;
    }
    int end() {
      return end_;
    }
    int take_slot() {
      int trampoline_slot = kInvalidSlotPos;
      if (free_slot_count_ <= 0) {
        // We have run out of space on trampolines.
        // Make sure we fail in debug mode, so we become aware of each case
        // when this happens.
        DCHECK(0);
        // Internal exception will be caught.
      } else {
        trampoline_slot = next_slot_;
        free_slot_count_--;
        next_slot_ += kTrampolineSlotsSize;
      }
      return trampoline_slot;
    }

   private:
    int start_;
    int end_;
    int next_slot_;
    int free_slot_count_;
  };

  int32_t get_trampoline_entry(int32_t pos);
  int unbound_labels_count_;
  // If trampoline is emitted, generated code is becoming large. As this is
  // already a slow case which can possibly break our code generation for the
  // extreme case, we use this information to trigger different mode of
  // branch instruction generation, where we use jump instructions rather
  // than regular branch instructions.
  bool trampoline_emitted_;
  static constexpr int kInvalidSlotPos = -1;

  // Internal reference positions, required for unbounded internal reference
  // labels.
  std::set<int> internal_reference_positions_;
  bool is_internal_reference(Label* L) {
    return internal_reference_positions_.find(L->pos()) !=
           internal_reference_positions_.end();
  }

  void EmittedCompactBranchInstruction() { prev_instr_compact_branch_ = true; }
  void ClearCompactBranchState() { prev_instr_compact_branch_ = false; }
  bool prev_instr_compact_branch_ = false;

  Trampoline trampoline_;
  bool internal_trampoline_exception_;

 private:
  void AllocateAndInstallRequestedHeapObjects(Isolate* isolate);

  friend class RegExpMacroAssemblerMIPS;
  friend class RelocInfo;
  friend class BlockTrampolinePoolScope;
  friend class EnsureSpace;
};

class EnsureSpace {
 public:
  explicit inline EnsureSpace(Assembler* assembler);
};

class UseScratchRegisterScope {
 public:
  explicit UseScratchRegisterScope(Assembler* assembler);
  ~UseScratchRegisterScope();

  Register Acquire();
  bool hasAvailable() const;

 private:
  RegList* available_;
  RegList old_available_;
};

// Define {RegisterName} methods for the register types.
DEFINE_REGISTER_NAMES(Register, GENERAL_REGISTERS)
DEFINE_REGISTER_NAMES(FPURegister, DOUBLE_REGISTERS)
DEFINE_REGISTER_NAMES(MSARegister, SIMD128_REGISTERS)

}  // namespace internal
}  // namespace v8

#endif  // V8_MIPS_ASSEMBLER_MIPS_H_
