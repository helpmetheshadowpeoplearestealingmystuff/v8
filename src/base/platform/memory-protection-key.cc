// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/platform/memory-protection-key.h"

#if V8_HAS_PKU_JIT_WRITE_PROTECT
#include <sys/mman.h>  // For {mprotect()} protection macros.
#undef MAP_TYPE  // Conflicts with MAP_TYPE in Torque-generated instance-types.h
#endif

#include "src/base/logging.h"
#include "src/base/macros.h"

// Runtime-detection of PKU support with {dlsym()}.
//
// For now, we support memory protection keys/PKEYs/PKU only for Linux on x64
// based on glibc functions {pkey_alloc()}, {pkey_free()}, etc.
// Those functions are only available since glibc version 2.27:
// https://man7.org/linux/man-pages/man2/pkey_alloc.2.html
// However, if we check the glibc verison with V8_GLIBC_PREPREQ here at compile
// time, this causes two problems due to dynamic linking of glibc:
// 1) If the compiling system _has_ a new enough glibc, the binary will include
// calls to {pkey_alloc()} etc., and then the runtime system must supply a
// new enough glibc version as well. That is, this potentially breaks runtime
// compatability on older systems (e.g., Ubuntu 16.04 with glibc 2.23).
// 2) If the compiling system _does not_ have a new enough glibc, PKU support
// will not be compiled in, even though the runtime system potentially _does_
// have support for it due to a new enough Linux kernel and glibc version.
// That is, this results in non-optimal security (PKU available, but not used).
// Hence, we do _not_ check the glibc version during compilation, and instead
// only at runtime try to load {pkey_alloc()} etc. with {dlsym()}.
// TODO(dlehmann): Move this import and freestanding functions below to
// base/platform/platform.h {OS} (lower-level functions) and
// {base::PageAllocator} (exported API).
#if V8_HAS_PKU_JIT_WRITE_PROTECT
#include <dlfcn.h>
#endif

namespace v8 {
namespace base {

namespace {
using pkey_mprotect_t = int (*)(void*, size_t, int, int);
using pkey_get_t = int (*)(int);
using pkey_set_t = int (*)(int, unsigned);

pkey_mprotect_t pkey_mprotect = nullptr;
pkey_get_t pkey_get = nullptr;
pkey_set_t pkey_set = nullptr;

#if DEBUG
bool pkey_api_initialized = false;
#endif

int GetProtectionFromMemoryPermission(PageAllocator::Permission permission) {
#if V8_HAS_PKU_JIT_WRITE_PROTECT
  // Mappings for PKU are either RWX (for code), no access (for uncommitted
  // memory), or RO for globals.
  switch (permission) {
    case PageAllocator::kNoAccess:
      return PROT_NONE;
    case PageAllocator::kRead:
      return PROT_READ;
    case PageAllocator::kReadWriteExecute:
      return PROT_READ | PROT_WRITE | PROT_EXEC;
    default:
      UNREACHABLE();
  }
#endif
  // Other platforms do not use PKU.
  UNREACHABLE();
}

}  // namespace

bool MemoryProtectionKey::InitializeMemoryProtectionKeySupport() {
  // Flip {pkey_api_initialized} (in debug mode) and check the new value.
  DCHECK_EQ(true, pkey_api_initialized = !pkey_api_initialized);
#if V8_HAS_PKU_JIT_WRITE_PROTECT
  // Try to find the pkey functions in glibc.
  void* pkey_mprotect_ptr = dlsym(RTLD_DEFAULT, "pkey_mprotect");
  if (!pkey_mprotect_ptr) return false;
  // If {pkey_mprotect} is available, the others must also be available.
  void* pkey_get_ptr = dlsym(RTLD_DEFAULT, "pkey_get");
  void* pkey_set_ptr = dlsym(RTLD_DEFAULT, "pkey_set");
  CHECK(pkey_get_ptr && pkey_set_ptr);

  pkey_mprotect = reinterpret_cast<pkey_mprotect_t>(pkey_mprotect_ptr);
  pkey_get = reinterpret_cast<pkey_get_t>(pkey_get_ptr);
  pkey_set = reinterpret_cast<pkey_set_t>(pkey_set_ptr);

  return true;
#else  // V8_HAS_PKU_JIT_WRITE_PROTECT
  UNREACHABLE();
#endif
}

// static
DISABLE_CFI_ICALL
bool MemoryProtectionKey::SetPermissionsAndKey(
    v8::PageAllocator* page_allocator, base::AddressRegion region,
    v8::PageAllocator::Permission page_permissions, int key) {
  DCHECK(pkey_api_initialized);
  DCHECK_NE(key, kNoMemoryProtectionKey);
  CHECK_NOT_NULL(pkey_mprotect);

  void* address = reinterpret_cast<void*>(region.begin());
  size_t size = region.size();

  // Copied with slight modifications from base/platform/platform-posix.cc
  // {OS::SetPermissions()}.
  // TODO(dlehmann): Move this block into its own function at the right
  // abstraction boundary (likely some static method in platform.h {OS})
  // once the whole PKU code is moved into base/platform/.
  DCHECK_EQ(0, region.begin() % page_allocator->CommitPageSize());
  DCHECK_EQ(0, size % page_allocator->CommitPageSize());

  int protection = GetProtectionFromMemoryPermission(page_permissions);

  int ret = pkey_mprotect(address, size, protection, key);

  if (ret == 0 && page_permissions == PageAllocator::kNoAccess) {
    // Similar to {OS::SetPermissions}, also discard the pages after switching
    // to no access. This is advisory; ignore errors and continue execution.
    USE(page_allocator->DiscardSystemPages(address, size));
  }

  return ret == /* success */ 0;
}

// static
DISABLE_CFI_ICALL
void MemoryProtectionKey::SetPermissionsForKey(int key,
                                               Permission permissions) {
  DCHECK(pkey_api_initialized);
  DCHECK_NE(kNoMemoryProtectionKey, key);

  // If a valid key was allocated, {pkey_set()} must also be available.
  DCHECK_NOT_NULL(pkey_set);

  CHECK_EQ(0 /* success */, pkey_set(key, permissions));
}

// static
DISABLE_CFI_ICALL
MemoryProtectionKey::Permission MemoryProtectionKey::GetKeyPermission(int key) {
  DCHECK(pkey_api_initialized);
  DCHECK_NE(kNoMemoryProtectionKey, key);

  // If a valid key was allocated, {pkey_get()} must also be available.
  DCHECK_NOT_NULL(pkey_get);

  int permission = pkey_get(key);
  CHECK(permission == kNoRestrictions || permission == kDisableAccess ||
        permission == kDisableWrite);
  return static_cast<Permission>(permission);
}

}  // namespace base
}  // namespace v8
