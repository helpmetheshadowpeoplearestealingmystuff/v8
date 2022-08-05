// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/graph-visualizer.h"

#include "src/compiler/node-origin-table.h"
#include "src/compiler/turboshaft/graph-visualizer.h"

namespace v8::internal::compiler::turboshaft {

JSONTurboshaftGraphWriter::JSONTurboshaftGraphWriter(
    std::ostream& os, const Graph& turboshaft_graph, NodeOriginTable* origins,
    Zone* zone)
    : os_(os),
      zone_(zone),
      turboshaft_graph_(turboshaft_graph),
      origins_(origins) {}

void JSONTurboshaftGraphWriter::Print() {
  os_ << "{\n\"nodes\":[";
  PrintNodes();
  os_ << "\n],\n\"edges\":[";
  PrintEdges();
  os_ << "\n],\n\"blocks\":[";
  PrintBlocks();
  os_ << "\n]}";
}

void JSONTurboshaftGraphWriter::PrintNodes() {
  bool first = true;
  for (const Block& block : turboshaft_graph_.blocks()) {
    for (const Operation& op : turboshaft_graph_.operations(block)) {
      OpIndex index = turboshaft_graph_.Index(op);
      if (!first) os_ << ",\n";
      first = false;
      os_ << "{\"id\":" << turboshaft_graph_.Index(op).id() << ",";
      os_ << "\"title\":\"" << OpcodeName(op.opcode) << "\",";
      os_ << "\"block_id\":" << block.index().id() << ",";
      os_ << "\"op_properties_type\":\"" << op.properties() << "\",";
      os_ << "\"properties\":\"";
      op.PrintOptions(os_);
      os_ << "\"";
      if (origins_) {
        NodeOrigin origin = origins_->GetNodeOrigin(index.id());
        if (origin.IsKnown()) {
          os_ << ", \"origin\":" << AsJSON(origin);
        }
      }
      SourcePosition position = turboshaft_graph_.source_positions()[index];
      if (position.IsKnown()) {
        os_ << ", \"sourcePosition\":" << compiler::AsJSON(position);
      }
      os_ << "}";
    }
  }
}

void JSONTurboshaftGraphWriter::PrintEdges() {
  bool first = true;
  for (const Block& block : turboshaft_graph_.blocks()) {
    for (const Operation& op : turboshaft_graph_.operations(block)) {
      int target_id = turboshaft_graph_.Index(op).id();
      for (OpIndex input : op.inputs()) {
        if (!first) os_ << ",\n";
        first = false;
        os_ << "{\"source\":" << input.id() << ",";
        os_ << "\"target\":" << target_id << "}";
      }
    }
  }
}

void JSONTurboshaftGraphWriter::PrintBlocks() {
  bool first_block = true;
  for (const Block& block : turboshaft_graph_.blocks()) {
    if (!first_block) os_ << ",\n";
    first_block = false;
    os_ << "{\"id\":" << block.index().id() << ",";
    os_ << "\"type\":\"" << block.kind() << "\",";
    os_ << "\"deferred\":" << std::boolalpha << block.IsDeferred() << ",";
    os_ << "\"predecessors\":[";
    bool first_predecessor = true;
    for (const Block* pred : block.Predecessors()) {
      if (!first_predecessor) os_ << ", ";
      first_predecessor = false;
      os_ << pred->index().id();
    }
    os_ << "]}";
  }
}

std::ostream& operator<<(std::ostream& os, const TurboshaftGraphAsJSON& ad) {
  JSONTurboshaftGraphWriter writer(os, ad.turboshaft_graph, ad.origins,
                                   ad.temp_zone);
  writer.Print();
  return os;
}

}  // namespace v8::internal::compiler::turboshaft
