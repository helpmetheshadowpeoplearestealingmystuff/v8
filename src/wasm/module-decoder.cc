// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/module-decoder.h"

#include "src/base/functional.h"
#include "src/base/platform/platform.h"
#include "src/base/platform/wrappers.h"
#include "src/flags/flags.h"
#include "src/init/v8.h"
#include "src/logging/metrics.h"
#include "src/objects/objects-inl.h"
#include "src/wasm/constant-expression.h"
#include "src/wasm/decoder.h"
#include "src/wasm/module-decoder-impl.h"
#include "src/wasm/struct-types.h"
#include "src/wasm/wasm-constants.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-opcodes-inl.h"

namespace v8 {
namespace internal {
namespace wasm {

ModuleResult DecodeWasmModule(
    const WasmFeatures& enabled, const byte* module_start,
    const byte* module_end, bool verify_functions, ModuleOrigin origin,
    Counters* counters, std::shared_ptr<metrics::Recorder> metrics_recorder,
    v8::metrics::Recorder::ContextId context_id, DecodingMethod decoding_method,
    AccountingAllocator* allocator) {
  size_t size = module_end - module_start;
  CHECK_LE(module_start, module_end);
  size_t max_size = max_module_size();
  if (size > max_size) {
    return ModuleResult{
        WasmError{0, "size > maximum module size (%zu): %zu", max_size, size}};
  }
  // TODO(bradnelson): Improve histogram handling of size_t.
  auto size_counter =
      SELECT_WASM_COUNTER(counters, origin, wasm, module_size_bytes);
  size_counter->AddSample(static_cast<int>(size));
  // Signatures are stored in zone memory, which have the same lifetime
  // as the {module}.
  ModuleDecoderImpl decoder(enabled, module_start, module_end, origin);
  v8::metrics::WasmModuleDecoded metrics_event;
  base::ElapsedTimer timer;
  timer.Start();
  base::ThreadTicks thread_ticks = base::ThreadTicks::IsSupported()
                                       ? base::ThreadTicks::Now()
                                       : base::ThreadTicks();
  ModuleResult result =
      decoder.DecodeModule(counters, allocator, verify_functions);

  // Record event metrics.
  metrics_event.wall_clock_duration_in_us = timer.Elapsed().InMicroseconds();
  timer.Stop();
  if (!thread_ticks.IsNull()) {
    metrics_event.cpu_duration_in_us =
        (base::ThreadTicks::Now() - thread_ticks).InMicroseconds();
  }
  metrics_event.success = decoder.ok() && result.ok();
  metrics_event.async = decoding_method == DecodingMethod::kAsync ||
                        decoding_method == DecodingMethod::kAsyncStream;
  metrics_event.streamed = decoding_method == DecodingMethod::kSyncStream ||
                           decoding_method == DecodingMethod::kAsyncStream;
  if (result.ok()) {
    metrics_event.function_count = result.value()->num_declared_functions;
  } else if (auto&& module = decoder.shared_module()) {
    metrics_event.function_count = module->num_declared_functions;
  }
  metrics_event.module_size_in_bytes = size;
  metrics_recorder->DelayMainThreadEvent(metrics_event, context_id);

  return result;
}

ModuleResult DecodeWasmModuleForDisassembler(const byte* module_start,
                                             const byte* module_end,
                                             AccountingAllocator* allocator) {
  constexpr bool verify_functions = false;
  ModuleDecoderImpl decoder(WasmFeatures::All(), module_start, module_end,
                            kWasmOrigin);
  return decoder.DecodeModule(nullptr, allocator, verify_functions);
}

ModuleDecoder::ModuleDecoder(const WasmFeatures& enabled)
    : enabled_features_(enabled) {}

ModuleDecoder::~ModuleDecoder() = default;

const std::shared_ptr<WasmModule>& ModuleDecoder::shared_module() const {
  return impl_->shared_module();
}

void ModuleDecoder::StartDecoding(
    Counters* counters, std::shared_ptr<metrics::Recorder> metrics_recorder,
    v8::metrics::Recorder::ContextId context_id, AccountingAllocator* allocator,
    ModuleOrigin origin) {
  DCHECK_NULL(impl_);
  impl_.reset(new ModuleDecoderImpl(enabled_features_, origin));
  impl_->StartDecoding(counters, allocator);
}

void ModuleDecoder::DecodeModuleHeader(base::Vector<const uint8_t> bytes,
                                       uint32_t offset) {
  impl_->DecodeModuleHeader(bytes, offset);
}

void ModuleDecoder::DecodeSection(SectionCode section_code,
                                  base::Vector<const uint8_t> bytes,
                                  uint32_t offset, bool verify_functions) {
  impl_->DecodeSection(section_code, bytes, offset, verify_functions);
}

void ModuleDecoder::DecodeFunctionBody(uint32_t index, uint32_t length,
                                       uint32_t offset, bool verify_functions) {
  impl_->DecodeFunctionBody(index, length, offset, verify_functions);
}

void ModuleDecoder::StartCodeSection() { impl_->StartCodeSection(); }

bool ModuleDecoder::CheckFunctionsCount(uint32_t functions_count,
                                        uint32_t error_offset) {
  return impl_->CheckFunctionsCount(functions_count, error_offset);
}

ModuleResult ModuleDecoder::FinishDecoding(bool verify_functions) {
  return impl_->FinishDecoding(verify_functions);
}

void ModuleDecoder::set_code_section(uint32_t offset, uint32_t size) {
  return impl_->set_code_section(offset, size);
}

size_t ModuleDecoder::IdentifyUnknownSection(ModuleDecoder* decoder,
                                             base::Vector<const uint8_t> bytes,
                                             uint32_t offset,
                                             SectionCode* result) {
  if (!decoder->ok()) return 0;
  decoder->impl_->Reset(bytes, offset);
  *result = IdentifyUnknownSectionInternal(decoder->impl_.get());
  return decoder->impl_->pc() - bytes.begin();
}

bool ModuleDecoder::ok() { return impl_->ok(); }

const FunctionSig* DecodeWasmSignatureForTesting(const WasmFeatures& enabled,
                                                 Zone* zone, const byte* start,
                                                 const byte* end) {
  ModuleDecoderImpl decoder(enabled, start, end, kWasmOrigin);
  return decoder.DecodeFunctionSignature(zone, start);
}

ConstantExpression DecodeWasmInitExprForTesting(const WasmFeatures& enabled,
                                                const byte* start,
                                                const byte* end,
                                                ValueType expected) {
  ModuleDecoderImpl decoder(enabled, start, end, kWasmOrigin);
  AccountingAllocator allocator;
  decoder.StartDecoding(nullptr, &allocator);
  return decoder.DecodeInitExprForTesting(expected);
}

FunctionResult DecodeWasmFunctionForTesting(
    const WasmFeatures& enabled, Zone* zone, const ModuleWireBytes& wire_bytes,
    const WasmModule* module, const byte* function_start,
    const byte* function_end, Counters* counters) {
  size_t size = function_end - function_start;
  CHECK_LE(function_start, function_end);
  if (size > kV8MaxWasmFunctionSize) {
    return FunctionResult{WasmError{0,
                                    "size > maximum function size (%zu): %zu",
                                    kV8MaxWasmFunctionSize, size}};
  }
  ModuleDecoderImpl decoder(enabled, function_start, function_end, kWasmOrigin);
  decoder.SetCounters(counters);
  return decoder.DecodeSingleFunction(zone, wire_bytes, module);
}

AsmJsOffsetsResult DecodeAsmJsOffsets(
    base::Vector<const uint8_t> encoded_offsets) {
  std::vector<AsmJsOffsetFunctionEntries> functions;

  Decoder decoder(encoded_offsets);
  uint32_t functions_count = decoder.consume_u32v("functions count");
  // Consistency check.
  DCHECK_GE(encoded_offsets.size(), functions_count);
  functions.reserve(functions_count);

  for (uint32_t i = 0; i < functions_count; ++i) {
    uint32_t size = decoder.consume_u32v("table size");
    if (size == 0) {
      functions.emplace_back();
      continue;
    }
    DCHECK(decoder.checkAvailable(size));
    const byte* table_end = decoder.pc() + size;
    uint32_t locals_size = decoder.consume_u32v("locals size");
    int function_start_position = decoder.consume_u32v("function start pos");
    int function_end_position = function_start_position;
    int last_byte_offset = locals_size;
    int last_asm_position = function_start_position;
    std::vector<AsmJsOffsetEntry> func_asm_offsets;
    func_asm_offsets.reserve(size / 4);  // conservative estimation
    // Add an entry for the stack check, associated with position 0.
    func_asm_offsets.push_back(
        {0, function_start_position, function_start_position});
    while (decoder.pc() < table_end) {
      DCHECK(decoder.ok());
      last_byte_offset += decoder.consume_u32v("byte offset delta");
      int call_position =
          last_asm_position + decoder.consume_i32v("call position delta");
      int to_number_position =
          call_position + decoder.consume_i32v("to_number position delta");
      last_asm_position = to_number_position;
      if (decoder.pc() == table_end) {
        // The last entry is the function end marker.
        DCHECK_EQ(call_position, to_number_position);
        function_end_position = call_position;
      } else {
        func_asm_offsets.push_back(
            {last_byte_offset, call_position, to_number_position});
      }
    }
    DCHECK_EQ(decoder.pc(), table_end);
    functions.emplace_back(AsmJsOffsetFunctionEntries{
        function_start_position, function_end_position,
        std::move(func_asm_offsets)});
  }
  DCHECK(decoder.ok());
  DCHECK(!decoder.more());

  return decoder.toResult(AsmJsOffsets{std::move(functions)});
}

std::vector<CustomSectionOffset> DecodeCustomSections(const byte* start,
                                                      const byte* end) {
  Decoder decoder(start, end);
  decoder.consume_bytes(4, "wasm magic");
  decoder.consume_bytes(4, "wasm version");

  std::vector<CustomSectionOffset> result;

  while (decoder.more()) {
    byte section_code = decoder.consume_u8("section code");
    uint32_t section_length = decoder.consume_u32v("section length");
    uint32_t section_start = decoder.pc_offset();
    if (section_code != 0) {
      // Skip known sections.
      decoder.consume_bytes(section_length, "section bytes");
      continue;
    }
    uint32_t name_length = decoder.consume_u32v("name length");
    uint32_t name_offset = decoder.pc_offset();
    decoder.consume_bytes(name_length, "section name");
    uint32_t payload_offset = decoder.pc_offset();
    if (section_length < (payload_offset - section_start)) {
      decoder.error("invalid section length");
      break;
    }
    uint32_t payload_length = section_length - (payload_offset - section_start);
    decoder.consume_bytes(payload_length);
    if (decoder.failed()) break;
    result.push_back({{section_start, section_length},
                      {name_offset, name_length},
                      {payload_offset, payload_length}});
  }

  return result;
}

namespace {

bool FindNameSection(Decoder* decoder) {
  static constexpr int kModuleHeaderSize = 8;
  decoder->consume_bytes(kModuleHeaderSize, "module header");

  WasmSectionIterator section_iter(decoder);

  while (decoder->ok() && section_iter.more() &&
         section_iter.section_code() != kNameSectionCode) {
    section_iter.advance(true);
  }
  if (!section_iter.more()) return false;

  // Reset the decoder to not read beyond the name section end.
  decoder->Reset(section_iter.payload(), decoder->pc_offset());
  return true;
}

}  // namespace

void DecodeFunctionNames(const byte* module_start, const byte* module_end,
                         NameMap& names) {
  Decoder decoder(module_start, module_end);
  if (FindNameSection(&decoder)) {
    while (decoder.ok() && decoder.more()) {
      uint8_t name_type = decoder.consume_u8("name type");
      if (name_type & 0x80) break;  // no varuint7

      uint32_t name_payload_len = decoder.consume_u32v("name payload length");
      if (!decoder.checkAvailable(name_payload_len)) break;

      if (name_type != NameSectionKindCode::kFunctionCode) {
        decoder.consume_bytes(name_payload_len, "name subsection payload");
        continue;
      }
      uint32_t functions_count = decoder.consume_u32v("functions count");

      for (; decoder.ok() && functions_count > 0; --functions_count) {
        uint32_t function_index = decoder.consume_u32v("function index");
        WireBytesRef name =
            consume_string(&decoder, StringValidation::kNone, "function name");

        // Be lenient with errors in the name section: Ignore non-UTF8 names.
        // You can even assign to the same function multiple times (last valid
        // one wins).
        if (decoder.ok() && validate_utf8(&decoder, name)) {
          names.Put(function_index, name);
        }
      }
    }
  }
  names.FinishInitialization();
}

namespace {

void DecodeNameMap(NameMap& target, Decoder& decoder) {
  uint32_t count = decoder.consume_u32v("names count");
  for (uint32_t i = 0; i < count; i++) {
    uint32_t index = decoder.consume_u32v("index");
    WireBytesRef name =
        consume_string(&decoder, StringValidation::kNone, "name");
    if (!decoder.ok()) break;
    if (index > kMaxInt) continue;
    if (name.is_empty()) continue;  // Empty names are useless.
    if (!validate_utf8(&decoder, name)) continue;
    target.Put(index, name);
  }
  target.FinishInitialization();
}

void DecodeIndirectNameMap(IndirectNameMap& target, Decoder& decoder) {
  uint32_t outer_count = decoder.consume_u32v("outer count");
  for (uint32_t i = 0; i < outer_count; ++i) {
    uint32_t outer_index = decoder.consume_u32v("outer index");
    if (outer_index > kMaxInt) continue;
    NameMap names;
    uint32_t inner_count = decoder.consume_u32v("inner count");
    for (uint32_t k = 0; k < inner_count; ++k) {
      uint32_t inner_index = decoder.consume_u32v("inner index");
      WireBytesRef name =
          consume_string(&decoder, StringValidation::kNone, "name");
      if (!decoder.ok()) break;
      if (inner_index > kMaxInt) continue;
      if (name.is_empty()) continue;  // Empty names are useless.
      if (!validate_utf8(&decoder, name)) continue;
      names.Put(inner_index, name);
    }
    names.FinishInitialization();
    target.Put(outer_index, std::move(names));
  }
  target.FinishInitialization();
}

}  // namespace

DecodedNameSection::DecodedNameSection(base::Vector<const uint8_t> wire_bytes,
                                       WireBytesRef name_section) {
  if (name_section.is_empty()) return;  // No name section.
  Decoder decoder(wire_bytes.begin() + name_section.offset(),
                  wire_bytes.begin() + name_section.end_offset(),
                  name_section.offset());
  while (decoder.ok() && decoder.more()) {
    uint8_t name_type = decoder.consume_u8("name type");
    if (name_type & 0x80) break;  // no varuint7

    uint32_t name_payload_len = decoder.consume_u32v("name payload length");
    if (!decoder.checkAvailable(name_payload_len)) break;

    switch (name_type) {
      case kModuleCode:
      case kFunctionCode:
        // Already handled elsewhere.
        decoder.consume_bytes(name_payload_len);
        break;
      case kLocalCode:
        DecodeIndirectNameMap(local_names_, decoder);
        break;
      case kLabelCode:
        DecodeIndirectNameMap(label_names_, decoder);
        break;
      case kTypeCode:
        DecodeNameMap(type_names_, decoder);
        break;
      case kTableCode:
        DecodeNameMap(table_names_, decoder);
        break;
      case kMemoryCode:
        DecodeNameMap(memory_names_, decoder);
        break;
      case kGlobalCode:
        DecodeNameMap(global_names_, decoder);
        break;
      case kElementSegmentCode:
        DecodeNameMap(element_segment_names_, decoder);
        break;
      case kDataSegmentCode:
        DecodeNameMap(data_segment_names_, decoder);
        break;
      case kFieldCode:
        DecodeIndirectNameMap(field_names_, decoder);
        break;
      case kTagCode:
        DecodeNameMap(tag_names_, decoder);
        break;
    }
  }
}

#undef TRACE

}  // namespace wasm
}  // namespace internal
}  // namespace v8
