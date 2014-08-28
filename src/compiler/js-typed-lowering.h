// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_OPERATOR_REDUCERS_H_
#define V8_COMPILER_OPERATOR_REDUCERS_H_

#include "src/compiler/graph-reducer.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node.h"
#include "src/compiler/simplified-operator.h"

namespace v8 {
namespace internal {
namespace compiler {

// Lowers JS-level operators to simplified operators based on types.
class JSTypedLowering : public Reducer {
 public:
  explicit JSTypedLowering(JSGraph* jsgraph)
      : jsgraph_(jsgraph),
        simplified_(jsgraph->zone()),
        machine_(jsgraph->zone()) {}
  virtual ~JSTypedLowering() {}

  virtual Reduction Reduce(Node* node);

  JSGraph* jsgraph() { return jsgraph_; }
  Graph* graph() { return jsgraph_->graph(); }

 private:
  friend class JSBinopReduction;
  JSGraph* jsgraph_;
  SimplifiedOperatorBuilder simplified_;
  MachineOperatorBuilder machine_;

  Reduction ReplaceEagerly(Node* old, Node* node);
  Reduction ReplaceWith(Node* node) { return Reducer::Replace(node); }
  Reduction ReduceJSAdd(Node* node);
  Reduction ReduceJSComparison(Node* node);
  Reduction ReduceJSPropertyLoad(Node* node);
  Reduction ReduceJSEqual(Node* node, bool invert);
  Reduction ReduceJSStrictEqual(Node* node, bool invert);
  Reduction ReduceJSToNumberInput(Node* input);
  Reduction ReduceJSToStringInput(Node* input);
  Reduction ReduceJSToBooleanInput(Node* input);
  Reduction ReduceNumberBinop(Node* node, Operator* numberOp);
  Reduction ReduceI32Binop(Node* node, bool left_signed, bool right_signed,
                           Operator* intOp);
  Reduction ReduceI32Shift(Node* node, bool left_signed, Operator* shift_op);

  JSOperatorBuilder* javascript() { return jsgraph_->javascript(); }
  CommonOperatorBuilder* common() { return jsgraph_->common(); }
  SimplifiedOperatorBuilder* simplified() { return &simplified_; }
  MachineOperatorBuilder* machine() { return &machine_; }
};
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_OPERATOR_REDUCERS_H_
