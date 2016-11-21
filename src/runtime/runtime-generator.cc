// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/arguments.h"
#include "src/factory.h"
#include "src/frames-inl.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_CreateJSGeneratorObject) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, receiver, 1);
  CHECK(IsResumableFunction(function->shared()->kind()));

  Handle<FixedArray> operand_stack;
  if (function->shared()->HasBytecodeArray()) {
    // New-style generators.
    DCHECK(!function->shared()->HasBaselineCode());
    int size = function->shared()->bytecode_array()->register_count();
    operand_stack = isolate->factory()->NewFixedArray(size);
  } else {
    // Old-style generators.
    DCHECK(function->shared()->HasBaselineCode());
    operand_stack = isolate->factory()->empty_fixed_array();
  }

  Handle<JSGeneratorObject> generator =
      isolate->factory()->NewJSGeneratorObject(function);
  generator->set_function(*function);
  generator->set_context(isolate->context());
  generator->set_receiver(*receiver);
  generator->set_operand_stack(*operand_stack);
  generator->set_continuation(JSGeneratorObject::kGeneratorExecuting);
  return *generator;
}

RUNTIME_FUNCTION(Runtime_GeneratorClose) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, generator, 0);

  generator->set_continuation(JSGeneratorObject::kGeneratorClosed);

  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_GeneratorGetFunction) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, generator, 0);

  return generator->function();
}

RUNTIME_FUNCTION(Runtime_GeneratorGetReceiver) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, generator, 0);

  return generator->receiver();
}

RUNTIME_FUNCTION(Runtime_GeneratorGetInputOrDebugPos) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, generator, 0);

  return generator->input_or_debug_pos();
}

RUNTIME_FUNCTION(Runtime_GeneratorGetResumeMode) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, generator, 0);

  return Smi::FromInt(generator->resume_mode());
}

RUNTIME_FUNCTION(Runtime_GeneratorGetContinuation) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, generator, 0);

  return Smi::FromInt(generator->continuation());
}

RUNTIME_FUNCTION(Runtime_GeneratorGetSourcePosition) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, generator, 0);

  if (!generator->is_suspended()) return isolate->heap()->undefined_value();
  return Smi::FromInt(generator->source_position());
}

}  // namespace internal
}  // namespace v8
