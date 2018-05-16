// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/torque/declarations.h"
#include "src/torque/declarable.h"

namespace v8 {
namespace internal {
namespace torque {

Scope* Declarations::GetNodeScope(const AstNode* node) {
  std::pair<const AstNode*, TypeVector> key(
      node, current_generic_specialization_ == nullptr
                ? TypeVector()
                : current_generic_specialization_->second);
  auto i = scopes_.find(key);
  if (i != scopes_.end()) return i->second;
  Scope* result = chain_.NewScope();
  scopes_[key] = result;
  return result;
}

Scope* Declarations::GetGenericScope(Generic* generic,
                                     const TypeVector& types) {
  std::pair<const AstNode*, TypeVector> key(generic->declaration()->callable,
                                            types);
  auto i = scopes_.find(key);
  if (i != scopes_.end()) return i->second;
  Scope* result = chain_.NewScope();
  scopes_[key] = result;
  return result;
}

void Declarations::CheckAlreadyDeclared(SourcePosition pos,
                                        const std::string& name,
                                        const char* new_type) {
  auto i = chain_.ShallowLookup(name);
  if (i != nullptr) {
    std::stringstream s;
    s << "cannot redeclare " << name << " (type " << new_type << ") at "
      << PositionAsString(pos) << std::endl;
    ReportError(s.str());
  }
}

const Type* Declarations::LookupType(SourcePosition pos,
                                     const std::string& name) {
  Declarable* raw = Lookup(pos, name);
  if (raw->IsType()) {
    return Type::cast(raw);
  } else if (raw->IsTypeAlias()) {
    return TypeAlias::cast(raw)->type();
  }
  std::stringstream s;
  s << "declaration \"" << name << "\" is not a Type at "
    << PositionAsString(pos);
  ReportError(s.str());
  return nullptr;
}

const Type* Declarations::LookupGlobalType(const std::string& name) {
  return Type::cast(LookupGlobalScope(name));
}

const Type* Declarations::LookupGlobalType(SourcePosition pos,
                                           const std::string& name) {
  Declarable* raw = LookupGlobalScope(pos, name);
  if (!raw->IsType()) {
    std::stringstream s;
    s << "declaration \"" << name << "\" is not a Type at "
      << PositionAsString(pos);
    ReportError(s.str());
  }
  return Type::cast(raw);
}

const Type* Declarations::GetFunctionPointerType(SourcePosition pos,
                                                 TypeVector argument_types,
                                                 const Type* return_type) {
  const Type* code_type = LookupGlobalType(pos, CODE_TYPE_STRING);
  return function_pointer_types_.Add(
      FunctionPointerType(code_type, argument_types, return_type));
}

const Type* Declarations::GetType(SourcePosition pos,
                                  TypeExpression* type_expression) {
  if (auto* basic = BasicTypeExpression::DynamicCast(type_expression)) {
    std::string name =
        (basic->is_constexpr ? CONSTEXPR_TYPE_PREFIX : "") + basic->name;
    return LookupType(pos, name);
  } else {
    auto* function_type_exp = FunctionTypeExpression::cast(type_expression);
    TypeVector argument_types;
    for (TypeExpression* type_exp : function_type_exp->parameters.types) {
      argument_types.push_back(GetType(pos, type_exp));
    }
    return GetFunctionPointerType(pos, argument_types,
                                  GetType(pos, function_type_exp->return_type));
  }
}

Builtin* Declarations::FindSomeInternalBuiltinWithType(
    const FunctionPointerType* type) {
  for (auto& declarable : declarables_) {
    if (Builtin* builtin = Builtin::DynamicCast(declarable.get())) {
      if (!builtin->IsExternal() && builtin->kind() == Builtin::kStub &&
          builtin->signature().return_type == type->return_type() &&
          builtin->signature().parameter_types.types ==
              type->parameter_types()) {
        return builtin;
      }
    }
  }
  return nullptr;
}

Value* Declarations::LookupValue(SourcePosition pos, const std::string& name) {
  Declarable* d = Lookup(pos, name);
  if (!d->IsValue()) {
    std::stringstream s;
    s << "declaration \"" << name << "\" is not a Value at "
      << PositionAsString(pos);
    ReportError(s.str());
  }
  return Value::cast(d);
}

Label* Declarations::LookupLabel(SourcePosition pos, const std::string& name) {
  Declarable* d = Lookup(pos, name);
  if (!d->IsLabel()) {
    std::stringstream s;
    s << "declaration \"" << name << "\" is not a Label at "
      << PositionAsString(pos);
    ReportError(s.str());
  }
  return Label::cast(d);
}

Macro* Declarations::LookupMacro(SourcePosition pos, const std::string& name,
                                 const TypeVector& types) {
  Declarable* declarable = Lookup(name);
  if (declarable != nullptr) {
    if (declarable->IsMacroList()) {
      for (auto& m : MacroList::cast(declarable)->list()) {
        if (m->signature().parameter_types.types == types &&
            !m->signature().parameter_types.var_args) {
          return m;
        }
      }
    }
  }
  std::stringstream stream;
  stream << "macro " << name << " with parameter types " << types
         << " referenced at " << PositionAsString(pos) << " is not defined";
  ReportError(stream.str());
  return nullptr;
}

Builtin* Declarations::LookupBuiltin(SourcePosition pos,
                                     const std::string& name) {
  Declarable* declarable = Lookup(name);
  if (declarable != nullptr) {
    if (declarable->IsBuiltin()) {
      return Builtin::cast(declarable);
    }
    ReportError(name + " referenced at " + PositionAsString(pos) +
                " is not a builtin");
  }
  ReportError(std::string("builtin ") + name + " referenced at " +
              PositionAsString(pos) + " is not defined");
  return nullptr;
}

Generic* Declarations::LookupGeneric(const SourcePosition& pos,
                                     const std::string& name) {
  Declarable* declarable = Lookup(name);
  if (declarable != nullptr) {
    if (declarable->IsGeneric()) {
      return Generic::cast(declarable);
    }
    ReportError(name + " referenced at " + PositionAsString(pos) +
                " is not a generic");
  }
  ReportError(std::string("generic ") + name + " referenced at " +
              PositionAsString(pos) + " is not defined");
  return nullptr;
}

const AbstractType* Declarations::DeclareAbstractType(
    SourcePosition pos, const std::string& name, const std::string& generated,
    const std::string* parent) {
  CheckAlreadyDeclared(pos, name, "type");
  const Type* parent_type = nullptr;
  if (parent != nullptr) {
    Declarable* maybe_parent_type = Lookup(*parent);
    if (maybe_parent_type == nullptr) {
      std::stringstream s;
      s << "cannot find parent type \"" << *parent << "\" at  "
        << PositionAsString(pos);
      ReportError(s.str());
    }
    if (!maybe_parent_type->IsType()) {
      std::stringstream s;
      s << "parent \"" << *parent << "\" of type \"" << name << "\""
        << " is not a type "
        << " at  " << PositionAsString(pos);
      ReportError(s.str());
    }
    parent_type = Type::cast(maybe_parent_type);
  }
  AbstractType* result = new AbstractType(parent_type, name, generated);
  Declare(name, std::unique_ptr<Declarable>(result));
  return result;
}

void Declarations::DeclareTypeAlias(SourcePosition pos, const std::string& name,
                                    const Type* aliased_type) {
  CheckAlreadyDeclared(pos, name, "aliased type");
  TypeAlias* result = new TypeAlias(name, aliased_type);
  Declare(name, std::unique_ptr<TypeAlias>(result));
}

Label* Declarations::DeclareLabel(SourcePosition pos, const std::string& name) {
  CheckAlreadyDeclared(pos, name, "label");
  Label* result = new Label(name);
  Declare(name, std::unique_ptr<Declarable>(result));
  return result;
}

Macro* Declarations::DeclareMacro(SourcePosition pos, const std::string& name,
                                  const Signature& signature) {
  auto previous = chain_.Lookup(name);
  MacroList* macro_list = nullptr;
  if (previous == nullptr) {
    macro_list = new MacroList();
    Declare(name, std::unique_ptr<Declarable>(macro_list));
  } else if (!previous->IsMacroList()) {
    std::stringstream s;
    s << "cannot redeclare non-macro " << name << " as a macro at "
      << PositionAsString(pos);
    ReportError(s.str());
  } else {
    macro_list = MacroList::cast(previous);
  }
  for (auto& macro : macro_list->list()) {
    if (signature.parameter_types.types ==
            macro->signature().parameter_types.types &&
        signature.parameter_types.var_args ==
            macro->signature().parameter_types.var_args) {
      std::stringstream s;
      s << "cannot redeclare " << name
        << " as a macro with identical parameter list "
        << signature.parameter_types << PositionAsString(pos);
      ReportError(s.str());
    }
  }
  return macro_list->AddMacro(new Macro(name, signature));
}

Builtin* Declarations::DeclareBuiltin(SourcePosition pos,
                                      const std::string& name,
                                      Builtin::Kind kind, bool external,
                                      const Signature& signature) {
  CheckAlreadyDeclared(pos, name, "builtin");
  Builtin* result = new Builtin(name, kind, external, signature);
  Declare(name, std::unique_ptr<Declarable>(result));
  return result;
}

RuntimeFunction* Declarations::DeclareRuntimeFunction(
    SourcePosition pos, const std::string& name, const Signature& signature) {
  CheckAlreadyDeclared(pos, name, "runtime function");
  RuntimeFunction* result = new RuntimeFunction(name, signature);
  Declare(name, std::unique_ptr<Declarable>(result));
  return result;
}

Variable* Declarations::DeclareVariable(SourcePosition pos,
                                        const std::string& var,
                                        const Type* type) {
  std::string name(var + std::to_string(GetNextUniqueDeclarationNumber()));
  CheckAlreadyDeclared(pos, var, "variable");
  Variable* result = new Variable(var, name, type);
  Declare(var, std::unique_ptr<Declarable>(result));
  return result;
}

Parameter* Declarations::DeclareParameter(SourcePosition pos,
                                          const std::string& name,
                                          const std::string& var_name,
                                          const Type* type) {
  CheckAlreadyDeclared(pos, name, "parameter");
  Parameter* result = new Parameter(name, type, var_name);
  Declare(name, std::unique_ptr<Declarable>(result));
  return result;
}

Label* Declarations::DeclarePrivateLabel(SourcePosition pos,
                                         const std::string& raw_name) {
  std::string name =
      raw_name + "_" + std::to_string(GetNextUniqueDeclarationNumber());
  CheckAlreadyDeclared(pos, name, "label");
  Label* result = new Label(name);
  Declare(name, std::unique_ptr<Declarable>(result));
  return result;
}

void Declarations::DeclareConstant(SourcePosition pos, const std::string& name,
                                   const Type* type, const std::string& value) {
  CheckAlreadyDeclared(pos, name, "constant, parameter or arguments");
  Constant* result = new Constant(name, type, value);
  Declare(name, std::unique_ptr<Declarable>(result));
}

Generic* Declarations::DeclareGeneric(SourcePosition pos,
                                      const std::string& name, Module* module,
                                      GenericDeclaration* generic) {
  CheckAlreadyDeclared(pos, name, "generic");
  Generic* result = new Generic(name, module, generic);
  Declare(name, std::unique_ptr<Generic>(result));
  generic_declaration_scopes_[result] = GetScopeChainSnapshot();
  return result;
}

TypeVector Declarations::GetCurrentSpecializationTypeNamesVector() {
  TypeVector result;
  if (current_generic_specialization_ != nullptr) {
    result = current_generic_specialization_->second;
  }
  return result;
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
