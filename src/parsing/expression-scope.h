// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_EXPRESSION_SCOPE_H_
#define V8_PARSING_EXPRESSION_SCOPE_H_

#include "src/ast/scopes.h"
#include "src/message-template.h"
#include "src/parsing/scanner.h"
#include "src/zone/zone.h"  // For ScopedPtrList.

namespace v8 {
namespace internal {

template <typename Types>
class ExpressionParsingScope;
template <typename Types>
class AccumulationScope;
template <typename Types>
class ArrowHeadParsingScope;
class VariableProxy;

// ExpressionScope is used in a stack fashion, and is used to specialize
// expression parsing for the task at hand. It allows the parser to reuse the
// same code to parse destructuring declarations, assignment patterns,
// expressions, and (async) arrow function heads.
//
// One of the specific subclasses needs to be instantiated to tell the parser
// the meaning of the expression it will parse next. The parser then calls
// Record* on the expression_scope() to indicate errors. The expression_scope
// will either discard those errors, immediately report those errors, or
// classify the errors for later validation.
// TODO(verwaest): Record is a slightly odd name since it will directly throw
// for unambiguous scopes.
template <typename Types>
class ExpressionScope {
 public:
  typedef typename Types::Impl ParserT;
  typedef typename Types::Expression ExpressionT;

  VariableProxy* NewVariable(const AstRawString* name,
                             int pos = kNoSourcePosition) {
    VariableProxy* result = parser_->NewRawVariable(name, pos);
    if (CanBeExpression()) AsExpressionParsingScope()->TrackVariable(result);
    return result;
  }

  void MarkIdentifierAsAssigned() {
    if (!CanBeExpression()) return;
    AsExpressionParsingScope()->MarkIdentifierAsAssigned();
  }

  void ValidateAsPattern(ExpressionT expression, int begin, int end) {
    if (!CanBeExpression()) return;
    AsExpressionParsingScope()->ValidatePattern(expression, begin, end);
    AsExpressionParsingScope()->ClearExpressionError();
  }

  // Record async arrow parameters errors in all ambiguous async arrow scopes in
  // the chain up to the first unambiguous scope.
  void RecordAsyncArrowParametersError(const Scanner::Location& loc,
                                       MessageTemplate message) {
    // Only ambiguous scopes (ExpressionParsingScope, *ArrowHeadParsingScope)
    // need to propagate errors to a possible kAsyncArrowHeadParsingScope, so
    // immediately return if the current scope is not ambiguous.
    if (!CanBeExpression()) return;
    AsExpressionParsingScope()->RecordAsyncArrowParametersError(loc, message);
  }

  // Record initializer errors in all scopes that can turn into parameter scopes
  // (ArrowHeadParsingScopes) up to the first known unambiguous parameter scope.
  void RecordParameterInitializerError(const Scanner::Location& loc,
                                       MessageTemplate message) {
    ExpressionScope* scope = this;
    while (!scope->IsCertainlyParameterDeclaration()) {
      if (!has_possible_parameter_in_scope_chain_) return;
      if (scope->CanBeParameterDeclaration()) {
        scope->AsArrowHeadParsingScope()->RecordDeclarationError(loc, message);
      }
      scope = scope->parent();
      if (scope == nullptr) return;
    }
    Report(loc, message);
  }

  void RecordPatternError(const Scanner::Location& loc,
                          MessageTemplate message) {
    // TODO(verwaest): Non-assigning expression?
    if (IsCertainlyPattern()) {
      Report(loc, message);
    } else {
      AsExpressionParsingScope()->RecordPatternError(loc, message);
    }
  }

  void RecordStrictModeParameterError(const Scanner::Location& loc,
                                      MessageTemplate message) {
    DCHECK_IMPLIES(!has_error(), loc.IsValid());
    if (!CanBeParameterDeclaration()) return;
    if (IsCertainlyParameterDeclaration()) {
      if (is_strict(parser_->language_mode())) {
        Report(loc, message);
      } else {
        parser_->parameters_->set_strict_parameter_error(loc, message);
      }
    } else {
      parser_->next_arrow_function_info_.strict_parameter_error_location = loc;
      parser_->next_arrow_function_info_.strict_parameter_error_message =
          message;
    }
  }

  void RecordDeclarationError(const Scanner::Location& loc,
                              MessageTemplate message) {
    if (!CanBeDeclaration()) return;
    if (IsCertainlyDeclaration()) {
      Report(loc, message);
    } else {
      AsArrowHeadParsingScope()->RecordDeclarationError(loc, message);
    }
  }

  void RecordExpressionError(const Scanner::Location& loc,
                             MessageTemplate message) {
    if (!CanBeExpression()) return;
    // TODO(verwaest): Non-assigning expression?
    // if (IsCertainlyExpression()) Report(loc, message);
    AsExpressionParsingScope()->RecordExpressionError(loc, message);
  }

  void RecordLexicalDeclarationError(const Scanner::Location& loc,
                                     MessageTemplate message) {
    if (IsLexicalDeclaration()) Report(loc, message);
  }

  void RecordNonSimpleParameter() {
    if (!IsArrowHeadParsingScope()) return;
    AsArrowHeadParsingScope()->RecordNonSimpleParameter();
  }

 protected:
  enum ScopeType : uint8_t {
    // Expression or assignment target.
    kExpression,

    // Declaration or expression or assignment target.
    kMaybeArrowParameterDeclaration,
    kMaybeAsyncArrowParameterDeclaration,

    // Declarations.
    kParameterDeclaration,
    kVarDeclaration,
    kLexicalDeclaration,
  };

  ParserT* parser() const { return parser_; }
  ExpressionScope* parent() const { return parent_; }

  void Report(const Scanner::Location& loc, MessageTemplate message) const {
    parser_->ReportMessageAt(loc, message);
  }

  ExpressionScope(ParserT* parser, ScopeType type)
      : parser_(parser),
        parent_(parser->expression_scope_),
        type_(type),
        has_possible_parameter_in_scope_chain_(
            CanBeParameterDeclaration() ||
            (parent_ && parent_->has_possible_parameter_in_scope_chain_)) {
    parser->expression_scope_ = this;
  }

  ~ExpressionScope() {
    DCHECK(parser_->expression_scope_ == this ||
           parser_->expression_scope_ == parent_);
    parser_->expression_scope_ = parent_;
  }

  ExpressionParsingScope<Types>* AsExpressionParsingScope() {
    DCHECK(CanBeExpression());
    return static_cast<ExpressionParsingScope<Types>*>(this);
  }

#ifdef DEBUG
  bool has_error() const { return parser_->has_error(); }
#endif

  bool CanBeExpression() const {
    return IsInRange(type_, kExpression, kMaybeAsyncArrowParameterDeclaration);
  }
  bool CanBeDeclaration() const {
    return IsInRange(type_, kMaybeArrowParameterDeclaration,
                     kLexicalDeclaration);
  }
  bool IsCertainlyDeclaration() const {
    return IsInRange(type_, kParameterDeclaration, kLexicalDeclaration);
  }
  bool IsVariableDeclaration() const {
    return IsInRange(type_, kVarDeclaration, kLexicalDeclaration);
  }
  bool IsAsyncArrowHeadParsingScope() const {
    return type_ == kMaybeAsyncArrowParameterDeclaration;
  }

 private:
  friend class AccumulationScope<Types>;
  friend class ExpressionParsingScope<Types>;

  ArrowHeadParsingScope<Types>* AsArrowHeadParsingScope() {
    DCHECK(IsArrowHeadParsingScope());
    return static_cast<ArrowHeadParsingScope<Types>*>(this);
  }

  bool IsArrowHeadParsingScope() const {
    return IsInRange(type_, kMaybeArrowParameterDeclaration,
                     kMaybeAsyncArrowParameterDeclaration);
  }
  bool IsCertainlyPattern() const { return IsCertainlyDeclaration(); }
  bool CanBeParameterDeclaration() const {
    return IsInRange(type_, kMaybeArrowParameterDeclaration,
                     kParameterDeclaration);
  }
  bool IsCertainlyParameterDeclaration() const {
    return type_ == kParameterDeclaration;
  }
  bool IsLexicalDeclaration() const { return type_ == kLexicalDeclaration; }

  ParserT* parser_;
  ExpressionScope<Types>* parent_;
  ScopeType type_;
  bool has_possible_parameter_in_scope_chain_;

  DISALLOW_COPY_AND_ASSIGN(ExpressionScope);
};

// Used to unambiguously parse var, let, const declarations.
template <typename Types>
class VariableDeclarationParsingScope : public ExpressionScope<Types> {
 public:
  typedef typename Types::Impl ParserT;
  typedef class ExpressionScope<Types> ExpressionScopeT;
  typedef typename ExpressionScopeT::ScopeType ScopeType;

  VariableDeclarationParsingScope(ParserT* parser, VariableMode mode)
      : ExpressionScopeT(parser, IsLexicalVariableMode(mode)
                                     ? ExpressionScopeT::kLexicalDeclaration
                                     : ExpressionScopeT::kVarDeclaration),
        mode_(mode) {}

 private:
  VariableMode mode_;

  DISALLOW_COPY_AND_ASSIGN(VariableDeclarationParsingScope);
};

template <typename Types>
class ParameterDeclarationParsingScope : public ExpressionScope<Types> {
 public:
  typedef typename Types::Impl ParserT;
  typedef class ExpressionScope<Types> ExpressionScopeT;
  typedef typename ExpressionScopeT::ScopeType ScopeType;

  explicit ParameterDeclarationParsingScope(ParserT* parser)
      : ExpressionScopeT(parser, ExpressionScopeT::kParameterDeclaration) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ParameterDeclarationParsingScope);
};

// Parsing expressions is always ambiguous between at least left-hand-side and
// right-hand-side of assignments. This class is used to keep track of errors
// relevant for either side until it is clear what was being parsed.
// The class also keeps track of all variable proxies that are created while the
// scope was active. If the scope is an expression, the variable proxies will be
// added to the unresolved list. Otherwise they are declarations and aren't
// added. The list is also used to mark the variables as assigned in case we are
// parsing an assignment expression.
template <typename Types>
class ExpressionParsingScope : public ExpressionScope<Types> {
 public:
  typedef typename Types::Impl ParserT;
  typedef typename Types::Expression ExpressionT;
  typedef class ExpressionScope<Types> ExpressionScopeT;
  typedef typename ExpressionScopeT::ScopeType ScopeType;

  ExpressionParsingScope(ParserT* parser,
                         ScopeType type = ExpressionScopeT::kExpression)
      : ExpressionScopeT(parser, type),
        variable_list_(parser->variable_buffer()),
        has_async_arrow_in_scope_chain_(
            type == ExpressionScopeT::kMaybeAsyncArrowParameterDeclaration ||
            (this->parent() && this->parent()->CanBeExpression() &&
             this->parent()
                 ->AsExpressionParsingScope()
                 ->has_async_arrow_in_scope_chain_)) {
    DCHECK(this->CanBeExpression());
    clear(kExpressionIndex);
    clear(kPatternIndex);
  }

  void RecordAsyncArrowParametersError(const Scanner::Location& loc,
                                       MessageTemplate message) {
    for (ExpressionScopeT* scope = this; scope != nullptr;
         scope = scope->parent()) {
      if (!has_async_arrow_in_scope_chain_) break;
      if (scope->type_ ==
          ExpressionScopeT::kMaybeAsyncArrowParameterDeclaration) {
        scope->AsArrowHeadParsingScope()->RecordDeclarationError(loc, message);
      }
    }
  }

  ~ExpressionParsingScope() { DCHECK(this->has_error() || verified_); }

  ExpressionT ValidateAndRewriteReference(ExpressionT expression, int beg_pos,
                                          int end_pos) {
    if (V8_LIKELY(this->parser()->IsAssignableIdentifier(expression))) {
      MarkIdentifierAsAssigned();
      this->mark_verified();
      return expression;
    } else if (V8_LIKELY(expression->IsProperty())) {
      ValidateExpression();
      return expression;
    }
    this->mark_verified();
    return this->parser()->RewriteInvalidReferenceExpression(
        expression, beg_pos, end_pos, MessageTemplate::kInvalidLhsInFor,
        kSyntaxError);
  }

  void RecordExpressionError(const Scanner::Location& loc,
                             MessageTemplate message) {
    Record(kExpressionIndex, loc, message);
  }

  void RecordPatternError(const Scanner::Location& loc,
                          MessageTemplate message) {
    Record(kPatternIndex, loc, message);
  }

  void ValidateExpression() { Validate(kExpressionIndex); }

  void ValidatePattern(ExpressionT expression, int begin, int end) {
    Validate(kPatternIndex);
    if (expression->is_parenthesized()) {
      ExpressionScopeT::Report(Scanner::Location(begin, end),
                               MessageTemplate::kInvalidDestructuringTarget);
    }
    for (int i = 0; i < variable_list_.length(); i++) {
      variable_list_.at(i)->set_is_assigned();
    }
  }

  void ClearExpressionError() {
    DCHECK(verified_);
#ifdef DEBUG
    verified_ = false;
#endif
    clear(kExpressionIndex);
  }

  void TrackVariable(VariableProxy* variable) {
    if (!this->CanBeDeclaration()) {
      this->parser()->scope()->AddUnresolved(variable);
    }
    variable_list_.Add(variable);
  }

  void MarkIdentifierAsAssigned() {
    // It's possible we're parsing a syntax error. In that case it's not
    // guaranteed that there's a variable in the list.
    if (variable_list_.length() == 0) return;
    variable_list_.at(variable_list_.length() - 1)->set_is_assigned();
  }

 protected:
  bool is_verified() const {
#ifdef DEBUG
    return verified_;
#else
    return false;
#endif
  }

  void ValidatePattern() { Validate(kPatternIndex); }

  ScopedPtrList<VariableProxy>* variable_list() { return &variable_list_; }

 private:
  friend class AccumulationScope<Types>;

  enum ErrorNumber : uint8_t {
    kExpressionIndex = 0,
    kPatternIndex = 1,
    kNumberOfErrors = 2,
  };
  void clear(int index) {
    messages_[index] = MessageTemplate::kNone;
    locations_[index] = Scanner::Location::invalid();
  }
  bool is_valid(int index) const { return !locations_[index].IsValid(); }
  void Record(int index, const Scanner::Location& loc,
              MessageTemplate message) {
    DCHECK_IMPLIES(!this->has_error(), loc.IsValid());
    if (!is_valid(index)) return;
    messages_[index] = message;
    locations_[index] = loc;
  }
  void Validate(int index) {
    DCHECK(!this->is_verified());
    if (!is_valid(index)) Report(index);
    this->mark_verified();
  }
  void Report(int index) const {
    ExpressionScopeT::Report(locations_[index], messages_[index]);
  }

  // Debug verification to make sure every scope is validated exactly once.
  void mark_verified() {
#ifdef DEBUG
    verified_ = true;
#endif
  }
  void clear_verified() {
#ifdef DEBUG
    verified_ = false;
#endif
  }
#ifdef DEBUG
  bool verified_ = false;
#endif

  ScopedPtrList<VariableProxy> variable_list_;
  MessageTemplate messages_[kNumberOfErrors];
  Scanner::Location locations_[kNumberOfErrors];
  bool has_async_arrow_in_scope_chain_;

  DISALLOW_COPY_AND_ASSIGN(ExpressionParsingScope);
};

// This class is used to parse multiple ambiguous expressions and declarations
// in the same scope. E.g., in async(X,Y,Z) or [X,Y,Z], X and Y and Z will all
// be parsed in the respective outer ArrowHeadParsingScope and
// ExpressionParsingScope. It provides a clean error state in the underlying
// scope to parse the individual expressions, while keeping track of the
// expression and pattern errors since the start. The AccumulationScope is only
// used to keep track of the errors so far, and the underlying ExpressionScope
// keeps being used as the expression_scope(). If the expression_scope() isn't
// ambiguous, this class does not do anything.
template <typename Types>
class AccumulationScope {
 public:
  typedef typename Types::Impl ParserT;

  static const int kNumberOfErrors =
      ExpressionParsingScope<Types>::kNumberOfErrors;
  explicit AccumulationScope(ExpressionScope<Types>* scope) : scope_(nullptr) {
    if (!scope->CanBeExpression()) return;
    scope_ = scope->AsExpressionParsingScope();
    for (int i = 0; i < kNumberOfErrors; i++) {
      // If the underlying scope is already invalid at the start, stop
      // accumulating. That means an error was found outside of an
      // accumulating path.
      if (!scope_->is_valid(i)) {
        scope_ = nullptr;
        break;
      }
      copy(i);
    }
  }

  // Merge errors from the underlying ExpressionParsingScope into this scope.
  // Only keeps the first error across all accumulate calls, and removes the
  // error from the underlying scope.
  void Accumulate() {
    if (scope_ == nullptr) return;
    DCHECK(!scope_->is_verified());
    for (int i = 0; i < kNumberOfErrors; i++) {
      if (!locations_[i].IsValid()) copy(i);
      scope_->clear(i);
    }
  }

  // This is called instead of Accumulate in case the parsed member is already
  // known to be an expression. In that case we don't need to accumulate the
  // expression but rather validate it immediately. We also ignore the pattern
  // error since the parsed member is known to not be a pattern. This is
  // necessary for "{x:1}.y" parsed as part of an assignment pattern. {x:1} will
  // record a pattern error, but "{x:1}.y" is actually a valid as part of an
  // assignment pattern since it's a property access.
  void ValidateExpression() {
    if (scope_ == nullptr) return;
    DCHECK(!scope_->is_verified());
    scope_->ValidateExpression();
    DCHECK(scope_->is_verified());
    scope_->clear(ExpressionParsingScope<Types>::kPatternIndex);
#ifdef DEBUG
    scope_->clear_verified();
#endif
  }

  ~AccumulationScope() {
    if (scope_ == nullptr) return;
    Accumulate();
    for (int i = 0; i < kNumberOfErrors; i++) copy_back(i);
  }

 private:
  void copy(int entry) {
    messages_[entry] = scope_->messages_[entry];
    locations_[entry] = scope_->locations_[entry];
  }

  void copy_back(int entry) {
    if (!locations_[entry].IsValid()) return;
    scope_->messages_[entry] = messages_[entry];
    scope_->locations_[entry] = locations_[entry];
  }

  ExpressionParsingScope<Types>* scope_;
  MessageTemplate messages_[2];
  Scanner::Location locations_[2];

  DISALLOW_COPY_AND_ASSIGN(AccumulationScope);
};

// The head of an arrow function is ambiguous between expression, assignment
// pattern and declaration. This keeps track of the additional declaration
// error and allows the scope to be validated as a declaration rather than an
// expression or a pattern.
template <typename Types>
class ArrowHeadParsingScope : public ExpressionParsingScope<Types> {
 public:
  typedef typename Types::Impl ParserT;
  typedef typename ExpressionScope<Types>::ScopeType ScopeType;

  ArrowHeadParsingScope(ParserT* parser, FunctionKind kind)
      : ExpressionParsingScope<Types>(
            parser,
            kind == FunctionKind::kArrowFunction
                ? ExpressionScope<Types>::kMaybeArrowParameterDeclaration
                : ExpressionScope<
                      Types>::kMaybeAsyncArrowParameterDeclaration) {
    DCHECK(kind == FunctionKind::kAsyncArrowFunction ||
           kind == FunctionKind::kArrowFunction);
    DCHECK(this->CanBeDeclaration());
    DCHECK(!this->IsCertainlyDeclaration());
  }

  void ValidateExpression() {
    // Turns out this is not an arrow head. Clear any possible tracked strict
    // parameter errors, and reinterpret tracked variables as unresolved
    // references.
    this->parser()->next_arrow_function_info_.ClearStrictParameterError();
    ExpressionParsingScope<Types>::ValidateExpression();
    for (int i = 0; i < this->variable_list()->length(); i++) {
      this->parser()->scope()->AddUnresolved(this->variable_list()->at(i));
    }
  }

  DeclarationScope* ValidateAndCreateScope() {
    DCHECK(!this->is_verified());
    if (declaration_error_location.IsValid()) {
      ExpressionScope<Types>::Report(declaration_error_location,
                                     declaration_error_message);
    }
    this->ValidatePattern();

    DeclarationScope* result = this->parser()->NewFunctionScope(kind());
    if (!has_simple_parameter_list_) result->SetHasNonSimpleParameters();
    // TODO(verwaest): Add declarations.
    return result;
  }

  void RecordDeclarationError(const Scanner::Location& loc,
                              MessageTemplate message) {
    DCHECK_IMPLIES(!this->has_error(), loc.IsValid());
    declaration_error_location = loc;
    declaration_error_message = message;
  }

  void RecordNonSimpleParameter() { has_simple_parameter_list_ = false; }

 private:
  FunctionKind kind() const {
    return this->IsAsyncArrowHeadParsingScope()
               ? FunctionKind::kAsyncArrowFunction
               : FunctionKind::kArrowFunction;
  }

  Scanner::Location declaration_error_location = Scanner::Location::invalid();
  MessageTemplate declaration_error_message = MessageTemplate::kNone;
  bool has_simple_parameter_list_ = true;

  DISALLOW_COPY_AND_ASSIGN(ArrowHeadParsingScope);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_EXPRESSION_SCOPE_H_
