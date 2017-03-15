// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/parsing/preparsed-scope-data.h"

#include "src/ast/scopes.h"
#include "src/ast/variables.h"
#include "src/handles.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

namespace {

class VariableIsUsedField : public BitField16<bool, 0, 1> {};
class VariableMaybeAssignedField
    : public BitField16<bool, VariableIsUsedField::kNext, 1> {};
class VariableContextAllocatedField
    : public BitField16<bool, VariableMaybeAssignedField::kNext, 1> {};

const int kFunctionDataSize = 3;

}  // namespace

/*

  Internal data format for the backing store:

  ------------------------------------
  | scope type << only in debug      |
  | inner_scope_calls_eval_          |
  | data end index                   |
  | ----------------------           |
  | | data for variables |           |
  | | ...                |           |
  | ----------------------           |
  ------------------------------------
  ------------------------------------
  | data for inner scope_1           |
  | ...                              |
  ------------------------------------
  ...
  ------------------------------------
  | data for inner scope_n           |
  | ...                              |
  ------------------------------------
  << data end index points here
 */

void PreParsedScopeData::SaveData(Scope* scope) {
  DCHECK(!has_data_);

  if (scope->scope_type() == ScopeType::FUNCTION_SCOPE) {
    function_index_[scope->start_position()] =
        std::make_pair(scope->end_position(), backing_store_.size());
  }

  if (!ScopeNeedsData(scope)) {
    return;
  }

#ifdef DEBUG
  backing_store_.push_back(scope->scope_type());
#endif
  backing_store_.push_back(scope->inner_scope_calls_eval());
  // Reserve space for the data end index (which we don't know yet). The end
  // index is needed for skipping over data for a function scope when we skip
  // parsing of the corresponding function.
  size_t data_end_index = backing_store_.size();
  backing_store_.push_back(-1);

  if (!scope->is_hidden()) {
    for (Variable* var : *scope->locals()) {
      if (IsDeclaredVariableMode(var->mode())) {
        SaveDataForVariable(var);
      }
    }
  }

  SaveDataForInnerScopes(scope);

  backing_store_[data_end_index] = backing_store_.size();
}

void PreParsedScopeData::RestoreData(DeclarationScope* scope) const {
  int index = -1;

  DCHECK_EQ(scope->scope_type(), ScopeType::FUNCTION_SCOPE);

  bool success = FindFunctionData(scope->start_position(), &index);
  DCHECK(success);
  USE(success);

  RestoreData(scope, &index);
}

void PreParsedScopeData::RestoreData(Scope* scope, int* index_ptr) const {
  // It's possible that scope is not present in the data at all (since PreParser
  // doesn't create the corresponding scope). In this case, the Scope won't
  // contain any variables for which we need the data.
  if (!ScopeNeedsData(scope) && !IsSkippedFunctionScope(scope)) {
    return;
  }

  int& index = *index_ptr;

#ifdef DEBUG
  // Data integrity check.
  if (scope->scope_type() == ScopeType::FUNCTION_SCOPE) {
    int end_position_from_data = -1;
    FindFunctionEnd(scope->start_position(), &end_position_from_data);
    DCHECK_EQ(end_position_from_data, scope->end_position());
    int index_from_data = -1;
    FindFunctionData(scope->start_position(), &index_from_data);
    DCHECK_EQ(index_from_data, index);
  }
#endif

  if (IsSkippedFunctionScope(scope)) {
    // This scope is a function scope representing a function we want to
    // skip. So just skip over its data.
    DCHECK(!scope->must_use_preparsed_scope_data());
    index = backing_store_[index + 2];
    return;
  }

  DCHECK_EQ(backing_store_[index++], scope->scope_type());

  if (backing_store_[index++]) {
    scope->RecordEvalCall();
  }
  int data_end_index = backing_store_[index++];
  USE(data_end_index);

  if (!scope->is_hidden()) {
    for (Variable* var : *scope->locals()) {
      if (var->mode() == VAR || var->mode() == LET || var->mode() == CONST) {
        RestoreDataForVariable(var, index_ptr);
      }
    }
  }

  RestoreDataForInnerScopes(scope, index_ptr);

  DCHECK_EQ(data_end_index, index);
}

FixedUint32Array* PreParsedScopeData::Serialize(Isolate* isolate) const {
  // FIXME(marja): save space by using a byte array and converting
  // function_index_ to bytes.
  Handle<JSTypedArray> js_array = isolate->factory()->NewJSTypedArray(
      UINT32_ELEMENTS,
      function_index_.size() * kFunctionDataSize + backing_store_.size() + 1);
  FixedUint32Array* array = FixedUint32Array::cast(js_array->elements());

  array->set(0, static_cast<uint32_t>(function_index_.size()));
  int i = 1;
  for (const auto& item : function_index_) {
    array->set(i++, item.first);
    array->set(i++, item.second.first);
    array->set(i++, item.second.second);
  }

  for (size_t j = 0; j < backing_store_.size(); ++j) {
    array->set(i++, static_cast<uint32_t>(backing_store_[j]));
  }
  return array;
}

void PreParsedScopeData::Deserialize(Handle<FixedUint32Array> array) {
  has_data_ = true;
  DCHECK(!array.is_null());
  if (array->length() == 0) {
    return;
  }
  int function_count = array->get_scalar(0);
  CHECK(array->length() > function_count * kFunctionDataSize);
  if (function_count == 0) {
    return;
  }
  int i = 1;
  for (; i < function_count * kFunctionDataSize + 1; i += kFunctionDataSize) {
    function_index_[array->get_scalar(i)] =
        std::make_pair(array->get_scalar(i + 1), array->get_scalar(i + 2));
  }
  CHECK_EQ(function_index_.size(), function_count);

  backing_store_.reserve(array->length() - i);
  for (; i < array->length(); ++i) {
    backing_store_.push_back(array->get_scalar(i));
  }
}

bool PreParsedScopeData::FindFunctionEnd(int start_pos, int* end_pos) const {
  auto it = function_index_.find(start_pos);
  if (it == function_index_.end()) {
    return false;
  }
  *end_pos = it->second.first;
  return true;
}

void PreParsedScopeData::SaveDataForVariable(Variable* var) {
#ifdef DEBUG
  // Store the variable name in debug mode; this way we can check that we
  // restore data to the correct variable.
  const AstRawString* name = var->raw_name();
  backing_store_.push_back(name->length());
  for (int i = 0; i < name->length(); ++i) {
    backing_store_.push_back(name->raw_data()[i]);
  }
#endif
  // FIXME(marja): Only 3 bits needed, not a full byte.
  byte variable_data = VariableIsUsedField::encode(var->is_used()) |
                       VariableMaybeAssignedField::encode(
                           var->maybe_assigned() == kMaybeAssigned) |
                       VariableContextAllocatedField::encode(
                           var->has_forced_context_allocation());

  backing_store_.push_back(variable_data);
}

void PreParsedScopeData::RestoreDataForVariable(Variable* var,
                                                int* index_ptr) const {
  int& index = *index_ptr;
#ifdef DEBUG
  const AstRawString* name = var->raw_name();
  DCHECK_EQ(backing_store_[index++], name->length());
  for (int i = 0; i < name->length(); ++i) {
    DCHECK_EQ(backing_store_[index++], name->raw_data()[i]);
  }
#endif
  byte variable_data = backing_store_[index++];
  if (VariableIsUsedField::decode(variable_data)) {
    var->set_is_used();
  }
  if (VariableMaybeAssignedField::decode(variable_data)) {
    var->set_maybe_assigned();
  }
  if (VariableContextAllocatedField::decode(variable_data)) {
    var->ForceContextAllocation();
  }
}

void PreParsedScopeData::SaveDataForInnerScopes(Scope* scope) {
  // Inner scopes are stored in the reverse order, but we'd like to write the
  // data in the logical order. There might be many inner scopes, so we don't
  // want to recurse here.
  std::vector<Scope*> scopes;
  for (Scope* inner = scope->inner_scope(); inner != nullptr;
       inner = inner->sibling()) {
    scopes.push_back(inner);
  }
  for (int i = static_cast<int>(scopes.size()) - 1; i >= 0; --i) {
    SaveData(scopes[i]);
  }
}

void PreParsedScopeData::RestoreDataForInnerScopes(Scope* scope,
                                                   int* index_ptr) const {
  std::vector<Scope*> scopes;
  for (Scope* inner = scope->inner_scope(); inner != nullptr;
       inner = inner->sibling()) {
    scopes.push_back(inner);
  }
  for (int i = static_cast<int>(scopes.size()) - 1; i >= 0; --i) {
    RestoreData(scopes[i], index_ptr);
  }
}

bool PreParsedScopeData::FindFunctionData(int start_pos, int* index) const {
  auto it = function_index_.find(start_pos);
  if (it == function_index_.end()) {
    return false;
  }
  *index = it->second.second;
  return true;
}

bool PreParsedScopeData::ScopeNeedsData(Scope* scope) {
  if (scope->scope_type() == ScopeType::FUNCTION_SCOPE) {
    return true;
  }
  if (!scope->is_hidden()) {
    for (Variable* var : *scope->locals()) {
      if (var->mode() == VAR || var->mode() == LET || var->mode() == CONST) {
        return true;
      }
    }
  }
  for (Scope* inner = scope->inner_scope(); inner != nullptr;
       inner = inner->sibling()) {
    if (ScopeNeedsData(inner)) {
      return true;
    }
  }
  return false;
}

bool PreParsedScopeData::IsSkippedFunctionScope(Scope* scope) {
  return scope->is_declaration_scope() &&
         scope->AsDeclarationScope()->is_skipped_function();
}

}  // namespace internal
}  // namespace v8
