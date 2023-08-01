// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/operations.h"

#include <atomic>
#include <sstream>

#include "src/base/logging.h"
#include "src/base/optional.h"
#include "src/base/platform/mutex.h"
#include "src/codegen/bailout-reason.h"
#include "src/codegen/machine-type.h"
#include "src/common/globals.h"
#include "src/compiler/backend/instruction-selector.h"
#include "src/compiler/frame-states.h"
#include "src/compiler/graph-visualizer.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/turboshaft/deopt-data.h"
#include "src/compiler/turboshaft/graph.h"
#include "src/handles/handles-inl.h"
#include "src/handles/maybe-handles-inl.h"

#ifdef DEBUG
// For InWritableSharedSpace
#include "src/objects/objects-inl.h"
#endif

namespace v8::internal {
std::ostream& operator<<(std::ostream& os, AbortReason reason) {
  return os << GetAbortReason(reason);
}
}  // namespace v8::internal

namespace v8::internal::compiler::turboshaft {

void Print(const Operation& op) { std::cout << op << "\n"; }

Zone* get_zone(Graph* graph) { return graph->graph_zone(); }

bool AllowImplicitRepresentationChange(RegisterRepresentation actual_rep,
                                       RegisterRepresentation expected_rep) {
  if (actual_rep == expected_rep) {
    return true;
  }
  switch (expected_rep.value()) {
    case RegisterRepresentation::Word32():
      // TODO(mliedtke): Remove this once JS graph building and JS reducers
      // always produce explicit truncations.
      // We allow implicit 64- to 32-bit truncation.
      if (actual_rep == RegisterRepresentation::Word64()) {
        return true;
      }
      // We allow implicit tagged -> untagged conversions.
      // Even without pointer compression, we use `Word32And` for Smi-checks on
      // tagged values.
      if (actual_rep == any_of(RegisterRepresentation::Tagged(),
                               RegisterRepresentation::Compressed())) {
        return true;
      }
      break;
    case RegisterRepresentation::Word64():
      // We allow implicit tagged -> untagged conversions.
      if (kTaggedSize == kInt64Size &&
          actual_rep == RegisterRepresentation::Tagged()) {
        return true;
      }
      break;
    case RegisterRepresentation::Tagged():
      // We allow implicit untagged -> tagged conversions. This is only safe for
      // Smi values.
      if (actual_rep == RegisterRepresentation::PointerSized()) {
        return true;
      }
      break;
    case RegisterRepresentation::Compressed():
      // Compression is a no-op.
      if (actual_rep == any_of(RegisterRepresentation::Tagged(),
                               RegisterRepresentation::PointerSized(),
                               RegisterRepresentation::Word32())) {
        return true;
      }
      break;
    default:
      break;
  }
  return false;
}

bool ValidOpInputRep(
    const Graph& graph, OpIndex input,
    std::initializer_list<RegisterRepresentation> expected_reps,
    base::Optional<size_t> projection_index) {
  base::Vector<const RegisterRepresentation> input_reps =
      graph.Get(input).outputs_rep();
  RegisterRepresentation input_rep;
  if (projection_index) {
    if (*projection_index < input_reps.size()) {
      input_rep = input_reps[*projection_index];
    } else {
      std::cerr << "Turboshaft operation has input #" << input
                << " with wrong arity.\n";
      std::cerr << "Input has results " << PrintCollection(input_reps)
                << ", but expected at least " << (*projection_index + 1)
                << " results.\n";
      return false;
    }
  } else if (input_reps.size() == 1) {
    input_rep = input_reps[0];
  } else {
    std::cerr << "Turboshaft operation has input #" << input
              << " with wrong arity.\n";
    std::cerr << "Expected a single output but found " << input_reps.size()
              << ".\n";
    return false;
  }
  for (RegisterRepresentation expected_rep : expected_reps) {
    if (AllowImplicitRepresentationChange(input_rep, expected_rep)) {
      return true;
    }
  }
  std::cerr << "Turboshaft operation has input #" << input
            << " with wrong representation.\n";
  std::cerr << "Expected " << (expected_reps.size() > 1 ? "one of " : "")
            << PrintCollection(expected_reps).WithoutBrackets() << " but found "
            << input_rep << ".\n";
  return false;
}

bool ValidOpInputRep(const Graph& graph, OpIndex input,
                     RegisterRepresentation expected_rep,
                     base::Optional<size_t> projection_index) {
  return ValidOpInputRep(graph, input, {expected_rep}, projection_index);
}

const char* OpcodeName(Opcode opcode) {
#define OPCODE_NAME(Name) #Name,
  const char* table[kNumberOfOpcodes] = {
      TURBOSHAFT_OPERATION_LIST(OPCODE_NAME)};
#undef OPCODE_NAME
  return table[OpcodeIndex(opcode)];
}

std::ostream& operator<<(std::ostream& os, OperationPrintStyle styled_op) {
  const Operation& op = styled_op.op;
  os << OpcodeName(op.opcode);
  op.PrintInputs(os, styled_op.op_index_prefix);
  op.PrintOptions(os);
  return os;
}

std::ostream& operator<<(std::ostream& os, WordUnaryOp::Kind kind) {
  switch (kind) {
    case WordUnaryOp::Kind::kReverseBytes:
      return os << "ReverseBytes";
    case WordUnaryOp::Kind::kCountLeadingZeros:
      return os << "CountLeadingZeros";
    case WordUnaryOp::Kind::kCountTrailingZeros:
      return os << "CountTrailingZeros";
    case WordUnaryOp::Kind::kPopCount:
      return os << "PopCount";
    case WordUnaryOp::Kind::kSignExtend8:
      return os << "SignExtend8";
    case WordUnaryOp::Kind::kSignExtend16:
      return os << "SignExtend16";
  }
}

std::ostream& operator<<(std::ostream& os, FloatUnaryOp::Kind kind) {
  switch (kind) {
    case FloatUnaryOp::Kind::kAbs:
      return os << "Abs";
    case FloatUnaryOp::Kind::kNegate:
      return os << "Negate";
    case FloatUnaryOp::Kind::kSilenceNaN:
      return os << "SilenceNaN";
    case FloatUnaryOp::Kind::kRoundUp:
      return os << "RoundUp";
    case FloatUnaryOp::Kind::kRoundDown:
      return os << "RoundDown";
    case FloatUnaryOp::Kind::kRoundToZero:
      return os << "RoundToZero";
    case FloatUnaryOp::Kind::kRoundTiesEven:
      return os << "RoundTiesEven";
    case FloatUnaryOp::Kind::kLog:
      return os << "Log";
    case FloatUnaryOp::Kind::kLog2:
      return os << "Log2";
    case FloatUnaryOp::Kind::kLog10:
      return os << "Log10";
    case FloatUnaryOp::Kind::kLog1p:
      return os << "Log1p";
    case FloatUnaryOp::Kind::kSqrt:
      return os << "Sqrt";
    case FloatUnaryOp::Kind::kCbrt:
      return os << "Cbrt";
    case FloatUnaryOp::Kind::kExp:
      return os << "Exp";
    case FloatUnaryOp::Kind::kExpm1:
      return os << "Expm1";
    case FloatUnaryOp::Kind::kSin:
      return os << "Sin";
    case FloatUnaryOp::Kind::kCos:
      return os << "Cos";
    case FloatUnaryOp::Kind::kAsin:
      return os << "Asin";
    case FloatUnaryOp::Kind::kAcos:
      return os << "Acos";
    case FloatUnaryOp::Kind::kSinh:
      return os << "Sinh";
    case FloatUnaryOp::Kind::kCosh:
      return os << "Cosh";
    case FloatUnaryOp::Kind::kAsinh:
      return os << "Asinh";
    case FloatUnaryOp::Kind::kAcosh:
      return os << "Acosh";
    case FloatUnaryOp::Kind::kTan:
      return os << "Tan";
    case FloatUnaryOp::Kind::kTanh:
      return os << "Tanh";
    case FloatUnaryOp::Kind::kAtan:
      return os << "Atan";
    case FloatUnaryOp::Kind::kAtanh:
      return os << "Atanh";
  }
}

// static
bool FloatUnaryOp::IsSupported(Kind kind, FloatRepresentation rep) {
  switch (rep.value()) {
    case FloatRepresentation::Float32():
      switch (kind) {
        case Kind::kRoundDown:
          return SupportedOperations::float32_round_down();
        case Kind::kRoundUp:
          return SupportedOperations::float32_round_up();
        case Kind::kRoundToZero:
          return SupportedOperations::float32_round_to_zero();
        case Kind::kRoundTiesEven:
          return SupportedOperations::float32_round_ties_even();
        default:
          return true;
      }
    case FloatRepresentation::Float64():
      switch (kind) {
        case Kind::kRoundDown:
          return SupportedOperations::float64_round_down();
        case Kind::kRoundUp:
          return SupportedOperations::float64_round_up();
        case Kind::kRoundToZero:
          return SupportedOperations::float64_round_to_zero();
        case Kind::kRoundTiesEven:
          return SupportedOperations::float64_round_ties_even();
        default:
          return true;
      }
  }
}

// static
bool WordUnaryOp::IsSupported(Kind kind, WordRepresentation rep) {
  switch (kind) {
    case Kind::kCountLeadingZeros:
    case Kind::kReverseBytes:
    case Kind::kSignExtend8:
    case Kind::kSignExtend16:
      return true;
    case Kind::kCountTrailingZeros:
      return rep == WordRepresentation::Word32()
                 ? SupportedOperations::word32_ctz()
                 : SupportedOperations::word64_ctz();
    case Kind::kPopCount:
      return rep == WordRepresentation::Word32()
                 ? SupportedOperations::word32_popcnt()
                 : SupportedOperations::word64_popcnt();
  }
}

std::ostream& operator<<(std::ostream& os, ShiftOp::Kind kind) {
  switch (kind) {
    case ShiftOp::Kind::kShiftRightArithmeticShiftOutZeros:
      return os << "ShiftRightArithmeticShiftOutZeros";
    case ShiftOp::Kind::kShiftRightArithmetic:
      return os << "ShiftRightArithmetic";
    case ShiftOp::Kind::kShiftRightLogical:
      return os << "ShiftRightLogical";
    case ShiftOp::Kind::kShiftLeft:
      return os << "ShiftLeft";
    case ShiftOp::Kind::kRotateRight:
      return os << "RotateRight";
    case ShiftOp::Kind::kRotateLeft:
      return os << "RotateLeft";
  }
}

std::ostream& operator<<(std::ostream& os, ComparisonOp::Kind kind) {
  switch (kind) {
    case ComparisonOp::Kind::kSignedLessThan:
      return os << "SignedLessThan";
    case ComparisonOp::Kind::kSignedLessThanOrEqual:
      return os << "SignedLessThanOrEqual";
    case ComparisonOp::Kind::kUnsignedLessThan:
      return os << "UnsignedLessThan";
    case ComparisonOp::Kind::kUnsignedLessThanOrEqual:
      return os << "UnsignedLessThanOrEqual";
  }
}

std::ostream& operator<<(std::ostream& os, ChangeOp::Kind kind) {
  switch (kind) {
    case ChangeOp::Kind::kFloatConversion:
      return os << "FloatConversion";
    case ChangeOp::Kind::kJSFloatTruncate:
      return os << "JSFloatTruncate";
    case ChangeOp::Kind::kSignedFloatTruncateOverflowToMin:
      return os << "SignedFloatTruncateOverflowToMin";
    case ChangeOp::Kind::kUnsignedFloatTruncateOverflowToMin:
      return os << "UnsignedFloatTruncateOverflowToMin";
    case ChangeOp::Kind::kSignedToFloat:
      return os << "SignedToFloat";
    case ChangeOp::Kind::kUnsignedToFloat:
      return os << "UnsignedToFloat";
    case ChangeOp::Kind::kExtractHighHalf:
      return os << "ExtractHighHalf";
    case ChangeOp::Kind::kExtractLowHalf:
      return os << "ExtractLowHalf";
    case ChangeOp::Kind::kZeroExtend:
      return os << "ZeroExtend";
    case ChangeOp::Kind::kSignExtend:
      return os << "SignExtend";
    case ChangeOp::Kind::kTruncate:
      return os << "Truncate";
    case ChangeOp::Kind::kBitcast:
      return os << "Bitcast";
  }
}

std::ostream& operator<<(std::ostream& os, ChangeOrDeoptOp::Kind kind) {
  switch (kind) {
    case ChangeOrDeoptOp::Kind::kUint32ToInt32:
      return os << "Uint32ToInt32";
    case ChangeOrDeoptOp::Kind::kInt64ToInt32:
      return os << "Int64ToInt32";
    case ChangeOrDeoptOp::Kind::kUint64ToInt32:
      return os << "Uint64ToInt32";
    case ChangeOrDeoptOp::Kind::kUint64ToInt64:
      return os << "Uint64ToInt64";
    case ChangeOrDeoptOp::Kind::kFloat64ToInt32:
      return os << "Float64ToInt32";
    case ChangeOrDeoptOp::Kind::kFloat64ToInt64:
      return os << "Float64ToInt64";
    case ChangeOrDeoptOp::Kind::kFloat64NotHole:
      return os << "Float64NotHole";
  }
}

std::ostream& operator<<(std::ostream& os, TryChangeOp::Kind kind) {
  switch (kind) {
    case TryChangeOp::Kind::kSignedFloatTruncateOverflowUndefined:
      return os << "SignedFloatTruncateOverflowUndefined";
    case TryChangeOp::Kind::kUnsignedFloatTruncateOverflowUndefined:
      return os << "UnsignedFloatTruncateOverflowUndefined";
  }
}

std::ostream& operator<<(std::ostream& os, ChangeOp::Assumption assumption) {
  switch (assumption) {
    case ChangeOp::Assumption::kNoAssumption:
      return os << "NoAssumption";
    case ChangeOp::Assumption::kNoOverflow:
      return os << "NoOverflow";
    case ChangeOp::Assumption::kReversible:
      return os << "Reversible";
  }
}

std::ostream& operator<<(std::ostream& os, SelectOp::Implementation kind) {
  switch (kind) {
    case SelectOp::Implementation::kBranch:
      return os << "Branch";
    case SelectOp::Implementation::kCMove:
      return os << "CMove";
  }
}

std::ostream& operator<<(std::ostream& os, FrameConstantOp::Kind kind) {
  switch (kind) {
    case FrameConstantOp::Kind::kStackCheckOffset:
      return os << "stack check offset";
    case FrameConstantOp::Kind::kFramePointer:
      return os << "frame pointer";
    case FrameConstantOp::Kind::kParentFramePointer:
      return os << "parent frame pointer";
  }
}

void Operation::PrintInputs(std::ostream& os,
                            const std::string& op_index_prefix) const {
  switch (opcode) {
#define SWITCH_CASE(Name)                              \
  case Opcode::k##Name:                                \
    Cast<Name##Op>().PrintInputs(os, op_index_prefix); \
    break;
    TURBOSHAFT_OPERATION_LIST(SWITCH_CASE)
#undef SWITCH_CASE
  }
}

void Operation::PrintOptions(std::ostream& os) const {
  switch (opcode) {
#define SWITCH_CASE(Name)              \
  case Opcode::k##Name:                \
    Cast<Name##Op>().PrintOptions(os); \
    break;
    TURBOSHAFT_OPERATION_LIST(SWITCH_CASE)
#undef SWITCH_CASE
  }
}

void PendingLoopPhiOp::PrintOptions(std::ostream& os) const {
  os << "[" << rep << ", ";
  switch (kind) {
    case Kind::kOldGraphIndex:
      os << "#o" << old_backedge_index();
      break;
    case Kind::kFromSeaOfNodes:
      os << "Node(#" << old_backedge_node()->id() << ")";
      break;
    case Kind::kLabelParameter:
      os << "PhiIndex(" << phi_index().index << ")";
      break;
    case Kind::kVariable:
      os << "Variable(" << &variable().data() << ")";
      break;
  }
  os << "]";
}

void ConstantOp::PrintOptions(std::ostream& os) const {
  os << "[";
  switch (kind) {
    case Kind::kWord32:
      os << "word32: " << static_cast<int32_t>(storage.integral);
      break;
    case Kind::kWord64:
      os << "word64: " << static_cast<int64_t>(storage.integral);
      break;
    case Kind::kNumber:
      os << "number: " << number();
      break;
    case Kind::kTaggedIndex:
      os << "tagged index: " << tagged_index();
      break;
    case Kind::kFloat64:
      os << "float64: " << float64();
      break;
    case Kind::kFloat32:
      os << "float32: " << float32();
      break;
    case Kind::kExternal:
      os << "external: " << external_reference();
      break;
    case Kind::kHeapObject:
      os << "heap object: " << JSONEscaped(handle());
      break;
    case Kind::kCompressedHeapObject:
      os << "compressed heap object: " << JSONEscaped(handle());
      break;
    case Kind::kRelocatableWasmCall:
      os << "relocatable wasm call: 0x"
         << reinterpret_cast<void*>(storage.integral);
      break;
    case Kind::kRelocatableWasmStubCall:
      os << "relocatable wasm stub call: 0x"
         << reinterpret_cast<void*>(storage.integral);
      break;
  }
  os << "]";
}

void ParameterOp::PrintOptions(std::ostream& os) const {
  os << "[" << parameter_index;
  if (debug_name) os << ", " << debug_name;
  os << "]";
}

void LoadOp::PrintInputs(std::ostream& os,
                         const std::string& op_index_prefix) const {
  os << " *(" << op_index_prefix << base().id();
  if (offset < 0) {
    os << " - " << -offset;
  } else if (offset > 0) {
    os << " + " << offset;
  }
  if (index().valid()) {
    os << " + " << op_index_prefix << index().id();
    if (element_size_log2 > 0) os << "*" << (1 << element_size_log2);
  }
  os << ") ";
}
void LoadOp::PrintOptions(std::ostream& os) const {
  os << "[";
  os << (kind.tagged_base ? "tagged base" : "raw");
  if (kind.maybe_unaligned) os << ", unaligned";
  os << ", " << loaded_rep;
  if (element_size_log2 != 0)
    os << ", element size: 2^" << int{element_size_log2};
  if (offset != 0) os << ", offset: " << offset;
  os << "]";
}

void StoreOp::PrintInputs(std::ostream& os,
                          const std::string& op_index_prefix) const {
  os << " *(" << op_index_prefix << base().id();
  if (offset < 0) {
    os << " - " << -offset;
  } else if (offset > 0) {
    os << " + " << offset;
  }
  if (index().valid()) {
    os << " + " << op_index_prefix << index().id();
    if (element_size_log2 > 0) os << "*" << (1 << element_size_log2);
  }
  os << ") = " << op_index_prefix << value().id() << " ";
}
void StoreOp::PrintOptions(std::ostream& os) const {
  os << "[";
  os << (kind.tagged_base ? "tagged base" : "raw");
  if (kind.maybe_unaligned) os << ", unaligned";
  os << ", " << stored_rep;
  os << ", " << write_barrier;
  if (element_size_log2 != 0)
    os << ", element size: 2^" << int{element_size_log2};
  if (offset != 0) os << ", offset: " << offset;
  if (maybe_initializing_or_transitioning) os << ", initializing";
  os << "]";
}

void AllocateOp::PrintOptions(std::ostream& os) const {
  os << "[";
  os << type << ", ";
  os << (allow_large_objects == AllowLargeObjects::kTrue ? "allow large objects"
                                                         : "no large objects");
  os << "]";
}

void DecodeExternalPointerOp::PrintOptions(std::ostream& os) const {
  os << "[";
  os << "tag: " << std::hex << tag << std::dec;
  os << "]";
}

void FrameStateOp::PrintOptions(std::ostream& os) const {
  os << "[";
  os << (inlined ? "inlined" : "not inlined");
  os << ", ";
  os << data->frame_state_info;
  os << ", state values:";
  FrameStateData::Iterator it = data->iterator(state_values());
  while (it.has_more()) {
    os << " ";
    switch (it.current_instr()) {
      case FrameStateData::Instr::kInput: {
        MachineType type;
        OpIndex input;
        it.ConsumeInput(&type, &input);
        os << "#" << input.id() << "(" << type << ")";
        break;
      }
      case FrameStateData::Instr::kUnusedRegister:
        it.ConsumeUnusedRegister();
        os << ".";
        break;
      case FrameStateData::Instr::kDematerializedObject: {
        uint32_t id;
        uint32_t field_count;
        it.ConsumeDematerializedObject(&id, &field_count);
        os << "$" << id << "(field count: " << field_count << ")";
        break;
      }
      case FrameStateData::Instr::kDematerializedObjectReference: {
        uint32_t id;
        it.ConsumeDematerializedObjectReference(&id);
        os << "$" << id;
        break;
      }
      case FrameStateData::Instr::kArgumentsElements: {
        CreateArgumentsType type;
        it.ConsumeArgumentsElements(&type);
        os << "ArgumentsElements(" << type << ")";
        break;
      }
      case FrameStateData::Instr::kArgumentsLength: {
        it.ConsumeArgumentsLength();
        os << "ArgumentsLength";
        break;
      }
    }
  }
  os << "]";
}

void FrameStateOp::Validate(const Graph& graph) const {
  if (inlined) {
    DCHECK(Get(graph, parent_frame_state()).Is<FrameStateOp>());
  }
  FrameStateData::Iterator it = data->iterator(state_values());
  while (it.has_more()) {
    switch (it.current_instr()) {
      case FrameStateData::Instr::kInput: {
        MachineType type;
        OpIndex input;
        it.ConsumeInput(&type, &input);
        RegisterRepresentation rep =
            RegisterRepresentation::FromMachineRepresentation(
                type.representation());
        if (rep == RegisterRepresentation::Tagged()) {
          // The deoptimizer can handle compressed values.
          rep = RegisterRepresentation::Compressed();
        }
        DCHECK(ValidOpInputRep(graph, input, rep));
        break;
      }
      case FrameStateData::Instr::kUnusedRegister:
        it.ConsumeUnusedRegister();
        break;
      case FrameStateData::Instr::kDematerializedObject: {
        uint32_t id;
        uint32_t field_count;
        it.ConsumeDematerializedObject(&id, &field_count);
        break;
      }
      case FrameStateData::Instr::kDematerializedObjectReference: {
        uint32_t id;
        it.ConsumeDematerializedObjectReference(&id);
        break;
      }
      case FrameStateData::Instr::kArgumentsElements: {
        CreateArgumentsType type;
        it.ConsumeArgumentsElements(&type);
        break;
      }
      case FrameStateData::Instr::kArgumentsLength: {
        it.ConsumeArgumentsLength();
        break;
      }
    }
  }
}

void DidntThrowOp::Validate(const Graph& graph) const {
#ifdef DEBUG
  DCHECK(MayThrow(graph.Get(throwing_operation()).opcode));
  switch (graph.Get(throwing_operation()).opcode) {
    case Opcode::kCall: {
      auto& call_op = graph.Get(throwing_operation()).Cast<CallOp>();
      DCHECK(call_op.descriptor->out_reps == outputs_rep());
      break;
    }
    default:
      UNREACHABLE();
  }
  // Check that `may_throw()` is either immediately before or that there is only
  // a `CheckExceptionOp` in-between.
  OpIndex this_index = graph.Index(*this);
  OpIndex in_between = graph.NextIndex(throwing_operation());
  if (has_catch_block) {
    DCHECK_NE(in_between, this_index);
    auto& catch_op = graph.Get(in_between).Cast<CheckExceptionOp>();
    DCHECK_EQ(catch_op.didnt_throw_block->begin(), this_index);
  } else {
    DCHECK_EQ(in_between, this_index);
  }
#endif
}

void WordBinopOp::PrintOptions(std::ostream& os) const {
  os << "[";
  switch (kind) {
    case Kind::kAdd:
      os << "Add, ";
      break;
    case Kind::kSub:
      os << "Sub, ";
      break;
    case Kind::kMul:
      os << "Mul, ";
      break;
    case Kind::kSignedMulOverflownBits:
      os << "SignedMulOverflownBits, ";
      break;
    case Kind::kUnsignedMulOverflownBits:
      os << "UnsignedMulOverflownBits, ";
      break;
    case Kind::kSignedDiv:
      os << "SignedDiv, ";
      break;
    case Kind::kUnsignedDiv:
      os << "UnsignedDiv, ";
      break;
    case Kind::kSignedMod:
      os << "SignedMod, ";
      break;
    case Kind::kUnsignedMod:
      os << "UnsignedMod, ";
      break;
    case Kind::kBitwiseAnd:
      os << "BitwiseAnd, ";
      break;
    case Kind::kBitwiseOr:
      os << "BitwiseOr, ";
      break;
    case Kind::kBitwiseXor:
      os << "BitwiseXor, ";
      break;
  }
  os << rep;
  os << "]";
}

void FloatBinopOp::PrintOptions(std::ostream& os) const {
  os << "[";
  switch (kind) {
    case Kind::kAdd:
      os << "Add, ";
      break;
    case Kind::kSub:
      os << "Sub, ";
      break;
    case Kind::kMul:
      os << "Mul, ";
      break;
    case Kind::kDiv:
      os << "Div, ";
      break;
    case Kind::kMod:
      os << "Mod, ";
      break;
    case Kind::kMin:
      os << "Min, ";
      break;
    case Kind::kMax:
      os << "Max, ";
      break;
    case Kind::kPower:
      os << "Power, ";
      break;
    case Kind::kAtan2:
      os << "Atan2, ";
      break;
  }
  os << rep;
  os << "]";
}

void Word32PairBinopOp::PrintOptions(std::ostream& os) const {
  os << "[";
  switch (kind) {
    case Kind::kAdd:
      os << "Add";
      break;
    case Kind::kSub:
      os << "Sub";
      break;
    case Kind::kMul:
      os << "Mul";
      break;
    case Kind::kShiftLeft:
      os << "ShiftLeft";
      break;
    case Kind::kShiftRightArithmetic:
      os << "ShiftRightSigned";
      break;
    case Kind::kShiftRightLogical:
      os << "ShiftRightUnsigned";
      break;
  }
  os << "]";
}

void OverflowCheckedBinopOp::PrintOptions(std::ostream& os) const {
  os << "[";
  switch (kind) {
    case Kind::kSignedAdd:
      os << "signed add, ";
      break;
    case Kind::kSignedSub:
      os << "signed sub, ";
      break;
    case Kind::kSignedMul:
      os << "signed mul, ";
      break;
  }
  os << rep;
  os << "]";
}

std::ostream& operator<<(std::ostream& os, OpIndex idx) {
  if (!idx.valid()) {
    return os << "<invalid OpIndex>";
  }
  return os << idx.id();
}

std::ostream& operator<<(std::ostream& os, BlockIndex b) {
  if (!b.valid()) {
    return os << "<invalid block>";
  }
  return os << 'B' << b.id();
}

std::ostream& operator<<(std::ostream& os, const Block* b) {
  return os << b->index();
}

std::ostream& operator<<(std::ostream& os, OpEffects effects) {
  auto produce_consume = [](bool produces, bool consumes) {
    if (!produces && !consumes) {
      return "🁣";
    } else if (produces && !consumes) {
      return "🁤";
    } else if (!produces && consumes) {
      return "🁪";
    } else if (produces && consumes) {
      return "🁫";
    }
    UNREACHABLE();
  };
  os << produce_consume(effects.produces.load_heap_memory,
                        effects.consumes.load_heap_memory);
  os << produce_consume(effects.produces.load_off_heap_memory,
                        effects.consumes.load_off_heap_memory);
  os << "\u2003";  // em space
  os << produce_consume(effects.produces.store_heap_memory,
                        effects.consumes.store_heap_memory);
  os << produce_consume(effects.produces.store_off_heap_memory,
                        effects.consumes.store_off_heap_memory);
  os << "\u2003";  // em space
  os << produce_consume(effects.produces.before_raw_heap_access,
                        effects.consumes.before_raw_heap_access);
  os << produce_consume(effects.produces.after_raw_heap_access,
                        effects.consumes.after_raw_heap_access);
  os << "\u2003";  // em space
  os << produce_consume(effects.produces.control_flow,
                        effects.consumes.control_flow);
  os << "\u2003";  // em space
  os << (effects.can_create_identity ? "i" : "_");
  os << " " << (effects.can_allocate ? "a" : "_");
  return os;
}

void SwitchOp::PrintOptions(std::ostream& os) const {
  os << "[";
  for (const Case& c : cases) {
    os << "case " << c.value << ": " << c.destination << ", ";
  }
  os << " default: " << default_case << "]";
}

std::ostream& operator<<(std::ostream& os, ObjectIsOp::Kind kind) {
  switch (kind) {
    case ObjectIsOp::Kind::kArrayBufferView:
      return os << "ArrayBufferView";
    case ObjectIsOp::Kind::kBigInt:
      return os << "BigInt";
    case ObjectIsOp::Kind::kBigInt64:
      return os << "BigInt64";
    case ObjectIsOp::Kind::kCallable:
      return os << "Callable";
    case ObjectIsOp::Kind::kConstructor:
      return os << "Constructor";
    case ObjectIsOp::Kind::kDetectableCallable:
      return os << "DetectableCallable";
    case ObjectIsOp::Kind::kInternalizedString:
      return os << "InternalizedString";
    case ObjectIsOp::Kind::kNonCallable:
      return os << "NonCallable";
    case ObjectIsOp::Kind::kNumber:
      return os << "Number";
    case ObjectIsOp::Kind::kReceiver:
      return os << "Receiver";
    case ObjectIsOp::Kind::kReceiverOrNullOrUndefined:
      return os << "ReceiverOrNullOrUndefined";
    case ObjectIsOp::Kind::kSmi:
      return os << "Smi";
    case ObjectIsOp::Kind::kString:
      return os << "String";
    case ObjectIsOp::Kind::kSymbol:
      return os << "Symbol";
    case ObjectIsOp::Kind::kUndetectable:
      return os << "Undetectable";
  }
}

std::ostream& operator<<(std::ostream& os,
                         ObjectIsOp::InputAssumptions input_assumptions) {
  switch (input_assumptions) {
    case ObjectIsOp::InputAssumptions::kNone:
      return os << "None";
    case ObjectIsOp::InputAssumptions::kHeapObject:
      return os << "HeapObject";
    case ObjectIsOp::InputAssumptions::kBigInt:
      return os << "BigInt";
  }
}

std::ostream& operator<<(std::ostream& os, NumericKind kind) {
  switch (kind) {
    case NumericKind::kFloat64Hole:
      return os << "Float64Hole";
    case NumericKind::kFinite:
      return os << "Finite";
    case NumericKind::kInteger:
      return os << "Integer";
    case NumericKind::kSafeInteger:
      return os << "SafeInteger";
    case NumericKind::kMinusZero:
      return os << "MinusZero";
    case NumericKind::kNaN:
      return os << "NaN";
  }
}

std::ostream& operator<<(std::ostream& os, ConvertOp::Kind kind) {
  switch (kind) {
    case ConvertOp::Kind::kObject:
      return os << "Object";
    case ConvertOp::Kind::kBoolean:
      return os << "Boolean";
    case ConvertOp::Kind::kNumber:
      return os << "Number";
    case ConvertOp::Kind::kNumberOrOddball:
      return os << "NumberOrOddball";
    case ConvertOp::Kind::kPlainPrimitive:
      return os << "PlainPrimitive";
    case ConvertOp::Kind::kString:
      return os << "String";
    case ConvertOp::Kind::kSmi:
      return os << "Smi";
  }
}

std::ostream& operator<<(std::ostream& os,
                         ConvertUntaggedToJSPrimitiveOp::JSPrimitiveKind kind) {
  switch (kind) {
    case ConvertUntaggedToJSPrimitiveOp::JSPrimitiveKind::kBigInt:
      return os << "BigInt";
    case ConvertUntaggedToJSPrimitiveOp::JSPrimitiveKind::kBoolean:
      return os << "Boolean";
    case ConvertUntaggedToJSPrimitiveOp::JSPrimitiveKind::kHeapNumber:
      return os << "HeapNumber";
    case ConvertUntaggedToJSPrimitiveOp::JSPrimitiveKind::kNumber:
      return os << "Number";
    case ConvertUntaggedToJSPrimitiveOp::JSPrimitiveKind::kSmi:
      return os << "Smi";
    case ConvertUntaggedToJSPrimitiveOp::JSPrimitiveKind::kString:
      return os << "String";
  }
}

std::ostream& operator<<(
    std::ostream& os,
    ConvertUntaggedToJSPrimitiveOp::InputInterpretation input_interpretation) {
  switch (input_interpretation) {
    case ConvertUntaggedToJSPrimitiveOp::InputInterpretation::kSigned:
      return os << "Signed";
    case ConvertUntaggedToJSPrimitiveOp::InputInterpretation::kUnsigned:
      return os << "Unsigned";
    case ConvertUntaggedToJSPrimitiveOp::InputInterpretation::kCharCode:
      return os << "CharCode";
    case ConvertUntaggedToJSPrimitiveOp::InputInterpretation::kCodePoint:
      return os << "CodePoint";
  }
}

std::ostream& operator<<(
    std::ostream& os,
    ConvertUntaggedToJSPrimitiveOrDeoptOp::JSPrimitiveKind kind) {
  switch (kind) {
    case ConvertUntaggedToJSPrimitiveOrDeoptOp::JSPrimitiveKind::kSmi:
      return os << "Smi";
  }
}

std::ostream& operator<<(
    std::ostream& os, ConvertUntaggedToJSPrimitiveOrDeoptOp::InputInterpretation
                          input_interpretation) {
  switch (input_interpretation) {
    case ConvertUntaggedToJSPrimitiveOrDeoptOp::InputInterpretation::kSigned:
      return os << "Signed";
    case ConvertUntaggedToJSPrimitiveOrDeoptOp::InputInterpretation::kUnsigned:
      return os << "Unsigned";
  }
}

std::ostream& operator<<(std::ostream& os,
                         ConvertJSPrimitiveToUntaggedOp::UntaggedKind kind) {
  switch (kind) {
    case ConvertJSPrimitiveToUntaggedOp::UntaggedKind::kInt32:
      return os << "Int32";
    case ConvertJSPrimitiveToUntaggedOp::UntaggedKind::kInt64:
      return os << "Int64";
    case ConvertJSPrimitiveToUntaggedOp::UntaggedKind::kUint32:
      return os << "Uint32";
    case ConvertJSPrimitiveToUntaggedOp::UntaggedKind::kBit:
      return os << "Bit";
    case ConvertJSPrimitiveToUntaggedOp::UntaggedKind::kFloat64:
      return os << "Float64";
  }
}

std::ostream& operator<<(
    std::ostream& os,
    ConvertJSPrimitiveToUntaggedOp::InputAssumptions input_assumptions) {
  switch (input_assumptions) {
    case ConvertJSPrimitiveToUntaggedOp::InputAssumptions::kBoolean:
      return os << "Boolean";
    case ConvertJSPrimitiveToUntaggedOp::InputAssumptions::kSmi:
      return os << "Smi";
    case ConvertJSPrimitiveToUntaggedOp::InputAssumptions::kNumberOrOddball:
      return os << "NumberOrOddball";
    case ConvertJSPrimitiveToUntaggedOp::InputAssumptions::kPlainPrimitive:
      return os << "PlainPrimitive";
  }
}

std::ostream& operator<<(
    std::ostream& os,
    ConvertJSPrimitiveToUntaggedOrDeoptOp::UntaggedKind kind) {
  switch (kind) {
    case ConvertJSPrimitiveToUntaggedOrDeoptOp::UntaggedKind::kInt32:
      return os << "Int32";
    case ConvertJSPrimitiveToUntaggedOrDeoptOp::UntaggedKind::kInt64:
      return os << "Int64";
    case ConvertJSPrimitiveToUntaggedOrDeoptOp::UntaggedKind::kFloat64:
      return os << "Float64";
    case ConvertJSPrimitiveToUntaggedOrDeoptOp::UntaggedKind::kArrayIndex:
      return os << "ArrayIndex";
  }
}

std::ostream& operator<<(
    std::ostream& os,
    ConvertJSPrimitiveToUntaggedOrDeoptOp::JSPrimitiveKind kind) {
  switch (kind) {
    case ConvertJSPrimitiveToUntaggedOrDeoptOp::JSPrimitiveKind::kNumber:
      return os << "Number";
    case ConvertJSPrimitiveToUntaggedOrDeoptOp::JSPrimitiveKind::
        kNumberOrBoolean:
      return os << "NumberOrBoolean";
    case ConvertJSPrimitiveToUntaggedOrDeoptOp::JSPrimitiveKind::
        kNumberOrOddball:
      return os << "NumberOrOddball";
    case ConvertJSPrimitiveToUntaggedOrDeoptOp::JSPrimitiveKind::
        kNumberOrString:
      return os << "NumberOrString";
    case ConvertJSPrimitiveToUntaggedOrDeoptOp::JSPrimitiveKind::kSmi:
      return os << "Smi";
  }
}

std::ostream& operator<<(std::ostream& os,
                         TruncateJSPrimitiveToUntaggedOp::UntaggedKind kind) {
  switch (kind) {
    case TruncateJSPrimitiveToUntaggedOp::UntaggedKind::kInt32:
      return os << "Int32";
    case TruncateJSPrimitiveToUntaggedOp::UntaggedKind::kInt64:
      return os << "Int64";
    case TruncateJSPrimitiveToUntaggedOp::UntaggedKind::kBit:
      return os << "Bit";
  }
}

std::ostream& operator<<(
    std::ostream& os,
    TruncateJSPrimitiveToUntaggedOp::InputAssumptions input_assumptions) {
  switch (input_assumptions) {
    case TruncateJSPrimitiveToUntaggedOp::InputAssumptions::kBigInt:
      return os << "BigInt";
    case TruncateJSPrimitiveToUntaggedOp::InputAssumptions::kNumberOrOddball:
      return os << "NumberOrOddball";
    case TruncateJSPrimitiveToUntaggedOp::InputAssumptions::kHeapObject:
      return os << "HeapObject";
    case TruncateJSPrimitiveToUntaggedOp::InputAssumptions::kObject:
      return os << "Object";
  }
}

std::ostream& operator<<(
    std::ostream& os,
    TruncateJSPrimitiveToUntaggedOrDeoptOp::UntaggedKind kind) {
  switch (kind) {
    case TruncateJSPrimitiveToUntaggedOrDeoptOp::UntaggedKind::kInt32:
      return os << "Int32";
  }
}

std::ostream& operator<<(std::ostream& os, NewArrayOp::Kind kind) {
  switch (kind) {
    case NewArrayOp::Kind::kDouble:
      return os << "Double";
    case NewArrayOp::Kind::kObject:
      return os << "Object";
  }
}

std::ostream& operator<<(std::ostream& os, DoubleArrayMinMaxOp::Kind kind) {
  switch (kind) {
    case DoubleArrayMinMaxOp::Kind::kMin:
      return os << "Min";
    case DoubleArrayMinMaxOp::Kind::kMax:
      return os << "Max";
  }
}

std::ostream& operator<<(std::ostream& os, BigIntBinopOp::Kind kind) {
  switch (kind) {
    case BigIntBinopOp::Kind::kAdd:
      return os << "Add";
    case BigIntBinopOp::Kind::kSub:
      return os << "Sub";
    case BigIntBinopOp::Kind::kMul:
      return os << "Mul";
    case BigIntBinopOp::Kind::kDiv:
      return os << "Div";
    case BigIntBinopOp::Kind::kMod:
      return os << "Mod";
    case BigIntBinopOp::Kind::kBitwiseAnd:
      return os << "BitwiseAnd";
    case BigIntBinopOp::Kind::kBitwiseOr:
      return os << "BitwiseOr";
    case BigIntBinopOp::Kind::kBitwiseXor:
      return os << "BitwiseXor";
    case BigIntBinopOp::Kind::kShiftLeft:
      return os << "ShiftLeft";
    case BigIntBinopOp::Kind::kShiftRightArithmetic:
      return os << "ShiftRightArithmetic";
  }
}

std::ostream& operator<<(std::ostream& os, BigIntComparisonOp::Kind kind) {
  switch (kind) {
    case BigIntComparisonOp::Kind::kLessThan:
      return os << "LessThan";
    case BigIntComparisonOp::Kind::kLessThanOrEqual:
      return os << "LessThanOrEqual";
  }
}

std::ostream& operator<<(std::ostream& os, BigIntUnaryOp::Kind kind) {
  switch (kind) {
    case BigIntUnaryOp::Kind::kNegate:
      return os << "Negate";
  }
}

std::ostream& operator<<(std::ostream& os, StringAtOp::Kind kind) {
  switch (kind) {
    case StringAtOp::Kind::kCharCode:
      return os << "CharCode";
    case StringAtOp::Kind::kCodePoint:
      return os << "CodePoint";
  }
}

#ifdef V8_INTL_SUPPORT
std::ostream& operator<<(std::ostream& os, StringToCaseIntlOp::Kind kind) {
  switch (kind) {
    case StringToCaseIntlOp::Kind::kLower:
      return os << "Lower";
    case StringToCaseIntlOp::Kind::kUpper:
      return os << "Upper";
  }
}
#endif  // V8_INTL_SUPPORT

std::ostream& operator<<(std::ostream& os, StringComparisonOp::Kind kind) {
  switch (kind) {
    case StringComparisonOp::Kind::kLessThan:
      return os << "LessThan";
    case StringComparisonOp::Kind::kLessThanOrEqual:
      return os << "LessThanOrEqual";
  }
}

std::ostream& operator<<(std::ostream& os, ArgumentsLengthOp::Kind kind) {
  switch (kind) {
    case ArgumentsLengthOp::Kind::kArguments:
      return os << "Arguments";
    case ArgumentsLengthOp::Kind::kRest:
      return os << "Rest";
  }
}

std::ostream& operator<<(std::ostream& os,
                         TransitionAndStoreArrayElementOp::Kind kind) {
  switch (kind) {
    case TransitionAndStoreArrayElementOp::Kind::kElement:
      return os << "Element";
    case TransitionAndStoreArrayElementOp::Kind::kNumberElement:
      return os << "NumberElement";
    case TransitionAndStoreArrayElementOp::Kind::kOddballElement:
      return os << "OddballElement";
    case TransitionAndStoreArrayElementOp::Kind::kNonNumberElement:
      return os << "NonNumberElement";
    case TransitionAndStoreArrayElementOp::Kind::kSignedSmallElement:
      return os << "SignedSmallElement";
  }
}

std::ostream& operator<<(std::ostream& os, SameValueOp::Mode mode) {
  switch (mode) {
    case SameValueOp::Mode::kSameValue:
      return os << "SameValue";
    case SameValueOp::Mode::kSameValueNumbersOnly:
      return os << "SameValueNumbersOnly";
  }
}

std::ostream& operator<<(std::ostream& os, FindOrderedHashEntryOp::Kind kind) {
  switch (kind) {
    case FindOrderedHashEntryOp::Kind::kFindOrderedHashMapEntry:
      return os << "FindOrderedHashMapEntry";
    case FindOrderedHashEntryOp::Kind::kFindOrderedHashMapEntryForInt32Key:
      return os << "FindOrderedHashMapEntryForInt32Key";
    case FindOrderedHashEntryOp::Kind::kFindOrderedHashSetEntry:
      return os << "FindOrderedHashSetEntry";
  }
}

#if V8_ENABLE_WEBASSEMBLY

const RegisterRepresentation& RepresentationFor(wasm::ValueType type) {
  static const RegisterRepresentation kWord32 =
      RegisterRepresentation::Word32();
  static const RegisterRepresentation kWord64 =
      RegisterRepresentation::Word64();
  static const RegisterRepresentation kFloat32 =
      RegisterRepresentation::Float32();
  static const RegisterRepresentation kFloat64 =
      RegisterRepresentation::Float64();
  static const RegisterRepresentation kTagged =
      RegisterRepresentation::Tagged();
  static const RegisterRepresentation kSimd128 =
      RegisterRepresentation::Simd128();

  switch (type.kind()) {
    case wasm::kI8:
    case wasm::kI16:
    case wasm::kI32:
      return kWord32;
    case wasm::kI64:
      return kWord64;
    case wasm::kF32:
      return kFloat32;
    case wasm::kF64:
      return kFloat64;
    case wasm::kRefNull:
    case wasm::kRef:
      return kTagged;
    case wasm::kS128:
      return kSimd128;
    case wasm::kVoid:
    case wasm::kRtt:
    case wasm::kBottom:
      UNREACHABLE();
  }
}

std::ostream& operator<<(std::ostream& os, Simd128BinopOp::Kind kind) {
  switch (kind) {
#define PRINT_KIND(kind)              \
  case Simd128BinopOp::Kind::k##kind: \
    return os << #kind;
    FOREACH_SIMD_128_BINARY_OPCODE(PRINT_KIND)
  }
#undef PRINT_KIND
}

std::ostream& operator<<(std::ostream& os, Simd128UnaryOp::Kind kind) {
  switch (kind) {
#define PRINT_KIND(kind)              \
  case Simd128UnaryOp::Kind::k##kind: \
    return os << #kind;
    FOREACH_SIMD_128_UNARY_OPCODE(PRINT_KIND)
  }
#undef PRINT_KIND
}

#endif  // V8_ENABLE_WEBASSEBMLY

std::string Operation::ToString() const {
  std::stringstream ss;
  ss << *this;
  return ss.str();
}

base::LazyMutex SupportedOperations::mutex_ = LAZY_MUTEX_INITIALIZER;
SupportedOperations SupportedOperations::instance_;
bool SupportedOperations::initialized_;

void SupportedOperations::Initialize() {
  base::MutexGuard lock(mutex_.Pointer());
  if (initialized_) return;
  initialized_ = true;

  MachineOperatorBuilder::Flags supported =
      InstructionSelector::SupportedMachineOperatorFlags();
#define SET_SUPPORTED(name, machine_name) \
  instance_.name##_ = supported & MachineOperatorBuilder::Flag::k##machine_name;

  SUPPORTED_OPERATIONS_LIST(SET_SUPPORTED)
#undef SET_SUPPORTED
}

base::SmallVector<Block*, 4> SuccessorBlocks(const Block& block,
                                             const Graph& graph) {
  return SuccessorBlocks(block.LastOperation(graph));
}

// static
bool SupportedOperations::IsUnalignedLoadSupported(MemoryRepresentation repr) {
  return InstructionSelector::AlignmentRequirements().IsUnalignedLoadSupported(
      repr.ToMachineType().representation());
}

// static
bool SupportedOperations::IsUnalignedStoreSupported(MemoryRepresentation repr) {
  return InstructionSelector::AlignmentRequirements().IsUnalignedStoreSupported(
      repr.ToMachineType().representation());
}

void CheckExceptionOp::Validate(const Graph& graph) const {
  DCHECK_NE(didnt_throw_block, catch_block);
  // `CheckException` should follow right after the throwing operation.
  DCHECK_EQ(throwing_operation(), graph.PreviousIndex(graph.Index(*this)));
}

namespace {
// Ensures basic consistency of representation mapping.
class InputsRepFactoryCheck : InputsRepFactory {
  static_assert(*ToMaybeRepPointer(RegisterRepresentation::Word32()) ==
                MaybeRegisterRepresentation::Word32());
  static_assert(*ToMaybeRepPointer(RegisterRepresentation::Word64()) ==
                MaybeRegisterRepresentation::Word64());
  static_assert(*ToMaybeRepPointer(RegisterRepresentation::Float32()) ==
                MaybeRegisterRepresentation::Float32());
  static_assert(*ToMaybeRepPointer(RegisterRepresentation::Float64()) ==
                MaybeRegisterRepresentation::Float64());
  static_assert(*ToMaybeRepPointer(RegisterRepresentation::Tagged()) ==
                MaybeRegisterRepresentation::Tagged());
  static_assert(*ToMaybeRepPointer(RegisterRepresentation::Compressed()) ==
                MaybeRegisterRepresentation::Compressed());
  static_assert(*ToMaybeRepPointer(RegisterRepresentation::Simd128()) ==
                MaybeRegisterRepresentation::Simd128());
};
}  // namespace

}  // namespace v8::internal::compiler::turboshaft
