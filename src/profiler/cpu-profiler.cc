// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/profiler/cpu-profiler.h"

#include <unordered_map>
#include <utility>

#include "src/base/lazy-instance.h"
#include "src/base/template-utils.h"
#include "src/debug/debug.h"
#include "src/execution/frames-inl.h"
#include "src/execution/vm-state-inl.h"
#include "src/locked-queue-inl.h"
#include "src/logging/counters.h"
#include "src/logging/log.h"
#include "src/profiler/cpu-profiler-inl.h"
#include "src/wasm/wasm-engine.h"

namespace v8 {
namespace internal {

static const int kProfilerStackSize = 64 * KB;

class CpuSampler : public sampler::Sampler {
 public:
  CpuSampler(Isolate* isolate, SamplingEventsProcessor* processor)
      : sampler::Sampler(reinterpret_cast<v8::Isolate*>(isolate)),
        processor_(processor) {}

  void SampleStack(const v8::RegisterState& regs) override {
    TickSample* sample = processor_->StartTickSample();
    if (sample == nullptr) return;
    Isolate* isolate = reinterpret_cast<Isolate*>(this->isolate());
    sample->Init(isolate, regs, TickSample::kIncludeCEntryFrame,
                 /* update_stats */ true,
                 /* use_simulator_reg_state */ true, processor_->period());
    if (is_counting_samples_ && !sample->timestamp.IsNull()) {
      if (sample->state == JS) ++js_sample_count_;
      if (sample->state == EXTERNAL) ++external_sample_count_;
    }
    processor_->FinishTickSample();
  }

 private:
  SamplingEventsProcessor* processor_;
};

ProfilerEventsProcessor::ProfilerEventsProcessor(Isolate* isolate,
                                                 ProfileGenerator* generator)
    : Thread(Thread::Options("v8:ProfEvntProc", kProfilerStackSize)),
      generator_(generator),
      running_(1),
      last_code_event_id_(0),
      last_processed_code_event_id_(0),
      isolate_(isolate),
      profiling_scope_(isolate) {}

SamplingEventsProcessor::SamplingEventsProcessor(Isolate* isolate,
                                                 ProfileGenerator* generator,
                                                 base::TimeDelta period,
                                                 bool use_precise_sampling)
    : ProfilerEventsProcessor(isolate, generator),
      sampler_(new CpuSampler(isolate, this)),
      period_(period),
      use_precise_sampling_(use_precise_sampling) {
  sampler_->Start();
}

SamplingEventsProcessor::~SamplingEventsProcessor() { sampler_->Stop(); }

ProfilerEventsProcessor::~ProfilerEventsProcessor() = default;

void ProfilerEventsProcessor::Enqueue(const CodeEventsContainer& event) {
  event.generic.order = ++last_code_event_id_;
  events_buffer_.Enqueue(event);
}

void ProfilerEventsProcessor::AddDeoptStack(Address from, int fp_to_sp_delta) {
  TickSampleEventRecord record(last_code_event_id_);
  RegisterState regs;
  Address fp = isolate_->c_entry_fp(isolate_->thread_local_top());
  regs.sp = reinterpret_cast<void*>(fp - fp_to_sp_delta);
  regs.fp = reinterpret_cast<void*>(fp);
  regs.pc = reinterpret_cast<void*>(from);
  record.sample.Init(isolate_, regs, TickSample::kSkipCEntryFrame, false,
                     false);
  ticks_from_vm_buffer_.Enqueue(record);
}

void ProfilerEventsProcessor::AddCurrentStack(bool update_stats) {
  TickSampleEventRecord record(last_code_event_id_);
  RegisterState regs;
  StackFrameIterator it(isolate_);
  if (!it.done()) {
    StackFrame* frame = it.frame();
    regs.sp = reinterpret_cast<void*>(frame->sp());
    regs.fp = reinterpret_cast<void*>(frame->fp());
    regs.pc = reinterpret_cast<void*>(frame->pc());
  }
  record.sample.Init(isolate_, regs, TickSample::kSkipCEntryFrame, update_stats,
                     false);
  ticks_from_vm_buffer_.Enqueue(record);
}

void ProfilerEventsProcessor::AddSample(TickSample sample) {
  TickSampleEventRecord record(last_code_event_id_);
  record.sample = sample;
  ticks_from_vm_buffer_.Enqueue(record);
}

void ProfilerEventsProcessor::StopSynchronously() {
  if (!base::Relaxed_AtomicExchange(&running_, 0)) return;
  {
    base::MutexGuard guard(&running_mutex_);
    running_cond_.NotifyOne();
  }
  Join();
}


bool ProfilerEventsProcessor::ProcessCodeEvent() {
  CodeEventsContainer record;
  if (events_buffer_.Dequeue(&record)) {
    switch (record.generic.type) {
#define PROFILER_TYPE_CASE(type, clss)                          \
      case CodeEventRecord::type:                               \
        record.clss##_.UpdateCodeMap(generator_->code_map());   \
        break;

      CODE_EVENTS_TYPE_LIST(PROFILER_TYPE_CASE)

#undef PROFILER_TYPE_CASE
      default: return true;  // Skip record.
    }
    last_processed_code_event_id_ = record.generic.order;
    return true;
  }
  return false;
}

void ProfilerEventsProcessor::CodeEventHandler(
    const CodeEventsContainer& evt_rec) {
  switch (evt_rec.generic.type) {
    case CodeEventRecord::CODE_CREATION:
    case CodeEventRecord::CODE_MOVE:
    case CodeEventRecord::CODE_DISABLE_OPT:
      Enqueue(evt_rec);
      break;
    case CodeEventRecord::CODE_DEOPT: {
      const CodeDeoptEventRecord* rec = &evt_rec.CodeDeoptEventRecord_;
      Address pc = rec->pc;
      int fp_to_sp_delta = rec->fp_to_sp_delta;
      Enqueue(evt_rec);
      AddDeoptStack(pc, fp_to_sp_delta);
      break;
    }
    case CodeEventRecord::NONE:
    case CodeEventRecord::REPORT_BUILTIN:
      UNREACHABLE();
  }
}

ProfilerEventsProcessor::SampleProcessingResult
SamplingEventsProcessor::ProcessOneSample() {
  TickSampleEventRecord record1;
  if (ticks_from_vm_buffer_.Peek(&record1) &&
      (record1.order == last_processed_code_event_id_)) {
    TickSampleEventRecord record;
    ticks_from_vm_buffer_.Dequeue(&record);
    generator_->RecordTickSample(record.sample);
    return OneSampleProcessed;
  }

  const TickSampleEventRecord* record = ticks_buffer_.Peek();
  if (record == nullptr) {
    if (ticks_from_vm_buffer_.IsEmpty()) return NoSamplesInQueue;
    return FoundSampleForNextCodeEvent;
  }
  if (record->order != last_processed_code_event_id_) {
    return FoundSampleForNextCodeEvent;
  }
  generator_->RecordTickSample(record->sample);
  ticks_buffer_.Remove();
  return OneSampleProcessed;
}

void SamplingEventsProcessor::Run() {
  base::MutexGuard guard(&running_mutex_);
  while (!!base::Relaxed_Load(&running_)) {
    base::TimeTicks nextSampleTime =
        base::TimeTicks::HighResolutionNow() + period_;
    base::TimeTicks now;
    SampleProcessingResult result;
    // Keep processing existing events until we need to do next sample
    // or the ticks buffer is empty.
    do {
      result = ProcessOneSample();
      if (result == FoundSampleForNextCodeEvent) {
        // All ticks of the current last_processed_code_event_id_ are
        // processed, proceed to the next code event.
        ProcessCodeEvent();
      }
      now = base::TimeTicks::HighResolutionNow();
    } while (result != NoSamplesInQueue && now < nextSampleTime);

    if (nextSampleTime > now) {
#if V8_OS_WIN
      if (use_precise_sampling_ &&
          nextSampleTime - now < base::TimeDelta::FromMilliseconds(100)) {
        // Do not use Sleep on Windows as it is very imprecise, with up to 16ms
        // jitter, which is unacceptable for short profile intervals.
        while (base::TimeTicks::HighResolutionNow() < nextSampleTime) {
        }
      } else  // NOLINT
#else
      USE(use_precise_sampling_);
#endif  // V8_OS_WIN
      {
        // Allow another thread to interrupt the delay between samples in the
        // event of profiler shutdown.
        while (now < nextSampleTime &&
               running_cond_.WaitFor(&running_mutex_, nextSampleTime - now)) {
          // If true was returned, we got interrupted before the timeout
          // elapsed. If this was not due to a change in running state, a
          // spurious wakeup occurred (thus we should continue to wait).
          if (!base::Relaxed_Load(&running_)) {
            break;
          }
          now = base::TimeTicks::HighResolutionNow();
        }
      }
    }

    // Schedule next sample.
    sampler_->DoSample();
  }

  // Process remaining tick events.
  do {
    SampleProcessingResult result;
    do {
      result = ProcessOneSample();
    } while (result == OneSampleProcessed);
  } while (ProcessCodeEvent());
}

void SamplingEventsProcessor::SetSamplingInterval(base::TimeDelta period) {
  if (period_ == period) return;
  StopSynchronously();

  period_ = period;
  base::Relaxed_Store(&running_, 1);

  StartSynchronously();
}

void* SamplingEventsProcessor::operator new(size_t size) {
  return AlignedAlloc(size, alignof(SamplingEventsProcessor));
}

void SamplingEventsProcessor::operator delete(void* ptr) { AlignedFree(ptr); }

int CpuProfiler::GetProfilesCount() {
  // The count of profiles doesn't depend on a security token.
  return static_cast<int>(profiles_->profiles()->size());
}


CpuProfile* CpuProfiler::GetProfile(int index) {
  return profiles_->profiles()->at(index).get();
}


void CpuProfiler::DeleteAllProfiles() {
  if (is_profiling_) StopProcessor();
  ResetProfiles();
}


void CpuProfiler::DeleteProfile(CpuProfile* profile) {
  profiles_->RemoveProfile(profile);
  if (profiles_->profiles()->empty() && !is_profiling_) {
    // If this was the last profile, clean up all accessory data as well.
    ResetProfiles();
  }
}

namespace {

class CpuProfilersManager {
 public:
  void AddProfiler(Isolate* isolate, CpuProfiler* profiler) {
    base::MutexGuard lock(&mutex_);
    profilers_.emplace(isolate, profiler);
  }

  void RemoveProfiler(Isolate* isolate, CpuProfiler* profiler) {
    base::MutexGuard lock(&mutex_);
    auto range = profilers_.equal_range(isolate);
    for (auto it = range.first; it != range.second; ++it) {
      if (it->second != profiler) continue;
      profilers_.erase(it);
      return;
    }
    UNREACHABLE();
  }

  void CallCollectSample(Isolate* isolate) {
    base::MutexGuard lock(&mutex_);
    auto range = profilers_.equal_range(isolate);
    for (auto it = range.first; it != range.second; ++it) {
      it->second->CollectSample();
    }
  }

 private:
  std::unordered_multimap<Isolate*, CpuProfiler*> profilers_;
  base::Mutex mutex_;
};

DEFINE_LAZY_LEAKY_OBJECT_GETTER(CpuProfilersManager, GetProfilersManager)

}  // namespace

CpuProfiler::CpuProfiler(Isolate* isolate, CpuProfilingNamingMode naming_mode)
    : CpuProfiler(isolate, naming_mode, new CpuProfilesCollection(isolate),
                  nullptr, nullptr) {}

CpuProfiler::CpuProfiler(Isolate* isolate, CpuProfilingNamingMode naming_mode,
                         CpuProfilesCollection* test_profiles,
                         ProfileGenerator* test_generator,
                         ProfilerEventsProcessor* test_processor)
    : isolate_(isolate),
      naming_mode_(naming_mode),
      base_sampling_interval_(base::TimeDelta::FromMicroseconds(
          FLAG_cpu_profiler_sampling_interval)),
      profiles_(test_profiles),
      generator_(test_generator),
      processor_(test_processor),
      is_profiling_(false) {
  profiles_->set_cpu_profiler(this);
  GetProfilersManager()->AddProfiler(isolate, this);
}

CpuProfiler::~CpuProfiler() {
  DCHECK(!is_profiling_);
  GetProfilersManager()->RemoveProfiler(isolate_, this);
}

void CpuProfiler::set_sampling_interval(base::TimeDelta value) {
  DCHECK(!is_profiling_);
  base_sampling_interval_ = value;
}

void CpuProfiler::set_use_precise_sampling(bool value) {
  DCHECK(!is_profiling_);
  use_precise_sampling_ = value;
}

void CpuProfiler::ResetProfiles() {
  profiles_.reset(new CpuProfilesCollection(isolate_));
  profiles_->set_cpu_profiler(this);
  profiler_listener_.reset();
  generator_.reset();
}

void CpuProfiler::CreateEntriesForRuntimeCallStats() {
  RuntimeCallStats* rcs = isolate_->counters()->runtime_call_stats();
  CodeMap* code_map = generator_->code_map();
  for (int i = 0; i < RuntimeCallStats::kNumberOfCounters; ++i) {
    RuntimeCallCounter* counter = rcs->GetCounter(i);
    DCHECK(counter->name());
    auto entry = new CodeEntry(CodeEventListener::FUNCTION_TAG, counter->name(),
                               "native V8Runtime");
    code_map->AddCode(reinterpret_cast<Address>(counter), entry, 1);
  }
}

base::TimeDelta CpuProfiler::ComputeSamplingInterval() const {
  return profiles_->GetCommonSamplingInterval();
}

void CpuProfiler::AdjustSamplingInterval() {
  if (!processor_) return;

  base::TimeDelta base_interval = ComputeSamplingInterval();
  processor_->SetSamplingInterval(base_interval);
}

// static
void CpuProfiler::CollectSample(Isolate* isolate) {
  GetProfilersManager()->CallCollectSample(isolate);
}

void CpuProfiler::CollectSample() {
  if (processor_) {
    processor_->AddCurrentStack();
  }
}

void CpuProfiler::StartProfiling(const char* title,
                                 CpuProfilingOptions options) {
  if (profiles_->StartProfiling(title, options)) {
    TRACE_EVENT0("v8", "CpuProfiler::StartProfiling");
    AdjustSamplingInterval();
    StartProcessorIfNotStarted();
  }
}

void CpuProfiler::StartProfiling(String title, CpuProfilingOptions options) {
  StartProfiling(profiles_->GetName(title), options);
}

void CpuProfiler::StartProcessorIfNotStarted() {
  if (processor_) {
    processor_->AddCurrentStack();
    return;
  }
  isolate_->wasm_engine()->EnableCodeLogging(isolate_);
  Logger* logger = isolate_->logger();

  bool codemap_needs_initialization = false;
  if (!generator_) {
    generator_.reset(new ProfileGenerator(profiles_.get()));
    codemap_needs_initialization = true;
    CreateEntriesForRuntimeCallStats();
  }
  base::TimeDelta sampling_interval = ComputeSamplingInterval();
  processor_.reset(new SamplingEventsProcessor(
      isolate_, generator_.get(), sampling_interval, use_precise_sampling_));
  if (profiler_listener_) {
    profiler_listener_->set_observer(processor_.get());
  } else {
    profiler_listener_.reset(
        new ProfilerListener(isolate_, processor_.get(), naming_mode_));
  }
  logger->AddCodeEventListener(profiler_listener_.get());
  is_profiling_ = true;
  // Enumerate stuff we already have in the heap.
  DCHECK(isolate_->heap()->HasBeenSetUp());
  if (codemap_needs_initialization) {
    if (!FLAG_prof_browser_mode) {
      logger->LogCodeObjects();
    }
    logger->LogCompiledFunctions();
    logger->LogAccessorCallbacks();
    LogBuiltins();
  }
  // Enable stack sampling.
  processor_->AddCurrentStack();
  processor_->StartSynchronously();
}

CpuProfile* CpuProfiler::StopProfiling(const char* title) {
  if (!is_profiling_) return nullptr;
  StopProcessorIfLastProfile(title);
  CpuProfile* result = profiles_->StopProfiling(title);
  AdjustSamplingInterval();
  return result;
}

CpuProfile* CpuProfiler::StopProfiling(String title) {
  return StopProfiling(profiles_->GetName(title));
}

void CpuProfiler::StopProcessorIfLastProfile(const char* title) {
  if (!profiles_->IsLastProfile(title)) return;
  StopProcessor();
}

void CpuProfiler::StopProcessor() {
  Logger* logger = isolate_->logger();
  is_profiling_ = false;
  logger->RemoveCodeEventListener(profiler_listener_.get());
  processor_->StopSynchronously();
  processor_.reset();
}


void CpuProfiler::LogBuiltins() {
  Builtins* builtins = isolate_->builtins();
  DCHECK(builtins->is_initialized());
  for (int i = 0; i < Builtins::builtin_count; i++) {
    CodeEventsContainer evt_rec(CodeEventRecord::REPORT_BUILTIN);
    ReportBuiltinEventRecord* rec = &evt_rec.ReportBuiltinEventRecord_;
    Builtins::Name id = static_cast<Builtins::Name>(i);
    rec->instruction_start = builtins->builtin(id)->InstructionStart();
    rec->builtin_id = id;
    processor_->Enqueue(evt_rec);
  }
}

}  // namespace internal
}  // namespace v8
