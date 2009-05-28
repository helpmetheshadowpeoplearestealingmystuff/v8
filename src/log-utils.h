// Copyright 2006-2009 the V8 project authors. All rights reserved.
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

#ifndef V8_LOG_UTILS_H_
#define V8_LOG_UTILS_H_

namespace v8 {
namespace internal {

#ifdef ENABLE_LOGGING_AND_PROFILING

// A memory buffer that increments its size as you write in it.  Size
// is incremented with 'block_size' steps, never exceeding 'max_size'.
// During growth, memory contents are never copied.
//
// An instance of this class is created dynamically by Log.
class LogDynamicBuffer {
 public:
  LogDynamicBuffer(int block_size, int max_size);

  ~LogDynamicBuffer();

  // Reads contents of the buffer starting from 'from_pos'.  Upon
  // return, 'dest_buf' is filled with the data. Actual amount of data
  // filled is returned, it is <= 'buf_size'.
  int Read(int from_pos, char* dest_buf, int buf_size);

  // Writes 'data' to the buffer, making it larger if necessary.  If
  // data is too big to fit in the buffer, it doesn't get written at
  // all. Returns amount of data written (it is either 'data_size', or
  // 0, if 'data' is too big).
  int Write(const char* data, int data_size);

 private:
  void AllocateBlock(int index) {
    blocks_[index] = NewArray<char>(block_size_);
  }

  int BlockIndex(int pos) const { return pos / block_size_; }

  int BlocksCount() const { return BlockIndex(max_size_) + 1; }

  int PosInBlock(int pos) const { return pos % block_size_; }

  const int block_size_;
  const int max_size_;
  ScopedVector<char*> blocks_;
  int write_pos_;
  int block_index_;
  int block_write_pos_;
};


// Functions and data for performing output of log messages.
class Log : public AllStatic {
 public:
  // Opens stdout for logging.
  static void OpenStdout();

  // Opens file for logging.
  static void OpenFile(const char* name);

  // Opens memory buffer for logging.
  static void OpenMemoryBuffer();

  // Frees all resources acquired in Open... functions.
  static void Close();

  // See description in include/v8.h.
  static int GetLogLines(int from_pos, char* dest_buf, int max_size);

  // Returns whether logging is enabled.
  static bool IsEnabled() {
    return output_handle_ != NULL || output_buffer_ != NULL;
  }

 private:
  typedef int (*WritePtr)(const char* msg, int length);

  // Initialization function called from Open... functions.
  static void Init();

  // Write functions assume that mutex_ is acquired by the caller.
  static WritePtr Write;

  // Implementation of writing to a log file.
  static int WriteToFile(const char* msg, int length) {
    ASSERT(output_handle_ != NULL);
    int rv = fwrite(msg, 1, length, output_handle_);
    ASSERT(length == rv);
    return rv;
  }

  // Implementation of writing to a memory buffer.
  static int WriteToMemory(const char* msg, int length) {
    ASSERT(output_buffer_ != NULL);
    return output_buffer_->Write(msg, length);
  }

  // When logging is active, either output_handle_ or output_buffer_ is used
  // to store a pointer to log destination. If logging was opened via OpenStdout
  // or OpenFile, then output_handle_ is used. If logging was opened
  // via OpenMemoryBuffer, then output_buffer_ is used.
  // mutex_ should be acquired before using output_handle_ or output_buffer_.
  static FILE* output_handle_;

  static LogDynamicBuffer* output_buffer_;

  // Size of dynamic buffer block (and dynamic buffer initial size).
  static const int kDynamicBufferBlockSize = 65536;

  // Maximum size of dynamic buffer.
  static const int kMaxDynamicBufferSize = 50 * 1024 * 1024;

  // mutex_ is a Mutex used for enforcing exclusive
  // access to the formatting buffer and the log file or log memory buffer.
  static Mutex* mutex_;

  // Size of buffer used for formatting log messages.
  static const int kMessageBufferSize = 2048;

  // Buffer used for formatting log messages. This is a singleton buffer and
  // mutex_ should be acquired before using it.
  static char* message_buffer_;

  friend class LogMessageBuilder;
};


// Utility class for formatting log messages. It fills the message into the
// static buffer in Log.
class LogMessageBuilder BASE_EMBEDDED {
 public:
  // Create a message builder starting from position 0. This acquires the mutex
  // in the log as well.
  explicit LogMessageBuilder();
  ~LogMessageBuilder() { }

  // Append string data to the log message.
  void Append(const char* format, ...);

  // Append string data to the log message.
  void Append(const char* format, va_list args);

  // Append a character to the log message.
  void Append(const char c);

  // Append a heap string.
  void Append(String* str);

  void AppendDetailed(String* str, bool show_impl_info);

  // Write the log message to the log file currently opened.
  void WriteToLogFile();

  // Write a null-terminated string to to the log file currently opened.
  void WriteCStringToLogFile(const char* str);

 private:
  ScopedLock sl;
  int pos_;
};

#endif  // ENABLE_LOGGING_AND_PROFILING

} }  // namespace v8::internal

#endif  // V8_LOG_UTILS_H_
