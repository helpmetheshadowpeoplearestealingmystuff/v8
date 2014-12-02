// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/access-builder.h"
#include "src/compiler/graph-inl.h"
#include "src/compiler/js-builtin-reducer.h"
#include "src/compiler/js-typed-lowering.h"
#include "src/compiler/node-aux-data-inl.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties-inl.h"
#include "src/types.h"

namespace v8 {
namespace internal {
namespace compiler {

// TODO(turbofan): js-typed-lowering improvements possible
// - immediately put in type bounds for all new nodes
// - relax effects from generic but not-side-effecting operations
// - relax effects for ToNumber(mixed)


// Relax the effects of {node} by immediately replacing effect uses of {node}
// with the effect input to {node}.
// TODO(turbofan): replace the effect input to {node} with {graph->start()}.
// TODO(titzer): move into a GraphEditor?
static void RelaxEffects(Node* node) {
  NodeProperties::ReplaceWithValue(node, node, NULL);
}


JSTypedLowering::JSTypedLowering(JSGraph* jsgraph)
    : jsgraph_(jsgraph), simplified_(jsgraph->zone()) {
  Factory* factory = zone()->isolate()->factory();
  Handle<Object> zero = factory->NewNumber(0.0);
  Handle<Object> one = factory->NewNumber(1.0);
  zero_range_ = Type::Range(zero, zero, zone());
  one_range_ = Type::Range(one, one, zone());
  Handle<Object> thirtyone = factory->NewNumber(31.0);
  zero_thirtyone_range_ = Type::Range(zero, thirtyone, zone());
  // TODO(jarin): Can we have a correctification of the stupid type system?
  // These stupid work-arounds are just stupid!
  shifted_int32_ranges_[0] = Type::Signed32();
  if (SmiValuesAre31Bits()) {
    shifted_int32_ranges_[1] = Type::SignedSmall();
    for (size_t k = 2; k < arraysize(shifted_int32_ranges_); ++k) {
      Handle<Object> min = factory->NewNumber(kMinInt / (1 << k));
      Handle<Object> max = factory->NewNumber(kMaxInt / (1 << k));
      shifted_int32_ranges_[k] = Type::Range(min, max, zone());
    }
  } else {
    for (size_t k = 1; k < arraysize(shifted_int32_ranges_); ++k) {
      Handle<Object> min = factory->NewNumber(kMinInt / (1 << k));
      Handle<Object> max = factory->NewNumber(kMaxInt / (1 << k));
      shifted_int32_ranges_[k] = Type::Range(min, max, zone());
    }
  }
}


Reduction JSTypedLowering::ReplaceEagerly(Node* old, Node* node) {
  NodeProperties::ReplaceWithValue(old, node, node);
  return Changed(node);
}


// A helper class to simplify the process of reducing a single binop node with a
// JSOperator. This class manages the rewriting of context, control, and effect
// dependencies during lowering of a binop and contains numerous helper
// functions for matching the types of inputs to an operation.
class JSBinopReduction {
 public:
  JSBinopReduction(JSTypedLowering* lowering, Node* node)
      : lowering_(lowering),
        node_(node),
        left_type_(NodeProperties::GetBounds(node->InputAt(0)).upper),
        right_type_(NodeProperties::GetBounds(node->InputAt(1)).upper) {}

  void ConvertInputsToNumber() {
    node_->ReplaceInput(0, ConvertToNumber(left()));
    node_->ReplaceInput(1, ConvertToNumber(right()));
  }

  void ConvertInputsToInt32(bool left_signed, bool right_signed) {
    node_->ReplaceInput(0, ConvertToI32(left_signed, left()));
    node_->ReplaceInput(1, ConvertToI32(right_signed, right()));
  }

  void ConvertInputsToString() {
    node_->ReplaceInput(0, ConvertToString(left()));
    node_->ReplaceInput(1, ConvertToString(right()));
  }

  // Convert inputs for bitwise shift operation (ES5 spec 11.7).
  void ConvertInputsForShift(bool left_signed) {
    node_->ReplaceInput(0, ConvertToI32(left_signed, left()));
    Node* rnum = ConvertToI32(false, right());
    Type* rnum_type = NodeProperties::GetBounds(rnum).upper;
    if (!rnum_type->Is(lowering_->zero_thirtyone_range_)) {
      rnum = graph()->NewNode(machine()->Word32And(), rnum,
                              jsgraph()->Int32Constant(0x1F));
    }
    node_->ReplaceInput(1, rnum);
  }

  void SwapInputs() {
    Node* l = left();
    Node* r = right();
    node_->ReplaceInput(0, r);
    node_->ReplaceInput(1, l);
    std::swap(left_type_, right_type_);
  }

  // Remove all effect and control inputs and outputs to this node and change
  // to the pure operator {op}, possibly inserting a boolean inversion.
  Reduction ChangeToPureOperator(const Operator* op, bool invert = false) {
    DCHECK_EQ(0, op->EffectInputCount());
    DCHECK_EQ(false, OperatorProperties::HasContextInput(op));
    DCHECK_EQ(0, op->ControlInputCount());
    DCHECK_EQ(2, op->ValueInputCount());

    // Remove the effects from the node, if any, and update its effect usages.
    if (node_->op()->EffectInputCount() > 0) {
      RelaxEffects(node_);
    }
    // Remove the inputs corresponding to context, effect, and control.
    NodeProperties::RemoveNonValueInputs(node_);
    // Finally, update the operator to the new one.
    node_->set_op(op);

    if (invert) {
      // Insert an boolean not to invert the value.
      Node* value = graph()->NewNode(simplified()->BooleanNot(), node_);
      node_->ReplaceUses(value);
      // Note: ReplaceUses() smashes all uses, so smash it back here.
      value->ReplaceInput(0, node_);
      return lowering_->ReplaceWith(value);
    }
    return lowering_->Changed(node_);
  }

  bool OneInputIs(Type* t) { return left_type_->Is(t) || right_type_->Is(t); }

  bool BothInputsAre(Type* t) {
    return left_type_->Is(t) && right_type_->Is(t);
  }

  bool OneInputCannotBe(Type* t) {
    return !left_type_->Maybe(t) || !right_type_->Maybe(t);
  }

  bool NeitherInputCanBe(Type* t) {
    return !left_type_->Maybe(t) && !right_type_->Maybe(t);
  }

  Node* effect() { return NodeProperties::GetEffectInput(node_); }
  Node* control() { return NodeProperties::GetControlInput(node_); }
  Node* context() { return NodeProperties::GetContextInput(node_); }
  Node* left() { return NodeProperties::GetValueInput(node_, 0); }
  Node* right() { return NodeProperties::GetValueInput(node_, 1); }
  Type* left_type() { return left_type_; }
  Type* right_type() { return right_type_; }

  SimplifiedOperatorBuilder* simplified() { return lowering_->simplified(); }
  Graph* graph() { return lowering_->graph(); }
  JSGraph* jsgraph() { return lowering_->jsgraph(); }
  JSOperatorBuilder* javascript() { return lowering_->javascript(); }
  MachineOperatorBuilder* machine() { return lowering_->machine(); }

 private:
  JSTypedLowering* lowering_;  // The containing lowering instance.
  Node* node_;                 // The original node.
  Type* left_type_;            // Cache of the left input's type.
  Type* right_type_;           // Cache of the right input's type.

  Node* ConvertToString(Node* node) {
    // Avoid introducing too many eager ToString() operations.
    Reduction reduced = lowering_->ReduceJSToStringInput(node);
    if (reduced.Changed()) return reduced.replacement();
    Node* n = graph()->NewNode(javascript()->ToString(), node, context(),
                               effect(), control());
    update_effect(n);
    return n;
  }

  Node* ConvertToNumber(Node* node) {
    // Avoid introducing too many eager ToNumber() operations.
    Reduction reduced = lowering_->ReduceJSToNumberInput(node);
    if (reduced.Changed()) return reduced.replacement();
    Node* n = graph()->NewNode(javascript()->ToNumber(), node, context(),
                               effect(), control());
    update_effect(n);
    return n;
  }

  Node* ConvertToI32(bool is_signed, Node* node) {
    // Avoid introducing too many eager NumberToXXnt32() operations.
    node = ConvertToNumber(node);
    Type* type = is_signed ? Type::Signed32() : Type::Unsigned32();
    Type* input_type = NodeProperties::GetBounds(node).upper;

    if (input_type->Is(type)) return node;  // already in the value range.

    const Operator* op = is_signed ? simplified()->NumberToInt32()
                                   : simplified()->NumberToUint32();
    Node* n = graph()->NewNode(op, node);
    return n;
  }

  void update_effect(Node* effect) {
    NodeProperties::ReplaceEffectInput(node_, effect);
  }
};


Reduction JSTypedLowering::ReduceJSAdd(Node* node) {
  JSBinopReduction r(this, node);
  if (r.BothInputsAre(Type::Number())) {
    // JSAdd(x:number, y:number) => NumberAdd(x, y)
    return r.ChangeToPureOperator(simplified()->NumberAdd());
  }
  Type* maybe_string = Type::Union(Type::String(), Type::Receiver(), zone());
  if (r.BothInputsAre(Type::Primitive()) && r.NeitherInputCanBe(maybe_string)) {
    // JSAdd(x:-string, y:-string) => NumberAdd(ToNumber(x), ToNumber(y))
    r.ConvertInputsToNumber();
    return r.ChangeToPureOperator(simplified()->NumberAdd());
  }
#if 0
  // TODO(turbofan): General ToNumber disabled for now because:
  //   a) The inserted ToNumber operation screws up observability of valueOf.
  //   b) Deoptimization at ToNumber doesn't have corresponding bailout id.
  Type* maybe_string = Type::Union(Type::String(), Type::Receiver(), zone());
  if (r.NeitherInputCanBe(maybe_string)) {
    ...
  }
#endif
#if 0
  // TODO(turbofan): Lowering of StringAdd is disabled for now because:
  //   a) The inserted ToString operation screws up valueOf vs. toString order.
  //   b) Deoptimization at ToString doesn't have corresponding bailout id.
  //   c) Our current StringAddStub is actually non-pure and requires context.
  if (r.OneInputIs(Type::String())) {
    // JSAdd(x:string, y:string) => StringAdd(x, y)
    // JSAdd(x:string, y) => StringAdd(x, ToString(y))
    // JSAdd(x, y:string) => StringAdd(ToString(x), y)
    r.ConvertInputsToString();
    return r.ChangeToPureOperator(simplified()->StringAdd());
  }
#endif
  return NoChange();
}


Reduction JSTypedLowering::ReduceJSBitwiseOr(Node* node) {
  JSBinopReduction r(this, node);
  if (r.BothInputsAre(Type::Primitive()) || r.OneInputIs(zero_range_)) {
    // TODO(jarin): Propagate frame state input from non-primitive input node to
    // JSToNumber node.
    // TODO(titzer): some Smi bitwise operations don't really require going
    // all the way to int32, which can save tagging/untagging for some
    // operations
    // on some platforms.
    // TODO(turbofan): make this heuristic configurable for code size.
    r.ConvertInputsToInt32(true, true);
    return r.ChangeToPureOperator(machine()->Word32Or());
  }
  return NoChange();
}


Reduction JSTypedLowering::ReduceJSMultiply(Node* node) {
  JSBinopReduction r(this, node);
  if (r.BothInputsAre(Type::Primitive()) || r.OneInputIs(one_range_)) {
    // TODO(jarin): Propagate frame state input from non-primitive input node to
    // JSToNumber node.
    r.ConvertInputsToNumber();
    return r.ChangeToPureOperator(simplified()->NumberMultiply());
  }
  // TODO(turbofan): relax/remove the effects of this operator in other cases.
  return NoChange();
}


Reduction JSTypedLowering::ReduceNumberBinop(Node* node,
                                             const Operator* numberOp) {
  JSBinopReduction r(this, node);
  if (r.BothInputsAre(Type::Primitive())) {
    r.ConvertInputsToNumber();
    return r.ChangeToPureOperator(numberOp);
  }
#if 0
  // TODO(turbofan): General ToNumber disabled for now because:
  //   a) The inserted ToNumber operation screws up observability of valueOf.
  //   b) Deoptimization at ToNumber doesn't have corresponding bailout id.
  if (r.OneInputIs(Type::Primitive())) {
    // If at least one input is a primitive, then insert appropriate conversions
    // to number and reduce this operator to the given numeric one.
    // TODO(turbofan): make this heuristic configurable for code size.
    r.ConvertInputsToNumber();
    return r.ChangeToPureOperator(numberOp);
  }
#endif
  // TODO(turbofan): relax/remove the effects of this operator in other cases.
  return NoChange();
}


Reduction JSTypedLowering::ReduceI32Binop(Node* node, bool left_signed,
                                          bool right_signed,
                                          const Operator* intOp) {
  JSBinopReduction r(this, node);
  if (r.BothInputsAre(Type::Primitive())) {
    // TODO(titzer): some Smi bitwise operations don't really require going
    // all the way to int32, which can save tagging/untagging for some
    // operations
    // on some platforms.
    // TODO(turbofan): make this heuristic configurable for code size.
    r.ConvertInputsToInt32(left_signed, right_signed);
    return r.ChangeToPureOperator(intOp);
  }
  return NoChange();
}


Reduction JSTypedLowering::ReduceI32Shift(Node* node, bool left_signed,
                                          const Operator* shift_op) {
  JSBinopReduction r(this, node);
  if (r.BothInputsAre(Type::Primitive())) {
    r.ConvertInputsForShift(left_signed);
    return r.ChangeToPureOperator(shift_op);
  }
  return NoChange();
}


Reduction JSTypedLowering::ReduceJSComparison(Node* node) {
  JSBinopReduction r(this, node);
  if (r.BothInputsAre(Type::String())) {
    // If both inputs are definitely strings, perform a string comparison.
    const Operator* stringOp;
    switch (node->opcode()) {
      case IrOpcode::kJSLessThan:
        stringOp = simplified()->StringLessThan();
        break;
      case IrOpcode::kJSGreaterThan:
        stringOp = simplified()->StringLessThan();
        r.SwapInputs();  // a > b => b < a
        break;
      case IrOpcode::kJSLessThanOrEqual:
        stringOp = simplified()->StringLessThanOrEqual();
        break;
      case IrOpcode::kJSGreaterThanOrEqual:
        stringOp = simplified()->StringLessThanOrEqual();
        r.SwapInputs();  // a >= b => b <= a
        break;
      default:
        return NoChange();
    }
    return r.ChangeToPureOperator(stringOp);
  }
#if 0
  // TODO(turbofan): General ToNumber disabled for now because:
  //   a) The inserted ToNumber operation screws up observability of valueOf.
  //   b) Deoptimization at ToNumber doesn't have corresponding bailout id.
  Type* maybe_string = Type::Union(Type::String(), Type::Receiver(), zone());
  if (r.OneInputCannotBe(maybe_string)) {
    // If one input cannot be a string, then emit a number comparison.
    ...
  }
#endif
  Type* maybe_string = Type::Union(Type::String(), Type::Receiver(), zone());
  if (r.BothInputsAre(Type::Primitive()) && r.OneInputCannotBe(maybe_string)) {
    const Operator* less_than;
    const Operator* less_than_or_equal;
    if (r.BothInputsAre(Type::Unsigned32())) {
      less_than = machine()->Uint32LessThan();
      less_than_or_equal = machine()->Uint32LessThanOrEqual();
    } else if (r.BothInputsAre(Type::Signed32())) {
      less_than = machine()->Int32LessThan();
      less_than_or_equal = machine()->Int32LessThanOrEqual();
    } else {
      // TODO(turbofan): mixed signed/unsigned int32 comparisons.
      r.ConvertInputsToNumber();
      less_than = simplified()->NumberLessThan();
      less_than_or_equal = simplified()->NumberLessThanOrEqual();
    }
    const Operator* comparison;
    switch (node->opcode()) {
      case IrOpcode::kJSLessThan:
        comparison = less_than;
        break;
      case IrOpcode::kJSGreaterThan:
        comparison = less_than;
        r.SwapInputs();  // a > b => b < a
        break;
      case IrOpcode::kJSLessThanOrEqual:
        comparison = less_than_or_equal;
        break;
      case IrOpcode::kJSGreaterThanOrEqual:
        comparison = less_than_or_equal;
        r.SwapInputs();  // a >= b => b <= a
        break;
      default:
        return NoChange();
    }
    return r.ChangeToPureOperator(comparison);
  }
  // TODO(turbofan): relax/remove effects of this operator in other cases.
  return NoChange();  // Keep a generic comparison.
}


Reduction JSTypedLowering::ReduceJSEqual(Node* node, bool invert) {
  JSBinopReduction r(this, node);

  if (r.BothInputsAre(Type::Number())) {
    return r.ChangeToPureOperator(simplified()->NumberEqual(), invert);
  }
  if (r.BothInputsAre(Type::String())) {
    return r.ChangeToPureOperator(simplified()->StringEqual(), invert);
  }
  if (r.BothInputsAre(Type::Receiver())) {
    return r.ChangeToPureOperator(
        simplified()->ReferenceEqual(Type::Receiver()), invert);
  }
  // TODO(turbofan): js-typed-lowering of Equal(undefined)
  // TODO(turbofan): js-typed-lowering of Equal(null)
  // TODO(turbofan): js-typed-lowering of Equal(boolean)
  return NoChange();
}


Reduction JSTypedLowering::ReduceJSStrictEqual(Node* node, bool invert) {
  JSBinopReduction r(this, node);
  if (r.left() == r.right()) {
    // x === x is always true if x != NaN
    if (!r.left_type()->Maybe(Type::NaN())) {
      return ReplaceEagerly(node, jsgraph()->BooleanConstant(!invert));
    }
  }
  Type* string_or_number = Type::Union(Type::String(), Type::Number(), zone());
  if (r.OneInputCannotBe(string_or_number)) {
    // For values with canonical representation (i.e. not string nor number) an
    // empty type intersection means the values cannot be strictly equal.
    if (!r.left_type()->Maybe(r.right_type())) {
      return ReplaceEagerly(node, jsgraph()->BooleanConstant(invert));
    }
  }
  if (r.OneInputIs(Type::Undefined())) {
    return r.ChangeToPureOperator(
        simplified()->ReferenceEqual(Type::Undefined()), invert);
  }
  if (r.OneInputIs(Type::Null())) {
    return r.ChangeToPureOperator(simplified()->ReferenceEqual(Type::Null()),
                                  invert);
  }
  if (r.OneInputIs(Type::Boolean())) {
    return r.ChangeToPureOperator(simplified()->ReferenceEqual(Type::Boolean()),
                                  invert);
  }
  if (r.OneInputIs(Type::Object())) {
    return r.ChangeToPureOperator(simplified()->ReferenceEqual(Type::Object()),
                                  invert);
  }
  if (r.OneInputIs(Type::Receiver())) {
    return r.ChangeToPureOperator(
        simplified()->ReferenceEqual(Type::Receiver()), invert);
  }
  if (r.BothInputsAre(Type::String())) {
    return r.ChangeToPureOperator(simplified()->StringEqual(), invert);
  }
  if (r.BothInputsAre(Type::Number())) {
    return r.ChangeToPureOperator(simplified()->NumberEqual(), invert);
  }
  // TODO(turbofan): js-typed-lowering of StrictEqual(mixed types)
  return NoChange();
}


Reduction JSTypedLowering::ReduceJSToNumberInput(Node* input) {
  if (input->opcode() == IrOpcode::kJSToNumber) {
    // Recursively try to reduce the input first.
    Reduction result = ReduceJSToNumber(input);
    if (result.Changed()) return result;
    return Changed(input);  // JSToNumber(JSToNumber(x)) => JSToNumber(x)
  }
  Type* input_type = NodeProperties::GetBounds(input).upper;
  if (input_type->Is(Type::Number())) {
    // JSToNumber(x:number) => x
    return Changed(input);
  }
  if (input_type->Is(Type::Undefined())) {
    // JSToNumber(undefined) => #NaN
    return ReplaceWith(jsgraph()->NaNConstant());
  }
  if (input_type->Is(Type::Null())) {
    // JSToNumber(null) => #0
    return ReplaceWith(jsgraph()->ZeroConstant());
  }
  if (input_type->Is(Type::Boolean())) {
    // JSToNumber(x:boolean) => BooleanToNumber(x)
    return ReplaceWith(
        graph()->NewNode(simplified()->BooleanToNumber(), input));
  }
  // TODO(turbofan): js-typed-lowering of ToNumber(x:string)
  return NoChange();
}


Reduction JSTypedLowering::ReduceJSToNumber(Node* node) {
  // Try to reduce the input first.
  Node* const input = node->InputAt(0);
  Reduction reduction = ReduceJSToNumberInput(input);
  if (reduction.Changed()) {
    NodeProperties::ReplaceWithValue(node, reduction.replacement());
    return reduction;
  }
  Type* const input_type = NodeProperties::GetBounds(input).upper;
  if (input->opcode() == IrOpcode::kPhi && input_type->Is(Type::Primitive())) {
    Node* const context = node->InputAt(1);
    // JSToNumber(phi(x1,...,xn,control):primitive)
    //   => phi(JSToNumber(x1),...,JSToNumber(xn),control):number
    RelaxEffects(node);
    int const input_count = input->InputCount() - 1;
    Node* const control = input->InputAt(input_count);
    DCHECK_LE(0, input_count);
    DCHECK(NodeProperties::IsControl(control));
    DCHECK(NodeProperties::GetBounds(node).upper->Is(Type::Number()));
    DCHECK(!NodeProperties::GetBounds(input).upper->Is(Type::Number()));
    node->set_op(common()->Phi(kMachAnyTagged, input_count));
    for (int i = 0; i < input_count; ++i) {
      Node* value = input->InputAt(i);
      // Recursively try to reduce the value first.
      Reduction reduction = ReduceJSToNumberInput(value);
      if (reduction.Changed()) {
        value = reduction.replacement();
      } else {
        value = graph()->NewNode(javascript()->ToNumber(), value, context,
                                 graph()->start(), graph()->start());
      }
      if (i < node->InputCount()) {
        node->ReplaceInput(i, value);
      } else {
        node->AppendInput(graph()->zone(), value);
      }
    }
    if (input_count < node->InputCount()) {
      node->ReplaceInput(input_count, control);
    } else {
      node->AppendInput(graph()->zone(), control);
    }
    node->TrimInputCount(input_count + 1);
    return Changed(node);
  }
  return NoChange();
}


Reduction JSTypedLowering::ReduceJSToStringInput(Node* input) {
  if (input->opcode() == IrOpcode::kJSToString) {
    // Recursively try to reduce the input first.
    Reduction result = ReduceJSToStringInput(input->InputAt(0));
    if (result.Changed()) {
      RelaxEffects(input);
      return result;
    }
    return Changed(input);  // JSToString(JSToString(x)) => JSToString(x)
  }
  Type* input_type = NodeProperties::GetBounds(input).upper;
  if (input_type->Is(Type::String())) {
    return Changed(input);  // JSToString(x:string) => x
  }
  if (input_type->Is(Type::Undefined())) {
    return ReplaceWith(jsgraph()->HeapConstant(
        graph()->zone()->isolate()->factory()->undefined_string()));
  }
  if (input_type->Is(Type::Null())) {
    return ReplaceWith(jsgraph()->HeapConstant(
        graph()->zone()->isolate()->factory()->null_string()));
  }
  // TODO(turbofan): js-typed-lowering of ToString(x:boolean)
  // TODO(turbofan): js-typed-lowering of ToString(x:number)
  return NoChange();
}


Reduction JSTypedLowering::ReduceJSToBooleanInput(Node* input) {
  if (input->opcode() == IrOpcode::kJSToBoolean) {
    // Recursively try to reduce the input first.
    Reduction result = ReduceJSToBoolean(input);
    if (result.Changed()) return result;
    return Changed(input);  // JSToBoolean(JSToBoolean(x)) => JSToBoolean(x)
  }
  Type* input_type = NodeProperties::GetBounds(input).upper;
  if (input_type->Is(Type::Boolean())) {
    return Changed(input);  // JSToBoolean(x:boolean) => x
  }
  if (input_type->Is(Type::Undefined())) {
    // JSToBoolean(undefined) => #false
    return ReplaceWith(jsgraph()->FalseConstant());
  }
  if (input_type->Is(Type::Null())) {
    // JSToBoolean(null) => #false
    return ReplaceWith(jsgraph()->FalseConstant());
  }
  if (input_type->Is(Type::DetectableReceiver())) {
    // JSToBoolean(x:detectable) => #true
    return ReplaceWith(jsgraph()->TrueConstant());
  }
  if (input_type->Is(Type::Undetectable())) {
    // JSToBoolean(x:undetectable) => #false
    return ReplaceWith(jsgraph()->FalseConstant());
  }
  if (input_type->Is(Type::OrderedNumber())) {
    // JSToBoolean(x:ordered-number) => BooleanNot(NumberEqual(x, #0))
    Node* cmp = graph()->NewNode(simplified()->NumberEqual(), input,
                                 jsgraph()->ZeroConstant());
    Node* inv = graph()->NewNode(simplified()->BooleanNot(), cmp);
    return ReplaceWith(inv);
  }
  if (input_type->Is(Type::String())) {
    // JSToBoolean(x:string) => BooleanNot(NumberEqual(x.length, #0))
    FieldAccess access = AccessBuilder::ForStringLength();
    Node* length = graph()->NewNode(simplified()->LoadField(access), input,
                                    graph()->start(), graph()->start());
    Node* cmp = graph()->NewNode(simplified()->NumberEqual(), length,
                                 jsgraph()->ZeroConstant());
    Node* inv = graph()->NewNode(simplified()->BooleanNot(), cmp);
    return ReplaceWith(inv);
  }
  return NoChange();
}


Reduction JSTypedLowering::ReduceJSToBoolean(Node* node) {
  // Try to reduce the input first.
  Node* const input = node->InputAt(0);
  Reduction reduction = ReduceJSToBooleanInput(input);
  if (reduction.Changed()) {
    NodeProperties::ReplaceWithValue(node, reduction.replacement());
    return reduction;
  }
  Type* const input_type = NodeProperties::GetBounds(input).upper;
  if (input->opcode() == IrOpcode::kPhi && input_type->Is(Type::Primitive())) {
    Node* const context = node->InputAt(1);
    // JSToBoolean(phi(x1,...,xn,control):primitive)
    //   => phi(JSToBoolean(x1),...,JSToBoolean(xn),control):boolean
    RelaxEffects(node);
    int const input_count = input->InputCount() - 1;
    Node* const control = input->InputAt(input_count);
    DCHECK_LE(0, input_count);
    DCHECK(NodeProperties::IsControl(control));
    DCHECK(NodeProperties::GetBounds(node).upper->Is(Type::Boolean()));
    DCHECK(!NodeProperties::GetBounds(input).upper->Is(Type::Boolean()));
    node->set_op(common()->Phi(kMachAnyTagged, input_count));
    for (int i = 0; i < input_count; ++i) {
      Node* value = input->InputAt(i);
      // Recursively try to reduce the value first.
      Reduction reduction = ReduceJSToBooleanInput(value);
      if (reduction.Changed()) {
        value = reduction.replacement();
      } else {
        value = graph()->NewNode(javascript()->ToBoolean(), value, context,
                                 graph()->start(), graph()->start());
      }
      if (i < node->InputCount()) {
        node->ReplaceInput(i, value);
      } else {
        node->AppendInput(graph()->zone(), value);
      }
    }
    if (input_count < node->InputCount()) {
      node->ReplaceInput(input_count, control);
    } else {
      node->AppendInput(graph()->zone(), control);
    }
    node->TrimInputCount(input_count + 1);
    return Changed(node);
  }
  return NoChange();
}


Reduction JSTypedLowering::ReduceJSLoadProperty(Node* node) {
  Node* key = NodeProperties::GetValueInput(node, 1);
  Node* base = NodeProperties::GetValueInput(node, 0);
  Type* key_type = NodeProperties::GetBounds(key).upper;
  Type* base_type = NodeProperties::GetBounds(base).upper;
  // TODO(mstarzinger): This lowering is not correct if:
  //   a) The typed array or it's buffer is neutered.
  if (base_type->IsConstant() &&
      base_type->AsConstant()->Value()->IsJSTypedArray()) {
    Handle<JSTypedArray> const array =
        Handle<JSTypedArray>::cast(base_type->AsConstant()->Value());
    BufferAccess const access(array->type());
    size_t const k = ElementSizeLog2Of(access.machine_type());
    double const byte_length = array->byte_length()->Number();
    CHECK_LT(k, arraysize(shifted_int32_ranges_));
    if (IsExternalArrayElementsKind(array->map()->elements_kind()) &&
        access.external_array_type() != kExternalUint8ClampedArray &&
        key_type->Is(shifted_int32_ranges_[k]) && byte_length <= kMaxInt) {
      // JSLoadProperty(typed-array, int32)
      Handle<ExternalArray> elements =
          Handle<ExternalArray>::cast(handle(array->elements()));
      Node* buffer = jsgraph()->PointerConstant(elements->external_pointer());
      Node* length = jsgraph()->Constant(byte_length);
      Node* effect = NodeProperties::GetEffectInput(node);
      Node* control = NodeProperties::GetControlInput(node);
      // Check if we can avoid the bounds check.
      if (key_type->Min() >= 0 && key_type->Max() < array->length()->Number()) {
        Node* load = graph()->NewNode(
            simplified()->LoadElement(
                AccessBuilder::ForTypedArrayElement(array->type(), true)),
            buffer, key, effect, control);
        return ReplaceEagerly(node, load);
      }
      // Compute byte offset.
      Node* offset = Word32Shl(key, static_cast<int>(k));
      Node* load = graph()->NewNode(simplified()->LoadBuffer(access), buffer,
                                    offset, length, effect, control);
      return ReplaceEagerly(node, load);
    }
  }
  return NoChange();
}


Reduction JSTypedLowering::ReduceJSStoreProperty(Node* node) {
  Node* key = NodeProperties::GetValueInput(node, 1);
  Node* base = NodeProperties::GetValueInput(node, 0);
  Node* value = NodeProperties::GetValueInput(node, 2);
  Type* key_type = NodeProperties::GetBounds(key).upper;
  Type* base_type = NodeProperties::GetBounds(base).upper;
  Type* value_type = NodeProperties::GetBounds(value).upper;
  // TODO(mstarzinger): This lowering is not correct if:
  //   a) The typed array or its buffer is neutered.
  if (base_type->IsConstant() &&
      base_type->AsConstant()->Value()->IsJSTypedArray()) {
    Handle<JSTypedArray> const array =
        Handle<JSTypedArray>::cast(base_type->AsConstant()->Value());
    BufferAccess const access(array->type());
    size_t const k = ElementSizeLog2Of(access.machine_type());
    double const byte_length = array->byte_length()->Number();
    CHECK_LT(k, arraysize(shifted_int32_ranges_));
    if (IsExternalArrayElementsKind(array->map()->elements_kind()) &&
        access.external_array_type() != kExternalUint8ClampedArray &&
        key_type->Is(shifted_int32_ranges_[k]) && byte_length <= kMaxInt) {
      // JSLoadProperty(typed-array, int32)
      Handle<ExternalArray> elements =
          Handle<ExternalArray>::cast(handle(array->elements()));
      Node* buffer = jsgraph()->PointerConstant(elements->external_pointer());
      Node* length = jsgraph()->Constant(byte_length);
      Node* context = NodeProperties::GetContextInput(node);
      Node* effect = NodeProperties::GetEffectInput(node);
      Node* control = NodeProperties::GetControlInput(node);
      // Convert to a number first.
      if (!value_type->Is(Type::Number())) {
        Reduction number_reduction = ReduceJSToNumberInput(value);
        if (number_reduction.Changed()) {
          value = number_reduction.replacement();
        } else {
          value = effect = graph()->NewNode(javascript()->ToNumber(), value,
                                            context, effect, control);
        }
      }
      // For integer-typed arrays, convert to the integer type.
      if (TypeOf(access.machine_type()) == kTypeInt32 &&
          !value_type->Is(Type::Signed32())) {
        value = graph()->NewNode(simplified()->NumberToInt32(), value);
      } else if (TypeOf(access.machine_type()) == kTypeUint32 &&
                 !value_type->Is(Type::Unsigned32())) {
        value = graph()->NewNode(simplified()->NumberToUint32(), value);
      }
      // Check if we can avoid the bounds check.
      if (key_type->Min() >= 0 && key_type->Max() < array->length()->Number()) {
        node->set_op(simplified()->StoreElement(
            AccessBuilder::ForTypedArrayElement(array->type(), true)));
        node->ReplaceInput(0, buffer);
        DCHECK_EQ(key, node->InputAt(1));
        node->ReplaceInput(2, value);
        node->ReplaceInput(3, effect);
        node->ReplaceInput(4, control);
        node->TrimInputCount(5);
        return Changed(node);
      }
      // Compute byte offset.
      Node* offset = Word32Shl(key, static_cast<int>(k));
      // Turn into a StoreBuffer operation.
      node->set_op(simplified()->StoreBuffer(access));
      node->ReplaceInput(0, buffer);
      node->ReplaceInput(1, offset);
      node->ReplaceInput(2, length);
      node->ReplaceInput(3, value);
      node->ReplaceInput(4, effect);
      DCHECK_EQ(control, node->InputAt(5));
      DCHECK_EQ(6, node->InputCount());
      return Changed(node);
    }
  }
  return NoChange();
}


static Reduction ReplaceWithReduction(Node* node, Reduction reduction) {
  if (reduction.Changed()) {
    NodeProperties::ReplaceWithValue(node, reduction.replacement());
    return reduction;
  }
  return Reducer::NoChange();
}


Reduction JSTypedLowering::Reduce(Node* node) {
  // Check if the output type is a singleton.  In that case we already know the
  // result value and can simply replace the node unless there are effects.
  if (NodeProperties::IsTyped(node) &&
      NodeProperties::GetBounds(node).upper->IsConstant() &&
      !IrOpcode::IsLeafOpcode(node->opcode()) &&
      node->op()->EffectOutputCount() == 0) {
    return ReplaceEagerly(node, jsgraph()->Constant(
        NodeProperties::GetBounds(node).upper->AsConstant()->Value()));
    // TODO(neis): Extend this to Range(x,x), NaN, MinusZero, ...?
  }
  switch (node->opcode()) {
    case IrOpcode::kJSEqual:
      return ReduceJSEqual(node, false);
    case IrOpcode::kJSNotEqual:
      return ReduceJSEqual(node, true);
    case IrOpcode::kJSStrictEqual:
      return ReduceJSStrictEqual(node, false);
    case IrOpcode::kJSStrictNotEqual:
      return ReduceJSStrictEqual(node, true);
    case IrOpcode::kJSLessThan:         // fall through
    case IrOpcode::kJSGreaterThan:      // fall through
    case IrOpcode::kJSLessThanOrEqual:  // fall through
    case IrOpcode::kJSGreaterThanOrEqual:
      return ReduceJSComparison(node);
    case IrOpcode::kJSBitwiseOr:
      return ReduceJSBitwiseOr(node);
    case IrOpcode::kJSBitwiseXor:
      return ReduceI32Binop(node, true, true, machine()->Word32Xor());
    case IrOpcode::kJSBitwiseAnd:
      return ReduceI32Binop(node, true, true, machine()->Word32And());
    case IrOpcode::kJSShiftLeft:
      return ReduceI32Shift(node, true, machine()->Word32Shl());
    case IrOpcode::kJSShiftRight:
      return ReduceI32Shift(node, true, machine()->Word32Sar());
    case IrOpcode::kJSShiftRightLogical:
      return ReduceI32Shift(node, false, machine()->Word32Shr());
    case IrOpcode::kJSAdd:
      return ReduceJSAdd(node);
    case IrOpcode::kJSSubtract:
      return ReduceNumberBinop(node, simplified()->NumberSubtract());
    case IrOpcode::kJSMultiply:
      return ReduceJSMultiply(node);
    case IrOpcode::kJSDivide:
      return ReduceNumberBinop(node, simplified()->NumberDivide());
    case IrOpcode::kJSModulus:
      return ReduceNumberBinop(node, simplified()->NumberModulus());
    case IrOpcode::kJSUnaryNot: {
      Reduction result = ReduceJSToBooleanInput(node->InputAt(0));
      Node* value;
      if (result.Changed()) {
        // JSUnaryNot(x:boolean) => BooleanNot(x)
        value =
            graph()->NewNode(simplified()->BooleanNot(), result.replacement());
        NodeProperties::ReplaceWithValue(node, value);
        return Changed(value);
      } else {
        // JSUnaryNot(x) => BooleanNot(JSToBoolean(x))
        value = graph()->NewNode(simplified()->BooleanNot(), node);
        node->set_op(javascript()->ToBoolean());
        NodeProperties::ReplaceWithValue(node, value, node);
        // Note: ReplaceUses() smashes all uses, so smash it back here.
        value->ReplaceInput(0, node);
        return Changed(node);
      }
    }
    case IrOpcode::kJSToBoolean:
      return ReduceJSToBoolean(node);
    case IrOpcode::kJSToNumber:
      return ReduceJSToNumber(node);
    case IrOpcode::kJSToString:
      return ReplaceWithReduction(node,
                                  ReduceJSToStringInput(node->InputAt(0)));
    case IrOpcode::kJSLoadProperty:
      return ReduceJSLoadProperty(node);
    case IrOpcode::kJSStoreProperty:
      return ReduceJSStoreProperty(node);
    case IrOpcode::kJSCallFunction:
      return JSBuiltinReducer(jsgraph()).Reduce(node);
    default:
      break;
  }
  return NoChange();
}


Node* JSTypedLowering::Word32Shl(Node* const lhs, int32_t const rhs) {
  if (rhs == 0) return lhs;
  return graph()->NewNode(machine()->Word32Shl(), lhs,
                          jsgraph()->Int32Constant(rhs));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
