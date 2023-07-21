// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/template-objects.h"

#include "src/base/functional.h"
#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/objects/js-array.h"
#include "src/objects/objects-inl.h"
#include "src/objects/property-descriptor.h"
#include "src/objects/template-objects-inl.h"

namespace v8 {
namespace internal {

namespace {
bool CachedTemplateMatches(Isolate* isolate, NativeContext native_context,
                           JSArray entry, int function_literal_id, int slot_id,
                           DisallowGarbageCollection& no_gc) {
  if (native_context->is_js_array_template_literal_object_map(
          entry->map(isolate))) {
    TemplateLiteralObject template_object = TemplateLiteralObject::cast(entry);
    return template_object->function_literal_id() == function_literal_id &&
           template_object->slot_id() == slot_id;
  }

  Handle<JSArray> entry_handle(entry, isolate);
  Smi cached_function_literal_id = Smi::cast(*JSReceiver::GetDataProperty(
      isolate, entry_handle,
      isolate->factory()->template_literal_function_literal_id_symbol()));
  if (cached_function_literal_id.value() != function_literal_id) return false;

  Smi cached_slot_id = Smi::cast(*JSReceiver::GetDataProperty(
      isolate, entry_handle,
      isolate->factory()->template_literal_slot_id_symbol()));
  if (cached_slot_id.value() != slot_id) return false;

  return true;
}
}  // namespace

// static
Handle<JSArray> TemplateObjectDescription::GetTemplateObject(
    Isolate* isolate, Handle<NativeContext> native_context,
    Handle<TemplateObjectDescription> description,
    Handle<SharedFunctionInfo> shared_info, int slot_id) {
  int function_literal_id = shared_info->function_literal_id();

  // Check the template weakmap to see if the template object already exists.
  Handle<Script> script(Script::cast(shared_info->script(isolate)), isolate);
  int32_t hash =
      EphemeronHashTable::ShapeT::Hash(ReadOnlyRoots(isolate), script);
  MaybeHandle<ArrayList> maybe_cached_templates;

  if (!native_context->template_weakmap().IsUndefined(isolate)) {
    DisallowGarbageCollection no_gc;
    // The no_gc keeps this safe, and gcmole is confused because
    // CachedTemplateMatches calls JSReceiver::GetDataProperty.
    DisableGCMole no_gcmole;
    ReadOnlyRoots roots(isolate);
    EphemeronHashTable template_weakmap =
        EphemeronHashTable::cast(native_context->template_weakmap());
    Object cached_templates_lookup =
        template_weakmap->Lookup(isolate, script, hash);
    if (!cached_templates_lookup.IsTheHole(roots)) {
      ArrayList cached_templates = ArrayList::cast(cached_templates_lookup);
      maybe_cached_templates = handle(cached_templates, isolate);

      // Linear search over the cached template array list for a template
      // object matching the given function_literal_id + slot_id.
      // TODO(leszeks): Consider keeping this list sorted for faster lookup.
      for (int i = 0; i < cached_templates->Length(); i++) {
        JSArray template_object = JSArray::cast(cached_templates->Get(i));
        if (CachedTemplateMatches(isolate, *native_context, template_object,
                                  function_literal_id, slot_id, no_gc)) {
          return handle(template_object, isolate);
        }
      }
    }
  }

  // Create the raw object from the {raw_strings}.
  Handle<FixedArray> raw_strings(description->raw_strings(), isolate);
  Handle<FixedArray> cooked_strings(description->cooked_strings(), isolate);
  Handle<JSArray> template_object =
      isolate->factory()->NewJSArrayForTemplateLiteralArray(
          cooked_strings, raw_strings, function_literal_id, slot_id);

  // Insert the template object into the cached template array list.
  Handle<ArrayList> cached_templates;
  if (!maybe_cached_templates.ToHandle(&cached_templates)) {
    cached_templates = isolate->factory()->NewArrayList(1);
  }
  cached_templates = ArrayList::Add(isolate, cached_templates, template_object);

  // Compare the cached_templates to the original maybe_cached_templates loaded
  // from the weakmap -- if it doesn't match, we need to update the weakmap.
  Handle<ArrayList> old_cached_templates;
  if (!maybe_cached_templates.ToHandle(&old_cached_templates) ||
      *old_cached_templates != *cached_templates) {
    HeapObject maybe_template_weakmap = native_context->template_weakmap();
    Handle<EphemeronHashTable> template_weakmap;
    if (maybe_template_weakmap.IsUndefined()) {
      template_weakmap = EphemeronHashTable::New(isolate, 1);
    } else {
      template_weakmap =
          handle(EphemeronHashTable::cast(maybe_template_weakmap), isolate);
    }
    template_weakmap = EphemeronHashTable::Put(isolate, template_weakmap,
                                               script, cached_templates, hash);
    native_context->set_template_weakmap(*template_weakmap);
  }

  // Check that the list is in the appropriate location on the weakmap, and
  // that the appropriate entry is in the right location in this list.
  DCHECK_EQ(EphemeronHashTable::cast(native_context->template_weakmap())
                ->Lookup(isolate, script, hash),
            *cached_templates);
  DCHECK_EQ(cached_templates->Get(cached_templates->Length() - 1),
            *template_object);

  return template_object;
}

}  // namespace internal
}  // namespace v8
