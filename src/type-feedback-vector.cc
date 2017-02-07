// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/type-feedback-vector.h"

#include "src/code-stubs.h"
#include "src/ic/ic-inl.h"
#include "src/ic/ic-state.h"
#include "src/objects.h"
#include "src/type-feedback-vector-inl.h"

namespace v8 {
namespace internal {


static bool IsPropertyNameFeedback(Object* feedback) {
  if (feedback->IsString()) return true;
  if (!feedback->IsSymbol()) return false;
  Symbol* symbol = Symbol::cast(feedback);
  Heap* heap = symbol->GetHeap();
  return symbol != heap->uninitialized_symbol() &&
         symbol != heap->premonomorphic_symbol() &&
         symbol != heap->megamorphic_symbol();
}


std::ostream& operator<<(std::ostream& os, FeedbackVectorSlotKind kind) {
  return os << TypeFeedbackMetadata::Kind2String(kind);
}


FeedbackVectorSlotKind TypeFeedbackMetadata::GetKind(
    FeedbackVectorSlot slot) const {
  int index = VectorICComputer::index(kReservedIndexCount, slot.ToInt());
  int data = Smi::cast(get(index))->value();
  return VectorICComputer::decode(data, slot.ToInt());
}

void TypeFeedbackMetadata::SetKind(FeedbackVectorSlot slot,
                                   FeedbackVectorSlotKind kind) {
  int index = VectorICComputer::index(kReservedIndexCount, slot.ToInt());
  int data = Smi::cast(get(index))->value();
  int new_data = VectorICComputer::encode(data, slot.ToInt(), kind);
  set(index, Smi::FromInt(new_data));
}


template Handle<TypeFeedbackMetadata> TypeFeedbackMetadata::New(
    Isolate* isolate, const StaticFeedbackVectorSpec* spec);
template Handle<TypeFeedbackMetadata> TypeFeedbackMetadata::New(
    Isolate* isolate, const FeedbackVectorSpec* spec);


// static
template <typename Spec>
Handle<TypeFeedbackMetadata> TypeFeedbackMetadata::New(Isolate* isolate,
                                                       const Spec* spec) {
  Factory* factory = isolate->factory();

  const int slot_count = spec->slots();
  const int slot_kinds_length = VectorICComputer::word_count(slot_count);
  const int length = slot_kinds_length + kReservedIndexCount;
  if (length == kReservedIndexCount) {
    return Handle<TypeFeedbackMetadata>::cast(factory->empty_fixed_array());
  }
#ifdef DEBUG
  for (int i = 0; i < slot_count;) {
    FeedbackVectorSlotKind kind = spec->GetKind(FeedbackVectorSlot(i));
    int entry_size = TypeFeedbackMetadata::GetSlotSize(kind);
    for (int j = 1; j < entry_size; j++) {
      FeedbackVectorSlotKind kind = spec->GetKind(FeedbackVectorSlot(i + j));
      DCHECK_EQ(FeedbackVectorSlotKind::INVALID, kind);
    }
    i += entry_size;
  }
#endif

  Handle<FixedArray> array = factory->NewFixedArray(length, TENURED);
  array->set(kSlotsCountIndex, Smi::FromInt(slot_count));
  // Fill the bit-vector part with zeros.
  for (int i = 0; i < slot_kinds_length; i++) {
    array->set(kReservedIndexCount + i, Smi::kZero);
  }

  Handle<TypeFeedbackMetadata> metadata =
      Handle<TypeFeedbackMetadata>::cast(array);

  for (int i = 0; i < slot_count; i++) {
    FeedbackVectorSlot slot(i);
    FeedbackVectorSlotKind kind = spec->GetKind(slot);
    metadata->SetKind(slot, kind);
  }

  // It's important that the TypeFeedbackMetadata have a COW map, since it's
  // pointed to by both a SharedFunctionInfo and indirectly by closures through
  // the TypeFeedbackVector. The serializer uses the COW map type to decide
  // this object belongs in the startup snapshot and not the partial
  // snapshot(s).
  metadata->set_map(isolate->heap()->fixed_cow_array_map());

  return metadata;
}

bool TypeFeedbackMetadata::SpecDiffersFrom(
    const FeedbackVectorSpec* other_spec) const {
  if (other_spec->slots() != slot_count()) {
    return true;
  }

  int slots = slot_count();
  for (int i = 0; i < slots;) {
    FeedbackVectorSlot slot(i);
    FeedbackVectorSlotKind kind = GetKind(slot);
    int entry_size = TypeFeedbackMetadata::GetSlotSize(kind);

    if (kind != other_spec->GetKind(slot)) {
      return true;
    }
    i += entry_size;
  }
  return false;
}

bool TypeFeedbackMetadata::DiffersFrom(
    const TypeFeedbackMetadata* other_metadata) const {
  if (other_metadata->slot_count() != slot_count()) {
    return true;
  }

  int slots = slot_count();
  for (int i = 0; i < slots;) {
    FeedbackVectorSlot slot(i);
    FeedbackVectorSlotKind kind = GetKind(slot);
    int entry_size = TypeFeedbackMetadata::GetSlotSize(kind);
    if (GetKind(slot) != other_metadata->GetKind(slot)) {
      return true;
    }
    i += entry_size;
  }
  return false;
}

const char* TypeFeedbackMetadata::Kind2String(FeedbackVectorSlotKind kind) {
  switch (kind) {
    case FeedbackVectorSlotKind::INVALID:
      return "INVALID";
    case FeedbackVectorSlotKind::CALL_IC:
      return "CALL_IC";
    case FeedbackVectorSlotKind::LOAD_IC:
      return "LOAD_IC";
    case FeedbackVectorSlotKind::LOAD_GLOBAL_INSIDE_TYPEOF_IC:
      return "LOAD_GLOBAL_INSIDE_TYPEOF_IC";
    case FeedbackVectorSlotKind::LOAD_GLOBAL_NOT_INSIDE_TYPEOF_IC:
      return "LOAD_GLOBAL_NOT_INSIDE_TYPEOF_IC";
    case FeedbackVectorSlotKind::KEYED_LOAD_IC:
      return "KEYED_LOAD_IC";
    case FeedbackVectorSlotKind::STORE_SLOPPY_IC:
      return "STORE_SLOPPY_IC";
    case FeedbackVectorSlotKind::STORE_STRICT_IC:
      return "STORE_STRICT_IC";
    case FeedbackVectorSlotKind::KEYED_STORE_SLOPPY_IC:
      return "KEYED_STORE_SLOPPY_IC";
    case FeedbackVectorSlotKind::KEYED_STORE_STRICT_IC:
      return "KEYED_STORE_STRICT_IC";
    case FeedbackVectorSlotKind::INTERPRETER_BINARYOP_IC:
      return "INTERPRETER_BINARYOP_IC";
    case FeedbackVectorSlotKind::INTERPRETER_COMPARE_IC:
      return "INTERPRETER_COMPARE_IC";
    case FeedbackVectorSlotKind::STORE_DATA_PROPERTY_IN_LITERAL_IC:
      return "STORE_DATA_PROPERTY_IN_LITERAL_IC";
    case FeedbackVectorSlotKind::CREATE_CLOSURE:
      return "CREATE_CLOSURE";
    case FeedbackVectorSlotKind::LITERAL:
      return "LITERAL";
    case FeedbackVectorSlotKind::GENERAL:
      return "STUB";
    case FeedbackVectorSlotKind::KINDS_NUMBER:
      break;
  }
  UNREACHABLE();
  return "?";
}

FeedbackVectorSlotKind TypeFeedbackVector::GetKind(
    FeedbackVectorSlot slot) const {
  DCHECK(!is_empty());
  return metadata()->GetKind(slot);
}

// static
Handle<TypeFeedbackVector> TypeFeedbackVector::New(
    Isolate* isolate, Handle<TypeFeedbackMetadata> metadata) {
  Factory* factory = isolate->factory();

  const int slot_count = metadata->slot_count();
  const int length = slot_count + kReservedIndexCount;
  if (length == kReservedIndexCount) {
    return Handle<TypeFeedbackVector>::cast(
        factory->empty_type_feedback_vector());
  }

  Handle<FixedArray> array = factory->NewFixedArray(length, TENURED);
  array->set_map_no_write_barrier(isolate->heap()->type_feedback_vector_map());
  array->set(kMetadataIndex, *metadata);
  array->set(kInvocationCountIndex, Smi::kZero);

  // Ensure we can skip the write barrier
  Handle<Object> uninitialized_sentinel = UninitializedSentinel(isolate);
  DCHECK_EQ(isolate->heap()->uninitialized_symbol(), *uninitialized_sentinel);
  Handle<Oddball> undefined_value = factory->undefined_value();
  for (int i = 0; i < slot_count;) {
    FeedbackVectorSlot slot(i);
    FeedbackVectorSlotKind kind = metadata->GetKind(slot);
    int index = TypeFeedbackVector::GetIndex(slot);
    int entry_size = TypeFeedbackMetadata::GetSlotSize(kind);

    Object* extra_value = *uninitialized_sentinel;
    switch (kind) {
      case FeedbackVectorSlotKind::LOAD_GLOBAL_INSIDE_TYPEOF_IC:
      case FeedbackVectorSlotKind::LOAD_GLOBAL_NOT_INSIDE_TYPEOF_IC:
        array->set(index, isolate->heap()->empty_weak_cell(),
                   SKIP_WRITE_BARRIER);
        break;
      case FeedbackVectorSlotKind::INTERPRETER_COMPARE_IC:
      case FeedbackVectorSlotKind::INTERPRETER_BINARYOP_IC:
        array->set(index, Smi::kZero, SKIP_WRITE_BARRIER);
        break;
      case FeedbackVectorSlotKind::CREATE_CLOSURE: {
        Handle<Cell> cell = factory->NewCell(undefined_value);
        array->set(index, *cell);
        break;
      }
      case FeedbackVectorSlotKind::LITERAL:
        array->set(index, *undefined_value, SKIP_WRITE_BARRIER);
        break;
      case FeedbackVectorSlotKind::CALL_IC:
        array->set(index, *uninitialized_sentinel, SKIP_WRITE_BARRIER);
        extra_value = Smi::kZero;
        break;
      case FeedbackVectorSlotKind::LOAD_IC:
      case FeedbackVectorSlotKind::KEYED_LOAD_IC:
      case FeedbackVectorSlotKind::STORE_SLOPPY_IC:
      case FeedbackVectorSlotKind::STORE_STRICT_IC:
      case FeedbackVectorSlotKind::KEYED_STORE_SLOPPY_IC:
      case FeedbackVectorSlotKind::KEYED_STORE_STRICT_IC:
      case FeedbackVectorSlotKind::STORE_DATA_PROPERTY_IN_LITERAL_IC:
      case FeedbackVectorSlotKind::GENERAL:
        array->set(index, *uninitialized_sentinel, SKIP_WRITE_BARRIER);
        break;

      case FeedbackVectorSlotKind::INVALID:
      case FeedbackVectorSlotKind::KINDS_NUMBER:
        UNREACHABLE();
        array->set(index, Smi::kZero, SKIP_WRITE_BARRIER);
        break;
    }
    for (int j = 1; j < entry_size; j++) {
      array->set(index + j, extra_value, SKIP_WRITE_BARRIER);
    }
    i += entry_size;
  }
  return Handle<TypeFeedbackVector>::cast(array);
}

// static
Handle<TypeFeedbackVector> TypeFeedbackVector::Copy(
    Isolate* isolate, Handle<TypeFeedbackVector> vector) {
  Handle<TypeFeedbackVector> result;
  result = Handle<TypeFeedbackVector>::cast(
      isolate->factory()->CopyFixedArray(Handle<FixedArray>::cast(vector)));
  return result;
}

// This logic is copied from
// StaticMarkingVisitor<StaticVisitor>::VisitCodeTarget.
static bool ClearLogic(Isolate* isolate) {
  return FLAG_cleanup_code_caches_at_gc && isolate->serializer_enabled();
}

void TypeFeedbackVector::ClearSlotsImpl(SharedFunctionInfo* shared,
                                        bool force_clear) {
  Isolate* isolate = GetIsolate();
  if (!force_clear && !ClearLogic(isolate)) return;

  if (this == isolate->heap()->empty_type_feedback_vector()) return;

  Object* uninitialized_sentinel =
      TypeFeedbackVector::RawUninitializedSentinel(isolate);
  Oddball* undefined_value = isolate->heap()->undefined_value();

  TypeFeedbackMetadataIterator iter(metadata());
  while (iter.HasNext()) {
    FeedbackVectorSlot slot = iter.Next();
    FeedbackVectorSlotKind kind = iter.kind();

    Object* obj = Get(slot);
    if (obj != uninitialized_sentinel) {
      switch (kind) {
        case FeedbackVectorSlotKind::CALL_IC: {
          CallICNexus nexus(this, slot);
          nexus.Clear(shared->code());
          break;
        }
        case FeedbackVectorSlotKind::LOAD_IC: {
          LoadICNexus nexus(this, slot);
          nexus.Clear(shared->code());
          break;
        }
        case FeedbackVectorSlotKind::LOAD_GLOBAL_INSIDE_TYPEOF_IC:
        case FeedbackVectorSlotKind::LOAD_GLOBAL_NOT_INSIDE_TYPEOF_IC: {
          LoadGlobalICNexus nexus(this, slot);
          nexus.Clear(shared->code());
          break;
        }
        case FeedbackVectorSlotKind::KEYED_LOAD_IC: {
          KeyedLoadICNexus nexus(this, slot);
          nexus.Clear(shared->code());
          break;
        }
        case FeedbackVectorSlotKind::STORE_SLOPPY_IC:
        case FeedbackVectorSlotKind::STORE_STRICT_IC: {
          StoreICNexus nexus(this, slot);
          nexus.Clear(shared->code());
          break;
        }
        case FeedbackVectorSlotKind::KEYED_STORE_SLOPPY_IC:
        case FeedbackVectorSlotKind::KEYED_STORE_STRICT_IC: {
          KeyedStoreICNexus nexus(this, slot);
          nexus.Clear(shared->code());
          break;
        }
        case FeedbackVectorSlotKind::INTERPRETER_BINARYOP_IC:
        case FeedbackVectorSlotKind::INTERPRETER_COMPARE_IC: {
          DCHECK(Get(slot)->IsSmi());
          // don't clear these smi slots.
          // Set(slot, Smi::kZero);
          break;
        }
        case FeedbackVectorSlotKind::CREATE_CLOSURE: {
          break;
        }
        case FeedbackVectorSlotKind::GENERAL: {
          if (obj->IsHeapObject()) {
            InstanceType instance_type =
                HeapObject::cast(obj)->map()->instance_type();
            // AllocationSites are exempt from clearing. They don't store Maps
            // or Code pointers which can cause memory leaks if not cleared
            // regularly.
            if (instance_type != ALLOCATION_SITE_TYPE) {
              Set(slot, uninitialized_sentinel, SKIP_WRITE_BARRIER);
            }
          }
          break;
        }
        case FeedbackVectorSlotKind::LITERAL: {
          Set(slot, undefined_value, SKIP_WRITE_BARRIER);
          break;
        }
        case FeedbackVectorSlotKind::STORE_DATA_PROPERTY_IN_LITERAL_IC: {
          StoreDataPropertyInLiteralICNexus nexus(this, slot);
          nexus.Clear(shared->code());
          break;
        }
        case FeedbackVectorSlotKind::INVALID:
        case FeedbackVectorSlotKind::KINDS_NUMBER:
          UNREACHABLE();
          break;
      }
    }
  }
}


Handle<FixedArray> FeedbackNexus::EnsureArrayOfSize(int length) {
  Isolate* isolate = GetIsolate();
  Handle<Object> feedback = handle(GetFeedback(), isolate);
  if (!feedback->IsFixedArray() ||
      FixedArray::cast(*feedback)->length() != length) {
    Handle<FixedArray> array = isolate->factory()->NewFixedArray(length);
    SetFeedback(*array);
    return array;
  }
  return Handle<FixedArray>::cast(feedback);
}


Handle<FixedArray> FeedbackNexus::EnsureExtraArrayOfSize(int length) {
  Isolate* isolate = GetIsolate();
  Handle<Object> feedback_extra = handle(GetFeedbackExtra(), isolate);
  if (!feedback_extra->IsFixedArray() ||
      FixedArray::cast(*feedback_extra)->length() != length) {
    Handle<FixedArray> array = isolate->factory()->NewFixedArray(length);
    SetFeedbackExtra(*array);
    return array;
  }
  return Handle<FixedArray>::cast(feedback_extra);
}

void FeedbackNexus::InstallHandlers(Handle<FixedArray> array,
                                    MapHandleList* maps,
                                    List<Handle<Object>>* handlers) {
  int receiver_count = maps->length();
  for (int current = 0; current < receiver_count; ++current) {
    Handle<Map> map = maps->at(current);
    Handle<WeakCell> cell = Map::WeakCellForMap(map);
    array->set(current * 2, *cell);
    array->set(current * 2 + 1, *handlers->at(current));
  }
}


void FeedbackNexus::ConfigureUninitialized() {
  SetFeedback(*TypeFeedbackVector::UninitializedSentinel(GetIsolate()),
              SKIP_WRITE_BARRIER);
  SetFeedbackExtra(*TypeFeedbackVector::UninitializedSentinel(GetIsolate()),
                   SKIP_WRITE_BARRIER);
}


void FeedbackNexus::ConfigurePremonomorphic() {
  SetFeedback(*TypeFeedbackVector::PremonomorphicSentinel(GetIsolate()),
              SKIP_WRITE_BARRIER);
  SetFeedbackExtra(*TypeFeedbackVector::UninitializedSentinel(GetIsolate()),
                   SKIP_WRITE_BARRIER);
}


void FeedbackNexus::ConfigureMegamorphic() {
  // Keyed ICs must use ConfigureMegamorphicKeyed.
  DCHECK(!vector()->IsKeyedLoadIC(slot()));
  DCHECK(!vector()->IsKeyedStoreIC(slot()));

  Isolate* isolate = GetIsolate();
  SetFeedback(*TypeFeedbackVector::MegamorphicSentinel(isolate),
              SKIP_WRITE_BARRIER);
  SetFeedbackExtra(*TypeFeedbackVector::UninitializedSentinel(isolate),
                   SKIP_WRITE_BARRIER);
}

void KeyedLoadICNexus::ConfigureMegamorphicKeyed(IcCheckType property_type) {
  Isolate* isolate = GetIsolate();
  SetFeedback(*TypeFeedbackVector::MegamorphicSentinel(isolate),
              SKIP_WRITE_BARRIER);
  SetFeedbackExtra(Smi::FromInt(static_cast<int>(property_type)),
                   SKIP_WRITE_BARRIER);
}

void KeyedStoreICNexus::ConfigureMegamorphicKeyed(IcCheckType property_type) {
  Isolate* isolate = GetIsolate();
  SetFeedback(*TypeFeedbackVector::MegamorphicSentinel(isolate),
              SKIP_WRITE_BARRIER);
  SetFeedbackExtra(Smi::FromInt(static_cast<int>(property_type)),
                   SKIP_WRITE_BARRIER);
}

InlineCacheState LoadICNexus::StateFromFeedback() const {
  Isolate* isolate = GetIsolate();
  Object* feedback = GetFeedback();

  if (feedback == *TypeFeedbackVector::UninitializedSentinel(isolate)) {
    return UNINITIALIZED;
  } else if (feedback == *TypeFeedbackVector::MegamorphicSentinel(isolate)) {
    return MEGAMORPHIC;
  } else if (feedback == *TypeFeedbackVector::PremonomorphicSentinel(isolate)) {
    return PREMONOMORPHIC;
  } else if (feedback->IsFixedArray()) {
    // Determine state purely by our structure, don't check if the maps are
    // cleared.
    return POLYMORPHIC;
  } else if (feedback->IsWeakCell()) {
    // Don't check if the map is cleared.
    return MONOMORPHIC;
  }

  return UNINITIALIZED;
}

InlineCacheState LoadGlobalICNexus::StateFromFeedback() const {
  Isolate* isolate = GetIsolate();
  Object* feedback = GetFeedback();

  Object* extra = GetFeedbackExtra();
  if (!WeakCell::cast(feedback)->cleared() ||
      extra != *TypeFeedbackVector::UninitializedSentinel(isolate)) {
    return MONOMORPHIC;
  }
  return UNINITIALIZED;
}

InlineCacheState KeyedLoadICNexus::StateFromFeedback() const {
  Isolate* isolate = GetIsolate();
  Object* feedback = GetFeedback();

  if (feedback == *TypeFeedbackVector::UninitializedSentinel(isolate)) {
    return UNINITIALIZED;
  } else if (feedback == *TypeFeedbackVector::PremonomorphicSentinel(isolate)) {
    return PREMONOMORPHIC;
  } else if (feedback == *TypeFeedbackVector::MegamorphicSentinel(isolate)) {
    return MEGAMORPHIC;
  } else if (feedback->IsFixedArray()) {
    // Determine state purely by our structure, don't check if the maps are
    // cleared.
    return POLYMORPHIC;
  } else if (feedback->IsWeakCell()) {
    // Don't check if the map is cleared.
    return MONOMORPHIC;
  } else if (feedback->IsName()) {
    Object* extra = GetFeedbackExtra();
    FixedArray* extra_array = FixedArray::cast(extra);
    return extra_array->length() > 2 ? POLYMORPHIC : MONOMORPHIC;
  }

  return UNINITIALIZED;
}


InlineCacheState StoreICNexus::StateFromFeedback() const {
  Isolate* isolate = GetIsolate();
  Object* feedback = GetFeedback();

  if (feedback == *TypeFeedbackVector::UninitializedSentinel(isolate)) {
    return UNINITIALIZED;
  } else if (feedback == *TypeFeedbackVector::MegamorphicSentinel(isolate)) {
    return MEGAMORPHIC;
  } else if (feedback == *TypeFeedbackVector::PremonomorphicSentinel(isolate)) {
    return PREMONOMORPHIC;
  } else if (feedback->IsFixedArray()) {
    // Determine state purely by our structure, don't check if the maps are
    // cleared.
    return POLYMORPHIC;
  } else if (feedback->IsWeakCell()) {
    // Don't check if the map is cleared.
    return MONOMORPHIC;
  }

  return UNINITIALIZED;
}


InlineCacheState KeyedStoreICNexus::StateFromFeedback() const {
  Isolate* isolate = GetIsolate();
  Object* feedback = GetFeedback();

  if (feedback == *TypeFeedbackVector::UninitializedSentinel(isolate)) {
    return UNINITIALIZED;
  } else if (feedback == *TypeFeedbackVector::PremonomorphicSentinel(isolate)) {
    return PREMONOMORPHIC;
  } else if (feedback == *TypeFeedbackVector::MegamorphicSentinel(isolate)) {
    return MEGAMORPHIC;
  } else if (feedback->IsFixedArray()) {
    // Determine state purely by our structure, don't check if the maps are
    // cleared.
    return POLYMORPHIC;
  } else if (feedback->IsWeakCell()) {
    // Don't check if the map is cleared.
    return MONOMORPHIC;
  } else if (feedback->IsName()) {
    Object* extra = GetFeedbackExtra();
    FixedArray* extra_array = FixedArray::cast(extra);
    return extra_array->length() > 2 ? POLYMORPHIC : MONOMORPHIC;
  }

  return UNINITIALIZED;
}


InlineCacheState CallICNexus::StateFromFeedback() const {
  Isolate* isolate = GetIsolate();
  Object* feedback = GetFeedback();
  DCHECK(GetFeedbackExtra() ==
             *TypeFeedbackVector::UninitializedSentinel(isolate) ||
         GetFeedbackExtra()->IsSmi());

  if (feedback == *TypeFeedbackVector::MegamorphicSentinel(isolate)) {
    return GENERIC;
  } else if (feedback->IsAllocationSite() || feedback->IsWeakCell()) {
    return MONOMORPHIC;
  }

  CHECK(feedback == *TypeFeedbackVector::UninitializedSentinel(isolate));
  return UNINITIALIZED;
}


int CallICNexus::ExtractCallCount() {
  Object* call_count = GetFeedbackExtra();
  CHECK(call_count->IsSmi());
  int value = Smi::cast(call_count)->value();
  return value;
}

float CallICNexus::ComputeCallFrequency() {
  double const invocation_count = vector()->invocation_count();
  double const call_count = ExtractCallCount();
  return static_cast<float>(call_count / invocation_count);
}

void CallICNexus::Clear(Code* host) { CallIC::Clear(GetIsolate(), host, this); }

void CallICNexus::ConfigureUninitialized() {
  Isolate* isolate = GetIsolate();
  SetFeedback(*TypeFeedbackVector::UninitializedSentinel(isolate),
              SKIP_WRITE_BARRIER);
  SetFeedbackExtra(Smi::kZero, SKIP_WRITE_BARRIER);
}

void CallICNexus::ConfigureMonomorphicArray() {
  Object* feedback = GetFeedback();
  if (!feedback->IsAllocationSite()) {
    Handle<AllocationSite> new_site =
        GetIsolate()->factory()->NewAllocationSite();
    SetFeedback(*new_site);
  }
  SetFeedbackExtra(Smi::FromInt(1), SKIP_WRITE_BARRIER);
}


void CallICNexus::ConfigureMonomorphic(Handle<JSFunction> function) {
  Handle<WeakCell> new_cell = GetIsolate()->factory()->NewWeakCell(function);
  SetFeedback(*new_cell);
  SetFeedbackExtra(Smi::FromInt(1), SKIP_WRITE_BARRIER);
}


void CallICNexus::ConfigureMegamorphic() {
  SetFeedback(*TypeFeedbackVector::MegamorphicSentinel(GetIsolate()),
              SKIP_WRITE_BARRIER);
  Smi* count = Smi::cast(GetFeedbackExtra());
  int new_count = count->value() + 1;
  SetFeedbackExtra(Smi::FromInt(new_count), SKIP_WRITE_BARRIER);
}

void CallICNexus::ConfigureMegamorphic(int call_count) {
  SetFeedback(*TypeFeedbackVector::MegamorphicSentinel(GetIsolate()),
              SKIP_WRITE_BARRIER);
  SetFeedbackExtra(Smi::FromInt(call_count), SKIP_WRITE_BARRIER);
}

void LoadICNexus::ConfigureMonomorphic(Handle<Map> receiver_map,
                                       Handle<Object> handler) {
  Handle<WeakCell> cell = Map::WeakCellForMap(receiver_map);
  SetFeedback(*cell);
  SetFeedbackExtra(*handler);
}

void LoadGlobalICNexus::ConfigureUninitialized() {
  Isolate* isolate = GetIsolate();
  SetFeedback(isolate->heap()->empty_weak_cell(), SKIP_WRITE_BARRIER);
  SetFeedbackExtra(*TypeFeedbackVector::UninitializedSentinel(isolate),
                   SKIP_WRITE_BARRIER);
}

void LoadGlobalICNexus::ConfigurePropertyCellMode(Handle<PropertyCell> cell) {
  Isolate* isolate = GetIsolate();
  SetFeedback(*isolate->factory()->NewWeakCell(cell));
  SetFeedbackExtra(*TypeFeedbackVector::UninitializedSentinel(isolate),
                   SKIP_WRITE_BARRIER);
}

void LoadGlobalICNexus::ConfigureHandlerMode(Handle<Object> handler) {
  SetFeedback(GetIsolate()->heap()->empty_weak_cell());
  SetFeedbackExtra(*handler);
}

void KeyedLoadICNexus::ConfigureMonomorphic(Handle<Name> name,
                                            Handle<Map> receiver_map,
                                            Handle<Object> handler) {
  Handle<WeakCell> cell = Map::WeakCellForMap(receiver_map);
  if (name.is_null()) {
    SetFeedback(*cell);
    SetFeedbackExtra(*handler);
  } else {
    Handle<FixedArray> array = EnsureExtraArrayOfSize(2);
    SetFeedback(*name);
    array->set(0, *cell);
    array->set(1, *handler);
  }
}

void StoreICNexus::ConfigureMonomorphic(Handle<Map> receiver_map,
                                        Handle<Object> handler) {
  Handle<WeakCell> cell = Map::WeakCellForMap(receiver_map);
  SetFeedback(*cell);
  SetFeedbackExtra(*handler);
}

void KeyedStoreICNexus::ConfigureMonomorphic(Handle<Name> name,
                                             Handle<Map> receiver_map,
                                             Handle<Object> handler) {
  Handle<WeakCell> cell = Map::WeakCellForMap(receiver_map);
  if (name.is_null()) {
    SetFeedback(*cell);
    SetFeedbackExtra(*handler);
  } else {
    Handle<FixedArray> array = EnsureExtraArrayOfSize(2);
    SetFeedback(*name);
    array->set(0, *cell);
    array->set(1, *handler);
  }
}

void LoadICNexus::ConfigurePolymorphic(MapHandleList* maps,
                                       List<Handle<Object>>* handlers) {
  Isolate* isolate = GetIsolate();
  int receiver_count = maps->length();
  Handle<FixedArray> array = EnsureArrayOfSize(receiver_count * 2);
  InstallHandlers(array, maps, handlers);
  SetFeedbackExtra(*TypeFeedbackVector::UninitializedSentinel(isolate),
                   SKIP_WRITE_BARRIER);
}

void KeyedLoadICNexus::ConfigurePolymorphic(Handle<Name> name,
                                            MapHandleList* maps,
                                            List<Handle<Object>>* handlers) {
  int receiver_count = maps->length();
  DCHECK(receiver_count > 1);
  Handle<FixedArray> array;
  if (name.is_null()) {
    array = EnsureArrayOfSize(receiver_count * 2);
    SetFeedbackExtra(*TypeFeedbackVector::UninitializedSentinel(GetIsolate()),
                     SKIP_WRITE_BARRIER);
  } else {
    array = EnsureExtraArrayOfSize(receiver_count * 2);
    SetFeedback(*name);
  }

  InstallHandlers(array, maps, handlers);
}

void StoreICNexus::ConfigurePolymorphic(MapHandleList* maps,
                                        List<Handle<Object>>* handlers) {
  Isolate* isolate = GetIsolate();
  int receiver_count = maps->length();
  Handle<FixedArray> array = EnsureArrayOfSize(receiver_count * 2);
  InstallHandlers(array, maps, handlers);
  SetFeedbackExtra(*TypeFeedbackVector::UninitializedSentinel(isolate),
                   SKIP_WRITE_BARRIER);
}

void KeyedStoreICNexus::ConfigurePolymorphic(Handle<Name> name,
                                             MapHandleList* maps,
                                             List<Handle<Object>>* handlers) {
  int receiver_count = maps->length();
  DCHECK(receiver_count > 1);
  Handle<FixedArray> array;
  if (name.is_null()) {
    array = EnsureArrayOfSize(receiver_count * 2);
    SetFeedbackExtra(*TypeFeedbackVector::UninitializedSentinel(GetIsolate()),
                     SKIP_WRITE_BARRIER);
  } else {
    array = EnsureExtraArrayOfSize(receiver_count * 2);
    SetFeedback(*name);
  }

  InstallHandlers(array, maps, handlers);
}

void KeyedStoreICNexus::ConfigurePolymorphic(MapHandleList* maps,
                                             MapHandleList* transitioned_maps,
                                             List<Handle<Object>>* handlers) {
  int receiver_count = maps->length();
  DCHECK(receiver_count > 1);
  Handle<FixedArray> array = EnsureArrayOfSize(receiver_count * 3);
  SetFeedbackExtra(*TypeFeedbackVector::UninitializedSentinel(GetIsolate()),
                   SKIP_WRITE_BARRIER);

  Handle<Oddball> undefined_value = GetIsolate()->factory()->undefined_value();
  for (int i = 0; i < receiver_count; ++i) {
    Handle<Map> map = maps->at(i);
    Handle<WeakCell> cell = Map::WeakCellForMap(map);
    array->set(i * 3, *cell);
    if (!transitioned_maps->at(i).is_null()) {
      Handle<Map> transitioned_map = transitioned_maps->at(i);
      cell = Map::WeakCellForMap(transitioned_map);
      array->set((i * 3) + 1, *cell);
    } else {
      array->set((i * 3) + 1, *undefined_value);
    }
    array->set((i * 3) + 2, *handlers->at(i));
  }
}

namespace {

int GetStepSize(FixedArray* array, Isolate* isolate) {
  // The array should be of the form
  // [map, handler, map, handler, ...]
  // or
  // [map, map, handler, map, map, handler, ...]
  // where "map" is either a WeakCell or |undefined|,
  // and "handler" is either a Code object or a Smi.
  DCHECK(array->length() >= 2);
  Object* second = array->get(1);
  if (second->IsWeakCell() || second->IsUndefined(isolate)) return 3;
  DCHECK(IC::IsHandler(second));
  return 2;
}

}  // namespace

int FeedbackNexus::ExtractMaps(MapHandleList* maps) const {
  Isolate* isolate = GetIsolate();
  Object* feedback = GetFeedback();
  bool is_named_feedback = IsPropertyNameFeedback(feedback);
  if (feedback->IsFixedArray() || is_named_feedback) {
    int found = 0;
    if (is_named_feedback) {
      feedback = GetFeedbackExtra();
    }
    FixedArray* array = FixedArray::cast(feedback);
    int increment = GetStepSize(array, isolate);
    for (int i = 0; i < array->length(); i += increment) {
      DCHECK(array->get(i)->IsWeakCell());
      WeakCell* cell = WeakCell::cast(array->get(i));
      if (!cell->cleared()) {
        Map* map = Map::cast(cell->value());
        maps->Add(handle(map, isolate));
        found++;
      }
    }
    return found;
  } else if (feedback->IsWeakCell()) {
    WeakCell* cell = WeakCell::cast(feedback);
    if (!cell->cleared()) {
      Map* map = Map::cast(cell->value());
      maps->Add(handle(map, isolate));
      return 1;
    }
  }

  return 0;
}

MaybeHandle<Object> FeedbackNexus::FindHandlerForMap(Handle<Map> map) const {
  Object* feedback = GetFeedback();
  Isolate* isolate = GetIsolate();
  bool is_named_feedback = IsPropertyNameFeedback(feedback);
  if (feedback->IsFixedArray() || is_named_feedback) {
    if (is_named_feedback) {
      feedback = GetFeedbackExtra();
    }
    FixedArray* array = FixedArray::cast(feedback);
    int increment = GetStepSize(array, isolate);
    for (int i = 0; i < array->length(); i += increment) {
      DCHECK(array->get(i)->IsWeakCell());
      WeakCell* cell = WeakCell::cast(array->get(i));
      if (!cell->cleared()) {
        Map* array_map = Map::cast(cell->value());
        if (array_map == *map) {
          Object* code = array->get(i + increment - 1);
          DCHECK(IC::IsHandler(code));
          return handle(code, isolate);
        }
      }
    }
  } else if (feedback->IsWeakCell()) {
    WeakCell* cell = WeakCell::cast(feedback);
    if (!cell->cleared()) {
      Map* cell_map = Map::cast(cell->value());
      if (cell_map == *map) {
        Object* code = GetFeedbackExtra();
        DCHECK(IC::IsHandler(code));
        return handle(code, isolate);
      }
    }
  }

  return MaybeHandle<Code>();
}

bool FeedbackNexus::FindHandlers(List<Handle<Object>>* code_list,
                                 int length) const {
  Object* feedback = GetFeedback();
  Isolate* isolate = GetIsolate();
  int count = 0;
  bool is_named_feedback = IsPropertyNameFeedback(feedback);
  if (feedback->IsFixedArray() || is_named_feedback) {
    if (is_named_feedback) {
      feedback = GetFeedbackExtra();
    }
    FixedArray* array = FixedArray::cast(feedback);
    int increment = GetStepSize(array, isolate);
    for (int i = 0; i < array->length(); i += increment) {
      DCHECK(array->get(i)->IsWeakCell());
      WeakCell* cell = WeakCell::cast(array->get(i));
      // Be sure to skip handlers whose maps have been cleared.
      if (!cell->cleared()) {
        Object* code = array->get(i + increment - 1);
        DCHECK(IC::IsHandler(code));
        code_list->Add(handle(code, isolate));
        count++;
      }
    }
  } else if (feedback->IsWeakCell()) {
    WeakCell* cell = WeakCell::cast(feedback);
    if (!cell->cleared()) {
      Object* code = GetFeedbackExtra();
      DCHECK(IC::IsHandler(code));
      code_list->Add(handle(code, isolate));
      count++;
    }
  }
  return count == length;
}


void LoadICNexus::Clear(Code* host) { LoadIC::Clear(GetIsolate(), host, this); }

void LoadGlobalICNexus::Clear(Code* host) {
  LoadGlobalIC::Clear(GetIsolate(), host, this);
}

void KeyedLoadICNexus::Clear(Code* host) {
  KeyedLoadIC::Clear(GetIsolate(), host, this);
}


Name* KeyedLoadICNexus::FindFirstName() const {
  Object* feedback = GetFeedback();
  if (IsPropertyNameFeedback(feedback)) {
    return Name::cast(feedback);
  }
  return NULL;
}


Name* KeyedStoreICNexus::FindFirstName() const {
  Object* feedback = GetFeedback();
  if (IsPropertyNameFeedback(feedback)) {
    return Name::cast(feedback);
  }
  return NULL;
}


void StoreICNexus::Clear(Code* host) {
  StoreIC::Clear(GetIsolate(), host, this);
}


void KeyedStoreICNexus::Clear(Code* host) {
  KeyedStoreIC::Clear(GetIsolate(), host, this);
}


KeyedAccessStoreMode KeyedStoreICNexus::GetKeyedAccessStoreMode() const {
  KeyedAccessStoreMode mode = STANDARD_STORE;
  MapHandleList maps;
  List<Handle<Object>> handlers;

  if (GetKeyType() == PROPERTY) return mode;

  ExtractMaps(&maps);
  FindHandlers(&handlers, maps.length());
  for (int i = 0; i < handlers.length(); i++) {
    // The first handler that isn't the slow handler will have the bits we need.
    Handle<Object> maybe_code_handler = handlers.at(i);
    Handle<Code> handler;
    if (maybe_code_handler->IsTuple2()) {
      Handle<Tuple2> data_handler = Handle<Tuple2>::cast(maybe_code_handler);
      handler = handle(Code::cast(data_handler->value2()));
    } else {
      handler = Handle<Code>::cast(maybe_code_handler);
    }
    CodeStub::Major major_key = CodeStub::MajorKeyFromKey(handler->stub_key());
    uint32_t minor_key = CodeStub::MinorKeyFromKey(handler->stub_key());
    CHECK(major_key == CodeStub::KeyedStoreSloppyArguments ||
          major_key == CodeStub::StoreFastElement ||
          major_key == CodeStub::StoreSlowElement ||
          major_key == CodeStub::ElementsTransitionAndStore ||
          major_key == CodeStub::NoCache);
    if (major_key != CodeStub::NoCache) {
      mode = CommonStoreModeBits::decode(minor_key);
      break;
    }
  }

  return mode;
}

IcCheckType KeyedLoadICNexus::GetKeyType() const {
  Object* feedback = GetFeedback();
  if (feedback == *TypeFeedbackVector::MegamorphicSentinel(GetIsolate())) {
    return static_cast<IcCheckType>(Smi::cast(GetFeedbackExtra())->value());
  }
  return IsPropertyNameFeedback(feedback) ? PROPERTY : ELEMENT;
}

IcCheckType KeyedStoreICNexus::GetKeyType() const {
  Object* feedback = GetFeedback();
  if (feedback == *TypeFeedbackVector::MegamorphicSentinel(GetIsolate())) {
    return static_cast<IcCheckType>(Smi::cast(GetFeedbackExtra())->value());
  }
  return IsPropertyNameFeedback(feedback) ? PROPERTY : ELEMENT;
}

InlineCacheState BinaryOpICNexus::StateFromFeedback() const {
  BinaryOperationHint hint = GetBinaryOperationFeedback();
  if (hint == BinaryOperationHint::kNone) {
    return UNINITIALIZED;
  } else if (hint == BinaryOperationHint::kAny) {
    return GENERIC;
  }

  return MONOMORPHIC;
}

InlineCacheState CompareICNexus::StateFromFeedback() const {
  CompareOperationHint hint = GetCompareOperationFeedback();
  if (hint == CompareOperationHint::kNone) {
    return UNINITIALIZED;
  } else if (hint == CompareOperationHint::kAny) {
    return GENERIC;
  }

  return MONOMORPHIC;
}

BinaryOperationHint BinaryOpICNexus::GetBinaryOperationFeedback() const {
  int feedback = Smi::cast(GetFeedback())->value();
  return BinaryOperationHintFromFeedback(feedback);
}

CompareOperationHint CompareICNexus::GetCompareOperationFeedback() const {
  int feedback = Smi::cast(GetFeedback())->value();
  return CompareOperationHintFromFeedback(feedback);
}

InlineCacheState StoreDataPropertyInLiteralICNexus::StateFromFeedback() const {
  Isolate* isolate = GetIsolate();
  Object* feedback = GetFeedback();

  if (feedback == *TypeFeedbackVector::UninitializedSentinel(isolate)) {
    return UNINITIALIZED;
  } else if (feedback->IsWeakCell()) {
    // Don't check if the map is cleared.
    return MONOMORPHIC;
  }

  return MEGAMORPHIC;
}

void StoreDataPropertyInLiteralICNexus::ConfigureMonomorphic(
    Handle<Name> name, Handle<Map> receiver_map) {
  Handle<WeakCell> cell = Map::WeakCellForMap(receiver_map);

  SetFeedback(*cell);
  SetFeedbackExtra(*name);
}

}  // namespace internal
}  // namespace v8
