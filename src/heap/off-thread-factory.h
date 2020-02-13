// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_OFF_THREAD_FACTORY_H_
#define V8_HEAP_OFF_THREAD_FACTORY_H_

#include <map>
#include <vector>
#include "src/base/logging.h"
#include "src/common/globals.h"
#include "src/handles/handles.h"
#include "src/heap/factory-base.h"
#include "src/heap/heap.h"
#include "src/heap/read-only-heap.h"
#include "src/heap/spaces.h"
#include "src/objects/heap-object.h"
#include "src/objects/map.h"
#include "src/objects/objects.h"
#include "src/objects/shared-function-info.h"
#include "src/roots/roots.h"

namespace v8 {
namespace internal {

class AstValueFactory;
class AstRawString;
class AstConsString;
class OffThreadIsolate;

class OffThreadFactory;

template <>
struct HandleTraits<OffThreadFactory> {
  template <typename T>
  using HandleType = OffThreadHandle<T>;
  template <typename T>
  using MaybeHandleType = OffThreadHandle<T>;
  using HandleScopeType = OffThreadHandleScope;
};

struct RelativeSlot {
  RelativeSlot() = default;
  RelativeSlot(Address object_address, int slot_offset)
      : object_address(object_address), slot_offset(slot_offset) {}

  Address object_address;
  int slot_offset;
};

class V8_EXPORT_PRIVATE OffThreadFactory
    : public FactoryBase<OffThreadFactory> {
 public:
  explicit OffThreadFactory(Isolate* isolate);

  ReadOnlyRoots read_only_roots() const { return roots_; }

#define ROOT_ACCESSOR(Type, name, CamelName) \
  inline OffThreadHandle<Type> name();
  READ_ONLY_ROOT_LIST(ROOT_ACCESSOR)
#undef ROOT_ACCESSOR

  void FinishOffThread();
  void Publish(Isolate* isolate);

  // The parser shouldn't allow the OffThreadFactory to get into a state where
  // it generates errors.
  OffThreadHandle<Object> NewInvalidStringLengthError() { UNREACHABLE(); }
  OffThreadHandle<Object> NewRangeError(MessageTemplate template_index) {
    UNREACHABLE();
  }

  OffThreadHandle<FixedArray> StringWrapperForTest(
      OffThreadHandle<String> string);

 private:
  friend class FactoryBase<OffThreadFactory>;

  // ------
  // Customization points for FactoryBase.
  HeapObject AllocateRaw(int size, AllocationType allocation,
                         AllocationAlignment alignment = kWordAligned);

  OffThreadIsolate* isolate() {
    // Downcast to the privately inherited sub-class using c-style casts to
    // avoid undefined behavior (as static_cast cannot cast across private
    // bases).
    // NOLINTNEXTLINE (google-readability-casting)
    return (OffThreadIsolate*)this;  // NOLINT(readability/casting)
  }
  inline bool CanAllocateInReadOnlySpace() { return false; }
  inline bool EmptyStringRootIsInitialized() { return true; }
  // ------

  OffThreadHandle<String> MakeOrFindTwoCharacterString(uint16_t c1,
                                                       uint16_t c2);

  void AddToScriptList(OffThreadHandle<Script> shared);
  // ------

  ReadOnlyRoots roots_;
  OffThreadSpace space_;
  OffThreadLargeObjectSpace lo_space_;
  std::vector<RelativeSlot> string_slots_;
  std::vector<Script> script_list_;
  bool is_finished = false;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_OFF_THREAD_FACTORY_H_
