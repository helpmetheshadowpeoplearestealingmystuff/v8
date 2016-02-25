// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/int64-lowering.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/linkage.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node.h"

#include "src/compiler/node-properties.h"

#include "src/signature.h"

#include "src/wasm/wasm-module.h"

#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"
#include "testing/gmock-support.h"

using testing::AllOf;
using testing::Capture;
using testing::CaptureEq;

namespace v8 {
namespace internal {
namespace compiler {

class Int64LoweringTest : public GraphTest {
 public:
  Int64LoweringTest() : GraphTest(), machine_(zone()) {
    value_[0] = 0x1234567890abcdef;
    value_[1] = 0x1edcba098765432f;
    value_[2] = 0x1133557799886644;
  }

  MachineOperatorBuilder* machine() { return &machine_; }

  void LowerGraph(Node* node, Signature<MachineRepresentation>* signature) {
    Node* ret = graph()->NewNode(common()->Return(), node, graph()->start(),
                                 graph()->start());
    NodeProperties::MergeControlToEnd(graph(), common(), ret);

    Int64Lowering lowering(graph(), machine(), common(), zone(), signature);
    lowering.LowerGraph();
  }

  void LowerGraph(Node* node, MachineRepresentation return_type,
                  MachineRepresentation rep = MachineRepresentation::kWord32,
                  int num_params = 0) {
    Signature<MachineRepresentation>::Builder sig_builder(zone(), 1,
                                                          num_params);
    sig_builder.AddReturn(return_type);
    for (int i = 0; i < num_params; i++) {
      sig_builder.AddParam(rep);
    }
    LowerGraph(node, sig_builder.Build());
  }

  void CompareCallDescriptors(const CallDescriptor* lhs,
                              const CallDescriptor* rhs) {
    EXPECT_THAT(lhs->CalleeSavedFPRegisters(), rhs->CalleeSavedFPRegisters());
    EXPECT_THAT(lhs->CalleeSavedRegisters(), rhs->CalleeSavedRegisters());
    EXPECT_THAT(lhs->FrameStateCount(), rhs->FrameStateCount());
    EXPECT_THAT(lhs->InputCount(), rhs->InputCount());
    for (size_t i = 0; i < lhs->InputCount(); i++) {
      EXPECT_THAT(lhs->GetInputLocation(i), rhs->GetInputLocation(i));
      EXPECT_THAT(lhs->GetInputType(i), rhs->GetInputType(i));
    }
    EXPECT_THAT(lhs->ReturnCount(), rhs->ReturnCount());
    for (size_t i = 0; i < lhs->ReturnCount(); i++) {
      EXPECT_THAT(lhs->GetReturnLocation(i), rhs->GetReturnLocation(i));
      EXPECT_THAT(lhs->GetReturnType(i), rhs->GetReturnType(i));
    }
    EXPECT_THAT(lhs->flags(), rhs->flags());
    EXPECT_THAT(lhs->kind(), rhs->kind());
  }

  int64_t value(int i) { return value_[i]; }

  int32_t low_word_value(int i) {
    return static_cast<int32_t>(value_[i] & 0xffffffff);
  }

  int32_t high_word_value(int i) {
    return static_cast<int32_t>(value_[i] >> 32);
  }

  void TestComparison(
      const Operator* op,
      Matcher<Node*> (*high_word_matcher)(const Matcher<Node*>& lhs_matcher,
                                          const Matcher<Node*>& rhs_matcher),
      Matcher<Node*> (*low_word_matcher)(const Matcher<Node*>& lhs_matcher,
                                         const Matcher<Node*>& rhs_matcher)) {
    LowerGraph(
        graph()->NewNode(op, Int64Constant(value(0)), Int64Constant(value(1))),
        MachineRepresentation::kWord32);
    EXPECT_THAT(
        graph()->end()->InputAt(1),
        IsReturn(IsWord32Or(
                     high_word_matcher(IsInt32Constant(high_word_value(0)),
                                       IsInt32Constant(high_word_value(1))),
                     IsWord32And(
                         IsWord32Equal(IsInt32Constant(high_word_value(0)),
                                       IsInt32Constant(high_word_value(1))),
                         low_word_matcher(IsInt32Constant(low_word_value(0)),
                                          IsInt32Constant(low_word_value(1))))),
                 start(), start()));
  }

 private:
  MachineOperatorBuilder machine_;
  int64_t value_[3];
};

TEST_F(Int64LoweringTest, Int64Constant) {
  if (4 != kPointerSize) return;

  LowerGraph(Int64Constant(value(0)), MachineRepresentation::kWord64);
  EXPECT_THAT(graph()->end()->InputAt(1),
              IsReturn2(IsInt32Constant(low_word_value(0)),
                        IsInt32Constant(high_word_value(0)), start(), start()));
}

TEST_F(Int64LoweringTest, Int64Load) {
  if (4 != kPointerSize) return;

  int32_t base = 0x1234;
  int32_t index = 0x5678;

  LowerGraph(graph()->NewNode(machine()->Load(MachineType::Int64()),
                              Int32Constant(base), Int32Constant(index),
                              start(), start()),
             MachineRepresentation::kWord64);

  Capture<Node*> high_word_load;
  Matcher<Node*> high_word_load_matcher =
      IsLoad(MachineType::Int32(), IsInt32Constant(base),
             IsInt32Add(IsInt32Constant(index), IsInt32Constant(0x4)), start(),
             start());

  EXPECT_THAT(
      graph()->end()->InputAt(1),
      IsReturn2(IsLoad(MachineType::Int32(), IsInt32Constant(base),
                       IsInt32Constant(index), AllOf(CaptureEq(&high_word_load),
                                                     high_word_load_matcher),
                       start()),
                AllOf(CaptureEq(&high_word_load), high_word_load_matcher),
                start(), start()));
}

TEST_F(Int64LoweringTest, Int64Store) {
  if (4 != kPointerSize) return;

  // We have to build the TF graph explicitly here because Store does not return
  // a value.

  int32_t base = 1111;
  int32_t index = 2222;
  int32_t return_value = 0x5555;

  Signature<MachineRepresentation>::Builder sig_builder(zone(), 1, 0);
  sig_builder.AddReturn(MachineRepresentation::kWord32);

  Node* store = graph()->NewNode(
      machine()->Store(StoreRepresentation(MachineRepresentation::kWord64,
                                           WriteBarrierKind::kNoWriteBarrier)),
      Int32Constant(base), Int32Constant(index), Int64Constant(value(0)),
      start(), start());

  Node* ret = graph()->NewNode(common()->Return(), Int32Constant(return_value),
                               store, start());

  NodeProperties::MergeControlToEnd(graph(), common(), ret);

  Int64Lowering lowering(graph(), machine(), common(), zone(),
                         sig_builder.Build());
  lowering.LowerGraph();

  const StoreRepresentation rep(MachineRepresentation::kWord32,
                                kNoWriteBarrier);

  EXPECT_THAT(
      graph()->end()->InputAt(1),
      IsReturn(
          IsInt32Constant(return_value),
          IsStore(
              rep, IsInt32Constant(base), IsInt32Constant(index),
              IsInt32Constant(low_word_value(0)),
              IsStore(rep, IsInt32Constant(base),
                      IsInt32Add(IsInt32Constant(index), IsInt32Constant(4)),
                      IsInt32Constant(high_word_value(0)), start(), start()),
              start()),
          start()));
}

TEST_F(Int64LoweringTest, Int64And) {
  if (4 != kPointerSize) return;

  LowerGraph(graph()->NewNode(machine()->Word64And(), Int64Constant(value(0)),
                              Int64Constant(value(1))),
             MachineRepresentation::kWord64);
  EXPECT_THAT(graph()->end()->InputAt(1),
              IsReturn2(IsWord32And(IsInt32Constant(low_word_value(0)),
                                    IsInt32Constant(low_word_value(1))),
                        IsWord32And(IsInt32Constant(high_word_value(0)),
                                    IsInt32Constant(high_word_value(1))),
                        start(), start()));
}

TEST_F(Int64LoweringTest, TruncateInt64ToInt32) {
  if (4 != kPointerSize) return;

  LowerGraph(graph()->NewNode(machine()->TruncateInt64ToInt32(),
                              Int64Constant(value(0))),
             MachineRepresentation::kWord32);
  EXPECT_THAT(graph()->end()->InputAt(1),
              IsReturn(IsInt32Constant(low_word_value(0)), start(), start()));
}

TEST_F(Int64LoweringTest, Parameter) {
  if (4 != kPointerSize) return;

  LowerGraph(Parameter(0), MachineRepresentation::kWord64,
             MachineRepresentation::kWord64, 1);

  EXPECT_THAT(graph()->end()->InputAt(1),
              IsReturn2(IsParameter(0), IsParameter(1), start(), start()));
}

TEST_F(Int64LoweringTest, Parameter2) {
  if (4 != kPointerSize) return;

  Signature<MachineRepresentation>::Builder sig_builder(zone(), 1, 5);
  sig_builder.AddReturn(MachineRepresentation::kWord32);

  sig_builder.AddParam(MachineRepresentation::kWord32);
  sig_builder.AddParam(MachineRepresentation::kWord64);
  sig_builder.AddParam(MachineRepresentation::kFloat64);
  sig_builder.AddParam(MachineRepresentation::kWord64);
  sig_builder.AddParam(MachineRepresentation::kWord32);

  int start_parameter = start()->op()->ValueOutputCount();
  LowerGraph(Parameter(4), sig_builder.Build());

  EXPECT_THAT(graph()->end()->InputAt(1),
              IsReturn(IsParameter(6), start(), start()));
  // The parameter of the start node should increase by 2, because we lowered
  // two parameter nodes.
  EXPECT_THAT(start()->op()->ValueOutputCount(), start_parameter + 2);
}

TEST_F(Int64LoweringTest, CallI64Return) {
  if (4 != kPointerSize) return;

  int32_t function = 0x9999;

  Signature<MachineRepresentation>::Builder sig_builder(zone(), 1, 0);
  sig_builder.AddReturn(MachineRepresentation::kWord64);

  compiler::CallDescriptor* desc =
      wasm::ModuleEnv::GetWasmCallDescriptor(zone(), sig_builder.Build());

  LowerGraph(graph()->NewNode(common()->Call(desc), Int32Constant(function),
                              start(), start()),
             MachineRepresentation::kWord64);

  Capture<Node*> call;
  Matcher<Node*> call_matcher =
      IsCall(testing::_, IsInt32Constant(function), start(), start());

  EXPECT_THAT(graph()->end()->InputAt(1),
              IsReturn2(IsProjection(0, AllOf(CaptureEq(&call), call_matcher)),
                        IsProjection(1, AllOf(CaptureEq(&call), call_matcher)),
                        start(), start()));

  CompareCallDescriptors(
      OpParameter<const CallDescriptor*>(
          graph()->end()->InputAt(1)->InputAt(0)->InputAt(0)),
      wasm::ModuleEnv::GetI32WasmCallDescriptor(zone(), desc));
}

TEST_F(Int64LoweringTest, CallI64Parameter) {
  if (4 != kPointerSize) return;

  int32_t function = 0x9999;

  Signature<MachineRepresentation>::Builder sig_builder(zone(), 1, 3);
  sig_builder.AddReturn(MachineRepresentation::kWord32);
  sig_builder.AddParam(MachineRepresentation::kWord64);
  sig_builder.AddParam(MachineRepresentation::kWord32);
  sig_builder.AddParam(MachineRepresentation::kWord64);

  compiler::CallDescriptor* desc =
      wasm::ModuleEnv::GetWasmCallDescriptor(zone(), sig_builder.Build());

  LowerGraph(graph()->NewNode(common()->Call(desc), Int32Constant(function),
                              Int64Constant(value(0)),
                              Int32Constant(low_word_value(1)),
                              Int64Constant(value(2)), start(), start()),
             MachineRepresentation::kWord32);

  EXPECT_THAT(
      graph()->end()->InputAt(1),
      IsReturn(IsCall(testing::_, IsInt32Constant(function),
                      IsInt32Constant(low_word_value(0)),
                      IsInt32Constant(high_word_value(0)),
                      IsInt32Constant(low_word_value(1)),
                      IsInt32Constant(low_word_value(2)),
                      IsInt32Constant(high_word_value(2)), start(), start()),
               start(), start()));

  CompareCallDescriptors(
      OpParameter<const CallDescriptor*>(
          graph()->end()->InputAt(1)->InputAt(0)),
      wasm::ModuleEnv::GetI32WasmCallDescriptor(zone(), desc));
}

// todo(ahaas): I added a list of missing instructions here to make merging
// easier when I do them one by one.
// kExprI64Add:
// kExprI64Sub:
// kExprI64Mul:
// kExprI64DivS:
// kExprI64DivU:
// kExprI64RemS:
// kExprI64RemU:
// kExprI64And:
// kExprI64Ior:
TEST_F(Int64LoweringTest, Int64Ior) {
  if (4 != kPointerSize) return;

  LowerGraph(graph()->NewNode(machine()->Word64Or(), Int64Constant(value(0)),
                              Int64Constant(value(1))),
             MachineRepresentation::kWord64);
  EXPECT_THAT(graph()->end()->InputAt(1),
              IsReturn2(IsWord32Or(IsInt32Constant(low_word_value(0)),
                                   IsInt32Constant(low_word_value(1))),
                        IsWord32Or(IsInt32Constant(high_word_value(0)),
                                   IsInt32Constant(high_word_value(1))),
                        start(), start()));
}

// kExprI64Xor:
TEST_F(Int64LoweringTest, Int64Xor) {
  if (4 != kPointerSize) return;

  LowerGraph(graph()->NewNode(machine()->Word64Xor(), Int64Constant(value(0)),
                              Int64Constant(value(1))),
             MachineRepresentation::kWord64);
  EXPECT_THAT(graph()->end()->InputAt(1),
              IsReturn2(IsWord32Xor(IsInt32Constant(low_word_value(0)),
                                    IsInt32Constant(low_word_value(1))),
                        IsWord32Xor(IsInt32Constant(high_word_value(0)),
                                    IsInt32Constant(high_word_value(1))),
                        start(), start()));
}
// kExprI64Shl:
// kExprI64ShrU:
// kExprI64ShrS:
// kExprI64Eq:
TEST_F(Int64LoweringTest, Int64Eq) {
  if (4 != kPointerSize) return;

  LowerGraph(graph()->NewNode(machine()->Word64Equal(), Int64Constant(value(0)),
                              Int64Constant(value(1))),
             MachineRepresentation::kWord32);
  EXPECT_THAT(
      graph()->end()->InputAt(1),
      IsReturn(IsWord32Equal(
                   IsWord32Or(IsWord32Xor(IsInt32Constant(low_word_value(0)),
                                          IsInt32Constant(low_word_value(1))),
                              IsWord32Xor(IsInt32Constant(high_word_value(0)),
                                          IsInt32Constant(high_word_value(1)))),
                   IsInt32Constant(0)),
               start(), start()));
}

// kExprI64Ne:
// kExprI64LtS:
TEST_F(Int64LoweringTest, Int64LtS) {
  if (4 != kPointerSize) return;
  TestComparison(machine()->Int64LessThan(), IsInt32LessThan, IsUint32LessThan);
}
// kExprI64LeS:
TEST_F(Int64LoweringTest, Int64LeS) {
  if (4 != kPointerSize) return;
  TestComparison(machine()->Int64LessThanOrEqual(), IsInt32LessThan,
                 IsUint32LessThanOrEqual);
}
// kExprI64LtU:
TEST_F(Int64LoweringTest, Int64LtU) {
  if (4 != kPointerSize) return;
  TestComparison(machine()->Uint64LessThan(), IsUint32LessThan,
                 IsUint32LessThan);
}
// kExprI64LeU:
TEST_F(Int64LoweringTest, Int64LeU) {
  if (4 != kPointerSize) return;
  TestComparison(machine()->Uint64LessThanOrEqual(), IsUint32LessThan,
                 IsUint32LessThanOrEqual);
}

// kExprI32ConvertI64:
// kExprI64SConvertI32:
// kExprI64UConvertI32:

// kExprF64ReinterpretI64:
// kExprI64ReinterpretF64:

// kExprI64Clz:
// kExprI64Ctz:
// kExprI64Popcnt:

// kExprF32SConvertI64:
// kExprF32UConvertI64:
// kExprF64SConvertI64:
// kExprF64UConvertI64:
// kExprI64SConvertF32:
// kExprI64SConvertF64:
// kExprI64UConvertF32:
// kExprI64UConvertF64:

}  // namespace compiler
}  // namespace internal
}  // namespace v8
