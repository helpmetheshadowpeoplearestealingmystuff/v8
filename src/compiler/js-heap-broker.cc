// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-heap-broker.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

HeapObjectRef::HeapObjectRef(Handle<Object> object) : ObjectRef(object) {
  AllowHandleDereference handle_dereference;
  SLOW_DCHECK(object->IsHeapObject());
}

JSFunctionRef::JSFunctionRef(Handle<Object> object) : HeapObjectRef(object) {
  AllowHandleDereference handle_dereference;
  SLOW_DCHECK(object->IsJSFunction());
}

HeapNumberRef::HeapNumberRef(Handle<Object> object) : HeapObjectRef(object) {
  AllowHandleDereference handle_dereference;
  SLOW_DCHECK(object->IsHeapNumber());
}

ContextRef::ContextRef(Handle<Object> object) : HeapObjectRef(object) {
  AllowHandleDereference handle_dereference;
  SLOW_DCHECK(object->IsContext());
}

bool ObjectRef::IsSmi() const {
  AllowHandleDereference allow_handle_dereference;
  return object_->IsSmi();
}

int ObjectRef::AsSmi() const { return object<Smi>()->value(); }

HeapObjectRef ObjectRef::AsHeapObjectRef() const {
  return HeapObjectRef(object<HeapObject>());
}

base::Optional<ContextRef> ContextRef::previous(
    const JSHeapBroker* broker) const {
  AllowHandleAllocation handle_allocation;
  AllowHandleDereference handle_dereference;
  Context* previous = object<Context>()->previous();
  if (previous == nullptr) return base::Optional<ContextRef>();
  return ContextRef(handle(previous, broker->isolate()));
}

base::Optional<ObjectRef> ContextRef::get(const JSHeapBroker* broker,
                                          int index) const {
  AllowHandleAllocation handle_allocation;
  AllowHandleDereference handle_dereference;
  Handle<Object> value(object<Context>()->get(index), broker->isolate());
  return ObjectRef(value);
}

JSHeapBroker::JSHeapBroker(Isolate* isolate) : isolate_(isolate) {}

HeapObjectType JSHeapBroker::HeapObjectTypeFromMap(Map* map) const {
  AllowHandleDereference allow_handle_dereference;
  Heap* heap = isolate_->heap();
  HeapObjectType::OddballType oddball_type = HeapObjectType::kNone;
  if (map->instance_type() == ODDBALL_TYPE) {
    if (map == heap->undefined_map()) {
      oddball_type = HeapObjectType::kUndefined;
    } else if (map == heap->null_map()) {
      oddball_type = HeapObjectType::kNull;
    } else if (map == heap->boolean_map()) {
      oddball_type = HeapObjectType::kBoolean;
    } else if (map == heap->the_hole_map()) {
      oddball_type = HeapObjectType::kHole;
    } else {
      oddball_type = HeapObjectType::kOther;
      DCHECK(map == heap->uninitialized_map() ||
             map == heap->termination_exception_map() ||
             map == heap->arguments_marker_map() ||
             map == heap->optimized_out_map() ||
             map == heap->stale_register_map());
    }
  }
  HeapObjectType::Flags flags(0);
  if (map->is_undetectable()) flags |= HeapObjectType::kUndetectable;
  if (map->is_callable()) flags |= HeapObjectType::kCallable;

  return HeapObjectType(map->instance_type(), flags, oddball_type);
}

// static
base::Optional<int> JSHeapBroker::TryGetSmi(Handle<Object> object) {
  AllowHandleDereference allow_handle_dereference;
  if (!object->IsSmi()) return base::Optional<int>();
  return Smi::cast(*object)->value();
}

#define HEAP_KIND_FUNCTIONS_DEF(Name)                \
  bool ObjectRef::Is##Name() const {                 \
    AllowHandleDereference allow_handle_dereference; \
    return object<Object>()->Is##Name();             \
  }
HEAP_BROKER_KIND_LIST(HEAP_KIND_FUNCTIONS_DEF)
#undef HEAP_KIND_FUNCTIONS_DEF

#define HEAP_DATA_FUNCTIONS_DEF(Name)       \
  Name##Ref ObjectRef::As##Name() const {   \
    SLOW_DCHECK(Is##Name());                \
    return Name##Ref(object<HeapObject>()); \
  }
HEAP_BROKER_DATA_LIST(HEAP_DATA_FUNCTIONS_DEF)
#undef HEAP_DATA_FUNCTIONS_DEF

HeapObjectType HeapObjectRef::type(const JSHeapBroker* broker) const {
  AllowHandleDereference allow_handle_dereference;
  return broker->HeapObjectTypeFromMap(object<HeapObject>()->map());
}

double HeapNumberRef::value() const {
  AllowHandleDereference allow_handle_dereference;
  return object<HeapObject>()->Number();
}

bool JSFunctionRef::HasBuiltinFunctionId() const {
  AllowHandleDereference allow_handle_dereference;
  return object<JSFunction>()->shared()->HasBuiltinFunctionId();
}

BuiltinFunctionId JSFunctionRef::GetBuiltinFunctionId() const {
  AllowHandleDereference allow_handle_dereference;
  return object<JSFunction>()->shared()->builtin_function_id();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
