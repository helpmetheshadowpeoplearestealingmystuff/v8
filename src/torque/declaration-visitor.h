// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_DECLARATION_VISITOR_H_
#define V8_TORQUE_DECLARATION_VISITOR_H_

#include <set>
#include <string>

#include "src/base/macros.h"
#include "src/torque/declarations.h"
#include "src/torque/file-visitor.h"
#include "src/torque/global-context.h"
#include "src/torque/types.h"
#include "src/torque/utils.h"

namespace v8 {
namespace internal {
namespace torque {

class DeclarationVisitor : public FileVisitor {
 public:
  void Visit(Ast* ast) {
    CurrentScope::Scope current_namespace(GlobalContext::GetDefaultModule());
    for (Declaration* child : ast->declarations()) Visit(child);
  }

  void Visit(Declaration* decl);

  Module* GetOrCreateModule(const std::string& name) {
    std::vector<Module*> existing_modules =
        FilterDeclarables<Module>(Declarations::TryLookupShallow(name));
    if (existing_modules.empty()) {
      return Declarations::DeclareModule(name);
    }
    DCHECK_EQ(1, existing_modules.size());
    return existing_modules.front();
  }

  void Visit(ModuleDeclaration* decl) {
    CurrentScope::Scope current_scope(GetOrCreateModule(decl->name));
    for (Declaration* child : decl->declarations) Visit(child);
  }

  void Visit(TypeDeclaration* decl);

  void Visit(TypeAliasDeclaration* decl) {
    const Type* type = Declarations::GetType(decl->type);
    type->AddAlias(decl->name);
    Declarations::DeclareType(decl->name, type, true);
  }

  Builtin* CreateBuiltin(BuiltinDeclaration* decl, std::string external_name,
                         std::string readable_name, Signature signature,
                         base::Optional<Statement*> body);
  void Visit(ExternalBuiltinDeclaration* decl, const Signature& signature,
             base::Optional<Statement*> body) {
    Declarations::Declare(
        decl->name,
        CreateBuiltin(decl, decl->name, decl->name, signature, base::nullopt));
  }

  void Visit(ExternalRuntimeDeclaration* decl, const Signature& sig,
             base::Optional<Statement*> body);
  void Visit(ExternalMacroDeclaration* decl, const Signature& sig,
             base::Optional<Statement*> body);
  void Visit(TorqueBuiltinDeclaration* decl, const Signature& signature,
             base::Optional<Statement*> body);
  void Visit(TorqueMacroDeclaration* decl, const Signature& signature,
             base::Optional<Statement*> body);

  void Visit(CallableNode* decl, const Signature& signature,
             base::Optional<Statement*> body);

  void Visit(ConstDeclaration* decl);
  void Visit(StandardDeclaration* decl);
  void Visit(GenericDeclaration* decl);
  void Visit(SpecializationDeclaration* decl);
  void Visit(ExternConstDeclaration* decl);
  void Visit(StructDeclaration* decl);

  Signature MakeSpecializedSignature(const SpecializationKey& key);
  Callable* SpecializeImplicit(const SpecializationKey& key);
  Callable* Specialize(const SpecializationKey& key, CallableNode* declaration,
                       base::Optional<const CallableNodeSignature*> signature,
                       Statement* body);

 private:
  void DeclareSpecializedTypes(const SpecializationKey& key);
};

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_DECLARATION_VISITOR_H_
