// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_INTERPRETER_ASSEMBLER_H_
#define V8_INTERPRETER_INTERPRETER_ASSEMBLER_H_

#include "src/builtins/builtins.h"
#include "src/codegen/code-stub-assembler.h"
#include "src/common/globals.h"
#include "src/interpreter/bytecode-register.h"
#include "src/interpreter/bytecodes.h"
#include "src/runtime/runtime.h"
#include "src/utils/allocation.h"

namespace v8 {
namespace internal {
namespace interpreter {

class V8_EXPORT_PRIVATE InterpreterAssembler : public CodeStubAssembler {
 public:
  InterpreterAssembler(compiler::CodeAssemblerState* state, Bytecode bytecode,
                       OperandScale operand_scale);
  ~InterpreterAssembler();

  // Returns the 32-bit unsigned count immediate for bytecode operand
  // |operand_index| in the current bytecode.
  compiler::TNode<Uint32T> BytecodeOperandCount(int operand_index);
  // Returns the 32-bit unsigned flag for bytecode operand |operand_index|
  // in the current bytecode.
  compiler::TNode<Uint32T> BytecodeOperandFlag(int operand_index);
  // Returns the 32-bit zero-extended index immediate for bytecode operand
  // |operand_index| in the current bytecode.
  compiler::TNode<Uint32T> BytecodeOperandIdxInt32(int operand_index);
  // Returns the word zero-extended index immediate for bytecode operand
  // |operand_index| in the current bytecode.
  compiler::TNode<UintPtrT> BytecodeOperandIdx(int operand_index);
  // Returns the smi index immediate for bytecode operand |operand_index|
  // in the current bytecode.
  compiler::TNode<Smi> BytecodeOperandIdxSmi(int operand_index);
  // Returns the 32-bit unsigned immediate for bytecode operand |operand_index|
  // in the current bytecode.
  compiler::TNode<Uint32T> BytecodeOperandUImm(int operand_index);
  // Returns the word-size unsigned immediate for bytecode operand
  // |operand_index| in the current bytecode.
  compiler::TNode<UintPtrT> BytecodeOperandUImmWord(int operand_index);
  // Returns the unsigned smi immediate for bytecode operand |operand_index| in
  // the current bytecode.
  compiler::TNode<Smi> BytecodeOperandUImmSmi(int operand_index);
  // Returns the 32-bit signed immediate for bytecode operand |operand_index|
  // in the current bytecode.
  compiler::TNode<Int32T> BytecodeOperandImm(int operand_index);
  // Returns the word-size signed immediate for bytecode operand |operand_index|
  // in the current bytecode.
  compiler::TNode<IntPtrT> BytecodeOperandImmIntPtr(int operand_index);
  // Returns the smi immediate for bytecode operand |operand_index| in the
  // current bytecode.
  compiler::TNode<Smi> BytecodeOperandImmSmi(int operand_index);
  // Returns the 32-bit unsigned runtime id immediate for bytecode operand
  // |operand_index| in the current bytecode.
  compiler::TNode<Uint32T> BytecodeOperandRuntimeId(int operand_index);
  // Returns the word zero-extended native context index immediate for bytecode
  // operand |operand_index| in the current bytecode.
  compiler::TNode<UintPtrT> BytecodeOperandNativeContextIndex(
      int operand_index);
  // Returns the 32-bit unsigned intrinsic id immediate for bytecode operand
  // |operand_index| in the current bytecode.
  compiler::TNode<Uint32T> BytecodeOperandIntrinsicId(int operand_index);
  // Accumulator.
  compiler::TNode<Object> GetAccumulator();
  void SetAccumulator(SloppyTNode<Object> value);

  // Context.
  compiler::TNode<Context> GetContext();
  void SetContext(compiler::TNode<Context> value);

  // Context at |depth| in the context chain starting at |context|.
  compiler::TNode<Context> GetContextAtDepth(compiler::TNode<Context> context,
                                             compiler::TNode<Uint32T> depth);

  // Goto the given |target| if the context chain starting at |context| has any
  // extensions up to the given |depth|.
  void GotoIfHasContextExtensionUpToDepth(compiler::TNode<Context> context,
                                          compiler::TNode<Uint32T> depth,
                                          Label* target);

  // A RegListNodePair provides an abstraction over lists of registers.
  class RegListNodePair {
   public:
    RegListNodePair(TNode<IntPtrT> base_reg_location, TNode<Word32T> reg_count)
        : base_reg_location_(base_reg_location), reg_count_(reg_count) {}

    compiler::TNode<Word32T> reg_count() const { return reg_count_; }
    compiler::TNode<IntPtrT> base_reg_location() const {
      return base_reg_location_;
    }

   private:
    compiler::TNode<IntPtrT> base_reg_location_;
    compiler::TNode<Word32T> reg_count_;
  };

  // Backup/restore register file to/from a fixed array of the correct length.
  // There is an asymmetry between suspend/export and resume/import.
  // - Suspend copies arguments and registers to the generator.
  // - Resume copies only the registers from the generator, the arguments
  //   are copied by the ResumeGenerator trampoline.
  compiler::TNode<FixedArray> ExportParametersAndRegisterFile(
      TNode<FixedArray> array, const RegListNodePair& registers,
      TNode<Int32T> formal_parameter_count);
  compiler::TNode<FixedArray> ImportRegisterFile(
      TNode<FixedArray> array, const RegListNodePair& registers,
      TNode<Int32T> formal_parameter_count);

  // Loads from and stores to the interpreter register file.
  compiler::TNode<Object> LoadRegister(Register reg);
  compiler::TNode<IntPtrT> LoadAndUntagRegister(Register reg);
  compiler::TNode<Object> LoadRegisterAtOperandIndex(int operand_index);
  std::pair<compiler::TNode<Object>, compiler::TNode<Object>>
  LoadRegisterPairAtOperandIndex(int operand_index);
  void StoreRegister(compiler::TNode<Object> value, Register reg);
  void StoreRegisterAtOperandIndex(compiler::TNode<Object> value,
                                   int operand_index);
  void StoreRegisterPairAtOperandIndex(compiler::TNode<Object> value1,
                                       compiler::TNode<Object> value2,
                                       int operand_index);
  void StoreRegisterTripleAtOperandIndex(compiler::TNode<Object> value1,
                                         compiler::TNode<Object> value2,
                                         compiler::TNode<Object> value3,
                                         int operand_index);

  RegListNodePair GetRegisterListAtOperandIndex(int operand_index);
  TNode<Object> LoadRegisterFromRegisterList(const RegListNodePair& reg_list,
                                             int index);
  TNode<IntPtrT> RegisterLocationInRegisterList(const RegListNodePair& reg_list,
                                                int index);

  // Load constant at the index specified in operand |operand_index| from the
  // constant pool.
  compiler::TNode<Object> LoadConstantPoolEntryAtOperandIndex(
      int operand_index);
  // Load and untag constant at the index specified in operand |operand_index|
  // from the constant pool.
  TNode<IntPtrT> LoadAndUntagConstantPoolEntryAtOperandIndex(int operand_index);
  // Load constant at |index| in the constant pool.
  compiler::TNode<Object> LoadConstantPoolEntry(compiler::TNode<WordT> index);
  // Load and untag constant at |index| in the constant pool.
  TNode<IntPtrT> LoadAndUntagConstantPoolEntry(compiler::TNode<WordT> index);

  // Load the FeedbackVector for the current function. The retuned node could be
  // undefined.
  compiler::TNode<HeapObject> LoadFeedbackVector();

  // Increment the call count for a CALL_IC or construct call.
  // The call count is located at feedback_vector[slot_id + 1].
  void IncrementCallCount(compiler::Node* feedback_vector,
                          compiler::Node* slot_id);

  // Collect the callable |target| feedback for either a CALL_IC or
  // an INSTANCEOF_IC in the |feedback_vector| at |slot_id|.
  void CollectCallableFeedback(compiler::Node* target, compiler::Node* context,
                               compiler::Node* feedback_vector,
                               compiler::Node* slot_id);

  // Collect CALL_IC feedback for |target| function in the
  // |feedback_vector| at |slot_id|, and the call counts in
  // the |feedback_vector| at |slot_id+1|.
  void CollectCallFeedback(compiler::Node* target, compiler::Node* context,
                           compiler::Node* maybe_feedback_vector,
                           compiler::Node* slot_id);

  // Call JSFunction or Callable |function| with |args| arguments, possibly
  // including the receiver depending on |receiver_mode|. After the call returns
  // directly dispatches to the next bytecode.
  void CallJSAndDispatch(compiler::Node* function, compiler::Node* context,
                         const RegListNodePair& args,
                         ConvertReceiverMode receiver_mode);

  // Call JSFunction or Callable |function| with |arg_count| arguments (not
  // including receiver) passed as |args|, possibly including the receiver
  // depending on |receiver_mode|. After the call returns directly dispatches to
  // the next bytecode.
  template <class... TArgs>
  void CallJSAndDispatch(Node* function, Node* context, Node* arg_count,
                         ConvertReceiverMode receiver_mode, TArgs... args);

  // Call JSFunction or Callable |function| with |args|
  // arguments (not including receiver), and the final argument being spread.
  // After the call returns directly dispatches to the next bytecode.
  void CallJSWithSpreadAndDispatch(compiler::Node* function,
                                   compiler::Node* context,
                                   const RegListNodePair& args,
                                   compiler::Node* slot_id,
                                   compiler::Node* feedback_vector);

  // Call constructor |target| with |args| arguments (not including receiver).
  // The |new_target| is the same as the |target| for the new keyword, but
  // differs for the super keyword.
  compiler::Node* Construct(compiler::SloppyTNode<Object> target,
                            compiler::Node* context,
                            compiler::SloppyTNode<Object> new_target,
                            const RegListNodePair& args,
                            compiler::Node* slot_id,
                            compiler::Node* feedback_vector);

  // Call constructor |target| with |args| arguments (not including
  // receiver). The last argument is always a spread. The |new_target| is the
  // same as the |target| for the new keyword, but differs for the super
  // keyword.
  compiler::Node* ConstructWithSpread(compiler::Node* target,
                                      compiler::Node* context,
                                      compiler::Node* new_target,
                                      const RegListNodePair& args,
                                      compiler::Node* slot_id,
                                      compiler::Node* feedback_vector);

  // Call runtime function with |args| arguments which will return |return_size|
  // number of values.
  compiler::Node* CallRuntimeN(compiler::Node* function_id,
                               compiler::Node* context,
                               const RegListNodePair& args,
                               int return_size = 1);

  // Jump forward relative to the current bytecode by the |jump_offset|.
  void Jump(compiler::TNode<IntPtrT> jump_offset);

  // Jump backward relative to the current bytecode by the |jump_offset|.
  void JumpBackward(compiler::TNode<IntPtrT> jump_offset);

  // Jump forward relative to the current bytecode by |jump_offset| if the
  // word values |lhs| and |rhs| are equal.
  void JumpIfTaggedEqual(compiler::TNode<Object> lhs,
                         compiler::TNode<Object> rhs,
                         compiler::TNode<IntPtrT> jump_offset);

  // Jump forward relative to the current bytecode by |jump_offset| if the
  // word values |lhs| and |rhs| are not equal.
  void JumpIfTaggedNotEqual(compiler::TNode<Object> lhs,
                            compiler::TNode<Object> rhs,
                            compiler::TNode<IntPtrT> jump_offset);

  // Updates the profiler interrupt budget for a return.
  void UpdateInterruptBudgetOnReturn();

  // Returns the OSR nesting level from the bytecode header.
  compiler::TNode<Int8T> LoadOsrNestingLevel();

  // Dispatch to the bytecode.
  void Dispatch();

  // Dispatch bytecode as wide operand variant.
  void DispatchWide(OperandScale operand_scale);

  // Dispatch to |target_bytecode| at |new_bytecode_offset|.
  // |target_bytecode| should be equivalent to loading from the offset.
  void DispatchToBytecode(compiler::TNode<WordT> target_bytecode,
                          compiler::TNode<IntPtrT> new_bytecode_offset);

  // Abort with the given abort reason.
  void Abort(AbortReason abort_reason);
  void AbortIfWordNotEqual(compiler::TNode<WordT> lhs,
                           compiler::TNode<WordT> rhs,
                           AbortReason abort_reason);
  // Abort if |register_count| is invalid for given register file array.
  void AbortIfRegisterCountInvalid(compiler::Node* parameters_and_registers,
                                   compiler::Node* formal_parameter_count,
                                   compiler::Node* register_count);

  // Dispatch to frame dropper trampoline if necessary.
  void MaybeDropFrames(compiler::Node* context);

  // Returns the offset from the BytecodeArrayPointer of the current bytecode.
  TNode<IntPtrT> BytecodeOffset();

 protected:
  Bytecode bytecode() const { return bytecode_; }
  static bool TargetSupportsUnalignedAccess();

  void ToNumberOrNumeric(Object::Conversion mode);

 private:
  // Returns a pointer to the current function's BytecodeArray object.
  TNode<BytecodeArray> BytecodeArrayTaggedPointer();

  // Returns a pointer to first entry in the interpreter dispatch table.
  TNode<ExternalReference> DispatchTablePointer();

  // Returns the accumulator value without checking whether bytecode
  // uses it. This is intended to be used only in dispatch and in
  // tracing as these need to bypass accumulator use validity checks.
  TNode<Object> GetAccumulatorUnchecked();

  // Returns the frame pointer for the interpreted frame of the function being
  // interpreted.
  TNode<RawPtrT> GetInterpretedFramePointer();

  // Operations on registers.
  compiler::TNode<IntPtrT> RegisterLocation(Register reg);
  compiler::TNode<IntPtrT> RegisterLocation(compiler::TNode<IntPtrT> reg_index);
  compiler::TNode<IntPtrT> NextRegister(compiler::TNode<IntPtrT> reg_index);
  compiler::TNode<Object> LoadRegister(compiler::TNode<IntPtrT> reg_index);
  void StoreRegister(compiler::TNode<Object> value,
                     compiler::TNode<IntPtrT> reg_index);

  // Saves and restores interpreter bytecode offset to the interpreter stack
  // frame when performing a call.
  void CallPrologue();
  void CallEpilogue();

  // Increment the dispatch counter for the (current, next) bytecode pair.
  void TraceBytecodeDispatch(compiler::Node* target_index);

  // Traces the current bytecode by calling |function_id|.
  void TraceBytecode(Runtime::FunctionId function_id);

  // Updates the bytecode array's interrupt budget by a 32-bit unsigned |weight|
  // and calls Runtime::kInterrupt if counter reaches zero. If |backward|, then
  // the interrupt budget is decremented, otherwise it is incremented.
  void UpdateInterruptBudget(compiler::Node* weight, bool backward);

  // Returns the offset of register |index| relative to RegisterFilePointer().
  compiler::TNode<IntPtrT> RegisterFrameOffset(compiler::TNode<IntPtrT> index);

  // Returns the offset of an operand relative to the current bytecode offset.
  compiler::Node* OperandOffset(int operand_index);

  // Returns a value built from an sequence of bytes in the bytecode
  // array starting at |relative_offset| from the current bytecode.
  // The |result_type| determines the size and signedness.  of the
  // value read. This method should only be used on architectures that
  // do not support unaligned memory accesses.
  compiler::TNode<Word32T> BytecodeOperandReadUnaligned(
      int relative_offset, MachineType result_type,
      LoadSensitivity needs_poisoning = LoadSensitivity::kCritical);

  // Returns zero- or sign-extended to word32 value of the operand.
  compiler::TNode<Uint8T> BytecodeOperandUnsignedByte(
      int operand_index,
      LoadSensitivity needs_poisoning = LoadSensitivity::kCritical);
  compiler::TNode<Int8T> BytecodeOperandSignedByte(
      int operand_index,
      LoadSensitivity needs_poisoning = LoadSensitivity::kCritical);
  compiler::TNode<Uint16T> BytecodeOperandUnsignedShort(
      int operand_index,
      LoadSensitivity needs_poisoning = LoadSensitivity::kCritical);
  compiler::TNode<Int16T> BytecodeOperandSignedShort(
      int operand_index,
      LoadSensitivity needs_poisoning = LoadSensitivity::kCritical);
  compiler::TNode<Uint32T> BytecodeOperandUnsignedQuad(
      int operand_index,
      LoadSensitivity needs_poisoning = LoadSensitivity::kCritical);
  compiler::TNode<Int32T> BytecodeOperandSignedQuad(
      int operand_index,
      LoadSensitivity needs_poisoning = LoadSensitivity::kCritical);

  // Returns zero- or sign-extended to word32 value of the operand of
  // given size.
  compiler::TNode<Int32T> BytecodeSignedOperand(
      int operand_index, OperandSize operand_size,
      LoadSensitivity needs_poisoning = LoadSensitivity::kCritical);
  compiler::TNode<Uint32T> BytecodeUnsignedOperand(
      int operand_index, OperandSize operand_size,
      LoadSensitivity needs_poisoning = LoadSensitivity::kCritical);

  // Returns the word-size sign-extended register index for bytecode operand
  // |operand_index| in the current bytecode. Value is not poisoned on
  // speculation since the value loaded from the register is poisoned instead.
  compiler::TNode<IntPtrT> BytecodeOperandReg(
      int operand_index,
      LoadSensitivity needs_poisoning = LoadSensitivity::kCritical);

  // Returns the word zero-extended index immediate for bytecode operand
  // |operand_index| in the current bytecode for use when loading a .
  compiler::TNode<UintPtrT> BytecodeOperandConstantPoolIdx(
      int operand_index,
      LoadSensitivity needs_poisoning = LoadSensitivity::kCritical);

  // Jump relative to the current bytecode by the |jump_offset|. If |backward|,
  // then jump backward (subtract the offset), otherwise jump forward (add the
  // offset). Helper function for Jump and JumpBackward.
  void Jump(compiler::TNode<IntPtrT> jump_offset, bool backward);

  // Jump forward relative to the current bytecode by |jump_offset| if the
  // |condition| is true. Helper function for JumpIfTaggedEqual and
  // JumpIfTaggedNotEqual.
  void JumpConditional(compiler::TNode<BoolT> condition,
                       compiler::TNode<IntPtrT> jump_offset);

  // Save the bytecode offset to the interpreter frame.
  void SaveBytecodeOffset();
  // Reload the bytecode offset from the interpreter frame.
  TNode<IntPtrT> ReloadBytecodeOffset();

  // Updates and returns BytecodeOffset() advanced by the current bytecode's
  // size. Traces the exit of the current bytecode.
  TNode<IntPtrT> Advance();

  // Updates and returns BytecodeOffset() advanced by delta bytecodes.
  // Traces the exit of the current bytecode.
  TNode<IntPtrT> Advance(int delta);
  TNode<IntPtrT> Advance(TNode<IntPtrT> delta, bool backward = false);

  // Load the bytecode at |bytecode_offset|.
  compiler::TNode<WordT> LoadBytecode(compiler::TNode<IntPtrT> bytecode_offset);

  // Look ahead for Star and inline it in a branch. Returns a new target
  // bytecode node for dispatch.
  compiler::TNode<WordT> StarDispatchLookahead(
      compiler::TNode<WordT> target_bytecode);

  // Build code for Star at the current BytecodeOffset() and Advance() to the
  // next dispatch offset.
  void InlineStar();

  // Dispatch to the bytecode handler with code entry point |handler_entry|.
  void DispatchToBytecodeHandlerEntry(compiler::TNode<RawPtrT> handler_entry,
                                      compiler::TNode<IntPtrT> bytecode_offset);

  int CurrentBytecodeSize() const;

  OperandScale operand_scale() const { return operand_scale_; }

  Bytecode bytecode_;
  OperandScale operand_scale_;
  CodeStubAssembler::TVariable<RawPtrT> interpreted_frame_pointer_;
  CodeStubAssembler::TVariable<BytecodeArray> bytecode_array_;
  CodeStubAssembler::TVariable<IntPtrT> bytecode_offset_;
  CodeStubAssembler::TVariable<ExternalReference> dispatch_table_;
  CodeStubAssembler::TVariable<Object> accumulator_;
  AccumulatorUse accumulator_use_;
  bool made_call_;
  bool reloaded_frame_ptr_;
  bool bytecode_array_valid_;

  DISALLOW_COPY_AND_ASSIGN(InterpreterAssembler);
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_INTERPRETER_ASSEMBLER_H_
