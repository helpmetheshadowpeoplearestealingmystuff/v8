// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_BYTECODE_PIPELINE_H_
#define V8_INTERPRETER_BYTECODE_PIPELINE_H_

#include "src/base/compiler-specific.h"
#include "src/globals.h"
#include "src/interpreter/bytecode-register-allocator.h"
#include "src/interpreter/bytecode-register.h"
#include "src/interpreter/bytecodes.h"
#include "src/objects.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace interpreter {

class BytecodeLabel;
class BytecodeNode;
class BytecodeSourceInfo;

// Interface for bytecode pipeline stages.
class BytecodePipelineStage {
 public:
  virtual ~BytecodePipelineStage() {}

  // Write bytecode node |node| into pipeline. The node is only valid
  // for the duration of the call. Callee's should clone it if
  // deferring Write() to the next stage.
  virtual void Write(BytecodeNode* node) = 0;

  // Write jump bytecode node |node| which jumps to |label| into pipeline.
  // The node and label are only valid for the duration of the call. This call
  // implicitly ends the current basic block so should always write to the next
  // stage.
  virtual void WriteJump(BytecodeNode* node, BytecodeLabel* label) = 0;

  // Binds |label| to the current bytecode location. This call implicitly
  // ends the current basic block and so any deferred bytecodes should be
  // written to the next stage.
  virtual void BindLabel(BytecodeLabel* label) = 0;

  // Binds |label| to the location of |target|. This call implicitly
  // ends the current basic block and so any deferred bytecodes should be
  // written to the next stage.
  virtual void BindLabel(const BytecodeLabel& target, BytecodeLabel* label) = 0;

  // Flush the pipeline and generate a bytecode array.
  virtual Handle<BytecodeArray> ToBytecodeArray(
      Isolate* isolate, int register_count, int parameter_count,
      Handle<FixedArray> handler_table) = 0;
};

// Source code position information.
class BytecodeSourceInfo final {
 public:
  static const int kUninitializedPosition = -1;

  BytecodeSourceInfo()
      : position_type_(PositionType::kNone),
        source_position_(kUninitializedPosition) {}

  BytecodeSourceInfo(int source_position, bool is_statement)
      : position_type_(is_statement ? PositionType::kStatement
                                    : PositionType::kExpression),
        source_position_(source_position) {
    DCHECK_GE(source_position, 0);
  }

  // Makes instance into a statement position.
  void MakeStatementPosition(int source_position) {
    // Statement positions can be replaced by other statement
    // positions. For example , "for (x = 0; x < 3; ++x) 7;" has a
    // statement position associated with 7 but no bytecode associated
    // with it. Then Next is emitted after the body and has
    // statement position and overrides the existing one.
    position_type_ = PositionType::kStatement;
    source_position_ = source_position;
  }

  // Makes instance into an expression position. Instance should not
  // be a statement position otherwise it could be lost and impair the
  // debugging experience.
  void MakeExpressionPosition(int source_position) {
    DCHECK(!is_statement());
    position_type_ = PositionType::kExpression;
    source_position_ = source_position;
  }

  // Forces an instance into an expression position.
  void ForceExpressionPosition(int source_position) {
    position_type_ = PositionType::kExpression;
    source_position_ = source_position;
  }

  int source_position() const {
    DCHECK(is_valid());
    return source_position_;
  }

  bool is_statement() const {
    return position_type_ == PositionType::kStatement;
  }
  bool is_expression() const {
    return position_type_ == PositionType::kExpression;
  }

  bool is_valid() const { return position_type_ != PositionType::kNone; }
  void set_invalid() {
    position_type_ = PositionType::kNone;
    source_position_ = kUninitializedPosition;
  }

  bool operator==(const BytecodeSourceInfo& other) const {
    return position_type_ == other.position_type_ &&
           source_position_ == other.source_position_;
  }

  bool operator!=(const BytecodeSourceInfo& other) const {
    return position_type_ != other.position_type_ ||
           source_position_ != other.source_position_;
  }

 private:
  enum class PositionType : uint8_t { kNone, kExpression, kStatement };

  PositionType position_type_;
  int source_position_;
};

// A container for a generated bytecode, it's operands, and source information.
// These must be allocated by a BytecodeNodeAllocator instance.
class V8_EXPORT_PRIVATE BytecodeNode final : NON_EXPORTED_BASE(ZoneObject) {
 public:
  INLINE(BytecodeNode(Bytecode bytecode,
                      BytecodeSourceInfo source_info = BytecodeSourceInfo()))
      : bytecode_(bytecode),
        operand_count_(0),
        operand_scale_(OperandScale::kSingle),
        source_info_(source_info) {
    DCHECK_EQ(Bytecodes::NumberOfOperands(bytecode), operand_count());
  }

  INLINE(BytecodeNode(Bytecode bytecode, uint32_t operand0,
                      BytecodeSourceInfo source_info = BytecodeSourceInfo()))
      : bytecode_(bytecode),
        operand_count_(1),
        operand_scale_(OperandScale::kSingle),
        source_info_(source_info) {
    DCHECK_EQ(Bytecodes::NumberOfOperands(bytecode), operand_count());
    SetOperand(0, operand0);
  }

  INLINE(BytecodeNode(Bytecode bytecode, uint32_t operand0, uint32_t operand1,
                      BytecodeSourceInfo source_info = BytecodeSourceInfo()))
      : bytecode_(bytecode),
        operand_count_(2),
        operand_scale_(OperandScale::kSingle),
        source_info_(source_info) {
    DCHECK_EQ(Bytecodes::NumberOfOperands(bytecode), operand_count());
    SetOperand(0, operand0);
    SetOperand(1, operand1);
  }

  INLINE(BytecodeNode(Bytecode bytecode, uint32_t operand0, uint32_t operand1,
                      uint32_t operand2,
                      BytecodeSourceInfo source_info = BytecodeSourceInfo()))
      : bytecode_(bytecode),
        operand_count_(3),
        operand_scale_(OperandScale::kSingle),
        source_info_(source_info) {
    DCHECK_EQ(Bytecodes::NumberOfOperands(bytecode), operand_count());
    SetOperand(0, operand0);
    SetOperand(1, operand1);
    SetOperand(2, operand2);
  }

  INLINE(BytecodeNode(Bytecode bytecode, uint32_t operand0, uint32_t operand1,
                      uint32_t operand2, uint32_t operand3,
                      BytecodeSourceInfo source_info = BytecodeSourceInfo()))
      : bytecode_(bytecode),
        operand_count_(4),
        operand_scale_(OperandScale::kSingle),
        source_info_(source_info) {
    DCHECK_EQ(Bytecodes::NumberOfOperands(bytecode), operand_count());
    SetOperand(0, operand0);
    SetOperand(1, operand1);
    SetOperand(2, operand2);
    SetOperand(3, operand3);
  }

  // Replace the bytecode of this node with |bytecode| and keep the operands.
  void replace_bytecode(Bytecode bytecode) {
    DCHECK_EQ(Bytecodes::NumberOfOperands(bytecode_),
              Bytecodes::NumberOfOperands(bytecode));
    bytecode_ = bytecode;
  }

  void set_bytecode(Bytecode bytecode) {
    DCHECK_EQ(Bytecodes::NumberOfOperands(bytecode), 0);
    bytecode_ = bytecode;
    operand_count_ = 0;
    operand_scale_ = OperandScale::kSingle;
  }

  void set_bytecode(Bytecode bytecode, uint32_t operand0) {
    DCHECK_EQ(Bytecodes::NumberOfOperands(bytecode), 1);
    bytecode_ = bytecode;
    operand_count_ = 1;
    operand_scale_ = OperandScale::kSingle;
    SetOperand(0, operand0);
  }

  void set_bytecode(Bytecode bytecode, uint32_t operand0, uint32_t operand1) {
    DCHECK_EQ(Bytecodes::NumberOfOperands(bytecode), 2);
    bytecode_ = bytecode;
    operand_count_ = 2;
    operand_scale_ = OperandScale::kSingle;
    SetOperand(0, operand0);
    SetOperand(1, operand1);
  }

  void set_bytecode(Bytecode bytecode, uint32_t operand0, uint32_t operand1,
                    uint32_t operand2) {
    DCHECK_EQ(Bytecodes::NumberOfOperands(bytecode), 3);
    bytecode_ = bytecode;
    operand_count_ = 3;
    operand_scale_ = OperandScale::kSingle;
    SetOperand(0, operand0);
    SetOperand(1, operand1);
    SetOperand(2, operand2);
  }

  // Print to stream |os|.
  void Print(std::ostream& os) const;

  // Transform to a node representing |new_bytecode| which has one
  // operand more than the current bytecode.
  void Transform(Bytecode new_bytecode, uint32_t extra_operand) {
    DCHECK_EQ(Bytecodes::NumberOfOperands(new_bytecode),
              Bytecodes::NumberOfOperands(bytecode()) + 1);
    DCHECK(Bytecodes::NumberOfOperands(bytecode()) < 1 ||
           Bytecodes::GetOperandType(new_bytecode, 0) ==
               Bytecodes::GetOperandType(bytecode(), 0));
    DCHECK(Bytecodes::NumberOfOperands(bytecode()) < 2 ||
           Bytecodes::GetOperandType(new_bytecode, 1) ==
               Bytecodes::GetOperandType(bytecode(), 1));
    DCHECK(Bytecodes::NumberOfOperands(bytecode()) < 3 ||
           Bytecodes::GetOperandType(new_bytecode, 2) ==
               Bytecodes::GetOperandType(bytecode(), 2));
    DCHECK(Bytecodes::NumberOfOperands(bytecode()) < 4);

    bytecode_ = new_bytecode;
    operand_count_++;
    SetOperand(operand_count() - 1, extra_operand);
  }

  Bytecode bytecode() const { return bytecode_; }

  uint32_t operand(int i) const {
    DCHECK_LT(i, operand_count());
    return operands_[i];
  }
  const uint32_t* operands() const { return operands_; }

  int operand_count() const { return operand_count_; }
  OperandScale operand_scale() const { return operand_scale_; }

  const BytecodeSourceInfo& source_info() const { return source_info_; }
  void set_source_info(BytecodeSourceInfo source_info) {
    source_info_ = source_info;
  }

  bool operator==(const BytecodeNode& other) const;
  bool operator!=(const BytecodeNode& other) const { return !(*this == other); }

 private:
  INLINE(void UpdateScaleForOperand(int operand_index, uint32_t operand)) {
    if (Bytecodes::OperandIsScalableSignedByte(bytecode(), operand_index)) {
      operand_scale_ =
          std::max(operand_scale_, Bytecodes::ScaleForSignedOperand(operand));
    } else if (Bytecodes::OperandIsScalableUnsignedByte(bytecode(),
                                                        operand_index)) {
      operand_scale_ =
          std::max(operand_scale_, Bytecodes::ScaleForUnsignedOperand(operand));
    }
  }

  INLINE(void SetOperand(int operand_index, uint32_t operand)) {
    operands_[operand_index] = operand;
    UpdateScaleForOperand(operand_index, operand);
  }

  Bytecode bytecode_;
  uint32_t operands_[Bytecodes::kMaxOperands];
  int operand_count_;
  OperandScale operand_scale_;
  BytecodeSourceInfo source_info_;
};

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os,
                                           const BytecodeSourceInfo& info);
V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os,
                                           const BytecodeNode& node);

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_BYTECODE_PIPELINE_H_
