// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <regex>
#include <string>

#include "src/base/vector.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/string-builder-multiline.h"
#include "src/wasm/wasm-disassembler-impl.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {
namespace wasm {

class WasmDisassemblerTest : public ::v8::TestWithPlatform {};

// Code that is shared for all tests, the only difference is the input module
// and expected disassembler output.
void CheckDisassemblerOutput(base::Vector<const byte> module_bytes,
                             std::string expected_output) {
  AccountingAllocator allocator;

  ModuleResult module_result = DecodeWasmModuleForDisassembler(
      module_bytes.begin(), module_bytes.end(), &allocator);
  DCHECK(module_result.ok());
  WasmModule* module = module_result.value().get();

  ModuleWireBytes wire_bytes(module_bytes);
  NamesProvider names(module, module_bytes);

  MultiLineStringBuilder output_sb;

  ModuleDisassembler md(output_sb, module, &names, wire_bytes, &allocator);
  constexpr size_t max_mb = 100;  // Even 1 would be enough.
  md.PrintModule({0, 2}, max_mb);

  std::ostringstream output;
  output_sb.WriteTo(output);

  // Remove comment lines from expected output since they cannot be recovered
  // by a disassembler.
  // They were also used as part of the C++/WAT polyglot trick described below.
  expected_output =
      std::regex_replace(expected_output, std::regex(" *;;[^\\n]*\\n?"), "");

  EXPECT_EQ(output.str(), expected_output);
}

TEST_F(WasmDisassemblerTest, Mvp) {
  // If you want to extend this test (and the other tests below):
  // 1. Modify the included .wat.inc file(s), e.g., add more instructions.
  // 2. Convert the Wasm text file to a Wasm binary with `wat2wasm`.
  // 3. Convert the Wasm binary to an array init expression with
  // `wami --full-hexdump` and paste it into the included file below.
  // One liner example (Linux):
  // wat2wasm wasm-disassembler-unittest-mvp.wat.inc --output=-
  // | wami --full-hexdump
  // | head -n-1 | tail -n+2 > wasm-disassembler-unittest-mvp.wasm.inc
  constexpr byte module_bytes[] = {
#include "wasm-disassembler-unittest-mvp.wasm.inc"
  };

  // Little trick: polyglot C++/WebAssembly text file.
  // We want to include the expected disassembler text output as a string into
  // this test (instead of reading it from the file at runtime, which would make
  // it dependent on the current working directory).
  // At the same time, we want the included file itself to be valid WAT, such
  // that it can be processed e.g. by wat2wasm to build the module bytes above.
  // For that to work, we abuse that ;; starts a line comment in WAT, but at
  // the same time, ;; in C++ are just two empty statements, which are no
  // harm when including the file here either.
  std::string expected;
#include "wasm-disassembler-unittest-mvp.wat.inc"

  CheckDisassemblerOutput(base::ArrayVector(module_bytes), expected);
}

TEST_F(WasmDisassemblerTest, Names) {
  // You can create a binary with a custom name section from the text format via
  // `wat2wasm --debug-names`.
  constexpr byte module_bytes[] = {
#include "wasm-disassembler-unittest-names.wasm.inc"
  };
  std::string expected;
#include "wasm-disassembler-unittest-names.wat.inc"
  CheckDisassemblerOutput(base::ArrayVector(module_bytes), expected);
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
