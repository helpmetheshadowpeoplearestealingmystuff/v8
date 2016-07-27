// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/effect-control-linearizer.h"

#include "src/code-factory.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/node.h"
#include "src/compiler/schedule.h"

namespace v8 {
namespace internal {
namespace compiler {

EffectControlLinearizer::EffectControlLinearizer(JSGraph* js_graph,
                                                 Schedule* schedule,
                                                 Zone* temp_zone)
    : js_graph_(js_graph), schedule_(schedule), temp_zone_(temp_zone) {}

Graph* EffectControlLinearizer::graph() const { return js_graph_->graph(); }
CommonOperatorBuilder* EffectControlLinearizer::common() const {
  return js_graph_->common();
}
SimplifiedOperatorBuilder* EffectControlLinearizer::simplified() const {
  return js_graph_->simplified();
}
MachineOperatorBuilder* EffectControlLinearizer::machine() const {
  return js_graph_->machine();
}

namespace {

struct BlockEffectControlData {
  Node* current_effect = nullptr;       // New effect.
  Node* current_control = nullptr;      // New control.
  Node* current_frame_state = nullptr;  // New frame state.
};

class BlockEffectControlMap {
 public:
  explicit BlockEffectControlMap(Zone* temp_zone) : map_(temp_zone) {}

  BlockEffectControlData& For(BasicBlock* from, BasicBlock* to) {
    return map_[std::make_pair(from->rpo_number(), to->rpo_number())];
  }

  const BlockEffectControlData& For(BasicBlock* from, BasicBlock* to) const {
    return map_.at(std::make_pair(from->rpo_number(), to->rpo_number()));
  }

 private:
  typedef std::pair<int32_t, int32_t> Key;
  typedef ZoneMap<Key, BlockEffectControlData> Map;

  Map map_;
};

// Effect phis that need to be updated after the first pass.
struct PendingEffectPhi {
  Node* effect_phi;
  BasicBlock* block;

  PendingEffectPhi(Node* effect_phi, BasicBlock* block)
      : effect_phi(effect_phi), block(block) {}
};

void UpdateEffectPhi(Node* node, BasicBlock* block,
                     BlockEffectControlMap* block_effects) {
  // Update all inputs to an effect phi with the effects from the given
  // block->effect map.
  DCHECK_EQ(IrOpcode::kEffectPhi, node->opcode());
  DCHECK_EQ(node->op()->EffectInputCount(), block->PredecessorCount());
  for (int i = 0; i < node->op()->EffectInputCount(); i++) {
    Node* input = node->InputAt(i);
    BasicBlock* predecessor = block->PredecessorAt(static_cast<size_t>(i));
    const BlockEffectControlData& block_effect =
        block_effects->For(predecessor, block);
    if (input != block_effect.current_effect) {
      node->ReplaceInput(i, block_effect.current_effect);
    }
  }
}

void UpdateBlockControl(BasicBlock* block,
                        BlockEffectControlMap* block_effects) {
  Node* control = block->NodeAt(0);
  DCHECK(NodeProperties::IsControl(control));

  // Do not rewire the end node.
  if (control->opcode() == IrOpcode::kEnd) return;

  // Update all inputs to the given control node with the correct control.
  DCHECK(control->opcode() == IrOpcode::kMerge ||
         control->op()->ControlInputCount() == block->PredecessorCount());
  if (control->op()->ControlInputCount() != block->PredecessorCount()) {
    return;  // We already re-wired the control inputs of this node.
  }
  for (int i = 0; i < control->op()->ControlInputCount(); i++) {
    Node* input = NodeProperties::GetControlInput(control, i);
    BasicBlock* predecessor = block->PredecessorAt(static_cast<size_t>(i));
    const BlockEffectControlData& block_effect =
        block_effects->For(predecessor, block);
    if (input != block_effect.current_control) {
      NodeProperties::ReplaceControlInput(control, block_effect.current_control,
                                          i);
    }
  }
}

bool HasIncomingBackEdges(BasicBlock* block) {
  for (BasicBlock* pred : block->predecessors()) {
    if (pred->rpo_number() >= block->rpo_number()) {
      return true;
    }
  }
  return false;
}

void RemoveRegionNode(Node* node) {
  DCHECK(IrOpcode::kFinishRegion == node->opcode() ||
         IrOpcode::kBeginRegion == node->opcode());
  // Update the value/context uses to the value input of the finish node and
  // the effect uses to the effect input.
  for (Edge edge : node->use_edges()) {
    DCHECK(!edge.from()->IsDead());
    if (NodeProperties::IsEffectEdge(edge)) {
      edge.UpdateTo(NodeProperties::GetEffectInput(node));
    } else {
      DCHECK(!NodeProperties::IsControlEdge(edge));
      DCHECK(!NodeProperties::IsFrameStateEdge(edge));
      edge.UpdateTo(node->InputAt(0));
    }
  }
  node->Kill();
}

void TryCloneBranch(Node* node, BasicBlock* block, Graph* graph,
                    CommonOperatorBuilder* common,
                    BlockEffectControlMap* block_effects) {
  DCHECK_EQ(IrOpcode::kBranch, node->opcode());

  // This optimization is a special case of (super)block cloning. It takes an
  // input graph as shown below and clones the Branch node for every predecessor
  // to the Merge, essentially removing the Merge completely. This avoids
  // materializing the bit for the Phi and may offer potential for further
  // branch folding optimizations (i.e. because one or more inputs to the Phi is
  // a constant). Note that there may be more Phi nodes hanging off the Merge,
  // but we can only a certain subset of them currently (actually only Phi and
  // EffectPhi nodes whose uses have either the IfTrue or IfFalse as control
  // input).

  //   Control1 ... ControlN
  //      ^            ^
  //      |            |   Cond1 ... CondN
  //      +----+  +----+     ^         ^
  //           |  |          |         |
  //           |  |     +----+         |
  //          Merge<--+ | +------------+
  //            ^      \|/
  //            |      Phi
  //            |       |
  //          Branch----+
  //            ^
  //            |
  //      +-----+-----+
  //      |           |
  //    IfTrue     IfFalse
  //      ^           ^
  //      |           |

  // The resulting graph (modulo the Phi and EffectPhi nodes) looks like this:

  // Control1 Cond1 ... ControlN CondN
  //    ^      ^           ^      ^
  //    \      /           \      /
  //     Branch     ...     Branch
  //       ^                  ^
  //       |                  |
  //   +---+---+          +---+----+
  //   |       |          |        |
  // IfTrue IfFalse ... IfTrue  IfFalse
  //   ^       ^          ^        ^
  //   |       |          |        |
  //   +--+ +-------------+        |
  //      | |  +--------------+ +--+
  //      | |                 | |
  //     Merge               Merge
  //       ^                   ^
  //       |                   |

  Node* branch = node;
  Node* cond = NodeProperties::GetValueInput(branch, 0);
  if (!cond->OwnedBy(branch) || cond->opcode() != IrOpcode::kPhi) return;
  Node* merge = NodeProperties::GetControlInput(branch);
  if (merge->opcode() != IrOpcode::kMerge ||
      NodeProperties::GetControlInput(cond) != merge) {
    return;
  }
  // Grab the IfTrue/IfFalse projections of the Branch.
  BranchMatcher matcher(branch);
  // Check/collect other Phi/EffectPhi nodes hanging off the Merge.
  NodeVector phis(graph->zone());
  for (Node* const use : merge->uses()) {
    if (use == branch || use == cond) continue;
    // We cannot currently deal with non-Phi/EffectPhi nodes hanging off the
    // Merge. Ideally, we would just clone the nodes (and everything that
    // depends on it to some distant join point), but that requires knowledge
    // about dominance/post-dominance.
    if (!NodeProperties::IsPhi(use)) return;
    for (Edge edge : use->use_edges()) {
      // Right now we can only handle Phi/EffectPhi nodes whose uses are
      // directly control-dependend on either the IfTrue or the IfFalse
      // successor, because we know exactly how to update those uses.
      if (edge.from()->op()->ControlInputCount() != 1) return;
      Node* control = NodeProperties::GetControlInput(edge.from());
      if (NodeProperties::IsPhi(edge.from())) {
        control = NodeProperties::GetControlInput(control, edge.index());
      }
      if (control != matcher.IfTrue() && control != matcher.IfFalse()) return;
    }
    phis.push_back(use);
  }
  BranchHint const hint = BranchHintOf(branch->op());
  int const input_count = merge->op()->ControlInputCount();
  DCHECK_LE(1, input_count);
  Node** const inputs = graph->zone()->NewArray<Node*>(2 * input_count);
  Node** const merge_true_inputs = &inputs[0];
  Node** const merge_false_inputs = &inputs[input_count];
  for (int index = 0; index < input_count; ++index) {
    Node* cond1 = NodeProperties::GetValueInput(cond, index);
    Node* control1 = NodeProperties::GetControlInput(merge, index);
    Node* branch1 = graph->NewNode(common->Branch(hint), cond1, control1);
    merge_true_inputs[index] = graph->NewNode(common->IfTrue(), branch1);
    merge_false_inputs[index] = graph->NewNode(common->IfFalse(), branch1);
  }
  Node* const merge_true = matcher.IfTrue();
  Node* const merge_false = matcher.IfFalse();
  merge_true->TrimInputCount(0);
  merge_false->TrimInputCount(0);
  for (int i = 0; i < input_count; ++i) {
    merge_true->AppendInput(graph->zone(), merge_true_inputs[i]);
    merge_false->AppendInput(graph->zone(), merge_false_inputs[i]);
  }
  DCHECK_EQ(2, block->SuccessorCount());
  NodeProperties::ChangeOp(matcher.IfTrue(), common->Merge(input_count));
  NodeProperties::ChangeOp(matcher.IfFalse(), common->Merge(input_count));
  int const true_index =
      block->SuccessorAt(0)->NodeAt(0) == matcher.IfTrue() ? 0 : 1;
  BlockEffectControlData* true_block_data =
      &block_effects->For(block, block->SuccessorAt(true_index));
  BlockEffectControlData* false_block_data =
      &block_effects->For(block, block->SuccessorAt(true_index ^ 1));
  for (Node* const phi : phis) {
    for (int index = 0; index < input_count; ++index) {
      inputs[index] = phi->InputAt(index);
    }
    inputs[input_count] = merge_true;
    Node* phi_true = graph->NewNode(phi->op(), input_count + 1, inputs);
    inputs[input_count] = merge_false;
    Node* phi_false = graph->NewNode(phi->op(), input_count + 1, inputs);
    if (phi->UseCount() == 0) {
      DCHECK_EQ(phi->opcode(), IrOpcode::kEffectPhi);
      DCHECK_EQ(input_count, block->SuccessorCount());
    } else {
      for (Edge edge : phi->use_edges()) {
        Node* control = NodeProperties::GetControlInput(edge.from());
        if (NodeProperties::IsPhi(edge.from())) {
          control = NodeProperties::GetControlInput(control, edge.index());
        }
        DCHECK(control == matcher.IfTrue() || control == matcher.IfFalse());
        edge.UpdateTo((control == matcher.IfTrue()) ? phi_true : phi_false);
      }
    }
    if (phi->opcode() == IrOpcode::kEffectPhi) {
      true_block_data->current_effect = phi_true;
      false_block_data->current_effect = phi_false;
    }
    phi->Kill();
  }
  // Fix up IfTrue and IfFalse and kill all dead nodes.
  if (branch == block->control_input()) {
    true_block_data->current_control = merge_true;
    false_block_data->current_control = merge_false;
  }
  branch->Kill();
  cond->Kill();
  merge->Kill();
}
}  // namespace

void EffectControlLinearizer::Run() {
  BlockEffectControlMap block_effects(temp_zone());
  ZoneVector<PendingEffectPhi> pending_effect_phis(temp_zone());
  ZoneVector<BasicBlock*> pending_block_controls(temp_zone());
  NodeVector inputs_buffer(temp_zone());

  for (BasicBlock* block : *(schedule()->rpo_order())) {
    size_t instr = 0;

    // The control node should be the first.
    Node* control = block->NodeAt(instr);
    DCHECK(NodeProperties::IsControl(control));
    // Update the control inputs.
    if (HasIncomingBackEdges(block)) {
      // If there are back edges, we need to update later because we have not
      // computed the control yet. This should only happen for loops.
      DCHECK_EQ(IrOpcode::kLoop, control->opcode());
      pending_block_controls.push_back(block);
    } else {
      // If there are no back edges, we can update now.
      UpdateBlockControl(block, &block_effects);
    }
    instr++;

    // Iterate over the phis and update the effect phis.
    Node* effect = nullptr;
    Node* terminate = nullptr;
    for (; instr < block->NodeCount(); instr++) {
      Node* node = block->NodeAt(instr);
      // Only go through the phis and effect phis.
      if (node->opcode() == IrOpcode::kEffectPhi) {
        // There should be at most one effect phi in a block.
        DCHECK_NULL(effect);
        // IfException blocks should not have effect phis.
        DCHECK_NE(IrOpcode::kIfException, control->opcode());
        effect = node;

        // Make sure we update the inputs to the incoming blocks' effects.
        if (HasIncomingBackEdges(block)) {
          // In case of loops, we do not update the effect phi immediately
          // because the back predecessor has not been handled yet. We just
          // record the effect phi for later processing.
          pending_effect_phis.push_back(PendingEffectPhi(node, block));
        } else {
          UpdateEffectPhi(node, block, &block_effects);
        }
      } else if (node->opcode() == IrOpcode::kPhi) {
        // Just skip phis.
      } else if (node->opcode() == IrOpcode::kTerminate) {
        DCHECK(terminate == nullptr);
        terminate = node;
      } else {
        break;
      }
    }

    if (effect == nullptr) {
      // There was no effect phi.
      DCHECK(!HasIncomingBackEdges(block));
      if (block == schedule()->start()) {
        // Start block => effect is start.
        DCHECK_EQ(graph()->start(), control);
        effect = graph()->start();
      } else if (control->opcode() == IrOpcode::kEnd) {
        // End block is just a dummy, no effect needed.
        DCHECK_EQ(BasicBlock::kNone, block->control());
        DCHECK_EQ(1u, block->size());
        effect = nullptr;
      } else {
        // If all the predecessors have the same effect, we can use it as our
        // current effect.
        effect =
            block_effects.For(block->PredecessorAt(0), block).current_effect;
        for (size_t i = 1; i < block->PredecessorCount(); ++i) {
          if (block_effects.For(block->PredecessorAt(i), block)
                  .current_effect != effect) {
            effect = nullptr;
            break;
          }
        }
        if (effect == nullptr) {
          DCHECK_NE(IrOpcode::kIfException, control->opcode());
          // The input blocks do not have the same effect. We have
          // to create an effect phi node.
          inputs_buffer.clear();
          inputs_buffer.resize(block->PredecessorCount(), jsgraph()->Dead());
          inputs_buffer.push_back(control);
          effect = graph()->NewNode(
              common()->EffectPhi(static_cast<int>(block->PredecessorCount())),
              static_cast<int>(inputs_buffer.size()), &(inputs_buffer.front()));
          // For loops, we update the effect phi node later to break cycles.
          if (control->opcode() == IrOpcode::kLoop) {
            pending_effect_phis.push_back(PendingEffectPhi(effect, block));
          } else {
            UpdateEffectPhi(effect, block, &block_effects);
          }
        } else if (control->opcode() == IrOpcode::kIfException) {
          // The IfException is connected into the effect chain, so we need
          // to update the effect here.
          NodeProperties::ReplaceEffectInput(control, effect);
          effect = control;
        }
      }
    }

    // Fixup the Terminate node.
    if (terminate != nullptr) {
      NodeProperties::ReplaceEffectInput(terminate, effect);
    }

    // The frame state at block entry is determined by the frame states leaving
    // all predecessors. In case there is no frame state dominating this block,
    // we can rely on a checkpoint being present before the next deoptimization.
    // TODO(mstarzinger): Eventually we will need to go hunt for a frame state
    // once deoptimizing nodes roam freely through the schedule.
    Node* frame_state = nullptr;
    if (block != schedule()->start()) {
      // If all the predecessors have the same effect, we can use it
      // as our current effect.
      frame_state =
          block_effects.For(block->PredecessorAt(0), block).current_frame_state;
      for (size_t i = 1; i < block->PredecessorCount(); i++) {
        if (block_effects.For(block->PredecessorAt(i), block)
                .current_frame_state != frame_state) {
          frame_state = nullptr;
          break;
        }
      }
    }

    // Process the ordinary instructions.
    for (; instr < block->NodeCount(); instr++) {
      Node* node = block->NodeAt(instr);
      ProcessNode(node, &frame_state, &effect, &control);
    }

    switch (block->control()) {
      case BasicBlock::kGoto:
      case BasicBlock::kNone:
        break;

      case BasicBlock::kCall:
      case BasicBlock::kTailCall:
      case BasicBlock::kSwitch:
      case BasicBlock::kReturn:
      case BasicBlock::kDeoptimize:
      case BasicBlock::kThrow:
        ProcessNode(block->control_input(), &frame_state, &effect, &control);
        break;

      case BasicBlock::kBranch:
        ProcessNode(block->control_input(), &frame_state, &effect, &control);
        TryCloneBranch(block->control_input(), block, graph(), common(),
                       &block_effects);
        break;
    }

    // Store the effect, control and frame state for later use.
    for (BasicBlock* successor : block->successors()) {
      BlockEffectControlData* data = &block_effects.For(block, successor);
      if (data->current_effect == nullptr) {
        data->current_effect = effect;
      }
      if (data->current_control == nullptr) {
        data->current_control = control;
      }
      data->current_frame_state = frame_state;
    }
  }

  // Update the incoming edges of the effect phis that could not be processed
  // during the first pass (because they could have incoming back edges).
  for (const PendingEffectPhi& pending_effect_phi : pending_effect_phis) {
    UpdateEffectPhi(pending_effect_phi.effect_phi, pending_effect_phi.block,
                    &block_effects);
  }
  for (BasicBlock* pending_block_control : pending_block_controls) {
    UpdateBlockControl(pending_block_control, &block_effects);
  }
}

namespace {

void TryScheduleCallIfSuccess(Node* node, Node** control) {
  // Schedule the call's IfSuccess node if there is no exception use.
  if (!NodeProperties::IsExceptionalCall(node)) {
    for (Edge edge : node->use_edges()) {
      if (NodeProperties::IsControlEdge(edge) &&
          edge.from()->opcode() == IrOpcode::kIfSuccess) {
        *control = edge.from();
      }
    }
  }
}

}  // namespace

void EffectControlLinearizer::ProcessNode(Node* node, Node** frame_state,
                                          Node** effect, Node** control) {
  // If the node needs to be wired into the effect/control chain, do this
  // here. Pass current frame state for lowering to eager deoptimization.
  if (TryWireInStateEffect(node, *frame_state, effect, control)) {
    return;
  }

  // If the node has a visible effect, then there must be a checkpoint in the
  // effect chain before we are allowed to place another eager deoptimization
  // point. We zap the frame state to ensure this invariant is maintained.
  if (region_observability_ == RegionObservability::kObservable &&
      !node->op()->HasProperty(Operator::kNoWrite)) {
    *frame_state = nullptr;
  }

  // Remove the end markers of 'atomic' allocation region because the
  // region should be wired-in now.
  if (node->opcode() == IrOpcode::kFinishRegion) {
    // Reset the current region observability.
    region_observability_ = RegionObservability::kObservable;
    // Update the value uses to the value input of the finish node and
    // the effect uses to the effect input.
    return RemoveRegionNode(node);
  }
  if (node->opcode() == IrOpcode::kBeginRegion) {
    // Determine the observability for this region and use that for all
    // nodes inside the region (i.e. ignore the absence of kNoWrite on
    // StoreField and other operators).
    DCHECK_NE(RegionObservability::kNotObservable, region_observability_);
    region_observability_ = RegionObservabilityOf(node->op());
    // Update the value uses to the value input of the finish node and
    // the effect uses to the effect input.
    return RemoveRegionNode(node);
  }

  // Special treatment for checkpoint nodes.
  if (node->opcode() == IrOpcode::kCheckpoint) {
    // Unlink the check point; effect uses will be updated to the incoming
    // effect that is passed. The frame state is preserved for lowering.
    DCHECK_EQ(RegionObservability::kObservable, region_observability_);
    *frame_state = NodeProperties::GetFrameStateInput(node, 0);
    return;
  }

  if (node->opcode() == IrOpcode::kIfSuccess) {
    // We always schedule IfSuccess with its call, so skip it here.
    DCHECK_EQ(IrOpcode::kCall, node->InputAt(0)->opcode());
    // The IfSuccess node should not belong to an exceptional call node
    // because such IfSuccess nodes should only start a basic block (and
    // basic block start nodes are not handled in the ProcessNode method).
    DCHECK(!NodeProperties::IsExceptionalCall(node->InputAt(0)));
    return;
  }

  // If the node takes an effect, replace with the current one.
  if (node->op()->EffectInputCount() > 0) {
    DCHECK_EQ(1, node->op()->EffectInputCount());
    Node* input_effect = NodeProperties::GetEffectInput(node);

    if (input_effect != *effect) {
      NodeProperties::ReplaceEffectInput(node, *effect);
    }

    // If the node produces an effect, update our current effect. (However,
    // ignore new effect chains started with ValueEffect.)
    if (node->op()->EffectOutputCount() > 0) {
      DCHECK_EQ(1, node->op()->EffectOutputCount());
      *effect = node;
    }
  } else {
    // New effect chain is only started with a Start or ValueEffect node.
    DCHECK(node->op()->EffectOutputCount() == 0 ||
           node->opcode() == IrOpcode::kStart);
  }

  // Rewire control inputs.
  for (int i = 0; i < node->op()->ControlInputCount(); i++) {
    NodeProperties::ReplaceControlInput(node, *control, i);
  }
  // Update the current control and wire IfSuccess right after calls.
  if (node->op()->ControlOutputCount() > 0) {
    *control = node;
    if (node->opcode() == IrOpcode::kCall) {
      // Schedule the call's IfSuccess node (if there is no exception use).
      TryScheduleCallIfSuccess(node, control);
    }
  }
}

bool EffectControlLinearizer::TryWireInStateEffect(Node* node,
                                                   Node* frame_state,
                                                   Node** effect,
                                                   Node** control) {
  ValueEffectControl state(nullptr, nullptr, nullptr);
  switch (node->opcode()) {
    case IrOpcode::kChangeBitToTagged:
      state = LowerChangeBitToTagged(node, *effect, *control);
      break;
    case IrOpcode::kChangeInt31ToTaggedSigned:
      state = LowerChangeInt31ToTaggedSigned(node, *effect, *control);
      break;
    case IrOpcode::kChangeInt32ToTagged:
      state = LowerChangeInt32ToTagged(node, *effect, *control);
      break;
    case IrOpcode::kChangeUint32ToTagged:
      state = LowerChangeUint32ToTagged(node, *effect, *control);
      break;
    case IrOpcode::kChangeFloat64ToTagged:
      state = LowerChangeFloat64ToTagged(node, *effect, *control);
      break;
    case IrOpcode::kChangeTaggedSignedToInt32:
      state = LowerChangeTaggedSignedToInt32(node, *effect, *control);
      break;
    case IrOpcode::kChangeTaggedToBit:
      state = LowerChangeTaggedToBit(node, *effect, *control);
      break;
    case IrOpcode::kChangeTaggedToInt32:
      state = LowerChangeTaggedToInt32(node, *effect, *control);
      break;
    case IrOpcode::kChangeTaggedToUint32:
      state = LowerChangeTaggedToUint32(node, *effect, *control);
      break;
    case IrOpcode::kChangeTaggedToFloat64:
      state = LowerChangeTaggedToFloat64(node, *effect, *control);
      break;
    case IrOpcode::kTruncateTaggedToFloat64:
      state = LowerTruncateTaggedToFloat64(node, *effect, *control);
      break;
    case IrOpcode::kCheckBounds:
      state = LowerCheckBounds(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kCheckNumber:
      state = LowerCheckNumber(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kCheckString:
      state = LowerCheckString(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kCheckIf:
      state = LowerCheckIf(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kCheckTaggedPointer:
      state = LowerCheckTaggedPointer(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kCheckTaggedSigned:
      state = LowerCheckTaggedSigned(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kCheckedInt32Add:
      state = LowerCheckedInt32Add(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kCheckedInt32Sub:
      state = LowerCheckedInt32Sub(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kCheckedInt32Div:
      state = LowerCheckedInt32Div(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kCheckedInt32Mod:
      state = LowerCheckedInt32Mod(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kCheckedUint32Div:
      state = LowerCheckedUint32Div(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kCheckedUint32Mod:
      state = LowerCheckedUint32Mod(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kCheckedInt32Mul:
      state = LowerCheckedInt32Mul(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kCheckedUint32ToInt32:
      state = LowerCheckedUint32ToInt32(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kCheckedFloat64ToInt32:
      state = LowerCheckedFloat64ToInt32(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kCheckedTaggedToInt32:
      state = LowerCheckedTaggedToInt32(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kCheckedTaggedToFloat64:
      state = LowerCheckedTaggedToFloat64(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kTruncateTaggedToWord32:
      state = LowerTruncateTaggedToWord32(node, *effect, *control);
      break;
    case IrOpcode::kCheckedTruncateTaggedToWord32:
      state = LowerCheckedTruncateTaggedToWord32(node, frame_state, *effect,
                                                 *control);
      break;
    case IrOpcode::kObjectIsCallable:
      state = LowerObjectIsCallable(node, *effect, *control);
      break;
    case IrOpcode::kObjectIsNumber:
      state = LowerObjectIsNumber(node, *effect, *control);
      break;
    case IrOpcode::kObjectIsReceiver:
      state = LowerObjectIsReceiver(node, *effect, *control);
      break;
    case IrOpcode::kObjectIsSmi:
      state = LowerObjectIsSmi(node, *effect, *control);
      break;
    case IrOpcode::kObjectIsString:
      state = LowerObjectIsString(node, *effect, *control);
      break;
    case IrOpcode::kObjectIsUndetectable:
      state = LowerObjectIsUndetectable(node, *effect, *control);
      break;
    case IrOpcode::kStringFromCharCode:
      state = LowerStringFromCharCode(node, *effect, *control);
      break;
    case IrOpcode::kCheckFloat64Hole:
      state = LowerCheckFloat64Hole(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kCheckTaggedHole:
      state = LowerCheckTaggedHole(node, frame_state, *effect, *control);
      break;
    case IrOpcode::kPlainPrimitiveToNumber:
      state = LowerPlainPrimitiveToNumber(node, *effect, *control);
      break;
    case IrOpcode::kPlainPrimitiveToWord32:
      state = LowerPlainPrimitiveToWord32(node, *effect, *control);
      break;
    case IrOpcode::kPlainPrimitiveToFloat64:
      state = LowerPlainPrimitiveToFloat64(node, *effect, *control);
      break;
    case IrOpcode::kTransitionElementsKind:
      state = LowerTransitionElementsKind(node, *effect, *control);
      break;
    default:
      return false;
  }
  NodeProperties::ReplaceUses(node, state.value, state.effect, state.control);
  *effect = state.effect;
  *control = state.control;
  return true;
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerChangeFloat64ToTagged(Node* node, Node* effect,
                                                    Node* control) {
  Node* value = node->InputAt(0);

  Node* value32 = graph()->NewNode(machine()->RoundFloat64ToInt32(), value);
  Node* check_same = graph()->NewNode(
      machine()->Float64Equal(), value,
      graph()->NewNode(machine()->ChangeInt32ToFloat64(), value32));
  Node* branch_same = graph()->NewNode(common()->Branch(), check_same, control);

  Node* if_smi = graph()->NewNode(common()->IfTrue(), branch_same);
  Node* vsmi;
  Node* if_box = graph()->NewNode(common()->IfFalse(), branch_same);

  // Check if {value} is -0.
  Node* check_zero = graph()->NewNode(machine()->Word32Equal(), value32,
                                      jsgraph()->Int32Constant(0));
  Node* branch_zero = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                       check_zero, if_smi);

  Node* if_zero = graph()->NewNode(common()->IfTrue(), branch_zero);
  Node* if_notzero = graph()->NewNode(common()->IfFalse(), branch_zero);

  // In case of 0, we need to check the high bits for the IEEE -0 pattern.
  Node* check_negative = graph()->NewNode(
      machine()->Int32LessThan(),
      graph()->NewNode(machine()->Float64ExtractHighWord32(), value),
      jsgraph()->Int32Constant(0));
  Node* branch_negative = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                           check_negative, if_zero);

  Node* if_negative = graph()->NewNode(common()->IfTrue(), branch_negative);
  Node* if_notnegative = graph()->NewNode(common()->IfFalse(), branch_negative);

  // We need to create a box for negative 0.
  if_smi = graph()->NewNode(common()->Merge(2), if_notzero, if_notnegative);
  if_box = graph()->NewNode(common()->Merge(2), if_box, if_negative);

  // On 64-bit machines we can just wrap the 32-bit integer in a smi, for 32-bit
  // machines we need to deal with potential overflow and fallback to boxing.
  if (machine()->Is64()) {
    vsmi = ChangeInt32ToSmi(value32);
  } else {
    Node* smi_tag = graph()->NewNode(machine()->Int32AddWithOverflow(), value32,
                                     value32, if_smi);

    Node* check_ovf =
        graph()->NewNode(common()->Projection(1), smi_tag, if_smi);
    Node* branch_ovf = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                        check_ovf, if_smi);

    Node* if_ovf = graph()->NewNode(common()->IfTrue(), branch_ovf);
    if_box = graph()->NewNode(common()->Merge(2), if_ovf, if_box);

    if_smi = graph()->NewNode(common()->IfFalse(), branch_ovf);
    vsmi = graph()->NewNode(common()->Projection(0), smi_tag, if_smi);
  }

  // Allocate the box for the {value}.
  ValueEffectControl box = AllocateHeapNumberWithValue(value, effect, if_box);

  control = graph()->NewNode(common()->Merge(2), if_smi, box.control);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                           vsmi, box.value, control);
  effect =
      graph()->NewNode(common()->EffectPhi(2), effect, box.effect, control);
  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerChangeBitToTagged(Node* node, Node* effect,
                                                Node* control) {
  Node* value = node->InputAt(0);

  Node* branch = graph()->NewNode(common()->Branch(), value, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* vtrue = jsgraph()->TrueConstant();

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* vfalse = jsgraph()->FalseConstant();

  control = graph()->NewNode(common()->Merge(2), if_true, if_false);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                           vtrue, vfalse, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerChangeInt31ToTaggedSigned(Node* node,
                                                        Node* effect,
                                                        Node* control) {
  Node* value = node->InputAt(0);
  value = ChangeInt32ToSmi(value);
  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerChangeInt32ToTagged(Node* node, Node* effect,
                                                  Node* control) {
  Node* value = node->InputAt(0);

  if (machine()->Is64()) {
    return ValueEffectControl(ChangeInt32ToSmi(value), effect, control);
  }

  Node* add = graph()->NewNode(machine()->Int32AddWithOverflow(), value, value,
                               control);

  Node* ovf = graph()->NewNode(common()->Projection(1), add, control);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kFalse), ovf, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  ValueEffectControl alloc =
      AllocateHeapNumberWithValue(ChangeInt32ToFloat64(value), effect, if_true);

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* vfalse = graph()->NewNode(common()->Projection(0), add, if_false);

  Node* merge = graph()->NewNode(common()->Merge(2), alloc.control, if_false);
  Node* phi = graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                               alloc.value, vfalse, merge);
  Node* ephi =
      graph()->NewNode(common()->EffectPhi(2), alloc.effect, effect, merge);

  return ValueEffectControl(phi, ephi, merge);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerChangeUint32ToTagged(Node* node, Node* effect,
                                                   Node* control) {
  Node* value = node->InputAt(0);

  Node* check = graph()->NewNode(machine()->Uint32LessThanOrEqual(), value,
                                 SmiMaxValueConstant());
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* vtrue = ChangeUint32ToSmi(value);

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  ValueEffectControl alloc = AllocateHeapNumberWithValue(
      ChangeUint32ToFloat64(value), effect, if_false);

  Node* merge = graph()->NewNode(common()->Merge(2), if_true, alloc.control);
  Node* phi = graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                               vtrue, alloc.value, merge);
  Node* ephi =
      graph()->NewNode(common()->EffectPhi(2), effect, alloc.effect, merge);

  return ValueEffectControl(phi, ephi, merge);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerChangeTaggedSignedToInt32(Node* node,
                                                        Node* effect,
                                                        Node* control) {
  Node* value = node->InputAt(0);
  value = ChangeSmiToInt32(value);
  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerChangeTaggedToBit(Node* node, Node* effect,
                                                Node* control) {
  Node* value = node->InputAt(0);
  value = graph()->NewNode(machine()->WordEqual(), value,
                           jsgraph()->TrueConstant());
  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerChangeTaggedToInt32(Node* node, Node* effect,
                                                  Node* control) {
  Node* value = node->InputAt(0);

  Node* check = ObjectIsSmi(value);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;
  Node* vtrue = ChangeSmiToInt32(value);

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* efalse = effect;
  Node* vfalse;
  {
    STATIC_ASSERT(HeapNumber::kValueOffset == Oddball::kToNumberRawOffset);
    vfalse = efalse = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForHeapNumberValue()), value,
        efalse, if_false);
    vfalse = graph()->NewNode(machine()->ChangeFloat64ToInt32(), vfalse);
  }

  control = graph()->NewNode(common()->Merge(2), if_true, if_false);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 2),
                           vtrue, vfalse, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerChangeTaggedToUint32(Node* node, Node* effect,
                                                   Node* control) {
  Node* value = node->InputAt(0);

  Node* check = ObjectIsSmi(value);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;
  Node* vtrue = ChangeSmiToInt32(value);

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* efalse = effect;
  Node* vfalse;
  {
    STATIC_ASSERT(HeapNumber::kValueOffset == Oddball::kToNumberRawOffset);
    vfalse = efalse = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForHeapNumberValue()), value,
        efalse, if_false);
    vfalse = graph()->NewNode(machine()->ChangeFloat64ToUint32(), vfalse);
  }

  control = graph()->NewNode(common()->Merge(2), if_true, if_false);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 2),
                           vtrue, vfalse, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerChangeTaggedToFloat64(Node* node, Node* effect,
                                                    Node* control) {
  return LowerTruncateTaggedToFloat64(node, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerTruncateTaggedToFloat64(Node* node, Node* effect,
                                                      Node* control) {
  Node* value = node->InputAt(0);

  Node* check = ObjectIsSmi(value);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;
  Node* vtrue;
  {
    vtrue = ChangeSmiToInt32(value);
    vtrue = graph()->NewNode(machine()->ChangeInt32ToFloat64(), vtrue);
  }

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* efalse = effect;
  Node* vfalse;
  {
    STATIC_ASSERT(HeapNumber::kValueOffset == Oddball::kToNumberRawOffset);
    vfalse = efalse = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForHeapNumberValue()), value,
        efalse, if_false);
  }

  control = graph()->NewNode(common()->Merge(2), if_true, if_false);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kFloat64, 2),
                           vtrue, vfalse, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckBounds(Node* node, Node* frame_state,
                                          Node* effect, Node* control) {
  Node* index = node->InputAt(0);
  Node* limit = node->InputAt(1);

  Node* check = graph()->NewNode(machine()->Uint32LessThan(), index, limit);
  control = effect = graph()->NewNode(
      common()->DeoptimizeUnless(DeoptimizeReason::kOutOfBounds), check,
      frame_state, effect, control);

  return ValueEffectControl(index, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckNumber(Node* node, Node* frame_state,
                                          Node* effect, Node* control) {
  Node* value = node->InputAt(0);

  Node* check0 = ObjectIsSmi(value);
  Node* branch0 =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check0, control);

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* etrue0 = effect;

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* efalse0 = effect;
  {
    Node* value_map = efalse0 =
        graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                         value, efalse0, if_false0);
    Node* check1 = graph()->NewNode(machine()->WordEqual(), value_map,
                                    jsgraph()->HeapNumberMapConstant());
    if_false0 = efalse0 = graph()->NewNode(
        common()->DeoptimizeUnless(DeoptimizeReason::kNotAHeapNumber), check1,
        frame_state, efalse0, if_false0);
  }

  control = graph()->NewNode(common()->Merge(2), if_true0, if_false0);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue0, efalse0, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckString(Node* node, Node* frame_state,
                                          Node* effect, Node* control) {
  Node* value = node->InputAt(0);

  Node* check0 = ObjectIsSmi(value);
  control = effect =
      graph()->NewNode(common()->DeoptimizeIf(DeoptimizeReason::kSmi), check0,
                       frame_state, effect, control);

  Node* value_map = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForMap()), value, effect, control);
  Node* value_instance_type = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForMapInstanceType()), value_map,
      effect, control);

  Node* check1 =
      graph()->NewNode(machine()->Uint32LessThan(), value_instance_type,
                       jsgraph()->Uint32Constant(FIRST_NONSTRING_TYPE));
  control = effect = graph()->NewNode(
      common()->DeoptimizeUnless(DeoptimizeReason::kWrongInstanceType), check1,
      frame_state, effect, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckIf(Node* node, Node* frame_state,
                                      Node* effect, Node* control) {
  Node* value = node->InputAt(0);

  control = effect =
      graph()->NewNode(common()->DeoptimizeUnless(DeoptimizeReason::kNoReason),
                       value, frame_state, effect, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckTaggedPointer(Node* node, Node* frame_state,
                                                 Node* effect, Node* control) {
  Node* value = node->InputAt(0);

  Node* check = ObjectIsSmi(value);
  control = effect =
      graph()->NewNode(common()->DeoptimizeIf(DeoptimizeReason::kSmi), check,
                       frame_state, effect, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckTaggedSigned(Node* node, Node* frame_state,
                                                Node* effect, Node* control) {
  Node* value = node->InputAt(0);

  Node* check = ObjectIsSmi(value);
  control = effect =
      graph()->NewNode(common()->DeoptimizeUnless(DeoptimizeReason::kNotASmi),
                       check, frame_state, effect, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckedInt32Add(Node* node, Node* frame_state,
                                              Node* effect, Node* control) {
  Node* lhs = node->InputAt(0);
  Node* rhs = node->InputAt(1);

  Node* value =
      graph()->NewNode(machine()->Int32AddWithOverflow(), lhs, rhs, control);

  Node* check = graph()->NewNode(common()->Projection(1), value, control);
  control = effect =
      graph()->NewNode(common()->DeoptimizeIf(DeoptimizeReason::kOverflow),
                       check, frame_state, effect, control);

  value = graph()->NewNode(common()->Projection(0), value, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckedInt32Sub(Node* node, Node* frame_state,
                                              Node* effect, Node* control) {
  Node* lhs = node->InputAt(0);
  Node* rhs = node->InputAt(1);

  Node* value =
      graph()->NewNode(machine()->Int32SubWithOverflow(), lhs, rhs, control);

  Node* check = graph()->NewNode(common()->Projection(1), value, control);
  control = effect =
      graph()->NewNode(common()->DeoptimizeIf(DeoptimizeReason::kOverflow),
                       check, frame_state, effect, control);

  value = graph()->NewNode(common()->Projection(0), value, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckedInt32Div(Node* node, Node* frame_state,
                                              Node* effect, Node* control) {
  Node* zero = jsgraph()->Int32Constant(0);
  Node* minusone = jsgraph()->Int32Constant(-1);
  Node* minint = jsgraph()->Int32Constant(std::numeric_limits<int32_t>::min());

  Node* lhs = node->InputAt(0);
  Node* rhs = node->InputAt(1);

  // Check if {rhs} is positive (and not zero).
  Node* check0 = graph()->NewNode(machine()->Int32LessThan(), zero, rhs);
  Node* branch0 =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check0, control);

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* etrue0 = effect;
  Node* vtrue0;
  {
    // Fast case, no additional checking required.
    vtrue0 = graph()->NewNode(machine()->Int32Div(), lhs, rhs, if_true0);
  }

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* efalse0 = effect;
  Node* vfalse0;
  {
    // Check if {rhs} is zero.
    Node* check = graph()->NewNode(machine()->Word32Equal(), rhs, zero);
    if_false0 = efalse0 = graph()->NewNode(
        common()->DeoptimizeIf(DeoptimizeReason::kDivisionByZero), check,
        frame_state, efalse0, if_false0);

    // Check if {lhs} is zero, as that would produce minus zero.
    check = graph()->NewNode(machine()->Word32Equal(), lhs, zero);
    if_false0 = efalse0 =
        graph()->NewNode(common()->DeoptimizeIf(DeoptimizeReason::kMinusZero),
                         check, frame_state, efalse0, if_false0);

    // Check if {lhs} is kMinInt and {rhs} is -1, in which case we'd have
    // to return -kMinInt, which is not representable.
    Node* check1 = graph()->NewNode(machine()->Word32Equal(), lhs, minint);
    Node* branch1 = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                     check1, if_false0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* etrue1 = efalse0;
    {
      // Check if {rhs} is -1.
      Node* check = graph()->NewNode(machine()->Word32Equal(), rhs, minusone);
      if_true1 = etrue1 =
          graph()->NewNode(common()->DeoptimizeIf(DeoptimizeReason::kOverflow),
                           check, frame_state, etrue1, if_true1);
    }

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* efalse1 = efalse0;

    if_false0 = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
    efalse0 =
        graph()->NewNode(common()->EffectPhi(2), etrue1, efalse1, if_false0);

    // Perform the actual integer division.
    vfalse0 = graph()->NewNode(machine()->Int32Div(), lhs, rhs, if_false0);
  }

  control = graph()->NewNode(common()->Merge(2), if_true0, if_false0);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue0, efalse0, control);
  Node* value =
      graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 2), vtrue0,
                       vfalse0, control);

  // Check if the remainder is non-zero.
  Node* check =
      graph()->NewNode(machine()->Word32Equal(), lhs,
                       graph()->NewNode(machine()->Int32Mul(), rhs, value));
  control = effect = graph()->NewNode(
      common()->DeoptimizeUnless(DeoptimizeReason::kLostPrecision), check,
      frame_state, effect, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckedInt32Mod(Node* node, Node* frame_state,
                                              Node* effect, Node* control) {
  Node* zero = jsgraph()->Int32Constant(0);
  Node* minusone = jsgraph()->Int32Constant(-1);
  Node* minint = jsgraph()->Int32Constant(std::numeric_limits<int32_t>::min());

  Node* lhs = node->InputAt(0);
  Node* rhs = node->InputAt(1);

  // Ensure that {rhs} is not zero, otherwise we'd have to return NaN.
  Node* check = graph()->NewNode(machine()->Word32Equal(), rhs, zero);
  control = effect = graph()->NewNode(
      common()->DeoptimizeIf(DeoptimizeReason::kDivisionByZero), check,
      frame_state, effect, control);

  // Check if {lhs} is positive or zero.
  Node* check0 = graph()->NewNode(machine()->Int32LessThanOrEqual(), zero, lhs);
  Node* branch0 =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check0, control);

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* etrue0 = effect;
  Node* vtrue0;
  {
    // Fast case, no additional checking required.
    vtrue0 = graph()->NewNode(machine()->Int32Mod(), lhs, rhs, if_true0);
  }

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* efalse0 = effect;
  Node* vfalse0;
  {
    // Check if {lhs} is kMinInt and {rhs} is -1, in which case we'd have
    // to return -0.
    Node* check1 = graph()->NewNode(machine()->Word32Equal(), lhs, minint);
    Node* branch1 = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                     check1, if_false0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* etrue1 = efalse0;
    {
      // Check if {rhs} is -1.
      Node* check = graph()->NewNode(machine()->Word32Equal(), rhs, minusone);
      if_true1 = etrue1 =
          graph()->NewNode(common()->DeoptimizeIf(DeoptimizeReason::kMinusZero),
                           check, frame_state, etrue1, if_true1);
    }

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* efalse1 = efalse0;

    if_false0 = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
    efalse0 =
        graph()->NewNode(common()->EffectPhi(2), etrue1, efalse1, if_false0);

    // Perform the actual integer modulos.
    vfalse0 = graph()->NewNode(machine()->Int32Mod(), lhs, rhs, if_false0);

    // Check if the result is zero, because in that case we'd have to return
    // -0 here since we always take the signe of the {lhs} which is negative.
    Node* check = graph()->NewNode(machine()->Word32Equal(), vfalse0, zero);
    if_false0 = efalse0 =
        graph()->NewNode(common()->DeoptimizeIf(DeoptimizeReason::kMinusZero),
                         check, frame_state, efalse0, if_false0);
  }

  control = graph()->NewNode(common()->Merge(2), if_true0, if_false0);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue0, efalse0, control);
  Node* value =
      graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 2), vtrue0,
                       vfalse0, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckedUint32Div(Node* node, Node* frame_state,
                                               Node* effect, Node* control) {
  Node* zero = jsgraph()->Int32Constant(0);

  Node* lhs = node->InputAt(0);
  Node* rhs = node->InputAt(1);

  // Ensure that {rhs} is not zero, otherwise we'd have to return NaN.
  Node* check = graph()->NewNode(machine()->Word32Equal(), rhs, zero);
  control = effect = graph()->NewNode(
      common()->DeoptimizeIf(DeoptimizeReason::kDivisionByZero), check,
      frame_state, effect, control);

  // Perform the actual unsigned integer division.
  Node* value = graph()->NewNode(machine()->Uint32Div(), lhs, rhs, control);

  // Check if the remainder is non-zero.
  check = graph()->NewNode(machine()->Word32Equal(), lhs,
                           graph()->NewNode(machine()->Int32Mul(), rhs, value));
  control = effect = graph()->NewNode(
      common()->DeoptimizeUnless(DeoptimizeReason::kLostPrecision), check,
      frame_state, effect, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckedUint32Mod(Node* node, Node* frame_state,
                                               Node* effect, Node* control) {
  Node* zero = jsgraph()->Int32Constant(0);

  Node* lhs = node->InputAt(0);
  Node* rhs = node->InputAt(1);

  // Ensure that {rhs} is not zero, otherwise we'd have to return NaN.
  Node* check = graph()->NewNode(machine()->Word32Equal(), rhs, zero);
  control = effect = graph()->NewNode(
      common()->DeoptimizeIf(DeoptimizeReason::kDivisionByZero), check,
      frame_state, effect, control);

  // Perform the actual unsigned integer modulus.
  Node* value = graph()->NewNode(machine()->Uint32Mod(), lhs, rhs, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckedInt32Mul(Node* node, Node* frame_state,
                                              Node* effect, Node* control) {
  CheckForMinusZeroMode mode = CheckMinusZeroModeOf(node->op());
  Node* zero = jsgraph()->Int32Constant(0);
  Node* lhs = node->InputAt(0);
  Node* rhs = node->InputAt(1);

  Node* projection =
      graph()->NewNode(machine()->Int32MulWithOverflow(), lhs, rhs, control);

  Node* check = graph()->NewNode(common()->Projection(1), projection, control);
  control = effect =
      graph()->NewNode(common()->DeoptimizeIf(DeoptimizeReason::kOverflow),
                       check, frame_state, effect, control);

  Node* value = graph()->NewNode(common()->Projection(0), projection, control);

  if (mode == CheckForMinusZeroMode::kCheckForMinusZero) {
    Node* check_zero = graph()->NewNode(machine()->Word32Equal(), value, zero);
    Node* branch_zero = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                         check_zero, control);

    Node* if_zero = graph()->NewNode(common()->IfTrue(), branch_zero);
    Node* e_if_zero = effect;
    {
      // We may need to return negative zero.
      Node* or_inputs = graph()->NewNode(machine()->Word32Or(), lhs, rhs);
      Node* check_or =
          graph()->NewNode(machine()->Int32LessThan(), or_inputs, zero);
      if_zero = e_if_zero =
          graph()->NewNode(common()->DeoptimizeIf(DeoptimizeReason::kMinusZero),
                           check_or, frame_state, e_if_zero, if_zero);
    }

    Node* if_not_zero = graph()->NewNode(common()->IfFalse(), branch_zero);
    Node* e_if_not_zero = effect;

    control = graph()->NewNode(common()->Merge(2), if_zero, if_not_zero);
    effect = graph()->NewNode(common()->EffectPhi(2), e_if_zero, e_if_not_zero,
                              control);
  }

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckedUint32ToInt32(Node* node,
                                                   Node* frame_state,
                                                   Node* effect,
                                                   Node* control) {
  Node* value = node->InputAt(0);
  Node* max_int = jsgraph()->Int32Constant(std::numeric_limits<int32_t>::max());
  Node* is_safe =
      graph()->NewNode(machine()->Uint32LessThanOrEqual(), value, max_int);
  control = effect = graph()->NewNode(
      common()->DeoptimizeUnless(DeoptimizeReason::kLostPrecision), is_safe,
      frame_state, effect, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::BuildCheckedFloat64ToInt32(Node* value,
                                                    Node* frame_state,
                                                    Node* effect,
                                                    Node* control) {
  Node* value32 = graph()->NewNode(machine()->RoundFloat64ToInt32(), value);
  Node* check_same = graph()->NewNode(
      machine()->Float64Equal(), value,
      graph()->NewNode(machine()->ChangeInt32ToFloat64(), value32));
  control = effect = graph()->NewNode(
      common()->DeoptimizeUnless(DeoptimizeReason::kLostPrecisionOrNaN),
      check_same, frame_state, effect, control);

  // Check if {value} is -0.
  Node* check_zero = graph()->NewNode(machine()->Word32Equal(), value32,
                                      jsgraph()->Int32Constant(0));
  Node* branch_zero = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                       check_zero, control);

  Node* if_zero = graph()->NewNode(common()->IfTrue(), branch_zero);
  Node* if_notzero = graph()->NewNode(common()->IfFalse(), branch_zero);

  // In case of 0, we need to check the high bits for the IEEE -0 pattern.
  Node* check_negative = graph()->NewNode(
      machine()->Int32LessThan(),
      graph()->NewNode(machine()->Float64ExtractHighWord32(), value),
      jsgraph()->Int32Constant(0));

  Node* deopt_minus_zero =
      graph()->NewNode(common()->DeoptimizeIf(DeoptimizeReason::kMinusZero),
                       check_negative, frame_state, effect, if_zero);

  Node* merge =
      graph()->NewNode(common()->Merge(2), deopt_minus_zero, if_notzero);

  effect =
      graph()->NewNode(common()->EffectPhi(2), deopt_minus_zero, effect, merge);

  return ValueEffectControl(value32, effect, merge);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckedFloat64ToInt32(Node* node,
                                                    Node* frame_state,
                                                    Node* effect,
                                                    Node* control) {
  Node* value = node->InputAt(0);

  return BuildCheckedFloat64ToInt32(value, frame_state, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckedTaggedToInt32(Node* node,
                                                   Node* frame_state,
                                                   Node* effect,
                                                   Node* control) {
  Node* value = node->InputAt(0);

  Node* check = ObjectIsSmi(value);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);

  // In the Smi case, just convert to int32.
  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;
  Node* vtrue = ChangeSmiToInt32(value);

  // In the non-Smi case, check the heap numberness, load the number and convert
  // to int32.
  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* efalse = effect;
  Node* vfalse;
  {
    Node* value_map = efalse =
        graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                         value, efalse, if_false);
    Node* check = graph()->NewNode(machine()->WordEqual(), value_map,
                                   jsgraph()->HeapNumberMapConstant());
    if_false = efalse = graph()->NewNode(
        common()->DeoptimizeUnless(DeoptimizeReason::kNotAHeapNumber), check,
        frame_state, efalse, if_false);
    vfalse = efalse = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForHeapNumberValue()), value,
        efalse, if_false);
    ValueEffectControl state =
        BuildCheckedFloat64ToInt32(vfalse, frame_state, efalse, if_false);
    if_false = state.control;
    efalse = state.effect;
    vfalse = state.value;
  }

  control = graph()->NewNode(common()->Merge(2), if_true, if_false);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 2),
                           vtrue, vfalse, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::BuildCheckedHeapNumberOrOddballToFloat64(
    Node* value, Node* frame_state, Node* effect, Node* control) {
  Node* value_map = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForMap()), value, effect, control);
  Node* check_number = graph()->NewNode(machine()->WordEqual(), value_map,
                                        jsgraph()->HeapNumberMapConstant());

  Node* branch = graph()->NewNode(common()->Branch(BranchHint::kTrue),
                                  check_number, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  // For oddballs also contain the numeric value, let us just check that
  // we have an oddball here.
  Node* efalse = effect;
  Node* instance_type = efalse = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForMapInstanceType()), value_map,
      efalse, if_false);
  Node* check_oddball =
      graph()->NewNode(machine()->Word32Equal(), instance_type,
                       jsgraph()->Int32Constant(ODDBALL_TYPE));
  if_false = efalse =
      graph()->NewNode(common()->DeoptimizeUnless(
                           DeoptimizeReason::kNotAHeapNumberUndefinedBoolean),
                       check_oddball, frame_state, efalse, if_false);
  STATIC_ASSERT(HeapNumber::kValueOffset == Oddball::kToNumberRawOffset);

  control = graph()->NewNode(common()->Merge(2), if_true, if_false);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);

  Node* result = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForHeapNumberValue()), value,
      effect, control);
  return ValueEffectControl(result, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckedTaggedToFloat64(Node* node,
                                                     Node* frame_state,
                                                     Node* effect,
                                                     Node* control) {
  Node* value = node->InputAt(0);

  Node* check = ObjectIsSmi(value);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);

  // In the Smi case, just convert to int32 and then float64.
  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;
  Node* vtrue = ChangeSmiToInt32(value);
  vtrue = graph()->NewNode(machine()->ChangeInt32ToFloat64(), vtrue);

  // Otherwise, check heap numberness and load the number.
  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  ValueEffectControl number_state = BuildCheckedHeapNumberOrOddballToFloat64(
      value, frame_state, effect, if_false);

  Node* merge =
      graph()->NewNode(common()->Merge(2), if_true, number_state.control);
  Node* effect_phi = graph()->NewNode(common()->EffectPhi(2), etrue,
                                      number_state.effect, merge);
  Node* result =
      graph()->NewNode(common()->Phi(MachineRepresentation::kFloat64, 2), vtrue,
                       number_state.value, merge);

  return ValueEffectControl(result, effect_phi, merge);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerTruncateTaggedToWord32(Node* node, Node* effect,
                                                     Node* control) {
  Node* value = node->InputAt(0);

  Node* check = ObjectIsSmi(value);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;
  Node* vtrue = ChangeSmiToInt32(value);

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* efalse = effect;
  Node* vfalse;
  {
    STATIC_ASSERT(HeapNumber::kValueOffset == Oddball::kToNumberRawOffset);
    vfalse = efalse = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForHeapNumberValue()), value,
        efalse, if_false);
    vfalse = graph()->NewNode(machine()->TruncateFloat64ToWord32(), vfalse);
  }

  control = graph()->NewNode(common()->Merge(2), if_true, if_false);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 2),
                           vtrue, vfalse, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckedTruncateTaggedToWord32(Node* node,
                                                            Node* frame_state,
                                                            Node* effect,
                                                            Node* control) {
  Node* value = node->InputAt(0);

  Node* check = ObjectIsSmi(value);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);

  // In the Smi case, just convert to int32.
  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;
  Node* vtrue = ChangeSmiToInt32(value);

  // Otherwise, check that it's a heap number or oddball and truncate the value
  // to int32.
  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  ValueEffectControl false_state = BuildCheckedHeapNumberOrOddballToFloat64(
      value, frame_state, effect, if_false);
  false_state.value =
      graph()->NewNode(machine()->TruncateFloat64ToWord32(), false_state.value);

  Node* merge =
      graph()->NewNode(common()->Merge(2), if_true, false_state.control);
  Node* effect_phi = graph()->NewNode(common()->EffectPhi(2), etrue,
                                      false_state.effect, merge);
  Node* result =
      graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 2), vtrue,
                       false_state.value, merge);

  return ValueEffectControl(result, effect_phi, merge);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerObjectIsCallable(Node* node, Node* effect,
                                               Node* control) {
  Node* value = node->InputAt(0);

  Node* check = ObjectIsSmi(value);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kFalse), check, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;
  Node* vtrue = jsgraph()->Int32Constant(0);

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* efalse = effect;
  Node* vfalse;
  {
    Node* value_map = efalse =
        graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                         value, efalse, if_false);
    Node* value_bit_field = efalse = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForMapBitField()), value_map,
        efalse, if_false);
    vfalse = graph()->NewNode(
        machine()->Word32Equal(),
        jsgraph()->Int32Constant(1 << Map::kIsCallable),
        graph()->NewNode(
            machine()->Word32And(), value_bit_field,
            jsgraph()->Int32Constant((1 << Map::kIsCallable) |
                                     (1 << Map::kIsUndetectable))));
  }

  control = graph()->NewNode(common()->Merge(2), if_true, if_false);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kBit, 2), vtrue,
                           vfalse, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerObjectIsNumber(Node* node, Node* effect,
                                             Node* control) {
  Node* value = node->InputAt(0);

  Node* check = ObjectIsSmi(value);
  Node* branch = graph()->NewNode(common()->Branch(), check, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;
  Node* vtrue = jsgraph()->Int32Constant(1);

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* efalse = effect;
  Node* vfalse;
  {
    Node* value_map = efalse =
        graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                         value, efalse, if_false);
    vfalse = graph()->NewNode(machine()->WordEqual(), value_map,
                              jsgraph()->HeapNumberMapConstant());
  }

  control = graph()->NewNode(common()->Merge(2), if_true, if_false);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kBit, 2), vtrue,
                           vfalse, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerObjectIsReceiver(Node* node, Node* effect,
                                               Node* control) {
  Node* value = node->InputAt(0);

  Node* check = ObjectIsSmi(value);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kFalse), check, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;
  Node* vtrue = jsgraph()->Int32Constant(0);

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* efalse = effect;
  Node* vfalse;
  {
    STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
    Node* value_map = efalse =
        graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                         value, efalse, if_false);
    Node* value_instance_type = efalse = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForMapInstanceType()), value_map,
        efalse, if_false);
    vfalse = graph()->NewNode(machine()->Uint32LessThanOrEqual(),
                              jsgraph()->Uint32Constant(FIRST_JS_RECEIVER_TYPE),
                              value_instance_type);
  }

  control = graph()->NewNode(common()->Merge(2), if_true, if_false);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kBit, 2), vtrue,
                           vfalse, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerObjectIsSmi(Node* node, Node* effect,
                                          Node* control) {
  Node* value = node->InputAt(0);
  value = ObjectIsSmi(value);
  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerObjectIsString(Node* node, Node* effect,
                                             Node* control) {
  Node* value = node->InputAt(0);

  Node* check = ObjectIsSmi(value);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kFalse), check, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;
  Node* vtrue = jsgraph()->Int32Constant(0);

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* efalse = effect;
  Node* vfalse;
  {
    Node* value_map = efalse =
        graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                         value, efalse, if_false);
    Node* value_instance_type = efalse = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForMapInstanceType()), value_map,
        efalse, if_false);
    vfalse = graph()->NewNode(machine()->Uint32LessThan(), value_instance_type,
                              jsgraph()->Uint32Constant(FIRST_NONSTRING_TYPE));
  }

  control = graph()->NewNode(common()->Merge(2), if_true, if_false);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kBit, 2), vtrue,
                           vfalse, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerObjectIsUndetectable(Node* node, Node* effect,
                                                   Node* control) {
  Node* value = node->InputAt(0);

  Node* check = ObjectIsSmi(value);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kFalse), check, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;
  Node* vtrue = jsgraph()->Int32Constant(0);

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* efalse = effect;
  Node* vfalse;
  {
    Node* value_map = efalse =
        graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                         value, efalse, if_false);
    Node* value_bit_field = efalse = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForMapBitField()), value_map,
        efalse, if_false);
    vfalse = graph()->NewNode(
        machine()->Word32Equal(),
        graph()->NewNode(
            machine()->Word32Equal(), jsgraph()->Int32Constant(0),
            graph()->NewNode(
                machine()->Word32And(), value_bit_field,
                jsgraph()->Int32Constant(1 << Map::kIsUndetectable))),
        jsgraph()->Int32Constant(0));
  }

  control = graph()->NewNode(common()->Merge(2), if_true, if_false);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kBit, 2), vtrue,
                           vfalse, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerStringFromCharCode(Node* node, Node* effect,
                                                 Node* control) {
  Node* value = node->InputAt(0);

  // Compute the character code.
  Node* code =
      graph()->NewNode(machine()->Word32And(), value,
                       jsgraph()->Int32Constant(String::kMaxUtf16CodeUnit));

  // Check if the {code} is a one-byte char code.
  Node* check0 =
      graph()->NewNode(machine()->Int32LessThanOrEqual(), code,
                       jsgraph()->Int32Constant(String::kMaxOneByteCharCode));
  Node* branch0 =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check0, control);

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* etrue0 = effect;
  Node* vtrue0;
  {
    // Load the isolate wide single character string cache.
    Node* cache =
        jsgraph()->HeapConstant(factory()->single_character_string_cache());

    // Compute the {cache} index for {code}.
    Node* index =
        machine()->Is32() ? code : graph()->NewNode(
                                       machine()->ChangeUint32ToUint64(), code);

    // Check if we have an entry for the {code} in the single character string
    // cache already.
    Node* entry = etrue0 = graph()->NewNode(
        simplified()->LoadElement(AccessBuilder::ForFixedArrayElement()), cache,
        index, etrue0, if_true0);

    Node* check1 = graph()->NewNode(machine()->WordEqual(), entry,
                                    jsgraph()->UndefinedConstant());
    Node* branch1 = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                     check1, if_true0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* etrue1 = etrue0;
    Node* vtrue1;
    {
      // Allocate a new SeqOneByteString for {code}.
      vtrue1 = etrue1 = graph()->NewNode(
          simplified()->Allocate(NOT_TENURED),
          jsgraph()->Int32Constant(SeqOneByteString::SizeFor(1)), etrue1,
          if_true1);
      etrue1 = graph()->NewNode(
          simplified()->StoreField(AccessBuilder::ForMap()), vtrue1,
          jsgraph()->HeapConstant(factory()->one_byte_string_map()), etrue1,
          if_true1);
      etrue1 = graph()->NewNode(
          simplified()->StoreField(AccessBuilder::ForNameHashField()), vtrue1,
          jsgraph()->IntPtrConstant(Name::kEmptyHashField), etrue1, if_true1);
      etrue1 = graph()->NewNode(
          simplified()->StoreField(AccessBuilder::ForStringLength()), vtrue1,
          jsgraph()->SmiConstant(1), etrue1, if_true1);
      etrue1 = graph()->NewNode(
          machine()->Store(StoreRepresentation(MachineRepresentation::kWord8,
                                               kNoWriteBarrier)),
          vtrue1, jsgraph()->IntPtrConstant(SeqOneByteString::kHeaderSize -
                                            kHeapObjectTag),
          code, etrue1, if_true1);

      // Remember it in the {cache}.
      etrue1 = graph()->NewNode(
          simplified()->StoreElement(AccessBuilder::ForFixedArrayElement()),
          cache, index, vtrue1, etrue1, if_true1);
    }

    // Use the {entry} from the {cache}.
    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* efalse1 = etrue0;
    Node* vfalse1 = entry;

    if_true0 = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
    etrue0 =
        graph()->NewNode(common()->EffectPhi(2), etrue1, efalse1, if_true0);
    vtrue0 = graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                              vtrue1, vfalse1, if_true0);
  }

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* efalse0 = effect;
  Node* vfalse0;
  {
    // Allocate a new SeqTwoByteString for {code}.
    vfalse0 = efalse0 =
        graph()->NewNode(simplified()->Allocate(NOT_TENURED),
                         jsgraph()->Int32Constant(SeqTwoByteString::SizeFor(1)),
                         efalse0, if_false0);
    efalse0 = graph()->NewNode(
        simplified()->StoreField(AccessBuilder::ForMap()), vfalse0,
        jsgraph()->HeapConstant(factory()->string_map()), efalse0, if_false0);
    efalse0 = graph()->NewNode(
        simplified()->StoreField(AccessBuilder::ForNameHashField()), vfalse0,
        jsgraph()->IntPtrConstant(Name::kEmptyHashField), efalse0, if_false0);
    efalse0 = graph()->NewNode(
        simplified()->StoreField(AccessBuilder::ForStringLength()), vfalse0,
        jsgraph()->SmiConstant(1), efalse0, if_false0);
    efalse0 = graph()->NewNode(
        machine()->Store(StoreRepresentation(MachineRepresentation::kWord16,
                                             kNoWriteBarrier)),
        vfalse0, jsgraph()->IntPtrConstant(SeqTwoByteString::kHeaderSize -
                                           kHeapObjectTag),
        code, efalse0, if_false0);
  }

  control = graph()->NewNode(common()->Merge(2), if_true0, if_false0);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue0, efalse0, control);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                           vtrue0, vfalse0, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckFloat64Hole(Node* node, Node* frame_state,
                                               Node* effect, Node* control) {
  // If we reach this point w/o eliminating the {node} that's marked
  // with allow-return-hole, we cannot do anything, so just deoptimize
  // in case of the hole NaN (similar to Crankshaft).
  Node* value = node->InputAt(0);
  Node* check = graph()->NewNode(
      machine()->Word32Equal(),
      graph()->NewNode(machine()->Float64ExtractHighWord32(), value),
      jsgraph()->Int32Constant(kHoleNanUpper32));
  control = effect =
      graph()->NewNode(common()->DeoptimizeIf(DeoptimizeReason::kHole), check,
                       frame_state, effect, control);

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerCheckTaggedHole(Node* node, Node* frame_state,
                                              Node* effect, Node* control) {
  CheckTaggedHoleMode mode = CheckTaggedHoleModeOf(node->op());
  Node* value = node->InputAt(0);
  Node* check = graph()->NewNode(machine()->WordEqual(), value,
                                 jsgraph()->TheHoleConstant());
  switch (mode) {
    case CheckTaggedHoleMode::kConvertHoleToUndefined:
      value = graph()->NewNode(
          common()->Select(MachineRepresentation::kTagged, BranchHint::kFalse),
          check, jsgraph()->UndefinedConstant(), value);
      break;
    case CheckTaggedHoleMode::kNeverReturnHole:
      control = effect =
          graph()->NewNode(common()->DeoptimizeIf(DeoptimizeReason::kHole),
                           check, frame_state, effect, control);
      break;
  }

  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::AllocateHeapNumberWithValue(Node* value, Node* effect,
                                                     Node* control) {
  Node* result = effect = graph()->NewNode(
      simplified()->Allocate(NOT_TENURED),
      jsgraph()->Int32Constant(HeapNumber::kSize), effect, control);
  effect = graph()->NewNode(simplified()->StoreField(AccessBuilder::ForMap()),
                            result, jsgraph()->HeapNumberMapConstant(), effect,
                            control);
  effect = graph()->NewNode(
      simplified()->StoreField(AccessBuilder::ForHeapNumberValue()), result,
      value, effect, control);
  return ValueEffectControl(result, effect, control);
}

Node* EffectControlLinearizer::ChangeInt32ToSmi(Node* value) {
  if (machine()->Is64()) {
    value = graph()->NewNode(machine()->ChangeInt32ToInt64(), value);
  }
  return graph()->NewNode(machine()->WordShl(), value, SmiShiftBitsConstant());
}

Node* EffectControlLinearizer::ChangeUint32ToSmi(Node* value) {
  if (machine()->Is64()) {
    value = graph()->NewNode(machine()->ChangeUint32ToUint64(), value);
  }
  return graph()->NewNode(machine()->WordShl(), value, SmiShiftBitsConstant());
}

Node* EffectControlLinearizer::ChangeInt32ToFloat64(Node* value) {
  return graph()->NewNode(machine()->ChangeInt32ToFloat64(), value);
}

Node* EffectControlLinearizer::ChangeUint32ToFloat64(Node* value) {
  return graph()->NewNode(machine()->ChangeUint32ToFloat64(), value);
}

Node* EffectControlLinearizer::ChangeSmiToInt32(Node* value) {
  value = graph()->NewNode(machine()->WordSar(), value, SmiShiftBitsConstant());
  if (machine()->Is64()) {
    value = graph()->NewNode(machine()->TruncateInt64ToInt32(), value);
  }
  return value;
}
Node* EffectControlLinearizer::ObjectIsSmi(Node* value) {
  return graph()->NewNode(
      machine()->WordEqual(),
      graph()->NewNode(machine()->WordAnd(), value,
                       jsgraph()->IntPtrConstant(kSmiTagMask)),
      jsgraph()->IntPtrConstant(kSmiTag));
}

Node* EffectControlLinearizer::SmiMaxValueConstant() {
  return jsgraph()->Int32Constant(Smi::kMaxValue);
}

Node* EffectControlLinearizer::SmiShiftBitsConstant() {
  return jsgraph()->IntPtrConstant(kSmiShiftSize + kSmiTagSize);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerPlainPrimitiveToNumber(Node* node, Node* effect,
                                                     Node* control) {
  Node* value = node->InputAt(0);
  Node* result = effect =
      graph()->NewNode(ToNumberOperator(), jsgraph()->ToNumberBuiltinConstant(),
                       value, jsgraph()->NoContextConstant(), effect, control);
  return ValueEffectControl(result, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerPlainPrimitiveToWord32(Node* node, Node* effect,
                                                     Node* control) {
  Node* value = node->InputAt(0);

  Node* check0 = ObjectIsSmi(value);
  Node* branch0 =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check0, control);

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* etrue0 = effect;
  Node* vtrue0 = ChangeSmiToInt32(value);

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* efalse0 = effect;
  Node* vfalse0;
  {
    vfalse0 = efalse0 = graph()->NewNode(
        ToNumberOperator(), jsgraph()->ToNumberBuiltinConstant(), value,
        jsgraph()->NoContextConstant(), efalse0, if_false0);

    Node* check1 = ObjectIsSmi(vfalse0);
    Node* branch1 = graph()->NewNode(common()->Branch(), check1, if_false0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* etrue1 = efalse0;
    Node* vtrue1 = ChangeSmiToInt32(vfalse0);

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* efalse1 = efalse0;
    Node* vfalse1;
    {
      vfalse1 = efalse1 = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForHeapNumberValue()), efalse0,
          efalse1, if_false1);
      vfalse1 = graph()->NewNode(machine()->TruncateFloat64ToWord32(), vfalse1);
    }

    if_false0 = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
    efalse0 =
        graph()->NewNode(common()->EffectPhi(2), etrue1, efalse1, if_false0);
    vfalse0 = graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 2),
                               vtrue1, vfalse1, if_false0);
  }

  control = graph()->NewNode(common()->Merge(2), if_true0, if_false0);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue0, efalse0, control);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 2),
                           vtrue0, vfalse0, control);
  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerPlainPrimitiveToFloat64(Node* node, Node* effect,
                                                      Node* control) {
  Node* value = node->InputAt(0);

  Node* check0 = ObjectIsSmi(value);
  Node* branch0 =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check0, control);

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* etrue0 = effect;
  Node* vtrue0;
  {
    vtrue0 = ChangeSmiToInt32(value);
    vtrue0 = graph()->NewNode(machine()->ChangeInt32ToFloat64(), vtrue0);
  }

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* efalse0 = effect;
  Node* vfalse0;
  {
    vfalse0 = efalse0 = graph()->NewNode(
        ToNumberOperator(), jsgraph()->ToNumberBuiltinConstant(), value,
        jsgraph()->NoContextConstant(), efalse0, if_false0);

    Node* check1 = ObjectIsSmi(vfalse0);
    Node* branch1 = graph()->NewNode(common()->Branch(), check1, if_false0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* etrue1 = efalse0;
    Node* vtrue1;
    {
      vtrue1 = ChangeSmiToInt32(vfalse0);
      vtrue1 = graph()->NewNode(machine()->ChangeInt32ToFloat64(), vtrue1);
    }

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* efalse1 = efalse0;
    Node* vfalse1;
    {
      vfalse1 = efalse1 = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForHeapNumberValue()), efalse0,
          efalse1, if_false1);
    }

    if_false0 = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
    efalse0 =
        graph()->NewNode(common()->EffectPhi(2), etrue1, efalse1, if_false0);
    vfalse0 =
        graph()->NewNode(common()->Phi(MachineRepresentation::kFloat64, 2),
                         vtrue1, vfalse1, if_false0);
  }

  control = graph()->NewNode(common()->Merge(2), if_true0, if_false0);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue0, efalse0, control);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kFloat64, 2),
                           vtrue0, vfalse0, control);
  return ValueEffectControl(value, effect, control);
}

EffectControlLinearizer::ValueEffectControl
EffectControlLinearizer::LowerTransitionElementsKind(Node* node, Node* effect,
                                                     Node* control) {
  ElementsTransition const transition = ElementsTransitionOf(node->op());
  Node* object = node->InputAt(0);
  Node* source_map = node->InputAt(1);
  Node* target_map = node->InputAt(2);

  // Load the current map of {object}.
  Node* object_map = effect =
      graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()), object,
                       effect, control);

  // Check if {object_map} is the same as {source_map}.
  Node* check =
      graph()->NewNode(machine()->WordEqual(), object_map, source_map);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kFalse), check, control);

  // Migrate the {object} from {source_map} to {target_map}.
  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;
  {
    switch (transition) {
      case ElementsTransition::kFastTransition: {
        // In-place migration of {object}, just store the {target_map}.
        etrue =
            graph()->NewNode(simplified()->StoreField(AccessBuilder::ForMap()),
                             object, target_map, etrue, if_true);
        break;
      }
      case ElementsTransition::kSlowTransition: {
        // Instance migration, call out to the runtime for {object}.
        Operator::Properties properties =
            Operator::kNoDeopt | Operator::kNoThrow;
        Runtime::FunctionId id = Runtime::kTransitionElementsKind;
        CallDescriptor const* desc = Linkage::GetRuntimeCallDescriptor(
            graph()->zone(), id, 2, properties, CallDescriptor::kNoFlags);
        etrue = graph()->NewNode(
            common()->Call(desc), jsgraph()->CEntryStubConstant(1), object,
            target_map,
            jsgraph()->ExternalConstant(ExternalReference(id, isolate())),
            jsgraph()->Int32Constant(2), jsgraph()->NoContextConstant(), etrue,
            if_true);
        break;
      }
    }
  }

  // Nothing to do if the {object} doesn't have the {source_map}.
  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* efalse = effect;

  control = graph()->NewNode(common()->Merge(2), if_true, if_false);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);

  return ValueEffectControl(nullptr, effect, control);
}

Factory* EffectControlLinearizer::factory() const {
  return isolate()->factory();
}

Isolate* EffectControlLinearizer::isolate() const {
  return jsgraph()->isolate();
}

Operator const* EffectControlLinearizer::ToNumberOperator() {
  if (!to_number_operator_.is_set()) {
    Callable callable = CodeFactory::ToNumber(isolate());
    CallDescriptor::Flags flags = CallDescriptor::kNoFlags;
    CallDescriptor* desc = Linkage::GetStubCallDescriptor(
        isolate(), graph()->zone(), callable.descriptor(), 0, flags,
        Operator::kNoThrow);
    to_number_operator_.set(common()->Call(desc));
  }
  return to_number_operator_.get();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
