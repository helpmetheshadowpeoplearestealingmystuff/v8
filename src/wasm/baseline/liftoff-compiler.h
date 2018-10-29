// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_LIFTOFF_COMPILER_H_
#define V8_WASM_BASELINE_LIFTOFF_COMPILER_H_

#include "src/base/macros.h"

namespace v8 {
namespace internal {

class Counters;

namespace wasm {

struct CompilationEnv;
class WasmCompilationUnit;
struct WasmFeatures;

class LiftoffCompilationUnit final {
 public:
  explicit LiftoffCompilationUnit(WasmCompilationUnit* wasm_unit)
      : wasm_unit_(wasm_unit) {}

  bool ExecuteCompilation(CompilationEnv*, Counters*, WasmFeatures* detected);

 private:
  WasmCompilationUnit* const wasm_unit_;

  DISALLOW_COPY_AND_ASSIGN(LiftoffCompilationUnit);
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_BASELINE_LIFTOFF_COMPILER_H_
