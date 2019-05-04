// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-math-gen.h"

#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/code-factory.h"
#include "src/code-stub-assembler.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// ES6 section 20.2.2 Function Properties of the Math Object

// ES6 #sec-math.abs
TF_BUILTIN(MathAbs, CodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);

  // We might need to loop once for ToNumber conversion.
  VARIABLE(var_x, MachineRepresentation::kTagged);
  Label loop(this, &var_x);
  var_x.Bind(Parameter(Descriptor::kX));
  Goto(&loop);
  BIND(&loop);
  {
    // Load the current {x} value.
    Node* x = var_x.value();

    // Check if {x} is a Smi or a HeapObject.
    Label if_xissmi(this), if_xisnotsmi(this);
    Branch(TaggedIsSmi(x), &if_xissmi, &if_xisnotsmi);

    BIND(&if_xissmi);
    {
      Label if_overflow(this, Label::kDeferred);

      // check if support abs function
      if (IsIntPtrAbsWithOverflowSupported()) {
        Node* pair = IntPtrAbsWithOverflow(x);
        Node* overflow = Projection(1, pair);
        GotoIf(overflow, &if_overflow);

        // There is a Smi representation for negated {x}.
        Node* result = Projection(0, pair);
        Return(BitcastWordToTagged(result));

      } else {
        // Check if {x} is already positive.
        Label if_xispositive(this), if_xisnotpositive(this);
        BranchIfSmiLessThanOrEqual(SmiConstant(0), CAST(x), &if_xispositive,
                                   &if_xisnotpositive);

        BIND(&if_xispositive);
        {
          // Just return the input {x}.
          Return(x);
        }

        BIND(&if_xisnotpositive);
        {
          // Try to negate the {x} value.
          TNode<Smi> result = TrySmiSub(SmiConstant(0), CAST(x), &if_overflow);
          Return(result);
        }
      }

      BIND(&if_overflow);
      { Return(NumberConstant(0.0 - Smi::kMinValue)); }
    }

    BIND(&if_xisnotsmi);
    {
      // Check if {x} is a HeapNumber.
      Label if_xisheapnumber(this), if_xisnotheapnumber(this, Label::kDeferred);
      Branch(IsHeapNumber(x), &if_xisheapnumber, &if_xisnotheapnumber);

      BIND(&if_xisheapnumber);
      {
        Node* x_value = LoadHeapNumberValue(x);
        Node* value = Float64Abs(x_value);
        Node* result = AllocateHeapNumberWithValue(value);
        Return(result);
      }

      BIND(&if_xisnotheapnumber);
      {
        // Need to convert {x} to a Number first.
        var_x.Bind(CallBuiltin(Builtins::kNonNumberToNumber, context, x));
        Goto(&loop);
      }
    }
  }
}

void MathBuiltinsAssembler::MathRoundingOperation(
    Node* context, Node* x,
    TNode<Float64T> (CodeStubAssembler::*float64op)(SloppyTNode<Float64T>)) {
  // We might need to loop once for ToNumber conversion.
  VARIABLE(var_x, MachineRepresentation::kTagged, x);
  Label loop(this, &var_x);
  Goto(&loop);
  BIND(&loop);
  {
    // Load the current {x} value.
    Node* x = var_x.value();

    // Check if {x} is a Smi or a HeapObject.
    Label if_xissmi(this), if_xisnotsmi(this);
    Branch(TaggedIsSmi(x), &if_xissmi, &if_xisnotsmi);

    BIND(&if_xissmi);
    {
      // Nothing to do when {x} is a Smi.
      Return(x);
    }

    BIND(&if_xisnotsmi);
    {
      // Check if {x} is a HeapNumber.
      Label if_xisheapnumber(this), if_xisnotheapnumber(this, Label::kDeferred);
      Branch(IsHeapNumber(x), &if_xisheapnumber, &if_xisnotheapnumber);

      BIND(&if_xisheapnumber);
      {
        Node* x_value = LoadHeapNumberValue(x);
        Node* value = (this->*float64op)(x_value);
        Node* result = ChangeFloat64ToTagged(value);
        Return(result);
      }

      BIND(&if_xisnotheapnumber);
      {
        // Need to convert {x} to a Number first.
        var_x.Bind(CallBuiltin(Builtins::kNonNumberToNumber, context, x));
        Goto(&loop);
      }
    }
  }
}

void MathBuiltinsAssembler::MathMaxMin(
    Node* context, Node* argc,
    TNode<Float64T> (CodeStubAssembler::*float64op)(SloppyTNode<Float64T>,
                                                    SloppyTNode<Float64T>),
    double default_val) {
  CodeStubArguments arguments(this, ChangeInt32ToIntPtr(argc));
  argc = arguments.GetLength(INTPTR_PARAMETERS);

  VARIABLE(result, MachineRepresentation::kFloat64);
  result.Bind(Float64Constant(default_val));

  CodeStubAssembler::VariableList vars({&result}, zone());
  arguments.ForEach(vars, [=, &result](Node* arg) {
    Node* float_value = TruncateTaggedToFloat64(context, arg);
    result.Bind((this->*float64op)(result.value(), float_value));
  });

  arguments.PopAndReturn(ChangeFloat64ToTagged(result.value()));
}

// ES6 #sec-math.ceil
TF_BUILTIN(MathCeil, MathBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* x = Parameter(Descriptor::kX);
  MathRoundingOperation(context, x, &CodeStubAssembler::Float64Ceil);
}

// ES6 #sec-math.floor
TF_BUILTIN(MathFloor, MathBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* x = Parameter(Descriptor::kX);
  MathRoundingOperation(context, x, &CodeStubAssembler::Float64Floor);
}

// ES6 #sec-math.imul
TF_BUILTIN(MathImul, CodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* x = Parameter(Descriptor::kX);
  Node* y = Parameter(Descriptor::kY);
  Node* x_value = TruncateTaggedToWord32(context, x);
  Node* y_value = TruncateTaggedToWord32(context, y);
  Node* value = Int32Mul(x_value, y_value);
  Node* result = ChangeInt32ToTagged(value);
  Return(result);
}

CodeStubAssembler::Node* MathBuiltinsAssembler::MathPow(Node* context,
                                                        Node* base,
                                                        Node* exponent) {
  Node* base_value = TruncateTaggedToFloat64(context, base);
  Node* exponent_value = TruncateTaggedToFloat64(context, exponent);
  Node* value = Float64Pow(base_value, exponent_value);
  return ChangeFloat64ToTagged(value);
}

// ES6 #sec-math.pow
TF_BUILTIN(MathPow, MathBuiltinsAssembler) {
  Return(MathPow(Parameter(Descriptor::kContext), Parameter(Descriptor::kBase),
                 Parameter(Descriptor::kExponent)));
}

// ES6 #sec-math.random
TF_BUILTIN(MathRandom, CodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* native_context = LoadNativeContext(context);

  // Load cache index.
  TVARIABLE(Smi, smi_index);
  smi_index = CAST(
      LoadContextElement(native_context, Context::MATH_RANDOM_INDEX_INDEX));

  // Cached random numbers are exhausted if index is 0. Go to slow path.
  Label if_cached(this);
  GotoIf(SmiAbove(smi_index.value(), SmiConstant(0)), &if_cached);

  // Cache exhausted, populate the cache. Return value is the new index.
  Node* const refill_math_random =
      ExternalConstant(ExternalReference::refill_math_random());
  Node* const isolate_ptr =
      ExternalConstant(ExternalReference::isolate_address(isolate()));
  MachineType type_tagged = MachineType::AnyTagged();
  MachineType type_ptr = MachineType::Pointer();

  smi_index = CAST(CallCFunction(refill_math_random, type_tagged,
                                 std::make_pair(type_ptr, isolate_ptr),
                                 std::make_pair(type_tagged, native_context)));
  Goto(&if_cached);

  // Compute next index by decrement.
  BIND(&if_cached);
  TNode<Smi> new_smi_index = SmiSub(smi_index.value(), SmiConstant(1));
  StoreContextElement(native_context, Context::MATH_RANDOM_INDEX_INDEX,
                      new_smi_index);

  // Load and return next cached random number.
  Node* array =
      LoadContextElement(native_context, Context::MATH_RANDOM_CACHE_INDEX);
  Node* random = LoadFixedDoubleArrayElement(
      array, new_smi_index, MachineType::Float64(), 0, SMI_PARAMETERS);
  Return(AllocateHeapNumberWithValue(random));
}

// ES6 #sec-math.round
TF_BUILTIN(MathRound, MathBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* x = Parameter(Descriptor::kX);
  MathRoundingOperation(context, x, &CodeStubAssembler::Float64Round);
}

// ES6 #sec-math.trunc
TF_BUILTIN(MathTrunc, MathBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* x = Parameter(Descriptor::kX);
  MathRoundingOperation(context, x, &CodeStubAssembler::Float64Trunc);
}

// ES6 #sec-math.max
TF_BUILTIN(MathMax, MathBuiltinsAssembler) {
  // TODO(ishell): use constants from Descriptor once the JSFunction linkage
  // arguments are reordered.
  Node* context = Parameter(Descriptor::kContext);
  Node* argc = Parameter(Descriptor::kJSActualArgumentsCount);
  MathMaxMin(context, argc, &CodeStubAssembler::Float64Max, -1.0 * V8_INFINITY);
}

// ES6 #sec-math.min
TF_BUILTIN(MathMin, MathBuiltinsAssembler) {
  // TODO(ishell): use constants from Descriptor once the JSFunction linkage
  // arguments are reordered.
  Node* context = Parameter(Descriptor::kContext);
  Node* argc = Parameter(Descriptor::kJSActualArgumentsCount);
  MathMaxMin(context, argc, &CodeStubAssembler::Float64Min, V8_INFINITY);
}

}  // namespace internal
}  // namespace v8
