// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "status.h"
#include "test_platform.h"

namespace v8_crdtp {
// =============================================================================
// Status and Error codes
// =============================================================================

TEST(StatusTest, StatusToASCIIString) {
  Status ok_status;
  EXPECT_EQ("OK", ok_status.ToASCIIString());
  Status json_error(Error::JSON_PARSER_COLON_EXPECTED, 42);
  EXPECT_EQ("JSON: colon expected at position 42", json_error.ToASCIIString());
  Status cbor_error(Error::CBOR_TRAILING_JUNK, 21);
  EXPECT_EQ("CBOR: trailing junk at position 21", cbor_error.ToASCIIString());
}
}  // namespace v8_crdtp
