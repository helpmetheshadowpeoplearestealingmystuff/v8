// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_AST_PRETTYPRINTER_H_
#define V8_AST_PRETTYPRINTER_H_

#include "src/allocation.h"
#include "src/ast/ast.h"
#include "src/base/compiler-specific.h"

namespace v8 {
namespace internal {

class CallPrinter : public AstVisitor {
 public:
  explicit CallPrinter(Isolate* isolate, bool is_builtin);
  virtual ~CallPrinter();

  // The following routine prints the node with position |position| into a
  // string. The result string is alive as long as the CallPrinter is alive.
  const char* Print(FunctionLiteral* program, int position);

  void PRINTF_FORMAT(2, 3) Print(const char* format, ...);

  void Find(AstNode* node, bool print = false);

// Individual nodes
#define DECLARE_VISIT(type) void Visit##type(type* node) override;
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

 private:
  void Init();
  Isolate* isolate_;
  char* output_;  // output string buffer
  int size_;      // output_ size
  int pos_;       // current printing position
  int position_;  // position of ast node to print
  bool found_;
  bool done_;
  bool is_builtin_;

  DEFINE_AST_VISITOR_SUBCLASS_MEMBERS();

 protected:
  void PrintLiteral(Object* value, bool quote);
  void PrintLiteral(const AstRawString* value, bool quote);
  void FindStatements(ZoneList<Statement*>* statements);
  void FindArguments(ZoneList<Expression*>* arguments);
};


#ifdef DEBUG

class PrettyPrinter: public AstVisitor {
 public:
  explicit PrettyPrinter(Isolate* isolate);
  virtual ~PrettyPrinter();

  // The following routines print a node into a string.
  // The result string is alive as long as the PrettyPrinter is alive.
  const char* Print(AstNode* node);
  const char* PrintExpression(FunctionLiteral* program);
  const char* PrintProgram(FunctionLiteral* program);

  void PRINTF_FORMAT(2, 3) Print(const char* format, ...);

  // Print a node to stdout.
  static void PrintOut(Isolate* isolate, AstNode* node);

  // Individual nodes
#define DECLARE_VISIT(type) void Visit##type(type* node) override;
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

 private:
  Isolate* isolate_;
  char* output_;  // output string buffer
  int size_;  // output_ size
  int pos_;  // current printing position

 protected:
  void Init();
  const char* Output() const { return output_; }

  virtual void PrintStatements(ZoneList<Statement*>* statements);
  void PrintLabels(ZoneList<const AstRawString*>* labels);
  virtual void PrintArguments(ZoneList<Expression*>* arguments);
  void PrintLiteral(Handle<Object> value, bool quote);
  void PrintLiteral(const AstRawString* value, bool quote);
  void PrintParameters(Scope* scope);
  void PrintDeclarations(ZoneList<Declaration*>* declarations);
  void PrintFunctionLiteral(FunctionLiteral* function);
  void PrintCaseClause(CaseClause* clause);
  void PrintObjectLiteralProperty(ObjectLiteralProperty* property);

  DEFINE_AST_VISITOR_SUBCLASS_MEMBERS();
};


// Prints the AST structure
class AstPrinter: public PrettyPrinter {
 public:
  explicit AstPrinter(Isolate* isolate);
  virtual ~AstPrinter();

  const char* PrintProgram(FunctionLiteral* program);

  // Print a node to stdout.
  static void PrintOut(Isolate* isolate, AstNode* node);

  // Individual nodes
#define DECLARE_VISIT(type) virtual void Visit##type(type* node);
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

 private:
  friend class IndentedScope;
  void PrintIndented(const char* txt);
  void PrintIndentedVisit(const char* s, AstNode* node);

  void PrintStatements(ZoneList<Statement*>* statements);
  void PrintDeclarations(ZoneList<Declaration*>* declarations);
  void PrintParameters(Scope* scope);
  void PrintArguments(ZoneList<Expression*>* arguments);
  void PrintCaseClause(CaseClause* clause);
  void PrintLiteralIndented(const char* info, Handle<Object> value, bool quote);
  void PrintLiteralWithModeIndented(const char* info,
                                    Variable* var,
                                    Handle<Object> value);
  void PrintLabelsIndented(ZoneList<const AstRawString*>* labels);
  void PrintProperties(ZoneList<ObjectLiteral::Property*>* properties);

  void inc_indent() { indent_++; }
  void dec_indent() { indent_--; }

  int indent_;
};

#endif  // DEBUG

}  // namespace internal
}  // namespace v8

#endif  // V8_AST_PRETTYPRINTER_H_
