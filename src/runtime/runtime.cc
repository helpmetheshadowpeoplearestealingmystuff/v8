// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <limits>

#include "src/v8.h"

#include "src/accessors.h"
#include "src/allocation-site-scopes.h"
#include "src/api.h"
#include "src/arguments.h"
#include "src/bailout-reason.h"
#include "src/base/cpu.h"
#include "src/base/platform/platform.h"
#include "src/bootstrapper.h"
#include "src/codegen.h"
#include "src/compilation-cache.h"
#include "src/compiler.h"
#include "src/conversions.h"
#include "src/deoptimizer.h"
#include "src/execution.h"
#include "src/full-codegen.h"
#include "src/global-handles.h"
#include "src/isolate-inl.h"
#include "src/parser.h"
#include "src/prototype.h"
#include "src/runtime/runtime.h"
#include "src/runtime/runtime-utils.h"
#include "src/scopeinfo.h"
#include "src/smart-pointers.h"
#include "src/utils.h"
#include "src/v8threads.h"


namespace v8 {
namespace internal {

// Header of runtime functions.
#define F(name, number_of_args, result_size)                    \
  Object* Runtime_##name(int args_length, Object** args_object, \
                         Isolate* isolate);

#define P(name, number_of_args, result_size)                       \
  ObjectPair Runtime_##name(int args_length, Object** args_object, \
                            Isolate* isolate);

#define I(name, number_of_args, result_size)                             \
  Object* RuntimeReference_##name(int args_length, Object** args_object, \
                                  Isolate* isolate);

RUNTIME_FUNCTION_LIST_RETURN_OBJECT(F)
RUNTIME_FUNCTION_LIST_RETURN_PAIR(P)
INLINE_OPTIMIZED_FUNCTION_LIST(F)
INLINE_FUNCTION_LIST(I)

#undef I
#undef F
#undef P


static Handle<Map> ComputeObjectLiteralMap(
    Handle<Context> context, Handle<FixedArray> constant_properties,
    bool* is_result_from_cache) {
  Isolate* isolate = context->GetIsolate();
  int properties_length = constant_properties->length();
  int number_of_properties = properties_length / 2;
  // Check that there are only internal strings and array indices among keys.
  int number_of_string_keys = 0;
  for (int p = 0; p != properties_length; p += 2) {
    Object* key = constant_properties->get(p);
    uint32_t element_index = 0;
    if (key->IsInternalizedString()) {
      number_of_string_keys++;
    } else if (key->ToArrayIndex(&element_index)) {
      // An index key does not require space in the property backing store.
      number_of_properties--;
    } else {
      // Bail out as a non-internalized-string non-index key makes caching
      // impossible.
      // DCHECK to make sure that the if condition after the loop is false.
      DCHECK(number_of_string_keys != number_of_properties);
      break;
    }
  }
  // If we only have internalized strings and array indices among keys then we
  // can use the map cache in the native context.
  const int kMaxKeys = 10;
  if ((number_of_string_keys == number_of_properties) &&
      (number_of_string_keys < kMaxKeys)) {
    // Create the fixed array with the key.
    Handle<FixedArray> keys =
        isolate->factory()->NewFixedArray(number_of_string_keys);
    if (number_of_string_keys > 0) {
      int index = 0;
      for (int p = 0; p < properties_length; p += 2) {
        Object* key = constant_properties->get(p);
        if (key->IsInternalizedString()) {
          keys->set(index++, key);
        }
      }
      DCHECK(index == number_of_string_keys);
    }
    *is_result_from_cache = true;
    return isolate->factory()->ObjectLiteralMapFromCache(context, keys);
  }
  *is_result_from_cache = false;
  return Map::Create(isolate, number_of_properties);
}


MUST_USE_RESULT static MaybeHandle<Object> CreateLiteralBoilerplate(
    Isolate* isolate, Handle<FixedArray> literals,
    Handle<FixedArray> constant_properties);


MUST_USE_RESULT static MaybeHandle<Object> CreateObjectLiteralBoilerplate(
    Isolate* isolate, Handle<FixedArray> literals,
    Handle<FixedArray> constant_properties, bool should_have_fast_elements,
    bool has_function_literal) {
  // Get the native context from the literals array.  This is the
  // context in which the function was created and we use the object
  // function from this context to create the object literal.  We do
  // not use the object function from the current native context
  // because this might be the object function from another context
  // which we should not have access to.
  Handle<Context> context =
      Handle<Context>(JSFunction::NativeContextFromLiterals(*literals));

  // In case we have function literals, we want the object to be in
  // slow properties mode for now. We don't go in the map cache because
  // maps with constant functions can't be shared if the functions are
  // not the same (which is the common case).
  bool is_result_from_cache = false;
  Handle<Map> map = has_function_literal
                        ? Handle<Map>(context->object_function()->initial_map())
                        : ComputeObjectLiteralMap(context, constant_properties,
                                                  &is_result_from_cache);

  PretenureFlag pretenure_flag =
      isolate->heap()->InNewSpace(*literals) ? NOT_TENURED : TENURED;

  Handle<JSObject> boilerplate =
      isolate->factory()->NewJSObjectFromMap(map, pretenure_flag);

  // Normalize the elements of the boilerplate to save space if needed.
  if (!should_have_fast_elements) JSObject::NormalizeElements(boilerplate);

  // Add the constant properties to the boilerplate.
  int length = constant_properties->length();
  bool should_transform =
      !is_result_from_cache && boilerplate->HasFastProperties();
  bool should_normalize = should_transform || has_function_literal;
  if (should_normalize) {
    // TODO(verwaest): We might not want to ever normalize here.
    JSObject::NormalizeProperties(boilerplate, KEEP_INOBJECT_PROPERTIES,
                                  length / 2);
  }
  // TODO(verwaest): Support tracking representations in the boilerplate.
  for (int index = 0; index < length; index += 2) {
    Handle<Object> key(constant_properties->get(index + 0), isolate);
    Handle<Object> value(constant_properties->get(index + 1), isolate);
    if (value->IsFixedArray()) {
      // The value contains the constant_properties of a
      // simple object or array literal.
      Handle<FixedArray> array = Handle<FixedArray>::cast(value);
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, value, CreateLiteralBoilerplate(isolate, literals, array),
          Object);
    }
    MaybeHandle<Object> maybe_result;
    uint32_t element_index = 0;
    if (key->IsInternalizedString()) {
      if (Handle<String>::cast(key)->AsArrayIndex(&element_index)) {
        // Array index as string (uint32).
        if (value->IsUninitialized()) value = handle(Smi::FromInt(0), isolate);
        maybe_result =
            JSObject::SetOwnElement(boilerplate, element_index, value, SLOPPY);
      } else {
        Handle<String> name(String::cast(*key));
        DCHECK(!name->AsArrayIndex(&element_index));
        maybe_result = JSObject::SetOwnPropertyIgnoreAttributes(
            boilerplate, name, value, NONE);
      }
    } else if (key->ToArrayIndex(&element_index)) {
      // Array index (uint32).
      if (value->IsUninitialized()) value = handle(Smi::FromInt(0), isolate);
      maybe_result =
          JSObject::SetOwnElement(boilerplate, element_index, value, SLOPPY);
    } else {
      // Non-uint32 number.
      DCHECK(key->IsNumber());
      double num = key->Number();
      char arr[100];
      Vector<char> buffer(arr, arraysize(arr));
      const char* str = DoubleToCString(num, buffer);
      Handle<String> name = isolate->factory()->NewStringFromAsciiChecked(str);
      maybe_result = JSObject::SetOwnPropertyIgnoreAttributes(boilerplate, name,
                                                              value, NONE);
    }
    // If setting the property on the boilerplate throws an
    // exception, the exception is converted to an empty handle in
    // the handle based operations.  In that case, we need to
    // convert back to an exception.
    RETURN_ON_EXCEPTION(isolate, maybe_result, Object);
  }

  // Transform to fast properties if necessary. For object literals with
  // containing function literals we defer this operation until after all
  // computed properties have been assigned so that we can generate
  // constant function properties.
  if (should_transform && !has_function_literal) {
    JSObject::MigrateSlowToFast(boilerplate,
                                boilerplate->map()->unused_property_fields());
  }

  return boilerplate;
}


MUST_USE_RESULT static MaybeHandle<Object> TransitionElements(
    Handle<Object> object, ElementsKind to_kind, Isolate* isolate) {
  HandleScope scope(isolate);
  if (!object->IsJSObject()) {
    isolate->ThrowIllegalOperation();
    return MaybeHandle<Object>();
  }
  ElementsKind from_kind =
      Handle<JSObject>::cast(object)->map()->elements_kind();
  if (Map::IsValidElementsTransition(from_kind, to_kind)) {
    JSObject::TransitionElementsKind(Handle<JSObject>::cast(object), to_kind);
    return object;
  }
  isolate->ThrowIllegalOperation();
  return MaybeHandle<Object>();
}


MaybeHandle<Object> Runtime::CreateArrayLiteralBoilerplate(
    Isolate* isolate, Handle<FixedArray> literals,
    Handle<FixedArray> elements) {
  // Create the JSArray.
  Handle<JSFunction> constructor(
      JSFunction::NativeContextFromLiterals(*literals)->array_function());

  PretenureFlag pretenure_flag =
      isolate->heap()->InNewSpace(*literals) ? NOT_TENURED : TENURED;

  Handle<JSArray> object = Handle<JSArray>::cast(
      isolate->factory()->NewJSObject(constructor, pretenure_flag));

  ElementsKind constant_elements_kind =
      static_cast<ElementsKind>(Smi::cast(elements->get(0))->value());
  Handle<FixedArrayBase> constant_elements_values(
      FixedArrayBase::cast(elements->get(1)));

  {
    DisallowHeapAllocation no_gc;
    DCHECK(IsFastElementsKind(constant_elements_kind));
    Context* native_context = isolate->context()->native_context();
    Object* maps_array = native_context->js_array_maps();
    DCHECK(!maps_array->IsUndefined());
    Object* map = FixedArray::cast(maps_array)->get(constant_elements_kind);
    object->set_map(Map::cast(map));
  }

  Handle<FixedArrayBase> copied_elements_values;
  if (IsFastDoubleElementsKind(constant_elements_kind)) {
    copied_elements_values = isolate->factory()->CopyFixedDoubleArray(
        Handle<FixedDoubleArray>::cast(constant_elements_values));
  } else {
    DCHECK(IsFastSmiOrObjectElementsKind(constant_elements_kind));
    const bool is_cow = (constant_elements_values->map() ==
                         isolate->heap()->fixed_cow_array_map());
    if (is_cow) {
      copied_elements_values = constant_elements_values;
#if DEBUG
      Handle<FixedArray> fixed_array_values =
          Handle<FixedArray>::cast(copied_elements_values);
      for (int i = 0; i < fixed_array_values->length(); i++) {
        DCHECK(!fixed_array_values->get(i)->IsFixedArray());
      }
#endif
    } else {
      Handle<FixedArray> fixed_array_values =
          Handle<FixedArray>::cast(constant_elements_values);
      Handle<FixedArray> fixed_array_values_copy =
          isolate->factory()->CopyFixedArray(fixed_array_values);
      copied_elements_values = fixed_array_values_copy;
      for (int i = 0; i < fixed_array_values->length(); i++) {
        if (fixed_array_values->get(i)->IsFixedArray()) {
          // The value contains the constant_properties of a
          // simple object or array literal.
          Handle<FixedArray> fa(FixedArray::cast(fixed_array_values->get(i)));
          Handle<Object> result;
          ASSIGN_RETURN_ON_EXCEPTION(
              isolate, result, CreateLiteralBoilerplate(isolate, literals, fa),
              Object);
          fixed_array_values_copy->set(i, *result);
        }
      }
    }
  }
  object->set_elements(*copied_elements_values);
  object->set_length(Smi::FromInt(copied_elements_values->length()));

  JSObject::ValidateElements(object);
  return object;
}


MUST_USE_RESULT static MaybeHandle<Object> CreateLiteralBoilerplate(
    Isolate* isolate, Handle<FixedArray> literals, Handle<FixedArray> array) {
  Handle<FixedArray> elements = CompileTimeValue::GetElements(array);
  const bool kHasNoFunctionLiteral = false;
  switch (CompileTimeValue::GetLiteralType(array)) {
    case CompileTimeValue::OBJECT_LITERAL_FAST_ELEMENTS:
      return CreateObjectLiteralBoilerplate(isolate, literals, elements, true,
                                            kHasNoFunctionLiteral);
    case CompileTimeValue::OBJECT_LITERAL_SLOW_ELEMENTS:
      return CreateObjectLiteralBoilerplate(isolate, literals, elements, false,
                                            kHasNoFunctionLiteral);
    case CompileTimeValue::ARRAY_LITERAL:
      return Runtime::CreateArrayLiteralBoilerplate(isolate, literals,
                                                    elements);
    default:
      UNREACHABLE();
      return MaybeHandle<Object>();
  }
}


RUNTIME_FUNCTION(Runtime_CreateObjectLiteral) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(FixedArray, literals, 0);
  CONVERT_SMI_ARG_CHECKED(literals_index, 1);
  CONVERT_ARG_HANDLE_CHECKED(FixedArray, constant_properties, 2);
  CONVERT_SMI_ARG_CHECKED(flags, 3);
  bool should_have_fast_elements = (flags & ObjectLiteral::kFastElements) != 0;
  bool has_function_literal = (flags & ObjectLiteral::kHasFunction) != 0;

  RUNTIME_ASSERT(literals_index >= 0 && literals_index < literals->length());

  // Check if boilerplate exists. If not, create it first.
  Handle<Object> literal_site(literals->get(literals_index), isolate);
  Handle<AllocationSite> site;
  Handle<JSObject> boilerplate;
  if (*literal_site == isolate->heap()->undefined_value()) {
    Handle<Object> raw_boilerplate;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, raw_boilerplate,
        CreateObjectLiteralBoilerplate(isolate, literals, constant_properties,
                                       should_have_fast_elements,
                                       has_function_literal));
    boilerplate = Handle<JSObject>::cast(raw_boilerplate);

    AllocationSiteCreationContext creation_context(isolate);
    site = creation_context.EnterNewScope();
    RETURN_FAILURE_ON_EXCEPTION(
        isolate, JSObject::DeepWalk(boilerplate, &creation_context));
    creation_context.ExitScope(site, boilerplate);

    // Update the functions literal and return the boilerplate.
    literals->set(literals_index, *site);
  } else {
    site = Handle<AllocationSite>::cast(literal_site);
    boilerplate =
        Handle<JSObject>(JSObject::cast(site->transition_info()), isolate);
  }

  AllocationSiteUsageContext usage_context(isolate, site, true);
  usage_context.EnterNewScope();
  MaybeHandle<Object> maybe_copy =
      JSObject::DeepCopy(boilerplate, &usage_context);
  usage_context.ExitScope(site, boilerplate);
  Handle<Object> copy;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, copy, maybe_copy);
  return *copy;
}


MUST_USE_RESULT static MaybeHandle<AllocationSite> GetLiteralAllocationSite(
    Isolate* isolate, Handle<FixedArray> literals, int literals_index,
    Handle<FixedArray> elements) {
  // Check if boilerplate exists. If not, create it first.
  Handle<Object> literal_site(literals->get(literals_index), isolate);
  Handle<AllocationSite> site;
  if (*literal_site == isolate->heap()->undefined_value()) {
    DCHECK(*elements != isolate->heap()->empty_fixed_array());
    Handle<Object> boilerplate;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, boilerplate,
        Runtime::CreateArrayLiteralBoilerplate(isolate, literals, elements),
        AllocationSite);

    AllocationSiteCreationContext creation_context(isolate);
    site = creation_context.EnterNewScope();
    if (JSObject::DeepWalk(Handle<JSObject>::cast(boilerplate),
                           &creation_context).is_null()) {
      return Handle<AllocationSite>::null();
    }
    creation_context.ExitScope(site, Handle<JSObject>::cast(boilerplate));

    literals->set(literals_index, *site);
  } else {
    site = Handle<AllocationSite>::cast(literal_site);
  }

  return site;
}


static MaybeHandle<JSObject> CreateArrayLiteralImpl(Isolate* isolate,
                                                    Handle<FixedArray> literals,
                                                    int literals_index,
                                                    Handle<FixedArray> elements,
                                                    int flags) {
  RUNTIME_ASSERT_HANDLIFIED(
      literals_index >= 0 && literals_index < literals->length(), JSObject);
  Handle<AllocationSite> site;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, site,
      GetLiteralAllocationSite(isolate, literals, literals_index, elements),
      JSObject);

  bool enable_mementos = (flags & ArrayLiteral::kDisableMementos) == 0;
  Handle<JSObject> boilerplate(JSObject::cast(site->transition_info()));
  AllocationSiteUsageContext usage_context(isolate, site, enable_mementos);
  usage_context.EnterNewScope();
  JSObject::DeepCopyHints hints = (flags & ArrayLiteral::kShallowElements) == 0
                                      ? JSObject::kNoHints
                                      : JSObject::kObjectIsShallow;
  MaybeHandle<JSObject> copy =
      JSObject::DeepCopy(boilerplate, &usage_context, hints);
  usage_context.ExitScope(site, boilerplate);
  return copy;
}


RUNTIME_FUNCTION(Runtime_CreateArrayLiteral) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(FixedArray, literals, 0);
  CONVERT_SMI_ARG_CHECKED(literals_index, 1);
  CONVERT_ARG_HANDLE_CHECKED(FixedArray, elements, 2);
  CONVERT_SMI_ARG_CHECKED(flags, 3);

  Handle<JSObject> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, CreateArrayLiteralImpl(isolate, literals, literals_index,
                                              elements, flags));
  return *result;
}


RUNTIME_FUNCTION(Runtime_CreateArrayLiteralStubBailout) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(FixedArray, literals, 0);
  CONVERT_SMI_ARG_CHECKED(literals_index, 1);
  CONVERT_ARG_HANDLE_CHECKED(FixedArray, elements, 2);

  Handle<JSObject> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      CreateArrayLiteralImpl(isolate, literals, literals_index, elements,
                             ArrayLiteral::kShallowElements));
  return *result;
}


RUNTIME_FUNCTION(Runtime_CreateSymbol) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, name, 0);
  RUNTIME_ASSERT(name->IsString() || name->IsUndefined());
  Handle<Symbol> symbol = isolate->factory()->NewSymbol();
  if (name->IsString()) symbol->set_name(*name);
  return *symbol;
}


RUNTIME_FUNCTION(Runtime_CreatePrivateSymbol) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, name, 0);
  RUNTIME_ASSERT(name->IsString() || name->IsUndefined());
  Handle<Symbol> symbol = isolate->factory()->NewPrivateSymbol();
  if (name->IsString()) symbol->set_name(*name);
  return *symbol;
}


RUNTIME_FUNCTION(Runtime_CreatePrivateOwnSymbol) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, name, 0);
  RUNTIME_ASSERT(name->IsString() || name->IsUndefined());
  Handle<Symbol> symbol = isolate->factory()->NewPrivateOwnSymbol();
  if (name->IsString()) symbol->set_name(*name);
  return *symbol;
}


RUNTIME_FUNCTION(Runtime_CreateGlobalPrivateOwnSymbol) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(String, name, 0);
  Handle<JSObject> registry = isolate->GetSymbolRegistry();
  Handle<String> part = isolate->factory()->private_intern_string();
  Handle<Object> privates;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, privates, Object::GetPropertyOrElement(registry, part));
  Handle<Object> symbol;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, symbol, Object::GetPropertyOrElement(privates, name));
  if (!symbol->IsSymbol()) {
    DCHECK(symbol->IsUndefined());
    symbol = isolate->factory()->NewPrivateSymbol();
    Handle<Symbol>::cast(symbol)->set_name(*name);
    Handle<Symbol>::cast(symbol)->set_is_own(true);
    JSObject::SetProperty(Handle<JSObject>::cast(privates), name, symbol,
                          STRICT).Assert();
  }
  return *symbol;
}


RUNTIME_FUNCTION(Runtime_NewSymbolWrapper) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Symbol, symbol, 0);
  return *Object::ToObject(isolate, symbol).ToHandleChecked();
}


RUNTIME_FUNCTION(Runtime_SymbolDescription) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Symbol, symbol, 0);
  return symbol->name();
}


RUNTIME_FUNCTION(Runtime_SymbolRegistry) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0);
  return *isolate->GetSymbolRegistry();
}


RUNTIME_FUNCTION(Runtime_SymbolIsPrivate) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Symbol, symbol, 0);
  return isolate->heap()->ToBoolean(symbol->is_private());
}


RUNTIME_FUNCTION(Runtime_CreateJSProxy) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, handler, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, prototype, 1);
  if (!prototype->IsJSReceiver()) prototype = isolate->factory()->null_value();
  return *isolate->factory()->NewJSProxy(handler, prototype);
}


RUNTIME_FUNCTION(Runtime_CreateJSFunctionProxy) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, handler, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, call_trap, 1);
  RUNTIME_ASSERT(call_trap->IsJSFunction() || call_trap->IsJSFunctionProxy());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, construct_trap, 2);
  CONVERT_ARG_HANDLE_CHECKED(Object, prototype, 3);
  if (!prototype->IsJSReceiver()) prototype = isolate->factory()->null_value();
  return *isolate->factory()->NewJSFunctionProxy(handler, call_trap,
                                                 construct_trap, prototype);
}


RUNTIME_FUNCTION(Runtime_IsJSProxy) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(obj->IsJSProxy());
}


RUNTIME_FUNCTION(Runtime_IsJSFunctionProxy) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(obj->IsJSFunctionProxy());
}


RUNTIME_FUNCTION(Runtime_GetHandler) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(JSProxy, proxy, 0);
  return proxy->handler();
}


RUNTIME_FUNCTION(Runtime_GetCallTrap) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(JSFunctionProxy, proxy, 0);
  return proxy->call_trap();
}


RUNTIME_FUNCTION(Runtime_GetConstructTrap) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(JSFunctionProxy, proxy, 0);
  return proxy->construct_trap();
}


RUNTIME_FUNCTION(Runtime_Fix) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSProxy, proxy, 0);
  JSProxy::Fix(proxy);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_GetPrototype) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, obj, 0);
  // We don't expect access checks to be needed on JSProxy objects.
  DCHECK(!obj->IsAccessCheckNeeded() || obj->IsJSObject());
  PrototypeIterator iter(isolate, obj, PrototypeIterator::START_AT_RECEIVER);
  do {
    if (PrototypeIterator::GetCurrent(iter)->IsAccessCheckNeeded() &&
        !isolate->MayNamedAccess(
            Handle<JSObject>::cast(PrototypeIterator::GetCurrent(iter)),
            isolate->factory()->proto_string(), v8::ACCESS_GET)) {
      isolate->ReportFailedAccessCheck(
          Handle<JSObject>::cast(PrototypeIterator::GetCurrent(iter)),
          v8::ACCESS_GET);
      RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
      return isolate->heap()->undefined_value();
    }
    iter.AdvanceIgnoringProxies();
    if (PrototypeIterator::GetCurrent(iter)->IsJSProxy()) {
      return *PrototypeIterator::GetCurrent(iter);
    }
  } while (!iter.IsAtEnd(PrototypeIterator::END_AT_NON_HIDDEN));
  return *PrototypeIterator::GetCurrent(iter);
}


RUNTIME_FUNCTION(Runtime_InternalSetPrototype) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, prototype, 1);
  DCHECK(!obj->IsAccessCheckNeeded());
  DCHECK(!obj->map()->is_observed());
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, JSObject::SetPrototype(obj, prototype, false));
  return *result;
}


RUNTIME_FUNCTION(Runtime_SetPrototype) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, prototype, 1);
  if (obj->IsAccessCheckNeeded() &&
      !isolate->MayNamedAccess(obj, isolate->factory()->proto_string(),
                               v8::ACCESS_SET)) {
    isolate->ReportFailedAccessCheck(obj, v8::ACCESS_SET);
    RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
    return isolate->heap()->undefined_value();
  }
  if (obj->map()->is_observed()) {
    Handle<Object> old_value =
        Object::GetPrototypeSkipHiddenPrototypes(isolate, obj);
    Handle<Object> result;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, result, JSObject::SetPrototype(obj, prototype, true));

    Handle<Object> new_value =
        Object::GetPrototypeSkipHiddenPrototypes(isolate, obj);
    if (!new_value->SameValue(*old_value)) {
      JSObject::EnqueueChangeRecord(
          obj, "setPrototype", isolate->factory()->proto_string(), old_value);
    }
    return *result;
  }
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, JSObject::SetPrototype(obj, prototype, true));
  return *result;
}


RUNTIME_FUNCTION(Runtime_IsInPrototypeChain) {
  HandleScope shs(isolate);
  DCHECK(args.length() == 2);
  // See ECMA-262, section 15.3.5.3, page 88 (steps 5 - 8).
  CONVERT_ARG_HANDLE_CHECKED(Object, O, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, V, 1);
  PrototypeIterator iter(isolate, V, PrototypeIterator::START_AT_RECEIVER);
  while (true) {
    iter.AdvanceIgnoringProxies();
    if (iter.IsAtEnd()) return isolate->heap()->false_value();
    if (iter.IsAtEnd(O)) return isolate->heap()->true_value();
  }
}


// Enumerator used as indices into the array returned from GetOwnProperty
enum PropertyDescriptorIndices {
  IS_ACCESSOR_INDEX,
  VALUE_INDEX,
  GETTER_INDEX,
  SETTER_INDEX,
  WRITABLE_INDEX,
  ENUMERABLE_INDEX,
  CONFIGURABLE_INDEX,
  DESCRIPTOR_SIZE
};


MUST_USE_RESULT static MaybeHandle<Object> GetOwnProperty(Isolate* isolate,
                                                          Handle<JSObject> obj,
                                                          Handle<Name> name) {
  Heap* heap = isolate->heap();
  Factory* factory = isolate->factory();

  PropertyAttributes attrs;
  uint32_t index = 0;
  Handle<Object> value;
  MaybeHandle<AccessorPair> maybe_accessors;
  // TODO(verwaest): Unify once indexed properties can be handled by the
  // LookupIterator.
  if (name->AsArrayIndex(&index)) {
    // Get attributes.
    Maybe<PropertyAttributes> maybe =
        JSReceiver::GetOwnElementAttribute(obj, index);
    if (!maybe.has_value) return MaybeHandle<Object>();
    attrs = maybe.value;
    if (attrs == ABSENT) return factory->undefined_value();

    // Get AccessorPair if present.
    maybe_accessors = JSObject::GetOwnElementAccessorPair(obj, index);

    // Get value if not an AccessorPair.
    if (maybe_accessors.is_null()) {
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, value, Runtime::GetElementOrCharAt(isolate, obj, index),
          Object);
    }
  } else {
    // Get attributes.
    LookupIterator it(obj, name, LookupIterator::HIDDEN);
    Maybe<PropertyAttributes> maybe = JSObject::GetPropertyAttributes(&it);
    if (!maybe.has_value) return MaybeHandle<Object>();
    attrs = maybe.value;
    if (attrs == ABSENT) return factory->undefined_value();

    // Get AccessorPair if present.
    if (it.state() == LookupIterator::ACCESSOR &&
        it.GetAccessors()->IsAccessorPair()) {
      maybe_accessors = Handle<AccessorPair>::cast(it.GetAccessors());
    }

    // Get value if not an AccessorPair.
    if (maybe_accessors.is_null()) {
      ASSIGN_RETURN_ON_EXCEPTION(isolate, value, Object::GetProperty(&it),
                                 Object);
    }
  }
  DCHECK(!isolate->has_pending_exception());
  Handle<FixedArray> elms = factory->NewFixedArray(DESCRIPTOR_SIZE);
  elms->set(ENUMERABLE_INDEX, heap->ToBoolean((attrs & DONT_ENUM) == 0));
  elms->set(CONFIGURABLE_INDEX, heap->ToBoolean((attrs & DONT_DELETE) == 0));
  elms->set(IS_ACCESSOR_INDEX, heap->ToBoolean(!maybe_accessors.is_null()));

  Handle<AccessorPair> accessors;
  if (maybe_accessors.ToHandle(&accessors)) {
    Handle<Object> getter(accessors->GetComponent(ACCESSOR_GETTER), isolate);
    Handle<Object> setter(accessors->GetComponent(ACCESSOR_SETTER), isolate);
    elms->set(GETTER_INDEX, *getter);
    elms->set(SETTER_INDEX, *setter);
  } else {
    elms->set(WRITABLE_INDEX, heap->ToBoolean((attrs & READ_ONLY) == 0));
    elms->set(VALUE_INDEX, *value);
  }

  return factory->NewJSArrayWithElements(elms);
}


// Returns an array with the property description:
//  if args[1] is not a property on args[0]
//          returns undefined
//  if args[1] is a data property on args[0]
//         [false, value, Writeable, Enumerable, Configurable]
//  if args[1] is an accessor on args[0]
//         [true, GetFunction, SetFunction, Enumerable, Configurable]
RUNTIME_FUNCTION(Runtime_GetOwnProperty) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     GetOwnProperty(isolate, obj, name));
  return *result;
}


RUNTIME_FUNCTION(Runtime_PreventExtensions) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     JSObject::PreventExtensions(obj));
  return *result;
}


RUNTIME_FUNCTION(Runtime_ToMethod) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, fun, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, home_object, 1);
  Handle<JSFunction> clone = JSFunction::CloneClosure(fun);
  Handle<Symbol> home_object_symbol(isolate->heap()->home_object_symbol());
  JSObject::SetOwnPropertyIgnoreAttributes(clone, home_object_symbol,
                                           home_object, DONT_ENUM).Assert();
  return *clone;
}


RUNTIME_FUNCTION(Runtime_HomeObjectSymbol) {
  DCHECK(args.length() == 0);
  return isolate->heap()->home_object_symbol();
}


RUNTIME_FUNCTION(Runtime_LoadFromSuper) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(Object, receiver, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, home_object, 1);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 2);

  if (home_object->IsAccessCheckNeeded() &&
      !isolate->MayNamedAccess(home_object, name, v8::ACCESS_GET)) {
    isolate->ReportFailedAccessCheck(home_object, v8::ACCESS_GET);
    RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
  }

  PrototypeIterator iter(isolate, home_object);
  Handle<Object> proto = PrototypeIterator::GetCurrent(iter);
  if (!proto->IsJSReceiver()) return isolate->heap()->undefined_value();

  LookupIterator it(receiver, name, Handle<JSReceiver>::cast(proto));
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result, Object::GetProperty(&it));
  return *result;
}


static Object* StoreToSuper(Isolate* isolate, Handle<JSObject> home_object,
                            Handle<Object> receiver, Handle<Name> name,
                            Handle<Object> value, StrictMode strict_mode) {
  if (home_object->IsAccessCheckNeeded() &&
      !isolate->MayNamedAccess(home_object, name, v8::ACCESS_SET)) {
    isolate->ReportFailedAccessCheck(home_object, v8::ACCESS_SET);
    RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
  }

  PrototypeIterator iter(isolate, home_object);
  Handle<Object> proto = PrototypeIterator::GetCurrent(iter);
  if (!proto->IsJSReceiver()) return isolate->heap()->undefined_value();

  LookupIterator it(receiver, name, Handle<JSReceiver>::cast(proto));
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      Object::SetProperty(&it, value, strict_mode,
                          Object::CERTAINLY_NOT_STORE_FROM_KEYED,
                          Object::SUPER_PROPERTY));
  return *result;
}


RUNTIME_FUNCTION(Runtime_StoreToSuper_Strict) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(Object, receiver, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, home_object, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 3);

  return StoreToSuper(isolate, home_object, receiver, name, value, STRICT);
}


RUNTIME_FUNCTION(Runtime_StoreToSuper_Sloppy) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(Object, receiver, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, home_object, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 3);

  return StoreToSuper(isolate, home_object, receiver, name, value, SLOPPY);
}


RUNTIME_FUNCTION(Runtime_IsExtensible) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(JSObject, obj, 0);
  if (obj->IsJSGlobalProxy()) {
    PrototypeIterator iter(isolate, obj);
    if (iter.IsAtEnd()) return isolate->heap()->false_value();
    DCHECK(iter.GetCurrent()->IsJSGlobalObject());
    obj = JSObject::cast(iter.GetCurrent());
  }
  return isolate->heap()->ToBoolean(obj->map()->is_extensible());
}


RUNTIME_FUNCTION(Runtime_CreateApiFunction) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(FunctionTemplateInfo, data, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, prototype, 1);
  return *isolate->factory()->CreateApiFunction(data, prototype);
}


RUNTIME_FUNCTION(Runtime_IsTemplate) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, arg, 0);
  bool result = arg->IsObjectTemplateInfo() || arg->IsFunctionTemplateInfo();
  return isolate->heap()->ToBoolean(result);
}


RUNTIME_FUNCTION(Runtime_GetTemplateField) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_CHECKED(HeapObject, templ, 0);
  CONVERT_SMI_ARG_CHECKED(index, 1);
  int offset = index * kPointerSize + HeapObject::kHeaderSize;
  InstanceType type = templ->map()->instance_type();
  RUNTIME_ASSERT(type == FUNCTION_TEMPLATE_INFO_TYPE ||
                 type == OBJECT_TEMPLATE_INFO_TYPE);
  RUNTIME_ASSERT(offset > 0);
  if (type == FUNCTION_TEMPLATE_INFO_TYPE) {
    RUNTIME_ASSERT(offset < FunctionTemplateInfo::kSize);
  } else {
    RUNTIME_ASSERT(offset < ObjectTemplateInfo::kSize);
  }
  return *HeapObject::RawField(templ, offset);
}


RUNTIME_FUNCTION(Runtime_DisableAccessChecks) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(HeapObject, object, 0);
  Handle<Map> old_map(object->map());
  bool needs_access_checks = old_map->is_access_check_needed();
  if (needs_access_checks) {
    // Copy map so it won't interfere constructor's initial map.
    Handle<Map> new_map = Map::Copy(old_map);
    new_map->set_is_access_check_needed(false);
    JSObject::MigrateToMap(Handle<JSObject>::cast(object), new_map);
  }
  return isolate->heap()->ToBoolean(needs_access_checks);
}


RUNTIME_FUNCTION(Runtime_EnableAccessChecks) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  Handle<Map> old_map(object->map());
  RUNTIME_ASSERT(!old_map->is_access_check_needed());
  // Copy map so it won't interfere constructor's initial map.
  Handle<Map> new_map = Map::Copy(old_map);
  new_map->set_is_access_check_needed(true);
  JSObject::MigrateToMap(object, new_map);
  return isolate->heap()->undefined_value();
}


static Object* ThrowRedeclarationError(Isolate* isolate, Handle<String> name) {
  HandleScope scope(isolate);
  Handle<Object> args[1] = {name};
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError("var_redeclaration", HandleVector(args, 1)));
}


// May throw a RedeclarationError.
static Object* DeclareGlobals(Isolate* isolate, Handle<GlobalObject> global,
                              Handle<String> name, Handle<Object> value,
                              PropertyAttributes attr, bool is_var,
                              bool is_const, bool is_function) {
  // Do the lookup own properties only, see ES5 erratum.
  LookupIterator it(global, name, LookupIterator::HIDDEN_SKIP_INTERCEPTOR);
  Maybe<PropertyAttributes> maybe = JSReceiver::GetPropertyAttributes(&it);
  if (!maybe.has_value) return isolate->heap()->exception();

  if (it.IsFound()) {
    PropertyAttributes old_attributes = maybe.value;
    // The name was declared before; check for conflicting re-declarations.
    if (is_const) return ThrowRedeclarationError(isolate, name);

    // Skip var re-declarations.
    if (is_var) return isolate->heap()->undefined_value();

    DCHECK(is_function);
    if ((old_attributes & DONT_DELETE) != 0) {
      // Only allow reconfiguring globals to functions in user code (no
      // natives, which are marked as read-only).
      DCHECK((attr & READ_ONLY) == 0);

      // Check whether we can reconfigure the existing property into a
      // function.
      PropertyDetails old_details = it.property_details();
      // TODO(verwaest): CALLBACKS invalidly includes ExecutableAccessInfo,
      // which are actually data properties, not accessor properties.
      if (old_details.IsReadOnly() || old_details.IsDontEnum() ||
          old_details.type() == CALLBACKS) {
        return ThrowRedeclarationError(isolate, name);
      }
      // If the existing property is not configurable, keep its attributes. Do
      attr = old_attributes;
    }
  }

  // Define or redefine own property.
  RETURN_FAILURE_ON_EXCEPTION(isolate, JSObject::SetOwnPropertyIgnoreAttributes(
                                           global, name, value, attr));

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_DeclareGlobals) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  Handle<GlobalObject> global(isolate->global_object());

  CONVERT_ARG_HANDLE_CHECKED(Context, context, 0);
  CONVERT_ARG_HANDLE_CHECKED(FixedArray, pairs, 1);
  CONVERT_SMI_ARG_CHECKED(flags, 2);

  // Traverse the name/value pairs and set the properties.
  int length = pairs->length();
  for (int i = 0; i < length; i += 2) {
    HandleScope scope(isolate);
    Handle<String> name(String::cast(pairs->get(i)));
    Handle<Object> initial_value(pairs->get(i + 1), isolate);

    // We have to declare a global const property. To capture we only
    // assign to it when evaluating the assignment for "const x =
    // <expr>" the initial value is the hole.
    bool is_var = initial_value->IsUndefined();
    bool is_const = initial_value->IsTheHole();
    bool is_function = initial_value->IsSharedFunctionInfo();
    DCHECK(is_var + is_const + is_function == 1);

    Handle<Object> value;
    if (is_function) {
      // Copy the function and update its context. Use it as value.
      Handle<SharedFunctionInfo> shared =
          Handle<SharedFunctionInfo>::cast(initial_value);
      Handle<JSFunction> function =
          isolate->factory()->NewFunctionFromSharedFunctionInfo(shared, context,
                                                                TENURED);
      value = function;
    } else {
      value = isolate->factory()->undefined_value();
    }

    // Compute the property attributes. According to ECMA-262,
    // the property must be non-configurable except in eval.
    bool is_native = DeclareGlobalsNativeFlag::decode(flags);
    bool is_eval = DeclareGlobalsEvalFlag::decode(flags);
    int attr = NONE;
    if (is_const) attr |= READ_ONLY;
    if (is_function && is_native) attr |= READ_ONLY;
    if (!is_const && !is_eval) attr |= DONT_DELETE;

    Object* result = DeclareGlobals(isolate, global, name, value,
                                    static_cast<PropertyAttributes>(attr),
                                    is_var, is_const, is_function);
    if (isolate->has_pending_exception()) return result;
  }

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_InitializeVarGlobal) {
  HandleScope scope(isolate);
  // args[0] == name
  // args[1] == language_mode
  // args[2] == value (optional)

  // Determine if we need to assign to the variable if it already
  // exists (based on the number of arguments).
  RUNTIME_ASSERT(args.length() == 3);

  CONVERT_ARG_HANDLE_CHECKED(String, name, 0);
  CONVERT_STRICT_MODE_ARG_CHECKED(strict_mode, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);

  Handle<GlobalObject> global(isolate->context()->global_object());
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, Object::SetProperty(global, name, value, strict_mode));
  return *result;
}


RUNTIME_FUNCTION(Runtime_InitializeConstGlobal) {
  HandleScope handle_scope(isolate);
  // All constants are declared with an initial value. The name
  // of the constant is the first argument and the initial value
  // is the second.
  RUNTIME_ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(String, name, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 1);

  Handle<GlobalObject> global = isolate->global_object();

  // Lookup the property as own on the global object.
  LookupIterator it(global, name, LookupIterator::HIDDEN_SKIP_INTERCEPTOR);
  Maybe<PropertyAttributes> maybe = JSReceiver::GetPropertyAttributes(&it);
  DCHECK(maybe.has_value);
  PropertyAttributes old_attributes = maybe.value;

  PropertyAttributes attr =
      static_cast<PropertyAttributes>(DONT_DELETE | READ_ONLY);
  // Set the value if the property is either missing, or the property attributes
  // allow setting the value without invoking an accessor.
  if (it.IsFound()) {
    // Ignore if we can't reconfigure the value.
    if ((old_attributes & DONT_DELETE) != 0) {
      if ((old_attributes & READ_ONLY) != 0 ||
          it.state() == LookupIterator::ACCESSOR) {
        return *value;
      }
      attr = static_cast<PropertyAttributes>(old_attributes | READ_ONLY);
    }
  }

  RETURN_FAILURE_ON_EXCEPTION(isolate, JSObject::SetOwnPropertyIgnoreAttributes(
                                           global, name, value, attr));

  return *value;
}


RUNTIME_FUNCTION(Runtime_DeclareLookupSlot) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 4);

  // Declarations are always made in a function, native, or global context. In
  // the case of eval code, the context passed is the context of the caller,
  // which may be some nested context and not the declaration context.
  CONVERT_ARG_HANDLE_CHECKED(Context, context_arg, 0);
  Handle<Context> context(context_arg->declaration_context());
  CONVERT_ARG_HANDLE_CHECKED(String, name, 1);
  CONVERT_SMI_ARG_CHECKED(attr_arg, 2);
  PropertyAttributes attr = static_cast<PropertyAttributes>(attr_arg);
  RUNTIME_ASSERT(attr == READ_ONLY || attr == NONE);
  CONVERT_ARG_HANDLE_CHECKED(Object, initial_value, 3);

  // TODO(verwaest): Unify the encoding indicating "var" with DeclareGlobals.
  bool is_var = *initial_value == NULL;
  bool is_const = initial_value->IsTheHole();
  bool is_function = initial_value->IsJSFunction();
  DCHECK(is_var + is_const + is_function == 1);

  int index;
  PropertyAttributes attributes;
  ContextLookupFlags flags = DONT_FOLLOW_CHAINS;
  BindingFlags binding_flags;
  Handle<Object> holder =
      context->Lookup(name, flags, &index, &attributes, &binding_flags);

  Handle<JSObject> object;
  Handle<Object> value =
      is_function ? initial_value
                  : Handle<Object>::cast(isolate->factory()->undefined_value());

  // TODO(verwaest): This case should probably not be covered by this function,
  // but by DeclareGlobals instead.
  if ((attributes != ABSENT && holder->IsJSGlobalObject()) ||
      (context_arg->has_extension() &&
       context_arg->extension()->IsJSGlobalObject())) {
    return DeclareGlobals(isolate, Handle<JSGlobalObject>::cast(holder), name,
                          value, attr, is_var, is_const, is_function);
  }

  if (attributes != ABSENT) {
    // The name was declared before; check for conflicting re-declarations.
    if (is_const || (attributes & READ_ONLY) != 0) {
      return ThrowRedeclarationError(isolate, name);
    }

    // Skip var re-declarations.
    if (is_var) return isolate->heap()->undefined_value();

    DCHECK(is_function);
    if (index >= 0) {
      DCHECK(holder.is_identical_to(context));
      context->set(index, *initial_value);
      return isolate->heap()->undefined_value();
    }

    object = Handle<JSObject>::cast(holder);

  } else if (context->has_extension()) {
    object = handle(JSObject::cast(context->extension()));
    DCHECK(object->IsJSContextExtensionObject() || object->IsJSGlobalObject());
  } else {
    DCHECK(context->IsFunctionContext());
    object =
        isolate->factory()->NewJSObject(isolate->context_extension_function());
    context->set_extension(*object);
  }

  RETURN_FAILURE_ON_EXCEPTION(isolate, JSObject::SetOwnPropertyIgnoreAttributes(
                                           object, name, value, attr));

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_InitializeLegacyConstLookupSlot) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);

  CONVERT_ARG_HANDLE_CHECKED(Object, value, 0);
  DCHECK(!value->IsTheHole());
  // Initializations are always done in a function or native context.
  CONVERT_ARG_HANDLE_CHECKED(Context, context_arg, 1);
  Handle<Context> context(context_arg->declaration_context());
  CONVERT_ARG_HANDLE_CHECKED(String, name, 2);

  int index;
  PropertyAttributes attributes;
  ContextLookupFlags flags = DONT_FOLLOW_CHAINS;
  BindingFlags binding_flags;
  Handle<Object> holder =
      context->Lookup(name, flags, &index, &attributes, &binding_flags);

  if (index >= 0) {
    DCHECK(holder->IsContext());
    // Property was found in a context.  Perform the assignment if the constant
    // was uninitialized.
    Handle<Context> context = Handle<Context>::cast(holder);
    DCHECK((attributes & READ_ONLY) != 0);
    if (context->get(index)->IsTheHole()) context->set(index, *value);
    return *value;
  }

  PropertyAttributes attr =
      static_cast<PropertyAttributes>(DONT_DELETE | READ_ONLY);

  // Strict mode handling not needed (legacy const is disallowed in strict
  // mode).

  // The declared const was configurable, and may have been deleted in the
  // meanwhile. If so, re-introduce the variable in the context extension.
  DCHECK(context_arg->has_extension());
  if (attributes == ABSENT) {
    holder = handle(context_arg->extension(), isolate);
  } else {
    // For JSContextExtensionObjects, the initializer can be run multiple times
    // if in a for loop: for (var i = 0; i < 2; i++) { const x = i; }. Only the
    // first assignment should go through. For JSGlobalObjects, additionally any
    // code can run in between that modifies the declared property.
    DCHECK(holder->IsJSGlobalObject() || holder->IsJSContextExtensionObject());

    LookupIterator it(holder, name, LookupIterator::HIDDEN_SKIP_INTERCEPTOR);
    Maybe<PropertyAttributes> maybe = JSReceiver::GetPropertyAttributes(&it);
    if (!maybe.has_value) return isolate->heap()->exception();
    PropertyAttributes old_attributes = maybe.value;

    // Ignore if we can't reconfigure the value.
    if ((old_attributes & DONT_DELETE) != 0) {
      if ((old_attributes & READ_ONLY) != 0 ||
          it.state() == LookupIterator::ACCESSOR) {
        return *value;
      }
      attr = static_cast<PropertyAttributes>(old_attributes | READ_ONLY);
    }
  }

  RETURN_FAILURE_ON_EXCEPTION(
      isolate, JSObject::SetOwnPropertyIgnoreAttributes(
                   Handle<JSObject>::cast(holder), name, value, attr));

  return *value;
}


RUNTIME_FUNCTION(Runtime_OptimizeObjectForAddingMultipleProperties) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_SMI_ARG_CHECKED(properties, 1);
  // Conservative upper limit to prevent fuzz tests from going OOM.
  RUNTIME_ASSERT(properties <= 100000);
  if (object->HasFastProperties() && !object->IsJSGlobalProxy()) {
    JSObject::NormalizeProperties(object, KEEP_INOBJECT_PROPERTIES, properties);
  }
  return *object;
}


RUNTIME_FUNCTION(Runtime_FinishArrayPrototypeSetup) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, prototype, 0);
  Object* length = prototype->length();
  RUNTIME_ASSERT(length->IsSmi() && Smi::cast(length)->value() == 0);
  RUNTIME_ASSERT(prototype->HasFastSmiOrObjectElements());
  // This is necessary to enable fast checks for absence of elements
  // on Array.prototype and below.
  prototype->set_elements(isolate->heap()->empty_fixed_array());
  return Smi::FromInt(0);
}


static void InstallBuiltin(Isolate* isolate, Handle<JSObject> holder,
                           const char* name, Builtins::Name builtin_name) {
  Handle<String> key = isolate->factory()->InternalizeUtf8String(name);
  Handle<Code> code(isolate->builtins()->builtin(builtin_name));
  Handle<JSFunction> optimized =
      isolate->factory()->NewFunctionWithoutPrototype(key, code);
  optimized->shared()->DontAdaptArguments();
  JSObject::AddProperty(holder, key, optimized, NONE);
}


RUNTIME_FUNCTION(Runtime_SpecialArrayFunctions) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0);
  Handle<JSObject> holder =
      isolate->factory()->NewJSObject(isolate->object_function());

  InstallBuiltin(isolate, holder, "pop", Builtins::kArrayPop);
  InstallBuiltin(isolate, holder, "push", Builtins::kArrayPush);
  InstallBuiltin(isolate, holder, "shift", Builtins::kArrayShift);
  InstallBuiltin(isolate, holder, "unshift", Builtins::kArrayUnshift);
  InstallBuiltin(isolate, holder, "slice", Builtins::kArraySlice);
  InstallBuiltin(isolate, holder, "splice", Builtins::kArraySplice);
  InstallBuiltin(isolate, holder, "concat", Builtins::kArrayConcat);

  return *holder;
}


RUNTIME_FUNCTION(Runtime_IsSloppyModeFunction) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(JSReceiver, callable, 0);
  if (!callable->IsJSFunction()) {
    HandleScope scope(isolate);
    Handle<Object> delegate;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, delegate, Execution::TryGetFunctionDelegate(
                               isolate, Handle<JSReceiver>(callable)));
    callable = JSFunction::cast(*delegate);
  }
  JSFunction* function = JSFunction::cast(callable);
  SharedFunctionInfo* shared = function->shared();
  return isolate->heap()->ToBoolean(shared->strict_mode() == SLOPPY);
}


RUNTIME_FUNCTION(Runtime_GetDefaultReceiver) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(JSReceiver, callable, 0);

  if (!callable->IsJSFunction()) {
    HandleScope scope(isolate);
    Handle<Object> delegate;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, delegate, Execution::TryGetFunctionDelegate(
                               isolate, Handle<JSReceiver>(callable)));
    callable = JSFunction::cast(*delegate);
  }
  JSFunction* function = JSFunction::cast(callable);

  SharedFunctionInfo* shared = function->shared();
  if (shared->native() || shared->strict_mode() == STRICT) {
    return isolate->heap()->undefined_value();
  }
  // Returns undefined for strict or native functions, or
  // the associated global receiver for "normal" functions.

  return function->global_proxy();
}


RUNTIME_FUNCTION(Runtime_FunctionGetName) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);

  CONVERT_ARG_CHECKED(JSFunction, f, 0);
  return f->shared()->name();
}


RUNTIME_FUNCTION(Runtime_FunctionSetName) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 2);

  CONVERT_ARG_CHECKED(JSFunction, f, 0);
  CONVERT_ARG_CHECKED(String, name, 1);
  f->shared()->set_name(name);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_FunctionNameShouldPrintAsAnonymous) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(JSFunction, f, 0);
  return isolate->heap()->ToBoolean(
      f->shared()->name_should_print_as_anonymous());
}


RUNTIME_FUNCTION(Runtime_FunctionMarkNameShouldPrintAsAnonymous) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(JSFunction, f, 0);
  f->shared()->set_name_should_print_as_anonymous(true);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_FunctionIsArrow) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(JSFunction, f, 0);
  return isolate->heap()->ToBoolean(f->shared()->is_arrow());
}


RUNTIME_FUNCTION(Runtime_FunctionIsConciseMethod) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(JSFunction, f, 0);
  return isolate->heap()->ToBoolean(f->shared()->is_concise_method());
}


RUNTIME_FUNCTION(Runtime_FunctionRemovePrototype) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);

  CONVERT_ARG_CHECKED(JSFunction, f, 0);
  RUNTIME_ASSERT(f->RemovePrototype());

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_FunctionGetScript) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);

  CONVERT_ARG_CHECKED(JSFunction, fun, 0);
  Handle<Object> script = Handle<Object>(fun->shared()->script(), isolate);
  if (!script->IsScript()) return isolate->heap()->undefined_value();

  return *Script::GetWrapper(Handle<Script>::cast(script));
}


RUNTIME_FUNCTION(Runtime_FunctionGetSourceCode) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);

  CONVERT_ARG_HANDLE_CHECKED(JSFunction, f, 0);
  Handle<SharedFunctionInfo> shared(f->shared());
  return *shared->GetSourceCode();
}


RUNTIME_FUNCTION(Runtime_FunctionGetScriptSourcePosition) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);

  CONVERT_ARG_CHECKED(JSFunction, fun, 0);
  int pos = fun->shared()->start_position();
  return Smi::FromInt(pos);
}


RUNTIME_FUNCTION(Runtime_FunctionGetPositionForOffset) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 2);

  CONVERT_ARG_CHECKED(Code, code, 0);
  CONVERT_NUMBER_CHECKED(int, offset, Int32, args[1]);

  RUNTIME_ASSERT(0 <= offset && offset < code->Size());

  Address pc = code->address() + offset;
  return Smi::FromInt(code->SourcePosition(pc));
}


RUNTIME_FUNCTION(Runtime_FunctionSetInstanceClassName) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 2);

  CONVERT_ARG_CHECKED(JSFunction, fun, 0);
  CONVERT_ARG_CHECKED(String, name, 1);
  fun->SetInstanceClassName(name);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_FunctionSetLength) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 2);

  CONVERT_ARG_CHECKED(JSFunction, fun, 0);
  CONVERT_SMI_ARG_CHECKED(length, 1);
  RUNTIME_ASSERT((length & 0xC0000000) == 0xC0000000 ||
                 (length & 0xC0000000) == 0x0);
  fun->shared()->set_length(length);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_FunctionSetPrototype) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(JSFunction, fun, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 1);
  RUNTIME_ASSERT(fun->should_have_prototype());
  Accessors::FunctionSetPrototype(fun, value);
  return args[0];  // return TOS
}


RUNTIME_FUNCTION(Runtime_FunctionIsAPIFunction) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);

  CONVERT_ARG_CHECKED(JSFunction, f, 0);
  return isolate->heap()->ToBoolean(f->shared()->IsApiFunction());
}


RUNTIME_FUNCTION(Runtime_FunctionIsBuiltin) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);

  CONVERT_ARG_CHECKED(JSFunction, f, 0);
  return isolate->heap()->ToBoolean(f->IsBuiltin());
}


RUNTIME_FUNCTION(Runtime_SetCode) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(JSFunction, target, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, source, 1);

  Handle<SharedFunctionInfo> target_shared(target->shared());
  Handle<SharedFunctionInfo> source_shared(source->shared());
  RUNTIME_ASSERT(!source_shared->bound());

  if (!Compiler::EnsureCompiled(source, KEEP_EXCEPTION)) {
    return isolate->heap()->exception();
  }

  // Mark both, the source and the target, as un-flushable because the
  // shared unoptimized code makes them impossible to enqueue in a list.
  DCHECK(target_shared->code()->gc_metadata() == NULL);
  DCHECK(source_shared->code()->gc_metadata() == NULL);
  target_shared->set_dont_flush(true);
  source_shared->set_dont_flush(true);

  // Set the code, scope info, formal parameter count, and the length
  // of the target shared function info.
  target_shared->ReplaceCode(source_shared->code());
  target_shared->set_scope_info(source_shared->scope_info());
  target_shared->set_length(source_shared->length());
  target_shared->set_feedback_vector(source_shared->feedback_vector());
  target_shared->set_formal_parameter_count(
      source_shared->formal_parameter_count());
  target_shared->set_script(source_shared->script());
  target_shared->set_start_position_and_type(
      source_shared->start_position_and_type());
  target_shared->set_end_position(source_shared->end_position());
  bool was_native = target_shared->native();
  target_shared->set_compiler_hints(source_shared->compiler_hints());
  target_shared->set_native(was_native);
  target_shared->set_profiler_ticks(source_shared->profiler_ticks());

  // Set the code of the target function.
  target->ReplaceCode(source_shared->code());
  DCHECK(target->next_function_link()->IsUndefined());

  // Make sure we get a fresh copy of the literal vector to avoid cross
  // context contamination.
  Handle<Context> context(source->context());
  int number_of_literals = source->NumberOfLiterals();
  Handle<FixedArray> literals =
      isolate->factory()->NewFixedArray(number_of_literals, TENURED);
  if (number_of_literals > 0) {
    literals->set(JSFunction::kLiteralNativeContextIndex,
                  context->native_context());
  }
  target->set_context(*context);
  target->set_literals(*literals);

  if (isolate->logger()->is_logging_code_events() ||
      isolate->cpu_profiler()->is_profiling()) {
    isolate->logger()->LogExistingFunction(source_shared,
                                           Handle<Code>(source_shared->code()));
  }

  return *target;
}


RUNTIME_FUNCTION(Runtime_ObjectFreeze) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);

  // %ObjectFreeze is a fast path and these cases are handled elsewhere.
  RUNTIME_ASSERT(!object->HasSloppyArgumentsElements() &&
                 !object->map()->is_observed() && !object->IsJSProxy());

  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result, JSObject::Freeze(object));
  return *result;
}


// Returns a single character string where first character equals
// string->Get(index).
static Handle<Object> GetCharAt(Handle<String> string, uint32_t index) {
  if (index < static_cast<uint32_t>(string->length())) {
    Factory* factory = string->GetIsolate()->factory();
    return factory->LookupSingleCharacterStringFromCode(
        String::Flatten(string)->Get(index));
  }
  return Execution::CharAt(string, index);
}


MaybeHandle<Object> Runtime::GetElementOrCharAt(Isolate* isolate,
                                                Handle<Object> object,
                                                uint32_t index) {
  // Handle [] indexing on Strings
  if (object->IsString()) {
    Handle<Object> result = GetCharAt(Handle<String>::cast(object), index);
    if (!result->IsUndefined()) return result;
  }

  // Handle [] indexing on String objects
  if (object->IsStringObjectWithCharacterAt(index)) {
    Handle<JSValue> js_value = Handle<JSValue>::cast(object);
    Handle<Object> result =
        GetCharAt(Handle<String>(String::cast(js_value->value())), index);
    if (!result->IsUndefined()) return result;
  }

  Handle<Object> result;
  if (object->IsString() || object->IsNumber() || object->IsBoolean()) {
    PrototypeIterator iter(isolate, object);
    return Object::GetElement(isolate, PrototypeIterator::GetCurrent(iter),
                              index);
  } else {
    return Object::GetElement(isolate, object, index);
  }
}


MUST_USE_RESULT
static MaybeHandle<Name> ToName(Isolate* isolate, Handle<Object> key) {
  if (key->IsName()) {
    return Handle<Name>::cast(key);
  } else {
    Handle<Object> converted;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, converted,
                               Execution::ToString(isolate, key), Name);
    return Handle<Name>::cast(converted);
  }
}


MaybeHandle<Object> Runtime::HasObjectProperty(Isolate* isolate,
                                               Handle<JSReceiver> object,
                                               Handle<Object> key) {
  Maybe<bool> maybe;
  // Check if the given key is an array index.
  uint32_t index;
  if (key->ToArrayIndex(&index)) {
    maybe = JSReceiver::HasElement(object, index);
  } else {
    // Convert the key to a name - possibly by calling back into JavaScript.
    Handle<Name> name;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, name, ToName(isolate, key), Object);

    maybe = JSReceiver::HasProperty(object, name);
  }

  if (!maybe.has_value) return MaybeHandle<Object>();
  return isolate->factory()->ToBoolean(maybe.value);
}


MaybeHandle<Object> Runtime::GetObjectProperty(Isolate* isolate,
                                               Handle<Object> object,
                                               Handle<Object> key) {
  if (object->IsUndefined() || object->IsNull()) {
    Handle<Object> args[2] = {key, object};
    THROW_NEW_ERROR(isolate, NewTypeError("non_object_property_load",
                                          HandleVector(args, 2)),
                    Object);
  }

  // Check if the given key is an array index.
  uint32_t index;
  if (key->ToArrayIndex(&index)) {
    return GetElementOrCharAt(isolate, object, index);
  }

  // Convert the key to a name - possibly by calling back into JavaScript.
  Handle<Name> name;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, name, ToName(isolate, key), Object);

  // Check if the name is trivially convertible to an index and get
  // the element if so.
  if (name->AsArrayIndex(&index)) {
    return GetElementOrCharAt(isolate, object, index);
  } else {
    return Object::GetProperty(object, name);
  }
}


RUNTIME_FUNCTION(Runtime_GetProperty) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, Runtime::GetObjectProperty(isolate, object, key));
  return *result;
}


// KeyedGetProperty is called from KeyedLoadIC::GenerateGeneric.
RUNTIME_FUNCTION(Runtime_KeyedGetProperty) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(Object, receiver_obj, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key_obj, 1);

  // Fast cases for getting named properties of the receiver JSObject
  // itself.
  //
  // The global proxy objects has to be excluded since LookupOwn on
  // the global proxy object can return a valid result even though the
  // global proxy object never has properties.  This is the case
  // because the global proxy object forwards everything to its hidden
  // prototype including own lookups.
  //
  // Additionally, we need to make sure that we do not cache results
  // for objects that require access checks.
  if (receiver_obj->IsJSObject()) {
    if (!receiver_obj->IsJSGlobalProxy() &&
        !receiver_obj->IsAccessCheckNeeded() && key_obj->IsName()) {
      DisallowHeapAllocation no_allocation;
      Handle<JSObject> receiver = Handle<JSObject>::cast(receiver_obj);
      Handle<Name> key = Handle<Name>::cast(key_obj);
      if (receiver->HasFastProperties()) {
        // Attempt to use lookup cache.
        Handle<Map> receiver_map(receiver->map(), isolate);
        KeyedLookupCache* keyed_lookup_cache = isolate->keyed_lookup_cache();
        int index = keyed_lookup_cache->Lookup(receiver_map, key);
        if (index != -1) {
          // Doubles are not cached, so raw read the value.
          return receiver->RawFastPropertyAt(
              FieldIndex::ForKeyedLookupCacheIndex(*receiver_map, index));
        }
        // Lookup cache miss.  Perform lookup and update the cache if
        // appropriate.
        LookupIterator it(receiver, key, LookupIterator::OWN);
        if (it.state() == LookupIterator::DATA &&
            it.property_details().type() == FIELD) {
          FieldIndex field_index = it.GetFieldIndex();
          // Do not track double fields in the keyed lookup cache. Reading
          // double values requires boxing.
          if (!it.representation().IsDouble()) {
            keyed_lookup_cache->Update(receiver_map, key,
                                       field_index.GetKeyedLookupCacheIndex());
          }
          AllowHeapAllocation allow_allocation;
          return *JSObject::FastPropertyAt(receiver, it.representation(),
                                           field_index);
        }
      } else {
        // Attempt dictionary lookup.
        NameDictionary* dictionary = receiver->property_dictionary();
        int entry = dictionary->FindEntry(key);
        if ((entry != NameDictionary::kNotFound) &&
            (dictionary->DetailsAt(entry).type() == NORMAL)) {
          Object* value = dictionary->ValueAt(entry);
          if (!receiver->IsGlobalObject()) return value;
          value = PropertyCell::cast(value)->value();
          if (!value->IsTheHole()) return value;
          // If value is the hole (meaning, absent) do the general lookup.
        }
      }
    } else if (key_obj->IsSmi()) {
      // JSObject without a name key. If the key is a Smi, check for a
      // definite out-of-bounds access to elements, which is a strong indicator
      // that subsequent accesses will also call the runtime. Proactively
      // transition elements to FAST_*_ELEMENTS to avoid excessive boxing of
      // doubles for those future calls in the case that the elements would
      // become FAST_DOUBLE_ELEMENTS.
      Handle<JSObject> js_object = Handle<JSObject>::cast(receiver_obj);
      ElementsKind elements_kind = js_object->GetElementsKind();
      if (IsFastDoubleElementsKind(elements_kind)) {
        Handle<Smi> key = Handle<Smi>::cast(key_obj);
        if (key->value() >= js_object->elements()->length()) {
          if (IsFastHoleyElementsKind(elements_kind)) {
            elements_kind = FAST_HOLEY_ELEMENTS;
          } else {
            elements_kind = FAST_ELEMENTS;
          }
          RETURN_FAILURE_ON_EXCEPTION(
              isolate, TransitionElements(js_object, elements_kind, isolate));
        }
      } else {
        DCHECK(IsFastSmiOrObjectElementsKind(elements_kind) ||
               !IsFastElementsKind(elements_kind));
      }
    }
  } else if (receiver_obj->IsString() && key_obj->IsSmi()) {
    // Fast case for string indexing using [] with a smi index.
    Handle<String> str = Handle<String>::cast(receiver_obj);
    int index = args.smi_at(1);
    if (index >= 0 && index < str->length()) {
      return *GetCharAt(str, index);
    }
  }

  // Fall back to GetObjectProperty.
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      Runtime::GetObjectProperty(isolate, receiver_obj, key_obj));
  return *result;
}


static bool IsValidAccessor(Handle<Object> obj) {
  return obj->IsUndefined() || obj->IsSpecFunction() || obj->IsNull();
}


// Transform getter or setter into something DefineAccessor can handle.
static Handle<Object> InstantiateAccessorComponent(Isolate* isolate,
                                                   Handle<Object> component) {
  if (component->IsUndefined()) return isolate->factory()->undefined_value();
  Handle<FunctionTemplateInfo> info =
      Handle<FunctionTemplateInfo>::cast(component);
  return Utils::OpenHandle(*Utils::ToLocal(info)->GetFunction());
}


RUNTIME_FUNCTION(Runtime_DefineApiAccessorProperty) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 5);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, getter, 2);
  CONVERT_ARG_HANDLE_CHECKED(Object, setter, 3);
  CONVERT_SMI_ARG_CHECKED(attribute, 4);
  RUNTIME_ASSERT(getter->IsUndefined() || getter->IsFunctionTemplateInfo());
  RUNTIME_ASSERT(setter->IsUndefined() || setter->IsFunctionTemplateInfo());
  RUNTIME_ASSERT(PropertyDetails::AttributesField::is_valid(
      static_cast<PropertyAttributes>(attribute)));
  RETURN_FAILURE_ON_EXCEPTION(
      isolate, JSObject::DefineAccessor(
                   object, name, InstantiateAccessorComponent(isolate, getter),
                   InstantiateAccessorComponent(isolate, setter),
                   static_cast<PropertyAttributes>(attribute)));
  return isolate->heap()->undefined_value();
}


// Implements part of 8.12.9 DefineOwnProperty.
// There are 3 cases that lead here:
// Step 4b - define a new accessor property.
// Steps 9c & 12 - replace an existing data property with an accessor property.
// Step 12 - update an existing accessor property with an accessor or generic
//           descriptor.
RUNTIME_FUNCTION(Runtime_DefineAccessorPropertyUnchecked) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 5);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);
  RUNTIME_ASSERT(!obj->IsNull());
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, getter, 2);
  RUNTIME_ASSERT(IsValidAccessor(getter));
  CONVERT_ARG_HANDLE_CHECKED(Object, setter, 3);
  RUNTIME_ASSERT(IsValidAccessor(setter));
  CONVERT_SMI_ARG_CHECKED(unchecked, 4);
  RUNTIME_ASSERT((unchecked & ~(READ_ONLY | DONT_ENUM | DONT_DELETE)) == 0);
  PropertyAttributes attr = static_cast<PropertyAttributes>(unchecked);

  bool fast = obj->HasFastProperties();
  RETURN_FAILURE_ON_EXCEPTION(
      isolate, JSObject::DefineAccessor(obj, name, getter, setter, attr));
  if (fast) JSObject::MigrateSlowToFast(obj, 0);
  return isolate->heap()->undefined_value();
}


// Implements part of 8.12.9 DefineOwnProperty.
// There are 3 cases that lead here:
// Step 4a - define a new data property.
// Steps 9b & 12 - replace an existing accessor property with a data property.
// Step 12 - update an existing data property with a data or generic
//           descriptor.
RUNTIME_FUNCTION(Runtime_DefineDataPropertyUnchecked) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, js_object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, obj_value, 2);
  CONVERT_SMI_ARG_CHECKED(unchecked, 3);
  RUNTIME_ASSERT((unchecked & ~(READ_ONLY | DONT_ENUM | DONT_DELETE)) == 0);
  PropertyAttributes attr = static_cast<PropertyAttributes>(unchecked);

  LookupIterator it(js_object, name, LookupIterator::OWN_SKIP_INTERCEPTOR);
  if (it.IsFound() && it.state() == LookupIterator::ACCESS_CHECK) {
    if (!isolate->MayNamedAccess(js_object, name, v8::ACCESS_SET)) {
      return isolate->heap()->undefined_value();
    }
    it.Next();
  }

  // Take special care when attributes are different and there is already
  // a property.
  if (it.state() == LookupIterator::ACCESSOR) {
    // Use IgnoreAttributes version since a readonly property may be
    // overridden and SetProperty does not allow this.
    Handle<Object> result;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, result,
        JSObject::SetOwnPropertyIgnoreAttributes(
            js_object, name, obj_value, attr, JSObject::DONT_FORCE_FIELD));
    return *result;
  }

  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      Runtime::DefineObjectProperty(js_object, name, obj_value, attr));
  return *result;
}


// Return property without being observable by accessors or interceptors.
RUNTIME_FUNCTION(Runtime_GetDataProperty) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, key, 1);
  return *JSObject::GetDataProperty(object, key);
}


MaybeHandle<Object> Runtime::SetObjectProperty(Isolate* isolate,
                                               Handle<Object> object,
                                               Handle<Object> key,
                                               Handle<Object> value,
                                               StrictMode strict_mode) {
  if (object->IsUndefined() || object->IsNull()) {
    Handle<Object> args[2] = {key, object};
    THROW_NEW_ERROR(isolate, NewTypeError("non_object_property_store",
                                          HandleVector(args, 2)),
                    Object);
  }

  if (object->IsJSProxy()) {
    Handle<Object> name_object;
    if (key->IsSymbol()) {
      name_object = key;
    } else {
      ASSIGN_RETURN_ON_EXCEPTION(isolate, name_object,
                                 Execution::ToString(isolate, key), Object);
    }
    Handle<Name> name = Handle<Name>::cast(name_object);
    return Object::SetProperty(Handle<JSProxy>::cast(object), name, value,
                               strict_mode);
  }

  // Check if the given key is an array index.
  uint32_t index;
  if (key->ToArrayIndex(&index)) {
    // TODO(verwaest): Support non-JSObject receivers.
    if (!object->IsJSObject()) return value;
    Handle<JSObject> js_object = Handle<JSObject>::cast(object);

    // In Firefox/SpiderMonkey, Safari and Opera you can access the characters
    // of a string using [] notation.  We need to support this too in
    // JavaScript.
    // In the case of a String object we just need to redirect the assignment to
    // the underlying string if the index is in range.  Since the underlying
    // string does nothing with the assignment then we can ignore such
    // assignments.
    if (js_object->IsStringObjectWithCharacterAt(index)) {
      return value;
    }

    JSObject::ValidateElements(js_object);
    if (js_object->HasExternalArrayElements() ||
        js_object->HasFixedTypedArrayElements()) {
      if (!value->IsNumber() && !value->IsUndefined()) {
        ASSIGN_RETURN_ON_EXCEPTION(isolate, value,
                                   Execution::ToNumber(isolate, value), Object);
      }
    }

    MaybeHandle<Object> result = JSObject::SetElement(
        js_object, index, value, NONE, strict_mode, true, SET_PROPERTY);
    JSObject::ValidateElements(js_object);

    return result.is_null() ? result : value;
  }

  if (key->IsName()) {
    Handle<Name> name = Handle<Name>::cast(key);
    if (name->AsArrayIndex(&index)) {
      // TODO(verwaest): Support non-JSObject receivers.
      if (!object->IsJSObject()) return value;
      Handle<JSObject> js_object = Handle<JSObject>::cast(object);
      if (js_object->HasExternalArrayElements()) {
        if (!value->IsNumber() && !value->IsUndefined()) {
          ASSIGN_RETURN_ON_EXCEPTION(
              isolate, value, Execution::ToNumber(isolate, value), Object);
        }
      }
      return JSObject::SetElement(js_object, index, value, NONE, strict_mode,
                                  true, SET_PROPERTY);
    } else {
      if (name->IsString()) name = String::Flatten(Handle<String>::cast(name));
      return Object::SetProperty(object, name, value, strict_mode);
    }
  }

  // Call-back into JavaScript to convert the key to a string.
  Handle<Object> converted;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, converted,
                             Execution::ToString(isolate, key), Object);
  Handle<String> name = Handle<String>::cast(converted);

  if (name->AsArrayIndex(&index)) {
    // TODO(verwaest): Support non-JSObject receivers.
    if (!object->IsJSObject()) return value;
    Handle<JSObject> js_object = Handle<JSObject>::cast(object);
    return JSObject::SetElement(js_object, index, value, NONE, strict_mode,
                                true, SET_PROPERTY);
  }
  return Object::SetProperty(object, name, value, strict_mode);
}


MaybeHandle<Object> Runtime::DefineObjectProperty(Handle<JSObject> js_object,
                                                  Handle<Object> key,
                                                  Handle<Object> value,
                                                  PropertyAttributes attr) {
  Isolate* isolate = js_object->GetIsolate();
  // Check if the given key is an array index.
  uint32_t index;
  if (key->ToArrayIndex(&index)) {
    // In Firefox/SpiderMonkey, Safari and Opera you can access the characters
    // of a string using [] notation.  We need to support this too in
    // JavaScript.
    // In the case of a String object we just need to redirect the assignment to
    // the underlying string if the index is in range.  Since the underlying
    // string does nothing with the assignment then we can ignore such
    // assignments.
    if (js_object->IsStringObjectWithCharacterAt(index)) {
      return value;
    }

    return JSObject::SetElement(js_object, index, value, attr, SLOPPY, false,
                                DEFINE_PROPERTY);
  }

  if (key->IsName()) {
    Handle<Name> name = Handle<Name>::cast(key);
    if (name->AsArrayIndex(&index)) {
      return JSObject::SetElement(js_object, index, value, attr, SLOPPY, false,
                                  DEFINE_PROPERTY);
    } else {
      if (name->IsString()) name = String::Flatten(Handle<String>::cast(name));
      return JSObject::SetOwnPropertyIgnoreAttributes(js_object, name, value,
                                                      attr);
    }
  }

  // Call-back into JavaScript to convert the key to a string.
  Handle<Object> converted;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, converted,
                             Execution::ToString(isolate, key), Object);
  Handle<String> name = Handle<String>::cast(converted);

  if (name->AsArrayIndex(&index)) {
    return JSObject::SetElement(js_object, index, value, attr, SLOPPY, false,
                                DEFINE_PROPERTY);
  } else {
    return JSObject::SetOwnPropertyIgnoreAttributes(js_object, name, value,
                                                    attr);
  }
}


MaybeHandle<Object> Runtime::DeleteObjectProperty(Isolate* isolate,
                                                  Handle<JSReceiver> receiver,
                                                  Handle<Object> key,
                                                  JSReceiver::DeleteMode mode) {
  // Check if the given key is an array index.
  uint32_t index;
  if (key->ToArrayIndex(&index)) {
    // In Firefox/SpiderMonkey, Safari and Opera you can access the
    // characters of a string using [] notation.  In the case of a
    // String object we just need to redirect the deletion to the
    // underlying string if the index is in range.  Since the
    // underlying string does nothing with the deletion, we can ignore
    // such deletions.
    if (receiver->IsStringObjectWithCharacterAt(index)) {
      return isolate->factory()->true_value();
    }

    return JSReceiver::DeleteElement(receiver, index, mode);
  }

  Handle<Name> name;
  if (key->IsName()) {
    name = Handle<Name>::cast(key);
  } else {
    // Call-back into JavaScript to convert the key to a string.
    Handle<Object> converted;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, converted,
                               Execution::ToString(isolate, key), Object);
    name = Handle<String>::cast(converted);
  }

  if (name->IsString()) name = String::Flatten(Handle<String>::cast(name));
  return JSReceiver::DeleteProperty(receiver, name, mode);
}


RUNTIME_FUNCTION(Runtime_SetHiddenProperty) {
  HandleScope scope(isolate);
  RUNTIME_ASSERT(args.length() == 3);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, key, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  RUNTIME_ASSERT(key->IsUniqueName());
  return *JSObject::SetHiddenProperty(object, key, value);
}


RUNTIME_FUNCTION(Runtime_AddNamedProperty) {
  HandleScope scope(isolate);
  RUNTIME_ASSERT(args.length() == 4);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, key, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  CONVERT_SMI_ARG_CHECKED(unchecked_attributes, 3);
  RUNTIME_ASSERT(
      (unchecked_attributes & ~(READ_ONLY | DONT_ENUM | DONT_DELETE)) == 0);
  // Compute attributes.
  PropertyAttributes attributes =
      static_cast<PropertyAttributes>(unchecked_attributes);

#ifdef DEBUG
  uint32_t index = 0;
  DCHECK(!key->ToArrayIndex(&index));
  LookupIterator it(object, key, LookupIterator::OWN_SKIP_INTERCEPTOR);
  Maybe<PropertyAttributes> maybe = JSReceiver::GetPropertyAttributes(&it);
  if (!maybe.has_value) return isolate->heap()->exception();
  RUNTIME_ASSERT(!it.IsFound());
#endif

  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      JSObject::SetOwnPropertyIgnoreAttributes(object, key, value, attributes));
  return *result;
}


RUNTIME_FUNCTION(Runtime_AddPropertyForTemplate) {
  HandleScope scope(isolate);
  RUNTIME_ASSERT(args.length() == 4);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  CONVERT_SMI_ARG_CHECKED(unchecked_attributes, 3);
  RUNTIME_ASSERT(
      (unchecked_attributes & ~(READ_ONLY | DONT_ENUM | DONT_DELETE)) == 0);
  // Compute attributes.
  PropertyAttributes attributes =
      static_cast<PropertyAttributes>(unchecked_attributes);

#ifdef DEBUG
  bool duplicate;
  if (key->IsName()) {
    LookupIterator it(object, Handle<Name>::cast(key),
                      LookupIterator::OWN_SKIP_INTERCEPTOR);
    Maybe<PropertyAttributes> maybe = JSReceiver::GetPropertyAttributes(&it);
    DCHECK(maybe.has_value);
    duplicate = it.IsFound();
  } else {
    uint32_t index = 0;
    RUNTIME_ASSERT(key->ToArrayIndex(&index));
    Maybe<bool> maybe = JSReceiver::HasOwnElement(object, index);
    if (!maybe.has_value) return isolate->heap()->exception();
    duplicate = maybe.value;
  }
  if (duplicate) {
    Handle<Object> args[1] = {key};
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate,
        NewTypeError("duplicate_template_property", HandleVector(args, 1)));
  }
#endif

  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      Runtime::DefineObjectProperty(object, key, value, attributes));
  return *result;
}


RUNTIME_FUNCTION(Runtime_SetProperty) {
  HandleScope scope(isolate);
  RUNTIME_ASSERT(args.length() == 4);

  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  CONVERT_STRICT_MODE_ARG_CHECKED(strict_mode_arg, 3);
  StrictMode strict_mode = strict_mode_arg;

  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      Runtime::SetObjectProperty(isolate, object, key, value, strict_mode));
  return *result;
}


// Adds an element to an array.
// This is used to create an indexed data property into an array.
RUNTIME_FUNCTION(Runtime_AddElement) {
  HandleScope scope(isolate);
  RUNTIME_ASSERT(args.length() == 4);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  CONVERT_SMI_ARG_CHECKED(unchecked_attributes, 3);
  RUNTIME_ASSERT(
      (unchecked_attributes & ~(READ_ONLY | DONT_ENUM | DONT_DELETE)) == 0);
  // Compute attributes.
  PropertyAttributes attributes =
      static_cast<PropertyAttributes>(unchecked_attributes);

  uint32_t index = 0;
  key->ToArrayIndex(&index);

  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, JSObject::SetElement(object, index, value, attributes,
                                            SLOPPY, false, DEFINE_PROPERTY));
  return *result;
}


RUNTIME_FUNCTION(Runtime_TransitionElementsKind) {
  HandleScope scope(isolate);
  RUNTIME_ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, array, 0);
  CONVERT_ARG_HANDLE_CHECKED(Map, map, 1);
  JSObject::TransitionElementsKind(array, map->elements_kind());
  return *array;
}


// Set the native flag on the function.
// This is used to decide if we should transform null and undefined
// into the global object when doing call and apply.
RUNTIME_FUNCTION(Runtime_SetNativeFlag) {
  SealHandleScope shs(isolate);
  RUNTIME_ASSERT(args.length() == 1);

  CONVERT_ARG_CHECKED(Object, object, 0);

  if (object->IsJSFunction()) {
    JSFunction* func = JSFunction::cast(object);
    func->shared()->set_native(true);
  }
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_SetInlineBuiltinFlag) {
  SealHandleScope shs(isolate);
  RUNTIME_ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);

  if (object->IsJSFunction()) {
    JSFunction* func = JSFunction::cast(*object);
    func->shared()->set_inline_builtin(true);
  }
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_StoreArrayLiteralElement) {
  HandleScope scope(isolate);
  RUNTIME_ASSERT(args.length() == 5);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_SMI_ARG_CHECKED(store_index, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  CONVERT_ARG_HANDLE_CHECKED(FixedArray, literals, 3);
  CONVERT_SMI_ARG_CHECKED(literal_index, 4);

  Object* raw_literal_cell = literals->get(literal_index);
  JSArray* boilerplate = NULL;
  if (raw_literal_cell->IsAllocationSite()) {
    AllocationSite* site = AllocationSite::cast(raw_literal_cell);
    boilerplate = JSArray::cast(site->transition_info());
  } else {
    boilerplate = JSArray::cast(raw_literal_cell);
  }
  Handle<JSArray> boilerplate_object(boilerplate);
  ElementsKind elements_kind = object->GetElementsKind();
  DCHECK(IsFastElementsKind(elements_kind));
  // Smis should never trigger transitions.
  DCHECK(!value->IsSmi());

  if (value->IsNumber()) {
    DCHECK(IsFastSmiElementsKind(elements_kind));
    ElementsKind transitioned_kind = IsFastHoleyElementsKind(elements_kind)
                                         ? FAST_HOLEY_DOUBLE_ELEMENTS
                                         : FAST_DOUBLE_ELEMENTS;
    if (IsMoreGeneralElementsKindTransition(
            boilerplate_object->GetElementsKind(), transitioned_kind)) {
      JSObject::TransitionElementsKind(boilerplate_object, transitioned_kind);
    }
    JSObject::TransitionElementsKind(object, transitioned_kind);
    DCHECK(IsFastDoubleElementsKind(object->GetElementsKind()));
    FixedDoubleArray* double_array = FixedDoubleArray::cast(object->elements());
    HeapNumber* number = HeapNumber::cast(*value);
    double_array->set(store_index, number->Number());
  } else {
    if (!IsFastObjectElementsKind(elements_kind)) {
      ElementsKind transitioned_kind = IsFastHoleyElementsKind(elements_kind)
                                           ? FAST_HOLEY_ELEMENTS
                                           : FAST_ELEMENTS;
      JSObject::TransitionElementsKind(object, transitioned_kind);
      ElementsKind boilerplate_elements_kind =
          boilerplate_object->GetElementsKind();
      if (IsMoreGeneralElementsKindTransition(boilerplate_elements_kind,
                                              transitioned_kind)) {
        JSObject::TransitionElementsKind(boilerplate_object, transitioned_kind);
      }
    }
    FixedArray* object_array = FixedArray::cast(object->elements());
    object_array->set(store_index, *value);
  }
  return *object;
}


RUNTIME_FUNCTION(Runtime_DebugPromiseRejectEvent) {
  DCHECK(args.length() == 2);
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, promise, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 1);
  isolate->debug()->OnPromiseReject(promise, value);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_DeleteProperty) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, key, 1);
  CONVERT_STRICT_MODE_ARG_CHECKED(strict_mode, 2);
  JSReceiver::DeleteMode delete_mode = strict_mode == STRICT
                                           ? JSReceiver::STRICT_DELETION
                                           : JSReceiver::NORMAL_DELETION;
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, JSReceiver::DeleteProperty(object, key, delete_mode));
  return *result;
}


static Object* HasOwnPropertyImplementation(Isolate* isolate,
                                            Handle<JSObject> object,
                                            Handle<Name> key) {
  Maybe<bool> maybe = JSReceiver::HasOwnProperty(object, key);
  if (!maybe.has_value) return isolate->heap()->exception();
  if (maybe.value) return isolate->heap()->true_value();
  // Handle hidden prototypes.  If there's a hidden prototype above this thing
  // then we have to check it for properties, because they are supposed to
  // look like they are on this object.
  PrototypeIterator iter(isolate, object);
  if (!iter.IsAtEnd() &&
      Handle<JSObject>::cast(PrototypeIterator::GetCurrent(iter))
          ->map()
          ->is_hidden_prototype()) {
    // TODO(verwaest): The recursion is not necessary for keys that are array
    // indices. Removing this.
    return HasOwnPropertyImplementation(
        isolate, Handle<JSObject>::cast(PrototypeIterator::GetCurrent(iter)),
        key);
  }
  RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
  return isolate->heap()->false_value();
}


RUNTIME_FUNCTION(Runtime_HasOwnProperty) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0)
  CONVERT_ARG_HANDLE_CHECKED(Name, key, 1);

  uint32_t index;
  const bool key_is_array_index = key->AsArrayIndex(&index);

  // Only JS objects can have properties.
  if (object->IsJSObject()) {
    Handle<JSObject> js_obj = Handle<JSObject>::cast(object);
    // Fast case: either the key is a real named property or it is not
    // an array index and there are no interceptors or hidden
    // prototypes.
    Maybe<bool> maybe = JSObject::HasRealNamedProperty(js_obj, key);
    if (!maybe.has_value) return isolate->heap()->exception();
    DCHECK(!isolate->has_pending_exception());
    if (maybe.value) {
      return isolate->heap()->true_value();
    }
    Map* map = js_obj->map();
    if (!key_is_array_index && !map->has_named_interceptor() &&
        !HeapObject::cast(map->prototype())->map()->is_hidden_prototype()) {
      return isolate->heap()->false_value();
    }
    // Slow case.
    return HasOwnPropertyImplementation(isolate, Handle<JSObject>(js_obj),
                                        Handle<Name>(key));
  } else if (object->IsString() && key_is_array_index) {
    // Well, there is one exception:  Handle [] on strings.
    Handle<String> string = Handle<String>::cast(object);
    if (index < static_cast<uint32_t>(string->length())) {
      return isolate->heap()->true_value();
    }
  }
  return isolate->heap()->false_value();
}


RUNTIME_FUNCTION(Runtime_HasProperty) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, receiver, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, key, 1);

  Maybe<bool> maybe = JSReceiver::HasProperty(receiver, key);
  if (!maybe.has_value) return isolate->heap()->exception();
  return isolate->heap()->ToBoolean(maybe.value);
}


RUNTIME_FUNCTION(Runtime_HasElement) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, receiver, 0);
  CONVERT_SMI_ARG_CHECKED(index, 1);

  Maybe<bool> maybe = JSReceiver::HasElement(receiver, index);
  if (!maybe.has_value) return isolate->heap()->exception();
  return isolate->heap()->ToBoolean(maybe.value);
}


RUNTIME_FUNCTION(Runtime_IsPropertyEnumerable) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, key, 1);

  Maybe<PropertyAttributes> maybe =
      JSReceiver::GetOwnPropertyAttributes(object, key);
  if (!maybe.has_value) return isolate->heap()->exception();
  if (maybe.value == ABSENT) maybe.value = DONT_ENUM;
  return isolate->heap()->ToBoolean((maybe.value & DONT_ENUM) == 0);
}


RUNTIME_FUNCTION(Runtime_GetPropertyNames) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, object, 0);
  Handle<JSArray> result;

  isolate->counters()->for_in()->Increment();
  Handle<FixedArray> elements;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, elements,
      JSReceiver::GetKeys(object, JSReceiver::INCLUDE_PROTOS));
  return *isolate->factory()->NewJSArrayWithElements(elements);
}


// Returns either a FixedArray as Runtime_GetPropertyNames,
// or, if the given object has an enum cache that contains
// all enumerable properties of the object and its prototypes
// have none, the map of the object. This is used to speed up
// the check for deletions during a for-in.
RUNTIME_FUNCTION(Runtime_GetPropertyNamesFast) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);

  CONVERT_ARG_CHECKED(JSReceiver, raw_object, 0);

  if (raw_object->IsSimpleEnum()) return raw_object->map();

  HandleScope scope(isolate);
  Handle<JSReceiver> object(raw_object);
  Handle<FixedArray> content;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, content,
      JSReceiver::GetKeys(object, JSReceiver::INCLUDE_PROTOS));

  // Test again, since cache may have been built by preceding call.
  if (object->IsSimpleEnum()) return object->map();

  return *content;
}


// Find the length of the prototype chain that is to be handled as one. If a
// prototype object is hidden it is to be viewed as part of the the object it
// is prototype for.
static int OwnPrototypeChainLength(JSObject* obj) {
  int count = 1;
  for (PrototypeIterator iter(obj->GetIsolate(), obj);
       !iter.IsAtEnd(PrototypeIterator::END_AT_NON_HIDDEN); iter.Advance()) {
    count++;
  }
  return count;
}


// Return the names of the own named properties.
// args[0]: object
// args[1]: PropertyAttributes as int
RUNTIME_FUNCTION(Runtime_GetOwnPropertyNames) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  if (!args[0]->IsJSObject()) {
    return isolate->heap()->undefined_value();
  }
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);
  CONVERT_SMI_ARG_CHECKED(filter_value, 1);
  PropertyAttributes filter = static_cast<PropertyAttributes>(filter_value);

  // Skip the global proxy as it has no properties and always delegates to the
  // real global object.
  if (obj->IsJSGlobalProxy()) {
    // Only collect names if access is permitted.
    if (obj->IsAccessCheckNeeded() &&
        !isolate->MayNamedAccess(obj, isolate->factory()->undefined_value(),
                                 v8::ACCESS_KEYS)) {
      isolate->ReportFailedAccessCheck(obj, v8::ACCESS_KEYS);
      RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
      return *isolate->factory()->NewJSArray(0);
    }
    PrototypeIterator iter(isolate, obj);
    obj = Handle<JSObject>::cast(PrototypeIterator::GetCurrent(iter));
  }

  // Find the number of objects making up this.
  int length = OwnPrototypeChainLength(*obj);

  // Find the number of own properties for each of the objects.
  ScopedVector<int> own_property_count(length);
  int total_property_count = 0;
  {
    PrototypeIterator iter(isolate, obj, PrototypeIterator::START_AT_RECEIVER);
    for (int i = 0; i < length; i++) {
      DCHECK(!iter.IsAtEnd());
      Handle<JSObject> jsproto =
          Handle<JSObject>::cast(PrototypeIterator::GetCurrent(iter));
      // Only collect names if access is permitted.
      if (jsproto->IsAccessCheckNeeded() &&
          !isolate->MayNamedAccess(jsproto,
                                   isolate->factory()->undefined_value(),
                                   v8::ACCESS_KEYS)) {
        isolate->ReportFailedAccessCheck(jsproto, v8::ACCESS_KEYS);
        RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
        return *isolate->factory()->NewJSArray(0);
      }
      int n;
      n = jsproto->NumberOfOwnProperties(filter);
      own_property_count[i] = n;
      total_property_count += n;
      iter.Advance();
    }
  }

  // Allocate an array with storage for all the property names.
  Handle<FixedArray> names =
      isolate->factory()->NewFixedArray(total_property_count);

  // Get the property names.
  int next_copy_index = 0;
  int hidden_strings = 0;
  {
    PrototypeIterator iter(isolate, obj, PrototypeIterator::START_AT_RECEIVER);
    for (int i = 0; i < length; i++) {
      DCHECK(!iter.IsAtEnd());
      Handle<JSObject> jsproto =
          Handle<JSObject>::cast(PrototypeIterator::GetCurrent(iter));
      jsproto->GetOwnPropertyNames(*names, next_copy_index, filter);
      if (i > 0) {
        // Names from hidden prototypes may already have been added
        // for inherited function template instances. Count the duplicates
        // and stub them out; the final copy pass at the end ignores holes.
        for (int j = next_copy_index;
             j < next_copy_index + own_property_count[i]; j++) {
          Object* name_from_hidden_proto = names->get(j);
          for (int k = 0; k < next_copy_index; k++) {
            if (names->get(k) != isolate->heap()->hidden_string()) {
              Object* name = names->get(k);
              if (name_from_hidden_proto == name) {
                names->set(j, isolate->heap()->hidden_string());
                hidden_strings++;
                break;
              }
            }
          }
        }
      }
      next_copy_index += own_property_count[i];

      // Hidden properties only show up if the filter does not skip strings.
      if ((filter & STRING) == 0 && JSObject::HasHiddenProperties(jsproto)) {
        hidden_strings++;
      }
      iter.Advance();
    }
  }

  // Filter out name of hidden properties object and
  // hidden prototype duplicates.
  if (hidden_strings > 0) {
    Handle<FixedArray> old_names = names;
    names = isolate->factory()->NewFixedArray(names->length() - hidden_strings);
    int dest_pos = 0;
    for (int i = 0; i < total_property_count; i++) {
      Object* name = old_names->get(i);
      if (name == isolate->heap()->hidden_string()) {
        hidden_strings--;
        continue;
      }
      names->set(dest_pos++, name);
    }
    DCHECK_EQ(0, hidden_strings);
  }

  return *isolate->factory()->NewJSArrayWithElements(names);
}


// Return the names of the own indexed properties.
// args[0]: object
RUNTIME_FUNCTION(Runtime_GetOwnElementNames) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  if (!args[0]->IsJSObject()) {
    return isolate->heap()->undefined_value();
  }
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);

  int n = obj->NumberOfOwnElements(static_cast<PropertyAttributes>(NONE));
  Handle<FixedArray> names = isolate->factory()->NewFixedArray(n);
  obj->GetOwnElementKeys(*names, static_cast<PropertyAttributes>(NONE));
  return *isolate->factory()->NewJSArrayWithElements(names);
}


// Return information on whether an object has a named or indexed interceptor.
// args[0]: object
RUNTIME_FUNCTION(Runtime_GetInterceptorInfo) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  if (!args[0]->IsJSObject()) {
    return Smi::FromInt(0);
  }
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);

  int result = 0;
  if (obj->HasNamedInterceptor()) result |= 2;
  if (obj->HasIndexedInterceptor()) result |= 1;

  return Smi::FromInt(result);
}


// Return property names from named interceptor.
// args[0]: object
RUNTIME_FUNCTION(Runtime_GetNamedInterceptorPropertyNames) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);

  if (obj->HasNamedInterceptor()) {
    Handle<JSObject> result;
    if (JSObject::GetKeysForNamedInterceptor(obj, obj).ToHandle(&result)) {
      return *result;
    }
  }
  return isolate->heap()->undefined_value();
}


// Return element names from indexed interceptor.
// args[0]: object
RUNTIME_FUNCTION(Runtime_GetIndexedInterceptorElementNames) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);

  if (obj->HasIndexedInterceptor()) {
    Handle<JSObject> result;
    if (JSObject::GetKeysForIndexedInterceptor(obj, obj).ToHandle(&result)) {
      return *result;
    }
  }
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_OwnKeys) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(JSObject, raw_object, 0);
  Handle<JSObject> object(raw_object);

  if (object->IsJSGlobalProxy()) {
    // Do access checks before going to the global object.
    if (object->IsAccessCheckNeeded() &&
        !isolate->MayNamedAccess(object, isolate->factory()->undefined_value(),
                                 v8::ACCESS_KEYS)) {
      isolate->ReportFailedAccessCheck(object, v8::ACCESS_KEYS);
      RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
      return *isolate->factory()->NewJSArray(0);
    }

    PrototypeIterator iter(isolate, object);
    // If proxy is detached we simply return an empty array.
    if (iter.IsAtEnd()) return *isolate->factory()->NewJSArray(0);
    object = Handle<JSObject>::cast(PrototypeIterator::GetCurrent(iter));
  }

  Handle<FixedArray> contents;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, contents, JSReceiver::GetKeys(object, JSReceiver::OWN_ONLY));

  // Some fast paths through GetKeysInFixedArrayFor reuse a cached
  // property array and since the result is mutable we have to create
  // a fresh clone on each invocation.
  int length = contents->length();
  Handle<FixedArray> copy = isolate->factory()->NewFixedArray(length);
  for (int i = 0; i < length; i++) {
    Object* entry = contents->get(i);
    if (entry->IsString()) {
      copy->set(i, entry);
    } else {
      DCHECK(entry->IsNumber());
      HandleScope scope(isolate);
      Handle<Object> entry_handle(entry, isolate);
      Handle<Object> entry_str =
          isolate->factory()->NumberToString(entry_handle);
      copy->set(i, *entry_str);
    }
  }
  return *isolate->factory()->NewJSArrayWithElements(copy);
}


RUNTIME_FUNCTION(Runtime_GetArgumentsProperty) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, raw_key, 0);

  // Compute the frame holding the arguments.
  JavaScriptFrameIterator it(isolate);
  it.AdvanceToArgumentsFrame();
  JavaScriptFrame* frame = it.frame();

  // Get the actual number of provided arguments.
  const uint32_t n = frame->ComputeParametersCount();

  // Try to convert the key to an index. If successful and within
  // index return the the argument from the frame.
  uint32_t index;
  if (raw_key->ToArrayIndex(&index) && index < n) {
    return frame->GetParameter(index);
  }

  HandleScope scope(isolate);
  if (raw_key->IsSymbol()) {
    Handle<Symbol> symbol = Handle<Symbol>::cast(raw_key);
    if (symbol->Equals(isolate->native_context()->iterator_symbol())) {
      return isolate->native_context()->array_values_iterator();
    }
    // Lookup in the initial Object.prototype object.
    Handle<Object> result;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, result,
        Object::GetProperty(isolate->initial_object_prototype(),
                            Handle<Symbol>::cast(raw_key)));
    return *result;
  }

  // Convert the key to a string.
  Handle<Object> converted;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, converted,
                                     Execution::ToString(isolate, raw_key));
  Handle<String> key = Handle<String>::cast(converted);

  // Try to convert the string key into an array index.
  if (key->AsArrayIndex(&index)) {
    if (index < n) {
      return frame->GetParameter(index);
    } else {
      Handle<Object> initial_prototype(isolate->initial_object_prototype());
      Handle<Object> result;
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
          isolate, result,
          Object::GetElement(isolate, initial_prototype, index));
      return *result;
    }
  }

  // Handle special arguments properties.
  if (String::Equals(isolate->factory()->length_string(), key)) {
    return Smi::FromInt(n);
  }
  if (String::Equals(isolate->factory()->callee_string(), key)) {
    JSFunction* function = frame->function();
    if (function->shared()->strict_mode() == STRICT) {
      THROW_NEW_ERROR_RETURN_FAILURE(
          isolate, NewTypeError("strict_arguments_callee",
                                HandleVector<Object>(NULL, 0)));
    }
    return function;
  }

  // Lookup in the initial Object.prototype object.
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      Object::GetProperty(isolate->initial_object_prototype(), key));
  return *result;
}


RUNTIME_FUNCTION(Runtime_ToFastProperties) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  if (object->IsJSObject() && !object->IsGlobalObject()) {
    JSObject::MigrateSlowToFast(Handle<JSObject>::cast(object), 0);
  }
  return *object;
}


RUNTIME_FUNCTION(Runtime_ToBool) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, object, 0);

  return isolate->heap()->ToBoolean(object->BooleanValue());
}


// Returns the type string of a value; see ECMA-262, 11.4.3 (p 47).
// Possible optimizations: put the type string into the oddballs.
RUNTIME_FUNCTION(Runtime_Typeof) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  if (obj->IsNumber()) return isolate->heap()->number_string();
  HeapObject* heap_obj = HeapObject::cast(obj);

  // typeof an undetectable object is 'undefined'
  if (heap_obj->map()->is_undetectable()) {
    return isolate->heap()->undefined_string();
  }

  InstanceType instance_type = heap_obj->map()->instance_type();
  if (instance_type < FIRST_NONSTRING_TYPE) {
    return isolate->heap()->string_string();
  }

  switch (instance_type) {
    case ODDBALL_TYPE:
      if (heap_obj->IsTrue() || heap_obj->IsFalse()) {
        return isolate->heap()->boolean_string();
      }
      if (heap_obj->IsNull()) {
        return isolate->heap()->object_string();
      }
      DCHECK(heap_obj->IsUndefined());
      return isolate->heap()->undefined_string();
    case SYMBOL_TYPE:
      return isolate->heap()->symbol_string();
    case JS_FUNCTION_TYPE:
    case JS_FUNCTION_PROXY_TYPE:
      return isolate->heap()->function_string();
    default:
      // For any kind of object not handled above, the spec rule for
      // host objects gives that it is okay to return "object"
      return isolate->heap()->object_string();
  }
}


RUNTIME_FUNCTION(Runtime_Booleanize) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_CHECKED(Object, value_raw, 0);
  CONVERT_SMI_ARG_CHECKED(token_raw, 1);
  intptr_t value = reinterpret_cast<intptr_t>(value_raw);
  Token::Value token = static_cast<Token::Value>(token_raw);
  switch (token) {
    case Token::EQ:
    case Token::EQ_STRICT:
      return isolate->heap()->ToBoolean(value == 0);
    case Token::NE:
    case Token::NE_STRICT:
      return isolate->heap()->ToBoolean(value != 0);
    case Token::LT:
      return isolate->heap()->ToBoolean(value < 0);
    case Token::GT:
      return isolate->heap()->ToBoolean(value > 0);
    case Token::LTE:
      return isolate->heap()->ToBoolean(value <= 0);
    case Token::GTE:
      return isolate->heap()->ToBoolean(value >= 0);
    default:
      // This should only happen during natives fuzzing.
      return isolate->heap()->undefined_value();
  }
}


RUNTIME_FUNCTION(Runtime_NewStringWrapper) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(String, value, 0);
  return *Object::ToObject(isolate, value).ToHandleChecked();
}


RUNTIME_FUNCTION(Runtime_AllocateHeapNumber) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0);
  return *isolate->factory()->NewHeapNumber(0);
}


static Handle<JSObject> NewSloppyArguments(Isolate* isolate,
                                           Handle<JSFunction> callee,
                                           Object** parameters,
                                           int argument_count) {
  Handle<JSObject> result =
      isolate->factory()->NewArgumentsObject(callee, argument_count);

  // Allocate the elements if needed.
  int parameter_count = callee->shared()->formal_parameter_count();
  if (argument_count > 0) {
    if (parameter_count > 0) {
      int mapped_count = Min(argument_count, parameter_count);
      Handle<FixedArray> parameter_map =
          isolate->factory()->NewFixedArray(mapped_count + 2, NOT_TENURED);
      parameter_map->set_map(isolate->heap()->sloppy_arguments_elements_map());

      Handle<Map> map = Map::Copy(handle(result->map()));
      map->set_elements_kind(SLOPPY_ARGUMENTS_ELEMENTS);

      result->set_map(*map);
      result->set_elements(*parameter_map);

      // Store the context and the arguments array at the beginning of the
      // parameter map.
      Handle<Context> context(isolate->context());
      Handle<FixedArray> arguments =
          isolate->factory()->NewFixedArray(argument_count, NOT_TENURED);
      parameter_map->set(0, *context);
      parameter_map->set(1, *arguments);

      // Loop over the actual parameters backwards.
      int index = argument_count - 1;
      while (index >= mapped_count) {
        // These go directly in the arguments array and have no
        // corresponding slot in the parameter map.
        arguments->set(index, *(parameters - index - 1));
        --index;
      }

      Handle<ScopeInfo> scope_info(callee->shared()->scope_info());
      while (index >= 0) {
        // Detect duplicate names to the right in the parameter list.
        Handle<String> name(scope_info->ParameterName(index));
        int context_local_count = scope_info->ContextLocalCount();
        bool duplicate = false;
        for (int j = index + 1; j < parameter_count; ++j) {
          if (scope_info->ParameterName(j) == *name) {
            duplicate = true;
            break;
          }
        }

        if (duplicate) {
          // This goes directly in the arguments array with a hole in the
          // parameter map.
          arguments->set(index, *(parameters - index - 1));
          parameter_map->set_the_hole(index + 2);
        } else {
          // The context index goes in the parameter map with a hole in the
          // arguments array.
          int context_index = -1;
          for (int j = 0; j < context_local_count; ++j) {
            if (scope_info->ContextLocalName(j) == *name) {
              context_index = j;
              break;
            }
          }
          DCHECK(context_index >= 0);
          arguments->set_the_hole(index);
          parameter_map->set(
              index + 2,
              Smi::FromInt(Context::MIN_CONTEXT_SLOTS + context_index));
        }

        --index;
      }
    } else {
      // If there is no aliasing, the arguments object elements are not
      // special in any way.
      Handle<FixedArray> elements =
          isolate->factory()->NewFixedArray(argument_count, NOT_TENURED);
      result->set_elements(*elements);
      for (int i = 0; i < argument_count; ++i) {
        elements->set(i, *(parameters - i - 1));
      }
    }
  }
  return result;
}


static Handle<JSObject> NewStrictArguments(Isolate* isolate,
                                           Handle<JSFunction> callee,
                                           Object** parameters,
                                           int argument_count) {
  Handle<JSObject> result =
      isolate->factory()->NewArgumentsObject(callee, argument_count);

  if (argument_count > 0) {
    Handle<FixedArray> array =
        isolate->factory()->NewUninitializedFixedArray(argument_count);
    DisallowHeapAllocation no_gc;
    WriteBarrierMode mode = array->GetWriteBarrierMode(no_gc);
    for (int i = 0; i < argument_count; i++) {
      array->set(i, *--parameters, mode);
    }
    result->set_elements(*array);
  }
  return result;
}


RUNTIME_FUNCTION(Runtime_NewArguments) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, callee, 0);
  JavaScriptFrameIterator it(isolate);

  // Find the frame that holds the actual arguments passed to the function.
  it.AdvanceToArgumentsFrame();
  JavaScriptFrame* frame = it.frame();

  // Determine parameter location on the stack and dispatch on language mode.
  int argument_count = frame->GetArgumentsLength();
  Object** parameters = reinterpret_cast<Object**>(frame->GetParameterSlot(-1));
  return callee->shared()->strict_mode() == STRICT
             ? *NewStrictArguments(isolate, callee, parameters, argument_count)
             : *NewSloppyArguments(isolate, callee, parameters, argument_count);
}


RUNTIME_FUNCTION(Runtime_NewSloppyArguments) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, callee, 0);
  Object** parameters = reinterpret_cast<Object**>(args[1]);
  CONVERT_SMI_ARG_CHECKED(argument_count, 2);
  return *NewSloppyArguments(isolate, callee, parameters, argument_count);
}


RUNTIME_FUNCTION(Runtime_NewStrictArguments) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, callee, 0)
  Object** parameters = reinterpret_cast<Object**>(args[1]);
  CONVERT_SMI_ARG_CHECKED(argument_count, 2);
  return *NewStrictArguments(isolate, callee, parameters, argument_count);
}


RUNTIME_FUNCTION(Runtime_NewClosureFromStubFailure) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(SharedFunctionInfo, shared, 0);
  Handle<Context> context(isolate->context());
  PretenureFlag pretenure_flag = NOT_TENURED;
  return *isolate->factory()->NewFunctionFromSharedFunctionInfo(shared, context,
                                                                pretenure_flag);
}


RUNTIME_FUNCTION(Runtime_NewClosure) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(Context, context, 0);
  CONVERT_ARG_HANDLE_CHECKED(SharedFunctionInfo, shared, 1);
  CONVERT_BOOLEAN_ARG_CHECKED(pretenure, 2);

  // The caller ensures that we pretenure closures that are assigned
  // directly to properties.
  PretenureFlag pretenure_flag = pretenure ? TENURED : NOT_TENURED;
  return *isolate->factory()->NewFunctionFromSharedFunctionInfo(shared, context,
                                                                pretenure_flag);
}


// Find the arguments of the JavaScript function invocation that called
// into C++ code. Collect these in a newly allocated array of handles (possibly
// prefixed by a number of empty handles).
static SmartArrayPointer<Handle<Object> > GetCallerArguments(Isolate* isolate,
                                                             int prefix_argc,
                                                             int* total_argc) {
  // Find frame containing arguments passed to the caller.
  JavaScriptFrameIterator it(isolate);
  JavaScriptFrame* frame = it.frame();
  List<JSFunction*> functions(2);
  frame->GetFunctions(&functions);
  if (functions.length() > 1) {
    int inlined_jsframe_index = functions.length() - 1;
    JSFunction* inlined_function = functions[inlined_jsframe_index];
    SlotRefValueBuilder slot_refs(
        frame, inlined_jsframe_index,
        inlined_function->shared()->formal_parameter_count());

    int args_count = slot_refs.args_length();

    *total_argc = prefix_argc + args_count;
    SmartArrayPointer<Handle<Object> > param_data(
        NewArray<Handle<Object> >(*total_argc));
    slot_refs.Prepare(isolate);
    for (int i = 0; i < args_count; i++) {
      Handle<Object> val = slot_refs.GetNext(isolate, 0);
      param_data[prefix_argc + i] = val;
    }
    slot_refs.Finish(isolate);

    return param_data;
  } else {
    it.AdvanceToArgumentsFrame();
    frame = it.frame();
    int args_count = frame->ComputeParametersCount();

    *total_argc = prefix_argc + args_count;
    SmartArrayPointer<Handle<Object> > param_data(
        NewArray<Handle<Object> >(*total_argc));
    for (int i = 0; i < args_count; i++) {
      Handle<Object> val = Handle<Object>(frame->GetParameter(i), isolate);
      param_data[prefix_argc + i] = val;
    }
    return param_data;
  }
}


RUNTIME_FUNCTION(Runtime_FunctionBindArguments) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, bound_function, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, bindee, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, this_object, 2);
  CONVERT_NUMBER_ARG_HANDLE_CHECKED(new_length, 3);

  // TODO(lrn): Create bound function in C++ code from premade shared info.
  bound_function->shared()->set_bound(true);
  // Get all arguments of calling function (Function.prototype.bind).
  int argc = 0;
  SmartArrayPointer<Handle<Object> > arguments =
      GetCallerArguments(isolate, 0, &argc);
  // Don't count the this-arg.
  if (argc > 0) {
    RUNTIME_ASSERT(arguments[0].is_identical_to(this_object));
    argc--;
  } else {
    RUNTIME_ASSERT(this_object->IsUndefined());
  }
  // Initialize array of bindings (function, this, and any existing arguments
  // if the function was already bound).
  Handle<FixedArray> new_bindings;
  int i;
  if (bindee->IsJSFunction() && JSFunction::cast(*bindee)->shared()->bound()) {
    Handle<FixedArray> old_bindings(
        JSFunction::cast(*bindee)->function_bindings());
    RUNTIME_ASSERT(old_bindings->length() > JSFunction::kBoundFunctionIndex);
    new_bindings =
        isolate->factory()->NewFixedArray(old_bindings->length() + argc);
    bindee = Handle<Object>(old_bindings->get(JSFunction::kBoundFunctionIndex),
                            isolate);
    i = 0;
    for (int n = old_bindings->length(); i < n; i++) {
      new_bindings->set(i, old_bindings->get(i));
    }
  } else {
    int array_size = JSFunction::kBoundArgumentsStartIndex + argc;
    new_bindings = isolate->factory()->NewFixedArray(array_size);
    new_bindings->set(JSFunction::kBoundFunctionIndex, *bindee);
    new_bindings->set(JSFunction::kBoundThisIndex, *this_object);
    i = 2;
  }
  // Copy arguments, skipping the first which is "this_arg".
  for (int j = 0; j < argc; j++, i++) {
    new_bindings->set(i, *arguments[j + 1]);
  }
  new_bindings->set_map_no_write_barrier(
      isolate->heap()->fixed_cow_array_map());
  bound_function->set_function_bindings(*new_bindings);

  // Update length. Have to remove the prototype first so that map migration
  // is happy about the number of fields.
  RUNTIME_ASSERT(bound_function->RemovePrototype());
  Handle<Map> bound_function_map(
      isolate->native_context()->bound_function_map());
  JSObject::MigrateToMap(bound_function, bound_function_map);
  Handle<String> length_string = isolate->factory()->length_string();
  PropertyAttributes attr =
      static_cast<PropertyAttributes>(DONT_DELETE | DONT_ENUM | READ_ONLY);
  RETURN_FAILURE_ON_EXCEPTION(
      isolate, JSObject::SetOwnPropertyIgnoreAttributes(
                   bound_function, length_string, new_length, attr));
  return *bound_function;
}


RUNTIME_FUNCTION(Runtime_BoundFunctionGetBindings) {
  HandleScope handles(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, callable, 0);
  if (callable->IsJSFunction()) {
    Handle<JSFunction> function = Handle<JSFunction>::cast(callable);
    if (function->shared()->bound()) {
      Handle<FixedArray> bindings(function->function_bindings());
      RUNTIME_ASSERT(bindings->map() == isolate->heap()->fixed_cow_array_map());
      return *isolate->factory()->NewJSArrayWithElements(bindings);
    }
  }
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_NewObjectFromBound) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  // First argument is a function to use as a constructor.
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  RUNTIME_ASSERT(function->shared()->bound());

  // The argument is a bound function. Extract its bound arguments
  // and callable.
  Handle<FixedArray> bound_args =
      Handle<FixedArray>(FixedArray::cast(function->function_bindings()));
  int bound_argc = bound_args->length() - JSFunction::kBoundArgumentsStartIndex;
  Handle<Object> bound_function(
      JSReceiver::cast(bound_args->get(JSFunction::kBoundFunctionIndex)),
      isolate);
  DCHECK(!bound_function->IsJSFunction() ||
         !Handle<JSFunction>::cast(bound_function)->shared()->bound());

  int total_argc = 0;
  SmartArrayPointer<Handle<Object> > param_data =
      GetCallerArguments(isolate, bound_argc, &total_argc);
  for (int i = 0; i < bound_argc; i++) {
    param_data[i] = Handle<Object>(
        bound_args->get(JSFunction::kBoundArgumentsStartIndex + i), isolate);
  }

  if (!bound_function->IsJSFunction()) {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, bound_function,
        Execution::TryGetConstructorDelegate(isolate, bound_function));
  }
  DCHECK(bound_function->IsJSFunction());

  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, Execution::New(Handle<JSFunction>::cast(bound_function),
                                      total_argc, param_data.get()));
  return *result;
}


static Object* Runtime_NewObjectHelper(Isolate* isolate,
                                       Handle<Object> constructor,
                                       Handle<AllocationSite> site) {
  // If the constructor isn't a proper function we throw a type error.
  if (!constructor->IsJSFunction()) {
    Vector<Handle<Object> > arguments = HandleVector(&constructor, 1);
    THROW_NEW_ERROR_RETURN_FAILURE(isolate,
                                   NewTypeError("not_constructor", arguments));
  }

  Handle<JSFunction> function = Handle<JSFunction>::cast(constructor);

  // If function should not have prototype, construction is not allowed. In this
  // case generated code bailouts here, since function has no initial_map.
  if (!function->should_have_prototype() && !function->shared()->bound()) {
    Vector<Handle<Object> > arguments = HandleVector(&constructor, 1);
    THROW_NEW_ERROR_RETURN_FAILURE(isolate,
                                   NewTypeError("not_constructor", arguments));
  }

  Debug* debug = isolate->debug();
  // Handle stepping into constructors if step into is active.
  if (debug->StepInActive()) {
    debug->HandleStepIn(function, Handle<Object>::null(), 0, true);
  }

  if (function->has_initial_map()) {
    if (function->initial_map()->instance_type() == JS_FUNCTION_TYPE) {
      // The 'Function' function ignores the receiver object when
      // called using 'new' and creates a new JSFunction object that
      // is returned.  The receiver object is only used for error
      // reporting if an error occurs when constructing the new
      // JSFunction. Factory::NewJSObject() should not be used to
      // allocate JSFunctions since it does not properly initialize
      // the shared part of the function. Since the receiver is
      // ignored anyway, we use the global object as the receiver
      // instead of a new JSFunction object. This way, errors are
      // reported the same way whether or not 'Function' is called
      // using 'new'.
      return isolate->global_proxy();
    }
  }

  // The function should be compiled for the optimization hints to be
  // available.
  Compiler::EnsureCompiled(function, CLEAR_EXCEPTION);

  Handle<JSObject> result;
  if (site.is_null()) {
    result = isolate->factory()->NewJSObject(function);
  } else {
    result = isolate->factory()->NewJSObjectWithMemento(function, site);
  }

  isolate->counters()->constructed_objects()->Increment();
  isolate->counters()->constructed_objects_runtime()->Increment();

  return *result;
}


RUNTIME_FUNCTION(Runtime_NewObject) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, constructor, 0);
  return Runtime_NewObjectHelper(isolate, constructor,
                                 Handle<AllocationSite>::null());
}


RUNTIME_FUNCTION(Runtime_NewObjectWithAllocationSite) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(Object, constructor, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, feedback, 0);
  Handle<AllocationSite> site;
  if (feedback->IsAllocationSite()) {
    // The feedback can be an AllocationSite or undefined.
    site = Handle<AllocationSite>::cast(feedback);
  }
  return Runtime_NewObjectHelper(isolate, constructor, site);
}


RUNTIME_FUNCTION(Runtime_FinalizeInstanceSize) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);

  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  function->CompleteInobjectSlackTracking();

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_CheckIsBootstrapping) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 0);
  RUNTIME_ASSERT(isolate->bootstrapper()->IsActive());
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_GetRootNaN) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 0);
  RUNTIME_ASSERT(isolate->bootstrapper()->IsActive());
  return isolate->heap()->nan_value();
}


RUNTIME_FUNCTION(Runtime_Call) {
  HandleScope scope(isolate);
  DCHECK(args.length() >= 2);
  int argc = args.length() - 2;
  CONVERT_ARG_CHECKED(JSReceiver, fun, argc + 1);
  Object* receiver = args[0];

  // If there are too many arguments, allocate argv via malloc.
  const int argv_small_size = 10;
  Handle<Object> argv_small_buffer[argv_small_size];
  SmartArrayPointer<Handle<Object> > argv_large_buffer;
  Handle<Object>* argv = argv_small_buffer;
  if (argc > argv_small_size) {
    argv = new Handle<Object>[argc];
    if (argv == NULL) return isolate->StackOverflow();
    argv_large_buffer = SmartArrayPointer<Handle<Object> >(argv);
  }

  for (int i = 0; i < argc; ++i) {
    argv[i] = Handle<Object>(args[1 + i], isolate);
  }

  Handle<JSReceiver> hfun(fun);
  Handle<Object> hreceiver(receiver, isolate);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      Execution::Call(isolate, hfun, hreceiver, argc, argv, true));
  return *result;
}


RUNTIME_FUNCTION(Runtime_Apply) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 5);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, fun, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, receiver, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, arguments, 2);
  CONVERT_INT32_ARG_CHECKED(offset, 3);
  CONVERT_INT32_ARG_CHECKED(argc, 4);
  RUNTIME_ASSERT(offset >= 0);
  // Loose upper bound to allow fuzzing. We'll most likely run out of
  // stack space before hitting this limit.
  static int kMaxArgc = 1000000;
  RUNTIME_ASSERT(argc >= 0 && argc <= kMaxArgc);

  // If there are too many arguments, allocate argv via malloc.
  const int argv_small_size = 10;
  Handle<Object> argv_small_buffer[argv_small_size];
  SmartArrayPointer<Handle<Object> > argv_large_buffer;
  Handle<Object>* argv = argv_small_buffer;
  if (argc > argv_small_size) {
    argv = new Handle<Object>[argc];
    if (argv == NULL) return isolate->StackOverflow();
    argv_large_buffer = SmartArrayPointer<Handle<Object> >(argv);
  }

  for (int i = 0; i < argc; ++i) {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, argv[i], Object::GetElement(isolate, arguments, offset + i));
  }

  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      Execution::Call(isolate, fun, receiver, argc, argv, true));
  return *result;
}


RUNTIME_FUNCTION(Runtime_GetFunctionDelegate) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  RUNTIME_ASSERT(!object->IsJSFunction());
  return *Execution::GetFunctionDelegate(isolate, object);
}


RUNTIME_FUNCTION(Runtime_GetConstructorDelegate) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  RUNTIME_ASSERT(!object->IsJSFunction());
  return *Execution::GetConstructorDelegate(isolate, object);
}


RUNTIME_FUNCTION(Runtime_NewGlobalContext) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  CONVERT_ARG_HANDLE_CHECKED(ScopeInfo, scope_info, 1);
  Handle<Context> result =
      isolate->factory()->NewGlobalContext(function, scope_info);

  DCHECK(function->context() == isolate->context());
  DCHECK(function->context()->global_object() == result->global_object());
  result->global_object()->set_global_context(*result);
  return *result;
}


RUNTIME_FUNCTION(Runtime_NewFunctionContext) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);

  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);

  DCHECK(function->context() == isolate->context());
  int length = function->shared()->scope_info()->ContextLength();
  return *isolate->factory()->NewFunctionContext(length, function);
}


RUNTIME_FUNCTION(Runtime_PushWithContext) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  Handle<JSReceiver> extension_object;
  if (args[0]->IsJSReceiver()) {
    extension_object = args.at<JSReceiver>(0);
  } else {
    // Try to convert the object to a proper JavaScript object.
    MaybeHandle<JSReceiver> maybe_object =
        Object::ToObject(isolate, args.at<Object>(0));
    if (!maybe_object.ToHandle(&extension_object)) {
      Handle<Object> handle = args.at<Object>(0);
      THROW_NEW_ERROR_RETURN_FAILURE(
          isolate, NewTypeError("with_expression", HandleVector(&handle, 1)));
    }
  }

  Handle<JSFunction> function;
  if (args[1]->IsSmi()) {
    // A smi sentinel indicates a context nested inside global code rather
    // than some function.  There is a canonical empty function that can be
    // gotten from the native context.
    function = handle(isolate->native_context()->closure());
  } else {
    function = args.at<JSFunction>(1);
  }

  Handle<Context> current(isolate->context());
  Handle<Context> context =
      isolate->factory()->NewWithContext(function, current, extension_object);
  isolate->set_context(*context);
  return *context;
}


RUNTIME_FUNCTION(Runtime_PushCatchContext) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(String, name, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, thrown_object, 1);
  Handle<JSFunction> function;
  if (args[2]->IsSmi()) {
    // A smi sentinel indicates a context nested inside global code rather
    // than some function.  There is a canonical empty function that can be
    // gotten from the native context.
    function = handle(isolate->native_context()->closure());
  } else {
    function = args.at<JSFunction>(2);
  }
  Handle<Context> current(isolate->context());
  Handle<Context> context = isolate->factory()->NewCatchContext(
      function, current, name, thrown_object);
  isolate->set_context(*context);
  return *context;
}


RUNTIME_FUNCTION(Runtime_PushBlockContext) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(ScopeInfo, scope_info, 0);
  Handle<JSFunction> function;
  if (args[1]->IsSmi()) {
    // A smi sentinel indicates a context nested inside global code rather
    // than some function.  There is a canonical empty function that can be
    // gotten from the native context.
    function = handle(isolate->native_context()->closure());
  } else {
    function = args.at<JSFunction>(1);
  }
  Handle<Context> current(isolate->context());
  Handle<Context> context =
      isolate->factory()->NewBlockContext(function, current, scope_info);
  isolate->set_context(*context);
  return *context;
}


RUNTIME_FUNCTION(Runtime_IsJSModule) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(obj->IsJSModule());
}


RUNTIME_FUNCTION(Runtime_PushModuleContext) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 2);
  CONVERT_SMI_ARG_CHECKED(index, 0);

  if (!args[1]->IsScopeInfo()) {
    // Module already initialized. Find hosting context and retrieve context.
    Context* host = Context::cast(isolate->context())->global_context();
    Context* context = Context::cast(host->get(index));
    DCHECK(context->previous() == isolate->context());
    isolate->set_context(context);
    return context;
  }

  CONVERT_ARG_HANDLE_CHECKED(ScopeInfo, scope_info, 1);

  // Allocate module context.
  HandleScope scope(isolate);
  Factory* factory = isolate->factory();
  Handle<Context> context = factory->NewModuleContext(scope_info);
  Handle<JSModule> module = factory->NewJSModule(context, scope_info);
  context->set_module(*module);
  Context* previous = isolate->context();
  context->set_previous(previous);
  context->set_closure(previous->closure());
  context->set_global_object(previous->global_object());
  isolate->set_context(*context);

  // Find hosting scope and initialize internal variable holding module there.
  previous->global_context()->set(index, *context);

  return *context;
}


RUNTIME_FUNCTION(Runtime_DeclareModules) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(FixedArray, descriptions, 0);
  Context* host_context = isolate->context();

  for (int i = 0; i < descriptions->length(); ++i) {
    Handle<ModuleInfo> description(ModuleInfo::cast(descriptions->get(i)));
    int host_index = description->host_index();
    Handle<Context> context(Context::cast(host_context->get(host_index)));
    Handle<JSModule> module(context->module());

    for (int j = 0; j < description->length(); ++j) {
      Handle<String> name(description->name(j));
      VariableMode mode = description->mode(j);
      int index = description->index(j);
      switch (mode) {
        case VAR:
        case LET:
        case CONST:
        case CONST_LEGACY: {
          PropertyAttributes attr =
              IsImmutableVariableMode(mode) ? FROZEN : SEALED;
          Handle<AccessorInfo> info =
              Accessors::MakeModuleExport(name, index, attr);
          Handle<Object> result =
              JSObject::SetAccessor(module, info).ToHandleChecked();
          DCHECK(!result->IsUndefined());
          USE(result);
          break;
        }
        case MODULE: {
          Object* referenced_context = Context::cast(host_context)->get(index);
          Handle<JSModule> value(Context::cast(referenced_context)->module());
          JSObject::SetOwnPropertyIgnoreAttributes(module, name, value, FROZEN)
              .Assert();
          break;
        }
        case INTERNAL:
        case TEMPORARY:
        case DYNAMIC:
        case DYNAMIC_GLOBAL:
        case DYNAMIC_LOCAL:
          UNREACHABLE();
      }
    }

    JSObject::PreventExtensions(module).Assert();
  }

  DCHECK(!isolate->has_pending_exception());
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_DeleteLookupSlot) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(Context, context, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, name, 1);

  int index;
  PropertyAttributes attributes;
  ContextLookupFlags flags = FOLLOW_CHAINS;
  BindingFlags binding_flags;
  Handle<Object> holder =
      context->Lookup(name, flags, &index, &attributes, &binding_flags);

  // If the slot was not found the result is true.
  if (holder.is_null()) {
    return isolate->heap()->true_value();
  }

  // If the slot was found in a context, it should be DONT_DELETE.
  if (holder->IsContext()) {
    return isolate->heap()->false_value();
  }

  // The slot was found in a JSObject, either a context extension object,
  // the global object, or the subject of a with.  Try to delete it
  // (respecting DONT_DELETE).
  Handle<JSObject> object = Handle<JSObject>::cast(holder);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     JSReceiver::DeleteProperty(object, name));
  return *result;
}


static Object* ComputeReceiverForNonGlobal(Isolate* isolate, JSObject* holder) {
  DCHECK(!holder->IsGlobalObject());
  Context* top = isolate->context();
  // Get the context extension function.
  JSFunction* context_extension_function =
      top->native_context()->context_extension_function();
  // If the holder isn't a context extension object, we just return it
  // as the receiver. This allows arguments objects to be used as
  // receivers, but only if they are put in the context scope chain
  // explicitly via a with-statement.
  Object* constructor = holder->map()->constructor();
  if (constructor != context_extension_function) return holder;
  // Fall back to using the global object as the implicit receiver if
  // the property turns out to be a local variable allocated in a
  // context extension object - introduced via eval.
  return isolate->heap()->undefined_value();
}


static ObjectPair LoadLookupSlotHelper(Arguments args, Isolate* isolate,
                                       bool throw_error) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());

  if (!args[0]->IsContext() || !args[1]->IsString()) {
    return MakePair(isolate->ThrowIllegalOperation(), NULL);
  }
  Handle<Context> context = args.at<Context>(0);
  Handle<String> name = args.at<String>(1);

  int index;
  PropertyAttributes attributes;
  ContextLookupFlags flags = FOLLOW_CHAINS;
  BindingFlags binding_flags;
  Handle<Object> holder =
      context->Lookup(name, flags, &index, &attributes, &binding_flags);
  if (isolate->has_pending_exception()) {
    return MakePair(isolate->heap()->exception(), NULL);
  }

  // If the index is non-negative, the slot has been found in a context.
  if (index >= 0) {
    DCHECK(holder->IsContext());
    // If the "property" we were looking for is a local variable, the
    // receiver is the global object; see ECMA-262, 3rd., 10.1.6 and 10.2.3.
    Handle<Object> receiver = isolate->factory()->undefined_value();
    Object* value = Context::cast(*holder)->get(index);
    // Check for uninitialized bindings.
    switch (binding_flags) {
      case MUTABLE_CHECK_INITIALIZED:
      case IMMUTABLE_CHECK_INITIALIZED_HARMONY:
        if (value->IsTheHole()) {
          Handle<Object> error;
          MaybeHandle<Object> maybe_error =
              isolate->factory()->NewReferenceError("not_defined",
                                                    HandleVector(&name, 1));
          if (maybe_error.ToHandle(&error)) isolate->Throw(*error);
          return MakePair(isolate->heap()->exception(), NULL);
        }
      // FALLTHROUGH
      case MUTABLE_IS_INITIALIZED:
      case IMMUTABLE_IS_INITIALIZED:
      case IMMUTABLE_IS_INITIALIZED_HARMONY:
        DCHECK(!value->IsTheHole());
        return MakePair(value, *receiver);
      case IMMUTABLE_CHECK_INITIALIZED:
        if (value->IsTheHole()) {
          DCHECK((attributes & READ_ONLY) != 0);
          value = isolate->heap()->undefined_value();
        }
        return MakePair(value, *receiver);
      case MISSING_BINDING:
        UNREACHABLE();
        return MakePair(NULL, NULL);
    }
  }

  // Otherwise, if the slot was found the holder is a context extension
  // object, subject of a with, or a global object.  We read the named
  // property from it.
  if (!holder.is_null()) {
    Handle<JSReceiver> object = Handle<JSReceiver>::cast(holder);
#ifdef DEBUG
    if (!object->IsJSProxy()) {
      Maybe<bool> maybe = JSReceiver::HasProperty(object, name);
      DCHECK(maybe.has_value);
      DCHECK(maybe.value);
    }
#endif
    // GetProperty below can cause GC.
    Handle<Object> receiver_handle(
        object->IsGlobalObject()
            ? Object::cast(isolate->heap()->undefined_value())
            : object->IsJSProxy() ? static_cast<Object*>(*object)
                                  : ComputeReceiverForNonGlobal(
                                        isolate, JSObject::cast(*object)),
        isolate);

    // No need to unhole the value here.  This is taken care of by the
    // GetProperty function.
    Handle<Object> value;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, value, Object::GetProperty(object, name),
        MakePair(isolate->heap()->exception(), NULL));
    return MakePair(*value, *receiver_handle);
  }

  if (throw_error) {
    // The property doesn't exist - throw exception.
    Handle<Object> error;
    MaybeHandle<Object> maybe_error = isolate->factory()->NewReferenceError(
        "not_defined", HandleVector(&name, 1));
    if (maybe_error.ToHandle(&error)) isolate->Throw(*error);
    return MakePair(isolate->heap()->exception(), NULL);
  } else {
    // The property doesn't exist - return undefined.
    return MakePair(isolate->heap()->undefined_value(),
                    isolate->heap()->undefined_value());
  }
}


RUNTIME_FUNCTION_RETURN_PAIR(Runtime_LoadLookupSlot) {
  return LoadLookupSlotHelper(args, isolate, true);
}


RUNTIME_FUNCTION_RETURN_PAIR(Runtime_LoadLookupSlotNoReferenceError) {
  return LoadLookupSlotHelper(args, isolate, false);
}


RUNTIME_FUNCTION(Runtime_StoreLookupSlot) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 4);

  CONVERT_ARG_HANDLE_CHECKED(Object, value, 0);
  CONVERT_ARG_HANDLE_CHECKED(Context, context, 1);
  CONVERT_ARG_HANDLE_CHECKED(String, name, 2);
  CONVERT_STRICT_MODE_ARG_CHECKED(strict_mode, 3);

  int index;
  PropertyAttributes attributes;
  ContextLookupFlags flags = FOLLOW_CHAINS;
  BindingFlags binding_flags;
  Handle<Object> holder =
      context->Lookup(name, flags, &index, &attributes, &binding_flags);
  // In case of JSProxy, an exception might have been thrown.
  if (isolate->has_pending_exception()) return isolate->heap()->exception();

  // The property was found in a context slot.
  if (index >= 0) {
    if ((attributes & READ_ONLY) == 0) {
      Handle<Context>::cast(holder)->set(index, *value);
    } else if (strict_mode == STRICT) {
      // Setting read only property in strict mode.
      THROW_NEW_ERROR_RETURN_FAILURE(
          isolate,
          NewTypeError("strict_cannot_assign", HandleVector(&name, 1)));
    }
    return *value;
  }

  // Slow case: The property is not in a context slot.  It is either in a
  // context extension object, a property of the subject of a with, or a
  // property of the global object.
  Handle<JSReceiver> object;
  if (attributes != ABSENT) {
    // The property exists on the holder.
    object = Handle<JSReceiver>::cast(holder);
  } else if (strict_mode == STRICT) {
    // If absent in strict mode: throw.
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewReferenceError("not_defined", HandleVector(&name, 1)));
  } else {
    // If absent in sloppy mode: add the property to the global object.
    object = Handle<JSReceiver>(context->global_object());
  }

  RETURN_FAILURE_ON_EXCEPTION(
      isolate, Object::SetProperty(object, name, value, strict_mode));

  return *value;
}


RUNTIME_FUNCTION(Runtime_Throw) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);

  return isolate->Throw(args[0]);
}


RUNTIME_FUNCTION(Runtime_ReThrow) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);

  return isolate->ReThrow(args[0]);
}


RUNTIME_FUNCTION(Runtime_PromoteScheduledException) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 0);
  return isolate->PromoteScheduledException();
}


RUNTIME_FUNCTION(Runtime_ThrowReferenceError) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, name, 0);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewReferenceError("not_defined", HandleVector(&name, 1)));
}


RUNTIME_FUNCTION(Runtime_ThrowNonMethodError) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewReferenceError("non_method", HandleVector<Object>(NULL, 0)));
}


RUNTIME_FUNCTION(Runtime_ThrowUnsupportedSuperError) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate,
      NewReferenceError("unsupported_super", HandleVector<Object>(NULL, 0)));
}


RUNTIME_FUNCTION(Runtime_StackGuard) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 0);

  // First check if this is a real stack overflow.
  StackLimitCheck check(isolate);
  if (check.JsHasOverflowed()) {
    return isolate->StackOverflow();
  }

  return isolate->stack_guard()->HandleInterrupts();
}


RUNTIME_FUNCTION(Runtime_Interrupt) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 0);
  return isolate->stack_guard()->HandleInterrupts();
}


static int StackSize(Isolate* isolate) {
  int n = 0;
  for (JavaScriptFrameIterator it(isolate); !it.done(); it.Advance()) n++;
  return n;
}


static void PrintTransition(Isolate* isolate, Object* result) {
  // indentation
  {
    const int nmax = 80;
    int n = StackSize(isolate);
    if (n <= nmax)
      PrintF("%4d:%*s", n, n, "");
    else
      PrintF("%4d:%*s", n, nmax, "...");
  }

  if (result == NULL) {
    JavaScriptFrame::PrintTop(isolate, stdout, true, false);
    PrintF(" {\n");
  } else {
    // function result
    PrintF("} -> ");
    result->ShortPrint();
    PrintF("\n");
  }
}


RUNTIME_FUNCTION(Runtime_TraceEnter) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 0);
  PrintTransition(isolate, NULL);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_TraceExit) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  PrintTransition(isolate, obj);
  return obj;  // return TOS
}


RUNTIME_FUNCTION(Runtime_GlobalProxy) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, global, 0);
  if (!global->IsJSGlobalObject()) return isolate->heap()->null_value();
  return JSGlobalObject::cast(global)->global_proxy();
}


RUNTIME_FUNCTION(Runtime_IsAttachedGlobal) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, global, 0);
  if (!global->IsJSGlobalObject()) return isolate->heap()->false_value();
  return isolate->heap()->ToBoolean(
      !JSGlobalObject::cast(global)->IsDetached());
}


RUNTIME_FUNCTION(Runtime_AllocateInNewSpace) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_SMI_ARG_CHECKED(size, 0);
  RUNTIME_ASSERT(IsAligned(size, kPointerSize));
  RUNTIME_ASSERT(size > 0);
  RUNTIME_ASSERT(size <= Page::kMaxRegularHeapObjectSize);
  return *isolate->factory()->NewFillerObject(size, false, NEW_SPACE);
}


RUNTIME_FUNCTION(Runtime_AllocateInTargetSpace) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_SMI_ARG_CHECKED(size, 0);
  CONVERT_SMI_ARG_CHECKED(flags, 1);
  RUNTIME_ASSERT(IsAligned(size, kPointerSize));
  RUNTIME_ASSERT(size > 0);
  RUNTIME_ASSERT(size <= Page::kMaxRegularHeapObjectSize);
  bool double_align = AllocateDoubleAlignFlag::decode(flags);
  AllocationSpace space = AllocateTargetSpace::decode(flags);
  return *isolate->factory()->NewFillerObject(size, double_align, space);
}


// Push an object unto an array of objects if it is not already in the
// array.  Returns true if the element was pushed on the stack and
// false otherwise.
RUNTIME_FUNCTION(Runtime_PushIfAbsent) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, array, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, element, 1);
  RUNTIME_ASSERT(array->HasFastSmiOrObjectElements());
  int length = Smi::cast(array->length())->value();
  FixedArray* elements = FixedArray::cast(array->elements());
  for (int i = 0; i < length; i++) {
    if (elements->get(i) == *element) return isolate->heap()->false_value();
  }

  // Strict not needed. Used for cycle detection in Array join implementation.
  RETURN_FAILURE_ON_EXCEPTION(
      isolate, JSObject::SetFastElement(array, length, element, SLOPPY, true));
  return isolate->heap()->true_value();
}


/**
 * A simple visitor visits every element of Array's.
 * The backend storage can be a fixed array for fast elements case,
 * or a dictionary for sparse array. Since Dictionary is a subtype
 * of FixedArray, the class can be used by both fast and slow cases.
 * The second parameter of the constructor, fast_elements, specifies
 * whether the storage is a FixedArray or Dictionary.
 *
 * An index limit is used to deal with the situation that a result array
 * length overflows 32-bit non-negative integer.
 */
class ArrayConcatVisitor {
 public:
  ArrayConcatVisitor(Isolate* isolate, Handle<FixedArray> storage,
                     bool fast_elements)
      : isolate_(isolate),
        storage_(Handle<FixedArray>::cast(
            isolate->global_handles()->Create(*storage))),
        index_offset_(0u),
        fast_elements_(fast_elements),
        exceeds_array_limit_(false) {}

  ~ArrayConcatVisitor() { clear_storage(); }

  void visit(uint32_t i, Handle<Object> elm) {
    if (i > JSObject::kMaxElementCount - index_offset_) {
      exceeds_array_limit_ = true;
      return;
    }
    uint32_t index = index_offset_ + i;

    if (fast_elements_) {
      if (index < static_cast<uint32_t>(storage_->length())) {
        storage_->set(index, *elm);
        return;
      }
      // Our initial estimate of length was foiled, possibly by
      // getters on the arrays increasing the length of later arrays
      // during iteration.
      // This shouldn't happen in anything but pathological cases.
      SetDictionaryMode();
      // Fall-through to dictionary mode.
    }
    DCHECK(!fast_elements_);
    Handle<SeededNumberDictionary> dict(
        SeededNumberDictionary::cast(*storage_));
    Handle<SeededNumberDictionary> result =
        SeededNumberDictionary::AtNumberPut(dict, index, elm);
    if (!result.is_identical_to(dict)) {
      // Dictionary needed to grow.
      clear_storage();
      set_storage(*result);
    }
  }

  void increase_index_offset(uint32_t delta) {
    if (JSObject::kMaxElementCount - index_offset_ < delta) {
      index_offset_ = JSObject::kMaxElementCount;
    } else {
      index_offset_ += delta;
    }
    // If the initial length estimate was off (see special case in visit()),
    // but the array blowing the limit didn't contain elements beyond the
    // provided-for index range, go to dictionary mode now.
    if (fast_elements_ &&
        index_offset_ >
            static_cast<uint32_t>(FixedArrayBase::cast(*storage_)->length())) {
      SetDictionaryMode();
    }
  }

  bool exceeds_array_limit() { return exceeds_array_limit_; }

  Handle<JSArray> ToArray() {
    Handle<JSArray> array = isolate_->factory()->NewJSArray(0);
    Handle<Object> length =
        isolate_->factory()->NewNumber(static_cast<double>(index_offset_));
    Handle<Map> map = JSObject::GetElementsTransitionMap(
        array, fast_elements_ ? FAST_HOLEY_ELEMENTS : DICTIONARY_ELEMENTS);
    array->set_map(*map);
    array->set_length(*length);
    array->set_elements(*storage_);
    return array;
  }

 private:
  // Convert storage to dictionary mode.
  void SetDictionaryMode() {
    DCHECK(fast_elements_);
    Handle<FixedArray> current_storage(*storage_);
    Handle<SeededNumberDictionary> slow_storage(
        SeededNumberDictionary::New(isolate_, current_storage->length()));
    uint32_t current_length = static_cast<uint32_t>(current_storage->length());
    for (uint32_t i = 0; i < current_length; i++) {
      HandleScope loop_scope(isolate_);
      Handle<Object> element(current_storage->get(i), isolate_);
      if (!element->IsTheHole()) {
        Handle<SeededNumberDictionary> new_storage =
            SeededNumberDictionary::AtNumberPut(slow_storage, i, element);
        if (!new_storage.is_identical_to(slow_storage)) {
          slow_storage = loop_scope.CloseAndEscape(new_storage);
        }
      }
    }
    clear_storage();
    set_storage(*slow_storage);
    fast_elements_ = false;
  }

  inline void clear_storage() {
    GlobalHandles::Destroy(Handle<Object>::cast(storage_).location());
  }

  inline void set_storage(FixedArray* storage) {
    storage_ =
        Handle<FixedArray>::cast(isolate_->global_handles()->Create(storage));
  }

  Isolate* isolate_;
  Handle<FixedArray> storage_;  // Always a global handle.
  // Index after last seen index. Always less than or equal to
  // JSObject::kMaxElementCount.
  uint32_t index_offset_;
  bool fast_elements_ : 1;
  bool exceeds_array_limit_ : 1;
};


static uint32_t EstimateElementCount(Handle<JSArray> array) {
  uint32_t length = static_cast<uint32_t>(array->length()->Number());
  int element_count = 0;
  switch (array->GetElementsKind()) {
    case FAST_SMI_ELEMENTS:
    case FAST_HOLEY_SMI_ELEMENTS:
    case FAST_ELEMENTS:
    case FAST_HOLEY_ELEMENTS: {
      // Fast elements can't have lengths that are not representable by
      // a 32-bit signed integer.
      DCHECK(static_cast<int32_t>(FixedArray::kMaxLength) >= 0);
      int fast_length = static_cast<int>(length);
      Handle<FixedArray> elements(FixedArray::cast(array->elements()));
      for (int i = 0; i < fast_length; i++) {
        if (!elements->get(i)->IsTheHole()) element_count++;
      }
      break;
    }
    case FAST_DOUBLE_ELEMENTS:
    case FAST_HOLEY_DOUBLE_ELEMENTS: {
      // Fast elements can't have lengths that are not representable by
      // a 32-bit signed integer.
      DCHECK(static_cast<int32_t>(FixedDoubleArray::kMaxLength) >= 0);
      int fast_length = static_cast<int>(length);
      if (array->elements()->IsFixedArray()) {
        DCHECK(FixedArray::cast(array->elements())->length() == 0);
        break;
      }
      Handle<FixedDoubleArray> elements(
          FixedDoubleArray::cast(array->elements()));
      for (int i = 0; i < fast_length; i++) {
        if (!elements->is_the_hole(i)) element_count++;
      }
      break;
    }
    case DICTIONARY_ELEMENTS: {
      Handle<SeededNumberDictionary> dictionary(
          SeededNumberDictionary::cast(array->elements()));
      int capacity = dictionary->Capacity();
      for (int i = 0; i < capacity; i++) {
        Handle<Object> key(dictionary->KeyAt(i), array->GetIsolate());
        if (dictionary->IsKey(*key)) {
          element_count++;
        }
      }
      break;
    }
    case SLOPPY_ARGUMENTS_ELEMENTS:
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) \
  case EXTERNAL_##TYPE##_ELEMENTS:                      \
  case TYPE##_ELEMENTS:

      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
      // External arrays are always dense.
      return length;
  }
  // As an estimate, we assume that the prototype doesn't contain any
  // inherited elements.
  return element_count;
}


template <class ExternalArrayClass, class ElementType>
static void IterateExternalArrayElements(Isolate* isolate,
                                         Handle<JSObject> receiver,
                                         bool elements_are_ints,
                                         bool elements_are_guaranteed_smis,
                                         ArrayConcatVisitor* visitor) {
  Handle<ExternalArrayClass> array(
      ExternalArrayClass::cast(receiver->elements()));
  uint32_t len = static_cast<uint32_t>(array->length());

  DCHECK(visitor != NULL);
  if (elements_are_ints) {
    if (elements_are_guaranteed_smis) {
      for (uint32_t j = 0; j < len; j++) {
        HandleScope loop_scope(isolate);
        Handle<Smi> e(Smi::FromInt(static_cast<int>(array->get_scalar(j))),
                      isolate);
        visitor->visit(j, e);
      }
    } else {
      for (uint32_t j = 0; j < len; j++) {
        HandleScope loop_scope(isolate);
        int64_t val = static_cast<int64_t>(array->get_scalar(j));
        if (Smi::IsValid(static_cast<intptr_t>(val))) {
          Handle<Smi> e(Smi::FromInt(static_cast<int>(val)), isolate);
          visitor->visit(j, e);
        } else {
          Handle<Object> e =
              isolate->factory()->NewNumber(static_cast<ElementType>(val));
          visitor->visit(j, e);
        }
      }
    }
  } else {
    for (uint32_t j = 0; j < len; j++) {
      HandleScope loop_scope(isolate);
      Handle<Object> e = isolate->factory()->NewNumber(array->get_scalar(j));
      visitor->visit(j, e);
    }
  }
}


// Used for sorting indices in a List<uint32_t>.
static int compareUInt32(const uint32_t* ap, const uint32_t* bp) {
  uint32_t a = *ap;
  uint32_t b = *bp;
  return (a == b) ? 0 : (a < b) ? -1 : 1;
}


static void CollectElementIndices(Handle<JSObject> object, uint32_t range,
                                  List<uint32_t>* indices) {
  Isolate* isolate = object->GetIsolate();
  ElementsKind kind = object->GetElementsKind();
  switch (kind) {
    case FAST_SMI_ELEMENTS:
    case FAST_ELEMENTS:
    case FAST_HOLEY_SMI_ELEMENTS:
    case FAST_HOLEY_ELEMENTS: {
      Handle<FixedArray> elements(FixedArray::cast(object->elements()));
      uint32_t length = static_cast<uint32_t>(elements->length());
      if (range < length) length = range;
      for (uint32_t i = 0; i < length; i++) {
        if (!elements->get(i)->IsTheHole()) {
          indices->Add(i);
        }
      }
      break;
    }
    case FAST_HOLEY_DOUBLE_ELEMENTS:
    case FAST_DOUBLE_ELEMENTS: {
      if (object->elements()->IsFixedArray()) {
        DCHECK(object->elements()->length() == 0);
        break;
      }
      Handle<FixedDoubleArray> elements(
          FixedDoubleArray::cast(object->elements()));
      uint32_t length = static_cast<uint32_t>(elements->length());
      if (range < length) length = range;
      for (uint32_t i = 0; i < length; i++) {
        if (!elements->is_the_hole(i)) {
          indices->Add(i);
        }
      }
      break;
    }
    case DICTIONARY_ELEMENTS: {
      Handle<SeededNumberDictionary> dict(
          SeededNumberDictionary::cast(object->elements()));
      uint32_t capacity = dict->Capacity();
      for (uint32_t j = 0; j < capacity; j++) {
        HandleScope loop_scope(isolate);
        Handle<Object> k(dict->KeyAt(j), isolate);
        if (dict->IsKey(*k)) {
          DCHECK(k->IsNumber());
          uint32_t index = static_cast<uint32_t>(k->Number());
          if (index < range) {
            indices->Add(index);
          }
        }
      }
      break;
    }
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) \
  case TYPE##_ELEMENTS:                                 \
  case EXTERNAL_##TYPE##_ELEMENTS:

      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
      {
        uint32_t length = static_cast<uint32_t>(
            FixedArrayBase::cast(object->elements())->length());
        if (range <= length) {
          length = range;
          // We will add all indices, so we might as well clear it first
          // and avoid duplicates.
          indices->Clear();
        }
        for (uint32_t i = 0; i < length; i++) {
          indices->Add(i);
        }
        if (length == range) return;  // All indices accounted for already.
        break;
      }
    case SLOPPY_ARGUMENTS_ELEMENTS: {
      MaybeHandle<Object> length_obj =
          Object::GetProperty(object, isolate->factory()->length_string());
      double length_num = length_obj.ToHandleChecked()->Number();
      uint32_t length = static_cast<uint32_t>(DoubleToInt32(length_num));
      ElementsAccessor* accessor = object->GetElementsAccessor();
      for (uint32_t i = 0; i < length; i++) {
        if (accessor->HasElement(object, object, i)) {
          indices->Add(i);
        }
      }
      break;
    }
  }

  PrototypeIterator iter(isolate, object);
  if (!iter.IsAtEnd()) {
    // The prototype will usually have no inherited element indices,
    // but we have to check.
    CollectElementIndices(
        Handle<JSObject>::cast(PrototypeIterator::GetCurrent(iter)), range,
        indices);
  }
}


/**
 * A helper function that visits elements of a JSArray in numerical
 * order.
 *
 * The visitor argument called for each existing element in the array
 * with the element index and the element's value.
 * Afterwards it increments the base-index of the visitor by the array
 * length.
 * Returns false if any access threw an exception, otherwise true.
 */
static bool IterateElements(Isolate* isolate, Handle<JSArray> receiver,
                            ArrayConcatVisitor* visitor) {
  uint32_t length = static_cast<uint32_t>(receiver->length()->Number());
  switch (receiver->GetElementsKind()) {
    case FAST_SMI_ELEMENTS:
    case FAST_ELEMENTS:
    case FAST_HOLEY_SMI_ELEMENTS:
    case FAST_HOLEY_ELEMENTS: {
      // Run through the elements FixedArray and use HasElement and GetElement
      // to check the prototype for missing elements.
      Handle<FixedArray> elements(FixedArray::cast(receiver->elements()));
      int fast_length = static_cast<int>(length);
      DCHECK(fast_length <= elements->length());
      for (int j = 0; j < fast_length; j++) {
        HandleScope loop_scope(isolate);
        Handle<Object> element_value(elements->get(j), isolate);
        if (!element_value->IsTheHole()) {
          visitor->visit(j, element_value);
        } else {
          Maybe<bool> maybe = JSReceiver::HasElement(receiver, j);
          if (!maybe.has_value) return false;
          if (maybe.value) {
            // Call GetElement on receiver, not its prototype, or getters won't
            // have the correct receiver.
            ASSIGN_RETURN_ON_EXCEPTION_VALUE(
                isolate, element_value,
                Object::GetElement(isolate, receiver, j), false);
            visitor->visit(j, element_value);
          }
        }
      }
      break;
    }
    case FAST_HOLEY_DOUBLE_ELEMENTS:
    case FAST_DOUBLE_ELEMENTS: {
      // Empty array is FixedArray but not FixedDoubleArray.
      if (length == 0) break;
      // Run through the elements FixedArray and use HasElement and GetElement
      // to check the prototype for missing elements.
      if (receiver->elements()->IsFixedArray()) {
        DCHECK(receiver->elements()->length() == 0);
        break;
      }
      Handle<FixedDoubleArray> elements(
          FixedDoubleArray::cast(receiver->elements()));
      int fast_length = static_cast<int>(length);
      DCHECK(fast_length <= elements->length());
      for (int j = 0; j < fast_length; j++) {
        HandleScope loop_scope(isolate);
        if (!elements->is_the_hole(j)) {
          double double_value = elements->get_scalar(j);
          Handle<Object> element_value =
              isolate->factory()->NewNumber(double_value);
          visitor->visit(j, element_value);
        } else {
          Maybe<bool> maybe = JSReceiver::HasElement(receiver, j);
          if (!maybe.has_value) return false;
          if (maybe.value) {
            // Call GetElement on receiver, not its prototype, or getters won't
            // have the correct receiver.
            Handle<Object> element_value;
            ASSIGN_RETURN_ON_EXCEPTION_VALUE(
                isolate, element_value,
                Object::GetElement(isolate, receiver, j), false);
            visitor->visit(j, element_value);
          }
        }
      }
      break;
    }
    case DICTIONARY_ELEMENTS: {
      Handle<SeededNumberDictionary> dict(receiver->element_dictionary());
      List<uint32_t> indices(dict->Capacity() / 2);
      // Collect all indices in the object and the prototypes less
      // than length. This might introduce duplicates in the indices list.
      CollectElementIndices(receiver, length, &indices);
      indices.Sort(&compareUInt32);
      int j = 0;
      int n = indices.length();
      while (j < n) {
        HandleScope loop_scope(isolate);
        uint32_t index = indices[j];
        Handle<Object> element;
        ASSIGN_RETURN_ON_EXCEPTION_VALUE(
            isolate, element, Object::GetElement(isolate, receiver, index),
            false);
        visitor->visit(index, element);
        // Skip to next different index (i.e., omit duplicates).
        do {
          j++;
        } while (j < n && indices[j] == index);
      }
      break;
    }
    case EXTERNAL_UINT8_CLAMPED_ELEMENTS: {
      Handle<ExternalUint8ClampedArray> pixels(
          ExternalUint8ClampedArray::cast(receiver->elements()));
      for (uint32_t j = 0; j < length; j++) {
        Handle<Smi> e(Smi::FromInt(pixels->get_scalar(j)), isolate);
        visitor->visit(j, e);
      }
      break;
    }
    case EXTERNAL_INT8_ELEMENTS: {
      IterateExternalArrayElements<ExternalInt8Array, int8_t>(
          isolate, receiver, true, true, visitor);
      break;
    }
    case EXTERNAL_UINT8_ELEMENTS: {
      IterateExternalArrayElements<ExternalUint8Array, uint8_t>(
          isolate, receiver, true, true, visitor);
      break;
    }
    case EXTERNAL_INT16_ELEMENTS: {
      IterateExternalArrayElements<ExternalInt16Array, int16_t>(
          isolate, receiver, true, true, visitor);
      break;
    }
    case EXTERNAL_UINT16_ELEMENTS: {
      IterateExternalArrayElements<ExternalUint16Array, uint16_t>(
          isolate, receiver, true, true, visitor);
      break;
    }
    case EXTERNAL_INT32_ELEMENTS: {
      IterateExternalArrayElements<ExternalInt32Array, int32_t>(
          isolate, receiver, true, false, visitor);
      break;
    }
    case EXTERNAL_UINT32_ELEMENTS: {
      IterateExternalArrayElements<ExternalUint32Array, uint32_t>(
          isolate, receiver, true, false, visitor);
      break;
    }
    case EXTERNAL_FLOAT32_ELEMENTS: {
      IterateExternalArrayElements<ExternalFloat32Array, float>(
          isolate, receiver, false, false, visitor);
      break;
    }
    case EXTERNAL_FLOAT64_ELEMENTS: {
      IterateExternalArrayElements<ExternalFloat64Array, double>(
          isolate, receiver, false, false, visitor);
      break;
    }
    default:
      UNREACHABLE();
      break;
  }
  visitor->increase_index_offset(length);
  return true;
}


/**
 * Array::concat implementation.
 * See ECMAScript 262, 15.4.4.4.
 * TODO(581): Fix non-compliance for very large concatenations and update to
 * following the ECMAScript 5 specification.
 */
RUNTIME_FUNCTION(Runtime_ArrayConcat) {
  HandleScope handle_scope(isolate);
  DCHECK(args.length() == 1);

  CONVERT_ARG_HANDLE_CHECKED(JSArray, arguments, 0);
  int argument_count = static_cast<int>(arguments->length()->Number());
  RUNTIME_ASSERT(arguments->HasFastObjectElements());
  Handle<FixedArray> elements(FixedArray::cast(arguments->elements()));

  // Pass 1: estimate the length and number of elements of the result.
  // The actual length can be larger if any of the arguments have getters
  // that mutate other arguments (but will otherwise be precise).
  // The number of elements is precise if there are no inherited elements.

  ElementsKind kind = FAST_SMI_ELEMENTS;

  uint32_t estimate_result_length = 0;
  uint32_t estimate_nof_elements = 0;
  for (int i = 0; i < argument_count; i++) {
    HandleScope loop_scope(isolate);
    Handle<Object> obj(elements->get(i), isolate);
    uint32_t length_estimate;
    uint32_t element_estimate;
    if (obj->IsJSArray()) {
      Handle<JSArray> array(Handle<JSArray>::cast(obj));
      length_estimate = static_cast<uint32_t>(array->length()->Number());
      if (length_estimate != 0) {
        ElementsKind array_kind =
            GetPackedElementsKind(array->map()->elements_kind());
        if (IsMoreGeneralElementsKindTransition(kind, array_kind)) {
          kind = array_kind;
        }
      }
      element_estimate = EstimateElementCount(array);
    } else {
      if (obj->IsHeapObject()) {
        if (obj->IsNumber()) {
          if (IsMoreGeneralElementsKindTransition(kind, FAST_DOUBLE_ELEMENTS)) {
            kind = FAST_DOUBLE_ELEMENTS;
          }
        } else if (IsMoreGeneralElementsKindTransition(kind, FAST_ELEMENTS)) {
          kind = FAST_ELEMENTS;
        }
      }
      length_estimate = 1;
      element_estimate = 1;
    }
    // Avoid overflows by capping at kMaxElementCount.
    if (JSObject::kMaxElementCount - estimate_result_length < length_estimate) {
      estimate_result_length = JSObject::kMaxElementCount;
    } else {
      estimate_result_length += length_estimate;
    }
    if (JSObject::kMaxElementCount - estimate_nof_elements < element_estimate) {
      estimate_nof_elements = JSObject::kMaxElementCount;
    } else {
      estimate_nof_elements += element_estimate;
    }
  }

  // If estimated number of elements is more than half of length, a
  // fixed array (fast case) is more time and space-efficient than a
  // dictionary.
  bool fast_case = (estimate_nof_elements * 2) >= estimate_result_length;

  if (fast_case && kind == FAST_DOUBLE_ELEMENTS) {
    Handle<FixedArrayBase> storage =
        isolate->factory()->NewFixedDoubleArray(estimate_result_length);
    int j = 0;
    bool failure = false;
    if (estimate_result_length > 0) {
      Handle<FixedDoubleArray> double_storage =
          Handle<FixedDoubleArray>::cast(storage);
      for (int i = 0; i < argument_count; i++) {
        Handle<Object> obj(elements->get(i), isolate);
        if (obj->IsSmi()) {
          double_storage->set(j, Smi::cast(*obj)->value());
          j++;
        } else if (obj->IsNumber()) {
          double_storage->set(j, obj->Number());
          j++;
        } else {
          JSArray* array = JSArray::cast(*obj);
          uint32_t length = static_cast<uint32_t>(array->length()->Number());
          switch (array->map()->elements_kind()) {
            case FAST_HOLEY_DOUBLE_ELEMENTS:
            case FAST_DOUBLE_ELEMENTS: {
              // Empty array is FixedArray but not FixedDoubleArray.
              if (length == 0) break;
              FixedDoubleArray* elements =
                  FixedDoubleArray::cast(array->elements());
              for (uint32_t i = 0; i < length; i++) {
                if (elements->is_the_hole(i)) {
                  // TODO(jkummerow/verwaest): We could be a bit more clever
                  // here: Check if there are no elements/getters on the
                  // prototype chain, and if so, allow creation of a holey
                  // result array.
                  // Same thing below (holey smi case).
                  failure = true;
                  break;
                }
                double double_value = elements->get_scalar(i);
                double_storage->set(j, double_value);
                j++;
              }
              break;
            }
            case FAST_HOLEY_SMI_ELEMENTS:
            case FAST_SMI_ELEMENTS: {
              FixedArray* elements(FixedArray::cast(array->elements()));
              for (uint32_t i = 0; i < length; i++) {
                Object* element = elements->get(i);
                if (element->IsTheHole()) {
                  failure = true;
                  break;
                }
                int32_t int_value = Smi::cast(element)->value();
                double_storage->set(j, int_value);
                j++;
              }
              break;
            }
            case FAST_HOLEY_ELEMENTS:
            case FAST_ELEMENTS:
              DCHECK_EQ(0, length);
              break;
            default:
              UNREACHABLE();
          }
        }
        if (failure) break;
      }
    }
    if (!failure) {
      Handle<JSArray> array = isolate->factory()->NewJSArray(0);
      Smi* length = Smi::FromInt(j);
      Handle<Map> map;
      map = JSObject::GetElementsTransitionMap(array, kind);
      array->set_map(*map);
      array->set_length(length);
      array->set_elements(*storage);
      return *array;
    }
    // In case of failure, fall through.
  }

  Handle<FixedArray> storage;
  if (fast_case) {
    // The backing storage array must have non-existing elements to preserve
    // holes across concat operations.
    storage =
        isolate->factory()->NewFixedArrayWithHoles(estimate_result_length);
  } else {
    // TODO(126): move 25% pre-allocation logic into Dictionary::Allocate
    uint32_t at_least_space_for =
        estimate_nof_elements + (estimate_nof_elements >> 2);
    storage = Handle<FixedArray>::cast(
        SeededNumberDictionary::New(isolate, at_least_space_for));
  }

  ArrayConcatVisitor visitor(isolate, storage, fast_case);

  for (int i = 0; i < argument_count; i++) {
    Handle<Object> obj(elements->get(i), isolate);
    if (obj->IsJSArray()) {
      Handle<JSArray> array = Handle<JSArray>::cast(obj);
      if (!IterateElements(isolate, array, &visitor)) {
        return isolate->heap()->exception();
      }
    } else {
      visitor.visit(0, obj);
      visitor.increase_index_offset(1);
    }
  }

  if (visitor.exceeds_array_limit()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate,
        NewRangeError("invalid_array_length", HandleVector<Object>(NULL, 0)));
  }
  return *visitor.ToArray();
}


// Moves all own elements of an object, that are below a limit, to positions
// starting at zero. All undefined values are placed after non-undefined values,
// and are followed by non-existing element. Does not change the length
// property.
// Returns the number of non-undefined elements collected.
// Returns -1 if hole removal is not supported by this method.
RUNTIME_FUNCTION(Runtime_RemoveArrayHoles) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_NUMBER_CHECKED(uint32_t, limit, Uint32, args[1]);
  return *JSObject::PrepareElementsForSort(object, limit);
}


// Move contents of argument 0 (an array) to argument 1 (an array)
RUNTIME_FUNCTION(Runtime_MoveArrayContents) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, from, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, to, 1);
  JSObject::ValidateElements(from);
  JSObject::ValidateElements(to);

  Handle<FixedArrayBase> new_elements(from->elements());
  ElementsKind from_kind = from->GetElementsKind();
  Handle<Map> new_map = JSObject::GetElementsTransitionMap(to, from_kind);
  JSObject::SetMapAndElements(to, new_map, new_elements);
  to->set_length(from->length());

  JSObject::ResetElements(from);
  from->set_length(Smi::FromInt(0));

  JSObject::ValidateElements(to);
  return *to;
}


// How many elements does this object/array have?
RUNTIME_FUNCTION(Runtime_EstimateNumberOfElements) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, array, 0);
  Handle<FixedArrayBase> elements(array->elements(), isolate);
  SealHandleScope shs(isolate);
  if (elements->IsDictionary()) {
    int result =
        Handle<SeededNumberDictionary>::cast(elements)->NumberOfElements();
    return Smi::FromInt(result);
  } else {
    DCHECK(array->length()->IsSmi());
    // For packed elements, we know the exact number of elements
    int length = elements->length();
    ElementsKind kind = array->GetElementsKind();
    if (IsFastPackedElementsKind(kind)) {
      return Smi::FromInt(length);
    }
    // For holey elements, take samples from the buffer checking for holes
    // to generate the estimate.
    const int kNumberOfHoleCheckSamples = 97;
    int increment = (length < kNumberOfHoleCheckSamples)
                        ? 1
                        : static_cast<int>(length / kNumberOfHoleCheckSamples);
    ElementsAccessor* accessor = array->GetElementsAccessor();
    int holes = 0;
    for (int i = 0; i < length; i += increment) {
      if (!accessor->HasElement(array, array, i, elements)) {
        ++holes;
      }
    }
    int estimate = static_cast<int>((kNumberOfHoleCheckSamples - holes) /
                                    kNumberOfHoleCheckSamples * length);
    return Smi::FromInt(estimate);
  }
}


// Returns an array that tells you where in the [0, length) interval an array
// might have elements.  Can either return an array of keys (positive integers
// or undefined) or a number representing the positive length of an interval
// starting at index 0.
// Intervals can span over some keys that are not in the object.
RUNTIME_FUNCTION(Runtime_GetArrayKeys) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, array, 0);
  CONVERT_NUMBER_CHECKED(uint32_t, length, Uint32, args[1]);
  if (array->elements()->IsDictionary()) {
    Handle<FixedArray> keys = isolate->factory()->empty_fixed_array();
    for (PrototypeIterator iter(isolate, array,
                                PrototypeIterator::START_AT_RECEIVER);
         !iter.IsAtEnd(); iter.Advance()) {
      if (PrototypeIterator::GetCurrent(iter)->IsJSProxy() ||
          JSObject::cast(*PrototypeIterator::GetCurrent(iter))
              ->HasIndexedInterceptor()) {
        // Bail out if we find a proxy or interceptor, likely not worth
        // collecting keys in that case.
        return *isolate->factory()->NewNumberFromUint(length);
      }
      Handle<JSObject> current =
          Handle<JSObject>::cast(PrototypeIterator::GetCurrent(iter));
      Handle<FixedArray> current_keys =
          isolate->factory()->NewFixedArray(current->NumberOfOwnElements(NONE));
      current->GetOwnElementKeys(*current_keys, NONE);
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
          isolate, keys, FixedArray::UnionOfKeys(keys, current_keys));
    }
    // Erase any keys >= length.
    // TODO(adamk): Remove this step when the contract of %GetArrayKeys
    // is changed to let this happen on the JS side.
    for (int i = 0; i < keys->length(); i++) {
      if (NumberToUint32(keys->get(i)) >= length) keys->set_undefined(i);
    }
    return *isolate->factory()->NewJSArrayWithElements(keys);
  } else {
    RUNTIME_ASSERT(array->HasFastSmiOrObjectElements() ||
                   array->HasFastDoubleElements());
    uint32_t actual_length = static_cast<uint32_t>(array->elements()->length());
    return *isolate->factory()->NewNumberFromUint(Min(actual_length, length));
  }
}


RUNTIME_FUNCTION(Runtime_LookupAccessor) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, receiver, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);
  CONVERT_SMI_ARG_CHECKED(flag, 2);
  AccessorComponent component = flag == 0 ? ACCESSOR_GETTER : ACCESSOR_SETTER;
  if (!receiver->IsJSObject()) return isolate->heap()->undefined_value();
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      JSObject::GetAccessor(Handle<JSObject>::cast(receiver), name, component));
  return *result;
}


// Collect the raw data for a stack trace.  Returns an array of 4
// element segments each containing a receiver, function, code and
// native code offset.
RUNTIME_FUNCTION(Runtime_CollectStackTrace) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, error_object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, caller, 1);

  if (!isolate->bootstrapper()->IsActive()) {
    // Optionally capture a more detailed stack trace for the message.
    isolate->CaptureAndSetDetailedStackTrace(error_object);
    // Capture a simple stack trace for the stack property.
    isolate->CaptureAndSetSimpleStackTrace(error_object, caller);
  }
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_LoadMutableDouble) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Smi, index, 1);
  RUNTIME_ASSERT((index->value() & 1) == 1);
  FieldIndex field_index =
      FieldIndex::ForLoadByFieldIndex(object->map(), index->value());
  if (field_index.is_inobject()) {
    RUNTIME_ASSERT(field_index.property_index() <
                   object->map()->inobject_properties());
  } else {
    RUNTIME_ASSERT(field_index.outobject_array_index() <
                   object->properties()->length());
  }
  Handle<Object> raw_value(object->RawFastPropertyAt(field_index), isolate);
  RUNTIME_ASSERT(raw_value->IsMutableHeapNumber());
  return *Object::WrapForRead(isolate, raw_value, Representation::Double());
}


RUNTIME_FUNCTION(Runtime_TryMigrateInstance) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  if (!object->IsJSObject()) return Smi::FromInt(0);
  Handle<JSObject> js_object = Handle<JSObject>::cast(object);
  if (!js_object->map()->is_deprecated()) return Smi::FromInt(0);
  // This call must not cause lazy deopts, because it's called from deferred
  // code where we can't handle lazy deopts for lack of a suitable bailout
  // ID. So we just try migration and signal failure if necessary,
  // which will also trigger a deopt.
  if (!JSObject::TryMigrateInstance(js_object)) return Smi::FromInt(0);
  return *object;
}


RUNTIME_FUNCTION(Runtime_GetFromCache) {
  SealHandleScope shs(isolate);
  // This is only called from codegen, so checks might be more lax.
  CONVERT_ARG_CHECKED(JSFunctionResultCache, cache, 0);
  CONVERT_ARG_CHECKED(Object, key, 1);

  {
    DisallowHeapAllocation no_alloc;

    int finger_index = cache->finger_index();
    Object* o = cache->get(finger_index);
    if (o == key) {
      // The fastest case: hit the same place again.
      return cache->get(finger_index + 1);
    }

    for (int i = finger_index - 2; i >= JSFunctionResultCache::kEntriesIndex;
         i -= 2) {
      o = cache->get(i);
      if (o == key) {
        cache->set_finger_index(i);
        return cache->get(i + 1);
      }
    }

    int size = cache->size();
    DCHECK(size <= cache->length());

    for (int i = size - 2; i > finger_index; i -= 2) {
      o = cache->get(i);
      if (o == key) {
        cache->set_finger_index(i);
        return cache->get(i + 1);
      }
    }
  }

  // There is no value in the cache.  Invoke the function and cache result.
  HandleScope scope(isolate);

  Handle<JSFunctionResultCache> cache_handle(cache);
  Handle<Object> key_handle(key, isolate);
  Handle<Object> value;
  {
    Handle<JSFunction> factory(JSFunction::cast(
        cache_handle->get(JSFunctionResultCache::kFactoryIndex)));
    // TODO(antonm): consider passing a receiver when constructing a cache.
    Handle<JSObject> receiver(isolate->global_proxy());
    // This handle is nor shared, nor used later, so it's safe.
    Handle<Object> argv[] = {key_handle};
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, value,
        Execution::Call(isolate, factory, receiver, arraysize(argv), argv));
  }

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    cache_handle->JSFunctionResultCacheVerify();
  }
#endif

  // Function invocation may have cleared the cache.  Reread all the data.
  int finger_index = cache_handle->finger_index();
  int size = cache_handle->size();

  // If we have spare room, put new data into it, otherwise evict post finger
  // entry which is likely to be the least recently used.
  int index = -1;
  if (size < cache_handle->length()) {
    cache_handle->set_size(size + JSFunctionResultCache::kEntrySize);
    index = size;
  } else {
    index = finger_index + JSFunctionResultCache::kEntrySize;
    if (index == cache_handle->length()) {
      index = JSFunctionResultCache::kEntriesIndex;
    }
  }

  DCHECK(index % 2 == 0);
  DCHECK(index >= JSFunctionResultCache::kEntriesIndex);
  DCHECK(index < cache_handle->length());

  cache_handle->set(index, *key_handle);
  cache_handle->set(index + 1, *value);
  cache_handle->set_finger_index(index);

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    cache_handle->JSFunctionResultCacheVerify();
  }
#endif

  return *value;
}


RUNTIME_FUNCTION(Runtime_MessageGetStartPosition) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(JSMessageObject, message, 0);
  return Smi::FromInt(message->start_position());
}


RUNTIME_FUNCTION(Runtime_MessageGetScript) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(JSMessageObject, message, 0);
  return message->script();
}


RUNTIME_FUNCTION(Runtime_IS_VAR) {
  UNREACHABLE();  // implemented as macro in the parser
  return NULL;
}


RUNTIME_FUNCTION(Runtime_IsJSGlobalProxy) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(obj->IsJSGlobalProxy());
}


RUNTIME_FUNCTION(Runtime_IsObserved) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);

  if (!args[0]->IsJSReceiver()) return isolate->heap()->false_value();
  CONVERT_ARG_CHECKED(JSReceiver, obj, 0);
  DCHECK(!obj->IsJSGlobalProxy() || !obj->map()->is_observed());
  return isolate->heap()->ToBoolean(obj->map()->is_observed());
}


RUNTIME_FUNCTION(Runtime_SetIsObserved) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, obj, 0);
  RUNTIME_ASSERT(!obj->IsJSGlobalProxy());
  if (obj->IsJSProxy()) return isolate->heap()->undefined_value();
  RUNTIME_ASSERT(!obj->map()->is_observed());

  DCHECK(obj->IsJSObject());
  JSObject::SetObserved(Handle<JSObject>::cast(obj));
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_EnqueueMicrotask) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, microtask, 0);
  isolate->EnqueueMicrotask(microtask);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_RunMicrotasks) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0);
  isolate->RunMicrotasks();
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_GetObservationState) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 0);
  return isolate->heap()->observation_state();
}


static bool ContextsHaveSameOrigin(Handle<Context> context1,
                                   Handle<Context> context2) {
  return context1->security_token() == context2->security_token();
}


RUNTIME_FUNCTION(Runtime_ObserverObjectAndRecordHaveSameOrigin) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, observer, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, record, 2);

  Handle<Context> observer_context(observer->context()->native_context());
  Handle<Context> object_context(object->GetCreationContext());
  Handle<Context> record_context(record->GetCreationContext());

  return isolate->heap()->ToBoolean(
      ContextsHaveSameOrigin(object_context, observer_context) &&
      ContextsHaveSameOrigin(object_context, record_context));
}


RUNTIME_FUNCTION(Runtime_ObjectWasCreatedInCurrentOrigin) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);

  Handle<Context> creation_context(object->GetCreationContext(), isolate);
  return isolate->heap()->ToBoolean(
      ContextsHaveSameOrigin(creation_context, isolate->native_context()));
}


RUNTIME_FUNCTION(Runtime_GetObjectContextObjectObserve) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);

  Handle<Context> context(object->GetCreationContext(), isolate);
  return context->native_object_observe();
}


RUNTIME_FUNCTION(Runtime_GetObjectContextObjectGetNotifier) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);

  Handle<Context> context(object->GetCreationContext(), isolate);
  return context->native_object_get_notifier();
}


RUNTIME_FUNCTION(Runtime_GetObjectContextNotifierPerformChange) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object_info, 0);

  Handle<Context> context(object_info->GetCreationContext(), isolate);
  return context->native_object_notifier_perform_change();
}


static Object* ArrayConstructorCommon(Isolate* isolate,
                                      Handle<JSFunction> constructor,
                                      Handle<AllocationSite> site,
                                      Arguments* caller_args) {
  Factory* factory = isolate->factory();

  bool holey = false;
  bool can_use_type_feedback = true;
  if (caller_args->length() == 1) {
    Handle<Object> argument_one = caller_args->at<Object>(0);
    if (argument_one->IsSmi()) {
      int value = Handle<Smi>::cast(argument_one)->value();
      if (value < 0 || value >= JSObject::kInitialMaxFastElementArray) {
        // the array is a dictionary in this case.
        can_use_type_feedback = false;
      } else if (value != 0) {
        holey = true;
      }
    } else {
      // Non-smi length argument produces a dictionary
      can_use_type_feedback = false;
    }
  }

  Handle<JSArray> array;
  if (!site.is_null() && can_use_type_feedback) {
    ElementsKind to_kind = site->GetElementsKind();
    if (holey && !IsFastHoleyElementsKind(to_kind)) {
      to_kind = GetHoleyElementsKind(to_kind);
      // Update the allocation site info to reflect the advice alteration.
      site->SetElementsKind(to_kind);
    }

    // We should allocate with an initial map that reflects the allocation site
    // advice. Therefore we use AllocateJSObjectFromMap instead of passing
    // the constructor.
    Handle<Map> initial_map(constructor->initial_map(), isolate);
    if (to_kind != initial_map->elements_kind()) {
      initial_map = Map::AsElementsKind(initial_map, to_kind);
    }

    // If we don't care to track arrays of to_kind ElementsKind, then
    // don't emit a memento for them.
    Handle<AllocationSite> allocation_site;
    if (AllocationSite::GetMode(to_kind) == TRACK_ALLOCATION_SITE) {
      allocation_site = site;
    }

    array = Handle<JSArray>::cast(factory->NewJSObjectFromMap(
        initial_map, NOT_TENURED, true, allocation_site));
  } else {
    array = Handle<JSArray>::cast(factory->NewJSObject(constructor));

    // We might need to transition to holey
    ElementsKind kind = constructor->initial_map()->elements_kind();
    if (holey && !IsFastHoleyElementsKind(kind)) {
      kind = GetHoleyElementsKind(kind);
      JSObject::TransitionElementsKind(array, kind);
    }
  }

  factory->NewJSArrayStorage(array, 0, 0, DONT_INITIALIZE_ARRAY_ELEMENTS);

  ElementsKind old_kind = array->GetElementsKind();
  RETURN_FAILURE_ON_EXCEPTION(
      isolate, ArrayConstructInitializeElements(array, caller_args));
  if (!site.is_null() &&
      (old_kind != array->GetElementsKind() || !can_use_type_feedback)) {
    // The arguments passed in caused a transition. This kind of complexity
    // can't be dealt with in the inlined hydrogen array constructor case.
    // We must mark the allocationsite as un-inlinable.
    site->SetDoNotInlineCall();
  }
  return *array;
}


RUNTIME_FUNCTION(Runtime_ArrayConstructor) {
  HandleScope scope(isolate);
  // If we get 2 arguments then they are the stub parameters (constructor, type
  // info).  If we get 4, then the first one is a pointer to the arguments
  // passed by the caller, and the last one is the length of the arguments
  // passed to the caller (redundant, but useful to check on the deoptimizer
  // with an assert).
  Arguments empty_args(0, NULL);
  bool no_caller_args = args.length() == 2;
  DCHECK(no_caller_args || args.length() == 4);
  int parameters_start = no_caller_args ? 0 : 1;
  Arguments* caller_args =
      no_caller_args ? &empty_args : reinterpret_cast<Arguments*>(args[0]);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, constructor, parameters_start);
  CONVERT_ARG_HANDLE_CHECKED(Object, type_info, parameters_start + 1);
#ifdef DEBUG
  if (!no_caller_args) {
    CONVERT_SMI_ARG_CHECKED(arg_count, parameters_start + 2);
    DCHECK(arg_count == caller_args->length());
  }
#endif

  Handle<AllocationSite> site;
  if (!type_info.is_null() &&
      *type_info != isolate->heap()->undefined_value()) {
    site = Handle<AllocationSite>::cast(type_info);
    DCHECK(!site->SitePointsToLiteral());
  }

  return ArrayConstructorCommon(isolate, constructor, site, caller_args);
}


RUNTIME_FUNCTION(Runtime_InternalArrayConstructor) {
  HandleScope scope(isolate);
  Arguments empty_args(0, NULL);
  bool no_caller_args = args.length() == 1;
  DCHECK(no_caller_args || args.length() == 3);
  int parameters_start = no_caller_args ? 0 : 1;
  Arguments* caller_args =
      no_caller_args ? &empty_args : reinterpret_cast<Arguments*>(args[0]);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, constructor, parameters_start);
#ifdef DEBUG
  if (!no_caller_args) {
    CONVERT_SMI_ARG_CHECKED(arg_count, parameters_start + 1);
    DCHECK(arg_count == caller_args->length());
  }
#endif
  return ArrayConstructorCommon(isolate, constructor,
                                Handle<AllocationSite>::null(), caller_args);
}


RUNTIME_FUNCTION(Runtime_NormalizeElements) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, array, 0);
  RUNTIME_ASSERT(!array->HasExternalArrayElements() &&
                 !array->HasFixedTypedArrayElements());
  JSObject::NormalizeElements(array);
  return *array;
}


RUNTIME_FUNCTION(Runtime_MaxSmi) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 0);
  return Smi::FromInt(Smi::kMaxValue);
}


// TODO(dcarney): remove this function when TurboFan supports it.
// Takes the object to be iterated over and the result of GetPropertyNamesFast
// Returns pair (cache_array, cache_type).
RUNTIME_FUNCTION_RETURN_PAIR(Runtime_ForInInit) {
  SealHandleScope scope(isolate);
  DCHECK(args.length() == 2);
  // This simulates CONVERT_ARG_HANDLE_CHECKED for calls returning pairs.
  // Not worth creating a macro atm as this function should be removed.
  if (!args[0]->IsJSReceiver() || !args[1]->IsObject()) {
    Object* error = isolate->ThrowIllegalOperation();
    return MakePair(error, isolate->heap()->undefined_value());
  }
  Handle<JSReceiver> object = args.at<JSReceiver>(0);
  Handle<Object> cache_type = args.at<Object>(1);
  if (cache_type->IsMap()) {
    // Enum cache case.
    if (Map::EnumLengthBits::decode(Map::cast(*cache_type)->bit_field3()) ==
        0) {
      // 0 length enum.
      // Can't handle this case in the graph builder,
      // so transform it into the empty fixed array case.
      return MakePair(isolate->heap()->empty_fixed_array(), Smi::FromInt(1));
    }
    return MakePair(object->map()->instance_descriptors()->GetEnumCache(),
                    *cache_type);
  } else {
    // FixedArray case.
    Smi* new_cache_type = Smi::FromInt(object->IsJSProxy() ? 0 : 1);
    return MakePair(*Handle<FixedArray>::cast(cache_type), new_cache_type);
  }
}


// TODO(dcarney): remove this function when TurboFan supports it.
RUNTIME_FUNCTION(Runtime_ForInCacheArrayLength) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(Object, cache_type, 0);
  CONVERT_ARG_HANDLE_CHECKED(FixedArray, array, 1);
  int length = 0;
  if (cache_type->IsMap()) {
    length = Map::cast(*cache_type)->EnumLength();
  } else {
    DCHECK(cache_type->IsSmi());
    length = array->length();
  }
  return Smi::FromInt(length);
}


// TODO(dcarney): remove this function when TurboFan supports it.
// Takes (the object to be iterated over,
//        cache_array from ForInInit,
//        cache_type from ForInInit,
//        the current index)
// Returns pair (array[index], needs_filtering).
RUNTIME_FUNCTION_RETURN_PAIR(Runtime_ForInNext) {
  SealHandleScope scope(isolate);
  DCHECK(args.length() == 4);
  int32_t index;
  // This simulates CONVERT_ARG_HANDLE_CHECKED for calls returning pairs.
  // Not worth creating a macro atm as this function should be removed.
  if (!args[0]->IsJSReceiver() || !args[1]->IsFixedArray() ||
      !args[2]->IsObject() || !args[3]->ToInt32(&index)) {
    Object* error = isolate->ThrowIllegalOperation();
    return MakePair(error, isolate->heap()->undefined_value());
  }
  Handle<JSReceiver> object = args.at<JSReceiver>(0);
  Handle<FixedArray> array = args.at<FixedArray>(1);
  Handle<Object> cache_type = args.at<Object>(2);
  // Figure out first if a slow check is needed for this object.
  bool slow_check_needed = false;
  if (cache_type->IsMap()) {
    if (object->map() != Map::cast(*cache_type)) {
      // Object transitioned.  Need slow check.
      slow_check_needed = true;
    }
  } else {
    // No slow check needed for proxies.
    slow_check_needed = Smi::cast(*cache_type)->value() == 1;
  }
  return MakePair(array->get(index),
                  isolate->heap()->ToBoolean(slow_check_needed));
}


// ----------------------------------------------------------------------------
// Reference implementation for inlined runtime functions.  Only used when the
// compiler does not support a certain intrinsic.  Don't optimize these, but
// implement the intrinsic in the respective compiler instead.

// TODO(mstarzinger): These are place-holder stubs for TurboFan and will
// eventually all have a C++ implementation and this macro will be gone.
#define U(name)                               \
  RUNTIME_FUNCTION(RuntimeReference_##name) { \
    UNIMPLEMENTED();                          \
    return NULL;                              \
  }

U(IsStringWrapperSafeForDefaultValueOf)
U(DebugBreakInOptimizedCode)

#undef U


RUNTIME_FUNCTION(RuntimeReference_IsSmi) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(obj->IsSmi());
}


RUNTIME_FUNCTION(RuntimeReference_IsNonNegativeSmi) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(obj->IsSmi() &&
                                    Smi::cast(obj)->value() >= 0);
}


RUNTIME_FUNCTION(RuntimeReference_IsArray) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(obj->IsJSArray());
}


RUNTIME_FUNCTION(RuntimeReference_IsRegExp) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(obj->IsJSRegExp());
}


RUNTIME_FUNCTION(RuntimeReference_IsConstructCall) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 0);
  JavaScriptFrameIterator it(isolate);
  JavaScriptFrame* frame = it.frame();
  return isolate->heap()->ToBoolean(frame->IsConstructor());
}


RUNTIME_FUNCTION(RuntimeReference_CallFunction) {
  SealHandleScope shs(isolate);
  return __RT_impl_Runtime_Call(args, isolate);
}


RUNTIME_FUNCTION(RuntimeReference_ArgumentsLength) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 0);
  JavaScriptFrameIterator it(isolate);
  JavaScriptFrame* frame = it.frame();
  return Smi::FromInt(frame->GetArgumentsLength());
}


RUNTIME_FUNCTION(RuntimeReference_Arguments) {
  SealHandleScope shs(isolate);
  return __RT_impl_Runtime_GetArgumentsProperty(args, isolate);
}


RUNTIME_FUNCTION(RuntimeReference_ValueOf) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  if (!obj->IsJSValue()) return obj;
  return JSValue::cast(obj)->value();
}


RUNTIME_FUNCTION(RuntimeReference_SetValueOf) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  CONVERT_ARG_CHECKED(Object, value, 1);
  if (!obj->IsJSValue()) return value;
  JSValue::cast(obj)->set_value(value);
  return value;
}


RUNTIME_FUNCTION(RuntimeReference_ObjectEquals) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_CHECKED(Object, obj1, 0);
  CONVERT_ARG_CHECKED(Object, obj2, 1);
  return isolate->heap()->ToBoolean(obj1 == obj2);
}


RUNTIME_FUNCTION(RuntimeReference_IsObject) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  if (!obj->IsHeapObject()) return isolate->heap()->false_value();
  if (obj->IsNull()) return isolate->heap()->true_value();
  if (obj->IsUndetectableObject()) return isolate->heap()->false_value();
  Map* map = HeapObject::cast(obj)->map();
  bool is_non_callable_spec_object =
      map->instance_type() >= FIRST_NONCALLABLE_SPEC_OBJECT_TYPE &&
      map->instance_type() <= LAST_NONCALLABLE_SPEC_OBJECT_TYPE;
  return isolate->heap()->ToBoolean(is_non_callable_spec_object);
}


RUNTIME_FUNCTION(RuntimeReference_IsFunction) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(obj->IsJSFunction());
}


RUNTIME_FUNCTION(RuntimeReference_IsUndetectableObject) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(obj->IsUndetectableObject());
}


RUNTIME_FUNCTION(RuntimeReference_IsSpecObject) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(obj->IsSpecObject());
}


RUNTIME_FUNCTION(RuntimeReference_HasCachedArrayIndex) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  return isolate->heap()->false_value();
}


RUNTIME_FUNCTION(RuntimeReference_GetCachedArrayIndex) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(RuntimeReference_FastOneByteArrayJoin) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 2);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(RuntimeReference_ClassOf) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  if (!obj->IsJSReceiver()) return isolate->heap()->null_value();
  return JSReceiver::cast(obj)->class_name();
}


RUNTIME_FUNCTION(RuntimeReference_GetFromCache) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_SMI_ARG_CHECKED(id, 0);
  args[0] = isolate->native_context()->jsfunction_result_caches()->get(id);
  return __RT_impl_Runtime_GetFromCache(args, isolate);
}


// ----------------------------------------------------------------------------
// Implementation of Runtime

#define F(name, number_of_args, result_size)                                  \
  {                                                                           \
    Runtime::k##name, Runtime::RUNTIME, #name, FUNCTION_ADDR(Runtime_##name), \
        number_of_args, result_size                                           \
  }                                                                           \
  ,


#define I(name, number_of_args, result_size)                                \
  {                                                                         \
    Runtime::kInline##name, Runtime::INLINE, "_" #name,                     \
        FUNCTION_ADDR(RuntimeReference_##name), number_of_args, result_size \
  }                                                                         \
  ,


#define IO(name, number_of_args, result_size)                              \
  {                                                                        \
    Runtime::kInlineOptimized##name, Runtime::INLINE_OPTIMIZED, "_" #name, \
        FUNCTION_ADDR(Runtime_##name), number_of_args, result_size         \
  }                                                                        \
  ,


static const Runtime::Function kIntrinsicFunctions[] = {
    RUNTIME_FUNCTION_LIST(F) INLINE_OPTIMIZED_FUNCTION_LIST(F)
    INLINE_FUNCTION_LIST(I) INLINE_OPTIMIZED_FUNCTION_LIST(IO)};

#undef IO
#undef I
#undef F


void Runtime::InitializeIntrinsicFunctionNames(Isolate* isolate,
                                               Handle<NameDictionary> dict) {
  DCHECK(dict->NumberOfElements() == 0);
  HandleScope scope(isolate);
  for (int i = 0; i < kNumFunctions; ++i) {
    const char* name = kIntrinsicFunctions[i].name;
    if (name == NULL) continue;
    Handle<NameDictionary> new_dict = NameDictionary::Add(
        dict, isolate->factory()->InternalizeUtf8String(name),
        Handle<Smi>(Smi::FromInt(i), isolate),
        PropertyDetails(NONE, NORMAL, Representation::None()));
    // The dictionary does not need to grow.
    CHECK(new_dict.is_identical_to(dict));
  }
}


const Runtime::Function* Runtime::FunctionForName(Handle<String> name) {
  Heap* heap = name->GetHeap();
  int entry = heap->intrinsic_function_names()->FindEntry(name);
  if (entry != kNotFound) {
    Object* smi_index = heap->intrinsic_function_names()->ValueAt(entry);
    int function_index = Smi::cast(smi_index)->value();
    return &(kIntrinsicFunctions[function_index]);
  }
  return NULL;
}


const Runtime::Function* Runtime::FunctionForEntry(Address entry) {
  for (size_t i = 0; i < arraysize(kIntrinsicFunctions); ++i) {
    if (entry == kIntrinsicFunctions[i].entry) {
      return &(kIntrinsicFunctions[i]);
    }
  }
  return NULL;
}


const Runtime::Function* Runtime::FunctionForId(Runtime::FunctionId id) {
  return &(kIntrinsicFunctions[static_cast<int>(id)]);
}
}
}  // namespace v8::internal
