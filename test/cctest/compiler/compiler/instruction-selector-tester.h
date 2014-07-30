// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CCTEST_COMPILER_INSTRUCTION_SELECTOR_TEST_H_
#define V8_CCTEST_COMPILER_INSTRUCTION_SELECTOR_TEST_H_

#include <deque>
#include <set>

#include "src/compiler/instruction-selector.h"
#include "src/compiler/raw-machine-assembler.h"
#include "src/ostreams.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {
namespace compiler {

typedef std::set<int> VirtualRegisterSet;

enum InstructionSelectorTesterMode { kTargetMode, kInternalMode };

class InstructionSelectorTester : public HandleAndZoneScope,
                                  public RawMachineAssembler {
 public:
  enum Mode { kTargetMode, kInternalMode };

  static const int kParameterCount = 3;
  static MachineRepresentation* BuildParameterArray(Zone* zone) {
    MachineRepresentation* array =
        zone->NewArray<MachineRepresentation>(kParameterCount);
    for (int i = 0; i < kParameterCount; ++i) {
      array[i] = kMachineWord32;
    }
    return array;
  }

  explicit InstructionSelectorTester(Mode mode = kTargetMode)
      : RawMachineAssembler(
            new (main_zone()) Graph(main_zone()), new (main_zone())
            MachineCallDescriptorBuilder(kMachineWord32, kParameterCount,
                                         BuildParameterArray(main_zone())),
            MachineOperatorBuilder::pointer_rep()),
        mode_(mode) {}

  void SelectInstructions() {
    OFStream out(stdout);
    Schedule* schedule = Export();
    CHECK_NE(0, graph()->NodeCount());
    CompilationInfo info(main_isolate(), main_zone());
    Linkage linkage(&info, call_descriptor());
    InstructionSequence sequence(&linkage, graph(), schedule);
    SourcePositionTable source_positions(graph());
    InstructionSelector selector(&sequence, &source_positions);
    selector.SelectInstructions();
    out << "--- Code sequence after instruction selection --- " << endl
        << sequence;
    for (InstructionSequence::const_iterator i = sequence.begin();
         i != sequence.end(); ++i) {
      Instruction* instr = *i;
      if (instr->opcode() < 0) continue;
      if (mode_ == kTargetMode) {
        switch (ArchOpcodeField::decode(instr->opcode())) {
#define CASE(Name) \
  case k##Name:    \
    break;
          TARGET_ARCH_OPCODE_LIST(CASE)
#undef CASE
          default:
            continue;
        }
      }
      code.push_back(instr);
    }
    for (int vreg = 0; vreg < sequence.VirtualRegisterCount(); ++vreg) {
      if (sequence.IsDouble(vreg)) {
        CHECK(!sequence.IsReference(vreg));
        doubles.insert(vreg);
      }
      if (sequence.IsReference(vreg)) {
        CHECK(!sequence.IsDouble(vreg));
        references.insert(vreg);
      }
    }
    immediates.assign(sequence.immediates().begin(),
                      sequence.immediates().end());
  }

  int32_t ToInt32(const InstructionOperand* operand) const {
    size_t i = operand->index();
    CHECK(i < immediates.size());
    CHECK_EQ(InstructionOperand::IMMEDIATE, operand->kind());
    return immediates[i].ToInt32();
  }

  std::deque<Instruction*> code;
  VirtualRegisterSet doubles;
  VirtualRegisterSet references;
  std::deque<Constant> immediates;

 private:
  Mode mode_;
};


static inline void CheckSameVreg(InstructionOperand* exp,
                                 InstructionOperand* val) {
  CHECK_EQ(InstructionOperand::UNALLOCATED, exp->kind());
  CHECK_EQ(InstructionOperand::UNALLOCATED, val->kind());
  CHECK_EQ(UnallocatedOperand::cast(exp)->virtual_register(),
           UnallocatedOperand::cast(val)->virtual_register());
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_CCTEST_COMPILER_INSTRUCTION_SELECTOR_TEST_H_
