// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#include "objects.h"
#include "elements.h"
#include "utils.h"


// Each concrete ElementsAccessor can handle exactly one ElementsKind,
// several abstract ElementsAccessor classes are used to allow sharing
// common code.
//
// Inheritance hierarchy:
// - ElementsAccessorBase                        (abstract)
//   - FastElementsAccessor                      (abstract)
//     - FastObjectElementsAccessor
//     - FastDoubleElementsAccessor
//   - ExternalElementsAccessor                  (abstract)
//     - ExternalByteElementsAccessor
//     - ExternalUnsignedByteElementsAccessor
//     - ExternalShortElementsAccessor
//     - ExternalUnsignedShortElementsAccessor
//     - ExternalIntElementsAccessor
//     - ExternalUnsignedIntElementsAccessor
//     - ExternalFloatElementsAccessor
//     - ExternalDoubleElementsAccessor
//     - PixelElementsAccessor
//   - DictionaryElementsAccessor
//   - NonStrictArgumentsElementsAccessor


namespace v8 {
namespace internal {


// First argument in list is the accessor class, the second argument is the
// accessor ElementsKind, and the third is the backing store class.  Use the
// fast element handler for smi-only arrays.  The implementation is currently
// identical.  Note that the order must match that of the ElementsKind enum for
// the |accessor_array[]| below to work.
#define ELEMENTS_LIST(V)                                                \
  V(FastObjectElementsAccessor, FAST_SMI_ONLY_ELEMENTS, FixedArray)     \
  V(FastObjectElementsAccessor, FAST_ELEMENTS, FixedArray)              \
  V(FastDoubleElementsAccessor, FAST_DOUBLE_ELEMENTS, FixedDoubleArray) \
  V(DictionaryElementsAccessor, DICTIONARY_ELEMENTS,                    \
    SeededNumberDictionary)                                             \
  V(NonStrictArgumentsElementsAccessor, NON_STRICT_ARGUMENTS_ELEMENTS,  \
    FixedArray)                                                         \
  V(ExternalByteElementsAccessor, EXTERNAL_BYTE_ELEMENTS,               \
    ExternalByteArray)                                                  \
  V(ExternalUnsignedByteElementsAccessor,                               \
    EXTERNAL_UNSIGNED_BYTE_ELEMENTS, ExternalUnsignedByteArray)         \
  V(ExternalShortElementsAccessor, EXTERNAL_SHORT_ELEMENTS,             \
    ExternalShortArray)                                                 \
  V(ExternalUnsignedShortElementsAccessor,                              \
    EXTERNAL_UNSIGNED_SHORT_ELEMENTS, ExternalUnsignedShortArray)       \
  V(ExternalIntElementsAccessor, EXTERNAL_INT_ELEMENTS,                 \
    ExternalIntArray)                                                   \
  V(ExternalUnsignedIntElementsAccessor,                                \
    EXTERNAL_UNSIGNED_INT_ELEMENTS, ExternalUnsignedIntArray)           \
  V(ExternalFloatElementsAccessor,                                      \
    EXTERNAL_FLOAT_ELEMENTS, ExternalFloatArray)                        \
  V(ExternalDoubleElementsAccessor,                                     \
    EXTERNAL_DOUBLE_ELEMENTS, ExternalDoubleArray)                      \
  V(PixelElementsAccessor, EXTERNAL_PIXEL_ELEMENTS, ExternalPixelArray)


template<ElementsKind Kind> class ElementsKindTraits {
 public:
  typedef FixedArrayBase BackingStore;
};

#define ELEMENTS_TRAITS(Class, KindParam, Store)               \
template<> class ElementsKindTraits<KindParam> {               \
  public:                                                      \
  static const ElementsKind Kind = KindParam;                  \
  typedef Store BackingStore;                                  \
};
ELEMENTS_LIST(ELEMENTS_TRAITS)
#undef ELEMENTS_TRAITS


ElementsAccessor** ElementsAccessor::elements_accessors_;


static bool HasKey(FixedArray* array, Object* key) {
  int len0 = array->length();
  for (int i = 0; i < len0; i++) {
    Object* element = array->get(i);
    if (element->IsSmi() && element == key) return true;
    if (element->IsString() &&
        key->IsString() && String::cast(element)->Equals(String::cast(key))) {
      return true;
    }
  }
  return false;
}


static Failure* ThrowArrayLengthRangeError(Heap* heap) {
  HandleScope scope(heap->isolate());
  return heap->isolate()->Throw(
      *heap->isolate()->factory()->NewRangeError("invalid_array_length",
          HandleVector<Object>(NULL, 0)));
}


void CopyObjectToObjectElements(FixedArray* from,
                                ElementsKind from_kind,
                                uint32_t from_start,
                                FixedArray* to,
                                ElementsKind to_kind,
                                uint32_t to_start,
                                int raw_copy_size) {
  ASSERT(to->map() != HEAP->fixed_cow_array_map());
  ASSERT(from_kind == FAST_ELEMENTS || from_kind == FAST_SMI_ONLY_ELEMENTS);
  ASSERT(to_kind == FAST_ELEMENTS || to_kind == FAST_SMI_ONLY_ELEMENTS);
  int copy_size = raw_copy_size;
  if (raw_copy_size < 0) {
    ASSERT(raw_copy_size == ElementsAccessor::kCopyToEnd ||
           raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole);
    copy_size = Min(from->length() - from_start,
                    to->length() - to_start);
#ifdef DEBUG
    // FAST_ELEMENT arrays cannot be uninitialized. Ensure they are already
    // marked with the hole.
    if (raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole) {
      for (int i = to_start + copy_size; i < to->length(); ++i) {
        ASSERT(to->get(i)->IsTheHole());
      }
    }
#endif
  }
  ASSERT((copy_size + static_cast<int>(to_start)) <= to->length() &&
         (copy_size + static_cast<int>(from_start)) <= from->length());
  if (copy_size == 0) return;
  Address to_address = to->address() + FixedArray::kHeaderSize;
  Address from_address = from->address() + FixedArray::kHeaderSize;
  CopyWords(reinterpret_cast<Object**>(to_address) + to_start,
            reinterpret_cast<Object**>(from_address) + from_start,
            copy_size);
  if (from_kind == FAST_ELEMENTS && to_kind == FAST_ELEMENTS) {
    Heap* heap = from->GetHeap();
    if (!heap->InNewSpace(to)) {
      heap->RecordWrites(to->address(),
                         to->OffsetOfElementAt(to_start),
                         copy_size);
    }
    heap->incremental_marking()->RecordWrites(to);
  }
}


static void CopyDictionaryToObjectElements(SeededNumberDictionary* from,
                                           uint32_t from_start,
                                           FixedArray* to,
                                           ElementsKind to_kind,
                                           uint32_t to_start,
                                           int raw_copy_size) {
  int copy_size = raw_copy_size;
  Heap* heap = from->GetHeap();
  if (raw_copy_size < 0) {
    ASSERT(raw_copy_size == ElementsAccessor::kCopyToEnd ||
           raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole);
    copy_size = from->max_number_key() + 1 - from_start;
#ifdef DEBUG
    // FAST_ELEMENT arrays cannot be uninitialized. Ensure they are already
    // marked with the hole.
    if (raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole) {
      for (int i = to_start + copy_size; i < to->length(); ++i) {
        ASSERT(to->get(i)->IsTheHole());
      }
    }
#endif
  }
  ASSERT((copy_size + static_cast<int>(to_start)) <= to->length());
  ASSERT(to != from);
  ASSERT(to_kind == FAST_ELEMENTS || to_kind == FAST_SMI_ONLY_ELEMENTS);
  if (copy_size == 0) return;
  for (int i = 0; i < copy_size; i++) {
    int entry = from->FindEntry(i + from_start);
    if (entry != SeededNumberDictionary::kNotFound) {
      Object* value = from->ValueAt(entry);
      ASSERT(!value->IsTheHole());
      to->set(i + to_start, value, SKIP_WRITE_BARRIER);
    } else {
      to->set_the_hole(i + to_start);
    }
  }
  if (to_kind == FAST_ELEMENTS) {
    if (!heap->InNewSpace(to)) {
      heap->RecordWrites(to->address(),
                         to->OffsetOfElementAt(to_start),
                         copy_size);
    }
    heap->incremental_marking()->RecordWrites(to);
  }
}


MUST_USE_RESULT static MaybeObject* CopyDoubleToObjectElements(
    FixedDoubleArray* from,
    uint32_t from_start,
    FixedArray* to,
    ElementsKind to_kind,
    uint32_t to_start,
    int raw_copy_size) {
  ASSERT(to_kind == FAST_ELEMENTS || to_kind == FAST_SMI_ONLY_ELEMENTS);
  int copy_size = raw_copy_size;
  if (raw_copy_size < 0) {
    ASSERT(raw_copy_size == ElementsAccessor::kCopyToEnd ||
           raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole);
    copy_size = Min(from->length() - from_start,
                    to->length() - to_start);
#ifdef DEBUG
    // FAST_ELEMENT arrays cannot be uninitialized. Ensure they are already
    // marked with the hole.
    if (raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole) {
      for (int i = to_start + copy_size; i < to->length(); ++i) {
        ASSERT(to->get(i)->IsTheHole());
      }
    }
#endif
  }
  ASSERT((copy_size + static_cast<int>(to_start)) <= to->length() &&
         (copy_size + static_cast<int>(from_start)) <= from->length());
  if (copy_size == 0) return from;
  for (int i = 0; i < copy_size; ++i) {
    if (to_kind == FAST_SMI_ONLY_ELEMENTS) {
      UNIMPLEMENTED();
      return Failure::Exception();
    } else {
      MaybeObject* maybe_value = from->get(i + from_start);
      Object* value;
      ASSERT(to_kind == FAST_ELEMENTS);
      // Because FAST_DOUBLE_ELEMENTS -> FAST_ELEMENT allocate HeapObjects
      // iteratively, the allocate must succeed within a single GC cycle,
      // otherwise the retry after the GC will also fail. In order to ensure
      // that no GC is triggered, allocate HeapNumbers from old space if they
      // can't be taken from new space.
      if (!maybe_value->ToObject(&value)) {
        ASSERT(maybe_value->IsRetryAfterGC() || maybe_value->IsOutOfMemory());
        Heap* heap = from->GetHeap();
        MaybeObject* maybe_value_object =
            heap->AllocateHeapNumber(from->get_scalar(i + from_start),
                                     TENURED);
        if (!maybe_value_object->ToObject(&value)) return maybe_value_object;
      }
      to->set(i + to_start, value, UPDATE_WRITE_BARRIER);
    }
  }
  return to;
}


static void CopyDoubleToDoubleElements(FixedDoubleArray* from,
                                       uint32_t from_start,
                                       FixedDoubleArray* to,
                                       uint32_t to_start,
                                       int raw_copy_size) {
  int copy_size = raw_copy_size;
  if (raw_copy_size < 0) {
    ASSERT(raw_copy_size == ElementsAccessor::kCopyToEnd ||
           raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole);
    copy_size = Min(from->length() - from_start,
                    to->length() - to_start);
    if (raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole) {
      for (int i = to_start + copy_size; i < to->length(); ++i) {
        to->set_the_hole(i);
      }
    }
  }
  ASSERT((copy_size + static_cast<int>(to_start)) <= to->length() &&
         (copy_size + static_cast<int>(from_start)) <= from->length());
  if (copy_size == 0) return;
  Address to_address = to->address() + FixedDoubleArray::kHeaderSize;
  Address from_address = from->address() + FixedDoubleArray::kHeaderSize;
  to_address += kDoubleSize * to_start;
  from_address += kDoubleSize * from_start;
  int words_per_double = (kDoubleSize / kPointerSize);
  CopyWords(reinterpret_cast<Object**>(to_address),
            reinterpret_cast<Object**>(from_address),
            words_per_double * copy_size);
}


static void CopyObjectToDoubleElements(FixedArray* from,
                                       uint32_t from_start,
                                       FixedDoubleArray* to,
                                       uint32_t to_start,
                                       int raw_copy_size) {
  int copy_size = raw_copy_size;
  if (raw_copy_size < 0) {
    ASSERT(raw_copy_size == ElementsAccessor::kCopyToEnd ||
           raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole);
    copy_size = from->length() - from_start;
    if (raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole) {
      for (int i = to_start + copy_size; i < to->length(); ++i) {
        to->set_the_hole(i);
      }
    }
  }
  ASSERT((copy_size + static_cast<int>(to_start)) <= to->length() &&
         (copy_size + static_cast<int>(from_start)) <= from->length());
  if (copy_size == 0) return;
  for (int i = 0; i < copy_size; i++) {
    Object* hole_or_object = from->get(i + from_start);
    if (hole_or_object->IsTheHole()) {
      to->set_the_hole(i + to_start);
    } else {
      to->set(i + to_start, hole_or_object->Number());
    }
  }
}


static void CopyDictionaryToDoubleElements(SeededNumberDictionary* from,
                                           uint32_t from_start,
                                           FixedDoubleArray* to,
                                           uint32_t to_start,
                                           int raw_copy_size) {
  int copy_size = raw_copy_size;
  if (copy_size < 0) {
    ASSERT(copy_size == ElementsAccessor::kCopyToEnd ||
           copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole);
    copy_size = from->max_number_key() + 1 - from_start;
    if (raw_copy_size == ElementsAccessor::kCopyToEndAndInitializeToHole) {
      for (int i = to_start + copy_size; i < to->length(); ++i) {
        to->set_the_hole(i);
      }
    }
  }
  ASSERT(copy_size + static_cast<int>(to_start) <= to->length());
  if (copy_size == 0) return;
  for (int i = 0; i < copy_size; i++) {
    int entry = from->FindEntry(i + from_start);
    if (entry != SeededNumberDictionary::kNotFound) {
      to->set(i + to_start, from->ValueAt(entry)->Number());
    } else {
      to->set_the_hole(i + to_start);
    }
  }
}


// Base class for element handler implementations. Contains the
// the common logic for objects with different ElementsKinds.
// Subclasses must specialize method for which the element
// implementation differs from the base class implementation.
//
// This class is intended to be used in the following way:
//
//   class SomeElementsAccessor :
//       public ElementsAccessorBase<SomeElementsAccessor,
//                                   BackingStoreClass> {
//     ...
//   }
//
// This is an example of the Curiously Recurring Template Pattern (see
// http://en.wikipedia.org/wiki/Curiously_recurring_template_pattern).  We use
// CRTP to guarantee aggressive compile time optimizations (i.e.  inlining and
// specialization of SomeElementsAccessor methods).
template <typename ElementsAccessorSubclass,
          typename ElementsTraitsParam>
class ElementsAccessorBase : public ElementsAccessor {
 protected:
  explicit ElementsAccessorBase(const char* name)
      : ElementsAccessor(name) { }

  typedef ElementsTraitsParam ElementsTraits;
  typedef typename ElementsTraitsParam::BackingStore BackingStore;

  virtual ElementsKind kind() const { return ElementsTraits::Kind; }

  static bool HasElementImpl(Object* receiver,
                             JSObject* holder,
                             uint32_t key,
                             BackingStore* backing_store) {
    MaybeObject* element =
        ElementsAccessorSubclass::GetImpl(receiver, holder, key, backing_store);
    return !element->IsTheHole();
  }

  virtual bool HasElement(Object* receiver,
                          JSObject* holder,
                          uint32_t key,
                          FixedArrayBase* backing_store) {
    if (backing_store == NULL) {
      backing_store = holder->elements();
    }
    return ElementsAccessorSubclass::HasElementImpl(
        receiver, holder, key, BackingStore::cast(backing_store));
  }

  virtual MaybeObject* Get(Object* receiver,
                           JSObject* holder,
                           uint32_t key,
                           FixedArrayBase* backing_store) {
    if (backing_store == NULL) {
      backing_store = holder->elements();
    }
    return ElementsAccessorSubclass::GetImpl(
        receiver, holder, key, BackingStore::cast(backing_store));
  }

  static MaybeObject* GetImpl(Object* receiver,
                              JSObject* obj,
                              uint32_t key,
                              BackingStore* backing_store) {
    return (key < ElementsAccessorSubclass::GetCapacityImpl(backing_store))
           ? backing_store->get(key)
           : backing_store->GetHeap()->the_hole_value();
  }

  virtual MaybeObject* SetLength(JSArray* array,
                                 Object* length) {
    return ElementsAccessorSubclass::SetLengthImpl(
        array, length, BackingStore::cast(array->elements()));
  }

  static MaybeObject* SetLengthImpl(JSObject* obj,
                                    Object* length,
                                    BackingStore* backing_store);

  virtual MaybeObject* SetCapacityAndLength(JSArray* array,
                                            int capacity,
                                            int length) {
    return ElementsAccessorSubclass::SetFastElementsCapacityAndLength(
        array,
        capacity,
        length);
  }

  static MaybeObject* SetFastElementsCapacityAndLength(JSObject* obj,
                                                       int capacity,
                                                       int length) {
    UNIMPLEMENTED();
    return obj;
  }

  virtual MaybeObject* Delete(JSObject* obj,
                              uint32_t key,
                              JSReceiver::DeleteMode mode) = 0;

  static MaybeObject* CopyElementsImpl(FixedArrayBase* from,
                                       uint32_t from_start,
                                       FixedArrayBase* to,
                                       ElementsKind to_kind,
                                       uint32_t to_start,
                                       int copy_size) {
    UNREACHABLE();
    return NULL;
  }

  virtual MaybeObject* CopyElements(JSObject* from_holder,
                                    uint32_t from_start,
                                    FixedArrayBase* to,
                                    ElementsKind to_kind,
                                    uint32_t to_start,
                                    int copy_size,
                                    FixedArrayBase* from) {
    if (from == NULL) {
      from = from_holder->elements();
    }
    if (from->length() == 0) {
      return from;
    }
    return ElementsAccessorSubclass::CopyElementsImpl(
        from, from_start, to, to_kind, to_start, copy_size);
  }

  virtual MaybeObject* AddElementsToFixedArray(Object* receiver,
                                               JSObject* holder,
                                               FixedArray* to,
                                               FixedArrayBase* from) {
    int len0 = to->length();
#ifdef DEBUG
    if (FLAG_enable_slow_asserts) {
      for (int i = 0; i < len0; i++) {
        ASSERT(!to->get(i)->IsTheHole());
      }
    }
#endif
    if (from == NULL) {
      from = holder->elements();
    }
    BackingStore* backing_store = BackingStore::cast(from);
    uint32_t len1 = ElementsAccessorSubclass::GetCapacityImpl(backing_store);

    // Optimize if 'other' is empty.
    // We cannot optimize if 'this' is empty, as other may have holes.
    if (len1 == 0) return to;

    // Compute how many elements are not in other.
    uint32_t extra = 0;
    for (uint32_t y = 0; y < len1; y++) {
      uint32_t key =
          ElementsAccessorSubclass::GetKeyForIndexImpl(backing_store, y);
      if (ElementsAccessorSubclass::HasElementImpl(
              receiver, holder, key, backing_store)) {
        MaybeObject* maybe_value =
            ElementsAccessorSubclass::GetImpl(receiver, holder,
                                              key, backing_store);
        Object* value;
        if (!maybe_value->ToObject(&value)) return maybe_value;
        ASSERT(!value->IsTheHole());
        if (!HasKey(to, value)) {
          extra++;
        }
      }
    }

    if (extra == 0) return to;

    // Allocate the result
    FixedArray* result;
    MaybeObject* maybe_obj =
        backing_store->GetHeap()->AllocateFixedArray(len0 + extra);
    if (!maybe_obj->To<FixedArray>(&result)) return maybe_obj;

    // Fill in the content
    {
      AssertNoAllocation no_gc;
      WriteBarrierMode mode = result->GetWriteBarrierMode(no_gc);
      for (int i = 0; i < len0; i++) {
        Object* e = to->get(i);
        ASSERT(e->IsString() || e->IsNumber());
        result->set(i, e, mode);
      }
    }
    // Fill in the extra values.
    uint32_t index = 0;
    for (uint32_t y = 0; y < len1; y++) {
      uint32_t key =
          ElementsAccessorSubclass::GetKeyForIndexImpl(backing_store, y);
      if (ElementsAccessorSubclass::HasElementImpl(
              receiver, holder, key, backing_store)) {
        MaybeObject* maybe_value =
            ElementsAccessorSubclass::GetImpl(receiver, holder,
                                              key, backing_store);
        Object* value;
        if (!maybe_value->ToObject(&value)) return maybe_value;
        if (!value->IsTheHole() && !HasKey(to, value)) {
          result->set(len0 + index, value);
          index++;
        }
      }
    }
    ASSERT(extra == index);
    return result;
  }

 protected:
  static uint32_t GetCapacityImpl(BackingStore* backing_store) {
    return backing_store->length();
  }

  virtual uint32_t GetCapacity(FixedArrayBase* backing_store) {
    return ElementsAccessorSubclass::GetCapacityImpl(
        BackingStore::cast(backing_store));
  }

  static uint32_t GetKeyForIndexImpl(BackingStore* backing_store,
                                     uint32_t index) {
    return index;
  }

  virtual uint32_t GetKeyForIndex(FixedArrayBase* backing_store,
                                  uint32_t index) {
    return ElementsAccessorSubclass::GetKeyForIndexImpl(
        BackingStore::cast(backing_store), index);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ElementsAccessorBase);
};


// Super class for all fast element arrays.
template<typename FastElementsAccessorSubclass,
         typename KindTraits,
         int ElementSize>
class FastElementsAccessor
    : public ElementsAccessorBase<FastElementsAccessorSubclass, KindTraits> {
 public:
  explicit FastElementsAccessor(const char* name)
      : ElementsAccessorBase<FastElementsAccessorSubclass,
                             KindTraits>(name) {}
 protected:
  friend class ElementsAccessorBase<FastElementsAccessorSubclass, KindTraits>;

  typedef typename KindTraits::BackingStore BackingStore;

  // Adjusts the length of the fast backing store or returns the new length or
  // undefined in case conversion to a slow backing store should be performed.
  static MaybeObject* SetLengthWithoutNormalize(BackingStore* backing_store,
                                                JSArray* array,
                                                Object* length_object,
                                                uint32_t length) {
    uint32_t old_capacity = backing_store->length();

    // Check whether the backing store should be shrunk.
    if (length <= old_capacity) {
      if (array->HasFastTypeElements()) {
        MaybeObject* maybe_obj = array->EnsureWritableFastElements();
        if (!maybe_obj->To(&backing_store)) return maybe_obj;
      }
      if (2 * length <= old_capacity) {
        // If more than half the elements won't be used, trim the array.
        if (length == 0) {
          array->initialize_elements();
        } else {
          backing_store->set_length(length);
          Address filler_start = backing_store->address() +
              BackingStore::OffsetOfElementAt(length);
          int filler_size = (old_capacity - length) * ElementSize;
          array->GetHeap()->CreateFillerObjectAt(filler_start, filler_size);
        }
      } else {
        // Otherwise, fill the unused tail with holes.
        int old_length = FastD2I(array->length()->Number());
        for (int i = length; i < old_length; i++) {
          backing_store->set_the_hole(i);
        }
      }
      return length_object;
    }

    // Check whether the backing store should be expanded.
    uint32_t min = JSObject::NewElementsCapacity(old_capacity);
    uint32_t new_capacity = length > min ? length : min;
    if (!array->ShouldConvertToSlowElements(new_capacity)) {
      MaybeObject* result = FastElementsAccessorSubclass::
          SetFastElementsCapacityAndLength(array, new_capacity, length);
      if (result->IsFailure()) return result;
      return length_object;
    }

    // Request conversion to slow elements.
    return array->GetHeap()->undefined_value();
  }
};


class FastObjectElementsAccessor
    : public FastElementsAccessor<FastObjectElementsAccessor,
                                  ElementsKindTraits<FAST_ELEMENTS>,
                                  kPointerSize> {
 public:
  explicit FastObjectElementsAccessor(const char* name)
      : FastElementsAccessor<FastObjectElementsAccessor,
                             ElementsKindTraits<FAST_ELEMENTS>,
                             kPointerSize>(name) {}

  static MaybeObject* DeleteCommon(JSObject* obj,
                                   uint32_t key) {
    ASSERT(obj->HasFastElements() ||
           obj->HasFastSmiOnlyElements() ||
           obj->HasFastArgumentsElements());
    Heap* heap = obj->GetHeap();
    FixedArray* backing_store = FixedArray::cast(obj->elements());
    if (backing_store->map() == heap->non_strict_arguments_elements_map()) {
      backing_store = FixedArray::cast(backing_store->get(1));
    } else {
      Object* writable;
      MaybeObject* maybe = obj->EnsureWritableFastElements();
      if (!maybe->ToObject(&writable)) return maybe;
      backing_store = FixedArray::cast(writable);
    }
    uint32_t length = static_cast<uint32_t>(
        obj->IsJSArray()
        ? Smi::cast(JSArray::cast(obj)->length())->value()
        : backing_store->length());
    if (key < length) {
      backing_store->set_the_hole(key);
      // If an old space backing store is larger than a certain size and
      // has too few used values, normalize it.
      // To avoid doing the check on every delete we require at least
      // one adjacent hole to the value being deleted.
      Object* hole = heap->the_hole_value();
      const int kMinLengthForSparsenessCheck = 64;
      if (backing_store->length() >= kMinLengthForSparsenessCheck &&
          !heap->InNewSpace(backing_store) &&
          ((key > 0 && backing_store->get(key - 1) == hole) ||
           (key + 1 < length && backing_store->get(key + 1) == hole))) {
        int num_used = 0;
        for (int i = 0; i < backing_store->length(); ++i) {
          if (backing_store->get(i) != hole) ++num_used;
          // Bail out early if more than 1/4 is used.
          if (4 * num_used > backing_store->length()) break;
        }
        if (4 * num_used <= backing_store->length()) {
          MaybeObject* result = obj->NormalizeElements();
          if (result->IsFailure()) return result;
        }
      }
    }
    return heap->true_value();
  }

  static MaybeObject* CopyElementsImpl(FixedArrayBase* from,
                                       uint32_t from_start,
                                       FixedArrayBase* to,
                                       ElementsKind to_kind,
                                       uint32_t to_start,
                                       int copy_size) {
    switch (to_kind) {
      case FAST_SMI_ONLY_ELEMENTS:
      case FAST_ELEMENTS: {
        CopyObjectToObjectElements(
            FixedArray::cast(from), ElementsTraits::Kind, from_start,
            FixedArray::cast(to), to_kind, to_start, copy_size);
        return from;
      }
      case FAST_DOUBLE_ELEMENTS:
        CopyObjectToDoubleElements(
            FixedArray::cast(from), from_start,
            FixedDoubleArray::cast(to), to_start, copy_size);
        return from;
      default:
        UNREACHABLE();
    }
    return to->GetHeap()->undefined_value();
  }


  static MaybeObject* SetFastElementsCapacityAndLength(JSObject* obj,
                                                       uint32_t capacity,
                                                       uint32_t length) {
    JSObject::SetFastElementsCapacityMode set_capacity_mode =
        obj->HasFastSmiOnlyElements()
            ? JSObject::kAllowSmiOnlyElements
            : JSObject::kDontAllowSmiOnlyElements;
    return obj->SetFastElementsCapacityAndLength(capacity,
                                                 length,
                                                 set_capacity_mode);
  }

 protected:
  friend class FastElementsAccessor<FastObjectElementsAccessor,
                                    ElementsKindTraits<FAST_ELEMENTS>,
                                    kPointerSize>;

  virtual MaybeObject* Delete(JSObject* obj,
                              uint32_t key,
                              JSReceiver::DeleteMode mode) {
    return DeleteCommon(obj, key);
  }
};


class FastDoubleElementsAccessor
    : public FastElementsAccessor<FastDoubleElementsAccessor,
                                  ElementsKindTraits<FAST_DOUBLE_ELEMENTS>,
                                  kDoubleSize> {
 public:
  explicit FastDoubleElementsAccessor(const char* name)
      : FastElementsAccessor<FastDoubleElementsAccessor,
                             ElementsKindTraits<FAST_DOUBLE_ELEMENTS>,
                             kDoubleSize>(name) {}

  static MaybeObject* SetFastElementsCapacityAndLength(JSObject* obj,
                                                       uint32_t capacity,
                                                       uint32_t length) {
    return obj->SetFastDoubleElementsCapacityAndLength(capacity, length);
  }

 protected:
  friend class ElementsAccessorBase<FastDoubleElementsAccessor,
                                    ElementsKindTraits<FAST_DOUBLE_ELEMENTS> >;
  friend class FastElementsAccessor<FastDoubleElementsAccessor,
                                    ElementsKindTraits<FAST_DOUBLE_ELEMENTS>,
                                    kDoubleSize>;

  static MaybeObject* CopyElementsImpl(FixedArrayBase* from,
                                       uint32_t from_start,
                                       FixedArrayBase* to,
                                       ElementsKind to_kind,
                                       uint32_t to_start,
                                       int copy_size) {
    switch (to_kind) {
      case FAST_SMI_ONLY_ELEMENTS:
      case FAST_ELEMENTS:
        return CopyDoubleToObjectElements(
            FixedDoubleArray::cast(from), from_start, FixedArray::cast(to),
            to_kind, to_start, copy_size);
      case FAST_DOUBLE_ELEMENTS:
        CopyDoubleToDoubleElements(FixedDoubleArray::cast(from), from_start,
                                   FixedDoubleArray::cast(to),
                                   to_start, copy_size);
        return from;
      default:
        UNREACHABLE();
    }
    return to->GetHeap()->undefined_value();
  }

  virtual MaybeObject* Delete(JSObject* obj,
                              uint32_t key,
                              JSReceiver::DeleteMode mode) {
    int length = obj->IsJSArray()
        ? Smi::cast(JSArray::cast(obj)->length())->value()
        : FixedDoubleArray::cast(obj->elements())->length();
    if (key < static_cast<uint32_t>(length)) {
      FixedDoubleArray::cast(obj->elements())->set_the_hole(key);
    }
    return obj->GetHeap()->true_value();
  }

  static bool HasElementImpl(Object* receiver,
                             JSObject* holder,
                             uint32_t key,
                             FixedDoubleArray* backing_store) {
    return key < static_cast<uint32_t>(backing_store->length()) &&
        !backing_store->is_the_hole(key);
  }
};


// Super class for all external element arrays.
template<typename ExternalElementsAccessorSubclass,
         ElementsKind Kind>
class ExternalElementsAccessor
    : public ElementsAccessorBase<ExternalElementsAccessorSubclass,
                                  ElementsKindTraits<Kind> > {
 public:
  explicit ExternalElementsAccessor(const char* name)
      : ElementsAccessorBase<ExternalElementsAccessorSubclass,
                             ElementsKindTraits<Kind> >(name) {}

 protected:
  typedef typename ElementsKindTraits<Kind>::BackingStore BackingStore;

  friend class ElementsAccessorBase<ExternalElementsAccessorSubclass,
                                    ElementsKindTraits<Kind> >;

  static MaybeObject* GetImpl(Object* receiver,
                              JSObject* obj,
                              uint32_t key,
                              BackingStore* backing_store) {
    return
        key < ExternalElementsAccessorSubclass::GetCapacityImpl(backing_store)
        ? backing_store->get(key)
        : backing_store->GetHeap()->undefined_value();
  }

  static MaybeObject* SetLengthImpl(JSObject* obj,
                                    Object* length,
                                    BackingStore* backing_store) {
    // External arrays do not support changing their length.
    UNREACHABLE();
    return obj;
  }

  virtual MaybeObject* Delete(JSObject* obj,
                              uint32_t key,
                              JSReceiver::DeleteMode mode) {
    // External arrays always ignore deletes.
    return obj->GetHeap()->true_value();
  }

  static bool HasElementImpl(Object* receiver,
                             JSObject* holder,
                             uint32_t key,
                             BackingStore* backing_store) {
    uint32_t capacity =
        ExternalElementsAccessorSubclass::GetCapacityImpl(backing_store);
    return key < capacity;
  }
};


class ExternalByteElementsAccessor
    : public ExternalElementsAccessor<ExternalByteElementsAccessor,
                                      EXTERNAL_BYTE_ELEMENTS> {
 public:
  explicit ExternalByteElementsAccessor(const char* name)
      : ExternalElementsAccessor<ExternalByteElementsAccessor,
                                 EXTERNAL_BYTE_ELEMENTS>(name) {}
};


class ExternalUnsignedByteElementsAccessor
    : public ExternalElementsAccessor<ExternalUnsignedByteElementsAccessor,
                                      EXTERNAL_UNSIGNED_BYTE_ELEMENTS> {
 public:
  explicit ExternalUnsignedByteElementsAccessor(const char* name)
      : ExternalElementsAccessor<ExternalUnsignedByteElementsAccessor,
                                 EXTERNAL_UNSIGNED_BYTE_ELEMENTS>(name) {}
};


class ExternalShortElementsAccessor
    : public ExternalElementsAccessor<ExternalShortElementsAccessor,
                                      EXTERNAL_SHORT_ELEMENTS> {
 public:
  explicit ExternalShortElementsAccessor(const char* name)
      : ExternalElementsAccessor<ExternalShortElementsAccessor,
                                 EXTERNAL_SHORT_ELEMENTS>(name) {}
};


class ExternalUnsignedShortElementsAccessor
    : public ExternalElementsAccessor<ExternalUnsignedShortElementsAccessor,
                                      EXTERNAL_UNSIGNED_SHORT_ELEMENTS> {
 public:
  explicit ExternalUnsignedShortElementsAccessor(const char* name)
      : ExternalElementsAccessor<ExternalUnsignedShortElementsAccessor,
                                 EXTERNAL_UNSIGNED_SHORT_ELEMENTS>(name) {}
};


class ExternalIntElementsAccessor
    : public ExternalElementsAccessor<ExternalIntElementsAccessor,
                                      EXTERNAL_INT_ELEMENTS> {
 public:
  explicit ExternalIntElementsAccessor(const char* name)
      : ExternalElementsAccessor<ExternalIntElementsAccessor,
                                 EXTERNAL_INT_ELEMENTS>(name) {}
};


class ExternalUnsignedIntElementsAccessor
    : public ExternalElementsAccessor<ExternalUnsignedIntElementsAccessor,
                                      EXTERNAL_UNSIGNED_INT_ELEMENTS> {
 public:
  explicit ExternalUnsignedIntElementsAccessor(const char* name)
      : ExternalElementsAccessor<ExternalUnsignedIntElementsAccessor,
                                 EXTERNAL_UNSIGNED_INT_ELEMENTS>(name) {}
};


class ExternalFloatElementsAccessor
    : public ExternalElementsAccessor<ExternalFloatElementsAccessor,
                                      EXTERNAL_FLOAT_ELEMENTS> {
 public:
  explicit ExternalFloatElementsAccessor(const char* name)
      : ExternalElementsAccessor<ExternalFloatElementsAccessor,
                                 EXTERNAL_FLOAT_ELEMENTS>(name) {}
};


class ExternalDoubleElementsAccessor
    : public ExternalElementsAccessor<ExternalDoubleElementsAccessor,
                                      EXTERNAL_DOUBLE_ELEMENTS> {
 public:
  explicit ExternalDoubleElementsAccessor(const char* name)
      : ExternalElementsAccessor<ExternalDoubleElementsAccessor,
                                 EXTERNAL_DOUBLE_ELEMENTS>(name) {}
};


class PixelElementsAccessor
    : public ExternalElementsAccessor<PixelElementsAccessor,
                                      EXTERNAL_PIXEL_ELEMENTS> {
 public:
  explicit PixelElementsAccessor(const char* name)
      : ExternalElementsAccessor<PixelElementsAccessor,
                                 EXTERNAL_PIXEL_ELEMENTS>(name) {}
};


class DictionaryElementsAccessor
    : public ElementsAccessorBase<DictionaryElementsAccessor,
                                  ElementsKindTraits<DICTIONARY_ELEMENTS> > {
 public:
  explicit DictionaryElementsAccessor(const char* name)
      : ElementsAccessorBase<DictionaryElementsAccessor,
                             ElementsKindTraits<DICTIONARY_ELEMENTS> >(name) {}

  // Adjusts the length of the dictionary backing store and returns the new
  // length according to ES5 section 15.4.5.2 behavior.
  static MaybeObject* SetLengthWithoutNormalize(SeededNumberDictionary* dict,
                                                JSArray* array,
                                                Object* length_object,
                                                uint32_t length) {
    if (length == 0) {
      // If the length of a slow array is reset to zero, we clear
      // the array and flush backing storage. This has the added
      // benefit that the array returns to fast mode.
      Object* obj;
      MaybeObject* maybe_obj = array->ResetElements();
      if (!maybe_obj->ToObject(&obj)) return maybe_obj;
    } else {
      uint32_t new_length = length;
      uint32_t old_length = static_cast<uint32_t>(array->length()->Number());
      if (new_length < old_length) {
        // Find last non-deletable element in range of elements to be
        // deleted and adjust range accordingly.
        Heap* heap = array->GetHeap();
        int capacity = dict->Capacity();
        for (int i = 0; i < capacity; i++) {
          Object* key = dict->KeyAt(i);
          if (key->IsNumber()) {
            uint32_t number = static_cast<uint32_t>(key->Number());
            if (new_length <= number && number < old_length) {
              PropertyDetails details = dict->DetailsAt(i);
              if (details.IsDontDelete()) new_length = number + 1;
            }
          }
        }
        if (new_length != length) {
          MaybeObject* maybe_object = heap->NumberFromUint32(new_length);
          if (!maybe_object->To(&length_object)) return maybe_object;
        }

        // Remove elements that should be deleted.
        int removed_entries = 0;
        Object* the_hole_value = heap->the_hole_value();
        for (int i = 0; i < capacity; i++) {
          Object* key = dict->KeyAt(i);
          if (key->IsNumber()) {
            uint32_t number = static_cast<uint32_t>(key->Number());
            if (new_length <= number && number < old_length) {
              dict->SetEntry(i, the_hole_value, the_hole_value);
              removed_entries++;
            }
          }
        }

        // Update the number of elements.
        dict->ElementsRemoved(removed_entries);
      }
    }
    return length_object;
  }

  static MaybeObject* DeleteCommon(JSObject* obj,
                                   uint32_t key,
                                   JSReceiver::DeleteMode mode) {
    Isolate* isolate = obj->GetIsolate();
    Heap* heap = isolate->heap();
    FixedArray* backing_store = FixedArray::cast(obj->elements());
    bool is_arguments =
        (obj->GetElementsKind() == NON_STRICT_ARGUMENTS_ELEMENTS);
    if (is_arguments) {
      backing_store = FixedArray::cast(backing_store->get(1));
    }
    SeededNumberDictionary* dictionary =
        SeededNumberDictionary::cast(backing_store);
    int entry = dictionary->FindEntry(key);
    if (entry != SeededNumberDictionary::kNotFound) {
      Object* result = dictionary->DeleteProperty(entry, mode);
      if (result == heap->true_value()) {
        MaybeObject* maybe_elements = dictionary->Shrink(key);
        FixedArray* new_elements = NULL;
        if (!maybe_elements->To(&new_elements)) {
          return maybe_elements;
        }
        if (is_arguments) {
          FixedArray::cast(obj->elements())->set(1, new_elements);
        } else {
          obj->set_elements(new_elements);
        }
      }
      if (mode == JSObject::STRICT_DELETION &&
          result == heap->false_value()) {
        // In strict mode, attempting to delete a non-configurable property
        // throws an exception.
        HandleScope scope(isolate);
        Handle<Object> holder(obj);
        Handle<Object> name = isolate->factory()->NewNumberFromUint(key);
        Handle<Object> args[2] = { name, holder };
        Handle<Object> error =
            isolate->factory()->NewTypeError("strict_delete_property",
                                             HandleVector(args, 2));
        return isolate->Throw(*error);
      }
    }
    return heap->true_value();
  }

  static MaybeObject* CopyElementsImpl(FixedArrayBase* from,
                                       uint32_t from_start,
                                       FixedArrayBase* to,
                                       ElementsKind to_kind,
                                       uint32_t to_start,
                                       int copy_size) {
    switch (to_kind) {
      case FAST_SMI_ONLY_ELEMENTS:
      case FAST_ELEMENTS:
        CopyDictionaryToObjectElements(
            SeededNumberDictionary::cast(from), from_start,
            FixedArray::cast(to), to_kind, to_start, copy_size);
        return from;
      case FAST_DOUBLE_ELEMENTS:
        CopyDictionaryToDoubleElements(
            SeededNumberDictionary::cast(from), from_start,
            FixedDoubleArray::cast(to), to_start, copy_size);
        return from;
      default:
        UNREACHABLE();
    }
    return to->GetHeap()->undefined_value();
  }


 protected:
  friend class ElementsAccessorBase<DictionaryElementsAccessor,
                                    ElementsKindTraits<DICTIONARY_ELEMENTS> >;

  virtual MaybeObject* Delete(JSObject* obj,
                              uint32_t key,
                              JSReceiver::DeleteMode mode) {
    return DeleteCommon(obj, key, mode);
  }

  static MaybeObject* GetImpl(Object* receiver,
                              JSObject* obj,
                              uint32_t key,
                              SeededNumberDictionary* backing_store) {
    int entry = backing_store->FindEntry(key);
    if (entry != SeededNumberDictionary::kNotFound) {
      Object* element = backing_store->ValueAt(entry);
      PropertyDetails details = backing_store->DetailsAt(entry);
      if (details.type() == CALLBACKS) {
        return obj->GetElementWithCallback(receiver,
                                           element,
                                           key,
                                           obj);
      } else {
        return element;
      }
    }
    return obj->GetHeap()->the_hole_value();
  }

  static bool HasElementImpl(Object* receiver,
                             JSObject* holder,
                             uint32_t key,
                             SeededNumberDictionary* backing_store) {
    return backing_store->FindEntry(key) !=
        SeededNumberDictionary::kNotFound;
  }

  static uint32_t GetKeyForIndexImpl(SeededNumberDictionary* dict,
                                     uint32_t index) {
    Object* key = dict->KeyAt(index);
    return Smi::cast(key)->value();
  }
};


class NonStrictArgumentsElementsAccessor : public ElementsAccessorBase<
    NonStrictArgumentsElementsAccessor,
    ElementsKindTraits<NON_STRICT_ARGUMENTS_ELEMENTS> > {
 public:
  explicit NonStrictArgumentsElementsAccessor(const char* name)
      : ElementsAccessorBase<
          NonStrictArgumentsElementsAccessor,
          ElementsKindTraits<NON_STRICT_ARGUMENTS_ELEMENTS> >(name) {}
 protected:
  friend class ElementsAccessorBase<
      NonStrictArgumentsElementsAccessor,
      ElementsKindTraits<NON_STRICT_ARGUMENTS_ELEMENTS> >;

  static MaybeObject* GetImpl(Object* receiver,
                              JSObject* obj,
                              uint32_t key,
                              FixedArray* parameter_map) {
    Object* probe = GetParameterMapArg(obj, parameter_map, key);
    if (!probe->IsTheHole()) {
      Context* context = Context::cast(parameter_map->get(0));
      int context_index = Smi::cast(probe)->value();
      ASSERT(!context->get(context_index)->IsTheHole());
      return context->get(context_index);
    } else {
      // Object is not mapped, defer to the arguments.
      FixedArray* arguments = FixedArray::cast(parameter_map->get(1));
      MaybeObject* maybe_result = ElementsAccessor::ForArray(arguments)->Get(
          receiver, obj, key, arguments);
      Object* result;
      if (!maybe_result->ToObject(&result)) return maybe_result;
      // Elements of the arguments object in slow mode might be slow aliases.
      if (result->IsAliasedArgumentsEntry()) {
        AliasedArgumentsEntry* entry = AliasedArgumentsEntry::cast(result);
        Context* context = Context::cast(parameter_map->get(0));
        int context_index = entry->aliased_context_slot();
        ASSERT(!context->get(context_index)->IsTheHole());
        return context->get(context_index);
      } else {
        return result;
      }
    }
  }

  static MaybeObject* SetLengthImpl(JSObject* obj,
                                    Object* length,
                                    FixedArray* parameter_map) {
    // TODO(mstarzinger): This was never implemented but will be used once we
    // correctly implement [[DefineOwnProperty]] on arrays.
    UNIMPLEMENTED();
    return obj;
  }

  virtual MaybeObject* Delete(JSObject* obj,
                              uint32_t key,
                              JSReceiver::DeleteMode mode) {
    FixedArray* parameter_map = FixedArray::cast(obj->elements());
    Object* probe = GetParameterMapArg(obj, parameter_map, key);
    if (!probe->IsTheHole()) {
      // TODO(kmillikin): We could check if this was the last aliased
      // parameter, and revert to normal elements in that case.  That
      // would enable GC of the context.
      parameter_map->set_the_hole(key + 2);
    } else {
      FixedArray* arguments = FixedArray::cast(parameter_map->get(1));
      if (arguments->IsDictionary()) {
        return DictionaryElementsAccessor::DeleteCommon(obj, key, mode);
      } else {
        return FastObjectElementsAccessor::DeleteCommon(obj, key);
      }
    }
    return obj->GetHeap()->true_value();
  }

  static MaybeObject* CopyElementsImpl(FixedArrayBase* from,
                                       uint32_t from_start,
                                       FixedArrayBase* to,
                                       ElementsKind to_kind,
                                       uint32_t to_start,
                                       int copy_size) {
    FixedArray* parameter_map = FixedArray::cast(from);
    FixedArray* arguments = FixedArray::cast(parameter_map->get(1));
    ElementsAccessor* accessor = ElementsAccessor::ForArray(arguments);
    return accessor->CopyElements(NULL, from_start, to, to_kind,
                                  to_start, copy_size, arguments);
  }

  static uint32_t GetCapacityImpl(FixedArray* parameter_map) {
    FixedArrayBase* arguments = FixedArrayBase::cast(parameter_map->get(1));
    return Max(static_cast<uint32_t>(parameter_map->length() - 2),
               ForArray(arguments)->GetCapacity(arguments));
  }

  static uint32_t GetKeyForIndexImpl(FixedArray* dict,
                                     uint32_t index) {
    return index;
  }

  static bool HasElementImpl(Object* receiver,
                             JSObject* holder,
                             uint32_t key,
                             FixedArray* parameter_map) {
    Object* probe = GetParameterMapArg(holder, parameter_map, key);
    if (!probe->IsTheHole()) {
      return true;
    } else {
      FixedArrayBase* arguments = FixedArrayBase::cast(parameter_map->get(1));
      ElementsAccessor* accessor = ElementsAccessor::ForArray(arguments);
      return !accessor->Get(receiver, holder, key, arguments)->IsTheHole();
    }
  }

 private:
  static Object* GetParameterMapArg(JSObject* holder,
                                    FixedArray* parameter_map,
                                    uint32_t key) {
    uint32_t length = holder->IsJSArray()
        ? Smi::cast(JSArray::cast(holder)->length())->value()
        : parameter_map->length();
    return key < (length - 2 )
        ? parameter_map->get(key + 2)
        : parameter_map->GetHeap()->the_hole_value();
  }
};


ElementsAccessor* ElementsAccessor::ForArray(FixedArrayBase* array) {
  switch (array->map()->instance_type()) {
    case FIXED_ARRAY_TYPE:
      if (array->IsDictionary()) {
        return elements_accessors_[DICTIONARY_ELEMENTS];
      } else {
        return elements_accessors_[FAST_ELEMENTS];
      }
    case EXTERNAL_BYTE_ARRAY_TYPE:
      return elements_accessors_[EXTERNAL_BYTE_ELEMENTS];
    case EXTERNAL_UNSIGNED_BYTE_ARRAY_TYPE:
      return elements_accessors_[EXTERNAL_UNSIGNED_BYTE_ELEMENTS];
    case EXTERNAL_SHORT_ARRAY_TYPE:
      return elements_accessors_[EXTERNAL_SHORT_ELEMENTS];
    case EXTERNAL_UNSIGNED_SHORT_ARRAY_TYPE:
      return elements_accessors_[EXTERNAL_UNSIGNED_SHORT_ELEMENTS];
    case EXTERNAL_INT_ARRAY_TYPE:
      return elements_accessors_[EXTERNAL_INT_ELEMENTS];
    case EXTERNAL_UNSIGNED_INT_ARRAY_TYPE:
      return elements_accessors_[EXTERNAL_UNSIGNED_INT_ELEMENTS];
    case EXTERNAL_FLOAT_ARRAY_TYPE:
      return elements_accessors_[EXTERNAL_FLOAT_ELEMENTS];
    case EXTERNAL_DOUBLE_ARRAY_TYPE:
      return elements_accessors_[EXTERNAL_DOUBLE_ELEMENTS];
    case EXTERNAL_PIXEL_ARRAY_TYPE:
      return elements_accessors_[EXTERNAL_PIXEL_ELEMENTS];
    default:
      UNREACHABLE();
      return NULL;
  }
}


void ElementsAccessor::InitializeOncePerProcess() {
  static struct ConcreteElementsAccessors {
#define ACCESSOR_STRUCT(Class, Kind, Store) Class* Kind##_handler;
    ELEMENTS_LIST(ACCESSOR_STRUCT)
#undef ACCESSOR_STRUCT
  } element_accessors = {
#define ACCESSOR_INIT(Class, Kind, Store) new Class(#Kind),
    ELEMENTS_LIST(ACCESSOR_INIT)
#undef ACCESSOR_INIT
  };

  static ElementsAccessor* accessor_array[] = {
#define ACCESSOR_ARRAY(Class, Kind, Store) element_accessors.Kind##_handler,
    ELEMENTS_LIST(ACCESSOR_ARRAY)
#undef ACCESSOR_ARRAY
  };

  STATIC_ASSERT((sizeof(accessor_array) / sizeof(*accessor_array)) ==
                kElementsKindCount);

  elements_accessors_ = accessor_array;
}


template <typename ElementsAccessorSubclass, typename ElementsKindTraits>
MaybeObject* ElementsAccessorBase<ElementsAccessorSubclass,
                                  ElementsKindTraits>::
    SetLengthImpl(JSObject* obj,
                  Object* length,
                  typename ElementsKindTraits::BackingStore* backing_store) {
  JSArray* array = JSArray::cast(obj);

  // Fast case: The new length fits into a Smi.
  MaybeObject* maybe_smi_length = length->ToSmi();
  Object* smi_length = Smi::FromInt(0);
  if (maybe_smi_length->ToObject(&smi_length) && smi_length->IsSmi()) {
    const int value = Smi::cast(smi_length)->value();
    if (value >= 0) {
      Object* new_length;
      MaybeObject* result = ElementsAccessorSubclass::
          SetLengthWithoutNormalize(backing_store, array, smi_length, value);
      if (!result->ToObject(&new_length)) return result;
      ASSERT(new_length->IsSmi() || new_length->IsUndefined());
      if (new_length->IsSmi()) {
        array->set_length(Smi::cast(new_length));
        return array;
      }
    } else {
      return ThrowArrayLengthRangeError(array->GetHeap());
    }
  }

  // Slow case: The new length does not fit into a Smi or conversion
  // to slow elements is needed for other reasons.
  if (length->IsNumber()) {
    uint32_t value;
    if (length->ToArrayIndex(&value)) {
      SeededNumberDictionary* dictionary;
      MaybeObject* maybe_object = array->NormalizeElements();
      if (!maybe_object->To(&dictionary)) return maybe_object;
      Object* new_length;
      MaybeObject* result = DictionaryElementsAccessor::
          SetLengthWithoutNormalize(dictionary, array, length, value);
      if (!result->ToObject(&new_length)) return result;
      ASSERT(new_length->IsNumber());
      array->set_length(new_length);
      return array;
    } else {
      return ThrowArrayLengthRangeError(array->GetHeap());
    }
  }

  // Fall-back case: The new length is not a number so make the array
  // size one and set only element to length.
  FixedArray* new_backing_store;
  MaybeObject* maybe_obj = array->GetHeap()->AllocateFixedArray(1);
  if (!maybe_obj->To(&new_backing_store)) return maybe_obj;
  new_backing_store->set(0, length);
  { MaybeObject* result = array->SetContent(new_backing_store);
    if (result->IsFailure()) return result;
  }
  return array;
}


} }  // namespace v8::internal
