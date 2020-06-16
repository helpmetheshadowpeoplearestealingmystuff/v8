// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_VISITOR_H_
#define V8_HEAP_CPPGC_VISITOR_H_

#include "include/cppgc/persistent.h"
#include "include/cppgc/visitor.h"
#include "src/heap/cppgc/heap-object-header.h"

namespace cppgc {
namespace internal {

class HeapBase;
class HeapObjectHeader;
class PageBackend;

// Base visitor that is allowed to create a public cppgc::Visitor object and
// use its internals.
class VisitorBase : public cppgc::Visitor {
 public:
  VisitorBase() = default;

  template <typename T>
  void TraceRootForTesting(const Persistent<T>& p, const SourceLocation& loc) {
    TraceRoot(p, loc);
  }

  template <typename T>
  void TraceRootForTesting(const WeakPersistent<T>& p,
                           const SourceLocation& loc) {
    TraceRoot(p, loc);
  }
};

// Regular visitor that additionally allows for conservative tracing.
class ConservativeTracingVisitor : public VisitorBase {
 public:
  ConservativeTracingVisitor(HeapBase&, PageBackend&);

  void TraceConservativelyIfNeeded(const void*);

 protected:
  using TraceConservativelyCallback = void(ConservativeTracingVisitor*,
                                           const HeapObjectHeader&);
  virtual void VisitConservatively(HeapObjectHeader&,
                                   TraceConservativelyCallback) {}

  HeapBase& heap_;
  PageBackend& page_backend_;
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_VISITOR_H_
