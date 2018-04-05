// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_OPCODES_H_
#define V8_WASM_WASM_OPCODES_H_

#include "src/globals.h"
#include "src/machine-type.h"
#include "src/runtime/runtime.h"
#include "src/signature.h"
#include "src/wasm/wasm-constants.h"

namespace v8 {
namespace internal {
namespace wasm {

// We reuse the internal machine type to represent WebAssembly types.
// A typedef improves readability without adding a whole new type system.
using ValueType = MachineRepresentation;
constexpr ValueType kWasmStmt = MachineRepresentation::kNone;
constexpr ValueType kWasmI32 = MachineRepresentation::kWord32;
constexpr ValueType kWasmI64 = MachineRepresentation::kWord64;
constexpr ValueType kWasmF32 = MachineRepresentation::kFloat32;
constexpr ValueType kWasmF64 = MachineRepresentation::kFloat64;
constexpr ValueType kWasmAnyRef = MachineRepresentation::kTaggedPointer;
constexpr ValueType kWasmS128 = MachineRepresentation::kSimd128;
constexpr ValueType kWasmVar = MachineRepresentation::kTagged;

using FunctionSig = Signature<ValueType>;
std::ostream& operator<<(std::ostream& os, const FunctionSig& function);
bool IsJSCompatibleSignature(const FunctionSig* sig);

using WasmName = Vector<const char>;

// Control expressions and blocks.
#define FOREACH_CONTROL_OPCODE(V)         \
  V(Unreachable, 0x00, _)                 \
  V(Nop, 0x01, _)                         \
  V(Block, 0x02, _)                       \
  V(Loop, 0x03, _)                        \
  V(If, 0x004, _)                         \
  V(Else, 0x05, _)                        \
  V(Try, 0x06, _ /* eh_prototype */)      \
  V(Catch, 0x07, _ /* eh_prototype */)    \
  V(Throw, 0x08, _ /* eh_prototype */)    \
  V(Rethrow, 0x09, _ /* eh_prototype */)  \
  V(CatchAll, 0x0a, _ /* eh prototype */) \
  V(End, 0x0b, _)                         \
  V(Br, 0x0c, _)                          \
  V(BrIf, 0x0d, _)                        \
  V(BrTable, 0x0e, _)                     \
  V(Return, 0x0f, _)

// Constants, locals, globals, and calls.
#define FOREACH_MISC_OPCODE(V) \
  V(CallFunction, 0x10, _)     \
  V(CallIndirect, 0x11, _)     \
  V(Drop, 0x1a, _)             \
  V(Select, 0x1b, _)           \
  V(GetLocal, 0x20, _)         \
  V(SetLocal, 0x21, _)         \
  V(TeeLocal, 0x22, _)         \
  V(GetGlobal, 0x23, _)        \
  V(SetGlobal, 0x24, _)        \
  V(I32Const, 0x41, _)         \
  V(I64Const, 0x42, _)         \
  V(F32Const, 0x43, _)         \
  V(F64Const, 0x44, _)         \
  V(RefNull, 0xd0, _)

// Load memory expressions.
#define FOREACH_LOAD_MEM_OPCODE(V) \
  V(I32LoadMem, 0x28, i_i)         \
  V(I64LoadMem, 0x29, l_i)         \
  V(F32LoadMem, 0x2a, f_i)         \
  V(F64LoadMem, 0x2b, d_i)         \
  V(I32LoadMem8S, 0x2c, i_i)       \
  V(I32LoadMem8U, 0x2d, i_i)       \
  V(I32LoadMem16S, 0x2e, i_i)      \
  V(I32LoadMem16U, 0x2f, i_i)      \
  V(I64LoadMem8S, 0x30, l_i)       \
  V(I64LoadMem8U, 0x31, l_i)       \
  V(I64LoadMem16S, 0x32, l_i)      \
  V(I64LoadMem16U, 0x33, l_i)      \
  V(I64LoadMem32S, 0x34, l_i)      \
  V(I64LoadMem32U, 0x35, l_i)

// Store memory expressions.
#define FOREACH_STORE_MEM_OPCODE(V) \
  V(I32StoreMem, 0x36, v_ii)        \
  V(I64StoreMem, 0x37, v_il)        \
  V(F32StoreMem, 0x38, v_if)        \
  V(F64StoreMem, 0x39, v_id)        \
  V(I32StoreMem8, 0x3a, v_ii)       \
  V(I32StoreMem16, 0x3b, v_ii)      \
  V(I64StoreMem8, 0x3c, v_il)       \
  V(I64StoreMem16, 0x3d, v_il)      \
  V(I64StoreMem32, 0x3e, v_il)

// Miscellaneous memory expressions
#define FOREACH_MISC_MEM_OPCODE(V) \
  V(MemorySize, 0x3f, i_v)         \
  V(GrowMemory, 0x40, i_i)

// Expressions with signatures.
#define FOREACH_SIMPLE_OPCODE(V)  \
  V(I32Eqz, 0x45, i_i)            \
  V(I32Eq, 0x46, i_ii)            \
  V(I32Ne, 0x47, i_ii)            \
  V(I32LtS, 0x48, i_ii)           \
  V(I32LtU, 0x49, i_ii)           \
  V(I32GtS, 0x4a, i_ii)           \
  V(I32GtU, 0x4b, i_ii)           \
  V(I32LeS, 0x4c, i_ii)           \
  V(I32LeU, 0x4d, i_ii)           \
  V(I32GeS, 0x4e, i_ii)           \
  V(I32GeU, 0x4f, i_ii)           \
  V(I64Eqz, 0x50, i_l)            \
  V(I64Eq, 0x51, i_ll)            \
  V(I64Ne, 0x52, i_ll)            \
  V(I64LtS, 0x53, i_ll)           \
  V(I64LtU, 0x54, i_ll)           \
  V(I64GtS, 0x55, i_ll)           \
  V(I64GtU, 0x56, i_ll)           \
  V(I64LeS, 0x57, i_ll)           \
  V(I64LeU, 0x58, i_ll)           \
  V(I64GeS, 0x59, i_ll)           \
  V(I64GeU, 0x5a, i_ll)           \
  V(F32Eq, 0x5b, i_ff)            \
  V(F32Ne, 0x5c, i_ff)            \
  V(F32Lt, 0x5d, i_ff)            \
  V(F32Gt, 0x5e, i_ff)            \
  V(F32Le, 0x5f, i_ff)            \
  V(F32Ge, 0x60, i_ff)            \
  V(F64Eq, 0x61, i_dd)            \
  V(F64Ne, 0x62, i_dd)            \
  V(F64Lt, 0x63, i_dd)            \
  V(F64Gt, 0x64, i_dd)            \
  V(F64Le, 0x65, i_dd)            \
  V(F64Ge, 0x66, i_dd)            \
  V(I32Clz, 0x67, i_i)            \
  V(I32Ctz, 0x68, i_i)            \
  V(I32Popcnt, 0x69, i_i)         \
  V(I32Add, 0x6a, i_ii)           \
  V(I32Sub, 0x6b, i_ii)           \
  V(I32Mul, 0x6c, i_ii)           \
  V(I32DivS, 0x6d, i_ii)          \
  V(I32DivU, 0x6e, i_ii)          \
  V(I32RemS, 0x6f, i_ii)          \
  V(I32RemU, 0x70, i_ii)          \
  V(I32And, 0x71, i_ii)           \
  V(I32Ior, 0x72, i_ii)           \
  V(I32Xor, 0x73, i_ii)           \
  V(I32Shl, 0x74, i_ii)           \
  V(I32ShrS, 0x75, i_ii)          \
  V(I32ShrU, 0x76, i_ii)          \
  V(I32Rol, 0x77, i_ii)           \
  V(I32Ror, 0x78, i_ii)           \
  V(I64Clz, 0x79, l_l)            \
  V(I64Ctz, 0x7a, l_l)            \
  V(I64Popcnt, 0x7b, l_l)         \
  V(I64Add, 0x7c, l_ll)           \
  V(I64Sub, 0x7d, l_ll)           \
  V(I64Mul, 0x7e, l_ll)           \
  V(I64DivS, 0x7f, l_ll)          \
  V(I64DivU, 0x80, l_ll)          \
  V(I64RemS, 0x81, l_ll)          \
  V(I64RemU, 0x82, l_ll)          \
  V(I64And, 0x83, l_ll)           \
  V(I64Ior, 0x84, l_ll)           \
  V(I64Xor, 0x85, l_ll)           \
  V(I64Shl, 0x86, l_ll)           \
  V(I64ShrS, 0x87, l_ll)          \
  V(I64ShrU, 0x88, l_ll)          \
  V(I64Rol, 0x89, l_ll)           \
  V(I64Ror, 0x8a, l_ll)           \
  V(F32Abs, 0x8b, f_f)            \
  V(F32Neg, 0x8c, f_f)            \
  V(F32Ceil, 0x8d, f_f)           \
  V(F32Floor, 0x8e, f_f)          \
  V(F32Trunc, 0x8f, f_f)          \
  V(F32NearestInt, 0x90, f_f)     \
  V(F32Sqrt, 0x91, f_f)           \
  V(F32Add, 0x92, f_ff)           \
  V(F32Sub, 0x93, f_ff)           \
  V(F32Mul, 0x94, f_ff)           \
  V(F32Div, 0x95, f_ff)           \
  V(F32Min, 0x96, f_ff)           \
  V(F32Max, 0x97, f_ff)           \
  V(F32CopySign, 0x98, f_ff)      \
  V(F64Abs, 0x99, d_d)            \
  V(F64Neg, 0x9a, d_d)            \
  V(F64Ceil, 0x9b, d_d)           \
  V(F64Floor, 0x9c, d_d)          \
  V(F64Trunc, 0x9d, d_d)          \
  V(F64NearestInt, 0x9e, d_d)     \
  V(F64Sqrt, 0x9f, d_d)           \
  V(F64Add, 0xa0, d_dd)           \
  V(F64Sub, 0xa1, d_dd)           \
  V(F64Mul, 0xa2, d_dd)           \
  V(F64Div, 0xa3, d_dd)           \
  V(F64Min, 0xa4, d_dd)           \
  V(F64Max, 0xa5, d_dd)           \
  V(F64CopySign, 0xa6, d_dd)      \
  V(I32ConvertI64, 0xa7, i_l)     \
  V(I32SConvertF32, 0xa8, i_f)    \
  V(I32UConvertF32, 0xa9, i_f)    \
  V(I32SConvertF64, 0xaa, i_d)    \
  V(I32UConvertF64, 0xab, i_d)    \
  V(I64SConvertI32, 0xac, l_i)    \
  V(I64UConvertI32, 0xad, l_i)    \
  V(I64SConvertF32, 0xae, l_f)    \
  V(I64UConvertF32, 0xaf, l_f)    \
  V(I64SConvertF64, 0xb0, l_d)    \
  V(I64UConvertF64, 0xb1, l_d)    \
  V(F32SConvertI32, 0xb2, f_i)    \
  V(F32UConvertI32, 0xb3, f_i)    \
  V(F32SConvertI64, 0xb4, f_l)    \
  V(F32UConvertI64, 0xb5, f_l)    \
  V(F32ConvertF64, 0xb6, f_d)     \
  V(F64SConvertI32, 0xb7, d_i)    \
  V(F64UConvertI32, 0xb8, d_i)    \
  V(F64SConvertI64, 0xb9, d_l)    \
  V(F64UConvertI64, 0xba, d_l)    \
  V(F64ConvertF32, 0xbb, d_f)     \
  V(I32ReinterpretF32, 0xbc, i_f) \
  V(I64ReinterpretF64, 0xbd, l_d) \
  V(F32ReinterpretI32, 0xbe, f_i) \
  V(F64ReinterpretI64, 0xbf, d_l) \
  V(I32SExtendI8, 0xc0, i_i)      \
  V(I32SExtendI16, 0xc1, i_i)     \
  V(I64SExtendI8, 0xc2, l_l)      \
  V(I64SExtendI16, 0xc3, l_l)     \
  V(I64SExtendI32, 0xc4, l_l)     \
  V(RefIsNull, 0xd1, i_r)         \
  V(RefEq, 0xd2, i_rr)

// For compatibility with Asm.js.
#define FOREACH_ASMJS_COMPAT_OPCODE(V) \
  V(F64Acos, 0xc5, d_d)                \
  V(F64Asin, 0xc6, d_d)                \
  V(F64Atan, 0xc7, d_d)                \
  V(F64Cos, 0xc8, d_d)                 \
  V(F64Sin, 0xc9, d_d)                 \
  V(F64Tan, 0xca, d_d)                 \
  V(F64Exp, 0xcb, d_d)                 \
  V(F64Log, 0xcc, d_d)                 \
  V(F64Atan2, 0xcd, d_dd)              \
  V(F64Pow, 0xce, d_dd)                \
  V(F64Mod, 0xcf, d_dd)                \
  V(I32AsmjsDivS, 0xd3, i_ii)          \
  V(I32AsmjsDivU, 0xd4, i_ii)          \
  V(I32AsmjsRemS, 0xd5, i_ii)          \
  V(I32AsmjsRemU, 0xd6, i_ii)          \
  V(I32AsmjsLoadMem8S, 0xd7, i_i)      \
  V(I32AsmjsLoadMem8U, 0xd8, i_i)      \
  V(I32AsmjsLoadMem16S, 0xd9, i_i)     \
  V(I32AsmjsLoadMem16U, 0xda, i_i)     \
  V(I32AsmjsLoadMem, 0xdb, i_i)        \
  V(F32AsmjsLoadMem, 0xdc, f_i)        \
  V(F64AsmjsLoadMem, 0xdd, d_i)        \
  V(I32AsmjsStoreMem8, 0xde, i_ii)     \
  V(I32AsmjsStoreMem16, 0xdf, i_ii)    \
  V(I32AsmjsStoreMem, 0xe0, i_ii)      \
  V(F32AsmjsStoreMem, 0xe1, f_if)      \
  V(F64AsmjsStoreMem, 0xe2, d_id)      \
  V(I32AsmjsSConvertF32, 0xe3, i_f)    \
  V(I32AsmjsUConvertF32, 0xe4, i_f)    \
  V(I32AsmjsSConvertF64, 0xe5, i_d)    \
  V(I32AsmjsUConvertF64, 0xe6, i_d)

#define FOREACH_SIMD_0_OPERAND_OPCODE(V) \
  V(F32x4Splat, 0xfd00, s_f)             \
  V(F32x4Abs, 0xfd03, s_s)               \
  V(F32x4Neg, 0xfd04, s_s)               \
  V(F32x4RecipApprox, 0xfd06, s_s)       \
  V(F32x4RecipSqrtApprox, 0xfd07, s_s)   \
  V(F32x4Add, 0xfd08, s_ss)              \
  V(F32x4AddHoriz, 0xfdb9, s_ss)         \
  V(F32x4Sub, 0xfd09, s_ss)              \
  V(F32x4Mul, 0xfd0a, s_ss)              \
  V(F32x4Min, 0xfd0c, s_ss)              \
  V(F32x4Max, 0xfd0d, s_ss)              \
  V(F32x4Eq, 0xfd10, s_ss)               \
  V(F32x4Ne, 0xfd11, s_ss)               \
  V(F32x4Lt, 0xfd12, s_ss)               \
  V(F32x4Le, 0xfd13, s_ss)               \
  V(F32x4Gt, 0xfd14, s_ss)               \
  V(F32x4Ge, 0xfd15, s_ss)               \
  V(F32x4SConvertI32x4, 0xfd19, s_s)     \
  V(F32x4UConvertI32x4, 0xfd1a, s_s)     \
  V(I32x4Splat, 0xfd1b, s_i)             \
  V(I32x4Neg, 0xfd1e, s_s)               \
  V(I32x4Add, 0xfd1f, s_ss)              \
  V(I32x4AddHoriz, 0xfdba, s_ss)         \
  V(I32x4Sub, 0xfd20, s_ss)              \
  V(I32x4Mul, 0xfd21, s_ss)              \
  V(I32x4MinS, 0xfd22, s_ss)             \
  V(I32x4MaxS, 0xfd23, s_ss)             \
  V(I32x4Eq, 0xfd26, s_ss)               \
  V(I32x4Ne, 0xfd27, s_ss)               \
  V(I32x4LtS, 0xfd28, s_ss)              \
  V(I32x4LeS, 0xfd29, s_ss)              \
  V(I32x4GtS, 0xfd2a, s_ss)              \
  V(I32x4GeS, 0xfd2b, s_ss)              \
  V(I32x4SConvertF32x4, 0xfd2f, s_s)     \
  V(I32x4UConvertF32x4, 0xfd37, s_s)     \
  V(I32x4SConvertI16x8Low, 0xfd94, s_s)  \
  V(I32x4SConvertI16x8High, 0xfd95, s_s) \
  V(I32x4UConvertI16x8Low, 0xfd96, s_s)  \
  V(I32x4UConvertI16x8High, 0xfd97, s_s) \
  V(I32x4MinU, 0xfd30, s_ss)             \
  V(I32x4MaxU, 0xfd31, s_ss)             \
  V(I32x4LtU, 0xfd33, s_ss)              \
  V(I32x4LeU, 0xfd34, s_ss)              \
  V(I32x4GtU, 0xfd35, s_ss)              \
  V(I32x4GeU, 0xfd36, s_ss)              \
  V(I16x8Splat, 0xfd38, s_i)             \
  V(I16x8Neg, 0xfd3b, s_s)               \
  V(I16x8Add, 0xfd3c, s_ss)              \
  V(I16x8AddSaturateS, 0xfd3d, s_ss)     \
  V(I16x8AddHoriz, 0xfdbb, s_ss)         \
  V(I16x8Sub, 0xfd3e, s_ss)              \
  V(I16x8SubSaturateS, 0xfd3f, s_ss)     \
  V(I16x8Mul, 0xfd40, s_ss)              \
  V(I16x8MinS, 0xfd41, s_ss)             \
  V(I16x8MaxS, 0xfd42, s_ss)             \
  V(I16x8Eq, 0xfd45, s_ss)               \
  V(I16x8Ne, 0xfd46, s_ss)               \
  V(I16x8LtS, 0xfd47, s_ss)              \
  V(I16x8LeS, 0xfd48, s_ss)              \
  V(I16x8GtS, 0xfd49, s_ss)              \
  V(I16x8GeS, 0xfd4a, s_ss)              \
  V(I16x8AddSaturateU, 0xfd4e, s_ss)     \
  V(I16x8SubSaturateU, 0xfd4f, s_ss)     \
  V(I16x8MinU, 0xfd50, s_ss)             \
  V(I16x8MaxU, 0xfd51, s_ss)             \
  V(I16x8LtU, 0xfd53, s_ss)              \
  V(I16x8LeU, 0xfd54, s_ss)              \
  V(I16x8GtU, 0xfd55, s_ss)              \
  V(I16x8GeU, 0xfd56, s_ss)              \
  V(I16x8SConvertI32x4, 0xfd98, s_ss)    \
  V(I16x8UConvertI32x4, 0xfd99, s_ss)    \
  V(I16x8SConvertI8x16Low, 0xfd9a, s_s)  \
  V(I16x8SConvertI8x16High, 0xfd9b, s_s) \
  V(I16x8UConvertI8x16Low, 0xfd9c, s_s)  \
  V(I16x8UConvertI8x16High, 0xfd9d, s_s) \
  V(I8x16Splat, 0xfd57, s_i)             \
  V(I8x16Neg, 0xfd5a, s_s)               \
  V(I8x16Add, 0xfd5b, s_ss)              \
  V(I8x16AddSaturateS, 0xfd5c, s_ss)     \
  V(I8x16Sub, 0xfd5d, s_ss)              \
  V(I8x16SubSaturateS, 0xfd5e, s_ss)     \
  V(I8x16Mul, 0xfd5f, s_ss)              \
  V(I8x16MinS, 0xfd60, s_ss)             \
  V(I8x16MaxS, 0xfd61, s_ss)             \
  V(I8x16Eq, 0xfd64, s_ss)               \
  V(I8x16Ne, 0xfd65, s_ss)               \
  V(I8x16LtS, 0xfd66, s_ss)              \
  V(I8x16LeS, 0xfd67, s_ss)              \
  V(I8x16GtS, 0xfd68, s_ss)              \
  V(I8x16GeS, 0xfd69, s_ss)              \
  V(I8x16AddSaturateU, 0xfd6d, s_ss)     \
  V(I8x16SubSaturateU, 0xfd6e, s_ss)     \
  V(I8x16MinU, 0xfd6f, s_ss)             \
  V(I8x16MaxU, 0xfd70, s_ss)             \
  V(I8x16LtU, 0xfd72, s_ss)              \
  V(I8x16LeU, 0xfd73, s_ss)              \
  V(I8x16GtU, 0xfd74, s_ss)              \
  V(I8x16GeU, 0xfd75, s_ss)              \
  V(I8x16SConvertI16x8, 0xfd9e, s_ss)    \
  V(I8x16UConvertI16x8, 0xfd9f, s_ss)    \
  V(S128And, 0xfd76, s_ss)               \
  V(S128Or, 0xfd77, s_ss)                \
  V(S128Xor, 0xfd78, s_ss)               \
  V(S128Not, 0xfd79, s_s)                \
  V(S128Select, 0xfd2c, s_sss)           \
  V(S1x4AnyTrue, 0xfd84, i_s)            \
  V(S1x4AllTrue, 0xfd85, i_s)            \
  V(S1x8AnyTrue, 0xfd8a, i_s)            \
  V(S1x8AllTrue, 0xfd8b, i_s)            \
  V(S1x16AnyTrue, 0xfd90, i_s)           \
  V(S1x16AllTrue, 0xfd91, i_s)

#define FOREACH_SIMD_1_OPERAND_OPCODE(V) \
  V(F32x4ExtractLane, 0xfd01, _)         \
  V(F32x4ReplaceLane, 0xfd02, _)         \
  V(I32x4ExtractLane, 0xfd1c, _)         \
  V(I32x4ReplaceLane, 0xfd1d, _)         \
  V(I32x4Shl, 0xfd24, _)                 \
  V(I32x4ShrS, 0xfd25, _)                \
  V(I32x4ShrU, 0xfd32, _)                \
  V(I16x8ExtractLane, 0xfd39, _)         \
  V(I16x8ReplaceLane, 0xfd3a, _)         \
  V(I16x8Shl, 0xfd43, _)                 \
  V(I16x8ShrS, 0xfd44, _)                \
  V(I16x8ShrU, 0xfd52, _)                \
  V(I8x16ExtractLane, 0xfd58, _)         \
  V(I8x16ReplaceLane, 0xfd59, _)         \
  V(I8x16Shl, 0xfd62, _)                 \
  V(I8x16ShrS, 0xfd63, _)                \
  V(I8x16ShrU, 0xfd71, _)

#define FOREACH_SIMD_MASK_OPERAND_OPCODE(V) V(S8x16Shuffle, 0xfd6b, s_ss)

#define FOREACH_SIMD_MEM_OPCODE(V) \
  V(S128LoadMem, 0xfd80, s_i)      \
  V(S128StoreMem, 0xfd81, v_is)

#define FOREACH_NUMERIC_OPCODE(V)   \
  V(I32SConvertSatF32, 0xfc00, i_f) \
  V(I32UConvertSatF32, 0xfc01, i_f) \
  V(I32SConvertSatF64, 0xfc02, i_d) \
  V(I32UConvertSatF64, 0xfc03, i_d) \
  V(I64SConvertSatF32, 0xfc04, l_f) \
  V(I64UConvertSatF32, 0xfc05, l_f) \
  V(I64SConvertSatF64, 0xfc06, l_d) \
  V(I64UConvertSatF64, 0xfc07, l_d)

#define FOREACH_ATOMIC_OPCODE(V)                \
  V(I32AtomicLoad, 0xfe10, i_i)                 \
  V(I64AtomicLoad, 0xfe11, l_i)                 \
  V(I32AtomicLoad8U, 0xfe12, i_i)               \
  V(I32AtomicLoad16U, 0xfe13, i_i)              \
  V(I64AtomicLoad8U, 0xfe14, l_i)               \
  V(I64AtomicLoad16U, 0xfe15, l_i)              \
  V(I64AtomicLoad32U, 0xfe16, l_i)              \
  V(I32AtomicStore, 0xfe17, v_ii)               \
  V(I64AtomicStore, 0xfe18, v_il)               \
  V(I32AtomicStore8U, 0xfe19, v_ii)             \
  V(I32AtomicStore16U, 0xfe1a, v_ii)            \
  V(I64AtomicStore8U, 0xfe1b, v_il)             \
  V(I64AtomicStore16U, 0xfe1c, v_il)            \
  V(I64AtomicStore32U, 0xfe1d, v_il)            \
  V(I32AtomicAdd, 0xfe1e, i_ii)                 \
  V(I64AtomicAdd, 0xfe1f, l_il)                 \
  V(I32AtomicAdd8U, 0xfe20, i_ii)               \
  V(I32AtomicAdd16U, 0xfe21, i_ii)              \
  V(I64AtomicAdd8U, 0xfe22, l_il)               \
  V(I64AtomicAdd16U, 0xfe23, l_il)              \
  V(I64AtomicAdd32U, 0xfe24, l_il)              \
  V(I32AtomicSub, 0xfe25, i_ii)                 \
  V(I64AtomicSub, 0xfe26, l_il)                 \
  V(I32AtomicSub8U, 0xfe27, i_ii)               \
  V(I32AtomicSub16U, 0xfe28, i_ii)              \
  V(I64AtomicSub8U, 0xfe29, l_il)               \
  V(I64AtomicSub16U, 0xfe2a, l_il)              \
  V(I64AtomicSub32U, 0xfe2b, l_il)              \
  V(I32AtomicAnd, 0xfe2c, i_ii)                 \
  V(I64AtomicAnd, 0xfe2d, l_il)                 \
  V(I32AtomicAnd8U, 0xfe2e, i_ii)               \
  V(I32AtomicAnd16U, 0xfe2f, i_ii)              \
  V(I64AtomicAnd8U, 0xfe30, l_il)               \
  V(I64AtomicAnd16U, 0xfe31, l_il)              \
  V(I64AtomicAnd32U, 0xfe32, l_il)              \
  V(I32AtomicOr, 0xfe33, i_ii)                  \
  V(I64AtomicOr, 0xfe34, l_il)                  \
  V(I32AtomicOr8U, 0xfe35, i_ii)                \
  V(I32AtomicOr16U, 0xfe36, i_ii)               \
  V(I64AtomicOr8U, 0xfe37, l_il)                \
  V(I64AtomicOr16U, 0xfe38, l_il)               \
  V(I64AtomicOr32U, 0xfe39, l_il)               \
  V(I32AtomicXor, 0xfe3a, i_ii)                 \
  V(I64AtomicXor, 0xfe3b, l_il)                 \
  V(I32AtomicXor8U, 0xfe3c, i_ii)               \
  V(I32AtomicXor16U, 0xfe3d, i_ii)              \
  V(I64AtomicXor8U, 0xfe3e, l_il)               \
  V(I64AtomicXor16U, 0xfe3f, l_il)              \
  V(I64AtomicXor32U, 0xfe40, l_il)              \
  V(I32AtomicExchange, 0xfe41, i_ii)            \
  V(I64AtomicExchange, 0xfe42, l_il)            \
  V(I32AtomicExchange8U, 0xfe43, i_ii)          \
  V(I32AtomicExchange16U, 0xfe44, i_ii)         \
  V(I64AtomicExchange8U, 0xfe45, l_il)          \
  V(I64AtomicExchange16U, 0xfe46, l_il)         \
  V(I64AtomicExchange32U, 0xfe47, l_il)         \
  V(I32AtomicCompareExchange, 0xfe48, i_iii)    \
  V(I64AtomicCompareExchange, 0xfe49, l_ill)    \
  V(I32AtomicCompareExchange8U, 0xfe4a, i_iii)  \
  V(I32AtomicCompareExchange16U, 0xfe4b, i_iii) \
  V(I64AtomicCompareExchange8U, 0xfe4c, l_ill)  \
  V(I64AtomicCompareExchange16U, 0xfe4d, l_ill) \
  V(I64AtomicCompareExchange32U, 0xfe4e, l_ill)

// All opcodes.
#define FOREACH_OPCODE(V)             \
  FOREACH_CONTROL_OPCODE(V)           \
  FOREACH_MISC_OPCODE(V)              \
  FOREACH_SIMPLE_OPCODE(V)            \
  FOREACH_STORE_MEM_OPCODE(V)         \
  FOREACH_LOAD_MEM_OPCODE(V)          \
  FOREACH_MISC_MEM_OPCODE(V)          \
  FOREACH_ASMJS_COMPAT_OPCODE(V)      \
  FOREACH_SIMD_0_OPERAND_OPCODE(V)    \
  FOREACH_SIMD_1_OPERAND_OPCODE(V)    \
  FOREACH_SIMD_MASK_OPERAND_OPCODE(V) \
  FOREACH_SIMD_MEM_OPCODE(V)          \
  FOREACH_ATOMIC_OPCODE(V)            \
  FOREACH_NUMERIC_OPCODE(V)

// All signatures.
#define FOREACH_SIGNATURE(V)                       \
  FOREACH_SIMD_SIGNATURE(V)                        \
  V(i_ii, kWasmI32, kWasmI32, kWasmI32)            \
  V(i_i, kWasmI32, kWasmI32)                       \
  V(i_v, kWasmI32)                                 \
  V(i_ff, kWasmI32, kWasmF32, kWasmF32)            \
  V(i_f, kWasmI32, kWasmF32)                       \
  V(i_dd, kWasmI32, kWasmF64, kWasmF64)            \
  V(i_d, kWasmI32, kWasmF64)                       \
  V(i_l, kWasmI32, kWasmI64)                       \
  V(l_ll, kWasmI64, kWasmI64, kWasmI64)            \
  V(i_ll, kWasmI32, kWasmI64, kWasmI64)            \
  V(l_l, kWasmI64, kWasmI64)                       \
  V(l_i, kWasmI64, kWasmI32)                       \
  V(l_f, kWasmI64, kWasmF32)                       \
  V(l_d, kWasmI64, kWasmF64)                       \
  V(f_ff, kWasmF32, kWasmF32, kWasmF32)            \
  V(f_f, kWasmF32, kWasmF32)                       \
  V(f_d, kWasmF32, kWasmF64)                       \
  V(f_i, kWasmF32, kWasmI32)                       \
  V(f_l, kWasmF32, kWasmI64)                       \
  V(d_dd, kWasmF64, kWasmF64, kWasmF64)            \
  V(d_d, kWasmF64, kWasmF64)                       \
  V(d_f, kWasmF64, kWasmF32)                       \
  V(d_i, kWasmF64, kWasmI32)                       \
  V(d_l, kWasmF64, kWasmI64)                       \
  V(v_ii, kWasmStmt, kWasmI32, kWasmI32)           \
  V(v_id, kWasmStmt, kWasmI32, kWasmF64)           \
  V(d_id, kWasmF64, kWasmI32, kWasmF64)            \
  V(v_if, kWasmStmt, kWasmI32, kWasmF32)           \
  V(f_if, kWasmF32, kWasmI32, kWasmF32)            \
  V(v_il, kWasmStmt, kWasmI32, kWasmI64)           \
  V(l_il, kWasmI64, kWasmI32, kWasmI64)            \
  V(i_iii, kWasmI32, kWasmI32, kWasmI32, kWasmI32) \
  V(l_ill, kWasmI64, kWasmI32, kWasmI64, kWasmI64) \
  V(i_r, kWasmI32, kWasmAnyRef)                    \
  V(i_rr, kWasmI32, kWasmAnyRef, kWasmAnyRef)

#define FOREACH_SIMD_SIGNATURE(V)          \
  V(s_s, kWasmS128, kWasmS128)             \
  V(s_f, kWasmS128, kWasmF32)              \
  V(s_ss, kWasmS128, kWasmS128, kWasmS128) \
  V(s_i, kWasmS128, kWasmI32)              \
  V(s_si, kWasmS128, kWasmS128, kWasmI32)  \
  V(i_s, kWasmI32, kWasmS128)              \
  V(s_sss, kWasmS128, kWasmS128, kWasmS128, kWasmS128)

#define FOREACH_PREFIX(V) \
  V(Numeric, 0xfc)        \
  V(Simd, 0xfd)           \
  V(Atomic, 0xfe)

enum WasmOpcode {
// Declare expression opcodes.
#define DECLARE_NAMED_ENUM(name, opcode, sig) kExpr##name = opcode,
  FOREACH_OPCODE(DECLARE_NAMED_ENUM)
#undef DECLARE_NAMED_ENUM
#define DECLARE_PREFIX(name, opcode) k##name##Prefix = opcode,
      FOREACH_PREFIX(DECLARE_PREFIX)
#undef DECLARE_PREFIX
};

// The reason for a trap.
#define FOREACH_WASM_TRAPREASON(V) \
  V(TrapUnreachable)               \
  V(TrapMemOutOfBounds)            \
  V(TrapDivByZero)                 \
  V(TrapDivUnrepresentable)        \
  V(TrapRemByZero)                 \
  V(TrapFloatUnrepresentable)      \
  V(TrapFuncInvalid)               \
  V(TrapFuncSigMismatch)

enum TrapReason {
#define DECLARE_ENUM(name) k##name,
  FOREACH_WASM_TRAPREASON(DECLARE_ENUM)
  kTrapCount
#undef DECLARE_ENUM
};

// TODO(clemensh): Compute memtype and size from ValueType once we have c++14
// constexpr support.
#define FOREACH_LOAD_TYPE(V) \
  V(I32, , Int32, 2)         \
  V(I32, 8S, Int8, 0)        \
  V(I32, 8U, Uint8, 0)       \
  V(I32, 16S, Int16, 1)      \
  V(I32, 16U, Uint16, 1)     \
  V(I64, , Int64, 3)         \
  V(I64, 8S, Int8, 0)        \
  V(I64, 8U, Uint8, 0)       \
  V(I64, 16S, Int16, 1)      \
  V(I64, 16U, Uint16, 1)     \
  V(I64, 32S, Int32, 2)      \
  V(I64, 32U, Uint32, 2)     \
  V(F32, , Float32, 2)       \
  V(F64, , Float64, 3)       \
  V(S128, , Simd128, 4)

class LoadType {
 public:
  enum LoadTypeValue : uint8_t {
#define DEF_ENUM(type, suffix, ...) k##type##Load##suffix,
    FOREACH_LOAD_TYPE(DEF_ENUM)
#undef DEF_ENUM
  };

  // Allow implicit convertion of the enum value to this wrapper.
  constexpr LoadType(LoadTypeValue val)  // NOLINT(runtime/explicit)
      : val_(val) {}

  constexpr LoadTypeValue value() const { return val_; }
  constexpr unsigned size_log_2() const { return kLoadSizeLog2[val_]; }
  constexpr unsigned size() const { return 1 << size_log_2(); }
  constexpr ValueType value_type() const { return kValueType[val_]; }
  constexpr MachineType mem_type() const { return kMemType[val_]; }

  static LoadType ForValueType(ValueType type) {
    switch (type) {
      case kWasmI32:
        return kI32Load;
      case kWasmI64:
        return kI64Load;
      case kWasmF32:
        return kF32Load;
      case kWasmF64:
        return kF64Load;
      default:
        UNREACHABLE();
    }
  }

 private:
  const LoadTypeValue val_;

  static constexpr uint8_t kLoadSizeLog2[] = {
#define LOAD_SIZE(_, __, ___, size) size,
      FOREACH_LOAD_TYPE(LOAD_SIZE)
#undef LOAD_SIZE
  };

  static constexpr ValueType kValueType[] = {
#define VALUE_TYPE(type, ...) kWasm##type,
      FOREACH_LOAD_TYPE(VALUE_TYPE)
#undef VALUE_TYPE
  };

  static constexpr MachineType kMemType[] = {
#define MEMTYPE(_, __, memtype, ___) MachineType::memtype(),
      FOREACH_LOAD_TYPE(MEMTYPE)
#undef MEMTYPE
  };
};

#define FOREACH_STORE_TYPE(V) \
  V(I32, , Word32, 2)         \
  V(I32, 8, Word8, 0)         \
  V(I32, 16, Word16, 1)       \
  V(I64, , Word64, 3)         \
  V(I64, 8, Word8, 0)         \
  V(I64, 16, Word16, 1)       \
  V(I64, 32, Word32, 2)       \
  V(F32, , Float32, 2)        \
  V(F64, , Float64, 3)        \
  V(S128, , Simd128, 4)

class StoreType {
 public:
  enum StoreTypeValue : uint8_t {
#define DEF_ENUM(type, suffix, ...) k##type##Store##suffix,
    FOREACH_STORE_TYPE(DEF_ENUM)
#undef DEF_ENUM
  };

  // Allow implicit convertion of the enum value to this wrapper.
  constexpr StoreType(StoreTypeValue val)  // NOLINT(runtime/explicit)
      : val_(val) {}

  constexpr StoreTypeValue value() const { return val_; }
  constexpr unsigned size_log_2() const { return kStoreSizeLog2[val_]; }
  constexpr unsigned size() const { return 1 << size_log_2(); }
  constexpr ValueType value_type() const { return kValueType[val_]; }
  constexpr ValueType mem_rep() const { return kMemRep[val_]; }

  static StoreType ForValueType(ValueType type) {
    switch (type) {
      case kWasmI32:
        return kI32Store;
      case kWasmI64:
        return kI64Store;
      case kWasmF32:
        return kF32Store;
      case kWasmF64:
        return kF64Store;
      default:
        UNREACHABLE();
    }
  }

 private:
  const StoreTypeValue val_;

  static constexpr uint8_t kStoreSizeLog2[] = {
#define STORE_SIZE(_, __, ___, size) size,
      FOREACH_STORE_TYPE(STORE_SIZE)
#undef STORE_SIZE
  };

  static constexpr ValueType kValueType[] = {
#define VALUE_TYPE(type, ...) kWasm##type,
      FOREACH_STORE_TYPE(VALUE_TYPE)
#undef VALUE_TYPE
  };

  static constexpr MachineRepresentation kMemRep[] = {
#define MEMREP(_, __, memrep, ___) MachineRepresentation::k##memrep,
      FOREACH_STORE_TYPE(MEMREP)
#undef MEMREP
  };
};

// A collection of opcode-related static methods.
class V8_EXPORT_PRIVATE WasmOpcodes {
 public:
  static const char* OpcodeName(WasmOpcode opcode);
  static FunctionSig* Signature(WasmOpcode opcode);
  static FunctionSig* AsmjsSignature(WasmOpcode opcode);
  static bool IsPrefixOpcode(WasmOpcode opcode);
  static bool IsControlOpcode(WasmOpcode opcode);
  static bool IsSignExtensionOpcode(WasmOpcode opcode);
  static bool IsAnyRefOpcode(WasmOpcode opcode);
  // Check whether the given opcode always jumps, i.e. all instructions after
  // this one in the current block are dead. Returns false for |end|.
  static bool IsUnconditionalJump(WasmOpcode opcode);

  static int TrapReasonToMessageId(TrapReason reason);
  static const char* TrapReasonMessage(TrapReason reason);

  static byte MemSize(MachineType type) {
    return MemSize(type.representation());
  }

  static byte MemSize(ValueType type) { return 1 << ElementSizeLog2Of(type); }

  static ValueTypeCode ValueTypeCodeFor(ValueType type) {
    switch (type) {
      case kWasmI32:
        return kLocalI32;
      case kWasmI64:
        return kLocalI64;
      case kWasmF32:
        return kLocalF32;
      case kWasmF64:
        return kLocalF64;
      case kWasmS128:
        return kLocalS128;
      case kWasmAnyRef:
        return kLocalAnyRef;
      case kWasmStmt:
        return kLocalVoid;
      default:
        UNREACHABLE();
    }
  }

  static MachineType MachineTypeFor(ValueType type) {
    switch (type) {
      case kWasmI32:
        return MachineType::Int32();
      case kWasmI64:
        return MachineType::Int64();
      case kWasmF32:
        return MachineType::Float32();
      case kWasmF64:
        return MachineType::Float64();
      case kWasmAnyRef:
        return MachineType::TaggedPointer();
      case kWasmS128:
        return MachineType::Simd128();
      case kWasmStmt:
        return MachineType::None();
      default:
        UNREACHABLE();
    }
  }

  static ValueType ValueTypeFor(MachineType type) {
    switch (type.representation()) {
      case MachineRepresentation::kWord8:
      case MachineRepresentation::kWord16:
      case MachineRepresentation::kWord32:
        return kWasmI32;
      case MachineRepresentation::kWord64:
        return kWasmI64;
      case MachineRepresentation::kFloat32:
        return kWasmF32;
      case MachineRepresentation::kFloat64:
        return kWasmF64;
      case MachineRepresentation::kTaggedPointer:
        return kWasmAnyRef;
      case MachineRepresentation::kSimd128:
        return kWasmS128;
      default:
        UNREACHABLE();
    }
  }

  static char ShortNameOf(ValueType type) {
    switch (type) {
      case kWasmI32:
        return 'i';
      case kWasmI64:
        return 'l';
      case kWasmF32:
        return 'f';
      case kWasmF64:
        return 'd';
      case kWasmAnyRef:
        return 'r';
      case kWasmS128:
        return 's';
      case kWasmStmt:
        return 'v';
      case kWasmVar:
        return '*';
      default:
        return '?';
    }
  }

  static const char* TypeName(ValueType type) {
    switch (type) {
      case kWasmI32:
        return "i32";
      case kWasmI64:
        return "i64";
      case kWasmF32:
        return "f32";
      case kWasmF64:
        return "f64";
      case kWasmAnyRef:
        return "ref";
      case kWasmS128:
        return "s128";
      case kWasmStmt:
        return "<stmt>";
      case kWasmVar:
        return "<var>";
      default:
        return "<unknown>";
    }
  }
};

// Representation of an initializer expression.
struct WasmInitExpr {
  enum WasmInitKind {
    kNone,
    kGlobalIndex,
    kI32Const,
    kI64Const,
    kF32Const,
    kF64Const,
    kAnyRefConst,
  } kind;

  union {
    int32_t i32_const;
    int64_t i64_const;
    float f32_const;
    double f64_const;
    uint32_t global_index;
  } val;

  WasmInitExpr() : kind(kNone) {}
  explicit WasmInitExpr(int32_t v) : kind(kI32Const) { val.i32_const = v; }
  explicit WasmInitExpr(int64_t v) : kind(kI64Const) { val.i64_const = v; }
  explicit WasmInitExpr(float v) : kind(kF32Const) { val.f32_const = v; }
  explicit WasmInitExpr(double v) : kind(kF64Const) { val.f64_const = v; }
  WasmInitExpr(WasmInitKind kind, uint32_t global_index) : kind(kGlobalIndex) {
    val.global_index = global_index;
  }
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_OPCODES_H_
