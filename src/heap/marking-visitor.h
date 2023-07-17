// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MARKING_VISITOR_H_
#define V8_HEAP_MARKING_VISITOR_H_

#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/heap/ephemeron-remembered-set.h"
#include "src/heap/marking-state.h"
#include "src/heap/marking-worklist.h"
#include "src/heap/objects-visiting.h"
#include "src/heap/pretenuring-handler.h"
#include "src/heap/spaces.h"
#include "src/heap/weak-object-worklists.h"
#include "src/objects/string.h"

namespace v8 {
namespace internal {

struct EphemeronMarking {
  std::vector<HeapObject> newly_discovered;
  bool newly_discovered_overflowed;
  size_t newly_discovered_limit;
};

// The base class for all marking visitors (main and concurrent marking) but
// also for e.g. the reference summarizer. It implements marking logic with
// support for bytecode flushing, embedder tracing and weak references.
//
// Derived classes are expected to provide the following methods:
// - CanUpdateValuesInHeap
// - AddStrongReferenceForReferenceSummarizer
// - AddWeakReferenceForReferenceSummarizer
// - TryMark
// - IsMarked
// - retaining_path_mode
// - RecordSlot
// - RecordRelocSlot
//
// These methods capture the difference between the different visitor
// implementations. For example, the concurrent visitor has to use the locking
// for string types that can be transitioned to other types on the main thread
// concurrently. On the other hand, the reference summarizer is not supposed to
// write into heap objects.
template <typename ConcreteVisitor>
class MarkingVisitorBase : public ConcurrentHeapVisitor<int, ConcreteVisitor> {
 public:
  MarkingVisitorBase(MarkingWorklists::Local* local_marking_worklists,
                     WeakObjects::Local* local_weak_objects, Heap* heap,
                     unsigned mark_compact_epoch,
                     base::EnumSet<CodeFlushMode> code_flush_mode,
                     bool trace_embedder_fields,
                     bool should_keep_ages_unchanged,
                     uint16_t code_flushing_increase)
      : ConcurrentHeapVisitor<int, ConcreteVisitor>(heap->isolate()),
        local_marking_worklists_(local_marking_worklists),
        local_weak_objects_(local_weak_objects),
        heap_(heap),
        mark_compact_epoch_(mark_compact_epoch),
        code_flush_mode_(code_flush_mode),
        trace_embedder_fields_(trace_embedder_fields),
        should_keep_ages_unchanged_(should_keep_ages_unchanged),
        should_mark_shared_heap_(heap->isolate()->is_shared_space_isolate()),
        code_flushing_increase_(code_flushing_increase),
        isolate_in_background_(heap->isolate()->IsIsolateInBackground())
#ifdef V8_ENABLE_SANDBOX
        ,
        external_pointer_table_(&heap->isolate()->external_pointer_table()),
        shared_external_pointer_table_(
            &heap->isolate()->shared_external_pointer_table()),
        shared_external_pointer_space_(
            heap->isolate()->shared_external_pointer_space())
#endif  // V8_ENABLE_SANDBOX
  {
  }

  V8_INLINE int VisitBytecodeArray(Map map, BytecodeArray object);
  V8_INLINE int VisitDescriptorArrayStrongly(Map map, DescriptorArray object);
  V8_INLINE int VisitDescriptorArray(Map map, DescriptorArray object);
  V8_INLINE int VisitEphemeronHashTable(Map map, EphemeronHashTable object);
  V8_INLINE int VisitFixedArray(Map map, FixedArray object);
  V8_INLINE int VisitJSApiObject(Map map, JSObject object);
  V8_INLINE int VisitJSArrayBuffer(Map map, JSArrayBuffer object);
  V8_INLINE int VisitJSDataViewOrRabGsabDataView(
      Map map, JSDataViewOrRabGsabDataView object);
  V8_INLINE int VisitJSFunction(Map map, JSFunction object);
  V8_INLINE int VisitJSTypedArray(Map map, JSTypedArray object);
  V8_INLINE int VisitJSWeakRef(Map map, JSWeakRef object);
  V8_INLINE int VisitMap(Map map, Map object);
  V8_INLINE int VisitSharedFunctionInfo(Map map, SharedFunctionInfo object);
  V8_INLINE int VisitTransitionArray(Map map, TransitionArray object);
  V8_INLINE int VisitWeakCell(Map map, WeakCell object);

  // ObjectVisitor overrides.
  void VisitMapPointer(HeapObject host) final {
    Map map = host.map(ObjectVisitorWithCageBases::cage_base());
    ProcessStrongHeapObject(host, host.map_slot(), map);
  }
  V8_INLINE void VisitPointer(HeapObject host, ObjectSlot p) final {
    VisitPointersImpl(host, p, p + 1);
  }
  V8_INLINE void VisitPointer(HeapObject host, MaybeObjectSlot p) final {
    VisitPointersImpl(host, p, p + 1);
  }
  V8_INLINE void VisitPointers(HeapObject host, ObjectSlot start,
                               ObjectSlot end) final {
    VisitPointersImpl(host, start, end);
  }
  V8_INLINE void VisitPointers(HeapObject host, MaybeObjectSlot start,
                               MaybeObjectSlot end) final {
    VisitPointersImpl(host, start, end);
  }
  V8_INLINE void VisitInstructionStreamPointer(
      Code host, InstructionStreamSlot slot) final {
    VisitInstructionStreamPointerImpl(host, slot);
  }
  V8_INLINE void VisitEmbeddedPointer(InstructionStream host,
                                      RelocInfo* rinfo) final;
  V8_INLINE void VisitCodeTarget(InstructionStream host,
                                 RelocInfo* rinfo) final;
  void VisitCustomWeakPointers(HeapObject host, ObjectSlot start,
                               ObjectSlot end) final {
    // Weak list pointers should be ignored during marking. The lists are
    // reconstructed after GC.
  }

  V8_INLINE void VisitExternalPointer(HeapObject host, ExternalPointerSlot slot,
                                      ExternalPointerTag tag) final;
#ifdef V8_CODE_POINTER_SANDBOXING
  V8_INLINE void VisitCodePointerHandle(HeapObject host,
                                        CodePointerHandle handle) final;
#endif

  void SynchronizePageAccess(HeapObject heap_object) {
#ifdef THREAD_SANITIZER
    // This is needed because TSAN does not process the memory fence
    // emitted after page initialization.
    BasicMemoryChunk::FromHeapObject(heap_object)->SynchronizedHeapLoad();
#endif
  }

  bool ShouldMarkObject(HeapObject object) const {
    if (object.InReadOnlySpace()) return false;
    if (should_mark_shared_heap_) return true;
    return !object.InAnySharedSpace();
  }

  // Marks the object grey and pushes it on the marking work list.
  V8_INLINE void MarkObject(HeapObject host, HeapObject obj);

  V8_INLINE static constexpr bool ShouldVisitReadOnlyMapPointer() {
    return false;
  }

  // Convenience method.
  bool IsUnmarked(HeapObject obj) const {
    return !concrete_visitor()->IsMarked(obj);
  }

 protected:
  using ConcurrentHeapVisitor<int, ConcreteVisitor>::concrete_visitor;

  template <typename THeapObjectSlot>
  void ProcessStrongHeapObject(HeapObject host, THeapObjectSlot slot,
                               HeapObject heap_object);
  template <typename THeapObjectSlot>
  void ProcessWeakHeapObject(HeapObject host, THeapObjectSlot slot,
                             HeapObject heap_object);

  template <typename TSlot>
  V8_INLINE void VisitPointerImpl(HeapObject host, TSlot p);

  template <typename TSlot>
  V8_INLINE void VisitPointersImpl(HeapObject host, TSlot start, TSlot end);

  // Similar to VisitPointersImpl() but using code cage base for loading from
  // the slot.
  V8_INLINE void VisitInstructionStreamPointerImpl(Code host,
                                                   InstructionStreamSlot slot);

  V8_INLINE void VisitDescriptorsForMap(Map map);

  template <typename T>
  int VisitEmbedderTracingSubclass(Map map, T object);
  template <typename T>
  int VisitEmbedderTracingSubClassWithEmbedderTracing(Map map, T object);
  template <typename T>
  int VisitEmbedderTracingSubClassNoEmbedderTracing(Map map, T object);

  V8_INLINE int VisitFixedArrayWithProgressBar(Map map, FixedArray object,
                                               ProgressBar& progress_bar);
  V8_INLINE int VisitFixedArrayRegularly(Map map, FixedArray object);

  // Methods needed for supporting code flushing.
  bool ShouldFlushCode(SharedFunctionInfo sfi) const;
  bool ShouldFlushBaselineCode(JSFunction js_function) const;

  bool HasBytecodeArrayForFlushing(SharedFunctionInfo sfi) const;
  bool IsOld(SharedFunctionInfo sfi) const;
  void MakeOlder(SharedFunctionInfo sfi) const;

  MarkingWorklists::Local* const local_marking_worklists_;
  WeakObjects::Local* const local_weak_objects_;
  Heap* const heap_;
  const unsigned mark_compact_epoch_;
  const base::EnumSet<CodeFlushMode> code_flush_mode_;
  const bool trace_embedder_fields_;
  const bool should_keep_ages_unchanged_;
  const bool should_mark_shared_heap_;
  const uint16_t code_flushing_increase_;
  const bool isolate_in_background_;
#ifdef V8_ENABLE_SANDBOX
  ExternalPointerTable* const external_pointer_table_;
  ExternalPointerTable* const shared_external_pointer_table_;
  ExternalPointerTable::Space* const shared_external_pointer_space_;
#endif  // V8_ENABLE_SANDBOX
};

// This is the common base class for main and concurrent full marking visitors.
// Derived class are expected to provide the same methods as for
// MarkingVisitorBase except for those defined in this class.
template <typename ConcreteVisitor>
class FullMarkingVisitorBase : public MarkingVisitorBase<ConcreteVisitor> {
 public:
  FullMarkingVisitorBase(MarkingWorklists::Local* local_marking_worklists,
                         WeakObjects::Local* local_weak_objects, Heap* heap,
                         unsigned mark_compact_epoch,
                         base::EnumSet<CodeFlushMode> code_flush_mode,
                         bool trace_embedder_fields,
                         bool should_keep_ages_unchanged,
                         uint16_t code_flushing_increase)
      : MarkingVisitorBase<ConcreteVisitor>(
            local_marking_worklists, local_weak_objects, heap,
            mark_compact_epoch, code_flush_mode, trace_embedder_fields,
            should_keep_ages_unchanged, code_flushing_increase) {}

  V8_INLINE void AddStrongReferenceForReferenceSummarizer(HeapObject host,
                                                          HeapObject obj) {}

  V8_INLINE void AddWeakReferenceForReferenceSummarizer(HeapObject host,
                                                        HeapObject obj) {}

  constexpr bool CanUpdateValuesInHeap() { return true; }

  bool TryMark(HeapObject obj) {
    return MarkBit::From(obj).Set<AccessMode::ATOMIC>();
  }
  bool IsMarked(HeapObject obj) const {
    return MarkBit::From(obj).Get<AccessMode::ATOMIC>();
  }
};

template <typename ConcreteVisitor, typename MarkingState>
class YoungGenerationMarkingVisitorBase
    : public NewSpaceVisitor<ConcreteVisitor> {
 public:
  YoungGenerationMarkingVisitorBase(
      Isolate* isolate, MarkingWorklists::Local* worklists_local,
      EphemeronRememberedSet::TableList::Local* ephemeron_tables_local,
      PretenuringHandler::PretenuringFeedbackMap* local_pretenuring_feedback);

  ~YoungGenerationMarkingVisitorBase() override;

  YoungGenerationMarkingVisitorBase(const YoungGenerationMarkingVisitorBase&) =
      delete;
  YoungGenerationMarkingVisitorBase& operator=(
      const YoungGenerationMarkingVisitorBase&) = delete;

  V8_INLINE void VisitPointers(HeapObject host, ObjectSlot start,
                               ObjectSlot end) final {
    concrete_visitor()->VisitPointersImpl(host, start, end);
  }
  V8_INLINE void VisitPointers(HeapObject host, MaybeObjectSlot start,
                               MaybeObjectSlot end) final {
    concrete_visitor()->VisitPointersImpl(host, start, end);
  }
  V8_INLINE void VisitPointer(HeapObject host, ObjectSlot p) final {
    concrete_visitor()->VisitPointersImpl(host, p, p + 1);
  }
  V8_INLINE void VisitPointer(HeapObject host, MaybeObjectSlot p) final {
    concrete_visitor()->VisitPointersImpl(host, p, p + 1);
  }

  V8_INLINE int VisitJSApiObject(Map map, JSObject object);
  V8_INLINE int VisitJSArrayBuffer(Map map, JSArrayBuffer object);
  V8_INLINE int VisitJSDataViewOrRabGsabDataView(
      Map map, JSDataViewOrRabGsabDataView object);
  V8_INLINE int VisitEphemeronHashTable(Map map, EphemeronHashTable table);
  V8_INLINE int VisitJSObject(Map map, JSObject object);
  V8_INLINE int VisitJSObjectFast(Map map, JSObject object);
  template <typename T, typename TBodyDescriptor = typename T::BodyDescriptor>
  V8_INLINE int VisitJSObjectSubclass(Map map, T object);
  V8_INLINE int VisitJSTypedArray(Map map, JSTypedArray object);

  MarkingWorklists::Local* worklists_local() const { return worklists_local_; }

  bool TryMark(HeapObject obj) {
    return MarkBit::From(obj).Set<AccessMode::ATOMIC>();
  }

 protected:
  using NewSpaceVisitor<ConcreteVisitor>::concrete_visitor;

  PretenuringHandler* pretenuring_handler() { return pretenuring_handler_; }

  template <typename T>
  int VisitEmbedderTracingSubClassWithEmbedderTracing(Map map, T object);

 private:
  MarkingWorklists::Local* worklists_local_;
  EphemeronRememberedSet::TableList::Local* ephemeron_tables_local_;
  PretenuringHandler* const pretenuring_handler_;
  PretenuringHandler::PretenuringFeedbackMap* const local_pretenuring_feedback_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MARKING_VISITOR_H_
