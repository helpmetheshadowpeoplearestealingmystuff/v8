// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_PREPARSER_H_
#define V8_PARSING_PREPARSER_H_

#include "src/ast/ast.h"
#include "src/ast/scopes.h"
#include "src/parsing/parser-base.h"
#include "src/parsing/preparser-logger.h"
#include "src/pending-compilation-error-handler.h"

namespace v8 {
namespace internal {

// Whereas the Parser generates AST during the recursive descent,
// the PreParser doesn't create a tree. Instead, it passes around minimal
// data objects (PreParserExpression, PreParserIdentifier etc.) which contain
// just enough data for the upper layer functions. PreParserFactory is
// responsible for creating these dummy objects. It provides a similar kind of
// interface as AstNodeFactory, so ParserBase doesn't need to care which one is
// used.

class PreparseDataBuilder;

class PreParserIdentifier {
 public:
  PreParserIdentifier() : type_(kUnknownIdentifier) {}
  static PreParserIdentifier Default() {
    return PreParserIdentifier(kUnknownIdentifier);
  }
  static PreParserIdentifier Null() {
    return PreParserIdentifier(kNullIdentifier);
  }
  static PreParserIdentifier Eval() {
    return PreParserIdentifier(kEvalIdentifier);
  }
  static PreParserIdentifier Arguments() {
    return PreParserIdentifier(kArgumentsIdentifier);
  }
  static PreParserIdentifier Constructor() {
    return PreParserIdentifier(kConstructorIdentifier);
  }
  static PreParserIdentifier Await() {
    return PreParserIdentifier(kAwaitIdentifier);
  }
  static PreParserIdentifier Async() {
    return PreParserIdentifier(kAsyncIdentifier);
  }
  static PreParserIdentifier Name() {
    return PreParserIdentifier(kNameIdentifier);
  }
  static PreParserIdentifier PrivateName() {
    return PreParserIdentifier(kPrivateNameIdentifier);
  }
  bool IsNull() const { return type_ == kNullIdentifier; }
  bool IsEval() const { return type_ == kEvalIdentifier; }
  bool IsAsync() const { return type_ == kAsyncIdentifier; }
  bool IsArguments() const { return type_ == kArgumentsIdentifier; }
  bool IsEvalOrArguments() const {
    STATIC_ASSERT(kEvalIdentifier + 1 == kArgumentsIdentifier);
    return IsInRange(type_, kEvalIdentifier, kArgumentsIdentifier);
  }
  bool IsConstructor() const { return type_ == kConstructorIdentifier; }
  bool IsAwait() const { return type_ == kAwaitIdentifier; }
  bool IsName() const { return type_ == kNameIdentifier; }
  bool IsPrivateName() const { return type_ == kPrivateNameIdentifier; }

 private:
  enum Type : uint8_t {
    kNullIdentifier,
    kUnknownIdentifier,
    kEvalIdentifier,
    kArgumentsIdentifier,
    kConstructorIdentifier,
    kAwaitIdentifier,
    kAsyncIdentifier,
    kNameIdentifier,
    kPrivateNameIdentifier
  };

  explicit PreParserIdentifier(Type type) : string_(nullptr), type_(type) {}
  const AstRawString* string_;

  Type type_;
  friend class PreParserExpression;
  friend class PreParser;
  friend class PreParserFactory;
};

class PreParserExpression {
 public:
  PreParserExpression() : code_(TypeField::encode(kNull)) {}

  static PreParserExpression Null() { return PreParserExpression(); }
  static PreParserExpression Failure() {
    return PreParserExpression(TypeField::encode(kFailure));
  }

  static PreParserExpression Default() {
    return PreParserExpression(TypeField::encode(kExpression));
  }

  static PreParserExpression Spread(const PreParserExpression& expression) {
    return PreParserExpression(TypeField::encode(kSpreadExpression));
  }

  static PreParserExpression FromIdentifier(const PreParserIdentifier& id) {
    return PreParserExpression(TypeField::encode(kIdentifierExpression) |
                               IdentifierTypeField::encode(id.type_));
  }

  static PreParserExpression BinaryOperation(const PreParserExpression& left,
                                             Token::Value op,
                                             const PreParserExpression& right,
                                             Zone* zone) {
    return PreParserExpression(TypeField::encode(kExpression));
  }

  static PreParserExpression Assignment() {
    return PreParserExpression(TypeField::encode(kExpression) |
                               ExpressionTypeField::encode(kAssignment));
  }

  static PreParserExpression NewTargetExpression() {
    return PreParserExpression::Default();
  }

  static PreParserExpression ObjectLiteral() {
    return PreParserExpression(TypeField::encode(kObjectLiteralExpression));
  }

  static PreParserExpression ArrayLiteral() {
    return PreParserExpression(TypeField::encode(kArrayLiteralExpression));
  }

  static PreParserExpression StringLiteral() {
    return PreParserExpression(TypeField::encode(kStringLiteralExpression));
  }

  static PreParserExpression This() {
    return PreParserExpression(TypeField::encode(kExpression) |
                               ExpressionTypeField::encode(kThisExpression));
  }

  static PreParserExpression ThisPropertyWithPrivateFieldKey() {
    return PreParserExpression(TypeField::encode(kExpression) |
                               ExpressionTypeField::encode(
                                   kThisPropertyExpressionWithPrivateFieldKey));
  }

  static PreParserExpression ThisProperty() {
    return PreParserExpression(
        TypeField::encode(kExpression) |
        ExpressionTypeField::encode(kThisPropertyExpression));
  }

  static PreParserExpression Property() {
    return PreParserExpression(
        TypeField::encode(kExpression) |
        ExpressionTypeField::encode(kPropertyExpression));
  }

  static PreParserExpression PropertyWithPrivateFieldKey() {
    return PreParserExpression(
        TypeField::encode(kExpression) |
        ExpressionTypeField::encode(kPropertyExpressionWithPrivateFieldKey));
  }

  static PreParserExpression Call() {
    return PreParserExpression(TypeField::encode(kExpression) |
                               ExpressionTypeField::encode(kCallExpression));
  }

  static PreParserExpression CallEval() {
    return PreParserExpression(
        TypeField::encode(kExpression) |
        ExpressionTypeField::encode(kCallEvalExpression));
  }

  static PreParserExpression CallTaggedTemplate() {
    return PreParserExpression(
        TypeField::encode(kExpression) |
        ExpressionTypeField::encode(kCallTaggedTemplateExpression));
  }

  bool is_tagged_template() const {
    DCHECK(IsCall());
    return ExpressionTypeField::decode(code_) == kCallTaggedTemplateExpression;
  }

  static PreParserExpression SuperCallReference() {
    return PreParserExpression(
        TypeField::encode(kExpression) |
        ExpressionTypeField::encode(kSuperCallReference));
  }

  bool IsNull() const { return TypeField::decode(code_) == kNull; }
  bool IsFailureExpression() const {
    return TypeField::decode(code_) == kFailure;
  }

  bool IsIdentifier() const {
    return TypeField::decode(code_) == kIdentifierExpression;
  }

  PreParserIdentifier AsIdentifier() const {
    DCHECK(IsIdentifier());
    return PreParserIdentifier(IdentifierTypeField::decode(code_));
  }

  bool IsAssignment() const {
    return TypeField::decode(code_) == kExpression &&
           ExpressionTypeField::decode(code_) == kAssignment;
  }

  bool IsObjectLiteral() const {
    return TypeField::decode(code_) == kObjectLiteralExpression;
  }

  bool IsArrayLiteral() const {
    return TypeField::decode(code_) == kArrayLiteralExpression;
  }

  bool IsPattern() const {
    STATIC_ASSERT(kObjectLiteralExpression + 1 == kArrayLiteralExpression);
    return IsInRange(TypeField::decode(code_), kObjectLiteralExpression,
                     kArrayLiteralExpression);
  }

  bool IsStringLiteral() const {
    return TypeField::decode(code_) == kStringLiteralExpression;
  }

  bool IsThis() const {
    return TypeField::decode(code_) == kExpression &&
           ExpressionTypeField::decode(code_) == kThisExpression;
  }

  bool IsThisProperty() const {
    return TypeField::decode(code_) == kExpression &&
           (ExpressionTypeField::decode(code_) == kThisPropertyExpression ||
            ExpressionTypeField::decode(code_) ==
                kThisPropertyExpressionWithPrivateFieldKey);
  }

  bool IsProperty() const {
    return TypeField::decode(code_) == kExpression &&
           (ExpressionTypeField::decode(code_) == kPropertyExpression ||
            ExpressionTypeField::decode(code_) == kThisPropertyExpression ||
            ExpressionTypeField::decode(code_) ==
                kPropertyExpressionWithPrivateFieldKey ||
            ExpressionTypeField::decode(code_) ==
                kThisPropertyExpressionWithPrivateFieldKey);
  }

  bool IsPropertyWithPrivateFieldKey() const {
    return TypeField::decode(code_) == kExpression &&
           (ExpressionTypeField::decode(code_) ==
                kPropertyExpressionWithPrivateFieldKey ||
            ExpressionTypeField::decode(code_) ==
                kThisPropertyExpressionWithPrivateFieldKey);
  }

  bool IsCall() const {
    return TypeField::decode(code_) == kExpression &&
           (ExpressionTypeField::decode(code_) == kCallExpression ||
            ExpressionTypeField::decode(code_) == kCallEvalExpression ||
            ExpressionTypeField::decode(code_) ==
                kCallTaggedTemplateExpression);
  }
  PreParserExpression* AsCall() {
    if (IsCall()) return this;
    return nullptr;
  }

  bool IsSuperCallReference() const {
    return TypeField::decode(code_) == kExpression &&
           ExpressionTypeField::decode(code_) == kSuperCallReference;
  }

  bool IsValidReferenceExpression() const {
    return IsIdentifier() || IsProperty();
  }

  // At the moment PreParser doesn't track these expression types.
  bool IsFunctionLiteral() const { return false; }
  bool IsCallNew() const { return false; }

  bool IsSpread() const {
    return TypeField::decode(code_) == kSpreadExpression;
  }

  bool is_parenthesized() const { return IsParenthesizedField::decode(code_); }

  void mark_parenthesized() {
    code_ = IsParenthesizedField::update(code_, true);
  }

  void clear_parenthesized() {
    code_ = IsParenthesizedField::update(code_, false);
  }

  PreParserExpression AsFunctionLiteral() { return *this; }

  // Dummy implementation for making expression->somefunc() work in both Parser
  // and PreParser.
  PreParserExpression* operator->() { return this; }

  // More dummy implementations of things PreParser doesn't need to track:
  void SetShouldEagerCompile() {}
  void mark_as_oneshot_iife() {}

  int position() const { return kNoSourcePosition; }
  void set_function_token_position(int position) {}
  void set_scope(Scope* scope) {}
  void set_suspend_count(int suspend_count) {}

 private:
  enum Type {
    kNull,
    kFailure,
    kExpression,
    kIdentifierExpression,
    kStringLiteralExpression,
    kSpreadExpression,
    kObjectLiteralExpression,
    kArrayLiteralExpression
  };

  enum ExpressionType {
    kThisExpression,
    kThisPropertyExpression,
    kThisPropertyExpressionWithPrivateFieldKey,
    kPropertyExpression,
    kPropertyExpressionWithPrivateFieldKey,
    kCallExpression,
    kCallEvalExpression,
    kCallTaggedTemplateExpression,
    kSuperCallReference,
    kAssignment
  };

  explicit PreParserExpression(uint32_t expression_code)
      : code_(expression_code) {}

  // The first three bits are for the Type.
  typedef BitField<Type, 0, 3> TypeField;

  // The high order bit applies only to nodes which would inherit from the
  // Expression ASTNode --- This is by necessity, due to the fact that
  // Expression nodes may be represented as multiple Types, not exclusively
  // through kExpression.
  // TODO(caitp, adamk): clean up PreParserExpression bitfields.
  typedef BitField<bool, TypeField::kNext, 1> IsParenthesizedField;

  // The rest of the bits are interpreted depending on the value
  // of the Type field, so they can share the storage.
  typedef BitField<ExpressionType, IsParenthesizedField::kNext, 4>
      ExpressionTypeField;
  typedef BitField<PreParserIdentifier::Type, IsParenthesizedField::kNext, 8>
      IdentifierTypeField;
  typedef BitField<bool, IsParenthesizedField::kNext, 1>
      HasCoverInitializedNameField;

  uint32_t code_;
  friend class PreParser;
  friend class PreParserFactory;
  friend class PreParserExpressionList;
};

class PreParserStatement;
class PreParserStatementList {
 public:
  PreParserStatementList() : PreParserStatementList(false) {}
  PreParserStatementList* operator->() { return this; }
  void Add(const PreParserStatement& element, Zone* zone) {}
  static PreParserStatementList Null() { return PreParserStatementList(true); }
  bool IsNull() const { return is_null_; }

 private:
  explicit PreParserStatementList(bool is_null) : is_null_(is_null) {}
  bool is_null_;
};

class PreParserScopedStatementList {
 public:
  explicit PreParserScopedStatementList(std::vector<void*>* buffer) {}
  void Rewind() {}
  void MergeInto(const PreParserScopedStatementList* other) {}
  void Add(const PreParserStatement& element) {}
  int length() { return 0; }
};

// The pre-parser doesn't need to build lists of expressions, identifiers, or
// the like. If the PreParser is used in variable tracking mode, it needs to
// build lists of variables though.
class PreParserExpressionList {
 public:
  explicit PreParserExpressionList(std::vector<void*>* buffer) : length_(0) {}

  int length() const { return length_; }

  void Add(const PreParserExpression& expression) {
    ++length_;
  }

 private:
  int length_;

  friend class PreParser;
  friend class PreParserFactory;
};

class PreParserStatement {
 public:
  static PreParserStatement Default() {
    return PreParserStatement(kUnknownStatement);
  }

  static PreParserStatement Null() {
    return PreParserStatement(kNullStatement);
  }

  static PreParserStatement Empty() {
    return PreParserStatement(kEmptyStatement);
  }

  static PreParserStatement Jump() {
    return PreParserStatement(kJumpStatement);
  }

  void InitializeStatements(const PreParserScopedStatementList& statements,
                            Zone* zone) {}

  // Creates expression statement from expression.
  // Preserves being an unparenthesized string literal, possibly
  // "use strict".
  static PreParserStatement ExpressionStatement(
      const PreParserExpression& expression) {
    if (expression.IsStringLiteral()) {
      return PreParserStatement(kStringLiteralExpressionStatement);
    }
    return Default();
  }

  bool IsStringLiteral() { return code_ == kStringLiteralExpressionStatement; }

  bool IsJumpStatement() {
    return code_ == kJumpStatement;
  }

  bool IsNull() { return code_ == kNullStatement; }

  bool IsEmptyStatement() {
    DCHECK(!IsNull());
    return code_ == kEmptyStatement;
  }

  // Dummy implementation for making statement->somefunc() work in both Parser
  // and PreParser.
  PreParserStatement* operator->() { return this; }

  PreParserStatementList statements() { return PreParserStatementList(); }
  PreParserStatementList cases() { return PreParserStatementList(); }

  void set_scope(Scope* scope) {}
  void Initialize(const PreParserExpression& cond, PreParserStatement body,
                  const SourceRange& body_range = {}) {}
  void Initialize(PreParserStatement init, const PreParserExpression& cond,
                  PreParserStatement next, PreParserStatement body,
                  const SourceRange& body_range = {}) {}
  void Initialize(PreParserExpression each, const PreParserExpression& subject,
                  PreParserStatement body, const SourceRange& body_range = {}) {
  }

 protected:
  enum Type {
    kNullStatement,
    kEmptyStatement,
    kUnknownStatement,
    kJumpStatement,
    kStringLiteralExpressionStatement,
  };

  explicit PreParserStatement(Type code) : code_(code) {}

 private:
  Type code_;
};

// A PreParserBlock extends statement with a place to store the scope.
// The scope is dropped as the block is returned as a statement.
class PreParserBlock : public PreParserStatement {
 public:
  void set_scope(Scope* scope) { scope_ = scope; }
  Scope* scope() const { return scope_; }
  static PreParserBlock Default() {
    return PreParserBlock(PreParserStatement::kUnknownStatement);
  }
  static PreParserBlock Null() {
    return PreParserBlock(PreParserStatement::kNullStatement);
  }
  // Dummy implementation for making block->somefunc() work in both Parser and
  // PreParser.
  PreParserBlock* operator->() { return this; }

 private:
  explicit PreParserBlock(PreParserStatement::Type type)
      : PreParserStatement(type), scope_(nullptr) {}
  Scope* scope_;
};

class PreParserFactory {
 public:
  explicit PreParserFactory(AstValueFactory* ast_value_factory, Zone* zone)
      : ast_node_factory_(ast_value_factory, zone), zone_(zone) {}

  AstNodeFactory* ast_node_factory() { return &ast_node_factory_; }

  PreParserExpression NewStringLiteral(const PreParserIdentifier& identifier,
                                       int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewNumberLiteral(double number,
                                       int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewUndefinedLiteral(int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewTheHoleLiteral() {
    return PreParserExpression::Default();
  }
  PreParserExpression NewRegExpLiteral(const PreParserIdentifier& js_pattern,
                                       int js_flags, int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewArrayLiteral(const PreParserExpressionList& values,
                                      int first_spread_index, int pos) {
    return PreParserExpression::ArrayLiteral();
  }
  PreParserExpression NewClassLiteralProperty(const PreParserExpression& key,
                                              const PreParserExpression& value,
                                              ClassLiteralProperty::Kind kind,
                                              bool is_static,
                                              bool is_computed_name,
                                              bool is_private) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewObjectLiteralProperty(const PreParserExpression& key,
                                               const PreParserExpression& value,
                                               ObjectLiteralProperty::Kind kind,
                                               bool is_computed_name) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewObjectLiteralProperty(const PreParserExpression& key,
                                               const PreParserExpression& value,
                                               bool is_computed_name) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewObjectLiteral(
      const PreParserExpressionList& properties, int boilerplate_properties,
      int pos, bool has_rest_property) {
    return PreParserExpression::ObjectLiteral();
  }
  PreParserExpression NewVariableProxy(void* variable) {
    return PreParserExpression::Default();
  }

  PreParserExpression NewProperty(const PreParserExpression& obj,
                                  const PreParserExpression& key, int pos) {
    if (key.IsIdentifier() && key.AsIdentifier().IsPrivateName()) {
      if (obj.IsThis()) {
        return PreParserExpression::ThisPropertyWithPrivateFieldKey();
      }
      return PreParserExpression::PropertyWithPrivateFieldKey();
    }

    if (obj.IsThis()) {
      return PreParserExpression::ThisProperty();
    }
    return PreParserExpression::Property();
  }
  PreParserExpression NewUnaryOperation(Token::Value op,
                                        const PreParserExpression& expression,
                                        int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewBinaryOperation(Token::Value op,
                                         const PreParserExpression& left,
                                         const PreParserExpression& right,
                                         int pos) {
    return PreParserExpression::BinaryOperation(left, op, right, zone_);
  }
  PreParserExpression NewCompareOperation(Token::Value op,
                                          const PreParserExpression& left,
                                          const PreParserExpression& right,
                                          int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewAssignment(Token::Value op,
                                    const PreParserExpression& left,
                                    const PreParserExpression& right, int pos) {
    // Identifiers need to be tracked since this might be a parameter with a
    // default value inside an arrow function parameter list.
    return PreParserExpression::Assignment();
  }
  PreParserExpression NewYield(const PreParserExpression& expression, int pos,
                               Suspend::OnAbruptResume on_abrupt_resume) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewAwait(const PreParserExpression& expression, int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewYieldStar(const PreParserExpression& iterable,
                                   int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewConditional(const PreParserExpression& condition,
                                     const PreParserExpression& then_expression,
                                     const PreParserExpression& else_expression,
                                     int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewCountOperation(Token::Value op, bool is_prefix,
                                        const PreParserExpression& expression,
                                        int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewCall(
      PreParserExpression expression, const PreParserExpressionList& arguments,
      int pos, Call::PossiblyEval possibly_eval = Call::NOT_EVAL) {
    if (possibly_eval == Call::IS_POSSIBLY_EVAL) {
      DCHECK(expression.IsIdentifier() && expression.AsIdentifier().IsEval());
      return PreParserExpression::CallEval();
    }
    return PreParserExpression::Call();
  }
  PreParserExpression NewTaggedTemplate(
      PreParserExpression expression, const PreParserExpressionList& arguments,
      int pos) {
    return PreParserExpression::CallTaggedTemplate();
  }
  PreParserExpression NewCallNew(const PreParserExpression& expression,
                                 const PreParserExpressionList& arguments,
                                 int pos) {
    return PreParserExpression::Default();
  }
  PreParserStatement NewReturnStatement(
      const PreParserExpression& expression, int pos,
      int continuation_pos = kNoSourcePosition) {
    return PreParserStatement::Jump();
  }
  PreParserStatement NewAsyncReturnStatement(
      const PreParserExpression& expression, int pos,
      int continuation_pos = kNoSourcePosition) {
    return PreParserStatement::Jump();
  }
  PreParserExpression NewFunctionLiteral(
      const PreParserIdentifier& name, Scope* scope,
      const PreParserScopedStatementList& body, int expected_property_count,
      int parameter_count, int function_length,
      FunctionLiteral::ParameterFlag has_duplicate_parameters,
      FunctionLiteral::FunctionType function_type,
      FunctionLiteral::EagerCompileHint eager_compile_hint, int position,
      bool has_braces, int function_literal_id,
      ProducedPreparseData* produced_preparse_data = nullptr) {
    DCHECK_NULL(produced_preparse_data);
    return PreParserExpression::Default();
  }

  PreParserExpression NewSpread(const PreParserExpression& expression, int pos,
                                int expr_pos) {
    return PreParserExpression::Spread(expression);
  }

  PreParserExpression NewEmptyParentheses(int pos) {
    PreParserExpression result = PreParserExpression::Default();
    result.mark_parenthesized();
    return result;
  }

  PreParserStatement EmptyStatement() { return PreParserStatement::Default(); }

  PreParserBlock NewBlock(int capacity, bool ignore_completion_value) {
    return PreParserBlock::Default();
  }

  PreParserBlock NewBlock(bool ignore_completion_value,
                          ZonePtrList<const AstRawString>* labels) {
    return PreParserBlock::Default();
  }

  PreParserBlock NewBlock(bool ignore_completion_value,
                          const PreParserScopedStatementList& list) {
    return PreParserBlock::Default();
  }

  PreParserStatement NewDebuggerStatement(int pos) {
    return PreParserStatement::Default();
  }

  PreParserStatement NewExpressionStatement(const PreParserExpression& expr,
                                            int pos) {
    return PreParserStatement::ExpressionStatement(expr);
  }

  PreParserStatement NewIfStatement(const PreParserExpression& condition,
                                    PreParserStatement then_statement,
                                    PreParserStatement else_statement, int pos,
                                    SourceRange then_range = {},
                                    SourceRange else_range = {}) {
    // This must return a jump statement iff both clauses are jump statements.
    return else_statement.IsJumpStatement() ? then_statement : else_statement;
  }

  PreParserStatement NewBreakStatement(
      PreParserStatement target, int pos,
      int continuation_pos = kNoSourcePosition) {
    return PreParserStatement::Jump();
  }

  PreParserStatement NewContinueStatement(
      PreParserStatement target, int pos,
      int continuation_pos = kNoSourcePosition) {
    return PreParserStatement::Jump();
  }

  PreParserStatement NewWithStatement(Scope* scope,
                                      const PreParserExpression& expression,
                                      PreParserStatement statement, int pos) {
    return PreParserStatement::Default();
  }

  PreParserStatement NewDoWhileStatement(
      ZonePtrList<const AstRawString>* labels,
      ZonePtrList<const AstRawString>* own_labels, int pos) {
    return PreParserStatement::Default();
  }

  PreParserStatement NewWhileStatement(
      ZonePtrList<const AstRawString>* labels,
      ZonePtrList<const AstRawString>* own_labels, int pos) {
    return PreParserStatement::Default();
  }

  PreParserStatement NewSwitchStatement(ZonePtrList<const AstRawString>* labels,
                                        const PreParserExpression& tag,
                                        int pos) {
    return PreParserStatement::Default();
  }

  PreParserStatement NewCaseClause(
      const PreParserExpression& label,
      const PreParserScopedStatementList& statements) {
    return PreParserStatement::Default();
  }

  PreParserStatement NewForStatement(
      ZonePtrList<const AstRawString>* labels,
      ZonePtrList<const AstRawString>* own_labels, int pos) {
    return PreParserStatement::Default();
  }

  PreParserStatement NewForEachStatement(
      ForEachStatement::VisitMode visit_mode,
      ZonePtrList<const AstRawString>* labels,
      ZonePtrList<const AstRawString>* own_labels, int pos) {
    return PreParserStatement::Default();
  }

  PreParserStatement NewForOfStatement(
      ZonePtrList<const AstRawString>* labels,
      ZonePtrList<const AstRawString>* own_labels, int pos, IteratorType type) {
    return PreParserStatement::Default();
  }

  PreParserExpression NewCallRuntime(
      Runtime::FunctionId id, ZoneChunkList<PreParserExpression>* arguments,
      int pos) {
    return PreParserExpression::Default();
  }

  PreParserExpression NewImportCallExpression(const PreParserExpression& args,
                                              int pos) {
    return PreParserExpression::Default();
  }

 private:
  // For creating VariableProxy objects to track unresolved variables.
  AstNodeFactory ast_node_factory_;
  Zone* zone_;
};

class PreParser;

class PreParserFormalParameters : public FormalParametersBase {
 public:
  explicit PreParserFormalParameters(DeclarationScope* scope)
      : FormalParametersBase(scope) {}

  void set_has_duplicate() { has_duplicate_ = true; }
  bool has_duplicate() { return has_duplicate_; }
  void ValidateDuplicate(PreParser* preparser) const;

  void set_strict_parameter_error(const Scanner::Location& loc,
                                  MessageTemplate message) {
    strict_parameter_error_ = loc.IsValid();
  }
  void ValidateStrictMode(PreParser* preparser) const;

 private:
  bool has_duplicate_ = false;
  bool strict_parameter_error_ = false;
};

class PreParserTarget {
 public:
  PreParserTarget(ParserBase<PreParser>* preparser,
                  PreParserStatement statement) {}
};

class PreParserTargetScope {
 public:
  explicit PreParserTargetScope(ParserBase<PreParser>* preparser) {}
};

class PreParserFuncNameInferrer {
 public:
  explicit PreParserFuncNameInferrer(AstValueFactory* avf) {}
  void RemoveAsyncKeywordFromEnd() const {}
  void Infer() const {}
  void RemoveLastFunction() const {}

  class State {
   public:
    explicit State(PreParserFuncNameInferrer* fni) {}

   private:
    DISALLOW_COPY_AND_ASSIGN(State);
  };

 private:
  DISALLOW_COPY_AND_ASSIGN(PreParserFuncNameInferrer);
};

class PreParserSourceRange {
 public:
  PreParserSourceRange() {}
  PreParserSourceRange(int start, int end) {}
  static PreParserSourceRange Empty() { return PreParserSourceRange(); }
  static PreParserSourceRange OpenEnded(int32_t start) { return Empty(); }
  static const PreParserSourceRange& ContinuationOf(
      const PreParserSourceRange& that, int end) {
    return that;
  }
};

class PreParserSourceRangeScope {
 public:
  PreParserSourceRangeScope(Scanner* scanner, PreParserSourceRange* range) {}
  const PreParserSourceRange& Finalize() const { return range_; }

 private:
  PreParserSourceRange range_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(PreParserSourceRangeScope);
};

class PreParserPropertyList {};

template <>
struct ParserTypes<PreParser> {
  typedef ParserBase<PreParser> Base;
  typedef PreParser Impl;

  // Return types for traversing functions.
  typedef PreParserExpression ClassLiteralProperty;
  typedef PreParserExpression Expression;
  typedef PreParserExpression FunctionLiteral;
  typedef PreParserExpression ObjectLiteralProperty;
  typedef PreParserExpression Suspend;
  typedef PreParserExpressionList ExpressionList;
  typedef PreParserExpressionList ObjectPropertyList;
  typedef PreParserFormalParameters FormalParameters;
  typedef PreParserIdentifier Identifier;
  typedef PreParserPropertyList ClassPropertyList;
  typedef PreParserScopedStatementList StatementList;
  typedef PreParserBlock Block;
  typedef PreParserStatement BreakableStatement;
  typedef PreParserStatement ForStatement;
  typedef PreParserStatement IterationStatement;
  typedef PreParserStatement Statement;

  // For constructing objects returned by the traversing functions.
  typedef PreParserFactory Factory;

  // Other implementation-specific tasks.
  typedef PreParserFuncNameInferrer FuncNameInferrer;
  typedef PreParserSourceRange SourceRange;
  typedef PreParserSourceRangeScope SourceRangeScope;
  typedef PreParserTarget Target;
  typedef PreParserTargetScope TargetScope;
};


// Preparsing checks a JavaScript program and emits preparse-data that helps
// a later parsing to be faster.
// See preparse-data-format.h for the data format.

// The PreParser checks that the syntax follows the grammar for JavaScript,
// and collects some information about the program along the way.
// The grammar check is only performed in order to understand the program
// sufficiently to deduce some information about it, that can be used
// to speed up later parsing. Finding errors is not the goal of pre-parsing,
// rather it is to speed up properly written and correct programs.
// That means that contextual checks (like a label being declared where
// it is used) are generally omitted.
class PreParser : public ParserBase<PreParser> {
  friend class ParserBase<PreParser>;

 public:
  typedef PreParserIdentifier Identifier;
  typedef PreParserExpression Expression;
  typedef PreParserStatement Statement;

  enum PreParseResult {
    kPreParseStackOverflow,
    kPreParseNotIdentifiableError,
    kPreParseSuccess
  };

  PreParser(Zone* zone, Scanner* scanner, uintptr_t stack_limit,
            AstValueFactory* ast_value_factory,
            PendingCompilationErrorHandler* pending_error_handler,
            RuntimeCallStats* runtime_call_stats, Logger* logger,
            int script_id = -1, bool parsing_module = false,
            bool parsing_on_main_thread = true)
      : ParserBase<PreParser>(zone, scanner, stack_limit, nullptr,
                              ast_value_factory, pending_error_handler,
                              runtime_call_stats, logger, script_id,
                              parsing_module, parsing_on_main_thread),
        use_counts_(nullptr),
        preparse_data_builder_(nullptr),
        preparse_data_builder_buffer_() {
    preparse_data_builder_buffer_.reserve(16);
  }

  static bool IsPreParser() { return true; }

  PreParserLogger* logger() { return &log_; }

  // Pre-parse the program from the character stream; returns true on
  // success (even if parsing failed, the pre-parse data successfully
  // captured the syntax error), and false if a stack-overflow happened
  // during parsing.
  PreParseResult PreParseProgram();

  // Parses a single function literal, from the opening parentheses before
  // parameters to the closing brace after the body.
  // Returns a FunctionEntry describing the body of the function in enough
  // detail that it can be lazily compiled.
  // The scanner is expected to have matched the "function" or "function*"
  // keyword and parameters, and have consumed the initial '{'.
  // At return, unless an error occurred, the scanner is positioned before the
  // the final '}'.
  PreParseResult PreParseFunction(
      const AstRawString* function_name, FunctionKind kind,
      FunctionLiteral::FunctionType function_type,
      DeclarationScope* function_scope, int* use_counts,
      ProducedPreparseData** produced_preparser_scope_data, int script_id);

  PreparseDataBuilder* preparse_data_builder() const {
    return preparse_data_builder_;
  }

  void set_preparse_data_builder(PreparseDataBuilder* preparse_data_builder) {
    preparse_data_builder_ = preparse_data_builder;
  }

  std::vector<void*>* preparse_data_builder_buffer() {
    return &preparse_data_builder_buffer_;
  }

 private:
  friend class i::ExpressionScope<ParserTypes<PreParser>>;
  friend class i::VariableDeclarationParsingScope<ParserTypes<PreParser>>;
  friend class i::ParameterDeclarationParsingScope<ParserTypes<PreParser>>;
  friend class i::ArrowHeadParsingScope<ParserTypes<PreParser>>;
  friend class PreParserFormalParameters;
  // These types form an algebra over syntactic categories that is just
  // rich enough to let us recognize and propagate the constructs that
  // are either being counted in the preparser data, or is important
  // to throw the correct syntax error exceptions.

  // All ParseXXX functions take as the last argument an *ok parameter
  // which is set to false if parsing failed; it is unchanged otherwise.
  // By making the 'exception handling' explicit, we are forced to check
  // for failure at the call sites.

  // Indicates that we won't switch from the preparser to the preparser; we'll
  // just stay where we are.
  bool AllowsLazyParsingWithoutUnresolvedVariables() const { return false; }
  bool parse_lazily() const { return false; }

  PendingCompilationErrorHandler* pending_error_handler() {
    return pending_error_handler_;
  }

  V8_INLINE bool SkipFunction(const AstRawString* name, FunctionKind kind,
                              FunctionLiteral::FunctionType function_type,
                              DeclarationScope* function_scope,
                              int* num_parameters,
                              ProducedPreparseData** produced_preparse_data) {
    UNREACHABLE();
  }

  Expression ParseFunctionLiteral(
      Identifier name, Scanner::Location function_name_location,
      FunctionNameValidity function_name_validity, FunctionKind kind,
      int function_token_pos, FunctionLiteral::FunctionType function_type,
      LanguageMode language_mode,
      ZonePtrList<const AstRawString>* arguments_for_wrapped_function);

  PreParserExpression InitializeObjectLiteral(PreParserExpression literal) {
    return literal;
  }

  void ParseStatementListAndLogFunction(PreParserFormalParameters* formals);

  struct TemplateLiteralState {};

  V8_INLINE TemplateLiteralState OpenTemplateLiteral(int pos) {
    return TemplateLiteralState();
  }
  V8_INLINE void AddTemplateExpression(TemplateLiteralState* state,
                                       const PreParserExpression& expression) {}
  V8_INLINE void AddTemplateSpan(TemplateLiteralState* state, bool should_cook,
                                 bool tail) {}
  V8_INLINE PreParserExpression CloseTemplateLiteral(
      TemplateLiteralState* state, int start, const PreParserExpression& tag) {
    return PreParserExpression::Default();
  }
  V8_INLINE bool IsPropertyWithPrivateFieldKey(
      const PreParserExpression& expression) {
    return expression.IsPropertyWithPrivateFieldKey();
  }
  V8_INLINE void SetLanguageMode(Scope* scope, LanguageMode mode) {
    scope->SetLanguageMode(mode);
  }
  V8_INLINE void SetAsmModule() {}

  V8_INLINE PreParserExpression SpreadCall(const PreParserExpression& function,
                                           const PreParserExpressionList& args,
                                           int pos,
                                           Call::PossiblyEval possibly_eval);
  V8_INLINE PreParserExpression
  SpreadCallNew(const PreParserExpression& function,
                const PreParserExpressionList& args, int pos);

  V8_INLINE void PrepareGeneratorVariables() {}
  V8_INLINE void RewriteAsyncFunctionBody(
      const PreParserScopedStatementList* body, PreParserStatement block,
      const PreParserExpression& return_value) {}

  V8_INLINE void DeclareLabel(ZonePtrList<const AstRawString>** labels,
                              ZonePtrList<const AstRawString>** own_labels,
                              const PreParserExpression& expr) {
    DCHECK(!parsing_module_ || !expr.AsIdentifier().IsAwait());
    DCHECK(IsIdentifier(expr));
  }

  // TODO(nikolaos): The preparser currently does not keep track of labels.
  V8_INLINE bool ContainsLabel(ZonePtrList<const AstRawString>* labels,
                               const PreParserIdentifier& label) {
    return false;
  }

  V8_INLINE PreParserExpression
  RewriteReturn(const PreParserExpression& return_value, int pos) {
    return return_value;
  }
  V8_INLINE PreParserStatement
  RewriteSwitchStatement(PreParserStatement switch_statement, Scope* scope) {
    return PreParserStatement::Default();
  }

  void DeclareVariable(VariableProxy* proxy, VariableKind kind,
                       VariableMode mode, InitializationFlag init, Scope* scope,
                       bool* was_added, int position) {
    DeclareVariableName(proxy->raw_name(), mode, scope, was_added, position,
                        kind);
  }

  void DeclareVariableName(const AstRawString* name, VariableMode mode,
                           Scope* scope, bool* was_added,
                           int position = kNoSourcePosition,
                           VariableKind kind = NORMAL_VARIABLE) {
    Variable* var = scope->DeclareVariableName(name, mode, was_added, kind);
    if (var == nullptr) {
      ReportUnidentifiableError();
      return;
    }
    if (var->scope() != scope) {
      DCHECK_NE(kNoSourcePosition, position);
      DCHECK_EQ(VariableMode::kVar, mode);
      Declaration* nested_declaration =
          factory()->ast_node_factory()->NewNestedVariableDeclaration(scope,
                                                                      position);
      nested_declaration->set_var(var);
      var->scope()->declarations()->Add(nested_declaration);
    }
  }

  V8_INLINE PreParserBlock RewriteCatchPattern(CatchInfo* catch_info) {
    return PreParserBlock::Default();
  }

  V8_INLINE void ReportVarRedeclarationIn(const AstRawString* name,
                                          Scope* scope) {
    ReportUnidentifiableError();
  }

  V8_INLINE PreParserStatement RewriteTryStatement(
      PreParserStatement try_block, PreParserStatement catch_block,
      const SourceRange& catch_range, PreParserStatement finally_block,
      const SourceRange& finally_range, const CatchInfo& catch_info, int pos) {
    return PreParserStatement::Default();
  }

  V8_INLINE void ReportUnexpectedTokenAt(
      Scanner::Location location, Token::Value token,
      MessageTemplate message = MessageTemplate::kUnexpectedToken) {
    ReportUnidentifiableError();
  }
  V8_INLINE void ParseAndRewriteGeneratorFunctionBody(
      int pos, FunctionKind kind, PreParserScopedStatementList* body) {
    ParseStatementList(body, Token::RBRACE);
  }
  V8_INLINE void ParseAndRewriteAsyncGeneratorFunctionBody(
      int pos, FunctionKind kind, PreParserScopedStatementList* body) {
    ParseStatementList(body, Token::RBRACE);
  }
  V8_INLINE void DeclareFunctionNameVar(
      const AstRawString* function_name,
      FunctionLiteral::FunctionType function_type,
      DeclarationScope* function_scope) {
    if (function_type == FunctionLiteral::kNamedExpression &&
        function_scope->LookupLocal(function_name) == nullptr) {
      DCHECK_EQ(function_scope, scope());
      function_scope->DeclareFunctionVar(function_name);
    }
  }

  V8_INLINE void DeclareFunctionNameVar(
      const PreParserIdentifier& function_name,
      FunctionLiteral::FunctionType function_type,
      DeclarationScope* function_scope) {
    DeclareFunctionNameVar(function_name.string_, function_type,
                           function_scope);
  }

  bool IdentifierEquals(const PreParserIdentifier& identifier,
                        const AstRawString* other);

  // TODO(nikolaos): The preparser currently does not keep track of labels
  // and targets.
  V8_INLINE PreParserStatement
  LookupBreakTarget(const PreParserIdentifier& label) {
    return PreParserStatement::Default();
  }
  V8_INLINE PreParserStatement
  LookupContinueTarget(const PreParserIdentifier& label) {
    return PreParserStatement::Default();
  }

  V8_INLINE PreParserStatement
  DeclareFunction(const PreParserIdentifier& variable_name,
                  const PreParserExpression& function, VariableMode mode,
                  int beg_pos, int end_pos, bool is_sloppy_block_function,
                  ZonePtrList<const AstRawString>* names) {
    DCHECK_NULL(names);
    if (variable_name.string_ != nullptr) {
      bool was_added;
      if (is_strict(language_mode())) {
        DeclareVariableName(variable_name.string_, mode, scope(), &was_added);
      } else {
        scope()->DeclareVariableName(variable_name.string_, mode, &was_added);
      }
      if (is_sloppy_block_function) {
        GetDeclarationScope()->DeclareSloppyBlockFunction(variable_name.string_,
                                                          scope());
      }
    }
    return Statement::Default();
  }

  V8_INLINE PreParserStatement DeclareClass(
      const PreParserIdentifier& variable_name,
      const PreParserExpression& value, ZonePtrList<const AstRawString>* names,
      int class_token_pos, int end_pos) {
    // Preparser shouldn't be used in contexts where we need to track the names.
    DCHECK_NULL(names);
    if (variable_name.string_ != nullptr) {
      bool was_added;
      DeclareVariableName(variable_name.string_, VariableMode::kLet, scope(),
                          &was_added);
    }
    return PreParserStatement::Default();
  }
  V8_INLINE void DeclareClassVariable(const PreParserIdentifier& name,
                                      ClassInfo* class_info,
                                      int class_token_pos) {
    if (name.string_ != nullptr) {
      bool was_added;
      DeclareVariableName(name.string_, VariableMode::kConst, scope(),
                          &was_added);
    }
  }
  V8_INLINE void DeclareClassProperty(const PreParserIdentifier& class_name,
                                      const PreParserExpression& property,
                                      bool is_constructor,
                                      ClassInfo* class_info) {}

  V8_INLINE void DeclareClassField(const PreParserExpression& property,
                                   const PreParserIdentifier& property_name,
                                   bool is_static, bool is_computed_name,
                                   bool is_private, ClassInfo* class_info) {
    DCHECK_IMPLIES(is_computed_name, !is_private);
    if (is_computed_name) {
      bool was_added;
      DeclareVariableName(
          ClassFieldVariableName(ast_value_factory(),
                                 class_info->computed_field_count),
          VariableMode::kConst, scope(), &was_added);
    } else if (is_private && property_name.string_ != nullptr) {
      bool was_added;
      DeclareVariableName(property_name.string_, VariableMode::kConst, scope(),
                          &was_added);
    }
  }

  V8_INLINE PreParserExpression
  RewriteClassLiteral(Scope* scope, const PreParserIdentifier& name,
                      ClassInfo* class_info, int pos, int end_pos) {
    bool has_default_constructor = !class_info->has_seen_constructor;
    // Account for the default constructor.
    if (has_default_constructor) {
      // Creating and disposing of a FunctionState makes tracking of
      // next_function_is_likely_called match what Parser does. TODO(marja):
      // Make the lazy function + next_function_is_likely_called + default ctor
      // logic less surprising. Default ctors shouldn't affect the laziness of
      // functions.
      bool has_extends = class_info->extends.IsNull();
      FunctionKind kind = has_extends ? FunctionKind::kDefaultDerivedConstructor
                                      : FunctionKind::kDefaultBaseConstructor;
      DeclarationScope* function_scope = NewFunctionScope(kind);
      SetLanguageMode(function_scope, LanguageMode::kStrict);
      function_scope->set_start_position(pos);
      function_scope->set_end_position(pos);
      FunctionState function_state(&function_state_, &scope_, function_scope);
      GetNextFunctionLiteralId();
    }
    if (class_info->has_static_class_fields) {
      GetNextFunctionLiteralId();
    }
    if (class_info->has_instance_members) {
      GetNextFunctionLiteralId();
    }
    return PreParserExpression::Default();
  }

  V8_INLINE PreParserStatement DeclareNative(const PreParserIdentifier& name,
                                             int pos) {
    return PreParserStatement::Default();
  }

  V8_INLINE void QueueDestructuringAssignmentForRewriting(
      PreParserExpression assignment) {}

  // Helper functions for recursive descent.
  V8_INLINE bool IsEval(const PreParserIdentifier& identifier) const {
    return identifier.IsEval();
  }

  V8_INLINE bool IsAsync(const PreParserIdentifier& identifier) const {
    return identifier.IsAsync();
  }

  V8_INLINE bool IsArguments(const PreParserIdentifier& identifier) const {
    return identifier.IsArguments();
  }

  V8_INLINE bool IsEvalOrArguments(
      const PreParserIdentifier& identifier) const {
    return identifier.IsEvalOrArguments();
  }

  V8_INLINE bool IsAwait(const PreParserIdentifier& identifier) const {
    return identifier.IsAwait();
  }

  // Returns true if the expression is of type "this.foo".
  V8_INLINE static bool IsThisProperty(const PreParserExpression& expression) {
    return expression.IsThisProperty();
  }

  V8_INLINE static bool IsIdentifier(const PreParserExpression& expression) {
    return expression.IsIdentifier();
  }

  V8_INLINE static PreParserIdentifier AsIdentifier(
      const PreParserExpression& expression) {
    return expression.AsIdentifier();
  }

  V8_INLINE static PreParserExpression AsIdentifierExpression(
      const PreParserExpression& expression) {
    return expression;
  }

  V8_INLINE bool IsConstructor(const PreParserIdentifier& identifier) const {
    return identifier.IsConstructor();
  }

  V8_INLINE bool IsName(const PreParserIdentifier& identifier) const {
    return identifier.IsName();
  }

  V8_INLINE static bool IsBoilerplateProperty(
      const PreParserExpression& property) {
    // PreParser doesn't count boilerplate properties.
    return false;
  }

  V8_INLINE bool IsNative(const PreParserExpression& expr) const {
    // Preparsing is disabled for extensions (because the extension
    // details aren't passed to lazily compiled functions), so we
    // don't accept "native function" in the preparser and there is
    // no need to keep track of "native".
    return false;
  }

  V8_INLINE static bool IsArrayIndex(const PreParserIdentifier& string,
                                     uint32_t* index) {
    return false;
  }

  V8_INLINE bool IsStringLiteral(PreParserStatement statement) const {
    return statement.IsStringLiteral();
  }

  V8_INLINE static void GetDefaultStrings(
      PreParserIdentifier* default_string,
      PreParserIdentifier* star_default_star_string) {}

  // Functions for encapsulating the differences between parsing and preparsing;
  // operations interleaved with the recursive descent.
  V8_INLINE static void PushLiteralName(const PreParserIdentifier& id) {}
  V8_INLINE static void PushVariableName(const PreParserIdentifier& id) {}
  V8_INLINE void PushPropertyName(const PreParserExpression& expression) {}
  V8_INLINE void PushEnclosingName(const PreParserIdentifier& name) {}
  V8_INLINE static void AddFunctionForNameInference(
      const PreParserExpression& expression) {}
  V8_INLINE static void InferFunctionName() {}

  V8_INLINE static void CheckAssigningFunctionLiteralToProperty(
      const PreParserExpression& left, const PreParserExpression& right) {}

  V8_INLINE bool ShortcutNumericLiteralBinaryExpression(
      PreParserExpression* x, const PreParserExpression& y, Token::Value op,
      int pos) {
    return false;
  }

  V8_INLINE NaryOperation* CollapseNaryExpression(PreParserExpression* x,
                                                  PreParserExpression y,
                                                  Token::Value op, int pos,
                                                  const SourceRange& range) {
    x->clear_parenthesized();
    return nullptr;
  }

  V8_INLINE PreParserExpression BuildUnaryExpression(
      const PreParserExpression& expression, Token::Value op, int pos) {
    return PreParserExpression::Default();
  }

  V8_INLINE PreParserStatement
  BuildInitializationBlock(DeclarationParsingResult* parsing_result) {
    return PreParserStatement::Default();
  }

  V8_INLINE PreParserBlock RewriteForVarInLegacy(const ForInfo& for_info) {
    return PreParserBlock::Null();
  }

  V8_INLINE void DesugarBindingInForEachStatement(
      ForInfo* for_info, PreParserStatement* body_block,
      PreParserExpression* each_variable) {
  }

  V8_INLINE PreParserBlock CreateForEachStatementTDZ(PreParserBlock init_block,
                                                     const ForInfo& for_info) {
    if (IsLexicalVariableMode(for_info.parsing_result.descriptor.mode)) {
      for (auto name : for_info.bound_names) {
        bool was_added;
        DeclareVariableName(name, VariableMode::kLet, scope(), &was_added);
      }
      return PreParserBlock::Default();
    }
    return init_block;
  }

  V8_INLINE StatementT DesugarLexicalBindingsInForStatement(
      PreParserStatement loop, PreParserStatement init,
      const PreParserExpression& cond, PreParserStatement next,
      PreParserStatement body, Scope* inner_scope, const ForInfo& for_info) {
    // See Parser::DesugarLexicalBindingsInForStatement.
    for (auto name : for_info.bound_names) {
      bool was_added;
      DeclareVariableName(name, for_info.parsing_result.descriptor.mode,
                          inner_scope, &was_added);
    }
    return loop;
  }

  PreParserBlock BuildParameterInitializationBlock(
      const PreParserFormalParameters& parameters);

  V8_INLINE PreParserBlock
  BuildRejectPromiseOnException(PreParserStatement init_block) {
    return PreParserBlock::Default();
  }

  V8_INLINE void InsertSloppyBlockFunctionVarBindings(DeclarationScope* scope) {
    scope->HoistSloppyBlockFunctions(nullptr);
  }

  V8_INLINE void InsertShadowingVarBindingInitializers(
      PreParserStatement block) {}

  V8_INLINE PreParserExpression NewThrowReferenceError(MessageTemplate message,
                                                       int pos) {
    return PreParserExpression::Default();
  }

  V8_INLINE PreParserExpression NewThrowSyntaxError(
      MessageTemplate message, const PreParserIdentifier& arg, int pos) {
    return PreParserExpression::Default();
  }

  V8_INLINE PreParserExpression NewThrowTypeError(
      MessageTemplate message, const PreParserIdentifier& arg, int pos) {
    return PreParserExpression::Default();
  }

  // Reporting errors.
  void ReportMessageAt(Scanner::Location source_location,
                       MessageTemplate message, const char* arg = nullptr,
                       ParseErrorType error_type = kSyntaxError) {
    pending_error_handler()->ReportMessageAt(source_location.beg_pos,
                                             source_location.end_pos, message,
                                             arg, error_type);
    scanner()->set_parser_error();
  }

  V8_INLINE void ReportUnidentifiableError() {
    pending_error_handler()->set_unidentifiable_error();
    scanner()->set_parser_error();
  }

  V8_INLINE void ReportMessageAt(Scanner::Location source_location,
                                 MessageTemplate message,
                                 const PreParserIdentifier& arg,
                                 ParseErrorType error_type = kSyntaxError) {
    UNREACHABLE();
  }

  void ReportMessageAt(Scanner::Location source_location,
                       MessageTemplate message, const AstRawString* arg,
                       ParseErrorType error_type = kSyntaxError) {
    pending_error_handler()->ReportMessageAt(source_location.beg_pos,
                                             source_location.end_pos, message,
                                             arg, error_type);
    scanner()->set_parser_error();
  }

  // "null" return type creators.
  V8_INLINE static PreParserIdentifier NullIdentifier() {
    return PreParserIdentifier::Null();
  }
  V8_INLINE static PreParserExpression NullExpression() {
    return PreParserExpression::Null();
  }
  V8_INLINE static PreParserExpression FailureExpression() {
    return PreParserExpression::Failure();
  }
  V8_INLINE static PreParserExpression NullLiteralProperty() {
    return PreParserExpression::Null();
  }
  V8_INLINE static PreParserStatementList NullStatementList() {
    return PreParserStatementList::Null();
  }
  V8_INLINE static PreParserStatement NullStatement() {
    return PreParserStatement::Null();
  }
  V8_INLINE static PreParserBlock NullBlock() { return PreParserBlock::Null(); }

  template <typename T>
  V8_INLINE static bool IsNull(T subject) {
    return subject.IsNull();
  }

  V8_INLINE PreParserIdentifier EmptyIdentifierString() const {
    return PreParserIdentifier::Default();
  }

  // Producing data during the recursive descent.
  PreParserIdentifier GetSymbol() const;

  V8_INLINE PreParserIdentifier GetNextSymbol() const {
    return PreParserIdentifier::Default();
  }

  V8_INLINE PreParserIdentifier GetNumberAsSymbol() const {
    return PreParserIdentifier::Default();
  }

  V8_INLINE PreParserExpression ThisExpression(int pos = kNoSourcePosition) {
    scope()->NewUnresolved(factory()->ast_node_factory(),
                           ast_value_factory()->this_string(), pos,
                           THIS_VARIABLE);
    return PreParserExpression::This();
  }

  V8_INLINE PreParserExpression NewSuperPropertyReference(int pos) {
    scope()->NewUnresolved(factory()->ast_node_factory(),
                           ast_value_factory()->this_function_string(), pos,
                           NORMAL_VARIABLE);
    scope()->NewUnresolved(factory()->ast_node_factory(),
                           ast_value_factory()->this_string(), pos,
                           THIS_VARIABLE);
    return PreParserExpression::Default();
  }

  V8_INLINE PreParserExpression NewSuperCallReference(int pos) {
    scope()->NewUnresolved(factory()->ast_node_factory(),
                           ast_value_factory()->this_function_string(), pos,
                           NORMAL_VARIABLE);
    scope()->NewUnresolved(factory()->ast_node_factory(),
                           ast_value_factory()->new_target_string(), pos,
                           NORMAL_VARIABLE);
    scope()->NewUnresolved(factory()->ast_node_factory(),
                           ast_value_factory()->this_string(), pos,
                           THIS_VARIABLE);
    return PreParserExpression::SuperCallReference();
  }

  V8_INLINE PreParserExpression NewTargetExpression(int pos) {
    return PreParserExpression::NewTargetExpression();
  }

  V8_INLINE PreParserExpression ImportMetaExpression(int pos) {
    return PreParserExpression::Default();
  }

  V8_INLINE PreParserExpression ExpressionFromLiteral(Token::Value token,
                                                      int pos) {
    if (token != Token::STRING) return PreParserExpression::Default();
    return PreParserExpression::StringLiteral();
  }

  PreParserExpression ExpressionFromIdentifier(
      const PreParserIdentifier& name, int start_position,
      InferName infer = InferName::kYes) {
    if (name.string_ != nullptr) {
      expression_scope()->NewVariable(name.string_, start_position);
    }
    return PreParserExpression::FromIdentifier(name);
  }

  V8_INLINE Variable* DeclareCatchVariableName(
      Scope* scope, const PreParserIdentifier& identifier) {
    return scope->DeclareCatchVariableName(identifier.string_);
  }

  V8_INLINE PreParserPropertyList NewClassPropertyList(int size) const {
    return PreParserPropertyList();
  }

  V8_INLINE PreParserStatementList NewStatementList(int size) const {
    return PreParserStatementList();
  }

  V8_INLINE PreParserExpression
  NewV8Intrinsic(const PreParserIdentifier& name,
                 const PreParserExpressionList& arguments, int pos) {
    return PreParserExpression::Default();
  }

  V8_INLINE PreParserStatement
  NewThrowStatement(const PreParserExpression& exception, int pos) {
    return PreParserStatement::Jump();
  }

  V8_INLINE void AddFormalParameter(PreParserFormalParameters* parameters,
                                    PreParserExpression& pattern,
                                    const PreParserExpression& initializer,
                                    int initializer_end_position,
                                    bool is_rest) {
    DeclarationScope* scope = parameters->scope;
    scope->RecordParameter(is_rest);
    parameters->UpdateArityAndFunctionLength(!initializer.IsNull(), is_rest);
  }

  V8_INLINE void DeclareFormalParameters(
      const PreParserFormalParameters* parameters) {
    if (!parameters->is_simple) parameters->scope->SetHasNonSimpleParameters();
  }

  V8_INLINE void DeclareArrowFunctionFormalParameters(
      PreParserFormalParameters* parameters, const PreParserExpression& params,
      const Scanner::Location& params_loc) {
  }

  V8_INLINE PreParserExpression
  ExpressionListToExpression(const PreParserExpressionList& args) {
    return PreParserExpression::Default();
  }

  V8_INLINE void SetFunctionNameFromPropertyName(
      const PreParserExpression& property, const PreParserIdentifier& name,
      const AstRawString* prefix = nullptr) {}
  V8_INLINE void SetFunctionNameFromIdentifierRef(
      const PreParserExpression& value, const PreParserExpression& identifier) {
  }

  V8_INLINE void CountUsage(v8::Isolate::UseCounterFeature feature) {
    if (use_counts_ != nullptr) ++use_counts_[feature];
  }

  V8_INLINE bool ParsingDynamicFunctionDeclaration() const { return false; }

// Generate empty functions here as the preparser does not collect source
// ranges for block coverage.
#define DEFINE_RECORD_SOURCE_RANGE(Name) \
  template <typename... Ts>              \
  V8_INLINE void Record##Name##SourceRange(Ts... args) {}
  AST_SOURCE_RANGE_LIST(DEFINE_RECORD_SOURCE_RANGE)
#undef DEFINE_RECORD_SOURCE_RANGE

  // Preparser's private field members.

  int* use_counts_;
  PreParserLogger log_;

  PreparseDataBuilder* preparse_data_builder_;
  std::vector<void*> preparse_data_builder_buffer_;
};

PreParserExpression PreParser::SpreadCall(const PreParserExpression& function,
                                          const PreParserExpressionList& args,
                                          int pos,
                                          Call::PossiblyEval possibly_eval) {
  return factory()->NewCall(function, args, pos, possibly_eval);
}

PreParserExpression PreParser::SpreadCallNew(
    const PreParserExpression& function, const PreParserExpressionList& args,
    int pos) {
  return factory()->NewCallNew(function, args, pos);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_PREPARSER_H_
