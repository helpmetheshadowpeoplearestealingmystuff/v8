// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecode-array-accessor.h"

#include "src/interpreter/bytecode-decoder.h"
#include "src/interpreter/interpreter-intrinsics.h"
#include "src/objects/code-inl.h"
#include "src/objects/feedback-vector.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {
namespace interpreter {

namespace {

class OnHeapBytecodeArray final : public AbstractBytecodeArray {
 public:
  explicit OnHeapBytecodeArray(Handle<BytecodeArray> bytecode_array)
      : AbstractBytecodeArray(true), array_(bytecode_array) {
    // TODO(verwaest): Pass the LocalHeap into the constructor.
    local_heap_ = LocalHeap::Current();
    if (!local_heap_) {
      local_heap_ = Isolate::Current()->main_thread_local_heap();
    }
  }

  int length() const override { return array_->length(); }

  int parameter_count() const override { return array_->parameter_count(); }

  Address GetFirstBytecodeAddress() const override {
    return array_->GetFirstBytecodeAddress();
  }

  Handle<Object> GetConstantAtIndex(int index,
                                    Isolate* isolate) const override {
    return handle(array_->constant_pool().get(index), isolate);
  }

  bool IsConstantAtIndexSmi(int index) const override {
    return array_->constant_pool().get(index).IsSmi();
  }

  Smi GetConstantAtIndexAsSmi(int index) const override {
    return Smi::cast(array_->constant_pool().get(index));
  }

  LocalHeap* local_heap() const override { return local_heap_; }

 private:
  LocalHeap* local_heap_;
  Handle<BytecodeArray> array_;
};

}  // namespace

BytecodeArrayAccessor::BytecodeArrayAccessor(
    std::unique_ptr<AbstractBytecodeArray> bytecode_array, int initial_offset)
    : bytecode_array_(std::move(bytecode_array)),
      start_(reinterpret_cast<uint8_t*>(
          bytecode_array_->GetFirstBytecodeAddress())),
      end_(start_ + bytecode_array_->length()),
      cursor_(start_ + initial_offset),
      operand_scale_(OperandScale::kSingle),
      prefix_size_(0) {
  if (bytecode_array_->can_move()) {
    bytecode_array_->local_heap()->AddGCEpilogueCallback(UpdatePointersCallback,
                                                         this);
  }
  UpdateOperandScale();
}

BytecodeArrayAccessor::BytecodeArrayAccessor(
    Handle<BytecodeArray> bytecode_array, int initial_offset)
    : BytecodeArrayAccessor(
          std::make_unique<OnHeapBytecodeArray>(bytecode_array),
          initial_offset) {}

BytecodeArrayAccessor::~BytecodeArrayAccessor() {
  if (bytecode_array_->can_move()) {
    bytecode_array_->local_heap()->RemoveGCEpilogueCallback(
        UpdatePointersCallback, this);
  }
}

void BytecodeArrayAccessor::SetOffset(int offset) {
  if (offset < 0) return;
  cursor_ = reinterpret_cast<uint8_t*>(
      bytecode_array()->GetFirstBytecodeAddress() + offset);
  UpdateOperandScale();
}

void BytecodeArrayAccessor::ApplyDebugBreak() {
  // Get the raw bytecode from the bytecode array. This may give us a
  // scaling prefix, which we can patch with the matching debug-break
  // variant.
  uint8_t* cursor = cursor_ - prefix_size_;
  interpreter::Bytecode bytecode = interpreter::Bytecodes::FromByte(*cursor);
  if (interpreter::Bytecodes::IsDebugBreak(bytecode)) return;
  interpreter::Bytecode debugbreak =
      interpreter::Bytecodes::GetDebugBreak(bytecode);
  *cursor = interpreter::Bytecodes::ToByte(debugbreak);
}

int BytecodeArrayAccessor::current_bytecode_size() const {
  return prefix_size_ +
         Bytecodes::Size(current_bytecode(), current_operand_scale());
}

uint32_t BytecodeArrayAccessor::GetUnsignedOperand(
    int operand_index, OperandType operand_type) const {
  DCHECK_GE(operand_index, 0);
  DCHECK_LT(operand_index, Bytecodes::NumberOfOperands(current_bytecode()));
  DCHECK_EQ(operand_type,
            Bytecodes::GetOperandType(current_bytecode(), operand_index));
  DCHECK(Bytecodes::IsUnsignedOperandType(operand_type));
  Address operand_start =
      reinterpret_cast<Address>(cursor_) +
      Bytecodes::GetOperandOffset(current_bytecode(), operand_index,
                                  current_operand_scale());
  return BytecodeDecoder::DecodeUnsignedOperand(operand_start, operand_type,
                                                current_operand_scale());
}

int32_t BytecodeArrayAccessor::GetSignedOperand(
    int operand_index, OperandType operand_type) const {
  DCHECK_GE(operand_index, 0);
  DCHECK_LT(operand_index, Bytecodes::NumberOfOperands(current_bytecode()));
  DCHECK_EQ(operand_type,
            Bytecodes::GetOperandType(current_bytecode(), operand_index));
  DCHECK(!Bytecodes::IsUnsignedOperandType(operand_type));
  Address operand_start =
      reinterpret_cast<Address>(cursor_) +
      Bytecodes::GetOperandOffset(current_bytecode(), operand_index,
                                  current_operand_scale());
  return BytecodeDecoder::DecodeSignedOperand(operand_start, operand_type,
                                              current_operand_scale());
}

uint32_t BytecodeArrayAccessor::GetFlagOperand(int operand_index) const {
  DCHECK_EQ(Bytecodes::GetOperandType(current_bytecode(), operand_index),
            OperandType::kFlag8);
  return GetUnsignedOperand(operand_index, OperandType::kFlag8);
}

uint32_t BytecodeArrayAccessor::GetUnsignedImmediateOperand(
    int operand_index) const {
  DCHECK_EQ(Bytecodes::GetOperandType(current_bytecode(), operand_index),
            OperandType::kUImm);
  return GetUnsignedOperand(operand_index, OperandType::kUImm);
}

int32_t BytecodeArrayAccessor::GetImmediateOperand(int operand_index) const {
  DCHECK_EQ(Bytecodes::GetOperandType(current_bytecode(), operand_index),
            OperandType::kImm);
  return GetSignedOperand(operand_index, OperandType::kImm);
}

uint32_t BytecodeArrayAccessor::GetRegisterCountOperand(
    int operand_index) const {
  DCHECK_EQ(Bytecodes::GetOperandType(current_bytecode(), operand_index),
            OperandType::kRegCount);
  return GetUnsignedOperand(operand_index, OperandType::kRegCount);
}

uint32_t BytecodeArrayAccessor::GetIndexOperand(int operand_index) const {
  OperandType operand_type =
      Bytecodes::GetOperandType(current_bytecode(), operand_index);
  DCHECK_EQ(operand_type, OperandType::kIdx);
  return GetUnsignedOperand(operand_index, operand_type);
}

FeedbackSlot BytecodeArrayAccessor::GetSlotOperand(int operand_index) const {
  int index = GetIndexOperand(operand_index);
  return FeedbackVector::ToSlot(index);
}

Register BytecodeArrayAccessor::GetReceiver() const {
  return Register::FromParameterIndex(0, bytecode_array()->parameter_count());
}

Register BytecodeArrayAccessor::GetParameter(int parameter_index) const {
  DCHECK_GE(parameter_index, 0);
  // The parameter indices are shifted by 1 (receiver is the
  // first entry).
  return Register::FromParameterIndex(parameter_index + 1,
                                      bytecode_array()->parameter_count());
}

Register BytecodeArrayAccessor::GetRegisterOperand(int operand_index) const {
  OperandType operand_type =
      Bytecodes::GetOperandType(current_bytecode(), operand_index);
  Address operand_start =
      reinterpret_cast<Address>(cursor_) +
      Bytecodes::GetOperandOffset(current_bytecode(), operand_index,
                                  current_operand_scale());
  return BytecodeDecoder::DecodeRegisterOperand(operand_start, operand_type,
                                                current_operand_scale());
}

int BytecodeArrayAccessor::GetRegisterOperandRange(int operand_index) const {
  DCHECK_LE(operand_index, Bytecodes::NumberOfOperands(current_bytecode()));
  const OperandType* operand_types =
      Bytecodes::GetOperandTypes(current_bytecode());
  OperandType operand_type = operand_types[operand_index];
  DCHECK(Bytecodes::IsRegisterOperandType(operand_type));
  if (operand_type == OperandType::kRegList ||
      operand_type == OperandType::kRegOutList) {
    return GetRegisterCountOperand(operand_index + 1);
  } else {
    return Bytecodes::GetNumberOfRegistersRepresentedBy(operand_type);
  }
}

Runtime::FunctionId BytecodeArrayAccessor::GetRuntimeIdOperand(
    int operand_index) const {
  OperandType operand_type =
      Bytecodes::GetOperandType(current_bytecode(), operand_index);
  DCHECK_EQ(operand_type, OperandType::kRuntimeId);
  uint32_t raw_id = GetUnsignedOperand(operand_index, operand_type);
  return static_cast<Runtime::FunctionId>(raw_id);
}

uint32_t BytecodeArrayAccessor::GetNativeContextIndexOperand(
    int operand_index) const {
  OperandType operand_type =
      Bytecodes::GetOperandType(current_bytecode(), operand_index);
  DCHECK_EQ(operand_type, OperandType::kNativeContextIndex);
  return GetUnsignedOperand(operand_index, operand_type);
}

Runtime::FunctionId BytecodeArrayAccessor::GetIntrinsicIdOperand(
    int operand_index) const {
  OperandType operand_type =
      Bytecodes::GetOperandType(current_bytecode(), operand_index);
  DCHECK_EQ(operand_type, OperandType::kIntrinsicId);
  uint32_t raw_id = GetUnsignedOperand(operand_index, operand_type);
  return IntrinsicsHelper::ToRuntimeId(
      static_cast<IntrinsicsHelper::IntrinsicId>(raw_id));
}

Handle<Object> BytecodeArrayAccessor::GetConstantAtIndex(
    int index, Isolate* isolate) const {
  return bytecode_array()->GetConstantAtIndex(index, isolate);
}

bool BytecodeArrayAccessor::IsConstantAtIndexSmi(int index) const {
  return bytecode_array()->IsConstantAtIndexSmi(index);
}

Smi BytecodeArrayAccessor::GetConstantAtIndexAsSmi(int index) const {
  return bytecode_array()->GetConstantAtIndexAsSmi(index);
}

Handle<Object> BytecodeArrayAccessor::GetConstantForIndexOperand(
    int operand_index, Isolate* isolate) const {
  return GetConstantAtIndex(GetIndexOperand(operand_index), isolate);
}

int BytecodeArrayAccessor::GetRelativeJumpTargetOffset() const {
  Bytecode bytecode = current_bytecode();
  if (interpreter::Bytecodes::IsJumpImmediate(bytecode)) {
    int relative_offset = GetUnsignedImmediateOperand(0);
    if (bytecode == Bytecode::kJumpLoop) {
      relative_offset = -relative_offset;
    }
    return relative_offset;
  } else if (interpreter::Bytecodes::IsJumpConstant(bytecode)) {
    Smi smi = GetConstantAtIndexAsSmi(GetIndexOperand(0));
    return smi.value();
  } else {
    UNREACHABLE();
  }
}

int BytecodeArrayAccessor::GetJumpTargetOffset() const {
  return GetAbsoluteOffset(GetRelativeJumpTargetOffset());
}

JumpTableTargetOffsets BytecodeArrayAccessor::GetJumpTableTargetOffsets()
    const {
  uint32_t table_start, table_size;
  int32_t case_value_base;
  if (current_bytecode() == Bytecode::kSwitchOnGeneratorState) {
    table_start = GetIndexOperand(1);
    table_size = GetUnsignedImmediateOperand(2);
    case_value_base = 0;
  } else {
    DCHECK_EQ(current_bytecode(), Bytecode::kSwitchOnSmiNoFeedback);
    table_start = GetIndexOperand(0);
    table_size = GetUnsignedImmediateOperand(1);
    case_value_base = GetImmediateOperand(2);
  }
  return JumpTableTargetOffsets(this, table_start, table_size, case_value_base);
}

int BytecodeArrayAccessor::GetAbsoluteOffset(int relative_offset) const {
  return current_offset() + relative_offset + prefix_size_;
}

std::ostream& BytecodeArrayAccessor::PrintTo(std::ostream& os) const {
  return BytecodeDecoder::Decode(os, cursor_ - prefix_size_,
                                 bytecode_array()->parameter_count());
}

JumpTableTargetOffsets::JumpTableTargetOffsets(
    const BytecodeArrayAccessor* accessor, int table_start, int table_size,
    int case_value_base)
    : accessor_(accessor),
      table_start_(table_start),
      table_size_(table_size),
      case_value_base_(case_value_base) {}

JumpTableTargetOffsets::iterator JumpTableTargetOffsets::begin() const {
  return iterator(case_value_base_, table_start_, table_start_ + table_size_,
                  accessor_);
}
JumpTableTargetOffsets::iterator JumpTableTargetOffsets::end() const {
  return iterator(case_value_base_ + table_size_, table_start_ + table_size_,
                  table_start_ + table_size_, accessor_);
}
int JumpTableTargetOffsets::size() const {
  int ret = 0;
  // TODO(leszeks): Is there a more efficient way of doing this than iterating?
  for (const auto& entry : *this) {
    USE(entry);
    ret++;
  }
  return ret;
}

JumpTableTargetOffsets::iterator::iterator(
    int case_value, int table_offset, int table_end,
    const BytecodeArrayAccessor* accessor)
    : accessor_(accessor),
      current_(Smi::zero()),
      index_(case_value),
      table_offset_(table_offset),
      table_end_(table_end) {
  UpdateAndAdvanceToValid();
}

JumpTableTargetOffset JumpTableTargetOffsets::iterator::operator*() {
  DCHECK_LT(table_offset_, table_end_);
  return {index_, accessor_->GetAbsoluteOffset(Smi::ToInt(current_))};
}

JumpTableTargetOffsets::iterator& JumpTableTargetOffsets::iterator::
operator++() {
  DCHECK_LT(table_offset_, table_end_);
  ++table_offset_;
  ++index_;
  UpdateAndAdvanceToValid();
  return *this;
}

bool JumpTableTargetOffsets::iterator::operator!=(
    const JumpTableTargetOffsets::iterator& other) {
  DCHECK_EQ(accessor_, other.accessor_);
  DCHECK_EQ(table_end_, other.table_end_);
  DCHECK_EQ(index_ - other.index_, table_offset_ - other.table_offset_);
  return index_ != other.index_;
}

void JumpTableTargetOffsets::iterator::UpdateAndAdvanceToValid() {
  while (table_offset_ < table_end_ &&
         !accessor_->IsConstantAtIndexSmi(table_offset_)) {
    ++table_offset_;
    ++index_;
  }

  // Make sure we haven't reached the end of the table with a hole in current.
  if (table_offset_ < table_end_) {
    DCHECK(accessor_->IsConstantAtIndexSmi(table_offset_));
    current_ = accessor_->GetConstantAtIndexAsSmi(table_offset_);
  }
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
