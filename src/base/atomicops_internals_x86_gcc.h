// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is an internal atomic implementation, use atomicops.h instead.

#ifndef V8_BASE_ATOMICOPS_INTERNALS_X86_GCC_H_
#define V8_BASE_ATOMICOPS_INTERNALS_X86_GCC_H_

#include "src/base/base-export.h"

namespace v8 {
namespace base {

// This struct is not part of the public API of this module; clients may not
// use it.
// Features of this x86.  Values may not be correct before main() is run,
// but are set conservatively.
struct AtomicOps_x86CPUFeatureStruct {
  bool has_amd_lock_mb_bug;  // Processor has AMD memory-barrier bug; do lfence
                             // after acquire compare-and-swap.
#if !defined(__SSE2__)
  bool has_sse2;             // Processor has SSE2.
#endif
};
V8_BASE_EXPORT extern struct AtomicOps_x86CPUFeatureStruct
    AtomicOps_Internalx86CPUFeatures;

#define ATOMICOPS_COMPILER_BARRIER() __asm__ __volatile__("" : : : "memory")

// 32-bit low-level operations on any platform.

inline Atomic32 NoBarrier_CompareAndSwap(volatile Atomic32* ptr,
                                         Atomic32 old_value,
                                         Atomic32 new_value) {
  Atomic32 prev;
  __asm__ __volatile__("lock; cmpxchgl %1,%2"
                       : "=a" (prev)
                       : "q" (new_value), "m" (*ptr), "0" (old_value)
                       : "memory");
  return prev;
}

inline Atomic32 NoBarrier_AtomicExchange(volatile Atomic32* ptr,
                                         Atomic32 new_value) {
  __asm__ __volatile__("xchgl %1,%0"  // The lock prefix is implicit for xchg.
                       : "=r" (new_value)
                       : "m" (*ptr), "0" (new_value)
                       : "memory");
  return new_value;  // Now it's the previous value.
}

inline Atomic32 NoBarrier_AtomicIncrement(volatile Atomic32* ptr,
                                          Atomic32 increment) {
  Atomic32 temp = increment;
  __asm__ __volatile__("lock; xaddl %0,%1"
                       : "+r" (temp), "+m" (*ptr)
                       : : "memory");
  // temp now holds the old value of *ptr
  return temp + increment;
}

inline Atomic32 Barrier_AtomicIncrement(volatile Atomic32* ptr,
                                        Atomic32 increment) {
  Atomic32 temp = increment;
  __asm__ __volatile__("lock; xaddl %0,%1"
                       : "+r" (temp), "+m" (*ptr)
                       : : "memory");
  // temp now holds the old value of *ptr
  if (AtomicOps_Internalx86CPUFeatures.has_amd_lock_mb_bug) {
    __asm__ __volatile__("lfence" : : : "memory");
  }
  return temp + increment;
}

inline Atomic32 Acquire_CompareAndSwap(volatile Atomic32* ptr,
                                       Atomic32 old_value,
                                       Atomic32 new_value) {
  Atomic32 x = NoBarrier_CompareAndSwap(ptr, old_value, new_value);
  if (AtomicOps_Internalx86CPUFeatures.has_amd_lock_mb_bug) {
    __asm__ __volatile__("lfence" : : : "memory");
  }
  return x;
}

inline Atomic32 Release_CompareAndSwap(volatile Atomic32* ptr,
                                       Atomic32 old_value,
                                       Atomic32 new_value) {
  return NoBarrier_CompareAndSwap(ptr, old_value, new_value);
}

inline void NoBarrier_Store(volatile Atomic8* ptr, Atomic8 value) {
  *ptr = value;
}

inline void NoBarrier_Store(volatile Atomic32* ptr, Atomic32 value) {
  *ptr = value;
}

#if defined(__x86_64__) || defined(__SSE2__)

// 64-bit implementations of memory barrier can be simpler, because it
// "mfence" is guaranteed to exist.
inline void MemoryBarrier() {
  __asm__ __volatile__("mfence" : : : "memory");
}

inline void Acquire_Store(volatile Atomic32* ptr, Atomic32 value) {
  *ptr = value;
  MemoryBarrier();
}

#else

inline void MemoryBarrier() {
  if (AtomicOps_Internalx86CPUFeatures.has_sse2) {
    __asm__ __volatile__("mfence" : : : "memory");
  } else {  // mfence is faster but not present on PIII
    Atomic32 x = 0;
    NoBarrier_AtomicExchange(&x, 0);  // acts as a barrier on PIII
  }
}

inline void Acquire_Store(volatile Atomic32* ptr, Atomic32 value) {
  if (AtomicOps_Internalx86CPUFeatures.has_sse2) {
    *ptr = value;
    __asm__ __volatile__("mfence" : : : "memory");
  } else {
    NoBarrier_AtomicExchange(ptr, value);
                          // acts as a barrier on PIII
  }
}
#endif

inline void Release_Store(volatile Atomic32* ptr, Atomic32 value) {
  ATOMICOPS_COMPILER_BARRIER();
  *ptr = value;  // An x86 store acts as a release barrier.
  // See comments in Atomic64 version of Release_Store(), below.
}

inline Atomic8 NoBarrier_Load(volatile const Atomic8* ptr) {
  return *ptr;
}

inline Atomic32 NoBarrier_Load(volatile const Atomic32* ptr) {
  return *ptr;
}

inline Atomic32 Acquire_Load(volatile const Atomic32* ptr) {
  Atomic32 value = *ptr;  // An x86 load acts as a acquire barrier.
  // See comments in Atomic64 version of Release_Store(), below.
  ATOMICOPS_COMPILER_BARRIER();
  return value;
}

inline Atomic32 Release_Load(volatile const Atomic32* ptr) {
  MemoryBarrier();
  return *ptr;
}

#if defined(__x86_64__) && defined(V8_HOST_ARCH_64_BIT)

// 64-bit low-level operations on 64-bit platform.

inline Atomic64 NoBarrier_CompareAndSwap(volatile Atomic64* ptr,
                                         Atomic64 old_value,
                                         Atomic64 new_value) {
  Atomic64 prev;
  __asm__ __volatile__("lock; cmpxchgq %1,%2"
                       : "=a" (prev)
                       : "q" (new_value), "m" (*ptr), "0" (old_value)
                       : "memory");
  return prev;
}

inline Atomic64 NoBarrier_AtomicExchange(volatile Atomic64* ptr,
                                         Atomic64 new_value) {
  __asm__ __volatile__("xchgq %1,%0"  // The lock prefix is implicit for xchg.
                       : "=r" (new_value)
                       : "m" (*ptr), "0" (new_value)
                       : "memory");
  return new_value;  // Now it's the previous value.
}

inline Atomic64 NoBarrier_AtomicIncrement(volatile Atomic64* ptr,
                                          Atomic64 increment) {
  Atomic64 temp = increment;
  __asm__ __volatile__("lock; xaddq %0,%1"
                       : "+r" (temp), "+m" (*ptr)
                       : : "memory");
  // temp now contains the previous value of *ptr
  return temp + increment;
}

inline Atomic64 Barrier_AtomicIncrement(volatile Atomic64* ptr,
                                        Atomic64 increment) {
  Atomic64 temp = increment;
  __asm__ __volatile__("lock; xaddq %0,%1"
                       : "+r" (temp), "+m" (*ptr)
                       : : "memory");
  // temp now contains the previous value of *ptr
  if (AtomicOps_Internalx86CPUFeatures.has_amd_lock_mb_bug) {
    __asm__ __volatile__("lfence" : : : "memory");
  }
  return temp + increment;
}

inline void NoBarrier_Store(volatile Atomic64* ptr, Atomic64 value) {
  *ptr = value;
}

inline void Acquire_Store(volatile Atomic64* ptr, Atomic64 value) {
  *ptr = value;
  MemoryBarrier();
}

inline void Release_Store(volatile Atomic64* ptr, Atomic64 value) {
  ATOMICOPS_COMPILER_BARRIER();

  *ptr = value;  // An x86 store acts as a release barrier
                 // for current AMD/Intel chips as of Jan 2008.
                 // See also Acquire_Load(), below.

  // When new chips come out, check:
  //  IA-32 Intel Architecture Software Developer's Manual, Volume 3:
  //  System Programming Guide, Chatper 7: Multiple-processor management,
  //  Section 7.2, Memory Ordering.
  // Last seen at:
  //   http://developer.intel.com/design/pentium4/manuals/index_new.htm
  //
  // x86 stores/loads fail to act as barriers for a few instructions (clflush
  // maskmovdqu maskmovq movntdq movnti movntpd movntps movntq) but these are
  // not generated by the compiler, and are rare.  Users of these instructions
  // need to know about cache behaviour in any case since all of these involve
  // either flushing cache lines or non-temporal cache hints.
}

inline Atomic64 NoBarrier_Load(volatile const Atomic64* ptr) {
  return *ptr;
}

inline Atomic64 Acquire_Load(volatile const Atomic64* ptr) {
  Atomic64 value = *ptr;  // An x86 load acts as a acquire barrier,
                          // for current AMD/Intel chips as of Jan 2008.
                          // See also Release_Store(), above.
  ATOMICOPS_COMPILER_BARRIER();
  return value;
}

inline Atomic64 Release_Load(volatile const Atomic64* ptr) {
  MemoryBarrier();
  return *ptr;
}

inline Atomic64 Acquire_CompareAndSwap(volatile Atomic64* ptr,
                                       Atomic64 old_value,
                                       Atomic64 new_value) {
  Atomic64 x = NoBarrier_CompareAndSwap(ptr, old_value, new_value);
  if (AtomicOps_Internalx86CPUFeatures.has_amd_lock_mb_bug) {
    __asm__ __volatile__("lfence" : : : "memory");
  }
  return x;
}

inline Atomic64 Release_CompareAndSwap(volatile Atomic64* ptr,
                                       Atomic64 old_value,
                                       Atomic64 new_value) {
  return NoBarrier_CompareAndSwap(ptr, old_value, new_value);
}

#endif  // defined(__x86_64__)

}  // namespace base
}  // namespace v8

#undef ATOMICOPS_COMPILER_BARRIER

#endif  // V8_BASE_ATOMICOPS_INTERNALS_X86_GCC_H_
