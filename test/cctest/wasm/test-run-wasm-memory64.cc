// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-opcodes-inl.h"
#include "test/cctest/cctest.h"
#include "test/cctest/wasm/wasm-run-utils.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"

namespace v8 {
namespace internal {
namespace wasm {

template <typename ReturnType, typename... ParamTypes>
class Memory64Runner : public WasmRunner<ReturnType, ParamTypes...> {
 public:
  explicit Memory64Runner(TestExecutionTier execution_tier)
      : WasmRunner<ReturnType, ParamTypes...>(execution_tier) {
    this->builder().EnableFeature(kFeature_memory64);
    this->builder().SetMemory64();
  }
};

WASM_EXEC_TEST(Load) {
  // TODO(clemensb): Implement memory64 in the interpreter.
  if (execution_tier == TestExecutionTier::kInterpreter) return;

  // TODO(clemensb): Fix memory64 in Turbofan on 32-bit systems.
  if (execution_tier == TestExecutionTier::kTurbofan &&
      kSystemPointerSize == 4) {
    return;
  }

  Memory64Runner<uint32_t, uint64_t> r(execution_tier);
  uint32_t* memory =
      r.builder().AddMemoryElems<uint32_t>(kWasmPageSize / sizeof(int32_t));

  BUILD(r, WASM_LOAD_MEM(MachineType::Int32(), WASM_LOCAL_GET(0)));

  CHECK_EQ(0, r.Call(0));

  memory[0] = 0x12345678;
  CHECK_EQ(0x12345678, r.Call(0));
  CHECK_EQ(0x123456, r.Call(1));
  CHECK_EQ(0x1234, r.Call(2));
  CHECK_EQ(0x12, r.Call(3));
  CHECK_EQ(0x0, r.Call(4));

  // TODO(clemensb): Check traps.
}

// TODO(clemensb): Test atomic instructions.

}  // namespace wasm
}  // namespace internal
}  // namespace v8
