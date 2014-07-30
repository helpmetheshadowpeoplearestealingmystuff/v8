// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/source-position.h"
#include "src/compiler/graph.h"
#include "src/compiler/node-aux-data-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

class SourcePositionTable::Decorator : public GraphDecorator {
 public:
  explicit Decorator(SourcePositionTable* source_positions)
      : source_positions_(source_positions) {}

  virtual void Decorate(Node* node) {
    ASSERT(!source_positions_->current_position_.IsInvalid());
    source_positions_->table_.Set(node, source_positions_->current_position_);
  }

 private:
  SourcePositionTable* source_positions_;
};


SourcePositionTable::SourcePositionTable(Graph* graph)
    : graph_(graph),
      decorator_(NULL),
      current_position_(SourcePosition::Invalid()),
      table_(graph) {}


void SourcePositionTable::AddDecorator() {
  ASSERT(decorator_ == NULL);
  decorator_ = new (graph_->zone()) Decorator(this);
  graph_->AddDecorator(decorator_);
}


void SourcePositionTable::RemoveDecorator() {
  ASSERT(decorator_ != NULL);
  graph_->RemoveDecorator(decorator_);
  decorator_ = NULL;
}


SourcePosition SourcePositionTable::GetSourcePosition(Node* node) {
  return table_.Get(node);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
