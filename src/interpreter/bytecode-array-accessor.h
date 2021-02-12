// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_BYTECODE_ARRAY_ACCESSOR_H_
#define V8_INTERPRETER_BYTECODE_ARRAY_ACCESSOR_H_

#include <memory>

#include "src/base/optional.h"
#include "src/common/globals.h"
#include "src/handles/handles.h"
#include "src/interpreter/bytecode-register.h"
#include "src/interpreter/bytecodes.h"
#include "src/objects/objects.h"
#include "src/objects/smi.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {

class BytecodeArray;

namespace interpreter {

class BytecodeArrayAccessor;

struct V8_EXPORT_PRIVATE JumpTableTargetOffset {
  int case_value;
  int target_offset;
};

class V8_EXPORT_PRIVATE JumpTableTargetOffsets final {
 public:
  // Minimal iterator implementation for use in ranged-for.
  class V8_EXPORT_PRIVATE iterator final {
   public:
    iterator(int case_value, int table_offset, int table_end,
             const BytecodeArrayAccessor* accessor);

    JumpTableTargetOffset operator*();
    iterator& operator++();
    bool operator!=(const iterator& other);

   private:
    void UpdateAndAdvanceToValid();

    const BytecodeArrayAccessor* accessor_;
    Smi current_;
    int index_;
    int table_offset_;
    int table_end_;
  };

  JumpTableTargetOffsets(const BytecodeArrayAccessor* accessor, int table_start,
                         int table_size, int case_value_base);

  iterator begin() const;
  iterator end() const;

  int size() const;

 private:
  const BytecodeArrayAccessor* accessor_;
  int table_start_;
  int table_size_;
  int case_value_base_;
};

class V8_EXPORT_PRIVATE AbstractBytecodeArray {
 public:
  AbstractBytecodeArray();
  virtual int length() const = 0;
  virtual int parameter_count() const = 0;
  virtual Address GetFirstBytecodeAddress() const = 0;

  virtual Handle<Object> GetConstantAtIndex(int index,
                                            Isolate* isolate) const = 0;
  virtual bool IsConstantAtIndexSmi(int index) const = 0;
  virtual Smi GetConstantAtIndexAsSmi(int index) const = 0;

  virtual ~AbstractBytecodeArray() = default;
  LocalHeap* local_heap() const { return local_heap_; }

 private:
  LocalHeap* local_heap_;
};

class V8_EXPORT_PRIVATE BytecodeArrayAccessor {
 public:
  BytecodeArrayAccessor(std::unique_ptr<AbstractBytecodeArray> bytecode_array,
                        int initial_offset);

  BytecodeArrayAccessor(Handle<BytecodeArray> bytecode_array,
                        int initial_offset);
  ~BytecodeArrayAccessor();

  BytecodeArrayAccessor(const BytecodeArrayAccessor&) = delete;
  BytecodeArrayAccessor& operator=(const BytecodeArrayAccessor&) = delete;

  inline void Advance() {
    cursor_ += Bytecodes::Size(current_bytecode(), current_operand_scale());
    UpdateOperandScale();
  }
  void SetOffset(int offset);
  void Reset() { SetOffset(0); }

  void ApplyDebugBreak();

  inline Bytecode current_bytecode() const {
    DCHECK(!done());
    uint8_t current_byte = *cursor_;
    Bytecode current_bytecode = Bytecodes::FromByte(current_byte);
    DCHECK(!Bytecodes::IsPrefixScalingBytecode(current_bytecode));
    return current_bytecode;
  }
  int current_bytecode_size() const;
  int current_offset() const {
    return static_cast<int>(cursor_ - start_ - prefix_size_);
  }
  OperandScale current_operand_scale() const { return operand_scale_; }
  AbstractBytecodeArray* bytecode_array() const {
    return bytecode_array_.get();
  }

  uint32_t GetFlagOperand(int operand_index) const;
  uint32_t GetUnsignedImmediateOperand(int operand_index) const;
  int32_t GetImmediateOperand(int operand_index) const;
  uint32_t GetIndexOperand(int operand_index) const;
  FeedbackSlot GetSlotOperand(int operand_index) const;
  Register GetReceiver() const;
  Register GetParameter(int parameter_index) const;
  uint32_t GetRegisterCountOperand(int operand_index) const;
  Register GetRegisterOperand(int operand_index) const;
  std::pair<Register, Register> GetRegisterPairOperand(int operand_index) const;
  RegisterList GetRegisterListOperand(int operand_index) const;
  int GetRegisterOperandRange(int operand_index) const;
  Runtime::FunctionId GetRuntimeIdOperand(int operand_index) const;
  Runtime::FunctionId GetIntrinsicIdOperand(int operand_index) const;
  uint32_t GetNativeContextIndexOperand(int operand_index) const;
  Handle<Object> GetConstantAtIndex(int offset, Isolate* isolate) const;
  bool IsConstantAtIndexSmi(int offset) const;
  Smi GetConstantAtIndexAsSmi(int offset) const;
  Handle<Object> GetConstantForIndexOperand(int operand_index,
                                            Isolate* isolate) const;

  // Returns the relative offset of the branch target at the current bytecode.
  // It is an error to call this method if the bytecode is not for a jump or
  // conditional jump. Returns a negative offset for backward jumps.
  int GetRelativeJumpTargetOffset() const;
  // Returns the absolute offset of the branch target at the current bytecode.
  // It is an error to call this method if the bytecode is not for a jump or
  // conditional jump.
  int GetJumpTargetOffset() const;
  // Returns an iterator over the absolute offsets of the targets of the current
  // switch bytecode's jump table. It is an error to call this method if the
  // bytecode is not a switch.
  JumpTableTargetOffsets GetJumpTableTargetOffsets() const;

  // Returns the absolute offset of the bytecode at the given relative offset
  // from the current bytecode.
  int GetAbsoluteOffset(int relative_offset) const;

  std::ostream& PrintTo(std::ostream& os) const;

  static void UpdatePointersCallback(void* accessor) {
    reinterpret_cast<BytecodeArrayAccessor*>(accessor)->UpdatePointers();
  }

  void UpdatePointers() {
    DisallowGarbageCollection no_gc;
    uint8_t* start =
        reinterpret_cast<uint8_t*>(bytecode_array_->GetFirstBytecodeAddress());
    if (start != start_) {
      start_ = start;
      uint8_t* end = start + bytecode_array_->length();
      size_t distance_to_end = end_ - cursor_;
      cursor_ = end - distance_to_end;
      end_ = end;
    }
  }

  inline bool done() const { return cursor_ >= end_; }

 private:
  uint32_t GetUnsignedOperand(int operand_index,
                              OperandType operand_type) const;
  int32_t GetSignedOperand(int operand_index, OperandType operand_type) const;

  inline void UpdateOperandScale() {
    if (done()) return;
    uint8_t current_byte = *cursor_;
    Bytecode current_bytecode = Bytecodes::FromByte(current_byte);
    if (Bytecodes::IsPrefixScalingBytecode(current_bytecode)) {
      operand_scale_ =
          Bytecodes::PrefixBytecodeToOperandScale(current_bytecode);
      ++cursor_;
      prefix_size_ = 1;
    } else {
      operand_scale_ = OperandScale::kSingle;
      prefix_size_ = 0;
    }
  }

  std::unique_ptr<AbstractBytecodeArray> bytecode_array_;
  uint8_t* start_;
  uint8_t* end_;
  // The cursor always points to the active bytecode. If there's a prefix, the
  // prefix is at (cursor - 1).
  uint8_t* cursor_;
  OperandScale operand_scale_;
  int prefix_size_;
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_BYTECODE_ARRAY_ACCESSOR_H_
