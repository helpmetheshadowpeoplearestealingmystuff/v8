// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LOOKUP_CACHE_INL_H_
#define V8_LOOKUP_CACHE_INL_H_

#include "src/lookup-cache.h"

#include "src/objects-inl.h"

namespace v8 {
namespace internal {

// static
int DescriptorLookupCache::Hash(Object* source, Name* name) {
  DCHECK(name->IsUniqueName());
  // Uses only lower 32 bits if pointers are larger.
  uint32_t source_hash =
      static_cast<uint32_t>(reinterpret_cast<uintptr_t>(source)) >>
      kPointerSizeLog2;
  uint32_t name_hash = name->hash_field();
  return (source_hash ^ name_hash) % kLength;
}

int DescriptorLookupCache::Lookup(Map* source, Name* name) {
  int index = Hash(source, name);
  Key& key = keys_[index];
  if ((key.source == source) && (key.name == name)) return results_[index];
  return kAbsent;
}

void DescriptorLookupCache::Update(Map* source, Name* name, int result) {
  DCHECK(result != kAbsent);
  int index = Hash(source, name);
  Key& key = keys_[index];
  key.source = source;
  key.name = name;
  results_[index] = result;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_LOOKUP_CACHE_INL_H_
