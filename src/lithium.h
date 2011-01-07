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

#ifndef V8_LITHIUM_H_
#define V8_LITHIUM_H_

#include "lithium-allocator.h"

namespace v8 {
namespace internal {

class LGapNode;

class LGapResolver BASE_EMBEDDED {
 public:
  LGapResolver(const ZoneList<LMoveOperands>* moves, LOperand* marker_operand);
  const ZoneList<LMoveOperands>* ResolveInReverseOrder();

 private:
  LGapNode* LookupNode(LOperand* operand);
  bool CanReach(LGapNode* a, LGapNode* b, int visited_id);
  bool CanReach(LGapNode* a, LGapNode* b);
  void RegisterMove(LMoveOperands move);
  void AddResultMove(LOperand* from, LOperand* to);
  void AddResultMove(LGapNode* from, LGapNode* to);
  void ResolveCycle(LGapNode* start);

  ZoneList<LGapNode*> nodes_;
  ZoneList<LGapNode*> identified_cycles_;
  ZoneList<LMoveOperands> result_;
  LOperand* marker_operand_;
  int next_visited_id_;
  int bailout_after_ast_id_;
};


} }  // namespace v8::internal

#endif  // V8_LITHIUM_H_
