// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-code-generator.h"

#include <algorithm>

#include "src/base/hashmap.h"
#include "src/codegen/code-desc.h"
#include "src/codegen/register.h"
#include "src/codegen/reglist.h"
#include "src/codegen/safepoint-table.h"
#include "src/codegen/source-position.h"
#include "src/common/globals.h"
#include "src/compiler/backend/instruction.h"
#include "src/deoptimizer/translation-array.h"
#include "src/execution/frame-constants.h"
#include "src/interpreter/bytecode-register.h"
#include "src/maglev/maglev-code-gen-state.h"
#include "src/maglev/maglev-compilation-unit.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-graph-printer.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-regalloc-data.h"
#include "src/objects/code-inl.h"
#include "src/utils/identity-map.h"

namespace v8 {
namespace internal {
namespace maglev {

#define __ masm()->

namespace {

template <typename RegisterT>
struct RegisterTHelper;
template <>
struct RegisterTHelper<Register> {
  static constexpr Register kScratch = kScratchRegister;
  static constexpr RegList kAllocatableRegisters = kAllocatableGeneralRegisters;
};
template <>
struct RegisterTHelper<DoubleRegister> {
  static constexpr DoubleRegister kScratch = kScratchDoubleReg;
  static constexpr DoubleRegList kAllocatableRegisters =
      kAllocatableDoubleRegisters;
};

// The ParallelMoveResolver is used to resolve multiple moves between registers
// and stack slots that are intended to happen, semantically, in parallel. It
// finds chains of moves that would clobber each other, and emits them in a non
// clobbering order; it also detects cycles of moves and breaks them by moving
// to a temporary.
//
// For example, given the moves:
//
//     r1 -> r2
//     r2 -> r3
//     r3 -> r4
//     r4 -> r1
//     r4 -> r5
//
// These can be represented as a move graph
//
//     r2 → r3
//     ↑     ↓
//     r1 ← r4 → r5
//
// and safely emitted (breaking the cycle with a temporary) as
//
//     r1 -> tmp
//     r4 -> r1
//     r4 -> r5
//     r3 -> r4
//     r2 -> r3
//    tmp -> r2
//
// It additionally keeps track of materialising moves, which don't have a stack
// slot but rather materialise a value from, e.g., a constant. These can safely
// be emitted at the end, once all the parallel moves are done.
template <typename RegisterT>
class ParallelMoveResolver {
  static constexpr RegisterT kScratchRegT =
      RegisterTHelper<RegisterT>::kScratch;

  static constexpr auto kAllocatableRegistersT =
      RegisterTHelper<RegisterT>::kAllocatableRegisters;

 public:
  explicit ParallelMoveResolver(MaglevCodeGenState* code_gen_state)
      : code_gen_state_(code_gen_state) {}

  void RecordMove(ValueNode* source_node, compiler::InstructionOperand source,
                  compiler::AllocatedOperand target) {
    if (target.IsRegister()) {
      RecordMoveToRegister(source_node, source, ToRegisterT<RegisterT>(target));
    } else {
      RecordMoveToStackSlot(
          source_node, source,
          code_gen_state_->GetFramePointerOffsetForStackSlot(target));
    }
  }

  void RecordMove(ValueNode* source_node, compiler::InstructionOperand source,
                  RegisterT target_reg) {
    RecordMoveToRegister(source_node, source, target_reg);
  }

  void EmitMoves() {
    for (RegisterT reg : kAllocatableRegistersT) {
      StartEmitMoveChain(reg);
      ValueNode* materializing_register_move =
          materializing_register_moves_[reg.code()];
      if (materializing_register_move) {
        materializing_register_move->LoadToRegister(code_gen_state_, reg);
      }
    }
    // Emit stack moves until the move set is empty -- each EmitMoveChain will
    // pop entries off the moves_from_stack_slot map so we can't use a simple
    // iteration here.
    while (!moves_from_stack_slot_.empty()) {
      StartEmitMoveChain(moves_from_stack_slot_.begin()->first);
    }
    for (auto [stack_slot, node] : materializing_stack_slot_moves_) {
      node->LoadToRegister(code_gen_state_, kScratchRegT);
      EmitStackMove(stack_slot, kScratchRegT);
    }
  }

  ParallelMoveResolver(ParallelMoveResolver&&) = delete;
  ParallelMoveResolver operator=(ParallelMoveResolver&&) = delete;
  ParallelMoveResolver(const ParallelMoveResolver&) = delete;
  ParallelMoveResolver operator=(const ParallelMoveResolver&) = delete;

 private:
  // The targets of moves from a source, i.e. the set of outgoing edges for a
  // node in the move graph.
  struct GapMoveTargets {
    RegListBase<RegisterT> registers;
    base::SmallVector<uint32_t, 1> stack_slots =
        base::SmallVector<uint32_t, 1>{};

    GapMoveTargets() = default;
    GapMoveTargets(GapMoveTargets&&) V8_NOEXCEPT = default;
    GapMoveTargets& operator=(GapMoveTargets&&) V8_NOEXCEPT = default;
    GapMoveTargets(const GapMoveTargets&) = delete;
    GapMoveTargets& operator=(const GapMoveTargets&) = delete;

    bool is_empty() const {
      return registers.is_empty() && stack_slots.empty();
    }
  };

#ifdef DEBUG
  void CheckNoExistingMoveToRegister(RegisterT target_reg) {
    for (RegisterT reg : kAllocatableRegistersT) {
      if (moves_from_register_[reg.code()].registers.has(target_reg)) {
        FATAL("Existing move from %s to %s", RegisterName(reg),
              RegisterName(target_reg));
      }
    }
    for (auto& [stack_slot, targets] : moves_from_stack_slot_) {
      if (targets.registers.has(target_reg)) {
        FATAL("Existing move from stack slot %d to %s", stack_slot,
              RegisterName(target_reg));
      }
    }
    if (materializing_register_moves_[target_reg.code()] != nullptr) {
      FATAL("Existing materialization of %p to %s",
            materializing_register_moves_[target_reg.code()],
            RegisterName(target_reg));
    }
  }

  void CheckNoExistingMoveToStackSlot(uint32_t target_slot) {
    for (Register reg : kAllocatableRegistersT) {
      auto& stack_slots = moves_from_register_[reg.code()].stack_slots;
      if (std::any_of(stack_slots.begin(), stack_slots.end(),
                      [&](uint32_t slot) { return slot == target_slot; })) {
        FATAL("Existing move from %s to stack slot %d", RegisterName(reg),
              target_slot);
      }
    }
    for (auto& [stack_slot, targets] : moves_from_stack_slot_) {
      auto& stack_slots = targets.stack_slots;
      if (std::any_of(stack_slots.begin(), stack_slots.end(),
                      [&](uint32_t slot) { return slot == target_slot; })) {
        FATAL("Existing move from stack slot %d to stack slot %d", stack_slot,
              target_slot);
      }
    }
    for (auto& [stack_slot, node] : materializing_stack_slot_moves_) {
      if (stack_slot == target_slot) {
        FATAL("Existing materialization of %p to stack slot %d", node,
              stack_slot);
      }
    }
  }
#else
  void CheckNoExistingMoveToRegister(RegisterT target_reg) {}
  void CheckNoExistingMoveToStackSlot(uint32_t target_slot) {}
#endif

  void RecordMoveToRegister(ValueNode* node,
                            compiler::InstructionOperand source,
                            RegisterT target_reg) {
    // There shouldn't have been another move to this register already.
    CheckNoExistingMoveToRegister(target_reg);

    if (source.IsAnyRegister()) {
      RegisterT source_reg = ToRegisterT<RegisterT>(source);
      if (target_reg != source_reg) {
        moves_from_register_[source_reg.code()].registers.set(target_reg);
      }
    } else if (source.IsAnyStackSlot()) {
      uint32_t source_slot = code_gen_state_->GetFramePointerOffsetForStackSlot(
          compiler::AllocatedOperand::cast(source));
      moves_from_stack_slot_[source_slot].registers.set(target_reg);
    } else {
      DCHECK(source.IsConstant());
      DCHECK(IsConstantNode(node->opcode()));
      materializing_register_moves_[target_reg.code()] = node;
    }
  }

  void RecordMoveToStackSlot(ValueNode* node,
                             compiler::InstructionOperand source,
                             uint32_t target_slot) {
    // There shouldn't have been another move to this stack slot already.
    CheckNoExistingMoveToStackSlot(target_slot);

    if (source.IsAnyRegister()) {
      RegisterT source_reg = ToRegisterT<RegisterT>(source);
      moves_from_register_[source_reg.code()].stack_slots.push_back(
          target_slot);
    } else if (source.IsAnyStackSlot()) {
      uint32_t source_slot = code_gen_state_->GetFramePointerOffsetForStackSlot(
          compiler::AllocatedOperand::cast(source));
      if (source_slot != target_slot) {
        moves_from_stack_slot_[source_slot].stack_slots.push_back(target_slot);
      }
    } else {
      DCHECK(source.IsConstant());
      DCHECK(IsConstantNode(node->opcode()));
      materializing_stack_slot_moves_.emplace_back(target_slot, node);
    }
  }

  // Finds and clears the targets for a given source. In terms of move graph,
  // this returns and removes all outgoing edges from the source.
  GapMoveTargets PopTargets(RegisterT source_reg) {
    return std::exchange(moves_from_register_[source_reg.code()],
                         GapMoveTargets{});
  }
  GapMoveTargets PopTargets(uint32_t source_slot) {
    auto handle = moves_from_stack_slot_.extract(source_slot);
    if (handle.empty()) return {};
    DCHECK(!handle.mapped().is_empty());
    return std::move(handle.mapped());
  }

  // Emit a single move chain starting at the given source (either a register or
  // a stack slot). This is a destructive operation on the move graph, and
  // removes the emitted edges from the graph. Subsequent calls with the same
  // source should emit no code.
  template <typename SourceT>
  void StartEmitMoveChain(SourceT source) {
    GapMoveTargets targets = PopTargets(source);
    if (targets.is_empty()) return;

    // Start recursively emitting the move chain, with this source as the start
    // of the chain.
    bool has_cycle = RecursivelyEmitMoveChainTargets(source, targets);

    // Each connected component in the move graph can only have one cycle
    // (proof: each target can only have one incoming edge, so cycles in the
    // graph can only have outgoing edges, so there's no way to connect two
    // cycles). This means that if there's a cycle, the saved value must be the
    // chain start.
    if (has_cycle) {
      Pop(kScratchRegT);
      EmitMovesFromSource(kScratchRegT, targets);
      __ RecordComment("--   * End of cycle");
    } else {
      EmitMovesFromSource(source, targets);
      __ RecordComment("--   * Chain emitted with no cycles");
    }
  }

  template <typename ChainStartT, typename SourceT>
  bool ContinueEmitMoveChain(ChainStartT chain_start, SourceT source) {
    if constexpr (std::is_same_v<ChainStartT, SourceT>) {
      // If the recursion has returned to the start of the chain, then this must
      // be a cycle.
      if (chain_start == source) {
        __ RecordComment("--   * Cycle");
        // TODO(leszeks): We push the value instead of moving it to a scratch
        // register, because there can be stack->stack moves in the chain which
        // clobber the scratch register. We could, however, perform this pop
        // lazily, by keeping track of whether the scratch register has been
        // clobbered.
        Push(chain_start);
        return true;
      }
    }

    GapMoveTargets targets = PopTargets(source);
    if (targets.is_empty()) {
      __ RecordComment("--   * End of chain");
      return false;
    }

    bool has_cycle = RecursivelyEmitMoveChainTargets(chain_start, targets);

    EmitMovesFromSource(source, targets);
    return has_cycle;
  }

  // Calls RecursivelyEmitMoveChain for each target of a source. This is used to
  // share target visiting code between StartEmitMoveChain and
  // ContinueEmitMoveChain.
  template <typename ChainStartT>
  bool RecursivelyEmitMoveChainTargets(ChainStartT chain_start,
                                       GapMoveTargets& targets) {
    bool has_cycle = false;
    for (auto target : targets.registers) {
      has_cycle |= ContinueEmitMoveChain(chain_start, target);
    }
    for (uint32_t target_slot : targets.stack_slots) {
      has_cycle |= ContinueEmitMoveChain(chain_start, target_slot);
    }
    return has_cycle;
  }

  void EmitMovesFromSource(RegisterT source_reg,
                           const GapMoveTargets& targets) {
    DCHECK(moves_from_register_[source_reg.code()].is_empty());
    for (RegisterT target_reg : targets.registers) {
      DCHECK(moves_from_register_[target_reg.code()].is_empty());
      __ Move(target_reg, source_reg);
    }
    for (uint32_t target_slot : targets.stack_slots) {
      DCHECK_EQ(moves_from_stack_slot_.find(target_slot),
                moves_from_stack_slot_.end());
      EmitStackMove(target_slot, source_reg);
    }
  }

  void EmitMovesFromSource(uint32_t source_slot,
                           const GapMoveTargets& targets) {
    DCHECK_EQ(moves_from_stack_slot_.find(source_slot),
              moves_from_stack_slot_.end());
    for (RegisterT target_reg : targets.registers) {
      DCHECK(moves_from_register_[target_reg.code()].is_empty());
      EmitStackMove(target_reg, source_slot);
    }
    for (uint32_t target_slot : targets.stack_slots) {
      DCHECK_EQ(moves_from_stack_slot_.find(target_slot),
                moves_from_stack_slot_.end());
      EmitStackMove(kScratchRegT, source_slot);
      EmitStackMove(target_slot, kScratchRegT);
    }
  }

  // The slot index used for representing slots in the move graph is the offset
  // from the frame pointer. These helpers help translate this into an actual
  // machine move.
  void EmitStackMove(uint32_t target_slot, Register source_reg) {
    __ movq(MemOperand(rbp, target_slot), source_reg);
  }
  void EmitStackMove(uint32_t target_slot, DoubleRegister source_reg) {
    __ Movsd(MemOperand(rbp, target_slot), source_reg);
  }
  void EmitStackMove(Register target_reg, uint32_t source_slot) {
    __ movq(target_reg, MemOperand(rbp, source_slot));
  }
  void EmitStackMove(DoubleRegister target_reg, uint32_t source_slot) {
    __ Movsd(target_reg, MemOperand(rbp, source_slot));
  }

  void Push(Register reg) { __ Push(reg); }
  void Push(DoubleRegister reg) { __ PushAll({reg}); }
  void Push(uint32_t stack_slot) {
    __ movq(kScratchRegister, MemOperand(rbp, stack_slot));
    __ movq(MemOperand(rsp, -1), kScratchRegister);
  }
  void Pop(Register reg) { __ Pop(reg); }
  void Pop(DoubleRegister reg) { __ PopAll({reg}); }
  void Pop(uint32_t stack_slot) {
    __ movq(kScratchRegister, MemOperand(rsp, -1));
    __ movq(MemOperand(rbp, stack_slot), kScratchRegister);
  }

  MacroAssembler* masm() { return code_gen_state_->masm(); }

  MaglevCodeGenState* code_gen_state_;

  // Keep moves to/from registers and stack slots separate -- there are a fixed
  // number of registers but an infinite number of stack slots, so the register
  // moves can be kept in a fixed size array while the stack slot moves need a
  // map.

  // moves_from_register_[source] = target.
  std::array<GapMoveTargets, RegisterT::kNumRegisters> moves_from_register_ =
      {};

  // moves_from_stack_slot_[source] = target.
  std::unordered_map<uint32_t, GapMoveTargets> moves_from_stack_slot_;

  // materializing_register_moves[target] = node.
  std::array<ValueNode*, RegisterT::kNumRegisters>
      materializing_register_moves_ = {};

  // materializing_stack_slot_moves = {(node,target), ... }.
  std::vector<std::pair<uint32_t, ValueNode*>> materializing_stack_slot_moves_;
};

class MaglevCodeGeneratingNodeProcessor {
 public:
  explicit MaglevCodeGeneratingNodeProcessor(MaglevCodeGenState* code_gen_state)
      : code_gen_state_(code_gen_state) {}

  void PreProcessGraph(MaglevCompilationInfo*, Graph* graph) {
    if (FLAG_maglev_break_on_entry) {
      __ int3();
    }

    __ BailoutIfDeoptimized(rbx);

    __ EnterFrame(StackFrame::MAGLEV);

    // Save arguments in frame.
    // TODO(leszeks): Consider eliding this frame if we don't make any calls
    // that could clobber these registers.
    __ Push(kContextRegister);
    __ Push(kJSFunctionRegister);              // Callee's JS function.
    __ Push(kJavaScriptCallArgCountRegister);  // Actual argument count.

    // TODO(v8:7700): Handle TieringState and cached optimized code. See also:
    // LoadTieringStateAndJumpIfNeedsProcessing and
    // MaybeOptimizeCodeOrTailCallOptimizedCodeSlot.

    code_gen_state_->set_untagged_slots(graph->untagged_stack_slots());
    code_gen_state_->set_tagged_slots(graph->tagged_stack_slots());

    // Initialize stack slots.
    if (graph->tagged_stack_slots() > 0) {
      ASM_CODE_COMMENT_STRING(masm(), "Initializing stack slots");
      // TODO(leszeks): Consider filling with xmm + movdqa instead.
      __ Move(rax, Immediate(0));

      // Magic value. Experimentally, an unroll size of 8 doesn't seem any worse
      // than fully unrolled pushes.
      const int kLoopUnrollSize = 8;
      int tagged_slots = graph->tagged_stack_slots();
      if (tagged_slots < 2 * kLoopUnrollSize) {
        // If the frame is small enough, just unroll the frame fill completely.
        for (int i = 0; i < tagged_slots; ++i) {
          __ pushq(rax);
        }
      } else {
        // Extract the first few slots to round to the unroll size.
        int first_slots = tagged_slots % kLoopUnrollSize;
        for (int i = 0; i < first_slots; ++i) {
          __ pushq(rax);
        }
        __ Move(rbx, Immediate(tagged_slots / kLoopUnrollSize));
        // We enter the loop unconditionally, so make sure we need to loop at
        // least once.
        DCHECK_GT(tagged_slots / kLoopUnrollSize, 0);
        Label loop;
        __ bind(&loop);
        for (int i = 0; i < kLoopUnrollSize; ++i) {
          __ pushq(rax);
        }
        __ decl(rbx);
        __ j(greater, &loop);
      }
    }
    if (graph->untagged_stack_slots() > 0) {
      // Extend rsp by the size of the remaining untagged part of the frame, no
      // need to initialise these.
      __ subq(rsp,
              Immediate(graph->untagged_stack_slots() * kSystemPointerSize));
    }
  }

  void PostProcessGraph(MaglevCompilationInfo*, Graph*) {}

  void PreProcessBasicBlock(MaglevCompilationInfo*, BasicBlock* block) {
    if (FLAG_code_comments) {
      std::stringstream ss;
      ss << "-- Block b" << graph_labeller()->BlockId(block);
      __ RecordComment(ss.str());
    }

    __ bind(block->label());
  }

  template <typename NodeT>
  void Process(NodeT* node, const ProcessingState& state) {
    if (FLAG_code_comments) {
      std::stringstream ss;
      ss << "--   " << graph_labeller()->NodeId(node) << ": "
         << PrintNode(graph_labeller(), node);
      __ RecordComment(ss.str());
    }

    if (FLAG_debug_code) {
      __ movq(kScratchRegister, rbp);
      __ subq(kScratchRegister, rsp);
      __ cmpq(kScratchRegister,
              Immediate(code_gen_state_->stack_slots() * kSystemPointerSize +
                        StandardFrameConstants::kFixedFrameSizeFromFp));
      __ Assert(equal, AbortReason::kStackAccessBelowStackPointer);
    }

    // Emit Phi moves before visiting the control node.
    if (std::is_base_of<UnconditionalControlNode, NodeT>::value) {
      EmitBlockEndGapMoves(node->template Cast<UnconditionalControlNode>(),
                           state);
    }

    node->GenerateCode(code_gen_state_, state);

    if (std::is_base_of<ValueNode, NodeT>::value) {
      ValueNode* value_node = node->template Cast<ValueNode>();
      if (value_node->is_spilled()) {
        compiler::AllocatedOperand source =
            compiler::AllocatedOperand::cast(value_node->result().operand());
        // We shouldn't spill nodes which already output to the stack.
        if (!source.IsAnyStackSlot()) {
          if (FLAG_code_comments) __ RecordComment("--   Spill:");
          if (source.IsRegister()) {
            __ movq(code_gen_state_->GetStackSlot(value_node->spill_slot()),
                    ToRegister(source));
          } else {
            __ Movsd(code_gen_state_->GetStackSlot(value_node->spill_slot()),
                     ToDoubleRegister(source));
          }
        } else {
          // Otherwise, the result source stack slot should be equal to the
          // spill slot.
          DCHECK_EQ(source.index(), value_node->spill_slot().index());
        }
      }
    }
  }

  void EmitBlockEndGapMoves(UnconditionalControlNode* node,
                            const ProcessingState& state) {
    BasicBlock* target = node->target();
    if (!target->has_state()) {
      __ RecordComment("--   Target has no state, must be a fallthrough");
      return;
    }

    int predecessor_id = state.block()->predecessor_id();

    // TODO(leszeks): Move these to fields, to allow their data structure
    // allocations to be reused. Will need some sort of state resetting.
    ParallelMoveResolver<Register> register_moves(code_gen_state_);
    ParallelMoveResolver<DoubleRegister> double_register_moves(code_gen_state_);

    // Remember what registers were assigned to by a Phi, to avoid clobbering
    // them with RegisterMoves.
    RegList registers_set_by_phis;

    __ RecordComment("--   Gap moves:");

    if (target->has_phi()) {
      Phi::List* phis = target->phis();
      for (Phi* phi : *phis) {
        // Ignore dead phis.
        // TODO(leszeks): We should remove dead phis entirely and turn this into
        // a DCHECK.
        if (!phi->has_valid_live_range()) {
          if (FLAG_code_comments) {
            std::stringstream ss;
            ss << "--   * "
               << phi->input(state.block()->predecessor_id()).operand() << " → "
               << target << " (n" << graph_labeller()->NodeId(phi)
               << ") [DEAD]";
            __ RecordComment(ss.str());
          }
          continue;
        }
        Input& input = phi->input(state.block()->predecessor_id());
        ValueNode* node = input.node();
        compiler::InstructionOperand source = input.operand();
        compiler::AllocatedOperand target =
            compiler::AllocatedOperand::cast(phi->result().operand());
        if (FLAG_code_comments) {
          std::stringstream ss;
          ss << "--   * " << source << " → " << target << " (n"
             << graph_labeller()->NodeId(phi) << ")";
          __ RecordComment(ss.str());
        }
        register_moves.RecordMove(node, source, target);
        if (target.IsAnyRegister()) {
          registers_set_by_phis.set(target.GetRegister());
        }
      }
    }

    target->state()->register_state().ForEachGeneralRegister(
        [&](Register reg, RegisterState& state) {
          // Don't clobber registers set by a Phi.
          if (registers_set_by_phis.has(reg)) return;

          ValueNode* node;
          RegisterMerge* merge;
          if (LoadMergeState(state, &node, &merge)) {
            compiler::InstructionOperand source =
                merge->operand(predecessor_id);
            if (FLAG_code_comments) {
              std::stringstream ss;
              ss << "--   * " << source << " → " << reg;
              __ RecordComment(ss.str());
            }
            register_moves.RecordMove(node, source, reg);
          }
        });

    register_moves.EmitMoves();

    __ RecordComment("--   Double gap moves:");

    target->state()->register_state().ForEachDoubleRegister(
        [&](DoubleRegister reg, RegisterState& state) {
          ValueNode* node;
          RegisterMerge* merge;
          if (LoadMergeState(state, &node, &merge)) {
            compiler::InstructionOperand source =
                merge->operand(predecessor_id);
            if (FLAG_code_comments) {
              std::stringstream ss;
              ss << "--   * " << source << " → " << reg;
              __ RecordComment(ss.str());
            }
            double_register_moves.RecordMove(node, source, reg);
          }
        });

    double_register_moves.EmitMoves();
  }

  Isolate* isolate() const { return code_gen_state_->isolate(); }
  MacroAssembler* masm() const { return code_gen_state_->masm(); }
  MaglevGraphLabeller* graph_labeller() const {
    return code_gen_state_->graph_labeller();
  }
  MaglevSafepointTableBuilder* safepoint_table_builder() const {
    return code_gen_state_->safepoint_table_builder();
  }

 private:
  MaglevCodeGenState* code_gen_state_;
};

}  // namespace

class MaglevCodeGeneratorImpl final {
 public:
  static MaybeHandle<Code> Generate(MaglevCompilationInfo* compilation_info,
                                    Graph* graph) {
    return MaglevCodeGeneratorImpl(compilation_info, graph).Generate();
  }

 private:
  static constexpr int kOptimizedOutConstantIndex = 0;

  MaglevCodeGeneratorImpl(MaglevCompilationInfo* compilation_info, Graph* graph)
      : safepoint_table_builder_(compilation_info->zone(),
                                 graph->tagged_stack_slots(),
                                 graph->untagged_stack_slots()),
        translation_array_builder_(compilation_info->zone()),
        code_gen_state_(compilation_info, safepoint_table_builder()),
        processor_(compilation_info, &code_gen_state_),
        graph_(graph),
        deopt_literals_(compilation_info->isolate()->heap()) {}

  MaybeHandle<Code> Generate() {
    EmitCode();
    if (code_gen_state_.found_unsupported_code_paths()) return {};
    EmitMetadata();
    return BuildCodeObject();
  }

  void EmitCode() {
    processor_.ProcessGraph(graph_);
    EmitDeferredCode();
    EmitDeopts();

    // Add the bytecode to the deopt literals to make sure it's held strongly.
    // TODO(leszeks): Do this fo inlined functions too.
    GetDeoptLiteral(*code_gen_state_.compilation_info()
                         ->toplevel_compilation_unit()
                         ->bytecode()
                         .object());
  }

  void EmitDeferredCode() {
    for (DeferredCodeInfo* deferred_code : code_gen_state_.deferred_code()) {
      __ RecordComment("-- Deferred block");
      __ bind(&deferred_code->deferred_code_label);
      deferred_code->Generate(&code_gen_state_, &deferred_code->return_label);
      __ Trap();
    }
  }

  void EmitDeopts() {
    deopt_exit_start_offset_ = __ pc_offset();

    // We'll emit the optimized out constant a bunch of times, so to avoid
    // looking it up in the literal map every time, add it now with the fixed
    // offset 0.
    int optimized_out_constant_index =
        GetDeoptLiteral(ReadOnlyRoots(isolate()).optimized_out());
    USE(optimized_out_constant_index);
    DCHECK_EQ(kOptimizedOutConstantIndex, optimized_out_constant_index);

    int deopt_index = 0;

    __ RecordComment("-- Non-lazy deopts");
    for (EagerDeoptInfo* deopt_info : code_gen_state_.eager_deopts()) {
      EmitEagerDeopt(deopt_info);

      // TODO(leszeks): Record source positions.
      __ RecordDeoptReason(deopt_info->reason, 0, SourcePosition::Unknown(),
                           deopt_index);
      __ bind(&deopt_info->deopt_entry_label);
      __ CallForDeoptimization(Builtin::kDeoptimizationEntry_Eager, deopt_index,
                               &deopt_info->deopt_entry_label,
                               DeoptimizeKind::kEager, nullptr, nullptr);
      deopt_index++;
    }

    __ RecordComment("-- Lazy deopts");
    int last_updated_safepoint = 0;
    for (LazyDeoptInfo* deopt_info : code_gen_state_.lazy_deopts()) {
      EmitLazyDeopt(deopt_info);

      __ bind(&deopt_info->deopt_entry_label);
      __ CallForDeoptimization(Builtin::kDeoptimizationEntry_Lazy, deopt_index,
                               &deopt_info->deopt_entry_label,
                               DeoptimizeKind::kLazy, nullptr, nullptr);

      last_updated_safepoint =
          safepoint_table_builder_.UpdateDeoptimizationInfo(
              deopt_info->deopting_call_return_pc,
              deopt_info->deopt_entry_label.pos(), last_updated_safepoint,
              deopt_index);
      deopt_index++;
    }
  }

  const InputLocation* EmitDeoptFrame(const MaglevCompilationUnit& unit,
                                      const CheckpointedInterpreterState& state,
                                      const InputLocation* input_locations) {
    if (state.parent) {
      // Deopt input locations are in the order of deopt frame emission, so
      // update the pointer after emitting the parent frame.
      input_locations =
          EmitDeoptFrame(*unit.caller(), *state.parent, input_locations);
    }

    // Returns are used for updating an accumulator or register after a lazy
    // deopt.
    const int return_offset = 0;
    const int return_count = 0;
    translation_array_builder_.BeginInterpretedFrame(
        state.bytecode_position,
        GetDeoptLiteral(*unit.shared_function_info().object()),
        unit.register_count(), return_offset, return_count);

    return EmitDeoptFrameValues(unit, state.register_frame, input_locations,
                                interpreter::Register::invalid_value());
  }

  void EmitEagerDeopt(EagerDeoptInfo* deopt_info) {
    int frame_count = 1 + deopt_info->unit.inlining_depth();
    int jsframe_count = frame_count;
    int update_feedback_count = 0;
    deopt_info->translation_index = translation_array_builder_.BeginTranslation(
        frame_count, jsframe_count, update_feedback_count);

    EmitDeoptFrame(deopt_info->unit, deopt_info->state,
                   deopt_info->input_locations);
  }

  void EmitLazyDeopt(LazyDeoptInfo* deopt_info) {
    const MaglevCompilationUnit& unit = deopt_info->unit;
    DCHECK_NULL(unit.caller());
    DCHECK_EQ(unit.inlining_depth(), 0);

    int frame_count = 1;
    int jsframe_count = 1;
    int update_feedback_count = 0;
    deopt_info->translation_index = translation_array_builder_.BeginTranslation(
        frame_count, jsframe_count, update_feedback_count);

    // Return offsets are counted from the end of the translation frame, which
    // is the array [parameters..., locals..., accumulator].
    int return_offset;
    if (deopt_info->result_location ==
        interpreter::Register::virtual_accumulator()) {
      return_offset = 0;
    } else if (deopt_info->result_location.is_parameter()) {
      // This is slightly tricky to reason about because of zero indexing and
      // fence post errors. As an example, consider a frame with 2 locals and
      // 2 parameters, where we want argument index 1 -- looking at the array
      // in reverse order we have:
      //   [acc, r1, r0, a1, a0]
      //                  ^
      // and this calculation gives, correctly:
      //   2 + 2 - 1 = 3
      return_offset = unit.register_count() + unit.parameter_count() -
                      deopt_info->result_location.ToParameterIndex();
    } else {
      return_offset =
          unit.register_count() - deopt_info->result_location.index();
    }
    // TODO(leszeks): Support lazy deopts with multiple return values.
    int return_count = 1;
    translation_array_builder_.BeginInterpretedFrame(
        deopt_info->state.bytecode_position,
        GetDeoptLiteral(*unit.shared_function_info().object()),
        unit.register_count(), return_offset, return_count);

    EmitDeoptFrameValues(unit, deopt_info->state.register_frame,
                         deopt_info->input_locations,
                         deopt_info->result_location);
  }

  void EmitDeoptStoreRegister(const compiler::AllocatedOperand& operand,
                              ValueRepresentation repr) {
    switch (repr) {
      case ValueRepresentation::kTagged:
        translation_array_builder_.StoreRegister(operand.GetRegister());
        break;
      case ValueRepresentation::kInt32:
        translation_array_builder_.StoreInt32Register(operand.GetRegister());
        break;
      case ValueRepresentation::kFloat64:
        translation_array_builder_.StoreDoubleRegister(
            operand.GetDoubleRegister());
        break;
    }
  }

  void EmitDeoptStoreStackSlot(const compiler::AllocatedOperand& operand,
                               ValueRepresentation repr) {
    int stack_slot = DeoptStackSlotFromStackSlot(operand);
    switch (repr) {
      case ValueRepresentation::kTagged:
        translation_array_builder_.StoreStackSlot(stack_slot);
        break;
      case ValueRepresentation::kInt32:
        translation_array_builder_.StoreInt32StackSlot(stack_slot);
        break;
      case ValueRepresentation::kFloat64:
        translation_array_builder_.StoreDoubleStackSlot(stack_slot);
        break;
    }
  }

  void EmitDeoptFrameSingleValue(ValueNode* value,
                                 const InputLocation& input_location) {
    if (input_location.operand().IsConstant()) {
      translation_array_builder_.StoreLiteral(
          GetDeoptLiteral(*value->Reify(isolate())));
    } else {
      const compiler::AllocatedOperand& operand =
          compiler::AllocatedOperand::cast(input_location.operand());
      ValueRepresentation repr = value->properties().value_representation();
      if (operand.IsAnyRegister()) {
        EmitDeoptStoreRegister(operand, repr);
      } else {
        EmitDeoptStoreStackSlot(operand, repr);
      }
    }
  }

  constexpr int DeoptStackSlotIndexFromFPOffset(int offset) {
    return 1 - offset / kSystemPointerSize;
  }

  int DeoptStackSlotFromStackSlot(const compiler::AllocatedOperand& operand) {
    return DeoptStackSlotIndexFromFPOffset(
        code_gen_state_.GetFramePointerOffsetForStackSlot(operand));
  }

  const InputLocation* EmitDeoptFrameValues(
      const MaglevCompilationUnit& compilation_unit,
      const CompactInterpreterFrameState* checkpoint_state,
      const InputLocation* input_locations,
      interpreter::Register result_location) {
    // Closure
    if (compilation_unit.inlining_depth() == 0) {
      int closure_index = DeoptStackSlotIndexFromFPOffset(
          StandardFrameConstants::kFunctionOffset);
      translation_array_builder_.StoreStackSlot(closure_index);
    } else {
      translation_array_builder_.StoreLiteral(
          GetDeoptLiteral(*compilation_unit.function().object()));
    }

    // TODO(leszeks): The input locations array happens to be in the same order
    // as parameters+context+locals+accumulator are accessed here. We should
    // make this clearer and guard against this invariant failing.
    const InputLocation* input_location = input_locations;

    // Parameters
    {
      int i = 0;
      checkpoint_state->ForEachParameter(
          compilation_unit, [&](ValueNode* value, interpreter::Register reg) {
            DCHECK_EQ(reg.ToParameterIndex(), i);
            if (reg != result_location) {
              EmitDeoptFrameSingleValue(value, *input_location);
            } else {
              translation_array_builder_.StoreLiteral(
                  kOptimizedOutConstantIndex);
            }
            i++;
            input_location++;
          });
    }

    // Context
    ValueNode* value = checkpoint_state->context(compilation_unit);
    EmitDeoptFrameSingleValue(value, *input_location);
    input_location++;

    // Locals
    {
      int i = 0;
      checkpoint_state->ForEachLocal(
          compilation_unit, [&](ValueNode* value, interpreter::Register reg) {
            DCHECK_LE(i, reg.index());
            if (reg == result_location) {
              input_location++;
              return;
            }
            while (i < reg.index()) {
              translation_array_builder_.StoreLiteral(
                  kOptimizedOutConstantIndex);
              i++;
            }
            DCHECK_EQ(i, reg.index());
            EmitDeoptFrameSingleValue(value, *input_location);
            i++;
            input_location++;
          });
      while (i < compilation_unit.register_count()) {
        translation_array_builder_.StoreLiteral(kOptimizedOutConstantIndex);
        i++;
      }
    }

    // Accumulator
    {
      if (checkpoint_state->liveness()->AccumulatorIsLive() &&
          result_location != interpreter::Register::virtual_accumulator()) {
        ValueNode* value = checkpoint_state->accumulator(compilation_unit);
        EmitDeoptFrameSingleValue(value, *input_location);
      } else {
        translation_array_builder_.StoreLiteral(kOptimizedOutConstantIndex);
      }
    }

    return input_location;
  }

  void EmitMetadata() {
    // Final alignment before starting on the metadata section.
    masm()->Align(Code::kMetadataAlignment);

    safepoint_table_builder()->Emit(masm());
  }

  MaybeHandle<Code> BuildCodeObject() {
    CodeDesc desc;
    static constexpr int kNoHandlerTableOffset = 0;
    masm()->GetCode(isolate(), &desc, safepoint_table_builder(),
                    kNoHandlerTableOffset);
    return Factory::CodeBuilder{isolate(), desc, CodeKind::MAGLEV}
        .set_stack_slots(stack_slot_count_with_fixed_frame())
        .set_deoptimization_data(GenerateDeoptimizationData())
        .TryBuild();
  }

  Handle<DeoptimizationData> GenerateDeoptimizationData() {
    int eager_deopt_count =
        static_cast<int>(code_gen_state_.eager_deopts().size());
    int lazy_deopt_count =
        static_cast<int>(code_gen_state_.lazy_deopts().size());
    int deopt_count = lazy_deopt_count + eager_deopt_count;
    if (deopt_count == 0) {
      return DeoptimizationData::Empty(isolate());
    }
    Handle<DeoptimizationData> data =
        DeoptimizationData::New(isolate(), deopt_count, AllocationType::kOld);

    Handle<TranslationArray> translation_array =
        translation_array_builder_.ToTranslationArray(isolate()->factory());

    data->SetTranslationByteArray(*translation_array);
    // TODO(leszeks): Fix with the real inlined function count.
    data->SetInlinedFunctionCount(Smi::zero());
    // TODO(leszeks): Support optimization IDs
    data->SetOptimizationId(Smi::zero());

    DCHECK_NE(deopt_exit_start_offset_, -1);
    data->SetDeoptExitStart(Smi::FromInt(deopt_exit_start_offset_));
    data->SetEagerDeoptCount(Smi::FromInt(eager_deopt_count));
    data->SetLazyDeoptCount(Smi::FromInt(lazy_deopt_count));

    data->SetSharedFunctionInfo(*code_gen_state_.compilation_info()
                                     ->toplevel_compilation_unit()
                                     ->shared_function_info()
                                     .object());

    Handle<DeoptimizationLiteralArray> literals =
        isolate()->factory()->NewDeoptimizationLiteralArray(
            deopt_literals_.size());
    IdentityMap<int, base::DefaultAllocationPolicy>::IteratableScope iterate(
        &deopt_literals_);
    for (auto it = iterate.begin(); it != iterate.end(); ++it) {
      literals->set(*it.entry(), it.key());
    }
    data->SetLiteralArray(*literals);

    // TODO(leszeks): Fix with the real inlining positions.
    Handle<PodArray<InliningPosition>> inlining_positions =
        PodArray<InliningPosition>::New(isolate(), 0);
    data->SetInliningPositions(*inlining_positions);

    // TODO(leszeks): Fix once we have OSR.
    BytecodeOffset osr_offset = BytecodeOffset::None();
    data->SetOsrBytecodeOffset(Smi::FromInt(osr_offset.ToInt()));
    data->SetOsrPcOffset(Smi::FromInt(-1));

    // Populate deoptimization entries.
    int i = 0;
    for (EagerDeoptInfo* deopt_info : code_gen_state_.eager_deopts()) {
      DCHECK_NE(deopt_info->translation_index, -1);
      data->SetBytecodeOffset(i, deopt_info->state.bytecode_position);
      data->SetTranslationIndex(i, Smi::FromInt(deopt_info->translation_index));
      data->SetPc(i, Smi::FromInt(deopt_info->deopt_entry_label.pos()));
#ifdef DEBUG
      data->SetNodeId(i, Smi::FromInt(i));
#endif  // DEBUG
      i++;
    }
    for (LazyDeoptInfo* deopt_info : code_gen_state_.lazy_deopts()) {
      DCHECK_NE(deopt_info->translation_index, -1);
      data->SetBytecodeOffset(i, deopt_info->state.bytecode_position);
      data->SetTranslationIndex(i, Smi::FromInt(deopt_info->translation_index));
      data->SetPc(i, Smi::FromInt(deopt_info->deopt_entry_label.pos()));
#ifdef DEBUG
      data->SetNodeId(i, Smi::FromInt(i));
#endif  // DEBUG
      i++;
    }

    return data;
  }

  int stack_slot_count() const { return code_gen_state_.stack_slots(); }
  int stack_slot_count_with_fixed_frame() const {
    return stack_slot_count() + StandardFrameConstants::kFixedSlotCount;
  }

  Isolate* isolate() const {
    return code_gen_state_.compilation_info()->isolate();
  }
  MacroAssembler* masm() { return code_gen_state_.masm(); }
  MaglevSafepointTableBuilder* safepoint_table_builder() {
    return &safepoint_table_builder_;
  }
  TranslationArrayBuilder* translation_array_builder() {
    return &translation_array_builder_;
  }

  int GetDeoptLiteral(Object obj) {
    IdentityMapFindResult<int> res = deopt_literals_.FindOrInsert(obj);
    if (!res.already_exists) {
      DCHECK_EQ(0, *res.entry);
      *res.entry = deopt_literals_.size() - 1;
    }
    return *res.entry;
  }

  MaglevSafepointTableBuilder safepoint_table_builder_;
  TranslationArrayBuilder translation_array_builder_;
  MaglevCodeGenState code_gen_state_;
  GraphProcessor<MaglevCodeGeneratingNodeProcessor> processor_;
  Graph* const graph_;
  IdentityMap<int, base::DefaultAllocationPolicy> deopt_literals_;

  int deopt_exit_start_offset_ = -1;
};

// static
MaybeHandle<Code> MaglevCodeGenerator::Generate(
    MaglevCompilationInfo* compilation_info, Graph* graph) {
  return MaglevCodeGeneratorImpl::Generate(compilation_info, graph);
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
