// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/branch-elimination.h"

#include "src/compiler/js-graph.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/simplified-operator.h"

namespace v8 {
namespace internal {
namespace compiler {

BranchElimination::BranchElimination(Editor* editor, JSGraph* js_graph,
                                     Zone* zone)
    : AdvancedReducer(editor),
      jsgraph_(js_graph),
      node_conditions_(js_graph->graph()->NodeCount(), zone),
      reduced_(js_graph->graph()->NodeCount(), zone),
      zone_(zone),
      dead_(js_graph->Dead()) {}

BranchElimination::~BranchElimination() {}


Reduction BranchElimination::Reduce(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kDead:
      return NoChange();
    case IrOpcode::kDeoptimizeIf:
    case IrOpcode::kDeoptimizeUnless:
      return ReduceDeoptimizeConditional(node);
    case IrOpcode::kMerge:
      return ReduceMerge(node);
    case IrOpcode::kLoop:
      return ReduceLoop(node);
    case IrOpcode::kBranch:
      return ReduceBranch(node);
    case IrOpcode::kIfFalse:
      return ReduceIf(node, false);
    case IrOpcode::kIfTrue:
      return ReduceIf(node, true);
    case IrOpcode::kStart:
      return ReduceStart(node);
    default:
      if (node->op()->ControlOutputCount() > 0) {
        return ReduceOtherControl(node);
      }
      break;
  }
  return NoChange();
}


Reduction BranchElimination::ReduceBranch(Node* node) {
  Node* condition = node->InputAt(0);
  Node* control_input = NodeProperties::GetControlInput(node, 0);
  ControlPathConditions from_input = node_conditions_.Get(control_input);
  Maybe<bool> condition_value = from_input.LookupCondition(condition);
  // If we know the condition we can discard the branch.
  if (condition_value.IsJust()) {
    bool known_value = condition_value.FromJust();
    for (Node* const use : node->uses()) {
      switch (use->opcode()) {
        case IrOpcode::kIfTrue:
          Replace(use, known_value ? control_input : dead());
          break;
        case IrOpcode::kIfFalse:
          Replace(use, known_value ? dead() : control_input);
          break;
        default:
          UNREACHABLE();
      }
    }
    return Replace(dead());
  }
  return TakeConditionsFromFirstControl(node);
}

Reduction BranchElimination::ReduceDeoptimizeConditional(Node* node) {
  DCHECK(node->opcode() == IrOpcode::kDeoptimizeIf ||
         node->opcode() == IrOpcode::kDeoptimizeUnless);
  bool condition_is_true = node->opcode() == IrOpcode::kDeoptimizeUnless;
  DeoptimizeParameters p = DeoptimizeParametersOf(node->op());
  Node* condition = NodeProperties::GetValueInput(node, 0);
  Node* frame_state = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  // If we do not know anything about the predecessor, do not propagate just
  // yet because we will have to recompute anyway once we compute the
  // predecessor.
  if (!reduced_.Get(control)) {
    return NoChange();
  }

  ControlPathConditions conditions = node_conditions_.Get(control);
  Maybe<bool> condition_value = conditions.LookupCondition(condition);
  if (condition_value.IsJust()) {
    // If we know the condition we can discard the branch.
    if (condition_is_true == condition_value.FromJust()) {
      // We don't update the conditions here, because we're replacing {node}
      // with the {control} node that already contains the right information.
      ReplaceWithValue(node, dead(), effect, control);
    } else {
      control = graph()->NewNode(
          common()->Deoptimize(p.kind(), p.reason(), p.feedback()), frame_state,
          effect, control);
      // TODO(bmeurer): This should be on the AdvancedReducer somehow.
      NodeProperties::MergeControlToEnd(graph(), common(), control);
      Revisit(graph()->end());
    }
    return Replace(dead());
  }
  return UpdateConditions(node, conditions, condition, condition_is_true);
}

Reduction BranchElimination::ReduceIf(Node* node, bool is_true_branch) {
  // Add the condition to the list arriving from the input branch.
  Node* branch = NodeProperties::GetControlInput(node, 0);
  ControlPathConditions from_branch = node_conditions_.Get(branch);
  // If we do not know anything about the predecessor, do not propagate just
  // yet because we will have to recompute anyway once we compute the
  // predecessor.
  if (!reduced_.Get(branch)) {
    return NoChange();
  }
  Node* condition = branch->InputAt(0);
  return UpdateConditions(node, from_branch, condition, is_true_branch);
}


Reduction BranchElimination::ReduceLoop(Node* node) {
  // Here we rely on having only reducible loops:
  // The loop entry edge always dominates the header, so we can just use
  // the information from the loop entry edge.
  return TakeConditionsFromFirstControl(node);
}


Reduction BranchElimination::ReduceMerge(Node* node) {
  // Shortcut for the case when we do not know anything about some
  // input.
  Node::Inputs inputs = node->inputs();
  for (Node* input : inputs) {
    if (!reduced_.Get(input)) {
      return NoChange();
    }
  }

  auto input_it = inputs.begin();

  DCHECK_GT(inputs.count(), 0);

  ControlPathConditions conditions = node_conditions_.Get(*input_it);
  ++input_it;
  // Merge the first input's conditions with the conditions from the other
  // inputs.
  auto input_end = inputs.end();
  for (; input_it != input_end; ++input_it) {
    // Change the current condition list to a longest common tail
    // of this condition list and the other list. (The common tail
    // should correspond to the list from the common dominator.)
    conditions.ResetToCommonAncestor(node_conditions_.Get(*input_it));
  }
  return UpdateConditions(node, conditions);
}


Reduction BranchElimination::ReduceStart(Node* node) {
  return UpdateConditions(node, {});
}


Reduction BranchElimination::ReduceOtherControl(Node* node) {
  DCHECK_EQ(1, node->op()->ControlInputCount());
  return TakeConditionsFromFirstControl(node);
}


Reduction BranchElimination::TakeConditionsFromFirstControl(Node* node) {
  // We just propagate the information from the control input (ideally,
  // we would only revisit control uses if there is change).
  Node* input = NodeProperties::GetControlInput(node, 0);
  if (!reduced_.Get(input)) return NoChange();
  return UpdateConditions(node, node_conditions_.Get(input));
}

Reduction BranchElimination::UpdateConditions(
    Node* node, ControlPathConditions conditions) {
  // Only signal that the node has Changed if the condition information has
  // changed.
  if (reduced_.Set(node, true) | node_conditions_.Set(node, conditions)) {
    return Changed(node);
  }
  return NoChange();
}

Reduction BranchElimination::UpdateConditions(
    Node* node, ControlPathConditions prev_conditions, Node* current_condition,
    bool is_true_branch) {
  ControlPathConditions original = node_conditions_.Get(node);
  // The control path for the node is the path obtained by appending the
  // current_condition to the prev_conditions. Use the original control path as
  // a hint to avoid allocations.
  prev_conditions.AddCondition(zone_, current_condition, is_true_branch,
                               original);
  return UpdateConditions(node, prev_conditions);
}

void BranchElimination::ControlPathConditions::AddCondition(
    Zone* zone, Node* condition, bool is_true, ControlPathConditions hint) {
  DCHECK(LookupCondition(condition).IsNothing());
  PushFront({condition, is_true}, zone, hint);
}


Maybe<bool> BranchElimination::ControlPathConditions::LookupCondition(
    Node* condition) const {
  for (BranchCondition element : *this) {
    if (element.condition == condition) return Just<bool>(element.is_true);
  }
  return Nothing<bool>();
}

Graph* BranchElimination::graph() const { return jsgraph()->graph(); }

CommonOperatorBuilder* BranchElimination::common() const {
  return jsgraph()->common();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
