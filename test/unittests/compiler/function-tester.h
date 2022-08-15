// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNITTESTS_COMPILER_FUNCTION_TESTER_H_
#define V8_UNITTESTS_COMPILER_FUNCTION_TESTER_H_

#include "src/compiler/graph.h"
#include "src/compiler/js-heap-broker.h"
#include "src/execution/execution.h"
#include "src/handles/handles.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {

namespace compiler {

class FunctionTester {
 public:
  explicit FunctionTester(Isolate* i_isolate, const char* source,
                          uint32_t flags = 0);

  FunctionTester(Isolate* i_isolate, Graph* graph, int param_count);

  FunctionTester(Isolate* i_isolate, Handle<Code> code, int param_count);

  // Assumes VoidDescriptor call interface.
  explicit FunctionTester(Isolate* i_isolate, Handle<Code> code);

  Isolate* isolate;
  CanonicalHandleScope canonical;
  Handle<JSFunction> function;

  MaybeHandle<Object> Call() {
    return Execution::Call(isolate, function, undefined(), 0, nullptr);
  }

  template <typename Arg1, typename... Args>
  MaybeHandle<Object> Call(Arg1 arg1, Args... args) {
    const int nof_args = sizeof...(Args) + 1;
    Handle<Object> call_args[] = {arg1, args...};
    return Execution::Call(isolate, function, undefined(), nof_args, call_args);
  }

  template <typename T, typename... Args>
  Handle<T> CallChecked(Args... args) {
    Handle<Object> result = Call(args...).ToHandleChecked();
    return Handle<T>::cast(result);
  }

  void CheckThrows(Handle<Object> a);
  void CheckThrows(Handle<Object> a, Handle<Object> b);
  v8::Local<v8::Message> CheckThrowsReturnMessage(Handle<Object> a,
                                                  Handle<Object> b);
  void CheckCall(Handle<Object> expected, Handle<Object> a, Handle<Object> b,
                 Handle<Object> c, Handle<Object> d);

  void CheckCall(Handle<Object> expected, Handle<Object> a, Handle<Object> b,
                 Handle<Object> c) {
    return CheckCall(expected, a, b, c, undefined());
  }

  void CheckCall(Handle<Object> expected, Handle<Object> a, Handle<Object> b) {
    return CheckCall(expected, a, b, undefined());
  }

  void CheckCall(Handle<Object> expected, Handle<Object> a) {
    CheckCall(expected, a, undefined());
  }

  void CheckCall(Handle<Object> expected) { CheckCall(expected, undefined()); }

  void CheckCall(double expected, double a, double b) {
    CheckCall(NewNumber(expected), NewNumber(a), NewNumber(b));
  }

  void CheckTrue(Handle<Object> a) { CheckCall(true_value(), a); }

  void CheckTrue(Handle<Object> a, Handle<Object> b) {
    CheckCall(true_value(), a, b);
  }

  void CheckTrue(Handle<Object> a, Handle<Object> b, Handle<Object> c) {
    CheckCall(true_value(), a, b, c);
  }

  void CheckTrue(Handle<Object> a, Handle<Object> b, Handle<Object> c,
                 Handle<Object> d) {
    CheckCall(true_value(), a, b, c, d);
  }

  void CheckTrue(double a, double b) {
    CheckCall(true_value(), NewNumber(a), NewNumber(b));
  }

  void CheckFalse(Handle<Object> a) { CheckCall(false_value(), a); }

  void CheckFalse(Handle<Object> a, Handle<Object> b) {
    CheckCall(false_value(), a, b);
  }

  void CheckFalse(double a, double b) {
    CheckCall(false_value(), NewNumber(a), NewNumber(b));
  }

  Handle<JSFunction> NewFunction(const char* source);
  Handle<JSObject> NewObject(const char* source);

  Handle<String> NewString(const char* string);
  Handle<Object> NewNumber(double value);
  Handle<Object> infinity();
  Handle<Object> minus_infinity();
  Handle<Object> nan();
  Handle<Object> undefined();
  Handle<Object> null();
  Handle<Object> true_value();
  Handle<Object> false_value();

 private:
  uint32_t flags_;

  Handle<JSFunction> Compile(Handle<JSFunction> function);
  std::string BuildFunction(int param_count) {
    std::string function_string = "(function(";
    if (param_count > 0) {
      function_string += 'a';
      for (int i = 1; i < param_count; i++) {
        function_string += ',';
        function_string += static_cast<char>('a' + i);
      }
    }
    function_string += "){})";
    return function_string;
  }

  // Compile the given machine graph instead of the source of the function
  // and replace the JSFunction's code with the result.
  Handle<JSFunction> CompileGraph(Graph* graph);

  // Takes a JSFunction and runs it through the test version of the optimizing
  // pipeline, allocating the temporary compilation artifacts in a given Zone.
  // For possible {flags} values, look at OptimizedCompilationInfo::Flag.  If
  // {out_broker} is not nullptr, returns the JSHeapBroker via that
  // (transferring ownership to the caller).
  Handle<JSFunction> Optimize(
      Handle<JSFunction> function, Zone* zone, uint32_t flags,
      std::unique_ptr<compiler::JSHeapBroker>* out_broker = nullptr);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_UNITTESTS_COMPILER_FUNCTION_TESTER_H_
