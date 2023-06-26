// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_IR_H_
#define V8_MAGLEV_MAGLEV_IR_H_

#include "src/base/bit-field.h"
#include "src/base/bits.h"
#include "src/base/discriminated-union.h"
#include "src/base/enum-set.h"
#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/base/small-vector.h"
#include "src/base/threaded-list.h"
#include "src/codegen/label.h"
#include "src/codegen/machine-type.h"
#include "src/codegen/reglist.h"
#include "src/codegen/source-position.h"
#include "src/common/globals.h"
#include "src/common/operation.h"
#include "src/compiler/access-info.h"
#include "src/compiler/backend/instruction.h"
#include "src/compiler/feedback-source.h"
#include "src/compiler/heap-refs.h"
// TODO(dmercadier): move the Turboshaft utils functions to shared code (in
// particular, any_of, which is the reason we're including this Turboshaft
// header)
#include "src/compiler/turboshaft/snapshot-table.h"
#include "src/compiler/turboshaft/utils.h"
#include "src/deoptimizer/deoptimize-reason.h"
#include "src/interpreter/bytecode-flags.h"
#include "src/interpreter/bytecode-register.h"
#include "src/maglev/maglev-compilation-unit.h"
#include "src/objects/smi.h"
#include "src/roots/roots.h"
#include "src/utils/utils.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

enum Condition : int;

namespace maglev {

class BasicBlock;
class ProcessingState;
class MaglevAssembler;
class MaglevCodeGenState;
class MaglevCompilationUnit;
class MaglevGraphLabeller;
class MaglevVregAllocationState;
class CompactInterpreterFrameState;
class MergePointInterpreterFrameState;

// Nodes are either
// 1. side-effecting or value-holding SSA nodes in the body of basic blocks, or
// 2. Control nodes that store the control flow at the end of basic blocks, and
// form a separate node hierarchy to non-control nodes.
//
// The macro lists below must match the node class hierarchy.

#define GENERIC_OPERATIONS_NODE_LIST(V) \
  V(GenericAdd)                         \
  V(GenericSubtract)                    \
  V(GenericMultiply)                    \
  V(GenericDivide)                      \
  V(GenericModulus)                     \
  V(GenericExponentiate)                \
  V(GenericBitwiseAnd)                  \
  V(GenericBitwiseOr)                   \
  V(GenericBitwiseXor)                  \
  V(GenericShiftLeft)                   \
  V(GenericShiftRight)                  \
  V(GenericShiftRightLogical)           \
  V(GenericBitwiseNot)                  \
  V(GenericNegate)                      \
  V(GenericIncrement)                   \
  V(GenericDecrement)                   \
  V(GenericEqual)                       \
  V(GenericStrictEqual)                 \
  V(GenericLessThan)                    \
  V(GenericLessThanOrEqual)             \
  V(GenericGreaterThan)                 \
  V(GenericGreaterThanOrEqual)

#define INT32_OPERATIONS_NODE_LIST(V) \
  V(Int32AddWithOverflow)             \
  V(Int32SubtractWithOverflow)        \
  V(Int32MultiplyWithOverflow)        \
  V(Int32DivideWithOverflow)          \
  V(Int32ModulusWithOverflow)         \
  V(Int32BitwiseAnd)                  \
  V(Int32BitwiseOr)                   \
  V(Int32BitwiseXor)                  \
  V(Int32ShiftLeft)                   \
  V(Int32ShiftRight)                  \
  V(Int32ShiftRightLogical)           \
  V(Int32BitwiseNot)                  \
  V(Int32NegateWithOverflow)          \
  V(Int32IncrementWithOverflow)       \
  V(Int32DecrementWithOverflow)       \
  V(Int32Equal)                       \
  V(Int32StrictEqual)                 \
  V(Int32LessThan)                    \
  V(Int32LessThanOrEqual)             \
  V(Int32GreaterThan)                 \
  V(Int32GreaterThanOrEqual)

#define FLOAT64_OPERATIONS_NODE_LIST(V) \
  V(Float64Add)                         \
  V(Float64Subtract)                    \
  V(Float64Multiply)                    \
  V(Float64Divide)                      \
  V(Float64Exponentiate)                \
  V(Float64Modulus)                     \
  V(Float64Negate)                      \
  V(Float64Round)                       \
  V(Float64Equal)                       \
  V(Float64StrictEqual)                 \
  V(Float64LessThan)                    \
  V(Float64LessThanOrEqual)             \
  V(Float64GreaterThan)                 \
  V(Float64GreaterThanOrEqual)          \
  V(Float64Ieee754Unary)

#define CONSTANT_VALUE_NODE_LIST(V) \
  V(Constant)                       \
  V(ExternalConstant)               \
  V(Float64Constant)                \
  V(Int32Constant)                  \
  V(RootConstant)                   \
  V(SmiConstant)

#define INLINE_BUILTIN_NODE_LIST(V) \
  V(BuiltinStringFromCharCode)      \
  V(BuiltinStringPrototypeCharCodeOrCodePointAt)

#define VALUE_NODE_LIST(V)                   \
  V(Identity)                                \
  V(AllocateRaw)                             \
  V(Call)                                    \
  V(CallBuiltin)                             \
  V(CallRuntime)                             \
  V(CallWithArrayLike)                       \
  V(CallWithSpread)                          \
  V(CallKnownApiFunction)                    \
  V(CallKnownJSFunction)                     \
  V(CallSelf)                                \
  V(Construct)                               \
  V(CheckConstructResult)                    \
  V(ConstructWithSpread)                     \
  V(ConvertReceiver)                         \
  V(ConvertHoleToUndefined)                  \
  V(CreateArrayLiteral)                      \
  V(CreateShallowArrayLiteral)               \
  V(CreateObjectLiteral)                     \
  V(CreateShallowObjectLiteral)              \
  V(CreateFunctionContext)                   \
  V(CreateClosure)                           \
  V(FastCreateClosure)                       \
  V(CreateRegExpLiteral)                     \
  V(DeleteProperty)                          \
  V(EnsureWritableFastElements)              \
  V(FoldedAllocation)                        \
  V(ForInPrepare)                            \
  V(ForInNext)                               \
  V(GeneratorRestoreRegister)                \
  V(GetIterator)                             \
  V(GetSecondReturnedValue)                  \
  V(GetTemplateObject)                       \
  V(HasInPrototypeChain)                     \
  V(InitialValue)                            \
  V(LoadPolymorphicDoubleField)              \
  V(LoadPolymorphicTaggedField)              \
  V(LoadTaggedField)                         \
  V(LoadDoubleField)                         \
  V(LoadTaggedFieldByFieldIndex)             \
  V(LoadFixedArrayElement)                   \
  V(LoadFixedDoubleArrayElement)             \
  V(LoadHoleyFixedDoubleArrayElement)        \
  V(LoadSignedIntDataViewElement)            \
  V(LoadDoubleDataViewElement)               \
  V(LoadSignedIntTypedArrayElement)          \
  V(LoadSignedIntTypedArrayElementNoDeopt)   \
  V(LoadUnsignedIntTypedArrayElement)        \
  V(LoadUnsignedIntTypedArrayElementNoDeopt) \
  V(LoadDoubleTypedArrayElement)             \
  V(LoadDoubleTypedArrayElementNoDeopt)      \
  V(LoadEnumCacheLength)                     \
  V(LoadGlobal)                              \
  V(LoadNamedGeneric)                        \
  V(LoadNamedFromSuperGeneric)               \
  V(MaybeGrowAndEnsureWritableFastElements)  \
  V(SetNamedGeneric)                         \
  V(DefineNamedOwnGeneric)                   \
  V(StoreInArrayLiteralGeneric)              \
  V(StoreGlobal)                             \
  V(GetKeyedGeneric)                         \
  V(SetKeyedGeneric)                         \
  V(DefineKeyedOwnGeneric)                   \
  V(Phi)                                     \
  V(RegisterInput)                           \
  V(CheckedSmiTagInt32)                      \
  V(CheckedSmiTagUint32)                     \
  V(UnsafeSmiTag)                            \
  V(CheckedSmiUntag)                         \
  V(UnsafeSmiUntag)                          \
  V(CheckedInternalizedString)               \
  V(CheckedObjectToIndex)                    \
  V(CheckedTruncateNumberOrOddballToInt32)   \
  V(CheckedInt32ToUint32)                    \
  V(CheckedUint32ToInt32)                    \
  V(ChangeInt32ToFloat64)                    \
  V(ChangeUint32ToFloat64)                   \
  V(CheckedTruncateFloat64ToInt32)           \
  V(CheckedTruncateFloat64ToUint32)          \
  V(TruncateNumberOrOddballToInt32)          \
  V(TruncateUint32ToInt32)                   \
  V(TruncateFloat64ToInt32)                  \
  V(UnsafeTruncateUint32ToInt32)             \
  V(UnsafeTruncateFloat64ToInt32)            \
  V(Int32ToUint8Clamped)                     \
  V(Uint32ToUint8Clamped)                    \
  V(Float64ToUint8Clamped)                   \
  V(CheckedNumberToUint8Clamped)             \
  V(Int32ToNumber)                           \
  V(Uint32ToNumber)                          \
  V(Float64ToTagged)                         \
  V(HoleyFloat64ToTagged)                    \
  V(CheckedSmiTagFloat64)                    \
  V(CheckedNumberOrOddballToFloat64)         \
  V(UncheckedNumberOrOddballToFloat64)       \
  V(CheckedHoleyFloat64ToFloat64)            \
  V(HoleyFloat64ToMaybeNanFloat64)           \
  V(LogicalNot)                              \
  V(SetPendingMessage)                       \
  V(StringAt)                                \
  V(StringEqual)                             \
  V(StringLength)                            \
  V(StringConcat)                            \
  V(ToBoolean)                               \
  V(ToBooleanLogicalNot)                     \
  V(TaggedEqual)                             \
  V(TaggedNotEqual)                          \
  V(TestInstanceOf)                          \
  V(TestUndetectable)                        \
  V(TestTypeOf)                              \
  V(ToName)                                  \
  V(ToNumberOrNumeric)                       \
  V(ToObject)                                \
  V(ToString)                                \
  V(NumberToString)                          \
  CONSTANT_VALUE_NODE_LIST(V)                \
  INT32_OPERATIONS_NODE_LIST(V)              \
  FLOAT64_OPERATIONS_NODE_LIST(V)            \
  GENERIC_OPERATIONS_NODE_LIST(V)            \
  INLINE_BUILTIN_NODE_LIST(V)

#define GAP_MOVE_NODE_LIST(V) \
  V(ConstantGapMove)          \
  V(GapMove)

#define NODE_LIST(V)                        \
  V(AssertInt32)                            \
  V(CheckDynamicValue)                      \
  V(CheckInt32IsSmi)                        \
  V(CheckUint32IsSmi)                       \
  V(CheckHoleyFloat64IsSmi)                 \
  V(CheckHeapObject)                        \
  V(CheckInt32Condition)                    \
  V(CheckFixedArrayNonEmpty)                \
  V(CheckJSDataViewBounds)                  \
  V(CheckJSTypedArrayBounds)                \
  V(CheckMaps)                              \
  V(CheckMapsWithMigration)                 \
  V(CheckNumber)                            \
  V(CheckSmi)                               \
  V(CheckString)                            \
  V(CheckSymbol)                            \
  V(CheckValue)                             \
  V(CheckValueEqualsInt32)                  \
  V(CheckValueEqualsFloat64)                \
  V(CheckValueEqualsString)                 \
  V(CheckInstanceType)                      \
  V(DebugBreak)                             \
  V(FunctionEntryStackCheck)                \
  V(GeneratorStore)                         \
  V(TryOnStackReplacement)                  \
  V(StoreMap)                               \
  V(StoreDoubleField)                       \
  V(StoreFixedArrayElementWithWriteBarrier) \
  V(StoreFixedArrayElementNoWriteBarrier)   \
  V(StoreFixedDoubleArrayElement)           \
  V(StoreFloat64)                           \
  V(StoreIntTypedArrayElement)              \
  V(StoreIntTypedArrayElementNoDeopt)       \
  V(StoreDoubleTypedArrayElement)           \
  V(StoreDoubleTypedArrayElementNoDeopt)    \
  V(StoreSignedIntDataViewElement)          \
  V(StoreDoubleDataViewElement)             \
  V(StoreTaggedFieldNoWriteBarrier)         \
  V(StoreTaggedFieldWithWriteBarrier)       \
  V(ReduceInterruptBudgetForLoop)           \
  V(ReduceInterruptBudgetForReturn)         \
  V(ThrowReferenceErrorIfHole)              \
  V(ThrowSuperNotCalledIfHole)              \
  V(ThrowSuperAlreadyCalledIfNotHole)       \
  V(ThrowIfNotCallable)                     \
  V(ThrowIfNotSuperConstructor)             \
  V(TransitionElementsKindOrCheckMap)       \
  V(UpdateJSArrayLength)                    \
  GAP_MOVE_NODE_LIST(V)                     \
  VALUE_NODE_LIST(V)

#define BRANCH_CONTROL_NODE_LIST(V) \
  V(BranchIfRootConstant)           \
  V(BranchIfToBooleanTrue)          \
  V(BranchIfInt32ToBooleanTrue)     \
  V(BranchIfFloat64ToBooleanTrue)   \
  V(BranchIfFloat64IsHole)          \
  V(BranchIfReferenceCompare)       \
  V(BranchIfInt32Compare)           \
  V(BranchIfFloat64Compare)         \
  V(BranchIfUndefinedOrNull)        \
  V(BranchIfUndetectable)           \
  V(BranchIfJSReceiver)             \
  V(BranchIfTypeOf)

#define CONDITIONAL_CONTROL_NODE_LIST(V) \
  V(Switch)                              \
  BRANCH_CONTROL_NODE_LIST(V)

#define UNCONDITIONAL_CONTROL_NODE_LIST(V) \
  V(Jump)                                  \
  V(JumpLoop)

#define TERMINAL_CONTROL_NODE_LIST(V) \
  V(Abort)                            \
  V(Return)                           \
  V(Deopt)

#define CONTROL_NODE_LIST(V)       \
  TERMINAL_CONTROL_NODE_LIST(V)    \
  CONDITIONAL_CONTROL_NODE_LIST(V) \
  UNCONDITIONAL_CONTROL_NODE_LIST(V)

#define NODE_BASE_LIST(V) \
  NODE_LIST(V)            \
  CONTROL_NODE_LIST(V)

// Define the opcode enum.
#define DEF_OPCODES(type) k##type,
enum class Opcode : uint16_t { NODE_BASE_LIST(DEF_OPCODES) };
#undef DEF_OPCODES
#define PLUS_ONE(type) +1
static constexpr int kOpcodeCount = NODE_BASE_LIST(PLUS_ONE);
static constexpr Opcode kFirstOpcode = static_cast<Opcode>(0);
static constexpr Opcode kLastOpcode = static_cast<Opcode>(kOpcodeCount - 1);
#undef PLUS_ONE

const char* OpcodeToString(Opcode opcode);
inline std::ostream& operator<<(std::ostream& os, Opcode opcode) {
  return os << OpcodeToString(opcode);
}

#define V(Name) Opcode::k##Name,
static constexpr Opcode kFirstValueNodeOpcode =
    std::min({VALUE_NODE_LIST(V) kLastOpcode});
static constexpr Opcode kLastValueNodeOpcode =
    std::max({VALUE_NODE_LIST(V) kFirstOpcode});
static constexpr Opcode kFirstConstantNodeOpcode =
    std::min({CONSTANT_VALUE_NODE_LIST(V) kLastOpcode});
static constexpr Opcode kLastConstantNodeOpcode =
    std::max({CONSTANT_VALUE_NODE_LIST(V) kFirstOpcode});
static constexpr Opcode kFirstGapMoveNodeOpcode =
    std::min({GAP_MOVE_NODE_LIST(V) kLastOpcode});
static constexpr Opcode kLastGapMoveNodeOpcode =
    std::max({GAP_MOVE_NODE_LIST(V) kFirstOpcode});

static constexpr Opcode kFirstNodeOpcode = std::min({NODE_LIST(V) kLastOpcode});
static constexpr Opcode kLastNodeOpcode = std::max({NODE_LIST(V) kFirstOpcode});

static constexpr Opcode kFirstBranchControlNodeOpcode =
    std::min({BRANCH_CONTROL_NODE_LIST(V) kLastOpcode});
static constexpr Opcode kLastBranchControlNodeOpcode =
    std::max({BRANCH_CONTROL_NODE_LIST(V) kFirstOpcode});

static constexpr Opcode kFirstConditionalControlNodeOpcode =
    std::min({CONDITIONAL_CONTROL_NODE_LIST(V) kLastOpcode});
static constexpr Opcode kLastConditionalControlNodeOpcode =
    std::max({CONDITIONAL_CONTROL_NODE_LIST(V) kFirstOpcode});

static constexpr Opcode kLastUnconditionalControlNodeOpcode =
    std::max({UNCONDITIONAL_CONTROL_NODE_LIST(V) kFirstOpcode});
static constexpr Opcode kFirstUnconditionalControlNodeOpcode =
    std::min({UNCONDITIONAL_CONTROL_NODE_LIST(V) kLastOpcode});

static constexpr Opcode kLastTerminalControlNodeOpcode =
    std::max({TERMINAL_CONTROL_NODE_LIST(V) kFirstOpcode});
static constexpr Opcode kFirstTerminalControlNodeOpcode =
    std::min({TERMINAL_CONTROL_NODE_LIST(V) kLastOpcode});

static constexpr Opcode kFirstControlNodeOpcode =
    std::min({CONTROL_NODE_LIST(V) kLastOpcode});
static constexpr Opcode kLastControlNodeOpcode =
    std::max({CONTROL_NODE_LIST(V) kFirstOpcode});
#undef V

constexpr bool IsValueNode(Opcode opcode) {
  return kFirstValueNodeOpcode <= opcode && opcode <= kLastValueNodeOpcode;
}
constexpr bool IsConstantNode(Opcode opcode) {
  return kFirstConstantNodeOpcode <= opcode &&
         opcode <= kLastConstantNodeOpcode;
}
constexpr bool IsGapMoveNode(Opcode opcode) {
  return kFirstGapMoveNodeOpcode <= opcode && opcode <= kLastGapMoveNodeOpcode;
}
constexpr bool IsControlNode(Opcode opcode) {
  return kFirstControlNodeOpcode <= opcode && opcode <= kLastControlNodeOpcode;
}
constexpr bool IsBranchControlNode(Opcode opcode) {
  return kFirstBranchControlNodeOpcode <= opcode &&
         opcode <= kLastBranchControlNodeOpcode;
}
constexpr bool IsConditionalControlNode(Opcode opcode) {
  return kFirstConditionalControlNodeOpcode <= opcode &&
         opcode <= kLastConditionalControlNodeOpcode;
}
constexpr bool IsUnconditionalControlNode(Opcode opcode) {
  return kFirstUnconditionalControlNodeOpcode <= opcode &&
         opcode <= kLastUnconditionalControlNodeOpcode;
}
constexpr bool IsTerminalControlNode(Opcode opcode) {
  return kFirstTerminalControlNodeOpcode <= opcode &&
         opcode <= kLastTerminalControlNodeOpcode;
}

// Forward-declare NodeBase sub-hierarchies.
class Node;
class ControlNode;
class ConditionalControlNode;
class BranchControlNode;
class UnconditionalControlNode;
class TerminalControlNode;
class ValueNode;

enum class ValueRepresentation : uint8_t {
  kTagged,
  kInt32,
  kUint32,
  kFloat64,
  kHoleyFloat64,
  kWord64
};

inline constexpr bool IsDoubleRepresentation(ValueRepresentation repr) {
  return repr == ValueRepresentation::kFloat64 ||
         repr == ValueRepresentation::kHoleyFloat64;
}

// The intersection (using `&`) of any two NodeTypes must be a valid NodeType
// (possibly "kUnknown").
// All heap object types include the heap object bit, so that they can be
// checked for AnyHeapObject with a single bit check.
#define NODE_TYPE_LIST(V)                                         \
  V(Unknown, 0)                                                   \
  V(NumberOrOddball, (1 << 1))                                    \
  V(Number, (1 << 2) | kNumberOrOddball)                          \
  V(ObjectWithKnownMap, (1 << 3))                                 \
  V(Smi, (1 << 4) | kObjectWithKnownMap | kNumber)                \
  V(AnyHeapObject, (1 << 5))                                      \
  V(Oddball, (1 << 6) | kAnyHeapObject | kNumberOrOddball)        \
  V(Boolean, (1 << 7) | kOddball)                                 \
  V(Name, (1 << 8) | kAnyHeapObject)                              \
  V(String, (1 << 9) | kName)                                     \
  V(InternalizedString, (1 << 10) | kString)                      \
  V(Symbol, (1 << 11) | kName)                                    \
  V(JSReceiver, (1 << 12) | kAnyHeapObject)                       \
  V(Callable, (1 << 13) | kJSReceiver | kAnyHeapObject)           \
  V(HeapObjectWithKnownMap, kObjectWithKnownMap | kAnyHeapObject) \
  V(HeapNumber, kHeapObjectWithKnownMap | kNumber)                \
  V(JSReceiverWithKnownMap, kJSReceiver | kHeapObjectWithKnownMap)

enum class NodeType : uint16_t {
#define DEFINE_NODE_TYPE(Name, Value) k##Name = Value,
  NODE_TYPE_LIST(DEFINE_NODE_TYPE)
#undef DEFINE_NODE_TYPE
};

inline NodeType CombineType(NodeType left, NodeType right) {
  return static_cast<NodeType>(static_cast<int>(left) |
                               static_cast<int>(right));
}
inline NodeType IntersectType(NodeType left, NodeType right) {
  return static_cast<NodeType>(static_cast<int>(left) &
                               static_cast<int>(right));
}
inline bool NodeTypeIs(NodeType type, NodeType to_check) {
  int right = static_cast<int>(to_check);
  return (static_cast<int>(type) & right) == right;
}

#define DEFINE_NODE_TYPE_CHECK(Type, _)         \
  inline bool NodeTypeIs##Type(NodeType type) { \
    return NodeTypeIs(type, NodeType::k##Type); \
  }
NODE_TYPE_LIST(DEFINE_NODE_TYPE_CHECK)
#undef DEFINE_NODE_TYPE_CHECK

enum class TaggedToFloat64ConversionType : uint8_t {
  kOnlyNumber,
  kNumberOrOddball,
};

constexpr Condition ConditionFor(Operation cond);
constexpr Condition ConditionForNaN();

bool FromConstantToBool(LocalIsolate* local_isolate, ValueNode* node);
bool FromConstantToBool(MaglevAssembler* masm, ValueNode* node);

inline int ExternalArrayElementSize(const ExternalArrayType element_type) {
  switch (element_type) {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) \
  case kExternal##Type##Array:                    \
    DCHECK_LE(sizeof(ctype), 8);                  \
    return sizeof(ctype);
    TYPED_ARRAYS(TYPED_ARRAY_CASE)
    default:
      UNREACHABLE();
#undef TYPED_ARRAY_CASE
  }
}

inline int ElementsKindSize(ElementsKind element_kind) {
  switch (element_kind) {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) \
  case TYPE##_ELEMENTS:                           \
    DCHECK_LE(sizeof(ctype), 8);                  \
    return sizeof(ctype);
    TYPED_ARRAYS(TYPED_ARRAY_CASE)
    default:
      UNREACHABLE();
#undef TYPED_ARRAY_CASE
  }
}

inline std::ostream& operator<<(std::ostream& os,
                                const ValueRepresentation& repr) {
  switch (repr) {
    case ValueRepresentation::kTagged:
      return os << "Tagged";
    case ValueRepresentation::kInt32:
      return os << "Int32";
    case ValueRepresentation::kUint32:
      return os << "Uint32";
    case ValueRepresentation::kFloat64:
      return os << "Float64";
    case ValueRepresentation::kHoleyFloat64:
      return os << "HoleyFloat64";
    case ValueRepresentation::kWord64:
      return os << "Word64";
  }
}

inline std::ostream& operator<<(
    std::ostream& os, const TaggedToFloat64ConversionType& conversion_type) {
  switch (conversion_type) {
    case TaggedToFloat64ConversionType::kOnlyNumber:
      return os << "Number";
    case TaggedToFloat64ConversionType::kNumberOrOddball:
      return os << "NumberOrOddball";
  }
}

inline bool HasOnlyJSTypedArrayMaps(base::Vector<const compiler::MapRef> maps) {
  for (compiler::MapRef map : maps) {
    if (!map.IsJSTypedArrayMap()) return false;
  }
  return true;
}

inline bool HasOnlyJSArrayMaps(base::Vector<const compiler::MapRef> maps) {
  for (compiler::MapRef map : maps) {
    if (!map.IsJSArrayMap()) return false;
  }
  return true;
}

inline bool HasOnlyJSObjectMaps(base::Vector<const compiler::MapRef> maps) {
  for (compiler::MapRef map : maps) {
    if (!map.IsJSObjectMap()) return false;
  }
  return true;
}

inline bool HasOnlyStringMaps(base::Vector<const compiler::MapRef> maps) {
  for (compiler::MapRef map : maps) {
    if (!map.IsStringMap()) return false;
  }
  return true;
}

inline bool HasOnlyNumberMaps(base::Vector<const compiler::MapRef> maps) {
  for (compiler::MapRef map : maps) {
    if (map.instance_type() != HEAP_NUMBER_TYPE) return false;
  }
  return true;
}

#define DEF_FORWARD_DECLARATION(type, ...) class type;
NODE_BASE_LIST(DEF_FORWARD_DECLARATION)
#undef DEF_FORWARD_DECLARATION

using NodeIdT = uint32_t;
static constexpr uint32_t kInvalidNodeId = 0;
static constexpr uint32_t kFirstValidNodeId = 1;

// Represents either a direct BasicBlock pointer, or an entry in a list of
// unresolved BasicBlockRefs which will be mutated (in place) at some point into
// direct BasicBlock pointers.
class BasicBlockRef {
  struct BasicBlockRefBuilder;

 public:
  BasicBlockRef() : next_ref_(nullptr) {
#ifdef DEBUG
    state_ = kRefList;
#endif
  }
  explicit BasicBlockRef(BasicBlock* block) : block_ptr_(block) {
#ifdef DEBUG
    state_ = kBlockPointer;
#endif
  }

  // Refs can't be copied or moved, since they are referenced by `this` pointer
  // in the ref list.
  BasicBlockRef(const BasicBlockRef&) = delete;
  BasicBlockRef(BasicBlockRef&&) = delete;
  BasicBlockRef& operator=(const BasicBlockRef&) = delete;
  BasicBlockRef& operator=(BasicBlockRef&&) = delete;

  // Construct a new ref-list mode BasicBlockRef and add it to the given ref
  // list.
  explicit BasicBlockRef(BasicBlockRef* ref_list_head) : BasicBlockRef() {
    BasicBlockRef* old_next_ptr = MoveToRefList(ref_list_head);
    USE(old_next_ptr);
    DCHECK_NULL(old_next_ptr);
  }

  // Change this ref to a direct basic block pointer, returning the old "next"
  // pointer of the current ref.
  BasicBlockRef* SetToBlockAndReturnNext(BasicBlock* block) {
    DCHECK_EQ(state_, kRefList);

    BasicBlockRef* old_next_ptr = next_ref_;
    block_ptr_ = block;
#ifdef DEBUG
    state_ = kBlockPointer;
#endif
    return old_next_ptr;
  }

  // Reset this ref list to null, returning the old ref list (i.e. the old
  // "next" pointer).
  BasicBlockRef* Reset() {
    DCHECK_EQ(state_, kRefList);

    BasicBlockRef* old_next_ptr = next_ref_;
    next_ref_ = nullptr;
    return old_next_ptr;
  }

  // Move this ref to the given ref list, returning the old "next" pointer of
  // the current ref.
  BasicBlockRef* MoveToRefList(BasicBlockRef* ref_list_head) {
    DCHECK_EQ(state_, kRefList);
    DCHECK_EQ(ref_list_head->state_, kRefList);

    BasicBlockRef* old_next_ptr = next_ref_;
    next_ref_ = ref_list_head->next_ref_;
    ref_list_head->next_ref_ = this;
    return old_next_ptr;
  }

  void Bind(BasicBlock* block) {
    DCHECK_EQ(state_, kRefList);

    BasicBlockRef* next_ref = SetToBlockAndReturnNext(block);
    while (next_ref != nullptr) {
      next_ref = next_ref->SetToBlockAndReturnNext(block);
    }
    DCHECK_EQ(block_ptr(), block);
  }

  BasicBlock* block_ptr() const {
    DCHECK_EQ(state_, kBlockPointer);
    return block_ptr_;
  }

  BasicBlockRef* next_ref() const {
    DCHECK_EQ(state_, kRefList);
    return next_ref_;
  }

  bool has_ref() const {
    DCHECK_EQ(state_, kRefList);
    return next_ref_ != nullptr;
  }

 private:
  union {
    BasicBlock* block_ptr_;
    BasicBlockRef* next_ref_;
  };
#ifdef DEBUG
  enum { kBlockPointer, kRefList } state_;
#endif  // DEBUG
};

class OpProperties {
 public:
  constexpr bool is_call() const {
    // Only returns true for non-deferred calls. Use `is_any_call` to check
    // deferred calls as well.
    return kIsCallBit::decode(bitfield_);
  }
  constexpr bool is_any_call() const { return is_call() || is_deferred_call(); }
  constexpr bool can_eager_deopt() const {
    return kCanEagerDeoptBit::decode(bitfield_);
  }
  constexpr bool can_lazy_deopt() const {
    return kCanLazyDeoptBit::decode(bitfield_);
  }
  constexpr bool can_deopt() const {
    return can_eager_deopt() || can_lazy_deopt();
  }
  constexpr bool can_throw() const {
    return kCanThrowBit::decode(bitfield_) && can_lazy_deopt();
  }
  constexpr bool can_read() const { return kCanReadBit::decode(bitfield_); }
  constexpr bool can_write() const { return kCanWriteBit::decode(bitfield_); }
  constexpr bool can_allocate() const {
    return kCanAllocate::decode(bitfield_);
  }
  constexpr ValueRepresentation value_representation() const {
    return kValueRepresentationBits::decode(bitfield_);
  }
  constexpr bool is_tagged() const {
    return value_representation() == ValueRepresentation::kTagged;
  }
  constexpr bool is_conversion() const {
    return kIsConversionBit::decode(bitfield_);
  }
  constexpr bool needs_register_snapshot() const {
    return kNeedsRegisterSnapshotBit::decode(bitfield_);
  }
  constexpr bool is_pure() const {
    return (bitfield_ & kPureMask) == kPureValue;
  }
  constexpr bool is_required_when_unused() {
    if (is_conversion()) {
      // Calls in conversions are not counted as a side-effect as far as
      // is_required_when_unused is concerned, since they should always be to
      // the Allocate builtin.
      return can_write() || can_throw() || can_deopt();
    } else {
      return can_write() || can_throw() || can_deopt() || is_any_call();
    }
  }

  constexpr OpProperties operator|(const OpProperties& that) {
    return OpProperties(bitfield_ | that.bitfield_);
  }

  static constexpr OpProperties Pure() { return OpProperties(kPureValue); }
  static constexpr OpProperties Call() {
    return OpProperties(kIsCallBit::encode(true));
  }
  static constexpr OpProperties EagerDeopt() {
    return OpProperties(kCanEagerDeoptBit::encode(true));
  }
  static constexpr OpProperties LazyDeopt() {
    return OpProperties(kCanLazyDeoptBit::encode(true));
  }
  static constexpr OpProperties CanThrow() {
    return OpProperties(kCanThrowBit::encode(true)) | LazyDeopt();
  }
  static constexpr OpProperties CanRead() {
    return OpProperties(kCanReadBit::encode(true));
  }
  static constexpr OpProperties CanWrite() {
    return OpProperties(kCanWriteBit::encode(true));
  }
  static constexpr OpProperties CanAllocate() {
    return OpProperties(kCanAllocate::encode(true));
  }
  static constexpr OpProperties TaggedValue() {
    return OpProperties(
        kValueRepresentationBits::encode(ValueRepresentation::kTagged));
  }
  static constexpr OpProperties ExternalReference() {
    return OpProperties(
        kValueRepresentationBits::encode(ValueRepresentation::kWord64));
  }
  static constexpr OpProperties Int32() {
    return OpProperties(
        kValueRepresentationBits::encode(ValueRepresentation::kInt32));
  }
  static constexpr OpProperties Uint32() {
    return OpProperties(
        kValueRepresentationBits::encode(ValueRepresentation::kUint32));
  }
  static constexpr OpProperties Float64() {
    return OpProperties(
        kValueRepresentationBits::encode(ValueRepresentation::kFloat64));
  }
  static constexpr OpProperties HoleyFloat64() {
    return OpProperties(
        kValueRepresentationBits::encode(ValueRepresentation::kHoleyFloat64));
  }
  static constexpr OpProperties ConversionNode() {
    return OpProperties(kIsConversionBit::encode(true));
  }
  static constexpr OpProperties CanCallUserCode() {
    return AnySideEffects() | LazyDeopt() | CanThrow();
  }
  // Without auditing the call target, we must assume it can cause a lazy deopt
  // and throw. Use this when codegen calls runtime or a builtin, unless
  // certain that the target either doesn't throw or cannot deopt.
  // TODO(jgruber): Go through all nodes marked with this property and decide
  // whether to keep it (or remove either the lazy-deopt or throw flag).
  static constexpr OpProperties GenericRuntimeOrBuiltinCall() {
    return Call() | CanCallUserCode();
  }
  static constexpr OpProperties JSCall() { return Call() | CanCallUserCode(); }
  static constexpr OpProperties AnySideEffects() {
    return CanRead() | CanWrite() | CanAllocate();
  }
  static constexpr OpProperties DeferredCall() {
    // Operations with a deferred call need a snapshot of register state,
    // because they need to be able to push registers to save them, and annotate
    // the safepoint with information about which registers are tagged.
    return NeedsRegisterSnapshot();
  }

  constexpr explicit OpProperties(uint32_t bitfield) : bitfield_(bitfield) {}
  operator uint32_t() const { return bitfield_; }

  OpProperties WithNewValueRepresentation(ValueRepresentation new_repr) const {
    return OpProperties(kValueRepresentationBits::update(bitfield_, new_repr));
  }

  OpProperties WithoutDeopt() const {
    return OpProperties(kCanLazyDeoptBit::update(
        kCanEagerDeoptBit::update(bitfield_, false), false));
  }

 private:
  using kIsCallBit = base::BitField<bool, 0, 1>;
  using kCanEagerDeoptBit = kIsCallBit::Next<bool, 1>;
  using kCanLazyDeoptBit = kCanEagerDeoptBit::Next<bool, 1>;
  using kCanThrowBit = kCanLazyDeoptBit::Next<bool, 1>;
  using kCanReadBit = kCanThrowBit::Next<bool, 1>;
  using kCanWriteBit = kCanReadBit::Next<bool, 1>;
  using kCanAllocate = kCanWriteBit::Next<bool, 1>;
  using kValueRepresentationBits = kCanAllocate::Next<ValueRepresentation, 3>;
  using kIsConversionBit = kValueRepresentationBits::Next<bool, 1>;
  using kNeedsRegisterSnapshotBit = kIsConversionBit::Next<bool, 1>;

  static const uint32_t kPureMask =
      kCanReadBit::kMask | kCanWriteBit::kMask | kCanAllocate::kMask;
  static const uint32_t kPureValue = kCanReadBit::encode(false) |
                                     kCanWriteBit::encode(false) |
                                     kCanAllocate::encode(false);

  // NeedsRegisterSnapshot is only used for DeferredCall, and we rely on this in
  // `is_deferred_call` to detect deferred calls. If you need to use
  // NeedsRegisterSnapshot for something else that DeferredCalls, then you'll
  // have to update `is_any_call`.
  static constexpr OpProperties NeedsRegisterSnapshot() {
    return OpProperties(kNeedsRegisterSnapshotBit::encode(true));
  }

  const uint32_t bitfield_;

 public:
  static const size_t kSize = kNeedsRegisterSnapshotBit::kLastUsedBit + 1;

  constexpr bool is_deferred_call() const {
    // Currently, there is no kDeferredCall bit, but DeferredCall only sets a
    // single bit: kNeedsRegisterSnapShot. If this static assert breaks, it
    // means that you added additional properties to DeferredCall, and you
    // should update this function accordingly.
    static_assert(DeferredCall().bitfield_ ==
                  kNeedsRegisterSnapshotBit::encode(true));
    return needs_register_snapshot();
  }
};

constexpr inline OpProperties StaticPropertiesForOpcode(Opcode opcode);

class ValueLocation {
 public:
  ValueLocation() = default;

  template <typename... Args>
  void SetUnallocated(Args&&... args) {
    DCHECK(operand_.IsInvalid());
    operand_ = compiler::UnallocatedOperand(args...);
  }

  template <typename... Args>
  void SetAllocated(Args&&... args) {
    DCHECK(operand_.IsUnallocated());
    operand_ = compiler::AllocatedOperand(args...);
  }

  // Only to be used on inputs that inherit allocation.
  void InjectLocation(compiler::InstructionOperand location) {
    operand_ = location;
  }

  // We use USED_AT_START to indicate that the input will be clobbered.
  bool Cloberred() {
    DCHECK(operand_.IsUnallocated());
    return compiler::UnallocatedOperand::cast(operand_).IsUsedAtStart();
  }

  template <typename... Args>
  void SetConstant(Args&&... args) {
    DCHECK(operand_.IsUnallocated());
    operand_ = compiler::ConstantOperand(args...);
  }

  Register AssignedGeneralRegister() const {
    DCHECK(!IsDoubleRegister());
    return compiler::AllocatedOperand::cast(operand_).GetRegister();
  }

  DoubleRegister AssignedDoubleRegister() const {
    DCHECK(IsDoubleRegister());
    return compiler::AllocatedOperand::cast(operand_).GetDoubleRegister();
  }

  bool IsAnyRegister() const { return operand_.IsAnyRegister(); }
  bool IsGeneralRegister() const { return operand_.IsRegister(); }
  bool IsDoubleRegister() const { return operand_.IsDoubleRegister(); }

  const compiler::InstructionOperand& operand() const { return operand_; }
  const compiler::InstructionOperand& operand() { return operand_; }

 private:
  compiler::InstructionOperand operand_;
};

class InputLocation : public ValueLocation {
 public:
  NodeIdT next_use_id() const { return next_use_id_; }
  // Used in ValueNode::mark_use
  NodeIdT* get_next_use_id_address() { return &next_use_id_; }

 private:
  NodeIdT next_use_id_ = kInvalidNodeId;
};

class Input : public InputLocation {
 public:
  explicit Input(ValueNode* node) : node_(node) {}
  ValueNode* node() const { return node_; }

 private:
  ValueNode* node_;
};

class InterpretedDeoptFrame;
class InlinedArgumentsDeoptFrame;
class ConstructStubDeoptFrame;
class BuiltinContinuationDeoptFrame;
class DeoptFrame {
 public:
  enum class FrameType {
    kInterpretedFrame,
    kInlinedArgumentsFrame,
    kConstructStubFrame,
    kBuiltinContinuationFrame,
  };

  struct InterpretedFrameData {
    const MaglevCompilationUnit& unit;
    const CompactInterpreterFrameState* frame_state;
    ValueNode* closure;
    const BytecodeOffset bytecode_position;
    const SourcePosition source_position;
  };

  struct InlinedArgumentsFrameData {
    const MaglevCompilationUnit& unit;
    const BytecodeOffset bytecode_position;
    ValueNode* closure;
    const base::Vector<ValueNode*> arguments;
  };

  struct ConstructStubFrameData {
    const MaglevCompilationUnit& unit;
    const BytecodeOffset bytecode_position;
    const SourcePosition source_position;
    ValueNode* closure;
    ValueNode* receiver;
    const base::Vector<ValueNode*> arguments_without_receiver;
    ValueNode* context;
  };

  struct BuiltinContinuationFrameData {
    const Builtin builtin_id;
    const base::Vector<ValueNode*> parameters;
    ValueNode* context;
    compiler::OptionalJSFunctionRef maybe_js_target;
  };

  using FrameData = base::DiscriminatedUnion<
      FrameType, InterpretedFrameData, InlinedArgumentsFrameData,
      ConstructStubFrameData, BuiltinContinuationFrameData>;

  DeoptFrame(FrameData&& data, DeoptFrame* parent)
      : data_(std::move(data)), parent_(parent) {}

  FrameType type() const { return data_.tag(); }
  DeoptFrame* parent() { return parent_; }
  const DeoptFrame* parent() const { return parent_; }

  inline const InterpretedDeoptFrame& as_interpreted() const;
  inline const InlinedArgumentsDeoptFrame& as_inlined_arguments() const;
  inline const ConstructStubDeoptFrame& as_construct_stub() const;
  inline const BuiltinContinuationDeoptFrame& as_builtin_continuation() const;
  inline InterpretedDeoptFrame& as_interpreted();
  inline InlinedArgumentsDeoptFrame& as_inlined_arguments();
  inline ConstructStubDeoptFrame& as_construct_stub();
  inline BuiltinContinuationDeoptFrame& as_builtin_continuation();
  bool IsJsFrame() const {
    switch (data_.tag()) {
      case FrameType::kInterpretedFrame:
      case FrameType::kBuiltinContinuationFrame:
        return true;
      case FrameType::kConstructStubFrame:
      case FrameType::kInlinedArgumentsFrame:
        return false;
    }
  }

 protected:
  DeoptFrame(InterpretedFrameData&& data, DeoptFrame* parent)
      : data_(std::move(data)), parent_(parent) {}
  DeoptFrame(InlinedArgumentsFrameData&& data, DeoptFrame* parent)
      : data_(std::move(data)), parent_(parent) {}
  DeoptFrame(ConstructStubFrameData&& data, DeoptFrame* parent)
      : data_(std::move(data)), parent_(parent) {}
  DeoptFrame(BuiltinContinuationFrameData&& data, DeoptFrame* parent)
      : data_(std::move(data)), parent_(parent) {}

  FrameData data_;
  DeoptFrame* const parent_;
};

class InterpretedDeoptFrame : public DeoptFrame {
 public:
  InterpretedDeoptFrame(const MaglevCompilationUnit& unit,
                        const CompactInterpreterFrameState* frame_state,
                        ValueNode* closure, BytecodeOffset bytecode_position,
                        SourcePosition source_position, DeoptFrame* parent)
      : DeoptFrame(InterpretedFrameData{unit, frame_state, closure,
                                        bytecode_position, source_position},
                   parent) {}

  const MaglevCompilationUnit& unit() const { return data().unit; }
  const CompactInterpreterFrameState* frame_state() const {
    return data().frame_state;
  }
  ValueNode*& closure() { return data().closure; }
  ValueNode* closure() const { return data().closure; }
  BytecodeOffset bytecode_position() const { return data().bytecode_position; }
  SourcePosition source_position() const { return data().source_position; }

 private:
  InterpretedFrameData& data() { return data_.get<InterpretedFrameData>(); }
  const InterpretedFrameData& data() const {
    return data_.get<InterpretedFrameData>();
  }
};

// Make sure storing/passing deopt frames by value doesn't truncate them.
static_assert(sizeof(InterpretedDeoptFrame) == sizeof(DeoptFrame));

inline const InterpretedDeoptFrame& DeoptFrame::as_interpreted() const {
  DCHECK_EQ(type(), FrameType::kInterpretedFrame);
  return static_cast<const InterpretedDeoptFrame&>(*this);
}
inline InterpretedDeoptFrame& DeoptFrame::as_interpreted() {
  DCHECK_EQ(type(), FrameType::kInterpretedFrame);
  return static_cast<InterpretedDeoptFrame&>(*this);
}

class InlinedArgumentsDeoptFrame : public DeoptFrame {
 public:
  InlinedArgumentsDeoptFrame(const MaglevCompilationUnit& unit,
                             BytecodeOffset bytecode_position,
                             ValueNode* closure,
                             base::Vector<ValueNode*> arguments,
                             DeoptFrame* parent)
      : DeoptFrame(InlinedArgumentsFrameData{unit, bytecode_position, closure,
                                             arguments},
                   parent) {}

  const MaglevCompilationUnit& unit() const { return data().unit; }
  BytecodeOffset bytecode_position() const { return data().bytecode_position; }
  ValueNode*& closure() { return data().closure; }
  ValueNode* closure() const { return data().closure; }
  base::Vector<ValueNode*> arguments() const { return data().arguments; }

 private:
  InlinedArgumentsFrameData& data() {
    return data_.get<InlinedArgumentsFrameData>();
  }
  const InlinedArgumentsFrameData& data() const {
    return data_.get<InlinedArgumentsFrameData>();
  }
};

// Make sure storing/passing deopt frames by value doesn't truncate them.
static_assert(sizeof(InlinedArgumentsDeoptFrame) == sizeof(DeoptFrame));

inline const InlinedArgumentsDeoptFrame& DeoptFrame::as_inlined_arguments()
    const {
  DCHECK_EQ(type(), FrameType::kInlinedArgumentsFrame);
  return static_cast<const InlinedArgumentsDeoptFrame&>(*this);
}
inline InlinedArgumentsDeoptFrame& DeoptFrame::as_inlined_arguments() {
  DCHECK_EQ(type(), FrameType::kInlinedArgumentsFrame);
  return static_cast<InlinedArgumentsDeoptFrame&>(*this);
}

class ConstructStubDeoptFrame : public DeoptFrame {
 public:
  ConstructStubDeoptFrame(const MaglevCompilationUnit& unit,
                          BytecodeOffset bytecode_position,
                          SourcePosition source_position, ValueNode* closure,
                          ValueNode* receiver,
                          base::Vector<ValueNode*> arguments_without_receiver,
                          ValueNode* context, DeoptFrame* parent)
      : DeoptFrame(ConstructStubFrameData{unit, bytecode_position,
                                          source_position, closure, receiver,
                                          arguments_without_receiver, context},
                   parent) {}

  const MaglevCompilationUnit& unit() const { return data().unit; }
  BytecodeOffset bytecode_position() const { return data().bytecode_position; }
  ValueNode*& closure() { return data().closure; }
  ValueNode* closure() const { return data().closure; }
  ValueNode*& receiver() { return data().receiver; }
  ValueNode* receiver() const { return data().receiver; }
  base::Vector<ValueNode*> arguments_without_receiver() const {
    return data().arguments_without_receiver;
  }
  ValueNode*& context() { return data().context; }
  ValueNode* context() const { return data().context; }
  SourcePosition source_position() const { return data().source_position; }

 private:
  ConstructStubFrameData& data() { return data_.get<ConstructStubFrameData>(); }
  const ConstructStubFrameData& data() const {
    return data_.get<ConstructStubFrameData>();
  }
};

// Make sure storing/passing deopt frames by value doesn't truncate them.
static_assert(sizeof(ConstructStubDeoptFrame) == sizeof(DeoptFrame));

inline const ConstructStubDeoptFrame& DeoptFrame::as_construct_stub() const {
  DCHECK_EQ(type(), FrameType::kConstructStubFrame);
  return static_cast<const ConstructStubDeoptFrame&>(*this);
}

inline ConstructStubDeoptFrame& DeoptFrame::as_construct_stub() {
  DCHECK_EQ(type(), FrameType::kConstructStubFrame);
  return static_cast<ConstructStubDeoptFrame&>(*this);
}

class BuiltinContinuationDeoptFrame : public DeoptFrame {
 public:
  BuiltinContinuationDeoptFrame(Builtin builtin_id,
                                base::Vector<ValueNode*> parameters,
                                ValueNode* context,
                                compiler::OptionalJSFunctionRef maybe_js_target,
                                DeoptFrame* parent)
      : DeoptFrame(BuiltinContinuationFrameData{builtin_id, parameters, context,
                                                maybe_js_target},
                   parent) {}

  const Builtin& builtin_id() const { return data().builtin_id; }
  base::Vector<ValueNode*> parameters() const { return data().parameters; }
  ValueNode*& context() { return data().context; }
  ValueNode* context() const { return data().context; }
  bool is_javascript() const { return data().maybe_js_target.has_value(); }
  compiler::JSFunctionRef javascript_target() const {
    return data().maybe_js_target.value();
  }

 private:
  BuiltinContinuationFrameData& data() {
    return data_.get<BuiltinContinuationFrameData>();
  }
  const BuiltinContinuationFrameData& data() const {
    return data_.get<BuiltinContinuationFrameData>();
  }
};

// Make sure storing/passing deopt frames by value doesn't truncate them.
static_assert(sizeof(BuiltinContinuationDeoptFrame) == sizeof(DeoptFrame));

inline const BuiltinContinuationDeoptFrame&
DeoptFrame::as_builtin_continuation() const {
  DCHECK_EQ(type(), FrameType::kBuiltinContinuationFrame);
  return static_cast<const BuiltinContinuationDeoptFrame&>(*this);
}
inline BuiltinContinuationDeoptFrame& DeoptFrame::as_builtin_continuation() {
  DCHECK_EQ(type(), FrameType::kBuiltinContinuationFrame);
  return static_cast<BuiltinContinuationDeoptFrame&>(*this);
}

class DeoptInfo {
 protected:
  DeoptInfo(Zone* zone, const DeoptFrame top_frame,
            compiler::FeedbackSource feedback_to_update);

 public:
  DeoptFrame& top_frame() { return top_frame_; }
  const DeoptFrame& top_frame() const { return top_frame_; }
  const compiler::FeedbackSource& feedback_to_update() const {
    return feedback_to_update_;
  }

  InputLocation* input_locations() const { return input_locations_; }
  Label* deopt_entry_label() { return &deopt_entry_label_; }

  int translation_index() const { return translation_index_; }
  void set_translation_index(int index) { translation_index_ = index; }

 private:
  DeoptFrame top_frame_;
  const compiler::FeedbackSource feedback_to_update_;
  InputLocation* const input_locations_;
  Label deopt_entry_label_;
  int translation_index_ = -1;
};

struct RegisterSnapshot {
  RegList live_registers;
  RegList live_tagged_registers;
  DoubleRegList live_double_registers;
};

class EagerDeoptInfo : public DeoptInfo {
 public:
  EagerDeoptInfo(Zone* zone, const DeoptFrame top_frame,
                 compiler::FeedbackSource feedback_to_update)
      : DeoptInfo(zone, top_frame, feedback_to_update) {}

  DeoptimizeReason reason() const { return reason_; }
  void set_reason(DeoptimizeReason reason) { reason_ = reason; }

 private:
  DeoptimizeReason reason_ = DeoptimizeReason::kUnknown;
};

class LazyDeoptInfo : public DeoptInfo {
 public:
  LazyDeoptInfo(Zone* zone, const DeoptFrame top_frame,
                interpreter::Register result_location, int result_size,
                compiler::FeedbackSource feedback_to_update)
      : DeoptInfo(zone, top_frame, feedback_to_update),
        result_location_(result_location),
        bitfield_(
            DeoptingCallReturnPcField::encode(kUninitializedCallReturnPc) |
            ResultSizeField::encode(result_size)) {}

  interpreter::Register result_location() const {
    // We should only be checking this for interpreted frames, other kinds of
    // frames shouldn't be considered for result locations.
    DCHECK_EQ(top_frame().type(), DeoptFrame::FrameType::kInterpretedFrame);
    return result_location_;
  }
  int result_size() const {
    // We should only be checking this for interpreted frames, other kinds of
    // frames shouldn't be considered for result locations.
    DCHECK_EQ(top_frame().type(), DeoptFrame::FrameType::kInterpretedFrame);
    return ResultSizeField::decode(bitfield_);
  }

  bool IsResultRegister(interpreter::Register reg) const;
  void UpdateResultLocation(interpreter::Register result_location,
                            int result_size) {
    // We should only update to a subset of the existing result location.
    DCHECK_GE(result_location.index(), result_location_.index());
    DCHECK_LE(result_location.index() + result_size,
              result_location_.index() + this->result_size());
    result_location_ = result_location;
    bitfield_ = ResultSizeField::update(bitfield_, result_size);
  }
  bool HasResultLocation() const {
    // We should only be checking this for interpreted frames, other kinds of
    // frames shouldn't be considered for result locations.
    DCHECK_EQ(top_frame().type(), DeoptFrame::FrameType::kInterpretedFrame);
    return result_location_.is_valid();
  }

  int deopting_call_return_pc() const {
    DCHECK_NE(DeoptingCallReturnPcField::decode(bitfield_),
              kUninitializedCallReturnPc);
    return DeoptingCallReturnPcField::decode(bitfield_);
  }
  void set_deopting_call_return_pc(int pc) {
    DCHECK_EQ(DeoptingCallReturnPcField::decode(bitfield_),
              kUninitializedCallReturnPc);
    bitfield_ = DeoptingCallReturnPcField::update(bitfield_, pc);
  }

 private:
  using DeoptingCallReturnPcField = base::BitField<unsigned int, 0, 30>;
  using ResultSizeField = DeoptingCallReturnPcField::Next<unsigned int, 2>;

  // The max code size is enforced by the various assemblers, but it's not
  // visible here, so static assert against the magic constant that we happen
  // to know is correct.
  static constexpr int kMaxCodeSize = 512 * MB;
  static constexpr unsigned int kUninitializedCallReturnPc =
      DeoptingCallReturnPcField::kMax;
  static_assert(DeoptingCallReturnPcField::is_valid(kMaxCodeSize));
  static_assert(kMaxCodeSize != kUninitializedCallReturnPc);

  // Lazy deopts can have at most two result registers -- temporarily three for
  // ForInPrepare.
  static_assert(ResultSizeField::kMax >= 3);

  interpreter::Register result_location_;
  uint32_t bitfield_;
};

class ExceptionHandlerInfo {
 public:
  const int kNoExceptionHandlerPCOffsetMarker = 0xdeadbeef;

  ExceptionHandlerInfo()
      : catch_block(), pc_offset(kNoExceptionHandlerPCOffsetMarker) {}

  explicit ExceptionHandlerInfo(BasicBlockRef* catch_block_ref)
      : catch_block(catch_block_ref), pc_offset(-1) {}

  bool HasExceptionHandler() {
    return pc_offset != kNoExceptionHandlerPCOffsetMarker;
  }

  BasicBlockRef catch_block;
  Label trampoline_entry;
  int pc_offset;
};

// Dummy type for the initial raw allocation.
struct NodeWithInlineInputs {};

namespace detail {
// Helper for getting the static opcode of a Node subclass. This is in a
// "detail" namespace rather than in NodeBase because we can't template
// specialize outside of namespace scopes before C++17.
template <class T>
struct opcode_of_helper;

#define DEF_OPCODE_OF(Name)                          \
  template <>                                        \
  struct opcode_of_helper<Name> {                    \
    static constexpr Opcode value = Opcode::k##Name; \
  };
NODE_BASE_LIST(DEF_OPCODE_OF)
#undef DEF_OPCODE_OF

template <typename T>
constexpr T* ObjectPtrBeforeAddress(void* address) {
  char* address_as_char_ptr = reinterpret_cast<char*>(address);
  char* object_ptr_as_char_ptr = address_as_char_ptr - sizeof(T);
  return reinterpret_cast<T*>(object_ptr_as_char_ptr);
}

template <typename T>
constexpr const T* ObjectPtrBeforeAddress(const void* address) {
  const char* address_as_char_ptr = reinterpret_cast<const char*>(address);
  const char* object_ptr_as_char_ptr = address_as_char_ptr - sizeof(T);
  return reinterpret_cast<const T*>(object_ptr_as_char_ptr);
}

}  // namespace detail

class NodeBase : public ZoneObject {
 private:
  // Bitfield specification.
  using OpcodeField = base::BitField64<Opcode, 0, 16>;
  static_assert(OpcodeField::is_valid(kLastOpcode));
  using OpPropertiesField =
      OpcodeField::Next<OpProperties, OpProperties::kSize>;
  using NumTemporariesNeededField = OpPropertiesField::Next<uint8_t, 2>;
  using NumDoubleTemporariesNeededField =
      NumTemporariesNeededField::Next<uint8_t, 1>;
  // Align input count to 32-bit.
  using UnusedField = NumDoubleTemporariesNeededField::Next<uint8_t, 1>;
  using InputCountField = UnusedField::Next<size_t, 17>;
  static_assert(InputCountField::kShift == 32);

 protected:
  // Subclasses may use the remaining bitfield bits.
  template <class T, int size>
  using NextBitField = InputCountField::Next<T, size>;

  static constexpr int kMaxInputs = InputCountField::kMax;

 public:
  template <class T>
  static constexpr Opcode opcode_of = detail::opcode_of_helper<T>::value;

  template <class Derived, typename... Args>
  static Derived* New(Zone* zone, std::initializer_list<ValueNode*> inputs,
                      Args&&... args) {
    Derived* node =
        Allocate<Derived>(zone, inputs.size(), std::forward<Args>(args)...);

    int i = 0;
    for (ValueNode* input : inputs) {
      DCHECK_NOT_NULL(input);
      node->set_input(i++, input);
    }

    return node;
  }

  // Inputs must be initialized manually.
  template <class Derived, typename... Args>
  static Derived* New(Zone* zone, size_t input_count, Args&&... args) {
    Derived* node =
        Allocate<Derived>(zone, input_count, std::forward<Args>(args)...);
    return node;
  }

  // Overwritten by subclasses.
  static constexpr OpProperties kProperties =
      OpProperties::Pure() | OpProperties::TaggedValue();

  constexpr Opcode opcode() const { return OpcodeField::decode(bitfield_); }
  constexpr OpProperties properties() const {
    return OpPropertiesField::decode(bitfield_);
  }
  void set_properties(OpProperties properties) {
    bitfield_ = OpPropertiesField::update(bitfield_, properties);
  }

  template <class T>
  constexpr bool Is() const;

  template <class T>
  constexpr T* Cast() {
    DCHECK(Is<T>());
    return static_cast<T*>(this);
  }
  template <class T>
  constexpr const T* Cast() const {
    DCHECK(Is<T>());
    return static_cast<const T*>(this);
  }
  template <class T>
  constexpr T* TryCast() {
    return Is<T>() ? static_cast<T*>(this) : nullptr;
  }

  constexpr bool has_inputs() const { return input_count() > 0; }
  constexpr int input_count() const {
    static_assert(InputCountField::kMax <= kMaxInt);
    return static_cast<int>(InputCountField::decode(bitfield_));
  }

  constexpr Input& input(int index) {
    DCHECK_LT(index, input_count());
    return *(input_base() - index);
  }
  constexpr const Input& input(int index) const {
    DCHECK_LT(index, input_count());
    return *(input_base() - index);
  }

  // Input iterators, use like:
  //
  //  for (Input& input : *node) { ... }
  constexpr auto begin() { return std::make_reverse_iterator(&input(-1)); }
  constexpr auto end() {
    return std::make_reverse_iterator(&input(input_count() - 1));
  }

  constexpr bool has_id() const { return id_ != kInvalidNodeId; }
  constexpr NodeIdT id() const {
    DCHECK_NE(id_, kInvalidNodeId);
    return id_;
  }
  void set_id(NodeIdT id) {
    DCHECK_EQ(id_, kInvalidNodeId);
    DCHECK_NE(id, kInvalidNodeId);
    id_ = id;
  }

  template <typename RegisterT>
  uint8_t num_temporaries_needed() const {
    if constexpr (std::is_same_v<RegisterT, Register>) {
      return NumTemporariesNeededField::decode(bitfield_);
    } else {
      return NumDoubleTemporariesNeededField::decode(bitfield_);
    }
  }

  template <typename RegisterT>
  RegListBase<RegisterT>& temporaries() {
    if constexpr (std::is_same_v<RegisterT, Register>) {
      return temporaries_;
    } else {
      return double_temporaries_;
    }
  }

  RegList& general_temporaries() { return temporaries_; }
  DoubleRegList& double_temporaries() { return double_temporaries_; }

  template <typename RegisterT>
  void assign_temporaries(RegListBase<RegisterT> list) {
    if constexpr (std::is_same_v<RegisterT, Register>) {
      temporaries_ = list;
    } else {
      double_temporaries_ = list;
    }
  }

  enum class InputAllocationPolicy { kFixedRegister, kArbitraryRegister, kAny };

  // Some parts of Maglev require a specific iteration order of the inputs (such
  // as UseMarkingProcessor::MarkInputUses or
  // StraightForwardRegisterAllocator::AssignInputs). For such cases,
  // `ForAllInputsInRegallocAssignmentOrder` can be called with a callback `f`
  // that will be called for each input in the "correct" order.
  template <typename Function>
  void ForAllInputsInRegallocAssignmentOrder(Function&& f);

  void Print(std::ostream& os, MaglevGraphLabeller*,
             bool skip_targets = false) const;

  // For GDB: Print any Node with `print node->Print()`.
  void Print() const;

  EagerDeoptInfo* eager_deopt_info() {
    DCHECK(properties().can_eager_deopt());
    DCHECK(!properties().can_lazy_deopt());
    return reinterpret_cast<EagerDeoptInfo*>(deopt_info_address());
  }

  LazyDeoptInfo* lazy_deopt_info() {
    DCHECK(properties().can_lazy_deopt());
    DCHECK(!properties().can_eager_deopt());
    return reinterpret_cast<LazyDeoptInfo*>(deopt_info_address());
  }

  const RegisterSnapshot& register_snapshot() const {
    DCHECK(properties().needs_register_snapshot());
    return *reinterpret_cast<RegisterSnapshot*>(register_snapshot_address());
  }

  ExceptionHandlerInfo* exception_handler_info() {
    DCHECK(properties().can_throw());
    return reinterpret_cast<ExceptionHandlerInfo*>(exception_handler_address());
  }

  void set_register_snapshot(RegisterSnapshot snapshot) {
    DCHECK(properties().needs_register_snapshot());
    *reinterpret_cast<RegisterSnapshot*>(register_snapshot_address()) =
        snapshot;
  }

  void change_input(int index, ValueNode* node) { set_input(index, node); }

  void change_representation(ValueRepresentation new_repr) {
    DCHECK_EQ(opcode(), Opcode::kPhi);
    bitfield_ = OpPropertiesField::update(
        bitfield_, properties().WithNewValueRepresentation(new_repr));
  }

  void set_opcode(Opcode new_opcode) {
    bitfield_ = OpcodeField::update(bitfield_, new_opcode);
  }

  void CopyEagerDeoptInfoOf(NodeBase* other, Zone* zone) {
    new (eager_deopt_info())
        EagerDeoptInfo(zone, other->eager_deopt_info()->top_frame(),
                       other->eager_deopt_info()->feedback_to_update());
  }

  void SetEagerDeoptInfo(Zone* zone, DeoptFrame deopt_frame,
                         compiler::FeedbackSource feedback_to_update =
                             compiler::FeedbackSource()) {
    DCHECK(properties().can_eager_deopt());
    new (eager_deopt_info())
        EagerDeoptInfo(zone, deopt_frame, feedback_to_update);
  }

  template <typename NodeT>
  void OverwriteWith() {
    OverwriteWith(NodeBase::opcode_of<NodeT>, NodeT::kProperties);
  }

  void OverwriteWith(
      Opcode new_opcode,
      base::Optional<OpProperties> maybe_new_properties = base::nullopt) {
    OpProperties new_properties = maybe_new_properties.has_value()
                                      ? maybe_new_properties.value()
                                      : StaticPropertiesForOpcode(new_opcode);
#ifdef DEBUG
    CheckCanOverwriteWith(new_opcode, new_properties);
#endif
    set_opcode(new_opcode);
    set_properties(new_properties);
  }

 protected:
  explicit NodeBase(uint64_t bitfield) : bitfield_(bitfield) {}

  // Allow updating bits above NextBitField from subclasses
  constexpr uint64_t bitfield() const { return bitfield_; }
  void set_bitfield(uint64_t new_bitfield) {
#ifdef DEBUG
    // Make sure that all the base bitfield bits (all bits before the next
    // bitfield start) are equal in the new value.s
    const uint64_t base_bitfield_mask =
        (uint64_t{1} << NextBitField<bool, 1>::kShift) - 1;
    DCHECK_EQ(bitfield_ & base_bitfield_mask,
              new_bitfield & base_bitfield_mask);
#endif
    bitfield_ = new_bitfield;
  }

  constexpr Input* input_base() {
    return detail::ObjectPtrBeforeAddress<Input>(this);
  }
  constexpr const Input* input_base() const {
    return detail::ObjectPtrBeforeAddress<Input>(this);
  }
  Input* last_input() { return &input(input_count() - 1); }
  const Input* last_input() const { return &input(input_count() - 1); }

  Address last_input_address() const {
    return reinterpret_cast<Address>(last_input());
  }

  void set_input(int index, ValueNode* node) {
    new (&input(index)) Input(node);
  }

  // For nodes that don't have data past the input, allow trimming the input
  // count. This is used by Phis to reduce inputs when merging in dead control
  // flow.
  void reduce_input_count() {
    DCHECK_EQ(opcode(), Opcode::kPhi);
    DCHECK(!properties().can_lazy_deopt());
    DCHECK(!properties().can_eager_deopt());
    bitfield_ = InputCountField::update(bitfield_, input_count() - 1);
  }

  // Specify that there need to be a certain number of registers free (i.e.
  // useable as scratch registers) on entry into this node.
  //
  // Does not include any registers requested by RequireSpecificTemporary.
  void set_temporaries_needed(uint8_t value) {
    DCHECK_EQ(num_temporaries_needed<Register>(), 0);
    bitfield_ = NumTemporariesNeededField::update(bitfield_, value);
  }

  void set_double_temporaries_needed(uint8_t value) {
    DCHECK_EQ(num_temporaries_needed<DoubleRegister>(), 0);
    bitfield_ = NumDoubleTemporariesNeededField::update(bitfield_, value);
  }

  // Require that a specific register is free (and therefore clobberable) by the
  // entry into this node.
  void RequireSpecificTemporary(Register reg) { temporaries_.set(reg); }

  void RequireSpecificDoubleTemporary(DoubleRegister reg) {
    double_temporaries_.set(reg);
  }

 private:
  template <class Derived, typename... Args>

  static Derived* Allocate(Zone* zone, size_t input_count, Args&&... args) {
    static_assert(
        !Derived::kProperties.can_eager_deopt() ||
            !Derived::kProperties.can_lazy_deopt(),
        "The current deopt info representation, at the end of inputs, requires "
        "that we cannot have both lazy and eager deopts on a node. If we ever "
        "need this, we have to update accessors to check node->properties() "
        "for which deopts are active.");
    constexpr size_t size_before_inputs =
        ExceptionHandlerInfoSize(Derived::kProperties) +
        RegisterSnapshotSize(Derived::kProperties) +
        EagerDeoptInfoSize(Derived::kProperties) +
        LazyDeoptInfoSize(Derived::kProperties);

    static_assert(IsAligned(size_before_inputs, alignof(Input)));
    const size_t size_before_node =
        size_before_inputs + input_count * sizeof(Input);

    DCHECK(IsAligned(size_before_inputs, alignof(Derived)));
    const size_t size = size_before_node + sizeof(Derived);
    intptr_t raw_buffer =
        reinterpret_cast<intptr_t>(zone->Allocate<NodeWithInlineInputs>(size));
    void* node_buffer = reinterpret_cast<void*>(raw_buffer + size_before_node);
    uint64_t bitfield = OpcodeField::encode(opcode_of<Derived>) |
                        OpPropertiesField::encode(Derived::kProperties) |
                        InputCountField::encode(input_count);
    Derived* node =
        new (node_buffer) Derived(bitfield, std::forward<Args>(args)...);
    return node;
  }

  static constexpr size_t ExceptionHandlerInfoSize(OpProperties properties) {
    return RoundUp<alignof(Input)>(
        properties.can_throw() ? sizeof(ExceptionHandlerInfo) : 0);
  }

  static constexpr size_t RegisterSnapshotSize(OpProperties properties) {
    return RoundUp<alignof(Input)>(
        properties.needs_register_snapshot() ? sizeof(RegisterSnapshot) : 0);
  }

  static constexpr size_t EagerDeoptInfoSize(OpProperties properties) {
    return RoundUp<alignof(Input)>(
        properties.can_eager_deopt() ? sizeof(EagerDeoptInfo) : 0);
  }

  static constexpr size_t LazyDeoptInfoSize(OpProperties properties) {
    return RoundUp<alignof(Input)>(
        properties.can_lazy_deopt() ? sizeof(LazyDeoptInfo) : 0);
  }

  // Returns the position of deopt info if it exists, otherwise returns
  // its position as if DeoptInfo size were zero.
  Address deopt_info_address() const {
    DCHECK(!properties().can_eager_deopt() || !properties().can_lazy_deopt());
    size_t extra =
        EagerDeoptInfoSize(properties()) + LazyDeoptInfoSize(properties());
    return last_input_address() - extra;
  }

  // Returns the position of register snapshot if it exists, otherwise returns
  // its position as if RegisterSnapshot size were zero.
  Address register_snapshot_address() const {
    size_t extra = RegisterSnapshotSize(properties());
    return deopt_info_address() - extra;
  }

  // Returns the position of exception handler info if it exists, otherwise
  // returns its position as if ExceptionHandlerInfo size were zero.
  Address exception_handler_address() const {
    size_t extra = ExceptionHandlerInfoSize(properties());
    return register_snapshot_address() - extra;
  }

  void CheckCanOverwriteWith(Opcode new_opcode, OpProperties new_properties);

  uint64_t bitfield_;
  NodeIdT id_ = kInvalidNodeId;
  RegList temporaries_;
  DoubleRegList double_temporaries_;

  NodeBase() = delete;
  NodeBase(const NodeBase&) = delete;
  NodeBase(NodeBase&&) = delete;
  NodeBase& operator=(const NodeBase&) = delete;
  NodeBase& operator=(NodeBase&&) = delete;
};

template <class T>
constexpr bool NodeBase::Is() const {
  return opcode() == opcode_of<T>;
}

// Specialized sub-hierarchy type checks.
template <>
constexpr bool NodeBase::Is<ValueNode>() const {
  return IsValueNode(opcode());
}
template <>
constexpr bool NodeBase::Is<ControlNode>() const {
  return IsControlNode(opcode());
}
template <>
constexpr bool NodeBase::Is<BranchControlNode>() const {
  return IsBranchControlNode(opcode());
}
template <>
constexpr bool NodeBase::Is<ConditionalControlNode>() const {
  return IsConditionalControlNode(opcode());
}
template <>
constexpr bool NodeBase::Is<UnconditionalControlNode>() const {
  return IsUnconditionalControlNode(opcode());
}
template <>
constexpr bool NodeBase::Is<TerminalControlNode>() const {
  return IsTerminalControlNode(opcode());
}

void CheckValueInputIs(const NodeBase* node, int i,
                       ValueRepresentation expected,
                       MaglevGraphLabeller* graph_labeller);

// The Node class hierarchy contains all non-control nodes.
class Node : public NodeBase {
 public:
  using List = base::ThreadedListWithUnsafeInsertions<Node>;

  inline ValueLocation& result();

  Node* NextNode() const { return next_; }

 protected:
  using NodeBase::NodeBase;

 private:
  Node** next() { return &next_; }
  Node* next_ = nullptr;

  friend List;
  friend base::ThreadedListTraits<Node>;
};

// All non-control nodes with a result.
class ValueNode : public Node {
 private:
  using TaggedResultNeedsDecompressField = NextBitField<bool, 1>;

 protected:
  // Subclasses may use the remaining bitfield bits.
  template <class T, int size>
  using NextBitField = TaggedResultNeedsDecompressField::Next<T, size>;

 public:
  ValueLocation& result() { return result_; }
  const ValueLocation& result() const { return result_; }

  void SetHint(compiler::InstructionOperand hint);

  void ClearHint() { hint_ = compiler::InstructionOperand(); }

  bool has_hint() { return !hint_.IsInvalid(); }

  template <typename RegisterT>
  RegisterT GetRegisterHint() {
    if (hint_.IsInvalid()) return RegisterT::no_reg();
    return RegisterT::from_code(
        compiler::UnallocatedOperand::cast(hint_).fixed_register_index());
  }

  const compiler::InstructionOperand& hint() const {
    DCHECK(hint_.IsInvalid() || hint_.IsUnallocated());
    return hint_;
  }

  bool is_loadable() const {
    DCHECK_EQ(state_, kSpill);
    return spill_.IsConstant() || spill_.IsAnyStackSlot();
  }

  bool is_spilled() const {
    DCHECK_EQ(state_, kSpill);
    return spill_.IsAnyStackSlot();
  }

  void SetNoSpill();
  void SetConstantLocation();

  /* For constants only. */
  void LoadToRegister(MaglevAssembler*, Register);
  void LoadToRegister(MaglevAssembler*, DoubleRegister);
  void DoLoadToRegister(MaglevAssembler*, Register);
  void DoLoadToRegister(MaglevAssembler*, DoubleRegister);
  Handle<Object> Reify(LocalIsolate* isolate) const;

  void Spill(compiler::AllocatedOperand operand) {
#ifdef DEBUG
    if (state_ == kLastUse) {
      state_ = kSpill;
    } else {
      DCHECK(!is_loadable());
    }
#endif  // DEBUG
    DCHECK(!IsConstantNode(opcode()));
    DCHECK(operand.IsAnyStackSlot());
    spill_ = operand;
    DCHECK(spill_.IsAnyStackSlot());
  }

  compiler::AllocatedOperand spill_slot() const {
    DCHECK(is_spilled());
    return compiler::AllocatedOperand::cast(loadable_slot());
  }

  compiler::InstructionOperand loadable_slot() const {
    DCHECK_EQ(state_, kSpill);
    DCHECK(is_loadable());
    return spill_;
  }

  void mark_use(NodeIdT id, InputLocation* input_location) {
    DCHECK_EQ(state_, kLastUse);
    DCHECK_NE(id, kInvalidNodeId);
    DCHECK_LT(start_id(), id);
    DCHECK_IMPLIES(has_valid_live_range(), id >= end_id_);
    end_id_ = id;
    *last_uses_next_use_id_ = id;
    last_uses_next_use_id_ = input_location->get_next_use_id_address();
    DCHECK_EQ(*last_uses_next_use_id_, kInvalidNodeId);
  }

  struct LiveRange {
    NodeIdT start = kInvalidNodeId;
    NodeIdT end = kInvalidNodeId;  // Inclusive.
  };

  bool has_valid_live_range() const { return end_id_ != 0; }
  LiveRange live_range() const { return {start_id(), end_id_}; }
  NodeIdT next_use() const { return next_use_; }

  // The following metods should only be used during register allocation, to
  // mark the _current_ state of this Node according to the register allocator.
  void set_next_use(NodeIdT use) { next_use_ = use; }

  // A node is dead once it has no more upcoming uses.
  bool is_dead() const { return next_use_ == kInvalidNodeId; }

  constexpr bool use_double_register() const {
    return IsDoubleRepresentation(properties().value_representation());
  }

  constexpr bool is_tagged() const {
    return (properties().value_representation() ==
            ValueRepresentation::kTagged);
  }

  constexpr bool decompresses_tagged_result() const {
    return TaggedResultNeedsDecompressField::decode(bitfield());
  }
  void SetTaggedResultNeedsDecompress() {
    DCHECK_IMPLIES(!Is<Identity>(), is_tagged());
    DCHECK_IMPLIES(Is<Identity>(), input(0).node()->is_tagged());
    set_bitfield(TaggedResultNeedsDecompressField::update(bitfield(), true));
    if (Is<Phi>()) {
      for (Input& input : *this) {
        // Avoid endless recursion by terminating on values already marked.
        if (input.node()->decompresses_tagged_result()) continue;
        input.node()->SetTaggedResultNeedsDecompress();
      }
    } else if (Is<Identity>()) {
      DCHECK_EQ(input_count(), 0);
      input(0).node()->SetTaggedResultNeedsDecompress();
    }
  }

  constexpr ValueRepresentation value_representation() const {
    return properties().value_representation();
  }

  constexpr MachineRepresentation GetMachineRepresentation() const {
    switch (properties().value_representation()) {
      case ValueRepresentation::kTagged:
        return MachineRepresentation::kTagged;
      case ValueRepresentation::kInt32:
      case ValueRepresentation::kUint32:
        return MachineRepresentation::kWord32;
      case ValueRepresentation::kWord64:
        return MachineRepresentation::kWord64;
      case ValueRepresentation::kFloat64:
        return MachineRepresentation::kFloat64;
      case ValueRepresentation::kHoleyFloat64:
        return MachineRepresentation::kFloat64;
    }
  }

  void InitializeRegisterData() {
    if (use_double_register()) {
      double_registers_with_result_ = kEmptyDoubleRegList;
    } else {
      registers_with_result_ = kEmptyRegList;
    }
  }

  void AddRegister(Register reg) {
    DCHECK(!use_double_register());
    registers_with_result_.set(reg);
  }
  void AddRegister(DoubleRegister reg) {
    DCHECK(use_double_register());
    double_registers_with_result_.set(reg);
  }

  void RemoveRegister(Register reg) {
    DCHECK(!use_double_register());
    registers_with_result_.clear(reg);
  }
  void RemoveRegister(DoubleRegister reg) {
    DCHECK(use_double_register());
    double_registers_with_result_.clear(reg);
  }

  template <typename T>
  inline RegListBase<T> ClearRegisters();

  int num_registers() const {
    if (use_double_register()) {
      return double_registers_with_result_.Count();
    }
    return registers_with_result_.Count();
  }
  bool has_register() const {
    if (use_double_register()) {
      return double_registers_with_result_ != kEmptyDoubleRegList;
    }
    return registers_with_result_ != kEmptyRegList;
  }
  bool is_in_register(Register reg) const {
    DCHECK(!use_double_register());
    return registers_with_result_.has(reg);
  }
  bool is_in_register(DoubleRegister reg) const {
    DCHECK(use_double_register());
    return double_registers_with_result_.has(reg);
  }

  template <typename T>
  RegListBase<T> result_registers() {
    if constexpr (std::is_same<T, DoubleRegister>::value) {
      DCHECK(use_double_register());
      return double_registers_with_result_;
    } else {
      DCHECK(!use_double_register());
      return registers_with_result_;
    }
  }

  compiler::InstructionOperand allocation() const {
    if (has_register()) {
      return compiler::AllocatedOperand(compiler::LocationOperand::REGISTER,
                                        GetMachineRepresentation(),
                                        FirstRegisterCode());
    }
    DCHECK(is_loadable());
    return spill_;
  }

 protected:
  explicit ValueNode(uint64_t bitfield)
      : Node(bitfield),
        last_uses_next_use_id_(&next_use_),
        hint_(compiler::InstructionOperand())
#ifdef DEBUG
        ,
        state_(kLastUse)
#endif  // DEBUG
  {
    InitializeRegisterData();
  }

  int FirstRegisterCode() const {
    if (use_double_register()) {
      return double_registers_with_result_.first().code();
    }
    return registers_with_result_.first().code();
  }

  // Rename for better pairing with `end_id`.
  NodeIdT start_id() const { return id(); }

  NodeIdT end_id_ = kInvalidNodeId;
  NodeIdT next_use_ = kInvalidNodeId;
  ValueLocation result_;
  union {
    RegList registers_with_result_;
    DoubleRegList double_registers_with_result_;
  };
  union {
    // Pointer to the current last use's next_use_id field. Most of the time
    // this will be a pointer to an Input's next_use_id_ field, but it's
    // initialized to this node's next_use_ to track the first use.
    NodeIdT* last_uses_next_use_id_;
    compiler::InstructionOperand spill_;
  };
  compiler::InstructionOperand hint_;
#ifdef DEBUG
  enum {kLastUse, kSpill} state_;
#endif  // DEBUG
};

template <>
inline RegList ValueNode::ClearRegisters() {
  DCHECK(!use_double_register());
  return std::exchange(registers_with_result_, kEmptyRegList);
}

template <>
inline DoubleRegList ValueNode::ClearRegisters() {
  DCHECK(use_double_register());
  return std::exchange(double_registers_with_result_, kEmptyDoubleRegList);
}

ValueLocation& Node::result() {
  DCHECK(Is<ValueNode>());
  return Cast<ValueNode>()->result();
}

// Mixin for a node with known class (and therefore known opcode and static
// properties), but possibly unknown numbers of inputs.
template <typename Base, typename Derived>
class NodeTMixin : public Base {
 public:
  // Shadowing for static knowledge.
  constexpr Opcode opcode() const { return NodeBase::opcode_of<Derived>; }
  constexpr const OpProperties& properties() const {
    return Derived::kProperties;
  }

  template <typename... Args>
  static Derived* New(Zone* zone, std::initializer_list<ValueNode*> inputs,
                      Args&&... args) {
    return NodeBase::New<Derived>(zone, inputs, std::forward<Args>...);
  }
  template <typename... Args>
  static Derived* New(Zone* zone, size_t input_count, Args&&... args) {
    return NodeBase::New<Derived>(zone, input_count, std::forward<Args>...);
  }

 protected:
  template <typename... Args>
  explicit NodeTMixin(uint64_t bitfield, Args&&... args)
      : Base(bitfield, std::forward<Args>(args)...) {
    DCHECK_EQ(NodeBase::opcode(), NodeBase::opcode_of<Derived>);
    DCHECK_EQ(NodeBase::properties(), Derived::kProperties);
  }
};

namespace detail {
// Helper class for defining input types as a std::array, but without
// accidental initialisation with the wrong sized initializer_list.
template <size_t Size>
class ArrayWrapper : public std::array<ValueRepresentation, Size> {
 public:
  template <typename... Args>
  explicit constexpr ArrayWrapper(Args&&... args)
      : std::array<ValueRepresentation, Size>({args...}) {
    static_assert(sizeof...(args) == Size);
  }
};
struct YouNeedToDefineAnInputTypesArrayInYourDerivedClass {};
}  // namespace detail

// Mixin for a node with known class (and therefore known opcode and static
// properties), and known numbers of inputs.
template <size_t InputCount, typename Base, typename Derived>
class FixedInputNodeTMixin : public NodeTMixin<Base, Derived> {
  static constexpr size_t kInputCount = InputCount;

 public:
  // Shadowing for static knowledge.
  constexpr bool has_inputs() const { return input_count() > 0; }
  constexpr uint16_t input_count() const { return kInputCount; }
  constexpr auto end() {
    return std::make_reverse_iterator(&this->input(input_count() - 1));
  }

  void VerifyInputs(MaglevGraphLabeller* graph_labeller) const {
    if constexpr (kInputCount != 0) {
      static_assert(
          std::is_same_v<const InputTypes, decltype(Derived::kInputTypes)>);
      static_assert(kInputCount == Derived::kInputTypes.size());
      for (int i = 0; i < static_cast<int>(kInputCount); ++i) {
        CheckValueInputIs(this, i, Derived::kInputTypes[i], graph_labeller);
      }
    }
  }

  void MarkTaggedInputsAsDecompressing() const {
    if constexpr (kInputCount != 0) {
      static_assert(
          std::is_same_v<const InputTypes, decltype(Derived::kInputTypes)>);
      static_assert(kInputCount == Derived::kInputTypes.size());
      for (int i = 0; i < static_cast<int>(kInputCount); ++i) {
        if (Derived::kInputTypes[i] == ValueRepresentation::kTagged) {
          ValueNode* input_node = this->input(i).node();
          input_node->SetTaggedResultNeedsDecompress();
        }
      }
    }
  }

 protected:
  using InputTypes = detail::ArrayWrapper<kInputCount>;
  detail::YouNeedToDefineAnInputTypesArrayInYourDerivedClass kInputTypes;

  template <typename... Args>
  explicit FixedInputNodeTMixin(uint64_t bitfield, Args&&... args)
      : NodeTMixin<Base, Derived>(bitfield, std::forward<Args>(args)...) {
    DCHECK_EQ(NodeBase::input_count(), kInputCount);
  }
};

template <class Derived>
using NodeT = NodeTMixin<Node, Derived>;

template <class Derived>
using ValueNodeT = NodeTMixin<ValueNode, Derived>;

template <size_t InputCount, class Derived>
using FixedInputNodeT =
    FixedInputNodeTMixin<InputCount, NodeT<Derived>, Derived>;

template <size_t InputCount, class Derived>
using FixedInputValueNodeT =
    FixedInputNodeTMixin<InputCount, ValueNodeT<Derived>, Derived>;

class Identity : public FixedInputValueNodeT<1, Identity> {
  using Base = FixedInputValueNodeT<1, Identity>;

 public:
  static constexpr OpProperties kProperties = OpProperties::Pure();

  explicit Identity(uint64_t bitfield) : Base(bitfield) {}

  void VerifyInputs(MaglevGraphLabeller*) const {
    // Identity is valid for all input types.
  }
  void MarkTaggedInputsAsDecompressing() {
    // Do not mark inputs as decompressing here, since we don't yet know whether
    // this Phi needs decompression. Instead, let
    // Node::SetTaggedResultNeedsDecompress pass through phis.
  }
  void SetValueLocationConstraints() {}
  void GenerateCode(MaglevAssembler*, const ProcessingState&) {}
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

template <class Derived, Operation kOperation>
class UnaryWithFeedbackNode : public FixedInputValueNodeT<1, Derived> {
  using Base = FixedInputValueNodeT<1, Derived>;

 public:
  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::JSCall();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  static constexpr int kOperandIndex = 0;
  Input& operand_input() { return Node::input(kOperandIndex); }
  compiler::FeedbackSource feedback() const { return feedback_; }

 protected:
  explicit UnaryWithFeedbackNode(uint64_t bitfield,
                                 const compiler::FeedbackSource& feedback)
      : Base(bitfield), feedback_(feedback) {}

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

  const compiler::FeedbackSource feedback_;
};

template <class Derived, Operation kOperation>
class BinaryWithFeedbackNode : public FixedInputValueNodeT<2, Derived> {
  using Base = FixedInputValueNodeT<2, Derived>;

 public:
  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::JSCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  static constexpr int kLeftIndex = 0;
  static constexpr int kRightIndex = 1;
  Input& left_input() { return Node::input(kLeftIndex); }
  Input& right_input() { return Node::input(kRightIndex); }
  compiler::FeedbackSource feedback() const { return feedback_; }

 protected:
  BinaryWithFeedbackNode(uint64_t bitfield,
                         const compiler::FeedbackSource& feedback)
      : Base(bitfield), feedback_(feedback) {}

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

  const compiler::FeedbackSource feedback_;
};

#define DEF_OPERATION_WITH_FEEDBACK_NODE(Name, Super, OpName)         \
  class Name : public Super<Name, Operation::k##OpName> {             \
    using Base = Super<Name, Operation::k##OpName>;                   \
                                                                      \
   public:                                                            \
    Name(uint64_t bitfield, const compiler::FeedbackSource& feedback) \
        : Base(bitfield, feedback) {}                                 \
    int MaxCallStackArgs() const { return 0; }                        \
    void SetValueLocationConstraints();                               \
    void GenerateCode(MaglevAssembler*, const ProcessingState&);      \
    void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}    \
  };

#define DEF_UNARY_WITH_FEEDBACK_NODE(Name) \
  DEF_OPERATION_WITH_FEEDBACK_NODE(Generic##Name, UnaryWithFeedbackNode, Name)
#define DEF_BINARY_WITH_FEEDBACK_NODE(Name) \
  DEF_OPERATION_WITH_FEEDBACK_NODE(Generic##Name, BinaryWithFeedbackNode, Name)
UNARY_OPERATION_LIST(DEF_UNARY_WITH_FEEDBACK_NODE)
ARITHMETIC_OPERATION_LIST(DEF_BINARY_WITH_FEEDBACK_NODE)
COMPARISON_OPERATION_LIST(DEF_BINARY_WITH_FEEDBACK_NODE)
#undef DEF_UNARY_WITH_FEEDBACK_NODE
#undef DEF_BINARY_WITH_FEEDBACK_NODE
#undef DEF_OPERATION_WITH_FEEDBACK_NODE

template <class Derived, Operation kOperation>
class Int32BinaryWithOverflowNode : public FixedInputValueNodeT<2, Derived> {
  using Base = FixedInputValueNodeT<2, Derived>;

 public:
  static constexpr OpProperties kProperties =
      OpProperties::EagerDeopt() | OpProperties::Int32();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kInt32, ValueRepresentation::kInt32};

  static constexpr int kLeftIndex = 0;
  static constexpr int kRightIndex = 1;
  Input& left_input() { return Node::input(kLeftIndex); }
  Input& right_input() { return Node::input(kRightIndex); }

 protected:
  explicit Int32BinaryWithOverflowNode(uint64_t bitfield) : Base(bitfield) {}

  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

#define DEF_OPERATION_NODE(Name, Super, OpName)                    \
  class Name : public Super<Name, Operation::k##OpName> {          \
    using Base = Super<Name, Operation::k##OpName>;                \
                                                                   \
   public:                                                         \
    explicit Name(uint64_t bitfield) : Base(bitfield) {}           \
    void SetValueLocationConstraints();                            \
    void GenerateCode(MaglevAssembler*, const ProcessingState&);   \
    void PrintParams(std::ostream&, MaglevGraphLabeller*) const {} \
  };

#define DEF_INT32_BINARY_WITH_OVERFLOW_NODE(Name)                            \
  DEF_OPERATION_NODE(Int32##Name##WithOverflow, Int32BinaryWithOverflowNode, \
                     Name)
DEF_INT32_BINARY_WITH_OVERFLOW_NODE(Add)
DEF_INT32_BINARY_WITH_OVERFLOW_NODE(Subtract)
DEF_INT32_BINARY_WITH_OVERFLOW_NODE(Multiply)
DEF_INT32_BINARY_WITH_OVERFLOW_NODE(Divide)
DEF_INT32_BINARY_WITH_OVERFLOW_NODE(Modulus)
#undef DEF_INT32_BINARY_WITH_OVERFLOW_NODE

template <class Derived, Operation kOperation>
class Int32BinaryNode : public FixedInputValueNodeT<2, Derived> {
  using Base = FixedInputValueNodeT<2, Derived>;

 public:
  static constexpr OpProperties kProperties = OpProperties::Int32();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kInt32, ValueRepresentation::kInt32};

  static constexpr int kLeftIndex = 0;
  static constexpr int kRightIndex = 1;
  Input& left_input() { return Node::input(kLeftIndex); }
  Input& right_input() { return Node::input(kRightIndex); }

 protected:
  explicit Int32BinaryNode(uint64_t bitfield) : Base(bitfield) {}
};

#define DEF_INT32_BINARY_NODE(Name) \
  DEF_OPERATION_NODE(Int32##Name, Int32BinaryNode, Name)
DEF_INT32_BINARY_NODE(BitwiseAnd)
DEF_INT32_BINARY_NODE(BitwiseOr)
DEF_INT32_BINARY_NODE(BitwiseXor)
DEF_INT32_BINARY_NODE(ShiftLeft)
DEF_INT32_BINARY_NODE(ShiftRight)
#undef DEF_INT32_BINARY_NODE

class Int32BitwiseNot : public FixedInputValueNodeT<1, Int32BitwiseNot> {
  using Base = FixedInputValueNodeT<1, Int32BitwiseNot>;

 public:
  explicit Int32BitwiseNot(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::Int32();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kInt32};

  static constexpr int kValueIndex = 0;
  Input& value_input() { return Node::input(kValueIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

template <class Derived, Operation kOperation>
class Int32UnaryWithOverflowNode : public FixedInputValueNodeT<1, Derived> {
  using Base = FixedInputValueNodeT<1, Derived>;

 public:
  static constexpr OpProperties kProperties =
      OpProperties::EagerDeopt() | OpProperties::Int32();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kInt32};

  static constexpr int kValueIndex = 0;
  Input& value_input() { return Node::input(kValueIndex); }

 protected:
  explicit Int32UnaryWithOverflowNode(uint64_t bitfield) : Base(bitfield) {}
};

#define DEF_INT32_UNARY_WITH_OVERFLOW_NODE(Name)                            \
  DEF_OPERATION_NODE(Int32##Name##WithOverflow, Int32UnaryWithOverflowNode, \
                     Name)
DEF_INT32_UNARY_WITH_OVERFLOW_NODE(Negate)
DEF_INT32_UNARY_WITH_OVERFLOW_NODE(Increment)
DEF_INT32_UNARY_WITH_OVERFLOW_NODE(Decrement)
#undef DEF_INT32_UNARY_WITH_OVERFLOW_NODE

class Int32ShiftRightLogical
    : public FixedInputValueNodeT<2, Int32ShiftRightLogical> {
  using Base = FixedInputValueNodeT<2, Int32ShiftRightLogical>;

 public:
  explicit Int32ShiftRightLogical(uint64_t bitfield) : Base(bitfield) {}

  // Unlike the other Int32 nodes, logical right shift returns a Uint32.
  static constexpr OpProperties kProperties = OpProperties::Uint32();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kInt32, ValueRepresentation::kInt32};

  static constexpr int kLeftIndex = 0;
  static constexpr int kRightIndex = 1;
  Input& left_input() { return Node::input(kLeftIndex); }
  Input& right_input() { return Node::input(kRightIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

template <class Derived, Operation kOperation>
class Int32CompareNode : public FixedInputValueNodeT<2, Derived> {
  using Base = FixedInputValueNodeT<2, Derived>;

 public:
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kInt32, ValueRepresentation::kInt32};

  static constexpr int kLeftIndex = 0;
  static constexpr int kRightIndex = 1;
  Input& left_input() { return Node::input(kLeftIndex); }
  Input& right_input() { return Node::input(kRightIndex); }

 protected:
  explicit Int32CompareNode(uint64_t bitfield) : Base(bitfield) {}

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

#define DEF_INT32_COMPARE_NODE(Name) \
  DEF_OPERATION_NODE(Int32##Name, Int32CompareNode, Name)
DEF_INT32_COMPARE_NODE(Equal)
DEF_INT32_COMPARE_NODE(StrictEqual)
DEF_INT32_COMPARE_NODE(LessThan)
DEF_INT32_COMPARE_NODE(LessThanOrEqual)
DEF_INT32_COMPARE_NODE(GreaterThan)
DEF_INT32_COMPARE_NODE(GreaterThanOrEqual)
#undef DEF_INT32_COMPARE_NODE

template <class Derived, Operation kOperation>
class Float64BinaryNode : public FixedInputValueNodeT<2, Derived> {
  using Base = FixedInputValueNodeT<2, Derived>;

 public:
  static constexpr OpProperties kProperties = OpProperties::Float64();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kHoleyFloat64, ValueRepresentation::kHoleyFloat64};

  static constexpr int kLeftIndex = 0;
  static constexpr int kRightIndex = 1;
  Input& left_input() { return Node::input(kLeftIndex); }
  Input& right_input() { return Node::input(kRightIndex); }

 protected:
  explicit Float64BinaryNode(uint64_t bitfield) : Base(bitfield) {}

  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

#define DEF_OPERATION_NODE_WITH_CALL(Name, Super, OpName)          \
  class Name : public Super<Name, Operation::k##OpName> {          \
    using Base = Super<Name, Operation::k##OpName>;                \
                                                                   \
   public:                                                         \
    explicit Name(uint64_t bitfield) : Base(bitfield) {}           \
    int MaxCallStackArgs() const;                                  \
    void SetValueLocationConstraints();                            \
    void GenerateCode(MaglevAssembler*, const ProcessingState&);   \
    void PrintParams(std::ostream&, MaglevGraphLabeller*) const {} \
  };

template <class Derived, Operation kOperation>
class Float64BinaryNodeWithCall : public FixedInputValueNodeT<2, Derived> {
  using Base = FixedInputValueNodeT<2, Derived>;

 public:
  static constexpr OpProperties kProperties =
      OpProperties::Float64() | OpProperties::Call();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kHoleyFloat64, ValueRepresentation::kHoleyFloat64};

  static constexpr int kLeftIndex = 0;
  static constexpr int kRightIndex = 1;
  Input& left_input() { return Node::input(kLeftIndex); }
  Input& right_input() { return Node::input(kRightIndex); }

 protected:
  explicit Float64BinaryNodeWithCall(uint64_t bitfield) : Base(bitfield) {}

  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

#define DEF_FLOAT64_BINARY_NODE(Name) \
  DEF_OPERATION_NODE(Float64##Name, Float64BinaryNode, Name)
#define DEF_FLOAT64_BINARY_NODE_WITH_CALL(Name) \
  DEF_OPERATION_NODE_WITH_CALL(Float64##Name, Float64BinaryNodeWithCall, Name)
DEF_FLOAT64_BINARY_NODE(Add)
DEF_FLOAT64_BINARY_NODE(Subtract)
DEF_FLOAT64_BINARY_NODE(Multiply)
DEF_FLOAT64_BINARY_NODE(Divide)
#ifdef V8_TARGET_ARCH_ARM64
// On Arm64, floating point modulus is implemented with a call to a C++
// function, while on x64, it's implemented natively without call.
DEF_FLOAT64_BINARY_NODE_WITH_CALL(Modulus)
#else
DEF_FLOAT64_BINARY_NODE(Modulus)
#endif
DEF_FLOAT64_BINARY_NODE_WITH_CALL(Exponentiate)
#undef DEF_FLOAT64_BINARY_NODE
#undef DEF_FLOAT64_BINARY_NODE_WITH_CALL

template <class Derived, Operation kOperation>
class Float64CompareNode : public FixedInputValueNodeT<2, Derived> {
  using Base = FixedInputValueNodeT<2, Derived>;

 public:
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kFloat64, ValueRepresentation::kFloat64};

  static constexpr int kLeftIndex = 0;
  static constexpr int kRightIndex = 1;
  Input& left_input() { return Node::input(kLeftIndex); }
  Input& right_input() { return Node::input(kRightIndex); }

 protected:
  explicit Float64CompareNode(uint64_t bitfield) : Base(bitfield) {}

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

#define DEF_FLOAT64_COMPARE_NODE(Name) \
  DEF_OPERATION_NODE(Float64##Name, Float64CompareNode, Name)
DEF_FLOAT64_COMPARE_NODE(Equal)
DEF_FLOAT64_COMPARE_NODE(StrictEqual)
DEF_FLOAT64_COMPARE_NODE(LessThan)
DEF_FLOAT64_COMPARE_NODE(LessThanOrEqual)
DEF_FLOAT64_COMPARE_NODE(GreaterThan)
DEF_FLOAT64_COMPARE_NODE(GreaterThanOrEqual)
#undef DEF_FLOAT64_COMPARE_NODE

#undef DEF_OPERATION_NODE
#undef DEF_OPERATION_NODE_WITH_CALL

class Float64Negate : public FixedInputValueNodeT<1, Float64Negate> {
  using Base = FixedInputValueNodeT<1, Float64Negate>;

 public:
  explicit Float64Negate(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::Float64();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kHoleyFloat64};

  Input& input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class Float64Ieee754Unary
    : public FixedInputValueNodeT<1, Float64Ieee754Unary> {
  using Base = FixedInputValueNodeT<1, Float64Ieee754Unary>;

 public:
  explicit Float64Ieee754Unary(uint64_t bitfield,
                               ExternalReference ieee_function)
      : Base(bitfield), ieee_function_(ieee_function) {}

  static constexpr OpProperties kProperties =
      OpProperties::Float64() | OpProperties::Call();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kHoleyFloat64};

  Input& input() { return Node::input(0); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  ExternalReference ieee_function_;
};

class CheckInt32IsSmi : public FixedInputNodeT<1, CheckInt32IsSmi> {
  using Base = FixedInputNodeT<1, CheckInt32IsSmi>;

 public:
  explicit CheckInt32IsSmi(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kInt32};

  Input& input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class CheckUint32IsSmi : public FixedInputNodeT<1, CheckUint32IsSmi> {
  using Base = FixedInputNodeT<1, CheckUint32IsSmi>;

 public:
  explicit CheckUint32IsSmi(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kUint32};

  Input& input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class CheckHoleyFloat64IsSmi
    : public FixedInputNodeT<1, CheckHoleyFloat64IsSmi> {
  using Base = FixedInputNodeT<1, CheckHoleyFloat64IsSmi>;

 public:
  explicit CheckHoleyFloat64IsSmi(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kHoleyFloat64};

  Input& input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class CheckedSmiTagInt32 : public FixedInputValueNodeT<1, CheckedSmiTagInt32> {
  using Base = FixedInputValueNodeT<1, CheckedSmiTagInt32>;

 public:
  explicit CheckedSmiTagInt32(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::EagerDeopt() | OpProperties::ConversionNode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kInt32};

  Input& input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class CheckedSmiTagUint32
    : public FixedInputValueNodeT<1, CheckedSmiTagUint32> {
  using Base = FixedInputValueNodeT<1, CheckedSmiTagUint32>;

 public:
  explicit CheckedSmiTagUint32(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::EagerDeopt() | OpProperties::ConversionNode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kUint32};

  Input& input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

// Input must guarantee to fit in a Smi.
class UnsafeSmiTag : public FixedInputValueNodeT<1, UnsafeSmiTag> {
  using Base = FixedInputValueNodeT<1, UnsafeSmiTag>;

 public:
  explicit UnsafeSmiTag(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::ConversionNode();

  Input& input() { return Node::input(0); }

  void VerifyInputs(MaglevGraphLabeller*) const;
  void MarkTaggedInputsAsDecompressing() {
    // No tagged inputs.
  }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class CheckedSmiUntag : public FixedInputValueNodeT<1, CheckedSmiUntag> {
  using Base = FixedInputValueNodeT<1, CheckedSmiUntag>;

 public:
  explicit CheckedSmiUntag(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt() |
                                              OpProperties::Int32() |
                                              OpProperties::ConversionNode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input& input() { return Node::input(0); }

  void MarkTaggedInputsAsDecompressing() {
    // Don't need to decompress to untag.
  }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class UnsafeSmiUntag : public FixedInputValueNodeT<1, UnsafeSmiUntag> {
  using Base = FixedInputValueNodeT<1, UnsafeSmiUntag>;

 public:
  explicit UnsafeSmiUntag(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::Int32() | OpProperties::ConversionNode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input& input() { return Node::input(0); }

  void MarkTaggedInputsAsDecompressing() {
    // Don't need to decompress to untag.
  }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class Int32Constant : public FixedInputValueNodeT<0, Int32Constant> {
  using Base = FixedInputValueNodeT<0, Int32Constant>;

 public:
  using OutputRegister = Register;

  explicit Int32Constant(uint64_t bitfield, int32_t value)
      : Base(bitfield), value_(value) {}

  static constexpr OpProperties kProperties = OpProperties::Int32();

  int32_t value() const { return value_; }

  bool ToBoolean(LocalIsolate* local_isolate) const { return value_ != 0; }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

  void DoLoadToRegister(MaglevAssembler*, OutputRegister);
  Handle<Object> DoReify(LocalIsolate* isolate) const;

 private:
  const int32_t value_;
};

class Float64Constant : public FixedInputValueNodeT<0, Float64Constant> {
  using Base = FixedInputValueNodeT<0, Float64Constant>;

 public:
  using OutputRegister = DoubleRegister;

  explicit Float64Constant(uint64_t bitfield, Float64 value)
      : Base(bitfield), value_(value) {}

  static constexpr OpProperties kProperties = OpProperties::Float64();

  Float64 value() const { return value_; }

  bool ToBoolean(LocalIsolate* local_isolate) const {
    return value_.get_scalar() != 0.0 && !value_.is_nan();
  }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

  void DoLoadToRegister(MaglevAssembler*, OutputRegister);
  Handle<Object> DoReify(LocalIsolate* isolate) const;

 private:
  const Float64 value_;
};

class Int32ToUint8Clamped
    : public FixedInputValueNodeT<1, Int32ToUint8Clamped> {
  using Base = FixedInputValueNodeT<1, Int32ToUint8Clamped>;

 public:
  explicit Int32ToUint8Clamped(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::Int32();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kInt32};

  Input& input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class Uint32ToUint8Clamped
    : public FixedInputValueNodeT<1, Uint32ToUint8Clamped> {
  using Base = FixedInputValueNodeT<1, Uint32ToUint8Clamped>;

 public:
  explicit Uint32ToUint8Clamped(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::Int32();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kUint32};

  Input& input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class Float64ToUint8Clamped
    : public FixedInputValueNodeT<1, Float64ToUint8Clamped> {
  using Base = FixedInputValueNodeT<1, Float64ToUint8Clamped>;

 public:
  explicit Float64ToUint8Clamped(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::Int32();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kHoleyFloat64};

  Input& input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class CheckedNumberToUint8Clamped
    : public FixedInputValueNodeT<1, CheckedNumberToUint8Clamped> {
  using Base = FixedInputValueNodeT<1, CheckedNumberToUint8Clamped>;

 public:
  explicit CheckedNumberToUint8Clamped(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::EagerDeopt() | OpProperties::Int32();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input& input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class Int32ToNumber : public FixedInputValueNodeT<1, Int32ToNumber> {
  using Base = FixedInputValueNodeT<1, Int32ToNumber>;

 public:
  explicit Int32ToNumber(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::CanAllocate() |
                                              OpProperties::DeferredCall() |
                                              OpProperties::ConversionNode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kInt32};

  Input& input() { return Node::input(0); }

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class Uint32ToNumber : public FixedInputValueNodeT<1, Uint32ToNumber> {
  using Base = FixedInputValueNodeT<1, Uint32ToNumber>;

 public:
  explicit Uint32ToNumber(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::CanAllocate() |
                                              OpProperties::DeferredCall() |
                                              OpProperties::ConversionNode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kUint32};

  Input& input() { return Node::input(0); }

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class Float64ToTagged : public FixedInputValueNodeT<1, Float64ToTagged> {
  using Base = FixedInputValueNodeT<1, Float64ToTagged>;

 public:
  enum class ConversionMode { kCanonicalizeSmi, kForceHeapNumber };
  explicit Float64ToTagged(
      uint64_t bitfield, ConversionMode mode = ConversionMode::kCanonicalizeSmi)
      : Base(ConversionModeBitField::update(bitfield, mode)) {}
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kFloat64};

  static constexpr OpProperties kProperties = OpProperties::CanAllocate() |
                                              OpProperties::DeferredCall() |
                                              OpProperties::ConversionNode();

  Input& input() { return Node::input(0); }

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  bool canonicalize_smi() {
    return ConversionModeBitField::decode(bitfield()) ==
           ConversionMode::kCanonicalizeSmi;
  }
  using ConversionModeBitField = NextBitField<ConversionMode, 1>;
};

class HoleyFloat64ToTagged
    : public FixedInputValueNodeT<1, HoleyFloat64ToTagged> {
  using Base = FixedInputValueNodeT<1, HoleyFloat64ToTagged>;

 public:
  enum class ConversionMode { kCanonicalizeSmi, kForceHeapNumber };
  explicit HoleyFloat64ToTagged(
      uint64_t bitfield, ConversionMode mode = ConversionMode::kCanonicalizeSmi)
      : Base(ConversionModeBitField::update(bitfield, mode)) {}
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kHoleyFloat64};

  static constexpr OpProperties kProperties = OpProperties::CanAllocate() |
                                              OpProperties::DeferredCall() |
                                              OpProperties::ConversionNode();

  Input& input() { return Node::input(0); }

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  bool canonicalize_smi() {
    return ConversionModeBitField::decode(bitfield()) ==
           ConversionMode::kCanonicalizeSmi;
  }
  using ConversionModeBitField = NextBitField<ConversionMode, 1>;
};

class CheckedSmiTagFloat64
    : public FixedInputValueNodeT<1, CheckedSmiTagFloat64> {
  using Base = FixedInputValueNodeT<1, CheckedSmiTagFloat64>;

 public:
  explicit CheckedSmiTagFloat64(uint64_t bitfield) : Base(bitfield) {}
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kHoleyFloat64};

  static constexpr OpProperties kProperties =
      OpProperties::EagerDeopt() | OpProperties::ConversionNode();

  Input& input() { return Node::input(0); }

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class CheckedInt32ToUint32
    : public FixedInputValueNodeT<1, CheckedInt32ToUint32> {
  using Base = FixedInputValueNodeT<1, CheckedInt32ToUint32>;

 public:
  explicit CheckedInt32ToUint32(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::Uint32() |
                                              OpProperties::ConversionNode() |
                                              OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kInt32};

  Input& input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class CheckedUint32ToInt32
    : public FixedInputValueNodeT<1, CheckedUint32ToInt32> {
  using Base = FixedInputValueNodeT<1, CheckedUint32ToInt32>;

 public:
  explicit CheckedUint32ToInt32(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::Int32() |
                                              OpProperties::ConversionNode() |
                                              OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kUint32};

  Input& input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class ChangeInt32ToFloat64
    : public FixedInputValueNodeT<1, ChangeInt32ToFloat64> {
  using Base = FixedInputValueNodeT<1, ChangeInt32ToFloat64>;

 public:
  explicit ChangeInt32ToFloat64(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::Float64() | OpProperties::ConversionNode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kInt32};

  Input& input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class ChangeUint32ToFloat64
    : public FixedInputValueNodeT<1, ChangeUint32ToFloat64> {
  using Base = FixedInputValueNodeT<1, ChangeUint32ToFloat64>;

 public:
  explicit ChangeUint32ToFloat64(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::Float64() | OpProperties::ConversionNode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kUint32};

  Input& input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class CheckedTruncateFloat64ToInt32
    : public FixedInputValueNodeT<1, CheckedTruncateFloat64ToInt32> {
  using Base = FixedInputValueNodeT<1, CheckedTruncateFloat64ToInt32>;

 public:
  explicit CheckedTruncateFloat64ToInt32(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt() |
                                              OpProperties::Int32() |
                                              OpProperties::ConversionNode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kHoleyFloat64};

  Input& input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class Float64Round : public FixedInputValueNodeT<1, Float64Round> {
  using Base = FixedInputValueNodeT<1, Float64Round>;

 public:
  enum class Kind { kFloor, kCeil, kNearest };

  static Builtin continuation(Kind kind) {
    switch (kind) {
      case Kind::kCeil:
        return Builtin::kMathCeilContinuation;
      case Kind::kFloor:
        return Builtin::kMathFloorContinuation;
      case Kind::kNearest:
        return Builtin::kMathRoundContinuation;
    }
  }

  explicit Float64Round(uint64_t bitfield, Kind kind)
      : Base(bitfield), kind_(kind) {}

  static constexpr OpProperties kProperties = OpProperties::Float64();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kHoleyFloat64};

  Input& input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  Kind kind_;
};

class CheckedTruncateFloat64ToUint32
    : public FixedInputValueNodeT<1, CheckedTruncateFloat64ToUint32> {
  using Base = FixedInputValueNodeT<1, CheckedTruncateFloat64ToUint32>;

 public:
  explicit CheckedTruncateFloat64ToUint32(uint64_t bitfield) : Base(bitfield) {}
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kHoleyFloat64};

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt() |
                                              OpProperties::Uint32() |
                                              OpProperties::ConversionNode();

  Input& input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

#define DEFINE_TRUNCATE_NODE(name, from_repr, properties)          \
  class name : public FixedInputValueNodeT<1, name> {              \
    using Base = FixedInputValueNodeT<1, name>;                    \
                                                                   \
   public:                                                         \
    explicit name(uint64_t bitfield) : Base(bitfield) {}           \
                                                                   \
    static constexpr OpProperties kProperties = properties;        \
    static constexpr typename Base::InputTypes kInputTypes{        \
        ValueRepresentation::k##from_repr};                        \
                                                                   \
    Input& input() { return Node::input(0); }                      \
                                                                   \
    void SetValueLocationConstraints();                            \
    void GenerateCode(MaglevAssembler*, const ProcessingState&);   \
    void PrintParams(std::ostream&, MaglevGraphLabeller*) const {} \
  };

DEFINE_TRUNCATE_NODE(TruncateUint32ToInt32, Uint32, OpProperties::Int32())
DEFINE_TRUNCATE_NODE(TruncateFloat64ToInt32, HoleyFloat64,
                     OpProperties::Int32())
DEFINE_TRUNCATE_NODE(UnsafeTruncateUint32ToInt32, Uint32, OpProperties::Int32())
DEFINE_TRUNCATE_NODE(UnsafeTruncateFloat64ToInt32, HoleyFloat64,
                     OpProperties::Int32())

#undef DEFINE_TRUNCATE_NODE

class CheckedNumberOrOddballToFloat64
    : public FixedInputValueNodeT<1, CheckedNumberOrOddballToFloat64> {
  using Base = FixedInputValueNodeT<1, CheckedNumberOrOddballToFloat64>;

 public:
  explicit CheckedNumberOrOddballToFloat64(
      uint64_t bitfield, TaggedToFloat64ConversionType conversion_type)
      : Base(TaggedToFloat64ConversionTypeOffset::update(bitfield,
                                                         conversion_type)) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt() |
                                              OpProperties::Float64() |
                                              OpProperties::ConversionNode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input& input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

  TaggedToFloat64ConversionType conversion_type() const {
    return TaggedToFloat64ConversionTypeOffset::decode(bitfield());
  }

 private:
  using TaggedToFloat64ConversionTypeOffset =
      NextBitField<TaggedToFloat64ConversionType, 1>;
};

class UncheckedNumberOrOddballToFloat64
    : public FixedInputValueNodeT<1, UncheckedNumberOrOddballToFloat64> {
  using Base = FixedInputValueNodeT<1, UncheckedNumberOrOddballToFloat64>;

 public:
  explicit UncheckedNumberOrOddballToFloat64(
      uint64_t bitfield, TaggedToFloat64ConversionType conversion_type)
      : Base(TaggedToFloat64ConversionTypeOffset::update(bitfield,
                                                         conversion_type)) {}

  static constexpr OpProperties kProperties =
      OpProperties::Float64() | OpProperties::ConversionNode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input& input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

  TaggedToFloat64ConversionType conversion_type() const {
    return TaggedToFloat64ConversionTypeOffset::decode(bitfield());
  }

 private:
  using TaggedToFloat64ConversionTypeOffset =
      NextBitField<TaggedToFloat64ConversionType, 1>;
};

class CheckedHoleyFloat64ToFloat64
    : public FixedInputValueNodeT<1, CheckedHoleyFloat64ToFloat64> {
  using Base = FixedInputValueNodeT<1, CheckedHoleyFloat64ToFloat64>;

 public:
  explicit CheckedHoleyFloat64ToFloat64(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt() |
                                              OpProperties::Float64() |
                                              OpProperties::ConversionNode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kHoleyFloat64};

  Input& input() { return Node::input(0); }

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class HoleyFloat64ToMaybeNanFloat64
    : public FixedInputValueNodeT<1, HoleyFloat64ToMaybeNanFloat64> {
  using Base = FixedInputValueNodeT<1, HoleyFloat64ToMaybeNanFloat64>;

 public:
  explicit HoleyFloat64ToMaybeNanFloat64(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::Float64();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kHoleyFloat64};

  Input& input() { return Node::input(0); }

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class TruncateNumberOrOddballToInt32
    : public FixedInputValueNodeT<1, TruncateNumberOrOddballToInt32> {
  using Base = FixedInputValueNodeT<1, TruncateNumberOrOddballToInt32>;

 public:
  explicit TruncateNumberOrOddballToInt32(
      uint64_t bitfield, TaggedToFloat64ConversionType conversion_type)
      : Base(TaggedToFloat64ConversionTypeOffset::update(bitfield,
                                                         conversion_type)) {}

  static constexpr OpProperties kProperties = OpProperties::Int32();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input& input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

  TaggedToFloat64ConversionType conversion_type() const {
    return TaggedToFloat64ConversionTypeOffset::decode(bitfield());
  }

 private:
  using TaggedToFloat64ConversionTypeOffset =
      NextBitField<TaggedToFloat64ConversionType, 1>;
};

class CheckedTruncateNumberOrOddballToInt32
    : public FixedInputValueNodeT<1, CheckedTruncateNumberOrOddballToInt32> {
  using Base = FixedInputValueNodeT<1, CheckedTruncateNumberOrOddballToInt32>;

 public:
  explicit CheckedTruncateNumberOrOddballToInt32(
      uint64_t bitfield, TaggedToFloat64ConversionType conversion_type)
      : Base(TaggedToFloat64ConversionTypeOffset::update(bitfield,
                                                         conversion_type)) {}

  static constexpr OpProperties kProperties =
      OpProperties::EagerDeopt() | OpProperties::Int32();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input& input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

  TaggedToFloat64ConversionType conversion_type() const {
    return TaggedToFloat64ConversionTypeOffset::decode(bitfield());
  }

 private:
  using TaggedToFloat64ConversionTypeOffset =
      NextBitField<TaggedToFloat64ConversionType, 1>;
};

class LogicalNot : public FixedInputValueNodeT<1, LogicalNot> {
  using Base = FixedInputValueNodeT<1, LogicalNot>;

 public:
  explicit LogicalNot(uint64_t bitfield) : Base(bitfield) {}

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input& value() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class SetPendingMessage : public FixedInputValueNodeT<1, SetPendingMessage> {
  using Base = FixedInputValueNodeT<1, SetPendingMessage>;

 public:
  explicit SetPendingMessage(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanWrite() | OpProperties::CanRead();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input& value() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

enum class CheckType { kCheckHeapObject, kOmitHeapObjectCheck };
class ToBoolean : public FixedInputValueNodeT<1, ToBoolean> {
  using Base = FixedInputValueNodeT<1, ToBoolean>;

 public:
  explicit ToBoolean(uint64_t bitfield, CheckType check_type)
      : Base(CheckTypeBitField::update(bitfield, check_type)) {}

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input& value() { return Node::input(0); }
  CheckType check_type() const { return CheckTypeBitField::decode(bitfield()); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  using CheckTypeBitField = NextBitField<CheckType, 1>;
};

class ToBooleanLogicalNot
    : public FixedInputValueNodeT<1, ToBooleanLogicalNot> {
  using Base = FixedInputValueNodeT<1, ToBooleanLogicalNot>;

 public:
  explicit ToBooleanLogicalNot(uint64_t bitfield, CheckType check_type)
      : Base(CheckTypeBitField::update(bitfield, check_type)) {}

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input& value() { return Node::input(0); }
  CheckType check_type() const { return CheckTypeBitField::decode(bitfield()); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  using CheckTypeBitField = NextBitField<CheckType, 1>;
};

class StringEqual : public FixedInputValueNodeT<2, StringEqual> {
  using Base = FixedInputValueNodeT<2, StringEqual>;

 public:
  explicit StringEqual(uint64_t bitfield) : Base(bitfield) {}
  static constexpr OpProperties kProperties =
      OpProperties::Call() | OpProperties::LazyDeopt();

  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  Input& lhs() { return Node::input(0); }
  Input& rhs() { return Node::input(1); }

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class TaggedEqual : public FixedInputValueNodeT<2, TaggedEqual> {
  using Base = FixedInputValueNodeT<2, TaggedEqual>;

 public:
  explicit TaggedEqual(uint64_t bitfield) : Base(bitfield) {}

  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  Input& lhs() { return Node::input(0); }
  Input& rhs() { return Node::input(1); }

  void MarkTaggedInputsAsDecompressing() {
    // Don't need to decompress to compare reference equality.
  }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class TaggedNotEqual : public FixedInputValueNodeT<2, TaggedNotEqual> {
  using Base = FixedInputValueNodeT<2, TaggedNotEqual>;

 public:
  explicit TaggedNotEqual(uint64_t bitfield) : Base(bitfield) {}

  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  Input& lhs() { return Node::input(0); }
  Input& rhs() { return Node::input(1); }

  void MarkTaggedInputsAsDecompressing() {
    // Don't need to decompress to compare reference equality.
  }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class TestInstanceOf : public FixedInputValueNodeT<3, TestInstanceOf> {
  using Base = FixedInputValueNodeT<3, TestInstanceOf>;

 public:
  explicit TestInstanceOf(uint64_t bitfield, compiler::FeedbackSource feedback)
      : Base(bitfield), feedback_(feedback) {}

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::JSCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged,
      ValueRepresentation::kTagged};

  Input& context() { return input(0); }
  Input& object() { return input(1); }
  Input& callable() { return input(2); }
  compiler::FeedbackSource feedback() const { return feedback_; }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  const compiler::FeedbackSource feedback_;
};

class TestUndetectable : public FixedInputValueNodeT<1, TestUndetectable> {
  using Base = FixedInputValueNodeT<1, TestUndetectable>;

 public:
  explicit TestUndetectable(uint64_t bitfield, CheckType check_type)
      : Base(CheckTypeBitField::update(bitfield, check_type)) {}

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input& value() { return Node::input(0); }
  CheckType check_type() const { return CheckTypeBitField::decode(bitfield()); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  using CheckTypeBitField = NextBitField<CheckType, 1>;
};

class TestTypeOf : public FixedInputValueNodeT<1, TestTypeOf> {
  using Base = FixedInputValueNodeT<1, TestTypeOf>;

 public:
  explicit TestTypeOf(uint64_t bitfield,
                      interpreter::TestTypeOfFlags::LiteralFlag literal)
      : Base(bitfield), literal_(literal) {}

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input& value() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  interpreter::TestTypeOfFlags::LiteralFlag literal_;
};

class ToName : public FixedInputValueNodeT<2, ToName> {
  using Base = FixedInputValueNodeT<2, ToName>;

 public:
  explicit ToName(uint64_t bitfield) : Base(bitfield) {}

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::JSCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  Input& context() { return Node::input(0); }
  Input& value_input() { return Node::input(1); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class ToNumberOrNumeric : public FixedInputValueNodeT<1, ToNumberOrNumeric> {
  using Base = FixedInputValueNodeT<1, ToNumberOrNumeric>;

 public:
  explicit ToNumberOrNumeric(uint64_t bitfield, Object::Conversion mode)
      : Base(bitfield), mode_(mode) {}

  static constexpr OpProperties kProperties =
      OpProperties::DeferredCall() | OpProperties::CanCallUserCode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input& value_input() { return Node::input(0); }
  Object::Conversion mode() const { return mode_; }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  const Object::Conversion mode_;
};

class DeleteProperty : public FixedInputValueNodeT<3, DeleteProperty> {
  using Base = FixedInputValueNodeT<3, DeleteProperty>;

 public:
  explicit DeleteProperty(uint64_t bitfield, LanguageMode mode)
      : Base(bitfield), mode_(mode) {}

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::JSCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged,
      ValueRepresentation::kTagged};

  Input& context() { return Node::input(0); }
  Input& object() { return Node::input(1); }
  Input& key() { return Node::input(2); }

  LanguageMode mode() const { return mode_; }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const LanguageMode mode_;
};

class GeneratorStore : public NodeT<GeneratorStore> {
  using Base = NodeT<GeneratorStore>;

 public:
  // We assume the context as fixed input.
  static constexpr int kContextIndex = 0;
  static constexpr int kGeneratorIndex = 1;
  static constexpr int kFixedInputCount = 2;

  // This ctor is used when for variable input counts.
  // Inputs must be initialized manually.
  GeneratorStore(uint64_t bitfield, ValueNode* context, ValueNode* generator,
                 int suspend_id, int bytecode_offset)
      : Base(bitfield),
        suspend_id_(suspend_id),
        bytecode_offset_(bytecode_offset) {
    set_input(kContextIndex, context);
    set_input(kGeneratorIndex, generator);
  }

  static constexpr OpProperties kProperties = OpProperties::DeferredCall();

  int suspend_id() const { return suspend_id_; }
  int bytecode_offset() const { return bytecode_offset_; }

  Input& context_input() { return input(kContextIndex); }
  Input& generator_input() { return input(kGeneratorIndex); }

  int num_parameters_and_registers() const {
    return input_count() - kFixedInputCount;
  }
  Input& parameters_and_registers(int i) { return input(i + kFixedInputCount); }
  void set_parameters_and_registers(int i, ValueNode* node) {
    set_input(i + kFixedInputCount, node);
  }

  int MaxCallStackArgs() const;
  void VerifyInputs(MaglevGraphLabeller* graph_labeller) const;
  void MarkTaggedInputsAsDecompressing() {
    // Don't need to decompress to store.
  }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  const int suspend_id_;
  const int bytecode_offset_;
};

class TryOnStackReplacement : public FixedInputNodeT<1, TryOnStackReplacement> {
  using Base = FixedInputNodeT<1, TryOnStackReplacement>;

 public:
  explicit TryOnStackReplacement(uint64_t bitfield, int32_t loop_depth,
                                 FeedbackSlot feedback_slot,
                                 BytecodeOffset osr_offset,
                                 MaglevCompilationUnit* unit)
      : Base(bitfield),
        loop_depth_(loop_depth),
        feedback_slot_(feedback_slot),
        osr_offset_(osr_offset),
        unit_(unit) {}

  static constexpr OpProperties kProperties = OpProperties::DeferredCall() |
                                              OpProperties::EagerDeopt() |
                                              OpProperties::Call();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input& closure() { return Node::input(0); }

  const MaglevCompilationUnit* unit() const { return unit_; }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  // For OSR.
  const int32_t loop_depth_;
  const FeedbackSlot feedback_slot_;
  const BytecodeOffset osr_offset_;
  MaglevCompilationUnit* const unit_;
};

class ForInPrepare : public FixedInputValueNodeT<2, ForInPrepare> {
  using Base = FixedInputValueNodeT<2, ForInPrepare>;

 public:
  explicit ForInPrepare(uint64_t bitfield, compiler::FeedbackSource& feedback)
      : Base(bitfield), feedback_(feedback) {}

  static constexpr OpProperties kProperties =
      OpProperties::GenericRuntimeOrBuiltinCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  compiler::FeedbackSource feedback() const { return feedback_; }

  Input& context() { return Node::input(0); }
  Input& enumerator() { return Node::input(1); }

  int ReturnCount() const { return 2; }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  const compiler::FeedbackSource feedback_;
};

class ForInNext : public FixedInputValueNodeT<5, ForInNext> {
  using Base = FixedInputValueNodeT<5, ForInNext>;

 public:
  explicit ForInNext(uint64_t bitfield, compiler::FeedbackSource& feedback)
      : Base(bitfield), feedback_(feedback) {}

  static constexpr OpProperties kProperties = OpProperties::JSCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged,
      ValueRepresentation::kTagged, ValueRepresentation::kTagged,
      ValueRepresentation::kTagged};

  compiler::FeedbackSource feedback() const { return feedback_; }

  Input& context() { return Node::input(0); }
  Input& receiver() { return Node::input(1); }
  Input& cache_array() { return Node::input(2); }
  Input& cache_type() { return Node::input(3); }
  Input& cache_index() { return Node::input(4); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  const compiler::FeedbackSource feedback_;
};

class GetIterator : public FixedInputValueNodeT<2, GetIterator> {
  using Base = FixedInputValueNodeT<2, GetIterator>;

 public:
  explicit GetIterator(uint64_t bitfield, int load_slot, int call_slot,
                       compiler::FeedbackVectorRef feedback)
      : Base(bitfield),
        load_slot_(load_slot),
        call_slot_(call_slot),
        feedback_(feedback.object()) {}

  static constexpr OpProperties kProperties = OpProperties::JSCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  Input& context() { return input(0); }
  Input& receiver() { return input(1); }

  int load_slot() const { return load_slot_; }
  int call_slot() const { return call_slot_; }
  Handle<FeedbackVector> feedback() const { return feedback_; }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  const int load_slot_;
  const int call_slot_;
  const Handle<FeedbackVector> feedback_;
};

class GetSecondReturnedValue
    : public FixedInputValueNodeT<0, GetSecondReturnedValue> {
  using Base = FixedInputValueNodeT<0, GetSecondReturnedValue>;

 public:
  explicit GetSecondReturnedValue(uint64_t bitfield) : Base(bitfield) {}

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class ToObject : public FixedInputValueNodeT<2, ToObject> {
  using Base = FixedInputValueNodeT<2, ToObject>;

 public:
  explicit ToObject(uint64_t bitfield, CheckType check_type)
      : Base(CheckTypeBitField::update(bitfield, check_type)) {}

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::JSCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  Input& context() { return Node::input(0); }
  Input& value_input() { return Node::input(1); }
  CheckType check_type() const { return CheckTypeBitField::decode(bitfield()); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  using CheckTypeBitField = NextBitField<CheckType, 1>;
};

class ToString : public FixedInputValueNodeT<2, ToString> {
  using Base = FixedInputValueNodeT<2, ToString>;

 public:
  enum ConversionMode { kConvertSymbol, kThrowOnSymbol };
  explicit ToString(uint64_t bitfield, ConversionMode mode)
      : Base(ConversionModeBitField::update(bitfield, mode)) {}

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::JSCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  Input& context() { return Node::input(0); }
  Input& value_input() { return Node::input(1); }
  ConversionMode mode() const {
    return ConversionModeBitField::decode(bitfield());
  }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  using ConversionModeBitField = NextBitField<ConversionMode, 1>;
};

class NumberToString : public FixedInputValueNodeT<1, NumberToString> {
  using Base = FixedInputValueNodeT<1, NumberToString>;

 public:
  explicit NumberToString(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::Call() | OpProperties::LazyDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input& value_input() { return Node::input(0); }

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class GeneratorRestoreRegister
    : public FixedInputValueNodeT<2, GeneratorRestoreRegister> {
  using Base = FixedInputValueNodeT<2, GeneratorRestoreRegister>;

 public:
  explicit GeneratorRestoreRegister(uint64_t bitfield, int index)
      : Base(bitfield), index_(index) {}

  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  Input& array_input() { return input(0); }
  Input& stale_input() { return input(1); }
  int index() const { return index_; }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  const int index_;
};

class InitialValue : public FixedInputValueNodeT<0, InitialValue> {
  using Base = FixedInputValueNodeT<0, InitialValue>;

 public:
  explicit InitialValue(uint64_t bitfield, interpreter::Register source);

  interpreter::Register source() const { return source_; }
  uint32_t stack_slot() const;
  static uint32_t stack_slot(uint32_t register_idx);

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const interpreter::Register source_;
};

class RegisterInput : public FixedInputValueNodeT<0, RegisterInput> {
  using Base = FixedInputValueNodeT<0, RegisterInput>;

 public:
  explicit RegisterInput(uint64_t bitfield, Register input)
      : Base(bitfield), input_(input) {}

  Register input() const { return input_; }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const Register input_;
};

class SmiConstant : public FixedInputValueNodeT<0, SmiConstant> {
  using Base = FixedInputValueNodeT<0, SmiConstant>;

 public:
  using OutputRegister = Register;

  explicit SmiConstant(uint64_t bitfield, Smi value)
      : Base(bitfield), value_(value) {}

  Smi value() const { return value_; }

  bool ToBoolean(LocalIsolate* local_isolate) const {
    return value_ != Smi::FromInt(0);
  }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

  void DoLoadToRegister(MaglevAssembler*, OutputRegister);
  Handle<Object> DoReify(LocalIsolate* isolate) const;

 private:
  const Smi value_;
};

class ExternalConstant : public FixedInputValueNodeT<0, ExternalConstant> {
  using Base = FixedInputValueNodeT<0, ExternalConstant>;

 public:
  using OutputRegister = Register;

  explicit ExternalConstant(uint64_t bitfield,
                            const ExternalReference& reference)
      : Base(bitfield), reference_(reference) {}

  static constexpr OpProperties kProperties =
      OpProperties::Pure() | OpProperties::ExternalReference();

  ExternalReference reference() const { return reference_; }

  bool ToBoolean(LocalIsolate* local_isolate) const { UNREACHABLE(); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

  void DoLoadToRegister(MaglevAssembler*, OutputRegister);
  Handle<Object> DoReify(LocalIsolate* isolate) const;

 private:
  const ExternalReference reference_;
};

class Constant : public FixedInputValueNodeT<0, Constant> {
  using Base = FixedInputValueNodeT<0, Constant>;

 public:
  using OutputRegister = Register;

  explicit Constant(uint64_t bitfield, compiler::HeapObjectRef object)
      : Base(bitfield), object_(object) {}

  bool ToBoolean(LocalIsolate* local_isolate) const {
    return object_.object()->BooleanValue(local_isolate);
  }

  bool IsTheHole(compiler::JSHeapBroker* broker) const {
    return object_.IsTheHole();
  }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

  compiler::HeapObjectRef object() { return object_; }

  void DoLoadToRegister(MaglevAssembler*, OutputRegister);
  Handle<Object> DoReify(LocalIsolate* isolate) const;

  compiler::HeapObjectRef ref() const { return object_; }

 private:
  const compiler::HeapObjectRef object_;
};

class RootConstant : public FixedInputValueNodeT<0, RootConstant> {
  using Base = FixedInputValueNodeT<0, RootConstant>;

 public:
  using OutputRegister = Register;

  explicit RootConstant(uint64_t bitfield, RootIndex index)
      : Base(bitfield), index_(index) {}

  bool ToBoolean(LocalIsolate* local_isolate) const;

  RootIndex index() const { return index_; }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

  void DoLoadToRegister(MaglevAssembler*, OutputRegister);
  Handle<Object> DoReify(LocalIsolate* isolate) const;

 private:
  const RootIndex index_;
};

class CreateArrayLiteral : public FixedInputValueNodeT<0, CreateArrayLiteral> {
  using Base = FixedInputValueNodeT<0, CreateArrayLiteral>;

 public:
  explicit CreateArrayLiteral(uint64_t bitfield,
                              compiler::HeapObjectRef constant_elements,
                              const compiler::FeedbackSource& feedback,
                              int flags)
      : Base(bitfield),
        constant_elements_(constant_elements),
        feedback_(feedback),
        flags_(flags) {}

  compiler::HeapObjectRef constant_elements() { return constant_elements_; }
  compiler::FeedbackSource feedback() const { return feedback_; }
  int flags() const { return flags_; }

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::Call() |
                                              OpProperties::CanThrow() |
                                              OpProperties::LazyDeopt();

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  const compiler::HeapObjectRef constant_elements_;
  const compiler::FeedbackSource feedback_;
  const int flags_;
};

class CreateShallowArrayLiteral
    : public FixedInputValueNodeT<0, CreateShallowArrayLiteral> {
  using Base = FixedInputValueNodeT<0, CreateShallowArrayLiteral>;

 public:
  explicit CreateShallowArrayLiteral(uint64_t bitfield,
                                     compiler::HeapObjectRef constant_elements,
                                     const compiler::FeedbackSource& feedback,
                                     int flags)
      : Base(bitfield),
        constant_elements_(constant_elements),
        feedback_(feedback),
        flags_(flags) {}

  compiler::HeapObjectRef constant_elements() { return constant_elements_; }
  compiler::FeedbackSource feedback() const { return feedback_; }
  int flags() const { return flags_; }

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties =
      OpProperties::GenericRuntimeOrBuiltinCall();

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  const compiler::HeapObjectRef constant_elements_;
  const compiler::FeedbackSource feedback_;
  const int flags_;
};

class CreateObjectLiteral
    : public FixedInputValueNodeT<0, CreateObjectLiteral> {
  using Base = FixedInputValueNodeT<0, CreateObjectLiteral>;

 public:
  explicit CreateObjectLiteral(
      uint64_t bitfield,
      compiler::ObjectBoilerplateDescriptionRef boilerplate_descriptor,
      const compiler::FeedbackSource& feedback, int flags)
      : Base(bitfield),
        boilerplate_descriptor_(boilerplate_descriptor),
        feedback_(feedback),
        flags_(flags) {}

  compiler::ObjectBoilerplateDescriptionRef boilerplate_descriptor() {
    return boilerplate_descriptor_;
  }
  compiler::FeedbackSource feedback() const { return feedback_; }
  int flags() const { return flags_; }

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::Call() |
                                              OpProperties::CanThrow() |
                                              OpProperties::LazyDeopt();

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  const compiler::ObjectBoilerplateDescriptionRef boilerplate_descriptor_;
  const compiler::FeedbackSource feedback_;
  const int flags_;
};

class CreateShallowObjectLiteral
    : public FixedInputValueNodeT<0, CreateShallowObjectLiteral> {
  using Base = FixedInputValueNodeT<0, CreateShallowObjectLiteral>;

 public:
  explicit CreateShallowObjectLiteral(
      uint64_t bitfield,
      compiler::ObjectBoilerplateDescriptionRef boilerplate_descriptor,
      const compiler::FeedbackSource& feedback, int flags)
      : Base(bitfield),
        boilerplate_descriptor_(boilerplate_descriptor),
        feedback_(feedback),
        flags_(flags) {}

  // TODO(victorgomes): We should not need a boilerplate descriptor in
  // CreateShallowObjectLiteral.
  compiler::ObjectBoilerplateDescriptionRef boilerplate_descriptor() {
    return boilerplate_descriptor_;
  }
  compiler::FeedbackSource feedback() const { return feedback_; }
  int flags() const { return flags_; }

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties =
      OpProperties::GenericRuntimeOrBuiltinCall();

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  const compiler::ObjectBoilerplateDescriptionRef boilerplate_descriptor_;
  const compiler::FeedbackSource feedback_;
  const int flags_;
};

class AllocateRaw : public FixedInputValueNodeT<0, AllocateRaw> {
  using Base = FixedInputValueNodeT<0, AllocateRaw>;

 public:
  explicit AllocateRaw(uint64_t bitfield, AllocationType allocation_type,
                       int size)
      : Base(bitfield), allocation_type_(allocation_type), size_(size) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanAllocate() | OpProperties::DeferredCall();

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

  AllocationType allocation_type() const { return allocation_type_; }
  int size() const { return size_; }

  // Allow increasing the size for allocation folding.
  void extend(int size) {
    DCHECK_GT(size, 0);
    size_ += size;
  }

 private:
  AllocationType allocation_type_;
  int size_;
};

class FoldedAllocation : public FixedInputValueNodeT<1, FoldedAllocation> {
  using Base = FixedInputValueNodeT<1, FoldedAllocation>;

 public:
  explicit FoldedAllocation(uint64_t bitfield, int offset)
      : Base(bitfield), offset_(offset) {}

  Input& raw_allocation() { return input(0); }

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

  void VerifyInputs(MaglevGraphLabeller* graph_labeller) const;

  int offset() const { return offset_; }

 private:
  int offset_;
};

class CreateFunctionContext
    : public FixedInputValueNodeT<1, CreateFunctionContext> {
  using Base = FixedInputValueNodeT<1, CreateFunctionContext>;

 public:
  explicit CreateFunctionContext(uint64_t bitfield,
                                 compiler::ScopeInfoRef scope_info,
                                 uint32_t slot_count, ScopeType scope_type)
      : Base(bitfield),
        scope_info_(scope_info),
        slot_count_(slot_count),
        scope_type_(scope_type) {}

  compiler::ScopeInfoRef scope_info() const { return scope_info_; }
  uint32_t slot_count() const { return slot_count_; }
  ScopeType scope_type() const { return scope_type_; }

  Input& context() { return input(0); }

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties =
      OpProperties::GenericRuntimeOrBuiltinCall();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const compiler::ScopeInfoRef scope_info_;
  const uint32_t slot_count_;
  ScopeType scope_type_;
};

class FastCreateClosure : public FixedInputValueNodeT<1, FastCreateClosure> {
  using Base = FixedInputValueNodeT<1, FastCreateClosure>;

 public:
  explicit FastCreateClosure(
      uint64_t bitfield, compiler::SharedFunctionInfoRef shared_function_info,
      compiler::FeedbackCellRef feedback_cell)
      : Base(bitfield),
        shared_function_info_(shared_function_info),
        feedback_cell_(feedback_cell) {}

  compiler::SharedFunctionInfoRef shared_function_info() const {
    return shared_function_info_;
  }
  compiler::FeedbackCellRef feedback_cell() const { return feedback_cell_; }

  Input& context() { return input(0); }

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::Call() |
                                              OpProperties::CanAllocate() |
                                              OpProperties::LazyDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const compiler::SharedFunctionInfoRef shared_function_info_;
  const compiler::FeedbackCellRef feedback_cell_;
};

class CreateRegExpLiteral
    : public FixedInputValueNodeT<0, CreateRegExpLiteral> {
  using Base = FixedInputValueNodeT<0, CreateRegExpLiteral>;

 public:
  explicit CreateRegExpLiteral(uint64_t bitfield, compiler::StringRef pattern,
                               const compiler::FeedbackSource& feedback,
                               int flags)
      : Base(bitfield), pattern_(pattern), feedback_(feedback), flags_(flags) {}

  compiler::StringRef pattern() { return pattern_; }
  compiler::FeedbackSource feedback() const { return feedback_; }
  int flags() const { return flags_; }

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::Call();

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  compiler::StringRef pattern_;
  const compiler::FeedbackSource feedback_;
  const int flags_;
};

class CreateClosure : public FixedInputValueNodeT<1, CreateClosure> {
  using Base = FixedInputValueNodeT<1, CreateClosure>;

 public:
  explicit CreateClosure(uint64_t bitfield,
                         compiler::SharedFunctionInfoRef shared_function_info,
                         compiler::FeedbackCellRef feedback_cell,
                         bool pretenured)
      : Base(bitfield),
        shared_function_info_(shared_function_info),
        feedback_cell_(feedback_cell),
        pretenured_(pretenured) {}

  compiler::SharedFunctionInfoRef shared_function_info() const {
    return shared_function_info_;
  }
  compiler::FeedbackCellRef feedback_cell() const { return feedback_cell_; }
  bool pretenured() const { return pretenured_; }

  Input& context() { return input(0); }

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::Call();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const compiler::SharedFunctionInfoRef shared_function_info_;
  const compiler::FeedbackCellRef feedback_cell_;
  const bool pretenured_;
};

#define ASSERT_CONDITION(V) \
  V(Equal)                  \
  V(NotEqual)               \
  V(LessThan)               \
  V(LessThanEqual)          \
  V(GreaterThan)            \
  V(GreaterThanEqual)       \
  V(UnsignedLessThan)       \
  V(UnsignedLessThanEqual)  \
  V(UnsignedGreaterThan)    \
  V(UnsignedGreaterThanEqual)

enum class AssertCondition {
#define D(Name) k##Name,
  ASSERT_CONDITION(D)
#undef D
};
static constexpr int kNumAssertConditions =
#define D(Name) +1
    0 ASSERT_CONDITION(D);
#undef D

inline std::ostream& operator<<(std::ostream& os, const AssertCondition cond) {
  switch (cond) {
#define CASE(Name)               \
  case AssertCondition::k##Name: \
    os << #Name;                 \
    break;
    ASSERT_CONDITION(CASE)
#undef CASE
  }
  return os;
}

class AssertInt32 : public FixedInputNodeT<2, AssertInt32> {
  using Base = FixedInputNodeT<2, AssertInt32>;

 public:
  explicit AssertInt32(uint64_t bitfield, AssertCondition condition,
                       AbortReason reason)
      : Base(bitfield), condition_(condition), reason_(reason) {}

  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kInt32, ValueRepresentation::kInt32};

  Input& left_input() { return input(0); }
  Input& right_input() { return input(1); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  AssertCondition condition_;
  AbortReason reason_;
};

class CheckMaps : public FixedInputNodeT<1, CheckMaps> {
  using Base = FixedInputNodeT<1, CheckMaps>;

 public:
  explicit CheckMaps(uint64_t bitfield, const compiler::ZoneRefSet<Map>& maps,
                     CheckType check_type)
      : Base(CheckTypeBitField::update(bitfield, check_type)), maps_(maps) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  const compiler::ZoneRefSet<Map>& maps() const { return maps_; }
  CheckType check_type() const { return CheckTypeBitField::decode(bitfield()); }

  static constexpr int kReceiverIndex = 0;
  Input& receiver_input() { return input(kReceiverIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  using CheckTypeBitField = NextBitField<CheckType, 1>;
  const compiler::ZoneRefSet<Map> maps_;
};

class CheckValue : public FixedInputNodeT<1, CheckValue> {
  using Base = FixedInputNodeT<1, CheckValue>;

 public:
  explicit CheckValue(uint64_t bitfield, compiler::HeapObjectRef value)
      : Base(bitfield), value_(value) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  compiler::HeapObjectRef value() const { return value_; }

  static constexpr int kTargetIndex = 0;
  Input& target_input() { return input(kTargetIndex); }

  void MarkTaggedInputsAsDecompressing() {
    // Don't need to decompress to compare reference equality.
  }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const compiler::HeapObjectRef value_;
};

class CheckValueEqualsInt32 : public FixedInputNodeT<1, CheckValueEqualsInt32> {
  using Base = FixedInputNodeT<1, CheckValueEqualsInt32>;

 public:
  explicit CheckValueEqualsInt32(uint64_t bitfield, int32_t value)
      : Base(bitfield), value_(value) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kInt32};

  int32_t value() const { return value_; }

  static constexpr int kTargetIndex = 0;
  Input& target_input() { return input(kTargetIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const int32_t value_;
};

class CheckValueEqualsFloat64
    : public FixedInputNodeT<1, CheckValueEqualsFloat64> {
  using Base = FixedInputNodeT<1, CheckValueEqualsFloat64>;

 public:
  explicit CheckValueEqualsFloat64(uint64_t bitfield, double value)
      : Base(bitfield), value_(value) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kFloat64};

  double value() const { return value_; }

  static constexpr int kTargetIndex = 0;
  Input& target_input() { return input(kTargetIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const double value_;
};

class CheckValueEqualsString
    : public FixedInputNodeT<1, CheckValueEqualsString> {
  using Base = FixedInputNodeT<1, CheckValueEqualsString>;

 public:
  explicit CheckValueEqualsString(uint64_t bitfield,
                                  compiler::InternalizedStringRef value)
      : Base(bitfield), value_(value) {}

  // Can allocate if strings are flattened for comparison.
  static constexpr OpProperties kProperties = OpProperties::CanAllocate() |
                                              OpProperties::EagerDeopt() |
                                              OpProperties::DeferredCall();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  compiler::InternalizedStringRef value() const { return value_; }

  static constexpr int kTargetIndex = 0;
  Input& target_input() { return input(kTargetIndex); }

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const compiler::InternalizedStringRef value_;
};

class CheckDynamicValue : public FixedInputNodeT<2, CheckDynamicValue> {
  using Base = FixedInputNodeT<2, CheckDynamicValue>;

 public:
  explicit CheckDynamicValue(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  static constexpr int kFirstIndex = 0;
  static constexpr int kSecondIndex = 1;
  Input& first_input() { return input(kFirstIndex); }
  Input& second_input() { return input(kSecondIndex); }

  void MarkTaggedInputsAsDecompressing() {
    // Don't need to decompress to compare reference equality.
  }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class CheckSmi : public FixedInputNodeT<1, CheckSmi> {
  using Base = FixedInputNodeT<1, CheckSmi>;

 public:
  explicit CheckSmi(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  static constexpr int kReceiverIndex = 0;
  Input& receiver_input() { return input(kReceiverIndex); }

  using Node::set_input;

  void MarkTaggedInputsAsDecompressing() {
    // Don't need to decompress to check Smi bits.
  }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class CheckNumber : public FixedInputNodeT<1, CheckNumber> {
  using Base = FixedInputNodeT<1, CheckNumber>;

 public:
  explicit CheckNumber(uint64_t bitfield, Object::Conversion mode)
      : Base(bitfield), mode_(mode) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  static constexpr int kReceiverIndex = 0;
  Input& receiver_input() { return input(kReceiverIndex); }
  Object::Conversion mode() const { return mode_; }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  const Object::Conversion mode_;
};

class CheckHeapObject : public FixedInputNodeT<1, CheckHeapObject> {
  using Base = FixedInputNodeT<1, CheckHeapObject>;

 public:
  explicit CheckHeapObject(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  static constexpr int kReceiverIndex = 0;
  Input& receiver_input() { return input(kReceiverIndex); }

  void MarkTaggedInputsAsDecompressing() {
    // Don't need to decompress to check Smi bits.
  }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class CheckSymbol : public FixedInputNodeT<1, CheckSymbol> {
  using Base = FixedInputNodeT<1, CheckSymbol>;

 public:
  explicit CheckSymbol(uint64_t bitfield, CheckType check_type)
      : Base(CheckTypeBitField::update(bitfield, check_type)) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  static constexpr int kReceiverIndex = 0;
  Input& receiver_input() { return input(kReceiverIndex); }
  CheckType check_type() const { return CheckTypeBitField::decode(bitfield()); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  using CheckTypeBitField = NextBitField<CheckType, 1>;
};

class CheckInstanceType : public FixedInputNodeT<1, CheckInstanceType> {
  using Base = FixedInputNodeT<1, CheckInstanceType>;

 public:
  explicit CheckInstanceType(uint64_t bitfield, CheckType check_type,
                             InstanceType instance_type)
      : CheckInstanceType(bitfield, check_type, instance_type, instance_type) {}
  explicit CheckInstanceType(uint64_t bitfield, CheckType check_type,
                             InstanceType first_instance_type,
                             InstanceType last_instance_type)
      : Base(CheckTypeBitField::update(bitfield, check_type)),
        first_instance_type_(first_instance_type),
        last_instance_type_(last_instance_type) {
    DCHECK_LE(first_instance_type, last_instance_type);
  }

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  static constexpr int kReceiverIndex = 0;
  Input& receiver_input() { return input(kReceiverIndex); }

  CheckType check_type() const { return CheckTypeBitField::decode(bitfield()); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  using CheckTypeBitField = NextBitField<CheckType, 1>;
  const InstanceType first_instance_type_;
  const InstanceType last_instance_type_;
};

class CheckString : public FixedInputNodeT<1, CheckString> {
  using Base = FixedInputNodeT<1, CheckString>;

 public:
  explicit CheckString(uint64_t bitfield, CheckType check_type)
      : Base(CheckTypeBitField::update(bitfield, check_type)) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  static constexpr int kReceiverIndex = 0;
  Input& receiver_input() { return input(kReceiverIndex); }
  CheckType check_type() const { return CheckTypeBitField::decode(bitfield()); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  using CheckTypeBitField = NextBitField<CheckType, 1>;
};

class CheckMapsWithMigration
    : public FixedInputNodeT<1, CheckMapsWithMigration> {
  using Base = FixedInputNodeT<1, CheckMapsWithMigration>;

 public:
  explicit CheckMapsWithMigration(uint64_t bitfield,
                                  const compiler::ZoneRefSet<Map>& maps,
                                  CheckType check_type)
      : Base(CheckTypeBitField::update(bitfield, check_type)), maps_(maps) {}

  static constexpr OpProperties kProperties =
      OpProperties::EagerDeopt() | OpProperties::DeferredCall() |
      OpProperties::CanAllocate() | OpProperties::CanWrite() |
      OpProperties::CanRead();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  const compiler::ZoneRefSet<Map>& maps() const { return maps_; }

  static constexpr int kReceiverIndex = 0;
  Input& receiver_input() { return input(kReceiverIndex); }
  CheckType check_type() const { return CheckTypeBitField::decode(bitfield()); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  using CheckTypeBitField = NextBitField<CheckType, 1>;
  const compiler::ZoneRefSet<Map> maps_;
};

class CheckFixedArrayNonEmpty
    : public FixedInputNodeT<1, CheckFixedArrayNonEmpty> {
  using Base = FixedInputNodeT<1, CheckFixedArrayNonEmpty>;

 public:
  explicit CheckFixedArrayNonEmpty(uint64_t bitfield) : Base(bitfield) {}
  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  static constexpr int kReceiverIndex = 0;
  Input& receiver_input() { return input(kReceiverIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class CheckJSDataViewBounds : public FixedInputNodeT<2, CheckJSDataViewBounds> {
  using Base = FixedInputNodeT<2, CheckJSDataViewBounds>;

 public:
  explicit CheckJSDataViewBounds(uint64_t bitfield,
                                 ExternalArrayType element_type)
      : Base(bitfield), element_type_(element_type) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kInt32};

  static constexpr int kReceiverIndex = 0;
  static constexpr int kIndexIndex = 1;
  Input& receiver_input() { return input(kReceiverIndex); }
  Input& index_input() { return input(kIndexIndex); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  ExternalArrayType element_type_;
};

class CheckJSTypedArrayBounds
    : public FixedInputNodeT<2, CheckJSTypedArrayBounds> {
  using Base = FixedInputNodeT<2, CheckJSTypedArrayBounds>;

 public:
  explicit CheckJSTypedArrayBounds(uint64_t bitfield,
                                   ElementsKind elements_kind)
      : Base(bitfield), elements_kind_(elements_kind) {}
  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kUint32};

  static constexpr int kReceiverIndex = 0;
  static constexpr int kIndexIndex = 1;
  Input& receiver_input() { return input(kReceiverIndex); }
  Input& index_input() { return input(kIndexIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  ElementsKind elements_kind_;
};

class CheckInt32Condition : public FixedInputNodeT<2, CheckInt32Condition> {
  using Base = FixedInputNodeT<2, CheckInt32Condition>;

 public:
  explicit CheckInt32Condition(uint64_t bitfield, AssertCondition condition,
                               DeoptimizeReason reason)
      : Base(bitfield | ConditionField::encode(condition) |
             ReasonField::encode(reason)) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kInt32, ValueRepresentation::kInt32};

  static constexpr int kLeftIndex = 0;
  static constexpr int kRightIndex = 1;
  Input& left_input() { return input(kLeftIndex); }
  Input& right_input() { return input(kRightIndex); }

  AssertCondition condition() const {
    return ConditionField::decode(bitfield());
  }
  DeoptimizeReason reason() const { return ReasonField::decode(bitfield()); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  using ConditionField =
      NextBitField<AssertCondition, base::bits::WhichPowerOfTwo<size_t>(
                                        base::bits::RoundUpToPowerOfTwo32(
                                            kNumAssertConditions))>;
  using ReasonField =
      ConditionField::Next<DeoptimizeReason,
                           base::bits::WhichPowerOfTwo<size_t>(
                               base::bits::RoundUpToPowerOfTwo32(
                                   kDeoptimizeReasonCount))>;
};

class DebugBreak : public FixedInputNodeT<0, DebugBreak> {
  using Base = FixedInputNodeT<0, DebugBreak>;

 public:
  explicit DebugBreak(uint64_t bitfield) : Base(bitfield) {}

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class FunctionEntryStackCheck
    : public FixedInputNodeT<0, FunctionEntryStackCheck> {
  using Base = FixedInputNodeT<0, FunctionEntryStackCheck>;

 public:
  explicit FunctionEntryStackCheck(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::DeferredCall() | OpProperties::LazyDeopt();

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class CheckedInternalizedString
    : public FixedInputValueNodeT<1, CheckedInternalizedString> {
  using Base = FixedInputValueNodeT<1, CheckedInternalizedString>;

 public:
  explicit CheckedInternalizedString(uint64_t bitfield, CheckType check_type)
      : Base(CheckTypeBitField::update(bitfield, check_type)) {
    CHECK_EQ(properties().value_representation(), ValueRepresentation::kTagged);
  }

  static constexpr OpProperties kProperties =
      OpProperties::EagerDeopt() | OpProperties::TaggedValue();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  static constexpr int kObjectIndex = 0;
  Input& object_input() { return Node::input(kObjectIndex); }
  CheckType check_type() const { return CheckTypeBitField::decode(bitfield()); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  using CheckTypeBitField = NextBitField<CheckType, 1>;
};

class CheckedObjectToIndex
    : public FixedInputValueNodeT<1, CheckedObjectToIndex> {
  using Base = FixedInputValueNodeT<1, CheckedObjectToIndex>;

 public:
  explicit CheckedObjectToIndex(uint64_t bitfield, CheckType check_type)
      : Base(CheckTypeBitField::update(bitfield, check_type)) {}

  static constexpr OpProperties kProperties =
      OpProperties::EagerDeopt() | OpProperties::Int32() |
      OpProperties::DeferredCall() | OpProperties::ConversionNode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  static constexpr int kObjectIndex = 0;
  Input& object_input() { return Node::input(kObjectIndex); }
  CheckType check_type() const { return CheckTypeBitField::decode(bitfield()); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  using CheckTypeBitField = NextBitField<CheckType, 1>;
};

class GetTemplateObject : public FixedInputValueNodeT<1, GetTemplateObject> {
  using Base = FixedInputValueNodeT<1, GetTemplateObject>;

 public:
  explicit GetTemplateObject(
      uint64_t bitfield, compiler::SharedFunctionInfoRef shared_function_info,
      const compiler::FeedbackSource& feedback)
      : Base(bitfield),
        shared_function_info_(shared_function_info),
        feedback_(feedback) {}

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties =
      OpProperties::GenericRuntimeOrBuiltinCall();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input& description() { return input(0); }

  compiler::SharedFunctionInfoRef shared_function_info() {
    return shared_function_info_;
  }
  compiler::FeedbackSource feedback() const { return feedback_; }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  compiler::SharedFunctionInfoRef shared_function_info_;
  const compiler::FeedbackSource feedback_;
};

class HasInPrototypeChain
    : public FixedInputValueNodeT<1, HasInPrototypeChain> {
  using Base = FixedInputValueNodeT<1, HasInPrototypeChain>;

 public:
  explicit HasInPrototypeChain(uint64_t bitfield,
                               compiler::HeapObjectRef prototype)
      : Base(bitfield), prototype_(prototype) {}

  // The implementation can enter user code in the deferred call (due to
  // proxied getPrototypeOf).
  static constexpr OpProperties kProperties =
      OpProperties::DeferredCall() | OpProperties::CanCallUserCode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input& object() { return input(0); }

  compiler::HeapObjectRef prototype() { return prototype_; }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  compiler::HeapObjectRef prototype_;
};

class BuiltinStringFromCharCode
    : public FixedInputValueNodeT<1, BuiltinStringFromCharCode> {
  using Base = FixedInputValueNodeT<1, BuiltinStringFromCharCode>;

 public:
  explicit BuiltinStringFromCharCode(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanAllocate() | OpProperties::DeferredCall();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kInt32};

  Input& code_input() { return input(0); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class BuiltinStringPrototypeCharCodeOrCodePointAt
    : public FixedInputValueNodeT<2,
                                  BuiltinStringPrototypeCharCodeOrCodePointAt> {
  using Base =
      FixedInputValueNodeT<2, BuiltinStringPrototypeCharCodeOrCodePointAt>;

 public:
  enum Mode {
    kCharCodeAt,
    kCodePointAt,
  };

  explicit BuiltinStringPrototypeCharCodeOrCodePointAt(uint64_t bitfield,
                                                       Mode mode)
      : Base(bitfield), mode_(mode) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanAllocate() | OpProperties::CanRead() |
      OpProperties::DeferredCall() | OpProperties::Int32();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kInt32};

  static constexpr int kStringIndex = 0;
  static constexpr int kIndexIndex = 1;
  Input& string_input() { return input(kStringIndex); }
  Input& index_input() { return input(kIndexIndex); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  Mode mode_;
};

class PolymorphicAccessInfo {
 public:
  enum Kind {
    kNotFound,
    kConstant,
    kDataLoad,
    kModuleExport,
    kStringLength,
  };

  static PolymorphicAccessInfo NotFound(
      const ZoneVector<compiler::MapRef>& maps) {
    return PolymorphicAccessInfo(kNotFound, maps, Representation::Tagged());
  }
  static PolymorphicAccessInfo Constant(
      const ZoneVector<compiler::MapRef>& maps, compiler::ObjectRef constant) {
    return PolymorphicAccessInfo(kConstant, maps, Representation::Tagged(),
                                 constant);
  }
  static PolymorphicAccessInfo DataLoad(
      const ZoneVector<compiler::MapRef>& maps, Representation representation,
      compiler::OptionalJSObjectRef holder, FieldIndex field_index) {
    return PolymorphicAccessInfo(kDataLoad, maps, representation, holder,
                                 field_index);
  }
  static PolymorphicAccessInfo ModuleExport(
      const ZoneVector<compiler::MapRef>& maps, compiler::CellRef cell) {
    return PolymorphicAccessInfo(kModuleExport, maps, Representation::Tagged(),
                                 cell);
  }
  static PolymorphicAccessInfo StringLength(
      const ZoneVector<compiler::MapRef>& maps) {
    return PolymorphicAccessInfo(kStringLength, maps, Representation::Smi());
  }

  Kind kind() const { return kind_; }

  const ZoneVector<compiler::MapRef>& maps() const { return maps_; }

  Handle<Object> constant() const {
    DCHECK_EQ(kind_, kConstant);
    return constant_.object();
  }

  Handle<Cell> cell() const {
    DCHECK_EQ(kind_, kModuleExport);
    return constant_.AsCell().object();
  }

  compiler::OptionalJSObjectRef holder() const {
    DCHECK_EQ(kind_, kDataLoad);
    return data_load_.holder_;
  }

  FieldIndex field_index() const {
    DCHECK_EQ(kind_, kDataLoad);
    return data_load_.field_index_;
  }

  Representation field_representation() const { return representation_; }

 private:
  explicit PolymorphicAccessInfo(Kind kind,
                                 const ZoneVector<compiler::MapRef>& maps,
                                 Representation representation)
      : kind_(kind), maps_(maps), representation_(representation) {
    DCHECK(kind == kNotFound || kind == kStringLength);
  }

  PolymorphicAccessInfo(Kind kind, const ZoneVector<compiler::MapRef>& maps,
                        Representation representation,
                        compiler::ObjectRef constant)
      : kind_(kind),
        maps_(maps),
        representation_(representation),
        constant_(constant) {
    DCHECK(kind == kConstant || kind == kModuleExport);
  }

  PolymorphicAccessInfo(Kind kind, const ZoneVector<compiler::MapRef>& maps,
                        Representation representation,
                        compiler::OptionalJSObjectRef holder,
                        FieldIndex field_index)
      : kind_(kind),
        maps_(maps),
        representation_(representation),
        data_load_{holder, field_index} {
    DCHECK_EQ(kind, kDataLoad);
  }

  const Kind kind_;
  // TODO(victorgomes): Create a PolymorphicMapChecks and avoid the maps here.
  const ZoneVector<compiler::MapRef> maps_;
  const Representation representation_;
  union {
    const compiler::ObjectRef constant_;
    struct {
      const compiler::OptionalJSObjectRef holder_;
      const FieldIndex field_index_;
    } data_load_;
  };
};

class LoadPolymorphicTaggedField
    : public FixedInputValueNodeT<1, LoadPolymorphicTaggedField> {
  using Base = FixedInputValueNodeT<1, LoadPolymorphicTaggedField>;

 public:
  explicit LoadPolymorphicTaggedField(
      uint64_t bitfield, Representation field_representation,
      ZoneVector<PolymorphicAccessInfo>&& access_info)
      : Base(bitfield),
        field_representation_(field_representation),
        access_infos_(access_info) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanAllocate() | OpProperties::CanRead() |
      OpProperties::EagerDeopt() | OpProperties::DeferredCall();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  static constexpr int kObjectIndex = 0;
  Input& object_input() { return input(kObjectIndex); }

  Representation field_representation() const { return field_representation_; }
  const ZoneVector<PolymorphicAccessInfo> access_infos() const {
    return access_infos_;
  }

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  Representation field_representation_;
  ZoneVector<PolymorphicAccessInfo> access_infos_;
};

class LoadPolymorphicDoubleField
    : public FixedInputValueNodeT<1, LoadPolymorphicDoubleField> {
  using Base = FixedInputValueNodeT<1, LoadPolymorphicDoubleField>;

 public:
  explicit LoadPolymorphicDoubleField(
      uint64_t bitfield, ZoneVector<PolymorphicAccessInfo>&& access_info)
      : Base(bitfield), access_infos_(access_info) {}

  static constexpr OpProperties kProperties = OpProperties::CanRead() |
                                              OpProperties::EagerDeopt() |
                                              OpProperties::Float64();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  static constexpr int kObjectIndex = 0;
  Input& object_input() { return input(kObjectIndex); }
  const ZoneVector<PolymorphicAccessInfo> access_infos() const {
    return access_infos_;
  }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  ZoneVector<PolymorphicAccessInfo> access_infos_;
};

class LoadTaggedField : public FixedInputValueNodeT<1, LoadTaggedField> {
  using Base = FixedInputValueNodeT<1, LoadTaggedField>;

 public:
  explicit LoadTaggedField(uint64_t bitfield, int offset)
      : Base(bitfield), offset_(offset) {}

  static constexpr OpProperties kProperties = OpProperties::CanRead();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  int offset() const { return offset_; }

  static constexpr int kObjectIndex = 0;
  Input& object_input() { return input(kObjectIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const int offset_;
};

class LoadDoubleField : public FixedInputValueNodeT<1, LoadDoubleField> {
  using Base = FixedInputValueNodeT<1, LoadDoubleField>;

 public:
  explicit LoadDoubleField(uint64_t bitfield, int offset)
      : Base(bitfield), offset_(offset) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanRead() | OpProperties::Float64();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  int offset() const { return offset_; }

  static constexpr int kObjectIndex = 0;
  Input& object_input() { return input(kObjectIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const int offset_;
};

class LoadTaggedFieldByFieldIndex
    : public FixedInputValueNodeT<2, LoadTaggedFieldByFieldIndex> {
  using Base = FixedInputValueNodeT<2, LoadTaggedFieldByFieldIndex>;

 public:
  explicit LoadTaggedFieldByFieldIndex(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::CanAllocate() |
                                              OpProperties::CanRead() |
                                              OpProperties::DeferredCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  static constexpr int kObjectIndex = 0;
  static constexpr int kIndexIndex = 1;
  Input& object_input() { return input(kObjectIndex); }
  Input& index_input() { return input(kIndexIndex); }

  void MarkTaggedInputsAsDecompressing() {
    // Only need to decompress the object, the index should be a Smi.
    object_input().node()->SetTaggedResultNeedsDecompress();
  }
  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class LoadFixedArrayElement
    : public FixedInputValueNodeT<2, LoadFixedArrayElement> {
  using Base = FixedInputValueNodeT<2, LoadFixedArrayElement>;

 public:
  explicit LoadFixedArrayElement(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::CanRead();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kInt32};

  static constexpr int kElementsIndex = 0;
  static constexpr int kIndexIndex = 1;
  Input& elements_input() { return input(kElementsIndex); }
  Input& index_input() { return input(kIndexIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;
};

class EnsureWritableFastElements
    : public FixedInputValueNodeT<2, EnsureWritableFastElements> {
  using Base = FixedInputValueNodeT<2, EnsureWritableFastElements>;

 public:
  explicit EnsureWritableFastElements(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanAllocate() | OpProperties::DeferredCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  static constexpr int kElementsIndex = 0;
  static constexpr int kObjectIndex = 1;
  Input& elements_input() { return input(kElementsIndex); }
  Input& object_input() { return input(kObjectIndex); }

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class MaybeGrowAndEnsureWritableFastElements
    : public FixedInputValueNodeT<4, MaybeGrowAndEnsureWritableFastElements> {
  using Base = FixedInputValueNodeT<4, MaybeGrowAndEnsureWritableFastElements>;

 public:
  explicit MaybeGrowAndEnsureWritableFastElements(uint64_t bitfield,
                                                  ElementsKind elements_kind)
      : Base(bitfield), elements_kind_(elements_kind) {}

  static constexpr OpProperties kProperties = OpProperties::CanAllocate() |
                                              OpProperties::DeferredCall() |
                                              OpProperties::EagerDeopt();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged,
      ValueRepresentation::kInt32, ValueRepresentation::kInt32};

  static constexpr int kElementsIndex = 0;
  static constexpr int kObjectIndex = 1;
  static constexpr int kIndexIndex = 2;
  static constexpr int kElementsLengthIndex = 3;
  Input& elements_input() { return input(kElementsIndex); }
  Input& object_input() { return input(kObjectIndex); }
  Input& index_input() { return input(kIndexIndex); }
  Input& elements_length_input() { return input(kElementsLengthIndex); }

  ElementsKind elements_kind() const { return elements_kind_; }

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  const ElementsKind elements_kind_;
};

class StoreFixedArrayElementWithWriteBarrier
    : public FixedInputNodeT<3, StoreFixedArrayElementWithWriteBarrier> {
  using Base = FixedInputNodeT<3, StoreFixedArrayElementWithWriteBarrier>;

 public:
  explicit StoreFixedArrayElementWithWriteBarrier(uint64_t bitfield)
      : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanWrite() | OpProperties::DeferredCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kInt32,
      ValueRepresentation::kTagged};

  static constexpr int kElementsIndex = 0;
  static constexpr int kIndexIndex = 1;
  static constexpr int kValueIndex = 2;
  Input& elements_input() { return input(kElementsIndex); }
  Input& index_input() { return input(kIndexIndex); }
  Input& value_input() { return input(kValueIndex); }

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

// StoreFixedArrayElementNoWriteBarrier never does a Deferred Call. However,
// PhiRepresentationSelector can cause some StoreFixedArrayElementNoWriteBarrier
// to become StoreFixedArrayElementWithWriteBarrier, which can do Deferred
// Calls, and thus need the register snapshot. We thus set the DeferredCall
// property in StoreFixedArrayElementNoWriteBarrier so that it's allocated with
// enough space for the register snapshot.
class StoreFixedArrayElementNoWriteBarrier
    : public FixedInputNodeT<3, StoreFixedArrayElementNoWriteBarrier> {
  using Base = FixedInputNodeT<3, StoreFixedArrayElementNoWriteBarrier>;

 public:
  explicit StoreFixedArrayElementNoWriteBarrier(uint64_t bitfield)
      : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanWrite() | OpProperties::DeferredCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kInt32,
      ValueRepresentation::kTagged};

  static constexpr int kElementsIndex = 0;
  static constexpr int kIndexIndex = 1;
  static constexpr int kValueIndex = 2;
  Input& elements_input() { return input(kElementsIndex); }
  Input& index_input() { return input(kIndexIndex); }
  Input& value_input() { return input(kValueIndex); }

  int MaxCallStackArgs() const {
    // StoreFixedArrayElementNoWriteBarrier never really does any call.
    return 0;
  }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class LoadFixedDoubleArrayElement
    : public FixedInputValueNodeT<2, LoadFixedDoubleArrayElement> {
  using Base = FixedInputValueNodeT<2, LoadFixedDoubleArrayElement>;

 public:
  explicit LoadFixedDoubleArrayElement(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanRead() | OpProperties::Float64();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kInt32};

  static constexpr int kElementsIndex = 0;
  static constexpr int kIndexIndex = 1;
  Input& elements_input() { return input(kElementsIndex); }
  Input& index_input() { return input(kIndexIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class LoadHoleyFixedDoubleArrayElement
    : public FixedInputValueNodeT<2, LoadHoleyFixedDoubleArrayElement> {
  using Base = FixedInputValueNodeT<2, LoadHoleyFixedDoubleArrayElement>;

 public:
  explicit LoadHoleyFixedDoubleArrayElement(uint64_t bitfield)
      : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanRead() | OpProperties::HoleyFloat64();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kInt32};

  static constexpr int kElementsIndex = 0;
  static constexpr int kIndexIndex = 1;
  Input& elements_input() { return input(kElementsIndex); }
  Input& index_input() { return input(kIndexIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class StoreFixedDoubleArrayElement
    : public FixedInputNodeT<3, StoreFixedDoubleArrayElement> {
  using Base = FixedInputNodeT<3, StoreFixedDoubleArrayElement>;

 public:
  explicit StoreFixedDoubleArrayElement(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::CanWrite();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kInt32,
      ValueRepresentation::kHoleyFloat64};

  static constexpr int kElementsIndex = 0;
  static constexpr int kIndexIndex = 1;
  static constexpr int kValueIndex = 2;
  Input& elements_input() { return input(kElementsIndex); }
  Input& index_input() { return input(kIndexIndex); }
  Input& value_input() { return input(kValueIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class LoadSignedIntDataViewElement
    : public FixedInputValueNodeT<3, LoadSignedIntDataViewElement> {
  using Base = FixedInputValueNodeT<3, LoadSignedIntDataViewElement>;

 public:
  explicit LoadSignedIntDataViewElement(uint64_t bitfield,
                                        ExternalArrayType type)
      : Base(bitfield), type_(type) {
    DCHECK(type == ExternalArrayType::kExternalInt8Array ||
           type == ExternalArrayType::kExternalInt16Array ||
           type == ExternalArrayType::kExternalInt32Array);
  }

  static constexpr OpProperties kProperties =
      OpProperties::CanRead() | OpProperties::Int32();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kInt32,
      ValueRepresentation::kTagged};

  static constexpr int kObjectIndex = 0;
  static constexpr int kIndexIndex = 1;
  static constexpr int kIsLittleEndianIndex = 2;
  Input& object_input() { return input(kObjectIndex); }
  Input& index_input() { return input(kIndexIndex); }
  Input& is_little_endian_input() { return input(kIsLittleEndianIndex); }

  bool is_little_endian_constant() {
    return IsConstantNode(is_little_endian_input().node()->opcode());
  }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  ExternalArrayType type_;
};

class LoadDoubleDataViewElement
    : public FixedInputValueNodeT<3, LoadDoubleDataViewElement> {
  using Base = FixedInputValueNodeT<3, LoadDoubleDataViewElement>;

 public:
  explicit LoadDoubleDataViewElement(uint64_t bitfield, ExternalArrayType type)
      : Base(bitfield) {
    DCHECK_EQ(type, ExternalArrayType::kExternalFloat64Array);
  }

  static constexpr OpProperties kProperties =
      OpProperties::CanRead() | OpProperties::Float64();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kInt32,
      ValueRepresentation::kTagged};

  static constexpr int kObjectIndex = 0;
  static constexpr int kIndexIndex = 1;
  static constexpr int kIsLittleEndianIndex = 2;
  Input& object_input() { return input(kObjectIndex); }
  Input& index_input() { return input(kIndexIndex); }
  Input& is_little_endian_input() { return input(kIsLittleEndianIndex); }

  bool is_little_endian_constant() {
    return IsConstantNode(is_little_endian_input().node()->opcode());
  }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

#define LOAD_TYPED_ARRAY(name, properties, ...)                        \
  class name : public FixedInputValueNodeT<2, name> {                  \
    using Base = FixedInputValueNodeT<2, name>;                        \
                                                                       \
   public:                                                             \
    explicit name(uint64_t bitfield, ElementsKind elements_kind)       \
        : Base(bitfield), elements_kind_(elements_kind) {              \
      DCHECK(elements_kind ==                                          \
             v8::internal::compiler::turboshaft::any_of(__VA_ARGS__)); \
    }                                                                  \
                                                                       \
    static constexpr OpProperties kProperties =                        \
        OpProperties::CanRead() | properties;                          \
    static constexpr typename Base::InputTypes kInputTypes{            \
        ValueRepresentation::kTagged, ValueRepresentation::kUint32};   \
                                                                       \
    static constexpr int kObjectIndex = 0;                             \
    static constexpr int kIndexIndex = 1;                              \
    Input& object_input() { return input(kObjectIndex); }              \
    Input& index_input() { return input(kIndexIndex); }                \
                                                                       \
    void SetValueLocationConstraints();                                \
    void GenerateCode(MaglevAssembler*, const ProcessingState&);       \
    void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}     \
                                                                       \
   private:                                                            \
    ElementsKind elements_kind_;                                       \
  };

// Nodes that can deopt are larger, since they contain the DeoptInfo. Thus, to
// have better performance, we split the LoadxxxTypedArrayElement nodes in two:
// those who can deopt and those who can't. Deoptimization in a
// LoadxxxTypedArrayElement node is always because of a detached array buffer.
// The NoDeopt versions of the nodes rely on the ArrayBufferDetachingProtector,
// while the deopting versions have a runtime check that triggers a deopt if the
// buffer is detached.
LOAD_TYPED_ARRAY(LoadSignedIntTypedArrayElement,
                 OpProperties::EagerDeopt() | OpProperties::Int32(),
                 INT8_ELEMENTS, INT16_ELEMENTS, INT32_ELEMENTS)
LOAD_TYPED_ARRAY(LoadSignedIntTypedArrayElementNoDeopt, OpProperties::Int32(),
                 INT8_ELEMENTS, INT16_ELEMENTS, INT32_ELEMENTS)

LOAD_TYPED_ARRAY(LoadUnsignedIntTypedArrayElement,
                 OpProperties::EagerDeopt() | OpProperties::Uint32(),
                 UINT8_ELEMENTS, UINT8_CLAMPED_ELEMENTS, UINT16_ELEMENTS,
                 UINT16_ELEMENTS, UINT32_ELEMENTS)
LOAD_TYPED_ARRAY(LoadUnsignedIntTypedArrayElementNoDeopt,
                 OpProperties::Uint32(), UINT8_ELEMENTS, UINT8_CLAMPED_ELEMENTS,
                 UINT16_ELEMENTS, UINT16_ELEMENTS, UINT32_ELEMENTS)

LOAD_TYPED_ARRAY(LoadDoubleTypedArrayElement,
                 OpProperties::EagerDeopt() | OpProperties::Float64(),
                 FLOAT32_ELEMENTS, FLOAT64_ELEMENTS)
LOAD_TYPED_ARRAY(LoadDoubleTypedArrayElementNoDeopt, OpProperties::Float64(),
                 FLOAT32_ELEMENTS, FLOAT64_ELEMENTS)

#undef LOAD_TYPED_ARRAY

#define STORE_TYPED_ARRAY(name, properties, type, ...)                     \
  class name : public FixedInputNodeT<3, name> {                           \
    using Base = FixedInputNodeT<3, name>;                                 \
                                                                           \
   public:                                                                 \
    explicit name(uint64_t bitfield, ElementsKind elements_kind)           \
        : Base(bitfield), elements_kind_(elements_kind) {                  \
      DCHECK(elements_kind ==                                              \
             v8::internal::compiler::turboshaft::any_of(__VA_ARGS__));     \
    }                                                                      \
                                                                           \
    static constexpr OpProperties kProperties = properties;                \
    static constexpr typename Base::InputTypes kInputTypes{                \
        ValueRepresentation::kTagged, ValueRepresentation::kUint32, type}; \
                                                                           \
    static constexpr int kObjectIndex = 0;                                 \
    static constexpr int kIndexIndex = 1;                                  \
    static constexpr int kValueIndex = 2;                                  \
    Input& object_input() { return input(kObjectIndex); }                  \
    Input& index_input() { return input(kIndexIndex); }                    \
    Input& value_input() { return input(kValueIndex); }                    \
                                                                           \
    void SetValueLocationConstraints();                                    \
    void GenerateCode(MaglevAssembler*, const ProcessingState&);           \
    void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}         \
                                                                           \
   private:                                                                \
    ElementsKind elements_kind_;                                           \
  };

// Nodes that can deopt are larger, since they contain the DeoptInfo. Thus, to
// have better performance, we split the StorexxxTypedArrayElement nodes in two:
// those who can deopt and those who can't. Deoptimization in a
// StorexxxTypedArrayElement node is always because of a detached array buffer.
// The NoDeopt versions of the nodes rely on the ArrayBufferDetachingProtector,
// while the deopting versions have a runtime check that triggers a deopt if the
// buffer is detached.
STORE_TYPED_ARRAY(StoreIntTypedArrayElement,
                  OpProperties::EagerDeopt() | OpProperties::CanWrite(),
                  ValueRepresentation::kInt32, INT8_ELEMENTS, INT16_ELEMENTS,
                  INT32_ELEMENTS, UINT8_ELEMENTS, UINT8_CLAMPED_ELEMENTS,
                  UINT16_ELEMENTS, UINT16_ELEMENTS, UINT32_ELEMENTS)
STORE_TYPED_ARRAY(StoreIntTypedArrayElementNoDeopt, OpProperties::CanWrite(),
                  ValueRepresentation::kInt32, INT8_ELEMENTS, INT16_ELEMENTS,
                  INT32_ELEMENTS, UINT8_ELEMENTS, UINT8_CLAMPED_ELEMENTS,
                  UINT16_ELEMENTS, UINT16_ELEMENTS, UINT32_ELEMENTS)

STORE_TYPED_ARRAY(StoreDoubleTypedArrayElement,
                  OpProperties::EagerDeopt() | OpProperties::CanWrite(),
                  ValueRepresentation::kHoleyFloat64, FLOAT32_ELEMENTS,
                  FLOAT64_ELEMENTS)
STORE_TYPED_ARRAY(StoreDoubleTypedArrayElementNoDeopt, OpProperties::CanWrite(),
                  ValueRepresentation::kHoleyFloat64, FLOAT32_ELEMENTS,
                  FLOAT64_ELEMENTS)
#undef STORE_TYPED_ARRAY

class StoreSignedIntDataViewElement
    : public FixedInputNodeT<4, StoreSignedIntDataViewElement> {
  using Base = FixedInputNodeT<4, StoreSignedIntDataViewElement>;

 public:
  explicit StoreSignedIntDataViewElement(uint64_t bitfield,
                                         ExternalArrayType type)
      : Base(bitfield), type_(type) {
    DCHECK(type == ExternalArrayType::kExternalInt8Array ||
           type == ExternalArrayType::kExternalInt16Array ||
           type == ExternalArrayType::kExternalInt32Array);
  }

  static constexpr OpProperties kProperties = OpProperties::CanWrite();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kInt32,
      ValueRepresentation::kInt32, ValueRepresentation::kTagged};

  static constexpr int kObjectIndex = 0;
  static constexpr int kIndexIndex = 1;
  static constexpr int kValueIndex = 2;
  static constexpr int kIsLittleEndianIndex = 3;
  Input& object_input() { return input(kObjectIndex); }
  Input& index_input() { return input(kIndexIndex); }
  Input& value_input() { return input(kValueIndex); }
  Input& is_little_endian_input() { return input(kIsLittleEndianIndex); }

  bool is_little_endian_constant() {
    return IsConstantNode(is_little_endian_input().node()->opcode());
  }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  ExternalArrayType type_;
};

class StoreDoubleDataViewElement
    : public FixedInputNodeT<4, StoreDoubleDataViewElement> {
  using Base = FixedInputNodeT<4, StoreDoubleDataViewElement>;

 public:
  explicit StoreDoubleDataViewElement(uint64_t bitfield, ExternalArrayType type)
      : Base(bitfield) {
    DCHECK_EQ(type, ExternalArrayType::kExternalFloat64Array);
  }

  static constexpr OpProperties kProperties = OpProperties::CanWrite();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kInt32,
      ValueRepresentation::kHoleyFloat64, ValueRepresentation::kTagged};

  static constexpr int kObjectIndex = 0;
  static constexpr int kIndexIndex = 1;
  static constexpr int kValueIndex = 2;
  static constexpr int kIsLittleEndianIndex = 3;
  Input& object_input() { return input(kObjectIndex); }
  Input& index_input() { return input(kIndexIndex); }
  Input& value_input() { return input(kValueIndex); }
  Input& is_little_endian_input() { return input(kIsLittleEndianIndex); }

  bool is_little_endian_constant() {
    return IsConstantNode(is_little_endian_input().node()->opcode());
  }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class StoreDoubleField : public FixedInputNodeT<2, StoreDoubleField> {
  using Base = FixedInputNodeT<2, StoreDoubleField>;

 public:
  explicit StoreDoubleField(uint64_t bitfield, int offset)
      : Base(bitfield), offset_(offset) {}

  static constexpr OpProperties kProperties = OpProperties::CanWrite();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kFloat64};

  int offset() const { return offset_; }

  static constexpr int kObjectIndex = 0;
  static constexpr int kValueIndex = 1;
  Input& object_input() { return input(kObjectIndex); }
  Input& value_input() { return input(kValueIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const int offset_;
};

class StoreFloat64 : public FixedInputNodeT<2, StoreFloat64> {
  using Base = FixedInputNodeT<2, StoreFloat64>;

 public:
  explicit StoreFloat64(uint64_t bitfield, int offset)
      : Base(bitfield), offset_(offset) {}

  static constexpr OpProperties kProperties = OpProperties::CanWrite();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kFloat64};

  int offset() const { return offset_; }

  static constexpr int kObjectIndex = 0;
  static constexpr int kValueIndex = 1;
  Input& object_input() { return input(kObjectIndex); }
  Input& value_input() { return input(kValueIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const int offset_;
};

class StoreTaggedFieldNoWriteBarrier
    : public FixedInputNodeT<2, StoreTaggedFieldNoWriteBarrier> {
  using Base = FixedInputNodeT<2, StoreTaggedFieldNoWriteBarrier>;

 public:
  explicit StoreTaggedFieldNoWriteBarrier(uint64_t bitfield, int offset)
      : Base(bitfield), offset_(offset) {}

  // StoreTaggedFieldNoWriteBarrier never does a Deferred Call. However,
  // PhiRepresentationSelector can cause some StoreTaggedFieldNoWriteBarrier to
  // become StoreTaggedFieldWithWriteBarrier, which can do Deferred Calls, and
  // thus need the register snapshot. We thus set the DeferredCall property in
  // StoreTaggedFieldNoWriteBarrier so that it's allocated with enough space for
  // the register snapshot.
  static constexpr OpProperties kProperties =
      OpProperties::CanWrite() | OpProperties::DeferredCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  int offset() const { return offset_; }

  static constexpr int kObjectIndex = 0;
  static constexpr int kValueIndex = 1;
  Input& object_input() { return input(kObjectIndex); }
  Input& value_input() { return input(kValueIndex); }

  void MarkTaggedInputsAsDecompressing() {
    object_input().node()->SetTaggedResultNeedsDecompress();
    // Don't need to decompress value to store it.
  }
  int MaxCallStackArgs() const {
    // StoreTaggedFieldNoWriteBarrier never really does any call.
    return 0;
  }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const int offset_;
};

class StoreMap : public FixedInputNodeT<1, StoreMap> {
  using Base = FixedInputNodeT<1, StoreMap>;

 public:
  explicit StoreMap(uint64_t bitfield, compiler::MapRef map)
      : Base(bitfield), map_(map) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanWrite() | OpProperties::DeferredCall();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  static constexpr int kObjectIndex = 0;
  Input& object_input() { return input(kObjectIndex); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const compiler::MapRef map_;
};

class StoreTaggedFieldWithWriteBarrier
    : public FixedInputNodeT<2, StoreTaggedFieldWithWriteBarrier> {
  using Base = FixedInputNodeT<2, StoreTaggedFieldWithWriteBarrier>;

 public:
  explicit StoreTaggedFieldWithWriteBarrier(uint64_t bitfield, int offset)
      : Base(bitfield), offset_(offset) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanWrite() | OpProperties::DeferredCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  int offset() const { return offset_; }

  static constexpr int kObjectIndex = 0;
  static constexpr int kValueIndex = 1;
  Input& object_input() { return input(kObjectIndex); }
  Input& value_input() { return input(kValueIndex); }

  void MarkTaggedInputsAsDecompressing() {
    object_input().node()->SetTaggedResultNeedsDecompress();
    // Don't need to decompress value to store it.
  }
  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const int offset_;
};

class LoadGlobal : public FixedInputValueNodeT<1, LoadGlobal> {
  using Base = FixedInputValueNodeT<1, LoadGlobal>;

 public:
  explicit LoadGlobal(uint64_t bitfield, compiler::NameRef name,
                      const compiler::FeedbackSource& feedback,
                      TypeofMode typeof_mode)
      : Base(bitfield),
        name_(name),
        feedback_(feedback),
        typeof_mode_(typeof_mode) {}

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::JSCall();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  compiler::NameRef name() const { return name_; }
  compiler::FeedbackSource feedback() const { return feedback_; }
  TypeofMode typeof_mode() const { return typeof_mode_; }

  Input& context() { return input(0); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const compiler::NameRef name_;
  const compiler::FeedbackSource feedback_;
  const TypeofMode typeof_mode_;
};

class StoreGlobal : public FixedInputValueNodeT<2, StoreGlobal> {
  using Base = FixedInputValueNodeT<2, StoreGlobal>;

 public:
  explicit StoreGlobal(uint64_t bitfield, compiler::NameRef name,
                       const compiler::FeedbackSource& feedback)
      : Base(bitfield), name_(name), feedback_(feedback) {}

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::JSCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  compiler::NameRef name() const { return name_; }
  compiler::FeedbackSource feedback() const { return feedback_; }

  Input& context() { return input(0); }
  Input& value() { return input(1); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const compiler::NameRef name_;
  const compiler::FeedbackSource feedback_;
};

class UpdateJSArrayLength : public FixedInputNodeT<3, UpdateJSArrayLength> {
  using Base = FixedInputNodeT<3, UpdateJSArrayLength>;

 public:
  explicit UpdateJSArrayLength(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::CanWrite();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kInt32,
      ValueRepresentation::kInt32};

  static constexpr int kObjectIndex = 0;
  static constexpr int kIndexIndex = 1;
  static constexpr int kLengthIndex = 2;
  Input& object_input() { return input(kObjectIndex); }
  Input& index_input() { return input(kIndexIndex); }
  Input& length_input() { return input(kLengthIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class LoadNamedGeneric : public FixedInputValueNodeT<2, LoadNamedGeneric> {
  using Base = FixedInputValueNodeT<2, LoadNamedGeneric>;

 public:
  explicit LoadNamedGeneric(uint64_t bitfield, compiler::NameRef name,
                            const compiler::FeedbackSource& feedback)
      : Base(bitfield), name_(name), feedback_(feedback) {}

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::JSCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  compiler::NameRef name() const { return name_; }
  compiler::FeedbackSource feedback() const { return feedback_; }

  static constexpr int kContextIndex = 0;
  static constexpr int kObjectIndex = 1;
  Input& context() { return input(kContextIndex); }
  Input& object_input() { return input(kObjectIndex); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const compiler::NameRef name_;
  const compiler::FeedbackSource feedback_;
};

class LoadNamedFromSuperGeneric
    : public FixedInputValueNodeT<3, LoadNamedFromSuperGeneric> {
  using Base = FixedInputValueNodeT<3, LoadNamedFromSuperGeneric>;

 public:
  explicit LoadNamedFromSuperGeneric(uint64_t bitfield, compiler::NameRef name,
                                     const compiler::FeedbackSource& feedback)
      : Base(bitfield), name_(name), feedback_(feedback) {}

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::JSCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged,
      ValueRepresentation::kTagged};

  compiler::NameRef name() const { return name_; }
  compiler::FeedbackSource feedback() const { return feedback_; }

  static constexpr int kContextIndex = 0;
  static constexpr int kReceiverIndex = 1;
  static constexpr int kLookupStartObjectIndex = 2;
  Input& context() { return input(kContextIndex); }
  Input& receiver() { return input(kReceiverIndex); }
  Input& lookup_start_object() { return input(kLookupStartObjectIndex); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const compiler::NameRef name_;
  const compiler::FeedbackSource feedback_;
};

class SetNamedGeneric : public FixedInputValueNodeT<3, SetNamedGeneric> {
  using Base = FixedInputValueNodeT<3, SetNamedGeneric>;

 public:
  explicit SetNamedGeneric(uint64_t bitfield, compiler::NameRef name,
                           const compiler::FeedbackSource& feedback)
      : Base(bitfield), name_(name), feedback_(feedback) {}

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::JSCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged,
      ValueRepresentation::kTagged};

  compiler::NameRef name() const { return name_; }
  compiler::FeedbackSource feedback() const { return feedback_; }

  static constexpr int kContextIndex = 0;
  static constexpr int kObjectIndex = 1;
  static constexpr int kValueIndex = 2;
  Input& context() { return input(kContextIndex); }
  Input& object_input() { return input(kObjectIndex); }
  Input& value_input() { return input(kValueIndex); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const compiler::NameRef name_;
  const compiler::FeedbackSource feedback_;
};

class LoadEnumCacheLength
    : public FixedInputValueNodeT<1, LoadEnumCacheLength> {
  using Base = FixedInputValueNodeT<1, LoadEnumCacheLength>;

 public:
  explicit LoadEnumCacheLength(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanRead() | OpProperties::Int32();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  static constexpr int kMapInput = 0;
  Input& map_input() { return input(kMapInput); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class StringAt : public FixedInputValueNodeT<2, StringAt> {
  using Base = FixedInputValueNodeT<2, StringAt>;

 public:
  explicit StringAt(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::CanRead() |
                                              OpProperties::CanAllocate() |
                                              OpProperties::DeferredCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kInt32};

  static constexpr int kStringIndex = 0;
  static constexpr int kIndexIndex = 1;
  Input& string_input() { return input(kStringIndex); }
  Input& index_input() { return input(kIndexIndex); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class StringLength : public FixedInputValueNodeT<1, StringLength> {
  using Base = FixedInputValueNodeT<1, StringLength>;

 public:
  explicit StringLength(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanRead() | OpProperties::Int32();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  static constexpr int kObjectIndex = 0;
  Input& object_input() { return input(kObjectIndex); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class StringConcat : public FixedInputValueNodeT<2, StringConcat> {
  using Base = FixedInputValueNodeT<2, StringConcat>;

 public:
  explicit StringConcat(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::Call() |
                                              OpProperties::LazyDeopt() |
                                              OpProperties::CanThrow();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  Input& lhs() { return Node::input(0); }
  Input& rhs() { return Node::input(1); }

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class DefineNamedOwnGeneric
    : public FixedInputValueNodeT<3, DefineNamedOwnGeneric> {
  using Base = FixedInputValueNodeT<3, DefineNamedOwnGeneric>;

 public:
  explicit DefineNamedOwnGeneric(uint64_t bitfield, compiler::NameRef name,
                                 const compiler::FeedbackSource& feedback)
      : Base(bitfield), name_(name), feedback_(feedback) {}

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::JSCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged,
      ValueRepresentation::kTagged};

  compiler::NameRef name() const { return name_; }
  compiler::FeedbackSource feedback() const { return feedback_; }

  static constexpr int kContextIndex = 0;
  static constexpr int kObjectIndex = 1;
  static constexpr int kValueIndex = 2;
  Input& context() { return input(kContextIndex); }
  Input& object_input() { return input(kObjectIndex); }
  Input& value_input() { return input(kValueIndex); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const compiler::NameRef name_;
  const compiler::FeedbackSource feedback_;
};

class StoreInArrayLiteralGeneric
    : public FixedInputValueNodeT<4, StoreInArrayLiteralGeneric> {
  using Base = FixedInputValueNodeT<4, StoreInArrayLiteralGeneric>;

 public:
  explicit StoreInArrayLiteralGeneric(uint64_t bitfield,
                                      const compiler::FeedbackSource& feedback)
      : Base(bitfield), feedback_(feedback) {}

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::JSCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged,
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  compiler::FeedbackSource feedback() const { return feedback_; }

  static constexpr int kContextIndex = 0;
  static constexpr int kObjectIndex = 1;
  static constexpr int kNameIndex = 2;
  static constexpr int kValueIndex = 3;
  Input& context() { return input(kContextIndex); }
  Input& object_input() { return input(kObjectIndex); }
  Input& name_input() { return input(kNameIndex); }
  Input& value_input() { return input(kValueIndex); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  const compiler::FeedbackSource feedback_;
};

class GetKeyedGeneric : public FixedInputValueNodeT<3, GetKeyedGeneric> {
  using Base = FixedInputValueNodeT<3, GetKeyedGeneric>;

 public:
  explicit GetKeyedGeneric(uint64_t bitfield,
                           const compiler::FeedbackSource& feedback)
      : Base(bitfield), feedback_(feedback) {}

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::JSCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged,
      ValueRepresentation::kTagged};

  compiler::FeedbackSource feedback() const { return feedback_; }

  static constexpr int kContextIndex = 0;
  static constexpr int kObjectIndex = 1;
  static constexpr int kKeyIndex = 2;
  Input& context() { return input(kContextIndex); }
  Input& object_input() { return input(kObjectIndex); }
  Input& key_input() { return input(kKeyIndex); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  const compiler::FeedbackSource feedback_;
};

class SetKeyedGeneric : public FixedInputValueNodeT<4, SetKeyedGeneric> {
  using Base = FixedInputValueNodeT<4, SetKeyedGeneric>;

 public:
  explicit SetKeyedGeneric(uint64_t bitfield,
                           const compiler::FeedbackSource& feedback)
      : Base(bitfield), feedback_(feedback) {}

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::JSCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged,
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  compiler::FeedbackSource feedback() const { return feedback_; }

  static constexpr int kContextIndex = 0;
  static constexpr int kObjectIndex = 1;
  static constexpr int kKeyIndex = 2;
  static constexpr int kValueIndex = 3;
  Input& context() { return input(kContextIndex); }
  Input& object_input() { return input(kObjectIndex); }
  Input& key_input() { return input(kKeyIndex); }
  Input& value_input() { return input(kValueIndex); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  const compiler::FeedbackSource feedback_;
};

class DefineKeyedOwnGeneric
    : public FixedInputValueNodeT<5, DefineKeyedOwnGeneric> {
  using Base = FixedInputValueNodeT<5, DefineKeyedOwnGeneric>;

 public:
  explicit DefineKeyedOwnGeneric(uint64_t bitfield,
                                 const compiler::FeedbackSource& feedback)
      : Base(bitfield), feedback_(feedback) {}

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::JSCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged,
      ValueRepresentation::kTagged, ValueRepresentation::kTagged,
      ValueRepresentation::kTagged};

  compiler::FeedbackSource feedback() const { return feedback_; }

  static constexpr int kContextIndex = 0;
  static constexpr int kObjectIndex = 1;
  static constexpr int kKeyIndex = 2;
  static constexpr int kValueIndex = 3;
  static constexpr int kFlagsIndex = 4;
  Input& context() { return input(kContextIndex); }
  Input& object_input() { return input(kObjectIndex); }
  Input& key_input() { return input(kKeyIndex); }
  Input& value_input() { return input(kValueIndex); }
  Input& flags_input() { return input(kFlagsIndex); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  const compiler::FeedbackSource feedback_;
};

class GapMove : public FixedInputNodeT<0, GapMove> {
  using Base = FixedInputNodeT<0, GapMove>;

 public:
  GapMove(uint64_t bitfield, compiler::AllocatedOperand source,
          compiler::AllocatedOperand target)
      : Base(bitfield), source_(source), target_(target) {}

  compiler::AllocatedOperand source() const { return source_; }
  compiler::AllocatedOperand target() const { return target_; }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  compiler::AllocatedOperand source_;
  compiler::AllocatedOperand target_;
};

class ConstantGapMove : public FixedInputNodeT<0, ConstantGapMove> {
  using Base = FixedInputNodeT<0, ConstantGapMove>;

 public:
  ConstantGapMove(uint64_t bitfield, ValueNode* node,
                  compiler::AllocatedOperand target)
      : Base(bitfield), node_(node), target_(target) {}

  compiler::AllocatedOperand target() const { return target_; }
  ValueNode* node() const { return node_; }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  ValueNode* node_;
  compiler::InstructionOperand source_;
  compiler::AllocatedOperand target_;
};

class MergePointInterpreterFrameState;

// ValueRepresentation doesn't distinguish between Int32 and TruncatedInt32:
// both are Int32. For Phi untagging however, it's interesting to have a
// difference between the 2, as a TruncatedInt32 would allow untagging to
// Float64, whereas a Int32 use wouldn't (because it would require a deopting
// Float64->Int32 conversion, whereas the truncating version of this conversion
// cannot deopt). We thus use a UseRepresentation to record use hints for Phis.
enum class UseRepresentation : uint8_t {
  kTagged,
  kInt32,
  kTruncatedInt32,
  kUint32,
  kFloat64,
  kHoleyFloat64,
};

inline std::ostream& operator<<(std::ostream& os,
                                const UseRepresentation& repr) {
  switch (repr) {
    case UseRepresentation::kTagged:
      return os << "Tagged";
    case UseRepresentation::kInt32:
      return os << "Int32";
    case UseRepresentation::kTruncatedInt32:
      return os << "TruncatedInt32";
    case UseRepresentation::kUint32:
      return os << "Uint32";
    case UseRepresentation::kFloat64:
      return os << "Float64";
    case UseRepresentation::kHoleyFloat64:
      return os << "HoleyFloat64";
  }
}

typedef base::EnumSet<ValueRepresentation, int8_t> ValueRepresentationSet;
typedef base::EnumSet<UseRepresentation, int8_t> UseRepresentationSet;

// TODO(verwaest): It may make more sense to buffer phis in merged_states until
// we set up the interpreter frame state for code generation. At that point we
// can generate correctly-sized phis.
class Phi : public ValueNodeT<Phi> {
  using Base = ValueNodeT<Phi>;

 public:
  using List = base::ThreadedList<Phi>;

  // TODO(jgruber): More intuitive constructors, if possible.
  Phi(uint64_t bitfield, MergePointInterpreterFrameState* merge_state,
      interpreter::Register owner)
      : Base(bitfield),
        owner_(owner),
        merge_state_(merge_state),
        type_(NodeType::kUnknown),
        post_loop_type_(NodeType::kUnknown) {
    DCHECK_NOT_NULL(merge_state);
  }

  interpreter::Register owner() const { return owner_; }
  const MergePointInterpreterFrameState* merge_state() const {
    return merge_state_;
  }

  using Node::reduce_input_count;
  using Node::set_input;

  bool is_exception_phi() const { return input_count() == 0; }
  bool is_loop_phi() const;

  bool is_backedge_offset(int i) const {
    return is_loop_phi() && i == input_count() - 1;
  }

  void VerifyInputs(MaglevGraphLabeller* graph_labeller) const;
  void MarkTaggedInputsAsDecompressing() {
    // Do not mark inputs as decompressing here, since we don't yet know whether
    // this Phi needs decompression. Instead, let
    // Node::SetTaggedResultNeedsDecompress pass through phis.
  }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

  BasicBlock* predecessor_at(int i);

  void RecordUseReprHint(UseRepresentation repr, int current_offset) {
    RecordUseReprHint(UseRepresentationSet{repr}, current_offset);
  }

  void RecordUseReprHint(UseRepresentationSet repr_mask, int current_offset);

  UseRepresentationSet get_uses_repr_hints() { return uses_repr_hint_; }
  UseRepresentationSet get_same_loop_uses_repr_hints() {
    return same_loop_uses_repr_hint_;
  }

  void merge_post_loop_type(NodeType type) {
    DCHECK(!has_key_);
    post_loop_type_ = IntersectType(post_loop_type_, type);
  }
  void set_post_loop_type(NodeType type) {
    DCHECK(!has_key_);
    post_loop_type_ = type;
  }
  void promote_post_loop_type() {
    DCHECK(!has_key_);
    type_ = post_loop_type_;
  }

  void merge_type(NodeType type) {
    DCHECK(!has_key_);
    type_ = IntersectType(type_, type);
  }
  void set_type(NodeType type) {
    DCHECK(!has_key_);
    type_ = type;
  }
  NodeType type() const {
    DCHECK(!has_key_);
    return type_;
  }

  using Key = compiler::turboshaft::SnapshotTable<ValueNode*>::Key;
  bool has_key() const { return has_key_; }
  Key key() const {
    DCHECK(has_key_);
    return key_;
  }
  void set_key(Key key) {
    has_key_ = true;
    key_ = key;
  }

 private:
  Phi** next() { return &next_; }

  const interpreter::Register owner_;
  bool has_key_ = false;  // True if the {key_} field has been initialized.
  UseRepresentationSet uses_repr_hint_ = {};
  UseRepresentationSet same_loop_uses_repr_hint_ = {};

  Phi* next_ = nullptr;
  MergePointInterpreterFrameState* const merge_state_;

  union {
    struct {
      // The type of this Phi based on its predecessors' types.
      NodeType type_;
      // {type_} for loop Phis should always be Unknown until their backedge has
      // been bound (because we don't know what will be the type of the
      // backedge). However, once the backedge is bound, we might be able to
      // refine it. {post_loop_type_} is thus used to keep track of loop Phi
      // types: for loop Phis, we update {post_loop_type_} when we merge
      // predecessors, but keep {type_} as Unknown. Once the backedge is bound,
      // we set {type_} as {post_loop_type_}.
      NodeType post_loop_type_;
    };
    // After graph building, {type_} and {post_loop_type_} are not used anymore,
    // so we reuse this memory to store the SnapshotTable Key for this Phi for
    // phi untagging.
    Key key_;
  };

  friend base::ThreadedListTraits<Phi>;
};

class Call : public ValueNodeT<Call> {
  using Base = ValueNodeT<Call>;

 public:
  enum class TargetType { kJSFunction, kAny };
  // We assume function and context as fixed inputs.
  static constexpr int kFunctionIndex = 0;
  static constexpr int kContextIndex = 1;
  static constexpr int kFixedInputCount = 2;

  // We need enough inputs to have these fixed inputs plus the maximum arguments
  // to a function call.
  static_assert(kMaxInputs >= kFixedInputCount + Code::kMaxArguments);

  // This ctor is used when for variable input counts.
  // Inputs must be initialized manually.
  Call(uint64_t bitfield, ConvertReceiverMode mode, TargetType target_type,
       ValueNode* function, ValueNode* context)
      : Base(bitfield), receiver_mode_(mode), target_type_(target_type) {
    set_input(kFunctionIndex, function);
    set_input(kContextIndex, context);
  }

  static constexpr OpProperties kProperties = OpProperties::JSCall();

  Input& function() { return input(kFunctionIndex); }
  const Input& function() const { return input(kFunctionIndex); }
  Input& context() { return input(kContextIndex); }
  const Input& context() const { return input(kContextIndex); }
  int num_args() const { return input_count() - kFixedInputCount; }
  Input& arg(int i) { return input(i + kFixedInputCount); }
  void set_arg(int i, ValueNode* node) {
    set_input(i + kFixedInputCount, node);
  }
  auto args_begin() { return std::make_reverse_iterator(&arg(-1)); }
  auto args_end() { return std::make_reverse_iterator(&arg(num_args() - 1)); }

  void VerifyInputs(MaglevGraphLabeller* graph_labeller) const;
  void MarkTaggedInputsAsDecompressing();
  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  ConvertReceiverMode receiver_mode_;
  TargetType target_type_;
};

class Construct : public ValueNodeT<Construct> {
  using Base = ValueNodeT<Construct>;

 public:
  // We assume function and context as fixed inputs.
  static constexpr int kFunctionIndex = 0;
  static constexpr int kNewTargetIndex = 1;
  static constexpr int kContextIndex = 2;
  static constexpr int kFixedInputCount = 3;

  // We need enough inputs to have these fixed inputs plus the maximum arguments
  // to a function call.
  static_assert(kMaxInputs >= kFixedInputCount + Code::kMaxArguments);

  // This ctor is used when for variable input counts.
  // Inputs must be initialized manually.
  Construct(uint64_t bitfield, const compiler::FeedbackSource& feedback,
            ValueNode* function, ValueNode* new_target, ValueNode* context)
      : Base(bitfield), feedback_(feedback) {
    set_input(kFunctionIndex, function);
    set_input(kNewTargetIndex, new_target);
    set_input(kContextIndex, context);
  }

  static constexpr OpProperties kProperties = OpProperties::JSCall();

  Input& function() { return input(kFunctionIndex); }
  const Input& function() const { return input(kFunctionIndex); }
  Input& new_target() { return input(kNewTargetIndex); }
  const Input& new_target() const { return input(kNewTargetIndex); }
  Input& context() { return input(kContextIndex); }
  const Input& context() const { return input(kContextIndex); }
  int num_args() const { return input_count() - kFixedInputCount; }
  Input& arg(int i) { return input(i + kFixedInputCount); }
  void set_arg(int i, ValueNode* node) {
    set_input(i + kFixedInputCount, node);
  }
  auto args_begin() { return std::make_reverse_iterator(&arg(-1)); }
  auto args_end() { return std::make_reverse_iterator(&arg(num_args() - 1)); }

  compiler::FeedbackSource feedback() const { return feedback_; }

  void VerifyInputs(MaglevGraphLabeller* graph_labeller) const;
  void MarkTaggedInputsAsDecompressing();
  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  const compiler::FeedbackSource feedback_;
};

class CallBuiltin : public ValueNodeT<CallBuiltin> {
  using Base = ValueNodeT<CallBuiltin>;

 public:
  enum FeedbackSlotType { kTaggedIndex, kSmi };

  // This ctor is used when for variable input counts.
  // Inputs must be initialized manually.
  CallBuiltin(uint64_t bitfield, Builtin builtin)
      : Base(bitfield), builtin_(builtin) {
    DCHECK(
        !Builtins::CallInterfaceDescriptorFor(builtin).HasContextParameter());
  }

  // This ctor is used when for variable input counts.
  // Inputs must be initialized manually.
  CallBuiltin(uint64_t bitfield, Builtin builtin, ValueNode* context)
      : Base(bitfield), builtin_(builtin) {
    DCHECK(Builtins::CallInterfaceDescriptorFor(builtin).HasContextParameter());
    // We use the last valid input for the context.
    set_input(input_count() - 1, context);
  }

  // This is an overestimation, since some builtins might not call JS code.
  static constexpr OpProperties kProperties = OpProperties::JSCall();

  bool has_feedback() const { return feedback_.has_value(); }
  compiler::FeedbackSource feedback() const {
    DCHECK(has_feedback());
    return feedback_.value();
  }
  FeedbackSlotType slot_type() const {
    DCHECK(has_feedback());
    return slot_type_;
  }
  void set_feedback(compiler::FeedbackSource const& feedback,
                    FeedbackSlotType slot_type) {
    feedback_ = feedback;
    slot_type_ = slot_type;
  }

  Builtin builtin() const { return builtin_; }

  int InputCountWithoutContext() const {
    auto descriptor = Builtins::CallInterfaceDescriptorFor(builtin_);
    bool has_context = descriptor.HasContextParameter();
    int extra_input_count = has_context ? 1 : 0;
    return input_count() - extra_input_count;
  }

  int InputsInRegisterCount() const {
    auto descriptor = Builtins::CallInterfaceDescriptorFor(builtin_);
    if (has_feedback()) {
      int slot_index = InputCountWithoutContext();
      int vector_index = slot_index + 1;

      // There are three possibilities:
      // 1. Feedback slot and vector are in register.
      // 2. Feedback slot is in register and vector is on stack.
      // 3. Feedback slot and vector are on stack.
      if (vector_index < descriptor.GetRegisterParameterCount()) {
        return descriptor.GetRegisterParameterCount() - 2;
      } else if (vector_index == descriptor.GetRegisterParameterCount()) {
        return descriptor.GetRegisterParameterCount() - 1;
      } else {
        return descriptor.GetRegisterParameterCount();
      }
    }
    return descriptor.GetRegisterParameterCount();
  }

  auto stack_args_begin() {
    return std::make_reverse_iterator(&input(InputsInRegisterCount() - 1));
  }
  auto stack_args_end() {
    return std::make_reverse_iterator(&input(InputCountWithoutContext() - 1));
  }

  void set_arg(int i, ValueNode* node) { set_input(i, node); }

  int ReturnCount() const {
    return Builtins::CallInterfaceDescriptorFor(builtin_).GetReturnCount();
  }

  void VerifyInputs(MaglevGraphLabeller* graph_labeller) const;
  void MarkTaggedInputsAsDecompressing();
  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  template <typename... Args>
  void PushArguments(MaglevAssembler* masm, Args... extra_args);
  void PassFeedbackSlotInRegister(MaglevAssembler*);
  void PushFeedbackAndArguments(MaglevAssembler*);

  Builtin builtin_;
  base::Optional<compiler::FeedbackSource> feedback_;
  FeedbackSlotType slot_type_ = kTaggedIndex;
};

class CallRuntime : public ValueNodeT<CallRuntime> {
  using Base = ValueNodeT<CallRuntime>;

 public:
  // We assume the context as fixed input.
  static constexpr int kContextIndex = 0;
  static constexpr int kFixedInputCount = 1;

  // This ctor is used when for variable input counts.
  // Inputs must be initialized manually.
  CallRuntime(uint64_t bitfield, Runtime::FunctionId function_id,
              ValueNode* context)
      : Base(bitfield), function_id_(function_id) {
    set_input(kContextIndex, context);
  }

  static constexpr OpProperties kProperties = OpProperties::JSCall();

  Runtime::FunctionId function_id() const { return function_id_; }

  Input& context() { return input(kContextIndex); }
  const Input& context() const { return input(kContextIndex); }
  int num_args() const { return input_count() - kFixedInputCount; }
  Input& arg(int i) { return input(i + kFixedInputCount); }
  void set_arg(int i, ValueNode* node) {
    set_input(i + kFixedInputCount, node);
  }
  auto args_begin() { return std::make_reverse_iterator(&arg(-1)); }
  auto args_end() { return std::make_reverse_iterator(&arg(num_args() - 1)); }

  int ReturnCount() const {
    return Runtime::FunctionForId(function_id())->result_size;
  }

  void VerifyInputs(MaglevGraphLabeller* graph_labeller) const;
  void MarkTaggedInputsAsDecompressing();
  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  Runtime::FunctionId function_id_;
};

class CallWithSpread : public ValueNodeT<CallWithSpread> {
  using Base = ValueNodeT<CallWithSpread>;

 public:
  // We assume function and context as fixed inputs.
  static constexpr int kFunctionIndex = 0;
  static constexpr int kContextIndex = 1;
  static constexpr int kFixedInputCount = 2;

  // This ctor is used when for variable input counts.
  // Inputs must be initialized manually.
  CallWithSpread(uint64_t bitfield, ValueNode* function, ValueNode* context)
      : Base(bitfield) {
    set_input(kFunctionIndex, function);
    set_input(kContextIndex, context);
  }

  static constexpr OpProperties kProperties = OpProperties::JSCall();

  Input& function() { return input(kFunctionIndex); }
  const Input& function() const { return input(kFunctionIndex); }
  Input& context() { return input(kContextIndex); }
  const Input& context() const { return input(kContextIndex); }
  int num_args() const { return input_count() - kFixedInputCount; }
  int num_args_no_spread() const {
    DCHECK_GT(num_args(), 0);
    return num_args() - 1;
  }
  Input& arg(int i) { return input(i + kFixedInputCount); }
  void set_arg(int i, ValueNode* node) {
    set_input(i + kFixedInputCount, node);
  }
  auto args_no_spread_begin() { return std::make_reverse_iterator(&arg(-1)); }
  auto args_no_spread_end() {
    return std::make_reverse_iterator(&arg(num_args_no_spread() - 1));
  }
  Input& spread() {
    // Spread is the last argument/input.
    return input(input_count() - 1);
  }
  Input& receiver() { return arg(0); }

  void VerifyInputs(MaglevGraphLabeller* graph_labeller) const;
  void MarkTaggedInputsAsDecompressing();
  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class CallWithArrayLike : public FixedInputValueNodeT<4, CallWithArrayLike> {
  using Base = FixedInputValueNodeT<4, CallWithArrayLike>;

 public:
  // We assume function and context as fixed inputs.
  static constexpr int kFunctionIndex = 0;
  static constexpr int kReceiverIndex = 1;
  static constexpr int kArgumentsListIndex = 2;
  static constexpr int kContextIndex = 3;

  // This ctor is used when for variable input counts.
  // Inputs must be initialized manually.
  explicit CallWithArrayLike(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::JSCall();

  Input& function() { return input(kFunctionIndex); }
  Input& receiver() { return input(kReceiverIndex); }
  Input& arguments_list() { return input(kArgumentsListIndex); }
  Input& context() { return input(kContextIndex); }

  void VerifyInputs(MaglevGraphLabeller* graph_labeller) const;
  void MarkTaggedInputsAsDecompressing();
  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class CallSelf : public ValueNodeT<CallSelf> {
  using Base = ValueNodeT<CallSelf>;

 public:
  static constexpr int kClosureIndex = 0;
  static constexpr int kContextIndex = 1;
  static constexpr int kReceiverIndex = 2;
  static constexpr int kNewTargetIndex = 3;
  static constexpr int kFixedInputCount = 4;

  // We need enough inputs to have these fixed inputs plus the maximum arguments
  // to a function call.
  static_assert(kMaxInputs >= kFixedInputCount + Code::kMaxArguments);

  // This ctor is used when for variable input counts.
  // Inputs must be initialized manually.
  CallSelf(uint64_t bitfield,
           compiler::SharedFunctionInfoRef shared_function_info,
           ValueNode* closure, ValueNode* context, ValueNode* receiver,
           ValueNode* new_target)
      : Base(bitfield),
        shared_function_info_(shared_function_info),
        expected_parameter_count_(
            shared_function_info
                .internal_formal_parameter_count_with_receiver()) {
    set_input(kClosureIndex, closure);
    set_input(kContextIndex, context);
    set_input(kReceiverIndex, receiver);
    set_input(kNewTargetIndex, new_target);
  }

  static constexpr OpProperties kProperties = OpProperties::JSCall();

  Input& closure() { return input(kClosureIndex); }
  const Input& closure() const { return input(kClosureIndex); }
  Input& context() { return input(kContextIndex); }
  const Input& context() const { return input(kContextIndex); }
  Input& receiver() { return input(kReceiverIndex); }
  const Input& receiver() const { return input(kReceiverIndex); }
  Input& new_target() { return input(kNewTargetIndex); }
  const Input& new_target() const { return input(kNewTargetIndex); }
  int num_args() const { return input_count() - kFixedInputCount; }
  Input& arg(int i) { return input(i + kFixedInputCount); }
  void set_arg(int i, ValueNode* node) {
    set_input(i + kFixedInputCount, node);
  }
  auto args_begin() { return std::make_reverse_iterator(&arg(-1)); }
  auto args_end() { return std::make_reverse_iterator(&arg(num_args() - 1)); }

  compiler::SharedFunctionInfoRef shared_function_info() const {
    return shared_function_info_;
  }

  void VerifyInputs(MaglevGraphLabeller* graph_labeller) const;
  void MarkTaggedInputsAsDecompressing();
  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const compiler::SharedFunctionInfoRef shared_function_info_;
  // Cache the expected parameter count so that we can access it in
  // MaxCallStackArgs without needing to unpark the local isolate.
  int expected_parameter_count_;
};

class CallKnownJSFunction : public ValueNodeT<CallKnownJSFunction> {
  using Base = ValueNodeT<CallKnownJSFunction>;

 public:
  static constexpr int kClosureIndex = 0;
  static constexpr int kContextIndex = 1;
  static constexpr int kReceiverIndex = 2;
  static constexpr int kNewTargetIndex = 3;
  static constexpr int kFixedInputCount = 4;

  // We need enough inputs to have these fixed inputs plus the maximum arguments
  // to a function call.
  static_assert(kMaxInputs >= kFixedInputCount + Code::kMaxArguments);

  // This ctor is used when for variable input counts.
  // Inputs must be initialized manually.
  CallKnownJSFunction(uint64_t bitfield,
                      compiler::SharedFunctionInfoRef shared_function_info,
                      ValueNode* closure, ValueNode* context,
                      ValueNode* receiver, ValueNode* new_target)
      : Base(bitfield),
        shared_function_info_(shared_function_info),
        expected_parameter_count_(
            shared_function_info
                .internal_formal_parameter_count_with_receiver()) {
    set_input(kClosureIndex, closure);
    set_input(kContextIndex, context);
    set_input(kReceiverIndex, receiver);
    set_input(kNewTargetIndex, new_target);
  }

  static constexpr OpProperties kProperties = OpProperties::JSCall();

  Input& closure() { return input(kClosureIndex); }
  const Input& closure() const { return input(kClosureIndex); }
  Input& context() { return input(kContextIndex); }
  const Input& context() const { return input(kContextIndex); }
  Input& receiver() { return input(kReceiverIndex); }
  const Input& receiver() const { return input(kReceiverIndex); }
  Input& new_target() { return input(kNewTargetIndex); }
  const Input& new_target() const { return input(kNewTargetIndex); }
  int num_args() const { return input_count() - kFixedInputCount; }
  Input& arg(int i) { return input(i + kFixedInputCount); }
  void set_arg(int i, ValueNode* node) {
    set_input(i + kFixedInputCount, node);
  }
  auto args_begin() { return std::make_reverse_iterator(&arg(-1)); }
  auto args_end() { return std::make_reverse_iterator(&arg(num_args() - 1)); }

  compiler::SharedFunctionInfoRef shared_function_info() const {
    return shared_function_info_;
  }

  void VerifyInputs(MaglevGraphLabeller* graph_labeller) const;
  void MarkTaggedInputsAsDecompressing();
  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const compiler::SharedFunctionInfoRef shared_function_info_;
  // Cache the expected parameter count so that we can access it in
  // MaxCallStackArgs without needing to unpark the local isolate.
  int expected_parameter_count_;
};

class CallKnownApiFunction : public ValueNodeT<CallKnownApiFunction> {
  using Base = ValueNodeT<CallKnownApiFunction>;

 public:
  static constexpr int kContextIndex = 0;
  static constexpr int kReceiverIndex = 1;
  static constexpr int kFixedInputCount = 2;

  // We need enough inputs to have these fixed inputs plus the maximum arguments
  // to a function call.
  static_assert(kMaxInputs >= kFixedInputCount + Code::kMaxArguments);

  // This ctor is used when for variable input counts.
  // Inputs must be initialized manually.
  CallKnownApiFunction(uint64_t bitfield,
                       compiler::FunctionTemplateInfoRef function_template_info,
                       compiler::CallHandlerInfoRef call_handler_info,
                       compiler::ObjectRef data,
                       compiler::OptionalJSObjectRef api_holder,
                       ValueNode* context, ValueNode* receiver)
      : Base(bitfield),
        function_template_info_(function_template_info),
        call_handler_info_(call_handler_info),
        data_(data),
        api_holder_(api_holder) {
    set_input(kContextIndex, context);
    set_input(kReceiverIndex, receiver);
  }

  // TODO(ishell): introduce JSApiCall() which will take C++ ABI into account
  // when deciding which registers to splill.
  static constexpr OpProperties kProperties = OpProperties::JSCall();

  // Input& closure() { return input(kClosureIndex); }
  // const Input& closure() const { return input(kClosureIndex); }
  Input& context() { return input(kContextIndex); }
  const Input& context() const { return input(kContextIndex); }
  Input& receiver() { return input(kReceiverIndex); }
  const Input& receiver() const { return input(kReceiverIndex); }
  int num_args() const { return input_count() - kFixedInputCount; }
  Input& arg(int i) { return input(i + kFixedInputCount); }
  void set_arg(int i, ValueNode* node) {
    set_input(i + kFixedInputCount, node);
  }
  auto args_begin() { return std::make_reverse_iterator(&arg(-1)); }
  auto args_end() { return std::make_reverse_iterator(&arg(num_args() - 1)); }

  compiler::FunctionTemplateInfoRef function_template_info() const {
    return function_template_info_;
  }
  compiler::CallHandlerInfoRef call_handler_info() const {
    return call_handler_info_;
  }
  compiler::ObjectRef data() const { return data_; }
  void VerifyInputs(MaglevGraphLabeller* graph_labeller) const;
  void MarkTaggedInputsAsDecompressing();
  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const compiler::FunctionTemplateInfoRef function_template_info_;
  const compiler::CallHandlerInfoRef call_handler_info_;
  const compiler::ObjectRef data_;
  const compiler::OptionalJSObjectRef api_holder_;
};

class ConstructWithSpread : public ValueNodeT<ConstructWithSpread> {
  using Base = ValueNodeT<ConstructWithSpread>;

 public:
  // We assume function and context as fixed inputs.
  static constexpr int kFunctionIndex = 0;
  static constexpr int kNewTargetIndex = 1;
  static constexpr int kContextIndex = 2;
  static constexpr int kFixedInputCount = 3;

  // This ctor is used when for variable input counts.
  // Inputs must be initialized manually.
  ConstructWithSpread(uint64_t bitfield, compiler::FeedbackSource feedback,
                      ValueNode* function, ValueNode* new_target,
                      ValueNode* context)
      : Base(bitfield), feedback_(feedback) {
    set_input(kFunctionIndex, function);
    set_input(kNewTargetIndex, new_target);
    set_input(kContextIndex, context);
  }

  static constexpr OpProperties kProperties = OpProperties::JSCall();

  Input& function() { return input(kFunctionIndex); }
  const Input& function() const { return input(kFunctionIndex); }
  Input& new_target() { return input(kNewTargetIndex); }
  const Input& new_target() const { return input(kNewTargetIndex); }
  Input& context() { return input(kContextIndex); }
  const Input& context() const { return input(kContextIndex); }
  int num_args() const { return input_count() - kFixedInputCount; }
  int num_args_no_spread() const {
    DCHECK_GT(num_args(), 0);
    return num_args() - 1;
  }
  Input& arg(int i) { return input(i + kFixedInputCount); }
  void set_arg(int i, ValueNode* node) {
    set_input(i + kFixedInputCount, node);
  }
  Input& spread() {
    // Spread is the last argument/input.
    return input(input_count() - 1);
  }
  auto args_no_spread_begin() { return std::make_reverse_iterator(&arg(-1)); }
  auto args_no_spread_end() {
    return std::make_reverse_iterator(&arg(num_args_no_spread() - 1));
  }
  compiler::FeedbackSource feedback() const { return feedback_; }

  void VerifyInputs(MaglevGraphLabeller* graph_labeller) const;
  void MarkTaggedInputsAsDecompressing();
  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  const compiler::FeedbackSource feedback_;
};

class ConvertReceiver : public FixedInputValueNodeT<1, ConvertReceiver> {
  using Base = FixedInputValueNodeT<1, ConvertReceiver>;

 public:
  explicit ConvertReceiver(uint64_t bitfield,
                           compiler::NativeContextRef native_context,
                           ConvertReceiverMode mode)
      : Base(bitfield), native_context_(native_context), mode_(mode) {}

  Input& receiver_input() { return input(0); }

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::Call();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  const compiler::NativeContextRef native_context_;
  ConvertReceiverMode mode_;
};

class CheckConstructResult
    : public FixedInputValueNodeT<2, CheckConstructResult> {
  using Base = FixedInputValueNodeT<2, CheckConstructResult>;

 public:
  explicit CheckConstructResult(uint64_t bitfield) : Base(bitfield) {}

  Input& construct_result_input() { return input(0); }
  Input& implicit_receiver_input() { return input(1); }

  static constexpr OpProperties kProperties =
      OpProperties::CanThrow() | OpProperties::DeferredCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class ConvertHoleToUndefined
    : public FixedInputValueNodeT<1, ConvertHoleToUndefined> {
  using Base = FixedInputValueNodeT<1, ConvertHoleToUndefined>;

 public:
  explicit ConvertHoleToUndefined(uint64_t bitfield) : Base(bitfield) {}

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input& object_input() { return input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class ReduceInterruptBudgetForLoop
    : public FixedInputNodeT<0, ReduceInterruptBudgetForLoop> {
  using Base = FixedInputNodeT<0, ReduceInterruptBudgetForLoop>;

 public:
  explicit ReduceInterruptBudgetForLoop(uint64_t bitfield, int amount)
      : Base(bitfield), amount_(amount) {
    DCHECK_GT(amount, 0);
  }

  static constexpr OpProperties kProperties =
      OpProperties::DeferredCall() | OpProperties::LazyDeopt();

  int amount() const { return amount_; }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const int amount_;
};

class ReduceInterruptBudgetForReturn
    : public FixedInputNodeT<0, ReduceInterruptBudgetForReturn> {
  using Base = FixedInputNodeT<0, ReduceInterruptBudgetForReturn>;

 public:
  explicit ReduceInterruptBudgetForReturn(uint64_t bitfield, int amount)
      : Base(bitfield), amount_(amount) {
    DCHECK_GT(amount, 0);
  }

  static constexpr OpProperties kProperties = OpProperties::DeferredCall();

  int amount() const { return amount_; }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const int amount_;
};

class ThrowReferenceErrorIfHole
    : public FixedInputNodeT<1, ThrowReferenceErrorIfHole> {
  using Base = FixedInputNodeT<1, ThrowReferenceErrorIfHole>;

 public:
  explicit ThrowReferenceErrorIfHole(uint64_t bitfield, compiler::NameRef name)
      : Base(bitfield), name_(name) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanThrow() | OpProperties::DeferredCall();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  compiler::NameRef name() const { return name_; }

  Input& value() { return Node::input(0); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  const compiler::NameRef name_;
};

class ThrowSuperNotCalledIfHole
    : public FixedInputNodeT<1, ThrowSuperNotCalledIfHole> {
  using Base = FixedInputNodeT<1, ThrowSuperNotCalledIfHole>;

 public:
  explicit ThrowSuperNotCalledIfHole(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanThrow() | OpProperties::DeferredCall();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input& value() { return Node::input(0); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class ThrowSuperAlreadyCalledIfNotHole
    : public FixedInputNodeT<1, ThrowSuperAlreadyCalledIfNotHole> {
  using Base = FixedInputNodeT<1, ThrowSuperAlreadyCalledIfNotHole>;

 public:
  explicit ThrowSuperAlreadyCalledIfNotHole(uint64_t bitfield)
      : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanThrow() | OpProperties::DeferredCall();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input& value() { return Node::input(0); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class ThrowIfNotCallable : public FixedInputNodeT<1, ThrowIfNotCallable> {
  using Base = FixedInputNodeT<1, ThrowIfNotCallable>;

 public:
  explicit ThrowIfNotCallable(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanThrow() | OpProperties::DeferredCall();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input& value() { return Node::input(0); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class ThrowIfNotSuperConstructor
    : public FixedInputNodeT<2, ThrowIfNotSuperConstructor> {
  using Base = FixedInputNodeT<2, ThrowIfNotSuperConstructor>;

 public:
  explicit ThrowIfNotSuperConstructor(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanThrow() | OpProperties::DeferredCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  Input& constructor() { return Node::input(0); }
  Input& function() { return Node::input(1); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class TransitionElementsKindOrCheckMap
    : public FixedInputNodeT<1, TransitionElementsKindOrCheckMap> {
  using Base = FixedInputNodeT<1, TransitionElementsKindOrCheckMap>;

 public:
  explicit TransitionElementsKindOrCheckMap(
      uint64_t bitfield,
      base::Vector<const compiler::MapRef> transition_sources,
      compiler::MapRef transition_target, CheckType check_type)
      : Base(CheckTypeBitField::update(bitfield, check_type)),
        transition_sources_(transition_sources),
        transition_target_(transition_target) {}

  // TODO(leszeks): Special case the case where all transitions are fast.
  static constexpr OpProperties kProperties = OpProperties::AnySideEffects() |
                                              OpProperties::DeferredCall() |
                                              OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  CheckType check_type() const { return CheckTypeBitField::decode(bitfield()); }

  Input& object_input() { return Node::input(0); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  using CheckTypeBitField = NextBitField<CheckType, 1>;

  const base::Vector<const compiler::MapRef> transition_sources_;
  const compiler::MapRef transition_target_;
};

class ControlNode : public NodeBase {
 public:
  // A "hole" in control flow is a control node that unconditionally interrupts
  // linear control flow (either by jumping or by exiting).
  //
  // A "post-dominating" hole is a hole that is guaranteed to be be reached in
  // control flow after this node (i.e. it is a hole that is a post-dominator
  // of this node).
  ControlNode* next_post_dominating_hole() const {
    return next_post_dominating_hole_;
  }
  void set_next_post_dominating_hole(ControlNode* node) {
    DCHECK_IMPLIES(node != nullptr, node->Is<UnconditionalControlNode>() ||
                                        node->Is<TerminalControlNode>() ||
                                        node->Is<Switch>());
    next_post_dominating_hole_ = node;
  }

 protected:
  using NodeBase::NodeBase;

 private:
  ControlNode* next_post_dominating_hole_ = nullptr;
};

class UnconditionalControlNode : public ControlNode {
 public:
  BasicBlock* target() const { return target_.block_ptr(); }
  int predecessor_id() const { return predecessor_id_; }
  void set_predecessor_id(int id) { predecessor_id_ = id; }

 protected:
  explicit UnconditionalControlNode(uint64_t bitfield,
                                    BasicBlockRef* target_refs)
      : ControlNode(bitfield), target_(target_refs) {}
  explicit UnconditionalControlNode(uint64_t bitfield, BasicBlock* target)
      : ControlNode(bitfield), target_(target) {}

 private:
  const BasicBlockRef target_;
  int predecessor_id_ = 0;
};

template <class Derived>
class UnconditionalControlNodeT
    : public FixedInputNodeTMixin<0, UnconditionalControlNode, Derived> {
  static_assert(IsUnconditionalControlNode(NodeBase::opcode_of<Derived>));

 protected:
  explicit UnconditionalControlNodeT(uint64_t bitfield,
                                     BasicBlockRef* target_refs)
      : FixedInputNodeTMixin<0, UnconditionalControlNode, Derived>(
            bitfield, target_refs) {}
  explicit UnconditionalControlNodeT(uint64_t bitfield, BasicBlock* target)
      : FixedInputNodeTMixin<0, UnconditionalControlNode, Derived>(bitfield,
                                                                   target) {}
};

class ConditionalControlNode : public ControlNode {
 public:
  explicit ConditionalControlNode(uint64_t bitfield) : ControlNode(bitfield) {}
};

class BranchControlNode : public ConditionalControlNode {
 public:
  BranchControlNode(uint64_t bitfield, BasicBlockRef* if_true_refs,
                    BasicBlockRef* if_false_refs)
      : ConditionalControlNode(bitfield),
        if_true_(if_true_refs),
        if_false_(if_false_refs) {}

  BasicBlock* if_true() const { return if_true_.block_ptr(); }
  BasicBlock* if_false() const { return if_false_.block_ptr(); }

 private:
  BasicBlockRef if_true_;
  BasicBlockRef if_false_;
};

class TerminalControlNode : public ControlNode {
 protected:
  explicit TerminalControlNode(uint64_t bitfield) : ControlNode(bitfield) {}
};

template <size_t InputCount, class Derived>
class TeminalControlNodeT
    : public FixedInputNodeTMixin<InputCount, TerminalControlNode, Derived> {
  static_assert(IsTerminalControlNode(NodeBase::opcode_of<Derived>));

 protected:
  explicit TeminalControlNodeT(uint64_t bitfield)
      : FixedInputNodeTMixin<InputCount, TerminalControlNode, Derived>(
            bitfield) {}
};

template <size_t InputCount, class Derived>
class BranchControlNodeT
    : public FixedInputNodeTMixin<InputCount, BranchControlNode, Derived> {
  static_assert(IsBranchControlNode(NodeBase::opcode_of<Derived>));

 protected:
  explicit BranchControlNodeT(uint64_t bitfield, BasicBlockRef* if_true_refs,
                              BasicBlockRef* if_false_refs)
      : FixedInputNodeTMixin<InputCount, BranchControlNode, Derived>(
            bitfield, if_true_refs, if_false_refs) {}
};

class Jump : public UnconditionalControlNodeT<Jump> {
  using Base = UnconditionalControlNodeT<Jump>;

 public:
  Jump(uint64_t bitfield, BasicBlockRef* target_refs)
      : Base(bitfield, target_refs) {}

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class JumpLoop : public UnconditionalControlNodeT<JumpLoop> {
  using Base = UnconditionalControlNodeT<JumpLoop>;

 public:
  explicit JumpLoop(uint64_t bitfield, BasicBlock* target)
      : Base(bitfield, target) {}

  explicit JumpLoop(uint64_t bitfield, BasicBlockRef* ref)
      : Base(bitfield, ref) {}

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

  base::Vector<Input> used_nodes() { return used_node_locations_; }
  void set_used_nodes(base::Vector<Input> locations) {
    used_node_locations_ = locations;
  }

 private:
  base::Vector<Input> used_node_locations_;
};

class Abort : public TeminalControlNodeT<0, Abort> {
  using Base = TeminalControlNodeT<0, Abort>;

 public:
  explicit Abort(uint64_t bitfield, AbortReason reason)
      : Base(bitfield), reason_(reason) {
    DCHECK_EQ(NodeBase::opcode(), opcode_of<Abort>);
  }

  static constexpr OpProperties kProperties = OpProperties::Call();

  AbortReason reason() const { return reason_; }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const AbortReason reason_;
};

class Return : public TeminalControlNodeT<1, Return> {
  using Base = TeminalControlNodeT<1, Return>;

 public:
  explicit Return(uint64_t bitfield) : Base(bitfield) {
    DCHECK_EQ(NodeBase::opcode(), opcode_of<Return>);
  }

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input& value_input() { return input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class Deopt : public TeminalControlNodeT<0, Deopt> {
  using Base = TeminalControlNodeT<0, Deopt>;

 public:
  explicit Deopt(uint64_t bitfield, DeoptimizeReason reason)
      : Base(bitfield), reason_(reason) {
    DCHECK_EQ(NodeBase::opcode(), opcode_of<Deopt>);
  }

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();

  DeoptimizeReason reason() const { return reason_; }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  DeoptimizeReason reason_;
};

class Switch : public FixedInputNodeTMixin<1, ConditionalControlNode, Switch> {
  using Base = FixedInputNodeTMixin<1, ConditionalControlNode, Switch>;

 public:
  explicit Switch(uint64_t bitfield, int value_base, BasicBlockRef* targets,
                  int size)
      : Base(bitfield),
        value_base_(value_base),
        targets_(targets),
        size_(size),
        fallthrough_() {}

  explicit Switch(uint64_t bitfield, int value_base, BasicBlockRef* targets,
                  int size, BasicBlockRef* fallthrough)
      : Base(bitfield),
        value_base_(value_base),
        targets_(targets),
        size_(size),
        fallthrough_(fallthrough) {}

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kInt32};

  int value_base() const { return value_base_; }
  const BasicBlockRef* targets() const { return targets_; }
  int size() const { return size_; }

  bool has_fallthrough() const { return fallthrough_.has_value(); }
  BasicBlock* fallthrough() const {
    DCHECK(has_fallthrough());
    return fallthrough_.value().block_ptr();
  }

  Input& value() { return input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  const int value_base_;
  const BasicBlockRef* targets_;
  const int size_;
  base::Optional<BasicBlockRef> fallthrough_;
};

class BranchIfRootConstant
    : public BranchControlNodeT<1, BranchIfRootConstant> {
  using Base = BranchControlNodeT<1, BranchIfRootConstant>;

 public:
  explicit BranchIfRootConstant(uint64_t bitfield, RootIndex root_index,
                                BasicBlockRef* if_true_refs,
                                BasicBlockRef* if_false_refs)
      : Base(bitfield, if_true_refs, if_false_refs), root_index_(root_index) {}

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  RootIndex root_index() { return root_index_; }
  Input& condition_input() { return input(0); }

  void MarkTaggedInputsAsDecompressing() {
    // Don't need to decompress values to reference compare.
  }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  RootIndex root_index_;
};

class BranchIfUndefinedOrNull
    : public BranchControlNodeT<1, BranchIfUndefinedOrNull> {
  using Base = BranchControlNodeT<1, BranchIfUndefinedOrNull>;

 public:
  explicit BranchIfUndefinedOrNull(uint64_t bitfield,
                                   BasicBlockRef* if_true_refs,
                                   BasicBlockRef* if_false_refs)
      : Base(bitfield, if_true_refs, if_false_refs) {}

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input& condition_input() { return input(0); }

  void MarkTaggedInputsAsDecompressing() {
    // Don't need to decompress values to reference compare.
  }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class BranchIfUndetectable
    : public BranchControlNodeT<1, BranchIfUndetectable> {
  using Base = BranchControlNodeT<1, BranchIfUndetectable>;

 public:
  explicit BranchIfUndetectable(uint64_t bitfield, CheckType check_type,
                                BasicBlockRef* if_true_refs,
                                BasicBlockRef* if_false_refs)
      : Base(CheckTypeBitField::update(bitfield, check_type), if_true_refs,
             if_false_refs) {}

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input& condition_input() { return input(0); }
  CheckType check_type() const { return CheckTypeBitField::decode(bitfield()); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  using CheckTypeBitField = NextBitField<CheckType, 1>;
};

class BranchIfJSReceiver : public BranchControlNodeT<1, BranchIfJSReceiver> {
  using Base = BranchControlNodeT<1, BranchIfJSReceiver>;

 public:
  explicit BranchIfJSReceiver(uint64_t bitfield, BasicBlockRef* if_true_refs,
                              BasicBlockRef* if_false_refs)
      : Base(bitfield, if_true_refs, if_false_refs) {}

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input& condition_input() { return input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class BranchIfToBooleanTrue
    : public BranchControlNodeT<1, BranchIfToBooleanTrue> {
  using Base = BranchControlNodeT<1, BranchIfToBooleanTrue>;

 public:
  explicit BranchIfToBooleanTrue(uint64_t bitfield, CheckType check_type,
                                 BasicBlockRef* if_true_refs,
                                 BasicBlockRef* if_false_refs)
      : Base(CheckTypeBitField::update(bitfield, check_type), if_true_refs,
             if_false_refs) {}

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input& condition_input() { return input(0); }
  CheckType check_type() const { return CheckTypeBitField::decode(bitfield()); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  using CheckTypeBitField = NextBitField<CheckType, 1>;
};

class BranchIfInt32ToBooleanTrue
    : public BranchControlNodeT<1, BranchIfInt32ToBooleanTrue> {
  using Base = BranchControlNodeT<1, BranchIfInt32ToBooleanTrue>;

 public:
  explicit BranchIfInt32ToBooleanTrue(uint64_t bitfield,
                                      BasicBlockRef* if_true_refs,
                                      BasicBlockRef* if_false_refs)
      : Base(bitfield, if_true_refs, if_false_refs) {}

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kInt32};

  Input& condition_input() { return input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class BranchIfFloat64ToBooleanTrue
    : public BranchControlNodeT<1, BranchIfFloat64ToBooleanTrue> {
  using Base = BranchControlNodeT<1, BranchIfFloat64ToBooleanTrue>;

 public:
  explicit BranchIfFloat64ToBooleanTrue(uint64_t bitfield,
                                        BasicBlockRef* if_true_refs,
                                        BasicBlockRef* if_false_refs)
      : Base(bitfield, if_true_refs, if_false_refs) {}

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kHoleyFloat64};

  Input& condition_input() { return input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class BranchIfFloat64IsHole
    : public BranchControlNodeT<1, BranchIfFloat64IsHole> {
  using Base = BranchControlNodeT<1, BranchIfFloat64IsHole>;

 public:
  explicit BranchIfFloat64IsHole(uint64_t bitfield, BasicBlockRef* if_true_refs,
                                 BasicBlockRef* if_false_refs)
      : Base(bitfield, if_true_refs, if_false_refs) {}

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kHoleyFloat64};

  Input& condition_input() { return input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class BranchIfInt32Compare
    : public BranchControlNodeT<2, BranchIfInt32Compare> {
  using Base = BranchControlNodeT<2, BranchIfInt32Compare>;

 public:
  static constexpr int kLeftIndex = 0;
  static constexpr int kRightIndex = 1;
  Input& left_input() { return NodeBase::input(kLeftIndex); }
  Input& right_input() { return NodeBase::input(kRightIndex); }

  explicit BranchIfInt32Compare(uint64_t bitfield, Operation operation,
                                BasicBlockRef* if_true_refs,
                                BasicBlockRef* if_false_refs)
      : Base(bitfield, if_true_refs, if_false_refs), operation_(operation) {}

  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kInt32, ValueRepresentation::kInt32};

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  Operation operation_;
};

class BranchIfFloat64Compare
    : public BranchControlNodeT<2, BranchIfFloat64Compare> {
  using Base = BranchControlNodeT<2, BranchIfFloat64Compare>;

 public:
  static constexpr int kLeftIndex = 0;
  static constexpr int kRightIndex = 1;
  Input& left_input() { return NodeBase::input(kLeftIndex); }
  Input& right_input() { return NodeBase::input(kRightIndex); }

  explicit BranchIfFloat64Compare(uint64_t bitfield, Operation operation,
                                  BasicBlockRef* if_true_refs,
                                  BasicBlockRef* if_false_refs)
      : Base(bitfield, if_true_refs, if_false_refs), operation_(operation) {}

  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kFloat64, ValueRepresentation::kFloat64};

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  Operation operation_;
};

class BranchIfReferenceCompare
    : public BranchControlNodeT<2, BranchIfReferenceCompare> {
  using Base = BranchControlNodeT<2, BranchIfReferenceCompare>;

 public:
  static constexpr int kLeftIndex = 0;
  static constexpr int kRightIndex = 1;
  Input& left_input() { return NodeBase::input(kLeftIndex); }
  Input& right_input() { return NodeBase::input(kRightIndex); }

  explicit BranchIfReferenceCompare(uint64_t bitfield, Operation operation,
                                    BasicBlockRef* if_true_refs,
                                    BasicBlockRef* if_false_refs)
      : Base(bitfield, if_true_refs, if_false_refs), operation_(operation) {}

  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  void MarkTaggedInputsAsDecompressing() {
    // Don't need to decompress values to reference compare.
  }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  Operation operation_;
};

class BranchIfTypeOf : public BranchControlNodeT<1, BranchIfTypeOf> {
  using Base = BranchControlNodeT<1, BranchIfTypeOf>;

 public:
  static constexpr int kValueIndex = 0;
  Input& value_input() { return NodeBase::input(kValueIndex); }

  explicit BranchIfTypeOf(uint64_t bitfield,
                          interpreter::TestTypeOfFlags::LiteralFlag literal,
                          BasicBlockRef* if_true_refs,
                          BasicBlockRef* if_false_refs)
      : Base(bitfield, if_true_refs, if_false_refs), literal_(literal) {}

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  interpreter::TestTypeOfFlags::LiteralFlag literal_;
};

constexpr inline OpProperties StaticPropertiesForOpcode(Opcode opcode) {
  switch (opcode) {
#define CASE(op)      \
  case Opcode::k##op: \
    return op::kProperties;
    NODE_BASE_LIST(CASE)
#undef CASE
  }
}

template <typename Function>
inline void NodeBase::ForAllInputsInRegallocAssignmentOrder(Function&& f) {
  auto iterate_inputs = [&](InputAllocationPolicy category) {
    for (Input& input : *this) {
      switch (compiler::UnallocatedOperand::cast(input.operand())
                  .extended_policy()) {
        case compiler::UnallocatedOperand::MUST_HAVE_REGISTER:
          if (category == InputAllocationPolicy::kArbitraryRegister)
            f(category, &input);
          break;

        case compiler::UnallocatedOperand::REGISTER_OR_SLOT_OR_CONSTANT:
          if (category == InputAllocationPolicy::kAny) f(category, &input);
          break;

        case compiler::UnallocatedOperand::FIXED_REGISTER:
        case compiler::UnallocatedOperand::FIXED_FP_REGISTER:
          if (category == InputAllocationPolicy::kFixedRegister)
            f(category, &input);
          break;

        case compiler::UnallocatedOperand::REGISTER_OR_SLOT:
        case compiler::UnallocatedOperand::SAME_AS_INPUT:
        case compiler::UnallocatedOperand::NONE:
        case compiler::UnallocatedOperand::MUST_HAVE_SLOT:
          UNREACHABLE();
      }
    }
  };

  iterate_inputs(InputAllocationPolicy::kFixedRegister);
  iterate_inputs(InputAllocationPolicy::kArbitraryRegister);
  iterate_inputs(InputAllocationPolicy::kAny);
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_IR_H_
