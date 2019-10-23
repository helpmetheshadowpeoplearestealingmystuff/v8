// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/decompression-optimizer.h"

#include "src/compiler/graph.h"
#include "src/compiler/node-properties.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

bool IsMachineLoad(Node* const node) {
  const IrOpcode::Value opcode = node->opcode();
  return opcode == IrOpcode::kLoad || opcode == IrOpcode::kPoisonedLoad ||
         opcode == IrOpcode::kProtectedLoad ||
         opcode == IrOpcode::kUnalignedLoad;
}

bool IsHeapConstant(Node* const node) {
  return node->opcode() == IrOpcode::kHeapConstant;
}

bool CanBeCompressed(Node* const node) {
  return IsHeapConstant(node) ||
         (IsMachineLoad(node) &&
          CanBeTaggedPointer(
              LoadRepresentationOf(node->op()).representation()));
}

}  // anonymous namespace

DecompressionOptimizer::DecompressionOptimizer(Zone* zone, Graph* graph,
                                               CommonOperatorBuilder* common,
                                               MachineOperatorBuilder* machine)
    : graph_(graph),
      common_(common),
      machine_(machine),
      states_(graph, static_cast<uint32_t>(State::kNumberOfStates)),
      to_visit_(zone),
      compressed_candidate_nodes_(zone) {}

void DecompressionOptimizer::MarkNodes() {
  MaybeMarkAndQueueForRevisit(graph()->end(), State::kOnly32BitsObserved);
  while (!to_visit_.empty()) {
    Node* const node = to_visit_.front();
    to_visit_.pop_front();
    MarkNodeInputs(node);
  }
}

void DecompressionOptimizer::MarkNodeInputs(Node* node) {
  // Mark the value inputs.
  switch (node->opcode()) {
    // TODO(v8:7703): To be removed when the TaggedEqual implementation stops
    // using ChangeTaggedToCompressed.
    case IrOpcode::kChangeTaggedToCompressed:
      DCHECK_EQ(node->op()->ValueInputCount(), 1);
      MaybeMarkAndQueueForRevisit(node->InputAt(0),
                                  State::kOnly32BitsObserved);  // value
      break;
    case IrOpcode::kWord32Equal:
      DCHECK_EQ(node->op()->ValueInputCount(), 2);
      MaybeMarkAndQueueForRevisit(node->InputAt(0),
                                  State::kOnly32BitsObserved);  // value_0
      MaybeMarkAndQueueForRevisit(node->InputAt(1),
                                  State::kOnly32BitsObserved);  // value_1
      break;
    case IrOpcode::kStore:           // Fall through.
    case IrOpcode::kProtectedStore:  // Fall through.
    case IrOpcode::kUnalignedStore:
      DCHECK_EQ(node->op()->ValueInputCount(), 3);
      MaybeMarkAndQueueForRevisit(node->InputAt(0),
                                  State::kEverythingObserved);  // base pointer
      MaybeMarkAndQueueForRevisit(node->InputAt(1),
                                  State::kEverythingObserved);  // index
      // TODO(v8:7703): When the implementation is done, check if this ternary
      // operator is too restrictive, since we only mark Tagged stores as 32
      // bits.
      MaybeMarkAndQueueForRevisit(
          node->InputAt(2),
          IsAnyTagged(StoreRepresentationOf(node->op()).representation())
              ? State::kOnly32BitsObserved
              : State::kEverythingObserved);  // value
      break;
    default:
      // To be conservative, we assume that all value inputs need to be 64 bits
      // unless noted otherwise.
      for (int i = 0; i < node->op()->ValueInputCount(); ++i) {
        MaybeMarkAndQueueForRevisit(node->InputAt(i),
                                    State::kEverythingObserved);
      }
      break;
  }

  // We always mark the non-value input nodes as kOnly32BitsObserved so that
  // they will be visited. If they need to be kEverythingObserved, they will be
  // marked as such in a future pass.
  for (int i = node->op()->ValueInputCount(); i < node->InputCount(); ++i) {
    MaybeMarkAndQueueForRevisit(node->InputAt(i), State::kOnly32BitsObserved);
  }
}

void DecompressionOptimizer::MaybeMarkAndQueueForRevisit(Node* const node,
                                                         State state) {
  DCHECK_NE(state, State::kUnvisited);
  State previous_state = states_.Get(node);
  // Only update the state if we have relevant new information.
  if (previous_state == State::kUnvisited ||
      (previous_state == State::kOnly32BitsObserved &&
       state == State::kEverythingObserved)) {
    states_.Set(node, state);
    to_visit_.push_back(node);

    if (state == State::kOnly32BitsObserved && CanBeCompressed(node)) {
      compressed_candidate_nodes_.push_back(node);
    }
  }
}

void DecompressionOptimizer::ChangeHeapConstant(Node* const node) {
  DCHECK(IsHeapConstant(node));
  NodeProperties::ChangeOp(
      node, common()->CompressedHeapConstant(HeapConstantOf(node->op())));
}

void DecompressionOptimizer::ChangeLoad(Node* const node) {
  DCHECK(IsMachineLoad(node));
  // Change to a Compressed MachRep to avoid the full decompression.
  LoadRepresentation load_rep = LoadRepresentationOf(node->op());
  LoadRepresentation compressed_load_rep;
  if (load_rep == MachineType::AnyTagged()) {
    compressed_load_rep = MachineType::AnyCompressed();
  } else {
    DCHECK_EQ(load_rep, MachineType::TaggedPointer());
    compressed_load_rep = MachineType::CompressedPointer();
  }

  // Change to the Operator with the Compressed MachineRepresentation.
  switch (node->opcode()) {
    case IrOpcode::kLoad:
      NodeProperties::ChangeOp(node, machine()->Load(compressed_load_rep));
      break;
    case IrOpcode::kPoisonedLoad:
      NodeProperties::ChangeOp(node,
                               machine()->PoisonedLoad(compressed_load_rep));
      break;
    case IrOpcode::kProtectedLoad:
      NodeProperties::ChangeOp(node,
                               machine()->ProtectedLoad(compressed_load_rep));
      break;
    case IrOpcode::kUnalignedLoad:
      NodeProperties::ChangeOp(node,
                               machine()->UnalignedLoad(compressed_load_rep));
      break;
    default:
      UNREACHABLE();
  }
}

void DecompressionOptimizer::ChangeNodes() {
  for (Node* const node : compressed_candidate_nodes_) {
    // compressed_candidate_nodes_ contains all the nodes that once had the
    // State::kOnly32BitsObserved. If we later updated the state to be
    // State::IsEverythingObserved, then we have to ignore them. This is less
    // costly than removing them from the compressed_candidate_nodes_ NodeVector
    // when we update them to State::IsEverythingObserved.
    if (IsEverythingObserved(node)) continue;

    if (IsHeapConstant(node)) {
      ChangeHeapConstant(node);
    } else {
      ChangeLoad(node);
    }
  }
}

void DecompressionOptimizer::Reduce() {
  MarkNodes();
  ChangeNodes();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
