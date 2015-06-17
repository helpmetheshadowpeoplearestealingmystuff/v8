// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/graph-trimmer.h"

#include "src/compiler/graph.h"

namespace v8 {
namespace internal {
namespace compiler {

GraphTrimmer::GraphTrimmer(Zone* zone, Graph* graph)
    : graph_(graph), is_live_(graph, 2), live_(zone) {
  live_.reserve(graph->NodeCount());
}


GraphTrimmer::~GraphTrimmer() {}


void GraphTrimmer::TrimGraph() {
  // Mark end node as live.
  MarkAsLive(graph()->end());
  // Compute transitive closure of live nodes.
  for (size_t i = 0; i < live_.size(); ++i) {
    for (Node* const input : live_[i]->inputs()) {
      DCHECK_EQ(IsLive(input),
                std::find(live_.begin(), live_.end(), input) != live_.end());
      MarkAsLive(input);
    }
  }
  // Remove dead->live edges.
  for (Node* const live : live_) {
    DCHECK(IsLive(live));
    for (Edge edge : live->use_edges()) {
      Node* const user = edge.from();
      DCHECK_EQ(IsLive(user),
                std::find(live_.begin(), live_.end(), user) != live_.end());
      if (!IsLive(user)) {
        if (FLAG_trace_turbo_reduction) {
          OFStream os(stdout);
          os << "DeadLink: " << *user << "(" << edge.index() << ") -> " << *live
             << std::endl;
        }
        edge.UpdateTo(nullptr);
      }
    }
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
