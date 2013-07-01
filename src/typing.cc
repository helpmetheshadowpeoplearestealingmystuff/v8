// Copyright 2013 the V8 project authors. All rights reserved.
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

#include "typing.h"

#include "parser.h"  // for CompileTimeValue; TODO(rossberg): should move
#include "scopes.h"

namespace v8 {
namespace internal {


AstTyper::AstTyper(CompilationInfo* info)
    : info_(info),
      oracle_(
          Handle<Code>(info->closure()->shared()->code()),
          Handle<Context>(info->closure()->context()->native_context()),
          info->isolate(),
          info->zone()) {
  InitializeAstVisitor();
}


#define RECURSE(call)                         \
  do {                                        \
    ASSERT(!visitor->HasStackOverflow());     \
    call;                                     \
    if (visitor->HasStackOverflow()) return;  \
  } while (false)

void AstTyper::Run(CompilationInfo* info) {
  AstTyper* visitor = new(info->zone()) AstTyper(info);
  Scope* scope = info->scope();

  // Handle implicit declaration of the function name in named function
  // expressions before other declarations.
  if (scope->is_function_scope() && scope->function() != NULL) {
    RECURSE(visitor->VisitVariableDeclaration(scope->function()));
  }
  RECURSE(visitor->VisitDeclarations(scope->declarations()));
  RECURSE(visitor->VisitStatements(info->function()->body()));
}

#undef RECURSE

#define RECURSE(call)                \
  do {                               \
    ASSERT(!HasStackOverflow());     \
    call;                            \
    if (HasStackOverflow()) return;  \
  } while (false)


void AstTyper::VisitStatements(ZoneList<Statement*>* stmts) {
  for (int i = 0; i < stmts->length(); ++i) {
    Statement* stmt = stmts->at(i);
    RECURSE(Visit(stmt));
  }
}


void AstTyper::VisitBlock(Block* stmt) {
  RECURSE(VisitStatements(stmt->statements()));
}


void AstTyper::VisitExpressionStatement(ExpressionStatement* stmt) {
  RECURSE(Visit(stmt->expression()));
}


void AstTyper::VisitEmptyStatement(EmptyStatement* stmt) {
}


void AstTyper::VisitIfStatement(IfStatement* stmt) {
  RECURSE(Visit(stmt->condition()));
  RECURSE(Visit(stmt->then_statement()));
  RECURSE(Visit(stmt->else_statement()));

  if (!stmt->condition()->ToBooleanIsTrue() &&
      !stmt->condition()->ToBooleanIsFalse()) {
    stmt->condition()->RecordToBooleanTypeFeedback(oracle());
  }
}


void AstTyper::VisitContinueStatement(ContinueStatement* stmt) {
}


void AstTyper::VisitBreakStatement(BreakStatement* stmt) {
}


void AstTyper::VisitReturnStatement(ReturnStatement* stmt) {
  RECURSE(Visit(stmt->expression()));

  // TODO(rossberg): we only need this for inlining into test contexts...
  stmt->expression()->RecordToBooleanTypeFeedback(oracle());
}


void AstTyper::VisitWithStatement(WithStatement* stmt) {
  RECURSE(stmt->expression());
  RECURSE(stmt->statement());
}


void AstTyper::VisitSwitchStatement(SwitchStatement* stmt) {
  RECURSE(Visit(stmt->tag()));
  ZoneList<CaseClause*>* clauses = stmt->cases();
  SwitchStatement::SwitchType switch_type = stmt->switch_type();
  for (int i = 0; i < clauses->length(); ++i) {
    CaseClause* clause = clauses->at(i);
    if (!clause->is_default()) {
      Expression* label = clause->label();
      RECURSE(Visit(label));

      SwitchStatement::SwitchType label_switch_type =
          label->IsSmiLiteral() ? SwitchStatement::SMI_SWITCH :
          label->IsStringLiteral() ? SwitchStatement::STRING_SWITCH :
              SwitchStatement::GENERIC_SWITCH;
      if (switch_type == SwitchStatement::UNKNOWN_SWITCH)
        switch_type = label_switch_type;
      else if (switch_type != label_switch_type)
        switch_type = SwitchStatement::GENERIC_SWITCH;
    }
    RECURSE(VisitStatements(clause->statements()));
  }
  if (switch_type == SwitchStatement::UNKNOWN_SWITCH)
    switch_type = SwitchStatement::GENERIC_SWITCH;
  stmt->set_switch_type(switch_type);

  // TODO(rossberg): can we eliminate this special case and extra loop?
  if (switch_type == SwitchStatement::SMI_SWITCH) {
    for (int i = 0; i < clauses->length(); ++i) {
      CaseClause* clause = clauses->at(i);
      if (!clause->is_default())
        clause->RecordTypeFeedback(oracle());
    }
  }
}


void AstTyper::VisitDoWhileStatement(DoWhileStatement* stmt) {
  RECURSE(Visit(stmt->body()));
  RECURSE(Visit(stmt->cond()));

  if (!stmt->cond()->ToBooleanIsTrue()) {
    stmt->cond()->RecordToBooleanTypeFeedback(oracle());
  }
}


void AstTyper::VisitWhileStatement(WhileStatement* stmt) {
  RECURSE(Visit(stmt->cond()));
  RECURSE(Visit(stmt->body()));

  if (!stmt->cond()->ToBooleanIsTrue()) {
    stmt->cond()->RecordToBooleanTypeFeedback(oracle());
  }
}


void AstTyper::VisitForStatement(ForStatement* stmt) {
  if (stmt->init() != NULL) {
    RECURSE(Visit(stmt->init()));
  }
  if (stmt->cond() != NULL) {
    RECURSE(Visit(stmt->cond()));

    stmt->cond()->RecordToBooleanTypeFeedback(oracle());
  }
  RECURSE(Visit(stmt->body()));
  if (stmt->next() != NULL) {
    RECURSE(Visit(stmt->next()));
  }
}


void AstTyper::VisitForInStatement(ForInStatement* stmt) {
  RECURSE(Visit(stmt->enumerable()));
  RECURSE(Visit(stmt->body()));

  stmt->RecordTypeFeedback(oracle());
}


void AstTyper::VisitForOfStatement(ForOfStatement* stmt) {
  RECURSE(Visit(stmt->iterable()));
  RECURSE(Visit(stmt->body()));
}


void AstTyper::VisitTryCatchStatement(TryCatchStatement* stmt) {
  RECURSE(Visit(stmt->try_block()));
  RECURSE(Visit(stmt->catch_block()));
}


void AstTyper::VisitTryFinallyStatement(TryFinallyStatement* stmt) {
  RECURSE(Visit(stmt->try_block()));
  RECURSE(Visit(stmt->finally_block()));
}


void AstTyper::VisitDebuggerStatement(DebuggerStatement* stmt) {
}


void AstTyper::VisitFunctionLiteral(FunctionLiteral* expr) {
}


void AstTyper::VisitSharedFunctionInfoLiteral(SharedFunctionInfoLiteral* expr) {
}


void AstTyper::VisitConditional(Conditional* expr) {
  RECURSE(Visit(expr->condition()));
  RECURSE(Visit(expr->then_expression()));
  RECURSE(Visit(expr->else_expression()));

  expr->condition()->RecordToBooleanTypeFeedback(oracle());

  MergeLowerType(expr, Type::Intersect(
      expr->then_expression()->lower_type(),
      expr->else_expression()->lower_type()));
  MergeUpperType(expr, Type::Union(
      expr->then_expression()->upper_type(),
      expr->else_expression()->upper_type()));
}


void AstTyper::VisitVariableProxy(VariableProxy* expr) {
  // TODO(rossberg): typing of variables
}


void AstTyper::VisitLiteral(Literal* expr) {
  Type* type = Type::Constant(expr->value(), isolate_);
  MergeLowerType(expr, type);
  MergeUpperType(expr, type);
}


void AstTyper::VisitRegExpLiteral(RegExpLiteral* expr) {
  MergeLowerType(expr, Type::RegExp());
  MergeUpperType(expr, Type::RegExp());
}


void AstTyper::VisitObjectLiteral(ObjectLiteral* expr) {
  ZoneList<ObjectLiteral::Property*>* properties = expr->properties();
  for (int i = 0; i < properties->length(); ++i) {
    ObjectLiteral::Property* prop = properties->at(i);
    RECURSE(Visit(prop->value()));

    if ((prop->kind() == ObjectLiteral::Property::MATERIALIZED_LITERAL &&
        !CompileTimeValue::IsCompileTimeValue(prop->value())) ||
        prop->kind() == ObjectLiteral::Property::COMPUTED) {
      if (prop->key()->value()->IsInternalizedString() && prop->emit_store()) {
        prop->RecordTypeFeedback(oracle());
      }
    }
  }

  MergeLowerType(expr, Type::Object());
  MergeUpperType(expr, Type::Object());
}


void AstTyper::VisitArrayLiteral(ArrayLiteral* expr) {
  ZoneList<Expression*>* values = expr->values();
  for (int i = 0; i < values->length(); ++i) {
    Expression* value = values->at(i);
    RECURSE(Visit(value));
  }

  MergeLowerType(expr, Type::Array());
  MergeUpperType(expr, Type::Array());
}


void AstTyper::VisitAssignment(Assignment* expr) {
  // TODO(rossberg): Can we clean this up?
  if (expr->is_compound()) {
    RECURSE(Visit(expr->binary_operation()));

    Expression* target = expr->target();
    Property* prop = target->AsProperty();
    if (prop != NULL) {
      prop->RecordTypeFeedback(oracle(), zone());
      if (!prop->key()->IsPropertyName()) {  // i.e., keyed
        expr->RecordTypeFeedback(oracle(), zone());
      }
    }
  } else {
    RECURSE(Visit(expr->target()));
    RECURSE(Visit(expr->value()));

    if (expr->target()->AsProperty()) {
      expr->RecordTypeFeedback(oracle(), zone());
    }

    MergeLowerType(expr, expr->value()->lower_type());
    MergeUpperType(expr, expr->value()->upper_type());
  }
  // TODO(rossberg): handle target variables
}


void AstTyper::VisitYield(Yield* expr) {
  RECURSE(Visit(expr->generator_object()));
  RECURSE(Visit(expr->expression()));

  // We don't know anything about the type.
}


void AstTyper::VisitThrow(Throw* expr) {
  RECURSE(Visit(expr->exception()));

  // Lower type is None already.
  MergeUpperType(expr, Type::None());
}


void AstTyper::VisitProperty(Property* expr) {
  RECURSE(Visit(expr->obj()));
  RECURSE(Visit(expr->key()));

  expr->RecordTypeFeedback(oracle(), zone());

  // We don't know anything about the type.
}


void AstTyper::VisitCall(Call* expr) {
  RECURSE(Visit(expr->expression()));
  ZoneList<Expression*>* args = expr->arguments();
  for (int i = 0; i < args->length(); ++i) {
    Expression* arg = args->at(i);
    RECURSE(Visit(arg));
  }

  Expression* callee = expr->expression();
  Property* prop = callee->AsProperty();
  if (prop != NULL) {
    if (prop->key()->IsPropertyName())
      expr->RecordTypeFeedback(oracle(), CALL_AS_METHOD);
  } else {
    expr->RecordTypeFeedback(oracle(), CALL_AS_FUNCTION);
  }

  // We don't know anything about the type.
}


void AstTyper::VisitCallNew(CallNew* expr) {
  RECURSE(Visit(expr->expression()));
  ZoneList<Expression*>* args = expr->arguments();
  for (int i = 0; i < args->length(); ++i) {
    Expression* arg = args->at(i);
    RECURSE(Visit(arg));
  }

  expr->RecordTypeFeedback(oracle());

  // We don't know anything about the type.
}


void AstTyper::VisitCallRuntime(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  for (int i = 0; i < args->length(); ++i) {
    Expression* arg = args->at(i);
    RECURSE(Visit(arg));
  }

  // We don't know anything about the type.
}


void AstTyper::VisitUnaryOperation(UnaryOperation* expr) {
  RECURSE(Visit(expr->expression()));

  // Collect type feedback.
  Handle<Type> op_type = oracle()->UnaryType(expr->UnaryOperationFeedbackId());
  MergeLowerType(expr->expression(), op_type);
  if (expr->op() == Token::NOT) {
    // TODO(rossberg): only do in test or value context.
    expr->expression()->RecordToBooleanTypeFeedback(oracle());
  }

  switch (expr->op()) {
    case Token::NOT:
    case Token::DELETE:
      MergeLowerType(expr, Type::Boolean());
      MergeUpperType(expr, Type::Boolean());
      break;
    case Token::VOID:
      MergeLowerType(expr, Type::Undefined());
      MergeUpperType(expr, Type::Undefined());
      break;
    case Token::ADD:
    case Token::SUB: {
      MergeLowerType(expr, Type::Smi());
      Type* upper = *expr->expression()->upper_type();
      MergeUpperType(expr, upper->Is(Type::Number()) ? upper : Type::Number());
      break;
    }
    case Token::BIT_NOT:
      MergeLowerType(expr, Type::Smi());
      MergeUpperType(expr, Type::Signed32());
      break;
    case Token::TYPEOF:
      MergeLowerType(expr, Type::InternalizedString());
      MergeUpperType(expr, Type::InternalizedString());
      break;
    default:
      UNREACHABLE();
  }
}


void AstTyper::VisitCountOperation(CountOperation* expr) {
  RECURSE(Visit(expr->expression()));

  expr->RecordTypeFeedback(oracle(), zone());
  Property* prop = expr->expression()->AsProperty();
  if (prop != NULL) {
    prop->RecordTypeFeedback(oracle(), zone());
  }

  MergeLowerType(expr, Type::Smi());
  MergeUpperType(expr, Type::Number());
}


void AstTyper::VisitBinaryOperation(BinaryOperation* expr) {
  RECURSE(Visit(expr->left()));
  RECURSE(Visit(expr->right()));

  // Collect type feedback.
  Handle<Type> type, left_type, right_type;
  Maybe<int> fixed_right_arg;
  oracle()->BinaryType(expr->BinaryOperationFeedbackId(),
      &left_type, &right_type, &type, &fixed_right_arg);
  MergeLowerType(expr, type);
  MergeLowerType(expr->left(), left_type);
  MergeLowerType(expr->right(), right_type);
  expr->set_fixed_right_arg(fixed_right_arg);
  if (expr->op() == Token::OR || expr->op() == Token::AND) {
    expr->left()->RecordToBooleanTypeFeedback(oracle());
  }

  switch (expr->op()) {
    case Token::COMMA:
      MergeLowerType(expr, expr->right()->lower_type());
      MergeUpperType(expr, expr->right()->upper_type());
      break;
    case Token::OR:
    case Token::AND:
      MergeLowerType(expr, Type::Intersect(
          expr->left()->lower_type(), expr->right()->lower_type()));
      MergeUpperType(expr, Type::Union(
          expr->left()->upper_type(), expr->right()->upper_type()));
      break;
    case Token::BIT_OR:
    case Token::BIT_AND: {
      MergeLowerType(expr, Type::Smi());
      Type* upper =
          Type::Union(expr->left()->upper_type(), expr->right()->upper_type());
      MergeUpperType(expr,
          upper->Is(Type::Signed32()) ? upper : Type::Signed32());
      break;
    }
    case Token::BIT_XOR:
    case Token::SHL:
    case Token::SAR:
      MergeLowerType(expr, Type::Smi());
      MergeUpperType(expr, Type::Signed32());
      break;
    case Token::SHR:
      MergeLowerType(expr, Type::Smi());
      MergeUpperType(expr, Type::Unsigned32());
      break;
    case Token::ADD: {
      Handle<Type> l = expr->left()->lower_type();
      Handle<Type> r = expr->right()->lower_type();
      MergeLowerType(expr,
          l->Is(Type::Number()) && r->Is(Type::Number()) ? Type::Smi() :
          l->Is(Type::String()) || r->Is(Type::String()) ? Type::String() :
              Type::None());
      l = expr->left()->upper_type();
      r = expr->right()->upper_type();
      MergeUpperType(expr,
          l->Is(Type::Number()) && r->Is(Type::Number()) ? Type::Number() :
          l->Is(Type::String()) || r->Is(Type::String()) ? Type::String() :
              Type::NumberOrString());
      break;
    }
    case Token::SUB:
    case Token::MUL:
    case Token::DIV:
    case Token::MOD:
      MergeLowerType(expr, Type::Smi());
      MergeUpperType(expr, Type::Number());
      break;
    default:
      UNREACHABLE();
  }
}


void AstTyper::VisitCompareOperation(CompareOperation* expr) {
  RECURSE(Visit(expr->left()));
  RECURSE(Visit(expr->right()));

  // Collect type feedback.
  Handle<Type> left_type, right_type, combined_type;
  oracle()->CompareType(expr->CompareOperationFeedbackId(),
      &left_type, &right_type, &combined_type);
  MergeLowerType(expr->left(), left_type);
  MergeLowerType(expr->right(), right_type);
  expr->set_combined_type(combined_type);

  MergeLowerType(expr, Type::Boolean());
  MergeUpperType(expr, Type::Boolean());
}


void AstTyper::VisitThisFunction(ThisFunction* expr) {
}


void AstTyper::VisitDeclarations(ZoneList<Declaration*>* decls) {
  for (int i = 0; i < decls->length(); ++i) {
    Declaration* decl = decls->at(i);
    RECURSE(Visit(decl));
  }
}


void AstTyper::VisitVariableDeclaration(VariableDeclaration* declaration) {
}


void AstTyper::VisitFunctionDeclaration(FunctionDeclaration* declaration) {
  RECURSE(Visit(declaration->fun()));
}


void AstTyper::VisitModuleDeclaration(ModuleDeclaration* declaration) {
  RECURSE(Visit(declaration->module()));
}


void AstTyper::VisitImportDeclaration(ImportDeclaration* declaration) {
  RECURSE(Visit(declaration->module()));
}


void AstTyper::VisitExportDeclaration(ExportDeclaration* declaration) {
}


void AstTyper::VisitModuleLiteral(ModuleLiteral* module) {
  RECURSE(Visit(module->body()));
}


void AstTyper::VisitModuleVariable(ModuleVariable* module) {
}


void AstTyper::VisitModulePath(ModulePath* module) {
  RECURSE(Visit(module->module()));
}


void AstTyper::VisitModuleUrl(ModuleUrl* module) {
}


void AstTyper::VisitModuleStatement(ModuleStatement* stmt) {
  RECURSE(Visit(stmt->body()));
}


} }  // namespace v8::internal
