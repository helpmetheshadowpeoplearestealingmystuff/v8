// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/pipeline.h"

#include <fstream>  // NOLINT(readability/streams)
#include <sstream>

#include "src/base/platform/elapsed-timer.h"
#include "src/compiler/ast-graph-builder.h"
#include "src/compiler/basic-block-instrumentor.h"
#include "src/compiler/change-lowering.h"
#include "src/compiler/code-generator.h"
#include "src/compiler/control-reducer.h"
#include "src/compiler/graph-replay.h"
#include "src/compiler/graph-visualizer.h"
#include "src/compiler/instruction.h"
#include "src/compiler/instruction-selector.h"
#include "src/compiler/js-context-specialization.h"
#include "src/compiler/js-generic-lowering.h"
#include "src/compiler/js-inlining.h"
#include "src/compiler/js-typed-lowering.h"
#include "src/compiler/machine-operator-reducer.h"
#include "src/compiler/phi-reducer.h"
#include "src/compiler/register-allocator.h"
#include "src/compiler/schedule.h"
#include "src/compiler/scheduler.h"
#include "src/compiler/simplified-lowering.h"
#include "src/compiler/simplified-operator-reducer.h"
#include "src/compiler/typer.h"
#include "src/compiler/value-numbering-reducer.h"
#include "src/compiler/verifier.h"
#include "src/compiler/zone-pool.h"
#include "src/hydrogen.h"
#include "src/ostreams.h"
#include "src/utils.h"

namespace v8 {
namespace internal {
namespace compiler {

class PhaseStats {
 public:
  enum PhaseKind { CREATE_GRAPH, OPTIMIZATION, CODEGEN };

  PhaseStats(CompilationInfo* info, ZonePool* zone_pool, PhaseKind kind,
             const char* name)
      : info_(info),
        stats_scope_(zone_pool),
        kind_(kind),
        name_(name),
        size_(0) {
    if (FLAG_turbo_stats) {
      timer_.Start();
      size_ = info_->zone()->allocation_size();
    }
  }

  ~PhaseStats() {
    if (FLAG_turbo_stats) {
      base::TimeDelta delta = timer_.Elapsed();
      size_t bytes = info_->zone()->allocation_size() +
                     stats_scope_.GetMaxAllocatedBytes() - size_;
      HStatistics* stats = info_->isolate()->GetTStatistics();
      stats->SaveTiming(name_, delta, static_cast<int>(bytes));

      switch (kind_) {
        case CREATE_GRAPH:
          stats->IncrementCreateGraph(delta);
          break;
        case OPTIMIZATION:
          stats->IncrementOptimizeGraph(delta);
          break;
        case CODEGEN:
          stats->IncrementGenerateCode(delta);
          break;
      }
    }
  }

 private:
  CompilationInfo* info_;
  ZonePool::StatsScope stats_scope_;
  PhaseKind kind_;
  const char* name_;
  size_t size_;
  base::ElapsedTimer timer_;
};


static inline bool VerifyGraphs() {
#ifdef DEBUG
  return true;
#else
  return FLAG_turbo_verify;
#endif
}


struct TurboCfgFile : public std::ofstream {
  explicit TurboCfgFile(Isolate* isolate)
      : std::ofstream(isolate->GetTurboCfgFileName(), std::ios_base::app) {}
};


void Pipeline::VerifyAndPrintGraph(
    Graph* graph, const char* phase, bool untyped) {
  if (FLAG_trace_turbo) {
    char buffer[256];
    Vector<char> filename(buffer, sizeof(buffer));
    SmartArrayPointer<char> functionname;
    if (!info_->shared_info().is_null()) {
      functionname = info_->shared_info()->DebugName()->ToCString();
      if (strlen(functionname.get()) > 0) {
        SNPrintF(filename, "turbo-%s-%s", functionname.get(), phase);
      } else {
        SNPrintF(filename, "turbo-%p-%s", static_cast<void*>(info_), phase);
      }
    } else {
      SNPrintF(filename, "turbo-none-%s", phase);
    }
    std::replace(filename.start(), filename.start() + filename.length(), ' ',
                 '_');

    char dot_buffer[256];
    Vector<char> dot_filename(dot_buffer, sizeof(dot_buffer));
    SNPrintF(dot_filename, "%s.dot", filename.start());
    FILE* dot_file = base::OS::FOpen(dot_filename.start(), "w+");
    OFStream dot_of(dot_file);
    dot_of << AsDOT(*graph);
    fclose(dot_file);

    char json_buffer[256];
    Vector<char> json_filename(json_buffer, sizeof(json_buffer));
    SNPrintF(json_filename, "%s.json", filename.start());
    FILE* json_file = base::OS::FOpen(json_filename.start(), "w+");
    OFStream json_of(json_file);
    json_of << AsJSON(*graph);
    fclose(json_file);

    OFStream os(stdout);
    os << "-- " << phase << " graph printed to file " << filename.start()
       << "\n";
  }
  if (VerifyGraphs()) {
    Verifier::Run(graph,
        FLAG_turbo_types && !untyped ? Verifier::TYPED : Verifier::UNTYPED);
  }
}


class AstGraphBuilderWithPositions : public AstGraphBuilder {
 public:
  explicit AstGraphBuilderWithPositions(Zone* local_zone, CompilationInfo* info,
                                        JSGraph* jsgraph,
                                        SourcePositionTable* source_positions)
      : AstGraphBuilder(local_zone, info, jsgraph),
        source_positions_(source_positions) {}

  bool CreateGraph() {
    SourcePositionTable::Scope pos(source_positions_,
                                   SourcePosition::Unknown());
    return AstGraphBuilder::CreateGraph();
  }

#define DEF_VISIT(type)                                               \
  virtual void Visit##type(type* node) OVERRIDE {                     \
    SourcePositionTable::Scope pos(source_positions_,                 \
                                   SourcePosition(node->position())); \
    AstGraphBuilder::Visit##type(node);                               \
  }
  AST_NODE_LIST(DEF_VISIT)
#undef DEF_VISIT

 private:
  SourcePositionTable* source_positions_;
};


static void TraceSchedule(Schedule* schedule) {
  if (!FLAG_trace_turbo) return;
  OFStream os(stdout);
  os << "-- Schedule --------------------------------------\n" << *schedule;
}


Handle<Code> Pipeline::GenerateCode() {
  // This list must be kept in sync with DONT_TURBOFAN_NODE in ast.cc.
  if (info()->function()->dont_optimize_reason() == kTryCatchStatement ||
      info()->function()->dont_optimize_reason() == kTryFinallyStatement ||
      // TODO(turbofan): Make ES6 for-of work and remove this bailout.
      info()->function()->dont_optimize_reason() == kForOfStatement ||
      // TODO(turbofan): Make super work and remove this bailout.
      info()->function()->dont_optimize_reason() == kSuperReference ||
      // TODO(turbofan): Make class literals work and remove this bailout.
      info()->function()->dont_optimize_reason() == kClassLiteral ||
      // TODO(turbofan): Make OSR work and remove this bailout.
      info()->is_osr()) {
    return Handle<Code>::null();
  }

  if (FLAG_turbo_stats) isolate()->GetTStatistics()->Initialize(info_);

  if (FLAG_trace_turbo) {
    OFStream os(stdout);
    os << "---------------------------------------------------\n"
       << "Begin compiling method "
       << info()->function()->debug_name()->ToCString().get()
       << " using Turbofan" << std::endl;
    TurboCfgFile(isolate()) << AsC1VCompilation(info());
  }

  ZonePool zone_pool(isolate());

  // Build the graph.
  Graph graph(zone());
  SourcePositionTable source_positions(&graph);
  source_positions.AddDecorator();
  // TODO(turbofan): there is no need to type anything during initial graph
  // construction.  This is currently only needed for the node cache, which the
  // typer could sweep over later.
  Typer typer(&graph, info()->context());
  MachineOperatorBuilder machine;
  CommonOperatorBuilder common(zone());
  JSOperatorBuilder javascript(zone());
  JSGraph jsgraph(&graph, &common, &javascript, &machine);
  Node* context_node;
  {
    PhaseStats graph_builder_stats(info(), &zone_pool, PhaseStats::CREATE_GRAPH,
                                   "graph builder");
    ZonePool::Scope zone_scope(&zone_pool);
    AstGraphBuilderWithPositions graph_builder(zone_scope.zone(), info(),
                                               &jsgraph, &source_positions);
    graph_builder.CreateGraph();
    context_node = graph_builder.GetFunctionContext();
  }
  {
    PhaseStats phi_reducer_stats(info(), &zone_pool, PhaseStats::CREATE_GRAPH,
                                 "phi reduction");
    PhiReducer phi_reducer;
    GraphReducer graph_reducer(&graph);
    graph_reducer.AddReducer(&phi_reducer);
    graph_reducer.ReduceGraph();
    // TODO(mstarzinger): Running reducer once ought to be enough for everyone.
    graph_reducer.ReduceGraph();
    graph_reducer.ReduceGraph();
  }

  VerifyAndPrintGraph(&graph, "Initial untyped", true);

  if (info()->is_context_specializing()) {
    SourcePositionTable::Scope pos(&source_positions,
                                   SourcePosition::Unknown());
    // Specialize the code to the context as aggressively as possible.
    JSContextSpecializer spec(info(), &jsgraph, context_node);
    spec.SpecializeToContext();
    VerifyAndPrintGraph(&graph, "Context specialized", true);
  }

  if (info()->is_inlining_enabled()) {
    SourcePositionTable::Scope pos(&source_positions,
                                   SourcePosition::Unknown());
    ZonePool::Scope zone_scope(&zone_pool);
    JSInliner inliner(zone_scope.zone(), info(), &jsgraph);
    inliner.Inline();
    VerifyAndPrintGraph(&graph, "Inlined", true);
  }

  // Print a replay of the initial graph.
  if (FLAG_print_turbo_replay) {
    GraphReplayPrinter::PrintReplay(&graph);
  }

  // Bailout here in case target architecture is not supported.
  if (!SupportedTarget()) return Handle<Code>::null();

  if (info()->is_typing_enabled()) {
    {
      // Type the graph.
      PhaseStats typer_stats(info(), &zone_pool, PhaseStats::CREATE_GRAPH,
                             "typer");
      typer.Run();
      VerifyAndPrintGraph(&graph, "Typed");
    }
    {
      // Lower JSOperators where we can determine types.
      PhaseStats lowering_stats(info(), &zone_pool, PhaseStats::CREATE_GRAPH,
                                "typed lowering");
      SourcePositionTable::Scope pos(&source_positions,
                                     SourcePosition::Unknown());
      ValueNumberingReducer vn_reducer(zone());
      JSTypedLowering lowering(&jsgraph);
      SimplifiedOperatorReducer simple_reducer(&jsgraph);
      GraphReducer graph_reducer(&graph);
      graph_reducer.AddReducer(&vn_reducer);
      graph_reducer.AddReducer(&lowering);
      graph_reducer.AddReducer(&simple_reducer);
      graph_reducer.ReduceGraph();

      VerifyAndPrintGraph(&graph, "Lowered typed");
    }
    {
      // Lower simplified operators and insert changes.
      PhaseStats lowering_stats(info(), &zone_pool, PhaseStats::CREATE_GRAPH,
                                "simplified lowering");
      SourcePositionTable::Scope pos(&source_positions,
                                     SourcePosition::Unknown());
      SimplifiedLowering lowering(&jsgraph);
      lowering.LowerAllNodes();
      ValueNumberingReducer vn_reducer(zone());
      SimplifiedOperatorReducer simple_reducer(&jsgraph);
      GraphReducer graph_reducer(&graph);
      graph_reducer.AddReducer(&vn_reducer);
      graph_reducer.AddReducer(&simple_reducer);
      graph_reducer.ReduceGraph();

      VerifyAndPrintGraph(&graph, "Lowered simplified");
    }
    {
      // Lower changes that have been inserted before.
      PhaseStats lowering_stats(info(), &zone_pool, PhaseStats::OPTIMIZATION,
                                "change lowering");
      SourcePositionTable::Scope pos(&source_positions,
                                     SourcePosition::Unknown());
      Linkage linkage(info());
      ValueNumberingReducer vn_reducer(zone());
      SimplifiedOperatorReducer simple_reducer(&jsgraph);
      ChangeLowering lowering(&jsgraph, &linkage);
      MachineOperatorReducer mach_reducer(&jsgraph);
      GraphReducer graph_reducer(&graph);
      // TODO(titzer): Figure out if we should run all reducers at once here.
      graph_reducer.AddReducer(&vn_reducer);
      graph_reducer.AddReducer(&simple_reducer);
      graph_reducer.AddReducer(&lowering);
      graph_reducer.AddReducer(&mach_reducer);
      graph_reducer.ReduceGraph();

      // TODO(jarin, rossberg): Remove UNTYPED once machine typing works.
      VerifyAndPrintGraph(&graph, "Lowered changes", true);
    }

    {
      SourcePositionTable::Scope pos(&source_positions,
                                     SourcePosition::Unknown());
      PhaseStats control_reducer_stats(
          info(), &zone_pool, PhaseStats::CREATE_GRAPH, "control reduction");
      ZonePool::Scope zone_scope(&zone_pool);
      ControlReducer::ReduceGraph(zone_scope.zone(), &jsgraph, &common);

      VerifyAndPrintGraph(&graph, "Control reduced");
    }
  }

  {
    // Lower any remaining generic JSOperators.
    PhaseStats lowering_stats(info(), &zone_pool, PhaseStats::CREATE_GRAPH,
                              "generic lowering");
    SourcePositionTable::Scope pos(&source_positions,
                                   SourcePosition::Unknown());
    JSGenericLowering lowering(info(), &jsgraph);
    GraphReducer graph_reducer(&graph);
    graph_reducer.AddReducer(&lowering);
    graph_reducer.ReduceGraph();

    // TODO(jarin, rossberg): Remove UNTYPED once machine typing works.
    VerifyAndPrintGraph(&graph, "Lowered generic", true);
  }

  source_positions.RemoveDecorator();

  Handle<Code> code = Handle<Code>::null();
  {
    // Compute a schedule.
    Schedule* schedule = ComputeSchedule(&zone_pool, &graph);
    // Generate optimized code.
    PhaseStats codegen_stats(info(), &zone_pool, PhaseStats::CODEGEN,
                             "codegen");
    Linkage linkage(info());
    code =
        GenerateCode(&zone_pool, &linkage, &graph, schedule, &source_positions);
    info()->SetCode(code);
  }

  // Print optimized code.
  v8::internal::CodeGenerator::PrintCode(code, info());

  if (FLAG_trace_turbo) {
    OFStream os(stdout);
    os << "--------------------------------------------------\n"
       << "Finished compiling method "
       << info()->function()->debug_name()->ToCString().get()
       << " using Turbofan" << std::endl;
  }

  return code;
}


Schedule* Pipeline::ComputeSchedule(ZonePool* zone_pool, Graph* graph) {
  PhaseStats schedule_stats(info(), zone_pool, PhaseStats::CODEGEN,
                            "scheduling");
  Schedule* schedule = Scheduler::ComputeSchedule(zone_pool, graph);
  TraceSchedule(schedule);
  if (VerifyGraphs()) ScheduleVerifier::Run(schedule);
  return schedule;
}


Handle<Code> Pipeline::GenerateCodeForMachineGraph(Linkage* linkage,
                                                   Graph* graph,
                                                   Schedule* schedule) {
  ZonePool zone_pool(isolate());
  CHECK(SupportedBackend());
  if (schedule == NULL) {
    // TODO(rossberg): Should this really be untyped?
    VerifyAndPrintGraph(graph, "Machine", true);
    schedule = ComputeSchedule(&zone_pool, graph);
  }
  TraceSchedule(schedule);

  SourcePositionTable source_positions(graph);
  Handle<Code> code =
      GenerateCode(&zone_pool, linkage, graph, schedule, &source_positions);
#if ENABLE_DISASSEMBLER
  if (!code.is_null() && FLAG_print_opt_code) {
    CodeTracer::Scope tracing_scope(isolate()->GetCodeTracer());
    OFStream os(tracing_scope.file());
    code->Disassemble("test code", os);
  }
#endif
  return code;
}


Handle<Code> Pipeline::GenerateCode(ZonePool* zone_pool, Linkage* linkage,
                                    Graph* graph, Schedule* schedule,
                                    SourcePositionTable* source_positions) {
  DCHECK_NOT_NULL(graph);
  DCHECK_NOT_NULL(linkage);
  DCHECK_NOT_NULL(schedule);
  CHECK(SupportedBackend());

  BasicBlockProfiler::Data* profiler_data = NULL;
  if (FLAG_turbo_profiling) {
    profiler_data = BasicBlockInstrumentor::Instrument(info_, graph, schedule);
  }

  Zone* instruction_zone = schedule->zone();
  InstructionSequence sequence(instruction_zone, graph, schedule);

  // Select and schedule instructions covering the scheduled graph.
  {
    ZonePool::Scope zone_scope(zone_pool);
    InstructionSelector selector(zone_scope.zone(), linkage, &sequence,
                                 schedule, source_positions);
    selector.SelectInstructions();
  }

  if (FLAG_trace_turbo) {
    OFStream os(stdout);
    os << "----- Instruction sequence before register allocation -----\n"
       << sequence;
    TurboCfgFile(isolate())
        << AsC1V("CodeGen", schedule, source_positions, &sequence);
  }

  // Allocate registers.
  Frame frame;
  {
    int node_count = graph->NodeCount();
    if (node_count > UnallocatedOperand::kMaxVirtualRegisters) {
      linkage->info()->AbortOptimization(kNotEnoughVirtualRegistersForValues);
      return Handle<Code>::null();
    }
    ZonePool::Scope zone_scope(zone_pool);
    RegisterAllocator allocator(zone_scope.zone(), &frame, linkage->info(),
                                &sequence);
    if (!allocator.Allocate(zone_pool)) {
      linkage->info()->AbortOptimization(kNotEnoughVirtualRegistersRegalloc);
      return Handle<Code>::null();
    }
    if (FLAG_trace_turbo) {
      TurboCfgFile(isolate()) << AsC1VAllocator("CodeGen", &allocator);
    }
  }

  if (FLAG_trace_turbo) {
    OFStream os(stdout);
    os << "----- Instruction sequence after register allocation -----\n"
       << sequence;
  }

  // Generate native sequence.
  CodeGenerator generator(&frame, linkage, &sequence);
  Handle<Code> code = generator.GenerateCode();
  if (profiler_data != NULL) {
#if ENABLE_DISASSEMBLER
    std::ostringstream os;
    code->Disassemble(NULL, os);
    profiler_data->SetCode(&os);
#endif
  }
  return code;
}


void Pipeline::SetUp() {
  InstructionOperand::SetUpCaches();
}


void Pipeline::TearDown() {
  InstructionOperand::TearDownCaches();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
