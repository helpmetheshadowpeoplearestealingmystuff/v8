// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/read-only-promotion.h"

#include <unordered_set>

#include "src/common/assert-scope.h"
#include "src/execution/isolate.h"
#include "src/heap/combined-heap.h"
#include "src/heap/heap.h"
#include "src/objects/heap-object-inl.h"
#include "src/sandbox/external-pointer-table.h"

namespace v8 {
namespace internal {
namespace {

// Convenience aliases:
using HeapObjectSet = std::unordered_set<Tagged<HeapObject>, Object::Hasher,
                                         Object::KeyEqualSafe>;
using HeapObjectMap = std::unordered_map<Tagged<HeapObject>, Tagged<HeapObject>,
                                         Object::Hasher, Object::KeyEqualSafe>;
bool Contains(const HeapObjectSet& s, Tagged<HeapObject> o) {
  return s.count(o) != 0;
}
bool Contains(const HeapObjectMap& s, Tagged<HeapObject> o) {
  return s.count(o) != 0;
}

class Committee final {
 public:
  static std::vector<Tagged<HeapObject>> DeterminePromotees(
      Isolate* isolate, const DisallowGarbageCollection& no_gc,
      const SafepointScope& safepoint_scope) {
    return Committee(isolate).DeterminePromotees(safepoint_scope);
  }

 private:
  explicit Committee(Isolate* isolate) : isolate_(isolate) {}

  std::vector<Tagged<HeapObject>> DeterminePromotees(
      const SafepointScope& safepoint_scope) {
    DCHECK(promo_accepted_.empty());
    DCHECK(promo_rejected_.empty());

    // We assume that a full and precise GC has reclaimed all dead objects
    // and therefore that no filtering of unreachable objects is required here.
    HeapObjectIterator it(isolate_->heap(), safepoint_scope);
    for (Tagged<HeapObject> o = it.Next(); !o.is_null(); o = it.Next()) {
      DCHECK(!o.InReadOnlySpace());

      // Note that cycles prevent us from promoting/rejecting each subgraph as
      // we visit it, since locally we cannot determine whether the deferred
      // decision on the 'cycle object' will be 'promote' or 'reject'. This
      // could be solved if necessary (with more complex code), but for now
      // there are no performance issues.
      HeapObjectSet accepted_subgraph;  // Either all are accepted or none.
      HeapObjectSet visited;            // Cycle detection.
      if (!EvaluateSubgraph(o, &accepted_subgraph, &visited)) continue;
      if (accepted_subgraph.empty()) continue;

      if (V8_UNLIKELY(v8_flags.trace_read_only_promotion)) {
        LogAcceptedPromotionSet(accepted_subgraph);
      }
      promo_accepted_.insert(accepted_subgraph.begin(),
                             accepted_subgraph.end());
    }

    // Return promotees as a sorted list. Note that sorting uses object
    // addresses; the list order is deterministic only if heap layout
    // itself is deterministic (see v8_flags.predictable).
    std::vector<Tagged<HeapObject>> promotees{promo_accepted_.begin(),
                                              promo_accepted_.end()};
    std::sort(promotees.begin(), promotees.end(), Object::Comparer());

    return promotees;
  }

  // Returns `false` if the subgraph rooted at `o` is rejected.
  // Returns `true` if it is accepted, or if we've reached a cycle and `o`
  // will be processed further up the callchain.
  bool EvaluateSubgraph(Tagged<HeapObject> o, HeapObjectSet* accepted_subgraph,
                        HeapObjectSet* visited) {
    if (o.InReadOnlySpace()) return true;
    if (Contains(promo_rejected_, o)) return false;
    if (Contains(promo_accepted_, o)) return true;
    if (Contains(*visited, o)) return true;
    visited->insert(o);
    if (!IsPromoCandidate(isolate_, o)) {
      const auto& [it, inserted] = promo_rejected_.insert(o);
      if (V8_UNLIKELY(v8_flags.trace_read_only_promotion) && inserted) {
        LogRejectedPromotionForFailedPredicate(o);
      }
      return false;
    }
    // Recurse into outgoing pointers.
    CandidateVisitor v(this, accepted_subgraph, visited);
    o->Iterate(isolate_, &v);
    if (!v.all_slots_are_promo_candidates()) {
      const auto& [it, inserted] = promo_rejected_.insert(o);
      if (V8_UNLIKELY(v8_flags.trace_read_only_promotion) && inserted) {
        LogRejectedPromotionForInvalidSubgraph(o,
                                               v.first_rejected_slot_offset());
      }
      return false;
    }

    accepted_subgraph->insert(o);
    return true;
  }

#define PROMO_CANDIDATE_TYPE_LIST(V) \
  V(AccessCheckInfo)                 \
  V(AccessorInfo)                    \
  V(CallHandlerInfo)                 \
  V(Code)                            \
  V(InterceptorInfo)                 \
  V(ScopeInfo)                       \
  V(SharedFunctionInfo)              \
  V(Symbol)
  // TODO(jgruber): Don't forget to extend ReadOnlyPromotionImpl::Verify when
  // adding new object types here.

  static bool IsPromoCandidate(Isolate* isolate, Tagged<HeapObject> o) {
    const InstanceType itype = o->map(isolate)->instance_type();
#define V(TYPE)                                            \
  if (InstanceTypeChecker::Is##TYPE(itype)) {              \
    return IsPromoCandidate##TYPE(isolate, TYPE::cast(o)); \
    /* NOLINTNEXTLINE(readability/braces) */               \
  } else
    PROMO_CANDIDATE_TYPE_LIST(V)
    /* if { ... } else */ {
      return false;
    }
#undef V
    UNREACHABLE();
  }
#undef PROMO_CANDIDATE_TYPE_LIST

#define DEF_PROMO_CANDIDATE(Type)                                        \
  static bool IsPromoCandidate##Type(Isolate* isolate, Tagged<Type> o) { \
    return true;                                                         \
  }

  DEF_PROMO_CANDIDATE(AccessCheckInfo)
  DEF_PROMO_CANDIDATE(AccessorInfo)
  DEF_PROMO_CANDIDATE(CallHandlerInfo)
  static bool IsPromoCandidateCode(Isolate* isolate, Tagged<Code> o) {
    return Builtins::kCodeObjectsAreInROSpace && o->is_builtin();
  }
  DEF_PROMO_CANDIDATE(InterceptorInfo)
  DEF_PROMO_CANDIDATE(ScopeInfo)
  static bool IsPromoCandidateSharedFunctionInfo(Isolate* isolate,
                                                 Tagged<SharedFunctionInfo> o) {
    // Only internal SFIs are guaranteed to remain immutable.
    if (o->has_script(kAcquireLoad)) return false;
    // kIllegal is used for js_global_object_function, which is created during
    // bootstrapping but never rooted. We currently assumed that all objects in
    // the snapshot are live. But RO space is 1) not GC'd and 2) serialized
    // verbatim, preserving dead objects. As a workaround, exclude this builtin
    // id from RO allocation.
    // TODO(jgruber): A better solution. Remove the liveness assumption (see
    // test-heap-profiler.cc)? Overwrite dead RO objects with fillers
    // pre-serialization? Implement a RO GC pass pre-serialization?
    return o->HasBuiltinId() && o->builtin_id() != Builtin::kIllegal;
  }
  DEF_PROMO_CANDIDATE(Symbol)

#undef DEF_PROMO_CANDIDATE

  // Recurses into all tagged slots of an object and tracks whether predicates
  // failed on any part of the subgraph.
  class CandidateVisitor : public ObjectVisitor {
   public:
    CandidateVisitor(Committee* committee, HeapObjectSet* accepted_subgraph,
                     HeapObjectSet* visited)
        : committee_(committee),
          accepted_subgraph_(accepted_subgraph),
          visited_(visited) {}

    int first_rejected_slot_offset() const {
      return first_rejected_slot_offset_;
    }
    bool all_slots_are_promo_candidates() const {
      return first_rejected_slot_offset_ == -1;
    }

    void VisitPointers(Tagged<HeapObject> host, MaybeObjectSlot start,
                       MaybeObjectSlot end) final {
      if (!all_slots_are_promo_candidates()) return;
      for (MaybeObjectSlot slot = start; slot < end; slot++) {
        MaybeObject maybe_object = slot.load(committee_->isolate_);
        Tagged<HeapObject> heap_object;
        if (!maybe_object.GetHeapObject(&heap_object)) continue;
        if (!committee_->EvaluateSubgraph(heap_object, accepted_subgraph_,
                                          visited_)) {
          first_rejected_slot_offset_ =
              static_cast<int>(slot.address() - host.address());
          DCHECK_GE(first_rejected_slot_offset_, 0);
          return;
        }
      }
    }
    void VisitPointers(Tagged<HeapObject> host, ObjectSlot start,
                       ObjectSlot end) final {
      VisitPointers(host, MaybeObjectSlot(start), MaybeObjectSlot(end));
    }
    void VisitInstructionStreamPointer(Tagged<Code> host,
                                       InstructionStreamSlot slot) final {
      DCHECK(host->is_builtin());
    }
    void VisitMapPointer(Tagged<HeapObject> host) final {
      MaybeObjectSlot slot = host->RawMaybeWeakField(HeapObject::kMapOffset);
      VisitPointers(host, slot, slot + 1);
    }

   private:
    Committee* const committee_;
    HeapObjectSet* const accepted_subgraph_;
    HeapObjectSet* const visited_;
    int first_rejected_slot_offset_ = -1;
  };

  static void LogAcceptedPromotionSet(const HeapObjectSet& os) {
    std::cout << "ro-promotion: accepted set {";
    for (Tagged<HeapObject> o : os) {
      std::cout << reinterpret_cast<void*>(o.ptr()) << ", ";
    }
    std::cout << "}\n";
  }

  static void LogRejectedPromotionForFailedPredicate(Tagged<HeapObject> o) {
    std::cout << "ro-promotion: rejected due to failed predicate "
              << reinterpret_cast<void*>(o.ptr()) << " ("
              << o->map()->instance_type() << ")"
              << "\n";
  }

  void LogRejectedPromotionForInvalidSubgraph(Tagged<HeapObject> o,
                                              int first_rejected_slot_offset) {
    std::cout << "ro-promotion: rejected due to rejected subgraph "
              << reinterpret_cast<void*>(o.ptr()) << " ("
              << o->map()->instance_type() << ")"
              << " at slot offset " << first_rejected_slot_offset << " ";

    MaybeObjectSlot slot = o->RawMaybeWeakField(first_rejected_slot_offset);
    MaybeObject maybe_object = slot.load(isolate_);
    Tagged<HeapObject> heap_object;
    if (maybe_object.GetHeapObject(&heap_object)) {
      std::cout << reinterpret_cast<void*>(heap_object.ptr()) << " ("
                << heap_object->map()->instance_type() << ")"
                << "\n";
    } else {
      std::cout << "<cleared weak object>\n";
    }
  }

  Isolate* const isolate_;
  HeapObjectSet promo_accepted_;
  HeapObjectSet promo_rejected_;
};

class ReadOnlyPromotionImpl final : public AllStatic {
 public:
  static void CopyToReadOnlyHeap(
      Isolate* isolate, const std::vector<Tagged<HeapObject>>& promotees,
      HeapObjectMap* moves) {
    ReadOnlySpace* rospace = isolate->heap()->read_only_space();
    for (Tagged<HeapObject> src : promotees) {
      const int size = src->Size(isolate);
      Tagged<HeapObject> dst =
          rospace->AllocateRaw(size, kTaggedAligned).ToObjectChecked();
      Heap::CopyBlock(dst.address(), src.address(), size);
      moves->emplace(src, dst);
    }
  }

  static void UpdatePointers(Isolate* isolate,
                             const SafepointScope& safepoint_scope,
                             const HeapObjectMap& moves) {
    Heap* heap = isolate->heap();
#ifdef V8_COMPRESS_POINTERS
    ExternalPointerTable::UnsealReadOnlySegmentScope unseal_scope(
        &isolate->external_pointer_table());
#endif  // V8_COMPRESS_POINTERS
    UpdatePointersVisitor v(isolate, &moves);

    // Iterate all roots.
    EmbedderStackStateScope stack_scope(
        isolate->heap(), EmbedderStackStateScope::kExplicitInvocation,
        StackState::kNoHeapPointers);
    heap->IterateRoots(&v, base::EnumSet<SkipRoot>{});

    // Iterate all objects on the mutable heap.
    // We assume that a full and precise GC has reclaimed all dead objects
    // and therefore that no filtering of unreachable objects is required here.
    HeapObjectIterator it(heap, safepoint_scope);
    for (Tagged<HeapObject> o = it.Next(); !o.is_null(); o = it.Next()) {
      o->Iterate(isolate, &v);
    }

    // Iterate all objects we just copied into RO space.
    for (auto [src, dst] : moves) {
      dst->Iterate(isolate, &v);
    }
  }

  static void Verify(Isolate* isolate, const SafepointScope& safepoint_scope) {
#ifdef DEBUG
    // Verify that certain objects were promoted as expected.
    //
    // Known objects.
    Heap* heap = isolate->heap();
    CHECK(heap->promise_all_resolve_element_shared_fun().InReadOnlySpace());
    // TODO(jgruber): Extend here with more objects as they are added to
    // the promotion algorithm.

    // Builtin Code objects.
    if (Builtins::kCodeObjectsAreInROSpace) {
      Builtins* builtins = isolate->builtins();
      for (int i = 0; i < Builtins::kBuiltinCount; i++) {
        CHECK(builtins->code(static_cast<Builtin>(i)).InReadOnlySpace());
      }
    }
#endif  // DEBUG
  }

 private:
  class UpdatePointersVisitor final : public ObjectVisitor, public RootVisitor {
   public:
    UpdatePointersVisitor(Isolate* isolate, const HeapObjectMap* moves)
        : isolate_(isolate), moves_(moves) {
      for (auto [src, dst] : *moves_) {
        moves_reverse_lookup_.emplace(dst, src);
      }
    }

    // The RootVisitor interface.
    void VisitRootPointers(Root root, const char* description,
                           FullObjectSlot start, FullObjectSlot end) final {
      for (FullObjectSlot slot = start; slot < end; slot++) {
        ProcessSlot(root, slot);
      }
    }

    // The ObjectVisitor interface.
    void VisitPointers(Tagged<HeapObject> host, MaybeObjectSlot start,
                       MaybeObjectSlot end) final {
      for (MaybeObjectSlot slot = start; slot < end; slot++) {
        ProcessSlot(host, slot);
      }
    }
    void VisitPointers(Tagged<HeapObject> host, ObjectSlot start,
                       ObjectSlot end) final {
      VisitPointers(host, MaybeObjectSlot(start), MaybeObjectSlot(end));
    }
    void VisitInstructionStreamPointer(Tagged<Code> host,
                                       InstructionStreamSlot slot) final {
      // InstructionStream objects never move to RO space.
    }
    void VisitMapPointer(Tagged<HeapObject> host) final {
      ProcessSlot(host, host->RawMaybeWeakField(HeapObject::kMapOffset));
    }
    void VisitExternalPointer(Tagged<HeapObject> host, ExternalPointerSlot slot,
                              ExternalPointerTag tag) final {
#ifdef V8_ENABLE_SANDBOX
      auto it = moves_reverse_lookup_.find(host);
      if (it == moves_reverse_lookup_.end()) return;

      // If we reach here, `host` is a moved object with external pointer slots
      // located in RO space. To preserve the 1:1 relation between slots and
      // table entries, allocate a new entry (in
      // read_only_external_pointer_space) now.
      RecordProcessedSlotIfDebug(slot.address());
      Address slot_value = slot.load(isolate_, tag);
      slot.init(isolate_, slot_value, tag);

      if (V8_UNLIKELY(v8_flags.trace_read_only_promotion_verbose)) {
        LogUpdatedExternalPointerTableEntry(host, slot, slot_value);
      }
#endif  // V8_ENABLE_SANDBOX
    }
    void VisitIndirectPointer(Tagged<HeapObject> host, IndirectPointerSlot slot,
                              IndirectPointerMode mode) final {}
    void VisitIndirectPointerTableEntry(Tagged<HeapObject> host,
                                        IndirectPointerSlot slot) final {
#ifdef V8_CODE_POINTER_SANDBOXING
      // When an object owning an indirect pointer table entry is relocated, it
      // needs to update the entry to point to its new location. Currently, only
      // Code objects are referenced through indirect pointers, and they use the
      // code pointer table.
      CHECK(IsCode(host));

      // Due to the way we handle baseline code during serialization (we
      // manually skip over them), we may encounter such live Code objects in
      // mutable space during iteration. Do a lookup to make sure we only
      // update CPT entries for moved objects.
      auto it = moves_reverse_lookup_.find(host);
      if (it == moves_reverse_lookup_.end()) return;

      // If we reach here, `host` is a moved Code object located in RO space.
      CHECK(host.InReadOnlySpace());
      RecordProcessedSlotIfDebug(slot.address());

      Tagged<Code> dead_code = Code::cast(it->second);
      CHECK(IsCode(dead_code));
      CHECK(!dead_code.InReadOnlySpace());

      IndirectPointerHandle handle = slot.Relaxed_LoadHandle();
      CodePointerTable* cpt = GetProcessWideCodePointerTable();
      CHECK_EQ(dead_code, Object(cpt->GetCodeObject(handle)));

      // The old Code object (in mutable space) is dead. To preserve the 1:1
      // relation between Code objects and CPT entries, overwrite it immediately
      // with the filler object.
      isolate_->heap()->CreateFillerObjectAt(dead_code.address(), Code::kSize);

      // Update the CPT entry to point at the moved RO Code object.
      cpt->SetCodeObject(handle, host.ptr());

      if (V8_UNLIKELY(v8_flags.trace_read_only_promotion_verbose)) {
        LogUpdatedCodePointerTableEntry(host, slot, dead_code);
      }
#else
      UNREACHABLE();
#endif
    }
    void VisitRootPointers(Root root, const char* description,
                           OffHeapObjectSlot start,
                           OffHeapObjectSlot end) override {
      // We shouldn't have moved any string table contents (which is what
      // OffHeapObjectSlot currently refers to).
      for (OffHeapObjectSlot slot = start; slot < end; slot++) {
        Tagged<Object> o = slot.load(isolate_);
        if (!IsHeapObject(o)) continue;
        CHECK(!Contains(*moves_, HeapObject::cast(o)));
      }
    }

   private:
    void ProcessSlot(Root root, FullObjectSlot slot) {
      Tagged<Object> old_slot_value_obj = slot.load(isolate_);
      if (!IsHeapObject(old_slot_value_obj)) return;
      Tagged<HeapObject> old_slot_value = HeapObject::cast(old_slot_value_obj);
      auto it = moves_->find(old_slot_value);
      if (it == moves_->end()) return;
      Tagged<HeapObject> new_slot_value = it->second;
      slot.store(new_slot_value);
      if (V8_UNLIKELY(v8_flags.trace_read_only_promotion_verbose)) {
        LogUpdatedPointer(root, slot, old_slot_value, new_slot_value);
      }
    }
    void ProcessSlot(Tagged<HeapObject> host, MaybeObjectSlot slot) {
      Tagged<HeapObject> old_slot_value;
      if (!slot.load(isolate_).GetHeapObject(&old_slot_value)) return;
      auto it = moves_->find(old_slot_value);
      if (it == moves_->end()) return;
      Tagged<HeapObject> new_slot_value = it->second;
      slot.store(MaybeObject::FromObject(new_slot_value));
      if (V8_UNLIKELY(v8_flags.trace_read_only_promotion_verbose)) {
        LogUpdatedPointer(host, slot, old_slot_value, new_slot_value);
      }
    }

    void LogUpdatedPointer(Root root, FullObjectSlot slot,
                           Tagged<HeapObject> old_slot_value,
                           Tagged<HeapObject> new_slot_value) {
      std::cout << "ro-promotion: updated pointer {root "
                << static_cast<int>(root) << " slot "
                << reinterpret_cast<void*>(slot.address()) << " from "
                << reinterpret_cast<void*>(old_slot_value.ptr()) << " to "
                << reinterpret_cast<void*>(new_slot_value.ptr()) << "}\n";
    }
    void LogUpdatedPointer(Tagged<HeapObject> host, MaybeObjectSlot slot,
                           Tagged<HeapObject> old_slot_value,
                           Tagged<HeapObject> new_slot_value) {
      std::cout << "ro-promotion: updated pointer {host "
                << reinterpret_cast<void*>(host.address()) << " slot "
                << reinterpret_cast<void*>(slot.address()) << " from "
                << reinterpret_cast<void*>(old_slot_value.ptr()) << " to "
                << reinterpret_cast<void*>(new_slot_value.ptr()) << "}\n";
    }
    void LogUpdatedExternalPointerTableEntry(Tagged<HeapObject> host,
                                             ExternalPointerSlot slot,
                                             Address slot_value) {
      std::cout << "ro-promotion: updated external pointer slot {host "
                << reinterpret_cast<void*>(host.address()) << " slot "
                << reinterpret_cast<void*>(slot.address()) << " slot_value "
                << reinterpret_cast<void*>(slot_value) << "}\n";
    }
    void LogUpdatedCodePointerTableEntry(Tagged<HeapObject> host,
                                         IndirectPointerSlot slot,
                                         Tagged<Code> old_code_object) {
      std::cout << "ro-promotion: updated code pointer table entry {host "
                << reinterpret_cast<void*>(host.address()) << " slot "
                << reinterpret_cast<void*>(slot.address()) << " from "
                << reinterpret_cast<void*>(old_code_object.ptr()) << "}\n";
    }

#ifdef DEBUG
    void RecordProcessedSlotIfDebug(Address slot_address) {
      // If this fails, we're visiting some object multiple times by accident.
      CHECK_EQ(processed_slots_.count(slot_address), 0);
      processed_slots_.insert(slot_address);
    }
    std::unordered_set<Address> processed_slots_;  // To avoid dupe processing.
#else
    void RecordProcessedSlotIfDebug(Address slot_address) const {}
#endif  // DEBUG

    Isolate* const isolate_;
    const HeapObjectMap* moves_;
    HeapObjectMap moves_reverse_lookup_;
  };
};

}  // namespace

// static
void ReadOnlyPromotion::Promote(Isolate* isolate,
                                const SafepointScope& safepoint_scope,
                                const DisallowGarbageCollection& no_gc) {
  // Visit the mutable heap and determine the set of objects that can be
  // promoted to RO space.
  std::vector<Tagged<HeapObject>> promotees =
      Committee::DeterminePromotees(isolate, no_gc, safepoint_scope);
  // Physically copy promotee objects to RO space and track all object moves.
  HeapObjectMap moves;
  ReadOnlyPromotionImpl::CopyToReadOnlyHeap(isolate, promotees, &moves);
  // Update all references to moved objects to point at their new location in
  // RO space.
  ReadOnlyPromotionImpl::UpdatePointers(isolate, safepoint_scope, moves);
  ReadOnlyPromotionImpl::Verify(isolate, safepoint_scope);
}

}  // namespace internal
}  // namespace v8
