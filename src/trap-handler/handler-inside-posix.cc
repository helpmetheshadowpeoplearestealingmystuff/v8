// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// PLEASE READ BEFORE CHANGING THIS FILE!
//
// This file implements the out of bounds signal handler for
// WebAssembly. Signal handlers are notoriously difficult to get
// right, and getting it wrong can lead to security
// vulnerabilities. In order to minimize this risk, here are some
// rules to follow.
//
// 1. Do not introduce any new external dependencies. This file needs
//    to be self contained so it is easy to audit everything that a
//    signal handler might do.
//
// 2. Any changes must be reviewed by someone from the crash reporting
//    or security team. See OWNERS for suggested reviewers.
//
// For more information, see https://goo.gl/yMeyUY.
//
// This file contains most of the code that actually runs in a signal handler
// context. Some additional code is used both inside and outside the signal
// handler. This code can be found in handler-shared.cc.

#include "src/trap-handler/handler-inside-posix.h"

#include <signal.h>

#if defined(V8_OS_LINUX) || defined(V8_OS_FREEBSD)
#include <ucontext.h>
#elif V8_OS_MACOSX
#include <sys/ucontext.h>
#endif

#include <stddef.h>
#include <stdlib.h>

#include "src/trap-handler/trap-handler-internal.h"
#include "src/trap-handler/trap-handler.h"

namespace v8 {
namespace internal {
namespace trap_handler {

bool IsKernelGeneratedSignal(siginfo_t* info) {
  // On macOS, only `info->si_code > 0` is relevant, because macOS leaves
  // si_code at its default of 0 for signals that don’t originate in hardware.
  // The other conditions are only relevant for Linux.
  return info->si_code > 0 && info->si_code != SI_USER &&
         info->si_code != SI_QUEUE && info->si_code != SI_TIMER &&
         info->si_code != SI_ASYNCIO && info->si_code != SI_MESGQ;
}

class UnmaskOobSignalScope {
 public:
  UnmaskOobSignalScope() {
    sigset_t sigs;
    // Fortunately, sigemptyset and sigaddset are async-signal-safe according to
    // the POSIX standard.
    sigemptyset(&sigs);
    sigaddset(&sigs, kOobSignal);
    pthread_sigmask(SIG_UNBLOCK, &sigs, &old_mask_);
  }

  UnmaskOobSignalScope(const UnmaskOobSignalScope&) = delete;
  void operator=(const UnmaskOobSignalScope&) = delete;

  ~UnmaskOobSignalScope() { pthread_sigmask(SIG_SETMASK, &old_mask_, nullptr); }

 private:
  sigset_t old_mask_;
};

bool TryHandleSignal(int signum, siginfo_t* info, void* context) {
  // Ensure the faulting thread was actually running Wasm code. This should be
  // the first check in the trap handler to guarantee that the IsThreadInWasm
  // flag is only set in wasm code. Otherwise a later signal handler is executed
  // with the flag set.
  if (!g_thread_in_wasm_code) return false;

  // Clear g_thread_in_wasm_code, primarily to protect against nested faults.
  // The only path that resets the flag to true is if we find a landing pad (in
  // which case this function returns true). Otherwise we leave the flag unset
  // since we do not return to wasm code.
  g_thread_in_wasm_code = false;

  // Bail out early in case we got called for the wrong kind of signal.
  if (signum != kOobSignal) return false;

  // Make sure the signal was generated by the kernel and not some other source.
  if (!IsKernelGeneratedSignal(info)) return false;

  // Unmask the oob signal, which is automatically masked during the execution
  // of this handler. This ensures that crashes generated in this function will
  // be handled by the crash reporter. Otherwise, the process might be killed
  // with the crash going unreported. The scope object makes sure to restore the
  // signal mask on return from this function. We put the scope object in a
  // separate block to ensure that we restore the signal mask before we restore
  // the g_thread_in_wasm_code flag.
  {
    UnmaskOobSignalScope unmask_oob_signal;

    ucontext_t* uc = reinterpret_cast<ucontext_t*>(context);
#if V8_OS_LINUX && V8_TARGET_ARCH_X64
    auto* context_ip = &uc->uc_mcontext.gregs[REG_RIP];
#elif V8_OS_MACOSX && V8_TARGET_ARCH_ARM64
    auto* context_ip = &uc->uc_mcontext->__ss.__pc;
#elif V8_OS_MACOSX && V8_TARGET_ARCH_X64
    auto* context_ip = &uc->uc_mcontext->__ss.__rip;
#elif V8_OS_FREEBSD && V8_TARGET_ARCH_X64
    auto* context_ip = &uc->uc_mcontext.mc_rip;
#else
#error Unsupported platform
#endif
    uintptr_t fault_addr = *context_ip;
    uintptr_t landing_pad = 0;
    if (!TryFindLandingPad(fault_addr, &landing_pad)) return false;

    // Tell the caller to return to the landing pad.
    *context_ip = landing_pad;
  }
  // We will return to wasm code, so restore the g_thread_in_wasm_code flag.
  // This should only be done once the signal is blocked again (outside the
  // {UnmaskOobSignalScope}) to ensure that we do not catch a signal we raise
  // inside of the handler.
  g_thread_in_wasm_code = true;
  return true;
}

void HandleSignal(int signum, siginfo_t* info, void* context) {
  if (!TryHandleSignal(signum, info, context)) {
    // Since V8 didn't handle this signal, we want to re-raise the same signal.
    // For kernel-generated signals, we do this by restoring the original
    // handler and then returning. The fault will happen again and the usual
    // signal handling will happen.
    //
    // We handle user-generated signals by calling raise() instead. This is for
    // completeness. We should never actually see one of these, but just in
    // case, we do the right thing.
    RemoveTrapHandler();
    if (!IsKernelGeneratedSignal(info)) {
      raise(signum);
    }
  }
  // TryHandleSignal modifies context to change where we return to.
}

}  // namespace trap_handler
}  // namespace internal
}  // namespace v8
