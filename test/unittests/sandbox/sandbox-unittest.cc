// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/sandbox/sandbox.h"

#include <vector>

#include "src/base/virtual-address-space.h"
#include "test/unittests/test-utils.h"

#ifdef V8_ENABLE_SANDBOX

namespace v8 {
namespace internal {

TEST(SandboxTest, Initialization) {
  base::VirtualAddressSpace vas;

  Sandbox sandbox;

  EXPECT_FALSE(sandbox.is_initialized());
  EXPECT_FALSE(sandbox.is_disabled());
  EXPECT_FALSE(sandbox.is_partially_reserved());
  EXPECT_EQ(sandbox.size(), 0UL);

  EXPECT_TRUE(sandbox.Initialize(&vas));

  EXPECT_TRUE(sandbox.is_initialized());
  EXPECT_NE(sandbox.base(), 0UL);
  EXPECT_GT(sandbox.size(), 0UL);

  sandbox.TearDown();

  EXPECT_FALSE(sandbox.is_initialized());
}

TEST(SandboxTest, InitializationWithSize) {
  base::VirtualAddressSpace vas;
  // This test only works if virtual memory subspaces can be allocated.
  if (!vas.CanAllocateSubspaces()) return;

  Sandbox sandbox;
  size_t size = kSandboxMinimumSize;
  const bool use_guard_regions = false;
  EXPECT_TRUE(sandbox.Initialize(&vas, size, use_guard_regions));

  EXPECT_TRUE(sandbox.is_initialized());
  EXPECT_FALSE(sandbox.is_partially_reserved());
  EXPECT_EQ(sandbox.size(), size);

  sandbox.TearDown();
}

TEST(SandboxTest, PartiallyReservedSandboxInitialization) {
  base::VirtualAddressSpace vas;
  Sandbox sandbox;
  // Total size of the sandbox.
  size_t size = kSandboxSize;
  // Size of the virtual memory that is actually reserved at the start of the
  // sandbox.
  size_t reserved_size = 2 * vas.allocation_granularity();
  EXPECT_TRUE(
      sandbox.InitializeAsPartiallyReservedSandbox(&vas, size, reserved_size));

  EXPECT_TRUE(sandbox.is_initialized());
  EXPECT_TRUE(sandbox.is_partially_reserved());
  EXPECT_NE(sandbox.base(), 0UL);
  EXPECT_EQ(sandbox.size(), size);

  sandbox.TearDown();

  EXPECT_FALSE(sandbox.is_initialized());
}

TEST(SandboxTest, Contains) {
  base::VirtualAddressSpace vas;
  Sandbox sandbox;
  EXPECT_TRUE(sandbox.Initialize(&vas));

  Address base = sandbox.base();
  size_t size = sandbox.size();
  base::RandomNumberGenerator rng(::testing::FLAGS_gtest_random_seed);

  EXPECT_TRUE(sandbox.Contains(base));
  EXPECT_TRUE(sandbox.Contains(base + size - 1));
  for (int i = 0; i < 10; i++) {
    size_t offset = rng.NextInt64() % size;
    EXPECT_TRUE(sandbox.Contains(base + offset));
  }

  EXPECT_FALSE(sandbox.Contains(base - 1));
  EXPECT_FALSE(sandbox.Contains(base + size));
  for (int i = 0; i < 10; i++) {
    Address addr = rng.NextInt64();
    if (addr < base || addr >= base + size) {
      EXPECT_FALSE(sandbox.Contains(addr));
    }
  }

  sandbox.TearDown();
}

TEST(SandboxTest, PageAllocation) {
  base::VirtualAddressSpace root_vas;
  Sandbox sandbox;
  EXPECT_TRUE(sandbox.Initialize(&root_vas));

  const size_t kAllocatinSizesInPages[] = {1, 1, 2, 3, 5, 8, 13, 21, 34};
  constexpr int kNumAllocations = arraysize(kAllocatinSizesInPages);

  VirtualAddressSpace* vas = sandbox.address_space();
  size_t allocation_granularity = vas->allocation_granularity();
  std::vector<Address> allocations;
  for (int i = 0; i < kNumAllocations; i++) {
    size_t length = allocation_granularity * kAllocatinSizesInPages[i];
    size_t alignment = allocation_granularity;
    Address ptr = vas->AllocatePages(VirtualAddressSpace::kNoHint, length,
                                     alignment, PagePermissions::kNoAccess);
    EXPECT_NE(ptr, kNullAddress);
    EXPECT_TRUE(sandbox.Contains(ptr));
    allocations.push_back(ptr);
  }

  for (int i = 0; i < kNumAllocations; i++) {
    size_t length = allocation_granularity * kAllocatinSizesInPages[i];
    vas->FreePages(allocations[i], length);
  }

  sandbox.TearDown();
}

}  // namespace internal
}  // namespace v8

#endif  // V8_ENABLE_SANDBOX
