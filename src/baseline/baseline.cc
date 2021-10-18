// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/baseline/baseline.h"

#include "src/handles/maybe-handles.h"
#include "src/objects/shared-function-info-inl.h"

// TODO(v8:11421): Remove #if once baseline compiler is ported to other
// architectures.
#include "src/flags/flags.h"
#if ENABLE_SPARKPLUG

#include "src/baseline/baseline-assembler-inl.h"
#include "src/baseline/baseline-compiler.h"
#include "src/debug/debug.h"
#include "src/heap/factory-inl.h"
#include "src/logging/runtime-call-stats-scope.h"
#include "src/objects/script-inl.h"

namespace v8 {
namespace internal {

bool CanCompileWithBaseline(Isolate* isolate, SharedFunctionInfo shared) {
  DisallowGarbageCollection no_gc;

  // Check that baseline compiler is enabled.
  if (!FLAG_sparkplug) return false;

  // Check that short builtin calls are enabled if needed.
  if (FLAG_sparkplug_needs_short_builtins &&
      !isolate->is_short_builtin_calls_enabled()) {
    return false;
  }

  // Check if we actually have bytecode.
  if (!shared.HasBytecodeArray()) return false;

  // Do not optimize when debugger needs to hook into every call.
  if (isolate->debug()->needs_check_on_function_call()) return false;

  // Functions with breakpoints have to stay interpreted.
  if (shared.HasBreakInfo()) return false;

  // Functions with instrumented bytecode can't be baseline compiled since the
  // baseline code's bytecode array pointer is immutable.
  if (shared.HasDebugInfo() &&
      shared.GetDebugInfo().HasInstrumentedBytecodeArray()) {
    return false;
  }

  // Do not baseline compile if function doesn't pass sparkplug_filter.
  if (!shared.PassesFilter(FLAG_sparkplug_filter)) return false;

  return true;
}

MaybeHandle<Code> GenerateBaselineCode(Isolate* isolate,
                                       Handle<SharedFunctionInfo> shared) {
  RCS_SCOPE(isolate, RuntimeCallCounterId::kCompileBaseline);
  Handle<BytecodeArray> bytecode(shared->GetBytecodeArray(isolate), isolate);
  baseline::BaselineCompiler compiler(isolate, shared, bytecode);
  compiler.GenerateCode();
  MaybeHandle<Code> code = compiler.Build(isolate);
  if (FLAG_print_code && !code.is_null()) {
    code.ToHandleChecked()->Print();
  }
  return code;
}

void EmitReturnBaseline(MacroAssembler* masm) {
  baseline::BaselineAssembler::EmitReturn(masm);
}

}  // namespace internal
}  // namespace v8

#else

namespace v8 {
namespace internal {

bool CanCompileWithBaseline(Isolate* isolate, SharedFunctionInfo shared) {
  return false;
}

MaybeHandle<Code> GenerateBaselineCode(Isolate* isolate,
                                       Handle<SharedFunctionInfo> shared) {
  UNREACHABLE();
}

void EmitReturnBaseline(MacroAssembler* masm) { UNREACHABLE(); }

}  // namespace internal
}  // namespace v8

#endif
