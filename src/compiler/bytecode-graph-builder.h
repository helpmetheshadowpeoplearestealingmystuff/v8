// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_BYTECODE_GRAPH_BUILDER_H_
#define V8_COMPILER_BYTECODE_GRAPH_BUILDER_H_

#include "src/compiler.h"
#include "src/compiler/bytecode-branch-analysis.h"
#include "src/compiler/js-graph.h"
#include "src/interpreter/bytecode-array-iterator.h"
#include "src/interpreter/bytecodes.h"

namespace v8 {
namespace internal {
namespace compiler {

// The BytecodeGraphBuilder produces a high-level IR graph based on
// interpreter bytecodes.
class BytecodeGraphBuilder {
 public:
  BytecodeGraphBuilder(Zone* local_zone, CompilationInfo* info,
                       JSGraph* jsgraph);

  // Creates a graph by visiting bytecodes.
  bool CreateGraph(bool stack_check = true);

 private:
  class Environment;
  class FrameStateBeforeAndAfter;

  void CreateGraphBody(bool stack_check);
  void VisitBytecodes();

  // Get or create the node that represents the outer function closure.
  Node* GetFunctionClosure();

  // Get or create the node that represents the outer function context.
  Node* GetFunctionContext();

  // Get or create the node that represents the incoming new target value.
  Node* GetNewTarget();

  // Builder for accessing a (potentially immutable) object field.
  Node* BuildLoadObjectField(Node* object, int offset);
  Node* BuildLoadImmutableObjectField(Node* object, int offset);

  // Builder for accessing type feedback vector.
  Node* BuildLoadFeedbackVector();

  // Builder for loading the a native context field.
  Node* BuildLoadNativeContextField(int index);

  // Helper function for creating a pair containing type feedback vector and
  // a feedback slot.
  VectorSlotPair CreateVectorSlotPair(int slot_id);

  void set_environment(Environment* env) { environment_ = env; }
  const Environment* environment() const { return environment_; }
  Environment* environment() { return environment_; }

  // Node creation helpers
  Node* NewNode(const Operator* op, bool incomplete = false) {
    return MakeNode(op, 0, static_cast<Node**>(nullptr), incomplete);
  }

  Node* NewNode(const Operator* op, Node* n1) {
    Node* buffer[] = {n1};
    return MakeNode(op, arraysize(buffer), buffer, false);
  }

  Node* NewNode(const Operator* op, Node* n1, Node* n2) {
    Node* buffer[] = {n1, n2};
    return MakeNode(op, arraysize(buffer), buffer, false);
  }

  Node* NewNode(const Operator* op, Node* n1, Node* n2, Node* n3) {
    Node* buffer[] = {n1, n2, n3};
    return MakeNode(op, arraysize(buffer), buffer, false);
  }

  Node* NewNode(const Operator* op, Node* n1, Node* n2, Node* n3, Node* n4) {
    Node* buffer[] = {n1, n2, n3, n4};
    return MakeNode(op, arraysize(buffer), buffer, false);
  }

  // Helpers to create new control nodes.
  Node* NewIfTrue() { return NewNode(common()->IfTrue()); }
  Node* NewIfFalse() { return NewNode(common()->IfFalse()); }
  Node* NewMerge() { return NewNode(common()->Merge(1), true); }
  Node* NewLoop() { return NewNode(common()->Loop(1), true); }
  Node* NewBranch(Node* condition, BranchHint hint = BranchHint::kNone) {
    return NewNode(common()->Branch(hint), condition);
  }

  // Creates a new Phi node having {count} input values.
  Node* NewPhi(int count, Node* input, Node* control);
  Node* NewEffectPhi(int count, Node* input, Node* control);

  // Helpers for merging control, effect or value dependencies.
  Node* MergeControl(Node* control, Node* other);
  Node* MergeEffect(Node* effect, Node* other_effect, Node* control);
  Node* MergeValue(Node* value, Node* other_value, Node* control);

  // The main node creation chokepoint. Adds context, frame state, effect,
  // and control dependencies depending on the operator.
  Node* MakeNode(const Operator* op, int value_input_count, Node** value_inputs,
                 bool incomplete);

  // Helper to indicate a node exits the function body.
  void UpdateControlDependencyToLeaveFunction(Node* exit);

  Node** EnsureInputBufferSize(int size);

  Node* ProcessCallArguments(const Operator* call_op, Node* callee,
                             interpreter::Register receiver, size_t arity);
  Node* ProcessCallNewArguments(const Operator* call_new_op,
                                interpreter::Register callee,
                                interpreter::Register first_arg, size_t arity);
  Node* ProcessCallRuntimeArguments(const Operator* call_runtime_op,
                                    interpreter::Register first_arg,
                                    size_t arity);

  void BuildCreateLiteral(const Operator* op);
  void BuildCreateRegExpLiteral();
  void BuildCreateArrayLiteral();
  void BuildCreateObjectLiteral();
  void BuildCreateArguments(CreateArgumentsParameters::Type type);
  void BuildLoadGlobal(TypeofMode typeof_mode);
  void BuildStoreGlobal();
  void BuildNamedLoad();
  void BuildKeyedLoad();
  void BuildNamedStore();
  void BuildKeyedStore();
  void BuildLdaLookupSlot(TypeofMode typeof_mode);
  void BuildStaLookupSlot(LanguageMode language_mode);
  void BuildCall();
  void BuildCallJSRuntime();
  void BuildCallRuntime();
  void BuildCallRuntimeForPair();
  void BuildCallConstruct();
  void BuildBinaryOp(const Operator* op);
  void BuildCompareOp(const Operator* op);
  void BuildDelete();
  void BuildCastOperator(const Operator* js_op);
  void BuildForInPrepare();
  void BuildForInNext();

  // Control flow plumbing.
  void BuildJump(int source_offset, int target_offset);
  void BuildJump();
  void BuildConditionalJump(Node* condition);
  void BuildJumpIfEqual(Node* comperand);
  void BuildJumpIfToBooleanEqual(Node* boolean_comperand);

  // Constructing merge and loop headers.
  void MergeEnvironmentsOfBackwardBranches(int source_offset,
                                           int target_offset);
  void MergeEnvironmentsOfForwardBranches(int source_offset);
  void BuildLoopHeaderForBackwardBranches(int source_offset);

  // Simulates entry and exit of exception handlers.
  void EnterAndExitExceptionHandlers(int current_offset);

  // Attaches a frame state to |node| for the entry to the function.
  void PrepareEntryFrameState(Node* node);

  // Growth increment for the temporary buffer used to construct input lists to
  // new nodes.
  static const int kInputBufferSizeIncrement = 64;

  // An abstract representation for an exception handler that is being
  // entered and exited while the graph builder is iterating over the
  // underlying bytecode. The exception handlers within the bytecode are
  // well scoped, hence will form a stack during iteration.
  struct ExceptionHandler {
    int start_offset_;    // Start offset of the handled area in the bytecode.
    int end_offset_;      // End offset of the handled area in the bytecode.
    int handler_offset_;  // Handler entry offset within the bytecode.
  };

  // Field accessors
  Graph* graph() const { return jsgraph_->graph(); }
  CommonOperatorBuilder* common() const { return jsgraph_->common(); }
  Zone* graph_zone() const { return graph()->zone(); }
  CompilationInfo* info() const { return info_; }
  JSGraph* jsgraph() const { return jsgraph_; }
  JSOperatorBuilder* javascript() const { return jsgraph_->javascript(); }
  Zone* local_zone() const { return local_zone_; }
  const Handle<BytecodeArray>& bytecode_array() const {
    return bytecode_array_;
  }
  const Handle<HandlerTable>& exception_handler_table() const {
    return exception_handler_table_;
  }
  const FrameStateFunctionInfo* frame_state_function_info() const {
    return frame_state_function_info_;
  }

  LanguageMode language_mode() const {
    // TODO(mythria): Don't rely on parse information to get language mode.
    return info()->language_mode();
  }

  const interpreter::BytecodeArrayIterator& bytecode_iterator() const {
    return *bytecode_iterator_;
  }

  void set_bytecode_iterator(
      const interpreter::BytecodeArrayIterator* bytecode_iterator) {
    bytecode_iterator_ = bytecode_iterator;
  }

  const BytecodeBranchAnalysis* branch_analysis() const {
    return branch_analysis_;
  }

  void set_branch_analysis(BytecodeBranchAnalysis* branch_analysis) {
    branch_analysis_ = branch_analysis;
  }

#define DECLARE_VISIT_BYTECODE(name, ...) void Visit##name();
  BYTECODE_LIST(DECLARE_VISIT_BYTECODE)
#undef DECLARE_VISIT_BYTECODE

  Zone* local_zone_;
  CompilationInfo* info_;
  JSGraph* jsgraph_;
  Handle<BytecodeArray> bytecode_array_;
  Handle<HandlerTable> exception_handler_table_;
  const FrameStateFunctionInfo* frame_state_function_info_;
  const interpreter::BytecodeArrayIterator* bytecode_iterator_;
  BytecodeBranchAnalysis* branch_analysis_;  // TODO(mstarzinger): Make const.
  Environment* environment_;

  // Merge environments are snapshots of the environment at a particular
  // bytecode offset to be merged into a later environment.
  ZoneMap<int, Environment*> merge_environments_;

  // Loop header environments are environments created for bytecodes
  // where it is known there are back branches, ie a loop header.
  ZoneMap<int, Environment*> loop_header_environments_;

  // Exception handlers currently entered by the iteration.
  ZoneStack<ExceptionHandler> exception_handlers_;
  int current_exception_handler_;

  // Temporary storage for building node input lists.
  int input_buffer_size_;
  Node** input_buffer_;

  // Nodes representing values in the activation record.
  SetOncePointer<Node> function_context_;
  SetOncePointer<Node> function_closure_;
  SetOncePointer<Node> new_target_;

  // Optimization to cache loaded feedback vector.
  SetOncePointer<Node> feedback_vector_;

  // Control nodes that exit the function body.
  ZoneVector<Node*> exit_controls_;

  DISALLOW_COPY_AND_ASSIGN(BytecodeGraphBuilder);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_BYTECODE_GRAPH_BUILDER_H_
