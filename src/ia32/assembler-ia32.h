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
// Copyright 2011 the V8 project authors. All rights reserved.

// A light-weight IA32 Assembler.

#ifndef V8_IA32_ASSEMBLER_IA32_H_
#define V8_IA32_ASSEMBLER_IA32_H_

#include <deque>

#include "src/assembler.h"
#include "src/ia32/sse-instr.h"
#include "src/isolate.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

#define GENERAL_REGISTERS(V) \
  V(eax)                     \
  V(ecx)                     \
  V(edx)                     \
  V(ebx)                     \
  V(esp)                     \
  V(ebp)                     \
  V(esi)                     \
  V(edi)

#define ALLOCATABLE_GENERAL_REGISTERS(V) \
  V(eax)                                 \
  V(ecx)                                 \
  V(edx)                                 \
  V(ebx)                                 \
  V(esi)                                 \
  V(edi)

#define DOUBLE_REGISTERS(V) \
  V(xmm0)                   \
  V(xmm1)                   \
  V(xmm2)                   \
  V(xmm3)                   \
  V(xmm4)                   \
  V(xmm5)                   \
  V(xmm6)                   \
  V(xmm7)

#define FLOAT_REGISTERS DOUBLE_REGISTERS
#define SIMD128_REGISTERS DOUBLE_REGISTERS

#define ALLOCATABLE_DOUBLE_REGISTERS(V) \
  V(xmm1)                               \
  V(xmm2)                               \
  V(xmm3)                               \
  V(xmm4)                               \
  V(xmm5)                               \
  V(xmm6)                               \
  V(xmm7)

enum RegisterCode {
#define REGISTER_CODE(R) kRegCode_##R,
  GENERAL_REGISTERS(REGISTER_CODE)
#undef REGISTER_CODE
      kRegAfterLast
};

class Register : public RegisterBase<Register, kRegAfterLast> {
 public:
  bool is_byte_register() const { return reg_code_ <= 3; }

 private:
  friend class RegisterBase<Register, kRegAfterLast>;
  explicit constexpr Register(int code) : RegisterBase(code) {}
};

ASSERT_TRIVIALLY_COPYABLE(Register);
static_assert(sizeof(Register) == sizeof(int),
              "Register can efficiently be passed by value");

#define DEFINE_REGISTER(R) \
  constexpr Register R = Register::from_code<kRegCode_##R>();
GENERAL_REGISTERS(DEFINE_REGISTER)
#undef DEFINE_REGISTER
constexpr Register no_reg = Register::no_reg();

constexpr bool kPadArguments = false;
constexpr bool kSimpleFPAliasing = true;
constexpr bool kSimdMaskRegisters = false;

enum DoubleCode {
#define REGISTER_CODE(R) kDoubleCode_##R,
  DOUBLE_REGISTERS(REGISTER_CODE)
#undef REGISTER_CODE
      kDoubleAfterLast
};

class XMMRegister : public RegisterBase<XMMRegister, kDoubleAfterLast> {
  friend class RegisterBase<XMMRegister, kDoubleAfterLast>;
  explicit constexpr XMMRegister(int code) : RegisterBase(code) {}
};

typedef XMMRegister FloatRegister;

typedef XMMRegister DoubleRegister;

typedef XMMRegister Simd128Register;

#define DEFINE_REGISTER(R) \
  constexpr DoubleRegister R = DoubleRegister::from_code<kDoubleCode_##R>();
DOUBLE_REGISTERS(DEFINE_REGISTER)
#undef DEFINE_REGISTER
constexpr DoubleRegister no_double_reg = DoubleRegister::no_reg();

// Note that the bit values must match those used in actual instruction encoding
constexpr int kNumRegs = 8;

// Caller-saved registers
constexpr RegList kJSCallerSaved =
    Register::ListOf<eax, ecx, edx,
                     ebx,  // used as a caller-saved register in JavaScript code
                     edi   // callee function
                     >();

constexpr int kNumJSCallerSaved = 5;

// Number of registers for which space is reserved in safepoints.
constexpr int kNumSafepointRegisters = 8;

enum Condition {
  // any value < 0 is considered no_condition
  no_condition  = -1,

  overflow      =  0,
  no_overflow   =  1,
  below         =  2,
  above_equal   =  3,
  equal         =  4,
  not_equal     =  5,
  below_equal   =  6,
  above         =  7,
  negative      =  8,
  positive      =  9,
  parity_even   = 10,
  parity_odd    = 11,
  less          = 12,
  greater_equal = 13,
  less_equal    = 14,
  greater       = 15,

  // aliases
  carry         = below,
  not_carry     = above_equal,
  zero          = equal,
  not_zero      = not_equal,
  sign          = negative,
  not_sign      = positive
};


// Returns the equivalent of !cc.
// Negation of the default no_condition (-1) results in a non-default
// no_condition value (-2). As long as tests for no_condition check
// for condition < 0, this will work as expected.
inline Condition NegateCondition(Condition cc) {
  return static_cast<Condition>(cc ^ 1);
}


// Commute a condition such that {a cond b == b cond' a}.
inline Condition CommuteCondition(Condition cc) {
  switch (cc) {
    case below:
      return above;
    case above:
      return below;
    case above_equal:
      return below_equal;
    case below_equal:
      return above_equal;
    case less:
      return greater;
    case greater:
      return less;
    case greater_equal:
      return less_equal;
    case less_equal:
      return greater_equal;
    default:
      return cc;
  }
}


enum RoundingMode {
  kRoundToNearest = 0x0,
  kRoundDown = 0x1,
  kRoundUp = 0x2,
  kRoundToZero = 0x3
};

// -----------------------------------------------------------------------------
// Machine instruction Immediates

class Immediate BASE_EMBEDDED {
 public:
  inline explicit Immediate(int x, RelocInfo::Mode rmode = RelocInfo::NONE) {
    value_.immediate = x;
    rmode_ = rmode;
  }
  inline explicit Immediate(const ExternalReference& ext)
      : Immediate(ext.address(), RelocInfo::EXTERNAL_REFERENCE) {}
  inline explicit Immediate(Handle<HeapObject> handle)
      : Immediate(handle.address(), RelocInfo::EMBEDDED_OBJECT) {}
  inline explicit Immediate(Smi* value)
      : Immediate(reinterpret_cast<intptr_t>(value)) {}
  inline explicit Immediate(Address addr,
                            RelocInfo::Mode rmode = RelocInfo::NONE)
      : Immediate(reinterpret_cast<int32_t>(addr), rmode) {}

  static Immediate EmbeddedNumber(double number);  // Smi or HeapNumber.
  static Immediate EmbeddedCode(CodeStub* code);

  static Immediate CodeRelativeOffset(Label* label) {
    return Immediate(label);
  }

  bool is_heap_object_request() const {
    DCHECK_IMPLIES(is_heap_object_request_,
                   rmode_ == RelocInfo::EMBEDDED_OBJECT ||
                       rmode_ == RelocInfo::CODE_TARGET);
    return is_heap_object_request_;
  }

  HeapObjectRequest heap_object_request() const {
    DCHECK(is_heap_object_request());
    return value_.heap_object_request;
  }

  int immediate() const {
    DCHECK(!is_heap_object_request());
    return value_.immediate;
  }

  bool is_zero() const { return RelocInfo::IsNone(rmode_) && immediate() == 0; }
  bool is_int8() const {
    return RelocInfo::IsNone(rmode_) && i::is_int8(immediate());
  }
  bool is_uint8() const {
    return RelocInfo::IsNone(rmode_) && i::is_uint8(immediate());
  }
  bool is_int16() const {
    return RelocInfo::IsNone(rmode_) && i::is_int16(immediate());
  }

  bool is_uint16() const {
    return RelocInfo::IsNone(rmode_) && i::is_uint16(immediate());
  }

  RelocInfo::Mode rmode() const { return rmode_; }

 private:
  inline explicit Immediate(Label* value) {
    value_.immediate = reinterpret_cast<int32_t>(value);
    rmode_ = RelocInfo::INTERNAL_REFERENCE;
  }

  union Value {
    Value() {}
    HeapObjectRequest heap_object_request;
    int immediate;
  } value_;
  bool is_heap_object_request_ = false;
  RelocInfo::Mode rmode_;

  friend class Operand;
  friend class Assembler;
  friend class MacroAssembler;
};


// -----------------------------------------------------------------------------
// Machine instruction Operands

enum ScaleFactor {
  times_1 = 0,
  times_2 = 1,
  times_4 = 2,
  times_8 = 3,
  times_int_size = times_4,
  times_half_pointer_size = times_2,
  times_pointer_size = times_4,
  times_twice_pointer_size = times_8
};

class Operand {
 public:
  // reg
  INLINE(explicit Operand(Register reg)) { set_modrm(3, reg); }

  // XMM reg
  INLINE(explicit Operand(XMMRegister xmm_reg)) {
    Register reg = Register::from_code(xmm_reg.code());
    set_modrm(3, reg);
  }

  // [disp/r]
  INLINE(explicit Operand(int32_t disp, RelocInfo::Mode rmode)) {
    set_modrm(0, ebp);
    set_dispr(disp, rmode);
  }

  // [disp/r]
  INLINE(explicit Operand(Immediate imm)) {
    set_modrm(0, ebp);
    set_dispr(imm.immediate(), imm.rmode_);
  }

  // [base + disp/r]
  explicit Operand(Register base, int32_t disp,
                   RelocInfo::Mode rmode = RelocInfo::NONE);

  // [base + index*scale + disp/r]
  explicit Operand(Register base, Register index, ScaleFactor scale,
                   int32_t disp, RelocInfo::Mode rmode = RelocInfo::NONE);

  // [index*scale + disp/r]
  explicit Operand(Register index, ScaleFactor scale, int32_t disp,
                   RelocInfo::Mode rmode = RelocInfo::NONE);

  static Operand JumpTable(Register index, ScaleFactor scale, Label* table) {
    return Operand(index, scale, reinterpret_cast<int32_t>(table),
                   RelocInfo::INTERNAL_REFERENCE);
  }

  static Operand StaticVariable(const ExternalReference& ext) {
    return Operand(reinterpret_cast<int32_t>(ext.address()),
                   RelocInfo::EXTERNAL_REFERENCE);
  }

  static Operand StaticArray(Register index,
                             ScaleFactor scale,
                             const ExternalReference& arr) {
    return Operand(index, scale, reinterpret_cast<int32_t>(arr.address()),
                   RelocInfo::EXTERNAL_REFERENCE);
  }

  static Operand ForRegisterPlusImmediate(Register base, Immediate imm) {
    return Operand(base, imm.value_.immediate, imm.rmode_);
  }

  // Returns true if this Operand is a wrapper for the specified register.
  bool is_reg(Register reg) const { return is_reg(reg.code()); }
  bool is_reg(XMMRegister reg) const { return is_reg(reg.code()); }

  // Returns true if this Operand is a wrapper for one register.
  bool is_reg_only() const;

  // Asserts that this Operand is a wrapper for one register and returns the
  // register.
  Register reg() const;

 private:
  // Set the ModRM byte without an encoded 'reg' register. The
  // register is encoded later as part of the emit_operand operation.
  inline void set_modrm(int mod, Register rm) {
    DCHECK_EQ(mod & -4, 0);
    buf_[0] = mod << 6 | rm.code();
    len_ = 1;
  }

  inline void set_sib(ScaleFactor scale, Register index, Register base);
  inline void set_disp8(int8_t disp);
  inline void set_dispr(int32_t disp, RelocInfo::Mode rmode) {
    DCHECK(len_ == 1 || len_ == 2);
    int32_t* p = reinterpret_cast<int32_t*>(&buf_[len_]);
    *p = disp;
    len_ += sizeof(int32_t);
    rmode_ = rmode;
  }

  inline bool is_reg(int reg_code) const {
    return ((buf_[0] & 0xF8) == 0xC0)  // addressing mode is register only.
           && ((buf_[0] & 0x07) == reg_code);  // register codes match.
  }

  byte buf_[6];
  // The number of bytes in buf_.
  uint8_t len_;
  // Only valid if len_ > 4.
  RelocInfo::Mode rmode_;

  // TODO(clemensh): Get rid of this friendship, or make Operand immutable.
  friend class Assembler;
};
ASSERT_TRIVIALLY_COPYABLE(Operand);
static_assert(sizeof(Operand) <= 2 * kPointerSize,
              "Operand must be small enough to pass it by value");

// -----------------------------------------------------------------------------
// A Displacement describes the 32bit immediate field of an instruction which
// may be used together with a Label in order to refer to a yet unknown code
// position. Displacements stored in the instruction stream are used to describe
// the instruction and to chain a list of instructions using the same Label.
// A Displacement contains 2 different fields:
//
// next field: position of next displacement in the chain (0 = end of list)
// type field: instruction type
//
// A next value of null (0) indicates the end of a chain (note that there can
// be no displacement at position zero, because there is always at least one
// instruction byte before the displacement).
//
// Displacement _data field layout
//
// |31.....2|1......0|
// [  next  |  type  |

class Displacement BASE_EMBEDDED {
 public:
  enum Type { UNCONDITIONAL_JUMP, CODE_RELATIVE, OTHER, CODE_ABSOLUTE };

  int data() const { return data_; }
  Type type() const { return TypeField::decode(data_); }
  void next(Label* L) const {
    int n = NextField::decode(data_);
    n > 0 ? L->link_to(n) : L->Unuse();
  }
  void link_to(Label* L) { init(L, type()); }

  explicit Displacement(int data) { data_ = data; }

  Displacement(Label* L, Type type) { init(L, type); }

  void print() {
    PrintF("%s (%x) ", (type() == UNCONDITIONAL_JUMP ? "jmp" : "[other]"),
                       NextField::decode(data_));
  }

 private:
  int data_;

  class TypeField: public BitField<Type, 0, 2> {};
  class NextField: public BitField<int,  2, 32-2> {};

  void init(Label* L, Type type);
};


class Assembler : public AssemblerBase {
 private:
  // We check before assembling an instruction that there is sufficient
  // space to write an instruction and its relocation information.
  // The relocation writer's position must be kGap bytes above the end of
  // the generated instructions. This leaves enough space for the
  // longest possible ia32 instruction, 15 bytes, and the longest possible
  // relocation information encoding, RelocInfoWriter::kMaxLength == 16.
  // (There is a 15 byte limit on ia32 instruction length that rules out some
  // otherwise valid instructions.)
  // This allows for a single, fast space check per instruction.
  static constexpr int kGap = 32;

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
  Assembler(Isolate* isolate, void* buffer, int buffer_size)
      : Assembler(IsolateData(isolate), buffer, buffer_size) {}
  Assembler(IsolateData isolate_data, void* buffer, int buffer_size);
  virtual ~Assembler() {}

  // GetCode emits any pending (non-emitted) code and fills the descriptor
  // desc. GetCode() is idempotent; it returns the same result if no other
  // Assembler functions are invoked in between GetCode() calls.
  void GetCode(Isolate* isolate, CodeDesc* desc);

  // Read/Modify the code target in the branch/call instruction at pc.
  // The isolate argument is unused (and may be nullptr) when skipping flushing.
  inline static Address target_address_at(Address pc, Address constant_pool);
  inline static void set_target_address_at(
      Address pc, Address constant_pool, Address target,
      ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED);

  // Return the code target address at a call site from the return address
  // of that call in the instruction stream.
  inline static Address target_address_from_return_address(Address pc);

  // This sets the branch destination (which is in the instruction on x86).
  // This is for calls and branches within generated code.
  inline static void deserialization_set_special_target_at(
      Address instruction_payload, Code* code, Address target);

  // This sets the internal reference at the pc.
  inline static void deserialization_set_target_internal_reference_at(
      Address pc, Address target,
      RelocInfo::Mode mode = RelocInfo::INTERNAL_REFERENCE);

  static constexpr int kSpecialTargetSize = kPointerSize;

  // Distance between the address of the code target in the call instruction
  // and the return address
  static constexpr int kCallTargetAddressOffset = kPointerSize;

  static constexpr int kCallInstructionLength = 5;

  // One byte opcode for test al, 0xXX.
  static constexpr byte kTestAlByte = 0xA8;
  // One byte opcode for nop.
  static constexpr byte kNopByte = 0x90;

  // One byte opcode for a short unconditional jump.
  static constexpr byte kJmpShortOpcode = 0xEB;
  // One byte prefix for a short conditional jump.
  static constexpr byte kJccShortPrefix = 0x70;
  static constexpr byte kJncShortOpcode = kJccShortPrefix | not_carry;
  static constexpr byte kJcShortOpcode = kJccShortPrefix | carry;
  static constexpr byte kJnzShortOpcode = kJccShortPrefix | not_zero;
  static constexpr byte kJzShortOpcode = kJccShortPrefix | zero;

  // ---------------------------------------------------------------------------
  // Code generation
  //
  // - function names correspond one-to-one to ia32 instruction mnemonics
  // - unless specified otherwise, instructions operate on 32bit operands
  // - instructions on 8bit (byte) operands/registers have a trailing '_b'
  // - instructions on 16bit (word) operands/registers have a trailing '_w'
  // - naming conflicts with C++ keywords are resolved via a trailing '_'

  // NOTE ON INTERFACE: Currently, the interface is not very consistent
  // in the sense that some operations (e.g. mov()) can be called in more
  // the one way to generate the same instruction: The Register argument
  // can in some cases be replaced with an Operand(Register) argument.
  // This should be cleaned up and made more orthogonal. The questions
  // is: should we always use Operands instead of Registers where an
  // Operand is possible, or should we have a Register (overloaded) form
  // instead? We must be careful to make sure that the selected instruction
  // is obvious from the parameters to avoid hard-to-find code generation
  // bugs.

  // Insert the smallest number of nop instructions
  // possible to align the pc offset to a multiple
  // of m. m must be a power of 2.
  void Align(int m);
  // Insert the smallest number of zero bytes possible to align the pc offset
  // to a mulitple of m. m must be a power of 2 (>= 2).
  void DataAlign(int m);
  void Nop(int bytes = 1);
  // Aligns code to something that's optimal for a jump target for the platform.
  void CodeTargetAlign();

  // Stack
  void pushad();
  void popad();

  void pushfd();
  void popfd();

  void push(const Immediate& x);
  void push_imm32(int32_t imm32);
  void push(Register src);
  void push(Operand src);

  void pop(Register dst);
  void pop(Operand dst);

  void enter(const Immediate& size);
  void leave();

  // Moves
  void mov_b(Register dst, Register src) { mov_b(dst, Operand(src)); }
  void mov_b(Register dst, Operand src);
  void mov_b(Register dst, int8_t imm8) { mov_b(Operand(dst), imm8); }
  void mov_b(Operand dst, int8_t src) { mov_b(dst, Immediate(src)); }
  void mov_b(Operand dst, const Immediate& src);
  void mov_b(Operand dst, Register src);

  void mov_w(Register dst, Operand src);
  void mov_w(Operand dst, int16_t src) { mov_w(dst, Immediate(src)); }
  void mov_w(Operand dst, const Immediate& src);
  void mov_w(Operand dst, Register src);

  void mov(Register dst, int32_t imm32);
  void mov(Register dst, const Immediate& x);
  void mov(Register dst, Handle<HeapObject> handle);
  void mov(Register dst, Operand src);
  void mov(Register dst, Register src);
  void mov(Operand dst, const Immediate& x);
  void mov(Operand dst, Handle<HeapObject> handle);
  void mov(Operand dst, Register src);
  void mov(Operand dst, Address src, RelocInfo::Mode);

  void movsx_b(Register dst, Register src) { movsx_b(dst, Operand(src)); }
  void movsx_b(Register dst, Operand src);

  void movsx_w(Register dst, Register src) { movsx_w(dst, Operand(src)); }
  void movsx_w(Register dst, Operand src);

  void movzx_b(Register dst, Register src) { movzx_b(dst, Operand(src)); }
  void movzx_b(Register dst, Operand src);

  void movzx_w(Register dst, Register src) { movzx_w(dst, Operand(src)); }
  void movzx_w(Register dst, Operand src);

  // Conditional moves
  void cmov(Condition cc, Register dst, Register src) {
    cmov(cc, dst, Operand(src));
  }
  void cmov(Condition cc, Register dst, Operand src);

  // Flag management.
  void cld();

  // Repetitive string instructions.
  void rep_movs();
  void rep_stos();
  void stos();

  // Exchange
  void xchg(Register dst, Register src);
  void xchg(Register dst, Operand src);
  void xchg_b(Register reg, Operand op);
  void xchg_w(Register reg, Operand op);

  // Lock prefix
  void lock();

  // CompareExchange
  void cmpxchg(Operand dst, Register src);
  void cmpxchg_b(Operand dst, Register src);
  void cmpxchg_w(Operand dst, Register src);

  // Memory Fence
  void lfence();

  void pause();

  // Arithmetics
  void adc(Register dst, int32_t imm32);
  void adc(Register dst, Operand src);

  void add(Register dst, Register src) { add(dst, Operand(src)); }
  void add(Register dst, Operand src);
  void add(Operand dst, Register src);
  void add(Register dst, const Immediate& imm) { add(Operand(dst), imm); }
  void add(Operand dst, const Immediate& x);

  void and_(Register dst, int32_t imm32);
  void and_(Register dst, const Immediate& x);
  void and_(Register dst, Register src) { and_(dst, Operand(src)); }
  void and_(Register dst, Operand src);
  void and_(Operand dst, Register src);
  void and_(Operand dst, const Immediate& x);

  void cmpb(Register reg, Immediate imm8) { cmpb(Operand(reg), imm8); }
  void cmpb(Operand op, Immediate imm8);
  void cmpb(Register reg, Operand op);
  void cmpb(Operand op, Register reg);
  void cmpb(Register dst, Register src) { cmpb(Operand(dst), src); }
  void cmpb_al(Operand op);
  void cmpw_ax(Operand op);
  void cmpw(Operand dst, Immediate src);
  void cmpw(Register dst, Immediate src) { cmpw(Operand(dst), src); }
  void cmpw(Register dst, Operand src);
  void cmpw(Register dst, Register src) { cmpw(Operand(dst), src); }
  void cmpw(Operand dst, Register src);
  void cmp(Register reg, int32_t imm32);
  void cmp(Register reg, Handle<HeapObject> handle);
  void cmp(Register reg0, Register reg1) { cmp(reg0, Operand(reg1)); }
  void cmp(Register reg, Operand op);
  void cmp(Register reg, const Immediate& imm) { cmp(Operand(reg), imm); }
  void cmp(Operand op, Register reg);
  void cmp(Operand op, const Immediate& imm);
  void cmp(Operand op, Handle<HeapObject> handle);

  void dec_b(Register dst);
  void dec_b(Operand dst);

  void dec(Register dst);
  void dec(Operand dst);

  void cdq();

  void idiv(Register src) { idiv(Operand(src)); }
  void idiv(Operand src);
  void div(Register src) { div(Operand(src)); }
  void div(Operand src);

  // Signed multiply instructions.
  void imul(Register src);                               // edx:eax = eax * src.
  void imul(Register dst, Register src) { imul(dst, Operand(src)); }
  void imul(Register dst, Operand src);                  // dst = dst * src.
  void imul(Register dst, Register src, int32_t imm32);  // dst = src * imm32.
  void imul(Register dst, Operand src, int32_t imm32);

  void inc(Register dst);
  void inc(Operand dst);

  void lea(Register dst, Operand src);

  // Unsigned multiply instruction.
  void mul(Register src);                                // edx:eax = eax * reg.

  void neg(Register dst);
  void neg(Operand dst);

  void not_(Register dst);
  void not_(Operand dst);

  void or_(Register dst, int32_t imm32);
  void or_(Register dst, Register src) { or_(dst, Operand(src)); }
  void or_(Register dst, Operand src);
  void or_(Operand dst, Register src);
  void or_(Register dst, const Immediate& imm) { or_(Operand(dst), imm); }
  void or_(Operand dst, const Immediate& x);

  void rcl(Register dst, uint8_t imm8);
  void rcr(Register dst, uint8_t imm8);

  void ror(Register dst, uint8_t imm8) { ror(Operand(dst), imm8); }
  void ror(Operand dst, uint8_t imm8);
  void ror_cl(Register dst) { ror_cl(Operand(dst)); }
  void ror_cl(Operand dst);

  void sar(Register dst, uint8_t imm8) { sar(Operand(dst), imm8); }
  void sar(Operand dst, uint8_t imm8);
  void sar_cl(Register dst) { sar_cl(Operand(dst)); }
  void sar_cl(Operand dst);

  void sbb(Register dst, Operand src);

  void shl(Register dst, uint8_t imm8) { shl(Operand(dst), imm8); }
  void shl(Operand dst, uint8_t imm8);
  void shl_cl(Register dst) { shl_cl(Operand(dst)); }
  void shl_cl(Operand dst);
  void shld(Register dst, Register src, uint8_t shift);
  void shld_cl(Register dst, Register src);

  void shr(Register dst, uint8_t imm8) { shr(Operand(dst), imm8); }
  void shr(Operand dst, uint8_t imm8);
  void shr_cl(Register dst) { shr_cl(Operand(dst)); }
  void shr_cl(Operand dst);
  void shrd(Register dst, Register src, uint8_t shift);
  void shrd_cl(Register dst, Register src) { shrd_cl(Operand(dst), src); }
  void shrd_cl(Operand dst, Register src);

  void sub(Register dst, const Immediate& imm) { sub(Operand(dst), imm); }
  void sub(Operand dst, const Immediate& x);
  void sub(Register dst, Register src) { sub(dst, Operand(src)); }
  void sub(Register dst, Operand src);
  void sub(Operand dst, Register src);
  void sub_sp_32(uint32_t imm);

  void test(Register reg, const Immediate& imm);
  void test(Register reg0, Register reg1) { test(reg0, Operand(reg1)); }
  void test(Register reg, Operand op);
  void test(Operand op, const Immediate& imm);
  void test(Operand op, Register reg) { test(reg, op); }
  void test_b(Register reg, Operand op);
  void test_b(Register reg, Immediate imm8);
  void test_b(Operand op, Immediate imm8);
  void test_b(Operand op, Register reg) { test_b(reg, op); }
  void test_b(Register dst, Register src) { test_b(dst, Operand(src)); }
  void test_w(Register reg, Operand op);
  void test_w(Register reg, Immediate imm16);
  void test_w(Operand op, Immediate imm16);
  void test_w(Operand op, Register reg) { test_w(reg, op); }
  void test_w(Register dst, Register src) { test_w(dst, Operand(src)); }

  void xor_(Register dst, int32_t imm32);
  void xor_(Register dst, Register src) { xor_(dst, Operand(src)); }
  void xor_(Register dst, Operand src);
  void xor_(Operand dst, Register src);
  void xor_(Register dst, const Immediate& imm) { xor_(Operand(dst), imm); }
  void xor_(Operand dst, const Immediate& x);

  // Bit operations.
  void bt(Operand dst, Register src);
  void bts(Register dst, Register src) { bts(Operand(dst), src); }
  void bts(Operand dst, Register src);
  void bsr(Register dst, Register src) { bsr(dst, Operand(src)); }
  void bsr(Register dst, Operand src);
  void bsf(Register dst, Register src) { bsf(dst, Operand(src)); }
  void bsf(Register dst, Operand src);

  // Miscellaneous
  void hlt();
  void int3();
  void nop();
  void ret(int imm16);
  void ud2();

  // Label operations & relative jumps (PPUM Appendix D)
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

  void bind(Label* L);  // binds an unbound label L to the current code position

  // Calls
  void call(Label* L);
  void call(byte* entry, RelocInfo::Mode rmode);
  int CallSize(Operand adr);
  void call(Register reg) { call(Operand(reg)); }
  void call(Operand adr);
  int CallSize(Handle<Code> code, RelocInfo::Mode mode);
  void call(Handle<Code> code, RelocInfo::Mode rmode);
  void call(CodeStub* stub);
  void wasm_call(Address address, RelocInfo::Mode rmode);

  // Jumps
  // unconditional jump to L
  void jmp(Label* L, Label::Distance distance = Label::kFar);
  void jmp(byte* entry, RelocInfo::Mode rmode);
  void jmp(Register reg) { jmp(Operand(reg)); }
  void jmp(Operand adr);
  void jmp(Handle<Code> code, RelocInfo::Mode rmode);

  // Conditional jumps
  void j(Condition cc,
         Label* L,
         Label::Distance distance = Label::kFar);
  void j(Condition cc, byte* entry, RelocInfo::Mode rmode);
  void j(Condition cc, Handle<Code> code,
         RelocInfo::Mode rmode = RelocInfo::CODE_TARGET);

  // Floating-point operations
  void fld(int i);
  void fstp(int i);

  void fld1();
  void fldz();
  void fldpi();
  void fldln2();

  void fld_s(Operand adr);
  void fld_d(Operand adr);

  void fstp_s(Operand adr);
  void fst_s(Operand adr);
  void fstp_d(Operand adr);
  void fst_d(Operand adr);

  void fild_s(Operand adr);
  void fild_d(Operand adr);

  void fist_s(Operand adr);

  void fistp_s(Operand adr);
  void fistp_d(Operand adr);

  // The fisttp instructions require SSE3.
  void fisttp_s(Operand adr);
  void fisttp_d(Operand adr);

  void fabs();
  void fchs();
  void fcos();
  void fsin();
  void fptan();
  void fyl2x();
  void f2xm1();
  void fscale();
  void fninit();

  void fadd(int i);
  void fadd_i(int i);
  void fsub(int i);
  void fsub_i(int i);
  void fmul(int i);
  void fmul_i(int i);
  void fdiv(int i);
  void fdiv_i(int i);

  void fisub_s(Operand adr);

  void faddp(int i = 1);
  void fsubp(int i = 1);
  void fsubrp(int i = 1);
  void fmulp(int i = 1);
  void fdivp(int i = 1);
  void fprem();
  void fprem1();

  void fxch(int i = 1);
  void fincstp();
  void ffree(int i = 0);

  void ftst();
  void fucomp(int i);
  void fucompp();
  void fucomi(int i);
  void fucomip();
  void fcompp();
  void fnstsw_ax();
  void fwait();
  void fnclex();

  void frndint();

  void sahf();
  void setcc(Condition cc, Register reg);

  void cpuid();

  // SSE instructions
  void addss(XMMRegister dst, XMMRegister src) { addss(dst, Operand(src)); }
  void addss(XMMRegister dst, Operand src);
  void subss(XMMRegister dst, XMMRegister src) { subss(dst, Operand(src)); }
  void subss(XMMRegister dst, Operand src);
  void mulss(XMMRegister dst, XMMRegister src) { mulss(dst, Operand(src)); }
  void mulss(XMMRegister dst, Operand src);
  void divss(XMMRegister dst, XMMRegister src) { divss(dst, Operand(src)); }
  void divss(XMMRegister dst, Operand src);
  void sqrtss(XMMRegister dst, XMMRegister src) { sqrtss(dst, Operand(src)); }
  void sqrtss(XMMRegister dst, Operand src);

  void ucomiss(XMMRegister dst, XMMRegister src) { ucomiss(dst, Operand(src)); }
  void ucomiss(XMMRegister dst, Operand src);
  void movaps(XMMRegister dst, XMMRegister src);
  void movups(XMMRegister dst, XMMRegister src);
  void movups(XMMRegister dst, Operand src);
  void movups(Operand dst, XMMRegister src);
  void shufps(XMMRegister dst, XMMRegister src, byte imm8);

  void maxss(XMMRegister dst, XMMRegister src) { maxss(dst, Operand(src)); }
  void maxss(XMMRegister dst, Operand src);
  void minss(XMMRegister dst, XMMRegister src) { minss(dst, Operand(src)); }
  void minss(XMMRegister dst, Operand src);

  void andps(XMMRegister dst, Operand src);
  void andps(XMMRegister dst, XMMRegister src) { andps(dst, Operand(src)); }
  void xorps(XMMRegister dst, Operand src);
  void xorps(XMMRegister dst, XMMRegister src) { xorps(dst, Operand(src)); }
  void orps(XMMRegister dst, Operand src);
  void orps(XMMRegister dst, XMMRegister src) { orps(dst, Operand(src)); }

  void addps(XMMRegister dst, Operand src);
  void addps(XMMRegister dst, XMMRegister src) { addps(dst, Operand(src)); }
  void subps(XMMRegister dst, Operand src);
  void subps(XMMRegister dst, XMMRegister src) { subps(dst, Operand(src)); }
  void mulps(XMMRegister dst, Operand src);
  void mulps(XMMRegister dst, XMMRegister src) { mulps(dst, Operand(src)); }
  void divps(XMMRegister dst, Operand src);
  void divps(XMMRegister dst, XMMRegister src) { divps(dst, Operand(src)); }
  void rcpps(XMMRegister dst, Operand src);
  void rcpps(XMMRegister dst, XMMRegister src) { rcpps(dst, Operand(src)); }
  void rsqrtps(XMMRegister dst, Operand src);
  void rsqrtps(XMMRegister dst, XMMRegister src) { rsqrtps(dst, Operand(src)); }
  void haddps(XMMRegister dst, Operand src);
  void haddps(XMMRegister dst, XMMRegister src) { haddps(dst, Operand(src)); }

  void minps(XMMRegister dst, Operand src);
  void minps(XMMRegister dst, XMMRegister src) { minps(dst, Operand(src)); }
  void maxps(XMMRegister dst, Operand src);
  void maxps(XMMRegister dst, XMMRegister src) { maxps(dst, Operand(src)); }

  void cmpps(XMMRegister dst, Operand src, int8_t cmp);
#define SSE_CMP_P(instr, imm8)                       \
  void instr##ps(XMMRegister dst, XMMRegister src) { \
    cmpps(dst, Operand(src), imm8);                  \
  }                                                  \
  void instr##ps(XMMRegister dst, Operand src) { cmpps(dst, src, imm8); }

  SSE_CMP_P(cmpeq, 0x0);
  SSE_CMP_P(cmplt, 0x1);
  SSE_CMP_P(cmple, 0x2);
  SSE_CMP_P(cmpneq, 0x4);

#undef SSE_CMP_P

  // SSE2 instructions
  void cvttss2si(Register dst, Operand src);
  void cvttss2si(Register dst, XMMRegister src) {
    cvttss2si(dst, Operand(src));
  }
  void cvttsd2si(Register dst, Operand src);
  void cvttsd2si(Register dst, XMMRegister src) {
    cvttsd2si(dst, Operand(src));
  }
  void cvtsd2si(Register dst, XMMRegister src);

  void cvtsi2ss(XMMRegister dst, Register src) { cvtsi2ss(dst, Operand(src)); }
  void cvtsi2ss(XMMRegister dst, Operand src);
  void cvtsi2sd(XMMRegister dst, Register src) { cvtsi2sd(dst, Operand(src)); }
  void cvtsi2sd(XMMRegister dst, Operand src);
  void cvtss2sd(XMMRegister dst, Operand src);
  void cvtss2sd(XMMRegister dst, XMMRegister src) {
    cvtss2sd(dst, Operand(src));
  }
  void cvtsd2ss(XMMRegister dst, Operand src);
  void cvtsd2ss(XMMRegister dst, XMMRegister src) {
    cvtsd2ss(dst, Operand(src));
  }
  void cvtdq2ps(XMMRegister dst, XMMRegister src) {
    cvtdq2ps(dst, Operand(src));
  }
  void cvtdq2ps(XMMRegister dst, Operand src);
  void cvttps2dq(XMMRegister dst, XMMRegister src) {
    cvttps2dq(dst, Operand(src));
  }
  void cvttps2dq(XMMRegister dst, Operand src);

  void addsd(XMMRegister dst, XMMRegister src) { addsd(dst, Operand(src)); }
  void addsd(XMMRegister dst, Operand src);
  void subsd(XMMRegister dst, XMMRegister src) { subsd(dst, Operand(src)); }
  void subsd(XMMRegister dst, Operand src);
  void mulsd(XMMRegister dst, XMMRegister src) { mulsd(dst, Operand(src)); }
  void mulsd(XMMRegister dst, Operand src);
  void divsd(XMMRegister dst, XMMRegister src) { divsd(dst, Operand(src)); }
  void divsd(XMMRegister dst, Operand src);
  void xorpd(XMMRegister dst, XMMRegister src) { xorpd(dst, Operand(src)); }
  void xorpd(XMMRegister dst, Operand src);
  void sqrtsd(XMMRegister dst, XMMRegister src) { sqrtsd(dst, Operand(src)); }
  void sqrtsd(XMMRegister dst, Operand src);

  void andpd(XMMRegister dst, XMMRegister src);
  void orpd(XMMRegister dst, XMMRegister src);

  void ucomisd(XMMRegister dst, XMMRegister src) { ucomisd(dst, Operand(src)); }
  void ucomisd(XMMRegister dst, Operand src);

  void roundss(XMMRegister dst, XMMRegister src, RoundingMode mode);
  void roundsd(XMMRegister dst, XMMRegister src, RoundingMode mode);

  void movmskpd(Register dst, XMMRegister src);
  void movmskps(Register dst, XMMRegister src);

  void cmpltsd(XMMRegister dst, XMMRegister src);

  void maxsd(XMMRegister dst, XMMRegister src) { maxsd(dst, Operand(src)); }
  void maxsd(XMMRegister dst, Operand src);
  void minsd(XMMRegister dst, XMMRegister src) { minsd(dst, Operand(src)); }
  void minsd(XMMRegister dst, Operand src);

  void movdqa(XMMRegister dst, Operand src);
  void movdqa(Operand dst, XMMRegister src);
  void movdqu(XMMRegister dst, Operand src);
  void movdqu(Operand dst, XMMRegister src);
  void movdq(bool aligned, XMMRegister dst, Operand src) {
    if (aligned) {
      movdqa(dst, src);
    } else {
      movdqu(dst, src);
    }
  }

  void movd(XMMRegister dst, Register src) { movd(dst, Operand(src)); }
  void movd(XMMRegister dst, Operand src);
  void movd(Register dst, XMMRegister src) { movd(Operand(dst), src); }
  void movd(Operand dst, XMMRegister src);
  void movsd(XMMRegister dst, XMMRegister src) { movsd(dst, Operand(src)); }
  void movsd(XMMRegister dst, Operand src);
  void movsd(Operand dst, XMMRegister src);

  void movss(XMMRegister dst, Operand src);
  void movss(Operand dst, XMMRegister src);
  void movss(XMMRegister dst, XMMRegister src) { movss(dst, Operand(src)); }
  void extractps(Register dst, XMMRegister src, byte imm8);

  void ptest(XMMRegister dst, XMMRegister src);

  void psllw(XMMRegister reg, int8_t shift);
  void pslld(XMMRegister reg, int8_t shift);
  void psrlw(XMMRegister reg, int8_t shift);
  void psrld(XMMRegister reg, int8_t shift);
  void psraw(XMMRegister reg, int8_t shift);
  void psrad(XMMRegister reg, int8_t shift);
  void psllq(XMMRegister reg, int8_t shift);
  void psllq(XMMRegister dst, XMMRegister src);
  void psrlq(XMMRegister reg, int8_t shift);
  void psrlq(XMMRegister dst, XMMRegister src);

  void pshuflw(XMMRegister dst, XMMRegister src, uint8_t shuffle) {
    pshuflw(dst, Operand(src), shuffle);
  }
  void pshuflw(XMMRegister dst, Operand src, uint8_t shuffle);
  void pshufd(XMMRegister dst, XMMRegister src, uint8_t shuffle) {
    pshufd(dst, Operand(src), shuffle);
  }
  void pshufd(XMMRegister dst, Operand src, uint8_t shuffle);

  void pextrb(Register dst, XMMRegister src, int8_t offset) {
    pextrb(Operand(dst), src, offset);
  }
  void pextrb(Operand dst, XMMRegister src, int8_t offset);
  // Use SSE4_1 encoding for pextrw reg, xmm, imm8 for consistency
  void pextrw(Register dst, XMMRegister src, int8_t offset) {
    pextrw(Operand(dst), src, offset);
  }
  void pextrw(Operand dst, XMMRegister src, int8_t offset);
  void pextrd(Register dst, XMMRegister src, int8_t offset) {
    pextrd(Operand(dst), src, offset);
  }
  void pextrd(Operand dst, XMMRegister src, int8_t offset);

  void insertps(XMMRegister dst, XMMRegister src, int8_t offset) {
    insertps(dst, Operand(src), offset);
  }
  void insertps(XMMRegister dst, Operand src, int8_t offset);
  void pinsrb(XMMRegister dst, Register src, int8_t offset) {
    pinsrb(dst, Operand(src), offset);
  }
  void pinsrb(XMMRegister dst, Operand src, int8_t offset);
  void pinsrw(XMMRegister dst, Register src, int8_t offset) {
    pinsrw(dst, Operand(src), offset);
  }
  void pinsrw(XMMRegister dst, Operand src, int8_t offset);
  void pinsrd(XMMRegister dst, Register src, int8_t offset) {
    pinsrd(dst, Operand(src), offset);
  }
  void pinsrd(XMMRegister dst, Operand src, int8_t offset);

  // AVX instructions
  void vfmadd132sd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmadd132sd(dst, src1, Operand(src2));
  }
  void vfmadd213sd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmadd213sd(dst, src1, Operand(src2));
  }
  void vfmadd231sd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmadd231sd(dst, src1, Operand(src2));
  }
  void vfmadd132sd(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmasd(0x99, dst, src1, src2);
  }
  void vfmadd213sd(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmasd(0xa9, dst, src1, src2);
  }
  void vfmadd231sd(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmasd(0xb9, dst, src1, src2);
  }
  void vfmsub132sd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmsub132sd(dst, src1, Operand(src2));
  }
  void vfmsub213sd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmsub213sd(dst, src1, Operand(src2));
  }
  void vfmsub231sd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmsub231sd(dst, src1, Operand(src2));
  }
  void vfmsub132sd(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmasd(0x9b, dst, src1, src2);
  }
  void vfmsub213sd(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmasd(0xab, dst, src1, src2);
  }
  void vfmsub231sd(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmasd(0xbb, dst, src1, src2);
  }
  void vfnmadd132sd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfnmadd132sd(dst, src1, Operand(src2));
  }
  void vfnmadd213sd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfnmadd213sd(dst, src1, Operand(src2));
  }
  void vfnmadd231sd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfnmadd231sd(dst, src1, Operand(src2));
  }
  void vfnmadd132sd(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmasd(0x9d, dst, src1, src2);
  }
  void vfnmadd213sd(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmasd(0xad, dst, src1, src2);
  }
  void vfnmadd231sd(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmasd(0xbd, dst, src1, src2);
  }
  void vfnmsub132sd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfnmsub132sd(dst, src1, Operand(src2));
  }
  void vfnmsub213sd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfnmsub213sd(dst, src1, Operand(src2));
  }
  void vfnmsub231sd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfnmsub231sd(dst, src1, Operand(src2));
  }
  void vfnmsub132sd(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmasd(0x9f, dst, src1, src2);
  }
  void vfnmsub213sd(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmasd(0xaf, dst, src1, src2);
  }
  void vfnmsub231sd(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmasd(0xbf, dst, src1, src2);
  }
  void vfmasd(byte op, XMMRegister dst, XMMRegister src1, Operand src2);

  void vfmadd132ss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmadd132ss(dst, src1, Operand(src2));
  }
  void vfmadd213ss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmadd213ss(dst, src1, Operand(src2));
  }
  void vfmadd231ss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmadd231ss(dst, src1, Operand(src2));
  }
  void vfmadd132ss(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmass(0x99, dst, src1, src2);
  }
  void vfmadd213ss(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmass(0xa9, dst, src1, src2);
  }
  void vfmadd231ss(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmass(0xb9, dst, src1, src2);
  }
  void vfmsub132ss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmsub132ss(dst, src1, Operand(src2));
  }
  void vfmsub213ss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmsub213ss(dst, src1, Operand(src2));
  }
  void vfmsub231ss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmsub231ss(dst, src1, Operand(src2));
  }
  void vfmsub132ss(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmass(0x9b, dst, src1, src2);
  }
  void vfmsub213ss(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmass(0xab, dst, src1, src2);
  }
  void vfmsub231ss(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmass(0xbb, dst, src1, src2);
  }
  void vfnmadd132ss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfnmadd132ss(dst, src1, Operand(src2));
  }
  void vfnmadd213ss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfnmadd213ss(dst, src1, Operand(src2));
  }
  void vfnmadd231ss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfnmadd231ss(dst, src1, Operand(src2));
  }
  void vfnmadd132ss(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmass(0x9d, dst, src1, src2);
  }
  void vfnmadd213ss(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmass(0xad, dst, src1, src2);
  }
  void vfnmadd231ss(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmass(0xbd, dst, src1, src2);
  }
  void vfnmsub132ss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfnmsub132ss(dst, src1, Operand(src2));
  }
  void vfnmsub213ss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfnmsub213ss(dst, src1, Operand(src2));
  }
  void vfnmsub231ss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfnmsub231ss(dst, src1, Operand(src2));
  }
  void vfnmsub132ss(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmass(0x9f, dst, src1, src2);
  }
  void vfnmsub213ss(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmass(0xaf, dst, src1, src2);
  }
  void vfnmsub231ss(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmass(0xbf, dst, src1, src2);
  }
  void vfmass(byte op, XMMRegister dst, XMMRegister src1, Operand src2);

  void vaddsd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vaddsd(dst, src1, Operand(src2));
  }
  void vaddsd(XMMRegister dst, XMMRegister src1, Operand src2) {
    vsd(0x58, dst, src1, src2);
  }
  void vsubsd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vsubsd(dst, src1, Operand(src2));
  }
  void vsubsd(XMMRegister dst, XMMRegister src1, Operand src2) {
    vsd(0x5c, dst, src1, src2);
  }
  void vmulsd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vmulsd(dst, src1, Operand(src2));
  }
  void vmulsd(XMMRegister dst, XMMRegister src1, Operand src2) {
    vsd(0x59, dst, src1, src2);
  }
  void vdivsd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vdivsd(dst, src1, Operand(src2));
  }
  void vdivsd(XMMRegister dst, XMMRegister src1, Operand src2) {
    vsd(0x5e, dst, src1, src2);
  }
  void vmaxsd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vmaxsd(dst, src1, Operand(src2));
  }
  void vmaxsd(XMMRegister dst, XMMRegister src1, Operand src2) {
    vsd(0x5f, dst, src1, src2);
  }
  void vminsd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vminsd(dst, src1, Operand(src2));
  }
  void vminsd(XMMRegister dst, XMMRegister src1, Operand src2) {
    vsd(0x5d, dst, src1, src2);
  }
  void vsd(byte op, XMMRegister dst, XMMRegister src1, Operand src2);

  void vaddss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vaddss(dst, src1, Operand(src2));
  }
  void vaddss(XMMRegister dst, XMMRegister src1, Operand src2) {
    vss(0x58, dst, src1, src2);
  }
  void vsubss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vsubss(dst, src1, Operand(src2));
  }
  void vsubss(XMMRegister dst, XMMRegister src1, Operand src2) {
    vss(0x5c, dst, src1, src2);
  }
  void vmulss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vmulss(dst, src1, Operand(src2));
  }
  void vmulss(XMMRegister dst, XMMRegister src1, Operand src2) {
    vss(0x59, dst, src1, src2);
  }
  void vdivss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vdivss(dst, src1, Operand(src2));
  }
  void vdivss(XMMRegister dst, XMMRegister src1, Operand src2) {
    vss(0x5e, dst, src1, src2);
  }
  void vmaxss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vmaxss(dst, src1, Operand(src2));
  }
  void vmaxss(XMMRegister dst, XMMRegister src1, Operand src2) {
    vss(0x5f, dst, src1, src2);
  }
  void vminss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vminss(dst, src1, Operand(src2));
  }
  void vminss(XMMRegister dst, XMMRegister src1, Operand src2) {
    vss(0x5d, dst, src1, src2);
  }
  void vss(byte op, XMMRegister dst, XMMRegister src1, Operand src2);

  void vrcpps(XMMRegister dst, XMMRegister src) { vrcpps(dst, Operand(src)); }
  void vrcpps(XMMRegister dst, Operand src) {
    vinstr(0x53, dst, xmm0, src, kNone, k0F, kWIG);
  }
  void vrsqrtps(XMMRegister dst, XMMRegister src) {
    vrsqrtps(dst, Operand(src));
  }
  void vrsqrtps(XMMRegister dst, Operand src) {
    vinstr(0x52, dst, xmm0, src, kNone, k0F, kWIG);
  }
  void vhaddps(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vhaddps(dst, src1, Operand(src2));
  }
  void vhaddps(XMMRegister dst, XMMRegister src1, Operand src2) {
    vinstr(0x7C, dst, src1, src2, kF2, k0F, kWIG);
  }
  void vmovaps(XMMRegister dst, XMMRegister src) {
    vps(0x28, dst, xmm0, Operand(src));
  }
  void vshufps(XMMRegister dst, XMMRegister src1, XMMRegister src2, byte imm8) {
    vshufps(dst, src1, Operand(src2), imm8);
  }
  void vshufps(XMMRegister dst, XMMRegister src1, Operand src2, byte imm8);

  void vpsllw(XMMRegister dst, XMMRegister src, int8_t imm8);
  void vpslld(XMMRegister dst, XMMRegister src, int8_t imm8);
  void vpsrlw(XMMRegister dst, XMMRegister src, int8_t imm8);
  void vpsrld(XMMRegister dst, XMMRegister src, int8_t imm8);
  void vpsraw(XMMRegister dst, XMMRegister src, int8_t imm8);
  void vpsrad(XMMRegister dst, XMMRegister src, int8_t imm8);

  void vpshuflw(XMMRegister dst, XMMRegister src, uint8_t shuffle) {
    vpshuflw(dst, Operand(src), shuffle);
  }
  void vpshuflw(XMMRegister dst, Operand src, uint8_t shuffle);
  void vpshufd(XMMRegister dst, XMMRegister src, uint8_t shuffle) {
    vpshufd(dst, Operand(src), shuffle);
  }
  void vpshufd(XMMRegister dst, Operand src, uint8_t shuffle);

  void vpextrb(Register dst, XMMRegister src, int8_t offset) {
    vpextrb(Operand(dst), src, offset);
  }
  void vpextrb(Operand dst, XMMRegister src, int8_t offset);
  void vpextrw(Register dst, XMMRegister src, int8_t offset) {
    vpextrw(Operand(dst), src, offset);
  }
  void vpextrw(Operand dst, XMMRegister src, int8_t offset);
  void vpextrd(Register dst, XMMRegister src, int8_t offset) {
    vpextrd(Operand(dst), src, offset);
  }
  void vpextrd(Operand dst, XMMRegister src, int8_t offset);

  void vinsertps(XMMRegister dst, XMMRegister src1, XMMRegister src2,
                 int8_t offset) {
    vinsertps(dst, src1, Operand(src2), offset);
  }
  void vinsertps(XMMRegister dst, XMMRegister src1, Operand src2,
                 int8_t offset);
  void vpinsrb(XMMRegister dst, XMMRegister src1, Register src2,
               int8_t offset) {
    vpinsrb(dst, src1, Operand(src2), offset);
  }
  void vpinsrb(XMMRegister dst, XMMRegister src1, Operand src2, int8_t offset);
  void vpinsrw(XMMRegister dst, XMMRegister src1, Register src2,
               int8_t offset) {
    vpinsrw(dst, src1, Operand(src2), offset);
  }
  void vpinsrw(XMMRegister dst, XMMRegister src1, Operand src2, int8_t offset);
  void vpinsrd(XMMRegister dst, XMMRegister src1, Register src2,
               int8_t offset) {
    vpinsrd(dst, src1, Operand(src2), offset);
  }
  void vpinsrd(XMMRegister dst, XMMRegister src1, Operand src2, int8_t offset);

  void vcvtdq2ps(XMMRegister dst, XMMRegister src) {
    vcvtdq2ps(dst, Operand(src));
  }
  void vcvtdq2ps(XMMRegister dst, Operand src) {
    vinstr(0x5B, dst, xmm0, src, kNone, k0F, kWIG);
  }
  void vcvttps2dq(XMMRegister dst, XMMRegister src) {
    vcvttps2dq(dst, Operand(src));
  }
  void vcvttps2dq(XMMRegister dst, Operand src) {
    vinstr(0x5B, dst, xmm0, src, kF3, k0F, kWIG);
  }

  void vmovdqu(XMMRegister dst, Operand src) {
    vinstr(0x6F, dst, xmm0, src, kF3, k0F, kWIG);
  }
  void vmovdqu(Operand dst, XMMRegister src) {
    vinstr(0x7F, src, xmm0, dst, kF3, k0F, kWIG);
  }
  void vmovd(XMMRegister dst, Register src) { vmovd(dst, Operand(src)); }
  void vmovd(XMMRegister dst, Operand src) {
    vinstr(0x6E, dst, xmm0, src, k66, k0F, kWIG);
  }
  void vmovd(Register dst, XMMRegister src) { movd(Operand(dst), src); }
  void vmovd(Operand dst, XMMRegister src) {
    vinstr(0x7E, src, xmm0, dst, k66, k0F, kWIG);
  }

  // BMI instruction
  void andn(Register dst, Register src1, Register src2) {
    andn(dst, src1, Operand(src2));
  }
  void andn(Register dst, Register src1, Operand src2) {
    bmi1(0xf2, dst, src1, src2);
  }
  void bextr(Register dst, Register src1, Register src2) {
    bextr(dst, Operand(src1), src2);
  }
  void bextr(Register dst, Operand src1, Register src2) {
    bmi1(0xf7, dst, src2, src1);
  }
  void blsi(Register dst, Register src) { blsi(dst, Operand(src)); }
  void blsi(Register dst, Operand src) { bmi1(0xf3, ebx, dst, src); }
  void blsmsk(Register dst, Register src) { blsmsk(dst, Operand(src)); }
  void blsmsk(Register dst, Operand src) { bmi1(0xf3, edx, dst, src); }
  void blsr(Register dst, Register src) { blsr(dst, Operand(src)); }
  void blsr(Register dst, Operand src) { bmi1(0xf3, ecx, dst, src); }
  void tzcnt(Register dst, Register src) { tzcnt(dst, Operand(src)); }
  void tzcnt(Register dst, Operand src);

  void lzcnt(Register dst, Register src) { lzcnt(dst, Operand(src)); }
  void lzcnt(Register dst, Operand src);

  void popcnt(Register dst, Register src) { popcnt(dst, Operand(src)); }
  void popcnt(Register dst, Operand src);

  void bzhi(Register dst, Register src1, Register src2) {
    bzhi(dst, Operand(src1), src2);
  }
  void bzhi(Register dst, Operand src1, Register src2) {
    bmi2(kNone, 0xf5, dst, src2, src1);
  }
  void mulx(Register dst1, Register dst2, Register src) {
    mulx(dst1, dst2, Operand(src));
  }
  void mulx(Register dst1, Register dst2, Operand src) {
    bmi2(kF2, 0xf6, dst1, dst2, src);
  }
  void pdep(Register dst, Register src1, Register src2) {
    pdep(dst, src1, Operand(src2));
  }
  void pdep(Register dst, Register src1, Operand src2) {
    bmi2(kF2, 0xf5, dst, src1, src2);
  }
  void pext(Register dst, Register src1, Register src2) {
    pext(dst, src1, Operand(src2));
  }
  void pext(Register dst, Register src1, Operand src2) {
    bmi2(kF3, 0xf5, dst, src1, src2);
  }
  void sarx(Register dst, Register src1, Register src2) {
    sarx(dst, Operand(src1), src2);
  }
  void sarx(Register dst, Operand src1, Register src2) {
    bmi2(kF3, 0xf7, dst, src2, src1);
  }
  void shlx(Register dst, Register src1, Register src2) {
    shlx(dst, Operand(src1), src2);
  }
  void shlx(Register dst, Operand src1, Register src2) {
    bmi2(k66, 0xf7, dst, src2, src1);
  }
  void shrx(Register dst, Register src1, Register src2) {
    shrx(dst, Operand(src1), src2);
  }
  void shrx(Register dst, Operand src1, Register src2) {
    bmi2(kF2, 0xf7, dst, src2, src1);
  }
  void rorx(Register dst, Register src, byte imm8) {
    rorx(dst, Operand(src), imm8);
  }
  void rorx(Register dst, Operand src, byte imm8);

#define PACKED_OP_LIST(V) \
  V(and, 0x54)            \
  V(xor, 0x57)            \
  V(add, 0x58)            \
  V(mul, 0x59)            \
  V(sub, 0x5c)            \
  V(min, 0x5d)            \
  V(div, 0x5e)            \
  V(max, 0x5f)

#define AVX_PACKED_OP_DECLARE(name, opcode)                               \
  void v##name##ps(XMMRegister dst, XMMRegister src1, XMMRegister src2) { \
    vps(opcode, dst, src1, Operand(src2));                                \
  }                                                                       \
  void v##name##ps(XMMRegister dst, XMMRegister src1, Operand src2) {     \
    vps(opcode, dst, src1, src2);                                         \
  }                                                                       \
  void v##name##pd(XMMRegister dst, XMMRegister src1, XMMRegister src2) { \
    vpd(opcode, dst, src1, Operand(src2));                                \
  }                                                                       \
  void v##name##pd(XMMRegister dst, XMMRegister src1, Operand src2) {     \
    vpd(opcode, dst, src1, src2);                                         \
  }

  PACKED_OP_LIST(AVX_PACKED_OP_DECLARE);
  void vps(byte op, XMMRegister dst, XMMRegister src1, Operand src2);
  void vpd(byte op, XMMRegister dst, XMMRegister src1, Operand src2);

  void vcmpps(XMMRegister dst, XMMRegister src1, Operand src2, int8_t cmp);
#define AVX_CMP_P(instr, imm8)                                          \
  void instr##ps(XMMRegister dst, XMMRegister src1, XMMRegister src2) { \
    vcmpps(dst, src1, Operand(src2), imm8);                             \
  }                                                                     \
  void instr##ps(XMMRegister dst, XMMRegister src1, Operand src2) {     \
    vcmpps(dst, src1, src2, imm8);                                      \
  }

  AVX_CMP_P(vcmpeq, 0x0);
  AVX_CMP_P(vcmplt, 0x1);
  AVX_CMP_P(vcmple, 0x2);
  AVX_CMP_P(vcmpneq, 0x4);

#undef AVX_CMP_P

// Other SSE and AVX instructions
#define DECLARE_SSE2_INSTRUCTION(instruction, prefix, escape, opcode) \
  void instruction(XMMRegister dst, XMMRegister src) {                \
    instruction(dst, Operand(src));                                   \
  }                                                                   \
  void instruction(XMMRegister dst, Operand src) {                    \
    sse2_instr(dst, src, 0x##prefix, 0x##escape, 0x##opcode);         \
  }

  SSE2_INSTRUCTION_LIST(DECLARE_SSE2_INSTRUCTION)
#undef DECLARE_SSE2_INSTRUCTION

#define DECLARE_SSE2_AVX_INSTRUCTION(instruction, prefix, escape, opcode)    \
  void v##instruction(XMMRegister dst, XMMRegister src1, XMMRegister src2) { \
    v##instruction(dst, src1, Operand(src2));                                \
  }                                                                          \
  void v##instruction(XMMRegister dst, XMMRegister src1, Operand src2) {     \
    vinstr(0x##opcode, dst, src1, src2, k##prefix, k##escape, kW0);          \
  }

  SSE2_INSTRUCTION_LIST(DECLARE_SSE2_AVX_INSTRUCTION)
#undef DECLARE_SSE2_AVX_INSTRUCTION

#define DECLARE_SSSE3_INSTRUCTION(instruction, prefix, escape1, escape2,     \
                                  opcode)                                    \
  void instruction(XMMRegister dst, XMMRegister src) {                       \
    instruction(dst, Operand(src));                                          \
  }                                                                          \
  void instruction(XMMRegister dst, Operand src) {                           \
    ssse3_instr(dst, src, 0x##prefix, 0x##escape1, 0x##escape2, 0x##opcode); \
  }

  SSSE3_INSTRUCTION_LIST(DECLARE_SSSE3_INSTRUCTION)
#undef DECLARE_SSSE3_INSTRUCTION

#define DECLARE_SSE4_INSTRUCTION(instruction, prefix, escape1, escape2,     \
                                 opcode)                                    \
  void instruction(XMMRegister dst, XMMRegister src) {                      \
    instruction(dst, Operand(src));                                         \
  }                                                                         \
  void instruction(XMMRegister dst, Operand src) {                          \
    sse4_instr(dst, src, 0x##prefix, 0x##escape1, 0x##escape2, 0x##opcode); \
  }

  SSE4_INSTRUCTION_LIST(DECLARE_SSE4_INSTRUCTION)
#undef DECLARE_SSE4_INSTRUCTION

#define DECLARE_SSE34_AVX_INSTRUCTION(instruction, prefix, escape1, escape2,  \
                                      opcode)                                 \
  void v##instruction(XMMRegister dst, XMMRegister src1, XMMRegister src2) {  \
    v##instruction(dst, src1, Operand(src2));                                 \
  }                                                                           \
  void v##instruction(XMMRegister dst, XMMRegister src1, Operand src2) {      \
    vinstr(0x##opcode, dst, src1, src2, k##prefix, k##escape1##escape2, kW0); \
  }

  SSSE3_INSTRUCTION_LIST(DECLARE_SSE34_AVX_INSTRUCTION)
  SSE4_INSTRUCTION_LIST(DECLARE_SSE34_AVX_INSTRUCTION)
#undef DECLARE_SSE34_AVX_INSTRUCTION

  // Prefetch src position into cache level.
  // Level 1, 2 or 3 specifies CPU cache level. Level 0 specifies a
  // non-temporal
  void prefetch(Operand src, int level);
  // TODO(lrn): Need SFENCE for movnt?

  // Check the code size generated from label to here.
  int SizeOfCodeGeneratedSince(Label* label) {
    return pc_offset() - label->pos();
  }

  // Use --code-comments to enable.
  void RecordComment(const char* msg);

  // Record a deoptimization reason that can be used by a log or cpu profiler.
  // Use --trace-deopt to enable.
  void RecordDeoptReason(DeoptimizeReason reason, SourcePosition position,
                         int id);

  // Writes a single byte or word of data in the code stream.  Used for
  // inline tables, e.g., jump-tables.
  void db(uint8_t data);
  void dd(uint32_t data);
  void dq(uint64_t data);
  void dp(uintptr_t data) { dd(data); }
  void dd(Label* label);

  // Check if there is less than kGap bytes available in the buffer.
  // If this is the case, we need to grow the buffer before emitting
  // an instruction or relocation information.
  inline bool buffer_overflow() const {
    return pc_ >= reloc_info_writer.pos() - kGap;
  }

  // Get the number of bytes available in the buffer.
  inline int available_space() const { return reloc_info_writer.pos() - pc_; }

  static bool IsNop(Address addr);

  int relocation_writer_size() {
    return (buffer_ + buffer_size_) - reloc_info_writer.pos();
  }

  // Avoid overflows for displacements etc.
  static constexpr int kMaximalBufferSize = 512 * MB;

  byte byte_at(int pos) { return buffer_[pos]; }
  void set_byte_at(int pos, byte value) { buffer_[pos] = value; }

  void PatchConstantPoolAccessInstruction(int pc_offset, int offset,
                                          ConstantPoolEntry::Access access,
                                          ConstantPoolEntry::Type type) {
    // No embedded constant pool support.
    UNREACHABLE();
  }

 protected:
  void emit_sse_operand(XMMRegister reg, Operand adr);
  void emit_sse_operand(XMMRegister dst, XMMRegister src);
  void emit_sse_operand(Register dst, XMMRegister src);
  void emit_sse_operand(XMMRegister dst, Register src);

  byte* addr_at(int pos) { return buffer_ + pos; }


 private:
  uint32_t long_at(int pos)  {
    return *reinterpret_cast<uint32_t*>(addr_at(pos));
  }
  void long_at_put(int pos, uint32_t x)  {
    *reinterpret_cast<uint32_t*>(addr_at(pos)) = x;
  }

  // code emission
  void GrowBuffer();
  inline void emit(uint32_t x);
  inline void emit(Handle<HeapObject> handle);
  inline void emit(uint32_t x, RelocInfo::Mode rmode);
  inline void emit(Handle<Code> code, RelocInfo::Mode rmode);
  inline void emit(const Immediate& x);
  inline void emit_b(Immediate x);
  inline void emit_w(const Immediate& x);
  inline void emit_q(uint64_t x);

  // Emit the code-object-relative offset of the label's position
  inline void emit_code_relative_offset(Label* label);

  // instruction generation
  void emit_arith_b(int op1, int op2, Register dst, int imm8);

  // Emit a basic arithmetic instruction (i.e. first byte of the family is 0x81)
  // with a given destination expression and an immediate operand.  It attempts
  // to use the shortest encoding possible.
  // sel specifies the /n in the modrm byte (see the Intel PRM).
  void emit_arith(int sel, Operand dst, const Immediate& x);

  void emit_operand(Register reg, Operand adr);

  void emit_label(Label* label);

  void emit_farith(int b1, int b2, int i);

  // Emit vex prefix
  enum SIMDPrefix { kNone = 0x0, k66 = 0x1, kF3 = 0x2, kF2 = 0x3 };
  enum VectorLength { kL128 = 0x0, kL256 = 0x4, kLIG = kL128, kLZ = kL128 };
  enum VexW { kW0 = 0x0, kW1 = 0x80, kWIG = kW0 };
  enum LeadingOpcode { k0F = 0x1, k0F38 = 0x2, k0F3A = 0x3 };
  inline void emit_vex_prefix(XMMRegister v, VectorLength l, SIMDPrefix pp,
                              LeadingOpcode m, VexW w);
  inline void emit_vex_prefix(Register v, VectorLength l, SIMDPrefix pp,
                              LeadingOpcode m, VexW w);

  // labels
  void print(const Label* L);
  void bind_to(Label* L, int pos);

  // displacements
  inline Displacement disp_at(Label* L);
  inline void disp_at_put(Label* L, Displacement disp);
  inline void emit_disp(Label* L, Displacement::Type type);
  inline void emit_near_disp(Label* L);

  void sse2_instr(XMMRegister dst, Operand src, byte prefix, byte escape,
                  byte opcode);
  void ssse3_instr(XMMRegister dst, Operand src, byte prefix, byte escape1,
                   byte escape2, byte opcode);
  void sse4_instr(XMMRegister dst, Operand src, byte prefix, byte escape1,
                  byte escape2, byte opcode);
  void vinstr(byte op, XMMRegister dst, XMMRegister src1, Operand src2,
              SIMDPrefix pp, LeadingOpcode m, VexW w);
  // Most BMI instructions are similar.
  void bmi1(byte op, Register reg, Register vreg, Operand rm);
  void bmi2(SIMDPrefix pp, byte op, Register reg, Register vreg, Operand rm);

  // record reloc info for current pc_
  void RecordRelocInfo(RelocInfo::Mode rmode, intptr_t data = 0);

  // record the position of jmp/jcc instruction
  void record_farjmp_position(Label* L, int pos);

  bool is_optimizable_farjmp(int idx);

  friend class EnsureSpace;

  // Internal reference positions, required for (potential) patching in
  // GrowBuffer(); contains only those internal references whose labels
  // are already bound.
  std::deque<int> internal_reference_positions_;

  // code generation
  RelocInfoWriter reloc_info_writer;

  // The following functions help with avoiding allocations of embedded heap
  // objects during the code assembly phase. {RequestHeapObject} records the
  // need for a future heap number allocation or code stub generation. After
  // code assembly, {AllocateAndInstallRequestedHeapObjects} will allocate these
  // objects and place them where they are expected (determined by the pc offset
  // associated with each request). That is, for each request, it will patch the
  // dummy heap object handle that we emitted during code assembly with the
  // actual heap object handle.
  void RequestHeapObject(HeapObjectRequest request);
  void AllocateAndInstallRequestedHeapObjects(Isolate* isolate);

  std::forward_list<HeapObjectRequest> heap_object_requests_;

  // Variables for this instance of assembler
  int farjmp_num_ = 0;
  std::deque<int> farjmp_positions_;
  std::map<Label*, std::vector<int>> label_farjmp_maps_;
};


// Helper class that ensures that there is enough space for generating
// instructions and relocation information.  The constructor makes
// sure that there is enough space and (in debug mode) the destructor
// checks that we did not generate too much.
class EnsureSpace BASE_EMBEDDED {
 public:
  explicit EnsureSpace(Assembler* assembler) : assembler_(assembler) {
    if (assembler_->buffer_overflow()) assembler_->GrowBuffer();
#ifdef DEBUG
    space_before_ = assembler_->available_space();
#endif
  }

#ifdef DEBUG
  ~EnsureSpace() {
    int bytes_generated = space_before_ - assembler_->available_space();
    DCHECK(bytes_generated < assembler_->kGap);
  }
#endif

 private:
  Assembler* assembler_;
#ifdef DEBUG
  int space_before_;
#endif
};

}  // namespace internal
}  // namespace v8

#endif  // V8_IA32_ASSEMBLER_IA32_H_
