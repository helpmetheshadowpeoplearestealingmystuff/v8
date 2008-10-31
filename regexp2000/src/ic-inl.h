// Copyright 2006-2008 the V8 project authors. All rights reserved.
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
#include "debug.h"
#include "macro-assembler.h"

namespace v8 { namespace internal {


Address IC::address() {
  // Get the address of the call.
  Address result = pc() - Assembler::kTargetAddrToReturnAddrDist;

  // First check if any break points are active if not just return the address
  // of the call.
  if (!Debug::has_break_points()) return result;

  // At least one break point is active perform additional test to ensure that
  // break point locations are updated correctly.
  if (Debug::IsDebugBreak(Assembler::target_address_at(result))) {
    // If the call site is a call to debug break then return the address in
    // the original code instead of the address in the running code. This will
    // cause the original code to be updated and keeps the breakpoint active in
    // the running code.
    return OriginalCodeAddress();
  } else {
    // No break point here just return the address of the call.
    return result;
  }
}


Code* IC::GetTargetAtAddress(Address address) {
  Address target = Assembler::target_address_at(address);
  HeapObject* code = HeapObject::FromAddress(target - Code::kHeaderSize);
  // GetTargetAtAddress is called from IC::Clear which in turn is
  // called when marking objects during mark sweep. reinterpret_cast
  // is therefore used instead of the more appropriate
  // Code::cast. Code::cast does not work when the object's map is
  // marked.
  Code* result = reinterpret_cast<Code*>(code);
  ASSERT(result->is_inline_cache_stub());
  return result;
}


void IC::SetTargetAtAddress(Address address, Code* target) {
  ASSERT(target->is_inline_cache_stub());
  Assembler::set_target_address_at(address, target->instruction_start());
}


Map* IC::GetCodeCacheMapForObject(Object* object) {
  if (object->IsJSObject()) return JSObject::cast(object)->map();
  // If the object is a value, we use the prototype map for the cache.
  ASSERT(object->IsString() || object->IsNumber() || object->IsBoolean());
  return JSObject::cast(object->GetPrototype())->map();
}


} }  // namespace v8::internal

#endif  // V8_IC_INL_H_
