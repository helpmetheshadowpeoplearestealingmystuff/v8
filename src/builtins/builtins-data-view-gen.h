// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_DATA_VIEW_GEN_H_
#define V8_BUILTINS_BUILTINS_DATA_VIEW_GEN_H_

#include "src/code-stub-assembler.h"

namespace v8 {
namespace internal {

class DataViewBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit DataViewBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  TNode<Smi> LoadDataViewByteOffset(TNode<JSDataView> data_view) {
    return LoadObjectField<Smi>(data_view, JSDataView::kByteOffsetOffset);
  }

  TNode<Smi> LoadDataViewByteLength(TNode<JSDataView> data_view) {
    return LoadObjectField<Smi>(data_view, JSDataView::kByteLengthOffset);
  }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_DATA_VIEW_GEN_H_
