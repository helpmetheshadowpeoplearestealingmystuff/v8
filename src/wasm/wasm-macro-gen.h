// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_MACRO_GEN_H_
#define V8_WASM_MACRO_GEN_H_

#include "src/wasm/wasm-opcodes.h"

#define U32_LE(v)                                    \
  static_cast<byte>(v), static_cast<byte>((v) >> 8), \
      static_cast<byte>((v) >> 16), static_cast<byte>((v) >> 24)

#define U16_LE(v) static_cast<byte>(v), static_cast<byte>((v) >> 8)

#define WASM_MODULE_HEADER U32_LE(kWasmMagic), U32_LE(kWasmVersion)

#define SIG_INDEX(v) U16_LE(v)
// TODO(binji): make SIG_INDEX match this.
#define IMPORT_SIG_INDEX(v) U32V_1(v)
#define FUNC_INDEX(v) U32V_1(v)
#define NO_NAME U32V_1(0)
#define NAME_LENGTH(v) U32V_1(v)

#define ZERO_ALIGNMENT 0
#define ZERO_OFFSET 0

#define BR_TARGET(v) U32_LE(v)

#define MASK_7 ((1 << 7) - 1)
#define MASK_14 ((1 << 14) - 1)
#define MASK_21 ((1 << 21) - 1)
#define MASK_28 ((1 << 28) - 1)

#define U32V_1(x) static_cast<byte>((x)&MASK_7)
#define U32V_2(x) \
  static_cast<byte>(((x)&MASK_7) | 0x80), static_cast<byte>(((x) >> 7) & MASK_7)
#define U32V_3(x)                                      \
  static_cast<byte>((((x)) & MASK_7) | 0x80),          \
      static_cast<byte>((((x) >> 7) & MASK_7) | 0x80), \
      static_cast<byte>(((x) >> 14) & MASK_7)
#define U32V_4(x)                                       \
  static_cast<byte>(((x)&MASK_7) | 0x80),               \
      static_cast<byte>((((x) >> 7) & MASK_7) | 0x80),  \
      static_cast<byte>((((x) >> 14) & MASK_7) | 0x80), \
      static_cast<byte>(((x) >> 21) & MASK_7)
#define U32V_5(x)                                       \
  static_cast<byte>(((x)&MASK_7) | 0x80),               \
      static_cast<byte>((((x) >> 7) & MASK_7) | 0x80),  \
      static_cast<byte>((((x) >> 14) & MASK_7) | 0x80), \
      static_cast<byte>((((x) >> 21) & MASK_7) | 0x80), \
      static_cast<byte>((((x) >> 28) & MASK_7))

// Convenience macros for building Wasm bytecode directly into a byte array.

//------------------------------------------------------------------------------
// Control.
//------------------------------------------------------------------------------
#define WASM_NOP kExprNop

#define WASM_BLOCK(count, ...) kExprBlock, static_cast<byte>(count), __VA_ARGS__
#define WASM_INFINITE_LOOP kExprLoop, 1, kExprBr, 0, kExprNop
#define WASM_LOOP(count, ...) kExprLoop, static_cast<byte>(count), __VA_ARGS__
#define WASM_IF(cond, tstmt) kExprIf, cond, tstmt
#define WASM_IF_ELSE(cond, tstmt, fstmt) kExprIfElse, cond, tstmt, fstmt
#define WASM_SELECT(cond, tval, fval) kExprSelect, cond, tval, fval
#define WASM_BR(depth) kExprBr, static_cast<byte>(depth), kExprNop
#define WASM_BR_IF(depth, cond) \
  kExprBrIf, static_cast<byte>(depth), kExprNop, cond
#define WASM_BRV(depth, val) kExprBr, static_cast<byte>(depth), val
#define WASM_BRV_IF(depth, val, cond) \
  kExprBrIf, static_cast<byte>(depth), val, cond
#define WASM_BREAK(depth) kExprBr, static_cast<byte>(depth + 1), kExprNop
#define WASM_CONTINUE(depth) kExprBr, static_cast<byte>(depth), kExprNop
#define WASM_BREAKV(depth, val) kExprBr, static_cast<byte>(depth + 1), val
#define WASM_RETURN0 kExprReturn
#define WASM_RETURN(...) kExprReturn, __VA_ARGS__
#define WASM_UNREACHABLE kExprUnreachable

#define WASM_BR_TABLE(key, count, ...) \
  kExprBrTable, U32V_1(count), __VA_ARGS__, key

#define WASM_CASE(x) static_cast<byte>(x), static_cast<byte>(x >> 8)
#define WASM_CASE_BR(x) static_cast<byte>(x), static_cast<byte>(0x80 | (x) >> 8)

//------------------------------------------------------------------------------
// Misc expressions.
//------------------------------------------------------------------------------
#define WASM_ID(...) __VA_ARGS__
#define WASM_ZERO kExprI8Const, 0
#define WASM_ONE kExprI8Const, 1
#define WASM_I8(val) kExprI8Const, static_cast<byte>(val)

#define I32V_MIN(length) -(1 << (6 + (7 * ((length) - 1))))
#define I32V_MAX(length) ((1 << (6 + (7 * ((length) - 1)))) - 1)
#define I64V_MIN(length) -(1LL << (6 + (7 * ((length) - 1))))
#define I64V_MAX(length) ((1LL << (6 + 7 * ((length) - 1))) - 1)

#define I32V_IN_RANGE(value, length) \
  ((value) >= I32V_MIN(length) && (value) <= I32V_MAX(length))
#define I64V_IN_RANGE(value, length) \
  ((value) >= I64V_MIN(length) && (value) <= I64V_MAX(length))

#define WASM_NO_LOCALS 0

namespace v8 {
namespace internal {
namespace wasm {

inline void CheckI32v(int32_t value, int length) {
  DCHECK(length >= 1 && length <= 5);
  DCHECK(length == 5 || I32V_IN_RANGE(value, length));
}

inline void CheckI64v(int64_t value, int length) {
  DCHECK(length >= 1 && length <= 10);
  DCHECK(length == 10 || I64V_IN_RANGE(value, length));
}

// A helper for encoding local declarations prepended to the body of a
// function.
class LocalDeclEncoder {
 public:
  // Prepend local declarations by creating a new buffer and copying data
  // over. The new buffer must be delete[]'d by the caller.
  void Prepend(const byte** start, const byte** end) const {
    size_t size = (*end - *start);
    byte* buffer = new byte[Size() + size];
    size_t pos = Emit(buffer);
    memcpy(buffer + pos, *start, size);
    pos += size;
    *start = buffer;
    *end = buffer + pos;
  }

  size_t Emit(byte* buffer) const {
    size_t pos = 0;
    pos = WriteUint32v(buffer, pos, static_cast<uint32_t>(local_decls.size()));
    for (size_t i = 0; i < local_decls.size(); i++) {
      pos = WriteUint32v(buffer, pos, local_decls[i].first);
      buffer[pos++] = WasmOpcodes::LocalTypeCodeFor(local_decls[i].second);
    }
    DCHECK_EQ(Size(), pos);
    return pos;
  }

  // Add locals declarations to this helper. Return the index of the newly added
  // local(s), with an optional adjustment for the parameters.
  uint32_t AddLocals(uint32_t count, LocalType type,
                     FunctionSig* sig = nullptr) {
    if (count == 0) {
      return static_cast<uint32_t>((sig ? sig->parameter_count() : 0) +
                                   local_decls.size());
    }
    size_t pos = local_decls.size();
    if (local_decls.size() > 0 && local_decls.back().second == type) {
      count += local_decls.back().first;
      local_decls.pop_back();
    }
    local_decls.push_back(std::pair<uint32_t, LocalType>(count, type));
    return static_cast<uint32_t>(pos + (sig ? sig->parameter_count() : 0));
  }

  size_t Size() const {
    size_t size = SizeofUint32v(static_cast<uint32_t>(local_decls.size()));
    for (auto p : local_decls) size += 1 + SizeofUint32v(p.first);
    return size;
  }

 private:
  std::vector<std::pair<uint32_t, LocalType>> local_decls;

  size_t SizeofUint32v(uint32_t val) const {
    size_t size = 1;
    while (true) {
      byte b = val & MASK_7;
      if (b == val) return size;
      size++;
      val = val >> 7;
    }
  }

  // TODO(titzer): lift encoding of u32v to a common place.
  size_t WriteUint32v(byte* buffer, size_t pos, uint32_t val) const {
    while (true) {
      byte b = val & MASK_7;
      if (b == val) {
        buffer[pos++] = b;
        break;
      }
      buffer[pos++] = 0x80 | b;
      val = val >> 7;
    }
    return pos;
  }
};
}  // namespace wasm
}  // namespace internal
}  // namespace v8

//------------------------------------------------------------------------------
// Int32 Const operations
//------------------------------------------------------------------------------
#define WASM_I32V(val) kExprI32Const, U32V_5(val)

#define WASM_I32V_1(val) \
  static_cast<byte>(CheckI32v((val), 1), kExprI32Const), U32V_1(val)
#define WASM_I32V_2(val) \
  static_cast<byte>(CheckI32v((val), 2), kExprI32Const), U32V_2(val)
#define WASM_I32V_3(val) \
  static_cast<byte>(CheckI32v((val), 3), kExprI32Const), U32V_3(val)
#define WASM_I32V_4(val) \
  static_cast<byte>(CheckI32v((val), 4), kExprI32Const), U32V_4(val)
#define WASM_I32V_5(val) \
  static_cast<byte>(CheckI32v((val), 5), kExprI32Const), U32V_5(val)

//------------------------------------------------------------------------------
// Int64 Const operations
//------------------------------------------------------------------------------
#define WASM_I64V(val)                                                        \
  kExprI64Const,                                                              \
      static_cast<byte>((static_cast<int64_t>(val) & MASK_7) | 0x80),         \
      static_cast<byte>(((static_cast<int64_t>(val) >> 7) & MASK_7) | 0x80),  \
      static_cast<byte>(((static_cast<int64_t>(val) >> 14) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 21) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 28) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 35) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 42) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 49) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 56) & MASK_7) | 0x80), \
      static_cast<byte>((static_cast<int64_t>(val) >> 63) & MASK_7)

#define WASM_I64V_1(val)                                                     \
  static_cast<byte>(CheckI64v(static_cast<int64_t>(val), 1), kExprI64Const), \
      static_cast<byte>(static_cast<int64_t>(val) & MASK_7)
#define WASM_I64V_2(val)                                                     \
  static_cast<byte>(CheckI64v(static_cast<int64_t>(val), 2), kExprI64Const), \
      static_cast<byte>((static_cast<int64_t>(val) & MASK_7) | 0x80),        \
      static_cast<byte>((static_cast<int64_t>(val) >> 7) & MASK_7)
#define WASM_I64V_3(val)                                                     \
  static_cast<byte>(CheckI64v(static_cast<int64_t>(val), 3), kExprI64Const), \
      static_cast<byte>((static_cast<int64_t>(val) & MASK_7) | 0x80),        \
      static_cast<byte>(((static_cast<int64_t>(val) >> 7) & MASK_7) | 0x80), \
      static_cast<byte>((static_cast<int64_t>(val) >> 14) & MASK_7)
#define WASM_I64V_4(val)                                                      \
  static_cast<byte>(CheckI64v(static_cast<int64_t>(val), 4), kExprI64Const),  \
      static_cast<byte>((static_cast<int64_t>(val) & MASK_7) | 0x80),         \
      static_cast<byte>(((static_cast<int64_t>(val) >> 7) & MASK_7) | 0x80),  \
      static_cast<byte>(((static_cast<int64_t>(val) >> 14) & MASK_7) | 0x80), \
      static_cast<byte>((static_cast<int64_t>(val) >> 21) & MASK_7)
#define WASM_I64V_5(val)                                                      \
  static_cast<byte>(CheckI64v(static_cast<int64_t>(val), 5), kExprI64Const),  \
      static_cast<byte>((static_cast<int64_t>(val) & MASK_7) | 0x80),         \
      static_cast<byte>(((static_cast<int64_t>(val) >> 7) & MASK_7) | 0x80),  \
      static_cast<byte>(((static_cast<int64_t>(val) >> 14) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 21) & MASK_7) | 0x80), \
      static_cast<byte>((static_cast<int64_t>(val) >> 28) & MASK_7)
#define WASM_I64V_6(val)                                                      \
  static_cast<byte>(CheckI64v(static_cast<int64_t>(val), 6), kExprI64Const),  \
      static_cast<byte>((static_cast<int64_t>(val) & MASK_7) | 0x80),         \
      static_cast<byte>(((static_cast<int64_t>(val) >> 7) & MASK_7) | 0x80),  \
      static_cast<byte>(((static_cast<int64_t>(val) >> 14) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 21) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 28) & MASK_7) | 0x80), \
      static_cast<byte>((static_cast<int64_t>(val) >> 35) & MASK_7)
#define WASM_I64V_7(val)                                                      \
  static_cast<byte>(CheckI64v(static_cast<int64_t>(val), 7), kExprI64Const),  \
      static_cast<byte>((static_cast<int64_t>(val) & MASK_7) | 0x80),         \
      static_cast<byte>(((static_cast<int64_t>(val) >> 7) & MASK_7) | 0x80),  \
      static_cast<byte>(((static_cast<int64_t>(val) >> 14) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 21) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 28) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 35) & MASK_7) | 0x80), \
      static_cast<byte>((static_cast<int64_t>(val) >> 42) & MASK_7)
#define WASM_I64V_8(val)                                                      \
  static_cast<byte>(CheckI64v(static_cast<int64_t>(val), 8), kExprI64Const),  \
      static_cast<byte>((static_cast<int64_t>(val) & MASK_7) | 0x80),         \
      static_cast<byte>(((static_cast<int64_t>(val) >> 7) & MASK_7) | 0x80),  \
      static_cast<byte>(((static_cast<int64_t>(val) >> 14) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 21) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 28) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 35) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 42) & MASK_7) | 0x80), \
      static_cast<byte>((static_cast<int64_t>(val) >> 49) & MASK_7)
#define WASM_I64V_9(val)                                                      \
  static_cast<byte>(CheckI64v(static_cast<int64_t>(val), 9), kExprI64Const),  \
      static_cast<byte>((static_cast<int64_t>(val) & MASK_7) | 0x80),         \
      static_cast<byte>(((static_cast<int64_t>(val) >> 7) & MASK_7) | 0x80),  \
      static_cast<byte>(((static_cast<int64_t>(val) >> 14) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 21) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 28) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 35) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 42) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 49) & MASK_7) | 0x80), \
      static_cast<byte>((static_cast<int64_t>(val) >> 56) & MASK_7)
#define WASM_I64V_10(val)                                                     \
  static_cast<byte>(CheckI64v(static_cast<int64_t>(val), 10), kExprI64Const), \
      static_cast<byte>((static_cast<int64_t>(val) & MASK_7) | 0x80),         \
      static_cast<byte>(((static_cast<int64_t>(val) >> 7) & MASK_7) | 0x80),  \
      static_cast<byte>(((static_cast<int64_t>(val) >> 14) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 21) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 28) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 35) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 42) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 49) & MASK_7) | 0x80), \
      static_cast<byte>(((static_cast<int64_t>(val) >> 56) & MASK_7) | 0x80), \
      static_cast<byte>((static_cast<int64_t>(val) >> 63) & MASK_7)

#define WASM_F32(val)                                                       \
  kExprF32Const,                                                            \
      static_cast<byte>(bit_cast<int32_t>(static_cast<float>(val))),        \
      static_cast<byte>(bit_cast<uint32_t>(static_cast<float>(val)) >> 8),  \
      static_cast<byte>(bit_cast<uint32_t>(static_cast<float>(val)) >> 16), \
      static_cast<byte>(bit_cast<uint32_t>(static_cast<float>(val)) >> 24)
#define WASM_F64(val)                                        \
  kExprF64Const, static_cast<byte>(bit_cast<uint64_t>(val)), \
      static_cast<byte>(bit_cast<uint64_t>(val) >> 8),       \
      static_cast<byte>(bit_cast<uint64_t>(val) >> 16),      \
      static_cast<byte>(bit_cast<uint64_t>(val) >> 24),      \
      static_cast<byte>(bit_cast<uint64_t>(val) >> 32),      \
      static_cast<byte>(bit_cast<uint64_t>(val) >> 40),      \
      static_cast<byte>(bit_cast<uint64_t>(val) >> 48),      \
      static_cast<byte>(bit_cast<uint64_t>(val) >> 56)
#define WASM_GET_LOCAL(index) kExprGetLocal, static_cast<byte>(index)
#define WASM_SET_LOCAL(index, val) kExprSetLocal, static_cast<byte>(index), val
#define WASM_LOAD_GLOBAL(index) kExprLoadGlobal, static_cast<byte>(index)
#define WASM_STORE_GLOBAL(index, val) \
  kExprStoreGlobal, static_cast<byte>(index), val
#define WASM_LOAD_MEM(type, index)                                      \
  static_cast<byte>(                                                    \
      v8::internal::wasm::WasmOpcodes::LoadStoreOpcodeOf(type, false)), \
      ZERO_ALIGNMENT, ZERO_OFFSET, index
#define WASM_STORE_MEM(type, index, val)                               \
  static_cast<byte>(                                                   \
      v8::internal::wasm::WasmOpcodes::LoadStoreOpcodeOf(type, true)), \
      ZERO_ALIGNMENT, ZERO_OFFSET, index, val
#define WASM_LOAD_MEM_OFFSET(type, offset, index)                       \
  static_cast<byte>(                                                    \
      v8::internal::wasm::WasmOpcodes::LoadStoreOpcodeOf(type, false)), \
      ZERO_ALIGNMENT, U32V_1(offset), index
#define WASM_STORE_MEM_OFFSET(type, offset, index, val)                \
  static_cast<byte>(                                                   \
      v8::internal::wasm::WasmOpcodes::LoadStoreOpcodeOf(type, true)), \
      ZERO_ALIGNMENT, U32V_1(offset), index, val
#define WASM_CALL_FUNCTION(index, ...) \
  kExprCallFunction, static_cast<byte>(index), __VA_ARGS__
#define WASM_CALL_IMPORT(index, ...) \
  kExprCallImport, static_cast<byte>(index), __VA_ARGS__
#define WASM_CALL_INDIRECT(index, func, ...) \
  kExprCallIndirect, static_cast<byte>(index), func, __VA_ARGS__
#define WASM_CALL_FUNCTION0(index) kExprCallFunction, static_cast<byte>(index)
#define WASM_CALL_IMPORT0(index) kExprCallImport, static_cast<byte>(index)
#define WASM_CALL_INDIRECT0(index, func) \
  kExprCallIndirect, static_cast<byte>(index), func
#define WASM_NOT(x) kExprI32Eqz, x

//------------------------------------------------------------------------------
// Constructs that are composed of multiple bytecodes.
//------------------------------------------------------------------------------
#define WASM_WHILE(x, y) kExprLoop, 1, kExprIf, x, kExprBr, 0, y
#define WASM_INC_LOCAL(index)                                          \
  kExprSetLocal, static_cast<byte>(index), kExprI32Add, kExprGetLocal, \
      static_cast<byte>(index), kExprI8Const, 1
#define WASM_INC_LOCAL_BY(index, count)                                \
  kExprSetLocal, static_cast<byte>(index), kExprI32Add, kExprGetLocal, \
      static_cast<byte>(index), kExprI8Const, static_cast<int8_t>(count)

#define WASM_UNOP(opcode, x) static_cast<byte>(opcode), x
#define WASM_BINOP(opcode, x, y) static_cast<byte>(opcode), x, y

//------------------------------------------------------------------------------
// Int32 operations
//------------------------------------------------------------------------------
#define WASM_I32_ADD(x, y) kExprI32Add, x, y
#define WASM_I32_SUB(x, y) kExprI32Sub, x, y
#define WASM_I32_MUL(x, y) kExprI32Mul, x, y
#define WASM_I32_DIVS(x, y) kExprI32DivS, x, y
#define WASM_I32_DIVU(x, y) kExprI32DivU, x, y
#define WASM_I32_REMS(x, y) kExprI32RemS, x, y
#define WASM_I32_REMU(x, y) kExprI32RemU, x, y
#define WASM_I32_AND(x, y) kExprI32And, x, y
#define WASM_I32_IOR(x, y) kExprI32Ior, x, y
#define WASM_I32_XOR(x, y) kExprI32Xor, x, y
#define WASM_I32_SHL(x, y) kExprI32Shl, x, y
#define WASM_I32_SHR(x, y) kExprI32ShrU, x, y
#define WASM_I32_SAR(x, y) kExprI32ShrS, x, y
#define WASM_I32_ROR(x, y) kExprI32Ror, x, y
#define WASM_I32_ROL(x, y) kExprI32Rol, x, y
#define WASM_I32_EQ(x, y) kExprI32Eq, x, y
#define WASM_I32_NE(x, y) kExprI32Ne, x, y
#define WASM_I32_LTS(x, y) kExprI32LtS, x, y
#define WASM_I32_LES(x, y) kExprI32LeS, x, y
#define WASM_I32_LTU(x, y) kExprI32LtU, x, y
#define WASM_I32_LEU(x, y) kExprI32LeU, x, y
#define WASM_I32_GTS(x, y) kExprI32GtS, x, y
#define WASM_I32_GES(x, y) kExprI32GeS, x, y
#define WASM_I32_GTU(x, y) kExprI32GtU, x, y
#define WASM_I32_GEU(x, y) kExprI32GeU, x, y
#define WASM_I32_CLZ(x) kExprI32Clz, x
#define WASM_I32_CTZ(x) kExprI32Ctz, x
#define WASM_I32_POPCNT(x) kExprI32Popcnt, x

//------------------------------------------------------------------------------
// Int64 operations
//------------------------------------------------------------------------------
#define WASM_I64_ADD(x, y) kExprI64Add, x, y
#define WASM_I64_SUB(x, y) kExprI64Sub, x, y
#define WASM_I64_MUL(x, y) kExprI64Mul, x, y
#define WASM_I64_DIVS(x, y) kExprI64DivS, x, y
#define WASM_I64_DIVU(x, y) kExprI64DivU, x, y
#define WASM_I64_REMS(x, y) kExprI64RemS, x, y
#define WASM_I64_REMU(x, y) kExprI64RemU, x, y
#define WASM_I64_AND(x, y) kExprI64And, x, y
#define WASM_I64_IOR(x, y) kExprI64Ior, x, y
#define WASM_I64_XOR(x, y) kExprI64Xor, x, y
#define WASM_I64_SHL(x, y) kExprI64Shl, x, y
#define WASM_I64_SHR(x, y) kExprI64ShrU, x, y
#define WASM_I64_SAR(x, y) kExprI64ShrS, x, y
#define WASM_I64_ROR(x, y) kExprI64Ror, x, y
#define WASM_I64_ROL(x, y) kExprI64Rol, x, y
#define WASM_I64_EQ(x, y) kExprI64Eq, x, y
#define WASM_I64_NE(x, y) kExprI64Ne, x, y
#define WASM_I64_LTS(x, y) kExprI64LtS, x, y
#define WASM_I64_LES(x, y) kExprI64LeS, x, y
#define WASM_I64_LTU(x, y) kExprI64LtU, x, y
#define WASM_I64_LEU(x, y) kExprI64LeU, x, y
#define WASM_I64_GTS(x, y) kExprI64GtS, x, y
#define WASM_I64_GES(x, y) kExprI64GeS, x, y
#define WASM_I64_GTU(x, y) kExprI64GtU, x, y
#define WASM_I64_GEU(x, y) kExprI64GeU, x, y
#define WASM_I64_CLZ(x) kExprI64Clz, x
#define WASM_I64_CTZ(x) kExprI64Ctz, x
#define WASM_I64_POPCNT(x) kExprI64Popcnt, x

//------------------------------------------------------------------------------
// Float32 operations
//------------------------------------------------------------------------------
#define WASM_F32_ADD(x, y) kExprF32Add, x, y
#define WASM_F32_SUB(x, y) kExprF32Sub, x, y
#define WASM_F32_MUL(x, y) kExprF32Mul, x, y
#define WASM_F32_DIV(x, y) kExprF32Div, x, y
#define WASM_F32_MIN(x, y) kExprF32Min, x, y
#define WASM_F32_MAX(x, y) kExprF32Max, x, y
#define WASM_F32_ABS(x) kExprF32Abs, x
#define WASM_F32_NEG(x) kExprF32Neg, x
#define WASM_F32_COPYSIGN(x, y) kExprF32CopySign, x, y
#define WASM_F32_CEIL(x) kExprF32Ceil, x
#define WASM_F32_FLOOR(x) kExprF32Floor, x
#define WASM_F32_TRUNC(x) kExprF32Trunc, x
#define WASM_F32_NEARESTINT(x) kExprF32NearestInt, x
#define WASM_F32_SQRT(x) kExprF32Sqrt, x
#define WASM_F32_EQ(x, y) kExprF32Eq, x, y
#define WASM_F32_NE(x, y) kExprF32Ne, x, y
#define WASM_F32_LT(x, y) kExprF32Lt, x, y
#define WASM_F32_LE(x, y) kExprF32Le, x, y
#define WASM_F32_GT(x, y) kExprF32Gt, x, y
#define WASM_F32_GE(x, y) kExprF32Ge, x, y

//------------------------------------------------------------------------------
// Float64 operations
//------------------------------------------------------------------------------
#define WASM_F64_ADD(x, y) kExprF64Add, x, y
#define WASM_F64_SUB(x, y) kExprF64Sub, x, y
#define WASM_F64_MUL(x, y) kExprF64Mul, x, y
#define WASM_F64_DIV(x, y) kExprF64Div, x, y
#define WASM_F64_MIN(x, y) kExprF64Min, x, y
#define WASM_F64_MAX(x, y) kExprF64Max, x, y
#define WASM_F64_ABS(x) kExprF64Abs, x
#define WASM_F64_NEG(x) kExprF64Neg, x
#define WASM_F64_COPYSIGN(x, y) kExprF64CopySign, x, y
#define WASM_F64_CEIL(x) kExprF64Ceil, x
#define WASM_F64_FLOOR(x) kExprF64Floor, x
#define WASM_F64_TRUNC(x) kExprF64Trunc, x
#define WASM_F64_NEARESTINT(x) kExprF64NearestInt, x
#define WASM_F64_SQRT(x) kExprF64Sqrt, x
#define WASM_F64_EQ(x, y) kExprF64Eq, x, y
#define WASM_F64_NE(x, y) kExprF64Ne, x, y
#define WASM_F64_LT(x, y) kExprF64Lt, x, y
#define WASM_F64_LE(x, y) kExprF64Le, x, y
#define WASM_F64_GT(x, y) kExprF64Gt, x, y
#define WASM_F64_GE(x, y) kExprF64Ge, x, y

//------------------------------------------------------------------------------
// Type conversions.
//------------------------------------------------------------------------------
#define WASM_I32_SCONVERT_F32(x) kExprI32SConvertF32, x
#define WASM_I32_SCONVERT_F64(x) kExprI32SConvertF64, x
#define WASM_I32_UCONVERT_F32(x) kExprI32UConvertF32, x
#define WASM_I32_UCONVERT_F64(x) kExprI32UConvertF64, x
#define WASM_I32_CONVERT_I64(x) kExprI32ConvertI64, x
#define WASM_I64_SCONVERT_F32(x) kExprI64SConvertF32, x
#define WASM_I64_SCONVERT_F64(x) kExprI64SConvertF64, x
#define WASM_I64_UCONVERT_F32(x) kExprI64UConvertF32, x
#define WASM_I64_UCONVERT_F64(x) kExprI64UConvertF64, x
#define WASM_I64_SCONVERT_I32(x) kExprI64SConvertI32, x
#define WASM_I64_UCONVERT_I32(x) kExprI64UConvertI32, x
#define WASM_F32_SCONVERT_I32(x) kExprF32SConvertI32, x
#define WASM_F32_UCONVERT_I32(x) kExprF32UConvertI32, x
#define WASM_F32_SCONVERT_I64(x) kExprF32SConvertI64, x
#define WASM_F32_UCONVERT_I64(x) kExprF32UConvertI64, x
#define WASM_F32_CONVERT_F64(x) kExprF32ConvertF64, x
#define WASM_F32_REINTERPRET_I32(x) kExprF32ReinterpretI32, x
#define WASM_F64_SCONVERT_I32(x) kExprF64SConvertI32, x
#define WASM_F64_UCONVERT_I32(x) kExprF64UConvertI32, x
#define WASM_F64_SCONVERT_I64(x) kExprF64SConvertI64, x
#define WASM_F64_UCONVERT_I64(x) kExprF64UConvertI64, x
#define WASM_F64_CONVERT_F32(x) kExprF64ConvertF32, x
#define WASM_F64_REINTERPRET_I64(x) kExprF64ReinterpretI64, x
#define WASM_I32_REINTERPRET_F32(x) kExprI32ReinterpretF32, x
#define WASM_I64_REINTERPRET_F64(x) kExprI64ReinterpretF64, x

#endif  // V8_WASM_MACRO_GEN_H_
