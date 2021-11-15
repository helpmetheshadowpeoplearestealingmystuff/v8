// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_UNOPTIMIZED_COMPILATION_INFO_H_
#define V8_CODEGEN_UNOPTIMIZED_COMPILATION_INFO_H_

#include <memory>

#include "src/codegen/source-position-table.h"
#include "src/common/globals.h"
#include "src/handles/handles.h"
#include "src/objects/feedback-vector.h"
#include "src/objects/objects.h"
#include "src/parsing/parse-info.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {

class AsmWasmData;
class CoverageInfo;
class DeclarationScope;
class FunctionLiteral;
class Isolate;
class ParseInfo;
class SourceRangeMap;
class Zone;

// UnoptimizedCompilationInfo encapsulates the information needed to compile
// unoptimized code for a given function, and the results of the compilation.
class V8_EXPORT_PRIVATE UnoptimizedCompilationInfo final {
 public:
  UnoptimizedCompilationInfo(Zone* zone, ParseInfo* parse_info,
                             FunctionLiteral* literal);

  const UnoptimizedCompileFlags& flags() const { return flags_; }
  const UnoptimizedCompileState* state() const { return state_; }
  const Utf16CharacterStream* character_stream() const {
    return character_stream_;
  }

  // Accessors for the input data of the function being compiled.

  FunctionLiteral* literal() const { return literal_; }
  void set_literal(FunctionLiteral* literal) {
    DCHECK_NOT_NULL(literal);
    literal_ = literal;
  }
  void ClearLiteral() { literal_ = nullptr; }

  DeclarationScope* scope() const;

  int num_parameters() const;
  int num_parameters_including_this() const;

  // Accessors for optional compilation features.

  SourcePositionTableBuilder::RecordingMode SourcePositionRecordingMode() const;

  bool has_source_range_map() const { return source_range_map_ != nullptr; }
  SourceRangeMap* source_range_map() const { return source_range_map_; }
  void set_source_range_map(SourceRangeMap* source_range_map) {
    source_range_map_ = source_range_map;
  }

  bool has_coverage_info() const { return !coverage_info_.is_null(); }
  Handle<CoverageInfo> coverage_info() const { return coverage_info_; }
  void set_coverage_info(Handle<CoverageInfo> coverage_info) {
    coverage_info_ = coverage_info;
  }

  // Accessors for the output of compilation.

  bool has_bytecode_array() const { return !bytecode_array_.is_null(); }
  Handle<BytecodeArray> bytecode_array() const { return bytecode_array_; }
  void SetBytecodeArray(Handle<BytecodeArray> bytecode_array) {
    bytecode_array_ = bytecode_array;
  }

  bool has_asm_wasm_data() const { return !asm_wasm_data_.is_null(); }
  Handle<AsmWasmData> asm_wasm_data() const { return asm_wasm_data_; }
  void SetAsmWasmData(Handle<AsmWasmData> asm_wasm_data) {
    asm_wasm_data_ = asm_wasm_data;
  }

  FeedbackVectorSpec* feedback_vector_spec() { return &feedback_vector_spec_; }

 private:
  // Compilation flags.
  const UnoptimizedCompileFlags flags_;

  // Compilation state.
  const UnoptimizedCompileState* state_;
  const Utf16CharacterStream* character_stream_;

  // The root AST node of the function literal being compiled.
  FunctionLiteral* literal_;

  // Used when block coverage is enabled.
  SourceRangeMap* source_range_map_;

  // Encapsulates coverage information gathered by the bytecode generator.
  // Needs to be stored on the shared function info once compilation completes.
  Handle<CoverageInfo> coverage_info_;

  // Holds the bytecode array generated by the interpreter.
  Handle<BytecodeArray> bytecode_array_;

  // Holds the asm_wasm data struct generated by the asmjs compiler.
  Handle<AsmWasmData> asm_wasm_data_;

  // Holds the feedback vector spec generated during compilation
  FeedbackVectorSpec feedback_vector_spec_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_UNOPTIMIZED_COMPILATION_INFO_H_
