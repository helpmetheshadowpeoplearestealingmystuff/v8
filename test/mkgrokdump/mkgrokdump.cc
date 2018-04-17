// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "include/libplatform/libplatform.h"
#include "include/v8.h"

#include "src/frames.h"
#include "src/heap/heap.h"
#include "src/heap/spaces.h"
#include "src/isolate.h"
#include "src/objects-inl.h"

namespace v8 {

static const char* kHeader =
    "# Copyright 2017 the V8 project authors. All rights reserved.\n"
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

#define ROOT_LIST_CASE(type, name, camel_name) \
  if (n == NULL && o == space->heap()->name()) n = #camel_name;
#define STRUCT_LIST_CASE(upper_name, camel_name, name) \
  if (n == NULL && o == space->heap()->name##_map()) n = #camel_name "Map";
static void DumpMaps(i::PagedSpace* space) {
  i::HeapObjectIterator it(space);
  for (i::Object* o = it.Next(); o != NULL; o = it.Next()) {
    if (!o->IsMap()) continue;
    i::Map* m = i::Map::cast(o);
    const char* n = NULL;
    intptr_t p = reinterpret_cast<intptr_t>(m) & 0x7FFFF;
    int t = m->instance_type();
    ROOT_LIST(ROOT_LIST_CASE)
    STRUCT_LIST(STRUCT_LIST_CASE)
    if (n == NULL) continue;
    const char* sname = AllocationSpaceName(space->identity());
    i::PrintF("  (\"%s\", 0x%05" V8PRIxPTR "): (%d, \"%s\"),\n", sname, p, t,
              n);
  }
}
#undef STRUCT_LIST_CASE
#undef ROOT_LIST_CASE

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

    // Dump the KNOWN_MAP table to the console.
    i::PrintF("\n# List of known V8 maps.\n");
    i::PrintF("KNOWN_MAPS = {\n");
    DumpMaps(heap->read_only_space());
    DumpMaps(heap->map_space());
    i::PrintF("}\n");

    // Dump the KNOWN_OBJECTS table to the console.
    i::PrintF("\n# List of known V8 objects.\n");
#define ROOT_LIST_CASE(type, name, camel_name) \
  if (n == NULL && o == heap->name()) {        \
    n = #camel_name;                           \
    i = i::Heap::k##camel_name##RootIndex;     \
  }
    i::PagedSpaces spit(heap, i::PagedSpaces::SpacesSpecifier::kAllPagedSpaces);
    i::PrintF("KNOWN_OBJECTS = {\n");
    for (i::PagedSpace* s = spit.next(); s != NULL; s = spit.next()) {
      i::HeapObjectIterator it(s);
      // Code objects are generally platform-dependent.
      if (s->identity() == i::CODE_SPACE || s->identity() == i::MAP_SPACE)
        continue;
      const char* sname = AllocationSpaceName(s->identity());
      for (i::Object* o = it.Next(); o != NULL; o = it.Next()) {
        // Skip maps in RO_SPACE since they will be reported elsewhere.
        if (o->IsMap()) continue;
        const char* n = NULL;
        i::Heap::RootListIndex i = i::Heap::kStrongRootListLength;
        intptr_t p = reinterpret_cast<intptr_t>(o) & 0x7FFFF;
        ROOT_LIST(ROOT_LIST_CASE)
        if (n == NULL) continue;
        if (!i::Heap::RootIsImmortalImmovable(i)) continue;
        i::PrintF("  (\"%s\", 0x%05" V8PRIxPTR "): \"%s\",\n", sname, p, n);
      }
    }
    i::PrintF("}\n");
#undef ROOT_LIST_CASE

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
