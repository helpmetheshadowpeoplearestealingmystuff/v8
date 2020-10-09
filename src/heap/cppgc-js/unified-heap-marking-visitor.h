// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_JS_UNIFIED_HEAP_MARKING_VISITOR_H_
#define V8_HEAP_CPPGC_JS_UNIFIED_HEAP_MARKING_VISITOR_H_

#include "include/cppgc/trace-trait.h"
#include "include/v8-cppgc.h"
#include "src/base/macros.h"
#include "src/heap/cppgc-js/unified-heap-marking-state.h"
#include "src/heap/cppgc/marking-visitor.h"

namespace cppgc {
namespace internal {
class ConcurrentMarkingState;
class MarkingStateBase;
class MutatorMarkingState;
}  // namespace internal
}  // namespace cppgc

namespace v8 {
namespace internal {

using cppgc::TraceDescriptor;
using cppgc::WeakCallback;
using cppgc::internal::ConcurrentMarkingState;
using cppgc::internal::HeapBase;
using cppgc::internal::MarkingStateBase;
using cppgc::internal::MutatorMarkingState;

class V8_EXPORT_PRIVATE UnifiedHeapMarkingVisitorBase : public JSVisitor {
 public:
  UnifiedHeapMarkingVisitorBase(HeapBase&, MarkingStateBase&,
                                UnifiedHeapMarkingState&);
  ~UnifiedHeapMarkingVisitorBase() override = default;

 protected:
  // C++ handling.
  void Visit(const void*, TraceDescriptor) final;
  void VisitWeak(const void*, TraceDescriptor, WeakCallback, const void*) final;
  void RegisterWeakCallback(WeakCallback, const void*) final;

  // JS handling.
  void Visit(const internal::JSMemberBase& ref) final;

  MarkingStateBase& marking_state_;
  UnifiedHeapMarkingState& unified_heap_marking_state_;
};

class V8_EXPORT_PRIVATE MutatorUnifiedHeapMarkingVisitor final
    : public UnifiedHeapMarkingVisitorBase {
 public:
  MutatorUnifiedHeapMarkingVisitor(HeapBase&, MutatorMarkingState&,
                                   UnifiedHeapMarkingState&);
  ~MutatorUnifiedHeapMarkingVisitor() override = default;

 protected:
  void VisitRoot(const void*, TraceDescriptor) final;
  void VisitWeakRoot(const void*, TraceDescriptor, WeakCallback,
                     const void*) final;
};

class V8_EXPORT_PRIVATE ConcurrentUnifiedHeapMarkingVisitor final
    : public UnifiedHeapMarkingVisitorBase {
 public:
  ConcurrentUnifiedHeapMarkingVisitor(HeapBase&, ConcurrentMarkingState&,
                                      UnifiedHeapMarkingState&);
  ~ConcurrentUnifiedHeapMarkingVisitor() override = default;

 protected:
  void VisitRoot(const void*, TraceDescriptor) final { UNREACHABLE(); }
  void VisitWeakRoot(const void*, TraceDescriptor, WeakCallback,
                     const void*) final {
    UNREACHABLE();
  }

  bool DeferTraceToMutatorThreadIfConcurrent(const void*, cppgc::TraceCallback,
                                             size_t) final;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_CPPGC_JS_UNIFIED_HEAP_MARKING_VISITOR_H_
