// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/arguments.h"
#include "src/runtime/runtime-utils.h"


namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_SetInitialize) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSSet, holder, 0);
  Handle<OrderedHashSet> table = isolate->factory()->NewOrderedHashSet();
  holder->set_table(*table);
  return *holder;
}


RUNTIME_FUNCTION(Runtime_SetAdd) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSSet, holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  Handle<OrderedHashSet> table(OrderedHashSet::cast(holder->table()));
  table = OrderedHashSet::Add(table, key);
  holder->set_table(*table);
  return *holder;
}


RUNTIME_FUNCTION(Runtime_SetHas) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSSet, holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  Handle<OrderedHashSet> table(OrderedHashSet::cast(holder->table()));
  return isolate->heap()->ToBoolean(table->Contains(key));
}


RUNTIME_FUNCTION(Runtime_SetDelete) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSSet, holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  Handle<OrderedHashSet> table(OrderedHashSet::cast(holder->table()));
  bool was_present = false;
  table = OrderedHashSet::Remove(table, key, &was_present);
  holder->set_table(*table);
  return isolate->heap()->ToBoolean(was_present);
}


RUNTIME_FUNCTION(Runtime_SetClear) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSSet, holder, 0);
  Handle<OrderedHashSet> table(OrderedHashSet::cast(holder->table()));
  table = OrderedHashSet::Clear(table);
  holder->set_table(*table);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_SetGetSize) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSSet, holder, 0);
  Handle<OrderedHashSet> table(OrderedHashSet::cast(holder->table()));
  return Smi::FromInt(table->NumberOfElements());
}


RUNTIME_FUNCTION(Runtime_SetIteratorInitialize) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSSetIterator, holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSSet, set, 1);
  CONVERT_SMI_ARG_CHECKED(kind, 2)
  RUNTIME_ASSERT(kind == JSSetIterator::kKindValues ||
                 kind == JSSetIterator::kKindEntries);
  Handle<OrderedHashSet> table(OrderedHashSet::cast(set->table()));
  holder->set_table(*table);
  holder->set_index(Smi::FromInt(0));
  holder->set_kind(Smi::FromInt(kind));
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_SetIteratorClone) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSSetIterator, holder, 0);

  Handle<JSSetIterator> result = isolate->factory()->NewJSSetIterator();
  result->set_table(holder->table());
  result->set_index(Smi::FromInt(Smi::cast(holder->index())->value()));
  result->set_kind(Smi::FromInt(Smi::cast(holder->kind())->value()));

  return *result;
}


RUNTIME_FUNCTION(Runtime_SetIteratorNext) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_CHECKED(JSSetIterator, holder, 0);
  CONVERT_ARG_CHECKED(JSArray, value_array, 1);
  return holder->Next(value_array);
}


// The array returned contains the following information:
// 0: HasMore flag
// 1: Iteration index
// 2: Iteration kind
RUNTIME_FUNCTION(Runtime_SetIteratorDetails) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSSetIterator, holder, 0);
  Handle<FixedArray> details = isolate->factory()->NewFixedArray(4);
  details->set(0, isolate->heap()->ToBoolean(holder->HasMore()));
  details->set(1, holder->index());
  details->set(2, holder->kind());
  return *isolate->factory()->NewJSArrayWithElements(details);
}


RUNTIME_FUNCTION(Runtime_MapInitialize) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSMap, holder, 0);
  Handle<OrderedHashMap> table = isolate->factory()->NewOrderedHashMap();
  holder->set_table(*table);
  return *holder;
}


RUNTIME_FUNCTION(Runtime_MapGet) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSMap, holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  Handle<OrderedHashMap> table(OrderedHashMap::cast(holder->table()));
  Handle<Object> lookup(table->Lookup(key), isolate);
  return lookup->IsTheHole() ? isolate->heap()->undefined_value() : *lookup;
}


RUNTIME_FUNCTION(Runtime_MapHas) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSMap, holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  Handle<OrderedHashMap> table(OrderedHashMap::cast(holder->table()));
  Handle<Object> lookup(table->Lookup(key), isolate);
  return isolate->heap()->ToBoolean(!lookup->IsTheHole());
}


RUNTIME_FUNCTION(Runtime_MapDelete) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSMap, holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  Handle<OrderedHashMap> table(OrderedHashMap::cast(holder->table()));
  bool was_present = false;
  Handle<OrderedHashMap> new_table =
      OrderedHashMap::Remove(table, key, &was_present);
  holder->set_table(*new_table);
  return isolate->heap()->ToBoolean(was_present);
}


RUNTIME_FUNCTION(Runtime_MapClear) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSMap, holder, 0);
  Handle<OrderedHashMap> table(OrderedHashMap::cast(holder->table()));
  table = OrderedHashMap::Clear(table);
  holder->set_table(*table);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_MapSet) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSMap, holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  Handle<OrderedHashMap> table(OrderedHashMap::cast(holder->table()));
  Handle<OrderedHashMap> new_table = OrderedHashMap::Put(table, key, value);
  holder->set_table(*new_table);
  return *holder;
}


RUNTIME_FUNCTION(Runtime_MapGetSize) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSMap, holder, 0);
  Handle<OrderedHashMap> table(OrderedHashMap::cast(holder->table()));
  return Smi::FromInt(table->NumberOfElements());
}


RUNTIME_FUNCTION(Runtime_MapIteratorInitialize) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSMapIterator, holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSMap, map, 1);
  CONVERT_SMI_ARG_CHECKED(kind, 2)
  RUNTIME_ASSERT(kind == JSMapIterator::kKindKeys ||
                 kind == JSMapIterator::kKindValues ||
                 kind == JSMapIterator::kKindEntries);
  Handle<OrderedHashMap> table(OrderedHashMap::cast(map->table()));
  holder->set_table(*table);
  holder->set_index(Smi::FromInt(0));
  holder->set_kind(Smi::FromInt(kind));
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_MapIteratorClone) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSMapIterator, holder, 0);

  Handle<JSMapIterator> result = isolate->factory()->NewJSMapIterator();
  result->set_table(holder->table());
  result->set_index(Smi::FromInt(Smi::cast(holder->index())->value()));
  result->set_kind(Smi::FromInt(Smi::cast(holder->kind())->value()));

  return *result;
}


// The array returned contains the following information:
// 0: HasMore flag
// 1: Iteration index
// 2: Iteration kind
RUNTIME_FUNCTION(Runtime_MapIteratorDetails) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSMapIterator, holder, 0);
  Handle<FixedArray> details = isolate->factory()->NewFixedArray(4);
  details->set(0, isolate->heap()->ToBoolean(holder->HasMore()));
  details->set(1, holder->index());
  details->set(2, holder->kind());
  return *isolate->factory()->NewJSArrayWithElements(details);
}


RUNTIME_FUNCTION(Runtime_GetWeakMapEntries) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSWeakCollection, holder, 0);
  CONVERT_NUMBER_CHECKED(int, max_entries, Int32, args[1]);
  RUNTIME_ASSERT(max_entries >= 0);

  Handle<ObjectHashTable> table(ObjectHashTable::cast(holder->table()));
  if (max_entries == 0 || max_entries > table->NumberOfElements()) {
    max_entries = table->NumberOfElements();
  }
  Handle<FixedArray> entries =
      isolate->factory()->NewFixedArray(max_entries * 2);
  {
    DisallowHeapAllocation no_gc;
    int count = 0;
    for (int i = 0; count / 2 < max_entries && i < table->Capacity(); i++) {
      Handle<Object> key(table->KeyAt(i), isolate);
      if (table->IsKey(*key)) {
        entries->set(count++, *key);
        Object* value = table->Lookup(key);
        entries->set(count++, value);
      }
    }
    DCHECK_EQ(max_entries * 2, count);
  }
  return *isolate->factory()->NewJSArrayWithElements(entries);
}


RUNTIME_FUNCTION(Runtime_MapIteratorNext) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_CHECKED(JSMapIterator, holder, 0);
  CONVERT_ARG_CHECKED(JSArray, value_array, 1);
  return holder->Next(value_array);
}


static Handle<JSWeakCollection> WeakCollectionInitialize(
    Isolate* isolate, Handle<JSWeakCollection> weak_collection) {
  DCHECK(weak_collection->map()->inobject_properties() == 0);
  Handle<ObjectHashTable> table = ObjectHashTable::New(isolate, 0);
  weak_collection->set_table(*table);
  return weak_collection;
}


RUNTIME_FUNCTION(Runtime_WeakCollectionInitialize) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSWeakCollection, weak_collection, 0);
  return *WeakCollectionInitialize(isolate, weak_collection);
}


RUNTIME_FUNCTION(Runtime_WeakCollectionGet) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSWeakCollection, weak_collection, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  RUNTIME_ASSERT(key->IsJSReceiver() || key->IsSymbol());
  Handle<ObjectHashTable> table(
      ObjectHashTable::cast(weak_collection->table()));
  RUNTIME_ASSERT(table->IsKey(*key));
  Handle<Object> lookup(table->Lookup(key), isolate);
  return lookup->IsTheHole() ? isolate->heap()->undefined_value() : *lookup;
}


RUNTIME_FUNCTION(Runtime_WeakCollectionHas) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSWeakCollection, weak_collection, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  RUNTIME_ASSERT(key->IsJSReceiver() || key->IsSymbol());
  Handle<ObjectHashTable> table(
      ObjectHashTable::cast(weak_collection->table()));
  RUNTIME_ASSERT(table->IsKey(*key));
  Handle<Object> lookup(table->Lookup(key), isolate);
  return isolate->heap()->ToBoolean(!lookup->IsTheHole());
}


RUNTIME_FUNCTION(Runtime_WeakCollectionDelete) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSWeakCollection, weak_collection, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  RUNTIME_ASSERT(key->IsJSReceiver() || key->IsSymbol());
  Handle<ObjectHashTable> table(
      ObjectHashTable::cast(weak_collection->table()));
  RUNTIME_ASSERT(table->IsKey(*key));
  bool was_present = false;
  Handle<ObjectHashTable> new_table =
      ObjectHashTable::Remove(table, key, &was_present);
  weak_collection->set_table(*new_table);
  if (*table != *new_table) {
    // Zap the old table since we didn't record slots for its elements.
    table->FillWithHoles(0, table->length());
  }
  return isolate->heap()->ToBoolean(was_present);
}


RUNTIME_FUNCTION(Runtime_WeakCollectionSet) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSWeakCollection, weak_collection, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  RUNTIME_ASSERT(key->IsJSReceiver() || key->IsSymbol());
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  Handle<ObjectHashTable> table(
      ObjectHashTable::cast(weak_collection->table()));
  RUNTIME_ASSERT(table->IsKey(*key));
  Handle<ObjectHashTable> new_table = ObjectHashTable::Put(table, key, value);
  weak_collection->set_table(*new_table);
  if (*table != *new_table) {
    // Zap the old table since we didn't record slots for its elements.
    table->FillWithHoles(0, table->length());
  }
  return *weak_collection;
}


RUNTIME_FUNCTION(Runtime_GetWeakSetValues) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSWeakCollection, holder, 0);
  CONVERT_NUMBER_CHECKED(int, max_values, Int32, args[1]);
  RUNTIME_ASSERT(max_values >= 0);

  Handle<ObjectHashTable> table(ObjectHashTable::cast(holder->table()));
  if (max_values == 0 || max_values > table->NumberOfElements()) {
    max_values = table->NumberOfElements();
  }
  Handle<FixedArray> values = isolate->factory()->NewFixedArray(max_values);
  {
    DisallowHeapAllocation no_gc;
    int count = 0;
    for (int i = 0; count < max_values && i < table->Capacity(); i++) {
      Handle<Object> key(table->KeyAt(i), isolate);
      if (table->IsKey(*key)) values->set(count++, *key);
    }
    DCHECK_EQ(max_values, count);
  }
  return *isolate->factory()->NewJSArrayWithElements(values);
}


RUNTIME_FUNCTION(Runtime_ObservationWeakMapCreate) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0);
  // TODO(adamk): Currently this runtime function is only called three times per
  // isolate. If it's called more often, the map should be moved into the
  // strong root list.
  Handle<Map> map =
      isolate->factory()->NewMap(JS_WEAK_MAP_TYPE, JSWeakMap::kSize);
  Handle<JSWeakMap> weakmap =
      Handle<JSWeakMap>::cast(isolate->factory()->NewJSObjectFromMap(map));
  return *WeakCollectionInitialize(isolate, weakmap);
}
}
}  // namespace v8::internal
