// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_STRING_UTIL_H_
#define V8_INSPECTOR_STRING_UTIL_H_

#include <stdint.h>
#include <memory>

#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/inspector/string-16.h"

#include "include/v8-inspector.h"

namespace v8_inspector {

namespace protocol {

class Value;

using String = v8_inspector::String16;
using StringBuilder = v8_inspector::String16Builder;
struct ProtocolMessage {
  String json;
  std::vector<uint8_t> binary;
};

class StringUTF8Adapter {
 public:
  explicit StringUTF8Adapter(const String& string) : string_(string.utf8()) {}
  const char* Data() const { return string_.data(); }
  size_t length() const { return string_.length(); }

 private:
  std::string string_;
};

class StringUtil {
 public:
  static String substring(const String& s, size_t pos, size_t len) {
    return s.substring(pos, len);
  }
  static String fromInteger(int number) { return String::fromInteger(number); }
  static String fromInteger(size_t number) {
    return String::fromInteger(number);
  }
  static String fromDouble(double number) { return String::fromDouble(number); }
  static double toDouble(const char* s, size_t len, bool* isOk);
  static size_t find(const String& s, const char* needle) {
    return s.find(needle);
  }
  static size_t find(const String& s, const String& needle) {
    return s.find(needle);
  }
  static const size_t kNotFound = String::kNotFound;
  static void builderAppend(StringBuilder& builder, const String& s) {
    builder.append(s);
  }
  static void builderAppend(StringBuilder& builder, UChar c) {
    builder.append(c);
  }
  static void builderAppend(StringBuilder& builder, const char* s, size_t len) {
    builder.append(s, len);
  }
  static void builderAppendQuotedString(StringBuilder&, const String&);
  static void builderReserve(StringBuilder& builder, size_t capacity) {
    builder.reserveCapacity(capacity);
  }
  static String builderToString(StringBuilder& builder) {
    return builder.toString();
  }
  static std::unique_ptr<protocol::Value> parseJSON(const String16& json);
  static std::unique_ptr<protocol::Value> parseJSON(const StringView& json);
  static std::unique_ptr<protocol::Value> parseProtocolMessage(
      const ProtocolMessage&);
  static ProtocolMessage jsonToMessage(String message);
  static ProtocolMessage binaryToMessage(std::vector<uint8_t> message);

  static String fromUTF8(const uint8_t* data, size_t length) {
    return String16::fromUTF8(reinterpret_cast<const char*>(data), length);
  }
  static void writeUTF8(const String& string, std::vector<uint8_t>* out) {
    // TODO(pfeldman): get rid of the copy here.
    std::string utf8 = string.utf8();
    const uint8_t* data = reinterpret_cast<const uint8_t*>(utf8.data());
    out->insert(out->end(), data, data + utf8.length());
  }
};

// A read-only sequence of uninterpreted bytes with reference-counted storage.
// Though the templates for generating the protocol bindings reference
// this type, js_protocol.pdl doesn't have a field of type 'binary', so
// therefore it's unnecessary to provide an implementation here.
class Binary {
 public:
  const uint8_t* data() const { UNIMPLEMENTED(); }
  size_t size() const { UNIMPLEMENTED(); }
  String toBase64() const { UNIMPLEMENTED(); }
  static Binary fromBase64(const String& base64, bool* success) {
    UNIMPLEMENTED();
  }
  static Binary fromSpan(const uint8_t* data, size_t size) { UNIMPLEMENTED(); }
};
}  // namespace protocol

v8::Local<v8::String> toV8String(v8::Isolate*, const String16&);
v8::Local<v8::String> toV8StringInternalized(v8::Isolate*, const String16&);
v8::Local<v8::String> toV8StringInternalized(v8::Isolate*, const char*);
v8::Local<v8::String> toV8String(v8::Isolate*, const StringView&);
// TODO(dgozman): rename to toString16.
String16 toProtocolString(v8::Isolate*, v8::Local<v8::String>);
String16 toProtocolStringWithTypeCheck(v8::Isolate*, v8::Local<v8::Value>);
String16 toString16(const StringView&);
StringView toStringView(const String16&);
bool stringViewStartsWith(const StringView&, const char*);

class StringBufferImpl : public StringBuffer {
 public:
  // Destroys string's content.
  static std::unique_ptr<StringBufferImpl> adopt(String16&);
  const StringView& string() override { return m_string; }

 private:
  explicit StringBufferImpl(String16&);
  String16 m_owner;
  StringView m_string;

  DISALLOW_COPY_AND_ASSIGN(StringBufferImpl);
};

class BinaryStringBuffer : public StringBuffer {
 public:
  explicit BinaryStringBuffer(std::vector<uint8_t> data)
      : m_data(std::move(data)), m_string(m_data.data(), m_data.size()) {}
  const StringView& string() override { return m_string; }

 private:
  std::vector<uint8_t> m_data;
  StringView m_string;

  DISALLOW_COPY_AND_ASSIGN(BinaryStringBuffer);
};

String16 debuggerIdToString(const std::pair<int64_t, int64_t>& debuggerId);
String16 stackTraceIdToString(uintptr_t id);

}  //  namespace v8_inspector

#endif  // V8_INSPECTOR_STRING_UTIL_H_
