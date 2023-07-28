// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/assembler-inl.h"
#include "src/codegen/reloc-info.h"
#include "src/heap/evacuation-verifier-inl.h"
#include "src/objects/map-inl.h"

namespace v8 {
namespace internal {

#ifdef VERIFY_HEAP

EvacuationVerifier::EvacuationVerifier(Heap* heap)
    : ObjectVisitorWithCageBases(heap), heap_(heap) {}

void EvacuationVerifier::Run() {
  DCHECK(!heap_->sweeping_in_progress());
  VerifyRoots();
  VerifyEvacuation(heap_->new_space());
  VerifyEvacuation(heap_->old_space());
  VerifyEvacuation(heap_->code_space());
  if (heap_->shared_space()) VerifyEvacuation(heap_->shared_space());
}

void EvacuationVerifier::VisitPointers(HeapObject host, ObjectSlot start,
                                       ObjectSlot end) {
  VerifyPointersImpl(start, end);
}

void EvacuationVerifier::VisitPointers(HeapObject host, MaybeObjectSlot start,
                                       MaybeObjectSlot end) {
  VerifyPointersImpl(start, end);
}

void EvacuationVerifier::VisitInstructionStreamPointer(
    Code host, InstructionStreamSlot slot) {
  Object maybe_code = slot.load(code_cage_base());
  HeapObject code;
  // The slot might contain smi during Code creation, so skip it.
  if (maybe_code.GetHeapObject(&code)) {
    VerifyHeapObjectImpl(code);
  }
}

void EvacuationVerifier::VisitRootPointers(Root root, const char* description,
                                           FullObjectSlot start,
                                           FullObjectSlot end) {
  VerifyPointersImpl(start, end);
}

void EvacuationVerifier::VisitMapPointer(HeapObject object) {
  VerifyHeapObjectImpl(object->map(cage_base()));
}

void EvacuationVerifier::VisitCodeTarget(InstructionStream host,
                                         RelocInfo* rinfo) {
  InstructionStream target =
      InstructionStream::FromTargetAddress(rinfo->target_address());
  VerifyHeapObjectImpl(target);
}

void EvacuationVerifier::VisitEmbeddedPointer(InstructionStream host,
                                              RelocInfo* rinfo) {
  VerifyHeapObjectImpl(rinfo->target_object(cage_base()));
}

void EvacuationVerifier::VerifyRoots() {
  heap_->IterateRootsIncludingClients(
      this,
      base::EnumSet<SkipRoot>{SkipRoot::kWeak, SkipRoot::kConservativeStack});
}

void EvacuationVerifier::VerifyEvacuationOnPage(Address start, Address end) {
  Address current = start;
  while (current < end) {
    HeapObject object = HeapObject::FromAddress(current);
    if (!IsFreeSpaceOrFiller(object, cage_base())) {
      object->Iterate(cage_base(), this);
    }
    current += ALIGN_TO_ALLOCATION_ALIGNMENT(object->Size(cage_base()));
  }
}

void EvacuationVerifier::VerifyEvacuation(NewSpace* space) {
  if (!space) return;
  if (v8_flags.minor_ms) {
    VerifyEvacuation(PagedNewSpace::From(space)->paged_space());
    return;
  }
  PageRange range(space->first_allocatable_address(), space->top());
  for (auto it = range.begin(); it != range.end();) {
    Page* page = *(it++);
    Address current = page->area_start();
    Address limit = it != range.end() ? page->area_end() : space->top();
    CHECK(limit == space->top() || !page->Contains(space->top()));
    VerifyEvacuationOnPage(current, limit);
  }
}

void EvacuationVerifier::VerifyEvacuation(PagedSpaceBase* space) {
  for (Page* p : *space) {
    if (p->IsEvacuationCandidate()) continue;
    if (p->Contains(space->top())) {
      CodePageMemoryModificationScope memory_modification_scope(p);
      heap_->CreateFillerObjectAt(
          space->top(), static_cast<int>(space->limit() - space->top()));
    }
    VerifyEvacuationOnPage(p->area_start(), p->area_end());
  }
}

#endif  // VERIFY_HEAP

}  // namespace internal
}  // namespace v8
