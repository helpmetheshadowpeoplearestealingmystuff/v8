// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_DECLARABLE_H_
#define V8_TORQUE_DECLARABLE_H_

#include <cassert>
#include <string>

#include "src/base/functional.h"
#include "src/base/logging.h"
#include "src/torque/ast.h"
#include "src/torque/types.h"
#include "src/torque/utils.h"

namespace v8 {
namespace internal {
namespace torque {

class Scope;
class ScopeChain;

class Declarable {
 public:
  virtual ~Declarable() {}
  enum Kind {
    kAbstractType,
    kFunctionPointerType,
    kVariable,
    kParameter,
    kMacro,
    kMacroList,
    kBuiltin,
    kRuntimeFunction,
    kGeneric,
    kTypeAlias,
    kLabel,
    kConstant
  };
  Kind kind() const { return kind_; }
  bool IsAbstractType() const { return kind() == kAbstractType; }
  bool IsFunctionPointerType() const { return kind() == kFunctionPointerType; }
  bool IsType() const { return IsAbstractType() || IsFunctionPointerType(); }
  bool IsMacro() const { return kind() == kMacro; }
  bool IsBuiltin() const { return kind() == kBuiltin; }
  bool IsRuntimeFunction() const { return kind() == kRuntimeFunction; }
  bool IsGeneric() const { return kind() == kGeneric; }
  bool IsTypeAlias() const { return kind() == kTypeAlias; }
  bool IsParameter() const { return kind() == kParameter; }
  bool IsLabel() const { return kind() == kLabel; }
  bool IsVariable() const { return kind() == kVariable; }
  bool IsMacroList() const { return kind() == kMacroList; }
  bool IsConstant() const { return kind() == kConstant; }
  bool IsValue() const { return IsVariable() || IsConstant() || IsParameter(); }
  virtual const char* type_name() const { return "<<unknown>>"; }

 protected:
  explicit Declarable(Kind kind) : kind_(kind) {}

 private:
  Kind kind_;
};

#define DECLARE_DECLARABLE_BOILERPLATE(x, y)                  \
  static x* cast(Declarable* declarable) {                    \
    DCHECK(declarable->Is##x());                              \
    return static_cast<x*>(declarable);                       \
  }                                                           \
  static const x* cast(const Declarable* declarable) {        \
    DCHECK(declarable->Is##x());                              \
    return static_cast<const x*>(declarable);                 \
  }                                                           \
  const char* type_name() const override { return #y; }       \
  static x* DynamicCast(Declarable* declarable) {             \
    if (!declarable) return nullptr;                          \
    if (!declarable->Is##x()) return nullptr;                 \
    return static_cast<x*>(declarable);                       \
  }                                                           \
  static const x* DynamicCast(const Declarable* declarable) { \
    if (!declarable) return nullptr;                          \
    if (!declarable->Is##x()) return nullptr;                 \
    return static_cast<const x*>(declarable);                 \
  }

class Type : public Declarable {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(Type, type);

  bool IsSubtypeOf(const Type* supertype) const;
  virtual std::string ToString() const = 0;
  virtual std::string MangledName() const = 0;
  bool IsVoid() const { return IsAbstractName(VOID_TYPE_STRING); }
  bool IsNever() const { return IsAbstractName(NEVER_TYPE_STRING); }
  bool IsBool() const { return IsAbstractName(BOOL_TYPE_STRING); }
  bool IsConstexprBool() const {
    return IsAbstractName(CONSTEXPR_BOOL_TYPE_STRING);
  }
  bool IsVoidOrNever() const { return IsVoid() || IsNever(); }
  virtual const std::string& GetGeneratedTypeName() const = 0;
  virtual std::string GetGeneratedTNodeTypeName() const = 0;
  virtual bool IsConstexpr() const = 0;

 protected:
  Type(Declarable::Kind kind, const Type* parent)
      : Declarable(kind), parent_(parent) {}
  const Type* parent() const { return parent_; }

 private:
  bool IsAbstractName(const std::string& name) const;

  const Type* const parent_;
};

class AbstractType : public Type {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(AbstractType, abstract_type);
  AbstractType(const Type* parent, const std::string& name,
               const std::string& generated_type)
      : Type(Declarable::kAbstractType, parent),
        name_(name),
        generated_type_(generated_type) {}
  const std::string& name() const { return name_; }
  std::string ToString() const override { return name(); }
  std::string MangledName() const override { return "AT" + name(); }
  const std::string& GetGeneratedTypeName() const override {
    return generated_type_;
  }
  std::string GetGeneratedTNodeTypeName() const override;
  bool IsConstexpr() const override {
    return name().substr(0, strlen(CONSTEXPR_TYPE_PREFIX)) ==
           CONSTEXPR_TYPE_PREFIX;
  }

 private:
  const std::string name_;
  const std::string generated_type_;
};

// For now, function pointers are restricted to Code objects of Torque-defined
// builtins.
class FunctionPointerType : public Type {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(FunctionPointerType, function_pointer_type);
  FunctionPointerType(const Type* parent, TypeVector parameter_types,
                      const Type* return_type)
      : Type(Declarable::kFunctionPointerType, parent),
        parameter_types_(parameter_types),
        return_type_(return_type) {}
  std::string ToString() const override;
  std::string MangledName() const override;
  const std::string& GetGeneratedTypeName() const override {
    return parent()->GetGeneratedTypeName();
  }
  std::string GetGeneratedTNodeTypeName() const override {
    return parent()->GetGeneratedTNodeTypeName();
  }
  bool IsConstexpr() const override { return parent()->IsConstexpr(); }

  const TypeVector& parameter_types() const { return parameter_types_; }
  const Type* return_type() const { return return_type_; }

  friend size_t hash_value(const FunctionPointerType& p) {
    size_t result = base::hash_value(p.return_type_);
    for (const Type* parameter : p.parameter_types_) {
      result = base::hash_combine(result, parameter);
    }
    return result;
  }
  bool operator==(const FunctionPointerType& other) const {
    return parameter_types_ == other.parameter_types_ &&
           return_type_ == other.return_type_;
  }

 private:
  const TypeVector parameter_types_;
  const Type* const return_type_;
};

inline std::ostream& operator<<(std::ostream& os, const Type* t) {
  os << t->ToString();
  return os;
}

class Value : public Declarable {
 public:
  const std::string& name() const { return name_; }
  virtual bool IsConst() const { return true; }
  virtual std::string GetValueForDeclaration() const = 0;
  virtual std::string GetValueForRead() const {
    return GetValueForDeclaration();
  }
  virtual std::string GetValueForWrite() const { UNREACHABLE(); }
  DECLARE_DECLARABLE_BOILERPLATE(Value, value);
  const Type* type() const { return type_; }

 protected:
  Value(Kind kind, const Type* type, const std::string& name)
      : Declarable(kind), type_(type), name_(name) {}

 private:
  const Type* type_;
  std::string name_;
};

class Parameter : public Value {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(Parameter, parameter);
  std::string GetValueForDeclaration() const override { return var_name_; }

 private:
  friend class Declarations;
  Parameter(const std::string& name, const Type* type,
            const std::string& var_name)
      : Value(Declarable::kParameter, type, name), var_name_(var_name) {}

  std::string var_name_;
};

class Variable : public Value {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(Variable, variable);
  bool IsConst() const override { return false; }
  std::string GetValueForDeclaration() const override { return value_; }
  std::string GetValueForRead() const override {
    if (type()->IsConstexpr()) {
      return std::string("*") + value_;
    } else {
      return value_ + "->value()";
    }
  }
  std::string GetValueForWrite() const override {
    return std::string("*") + value_;
  }
  void Define() { defined_ = true; }
  bool IsDefined() const { return defined_; }

 private:
  friend class Declarations;
  Variable(const std::string& name, const std::string& value, const Type* type)
      : Value(Declarable::kVariable, type, name),
        value_(value),
        defined_(false) {}

  std::string value_;
  bool defined_;
};

class Label : public Declarable {
 public:
  void AddVariable(Variable* var) { parameters_.push_back(var); }
  std::string name() const { return name_; }
  std::string generated() const { return generated_; }
  Variable* GetParameter(size_t i) const { return parameters_[i]; }
  size_t GetParameterCount() const { return parameters_.size(); }
  const std::vector<Variable*>& GetParameters() const { return parameters_; }

  DECLARE_DECLARABLE_BOILERPLATE(Label, label);
  void MarkUsed() { used_ = true; }
  bool IsUsed() const { return used_; }

 private:
  friend class Declarations;
  explicit Label(const std::string& name)
      : Declarable(Declarable::kLabel),
        name_(name),
        generated_("label_" + name + "_" + std::to_string(next_id_++)),
        used_(false) {}

  std::string name_;
  std::string generated_;
  std::vector<Variable*> parameters_;
  static size_t next_id_;
  bool used_;
};

class Constant : public Value {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(Constant, constant);
  std::string GetValueForDeclaration() const override { return value_; }

 private:
  friend class Declarations;
  explicit Constant(const std::string& name, const Type* type,
                    const std::string& value)
      : Value(Declarable::kConstant, type, name), value_(value) {}

  std::string value_;
};

class Callable : public Declarable {
 public:
  static Callable* cast(Declarable* declarable) {
    assert(declarable->IsMacro() || declarable->IsBuiltin() ||
           declarable->IsRuntimeFunction());
    return static_cast<Callable*>(declarable);
  }
  static const Callable* cast(const Declarable* declarable) {
    assert(declarable->IsMacro() || declarable->IsBuiltin() ||
           declarable->IsRuntimeFunction());
    return static_cast<const Callable*>(declarable);
  }
  const std::string& name() const { return name_; }
  const Signature& signature() const { return signature_; }
  const NameVector& parameter_names() const {
    return signature_.parameter_names;
  }
  bool HasReturnValue() const {
    return !signature_.return_type->IsVoidOrNever();
  }
  void IncrementReturns() { ++returns_; }
  bool HasReturns() const { return returns_; }

 protected:
  Callable(Declarable::Kind kind, const std::string& name,
           const Signature& signature)
      : Declarable(kind), name_(name), signature_(signature), returns_(0) {}

 private:
  std::string name_;
  Signature signature_;
  size_t returns_;
};

class Macro : public Callable {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(Macro, macro);

 protected:
  Macro(Declarable::Kind type, const std::string& name,
        const Signature& signature)
      : Callable(type, name, signature) {}

 private:
  friend class Declarations;
  Macro(const std::string& name, const Signature& signature)
      : Macro(Declarable::kMacro, name, signature) {}
};

class MacroList : public Declarable {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(MacroList, macro_list);
  const std::vector<Macro*>& list() { return list_; }
  Macro* AddMacro(Macro* macro) {
    list_.emplace_back(macro);
    return macro;
  }

 private:
  friend class Declarations;
  MacroList() : Declarable(Declarable::kMacroList) {}

  std::vector<Macro*> list_;
};

class Builtin : public Callable {
 public:
  enum Kind { kStub, kFixedArgsJavaScript, kVarArgsJavaScript };
  DECLARE_DECLARABLE_BOILERPLATE(Builtin, builtin);
  Kind kind() const { return kind_; }
  bool IsStub() const { return kind_ == kStub; }
  bool IsVarArgsJavaScript() const { return kind_ == kVarArgsJavaScript; }
  bool IsFixedArgsJavaScript() const { return kind_ == kFixedArgsJavaScript; }
  bool IsExternal() const { return external_; }

 private:
  friend class Declarations;
  Builtin(const std::string& name, Builtin::Kind kind, bool external,
          const Signature& signature)
      : Callable(Declarable::kBuiltin, name, signature),
        kind_(kind),
        external_(external) {}

  Kind kind_;
  bool external_;
};

class RuntimeFunction : public Callable {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(RuntimeFunction, runtime);

 private:
  friend class Declarations;
  RuntimeFunction(const std::string& name, const Signature& signature)
      : Callable(Declarable::kRuntimeFunction, name, signature) {}
};

class Generic : public Declarable {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(Generic, generic);

  GenericDeclaration* declaration() const { return declaration_; }
  Module* module() const { return module_; }

 private:
  friend class Declarations;
  Generic(const std::string& name, Module* module,
          GenericDeclaration* declaration)
      : Declarable(Declarable::kGeneric),
        module_(module),
        declaration_(declaration) {}

  Module* module_;
  GenericDeclaration* declaration_;
};

typedef std::pair<Generic*, TypeVector> SpecializationKey;

class TypeAlias : public Declarable {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(TypeAlias, instantiated_type);

  const Type* type() const { return type_; }

 private:
  friend class Declarations;
  TypeAlias(const std::string& name, const Type* type)
      : Declarable(Declarable::kTypeAlias), type_(type) {}

  const Type* type_;
};

inline std::ostream& operator<<(std::ostream& os, const Callable& m) {
  os << "callable " << m.name() << "(" << m.signature().parameter_types
     << "): " << m.signature().return_type;
  return os;
}

inline std::ostream& operator<<(std::ostream& os, const Variable& v) {
  os << "variable " << v.name() << ": " << v.type();
  return os;
}

inline std::ostream& operator<<(std::ostream& os, const Builtin& b) {
  os << "builtin " << b.signature().return_type << " " << b.name()
     << b.signature().parameter_types;
  return os;
}

inline std::ostream& operator<<(std::ostream& os, const RuntimeFunction& b) {
  os << "runtime function " << b.signature().return_type << " " << b.name()
     << b.signature().parameter_types;
  return os;
}

#undef DECLARE_DECLARABLE_BOILERPLATE

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_DECLARABLE_H_
