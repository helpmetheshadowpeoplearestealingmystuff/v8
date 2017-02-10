// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_CONSTANT_ARRAY_BUILDER_H_
#define V8_INTERPRETER_CONSTANT_ARRAY_BUILDER_H_

#include "src/globals.h"
#include "src/identity-map.h"
#include "src/interpreter/bytecodes.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {

class Isolate;
class AstRawString;
class AstValue;

namespace interpreter {

// Constant array entries that represent singletons.
#define SINGLETON_CONSTANT_ENTRY_TYPES(V) \
  V(IteratorSymbol, iterator_symbol)      \
  V(HomeObjectSymbol, home_object_symbol) \
  V(EmptyFixedArray, empty_fixed_array)

// A helper class for constructing constant arrays for the
// interpreter. Each instance of this class is intended to be used to
// generate exactly one FixedArray of constants via the ToFixedArray
// method.
class V8_EXPORT_PRIVATE ConstantArrayBuilder final BASE_EMBEDDED {
 public:
  // Capacity of the 8-bit operand slice.
  static const size_t k8BitCapacity = 1u << kBitsPerByte;

  // Capacity of the 16-bit operand slice.
  static const size_t k16BitCapacity = (1u << 2 * kBitsPerByte) - k8BitCapacity;

  // Capacity of the 32-bit operand slice.
  static const size_t k32BitCapacity =
      kMaxUInt32 - k16BitCapacity - k8BitCapacity + 1;

  ConstantArrayBuilder(Zone* zone);

  // Generate a fixed array of constant handles based on inserted objects.
  Handle<FixedArray> ToFixedArray(Isolate* isolate);

  // Returns the object, as a handle in |isolate|, that is in the constant pool
  // array at index |index|. Returns null if there is no handle at this index.
  // Only expected to be used in tests.
  MaybeHandle<Object> At(size_t index, Isolate* isolate) const;

  // Returns the number of elements in the array.
  size_t size() const;

  // Insert an object into the constants array if it is not already present.
  // Returns the array index associated with the object.
  size_t Insert(Smi* smi);
  size_t Insert(const AstRawString* raw_string);
  size_t Insert(const AstValue* heap_number);
  size_t Insert(const Scope* scope);
#define INSERT_ENTRY(NAME, ...) size_t Insert##NAME();
  SINGLETON_CONSTANT_ENTRY_TYPES(INSERT_ENTRY)
#undef INSERT_ENTRY

  // Inserts an empty entry and returns the array index associated with the
  // reservation. The entry's handle value can be inserted by calling
  // SetDeferredAt().
  size_t InsertDeferred();

  // Sets the deferred value at |index| to |object|.
  void SetDeferredAt(size_t index, Handle<Object> object);

  // Creates a reserved entry in the constant pool and returns
  // the size of the operand that'll be required to hold the entry
  // when committed.
  OperandSize CreateReservedEntry();

  // Commit reserved entry and returns the constant pool index for the
  // SMI value.
  size_t CommitReservedEntry(OperandSize operand_size, Smi* value);

  // Discards constant pool reservation.
  void DiscardReservedEntry(OperandSize operand_size);

 private:
  typedef uint32_t index_t;

  class Entry {
   private:
    enum class Tag : uint8_t;

   public:
    explicit Entry(Smi* smi) : smi_(smi), tag_(Tag::kSmi) {}
    explicit Entry(const AstRawString* raw_string)
        : raw_string_(raw_string), tag_(Tag::kRawString) {}
    explicit Entry(const AstValue* heap_number)
        : heap_number_(heap_number), tag_(Tag::kHeapNumber) {}
    explicit Entry(const Scope* scope) : scope_(scope), tag_(Tag::kScope) {}

#define CONSTRUCT_ENTRY(NAME, LOWER_NAME) \
  static Entry NAME() { return Entry(Tag::k##NAME); }
    SINGLETON_CONSTANT_ENTRY_TYPES(CONSTRUCT_ENTRY)
#undef CONSTRUCT_ENTRY

    static Entry Deferred() { return Entry(Tag::kDeferred); }

    bool IsDeferred() const { return tag_ == Tag::kDeferred; }

    void SetDeferred(Handle<Object> handle) {
      DCHECK(tag_ == Tag::kDeferred);
      tag_ = Tag::kHandle;
      handle_ = handle;
    }

    Handle<Object> ToHandle(Isolate* isolate) const;

   private:
    explicit Entry(Tag tag) : tag_(tag) {}

    union {
      Handle<Object> handle_;
      Smi* smi_;
      const AstRawString* raw_string_;
      const AstValue* heap_number_;
      const Scope* scope_;
    };

    enum class Tag : uint8_t {
      kDeferred,
      kHandle,
      kSmi,
      kRawString,
      kHeapNumber,
      kScope,
#define ENTRY_TAG(NAME, ...) k##NAME,
      SINGLETON_CONSTANT_ENTRY_TYPES(ENTRY_TAG)
#undef ENTRY_TAG
    } tag_;
  };

  index_t AllocateIndex(Entry constant_entry);
  index_t AllocateReservedEntry(Smi* value);

  struct ConstantArraySlice final : public ZoneObject {
    ConstantArraySlice(Zone* zone, size_t start_index, size_t capacity,
                       OperandSize operand_size);
    void Reserve();
    void Unreserve();
    size_t Allocate(Entry entry);
    Entry& At(size_t index);
    const Entry& At(size_t index) const;

#if DEBUG
    void CheckAllElementsAreUnique(Isolate* isolate) const;
#endif

    inline size_t available() const { return capacity() - reserved() - size(); }
    inline size_t reserved() const { return reserved_; }
    inline size_t capacity() const { return capacity_; }
    inline size_t size() const { return constants_.size(); }
    inline size_t start_index() const { return start_index_; }
    inline size_t max_index() const { return start_index_ + capacity() - 1; }
    inline OperandSize operand_size() const { return operand_size_; }

   private:
    const size_t start_index_;
    const size_t capacity_;
    size_t reserved_;
    OperandSize operand_size_;
    ZoneVector<Entry> constants_;

    DISALLOW_COPY_AND_ASSIGN(ConstantArraySlice);
  };

  ConstantArraySlice* IndexToSlice(size_t index) const;
  ConstantArraySlice* OperandSizeToSlice(OperandSize operand_size) const;

  ConstantArraySlice* idx_slice_[3];
  base::TemplateHashMapImpl<intptr_t, index_t,
                            base::KeyEqualityMatcher<intptr_t>,
                            ZoneAllocationPolicy>
      constants_map_;
  ZoneMap<Smi*, index_t> smi_map_;
  ZoneVector<std::pair<Smi*, index_t>> smi_pairs_;

#define SINGLETON_ENTRY_FIELD(NAME, LOWER_NAME) int LOWER_NAME##_;
  SINGLETON_CONSTANT_ENTRY_TYPES(SINGLETON_ENTRY_FIELD)
#undef SINGLETON_ENTRY_FIELD

  Zone* zone_;
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_CONSTANT_ARRAY_BUILDER_H_
