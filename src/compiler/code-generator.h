// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_CODE_GENERATOR_H_
#define V8_COMPILER_CODE_GENERATOR_H_

#include <deque>

#include "src/compiler/gap-resolver.h"
#include "src/compiler/instruction.h"
#include "src/deoptimizer.h"
#include "src/macro-assembler.h"
#include "src/safepoint-table.h"

namespace v8 {
namespace internal {
namespace compiler {

// Generates native code for a sequence of instructions.
class CodeGenerator V8_FINAL : public GapResolver::Assembler {
 public:
  explicit CodeGenerator(InstructionSequence* code);

  // Generate native code.
  Handle<Code> GenerateCode();

  InstructionSequence* code() const { return code_; }
  Frame* frame() const { return code()->frame(); }
  Graph* graph() const { return code()->graph(); }
  Isolate* isolate() const { return zone()->isolate(); }
  Linkage* linkage() const { return code()->linkage(); }
  Schedule* schedule() const { return code()->schedule(); }

 private:
  MacroAssembler* masm() { return &masm_; }
  GapResolver* resolver() { return &resolver_; }
  SafepointTableBuilder* safepoints() { return &safepoints_; }
  Zone* zone() const { return code()->zone(); }

  // Checks if {block} will appear directly after {current_block_} when
  // assembling code, in which case, a fall-through can be used.
  bool IsNextInAssemblyOrder(const BasicBlock* block) const {
    return block->rpo_number_ == (current_block_->rpo_number_ + 1) &&
           block->deferred_ == current_block_->deferred_;
  }

  // Record a safepoint with the given pointer map.
  Safepoint::Id RecordSafepoint(PointerMap* pointers, Safepoint::Kind kind,
                                int arguments, Safepoint::DeoptMode deopt_mode);

  // Assemble code for the specified instruction.
  void AssembleInstruction(Instruction* instr);
  void AssembleSourcePosition(SourcePositionInstruction* instr);
  void AssembleGap(GapInstruction* gap);

  // ===========================================================================
  // ============= Architecture-specific code generation methods. ==============
  // ===========================================================================

  void AssembleArchInstruction(Instruction* instr);
  void AssembleArchBranch(Instruction* instr, FlagsCondition condition);
  void AssembleArchBoolean(Instruction* instr, FlagsCondition condition);

  // Generates an architecture-specific, descriptor-specific prologue
  // to set up a stack frame.
  void AssemblePrologue();
  // Generates an architecture-specific, descriptor-specific return sequence
  // to tear down a stack frame.
  void AssembleReturn();

  // ===========================================================================
  // ============== Architecture-specific gap resolver methods. ================
  // ===========================================================================

  // Interface used by the gap resolver to emit moves and swaps.
  virtual void AssembleMove(InstructionOperand* source,
                            InstructionOperand* destination) V8_OVERRIDE;
  virtual void AssembleSwap(InstructionOperand* source,
                            InstructionOperand* destination) V8_OVERRIDE;

  // ===========================================================================
  // Deoptimization table construction
  void AddSafepointAndDeopt(Instruction* instr);
  void UpdateSafepointsWithDeoptimizationPc();
  void RecordLazyDeoptimizationEntry(Instruction* instr,
                                     Safepoint::Id safepoint_id);
  void PopulateDeoptimizationData(Handle<Code> code);
  int DefineDeoptimizationLiteral(Handle<Object> literal);
  int BuildTranslation(Instruction* instr, int frame_state_offset);
  void AddTranslationForOperand(Translation* translation, Instruction* instr,
                                InstructionOperand* op);
  void AddNopForSmiCodeInlining();
  // ===========================================================================

  class LazyDeoptimizationEntry V8_FINAL {
   public:
    LazyDeoptimizationEntry(int position_after_call, Label* continuation,
                            Label* deoptimization, Safepoint::Id safepoint_id)
        : position_after_call_(position_after_call),
          continuation_(continuation),
          deoptimization_(deoptimization),
          safepoint_id_(safepoint_id) {}

    int position_after_call() const { return position_after_call_; }
    Label* continuation() const { return continuation_; }
    Label* deoptimization() const { return deoptimization_; }
    Safepoint::Id safepoint_id() const { return safepoint_id_; }

   private:
    int position_after_call_;
    Label* continuation_;
    Label* deoptimization_;
    Safepoint::Id safepoint_id_;
  };

  struct DeoptimizationState : ZoneObject {
    int translation_id_;

    explicit DeoptimizationState(int translation_id)
        : translation_id_(translation_id) {}
  };

  InstructionSequence* code_;
  BasicBlock* current_block_;
  SourcePosition current_source_position_;
  MacroAssembler masm_;
  GapResolver resolver_;
  SafepointTableBuilder safepoints_;
  ZoneDeque<LazyDeoptimizationEntry> lazy_deoptimization_entries_;
  ZoneDeque<DeoptimizationState*> deoptimization_states_;
  ZoneDeque<Handle<Object> > deoptimization_literals_;
  TranslationBuffer translations_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_CODE_GENERATOR_H
