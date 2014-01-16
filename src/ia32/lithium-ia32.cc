// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#if V8_TARGET_ARCH_IA32

#include "lithium-allocator-inl.h"
#include "ia32/lithium-ia32.h"
#include "ia32/lithium-codegen-ia32.h"
#include "hydrogen-osr.h"

namespace v8 {
namespace internal {

#define DEFINE_COMPILE(type)                            \
  void L##type::CompileToNative(LCodeGen* generator) {  \
    generator->Do##type(this);                          \
  }
LITHIUM_CONCRETE_INSTRUCTION_LIST(DEFINE_COMPILE)
#undef DEFINE_COMPILE


#ifdef DEBUG
void LInstruction::VerifyCall() {
  // Call instructions can use only fixed registers as temporaries and
  // outputs because all registers are blocked by the calling convention.
  // Inputs operands must use a fixed register or use-at-start policy or
  // a non-register policy.
  ASSERT(Output() == NULL ||
         LUnallocated::cast(Output())->HasFixedPolicy() ||
         !LUnallocated::cast(Output())->HasRegisterPolicy());
  for (UseIterator it(this); !it.Done(); it.Advance()) {
    LUnallocated* operand = LUnallocated::cast(it.Current());
    ASSERT(operand->HasFixedPolicy() ||
           operand->IsUsedAtStart());
  }
  for (TempIterator it(this); !it.Done(); it.Advance()) {
    LUnallocated* operand = LUnallocated::cast(it.Current());
    ASSERT(operand->HasFixedPolicy() ||!operand->HasRegisterPolicy());
  }
}
#endif


bool LInstruction::HasDoubleRegisterResult() {
  return HasResult() && result()->IsDoubleRegister();
}


bool LInstruction::HasDoubleRegisterInput() {
  for (int i = 0; i < InputCount(); i++) {
    LOperand* op = InputAt(i);
    if (op != NULL && op->IsDoubleRegister()) {
      return true;
    }
  }
  return false;
}


bool LInstruction::IsDoubleInput(X87Register reg, LCodeGen* cgen) {
  for (int i = 0; i < InputCount(); i++) {
    LOperand* op = InputAt(i);
    if (op != NULL && op->IsDoubleRegister()) {
      if (cgen->ToX87Register(op).is(reg)) return true;
    }
  }
  return false;
}


void LInstruction::PrintTo(StringStream* stream) {
  stream->Add("%s ", this->Mnemonic());

  PrintOutputOperandTo(stream);

  PrintDataTo(stream);

  if (HasEnvironment()) {
    stream->Add(" ");
    environment()->PrintTo(stream);
  }

  if (HasPointerMap()) {
    stream->Add(" ");
    pointer_map()->PrintTo(stream);
  }
}


void LInstruction::PrintDataTo(StringStream* stream) {
  stream->Add("= ");
  for (int i = 0; i < InputCount(); i++) {
    if (i > 0) stream->Add(" ");
    if (InputAt(i) == NULL) {
      stream->Add("NULL");
    } else {
      InputAt(i)->PrintTo(stream);
    }
  }
}


void LInstruction::PrintOutputOperandTo(StringStream* stream) {
  if (HasResult()) result()->PrintTo(stream);
}


void LLabel::PrintDataTo(StringStream* stream) {
  LGap::PrintDataTo(stream);
  LLabel* rep = replacement();
  if (rep != NULL) {
    stream->Add(" Dead block replaced with B%d", rep->block_id());
  }
}


bool LGap::IsRedundant() const {
  for (int i = 0; i < 4; i++) {
    if (parallel_moves_[i] != NULL && !parallel_moves_[i]->IsRedundant()) {
      return false;
    }
  }

  return true;
}


void LGap::PrintDataTo(StringStream* stream) {
  for (int i = 0; i < 4; i++) {
    stream->Add("(");
    if (parallel_moves_[i] != NULL) {
      parallel_moves_[i]->PrintDataTo(stream);
    }
    stream->Add(") ");
  }
}


const char* LArithmeticD::Mnemonic() const {
  switch (op()) {
    case Token::ADD: return "add-d";
    case Token::SUB: return "sub-d";
    case Token::MUL: return "mul-d";
    case Token::DIV: return "div-d";
    case Token::MOD: return "mod-d";
    default:
      UNREACHABLE();
      return NULL;
  }
}


const char* LArithmeticT::Mnemonic() const {
  switch (op()) {
    case Token::ADD: return "add-t";
    case Token::SUB: return "sub-t";
    case Token::MUL: return "mul-t";
    case Token::MOD: return "mod-t";
    case Token::DIV: return "div-t";
    case Token::BIT_AND: return "bit-and-t";
    case Token::BIT_OR: return "bit-or-t";
    case Token::BIT_XOR: return "bit-xor-t";
    case Token::ROR: return "ror-t";
    case Token::SHL: return "sal-t";
    case Token::SAR: return "sar-t";
    case Token::SHR: return "shr-t";
    default:
      UNREACHABLE();
      return NULL;
  }
}


bool LGoto::HasInterestingComment(LCodeGen* gen) const {
  return !gen->IsNextEmittedBlock(block_id());
}


void LGoto::PrintDataTo(StringStream* stream) {
  stream->Add("B%d", block_id());
}


void LBranch::PrintDataTo(StringStream* stream) {
  stream->Add("B%d | B%d on ", true_block_id(), false_block_id());
  value()->PrintTo(stream);
}


void LCompareNumericAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if ");
  left()->PrintTo(stream);
  stream->Add(" %s ", Token::String(op()));
  right()->PrintTo(stream);
  stream->Add(" then B%d else B%d", true_block_id(), false_block_id());
}


void LIsObjectAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if is_object(");
  value()->PrintTo(stream);
  stream->Add(") then B%d else B%d", true_block_id(), false_block_id());
}


void LIsStringAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if is_string(");
  value()->PrintTo(stream);
  stream->Add(") then B%d else B%d", true_block_id(), false_block_id());
}


void LIsSmiAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if is_smi(");
  value()->PrintTo(stream);
  stream->Add(") then B%d else B%d", true_block_id(), false_block_id());
}


void LIsUndetectableAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if is_undetectable(");
  value()->PrintTo(stream);
  stream->Add(") then B%d else B%d", true_block_id(), false_block_id());
}


void LStringCompareAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if string_compare(");
  left()->PrintTo(stream);
  right()->PrintTo(stream);
  stream->Add(") then B%d else B%d", true_block_id(), false_block_id());
}


void LHasInstanceTypeAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if has_instance_type(");
  value()->PrintTo(stream);
  stream->Add(") then B%d else B%d", true_block_id(), false_block_id());
}


void LHasCachedArrayIndexAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if has_cached_array_index(");
  value()->PrintTo(stream);
  stream->Add(") then B%d else B%d", true_block_id(), false_block_id());
}


void LClassOfTestAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if class_of_test(");
  value()->PrintTo(stream);
  stream->Add(", \"%o\") then B%d else B%d",
              *hydrogen()->class_name(),
              true_block_id(),
              false_block_id());
}


void LTypeofIsAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("if typeof ");
  value()->PrintTo(stream);
  stream->Add(" == \"%s\" then B%d else B%d",
              hydrogen()->type_literal()->ToCString().get(),
              true_block_id(), false_block_id());
}


void LStoreCodeEntry::PrintDataTo(StringStream* stream) {
  stream->Add(" = ");
  function()->PrintTo(stream);
  stream->Add(".code_entry = ");
  code_object()->PrintTo(stream);
}


void LInnerAllocatedObject::PrintDataTo(StringStream* stream) {
  stream->Add(" = ");
  base_object()->PrintTo(stream);
  stream->Add(" + ");
  offset()->PrintTo(stream);
}


void LCallJSFunction::PrintDataTo(StringStream* stream) {
  stream->Add("= ");
  function()->PrintTo(stream);
  stream->Add("#%d / ", arity());
}


void LCallWithDescriptor::PrintDataTo(StringStream* stream) {
  for (int i = 0; i < InputCount(); i++) {
    InputAt(i)->PrintTo(stream);
    stream->Add(" ");
  }
  stream->Add("#%d / ", arity());
}


void LLoadContextSlot::PrintDataTo(StringStream* stream) {
  context()->PrintTo(stream);
  stream->Add("[%d]", slot_index());
}


void LStoreContextSlot::PrintDataTo(StringStream* stream) {
  context()->PrintTo(stream);
  stream->Add("[%d] <- ", slot_index());
  value()->PrintTo(stream);
}


void LInvokeFunction::PrintDataTo(StringStream* stream) {
  stream->Add("= ");
  context()->PrintTo(stream);
  stream->Add(" ");
  function()->PrintTo(stream);
  stream->Add(" #%d / ", arity());
}


void LCallNew::PrintDataTo(StringStream* stream) {
  stream->Add("= ");
  context()->PrintTo(stream);
  stream->Add(" ");
  constructor()->PrintTo(stream);
  stream->Add(" #%d / ", arity());
}


void LCallNewArray::PrintDataTo(StringStream* stream) {
  stream->Add("= ");
  context()->PrintTo(stream);
  stream->Add(" ");
  constructor()->PrintTo(stream);
  stream->Add(" #%d / ", arity());
  ElementsKind kind = hydrogen()->elements_kind();
  stream->Add(" (%s) ", ElementsKindToString(kind));
}


void LAccessArgumentsAt::PrintDataTo(StringStream* stream) {
  arguments()->PrintTo(stream);

  stream->Add(" length ");
  length()->PrintTo(stream);

  stream->Add(" index ");
  index()->PrintTo(stream);
}


int LPlatformChunk::GetNextSpillIndex(RegisterKind kind) {
  // Skip a slot if for a double-width slot.
  if (kind == DOUBLE_REGISTERS) {
    spill_slot_count_++;
    spill_slot_count_ |= 1;
    num_double_slots_++;
  }
  return spill_slot_count_++;
}


LOperand* LPlatformChunk::GetNextSpillSlot(RegisterKind kind) {
  int index = GetNextSpillIndex(kind);
  if (kind == DOUBLE_REGISTERS) {
    return LDoubleStackSlot::Create(index, zone());
  } else {
    ASSERT(kind == GENERAL_REGISTERS);
    return LStackSlot::Create(index, zone());
  }
}


void LStoreNamedField::PrintDataTo(StringStream* stream) {
  object()->PrintTo(stream);
  hydrogen()->access().PrintTo(stream);
  stream->Add(" <- ");
  value()->PrintTo(stream);
}


void LStoreNamedGeneric::PrintDataTo(StringStream* stream) {
  object()->PrintTo(stream);
  stream->Add(".");
  stream->Add(String::cast(*name())->ToCString().get());
  stream->Add(" <- ");
  value()->PrintTo(stream);
}


void LLoadKeyed::PrintDataTo(StringStream* stream) {
  elements()->PrintTo(stream);
  stream->Add("[");
  key()->PrintTo(stream);
  if (hydrogen()->IsDehoisted()) {
    stream->Add(" + %d]", additional_index());
  } else {
    stream->Add("]");
  }
}


void LStoreKeyed::PrintDataTo(StringStream* stream) {
  elements()->PrintTo(stream);
  stream->Add("[");
  key()->PrintTo(stream);
  if (hydrogen()->IsDehoisted()) {
    stream->Add(" + %d] <-", additional_index());
  } else {
    stream->Add("] <- ");
  }

  if (value() == NULL) {
    ASSERT(hydrogen()->IsConstantHoleStore() &&
           hydrogen()->value()->representation().IsDouble());
    stream->Add("<the hole(nan)>");
  } else {
    value()->PrintTo(stream);
  }
}


void LStoreKeyedGeneric::PrintDataTo(StringStream* stream) {
  object()->PrintTo(stream);
  stream->Add("[");
  key()->PrintTo(stream);
  stream->Add("] <- ");
  value()->PrintTo(stream);
}


void LTransitionElementsKind::PrintDataTo(StringStream* stream) {
  object()->PrintTo(stream);
  stream->Add(" %p -> %p", *original_map(), *transitioned_map());
}


LPlatformChunk* LChunkBuilder::Build() {
  ASSERT(is_unused());
  chunk_ = new(zone()) LPlatformChunk(info(), graph());
  LPhase phase("L_Building chunk", chunk_);
  status_ = BUILDING;

  // Reserve the first spill slot for the state of dynamic alignment.
  if (info()->IsOptimizing()) {
    int alignment_state_index = chunk_->GetNextSpillIndex(GENERAL_REGISTERS);
    ASSERT_EQ(alignment_state_index, 0);
    USE(alignment_state_index);
  }

  // If compiling for OSR, reserve space for the unoptimized frame,
  // which will be subsumed into this frame.
  if (graph()->has_osr()) {
    for (int i = graph()->osr()->UnoptimizedFrameSlots(); i > 0; i--) {
      chunk_->GetNextSpillIndex(GENERAL_REGISTERS);
    }
  }

  const ZoneList<HBasicBlock*>* blocks = graph()->blocks();
  for (int i = 0; i < blocks->length(); i++) {
    HBasicBlock* next = NULL;
    if (i < blocks->length() - 1) next = blocks->at(i + 1);
    DoBasicBlock(blocks->at(i), next);
    if (is_aborted()) return NULL;
  }
  status_ = DONE;
  return chunk_;
}


void LChunkBuilder::Abort(BailoutReason reason) {
  info()->set_bailout_reason(reason);
  status_ = ABORTED;
}


LUnallocated* LChunkBuilder::ToUnallocated(Register reg) {
  return new(zone()) LUnallocated(LUnallocated::FIXED_REGISTER,
                                  Register::ToAllocationIndex(reg));
}


LUnallocated* LChunkBuilder::ToUnallocated(XMMRegister reg) {
  return new(zone()) LUnallocated(LUnallocated::FIXED_DOUBLE_REGISTER,
                                  XMMRegister::ToAllocationIndex(reg));
}


LOperand* LChunkBuilder::UseFixed(HValue* value, Register fixed_register) {
  return Use(value, ToUnallocated(fixed_register));
}


LOperand* LChunkBuilder::UseFixedDouble(HValue* value, XMMRegister reg) {
  return Use(value, ToUnallocated(reg));
}


LOperand* LChunkBuilder::UseRegister(HValue* value) {
  return Use(value, new(zone()) LUnallocated(LUnallocated::MUST_HAVE_REGISTER));
}


LOperand* LChunkBuilder::UseRegisterAtStart(HValue* value) {
  return Use(value,
             new(zone()) LUnallocated(LUnallocated::MUST_HAVE_REGISTER,
                                      LUnallocated::USED_AT_START));
}


LOperand* LChunkBuilder::UseTempRegister(HValue* value) {
  return Use(value, new(zone()) LUnallocated(LUnallocated::WRITABLE_REGISTER));
}


LOperand* LChunkBuilder::Use(HValue* value) {
  return Use(value, new(zone()) LUnallocated(LUnallocated::NONE));
}


LOperand* LChunkBuilder::UseAtStart(HValue* value) {
  return Use(value, new(zone()) LUnallocated(LUnallocated::NONE,
                                             LUnallocated::USED_AT_START));
}


static inline bool CanBeImmediateConstant(HValue* value) {
  return value->IsConstant() && HConstant::cast(value)->NotInNewSpace();
}


LOperand* LChunkBuilder::UseOrConstant(HValue* value) {
  return CanBeImmediateConstant(value)
      ? chunk_->DefineConstantOperand(HConstant::cast(value))
      : Use(value);
}


LOperand* LChunkBuilder::UseOrConstantAtStart(HValue* value) {
  return CanBeImmediateConstant(value)
      ? chunk_->DefineConstantOperand(HConstant::cast(value))
      : UseAtStart(value);
}


LOperand* LChunkBuilder::UseFixedOrConstant(HValue* value,
                                            Register fixed_register) {
  return CanBeImmediateConstant(value)
      ? chunk_->DefineConstantOperand(HConstant::cast(value))
      : UseFixed(value, fixed_register);
}


LOperand* LChunkBuilder::UseRegisterOrConstant(HValue* value) {
  return CanBeImmediateConstant(value)
      ? chunk_->DefineConstantOperand(HConstant::cast(value))
      : UseRegister(value);
}


LOperand* LChunkBuilder::UseRegisterOrConstantAtStart(HValue* value) {
  return CanBeImmediateConstant(value)
      ? chunk_->DefineConstantOperand(HConstant::cast(value))
      : UseRegisterAtStart(value);
}


LOperand* LChunkBuilder::UseConstant(HValue* value) {
  return chunk_->DefineConstantOperand(HConstant::cast(value));
}


LOperand* LChunkBuilder::UseAny(HValue* value) {
  return value->IsConstant()
      ? chunk_->DefineConstantOperand(HConstant::cast(value))
      :  Use(value, new(zone()) LUnallocated(LUnallocated::ANY));
}


LOperand* LChunkBuilder::Use(HValue* value, LUnallocated* operand) {
  if (value->EmitAtUses()) {
    HInstruction* instr = HInstruction::cast(value);
    VisitInstruction(instr);
  }
  operand->set_virtual_register(value->id());
  return operand;
}


LInstruction* LChunkBuilder::Define(LTemplateResultInstruction<1>* instr,
                                    LUnallocated* result) {
  result->set_virtual_register(current_instruction_->id());
  instr->set_result(result);
  return instr;
}


LInstruction* LChunkBuilder::DefineAsRegister(
    LTemplateResultInstruction<1>* instr) {
  return Define(instr,
                new(zone()) LUnallocated(LUnallocated::MUST_HAVE_REGISTER));
}


LInstruction* LChunkBuilder::DefineAsSpilled(
    LTemplateResultInstruction<1>* instr,
    int index) {
  return Define(instr,
                new(zone()) LUnallocated(LUnallocated::FIXED_SLOT, index));
}


LInstruction* LChunkBuilder::DefineSameAsFirst(
    LTemplateResultInstruction<1>* instr) {
  return Define(instr,
                new(zone()) LUnallocated(LUnallocated::SAME_AS_FIRST_INPUT));
}


LInstruction* LChunkBuilder::DefineFixed(LTemplateResultInstruction<1>* instr,
                                         Register reg) {
  return Define(instr, ToUnallocated(reg));
}


LInstruction* LChunkBuilder::DefineFixedDouble(
    LTemplateResultInstruction<1>* instr,
    XMMRegister reg) {
  return Define(instr, ToUnallocated(reg));
}


LInstruction* LChunkBuilder::AssignEnvironment(LInstruction* instr) {
  HEnvironment* hydrogen_env = current_block_->last_environment();
  int argument_index_accumulator = 0;
  ZoneList<HValue*> objects_to_materialize(0, zone());
  instr->set_environment(CreateEnvironment(hydrogen_env,
                                           &argument_index_accumulator,
                                           &objects_to_materialize));
  return instr;
}


LInstruction* LChunkBuilder::MarkAsCall(LInstruction* instr,
                                        HInstruction* hinstr,
                                        CanDeoptimize can_deoptimize) {
  info()->MarkAsNonDeferredCalling();

#ifdef DEBUG
  instr->VerifyCall();
#endif
  instr->MarkAsCall();
  instr = AssignPointerMap(instr);

  if (hinstr->HasObservableSideEffects()) {
    ASSERT(hinstr->next()->IsSimulate());
    HSimulate* sim = HSimulate::cast(hinstr->next());
    ASSERT(instruction_pending_deoptimization_environment_ == NULL);
    ASSERT(pending_deoptimization_ast_id_.IsNone());
    instruction_pending_deoptimization_environment_ = instr;
    pending_deoptimization_ast_id_ = sim->ast_id();
  }

  // If instruction does not have side-effects lazy deoptimization
  // after the call will try to deoptimize to the point before the call.
  // Thus we still need to attach environment to this call even if
  // call sequence can not deoptimize eagerly.
  bool needs_environment =
      (can_deoptimize == CAN_DEOPTIMIZE_EAGERLY) ||
      !hinstr->HasObservableSideEffects();
  if (needs_environment && !instr->HasEnvironment()) {
    instr = AssignEnvironment(instr);
  }

  return instr;
}


LInstruction* LChunkBuilder::AssignPointerMap(LInstruction* instr) {
  ASSERT(!instr->HasPointerMap());
  instr->set_pointer_map(new(zone()) LPointerMap(zone()));
  return instr;
}


LUnallocated* LChunkBuilder::TempRegister() {
  LUnallocated* operand =
      new(zone()) LUnallocated(LUnallocated::MUST_HAVE_REGISTER);
  int vreg = allocator_->GetVirtualRegister();
  if (!allocator_->AllocationOk()) {
    Abort(kOutOfVirtualRegistersWhileTryingToAllocateTempRegister);
    vreg = 0;
  }
  operand->set_virtual_register(vreg);
  return operand;
}


LOperand* LChunkBuilder::FixedTemp(Register reg) {
  LUnallocated* operand = ToUnallocated(reg);
  ASSERT(operand->HasFixedPolicy());
  return operand;
}


LOperand* LChunkBuilder::FixedTemp(XMMRegister reg) {
  LUnallocated* operand = ToUnallocated(reg);
  ASSERT(operand->HasFixedPolicy());
  return operand;
}


LInstruction* LChunkBuilder::DoBlockEntry(HBlockEntry* instr) {
  return new(zone()) LLabel(instr->block());
}


LInstruction* LChunkBuilder::DoDummyUse(HDummyUse* instr) {
  return DefineAsRegister(new(zone()) LDummyUse(UseAny(instr->value())));
}


LInstruction* LChunkBuilder::DoEnvironmentMarker(HEnvironmentMarker* instr) {
  UNREACHABLE();
  return NULL;
}


LInstruction* LChunkBuilder::DoDeoptimize(HDeoptimize* instr) {
  return AssignEnvironment(new(zone()) LDeoptimize);
}


LInstruction* LChunkBuilder::DoShift(Token::Value op,
                                     HBitwiseBinaryOperation* instr) {
  if (instr->representation().IsSmiOrInteger32()) {
    ASSERT(instr->left()->representation().Equals(instr->representation()));
    ASSERT(instr->right()->representation().Equals(instr->representation()));
    LOperand* left = UseRegisterAtStart(instr->left());

    HValue* right_value = instr->right();
    LOperand* right = NULL;
    int constant_value = 0;
    bool does_deopt = false;
    if (right_value->IsConstant()) {
      HConstant* constant = HConstant::cast(right_value);
      right = chunk_->DefineConstantOperand(constant);
      constant_value = constant->Integer32Value() & 0x1f;
      // Left shifts can deoptimize if we shift by > 0 and the result cannot be
      // truncated to smi.
      if (instr->representation().IsSmi() && constant_value > 0) {
        does_deopt = !instr->CheckUsesForFlag(HValue::kTruncatingToSmi);
      }
    } else {
      right = UseFixed(right_value, ecx);
    }

    // Shift operations can only deoptimize if we do a logical shift by 0 and
    // the result cannot be truncated to int32.
    if (op == Token::SHR && constant_value == 0) {
      if (FLAG_opt_safe_uint32_operations) {
        does_deopt = !instr->CheckFlag(HInstruction::kUint32);
      } else {
        does_deopt = !instr->CheckUsesForFlag(HValue::kTruncatingToInt32);
      }
    }

    LInstruction* result =
        DefineSameAsFirst(new(zone()) LShiftI(op, left, right, does_deopt));
    return does_deopt ? AssignEnvironment(result) : result;
  } else {
    return DoArithmeticT(op, instr);
  }
}


LInstruction* LChunkBuilder::DoArithmeticD(Token::Value op,
                                           HArithmeticBinaryOperation* instr) {
  ASSERT(instr->representation().IsDouble());
  ASSERT(instr->left()->representation().IsDouble());
  ASSERT(instr->right()->representation().IsDouble());
  if (op == Token::MOD) {
    LOperand* left = UseRegisterAtStart(instr->BetterLeftOperand());
    LOperand* right = UseRegisterAtStart(instr->BetterRightOperand());
    LArithmeticD* result = new(zone()) LArithmeticD(op, left, right);
    return MarkAsCall(DefineSameAsFirst(result), instr);
  } else {
    LOperand* left = UseRegisterAtStart(instr->BetterLeftOperand());
    LOperand* right = UseRegisterAtStart(instr->BetterRightOperand());
    LArithmeticD* result = new(zone()) LArithmeticD(op, left, right);
    return DefineSameAsFirst(result);
  }
}


LInstruction* LChunkBuilder::DoArithmeticT(Token::Value op,
                                           HBinaryOperation* instr) {
  HValue* left = instr->left();
  HValue* right = instr->right();
  ASSERT(left->representation().IsTagged());
  ASSERT(right->representation().IsTagged());
  LOperand* context = UseFixed(instr->context(), esi);
  LOperand* left_operand = UseFixed(left, edx);
  LOperand* right_operand = UseFixed(right, eax);
  LArithmeticT* result =
      new(zone()) LArithmeticT(op, context, left_operand, right_operand);
  return MarkAsCall(DefineFixed(result, eax), instr);
}


void LChunkBuilder::DoBasicBlock(HBasicBlock* block, HBasicBlock* next_block) {
  ASSERT(is_building());
  current_block_ = block;
  next_block_ = next_block;
  if (block->IsStartBlock()) {
    block->UpdateEnvironment(graph_->start_environment());
    argument_count_ = 0;
  } else if (block->predecessors()->length() == 1) {
    // We have a single predecessor => copy environment and outgoing
    // argument count from the predecessor.
    ASSERT(block->phis()->length() == 0);
    HBasicBlock* pred = block->predecessors()->at(0);
    HEnvironment* last_environment = pred->last_environment();
    ASSERT(last_environment != NULL);
    // Only copy the environment, if it is later used again.
    if (pred->end()->SecondSuccessor() == NULL) {
      ASSERT(pred->end()->FirstSuccessor() == block);
    } else {
      if (pred->end()->FirstSuccessor()->block_id() > block->block_id() ||
          pred->end()->SecondSuccessor()->block_id() > block->block_id()) {
        last_environment = last_environment->Copy();
      }
    }
    block->UpdateEnvironment(last_environment);
    ASSERT(pred->argument_count() >= 0);
    argument_count_ = pred->argument_count();
  } else {
    // We are at a state join => process phis.
    HBasicBlock* pred = block->predecessors()->at(0);
    // No need to copy the environment, it cannot be used later.
    HEnvironment* last_environment = pred->last_environment();
    for (int i = 0; i < block->phis()->length(); ++i) {
      HPhi* phi = block->phis()->at(i);
      if (phi->HasMergedIndex()) {
        last_environment->SetValueAt(phi->merged_index(), phi);
      }
    }
    for (int i = 0; i < block->deleted_phis()->length(); ++i) {
      if (block->deleted_phis()->at(i) < last_environment->length()) {
        last_environment->SetValueAt(block->deleted_phis()->at(i),
                                     graph_->GetConstantUndefined());
      }
    }
    block->UpdateEnvironment(last_environment);
    // Pick up the outgoing argument count of one of the predecessors.
    argument_count_ = pred->argument_count();
  }
  HInstruction* current = block->first();
  int start = chunk_->instructions()->length();
  while (current != NULL && !is_aborted()) {
    // Code for constants in registers is generated lazily.
    if (!current->EmitAtUses()) {
      VisitInstruction(current);
    }
    current = current->next();
  }
  int end = chunk_->instructions()->length() - 1;
  if (end >= start) {
    block->set_first_instruction_index(start);
    block->set_last_instruction_index(end);
  }
  block->set_argument_count(argument_count_);
  next_block_ = NULL;
  current_block_ = NULL;
}


void LChunkBuilder::VisitInstruction(HInstruction* current) {
  HInstruction* old_current = current_instruction_;
  current_instruction_ = current;

  LInstruction* instr = NULL;
  if (current->CanReplaceWithDummyUses()) {
    if (current->OperandCount() == 0) {
      instr = DefineAsRegister(new(zone()) LDummy());
    } else {
      instr = DefineAsRegister(new(zone())
          LDummyUse(UseAny(current->OperandAt(0))));
    }
    for (int i = 1; i < current->OperandCount(); ++i) {
      LInstruction* dummy =
          new(zone()) LDummyUse(UseAny(current->OperandAt(i)));
      dummy->set_hydrogen_value(current);
      chunk_->AddInstruction(dummy, current_block_);
    }
  } else {
    instr = current->CompileToLithium(this);
  }

  argument_count_ += current->argument_delta();
  ASSERT(argument_count_ >= 0);

  if (instr != NULL) {
    // Associate the hydrogen instruction first, since we may need it for
    // the ClobbersRegisters() or ClobbersDoubleRegisters() calls below.
    instr->set_hydrogen_value(current);

#if DEBUG
    // Make sure that the lithium instruction has either no fixed register
    // constraints in temps or the result OR no uses that are only used at
    // start. If this invariant doesn't hold, the register allocator can decide
    // to insert a split of a range immediately before the instruction due to an
    // already allocated register needing to be used for the instruction's fixed
    // register constraint. In this case, The register allocator won't see an
    // interference between the split child and the use-at-start (it would if
    // the it was just a plain use), so it is free to move the split child into
    // the same register that is used for the use-at-start.
    // See https://code.google.com/p/chromium/issues/detail?id=201590
    if (!(instr->ClobbersRegisters() && instr->ClobbersDoubleRegisters())) {
      int fixed = 0;
      int used_at_start = 0;
      for (UseIterator it(instr); !it.Done(); it.Advance()) {
        LUnallocated* operand = LUnallocated::cast(it.Current());
        if (operand->IsUsedAtStart()) ++used_at_start;
      }
      if (instr->Output() != NULL) {
        if (LUnallocated::cast(instr->Output())->HasFixedPolicy()) ++fixed;
      }
      for (TempIterator it(instr); !it.Done(); it.Advance()) {
        LUnallocated* operand = LUnallocated::cast(it.Current());
        if (operand->HasFixedPolicy()) ++fixed;
      }
      ASSERT(fixed == 0 || used_at_start == 0);
    }
#endif

    if (FLAG_stress_pointer_maps && !instr->HasPointerMap()) {
      instr = AssignPointerMap(instr);
    }
    if (FLAG_stress_environments && !instr->HasEnvironment()) {
      instr = AssignEnvironment(instr);
    }
    if (!CpuFeatures::IsSafeForSnapshot(SSE2) && instr->IsGoto() &&
        LGoto::cast(instr)->jumps_to_join()) {
      // TODO(olivf) Since phis of spilled values are joined as registers
      // (not in the stack slot), we need to allow the goto gaps to keep one
      // x87 register alive. To ensure all other values are still spilled, we
      // insert a fpu register barrier right before.
      LClobberDoubles* clobber = new(zone()) LClobberDoubles();
      clobber->set_hydrogen_value(current);
      chunk_->AddInstruction(clobber, current_block_);
    }
    chunk_->AddInstruction(instr, current_block_);
  }
  current_instruction_ = old_current;
}


LInstruction* LChunkBuilder::DoGoto(HGoto* instr) {
  return new(zone()) LGoto(instr->FirstSuccessor());
}


LInstruction* LChunkBuilder::DoBranch(HBranch* instr) {
  LInstruction* goto_instr = CheckElideControlInstruction(instr);
  if (goto_instr != NULL) return goto_instr;

  ToBooleanStub::Types expected = instr->expected_input_types();

  // Tagged values that are not known smis or booleans require a
  // deoptimization environment. If the instruction is generic no
  // environment is needed since all cases are handled.
  HValue* value = instr->value();
  Representation rep = value->representation();
  HType type = value->type();
  if (!rep.IsTagged() || type.IsSmi() || type.IsBoolean()) {
    return new(zone()) LBranch(UseRegister(value), NULL);
  }

  bool needs_temp = expected.NeedsMap() || expected.IsEmpty();
  LOperand* temp = needs_temp ? TempRegister() : NULL;

  // The Generic stub does not have a deopt, so we need no environment.
  if (expected.IsGeneric()) {
    return new(zone()) LBranch(UseRegister(value), temp);
  }

  // We need a temporary register when we have to access the map *or* we have
  // no type info yet, in which case we handle all cases (including the ones
  // involving maps).
  return AssignEnvironment(new(zone()) LBranch(UseRegister(value), temp));
}


LInstruction* LChunkBuilder::DoDebugBreak(HDebugBreak* instr) {
  return new(zone()) LDebugBreak();
}


LInstruction* LChunkBuilder::DoCompareMap(HCompareMap* instr) {
  ASSERT(instr->value()->representation().IsTagged());
  LOperand* value = UseRegisterAtStart(instr->value());
  return new(zone()) LCmpMapAndBranch(value);
}


LInstruction* LChunkBuilder::DoArgumentsLength(HArgumentsLength* length) {
  info()->MarkAsRequiresFrame();
  return DefineAsRegister(new(zone()) LArgumentsLength(Use(length->value())));
}


LInstruction* LChunkBuilder::DoArgumentsElements(HArgumentsElements* elems) {
  info()->MarkAsRequiresFrame();
  return DefineAsRegister(new(zone()) LArgumentsElements);
}


LInstruction* LChunkBuilder::DoInstanceOf(HInstanceOf* instr) {
  LOperand* left = UseFixed(instr->left(), InstanceofStub::left());
  LOperand* right = UseFixed(instr->right(), InstanceofStub::right());
  LOperand* context = UseFixed(instr->context(), esi);
  LInstanceOf* result = new(zone()) LInstanceOf(context, left, right);
  return MarkAsCall(DefineFixed(result, eax), instr);
}


LInstruction* LChunkBuilder::DoInstanceOfKnownGlobal(
    HInstanceOfKnownGlobal* instr) {
  LInstanceOfKnownGlobal* result =
      new(zone()) LInstanceOfKnownGlobal(
          UseFixed(instr->context(), esi),
          UseFixed(instr->left(), InstanceofStub::left()),
          FixedTemp(edi));
  return MarkAsCall(DefineFixed(result, eax), instr);
}


LInstruction* LChunkBuilder::DoWrapReceiver(HWrapReceiver* instr) {
  LOperand* receiver = UseRegister(instr->receiver());
  LOperand* function = UseRegister(instr->function());
  LOperand* temp = TempRegister();
  LWrapReceiver* result =
      new(zone()) LWrapReceiver(receiver, function, temp);
  return AssignEnvironment(DefineSameAsFirst(result));
}


LInstruction* LChunkBuilder::DoApplyArguments(HApplyArguments* instr) {
  LOperand* function = UseFixed(instr->function(), edi);
  LOperand* receiver = UseFixed(instr->receiver(), eax);
  LOperand* length = UseFixed(instr->length(), ebx);
  LOperand* elements = UseFixed(instr->elements(), ecx);
  LApplyArguments* result = new(zone()) LApplyArguments(function,
                                                        receiver,
                                                        length,
                                                        elements);
  return MarkAsCall(DefineFixed(result, eax), instr, CAN_DEOPTIMIZE_EAGERLY);
}


LInstruction* LChunkBuilder::DoPushArgument(HPushArgument* instr) {
  LOperand* argument = UseAny(instr->argument());
  return new(zone()) LPushArgument(argument);
}


LInstruction* LChunkBuilder::DoStoreCodeEntry(
    HStoreCodeEntry* store_code_entry) {
  LOperand* function = UseRegister(store_code_entry->function());
  LOperand* code_object = UseTempRegister(store_code_entry->code_object());
  return new(zone()) LStoreCodeEntry(function, code_object);
}


LInstruction* LChunkBuilder::DoInnerAllocatedObject(
    HInnerAllocatedObject* instr) {
  LOperand* base_object = UseRegisterAtStart(instr->base_object());
  LOperand* offset = UseRegisterOrConstantAtStart(instr->offset());
  return DefineAsRegister(
      new(zone()) LInnerAllocatedObject(base_object, offset));
}


LInstruction* LChunkBuilder::DoThisFunction(HThisFunction* instr) {
  return instr->HasNoUses()
      ? NULL
      : DefineAsRegister(new(zone()) LThisFunction);
}


LInstruction* LChunkBuilder::DoContext(HContext* instr) {
  if (instr->HasNoUses()) return NULL;

  if (info()->IsStub()) {
    return DefineFixed(new(zone()) LContext, esi);
  }

  return DefineAsRegister(new(zone()) LContext);
}


LInstruction* LChunkBuilder::DoOuterContext(HOuterContext* instr) {
  LOperand* context = UseRegisterAtStart(instr->value());
  return DefineAsRegister(new(zone()) LOuterContext(context));
}


LInstruction* LChunkBuilder::DoDeclareGlobals(HDeclareGlobals* instr) {
  LOperand* context = UseFixed(instr->context(), esi);
  return MarkAsCall(new(zone()) LDeclareGlobals(context), instr);
}


LInstruction* LChunkBuilder::DoGlobalObject(HGlobalObject* instr) {
  LOperand* context = UseRegisterAtStart(instr->value());
  return DefineAsRegister(new(zone()) LGlobalObject(context));
}


LInstruction* LChunkBuilder::DoGlobalReceiver(HGlobalReceiver* instr) {
  LOperand* global_object = UseRegisterAtStart(instr->value());
  return DefineAsRegister(new(zone()) LGlobalReceiver(global_object));
}


LInstruction* LChunkBuilder::DoCallJSFunction(
    HCallJSFunction* instr) {
  LOperand* function = UseFixed(instr->function(), edi);

  LCallJSFunction* result = new(zone()) LCallJSFunction(function);

  return MarkAsCall(DefineFixed(result, eax), instr, CANNOT_DEOPTIMIZE_EAGERLY);
}


LInstruction* LChunkBuilder::DoCallWithDescriptor(
    HCallWithDescriptor* instr) {
  const CallInterfaceDescriptor* descriptor = instr->descriptor();

  LOperand* target = UseRegisterOrConstantAtStart(instr->target());
  ZoneList<LOperand*> ops(instr->OperandCount(), zone());
  ops.Add(target, zone());
  for (int i = 1; i < instr->OperandCount(); i++) {
    LOperand* op = UseFixed(instr->OperandAt(i),
        descriptor->GetParameterRegister(i - 1));
    ops.Add(op, zone());
  }

  LCallWithDescriptor* result = new(zone()) LCallWithDescriptor(
      descriptor, ops, zone());
  return MarkAsCall(DefineFixed(result, eax), instr, CANNOT_DEOPTIMIZE_EAGERLY);
}


LInstruction* LChunkBuilder::DoInvokeFunction(HInvokeFunction* instr) {
  LOperand* context = UseFixed(instr->context(), esi);
  LOperand* function = UseFixed(instr->function(), edi);
  LInvokeFunction* result = new(zone()) LInvokeFunction(context, function);
  return MarkAsCall(DefineFixed(result, eax), instr, CANNOT_DEOPTIMIZE_EAGERLY);
}


LInstruction* LChunkBuilder::DoUnaryMathOperation(HUnaryMathOperation* instr) {
  switch (instr->op()) {
    case kMathFloor: return DoMathFloor(instr);
    case kMathRound: return DoMathRound(instr);
    case kMathAbs: return DoMathAbs(instr);
    case kMathLog: return DoMathLog(instr);
    case kMathExp: return DoMathExp(instr);
    case kMathSqrt: return DoMathSqrt(instr);
    case kMathPowHalf: return DoMathPowHalf(instr);
    default:
      UNREACHABLE();
      return NULL;
  }
}


LInstruction* LChunkBuilder::DoMathFloor(HUnaryMathOperation* instr) {
  LOperand* input = UseRegisterAtStart(instr->value());
  LMathFloor* result = new(zone()) LMathFloor(input);
  return AssignEnvironment(DefineAsRegister(result));
}


LInstruction* LChunkBuilder::DoMathRound(HUnaryMathOperation* instr) {
  LOperand* input = UseRegister(instr->value());
  LOperand* temp = FixedTemp(xmm4);
  LMathRound* result = new(zone()) LMathRound(input, temp);
  return AssignEnvironment(DefineAsRegister(result));
}


LInstruction* LChunkBuilder::DoMathAbs(HUnaryMathOperation* instr) {
  LOperand* context = UseAny(instr->context());  // Deferred use.
  LOperand* input = UseRegisterAtStart(instr->value());
  LMathAbs* result = new(zone()) LMathAbs(context, input);
  return AssignEnvironment(AssignPointerMap(DefineSameAsFirst(result)));
}


LInstruction* LChunkBuilder::DoMathLog(HUnaryMathOperation* instr) {
  ASSERT(instr->representation().IsDouble());
  ASSERT(instr->value()->representation().IsDouble());
  LOperand* input = UseRegisterAtStart(instr->value());
  return MarkAsCall(DefineSameAsFirst(new(zone()) LMathLog(input)), instr);
}


LInstruction* LChunkBuilder::DoMathExp(HUnaryMathOperation* instr) {
  ASSERT(instr->representation().IsDouble());
  ASSERT(instr->value()->representation().IsDouble());
  LOperand* value = UseTempRegister(instr->value());
  LOperand* temp1 = TempRegister();
  LOperand* temp2 = TempRegister();
  LMathExp* result = new(zone()) LMathExp(value, temp1, temp2);
  return DefineAsRegister(result);
}


LInstruction* LChunkBuilder::DoMathSqrt(HUnaryMathOperation* instr) {
  LOperand* input = UseRegisterAtStart(instr->value());
  LMathSqrt* result = new(zone()) LMathSqrt(input);
  return DefineSameAsFirst(result);
}


LInstruction* LChunkBuilder::DoMathPowHalf(HUnaryMathOperation* instr) {
  LOperand* input = UseRegisterAtStart(instr->value());
  LOperand* temp = TempRegister();
  LMathPowHalf* result = new(zone()) LMathPowHalf(input, temp);
  return DefineSameAsFirst(result);
}


LInstruction* LChunkBuilder::DoCallNew(HCallNew* instr) {
  LOperand* context = UseFixed(instr->context(), esi);
  LOperand* constructor = UseFixed(instr->constructor(), edi);
  LCallNew* result = new(zone()) LCallNew(context, constructor);
  return MarkAsCall(DefineFixed(result, eax), instr);
}


LInstruction* LChunkBuilder::DoCallNewArray(HCallNewArray* instr) {
  LOperand* context = UseFixed(instr->context(), esi);
  LOperand* constructor = UseFixed(instr->constructor(), edi);
  LCallNewArray* result = new(zone()) LCallNewArray(context, constructor);
  return MarkAsCall(DefineFixed(result, eax), instr);
}


LInstruction* LChunkBuilder::DoCallFunction(HCallFunction* instr) {
  LOperand* context = UseFixed(instr->context(), esi);
  LOperand* function = UseFixed(instr->function(), edi);
  LCallFunction* call = new(zone()) LCallFunction(context, function);
  LInstruction* result = DefineFixed(call, eax);
  if (instr->IsTailCall()) return result;
  return MarkAsCall(result, instr);
}


LInstruction* LChunkBuilder::DoCallRuntime(HCallRuntime* instr) {
  LOperand* context = UseFixed(instr->context(), esi);
  return MarkAsCall(DefineFixed(new(zone()) LCallRuntime(context), eax), instr);
}


LInstruction* LChunkBuilder::DoRor(HRor* instr) {
  return DoShift(Token::ROR, instr);
}


LInstruction* LChunkBuilder::DoShr(HShr* instr) {
  return DoShift(Token::SHR, instr);
}


LInstruction* LChunkBuilder::DoSar(HSar* instr) {
  return DoShift(Token::SAR, instr);
}


LInstruction* LChunkBuilder::DoShl(HShl* instr) {
  return DoShift(Token::SHL, instr);
}


LInstruction* LChunkBuilder::DoBitwise(HBitwise* instr) {
  if (instr->representation().IsSmiOrInteger32()) {
    ASSERT(instr->left()->representation().Equals(instr->representation()));
    ASSERT(instr->right()->representation().Equals(instr->representation()));
    ASSERT(instr->CheckFlag(HValue::kTruncatingToInt32));

    LOperand* left = UseRegisterAtStart(instr->BetterLeftOperand());
    LOperand* right = UseOrConstantAtStart(instr->BetterRightOperand());
    return DefineSameAsFirst(new(zone()) LBitI(left, right));
  } else {
    return DoArithmeticT(instr->op(), instr);
  }
}


LInstruction* LChunkBuilder::DoDiv(HDiv* instr) {
  if (instr->representation().IsSmiOrInteger32()) {
    ASSERT(instr->left()->representation().Equals(instr->representation()));
    ASSERT(instr->right()->representation().Equals(instr->representation()));
    if (instr->HasPowerOf2Divisor()) {
      ASSERT(!instr->CheckFlag(HValue::kCanBeDivByZero));
      LOperand* value = UseRegisterAtStart(instr->left());
      LDivI* div =
          new(zone()) LDivI(value, UseOrConstant(instr->right()), NULL);
      return AssignEnvironment(DefineSameAsFirst(div));
    }
    // The temporary operand is necessary to ensure that right is not allocated
    // into edx.
    LOperand* temp = FixedTemp(edx);
    LOperand* dividend = UseFixed(instr->left(), eax);
    LOperand* divisor = UseRegister(instr->right());
    LDivI* result = new(zone()) LDivI(dividend, divisor, temp);
    return AssignEnvironment(DefineFixed(result, eax));
  } else if (instr->representation().IsDouble()) {
    return DoArithmeticD(Token::DIV, instr);
  } else {
    return DoArithmeticT(Token::DIV, instr);
  }
}


HValue* LChunkBuilder::SimplifiedDivisorForMathFloorOfDiv(HValue* divisor) {
  if (divisor->IsConstant() &&
      HConstant::cast(divisor)->HasInteger32Value()) {
    HConstant* constant_val = HConstant::cast(divisor);
    return constant_val->CopyToRepresentation(Representation::Integer32(),
                                              divisor->block()->zone());
  }
  // A value with an integer representation does not need to be transformed.
  if (divisor->representation().IsInteger32()) {
    return divisor;
  // A change from an integer32 can be replaced by the integer32 value.
  } else if (divisor->IsChange() &&
             HChange::cast(divisor)->from().IsInteger32()) {
    return HChange::cast(divisor)->value();
  }
  return NULL;
}


LInstruction* LChunkBuilder::DoMathFloorOfDiv(HMathFloorOfDiv* instr) {
  HValue* right = instr->right();
  if (!right->IsConstant()) {
    ASSERT(right->representation().IsInteger32());
    // The temporary operand is necessary to ensure that right is not allocated
    // into edx.
    LOperand* temp = FixedTemp(edx);
    LOperand* dividend = UseFixed(instr->left(), eax);
    LOperand* divisor = UseRegister(instr->right());
    LDivI* flooring_div = new(zone()) LDivI(dividend, divisor, temp);
    return AssignEnvironment(DefineFixed(flooring_div, eax));
  }

  ASSERT(right->IsConstant() && HConstant::cast(right)->HasInteger32Value());
  LOperand* divisor = chunk_->DefineConstantOperand(HConstant::cast(right));
  int32_t divisor_si = HConstant::cast(right)->Integer32Value();
  if (divisor_si == 0) {
    LOperand* dividend = UseRegister(instr->left());
    return AssignEnvironment(DefineAsRegister(
        new(zone()) LMathFloorOfDiv(dividend, divisor, NULL)));
  } else if (IsPowerOf2(abs(divisor_si))) {
    // use dividend as temp if divisor < 0 && divisor != -1
    LOperand* dividend = divisor_si < -1 ? UseTempRegister(instr->left()) :
                         UseRegisterAtStart(instr->left());
    LInstruction* result = DefineAsRegister(
        new(zone()) LMathFloorOfDiv(dividend, divisor, NULL));
    return divisor_si < 0 ? AssignEnvironment(result) : result;
  } else {
    // needs edx:eax, plus a temp
    LOperand* dividend = UseFixed(instr->left(), eax);
    LOperand* temp = TempRegister();
    LInstruction* result = DefineFixed(
        new(zone()) LMathFloorOfDiv(dividend, divisor, temp), edx);
    return divisor_si < 0 ? AssignEnvironment(result) : result;
  }
}


LInstruction* LChunkBuilder::DoMod(HMod* instr) {
  HValue* left = instr->left();
  HValue* right = instr->right();
  if (instr->representation().IsSmiOrInteger32()) {
    ASSERT(instr->left()->representation().Equals(instr->representation()));
    ASSERT(instr->right()->representation().Equals(instr->representation()));

    if (instr->HasPowerOf2Divisor()) {
      ASSERT(!right->CanBeZero());
      LModI* mod = new(zone()) LModI(UseRegisterAtStart(left),
                                     UseOrConstant(right),
                                     NULL);
      LInstruction* result = DefineSameAsFirst(mod);
      return (left->CanBeNegative() &&
              instr->CheckFlag(HValue::kBailoutOnMinusZero))
          ? AssignEnvironment(result)
          : result;
      return AssignEnvironment(DefineSameAsFirst(mod));
    } else {
      // The temporary operand is necessary to ensure that right is not
      // allocated into edx.
      LModI* mod = new(zone()) LModI(UseFixed(left, eax),
                                     UseRegister(right),
                                     FixedTemp(edx));
      LInstruction* result = DefineFixed(mod, edx);
      return (right->CanBeZero() ||
              (left->RangeCanInclude(kMinInt) &&
               right->RangeCanInclude(-1) &&
               instr->CheckFlag(HValue::kBailoutOnMinusZero)) ||
              (left->CanBeNegative() &&
               instr->CanBeZero() &&
               instr->CheckFlag(HValue::kBailoutOnMinusZero)))
          ? AssignEnvironment(result)
          : result;
    }
  } else if (instr->representation().IsDouble()) {
    return DoArithmeticD(Token::MOD, instr);
  } else {
    return DoArithmeticT(Token::MOD, instr);
  }
}


LInstruction* LChunkBuilder::DoMul(HMul* instr) {
  if (instr->representation().IsSmiOrInteger32()) {
    ASSERT(instr->left()->representation().Equals(instr->representation()));
    ASSERT(instr->right()->representation().Equals(instr->representation()));
    LOperand* left = UseRegisterAtStart(instr->BetterLeftOperand());
    LOperand* right = UseOrConstant(instr->BetterRightOperand());
    LOperand* temp = NULL;
    if (instr->CheckFlag(HValue::kBailoutOnMinusZero)) {
      temp = TempRegister();
    }
    LMulI* mul = new(zone()) LMulI(left, right, temp);
    if (instr->CheckFlag(HValue::kCanOverflow) ||
        instr->CheckFlag(HValue::kBailoutOnMinusZero)) {
      AssignEnvironment(mul);
    }
    return DefineSameAsFirst(mul);
  } else if (instr->representation().IsDouble()) {
    return DoArithmeticD(Token::MUL, instr);
  } else {
    return DoArithmeticT(Token::MUL, instr);
  }
}


LInstruction* LChunkBuilder::DoSub(HSub* instr) {
  if (instr->representation().IsSmiOrInteger32()) {
    ASSERT(instr->left()->representation().Equals(instr->representation()));
    ASSERT(instr->right()->representation().Equals(instr->representation()));
    LOperand* left = UseRegisterAtStart(instr->left());
    LOperand* right = UseOrConstantAtStart(instr->right());
    LSubI* sub = new(zone()) LSubI(left, right);
    LInstruction* result = DefineSameAsFirst(sub);
    if (instr->CheckFlag(HValue::kCanOverflow)) {
      result = AssignEnvironment(result);
    }
    return result;
  } else if (instr->representation().IsDouble()) {
    return DoArithmeticD(Token::SUB, instr);
  } else {
    return DoArithmeticT(Token::SUB, instr);
  }
}


LInstruction* LChunkBuilder::DoAdd(HAdd* instr) {
  if (instr->representation().IsSmiOrInteger32()) {
    ASSERT(instr->left()->representation().Equals(instr->representation()));
    ASSERT(instr->right()->representation().Equals(instr->representation()));
    // Check to see if it would be advantageous to use an lea instruction rather
    // than an add. This is the case when no overflow check is needed and there
    // are multiple uses of the add's inputs, so using a 3-register add will
    // preserve all input values for later uses.
    bool use_lea = LAddI::UseLea(instr);
    LOperand* left = UseRegisterAtStart(instr->BetterLeftOperand());
    HValue* right_candidate = instr->BetterRightOperand();
    LOperand* right = use_lea
        ? UseRegisterOrConstantAtStart(right_candidate)
        : UseOrConstantAtStart(right_candidate);
    LAddI* add = new(zone()) LAddI(left, right);
    bool can_overflow = instr->CheckFlag(HValue::kCanOverflow);
    LInstruction* result = use_lea
        ? DefineAsRegister(add)
        : DefineSameAsFirst(add);
    if (can_overflow) {
      result = AssignEnvironment(result);
    }
    return result;
  } else if (instr->representation().IsDouble()) {
    return DoArithmeticD(Token::ADD, instr);
  } else if (instr->representation().IsExternal()) {
    ASSERT(instr->left()->representation().IsExternal());
    ASSERT(instr->right()->representation().IsInteger32());
    ASSERT(!instr->CheckFlag(HValue::kCanOverflow));
    bool use_lea = LAddI::UseLea(instr);
    LOperand* left = UseRegisterAtStart(instr->left());
    HValue* right_candidate = instr->right();
    LOperand* right = use_lea
        ? UseRegisterOrConstantAtStart(right_candidate)
        : UseOrConstantAtStart(right_candidate);
    LAddI* add = new(zone()) LAddI(left, right);
    LInstruction* result = use_lea
        ? DefineAsRegister(add)
        : DefineSameAsFirst(add);
    return result;
  } else {
    return DoArithmeticT(Token::ADD, instr);
  }
}


LInstruction* LChunkBuilder::DoMathMinMax(HMathMinMax* instr) {
  LOperand* left = NULL;
  LOperand* right = NULL;
  if (instr->representation().IsSmiOrInteger32()) {
    ASSERT(instr->left()->representation().Equals(instr->representation()));
    ASSERT(instr->right()->representation().Equals(instr->representation()));
    left = UseRegisterAtStart(instr->BetterLeftOperand());
    right = UseOrConstantAtStart(instr->BetterRightOperand());
  } else {
    ASSERT(instr->representation().IsDouble());
    ASSERT(instr->left()->representation().IsDouble());
    ASSERT(instr->right()->representation().IsDouble());
    left = UseRegisterAtStart(instr->left());
    right = UseRegisterAtStart(instr->right());
  }
  LMathMinMax* minmax = new(zone()) LMathMinMax(left, right);
  return DefineSameAsFirst(minmax);
}


LInstruction* LChunkBuilder::DoPower(HPower* instr) {
  ASSERT(instr->representation().IsDouble());
  // We call a C function for double power. It can't trigger a GC.
  // We need to use fixed result register for the call.
  Representation exponent_type = instr->right()->representation();
  ASSERT(instr->left()->representation().IsDouble());
  LOperand* left = UseFixedDouble(instr->left(), xmm2);
  LOperand* right = exponent_type.IsDouble() ?
      UseFixedDouble(instr->right(), xmm1) :
      UseFixed(instr->right(), eax);
  LPower* result = new(zone()) LPower(left, right);
  return MarkAsCall(DefineFixedDouble(result, xmm3), instr,
                    CAN_DEOPTIMIZE_EAGERLY);
}


LInstruction* LChunkBuilder::DoCompareGeneric(HCompareGeneric* instr) {
  ASSERT(instr->left()->representation().IsSmiOrTagged());
  ASSERT(instr->right()->representation().IsSmiOrTagged());
  LOperand* context = UseFixed(instr->context(), esi);
  LOperand* left = UseFixed(instr->left(), edx);
  LOperand* right = UseFixed(instr->right(), eax);
  LCmpT* result = new(zone()) LCmpT(context, left, right);
  return MarkAsCall(DefineFixed(result, eax), instr);
}


LInstruction* LChunkBuilder::DoCompareNumericAndBranch(
    HCompareNumericAndBranch* instr) {
  Representation r = instr->representation();
  if (r.IsSmiOrInteger32()) {
    ASSERT(instr->left()->representation().Equals(r));
    ASSERT(instr->right()->representation().Equals(r));
    LOperand* left = UseRegisterOrConstantAtStart(instr->left());
    LOperand* right = UseOrConstantAtStart(instr->right());
    return new(zone()) LCompareNumericAndBranch(left, right);
  } else {
    ASSERT(r.IsDouble());
    ASSERT(instr->left()->representation().IsDouble());
    ASSERT(instr->right()->representation().IsDouble());
    LOperand* left;
    LOperand* right;
    if (CanBeImmediateConstant(instr->left()) &&
        CanBeImmediateConstant(instr->right())) {
      // The code generator requires either both inputs to be constant
      // operands, or neither.
      left = UseConstant(instr->left());
      right = UseConstant(instr->right());
    } else {
      left = UseRegisterAtStart(instr->left());
      right = UseRegisterAtStart(instr->right());
    }
    return new(zone()) LCompareNumericAndBranch(left, right);
  }
}


LInstruction* LChunkBuilder::DoCompareObjectEqAndBranch(
    HCompareObjectEqAndBranch* instr) {
  LInstruction* goto_instr = CheckElideControlInstruction(instr);
  if (goto_instr != NULL) return goto_instr;
  LOperand* left = UseRegisterAtStart(instr->left());
  LOperand* right = UseOrConstantAtStart(instr->right());
  return new(zone()) LCmpObjectEqAndBranch(left, right);
}


LInstruction* LChunkBuilder::DoCompareHoleAndBranch(
    HCompareHoleAndBranch* instr) {
  LOperand* value = UseRegisterAtStart(instr->value());
  return new(zone()) LCmpHoleAndBranch(value);
}


LInstruction* LChunkBuilder::DoCompareMinusZeroAndBranch(
    HCompareMinusZeroAndBranch* instr) {
  LInstruction* goto_instr = CheckElideControlInstruction(instr);
  if (goto_instr != NULL) return goto_instr;
  LOperand* value = UseRegister(instr->value());
  LOperand* scratch = TempRegister();
  return new(zone()) LCompareMinusZeroAndBranch(value, scratch);
}


LInstruction* LChunkBuilder::DoIsObjectAndBranch(HIsObjectAndBranch* instr) {
  ASSERT(instr->value()->representation().IsSmiOrTagged());
  LOperand* temp = TempRegister();
  return new(zone()) LIsObjectAndBranch(UseRegister(instr->value()), temp);
}


LInstruction* LChunkBuilder::DoIsStringAndBranch(HIsStringAndBranch* instr) {
  ASSERT(instr->value()->representation().IsTagged());
  LOperand* temp = TempRegister();
  return new(zone()) LIsStringAndBranch(UseRegister(instr->value()), temp);
}


LInstruction* LChunkBuilder::DoIsSmiAndBranch(HIsSmiAndBranch* instr) {
  ASSERT(instr->value()->representation().IsTagged());
  return new(zone()) LIsSmiAndBranch(Use(instr->value()));
}


LInstruction* LChunkBuilder::DoIsUndetectableAndBranch(
    HIsUndetectableAndBranch* instr) {
  ASSERT(instr->value()->representation().IsTagged());
  return new(zone()) LIsUndetectableAndBranch(
      UseRegisterAtStart(instr->value()), TempRegister());
}


LInstruction* LChunkBuilder::DoStringCompareAndBranch(
    HStringCompareAndBranch* instr) {
  ASSERT(instr->left()->representation().IsTagged());
  ASSERT(instr->right()->representation().IsTagged());
  LOperand* context = UseFixed(instr->context(), esi);
  LOperand* left = UseFixed(instr->left(), edx);
  LOperand* right = UseFixed(instr->right(), eax);

  LStringCompareAndBranch* result = new(zone())
      LStringCompareAndBranch(context, left, right);

  return MarkAsCall(result, instr);
}


LInstruction* LChunkBuilder::DoHasInstanceTypeAndBranch(
    HHasInstanceTypeAndBranch* instr) {
  ASSERT(instr->value()->representation().IsTagged());
  return new(zone()) LHasInstanceTypeAndBranch(
      UseRegisterAtStart(instr->value()),
      TempRegister());
}


LInstruction* LChunkBuilder::DoGetCachedArrayIndex(
    HGetCachedArrayIndex* instr)  {
  ASSERT(instr->value()->representation().IsTagged());
  LOperand* value = UseRegisterAtStart(instr->value());

  return DefineAsRegister(new(zone()) LGetCachedArrayIndex(value));
}


LInstruction* LChunkBuilder::DoHasCachedArrayIndexAndBranch(
    HHasCachedArrayIndexAndBranch* instr) {
  ASSERT(instr->value()->representation().IsTagged());
  return new(zone()) LHasCachedArrayIndexAndBranch(
      UseRegisterAtStart(instr->value()));
}


LInstruction* LChunkBuilder::DoClassOfTestAndBranch(
    HClassOfTestAndBranch* instr) {
  ASSERT(instr->value()->representation().IsTagged());
  return new(zone()) LClassOfTestAndBranch(UseRegister(instr->value()),
                                           TempRegister(),
                                           TempRegister());
}


LInstruction* LChunkBuilder::DoMapEnumLength(HMapEnumLength* instr) {
  LOperand* map = UseRegisterAtStart(instr->value());
  return DefineAsRegister(new(zone()) LMapEnumLength(map));
}


LInstruction* LChunkBuilder::DoElementsKind(HElementsKind* instr) {
  LOperand* object = UseRegisterAtStart(instr->value());
  return DefineAsRegister(new(zone()) LElementsKind(object));
}


LInstruction* LChunkBuilder::DoValueOf(HValueOf* instr) {
  LOperand* object = UseRegister(instr->value());
  LValueOf* result = new(zone()) LValueOf(object, TempRegister());
  return DefineSameAsFirst(result);
}


LInstruction* LChunkBuilder::DoDateField(HDateField* instr) {
  LOperand* date = UseFixed(instr->value(), eax);
  LDateField* result =
      new(zone()) LDateField(date, FixedTemp(ecx), instr->index());
  return MarkAsCall(DefineFixed(result, eax), instr, CAN_DEOPTIMIZE_EAGERLY);
}


LInstruction* LChunkBuilder::DoSeqStringGetChar(HSeqStringGetChar* instr) {
  LOperand* string = UseRegisterAtStart(instr->string());
  LOperand* index = UseRegisterOrConstantAtStart(instr->index());
  return DefineAsRegister(new(zone()) LSeqStringGetChar(string, index));
}


LOperand* LChunkBuilder::GetSeqStringSetCharOperand(HSeqStringSetChar* instr) {
  if (instr->encoding() == String::ONE_BYTE_ENCODING) {
    if (FLAG_debug_code) {
      return UseFixed(instr->value(), eax);
    } else {
      return UseFixedOrConstant(instr->value(), eax);
    }
  } else {
    if (FLAG_debug_code) {
      return UseRegisterAtStart(instr->value());
    } else {
      return UseRegisterOrConstantAtStart(instr->value());
    }
  }
}


LInstruction* LChunkBuilder::DoSeqStringSetChar(HSeqStringSetChar* instr) {
  LOperand* string = UseRegisterAtStart(instr->string());
  LOperand* index = FLAG_debug_code
      ? UseRegisterAtStart(instr->index())
      : UseRegisterOrConstantAtStart(instr->index());
  LOperand* value = GetSeqStringSetCharOperand(instr);
  LOperand* context = FLAG_debug_code ? UseFixed(instr->context(), esi) : NULL;
  LInstruction* result = new(zone()) LSeqStringSetChar(context, string,
                                                       index, value);
  if (FLAG_debug_code) {
    result = MarkAsCall(result, instr);
  }
  return result;
}


LInstruction* LChunkBuilder::DoBoundsCheck(HBoundsCheck* instr) {
  return AssignEnvironment(new(zone()) LBoundsCheck(
      UseRegisterOrConstantAtStart(instr->index()),
      UseAtStart(instr->length())));
}


LInstruction* LChunkBuilder::DoBoundsCheckBaseIndexInformation(
    HBoundsCheckBaseIndexInformation* instr) {
  UNREACHABLE();
  return NULL;
}


LInstruction* LChunkBuilder::DoAbnormalExit(HAbnormalExit* instr) {
  // The control instruction marking the end of a block that completed
  // abruptly (e.g., threw an exception).  There is nothing specific to do.
  return NULL;
}


LInstruction* LChunkBuilder::DoThrow(HThrow* instr) {
  LOperand* context = UseFixed(instr->context(), esi);
  LOperand* value = UseFixed(instr->value(), eax);
  return MarkAsCall(new(zone()) LThrow(context, value), instr);
}


LInstruction* LChunkBuilder::DoUseConst(HUseConst* instr) {
  return NULL;
}


LInstruction* LChunkBuilder::DoForceRepresentation(HForceRepresentation* bad) {
  // All HForceRepresentation instructions should be eliminated in the
  // representation change phase of Hydrogen.
  UNREACHABLE();
  return NULL;
}


LInstruction* LChunkBuilder::DoChange(HChange* instr) {
  Representation from = instr->from();
  Representation to = instr->to();
  if (from.IsSmi()) {
    if (to.IsTagged()) {
      LOperand* value = UseRegister(instr->value());
      return DefineSameAsFirst(new(zone()) LDummyUse(value));
    }
    from = Representation::Tagged();
  }
  // Only mark conversions that might need to allocate as calling rather than
  // all changes. This makes simple, non-allocating conversion not have to force
  // building a stack frame.
  if (from.IsTagged()) {
    if (to.IsDouble()) {
      LOperand* value = UseRegister(instr->value());
      // Temp register only necessary for minus zero check.
      LOperand* temp = TempRegister();
      LNumberUntagD* res = new(zone()) LNumberUntagD(value, temp);
      return AssignEnvironment(DefineAsRegister(res));
    } else if (to.IsSmi()) {
      HValue* val = instr->value();
      LOperand* value = UseRegister(val);
      if (val->type().IsSmi()) {
        return DefineSameAsFirst(new(zone()) LDummyUse(value));
      }
      return AssignEnvironment(DefineSameAsFirst(new(zone()) LCheckSmi(value)));
    } else {
      ASSERT(to.IsInteger32());
      HValue* val = instr->value();
      if (val->type().IsSmi() || val->representation().IsSmi()) {
        LOperand* value = UseRegister(val);
        return DefineSameAsFirst(new(zone()) LSmiUntag(value, false));
      } else {
        bool truncating = instr->CanTruncateToInt32();
        LOperand* xmm_temp =
            (CpuFeatures::IsSafeForSnapshot(SSE2) && !truncating)
                ? FixedTemp(xmm1) : NULL;
        LTaggedToI* res = new(zone()) LTaggedToI(UseRegister(val), xmm_temp);
        return AssignEnvironment(DefineSameAsFirst(res));
      }
    }
  } else if (from.IsDouble()) {
    if (to.IsTagged()) {
      info()->MarkAsDeferredCalling();
      LOperand* value = UseRegisterAtStart(instr->value());
      LOperand* temp = FLAG_inline_new ? TempRegister() : NULL;

      // Make sure that temp and result_temp are different registers.
      LUnallocated* result_temp = TempRegister();
      LNumberTagD* result = new(zone()) LNumberTagD(value, temp);
      return AssignPointerMap(Define(result, result_temp));
    } else if (to.IsSmi()) {
      LOperand* value = UseRegister(instr->value());
      return AssignEnvironment(
          DefineAsRegister(new(zone()) LDoubleToSmi(value)));
    } else {
      ASSERT(to.IsInteger32());
      bool truncating = instr->CanTruncateToInt32();
      bool needs_temp = CpuFeatures::IsSafeForSnapshot(SSE2) && !truncating;
      LOperand* value = needs_temp ?
          UseTempRegister(instr->value()) : UseRegister(instr->value());
      LOperand* temp = needs_temp ? TempRegister() : NULL;
      return AssignEnvironment(
          DefineAsRegister(new(zone()) LDoubleToI(value, temp)));
    }
  } else if (from.IsInteger32()) {
    info()->MarkAsDeferredCalling();
    if (to.IsTagged()) {
      HValue* val = instr->value();
      LOperand* value = UseRegister(val);
      if (val->HasRange() && val->range()->IsInSmiRange()) {
        return DefineSameAsFirst(new(zone()) LSmiTag(value));
      } else if (val->CheckFlag(HInstruction::kUint32)) {
        LOperand* temp = CpuFeatures::IsSupported(SSE2) ? FixedTemp(xmm1)
                                                        : NULL;
        LNumberTagU* result = new(zone()) LNumberTagU(value, temp);
        return AssignEnvironment(AssignPointerMap(DefineSameAsFirst(result)));
      } else {
        LNumberTagI* result = new(zone()) LNumberTagI(value);
        return AssignEnvironment(AssignPointerMap(DefineSameAsFirst(result)));
      }
    } else if (to.IsSmi()) {
      HValue* val = instr->value();
      LOperand* value = UseRegister(val);
      LInstruction* result = val->CheckFlag(HInstruction::kUint32)
           ? DefineSameAsFirst(new(zone()) LUint32ToSmi(value))
           : DefineSameAsFirst(new(zone()) LInteger32ToSmi(value));
      if (val->HasRange() && val->range()->IsInSmiRange()) {
        return result;
      }
      return AssignEnvironment(result);
    } else {
      ASSERT(to.IsDouble());
      if (instr->value()->CheckFlag(HInstruction::kUint32)) {
        LOperand* temp = FixedTemp(xmm1);
        return DefineAsRegister(
            new(zone()) LUint32ToDouble(UseRegister(instr->value()), temp));
      } else {
        return DefineAsRegister(
            new(zone()) LInteger32ToDouble(Use(instr->value())));
      }
    }
  }
  UNREACHABLE();
  return NULL;
}


LInstruction* LChunkBuilder::DoCheckHeapObject(HCheckHeapObject* instr) {
  LOperand* value = UseAtStart(instr->value());
  return AssignEnvironment(new(zone()) LCheckNonSmi(value));
}


LInstruction* LChunkBuilder::DoCheckSmi(HCheckSmi* instr) {
  LOperand* value = UseRegisterAtStart(instr->value());
  return AssignEnvironment(new(zone()) LCheckSmi(value));
}


LInstruction* LChunkBuilder::DoCheckInstanceType(HCheckInstanceType* instr) {
  LOperand* value = UseRegisterAtStart(instr->value());
  LOperand* temp = TempRegister();
  LCheckInstanceType* result = new(zone()) LCheckInstanceType(value, temp);
  return AssignEnvironment(result);
}


LInstruction* LChunkBuilder::DoCheckValue(HCheckValue* instr) {
  // If the object is in new space, we'll emit a global cell compare and so
  // want the value in a register.  If the object gets promoted before we
  // emit code, we will still get the register but will do an immediate
  // compare instead of the cell compare.  This is safe.
  LOperand* value = instr->object_in_new_space()
      ? UseRegisterAtStart(instr->value()) : UseAtStart(instr->value());
  return AssignEnvironment(new(zone()) LCheckValue(value));
}


LInstruction* LChunkBuilder::DoCheckMaps(HCheckMaps* instr) {
  LOperand* value = NULL;
  if (!instr->CanOmitMapChecks()) {
    value = UseRegisterAtStart(instr->value());
    if (instr->has_migration_target()) info()->MarkAsDeferredCalling();
  }
  LCheckMaps* result = new(zone()) LCheckMaps(value);
  if (!instr->CanOmitMapChecks()) {
    AssignEnvironment(result);
    if (instr->has_migration_target()) return AssignPointerMap(result);
  }
  return result;
}


LInstruction* LChunkBuilder::DoClampToUint8(HClampToUint8* instr) {
  HValue* value = instr->value();
  Representation input_rep = value->representation();
  if (input_rep.IsDouble()) {
    LOperand* reg = UseRegister(value);
    return DefineFixed(new(zone()) LClampDToUint8(reg), eax);
  } else if (input_rep.IsInteger32()) {
    LOperand* reg = UseFixed(value, eax);
    return DefineFixed(new(zone()) LClampIToUint8(reg), eax);
  } else {
    ASSERT(input_rep.IsSmiOrTagged());
    if (CpuFeatures::IsSupported(SSE2)) {
      LOperand* reg = UseFixed(value, eax);
      // Register allocator doesn't (yet) support allocation of double
      // temps. Reserve xmm1 explicitly.
      LOperand* temp = FixedTemp(xmm1);
      LClampTToUint8* result = new(zone()) LClampTToUint8(reg, temp);
      return AssignEnvironment(DefineFixed(result, eax));
    } else {
      LOperand* value = UseRegister(instr->value());
      LClampTToUint8NoSSE2* res =
          new(zone()) LClampTToUint8NoSSE2(value, TempRegister(),
                                           TempRegister(), TempRegister());
      return AssignEnvironment(DefineFixed(res, ecx));
    }
  }
}


LInstruction* LChunkBuilder::DoReturn(HReturn* instr) {
  LOperand* context = info()->IsStub() ? UseFixed(instr->context(), esi) : NULL;
  LOperand* parameter_count = UseRegisterOrConstant(instr->parameter_count());
  return new(zone()) LReturn(
      UseFixed(instr->value(), eax), context, parameter_count);
}


LInstruction* LChunkBuilder::DoConstant(HConstant* instr) {
  Representation r = instr->representation();
  if (r.IsSmi()) {
    return DefineAsRegister(new(zone()) LConstantS);
  } else if (r.IsInteger32()) {
    return DefineAsRegister(new(zone()) LConstantI);
  } else if (r.IsDouble()) {
    double value = instr->DoubleValue();
    bool value_is_zero = BitCast<uint64_t, double>(value) == 0;
    LOperand* temp = value_is_zero ? NULL : TempRegister();
    return DefineAsRegister(new(zone()) LConstantD(temp));
  } else if (r.IsExternal()) {
    return DefineAsRegister(new(zone()) LConstantE);
  } else if (r.IsTagged()) {
    return DefineAsRegister(new(zone()) LConstantT);
  } else {
    UNREACHABLE();
    return NULL;
  }
}


LInstruction* LChunkBuilder::DoLoadGlobalCell(HLoadGlobalCell* instr) {
  LLoadGlobalCell* result = new(zone()) LLoadGlobalCell;
  return instr->RequiresHoleCheck()
      ? AssignEnvironment(DefineAsRegister(result))
      : DefineAsRegister(result);
}


LInstruction* LChunkBuilder::DoLoadGlobalGeneric(HLoadGlobalGeneric* instr) {
  LOperand* context = UseFixed(instr->context(), esi);
  LOperand* global_object = UseFixed(instr->global_object(), edx);
  LLoadGlobalGeneric* result =
      new(zone()) LLoadGlobalGeneric(context, global_object);
  return MarkAsCall(DefineFixed(result, eax), instr);
}


LInstruction* LChunkBuilder::DoStoreGlobalCell(HStoreGlobalCell* instr) {
  LStoreGlobalCell* result =
      new(zone()) LStoreGlobalCell(UseRegister(instr->value()));
  return instr->RequiresHoleCheck() ? AssignEnvironment(result) : result;
}


LInstruction* LChunkBuilder::DoStoreGlobalGeneric(HStoreGlobalGeneric* instr) {
  LOperand* context = UseFixed(instr->context(), esi);
  LOperand* global_object = UseFixed(instr->global_object(), edx);
  LOperand* value = UseFixed(instr->value(), eax);
  LStoreGlobalGeneric* result =
      new(zone()) LStoreGlobalGeneric(context, global_object, value);
  return MarkAsCall(result, instr);
}


LInstruction* LChunkBuilder::DoLoadContextSlot(HLoadContextSlot* instr) {
  LOperand* context = UseRegisterAtStart(instr->value());
  LInstruction* result =
      DefineAsRegister(new(zone()) LLoadContextSlot(context));
  return instr->RequiresHoleCheck() ? AssignEnvironment(result) : result;
}


LInstruction* LChunkBuilder::DoStoreContextSlot(HStoreContextSlot* instr) {
  LOperand* value;
  LOperand* temp;
  LOperand* context = UseRegister(instr->context());
  if (instr->NeedsWriteBarrier()) {
    value = UseTempRegister(instr->value());
    temp = TempRegister();
  } else {
    value = UseRegister(instr->value());
    temp = NULL;
  }
  LInstruction* result = new(zone()) LStoreContextSlot(context, value, temp);
  return instr->RequiresHoleCheck() ? AssignEnvironment(result) : result;
}


LInstruction* LChunkBuilder::DoLoadNamedField(HLoadNamedField* instr) {
  LOperand* obj = (instr->access().IsExternalMemory() &&
                   instr->access().offset() == 0)
      ? UseRegisterOrConstantAtStart(instr->object())
      : UseRegisterAtStart(instr->object());
  return DefineAsRegister(new(zone()) LLoadNamedField(obj));
}


LInstruction* LChunkBuilder::DoLoadNamedGeneric(HLoadNamedGeneric* instr) {
  LOperand* context = UseFixed(instr->context(), esi);
  LOperand* object = UseFixed(instr->object(), edx);
  LLoadNamedGeneric* result = new(zone()) LLoadNamedGeneric(context, object);
  return MarkAsCall(DefineFixed(result, eax), instr);
}


LInstruction* LChunkBuilder::DoLoadFunctionPrototype(
    HLoadFunctionPrototype* instr) {
  return AssignEnvironment(DefineAsRegister(
      new(zone()) LLoadFunctionPrototype(UseRegister(instr->function()),
                                         TempRegister())));
}


LInstruction* LChunkBuilder::DoLoadRoot(HLoadRoot* instr) {
  return DefineAsRegister(new(zone()) LLoadRoot);
}


LInstruction* LChunkBuilder::DoLoadExternalArrayPointer(
    HLoadExternalArrayPointer* instr) {
  LOperand* input = UseRegisterAtStart(instr->value());
  return DefineAsRegister(new(zone()) LLoadExternalArrayPointer(input));
}


LInstruction* LChunkBuilder::DoLoadKeyed(HLoadKeyed* instr) {
  ASSERT(instr->key()->representation().IsSmiOrInteger32());
  ElementsKind elements_kind = instr->elements_kind();
  bool clobbers_key = ExternalArrayOpRequiresTemp(
      instr->key()->representation(), elements_kind);
  LOperand* key = clobbers_key
      ? UseTempRegister(instr->key())
      : UseRegisterOrConstantAtStart(instr->key());
  LLoadKeyed* result = NULL;

  if (!instr->is_external()) {
    LOperand* obj = UseRegisterAtStart(instr->elements());
    result = new(zone()) LLoadKeyed(obj, key);
  } else {
    ASSERT(
        (instr->representation().IsInteger32() &&
         (elements_kind != EXTERNAL_FLOAT_ELEMENTS) &&
         (elements_kind != EXTERNAL_DOUBLE_ELEMENTS)) ||
        (instr->representation().IsDouble() &&
         ((elements_kind == EXTERNAL_FLOAT_ELEMENTS) ||
          (elements_kind == EXTERNAL_DOUBLE_ELEMENTS))));
    LOperand* external_pointer = UseRegister(instr->elements());
    result = new(zone()) LLoadKeyed(external_pointer, key);
  }

  DefineAsRegister(result);
  bool can_deoptimize = instr->RequiresHoleCheck() ||
      (elements_kind == EXTERNAL_UNSIGNED_INT_ELEMENTS);
  // An unsigned int array load might overflow and cause a deopt, make sure it
  // has an environment.
  return can_deoptimize ? AssignEnvironment(result) : result;
}


LInstruction* LChunkBuilder::DoLoadKeyedGeneric(HLoadKeyedGeneric* instr) {
  LOperand* context = UseFixed(instr->context(), esi);
  LOperand* object = UseFixed(instr->object(), edx);
  LOperand* key = UseFixed(instr->key(), ecx);

  LLoadKeyedGeneric* result =
      new(zone()) LLoadKeyedGeneric(context, object, key);
  return MarkAsCall(DefineFixed(result, eax), instr);
}


LOperand* LChunkBuilder::GetStoreKeyedValueOperand(HStoreKeyed* instr) {
  ElementsKind elements_kind = instr->elements_kind();

  // Determine if we need a byte register in this case for the value.
  bool val_is_fixed_register =
      elements_kind == EXTERNAL_BYTE_ELEMENTS ||
      elements_kind == EXTERNAL_UNSIGNED_BYTE_ELEMENTS ||
      elements_kind == EXTERNAL_PIXEL_ELEMENTS;
  if (val_is_fixed_register) {
    return UseFixed(instr->value(), eax);
  }

  if (!CpuFeatures::IsSafeForSnapshot(SSE2) &&
      IsDoubleOrFloatElementsKind(elements_kind)) {
    return UseRegisterAtStart(instr->value());
  }

  return UseRegister(instr->value());
}


LInstruction* LChunkBuilder::DoStoreKeyed(HStoreKeyed* instr) {
  if (!instr->is_external()) {
    ASSERT(instr->elements()->representation().IsTagged());
    ASSERT(instr->key()->representation().IsInteger32() ||
           instr->key()->representation().IsSmi());

    if (instr->value()->representation().IsDouble()) {
      LOperand* object = UseRegisterAtStart(instr->elements());
      LOperand* val = NULL;
      val = UseRegisterAtStart(instr->value());
      LOperand* key = UseRegisterOrConstantAtStart(instr->key());
      return new(zone()) LStoreKeyed(object, key, val);
    } else {
      ASSERT(instr->value()->representation().IsSmiOrTagged());
      bool needs_write_barrier = instr->NeedsWriteBarrier();

      LOperand* obj = UseRegister(instr->elements());
      LOperand* val;
      LOperand* key;
      if (needs_write_barrier) {
        val = UseTempRegister(instr->value());
        key = UseTempRegister(instr->key());
      } else {
        val = UseRegisterOrConstantAtStart(instr->value());
        key = UseRegisterOrConstantAtStart(instr->key());
      }
      return new(zone()) LStoreKeyed(obj, key, val);
    }
  }

  ElementsKind elements_kind = instr->elements_kind();
  ASSERT(
      (instr->value()->representation().IsInteger32() &&
       (elements_kind != EXTERNAL_FLOAT_ELEMENTS) &&
       (elements_kind != EXTERNAL_DOUBLE_ELEMENTS)) ||
      (instr->value()->representation().IsDouble() &&
       ((elements_kind == EXTERNAL_FLOAT_ELEMENTS) ||
        (elements_kind == EXTERNAL_DOUBLE_ELEMENTS))));
  ASSERT(instr->elements()->representation().IsExternal());

  LOperand* external_pointer = UseRegister(instr->elements());
  LOperand* val = GetStoreKeyedValueOperand(instr);
  bool clobbers_key = ExternalArrayOpRequiresTemp(
      instr->key()->representation(), elements_kind);
  LOperand* key = clobbers_key
      ? UseTempRegister(instr->key())
      : UseRegisterOrConstantAtStart(instr->key());
  return new(zone()) LStoreKeyed(external_pointer,
                                 key,
                                 val);
}


LInstruction* LChunkBuilder::DoStoreKeyedGeneric(HStoreKeyedGeneric* instr) {
  LOperand* context = UseFixed(instr->context(), esi);
  LOperand* object = UseFixed(instr->object(), edx);
  LOperand* key = UseFixed(instr->key(), ecx);
  LOperand* value = UseFixed(instr->value(), eax);

  ASSERT(instr->object()->representation().IsTagged());
  ASSERT(instr->key()->representation().IsTagged());
  ASSERT(instr->value()->representation().IsTagged());

  LStoreKeyedGeneric* result =
      new(zone()) LStoreKeyedGeneric(context, object, key, value);
  return MarkAsCall(result, instr);
}


LInstruction* LChunkBuilder::DoTransitionElementsKind(
    HTransitionElementsKind* instr) {
  LOperand* object = UseRegister(instr->object());
  if (IsSimpleMapChangeTransition(instr->from_kind(), instr->to_kind())) {
    LOperand* object = UseRegister(instr->object());
    LOperand* new_map_reg = TempRegister();
    LOperand* temp_reg = TempRegister();
    LTransitionElementsKind* result =
        new(zone()) LTransitionElementsKind(object, NULL,
                                            new_map_reg, temp_reg);
    return result;
  } else {
    LOperand* context = UseFixed(instr->context(), esi);
    LTransitionElementsKind* result =
        new(zone()) LTransitionElementsKind(object, context, NULL, NULL);
    return AssignPointerMap(result);
  }
}


LInstruction* LChunkBuilder::DoTrapAllocationMemento(
    HTrapAllocationMemento* instr) {
  LOperand* object = UseRegister(instr->object());
  LOperand* temp = TempRegister();
  LTrapAllocationMemento* result =
      new(zone()) LTrapAllocationMemento(object, temp);
  return AssignEnvironment(result);
}


LInstruction* LChunkBuilder::DoStoreNamedField(HStoreNamedField* instr) {
  bool is_in_object = instr->access().IsInobject();
  bool is_external_location = instr->access().IsExternalMemory() &&
      instr->access().offset() == 0;
  bool needs_write_barrier = instr->NeedsWriteBarrier();
  bool needs_write_barrier_for_map = instr->has_transition() &&
      instr->NeedsWriteBarrierForMap();

  LOperand* obj;
  if (needs_write_barrier) {
    obj = is_in_object
        ? UseRegister(instr->object())
        : UseTempRegister(instr->object());
  } else if (is_external_location) {
    ASSERT(!is_in_object);
    ASSERT(!needs_write_barrier);
    ASSERT(!needs_write_barrier_for_map);
    obj = UseRegisterOrConstant(instr->object());
  } else {
    obj = needs_write_barrier_for_map
        ? UseRegister(instr->object())
        : UseRegisterAtStart(instr->object());
  }

  bool can_be_constant = instr->value()->IsConstant() &&
      HConstant::cast(instr->value())->NotInNewSpace() &&
      !(FLAG_track_double_fields && instr->field_representation().IsDouble());

  LOperand* val;
  if (instr->field_representation().IsInteger8() ||
      instr->field_representation().IsUInteger8()) {
    // mov_b requires a byte register (i.e. any of eax, ebx, ecx, edx).
    // Just force the value to be in eax and we're safe here.
    val = UseFixed(instr->value(), eax);
  } else if (needs_write_barrier) {
    val = UseTempRegister(instr->value());
  } else if (can_be_constant) {
    val = UseRegisterOrConstant(instr->value());
  } else if (FLAG_track_fields && instr->field_representation().IsSmi()) {
    val = UseTempRegister(instr->value());
  } else if (FLAG_track_double_fields &&
             instr->field_representation().IsDouble()) {
    val = UseRegisterAtStart(instr->value());
  } else {
    val = UseRegister(instr->value());
  }

  // We only need a scratch register if we have a write barrier or we
  // have a store into the properties array (not in-object-property).
  LOperand* temp = (!is_in_object || needs_write_barrier ||
                    needs_write_barrier_for_map) ? TempRegister() : NULL;

  // We need a temporary register for write barrier of the map field.
  LOperand* temp_map = needs_write_barrier_for_map ? TempRegister() : NULL;

  LStoreNamedField* result =
      new(zone()) LStoreNamedField(obj, val, temp, temp_map);
  if (FLAG_track_heap_object_fields &&
      instr->field_representation().IsHeapObject()) {
    if (!instr->value()->type().IsHeapObject()) {
      return AssignEnvironment(result);
    }
  }
  return result;
}


LInstruction* LChunkBuilder::DoStoreNamedGeneric(HStoreNamedGeneric* instr) {
  LOperand* context = UseFixed(instr->context(), esi);
  LOperand* object = UseFixed(instr->object(), edx);
  LOperand* value = UseFixed(instr->value(), eax);

  LStoreNamedGeneric* result =
      new(zone()) LStoreNamedGeneric(context, object, value);
  return MarkAsCall(result, instr);
}


LInstruction* LChunkBuilder::DoStringAdd(HStringAdd* instr) {
  LOperand* context = UseFixed(instr->context(), esi);
  LOperand* left = FLAG_new_string_add
      ? UseFixed(instr->left(), edx)
      : UseOrConstantAtStart(instr->left());
  LOperand* right = FLAG_new_string_add
      ? UseFixed(instr->right(), eax)
      : UseOrConstantAtStart(instr->right());
  LStringAdd* string_add = new(zone()) LStringAdd(context, left, right);
  return MarkAsCall(DefineFixed(string_add, eax), instr);
}


LInstruction* LChunkBuilder::DoStringCharCodeAt(HStringCharCodeAt* instr) {
  LOperand* string = UseTempRegister(instr->string());
  LOperand* index = UseTempRegister(instr->index());
  LOperand* context = UseAny(instr->context());
  LStringCharCodeAt* result =
      new(zone()) LStringCharCodeAt(context, string, index);
  return AssignEnvironment(AssignPointerMap(DefineAsRegister(result)));
}


LInstruction* LChunkBuilder::DoStringCharFromCode(HStringCharFromCode* instr) {
  LOperand* char_code = UseRegister(instr->value());
  LOperand* context = UseAny(instr->context());
  LStringCharFromCode* result =
      new(zone()) LStringCharFromCode(context, char_code);
  return AssignPointerMap(DefineAsRegister(result));
}


LInstruction* LChunkBuilder::DoAllocate(HAllocate* instr) {
  info()->MarkAsDeferredCalling();
  LOperand* context = UseAny(instr->context());
  LOperand* size = instr->size()->IsConstant()
      ? UseConstant(instr->size())
      : UseTempRegister(instr->size());
  LOperand* temp = TempRegister();
  LAllocate* result = new(zone()) LAllocate(context, size, temp);
  return AssignPointerMap(DefineAsRegister(result));
}


LInstruction* LChunkBuilder::DoRegExpLiteral(HRegExpLiteral* instr) {
  LOperand* context = UseFixed(instr->context(), esi);
  return MarkAsCall(
      DefineFixed(new(zone()) LRegExpLiteral(context), eax), instr);
}


LInstruction* LChunkBuilder::DoFunctionLiteral(HFunctionLiteral* instr) {
  LOperand* context = UseFixed(instr->context(), esi);
  return MarkAsCall(
      DefineFixed(new(zone()) LFunctionLiteral(context), eax), instr);
}


LInstruction* LChunkBuilder::DoOsrEntry(HOsrEntry* instr) {
  ASSERT(argument_count_ == 0);
  allocator_->MarkAsOsrEntry();
  current_block_->last_environment()->set_ast_id(instr->ast_id());
  return AssignEnvironment(new(zone()) LOsrEntry);
}


LInstruction* LChunkBuilder::DoParameter(HParameter* instr) {
  LParameter* result = new(zone()) LParameter;
  if (instr->kind() == HParameter::STACK_PARAMETER) {
    int spill_index = chunk()->GetParameterStackSlot(instr->index());
    return DefineAsSpilled(result, spill_index);
  } else {
    ASSERT(info()->IsStub());
    CodeStubInterfaceDescriptor* descriptor =
        info()->code_stub()->GetInterfaceDescriptor(info()->isolate());
    int index = static_cast<int>(instr->index());
    Register reg = descriptor->GetParameterRegister(index);
    return DefineFixed(result, reg);
  }
}


LInstruction* LChunkBuilder::DoUnknownOSRValue(HUnknownOSRValue* instr) {
  // Use an index that corresponds to the location in the unoptimized frame,
  // which the optimized frame will subsume.
  int env_index = instr->index();
  int spill_index = 0;
  if (instr->environment()->is_parameter_index(env_index)) {
    spill_index = chunk()->GetParameterStackSlot(env_index);
  } else {
    spill_index = env_index - instr->environment()->first_local_index();
    if (spill_index > LUnallocated::kMaxFixedSlotIndex) {
      Abort(kNotEnoughSpillSlotsForOsr);
      spill_index = 0;
    }
    if (spill_index == 0) {
      // The dynamic frame alignment state overwrites the first local.
      // The first local is saved at the end of the unoptimized frame.
      spill_index = graph()->osr()->UnoptimizedFrameSlots();
    }
  }
  return DefineAsSpilled(new(zone()) LUnknownOSRValue, spill_index);
}


LInstruction* LChunkBuilder::DoCallStub(HCallStub* instr) {
  LOperand* context = UseFixed(instr->context(), esi);
  LCallStub* result = new(zone()) LCallStub(context);
  return MarkAsCall(DefineFixed(result, eax), instr);
}


LInstruction* LChunkBuilder::DoArgumentsObject(HArgumentsObject* instr) {
  // There are no real uses of the arguments object.
  // arguments.length and element access are supported directly on
  // stack arguments, and any real arguments object use causes a bailout.
  // So this value is never used.
  return NULL;
}


LInstruction* LChunkBuilder::DoCapturedObject(HCapturedObject* instr) {
  instr->ReplayEnvironment(current_block_->last_environment());

  // There are no real uses of a captured object.
  return NULL;
}


LInstruction* LChunkBuilder::DoAccessArgumentsAt(HAccessArgumentsAt* instr) {
  info()->MarkAsRequiresFrame();
  LOperand* args = UseRegister(instr->arguments());
  LOperand* length;
  LOperand* index;
  if (instr->length()->IsConstant() && instr->index()->IsConstant()) {
    length = UseRegisterOrConstant(instr->length());
    index = UseOrConstant(instr->index());
  } else {
    length = UseTempRegister(instr->length());
    index = Use(instr->index());
  }
  return DefineAsRegister(new(zone()) LAccessArgumentsAt(args, length, index));
}


LInstruction* LChunkBuilder::DoToFastProperties(HToFastProperties* instr) {
  LOperand* object = UseFixed(instr->value(), eax);
  LToFastProperties* result = new(zone()) LToFastProperties(object);
  return MarkAsCall(DefineFixed(result, eax), instr);
}


LInstruction* LChunkBuilder::DoTypeof(HTypeof* instr) {
  LOperand* context = UseFixed(instr->context(), esi);
  LOperand* value = UseAtStart(instr->value());
  LTypeof* result = new(zone()) LTypeof(context, value);
  return MarkAsCall(DefineFixed(result, eax), instr);
}


LInstruction* LChunkBuilder::DoTypeofIsAndBranch(HTypeofIsAndBranch* instr) {
  LInstruction* goto_instr = CheckElideControlInstruction(instr);
  if (goto_instr != NULL) return goto_instr;
  return new(zone()) LTypeofIsAndBranch(UseTempRegister(instr->value()));
}


LInstruction* LChunkBuilder::DoIsConstructCallAndBranch(
    HIsConstructCallAndBranch* instr) {
  return new(zone()) LIsConstructCallAndBranch(TempRegister());
}


LInstruction* LChunkBuilder::DoSimulate(HSimulate* instr) {
  instr->ReplayEnvironment(current_block_->last_environment());

  // If there is an instruction pending deoptimization environment create a
  // lazy bailout instruction to capture the environment.
  if (!pending_deoptimization_ast_id_.IsNone()) {
    ASSERT(pending_deoptimization_ast_id_ == instr->ast_id());
    LLazyBailout* lazy_bailout = new(zone()) LLazyBailout;
    LInstruction* result = AssignEnvironment(lazy_bailout);
    // Store the lazy deopt environment with the instruction if needed. Right
    // now it is only used for LInstanceOfKnownGlobal.
    instruction_pending_deoptimization_environment_->
        SetDeferredLazyDeoptimizationEnvironment(result->environment());
    instruction_pending_deoptimization_environment_ = NULL;
    pending_deoptimization_ast_id_ = BailoutId::None();
    return result;
  }

  return NULL;
}


LInstruction* LChunkBuilder::DoStackCheck(HStackCheck* instr) {
  info()->MarkAsDeferredCalling();
  if (instr->is_function_entry()) {
    LOperand* context = UseFixed(instr->context(), esi);
    return MarkAsCall(new(zone()) LStackCheck(context), instr);
  } else {
    ASSERT(instr->is_backwards_branch());
    LOperand* context = UseAny(instr->context());
    return AssignEnvironment(
        AssignPointerMap(new(zone()) LStackCheck(context)));
  }
}


LInstruction* LChunkBuilder::DoEnterInlined(HEnterInlined* instr) {
  HEnvironment* outer = current_block_->last_environment();
  HConstant* undefined = graph()->GetConstantUndefined();
  HEnvironment* inner = outer->CopyForInlining(instr->closure(),
                                               instr->arguments_count(),
                                               instr->function(),
                                               undefined,
                                               instr->inlining_kind());
  // Only replay binding of arguments object if it wasn't removed from graph.
  if (instr->arguments_var() != NULL && instr->arguments_object()->IsLinked()) {
    inner->Bind(instr->arguments_var(), instr->arguments_object());
  }
  inner->set_entry(instr);
  current_block_->UpdateEnvironment(inner);
  chunk_->AddInlinedClosure(instr->closure());
  return NULL;
}


LInstruction* LChunkBuilder::DoLeaveInlined(HLeaveInlined* instr) {
  LInstruction* pop = NULL;

  HEnvironment* env = current_block_->last_environment();

  if (env->entry()->arguments_pushed()) {
    int argument_count = env->arguments_environment()->parameter_count();
    pop = new(zone()) LDrop(argument_count);
    ASSERT(instr->argument_delta() == -argument_count);
  }

  HEnvironment* outer = current_block_->last_environment()->
      DiscardInlined(false);
  current_block_->UpdateEnvironment(outer);
  return pop;
}


LInstruction* LChunkBuilder::DoForInPrepareMap(HForInPrepareMap* instr) {
  LOperand* context = UseFixed(instr->context(), esi);
  LOperand* object = UseFixed(instr->enumerable(), eax);
  LForInPrepareMap* result = new(zone()) LForInPrepareMap(context, object);
  return MarkAsCall(DefineFixed(result, eax), instr, CAN_DEOPTIMIZE_EAGERLY);
}


LInstruction* LChunkBuilder::DoForInCacheArray(HForInCacheArray* instr) {
  LOperand* map = UseRegister(instr->map());
  return AssignEnvironment(DefineAsRegister(
      new(zone()) LForInCacheArray(map)));
}


LInstruction* LChunkBuilder::DoCheckMapValue(HCheckMapValue* instr) {
  LOperand* value = UseRegisterAtStart(instr->value());
  LOperand* map = UseRegisterAtStart(instr->map());
  return AssignEnvironment(new(zone()) LCheckMapValue(value, map));
}


LInstruction* LChunkBuilder::DoLoadFieldByIndex(HLoadFieldByIndex* instr) {
  LOperand* object = UseRegister(instr->object());
  LOperand* index = UseTempRegister(instr->index());
  return DefineSameAsFirst(new(zone()) LLoadFieldByIndex(object, index));
}


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_IA32
