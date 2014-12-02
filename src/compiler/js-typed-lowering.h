// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_TYPED_LOWERING_H_
#define V8_COMPILER_JS_TYPED_LOWERING_H_

#include "src/compiler/graph-reducer.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node.h"
#include "src/compiler/simplified-operator.h"

namespace v8 {
namespace internal {
namespace compiler {

// Lowers JS-level operators to simplified operators based on types.
class JSTypedLowering FINAL : public Reducer {
 public:
  explicit JSTypedLowering(JSGraph* jsgraph);
  ~JSTypedLowering() {}

  Reduction Reduce(Node* node) OVERRIDE;

  JSGraph* jsgraph() { return jsgraph_; }
  Graph* graph() { return jsgraph_->graph(); }
  Zone* zone() { return jsgraph_->zone(); }

 private:
  friend class JSBinopReduction;

  Reduction ReplaceEagerly(Node* old, Node* node);
  Reduction ReplaceWith(Node* node) { return Reducer::Replace(node); }
  Reduction ReduceJSAdd(Node* node);
  Reduction ReduceJSBitwiseOr(Node* node);
  Reduction ReduceJSMultiply(Node* node);
  Reduction ReduceJSComparison(Node* node);
  Reduction ReduceJSLoadProperty(Node* node);
  Reduction ReduceJSStoreProperty(Node* node);
  Reduction ReduceJSEqual(Node* node, bool invert);
  Reduction ReduceJSStrictEqual(Node* node, bool invert);
  Reduction ReduceJSToNumberInput(Node* input);
  Reduction ReduceJSToNumber(Node* node);
  Reduction ReduceJSToStringInput(Node* input);
  Reduction ReduceJSToBooleanInput(Node* input);
  Reduction ReduceJSToBoolean(Node* node);
  Reduction ReduceNumberBinop(Node* node, const Operator* numberOp);
  Reduction ReduceI32Binop(Node* node, bool left_signed, bool right_signed,
                           const Operator* intOp);
  Reduction ReduceI32Shift(Node* node, bool left_signed,
                           const Operator* shift_op);

  Node* Word32Shl(Node* const lhs, int32_t const rhs);

  JSOperatorBuilder* javascript() { return jsgraph_->javascript(); }
  CommonOperatorBuilder* common() { return jsgraph_->common(); }
  SimplifiedOperatorBuilder* simplified() { return &simplified_; }
  MachineOperatorBuilder* machine() { return jsgraph_->machine(); }

  JSGraph* jsgraph_;
  SimplifiedOperatorBuilder simplified_;
  Type* zero_range_;
  Type* one_range_;
  Type* zero_thirtyone_range_;
  Type* shifted_int32_ranges_[4];
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_TYPED_LOWERING_H_
