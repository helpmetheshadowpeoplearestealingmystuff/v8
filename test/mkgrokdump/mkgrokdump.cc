// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "include/libplatform/libplatform.h"
#include "include/v8.h"

#include "src/execution/frames.h"
#include "src/execution/isolate.h"
#include "src/heap/heap-inl.h"
#include "src/heap/read-only-heap.h"
#include "src/heap/spaces.h"
#include "src/objects-inl.h"

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

// Non-snapshot builds allocate objects to different places.
// Debug builds emit debug code, affecting code object sizes.
// Embedded builtins cause objects to be allocated in different locations.
#if defined(V8_EMBEDDED_BUILTINS) && defined(V8_USE_SNAPSHOT) && !defined(DEBUG)
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

static void DumpKnownMap(i::Heap* heap, const char* space_name,
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
  i::PrintF("  (\"%s\", 0x%05" V8PRIxPTR "): (%d, \"%s\"),\n", space_name,
            root_ptr, map->instance_type(), root_name);

#undef MUTABLE_ROOT_LIST_CASE
#undef RO_ROOT_LIST_CASE
}

static void DumpKnownObject(i::Heap* heap, const char* space_name,
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

  i::PrintF("  (\"%s\", 0x%05" V8PRIxPTR "): \"%s\",\n", space_name, root_ptr,
            root_name);

#undef ROOT_LIST_CASE
#undef RO_ROOT_LIST_CASE
}

static int DumpHeapConstants(const char* argv0) {
  // Start up V8.
  std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
  v8::V8::InitializePlatform(platform.get());
  v8::V8::Initialize();
  v8::V8::InitializeExternalStartupData(argv0);
  Isolate::CreateParams create_params;
  MockArrayBufferAllocator mock_arraybuffer_allocator;
  create_params.array_buffer_allocator = &mock_arraybuffer_allocator;
  Isolate* isolate = Isolate::New(create_params);
  {
    Isolate::Scope scope(isolate);
    i::Heap* heap = reinterpret_cast<i::Isolate*>(isolate)->heap();
    i::PrintF("%s", kHeader);
#define DUMP_TYPE(T) i::PrintF("  %d: \"%s\",\n", i::T, #T);
    i::PrintF("INSTANCE_TYPES = {\n");
    INSTANCE_TYPE_LIST(DUMP_TYPE)
    i::PrintF("}\n");
#undef DUMP_TYPE

    {
      // Dump the KNOWN_MAP table to the console.
      i::PrintF("\n# List of known V8 maps.\n");
      i::PrintF("KNOWN_MAPS = {\n");
      i::ReadOnlyHeapIterator ro_iterator(heap->read_only_heap());
      for (i::HeapObject object = ro_iterator.Next(); !object.is_null();
           object = ro_iterator.Next()) {
        if (!object->IsMap()) continue;
        DumpKnownMap(heap, i::Heap::GetSpaceName(i::RO_SPACE), object);
      }
      i::HeapObjectIterator iterator(heap->map_space());
      for (i::HeapObject object = iterator.Next(); !object.is_null();
           object = iterator.Next()) {
        if (!object->IsMap()) continue;
        DumpKnownMap(heap, i::Heap::GetSpaceName(i::MAP_SPACE), object);
      }
      i::PrintF("}\n");
    }

    {
      // Dump the KNOWN_OBJECTS table to the console.
      i::PrintF("\n# List of known V8 objects.\n");
      i::PrintF("KNOWN_OBJECTS = {\n");
      i::ReadOnlyHeapIterator ro_iterator(heap->read_only_heap());
      for (i::HeapObject object = ro_iterator.Next(); !object.is_null();
           object = ro_iterator.Next()) {
        // Skip read-only heap maps, they will be reported elsewhere.
        if (object->IsMap()) continue;
        DumpKnownObject(heap, i::Heap::GetSpaceName(i::RO_SPACE), object);
      }

      i::PagedSpaces spit(heap);
      for (i::PagedSpace* s = spit.next(); s != nullptr; s = spit.next()) {
        i::HeapObjectIterator it(s);
        // Code objects are generally platform-dependent.
        if (s->identity() == i::CODE_SPACE || s->identity() == i::MAP_SPACE)
          continue;
        const char* sname = s->name();
        for (i::HeapObject o = it.Next(); !o.is_null(); o = it.Next()) {
          DumpKnownObject(heap, sname, o);
        }
      }
      i::PrintF("}\n");
    }

    // Dump frame markers
    i::PrintF("\n# List of known V8 Frame Markers.\n");
#define DUMP_MARKER(T, class) i::PrintF("  \"%s\",\n", #T);
    i::PrintF("FRAME_MARKERS = (\n");
    STACK_FRAME_TYPE_LIST(DUMP_MARKER)
    i::PrintF(")\n");
#undef DUMP_MARKER
  }

  i::PrintF("\n# This set of constants is generated from a %s build.\n",
            kBuild);

  // Teardown.
  isolate->Dispose();
  v8::V8::ShutdownPlatform();
  return 0;
}

}  // namespace v8

int main(int argc, char* argv[]) { return v8::DumpHeapConstants(argv[0]); }
