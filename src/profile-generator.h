// Copyright 2011 the V8 project authors. All rights reserved.
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

#ifndef V8_PROFILE_GENERATOR_H_
#define V8_PROFILE_GENERATOR_H_

#include "allocation.h"
#include "hashmap.h"
#include "../include/v8-profiler.h"

namespace v8 {
namespace internal {

struct OffsetRange;

// Provides a storage of strings allocated in C++ heap, to hold them
// forever, even if they disappear from JS heap or external storage.
class StringsStorage {
 public:
  StringsStorage();
  ~StringsStorage();

  const char* GetCopy(const char* src);
  const char* GetFormatted(const char* format, ...);
  const char* GetVFormatted(const char* format, va_list args);
  const char* GetName(Name* name);
  const char* GetName(int index);
  inline const char* GetFunctionName(Name* name);
  inline const char* GetFunctionName(const char* name);
  size_t GetUsedMemorySize() const;

 private:
  static const int kMaxNameSize = 1024;

  INLINE(static bool StringsMatch(void* key1, void* key2)) {
    return strcmp(reinterpret_cast<char*>(key1),
                  reinterpret_cast<char*>(key2)) == 0;
  }
  const char* AddOrDisposeString(char* str, uint32_t hash);

  // Mapping of strings by String::Hash to const char* strings.
  HashMap names_;

  DISALLOW_COPY_AND_ASSIGN(StringsStorage);
};


class CodeEntry {
 public:
  // CodeEntry doesn't own name strings, just references them.
  INLINE(CodeEntry(Logger::LogEventsAndTags tag,
                   const char* name,
                   const char* name_prefix = CodeEntry::kEmptyNamePrefix,
                   const char* resource_name = CodeEntry::kEmptyResourceName,
                   int line_number = v8::CpuProfileNode::kNoLineNumberInfo));
  ~CodeEntry();

  INLINE(bool is_js_function() const) { return is_js_function_tag(tag_); }
  INLINE(const char* name_prefix() const) { return name_prefix_; }
  INLINE(bool has_name_prefix() const) { return name_prefix_[0] != '\0'; }
  INLINE(const char* name() const) { return name_; }
  INLINE(const char* resource_name() const) { return resource_name_; }
  INLINE(int line_number() const) { return line_number_; }
  INLINE(void set_shared_id(int shared_id)) { shared_id_ = shared_id; }
  INLINE(int script_id() const) { return script_id_; }
  INLINE(void set_script_id(int script_id)) { script_id_ = script_id; }

  INLINE(static bool is_js_function_tag(Logger::LogEventsAndTags tag));

  List<OffsetRange>* no_frame_ranges() const { return no_frame_ranges_; }
  void set_no_frame_ranges(List<OffsetRange>* ranges) {
    no_frame_ranges_ = ranges;
  }

  void SetBuiltinId(Builtins::Name id);
  Builtins::Name builtin_id() const { return builtin_id_; }

  void CopyData(const CodeEntry& source);
  uint32_t GetCallUid() const;
  bool IsSameAs(CodeEntry* entry) const;

  static const char* const kEmptyNamePrefix;
  static const char* const kEmptyResourceName;

 private:
  Logger::LogEventsAndTags tag_ : 8;
  Builtins::Name builtin_id_ : 8;
  const char* name_prefix_;
  const char* name_;
  const char* resource_name_;
  int line_number_;
  int shared_id_;
  int script_id_;
  List<OffsetRange>* no_frame_ranges_;

  DISALLOW_COPY_AND_ASSIGN(CodeEntry);
};


class ProfileTree;

class ProfileNode {
 public:
  INLINE(ProfileNode(ProfileTree* tree, CodeEntry* entry));

  ProfileNode* FindChild(CodeEntry* entry);
  ProfileNode* FindOrAddChild(CodeEntry* entry);
  INLINE(void IncrementSelfTicks()) { ++self_ticks_; }
  INLINE(void IncreaseSelfTicks(unsigned amount)) { self_ticks_ += amount; }
  INLINE(void IncreaseTotalTicks(unsigned amount)) { total_ticks_ += amount; }

  INLINE(CodeEntry* entry() const) { return entry_; }
  INLINE(unsigned self_ticks() const) { return self_ticks_; }
  INLINE(unsigned total_ticks() const) { return total_ticks_; }
  INLINE(const List<ProfileNode*>* children() const) { return &children_list_; }
  double GetSelfMillis() const;
  double GetTotalMillis() const;
  unsigned id() const { return id_; }

  void Print(int indent);

 private:
  INLINE(static bool CodeEntriesMatch(void* entry1, void* entry2)) {
    return reinterpret_cast<CodeEntry*>(entry1)->IsSameAs(
        reinterpret_cast<CodeEntry*>(entry2));
  }

  INLINE(static uint32_t CodeEntryHash(CodeEntry* entry)) {
    return entry->GetCallUid();
  }

  ProfileTree* tree_;
  CodeEntry* entry_;
  unsigned total_ticks_;
  unsigned self_ticks_;
  // Mapping from CodeEntry* to ProfileNode*
  HashMap children_;
  List<ProfileNode*> children_list_;
  unsigned id_;

  DISALLOW_COPY_AND_ASSIGN(ProfileNode);
};


class ProfileTree {
 public:
  ProfileTree();
  ~ProfileTree();

  ProfileNode* AddPathFromEnd(const Vector<CodeEntry*>& path);
  void AddPathFromStart(const Vector<CodeEntry*>& path);
  void CalculateTotalTicks();

  double TicksToMillis(unsigned ticks) const {
    return ticks * ms_to_ticks_scale_;
  }
  ProfileNode* root() const { return root_; }
  void SetTickRatePerMs(double ticks_per_ms);

  unsigned next_node_id() { return next_node_id_++; }

  void ShortPrint();
  void Print() {
    root_->Print(0);
  }

 private:
  template <typename Callback>
  void TraverseDepthFirst(Callback* callback);

  CodeEntry root_entry_;
  unsigned next_node_id_;
  ProfileNode* root_;
  double ms_to_ticks_scale_;

  DISALLOW_COPY_AND_ASSIGN(ProfileTree);
};


class CpuProfile {
 public:
  CpuProfile(const char* title, unsigned uid, bool record_samples);

  // Add pc -> ... -> main() call path to the profile.
  void AddPath(const Vector<CodeEntry*>& path);
  void CalculateTotalTicksAndSamplingRate();

  const char* title() const { return title_; }
  unsigned uid() const { return uid_; }
  const ProfileTree* top_down() const { return &top_down_; }

  int samples_count() const { return samples_.length(); }
  ProfileNode* sample(int index) const { return samples_.at(index); }

  int64_t start_time_us() const { return start_time_us_; }
  int64_t end_time_us() const { return end_time_us_; }

  void UpdateTicksScale();

  void ShortPrint();
  void Print();

 private:
  const char* title_;
  unsigned uid_;
  bool record_samples_;
  int64_t start_time_us_;
  int64_t end_time_us_;
  List<ProfileNode*> samples_;
  ProfileTree top_down_;

  DISALLOW_COPY_AND_ASSIGN(CpuProfile);
};


class CodeMap {
 public:
  CodeMap() : next_shared_id_(1) { }
  void AddCode(Address addr, CodeEntry* entry, unsigned size);
  void MoveCode(Address from, Address to);
  CodeEntry* FindEntry(Address addr, Address* start = NULL);
  int GetSharedId(Address addr);

  void Print();

 private:
  struct CodeEntryInfo {
    CodeEntryInfo(CodeEntry* an_entry, unsigned a_size)
        : entry(an_entry), size(a_size) { }
    CodeEntry* entry;
    unsigned size;
  };

  struct CodeTreeConfig {
    typedef Address Key;
    typedef CodeEntryInfo Value;
    static const Key kNoKey;
    static const Value NoValue() { return CodeEntryInfo(NULL, 0); }
    static int Compare(const Key& a, const Key& b) {
      return a < b ? -1 : (a > b ? 1 : 0);
    }
  };
  typedef SplayTree<CodeTreeConfig> CodeTree;

  class CodeTreePrinter {
   public:
    void Call(const Address& key, const CodeEntryInfo& value);
  };

  void DeleteAllCoveredCode(Address start, Address end);

  // Fake CodeEntry pointer to distinguish shared function entries.
  static CodeEntry* const kSharedFunctionCodeEntry;

  CodeTree tree_;
  int next_shared_id_;

  DISALLOW_COPY_AND_ASSIGN(CodeMap);
};


class CpuProfilesCollection {
 public:
  CpuProfilesCollection();
  ~CpuProfilesCollection();

  bool StartProfiling(const char* title, unsigned uid, bool record_samples);
  CpuProfile* StopProfiling(const char* title);
  List<CpuProfile*>* profiles() { return &finished_profiles_; }
  const char* GetName(Name* name) {
    return function_and_resource_names_.GetName(name);
  }
  const char* GetName(int args_count) {
    return function_and_resource_names_.GetName(args_count);
  }
  const char* GetFunctionName(Name* name) {
    return function_and_resource_names_.GetFunctionName(name);
  }
  const char* GetFunctionName(const char* name) {
    return function_and_resource_names_.GetFunctionName(name);
  }
  bool IsLastProfile(const char* title);
  void RemoveProfile(CpuProfile* profile);

  CodeEntry* NewCodeEntry(
      Logger::LogEventsAndTags tag,
      const char* name,
      const char* name_prefix = CodeEntry::kEmptyNamePrefix,
      const char* resource_name = CodeEntry::kEmptyResourceName,
      int line_number = v8::CpuProfileNode::kNoLineNumberInfo);

  // Called from profile generator thread.
  void AddPathToCurrentProfiles(const Vector<CodeEntry*>& path);

  // Limits the number of profiles that can be simultaneously collected.
  static const int kMaxSimultaneousProfiles = 100;

 private:
  StringsStorage function_and_resource_names_;
  List<CodeEntry*> code_entries_;
  List<CpuProfile*> finished_profiles_;

  // Accessed by VM thread and profile generator thread.
  List<CpuProfile*> current_profiles_;
  Semaphore* current_profiles_semaphore_;

  DISALLOW_COPY_AND_ASSIGN(CpuProfilesCollection);
};


class ProfileGenerator {
 public:
  explicit ProfileGenerator(CpuProfilesCollection* profiles);

  void RecordTickSample(const TickSample& sample);

  INLINE(CodeMap* code_map()) { return &code_map_; }

  static const char* const kAnonymousFunctionName;
  static const char* const kProgramEntryName;
  static const char* const kGarbageCollectorEntryName;
  // Used to represent frames for which we have no reliable way to
  // detect function.
  static const char* const kUnresolvedFunctionName;

 private:
  INLINE(CodeEntry* EntryForVMState(StateTag tag));

  CpuProfilesCollection* profiles_;
  CodeMap code_map_;
  CodeEntry* program_entry_;
  CodeEntry* gc_entry_;
  CodeEntry* unresolved_entry_;

  DISALLOW_COPY_AND_ASSIGN(ProfileGenerator);
};


} }  // namespace v8::internal

#endif  // V8_PROFILE_GENERATOR_H_
