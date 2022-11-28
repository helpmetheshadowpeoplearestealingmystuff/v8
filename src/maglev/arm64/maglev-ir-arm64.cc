// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-ir.h"

namespace v8 {
namespace internal {
namespace maglev {

// TODO(v8:7700): Remove this logic when all nodes are implemented.
class MaglevUnimplementedIRNode {
 public:
  MaglevUnimplementedIRNode() {}
  void PreProcessGraph(Graph* graph) {}
  void PostProcessGraph(Graph* graph) {}
  void PreProcessBasicBlock(BasicBlock* block) {}
  template <typename NodeT>
  void Process(NodeT* node, const ProcessingState& state);
  bool has_unimplemented_node() const { return has_unimplemented_node_; }

 private:
  bool has_unimplemented_node_ = false;
};

#define UNIMPLEMENTED_NODE(Node, ...)                                     \
  void Node ::AllocateVreg(MaglevVregAllocationState* vreg_state) {}      \
                                                                          \
  void Node ::GenerateCode(MaglevAssembler* masm,                         \
                           const ProcessingState& state) {                \
    USE(__VA_ARGS__);                                                     \
  }                                                                       \
  template <>                                                             \
  void MaglevUnimplementedIRNode::Process(Node* node,                     \
                                          const ProcessingState& state) { \
    std::cerr << "Unimplemented Maglev IR Node: " #Node << std::endl;     \
    has_unimplemented_node_ = true;                                       \
  }

bool MaglevGraphHasUnimplementedNode(Graph* graph) {
  GraphProcessor<MaglevUnimplementedIRNode> processor;
  processor.ProcessGraph(graph);
  return processor.node_processor().has_unimplemented_node();
}

UNIMPLEMENTED_NODE(GenericAdd)
UNIMPLEMENTED_NODE(GenericSubtract)
UNIMPLEMENTED_NODE(GenericMultiply)
UNIMPLEMENTED_NODE(GenericDivide)
UNIMPLEMENTED_NODE(GenericModulus)
UNIMPLEMENTED_NODE(GenericExponentiate)
UNIMPLEMENTED_NODE(GenericBitwiseAnd)
UNIMPLEMENTED_NODE(GenericBitwiseOr)
UNIMPLEMENTED_NODE(GenericBitwiseXor)
UNIMPLEMENTED_NODE(GenericShiftLeft)
UNIMPLEMENTED_NODE(GenericShiftRight)
UNIMPLEMENTED_NODE(GenericShiftRightLogical)
UNIMPLEMENTED_NODE(GenericBitwiseNot)
UNIMPLEMENTED_NODE(GenericNegate)
UNIMPLEMENTED_NODE(GenericIncrement)
UNIMPLEMENTED_NODE(GenericDecrement)
UNIMPLEMENTED_NODE(GenericEqual)
UNIMPLEMENTED_NODE(GenericStrictEqual)
UNIMPLEMENTED_NODE(GenericLessThan)
UNIMPLEMENTED_NODE(GenericLessThanOrEqual)
UNIMPLEMENTED_NODE(GenericGreaterThan)
UNIMPLEMENTED_NODE(GenericGreaterThanOrEqual)
UNIMPLEMENTED_NODE(Int32AddWithOverflow)
UNIMPLEMENTED_NODE(Int32SubtractWithOverflow)
UNIMPLEMENTED_NODE(Int32MultiplyWithOverflow)
UNIMPLEMENTED_NODE(Int32DivideWithOverflow)
UNIMPLEMENTED_NODE(Int32ModulusWithOverflow)
UNIMPLEMENTED_NODE(Int32BitwiseAnd)
UNIMPLEMENTED_NODE(Int32BitwiseOr)
UNIMPLEMENTED_NODE(Int32BitwiseXor)
UNIMPLEMENTED_NODE(Int32ShiftLeft)
UNIMPLEMENTED_NODE(Int32ShiftRight)
UNIMPLEMENTED_NODE(Int32ShiftRightLogical)
UNIMPLEMENTED_NODE(Int32BitwiseNot)
UNIMPLEMENTED_NODE(Int32NegateWithOverflow)
UNIMPLEMENTED_NODE(Int32IncrementWithOverflow)
UNIMPLEMENTED_NODE(Int32DecrementWithOverflow)
UNIMPLEMENTED_NODE(Int32Equal)
UNIMPLEMENTED_NODE(Int32StrictEqual)
UNIMPLEMENTED_NODE(Int32LessThan)
UNIMPLEMENTED_NODE(Int32LessThanOrEqual)
UNIMPLEMENTED_NODE(Int32GreaterThan)
UNIMPLEMENTED_NODE(Int32GreaterThanOrEqual)
UNIMPLEMENTED_NODE(Float64Add)
UNIMPLEMENTED_NODE(Float64Subtract)
UNIMPLEMENTED_NODE(Float64Multiply)
UNIMPLEMENTED_NODE(Float64Divide)
UNIMPLEMENTED_NODE(Float64Exponentiate)
UNIMPLEMENTED_NODE(Float64Modulus)
UNIMPLEMENTED_NODE(Float64Negate)
UNIMPLEMENTED_NODE(Float64Equal)
UNIMPLEMENTED_NODE(Float64StrictEqual)
UNIMPLEMENTED_NODE(Float64LessThan)
UNIMPLEMENTED_NODE(Float64LessThanOrEqual)
UNIMPLEMENTED_NODE(Float64GreaterThan)
UNIMPLEMENTED_NODE(Float64GreaterThanOrEqual)
UNIMPLEMENTED_NODE(Float64Ieee754Unary)
UNIMPLEMENTED_NODE(Constant)
UNIMPLEMENTED_NODE(Float64Constant)
UNIMPLEMENTED_NODE(Int32Constant)
UNIMPLEMENTED_NODE(RootConstant)
UNIMPLEMENTED_NODE(SmiConstant)
UNIMPLEMENTED_NODE(BuiltinStringFromCharCode)
UNIMPLEMENTED_NODE(BuiltinStringPrototypeCharCodeAt)
UNIMPLEMENTED_NODE(Call, receiver_mode_, target_type_, feedback_)
UNIMPLEMENTED_NODE(CallBuiltin)
UNIMPLEMENTED_NODE(CallRuntime)
UNIMPLEMENTED_NODE(CallWithArrayLike)
UNIMPLEMENTED_NODE(CallWithSpread)
UNIMPLEMENTED_NODE(CallKnownJSFunction)
UNIMPLEMENTED_NODE(Construct)
UNIMPLEMENTED_NODE(ConstructWithSpread)
UNIMPLEMENTED_NODE(ConvertReceiver, mode_)
UNIMPLEMENTED_NODE(ConvertHoleToUndefined)
UNIMPLEMENTED_NODE(CreateEmptyArrayLiteral)
UNIMPLEMENTED_NODE(CreateArrayLiteral)
UNIMPLEMENTED_NODE(CreateShallowArrayLiteral)
UNIMPLEMENTED_NODE(CreateObjectLiteral)
UNIMPLEMENTED_NODE(CreateEmptyObjectLiteral)
UNIMPLEMENTED_NODE(CreateShallowObjectLiteral)
UNIMPLEMENTED_NODE(CreateFunctionContext)
UNIMPLEMENTED_NODE(CreateClosure)
UNIMPLEMENTED_NODE(FastCreateClosure)
UNIMPLEMENTED_NODE(CreateRegExpLiteral)
UNIMPLEMENTED_NODE(DeleteProperty)
UNIMPLEMENTED_NODE(ForInPrepare)
UNIMPLEMENTED_NODE(ForInNext)
UNIMPLEMENTED_NODE(GeneratorRestoreRegister)
UNIMPLEMENTED_NODE(GetIterator)
UNIMPLEMENTED_NODE(GetSecondReturnedValue)
UNIMPLEMENTED_NODE(GetTemplateObject)
UNIMPLEMENTED_NODE(InitialValue)
UNIMPLEMENTED_NODE(LoadTaggedField)
UNIMPLEMENTED_NODE(LoadDoubleField)
UNIMPLEMENTED_NODE(LoadTaggedElement)
UNIMPLEMENTED_NODE(LoadSignedIntDataViewElement, type_)
UNIMPLEMENTED_NODE(LoadDoubleDataViewElement)
UNIMPLEMENTED_NODE(LoadSignedIntTypedArrayElement, elements_kind_)
UNIMPLEMENTED_NODE(LoadUnsignedIntTypedArrayElement, elements_kind_)
UNIMPLEMENTED_NODE(LoadDoubleTypedArrayElement, elements_kind_)
UNIMPLEMENTED_NODE(LoadDoubleElement)
UNIMPLEMENTED_NODE(LoadGlobal)
UNIMPLEMENTED_NODE(LoadNamedGeneric)
UNIMPLEMENTED_NODE(LoadNamedFromSuperGeneric)
UNIMPLEMENTED_NODE(SetNamedGeneric)
UNIMPLEMENTED_NODE(DefineNamedOwnGeneric)
UNIMPLEMENTED_NODE(StoreInArrayLiteralGeneric)
UNIMPLEMENTED_NODE(StoreGlobal)
UNIMPLEMENTED_NODE(GetKeyedGeneric)
UNIMPLEMENTED_NODE(SetKeyedGeneric)
UNIMPLEMENTED_NODE(DefineKeyedOwnGeneric)
UNIMPLEMENTED_NODE(Phi)
void Phi::AllocateVregInPostProcess(MaglevVregAllocationState*) {}
UNIMPLEMENTED_NODE(RegisterInput)
UNIMPLEMENTED_NODE(CheckedSmiTagInt32)
UNIMPLEMENTED_NODE(CheckedSmiTagUint32)
UNIMPLEMENTED_NODE(UnsafeSmiTag)
UNIMPLEMENTED_NODE(CheckedSmiUntag)
UNIMPLEMENTED_NODE(UnsafeSmiUntag)
UNIMPLEMENTED_NODE(CheckedInternalizedString, check_type_)
UNIMPLEMENTED_NODE(CheckedObjectToIndex)
UNIMPLEMENTED_NODE(CheckedTruncateNumberToInt32)
UNIMPLEMENTED_NODE(CheckedInt32ToUint32)
UNIMPLEMENTED_NODE(CheckedUint32ToInt32)
UNIMPLEMENTED_NODE(ChangeInt32ToFloat64)
UNIMPLEMENTED_NODE(ChangeUint32ToFloat64)
UNIMPLEMENTED_NODE(CheckedTruncateFloat64ToInt32)
UNIMPLEMENTED_NODE(CheckedTruncateFloat64ToUint32)
UNIMPLEMENTED_NODE(TruncateUint32ToInt32)
UNIMPLEMENTED_NODE(TruncateFloat64ToInt32)
UNIMPLEMENTED_NODE(Int32ToNumber)
UNIMPLEMENTED_NODE(Uint32ToNumber)
UNIMPLEMENTED_NODE(Float64Box)
UNIMPLEMENTED_NODE(HoleyFloat64Box)
UNIMPLEMENTED_NODE(CheckedFloat64Unbox)
UNIMPLEMENTED_NODE(LogicalNot)
UNIMPLEMENTED_NODE(SetPendingMessage)
UNIMPLEMENTED_NODE(StringAt)
UNIMPLEMENTED_NODE(StringLength)
UNIMPLEMENTED_NODE(ToBoolean)
UNIMPLEMENTED_NODE(ToBooleanLogicalNot)
UNIMPLEMENTED_NODE(TaggedEqual)
UNIMPLEMENTED_NODE(TaggedNotEqual)
UNIMPLEMENTED_NODE(TestInstanceOf)
UNIMPLEMENTED_NODE(TestUndetectable)
UNIMPLEMENTED_NODE(TestTypeOf, literal_)
UNIMPLEMENTED_NODE(ToName)
UNIMPLEMENTED_NODE(ToNumberOrNumeric)
UNIMPLEMENTED_NODE(ToObject)
UNIMPLEMENTED_NODE(ToString)
UNIMPLEMENTED_NODE(ConstantGapMove)
UNIMPLEMENTED_NODE(GapMove)
UNIMPLEMENTED_NODE(AssertInt32, condition_, reason_)
UNIMPLEMENTED_NODE(CheckDynamicValue)
UNIMPLEMENTED_NODE(CheckInt32IsSmi)
UNIMPLEMENTED_NODE(CheckUint32IsSmi)
UNIMPLEMENTED_NODE(CheckHeapObject)
UNIMPLEMENTED_NODE(CheckInt32Condition, condition_, reason_)
UNIMPLEMENTED_NODE(CheckJSArrayBounds)
UNIMPLEMENTED_NODE(CheckJSDataViewBounds, element_type_)
UNIMPLEMENTED_NODE(CheckJSObjectElementsBounds)
UNIMPLEMENTED_NODE(CheckJSTypedArrayBounds, elements_kind_)
UNIMPLEMENTED_NODE(CheckMaps, check_type_)
UNIMPLEMENTED_NODE(CheckMapsWithMigration, check_type_)
UNIMPLEMENTED_NODE(CheckNumber)
UNIMPLEMENTED_NODE(CheckSmi)
UNIMPLEMENTED_NODE(CheckString, check_type_)
UNIMPLEMENTED_NODE(CheckSymbol, check_type_)
UNIMPLEMENTED_NODE(CheckValue)
UNIMPLEMENTED_NODE(CheckInstanceType, check_type_)
UNIMPLEMENTED_NODE(DebugBreak)
UNIMPLEMENTED_NODE(GeneratorStore)
UNIMPLEMENTED_NODE(JumpLoopPrologue, loop_depth_, unit_)
UNIMPLEMENTED_NODE(StoreMap)
UNIMPLEMENTED_NODE(StoreDoubleField)
UNIMPLEMENTED_NODE(StoreSignedIntDataViewElement, type_)
UNIMPLEMENTED_NODE(StoreDoubleDataViewElement)
UNIMPLEMENTED_NODE(StoreTaggedFieldNoWriteBarrier)
UNIMPLEMENTED_NODE(StoreTaggedFieldWithWriteBarrier)
UNIMPLEMENTED_NODE(IncreaseInterruptBudget)
UNIMPLEMENTED_NODE(ReduceInterruptBudget)
UNIMPLEMENTED_NODE(ThrowReferenceErrorIfHole)
UNIMPLEMENTED_NODE(ThrowSuperNotCalledIfHole)
UNIMPLEMENTED_NODE(ThrowSuperAlreadyCalledIfNotHole)
UNIMPLEMENTED_NODE(ThrowIfNotSuperConstructor)
UNIMPLEMENTED_NODE(BranchIfRootConstant)
UNIMPLEMENTED_NODE(BranchIfToBooleanTrue)
UNIMPLEMENTED_NODE(BranchIfReferenceCompare, operation_)
UNIMPLEMENTED_NODE(BranchIfInt32Compare, operation_)
UNIMPLEMENTED_NODE(BranchIfFloat64Compare, operation_)
UNIMPLEMENTED_NODE(BranchIfUndefinedOrNull)
UNIMPLEMENTED_NODE(BranchIfJSReceiver)
UNIMPLEMENTED_NODE(Switch)
UNIMPLEMENTED_NODE(Jump)
UNIMPLEMENTED_NODE(JumpLoop)
UNIMPLEMENTED_NODE(JumpToInlined)
UNIMPLEMENTED_NODE(JumpFromInlined)
UNIMPLEMENTED_NODE(Abort)
UNIMPLEMENTED_NODE(Return)
UNIMPLEMENTED_NODE(Deopt)

}  // namespace maglev
}  // namespace internal
}  // namespace v8
