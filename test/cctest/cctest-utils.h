// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "src/base/build_config.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {

#ifdef V8_CC_GNU

DISABLE_ASAN inline uintptr_t GetStackPointer() {
  uintptr_t sp_addr;
#if V8_HOST_ARCH_X64
  __asm__ __volatile__("mov %%rsp, %0" : "=g"(sp_addr));
#elif V8_HOST_ARCH_IA32
  __asm__ __volatile__("mov %%esp, %0" : "=g"(sp_addr));
#elif V8_HOST_ARCH_ARM
  __asm__ __volatile__("str sp, %0" : "=g"(sp_addr));
#elif V8_HOST_ARCH_ARM64
  __asm__ __volatile__("mov x16, sp; str x16, %0" : "=g"(sp_addr));
#elif V8_HOST_ARCH_MIPS
  __asm__ __volatile__("sw $sp, %0" : "=g"(sp_addr));
#elif V8_HOST_ARCH_MIPS64
  __asm__ __volatile__("sd $sp, %0" : "=g"(sp_addr));
#elif defined(__s390x__) || defined(_ARCH_S390X)
  __asm__ __volatile__("stg %%r15, %0" : "=m"(sp_addr));
#elif defined(__s390__) || defined(_ARCH_S390)
  __asm__ __volatile__("st 15, %0" : "=m"(sp_addr));
#elif defined(__PPC64__) || defined(_ARCH_PPC64)
  __asm__ __volatile__("std 1, %0" : "=g"(sp_addr));
#elif defined(__PPC__) || defined(_ARCH_PPC)
  __asm__ __volatile__("stw 1, %0" : "=g"(sp_addr));
#else
#error Host architecture was not detected as supported by v8
#endif
  return sp_addr;
}

#endif  // V8_CC_GNU

}  // namespace internal
}  // namespace v8
