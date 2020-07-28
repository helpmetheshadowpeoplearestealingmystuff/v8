// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/simplified-lowering.h"

#include <limits>

#include "include/v8-fast-api-calls.h"
#include "src/base/bits.h"
#include "src/base/small-vector.h"
#include "src/codegen/code-factory.h"
#include "src/codegen/machine-type.h"
#include "src/codegen/tick-counter.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/compiler-source-position-table.h"
#include "src/compiler/diamond.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-origin-table.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/operation-typer.h"
#include "src/compiler/operator-properties.h"
#include "src/compiler/representation-change.h"
#include "src/compiler/simplified-operator.h"
#include "src/compiler/type-cache.h"
#include "src/numbers/conversions-inl.h"
#include "src/objects/objects.h"
#include "src/utils/address-map.h"

namespace v8 {
namespace internal {
namespace compiler {

// Macro for outputting trace information from representation inference.
#define TRACE(...)                                      \
  do {                                                  \
    if (FLAG_trace_representation) PrintF(__VA_ARGS__); \
  } while (false)

// Representation selection and lowering of {Simplified} operators to machine
// operators are interwined. We use a fixpoint calculation to compute both the
// output representation and the best possible lowering for {Simplified} nodes.
// Representation change insertion ensures that all values are in the correct
// machine representation after this phase, as dictated by the machine
// operators themselves.
enum Phase {
  // 1.) PROPAGATE: Traverse the graph from the end, pushing usage information
  //     backwards from uses to definitions, around cycles in phis, according
  //     to local rules for each operator.
  //     During this phase, the usage information for a node determines the best
  //     possible lowering for each operator so far, and that in turn determines
  //     the output representation.
  //     Therefore, to be correct, this phase must iterate to a fixpoint before
  //     the next phase can begin.
  PROPAGATE,

  // 2.) RETYPE: Propagate types from type feedback forwards.
  RETYPE,

  // 3.) LOWER: perform lowering for all {Simplified} nodes by replacing some
  //     operators for some nodes, expanding some nodes to multiple nodes, or
  //     removing some (redundant) nodes.
  //     During this phase, use the {RepresentationChanger} to insert
  //     representation changes between uses that demand a particular
  //     representation and nodes that produce a different representation.
  LOWER
};

namespace {

MachineRepresentation MachineRepresentationFromArrayType(
    ExternalArrayType array_type) {
  switch (array_type) {
    case kExternalUint8Array:
    case kExternalUint8ClampedArray:
    case kExternalInt8Array:
      return MachineRepresentation::kWord8;
    case kExternalUint16Array:
    case kExternalInt16Array:
      return MachineRepresentation::kWord16;
    case kExternalUint32Array:
    case kExternalInt32Array:
      return MachineRepresentation::kWord32;
    case kExternalFloat32Array:
      return MachineRepresentation::kFloat32;
    case kExternalFloat64Array:
      return MachineRepresentation::kFloat64;
    case kExternalBigInt64Array:
    case kExternalBigUint64Array:
      UNIMPLEMENTED();
  }
  UNREACHABLE();
}

UseInfo CheckedUseInfoAsWord32FromHint(
    NumberOperationHint hint, const FeedbackSource& feedback = FeedbackSource(),
    IdentifyZeros identify_zeros = kDistinguishZeros) {
  switch (hint) {
    case NumberOperationHint::kSignedSmall:
    case NumberOperationHint::kSignedSmallInputs:
      return UseInfo::CheckedSignedSmallAsWord32(identify_zeros, feedback);
    case NumberOperationHint::kSigned32:
      return UseInfo::CheckedSigned32AsWord32(identify_zeros, feedback);
    case NumberOperationHint::kNumber:
      return UseInfo::CheckedNumberAsWord32(feedback);
    case NumberOperationHint::kNumberOrBoolean:
      // Not used currently.
      UNREACHABLE();
    case NumberOperationHint::kNumberOrOddball:
      return UseInfo::CheckedNumberOrOddballAsWord32(feedback);
  }
  UNREACHABLE();
}

UseInfo CheckedUseInfoAsFloat64FromHint(
    NumberOperationHint hint, const FeedbackSource& feedback,
    IdentifyZeros identify_zeros = kDistinguishZeros) {
  switch (hint) {
    case NumberOperationHint::kSignedSmall:
    case NumberOperationHint::kSignedSmallInputs:
    case NumberOperationHint::kSigned32:
      // Not used currently.
      UNREACHABLE();
    case NumberOperationHint::kNumber:
      return UseInfo::CheckedNumberAsFloat64(identify_zeros, feedback);
    case NumberOperationHint::kNumberOrBoolean:
      return UseInfo::CheckedNumberOrBooleanAsFloat64(identify_zeros, feedback);
    case NumberOperationHint::kNumberOrOddball:
      return UseInfo::CheckedNumberOrOddballAsFloat64(identify_zeros, feedback);
  }
  UNREACHABLE();
}

UseInfo TruncatingUseInfoFromRepresentation(MachineRepresentation rep) {
  switch (rep) {
    case MachineRepresentation::kTaggedSigned:
      return UseInfo::TaggedSigned();
    case MachineRepresentation::kTaggedPointer:
    case MachineRepresentation::kTagged:
      return UseInfo::AnyTagged();
    case MachineRepresentation::kFloat64:
      return UseInfo::TruncatingFloat64();
    case MachineRepresentation::kFloat32:
      return UseInfo::Float32();
    case MachineRepresentation::kWord8:
    case MachineRepresentation::kWord16:
    case MachineRepresentation::kWord32:
      return UseInfo::TruncatingWord32();
    case MachineRepresentation::kWord64:
      return UseInfo::Word64();
    case MachineRepresentation::kBit:
      return UseInfo::Bool();
    case MachineRepresentation::kCompressedPointer:
    case MachineRepresentation::kCompressed:
    case MachineRepresentation::kSimd128:
    case MachineRepresentation::kNone:
      break;
  }
  UNREACHABLE();
}

UseInfo UseInfoForBasePointer(const FieldAccess& access) {
  return access.tag() != 0 ? UseInfo::AnyTagged() : UseInfo::Word();
}

UseInfo UseInfoForBasePointer(const ElementAccess& access) {
  return access.tag() != 0 ? UseInfo::AnyTagged() : UseInfo::Word();
}

void ReplaceEffectControlUses(Node* node, Node* effect, Node* control) {
  for (Edge edge : node->use_edges()) {
    if (NodeProperties::IsControlEdge(edge)) {
      edge.UpdateTo(control);
    } else if (NodeProperties::IsEffectEdge(edge)) {
      edge.UpdateTo(effect);
    } else {
      DCHECK(NodeProperties::IsValueEdge(edge) ||
             NodeProperties::IsContextEdge(edge));
    }
  }
}

bool CanOverflowSigned32(const Operator* op, Type left, Type right,
                         Zone* type_zone) {
  // We assume the inputs are checked Signed32 (or known statically
  // to be Signed32). Technically, the inputs could also be minus zero, but
  // that cannot cause overflow.
  left = Type::Intersect(left, Type::Signed32(), type_zone);
  right = Type::Intersect(right, Type::Signed32(), type_zone);
  if (left.IsNone() || right.IsNone()) return false;
  switch (op->opcode()) {
    case IrOpcode::kSpeculativeSafeIntegerAdd:
      return (left.Max() + right.Max() > kMaxInt) ||
             (left.Min() + right.Min() < kMinInt);

    case IrOpcode::kSpeculativeSafeIntegerSubtract:
      return (left.Max() - right.Min() > kMaxInt) ||
             (left.Min() - right.Max() < kMinInt);

    default:
      UNREACHABLE();
  }
  return true;
}

bool IsSomePositiveOrderedNumber(Type type) {
  return type.Is(Type::OrderedNumber()) && !type.IsNone() && type.Min() > 0;
}

}  // namespace

#ifdef DEBUG
// Helpers for monotonicity checking.
class InputUseInfos {
 public:
  explicit InputUseInfos(Zone* zone) : input_use_infos_(zone) {}

  void SetAndCheckInput(Node* node, int index, UseInfo use_info) {
    if (input_use_infos_.empty()) {
      input_use_infos_.resize(node->InputCount(), UseInfo::None());
    }
    // Check that the new use informatin is a super-type of the old
    // one.
    DCHECK(IsUseLessGeneral(input_use_infos_[index], use_info));
    input_use_infos_[index] = use_info;
  }

 private:
  ZoneVector<UseInfo> input_use_infos_;

  static bool IsUseLessGeneral(UseInfo use1, UseInfo use2) {
    return use1.truncation().IsLessGeneralThan(use2.truncation());
  }
};

#endif  // DEBUG

class RepresentationSelector {
 public:
  // Information for each node tracked during the fixpoint.
  class NodeInfo final {
   public:
    // Adds new use to the node. Returns true if something has changed
    // and the node has to be requeued.
    bool AddUse(UseInfo info) {
      Truncation old_truncation = truncation_;
      truncation_ = Truncation::Generalize(truncation_, info.truncation());
      return truncation_ != old_truncation;
    }

    void set_queued() { state_ = kQueued; }
    void set_visited() { state_ = kVisited; }
    void set_pushed() { state_ = kPushed; }
    void reset_state() { state_ = kUnvisited; }
    bool visited() const { return state_ == kVisited; }
    bool queued() const { return state_ == kQueued; }
    bool pushed() const { return state_ == kPushed; }
    bool unvisited() const { return state_ == kUnvisited; }
    Truncation truncation() const { return truncation_; }
    void set_output(MachineRepresentation output) { representation_ = output; }

    MachineRepresentation representation() const { return representation_; }

    // Helpers for feedback typing.
    void set_feedback_type(Type type) { feedback_type_ = type; }
    Type feedback_type() const { return feedback_type_; }
    void set_weakened() { weakened_ = true; }
    bool weakened() const { return weakened_; }
    void set_restriction_type(Type type) { restriction_type_ = type; }
    Type restriction_type() const { return restriction_type_; }

   private:
    enum State : uint8_t { kUnvisited, kPushed, kVisited, kQueued };
    State state_ = kUnvisited;
    MachineRepresentation representation_ =
        MachineRepresentation::kNone;             // Output representation.
    Truncation truncation_ = Truncation::None();  // Information about uses.

    Type restriction_type_ = Type::Any();
    Type feedback_type_;
    bool weakened_ = false;
  };

  RepresentationSelector(JSGraph* jsgraph, JSHeapBroker* broker, Zone* zone,
                         RepresentationChanger* changer,
                         SourcePositionTable* source_positions,
                         NodeOriginTable* node_origins,
                         TickCounter* tick_counter)
      : jsgraph_(jsgraph),
        zone_(zone),
        might_need_revisit_(zone),
        count_(jsgraph->graph()->NodeCount()),
        info_(count_, zone),
#ifdef DEBUG
        node_input_use_infos_(count_, InputUseInfos(zone), zone),
#endif
        replacements_(zone),
        changer_(changer),
        revisit_queue_(zone),
        traversal_nodes_(zone),
        source_positions_(source_positions),
        node_origins_(node_origins),
        type_cache_(TypeCache::Get()),
        op_typer_(broker, graph_zone()),
        tick_counter_(tick_counter) {
  }

  void ResetNodeInfoState() {
    // Clean up for the next phase.
    for (NodeInfo& info : info_) {
      info.reset_state();
    }
  }

  Type TypeOf(Node* node) {
    Type type = GetInfo(node)->feedback_type();
    return type.IsInvalid() ? NodeProperties::GetType(node) : type;
  }

  Type FeedbackTypeOf(Node* node) {
    Type type = GetInfo(node)->feedback_type();
    return type.IsInvalid() ? Type::None() : type;
  }

  Type TypePhi(Node* node) {
    int arity = node->op()->ValueInputCount();
    Type type = FeedbackTypeOf(node->InputAt(0));
    for (int i = 1; i < arity; ++i) {
      type = op_typer_.Merge(type, FeedbackTypeOf(node->InputAt(i)));
    }
    return type;
  }

  Type TypeSelect(Node* node) {
    return op_typer_.Merge(FeedbackTypeOf(node->InputAt(1)),
                           FeedbackTypeOf(node->InputAt(2)));
  }

  bool UpdateFeedbackType(Node* node) {
    if (node->op()->ValueOutputCount() == 0) return false;

    // For any non-phi node just wait until we get all inputs typed. We only
    // allow untyped inputs for phi nodes because phis are the only places
    // where cycles need to be broken.
    if (node->opcode() != IrOpcode::kPhi) {
      for (int i = 0; i < node->op()->ValueInputCount(); i++) {
        if (GetInfo(node->InputAt(i))->feedback_type().IsInvalid()) {
          return false;
        }
      }
    }

    NodeInfo* info = GetInfo(node);
    Type type = info->feedback_type();
    Type new_type = NodeProperties::GetType(node);

    // We preload these values here to avoid increasing the binary size too
    // much, which happens if we inline the calls into the macros below.
    Type input0_type;
    if (node->InputCount() > 0) input0_type = FeedbackTypeOf(node->InputAt(0));
    Type input1_type;
    if (node->InputCount() > 1) input1_type = FeedbackTypeOf(node->InputAt(1));

    switch (node->opcode()) {
#define DECLARE_CASE(Name)                               \
  case IrOpcode::k##Name: {                              \
    new_type = op_typer_.Name(input0_type, input1_type); \
    break;                                               \
  }
      SIMPLIFIED_NUMBER_BINOP_LIST(DECLARE_CASE)
      DECLARE_CASE(SameValue)
#undef DECLARE_CASE

#define DECLARE_CASE(Name)                                               \
  case IrOpcode::k##Name: {                                              \
    new_type = Type::Intersect(op_typer_.Name(input0_type, input1_type), \
                               info->restriction_type(), graph_zone());  \
    break;                                                               \
  }
      SIMPLIFIED_SPECULATIVE_NUMBER_BINOP_LIST(DECLARE_CASE)
      SIMPLIFIED_SPECULATIVE_BIGINT_BINOP_LIST(DECLARE_CASE)
#undef DECLARE_CASE

#define DECLARE_CASE(Name)                  \
  case IrOpcode::k##Name: {                 \
    new_type = op_typer_.Name(input0_type); \
    break;                                  \
  }
      SIMPLIFIED_NUMBER_UNOP_LIST(DECLARE_CASE)
#undef DECLARE_CASE

#define DECLARE_CASE(Name)                                              \
  case IrOpcode::k##Name: {                                             \
    new_type = Type::Intersect(op_typer_.Name(input0_type),             \
                               info->restriction_type(), graph_zone()); \
    break;                                                              \
  }
      SIMPLIFIED_SPECULATIVE_NUMBER_UNOP_LIST(DECLARE_CASE)
#undef DECLARE_CASE

      case IrOpcode::kConvertReceiver:
        new_type = op_typer_.ConvertReceiver(input0_type);
        break;

      case IrOpcode::kPlainPrimitiveToNumber:
        new_type = op_typer_.ToNumber(input0_type);
        break;

      case IrOpcode::kCheckBounds:
        new_type =
            Type::Intersect(op_typer_.CheckBounds(input0_type, input1_type),
                            info->restriction_type(), graph_zone());
        break;

      case IrOpcode::kCheckFloat64Hole:
        new_type = Type::Intersect(op_typer_.CheckFloat64Hole(input0_type),
                                   info->restriction_type(), graph_zone());
        break;

      case IrOpcode::kCheckNumber:
        new_type = Type::Intersect(op_typer_.CheckNumber(input0_type),
                                   info->restriction_type(), graph_zone());
        break;

      case IrOpcode::kPhi: {
        new_type = TypePhi(node);
        if (!type.IsInvalid()) {
          new_type = Weaken(node, type, new_type);
        }
        break;
      }

      case IrOpcode::kConvertTaggedHoleToUndefined:
        new_type = op_typer_.ConvertTaggedHoleToUndefined(
            FeedbackTypeOf(node->InputAt(0)));
        break;

      case IrOpcode::kTypeGuard: {
        new_type = op_typer_.TypeTypeGuard(node->op(),
                                           FeedbackTypeOf(node->InputAt(0)));
        break;
      }

      case IrOpcode::kSelect: {
        new_type = TypeSelect(node);
        break;
      }

      default:
        // Shortcut for operations that we do not handle.
        if (type.IsInvalid()) {
          GetInfo(node)->set_feedback_type(NodeProperties::GetType(node));
          return true;
        }
        return false;
    }
    // We need to guarantee that the feedback type is a subtype of the upper
    // bound. Naively that should hold, but weakening can actually produce
    // a bigger type if we are unlucky with ordering of phi typing. To be
    // really sure, just intersect the upper bound with the feedback type.
    new_type = Type::Intersect(GetUpperBound(node), new_type, graph_zone());

    if (!type.IsInvalid() && new_type.Is(type)) return false;
    GetInfo(node)->set_feedback_type(new_type);
    if (FLAG_trace_representation) {
      PrintNodeFeedbackType(node);
    }
    return true;
  }

  void PrintNodeFeedbackType(Node* n) {
    StdoutStream os;
    os << "#" << n->id() << ":" << *n->op() << "(";
    int j = 0;
    for (Node* const i : n->inputs()) {
      if (j++ > 0) os << ", ";
      os << "#" << i->id() << ":" << i->op()->mnemonic();
    }
    os << ")";
    if (NodeProperties::IsTyped(n)) {
      Type static_type = NodeProperties::GetType(n);
      os << "  [Static type: " << static_type;
      Type feedback_type = GetInfo(n)->feedback_type();
      if (!feedback_type.IsInvalid() && feedback_type != static_type) {
        os << ", Feedback type: " << feedback_type;
      }
      os << "]";
    }
    os << std::endl;
  }

  Type Weaken(Node* node, Type previous_type, Type current_type) {
    // If the types have nothing to do with integers, return the types.
    Type const integer = type_cache_->kInteger;
    if (!previous_type.Maybe(integer)) {
      return current_type;
    }
    DCHECK(current_type.Maybe(integer));

    Type current_integer = Type::Intersect(current_type, integer, graph_zone());
    DCHECK(!current_integer.IsNone());
    Type previous_integer =
        Type::Intersect(previous_type, integer, graph_zone());
    DCHECK(!previous_integer.IsNone());

    // Once we start weakening a node, we should always weaken.
    if (!GetInfo(node)->weakened()) {
      // Only weaken if there is range involved; we should converge quickly
      // for all other types (the exception is a union of many constants,
      // but we currently do not increase the number of constants in unions).
      Type previous = previous_integer.GetRange();
      Type current = current_integer.GetRange();
      if (current.IsInvalid() || previous.IsInvalid()) {
        return current_type;
      }
      // Range is involved => we are weakening.
      GetInfo(node)->set_weakened();
    }

    return Type::Union(current_type,
                       op_typer_.WeakenRange(previous_integer, current_integer),
                       graph_zone());
  }

  // Generates a pre-order traversal of the nodes, starting with End.
  void GenerateTraversal() {
    ZoneStack<NodeState> stack(zone_);

    stack.push({graph()->end(), 0});
    GetInfo(graph()->end())->set_pushed();
    while (!stack.empty()) {
      NodeState& current = stack.top();
      Node* node = current.node;

      // If there is an unvisited input, push it and continue with that node.
      bool pushed_unvisited = false;
      while (current.input_index < node->InputCount()) {
        Node* input = node->InputAt(current.input_index);
        NodeInfo* input_info = GetInfo(input);
        current.input_index++;
        if (input_info->unvisited()) {
          input_info->set_pushed();
          stack.push({input, 0});
          pushed_unvisited = true;
          break;
        } else if (input_info->pushed()) {
          // Optimization for the Retype phase.
          // If we had already pushed (and not visited) an input, it means that
          // the current node will be visited in the Retype phase before one of
          // its inputs. If this happens, the current node might need to be
          // revisited.
          MarkAsPossibleRevisit(node, input);
        }
      }

      if (pushed_unvisited) continue;

      stack.pop();
      NodeInfo* info = GetInfo(node);
      info->set_visited();

      // Generate the traversal
      traversal_nodes_.push_back(node);
    }
  }

  void PushNodeToRevisitIfVisited(Node* node) {
    NodeInfo* info = GetInfo(node);
    if (info->visited()) {
      TRACE(" QUEUEING #%d: %s\n", node->id(), node->op()->mnemonic());
      info->set_queued();
      revisit_queue_.push(node);
    }
  }

  // Tries to update the feedback type of the node, as well as setting its
  // machine representation (in VisitNode). Returns true iff updating the
  // feedback type is successful.
  bool RetypeNode(Node* node) {
    NodeInfo* info = GetInfo(node);
    info->set_visited();
    bool updated = UpdateFeedbackType(node);
    TRACE(" visit #%d: %s\n", node->id(), node->op()->mnemonic());
    VisitNode<RETYPE>(node, info->truncation(), nullptr);
    TRACE("  ==> output %s\n", MachineReprToString(info->representation()));
    return updated;
  }

  // Visits the node and marks it as visited. Inside of VisitNode, we might
  // change the truncation of one of our inputs (see EnqueueInput<PROPAGATE> for
  // this). If we change the truncation of an already visited node, we will add
  // it to the revisit queue.
  void PropagateTruncation(Node* node) {
    NodeInfo* info = GetInfo(node);
    info->set_visited();
    TRACE(" visit #%d: %s (trunc: %s)\n", node->id(), node->op()->mnemonic(),
          info->truncation().description());
    VisitNode<PROPAGATE>(node, info->truncation(), nullptr);
  }

  // Backward propagation of truncations to a fixpoint.
  void RunPropagatePhase() {
    TRACE("--{Propagate phase}--\n");
    ResetNodeInfoState();
    DCHECK(revisit_queue_.empty());

    // Process nodes in reverse post order, with End as the root.
    for (auto it = traversal_nodes_.crbegin(); it != traversal_nodes_.crend();
         ++it) {
      PropagateTruncation(*it);

      while (!revisit_queue_.empty()) {
        Node* node = revisit_queue_.front();
        revisit_queue_.pop();
        PropagateTruncation(node);
      }
    }
  }

  // Forward propagation of types from type feedback to a fixpoint.
  void RunRetypePhase() {
    TRACE("--{Retype phase}--\n");
    ResetNodeInfoState();
    DCHECK(revisit_queue_.empty());

    for (auto it = traversal_nodes_.cbegin(); it != traversal_nodes_.cend();
         ++it) {
      Node* node = *it;
      if (!RetypeNode(node)) continue;

      auto revisit_it = might_need_revisit_.find(node);
      if (revisit_it == might_need_revisit_.end()) continue;

      for (Node* const user : revisit_it->second) {
        PushNodeToRevisitIfVisited(user);
      }

      // Process the revisit queue.
      while (!revisit_queue_.empty()) {
        Node* revisit_node = revisit_queue_.front();
        revisit_queue_.pop();
        if (!RetypeNode(revisit_node)) continue;
        // Here we need to check all uses since we can't easily know which
        // nodes will need to be revisited due to having an input which was
        // a revisited node.
        for (Node* const user : revisit_node->uses()) {
          PushNodeToRevisitIfVisited(user);
        }
      }
    }
  }

  // Lowering and change insertion phase.
  void RunLowerPhase(SimplifiedLowering* lowering) {
    TRACE("--{Lower phase}--\n");
    for (auto it = traversal_nodes_.cbegin(); it != traversal_nodes_.cend();
         ++it) {
      Node* node = *it;
      NodeInfo* info = GetInfo(node);
      TRACE(" visit #%d: %s\n", node->id(), node->op()->mnemonic());
      // Reuse {VisitNode()} so the representation rules are in one place.
      SourcePositionTable::Scope scope(
          source_positions_, source_positions_->GetSourcePosition(node));
      NodeOriginTable::Scope origin_scope(node_origins_, "simplified lowering",
                                          node);
      VisitNode<LOWER>(node, info->truncation(), lowering);
    }

    // Perform the final replacements.
    for (NodeVector::iterator i = replacements_.begin();
         i != replacements_.end(); ++i) {
      Node* node = *i;
      Node* replacement = *(++i);
      node->ReplaceUses(replacement);
      node->Kill();
      // We also need to replace the node in the rest of the vector.
      for (NodeVector::iterator j = i + 1; j != replacements_.end(); ++j) {
        ++j;
        if (*j == node) *j = replacement;
      }
    }
  }

  void Run(SimplifiedLowering* lowering) {
    GenerateTraversal();
    RunPropagatePhase();
    RunRetypePhase();
    RunLowerPhase(lowering);
  }

  // Just assert for Retype and Lower. Propagate specialized below.
  template <Phase T>
  void EnqueueInput(Node* use_node, int index,
                    UseInfo use_info = UseInfo::None()) {
    static_assert(retype<T>() || lower<T>(),
                  "This version of ProcessRemainingInputs has to be called in "
                  "the Retype or Lower phase.");
  }

  template <Phase T>
  static constexpr bool propagate() {
    return T == PROPAGATE;
  }

  template <Phase T>
  static constexpr bool retype() {
    return T == RETYPE;
  }

  template <Phase T>
  static constexpr bool lower() {
    return T == LOWER;
  }

  template <Phase T>
  void SetOutput(Node* node, MachineRepresentation representation,
                 Type restriction_type = Type::Any());

  Type GetUpperBound(Node* node) { return NodeProperties::GetType(node); }

  bool InputCannotBe(Node* node, Type type) {
    DCHECK_EQ(1, node->op()->ValueInputCount());
    return !GetUpperBound(node->InputAt(0)).Maybe(type);
  }

  bool InputIs(Node* node, Type type) {
    DCHECK_EQ(1, node->op()->ValueInputCount());
    return GetUpperBound(node->InputAt(0)).Is(type);
  }

  bool BothInputsAreSigned32(Node* node) {
    return BothInputsAre(node, Type::Signed32());
  }

  bool BothInputsAreUnsigned32(Node* node) {
    return BothInputsAre(node, Type::Unsigned32());
  }

  bool BothInputsAre(Node* node, Type type) {
    DCHECK_EQ(2, node->op()->ValueInputCount());
    return GetUpperBound(node->InputAt(0)).Is(type) &&
           GetUpperBound(node->InputAt(1)).Is(type);
  }

  bool IsNodeRepresentationTagged(Node* node) {
    MachineRepresentation representation = GetInfo(node)->representation();
    return IsAnyTagged(representation);
  }

  bool OneInputCannotBe(Node* node, Type type) {
    DCHECK_EQ(2, node->op()->ValueInputCount());
    return !GetUpperBound(node->InputAt(0)).Maybe(type) ||
           !GetUpperBound(node->InputAt(1)).Maybe(type);
  }

  void ChangeToDeadValue(Node* node, Node* effect, Node* control) {
    DCHECK(TypeOf(node).IsNone());
    // If the node is unreachable, insert an Unreachable node and mark the
    // value dead.
    // TODO(jarin,tebbi) Find a way to unify/merge this insertion with
    // InsertUnreachableIfNecessary.
    Node* unreachable = effect =
        graph()->NewNode(jsgraph_->common()->Unreachable(), effect, control);
    const Operator* dead_value =
        jsgraph_->common()->DeadValue(GetInfo(node)->representation());
    node->ReplaceInput(0, unreachable);
    node->TrimInputCount(dead_value->ValueInputCount());
    ReplaceEffectControlUses(node, effect, control);
    NodeProperties::ChangeOp(node, dead_value);
  }

  void ChangeToPureOp(Node* node, const Operator* new_op) {
    DCHECK(new_op->HasProperty(Operator::kPure));
    DCHECK_EQ(new_op->ValueInputCount(), node->op()->ValueInputCount());
    if (node->op()->EffectInputCount() > 0) {
      DCHECK_LT(0, node->op()->ControlInputCount());
      Node* control = NodeProperties::GetControlInput(node);
      Node* effect = NodeProperties::GetEffectInput(node);
      if (TypeOf(node).IsNone()) {
        ChangeToDeadValue(node, effect, control);
        return;
      }
      // Rewire the effect and control chains.
      node->TrimInputCount(new_op->ValueInputCount());
      ReplaceEffectControlUses(node, effect, control);
    } else {
      DCHECK_EQ(0, node->op()->ControlInputCount());
    }
    NodeProperties::ChangeOp(node, new_op);
  }

  void ChangeUnaryToPureBinaryOp(Node* node, const Operator* new_op,
                                 int new_input_index, Node* new_input) {
    DCHECK(new_op->HasProperty(Operator::kPure));
    DCHECK_EQ(new_op->ValueInputCount(), 2);
    DCHECK_EQ(node->op()->ValueInputCount(), 1);
    DCHECK_LE(0, new_input_index);
    DCHECK_LE(new_input_index, 1);
    if (node->op()->EffectInputCount() > 0) {
      DCHECK_LT(0, node->op()->ControlInputCount());
      Node* control = NodeProperties::GetControlInput(node);
      Node* effect = NodeProperties::GetEffectInput(node);
      if (TypeOf(node).IsNone()) {
        ChangeToDeadValue(node, effect, control);
        return;
      }
      node->TrimInputCount(node->op()->ValueInputCount());
      ReplaceEffectControlUses(node, effect, control);
    } else {
      DCHECK_EQ(0, node->op()->ControlInputCount());
    }
    node->InsertInput(jsgraph_->zone(), new_input_index, new_input);
    NodeProperties::ChangeOp(node, new_op);
  }

  // Converts input {index} of {node} according to given UseInfo {use},
  // assuming the type of the input is {input_type}. If {input_type} is null,
  // it takes the input from the input node {TypeOf(node->InputAt(index))}.
  void ConvertInput(Node* node, int index, UseInfo use,
                    Type input_type = Type::Invalid()) {
    // In the change phase, insert a change before the use if necessary.
    if (use.representation() == MachineRepresentation::kNone)
      return;  // No input requirement on the use.
    Node* input = node->InputAt(index);
    DCHECK_NOT_NULL(input);
    NodeInfo* input_info = GetInfo(input);
    MachineRepresentation input_rep = input_info->representation();
    if (input_rep != use.representation() ||
        use.type_check() != TypeCheckKind::kNone) {
      // Output representation doesn't match usage.
      TRACE("  change: #%d:%s(@%d #%d:%s) ", node->id(), node->op()->mnemonic(),
            index, input->id(), input->op()->mnemonic());
      TRACE("from %s to %s:%s\n",
            MachineReprToString(input_info->representation()),
            MachineReprToString(use.representation()),
            use.truncation().description());
      if (input_type.IsInvalid()) {
        input_type = TypeOf(input);
      }
      Node* n = changer_->GetRepresentationFor(input, input_rep, input_type,
                                               node, use);
      node->ReplaceInput(index, n);
    }
  }

  template <Phase T>
  void ProcessInput(Node* node, int index, UseInfo use);

  // Just assert for Retype and Lower. Propagate specialized below.
  template <Phase T>
  void ProcessRemainingInputs(Node* node, int index) {
    static_assert(retype<T>() || lower<T>(),
                  "This version of ProcessRemainingInputs has to be called in "
                  "the Retype or Lower phase.");
    DCHECK_GE(index, NodeProperties::PastValueIndex(node));
    DCHECK_GE(index, NodeProperties::PastContextIndex(node));
  }

  // Marks node as a possible revisit since it is a use of input that will be
  // visited before input is visited.
  void MarkAsPossibleRevisit(Node* node, Node* input) {
    auto it = might_need_revisit_.find(input);
    if (it == might_need_revisit_.end()) {
      it = might_need_revisit_.insert({input, ZoneVector<Node*>(zone())}).first;
    }
    it->second.push_back(node);
    TRACE(" Marking #%d: %s as needing revisit due to #%d: %s\n", node->id(),
          node->op()->mnemonic(), input->id(), input->op()->mnemonic());
  }

  // Just assert for Retype. Propagate and Lower specialized below.
  template <Phase T>
  void VisitInputs(Node* node) {
    static_assert(
        retype<T>(),
        "This version of VisitInputs has to be called in the Retype phase.");
  }

  template <Phase T>
  void VisitReturn(Node* node) {
    int first_effect_index = NodeProperties::FirstEffectIndex(node);
    // Visit integer slot count to pop
    ProcessInput<T>(node, 0, UseInfo::TruncatingWord32());

    // Visit value, context and frame state inputs as tagged.
    for (int i = 1; i < first_effect_index; i++) {
      ProcessInput<T>(node, i, UseInfo::AnyTagged());
    }
    // Only enqueue other inputs (effects, control).
    for (int i = first_effect_index; i < node->InputCount(); i++) {
      EnqueueInput<T>(node, i);
    }
  }

  // Helper for an unused node.
  template <Phase T>
  void VisitUnused(Node* node) {
    int first_effect_index = NodeProperties::FirstEffectIndex(node);
    for (int i = 0; i < first_effect_index; i++) {
      ProcessInput<T>(node, i, UseInfo::None());
    }
    ProcessRemainingInputs<T>(node, first_effect_index);
    if (lower<T>()) Kill(node);
  }

  // Helper for no-op node.
  template <Phase T>
  void VisitNoop(Node* node, Truncation truncation) {
    if (truncation.IsUnused()) return VisitUnused<T>(node);
    MachineRepresentation representation =
        GetOutputInfoForPhi(node, TypeOf(node), truncation);
    VisitUnop<T>(node, UseInfo(representation, truncation), representation);
    if (lower<T>()) DeferReplacement(node, node->InputAt(0));
  }

  // Helper for binops of the R x L -> O variety.
  template <Phase T>
  void VisitBinop(Node* node, UseInfo left_use, UseInfo right_use,
                  MachineRepresentation output,
                  Type restriction_type = Type::Any()) {
    DCHECK_EQ(2, node->op()->ValueInputCount());
    ProcessInput<T>(node, 0, left_use);
    ProcessInput<T>(node, 1, right_use);
    for (int i = 2; i < node->InputCount(); i++) {
      EnqueueInput<T>(node, i);
    }
    SetOutput<T>(node, output, restriction_type);
  }

  // Helper for binops of the I x I -> O variety.
  template <Phase T>
  void VisitBinop(Node* node, UseInfo input_use, MachineRepresentation output,
                  Type restriction_type = Type::Any()) {
    VisitBinop<T>(node, input_use, input_use, output, restriction_type);
  }

  template <Phase T>
  void VisitSpeculativeInt32Binop(Node* node) {
    DCHECK_EQ(2, node->op()->ValueInputCount());
    if (BothInputsAre(node, Type::NumberOrOddball())) {
      return VisitBinop<T>(node, UseInfo::TruncatingWord32(),
                           MachineRepresentation::kWord32);
    }
    NumberOperationHint hint = NumberOperationHintOf(node->op());
    return VisitBinop<T>(node, CheckedUseInfoAsWord32FromHint(hint),
                         MachineRepresentation::kWord32);
  }

  // Helper for unops of the I -> O variety.
  template <Phase T>
  void VisitUnop(Node* node, UseInfo input_use, MachineRepresentation output,
                 Type restriction_type = Type::Any()) {
    DCHECK_EQ(1, node->op()->ValueInputCount());
    ProcessInput<T>(node, 0, input_use);
    ProcessRemainingInputs<T>(node, 1);
    SetOutput<T>(node, output, restriction_type);
  }

  // Helper for leaf nodes.
  template <Phase T>
  void VisitLeaf(Node* node, MachineRepresentation output) {
    DCHECK_EQ(0, node->InputCount());
    SetOutput<T>(node, output);
  }

  // Helpers for specific types of binops.

  template <Phase T>
  void VisitFloat64Binop(Node* node) {
    VisitBinop<T>(node, UseInfo::TruncatingFloat64(),
                  MachineRepresentation::kFloat64);
  }

  template <Phase T>
  void VisitInt64Binop(Node* node) {
    VisitBinop<T>(node, UseInfo::Word64(), MachineRepresentation::kWord64);
  }

  template <Phase T>
  void VisitWord32TruncatingBinop(Node* node) {
    VisitBinop<T>(node, UseInfo::TruncatingWord32(),
                  MachineRepresentation::kWord32);
  }

  // Infer representation for phi-like nodes.
  // The {node} parameter is only used to decide on the int64 representation.
  // Once the type system supports an external pointer type, the {node}
  // parameter can be removed.
  MachineRepresentation GetOutputInfoForPhi(Node* node, Type type,
                                            Truncation use) {
    // Compute the representation.
    if (type.Is(Type::None())) {
      return MachineRepresentation::kNone;
    } else if (type.Is(Type::Signed32()) || type.Is(Type::Unsigned32())) {
      return MachineRepresentation::kWord32;
    } else if (type.Is(Type::NumberOrOddball()) && use.IsUsedAsWord32()) {
      return MachineRepresentation::kWord32;
    } else if (type.Is(Type::Boolean())) {
      return MachineRepresentation::kBit;
    } else if (type.Is(Type::NumberOrOddball()) &&
               use.TruncatesOddballAndBigIntToNumber()) {
      return MachineRepresentation::kFloat64;
    } else if (type.Is(Type::Union(Type::SignedSmall(), Type::NaN(), zone()))) {
      // TODO(turbofan): For Phis that return either NaN or some Smi, it's
      // beneficial to not go all the way to double, unless the uses are
      // double uses. For tagging that just means some potentially expensive
      // allocation code; we might want to do the same for -0 as well?
      return MachineRepresentation::kTagged;
    } else if (type.Is(Type::Number())) {
      return MachineRepresentation::kFloat64;
    } else if (type.Is(Type::BigInt()) && use.IsUsedAsWord64()) {
      return MachineRepresentation::kWord64;
    } else if (type.Is(Type::ExternalPointer()) ||
               type.Is(Type::SandboxedExternalPointer())) {
      return MachineType::PointerRepresentation();
    }
    return MachineRepresentation::kTagged;
  }

  // Helper for handling selects.
  template <Phase T>
  void VisitSelect(Node* node, Truncation truncation,
                   SimplifiedLowering* lowering) {
    DCHECK(TypeOf(node->InputAt(0)).Is(Type::Boolean()));
    ProcessInput<T>(node, 0, UseInfo::Bool());

    MachineRepresentation output =
        GetOutputInfoForPhi(node, TypeOf(node), truncation);
    SetOutput<T>(node, output);

    if (lower<T>()) {
      // Update the select operator.
      SelectParameters p = SelectParametersOf(node->op());
      if (output != p.representation()) {
        NodeProperties::ChangeOp(node,
                                 lowering->common()->Select(output, p.hint()));
      }
    }
    // Convert inputs to the output representation of this phi, pass the
    // truncation truncation along.
    UseInfo input_use(output, truncation);
    ProcessInput<T>(node, 1, input_use);
    ProcessInput<T>(node, 2, input_use);
  }

  // Helper for handling phis.
  template <Phase T>
  void VisitPhi(Node* node, Truncation truncation,
                SimplifiedLowering* lowering) {
    MachineRepresentation output =
        GetOutputInfoForPhi(node, TypeOf(node), truncation);
    // Only set the output representation if not running with type
    // feedback. (Feedback typing will set the representation.)
    SetOutput<T>(node, output);

    int values = node->op()->ValueInputCount();
    if (lower<T>()) {
      // Update the phi operator.
      if (output != PhiRepresentationOf(node->op())) {
        NodeProperties::ChangeOp(node, lowering->common()->Phi(output, values));
      }
    }

    // Convert inputs to the output representation of this phi, pass the
    // truncation along.
    UseInfo input_use(output, truncation);
    for (int i = 0; i < node->InputCount(); i++) {
      ProcessInput<T>(node, i, i < values ? input_use : UseInfo::None());
    }
  }

  template <Phase T>
  void VisitObjectIs(Node* node, Type type, SimplifiedLowering* lowering) {
    Type const input_type = TypeOf(node->InputAt(0));
    if (input_type.Is(type)) {
      VisitUnop<T>(node, UseInfo::None(), MachineRepresentation::kBit);
      if (lower<T>()) {
        DeferReplacement(node, lowering->jsgraph()->Int32Constant(1));
      }
    } else {
      VisitUnop<T>(node, UseInfo::AnyTagged(), MachineRepresentation::kBit);
      if (lower<T>() && !input_type.Maybe(type)) {
        DeferReplacement(node, lowering->jsgraph()->Int32Constant(0));
      }
    }
  }

  template <Phase T>
  void VisitCheck(Node* node, Type type, SimplifiedLowering* lowering) {
    if (InputIs(node, type)) {
      VisitUnop<T>(node, UseInfo::AnyTagged(),
                   MachineRepresentation::kTaggedPointer);
      if (lower<T>()) DeferReplacement(node, node->InputAt(0));
    } else {
      VisitUnop<T>(node,
                   UseInfo::CheckedHeapObjectAsTaggedPointer(FeedbackSource()),
                   MachineRepresentation::kTaggedPointer);
    }
  }

  template <Phase T>
  void VisitCall(Node* node, SimplifiedLowering* lowering) {
    auto call_descriptor = CallDescriptorOf(node->op());
    int params = static_cast<int>(call_descriptor->ParameterCount());
    int value_input_count = node->op()->ValueInputCount();

    DCHECK_GT(value_input_count, 0);
    DCHECK_GE(value_input_count, params);

    // The target of the call.
    ProcessInput<T>(node, 0, UseInfo::Any());

    // For the parameters (indexes [1, ..., params]), propagate representation
    // information from call descriptor.
    for (int i = 1; i <= params; i++) {
      ProcessInput<T>(node, i,
                      TruncatingUseInfoFromRepresentation(
                          call_descriptor->GetInputType(i).representation()));
    }

    // Rest of the value inputs.
    for (int i = params + 1; i < value_input_count; i++) {
      ProcessInput<T>(node, i, UseInfo::AnyTagged());
    }

    // Effect and Control.
    ProcessRemainingInputs<T>(node, value_input_count);

    if (call_descriptor->ReturnCount() > 0) {
      SetOutput<T>(node, call_descriptor->GetReturnType(0).representation());
    } else {
      SetOutput<T>(node, MachineRepresentation::kTagged);
    }
  }

  void MaskShiftOperand(Node* node, Type rhs_type) {
    if (!rhs_type.Is(type_cache_->kZeroToThirtyOne)) {
      Node* const rhs = NodeProperties::GetValueInput(node, 1);
      node->ReplaceInput(1,
                         graph()->NewNode(jsgraph_->machine()->Word32And(), rhs,
                                          jsgraph_->Int32Constant(0x1F)));
    }
  }

  static MachineSemantic DeoptValueSemanticOf(Type type) {
    // We only need signedness to do deopt correctly.
    if (type.Is(Type::Signed32())) {
      return MachineSemantic::kInt32;
    } else if (type.Is(Type::Unsigned32())) {
      return MachineSemantic::kUint32;
    } else {
      return MachineSemantic::kAny;
    }
  }

  static MachineType DeoptMachineTypeOf(MachineRepresentation rep, Type type) {
    if (type.IsNone()) {
      return MachineType::None();
    }
    // Do not distinguish between various Tagged variations.
    if (IsAnyTagged(rep)) {
      return MachineType::AnyTagged();
    }
    if (rep == MachineRepresentation::kWord64) {
      if (type.Is(Type::BigInt())) {
        return MachineType::AnyTagged();
      }

      DCHECK(type.Is(TypeCache::Get()->kSafeInteger));
      return MachineType(rep, MachineSemantic::kInt64);
    }
    MachineType machine_type(rep, DeoptValueSemanticOf(type));
    DCHECK(machine_type.representation() != MachineRepresentation::kWord32 ||
           machine_type.semantic() == MachineSemantic::kInt32 ||
           machine_type.semantic() == MachineSemantic::kUint32);
    DCHECK(machine_type.representation() != MachineRepresentation::kBit ||
           type.Is(Type::Boolean()));
    return machine_type;
  }

  template <Phase T>
  void VisitStateValues(Node* node) {
    if (propagate<T>()) {
      for (int i = 0; i < node->InputCount(); i++) {
        // When lowering 64 bit BigInts to Word64 representation, we have to
        // make sure they are rematerialized before deoptimization. By
        // propagating a AnyTagged use, the RepresentationChanger is going to
        // insert the necessary conversions.
        // TODO(nicohartmann): Remove, once the deoptimizer can rematerialize
        // truncated BigInts.
        if (TypeOf(node->InputAt(i)).Is(Type::BigInt())) {
          EnqueueInput<T>(node, i, UseInfo::AnyTagged());
        } else {
          EnqueueInput<T>(node, i, UseInfo::Any());
        }
      }
    } else if (lower<T>()) {
      Zone* zone = jsgraph_->zone();
      ZoneVector<MachineType>* types =
          zone->New<ZoneVector<MachineType>>(node->InputCount(), zone);
      for (int i = 0; i < node->InputCount(); i++) {
        Node* input = node->InputAt(i);
        // TODO(nicohartmann): Remove, once the deoptimizer can rematerialize
        // truncated BigInts.
        if (TypeOf(input).Is(Type::BigInt())) {
          ConvertInput(node, i, UseInfo::AnyTagged());
        }

        (*types)[i] =
            DeoptMachineTypeOf(GetInfo(input)->representation(), TypeOf(input));
      }
      SparseInputMask mask = SparseInputMaskOf(node->op());
      NodeProperties::ChangeOp(
          node, jsgraph_->common()->TypedStateValues(types, mask));
    }
    SetOutput<T>(node, MachineRepresentation::kTagged);
  }

  template <Phase T>
  void VisitFrameState(Node* node) {
    DCHECK_EQ(5, node->op()->ValueInputCount());
    DCHECK_EQ(1, OperatorProperties::GetFrameStateInputCount(node->op()));

    ProcessInput<T>(node, 0, UseInfo::AnyTagged());  // Parameters.
    ProcessInput<T>(node, 1, UseInfo::AnyTagged());  // Registers.

    // Accumulator is a special flower - we need to remember its type in
    // a singleton typed-state-values node (as if it was a singleton
    // state-values node).
    Node* accumulator = node->InputAt(2);
    if (propagate<T>()) {
      // TODO(nicohartmann): Remove, once the deoptimizer can rematerialize
      // truncated BigInts.
      if (TypeOf(accumulator).Is(Type::BigInt())) {
        EnqueueInput<T>(node, 2, UseInfo::AnyTagged());
      } else {
        EnqueueInput<T>(node, 2, UseInfo::Any());
      }
    } else if (lower<T>()) {
      // TODO(nicohartmann): Remove, once the deoptimizer can rematerialize
      // truncated BigInts.
      if (TypeOf(accumulator).Is(Type::BigInt())) {
        ConvertInput(node, 2, UseInfo::AnyTagged());
      }
      Zone* zone = jsgraph_->zone();
      if (accumulator == jsgraph_->OptimizedOutConstant()) {
        node->ReplaceInput(2, jsgraph_->SingleDeadTypedStateValues());
      } else {
        ZoneVector<MachineType>* types =
            zone->New<ZoneVector<MachineType>>(1, zone);
        (*types)[0] = DeoptMachineTypeOf(GetInfo(accumulator)->representation(),
                                         TypeOf(accumulator));

        node->ReplaceInput(
            2, jsgraph_->graph()->NewNode(jsgraph_->common()->TypedStateValues(
                                              types, SparseInputMask::Dense()),
                                          node->InputAt(2)));
      }
    }

    ProcessInput<T>(node, 3, UseInfo::AnyTagged());  // Context.
    ProcessInput<T>(node, 4, UseInfo::AnyTagged());  // Closure.
    ProcessInput<T>(node, 5, UseInfo::AnyTagged());  // Outer frame state.
    return SetOutput<T>(node, MachineRepresentation::kTagged);
  }

  template <Phase T>
  void VisitObjectState(Node* node) {
    if (propagate<T>()) {
      for (int i = 0; i < node->InputCount(); i++) {
        // TODO(nicohartmann): Remove, once the deoptimizer can rematerialize
        // truncated BigInts.
        if (TypeOf(node->InputAt(i)).Is(Type::BigInt())) {
          EnqueueInput<T>(node, i, UseInfo::AnyTagged());
        } else {
          EnqueueInput<T>(node, i, UseInfo::Any());
        }
      }
    } else if (lower<T>()) {
      Zone* zone = jsgraph_->zone();
      ZoneVector<MachineType>* types =
          zone->New<ZoneVector<MachineType>>(node->InputCount(), zone);
      for (int i = 0; i < node->InputCount(); i++) {
        Node* input = node->InputAt(i);
        (*types)[i] =
            DeoptMachineTypeOf(GetInfo(input)->representation(), TypeOf(input));
        // TODO(nicohartmann): Remove, once the deoptimizer can rematerialize
        // truncated BigInts.
        if (TypeOf(node->InputAt(i)).Is(Type::BigInt())) {
          ConvertInput(node, i, UseInfo::AnyTagged());
        }
      }
      NodeProperties::ChangeOp(node, jsgraph_->common()->TypedObjectState(
                                         ObjectIdOf(node->op()), types));
    }
    SetOutput<T>(node, MachineRepresentation::kTagged);
  }

  const Operator* Int32Op(Node* node) {
    return changer_->Int32OperatorFor(node->opcode());
  }

  const Operator* Int32OverflowOp(Node* node) {
    return changer_->Int32OverflowOperatorFor(node->opcode());
  }

  const Operator* Int64Op(Node* node) {
    return changer_->Int64OperatorFor(node->opcode());
  }

  const Operator* Uint32Op(Node* node) {
    return changer_->Uint32OperatorFor(node->opcode());
  }

  const Operator* Uint32OverflowOp(Node* node) {
    return changer_->Uint32OverflowOperatorFor(node->opcode());
  }

  const Operator* Float64Op(Node* node) {
    return changer_->Float64OperatorFor(node->opcode());
  }

  WriteBarrierKind WriteBarrierKindFor(
      BaseTaggedness base_taggedness,
      MachineRepresentation field_representation, Type field_type,
      MachineRepresentation value_representation, Node* value) {
    if (base_taggedness == kTaggedBase &&
        CanBeTaggedPointer(field_representation)) {
      Type value_type = NodeProperties::GetType(value);
      if (value_representation == MachineRepresentation::kTaggedSigned) {
        // Write barriers are only for stores of heap objects.
        return kNoWriteBarrier;
      }
      if (field_type.Is(Type::BooleanOrNullOrUndefined()) ||
          value_type.Is(Type::BooleanOrNullOrUndefined())) {
        // Write barriers are not necessary when storing true, false, null or
        // undefined, because these special oddballs are always in the root set.
        return kNoWriteBarrier;
      }
      if (value_type.IsHeapConstant()) {
        RootIndex root_index;
        const RootsTable& roots_table = jsgraph_->isolate()->roots_table();
        if (roots_table.IsRootHandle(value_type.AsHeapConstant()->Value(),
                                     &root_index)) {
          if (RootsTable::IsImmortalImmovable(root_index)) {
            // Write barriers are unnecessary for immortal immovable roots.
            return kNoWriteBarrier;
          }
        }
      }
      if (field_representation == MachineRepresentation::kTaggedPointer ||
          value_representation == MachineRepresentation::kTaggedPointer) {
        // Write barriers for heap objects are cheaper.
        return kPointerWriteBarrier;
      }
      NumberMatcher m(value);
      if (m.HasValue()) {
        if (IsSmiDouble(m.Value())) {
          // Storing a smi doesn't need a write barrier.
          return kNoWriteBarrier;
        }
        // The NumberConstant will be represented as HeapNumber.
        return kPointerWriteBarrier;
      }
      return kFullWriteBarrier;
    }
    return kNoWriteBarrier;
  }

  WriteBarrierKind WriteBarrierKindFor(
      BaseTaggedness base_taggedness,
      MachineRepresentation field_representation, int field_offset,
      Type field_type, MachineRepresentation value_representation,
      Node* value) {
    WriteBarrierKind write_barrier_kind =
        WriteBarrierKindFor(base_taggedness, field_representation, field_type,
                            value_representation, value);
    if (write_barrier_kind != kNoWriteBarrier) {
      if (base_taggedness == kTaggedBase &&
          field_offset == HeapObject::kMapOffset) {
        write_barrier_kind = kMapWriteBarrier;
      }
    }
    return write_barrier_kind;
  }

  Graph* graph() const { return jsgraph_->graph(); }
  CommonOperatorBuilder* common() const { return jsgraph_->common(); }
  SimplifiedOperatorBuilder* simplified() const {
    return jsgraph_->simplified();
  }

  void LowerToCheckedInt32Mul(Node* node, Truncation truncation,
                              Type input0_type, Type input1_type) {
    // If one of the inputs is positive and/or truncation is being applied,
    // there is no need to return -0.
    CheckForMinusZeroMode mz_mode =
        truncation.IdentifiesZeroAndMinusZero() ||
                IsSomePositiveOrderedNumber(input0_type) ||
                IsSomePositiveOrderedNumber(input1_type)
            ? CheckForMinusZeroMode::kDontCheckForMinusZero
            : CheckForMinusZeroMode::kCheckForMinusZero;

    NodeProperties::ChangeOp(node, simplified()->CheckedInt32Mul(mz_mode));
  }

  void ChangeToInt32OverflowOp(Node* node) {
    NodeProperties::ChangeOp(node, Int32OverflowOp(node));
  }

  void ChangeToUint32OverflowOp(Node* node) {
    NodeProperties::ChangeOp(node, Uint32OverflowOp(node));
  }

  template <Phase T>
  void VisitSpeculativeIntegerAdditiveOp(Node* node, Truncation truncation,
                                         SimplifiedLowering* lowering) {
    Type left_upper = GetUpperBound(node->InputAt(0));
    Type right_upper = GetUpperBound(node->InputAt(1));

    if (left_upper.Is(type_cache_->kAdditiveSafeIntegerOrMinusZero) &&
        right_upper.Is(type_cache_->kAdditiveSafeIntegerOrMinusZero)) {
      // Only eliminate the node if its typing rule can be satisfied, namely
      // that a safe integer is produced.
      if (truncation.IsUnused()) return VisitUnused<T>(node);

      // If we know how to interpret the result or if the users only care
      // about the low 32-bits, we can truncate to Word32 do a wrapping
      // addition.
      if (GetUpperBound(node).Is(Type::Signed32()) ||
          GetUpperBound(node).Is(Type::Unsigned32()) ||
          truncation.IsUsedAsWord32()) {
        // => Int32Add/Sub
        VisitWord32TruncatingBinop<T>(node);
        if (lower<T>()) ChangeToPureOp(node, Int32Op(node));
        return;
      }
    }

    // Try to use type feedback.
    NumberOperationHint hint = NumberOperationHintOf(node->op());
    DCHECK(hint == NumberOperationHint::kSignedSmall ||
           hint == NumberOperationHint::kSigned32);

    Type left_feedback_type = TypeOf(node->InputAt(0));
    Type right_feedback_type = TypeOf(node->InputAt(1));
    // Handle the case when no int32 checks on inputs are necessary (but
    // an overflow check is needed on the output). Note that we do not
    // have to do any check if at most one side can be minus zero. For
    // subtraction we need to handle the case of -0 - 0 properly, since
    // that can produce -0.
    Type left_constraint_type =
        node->opcode() == IrOpcode::kSpeculativeSafeIntegerAdd
            ? Type::Signed32OrMinusZero()
            : Type::Signed32();
    if (left_upper.Is(left_constraint_type) &&
        right_upper.Is(Type::Signed32OrMinusZero()) &&
        (left_upper.Is(Type::Signed32()) || right_upper.Is(Type::Signed32()))) {
      VisitBinop<T>(node, UseInfo::TruncatingWord32(),
                    MachineRepresentation::kWord32, Type::Signed32());
    } else {
      // If the output's truncation is identify-zeros, we can pass it
      // along. Moreover, if the operation is addition and we know the
      // right-hand side is not minus zero, we do not have to distinguish
      // between 0 and -0.
      IdentifyZeros left_identify_zeros = truncation.identify_zeros();
      if (node->opcode() == IrOpcode::kSpeculativeSafeIntegerAdd &&
          !right_feedback_type.Maybe(Type::MinusZero())) {
        left_identify_zeros = kIdentifyZeros;
      }
      UseInfo left_use = CheckedUseInfoAsWord32FromHint(hint, FeedbackSource(),
                                                        left_identify_zeros);
      // For CheckedInt32Add and CheckedInt32Sub, we don't need to do
      // a minus zero check for the right hand side, since we already
      // know that the left hand side is a proper Signed32 value,
      // potentially guarded by a check.
      UseInfo right_use = CheckedUseInfoAsWord32FromHint(hint, FeedbackSource(),
                                                         kIdentifyZeros);
      VisitBinop<T>(node, left_use, right_use, MachineRepresentation::kWord32,
                    Type::Signed32());
    }
    if (lower<T>()) {
      if (truncation.IsUsedAsWord32() ||
          !CanOverflowSigned32(node->op(), left_feedback_type,
                               right_feedback_type, graph_zone())) {
        ChangeToPureOp(node, Int32Op(node));

      } else {
        ChangeToInt32OverflowOp(node);
      }
    }
    return;
  }

  template <Phase T>
  void VisitSpeculativeAdditiveOp(Node* node, Truncation truncation,
                                  SimplifiedLowering* lowering) {
    if (BothInputsAre(node, type_cache_->kAdditiveSafeIntegerOrMinusZero) &&
        (GetUpperBound(node).Is(Type::Signed32()) ||
         GetUpperBound(node).Is(Type::Unsigned32()) ||
         truncation.IsUsedAsWord32())) {
      // => Int32Add/Sub
      VisitWord32TruncatingBinop<T>(node);
      if (lower<T>()) ChangeToPureOp(node, Int32Op(node));
      return;
    }

    // default case => Float64Add/Sub
    VisitBinop<T>(node,
                  UseInfo::CheckedNumberOrOddballAsFloat64(kDistinguishZeros,
                                                           FeedbackSource()),
                  MachineRepresentation::kFloat64, Type::Number());
    if (lower<T>()) {
      ChangeToPureOp(node, Float64Op(node));
    }
    return;
  }

  template <Phase T>
  void VisitSpeculativeNumberModulus(Node* node, Truncation truncation,
                                     SimplifiedLowering* lowering) {
    if (BothInputsAre(node, Type::Unsigned32OrMinusZeroOrNaN()) &&
        (truncation.IsUsedAsWord32() ||
         NodeProperties::GetType(node).Is(Type::Unsigned32()))) {
      // => unsigned Uint32Mod
      VisitWord32TruncatingBinop<T>(node);
      if (lower<T>()) DeferReplacement(node, lowering->Uint32Mod(node));
      return;
    }
    if (BothInputsAre(node, Type::Signed32OrMinusZeroOrNaN()) &&
        (truncation.IsUsedAsWord32() ||
         NodeProperties::GetType(node).Is(Type::Signed32()))) {
      // => signed Int32Mod
      VisitWord32TruncatingBinop<T>(node);
      if (lower<T>()) DeferReplacement(node, lowering->Int32Mod(node));
      return;
    }

    // Try to use type feedback.
    NumberOperationHint hint = NumberOperationHintOf(node->op());

    // Handle the case when no uint32 checks on inputs are necessary
    // (but an overflow check is needed on the output).
    if (BothInputsAreUnsigned32(node)) {
      if (hint == NumberOperationHint::kSignedSmall ||
          hint == NumberOperationHint::kSigned32) {
        VisitBinop<T>(node, UseInfo::TruncatingWord32(),
                      MachineRepresentation::kWord32, Type::Unsigned32());
        if (lower<T>()) ChangeToUint32OverflowOp(node);
        return;
      }
    }

    // Handle the case when no int32 checks on inputs are necessary
    // (but an overflow check is needed on the output).
    if (BothInputsAre(node, Type::Signed32())) {
      // If both the inputs the feedback are int32, use the overflow op.
      if (hint == NumberOperationHint::kSignedSmall ||
          hint == NumberOperationHint::kSigned32) {
        VisitBinop<T>(node, UseInfo::TruncatingWord32(),
                      MachineRepresentation::kWord32, Type::Signed32());
        if (lower<T>()) ChangeToInt32OverflowOp(node);
        return;
      }
    }

    if (hint == NumberOperationHint::kSignedSmall ||
        hint == NumberOperationHint::kSigned32) {
      // If the result is truncated, we only need to check the inputs.
      // For the left hand side we just propagate the identify zeros
      // mode of the {truncation}; and for modulus the sign of the
      // right hand side doesn't matter anyways, so in particular there's
      // no observable difference between a 0 and a -0 then.
      UseInfo const lhs_use = CheckedUseInfoAsWord32FromHint(
          hint, FeedbackSource(), truncation.identify_zeros());
      UseInfo const rhs_use = CheckedUseInfoAsWord32FromHint(
          hint, FeedbackSource(), kIdentifyZeros);
      if (truncation.IsUsedAsWord32()) {
        VisitBinop<T>(node, lhs_use, rhs_use, MachineRepresentation::kWord32);
        if (lower<T>()) DeferReplacement(node, lowering->Int32Mod(node));
      } else if (BothInputsAre(node, Type::Unsigned32OrMinusZeroOrNaN())) {
        VisitBinop<T>(node, lhs_use, rhs_use, MachineRepresentation::kWord32,
                      Type::Unsigned32());
        if (lower<T>()) ChangeToUint32OverflowOp(node);
      } else {
        VisitBinop<T>(node, lhs_use, rhs_use, MachineRepresentation::kWord32,
                      Type::Signed32());
        if (lower<T>()) ChangeToInt32OverflowOp(node);
      }
      return;
    }

    if (TypeOf(node->InputAt(0)).Is(Type::Unsigned32()) &&
        TypeOf(node->InputAt(1)).Is(Type::Unsigned32()) &&
        (truncation.IsUsedAsWord32() ||
         NodeProperties::GetType(node).Is(Type::Unsigned32()))) {
      VisitBinop<T>(node, UseInfo::TruncatingWord32(),
                    MachineRepresentation::kWord32, Type::Number());
      if (lower<T>()) DeferReplacement(node, lowering->Uint32Mod(node));
      return;
    }
    if (TypeOf(node->InputAt(0)).Is(Type::Signed32()) &&
        TypeOf(node->InputAt(1)).Is(Type::Signed32()) &&
        (truncation.IsUsedAsWord32() ||
         NodeProperties::GetType(node).Is(Type::Signed32()))) {
      VisitBinop<T>(node, UseInfo::TruncatingWord32(),
                    MachineRepresentation::kWord32, Type::Number());
      if (lower<T>()) DeferReplacement(node, lowering->Int32Mod(node));
      return;
    }

    // default case => Float64Mod
    // For the left hand side we just propagate the identify zeros
    // mode of the {truncation}; and for modulus the sign of the
    // right hand side doesn't matter anyways, so in particular there's
    // no observable difference between a 0 and a -0 then.
    UseInfo const lhs_use = UseInfo::CheckedNumberOrOddballAsFloat64(
        truncation.identify_zeros(), FeedbackSource());
    UseInfo const rhs_use = UseInfo::CheckedNumberOrOddballAsFloat64(
        kIdentifyZeros, FeedbackSource());
    VisitBinop<T>(node, lhs_use, rhs_use, MachineRepresentation::kFloat64,
                  Type::Number());
    if (lower<T>()) ChangeToPureOp(node, Float64Op(node));
    return;
  }

  // Just assert for Propagate and Retype. Lower specialized below.
  template <Phase T>
  void InsertUnreachableIfNecessary(Node* node) {
    static_assert(propagate<T>() || retype<T>(),
                  "This version of InsertUnreachableIfNecessary has to be "
                  "called in the Propagate or Retype phase.");
  }

  template <Phase T>
  void VisitCheckBounds(Node* node, SimplifiedLowering* lowering) {
    CheckBoundsParameters const& p = CheckBoundsParametersOf(node->op());
    FeedbackSource const& feedback = p.check_parameters().feedback();
    Type const index_type = TypeOf(node->InputAt(0));
    Type const length_type = TypeOf(node->InputAt(1));

    // Conversions, if requested and needed, will be handled by the
    // representation changer, not by the lower-level Checked*Bounds operators.
    CheckBoundsFlags new_flags =
        p.flags().without(CheckBoundsFlag::kConvertStringAndMinusZero);

    if (length_type.Is(Type::Unsigned31())) {
      if (index_type.Is(Type::Integral32()) ||
          (index_type.Is(Type::Integral32OrMinusZero()) &&
           p.flags() & CheckBoundsFlag::kConvertStringAndMinusZero)) {
        // Map the values in the [-2^31,-1] range to the [2^31,2^32-1] range,
        // which will be considered out-of-bounds because the {length_type} is
        // limited to Unsigned31. This also converts -0 to 0.
        VisitBinop<T>(node, UseInfo::TruncatingWord32(),
                      MachineRepresentation::kWord32);
        if (lower<T>()) {
          if (lowering->poisoning_level_ ==
                  PoisoningMitigationLevel::kDontPoison &&
              (index_type.IsNone() || length_type.IsNone() ||
               (index_type.Min() >= 0.0 &&
                index_type.Max() < length_type.Min()))) {
            // The bounds check is redundant if we already know that
            // the index is within the bounds of [0.0, length[.
            // TODO(neis): Move this into TypedOptimization?
            new_flags |= CheckBoundsFlag::kAbortOnOutOfBounds;
          }
          NodeProperties::ChangeOp(
              node, simplified()->CheckedUint32Bounds(feedback, new_flags));
        }
      } else if (p.flags() & CheckBoundsFlag::kConvertStringAndMinusZero) {
        VisitBinop<T>(node, UseInfo::CheckedTaggedAsArrayIndex(feedback),
                      UseInfo::Word(), MachineType::PointerRepresentation());
        if (lower<T>()) {
          if (jsgraph_->machine()->Is64()) {
            NodeProperties::ChangeOp(
                node, simplified()->CheckedUint64Bounds(feedback, new_flags));
          } else {
            NodeProperties::ChangeOp(
                node, simplified()->CheckedUint32Bounds(feedback, new_flags));
          }
        }
      } else {
        VisitBinop<T>(
            node, UseInfo::CheckedSigned32AsWord32(kDistinguishZeros, feedback),
            UseInfo::TruncatingWord32(), MachineRepresentation::kWord32);
        if (lower<T>()) {
          NodeProperties::ChangeOp(
              node, simplified()->CheckedUint32Bounds(feedback, new_flags));
        }
      }
    } else {
      CHECK(length_type.Is(type_cache_->kPositiveSafeInteger));
      IdentifyZeros zero_handling =
          (p.flags() & CheckBoundsFlag::kConvertStringAndMinusZero)
              ? kIdentifyZeros
              : kDistinguishZeros;
      VisitBinop<T>(node,
                    UseInfo::CheckedSigned64AsWord64(zero_handling, feedback),
                    UseInfo::Word64(), MachineRepresentation::kWord64);
      if (lower<T>()) {
        NodeProperties::ChangeOp(
            node, simplified()->CheckedUint64Bounds(feedback, new_flags));
      }
    }
  }

  static MachineType MachineTypeFor(CTypeInfo::Type type) {
    switch (type) {
      case CTypeInfo::Type::kVoid:
        return MachineType::AnyTagged();
      case CTypeInfo::Type::kBool:
        return MachineType::Bool();
      case CTypeInfo::Type::kInt32:
        return MachineType::Int32();
      case CTypeInfo::Type::kUint32:
        return MachineType::Uint32();
      case CTypeInfo::Type::kInt64:
        return MachineType::Int64();
      case CTypeInfo::Type::kUint64:
        return MachineType::Uint64();
      case CTypeInfo::Type::kFloat32:
        return MachineType::Float32();
      case CTypeInfo::Type::kFloat64:
        return MachineType::Float64();
      case CTypeInfo::Type::kV8Value:
        return MachineType::AnyTagged();
    }
  }

  UseInfo UseInfoForFastApiCallArgument(CTypeInfo::Type type,
                                        FeedbackSource const& feedback) {
    switch (type) {
      case CTypeInfo::Type::kVoid:
        UNREACHABLE();
      case CTypeInfo::Type::kBool:
        return UseInfo::Bool();
      case CTypeInfo::Type::kInt32:
      case CTypeInfo::Type::kUint32:
      case CTypeInfo::Type::kFloat32:
        return UseInfo::CheckedNumberAsWord32(feedback);
      case CTypeInfo::Type::kInt64:
        return UseInfo::CheckedSigned64AsWord64(kIdentifyZeros, feedback);
      case CTypeInfo::Type::kFloat64:
        return UseInfo::CheckedNumberAsFloat64(kIdentifyZeros, feedback);
      // UseInfo::Word64 does not propagate any TypeCheckKind, so it relies
      // on the implicit assumption that Word64 representation only holds
      // Numbers, which is already no longer true with BigInts. By now,
      // BigInts are handled in a very conservative way to make sure they don't
      // fall into that pit, but future changes may break this here.
      case CTypeInfo::Type::kUint64:
        return UseInfo::Word64();
      case CTypeInfo::Type::kV8Value:
        return UseInfo::AnyTagged();
    }
  }

  static constexpr int kInitialArgumentsCount = 10;

  template <Phase T>
  void VisitFastApiCall(Node* node, SimplifiedLowering* lowering) {
    FastApiCallParameters const& op_params =
        FastApiCallParametersOf(node->op());
    const CFunctionInfo* c_signature = op_params.signature();
    const int c_arg_count = c_signature->ArgumentCount();
    CallDescriptor* call_descriptor = op_params.descriptor();
    int js_arg_count = static_cast<int>(call_descriptor->ParameterCount());
    const int value_input_count = node->op()->ValueInputCount();
    CHECK_EQ(FastApiCallNode::ArityForArgc(c_arg_count, js_arg_count),
             value_input_count);

    base::SmallVector<UseInfo, kInitialArgumentsCount> arg_use_info(
        c_arg_count);
    // The target of the fast call.
    ProcessInput<T>(node, 0, UseInfo::Word());
    // Propagate representation information from TypeInfo.
    for (int i = 0; i < c_arg_count; i++) {
      arg_use_info[i] = UseInfoForFastApiCallArgument(
          c_signature->ArgumentInfo(i).GetType(), op_params.feedback());
      ProcessInput<T>(node, i + FastApiCallNode::kFastTargetInputCount,
                      arg_use_info[i]);
    }

    // The call code for the slow call.
    ProcessInput<T>(node, c_arg_count + FastApiCallNode::kFastTargetInputCount,
                    UseInfo::AnyTagged());
    for (int i = 1; i <= js_arg_count; i++) {
      ProcessInput<T>(node,
                      c_arg_count + FastApiCallNode::kFastTargetInputCount + i,
                      TruncatingUseInfoFromRepresentation(
                          call_descriptor->GetInputType(i).representation()));
    }
    for (int i = c_arg_count + FastApiCallNode::kFastTargetInputCount +
                 js_arg_count;
         i < value_input_count; ++i) {
      ProcessInput<T>(node, i, UseInfo::AnyTagged());
    }
    ProcessRemainingInputs<T>(node, value_input_count);

    MachineType return_type =
        MachineTypeFor(c_signature->ReturnInfo().GetType());
    SetOutput<T>(node, return_type.representation());
  }

  // Dispatching routine for visiting the node {node} with the usage {use}.
  // Depending on the operator, propagate new usage info to the inputs.
  template <Phase T>
  void VisitNode(Node* node, Truncation truncation,
                 SimplifiedLowering* lowering) {
    tick_counter_->TickAndMaybeEnterSafepoint();

    // Unconditionally eliminate unused pure nodes (only relevant if there's
    // a pure operation in between two effectful ones, where the last one
    // is unused).
    // Note: We must not do this for constants, as they are cached and we
    // would thus kill the cached {node} during lowering (i.e. replace all
    // uses with Dead), but at that point some node lowering might have
    // already taken the constant {node} from the cache (while it was not
    // yet killed) and we would afterwards replace that use with Dead as well.
    if (node->op()->ValueInputCount() > 0 &&
        node->op()->HasProperty(Operator::kPure) && truncation.IsUnused()) {
      return VisitUnused<T>(node);
    }

    if (lower<T>()) InsertUnreachableIfNecessary<T>(node);

    switch (node->opcode()) {
      //------------------------------------------------------------------
      // Common operators.
      //------------------------------------------------------------------
      case IrOpcode::kStart:
        // We use Start as a terminator for the frame state chain, so even
        // tho Start doesn't really produce a value, we have to say Tagged
        // here, otherwise the input conversion will fail.
        return VisitLeaf<T>(node, MachineRepresentation::kTagged);
      case IrOpcode::kParameter:
        // TODO(titzer): use representation from linkage.
        return VisitUnop<T>(node, UseInfo::None(),
                            MachineRepresentation::kTagged);
      case IrOpcode::kInt32Constant:
        return VisitLeaf<T>(node, MachineRepresentation::kWord32);
      case IrOpcode::kInt64Constant:
        return VisitLeaf<T>(node, MachineRepresentation::kWord64);
      case IrOpcode::kExternalConstant:
        return VisitLeaf<T>(node, MachineType::PointerRepresentation());
      case IrOpcode::kNumberConstant: {
        double const value = OpParameter<double>(node->op());
        int value_as_int;
        if (DoubleToSmiInteger(value, &value_as_int)) {
          VisitLeaf<T>(node, MachineRepresentation::kTaggedSigned);
          if (lower<T>()) {
            intptr_t smi = bit_cast<intptr_t>(Smi::FromInt(value_as_int));
            DeferReplacement(node, lowering->jsgraph()->IntPtrConstant(smi));
          }
          return;
        }
        VisitLeaf<T>(node, MachineRepresentation::kTagged);
        return;
      }
      case IrOpcode::kHeapConstant:
      case IrOpcode::kDelayedStringConstant:
        return VisitLeaf<T>(node, MachineRepresentation::kTaggedPointer);
      case IrOpcode::kPointerConstant: {
        VisitLeaf<T>(node, MachineType::PointerRepresentation());
        if (lower<T>()) {
          intptr_t const value = OpParameter<intptr_t>(node->op());
          DeferReplacement(node, lowering->jsgraph()->IntPtrConstant(value));
        }
        return;
      }

      case IrOpcode::kBranch: {
        DCHECK(TypeOf(node->InputAt(0)).Is(Type::Boolean()));
        ProcessInput<T>(node, 0, UseInfo::Bool());
        EnqueueInput<T>(node, NodeProperties::FirstControlIndex(node));
        return;
      }
      case IrOpcode::kSwitch:
        ProcessInput<T>(node, 0, UseInfo::TruncatingWord32());
        EnqueueInput<T>(node, NodeProperties::FirstControlIndex(node));
        return;
      case IrOpcode::kSelect:
        return VisitSelect<T>(node, truncation, lowering);
      case IrOpcode::kPhi:
        return VisitPhi<T>(node, truncation, lowering);
      case IrOpcode::kCall:
        return VisitCall<T>(node, lowering);

      //------------------------------------------------------------------
      // JavaScript operators.
      //------------------------------------------------------------------
      case IrOpcode::kToBoolean: {
        if (truncation.IsUsedAsBool()) {
          ProcessInput<T>(node, 0, UseInfo::Bool());
          SetOutput<T>(node, MachineRepresentation::kBit);
          if (lower<T>()) DeferReplacement(node, node->InputAt(0));
        } else {
          VisitInputs<T>(node);
          SetOutput<T>(node, MachineRepresentation::kTaggedPointer);
        }
        return;
      }
      case IrOpcode::kJSToNumber:
      case IrOpcode::kJSToNumberConvertBigInt:
      case IrOpcode::kJSToNumeric: {
        DCHECK(NodeProperties::GetType(node).Is(Type::Union(
            Type::BigInt(), Type::NumberOrOddball(), graph()->zone())));
        VisitInputs<T>(node);
        // TODO(bmeurer): Optimize somewhat based on input type?
        if (truncation.IsUsedAsWord32()) {
          SetOutput<T>(node, MachineRepresentation::kWord32);
          if (lower<T>())
            lowering->DoJSToNumberOrNumericTruncatesToWord32(node, this);
        } else if (truncation.TruncatesOddballAndBigIntToNumber()) {
          SetOutput<T>(node, MachineRepresentation::kFloat64);
          if (lower<T>())
            lowering->DoJSToNumberOrNumericTruncatesToFloat64(node, this);
        } else {
          SetOutput<T>(node, MachineRepresentation::kTagged);
        }
        return;
      }

      //------------------------------------------------------------------
      // Simplified operators.
      //------------------------------------------------------------------
      case IrOpcode::kBooleanNot: {
        if (lower<T>()) {
          NodeInfo* input_info = GetInfo(node->InputAt(0));
          if (input_info->representation() == MachineRepresentation::kBit) {
            // BooleanNot(x: kRepBit) => Word32Equal(x, #0)
            node->AppendInput(jsgraph_->zone(), jsgraph_->Int32Constant(0));
            NodeProperties::ChangeOp(node, lowering->machine()->Word32Equal());
          } else if (CanBeTaggedPointer(input_info->representation())) {
            // BooleanNot(x: kRepTagged) => WordEqual(x, #false)
            node->AppendInput(jsgraph_->zone(), jsgraph_->FalseConstant());
            NodeProperties::ChangeOp(node, lowering->machine()->WordEqual());
          } else {
            DCHECK(TypeOf(node->InputAt(0)).IsNone());
            DeferReplacement(node, lowering->jsgraph()->Int32Constant(0));
          }
        } else {
          // No input representation requirement; adapt during lowering.
          ProcessInput<T>(node, 0, UseInfo::AnyTruncatingToBool());
          SetOutput<T>(node, MachineRepresentation::kBit);
        }
        return;
      }
      case IrOpcode::kNumberEqual: {
        Type const lhs_type = TypeOf(node->InputAt(0));
        Type const rhs_type = TypeOf(node->InputAt(1));
        // Regular number comparisons in JavaScript generally identify zeros,
        // so we always pass kIdentifyZeros for the inputs, and in addition
        // we can truncate -0 to 0 for otherwise Unsigned32 or Signed32 inputs.
        // For equality we also handle the case that one side is non-zero, in
        // which case we allow to truncate NaN to 0 on the other side.
        if ((lhs_type.Is(Type::Unsigned32OrMinusZero()) &&
             rhs_type.Is(Type::Unsigned32OrMinusZero())) ||
            (lhs_type.Is(Type::Unsigned32OrMinusZeroOrNaN()) &&
             rhs_type.Is(Type::Unsigned32OrMinusZeroOrNaN()) &&
             OneInputCannotBe(node, type_cache_->kZeroish))) {
          // => unsigned Int32Cmp
          VisitBinop<T>(node, UseInfo::TruncatingWord32(),
                        MachineRepresentation::kBit);
          if (lower<T>()) NodeProperties::ChangeOp(node, Uint32Op(node));
          return;
        }
        if ((lhs_type.Is(Type::Signed32OrMinusZero()) &&
             rhs_type.Is(Type::Signed32OrMinusZero())) ||
            (lhs_type.Is(Type::Signed32OrMinusZeroOrNaN()) &&
             rhs_type.Is(Type::Signed32OrMinusZeroOrNaN()) &&
             OneInputCannotBe(node, type_cache_->kZeroish))) {
          // => signed Int32Cmp
          VisitBinop<T>(node, UseInfo::TruncatingWord32(),
                        MachineRepresentation::kBit);
          if (lower<T>()) NodeProperties::ChangeOp(node, Int32Op(node));
          return;
        }
        // => Float64Cmp
        VisitBinop<T>(node, UseInfo::TruncatingFloat64(kIdentifyZeros),
                      MachineRepresentation::kBit);
        if (lower<T>()) NodeProperties::ChangeOp(node, Float64Op(node));
        return;
      }
      case IrOpcode::kNumberLessThan:
      case IrOpcode::kNumberLessThanOrEqual: {
        Type const lhs_type = TypeOf(node->InputAt(0));
        Type const rhs_type = TypeOf(node->InputAt(1));
        // Regular number comparisons in JavaScript generally identify zeros,
        // so we always pass kIdentifyZeros for the inputs, and in addition
        // we can truncate -0 to 0 for otherwise Unsigned32 or Signed32 inputs.
        if (lhs_type.Is(Type::Unsigned32OrMinusZero()) &&
            rhs_type.Is(Type::Unsigned32OrMinusZero())) {
          // => unsigned Int32Cmp
          VisitBinop<T>(node, UseInfo::TruncatingWord32(),
                        MachineRepresentation::kBit);
          if (lower<T>()) NodeProperties::ChangeOp(node, Uint32Op(node));
        } else if (lhs_type.Is(Type::Signed32OrMinusZero()) &&
                   rhs_type.Is(Type::Signed32OrMinusZero())) {
          // => signed Int32Cmp
          VisitBinop<T>(node, UseInfo::TruncatingWord32(),
                        MachineRepresentation::kBit);
          if (lower<T>()) NodeProperties::ChangeOp(node, Int32Op(node));
        } else {
          // => Float64Cmp
          VisitBinop<T>(node, UseInfo::TruncatingFloat64(kIdentifyZeros),
                        MachineRepresentation::kBit);
          if (lower<T>()) NodeProperties::ChangeOp(node, Float64Op(node));
        }
        return;
      }

      case IrOpcode::kSpeculativeSafeIntegerAdd:
      case IrOpcode::kSpeculativeSafeIntegerSubtract:
        return VisitSpeculativeIntegerAdditiveOp<T>(node, truncation, lowering);

      case IrOpcode::kSpeculativeNumberAdd:
      case IrOpcode::kSpeculativeNumberSubtract:
        return VisitSpeculativeAdditiveOp<T>(node, truncation, lowering);

      case IrOpcode::kSpeculativeNumberLessThan:
      case IrOpcode::kSpeculativeNumberLessThanOrEqual:
      case IrOpcode::kSpeculativeNumberEqual: {
        Type const lhs_type = TypeOf(node->InputAt(0));
        Type const rhs_type = TypeOf(node->InputAt(1));
        // Regular number comparisons in JavaScript generally identify zeros,
        // so we always pass kIdentifyZeros for the inputs, and in addition
        // we can truncate -0 to 0 for otherwise Unsigned32 or Signed32 inputs.
        if (lhs_type.Is(Type::Unsigned32OrMinusZero()) &&
            rhs_type.Is(Type::Unsigned32OrMinusZero())) {
          // => unsigned Int32Cmp
          VisitBinop<T>(node, UseInfo::TruncatingWord32(),
                        MachineRepresentation::kBit);
          if (lower<T>()) ChangeToPureOp(node, Uint32Op(node));
          return;
        } else if (lhs_type.Is(Type::Signed32OrMinusZero()) &&
                   rhs_type.Is(Type::Signed32OrMinusZero())) {
          // => signed Int32Cmp
          VisitBinop<T>(node, UseInfo::TruncatingWord32(),
                        MachineRepresentation::kBit);
          if (lower<T>()) ChangeToPureOp(node, Int32Op(node));
          return;
        }
        // Try to use type feedback.
        NumberOperationHint hint = NumberOperationHintOf(node->op());
        switch (hint) {
          case NumberOperationHint::kSigned32:
          case NumberOperationHint::kSignedSmall:
            if (propagate<T>()) {
              VisitBinop<T>(node,
                            CheckedUseInfoAsWord32FromHint(
                                hint, FeedbackSource(), kIdentifyZeros),
                            MachineRepresentation::kBit);
            } else if (retype<T>()) {
              SetOutput<T>(node, MachineRepresentation::kBit, Type::Any());
            } else {
              DCHECK(lower<T>());
              Node* lhs = node->InputAt(0);
              Node* rhs = node->InputAt(1);
              if (IsNodeRepresentationTagged(lhs) &&
                  IsNodeRepresentationTagged(rhs)) {
                VisitBinop<T>(node,
                              UseInfo::CheckedSignedSmallAsTaggedSigned(
                                  FeedbackSource(), kIdentifyZeros),
                              MachineRepresentation::kBit);
                ChangeToPureOp(
                    node, changer_->TaggedSignedOperatorFor(node->opcode()));

              } else {
                VisitBinop<T>(node,
                              CheckedUseInfoAsWord32FromHint(
                                  hint, FeedbackSource(), kIdentifyZeros),
                              MachineRepresentation::kBit);
                ChangeToPureOp(node, Int32Op(node));
              }
            }
            return;
          case NumberOperationHint::kSignedSmallInputs:
            // This doesn't make sense for compare operations.
            UNREACHABLE();
          case NumberOperationHint::kNumberOrOddball:
            // Abstract and strict equality don't perform ToNumber conversions
            // on Oddballs, so make sure we don't accidentially sneak in a
            // hint with Oddball feedback here.
            DCHECK_NE(IrOpcode::kSpeculativeNumberEqual, node->opcode());
            V8_FALLTHROUGH;
          case NumberOperationHint::kNumberOrBoolean:
          case NumberOperationHint::kNumber:
            VisitBinop<T>(node,
                          CheckedUseInfoAsFloat64FromHint(
                              hint, FeedbackSource(), kIdentifyZeros),
                          MachineRepresentation::kBit);
            if (lower<T>()) ChangeToPureOp(node, Float64Op(node));
            return;
        }
        UNREACHABLE();
        return;
      }

      case IrOpcode::kNumberAdd:
      case IrOpcode::kNumberSubtract: {
        if (TypeOf(node->InputAt(0))
                .Is(type_cache_->kAdditiveSafeIntegerOrMinusZero) &&
            TypeOf(node->InputAt(1))
                .Is(type_cache_->kAdditiveSafeIntegerOrMinusZero) &&
            (TypeOf(node).Is(Type::Signed32()) ||
             TypeOf(node).Is(Type::Unsigned32()) ||
             truncation.IsUsedAsWord32())) {
          // => Int32Add/Sub
          VisitWord32TruncatingBinop<T>(node);
          if (lower<T>()) ChangeToPureOp(node, Int32Op(node));
        } else if (jsgraph_->machine()->Is64() &&
                   BothInputsAre(node, type_cache_->kSafeInteger) &&
                   GetUpperBound(node).Is(type_cache_->kSafeInteger)) {
          // => Int64Add/Sub
          VisitInt64Binop<T>(node);
          if (lower<T>()) ChangeToPureOp(node, Int64Op(node));
        } else {
          // => Float64Add/Sub
          VisitFloat64Binop<T>(node);
          if (lower<T>()) ChangeToPureOp(node, Float64Op(node));
        }
        return;
      }
      case IrOpcode::kSpeculativeNumberMultiply: {
        if (BothInputsAre(node, Type::Integral32()) &&
            (NodeProperties::GetType(node).Is(Type::Signed32()) ||
             NodeProperties::GetType(node).Is(Type::Unsigned32()) ||
             (truncation.IsUsedAsWord32() &&
              NodeProperties::GetType(node).Is(
                  type_cache_->kSafeIntegerOrMinusZero)))) {
          // Multiply reduces to Int32Mul if the inputs are integers, and
          // (a) the output is either known to be Signed32, or
          // (b) the output is known to be Unsigned32, or
          // (c) the uses are truncating and the result is in the safe
          //     integer range.
          VisitWord32TruncatingBinop<T>(node);
          if (lower<T>()) ChangeToPureOp(node, Int32Op(node));
          return;
        }
        // Try to use type feedback.
        NumberOperationHint hint = NumberOperationHintOf(node->op());
        Type input0_type = TypeOf(node->InputAt(0));
        Type input1_type = TypeOf(node->InputAt(1));

        // Handle the case when no int32 checks on inputs are necessary
        // (but an overflow check is needed on the output).
        if (BothInputsAre(node, Type::Signed32())) {
          // If both inputs and feedback are int32, use the overflow op.
          if (hint == NumberOperationHint::kSignedSmall ||
              hint == NumberOperationHint::kSigned32) {
            VisitBinop<T>(node, UseInfo::TruncatingWord32(),
                          MachineRepresentation::kWord32, Type::Signed32());
            if (lower<T>()) {
              LowerToCheckedInt32Mul(node, truncation, input0_type,
                                     input1_type);
            }
            return;
          }
        }

        if (hint == NumberOperationHint::kSignedSmall ||
            hint == NumberOperationHint::kSigned32) {
          VisitBinop<T>(node, CheckedUseInfoAsWord32FromHint(hint),
                        MachineRepresentation::kWord32, Type::Signed32());
          if (lower<T>()) {
            LowerToCheckedInt32Mul(node, truncation, input0_type, input1_type);
          }
          return;
        }

        // Checked float64 x float64 => float64
        VisitBinop<T>(node,
                      UseInfo::CheckedNumberOrOddballAsFloat64(
                          kDistinguishZeros, FeedbackSource()),
                      MachineRepresentation::kFloat64, Type::Number());
        if (lower<T>()) ChangeToPureOp(node, Float64Op(node));
        return;
      }
      case IrOpcode::kNumberMultiply: {
        if (TypeOf(node->InputAt(0)).Is(Type::Integral32()) &&
            TypeOf(node->InputAt(1)).Is(Type::Integral32()) &&
            (TypeOf(node).Is(Type::Signed32()) ||
             TypeOf(node).Is(Type::Unsigned32()) ||
             (truncation.IsUsedAsWord32() &&
              TypeOf(node).Is(type_cache_->kSafeIntegerOrMinusZero)))) {
          // Multiply reduces to Int32Mul if the inputs are integers, and
          // (a) the output is either known to be Signed32, or
          // (b) the output is known to be Unsigned32, or
          // (c) the uses are truncating and the result is in the safe
          //     integer range.
          VisitWord32TruncatingBinop<T>(node);
          if (lower<T>()) ChangeToPureOp(node, Int32Op(node));
          return;
        }
        // Number x Number => Float64Mul
        VisitFloat64Binop<T>(node);
        if (lower<T>()) ChangeToPureOp(node, Float64Op(node));
        return;
      }
      case IrOpcode::kSpeculativeNumberDivide: {
        if (BothInputsAreUnsigned32(node) && truncation.IsUsedAsWord32()) {
          // => unsigned Uint32Div
          VisitWord32TruncatingBinop<T>(node);
          if (lower<T>()) DeferReplacement(node, lowering->Uint32Div(node));
          return;
        }
        if (BothInputsAreSigned32(node)) {
          if (NodeProperties::GetType(node).Is(Type::Signed32())) {
            // => signed Int32Div
            VisitWord32TruncatingBinop<T>(node);
            if (lower<T>()) DeferReplacement(node, lowering->Int32Div(node));
            return;
          }
          if (truncation.IsUsedAsWord32()) {
            // => signed Int32Div
            VisitWord32TruncatingBinop<T>(node);
            if (lower<T>()) DeferReplacement(node, lowering->Int32Div(node));
            return;
          }
        }

        // Try to use type feedback.
        NumberOperationHint hint = NumberOperationHintOf(node->op());

        // Handle the case when no uint32 checks on inputs are necessary
        // (but an overflow check is needed on the output).
        if (BothInputsAreUnsigned32(node)) {
          if (hint == NumberOperationHint::kSignedSmall ||
              hint == NumberOperationHint::kSigned32) {
            VisitBinop<T>(node, UseInfo::TruncatingWord32(),
                          MachineRepresentation::kWord32, Type::Unsigned32());
            if (lower<T>()) ChangeToUint32OverflowOp(node);
            return;
          }
        }

        // Handle the case when no int32 checks on inputs are necessary
        // (but an overflow check is needed on the output).
        if (BothInputsAreSigned32(node)) {
          // If both the inputs the feedback are int32, use the overflow op.
          if (hint == NumberOperationHint::kSignedSmall ||
              hint == NumberOperationHint::kSigned32) {
            VisitBinop<T>(node, UseInfo::TruncatingWord32(),
                          MachineRepresentation::kWord32, Type::Signed32());
            if (lower<T>()) ChangeToInt32OverflowOp(node);
            return;
          }
        }

        if (hint == NumberOperationHint::kSigned32 ||
            hint == NumberOperationHint::kSignedSmall ||
            hint == NumberOperationHint::kSignedSmallInputs) {
          // If the result is truncated, we only need to check the inputs.
          if (truncation.IsUsedAsWord32()) {
            VisitBinop<T>(node, CheckedUseInfoAsWord32FromHint(hint),
                          MachineRepresentation::kWord32);
            if (lower<T>()) DeferReplacement(node, lowering->Int32Div(node));
            return;
          } else if (hint != NumberOperationHint::kSignedSmallInputs) {
            VisitBinop<T>(node, CheckedUseInfoAsWord32FromHint(hint),
                          MachineRepresentation::kWord32, Type::Signed32());
            if (lower<T>()) ChangeToInt32OverflowOp(node);
            return;
          }
        }

        // default case => Float64Div
        VisitBinop<T>(node,
                      UseInfo::CheckedNumberOrOddballAsFloat64(
                          kDistinguishZeros, FeedbackSource()),
                      MachineRepresentation::kFloat64, Type::Number());
        if (lower<T>()) ChangeToPureOp(node, Float64Op(node));
        return;
      }
      case IrOpcode::kNumberDivide: {
        if (TypeOf(node->InputAt(0)).Is(Type::Unsigned32()) &&
            TypeOf(node->InputAt(1)).Is(Type::Unsigned32()) &&
            (truncation.IsUsedAsWord32() ||
             TypeOf(node).Is(Type::Unsigned32()))) {
          // => unsigned Uint32Div
          VisitWord32TruncatingBinop<T>(node);
          if (lower<T>()) DeferReplacement(node, lowering->Uint32Div(node));
          return;
        }
        if (TypeOf(node->InputAt(0)).Is(Type::Signed32()) &&
            TypeOf(node->InputAt(1)).Is(Type::Signed32()) &&
            (truncation.IsUsedAsWord32() ||
             TypeOf(node).Is(Type::Signed32()))) {
          // => signed Int32Div
          VisitWord32TruncatingBinop<T>(node);
          if (lower<T>()) DeferReplacement(node, lowering->Int32Div(node));
          return;
        }
        // Number x Number => Float64Div
        VisitFloat64Binop<T>(node);
        if (lower<T>()) ChangeToPureOp(node, Float64Op(node));
        return;
      }
      case IrOpcode::kSpeculativeNumberModulus:
        return VisitSpeculativeNumberModulus<T>(node, truncation, lowering);
      case IrOpcode::kNumberModulus: {
        Type const lhs_type = TypeOf(node->InputAt(0));
        Type const rhs_type = TypeOf(node->InputAt(1));
        if ((lhs_type.Is(Type::Unsigned32OrMinusZeroOrNaN()) &&
             rhs_type.Is(Type::Unsigned32OrMinusZeroOrNaN())) &&
            (truncation.IsUsedAsWord32() ||
             TypeOf(node).Is(Type::Unsigned32()))) {
          // => unsigned Uint32Mod
          VisitWord32TruncatingBinop<T>(node);
          if (lower<T>()) DeferReplacement(node, lowering->Uint32Mod(node));
          return;
        }
        if ((lhs_type.Is(Type::Signed32OrMinusZeroOrNaN()) &&
             rhs_type.Is(Type::Signed32OrMinusZeroOrNaN())) &&
            (truncation.IsUsedAsWord32() || TypeOf(node).Is(Type::Signed32()) ||
             (truncation.IdentifiesZeroAndMinusZero() &&
              TypeOf(node).Is(Type::Signed32OrMinusZero())))) {
          // => signed Int32Mod
          VisitWord32TruncatingBinop<T>(node);
          if (lower<T>()) DeferReplacement(node, lowering->Int32Mod(node));
          return;
        }
        // => Float64Mod
        // For the left hand side we just propagate the identify zeros
        // mode of the {truncation}; and for modulus the sign of the
        // right hand side doesn't matter anyways, so in particular there's
        // no observable difference between a 0 and a -0 then.
        UseInfo const lhs_use =
            UseInfo::TruncatingFloat64(truncation.identify_zeros());
        UseInfo const rhs_use = UseInfo::TruncatingFloat64(kIdentifyZeros);
        VisitBinop<T>(node, lhs_use, rhs_use, MachineRepresentation::kFloat64);
        if (lower<T>()) ChangeToPureOp(node, Float64Op(node));
        return;
      }
      case IrOpcode::kNumberBitwiseOr:
      case IrOpcode::kNumberBitwiseXor:
      case IrOpcode::kNumberBitwiseAnd: {
        VisitWord32TruncatingBinop<T>(node);
        if (lower<T>()) NodeProperties::ChangeOp(node, Int32Op(node));
        return;
      }
      case IrOpcode::kSpeculativeNumberBitwiseOr:
      case IrOpcode::kSpeculativeNumberBitwiseXor:
      case IrOpcode::kSpeculativeNumberBitwiseAnd:
        VisitSpeculativeInt32Binop<T>(node);
        if (lower<T>()) {
          ChangeToPureOp(node, Int32Op(node));
        }
        return;
      case IrOpcode::kNumberShiftLeft: {
        Type rhs_type = GetUpperBound(node->InputAt(1));
        VisitBinop<T>(node, UseInfo::TruncatingWord32(),
                      UseInfo::TruncatingWord32(),
                      MachineRepresentation::kWord32);
        if (lower<T>()) {
          MaskShiftOperand(node, rhs_type);
          ChangeToPureOp(node, lowering->machine()->Word32Shl());
        }
        return;
      }
      case IrOpcode::kSpeculativeNumberShiftLeft: {
        if (BothInputsAre(node, Type::NumberOrOddball())) {
          Type rhs_type = GetUpperBound(node->InputAt(1));
          VisitBinop<T>(node, UseInfo::TruncatingWord32(),
                        UseInfo::TruncatingWord32(),
                        MachineRepresentation::kWord32);
          if (lower<T>()) {
            MaskShiftOperand(node, rhs_type);
            ChangeToPureOp(node, lowering->machine()->Word32Shl());
          }
          return;
        }
        NumberOperationHint hint = NumberOperationHintOf(node->op());
        Type rhs_type = GetUpperBound(node->InputAt(1));
        VisitBinop<T>(node, CheckedUseInfoAsWord32FromHint(hint),
                      MachineRepresentation::kWord32, Type::Signed32());
        if (lower<T>()) {
          MaskShiftOperand(node, rhs_type);
          ChangeToPureOp(node, lowering->machine()->Word32Shl());
        }
        return;
      }
      case IrOpcode::kNumberShiftRight: {
        Type rhs_type = GetUpperBound(node->InputAt(1));
        VisitBinop<T>(node, UseInfo::TruncatingWord32(),
                      UseInfo::TruncatingWord32(),
                      MachineRepresentation::kWord32);
        if (lower<T>()) {
          MaskShiftOperand(node, rhs_type);
          ChangeToPureOp(node, lowering->machine()->Word32Sar());
        }
        return;
      }
      case IrOpcode::kSpeculativeNumberShiftRight: {
        if (BothInputsAre(node, Type::NumberOrOddball())) {
          Type rhs_type = GetUpperBound(node->InputAt(1));
          VisitBinop<T>(node, UseInfo::TruncatingWord32(),
                        UseInfo::TruncatingWord32(),
                        MachineRepresentation::kWord32);
          if (lower<T>()) {
            MaskShiftOperand(node, rhs_type);
            ChangeToPureOp(node, lowering->machine()->Word32Sar());
          }
          return;
        }
        NumberOperationHint hint = NumberOperationHintOf(node->op());
        Type rhs_type = GetUpperBound(node->InputAt(1));
        VisitBinop<T>(node, CheckedUseInfoAsWord32FromHint(hint),
                      MachineRepresentation::kWord32, Type::Signed32());
        if (lower<T>()) {
          MaskShiftOperand(node, rhs_type);
          ChangeToPureOp(node, lowering->machine()->Word32Sar());
        }
        return;
      }
      case IrOpcode::kNumberShiftRightLogical: {
        Type rhs_type = GetUpperBound(node->InputAt(1));
        VisitBinop<T>(node, UseInfo::TruncatingWord32(),
                      UseInfo::TruncatingWord32(),
                      MachineRepresentation::kWord32);
        if (lower<T>()) {
          MaskShiftOperand(node, rhs_type);
          ChangeToPureOp(node, lowering->machine()->Word32Shr());
        }
        return;
      }
      case IrOpcode::kSpeculativeNumberShiftRightLogical: {
        NumberOperationHint hint = NumberOperationHintOf(node->op());
        Type rhs_type = GetUpperBound(node->InputAt(1));
        if (rhs_type.Is(type_cache_->kZeroish) &&
            (hint == NumberOperationHint::kSignedSmall ||
             hint == NumberOperationHint::kSigned32) &&
            !truncation.IsUsedAsWord32()) {
          // The SignedSmall or Signed32 feedback means that the results that we
          // have seen so far were of type Unsigned31.  We speculate that this
          // will continue to hold.  Moreover, since the RHS is 0, the result
          // will just be the (converted) LHS.
          VisitBinop<T>(node, CheckedUseInfoAsWord32FromHint(hint),
                        MachineRepresentation::kWord32, Type::Unsigned31());
          if (lower<T>()) {
            node->RemoveInput(1);
            NodeProperties::ChangeOp(
                node, simplified()->CheckedUint32ToInt32(FeedbackSource()));
          }
          return;
        }
        if (BothInputsAre(node, Type::NumberOrOddball())) {
          VisitBinop<T>(node, UseInfo::TruncatingWord32(),
                        UseInfo::TruncatingWord32(),
                        MachineRepresentation::kWord32);
          if (lower<T>()) {
            MaskShiftOperand(node, rhs_type);
            ChangeToPureOp(node, lowering->machine()->Word32Shr());
          }
          return;
        }
        VisitBinop<T>(node, CheckedUseInfoAsWord32FromHint(hint),
                      MachineRepresentation::kWord32, Type::Unsigned32());
        if (lower<T>()) {
          MaskShiftOperand(node, rhs_type);
          ChangeToPureOp(node, lowering->machine()->Word32Shr());
        }
        return;
      }
      case IrOpcode::kNumberAbs: {
        // NumberAbs maps both 0 and -0 to 0, so we can generally
        // pass the kIdentifyZeros truncation to its input, and
        // choose to ignore minus zero in all cases.
        Type const input_type = TypeOf(node->InputAt(0));
        if (input_type.Is(Type::Unsigned32OrMinusZero())) {
          VisitUnop<T>(node, UseInfo::TruncatingWord32(),
                       MachineRepresentation::kWord32);
          if (lower<T>()) DeferReplacement(node, node->InputAt(0));
        } else if (input_type.Is(Type::Signed32OrMinusZero())) {
          VisitUnop<T>(node, UseInfo::TruncatingWord32(),
                       MachineRepresentation::kWord32);
          if (lower<T>()) DeferReplacement(node, lowering->Int32Abs(node));
        } else if (input_type.Is(type_cache_->kPositiveIntegerOrNaN)) {
          VisitUnop<T>(node, UseInfo::TruncatingFloat64(kIdentifyZeros),
                       MachineRepresentation::kFloat64);
          if (lower<T>()) DeferReplacement(node, node->InputAt(0));
        } else {
          VisitUnop<T>(node, UseInfo::TruncatingFloat64(kIdentifyZeros),
                       MachineRepresentation::kFloat64);
          if (lower<T>()) NodeProperties::ChangeOp(node, Float64Op(node));
        }
        return;
      }
      case IrOpcode::kNumberClz32: {
        VisitUnop<T>(node, UseInfo::TruncatingWord32(),
                     MachineRepresentation::kWord32);
        if (lower<T>()) NodeProperties::ChangeOp(node, Uint32Op(node));
        return;
      }
      case IrOpcode::kNumberImul: {
        VisitBinop<T>(node, UseInfo::TruncatingWord32(),
                      UseInfo::TruncatingWord32(),
                      MachineRepresentation::kWord32);
        if (lower<T>()) NodeProperties::ChangeOp(node, Uint32Op(node));
        return;
      }
      case IrOpcode::kNumberFround: {
        VisitUnop<T>(node, UseInfo::TruncatingFloat64(),
                     MachineRepresentation::kFloat32);
        if (lower<T>()) NodeProperties::ChangeOp(node, Float64Op(node));
        return;
      }
      case IrOpcode::kNumberMax: {
        // It is safe to use the feedback types for left and right hand side
        // here, since we can only narrow those types and thus we can only
        // promise a more specific truncation.
        // For NumberMax we generally propagate whether the truncation
        // identifies zeros to the inputs, and we choose to ignore minus
        // zero in those cases.
        Type const lhs_type = TypeOf(node->InputAt(0));
        Type const rhs_type = TypeOf(node->InputAt(1));
        if ((lhs_type.Is(Type::Unsigned32()) &&
             rhs_type.Is(Type::Unsigned32())) ||
            (lhs_type.Is(Type::Unsigned32OrMinusZero()) &&
             rhs_type.Is(Type::Unsigned32OrMinusZero()) &&
             truncation.IdentifiesZeroAndMinusZero())) {
          VisitWord32TruncatingBinop<T>(node);
          if (lower<T>()) {
            lowering->DoMax(node, lowering->machine()->Uint32LessThan(),
                            MachineRepresentation::kWord32);
          }
        } else if ((lhs_type.Is(Type::Signed32()) &&
                    rhs_type.Is(Type::Signed32())) ||
                   (lhs_type.Is(Type::Signed32OrMinusZero()) &&
                    rhs_type.Is(Type::Signed32OrMinusZero()) &&
                    truncation.IdentifiesZeroAndMinusZero())) {
          VisitWord32TruncatingBinop<T>(node);
          if (lower<T>()) {
            lowering->DoMax(node, lowering->machine()->Int32LessThan(),
                            MachineRepresentation::kWord32);
          }
        } else if (jsgraph_->machine()->Is64() &&
                   lhs_type.Is(type_cache_->kSafeInteger) &&
                   rhs_type.Is(type_cache_->kSafeInteger)) {
          VisitInt64Binop<T>(node);
          if (lower<T>()) {
            lowering->DoMax(node, lowering->machine()->Int64LessThan(),
                            MachineRepresentation::kWord64);
          }
        } else {
          VisitBinop<T>(node,
                        UseInfo::TruncatingFloat64(truncation.identify_zeros()),
                        MachineRepresentation::kFloat64);
          if (lower<T>()) {
            // If the right hand side is not NaN, and the left hand side
            // is not NaN (or -0 if the difference between the zeros is
            // observed), we can do a simple floating point comparison here.
            if (lhs_type.Is(truncation.IdentifiesZeroAndMinusZero()
                                ? Type::OrderedNumber()
                                : Type::PlainNumber()) &&
                rhs_type.Is(Type::OrderedNumber())) {
              lowering->DoMax(node, lowering->machine()->Float64LessThan(),
                              MachineRepresentation::kFloat64);
            } else {
              NodeProperties::ChangeOp(node, Float64Op(node));
            }
          }
        }
        return;
      }
      case IrOpcode::kNumberMin: {
        // It is safe to use the feedback types for left and right hand side
        // here, since we can only narrow those types and thus we can only
        // promise a more specific truncation.
        // For NumberMin we generally propagate whether the truncation
        // identifies zeros to the inputs, and we choose to ignore minus
        // zero in those cases.
        Type const lhs_type = TypeOf(node->InputAt(0));
        Type const rhs_type = TypeOf(node->InputAt(1));
        if ((lhs_type.Is(Type::Unsigned32()) &&
             rhs_type.Is(Type::Unsigned32())) ||
            (lhs_type.Is(Type::Unsigned32OrMinusZero()) &&
             rhs_type.Is(Type::Unsigned32OrMinusZero()) &&
             truncation.IdentifiesZeroAndMinusZero())) {
          VisitWord32TruncatingBinop<T>(node);
          if (lower<T>()) {
            lowering->DoMin(node, lowering->machine()->Uint32LessThan(),
                            MachineRepresentation::kWord32);
          }
        } else if ((lhs_type.Is(Type::Signed32()) &&
                    rhs_type.Is(Type::Signed32())) ||
                   (lhs_type.Is(Type::Signed32OrMinusZero()) &&
                    rhs_type.Is(Type::Signed32OrMinusZero()) &&
                    truncation.IdentifiesZeroAndMinusZero())) {
          VisitWord32TruncatingBinop<T>(node);
          if (lower<T>()) {
            lowering->DoMin(node, lowering->machine()->Int32LessThan(),
                            MachineRepresentation::kWord32);
          }
        } else if (jsgraph_->machine()->Is64() &&
                   lhs_type.Is(type_cache_->kSafeInteger) &&
                   rhs_type.Is(type_cache_->kSafeInteger)) {
          VisitInt64Binop<T>(node);
          if (lower<T>()) {
            lowering->DoMin(node, lowering->machine()->Int64LessThan(),
                            MachineRepresentation::kWord64);
          }
        } else {
          VisitBinop<T>(node,
                        UseInfo::TruncatingFloat64(truncation.identify_zeros()),
                        MachineRepresentation::kFloat64);
          if (lower<T>()) {
            // If the left hand side is not NaN, and the right hand side
            // is not NaN (or -0 if the difference between the zeros is
            // observed), we can do a simple floating point comparison here.
            if (lhs_type.Is(Type::OrderedNumber()) &&
                rhs_type.Is(truncation.IdentifiesZeroAndMinusZero()
                                ? Type::OrderedNumber()
                                : Type::PlainNumber())) {
              lowering->DoMin(node,
                              lowering->machine()->Float64LessThanOrEqual(),
                              MachineRepresentation::kFloat64);
            } else {
              NodeProperties::ChangeOp(node, Float64Op(node));
            }
          }
        }
        return;
      }
      case IrOpcode::kNumberAtan2:
      case IrOpcode::kNumberPow: {
        VisitBinop<T>(node, UseInfo::TruncatingFloat64(),
                      MachineRepresentation::kFloat64);
        if (lower<T>()) NodeProperties::ChangeOp(node, Float64Op(node));
        return;
      }
      case IrOpcode::kNumberCeil:
      case IrOpcode::kNumberFloor:
      case IrOpcode::kNumberRound:
      case IrOpcode::kNumberTrunc: {
        // For NumberCeil, NumberFloor, NumberRound and NumberTrunc we propagate
        // the zero identification part of the truncation, and we turn them into
        // no-ops if we figure out (late) that their input is already an
        // integer, NaN or -0.
        Type const input_type = TypeOf(node->InputAt(0));
        VisitUnop<T>(node,
                     UseInfo::TruncatingFloat64(truncation.identify_zeros()),
                     MachineRepresentation::kFloat64);
        if (lower<T>()) {
          if (input_type.Is(type_cache_->kIntegerOrMinusZeroOrNaN)) {
            DeferReplacement(node, node->InputAt(0));
          } else if (node->opcode() == IrOpcode::kNumberRound) {
            DeferReplacement(node, lowering->Float64Round(node));
          } else {
            NodeProperties::ChangeOp(node, Float64Op(node));
          }
        }
        return;
      }
      case IrOpcode::kCheckBigInt: {
        if (InputIs(node, Type::BigInt())) {
          VisitNoop<T>(node, truncation);
        } else {
          VisitUnop<T>(node, UseInfo::AnyTagged(),
                       MachineRepresentation::kTaggedPointer);
        }
        return;
      }
      case IrOpcode::kBigIntAsUintN: {
        ProcessInput<T>(node, 0, UseInfo::TruncatingWord64());
        SetOutput<T>(node, MachineRepresentation::kWord64, Type::BigInt());
        return;
      }
      case IrOpcode::kNumberAcos:
      case IrOpcode::kNumberAcosh:
      case IrOpcode::kNumberAsin:
      case IrOpcode::kNumberAsinh:
      case IrOpcode::kNumberAtan:
      case IrOpcode::kNumberAtanh:
      case IrOpcode::kNumberCos:
      case IrOpcode::kNumberCosh:
      case IrOpcode::kNumberExp:
      case IrOpcode::kNumberExpm1:
      case IrOpcode::kNumberLog:
      case IrOpcode::kNumberLog1p:
      case IrOpcode::kNumberLog2:
      case IrOpcode::kNumberLog10:
      case IrOpcode::kNumberCbrt:
      case IrOpcode::kNumberSin:
      case IrOpcode::kNumberSinh:
      case IrOpcode::kNumberTan:
      case IrOpcode::kNumberTanh: {
        VisitUnop<T>(node, UseInfo::TruncatingFloat64(),
                     MachineRepresentation::kFloat64);
        if (lower<T>()) NodeProperties::ChangeOp(node, Float64Op(node));
        return;
      }
      case IrOpcode::kNumberSign: {
        if (InputIs(node, Type::Signed32())) {
          VisitUnop<T>(node, UseInfo::TruncatingWord32(),
                       MachineRepresentation::kWord32);
          if (lower<T>()) DeferReplacement(node, lowering->Int32Sign(node));
        } else {
          VisitUnop<T>(node, UseInfo::TruncatingFloat64(),
                       MachineRepresentation::kFloat64);
          if (lower<T>()) DeferReplacement(node, lowering->Float64Sign(node));
        }
        return;
      }
      case IrOpcode::kNumberSilenceNaN: {
        Type const input_type = TypeOf(node->InputAt(0));
        if (input_type.Is(Type::OrderedNumber())) {
          // No need to silence anything if the input cannot be NaN.
          VisitUnop<T>(node, UseInfo::TruncatingFloat64(),
                       MachineRepresentation::kFloat64);
          if (lower<T>()) DeferReplacement(node, node->InputAt(0));
        } else {
          VisitUnop<T>(node, UseInfo::TruncatingFloat64(),
                       MachineRepresentation::kFloat64);
          if (lower<T>()) NodeProperties::ChangeOp(node, Float64Op(node));
        }
        return;
      }
      case IrOpcode::kNumberSqrt: {
        VisitUnop<T>(node, UseInfo::TruncatingFloat64(),
                     MachineRepresentation::kFloat64);
        if (lower<T>()) NodeProperties::ChangeOp(node, Float64Op(node));
        return;
      }
      case IrOpcode::kNumberToBoolean: {
        // For NumberToBoolean we don't care whether the input is 0 or
        // -0, since both of them are mapped to false anyways, so we
        // can generally pass kIdentifyZeros truncation.
        Type const input_type = TypeOf(node->InputAt(0));
        if (input_type.Is(Type::Integral32OrMinusZeroOrNaN())) {
          // 0, -0 and NaN all map to false, so we can safely truncate
          // all of them to zero here.
          VisitUnop<T>(node, UseInfo::TruncatingWord32(),
                       MachineRepresentation::kBit);
          if (lower<T>()) lowering->DoIntegral32ToBit(node);
        } else if (input_type.Is(Type::OrderedNumber())) {
          VisitUnop<T>(node, UseInfo::TruncatingFloat64(kIdentifyZeros),
                       MachineRepresentation::kBit);
          if (lower<T>()) lowering->DoOrderedNumberToBit(node);
        } else {
          VisitUnop<T>(node, UseInfo::TruncatingFloat64(kIdentifyZeros),
                       MachineRepresentation::kBit);
          if (lower<T>()) lowering->DoNumberToBit(node);
        }
        return;
      }
      case IrOpcode::kNumberToInt32: {
        // Just change representation if necessary.
        VisitUnop<T>(node, UseInfo::TruncatingWord32(),
                     MachineRepresentation::kWord32);
        if (lower<T>()) DeferReplacement(node, node->InputAt(0));
        return;
      }
      case IrOpcode::kNumberToString: {
        VisitUnop<T>(node, UseInfo::AnyTagged(),
                     MachineRepresentation::kTaggedPointer);
        return;
      }
      case IrOpcode::kNumberToUint32: {
        // Just change representation if necessary.
        VisitUnop<T>(node, UseInfo::TruncatingWord32(),
                     MachineRepresentation::kWord32);
        if (lower<T>()) DeferReplacement(node, node->InputAt(0));
        return;
      }
      case IrOpcode::kNumberToUint8Clamped: {
        Type const input_type = TypeOf(node->InputAt(0));
        if (input_type.Is(type_cache_->kUint8OrMinusZeroOrNaN)) {
          VisitUnop<T>(node, UseInfo::TruncatingWord32(),
                       MachineRepresentation::kWord32);
          if (lower<T>()) DeferReplacement(node, node->InputAt(0));
        } else if (input_type.Is(Type::Unsigned32OrMinusZeroOrNaN())) {
          VisitUnop<T>(node, UseInfo::TruncatingWord32(),
                       MachineRepresentation::kWord32);
          if (lower<T>()) lowering->DoUnsigned32ToUint8Clamped(node);
        } else if (input_type.Is(Type::Signed32OrMinusZeroOrNaN())) {
          VisitUnop<T>(node, UseInfo::TruncatingWord32(),
                       MachineRepresentation::kWord32);
          if (lower<T>()) lowering->DoSigned32ToUint8Clamped(node);
        } else if (input_type.Is(type_cache_->kIntegerOrMinusZeroOrNaN)) {
          VisitUnop<T>(node, UseInfo::TruncatingFloat64(),
                       MachineRepresentation::kFloat64);
          if (lower<T>()) lowering->DoIntegerToUint8Clamped(node);
        } else {
          VisitUnop<T>(node, UseInfo::TruncatingFloat64(),
                       MachineRepresentation::kFloat64);
          if (lower<T>()) lowering->DoNumberToUint8Clamped(node);
        }
        return;
      }
      case IrOpcode::kReferenceEqual: {
        VisitBinop<T>(node, UseInfo::AnyTagged(), MachineRepresentation::kBit);
        if (lower<T>()) {
          if (COMPRESS_POINTERS_BOOL) {
            NodeProperties::ChangeOp(node, lowering->machine()->Word32Equal());
          } else {
            NodeProperties::ChangeOp(node, lowering->machine()->WordEqual());
          }
        }
        return;
      }
      case IrOpcode::kSameValueNumbersOnly: {
        VisitBinop<T>(node, UseInfo::AnyTagged(),
                      MachineRepresentation::kTaggedPointer);
        return;
      }
      case IrOpcode::kSameValue: {
        if (truncation.IsUnused()) return VisitUnused<T>(node);
        if (BothInputsAre(node, Type::Number())) {
          VisitBinop<T>(node, UseInfo::TruncatingFloat64(),
                        MachineRepresentation::kBit);
          if (lower<T>()) {
            NodeProperties::ChangeOp(node,
                                     lowering->simplified()->NumberSameValue());
          }
        } else {
          VisitBinop<T>(node, UseInfo::AnyTagged(),
                        MachineRepresentation::kTaggedPointer);
        }
        return;
      }
      case IrOpcode::kTypeOf: {
        return VisitUnop<T>(node, UseInfo::AnyTagged(),
                            MachineRepresentation::kTaggedPointer);
      }
      case IrOpcode::kNewConsString: {
        ProcessInput<T>(node, 0, UseInfo::TruncatingWord32());  // length
        ProcessInput<T>(node, 1, UseInfo::AnyTagged());         // first
        ProcessInput<T>(node, 2, UseInfo::AnyTagged());         // second
        SetOutput<T>(node, MachineRepresentation::kTaggedPointer);
        return;
      }
      case IrOpcode::kSpeculativeBigIntAdd: {
        // TODO(nicohartmann@, chromium:1073440): There should be special
        // handling for trunction.IsUnused() that correctly propagates deadness,
        // but preserves type checking which may throw exceptions. Until this
        // is fully supported, we lower to int64 operations but keep pushing
        // type constraints.
        if (truncation.IsUsedAsWord64()) {
          VisitBinop<T>(
              node, UseInfo::CheckedBigIntTruncatingWord64(FeedbackSource{}),
              MachineRepresentation::kWord64);
          if (lower<T>()) {
            ChangeToPureOp(node, lowering->machine()->Int64Add());
          }
        } else {
          VisitBinop<T>(node,
                        UseInfo::CheckedBigIntAsTaggedPointer(FeedbackSource{}),
                        MachineRepresentation::kTaggedPointer);
          if (lower<T>()) {
            NodeProperties::ChangeOp(node, lowering->simplified()->BigIntAdd());
          }
        }
        return;
      }
      case IrOpcode::kSpeculativeBigIntSubtract: {
        if (truncation.IsUsedAsWord64()) {
          VisitBinop<T>(
              node, UseInfo::CheckedBigIntTruncatingWord64(FeedbackSource{}),
              MachineRepresentation::kWord64);
          if (lower<T>()) {
            ChangeToPureOp(node, lowering->machine()->Int64Sub());
          }
        } else {
          VisitBinop<T>(node,
                        UseInfo::CheckedBigIntAsTaggedPointer(FeedbackSource{}),
                        MachineRepresentation::kTaggedPointer);
          if (lower<T>()) {
            NodeProperties::ChangeOp(node,
                                     lowering->simplified()->BigIntSubtract());
          }
        }
        return;
      }
      case IrOpcode::kSpeculativeBigIntNegate: {
        if (truncation.IsUsedAsWord64()) {
          VisitUnop<T>(node,
                       UseInfo::CheckedBigIntTruncatingWord64(FeedbackSource{}),
                       MachineRepresentation::kWord64);
          if (lower<T>()) {
            ChangeUnaryToPureBinaryOp(node, lowering->machine()->Int64Sub(), 0,
                                      jsgraph_->Int64Constant(0));
          }
        } else {
          VisitUnop<T>(node,
                       UseInfo::CheckedBigIntAsTaggedPointer(FeedbackSource{}),
                       MachineRepresentation::kTaggedPointer);
          if (lower<T>()) {
            ChangeToPureOp(node, lowering->simplified()->BigIntNegate());
          }
        }
        return;
      }
      case IrOpcode::kStringConcat: {
        // TODO(turbofan): We currently depend on having this first length input
        // to make sure that the overflow check is properly scheduled before the
        // actual string concatenation. We should also use the length to pass it
        // to the builtin or decide in optimized code how to construct the
        // resulting string (i.e. cons string or sequential string).
        ProcessInput<T>(node, 0, UseInfo::TaggedSigned());  // length
        ProcessInput<T>(node, 1, UseInfo::AnyTagged());     // first
        ProcessInput<T>(node, 2, UseInfo::AnyTagged());     // second
        SetOutput<T>(node, MachineRepresentation::kTaggedPointer);
        return;
      }
      case IrOpcode::kStringEqual:
      case IrOpcode::kStringLessThan:
      case IrOpcode::kStringLessThanOrEqual: {
        return VisitBinop<T>(node, UseInfo::AnyTagged(),
                             MachineRepresentation::kTaggedPointer);
      }
      case IrOpcode::kStringCharCodeAt: {
        return VisitBinop<T>(node, UseInfo::AnyTagged(), UseInfo::Word(),
                             MachineRepresentation::kWord32);
      }
      case IrOpcode::kStringCodePointAt: {
        return VisitBinop<T>(node, UseInfo::AnyTagged(), UseInfo::Word(),
                             MachineRepresentation::kTaggedSigned);
      }
      case IrOpcode::kStringFromSingleCharCode: {
        VisitUnop<T>(node, UseInfo::TruncatingWord32(),
                     MachineRepresentation::kTaggedPointer);
        return;
      }
      case IrOpcode::kStringFromSingleCodePoint: {
        VisitUnop<T>(node, UseInfo::TruncatingWord32(),
                     MachineRepresentation::kTaggedPointer);
        return;
      }
      case IrOpcode::kStringFromCodePointAt: {
        return VisitBinop<T>(node, UseInfo::AnyTagged(), UseInfo::Word(),
                             MachineRepresentation::kTaggedPointer);
      }
      case IrOpcode::kStringIndexOf: {
        ProcessInput<T>(node, 0, UseInfo::AnyTagged());
        ProcessInput<T>(node, 1, UseInfo::AnyTagged());
        ProcessInput<T>(node, 2, UseInfo::TaggedSigned());
        SetOutput<T>(node, MachineRepresentation::kTaggedSigned);
        return;
      }
      case IrOpcode::kStringLength: {
        // TODO(bmeurer): The input representation should be TaggedPointer.
        // Fix this once we have a dedicated StringConcat/JSStringAdd
        // operator, which marks it's output as TaggedPointer properly.
        VisitUnop<T>(node, UseInfo::AnyTagged(),
                     MachineRepresentation::kWord32);
        return;
      }
      case IrOpcode::kStringSubstring: {
        ProcessInput<T>(node, 0, UseInfo::AnyTagged());
        ProcessInput<T>(node, 1, UseInfo::TruncatingWord32());
        ProcessInput<T>(node, 2, UseInfo::TruncatingWord32());
        ProcessRemainingInputs<T>(node, 3);
        SetOutput<T>(node, MachineRepresentation::kTaggedPointer);
        return;
      }
      case IrOpcode::kStringToLowerCaseIntl:
      case IrOpcode::kStringToUpperCaseIntl: {
        VisitUnop<T>(node, UseInfo::AnyTagged(),
                     MachineRepresentation::kTaggedPointer);
        return;
      }
      case IrOpcode::kCheckBounds:
        return VisitCheckBounds<T>(node, lowering);
      case IrOpcode::kPoisonIndex: {
        VisitUnop<T>(node, UseInfo::TruncatingWord32(),
                     MachineRepresentation::kWord32);
        return;
      }
      case IrOpcode::kCheckHeapObject: {
        if (InputCannotBe(node, Type::SignedSmall())) {
          VisitUnop<T>(node, UseInfo::AnyTagged(),
                       MachineRepresentation::kTaggedPointer);
        } else {
          VisitUnop<T>(
              node, UseInfo::CheckedHeapObjectAsTaggedPointer(FeedbackSource()),
              MachineRepresentation::kTaggedPointer);
        }
        if (lower<T>()) DeferReplacement(node, node->InputAt(0));
        return;
      }
      case IrOpcode::kCheckIf: {
        ProcessInput<T>(node, 0, UseInfo::Bool());
        ProcessRemainingInputs<T>(node, 1);
        SetOutput<T>(node, MachineRepresentation::kNone);
        return;
      }
      case IrOpcode::kCheckInternalizedString: {
        VisitCheck<T>(node, Type::InternalizedString(), lowering);
        return;
      }
      case IrOpcode::kCheckNumber: {
        Type const input_type = TypeOf(node->InputAt(0));
        if (input_type.Is(Type::Number())) {
          VisitNoop<T>(node, truncation);
        } else {
          VisitUnop<T>(node, UseInfo::AnyTagged(),
                       MachineRepresentation::kTagged);
        }
        return;
      }
      case IrOpcode::kCheckReceiver: {
        VisitCheck<T>(node, Type::Receiver(), lowering);
        return;
      }
      case IrOpcode::kCheckReceiverOrNullOrUndefined: {
        VisitCheck<T>(node, Type::ReceiverOrNullOrUndefined(), lowering);
        return;
      }
      case IrOpcode::kCheckSmi: {
        const CheckParameters& params = CheckParametersOf(node->op());
        if (SmiValuesAre32Bits() && truncation.IsUsedAsWord32()) {
          VisitUnop<T>(node,
                       UseInfo::CheckedSignedSmallAsWord32(kDistinguishZeros,
                                                           params.feedback()),
                       MachineRepresentation::kWord32);
        } else {
          VisitUnop<T>(
              node,
              UseInfo::CheckedSignedSmallAsTaggedSigned(params.feedback()),
              MachineRepresentation::kTaggedSigned);
        }
        if (lower<T>()) DeferReplacement(node, node->InputAt(0));
        return;
      }
      case IrOpcode::kCheckString: {
        const CheckParameters& params = CheckParametersOf(node->op());
        if (InputIs(node, Type::String())) {
          VisitUnop<T>(node, UseInfo::AnyTagged(),
                       MachineRepresentation::kTaggedPointer);
          if (lower<T>()) DeferReplacement(node, node->InputAt(0));
        } else {
          VisitUnop<T>(
              node,
              UseInfo::CheckedHeapObjectAsTaggedPointer(params.feedback()),
              MachineRepresentation::kTaggedPointer);
        }
        return;
      }
      case IrOpcode::kCheckSymbol: {
        VisitCheck<T>(node, Type::Symbol(), lowering);
        return;
      }

      case IrOpcode::kAllocate: {
        ProcessInput<T>(node, 0, UseInfo::Word());
        ProcessRemainingInputs<T>(node, 1);
        SetOutput<T>(node, MachineRepresentation::kTaggedPointer);
        return;
      }
      case IrOpcode::kLoadMessage: {
        if (truncation.IsUnused()) return VisitUnused<T>(node);
        VisitUnop<T>(node, UseInfo::Word(), MachineRepresentation::kTagged);
        return;
      }
      case IrOpcode::kStoreMessage: {
        ProcessInput<T>(node, 0, UseInfo::Word());
        ProcessInput<T>(node, 1, UseInfo::AnyTagged());
        ProcessRemainingInputs<T>(node, 2);
        SetOutput<T>(node, MachineRepresentation::kNone);
        return;
      }
      case IrOpcode::kLoadFieldByIndex: {
        if (truncation.IsUnused()) return VisitUnused<T>(node);
        VisitBinop<T>(node, UseInfo::AnyTagged(), UseInfo::TruncatingWord32(),
                      MachineRepresentation::kTagged);
        return;
      }
      case IrOpcode::kLoadField: {
        if (truncation.IsUnused()) return VisitUnused<T>(node);
        FieldAccess access = FieldAccessOf(node->op());
        MachineRepresentation const representation =
            access.machine_type.representation();
        VisitUnop<T>(node, UseInfoForBasePointer(access), representation);
        return;
      }
      case IrOpcode::kStoreField: {
        FieldAccess access = FieldAccessOf(node->op());
        Node* value_node = node->InputAt(1);
        NodeInfo* input_info = GetInfo(value_node);
        MachineRepresentation field_representation =
            access.machine_type.representation();

        // Convert to Smi if possible, such that we can avoid a write barrier.
        if (field_representation == MachineRepresentation::kTagged &&
            TypeOf(value_node).Is(Type::SignedSmall())) {
          field_representation = MachineRepresentation::kTaggedSigned;
        }
        WriteBarrierKind write_barrier_kind = WriteBarrierKindFor(
            access.base_is_tagged, field_representation, access.offset,
            access.type, input_info->representation(), value_node);

        ProcessInput<T>(node, 0, UseInfoForBasePointer(access));
        ProcessInput<T>(
            node, 1, TruncatingUseInfoFromRepresentation(field_representation));
        ProcessRemainingInputs<T>(node, 2);
        SetOutput<T>(node, MachineRepresentation::kNone);
        if (lower<T>()) {
          if (write_barrier_kind < access.write_barrier_kind) {
            access.write_barrier_kind = write_barrier_kind;
            NodeProperties::ChangeOp(
                node, jsgraph_->simplified()->StoreField(access));
          }
        }
        return;
      }
      case IrOpcode::kLoadElement: {
        if (truncation.IsUnused()) return VisitUnused<T>(node);
        ElementAccess access = ElementAccessOf(node->op());
        VisitBinop<T>(node, UseInfoForBasePointer(access), UseInfo::Word(),
                      access.machine_type.representation());
        return;
      }
      case IrOpcode::kLoadStackArgument: {
        if (truncation.IsUnused()) return VisitUnused<T>(node);
        VisitBinop<T>(node, UseInfo::Word(), MachineRepresentation::kTagged);
        return;
      }
      case IrOpcode::kStoreElement: {
        ElementAccess access = ElementAccessOf(node->op());
        Node* value_node = node->InputAt(2);
        NodeInfo* input_info = GetInfo(value_node);
        MachineRepresentation element_representation =
            access.machine_type.representation();

        // Convert to Smi if possible, such that we can avoid a write barrier.
        if (element_representation == MachineRepresentation::kTagged &&
            TypeOf(value_node).Is(Type::SignedSmall())) {
          element_representation = MachineRepresentation::kTaggedSigned;
        }
        WriteBarrierKind write_barrier_kind = WriteBarrierKindFor(
            access.base_is_tagged, element_representation, access.type,
            input_info->representation(), value_node);
        ProcessInput<T>(node, 0, UseInfoForBasePointer(access));  // base
        ProcessInput<T>(node, 1, UseInfo::Word());                // index
        ProcessInput<T>(node, 2,
                        TruncatingUseInfoFromRepresentation(
                            element_representation));  // value
        ProcessRemainingInputs<T>(node, 3);
        SetOutput<T>(node, MachineRepresentation::kNone);
        if (lower<T>()) {
          if (write_barrier_kind < access.write_barrier_kind) {
            access.write_barrier_kind = write_barrier_kind;
            NodeProperties::ChangeOp(
                node, jsgraph_->simplified()->StoreElement(access));
          }
        }
        return;
      }
      case IrOpcode::kNumberIsFloat64Hole: {
        VisitUnop<T>(node, UseInfo::TruncatingFloat64(),
                     MachineRepresentation::kBit);
        return;
      }
      case IrOpcode::kTransitionAndStoreElement: {
        Type value_type = TypeOf(node->InputAt(2));

        ProcessInput<T>(node, 0, UseInfo::AnyTagged());  // array
        ProcessInput<T>(node, 1, UseInfo::Word());       // index

        if (value_type.Is(Type::SignedSmall())) {
          ProcessInput<T>(node, 2, UseInfo::TruncatingWord32());  // value
          if (lower<T>()) {
            NodeProperties::ChangeOp(node,
                                     simplified()->StoreSignedSmallElement());
          }
        } else if (value_type.Is(Type::Number())) {
          ProcessInput<T>(node, 2, UseInfo::TruncatingFloat64());  // value
          if (lower<T>()) {
            Handle<Map> double_map = DoubleMapParameterOf(node->op());
            NodeProperties::ChangeOp(
                node,
                simplified()->TransitionAndStoreNumberElement(double_map));
          }
        } else if (value_type.Is(Type::NonNumber())) {
          ProcessInput<T>(node, 2, UseInfo::AnyTagged());  // value
          if (lower<T>()) {
            Handle<Map> fast_map = FastMapParameterOf(node->op());
            NodeProperties::ChangeOp(
                node, simplified()->TransitionAndStoreNonNumberElement(
                          fast_map, value_type));
          }
        } else {
          ProcessInput<T>(node, 2, UseInfo::AnyTagged());  // value
        }

        ProcessRemainingInputs<T>(node, 3);
        SetOutput<T>(node, MachineRepresentation::kNone);
        return;
      }
      case IrOpcode::kLoadTypedElement: {
        MachineRepresentation const rep =
            MachineRepresentationFromArrayType(ExternalArrayTypeOf(node->op()));
        ProcessInput<T>(node, 0, UseInfo::AnyTagged());  // buffer
        ProcessInput<T>(node, 1, UseInfo::AnyTagged());  // base pointer
        ProcessInput<T>(node, 2, UseInfo::Word());       // external pointer
        ProcessInput<T>(node, 3, UseInfo::Word());       // index
        ProcessRemainingInputs<T>(node, 4);
        SetOutput<T>(node, rep);
        return;
      }
      case IrOpcode::kLoadDataViewElement: {
        MachineRepresentation const rep =
            MachineRepresentationFromArrayType(ExternalArrayTypeOf(node->op()));
        ProcessInput<T>(node, 0, UseInfo::AnyTagged());  // object
        ProcessInput<T>(node, 1, UseInfo::Word());       // base
        ProcessInput<T>(node, 2, UseInfo::Word());       // index
        ProcessInput<T>(node, 3, UseInfo::Bool());       // little-endian
        ProcessRemainingInputs<T>(node, 4);
        SetOutput<T>(node, rep);
        return;
      }
      case IrOpcode::kStoreTypedElement: {
        MachineRepresentation const rep =
            MachineRepresentationFromArrayType(ExternalArrayTypeOf(node->op()));
        ProcessInput<T>(node, 0, UseInfo::AnyTagged());  // buffer
        ProcessInput<T>(node, 1, UseInfo::AnyTagged());  // base pointer
        ProcessInput<T>(node, 2, UseInfo::Word());       // external pointer
        ProcessInput<T>(node, 3, UseInfo::Word());       // index
        ProcessInput<T>(node, 4,
                        TruncatingUseInfoFromRepresentation(rep));  // value
        ProcessRemainingInputs<T>(node, 5);
        SetOutput<T>(node, MachineRepresentation::kNone);
        return;
      }
      case IrOpcode::kStoreDataViewElement: {
        MachineRepresentation const rep =
            MachineRepresentationFromArrayType(ExternalArrayTypeOf(node->op()));
        ProcessInput<T>(node, 0, UseInfo::AnyTagged());  // object
        ProcessInput<T>(node, 1, UseInfo::Word());       // base
        ProcessInput<T>(node, 2, UseInfo::Word());       // index
        ProcessInput<T>(node, 3,
                        TruncatingUseInfoFromRepresentation(rep));  // value
        ProcessInput<T>(node, 4, UseInfo::Bool());  // little-endian
        ProcessRemainingInputs<T>(node, 5);
        SetOutput<T>(node, MachineRepresentation::kNone);
        return;
      }
      case IrOpcode::kConvertReceiver: {
        Type input_type = TypeOf(node->InputAt(0));
        VisitBinop<T>(node, UseInfo::AnyTagged(),
                      MachineRepresentation::kTaggedPointer);
        if (lower<T>()) {
          // Try to optimize the {node} based on the input type.
          if (input_type.Is(Type::Receiver())) {
            DeferReplacement(node, node->InputAt(0));
          } else if (input_type.Is(Type::NullOrUndefined())) {
            DeferReplacement(node, node->InputAt(1));
          } else if (!input_type.Maybe(Type::NullOrUndefined())) {
            NodeProperties::ChangeOp(
                node, lowering->simplified()->ConvertReceiver(
                          ConvertReceiverMode::kNotNullOrUndefined));
          }
        }
        return;
      }
      case IrOpcode::kPlainPrimitiveToNumber: {
        if (InputIs(node, Type::Boolean())) {
          VisitUnop<T>(node, UseInfo::Bool(), MachineRepresentation::kWord32);
          if (lower<T>()) DeferReplacement(node, node->InputAt(0));
        } else if (InputIs(node, Type::String())) {
          VisitUnop<T>(node, UseInfo::AnyTagged(),
                       MachineRepresentation::kTagged);
          if (lower<T>()) {
            NodeProperties::ChangeOp(node, simplified()->StringToNumber());
          }
        } else if (truncation.IsUsedAsWord32()) {
          if (InputIs(node, Type::NumberOrOddball())) {
            VisitUnop<T>(node, UseInfo::TruncatingWord32(),
                         MachineRepresentation::kWord32);
            if (lower<T>()) DeferReplacement(node, node->InputAt(0));
          } else {
            VisitUnop<T>(node, UseInfo::AnyTagged(),
                         MachineRepresentation::kWord32);
            if (lower<T>()) {
              NodeProperties::ChangeOp(node,
                                       simplified()->PlainPrimitiveToWord32());
            }
          }
        } else if (truncation.TruncatesOddballAndBigIntToNumber()) {
          if (InputIs(node, Type::NumberOrOddball())) {
            VisitUnop<T>(node, UseInfo::TruncatingFloat64(),
                         MachineRepresentation::kFloat64);
            if (lower<T>()) DeferReplacement(node, node->InputAt(0));
          } else {
            VisitUnop<T>(node, UseInfo::AnyTagged(),
                         MachineRepresentation::kFloat64);
            if (lower<T>()) {
              NodeProperties::ChangeOp(node,
                                       simplified()->PlainPrimitiveToFloat64());
            }
          }
        } else {
          VisitUnop<T>(node, UseInfo::AnyTagged(),
                       MachineRepresentation::kTagged);
        }
        return;
      }
      case IrOpcode::kSpeculativeToNumber: {
        NumberOperationParameters const& p =
            NumberOperationParametersOf(node->op());
        switch (p.hint()) {
          case NumberOperationHint::kSigned32:
          case NumberOperationHint::kSignedSmall:
          case NumberOperationHint::kSignedSmallInputs:
            VisitUnop<T>(node,
                         CheckedUseInfoAsWord32FromHint(p.hint(), p.feedback()),
                         MachineRepresentation::kWord32, Type::Signed32());
            break;
          case NumberOperationHint::kNumber:
          case NumberOperationHint::kNumberOrBoolean:
          case NumberOperationHint::kNumberOrOddball:
            VisitUnop<T>(
                node, CheckedUseInfoAsFloat64FromHint(p.hint(), p.feedback()),
                MachineRepresentation::kFloat64);
            break;
        }
        if (lower<T>()) DeferReplacement(node, node->InputAt(0));
        return;
      }
      case IrOpcode::kObjectIsArrayBufferView: {
        // TODO(turbofan): Introduce a Type::ArrayBufferView?
        VisitUnop<T>(node, UseInfo::AnyTagged(), MachineRepresentation::kBit);
        return;
      }
      case IrOpcode::kObjectIsBigInt: {
        VisitObjectIs<T>(node, Type::BigInt(), lowering);
        return;
      }
      case IrOpcode::kObjectIsCallable: {
        VisitObjectIs<T>(node, Type::Callable(), lowering);
        return;
      }
      case IrOpcode::kObjectIsConstructor: {
        // TODO(turbofan): Introduce a Type::Constructor?
        VisitUnop<T>(node, UseInfo::AnyTagged(), MachineRepresentation::kBit);
        return;
      }
      case IrOpcode::kObjectIsDetectableCallable: {
        VisitObjectIs<T>(node, Type::DetectableCallable(), lowering);
        return;
      }
      case IrOpcode::kObjectIsFiniteNumber: {
        Type const input_type = GetUpperBound(node->InputAt(0));
        if (input_type.Is(type_cache_->kSafeInteger)) {
          VisitUnop<T>(node, UseInfo::None(), MachineRepresentation::kBit);
          if (lower<T>()) {
            DeferReplacement(node, lowering->jsgraph()->Int32Constant(1));
          }
        } else if (!input_type.Maybe(Type::Number())) {
          VisitUnop<T>(node, UseInfo::Any(), MachineRepresentation::kBit);
          if (lower<T>()) {
            DeferReplacement(node, lowering->jsgraph()->Int32Constant(0));
          }
        } else if (input_type.Is(Type::Number())) {
          VisitUnop<T>(node, UseInfo::TruncatingFloat64(),
                       MachineRepresentation::kBit);
          if (lower<T>()) {
            NodeProperties::ChangeOp(node,
                                     lowering->simplified()->NumberIsFinite());
          }
        } else {
          VisitUnop<T>(node, UseInfo::AnyTagged(), MachineRepresentation::kBit);
        }
        return;
      }
      case IrOpcode::kNumberIsFinite: {
        VisitUnop<T>(node, UseInfo::TruncatingFloat64(),
                     MachineRepresentation::kBit);
        return;
      }
      case IrOpcode::kObjectIsSafeInteger: {
        Type const input_type = GetUpperBound(node->InputAt(0));
        if (input_type.Is(type_cache_->kSafeInteger)) {
          VisitUnop<T>(node, UseInfo::None(), MachineRepresentation::kBit);
          if (lower<T>()) {
            DeferReplacement(node, lowering->jsgraph()->Int32Constant(1));
          }
        } else if (!input_type.Maybe(Type::Number())) {
          VisitUnop<T>(node, UseInfo::Any(), MachineRepresentation::kBit);
          if (lower<T>()) {
            DeferReplacement(node, lowering->jsgraph()->Int32Constant(0));
          }
        } else if (input_type.Is(Type::Number())) {
          VisitUnop<T>(node, UseInfo::TruncatingFloat64(),
                       MachineRepresentation::kBit);
          if (lower<T>()) {
            NodeProperties::ChangeOp(
                node, lowering->simplified()->NumberIsSafeInteger());
          }
        } else {
          VisitUnop<T>(node, UseInfo::AnyTagged(), MachineRepresentation::kBit);
        }
        return;
      }
      case IrOpcode::kNumberIsSafeInteger: {
        UNREACHABLE();
      }
      case IrOpcode::kObjectIsInteger: {
        Type const input_type = GetUpperBound(node->InputAt(0));
        if (input_type.Is(type_cache_->kSafeInteger)) {
          VisitUnop<T>(node, UseInfo::None(), MachineRepresentation::kBit);
          if (lower<T>()) {
            DeferReplacement(node, lowering->jsgraph()->Int32Constant(1));
          }
        } else if (!input_type.Maybe(Type::Number())) {
          VisitUnop<T>(node, UseInfo::Any(), MachineRepresentation::kBit);
          if (lower<T>()) {
            DeferReplacement(node, lowering->jsgraph()->Int32Constant(0));
          }
        } else if (input_type.Is(Type::Number())) {
          VisitUnop<T>(node, UseInfo::TruncatingFloat64(),
                       MachineRepresentation::kBit);
          if (lower<T>()) {
            NodeProperties::ChangeOp(node,
                                     lowering->simplified()->NumberIsInteger());
          }
        } else {
          VisitUnop<T>(node, UseInfo::AnyTagged(), MachineRepresentation::kBit);
        }
        return;
      }
      case IrOpcode::kNumberIsInteger: {
        VisitUnop<T>(node, UseInfo::TruncatingFloat64(),
                     MachineRepresentation::kBit);
        return;
      }
      case IrOpcode::kObjectIsMinusZero: {
        Type const input_type = GetUpperBound(node->InputAt(0));
        if (input_type.Is(Type::MinusZero())) {
          VisitUnop<T>(node, UseInfo::None(), MachineRepresentation::kBit);
          if (lower<T>()) {
            DeferReplacement(node, lowering->jsgraph()->Int32Constant(1));
          }
        } else if (!input_type.Maybe(Type::MinusZero())) {
          VisitUnop<T>(node, UseInfo::Any(), MachineRepresentation::kBit);
          if (lower<T>()) {
            DeferReplacement(node, lowering->jsgraph()->Int32Constant(0));
          }
        } else if (input_type.Is(Type::Number())) {
          VisitUnop<T>(node, UseInfo::TruncatingFloat64(),
                       MachineRepresentation::kBit);
          if (lower<T>()) {
            NodeProperties::ChangeOp(node, simplified()->NumberIsMinusZero());
          }
        } else {
          VisitUnop<T>(node, UseInfo::AnyTagged(), MachineRepresentation::kBit);
        }
        return;
      }
      case IrOpcode::kObjectIsNaN: {
        Type const input_type = GetUpperBound(node->InputAt(0));
        if (input_type.Is(Type::NaN())) {
          VisitUnop<T>(node, UseInfo::None(), MachineRepresentation::kBit);
          if (lower<T>()) {
            DeferReplacement(node, lowering->jsgraph()->Int32Constant(1));
          }
        } else if (!input_type.Maybe(Type::NaN())) {
          VisitUnop<T>(node, UseInfo::Any(), MachineRepresentation::kBit);
          if (lower<T>()) {
            DeferReplacement(node, lowering->jsgraph()->Int32Constant(0));
          }
        } else if (input_type.Is(Type::Number())) {
          VisitUnop<T>(node, UseInfo::TruncatingFloat64(),
                       MachineRepresentation::kBit);
          if (lower<T>()) {
            NodeProperties::ChangeOp(node, simplified()->NumberIsNaN());
          }
        } else {
          VisitUnop<T>(node, UseInfo::AnyTagged(), MachineRepresentation::kBit);
        }
        return;
      }
      case IrOpcode::kNumberIsNaN: {
        VisitUnop<T>(node, UseInfo::TruncatingFloat64(),
                     MachineRepresentation::kBit);
        return;
      }
      case IrOpcode::kObjectIsNonCallable: {
        VisitObjectIs<T>(node, Type::NonCallable(), lowering);
        return;
      }
      case IrOpcode::kObjectIsNumber: {
        VisitObjectIs<T>(node, Type::Number(), lowering);
        return;
      }
      case IrOpcode::kObjectIsReceiver: {
        VisitObjectIs<T>(node, Type::Receiver(), lowering);
        return;
      }
      case IrOpcode::kObjectIsSmi: {
        // TODO(turbofan): Optimize based on input representation.
        VisitUnop<T>(node, UseInfo::AnyTagged(), MachineRepresentation::kBit);
        return;
      }
      case IrOpcode::kObjectIsString: {
        VisitObjectIs<T>(node, Type::String(), lowering);
        return;
      }
      case IrOpcode::kObjectIsSymbol: {
        VisitObjectIs<T>(node, Type::Symbol(), lowering);
        return;
      }
      case IrOpcode::kObjectIsUndetectable: {
        VisitObjectIs<T>(node, Type::Undetectable(), lowering);
        return;
      }
      case IrOpcode::kArgumentsFrame: {
        SetOutput<T>(node, MachineType::PointerRepresentation());
        return;
      }
      case IrOpcode::kArgumentsLength: {
        VisitUnop<T>(node, UseInfo::Word(),
                     MachineRepresentation::kTaggedSigned);
        return;
      }
      case IrOpcode::kNewDoubleElements:
      case IrOpcode::kNewSmiOrObjectElements: {
        VisitUnop<T>(node, UseInfo::Word(),
                     MachineRepresentation::kTaggedPointer);
        return;
      }
      case IrOpcode::kNewArgumentsElements: {
        VisitBinop<T>(node, UseInfo::Word(), UseInfo::TaggedSigned(),
                      MachineRepresentation::kTaggedPointer);
        return;
      }
      case IrOpcode::kCheckFloat64Hole: {
        Type const input_type = TypeOf(node->InputAt(0));
        CheckFloat64HoleMode mode =
            CheckFloat64HoleParametersOf(node->op()).mode();
        if (mode == CheckFloat64HoleMode::kAllowReturnHole) {
          // If {mode} is allow-return-hole _and_ the {truncation}
          // identifies NaN and undefined, we can just pass along
          // the {truncation} and completely wipe the {node}.
          if (truncation.IsUnused()) return VisitUnused<T>(node);
          if (truncation.TruncatesOddballAndBigIntToNumber()) {
            VisitUnop<T>(node, UseInfo::TruncatingFloat64(),
                         MachineRepresentation::kFloat64);
            if (lower<T>()) DeferReplacement(node, node->InputAt(0));
            return;
          }
        }
        VisitUnop<T>(
            node, UseInfo(MachineRepresentation::kFloat64, Truncation::Any()),
            MachineRepresentation::kFloat64, Type::Number());
        if (lower<T>() && input_type.Is(Type::Number())) {
          DeferReplacement(node, node->InputAt(0));
        }
        return;
      }
      case IrOpcode::kCheckNotTaggedHole: {
        VisitUnop<T>(node, UseInfo::AnyTagged(),
                     MachineRepresentation::kTagged);
        return;
      }
      case IrOpcode::kCheckClosure: {
        VisitUnop<T>(
            node, UseInfo::CheckedHeapObjectAsTaggedPointer(FeedbackSource()),
            MachineRepresentation::kTaggedPointer);
        return;
      }
      case IrOpcode::kConvertTaggedHoleToUndefined: {
        if (InputIs(node, Type::NumberOrOddball()) &&
            truncation.IsUsedAsWord32()) {
          // Propagate the Word32 truncation.
          VisitUnop<T>(node, UseInfo::TruncatingWord32(),
                       MachineRepresentation::kWord32);
          if (lower<T>()) DeferReplacement(node, node->InputAt(0));
        } else if (InputIs(node, Type::NumberOrOddball()) &&
                   truncation.TruncatesOddballAndBigIntToNumber()) {
          // Propagate the Float64 truncation.
          VisitUnop<T>(node, UseInfo::TruncatingFloat64(),
                       MachineRepresentation::kFloat64);
          if (lower<T>()) DeferReplacement(node, node->InputAt(0));
        } else if (InputIs(node, Type::NonInternal())) {
          VisitUnop<T>(node, UseInfo::AnyTagged(),
                       MachineRepresentation::kTagged);
          if (lower<T>()) DeferReplacement(node, node->InputAt(0));
        } else {
          // TODO(turbofan): Add a (Tagged) truncation that identifies hole
          // and undefined, i.e. for a[i] === obj cases.
          VisitUnop<T>(node, UseInfo::AnyTagged(),
                       MachineRepresentation::kTagged);
        }
        return;
      }
      case IrOpcode::kCheckEqualsSymbol:
      case IrOpcode::kCheckEqualsInternalizedString:
        return VisitBinop<T>(node, UseInfo::AnyTagged(),
                             MachineRepresentation::kNone);
      case IrOpcode::kMapGuard:
        // Eliminate MapGuard nodes here.
        return VisitUnused<T>(node);
      case IrOpcode::kCheckMaps: {
        CheckMapsParameters const& p = CheckMapsParametersOf(node->op());
        return VisitUnop<T>(
            node, UseInfo::CheckedHeapObjectAsTaggedPointer(p.feedback()),
            MachineRepresentation::kNone);
      }
      case IrOpcode::kDynamicCheckMaps: {
        return VisitUnop<T>(
            node, UseInfo::CheckedHeapObjectAsTaggedPointer(FeedbackSource()),
            MachineRepresentation::kNone);
      }
      case IrOpcode::kTransitionElementsKind: {
        return VisitUnop<T>(
            node, UseInfo::CheckedHeapObjectAsTaggedPointer(FeedbackSource()),
            MachineRepresentation::kNone);
      }
      case IrOpcode::kCompareMaps:
        return VisitUnop<T>(
            node, UseInfo::CheckedHeapObjectAsTaggedPointer(FeedbackSource()),
            MachineRepresentation::kBit);
      case IrOpcode::kEnsureWritableFastElements:
        return VisitBinop<T>(node, UseInfo::AnyTagged(),
                             MachineRepresentation::kTaggedPointer);
      case IrOpcode::kMaybeGrowFastElements: {
        ProcessInput<T>(node, 0, UseInfo::AnyTagged());         // object
        ProcessInput<T>(node, 1, UseInfo::AnyTagged());         // elements
        ProcessInput<T>(node, 2, UseInfo::TruncatingWord32());  // index
        ProcessInput<T>(node, 3, UseInfo::TruncatingWord32());  // length
        ProcessRemainingInputs<T>(node, 4);
        SetOutput<T>(node, MachineRepresentation::kTaggedPointer);
        return;
      }

      case IrOpcode::kDateNow:
        VisitInputs<T>(node);
        return SetOutput<T>(node, MachineRepresentation::kTaggedPointer);
      case IrOpcode::kFrameState:
        return VisitFrameState<T>(node);
      case IrOpcode::kStateValues:
        return VisitStateValues<T>(node);
      case IrOpcode::kObjectState:
        return VisitObjectState<T>(node);
      case IrOpcode::kObjectId:
        return SetOutput<T>(node, MachineRepresentation::kTaggedPointer);

      case IrOpcode::kTypeGuard: {
        // We just get rid of the sigma here, choosing the best representation
        // for the sigma's type.
        Type type = TypeOf(node);
        MachineRepresentation representation =
            GetOutputInfoForPhi(node, type, truncation);

        // Here we pretend that the input has the sigma's type for the
        // conversion.
        UseInfo use(representation, truncation);
        if (propagate<T>()) {
          EnqueueInput<T>(node, 0, use);
        } else if (lower<T>()) {
          ConvertInput(node, 0, use, type);
        }
        ProcessRemainingInputs<T>(node, 1);
        SetOutput<T>(node, representation);
        return;
      }

      case IrOpcode::kFoldConstant:
        VisitInputs<T>(node);
        return SetOutput<T>(node, MachineRepresentation::kTaggedPointer);

      case IrOpcode::kFinishRegion:
        VisitInputs<T>(node);
        // Assume the output is tagged pointer.
        return SetOutput<T>(node, MachineRepresentation::kTaggedPointer);

      case IrOpcode::kReturn:
        VisitReturn<T>(node);
        // Assume the output is tagged.
        return SetOutput<T>(node, MachineRepresentation::kTagged);

      case IrOpcode::kFindOrderedHashMapEntry: {
        Type const key_type = TypeOf(node->InputAt(1));
        if (key_type.Is(Type::Signed32OrMinusZero())) {
          VisitBinop<T>(node, UseInfo::AnyTagged(), UseInfo::TruncatingWord32(),
                        MachineType::PointerRepresentation());
          if (lower<T>()) {
            NodeProperties::ChangeOp(
                node,
                lowering->simplified()->FindOrderedHashMapEntryForInt32Key());
          }
        } else {
          VisitBinop<T>(node, UseInfo::AnyTagged(),
                        MachineRepresentation::kTaggedSigned);
        }
        return;
      }

      case IrOpcode::kFastApiCall: {
        VisitFastApiCall<T>(node, lowering);
        return;
      }

      // Operators with all inputs tagged and no or tagged output have uniform
      // handling.
      case IrOpcode::kEnd:
      case IrOpcode::kIfSuccess:
      case IrOpcode::kIfException:
      case IrOpcode::kIfTrue:
      case IrOpcode::kIfFalse:
      case IrOpcode::kIfValue:
      case IrOpcode::kIfDefault:
      case IrOpcode::kDeoptimize:
      case IrOpcode::kEffectPhi:
      case IrOpcode::kTerminate:
      case IrOpcode::kCheckpoint:
      case IrOpcode::kLoop:
      case IrOpcode::kMerge:
      case IrOpcode::kThrow:
      case IrOpcode::kBeginRegion:
      case IrOpcode::kProjection:
      case IrOpcode::kOsrValue:
      case IrOpcode::kArgumentsElementsState:
      case IrOpcode::kArgumentsLengthState:
      case IrOpcode::kUnreachable:
      case IrOpcode::kRuntimeAbort:
// All JavaScript operators except JSToNumber have uniform handling.
#define OPCODE_CASE(name, ...) case IrOpcode::k##name:
        JS_SIMPLE_BINOP_LIST(OPCODE_CASE)
        JS_OBJECT_OP_LIST(OPCODE_CASE)
        JS_CONTEXT_OP_LIST(OPCODE_CASE)
        JS_OTHER_OP_LIST(OPCODE_CASE)
#undef OPCODE_CASE
      case IrOpcode::kJSBitwiseNot:
      case IrOpcode::kJSDecrement:
      case IrOpcode::kJSIncrement:
      case IrOpcode::kJSNegate:
      case IrOpcode::kJSToLength:
      case IrOpcode::kJSToName:
      case IrOpcode::kJSToObject:
      case IrOpcode::kJSToString:
      case IrOpcode::kJSParseInt:
        VisitInputs<T>(node);
        // Assume the output is tagged.
        return SetOutput<T>(node, MachineRepresentation::kTagged);
      case IrOpcode::kDeadValue:
        ProcessInput<T>(node, 0, UseInfo::Any());
        return SetOutput<T>(node, MachineRepresentation::kNone);
      case IrOpcode::kStaticAssert:
        return VisitUnop<T>(node, UseInfo::Any(),
                            MachineRepresentation::kTagged);
      case IrOpcode::kAssertType:
        return VisitUnop<T>(node, UseInfo::AnyTagged(),
                            MachineRepresentation::kTagged);
      default:
        FATAL(
            "Representation inference: unsupported opcode %i (%s), node #%i\n.",
            node->opcode(), node->op()->mnemonic(), node->id());
        break;
    }
    UNREACHABLE();
  }

  void DeferReplacement(Node* node, Node* replacement) {
    TRACE("defer replacement #%d:%s with #%d:%s\n", node->id(),
          node->op()->mnemonic(), replacement->id(),
          replacement->op()->mnemonic());

    // Disconnect the node from effect and control chains, if necessary.
    if (node->op()->EffectInputCount() > 0) {
      DCHECK_LT(0, node->op()->ControlInputCount());
      // Disconnect the node from effect and control chains.
      Node* control = NodeProperties::GetControlInput(node);
      Node* effect = NodeProperties::GetEffectInput(node);
      ReplaceEffectControlUses(node, effect, control);
    }

    replacements_.push_back(node);
    replacements_.push_back(replacement);

    node->NullAllInputs();  // Node is now dead.
  }

  void Kill(Node* node) {
    TRACE("killing #%d:%s\n", node->id(), node->op()->mnemonic());

    if (node->op()->EffectInputCount() == 1) {
      DCHECK_LT(0, node->op()->ControlInputCount());
      // Disconnect the node from effect and control chains.
      Node* control = NodeProperties::GetControlInput(node);
      Node* effect = NodeProperties::GetEffectInput(node);
      ReplaceEffectControlUses(node, effect, control);
    } else {
      DCHECK_EQ(0, node->op()->EffectInputCount());
      DCHECK_EQ(0, node->op()->ControlOutputCount());
      DCHECK_EQ(0, node->op()->EffectOutputCount());
    }

    node->ReplaceUses(jsgraph_->Dead());

    node->NullAllInputs();  // The {node} is now dead.
  }

 private:
  JSGraph* jsgraph_;
  Zone* zone_;                      // Temporary zone.
  // Map from node to its uses that might need to be revisited.
  ZoneMap<Node*, ZoneVector<Node*>> might_need_revisit_;
  size_t const count_;              // number of nodes in the graph
  ZoneVector<NodeInfo> info_;       // node id -> usage information
#ifdef DEBUG
  ZoneVector<InputUseInfos> node_input_use_infos_;  // Debug information about
                                                    // requirements on inputs.
#endif                                              // DEBUG
  NodeVector replacements_;         // replacements to be done after lowering
  RepresentationChanger* changer_;  // for inserting representation changes
  ZoneQueue<Node*> revisit_queue_;  // Queue for revisiting nodes.

  struct NodeState {
    Node* node;
    int input_index;
  };
  NodeVector traversal_nodes_;  // Order in which to traverse the nodes.
  // TODO(danno): RepresentationSelector shouldn't know anything about the
  // source positions table, but must for now since there currently is no other
  // way to pass down source position information to nodes created during
  // lowering. Once this phase becomes a vanilla reducer, it should get source
  // position information via the SourcePositionWrapper like all other reducers.
  SourcePositionTable* source_positions_;
  NodeOriginTable* node_origins_;
  TypeCache const* type_cache_;
  OperationTyper op_typer_;  // helper for the feedback typer
  TickCounter* const tick_counter_;

  NodeInfo* GetInfo(Node* node) {
    DCHECK(node->id() < count_);
    return &info_[node->id()];
  }
  Zone* zone() { return zone_; }
  Zone* graph_zone() { return jsgraph_->zone(); }
};

// Template specializations

// Enqueue {use_node}'s {index} input if the {use_info} contains new information
// for that input node.
template <>
void RepresentationSelector::EnqueueInput<PROPAGATE>(Node* use_node, int index,
                                                     UseInfo use_info) {
  Node* node = use_node->InputAt(index);
  NodeInfo* info = GetInfo(node);
#ifdef DEBUG
  // Check monotonicity of input requirements.
  node_input_use_infos_[use_node->id()].SetAndCheckInput(use_node, index,
                                                         use_info);
#endif  // DEBUG
  if (info->unvisited()) {
    info->AddUse(use_info);
    TRACE("  initial #%i: %s\n", node->id(), info->truncation().description());
    return;
  }
  TRACE("   queue #%i?: %s\n", node->id(), info->truncation().description());
  if (info->AddUse(use_info)) {
    // New usage information for the node is available.
    if (!info->queued()) {
      DCHECK(info->visited());
      revisit_queue_.push(node);
      info->set_queued();
      TRACE("   added: %s\n", info->truncation().description());
    } else {
      TRACE(" inqueue: %s\n", info->truncation().description());
    }
  }
}

template <>
void RepresentationSelector::SetOutput<PROPAGATE>(
    Node* node, MachineRepresentation representation, Type restriction_type) {
  NodeInfo* const info = GetInfo(node);
  info->set_restriction_type(restriction_type);
}

template <>
void RepresentationSelector::SetOutput<RETYPE>(
    Node* node, MachineRepresentation representation, Type restriction_type) {
  NodeInfo* const info = GetInfo(node);
  DCHECK(info->restriction_type().Is(restriction_type));
  DCHECK(restriction_type.Is(info->restriction_type()));
  info->set_output(representation);
}

template <>
void RepresentationSelector::SetOutput<LOWER>(
    Node* node, MachineRepresentation representation, Type restriction_type) {
  NodeInfo* const info = GetInfo(node);
  DCHECK_EQ(info->representation(), representation);
  DCHECK(info->restriction_type().Is(restriction_type));
  DCHECK(restriction_type.Is(info->restriction_type()));
  USE(info);
}

template <>
void RepresentationSelector::ProcessInput<PROPAGATE>(Node* node, int index,
                                                     UseInfo use) {
  DCHECK_IMPLIES(use.type_check() != TypeCheckKind::kNone,
                 !node->op()->HasProperty(Operator::kNoDeopt) &&
                     node->op()->EffectInputCount() > 0);
  EnqueueInput<PROPAGATE>(node, index, use);
}

template <>
void RepresentationSelector::ProcessInput<RETYPE>(Node* node, int index,
                                                  UseInfo use) {
  DCHECK_IMPLIES(use.type_check() != TypeCheckKind::kNone,
                 !node->op()->HasProperty(Operator::kNoDeopt) &&
                     node->op()->EffectInputCount() > 0);
}

template <>
void RepresentationSelector::ProcessInput<LOWER>(Node* node, int index,
                                                 UseInfo use) {
  DCHECK_IMPLIES(use.type_check() != TypeCheckKind::kNone,
                 !node->op()->HasProperty(Operator::kNoDeopt) &&
                     node->op()->EffectInputCount() > 0);
  ConvertInput(node, index, use);
}

template <>
void RepresentationSelector::ProcessRemainingInputs<PROPAGATE>(Node* node,
                                                               int index) {
  DCHECK_GE(index, NodeProperties::PastContextIndex(node));

  // Enqueue other inputs (effects, control).
  for (int i = std::max(index, NodeProperties::FirstEffectIndex(node));
       i < node->InputCount(); ++i) {
    EnqueueInput<PROPAGATE>(node, i);
  }
}

// The default, most general visitation case. For {node}, process all value,
// context, frame state, effect, and control inputs, assuming that value
// inputs should have {kRepTagged} representation and can observe all output
// values {kTypeAny}.
template <>
void RepresentationSelector::VisitInputs<PROPAGATE>(Node* node) {
  int first_effect_index = NodeProperties::FirstEffectIndex(node);
  // Visit value, context and frame state inputs as tagged.
  for (int i = 0; i < first_effect_index; i++) {
    ProcessInput<PROPAGATE>(node, i, UseInfo::AnyTagged());
  }
  // Only enqueue other inputs (effects, control).
  for (int i = first_effect_index; i < node->InputCount(); i++) {
    EnqueueInput<PROPAGATE>(node, i);
  }
}

template <>
void RepresentationSelector::VisitInputs<LOWER>(Node* node) {
  int first_effect_index = NodeProperties::FirstEffectIndex(node);
  // Visit value, context and frame state inputs as tagged.
  for (int i = 0; i < first_effect_index; i++) {
    ProcessInput<LOWER>(node, i, UseInfo::AnyTagged());
  }
}

template <>
void RepresentationSelector::InsertUnreachableIfNecessary<LOWER>(Node* node) {
  // If the node is effectful and it produces an impossible value, then we
  // insert Unreachable node after it.
  if (node->op()->ValueOutputCount() > 0 &&
      node->op()->EffectOutputCount() > 0 &&
      node->opcode() != IrOpcode::kUnreachable && TypeOf(node).IsNone()) {
    Node* control = (node->op()->ControlOutputCount() == 0)
                        ? NodeProperties::GetControlInput(node, 0)
                        : NodeProperties::FindSuccessfulControlProjection(node);

    Node* unreachable =
        graph()->NewNode(common()->Unreachable(), node, control);

    // Insert unreachable node and replace all the effect uses of the {node}
    // with the new unreachable node.
    for (Edge edge : node->use_edges()) {
      if (!NodeProperties::IsEffectEdge(edge)) continue;
      // Make sure to not overwrite the unreachable node's input. That would
      // create a cycle.
      if (edge.from() == unreachable) continue;
      // Avoid messing up the exceptional path.
      if (edge.from()->opcode() == IrOpcode::kIfException) {
        DCHECK(!node->op()->HasProperty(Operator::kNoThrow));
        DCHECK_EQ(NodeProperties::GetControlInput(edge.from()), node);
        continue;
      }

      edge.UpdateTo(unreachable);
    }
  }
}

SimplifiedLowering::SimplifiedLowering(JSGraph* jsgraph, JSHeapBroker* broker,
                                       Zone* zone,
                                       SourcePositionTable* source_positions,
                                       NodeOriginTable* node_origins,
                                       PoisoningMitigationLevel poisoning_level,
                                       TickCounter* tick_counter)
    : jsgraph_(jsgraph),
      broker_(broker),
      zone_(zone),
      type_cache_(TypeCache::Get()),
      source_positions_(source_positions),
      node_origins_(node_origins),
      poisoning_level_(poisoning_level),
      tick_counter_(tick_counter) {}

void SimplifiedLowering::LowerAllNodes() {
  RepresentationChanger changer(jsgraph(), broker_);
  RepresentationSelector selector(jsgraph(), broker_, zone_, &changer,
                                  source_positions_, node_origins_,
                                  tick_counter_);
  selector.Run(this);
}

void SimplifiedLowering::DoJSToNumberOrNumericTruncatesToFloat64(
    Node* node, RepresentationSelector* selector) {
  DCHECK(node->opcode() == IrOpcode::kJSToNumber ||
         node->opcode() == IrOpcode::kJSToNumberConvertBigInt ||
         node->opcode() == IrOpcode::kJSToNumeric);
  Node* value = node->InputAt(0);
  Node* context = node->InputAt(1);
  Node* frame_state = node->InputAt(2);
  Node* effect = node->InputAt(3);
  Node* control = node->InputAt(4);

  Node* check0 = graph()->NewNode(simplified()->ObjectIsSmi(), value);
  Node* branch0 =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check0, control);

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* etrue0 = effect;
  Node* vtrue0;
  {
    vtrue0 = graph()->NewNode(simplified()->ChangeTaggedSignedToInt32(), value);
    vtrue0 = graph()->NewNode(machine()->ChangeInt32ToFloat64(), vtrue0);
  }

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* efalse0 = effect;
  Node* vfalse0;
  {
    Operator const* op =
        node->opcode() == IrOpcode::kJSToNumber
            ? (node->opcode() == IrOpcode::kJSToNumberConvertBigInt
                   ? ToNumberConvertBigIntOperator()
                   : ToNumberOperator())
            : ToNumericOperator();
    Node* code = node->opcode() == IrOpcode::kJSToNumber
                     ? ToNumberCode()
                     : (node->opcode() == IrOpcode::kJSToNumberConvertBigInt
                            ? ToNumberConvertBigIntCode()
                            : ToNumericCode());
    vfalse0 = efalse0 = if_false0 = graph()->NewNode(
        op, code, value, context, frame_state, efalse0, if_false0);

    // Update potential {IfException} uses of {node} to point to the above
    // stub call node instead.
    Node* on_exception = nullptr;
    if (NodeProperties::IsExceptionalCall(node, &on_exception)) {
      NodeProperties::ReplaceControlInput(on_exception, vfalse0);
      NodeProperties::ReplaceEffectInput(on_exception, efalse0);
      if_false0 = graph()->NewNode(common()->IfSuccess(), vfalse0);
    }

    Node* check1 = graph()->NewNode(simplified()->ObjectIsSmi(), vfalse0);
    Node* branch1 = graph()->NewNode(common()->Branch(), check1, if_false0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* etrue1 = efalse0;
    Node* vtrue1;
    {
      vtrue1 =
          graph()->NewNode(simplified()->ChangeTaggedSignedToInt32(), vfalse0);
      vtrue1 = graph()->NewNode(machine()->ChangeInt32ToFloat64(), vtrue1);
    }

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* efalse1 = efalse0;
    Node* vfalse1;
    {
      vfalse1 = efalse1 = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForHeapNumberValue()), efalse0,
          efalse1, if_false1);
    }

    if_false0 = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
    efalse0 =
        graph()->NewNode(common()->EffectPhi(2), etrue1, efalse1, if_false0);
    vfalse0 =
        graph()->NewNode(common()->Phi(MachineRepresentation::kFloat64, 2),
                         vtrue1, vfalse1, if_false0);
  }

  control = graph()->NewNode(common()->Merge(2), if_true0, if_false0);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue0, efalse0, control);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kFloat64, 2),
                           vtrue0, vfalse0, control);

  // Replace effect and control uses appropriately.
  for (Edge edge : node->use_edges()) {
    if (NodeProperties::IsControlEdge(edge)) {
      if (edge.from()->opcode() == IrOpcode::kIfSuccess) {
        edge.from()->ReplaceUses(control);
        edge.from()->Kill();
      } else {
        DCHECK_NE(IrOpcode::kIfException, edge.from()->opcode());
        edge.UpdateTo(control);
      }
    } else if (NodeProperties::IsEffectEdge(edge)) {
      edge.UpdateTo(effect);
    }
  }

  selector->DeferReplacement(node, value);
}

void SimplifiedLowering::DoJSToNumberOrNumericTruncatesToWord32(
    Node* node, RepresentationSelector* selector) {
  DCHECK(node->opcode() == IrOpcode::kJSToNumber ||
         node->opcode() == IrOpcode::kJSToNumberConvertBigInt ||
         node->opcode() == IrOpcode::kJSToNumeric);
  Node* value = node->InputAt(0);
  Node* context = node->InputAt(1);
  Node* frame_state = node->InputAt(2);
  Node* effect = node->InputAt(3);
  Node* control = node->InputAt(4);

  Node* check0 = graph()->NewNode(simplified()->ObjectIsSmi(), value);
  Node* branch0 =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check0, control);

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* etrue0 = effect;
  Node* vtrue0 =
      graph()->NewNode(simplified()->ChangeTaggedSignedToInt32(), value);

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* efalse0 = effect;
  Node* vfalse0;
  {
    Operator const* op =
        node->opcode() == IrOpcode::kJSToNumber
            ? (node->opcode() == IrOpcode::kJSToNumberConvertBigInt
                   ? ToNumberConvertBigIntOperator()
                   : ToNumberOperator())
            : ToNumericOperator();
    Node* code = node->opcode() == IrOpcode::kJSToNumber
                     ? ToNumberCode()
                     : (node->opcode() == IrOpcode::kJSToNumberConvertBigInt
                            ? ToNumberConvertBigIntCode()
                            : ToNumericCode());
    vfalse0 = efalse0 = if_false0 = graph()->NewNode(
        op, code, value, context, frame_state, efalse0, if_false0);

    // Update potential {IfException} uses of {node} to point to the above
    // stub call node instead.
    Node* on_exception = nullptr;
    if (NodeProperties::IsExceptionalCall(node, &on_exception)) {
      NodeProperties::ReplaceControlInput(on_exception, vfalse0);
      NodeProperties::ReplaceEffectInput(on_exception, efalse0);
      if_false0 = graph()->NewNode(common()->IfSuccess(), vfalse0);
    }

    Node* check1 = graph()->NewNode(simplified()->ObjectIsSmi(), vfalse0);
    Node* branch1 = graph()->NewNode(common()->Branch(), check1, if_false0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* etrue1 = efalse0;
    Node* vtrue1 =
        graph()->NewNode(simplified()->ChangeTaggedSignedToInt32(), vfalse0);

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* efalse1 = efalse0;
    Node* vfalse1;
    {
      vfalse1 = efalse1 = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForHeapNumberValue()), efalse0,
          efalse1, if_false1);
      vfalse1 = graph()->NewNode(machine()->TruncateFloat64ToWord32(), vfalse1);
    }

    if_false0 = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
    efalse0 =
        graph()->NewNode(common()->EffectPhi(2), etrue1, efalse1, if_false0);
    vfalse0 = graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 2),
                               vtrue1, vfalse1, if_false0);
  }

  control = graph()->NewNode(common()->Merge(2), if_true0, if_false0);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue0, efalse0, control);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 2),
                           vtrue0, vfalse0, control);

  // Replace effect and control uses appropriately.
  for (Edge edge : node->use_edges()) {
    if (NodeProperties::IsControlEdge(edge)) {
      if (edge.from()->opcode() == IrOpcode::kIfSuccess) {
        edge.from()->ReplaceUses(control);
        edge.from()->Kill();
      } else {
        DCHECK_NE(IrOpcode::kIfException, edge.from()->opcode());
        edge.UpdateTo(control);
      }
    } else if (NodeProperties::IsEffectEdge(edge)) {
      edge.UpdateTo(effect);
    }
  }

  selector->DeferReplacement(node, value);
}

Node* SimplifiedLowering::Float64Round(Node* const node) {
  Node* const one = jsgraph()->Float64Constant(1.0);
  Node* const one_half = jsgraph()->Float64Constant(0.5);
  Node* const input = node->InputAt(0);

  // Round up towards Infinity, and adjust if the difference exceeds 0.5.
  Node* result = graph()->NewNode(machine()->Float64RoundUp().placeholder(),
                                  node->InputAt(0));
  return graph()->NewNode(
      common()->Select(MachineRepresentation::kFloat64),
      graph()->NewNode(
          machine()->Float64LessThanOrEqual(),
          graph()->NewNode(machine()->Float64Sub(), result, one_half), input),
      result, graph()->NewNode(machine()->Float64Sub(), result, one));
}

Node* SimplifiedLowering::Float64Sign(Node* const node) {
  Node* const minus_one = jsgraph()->Float64Constant(-1.0);
  Node* const zero = jsgraph()->Float64Constant(0.0);
  Node* const one = jsgraph()->Float64Constant(1.0);

  Node* const input = node->InputAt(0);

  return graph()->NewNode(
      common()->Select(MachineRepresentation::kFloat64),
      graph()->NewNode(machine()->Float64LessThan(), input, zero), minus_one,
      graph()->NewNode(
          common()->Select(MachineRepresentation::kFloat64),
          graph()->NewNode(machine()->Float64LessThan(), zero, input), one,
          input));
}

Node* SimplifiedLowering::Int32Abs(Node* const node) {
  Node* const input = node->InputAt(0);

  // Generate case for absolute integer value.
  //
  //    let sign = input >> 31 in
  //    (input ^ sign) - sign

  Node* sign = graph()->NewNode(machine()->Word32Sar(), input,
                                jsgraph()->Int32Constant(31));
  return graph()->NewNode(machine()->Int32Sub(),
                          graph()->NewNode(machine()->Word32Xor(), input, sign),
                          sign);
}

Node* SimplifiedLowering::Int32Div(Node* const node) {
  Int32BinopMatcher m(node);
  Node* const zero = jsgraph()->Int32Constant(0);
  Node* const minus_one = jsgraph()->Int32Constant(-1);
  Node* const lhs = m.left().node();
  Node* const rhs = m.right().node();

  if (m.right().Is(-1)) {
    return graph()->NewNode(machine()->Int32Sub(), zero, lhs);
  } else if (m.right().Is(0)) {
    return rhs;
  } else if (machine()->Int32DivIsSafe() || m.right().HasValue()) {
    return graph()->NewNode(machine()->Int32Div(), lhs, rhs, graph()->start());
  }

  // General case for signed integer division.
  //
  //    if 0 < rhs then
  //      lhs / rhs
  //    else
  //      if rhs < -1 then
  //        lhs / rhs
  //      else if rhs == 0 then
  //        0
  //      else
  //        0 - lhs
  //
  // Note: We do not use the Diamond helper class here, because it really hurts
  // readability with nested diamonds.
  const Operator* const merge_op = common()->Merge(2);
  const Operator* const phi_op =
      common()->Phi(MachineRepresentation::kWord32, 2);

  Node* check0 = graph()->NewNode(machine()->Int32LessThan(), zero, rhs);
  Node* branch0 = graph()->NewNode(common()->Branch(BranchHint::kTrue), check0,
                                   graph()->start());

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* true0 = graph()->NewNode(machine()->Int32Div(), lhs, rhs, if_true0);

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* false0;
  {
    Node* check1 = graph()->NewNode(machine()->Int32LessThan(), rhs, minus_one);
    Node* branch1 = graph()->NewNode(common()->Branch(), check1, if_false0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* true1 = graph()->NewNode(machine()->Int32Div(), lhs, rhs, if_true1);

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* false1;
    {
      Node* check2 = graph()->NewNode(machine()->Word32Equal(), rhs, zero);
      Node* branch2 = graph()->NewNode(common()->Branch(), check2, if_false1);

      Node* if_true2 = graph()->NewNode(common()->IfTrue(), branch2);
      Node* true2 = zero;

      Node* if_false2 = graph()->NewNode(common()->IfFalse(), branch2);
      Node* false2 = graph()->NewNode(machine()->Int32Sub(), zero, lhs);

      if_false1 = graph()->NewNode(merge_op, if_true2, if_false2);
      false1 = graph()->NewNode(phi_op, true2, false2, if_false1);
    }

    if_false0 = graph()->NewNode(merge_op, if_true1, if_false1);
    false0 = graph()->NewNode(phi_op, true1, false1, if_false0);
  }

  Node* merge0 = graph()->NewNode(merge_op, if_true0, if_false0);
  return graph()->NewNode(phi_op, true0, false0, merge0);
}

Node* SimplifiedLowering::Int32Mod(Node* const node) {
  Int32BinopMatcher m(node);
  Node* const zero = jsgraph()->Int32Constant(0);
  Node* const minus_one = jsgraph()->Int32Constant(-1);
  Node* const lhs = m.left().node();
  Node* const rhs = m.right().node();

  if (m.right().Is(-1) || m.right().Is(0)) {
    return zero;
  } else if (m.right().HasValue()) {
    return graph()->NewNode(machine()->Int32Mod(), lhs, rhs, graph()->start());
  }

  // General case for signed integer modulus, with optimization for (unknown)
  // power of 2 right hand side.
  //
  //   if 0 < rhs then
  //     msk = rhs - 1
  //     if rhs & msk != 0 then
  //       lhs % rhs
  //     else
  //       if lhs < 0 then
  //         -(-lhs & msk)
  //       else
  //         lhs & msk
  //   else
  //     if rhs < -1 then
  //       lhs % rhs
  //     else
  //       zero
  //
  // Note: We do not use the Diamond helper class here, because it really hurts
  // readability with nested diamonds.
  const Operator* const merge_op = common()->Merge(2);
  const Operator* const phi_op =
      common()->Phi(MachineRepresentation::kWord32, 2);

  Node* check0 = graph()->NewNode(machine()->Int32LessThan(), zero, rhs);
  Node* branch0 = graph()->NewNode(common()->Branch(BranchHint::kTrue), check0,
                                   graph()->start());

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* true0;
  {
    Node* msk = graph()->NewNode(machine()->Int32Add(), rhs, minus_one);

    Node* check1 = graph()->NewNode(machine()->Word32And(), rhs, msk);
    Node* branch1 = graph()->NewNode(common()->Branch(), check1, if_true0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* true1 = graph()->NewNode(machine()->Int32Mod(), lhs, rhs, if_true1);

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* false1;
    {
      Node* check2 = graph()->NewNode(machine()->Int32LessThan(), lhs, zero);
      Node* branch2 = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                       check2, if_false1);

      Node* if_true2 = graph()->NewNode(common()->IfTrue(), branch2);
      Node* true2 = graph()->NewNode(
          machine()->Int32Sub(), zero,
          graph()->NewNode(machine()->Word32And(),
                           graph()->NewNode(machine()->Int32Sub(), zero, lhs),
                           msk));

      Node* if_false2 = graph()->NewNode(common()->IfFalse(), branch2);
      Node* false2 = graph()->NewNode(machine()->Word32And(), lhs, msk);

      if_false1 = graph()->NewNode(merge_op, if_true2, if_false2);
      false1 = graph()->NewNode(phi_op, true2, false2, if_false1);
    }

    if_true0 = graph()->NewNode(merge_op, if_true1, if_false1);
    true0 = graph()->NewNode(phi_op, true1, false1, if_true0);
  }

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* false0;
  {
    Node* check1 = graph()->NewNode(machine()->Int32LessThan(), rhs, minus_one);
    Node* branch1 = graph()->NewNode(common()->Branch(BranchHint::kTrue),
                                     check1, if_false0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* true1 = graph()->NewNode(machine()->Int32Mod(), lhs, rhs, if_true1);

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* false1 = zero;

    if_false0 = graph()->NewNode(merge_op, if_true1, if_false1);
    false0 = graph()->NewNode(phi_op, true1, false1, if_false0);
  }

  Node* merge0 = graph()->NewNode(merge_op, if_true0, if_false0);
  return graph()->NewNode(phi_op, true0, false0, merge0);
}

Node* SimplifiedLowering::Int32Sign(Node* const node) {
  Node* const minus_one = jsgraph()->Int32Constant(-1);
  Node* const zero = jsgraph()->Int32Constant(0);
  Node* const one = jsgraph()->Int32Constant(1);

  Node* const input = node->InputAt(0);

  return graph()->NewNode(
      common()->Select(MachineRepresentation::kWord32),
      graph()->NewNode(machine()->Int32LessThan(), input, zero), minus_one,
      graph()->NewNode(
          common()->Select(MachineRepresentation::kWord32),
          graph()->NewNode(machine()->Int32LessThan(), zero, input), one,
          zero));
}

Node* SimplifiedLowering::Uint32Div(Node* const node) {
  Uint32BinopMatcher m(node);
  Node* const zero = jsgraph()->Uint32Constant(0);
  Node* const lhs = m.left().node();
  Node* const rhs = m.right().node();

  if (m.right().Is(0)) {
    return zero;
  } else if (machine()->Uint32DivIsSafe() || m.right().HasValue()) {
    return graph()->NewNode(machine()->Uint32Div(), lhs, rhs, graph()->start());
  }

  Node* check = graph()->NewNode(machine()->Word32Equal(), rhs, zero);
  Diamond d(graph(), common(), check, BranchHint::kFalse);
  Node* div = graph()->NewNode(machine()->Uint32Div(), lhs, rhs, d.if_false);
  return d.Phi(MachineRepresentation::kWord32, zero, div);
}

Node* SimplifiedLowering::Uint32Mod(Node* const node) {
  Uint32BinopMatcher m(node);
  Node* const minus_one = jsgraph()->Int32Constant(-1);
  Node* const zero = jsgraph()->Uint32Constant(0);
  Node* const lhs = m.left().node();
  Node* const rhs = m.right().node();

  if (m.right().Is(0)) {
    return zero;
  } else if (m.right().HasValue()) {
    return graph()->NewNode(machine()->Uint32Mod(), lhs, rhs, graph()->start());
  }

  // General case for unsigned integer modulus, with optimization for (unknown)
  // power of 2 right hand side.
  //
  //   if rhs == 0 then
  //     zero
  //   else
  //     msk = rhs - 1
  //     if rhs & msk != 0 then
  //       lhs % rhs
  //     else
  //       lhs & msk
  //
  // Note: We do not use the Diamond helper class here, because it really hurts
  // readability with nested diamonds.
  const Operator* const merge_op = common()->Merge(2);
  const Operator* const phi_op =
      common()->Phi(MachineRepresentation::kWord32, 2);

  Node* check0 = graph()->NewNode(machine()->Word32Equal(), rhs, zero);
  Node* branch0 = graph()->NewNode(common()->Branch(BranchHint::kFalse), check0,
                                   graph()->start());

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* true0 = zero;

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* false0;
  {
    Node* msk = graph()->NewNode(machine()->Int32Add(), rhs, minus_one);

    Node* check1 = graph()->NewNode(machine()->Word32And(), rhs, msk);
    Node* branch1 = graph()->NewNode(common()->Branch(), check1, if_false0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* true1 = graph()->NewNode(machine()->Uint32Mod(), lhs, rhs, if_true1);

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* false1 = graph()->NewNode(machine()->Word32And(), lhs, msk);

    if_false0 = graph()->NewNode(merge_op, if_true1, if_false1);
    false0 = graph()->NewNode(phi_op, true1, false1, if_false0);
  }

  Node* merge0 = graph()->NewNode(merge_op, if_true0, if_false0);
  return graph()->NewNode(phi_op, true0, false0, merge0);
}

void SimplifiedLowering::DoMax(Node* node, Operator const* op,
                               MachineRepresentation rep) {
  Node* const lhs = node->InputAt(0);
  Node* const rhs = node->InputAt(1);

  node->ReplaceInput(0, graph()->NewNode(op, lhs, rhs));
  DCHECK_EQ(rhs, node->InputAt(1));
  node->AppendInput(graph()->zone(), lhs);
  NodeProperties::ChangeOp(node, common()->Select(rep));
}

void SimplifiedLowering::DoMin(Node* node, Operator const* op,
                               MachineRepresentation rep) {
  Node* const lhs = node->InputAt(0);
  Node* const rhs = node->InputAt(1);

  node->InsertInput(graph()->zone(), 0, graph()->NewNode(op, lhs, rhs));
  DCHECK_EQ(lhs, node->InputAt(1));
  DCHECK_EQ(rhs, node->InputAt(2));
  NodeProperties::ChangeOp(node, common()->Select(rep));
}

void SimplifiedLowering::DoIntegral32ToBit(Node* node) {
  Node* const input = node->InputAt(0);
  Node* const zero = jsgraph()->Int32Constant(0);
  Operator const* const op = machine()->Word32Equal();

  node->ReplaceInput(0, graph()->NewNode(op, input, zero));
  node->AppendInput(graph()->zone(), zero);
  NodeProperties::ChangeOp(node, op);
}

void SimplifiedLowering::DoOrderedNumberToBit(Node* node) {
  Node* const input = node->InputAt(0);

  node->ReplaceInput(0, graph()->NewNode(machine()->Float64Equal(), input,
                                         jsgraph()->Float64Constant(0.0)));
  node->AppendInput(graph()->zone(), jsgraph()->Int32Constant(0));
  NodeProperties::ChangeOp(node, machine()->Word32Equal());
}

void SimplifiedLowering::DoNumberToBit(Node* node) {
  Node* const input = node->InputAt(0);

  node->ReplaceInput(0, jsgraph()->Float64Constant(0.0));
  node->AppendInput(graph()->zone(),
                    graph()->NewNode(machine()->Float64Abs(), input));
  NodeProperties::ChangeOp(node, machine()->Float64LessThan());
}

void SimplifiedLowering::DoIntegerToUint8Clamped(Node* node) {
  Node* const input = node->InputAt(0);
  Node* const min = jsgraph()->Float64Constant(0.0);
  Node* const max = jsgraph()->Float64Constant(255.0);

  node->ReplaceInput(
      0, graph()->NewNode(machine()->Float64LessThan(), min, input));
  node->AppendInput(
      graph()->zone(),
      graph()->NewNode(
          common()->Select(MachineRepresentation::kFloat64),
          graph()->NewNode(machine()->Float64LessThan(), input, max), input,
          max));
  node->AppendInput(graph()->zone(), min);
  NodeProperties::ChangeOp(node,
                           common()->Select(MachineRepresentation::kFloat64));
}

void SimplifiedLowering::DoNumberToUint8Clamped(Node* node) {
  Node* const input = node->InputAt(0);
  Node* const min = jsgraph()->Float64Constant(0.0);
  Node* const max = jsgraph()->Float64Constant(255.0);

  node->ReplaceInput(
      0, graph()->NewNode(
             common()->Select(MachineRepresentation::kFloat64),
             graph()->NewNode(machine()->Float64LessThan(), min, input),
             graph()->NewNode(
                 common()->Select(MachineRepresentation::kFloat64),
                 graph()->NewNode(machine()->Float64LessThan(), input, max),
                 input, max),
             min));
  NodeProperties::ChangeOp(node,
                           machine()->Float64RoundTiesEven().placeholder());
}

void SimplifiedLowering::DoSigned32ToUint8Clamped(Node* node) {
  Node* const input = node->InputAt(0);
  Node* const min = jsgraph()->Int32Constant(0);
  Node* const max = jsgraph()->Int32Constant(255);

  node->ReplaceInput(
      0, graph()->NewNode(machine()->Int32LessThanOrEqual(), input, max));
  node->AppendInput(
      graph()->zone(),
      graph()->NewNode(common()->Select(MachineRepresentation::kWord32),
                       graph()->NewNode(machine()->Int32LessThan(), input, min),
                       min, input));
  node->AppendInput(graph()->zone(), max);
  NodeProperties::ChangeOp(node,
                           common()->Select(MachineRepresentation::kWord32));
}

void SimplifiedLowering::DoUnsigned32ToUint8Clamped(Node* node) {
  Node* const input = node->InputAt(0);
  Node* const max = jsgraph()->Uint32Constant(255u);

  node->ReplaceInput(
      0, graph()->NewNode(machine()->Uint32LessThanOrEqual(), input, max));
  node->AppendInput(graph()->zone(), input);
  node->AppendInput(graph()->zone(), max);
  NodeProperties::ChangeOp(node,
                           common()->Select(MachineRepresentation::kWord32));
}

Node* SimplifiedLowering::ToNumberCode() {
  if (!to_number_code_.is_set()) {
    Callable callable = Builtins::CallableFor(isolate(), Builtins::kToNumber);
    to_number_code_.set(jsgraph()->HeapConstant(callable.code()));
  }
  return to_number_code_.get();
}

Node* SimplifiedLowering::ToNumberConvertBigIntCode() {
  if (!to_number_convert_big_int_code_.is_set()) {
    Callable callable =
        Builtins::CallableFor(isolate(), Builtins::kToNumberConvertBigInt);
    to_number_convert_big_int_code_.set(
        jsgraph()->HeapConstant(callable.code()));
  }
  return to_number_convert_big_int_code_.get();
}

Node* SimplifiedLowering::ToNumericCode() {
  if (!to_numeric_code_.is_set()) {
    Callable callable = Builtins::CallableFor(isolate(), Builtins::kToNumeric);
    to_numeric_code_.set(jsgraph()->HeapConstant(callable.code()));
  }
  return to_numeric_code_.get();
}

Operator const* SimplifiedLowering::ToNumberOperator() {
  if (!to_number_operator_.is_set()) {
    Callable callable = Builtins::CallableFor(isolate(), Builtins::kToNumber);
    CallDescriptor::Flags flags = CallDescriptor::kNeedsFrameState;
    auto call_descriptor = Linkage::GetStubCallDescriptor(
        graph()->zone(), callable.descriptor(),
        callable.descriptor().GetStackParameterCount(), flags,
        Operator::kNoProperties);
    to_number_operator_.set(common()->Call(call_descriptor));
  }
  return to_number_operator_.get();
}

Operator const* SimplifiedLowering::ToNumberConvertBigIntOperator() {
  if (!to_number_convert_big_int_operator_.is_set()) {
    Callable callable =
        Builtins::CallableFor(isolate(), Builtins::kToNumberConvertBigInt);
    CallDescriptor::Flags flags = CallDescriptor::kNeedsFrameState;
    auto call_descriptor = Linkage::GetStubCallDescriptor(
        graph()->zone(), callable.descriptor(),
        callable.descriptor().GetStackParameterCount(), flags,
        Operator::kNoProperties);
    to_number_convert_big_int_operator_.set(common()->Call(call_descriptor));
  }
  return to_number_convert_big_int_operator_.get();
}

Operator const* SimplifiedLowering::ToNumericOperator() {
  if (!to_numeric_operator_.is_set()) {
    Callable callable = Builtins::CallableFor(isolate(), Builtins::kToNumeric);
    CallDescriptor::Flags flags = CallDescriptor::kNeedsFrameState;
    auto call_descriptor = Linkage::GetStubCallDescriptor(
        graph()->zone(), callable.descriptor(),
        callable.descriptor().GetStackParameterCount(), flags,
        Operator::kNoProperties);
    to_numeric_operator_.set(common()->Call(call_descriptor));
  }
  return to_numeric_operator_.get();
}

#undef TRACE

}  // namespace compiler
}  // namespace internal
}  // namespace v8
