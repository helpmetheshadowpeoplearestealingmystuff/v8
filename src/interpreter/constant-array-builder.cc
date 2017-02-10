// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/constant-array-builder.h"

#include <functional>
#include <set>

#include "src/ast/ast-value-factory.h"
#include "src/ast/ast.h"
#include "src/ast/scopes.h"
#include "src/base/functional.h"
#include "src/isolate.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace interpreter {

ConstantArrayBuilder::ConstantArraySlice::ConstantArraySlice(
    Zone* zone, size_t start_index, size_t capacity, OperandSize operand_size)
    : start_index_(start_index),
      capacity_(capacity),
      reserved_(0),
      operand_size_(operand_size),
      constants_(zone) {}

void ConstantArrayBuilder::ConstantArraySlice::Reserve() {
  DCHECK_GT(available(), 0u);
  reserved_++;
  DCHECK_LE(reserved_, capacity() - constants_.size());
}

void ConstantArrayBuilder::ConstantArraySlice::Unreserve() {
  DCHECK_GT(reserved_, 0u);
  reserved_--;
}

size_t ConstantArrayBuilder::ConstantArraySlice::Allocate(
    ConstantArrayBuilder::Entry entry) {
  DCHECK_GT(available(), 0u);
  size_t index = constants_.size();
  DCHECK_LT(index, capacity());
  constants_.push_back(entry);
  return index + start_index();
}

ConstantArrayBuilder::Entry& ConstantArrayBuilder::ConstantArraySlice::At(
    size_t index) {
  DCHECK_GE(index, start_index());
  DCHECK_LT(index, start_index() + size());
  return constants_[index - start_index()];
}

const ConstantArrayBuilder::Entry& ConstantArrayBuilder::ConstantArraySlice::At(
    size_t index) const {
  DCHECK_GE(index, start_index());
  DCHECK_LT(index, start_index() + size());
  return constants_[index - start_index()];
}

#if DEBUG
void ConstantArrayBuilder::ConstantArraySlice::CheckAllElementsAreUnique(
    Isolate* isolate) const {
  std::set<Object*> elements;
  for (const Entry& entry : constants_) {
    Handle<Object> handle = entry.ToHandle(isolate);
    if (elements.find(*handle) != elements.end()) {
      std::ostringstream os;
      os << "Duplicate constant found: " << Brief(*handle) << std::endl;
      // Print all the entries in the slice to help debug duplicates.
      size_t i = start_index();
      for (const Entry& prev_entry : constants_) {
        os << i++ << ": " << Brief(*prev_entry.ToHandle(isolate)) << std::endl;
      }
      FATAL(os.str().c_str());
    }
    elements.insert(*handle);
  }
}
#endif

STATIC_CONST_MEMBER_DEFINITION const size_t ConstantArrayBuilder::k8BitCapacity;
STATIC_CONST_MEMBER_DEFINITION const size_t
    ConstantArrayBuilder::k16BitCapacity;
STATIC_CONST_MEMBER_DEFINITION const size_t
    ConstantArrayBuilder::k32BitCapacity;

ConstantArrayBuilder::ConstantArrayBuilder(Zone* zone)
    : constants_map_(16, base::KeyEqualityMatcher<intptr_t>(),
                     ZoneAllocationPolicy(zone)),
      smi_map_(zone),
      smi_pairs_(zone),
#define INIT_SINGLETON_ENTRY_FIELD(NAME, LOWER_NAME) LOWER_NAME##_(-1),
      SINGLETON_CONSTANT_ENTRY_TYPES(INIT_SINGLETON_ENTRY_FIELD)
#undef INIT_SINGLETON_ENTRY_FIELD
          zone_(zone) {
  idx_slice_[0] =
      new (zone) ConstantArraySlice(zone, 0, k8BitCapacity, OperandSize::kByte);
  idx_slice_[1] = new (zone) ConstantArraySlice(
      zone, k8BitCapacity, k16BitCapacity, OperandSize::kShort);
  idx_slice_[2] = new (zone) ConstantArraySlice(
      zone, k8BitCapacity + k16BitCapacity, k32BitCapacity, OperandSize::kQuad);
}

size_t ConstantArrayBuilder::size() const {
  size_t i = arraysize(idx_slice_);
  while (i > 0) {
    ConstantArraySlice* slice = idx_slice_[--i];
    if (slice->size() > 0) {
      return slice->start_index() + slice->size();
    }
  }
  return idx_slice_[0]->size();
}

ConstantArrayBuilder::ConstantArraySlice* ConstantArrayBuilder::IndexToSlice(
    size_t index) const {
  for (ConstantArraySlice* slice : idx_slice_) {
    if (index <= slice->max_index()) {
      return slice;
    }
  }
  UNREACHABLE();
  return nullptr;
}

MaybeHandle<Object> ConstantArrayBuilder::At(size_t index,
                                             Isolate* isolate) const {
  const ConstantArraySlice* slice = IndexToSlice(index);
  DCHECK_LT(index, slice->capacity());
  if (index < slice->start_index() + slice->size()) {
    const Entry& entry = slice->At(index);
    if (!entry.IsDeferred()) return entry.ToHandle(isolate);
  }
  return MaybeHandle<Object>();
}

Handle<FixedArray> ConstantArrayBuilder::ToFixedArray(Isolate* isolate) {
  Handle<FixedArray> fixed_array = isolate->factory()->NewFixedArrayWithHoles(
      static_cast<int>(size()), PretenureFlag::TENURED);
  int array_index = 0;
  for (const ConstantArraySlice* slice : idx_slice_) {
    DCHECK_EQ(slice->reserved(), 0);
    DCHECK(array_index == 0 ||
           base::bits::IsPowerOfTwo32(static_cast<uint32_t>(array_index)));
#if DEBUG
    // Different slices might contain the same element due to reservations, but
    // all elements within a slice should be unique. If this DCHECK fails, then
    // the AST nodes are not being internalized within a CanonicalHandleScope.
    slice->CheckAllElementsAreUnique(isolate);
#endif
    // Copy objects from slice into array.
    for (size_t i = 0; i < slice->size(); ++i) {
      fixed_array->set(array_index++,
                       *slice->At(slice->start_index() + i).ToHandle(isolate));
    }
    // Leave holes where reservations led to unused slots.
    size_t padding = slice->capacity() - slice->size();
    if (static_cast<size_t>(fixed_array->length() - array_index) <= padding) {
      break;
    }
    array_index += padding;
  }
  DCHECK_GE(array_index, fixed_array->length());
  return fixed_array;
}

size_t ConstantArrayBuilder::Insert(Smi* smi) {
  auto entry = smi_map_.find(smi);
  if (entry == smi_map_.end()) {
    return AllocateReservedEntry(smi);
  }
  return entry->second;
}

size_t ConstantArrayBuilder::Insert(const AstRawString* raw_string) {
  return constants_map_
      .LookupOrInsert(reinterpret_cast<intptr_t>(raw_string),
                      raw_string->hash(),
                      [&]() { return AllocateIndex(Entry(raw_string)); },
                      ZoneAllocationPolicy(zone_))
      ->value;
}

size_t ConstantArrayBuilder::Insert(const AstValue* heap_number) {
  // This method only accepts heap numbers. Other types of ast value should
  // either be passed through as raw values (in the case of strings), use the
  // singleton Insert methods (in the case of symbols), or skip the constant
  // pool entirely and use bytecodes with immediate values (Smis, booleans,
  // undefined, etc.).
  DCHECK(heap_number->IsHeapNumber());
  return constants_map_
      .LookupOrInsert(reinterpret_cast<intptr_t>(heap_number),
                      static_cast<uint32_t>(base::hash_value(heap_number)),
                      [&]() { return AllocateIndex(Entry(heap_number)); },
                      ZoneAllocationPolicy(zone_))
      ->value;
}

size_t ConstantArrayBuilder::Insert(const Scope* scope) {
  return constants_map_
      .LookupOrInsert(reinterpret_cast<intptr_t>(scope),
                      static_cast<uint32_t>(base::hash_value(scope)),
                      [&]() { return AllocateIndex(Entry(scope)); },
                      ZoneAllocationPolicy(zone_))
      ->value;
}

#define INSERT_ENTRY(NAME, LOWER_NAME)              \
  size_t ConstantArrayBuilder::Insert##NAME() {     \
    if (LOWER_NAME##_ < 0) {                        \
      LOWER_NAME##_ = AllocateIndex(Entry::NAME()); \
    }                                               \
    return LOWER_NAME##_;                           \
  }
SINGLETON_CONSTANT_ENTRY_TYPES(INSERT_ENTRY)
#undef INSERT_ENTRY

ConstantArrayBuilder::index_t ConstantArrayBuilder::AllocateIndex(
    ConstantArrayBuilder::Entry entry) {
  for (size_t i = 0; i < arraysize(idx_slice_); ++i) {
    if (idx_slice_[i]->available() > 0) {
      return static_cast<index_t>(idx_slice_[i]->Allocate(entry));
    }
  }
  UNREACHABLE();
  return kMaxUInt32;
}

ConstantArrayBuilder::ConstantArraySlice*
ConstantArrayBuilder::OperandSizeToSlice(OperandSize operand_size) const {
  ConstantArraySlice* slice = nullptr;
  switch (operand_size) {
    case OperandSize::kNone:
      UNREACHABLE();
      break;
    case OperandSize::kByte:
      slice = idx_slice_[0];
      break;
    case OperandSize::kShort:
      slice = idx_slice_[1];
      break;
    case OperandSize::kQuad:
      slice = idx_slice_[2];
      break;
  }
  DCHECK(slice->operand_size() == operand_size);
  return slice;
}

size_t ConstantArrayBuilder::InsertDeferred() {
  return AllocateIndex(Entry::Deferred());
}

void ConstantArrayBuilder::SetDeferredAt(size_t index, Handle<Object> object) {
  ConstantArraySlice* slice = IndexToSlice(index);
  return slice->At(index).SetDeferred(object);
}

OperandSize ConstantArrayBuilder::CreateReservedEntry() {
  for (size_t i = 0; i < arraysize(idx_slice_); ++i) {
    if (idx_slice_[i]->available() > 0) {
      idx_slice_[i]->Reserve();
      return idx_slice_[i]->operand_size();
    }
  }
  UNREACHABLE();
  return OperandSize::kNone;
}

ConstantArrayBuilder::index_t ConstantArrayBuilder::AllocateReservedEntry(
    Smi* value) {
  index_t index = static_cast<index_t>(AllocateIndex(Entry(value)));
  smi_map_[value] = index;
  return index;
}

size_t ConstantArrayBuilder::CommitReservedEntry(OperandSize operand_size,
                                                 Smi* value) {
  DiscardReservedEntry(operand_size);
  size_t index;
  auto entry = smi_map_.find(value);
  if (entry == smi_map_.end()) {
    index = AllocateReservedEntry(value);
  } else {
    ConstantArraySlice* slice = OperandSizeToSlice(operand_size);
    index = entry->second;
    if (index > slice->max_index()) {
      // The object is already in the constant array, but may have an
      // index too big for the reserved operand_size. So, duplicate
      // entry with the smaller operand size.
      index = AllocateReservedEntry(value);
    }
    DCHECK_LE(index, slice->max_index());
  }
  return index;
}

void ConstantArrayBuilder::DiscardReservedEntry(OperandSize operand_size) {
  OperandSizeToSlice(operand_size)->Unreserve();
}

Handle<Object> ConstantArrayBuilder::Entry::ToHandle(Isolate* isolate) const {
  switch (tag_) {
    case Tag::kDeferred:
      // We shouldn't have any deferred entries by now.
      UNREACHABLE();
      return Handle<Object>::null();
    case Tag::kHandle:
      return handle_;
    case Tag::kSmi:
      return handle(smi_, isolate);
    case Tag::kRawString:
      return raw_string_->string();
    case Tag::kHeapNumber:
      DCHECK(heap_number_->IsHeapNumber());
      return heap_number_->value();
    case Tag::kScope:
      return scope_->scope_info();
    case Tag::kIteratorSymbol:
      return isolate->factory()->iterator_symbol();
    case Tag::kHomeObjectSymbol:
      return isolate->factory()->home_object_symbol();
    case Tag::kEmptyFixedArray:
      return isolate->factory()->empty_fixed_array();
  }
  UNREACHABLE();
  return Handle<Object>::null();
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
