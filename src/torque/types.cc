// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fstream>
#include <iostream>

#include "src/torque/declarable.h"
#include "src/torque/types.h"

namespace v8 {
namespace internal {
namespace torque {

bool Type::IsSubtypeOf(const Type* supertype) const {
  const Type* subtype = this;
  while (subtype != nullptr) {
    if (subtype == supertype) return true;
    subtype = subtype->parent();
  }
  return false;
}

bool Type::IsAbstractName(const std::string& name) const {
  if (!IsAbstractType()) return false;
  return AbstractType::cast(this)->name() == name;
}

std::string AbstractType::GetGeneratedTNodeTypeName() const {
  std::string result = GetGeneratedTypeName();
  DCHECK_EQ(result.substr(0, 6), "TNode<");
  result = result.substr(6, result.length() - 7);
  return result;
}

std::string FunctionPointerType::ToString() const {
  std::stringstream result;
  result << "builtin (";
  bool first = true;
  for (const Type* t : parameter_types_) {
    if (!first) {
      result << ", ";
      first = false;
    }
    result << t;
  }
  result << ") => " << return_type_;
  return result.str();
}

std::string FunctionPointerType::MangledName() const {
  std::stringstream result;
  result << "FT";
  bool first = true;
  for (const Type* t : parameter_types_) {
    if (!first) {
      result << ", ";
      first = false;
    }
    std::string arg_type_string = t->MangledName();
    result << arg_type_string.size() << arg_type_string;
  }
  std::string return_type_string = return_type_->MangledName();
  result << return_type_string.size() << return_type_string;
  return result.str();
}

std::ostream& operator<<(std::ostream& os, const Signature& sig) {
  os << "(";
  for (size_t i = 0; i < sig.parameter_names.size(); ++i) {
    if (i > 0) os << ", ";
    if (!sig.parameter_names.empty()) os << sig.parameter_names[i] << ": ";
    os << sig.parameter_types.types[i];
  }
  if (sig.parameter_types.var_args) {
    if (sig.parameter_names.size()) os << ", ";
    os << "...";
  }
  os << ")";
  if (!sig.return_type->IsVoid()) {
    os << ": " << sig.return_type;
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, const TypeVector& types) {
  for (size_t i = 0; i < types.size(); ++i) {
    if (i > 0) os << ", ";
    os << types[i];
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, const ParameterTypes& p) {
  for (size_t i = 0; i < p.types.size(); ++i) {
    if (i > 0) os << ", ";
    os << p.types[i];
  }
  if (p.var_args) {
    if (p.types.size() > 0) os << ", ";
    os << "...";
  }
  return os;
}

bool Signature::HasSameTypesAs(const Signature& other) const {
  if (!(parameter_types.types == other.parameter_types.types &&
        parameter_types.var_args == other.parameter_types.var_args &&
        return_type == other.return_type)) {
    return false;
  }
  if (labels.size() != other.labels.size()) {
    return false;
  }
  size_t i = 0;
  for (auto l : labels) {
    if (l.types != other.labels[i++].types) {
      return false;
    }
  }
  return true;
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
