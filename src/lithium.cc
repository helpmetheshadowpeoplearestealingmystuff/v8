// Copyright 2011 the V8 project authors. All rights reserved.
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

#include "lithium.h"

namespace v8 {
namespace internal {

bool LParallelMove::IsRedundant() const {
  for (int i = 0; i < move_operands_.length(); ++i) {
    if (!move_operands_[i].IsRedundant()) return false;
  }
  return true;
}


void LParallelMove::PrintDataTo(StringStream* stream) const {
  bool first = true;
  for (int i = 0; i < move_operands_.length(); ++i) {
    if (!move_operands_[i].IsEliminated()) {
      LOperand* source = move_operands_[i].source();
      LOperand* destination = move_operands_[i].destination();
      if (!first) stream->Add(" ");
      first = false;
      if (source->Equals(destination)) {
        destination->PrintTo(stream);
      } else {
        destination->PrintTo(stream);
        stream->Add(" = ");
        source->PrintTo(stream);
      }
      stream->Add(";");
    }
  }
}


void LEnvironment::PrintTo(StringStream* stream) {
  stream->Add("[id=%d|", ast_id());
  stream->Add("[parameters=%d|", parameter_count());
  stream->Add("[arguments_stack_height=%d|", arguments_stack_height());
  for (int i = 0; i < values_.length(); ++i) {
    if (i != 0) stream->Add(";");
    if (values_[i] == NULL) {
      stream->Add("[hole]");
    } else {
      values_[i]->PrintTo(stream);
    }
  }
  stream->Add("]");
}


void LPointerMap::RecordPointer(LOperand* op) {
  // Do not record arguments as pointers.
  if (op->IsStackSlot() && op->index() < 0) return;
  ASSERT(!op->IsDoubleRegister() && !op->IsDoubleStackSlot());
  pointer_operands_.Add(op);
}


void LPointerMap::PrintTo(StringStream* stream) {
  stream->Add("{");
  for (int i = 0; i < pointer_operands_.length(); ++i) {
    if (i != 0) stream->Add(";");
    pointer_operands_[i]->PrintTo(stream);
  }
  stream->Add("} @%d", position());
}


} }  // namespace v8::internal
