// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/pgo.h"

#include "src/wasm/decoder.h"
#include "src/wasm/wasm-module-builder.h"  // For {ZoneBuffer}.

namespace v8::internal::wasm {

base::OwnedVector<uint8_t> GetProfileData(const WasmModule* module) {
  const TypeFeedbackStorage& type_feedback = module->type_feedback;
  AccountingAllocator allocator;
  Zone zone{&allocator, "wasm::GetProfileData"};
  ZoneBuffer buffer{&zone};
  base::MutexGuard mutex_guard{&type_feedback.mutex};

  // Get an ordered list of function indexes, so we generate deterministic data.
  std::vector<uint32_t> ordered_func_indexes;
  ordered_func_indexes.reserve(type_feedback.feedback_for_function.size());
  for (const auto& entry : type_feedback.feedback_for_function) {
    ordered_func_indexes.push_back(entry.first);
  }
  std::sort(ordered_func_indexes.begin(), ordered_func_indexes.end());

  buffer.write_u32v(static_cast<uint32_t>(ordered_func_indexes.size()));
  for (const uint32_t func_index : ordered_func_indexes) {
    buffer.write_u32v(func_index);
    // Serialize {feedback_vector}.
    const FunctionTypeFeedback& feedback =
        type_feedback.feedback_for_function.at(func_index);
    buffer.write_u32v(static_cast<uint32_t>(feedback.feedback_vector.size()));
    for (const CallSiteFeedback& call_site_feedback :
         feedback.feedback_vector) {
      int cases = call_site_feedback.num_cases();
      buffer.write_i32v(cases);
      for (int i = 0; i < cases; ++i) {
        buffer.write_i32v(call_site_feedback.function_index(i));
        buffer.write_i32v(call_site_feedback.call_count(i));
      }
    }
    // Serialize {call_targets}.
    buffer.write_u32v(static_cast<uint32_t>(feedback.call_targets.size()));
    for (uint32_t call_target : feedback.call_targets) {
      buffer.write_u32v(call_target);
    }
  }
  return base::OwnedVector<uint8_t>::Of(buffer);
}

void RestoreProfileData(WasmModule* module,
                        base::Vector<uint8_t> profile_data) {
  TypeFeedbackStorage& type_feedback = module->type_feedback;
  Decoder decoder{profile_data.begin(), profile_data.end()};
  uint32_t num_entries = decoder.consume_u32v("num function entries");
  CHECK_LE(num_entries, module->num_declared_functions);
  for (uint32_t missing_entries = num_entries; missing_entries > 0;
       --missing_entries) {
    uint32_t function_index = decoder.consume_u32v("function index");
    CHECK(!type_feedback.feedback_for_function.count(function_index));
    FunctionTypeFeedback& feedback =
        type_feedback.feedback_for_function[function_index];
    // Deserialize {feedback_vector}.
    uint32_t feedback_vector_size =
        decoder.consume_u32v("feedback vector size");
    feedback.feedback_vector.resize(feedback_vector_size);
    for (CallSiteFeedback& feedback : feedback.feedback_vector) {
      int num_cases = decoder.consume_i32v("num cases");
      if (num_cases == 0) continue;  // no feedback
      if (num_cases == 1) {          // monomorphic
        int called_function_index = decoder.consume_i32v("function index");
        int call_count = decoder.consume_i32v("call count");
        feedback = CallSiteFeedback{called_function_index, call_count};
      } else {  // polymorphic
        auto* polymorphic = new CallSiteFeedback::PolymorphicCase[num_cases];
        for (int i = 0; i < num_cases; ++i) {
          polymorphic[i].function_index =
              decoder.consume_i32v("function index");
          polymorphic[i].absolute_call_frequency =
              decoder.consume_i32v("call count");
        }
        feedback = CallSiteFeedback{polymorphic, num_cases};
      }
    }
    // Deserialize {call_targets}.
    uint32_t num_call_targets = decoder.consume_u32v("num call targets");
    feedback.call_targets =
        base::OwnedVector<uint32_t>::NewForOverwrite(num_call_targets);
    for (uint32_t& call_target : feedback.call_targets) {
      call_target = decoder.consume_u32v("call target");
    }
  }
  CHECK(decoder.ok());
  CHECK_EQ(decoder.pc(), decoder.end());
}

void DumpProfileToFile(const WasmModule* module,
                       base::Vector<const uint8_t> wire_bytes) {
  CHECK(!wire_bytes.empty());
  // File are named `profile-wasm-<hash>`.
  // We use the same hash as for reported scripts, to make it easier to
  // correlate files to wasm modules (see {CreateWasmScript}).
  uint32_t hash = static_cast<uint32_t>(GetWireBytesHash(wire_bytes));
  base::EmbeddedVector<char, 32> filename;
  SNPrintF(filename, "profile-wasm-%08x", hash);
  base::OwnedVector<uint8_t> profile_data = GetProfileData(module);
  PrintF("Dumping Wasm PGO data to file '%s' (%zu bytes)\n", filename.begin(),
         profile_data.size());
  if (FILE* file = base::OS::FOpen(filename.begin(), "wb")) {
    CHECK_EQ(profile_data.size(),
             fwrite(profile_data.begin(), 1, profile_data.size(), file));
    base::Fclose(file);
  }
}

void LoadProfileFromFile(WasmModule* module,
                         base::Vector<const uint8_t> wire_bytes) {
  CHECK(!wire_bytes.empty());
  // File are named `profile-wasm-<hash>`.
  // We use the same hash as for reported scripts, to make it easier to
  // correlate files to wasm modules (see {CreateWasmScript}).
  uint32_t hash = static_cast<uint32_t>(GetWireBytesHash(wire_bytes));
  base::EmbeddedVector<char, 32> filename;
  SNPrintF(filename, "profile-wasm-%08x", hash);

  FILE* file = base::OS::FOpen(filename.begin(), "rb");
  if (!file) {
    PrintF("No Wasm PGO data found: Cannot open file '%s'\n", filename.begin());
    return;
  }

  fseek(file, 0, SEEK_END);
  size_t size = ftell(file);
  rewind(file);

  PrintF("Loading Wasm PGO data from file '%s' (%zu bytes)\n", filename.begin(),
         size);
  base::OwnedVector<uint8_t> profile_data =
      base::OwnedVector<uint8_t>::NewForOverwrite(size);
  for (size_t read = 0; read < size;) {
    read += fread(profile_data.begin() + read, 1, size - read, file);
    CHECK(!ferror(file));
  }

  base::Fclose(file);

  RestoreProfileData(module, profile_data.as_vector());

  // Check that the generated profile is deterministic.
  DCHECK_EQ(profile_data.as_vector(), GetProfileData(module).as_vector());
}

}  // namespace v8::internal::wasm
