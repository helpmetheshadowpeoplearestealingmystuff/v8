// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/tagged-impl.h"

#include <sstream>

#include "src/objects/objects.h"
#include "src/objects/smi.h"
#include "src/objects/tagged-impl-inl.h"
#include "src/strings/string-stream.h"
#include "src/utils/ostreams.h"

#ifdef V8_EXTERNAL_CODE_SPACE
// For IsCodeSpaceObject().
#include "src/heap/heap-write-barrier-inl.h"
#endif

namespace v8 {
namespace internal {

#ifdef V8_EXTERNAL_CODE_SPACE
bool CheckObjectComparisonAllowed(Address a, Address b) {
  if (!HAS_STRONG_HEAP_OBJECT_TAG(a) || !HAS_STRONG_HEAP_OBJECT_TAG(b)) {
    return true;
  }
  HeapObject obj_a = HeapObject::unchecked_cast(Object(a));
  HeapObject obj_b = HeapObject::unchecked_cast(Object(b));
  // This check might fail when we try to compare InstructionStream object with
  // non-InstructionStream object. The main legitimate case when such "mixed"
  // comparison could happen is comparing two AbstractCode objects. If that's
  // the case one must use AbstractCode's == operator instead of Object's one or
  // SafeEquals().
  CHECK_EQ(IsCodeSpaceObject(obj_a), IsCodeSpaceObject(obj_b));
  return true;
}
#endif  // V8_EXTERNAL_CODE_SPACE

template <HeapObjectReferenceType kRefType, typename StorageType>
void ShortPrint(TaggedImpl<kRefType, StorageType> ptr, FILE* out) {
  OFStream os(out);
  os << Brief(ptr);
}
template void ShortPrint(
    TaggedImpl<HeapObjectReferenceType::STRONG, Address> ptr, FILE* out);
template void ShortPrint(TaggedImpl<HeapObjectReferenceType::WEAK, Address> ptr,
                         FILE* out);

template <HeapObjectReferenceType kRefType, typename StorageType>
void ShortPrint(TaggedImpl<kRefType, StorageType> ptr,
                StringStream* accumulator) {
  std::ostringstream os;
  os << Brief(ptr);
  accumulator->Add(os.str().c_str());
}
template void ShortPrint(
    TaggedImpl<HeapObjectReferenceType::STRONG, Address> ptr,
    StringStream* accumulator);
template void ShortPrint(TaggedImpl<HeapObjectReferenceType::WEAK, Address> ptr,
                         StringStream* accumulator);

template <HeapObjectReferenceType kRefType, typename StorageType>
void ShortPrint(TaggedImpl<kRefType, StorageType> ptr, std::ostream& os) {
  os << Brief(ptr);
}
template void ShortPrint(
    TaggedImpl<HeapObjectReferenceType::STRONG, Address> ptr, std::ostream& os);
template void ShortPrint(TaggedImpl<HeapObjectReferenceType::WEAK, Address> ptr,
                         std::ostream& os);

#ifdef OBJECT_PRINT
template <HeapObjectReferenceType kRefType, typename StorageType>
void Print(TaggedImpl<kRefType, StorageType> ptr) {
  StdoutStream os;
  Print(ptr, os);
  os << std::flush;
}
template void Print(TaggedImpl<HeapObjectReferenceType::STRONG, Address> ptr);
template void Print(TaggedImpl<HeapObjectReferenceType::WEAK, Address> ptr);

template <HeapObjectReferenceType kRefType, typename StorageType>
void Print(TaggedImpl<kRefType, StorageType> ptr, std::ostream& os) {
  Smi smi(0);
  HeapObject heap_object;
  if (ptr.ToSmi(&smi)) {
    os << "Smi: " << std::hex << "0x" << smi.value();
    os << std::dec << " (" << smi.value() << ")\n";
  } else if (ptr.IsCleared()) {
    os << "[cleared]";
  } else if (ptr.GetHeapObjectIfWeak(&heap_object)) {
    os << "[weak] ";
    heap_object->HeapObjectPrint(os);
  } else if (ptr.GetHeapObjectIfStrong(&heap_object)) {
    heap_object->HeapObjectPrint(os);
  } else {
    UNREACHABLE();
  }
}
template void Print(TaggedImpl<HeapObjectReferenceType::STRONG, Address> ptr,
                    std::ostream& os);
template void Print(TaggedImpl<HeapObjectReferenceType::WEAK, Address> ptr,
                    std::ostream& os);
#endif  // OBJECT_PRINT

// Explicit instantiation declarations.
template class TaggedImpl<HeapObjectReferenceType::STRONG, Address>;
template class TaggedImpl<HeapObjectReferenceType::WEAK, Address>;

}  // namespace internal
}  // namespace v8
