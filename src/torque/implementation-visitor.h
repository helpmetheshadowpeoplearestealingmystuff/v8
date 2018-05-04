// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_IMPLEMENTATION_VISITOR_H_
#define V8_TORQUE_IMPLEMENTATION_VISITOR_H_

#include <string>

#include "src/base/macros.h"
#include "src/torque/ast.h"
#include "src/torque/file-visitor.h"
#include "src/torque/global-context.h"
#include "src/torque/types.h"
#include "src/torque/utils.h"

namespace v8 {
namespace internal {
namespace torque {

struct LocationReference {
  LocationReference(Value* v, VisitResult b, VisitResult i)
      : value(v), base(b), index(i) {}
  Value* value;
  VisitResult base;
  VisitResult index;
};

class ImplementationVisitor : public FileVisitor {
 public:
  explicit ImplementationVisitor(GlobalContext& global_context)
      : FileVisitor(global_context), indent_(0), next_temp_(0) {}

  void Visit(Ast* ast) { Visit(ast->default_module()); }

  VisitResult Visit(Expression* expr);
  Type Visit(Statement* stmt);
  void Visit(Declaration* decl);

  LocationReference GetLocationReference(LocationExpression* location);
  LocationReference GetLocationReference(IdentifierExpression* expr) {
    return LocationReference(declarations()->LookupValue(expr->pos, expr->name),
                             {}, {});
  }
  LocationReference GetLocationReference(FieldAccessExpression* expr) {
    return LocationReference({}, Visit(expr->object), {});
  }
  LocationReference GetLocationReference(ElementAccessExpression* expr) {
    return LocationReference({}, Visit(expr->array), Visit(expr->index));
  }

  VisitResult GenerateFetchFromLocation(LocationExpression* location,
                                        LocationReference reference);
  VisitResult GenerateFetchFromLocation(IdentifierExpression* expr,
                                        LocationReference reference) {
    Value* value = reference.value;
    if (value->IsVariable() && !Variable::cast(value)->IsDefined()) {
      std::stringstream s;
      s << "\"" << value->name() << "\" is used before it is defined at "
        << PositionAsString(expr->pos);
      ReportError(s.str());
    }
    return VisitResult({value->type(), value->GetValueForRead()});
  }
  VisitResult GenerateFetchFromLocation(FieldAccessExpression* expr,
                                        LocationReference reference) {
    Arguments arguments;
    arguments.parameters = {reference.base};
    return GenerateOperation(expr->pos, std::string(".") + expr->field,
                             arguments);
  }
  VisitResult GenerateFetchFromLocation(ElementAccessExpression* expr,
                                        LocationReference reference) {
    Arguments arguments;
    arguments.parameters = {reference.base, reference.index};
    return GenerateOperation(expr->pos, "[]", arguments);
  }

  VisitResult Visit(IdentifierExpression* expr) {
    return GenerateFetchFromLocation(expr, GetLocationReference(expr));
  }
  VisitResult Visit(FieldAccessExpression* expr) {
    return GenerateFetchFromLocation(expr, GetLocationReference(expr));
  }
  VisitResult Visit(ElementAccessExpression* expr) {
    return GenerateFetchFromLocation(expr, GetLocationReference(expr));
  }

  VisitResult Visit(CastExpression* expr);
  VisitResult Visit(ConvertExpression* expr);

  void Visit(ModuleDeclaration* decl);
  void Visit(DefaultModuleDeclaration* decl) {
    Visit(implicit_cast<ModuleDeclaration*>(decl));
  }
  void Visit(ExplicitModuleDeclaration* decl) {
    Visit(implicit_cast<ModuleDeclaration*>(decl));
  }
  void Visit(MacroDeclaration* decl);
  void Visit(BuiltinDeclaration* decl);
  void Visit(TypeDeclaration* decl) {}
  void Visit(ConstDeclaration* decl) {}
  void Visit(ExternalMacroDeclaration* decl) {}
  void Visit(ExternalBuiltinDeclaration* decl) {}
  void Visit(ExternalRuntimeDeclaration* decl) {}

  VisitResult Visit(CallExpression* expr, bool is_tail = false);
  Type Visit(TailCallStatement* stmt);

  VisitResult Visit(ConditionalExpression* expr);

  VisitResult Visit(LogicalOrExpression* expr);
  VisitResult Visit(LogicalAndExpression* expr);

  LocationReference GetLocationReference(
      TorqueParser::LocationExpressionContext* locationExpression);

  VisitResult Visit(IncrementDecrementExpression* expr);
  VisitResult Visit(AssignmentExpression* expr);
  VisitResult Visit(StringLiteralExpression* expr);
  VisitResult Visit(NumberLiteralExpression* expr);

  Type Visit(TryCatchStatement* stmt);
  Type Visit(ReturnStatement* stmt);
  Type Visit(GotoStatement* stmt);
  Type Visit(IfStatement* stmt);
  Type Visit(WhileStatement* stmt);
  Type Visit(BreakStatement* stmt);
  Type Visit(ContinueStatement* stmt);
  Type Visit(ForLoopStatement* stmt);
  Type Visit(VarDeclarationStatement* stmt);
  Type Visit(ForOfLoopStatement* stmt);
  Type Visit(BlockStatement* block);
  Type Visit(ExpressionStatement* stmt);
  Type Visit(DebugStatement* stmt);
  Type Visit(AssertStatement* stmt);

  void GenerateImplementation(const std::string& dir, Module* module);

 private:
  std::string GetBaseAssemblerName(Module* module);

  std::string GetDSLAssemblerName(Module* module);

  void GenerateIndent();

  class ScopedIndent {
   public:
    explicit ScopedIndent(ImplementationVisitor* visitor, bool new_lines = true)
        : new_lines_(new_lines), visitor_(visitor) {
      if (new_lines) visitor->GenerateIndent();
      visitor->source_out() << "{";
      if (new_lines) visitor->source_out() << std::endl;
      visitor->indent_++;
    }
    ~ScopedIndent() {
      visitor_->indent_--;
      visitor_->GenerateIndent();
      visitor_->source_out() << "}";
      if (new_lines_) visitor_->source_out() << std::endl;
    }

   private:
    bool new_lines_;
    ImplementationVisitor* visitor_;
  };

  void GenerateChangedVarsFromControlSplit(AstNode* node);

  Type GetCommonType(SourcePosition pos, Type left, Type right);

  VisitResult GenerateCopy(const VisitResult& to_copy);

  void GenerateAssignToVariable(SourcePosition pos, Variable* var,
                                VisitResult value);

  void GenerateAssignToLocation(LocationExpression* location,
                                const LocationReference& reference,
                                VisitResult assignment_value);

  Variable* GenerateVariableDeclaration(
      AstNode* node, const std::string& name, const base::Optional<Type>& type,
      const base::Optional<VisitResult>& initialization = {});

  void GenerateParameter(SourcePosition pos, const std::string& parameter_name);

  void GenerateParameterList(SourcePosition pos, const NameVector& list,
                             size_t first = 0);

  VisitResult GenerateCall(SourcePosition pos, const std::string& callable_name,
                           const Arguments& parameters, bool tail_call);

  bool GenerateLabeledStatementBlocks(
      const std::vector<Statement*>& blocks,
      const std::vector<Label*>& statement_labels, Label* merge_label);

  void GenerateBranch(const VisitResult& condition, Label* true_label,
                      Label* false_label);

  bool GenerateExpressionBranch(Expression* expression,
                                const std::vector<Label*>& statement_labels,
                                const std::vector<Statement*>& statement_blocks,
                                Label* merge_label);

  void GenerateMacroFunctionDeclaration(std::ostream& o, SourcePosition pos,
                                        const std::string& macro_prefix,
                                        Macro* macro);

  VisitResult GenerateOperation(SourcePosition pos,
                                const std::string& operation,
                                Arguments arguments,
                                base::Optional<Type> return_type = {});

  VisitResult GenerateImplicitConvert(SourcePosition pos, Type destination_type,
                                      VisitResult source);

  std::string NewTempVariable();

  std::string GenerateNewTempVariable(Type type);

  void GenerateLabelDefinition(Label* label, AstNode* node = nullptr);

  void GenerateLabelBind(Label* label);

  void GenerateLabelGoto(Label* label);

  std::vector<Label*> LabelsFromIdentifiers(
      SourcePosition pos, const std::vector<std::string>& names);

  std::ostream& source_out() { return module_->source_stream(); }

  std::ostream& header_out() { return module_->header_stream(); }

  size_t indent_;
  int32_t next_temp_;
};

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_IMPLEMENTATION_VISITOR_H_
