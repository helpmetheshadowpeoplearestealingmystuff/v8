// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/external-reference-table.h"

#include "src/accessors.h"
#include "src/counters.h"
#include "src/external-reference.h"
#include "src/ic/stub-cache.h"

#if defined(DEBUG) && defined(V8_OS_LINUX) && !defined(V8_OS_ANDROID)
#define SYMBOLIZE_FUNCTION
#include <execinfo.h>
#include <vector>
#endif  // DEBUG && V8_OS_LINUX && !V8_OS_ANDROID

namespace v8 {
namespace internal {

// Forward declarations for C++ builtins.
#define FORWARD_DECLARE(Name) \
  Object* Builtin_##Name(int argc, Object** args, Isolate* isolate);
BUILTIN_LIST_C(FORWARD_DECLARE)
#undef FORWARD_DECLARE

ExternalReferenceTable* ExternalReferenceTable::instance(Isolate* isolate) {
  ExternalReferenceTable* external_reference_table =
      isolate->external_reference_table();
  if (external_reference_table == nullptr) {
    external_reference_table = new ExternalReferenceTable(isolate);
    isolate->set_external_reference_table(external_reference_table);
  }
  return external_reference_table;
}

ExternalReferenceTable::ExternalReferenceTable(Isolate* isolate) {
  // nullptr is preserved through serialization/deserialization.
  Add(nullptr, "nullptr");
  AddReferences(isolate);
  AddBuiltins(isolate);
  AddRuntimeFunctions(isolate);
  AddIsolateAddresses(isolate);
  AddAccessors(isolate);
  AddStubCache(isolate);
}

const char* ExternalReferenceTable::ResolveSymbol(void* address) {
#ifdef SYMBOLIZE_FUNCTION
  char** names = backtrace_symbols(&address, 1);
  const char* name = names[0];
  // The array of names is malloc'ed. However, each name string is static
  // and do not need to be freed.
  free(names);
  return name;
#else
  return "<unresolved>";
#endif  // SYMBOLIZE_FUNCTION
}

void ExternalReferenceTable::Add(Address address, const char* name) {
  refs_.emplace_back(address, name);
}

void ExternalReferenceTable::AddReferences(Isolate* isolate) {
#define ADD_EXTERNAL_REFERENCE(name, desc) \
  Add(ExternalReference::name(isolate).address(), desc);
  EXTERNAL_REFERENCE_LIST(ADD_EXTERNAL_REFERENCE)
#undef ADD_EXTERNAL_REFERENCE
}

void ExternalReferenceTable::AddBuiltins(Isolate* isolate) {
  struct CBuiltinEntry {
    Address address;
    const char* name;
  };
  static const CBuiltinEntry c_builtins[] = {
#define DEF_ENTRY(Name, ...) {FUNCTION_ADDR(&Builtin_##Name), "Builtin_" #Name},
      BUILTIN_LIST_C(DEF_ENTRY)
#undef DEF_ENTRY
  };
  for (unsigned i = 0; i < arraysize(c_builtins); ++i) {
    Add(ExternalReference(c_builtins[i].address, isolate).address(),
        c_builtins[i].name);
  }
}

void ExternalReferenceTable::AddRuntimeFunctions(Isolate* isolate) {
  struct RuntimeEntry {
    Runtime::FunctionId id;
    const char* name;
  };

  static const RuntimeEntry runtime_functions[] = {
#define RUNTIME_ENTRY(name, i1, i2) {Runtime::k##name, "Runtime::" #name},
      FOR_EACH_INTRINSIC(RUNTIME_ENTRY)
#undef RUNTIME_ENTRY
  };

  for (unsigned i = 0; i < arraysize(runtime_functions); ++i) {
    ExternalReference ref(runtime_functions[i].id, isolate);
    Add(ref.address(), runtime_functions[i].name);
  }
}

void ExternalReferenceTable::AddIsolateAddresses(Isolate* isolate) {
  // Top addresses
  static const char* address_names[] = {
#define BUILD_NAME_LITERAL(Name, name) "Isolate::" #name "_address",
      FOR_EACH_ISOLATE_ADDRESS_NAME(BUILD_NAME_LITERAL) nullptr
#undef BUILD_NAME_LITERAL
  };

  for (int i = 0; i < IsolateAddressId::kIsolateAddressCount; ++i) {
    Add(isolate->get_address_from_id(static_cast<IsolateAddressId>(i)),
        address_names[i]);
  }
}

void ExternalReferenceTable::AddAccessors(Isolate* isolate) {
  // Accessors
  struct AccessorRefTable {
    Address address;
    const char* name;
  };

  static const AccessorRefTable getters[] = {
#define ACCESSOR_INFO_DECLARATION(accessor_name, AccessorName) \
  {FUNCTION_ADDR(&Accessors::AccessorName##Getter),            \
   "Accessors::" #AccessorName "Getter"}, /* NOLINT(whitespace/indent) */
      ACCESSOR_INFO_LIST(ACCESSOR_INFO_DECLARATION)
#undef ACCESSOR_INFO_DECLARATION
  };
  static const AccessorRefTable setters[] = {
#define ACCESSOR_SETTER_DECLARATION(name) \
  { FUNCTION_ADDR(&Accessors::name), "Accessors::" #name},
      ACCESSOR_SETTER_LIST(ACCESSOR_SETTER_DECLARATION)
#undef ACCESSOR_SETTER_DECLARATION
  };

  for (unsigned i = 0; i < arraysize(getters); ++i) {
    Add(getters[i].address, getters[i].name);
  }

  for (unsigned i = 0; i < arraysize(setters); ++i) {
    Add(setters[i].address, setters[i].name);
  }
}

void ExternalReferenceTable::AddStubCache(Isolate* isolate) {
  StubCache* load_stub_cache = isolate->load_stub_cache();

  // Stub cache tables
  Add(load_stub_cache->key_reference(StubCache::kPrimary).address(),
      "Load StubCache::primary_->key");
  Add(load_stub_cache->value_reference(StubCache::kPrimary).address(),
      "Load StubCache::primary_->value");
  Add(load_stub_cache->map_reference(StubCache::kPrimary).address(),
      "Load StubCache::primary_->map");
  Add(load_stub_cache->key_reference(StubCache::kSecondary).address(),
      "Load StubCache::secondary_->key");
  Add(load_stub_cache->value_reference(StubCache::kSecondary).address(),
      "Load StubCache::secondary_->value");
  Add(load_stub_cache->map_reference(StubCache::kSecondary).address(),
      "Load StubCache::secondary_->map");

  StubCache* store_stub_cache = isolate->store_stub_cache();

  // Stub cache tables
  Add(store_stub_cache->key_reference(StubCache::kPrimary).address(),
      "Store StubCache::primary_->key");
  Add(store_stub_cache->value_reference(StubCache::kPrimary).address(),
      "Store StubCache::primary_->value");
  Add(store_stub_cache->map_reference(StubCache::kPrimary).address(),
      "Store StubCache::primary_->map");
  Add(store_stub_cache->key_reference(StubCache::kSecondary).address(),
      "Store StubCache::secondary_->key");
  Add(store_stub_cache->value_reference(StubCache::kSecondary).address(),
      "Store StubCache::secondary_->value");
  Add(store_stub_cache->map_reference(StubCache::kSecondary).address(),
      "Store StubCache::secondary_->map");
}

}  // namespace internal
}  // namespace v8

#undef SYMBOLIZE_FUNCTION
