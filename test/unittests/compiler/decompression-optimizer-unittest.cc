// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/decompression-optimizer.h"

#include "test/unittests/compiler/graph-unittest.h"

namespace v8 {
namespace internal {
namespace compiler {

class DecompressionOptimizerTest : public GraphTest {
 public:
  DecompressionOptimizerTest()
      : GraphTest(),
        machine_(zone(), MachineType::PointerRepresentation(),
                 MachineOperatorBuilder::kNoFlags) {}
  ~DecompressionOptimizerTest() override = default;

 protected:
  void Reduce() {
    DecompressionOptimizer decompression_optimizer(zone(), graph(), common(),
                                                   machine());
    decompression_optimizer.Reduce();
  }

  MachineRepresentation CompressedMachRep(MachineRepresentation mach_rep) {
    if (mach_rep == MachineRepresentation::kTagged) {
      return MachineRepresentation::kCompressed;
    } else {
      DCHECK_EQ(mach_rep, MachineRepresentation::kTaggedPointer);
      return MachineRepresentation::kCompressedPointer;
    }
  }

  MachineRepresentation CompressedMachRep(MachineType type) {
    return CompressedMachRep(type.representation());
  }

  MachineRepresentation LoadMachRep(Node* node) {
    return LoadRepresentationOf(node->op()).representation();
  }
  StoreRepresentation CreateStoreRep(MachineType type) {
    return StoreRepresentation(type.representation(),
                               WriteBarrierKind::kFullWriteBarrier);
  }

  const MachineType types[2] = {MachineType::AnyTagged(),
                                MachineType::TaggedPointer()};

  const Handle<HeapNumber> heap_constants[15] = {
      factory()->NewHeapNumber(0.0),
      factory()->NewHeapNumber(-0.0),
      factory()->NewHeapNumber(11.2),
      factory()->NewHeapNumber(-11.2),
      factory()->NewHeapNumber(3.1415 + 1.4142),
      factory()->NewHeapNumber(3.1415 - 1.4142),
      factory()->NewHeapNumber(0x0000000000000000),
      factory()->NewHeapNumber(0x0000000000000001),
      factory()->NewHeapNumber(0x0000FFFFFFFF0000),
      factory()->NewHeapNumber(0x7FFFFFFFFFFFFFFF),
      factory()->NewHeapNumber(0x8000000000000000),
      factory()->NewHeapNumber(0x8000000000000001),
      factory()->NewHeapNumber(0x8000FFFFFFFF0000),
      factory()->NewHeapNumber(0x8FFFFFFFFFFFFFFF),
      factory()->NewHeapNumber(0xFFFFFFFFFFFFFFFF)};

  MachineOperatorBuilder* machine() { return &machine_; }

 private:
  MachineOperatorBuilder machine_;
};

// -----------------------------------------------------------------------------
// Direct Load into Store.

TEST_F(DecompressionOptimizerTest, DirectLoadStore) {
  // Define variables.
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);

  // Test for both AnyTagged and TaggedPointer.
  for (size_t i = 0; i < arraysize(types); ++i) {
    // Create the graph.
    Node* base_pointer = graph()->NewNode(machine()->Load(types[i]), object,
                                          index, effect, control);
    Node* value = graph()->NewNode(machine()->Load(types[i]), base_pointer,
                                   index, effect, control);
    graph()->SetEnd(graph()->NewNode(machine()->Store(CreateStoreRep(types[i])),
                                     object, index, value, effect, control));

    // Change the nodes, and test the change.
    Reduce();
    EXPECT_EQ(LoadMachRep(base_pointer), types[i].representation());
    EXPECT_EQ(LoadMachRep(value), CompressedMachRep(types[i]));
  }
}

// -----------------------------------------------------------------------------
// Word32 Operations.

TEST_F(DecompressionOptimizerTest, Word32EqualTwoDecompresses) {
  // Define variables.
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);

  // Test for both AnyTagged and TaggedPointer, for both loads.
  for (size_t i = 0; i < arraysize(types); ++i) {
    for (size_t j = 0; j < arraysize(types); ++j) {
      // Create the graph.
      Node* load_1 = graph()->NewNode(machine()->Load(types[i]), object, index,
                                      effect, control);
      Node* change_to_tagged_1 =
          graph()->NewNode(machine()->ChangeTaggedToCompressed(), load_1);
      Node* load_2 = graph()->NewNode(machine()->Load(types[j]), object, index,
                                      effect, control);
      Node* change_to_tagged_2 =
          graph()->NewNode(machine()->ChangeTaggedToCompressed(), load_2);
      graph()->SetEnd(graph()->NewNode(machine()->Word32Equal(),
                                       change_to_tagged_1, change_to_tagged_2));

      // Change the nodes, and test the change.
      Reduce();
      EXPECT_EQ(LoadMachRep(load_1), CompressedMachRep(types[i]));
      EXPECT_EQ(LoadMachRep(load_2), CompressedMachRep(types[j]));
    }
  }
}

TEST_F(DecompressionOptimizerTest, Word32EqualDecompressAndConstant) {
  // Define variables.
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);

  // Test for both AnyTagged and TaggedPointer.
  for (size_t i = 0; i < arraysize(types); ++i) {
    for (size_t j = 0; j < arraysize(heap_constants); ++j) {
      // Create the graph.
      Node* load = graph()->NewNode(machine()->Load(types[i]), object, index,
                                    effect, control);
      Node* change_to_tagged =
          graph()->NewNode(machine()->ChangeTaggedToCompressed(), load);
      Node* constant =
          graph()->NewNode(common()->HeapConstant(heap_constants[j]));
      graph()->SetEnd(graph()->NewNode(machine()->Word32Equal(),
                                       change_to_tagged, constant));

      // Change the nodes, and test the change.
      Reduce();
      EXPECT_EQ(LoadMachRep(load), CompressedMachRep(types[i]));
      EXPECT_EQ(constant->opcode(), IrOpcode::kCompressedHeapConstant);
    }
  }
}

TEST_F(DecompressionOptimizerTest, Word32AndSmiCheck) {
  // Define variables.
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);

  // Test for both AnyTagged and TaggedPointer.
  for (size_t i = 0; i < arraysize(types); ++i) {
    // Create the graph.
    Node* load = graph()->NewNode(machine()->Load(types[i]), object, index,
                                  effect, control);
    Node* smi_tag_mask = graph()->NewNode(common()->Int32Constant(kSmiTagMask));
    Node* word32_and =
        graph()->NewNode(machine()->Word32And(), load, smi_tag_mask);
    Node* smi_tag = graph()->NewNode(common()->Int32Constant(kSmiTag));
    graph()->SetEnd(
        graph()->NewNode(machine()->Word32Equal(), word32_and, smi_tag));
    // Change the nodes, and test the change.
    Reduce();
    EXPECT_EQ(LoadMachRep(load), CompressedMachRep(types[i]));
  }
}

TEST_F(DecompressionOptimizerTest, Word32ShlSmiTag) {
  // Define variables.
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);

  // Test only for AnyTagged, since TaggedPointer can't be Smi tagged.
  // Create the graph.
  Node* load = graph()->NewNode(machine()->Load(MachineType::AnyTagged()),
                                object, index, effect, control);
  Node* truncation = graph()->NewNode(machine()->TruncateInt64ToInt32(), load);
  Node* smi_shift_bits =
      graph()->NewNode(common()->Int32Constant(kSmiShiftSize + kSmiTagSize));
  Node* word32_shl =
      graph()->NewNode(machine()->Word32Shl(), truncation, smi_shift_bits);
  graph()->SetEnd(
      graph()->NewNode(machine()->ChangeInt32ToInt64(), word32_shl));
  // Change the nodes, and test the change.
  Reduce();
  EXPECT_EQ(LoadMachRep(load), CompressedMachRep(MachineType::AnyTagged()));
}

TEST_F(DecompressionOptimizerTest, Word32SarSmiUntag) {
  // Define variables.
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);

  // Test only for AnyTagged, since TaggedPointer can't be Smi tagged.
  // Create the graph.
  Node* load = graph()->NewNode(machine()->Load(MachineType::AnyTagged()),
                                object, index, effect, control);
  Node* truncation = graph()->NewNode(machine()->TruncateInt64ToInt32(), load);
  Node* smi_shift_bits =
      graph()->NewNode(common()->Int32Constant(kSmiShiftSize + kSmiTagSize));
  Node* word32_sar =
      graph()->NewNode(machine()->Word32Sar(), truncation, smi_shift_bits);
  graph()->SetEnd(
      graph()->NewNode(machine()->ChangeInt32ToInt64(), word32_sar));
  // Change the nodes, and test the change.
  Reduce();
  EXPECT_EQ(LoadMachRep(load), CompressedMachRep(MachineType::AnyTagged()));
}

// -----------------------------------------------------------------------------
// FrameState and TypedStateValues interaction.

TEST_F(DecompressionOptimizerTest, TypedStateValues) {
  // Define variables.
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);
  const int number_of_inputs = 2;
  const ZoneVector<MachineType>* types_for_state_values =
      new (graph()->zone()->New(sizeof(ZoneVector<MachineType>)))
          ZoneVector<MachineType>(number_of_inputs, graph()->zone());
  SparseInputMask dense = SparseInputMask::Dense();

  // Test for both AnyTagged and TaggedPointer.
  for (size_t i = 0; i < arraysize(types); ++i) {
    for (size_t j = 0; j < arraysize(heap_constants); ++j) {
      // Create the graph.
      Node* load = graph()->NewNode(machine()->Load(types[i]), object, index,
                                    effect, control);
      Node* constant_1 =
          graph()->NewNode(common()->HeapConstant(heap_constants[j]));
      Node* typed_state_values = graph()->NewNode(
          common()->TypedStateValues(types_for_state_values, dense), load,
          constant_1);
      Node* constant_2 =
          graph()->NewNode(common()->HeapConstant(heap_constants[j]));
      graph()->SetEnd(graph()->NewNode(
          common()->FrameState(BailoutId::None(),
                               OutputFrameStateCombine::Ignore(), nullptr),
          typed_state_values, typed_state_values, typed_state_values,
          constant_2, UndefinedConstant(), graph()->start()));

      // Change the nodes, and test the change.
      Reduce();
      EXPECT_EQ(LoadMachRep(load), CompressedMachRep(types[i]));
      EXPECT_EQ(constant_1->opcode(), IrOpcode::kCompressedHeapConstant);
      EXPECT_EQ(constant_2->opcode(), IrOpcode::kCompressedHeapConstant);
    }
  }
}

// -----------------------------------------------------------------------------
// Phi

TEST_F(DecompressionOptimizerTest, PhiDecompressOrNot) {
  // Define variables.
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);
  const int number_of_inputs = 2;

  // Test for both AnyTagged and TaggedPointer.
  for (size_t i = 0; i < arraysize(types); ++i) {
    for (size_t j = 0; j < arraysize(heap_constants); ++j) {
      // Create the graph.
      // Base pointer
      Node* load_1 = graph()->NewNode(machine()->Load(types[i]), object, index,
                                      effect, control);
      Node* constant_1 =
          graph()->NewNode(common()->HeapConstant(heap_constants[j]));
      Node* phi_1 = graph()->NewNode(
          common()->Phi(types[i].representation(), number_of_inputs), load_1,
          constant_1, control);

      // Value
      Node* load_2 = graph()->NewNode(machine()->Load(types[i]), object, index,
                                      effect, control);
      Node* constant_2 =
          graph()->NewNode(common()->HeapConstant(heap_constants[j]));
      Node* phi_2 = graph()->NewNode(
          common()->Phi(types[i].representation(), number_of_inputs), load_2,
          constant_2, control);

      graph()->SetEnd(
          graph()->NewNode(machine()->Store(CreateStoreRep(types[i])), phi_1,
                           index, phi_2, effect, control));

      // Change the nodes, and test the change.
      Reduce();
      // Base pointer should not be compressed.
      EXPECT_EQ(LoadMachRep(load_1), types[i].representation());
      EXPECT_EQ(constant_1->opcode(), IrOpcode::kHeapConstant);
      EXPECT_EQ(PhiRepresentationOf(phi_1->op()), types[i].representation());
      // Value should be compressed.
      EXPECT_EQ(LoadMachRep(load_2), CompressedMachRep(types[i]));
      EXPECT_EQ(constant_2->opcode(), IrOpcode::kCompressedHeapConstant);
      EXPECT_EQ(PhiRepresentationOf(phi_2->op()), CompressedMachRep(types[i]));
    }
  }
}

TEST_F(DecompressionOptimizerTest, CascadingPhi) {
  // Define variables.
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);
  const int number_of_inputs = 2;

  // Test for both AnyTagged and TaggedPointer.
  for (size_t i = 0; i < arraysize(types); ++i) {
    // Create the graph.
    Node* load_1 = graph()->NewNode(machine()->Load(types[i]), object, index,
                                    effect, control);
    Node* load_2 = graph()->NewNode(machine()->Load(types[i]), object, index,
                                    effect, control);
    Node* load_3 = graph()->NewNode(machine()->Load(types[i]), object, index,
                                    effect, control);
    Node* load_4 = graph()->NewNode(machine()->Load(types[i]), object, index,
                                    effect, control);

    Node* phi_1 = graph()->NewNode(
        common()->Phi(types[i].representation(), number_of_inputs), load_1,
        load_2, control);
    Node* phi_2 = graph()->NewNode(
        common()->Phi(types[i].representation(), number_of_inputs), load_3,
        load_4, control);

    Node* final_phi = graph()->NewNode(
        common()->Phi(types[i].representation(), number_of_inputs), phi_1,
        phi_2, control);

    // Value
    graph()->SetEnd(final_phi);
    // Change the nodes, and test the change.
    Reduce();
    // Loads are all compressed
    EXPECT_EQ(LoadMachRep(load_1), CompressedMachRep(types[i]));
    EXPECT_EQ(LoadMachRep(load_2), CompressedMachRep(types[i]));
    EXPECT_EQ(LoadMachRep(load_3), CompressedMachRep(types[i]));
    EXPECT_EQ(LoadMachRep(load_4), CompressedMachRep(types[i]));
    // Phis too
    EXPECT_EQ(PhiRepresentationOf(phi_1->op()), CompressedMachRep(types[i]));
    EXPECT_EQ(PhiRepresentationOf(phi_2->op()), CompressedMachRep(types[i]));
    EXPECT_EQ(PhiRepresentationOf(final_phi->op()),
              CompressedMachRep(types[i]));
  }
}

TEST_F(DecompressionOptimizerTest, PhiWithOneCompressedAndOneTagged) {
  // If the phi is Compressed but one of the inputs is Tagged, then we insert a
  // ChangeTaggedToCompressed node.

  // Define variables.
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);
  const int number_of_inputs = 2;

  // Test for both AnyTagged and TaggedPointer.
  for (size_t i = 0; i < arraysize(types); ++i) {
    for (size_t j = 0; j < arraysize(heap_constants); ++j) {
      // Create the graph.
      // Base pointer in load_2, and phi input for value
      Node* load_1 = graph()->NewNode(machine()->Load(types[i]), object, index,
                                      effect, control);

      // load_2 blocks load_1 from being compressed.
      Node* load_2 = graph()->NewNode(machine()->Load(types[i]), load_1, index,
                                      effect, control);
      Node* phi = graph()->NewNode(
          common()->Phi(types[i].representation(), number_of_inputs), load_1,
          load_2, control);

      graph()->SetEnd(
          graph()->NewNode(machine()->Store(CreateStoreRep(types[i])), object,
                           index, phi, effect, control));

      // Change the nodes, and test the change.
      Reduce();
      EXPECT_EQ(LoadMachRep(load_1), types[i].representation());
      EXPECT_EQ(LoadMachRep(load_2), CompressedMachRep(types[i]));
      EXPECT_EQ(PhiRepresentationOf(phi->op()), CompressedMachRep(types[i]));
    }
  }
}

// -----------------------------------------------------------------------------
// Int cases.

TEST_F(DecompressionOptimizerTest, Int32LessThanOrEqualFromSpeculative) {
  // This case tests for what SpeculativeNumberLessThanOrEqual is lowered to.
  // Define variables.
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);

  // Test only for AnyTagged, since TaggedPointer can't be a Smi.
  // Create the graph.
  Node* load = graph()->NewNode(machine()->Load(MachineType::AnyTagged()),
                                object, index, effect, control);
  Node* constant = graph()->NewNode(common()->Int64Constant(5));
  graph()->SetEnd(
      graph()->NewNode(machine()->Int32LessThanOrEqual(), load, constant));
  // Change the nodes, and test the change.
  Reduce();
  EXPECT_EQ(LoadMachRep(load), CompressedMachRep(MachineType::AnyTagged()));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
