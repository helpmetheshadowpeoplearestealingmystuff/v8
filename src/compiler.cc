// Copyright 2009 the V8 project authors. All rights reserved.
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

#include "v8.h"

#include "bootstrapper.h"
#include "codegen-inl.h"
#include "compilation-cache.h"
#include "compiler.h"
#include "debug.h"
#include "fast-codegen.h"
#include "full-codegen.h"
#include "oprofile-agent.h"
#include "rewriter.h"
#include "scopes.h"
#include "usage-analyzer.h"

namespace v8 {
namespace internal {


static Handle<Code> MakeCode(Handle<Context> context, CompilationInfo* info) {
  FunctionLiteral* function = info->function();
  ASSERT(function != NULL);
  // Rewrite the AST by introducing .result assignments where needed.
  if (!Rewriter::Process(function) || !AnalyzeVariableUsage(function)) {
    // Signal a stack overflow by returning a null handle.  The stack
    // overflow exception will be thrown by the caller.
    return Handle<Code>::null();
  }

  {
    // Compute top scope and allocate variables. For lazy compilation
    // the top scope only contains the single lazily compiled function,
    // so this doesn't re-allocate variables repeatedly.
    HistogramTimerScope timer(&Counters::variable_allocation);
    Scope* top = info->scope();
    while (top->outer_scope() != NULL) top = top->outer_scope();
    top->AllocateVariables(context);
  }

#ifdef DEBUG
  if (Bootstrapper::IsActive() ?
      FLAG_print_builtin_scopes :
      FLAG_print_scopes) {
    info->scope()->Print();
  }
#endif

  // Optimize the AST.
  if (!Rewriter::Optimize(function)) {
    // Signal a stack overflow by returning a null handle.  The stack
    // overflow exception will be thrown by the caller.
    return Handle<Code>::null();
  }

  // Generate code and return it.  Code generator selection is governed by
  // which backends are enabled and whether the function is considered
  // run-once code or not:
  //
  //  --full-compiler enables the dedicated backend for code we expect to be
  //    run once
  //  --fast-compiler enables a speculative optimizing backend (for
  //    non-run-once code)
  //
  // The normal choice of backend can be overridden with the flags
  // --always-full-compiler and --always-fast-compiler, which are mutually
  // incompatible.
  CHECK(!FLAG_always_full_compiler || !FLAG_always_fast_compiler);

  Handle<SharedFunctionInfo> shared = info->shared_info();
  bool is_run_once = (shared.is_null())
      ? info->scope()->is_global_scope()
      : (shared->is_toplevel() || shared->try_full_codegen());

  if (FLAG_always_full_compiler || (FLAG_full_compiler && is_run_once)) {
    FullCodeGenSyntaxChecker checker;
    checker.Check(function);
    if (checker.has_supported_syntax()) {
      return FullCodeGenerator::MakeCode(info);
    }
  } else if (FLAG_always_fast_compiler ||
             (FLAG_fast_compiler && !is_run_once)) {
    FastCodeGenSyntaxChecker checker;
    checker.Check(info);
    if (checker.has_supported_syntax()) {
      return FastCodeGenerator::MakeCode(info);
    }
  }

  return CodeGenerator::MakeCode(info);
}


static Handle<JSFunction> MakeFunction(bool is_global,
                                       bool is_eval,
                                       Compiler::ValidationState validate,
                                       Handle<Script> script,
                                       Handle<Context> context,
                                       v8::Extension* extension,
                                       ScriptDataImpl* pre_data) {
  CompilationZoneScope zone_scope(DELETE_ON_EXIT);

  PostponeInterruptsScope postpone;

  ASSERT(!i::Top::global_context().is_null());
  script->set_context_data((*i::Top::global_context())->data());

  bool is_json = (validate == Compiler::VALIDATE_JSON);
#ifdef ENABLE_DEBUGGER_SUPPORT
  if (is_eval || is_json) {
    script->set_compilation_type(
        is_json ? Smi::FromInt(Script::COMPILATION_TYPE_JSON) :
                               Smi::FromInt(Script::COMPILATION_TYPE_EVAL));
    // For eval scripts add information on the function from which eval was
    // called.
    if (is_eval) {
      StackTraceFrameIterator it;
      if (!it.done()) {
        script->set_eval_from_shared(
            JSFunction::cast(it.frame()->function())->shared());
        int offset = static_cast<int>(
            it.frame()->pc() - it.frame()->code()->instruction_start());
        script->set_eval_from_instructions_offset(Smi::FromInt(offset));
      }
    }
  }

  // Notify debugger
  Debugger::OnBeforeCompile(script);
#endif

  // Only allow non-global compiles for eval.
  ASSERT(is_eval || is_global);

  // Build AST.
  FunctionLiteral* lit =
      MakeAST(is_global, script, extension, pre_data, is_json);

  // Check for parse errors.
  if (lit == NULL) {
    ASSERT(Top::has_pending_exception());
    return Handle<JSFunction>::null();
  }

  // Measure how long it takes to do the compilation; only take the
  // rest of the function into account to avoid overlap with the
  // parsing statistics.
  HistogramTimer* rate = is_eval
      ? &Counters::compile_eval
      : &Counters::compile;
  HistogramTimerScope timer(rate);

  // Compile the code.
  CompilationInfo info(lit, script, is_eval);
  Handle<Code> code = MakeCode(context, &info);

  // Check for stack-overflow exceptions.
  if (code.is_null()) {
    Top::StackOverflow();
    return Handle<JSFunction>::null();
  }

#if defined ENABLE_LOGGING_AND_PROFILING || defined ENABLE_OPROFILE_AGENT
  // Log the code generation for the script. Check explicit whether logging is
  // to avoid allocating when not required.
  if (Logger::is_logging() || OProfileAgent::is_enabled()) {
    if (script->name()->IsString()) {
      SmartPointer<char> data =
          String::cast(script->name())->ToCString(DISALLOW_NULLS);
      LOG(CodeCreateEvent(is_eval ? Logger::EVAL_TAG : Logger::SCRIPT_TAG,
                          *code, *data));
      OProfileAgent::CreateNativeCodeRegion(*data,
                                            code->instruction_start(),
                                            code->instruction_size());
    } else {
      LOG(CodeCreateEvent(is_eval ? Logger::EVAL_TAG : Logger::SCRIPT_TAG,
                          *code, ""));
      OProfileAgent::CreateNativeCodeRegion(is_eval ? "Eval" : "Script",
                                            code->instruction_start(),
                                            code->instruction_size());
    }
  }
#endif

  // Allocate function.
  Handle<JSFunction> fun =
      Factory::NewFunctionBoilerplate(lit->name(),
                                      lit->materialized_literal_count(),
                                      code);

  ASSERT_EQ(RelocInfo::kNoPosition, lit->function_token_position());
  Compiler::SetFunctionInfo(fun, lit, true, script);

  // Hint to the runtime system used when allocating space for initial
  // property space by setting the expected number of properties for
  // the instances of the function.
  SetExpectedNofPropertiesFromEstimate(fun, lit->expected_property_count());

#ifdef ENABLE_DEBUGGER_SUPPORT
  // Notify debugger
  Debugger::OnAfterCompile(script, fun);
#endif

  return fun;
}


static StaticResource<SafeStringInputBuffer> safe_string_input_buffer;


Handle<JSFunction> Compiler::Compile(Handle<String> source,
                                     Handle<Object> script_name,
                                     int line_offset, int column_offset,
                                     v8::Extension* extension,
                                     ScriptDataImpl* input_pre_data) {
  int source_length = source->length();
  Counters::total_load_size.Increment(source_length);
  Counters::total_compile_size.Increment(source_length);

  // The VM is in the COMPILER state until exiting this function.
  VMState state(COMPILER);

  // Do a lookup in the compilation cache but not for extensions.
  Handle<JSFunction> result;
  if (extension == NULL) {
    result = CompilationCache::LookupScript(source,
                                            script_name,
                                            line_offset,
                                            column_offset);
  }

  if (result.is_null()) {
    // No cache entry found. Do pre-parsing and compile the script.
    ScriptDataImpl* pre_data = input_pre_data;
    if (pre_data == NULL && source_length >= FLAG_min_preparse_length) {
      Access<SafeStringInputBuffer> buf(&safe_string_input_buffer);
      buf->Reset(source.location());
      pre_data = PreParse(source, buf.value(), extension);
    }

    // Create a script object describing the script to be compiled.
    Handle<Script> script = Factory::NewScript(source);
    if (!script_name.is_null()) {
      script->set_name(*script_name);
      script->set_line_offset(Smi::FromInt(line_offset));
      script->set_column_offset(Smi::FromInt(column_offset));
    }

    // Compile the function and add it to the cache.
    result = MakeFunction(true,
                          false,
                          DONT_VALIDATE_JSON,
                          script,
                          Handle<Context>::null(),
                          extension,
                          pre_data);
    if (extension == NULL && !result.is_null()) {
      CompilationCache::PutScript(source, result);
    }

    // Get rid of the pre-parsing data (if necessary).
    if (input_pre_data == NULL && pre_data != NULL) {
      delete pre_data;
    }
  }

  if (result.is_null()) Top::ReportPendingMessages();
  return result;
}


Handle<JSFunction> Compiler::CompileEval(Handle<String> source,
                                         Handle<Context> context,
                                         bool is_global,
                                         ValidationState validate) {
  // Note that if validation is required then no path through this
  // function is allowed to return a value without validating that
  // the input is legal json.

  int source_length = source->length();
  Counters::total_eval_size.Increment(source_length);
  Counters::total_compile_size.Increment(source_length);

  // The VM is in the COMPILER state until exiting this function.
  VMState state(COMPILER);

  // Do a lookup in the compilation cache; if the entry is not there,
  // invoke the compiler and add the result to the cache.  If we're
  // evaluating json we bypass the cache since we can't be sure a
  // potential value in the cache has been validated.
  Handle<JSFunction> result;
  if (validate == DONT_VALIDATE_JSON)
    result = CompilationCache::LookupEval(source, context, is_global);

  if (result.is_null()) {
    // Create a script object describing the script to be compiled.
    Handle<Script> script = Factory::NewScript(source);
    result = MakeFunction(is_global,
                          true,
                          validate,
                          script,
                          context,
                          NULL,
                          NULL);
    if (!result.is_null() && validate != VALIDATE_JSON) {
      // For json it's unlikely that we'll ever see exactly the same
      // string again so we don't use the compilation cache.
      CompilationCache::PutEval(source, context, is_global, result);
    }
  }

  return result;
}


bool Compiler::CompileLazy(CompilationInfo* info) {
  CompilationZoneScope zone_scope(DELETE_ON_EXIT);

  // The VM is in the COMPILER state until exiting this function.
  VMState state(COMPILER);

  PostponeInterruptsScope postpone;

  // Compute name, source code and script data.
  Handle<SharedFunctionInfo> shared = info->shared_info();
  Handle<String> name(String::cast(shared->name()));

  int start_position = shared->start_position();
  int end_position = shared->end_position();
  bool is_expression = shared->is_expression();
  Counters::total_compile_size.Increment(end_position - start_position);

  // Generate the AST for the lazily compiled function. The AST may be
  // NULL in case of parser stack overflow.
  FunctionLiteral* lit = MakeLazyAST(info->script(),
                                     name,
                                     start_position,
                                     end_position,
                                     is_expression);

  // Check for parse errors.
  if (lit == NULL) {
    ASSERT(Top::has_pending_exception());
    return false;
  }
  info->set_function(lit);

  // Measure how long it takes to do the lazy compilation; only take
  // the rest of the function into account to avoid overlap with the
  // lazy parsing statistics.
  HistogramTimerScope timer(&Counters::compile_lazy);

  // Compile the code.
  Handle<Code> code = MakeCode(Handle<Context>::null(), info);

  // Check for stack-overflow exception.
  if (code.is_null()) {
    Top::StackOverflow();
    return false;
  }

#if defined ENABLE_LOGGING_AND_PROFILING || defined ENABLE_OPROFILE_AGENT
  // Log the code generation. If source information is available include script
  // name and line number. Check explicit whether logging is enabled as finding
  // the line number is not for free.
  if (Logger::is_logging() || OProfileAgent::is_enabled()) {
    Handle<String> func_name(name->length() > 0 ?
                             *name : shared->inferred_name());
    Handle<Script> script = info->script();
    if (script->name()->IsString()) {
      int line_num = GetScriptLineNumber(script, start_position) + 1;
      LOG(CodeCreateEvent(Logger::LAZY_COMPILE_TAG, *code, *func_name,
                          String::cast(script->name()), line_num));
      OProfileAgent::CreateNativeCodeRegion(*func_name,
                                            String::cast(script->name()),
                                            line_num,
                                            code->instruction_start(),
                                            code->instruction_size());
    } else {
      LOG(CodeCreateEvent(Logger::LAZY_COMPILE_TAG, *code, *func_name));
      OProfileAgent::CreateNativeCodeRegion(*func_name,
                                            code->instruction_start(),
                                            code->instruction_size());
    }
  }
#endif

  // Update the shared function info with the compiled code.
  shared->set_code(*code);

  // Set the expected number of properties for instances.
  SetExpectedNofPropertiesFromEstimate(shared, lit->expected_property_count());

  // Set the optimication hints after performing lazy compilation, as these are
  // not set when the function is set up as a lazily compiled function.
  shared->SetThisPropertyAssignmentsInfo(
      lit->has_only_simple_this_property_assignments(),
      *lit->this_property_assignments());

  // Check the function has compiled code.
  ASSERT(shared->is_compiled());
  return true;
}


Handle<JSFunction> Compiler::BuildBoilerplate(FunctionLiteral* literal,
                                              Handle<Script> script,
                                              AstVisitor* caller) {
#ifdef DEBUG
  // We should not try to compile the same function literal more than
  // once.
  literal->mark_as_compiled();
#endif

  // Determine if the function can be lazily compiled. This is
  // necessary to allow some of our builtin JS files to be lazily
  // compiled. These builtins cannot be handled lazily by the parser,
  // since we have to know if a function uses the special natives
  // syntax, which is something the parser records.
  bool allow_lazy = literal->AllowsLazyCompilation();

  // Generate code
  Handle<Code> code;
  if (FLAG_lazy && allow_lazy) {
    code = ComputeLazyCompile(literal->num_parameters());
  } else {
    // The bodies of function literals have not yet been visited by
    // the AST optimizer/analyzer.
    if (!Rewriter::Optimize(literal)) {
      return Handle<JSFunction>::null();
    }

    // Generate code and return it.  The way that the compilation mode
    // is controlled by the command-line flags is described in
    // the static helper function MakeCode.
    CompilationInfo info(literal, script, false);

    CHECK(!FLAG_always_full_compiler || !FLAG_always_fast_compiler);
    bool is_run_once = literal->try_full_codegen();
    bool is_compiled = false;
    if (FLAG_always_full_compiler || (FLAG_full_compiler && is_run_once)) {
      FullCodeGenSyntaxChecker checker;
      checker.Check(literal);
      if (checker.has_supported_syntax()) {
        code = FullCodeGenerator::MakeCode(&info);
        is_compiled = true;
      }
    } else if (FLAG_always_fast_compiler ||
               (FLAG_fast_compiler && !is_run_once)) {
      // Since we are not lazily compiling we do not have a receiver to
      // specialize for.
      FastCodeGenSyntaxChecker checker;
      checker.Check(&info);
      if (checker.has_supported_syntax()) {
        code = FastCodeGenerator::MakeCode(&info);
        is_compiled = true;
      }
    }

    if (!is_compiled) {
      // We fall back to the classic V8 code generator.
      code = CodeGenerator::MakeCode(&info);
    }

    // Check for stack-overflow exception.
    if (code.is_null()) {
      caller->SetStackOverflow();
      return Handle<JSFunction>::null();
    }

    // Function compilation complete.
    LOG(CodeCreateEvent(Logger::FUNCTION_TAG, *code, *literal->name()));

#ifdef ENABLE_OPROFILE_AGENT
    OProfileAgent::CreateNativeCodeRegion(*literal->name(),
                                          code->instruction_start(),
                                          code->instruction_size());
#endif
  }

  // Create a boilerplate function.
  Handle<JSFunction> function =
      Factory::NewFunctionBoilerplate(literal->name(),
                                      literal->materialized_literal_count(),
                                      code);
  SetFunctionInfo(function, literal, false, script);

#ifdef ENABLE_DEBUGGER_SUPPORT
  // Notify debugger that a new function has been added.
  Debugger::OnNewFunction(function);
#endif

  // Set the expected number of properties for instances and return
  // the resulting function.
  SetExpectedNofPropertiesFromEstimate(function,
                                       literal->expected_property_count());
  return function;
}


// Sets the function info on a function.
// The start_position points to the first '(' character after the function name
// in the full script source. When counting characters in the script source the
// the first character is number 0 (not 1).
void Compiler::SetFunctionInfo(Handle<JSFunction> fun,
                               FunctionLiteral* lit,
                               bool is_toplevel,
                               Handle<Script> script) {
  fun->shared()->set_length(lit->num_parameters());
  fun->shared()->set_formal_parameter_count(lit->num_parameters());
  fun->shared()->set_script(*script);
  fun->shared()->set_function_token_position(lit->function_token_position());
  fun->shared()->set_start_position(lit->start_position());
  fun->shared()->set_end_position(lit->end_position());
  fun->shared()->set_is_expression(lit->is_expression());
  fun->shared()->set_is_toplevel(is_toplevel);
  fun->shared()->set_inferred_name(*lit->inferred_name());
  fun->shared()->SetThisPropertyAssignmentsInfo(
      lit->has_only_simple_this_property_assignments(),
      *lit->this_property_assignments());
  fun->shared()->set_try_full_codegen(lit->try_full_codegen());
}


} }  // namespace v8::internal
