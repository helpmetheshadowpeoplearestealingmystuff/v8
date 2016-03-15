// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/int64-lowering.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/compiler/linkage.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node-properties.h"

#include "src/compiler/node.h"
#include "src/wasm/wasm-module.h"
#include "src/zone.h"

namespace v8 {
namespace internal {
namespace compiler {

Int64Lowering::Int64Lowering(Graph* graph, MachineOperatorBuilder* machine,
                             CommonOperatorBuilder* common, Zone* zone,
                             Signature<MachineRepresentation>* signature)
    : zone_(zone),
      graph_(graph),
      machine_(machine),
      common_(common),
      state_(graph, 3),
      stack_(zone),
      replacements_(zone->NewArray<Replacement>(graph->NodeCount())),
      signature_(signature) {
  memset(replacements_, 0, sizeof(Replacement) * graph->NodeCount());
}

void Int64Lowering::LowerGraph() {
  if (!machine()->Is32()) {
    return;
  }
  stack_.push({graph()->end(), 0});
  state_.Set(graph()->end(), State::kOnStack);

  while (!stack_.empty()) {
    NodeState& top = stack_.top();
    if (top.input_index == top.node->InputCount()) {
      // All inputs of top have already been lowered, now lower top.
      stack_.pop();
      state_.Set(top.node, State::kVisited);
      LowerNode(top.node);
    } else {
      // Push the next input onto the stack.
      Node* input = top.node->InputAt(top.input_index++);
      if (state_.Get(input) == State::kUnvisited) {
        stack_.push({input, 0});
        state_.Set(input, State::kOnStack);
      }
    }
  }
}

static int GetParameterIndexAfterLowering(
    Signature<MachineRepresentation>* signature, int old_index) {
  int result = old_index;
  for (int i = 0; i < old_index; i++) {
    if (signature->GetParam(i) == MachineRepresentation::kWord64) {
      result++;
    }
  }
  return result;
}

static int GetParameterCountAfterLowering(
    Signature<MachineRepresentation>* signature) {
  return GetParameterIndexAfterLowering(
      signature, static_cast<int>(signature->parameter_count()));
}

static int GetReturnCountAfterLowering(
    Signature<MachineRepresentation>* signature) {
  int result = static_cast<int>(signature->return_count());
  for (int i = 0; i < static_cast<int>(signature->return_count()); i++) {
    if (signature->GetReturn(i) == MachineRepresentation::kWord64) {
      result++;
    }
  }
  return result;
}

void Int64Lowering::LowerNode(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kInt64Constant: {
      int64_t value = OpParameter<int64_t>(node);
      Node* low_node = graph()->NewNode(
          common()->Int32Constant(static_cast<int32_t>(value & 0xFFFFFFFF)));
      Node* high_node = graph()->NewNode(
          common()->Int32Constant(static_cast<int32_t>(value >> 32)));
      ReplaceNode(node, low_node, high_node);
      break;
    }
    case IrOpcode::kLoad: {
      LoadRepresentation load_rep = LoadRepresentationOf(node->op());

      if (load_rep.representation() == MachineRepresentation::kWord64) {
        Node* base = node->InputAt(0);
        Node* index = node->InputAt(1);
        Node* index_high =
            graph()->NewNode(machine()->Int32Add(), index,
                             graph()->NewNode(common()->Int32Constant(4)));

        const Operator* load_op = machine()->Load(MachineType::Int32());
        Node* high_node;
        if (node->InputCount() > 2) {
          Node* effect_high = node->InputAt(2);
          Node* control_high = node->InputAt(3);
          high_node = graph()->NewNode(load_op, base, index_high, effect_high,
                                       control_high);
          // change the effect change from old_node --> old_effect to
          // old_node --> high_node --> old_effect.
          node->ReplaceInput(2, high_node);
        } else {
          high_node = graph()->NewNode(load_op, base, index_high);
        }
        NodeProperties::ChangeOp(node, load_op);
        ReplaceNode(node, node, high_node);
      } else {
        DefaultLowering(node);
      }
      break;
    }
    case IrOpcode::kStore: {
      StoreRepresentation store_rep = StoreRepresentationOf(node->op());
      if (store_rep.representation() == MachineRepresentation::kWord64) {
        // We change the original store node to store the low word, and create
        // a new store node to store the high word. The effect and control edges
        // are copied from the original store to the new store node, the effect
        // edge of the original store is redirected to the new store.
        WriteBarrierKind write_barrier_kind = store_rep.write_barrier_kind();

        Node* base = node->InputAt(0);
        Node* index = node->InputAt(1);
        Node* index_high =
            graph()->NewNode(machine()->Int32Add(), index,
                             graph()->NewNode(common()->Int32Constant(4)));

        Node* value = node->InputAt(2);
        DCHECK(HasReplacementLow(value));
        DCHECK(HasReplacementHigh(value));

        const Operator* store_op = machine()->Store(StoreRepresentation(
            MachineRepresentation::kWord32, write_barrier_kind));

        Node* high_node;
        if (node->InputCount() > 3) {
          Node* effect_high = node->InputAt(3);
          Node* control_high = node->InputAt(4);
          high_node = graph()->NewNode(store_op, base, index_high,
                                       GetReplacementHigh(value), effect_high,
                                       control_high);
          node->ReplaceInput(3, high_node);

        } else {
          high_node = graph()->NewNode(store_op, base, index_high,
                                       GetReplacementHigh(value));
        }

        node->ReplaceInput(2, GetReplacementLow(value));
        NodeProperties::ChangeOp(node, store_op);
        ReplaceNode(node, node, high_node);
      } else {
        DefaultLowering(node);
      }
      break;
    }
    case IrOpcode::kStart: {
      int parameter_count = GetParameterCountAfterLowering(signature());
      // Only exchange the node if the parameter count actually changed.
      if (parameter_count != signature()->parameter_count()) {
        int delta =
            parameter_count - static_cast<int>(signature()->parameter_count());
        int new_output_count = node->op()->ValueOutputCount() + delta;
        NodeProperties::ChangeOp(node, common()->Start(new_output_count));
      }
      break;
    }
    case IrOpcode::kParameter: {
      DCHECK(node->InputCount() == 1);
      // Only exchange the node if the parameter count actually changed. We do
      // not even have to do the default lowering because the the start node,
      // the only input of a parameter node, only changes if the parameter count
      // changes.
      if (GetParameterCountAfterLowering(signature()) !=
          signature()->parameter_count()) {
        int old_index = ParameterIndexOf(node->op());
        int new_index = GetParameterIndexAfterLowering(signature(), old_index);
        NodeProperties::ChangeOp(node, common()->Parameter(new_index));

        Node* high_node = nullptr;
        if (signature()->GetParam(old_index) ==
            MachineRepresentation::kWord64) {
          high_node = graph()->NewNode(common()->Parameter(new_index + 1),
                                       graph()->start());
        }
        ReplaceNode(node, node, high_node);
      }
      break;
    }
    case IrOpcode::kReturn: {
      DefaultLowering(node);
      int new_return_count = GetReturnCountAfterLowering(signature());
      if (signature()->return_count() != new_return_count) {
        NodeProperties::ChangeOp(node, common()->Return(new_return_count));
      }
      break;
    }
    case IrOpcode::kCall: {
      CallDescriptor* descriptor = OpParameter<CallDescriptor*>(node);
      if (DefaultLowering(node) ||
          (descriptor->ReturnCount() == 1 &&
           descriptor->GetReturnType(0) == MachineType::Int64())) {
        // We have to adjust the call descriptor.
        const Operator* op = common()->Call(
            wasm::ModuleEnv::GetI32WasmCallDescriptor(zone(), descriptor));
        NodeProperties::ChangeOp(node, op);
      }
      if (descriptor->ReturnCount() == 1 &&
          descriptor->GetReturnType(0) == MachineType::Int64()) {
        // We access the additional return values through projections.
        Node* low_node = graph()->NewNode(common()->Projection(0), node);
        Node* high_node = graph()->NewNode(common()->Projection(1), node);
        ReplaceNode(node, low_node, high_node);
      }
      break;
    }
    case IrOpcode::kWord64And: {
      DCHECK(node->InputCount() == 2);
      Node* left = node->InputAt(0);
      Node* right = node->InputAt(1);

      Node* low_node =
          graph()->NewNode(machine()->Word32And(), GetReplacementLow(left),
                           GetReplacementLow(right));
      Node* high_node =
          graph()->NewNode(machine()->Word32And(), GetReplacementHigh(left),
                           GetReplacementHigh(right));
      ReplaceNode(node, low_node, high_node);
      break;
    }
    case IrOpcode::kTruncateInt64ToInt32: {
      DCHECK(node->InputCount() == 1);
      Node* input = node->InputAt(0);
      ReplaceNode(node, GetReplacementLow(input), nullptr);
      node->NullAllInputs();
      break;
    }
    // todo(ahaas): I added a list of missing instructions here to make merging
    // easier when I do them one by one.
    // kExprI64Add:
    case IrOpcode::kInt64Add: {
      DCHECK(node->InputCount() == 2);

      Node* right = node->InputAt(1);
      node->ReplaceInput(1, GetReplacementLow(right));
      node->AppendInput(zone(), GetReplacementHigh(right));

      Node* left = node->InputAt(0);
      node->ReplaceInput(0, GetReplacementLow(left));
      node->InsertInput(zone(), 1, GetReplacementHigh(left));

      NodeProperties::ChangeOp(node, machine()->Int32PairAdd());
      // We access the additional return values through projections.
      Node* low_node = graph()->NewNode(common()->Projection(0), node);
      Node* high_node = graph()->NewNode(common()->Projection(1), node);
      ReplaceNode(node, low_node, high_node);
      break;
    }

    // kExprI64Sub:
    // kExprI64Mul:
    // kExprI64DivS:
    // kExprI64DivU:
    // kExprI64RemS:
    // kExprI64RemU:
    // kExprI64Ior:
    case IrOpcode::kWord64Or: {
      DCHECK(node->InputCount() == 2);
      Node* left = node->InputAt(0);
      Node* right = node->InputAt(1);

      Node* low_node =
          graph()->NewNode(machine()->Word32Or(), GetReplacementLow(left),
                           GetReplacementLow(right));
      Node* high_node =
          graph()->NewNode(machine()->Word32Or(), GetReplacementHigh(left),
                           GetReplacementHigh(right));
      ReplaceNode(node, low_node, high_node);
      break;
    }

    // kExprI64Xor:
    case IrOpcode::kWord64Xor: {
      DCHECK(node->InputCount() == 2);
      Node* left = node->InputAt(0);
      Node* right = node->InputAt(1);

      Node* low_node =
          graph()->NewNode(machine()->Word32Xor(), GetReplacementLow(left),
                           GetReplacementLow(right));
      Node* high_node =
          graph()->NewNode(machine()->Word32Xor(), GetReplacementHigh(left),
                           GetReplacementHigh(right));
      ReplaceNode(node, low_node, high_node);
      break;
    }
    // kExprI64Shl:
    case IrOpcode::kWord64Shl: {
      // TODO(turbofan): if the shift count >= 32, then we can set the low word
      // of the output to 0 and just calculate the high word.
      DCHECK(node->InputCount() == 2);
      Node* shift = node->InputAt(1);
      if (HasReplacementLow(shift)) {
        // We do not have to care about the high word replacement, because
        // the shift can only be between 0 and 63 anyways.
        node->ReplaceInput(1, GetReplacementLow(shift));
      }

      Node* value = node->InputAt(0);
      node->ReplaceInput(0, GetReplacementLow(value));
      node->InsertInput(zone(), 1, GetReplacementHigh(value));

      NodeProperties::ChangeOp(node, machine()->Word32PairShl());
      // We access the additional return values through projections.
      Node* low_node = graph()->NewNode(common()->Projection(0), node);
      Node* high_node = graph()->NewNode(common()->Projection(1), node);
      ReplaceNode(node, low_node, high_node);
      break;
    }
    // kExprI64ShrU:
    case IrOpcode::kWord64Shr: {
      // TODO(turbofan): if the shift count >= 32, then we can set the low word
      // of the output to 0 and just calculate the high word.
      DCHECK(node->InputCount() == 2);
      Node* shift = node->InputAt(1);
      if (HasReplacementLow(shift)) {
        // We do not have to care about the high word replacement, because
        // the shift can only be between 0 and 63 anyways.
        node->ReplaceInput(1, GetReplacementLow(shift));
      }

      Node* value = node->InputAt(0);
      node->ReplaceInput(0, GetReplacementLow(value));
      node->InsertInput(zone(), 1, GetReplacementHigh(value));

      NodeProperties::ChangeOp(node, machine()->Word32PairShr());
      // We access the additional return values through projections.
      Node* low_node = graph()->NewNode(common()->Projection(0), node);
      Node* high_node = graph()->NewNode(common()->Projection(1), node);
      ReplaceNode(node, low_node, high_node);
      break;
    }
    // kExprI64ShrS:
    case IrOpcode::kWord64Sar: {
      // TODO(turbofan): if the shift count >= 32, then we can set the low word
      // of the output to 0 and just calculate the high word.
      DCHECK(node->InputCount() == 2);
      Node* shift = node->InputAt(1);
      if (HasReplacementLow(shift)) {
        // We do not have to care about the high word replacement, because
        // the shift can only be between 0 and 63 anyways.
        node->ReplaceInput(1, GetReplacementLow(shift));
      }

      Node* value = node->InputAt(0);
      node->ReplaceInput(0, GetReplacementLow(value));
      node->InsertInput(zone(), 1, GetReplacementHigh(value));

      NodeProperties::ChangeOp(node, machine()->Word32PairSar());
      // We access the additional return values through projections.
      Node* low_node = graph()->NewNode(common()->Projection(0), node);
      Node* high_node = graph()->NewNode(common()->Projection(1), node);
      ReplaceNode(node, low_node, high_node);
      break;
    }
    // kExprI64Eq:
    case IrOpcode::kWord64Equal: {
      DCHECK(node->InputCount() == 2);
      Node* left = node->InputAt(0);
      Node* right = node->InputAt(1);

      // TODO(wasm): Use explicit comparisons and && here?
      Node* replacement = graph()->NewNode(
          machine()->Word32Equal(),
          graph()->NewNode(
              machine()->Word32Or(),
              graph()->NewNode(machine()->Word32Xor(), GetReplacementLow(left),
                               GetReplacementLow(right)),
              graph()->NewNode(machine()->Word32Xor(), GetReplacementHigh(left),
                               GetReplacementHigh(right))),
          graph()->NewNode(common()->Int32Constant(0)));

      ReplaceNode(node, replacement, nullptr);
      break;
    }
    // kExprI64LtS:
    case IrOpcode::kInt64LessThan: {
      LowerComparison(node, machine()->Int32LessThan(),
                      machine()->Uint32LessThan());
      break;
    }
    case IrOpcode::kInt64LessThanOrEqual: {
      LowerComparison(node, machine()->Int32LessThan(),
                      machine()->Uint32LessThanOrEqual());
      break;
    }
    case IrOpcode::kUint64LessThan: {
      LowerComparison(node, machine()->Uint32LessThan(),
                      machine()->Uint32LessThan());
      break;
    }
    case IrOpcode::kUint64LessThanOrEqual: {
      LowerComparison(node, machine()->Uint32LessThan(),
                      machine()->Uint32LessThanOrEqual());
      break;
    }

    // kExprI64SConvertI32:
    case IrOpcode::kChangeInt32ToInt64: {
      DCHECK(node->InputCount() == 1);
      Node* input = node->InputAt(0);
      if (HasReplacementLow(input)) {
        input = GetReplacementLow(input);
      }
      // We use SAR to preserve the sign in the high word.
      ReplaceNode(
          node, input,
          graph()->NewNode(machine()->Word32Sar(), input,
                           graph()->NewNode(common()->Int32Constant(31))));
      node->NullAllInputs();
      break;
    }
    // kExprI64UConvertI32: {
    case IrOpcode::kChangeUint32ToUint64: {
      DCHECK(node->InputCount() == 1);
      Node* input = node->InputAt(0);
      if (HasReplacementLow(input)) {
        input = GetReplacementLow(input);
      }
      ReplaceNode(node, input, graph()->NewNode(common()->Int32Constant(0)));
      node->NullAllInputs();
      break;
    }
    // kExprF64ReinterpretI64:
    case IrOpcode::kBitcastInt64ToFloat64: {
      DCHECK(node->InputCount() == 1);
      Node* input = node->InputAt(0);
      Node* stack_slot = graph()->NewNode(
          machine()->StackSlot(MachineRepresentation::kWord64));

      Node* store_high_word = graph()->NewNode(
          machine()->Store(
              StoreRepresentation(MachineRepresentation::kWord32,
                                  WriteBarrierKind::kNoWriteBarrier)),
          stack_slot, graph()->NewNode(common()->Int32Constant(4)),
          GetReplacementHigh(input), graph()->start(), graph()->start());

      Node* store_low_word = graph()->NewNode(
          machine()->Store(
              StoreRepresentation(MachineRepresentation::kWord32,
                                  WriteBarrierKind::kNoWriteBarrier)),
          stack_slot, graph()->NewNode(common()->Int32Constant(0)),
          GetReplacementLow(input), store_high_word, graph()->start());

      Node* load =
          graph()->NewNode(machine()->Load(MachineType::Float64()), stack_slot,
                           graph()->NewNode(common()->Int32Constant(0)),
                           store_low_word, graph()->start());

      ReplaceNode(node, load, nullptr);
      break;
    }
    // kExprI64ReinterpretF64:
    case IrOpcode::kBitcastFloat64ToInt64: {
      DCHECK(node->InputCount() == 1);
      Node* input = node->InputAt(0);
      if (HasReplacementLow(input)) {
        input = GetReplacementLow(input);
      }
      Node* stack_slot = graph()->NewNode(
          machine()->StackSlot(MachineRepresentation::kWord64));
      Node* store = graph()->NewNode(
          machine()->Store(
              StoreRepresentation(MachineRepresentation::kFloat64,
                                  WriteBarrierKind::kNoWriteBarrier)),
          stack_slot, graph()->NewNode(common()->Int32Constant(0)), input,
          graph()->start(), graph()->start());

      Node* high_node =
          graph()->NewNode(machine()->Load(MachineType::Int32()), stack_slot,
                           graph()->NewNode(common()->Int32Constant(4)), store,
                           graph()->start());

      Node* low_node =
          graph()->NewNode(machine()->Load(MachineType::Int32()), stack_slot,
                           graph()->NewNode(common()->Int32Constant(0)), store,
                           graph()->start());
      ReplaceNode(node, low_node, high_node);
      break;
    }
    // kExprI64Clz:
    // kExprI64Ctz:
    case IrOpcode::kWord64Popcnt: {
      DCHECK(node->InputCount() == 1);
      Node* input = node->InputAt(0);
      // We assume that a Word64Popcnt node only has been created if
      // Word32Popcnt is actually supported.
      DCHECK(machine()->Word32Popcnt().IsSupported());
      ReplaceNode(node, graph()->NewNode(
                            machine()->Int32Add(),
                            graph()->NewNode(machine()->Word32Popcnt().op(),
                                             GetReplacementLow(input)),
                            graph()->NewNode(machine()->Word32Popcnt().op(),
                                             GetReplacementHigh(input))),
                  graph()->NewNode(common()->Int32Constant(0)));
      break;
    }
    // kExprI64Popcnt:

    default: { DefaultLowering(node); }
  }
}

void Int64Lowering::LowerComparison(Node* node, const Operator* high_word_op,
                                    const Operator* low_word_op) {
  DCHECK(node->InputCount() == 2);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  Node* replacement = graph()->NewNode(
      machine()->Word32Or(),
      graph()->NewNode(high_word_op, GetReplacementHigh(left),
                       GetReplacementHigh(right)),
      graph()->NewNode(
          machine()->Word32And(),
          graph()->NewNode(machine()->Word32Equal(), GetReplacementHigh(left),
                           GetReplacementHigh(right)),
          graph()->NewNode(low_word_op, GetReplacementLow(left),
                           GetReplacementLow(right))));

  ReplaceNode(node, replacement, nullptr);
}

bool Int64Lowering::DefaultLowering(Node* node) {
  bool something_changed = false;
  for (int i = NodeProperties::PastValueIndex(node) - 1; i >= 0; i--) {
    Node* input = node->InputAt(i);
    if (HasReplacementLow(input)) {
      something_changed = true;
      node->ReplaceInput(i, GetReplacementLow(input));
    }
    if (HasReplacementHigh(input)) {
      something_changed = true;
      node->InsertInput(zone(), i + 1, GetReplacementHigh(input));
    }
  }
  return something_changed;
}

void Int64Lowering::ReplaceNode(Node* old, Node* new_low, Node* new_high) {
  // if new_low == nullptr, then also new_high == nullptr.
  DCHECK(new_low != nullptr || new_high == nullptr);
  replacements_[old->id()].low = new_low;
  replacements_[old->id()].high = new_high;
}

bool Int64Lowering::HasReplacementLow(Node* node) {
  return replacements_[node->id()].low != nullptr;
}

Node* Int64Lowering::GetReplacementLow(Node* node) {
  Node* result = replacements_[node->id()].low;
  DCHECK(result);
  return result;
}

bool Int64Lowering::HasReplacementHigh(Node* node) {
  return replacements_[node->id()].high != nullptr;
}

Node* Int64Lowering::GetReplacementHigh(Node* node) {
  Node* result = replacements_[node->id()].high;
  DCHECK(result);
  return result;
}
}  // namespace compiler
}  // namespace internal
}  // namespace v8
