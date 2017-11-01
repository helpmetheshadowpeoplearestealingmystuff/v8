// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_STRING_GEN_H_
#define V8_BUILTINS_BUILTINS_STRING_GEN_H_

#include "src/code-stub-assembler.h"

namespace v8 {
namespace internal {

class StringBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit StringBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  // ES#sec-getsubstitution
  Node* GetSubstitution(Node* context, Node* subject_string,
                        Node* match_start_index, Node* match_end_index,
                        Node* replace_string);
  void StringEqual_Core(Node* context, Node* lhs, Node* lhs_instance_type,
                        Node* rhs, Node* rhs_instance_type, Node* length,
                        Label* if_equal, Label* if_not_equal,
                        Label* if_indirect);

 protected:
  void StringEqual_Loop(Node* lhs, Node* lhs_instance_type,
                        MachineType lhs_type, Node* rhs,
                        Node* rhs_instance_type, MachineType rhs_type,
                        Node* length, Label* if_equal, Label* if_not_equal);
  Node* DirectStringData(Node* string, Node* string_instance_type);

  void DispatchOnStringEncodings(Node* const lhs_instance_type,
                                 Node* const rhs_instance_type,
                                 Label* if_one_one, Label* if_one_two,
                                 Label* if_two_one, Label* if_two_two);

  template <typename SubjectChar, typename PatternChar>
  Node* CallSearchStringRaw(Node* const subject_ptr, Node* const subject_length,
                            Node* const search_ptr, Node* const search_length,
                            Node* const start_position);

  Node* PointerToStringDataAtIndex(Node* const string_data, Node* const index,
                                   String::Encoding encoding);

  // substr and slice have a common way of handling the {start} argument.
  void ConvertAndBoundsCheckStartArgument(Node* context, Variable* var_start,
                                          Node* start, Node* string_length);

  void GenerateStringEqual(Node* context, Node* left, Node* right);
  void GenerateStringRelationalComparison(Node* context, Node* left,
                                          Node* right, Operation op);

  TNode<Smi> ToSmiBetweenZeroAnd(SloppyTNode<Context> context,
                                 SloppyTNode<Object> value,
                                 SloppyTNode<Smi> limit);

  TNode<Uint32T> LoadSurrogatePairAt(SloppyTNode<String> string,
                                     SloppyTNode<Smi> length,
                                     SloppyTNode<Smi> index,
                                     UnicodeEncoding encoding);

  void StringIndexOf(Node* const subject_string, Node* const search_string,
                     Node* const position, std::function<void(Node*)> f_return);

  Node* IndexOfDollarChar(Node* const context, Node* const string);

  void RequireObjectCoercible(Node* const context, Node* const value,
                              const char* method_name);

  Node* SmiIsNegative(Node* const value) {
    return SmiLessThan(value, SmiConstant(0));
  }

  // Implements boilerplate logic for {match, split, replace, search} of the
  // form:
  //
  //  if (!IS_NULL_OR_UNDEFINED(object)) {
  //    var maybe_function = object[symbol];
  //    if (!IS_UNDEFINED(maybe_function)) {
  //      return %_Call(maybe_function, ...);
  //    }
  //  }
  //
  // Contains fast paths for Smi and RegExp objects.
  typedef std::function<Node*()> NodeFunction0;
  typedef std::function<Node*(Node* fn)> NodeFunction1;
  void MaybeCallFunctionAtSymbol(Node* const context, Node* const object,
                                 Handle<Symbol> symbol,
                                 const NodeFunction0& regexp_call,
                                 const NodeFunction1& generic_call,
                                 CodeStubArguments* args = nullptr);
};

class StringIncludesIndexOfAssembler : public StringBuiltinsAssembler {
 public:
  explicit StringIncludesIndexOfAssembler(compiler::CodeAssemblerState* state)
      : StringBuiltinsAssembler(state) {}

 protected:
  enum SearchVariant { kIncludes, kIndexOf };

  void Generate(SearchVariant variant);
};

class StringTrimAssembler : public StringBuiltinsAssembler {
 public:
  explicit StringTrimAssembler(compiler::CodeAssemblerState* state)
      : StringBuiltinsAssembler(state) {}

  void GotoIfNotWhiteSpaceOrLineTerminator(Node* const char_code,
                                           Label* const if_not_whitespace);

 protected:
  void Generate(String::TrimMode mode, const char* method);

  void ScanForNonWhiteSpaceOrLineTerminator(Node* const string_data,
                                            Node* const string_data_offset,
                                            Node* const is_stringonebyte,
                                            Variable* const var_index,
                                            Node* const end, int increment,
                                            Label* const if_none_found);

  void BuildLoop(Variable* const var_index, Node* const end, int increment,
                 Label* const if_none_found, Label* const out,
                 std::function<Node*(Node*)> get_character);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_STRING_GEN_H_
