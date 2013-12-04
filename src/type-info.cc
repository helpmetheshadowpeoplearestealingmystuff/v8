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

#include "ast.h"
#include "code-stubs.h"
#include "compiler.h"
#include "ic.h"
#include "macro-assembler.h"
#include "stub-cache.h"
#include "type-info.h"

#include "ic-inl.h"
#include "objects-inl.h"

namespace v8 {
namespace internal {


TypeInfo TypeInfo::FromValue(Handle<Object> value) {
  if (value->IsSmi()) {
    return TypeInfo::Smi();
  } else if (value->IsHeapNumber()) {
    return TypeInfo::IsInt32Double(HeapNumber::cast(*value)->value())
        ? TypeInfo::Integer32()
        : TypeInfo::Double();
  } else if (value->IsString()) {
    return TypeInfo::String();
  }
  return TypeInfo::Unknown();
}


TypeFeedbackOracle::TypeFeedbackOracle(Handle<Code> code,
                                       Handle<Context> native_context,
                                       Isolate* isolate,
                                       Zone* zone)
    : native_context_(native_context),
      isolate_(isolate),
      zone_(zone) {
  BuildDictionary(code);
  ASSERT(dictionary_->IsDictionary());
}


static uint32_t IdToKey(TypeFeedbackId ast_id) {
  return static_cast<uint32_t>(ast_id.ToInt());
}


Handle<Object> TypeFeedbackOracle::GetInfo(TypeFeedbackId ast_id) {
  int entry = dictionary_->FindEntry(IdToKey(ast_id));
  if (entry != UnseededNumberDictionary::kNotFound) {
    Object* value = dictionary_->ValueAt(entry);
    if (value->IsCell()) {
      Cell* cell = Cell::cast(value);
      return Handle<Object>(cell->value(), isolate_);
    } else {
      return Handle<Object>(value, isolate_);
    }
  }
  return Handle<Object>::cast(isolate_->factory()->undefined_value());
}


Handle<Cell> TypeFeedbackOracle::GetInfoCell(
    TypeFeedbackId ast_id) {
  int entry = dictionary_->FindEntry(IdToKey(ast_id));
  if (entry != UnseededNumberDictionary::kNotFound) {
    Cell* cell = Cell::cast(dictionary_->ValueAt(entry));
    return Handle<Cell>(cell, isolate_);
  }
  return Handle<Cell>::null();
}


bool TypeFeedbackOracle::LoadIsUninitialized(TypeFeedbackId id) {
  Handle<Object> map_or_code = GetInfo(id);
  if (map_or_code->IsMap()) return false;
  if (map_or_code->IsCode()) {
    Handle<Code> code = Handle<Code>::cast(map_or_code);
    return code->is_inline_cache_stub() && code->ic_state() == UNINITIALIZED;
  }
  return false;
}


bool TypeFeedbackOracle::LoadIsMonomorphicNormal(TypeFeedbackId id) {
  Handle<Object> map_or_code = GetInfo(id);
  if (map_or_code->IsMap()) return true;
  if (map_or_code->IsCode()) {
    Handle<Code> code = Handle<Code>::cast(map_or_code);
    bool preliminary_checks = code->is_keyed_load_stub() &&
        code->ic_state() == MONOMORPHIC &&
        Code::ExtractTypeFromFlags(code->flags()) == Code::NORMAL;
    if (!preliminary_checks) return false;
    Map* map = code->FindFirstMap();
    if (map == NULL) return false;
    map = map->CurrentMapForDeprecated();
    return map != NULL && !CanRetainOtherContext(map, *native_context_);
  }
  return false;
}


bool TypeFeedbackOracle::LoadIsPreMonomorphic(TypeFeedbackId id) {
  Handle<Object> map_or_code = GetInfo(id);
  if (map_or_code->IsCode()) {
    Handle<Code> code = Handle<Code>::cast(map_or_code);
    return code->is_inline_cache_stub() && code->ic_state() == PREMONOMORPHIC;
  }
  return false;
}


bool TypeFeedbackOracle::LoadIsPolymorphic(TypeFeedbackId id) {
  Handle<Object> map_or_code = GetInfo(id);
  if (map_or_code->IsCode()) {
    Handle<Code> code = Handle<Code>::cast(map_or_code);
    return code->is_keyed_load_stub() && code->ic_state() == POLYMORPHIC;
  }
  return false;
}


bool TypeFeedbackOracle::StoreIsUninitialized(TypeFeedbackId ast_id) {
  Handle<Object> map_or_code = GetInfo(ast_id);
  if (map_or_code->IsMap()) return false;
  if (!map_or_code->IsCode()) return false;
  Handle<Code> code = Handle<Code>::cast(map_or_code);
  return code->ic_state() == UNINITIALIZED;
}


bool TypeFeedbackOracle::StoreIsMonomorphicNormal(TypeFeedbackId ast_id) {
  Handle<Object> map_or_code = GetInfo(ast_id);
  if (map_or_code->IsMap()) return true;
  if (map_or_code->IsCode()) {
    Handle<Code> code = Handle<Code>::cast(map_or_code);
    bool preliminary_checks =
        code->is_keyed_store_stub() &&
        code->ic_state() == MONOMORPHIC &&
        Code::ExtractTypeFromFlags(code->flags()) == Code::NORMAL;
    if (!preliminary_checks) return false;
    Map* map = code->FindFirstMap();
    if (map == NULL) return false;
    map = map->CurrentMapForDeprecated();
    return map != NULL && !CanRetainOtherContext(map, *native_context_);
  }
  return false;
}


bool TypeFeedbackOracle::StoreIsPreMonomorphic(TypeFeedbackId ast_id) {
  Handle<Object> map_or_code = GetInfo(ast_id);
  if (map_or_code->IsCode()) {
    Handle<Code> code = Handle<Code>::cast(map_or_code);
    return code->ic_state() == PREMONOMORPHIC;
  }
  return false;
}


bool TypeFeedbackOracle::StoreIsKeyedPolymorphic(TypeFeedbackId ast_id) {
  Handle<Object> map_or_code = GetInfo(ast_id);
  if (map_or_code->IsCode()) {
    Handle<Code> code = Handle<Code>::cast(map_or_code);
    return code->is_keyed_store_stub() &&
        code->ic_state() == POLYMORPHIC;
  }
  return false;
}


bool TypeFeedbackOracle::CallIsMonomorphic(TypeFeedbackId id) {
  Handle<Object> value = GetInfo(id);
  return value->IsMap() || value->IsAllocationSite() || value->IsJSFunction() ||
      value->IsSmi() ||
      (value->IsCode() && Handle<Code>::cast(value)->ic_state() == MONOMORPHIC);
}


bool TypeFeedbackOracle::KeyedArrayCallIsHoley(TypeFeedbackId id) {
  Handle<Object> value = GetInfo(id);
  Handle<Code> code = Handle<Code>::cast(value);
  return KeyedArrayCallStub::IsHoley(code);
}


bool TypeFeedbackOracle::CallNewIsMonomorphic(TypeFeedbackId id) {
  Handle<Object> info = GetInfo(id);
  return info->IsAllocationSite() || info->IsJSFunction();
}


bool TypeFeedbackOracle::ObjectLiteralStoreIsMonomorphic(TypeFeedbackId id) {
  Handle<Object> map_or_code = GetInfo(id);
  return map_or_code->IsMap();
}


byte TypeFeedbackOracle::ForInType(TypeFeedbackId id) {
  Handle<Object> value = GetInfo(id);
  return value->IsSmi() &&
      Smi::cast(*value)->value() == TypeFeedbackCells::kForInFastCaseMarker
          ? ForInStatement::FAST_FOR_IN : ForInStatement::SLOW_FOR_IN;
}


Handle<Map> TypeFeedbackOracle::LoadMonomorphicReceiverType(TypeFeedbackId id) {
  ASSERT(LoadIsMonomorphicNormal(id));
  Handle<Object> map_or_code = GetInfo(id);
  if (map_or_code->IsCode()) {
    Handle<Code> code = Handle<Code>::cast(map_or_code);
    Map* map = code->FindFirstMap()->CurrentMapForDeprecated();
    return map == NULL || CanRetainOtherContext(map, *native_context_)
        ? Handle<Map>::null()
        : Handle<Map>(map);
  }
  return Handle<Map>::cast(map_or_code);
}


Handle<Map> TypeFeedbackOracle::StoreMonomorphicReceiverType(
    TypeFeedbackId ast_id) {
  ASSERT(StoreIsMonomorphicNormal(ast_id));
  Handle<Object> map_or_code = GetInfo(ast_id);
  if (map_or_code->IsCode()) {
    Handle<Code> code = Handle<Code>::cast(map_or_code);
    Map* map = code->FindFirstMap()->CurrentMapForDeprecated();
    return map == NULL || CanRetainOtherContext(map, *native_context_)
        ? Handle<Map>::null()
        : Handle<Map>(map);
  }
  return Handle<Map>::cast(map_or_code);
}


KeyedAccessStoreMode TypeFeedbackOracle::GetStoreMode(
    TypeFeedbackId ast_id) {
  Handle<Object> map_or_code = GetInfo(ast_id);
  if (map_or_code->IsCode()) {
    Handle<Code> code = Handle<Code>::cast(map_or_code);
    if (code->kind() == Code::KEYED_STORE_IC) {
      return KeyedStoreIC::GetKeyedAccessStoreMode(code->extra_ic_state());
    }
  }
  return STANDARD_STORE;
}


void TypeFeedbackOracle::LoadReceiverTypes(TypeFeedbackId id,
                                           Handle<String> name,
                                           SmallMapList* types) {
  Code::Flags flags = Code::ComputeFlags(
      Code::HANDLER, MONOMORPHIC, kNoExtraICState,
      Code::NORMAL, Code::LOAD_IC);
  CollectReceiverTypes(id, name, flags, types);
}


void TypeFeedbackOracle::StoreReceiverTypes(TypeFeedbackId id,
                                            Handle<String> name,
                                            SmallMapList* types) {
  Code::Flags flags = Code::ComputeFlags(
      Code::HANDLER, MONOMORPHIC, kNoExtraICState,
      Code::NORMAL, Code::STORE_IC);
  CollectReceiverTypes(id, name, flags, types);
}


void TypeFeedbackOracle::CallReceiverTypes(TypeFeedbackId id,
                                           Handle<String> name,
                                           int arity,
                                           CallKind call_kind,
                                           SmallMapList* types) {
  // Note: Currently we do not take string extra ic data into account
  // here.
  ContextualMode contextual_mode = call_kind == CALL_AS_FUNCTION
      ? CONTEXTUAL
      : NOT_CONTEXTUAL;
  ExtraICState extra_ic_state =
      CallIC::Contextual::encode(contextual_mode);

  Code::Flags flags = Code::ComputeMonomorphicFlags(
      Code::CALL_IC, extra_ic_state, OWN_MAP, Code::NORMAL, arity);
  CollectReceiverTypes(id, name, flags, types);
}


CheckType TypeFeedbackOracle::GetCallCheckType(TypeFeedbackId id) {
  Handle<Object> value = GetInfo(id);
  if (!value->IsSmi()) return RECEIVER_MAP_CHECK;
  CheckType check = static_cast<CheckType>(Smi::cast(*value)->value());
  ASSERT(check != RECEIVER_MAP_CHECK);
  return check;
}


Handle<JSFunction> TypeFeedbackOracle::GetCallTarget(TypeFeedbackId id) {
  Handle<Object> info = GetInfo(id);
  if (info->IsAllocationSite()) {
    return Handle<JSFunction>(isolate_->global_context()->array_function());
  } else {
    return Handle<JSFunction>::cast(info);
  }
}


Handle<JSFunction> TypeFeedbackOracle::GetCallNewTarget(TypeFeedbackId id) {
  Handle<Object> info = GetInfo(id);
  if (info->IsAllocationSite()) {
    return Handle<JSFunction>(isolate_->global_context()->array_function());
  } else {
    return Handle<JSFunction>::cast(info);
  }
}


Handle<Cell> TypeFeedbackOracle::GetCallNewAllocationInfoCell(
    TypeFeedbackId id) {
  return GetInfoCell(id);
}


Handle<Map> TypeFeedbackOracle::GetObjectLiteralStoreMap(TypeFeedbackId id) {
  ASSERT(ObjectLiteralStoreIsMonomorphic(id));
  return Handle<Map>::cast(GetInfo(id));
}


bool TypeFeedbackOracle::LoadIsBuiltin(
    TypeFeedbackId id, Builtins::Name builtin) {
  return *GetInfo(id) == isolate_->builtins()->builtin(builtin);
}


bool TypeFeedbackOracle::LoadIsStub(TypeFeedbackId id, ICStub* stub) {
  Handle<Object> object = GetInfo(id);
  if (!object->IsCode()) return false;
  Handle<Code> code = Handle<Code>::cast(object);
  if (!code->is_load_stub()) return false;
  if (code->ic_state() != MONOMORPHIC) return false;
  return stub->Describes(*code);
}


void TypeFeedbackOracle::CompareType(TypeFeedbackId id,
                                     Handle<Type>* left_type,
                                     Handle<Type>* right_type,
                                     Handle<Type>* combined_type) {
  Handle<Object> info = GetInfo(id);
  if (!info->IsCode()) {
    // For some comparisons we don't have ICs, e.g. LiteralCompareTypeof.
    *left_type = *right_type = *combined_type = handle(Type::None(), isolate_);
    return;
  }
  Handle<Code> code = Handle<Code>::cast(info);

  Handle<Map> map;
  Map* raw_map = code->FindFirstMap();
  if (raw_map != NULL) {
    raw_map = raw_map->CurrentMapForDeprecated();
    if (raw_map != NULL && !CanRetainOtherContext(raw_map, *native_context_)) {
      map = handle(raw_map, isolate_);
    }
  }

  if (code->is_compare_ic_stub()) {
    int stub_minor_key = code->stub_info();
    CompareIC::StubInfoToType(
        stub_minor_key, left_type, right_type, combined_type, map, isolate());
  } else if (code->is_compare_nil_ic_stub()) {
    CompareNilICStub stub(code->extended_extra_ic_state());
    *combined_type = stub.GetType(isolate_, map);
    *left_type = *right_type = stub.GetInputType(isolate_, map);
  }
}


void TypeFeedbackOracle::BinaryType(TypeFeedbackId id,
                                    Handle<Type>* left,
                                    Handle<Type>* right,
                                    Handle<Type>* result,
                                    Maybe<int>* fixed_right_arg,
                                    Token::Value op) {
  Handle<Object> object = GetInfo(id);
  if (!object->IsCode()) {
    // For some binary ops we don't have ICs, e.g. Token::COMMA, but for the
    // operations covered by the BinaryOpIC we should always have them.
    ASSERT(op < BinaryOpIC::State::FIRST_TOKEN ||
           op > BinaryOpIC::State::LAST_TOKEN);
    *left = *right = *result = handle(Type::None(), isolate_);
    *fixed_right_arg = Maybe<int>();
    return;
  }
  Handle<Code> code = Handle<Code>::cast(object);
  ASSERT_EQ(Code::BINARY_OP_IC, code->kind());
  BinaryOpIC::State state(code->extended_extra_ic_state());
  ASSERT_EQ(op, state.op());

  *left = state.GetLeftType(isolate());
  *right = state.GetRightType(isolate());
  *result = state.GetResultType(isolate());
  *fixed_right_arg = state.fixed_right_arg();
}


Handle<Type> TypeFeedbackOracle::ClauseType(TypeFeedbackId id) {
  Handle<Object> info = GetInfo(id);
  Handle<Type> result(Type::None(), isolate_);
  if (info->IsCode() && Handle<Code>::cast(info)->is_compare_ic_stub()) {
    Handle<Code> code = Handle<Code>::cast(info);
    CompareIC::State state = ICCompareStub::CompareState(code->stub_info());
    result = CompareIC::StateToType(isolate_, state);
  }
  return result;
}


Handle<Type> TypeFeedbackOracle::CountType(TypeFeedbackId id) {
  Handle<Object> object = GetInfo(id);
  if (!object->IsCode()) return handle(Type::None(), isolate_);
  Handle<Code> code = Handle<Code>::cast(object);
  ASSERT_EQ(Code::BINARY_OP_IC, code->kind());
  BinaryOpIC::State state(code->extended_extra_ic_state());
  return state.GetLeftType(isolate());
}


void TypeFeedbackOracle::PropertyReceiverTypes(
    TypeFeedbackId id, Handle<String> name,
    SmallMapList* receiver_types, bool* is_prototype) {
  receiver_types->Clear();
  FunctionPrototypeStub proto_stub(Code::LOAD_IC);
  *is_prototype = LoadIsStub(id, &proto_stub);
  if (!*is_prototype) {
    LoadReceiverTypes(id, name, receiver_types);
  }
}


void TypeFeedbackOracle::KeyedPropertyReceiverTypes(
    TypeFeedbackId id, SmallMapList* receiver_types, bool* is_string) {
  receiver_types->Clear();
  *is_string = false;
  if (LoadIsBuiltin(id, Builtins::kKeyedLoadIC_String)) {
    *is_string = true;
  } else if (LoadIsMonomorphicNormal(id)) {
    receiver_types->Add(LoadMonomorphicReceiverType(id), zone());
  } else if (LoadIsPolymorphic(id)) {
    receiver_types->Reserve(kMaxKeyedPolymorphism, zone());
    CollectKeyedReceiverTypes(id, receiver_types);
  }
}


void TypeFeedbackOracle::AssignmentReceiverTypes(
    TypeFeedbackId id, Handle<String> name, SmallMapList* receiver_types) {
  receiver_types->Clear();
  StoreReceiverTypes(id, name, receiver_types);
}


void TypeFeedbackOracle::KeyedAssignmentReceiverTypes(
    TypeFeedbackId id, SmallMapList* receiver_types,
    KeyedAccessStoreMode* store_mode) {
  receiver_types->Clear();
  if (StoreIsMonomorphicNormal(id)) {
    // Record receiver type for monomorphic keyed stores.
    receiver_types->Add(StoreMonomorphicReceiverType(id), zone());
  } else if (StoreIsKeyedPolymorphic(id)) {
    receiver_types->Reserve(kMaxKeyedPolymorphism, zone());
    CollectKeyedReceiverTypes(id, receiver_types);
  }
  *store_mode = GetStoreMode(id);
}


void TypeFeedbackOracle::CountReceiverTypes(
    TypeFeedbackId id, SmallMapList* receiver_types) {
  receiver_types->Clear();
  if (StoreIsMonomorphicNormal(id)) {
    // Record receiver type for monomorphic keyed stores.
    receiver_types->Add(StoreMonomorphicReceiverType(id), zone());
  } else if (StoreIsKeyedPolymorphic(id)) {
    receiver_types->Reserve(kMaxKeyedPolymorphism, zone());
    CollectKeyedReceiverTypes(id, receiver_types);
  } else {
    CollectPolymorphicStoreReceiverTypes(id, receiver_types);
  }
}


void TypeFeedbackOracle::CollectPolymorphicMaps(Handle<Code> code,
                                                SmallMapList* types) {
  MapHandleList maps;
  code->FindAllMaps(&maps);
  types->Reserve(maps.length(), zone());
  for (int i = 0; i < maps.length(); i++) {
    Handle<Map> map(maps.at(i));
    if (!CanRetainOtherContext(*map, *native_context_)) {
      types->AddMapIfMissing(map, zone());
    }
  }
}


void TypeFeedbackOracle::CollectReceiverTypes(TypeFeedbackId ast_id,
                                              Handle<String> name,
                                              Code::Flags flags,
                                              SmallMapList* types) {
  Handle<Object> object = GetInfo(ast_id);
  if (object->IsUndefined() || object->IsSmi()) return;

  if (object->IsMap()) {
    types->AddMapIfMissing(Handle<Map>::cast(object), zone());
  } else if (Handle<Code>::cast(object)->ic_state() == POLYMORPHIC ||
             Handle<Code>::cast(object)->ic_state() == MONOMORPHIC) {
    CollectPolymorphicMaps(Handle<Code>::cast(object), types);
  } else if (FLAG_collect_megamorphic_maps_from_stub_cache &&
      Handle<Code>::cast(object)->ic_state() == MEGAMORPHIC) {
    types->Reserve(4, zone());
    ASSERT(object->IsCode());
    isolate_->stub_cache()->CollectMatchingMaps(types,
                                                name,
                                                flags,
                                                native_context_,
                                                zone());
  }
}


// Check if a map originates from a given native context. We use this
// information to filter out maps from different context to avoid
// retaining objects from different tabs in Chrome via optimized code.
bool TypeFeedbackOracle::CanRetainOtherContext(Map* map,
                                               Context* native_context) {
  Object* constructor = NULL;
  while (!map->prototype()->IsNull()) {
    constructor = map->constructor();
    if (!constructor->IsNull()) {
      // If the constructor is not null or a JSFunction, we have to
      // conservatively assume that it may retain a native context.
      if (!constructor->IsJSFunction()) return true;
      // Check if the constructor directly references a foreign context.
      if (CanRetainOtherContext(JSFunction::cast(constructor),
                                native_context)) {
        return true;
      }
    }
    map = HeapObject::cast(map->prototype())->map();
  }
  constructor = map->constructor();
  if (constructor->IsNull()) return false;
  JSFunction* function = JSFunction::cast(constructor);
  return CanRetainOtherContext(function, native_context);
}


bool TypeFeedbackOracle::CanRetainOtherContext(JSFunction* function,
                                               Context* native_context) {
  return function->context()->global_object() != native_context->global_object()
      && function->context()->global_object() != native_context->builtins();
}


void TypeFeedbackOracle::CollectKeyedReceiverTypes(TypeFeedbackId ast_id,
                                                   SmallMapList* types) {
  Handle<Object> object = GetInfo(ast_id);
  if (!object->IsCode()) return;
  Handle<Code> code = Handle<Code>::cast(object);
  if (code->kind() == Code::KEYED_LOAD_IC ||
      code->kind() == Code::KEYED_STORE_IC) {
    CollectPolymorphicMaps(code, types);
  }
}


void TypeFeedbackOracle::CollectPolymorphicStoreReceiverTypes(
    TypeFeedbackId ast_id,
    SmallMapList* types) {
  Handle<Object> object = GetInfo(ast_id);
  if (!object->IsCode()) return;
  Handle<Code> code = Handle<Code>::cast(object);
  if (code->kind() == Code::STORE_IC && code->ic_state() == POLYMORPHIC) {
    CollectPolymorphicMaps(code, types);
  }
}


byte TypeFeedbackOracle::ToBooleanTypes(TypeFeedbackId id) {
  Handle<Object> object = GetInfo(id);
  return object->IsCode() ? Handle<Code>::cast(object)->to_boolean_state() : 0;
}


// Things are a bit tricky here: The iterator for the RelocInfos and the infos
// themselves are not GC-safe, so we first get all infos, then we create the
// dictionary (possibly triggering GC), and finally we relocate the collected
// infos before we process them.
void TypeFeedbackOracle::BuildDictionary(Handle<Code> code) {
  DisallowHeapAllocation no_allocation;
  ZoneList<RelocInfo> infos(16, zone());
  HandleScope scope(isolate_);
  GetRelocInfos(code, &infos);
  CreateDictionary(code, &infos);
  ProcessRelocInfos(&infos);
  ProcessTypeFeedbackCells(code);
  // Allocate handle in the parent scope.
  dictionary_ = scope.CloseAndEscape(dictionary_);
}


void TypeFeedbackOracle::GetRelocInfos(Handle<Code> code,
                                       ZoneList<RelocInfo>* infos) {
  int mask = RelocInfo::ModeMask(RelocInfo::CODE_TARGET_WITH_ID);
  for (RelocIterator it(*code, mask); !it.done(); it.next()) {
    infos->Add(*it.rinfo(), zone());
  }
}


void TypeFeedbackOracle::CreateDictionary(Handle<Code> code,
                                          ZoneList<RelocInfo>* infos) {
  AllowHeapAllocation allocation_allowed;
  int cell_count = code->type_feedback_info()->IsTypeFeedbackInfo()
      ? TypeFeedbackInfo::cast(code->type_feedback_info())->
          type_feedback_cells()->CellCount()
      : 0;
  int length = infos->length() + cell_count;
  byte* old_start = code->instruction_start();
  dictionary_ = isolate()->factory()->NewUnseededNumberDictionary(length);
  byte* new_start = code->instruction_start();
  RelocateRelocInfos(infos, old_start, new_start);
}


void TypeFeedbackOracle::RelocateRelocInfos(ZoneList<RelocInfo>* infos,
                                            byte* old_start,
                                            byte* new_start) {
  for (int i = 0; i < infos->length(); i++) {
    RelocInfo* info = &(*infos)[i];
    info->set_pc(new_start + (info->pc() - old_start));
  }
}


void TypeFeedbackOracle::ProcessRelocInfos(ZoneList<RelocInfo>* infos) {
  for (int i = 0; i < infos->length(); i++) {
    RelocInfo reloc_entry = (*infos)[i];
    Address target_address = reloc_entry.target_address();
    TypeFeedbackId ast_id =
        TypeFeedbackId(static_cast<unsigned>((*infos)[i].data()));
    Code* target = Code::GetCodeFromTargetAddress(target_address);
    switch (target->kind()) {
      case Code::LOAD_IC:
      case Code::STORE_IC:
      case Code::CALL_IC:
        if (target->ic_state() == MONOMORPHIC) {
          if (target->kind() == Code::CALL_IC &&
              target->check_type() != RECEIVER_MAP_CHECK) {
            SetInfo(ast_id, Smi::FromInt(target->check_type()));
          } else {
            Object* map = target->FindFirstMap();
            if (map == NULL) {
              SetInfo(ast_id, static_cast<Object*>(target));
            } else if (!CanRetainOtherContext(Map::cast(map),
                                              *native_context_)) {
              Map* feedback = Map::cast(map)->CurrentMapForDeprecated();
              if (feedback != NULL) SetInfo(ast_id, feedback);
            }
          }
        } else {
          SetInfo(ast_id, target);
        }
        break;

      case Code::KEYED_CALL_IC:
      case Code::KEYED_LOAD_IC:
      case Code::KEYED_STORE_IC:
      case Code::BINARY_OP_IC:
      case Code::COMPARE_IC:
      case Code::TO_BOOLEAN_IC:
      case Code::COMPARE_NIL_IC:
        SetInfo(ast_id, target);
        break;

      default:
        break;
    }
  }
}


void TypeFeedbackOracle::ProcessTypeFeedbackCells(Handle<Code> code) {
  Object* raw_info = code->type_feedback_info();
  if (!raw_info->IsTypeFeedbackInfo()) return;
  Handle<TypeFeedbackCells> cache(
      TypeFeedbackInfo::cast(raw_info)->type_feedback_cells());
  for (int i = 0; i < cache->CellCount(); i++) {
    TypeFeedbackId ast_id = cache->AstId(i);
    Cell* cell = cache->GetCell(i);
    Object* value = cell->value();
    if (value->IsSmi() ||
        value->IsAllocationSite() ||
        (value->IsJSFunction() &&
         !CanRetainOtherContext(JSFunction::cast(value),
                                *native_context_))) {
      SetInfo(ast_id, cell);
    }
  }
}


void TypeFeedbackOracle::SetInfo(TypeFeedbackId ast_id, Object* target) {
  ASSERT(dictionary_->FindEntry(IdToKey(ast_id)) ==
         UnseededNumberDictionary::kNotFound);
  MaybeObject* maybe_result = dictionary_->AtNumberPut(IdToKey(ast_id), target);
  USE(maybe_result);
#ifdef DEBUG
  Object* result = NULL;
  // Dictionary has been allocated with sufficient size for all elements.
  ASSERT(maybe_result->ToObject(&result));
  ASSERT(*dictionary_ == result);
#endif
}


Representation Representation::FromType(TypeInfo info) {
  if (info.IsUninitialized()) return Representation::None();
  if (info.IsSmi()) return Representation::Smi();
  if (info.IsInteger32()) return Representation::Integer32();
  if (info.IsDouble()) return Representation::Double();
  if (info.IsNumber()) return Representation::Double();
  return Representation::Tagged();
}


} }  // namespace v8::internal
