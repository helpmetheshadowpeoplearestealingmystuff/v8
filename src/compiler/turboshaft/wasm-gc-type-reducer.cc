// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/wasm-gc-type-reducer.h"

#include "src/compiler/turboshaft/analyzer-iterator.h"
#include "src/compiler/turboshaft/loop-finder.h"

namespace v8::internal::compiler::turboshaft {

void WasmGCTypeAnalyzer::Run() {
  LoopFinder loop_finder(phase_zone_, &graph_);
  AnalyzerIterator iterator(phase_zone_, graph_, loop_finder);
  while (iterator.HasNext()) {
    const Block& block = *iterator.Next();
    StartNewSnapshotFor(block);
    ProcessOperations(block);

    // Finish snapshot.
    Snapshot snapshot = types_table_.Seal();
    block_to_snapshot_[block.index()] = MaybeSnapshot(snapshot);

    // Consider re-processing for loops.
    if (const GotoOp* last = block.LastOperation(graph_).TryCast<GotoOp>()) {
      if (last->destination->IsLoop() &&
          last->destination->LastPredecessor() == &block) {
        const Block& loop_header = *last->destination;
        // Create a merged snapshot state for the forward- and backedge.
        StartNewSnapshotFor(loop_header);
        // Revisit the loop header and compare the new snapshot with the
        // existing one.
        ProcessOperations(loop_header);
        Snapshot old_snapshot = block_to_snapshot_[loop_header.index()].value();
        Snapshot snapshot = types_table_.Seal();
        // TODO(14108): The snapshot isn't needed at all, we only care about the
        // information if two snapshots are equivalent. Unfortunately, currently
        // this can only be answered by creating a merge snapshot.
        bool needs_revisit =
            CreateMergeSnapshot(base::VectorOf({old_snapshot, snapshot}));
        types_table_.Seal();  // Discard the snapshot.

        // TODO(14108): This currently encodes a fixed point analysis where the
        // analysis is finished once the backedge doesn't provide updated type
        // information any more compared to the previous iteration. This could
        // be stopped in cases where the backedge only refines types (i.e. only
        // defines more precise types than the previous iteration).
        if (needs_revisit) {
          block_to_snapshot_[loop_header.index()] = MaybeSnapshot(snapshot);
          // This will push the successors of the loop header to the iterator
          // stack, so the loop body will be visited in the next iteration.
          iterator.MarkLoopForRevisitSkipHeader();
        }
      }
    }
  }
}

void WasmGCTypeAnalyzer::StartNewSnapshotFor(const Block& block) {
  is_first_loop_header_evaluation_ = false;
  // Start new snapshot based on predecessor information.
  if (block.HasPredecessors() == 0) {
    // The first block just starts with an empty snapshot.
    DCHECK_EQ(block.index().id(), 0);
    types_table_.StartNewSnapshot();
  } else if (block.IsLoop()) {
    MaybeSnapshot back_edge_snap =
        block_to_snapshot_[block.LastPredecessor()->index()];
    if (back_edge_snap.has_value()) {
      // The loop was already visited at least once. In this case use the
      // available information from the backedge.
      CreateMergeSnapshot(block);
    } else {
      // The loop wasn't visited yet. There isn't any type information available
      // for the backedge.
      is_first_loop_header_evaluation_ = true;
      Snapshot forward_edge_snap =
          block_to_snapshot_
              [block.LastPredecessor()->NeighboringPredecessor()->index()]
                  .value();
      types_table_.StartNewSnapshot(forward_edge_snap);
    }
  } else if (block.IsBranchTarget()) {
    DCHECK_EQ(block.PredecessorCount(), 1);
    types_table_.StartNewSnapshot(
        block_to_snapshot_[block.LastPredecessor()->index()].value());
    const BranchOp* branch =
        block.Predecessors()[0]->LastOperation(graph_).TryCast<BranchOp>();
    if (branch != nullptr) {
      ProcessBranchOnTarget(*branch, block);
    }
  } else {
    DCHECK_EQ(block.kind(), Block::Kind::kMerge);
    CreateMergeSnapshot(block);
  }
}

void WasmGCTypeAnalyzer::ProcessOperations(const Block& block) {
  for (OpIndex op_idx : graph_.OperationIndices(block)) {
    Operation& op = graph_.Get(op_idx);
    switch (op.opcode) {
      case Opcode::kWasmTypeCast:
        ProcessTypeCast(op.Cast<WasmTypeCastOp>());
        break;
      case Opcode::kWasmTypeCheck:
        ProcessTypeCheck(op.Cast<WasmTypeCheckOp>());
        break;
      case Opcode::kAssertNotNull:
        ProcessAssertNotNull(op.Cast<AssertNotNullOp>());
        break;
      case Opcode::kNull:
        ProcessNull(op.Cast<NullOp>());
        break;
      case Opcode::kIsNull:
        ProcessIsNull(op.Cast<IsNullOp>());
        break;
      case Opcode::kParameter:
        ProcessParameter(op.Cast<ParameterOp>());
        break;
      case Opcode::kStructGet:
        ProcessStructGet(op.Cast<StructGetOp>());
        break;
      case Opcode::kStructSet:
        ProcessStructSet(op.Cast<StructSetOp>());
        break;
      case Opcode::kArrayLength:
        ProcessArrayLength(op.Cast<ArrayLengthOp>());
        break;
      case Opcode::kGlobalGet:
        ProcessGlobalGet(op.Cast<GlobalGetOp>());
        break;
      case Opcode::kWasmRefFunc:
        ProcessRefFunc(op.Cast<WasmRefFuncOp>());
        break;
      case Opcode::kWasmAllocateArray:
        ProcessAllocateArray(op.Cast<WasmAllocateArrayOp>());
        break;
      case Opcode::kWasmAllocateStruct:
        ProcessAllocateStruct(op.Cast<WasmAllocateStructOp>());
        break;
      case Opcode::kPhi:
        ProcessPhi(op.Cast<PhiOp>());
        break;
      case Opcode::kBranch:
        // Handling branch conditions implying special values is handled on the
        // beginning of the successor block.
      default:
        break;
    }
  }
}

void WasmGCTypeAnalyzer::ProcessTypeCast(const WasmTypeCastOp& type_cast) {
  OpIndex object = type_cast.object();
  wasm::ValueType target_type = type_cast.config.to;
  wasm::ValueType known_input_type = RefineTypeKnowledge(object, target_type);
  // The cast also returns the input itself, so we also need to update its
  // result type.
  RefineTypeKnowledge(graph_.Index(type_cast), target_type);
  input_type_map_[graph_.Index(type_cast)] = known_input_type;
}

void WasmGCTypeAnalyzer::ProcessTypeCheck(const WasmTypeCheckOp& type_check) {
  wasm::ValueType type = types_table_.Get(type_check.object());
  input_type_map_[graph_.Index(type_check)] = type;
}

void WasmGCTypeAnalyzer::ProcessAssertNotNull(
    const AssertNotNullOp& assert_not_null) {
  OpIndex object = assert_not_null.object();
  wasm::ValueType new_type = assert_not_null.type.AsNonNull();
  wasm::ValueType known_input_type = RefineTypeKnowledge(object, new_type);
  input_type_map_[graph_.Index(assert_not_null)] = known_input_type;
  // AssertNotNull also returns the input.
  RefineTypeKnowledge(graph_.Index(assert_not_null), new_type);
}

void WasmGCTypeAnalyzer::ProcessIsNull(const IsNullOp& is_null) {
  input_type_map_[graph_.Index(is_null)] = types_table_.Get(is_null.object());
}

void WasmGCTypeAnalyzer::ProcessParameter(const ParameterOp& parameter) {
  if (parameter.parameter_index != wasm::kWasmInstanceParameterIndex) {
    RefineTypeKnowledge(graph_.Index(parameter),
                        signature_->GetParam(parameter.parameter_index - 1));
  }
}

void WasmGCTypeAnalyzer::ProcessStructGet(const StructGetOp& struct_get) {
  // struct.get performs a null check.
  wasm::ValueType type = RefineTypeKnowledgeNotNull(struct_get.object());
  input_type_map_[graph_.Index(struct_get)] = type;
}

void WasmGCTypeAnalyzer::ProcessStructSet(const StructSetOp& struct_set) {
  // struct.set performs a null check.
  wasm::ValueType type = RefineTypeKnowledgeNotNull(struct_set.object());
  input_type_map_[graph_.Index(struct_set)] = type;
}

void WasmGCTypeAnalyzer::ProcessArrayLength(const ArrayLengthOp& array_length) {
  // array.len performs a null check.
  wasm::ValueType type = RefineTypeKnowledgeNotNull(array_length.array());
  input_type_map_[graph_.Index(array_length)] = type;
}

void WasmGCTypeAnalyzer::ProcessGlobalGet(const GlobalGetOp& global_get) {
  RefineTypeKnowledge(graph_.Index(global_get), global_get.global->type);
}

void WasmGCTypeAnalyzer::ProcessRefFunc(const WasmRefFuncOp& ref_func) {
  uint32_t sig_index = module_->functions[ref_func.function_index].sig_index;
  RefineTypeKnowledge(graph_.Index(ref_func), wasm::ValueType::Ref(sig_index));
}

void WasmGCTypeAnalyzer::ProcessAllocateArray(
    const WasmAllocateArrayOp& allocate_array) {
  uint32_t type_index =
      graph_.Get(allocate_array.rtt()).Cast<RttCanonOp>().type_index;
  RefineTypeKnowledge(graph_.Index(allocate_array),
                      wasm::ValueType::Ref(type_index));
}

void WasmGCTypeAnalyzer::ProcessAllocateStruct(
    const WasmAllocateStructOp& allocate_struct) {
  uint32_t type_index =
      graph_.Get(allocate_struct.rtt()).Cast<RttCanonOp>().type_index;
  RefineTypeKnowledge(graph_.Index(allocate_struct),
                      wasm::ValueType::Ref(type_index));
}

void WasmGCTypeAnalyzer::ProcessPhi(const PhiOp& phi) {
  // The result type of a phi is the union of all its input types.
  // If any of the inputs is the default value ValueType(), there isn't any type
  // knowledge inferrable.
  DCHECK_GT(phi.input_count, 0);
  if (is_first_loop_header_evaluation_) {
    // We don't know anything about the backedge yet, so we only use the
    // forward edge. We will revisit the loop header again once the block with
    // the back edge is evaluated.
    RefineTypeKnowledge(graph_.Index(phi), types_table_.Get((phi.input(0))));
    return;
  }
  wasm::ValueType union_type =
      types_table_.GetPredecessorValue(phi.input(0), 0);
  if (union_type == wasm::ValueType()) return;
  for (int i = 1; i < phi.input_count; ++i) {
    wasm::ValueType input_type =
        types_table_.GetPredecessorValue(phi.input(i), i);
    if (input_type == wasm::ValueType()) return;
    union_type = wasm::Union(union_type, input_type, module_, module_).type;
  }
  RefineTypeKnowledge(graph_.Index(phi), union_type);
}

void WasmGCTypeAnalyzer::ProcessBranchOnTarget(const BranchOp& branch,
                                               const Block& target) {
  const Operation& condition = graph_.Get(branch.condition());
  switch (condition.opcode) {
    case Opcode::kWasmTypeCheck: {
      if (branch.if_true == &target) {
        // It is known from now on that the type is at least the checked one.
        const WasmTypeCheckOp& check = condition.Cast<WasmTypeCheckOp>();
        wasm::ValueType known_input_type =
            RefineTypeKnowledge(check.object(), check.config.to);
        input_type_map_[branch.condition()] = known_input_type;
      }
    } break;
    case Opcode::kIsNull: {
      const IsNullOp& is_null = condition.Cast<IsNullOp>();
      if (branch.if_true == &target) {
        RefineTypeKnowledge(is_null.object(),
                            wasm::ToNullSentinel({is_null.type, module_}));
      } else {
        DCHECK_EQ(branch.if_false, &target);
        RefineTypeKnowledge(is_null.object(), is_null.type.AsNonNull());
      }
    } break;
    default:
      break;
  }
}

void WasmGCTypeAnalyzer::ProcessNull(const NullOp& null) {
  wasm::ValueType null_type = wasm::ToNullSentinel({null.type, module_});
  RefineTypeKnowledge(graph_.Index(null), null_type);
}

void WasmGCTypeAnalyzer::CreateMergeSnapshot(const Block& block) {
  base::SmallVector<Snapshot, 8> snapshots;
  NeighboringPredecessorIterable iterable = block.PredecessorsIterable();
  std::transform(iterable.begin(), iterable.end(),
                 std::back_insert_iterator(snapshots), [this](Block* pred) {
                   return block_to_snapshot_[pred->index()].value();
                 });
  // The predecessor snapshots need to be reversed to restore the "original"
  // order of predecessors. (This is used to map phi inputs to their
  // corresponding predecessor.)
  std::reverse(snapshots.begin(), snapshots.end());
  CreateMergeSnapshot(base::VectorOf(snapshots));
}

bool WasmGCTypeAnalyzer::CreateMergeSnapshot(
    base::Vector<const Snapshot> predecessors) {
  // The merging logic is also used to evaluate if two snapshots are
  // "identical", i.e. the known types for all operations are the same.
  bool types_are_equivalent = true;
  types_table_.StartNewSnapshot(
      predecessors, [this, &types_are_equivalent](
                        TypeSnapshotTable::Key,
                        base::Vector<const wasm::ValueType> predecessors) {
        DCHECK_GT(predecessors.size(), 1);
        wasm::ValueType first = predecessors[0];

        wasm::ValueType res = first;
        for (auto iter = predecessors.begin() + 1; iter != predecessors.end();
             ++iter) {
          types_are_equivalent &= first == *iter;
          if (res == wasm::ValueType() || *iter == wasm::ValueType()) {
            res = wasm::ValueType();
          } else {
            res = wasm::Union(res, *iter, module_, module_).type;
          }
        }
        return res;
      });
  return !types_are_equivalent;
}

wasm::ValueType WasmGCTypeAnalyzer::RefineTypeKnowledge(
    OpIndex object, wasm::ValueType new_type) {
  wasm::ValueType previous_value = types_table_.Get(object);
  wasm::ValueType intersection_type =
      previous_value == wasm::ValueType()
          ? new_type
          : wasm::Intersection(previous_value, new_type, module_, module_).type;
  types_table_.Set(object, intersection_type);
  return previous_value;
}

wasm::ValueType WasmGCTypeAnalyzer::RefineTypeKnowledgeNotNull(OpIndex object) {
  wasm::ValueType previous_value = types_table_.Get(object);
  types_table_.Set(object, previous_value.AsNonNull());
  return previous_value;
}

}  // namespace v8::internal::compiler::turboshaft
