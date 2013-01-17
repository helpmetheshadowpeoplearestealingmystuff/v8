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

#include "bootstrapper.h"
#include "code-stubs.h"
#include "stub-cache.h"
#include "factory.h"
#include "gdb-jit.h"
#include "macro-assembler.h"

namespace v8 {
namespace internal {

bool CodeStub::FindCodeInCache(Code** code_out, Isolate* isolate) {
  UnseededNumberDictionary* stubs = isolate->heap()->code_stubs();
  int index = stubs->FindEntry(GetKey());
  if (index != UnseededNumberDictionary::kNotFound) {
    *code_out = Code::cast(stubs->ValueAt(index));
    return true;
  }
  return false;
}


SmartArrayPointer<const char> CodeStub::GetName() {
  char buffer[100];
  NoAllocationStringAllocator allocator(buffer,
                                        static_cast<unsigned>(sizeof(buffer)));
  StringStream stream(&allocator);
  PrintName(&stream);
  return stream.ToCString();
}


void CodeStub::RecordCodeGeneration(Code* code, Isolate* isolate) {
  SmartArrayPointer<const char> name = GetName();
  PROFILE(isolate, CodeCreateEvent(Logger::STUB_TAG, code, *name));
  GDBJIT(AddCode(GDBJITInterface::STUB, *name, code));
  Counters* counters = isolate->counters();
  counters->total_stubs_code_size()->Increment(code->instruction_size());
}


int CodeStub::GetCodeKind() {
  return Code::STUB;
}


Handle<Code> PlatformCodeStub::GenerateCode() {
  Isolate* isolate = Isolate::Current();
  Factory* factory = isolate->factory();

  // Generate the new code.
  MacroAssembler masm(isolate, NULL, 256);

  {
    // Update the static counter each time a new code stub is generated.
    isolate->counters()->code_stubs()->Increment();

    // Nested stubs are not allowed for leaves.
    AllowStubCallsScope allow_scope(&masm, false);

    // Generate the code for the stub.
    masm.set_generating_stub(true);
    NoCurrentFrameScope scope(&masm);
    Generate(&masm);
  }

  // Create the code object.
  CodeDesc desc;
  masm.GetCode(&desc);

  // Copy the generated code into a heap object.
  Code::Flags flags = Code::ComputeFlags(
      static_cast<Code::Kind>(GetCodeKind()), GetICState());
  Handle<Code> new_object = factory->NewCode(
      desc, flags, masm.CodeObject(), NeedsImmovableCode());
  return new_object;
}


Handle<Code> CodeStub::GetCode() {
  Isolate* isolate = Isolate::Current();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();
  Code* code;
  if (UseSpecialCache()
      ? FindCodeInSpecialCache(&code, isolate)
      : FindCodeInCache(&code, isolate)) {
    ASSERT(IsPregenerated() == code->is_pregenerated());
    return Handle<Code>(code);
  }

  {
    HandleScope scope(isolate);

    Handle<Code> new_object = GenerateCode();
    new_object->set_major_key(MajorKey());
    FinishCode(new_object);
    RecordCodeGeneration(*new_object, isolate);

#ifdef ENABLE_DISASSEMBLER
    if (FLAG_print_code_stubs) {
      new_object->Disassemble(*GetName());
      PrintF("\n");
    }
#endif

    if (UseSpecialCache()) {
      AddToSpecialCache(new_object);
    } else {
      // Update the dictionary and the root in Heap.
      Handle<UnseededNumberDictionary> dict =
          factory->DictionaryAtNumberPut(
              Handle<UnseededNumberDictionary>(heap->code_stubs()),
              GetKey(),
              new_object);
      heap->public_set_code_stubs(*dict);
    }
    code = *new_object;
  }

  Activate(code);
  ASSERT(!NeedsImmovableCode() ||
         heap->lo_space()->Contains(code) ||
         heap->code_space()->FirstPage()->Contains(code->address()));
  return Handle<Code>(code, isolate);
}


const char* CodeStub::MajorName(CodeStub::Major major_key,
                                bool allow_unknown_keys) {
  switch (major_key) {
#define DEF_CASE(name) case name: return #name "Stub";
    CODE_STUB_LIST(DEF_CASE)
#undef DEF_CASE
    default:
      if (!allow_unknown_keys) {
        UNREACHABLE();
      }
      return NULL;
  }
}


void CodeStub::PrintName(StringStream* stream) {
  stream->Add("%s", MajorName(MajorKey(), false));
}


void BinaryOpStub::Generate(MacroAssembler* masm) {
  // Explicitly allow generation of nested stubs. It is safe here because
  // generation code does not use any raw pointers.
  AllowStubCallsScope allow_stub_calls(masm, true);

  BinaryOpIC::TypeInfo operands_type = Max(left_type_, right_type_);
  if (left_type_ == BinaryOpIC::ODDBALL && right_type_ == BinaryOpIC::ODDBALL) {
    // The OddballStub handles a number and an oddball, not two oddballs.
    operands_type = BinaryOpIC::GENERIC;
  }
  switch (operands_type) {
    case BinaryOpIC::UNINITIALIZED:
      GenerateTypeTransition(masm);
      break;
    case BinaryOpIC::SMI:
      GenerateSmiStub(masm);
      break;
    case BinaryOpIC::INT32:
      GenerateInt32Stub(masm);
      break;
    case BinaryOpIC::HEAP_NUMBER:
      GenerateHeapNumberStub(masm);
      break;
    case BinaryOpIC::ODDBALL:
      GenerateOddballStub(masm);
      break;
    case BinaryOpIC::STRING:
      GenerateStringStub(masm);
      break;
    case BinaryOpIC::GENERIC:
      GenerateGeneric(masm);
      break;
    default:
      UNREACHABLE();
  }
}


#define __ ACCESS_MASM(masm)


void BinaryOpStub::GenerateCallRuntime(MacroAssembler* masm) {
  switch (op_) {
    case Token::ADD:
      __ InvokeBuiltin(Builtins::ADD, JUMP_FUNCTION);
      break;
    case Token::SUB:
      __ InvokeBuiltin(Builtins::SUB, JUMP_FUNCTION);
      break;
    case Token::MUL:
      __ InvokeBuiltin(Builtins::MUL, JUMP_FUNCTION);
      break;
    case Token::DIV:
      __ InvokeBuiltin(Builtins::DIV, JUMP_FUNCTION);
      break;
    case Token::MOD:
      __ InvokeBuiltin(Builtins::MOD, JUMP_FUNCTION);
      break;
    case Token::BIT_OR:
      __ InvokeBuiltin(Builtins::BIT_OR, JUMP_FUNCTION);
      break;
    case Token::BIT_AND:
      __ InvokeBuiltin(Builtins::BIT_AND, JUMP_FUNCTION);
      break;
    case Token::BIT_XOR:
      __ InvokeBuiltin(Builtins::BIT_XOR, JUMP_FUNCTION);
      break;
    case Token::SAR:
      __ InvokeBuiltin(Builtins::SAR, JUMP_FUNCTION);
      break;
    case Token::SHR:
      __ InvokeBuiltin(Builtins::SHR, JUMP_FUNCTION);
      break;
    case Token::SHL:
      __ InvokeBuiltin(Builtins::SHL, JUMP_FUNCTION);
      break;
    default:
      UNREACHABLE();
  }
}


#undef __


void BinaryOpStub::PrintName(StringStream* stream) {
  const char* op_name = Token::Name(op_);
  const char* overwrite_name;
  switch (mode_) {
    case NO_OVERWRITE: overwrite_name = "Alloc"; break;
    case OVERWRITE_RIGHT: overwrite_name = "OverwriteRight"; break;
    case OVERWRITE_LEFT: overwrite_name = "OverwriteLeft"; break;
    default: overwrite_name = "UnknownOverwrite"; break;
  }
  stream->Add("BinaryOpStub_%s_%s_%s+%s",
              op_name,
              overwrite_name,
              BinaryOpIC::GetName(left_type_),
              BinaryOpIC::GetName(right_type_));
}


void BinaryOpStub::GenerateStringStub(MacroAssembler* masm) {
  ASSERT(left_type_ == BinaryOpIC::STRING || right_type_ == BinaryOpIC::STRING);
  ASSERT(op_ == Token::ADD);
  if (left_type_ == BinaryOpIC::STRING && right_type_ == BinaryOpIC::STRING) {
    GenerateBothStringStub(masm);
    return;
  }
  // Try to add arguments as strings, otherwise, transition to the generic
  // BinaryOpIC type.
  GenerateAddStrings(masm);
  GenerateTypeTransition(masm);
}


void ICCompareStub::AddToSpecialCache(Handle<Code> new_object) {
  ASSERT(*known_map_ != NULL);
  Isolate* isolate = new_object->GetIsolate();
  Factory* factory = isolate->factory();
  return Map::UpdateCodeCache(known_map_,
                              strict() ?
                                  factory->strict_compare_ic_symbol() :
                                  factory->compare_ic_symbol(),
                              new_object);
}


bool ICCompareStub::FindCodeInSpecialCache(Code** code_out, Isolate* isolate) {
  Factory* factory = isolate->factory();
  Code::Flags flags = Code::ComputeFlags(
      static_cast<Code::Kind>(GetCodeKind()),
      UNINITIALIZED);
  ASSERT(op_ == Token::EQ || op_ == Token::EQ_STRICT);
  Handle<Object> probe(
      known_map_->FindInCodeCache(
        strict() ?
            *factory->strict_compare_ic_symbol() :
            *factory->compare_ic_symbol(),
        flags));
  if (probe->IsCode()) {
    *code_out = Code::cast(*probe);
#ifdef DEBUG
    Token::Value cached_op;
    ICCompareStub::DecodeMinorKey((*code_out)->stub_info(), NULL, NULL, NULL,
                                  &cached_op);
    ASSERT(op_ == cached_op);
#endif
    return true;
  }
  return false;
}


int ICCompareStub::MinorKey() {
  return OpField::encode(op_ - Token::EQ) |
         LeftStateField::encode(left_) |
         RightStateField::encode(right_) |
         HandlerStateField::encode(state_);
}


void ICCompareStub::DecodeMinorKey(int minor_key,
                                   CompareIC::State* left_state,
                                   CompareIC::State* right_state,
                                   CompareIC::State* handler_state,
                                   Token::Value* op) {
  if (left_state) {
    *left_state =
        static_cast<CompareIC::State>(LeftStateField::decode(minor_key));
  }
  if (right_state) {
    *right_state =
        static_cast<CompareIC::State>(RightStateField::decode(minor_key));
  }
  if (handler_state) {
    *handler_state =
        static_cast<CompareIC::State>(HandlerStateField::decode(minor_key));
  }
  if (op) {
    *op = static_cast<Token::Value>(OpField::decode(minor_key) + Token::EQ);
  }
}


void ICCompareStub::Generate(MacroAssembler* masm) {
  switch (state_) {
    case CompareIC::UNINITIALIZED:
      GenerateMiss(masm);
      break;
    case CompareIC::SMI:
      GenerateSmis(masm);
      break;
    case CompareIC::HEAP_NUMBER:
      GenerateHeapNumbers(masm);
      break;
    case CompareIC::STRING:
      GenerateStrings(masm);
      break;
    case CompareIC::SYMBOL:
      GenerateSymbols(masm);
      break;
    case CompareIC::OBJECT:
      GenerateObjects(masm);
      break;
    case CompareIC::KNOWN_OBJECTS:
      ASSERT(*known_map_ != NULL);
      GenerateKnownObjects(masm);
      break;
    case CompareIC::GENERIC:
      GenerateGeneric(masm);
      break;
  }
}


void InstanceofStub::PrintName(StringStream* stream) {
  const char* args = "";
  if (HasArgsInRegisters()) {
    args = "_REGS";
  }

  const char* inline_check = "";
  if (HasCallSiteInlineCheck()) {
    inline_check = "_INLINE";
  }

  const char* return_true_false_object = "";
  if (ReturnTrueFalseObject()) {
    return_true_false_object = "_TRUEFALSE";
  }

  stream->Add("InstanceofStub%s%s%s",
              args,
              inline_check,
              return_true_false_object);
}


void JSEntryStub::FinishCode(Handle<Code> code) {
  Handle<FixedArray> handler_table =
      code->GetIsolate()->factory()->NewFixedArray(1, TENURED);
  handler_table->set(0, Smi::FromInt(handler_offset_));
  code->set_handler_table(*handler_table);
}


void KeyedLoadDictionaryElementStub::Generate(MacroAssembler* masm) {
  KeyedLoadStubCompiler::GenerateLoadDictionaryElement(masm);
}


void KeyedStoreElementStub::Generate(MacroAssembler* masm) {
  switch (elements_kind_) {
    case FAST_ELEMENTS:
    case FAST_HOLEY_ELEMENTS:
    case FAST_SMI_ELEMENTS:
    case FAST_HOLEY_SMI_ELEMENTS: {
      KeyedStoreStubCompiler::GenerateStoreFastElement(masm,
                                                       is_js_array_,
                                                       elements_kind_,
                                                       grow_mode_);
    }
      break;
    case FAST_DOUBLE_ELEMENTS:
    case FAST_HOLEY_DOUBLE_ELEMENTS:
      KeyedStoreStubCompiler::GenerateStoreFastDoubleElement(masm,
                                                             is_js_array_,
                                                             grow_mode_);
      break;
    case EXTERNAL_BYTE_ELEMENTS:
    case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
    case EXTERNAL_SHORT_ELEMENTS:
    case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
    case EXTERNAL_INT_ELEMENTS:
    case EXTERNAL_UNSIGNED_INT_ELEMENTS:
    case EXTERNAL_FLOAT_ELEMENTS:
    case EXTERNAL_DOUBLE_ELEMENTS:
    case EXTERNAL_PIXEL_ELEMENTS:
      KeyedStoreStubCompiler::GenerateStoreExternalArray(masm, elements_kind_);
      break;
    case DICTIONARY_ELEMENTS:
      KeyedStoreStubCompiler::GenerateStoreDictionaryElement(masm);
      break;
    case NON_STRICT_ARGUMENTS_ELEMENTS:
      UNREACHABLE();
      break;
  }
}


void ArgumentsAccessStub::PrintName(StringStream* stream) {
  stream->Add("ArgumentsAccessStub_");
  switch (type_) {
    case READ_ELEMENT: stream->Add("ReadElement"); break;
    case NEW_NON_STRICT_FAST: stream->Add("NewNonStrictFast"); break;
    case NEW_NON_STRICT_SLOW: stream->Add("NewNonStrictSlow"); break;
    case NEW_STRICT: stream->Add("NewStrict"); break;
  }
}


void CallFunctionStub::PrintName(StringStream* stream) {
  stream->Add("CallFunctionStub_Args%d", argc_);
  if (ReceiverMightBeImplicit()) stream->Add("_Implicit");
  if (RecordCallTarget()) stream->Add("_Recording");
}


void CallConstructStub::PrintName(StringStream* stream) {
  stream->Add("CallConstructStub");
  if (RecordCallTarget()) stream->Add("_Recording");
}


void ToBooleanStub::PrintName(StringStream* stream) {
  stream->Add("ToBooleanStub_");
  types_.Print(stream);
}


void ToBooleanStub::Types::Print(StringStream* stream) const {
  if (IsEmpty()) stream->Add("None");
  if (Contains(UNDEFINED)) stream->Add("Undefined");
  if (Contains(BOOLEAN)) stream->Add("Bool");
  if (Contains(NULL_TYPE)) stream->Add("Null");
  if (Contains(SMI)) stream->Add("Smi");
  if (Contains(SPEC_OBJECT)) stream->Add("SpecObject");
  if (Contains(STRING)) stream->Add("String");
  if (Contains(HEAP_NUMBER)) stream->Add("HeapNumber");
}


void ToBooleanStub::Types::TraceTransition(Types to) const {
  if (!FLAG_trace_ic) return;
  char buffer[100];
  NoAllocationStringAllocator allocator(buffer,
                                        static_cast<unsigned>(sizeof(buffer)));
  StringStream stream(&allocator);
  stream.Add("[ToBooleanIC (");
  Print(&stream);
  stream.Add("->");
  to.Print(&stream);
  stream.Add(")]\n");
  stream.OutputToStdOut();
}


bool ToBooleanStub::Types::Record(Handle<Object> object) {
  if (object->IsUndefined()) {
    Add(UNDEFINED);
    return false;
  } else if (object->IsBoolean()) {
    Add(BOOLEAN);
    return object->IsTrue();
  } else if (object->IsNull()) {
    Add(NULL_TYPE);
    return false;
  } else if (object->IsSmi()) {
    Add(SMI);
    return Smi::cast(*object)->value() != 0;
  } else if (object->IsSpecObject()) {
    Add(SPEC_OBJECT);
    return !object->IsUndetectableObject();
  } else if (object->IsString()) {
    Add(STRING);
    return !object->IsUndetectableObject() &&
        String::cast(*object)->length() != 0;
  } else if (object->IsHeapNumber()) {
    ASSERT(!object->IsUndetectableObject());
    Add(HEAP_NUMBER);
    double value = HeapNumber::cast(*object)->value();
    return value != 0 && !isnan(value);
  } else {
    // We should never see an internal object at runtime here!
    UNREACHABLE();
    return true;
  }
}


bool ToBooleanStub::Types::NeedsMap() const {
  return Contains(ToBooleanStub::SPEC_OBJECT)
      || Contains(ToBooleanStub::STRING)
      || Contains(ToBooleanStub::HEAP_NUMBER);
}


bool ToBooleanStub::Types::CanBeUndetectable() const {
  return Contains(ToBooleanStub::SPEC_OBJECT)
      || Contains(ToBooleanStub::STRING);
}


void ElementsTransitionAndStoreStub::Generate(MacroAssembler* masm) {
  Label fail;
  AllocationSiteMode mode = AllocationSiteInfo::GetMode(from_, to_);
  ASSERT(!IsFastHoleyElementsKind(from_) || IsFastHoleyElementsKind(to_));
  if (!FLAG_trace_elements_transitions) {
    if (IsFastSmiOrObjectElementsKind(to_)) {
      if (IsFastSmiOrObjectElementsKind(from_)) {
        ElementsTransitionGenerator::
            GenerateMapChangeElementsTransition(masm, mode, &fail);
      } else if (IsFastDoubleElementsKind(from_)) {
        ASSERT(!IsFastSmiElementsKind(to_));
        ElementsTransitionGenerator::GenerateDoubleToObject(masm, mode, &fail);
      } else {
        UNREACHABLE();
      }
      KeyedStoreStubCompiler::GenerateStoreFastElement(masm,
                                                       is_jsarray_,
                                                       to_,
                                                       grow_mode_);
    } else if (IsFastSmiElementsKind(from_) &&
               IsFastDoubleElementsKind(to_)) {
      ElementsTransitionGenerator::GenerateSmiToDouble(masm, mode, &fail);
      KeyedStoreStubCompiler::GenerateStoreFastDoubleElement(masm,
                                                             is_jsarray_,
                                                             grow_mode_);
    } else if (IsFastDoubleElementsKind(from_)) {
      ASSERT(to_ == FAST_HOLEY_DOUBLE_ELEMENTS);
      ElementsTransitionGenerator::
          GenerateMapChangeElementsTransition(masm, mode, &fail);
    } else {
      UNREACHABLE();
    }
  }
  masm->bind(&fail);
  KeyedStoreIC::GenerateRuntimeSetProperty(masm, strict_mode_);
}


FunctionEntryHook ProfileEntryHookStub::entry_hook_ = NULL;


void ProfileEntryHookStub::EntryHookTrampoline(intptr_t function,
                                               intptr_t stack_pointer) {
  if (entry_hook_ != NULL)
    entry_hook_(function, stack_pointer);
}


bool ProfileEntryHookStub::SetFunctionEntryHook(FunctionEntryHook entry_hook) {
  // We don't allow setting a new entry hook over one that's
  // already active, as the hooks won't stack.
  if (entry_hook != 0 && entry_hook_ != 0)
    return false;

  entry_hook_ = entry_hook;
  return true;
}


} }  // namespace v8::internal
