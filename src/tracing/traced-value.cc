// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/tracing/traced-value.h"

#include "src/base/platform/platform.h"

namespace v8 {
namespace tracing {

namespace {

#define DCHECK_CURRENT_CONTAINER_IS(x) DCHECK_EQ(x, nesting_stack_.back())
#define DCHECK_CONTAINER_STACK_DEPTH_EQ(x) DCHECK_EQ(x, nesting_stack_.size())
#ifdef DEBUG
const bool kStackTypeDict = false;
const bool kStackTypeArray = true;
#define DEBUG_PUSH_CONTAINER(x) nesting_stack_.push_back(x)
#define DEBUG_POP_CONTAINER() nesting_stack_.pop_back()
#else
#define DEBUG_PUSH_CONTAINER(x) ((void)0)
#define DEBUG_POP_CONTAINER() ((void)0)
#endif

std::string EscapeString(const std::string& value) {
  std::string result;
  result.reserve(value.length() + 2);
  result += '"';
  size_t length = value.length();
  char number_buffer[10];
  for (size_t src = 0; src < length; ++src) {
    char c = value[src];
    switch (c) {
      case '\t':
        result += "\\t";
        break;
      case '\n':
        result += "\\n";
        break;
      case '\"':
        result += "\\\"";
        break;
      case '\\':
        result += "\\\\";
        break;
      default:
        if (c < '\040') {
          base::OS::SNPrintF(
              number_buffer, arraysize(number_buffer), "\\u%04X",
              static_cast<unsigned>(static_cast<unsigned char>(c)));
          result += number_buffer;
        } else {
          result += c;
        }
    }
  }
  result += '"';
  return result;
}

}  // namespace

std::unique_ptr<TracedValue> TracedValue::Create() {
  return std::unique_ptr<TracedValue>(new TracedValue());
}

TracedValue::TracedValue() : first_item_(true) {
  DEBUG_PUSH_CONTAINER(kStackTypeDict);
}

TracedValue::~TracedValue() {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  DEBUG_POP_CONTAINER();
  DCHECK_CONTAINER_STACK_DEPTH_EQ(0u);
}

void TracedValue::SetInteger(const char* name, int value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  WriteName(name);
  data_ += std::to_string(value);
}

void TracedValue::SetDouble(const char* name, double value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  WriteName(name);
  data_ += std::to_string(value);
}

void TracedValue::SetBoolean(const char* name, bool value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  WriteName(name);
  data_ += value ? "true" : "false";
}

void TracedValue::SetString(const char* name, const std::string& value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  WriteName(name);
  data_ += EscapeString(value);
}

void TracedValue::BeginDictionary(const char* name) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  DEBUG_PUSH_CONTAINER(kStackTypeDict);
  WriteName(name);
  data_ += '{';
  first_item_ = true;
}

void TracedValue::BeginArray(const char* name) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  DEBUG_PUSH_CONTAINER(kStackTypeArray);
  WriteName(name);
  data_ += '[';
  first_item_ = true;
}

void TracedValue::AppendInteger(int value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeArray);
  WriteComma();
  data_ += std::to_string(value);
}

void TracedValue::AppendLongInteger(int64_t value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeArray);
  WriteComma();
  data_ += std::to_string(value);
}

void TracedValue::AppendDouble(double value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeArray);
  WriteComma();
  data_ += std::to_string(value);
}

void TracedValue::AppendBoolean(bool value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeArray);
  WriteComma();
  data_ += value ? "true" : "false";
}

void TracedValue::AppendString(const std::string& value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeArray);
  WriteComma();
  data_ += EscapeString(value);
}

void TracedValue::BeginDictionary() {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeArray);
  DEBUG_PUSH_CONTAINER(kStackTypeDict);
  WriteComma();
  data_ += '{';
  first_item_ = true;
}

void TracedValue::BeginArray() {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeArray);
  DEBUG_PUSH_CONTAINER(kStackTypeArray);
  WriteComma();
  data_ += '[';
  first_item_ = true;
}

void TracedValue::EndDictionary() {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  DEBUG_POP_CONTAINER();
  data_ += '}';
  first_item_ = false;
}

void TracedValue::EndArray() {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeArray);
  DEBUG_POP_CONTAINER();
  data_ += ']';
  first_item_ = false;
}

void TracedValue::WriteComma() {
  if (first_item_) {
    first_item_ = false;
  } else {
    data_ += ',';
  }
}

void TracedValue::WriteName(const char* name) {
  WriteComma();
  data_ += '"';
  data_ += name;
  data_ += "\":";
}

void TracedValue::AppendAsTraceFormat(std::string* out) const {
  *out += '{';
  *out += data_;
  *out += '}';
}

}  // namespace tracing
}  // namespace v8
