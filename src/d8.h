// Copyright 2012 the V8 project authors. All rights reserved.
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

#ifndef V8_D8_H_
#define V8_D8_H_

#ifndef V8_SHARED
#include "allocation.h"
#include "hashmap.h"
#include "smart-pointers.h"
#include "v8.h"
#else
#include "../include/v8.h"
#endif  // V8_SHARED

namespace v8 {


#ifndef V8_SHARED
// A single counter in a counter collection.
class Counter {
 public:
  static const int kMaxNameSize = 64;
  int32_t* Bind(const char* name, bool histogram);
  int32_t* ptr() { return &count_; }
  int32_t count() { return count_; }
  int32_t sample_total() { return sample_total_; }
  bool is_histogram() { return is_histogram_; }
  void AddSample(int32_t sample);
 private:
  int32_t count_;
  int32_t sample_total_;
  bool is_histogram_;
  uint8_t name_[kMaxNameSize];
};


// A set of counters and associated information.  An instance of this
// class is stored directly in the memory-mapped counters file if
// the --map-counters options is used
class CounterCollection {
 public:
  CounterCollection();
  Counter* GetNextCounter();
 private:
  static const unsigned kMaxCounters = 512;
  uint32_t magic_number_;
  uint32_t max_counters_;
  uint32_t max_name_size_;
  uint32_t counters_in_use_;
  Counter counters_[kMaxCounters];
};


class CounterMap {
 public:
  CounterMap(): hash_map_(Match) { }
  Counter* Lookup(const char* name) {
    i::HashMap::Entry* answer = hash_map_.Lookup(
        const_cast<char*>(name),
        Hash(name),
        false);
    if (!answer) return NULL;
    return reinterpret_cast<Counter*>(answer->value);
  }
  void Set(const char* name, Counter* value) {
    i::HashMap::Entry* answer = hash_map_.Lookup(
        const_cast<char*>(name),
        Hash(name),
        true);
    ASSERT(answer != NULL);
    answer->value = value;
  }
  class Iterator {
   public:
    explicit Iterator(CounterMap* map)
        : map_(&map->hash_map_), entry_(map_->Start()) { }
    void Next() { entry_ = map_->Next(entry_); }
    bool More() { return entry_ != NULL; }
    const char* CurrentKey() { return static_cast<const char*>(entry_->key); }
    Counter* CurrentValue() { return static_cast<Counter*>(entry_->value); }
   private:
    i::HashMap* map_;
    i::HashMap::Entry* entry_;
  };

 private:
  static int Hash(const char* name);
  static bool Match(void* key1, void* key2);
  i::HashMap hash_map_;
};
#endif  // V8_SHARED


class LineEditor {
 public:
  enum Type { DUMB = 0, READLINE = 1 };
  LineEditor(Type type, const char* name);
  virtual ~LineEditor() { }

  virtual Handle<String> Prompt(const char* prompt) = 0;
  virtual bool Open(Isolate* isolate) { return true; }
  virtual bool Close() { return true; }
  virtual void AddHistory(const char* str) { }

  const char* name() { return name_; }
  static LineEditor* Get() { return current_; }
 private:
  Type type_;
  const char* name_;
  static LineEditor* current_;
};


class SourceGroup {
 public:
  SourceGroup() :
#ifndef V8_SHARED
      next_semaphore_(v8::internal::OS::CreateSemaphore(0)),
      done_semaphore_(v8::internal::OS::CreateSemaphore(0)),
      thread_(NULL),
#endif  // V8_SHARED
      argv_(NULL),
      begin_offset_(0),
      end_offset_(0) {}

  ~SourceGroup();

  void Begin(char** argv, int offset) {
    argv_ = const_cast<const char**>(argv);
    begin_offset_ = offset;
  }

  void End(int offset) { end_offset_ = offset; }

  void Execute(Isolate* isolate);

#ifndef V8_SHARED
  void StartExecuteInThread();
  void WaitForThread();

 private:
  class IsolateThread : public i::Thread {
   public:
    explicit IsolateThread(SourceGroup* group)
        : i::Thread(GetThreadOptions()), group_(group) {}

    virtual void Run() {
      group_->ExecuteInThread();
    }

   private:
    SourceGroup* group_;
  };

  static i::Thread::Options GetThreadOptions();
  void ExecuteInThread();

  i::Semaphore* next_semaphore_;
  i::Semaphore* done_semaphore_;
  i::Thread* thread_;
#endif  // V8_SHARED

  void ExitShell(int exit_code);
  Handle<String> ReadFile(Isolate* isolate, const char* name);

  const char** argv_;
  int begin_offset_;
  int end_offset_;
};


class BinaryResource : public v8::String::ExternalAsciiStringResource {
 public:
  BinaryResource(const char* string, int length)
      : data_(string),
        length_(length) { }

  ~BinaryResource() {
    delete[] data_;
    data_ = NULL;
    length_ = 0;
  }

  virtual const char* data() const { return data_; }
  virtual size_t length() const { return length_; }

 private:
  const char* data_;
  size_t length_;
};


class ShellOptions {
 public:
  ShellOptions() :
#ifndef V8_SHARED
     use_preemption(true),
     preemption_interval(10),
     num_parallel_files(0),
     parallel_files(NULL),
#endif  // V8_SHARED
     script_executed(false),
     last_run(true),
     send_idle_notification(false),
     stress_opt(false),
     stress_deopt(false),
     interactive_shell(false),
     test_shell(false),
     num_isolates(1),
     isolate_sources(NULL) { }

  ~ShellOptions() {
#ifndef V8_SHARED
    delete[] parallel_files;
#endif  // V8_SHARED
    delete[] isolate_sources;
  }

#ifndef V8_SHARED
  bool use_preemption;
  int preemption_interval;
  int num_parallel_files;
  char** parallel_files;
#endif  // V8_SHARED
  bool script_executed;
  bool last_run;
  bool send_idle_notification;
  bool stress_opt;
  bool stress_deopt;
  bool interactive_shell;
  bool test_shell;
  int num_isolates;
  SourceGroup* isolate_sources;
};

#ifdef V8_SHARED
class Shell {
#else
class Shell : public i::AllStatic {
#endif  // V8_SHARED

 public:
  static bool ExecuteString(Isolate* isolate,
                            Handle<String> source,
                            Handle<Value> name,
                            bool print_result,
                            bool report_exceptions);
  static const char* ToCString(const v8::String::Utf8Value& value);
  static void ReportException(Isolate* isolate, TryCatch* try_catch);
  static Handle<String> ReadFile(Isolate* isolate, const char* name);
  static Persistent<Context> CreateEvaluationContext(Isolate* isolate);
  static int RunMain(Isolate* isolate, int argc, char* argv[]);
  static int Main(int argc, char* argv[]);
  static void Exit(int exit_code);
  static void OnExit();

#ifndef V8_SHARED
  static Handle<Array> GetCompletions(Isolate* isolate,
                                      Handle<String> text,
                                      Handle<String> full);
  static int* LookupCounter(const char* name);
  static void* CreateHistogram(const char* name,
                               int min,
                               int max,
                               size_t buckets);
  static void AddHistogramSample(void* histogram, int sample);
  static void MapCounters(const char* name);

#ifdef ENABLE_DEBUGGER_SUPPORT
  static Handle<Object> DebugMessageDetails(Handle<String> message);
  static Handle<Value> DebugCommandToJSONRequest(Handle<String> command);
  static void DispatchDebugMessages();
#endif  // ENABLE_DEBUGGER_SUPPORT
#endif  // V8_SHARED

  static Handle<Value> RealmCurrent(const Arguments& args);
  static Handle<Value> RealmOwner(const Arguments& args);
  static Handle<Value> RealmGlobal(const Arguments& args);
  static Handle<Value> RealmCreate(const Arguments& args);
  static Handle<Value> RealmDispose(const Arguments& args);
  static Handle<Value> RealmSwitch(const Arguments& args);
  static Handle<Value> RealmEval(const Arguments& args);
  static Handle<Value> RealmSharedGet(Local<String> property,
                                      const AccessorInfo& info);
  static void RealmSharedSet(Local<String> property,
                             Local<Value> value,
                             const AccessorInfo& info);

  static Handle<Value> Print(const Arguments& args);
  static Handle<Value> Write(const Arguments& args);
  static Handle<Value> Quit(const Arguments& args);
  static Handle<Value> Version(const Arguments& args);
  static Handle<Value> EnableProfiler(const Arguments& args);
  static Handle<Value> DisableProfiler(const Arguments& args);
  static Handle<Value> Read(const Arguments& args);
  static Handle<Value> ReadBuffer(const Arguments& args);
  static Handle<String> ReadFromStdin(Isolate* isolate);
  static Handle<Value> ReadLine(const Arguments& args) {
    return ReadFromStdin(args.GetIsolate());
  }
  static Handle<Value> Load(const Arguments& args);
  static Handle<Value> ArrayBuffer(const Arguments& args);
  static Handle<Value> Int8Array(const Arguments& args);
  static Handle<Value> Uint8Array(const Arguments& args);
  static Handle<Value> Int16Array(const Arguments& args);
  static Handle<Value> Uint16Array(const Arguments& args);
  static Handle<Value> Int32Array(const Arguments& args);
  static Handle<Value> Uint32Array(const Arguments& args);
  static Handle<Value> Float32Array(const Arguments& args);
  static Handle<Value> Float64Array(const Arguments& args);
  static Handle<Value> Uint8ClampedArray(const Arguments& args);
  static Handle<Value> ArrayBufferSlice(const Arguments& args);
  static Handle<Value> ArraySubArray(const Arguments& args);
  static Handle<Value> ArraySet(const Arguments& args);
  // The OS object on the global object contains methods for performing
  // operating system calls:
  //
  // os.system("program_name", ["arg1", "arg2", ...], timeout1, timeout2) will
  // run the command, passing the arguments to the program.  The standard output
  // of the program will be picked up and returned as a multiline string.  If
  // timeout1 is present then it should be a number.  -1 indicates no timeout
  // and a positive number is used as a timeout in milliseconds that limits the
  // time spent waiting between receiving output characters from the program.
  // timeout2, if present, should be a number indicating the limit in
  // milliseconds on the total running time of the program.  Exceptions are
  // thrown on timeouts or other errors or if the exit status of the program
  // indicates an error.
  //
  // os.chdir(dir) changes directory to the given directory.  Throws an
  // exception/ on error.
  //
  // os.setenv(variable, value) sets an environment variable.  Repeated calls to
  // this method leak memory due to the API of setenv in the standard C library.
  //
  // os.umask(alue) calls the umask system call and returns the old umask.
  //
  // os.mkdirp(name, mask) creates a directory.  The mask (if present) is anded
  // with the current umask.  Intermediate directories are created if necessary.
  // An exception is not thrown if the directory already exists.  Analogous to
  // the "mkdir -p" command.
  static Handle<Value> OSObject(const Arguments& args);
  static Handle<Value> System(const Arguments& args);
  static Handle<Value> ChangeDirectory(const Arguments& args);
  static Handle<Value> SetEnvironment(const Arguments& args);
  static Handle<Value> UnsetEnvironment(const Arguments& args);
  static Handle<Value> SetUMask(const Arguments& args);
  static Handle<Value> MakeDirectory(const Arguments& args);
  static Handle<Value> RemoveDirectory(const Arguments& args);

  static void AddOSMethods(Handle<ObjectTemplate> os_template);

  static const char* kPrompt;
  static ShellOptions options;

 private:
  static Persistent<Context> evaluation_context_;
#ifndef V8_SHARED
  static Persistent<Context> utility_context_;
  static CounterMap* counter_map_;
  // We statically allocate a set of local counters to be used if we
  // don't want to store the stats in a memory-mapped file
  static CounterCollection local_counters_;
  static CounterCollection* counters_;
  static i::OS::MemoryMappedFile* counters_file_;
  static i::Mutex* context_mutex_;

  static Counter* GetCounter(const char* name, bool is_histogram);
  static void InstallUtilityScript(Isolate* isolate);
#endif  // V8_SHARED
  static void Initialize(Isolate* isolate);
  static void InitializeDebugger(Isolate* isolate);
  static void RunShell(Isolate* isolate);
  static bool SetOptions(int argc, char* argv[]);
  static Handle<ObjectTemplate> CreateGlobalTemplate(Isolate* isolate);
  static Handle<FunctionTemplate> CreateArrayBufferTemplate(InvocationCallback);
  static Handle<FunctionTemplate> CreateArrayTemplate(InvocationCallback);
  static Handle<Value> CreateExternalArrayBuffer(Isolate* isolate,
                                                 Handle<Object> buffer,
                                                 int32_t size);
  static Handle<Object> CreateExternalArray(Isolate* isolate,
                                            Handle<Object> array,
                                            Handle<Object> buffer,
                                            ExternalArrayType type,
                                            int32_t length,
                                            int32_t byteLength,
                                            int32_t byteOffset,
                                            int32_t element_size);
  static Handle<Value> CreateExternalArray(const Arguments& args,
                                           ExternalArrayType type,
                                           int32_t element_size);
  static void ExternalArrayWeakCallback(Isolate* isolate,
                                        Persistent<Value> object,
                                        void* data);
};


}  // namespace v8


#endif  // V8_D8_H_
