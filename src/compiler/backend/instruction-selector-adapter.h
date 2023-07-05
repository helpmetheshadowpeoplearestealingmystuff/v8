// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_BACKEND_INSTRUCTION_SELECTOR_ADAPTER_H_
#define V8_COMPILER_BACKEND_INSTRUCTION_SELECTOR_ADAPTER_H_

#include "src/codegen/machine-type.h"
#include "src/compiler/backend/instruction.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/opcodes.h"
#include "src/compiler/operator.h"
#include "src/compiler/schedule.h"
#include "src/compiler/turboshaft/graph.h"
#include "src/compiler/turboshaft/operation-matching.h"
#include "src/compiler/turboshaft/operations.h"

// TODO(nicohartmann@):
// During the transition period to a generic instruction selector, some
// instantiations with TurboshaftAdapter will still call functions with
// Node* arguments. Use `DECLARE_UNREACHABLE_TURBOSHAFT_FALLBACK` to define
// a temporary fallback for these functions such that compilation is possible
// while transitioning the instruction selector incrementally. Once all uses
// of Node*, BasicBlock*, ... have been replaced, remove those fallbacks.
#define DECLARE_UNREACHABLE_TURBOSHAFT_FALLBACK(ret, name)             \
  template <typename... Args>                                          \
  std::enable_if_t<Adapter::IsTurboshaft &&                            \
                       detail::AnyTurbofanNodeOrBlock<Args...>::value, \
                   ret>                                                \
  name(Args...) {                                                      \
    UNREACHABLE();                                                     \
  }

namespace v8::internal::compiler {
namespace detail {
template <typename...>
struct AnyTurbofanNodeOrBlock;
template <typename Head, typename... Tail>
struct AnyTurbofanNodeOrBlock<Head, Tail...> {
  static constexpr bool value = std::is_same_v<Head, Node*> ||
                                std::is_same_v<Head, BasicBlock*> ||
                                AnyTurbofanNodeOrBlock<Tail...>::value;
};
template <>
struct AnyTurbofanNodeOrBlock<> {
  static constexpr bool value = false;
};
}  // namespace detail

struct TurbofanAdapter {
  static constexpr bool IsTurbofan = true;
  static constexpr bool IsTurboshaft = false;
  static constexpr bool AllowsImplicitWord64ToWord32Truncation = false;
  using schedule_t = Schedule*;
  using block_t = BasicBlock*;
  using block_range_t = ZoneVector<block_t>;
  using node_t = Node*;
  using inputs_t = Node::Inputs;
  using opcode_t = IrOpcode::Value;
  using id_t = uint32_t;

  class ConstantView {
   public:
    explicit ConstantView(node_t node) : node_(node) {
      DCHECK(node_->opcode() == IrOpcode::kInt32Constant ||
             node_->opcode() == IrOpcode::kInt64Constant ||
             node_->opcode() == IrOpcode::kRelocatableInt32Constant ||
             node_->opcode() == IrOpcode::kRelocatableInt64Constant ||
             node_->opcode() == IrOpcode::kHeapConstant ||
             node_->opcode() == IrOpcode::kCompressedHeapConstant ||
             node_->opcode() == IrOpcode::kNumberConstant);
    }

    bool is_int32() const {
      return node_->opcode() == IrOpcode::kInt32Constant;
    }
    bool is_relocatable_int32() const {
      return node_->opcode() == IrOpcode::kRelocatableInt32Constant;
    }
    int32_t int32_value() const {
      DCHECK(is_int32() || is_relocatable_int32());
      return OpParameter<int32_t>(node_->op());
    }
    bool is_int64() const {
      return node_->opcode() == IrOpcode::kInt64Constant;
    }
    bool is_relocatable_int64() const {
      return node_->opcode() == IrOpcode::kRelocatableInt64Constant;
    }
    int64_t int64_value() const {
      DCHECK(is_int64() || is_relocatable_int64());
      return OpParameter<int64_t>(node_->op());
    }
    bool is_heap_object() const {
      return node_->opcode() == IrOpcode::kHeapConstant;
    }
    bool is_compressed_heap_object() const {
      return node_->opcode() == IrOpcode::kCompressedHeapConstant;
    }
    Handle<HeapObject> heap_object_value() const {
      DCHECK(is_heap_object() || is_compressed_heap_object());
      return OpParameter<Handle<HeapObject>>(node_->op());
    }
    bool is_number() const {
      return node_->opcode() == IrOpcode::kNumberConstant;
    }
    double number_value() const {
      DCHECK(is_number());
      return OpParameter<double>(node_->op());
    }

    operator node_t() const { return node_; }

   private:
    node_t node_;
  };

  class CallView {
   public:
    explicit CallView(node_t node) : node_(node) {
      DCHECK(node_->opcode() == IrOpcode::kCall ||
             node_->opcode() == IrOpcode::kTailCall);
    }

    int return_count() const { return node_->op()->ValueOutputCount(); }
    node_t callee() const { return node_->InputAt(0); }
    node_t frame_state() const {
      return node_->InputAt(static_cast<int>(call_descriptor()->InputCount()));
    }
    base::Vector<node_t> arguments() const {
      base::Vector<node_t> inputs = node_->inputs_vector();
      return inputs.SubVector(1, inputs.size());
    }
    const CallDescriptor* call_descriptor() const {
      return CallDescriptorOf(node_->op());
    }

    operator node_t() const { return node_; }

   private:
    node_t node_;
  };

  class BranchView {
   public:
    explicit BranchView(node_t node) : node_(node) {
      DCHECK_EQ(node_->opcode(), IrOpcode::kBranch);
    }

    node_t condition() const { return node_->InputAt(0); }

    operator node_t() const { return node_; }

   private:
    node_t node_;
  };

  class WordBinopView {
   public:
    explicit WordBinopView(node_t node) : node_(node), m_(node) {}

    void EnsureConstantIsRightIfCommutative() {
      // Nothing to do. Matcher already ensures that.
    }

    node_t left() const { return m_.left().node(); }
    node_t right() const { return m_.right().node(); }

    operator node_t() const { return node_; }

   private:
    node_t node_;
    Int32BinopMatcher m_;
  };

  class LoadView {
   public:
    explicit LoadView(node_t node) : node_(node) {
      DCHECK(node_->opcode() == IrOpcode::kLoad ||
             node_->opcode() == IrOpcode::kLoadImmutable ||
             node_->opcode() == IrOpcode::kProtectedLoad ||
             node_->opcode() == IrOpcode::kLoadTrapOnNull);
    }

    LoadRepresentation loaded_rep() const {
      return LoadRepresentationOf(node_->op());
    }

    node_t base() const { return node_->InputAt(0); }
    node_t index() const { return node_->InputAt(1); }

    operator node_t() const { return node_; }

   private:
    node_t node_;
  };

  class StoreView {
   public:
    explicit StoreView(node_t node) : node_(node) {
      DCHECK(node->opcode() == IrOpcode::kStore ||
             node->opcode() == IrOpcode::kProtectedStore ||
             node->opcode() == IrOpcode::kStoreTrapOnNull ||
             node->opcode() == IrOpcode::kWord32AtomicStore ||
             node->opcode() == IrOpcode::kWord64AtomicStore);
    }

    StoreRepresentation stored_rep() const {
      switch (node_->opcode()) {
        case IrOpcode::kStore:
        case IrOpcode::kProtectedStore:
        case IrOpcode::kStoreTrapOnNull:
          return StoreRepresentationOf(node_->op());
        case IrOpcode::kWord32AtomicStore:
        case IrOpcode::kWord64AtomicStore:
          return AtomicStoreParametersOf(node_->op()).store_representation();
        default:
          UNREACHABLE();
      }
    }
    base::Optional<AtomicMemoryOrder> memory_order() const {
      switch (node_->opcode()) {
        case IrOpcode::kStore:
        case IrOpcode::kProtectedStore:
        case IrOpcode::kStoreTrapOnNull:
          return base::nullopt;
        case IrOpcode::kWord32AtomicStore:
        case IrOpcode::kWord64AtomicStore:
          return AtomicStoreParametersOf(node_->op()).order();
        default:
          UNREACHABLE();
      }
    }
    MemoryAccessKind access_kind() const {
      switch (node_->opcode()) {
        case IrOpcode::kStore:
          return MemoryAccessKind::kNormal;
        case IrOpcode::kProtectedStore:
        case IrOpcode::kStoreTrapOnNull:
          return MemoryAccessKind::kProtected;
        case IrOpcode::kWord32AtomicStore:
        case IrOpcode::kWord64AtomicStore:
          return AtomicStoreParametersOf(node_->op()).kind();
        default:
          UNREACHABLE();
      }
    }

    node_t base() const { return node_->InputAt(0); }
    node_t index() const { return node_->InputAt(1); }
    node_t value() const { return node_->InputAt(2); }
    int32_t displacement() const { return 0; }
    uint8_t element_size_log2() const { return 0; }

    bool is_store_trap_on_null() const {
      return node_->opcode() == IrOpcode::kStoreTrapOnNull;
    }

    operator node_t() const { return node_; }

   private:
    node_t node_;
  };

  class DeoptimizeView {
   public:
    explicit DeoptimizeView(node_t node) : node_(node) {
      DCHECK(node_->opcode() == IrOpcode::kDeoptimize ||
             node_->opcode() == IrOpcode::kDeoptimizeIf ||
             node_->opcode() == IrOpcode::kDeoptimizeUnless);
    }

    DeoptimizeReason reason() const {
      return DeoptimizeParametersOf(node_->op()).reason();
    }
    FeedbackSource feedback() const {
      return DeoptimizeParametersOf(node_->op()).feedback();
    }
    node_t frame_state() const {
      if (is_deoptimize_if()) return node_->InputAt(1);
      // TODO(nicohartmann@): Get right input for Deoptimize.
      UNIMPLEMENTED();
    }

    bool is_deoptimize() const {
      return node_->opcode() == IrOpcode::kDeoptimize;
    }
    bool is_deoptimize_if() const {
      return node_->opcode() == IrOpcode::kDeoptimizeIf;
    }
    bool is_deoptimize_unless() const {
      return node_->opcode() == IrOpcode::kDeoptimizeUnless;
    }

    node_t condition() const {
      DCHECK(is_deoptimize_if() || is_deoptimize_unless());
      return node_->InputAt(0);
    }

    operator node_t() const { return node_; }

   private:
    node_t node_;
  };

  bool is_constant(node_t node) const {
    switch (node->opcode()) {
      case IrOpcode::kInt32Constant:
      case IrOpcode::kInt64Constant:
      case IrOpcode::kRelocatableInt32Constant:
      case IrOpcode::kRelocatableInt64Constant:
      case IrOpcode::kHeapConstant:
      case IrOpcode::kCompressedHeapConstant:
      case IrOpcode::kNumberConstant:
        // For those, a view must be constructible.
        DCHECK_EQ(constant_view(node), node);
        return true;
      default:
        return false;
    }
  }
  bool is_load(node_t node) const {
    return node->opcode() == IrOpcode::kLoad ||
           node->opcode() == IrOpcode::kLoadImmutable ||
           node->opcode() == IrOpcode::kProtectedLoad ||
           node->opcode() == IrOpcode::kLoadTrapOnNull;
  }
  ConstantView constant_view(node_t node) const { return ConstantView{node}; }
  CallView call_view(node_t node) { return CallView{node}; }
  BranchView branch_view(node_t node) { return BranchView(node); }
  WordBinopView word_binop_view(node_t node) { return WordBinopView(node); }
  LoadView load_view(node_t node) {
    DCHECK(is_load(node));
    return LoadView(node);
  }
  StoreView store_view(node_t node) { return StoreView(node); }
  DeoptimizeView deoptimize_view(node_t node) { return DeoptimizeView(node); }

  void InitializeAdapter(schedule_t) {}

  block_t block(schedule_t schedule, node_t node) const {
    return schedule->block(node);
  }

  RpoNumber rpo_number(block_t block) const {
    return RpoNumber::FromInt(block->rpo_number());
  }

  const block_range_t& rpo_order(schedule_t schedule) const {
    return *schedule->rpo_order();
  }

  bool IsLoopHeader(block_t block) const { return block->IsLoopHeader(); }

  size_t PredecessorCount(block_t block) const {
    return block->PredecessorCount();
  }
  block_t PredecessorAt(block_t block, size_t index) const {
    return block->PredecessorAt(index);
  }

  base::iterator_range<NodeVector::iterator> nodes(block_t block) {
    return {block->begin(), block->end()};
  }

  bool IsPhi(node_t node) const { return node->opcode() == IrOpcode::kPhi; }
  MachineRepresentation phi_representation_of(node_t node) const {
    DCHECK(IsPhi(node));
    return PhiRepresentationOf(node->op());
  }
  bool IsRetain(node_t node) const {
    return node->opcode() == IrOpcode::kRetain;
  }
  bool IsHeapConstant(node_t node) const {
    return node->opcode() == IrOpcode::kHeapConstant;
  }
  bool IsExternalConstant(node_t node) const {
    return node->opcode() == IrOpcode::kExternalConstant;
  }
  bool IsRelocatableWasmConstant(node_t node) const {
    return node->opcode() == IrOpcode::kRelocatableInt32Constant ||
           node->opcode() == IrOpcode::kRelocatableInt64Constant;
  }
  bool IsLoadOrLoadImmutable(node_t node) const {
    return node->opcode() == IrOpcode::kLoad ||
           node->opcode() == IrOpcode::kLoadImmutable;
  }

  int value_input_count(node_t node) const {
    return node->op()->ValueInputCount();
  }
  node_t input_at(node_t node, size_t index) const {
    return node->InputAt(static_cast<int>(index));
  }
  inputs_t inputs(node_t node) const { return node->inputs(); }
  opcode_t opcode(node_t node) const { return node->opcode(); }
  bool is_exclusive_user_of(node_t user, node_t value) const {
    for (Edge const edge : value->use_edges()) {
      if (edge.from() != user && NodeProperties::IsValueEdge(edge)) {
        return false;
      }
    }
    return true;
  }

  id_t id(node_t node) const { return node->id(); }
  static bool valid(node_t node) { return node != nullptr; }

  node_t block_terminator(block_t block) const {
    return block->control_input();
  }
  node_t parent_frame_state(node_t node) const {
    DCHECK_EQ(node->opcode(), IrOpcode::kFrameState);
    DCHECK_EQ(FrameState{node}.outer_frame_state(),
              NodeProperties::GetFrameStateInput(node));
    return NodeProperties::GetFrameStateInput(node);
  }
  int parameter_index_of(node_t node) const {
    DCHECK_EQ(node->opcode(), IrOpcode::kParameter);
    return ParameterIndexOf(node->op());
  }
  size_t projection_index_of(node_t node) const {
    DCHECK_EQ(node->opcode(), IrOpcode::kProjection);
    return ProjectionIndexOf(node->op());
  }
  bool is_integer_constant(node_t node) const {
    return node->opcode() == IrOpcode::kInt32Constant ||
           node->opcode() == IrOpcode::kInt64Constant;
  }
  int64_t integer_constant(node_t node) const {
    if (node->opcode() == IrOpcode::kInt32Constant) {
      return OpParameter<int32_t>(node->op());
    }
    DCHECK_EQ(node->opcode(), IrOpcode::kInt64Constant);
    return OpParameter<int64_t>(node->op());
  }

  bool IsRequiredWhenUnused(node_t node) const {
    return !node->op()->HasProperty(Operator::kEliminatable);
  }
  bool IsCommutative(node_t node) const {
    return node->op()->HasProperty(Operator::kCommutative);
  }
};

struct TurboshaftAdapter
    : public turboshaft::OperationMatching<TurboshaftAdapter> {
  static constexpr bool IsTurbofan = false;
  static constexpr bool IsTurboshaft = true;
  static constexpr bool AllowsImplicitWord64ToWord32Truncation = true;
  // TODO(nicohartmann@): Rename schedule_t once Turbofan is gone.
  using schedule_t = turboshaft::Graph*;
  using block_t = turboshaft::Block*;
  using block_range_t = ZoneVector<block_t>;
  using node_t = turboshaft::OpIndex;
  using inputs_t = base::Vector<const node_t>;
  using opcode_t = turboshaft::Opcode;
  using id_t = uint32_t;

  class ConstantView {
    using Kind = turboshaft::ConstantOp::Kind;

   public:
    ConstantView(turboshaft::Graph* graph, node_t node) : node_(node) {
      op_ = &graph->Get(node_).Cast<turboshaft::ConstantOp>();
    }

    bool is_int32() const { return op_->kind == Kind::kWord32; }
    bool is_relocatable_int32() const {
      // We don't have this in turboshaft currently.
      return false;
    }
    int32_t int32_value() const {
      DCHECK(is_int32() || is_relocatable_int32());
      return op_->word32();
    }
    bool is_int64() const { return op_->kind == Kind::kWord64; }
    bool is_relocatable_int64() const {
      return op_->kind == Kind::kRelocatableWasmCall ||
             op_->kind == Kind::kRelocatableWasmStubCall;
    }
    int64_t int64_value() const {
      if (is_int64()) return op_->word64();
      DCHECK(is_relocatable_int64());
      return static_cast<int64_t>(op_->integral());
    }
    bool is_heap_object() const { return op_->kind == Kind::kHeapObject; }
    bool is_compressed_heap_object() const {
      return op_->kind == Kind::kCompressedHeapObject;
    }
    Handle<HeapObject> heap_object_value() const {
      DCHECK(is_heap_object() || is_compressed_heap_object());
      return op_->handle();
    }
    bool is_number() const { return op_->kind == Kind::kNumber; }
    double number_value() const {
      DCHECK(is_number());
      return op_->number();
    }

    operator node_t() const { return node_; }

   private:
    node_t node_;
    const turboshaft::ConstantOp* op_;
  };

  class CallView {
   public:
    explicit CallView(turboshaft::Graph* graph, node_t node) : node_(node) {
      if (graph->Get(node_).Is<turboshaft::TailCallOp>()) {
        UNIMPLEMENTED();
      }
      op_ = &graph->Get(node_).Cast<turboshaft::CallOp>();
    }

    int return_count() const {
      return static_cast<int>(op_->outputs_rep().size());
    }
    node_t callee() const { return op_->callee(); }
    node_t frame_state() const { return op_->frame_state(); }
    base::Vector<const node_t> arguments() const { return op_->arguments(); }
    const CallDescriptor* call_descriptor() const {
      return op_->descriptor->descriptor;
    }

    operator node_t() const { return node_; }

   private:
    node_t node_;
    const turboshaft::CallOp* op_;
  };

  class BranchView {
   public:
    explicit BranchView(turboshaft::Graph* graph, node_t node) : node_(node) {
      op_ = &graph->Get(node_).Cast<turboshaft::BranchOp>();
    }

    node_t condition() const { return op_->condition(); }

    operator node_t() const { return node_; }

   private:
    node_t node_;
    const turboshaft::BranchOp* op_;
  };

  class WordBinopView {
   public:
    explicit WordBinopView(turboshaft::Graph* graph, node_t node)
        : node_(node) {
      op_ = &graph->Get(node_).Cast<turboshaft::WordBinopOp>();
      left_ = op_->left();
      right_ = op_->right();
      can_put_constant_right_ =
          op_->IsCommutative(op_->kind) &&
          graph->Get(left_).Is<turboshaft::ConstantOp>() &&
          !graph->Get(right_).Is<turboshaft::ConstantOp>();
    }

    void EnsureConstantIsRightIfCommutative() {
      if (can_put_constant_right_) {
        std::swap(left_, right_);
        can_put_constant_right_ = false;
      }
    }

    node_t left() const { return left_; }
    node_t right() const { return right_; }

    operator node_t() const { return node_; }

   private:
    node_t node_;
    const turboshaft::WordBinopOp* op_;
    node_t left_;
    node_t right_;
    bool can_put_constant_right_;
  };

  class LoadView {
   public:
    LoadView(turboshaft::Graph* graph, node_t node) : node_(node) {
      op_ = &graph->Get(node_).Cast<turboshaft::LoadOp>();
    }

    LoadRepresentation loaded_rep() const {
      return op_->loaded_rep.ToMachineType();
    }

    node_t base() const { return op_->base(); }
    node_t index() const { return op_->index(); }

    operator node_t() const { return node_; }

   private:
    node_t node_;
    const turboshaft::LoadOp* op_;
  };

  class StoreView {
   public:
    StoreView(turboshaft::Graph* graph, node_t node) : node_(node) {
      op_ = &graph->Get(node_).Cast<turboshaft::StoreOp>();
    }

    StoreRepresentation stored_rep() const {
      return {op_->stored_rep.ToMachineType().representation(),
              op_->write_barrier};
    }
    base::Optional<AtomicMemoryOrder> memory_order() const {
      // TODO(nicohartmann@): Currently we only have non-atomic stores.
      return base::nullopt;
    }
    MemoryAccessKind access_kind() const {
      // TODO(nicohartmann@): Currently we only have non-atomic stores.
      return MemoryAccessKind::kNormal;
    }

    node_t base() const { return op_->base(); }
    node_t index() const { return op_->index(); }
    node_t value() const { return op_->value(); }
    int32_t displacement() const {
      static_assert(
          std::is_same_v<decltype(turboshaft::StoreOp::offset), int32_t>);
      return op_->offset;
    }
    uint8_t element_size_log2() const {
      static_assert(
          std::is_same_v<decltype(turboshaft::StoreOp::element_size_log2),
                         uint8_t>);
      return op_->element_size_log2;
    }

    bool is_store_trap_on_null() const { return false; }

    operator node_t() const { return node_; }

   private:
    node_t node_;
    const turboshaft::StoreOp* op_;
  };

  class DeoptimizeView {
   public:
    explicit DeoptimizeView(const turboshaft::Graph* graph, node_t node)
        : node_(node) {
      const auto& op = graph->Get(node);
      if (op.Is<turboshaft::DeoptimizeOp>()) {
        deoptimize_op_ = &op.Cast<turboshaft::DeoptimizeOp>();
        parameters_ = deoptimize_op_->parameters;
      } else {
        DCHECK(op.Is<turboshaft::DeoptimizeIfOp>());
        deoptimize_if_op_ = &op.Cast<turboshaft::DeoptimizeIfOp>();
        parameters_ = deoptimize_if_op_->parameters;
      }
    }

    DeoptimizeReason reason() const { return parameters_->reason(); }
    FeedbackSource feedback() const { return parameters_->feedback(); }
    node_t frame_state() const {
      return deoptimize_op_ ? deoptimize_op_->frame_state()
                            : deoptimize_if_op_->frame_state();
    }

    bool is_deoptimize() const { return deoptimize_op_ != nullptr; }
    bool is_deoptimize_if() const {
      return deoptimize_if_op_ != nullptr && !deoptimize_if_op_->negated;
    }
    bool is_deoptimize_unless() const {
      return deoptimize_if_op_ != nullptr && deoptimize_if_op_->negated;
    }

    node_t condition() const {
      DCHECK(is_deoptimize_if());
      return deoptimize_if_op_->condition();
    }

    operator node_t() const { return node_; }

   private:
    node_t node_;
    const turboshaft::DeoptimizeOp* deoptimize_op_ = nullptr;
    const turboshaft::DeoptimizeIfOp* deoptimize_if_op_ = nullptr;
    const DeoptimizeParameters* parameters_;
  };

  bool is_constant(node_t node) const {
    return graph_->Get(node).Is<turboshaft::ConstantOp>();
  }
  bool is_load(node_t node) const {
    return graph_->Get(node).Is<turboshaft::LoadOp>();
  }
  ConstantView constant_view(node_t node) { return ConstantView{graph_, node}; }
  CallView call_view(node_t node) { return CallView{graph_, node}; }
  BranchView branch_view(node_t node) { return BranchView(graph_, node); }
  WordBinopView word_binop_view(node_t node) {
    return WordBinopView(graph_, node);
  }
  LoadView load_view(node_t node) {
    DCHECK(is_load(node));
    return LoadView(graph_, node);
  }
  StoreView store_view(node_t node) { return StoreView(graph_, node); }
  DeoptimizeView deoptimize_view(node_t node) {
    return DeoptimizeView(graph_, node);
  }

  void InitializeAdapter(schedule_t schedule) { graph_ = schedule; }
  turboshaft::Graph* turboshaft_graph() const { return graph_; }

  block_t block(schedule_t schedule, node_t node) const {
    // TODO(nicohartmann@): This might be too slow and we should consider
    // precomputing.
    return &schedule->Get(schedule->BlockOf(node));
  }

  RpoNumber rpo_number(block_t block) const {
    return RpoNumber::FromInt(block->index().id());
  }

  const block_range_t& rpo_order(schedule_t schedule) {
    return schedule->blocks_vector();
  }

  bool IsLoopHeader(block_t block) const { return block->IsLoop(); }

  size_t PredecessorCount(block_t block) const {
    return block->PredecessorCount();
  }
  block_t PredecessorAt(block_t block, size_t index) const {
    return block->Predecessors()[index];
  }

  base::iterator_range<turboshaft::Graph::OpIndexIterator> nodes(
      block_t block) {
    return graph_->OperationIndices(*block);
  }

  bool IsPhi(node_t node) const {
    return graph_->Get(node).Is<turboshaft::PhiOp>();
  }
  MachineRepresentation phi_representation_of(node_t node) const {
    DCHECK(IsPhi(node));
    const turboshaft::PhiOp& phi = graph_->Get(node).Cast<turboshaft::PhiOp>();
    return phi.rep.machine_representation();
  }
  bool IsRetain(node_t node) const {
    return graph_->Get(node).Is<turboshaft::RetainOp>();
  }
  bool IsHeapConstant(node_t node) const {
    turboshaft::ConstantOp* constant =
        graph_->Get(node).TryCast<turboshaft::ConstantOp>();
    if (constant == nullptr) return false;
    return constant->kind == turboshaft::ConstantOp::Kind::kHeapObject;
  }
  bool IsExternalConstant(node_t node) const {
    turboshaft::ConstantOp* constant =
        graph_->Get(node).TryCast<turboshaft::ConstantOp>();
    if (constant == nullptr) return false;
    return constant->kind == turboshaft::ConstantOp::Kind::kExternal;
  }
  bool IsRelocatableWasmConstant(node_t node) const {
    turboshaft::ConstantOp* constant =
        graph_->Get(node).TryCast<turboshaft::ConstantOp>();
    if (constant == nullptr) return false;
    return constant->kind ==
           turboshaft::any_of(
               turboshaft::ConstantOp::Kind::kRelocatableWasmCall,
               turboshaft::ConstantOp::Kind::kRelocatableWasmStubCall);
  }
  bool IsLoadOrLoadImmutable(node_t node) const {
    return graph_->Get(node).opcode == turboshaft::Opcode::kLoad;
  }

  int value_input_count(node_t node) const {
    return graph_->Get(node).input_count;
  }
  node_t input_at(node_t node, size_t index) const {
    return graph_->Get(node).input(index);
  }
  inputs_t inputs(node_t node) const { return graph_->Get(node).inputs(); }
  opcode_t opcode(node_t node) const { return graph_->Get(node).opcode; }
  bool is_exclusive_user_of(node_t user, node_t value) const {
    DCHECK(valid(user));
    DCHECK(valid(value));
    const size_t use_count = base::count_if(
        graph_->Get(user).inputs(),
        [value](turboshaft::OpIndex input) { return input == value; });
    DCHECK_LT(0, use_count);
    DCHECK_LE(use_count, graph_->Get(value).saturated_use_count.Get());
    const turboshaft::Operation& value_op = graph_->Get(value);
    return (value_op.saturated_use_count.Get() == use_count) &&
           !value_op.saturated_use_count.IsSaturated();
  }

  id_t id(node_t node) const { return node.id(); }
  static bool valid(node_t node) { return node.valid(); }

  node_t block_terminator(block_t block) const {
    return graph_->PreviousIndex(block->end());
  }
  node_t parent_frame_state(node_t node) const {
    const turboshaft::FrameStateOp& frame_state =
        graph_->Get(node).Cast<turboshaft::FrameStateOp>();
    return frame_state.parent_frame_state();
  }
  int parameter_index_of(node_t node) const {
    const turboshaft::ParameterOp& parameter =
        graph_->Get(node).Cast<turboshaft::ParameterOp>();
    return parameter.parameter_index;
  }
  size_t projection_index_of(node_t node) const {
    const turboshaft::ProjectionOp& projection =
        graph_->Get(node).Cast<turboshaft::ProjectionOp>();
    return projection.index;
  }
  bool is_integer_constant(node_t node) const {
    if (auto constant = graph_->Get(node).TryCast<turboshaft::ConstantOp>()) {
      return constant->kind == turboshaft::ConstantOp::Kind::kWord32 ||
             constant->kind == turboshaft::ConstantOp::Kind::kWord64;
    }
    return false;
  }
  int64_t integer_constant(node_t node) const {
    const turboshaft::ConstantOp* constant =
        graph_->Get(node).TryCast<turboshaft::ConstantOp>();
    DCHECK_NOT_NULL(constant);
    return constant->signed_integral();
  }

  bool IsRequiredWhenUnused(node_t node) const {
    return graph_->Get(node).IsRequiredWhenUnused();
  }
  bool IsCommutative(node_t node) const {
    const turboshaft::Operation& op = graph_->Get(node);
    if (const auto binop = op.TryCast<turboshaft::WordBinopOp>()) {
      return turboshaft::WordBinopOp::IsCommutative(binop->kind);
    } else if (const auto binop =
                   op.TryCast<turboshaft::OverflowCheckedBinopOp>()) {
      return turboshaft::OverflowCheckedBinopOp::IsCommutative(binop->kind);
    } else if (const auto binop = op.TryCast<turboshaft::FloatBinopOp>()) {
      return turboshaft::FloatBinopOp::IsCommutative(binop->kind);
    } else if (op.Is<turboshaft::EqualOp>()) {
      return turboshaft::EqualOp::IsCommutative();
    }
    return false;
  }

 private:
  friend class turboshaft::OperationMatching<TurboshaftAdapter>;
  // Provide access to the graph for the OpMatcher.
  const turboshaft::Graph& output_graph() const { return *graph_; }

  turboshaft::Graph* graph_;
};

}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_BACKEND_INSTRUCTION_SELECTOR_ADAPTER_H_
