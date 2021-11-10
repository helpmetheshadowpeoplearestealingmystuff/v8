// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/wasm-escape-analysis.h"

#include "src/compiler/js-graph.h"
#include "src/compiler/node-properties.h"

namespace v8 {
namespace internal {
namespace compiler {

Reduction WasmEscapeAnalysis::Reduce(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kAllocateRaw:
      return ReduceAllocateRaw(node);
    default:
      return NoChange();
  }
}

Reduction WasmEscapeAnalysis::ReduceAllocateRaw(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kAllocateRaw);
  // TODO(manoskouk): Account for phis.

  // Collect all value edges of {node} in this vector.
  std::vector<Edge> value_edges;
  for (Edge edge : node->use_edges()) {
    if (NodeProperties::IsValueEdge(edge)) {
      if (edge.index() != 0 ||
          edge.from()->opcode() != IrOpcode::kStoreToObject) {
        // The allocated object is used for something other than storing into.
        return NoChange();
      }
      value_edges.push_back(edge);
    }
  }

  // Remove all discovered stores from the effect chain.
  for (Edge edge : value_edges) {
    DCHECK(NodeProperties::IsValueEdge(edge));
    DCHECK_EQ(edge.index(), 0);
    Node* use = edge.from();
    DCHECK(!use->IsDead());
    DCHECK_EQ(use->opcode(), IrOpcode::kStoreToObject);
    ReplaceWithValue(use, mcgraph_->Dead(), NodeProperties::GetEffectInput(use),
                     mcgraph_->Dead());
    use->Kill();
  }

  // Remove the allocation from the effect and control chains.
  ReplaceWithValue(node, mcgraph_->Dead(), NodeProperties::GetEffectInput(node),
                   NodeProperties::GetControlInput(node));

  return Changed(node);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
