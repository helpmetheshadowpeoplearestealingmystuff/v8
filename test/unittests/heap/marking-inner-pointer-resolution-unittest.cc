// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/mark-compact.h"
#include "test/unittests/heap/heap-utils.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {

#ifdef V8_ENABLE_INNER_POINTER_RESOLUTION_MB
namespace {

class InnerPointerResolutionTest : public TestWithIsolate {
 public:
  struct ObjectRequest {
    int size;  // The only required field.
    enum { REGULAR, FREE, LARGE } type = REGULAR;
    enum { WHITE, GREY, BLACK, BLACK_AREA } marked = WHITE;
    // If index_in_cell >= 0, the object is placed at the lowest address s.t.
    // Bitmap::IndexInCell(AddressToMarkbitIndex(address)) == index_in_cell.
    // To achieve this, padding (i.e., introducing a free-space object of the
    // appropriate size) may be necessary. If padding == CONSECUTIVE, no such
    // padding is allowed and it is just checked that object layout is as
    // intended.
    int index_in_cell = -1;
    enum { CONSECUTIVE, PAD_WHITE, PAD_BLACK } padding = CONSECUTIVE;
    // The id of the page on which the object was allocated and its address are
    // stored here.
    int page_id = -1;
    Address address = kNullAddress;
  };

  InnerPointerResolutionTest() = default;

  ~InnerPointerResolutionTest() override {
    for (auto [id, page] : pages_)
      allocator()->Free(MemoryAllocator::FreeMode::kImmediately, page);
  }

  InnerPointerResolutionTest(const InnerPointerResolutionTest&) = delete;
  InnerPointerResolutionTest& operator=(const InnerPointerResolutionTest&) =
      delete;

  Heap* heap() { return isolate()->heap(); }
  MemoryAllocator* allocator() { return heap()->memory_allocator(); }
  MarkCompactCollector* collector() { return heap()->mark_compact_collector(); }

  // Create, free and lookup pages, normal or large.

  int CreateNormalPage() {
    OldSpace* old_space = heap()->old_space();
    DCHECK_NE(nullptr, old_space);
    auto* page = allocator()->AllocatePage(
        MemoryAllocator::AllocationMode::kRegular, old_space, NOT_EXECUTABLE);
    EXPECT_NE(nullptr, page);
    int page_id = next_page_id_++;
    DCHECK_EQ(pages_.end(), pages_.find(page_id));
    pages_[page_id] = page;
    return page_id;
  }

  int CreateLargePage(size_t size) {
    OldLargeObjectSpace* lo_space = heap()->lo_space();
    EXPECT_NE(nullptr, lo_space);
    LargePage* page =
        allocator()->AllocateLargePage(lo_space, size, NOT_EXECUTABLE);
    EXPECT_NE(nullptr, page);
    int page_id = next_page_id_++;
    DCHECK_EQ(pages_.end(), pages_.find(page_id));
    pages_[page_id] = page;
    return page_id;
  }

  void FreePage(int page_id) {
    DCHECK_LE(0, page_id);
    auto it = pages_.find(page_id);
    DCHECK_NE(pages_.end(), it);
    allocator()->Free(MemoryAllocator::FreeMode::kImmediately, it->second);
    pages_.erase(it);
  }

  MemoryChunk* LookupPage(int page_id) {
    DCHECK_LE(0, page_id);
    auto it = pages_.find(page_id);
    DCHECK_NE(pages_.end(), it);
    return it->second;
  }

  bool IsPageAlive(int page_id) {
    DCHECK_LE(0, page_id);
    return pages_.find(page_id) != pages_.end();
  }

  // Creates a list of objects in a page and ensures that the page is iterable.
  int CreateObjectsInPage(const std::vector<ObjectRequest>& objects) {
    int page_id = CreateNormalPage();
    MemoryChunk* page = LookupPage(page_id);
    Address ptr = page->area_start();
    for (auto object : objects) {
      DCHECK_NE(ObjectRequest::LARGE, object.type);
      DCHECK_EQ(0, object.size % kTaggedSize);

      // Check if padding is needed.
      int index_in_cell = Bitmap::IndexInCell(page->AddressToMarkbitIndex(ptr));
      if (object.index_in_cell < 0) {
        object.index_in_cell = index_in_cell;
      } else if (object.padding != ObjectRequest::CONSECUTIVE) {
        DCHECK_LE(0, object.index_in_cell);
        DCHECK_GT(Bitmap::kBitsPerCell, object.index_in_cell);
        const int needed_padding_size =
            ((Bitmap::kBitsPerCell + object.index_in_cell - index_in_cell) %
             Bitmap::kBitsPerCell) *
            Bitmap::kBytesPerCell;
        if (needed_padding_size > 0) {
          ObjectRequest pad{needed_padding_size,
                            ObjectRequest::FREE,
                            object.padding == ObjectRequest::PAD_BLACK
                                ? ObjectRequest::BLACK_AREA
                                : ObjectRequest::WHITE,
                            index_in_cell,
                            ObjectRequest::CONSECUTIVE,
                            page_id,
                            ptr};
          ptr += needed_padding_size;
          DCHECK_LE(ptr, page->area_end());
          CreateObject(pad);
          index_in_cell = Bitmap::IndexInCell(page->AddressToMarkbitIndex(ptr));
        }
      }

      // This will fail if the marking bitmap's implementation parameters change
      // (e.g., Bitmap::kBitsPerCell) or the size of the page header changes.
      // In this case, the tests will need to be revised accordingly.
      EXPECT_EQ(index_in_cell, object.index_in_cell);

      object.page_id = page_id;
      object.address = ptr;
      ptr += object.size;
      DCHECK_LE(ptr, page->area_end());
      CreateObject(object);
    }

    // Create one last object that uses the remaining space on the page; this
    // simulates freeing the page's LAB.
    const int remaining_size = static_cast<int>(page->area_end() - ptr);
    const uint32_t index = page->AddressToMarkbitIndex(ptr);
    const int index_in_cell = Bitmap::IndexInCell(index);
    ObjectRequest last{remaining_size,
                       ObjectRequest::FREE,
                       ObjectRequest::WHITE,
                       index_in_cell,
                       ObjectRequest::CONSECUTIVE,
                       page_id,
                       ptr};
    CreateObject(last);
    return page_id;
  }

  std::vector<int> CreateLargeObjects(
      const std::vector<ObjectRequest>& objects) {
    std::vector<int> result;
    for (auto object : objects) {
      DCHECK_EQ(ObjectRequest::LARGE, object.type);
      int page_id = CreateLargePage(object.size);
      MemoryChunk* page = LookupPage(page_id);
      object.page_id = page_id;
      object.address = page->area_start();
      CHECK_EQ(object.address + object.size, page->area_end());
      CreateObject(object);
      result.push_back(page_id);
    }
    return result;
  }

  void CreateObject(const ObjectRequest& object) {
    objects_.push_back(object);

    // "Allocate" (i.e., manually place) the object in the page, set the map
    // and the size.
    switch (object.type) {
      case ObjectRequest::REGULAR:
      case ObjectRequest::LARGE: {
        DCHECK_LE(2 * kTaggedSize, object.size);
        ReadOnlyRoots roots(heap());
        HeapObject heap_object(HeapObject::FromAddress(object.address));
        heap_object.set_map_after_allocation(roots.unchecked_fixed_array_map(),
                                             SKIP_WRITE_BARRIER);
        FixedArray arr(FixedArray::cast(heap_object));
        arr.set_length((object.size - FixedArray::SizeFor(0)) / kTaggedSize);
        DCHECK_EQ(object.size, arr.AllocatedSize());
        break;
      }
      case ObjectRequest::FREE:
        heap()->CreateFillerObjectAt(object.address, object.size);
        break;
    }

    // Mark the object in the bitmap, if necessary.
    switch (object.marked) {
      case ObjectRequest::WHITE:
        break;
      case ObjectRequest::GREY:
        collector()->marking_state()->WhiteToGrey(
            HeapObject::FromAddress(object.address));
        break;
      case ObjectRequest::BLACK:
        DCHECK_LE(2 * kTaggedSize, object.size);
        collector()->marking_state()->WhiteToBlack(
            HeapObject::FromAddress(object.address));
        break;
      case ObjectRequest::BLACK_AREA: {
        MemoryChunk* page = LookupPage(object.page_id);
        collector()->marking_state()->bitmap(page)->SetRange(
            page->AddressToMarkbitIndex(object.address),
            page->AddressToMarkbitIndex(object.address + object.size));
        break;
      }
    }
  }

  // This must be called with a created object and an offset inside it.
  void RunTestInside(const ObjectRequest& object, int offset) {
    DCHECK_LE(0, offset);
    DCHECK_GT(object.size, offset);
    Address base_ptr =
        collector()->FindBasePtrForMarking(object.address + offset);
    bool should_return_null =
        !IsPageAlive(object.page_id) || (object.type == ObjectRequest::FREE) ||
        (object.type == ObjectRequest::REGULAR &&
         (object.marked == ObjectRequest::BLACK_AREA ||
          (object.marked == ObjectRequest::BLACK && offset < 2 * kTaggedSize) ||
          (object.marked == ObjectRequest::GREY && offset < kTaggedSize)));
    if (should_return_null)
      EXPECT_EQ(kNullAddress, base_ptr);
    else
      EXPECT_EQ(object.address, base_ptr);
  }

  // This must be called with an address not contained in any created object.
  void RunTestOutside(Address ptr) {
    Address base_ptr = collector()->FindBasePtrForMarking(ptr);
    EXPECT_EQ(kNullAddress, base_ptr);
  }

  void TestAll() {
    for (auto object : objects_) {
      RunTestInside(object, 0);
      RunTestInside(object, 1);
      RunTestInside(object, object.size / 2);
      RunTestInside(object, object.size - 1);
    }
    for (auto [id, page] : pages_) {
      const Address outside_ptr = page->area_start() - 42;
      DCHECK_LE(page->address(), outside_ptr);
      RunTestOutside(outside_ptr);
    }
    RunTestOutside(kNullAddress);
    RunTestOutside(static_cast<Address>(42));
    RunTestOutside(static_cast<Address>(kZapValue));
  }

 private:
  std::map<int, MemoryChunk*> pages_;
  int next_page_id_ = 0;
  std::vector<ObjectRequest> objects_;
};

}  // namespace

TEST_F(InnerPointerResolutionTest, EmptyPage) {
  if (v8_flags.enable_third_party_heap) return;
  CreateObjectsInPage({});
  TestAll();
}

// Tests with some objects laid out randomly.

TEST_F(InnerPointerResolutionTest, NothingMarked) {
  if (v8_flags.enable_third_party_heap) return;
  CreateObjectsInPage({
      {64},
      {48},
      {52},
      {512},
      {4, ObjectRequest::FREE},
      {60},
      {8, ObjectRequest::FREE},
      {8},
      {42176},
  });
  TestAll();
}

TEST_F(InnerPointerResolutionTest, AllMarked) {
  if (v8_flags.enable_third_party_heap) return;
  CreateObjectsInPage({
      {64, ObjectRequest::REGULAR, ObjectRequest::BLACK},
      {48, ObjectRequest::REGULAR, ObjectRequest::GREY},
      {52, ObjectRequest::REGULAR, ObjectRequest::BLACK},
      {512, ObjectRequest::REGULAR, ObjectRequest::BLACK},
      {4, ObjectRequest::FREE, ObjectRequest::GREY},
      {60, ObjectRequest::REGULAR, ObjectRequest::BLACK},
      {8, ObjectRequest::FREE, ObjectRequest::GREY},
      {8, ObjectRequest::REGULAR, ObjectRequest::BLACK},
      {42176, ObjectRequest::REGULAR, ObjectRequest::BLACK},
  });
  TestAll();
}

TEST_F(InnerPointerResolutionTest, SomeMarked) {
  if (v8_flags.enable_third_party_heap) return;
  CreateObjectsInPage({
      {64, ObjectRequest::REGULAR, ObjectRequest::WHITE},
      {48, ObjectRequest::REGULAR, ObjectRequest::WHITE},
      {52, ObjectRequest::REGULAR, ObjectRequest::BLACK},
      {512, ObjectRequest::REGULAR, ObjectRequest::WHITE},
      {4, ObjectRequest::FREE, ObjectRequest::GREY},
      {60, ObjectRequest::REGULAR, ObjectRequest::BLACK},
      {8, ObjectRequest::FREE, ObjectRequest::GREY},
      {8, ObjectRequest::REGULAR, ObjectRequest::WHITE},
      {42176, ObjectRequest::REGULAR, ObjectRequest::GREY},
  });
  TestAll();
}

TEST_F(InnerPointerResolutionTest, BlackAreas) {
  if (v8_flags.enable_third_party_heap) return;
  CreateObjectsInPage({
      {64, ObjectRequest::REGULAR, ObjectRequest::WHITE},
      {48, ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA},
      {52, ObjectRequest::REGULAR, ObjectRequest::BLACK},
      {512, ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA},
      {4, ObjectRequest::FREE, ObjectRequest::GREY},
      {60, ObjectRequest::REGULAR, ObjectRequest::BLACK},
      {8, ObjectRequest::FREE, ObjectRequest::GREY},
      {8, ObjectRequest::REGULAR, ObjectRequest::WHITE},
      {42176, ObjectRequest::REGULAR, ObjectRequest::GREY},
  });
  TestAll();
}

// Tests with specific object layout, to cover interesting and corner cases.

TEST_F(InnerPointerResolutionTest, ThreeMarkedObjectsInSameCell) {
  if (v8_flags.enable_third_party_heap) return;
  CreateObjectsInPage({
      // Some initial large unmarked object, followed by a small marked object
      // towards the end of the cell.
      {512},
      {20, ObjectRequest::REGULAR, ObjectRequest::BLACK, 20,
       ObjectRequest::PAD_WHITE},
      // Then three marked objects in the same cell.
      {32, ObjectRequest::REGULAR, ObjectRequest::BLACK, 3,
       ObjectRequest::PAD_WHITE},
      {48, ObjectRequest::REGULAR, ObjectRequest::BLACK, 11},
      {20, ObjectRequest::REGULAR, ObjectRequest::BLACK, 23},
      // This marked object is in the next cell.
      {64, ObjectRequest::REGULAR, ObjectRequest::BLACK, 17,
       ObjectRequest::PAD_WHITE},
  });
  TestAll();
}

TEST_F(InnerPointerResolutionTest, ThreeBlackAreasInSameCell) {
  if (v8_flags.enable_third_party_heap) return;
  CreateObjectsInPage({
      // Some initial large unmarked object, followed by a small black area
      // towards the end of the cell.
      {512},
      {20, ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA, 20,
       ObjectRequest::PAD_WHITE},
      // Then three black areas in the same cell.
      {32, ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA, 3,
       ObjectRequest::PAD_WHITE},
      {48, ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA, 11},
      {20, ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA, 23},
      // This black area is in the next cell.
      {64, ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA, 17,
       ObjectRequest::PAD_WHITE},
  });
  TestAll();
}

TEST_F(InnerPointerResolutionTest, SmallBlackAreaAtPageStart) {
  if (v8_flags.enable_third_party_heap) return;
  CreateObjectsInPage({
      {64, ObjectRequest::REGULAR, ObjectRequest::WHITE, 30,
       ObjectRequest::PAD_BLACK},
  });
  TestAll();
}

TEST_F(InnerPointerResolutionTest, SmallBlackAreaAtPageStartUntilCellBoundary) {
  if (v8_flags.enable_third_party_heap) return;
  CreateObjectsInPage({
      {8, ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA},
      {64, ObjectRequest::REGULAR, ObjectRequest::WHITE, 0,
       ObjectRequest::PAD_BLACK},
  });
  TestAll();
}

TEST_F(InnerPointerResolutionTest, LargeBlackAreaAtPageStart) {
  if (v8_flags.enable_third_party_heap) return;
  CreateObjectsInPage({
      {42 * Bitmap::kBitsPerCell * Bitmap::kBytesPerCell,
       ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA},
      {64, ObjectRequest::REGULAR, ObjectRequest::WHITE, 30,
       ObjectRequest::PAD_BLACK},
  });
  TestAll();
}

TEST_F(InnerPointerResolutionTest, LargeBlackAreaAtPageStartUntilCellBoundary) {
  if (v8_flags.enable_third_party_heap) return;
  CreateObjectsInPage({
      {42 * Bitmap::kBitsPerCell * Bitmap::kBytesPerCell,
       ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA},
      {64, ObjectRequest::REGULAR, ObjectRequest::WHITE, 0,
       ObjectRequest::PAD_BLACK},
  });
  TestAll();
}

TEST_F(InnerPointerResolutionTest, SmallBlackAreaStartingAtCellBoundary) {
  if (v8_flags.enable_third_party_heap) return;
  CreateObjectsInPage({
      {512},
      {20, ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA, 0,
       ObjectRequest::PAD_WHITE},
  });
  TestAll();
}

TEST_F(InnerPointerResolutionTest, LargeBlackAreaStartingAtCellBoundary) {
  if (v8_flags.enable_third_party_heap) return;
  CreateObjectsInPage({
      {512},
      {42 * Bitmap::kBitsPerCell * Bitmap::kBytesPerCell + 64,
       ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA, 0,
       ObjectRequest::PAD_WHITE},
  });
  TestAll();
}

TEST_F(InnerPointerResolutionTest, SmallBlackAreaEndingAtCellBoundary) {
  if (v8_flags.enable_third_party_heap) return;
  CreateObjectsInPage({
      {512},
      {8, ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA, 13,
       ObjectRequest::PAD_WHITE},
      {64, ObjectRequest::REGULAR, ObjectRequest::WHITE, 0,
       ObjectRequest::PAD_BLACK},
  });
  TestAll();
}

TEST_F(InnerPointerResolutionTest, LargeBlackAreaEndingAtCellBoundary) {
  if (v8_flags.enable_third_party_heap) return;
  CreateObjectsInPage({
      {512},
      {42 * Bitmap::kBitsPerCell * Bitmap::kBytesPerCell + 64,
       ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA, 0,
       ObjectRequest::PAD_WHITE},
      {64, ObjectRequest::REGULAR, ObjectRequest::WHITE, 0,
       ObjectRequest::PAD_BLACK},
  });
  TestAll();
}

TEST_F(InnerPointerResolutionTest, TwoSmallBlackAreasAtCellBoundaries) {
  if (v8_flags.enable_third_party_heap) return;
  CreateObjectsInPage({
      {512},
      {24, ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA, 0,
       ObjectRequest::PAD_WHITE},
      {8, ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA, 25,
       ObjectRequest::PAD_WHITE},
      {64, ObjectRequest::REGULAR, ObjectRequest::WHITE, 0,
       ObjectRequest::PAD_BLACK},
  });
  TestAll();
}

TEST_F(InnerPointerResolutionTest, BlackAreaOfOneCell) {
  if (v8_flags.enable_third_party_heap) return;
  CreateObjectsInPage({
      {512},
      {Bitmap::kBitsPerCell * Bitmap::kBytesPerCell, ObjectRequest::REGULAR,
       ObjectRequest::BLACK_AREA, 0, ObjectRequest::PAD_WHITE},
  });
  TestAll();
}

TEST_F(InnerPointerResolutionTest, BlackAreaOfManyCells) {
  if (v8_flags.enable_third_party_heap) return;
  CreateObjectsInPage({
      {512},
      {17 * Bitmap::kBitsPerCell * Bitmap::kBytesPerCell,
       ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA, 0,
       ObjectRequest::PAD_WHITE},
  });
  TestAll();
}

// Test with more pages, normal and large.

TEST_F(InnerPointerResolutionTest, TwoPages) {
  if (v8_flags.enable_third_party_heap) return;
  CreateObjectsInPage({
      {64, ObjectRequest::REGULAR, ObjectRequest::WHITE},
      {52, ObjectRequest::REGULAR, ObjectRequest::BLACK},
      {512, ObjectRequest::REGULAR, ObjectRequest::WHITE},
      {60, ObjectRequest::REGULAR, ObjectRequest::BLACK},
      {42176, ObjectRequest::REGULAR, ObjectRequest::GREY},
  });
  CreateObjectsInPage({
      {512, ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA},
      {64, ObjectRequest::REGULAR, ObjectRequest::WHITE},
      {48, ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA},
      {52, ObjectRequest::REGULAR, ObjectRequest::BLACK},
      {4, ObjectRequest::FREE, ObjectRequest::GREY},
      {8, ObjectRequest::FREE, ObjectRequest::GREY},
      {8, ObjectRequest::REGULAR, ObjectRequest::WHITE},
      {60, ObjectRequest::REGULAR, ObjectRequest::BLACK},
  });
  TestAll();
}

TEST_F(InnerPointerResolutionTest, OneLargePage) {
  if (v8_flags.enable_third_party_heap) return;
  CreateLargeObjects({
      {1 * MB, ObjectRequest::LARGE, ObjectRequest::WHITE},
  });
  TestAll();
}

TEST_F(InnerPointerResolutionTest, SeveralLargePages) {
  if (v8_flags.enable_third_party_heap) return;
  CreateLargeObjects({
      {1 * MB, ObjectRequest::LARGE, ObjectRequest::WHITE},
      {32 * MB, ObjectRequest::LARGE, ObjectRequest::BLACK},
  });
  TestAll();
}

TEST_F(InnerPointerResolutionTest, PagesOfBothKind) {
  if (v8_flags.enable_third_party_heap) return;
  CreateObjectsInPage({
      {64, ObjectRequest::REGULAR, ObjectRequest::WHITE},
      {52, ObjectRequest::REGULAR, ObjectRequest::BLACK},
      {512, ObjectRequest::REGULAR, ObjectRequest::WHITE},
      {60, ObjectRequest::REGULAR, ObjectRequest::BLACK},
      {42176, ObjectRequest::REGULAR, ObjectRequest::GREY},
  });
  CreateObjectsInPage({
      {512, ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA},
      {64, ObjectRequest::REGULAR, ObjectRequest::WHITE},
      {48, ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA},
      {52, ObjectRequest::REGULAR, ObjectRequest::BLACK},
      {4, ObjectRequest::FREE, ObjectRequest::GREY},
      {8, ObjectRequest::FREE, ObjectRequest::GREY},
      {8, ObjectRequest::REGULAR, ObjectRequest::WHITE},
      {60, ObjectRequest::REGULAR, ObjectRequest::BLACK},
  });
  CreateLargeObjects({
      {1 * MB, ObjectRequest::LARGE, ObjectRequest::WHITE},
      {32 * MB, ObjectRequest::LARGE, ObjectRequest::BLACK},
  });
  TestAll();
}

TEST_F(InnerPointerResolutionTest, FreePages) {
  if (v8_flags.enable_third_party_heap) return;
  int some_normal_page = CreateObjectsInPage({
      {64, ObjectRequest::REGULAR, ObjectRequest::WHITE},
      {52, ObjectRequest::REGULAR, ObjectRequest::BLACK},
      {512, ObjectRequest::REGULAR, ObjectRequest::WHITE},
      {60, ObjectRequest::REGULAR, ObjectRequest::BLACK},
      {42176, ObjectRequest::REGULAR, ObjectRequest::GREY},
  });
  CreateObjectsInPage({
      {512, ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA},
      {64, ObjectRequest::REGULAR, ObjectRequest::WHITE},
      {48, ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA},
      {52, ObjectRequest::REGULAR, ObjectRequest::BLACK},
      {4, ObjectRequest::FREE, ObjectRequest::GREY},
      {8, ObjectRequest::FREE, ObjectRequest::GREY},
      {8, ObjectRequest::REGULAR, ObjectRequest::WHITE},
      {60, ObjectRequest::REGULAR, ObjectRequest::BLACK},
  });
  auto large_pages = CreateLargeObjects({
      {1 * MB, ObjectRequest::LARGE, ObjectRequest::WHITE},
      {32 * MB, ObjectRequest::LARGE, ObjectRequest::BLACK},
  });
  TestAll();
  FreePage(some_normal_page);
  TestAll();
  FreePage(large_pages[0]);
  TestAll();
}

using InnerPointerResolutionHeapTest = TestWithHeapInternalsAndContext;

TEST_F(InnerPointerResolutionHeapTest, UnusedRegularYoungPages) {
  ManualGCScope manual_gc_scope(isolate());
  v8_flags.page_promotion = false;

  Persistent<v8::FixedArray> weak1, weak2;
  Address inner_ptr1, inner_ptr2, outside_ptr1, outside_ptr2;
  Page *page1, *page2;

  {
    PtrComprCageBase cage_base{isolate()};
    HandleScope scope(isolate());

    // Allocate two objects, large enough that they fall in two different young
    // generation pages.
    const int length =
        (heap()->MaxRegularHeapObjectSize(AllocationType::kYoung) -
         FixedArray::SizeFor(0)) /
        kTaggedSize;
    auto h1 = factory()->NewFixedArray(length, AllocationType::kYoung);
    auto h2 = factory()->NewFixedArray(length, AllocationType::kYoung);
    weak1.Reset(v8_isolate(), Utils::FixedArrayToLocal(h1));
    weak2.Reset(v8_isolate(), Utils::FixedArrayToLocal(h2));
    weak1.SetWeak();
    weak2.SetWeak();
    auto obj1 = h1->GetHeapObject();
    auto obj2 = h2->GetHeapObject();
    page1 = Page::FromHeapObject(obj1);
    EXPECT_TRUE(!page1->IsLargePage());
    EXPECT_TRUE(v8_flags.minor_mc || page1->IsToPage());
    page2 = Page::FromHeapObject(obj2);
    EXPECT_TRUE(!page2->IsLargePage());
    EXPECT_TRUE(v8_flags.minor_mc || page2->IsToPage());
    EXPECT_NE(page1, page2);

    // Keep inner pointers to both.
    inner_ptr1 = obj1.address() + 17 * kTaggedSize;
    inner_ptr2 = obj2.address() + 37 * kTaggedSize;

    // Keep pointers to the end of the pages, after the objects.
    outside_ptr1 = page1->area_end() - 3 * kTaggedSize;
    outside_ptr2 = page2->area_end() - 2 * kTaggedSize;
    EXPECT_LE(obj1.address() + obj1.Size(cage_base), outside_ptr1);
    EXPECT_LE(obj2.address() + obj2.Size(cage_base), outside_ptr2);

    // Ensure the young generation space is iterable.
    heap()->new_space()->MakeLinearAllocationAreaIterable();

    // Inner pointer resolution should work now, finding the objects in the
    // case of the inner pointers.
    EXPECT_EQ(
        obj1.address(),
        heap()->mark_compact_collector()->FindBasePtrForMarking(inner_ptr1));
    EXPECT_EQ(
        obj2.address(),
        heap()->mark_compact_collector()->FindBasePtrForMarking(inner_ptr2));
    EXPECT_EQ(
        kNullAddress,
        heap()->mark_compact_collector()->FindBasePtrForMarking(outside_ptr1));
    EXPECT_EQ(
        kNullAddress,
        heap()->mark_compact_collector()->FindBasePtrForMarking(outside_ptr2));
  }

  // Garbage collection should reclaim both objects.
  CollectGarbage(NEW_SPACE);
  EXPECT_TRUE(weak1.IsEmpty());
  EXPECT_TRUE(weak2.IsEmpty());
  EXPECT_EQ(AllocationSpace::NEW_SPACE, page1->owner_identity());
  EXPECT_EQ(AllocationSpace::NEW_SPACE, page1->owner_identity());
  EXPECT_TRUE(v8_flags.minor_mc || page1->IsFromPage());
  EXPECT_TRUE(v8_flags.minor_mc || page2->IsFromPage());

  // Inner pointer resolution should work with pointers to unused young
  // generation pages (in case of the scavenger, the two pages are now in the
  // "from" semispace). There are no objects to be found.
  EXPECT_EQ(
      kNullAddress,
      heap()->mark_compact_collector()->FindBasePtrForMarking(inner_ptr1));
  EXPECT_EQ(
      kNullAddress,
      heap()->mark_compact_collector()->FindBasePtrForMarking(inner_ptr2));
  EXPECT_EQ(
      kNullAddress,
      heap()->mark_compact_collector()->FindBasePtrForMarking(outside_ptr1));
  EXPECT_EQ(
      kNullAddress,
      heap()->mark_compact_collector()->FindBasePtrForMarking(outside_ptr2));

  // Garbage collection once more.
  CollectGarbage(NEW_SPACE);
  EXPECT_EQ(AllocationSpace::NEW_SPACE, page1->owner_identity());
  EXPECT_EQ(AllocationSpace::NEW_SPACE, page1->owner_identity());
  EXPECT_TRUE(v8_flags.minor_mc || page1->IsToPage());
  EXPECT_TRUE(v8_flags.minor_mc || page2->IsToPage());

  // Inner pointer resolution should work with pointers to unused young
  // generation pages (in case of the scavenger, the two pages are now in the
  // "to" semispace). There are no objects to be found.
  EXPECT_EQ(
      kNullAddress,
      heap()->mark_compact_collector()->FindBasePtrForMarking(inner_ptr1));
  EXPECT_EQ(
      kNullAddress,
      heap()->mark_compact_collector()->FindBasePtrForMarking(inner_ptr2));
  EXPECT_EQ(
      kNullAddress,
      heap()->mark_compact_collector()->FindBasePtrForMarking(outside_ptr1));
  EXPECT_EQ(
      kNullAddress,
      heap()->mark_compact_collector()->FindBasePtrForMarking(outside_ptr2));
}

TEST_F(InnerPointerResolutionHeapTest, UnusedLargeYoungPage) {
  ManualGCScope manual_gc_scope(isolate());
  v8_flags.page_promotion = false;

  Global<v8::FixedArray> weak;
  Address inner_ptr;
  Page* page;

  {
    PtrComprCageBase cage_base{isolate()};
    HandleScope scope(isolate());

    // Allocate a large object in the young generation.
    const int length =
        std::max(1 << kPageSizeBits,
                 2 * heap()->MaxRegularHeapObjectSize(AllocationType::kYoung)) /
        kTaggedSize;
    auto h = factory()->NewFixedArray(length, AllocationType::kYoung);
    weak.Reset(v8_isolate(), Utils::FixedArrayToLocal(h));
    weak.SetWeak();
    auto obj = h->GetHeapObject();
    page = Page::FromHeapObject(obj);
    EXPECT_TRUE(page->IsLargePage());
    EXPECT_EQ(AllocationSpace::NEW_LO_SPACE, page->owner_identity());
    EXPECT_TRUE(v8_flags.minor_mc || page->IsToPage());

    // Keep inner pointer.
    inner_ptr = obj.address() + 17 * kTaggedSize;

    // Inner pointer resolution should work now, finding the object.
    EXPECT_EQ(
        obj.address(),
        heap()->mark_compact_collector()->FindBasePtrForMarking(inner_ptr));
  }

  // Garbage collection should reclaim the object.
  CollectGarbage(NEW_SPACE);
  EXPECT_TRUE(weak.IsEmpty());

  // Inner pointer resolution should work with a pointer to an unused young
  // generation large page. There is no object to be found.
  EXPECT_EQ(kNullAddress,
            heap()->mark_compact_collector()->FindBasePtrForMarking(inner_ptr));
}

TEST_F(InnerPointerResolutionHeapTest, RegularPageAfterEnd) {
  // Allocate a regular page.
  OldSpace* old_space = heap()->old_space();
  DCHECK_NE(nullptr, old_space);
  auto* page = heap()->memory_allocator()->AllocatePage(
      MemoryAllocator::AllocationMode::kRegular, old_space, NOT_EXECUTABLE);
  EXPECT_NE(nullptr, page);

  // The end of the page area is expected not to coincide with the beginning of
  // the next page.
  const int size = (1 << kPageSizeBits) / 2;
  const Address mark = page->area_start() + size;
  heap()->CreateFillerObjectAt(page->area_start(), size);
  heap()->CreateFillerObjectAt(mark, static_cast<int>(page->area_end() - mark));
  Page::UpdateHighWaterMark(mark);
  page->ShrinkToHighWaterMark();
  EXPECT_FALSE(Page::IsAlignedToPageSize(page->area_end()));

  // Inner pointer resolution after the end of the page area should work.
  Address inner_ptr = page->area_end() + kTaggedSize;
  EXPECT_FALSE(Page::IsAlignedToPageSize(inner_ptr));
  EXPECT_EQ(kNullAddress,
            heap()->mark_compact_collector()->FindBasePtrForMarking(inner_ptr));

  // Deallocate the page.
  heap()->memory_allocator()->Free(MemoryAllocator::FreeMode::kImmediately,
                                   page);
}

TEST_F(InnerPointerResolutionHeapTest, LargePageAfterEnd) {
  // Allocate a large page.
  OldLargeObjectSpace* lo_space = heap()->lo_space();
  EXPECT_NE(nullptr, lo_space);
  const int size = 3 * (1 << kPageSizeBits) / 2;
  LargePage* page = heap()->memory_allocator()->AllocateLargePage(
      lo_space, size, NOT_EXECUTABLE);
  EXPECT_NE(nullptr, page);

  // The end of the page area is expected not to coincide with the beginning of
  // the next page.
  EXPECT_FALSE(Page::IsAlignedToPageSize(page->area_end()));

  // Inner pointer resolution after the end of the pare area should work.
  Address inner_ptr = page->area_end() + kTaggedSize;
  EXPECT_FALSE(Page::IsAlignedToPageSize(inner_ptr));
  EXPECT_EQ(kNullAddress,
            heap()->mark_compact_collector()->FindBasePtrForMarking(inner_ptr));

  // Deallocate the page.
  heap()->memory_allocator()->Free(MemoryAllocator::FreeMode::kImmediately,
                                   page);
}

#endif  // V8_ENABLE_INNER_POINTER_RESOLUTION_MB

}  // namespace internal
}  // namespace v8
