// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_FUNCTION_BODY_DECODER_IMPL_H_
#define V8_WASM_FUNCTION_BODY_DECODER_IMPL_H_

// Do only include this header for implementing new Interface of the
// WasmFullDecoder.

#include "src/base/platform/elapsed-timer.h"
#include "src/base/small-vector.h"
#include "src/utils/bit-vector.h"
#include "src/wasm/decoder.h"
#include "src/wasm/function-body-decoder.h"
#include "src/wasm/value-type.h"
#include "src/wasm/wasm-features.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-opcodes.h"
#include "src/wasm/wasm-subtyping.h"

namespace v8 {
namespace internal {
namespace wasm {

struct WasmGlobal;
struct WasmException;

#define TRACE(...)                                    \
  do {                                                \
    if (FLAG_trace_wasm_decoder) PrintF(__VA_ARGS__); \
  } while (false)

#define TRACE_INST_FORMAT "  @%-8d #%-20s|"

// Return the evaluation of `condition` if validate==true, DCHECK that it's
// true and always return true otherwise.
#define VALIDATE(condition)       \
  (validate ? (condition) : [&] { \
    DCHECK(condition);            \
    return true;                  \
  }())

#define CHECK_PROTOTYPE_OPCODE_GEN(feat, opt_break)                            \
  DCHECK(this->module_->origin == kWasmOrigin);                                \
  if (!this->enabled_.has_##feat()) {                                          \
    this->error("Invalid opcode (enable with --experimental-wasm-" #feat ")"); \
    opt_break                                                                  \
  } else {                                                                     \
    this->detected_->Add(kFeature_##feat);                                     \
  }

#define CHECK_PROTOTYPE_OPCODE(feat) CHECK_PROTOTYPE_OPCODE_GEN(feat, break;)

#define RET_ON_PROTOTYPE_OPCODE(feat) CHECK_PROTOTYPE_OPCODE_GEN(feat, )

#define OPCODE_ERROR(opcode, message)                                 \
  (this->errorf(this->pc_, "%s: %s", WasmOpcodes::OpcodeName(opcode), \
                (message)))

#define ATOMIC_OP_LIST(V)                \
  V(AtomicNotify, Uint32)                \
  V(I32AtomicWait, Uint32)               \
  V(I64AtomicWait, Uint64)               \
  V(I32AtomicLoad, Uint32)               \
  V(I64AtomicLoad, Uint64)               \
  V(I32AtomicLoad8U, Uint8)              \
  V(I32AtomicLoad16U, Uint16)            \
  V(I64AtomicLoad8U, Uint8)              \
  V(I64AtomicLoad16U, Uint16)            \
  V(I64AtomicLoad32U, Uint32)            \
  V(I32AtomicAdd, Uint32)                \
  V(I32AtomicAdd8U, Uint8)               \
  V(I32AtomicAdd16U, Uint16)             \
  V(I64AtomicAdd, Uint64)                \
  V(I64AtomicAdd8U, Uint8)               \
  V(I64AtomicAdd16U, Uint16)             \
  V(I64AtomicAdd32U, Uint32)             \
  V(I32AtomicSub, Uint32)                \
  V(I64AtomicSub, Uint64)                \
  V(I32AtomicSub8U, Uint8)               \
  V(I32AtomicSub16U, Uint16)             \
  V(I64AtomicSub8U, Uint8)               \
  V(I64AtomicSub16U, Uint16)             \
  V(I64AtomicSub32U, Uint32)             \
  V(I32AtomicAnd, Uint32)                \
  V(I64AtomicAnd, Uint64)                \
  V(I32AtomicAnd8U, Uint8)               \
  V(I32AtomicAnd16U, Uint16)             \
  V(I64AtomicAnd8U, Uint8)               \
  V(I64AtomicAnd16U, Uint16)             \
  V(I64AtomicAnd32U, Uint32)             \
  V(I32AtomicOr, Uint32)                 \
  V(I64AtomicOr, Uint64)                 \
  V(I32AtomicOr8U, Uint8)                \
  V(I32AtomicOr16U, Uint16)              \
  V(I64AtomicOr8U, Uint8)                \
  V(I64AtomicOr16U, Uint16)              \
  V(I64AtomicOr32U, Uint32)              \
  V(I32AtomicXor, Uint32)                \
  V(I64AtomicXor, Uint64)                \
  V(I32AtomicXor8U, Uint8)               \
  V(I32AtomicXor16U, Uint16)             \
  V(I64AtomicXor8U, Uint8)               \
  V(I64AtomicXor16U, Uint16)             \
  V(I64AtomicXor32U, Uint32)             \
  V(I32AtomicExchange, Uint32)           \
  V(I64AtomicExchange, Uint64)           \
  V(I32AtomicExchange8U, Uint8)          \
  V(I32AtomicExchange16U, Uint16)        \
  V(I64AtomicExchange8U, Uint8)          \
  V(I64AtomicExchange16U, Uint16)        \
  V(I64AtomicExchange32U, Uint32)        \
  V(I32AtomicCompareExchange, Uint32)    \
  V(I64AtomicCompareExchange, Uint64)    \
  V(I32AtomicCompareExchange8U, Uint8)   \
  V(I32AtomicCompareExchange16U, Uint16) \
  V(I64AtomicCompareExchange8U, Uint8)   \
  V(I64AtomicCompareExchange16U, Uint16) \
  V(I64AtomicCompareExchange32U, Uint32)

#define ATOMIC_STORE_OP_LIST(V) \
  V(I32AtomicStore, Uint32)     \
  V(I64AtomicStore, Uint64)     \
  V(I32AtomicStore8U, Uint8)    \
  V(I32AtomicStore16U, Uint16)  \
  V(I64AtomicStore8U, Uint8)    \
  V(I64AtomicStore16U, Uint16)  \
  V(I64AtomicStore32U, Uint32)

namespace value_type_reader {

// Read a value type starting at address 'pc' in 'decoder'.
// No bytes are consumed. The result is written into the 'result' parameter.
// Returns the amount of bytes read, or 0 if decoding failed.
// Registers an error if the type opcode is invalid iff validate is set.
template <Decoder::ValidateFlag validate>
ValueType read_value_type(Decoder* decoder, const byte* pc,
                          uint32_t* const length, const WasmFeatures& enabled) {
  *length = 1;
  byte val = decoder->read_u8<validate>(pc, "value type opcode");
  if (decoder->failed()) {
    return kWasmBottom;
  }

  ValueTypeCode code = static_cast<ValueTypeCode>(val);
  switch (code) {
    case kLocalI32:
      return kWasmI32;
    case kLocalI64:
      return kWasmI64;
    case kLocalF32:
      return kWasmF32;
    case kLocalF64:
      return kWasmF64;
    case kLocalExternRef:
      if (enabled.has_reftypes()) {
        return kWasmExternRef;
      }
      decoder->error(pc,
                     "invalid value type 'externref', enable with "
                     "--experimental-wasm-reftypes");
      return kWasmBottom;
    case kLocalFuncRef:
      if (enabled.has_reftypes()) {
        return kWasmFuncRef;
      }
      decoder->error(pc,
                     "invalid value type 'funcref', enable with "
                     "--experimental-wasm-reftypes");
      return kWasmBottom;
    case kLocalExnRef:
      if (enabled.has_eh()) {
        return kWasmExnRef;
      }
      decoder->error(pc,
                     "invalid value type 'exception ref', enable with "
                     "--experimental-wasm-eh");
      return kWasmBottom;
    case kLocalRef:
      if (enabled.has_gc()) {
        uint32_t type_index =
            decoder->read_u32v<validate>(pc + 1, length, "type index");
        (*length)++;
        if (!VALIDATE(type_index < kV8MaxWasmTypes)) {
          decoder->errorf(pc,
                          "Type index %u is greater than the maximum "
                          "number %zu of type definitions supported by V8",
                          type_index, kV8MaxWasmTypes);
          return kWasmBottom;
        } else {
          return ValueType(ValueType::kRef, type_index);
        }
      }
      decoder->error(pc,
                     "invalid value type 'ref', enable with "
                     "--experimental-wasm-gc");
      return kWasmBottom;
    case kLocalOptRef:
      if (enabled.has_gc()) {
        uint32_t type_index =
            decoder->read_u32v<validate>(pc + 1, length, "type index");
        (*length)++;
        if (!VALIDATE(type_index < kV8MaxWasmTypes)) {
          decoder->errorf(pc,
                          "Type index %u is greater than the maximum "
                          "number %zu of type definitions supported by V8",
                          type_index, kV8MaxWasmTypes);
          return kWasmBottom;
        } else {
          return ValueType(ValueType::kOptRef, type_index);
        }
      }
      decoder->error(pc,
                     "invalid value type 'optref', enable with "
                     "--experimental-wasm-gc");
      return kWasmBottom;
    case kLocalEqRef:
      if (enabled.has_gc()) {
        return kWasmEqRef;
      }
      decoder->error(pc,
                     "invalid value type 'eqref', enable with "
                     "--experimental-wasm-simd");
      return kWasmBottom;
    case kLocalI31Ref:
      if (enabled.has_gc()) {
        // TODO(7748): Implement
        decoder->error(pc, "'i31ref' is unimplemented");
      }
      decoder->error(pc,
                     "invalid value type 'i31ref', enable with "
                     "--experimental-wasm-simd");
      return kWasmBottom;
    case kLocalRttRef:
      if (enabled.has_gc()) {
        // TODO(7748): Implement
        decoder->error(pc, "'rttref' is unimplemented");
      }
      decoder->error(pc,
                     "invalid value type 'rttref', enable with "
                     "--experimental-wasm-simd");
      return kWasmBottom;
    case kLocalS128:
      if (enabled.has_simd()) {
        return kWasmS128;
      }
      decoder->error(pc,
                     "invalid value type 'Simd128', enable with "
                     "--experimental-wasm-simd");
      return kWasmBottom;
    case kLocalVoid:
    case kLocalI8:
    case kLocalI16:
      // Although these types are included in ValueType, they are technically
      // not value types and are only used in specific contexts. The caller of
      // this function is responsible to check for them separately.
      break;
  }
  // Malformed modules specifying invalid types can get here.
  return kWasmBottom;
}
}  // namespace value_type_reader

// Helpers for decoding different kinds of immediates which follow bytecodes.
template <Decoder::ValidateFlag validate>
struct LocalIndexImmediate {
  uint32_t index;
  ValueType type = kWasmStmt;
  uint32_t length;

  inline LocalIndexImmediate(Decoder* decoder, const byte* pc) {
    index = decoder->read_u32v<validate>(pc + 1, &length, "local index");
  }
};

template <Decoder::ValidateFlag validate>
struct ExceptionIndexImmediate {
  uint32_t index;
  const WasmException* exception = nullptr;
  uint32_t length;

  inline ExceptionIndexImmediate(Decoder* decoder, const byte* pc) {
    index = decoder->read_u32v<validate>(pc + 1, &length, "exception index");
  }
};

template <Decoder::ValidateFlag validate>
struct ImmI32Immediate {
  int32_t value;
  uint32_t length;
  inline ImmI32Immediate(Decoder* decoder, const byte* pc) {
    value = decoder->read_i32v<validate>(pc + 1, &length, "immi32");
  }
};

template <Decoder::ValidateFlag validate>
struct ImmI64Immediate {
  int64_t value;
  uint32_t length;
  inline ImmI64Immediate(Decoder* decoder, const byte* pc) {
    value = decoder->read_i64v<validate>(pc + 1, &length, "immi64");
  }
};

template <Decoder::ValidateFlag validate>
struct ImmF32Immediate {
  float value;
  uint32_t length = 4;
  inline ImmF32Immediate(Decoder* decoder, const byte* pc) {
    // We can't use bit_cast here because calling any helper function that
    // returns a float would potentially flip NaN bits per C++ semantics, so we
    // have to inline the memcpy call directly.
    uint32_t tmp = decoder->read_u32<validate>(pc + 1, "immf32");
    memcpy(&value, &tmp, sizeof(value));
  }
};

template <Decoder::ValidateFlag validate>
struct ImmF64Immediate {
  double value;
  uint32_t length = 8;
  inline ImmF64Immediate(Decoder* decoder, const byte* pc) {
    // Avoid bit_cast because it might not preserve the signalling bit of a NaN.
    uint64_t tmp = decoder->read_u64<validate>(pc + 1, "immf64");
    memcpy(&value, &tmp, sizeof(value));
  }
};

template <Decoder::ValidateFlag validate>
struct RefNullImmediate {
  ValueType type;
  uint32_t length = 1;
  inline RefNullImmediate(const WasmFeatures& enabled, Decoder* decoder,
                          const byte* pc) {
    type = value_type_reader::read_value_type<validate>(decoder, pc + 1,
                                                        &length, enabled);
  }
};

template <Decoder::ValidateFlag validate>
struct GlobalIndexImmediate {
  uint32_t index;
  ValueType type = kWasmStmt;
  const WasmGlobal* global = nullptr;
  uint32_t length;

  inline GlobalIndexImmediate(Decoder* decoder, const byte* pc) {
    index = decoder->read_u32v<validate>(pc + 1, &length, "global index");
  }
};

template <Decoder::ValidateFlag validate>
struct SelectTypeImmediate {
  uint32_t length;
  ValueType type;

  inline SelectTypeImmediate(const WasmFeatures& enabled, Decoder* decoder,
                             const byte* pc) {
    uint8_t num_types =
        decoder->read_u32v<validate>(pc + 1, &length, "number of select types");
    if (!VALIDATE(num_types == 1)) {
      decoder->error(
          pc + 1, "Invalid number of types. Select accepts exactly one type");
      return;
    }
    uint32_t type_length;
    type = value_type_reader::read_value_type<validate>(
        decoder, pc + length + 1, &type_length, enabled);
    length += type_length;
    if (type == kWasmBottom) {
      decoder->error(pc + 1, "invalid select type");
    }
  }
};

template <Decoder::ValidateFlag validate>
struct BlockTypeImmediate {
  uint32_t length = 1;
  ValueType type = kWasmStmt;
  uint32_t sig_index = 0;
  const FunctionSig* sig = nullptr;

  inline BlockTypeImmediate(const WasmFeatures& enabled, Decoder* decoder,
                            const byte* pc) {
    if (decoder->read_u8<validate>(pc + 1, "block type") == kLocalVoid) {
      // 1st case: void block. Struct fields stay at default values.
      return;
    }
    type = value_type_reader::read_value_type<validate>(decoder, pc + 1,
                                                        &length, enabled);
    if (type != kWasmBottom) {
      // 2nd case: block with val type immediate.
      return;
    }
    // It has to be the 3rd case: multi-value block,
    // which is represented by a type index.
    if (!VALIDATE(enabled.has_mv())) {
      decoder->error(pc + 1, "invalid block type");
      return;
    }
    if (!VALIDATE(decoder->ok())) return;
    int32_t index =
        decoder->read_i32v<validate>(pc + 1, &length, "block type index");
    if (!VALIDATE(length > 0 && index >= 0)) {
      decoder->error(pc + 1, "invalid block type index");
      return;
    }
    sig_index = static_cast<uint32_t>(index);
  }

  uint32_t in_arity() const {
    if (type != kWasmBottom) return 0;
    return static_cast<uint32_t>(sig->parameter_count());
  }
  uint32_t out_arity() const {
    if (type == kWasmStmt) return 0;
    if (type != kWasmBottom) return 1;
    return static_cast<uint32_t>(sig->return_count());
  }
  ValueType in_type(uint32_t index) {
    DCHECK_EQ(kWasmBottom, type);
    return sig->GetParam(index);
  }
  ValueType out_type(uint32_t index) {
    if (type == kWasmBottom) return sig->GetReturn(index);
    DCHECK_NE(kWasmStmt, type);
    DCHECK_EQ(0, index);
    return type;
  }
};

template <Decoder::ValidateFlag validate>
struct BranchDepthImmediate {
  uint32_t depth;
  uint32_t length;
  inline BranchDepthImmediate(Decoder* decoder, const byte* pc) {
    depth = decoder->read_u32v<validate>(pc + 1, &length, "branch depth");
  }
};

template <Decoder::ValidateFlag validate>
struct BranchOnExceptionImmediate {
  BranchDepthImmediate<validate> depth;
  ExceptionIndexImmediate<validate> index;
  uint32_t length = 0;
  inline BranchOnExceptionImmediate(Decoder* decoder, const byte* pc)
      : depth(BranchDepthImmediate<validate>(decoder, pc)),
        index(ExceptionIndexImmediate<validate>(decoder, pc + depth.length)) {
    length = depth.length + index.length;
  }
};

template <Decoder::ValidateFlag validate>
struct FunctionIndexImmediate {
  uint32_t index = 0;
  uint32_t length = 1;
  inline FunctionIndexImmediate(Decoder* decoder, const byte* pc) {
    index = decoder->read_u32v<validate>(pc + 1, &length, "function index");
  }
};

template <Decoder::ValidateFlag validate>
struct MemoryIndexImmediate {
  uint32_t index = 0;
  uint32_t length = 1;
  inline MemoryIndexImmediate() = default;
  inline MemoryIndexImmediate(Decoder* decoder, const byte* pc) {
    index = decoder->read_u8<validate>(pc + 1, "memory index");
    if (!VALIDATE(index == 0)) {
      decoder->errorf(pc + 1, "expected memory index 0, found %u", index);
    }
  }
};

template <Decoder::ValidateFlag validate>
struct TableIndexImmediate {
  uint32_t index = 0;
  unsigned length = 1;
  inline TableIndexImmediate() = default;
  inline TableIndexImmediate(Decoder* decoder, const byte* pc) {
    index = decoder->read_u32v<validate>(pc + 1, &length, "table index");
  }
};

template <Decoder::ValidateFlag validate>
struct StructIndexImmediate {
  uint32_t index = 0;
  uint32_t length = 0;
  const StructType* struct_type = nullptr;
  inline StructIndexImmediate(Decoder* decoder, const byte* pc) {
    index = decoder->read_u32v<validate>(pc, &length, "struct index");
  }
};

template <Decoder::ValidateFlag validate>
struct FieldIndexImmediate {
  StructIndexImmediate<validate> struct_index;
  uint32_t index = 0;
  uint32_t length = 0;
  inline FieldIndexImmediate(Decoder* decoder, const byte* pc)
      : struct_index(decoder, pc) {
    index = decoder->read_u32v<validate>(pc + struct_index.length, &length,
                                         "field index");
    length += struct_index.length;
  }
};

template <Decoder::ValidateFlag validate>
struct ArrayIndexImmediate {
  uint32_t index = 0;
  uint32_t length = 0;
  const ArrayType* array_type = nullptr;
  inline ArrayIndexImmediate(Decoder* decoder, const byte* pc) {
    index = decoder->read_u32v<validate>(pc, &length, "array index");
  }
};

template <Decoder::ValidateFlag validate>
struct CallIndirectImmediate {
  uint32_t table_index;
  uint32_t sig_index;
  const FunctionSig* sig = nullptr;
  uint32_t length = 0;
  inline CallIndirectImmediate(const WasmFeatures enabled, Decoder* decoder,
                               const byte* pc) {
    uint32_t len = 0;
    sig_index = decoder->read_u32v<validate>(pc + 1, &len, "signature index");
    TableIndexImmediate<validate> table(decoder, pc + len);
    if (!VALIDATE((table.index == 0 && table.length == 1) ||
                  enabled.has_reftypes())) {
      decoder->errorf(pc + 1 + len, "expected table index 0, found %u",
                      table.index);
    }
    table_index = table.index;
    length = len + table.length;
  }
};

template <Decoder::ValidateFlag validate>
struct CallFunctionImmediate {
  uint32_t index;
  const FunctionSig* sig = nullptr;
  uint32_t length;
  inline CallFunctionImmediate(Decoder* decoder, const byte* pc) {
    index = decoder->read_u32v<validate>(pc + 1, &length, "function index");
  }
};

template <Decoder::ValidateFlag validate>
struct BranchTableImmediate {
  uint32_t table_count;
  const byte* start;
  const byte* table;
  inline BranchTableImmediate(Decoder* decoder, const byte* pc) {
    DCHECK_EQ(kExprBrTable, decoder->read_u8<validate>(pc, "opcode"));
    start = pc + 1;
    uint32_t len = 0;
    table_count = decoder->read_u32v<validate>(pc + 1, &len, "table count");
    table = pc + 1 + len;
  }
};

// A helper to iterate over a branch table.
template <Decoder::ValidateFlag validate>
class BranchTableIterator {
 public:
  uint32_t cur_index() { return index_; }
  bool has_next() { return VALIDATE(decoder_->ok()) && index_ <= table_count_; }
  uint32_t next() {
    DCHECK(has_next());
    index_++;
    uint32_t length;
    uint32_t result =
        decoder_->read_u32v<validate>(pc_, &length, "branch table entry");
    pc_ += length;
    return result;
  }
  // length, including the length of the {BranchTableImmediate}, but not the
  // opcode.
  uint32_t length() {
    while (has_next()) next();
    return static_cast<uint32_t>(pc_ - start_);
  }
  const byte* pc() { return pc_; }

  BranchTableIterator(Decoder* decoder,
                      const BranchTableImmediate<validate>& imm)
      : decoder_(decoder),
        start_(imm.start),
        pc_(imm.table),
        table_count_(imm.table_count) {}

 private:
  Decoder* decoder_;
  const byte* start_;
  const byte* pc_;
  uint32_t index_ = 0;    // the current index.
  uint32_t table_count_;  // the count of entries, not including default.
};

template <Decoder::ValidateFlag validate>
struct MemoryAccessImmediate {
  uint32_t alignment;
  uint32_t offset;
  uint32_t length = 0;
  inline MemoryAccessImmediate(Decoder* decoder, const byte* pc,
                               uint32_t max_alignment) {
    uint32_t alignment_length;
    alignment =
        decoder->read_u32v<validate>(pc + 1, &alignment_length, "alignment");
    if (!VALIDATE(alignment <= max_alignment)) {
      decoder->errorf(pc + 1,
                      "invalid alignment; expected maximum alignment is %u, "
                      "actual alignment is %u",
                      max_alignment, alignment);
    }
    uint32_t offset_length;
    offset = decoder->read_u32v<validate>(pc + 1 + alignment_length,
                                          &offset_length, "offset");
    length = alignment_length + offset_length;
  }
};

// Immediate for SIMD lane operations.
template <Decoder::ValidateFlag validate>
struct SimdLaneImmediate {
  uint8_t lane;
  uint32_t length = 1;

  inline SimdLaneImmediate(Decoder* decoder, const byte* pc,
                           uint32_t opcode_length) {
    // Callers should pass in pc unchanged from where the decoding happens. 1 is
    // added to account for the SIMD prefix byte, and opcode_length is the
    // number of bytes the LEB encoding of the SIMD opcode takes.
    lane = decoder->read_u8<validate>(pc + 1 + opcode_length, "lane");
  }
};

// Immediate for SIMD S8x16 shuffle operations.
template <Decoder::ValidateFlag validate>
struct Simd8x16ShuffleImmediate {
  uint8_t shuffle[kSimd128Size] = {0};

  inline Simd8x16ShuffleImmediate(Decoder* decoder, const byte* pc,
                                  uint32_t opcode_length) {
    // Callers should pass in pc unchanged from where the decoding happens. 1 is
    // added to account for the SIMD prefix byte, and opcode_length is the
    // number of bytes the LEB encoding of the SIMD opcode takes.
    for (uint32_t i = 0; i < kSimd128Size; ++i) {
      shuffle[i] =
          decoder->read_u8<validate>(pc + 1 + opcode_length + i, "shuffle");
    }
  }
};

template <Decoder::ValidateFlag validate>
struct MemoryInitImmediate {
  uint32_t data_segment_index = 0;
  MemoryIndexImmediate<validate> memory;
  unsigned length = 0;

  inline MemoryInitImmediate(Decoder* decoder, const byte* pc) {
    uint32_t len = 0;
    data_segment_index =
        decoder->read_u32v<validate>(pc + 2, &len, "data segment index");
    memory = MemoryIndexImmediate<validate>(decoder, pc + 1 + len);
    length = len + memory.length;
  }
};

template <Decoder::ValidateFlag validate>
struct DataDropImmediate {
  uint32_t index;
  unsigned length;

  inline DataDropImmediate(Decoder* decoder, const byte* pc) {
    index = decoder->read_u32v<validate>(pc + 2, &length, "data segment index");
  }
};

template <Decoder::ValidateFlag validate>
struct MemoryCopyImmediate {
  MemoryIndexImmediate<validate> memory_src;
  MemoryIndexImmediate<validate> memory_dst;
  unsigned length = 0;

  inline MemoryCopyImmediate(Decoder* decoder, const byte* pc) {
    memory_src = MemoryIndexImmediate<validate>(decoder, pc + 1);
    memory_dst =
        MemoryIndexImmediate<validate>(decoder, pc + 1 + memory_src.length);
    length = memory_src.length + memory_dst.length;
  }
};

template <Decoder::ValidateFlag validate>
struct TableInitImmediate {
  uint32_t elem_segment_index = 0;
  TableIndexImmediate<validate> table;
  unsigned length = 0;

  inline TableInitImmediate(Decoder* decoder, const byte* pc) {
    uint32_t len = 0;
    elem_segment_index =
        decoder->read_u32v<validate>(pc + 2, &len, "elem segment index");
    table = TableIndexImmediate<validate>(decoder, pc + 1 + len);
    length = len + table.length;
  }
};

template <Decoder::ValidateFlag validate>
struct ElemDropImmediate {
  uint32_t index;
  unsigned length;

  inline ElemDropImmediate(Decoder* decoder, const byte* pc) {
    index = decoder->read_u32v<validate>(pc + 2, &length, "elem segment index");
  }
};

template <Decoder::ValidateFlag validate>
struct TableCopyImmediate {
  TableIndexImmediate<validate> table_dst;
  TableIndexImmediate<validate> table_src;
  unsigned length = 0;

  inline TableCopyImmediate(Decoder* decoder, const byte* pc) {
    table_dst = TableIndexImmediate<validate>(decoder, pc + 1);
    table_src =
        TableIndexImmediate<validate>(decoder, pc + 1 + table_dst.length);
    length = table_src.length + table_dst.length;
  }
};

// An entry on the value stack.
struct ValueBase {
  const byte* pc = nullptr;
  ValueType type = kWasmStmt;

  ValueBase(const byte* pc, ValueType type) : pc(pc), type(type) {}
};

template <typename Value>
struct Merge {
  uint32_t arity = 0;
  union {  // Either multiple values or a single value.
    Value* array;
    Value first;
  } vals = {nullptr};  // Initialize {array} with {nullptr}.

  // Tracks whether this merge was ever reached. Uses precise reachability, like
  // Reachability::kReachable.
  bool reached;

  explicit Merge(bool reached = false) : reached(reached) {}

  Value& operator[](uint32_t i) {
    DCHECK_GT(arity, i);
    return arity == 1 ? vals.first : vals.array[i];
  }
};

enum ControlKind : uint8_t {
  kControlIf,
  kControlIfElse,
  kControlBlock,
  kControlLoop,
  kControlLet,
  kControlTry,
  kControlTryCatch
};

enum Reachability : uint8_t {
  // reachable code.
  kReachable,
  // reachable code in unreachable block (implies normal validation).
  kSpecOnlyReachable,
  // code unreachable in its own block (implies polymorphic validation).
  kUnreachable
};

// An entry on the control stack (i.e. if, block, loop, or try).
template <typename Value>
struct ControlBase {
  ControlKind kind = kControlBlock;
  uint32_t locals_count = 0;
  uint32_t stack_depth = 0;  // stack height at the beginning of the construct.
  const uint8_t* pc = nullptr;
  Reachability reachability = kReachable;

  // Values merged into the start or end of this control construct.
  Merge<Value> start_merge;
  Merge<Value> end_merge;

  MOVE_ONLY_NO_DEFAULT_CONSTRUCTOR(ControlBase);

  ControlBase(ControlKind kind, uint32_t locals_count, uint32_t stack_depth,
              const uint8_t* pc, Reachability reachability)
      : kind(kind),
        locals_count(locals_count),
        stack_depth(stack_depth),
        pc(pc),
        reachability(reachability),
        start_merge(reachability == kReachable) {
    DCHECK(kind == kControlLet || locals_count == 0);
  }

  // Check whether the current block is reachable.
  bool reachable() const { return reachability == kReachable; }

  // Check whether the rest of the block is unreachable.
  // Note that this is different from {!reachable()}, as there is also the
  // "indirect unreachable state", for which both {reachable()} and
  // {unreachable()} return false.
  bool unreachable() const { return reachability == kUnreachable; }

  // Return the reachability of new control structs started in this block.
  Reachability innerReachability() const {
    return reachability == kReachable ? kReachable : kSpecOnlyReachable;
  }

  bool is_if() const { return is_onearmed_if() || is_if_else(); }
  bool is_onearmed_if() const { return kind == kControlIf; }
  bool is_if_else() const { return kind == kControlIfElse; }
  bool is_block() const { return kind == kControlBlock; }
  bool is_let() const { return kind == kControlLet; }
  bool is_loop() const { return kind == kControlLoop; }
  bool is_incomplete_try() const { return kind == kControlTry; }
  bool is_try_catch() const { return kind == kControlTryCatch; }
  bool is_try() const { return is_incomplete_try() || is_try_catch(); }

  inline Merge<Value>* br_merge() {
    return is_loop() ? &this->start_merge : &this->end_merge;
  }
};

// This is the list of callback functions that an interface for the
// WasmFullDecoder should implement.
// F(Name, args...)
#define INTERFACE_FUNCTIONS(F)                                                 \
  /* General: */                                                               \
  F(StartFunction)                                                             \
  F(StartFunctionBody, Control* block)                                         \
  F(FinishFunction)                                                            \
  F(OnFirstError)                                                              \
  F(NextInstruction, WasmOpcode)                                               \
  /* Control: */                                                               \
  F(Block, Control* block)                                                     \
  F(Loop, Control* block)                                                      \
  F(Try, Control* block)                                                       \
  F(Catch, Control* block, Value* exception)                                   \
  F(If, const Value& cond, Control* if_block)                                  \
  F(FallThruTo, Control* c)                                                    \
  F(PopControl, Control* block)                                                \
  F(EndControl, Control* block)                                                \
  /* Instructions: */                                                          \
  F(UnOp, WasmOpcode opcode, const Value& value, Value* result)                \
  F(BinOp, WasmOpcode opcode, const Value& lhs, const Value& rhs,              \
    Value* result)                                                             \
  F(I32Const, Value* result, int32_t value)                                    \
  F(I64Const, Value* result, int64_t value)                                    \
  F(F32Const, Value* result, float value)                                      \
  F(F64Const, Value* result, double value)                                     \
  F(RefNull, Value* result)                                                    \
  F(RefFunc, uint32_t function_index, Value* result)                           \
  F(RefAsNonNull, const Value& arg, Value* result)                             \
  F(Drop, const Value& value)                                                  \
  F(DoReturn, Vector<Value> values)                                            \
  F(LocalGet, Value* result, const LocalIndexImmediate<validate>& imm)         \
  F(LocalSet, const Value& value, const LocalIndexImmediate<validate>& imm)    \
  F(LocalTee, const Value& value, Value* result,                               \
    const LocalIndexImmediate<validate>& imm)                                  \
  F(AllocateLocals, Vector<Value> local_values)                                \
  F(DeallocateLocals, uint32_t count)                                          \
  F(GlobalGet, Value* result, const GlobalIndexImmediate<validate>& imm)       \
  F(GlobalSet, const Value& value, const GlobalIndexImmediate<validate>& imm)  \
  F(TableGet, const Value& index, Value* result,                               \
    const TableIndexImmediate<validate>& imm)                                  \
  F(TableSet, const Value& index, const Value& value,                          \
    const TableIndexImmediate<validate>& imm)                                  \
  F(Unreachable)                                                               \
  F(Select, const Value& cond, const Value& fval, const Value& tval,           \
    Value* result)                                                             \
  F(Br, Control* target)                                                       \
  F(BrIf, const Value& cond, uint32_t depth)                                   \
  F(BrTable, const BranchTableImmediate<validate>& imm, const Value& key)      \
  F(Else, Control* if_block)                                                   \
  F(LoadMem, LoadType type, const MemoryAccessImmediate<validate>& imm,        \
    const Value& index, Value* result)                                         \
  F(LoadTransform, LoadType type, LoadTransformationKind transform,            \
    MemoryAccessImmediate<validate>& imm, const Value& index, Value* result)   \
  F(StoreMem, StoreType type, const MemoryAccessImmediate<validate>& imm,      \
    const Value& index, const Value& value)                                    \
  F(CurrentMemoryPages, Value* result)                                         \
  F(MemoryGrow, const Value& value, Value* result)                             \
  F(CallDirect, const CallFunctionImmediate<validate>& imm,                    \
    const Value args[], Value returns[])                                       \
  F(CallIndirect, const Value& index,                                          \
    const CallIndirectImmediate<validate>& imm, const Value args[],            \
    Value returns[])                                                           \
  F(ReturnCall, const CallFunctionImmediate<validate>& imm,                    \
    const Value args[])                                                        \
  F(ReturnCallIndirect, const Value& index,                                    \
    const CallIndirectImmediate<validate>& imm, const Value args[])            \
  F(BrOnNull, const Value& ref_object, uint32_t depth)                         \
  F(SimdOp, WasmOpcode opcode, Vector<Value> args, Value* result)              \
  F(SimdLaneOp, WasmOpcode opcode, const SimdLaneImmediate<validate>& imm,     \
    const Vector<Value> inputs, Value* result)                                 \
  F(Simd8x16ShuffleOp, const Simd8x16ShuffleImmediate<validate>& imm,          \
    const Value& input0, const Value& input1, Value* result)                   \
  F(Throw, const ExceptionIndexImmediate<validate>& imm,                       \
    const Vector<Value>& args)                                                 \
  F(Rethrow, const Value& exception)                                           \
  F(BrOnException, const Value& exception,                                     \
    const ExceptionIndexImmediate<validate>& imm, uint32_t depth,              \
    Vector<Value> values)                                                      \
  F(AtomicOp, WasmOpcode opcode, Vector<Value> args,                           \
    const MemoryAccessImmediate<validate>& imm, Value* result)                 \
  F(AtomicFence)                                                               \
  F(MemoryInit, const MemoryInitImmediate<validate>& imm, const Value& dst,    \
    const Value& src, const Value& size)                                       \
  F(DataDrop, const DataDropImmediate<validate>& imm)                          \
  F(MemoryCopy, const MemoryCopyImmediate<validate>& imm, const Value& dst,    \
    const Value& src, const Value& size)                                       \
  F(MemoryFill, const MemoryIndexImmediate<validate>& imm, const Value& dst,   \
    const Value& value, const Value& size)                                     \
  F(TableInit, const TableInitImmediate<validate>& imm, Vector<Value> args)    \
  F(ElemDrop, const ElemDropImmediate<validate>& imm)                          \
  F(TableCopy, const TableCopyImmediate<validate>& imm, Vector<Value> args)    \
  F(TableGrow, const TableIndexImmediate<validate>& imm, const Value& value,   \
    const Value& delta, Value* result)                                         \
  F(TableSize, const TableIndexImmediate<validate>& imm, Value* result)        \
  F(TableFill, const TableIndexImmediate<validate>& imm, const Value& start,   \
    const Value& value, const Value& count)                                    \
  F(StructNew, const StructIndexImmediate<validate>& imm, const Value args[],  \
    Value* result)                                                             \
  F(StructGet, const Value& struct_object,                                     \
    const FieldIndexImmediate<validate>& field, bool is_signed, Value* result) \
  F(StructSet, const Value& struct_object,                                     \
    const FieldIndexImmediate<validate>& field, const Value& field_value)      \
  F(ArrayNew, const ArrayIndexImmediate<validate>& imm, const Value& length,   \
    const Value& initial_value, Value* result)                                 \
  F(ArrayGet, const Value& array_obj,                                          \
    const ArrayIndexImmediate<validate>& imm, const Value& index,              \
    bool is_signed, Value* result)                                             \
  F(ArraySet, const Value& array_obj,                                          \
    const ArrayIndexImmediate<validate>& imm, const Value& index,              \
    const Value& value)                                                        \
  F(ArrayLen, const Value& array_obj, Value* result)                           \
  F(PassThrough, const Value& from, Value* to)

// Generic Wasm bytecode decoder with utilities for decoding immediates,
// lengths, etc.
template <Decoder::ValidateFlag validate>
class WasmDecoder : public Decoder {
 public:
  WasmDecoder(const WasmModule* module, const WasmFeatures& enabled,
              WasmFeatures* detected, const FunctionSig* sig, const byte* start,
              const byte* end, uint32_t buffer_offset = 0)
      : Decoder(start, end, buffer_offset),
        module_(module),
        enabled_(enabled),
        detected_(detected),
        sig_(sig),
        local_types_(nullptr) {}
  const WasmModule* module_;
  const WasmFeatures enabled_;
  WasmFeatures* detected_;
  const FunctionSig* sig_;

  ZoneVector<ValueType>* local_types_;

  uint32_t total_locals() const {
    return local_types_ == nullptr
               ? 0
               : static_cast<uint32_t>(local_types_->size());
  }

  void InitializeLocalsFromSig() {
    if (sig_ != nullptr) {
      local_types_->assign(sig_->parameters().begin(),
                           sig_->parameters().end());
    }
  }

  // Decodes local definitions in the current decoder.
  // Returns true iff locals are found.
  // Writes the total length of decoded locals in 'total_length'.
  // If insert_postion is present, the decoded locals will be inserted into the
  // 'local_types_' of this decoder. Otherwise, this function is used just to
  // check validity and determine the encoding length of the locals in bytes.
  // The decoder's pc is not advanced. If no locals are found (i.e., no
  // compressed uint32 is found at pc), this will exit as 'false' and without an
  // error.
  bool DecodeLocals(const byte* pc, uint32_t* total_length,
                    const base::Optional<uint32_t> insert_position) {
    DCHECK_NOT_NULL(local_types_);

    uint32_t length;
    *total_length = 0;

    // The 'else' value is useless, we pass it for convenience.
    ZoneVector<ValueType>::iterator insert_iterator =
        insert_position.has_value()
            ? local_types_->begin() + insert_position.value()
            : local_types_->begin();

    // Decode local declarations, if any.
    uint32_t entries = read_u32v<kValidate>(pc, &length, "local decls count");
    if (failed()) {
      error(pc + *total_length, "invalid local decls count");
      return false;
    }

    *total_length += length;
    TRACE("local decls count: %u\n", entries);

    while (entries-- > 0) {
      if (!more()) {
        error(end(), "expected more local decls but reached end of input");
        return false;
      }
      uint32_t count =
          read_u32v<kValidate>(pc + *total_length, &length, "local count");
      if (failed()) {
        error(pc + *total_length, "invalid local count");
        return false;
      }
      DCHECK_LE(local_types_->size(), kV8MaxWasmFunctionLocals);
      if (count > kV8MaxWasmFunctionLocals - local_types_->size()) {
        error(pc + *total_length, "local count too large");
        return false;
      }
      *total_length += length;

      ValueType type = value_type_reader::read_value_type<kValidate>(
          this, pc + *total_length, &length, enabled_);
      if (type == kWasmBottom) {
        error(pc + *total_length, "invalid local type");
        return false;
      }
      *total_length += length;
      if (insert_position.has_value()) {
        // Move the insertion iterator to the end of the newly inserted locals.
        insert_iterator =
            local_types_->insert(insert_iterator, count, type) + count;
      }
    }
    DCHECK(ok());
    return true;
  }

  static BitVector* AnalyzeLoopAssignment(WasmDecoder* decoder, const byte* pc,
                                          uint32_t locals_count, Zone* zone) {
    if (pc >= decoder->end()) return nullptr;
    if (*pc != kExprLoop) return nullptr;

    // The number of locals_count is augmented by 2 so that 'locals_count - 2'
    // can be used to track mem_size, and 'locals_count - 1' to track mem_start.
    BitVector* assigned = new (zone) BitVector(locals_count, zone);
    int depth = 0;
    // Iteratively process all AST nodes nested inside the loop.
    while (pc < decoder->end() && VALIDATE(decoder->ok())) {
      WasmOpcode opcode = static_cast<WasmOpcode>(*pc);
      uint32_t length = 1;
      switch (opcode) {
        case kExprLoop:
        case kExprIf:
        case kExprBlock:
        case kExprTry:
          length = OpcodeLength(decoder, pc);
          depth++;
          break;
        case kExprLocalSet:  // fallthru
        case kExprLocalTee: {
          LocalIndexImmediate<validate> imm(decoder, pc);
          if (assigned->length() > 0 &&
              imm.index < static_cast<uint32_t>(assigned->length())) {
            // Unverified code might have an out-of-bounds index.
            assigned->Add(imm.index);
          }
          length = 1 + imm.length;
          break;
        }
        case kExprMemoryGrow:
        case kExprCallFunction:
        case kExprCallIndirect:
        case kExprReturnCall:
        case kExprReturnCallIndirect:
          // Add instance cache nodes to the assigned set.
          // TODO(titzer): make this more clear.
          assigned->Add(locals_count - 1);
          length = OpcodeLength(decoder, pc);
          break;
        case kExprEnd:
          depth--;
          break;
        default:
          length = OpcodeLength(decoder, pc);
          break;
      }
      if (depth <= 0) break;
      pc += length;
    }
    return VALIDATE(decoder->ok()) ? assigned : nullptr;
  }

  inline bool Validate(const byte* pc, LocalIndexImmediate<validate>& imm) {
    if (!VALIDATE(imm.index < total_locals())) {
      errorf(pc + 1, "invalid local index: %u", imm.index);
      return false;
    }
    imm.type = local_types_ ? local_types_->at(imm.index) : kWasmStmt;
    return true;
  }

  inline bool Validate(const byte* pc, RefNullImmediate<validate>& imm) {
    if (!VALIDATE(imm.type.IsNullable())) {
      errorf(pc + 1, "ref.null does not exist for %s", imm.type.type_name());
      return false;
    }
    return true;
  }

  inline bool Complete(const byte* pc, ExceptionIndexImmediate<validate>& imm) {
    if (!VALIDATE(imm.index < module_->exceptions.size())) return false;
    imm.exception = &module_->exceptions[imm.index];
    return true;
  }

  inline bool Validate(const byte* pc, ExceptionIndexImmediate<validate>& imm) {
    if (!Complete(pc, imm)) {
      errorf(pc + 1, "Invalid exception index: %u", imm.index);
      return false;
    }
    return true;
  }

  inline bool Validate(const byte* pc, GlobalIndexImmediate<validate>& imm) {
    if (!VALIDATE(imm.index < module_->globals.size())) {
      errorf(pc + 1, "invalid global index: %u", imm.index);
      return false;
    }
    imm.global = &module_->globals[imm.index];
    imm.type = imm.global->type;
    return true;
  }

  inline bool Complete(const byte* pc, StructIndexImmediate<validate>& imm) {
    if (!VALIDATE(module_->has_struct(imm.index))) return false;
    imm.struct_type = module_->struct_type(imm.index);
    return true;
  }

  inline bool Validate(const byte* pc, StructIndexImmediate<validate>& imm) {
    if (Complete(pc, imm)) return true;
    errorf(pc, "invalid struct index: %u", imm.index);
    return false;
  }

  inline bool Validate(const byte* pc, FieldIndexImmediate<validate>& imm) {
    if (!Validate(pc, imm.struct_index)) return false;
    if (imm.index < imm.struct_index.struct_type->field_count()) return true;
    errorf(pc + imm.struct_index.length, "invalid field index: %u", imm.index);
    return false;
  }

  inline bool Complete(const byte* pc, ArrayIndexImmediate<validate>& imm) {
    if (!VALIDATE(module_->has_array(imm.index))) return false;
    imm.array_type = module_->array_type(imm.index);
    return true;
  }

  inline bool Validate(const byte* pc, ArrayIndexImmediate<validate>& imm) {
    if (Complete(pc, imm)) return true;
    errorf(pc, "invalid array index: %u", imm.index);
    return false;
  }

  inline bool CanReturnCall(const FunctionSig* target_sig) {
    if (target_sig == nullptr) return false;
    size_t num_returns = sig_->return_count();
    if (num_returns != target_sig->return_count()) return false;
    for (size_t i = 0; i < num_returns; ++i) {
      if (sig_->GetReturn(i) != target_sig->GetReturn(i)) return false;
    }
    return true;
  }

  inline bool Complete(const byte* pc, CallFunctionImmediate<validate>& imm) {
    if (!VALIDATE(imm.index < module_->functions.size())) return false;
    imm.sig = module_->functions[imm.index].sig;
    if (imm.sig->return_count() > 1) {
      this->detected_->Add(kFeature_mv);
    }
    return true;
  }

  inline bool Validate(const byte* pc, CallFunctionImmediate<validate>& imm) {
    if (Complete(pc, imm)) {
      return true;
    }
    errorf(pc + 1, "invalid function index: %u", imm.index);
    return false;
  }

  inline bool Complete(const byte* pc, CallIndirectImmediate<validate>& imm) {
    if (!VALIDATE(module_->has_signature(imm.sig_index))) return false;
    imm.sig = module_->signature(imm.sig_index);
    if (imm.sig->return_count() > 1) {
      this->detected_->Add(kFeature_mv);
    }
    return true;
  }

  inline bool Validate(const byte* pc, CallIndirectImmediate<validate>& imm) {
    if (!VALIDATE(imm.table_index < module_->tables.size())) {
      error("function table has to exist to execute call_indirect");
      return false;
    }
    if (!VALIDATE(module_->tables[imm.table_index].type == kWasmFuncRef)) {
      error("table of call_indirect must be of type funcref");
      return false;
    }
    if (!Complete(pc, imm)) {
      errorf(pc + 1, "invalid signature index: #%u", imm.sig_index);
      return false;
    }
    return true;
  }

  inline bool Validate(const byte* pc, BranchDepthImmediate<validate>& imm,
                       size_t control_depth) {
    if (!VALIDATE(imm.depth < control_depth)) {
      errorf(pc + 1, "invalid branch depth: %u", imm.depth);
      return false;
    }
    return true;
  }

  bool Validate(const byte* pc, BranchTableImmediate<validate>& imm,
                size_t block_depth) {
    if (!VALIDATE(imm.table_count <= kV8MaxWasmFunctionBrTableSize)) {
      errorf(pc + 1, "invalid table count (> max br_table size): %u",
             imm.table_count);
      return false;
    }
    return checkAvailable(imm.table_count);
  }

  inline bool Validate(const byte* pc, WasmOpcode opcode,
                       SimdLaneImmediate<validate>& imm) {
    uint8_t num_lanes = 0;
    switch (opcode) {
      case kExprF64x2ExtractLane:
      case kExprF64x2ReplaceLane:
      case kExprI64x2ExtractLane:
      case kExprI64x2ReplaceLane:
        num_lanes = 2;
        break;
      case kExprF32x4ExtractLane:
      case kExprF32x4ReplaceLane:
      case kExprI32x4ExtractLane:
      case kExprI32x4ReplaceLane:
        num_lanes = 4;
        break;
      case kExprI16x8ExtractLaneS:
      case kExprI16x8ExtractLaneU:
      case kExprI16x8ReplaceLane:
        num_lanes = 8;
        break;
      case kExprI8x16ExtractLaneS:
      case kExprI8x16ExtractLaneU:
      case kExprI8x16ReplaceLane:
        num_lanes = 16;
        break;
      default:
        UNREACHABLE();
        break;
    }
    if (!VALIDATE(imm.lane >= 0 && imm.lane < num_lanes)) {
      error(pc_ + 2, "invalid lane index");
      return false;
    } else {
      return true;
    }
  }

  inline bool Validate(const byte* pc,
                       Simd8x16ShuffleImmediate<validate>& imm) {
    uint8_t max_lane = 0;
    for (uint32_t i = 0; i < kSimd128Size; ++i) {
      max_lane = std::max(max_lane, imm.shuffle[i]);
    }
    // Shuffle indices must be in [0..31] for a 16 lane shuffle.
    if (!VALIDATE(max_lane <= 2 * kSimd128Size)) {
      error(pc_ + 2, "invalid shuffle mask");
      return false;
    }
    return true;
  }

  inline bool Complete(BlockTypeImmediate<validate>& imm) {
    if (imm.type != kWasmBottom) return true;
    if (!VALIDATE(module_->has_signature(imm.sig_index))) return false;
    imm.sig = module_->signature(imm.sig_index);
    if (imm.sig->return_count() > 1) {
      this->detected_->Add(kFeature_mv);
    }
    return true;
  }

  inline bool Validate(BlockTypeImmediate<validate>& imm) {
    if (!Complete(imm)) {
      errorf(pc_, "block type index %u out of bounds (%zu types)",
             imm.sig_index, module_->types.size());
      return false;
    }
    return true;
  }

  inline bool Validate(const byte* pc, FunctionIndexImmediate<validate>& imm) {
    if (!VALIDATE(imm.index < module_->functions.size())) {
      errorf(pc, "invalid function index: %u", imm.index);
      return false;
    }
    if (!VALIDATE(module_->functions[imm.index].declared)) {
      this->errorf(pc, "undeclared reference to function #%u", imm.index);
      return false;
    }
    return true;
  }

  inline bool Validate(const byte* pc, MemoryIndexImmediate<validate>& imm) {
    if (!VALIDATE(module_->has_memory)) {
      errorf(pc + 1, "memory instruction with no memory");
      return false;
    }
    return true;
  }

  inline bool Validate(MemoryInitImmediate<validate>& imm) {
    if (!VALIDATE(imm.data_segment_index <
                  module_->num_declared_data_segments)) {
      errorf(pc_ + 2, "invalid data segment index: %u", imm.data_segment_index);
      return false;
    }
    if (!Validate(pc_ + imm.length - imm.memory.length - 1, imm.memory))
      return false;
    return true;
  }

  inline bool Validate(DataDropImmediate<validate>& imm) {
    if (!VALIDATE(imm.index < module_->num_declared_data_segments)) {
      errorf(pc_ + 2, "invalid data segment index: %u", imm.index);
      return false;
    }
    return true;
  }

  inline bool Validate(MemoryCopyImmediate<validate>& imm) {
    if (!Validate(pc_ + 1, imm.memory_src)) return false;
    if (!Validate(pc_ + 2, imm.memory_dst)) return false;
    return true;
  }

  inline bool Validate(const byte* pc, TableIndexImmediate<validate>& imm) {
    if (!VALIDATE(imm.index < module_->tables.size())) {
      errorf(pc, "invalid table index: %u", imm.index);
      return false;
    }
    return true;
  }

  inline bool Validate(TableInitImmediate<validate>& imm) {
    if (!VALIDATE(imm.elem_segment_index < module_->elem_segments.size())) {
      errorf(pc_ + 2, "invalid element segment index: %u",
             imm.elem_segment_index);
      return false;
    }
    if (!Validate(pc_ + imm.length - imm.table.length - 1, imm.table)) {
      return false;
    }
    ValueType elem_type = module_->elem_segments[imm.elem_segment_index].type;
    if (!VALIDATE(IsSubtypeOf(elem_type, module_->tables[imm.table.index].type,
                              module_))) {
      errorf(pc_ + 2, "table %u is not a super-type of %s", imm.table.index,
             elem_type.type_name());
      return false;
    }
    return true;
  }

  inline bool Validate(ElemDropImmediate<validate>& imm) {
    if (!VALIDATE(imm.index < module_->elem_segments.size())) {
      errorf(pc_ + 2, "invalid element segment index: %u", imm.index);
      return false;
    }
    return true;
  }

  inline bool Validate(TableCopyImmediate<validate>& imm) {
    if (!Validate(pc_ + 1, imm.table_src)) return false;
    if (!Validate(pc_ + 2, imm.table_dst)) return false;
    ValueType src_type = module_->tables[imm.table_src.index].type;
    if (!VALIDATE(IsSubtypeOf(
            src_type, module_->tables[imm.table_dst.index].type, module_))) {
      errorf(pc_ + 2, "table %u is not a super-type of %s", imm.table_dst.index,
             src_type.type_name());
      return false;
    }
    return true;
  }

  static uint32_t OpcodeLength(WasmDecoder* decoder, const byte* pc) {
    WasmOpcode opcode = static_cast<WasmOpcode>(*pc);
    switch (opcode) {
#define DECLARE_OPCODE_CASE(name, opcode, sig) case kExpr##name:
      FOREACH_LOAD_MEM_OPCODE(DECLARE_OPCODE_CASE)
      FOREACH_STORE_MEM_OPCODE(DECLARE_OPCODE_CASE)
#undef DECLARE_OPCODE_CASE
      {
        MemoryAccessImmediate<validate> imm(decoder, pc, UINT32_MAX);
        return 1 + imm.length;
      }
      case kExprBr:
      case kExprBrIf: {
        BranchDepthImmediate<validate> imm(decoder, pc);
        return 1 + imm.length;
      }
      case kExprGlobalGet:
      case kExprGlobalSet: {
        GlobalIndexImmediate<validate> imm(decoder, pc);
        return 1 + imm.length;
      }
      case kExprTableGet:
      case kExprTableSet: {
        TableIndexImmediate<validate> imm(decoder, pc);
        return 1 + imm.length;
      }
      case kExprCallFunction:
      case kExprReturnCall: {
        CallFunctionImmediate<validate> imm(decoder, pc);
        return 1 + imm.length;
      }
      case kExprCallIndirect:
      case kExprReturnCallIndirect: {
        CallIndirectImmediate<validate> imm(WasmFeatures::All(), decoder, pc);
        return 1 + imm.length;
      }

      case kExprTry:
      case kExprIf:  // fall through
      case kExprLoop:
      case kExprBlock: {
        BlockTypeImmediate<validate> imm(WasmFeatures::All(), decoder, pc);
        return 1 + imm.length;
      }

      case kExprLet: {
        BlockTypeImmediate<validate> imm(WasmFeatures::All(), decoder, pc);
        uint32_t locals_length;
        bool locals_result =
            decoder->DecodeLocals(decoder->pc() + 1 + imm.length,
                                  &locals_length, base::Optional<uint32_t>());
        return 1 + imm.length + (locals_result ? locals_length : 0);
      }

      case kExprThrow: {
        ExceptionIndexImmediate<validate> imm(decoder, pc);
        return 1 + imm.length;
      }

      case kExprBrOnExn: {
        BranchOnExceptionImmediate<validate> imm(decoder, pc);
        return 1 + imm.length;
      }

      case kExprBrOnNull: {
        BranchDepthImmediate<validate> imm(decoder, pc);
        return 1 + imm.length;
      }

      case kExprLocalGet:
      case kExprLocalSet:
      case kExprLocalTee: {
        LocalIndexImmediate<validate> imm(decoder, pc);
        return 1 + imm.length;
      }
      case kExprSelectWithType: {
        SelectTypeImmediate<validate> imm(WasmFeatures::All(), decoder, pc);
        return 1 + imm.length;
      }
      case kExprBrTable: {
        BranchTableImmediate<validate> imm(decoder, pc);
        BranchTableIterator<validate> iterator(decoder, imm);
        return 1 + iterator.length();
      }
      case kExprI32Const: {
        ImmI32Immediate<validate> imm(decoder, pc);
        return 1 + imm.length;
      }
      case kExprI64Const: {
        ImmI64Immediate<validate> imm(decoder, pc);
        return 1 + imm.length;
      }
      case kExprRefNull: {
        RefNullImmediate<validate> imm(WasmFeatures::All(), decoder, pc);
        return 1 + imm.length;
      }
      case kExprRefIsNull: {
        RefNullImmediate<validate> imm(WasmFeatures::All(), decoder, pc);
        return 1 + imm.length;
      }
      case kExprRefFunc: {
        FunctionIndexImmediate<validate> imm(decoder, pc);
        return 1 + imm.length;
      }
      case kExprMemoryGrow:
      case kExprMemorySize: {
        MemoryIndexImmediate<validate> imm(decoder, pc);
        return 1 + imm.length;
      }
      case kExprF32Const:
        return 5;
      case kExprF64Const:
        return 9;
      case kNumericPrefix: {
        byte numeric_index =
            decoder->read_u8<validate>(pc + 1, "numeric_index");
        WasmOpcode opcode =
            static_cast<WasmOpcode>(kNumericPrefix << 8 | numeric_index);
        switch (opcode) {
          case kExprI32SConvertSatF32:
          case kExprI32UConvertSatF32:
          case kExprI32SConvertSatF64:
          case kExprI32UConvertSatF64:
          case kExprI64SConvertSatF32:
          case kExprI64UConvertSatF32:
          case kExprI64SConvertSatF64:
          case kExprI64UConvertSatF64:
            return 2;
          case kExprMemoryInit: {
            MemoryInitImmediate<validate> imm(decoder, pc);
            return 2 + imm.length;
          }
          case kExprDataDrop: {
            DataDropImmediate<validate> imm(decoder, pc);
            return 2 + imm.length;
          }
          case kExprMemoryCopy: {
            MemoryCopyImmediate<validate> imm(decoder, pc);
            return 2 + imm.length;
          }
          case kExprMemoryFill: {
            MemoryIndexImmediate<validate> imm(decoder, pc + 1);
            return 2 + imm.length;
          }
          case kExprTableInit: {
            TableInitImmediate<validate> imm(decoder, pc);
            return 2 + imm.length;
          }
          case kExprElemDrop: {
            ElemDropImmediate<validate> imm(decoder, pc);
            return 2 + imm.length;
          }
          case kExprTableCopy: {
            TableCopyImmediate<validate> imm(decoder, pc);
            return 2 + imm.length;
          }
          case kExprTableGrow:
          case kExprTableSize:
          case kExprTableFill: {
            TableIndexImmediate<validate> imm(decoder, pc);
            return 2 + imm.length;
          }
          default:
            decoder->error(pc, "invalid numeric opcode");
            return 2;
        }
      }
      case kSimdPrefix: {
        uint32_t length = 0;
        opcode = decoder->read_prefixed_opcode<validate>(pc, &length);
        switch (opcode) {
#define DECLARE_OPCODE_CASE(name, opcode, sig) case kExpr##name:
          FOREACH_SIMD_0_OPERAND_OPCODE(DECLARE_OPCODE_CASE)
#undef DECLARE_OPCODE_CASE
          return 1 + length;
#define DECLARE_OPCODE_CASE(name, opcode, sig) case kExpr##name:
          FOREACH_SIMD_1_OPERAND_OPCODE(DECLARE_OPCODE_CASE)
#undef DECLARE_OPCODE_CASE
          return 2 + length;
#define DECLARE_OPCODE_CASE(name, opcode, sig) case kExpr##name:
          FOREACH_SIMD_MEM_OPCODE(DECLARE_OPCODE_CASE)
#undef DECLARE_OPCODE_CASE
          {
            MemoryAccessImmediate<validate> imm(decoder, pc + length,
                                                UINT32_MAX);
            return 1 + length + imm.length;
          }
          // Shuffles require a byte per lane, or 16 immediate bytes.
          case kExprS8x16Shuffle:
            return 1 + length + kSimd128Size;
          default:
            decoder->error(pc, "invalid SIMD opcode");
            return 1 + length;
        }
      }
      case kAtomicPrefix: {
        byte atomic_index = decoder->read_u8<validate>(pc + 1, "atomic_index");
        WasmOpcode opcode =
            static_cast<WasmOpcode>(kAtomicPrefix << 8 | atomic_index);
        switch (opcode) {
#define DECLARE_OPCODE_CASE(name, opcode, sig) case kExpr##name:
          FOREACH_ATOMIC_OPCODE(DECLARE_OPCODE_CASE)
#undef DECLARE_OPCODE_CASE
          {
            MemoryAccessImmediate<validate> imm(decoder, pc + 1, UINT32_MAX);
            return 2 + imm.length;
          }
#define DECLARE_OPCODE_CASE(name, opcode, sig) case kExpr##name:
          FOREACH_ATOMIC_0_OPERAND_OPCODE(DECLARE_OPCODE_CASE)
#undef DECLARE_OPCODE_CASE
          {
            return 2 + 1;
          }
          default:
            decoder->error(pc, "invalid Atomics opcode");
            return 2;
        }
      }
      case kGCPrefix: {
        byte gc_index = decoder->read_u8<validate>(pc + 1, "gc_index");
        WasmOpcode opcode = static_cast<WasmOpcode>(kGCPrefix << 8 | gc_index);
        switch (opcode) {
          case kExprStructNew:
          case kExprStructNewSub:
          case kExprStructNewDefault: {
            StructIndexImmediate<validate> imm(decoder, pc + 2);
            return 2 + imm.length;
          }
          case kExprStructGet:
          case kExprStructGetS:
          case kExprStructGetU:
          case kExprStructSet: {
            FieldIndexImmediate<validate> imm(decoder, pc + 2);
            return 2 + imm.length;
          }
          case kExprArrayNew:
          case kExprArrayNewSub:
          case kExprArrayNewDefault:
          case kExprArrayGet:
          case kExprArrayGetS:
          case kExprArrayGetU:
          case kExprArraySet:
          case kExprArrayLen: {
            ArrayIndexImmediate<validate> imm(decoder, pc + 2);
            return 2 + imm.length;
          }
          case kExprBrOnCast: {
            BranchDepthImmediate<validate> imm(decoder, pc + 2);
            return 2 + imm.length;
          }
          case kExprRttGet:
          case kExprRttSub: {
            // TODO(7748): Implement.
            decoder->error(pc, "rtt opcodes not implemented yet");
            return 2;
          }

          case kExprI31New:
          case kExprI31GetS:
          case kExprI31GetU:
          case kExprRefTest:
          case kExprRefCast:
            return 2;

          default:
            // This is unreachable except for malformed modules.
            decoder->error(pc, "invalid gc opcode");
            return 2;
        }
      }
      default:
        return 1;
    }
  }

  std::pair<uint32_t, uint32_t> StackEffect(const byte* pc) {
    WasmOpcode opcode = static_cast<WasmOpcode>(*pc);
    // Handle "simple" opcodes with a fixed signature first.
    const FunctionSig* sig = WasmOpcodes::Signature(opcode);
    if (!sig) sig = WasmOpcodes::AsmjsSignature(opcode);
    if (sig) return {sig->parameter_count(), sig->return_count()};

#define DECLARE_OPCODE_CASE(name, opcode, sig) case kExpr##name:
    // clang-format off
    switch (opcode) {
      case kExprSelect:
      case kExprSelectWithType:
        return {3, 1};
      case kExprTableSet:
      FOREACH_STORE_MEM_OPCODE(DECLARE_OPCODE_CASE)
        return {2, 0};
      FOREACH_LOAD_MEM_OPCODE(DECLARE_OPCODE_CASE)
      case kExprTableGet:
      case kExprLocalTee:
      case kExprMemoryGrow:
      case kExprRefAsNonNull:
      case kExprBrOnNull:
      case kExprRefIsNull:
        return {1, 1};
      case kExprLocalSet:
      case kExprGlobalSet:
      case kExprDrop:
      case kExprBrIf:
      case kExprBrTable:
      case kExprIf:
      case kExprRethrow:
        return {1, 0};
      case kExprLocalGet:
      case kExprGlobalGet:
      case kExprI32Const:
      case kExprI64Const:
      case kExprF32Const:
      case kExprF64Const:
      case kExprRefNull:
      case kExprRefFunc:
      case kExprMemorySize:
        return {0, 1};
      case kExprCallFunction: {
        CallFunctionImmediate<validate> imm(this, pc);
        CHECK(Complete(pc, imm));
        return {imm.sig->parameter_count(), imm.sig->return_count()};
      }
      case kExprCallIndirect: {
        CallIndirectImmediate<validate> imm(this->enabled_, this, pc);
        CHECK(Complete(pc, imm));
        // Indirect calls pop an additional argument for the table index.
        return {imm.sig->parameter_count() + 1,
                imm.sig->return_count()};
      }
      case kExprThrow: {
        ExceptionIndexImmediate<validate> imm(this, pc);
        CHECK(Complete(pc, imm));
        DCHECK_EQ(0, imm.exception->sig->return_count());
        return {imm.exception->sig->parameter_count(), 0};
      }
      case kExprBr:
      case kExprBlock:
      case kExprLoop:
      case kExprEnd:
      case kExprElse:
      case kExprTry:
      case kExprCatch:
      case kExprBrOnExn:
      case kExprNop:
      case kExprReturn:
      case kExprReturnCall:
      case kExprReturnCallIndirect:
      case kExprUnreachable:
        return {0, 0};
      case kExprLet:
        // TODO(7748): Implement
        return {0, 0};
      case kNumericPrefix:
      case kAtomicPrefix:
      case kSimdPrefix: {
        opcode = this->read_prefixed_opcode<validate>(pc);
        switch (opcode) {
          FOREACH_SIMD_1_OPERAND_1_PARAM_OPCODE(DECLARE_OPCODE_CASE)
            return {1, 1};
          FOREACH_SIMD_1_OPERAND_2_PARAM_OPCODE(DECLARE_OPCODE_CASE)
          FOREACH_SIMD_MASK_OPERAND_OPCODE(DECLARE_OPCODE_CASE)
            return {2, 1};
          default: {
            sig = WasmOpcodes::Signature(opcode);
            if (sig) {
              return {sig->parameter_count(), sig->return_count()};
            }
          }
        }
        V8_FALLTHROUGH;
      }
      default:
        FATAL("unimplemented opcode: %x (%s)", opcode,
              WasmOpcodes::OpcodeName(opcode));
        return {0, 0};
    }
#undef DECLARE_OPCODE_CASE
    // clang-format on
  }
};

#define CALL_INTERFACE(name, ...) interface_.name(this, ##__VA_ARGS__)
#define CALL_INTERFACE_IF_REACHABLE(name, ...)            \
  do {                                                    \
    DCHECK(!control_.empty());                            \
    DCHECK_EQ(current_code_reachable_,                    \
              this->ok() && control_.back().reachable()); \
    if (current_code_reachable_) {                        \
      interface_.name(this, ##__VA_ARGS__);               \
    }                                                     \
  } while (false)
#define CALL_INTERFACE_IF_PARENT_REACHABLE(name, ...)           \
  do {                                                          \
    DCHECK(!control_.empty());                                  \
    if (VALIDATE(this->ok()) &&                                 \
        (control_.size() == 1 || control_at(1)->reachable())) { \
      interface_.name(this, ##__VA_ARGS__);                     \
    }                                                           \
  } while (false)

template <Decoder::ValidateFlag validate, typename Interface>
class WasmFullDecoder : public WasmDecoder<validate> {
  using Value = typename Interface::Value;
  using Control = typename Interface::Control;
  using ArgVector = base::SmallVector<Value, 8>;

  // All Value types should be trivially copyable for performance. We push, pop,
  // and store them in local variables.
  ASSERT_TRIVIALLY_COPYABLE(Value);

 public:
  template <typename... InterfaceArgs>
  WasmFullDecoder(Zone* zone, const WasmModule* module,
                  const WasmFeatures& enabled, WasmFeatures* detected,
                  const FunctionBody& body, InterfaceArgs&&... interface_args)
      : WasmDecoder<validate>(module, enabled, detected, body.sig, body.start,
                              body.end, body.offset),
        zone_(zone),
        interface_(std::forward<InterfaceArgs>(interface_args)...),
        local_type_vec_(zone),
        stack_(zone),
        control_(zone) {
    this->local_types_ = &local_type_vec_;
  }

  Interface& interface() { return interface_; }

  bool Decode() {
    DCHECK(stack_.empty());
    DCHECK(control_.empty());

    if (this->end_ < this->pc_) {
      this->error("function body end < start");
      return false;
    }

    DCHECK_EQ(0, this->local_types_->size());
    this->InitializeLocalsFromSig();
    uint32_t locals_length;
    this->DecodeLocals(this->pc(), &locals_length,
                       static_cast<uint32_t>(this->local_types_->size()));
    this->consume_bytes(locals_length);

    CALL_INTERFACE(StartFunction);
    DecodeFunctionBody();
    if (!this->failed()) CALL_INTERFACE(FinishFunction);

    // Generate a better error message whether the unterminated control
    // structure is the function body block or an innner structure.
    if (control_.size() > 1) {
      this->error(control_.back().pc, "unterminated control structure");
    } else if (control_.size() == 1) {
      this->error("function body must end with \"end\" opcode");
    }

    if (this->failed()) return this->TraceFailed();

    TRACE("wasm-decode %s\n\n", VALIDATE(this->ok()) ? "ok" : "failed");

    return true;
  }

  bool TraceFailed() {
    TRACE("wasm-error module+%-6d func+%d: %s\n\n", this->error_.offset(),
          this->GetBufferRelativeOffset(this->error_.offset()),
          this->error_.message().c_str());
    return false;
  }

  const char* SafeOpcodeNameAt(const byte* pc) {
    if (pc >= this->end_) return "<end>";
    WasmOpcode opcode = static_cast<WasmOpcode>(*pc);
    if (!WasmOpcodes::IsPrefixOpcode(opcode)) {
      return WasmOpcodes::OpcodeName(static_cast<WasmOpcode>(opcode));
    }
    opcode = this->template read_prefixed_opcode<Decoder::kValidate>(pc);
    return WasmOpcodes::OpcodeName(opcode);
  }

  inline Zone* zone() const { return zone_; }

  inline uint32_t num_locals() const {
    return static_cast<uint32_t>(local_type_vec_.size());
  }

  inline ValueType GetLocalType(uint32_t index) {
    return local_type_vec_[index];
  }

  inline WasmCodePosition position() {
    int offset = static_cast<int>(this->pc_ - this->start_);
    DCHECK_EQ(this->pc_ - this->start_, offset);  // overflows cannot happen
    return offset;
  }

  inline uint32_t control_depth() const {
    return static_cast<uint32_t>(control_.size());
  }

  inline Control* control_at(uint32_t depth) {
    DCHECK_GT(control_.size(), depth);
    return &control_.back() - depth;
  }

  inline uint32_t stack_size() const {
    DCHECK_GE(kMaxUInt32, stack_.size());
    return static_cast<uint32_t>(stack_.size());
  }

  inline Value* stack_value(uint32_t depth) {
    DCHECK_LT(0, depth);
    DCHECK_GE(stack_.size(), depth);
    return &*(stack_.end() - depth);
  }

  void SetSucceedingCodeDynamicallyUnreachable() {
    Control* current = &control_.back();
    if (current->reachable()) {
      current->reachability = kSpecOnlyReachable;
      current_code_reachable_ = false;
    }
  }

 private:
  Zone* zone_;

  Interface interface_;

  ZoneVector<ValueType> local_type_vec_;  // types of local variables.
  ZoneVector<Value> stack_;               // stack of values.
  ZoneVector<Control> control_;           // stack of blocks, loops, and ifs.
  // Controls whether code should be generated for the current block (basically
  // a cache for {ok() && control_.back().reachable()}).
  bool current_code_reachable_ = true;

  static Value UnreachableValue(const uint8_t* pc) {
    return Value{pc, kWasmBottom};
  }

  bool CheckHasMemory() {
    if (!VALIDATE(this->module_->has_memory)) {
      this->error(this->pc_ - 1, "memory instruction with no memory");
      return false;
    }
    return true;
  }

  bool CheckHasMemoryForAtomics() {
    if (FLAG_wasm_atomics_on_non_shared_memory && CheckHasMemory()) return true;
    if (!VALIDATE(this->module_->has_shared_memory)) {
      this->error(this->pc_ - 1, "Atomic opcodes used without shared memory");
      return false;
    }
    return true;
  }

  class TraceLine {
   public:
    static constexpr int kMaxLen = 512;
    ~TraceLine() {
      if (!FLAG_trace_wasm_decoder) return;
      PrintF("%.*s\n", len_, buffer_);
    }

    // Appends a formatted string.
    PRINTF_FORMAT(2, 3)
    void Append(const char* format, ...) {
      if (!FLAG_trace_wasm_decoder) return;
      va_list va_args;
      va_start(va_args, format);
      size_t remaining_len = kMaxLen - len_;
      Vector<char> remaining_msg_space(buffer_ + len_, remaining_len);
      int len = VSNPrintF(remaining_msg_space, format, va_args);
      va_end(va_args);
      len_ += len < 0 ? remaining_len : len;
    }

   private:
    char buffer_[kMaxLen];
    int len_ = 0;
  };

  // Decodes the body of a function.
  void DecodeFunctionBody() {
    TRACE("wasm-decode %p...%p (module+%u, %d bytes)\n", this->start(),
          this->end(), this->pc_offset(),
          static_cast<int>(this->end() - this->start()));

    // Set up initial function block.
    {
      Control* c = PushControl(kControlBlock);
      InitMerge(&c->start_merge, 0, [](uint32_t) -> Value { UNREACHABLE(); });
      InitMerge(&c->end_merge,
                static_cast<uint32_t>(this->sig_->return_count()),
                [&](uint32_t i) {
                  return Value{this->pc_, this->sig_->GetReturn(i)};
                });
      CALL_INTERFACE(StartFunctionBody, c);
    }

    while (this->pc_ < this->end_) {  // decoding loop.
      uint32_t len = 1;
      WasmOpcode opcode = static_cast<WasmOpcode>(*this->pc_);

      CALL_INTERFACE_IF_REACHABLE(NextInstruction, opcode);

#if DEBUG
      TraceLine trace_msg;
#define TRACE_PART(...) trace_msg.Append(__VA_ARGS__)
      if (!WasmOpcodes::IsPrefixOpcode(opcode)) {
        TRACE_PART(TRACE_INST_FORMAT, startrel(this->pc_),
                   WasmOpcodes::OpcodeName(opcode));
      }
#else
#define TRACE_PART(...)
#endif

      switch (opcode) {
#define BUILD_SIMPLE_OPCODE(op, _, sig) \
  case kExpr##op:                       \
    BuildSimpleOperator_##sig(opcode);  \
    break;
        FOREACH_SIMPLE_OPCODE(BUILD_SIMPLE_OPCODE)
#undef BUILD_SIMPLE_OPCODE
        case kExprNop:
          break;
        case kExprBlock: {
          BlockTypeImmediate<validate> imm(this->enabled_, this, this->pc_);
          if (!this->Validate(imm)) break;
          ArgVector args = PopArgs(imm.sig);
          Control* block = PushControl(kControlBlock);
          SetBlockType(block, imm, args.begin());
          CALL_INTERFACE_IF_REACHABLE(Block, block);
          PushMergeValues(block, &block->start_merge);
          len = 1 + imm.length;
          break;
        }
        case kExprRethrow: {
          CHECK_PROTOTYPE_OPCODE(eh);
          Value exception = Pop(0, kWasmExnRef);
          CALL_INTERFACE_IF_REACHABLE(Rethrow, exception);
          EndControl();
          break;
        }
        case kExprThrow: {
          CHECK_PROTOTYPE_OPCODE(eh);
          ExceptionIndexImmediate<validate> imm(this, this->pc_);
          len = 1 + imm.length;
          if (!this->Validate(this->pc_, imm)) break;
          ArgVector args = PopArgs(imm.exception->ToFunctionSig());
          CALL_INTERFACE_IF_REACHABLE(Throw, imm, VectorOf(args));
          EndControl();
          break;
        }
        case kExprTry: {
          CHECK_PROTOTYPE_OPCODE(eh);
          BlockTypeImmediate<validate> imm(this->enabled_, this, this->pc_);
          if (!this->Validate(imm)) break;
          ArgVector args = PopArgs(imm.sig);
          Control* try_block = PushControl(kControlTry);
          SetBlockType(try_block, imm, args.begin());
          len = 1 + imm.length;
          CALL_INTERFACE_IF_REACHABLE(Try, try_block);
          PushMergeValues(try_block, &try_block->start_merge);
          break;
        }
        case kExprCatch: {
          CHECK_PROTOTYPE_OPCODE(eh);
          if (!VALIDATE(!control_.empty())) {
            this->error("catch does not match any try");
            break;
          }
          Control* c = &control_.back();
          if (!VALIDATE(c->is_try())) {
            this->error("catch does not match any try");
            break;
          }
          if (!VALIDATE(c->is_incomplete_try())) {
            this->error("catch already present for try");
            break;
          }
          c->kind = kControlTryCatch;
          FallThruTo(c);
          stack_.erase(stack_.begin() + c->stack_depth, stack_.end());
          c->reachability = control_at(1)->innerReachability();
          current_code_reachable_ = this->ok() && c->reachable();
          Value* exception = Push(kWasmExnRef);
          CALL_INTERFACE_IF_PARENT_REACHABLE(Catch, c, exception);
          break;
        }
        case kExprBrOnExn: {
          CHECK_PROTOTYPE_OPCODE(eh);
          BranchOnExceptionImmediate<validate> imm(this, this->pc_);
          if (!this->Validate(this->pc_, imm.depth, control_.size())) break;
          if (!this->Validate(this->pc_ + imm.depth.length, imm.index)) break;
          Control* c = control_at(imm.depth.depth);
          Value exception = Pop(0, kWasmExnRef);
          const WasmExceptionSig* sig = imm.index.exception->sig;
          size_t value_count = sig->parameter_count();
          // TODO(wasm): This operand stack mutation is an ugly hack to make
          // both type checking here as well as environment merging in the
          // graph builder interface work out of the box. We should introduce
          // special handling for both and do minimal/no stack mutation here.
          for (size_t i = 0; i < value_count; ++i) Push(sig->GetParam(i));
          Vector<Value> values(stack_.data() + c->stack_depth, value_count);
          TypeCheckBranchResult check_result = TypeCheckBranch(c, true);
          if (this->failed()) break;
          if (V8_LIKELY(check_result == kReachableBranch)) {
            CALL_INTERFACE(BrOnException, exception, imm.index, imm.depth.depth,
                           values);
            c->br_merge()->reached = true;
          } else if (check_result == kInvalidStack) {
            break;
          }
          len = 1 + imm.length;
          for (size_t i = 0; i < value_count; ++i) Pop();
          Value* pexception = Push(kWasmExnRef);
          *pexception = exception;
          break;
        }
        case kExprBrOnNull: {
          CHECK_PROTOTYPE_OPCODE(typed_funcref);
          BranchDepthImmediate<validate> imm(this, this->pc_);
          if (!this->Validate(this->pc_, imm, control_.size())) break;
          len = 1 + imm.length;
          Value ref_object = Pop();
          if (this->failed()) break;
          Control* c = control_at(imm.depth);
          TypeCheckBranchResult check_result = TypeCheckBranch(c, true);
          if (V8_LIKELY(check_result == kReachableBranch)) {
            switch (ref_object.type.kind()) {
              case ValueType::kRef: {
                Value* result = Push(
                    ValueType(ValueType::kRef, ref_object.type.ref_index()));
                CALL_INTERFACE(PassThrough, ref_object, result);
                break;
              }
              case ValueType::kOptRef: {
                // We need to Push the result value after calling BrOnNull on
                // the interface. Therefore we must sync the ref_object and
                // result nodes afterwards (in PassThrough).
                CALL_INTERFACE(BrOnNull, ref_object, imm.depth);
                Value* result = Push(
                    ValueType(ValueType::kRef, ref_object.type.ref_index()));
                CALL_INTERFACE(PassThrough, ref_object, result);
                c->br_merge()->reached = true;
                break;
              }
              default:
                this->error(this->pc_,
                            "invalid agrument type to ref.as_non_null");
                break;
            }
          }
          break;
        }
        case kExprLet: {
          CHECK_PROTOTYPE_OPCODE(typed_funcref);
          BlockTypeImmediate<validate> imm(this->enabled_, this, this->pc_);
          if (!this->Validate(imm)) break;
          uint32_t current_local_count =
              static_cast<uint32_t>(local_type_vec_.size());
          // Temporarily add the let-defined values
          // to the beginning of the function locals.
          uint32_t locals_length;
          if (!this->DecodeLocals(this->pc() + 1 + imm.length, &locals_length,
                                  0)) {
            break;
          }
          len = 1 + imm.length + locals_length;
          uint32_t locals_count = static_cast<uint32_t>(local_type_vec_.size() -
                                                        current_local_count);
          ArgVector let_local_values =
              PopArgs(static_cast<uint32_t>(imm.in_arity()),
                      VectorOf(local_type_vec_.data(), locals_count));
          ArgVector args = PopArgs(imm.sig);
          Control* let_block = PushControl(kControlLet, locals_count);
          SetBlockType(let_block, imm, args.begin());
          CALL_INTERFACE_IF_REACHABLE(Block, let_block);
          PushMergeValues(let_block, &let_block->start_merge);
          CALL_INTERFACE_IF_REACHABLE(AllocateLocals,
                                      VectorOf(let_local_values));
          break;
        }
        case kExprLoop: {
          BlockTypeImmediate<validate> imm(this->enabled_, this, this->pc_);
          if (!this->Validate(imm)) break;
          ArgVector args = PopArgs(imm.sig);
          Control* block = PushControl(kControlLoop);
          SetBlockType(&control_.back(), imm, args.begin());
          len = 1 + imm.length;
          CALL_INTERFACE_IF_REACHABLE(Loop, block);
          PushMergeValues(block, &block->start_merge);
          break;
        }
        case kExprIf: {
          BlockTypeImmediate<validate> imm(this->enabled_, this, this->pc_);
          if (!this->Validate(imm)) break;
          Value cond = Pop(0, kWasmI32);
          ArgVector args = PopArgs(imm.sig);
          if (!VALIDATE(this->ok())) break;
          Control* if_block = PushControl(kControlIf);
          SetBlockType(if_block, imm, args.begin());
          CALL_INTERFACE_IF_REACHABLE(If, cond, if_block);
          len = 1 + imm.length;
          PushMergeValues(if_block, &if_block->start_merge);
          break;
        }
        case kExprElse: {
          if (!VALIDATE(!control_.empty())) {
            this->error("else does not match any if");
            break;
          }
          Control* c = &control_.back();
          if (!VALIDATE(c->is_if())) {
            this->error(this->pc_, "else does not match an if");
            break;
          }
          if (c->is_if_else()) {
            this->error(this->pc_, "else already present for if");
            break;
          }
          if (!TypeCheckFallThru()) break;
          c->kind = kControlIfElse;
          CALL_INTERFACE_IF_PARENT_REACHABLE(Else, c);
          if (c->reachable()) c->end_merge.reached = true;
          PushMergeValues(c, &c->start_merge);
          c->reachability = control_at(1)->innerReachability();
          current_code_reachable_ = this->ok() && c->reachable();
          break;
        }
        case kExprEnd: {
          if (!VALIDATE(!control_.empty())) {
            this->error("end does not match any if, try, or block");
            break;
          }
          Control* c = &control_.back();
          if (!VALIDATE(!c->is_incomplete_try())) {
            this->error(this->pc_, "missing catch or catch-all in try");
            break;
          }
          if (c->is_onearmed_if()) {
            if (!VALIDATE(c->end_merge.arity == c->start_merge.arity)) {
              this->error(
                  c->pc,
                  "start-arity and end-arity of one-armed if must match");
              break;
            }
            if (!TypeCheckOneArmedIf(c)) break;
          }
          if (c->is_let()) {
            this->local_types_->erase(
                this->local_types_->begin(),
                this->local_types_->begin() + c->locals_count);
            CALL_INTERFACE_IF_REACHABLE(DeallocateLocals, c->locals_count);
          }
          if (!TypeCheckFallThru()) break;

          if (control_.size() == 1) {
            // If at the last (implicit) control, check we are at end.
            if (!VALIDATE(this->pc_ + 1 == this->end_)) {
              this->error(this->pc_ + 1, "trailing code after function end");
              break;
            }
            // The result of the block is the return value.
            TRACE_PART("\n" TRACE_INST_FORMAT, startrel(this->pc_),
                       "(implicit) return");
            DoReturn();
            control_.clear();
            break;
          }
          PopControl(c);
          break;
        }
        case kExprSelect: {
          Value cond = Pop(2, kWasmI32);
          Value fval = Pop();
          Value tval = Pop(0, fval.type);
          ValueType type = tval.type == kWasmBottom ? fval.type : tval.type;
          if (type.IsReferenceType()) {
            this->error(
                "select without type is only valid for value type inputs");
            break;
          }
          Value* result = Push(type);
          CALL_INTERFACE_IF_REACHABLE(Select, cond, fval, tval, result);
          break;
        }
        case kExprSelectWithType: {
          CHECK_PROTOTYPE_OPCODE(reftypes);
          SelectTypeImmediate<validate> imm(this->enabled_, this, this->pc_);
          if (this->failed()) break;
          Value cond = Pop(2, kWasmI32);
          Value fval = Pop(1, imm.type);
          Value tval = Pop(0, imm.type);
          Value* result = Push(imm.type);
          CALL_INTERFACE_IF_REACHABLE(Select, cond, fval, tval, result);
          len = 1 + imm.length;
          break;
        }
        case kExprBr: {
          BranchDepthImmediate<validate> imm(this, this->pc_);
          if (!this->Validate(this->pc_, imm, control_.size())) break;
          Control* c = control_at(imm.depth);
          TypeCheckBranchResult check_result = TypeCheckBranch(c, false);
          if (V8_LIKELY(check_result == kReachableBranch)) {
            if (imm.depth == control_.size() - 1) {
              DoReturn();
            } else {
              CALL_INTERFACE(Br, c);
              c->br_merge()->reached = true;
            }
          } else if (check_result == kInvalidStack) {
            break;
          }
          len = 1 + imm.length;
          EndControl();
          break;
        }
        case kExprBrIf: {
          BranchDepthImmediate<validate> imm(this, this->pc_);
          Value cond = Pop(0, kWasmI32);
          if (this->failed()) break;
          if (!this->Validate(this->pc_, imm, control_.size())) break;
          Control* c = control_at(imm.depth);
          TypeCheckBranchResult check_result = TypeCheckBranch(c, true);
          if (V8_LIKELY(check_result == kReachableBranch)) {
            CALL_INTERFACE(BrIf, cond, imm.depth);
            c->br_merge()->reached = true;
          } else if (check_result == kInvalidStack) {
            break;
          }
          len = 1 + imm.length;
          break;
        }
        case kExprBrTable: {
          BranchTableImmediate<validate> imm(this, this->pc_);
          BranchTableIterator<validate> iterator(this, imm);
          Value key = Pop(0, kWasmI32);
          if (this->failed()) break;
          if (!this->Validate(this->pc_, imm, control_.size())) break;

          // Cache the branch targets during the iteration, so that we can set
          // all branch targets as reachable after the {CALL_INTERFACE} call.
          std::vector<bool> br_targets(control_.size());

          // The result types of the br_table instruction. We have to check the
          // stack against these types. Only needed during validation.
          std::vector<ValueType> result_types;

          while (iterator.has_next()) {
            const uint32_t index = iterator.cur_index();
            const byte* pos = iterator.pc();
            uint32_t target = iterator.next();
            if (!VALIDATE(ValidateBrTableTarget(target, pos, index))) break;
            // Avoid redundant branch target checks.
            if (br_targets[target]) continue;
            br_targets[target] = true;

            if (validate) {
              if (index == 0) {
                // With the first branch target, initialize the result types.
                result_types = InitializeBrTableResultTypes(target);
              } else if (!UpdateBrTableResultTypes(&result_types, target, pos,
                                                   index)) {
                break;
              }
            }
          }

          if (!VALIDATE(TypeCheckBrTable(result_types))) break;

          DCHECK(this->ok());

          if (current_code_reachable_) {
            CALL_INTERFACE(BrTable, imm, key);

            for (int i = 0, e = control_depth(); i < e; ++i) {
              if (!br_targets[i]) continue;
              control_at(i)->br_merge()->reached = true;
            }
          }

          len = 1 + iterator.length();
          EndControl();
          break;
        }
        case kExprReturn: {
          if (V8_LIKELY(current_code_reachable_)) {
            if (!VALIDATE(TypeCheckReturn())) break;
            DoReturn();
          } else {
            // We pop all return values from the stack to check their type.
            // Since we deal with unreachable code, we do not have to keep the
            // values.
            int num_returns = static_cast<int>(this->sig_->return_count());
            for (int i = num_returns - 1; i >= 0; --i) {
              Pop(i, this->sig_->GetReturn(i));
            }
          }

          EndControl();
          break;
        }
        case kExprUnreachable: {
          CALL_INTERFACE_IF_REACHABLE(Unreachable);
          EndControl();
          break;
        }
        case kExprI32Const: {
          ImmI32Immediate<validate> imm(this, this->pc_);
          Value* value = Push(kWasmI32);
          CALL_INTERFACE_IF_REACHABLE(I32Const, value, imm.value);
          len = 1 + imm.length;
          break;
        }
        case kExprI64Const: {
          ImmI64Immediate<validate> imm(this, this->pc_);
          Value* value = Push(kWasmI64);
          CALL_INTERFACE_IF_REACHABLE(I64Const, value, imm.value);
          len = 1 + imm.length;
          break;
        }
        case kExprF32Const: {
          ImmF32Immediate<validate> imm(this, this->pc_);
          Value* value = Push(kWasmF32);
          CALL_INTERFACE_IF_REACHABLE(F32Const, value, imm.value);
          len = 1 + imm.length;
          break;
        }
        case kExprF64Const: {
          ImmF64Immediate<validate> imm(this, this->pc_);
          Value* value = Push(kWasmF64);
          CALL_INTERFACE_IF_REACHABLE(F64Const, value, imm.value);
          len = 1 + imm.length;
          break;
        }
        case kExprRefNull: {
          CHECK_PROTOTYPE_OPCODE(reftypes);
          RefNullImmediate<validate> imm(this->enabled_, this, this->pc_);
          if (!this->Validate(this->pc_, imm)) break;
          Value* value = Push(imm.type);
          CALL_INTERFACE_IF_REACHABLE(RefNull, value);
          len = 1 + imm.length;
          break;
        }
        case kExprRefIsNull: {
          CHECK_PROTOTYPE_OPCODE(reftypes);
          RefNullImmediate<validate> imm(this->enabled_, this, this->pc_);
          if (!this->Validate(this->pc_, imm)) break;
          Value value = Pop(0, imm.type);
          Value* result = Push(kWasmI32);
          CALL_INTERFACE_IF_REACHABLE(UnOp, opcode, value, result);
          len = 1 + imm.length;
          break;
        }
        case kExprRefFunc: {
          CHECK_PROTOTYPE_OPCODE(reftypes);
          FunctionIndexImmediate<validate> imm(this, this->pc_);
          if (!this->Validate(this->pc_, imm)) break;
          Value* value = Push(kWasmFuncRef);
          CALL_INTERFACE_IF_REACHABLE(RefFunc, imm.index, value);
          len = 1 + imm.length;
          break;
        }
        case kExprRefAsNonNull: {
          CHECK_PROTOTYPE_OPCODE(typed_funcref);
          Value value = Pop();
          switch (value.type.kind()) {
            case ValueType::kRef: {
              Value* result =
                  Push(ValueType(ValueType::kRef, value.type.ref_index()));
              CALL_INTERFACE_IF_REACHABLE(PassThrough, value, result);
              break;
            }
            case ValueType::kOptRef: {
              Value* result =
                  Push(ValueType(ValueType::kRef, value.type.ref_index()));
              CALL_INTERFACE_IF_REACHABLE(RefAsNonNull, value, result);
              break;
            }
            default:
              this->error(this->pc_ + 1,
                          "invalid agrument type to ref.as_non_null");
              break;
          }
          break;
        }
        case kExprLocalGet: {
          LocalIndexImmediate<validate> imm(this, this->pc_);
          if (!this->Validate(this->pc_, imm)) break;
          Value* value = Push(imm.type);
          CALL_INTERFACE_IF_REACHABLE(LocalGet, value, imm);
          len = 1 + imm.length;
          break;
        }
        case kExprLocalSet: {
          LocalIndexImmediate<validate> imm(this, this->pc_);
          if (!this->Validate(this->pc_, imm)) break;
          Value value = Pop(0, local_type_vec_[imm.index]);
          CALL_INTERFACE_IF_REACHABLE(LocalSet, value, imm);
          len = 1 + imm.length;
          break;
        }
        case kExprLocalTee: {
          LocalIndexImmediate<validate> imm(this, this->pc_);
          if (!this->Validate(this->pc_, imm)) break;
          Value value = Pop(0, local_type_vec_[imm.index]);
          Value* result = Push(value.type);
          CALL_INTERFACE_IF_REACHABLE(LocalTee, value, result, imm);
          len = 1 + imm.length;
          break;
        }
        case kExprDrop: {
          Value value = Pop();
          CALL_INTERFACE_IF_REACHABLE(Drop, value);
          break;
        }
        case kExprGlobalGet: {
          GlobalIndexImmediate<validate> imm(this, this->pc_);
          len = 1 + imm.length;
          if (!this->Validate(this->pc_, imm)) break;
          Value* result = Push(imm.type);
          CALL_INTERFACE_IF_REACHABLE(GlobalGet, result, imm);
          break;
        }
        case kExprGlobalSet: {
          GlobalIndexImmediate<validate> imm(this, this->pc_);
          len = 1 + imm.length;
          if (!this->Validate(this->pc_, imm)) break;
          if (!VALIDATE(imm.global->mutability)) {
            this->errorf(this->pc_, "immutable global #%u cannot be assigned",
                         imm.index);
            break;
          }
          Value value = Pop(0, imm.type);
          CALL_INTERFACE_IF_REACHABLE(GlobalSet, value, imm);
          break;
        }
        case kExprTableGet: {
          CHECK_PROTOTYPE_OPCODE(reftypes);
          TableIndexImmediate<validate> imm(this, this->pc_);
          len = 1 + imm.length;
          if (!this->Validate(this->pc_, imm)) break;
          Value index = Pop(0, kWasmI32);
          Value* result = Push(this->module_->tables[imm.index].type);
          CALL_INTERFACE_IF_REACHABLE(TableGet, index, result, imm);
          break;
        }
        case kExprTableSet: {
          CHECK_PROTOTYPE_OPCODE(reftypes);
          TableIndexImmediate<validate> imm(this, this->pc_);
          len = 1 + imm.length;
          if (!this->Validate(this->pc_, imm)) break;
          Value value = Pop(1, this->module_->tables[imm.index].type);
          Value index = Pop(0, kWasmI32);
          CALL_INTERFACE_IF_REACHABLE(TableSet, index, value, imm);
          break;
        }

        case kExprI32LoadMem8S:
          len = 1 + DecodeLoadMem(LoadType::kI32Load8S);
          break;
        case kExprI32LoadMem8U:
          len = 1 + DecodeLoadMem(LoadType::kI32Load8U);
          break;
        case kExprI32LoadMem16S:
          len = 1 + DecodeLoadMem(LoadType::kI32Load16S);
          break;
        case kExprI32LoadMem16U:
          len = 1 + DecodeLoadMem(LoadType::kI32Load16U);
          break;
        case kExprI32LoadMem:
          len = 1 + DecodeLoadMem(LoadType::kI32Load);
          break;
        case kExprI64LoadMem8S:
          len = 1 + DecodeLoadMem(LoadType::kI64Load8S);
          break;
        case kExprI64LoadMem8U:
          len = 1 + DecodeLoadMem(LoadType::kI64Load8U);
          break;
        case kExprI64LoadMem16S:
          len = 1 + DecodeLoadMem(LoadType::kI64Load16S);
          break;
        case kExprI64LoadMem16U:
          len = 1 + DecodeLoadMem(LoadType::kI64Load16U);
          break;
        case kExprI64LoadMem32S:
          len = 1 + DecodeLoadMem(LoadType::kI64Load32S);
          break;
        case kExprI64LoadMem32U:
          len = 1 + DecodeLoadMem(LoadType::kI64Load32U);
          break;
        case kExprI64LoadMem:
          len = 1 + DecodeLoadMem(LoadType::kI64Load);
          break;
        case kExprF32LoadMem:
          len = 1 + DecodeLoadMem(LoadType::kF32Load);
          break;
        case kExprF64LoadMem:
          len = 1 + DecodeLoadMem(LoadType::kF64Load);
          break;
        case kExprI32StoreMem8:
          len = 1 + DecodeStoreMem(StoreType::kI32Store8);
          break;
        case kExprI32StoreMem16:
          len = 1 + DecodeStoreMem(StoreType::kI32Store16);
          break;
        case kExprI32StoreMem:
          len = 1 + DecodeStoreMem(StoreType::kI32Store);
          break;
        case kExprI64StoreMem8:
          len = 1 + DecodeStoreMem(StoreType::kI64Store8);
          break;
        case kExprI64StoreMem16:
          len = 1 + DecodeStoreMem(StoreType::kI64Store16);
          break;
        case kExprI64StoreMem32:
          len = 1 + DecodeStoreMem(StoreType::kI64Store32);
          break;
        case kExprI64StoreMem:
          len = 1 + DecodeStoreMem(StoreType::kI64Store);
          break;
        case kExprF32StoreMem:
          len = 1 + DecodeStoreMem(StoreType::kF32Store);
          break;
        case kExprF64StoreMem:
          len = 1 + DecodeStoreMem(StoreType::kF64Store);
          break;
        case kExprMemoryGrow: {
          if (!CheckHasMemory()) break;
          MemoryIndexImmediate<validate> imm(this, this->pc_);
          len = 1 + imm.length;
          if (!VALIDATE(this->module_->origin == kWasmOrigin)) {
            this->error("grow_memory is not supported for asmjs modules");
            break;
          }
          Value value = Pop(0, kWasmI32);
          Value* result = Push(kWasmI32);
          CALL_INTERFACE_IF_REACHABLE(MemoryGrow, value, result);
          break;
        }
        case kExprMemorySize: {
          if (!CheckHasMemory()) break;
          MemoryIndexImmediate<validate> imm(this, this->pc_);
          Value* result = Push(kWasmI32);
          len = 1 + imm.length;
          CALL_INTERFACE_IF_REACHABLE(CurrentMemoryPages, result);
          break;
        }
        case kExprCallFunction: {
          CallFunctionImmediate<validate> imm(this, this->pc_);
          len = 1 + imm.length;
          if (!this->Validate(this->pc_, imm)) break;
          ArgVector args = PopArgs(imm.sig);
          Value* returns = PushReturns(imm.sig);
          CALL_INTERFACE_IF_REACHABLE(CallDirect, imm, args.begin(), returns);
          break;
        }
        case kExprCallIndirect: {
          CallIndirectImmediate<validate> imm(this->enabled_, this, this->pc_);
          len = 1 + imm.length;
          if (!this->Validate(this->pc_, imm)) break;
          Value index = Pop(0, kWasmI32);
          ArgVector args = PopArgs(imm.sig);
          Value* returns = PushReturns(imm.sig);
          CALL_INTERFACE_IF_REACHABLE(CallIndirect, index, imm, args.begin(),
                                      returns);
          break;
        }
        case kExprReturnCall: {
          CHECK_PROTOTYPE_OPCODE(return_call);

          CallFunctionImmediate<validate> imm(this, this->pc_);
          len = 1 + imm.length;
          if (!this->Validate(this->pc_, imm)) break;
          if (!this->CanReturnCall(imm.sig)) {
            OPCODE_ERROR(opcode, "tail call return types mismatch");
            break;
          }

          ArgVector args = PopArgs(imm.sig);

          CALL_INTERFACE_IF_REACHABLE(ReturnCall, imm, args.begin());
          EndControl();
          break;
        }
        case kExprReturnCallIndirect: {
          CHECK_PROTOTYPE_OPCODE(return_call);
          CallIndirectImmediate<validate> imm(this->enabled_, this, this->pc_);
          len = 1 + imm.length;
          if (!this->Validate(this->pc_, imm)) break;
          if (!this->CanReturnCall(imm.sig)) {
            OPCODE_ERROR(opcode, "tail call return types mismatch");
            break;
          }
          Value index = Pop(0, kWasmI32);
          ArgVector args = PopArgs(imm.sig);
          CALL_INTERFACE_IF_REACHABLE(ReturnCallIndirect, index, imm,
                                      args.begin());
          EndControl();
          break;
        }
        case kNumericPrefix: {
          ++len;
          byte numeric_index =
              this->template read_u8<validate>(this->pc_ + 1, "numeric index");
          opcode = static_cast<WasmOpcode>(opcode << 8 | numeric_index);
          if (opcode == kExprTableGrow || opcode == kExprTableSize ||
              opcode == kExprTableFill) {
            CHECK_PROTOTYPE_OPCODE(reftypes);
          } else if (opcode >= kExprMemoryInit) {
            CHECK_PROTOTYPE_OPCODE(bulk_memory);
          }
          TRACE_PART(TRACE_INST_FORMAT, startrel(this->pc_),
                     WasmOpcodes::OpcodeName(opcode));
          len += DecodeNumericOpcode(opcode);
          break;
        }
        case kSimdPrefix: {
          CHECK_PROTOTYPE_OPCODE(simd);
          uint32_t length = 0;
          opcode =
              this->template read_prefixed_opcode<validate>(this->pc_, &length);
          if (!VALIDATE(this->ok())) break;
          len += length;

          TRACE_PART(TRACE_INST_FORMAT, startrel(this->pc_),
                     WasmOpcodes::OpcodeName(opcode));
          len += DecodeSimdOpcode(opcode, length);
          break;
        }
        case kAtomicPrefix: {
          CHECK_PROTOTYPE_OPCODE(threads);
          len++;
          byte atomic_index =
              this->template read_u8<validate>(this->pc_ + 1, "atomic index");
          opcode = static_cast<WasmOpcode>(opcode << 8 | atomic_index);
          TRACE_PART(TRACE_INST_FORMAT, startrel(this->pc_),
                     WasmOpcodes::OpcodeName(opcode));
          len += DecodeAtomicOpcode(opcode);
          break;
        }
        case kGCPrefix: {
          CHECK_PROTOTYPE_OPCODE(gc);
          byte gc_index =
              this->template read_u8<validate>(this->pc_ + 1, "gc index");
          opcode = static_cast<WasmOpcode>(opcode << 8 | gc_index);
          TRACE_PART(TRACE_INST_FORMAT, startrel(this->pc_),
                     WasmOpcodes::OpcodeName(opcode));
          len = DecodeGCOpcode(opcode);
          break;
        }
// Note that prototype opcodes are not handled in the fastpath
// above this switch, to avoid checking a feature flag.
#define SIMPLE_PROTOTYPE_CASE(name, opc, sig) \
  case kExpr##name: /* fallthrough */
          FOREACH_SIMPLE_PROTOTYPE_OPCODE(SIMPLE_PROTOTYPE_CASE)
#undef SIMPLE_PROTOTYPE_CASE
          BuildSimplePrototypeOperator(opcode);
          break;
        default: {
          // Deal with special asmjs opcodes.
          if (is_asmjs_module(this->module_)) {
            const FunctionSig* sig = WasmOpcodes::AsmjsSignature(opcode);
            if (sig) {
              BuildSimpleOperator(opcode, sig);
            }
          } else {
            this->error("Invalid opcode");
            return;
          }
        }
      }

#if DEBUG
      if (FLAG_trace_wasm_decoder) {
        TRACE_PART(" ");
        for (Control& c : control_) {
          switch (c.kind) {
            case kControlIf:
              TRACE_PART("I");
              break;
            case kControlBlock:
              TRACE_PART("B");
              break;
            case kControlLoop:
              TRACE_PART("L");
              break;
            case kControlTry:
              TRACE_PART("T");
              break;
            case kControlIfElse:
            case kControlTryCatch:
            case kControlLet:  // TODO(7748): Implement
              break;
          }
          if (c.start_merge.arity) TRACE_PART("%u-", c.start_merge.arity);
          TRACE_PART("%u", c.end_merge.arity);
          if (!c.reachable()) TRACE_PART("%c", c.unreachable() ? '*' : '#');
        }
        TRACE_PART(" | ");
        for (size_t i = 0; i < stack_.size(); ++i) {
          Value& val = stack_[i];
          WasmOpcode opcode = static_cast<WasmOpcode>(*val.pc);
          if (WasmOpcodes::IsPrefixOpcode(opcode)) {
            opcode = this->template read_prefixed_opcode<Decoder::kNoValidate>(
                val.pc);
          }
          TRACE_PART(" %c@%d:%s", val.type.short_name(),
                     static_cast<int>(val.pc - this->start_),
                     WasmOpcodes::OpcodeName(opcode));
          // If the decoder failed, don't try to decode the immediates, as this
          // can trigger a DCHECK failure.
          if (this->failed()) continue;
          switch (opcode) {
            case kExprI32Const: {
              ImmI32Immediate<Decoder::kNoValidate> imm(this, val.pc);
              TRACE_PART("[%d]", imm.value);
              break;
            }
            case kExprLocalGet:
            case kExprLocalSet:
            case kExprLocalTee: {
              LocalIndexImmediate<Decoder::kNoValidate> imm(this, val.pc);
              TRACE_PART("[%u]", imm.index);
              break;
            }
            case kExprGlobalGet:
            case kExprGlobalSet: {
              GlobalIndexImmediate<Decoder::kNoValidate> imm(this, val.pc);
              TRACE_PART("[%u]", imm.index);
              break;
            }
            default:
              break;
          }
        }
      }
#endif
      this->pc_ += len;
    }  // end decode loop
    if (!VALIDATE(this->pc_ == this->end_) && this->ok()) {
      this->error("Beyond end of code");
    }
  }

  void EndControl() {
    DCHECK(!control_.empty());
    Control* current = &control_.back();
    stack_.erase(stack_.begin() + current->stack_depth, stack_.end());
    CALL_INTERFACE_IF_REACHABLE(EndControl, current);
    current->reachability = kUnreachable;
    current_code_reachable_ = false;
  }

  template<typename func>
  void InitMerge(Merge<Value>* merge, uint32_t arity, func get_val) {
    merge->arity = arity;
    if (arity == 1) {
      merge->vals.first = get_val(0);
    } else if (arity > 1) {
      merge->vals.array = zone_->NewArray<Value>(arity);
      for (uint32_t i = 0; i < arity; i++) {
        merge->vals.array[i] = get_val(i);
      }
    }
  }

  void SetBlockType(Control* c, BlockTypeImmediate<validate>& imm,
                    Value* args) {
    const byte* pc = this->pc_;
    InitMerge(&c->end_merge, imm.out_arity(), [pc, &imm](uint32_t i) {
      return Value{pc, imm.out_type(i)};
    });
    InitMerge(&c->start_merge, imm.in_arity(),
              [args](uint32_t i) { return args[i]; });
  }

  // Pops arguments as required by signature.
  V8_INLINE ArgVector PopArgs(const FunctionSig* sig) {
    int count = sig ? static_cast<int>(sig->parameter_count()) : 0;
    ArgVector args(count);
    for (int i = count - 1; i >= 0; --i) {
      args[i] = Pop(i, sig->GetParam(i));
    }
    return args;
  }

  V8_INLINE ArgVector PopArgs(const StructType* type) {
    int count = static_cast<int>(type->field_count());
    ArgVector args(count);
    for (int i = count - 1; i >= 0; i--) {
      args[i] = Pop(i, type->field(i).Unpack());
    }
    return args;
  }

  V8_INLINE ArgVector PopArgs(uint32_t base_index,
                              Vector<ValueType> arg_types) {
    ArgVector args(arg_types.size());
    for (int i = static_cast<int>(arg_types.size()) - 1; i >= 0; i--) {
      args[i] = Pop(base_index + i, arg_types[i]);
    }
    return args;
  }

  ValueType GetReturnType(const FunctionSig* sig) {
    DCHECK_GE(1, sig->return_count());
    return sig->return_count() == 0 ? kWasmStmt : sig->GetReturn();
  }

  Control* PushControl(ControlKind kind, uint32_t locals_count = 0) {
    Reachability reachability =
        control_.empty() ? kReachable : control_.back().innerReachability();
    control_.emplace_back(kind, locals_count, stack_size(), this->pc_,
                          reachability);
    current_code_reachable_ = this->ok() && reachability == kReachable;
    return &control_.back();
  }

  void PopControl(Control* c) {
    DCHECK_EQ(c, &control_.back());
    CALL_INTERFACE_IF_PARENT_REACHABLE(PopControl, c);

    // A loop just leaves the values on the stack.
    if (!c->is_loop()) PushMergeValues(c, &c->end_merge);

    bool parent_reached =
        c->reachable() || c->end_merge.reached || c->is_onearmed_if();
    control_.pop_back();
    // If the parent block was reachable before, but the popped control does not
    // return to here, this block becomes "spec only reachable".
    if (!parent_reached) SetSucceedingCodeDynamicallyUnreachable();
    current_code_reachable_ = control_.back().reachable();
  }

  int DecodeLoadMem(LoadType type, int prefix_len = 0) {
    if (!CheckHasMemory()) return 0;
    MemoryAccessImmediate<validate> imm(this, this->pc_ + prefix_len,
                                        type.size_log_2());
    Value index = Pop(0, kWasmI32);
    Value* result = Push(type.value_type());
    CALL_INTERFACE_IF_REACHABLE(LoadMem, type, imm, index, result);
    return imm.length;
  }

  int DecodeLoadTransformMem(LoadType type, LoadTransformationKind transform,
                             uint32_t opcode_length) {
    if (!CheckHasMemory()) return 0;
    // Load extends always load 64-bits.
    uint32_t max_alignment =
        transform == LoadTransformationKind::kExtend ? 3 : type.size_log_2();
    MemoryAccessImmediate<validate> imm(this, this->pc_ + opcode_length,
                                        max_alignment);
    Value index = Pop(0, kWasmI32);
    Value* result = Push(kWasmS128);
    CALL_INTERFACE_IF_REACHABLE(LoadTransform, type, transform, imm, index,
                                result);
    return imm.length;
  }

  int DecodeStoreMem(StoreType store, int prefix_len = 0) {
    if (!CheckHasMemory()) return 0;
    MemoryAccessImmediate<validate> imm(this, this->pc_ + prefix_len,
                                        store.size_log_2());
    Value value = Pop(1, store.value_type());
    Value index = Pop(0, kWasmI32);
    CALL_INTERFACE_IF_REACHABLE(StoreMem, store, imm, index, value);
    return imm.length;
  }

  bool ValidateBrTableTarget(uint32_t target, const byte* pos, int index) {
    if (!VALIDATE(target < this->control_.size())) {
      this->errorf(pos, "improper branch in br_table target %u (depth %u)",
                   index, target);
      return false;
    }
    return true;
  }

  std::vector<ValueType> InitializeBrTableResultTypes(uint32_t target) {
    Merge<Value>* merge = control_at(target)->br_merge();
    int br_arity = merge->arity;
    std::vector<ValueType> result(br_arity);
    for (int i = 0; i < br_arity; ++i) {
      result[i] = (*merge)[i].type;
    }
    return result;
  }

  bool UpdateBrTableResultTypes(std::vector<ValueType>* result_types,
                                uint32_t target, const byte* pos, int index) {
    Merge<Value>* merge = control_at(target)->br_merge();
    int br_arity = merge->arity;
    // First we check if the arities match.
    if (br_arity != static_cast<int>(result_types->size())) {
      this->errorf(pos,
                   "inconsistent arity in br_table target %u (previous was "
                   "%zu, this one is %u)",
                   index, result_types->size(), br_arity);
      return false;
    }

    for (int i = 0; i < br_arity; ++i) {
      if (this->enabled_.has_reftypes()) {
        // The expected type is the biggest common sub type of all targets.
        ValueType type = (*result_types)[i];
        (*result_types)[i] =
            CommonSubtype((*result_types)[i], (*merge)[i].type, this->module_);
        if ((*result_types)[i] == kWasmBottom) {
          this->errorf(pos,
                       "inconsistent type in br_table target %u (previous "
                       "was %s, this one is %s)",
                       index, type.type_name(), (*merge)[i].type.type_name());
          return false;
        }
      } else {
        // All target must have the same signature.
        if ((*result_types)[i] != (*merge)[i].type) {
          this->errorf(pos,
                       "inconsistent type in br_table target %u (previous "
                       "was %s, this one is %s)",
                       index, (*result_types)[i].type_name(),
                       (*merge)[i].type.type_name());
          return false;
        }
      }
    }
    return true;
  }

  bool TypeCheckBrTable(const std::vector<ValueType>& result_types) {
    int br_arity = static_cast<int>(result_types.size());
    if (V8_LIKELY(!control_.back().unreachable())) {
      int available =
          static_cast<int>(stack_.size()) - control_.back().stack_depth;
      // There have to be enough values on the stack.
      if (available < br_arity) {
        this->errorf(this->pc_,
                     "expected %u elements on the stack for branch to "
                     "@%d, found %u",
                     br_arity, startrel(control_.back().pc), available);
        return false;
      }
      Value* stack_values = &*(stack_.end() - br_arity);
      // Type-check the topmost br_arity values on the stack.
      for (int i = 0; i < br_arity; ++i) {
        Value& val = stack_values[i];
        if (!IsSubtypeOf(val.type, result_types[i], this->module_)) {
          this->errorf(this->pc_,
                       "type error in merge[%u] (expected %s, got %s)", i,
                       result_types[i].type_name(), val.type.type_name());
          return false;
        }
      }
    } else {  // !control_.back().reachable()
      // Pop values from the stack, accoring to the expected signature.
      for (int i = 0; i < br_arity; ++i) Pop(i + 1, result_types[i]);
    }
    return this->ok();
  }

  uint32_t SimdExtractLane(WasmOpcode opcode, ValueType type,
                           uint32_t opcode_length) {
    SimdLaneImmediate<validate> imm(this, this->pc_, opcode_length);
    if (this->Validate(this->pc_, opcode, imm)) {
      Value inputs[] = {Pop(0, kWasmS128)};
      Value* result = Push(type);
      CALL_INTERFACE_IF_REACHABLE(SimdLaneOp, opcode, imm, ArrayVector(inputs),
                                  result);
    }
    return imm.length;
  }

  uint32_t SimdReplaceLane(WasmOpcode opcode, ValueType type,
                           uint32_t opcode_length) {
    SimdLaneImmediate<validate> imm(this, this->pc_, opcode_length);
    if (this->Validate(this->pc_, opcode, imm)) {
      Value inputs[2] = {UnreachableValue(this->pc_),
                         UnreachableValue(this->pc_)};
      inputs[1] = Pop(1, type);
      inputs[0] = Pop(0, kWasmS128);
      Value* result = Push(kWasmS128);
      CALL_INTERFACE_IF_REACHABLE(SimdLaneOp, opcode, imm, ArrayVector(inputs),
                                  result);
    }
    return imm.length;
  }

  uint32_t Simd8x16ShuffleOp(uint32_t opcode_length) {
    Simd8x16ShuffleImmediate<validate> imm(this, this->pc_, opcode_length);
    if (this->Validate(this->pc_, imm)) {
      Value input1 = Pop(1, kWasmS128);
      Value input0 = Pop(0, kWasmS128);
      Value* result = Push(kWasmS128);
      CALL_INTERFACE_IF_REACHABLE(Simd8x16ShuffleOp, imm, input0, input1,
                                  result);
    }
    return 16;
  }

  uint32_t DecodeSimdOpcode(WasmOpcode opcode, uint32_t opcode_length) {
    // opcode_length is the number of bytes that this SIMD-specific opcode takes
    // up in the LEB128 encoded form.
    uint32_t len = 0;
    switch (opcode) {
      case kExprF64x2ExtractLane: {
        len = SimdExtractLane(opcode, kWasmF64, opcode_length);
        break;
      }
      case kExprF32x4ExtractLane: {
        len = SimdExtractLane(opcode, kWasmF32, opcode_length);
        break;
      }
      case kExprI64x2ExtractLane: {
        len = SimdExtractLane(opcode, kWasmI64, opcode_length);
        break;
      }
      case kExprI32x4ExtractLane:
      case kExprI16x8ExtractLaneS:
      case kExprI16x8ExtractLaneU:
      case kExprI8x16ExtractLaneS:
      case kExprI8x16ExtractLaneU: {
        len = SimdExtractLane(opcode, kWasmI32, opcode_length);
        break;
      }
      case kExprF64x2ReplaceLane: {
        len = SimdReplaceLane(opcode, kWasmF64, opcode_length);
        break;
      }
      case kExprF32x4ReplaceLane: {
        len = SimdReplaceLane(opcode, kWasmF32, opcode_length);
        break;
      }
      case kExprI64x2ReplaceLane: {
        len = SimdReplaceLane(opcode, kWasmI64, opcode_length);
        break;
      }
      case kExprI32x4ReplaceLane:
      case kExprI16x8ReplaceLane:
      case kExprI8x16ReplaceLane: {
        len = SimdReplaceLane(opcode, kWasmI32, opcode_length);
        break;
      }
      case kExprS8x16Shuffle: {
        len = Simd8x16ShuffleOp(opcode_length);
        break;
      }
      case kExprS128LoadMem:
        len = DecodeLoadMem(LoadType::kS128Load, opcode_length);
        break;
      case kExprS128StoreMem:
        len = DecodeStoreMem(StoreType::kS128Store, opcode_length);
        break;
      case kExprS8x16LoadSplat:
        len = DecodeLoadTransformMem(LoadType::kI32Load8S,
                                     LoadTransformationKind::kSplat,
                                     opcode_length);
        break;
      case kExprS16x8LoadSplat:
        len = DecodeLoadTransformMem(LoadType::kI32Load16S,
                                     LoadTransformationKind::kSplat,
                                     opcode_length);
        break;
      case kExprS32x4LoadSplat:
        len = DecodeLoadTransformMem(
            LoadType::kI32Load, LoadTransformationKind::kSplat, opcode_length);
        break;
      case kExprS64x2LoadSplat:
        len = DecodeLoadTransformMem(
            LoadType::kI64Load, LoadTransformationKind::kSplat, opcode_length);
        break;
      case kExprI16x8Load8x8S:
        len = DecodeLoadTransformMem(LoadType::kI32Load8S,
                                     LoadTransformationKind::kExtend,
                                     opcode_length);
        break;
      case kExprI16x8Load8x8U:
        len = DecodeLoadTransformMem(LoadType::kI32Load8U,
                                     LoadTransformationKind::kExtend,
                                     opcode_length);
        break;
      case kExprI32x4Load16x4S:
        len = DecodeLoadTransformMem(LoadType::kI32Load16S,
                                     LoadTransformationKind::kExtend,
                                     opcode_length);
        break;
      case kExprI32x4Load16x4U:
        len = DecodeLoadTransformMem(LoadType::kI32Load16U,
                                     LoadTransformationKind::kExtend,
                                     opcode_length);
        break;
      case kExprI64x2Load32x2S:
        len = DecodeLoadTransformMem(LoadType::kI64Load32S,
                                     LoadTransformationKind::kExtend,
                                     opcode_length);
        break;
      case kExprI64x2Load32x2U:
        len = DecodeLoadTransformMem(LoadType::kI64Load32U,
                                     LoadTransformationKind::kExtend,
                                     opcode_length);
        break;
      default: {
        if (!FLAG_wasm_simd_post_mvp &&
            WasmOpcodes::IsSimdPostMvpOpcode(opcode)) {
          this->error(
              "simd opcode not available, enable with --wasm-simd-post-mvp");
          break;
        }
        const FunctionSig* sig = WasmOpcodes::Signature(opcode);
        if (!VALIDATE(sig != nullptr)) {
          this->error("invalid simd opcode");
          break;
        }
        ArgVector args = PopArgs(sig);
        Value* results =
            sig->return_count() == 0 ? nullptr : Push(GetReturnType(sig));
        CALL_INTERFACE_IF_REACHABLE(SimdOp, opcode, VectorOf(args), results);
      }
    }
    return len;
  }

  uint32_t DecodeGCOpcode(WasmOpcode opcode) {
    uint32_t len = 2;
    switch (opcode) {
      case kExprStructNew: {
        StructIndexImmediate<validate> imm(this, this->pc_ + len);
        len += imm.length;
        if (!this->Validate(this->pc_, imm)) break;
        ArgVector args = PopArgs(imm.struct_type);
        Value* value = Push(ValueType(ValueType::kRef, imm.index));
        CALL_INTERFACE_IF_REACHABLE(StructNew, imm, args.begin(), value);
        break;
      }
      case kExprStructGet: {
        FieldIndexImmediate<validate> field(this, this->pc_ + len);
        if (!this->Validate(this->pc_ + len, field)) break;
        ValueType field_type =
            field.struct_index.struct_type->field(field.index);
        if (field_type.IsPacked()) {
          this->error(this->pc_,
                      "struct.get used with a field of packed type. "
                      "Use struct.get_s or struct.get_u instead.");
          break;
        }
        len += field.length;
        Value struct_obj =
            Pop(0, ValueType(ValueType::kOptRef, field.struct_index.index));
        Value* value = Push(field_type);
        CALL_INTERFACE_IF_REACHABLE(StructGet, struct_obj, field, true, value);
        break;
      }
      case kExprStructGetU:
      case kExprStructGetS: {
        FieldIndexImmediate<validate> field(this, this->pc_ + len);
        if (!this->Validate(this->pc_ + len, field)) break;
        len += field.length;
        ValueType field_type =
            field.struct_index.struct_type->field(field.index);
        if (!field_type.IsPacked()) {
          this->errorf(this->pc_,
                       "%s is only valid for packed struct fields. "
                       "Use struct.get instead.",
                       WasmOpcodes::OpcodeName(opcode));
          break;
        }
        Value struct_obj =
            Pop(0, ValueType(ValueType::kOptRef, field.struct_index.index));
        Value* value = Push(field_type.Unpack());
        CALL_INTERFACE_IF_REACHABLE(StructGet, struct_obj, field,
                                    opcode == kExprStructGetS, value);
        break;
      }
      case kExprStructSet: {
        FieldIndexImmediate<validate> field(this, this->pc_ + len);
        if (!this->Validate(this->pc_ + len, field)) break;
        len += field.length;
        const StructType* struct_type = field.struct_index.struct_type;
        if (!struct_type->mutability(field.index)) {
          this->error(this->pc_, "setting immutable struct field");
          break;
        }
        Value field_value =
            Pop(1, ValueType(struct_type->field(field.index).Unpack()));
        Value struct_obj =
            Pop(0, ValueType(ValueType::kOptRef, field.struct_index.index));
        CALL_INTERFACE_IF_REACHABLE(StructSet, struct_obj, field, field_value);
        break;
      }
      case kExprArrayNew: {
        ArrayIndexImmediate<validate> imm(this, this->pc_ + len);
        len += imm.length;
        if (!this->Validate(this->pc_, imm)) break;
        Value length = Pop(1, kWasmI32);
        Value initial_value = Pop(0, imm.array_type->element_type().Unpack());
        Value* value = Push(ValueType(ValueType::kRef, imm.index));
        CALL_INTERFACE_IF_REACHABLE(ArrayNew, imm, length, initial_value,
                                    value);
        break;
      }
      case kExprArrayGetS:
      case kExprArrayGetU: {
        ArrayIndexImmediate<validate> imm(this, this->pc_ + len);
        len += imm.length;
        if (!this->Validate(this->pc_ + len, imm)) break;
        if (!imm.array_type->element_type().IsPacked()) {
          this->errorf(this->pc_,
                       "%s is only valid for packed arrays. "
                       "Use or array.get instead.",
                       WasmOpcodes::OpcodeName(opcode));
          break;
        }
        Value index = Pop(1, kWasmI32);
        Value array_obj = Pop(0, ValueType(ValueType::kOptRef, imm.index));
        Value* value = Push(imm.array_type->element_type().Unpack());
        // TODO(7748): Optimize this when array_obj is non-nullable ref.
        CALL_INTERFACE_IF_REACHABLE(ArrayGet, array_obj, imm, index,
                                    opcode == kExprArrayGetS, value);
        break;
      }
      case kExprArrayGet: {
        ArrayIndexImmediate<validate> imm(this, this->pc_ + len);
        len += imm.length;
        if (!this->Validate(this->pc_ + len, imm)) break;
        if (imm.array_type->element_type().IsPacked()) {
          this->error(this->pc_,
                      "array.get used with a field of packed type. "
                      "Use array.get_s or array.get_u instead.");
          break;
        }
        Value index = Pop(1, kWasmI32);
        Value array_obj = Pop(0, ValueType(ValueType::kOptRef, imm.index));
        Value* value = Push(imm.array_type->element_type());
        // TODO(7748): Optimize this when array_obj is non-nullable ref.
        CALL_INTERFACE_IF_REACHABLE(ArrayGet, array_obj, imm, index, true,
                                    value);
        break;
      }
      case kExprArraySet: {
        ArrayIndexImmediate<validate> imm(this, this->pc_ + len);
        len += imm.length;
        if (!this->Validate(this->pc_ + len, imm)) break;
        if (!imm.array_type->mutability()) {
          this->error(this->pc_, "setting element of immutable array");
          break;
        }
        Value value = Pop(2, imm.array_type->element_type().Unpack());
        Value index = Pop(1, kWasmI32);
        Value array_obj = Pop(0, ValueType(ValueType::kOptRef, imm.index));
        // TODO(7748): Optimize this when array_obj is non-nullable ref.
        CALL_INTERFACE_IF_REACHABLE(ArraySet, array_obj, imm, index, value);
        break;
      }
      case kExprArrayLen: {
        ArrayIndexImmediate<validate> imm(this, this->pc_ + len);
        len += imm.length;
        if (!this->Validate(this->pc_ + len, imm)) break;
        Value array_obj = Pop(0, ValueType(ValueType::kOptRef, imm.index));
        Value* value = Push(kWasmI32);
        CALL_INTERFACE_IF_REACHABLE(ArrayLen, array_obj, value);
        break;
      }
      default:
        this->error("invalid gc opcode");
        return 0;
    }
    return len;
  }

  uint32_t DecodeAtomicOpcode(WasmOpcode opcode) {
    uint32_t len = 0;
    ValueType ret_type;
    const FunctionSig* sig = WasmOpcodes::Signature(opcode);
    if (!VALIDATE(sig != nullptr)) {
      this->error("invalid atomic opcode");
      return 0;
    }
    MachineType memtype;
    switch (opcode) {
#define CASE_ATOMIC_STORE_OP(Name, Type) \
  case kExpr##Name: {                    \
    memtype = MachineType::Type();       \
    ret_type = kWasmStmt;                \
    break;                               \
  }
      ATOMIC_STORE_OP_LIST(CASE_ATOMIC_STORE_OP)
#undef CASE_ATOMIC_OP
#define CASE_ATOMIC_OP(Name, Type) \
  case kExpr##Name: {              \
    memtype = MachineType::Type(); \
    ret_type = GetReturnType(sig); \
    break;                         \
  }
      ATOMIC_OP_LIST(CASE_ATOMIC_OP)
#undef CASE_ATOMIC_OP
      case kExprAtomicFence: {
        byte zero = this->template read_u8<validate>(this->pc_ + 2, "zero");
        if (!VALIDATE(zero == 0)) {
          this->error(this->pc_ + 2, "invalid atomic operand");
          return 0;
        }
        CALL_INTERFACE_IF_REACHABLE(AtomicFence);
        return 1;
      }
      default:
        this->error("invalid atomic opcode");
        return 0;
    }
    if (!CheckHasMemoryForAtomics()) return 0;
    MemoryAccessImmediate<validate> imm(
        this, this->pc_ + 1, ElementSizeLog2Of(memtype.representation()));
    len += imm.length;
    ArgVector args = PopArgs(sig);
    Value* result = ret_type == kWasmStmt ? nullptr : Push(GetReturnType(sig));
    CALL_INTERFACE_IF_REACHABLE(AtomicOp, opcode, VectorOf(args), imm, result);
    return len;
  }

  unsigned DecodeNumericOpcode(WasmOpcode opcode) {
    unsigned len = 0;
    const FunctionSig* sig = WasmOpcodes::Signature(opcode);
    if (sig != nullptr) {
      switch (opcode) {
        case kExprI32SConvertSatF32:
        case kExprI32UConvertSatF32:
        case kExprI32SConvertSatF64:
        case kExprI32UConvertSatF64:
        case kExprI64SConvertSatF32:
        case kExprI64UConvertSatF32:
        case kExprI64SConvertSatF64:
        case kExprI64UConvertSatF64:
          BuildSimpleOperator(opcode, sig);
          break;
        case kExprMemoryInit: {
          MemoryInitImmediate<validate> imm(this, this->pc_);
          if (!this->Validate(imm)) break;
          len += imm.length;
          Value size = Pop(2, sig->GetParam(2));
          Value src = Pop(1, sig->GetParam(1));
          Value dst = Pop(0, sig->GetParam(0));
          CALL_INTERFACE_IF_REACHABLE(MemoryInit, imm, dst, src, size);
          break;
        }
        case kExprDataDrop: {
          DataDropImmediate<validate> imm(this, this->pc_);
          if (!this->Validate(imm)) break;
          len += imm.length;
          CALL_INTERFACE_IF_REACHABLE(DataDrop, imm);
          break;
        }
        case kExprMemoryCopy: {
          MemoryCopyImmediate<validate> imm(this, this->pc_);
          if (!this->Validate(imm)) break;
          len += imm.length;
          Value size = Pop(2, sig->GetParam(2));
          Value src = Pop(1, sig->GetParam(1));
          Value dst = Pop(0, sig->GetParam(0));
          CALL_INTERFACE_IF_REACHABLE(MemoryCopy, imm, dst, src, size);
          break;
        }
        case kExprMemoryFill: {
          MemoryIndexImmediate<validate> imm(this, this->pc_ + 1);
          if (!this->Validate(this->pc_ + 1, imm)) break;
          len += imm.length;
          Value size = Pop(2, sig->GetParam(2));
          Value value = Pop(1, sig->GetParam(1));
          Value dst = Pop(0, sig->GetParam(0));
          CALL_INTERFACE_IF_REACHABLE(MemoryFill, imm, dst, value, size);
          break;
        }
        case kExprTableInit: {
          TableInitImmediate<validate> imm(this, this->pc_);
          if (!this->Validate(imm)) break;
          len += imm.length;
          ArgVector args = PopArgs(sig);
          CALL_INTERFACE_IF_REACHABLE(TableInit, imm, VectorOf(args));
          break;
        }
        case kExprElemDrop: {
          ElemDropImmediate<validate> imm(this, this->pc_);
          if (!this->Validate(imm)) break;
          len += imm.length;
          CALL_INTERFACE_IF_REACHABLE(ElemDrop, imm);
          break;
        }
        case kExprTableCopy: {
          TableCopyImmediate<validate> imm(this, this->pc_);
          if (!this->Validate(imm)) break;
          len += imm.length;
          ArgVector args = PopArgs(sig);
          CALL_INTERFACE_IF_REACHABLE(TableCopy, imm, VectorOf(args));
          break;
        }
        case kExprTableGrow: {
          TableIndexImmediate<validate> imm(this, this->pc_ + 1);
          if (!this->Validate(this->pc_, imm)) break;
          len += imm.length;
          Value delta = Pop(1, sig->GetParam(1));
          Value value = Pop(0, this->module_->tables[imm.index].type);
          Value* result = Push(kWasmI32);
          CALL_INTERFACE_IF_REACHABLE(TableGrow, imm, value, delta, result);
          break;
        }
        case kExprTableSize: {
          TableIndexImmediate<validate> imm(this, this->pc_ + 1);
          if (!this->Validate(this->pc_, imm)) break;
          len += imm.length;
          Value* result = Push(kWasmI32);
          CALL_INTERFACE_IF_REACHABLE(TableSize, imm, result);
          break;
        }
        case kExprTableFill: {
          TableIndexImmediate<validate> imm(this, this->pc_ + 1);
          if (!this->Validate(this->pc_, imm)) break;
          len += imm.length;
          Value count = Pop(2, sig->GetParam(2));
          Value value = Pop(1, this->module_->tables[imm.index].type);
          Value start = Pop(0, sig->GetParam(0));
          CALL_INTERFACE_IF_REACHABLE(TableFill, imm, start, value, count);
          break;
        }
        default:
          this->error("invalid numeric opcode");
          break;
      }
    } else {
      this->error("invalid numeric opcode");
    }
    return len;
  }

  void DoReturn() {
    size_t return_count = this->sig_->return_count();
    if (return_count > 1) {
      this->detected_->Add(kFeature_mv);
    }
    DCHECK_GE(stack_.size(), return_count);
    Vector<Value> return_values =
        return_count == 0
            ? Vector<Value>{}
            : Vector<Value>{&*(stack_.end() - return_count), return_count};

    CALL_INTERFACE_IF_REACHABLE(DoReturn, return_values);
  }

  inline Value* Push(ValueType type) {
    DCHECK_NE(kWasmStmt, type);
    stack_.emplace_back(this->pc_, type);
    return &stack_.back();
  }

  void PushMergeValues(Control* c, Merge<Value>* merge) {
    DCHECK_EQ(c, &control_.back());
    DCHECK(merge == &c->start_merge || merge == &c->end_merge);
    stack_.erase(stack_.begin() + c->stack_depth, stack_.end());
    if (merge->arity == 1) {
      stack_.push_back(merge->vals.first);
    } else {
      for (uint32_t i = 0; i < merge->arity; i++) {
        stack_.push_back(merge->vals.array[i]);
      }
    }
    DCHECK_EQ(c->stack_depth + merge->arity, stack_.size());
  }

  Value* PushReturns(const FunctionSig* sig) {
    size_t return_count = sig->return_count();
    if (return_count == 0) return nullptr;
    size_t old_size = stack_.size();
    for (size_t i = 0; i < return_count; ++i) {
      Push(sig->GetReturn(i));
    }
    return stack_.data() + old_size;
  }

  V8_INLINE Value Pop(int index, ValueType expected) {
    Value val = Pop();
    if (!VALIDATE(IsSubtypeOf(val.type, expected, this->module_) ||
                  val.type == kWasmBottom || expected == kWasmBottom)) {
      this->errorf(val.pc, "%s[%d] expected type %s, found %s of type %s",
                   SafeOpcodeNameAt(this->pc_), index, expected.type_name(),
                   SafeOpcodeNameAt(val.pc), val.type.type_name());
    }
    return val;
  }

  V8_INLINE Value Pop() {
    DCHECK(!control_.empty());
    uint32_t limit = control_.back().stack_depth;
    if (stack_.size() <= limit) {
      // Popping past the current control start in reachable code.
      if (!VALIDATE(control_.back().unreachable())) {
        this->errorf(this->pc_, "%s found empty stack",
                     SafeOpcodeNameAt(this->pc_));
      }
      return UnreachableValue(this->pc_);
    }
    Value val = stack_.back();
    stack_.pop_back();
    return val;
  }

  // Pops values from the stack, as defined by {merge}. Thereby we type-check
  // unreachable merges. Afterwards the values are pushed again on the stack
  // according to the signature in {merge}. This is done so follow-up validation
  // is possible.
  bool TypeCheckUnreachableMerge(Merge<Value>& merge, bool conditional_branch) {
    int arity = merge.arity;
    // For conditional branches, stack value '0' is the condition of the branch,
    // and the result values start at index '1'.
    int index_offset = conditional_branch ? 1 : 0;
    for (int i = arity - 1; i >= 0; --i) Pop(index_offset + i, merge[i].type);
    // Push values of the correct type back on the stack.
    for (int i = 0; i < arity; ++i) Push(merge[i].type);
    return this->ok();
  }

  int startrel(const byte* ptr) { return static_cast<int>(ptr - this->start_); }

  void FallThruTo(Control* c) {
    DCHECK_EQ(c, &control_.back());
    if (!TypeCheckFallThru()) return;
    if (!c->reachable()) return;

    if (!c->is_loop()) CALL_INTERFACE(FallThruTo, c);
    c->end_merge.reached = true;
  }

  bool TypeCheckMergeValues(Control* c, Merge<Value>* merge) {
    // This is a CHECK instead of a DCHECK because {validate} is a constexpr,
    // and a CHECK makes the whole function unreachable.
    static_assert(validate, "Call this function only within VALIDATE");
    DCHECK(merge == &c->start_merge || merge == &c->end_merge);
    DCHECK_GE(stack_.size(), c->stack_depth + merge->arity);
    // The computation of {stack_values} is only valid if {merge->arity} is >0.
    DCHECK_LT(0, merge->arity);
    Value* stack_values = &*(stack_.end() - merge->arity);
    // Typecheck the topmost {merge->arity} values on the stack.
    for (uint32_t i = 0; i < merge->arity; ++i) {
      Value& val = stack_values[i];
      Value& old = (*merge)[i];
      if (!IsSubtypeOf(val.type, old.type, this->module_)) {
        this->errorf(this->pc_, "type error in merge[%u] (expected %s, got %s)",
                     i, old.type.type_name(), val.type.type_name());
        return false;
      }
    }

    return true;
  }

  bool TypeCheckOneArmedIf(Control* c) {
    static_assert(validate, "Call this function only within VALIDATE");
    DCHECK(c->is_onearmed_if());
    DCHECK_EQ(c->start_merge.arity, c->end_merge.arity);
    for (uint32_t i = 0; i < c->start_merge.arity; ++i) {
      Value& start = c->start_merge[i];
      Value& end = c->end_merge[i];
      if (!IsSubtypeOf(start.type, end.type, this->module_)) {
        this->errorf(this->pc_, "type error in merge[%u] (expected %s, got %s)",
                     i, end.type.type_name(), start.type.type_name());
        return false;
      }
    }

    return true;
  }

  bool TypeCheckFallThru() {
    static_assert(validate, "Call this function only within VALIDATE");
    Control& c = control_.back();
    if (V8_LIKELY(c.reachable())) {
      uint32_t expected = c.end_merge.arity;
      DCHECK_GE(stack_.size(), c.stack_depth);
      uint32_t actual = static_cast<uint32_t>(stack_.size()) - c.stack_depth;
      // Fallthrus must match the arity of the control exactly.
      if (actual != expected) {
        this->errorf(
            this->pc_,
            "expected %u elements on the stack for fallthru to @%d, found %u",
            expected, startrel(c.pc), actual);
        return false;
      }
      if (expected == 0) return true;  // Fast path.

      return TypeCheckMergeValues(&c, &c.end_merge);
    }

    // Type-check an unreachable fallthru. First we do an arity check, then a
    // type check. Note that type-checking may require an adjustment of the
    // stack, if some stack values are missing to match the block signature.
    Merge<Value>& merge = c.end_merge;
    int arity = static_cast<int>(merge.arity);
    int available = static_cast<int>(stack_.size()) - c.stack_depth;
    // For fallthrus, not more than the needed values should be available.
    if (available > arity) {
      this->errorf(
          this->pc_,
          "expected %u elements on the stack for fallthru to @%d, found %u",
          arity, startrel(c.pc), available);
      return false;
    }
    // Pop all values from the stack for type checking of existing stack
    // values.
    return TypeCheckUnreachableMerge(merge, false);
  }

  enum TypeCheckBranchResult {
    kReachableBranch,
    kUnreachableBranch,
    kInvalidStack,
  };

  TypeCheckBranchResult TypeCheckBranch(Control* c, bool conditional_branch) {
    if (V8_LIKELY(control_.back().reachable())) {
      // We only do type-checking here. This is only needed during validation.
      if (!validate) return kReachableBranch;

      // Branches must have at least the number of values expected; can have
      // more.
      uint32_t expected = c->br_merge()->arity;
      if (expected == 0) return kReachableBranch;  // Fast path.
      DCHECK_GE(stack_.size(), control_.back().stack_depth);
      uint32_t actual =
          static_cast<uint32_t>(stack_.size()) - control_.back().stack_depth;
      if (expected > actual) {
        this->errorf(
            this->pc_,
            "expected %u elements on the stack for br to @%d, found %u",
            expected, startrel(c->pc), actual);
        return kInvalidStack;
      }
      return TypeCheckMergeValues(c, c->br_merge()) ? kReachableBranch
                                                    : kInvalidStack;
    }

    return TypeCheckUnreachableMerge(*c->br_merge(), conditional_branch)
               ? kUnreachableBranch
               : kInvalidStack;
  }

  bool TypeCheckReturn() {
    int num_returns = static_cast<int>(this->sig_->return_count());
    // No type checking is needed if there are no returns.
    if (num_returns == 0) return true;

    // Returns must have at least the number of values expected; can have more.
    int num_available =
        static_cast<int>(stack_.size()) - control_.back().stack_depth;
    if (num_available < num_returns) {
      this->errorf(this->pc_,
                   "expected %u elements on the stack for return, found %u",
                   num_returns, num_available);
      return false;
    }

    // Typecheck the topmost {num_returns} values on the stack.
    // This line requires num_returns > 0.
    Value* stack_values = &*(stack_.end() - num_returns);
    for (int i = 0; i < num_returns; ++i) {
      Value& val = stack_values[i];
      ValueType expected_type = this->sig_->GetReturn(i);
      if (!IsSubtypeOf(val.type, expected_type, this->module_)) {
        this->errorf(this->pc_,
                     "type error in return[%u] (expected %s, got %s)", i,
                     expected_type.type_name(), val.type.type_name());
        return false;
      }
    }
    return true;
  }

  void onFirstError() override {
    this->end_ = this->pc_;  // Terminate decoding loop.
    this->current_code_reachable_ = false;
    TRACE(" !%s\n", this->error_.message().c_str());
    CALL_INTERFACE(OnFirstError);
  }

  void BuildSimplePrototypeOperator(WasmOpcode opcode) {
    if (opcode == kExprRefEq) {
      RET_ON_PROTOTYPE_OPCODE(gc);
    }
    const FunctionSig* sig = WasmOpcodes::Signature(opcode);
    BuildSimpleOperator(opcode, sig);
  }

  void BuildSimpleOperator(WasmOpcode opcode, const FunctionSig* sig) {
    switch (sig->parameter_count()) {
      case 1: {
        Value val = Pop(0, sig->GetParam(0));
        Value* ret =
            sig->return_count() == 0 ? nullptr : Push(sig->GetReturn(0));
        CALL_INTERFACE_IF_REACHABLE(UnOp, opcode, val, ret);
        break;
      }
      case 2: {
        Value rval = Pop(1, sig->GetParam(1));
        Value lval = Pop(0, sig->GetParam(0));
        Value* ret =
            sig->return_count() == 0 ? nullptr : Push(sig->GetReturn(0));
        CALL_INTERFACE_IF_REACHABLE(BinOp, opcode, lval, rval, ret);
        break;
      }
      default:
        UNREACHABLE();
    }
  }

  void BuildSimpleOperator(WasmOpcode opcode, ValueType return_type,
                           ValueType arg_type) {
    Value val = Pop(0, arg_type);
    Value* ret = return_type == kWasmStmt ? nullptr : Push(return_type);
    CALL_INTERFACE_IF_REACHABLE(UnOp, opcode, val, ret);
  }

  void BuildSimpleOperator(WasmOpcode opcode, ValueType return_type,
                           ValueType lhs_type, ValueType rhs_type) {
    Value rval = Pop(1, rhs_type);
    Value lval = Pop(0, lhs_type);
    Value* ret = return_type == kWasmStmt ? nullptr : Push(return_type);
    CALL_INTERFACE_IF_REACHABLE(BinOp, opcode, lval, rval, ret);
  }

#define DEFINE_SIMPLE_SIG_OPERATOR(sig, ...)          \
  void BuildSimpleOperator_##sig(WasmOpcode opcode) { \
    BuildSimpleOperator(opcode, __VA_ARGS__);         \
  }
  FOREACH_SIGNATURE(DEFINE_SIMPLE_SIG_OPERATOR)
#undef DEFINE_SIMPLE_SIG_OPERATOR
};

#undef CALL_INTERFACE
#undef CALL_INTERFACE_IF_REACHABLE
#undef CALL_INTERFACE_IF_PARENT_REACHABLE

class EmptyInterface {
 public:
  static constexpr Decoder::ValidateFlag validate = Decoder::kValidate;
  using Value = ValueBase;
  using Control = ControlBase<Value>;
  using FullDecoder = WasmFullDecoder<validate, EmptyInterface>;

#define DEFINE_EMPTY_CALLBACK(name, ...) \
  void name(FullDecoder* decoder, ##__VA_ARGS__) {}
  INTERFACE_FUNCTIONS(DEFINE_EMPTY_CALLBACK)
#undef DEFINE_EMPTY_CALLBACK
};

#undef TRACE
#undef TRACE_INST_FORMAT
#undef VALIDATE
#undef CHECK_PROTOTYPE_OPCODE
#undef RET_ON_PROTOTYPE_OPCODE
#undef CHECK_PROTOTYPE_OPCODE_GEN
#undef OPCODE_ERROR

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_FUNCTION_BODY_DECODER_IMPL_H_
