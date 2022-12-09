// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_PRETENURING_HANDLER_H_
#define V8_HEAP_PRETENURING_HANDLER_H_

#include <memory>

#include "src/objects/allocation-site.h"
#include "src/objects/heap-object.h"
#include "src/objects/map.h"

namespace v8 {
namespace internal {

template <typename T>
class GlobalHandleVector;
class Heap;

class PretenturingHandler final {
 public:
  static const int kInitialFeedbackCapacity = 256;
  using PretenuringFeedbackMap =
      std::unordered_map<AllocationSite, size_t, Object::Hasher>;
  enum FindMementoMode { kForRuntime, kForGC };

  explicit PretenturingHandler(Heap* heap);
  ~PretenturingHandler();

  void reset();

  // If an object has an AllocationMemento trailing it, return it, otherwise
  // return a null AllocationMemento.
  template <FindMementoMode mode>
  inline AllocationMemento FindAllocationMemento(Map map, HeapObject object);

  // ===========================================================================
  // Allocation site tracking. =================================================
  // ===========================================================================

  // Updates the AllocationSite of a given {object}. The entry (including the
  // count) is cached on the local pretenuring feedback.
  inline void UpdateAllocationSite(
      Map map, HeapObject object, PretenuringFeedbackMap* pretenuring_feedback);

  // Merges local pretenuring feedback into the global one. Note that this
  // method needs to be called after evacuation, as allocation sites may be
  // evacuated and this method resolves forward pointers accordingly.
  void MergeAllocationSitePretenuringFeedback(
      const PretenuringFeedbackMap& local_pretenuring_feedback);

  // Adds an allocation site to the list of sites to be pretenured during the
  // next collection. Added allocation sites are pretenured independent of
  // their feedback.
  V8_EXPORT_PRIVATE void PretenureAllocationSiteOnNextCollection(
      AllocationSite site);

  // ===========================================================================
  // Pretenuring. ==============================================================
  // ===========================================================================

  // Pretenuring decisions are made based on feedback collected during new space
  // evacuation. Note that between feedback collection and calling this method
  // object in old space must not move.
  void ProcessPretenuringFeedback();

  // Removes an entry from the global pretenuring storage.
  void RemoveAllocationSitePretenuringFeedback(AllocationSite site);

#if DEBUG
  bool HasPretenuringFeedback() const {
    return !global_pretenuring_feedback_.empty();
  }
#endif  // DEBUG

 private:
  bool DeoptMaybeTenuredAllocationSites() const;

  Heap* const heap_;

  // The feedback storage is used to store allocation sites (keys) and how often
  // they have been visited (values) by finding a memento behind an object. The
  // storage is only alive temporary during a GC. The invariant is that all
  // pointers in this map are already fixed, i.e., they do not point to
  // forwarding pointers.
  PretenuringFeedbackMap global_pretenuring_feedback_;

  std::unique_ptr<GlobalHandleVector<AllocationSite>>
      allocation_sites_to_pretenure_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_PRETENURING_HANDLER_H_
