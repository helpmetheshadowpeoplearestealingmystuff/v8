// Copyright 2012 the V8 project authors. All rights reserved.
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

#ifndef V8_IC_INL_H_
#define V8_IC_INL_H_

#include "ic.h"

#include "compiler.h"
#include "debug.h"
#include "macro-assembler.h"

namespace v8 {
namespace internal {


Address IC::address() const {
  // Get the address of the call.
  Address result = Assembler::target_address_from_return_address(pc());

#ifdef ENABLE_DEBUGGER_SUPPORT
  Debug* debug = isolate()->debug();
  // First check if any break points are active if not just return the address
  // of the call.
  if (!debug->has_break_points()) return result;

  // At least one break point is active perform additional test to ensure that
  // break point locations are updated correctly.
  if (debug->IsDebugBreak(Assembler::target_address_at(result))) {
    // If the call site is a call to debug break then return the address in
    // the original code instead of the address in the running code. This will
    // cause the original code to be updated and keeps the breakpoint active in
    // the running code.
    return OriginalCodeAddress();
  } else {
    // No break point here just return the address of the call.
    return result;
  }
#else
  return result;
#endif
}


Code* IC::GetTargetAtAddress(Address address) {
  // Get the target address of the IC.
  Address target = Assembler::target_address_at(address);
  // Convert target address to the code object. Code::GetCodeFromTargetAddress
  // is safe for use during GC where the map might be marked.
  Code* result = Code::GetCodeFromTargetAddress(target);
  ASSERT(result->is_inline_cache_stub());
  return result;
}


void IC::SetTargetAtAddress(Address address, Code* target) {
  ASSERT(target->is_inline_cache_stub() || target->is_compare_ic_stub());
  Heap* heap = target->GetHeap();
  Code* old_target = GetTargetAtAddress(address);
#ifdef DEBUG
  // STORE_IC and KEYED_STORE_IC use Code::extra_ic_state() to mark
  // ICs as strict mode. The strict-ness of the IC must be preserved.
  if (old_target->kind() == Code::STORE_IC ||
      old_target->kind() == Code::KEYED_STORE_IC) {
    ASSERT(StoreIC::GetStrictMode(old_target->extra_ic_state()) ==
           StoreIC::GetStrictMode(target->extra_ic_state()));
  }
#endif
  Assembler::set_target_address_at(address, target->instruction_start());
  if (heap->gc_state() == Heap::MARK_COMPACT) {
    heap->mark_compact_collector()->RecordCodeTargetPatch(address, target);
  } else {
    heap->incremental_marking()->RecordCodeTargetPatch(address, target);
  }
  PostPatching(address, target, old_target);
}


InlineCacheHolderFlag IC::GetCodeCacheForObject(Object* object) {
  if (object->IsJSObject()) return OWN_MAP;

  // If the object is a value, we use the prototype map for the cache.
  ASSERT(object->IsString() || object->IsSymbol() ||
         object->IsNumber() || object->IsBoolean());
  return PROTOTYPE_MAP;
}


HeapObject* IC::GetCodeCacheHolder(Isolate* isolate,
                                 Object* object,
                                 InlineCacheHolderFlag holder) {
  if (object->IsSmi()) holder = PROTOTYPE_MAP;
  Object* map_owner = holder == OWN_MAP
      ? object : object->GetPrototype(isolate);
  return HeapObject::cast(map_owner);
}


InlineCacheHolderFlag IC::GetCodeCacheFlag(Type* type) {
  if (type->Is(Type::Boolean()) ||
      type->Is(Type::Number()) ||
      type->Is(Type::String()) ||
      type->Is(Type::Symbol())) {
    return PROTOTYPE_MAP;
  }
  return OWN_MAP;
}


Handle<Map> IC::GetCodeCacheHolder(InlineCacheHolderFlag flag,
                                   Type* type,
                                   Isolate* isolate) {
  if (flag == PROTOTYPE_MAP) {
    Context* context = isolate->context()->native_context();
    JSFunction* constructor;
    if (type->Is(Type::Boolean())) {
      constructor = context->boolean_function();
    } else if (type->Is(Type::Number())) {
      constructor = context->number_function();
    } else if (type->Is(Type::String())) {
      constructor = context->string_function();
    } else {
      ASSERT(type->Is(Type::Symbol()));
      constructor = context->symbol_function();
    }
    return handle(JSObject::cast(constructor->instance_prototype())->map());
  }
  return TypeToMap(type, isolate);
}


} }  // namespace v8::internal

#endif  // V8_IC_INL_H_
