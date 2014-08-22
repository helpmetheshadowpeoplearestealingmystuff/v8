// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_SIMPLIFIED_OPERATOR_REDUCER_H_
#define V8_COMPILER_SIMPLIFIED_OPERATOR_REDUCER_H_

#include "src/compiler/graph-reducer.h"

namespace v8 {
namespace internal {

// Forward declarations.
class Heap;

namespace compiler {

// Forward declarations.
class JSGraph;
class MachineOperatorBuilder;

class SimplifiedOperatorReducer V8_FINAL : public Reducer {
 public:
  SimplifiedOperatorReducer(JSGraph* jsgraph, MachineOperatorBuilder* machine)
      : jsgraph_(jsgraph), machine_(machine) {}
  virtual ~SimplifiedOperatorReducer();

  virtual Reduction Reduce(Node* node) V8_OVERRIDE;

 private:
  Reduction Change(Node* node, Operator* op, Node* a);
  Reduction ReplaceFloat64(double value);
  Reduction ReplaceInt32(int32_t value);
  Reduction ReplaceUint32(uint32_t value) {
    return ReplaceInt32(static_cast<int32_t>(value));
  }
  Reduction ReplaceNumber(double value);
  Reduction ReplaceNumber(int32_t value);

  Graph* graph() const;
  Heap* heap() const;
  JSGraph* jsgraph() const { return jsgraph_; }
  MachineOperatorBuilder* machine() const { return machine_; }

  JSGraph* jsgraph_;
  MachineOperatorBuilder* machine_;

  DISALLOW_COPY_AND_ASSIGN(SimplifiedOperatorReducer);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_SIMPLIFIED_OPERATOR_REDUCER_H_
