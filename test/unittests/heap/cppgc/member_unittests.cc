// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "include/cppgc/allocation.h"
#include "include/cppgc/garbage-collected.h"
#include "include/cppgc/member.h"
#include "include/cppgc/type-traits.h"

#include "test/unittests/heap/cppgc/tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

namespace {

struct GCed : GarbageCollected<GCed> {};
struct DerivedGCed : GCed {};

// Compile tests.
static_assert(!IsWeakV<Member<GCed>>, "Member is always strong.");
static_assert(IsWeakV<WeakMember<GCed>>, "WeakMember is always weak.");

struct CustomWriteBarrierPolicy {
  static size_t InitializingWriteBarriersTriggered;
  static size_t AssigningWriteBarriersTriggered;
  static void InitializingBarrier(const void* slot, const void* value) {
    ++InitializingWriteBarriersTriggered;
  }
  static void AssigningBarrier(const void* slot, const void* value) {
    ++AssigningWriteBarriersTriggered;
  }
};
size_t CustomWriteBarrierPolicy::InitializingWriteBarriersTriggered = 0;
size_t CustomWriteBarrierPolicy::AssigningWriteBarriersTriggered = 0;

using MemberWithCustomBarrier =
    BasicStrongMember<GCed, CustomWriteBarrierPolicy>;

struct CustomCheckingPolicy {
  static std::vector<GCed*> Cached;
  static size_t ChecksTriggered;
  void CheckPointer(const void* ptr) {
    EXPECT_NE(Cached.cend(), std::find(Cached.cbegin(), Cached.cend(), ptr));
    ++ChecksTriggered;
  }
};
std::vector<GCed*> CustomCheckingPolicy::Cached;
size_t CustomCheckingPolicy::ChecksTriggered = 0;

using MemberWithCustomChecking =
    internal::BasicMember<GCed, class StrongMemberTag,
                          DijkstraWriteBarrierPolicy, CustomCheckingPolicy>;

class MemberTest : public testing::TestSupportingAllocationOnly {};

}  // namespace

template <template <typename> class Member>
void EmptyTest() {
  {
    Member<GCed> empty;
    EXPECT_EQ(nullptr, empty.Get());
    EXPECT_EQ(nullptr, empty.Release());
  }
  {
    Member<GCed> empty = nullptr;
    EXPECT_EQ(nullptr, empty.Get());
    EXPECT_EQ(nullptr, empty.Release());
  }
}

TEST_F(MemberTest, Empty) {
  EmptyTest<Member>();
  EmptyTest<WeakMember>();
  EmptyTest<UntracedMember>();
}

template <template <typename> class Member>
void ClearTest(cppgc::Heap* heap) {
  Member<GCed> member = MakeGarbageCollected<GCed>(heap);
  EXPECT_NE(nullptr, member.Get());
  member.Clear();
  EXPECT_EQ(nullptr, member.Get());
}

TEST_F(MemberTest, Clear) {
  cppgc::Heap* heap = GetHeap();
  ClearTest<Member>(heap);
  ClearTest<WeakMember>(heap);
  ClearTest<UntracedMember>(heap);
}

template <template <typename> class Member>
void ReleaseTest(cppgc::Heap* heap) {
  GCed* gced = MakeGarbageCollected<GCed>(heap);
  Member<GCed> member = gced;
  EXPECT_NE(nullptr, member.Get());
  GCed* raw = member.Release();
  EXPECT_EQ(gced, raw);
  EXPECT_EQ(nullptr, member.Get());
}

TEST_F(MemberTest, Release) {
  cppgc::Heap* heap = GetHeap();
  ReleaseTest<Member>(heap);
  ReleaseTest<WeakMember>(heap);
  ReleaseTest<UntracedMember>(heap);
}

template <template <typename> class Member1, template <typename> class Member2>
void SwapTest(cppgc::Heap* heap) {
  GCed* gced1 = MakeGarbageCollected<GCed>(heap);
  GCed* gced2 = MakeGarbageCollected<GCed>(heap);
  Member1<GCed> member1 = gced1;
  Member2<GCed> member2 = gced2;
  EXPECT_EQ(gced1, member1.Get());
  EXPECT_EQ(gced2, member2.Get());
  member1.Swap(member2);
  EXPECT_EQ(gced2, member1.Get());
  EXPECT_EQ(gced1, member2.Get());
}

TEST_F(MemberTest, Swap) {
  cppgc::Heap* heap = GetHeap();
  SwapTest<Member, Member>(heap);
  SwapTest<Member, WeakMember>(heap);
  SwapTest<Member, UntracedMember>(heap);
  SwapTest<WeakMember, Member>(heap);
  SwapTest<WeakMember, WeakMember>(heap);
  SwapTest<WeakMember, UntracedMember>(heap);
  SwapTest<UntracedMember, Member>(heap);
  SwapTest<UntracedMember, WeakMember>(heap);
  SwapTest<UntracedMember, UntracedMember>(heap);
}

template <template <typename> class Member1, template <typename> class Member2>
void HeterogeneousConversionTest(cppgc::Heap* heap) {
  {
    Member1<GCed> member1 = MakeGarbageCollected<GCed>(heap);
    Member2<GCed> member2 = member1;
    EXPECT_EQ(member1.Get(), member2.Get());
  }
  {
    Member1<DerivedGCed> member1 = MakeGarbageCollected<DerivedGCed>(heap);
    Member2<GCed> member2 = member1;
    EXPECT_EQ(member1.Get(), member2.Get());
  }
  {
    Member1<GCed> member1 = MakeGarbageCollected<GCed>(heap);
    Member2<GCed> member2;
    member2 = member1;
    EXPECT_EQ(member1.Get(), member2.Get());
  }
  {
    Member1<DerivedGCed> member1 = MakeGarbageCollected<DerivedGCed>(heap);
    Member2<GCed> member2;
    member2 = member1;
    EXPECT_EQ(member1.Get(), member2.Get());
  }
}

TEST_F(MemberTest, HeterogeneousInterface) {
  cppgc::Heap* heap = GetHeap();
  HeterogeneousConversionTest<Member, Member>(heap);
  HeterogeneousConversionTest<Member, WeakMember>(heap);
  HeterogeneousConversionTest<Member, UntracedMember>(heap);
  HeterogeneousConversionTest<WeakMember, Member>(heap);
  HeterogeneousConversionTest<WeakMember, WeakMember>(heap);
  HeterogeneousConversionTest<WeakMember, UntracedMember>(heap);
  HeterogeneousConversionTest<UntracedMember, Member>(heap);
  HeterogeneousConversionTest<UntracedMember, WeakMember>(heap);
  HeterogeneousConversionTest<UntracedMember, UntracedMember>(heap);
}

template <template <typename> class Member1, template <typename> class Member2>
void EqualityTest(cppgc::Heap* heap) {
  {
    GCed* gced = MakeGarbageCollected<GCed>(heap);
    Member1<GCed> member1 = gced;
    Member2<GCed> member2 = gced;
    EXPECT_TRUE(member1 == member2);
    EXPECT_FALSE(member1 != member2);
    member2 = member1;
    EXPECT_TRUE(member1 == member2);
    EXPECT_FALSE(member1 != member2);
  }
  {
    Member1<GCed> member1 = MakeGarbageCollected<GCed>(heap);
    Member2<GCed> member2 = MakeGarbageCollected<GCed>(heap);
    EXPECT_TRUE(member1 != member2);
    EXPECT_FALSE(member1 == member2);
  }
}

TEST_F(MemberTest, EqualityTest) {
  cppgc::Heap* heap = GetHeap();
  EqualityTest<Member, Member>(heap);
  EqualityTest<Member, WeakMember>(heap);
  EqualityTest<Member, UntracedMember>(heap);
  EqualityTest<WeakMember, Member>(heap);
  EqualityTest<WeakMember, WeakMember>(heap);
  EqualityTest<WeakMember, UntracedMember>(heap);
  EqualityTest<UntracedMember, Member>(heap);
  EqualityTest<UntracedMember, WeakMember>(heap);
  EqualityTest<UntracedMember, UntracedMember>(heap);
}

TEST_F(MemberTest, WriteBarrierTriggered) {
  CustomWriteBarrierPolicy::InitializingWriteBarriersTriggered = 0;
  CustomWriteBarrierPolicy::AssigningWriteBarriersTriggered = 0;
  GCed* gced = MakeGarbageCollected<GCed>(GetHeap());
  MemberWithCustomBarrier member1 = gced;
  EXPECT_EQ(1u, CustomWriteBarrierPolicy::InitializingWriteBarriersTriggered);
  EXPECT_EQ(0u, CustomWriteBarrierPolicy::AssigningWriteBarriersTriggered);
  member1 = gced;
  EXPECT_EQ(1u, CustomWriteBarrierPolicy::InitializingWriteBarriersTriggered);
  EXPECT_EQ(1u, CustomWriteBarrierPolicy::AssigningWriteBarriersTriggered);
  member1 = nullptr;
  EXPECT_EQ(1u, CustomWriteBarrierPolicy::InitializingWriteBarriersTriggered);
  EXPECT_EQ(1u, CustomWriteBarrierPolicy::AssigningWriteBarriersTriggered);
  MemberWithCustomBarrier member2 = nullptr;
  // No initializing barriers for std::nullptr_t.
  EXPECT_EQ(1u, CustomWriteBarrierPolicy::InitializingWriteBarriersTriggered);
  EXPECT_EQ(1u, CustomWriteBarrierPolicy::AssigningWriteBarriersTriggered);
  member2 = kMemberSentinel;
  // No initializing barriers for member sentinel.
  EXPECT_EQ(1u, CustomWriteBarrierPolicy::InitializingWriteBarriersTriggered);
  EXPECT_EQ(1u, CustomWriteBarrierPolicy::AssigningWriteBarriersTriggered);
  member2.Swap(member1);
  EXPECT_EQ(3u, CustomWriteBarrierPolicy::AssigningWriteBarriersTriggered);
}

TEST_F(MemberTest, CheckingPolicy) {
  static constexpr size_t kElements = 64u;
  CustomCheckingPolicy::ChecksTriggered = 0u;

  for (std::size_t i = 0; i < kElements; ++i) {
    CustomCheckingPolicy::Cached.push_back(
        MakeGarbageCollected<GCed>(GetHeap()));
  }

  MemberWithCustomChecking member;
  for (GCed* item : CustomCheckingPolicy::Cached) {
    member = item;
  }
  EXPECT_EQ(CustomCheckingPolicy::Cached.size(),
            CustomCheckingPolicy::ChecksTriggered);
}
}  // namespace internal
}  // namespace cppgc
