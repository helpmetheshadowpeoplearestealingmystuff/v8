// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CRDTP_STATUS_H_
#define V8_CRDTP_STATUS_H_

#include <cstddef>
#include <limits>
#include <string>

#include "export.h"

namespace v8_crdtp {
// =============================================================================
// Status and Error codes
// =============================================================================

enum class Error {
  OK = 0,
  // JSON parsing errors - json_parser.{h,cc}.
  JSON_PARSER_UNPROCESSED_INPUT_REMAINS = 0x01,
  JSON_PARSER_STACK_LIMIT_EXCEEDED = 0x02,
  JSON_PARSER_NO_INPUT = 0x03,
  JSON_PARSER_INVALID_TOKEN = 0x04,
  JSON_PARSER_INVALID_NUMBER = 0x05,
  JSON_PARSER_INVALID_STRING = 0x06,
  JSON_PARSER_UNEXPECTED_ARRAY_END = 0x07,
  JSON_PARSER_COMMA_OR_ARRAY_END_EXPECTED = 0x08,
  JSON_PARSER_STRING_LITERAL_EXPECTED = 0x09,
  JSON_PARSER_COLON_EXPECTED = 0x0a,
  JSON_PARSER_UNEXPECTED_MAP_END = 0x0b,
  JSON_PARSER_COMMA_OR_MAP_END_EXPECTED = 0x0c,
  JSON_PARSER_VALUE_EXPECTED = 0x0d,

  CBOR_INVALID_INT32 = 0x0e,
  CBOR_INVALID_DOUBLE = 0x0f,
  CBOR_INVALID_ENVELOPE = 0x10,
  CBOR_ENVELOPE_CONTENTS_LENGTH_MISMATCH = 0x11,
  CBOR_MAP_OR_ARRAY_EXPECTED_IN_ENVELOPE = 0x12,
  CBOR_INVALID_STRING8 = 0x13,
  CBOR_INVALID_STRING16 = 0x14,
  CBOR_INVALID_BINARY = 0x15,
  CBOR_UNSUPPORTED_VALUE = 0x16,
  CBOR_NO_INPUT = 0x17,
  CBOR_INVALID_START_BYTE = 0x18,
  CBOR_UNEXPECTED_EOF_EXPECTED_VALUE = 0x19,
  CBOR_UNEXPECTED_EOF_IN_ARRAY = 0x1a,
  CBOR_UNEXPECTED_EOF_IN_MAP = 0x1b,
  CBOR_INVALID_MAP_KEY = 0x1c,
  CBOR_STACK_LIMIT_EXCEEDED = 0x1d,
  CBOR_TRAILING_JUNK = 0x1e,
  CBOR_MAP_START_EXPECTED = 0x1f,
  CBOR_MAP_STOP_EXPECTED = 0x20,
  CBOR_ARRAY_START_EXPECTED = 0x21,
  CBOR_ENVELOPE_SIZE_LIMIT_EXCEEDED = 0x22,

  BINDINGS_MANDATORY_FIELD_MISSING = 0x23,
  BINDINGS_BOOL_VALUE_EXPECTED = 0x24,
  BINDINGS_INT32_VALUE_EXPECTED = 0x25,
  BINDINGS_DOUBLE_VALUE_EXPECTED = 0x26,
  BINDINGS_STRING_VALUE_EXPECTED = 0x27,
  BINDINGS_STRING8_VALUE_EXPECTED = 0x28,
  BINDINGS_BINARY_VALUE_EXPECTED = 0x29,
};

// A status value with position that can be copied. The default status
// is OK. Usually, error status values should come with a valid position.
struct Status {
  static constexpr size_t npos() { return std::numeric_limits<size_t>::max(); }

  bool ok() const { return error == Error::OK; }

  Error error = Error::OK;
  size_t pos = npos();
  Status(Error error, size_t pos) : error(error), pos(pos) {}
  Status() = default;

  // Returns a 7 bit US-ASCII string, either "OK" or an error message
  // that includes the position.
  std::string ToASCIIString() const;

 private:
  std::string ToASCIIString(const char* msg) const;
};
}  // namespace v8_crdtp

#endif  // V8_CRDTP_STATUS_H_
