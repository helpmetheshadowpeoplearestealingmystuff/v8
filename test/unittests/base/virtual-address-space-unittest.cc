// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/virtual-address-space.h"

#include "src/base/emulated-virtual-address-subspace.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace base {

constexpr size_t KB = 1024;
constexpr size_t MB = KB * 1024;

void TestRandomPageAddressGeneration(v8::VirtualAddressSpace* space) {
  space->SetRandomSeed(::testing::FLAGS_gtest_random_seed);
  for (int i = 0; i < 10; i++) {
    Address addr = space->RandomPageAddress();
    EXPECT_GE(addr, space->base());
    EXPECT_LT(addr, space->base() + space->size());
  }
}

void TestBasicPageAllocation(v8::VirtualAddressSpace* space) {
  // Allocation sizes in KB.
  const size_t allocation_sizes[] = {4,   8,   12,  16,   32,  64,  128,
                                     256, 512, 768, 1024, 768, 512, 256,
                                     128, 64,  32,  16,   12,  8,   4};

  std::vector<Address> allocations;
  size_t alignment = space->allocation_granularity();
  for (size_t i = 0; i < arraysize(allocation_sizes); i++) {
    size_t size = allocation_sizes[i] * KB;
    if (!IsAligned(size, space->allocation_granularity())) continue;
    Address allocation =
        space->AllocatePages(VirtualAddressSpace::kNoHint, size, alignment,
                             PagePermissions::kReadWrite);

    ASSERT_NE(kNullAddress, allocation);
    EXPECT_GE(allocation, space->base());
    EXPECT_LT(allocation, space->base() + space->size());

    allocations.push_back(allocation);

    // Memory must be writable
    *reinterpret_cast<size_t*>(allocation) = size;
  }

  // Windows has an allocation granularity of 64KB and macOS could have 16KB, so
  // we won't necessarily have managed to obtain all allocations, but we
  // should've gotten all that are >= 64KB.
  EXPECT_GE(allocations.size(), 11UL);

  for (Address allocation : allocations) {
    //... and readable
    size_t size = *reinterpret_cast<size_t*>(allocation);
    EXPECT_TRUE(space->FreePages(allocation, size));
  }
}

void TestPageAllocationAlignment(v8::VirtualAddressSpace* space) {
  // In multiples of the allocation_granularity.
  const size_t alignments[] = {1, 2, 4, 8, 16, 32, 64};
  const size_t size = space->allocation_granularity();

  for (size_t i = 0; i < arraysize(alignments); i++) {
    size_t alignment = alignments[i] * space->allocation_granularity();
    Address allocation =
        space->AllocatePages(VirtualAddressSpace::kNoHint, size, alignment,
                             PagePermissions::kReadWrite);

    ASSERT_NE(kNullAddress, allocation);
    EXPECT_TRUE(IsAligned(allocation, alignment));
    EXPECT_GE(allocation, space->base());
    EXPECT_LT(allocation, space->base() + space->size());

    EXPECT_TRUE(space->FreePages(allocation, size));
  }
}

void TestParentSpaceCannotAllocateInChildSpace(v8::VirtualAddressSpace* parent,
                                               v8::VirtualAddressSpace* child) {
  child->SetRandomSeed(::testing::FLAGS_gtest_random_seed);

  size_t chunksize = parent->allocation_granularity();
  size_t alignment = chunksize;
  Address start = child->base();
  Address end = start + child->size();

  for (int i = 0; i < 10; i++) {
    Address hint = child->RandomPageAddress();
    Address allocation = parent->AllocatePages(hint, chunksize, alignment,
                                               PagePermissions::kNoAccess);
    ASSERT_NE(kNullAddress, allocation);
    EXPECT_TRUE(allocation < start || allocation >= end);
    EXPECT_TRUE(parent->FreePages(allocation, chunksize));
  }
}

TEST(VirtualAddressSpaceTest, TestRootSpace) {
  VirtualAddressSpace rootspace;

  TestRandomPageAddressGeneration(&rootspace);
  TestBasicPageAllocation(&rootspace);
  TestPageAllocationAlignment(&rootspace);
}

TEST(VirtualAddressSpaceTest, TestSubspace) {
  constexpr size_t kSubspaceSize = 32 * MB;
  constexpr size_t kSubSubspaceSize = 16 * MB;

  VirtualAddressSpace rootspace;

  if (!rootspace.CanAllocateSubspaces()) return;
  size_t subspace_alignment = rootspace.allocation_granularity();
  auto subspace = rootspace.AllocateSubspace(
      VirtualAddressSpace::kNoHint, kSubspaceSize, subspace_alignment,
      PagePermissions::kReadWriteExecute);
  ASSERT_TRUE(subspace);
  EXPECT_NE(kNullAddress, subspace->base());
  EXPECT_EQ(kSubspaceSize, subspace->size());

  TestRandomPageAddressGeneration(subspace.get());
  TestBasicPageAllocation(subspace.get());
  TestPageAllocationAlignment(subspace.get());
  TestParentSpaceCannotAllocateInChildSpace(&rootspace, subspace.get());

  // Test sub-subspaces
  if (!subspace->CanAllocateSubspaces()) return;
  size_t subsubspace_alignment = subspace->allocation_granularity();
  auto subsubspace = subspace->AllocateSubspace(
      VirtualAddressSpace::kNoHint, kSubSubspaceSize, subsubspace_alignment,
      PagePermissions::kReadWriteExecute);
  ASSERT_TRUE(subsubspace);
  EXPECT_NE(kNullAddress, subsubspace->base());
  EXPECT_EQ(kSubSubspaceSize, subsubspace->size());

  TestRandomPageAddressGeneration(subsubspace.get());
  TestBasicPageAllocation(subsubspace.get());
  TestPageAllocationAlignment(subsubspace.get());
  TestParentSpaceCannotAllocateInChildSpace(subspace.get(), subsubspace.get());
}

TEST(VirtualAddressSpaceTest, TestEmulatedSubspace) {
  constexpr size_t kSubspaceSize = 32 * MB;
  // Size chosen so page allocation tests will obtain pages in both the mapped
  // and the unmapped region.
  constexpr size_t kSubspaceMappedSize = 1 * MB;

  VirtualAddressSpace rootspace;

  size_t subspace_alignment = rootspace.allocation_granularity();
  ASSERT_TRUE(
      IsAligned(kSubspaceMappedSize, rootspace.allocation_granularity()));
  Address reservation = kNullAddress;
  for (int i = 0; i < 10; i++) {
    // Reserve the full size first at a random address, then free it again to
    // ensure that there's enough free space behind the final reservation.
    Address hint = rootspace.RandomPageAddress();
    reservation = rootspace.AllocatePages(hint, kSubspaceSize,
                                          rootspace.allocation_granularity(),
                                          PagePermissions::kNoAccess);
    ASSERT_NE(kNullAddress, reservation);
    hint = reservation;
    EXPECT_TRUE(rootspace.FreePages(reservation, kSubspaceSize));
    reservation =
        rootspace.AllocatePages(hint, kSubspaceMappedSize, subspace_alignment,
                                PagePermissions::kNoAccess);
    if (reservation == hint) {
      break;
    } else {
      EXPECT_TRUE(rootspace.FreePages(reservation, kSubspaceMappedSize));
      reservation = kNullAddress;
    }
  }
  ASSERT_NE(kNullAddress, reservation);

  EmulatedVirtualAddressSubspace subspace(&rootspace, reservation,
                                          kSubspaceMappedSize, kSubspaceSize);
  EXPECT_EQ(reservation, subspace.base());
  EXPECT_EQ(kSubspaceSize, subspace.size());

  TestRandomPageAddressGeneration(&subspace);
  TestBasicPageAllocation(&subspace);
  TestPageAllocationAlignment(&subspace);
  // An emulated subspace does *not* guarantee that the parent space cannot
  // allocate pages in it, so no TestParentSpaceCannotAllocateInChildSpace.
}

}  // namespace base
}  // namespace v8
