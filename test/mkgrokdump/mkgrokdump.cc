// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "include/libplatform/libplatform.h"
#include "include/v8-array-buffer.h"
#include "include/v8-initialization.h"
#include "src/execution/frames.h"
#include "src/execution/isolate.h"
#include "src/heap/heap-inl.h"
#include "src/heap/paged-spaces-inl.h"
#include "src/heap/read-only-heap.h"
#include "src/heap/safepoint.h"
#include "src/heap/spaces.h"
#include "src/objects/objects-inl.h"

namespace v8 {

static const char* kHeader =
    "# Copyright 2019 the V8 project authors. All rights reserved.\n"
    "# Use of this source code is governed by a BSD-style license that can\n"
    "# be found in the LICENSE file.\n"
    "\n"
    "# This file is automatically generated by mkgrokdump and should not\n"
    "# be modified manually.\n"
    "\n"
    "# List of known V8 instance types.\n";

// Debug builds emit debug code, affecting code object sizes.
#ifndef DEBUG
static const char* kBuild = "shipping";
#else
static const char* kBuild = "non-shipping";
#endif

class MockArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
 public:
  void* Allocate(size_t length) override { return nullptr; }
  void* AllocateUninitialized(size_t length) override { return nullptr; }
  void Free(void* p, size_t) override {}
};

static void DumpKnownMap(FILE* out, i::Heap* heap, const char* space_name,
                         i::HeapObject object) {
#define RO_ROOT_LIST_CASE(type, name, CamelName) \
  if (root_name == nullptr && object == roots.name()) root_name = #CamelName;
#define MUTABLE_ROOT_LIST_CASE(type, name, CamelName) \
  if (root_name == nullptr && object == heap->name()) root_name = #CamelName;

  i::ReadOnlyRoots roots(heap);
  const char* root_name = nullptr;
  i::Map map = i::Map::cast(object);
  intptr_t root_ptr =
      static_cast<intptr_t>(map.ptr()) & (i::Page::kPageSize - 1);

  READ_ONLY_ROOT_LIST(RO_ROOT_LIST_CASE)
  MUTABLE_ROOT_LIST(MUTABLE_ROOT_LIST_CASE)

  if (root_name == nullptr) return;
  i::PrintF(out, "  (\"%s\", 0x%05" V8PRIxPTR "): (%d, \"%s\"),\n", space_name,
            root_ptr, map.instance_type(), root_name);

#undef MUTABLE_ROOT_LIST_CASE
#undef RO_ROOT_LIST_CASE
}

static void DumpKnownObject(FILE* out, i::Heap* heap, const char* space_name,
                            i::HeapObject object) {
#define RO_ROOT_LIST_CASE(type, name, CamelName)        \
  if (root_name == nullptr && object == roots.name()) { \
    root_name = #CamelName;                             \
    root_index = i::RootIndex::k##CamelName;            \
  }
#define ROOT_LIST_CASE(type, name, CamelName)           \
  if (root_name == nullptr && object == heap->name()) { \
    root_name = #CamelName;                             \
    root_index = i::RootIndex::k##CamelName;            \
  }

  i::ReadOnlyRoots roots(heap);
  const char* root_name = nullptr;
  i::RootIndex root_index = i::RootIndex::kFirstSmiRoot;
  intptr_t root_ptr = object.ptr() & (i::Page::kPageSize - 1);

  STRONG_READ_ONLY_ROOT_LIST(RO_ROOT_LIST_CASE)
  MUTABLE_ROOT_LIST(ROOT_LIST_CASE)

  if (root_name == nullptr) return;
  if (!i::RootsTable::IsImmortalImmovable(root_index)) return;

  i::PrintF(out, "  (\"%s\", 0x%05" V8PRIxPTR "): \"%s\",\n", space_name,
            root_ptr, root_name);

#undef ROOT_LIST_CASE
#undef RO_ROOT_LIST_CASE
}

static void DumpSpaceFirstPageAddress(FILE* out, i::BaseSpace* space,
                                      i::Address first_page) {
  const char* name = space->name();
  i::Tagged_t compressed = i::CompressTagged(first_page);
  uintptr_t unsigned_compressed = static_cast<uint32_t>(compressed);
  i::PrintF(out, "  0x%08" V8PRIxPTR ": \"%s\",\n", unsigned_compressed, name);
}

template <typename SpaceT>
static void DumpSpaceFirstPageAddress(FILE* out, SpaceT* space) {
  i::Address first_page = space->FirstPageAddress();
  DumpSpaceFirstPageAddress(out, space, first_page);
}

static int DumpHeapConstants(FILE* out, const char* argv0) {
  // Start up V8.
  std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
  v8::V8::InitializePlatform(platform.get());
#ifdef V8_VIRTUAL_MEMORY_CAGE
  if (!v8::V8::InitializeVirtualMemoryCage()) {
    FATAL("Could not initialize the virtual memory cage");
  }
#endif
  v8::V8::Initialize();
  v8::V8::InitializeExternalStartupData(argv0);
  Isolate::CreateParams create_params;
  MockArrayBufferAllocator mock_arraybuffer_allocator;
  create_params.array_buffer_allocator = &mock_arraybuffer_allocator;
  Isolate* isolate = Isolate::New(create_params);
  {
    Isolate::Scope scope(isolate);
    i::Heap* heap = reinterpret_cast<i::Isolate*>(isolate)->heap();
    i::SafepointScope safepoint_scope(heap);
    i::ReadOnlyHeap* read_only_heap =
        reinterpret_cast<i::Isolate*>(isolate)->read_only_heap();
    i::PrintF(out, "%s", kHeader);
#define DUMP_TYPE(T) i::PrintF(out, "  %d: \"%s\",\n", i::T, #T);
    i::PrintF(out, "INSTANCE_TYPES = {\n");
    INSTANCE_TYPE_LIST(DUMP_TYPE)
    i::PrintF(out, "}\n");
#undef DUMP_TYPE

    {
      // Dump the KNOWN_MAP table to the console.
      i::PrintF(out, "\n# List of known V8 maps.\n");
      i::PrintF(out, "KNOWN_MAPS = {\n");
      i::ReadOnlyHeapObjectIterator ro_iterator(read_only_heap);
      for (i::HeapObject object = ro_iterator.Next(); !object.is_null();
           object = ro_iterator.Next()) {
        if (!object.IsMap()) continue;
        DumpKnownMap(out, heap, i::BaseSpace::GetSpaceName(i::RO_SPACE),
                     object);
      }
      i::PagedSpaceObjectIterator iterator(heap, heap->map_space());
      for (i::HeapObject object = iterator.Next(); !object.is_null();
           object = iterator.Next()) {
        if (!object.IsMap()) continue;
        DumpKnownMap(out, heap, i::BaseSpace::GetSpaceName(i::MAP_SPACE),
                     object);
      }
      i::PrintF(out, "}\n");
    }

    {
      // Dump the KNOWN_OBJECTS table to the console.
      i::PrintF(out, "\n# List of known V8 objects.\n");
      i::PrintF(out, "KNOWN_OBJECTS = {\n");
      i::ReadOnlyHeapObjectIterator ro_iterator(read_only_heap);
      for (i::HeapObject object = ro_iterator.Next(); !object.is_null();
           object = ro_iterator.Next()) {
        // Skip read-only heap maps, they will be reported elsewhere.
        if (object.IsMap()) continue;
        DumpKnownObject(out, heap, i::BaseSpace::GetSpaceName(i::RO_SPACE),
                        object);
      }

      i::PagedSpaceIterator spit(heap);
      for (i::PagedSpace* s = spit.Next(); s != nullptr; s = spit.Next()) {
        i::PagedSpaceObjectIterator it(heap, s);
        // Code objects are generally platform-dependent.
        if (s->identity() == i::CODE_SPACE || s->identity() == i::MAP_SPACE)
          continue;
        const char* sname = s->name();
        for (i::HeapObject o = it.Next(); !o.is_null(); o = it.Next()) {
          DumpKnownObject(out, heap, sname, o);
        }
      }
      i::PrintF(out, "}\n");
    }

    if (COMPRESS_POINTERS_BOOL) {
      // Dump a list of addresses for the first page of each space that contains
      // objects in the other tables above. This is only useful if two
      // assumptions hold:
      // 1. Those pages are positioned deterministically within the heap
      //    reservation block during snapshot deserialization.
      // 2. Those pages cannot ever be moved (such as by compaction).
      i::PrintF(out,
                "\n# Lower 32 bits of first page addresses for various heap "
                "spaces.\n");
      i::PrintF(out, "HEAP_FIRST_PAGES = {\n");
      i::PagedSpaceIterator it(heap);
      for (i::PagedSpace* s = it.Next(); s != nullptr; s = it.Next()) {
        // Code page is different on Windows vs Linux (bug v8:9844), so skip it.
        if (s->identity() == i::CODE_SPACE) {
          continue;
        }
        DumpSpaceFirstPageAddress(out, s);
      }
      DumpSpaceFirstPageAddress(out, read_only_heap->read_only_space());
      i::PrintF(out, "}\n");
    }

    // Dump frame markers
    i::PrintF(out, "\n# List of known V8 Frame Markers.\n");
#define DUMP_MARKER(T, class) i::PrintF(out, "  \"%s\",\n", #T);
    i::PrintF(out, "FRAME_MARKERS = (\n");
    STACK_FRAME_TYPE_LIST(DUMP_MARKER)
    i::PrintF(out, ")\n");
#undef DUMP_MARKER
  }

  i::PrintF(out, "\n# This set of constants is generated from a %s build.\n",
            kBuild);

  // Teardown.
  isolate->Dispose();
  v8::V8::ShutdownPlatform();
  return 0;
}

}  // namespace v8

int main(int argc, char* argv[]) {
  FILE* out = stdout;
  if (argc > 2 && strcmp(argv[1], "--outfile") == 0) {
    out = fopen(argv[2], "wb");
  }
  return v8::DumpHeapConstants(out, argv[0]);
}
