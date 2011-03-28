// Copyright 2009 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <stdarg.h>

#include "v8.h"

#include "bootstrapper.h"
#include "code-stubs.h"
#include "deoptimizer.h"
#include "global-handles.h"
#include "log.h"
#include "macro-assembler.h"
#include "runtime-profiler.h"
#include "serialize.h"
#include "string-stream.h"
#include "vm-state-inl.h"

namespace v8 {
namespace internal {

#ifdef ENABLE_LOGGING_AND_PROFILING

//
// Sliding state window.  Updates counters to keep track of the last
// window of kBufferSize states.  This is useful to track where we
// spent our time.
//
class SlidingStateWindow {
 public:
  explicit SlidingStateWindow(Isolate* isolate);
  ~SlidingStateWindow();
  void AddState(StateTag state);

 private:
  static const int kBufferSize = 256;
  Counters* counters_;
  int current_index_;
  bool is_full_;
  byte buffer_[kBufferSize];


  void IncrementStateCounter(StateTag state) {
    counters_->state_counters(state)->Increment();
  }


  void DecrementStateCounter(StateTag state) {
    counters_->state_counters(state)->Decrement();
  }
};


//
// The Profiler samples pc and sp values for the main thread.
// Each sample is appended to a circular buffer.
// An independent thread removes data and writes it to the log.
// This design minimizes the time spent in the sampler.
//
class Profiler: public Thread {
 public:
  explicit Profiler(Isolate* isolate);
  void Engage();
  void Disengage();

  // Inserts collected profiling data into buffer.
  void Insert(TickSample* sample) {
    if (paused_)
      return;

    if (Succ(head_) == tail_) {
      overflow_ = true;
    } else {
      buffer_[head_] = *sample;
      head_ = Succ(head_);
      buffer_semaphore_->Signal();  // Tell we have an element.
    }
  }

  // Waits for a signal and removes profiling data.
  bool Remove(TickSample* sample) {
    buffer_semaphore_->Wait();  // Wait for an element.
    *sample = buffer_[tail_];
    bool result = overflow_;
    tail_ = Succ(tail_);
    overflow_ = false;
    return result;
  }

  void Run();

  // Pause and Resume TickSample data collection.
  bool paused() const { return paused_; }
  void pause() { paused_ = true; }
  void resume() { paused_ = false; }

 private:
  // Returns the next index in the cyclic buffer.
  int Succ(int index) { return (index + 1) % kBufferSize; }

  // Cyclic buffer for communicating profiling samples
  // between the signal handler and the worker thread.
  static const int kBufferSize = 128;
  TickSample buffer_[kBufferSize];  // Buffer storage.
  int head_;  // Index to the buffer head.
  int tail_;  // Index to the buffer tail.
  bool overflow_;  // Tell whether a buffer overflow has occurred.
  Semaphore* buffer_semaphore_;  // Sempahore used for buffer synchronization.

  // Tells whether profiler is engaged, that is, processing thread is stated.
  bool engaged_;

  // Tells whether worker thread should continue running.
  bool running_;

  // Tells whether we are currently recording tick samples.
  bool paused_;
};


//
// StackTracer implementation
//
void StackTracer::Trace(Isolate* isolate, TickSample* sample) {
  ASSERT(isolate->IsInitialized());

  sample->tos = NULL;
  sample->frames_count = 0;

  // Avoid collecting traces while doing GC.
  if (sample->state == GC) return;

  const Address js_entry_sp =
      Isolate::js_entry_sp(isolate->thread_local_top());
  if (js_entry_sp == 0) {
    // Not executing JS now.
    return;
  }

  const Address callback = isolate->external_callback();
  if (callback != NULL) {
    sample->external_callback = callback;
    sample->has_external_callback = true;
  } else {
    // Sample potential return address value for frameless invocation of
    // stubs (we'll figure out later, if this value makes sense).
    sample->tos = Memory::Address_at(sample->sp);
    sample->has_external_callback = false;
  }

  SafeStackTraceFrameIterator it(isolate,
                                 sample->fp, sample->sp,
                                 sample->sp, js_entry_sp);
  int i = 0;
  while (!it.done() && i < TickSample::kMaxFramesCount) {
    sample->stack[i++] = it.frame()->pc();
    it.Advance();
  }
  sample->frames_count = i;
}


//
// Ticker used to provide ticks to the profiler and the sliding state
// window.
//
class Ticker: public Sampler {
 public:
  explicit Ticker(Isolate* isolate, int interval):
      Sampler(isolate, interval),
      window_(NULL),
      profiler_(NULL) {}

  ~Ticker() { if (IsActive()) Stop(); }

  virtual void Tick(TickSample* sample) {
    if (profiler_) profiler_->Insert(sample);
    if (window_) window_->AddState(sample->state);
  }

  void SetWindow(SlidingStateWindow* window) {
    window_ = window;
    if (!IsActive()) Start();
  }

  void ClearWindow() {
    window_ = NULL;
    if (!profiler_ && IsActive() && !RuntimeProfiler::IsEnabled()) Stop();
  }

  void SetProfiler(Profiler* profiler) {
    ASSERT(profiler_ == NULL);
    profiler_ = profiler;
    IncreaseProfilingDepth();
    if (!FLAG_prof_lazy && !IsActive()) Start();
  }

  void ClearProfiler() {
    DecreaseProfilingDepth();
    profiler_ = NULL;
    if (!window_ && IsActive() && !RuntimeProfiler::IsEnabled()) Stop();
  }

 protected:
  virtual void DoSampleStack(TickSample* sample) {
    StackTracer::Trace(isolate(), sample);
  }

 private:
  SlidingStateWindow* window_;
  Profiler* profiler_;
};


//
// SlidingStateWindow implementation.
//
SlidingStateWindow::SlidingStateWindow(Isolate* isolate)
    : counters_(isolate->counters()), current_index_(0), is_full_(false) {
  for (int i = 0; i < kBufferSize; i++) {
    buffer_[i] = static_cast<byte>(OTHER);
  }
  isolate->logger()->ticker_->SetWindow(this);
}


SlidingStateWindow::~SlidingStateWindow() {
  LOGGER->ticker_->ClearWindow();
}


void SlidingStateWindow::AddState(StateTag state) {
  if (is_full_) {
    DecrementStateCounter(static_cast<StateTag>(buffer_[current_index_]));
  } else if (current_index_ == kBufferSize - 1) {
    is_full_ = true;
  }
  buffer_[current_index_] = static_cast<byte>(state);
  IncrementStateCounter(state);
  ASSERT(IsPowerOf2(kBufferSize));
  current_index_ = (current_index_ + 1) & (kBufferSize - 1);
}


//
// Profiler implementation.
//
Profiler::Profiler(Isolate* isolate)
    : Thread(isolate, "v8:Profiler"),
      head_(0),
      tail_(0),
      overflow_(false),
      buffer_semaphore_(OS::CreateSemaphore(0)),
      engaged_(false),
      running_(false),
      paused_(false) {
}


void Profiler::Engage() {
  if (engaged_) return;
  engaged_ = true;

  // TODO(mnaganov): This is actually "Chromium" mode. Flags need to be revised.
  // http://code.google.com/p/v8/issues/detail?id=487
  if (!FLAG_prof_lazy) {
    OS::LogSharedLibraryAddresses();
  }

  // Start thread processing the profiler buffer.
  running_ = true;
  Start();

  // Register to get ticks.
  LOGGER->ticker_->SetProfiler(this);

  LOGGER->ProfilerBeginEvent();
}


void Profiler::Disengage() {
  if (!engaged_) return;

  // Stop receiving ticks.
  LOGGER->ticker_->ClearProfiler();

  // Terminate the worker thread by setting running_ to false,
  // inserting a fake element in the queue and then wait for
  // the thread to terminate.
  running_ = false;
  TickSample sample;
  // Reset 'paused_' flag, otherwise semaphore may not be signalled.
  resume();
  Insert(&sample);
  Join();

  LOG(ISOLATE, UncheckedStringEvent("profiler", "end"));
}


void Profiler::Run() {
  TickSample sample;
  bool overflow = Remove(&sample);
  i::Isolate* isolate = ISOLATE;
  while (running_) {
    LOG(isolate, TickEvent(&sample, overflow));
    overflow = Remove(&sample);
  }
}


//
// Logger class implementation.
//

Logger::Logger()
  : ticker_(NULL),
    profiler_(NULL),
    sliding_state_window_(NULL),
    log_events_(NULL),
    logging_nesting_(0),
    cpu_profiler_nesting_(0),
    heap_profiler_nesting_(0),
    log_(new Log(this)),
    is_initialized_(false),
    last_address_(NULL),
    prev_sp_(NULL),
    prev_function_(NULL),
    prev_to_(NULL),
    prev_code_(NULL) {
}

Logger::~Logger() {
  delete log_;
}

#define DECLARE_EVENT(ignore1, name) name,
static const char* const kLogEventsNames[Logger::NUMBER_OF_LOG_EVENTS] = {
  LOG_EVENTS_AND_TAGS_LIST(DECLARE_EVENT)
};
#undef DECLARE_EVENT


void Logger::ProfilerBeginEvent() {
  if (!log_->IsEnabled()) return;
  LogMessageBuilder msg(this);
  msg.Append("profiler,\"begin\",%d\n", kSamplingIntervalMs);
  msg.WriteToLogFile();
}

#endif  // ENABLE_LOGGING_AND_PROFILING


void Logger::StringEvent(const char* name, const char* value) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (FLAG_log) UncheckedStringEvent(name, value);
#endif
}


#ifdef ENABLE_LOGGING_AND_PROFILING
void Logger::UncheckedStringEvent(const char* name, const char* value) {
  if (!log_->IsEnabled()) return;
  LogMessageBuilder msg(this);
  msg.Append("%s,\"%s\"\n", name, value);
  msg.WriteToLogFile();
}
#endif


void Logger::IntEvent(const char* name, int value) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (FLAG_log) UncheckedIntEvent(name, value);
#endif
}


void Logger::IntPtrTEvent(const char* name, intptr_t value) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (FLAG_log) UncheckedIntPtrTEvent(name, value);
#endif
}


#ifdef ENABLE_LOGGING_AND_PROFILING
void Logger::UncheckedIntEvent(const char* name, int value) {
  if (!log_->IsEnabled()) return;
  LogMessageBuilder msg(this);
  msg.Append("%s,%d\n", name, value);
  msg.WriteToLogFile();
}
#endif


#ifdef ENABLE_LOGGING_AND_PROFILING
void Logger::UncheckedIntPtrTEvent(const char* name, intptr_t value) {
  if (!log_->IsEnabled()) return;
  LogMessageBuilder msg(this);
  msg.Append("%s,%" V8_PTR_PREFIX "d\n", name, value);
  msg.WriteToLogFile();
}
#endif


void Logger::HandleEvent(const char* name, Object** location) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!log_->IsEnabled() || !FLAG_log_handles) return;
  LogMessageBuilder msg(this);
  msg.Append("%s,0x%" V8PRIxPTR "\n", name, location);
  msg.WriteToLogFile();
#endif
}


#ifdef ENABLE_LOGGING_AND_PROFILING
// ApiEvent is private so all the calls come from the Logger class.  It is the
// caller's responsibility to ensure that log is enabled and that
// FLAG_log_api is true.
void Logger::ApiEvent(const char* format, ...) {
  ASSERT(log_->IsEnabled() && FLAG_log_api);
  LogMessageBuilder msg(this);
  va_list ap;
  va_start(ap, format);
  msg.AppendVA(format, ap);
  va_end(ap);
  msg.WriteToLogFile();
}
#endif


void Logger::ApiNamedSecurityCheck(Object* key) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!log_->IsEnabled() || !FLAG_log_api) return;
  if (key->IsString()) {
    SmartPointer<char> str =
        String::cast(key)->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
    ApiEvent("api,check-security,\"%s\"\n", *str);
  } else if (key->IsUndefined()) {
    ApiEvent("api,check-security,undefined\n");
  } else {
    ApiEvent("api,check-security,['no-name']\n");
  }
#endif
}


void Logger::SharedLibraryEvent(const char* library_path,
                                uintptr_t start,
                                uintptr_t end) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!log_->IsEnabled() || !FLAG_prof) return;
  LogMessageBuilder msg(this);
  msg.Append("shared-library,\"%s\",0x%08" V8PRIxPTR ",0x%08" V8PRIxPTR "\n",
             library_path,
             start,
             end);
  msg.WriteToLogFile();
#endif
}


void Logger::SharedLibraryEvent(const wchar_t* library_path,
                                uintptr_t start,
                                uintptr_t end) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!log_->IsEnabled() || !FLAG_prof) return;
  LogMessageBuilder msg(this);
  msg.Append("shared-library,\"%ls\",0x%08" V8PRIxPTR ",0x%08" V8PRIxPTR "\n",
             library_path,
             start,
             end);
  msg.WriteToLogFile();
#endif
}


#ifdef ENABLE_LOGGING_AND_PROFILING
void Logger::LogRegExpSource(Handle<JSRegExp> regexp) {
  // Prints "/" + re.source + "/" +
  //      (re.global?"g":"") + (re.ignorecase?"i":"") + (re.multiline?"m":"")
  LogMessageBuilder msg(this);

  Handle<Object> source = GetProperty(regexp, "source");
  if (!source->IsString()) {
    msg.Append("no source");
    return;
  }

  switch (regexp->TypeTag()) {
    case JSRegExp::ATOM:
      msg.Append('a');
      break;
    default:
      break;
  }
  msg.Append('/');
  msg.AppendDetailed(*Handle<String>::cast(source), false);
  msg.Append('/');

  // global flag
  Handle<Object> global = GetProperty(regexp, "global");
  if (global->IsTrue()) {
    msg.Append('g');
  }
  // ignorecase flag
  Handle<Object> ignorecase = GetProperty(regexp, "ignoreCase");
  if (ignorecase->IsTrue()) {
    msg.Append('i');
  }
  // multiline flag
  Handle<Object> multiline = GetProperty(regexp, "multiline");
  if (multiline->IsTrue()) {
    msg.Append('m');
  }

  msg.WriteToLogFile();
}
#endif  // ENABLE_LOGGING_AND_PROFILING


void Logger::RegExpCompileEvent(Handle<JSRegExp> regexp, bool in_cache) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!log_->IsEnabled() || !FLAG_log_regexp) return;
  LogMessageBuilder msg(this);
  msg.Append("regexp-compile,");
  LogRegExpSource(regexp);
  msg.Append(in_cache ? ",hit\n" : ",miss\n");
  msg.WriteToLogFile();
#endif
}


void Logger::LogRuntime(Vector<const char> format, JSArray* args) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!log_->IsEnabled() || !FLAG_log_runtime) return;
  HandleScope scope;
  LogMessageBuilder msg(this);
  for (int i = 0; i < format.length(); i++) {
    char c = format[i];
    if (c == '%' && i <= format.length() - 2) {
      i++;
      ASSERT('0' <= format[i] && format[i] <= '9');
      MaybeObject* maybe = args->GetElement(format[i] - '0');
      Object* obj;
      if (!maybe->ToObject(&obj)) {
        msg.Append("<exception>");
        continue;
      }
      i++;
      switch (format[i]) {
        case 's':
          msg.AppendDetailed(String::cast(obj), false);
          break;
        case 'S':
          msg.AppendDetailed(String::cast(obj), true);
          break;
        case 'r':
          Logger::LogRegExpSource(Handle<JSRegExp>(JSRegExp::cast(obj)));
          break;
        case 'x':
          msg.Append("0x%x", Smi::cast(obj)->value());
          break;
        case 'i':
          msg.Append("%i", Smi::cast(obj)->value());
          break;
        default:
          UNREACHABLE();
      }
    } else {
      msg.Append(c);
    }
  }
  msg.Append('\n');
  msg.WriteToLogFile();
#endif
}


void Logger::ApiIndexedSecurityCheck(uint32_t index) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!log_->IsEnabled() || !FLAG_log_api) return;
  ApiEvent("api,check-security,%u\n", index);
#endif
}


void Logger::ApiNamedPropertyAccess(const char* tag,
                                    JSObject* holder,
                                    Object* name) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  ASSERT(name->IsString());
  if (!log_->IsEnabled() || !FLAG_log_api) return;
  String* class_name_obj = holder->class_name();
  SmartPointer<char> class_name =
      class_name_obj->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  SmartPointer<char> property_name =
      String::cast(name)->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  ApiEvent("api,%s,\"%s\",\"%s\"\n", tag, *class_name, *property_name);
#endif
}

void Logger::ApiIndexedPropertyAccess(const char* tag,
                                      JSObject* holder,
                                      uint32_t index) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!log_->IsEnabled() || !FLAG_log_api) return;
  String* class_name_obj = holder->class_name();
  SmartPointer<char> class_name =
      class_name_obj->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  ApiEvent("api,%s,\"%s\",%u\n", tag, *class_name, index);
#endif
}

void Logger::ApiObjectAccess(const char* tag, JSObject* object) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!log_->IsEnabled() || !FLAG_log_api) return;
  String* class_name_obj = object->class_name();
  SmartPointer<char> class_name =
      class_name_obj->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  ApiEvent("api,%s,\"%s\"\n", tag, *class_name);
#endif
}


void Logger::ApiEntryCall(const char* name) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!log_->IsEnabled() || !FLAG_log_api) return;
  ApiEvent("api,%s\n", name);
#endif
}


void Logger::NewEvent(const char* name, void* object, size_t size) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!log_->IsEnabled() || !FLAG_log) return;
  LogMessageBuilder msg(this);
  msg.Append("new,%s,0x%" V8PRIxPTR ",%u\n", name, object,
             static_cast<unsigned int>(size));
  msg.WriteToLogFile();
#endif
}


void Logger::DeleteEvent(const char* name, void* object) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!log_->IsEnabled() || !FLAG_log) return;
  LogMessageBuilder msg(this);
  msg.Append("delete,%s,0x%" V8PRIxPTR "\n", name, object);
  msg.WriteToLogFile();
#endif
}


void Logger::NewEventStatic(const char* name, void* object, size_t size) {
  LOGGER->NewEvent(name, object, size);
}


void Logger::DeleteEventStatic(const char* name, void* object) {
  LOGGER->DeleteEvent(name, object);
}

#ifdef ENABLE_LOGGING_AND_PROFILING
void Logger::CallbackEventInternal(const char* prefix, const char* name,
                                   Address entry_point) {
  if (!log_->IsEnabled() || !FLAG_log_code) return;
  LogMessageBuilder msg(this);
  msg.Append("%s,%s,",
             kLogEventsNames[CODE_CREATION_EVENT],
             kLogEventsNames[CALLBACK_TAG]);
  msg.AppendAddress(entry_point);
  msg.Append(",1,\"%s%s\"", prefix, name);
  msg.Append('\n');
  msg.WriteToLogFile();
}
#endif


void Logger::CallbackEvent(String* name, Address entry_point) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!log_->IsEnabled() || !FLAG_log_code) return;
  SmartPointer<char> str =
      name->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  CallbackEventInternal("", *str, entry_point);
#endif
}


void Logger::GetterCallbackEvent(String* name, Address entry_point) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!log_->IsEnabled() || !FLAG_log_code) return;
  SmartPointer<char> str =
      name->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  CallbackEventInternal("get ", *str, entry_point);
#endif
}


void Logger::SetterCallbackEvent(String* name, Address entry_point) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!log_->IsEnabled() || !FLAG_log_code) return;
  SmartPointer<char> str =
      name->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  CallbackEventInternal("set ", *str, entry_point);
#endif
}


void Logger::CodeCreateEvent(LogEventsAndTags tag,
                             Code* code,
                             const char* comment) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!log_->IsEnabled() || !FLAG_log_code) return;
  LogMessageBuilder msg(this);
  msg.Append("%s,%s,",
             kLogEventsNames[CODE_CREATION_EVENT],
             kLogEventsNames[tag]);
  msg.AppendAddress(code->address());
  msg.Append(",%d,\"", code->ExecutableSize());
  for (const char* p = comment; *p != '\0'; p++) {
    if (*p == '"') {
      msg.Append('\\');
    }
    msg.Append(*p);
  }
  msg.Append('"');
  LowLevelCodeCreateEvent(code, &msg);
  msg.Append('\n');
  msg.WriteToLogFile();
#endif
}


void Logger::CodeCreateEvent(LogEventsAndTags tag,
                             Code* code,
                             String* name) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (name != NULL) {
    SmartPointer<char> str =
        name->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
    CodeCreateEvent(tag, code, *str);
  } else {
    CodeCreateEvent(tag, code, "");
  }
#endif
}


#ifdef ENABLE_LOGGING_AND_PROFILING
// ComputeMarker must only be used when SharedFunctionInfo is known.
static const char* ComputeMarker(Code* code) {
  switch (code->kind()) {
    case Code::FUNCTION: return code->optimizable() ? "~" : "";
    case Code::OPTIMIZED_FUNCTION: return "*";
    default: return "";
  }
}
#endif


void Logger::CodeCreateEvent(LogEventsAndTags tag,
                             Code* code,
                             SharedFunctionInfo* shared,
                             String* name) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!log_->IsEnabled() || !FLAG_log_code) return;
  if (code == Isolate::Current()->builtins()->builtin(
      Builtins::kLazyCompile))
    return;

  LogMessageBuilder msg(this);
  SmartPointer<char> str =
      name->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  msg.Append("%s,%s,",
             kLogEventsNames[CODE_CREATION_EVENT],
             kLogEventsNames[tag]);
  msg.AppendAddress(code->address());
  msg.Append(",%d,\"%s\",", code->ExecutableSize(), *str);
  msg.AppendAddress(shared->address());
  msg.Append(",%s", ComputeMarker(code));
  LowLevelCodeCreateEvent(code, &msg);
  msg.Append('\n');
  msg.WriteToLogFile();
#endif
}


// Although, it is possible to extract source and line from
// the SharedFunctionInfo object, we left it to caller
// to leave logging functions free from heap allocations.
void Logger::CodeCreateEvent(LogEventsAndTags tag,
                             Code* code,
                             SharedFunctionInfo* shared,
                             String* source, int line) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!log_->IsEnabled() || !FLAG_log_code) return;
  LogMessageBuilder msg(this);
  SmartPointer<char> name =
      shared->DebugName()->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  SmartPointer<char> sourcestr =
      source->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  msg.Append("%s,%s,",
             kLogEventsNames[CODE_CREATION_EVENT],
             kLogEventsNames[tag]);
  msg.AppendAddress(code->address());
  msg.Append(",%d,\"%s %s:%d\",",
             code->ExecutableSize(),
             *name,
             *sourcestr,
             line);
  msg.AppendAddress(shared->address());
  msg.Append(",%s", ComputeMarker(code));
  LowLevelCodeCreateEvent(code, &msg);
  msg.Append('\n');
  msg.WriteToLogFile();
#endif
}


void Logger::CodeCreateEvent(LogEventsAndTags tag, Code* code, int args_count) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!log_->IsEnabled() || !FLAG_log_code) return;
  LogMessageBuilder msg(this);
  msg.Append("%s,%s,",
             kLogEventsNames[CODE_CREATION_EVENT],
             kLogEventsNames[tag]);
  msg.AppendAddress(code->address());
  msg.Append(",%d,\"args_count: %d\"", code->ExecutableSize(), args_count);
  LowLevelCodeCreateEvent(code, &msg);
  msg.Append('\n');
  msg.WriteToLogFile();
#endif
}


void Logger::CodeMovingGCEvent() {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!log_->IsEnabled() || !FLAG_log_code || !FLAG_ll_prof) return;
  LogMessageBuilder msg(this);
  msg.Append("%s\n", kLogEventsNames[CODE_MOVING_GC]);
  msg.WriteToLogFile();
  OS::SignalCodeMovingGC();
#endif
}


void Logger::RegExpCodeCreateEvent(Code* code, String* source) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!log_->IsEnabled() || !FLAG_log_code) return;
  LogMessageBuilder msg(this);
  msg.Append("%s,%s,",
             kLogEventsNames[CODE_CREATION_EVENT],
             kLogEventsNames[REG_EXP_TAG]);
  msg.AppendAddress(code->address());
  msg.Append(",%d,\"", code->ExecutableSize());
  msg.AppendDetailed(source, false);
  msg.Append('\"');
  LowLevelCodeCreateEvent(code, &msg);
  msg.Append('\n');
  msg.WriteToLogFile();
#endif
}


void Logger::CodeMoveEvent(Address from, Address to) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  MoveEventInternal(CODE_MOVE_EVENT, from, to);
#endif
}


void Logger::CodeDeleteEvent(Address from) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  DeleteEventInternal(CODE_DELETE_EVENT, from);
#endif
}


void Logger::SnapshotPositionEvent(Address addr, int pos) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!log_->IsEnabled() || !FLAG_log_snapshot_positions) return;
  LogMessageBuilder msg(this);
  msg.Append("%s,", kLogEventsNames[SNAPSHOT_POSITION_EVENT]);
  msg.AppendAddress(addr);
  msg.Append(",%d", pos);
  msg.Append('\n');
  msg.WriteToLogFile();
#endif
}


void Logger::SharedFunctionInfoMoveEvent(Address from, Address to) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  MoveEventInternal(SHARED_FUNC_MOVE_EVENT, from, to);
#endif
}


#ifdef ENABLE_LOGGING_AND_PROFILING
void Logger::MoveEventInternal(LogEventsAndTags event,
                               Address from,
                               Address to) {
  if (!log_->IsEnabled() || !FLAG_log_code) return;
  LogMessageBuilder msg(this);
  msg.Append("%s,", kLogEventsNames[event]);
  msg.AppendAddress(from);
  msg.Append(',');
  msg.AppendAddress(to);
  msg.Append('\n');
  msg.WriteToLogFile();
}
#endif


#ifdef ENABLE_LOGGING_AND_PROFILING
void Logger::DeleteEventInternal(LogEventsAndTags event, Address from) {
  if (!log_->IsEnabled() || !FLAG_log_code) return;
  LogMessageBuilder msg(this);
  msg.Append("%s,", kLogEventsNames[event]);
  msg.AppendAddress(from);
  msg.Append('\n');
  msg.WriteToLogFile();
}
#endif


void Logger::ResourceEvent(const char* name, const char* tag) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!log_->IsEnabled() || !FLAG_log) return;
  LogMessageBuilder msg(this);
  msg.Append("%s,%s,", name, tag);

  uint32_t sec, usec;
  if (OS::GetUserTime(&sec, &usec) != -1) {
    msg.Append("%d,%d,", sec, usec);
  }
  msg.Append("%.0f", OS::TimeCurrentMillis());

  msg.Append('\n');
  msg.WriteToLogFile();
#endif
}


void Logger::SuspectReadEvent(String* name, Object* obj) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!log_->IsEnabled() || !FLAG_log_suspect) return;
  LogMessageBuilder msg(this);
  String* class_name = obj->IsJSObject()
                       ? JSObject::cast(obj)->class_name()
                       : HEAP->empty_string();
  msg.Append("suspect-read,");
  msg.Append(class_name);
  msg.Append(',');
  msg.Append('"');
  msg.Append(name);
  msg.Append('"');
  msg.Append('\n');
  msg.WriteToLogFile();
#endif
}


void Logger::HeapSampleBeginEvent(const char* space, const char* kind) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!log_->IsEnabled() || !FLAG_log_gc) return;
  LogMessageBuilder msg(this);
  // Using non-relative system time in order to be able to synchronize with
  // external memory profiling events (e.g. DOM memory size).
  msg.Append("heap-sample-begin,\"%s\",\"%s\",%.0f\n",
             space, kind, OS::TimeCurrentMillis());
  msg.WriteToLogFile();
#endif
}


void Logger::HeapSampleStats(const char* space, const char* kind,
                             intptr_t capacity, intptr_t used) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!log_->IsEnabled() || !FLAG_log_gc) return;
  LogMessageBuilder msg(this);
  msg.Append("heap-sample-stats,\"%s\",\"%s\","
                 "%" V8_PTR_PREFIX "d,%" V8_PTR_PREFIX "d\n",
             space, kind, capacity, used);
  msg.WriteToLogFile();
#endif
}


void Logger::HeapSampleEndEvent(const char* space, const char* kind) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!log_->IsEnabled() || !FLAG_log_gc) return;
  LogMessageBuilder msg(this);
  msg.Append("heap-sample-end,\"%s\",\"%s\"\n", space, kind);
  msg.WriteToLogFile();
#endif
}


void Logger::HeapSampleItemEvent(const char* type, int number, int bytes) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!log_->IsEnabled() || !FLAG_log_gc) return;
  LogMessageBuilder msg(this);
  msg.Append("heap-sample-item,%s,%d,%d\n", type, number, bytes);
  msg.WriteToLogFile();
#endif
}


void Logger::HeapSampleJSConstructorEvent(const char* constructor,
                                          int number, int bytes) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!log_->IsEnabled() || !FLAG_log_gc) return;
  LogMessageBuilder msg(this);
  msg.Append("heap-js-cons-item,%s,%d,%d\n", constructor, number, bytes);
  msg.WriteToLogFile();
#endif
}

// Event starts with comma, so we don't have it in the format string.
static const char kEventText[] = "heap-js-ret-item,%s";
// We take placeholder strings into account, but it's OK to be conservative.
static const int kEventTextLen = sizeof(kEventText)/sizeof(kEventText[0]);

void Logger::HeapSampleJSRetainersEvent(
    const char* constructor, const char* event) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!log_->IsEnabled() || !FLAG_log_gc) return;
  const int cons_len = StrLength(constructor);
  const int event_len = StrLength(event);
  int pos = 0;
  // Retainer lists can be long. We may need to split them into multiple events.
  do {
    LogMessageBuilder msg(this);
    msg.Append(kEventText, constructor);
    int to_write = event_len - pos;
    if (to_write > Log::kMessageBufferSize - (cons_len + kEventTextLen)) {
      int cut_pos = pos + Log::kMessageBufferSize - (cons_len + kEventTextLen);
      ASSERT(cut_pos < event_len);
      while (cut_pos > pos && event[cut_pos] != ',') --cut_pos;
      if (event[cut_pos] != ',') {
        // Crash in debug mode, skip in release mode.
        ASSERT(false);
        return;
      }
      // Append a piece of event that fits, without trailing comma.
      msg.AppendStringPart(event + pos, cut_pos - pos);
      // Start next piece with comma.
      pos = cut_pos;
    } else {
      msg.Append("%s", event + pos);
      pos += event_len;
    }
    msg.Append('\n');
    msg.WriteToLogFile();
  } while (pos < event_len);
#endif
}


void Logger::HeapSampleJSProducerEvent(const char* constructor,
                                       Address* stack) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!log_->IsEnabled() || !FLAG_log_gc) return;
  LogMessageBuilder msg(this);
  msg.Append("heap-js-prod-item,%s", constructor);
  while (*stack != NULL) {
    msg.Append(",0x%" V8PRIxPTR, *stack++);
  }
  msg.Append("\n");
  msg.WriteToLogFile();
#endif
}


void Logger::DebugTag(const char* call_site_tag) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!log_->IsEnabled() || !FLAG_log) return;
  LogMessageBuilder msg(this);
  msg.Append("debug-tag,%s\n", call_site_tag);
  msg.WriteToLogFile();
#endif
}


void Logger::DebugEvent(const char* event_type, Vector<uint16_t> parameter) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!log_->IsEnabled() || !FLAG_log) return;
  StringBuilder s(parameter.length() + 1);
  for (int i = 0; i < parameter.length(); ++i) {
    s.AddCharacter(static_cast<char>(parameter[i]));
  }
  char* parameter_string = s.Finalize();
  LogMessageBuilder msg(this);
  msg.Append("debug-queue-event,%s,%15.3f,%s\n",
             event_type,
             OS::TimeCurrentMillis(),
             parameter_string);
  DeleteArray(parameter_string);
  msg.WriteToLogFile();
#endif
}


#ifdef ENABLE_LOGGING_AND_PROFILING
void Logger::TickEvent(TickSample* sample, bool overflow) {
  if (!log_->IsEnabled() || !FLAG_prof) return;
  LogMessageBuilder msg(this);
  msg.Append("%s,", kLogEventsNames[TICK_EVENT]);
  msg.AppendAddress(sample->pc);
  msg.Append(',');
  msg.AppendAddress(sample->sp);
  msg.Append(',');
  if (sample->has_external_callback) {
    msg.Append(",1,");
    msg.AppendAddress(sample->external_callback);
  } else {
    msg.Append(",0,");
    msg.AppendAddress(sample->tos);
  }
  msg.Append(",%d", static_cast<int>(sample->state));
  if (overflow) {
    msg.Append(",overflow");
  }
  for (int i = 0; i < sample->frames_count; ++i) {
    msg.Append(',');
    msg.AppendAddress(sample->stack[i]);
  }
  msg.Append('\n');
  msg.WriteToLogFile();
}


int Logger::GetActiveProfilerModules() {
  int result = PROFILER_MODULE_NONE;
  if (profiler_ != NULL && !profiler_->paused()) {
    result |= PROFILER_MODULE_CPU;
  }
  if (FLAG_log_gc) {
    result |= PROFILER_MODULE_HEAP_STATS | PROFILER_MODULE_JS_CONSTRUCTORS;
  }
  return result;
}


void Logger::PauseProfiler(int flags, int tag) {
  if (!log_->IsEnabled()) return;
  if (profiler_ != NULL && (flags & PROFILER_MODULE_CPU)) {
    // It is OK to have negative nesting.
    if (--cpu_profiler_nesting_ == 0) {
      profiler_->pause();
      if (FLAG_prof_lazy) {
        if (!FLAG_sliding_state_window && !RuntimeProfiler::IsEnabled()) {
          ticker_->Stop();
        }
        FLAG_log_code = false;
        // Must be the same message as Log::kDynamicBufferSeal.
        LOG(ISOLATE, UncheckedStringEvent("profiler", "pause"));
      }
      --logging_nesting_;
    }
  }
  if (flags &
      (PROFILER_MODULE_HEAP_STATS | PROFILER_MODULE_JS_CONSTRUCTORS)) {
    if (--heap_profiler_nesting_ == 0) {
      FLAG_log_gc = false;
      --logging_nesting_;
    }
  }
  if (tag != 0) {
    UncheckedIntEvent("close-tag", tag);
  }
}


void Logger::ResumeProfiler(int flags, int tag) {
  if (!log_->IsEnabled()) return;
  if (tag != 0) {
    UncheckedIntEvent("open-tag", tag);
  }
  if (profiler_ != NULL && (flags & PROFILER_MODULE_CPU)) {
    if (cpu_profiler_nesting_++ == 0) {
      ++logging_nesting_;
      if (FLAG_prof_lazy) {
        profiler_->Engage();
        LOG(ISOLATE, UncheckedStringEvent("profiler", "resume"));
        FLAG_log_code = true;
        LogCompiledFunctions();
        LogAccessorCallbacks();
        if (!FLAG_sliding_state_window && !ticker_->IsActive()) {
          ticker_->Start();
        }
      }
      profiler_->resume();
    }
  }
  if (flags &
      (PROFILER_MODULE_HEAP_STATS | PROFILER_MODULE_JS_CONSTRUCTORS)) {
    if (heap_profiler_nesting_++ == 0) {
      ++logging_nesting_;
      FLAG_log_gc = true;
    }
  }
}


// This function can be called when Log's mutex is acquired,
// either from main or Profiler's thread.
void Logger::LogFailure() {
  PauseProfiler(PROFILER_MODULE_CPU, 0);
}


bool Logger::IsProfilerSamplerActive() {
  return ticker_->IsActive();
}


int Logger::GetLogLines(int from_pos, char* dest_buf, int max_size) {
  return log_->GetLogLines(from_pos, dest_buf, max_size);
}


class EnumerateOptimizedFunctionsVisitor: public OptimizedFunctionVisitor {
 public:
  EnumerateOptimizedFunctionsVisitor(Handle<SharedFunctionInfo>* sfis,
                                     Handle<Code>* code_objects,
                                     int* count)
      : sfis_(sfis), code_objects_(code_objects), count_(count) { }

  virtual void EnterContext(Context* context) {}
  virtual void LeaveContext(Context* context) {}

  virtual void VisitFunction(JSFunction* function) {
    if (sfis_ != NULL) {
      sfis_[*count_] = Handle<SharedFunctionInfo>(function->shared());
    }
    if (code_objects_ != NULL) {
      ASSERT(function->code()->kind() == Code::OPTIMIZED_FUNCTION);
      code_objects_[*count_] = Handle<Code>(function->code());
    }
    *count_ = *count_ + 1;
  }

 private:
  Handle<SharedFunctionInfo>* sfis_;
  Handle<Code>* code_objects_;
  int* count_;
};


static int EnumerateCompiledFunctions(Handle<SharedFunctionInfo>* sfis,
                                      Handle<Code>* code_objects) {
  AssertNoAllocation no_alloc;
  int compiled_funcs_count = 0;

  // Iterate the heap to find shared function info objects and record
  // the unoptimized code for them.
  HeapIterator iterator;
  for (HeapObject* obj = iterator.next(); obj != NULL; obj = iterator.next()) {
    if (!obj->IsSharedFunctionInfo()) continue;
    SharedFunctionInfo* sfi = SharedFunctionInfo::cast(obj);
    if (sfi->is_compiled()
        && (!sfi->script()->IsScript()
            || Script::cast(sfi->script())->HasValidSource())) {
      if (sfis != NULL) {
        sfis[compiled_funcs_count] = Handle<SharedFunctionInfo>(sfi);
      }
      if (code_objects != NULL) {
        code_objects[compiled_funcs_count] = Handle<Code>(sfi->code());
      }
      ++compiled_funcs_count;
    }
  }

  // Iterate all optimized functions in all contexts.
  EnumerateOptimizedFunctionsVisitor visitor(sfis,
                                             code_objects,
                                             &compiled_funcs_count);
  Deoptimizer::VisitAllOptimizedFunctions(&visitor);

  return compiled_funcs_count;
}


void Logger::LogCodeObject(Object* object) {
  if (FLAG_log_code) {
    Code* code_object = Code::cast(object);
    LogEventsAndTags tag = Logger::STUB_TAG;
    const char* description = "Unknown code from the snapshot";
    switch (code_object->kind()) {
      case Code::FUNCTION:
      case Code::OPTIMIZED_FUNCTION:
        return;  // We log this later using LogCompiledFunctions.
      case Code::BINARY_OP_IC:  // fall through
      case Code::TYPE_RECORDING_BINARY_OP_IC:   // fall through
      case Code::COMPARE_IC:  // fall through
      case Code::STUB:
        description =
            CodeStub::MajorName(CodeStub::GetMajorKey(code_object), true);
        if (description == NULL)
          description = "A stub from the snapshot";
        tag = Logger::STUB_TAG;
        break;
      case Code::BUILTIN:
        description = "A builtin from the snapshot";
        tag = Logger::BUILTIN_TAG;
        break;
      case Code::KEYED_LOAD_IC:
        description = "A keyed load IC from the snapshot";
        tag = Logger::KEYED_LOAD_IC_TAG;
        break;
      case Code::KEYED_EXTERNAL_ARRAY_LOAD_IC:
        description = "A keyed external array load IC from the snapshot";
        tag = Logger::KEYED_EXTERNAL_ARRAY_LOAD_IC_TAG;
        break;
      case Code::LOAD_IC:
        description = "A load IC from the snapshot";
        tag = Logger::LOAD_IC_TAG;
        break;
      case Code::STORE_IC:
        description = "A store IC from the snapshot";
        tag = Logger::STORE_IC_TAG;
        break;
      case Code::KEYED_STORE_IC:
        description = "A keyed store IC from the snapshot";
        tag = Logger::KEYED_STORE_IC_TAG;
        break;
      case Code::KEYED_EXTERNAL_ARRAY_STORE_IC:
        description = "A keyed external array store IC from the snapshot";
        tag = Logger::KEYED_EXTERNAL_ARRAY_STORE_IC_TAG;
        break;
      case Code::CALL_IC:
        description = "A call IC from the snapshot";
        tag = Logger::CALL_IC_TAG;
        break;
      case Code::KEYED_CALL_IC:
        description = "A keyed call IC from the snapshot";
        tag = Logger::KEYED_CALL_IC_TAG;
        break;
    }
    PROFILE(ISOLATE, CodeCreateEvent(tag, code_object, description));
  }
}


void Logger::LogCodeInfo() {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!log_->IsEnabled() || !FLAG_log_code || !FLAG_ll_prof) return;
#if V8_TARGET_ARCH_IA32
  const char arch[] = "ia32";
#elif V8_TARGET_ARCH_X64
  const char arch[] = "x64";
#elif V8_TARGET_ARCH_ARM
  const char arch[] = "arm";
#else
  const char arch[] = "unknown";
#endif
  LogMessageBuilder msg(this);
  msg.Append("code-info,%s,%d\n", arch, Code::kHeaderSize);
  msg.WriteToLogFile();
#endif  // ENABLE_LOGGING_AND_PROFILING
}


void Logger::LowLevelCodeCreateEvent(Code* code, LogMessageBuilder* msg) {
  if (!FLAG_ll_prof || log_->output_code_handle_ == NULL) return;
  int pos = static_cast<int>(ftell(log_->output_code_handle_));
  size_t rv = fwrite(code->instruction_start(), 1, code->instruction_size(),
                     log_->output_code_handle_);
  ASSERT(static_cast<size_t>(code->instruction_size()) == rv);
  USE(rv);
  msg->Append(",%d", pos);
}


void Logger::LogCodeObjects() {
  AssertNoAllocation no_alloc;
  HeapIterator iterator;
  for (HeapObject* obj = iterator.next(); obj != NULL; obj = iterator.next()) {
    if (obj->IsCode()) LogCodeObject(obj);
  }
}


void Logger::LogCompiledFunctions() {
  HandleScope scope;
  const int compiled_funcs_count = EnumerateCompiledFunctions(NULL, NULL);
  ScopedVector< Handle<SharedFunctionInfo> > sfis(compiled_funcs_count);
  ScopedVector< Handle<Code> > code_objects(compiled_funcs_count);
  EnumerateCompiledFunctions(sfis.start(), code_objects.start());

  // During iteration, there can be heap allocation due to
  // GetScriptLineNumber call.
  for (int i = 0; i < compiled_funcs_count; ++i) {
    if (*code_objects[i] == Isolate::Current()->builtins()->builtin(
        Builtins::kLazyCompile))
      continue;
    Handle<SharedFunctionInfo> shared = sfis[i];
    Handle<String> func_name(shared->DebugName());
    if (shared->script()->IsScript()) {
      Handle<Script> script(Script::cast(shared->script()));
      if (script->name()->IsString()) {
        Handle<String> script_name(String::cast(script->name()));
        int line_num = GetScriptLineNumber(script, shared->start_position());
        if (line_num > 0) {
          PROFILE(ISOLATE,
                  CodeCreateEvent(
                    Logger::ToNativeByScript(Logger::LAZY_COMPILE_TAG, *script),
                    *code_objects[i], *shared,
                    *script_name, line_num + 1));
        } else {
          // Can't distinguish eval and script here, so always use Script.
          PROFILE(ISOLATE,
                  CodeCreateEvent(
                      Logger::ToNativeByScript(Logger::SCRIPT_TAG, *script),
                      *code_objects[i], *shared, *script_name));
        }
      } else {
        PROFILE(ISOLATE,
                CodeCreateEvent(
                    Logger::ToNativeByScript(Logger::LAZY_COMPILE_TAG, *script),
                    *code_objects[i], *shared, *func_name));
      }
    } else if (shared->IsApiFunction()) {
      // API function.
      FunctionTemplateInfo* fun_data = shared->get_api_func_data();
      Object* raw_call_data = fun_data->call_code();
      if (!raw_call_data->IsUndefined()) {
        CallHandlerInfo* call_data = CallHandlerInfo::cast(raw_call_data);
        Object* callback_obj = call_data->callback();
        Address entry_point = v8::ToCData<Address>(callback_obj);
        PROFILE(ISOLATE, CallbackEvent(*func_name, entry_point));
      }
    } else {
      PROFILE(ISOLATE,
              CodeCreateEvent(
                  Logger::LAZY_COMPILE_TAG, *code_objects[i],
                  *shared, *func_name));
    }
  }
}


void Logger::LogAccessorCallbacks() {
  AssertNoAllocation no_alloc;
  HeapIterator iterator;
  i::Isolate* isolate = ISOLATE;
  for (HeapObject* obj = iterator.next(); obj != NULL; obj = iterator.next()) {
    if (!obj->IsAccessorInfo()) continue;
    AccessorInfo* ai = AccessorInfo::cast(obj);
    if (!ai->name()->IsString()) continue;
    String* name = String::cast(ai->name());
    Address getter_entry = v8::ToCData<Address>(ai->getter());
    if (getter_entry != 0) {
      PROFILE(isolate, GetterCallbackEvent(name, getter_entry));
    }
    Address setter_entry = v8::ToCData<Address>(ai->setter());
    if (setter_entry != 0) {
      PROFILE(isolate, SetterCallbackEvent(name, setter_entry));
    }
  }
}

#endif


bool Logger::Setup() {
#ifdef ENABLE_LOGGING_AND_PROFILING
  // Tests and EnsureInitialize() can call this twice in a row. It's harmless.
  if (is_initialized_) return true;
  is_initialized_ = true;

  // --ll-prof implies --log-code and --log-snapshot-positions.
  if (FLAG_ll_prof) {
    FLAG_log_code = true;
    FLAG_log_snapshot_positions = true;
  }

  // --prof_lazy controls --log-code, implies --noprof_auto.
  if (FLAG_prof_lazy) {
    FLAG_log_code = false;
    FLAG_prof_auto = false;
  }

  // TODO(isolates): this assert introduces cyclic dependency (logger
  // -> thread local top -> heap -> logger).
  // ASSERT(VMState::is_outermost_external());

  log_->Initialize();

  if (FLAG_ll_prof) LogCodeInfo();

  ticker_ = new Ticker(Isolate::Current(), kSamplingIntervalMs);

  Isolate* isolate = Isolate::Current();
  if (FLAG_sliding_state_window && sliding_state_window_ == NULL) {
    sliding_state_window_ = new SlidingStateWindow(isolate);
  }

  bool start_logging = FLAG_log || FLAG_log_runtime || FLAG_log_api
    || FLAG_log_code || FLAG_log_gc || FLAG_log_handles || FLAG_log_suspect
    || FLAG_log_regexp || FLAG_log_state_changes;

  if (start_logging) {
    logging_nesting_ = 1;
  }

  if (FLAG_prof) {
    profiler_ = new Profiler(isolate);
    if (!FLAG_prof_auto) {
      profiler_->pause();
    } else {
      logging_nesting_ = 1;
    }
    if (!FLAG_prof_lazy) {
      profiler_->Engage();
    }
  }

  return true;

#else
  return false;
#endif
}


Sampler* Logger::sampler() {
  return ticker_;
}


void Logger::EnsureTickerStarted() {
#ifdef ENABLE_LOGGING_AND_PROFILING
  ASSERT(ticker_ != NULL);
  if (!ticker_->IsActive()) ticker_->Start();
#endif
}


void Logger::EnsureTickerStopped() {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (ticker_ != NULL && ticker_->IsActive()) ticker_->Stop();
#endif
}


void Logger::TearDown() {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!is_initialized_) return;
  is_initialized_ = false;

  // Stop the profiler before closing the file.
  if (profiler_ != NULL) {
    profiler_->Disengage();
    delete profiler_;
    profiler_ = NULL;
  }

  delete sliding_state_window_;
  sliding_state_window_ = NULL;

  delete ticker_;
  ticker_ = NULL;

  log_->Close();
#endif
}


void Logger::EnableSlidingStateWindow() {
#ifdef ENABLE_LOGGING_AND_PROFILING
  // If the ticker is NULL, Logger::Setup has not been called yet.  In
  // that case, we set the sliding_state_window flag so that the
  // sliding window computation will be started when Logger::Setup is
  // called.
  if (ticker_ == NULL) {
    FLAG_sliding_state_window = true;
    return;
  }
  // Otherwise, if the sliding state window computation has not been
  // started we do it now.
  if (sliding_state_window_ == NULL) {
    sliding_state_window_ = new SlidingStateWindow(Isolate::Current());
  }
#endif
}


Mutex* SamplerRegistry::mutex_ = OS::CreateMutex();
List<Sampler*>* SamplerRegistry::active_samplers_ = NULL;


bool SamplerRegistry::IterateActiveSamplers(VisitSampler func, void* param) {
  ScopedLock lock(mutex_);
  for (int i = 0;
       ActiveSamplersExist() && i < active_samplers_->length();
       ++i) {
    func(active_samplers_->at(i), param);
  }
  return ActiveSamplersExist();
}


static void ComputeCpuProfiling(Sampler* sampler, void* flag_ptr) {
  bool* flag = reinterpret_cast<bool*>(flag_ptr);
  *flag |= sampler->IsProfiling();
}


SamplerRegistry::State SamplerRegistry::GetState() {
  bool flag = false;
  if (!IterateActiveSamplers(&ComputeCpuProfiling, &flag)) {
    return HAS_NO_SAMPLERS;
  }
  return flag ? HAS_CPU_PROFILING_SAMPLERS : HAS_SAMPLERS;
}


void SamplerRegistry::AddActiveSampler(Sampler* sampler) {
  ASSERT(sampler->IsActive());
  ScopedLock lock(mutex_);
  if (active_samplers_ == NULL) {
    active_samplers_ = new List<Sampler*>;
  } else {
    ASSERT(!active_samplers_->Contains(sampler));
  }
  active_samplers_->Add(sampler);
}


void SamplerRegistry::RemoveActiveSampler(Sampler* sampler) {
  ASSERT(sampler->IsActive());
  ScopedLock lock(mutex_);
  ASSERT(active_samplers_ != NULL);
  bool removed = active_samplers_->RemoveElement(sampler);
  ASSERT(removed);
  USE(removed);
}

} }  // namespace v8::internal
