// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PTR_COMPR_H_
#define V8_PTR_COMPR_H_

#if V8_TARGET_ARCH_64_BIT

#include "src/globals.h"
#include "src/objects/slots.h"

namespace v8 {
namespace internal {

constexpr size_t kPtrComprHeapReservationSize = size_t{4} * GB;
constexpr size_t kPtrComprIsolateRootBias = kPtrComprHeapReservationSize / 2;
constexpr size_t kPtrComprIsolateRootAlignment = size_t{4} * GB;

// A CompressedObjectSlot instance describes a kTaggedSize-sized field ("slot")
// holding a compressed tagged pointer (smi or heap object).
// Its address() is the address of the slot.
// The slot's contents can be read and written using operator* and store().
class CompressedObjectSlot
    : public SlotBase<CompressedObjectSlot, Tagged_t, kTaggedSize> {
 public:
  using TObject = ObjectPtr;
  using THeapObjectSlot = CompressedHeapObjectSlot;

  static constexpr bool kCanBeWeak = false;

  CompressedObjectSlot() : SlotBase(kNullAddress) {}
  explicit CompressedObjectSlot(Address ptr) : SlotBase(ptr) {}
  explicit CompressedObjectSlot(Address* ptr)
      : SlotBase(reinterpret_cast<Address>(ptr)) {}
  inline explicit CompressedObjectSlot(ObjectPtr* object);
  explicit CompressedObjectSlot(Object const* const* ptr)
      : SlotBase(reinterpret_cast<Address>(ptr)) {}
  template <typename T>
  explicit CompressedObjectSlot(SlotBase<T, TData, kSlotDataSize> slot)
      : SlotBase(slot.address()) {}

  inline Object* operator*() const;
  // TODO(3770): drop this in favor of operator* once migration is complete.
  inline ObjectPtr load() const;
  inline void store(Object* value) const;

  inline ObjectPtr Acquire_Load() const;
  inline ObjectPtr Relaxed_Load() const;
  inline void Relaxed_Store(ObjectPtr value) const;
  inline void Release_Store(ObjectPtr value) const;
  inline ObjectPtr Release_CompareAndSwap(ObjectPtr old,
                                          ObjectPtr target) const;
  // Old-style alternative for the above, temporarily separate to allow
  // incremental transition.
  // TODO(3770): Get rid of the duplication when the migration is complete.
  inline Object* Acquire_Load1() const;
  inline void Relaxed_Store1(Object* value) const;
  inline void Release_Store1(Object* value) const;
};

// A CompressedMapWordSlot instance describes a kTaggedSize-sized map-word field
// ("slot") of heap objects holding a compressed tagged pointer or a Smi
// representing forwaring pointer value.
// This slot kind is similar to CompressedObjectSlot but decompression of
// forwarding pointer is different.
// Its address() is the address of the slot.
// The slot's contents can be read and written using operator* and store().
class CompressedMapWordSlot
    : public SlotBase<CompressedMapWordSlot, Tagged_t, kTaggedSize> {
 public:
  using TObject = ObjectPtr;

  static constexpr bool kCanBeWeak = false;

  CompressedMapWordSlot() : SlotBase(kNullAddress) {}
  explicit CompressedMapWordSlot(Address ptr) : SlotBase(ptr) {}

  // Compares memory representation of a value stored in the slot with given
  // raw value without decompression.
  inline bool contains_value(Address raw_value) const;

  inline ObjectPtr load() const;
  inline void store(ObjectPtr value) const;

  inline ObjectPtr Relaxed_Load() const;
  inline void Relaxed_Store(ObjectPtr value) const;

  inline ObjectPtr Acquire_Load() const;
  inline void Release_Store(ObjectPtr value) const;
  inline ObjectPtr Release_CompareAndSwap(ObjectPtr old,
                                          ObjectPtr target) const;
};

// A CompressedMaybeObjectSlot instance describes a kTaggedSize-sized field
// ("slot") holding a possibly-weak compressed tagged pointer
// (think: MaybeObject).
// Its address() is the address of the slot.
// The slot's contents can be read and written using operator* and store().
class CompressedMaybeObjectSlot
    : public SlotBase<CompressedMaybeObjectSlot, Tagged_t, kTaggedSize> {
 public:
  using TObject = MaybeObject;
  using THeapObjectSlot = CompressedHeapObjectSlot;

  static constexpr bool kCanBeWeak = true;

  CompressedMaybeObjectSlot() : SlotBase(kNullAddress) {}
  explicit CompressedMaybeObjectSlot(Address ptr) : SlotBase(ptr) {}
  explicit CompressedMaybeObjectSlot(ObjectPtr* ptr)
      : SlotBase(reinterpret_cast<Address>(ptr)) {}
  explicit CompressedMaybeObjectSlot(Object** ptr)
      : SlotBase(reinterpret_cast<Address>(ptr)) {}
  explicit CompressedMaybeObjectSlot(HeapObject** ptr)
      : SlotBase(reinterpret_cast<Address>(ptr)) {}
  template <typename T>
  explicit CompressedMaybeObjectSlot(SlotBase<T, TData, kSlotDataSize> slot)
      : SlotBase(slot.address()) {}

  inline MaybeObject operator*() const;
  // TODO(3770): drop this once ObjectSlot::load() is dropped.
  inline MaybeObject load() const;
  inline void store(MaybeObject value) const;

  inline MaybeObject Relaxed_Load() const;
  inline void Relaxed_Store(MaybeObject value) const;
  inline void Release_CompareAndSwap(MaybeObject old, MaybeObject target) const;
};

// A CompressedHeapObjectSlot instance describes a kTaggedSize-sized field
// ("slot") holding a weak or strong compressed pointer to a heap object (think:
// HeapObjectReference).
// Its address() is the address of the slot.
// The slot's contents can be read and written using operator* and store().
// In case it is known that that slot contains a strong heap object pointer,
// ToHeapObject() can be used to retrieve that heap object.
class CompressedHeapObjectSlot
    : public SlotBase<CompressedHeapObjectSlot, Tagged_t, kTaggedSize> {
 public:
  CompressedHeapObjectSlot() : SlotBase(kNullAddress) {}
  explicit CompressedHeapObjectSlot(Address ptr) : SlotBase(ptr) {}
  explicit CompressedHeapObjectSlot(ObjectPtr* ptr)
      : SlotBase(reinterpret_cast<Address>(ptr)) {}
  explicit CompressedHeapObjectSlot(HeapObject** ptr)
      : SlotBase(reinterpret_cast<Address>(ptr)) {}
  template <typename T>
  explicit CompressedHeapObjectSlot(SlotBase<T, TData, kSlotDataSize> slot)
      : SlotBase(slot.address()) {}

  inline HeapObjectReference operator*() const;
  inline void store(HeapObjectReference value) const;

  HeapObject* ToHeapObject() const {
    DCHECK((*location() & kHeapObjectTagMask) == kHeapObjectTag);
    return reinterpret_cast<HeapObject*>(*location());
  }

  void StoreHeapObject(HeapObject* value) const {
    *reinterpret_cast<HeapObject**>(address()) = value;
  }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_64_BIT

#endif  // V8_PTR_COMPR_H_
