// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/type-narrowing-reducer.h"

#include "src/compiler/js-graph.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

TypeNarrowingReducer::TypeNarrowingReducer(Editor* editor, JSGraph* jsgraph)
    : AdvancedReducer(editor),
      jsgraph_(jsgraph),
      op_typer_(jsgraph->isolate(), zone()) {}

TypeNarrowingReducer::~TypeNarrowingReducer() {}

Reduction TypeNarrowingReducer::Reduce(Node* node) {
  Type new_type = Type::Any();

  switch (node->opcode()) {
    case IrOpcode::kNumberLessThan: {
      // TODO(turbofan) Reuse the logic from typer.cc (by integrating relational
      // comparisons with the operation typer).
      Type left_type = NodeProperties::GetType(node->InputAt(0));
      Type right_type = NodeProperties::GetType(node->InputAt(1));
      if (left_type.Is(Type::PlainNumber()) &&
          right_type.Is(Type::PlainNumber())) {
        Factory* const factory = jsgraph()->isolate()->factory();
        if (left_type.Max() < right_type.Min()) {
          new_type = Type::HeapConstant(jsgraph()->isolate(),
                                        factory->true_value(), zone());
        } else if (left_type.Min() >= right_type.Max()) {
          new_type = Type::HeapConstant(jsgraph()->isolate(),
                                        factory->false_value(), zone());
        }
      }
      break;
    }

    case IrOpcode::kTypeGuard: {
      new_type = op_typer_.TypeTypeGuard(
          node->op(), NodeProperties::GetType(node->InputAt(0)));
      break;
    }

#define DECLARE_CASE(Name)                                                \
  case IrOpcode::k##Name: {                                               \
    new_type = op_typer_.Name(NodeProperties::GetType(node->InputAt(0)),  \
                              NodeProperties::GetType(node->InputAt(1))); \
    break;                                                                \
  }
      SIMPLIFIED_NUMBER_BINOP_LIST(DECLARE_CASE)
      DECLARE_CASE(SameValue)
#undef DECLARE_CASE

#define DECLARE_CASE(Name)                                                \
  case IrOpcode::k##Name: {                                               \
    new_type = op_typer_.Name(NodeProperties::GetType(node->InputAt(0))); \
    break;                                                                \
  }
      SIMPLIFIED_NUMBER_UNOP_LIST(DECLARE_CASE)
      DECLARE_CASE(ToBoolean)
#undef DECLARE_CASE

    default:
      return NoChange();
  }

  Type original_type = NodeProperties::GetType(node);
  Type restricted = Type::Intersect(new_type, original_type, zone());
  if (!original_type.Is(restricted)) {
    NodeProperties::SetType(node, restricted);
    return Changed(node);
  }
  return NoChange();
}

Graph* TypeNarrowingReducer::graph() const { return jsgraph()->graph(); }

Zone* TypeNarrowingReducer::zone() const { return graph()->zone(); }

}  // namespace compiler
}  // namespace internal
}  // namespace v8
