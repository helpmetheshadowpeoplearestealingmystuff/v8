// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/turbo-assembler.h"

#include "src/builtins/builtins.h"
#include "src/builtins/constants-table-builder.h"
#include "src/codegen/external-reference-encoder.h"
#include "src/common/globals.h"
#include "src/execution/isolate-data.h"
#include "src/execution/isolate-inl.h"
#include "src/snapshot/embedded/embedded-data.h"

namespace v8 {
namespace internal {

TurboAssemblerBase::TurboAssemblerBase(Isolate* isolate,
                                       const AssemblerOptions& options,
                                       CodeObjectRequired create_code_object,
                                       std::unique_ptr<AssemblerBuffer> buffer)
    : Assembler(options, std::move(buffer)), isolate_(isolate) {
  if (create_code_object == CodeObjectRequired::kYes) {
    code_object_ = Handle<HeapObject>::New(
        ReadOnlyRoots(isolate).self_reference_marker(), isolate);
  }
}

Address TurboAssemblerBase::BuiltinEntry(Builtin builtin) {
  DCHECK(Builtins::IsBuiltinId(builtin));
  if (isolate_ != nullptr) {
    Address entry =
        isolate_->builtin_entry_table()[static_cast<int32_t>(builtin)];
    DCHECK_EQ(entry, EmbeddedData::FromBlob(isolate_).InstructionStartOfBuiltin(
                         builtin));
    return entry;
  }
  EmbeddedData d = EmbeddedData::FromBlob();
  return d.InstructionStartOfBuiltin(builtin);
}

void TurboAssemblerBase::IndirectLoadConstant(Register destination,
                                              Handle<HeapObject> object) {
  CHECK(root_array_available_);

  // Before falling back to the (fairly slow) lookup from the constants table,
  // check if any of the fast paths can be applied.

  Builtin builtin;
  RootIndex root_index;
  if (isolate()->roots_table().IsRootHandle(object, &root_index)) {
    // Roots are loaded relative to the root register.
    LoadRoot(destination, root_index);
  } else if (isolate()->builtins()->IsBuiltinHandle(object, &builtin)) {
    // Similar to roots, builtins may be loaded from the builtins table.
    LoadRootRelative(destination, RootRegisterOffsetForBuiltin(builtin));
  } else if (object.is_identical_to(code_object_) &&
             Builtins::IsBuiltinId(maybe_builtin_)) {
    // The self-reference loaded through Codevalue() may also be a builtin
    // and thus viable for a fast load.
    LoadRootRelative(destination, RootRegisterOffsetForBuiltin(maybe_builtin_));
  } else {
    CHECK(isolate()->IsGeneratingEmbeddedBuiltins());
    // Ensure the given object is in the builtins constants table and fetch its
    // index.
    BuiltinsConstantsTableBuilder* builder =
        isolate()->builtins_constants_table_builder();
    uint32_t index = builder->AddObject(object);

    // Slow load from the constants table.
    LoadFromConstantsTable(destination, index);
  }
}

void TurboAssemblerBase::IndirectLoadExternalReference(
    Register destination, ExternalReference reference) {
  CHECK(root_array_available_);

  if (IsAddressableThroughRootRegister(isolate(), reference)) {
    // Some external references can be efficiently loaded as an offset from
    // kRootRegister.
    intptr_t offset =
        RootRegisterOffsetForExternalReference(isolate(), reference);
    LoadRootRegisterOffset(destination, offset);
  } else {
    // Otherwise, do a memory load from the external reference table.
    LoadRootRelative(
        destination,
        RootRegisterOffsetForExternalReferenceTableEntry(isolate(), reference));
  }
}

// static
int32_t TurboAssemblerBase::RootRegisterOffsetForRootIndex(
    RootIndex root_index) {
  return IsolateData::root_slot_offset(root_index);
}

// static
int32_t TurboAssemblerBase::RootRegisterOffsetForBuiltin(Builtin builtin) {
  return IsolateData::BuiltinSlotOffset(builtin);
}

// static
intptr_t TurboAssemblerBase::RootRegisterOffsetForExternalReference(
    Isolate* isolate, const ExternalReference& reference) {
  return static_cast<intptr_t>(reference.address() - isolate->isolate_root());
}

// static
int32_t TurboAssemblerBase::RootRegisterOffsetForExternalReferenceTableEntry(
    Isolate* isolate, const ExternalReference& reference) {
  // Encode as an index into the external reference table stored on the
  // isolate.
  ExternalReferenceEncoder encoder(isolate);
  ExternalReferenceEncoder::Value v = encoder.Encode(reference.address());
  CHECK(!v.is_from_api());

  return IsolateData::external_reference_table_offset() +
         ExternalReferenceTable::OffsetOfEntry(v.index());
}

// static
bool TurboAssemblerBase::IsAddressableThroughRootRegister(
    Isolate* isolate, const ExternalReference& reference) {
  Address address = reference.address();
  return isolate->root_register_addressable_region().contains(address);
}

}  // namespace internal
}  // namespace v8
