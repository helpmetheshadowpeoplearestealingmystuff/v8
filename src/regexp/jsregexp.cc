// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/regexp/jsregexp.h"

#include "src/codegen/compilation-cache.h"
#include "src/heap/heap-inl.h"
#include "src/regexp/jsregexp-inl.h"
#include "src/regexp/regexp-compiler.h"
#include "src/regexp/regexp-dotprinter.h"
#include "src/regexp/regexp-interpreter.h"
#include "src/regexp/regexp-macro-assembler-arch.h"
#include "src/regexp/regexp-macro-assembler-irregexp.h"
#include "src/regexp/regexp-parser.h"
#include "src/strings/string-search.h"

namespace v8 {
namespace internal {

using namespace regexp_compiler_constants;  // NOLINT(build/namespaces)

V8_WARN_UNUSED_RESULT
static inline MaybeHandle<Object> ThrowRegExpException(
    Isolate* isolate, Handle<JSRegExp> re, Handle<String> pattern,
    Handle<String> error_text) {
  THROW_NEW_ERROR(isolate, NewSyntaxError(MessageTemplate::kMalformedRegExp,
                                          pattern, error_text),
                  Object);
}

inline void ThrowRegExpException(Isolate* isolate, Handle<JSRegExp> re,
                                 Handle<String> error_text) {
  USE(ThrowRegExpException(isolate, re, Handle<String>(re->Pattern(), isolate),
                           error_text));
}


ContainedInLattice AddRange(ContainedInLattice containment,
                            const int* ranges,
                            int ranges_length,
                            Interval new_range) {
  DCHECK_EQ(1, ranges_length & 1);
  DCHECK_EQ(String::kMaxCodePoint + 1, ranges[ranges_length - 1]);
  if (containment == kLatticeUnknown) return containment;
  bool inside = false;
  int last = 0;
  for (int i = 0; i < ranges_length; inside = !inside, last = ranges[i], i++) {
    // Consider the range from last to ranges[i].
    // We haven't got to the new range yet.
    if (ranges[i] <= new_range.from()) continue;
    // New range is wholly inside last-ranges[i].  Note that new_range.to() is
    // inclusive, but the values in ranges are not.
    if (last <= new_range.from() && new_range.to() < ranges[i]) {
      return Combine(containment, inside ? kLatticeIn : kLatticeOut);
    }
    return kLatticeUnknown;
  }
  return containment;
}

// Identifies the sort of regexps where the regexp engine is faster
// than the code used for atom matches.
static bool HasFewDifferentCharacters(Handle<String> pattern) {
  int length = Min(kMaxLookaheadForBoyerMoore, pattern->length());
  if (length <= kPatternTooShortForBoyerMoore) return false;
  const int kMod = 128;
  bool character_found[kMod];
  int different = 0;
  memset(&character_found[0], 0, sizeof(character_found));
  for (int i = 0; i < length; i++) {
    int ch = (pattern->Get(i) & (kMod - 1));
    if (!character_found[ch]) {
      character_found[ch] = true;
      different++;
      // We declare a regexp low-alphabet if it has at least 3 times as many
      // characters as it has different characters.
      if (different * 3 > length) return false;
    }
  }
  return true;
}

// Generic RegExp methods. Dispatches to implementation specific methods.

MaybeHandle<Object> RegExpImpl::Compile(Isolate* isolate, Handle<JSRegExp> re,
                                        Handle<String> pattern,
                                        JSRegExp::Flags flags) {
  DCHECK(pattern->IsFlat());

  Zone zone(isolate->allocator(), ZONE_NAME);
  CompilationCache* compilation_cache = isolate->compilation_cache();
  MaybeHandle<FixedArray> maybe_cached =
      compilation_cache->LookupRegExp(pattern, flags);
  Handle<FixedArray> cached;
  if (maybe_cached.ToHandle(&cached)) {
    re->set_data(*cached);
    return re;
  }

  PostponeInterruptsScope postpone(isolate);
  RegExpCompileData parse_result;
  FlatStringReader reader(isolate, pattern);
  DCHECK(!isolate->has_pending_exception());
  if (!RegExpParser::ParseRegExp(isolate, &zone, &reader, flags,
                                 &parse_result)) {
    // Throw an exception if we fail to parse the pattern.
    return ThrowRegExpException(isolate, re, pattern, parse_result.error);
  }

  bool has_been_compiled = false;

  if (parse_result.simple && !IgnoreCase(flags) && !IsSticky(flags) &&
      !HasFewDifferentCharacters(pattern)) {
    // Parse-tree is a single atom that is equal to the pattern.
    AtomCompile(isolate, re, pattern, flags, pattern);
    has_been_compiled = true;
  } else if (parse_result.tree->IsAtom() && !IsSticky(flags) &&
             parse_result.capture_count == 0) {
    RegExpAtom* atom = parse_result.tree->AsAtom();
    Vector<const uc16> atom_pattern = atom->data();
    Handle<String> atom_string;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, atom_string,
        isolate->factory()->NewStringFromTwoByte(atom_pattern), Object);
    if (!IgnoreCase(atom->flags()) && !HasFewDifferentCharacters(atom_string)) {
      AtomCompile(isolate, re, pattern, flags, atom_string);
      has_been_compiled = true;
    }
  }
  if (!has_been_compiled) {
    IrregexpInitialize(isolate, re, pattern, flags, parse_result.capture_count);
  }
  DCHECK(re->data().IsFixedArray());
  // Compilation succeeded so the data is set on the regexp
  // and we can store it in the cache.
  Handle<FixedArray> data(FixedArray::cast(re->data()), isolate);
  compilation_cache->PutRegExp(pattern, flags, data);

  return re;
}

MaybeHandle<Object> RegExpImpl::Exec(Isolate* isolate, Handle<JSRegExp> regexp,
                                     Handle<String> subject, int index,
                                     Handle<RegExpMatchInfo> last_match_info) {
  switch (regexp->TypeTag()) {
    case JSRegExp::ATOM:
      return AtomExec(isolate, regexp, subject, index, last_match_info);
    case JSRegExp::IRREGEXP: {
      return IrregexpExec(isolate, regexp, subject, index, last_match_info);
    }
    default:
      UNREACHABLE();
  }
}


// RegExp Atom implementation: Simple string search using indexOf.

void RegExpImpl::AtomCompile(Isolate* isolate, Handle<JSRegExp> re,
                             Handle<String> pattern, JSRegExp::Flags flags,
                             Handle<String> match_pattern) {
  isolate->factory()->SetRegExpAtomData(re, JSRegExp::ATOM, pattern, flags,
                                        match_pattern);
}

static void SetAtomLastCapture(Isolate* isolate,
                               Handle<RegExpMatchInfo> last_match_info,
                               String subject, int from, int to) {
  SealHandleScope shs(isolate);
  last_match_info->SetNumberOfCaptureRegisters(2);
  last_match_info->SetLastSubject(subject);
  last_match_info->SetLastInput(subject);
  last_match_info->SetCapture(0, from);
  last_match_info->SetCapture(1, to);
}

int RegExpImpl::AtomExecRaw(Isolate* isolate, Handle<JSRegExp> regexp,
                            Handle<String> subject, int index, int32_t* output,
                            int output_size) {
  DCHECK_LE(0, index);
  DCHECK_LE(index, subject->length());

  subject = String::Flatten(isolate, subject);
  DisallowHeapAllocation no_gc;  // ensure vectors stay valid

  String needle = String::cast(regexp->DataAt(JSRegExp::kAtomPatternIndex));
  int needle_len = needle.length();
  DCHECK(needle.IsFlat());
  DCHECK_LT(0, needle_len);

  if (index + needle_len > subject->length()) {
    return RegExpImpl::RE_FAILURE;
  }

  for (int i = 0; i < output_size; i += 2) {
    String::FlatContent needle_content = needle.GetFlatContent(no_gc);
    String::FlatContent subject_content = subject->GetFlatContent(no_gc);
    DCHECK(needle_content.IsFlat());
    DCHECK(subject_content.IsFlat());
    // dispatch on type of strings
    index =
        (needle_content.IsOneByte()
             ? (subject_content.IsOneByte()
                    ? SearchString(isolate, subject_content.ToOneByteVector(),
                                   needle_content.ToOneByteVector(), index)
                    : SearchString(isolate, subject_content.ToUC16Vector(),
                                   needle_content.ToOneByteVector(), index))
             : (subject_content.IsOneByte()
                    ? SearchString(isolate, subject_content.ToOneByteVector(),
                                   needle_content.ToUC16Vector(), index)
                    : SearchString(isolate, subject_content.ToUC16Vector(),
                                   needle_content.ToUC16Vector(), index)));
    if (index == -1) {
      return i / 2;  // Return number of matches.
    } else {
      output[i] = index;
      output[i+1] = index + needle_len;
      index += needle_len;
    }
  }
  return output_size / 2;
}

Handle<Object> RegExpImpl::AtomExec(Isolate* isolate, Handle<JSRegExp> re,
                                    Handle<String> subject, int index,
                                    Handle<RegExpMatchInfo> last_match_info) {
  static const int kNumRegisters = 2;
  STATIC_ASSERT(kNumRegisters <= Isolate::kJSRegexpStaticOffsetsVectorSize);
  int32_t* output_registers = isolate->jsregexp_static_offsets_vector();

  int res =
      AtomExecRaw(isolate, re, subject, index, output_registers, kNumRegisters);

  if (res == RegExpImpl::RE_FAILURE) return isolate->factory()->null_value();

  DCHECK_EQ(res, RegExpImpl::RE_SUCCESS);
  SealHandleScope shs(isolate);
  SetAtomLastCapture(isolate, last_match_info, *subject, output_registers[0],
                     output_registers[1]);
  return last_match_info;
}


// Irregexp implementation.

// Ensures that the regexp object contains a compiled version of the
// source for either one-byte or two-byte subject strings.
// If the compiled version doesn't already exist, it is compiled
// from the source pattern.
// If compilation fails, an exception is thrown and this function
// returns false.
bool RegExpImpl::EnsureCompiledIrregexp(Isolate* isolate, Handle<JSRegExp> re,
                                        Handle<String> sample_subject,
                                        bool is_one_byte) {
  Object compiled_code = re->DataAt(JSRegExp::code_index(is_one_byte));
  if (compiled_code != Smi::FromInt(JSRegExp::kUninitializedValue)) {
    DCHECK(FLAG_regexp_interpret_all ? compiled_code.IsByteArray()
                                     : compiled_code.IsCode());
    return true;
  }
  return CompileIrregexp(isolate, re, sample_subject, is_one_byte);
}

bool RegExpImpl::CompileIrregexp(Isolate* isolate, Handle<JSRegExp> re,
                                 Handle<String> sample_subject,
                                 bool is_one_byte) {
  // Compile the RegExp.
  Zone zone(isolate->allocator(), ZONE_NAME);
  PostponeInterruptsScope postpone(isolate);
#ifdef DEBUG
  Object entry = re->DataAt(JSRegExp::code_index(is_one_byte));
  // When arriving here entry can only be a smi representing an uncompiled
  // regexp.
  DCHECK(entry.IsSmi());
  int entry_value = Smi::ToInt(entry);
  DCHECK_EQ(JSRegExp::kUninitializedValue, entry_value);
#endif

  JSRegExp::Flags flags = re->GetFlags();

  Handle<String> pattern(re->Pattern(), isolate);
  pattern = String::Flatten(isolate, pattern);
  RegExpCompileData compile_data;
  FlatStringReader reader(isolate, pattern);
  if (!RegExpParser::ParseRegExp(isolate, &zone, &reader, flags,
                                 &compile_data)) {
    // Throw an exception if we fail to parse the pattern.
    // THIS SHOULD NOT HAPPEN. We already pre-parsed it successfully once.
    USE(ThrowRegExpException(isolate, re, pattern, compile_data.error));
    return false;
  }
  RegExpEngine::CompilationResult result =
      RegExpEngine::Compile(isolate, &zone, &compile_data, flags, pattern,
                            sample_subject, is_one_byte);
  if (result.error_message != nullptr) {
    // Unable to compile regexp.
    if (FLAG_correctness_fuzzer_suppressions &&
        strncmp(result.error_message, "Stack overflow", 15) == 0) {
      FATAL("Aborting on stack overflow");
    }
    Handle<String> error_message = isolate->factory()->NewStringFromUtf8(
        CStrVector(result.error_message)).ToHandleChecked();
    ThrowRegExpException(isolate, re, error_message);
    return false;
  }

  Handle<FixedArray> data =
      Handle<FixedArray>(FixedArray::cast(re->data()), isolate);
  data->set(JSRegExp::code_index(is_one_byte), result.code);
  SetIrregexpCaptureNameMap(*data, compile_data.capture_name_map);
  int register_max = IrregexpMaxRegisterCount(*data);
  if (result.num_registers > register_max) {
    SetIrregexpMaxRegisterCount(*data, result.num_registers);
  }

  return true;
}

int RegExpImpl::IrregexpMaxRegisterCount(FixedArray re) {
  return Smi::cast(re.get(JSRegExp::kIrregexpMaxRegisterCountIndex)).value();
}

void RegExpImpl::SetIrregexpMaxRegisterCount(FixedArray re, int value) {
  re.set(JSRegExp::kIrregexpMaxRegisterCountIndex, Smi::FromInt(value));
}

void RegExpImpl::SetIrregexpCaptureNameMap(FixedArray re,
                                           Handle<FixedArray> value) {
  if (value.is_null()) {
    re.set(JSRegExp::kIrregexpCaptureNameMapIndex, Smi::kZero);
  } else {
    re.set(JSRegExp::kIrregexpCaptureNameMapIndex, *value);
  }
}

int RegExpImpl::IrregexpNumberOfCaptures(FixedArray re) {
  return Smi::ToInt(re.get(JSRegExp::kIrregexpCaptureCountIndex));
}

int RegExpImpl::IrregexpNumberOfRegisters(FixedArray re) {
  return Smi::ToInt(re.get(JSRegExp::kIrregexpMaxRegisterCountIndex));
}

ByteArray RegExpImpl::IrregexpByteCode(FixedArray re, bool is_one_byte) {
  return ByteArray::cast(re.get(JSRegExp::code_index(is_one_byte)));
}

Code RegExpImpl::IrregexpNativeCode(FixedArray re, bool is_one_byte) {
  return Code::cast(re.get(JSRegExp::code_index(is_one_byte)));
}

void RegExpImpl::IrregexpInitialize(Isolate* isolate, Handle<JSRegExp> re,
                                    Handle<String> pattern,
                                    JSRegExp::Flags flags, int capture_count) {
  // Initialize compiled code entries to null.
  isolate->factory()->SetRegExpIrregexpData(re, JSRegExp::IRREGEXP, pattern,
                                            flags, capture_count);
}

int RegExpImpl::IrregexpPrepare(Isolate* isolate, Handle<JSRegExp> regexp,
                                Handle<String> subject) {
  DCHECK(subject->IsFlat());

  // Check representation of the underlying storage.
  bool is_one_byte = String::IsOneByteRepresentationUnderneath(*subject);
  if (!EnsureCompiledIrregexp(isolate, regexp, subject, is_one_byte)) return -1;

  if (FLAG_regexp_interpret_all) {
    // Byte-code regexp needs space allocated for all its registers.
    // The result captures are copied to the start of the registers array
    // if the match succeeds.  This way those registers are not clobbered
    // when we set the last match info from last successful match.
    return IrregexpNumberOfRegisters(FixedArray::cast(regexp->data())) +
           (IrregexpNumberOfCaptures(FixedArray::cast(regexp->data())) + 1) * 2;
  } else {
    // Native regexp only needs room to output captures. Registers are handled
    // internally.
    return (IrregexpNumberOfCaptures(FixedArray::cast(regexp->data())) + 1) * 2;
  }
}

int RegExpImpl::IrregexpExecRaw(Isolate* isolate, Handle<JSRegExp> regexp,
                                Handle<String> subject, int index,
                                int32_t* output, int output_size) {
  Handle<FixedArray> irregexp(FixedArray::cast(regexp->data()), isolate);

  DCHECK_LE(0, index);
  DCHECK_LE(index, subject->length());
  DCHECK(subject->IsFlat());

  bool is_one_byte = String::IsOneByteRepresentationUnderneath(*subject);

  if (!FLAG_regexp_interpret_all) {
    DCHECK(output_size >= (IrregexpNumberOfCaptures(*irregexp) + 1) * 2);
    do {
      EnsureCompiledIrregexp(isolate, regexp, subject, is_one_byte);
      Handle<Code> code(IrregexpNativeCode(*irregexp, is_one_byte), isolate);
      // The stack is used to allocate registers for the compiled regexp code.
      // This means that in case of failure, the output registers array is left
      // untouched and contains the capture results from the previous successful
      // match.  We can use that to set the last match info lazily.
      int res = NativeRegExpMacroAssembler::Match(code, subject, output,
                                                  output_size, index, isolate);
      if (res != NativeRegExpMacroAssembler::RETRY) {
        DCHECK(res != NativeRegExpMacroAssembler::EXCEPTION ||
               isolate->has_pending_exception());
        STATIC_ASSERT(static_cast<int>(NativeRegExpMacroAssembler::SUCCESS) ==
                      RE_SUCCESS);
        STATIC_ASSERT(static_cast<int>(NativeRegExpMacroAssembler::FAILURE) ==
                      RE_FAILURE);
        STATIC_ASSERT(static_cast<int>(NativeRegExpMacroAssembler::EXCEPTION) ==
                      RE_EXCEPTION);
        return res;
      }
      // If result is RETRY, the string has changed representation, and we
      // must restart from scratch.
      // In this case, it means we must make sure we are prepared to handle
      // the, potentially, different subject (the string can switch between
      // being internal and external, and even between being Latin1 and UC16,
      // but the characters are always the same).
      IrregexpPrepare(isolate, regexp, subject);
      is_one_byte = String::IsOneByteRepresentationUnderneath(*subject);
    } while (true);
    UNREACHABLE();
  } else {
    DCHECK(FLAG_regexp_interpret_all);
    DCHECK(output_size >= IrregexpNumberOfRegisters(*irregexp));
    // We must have done EnsureCompiledIrregexp, so we can get the number of
    // registers.
    int number_of_capture_registers =
        (IrregexpNumberOfCaptures(*irregexp) + 1) * 2;
    int32_t* raw_output = &output[number_of_capture_registers];

    do {
      // We do not touch the actual capture result registers until we know there
      // has been a match so that we can use those capture results to set the
      // last match info.
      for (int i = number_of_capture_registers - 1; i >= 0; i--) {
        raw_output[i] = -1;
      }
      Handle<ByteArray> byte_codes(IrregexpByteCode(*irregexp, is_one_byte),
                                   isolate);

      IrregexpInterpreter::Result result = IrregexpInterpreter::Match(
          isolate, byte_codes, subject, raw_output, index);
      DCHECK_IMPLIES(result == IrregexpInterpreter::EXCEPTION,
                     isolate->has_pending_exception());

      switch (result) {
        case IrregexpInterpreter::SUCCESS:
          // Copy capture results to the start of the registers array.
          MemCopy(output, raw_output,
                  number_of_capture_registers * sizeof(int32_t));
          return result;
        case IrregexpInterpreter::EXCEPTION:
        case IrregexpInterpreter::FAILURE:
          return result;
        case IrregexpInterpreter::RETRY:
          // The string has changed representation, and we must restart the
          // match.
          is_one_byte = String::IsOneByteRepresentationUnderneath(*subject);
          EnsureCompiledIrregexp(isolate, regexp, subject, is_one_byte);
          break;
      }
    } while (true);
    UNREACHABLE();
  }
}

MaybeHandle<Object> RegExpImpl::IrregexpExec(
    Isolate* isolate, Handle<JSRegExp> regexp, Handle<String> subject,
    int previous_index, Handle<RegExpMatchInfo> last_match_info) {
  DCHECK_EQ(regexp->TypeTag(), JSRegExp::IRREGEXP);

  subject = String::Flatten(isolate, subject);

  // Prepare space for the return values.
#ifdef DEBUG
  if (FLAG_regexp_interpret_all && FLAG_trace_regexp_bytecodes) {
    String pattern = regexp->Pattern();
    PrintF("\n\nRegexp match:   /%s/\n\n", pattern.ToCString().get());
    PrintF("\n\nSubject string: '%s'\n\n", subject->ToCString().get());
  }
#endif
  int required_registers =
      RegExpImpl::IrregexpPrepare(isolate, regexp, subject);
  if (required_registers < 0) {
    // Compiling failed with an exception.
    DCHECK(isolate->has_pending_exception());
    return MaybeHandle<Object>();
  }

  int32_t* output_registers = nullptr;
  if (required_registers > Isolate::kJSRegexpStaticOffsetsVectorSize) {
    output_registers = NewArray<int32_t>(required_registers);
  }
  std::unique_ptr<int32_t[]> auto_release(output_registers);
  if (output_registers == nullptr) {
    output_registers = isolate->jsregexp_static_offsets_vector();
  }

  int res =
      RegExpImpl::IrregexpExecRaw(isolate, regexp, subject, previous_index,
                                  output_registers, required_registers);
  if (res == RE_SUCCESS) {
    int capture_count =
        IrregexpNumberOfCaptures(FixedArray::cast(regexp->data()));
    return SetLastMatchInfo(isolate, last_match_info, subject, capture_count,
                            output_registers);
  }
  if (res == RE_EXCEPTION) {
    DCHECK(isolate->has_pending_exception());
    return MaybeHandle<Object>();
  }
  DCHECK(res == RE_FAILURE);
  return isolate->factory()->null_value();
}

Handle<RegExpMatchInfo> RegExpImpl::SetLastMatchInfo(
    Isolate* isolate, Handle<RegExpMatchInfo> last_match_info,
    Handle<String> subject, int capture_count, int32_t* match) {
  // This is the only place where match infos can grow. If, after executing the
  // regexp, RegExpExecStub finds that the match info is too small, it restarts
  // execution in RegExpImpl::Exec, which finally grows the match info right
  // here.

  int capture_register_count = (capture_count + 1) * 2;
  Handle<RegExpMatchInfo> result = RegExpMatchInfo::ReserveCaptures(
      isolate, last_match_info, capture_register_count);
  result->SetNumberOfCaptureRegisters(capture_register_count);

  if (*result != *last_match_info) {
    if (*last_match_info == *isolate->regexp_last_match_info()) {
      // This inner condition is only needed for special situations like the
      // regexp fuzzer, where we pass our own custom RegExpMatchInfo to
      // RegExpImpl::Exec; there actually want to bypass the Isolate's match
      // info and execute the regexp without side effects.
      isolate->native_context()->set_regexp_last_match_info(*result);
    }
  }

  DisallowHeapAllocation no_allocation;
  if (match != nullptr) {
    for (int i = 0; i < capture_register_count; i += 2) {
      result->SetCapture(i, match[i]);
      result->SetCapture(i + 1, match[i + 1]);
    }
  }
  result->SetLastSubject(*subject);
  result->SetLastInput(*subject);
  return result;
}

RegExpImpl::GlobalCache::GlobalCache(Handle<JSRegExp> regexp,
                                     Handle<String> subject, Isolate* isolate)
    : register_array_(nullptr),
      register_array_size_(0),
      regexp_(regexp),
      subject_(subject),
      isolate_(isolate) {
  bool interpreted = FLAG_regexp_interpret_all;

  if (regexp_->TypeTag() == JSRegExp::ATOM) {
    static const int kAtomRegistersPerMatch = 2;
    registers_per_match_ = kAtomRegistersPerMatch;
    // There is no distinction between interpreted and native for atom regexps.
    interpreted = false;
  } else {
    registers_per_match_ =
        RegExpImpl::IrregexpPrepare(isolate_, regexp_, subject_);
    if (registers_per_match_ < 0) {
      num_matches_ = -1;  // Signal exception.
      return;
    }
  }

  DCHECK(IsGlobal(regexp->GetFlags()));
  if (!interpreted) {
    register_array_size_ =
        Max(registers_per_match_, Isolate::kJSRegexpStaticOffsetsVectorSize);
    max_matches_ = register_array_size_ / registers_per_match_;
  } else {
    // Global loop in interpreted regexp is not implemented.  We choose
    // the size of the offsets vector so that it can only store one match.
    register_array_size_ = registers_per_match_;
    max_matches_ = 1;
  }

  if (register_array_size_ > Isolate::kJSRegexpStaticOffsetsVectorSize) {
    register_array_ = NewArray<int32_t>(register_array_size_);
  } else {
    register_array_ = isolate->jsregexp_static_offsets_vector();
  }

  // Set state so that fetching the results the first time triggers a call
  // to the compiled regexp.
  current_match_index_ = max_matches_ - 1;
  num_matches_ = max_matches_;
  DCHECK_LE(2, registers_per_match_);  // Each match has at least one capture.
  DCHECK_GE(register_array_size_, registers_per_match_);
  int32_t* last_match =
      &register_array_[current_match_index_ * registers_per_match_];
  last_match[0] = -1;
  last_match[1] = 0;
}

int RegExpImpl::GlobalCache::AdvanceZeroLength(int last_index) {
  if (IsUnicode(regexp_->GetFlags()) && last_index + 1 < subject_->length() &&
      unibrow::Utf16::IsLeadSurrogate(subject_->Get(last_index)) &&
      unibrow::Utf16::IsTrailSurrogate(subject_->Get(last_index + 1))) {
    // Advance over the surrogate pair.
    return last_index + 2;
  }
  return last_index + 1;
}

void RegExpEngine::DotPrint(const char* label, RegExpNode* node,
                            bool ignore_case) {
  DotPrinter::DotPrint(label, node, ignore_case);
}

RegExpEngine::CompilationResult RegExpEngine::Compile(
    Isolate* isolate, Zone* zone, RegExpCompileData* data,
    JSRegExp::Flags flags, Handle<String> pattern,
    Handle<String> sample_subject, bool is_one_byte) {
  if ((data->capture_count + 1) * 2 - 1 > RegExpMacroAssembler::kMaxRegister) {
    return CompilationResult::RegExpTooBig();
  }
  bool is_sticky = IsSticky(flags);
  bool is_global = IsGlobal(flags);
  bool is_unicode = IsUnicode(flags);
  RegExpCompiler compiler(isolate, zone, data->capture_count, is_one_byte);

  if (compiler.optimize())
    compiler.set_optimize(!TooMuchRegExpCode(isolate, pattern));

  // Sample some characters from the middle of the string.
  static const int kSampleSize = 128;

  sample_subject = String::Flatten(isolate, sample_subject);
  int chars_sampled = 0;
  int half_way = (sample_subject->length() - kSampleSize) / 2;
  for (int i = Max(0, half_way);
       i < sample_subject->length() && chars_sampled < kSampleSize;
       i++, chars_sampled++) {
    compiler.frequency_collator()->CountCharacter(sample_subject->Get(i));
  }

  // Wrap the body of the regexp in capture #0.
  RegExpNode* captured_body =
      RegExpCapture::ToNode(data->tree, 0, &compiler, compiler.accept());
  RegExpNode* node = captured_body;
  bool is_end_anchored = data->tree->IsAnchoredAtEnd();
  bool is_start_anchored = data->tree->IsAnchoredAtStart();
  int max_length = data->tree->max_match();
  if (!is_start_anchored && !is_sticky) {
    // Add a .*? at the beginning, outside the body capture, unless
    // this expression is anchored at the beginning or sticky.
    JSRegExp::Flags default_flags = JSRegExp::Flags();
    RegExpNode* loop_node = RegExpQuantifier::ToNode(
        0, RegExpTree::kInfinity, false,
        new (zone) RegExpCharacterClass('*', default_flags), &compiler,
        captured_body, data->contains_anchor);

    if (data->contains_anchor) {
      // Unroll loop once, to take care of the case that might start
      // at the start of input.
      ChoiceNode* first_step_node = new (zone) ChoiceNode(2, zone);
      first_step_node->AddAlternative(GuardedAlternative(captured_body));
      first_step_node->AddAlternative(GuardedAlternative(new (zone) TextNode(
          new (zone) RegExpCharacterClass('*', default_flags), false,
          loop_node)));
      node = first_step_node;
    } else {
      node = loop_node;
    }
  }
  if (is_one_byte) {
    node = node->FilterOneByte(RegExpCompiler::kMaxRecursion);
    // Do it again to propagate the new nodes to places where they were not
    // put because they had not been calculated yet.
    if (node != nullptr) {
      node = node->FilterOneByte(RegExpCompiler::kMaxRecursion);
    }
  } else if (is_unicode && (is_global || is_sticky)) {
    node = RegExpCompiler::OptionallyStepBackToLeadSurrogate(&compiler, node,
                                                             flags);
  }

  if (node == nullptr) node = new (zone) EndNode(EndNode::BACKTRACK, zone);
  data->node = node;
  Analysis analysis(isolate, is_one_byte);
  analysis.EnsureAnalyzed(node);
  if (analysis.has_failed()) {
    return CompilationResult(analysis.error_message());
  }

  // Create the correct assembler for the architecture.
  std::unique_ptr<RegExpMacroAssembler> macro_assembler;
  if (!FLAG_regexp_interpret_all) {
    // Native regexp implementation.
    DCHECK(!FLAG_jitless);

    NativeRegExpMacroAssembler::Mode mode =
        is_one_byte ? NativeRegExpMacroAssembler::LATIN1
                    : NativeRegExpMacroAssembler::UC16;

#if V8_TARGET_ARCH_IA32
    macro_assembler.reset(new RegExpMacroAssemblerIA32(
        isolate, zone, mode, (data->capture_count + 1) * 2));
#elif V8_TARGET_ARCH_X64
    macro_assembler.reset(new RegExpMacroAssemblerX64(
        isolate, zone, mode, (data->capture_count + 1) * 2));
#elif V8_TARGET_ARCH_ARM
    macro_assembler.reset(new RegExpMacroAssemblerARM(
        isolate, zone, mode, (data->capture_count + 1) * 2));
#elif V8_TARGET_ARCH_ARM64
    macro_assembler.reset(new RegExpMacroAssemblerARM64(
        isolate, zone, mode, (data->capture_count + 1) * 2));
#elif V8_TARGET_ARCH_S390
    macro_assembler.reset(new RegExpMacroAssemblerS390(
        isolate, zone, mode, (data->capture_count + 1) * 2));
#elif V8_TARGET_ARCH_PPC
    macro_assembler.reset(new RegExpMacroAssemblerPPC(
        isolate, zone, mode, (data->capture_count + 1) * 2));
#elif V8_TARGET_ARCH_MIPS
    macro_assembler.reset(new RegExpMacroAssemblerMIPS(
        isolate, zone, mode, (data->capture_count + 1) * 2));
#elif V8_TARGET_ARCH_MIPS64
    macro_assembler.reset(new RegExpMacroAssemblerMIPS(
        isolate, zone, mode, (data->capture_count + 1) * 2));
#else
#error "Unsupported architecture"
#endif
  } else {
    DCHECK(FLAG_regexp_interpret_all);

    // Interpreted regexp implementation.
    macro_assembler.reset(new RegExpMacroAssemblerIrregexp(isolate, zone));
  }

  macro_assembler->set_slow_safe(TooMuchRegExpCode(isolate, pattern));

  // Inserted here, instead of in Assembler, because it depends on information
  // in the AST that isn't replicated in the Node structure.
  static const int kMaxBacksearchLimit = 1024;
  if (is_end_anchored && !is_start_anchored && !is_sticky &&
      max_length < kMaxBacksearchLimit) {
    macro_assembler->SetCurrentPositionFromEnd(max_length);
  }

  if (is_global) {
    RegExpMacroAssembler::GlobalMode mode = RegExpMacroAssembler::GLOBAL;
    if (data->tree->min_match() > 0) {
      mode = RegExpMacroAssembler::GLOBAL_NO_ZERO_LENGTH_CHECK;
    } else if (is_unicode) {
      mode = RegExpMacroAssembler::GLOBAL_UNICODE;
    }
    macro_assembler->set_global_mode(mode);
  }

  RegExpCompiler::CompilationResult result = compiler.Assemble(
      isolate, macro_assembler.get(), node, data->capture_count, pattern);

  return RegExpEngine::CompilationResult(result.error_message, result.code,
                                         result.num_registers);
}

bool RegExpEngine::TooMuchRegExpCode(Isolate* isolate, Handle<String> pattern) {
  Heap* heap = isolate->heap();
  bool too_much = pattern->length() > RegExpImpl::kRegExpTooLargeToOptimize;
  if (isolate->total_regexp_code_generated() >
          RegExpImpl::kRegExpCompiledLimit &&
      heap->CommittedMemoryExecutable() >
          RegExpImpl::kRegExpExecutableMemoryLimit) {
    too_much = true;
  }
  return too_much;
}

Object RegExpResultsCache::Lookup(Heap* heap, String key_string,
                                  Object key_pattern,
                                  FixedArray* last_match_cache,
                                  ResultsCacheType type) {
  FixedArray cache;
  if (!key_string.IsInternalizedString()) return Smi::kZero;
  if (type == STRING_SPLIT_SUBSTRINGS) {
    DCHECK(key_pattern.IsString());
    if (!key_pattern.IsInternalizedString()) return Smi::kZero;
    cache = heap->string_split_cache();
  } else {
    DCHECK(type == REGEXP_MULTIPLE_INDICES);
    DCHECK(key_pattern.IsFixedArray());
    cache = heap->regexp_multiple_cache();
  }

  uint32_t hash = key_string.Hash();
  uint32_t index = ((hash & (kRegExpResultsCacheSize - 1)) &
                    ~(kArrayEntriesPerCacheEntry - 1));
  if (cache.get(index + kStringOffset) != key_string ||
      cache.get(index + kPatternOffset) != key_pattern) {
    index =
        ((index + kArrayEntriesPerCacheEntry) & (kRegExpResultsCacheSize - 1));
    if (cache.get(index + kStringOffset) != key_string ||
        cache.get(index + kPatternOffset) != key_pattern) {
      return Smi::kZero;
    }
  }

  *last_match_cache = FixedArray::cast(cache.get(index + kLastMatchOffset));
  return cache.get(index + kArrayOffset);
}

void RegExpResultsCache::Enter(Isolate* isolate, Handle<String> key_string,
                               Handle<Object> key_pattern,
                               Handle<FixedArray> value_array,
                               Handle<FixedArray> last_match_cache,
                               ResultsCacheType type) {
  Factory* factory = isolate->factory();
  Handle<FixedArray> cache;
  if (!key_string->IsInternalizedString()) return;
  if (type == STRING_SPLIT_SUBSTRINGS) {
    DCHECK(key_pattern->IsString());
    if (!key_pattern->IsInternalizedString()) return;
    cache = factory->string_split_cache();
  } else {
    DCHECK(type == REGEXP_MULTIPLE_INDICES);
    DCHECK(key_pattern->IsFixedArray());
    cache = factory->regexp_multiple_cache();
  }

  uint32_t hash = key_string->Hash();
  uint32_t index = ((hash & (kRegExpResultsCacheSize - 1)) &
                    ~(kArrayEntriesPerCacheEntry - 1));
  if (cache->get(index + kStringOffset) == Smi::kZero) {
    cache->set(index + kStringOffset, *key_string);
    cache->set(index + kPatternOffset, *key_pattern);
    cache->set(index + kArrayOffset, *value_array);
    cache->set(index + kLastMatchOffset, *last_match_cache);
  } else {
    uint32_t index2 =
        ((index + kArrayEntriesPerCacheEntry) & (kRegExpResultsCacheSize - 1));
    if (cache->get(index2 + kStringOffset) == Smi::kZero) {
      cache->set(index2 + kStringOffset, *key_string);
      cache->set(index2 + kPatternOffset, *key_pattern);
      cache->set(index2 + kArrayOffset, *value_array);
      cache->set(index2 + kLastMatchOffset, *last_match_cache);
    } else {
      cache->set(index2 + kStringOffset, Smi::kZero);
      cache->set(index2 + kPatternOffset, Smi::kZero);
      cache->set(index2 + kArrayOffset, Smi::kZero);
      cache->set(index2 + kLastMatchOffset, Smi::kZero);
      cache->set(index + kStringOffset, *key_string);
      cache->set(index + kPatternOffset, *key_pattern);
      cache->set(index + kArrayOffset, *value_array);
      cache->set(index + kLastMatchOffset, *last_match_cache);
    }
  }
  // If the array is a reasonably short list of substrings, convert it into a
  // list of internalized strings.
  if (type == STRING_SPLIT_SUBSTRINGS && value_array->length() < 100) {
    for (int i = 0; i < value_array->length(); i++) {
      Handle<String> str(String::cast(value_array->get(i)), isolate);
      Handle<String> internalized_str = factory->InternalizeString(str);
      value_array->set(i, *internalized_str);
    }
  }
  // Convert backing store to a copy-on-write array.
  value_array->set_map_no_write_barrier(
      ReadOnlyRoots(isolate).fixed_cow_array_map());
}

void RegExpResultsCache::Clear(FixedArray cache) {
  for (int i = 0; i < kRegExpResultsCacheSize; i++) {
    cache.set(i, Smi::kZero);
  }
}

}  // namespace internal
}  // namespace v8
