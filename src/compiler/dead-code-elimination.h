// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_DEAD_CODE_ELIMINATION_H_
#define V8_COMPILER_DEAD_CODE_ELIMINATION_H_

#include "src/base/compiler-specific.h"
#include "src/compiler/graph-reducer.h"
#include "src/globals.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;

// Propagates {Dead} control and {DeadValue} values through the graph and
// thereby removes dead code. When {DeadValue} hits the effect chain, a crashing
// {Unreachable} node is inserted and the rest of the effect chain is collapsed.
// We detect dead values based on types, pruning uses of DeadValue except for
// uses by phi and control nodes without effect input. These remaining uses of
// {DeadValue} are eliminated in the {EffectControlLinearizer}, when the effect
// chain is readily available to insert an {Unreachable} node for safety. In
// contrast to this, {Dead} must not remain in the graph.
class V8_EXPORT_PRIVATE DeadCodeElimination final
    : public NON_EXPORTED_BASE(AdvancedReducer) {
 public:
  DeadCodeElimination(Editor* editor, Graph* graph,
                      CommonOperatorBuilder* common);
  ~DeadCodeElimination() final {}

  const char* reducer_name() const override { return "DeadCodeElimination"; }

  Reduction Reduce(Node* node) final;

 private:
  Reduction ReduceEnd(Node* node);
  Reduction ReduceLoopOrMerge(Node* node);
  Reduction ReduceLoopExit(Node* node);
  Reduction ReduceNode(Node* node);
  Reduction ReducePureNode(Node* node);
  Reduction ReduceEffectNode(Node* node);

  Reduction RemoveLoopExit(Node* node);

  void TrimMergeOrPhi(Node* node, int size);

  Graph* graph() const { return graph_; }
  CommonOperatorBuilder* common() const { return common_; }
  Node* dead() const { return dead_; }
  Node* dead_value() const { return dead_value_; }

  Graph* const graph_;
  CommonOperatorBuilder* const common_;
  Node* const dead_;
  Node* const dead_value_;

  DISALLOW_COPY_AND_ASSIGN(DeadCodeElimination);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_DEAD_CODE_ELIMINATION_H_
