// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/wasm-gc-optimize-phase.h"

#include "src/compiler/js-heap-broker.h"
#include "src/compiler/turboshaft/wasm-gc-type-reducer.h"
#include "src/compiler/turboshaft/wasm-load-elimination-reducer.h"

namespace v8::internal::compiler::turboshaft {

void WasmGCOptimizePhase::Run(Zone* temp_zone) {
  UnparkedScopeIfNeeded scope(PipelineData::Get().broker(),
                              v8_flags.turboshaft_trace_reduction);
  OptimizationPhase<WasmLoadEliminationReducer, WasmGCTypeReducer>::Run(
      temp_zone);
}

}  // namespace v8::internal::compiler::turboshaft
