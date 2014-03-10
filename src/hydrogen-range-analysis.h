// Copyright 2013 the V8 project authors. All rights reserved.
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

#ifndef V8_HYDROGEN_RANGE_ANALYSIS_H_
#define V8_HYDROGEN_RANGE_ANALYSIS_H_

#include "hydrogen.h"

namespace v8 {
namespace internal {


class HRangeAnalysisPhase : public HPhase {
 public:
  explicit HRangeAnalysisPhase(HGraph* graph)
      : HPhase("H_Range analysis", graph), changed_ranges_(16, zone()),
        in_worklist_(graph->GetMaximumValueID(), zone()),
        worklist_(32, zone()) {}

  void Run();

 private:
  void TraceRange(const char* msg, ...);
  void InferControlFlowRange(HCompareNumericAndBranch* test,
                             HBasicBlock* dest);
  void UpdateControlFlowRange(Token::Value op, HValue* value, HValue* other);
  void InferRange(HValue* value);
  void RollBackTo(int index);
  void AddRange(HValue* value, Range* range);
  void AddToWorklist(HValue* value) {
    if (in_worklist_.Contains(value->id())) return;
    in_worklist_.Add(value->id());
    worklist_.Add(value, zone());
  }
  void PropagateMinusZeroChecks(HValue* value);

  ZoneList<HValue*> changed_ranges_;

  BitVector in_worklist_;
  ZoneList<HValue*> worklist_;

  DISALLOW_COPY_AND_ASSIGN(HRangeAnalysisPhase);
};


} }  // namespace v8::internal

#endif  // V8_HYDROGEN_RANGE_ANALYSIS_H_
