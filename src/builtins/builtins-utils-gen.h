// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_UTILS_GEN_H_
#define V8_BUILTINS_BUILTINS_UTILS_GEN_H_

#include "include/cppgc/source-location.h"
#include "src/builtins/builtins-descriptors.h"

namespace v8 {
namespace internal {

namespace compiler {
class CodeAssemblerState;
}  // namespace compiler

// ----------------------------------------------------------------------------
// Support macro for defining builtins with Turbofan.
// ----------------------------------------------------------------------------
//
// A builtin function is defined by writing:
//
//   TF_BUILTIN(name, code_assember_base_class) {
//     ...
//   }
//
// In the body of the builtin function the arguments can be accessed
// as "Parameter(n)".
#define TF_BUILTIN(Name, AssemblerBase)                                     \
  class Name##Assembler : public AssemblerBase {                            \
   public:                                                                  \
    using Descriptor = Builtin_##Name##_InterfaceDescriptor;                \
                                                                            \
    explicit Name##Assembler(compiler::CodeAssemblerState* state)           \
        : AssemblerBase(state) {}                                           \
    void Generate##Name##Impl();                                            \
                                                                            \
    Node* UntypedParameter(Descriptor::ParameterIndices index) {            \
      return CodeAssembler::UntypedParameter(static_cast<int>(index));      \
    }                                                                       \
                                                                            \
    template <class T>                                                      \
    TNode<T> Parameter(                                                     \
        Descriptor::ParameterIndices index,                                 \
        cppgc::SourceLocation loc = cppgc::SourceLocation::Current()) {     \
      return CodeAssembler::Parameter<T>(static_cast<int>(index), loc);     \
    }                                                                       \
                                                                            \
    template <class T>                                                      \
    TNode<T> UncheckedParameter(Descriptor::ParameterIndices index) {       \
      return CodeAssembler::UncheckedParameter<T>(static_cast<int>(index)); \
    }                                                                       \
  };                                                                        \
  void Builtins::Generate_##Name(compiler::CodeAssemblerState* state) {     \
    Name##Assembler assembler(state);                                       \
    state->SetInitialDebugInformation(#Name, __FILE__, __LINE__);           \
    if (Builtins::KindOf(Builtins::k##Name) == Builtins::TFJ) {             \
      assembler.PerformStackCheck(assembler.GetJSContextParameter());       \
    }                                                                       \
    assembler.Generate##Name##Impl();                                       \
  }                                                                         \
  void Name##Assembler::Generate##Name##Impl()

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_UTILS_GEN_H_
