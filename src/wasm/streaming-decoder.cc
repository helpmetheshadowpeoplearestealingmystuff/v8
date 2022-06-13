// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/streaming-decoder.h"

#include "src/base/platform/wrappers.h"
#include "src/handles/handles.h"
#include "src/objects/descriptor-array.h"
#include "src/objects/dictionary.h"
#include "src/objects/objects-inl.h"
#include "src/wasm/decoder.h"
#include "src/wasm/leb-helper.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-result.h"

#define TRACE_STREAMING(...)                            \
  do {                                                  \
    if (FLAG_trace_wasm_streaming) PrintF(__VA_ARGS__); \
  } while (false)

namespace v8 {
namespace internal {
namespace wasm {

class V8_EXPORT_PRIVATE AsyncStreamingDecoder : public StreamingDecoder {
 public:
  explicit AsyncStreamingDecoder(std::unique_ptr<StreamingProcessor> processor);
  AsyncStreamingDecoder(const AsyncStreamingDecoder&) = delete;
  AsyncStreamingDecoder& operator=(const AsyncStreamingDecoder&) = delete;

  // The buffer passed into OnBytesReceived is owned by the caller.
  void OnBytesReceived(base::Vector<const uint8_t> bytes) override;

  void Finish(bool can_use_compiled_module) override;

  void Abort() override;

  // Notify the StreamingDecoder that compilation ended and the
  // StreamingProcessor should not be called anymore.
  void NotifyCompilationEnded() override { Fail(); }

  void NotifyNativeModuleCreated(
      const std::shared_ptr<NativeModule>& native_module) override;

 private:
  // The SectionBuffer is the data object for the content of a single section.
  // It stores all bytes of the section (including section id and section
  // length), and the offset where the actual payload starts.
  class SectionBuffer : public WireBytesStorage {
   public:
    // id: The section id.
    // payload_length: The length of the payload.
    // length_bytes: The section length, as it is encoded in the module bytes.
    SectionBuffer(uint32_t module_offset, uint8_t id, size_t payload_length,
                  base::Vector<const uint8_t> length_bytes)
        :  // ID + length + payload
          module_offset_(module_offset),
          bytes_(base::OwnedVector<uint8_t>::NewForOverwrite(
              1 + length_bytes.length() + payload_length)),
          payload_offset_(1 + length_bytes.length()) {
      bytes_.start()[0] = id;
      memcpy(bytes_.start() + 1, &length_bytes.first(), length_bytes.length());
    }

    SectionCode section_code() const {
      return static_cast<SectionCode>(bytes_.start()[0]);
    }

    base::Vector<const uint8_t> GetCode(WireBytesRef ref) const final {
      DCHECK_LE(module_offset_, ref.offset());
      uint32_t offset_in_code_buffer = ref.offset() - module_offset_;
      return bytes().SubVector(offset_in_code_buffer,
                               offset_in_code_buffer + ref.length());
    }

    base::Optional<ModuleWireBytes> GetModuleBytes() const final { return {}; }

    uint32_t module_offset() const { return module_offset_; }
    base::Vector<uint8_t> bytes() const { return bytes_.as_vector(); }
    base::Vector<uint8_t> payload() const { return bytes() + payload_offset_; }
    size_t length() const { return bytes_.size(); }
    size_t payload_offset() const { return payload_offset_; }

   private:
    const uint32_t module_offset_;
    const base::OwnedVector<uint8_t> bytes_;
    const size_t payload_offset_;
  };

  // The decoding of a stream of wasm module bytes is organized in states. Each
  // state provides a buffer to store the bytes required for the current state,
  // information on how many bytes have already been received, how many bytes
  // are needed, and a {Next} function which starts the next state once all
  // bytes of the current state were received.
  //
  // The states change according to the following state diagram:
  //
  //       Start
  //         |
  //         |
  //         v
  // DecodeModuleHeader
  //         |   _________________________________________
  //         |   |                                        |
  //         v   v                                        |
  //  DecodeSectionID --> DecodeSectionLength --> DecodeSectionPayload
  //         A                  |
  //         |                  | (if the section id == code)
  //         |                  v
  //         |      DecodeNumberOfFunctions -- > DecodeFunctionLength
  //         |                                          A    |
  //         |                                          |    |
  //         |  (after all functions were read)         |    v
  //         ------------------------------------- DecodeFunctionBody
  //
  class DecodingState {
   public:
    virtual ~DecodingState() = default;

    // Reads the bytes for the current state and returns the number of read
    // bytes.
    virtual size_t ReadBytes(AsyncStreamingDecoder* streaming,
                             base::Vector<const uint8_t> bytes);

    // Returns the next state of the streaming decoding.
    virtual std::unique_ptr<DecodingState> Next(
        AsyncStreamingDecoder* streaming) = 0;
    // The buffer to store the received bytes.
    virtual base::Vector<uint8_t> buffer() = 0;
    // The number of bytes which were already received.
    size_t offset() const { return offset_; }
    void set_offset(size_t value) { offset_ = value; }
    // A flag to indicate if finishing the streaming decoder is allowed without
    // error.
    virtual bool is_finishing_allowed() const { return false; }

   private:
    size_t offset_ = 0;
  };

  // Forward declarations of the concrete states. This is needed so that they
  // can access private members of the AsyncStreamingDecoder.
  class DecodeVarInt32;
  class DecodeModuleHeader;
  class DecodeSectionID;
  class DecodeSectionLength;
  class DecodeSectionPayload;
  class DecodeNumberOfFunctions;
  class DecodeFunctionLength;
  class DecodeFunctionBody;

  // Creates a buffer for the next section of the module.
  SectionBuffer* CreateNewBuffer(uint32_t module_offset, uint8_t section_id,
                                 size_t length,
                                 base::Vector<const uint8_t> length_bytes);

  std::unique_ptr<DecodingState> Error(const WasmError& error) {
    if (ok()) processor_->OnError(error);
    Fail();
    return std::unique_ptr<DecodingState>(nullptr);
  }

  std::unique_ptr<DecodingState> Error(std::string message) {
    return Error(WasmError{module_offset_ - 1, std::move(message)});
  }

  void ProcessModuleHeader() {
    if (!ok()) return;
    if (!processor_->ProcessModuleHeader(state_->buffer(), 0)) Fail();
  }

  void ProcessSection(SectionBuffer* buffer) {
    if (!ok()) return;
    if (!processor_->ProcessSection(
            buffer->section_code(), buffer->payload(),
            buffer->module_offset() +
                static_cast<uint32_t>(buffer->payload_offset()))) {
      Fail();
    }
  }

  void StartCodeSection(int num_functions,
                        std::shared_ptr<WireBytesStorage> wire_bytes_storage,
                        int code_section_start, int code_section_length) {
    if (!ok()) return;
    // The offset passed to {ProcessCodeSectionHeader} is an error offset and
    // not the start offset of a buffer. Therefore we need the -1 here.
    if (!processor_->ProcessCodeSectionHeader(
            num_functions, module_offset() - 1, std::move(wire_bytes_storage),
            code_section_start, code_section_length)) {
      Fail();
    }
  }

  void ProcessFunctionBody(base::Vector<const uint8_t> bytes,
                           uint32_t module_offset) {
    if (!ok()) return;
    if (!processor_->ProcessFunctionBody(bytes, module_offset)) Fail();
  }

  void Fail() {
    // We reset the {processor_} field to represent failure. This also ensures
    // that we do not accidentally call further methods on the processor after
    // failure.
    processor_.reset();
  }

  bool ok() const { return processor_ != nullptr; }

  uint32_t module_offset() const { return module_offset_; }

  std::unique_ptr<StreamingProcessor> processor_;
  std::unique_ptr<DecodingState> state_;
  std::vector<std::shared_ptr<SectionBuffer>> section_buffers_;
  bool code_section_processed_ = false;
  uint32_t module_offset_ = 0;
  size_t total_size_ = 0;
  bool stream_finished_ = false;

  // We need wire bytes in an array for deserializing cached modules.
  std::vector<uint8_t> wire_bytes_for_deserializing_;
};

void AsyncStreamingDecoder::OnBytesReceived(base::Vector<const uint8_t> bytes) {
  if (deserializing()) {
    wire_bytes_for_deserializing_.insert(wire_bytes_for_deserializing_.end(),
                                         bytes.begin(), bytes.end());
    return;
  }

  TRACE_STREAMING("OnBytesReceived(%zu bytes)\n", bytes.size());

  size_t current = 0;
  while (ok() && current < bytes.size()) {
    size_t num_bytes =
        state_->ReadBytes(this, bytes.SubVector(current, bytes.size()));
    current += num_bytes;
    module_offset_ += num_bytes;
    if (state_->offset() == state_->buffer().size()) {
      state_ = state_->Next(this);
    }
  }
  total_size_ += bytes.size();
  if (ok()) {
    processor_->OnFinishedChunk();
  }
}

size_t AsyncStreamingDecoder::DecodingState::ReadBytes(
    AsyncStreamingDecoder* streaming, base::Vector<const uint8_t> bytes) {
  base::Vector<uint8_t> remaining_buf = buffer() + offset();
  size_t num_bytes = std::min(bytes.size(), remaining_buf.size());
  TRACE_STREAMING("ReadBytes(%zu bytes)\n", num_bytes);
  memcpy(remaining_buf.begin(), &bytes.first(), num_bytes);
  set_offset(offset() + num_bytes);
  return num_bytes;
}

void AsyncStreamingDecoder::Finish(bool can_use_compiled_module) {
  TRACE_STREAMING("Finish\n");
  DCHECK(!stream_finished_);
  stream_finished_ = true;
  if (!ok()) return;

  if (deserializing()) {
    base::Vector<const uint8_t> wire_bytes =
        base::VectorOf(wire_bytes_for_deserializing_);
    // Try to deserialize the module from wire bytes and module bytes.
    if (can_use_compiled_module &&
        processor_->Deserialize(compiled_module_bytes_, wire_bytes))
      return;

    // Compiled module bytes are invalidated by can_use_compiled_module = false
    // or the deserialization failed. Restart decoding using |wire_bytes|.
    compiled_module_bytes_ = {};
    DCHECK(!deserializing());
    OnBytesReceived(wire_bytes);
    // The decoder has received all wire bytes; fall through and finish.
  }

  if (!state_->is_finishing_allowed()) {
    // The byte stream ended too early, we report an error.
    Error("unexpected end of stream");
    return;
  }

  base::OwnedVector<uint8_t> bytes =
      base::OwnedVector<uint8_t>::NewForOverwrite(total_size_);
  uint8_t* cursor = bytes.start();
  {
#define BYTES(x) (x & 0xFF), (x >> 8) & 0xFF, (x >> 16) & 0xFF, (x >> 24) & 0xFF
    uint8_t module_header[]{BYTES(kWasmMagic), BYTES(kWasmVersion)};
#undef BYTES
    memcpy(cursor, module_header, arraysize(module_header));
    cursor += arraysize(module_header);
  }
  for (const auto& buffer : section_buffers_) {
    DCHECK_LE(cursor - bytes.start() + buffer->length(), total_size_);
    memcpy(cursor, buffer->bytes().begin(), buffer->length());
    cursor += buffer->length();
  }
  processor_->OnFinishedStream(std::move(bytes));
}

void AsyncStreamingDecoder::Abort() {
  TRACE_STREAMING("Abort\n");
  if (stream_finished_) return;
  stream_finished_ = true;
  if (!ok()) return;  // Failed already.
  processor_->OnAbort();
  Fail();
}

namespace {

class CallMoreFunctionsCanBeSerializedCallback
    : public CompilationEventCallback {
 public:
  CallMoreFunctionsCanBeSerializedCallback(
      std::weak_ptr<NativeModule> native_module,
      AsyncStreamingDecoder::MoreFunctionsCanBeSerializedCallback callback)
      : native_module_(std::move(native_module)),
        callback_(std::move(callback)) {
    // As a baseline we also count the modules that could be cached but
    // never reach the threshold.
    if (std::shared_ptr<NativeModule> module = native_module_.lock()) {
      module->counters()->wasm_cache_count()->AddSample(0);
    }
  }

  void call(CompilationEvent event) override {
    if (event != CompilationEvent::kFinishedCompilationChunk) return;
    // If the native module is still alive, get back a shared ptr and call the
    // callback.
    if (std::shared_ptr<NativeModule> native_module = native_module_.lock()) {
      native_module->counters()->wasm_cache_count()->AddSample(++cache_count_);
      callback_(native_module);
    }
  }

  ReleaseAfterFinalEvent release_after_final_event() override {
    return kKeepAfterFinalEvent;
  }

 private:
  const std::weak_ptr<NativeModule> native_module_;
  const AsyncStreamingDecoder::MoreFunctionsCanBeSerializedCallback callback_;
  int cache_count_ = 0;
};

}  // namespace

void AsyncStreamingDecoder::NotifyNativeModuleCreated(
    const std::shared_ptr<NativeModule>& native_module) {
  if (!more_functions_can_be_serialized_callback_) return;
  auto* comp_state = native_module->compilation_state();

  comp_state->AddCallback(
      std::make_unique<CallMoreFunctionsCanBeSerializedCallback>(
          native_module,
          std::move(more_functions_can_be_serialized_callback_)));
  more_functions_can_be_serialized_callback_ = {};
}

// An abstract class to share code among the states which decode VarInts. This
// class takes over the decoding of the VarInt and then calls the actual decode
// code with the decoded value.
class AsyncStreamingDecoder::DecodeVarInt32 : public DecodingState {
 public:
  explicit DecodeVarInt32(size_t max_value, const char* field_name)
      : max_value_(max_value), field_name_(field_name) {}

  base::Vector<uint8_t> buffer() override {
    return base::ArrayVector(byte_buffer_);
  }

  size_t ReadBytes(AsyncStreamingDecoder* streaming,
                   base::Vector<const uint8_t> bytes) override;

  std::unique_ptr<DecodingState> Next(
      AsyncStreamingDecoder* streaming) override;

  virtual std::unique_ptr<DecodingState> NextWithValue(
      AsyncStreamingDecoder* streaming) = 0;

 protected:
  uint8_t byte_buffer_[kMaxVarInt32Size];
  // The maximum valid value decoded in this state. {Next} returns an error if
  // this value is exceeded.
  const size_t max_value_;
  const char* const field_name_;
  size_t value_ = 0;
  size_t bytes_consumed_ = 0;
};

class AsyncStreamingDecoder::DecodeModuleHeader : public DecodingState {
 public:
  base::Vector<uint8_t> buffer() override {
    return base::ArrayVector(byte_buffer_);
  }

  std::unique_ptr<DecodingState> Next(
      AsyncStreamingDecoder* streaming) override;

 private:
  // Checks if the magic bytes of the module header are correct.
  void CheckHeader(Decoder* decoder);

  // The size of the module header.
  static constexpr size_t kModuleHeaderSize = 8;
  uint8_t byte_buffer_[kModuleHeaderSize];
};

class AsyncStreamingDecoder::DecodeSectionID : public DecodingState {
 public:
  explicit DecodeSectionID(uint32_t module_offset)
      : module_offset_(module_offset) {}

  base::Vector<uint8_t> buffer() override { return {&id_, 1}; }
  bool is_finishing_allowed() const override { return true; }

  std::unique_ptr<DecodingState> Next(
      AsyncStreamingDecoder* streaming) override;

 private:
  uint8_t id_ = 0;
  // The start offset of this section in the module.
  const uint32_t module_offset_;
};

class AsyncStreamingDecoder::DecodeSectionLength : public DecodeVarInt32 {
 public:
  explicit DecodeSectionLength(uint8_t id, uint32_t module_offset)
      : DecodeVarInt32(max_module_size(), "section length"),
        section_id_(id),
        module_offset_(module_offset) {}

  std::unique_ptr<DecodingState> NextWithValue(
      AsyncStreamingDecoder* streaming) override;

 private:
  const uint8_t section_id_;
  // The start offset of this section in the module.
  const uint32_t module_offset_;
};

class AsyncStreamingDecoder::DecodeSectionPayload : public DecodingState {
 public:
  explicit DecodeSectionPayload(SectionBuffer* section_buffer)
      : section_buffer_(section_buffer) {}

  base::Vector<uint8_t> buffer() override { return section_buffer_->payload(); }

  std::unique_ptr<DecodingState> Next(
      AsyncStreamingDecoder* streaming) override;

 private:
  SectionBuffer* const section_buffer_;
};

class AsyncStreamingDecoder::DecodeNumberOfFunctions : public DecodeVarInt32 {
 public:
  explicit DecodeNumberOfFunctions(SectionBuffer* section_buffer)
      : DecodeVarInt32(kV8MaxWasmFunctions, "functions count"),
        section_buffer_(section_buffer) {}

  std::unique_ptr<DecodingState> NextWithValue(
      AsyncStreamingDecoder* streaming) override;

 private:
  SectionBuffer* const section_buffer_;
};

class AsyncStreamingDecoder::DecodeFunctionLength : public DecodeVarInt32 {
 public:
  explicit DecodeFunctionLength(SectionBuffer* section_buffer,
                                size_t buffer_offset,
                                size_t num_remaining_functions)
      : DecodeVarInt32(kV8MaxWasmFunctionSize, "function body size"),
        section_buffer_(section_buffer),
        buffer_offset_(buffer_offset),
        // We are reading a new function, so one function less is remaining.
        num_remaining_functions_(num_remaining_functions - 1) {
    DCHECK_GT(num_remaining_functions, 0);
  }

  std::unique_ptr<DecodingState> NextWithValue(
      AsyncStreamingDecoder* streaming) override;

 private:
  SectionBuffer* const section_buffer_;
  const size_t buffer_offset_;
  const size_t num_remaining_functions_;
};

class AsyncStreamingDecoder::DecodeFunctionBody : public DecodingState {
 public:
  explicit DecodeFunctionBody(SectionBuffer* section_buffer,
                              size_t buffer_offset, size_t function_body_length,
                              size_t num_remaining_functions,
                              uint32_t module_offset)
      : section_buffer_(section_buffer),
        buffer_offset_(buffer_offset),
        function_body_length_(function_body_length),
        num_remaining_functions_(num_remaining_functions),
        module_offset_(module_offset) {}

  base::Vector<uint8_t> buffer() override {
    base::Vector<uint8_t> remaining_buffer =
        section_buffer_->bytes() + buffer_offset_;
    return remaining_buffer.SubVector(0, function_body_length_);
  }

  std::unique_ptr<DecodingState> Next(
      AsyncStreamingDecoder* streaming) override;

 private:
  SectionBuffer* const section_buffer_;
  const size_t buffer_offset_;
  const size_t function_body_length_;
  const size_t num_remaining_functions_;
  const uint32_t module_offset_;
};

size_t AsyncStreamingDecoder::DecodeVarInt32::ReadBytes(
    AsyncStreamingDecoder* streaming, base::Vector<const uint8_t> bytes) {
  base::Vector<uint8_t> buf = buffer();
  base::Vector<uint8_t> remaining_buf = buf + offset();
  size_t new_bytes = std::min(bytes.size(), remaining_buf.size());
  TRACE_STREAMING("ReadBytes of a VarInt\n");
  memcpy(remaining_buf.begin(), &bytes.first(), new_bytes);
  buf.Truncate(offset() + new_bytes);
  Decoder decoder(buf,
                  streaming->module_offset() - static_cast<uint32_t>(offset()));
  value_ = decoder.consume_u32v(field_name_);

  if (decoder.failed()) {
    if (new_bytes == remaining_buf.size()) {
      // We only report an error if we read all bytes.
      streaming->Error(decoder.error());
    }
    set_offset(offset() + new_bytes);
    return new_bytes;
  }

  // The number of bytes we actually needed to read.
  DCHECK_GT(decoder.pc(), buffer().begin());
  bytes_consumed_ = static_cast<size_t>(decoder.pc() - buf.begin());
  TRACE_STREAMING("  ==> %zu bytes consumed\n", bytes_consumed_);

  // We read all the bytes we needed.
  DCHECK_GT(bytes_consumed_, offset());
  new_bytes = bytes_consumed_ - offset();
  // Set the offset to the buffer size to signal that we are at the end of this
  // section.
  set_offset(buffer().size());
  return new_bytes;
}

std::unique_ptr<AsyncStreamingDecoder::DecodingState>
AsyncStreamingDecoder::DecodeVarInt32::Next(AsyncStreamingDecoder* streaming) {
  if (!streaming->ok()) return nullptr;

  if (value_ > max_value_) {
    std::ostringstream oss;
    oss << "The value " << value_ << " for " << field_name_
        << " exceeds the maximum allowed value of " << max_value_;
    return streaming->Error(oss.str());
  }

  return NextWithValue(streaming);
}

std::unique_ptr<AsyncStreamingDecoder::DecodingState>
AsyncStreamingDecoder::DecodeModuleHeader::Next(
    AsyncStreamingDecoder* streaming) {
  TRACE_STREAMING("DecodeModuleHeader\n");
  streaming->ProcessModuleHeader();
  if (!streaming->ok()) return nullptr;
  return std::make_unique<DecodeSectionID>(streaming->module_offset());
}

std::unique_ptr<AsyncStreamingDecoder::DecodingState>
AsyncStreamingDecoder::DecodeSectionID::Next(AsyncStreamingDecoder* streaming) {
  TRACE_STREAMING("DecodeSectionID: %u (%s)\n", id_,
                  SectionName(static_cast<SectionCode>(id_)));
  if (id_ != kUnknownSectionCode && !IsValidSectionCode(id_)) {
    return streaming->Error("invalid section code");
  }
  if (id_ == SectionCode::kCodeSectionCode) {
    // Explicitly check for multiple code sections as module decoder never
    // sees the code section and hence cannot track this section.
    if (streaming->code_section_processed_) {
      // TODO(wasm): This error message (and others in this class) is different
      // for non-streaming decoding. Bring them in sync and test.
      return streaming->Error("code section can only appear once");
    }
    streaming->code_section_processed_ = true;
  }
  return std::make_unique<DecodeSectionLength>(id_, module_offset_);
}

std::unique_ptr<AsyncStreamingDecoder::DecodingState>
AsyncStreamingDecoder::DecodeSectionLength::NextWithValue(
    AsyncStreamingDecoder* streaming) {
  TRACE_STREAMING("DecodeSectionLength(%zu)\n", value_);
  SectionBuffer* buf =
      streaming->CreateNewBuffer(module_offset_, section_id_, value_,
                                 buffer().SubVector(0, bytes_consumed_));
  DCHECK_NOT_NULL(buf);
  if (value_ == 0) {
    if (section_id_ == SectionCode::kCodeSectionCode) {
      return streaming->Error("code section cannot have size 0");
    }
    // Process section without payload as well, to enforce section order and
    // other feature checks specific to each individual section.
    streaming->ProcessSection(buf);
    if (!streaming->ok()) return nullptr;
    // There is no payload, we go to the next section immediately.
    return std::make_unique<DecodeSectionID>(streaming->module_offset_);
  }
  if (section_id_ == SectionCode::kCodeSectionCode) {
    // We reached the code section. All functions of the code section are put
    // into the same SectionBuffer.
    return std::make_unique<DecodeNumberOfFunctions>(buf);
  }
  return std::make_unique<DecodeSectionPayload>(buf);
}

std::unique_ptr<AsyncStreamingDecoder::DecodingState>
AsyncStreamingDecoder::DecodeSectionPayload::Next(
    AsyncStreamingDecoder* streaming) {
  TRACE_STREAMING("DecodeSectionPayload\n");
  streaming->ProcessSection(section_buffer_);
  if (!streaming->ok()) return nullptr;
  return std::make_unique<DecodeSectionID>(streaming->module_offset());
}

std::unique_ptr<AsyncStreamingDecoder::DecodingState>
AsyncStreamingDecoder::DecodeNumberOfFunctions::NextWithValue(
    AsyncStreamingDecoder* streaming) {
  TRACE_STREAMING("DecodeNumberOfFunctions(%zu)\n", value_);
  // Copy the bytes we read into the section buffer.
  base::Vector<uint8_t> payload_buf = section_buffer_->payload();
  if (payload_buf.size() < bytes_consumed_) {
    return streaming->Error("invalid code section length");
  }
  memcpy(payload_buf.begin(), buffer().begin(), bytes_consumed_);

  DCHECK_GE(kMaxInt, section_buffer_->module_offset() +
                         section_buffer_->payload_offset());
  int code_section_start = static_cast<int>(section_buffer_->module_offset() +
                                            section_buffer_->payload_offset());
  DCHECK_GE(kMaxInt, payload_buf.length());
  int code_section_len = static_cast<int>(payload_buf.length());
  DCHECK_GE(kMaxInt, value_);
  streaming->StartCodeSection(static_cast<int>(value_),
                              streaming->section_buffers_.back(),
                              code_section_start, code_section_len);
  if (!streaming->ok()) return nullptr;

  // {value} is the number of functions.
  if (value_ == 0) {
    if (payload_buf.size() != bytes_consumed_) {
      return streaming->Error("not all code section bytes were used");
    }
    return std::make_unique<DecodeSectionID>(streaming->module_offset());
  }

  return std::make_unique<DecodeFunctionLength>(
      section_buffer_, section_buffer_->payload_offset() + bytes_consumed_,
      value_);
}

std::unique_ptr<AsyncStreamingDecoder::DecodingState>
AsyncStreamingDecoder::DecodeFunctionLength::NextWithValue(
    AsyncStreamingDecoder* streaming) {
  TRACE_STREAMING("DecodeFunctionLength(%zu)\n", value_);
  // Copy the bytes we consumed into the section buffer.
  base::Vector<uint8_t> fun_length_buffer =
      section_buffer_->bytes() + buffer_offset_;
  if (fun_length_buffer.size() < bytes_consumed_) {
    return streaming->Error("read past code section end");
  }
  memcpy(fun_length_buffer.begin(), buffer().begin(), bytes_consumed_);

  // {value} is the length of the function.
  if (value_ == 0) return streaming->Error("invalid function length (0)");

  if (buffer_offset_ + bytes_consumed_ + value_ > section_buffer_->length()) {
    return streaming->Error("not enough code section bytes");
  }

  return std::make_unique<DecodeFunctionBody>(
      section_buffer_, buffer_offset_ + bytes_consumed_, value_,
      num_remaining_functions_, streaming->module_offset());
}

std::unique_ptr<AsyncStreamingDecoder::DecodingState>
AsyncStreamingDecoder::DecodeFunctionBody::Next(
    AsyncStreamingDecoder* streaming) {
  TRACE_STREAMING("DecodeFunctionBody\n");
  streaming->ProcessFunctionBody(buffer(), module_offset_);
  if (!streaming->ok()) return nullptr;

  size_t end_offset = buffer_offset_ + function_body_length_;
  if (num_remaining_functions_ > 0) {
    return std::make_unique<DecodeFunctionLength>(section_buffer_, end_offset,
                                                  num_remaining_functions_);
  }
  // We just read the last function body. Continue with the next section.
  if (end_offset != section_buffer_->length()) {
    return streaming->Error("not all code section bytes were used");
  }
  return std::make_unique<DecodeSectionID>(streaming->module_offset());
}

AsyncStreamingDecoder::AsyncStreamingDecoder(
    std::unique_ptr<StreamingProcessor> processor)
    : processor_(std::move(processor)),
      // A module always starts with a module header.
      state_(new DecodeModuleHeader()) {}

AsyncStreamingDecoder::SectionBuffer* AsyncStreamingDecoder::CreateNewBuffer(
    uint32_t module_offset, uint8_t section_id, size_t length,
    base::Vector<const uint8_t> length_bytes) {
  // Section buffers are allocated in the same order they appear in the module,
  // they will be processed and later on concatenated in that same order.
  section_buffers_.emplace_back(std::make_shared<SectionBuffer>(
      module_offset, section_id, length, length_bytes));
  return section_buffers_.back().get();
}

std::unique_ptr<StreamingDecoder> StreamingDecoder::CreateAsyncStreamingDecoder(
    std::unique_ptr<StreamingProcessor> processor) {
  return std::make_unique<AsyncStreamingDecoder>(std::move(processor));
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#undef TRACE_STREAMING
