// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_DECLARATIONS_H_
#define V8_TORQUE_DECLARATIONS_H_

#include <string>

#include "src/torque/declarable.h"
#include "src/torque/utils.h"

namespace v8 {
namespace internal {
namespace torque {

static constexpr const char* const kFromConstexprMacroName = "FromConstexpr";
static constexpr const char* kTrueLabelName = "_True";
static constexpr const char* kFalseLabelName = "_False";

template <class T>
std::vector<T*> FilterDeclarables(const std::vector<Declarable*> list) {
  std::vector<T*> result;
  for (Declarable* declarable : list) {
    if (T* t = T::DynamicCast(declarable)) {
      result.push_back(t);
    }
  }
  return result;
}

class Declarations {
 public:
  static std::vector<Declarable*> TryLookup(const std::string& name) {
    return CurrentScope::Get()->Lookup(name);
  }

  static std::vector<Declarable*> TryLookupShallow(const std::string& name) {
    return CurrentScope::Get()->LookupShallow(name);
  }

  template <class T>
  static std::vector<T*> TryLookup(const std::string& name) {
    return FilterDeclarables<T>(TryLookup(name));
  }

  static std::vector<Declarable*> Lookup(const std::string& name) {
    std::vector<Declarable*> d = TryLookup(name);
    if (d.empty()) {
      std::stringstream s;
      s << "cannot find \"" << name << "\"";
      ReportError(s.str());
    }
    return d;
  }

  static std::vector<Declarable*> LookupGlobalScope(const std::string& name);

  static const Type* LookupType(const std::string& name);
  static const Type* LookupGlobalType(const std::string& name);
  static const Type* GetType(TypeExpression* type_expression);

  static Builtin* FindSomeInternalBuiltinWithType(
      const FunctionPointerType* type);

  static Value* LookupValue(const std::string& name);

  static Macro* TryLookupMacro(const std::string& name,
                               const TypeVector& types);
  static base::Optional<Builtin*> TryLookupBuiltin(const std::string& name);

  static std::vector<Generic*> LookupGeneric(const std::string& name);
  static Generic* LookupUniqueGeneric(const std::string& name);

  static Module* DeclareModule(const std::string& name);

  static const AbstractType* DeclareAbstractType(
      const std::string& name, bool transient, const std::string& generated,
      base::Optional<const AbstractType*> non_constexpr_version,
      const base::Optional<std::string>& parent = {});

  static void DeclareType(const std::string& name, const Type* type,
                          bool redeclaration);

  static void DeclareStruct(const std::string& name,
                            const std::vector<NameAndType>& fields);

  static Macro* CreateMacro(const std::string& name, const Signature& signature,
                            bool transitioning,
                            base::Optional<Statement*> body);
  static Macro* DeclareMacro(const std::string& name,
                             const Signature& signature, bool transitioning,
                             base::Optional<Statement*> body,
                             base::Optional<std::string> op = {});

  static Builtin* CreateBuiltin(const std::string& name, Builtin::Kind kind,
                                const Signature& signature, bool transitioning,
                                base::Optional<Statement*> body);
  static Builtin* DeclareBuiltin(const std::string& name, Builtin::Kind kind,
                                 const Signature& signature, bool transitioning,
                                 base::Optional<Statement*> body);

  static RuntimeFunction* DeclareRuntimeFunction(const std::string& name,
                                                 const Signature& signature,
                                                 bool transitioning);

  static void DeclareExternConstant(const std::string& name, const Type* type,
                                    std::string value);
  static ModuleConstant* DeclareModuleConstant(const std::string& name,
                                               const Type* type,
                                               Expression* body);

  static Generic* DeclareGeneric(const std::string& name,
                                 GenericDeclaration* generic);

  template <class T>
  static T* Declare(const std::string& name, T* d) {
    CurrentScope::Get()->AddDeclarable(name, d);
    return d;
  }
  template <class T>
  static T* Declare(const std::string& name, std::unique_ptr<T> d) {
    return CurrentScope::Get()->AddDeclarable(name,
                                              RegisterDeclarable(std::move(d)));
  }

  static std::string GetGeneratedCallableName(
      const std::string& name, const TypeVector& specialized_types);
};

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_DECLARATIONS_H_
