// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/compiler/interpreter-assembler-unittest.h"

#include "src/compiler/graph.h"
#include "src/compiler/node.h"
#include "src/unique.h"
#include "test/unittests/compiler/compiler-test-utils.h"
#include "test/unittests/compiler/node-test-utils.h"

namespace v8 {
namespace internal {
namespace compiler {

const interpreter::Bytecode kBytecodes[] = {
#define DEFINE_BYTECODE(Name, ...) interpreter::Bytecode::k##Name,
    BYTECODE_LIST(DEFINE_BYTECODE)
#undef DEFINE_BYTECODE
};


Graph*
InterpreterAssemblerTest::InterpreterAssemblerForTest::GetCompletedGraph() {
  End();
  return graph();
}


Matcher<Node*> InterpreterAssemblerTest::InterpreterAssemblerForTest::IsLoad(
    const Matcher<LoadRepresentation>& rep_matcher,
    const Matcher<Node*>& base_matcher, const Matcher<Node*>& index_matcher) {
  return ::i::compiler::IsLoad(rep_matcher, base_matcher, index_matcher,
                               graph()->start(), graph()->start());
}


Matcher<Node*> InterpreterAssemblerTest::InterpreterAssemblerForTest::IsStore(
    const Matcher<StoreRepresentation>& rep_matcher,
    const Matcher<Node*>& base_matcher, const Matcher<Node*>& index_matcher,
    const Matcher<Node*>& value_matcher) {
  return ::i::compiler::IsStore(rep_matcher, base_matcher, index_matcher,
                                value_matcher, graph()->start(),
                                graph()->start());
}


Matcher<Node*> IsIntPtrAdd(const Matcher<Node*>& lhs_matcher,
                           const Matcher<Node*>& rhs_matcher) {
  return kPointerSize == 8 ? IsInt64Add(lhs_matcher, rhs_matcher)
                           : IsInt32Add(lhs_matcher, rhs_matcher);
}


Matcher<Node*> IsIntPtrSub(const Matcher<Node*>& lhs_matcher,
                           const Matcher<Node*>& rhs_matcher) {
  return kPointerSize == 8 ? IsInt64Sub(lhs_matcher, rhs_matcher)
                           : IsInt32Sub(lhs_matcher, rhs_matcher);
}


Matcher<Node*> IsWordShl(const Matcher<Node*>& lhs_matcher,
                         const Matcher<Node*>& rhs_matcher) {
  return kPointerSize == 8 ? IsWord64Shl(lhs_matcher, rhs_matcher)
                           : IsWord32Shl(lhs_matcher, rhs_matcher);
}


TARGET_TEST_F(InterpreterAssemblerTest, Dispatch) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerForTest m(this, bytecode);
    m.Dispatch();
    Graph* graph = m.GetCompletedGraph();

    Node* end = graph->end();
    EXPECT_EQ(1, end->InputCount());
    Node* tail_call_node = end->InputAt(0);

    Matcher<Node*> next_bytecode_offset_matcher =
        IsIntPtrAdd(IsParameter(Linkage::kInterpreterBytecodeOffsetParameter),
                    IsInt32Constant(interpreter::Bytecodes::Size(bytecode)));
    Matcher<Node*> target_bytecode_matcher = m.IsLoad(
        kMachUint8, IsParameter(Linkage::kInterpreterBytecodeArrayParameter),
        next_bytecode_offset_matcher);
    Matcher<Node*> code_target_matcher = m.IsLoad(
        kMachPtr, IsParameter(Linkage::kInterpreterDispatchTableParameter),
        IsWord32Shl(target_bytecode_matcher,
                    IsInt32Constant(kPointerSizeLog2)));

    EXPECT_EQ(CallDescriptor::kInterpreterDispatch,
              m.call_descriptor()->kind());
    EXPECT_THAT(
        tail_call_node,
        IsTailCall(m.call_descriptor(), code_target_matcher,
                   next_bytecode_offset_matcher,
                   IsParameter(Linkage::kInterpreterBytecodeArrayParameter),
                   IsParameter(Linkage::kInterpreterDispatchTableParameter),
                   graph->start(), graph->start()));
  }
}


TARGET_TEST_F(InterpreterAssemblerTest, Return) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerForTest m(this, bytecode);
    m.Return();
    Graph* graph = m.GetCompletedGraph();

    Node* end = graph->end();
    EXPECT_EQ(1, end->InputCount());
    Node* tail_call_node = end->InputAt(0);

    EXPECT_EQ(CallDescriptor::kInterpreterDispatch,
              m.call_descriptor()->kind());
    Matcher<Unique<HeapObject>> exit_trampoline(
        Unique<HeapObject>::CreateImmovable(
            isolate()->builtins()->InterpreterExitTrampoline()));
    EXPECT_THAT(
        tail_call_node,
        IsTailCall(m.call_descriptor(), IsHeapConstant(exit_trampoline),
                   IsParameter(Linkage::kInterpreterBytecodeOffsetParameter),
                   IsParameter(Linkage::kInterpreterBytecodeArrayParameter),
                   IsParameter(Linkage::kInterpreterDispatchTableParameter),
                   graph->start(), graph->start()));
  }
}


TARGET_TEST_F(InterpreterAssemblerTest, BytecodeOperand) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerForTest m(this, bytecode);
    int number_of_operands = interpreter::Bytecodes::NumberOfOperands(bytecode);
    for (int i = 0; i < number_of_operands; i++) {
      Node* load_arg_node = m.BytecodeOperand(i);
      EXPECT_THAT(
          load_arg_node,
          m.IsLoad(
              kMachUint8,
              IsParameter(Linkage::kInterpreterBytecodeArrayParameter),
              IsIntPtrAdd(
                  IsParameter(Linkage::kInterpreterBytecodeOffsetParameter),
                  IsInt32Constant(1 + i))));
    }
  }
}


TARGET_TEST_F(InterpreterAssemblerTest, LoadRegisterFixed) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerForTest m(this, bytecode);
    for (int i = 0; i < m.kMaxRegisterIndex; i++) {
      Node* load_reg_node = m.LoadRegister(i);
      EXPECT_THAT(load_reg_node,
                  m.IsLoad(kMachPtr, IsLoadFramePointer(),
                           IsInt32Constant(m.kFirstRegisterOffsetFromFp -
                                           (i << kPointerSizeLog2))));
    }
  }
}


TARGET_TEST_F(InterpreterAssemblerTest, LoadRegister) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerForTest m(this, bytecode);
    Node* reg_index_node = m.Int32Constant(44);
    Node* load_reg_node = m.LoadRegister(reg_index_node);
    EXPECT_THAT(
        load_reg_node,
        m.IsLoad(kMachPtr, IsLoadFramePointer(),
                 IsIntPtrSub(IsInt32Constant(m.kFirstRegisterOffsetFromFp),
                             IsWordShl(reg_index_node,
                                       IsInt32Constant(kPointerSizeLog2)))));
  }
}


TARGET_TEST_F(InterpreterAssemblerTest, StoreRegisterFixed) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerForTest m(this, bytecode);
    Node* store_value = m.Int32Constant(0xdeadbeef);
    for (int i = 0; i < m.kMaxRegisterIndex; i++) {
      Node* store_reg_node = m.StoreRegister(store_value, i);
      EXPECT_THAT(store_reg_node,
                  m.IsStore(StoreRepresentation(kMachPtr, kNoWriteBarrier),
                            IsLoadFramePointer(),
                            IsInt32Constant(m.kFirstRegisterOffsetFromFp -
                                            (i << kPointerSizeLog2)),
                            store_value));
    }
  }
}


TARGET_TEST_F(InterpreterAssemblerTest, StoreRegister) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerForTest m(this, bytecode);
    Node* store_value = m.Int32Constant(0xdeadbeef);
    Node* reg_index_node = m.Int32Constant(44);
    Node* store_reg_node = m.StoreRegister(store_value, reg_index_node);
    EXPECT_THAT(
        store_reg_node,
        m.IsStore(StoreRepresentation(kMachPtr, kNoWriteBarrier),
                  IsLoadFramePointer(),
                  IsIntPtrSub(IsInt32Constant(m.kFirstRegisterOffsetFromFp),
                              IsWordShl(reg_index_node,
                                        IsInt32Constant(kPointerSizeLog2))),
                  store_value));
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
