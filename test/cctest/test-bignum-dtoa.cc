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

#include <stdlib.h>

#include "src/v8.h"

#include "src/bignum-dtoa.h"

#include "src/base/platform/platform.h"
#include "src/double.h"
#include "test/cctest/cctest.h"
#include "test/cctest/gay-fixed.h"
#include "test/cctest/gay-precision.h"
#include "test/cctest/gay-shortest.h"

using namespace v8::internal;


// Removes trailing '0' digits.
// Can return the empty string if all digits are 0.
static void TrimRepresentation(Vector<char> representation) {
  int len = StrLength(representation.start());
  int i;
  for (i = len - 1; i >= 0; --i) {
    if (representation[i] != '0') break;
  }
  representation[i + 1] = '\0';
}


static const int kBufferSize = 100;


TEST(BignumDtoaVariousDoubles) {
  char buffer_container[kBufferSize];
  Vector<char> buffer(buffer_container, kBufferSize);
  int length;
  int point;

  BignumDtoa(1.0, BIGNUM_DTOA_SHORTEST, 0, buffer, &length, &point);
  CHECK_EQ("1", buffer.start());
  CHECK_EQ(1, point);

  BignumDtoa(1.0, BIGNUM_DTOA_FIXED, 3, buffer, &length, &point);
  CHECK_GE(3, length - point);
  TrimRepresentation(buffer);
  CHECK_EQ("1", buffer.start());
  CHECK_EQ(1, point);

  BignumDtoa(1.0, BIGNUM_DTOA_PRECISION, 3, buffer, &length, &point);
  CHECK_GE(3, length);
  TrimRepresentation(buffer);
  CHECK_EQ("1", buffer.start());
  CHECK_EQ(1, point);

  BignumDtoa(1.5, BIGNUM_DTOA_SHORTEST, 0, buffer, &length, &point);
  CHECK_EQ("15", buffer.start());
  CHECK_EQ(1, point);

  BignumDtoa(1.5, BIGNUM_DTOA_FIXED, 10, buffer, &length, &point);
  CHECK_GE(10, length - point);
  TrimRepresentation(buffer);
  CHECK_EQ("15", buffer.start());
  CHECK_EQ(1, point);

  BignumDtoa(1.5, BIGNUM_DTOA_PRECISION, 10, buffer, &length, &point);
  CHECK_GE(10, length);
  TrimRepresentation(buffer);
  CHECK_EQ("15", buffer.start());
  CHECK_EQ(1, point);

  double min_double = 5e-324;
  BignumDtoa(min_double, BIGNUM_DTOA_SHORTEST, 0, buffer, &length, &point);
  CHECK_EQ("5", buffer.start());
  CHECK_EQ(-323, point);

  BignumDtoa(min_double, BIGNUM_DTOA_FIXED, 5, buffer, &length, &point);
  CHECK_GE(5, length - point);
  TrimRepresentation(buffer);
  CHECK_EQ("", buffer.start());

  BignumDtoa(min_double, BIGNUM_DTOA_PRECISION, 5, buffer, &length, &point);
  CHECK_GE(5, length);
  TrimRepresentation(buffer);
  CHECK_EQ("49407", buffer.start());
  CHECK_EQ(-323, point);

  double max_double = 1.7976931348623157e308;
  BignumDtoa(max_double, BIGNUM_DTOA_SHORTEST, 0, buffer, &length, &point);
  CHECK_EQ("17976931348623157", buffer.start());
  CHECK_EQ(309, point);

  BignumDtoa(max_double, BIGNUM_DTOA_PRECISION, 7, buffer, &length, &point);
  CHECK_GE(7, length);
  TrimRepresentation(buffer);
  CHECK_EQ("1797693", buffer.start());
  CHECK_EQ(309, point);

  BignumDtoa(4294967272.0, BIGNUM_DTOA_SHORTEST, 0, buffer, &length, &point);
  CHECK_EQ("4294967272", buffer.start());
  CHECK_EQ(10, point);

  BignumDtoa(4294967272.0, BIGNUM_DTOA_FIXED, 5, buffer, &length, &point);
  CHECK_EQ("429496727200000", buffer.start());
  CHECK_EQ(10, point);


  BignumDtoa(4294967272.0, BIGNUM_DTOA_PRECISION, 14, buffer, &length, &point);
  CHECK_GE(14, length);
  TrimRepresentation(buffer);
  CHECK_EQ("4294967272", buffer.start());
  CHECK_EQ(10, point);

  BignumDtoa(4.1855804968213567e298, BIGNUM_DTOA_SHORTEST, 0,
             buffer, &length, &point);
  CHECK_EQ("4185580496821357", buffer.start());
  CHECK_EQ(299, point);

  BignumDtoa(4.1855804968213567e298, BIGNUM_DTOA_PRECISION, 20,
             buffer, &length, &point);
  CHECK_GE(20, length);
  TrimRepresentation(buffer);
  CHECK_EQ("41855804968213567225", buffer.start());
  CHECK_EQ(299, point);

  BignumDtoa(5.5626846462680035e-309, BIGNUM_DTOA_SHORTEST, 0,
             buffer, &length, &point);
  CHECK_EQ("5562684646268003", buffer.start());
  CHECK_EQ(-308, point);

  BignumDtoa(5.5626846462680035e-309, BIGNUM_DTOA_PRECISION, 1,
             buffer, &length, &point);
  CHECK_GE(1, length);
  TrimRepresentation(buffer);
  CHECK_EQ("6", buffer.start());
  CHECK_EQ(-308, point);

  BignumDtoa(2147483648.0, BIGNUM_DTOA_SHORTEST, 0,
             buffer, &length, &point);
  CHECK_EQ("2147483648", buffer.start());
  CHECK_EQ(10, point);


  BignumDtoa(2147483648.0, BIGNUM_DTOA_FIXED, 2,
             buffer, &length, &point);
  CHECK_GE(2, length - point);
  TrimRepresentation(buffer);
  CHECK_EQ("2147483648", buffer.start());
  CHECK_EQ(10, point);

  BignumDtoa(2147483648.0, BIGNUM_DTOA_PRECISION, 5,
             buffer, &length, &point);
  CHECK_GE(5, length);
  TrimRepresentation(buffer);
  CHECK_EQ("21475", buffer.start());
  CHECK_EQ(10, point);

  BignumDtoa(3.5844466002796428e+298, BIGNUM_DTOA_SHORTEST, 0,
             buffer, &length, &point);
  CHECK_EQ("35844466002796428", buffer.start());
  CHECK_EQ(299, point);

  BignumDtoa(3.5844466002796428e+298, BIGNUM_DTOA_PRECISION, 10,
             buffer, &length, &point);
  CHECK_GE(10, length);
  TrimRepresentation(buffer);
  CHECK_EQ("35844466", buffer.start());
  CHECK_EQ(299, point);

  uint64_t smallest_normal64 = V8_2PART_UINT64_C(0x00100000, 00000000);
  double v = Double(smallest_normal64).value();
  BignumDtoa(v, BIGNUM_DTOA_SHORTEST, 0, buffer, &length, &point);
  CHECK_EQ("22250738585072014", buffer.start());
  CHECK_EQ(-307, point);

  BignumDtoa(v, BIGNUM_DTOA_PRECISION, 20, buffer, &length, &point);
  CHECK_GE(20, length);
  TrimRepresentation(buffer);
  CHECK_EQ("22250738585072013831", buffer.start());
  CHECK_EQ(-307, point);

  uint64_t largest_denormal64 = V8_2PART_UINT64_C(0x000FFFFF, FFFFFFFF);
  v = Double(largest_denormal64).value();
  BignumDtoa(v, BIGNUM_DTOA_SHORTEST, 0, buffer, &length, &point);
  CHECK_EQ("2225073858507201", buffer.start());
  CHECK_EQ(-307, point);

  BignumDtoa(v, BIGNUM_DTOA_PRECISION, 20, buffer, &length, &point);
  CHECK_GE(20, length);
  TrimRepresentation(buffer);
  CHECK_EQ("2225073858507200889", buffer.start());
  CHECK_EQ(-307, point);

  BignumDtoa(4128420500802942e-24, BIGNUM_DTOA_SHORTEST, 0,
             buffer, &length, &point);
  CHECK_EQ("4128420500802942", buffer.start());
  CHECK_EQ(-8, point);

  v = 3.9292015898194142585311918e-10;
  BignumDtoa(v, BIGNUM_DTOA_SHORTEST, 0, buffer, &length, &point);
  CHECK_EQ("39292015898194143", buffer.start());

  v = 4194304.0;
  BignumDtoa(v, BIGNUM_DTOA_FIXED, 5, buffer, &length, &point);
  CHECK_GE(5, length - point);
  TrimRepresentation(buffer);
  CHECK_EQ("4194304", buffer.start());

  v = 3.3161339052167390562200598e-237;
  BignumDtoa(v, BIGNUM_DTOA_PRECISION, 19, buffer, &length, &point);
  CHECK_GE(19, length);
  TrimRepresentation(buffer);
  CHECK_EQ("3316133905216739056", buffer.start());
  CHECK_EQ(-236, point);

  v = 7.9885183916008099497815232e+191;
  BignumDtoa(v, BIGNUM_DTOA_PRECISION, 4, buffer, &length, &point);
  CHECK_GE(4, length);
  TrimRepresentation(buffer);
  CHECK_EQ("7989", buffer.start());
  CHECK_EQ(192, point);

  v = 1.0000000000000012800000000e+17;
  BignumDtoa(v, BIGNUM_DTOA_FIXED, 1, buffer, &length, &point);
  CHECK_GE(1, length - point);
  TrimRepresentation(buffer);
  CHECK_EQ("100000000000000128", buffer.start());
  CHECK_EQ(18, point);
}


TEST(BignumDtoaGayShortest) {
  char buffer_container[kBufferSize];
  Vector<char> buffer(buffer_container, kBufferSize);
  int length;
  int point;

  Vector<const PrecomputedShortest> precomputed =
      PrecomputedShortestRepresentations();
  for (int i = 0; i < precomputed.length(); ++i) {
    const PrecomputedShortest current_test = precomputed[i];
    double v = current_test.v;
    BignumDtoa(v, BIGNUM_DTOA_SHORTEST, 0, buffer, &length, &point);
    CHECK_EQ(current_test.decimal_point, point);
    CHECK_EQ(current_test.representation, buffer.start());
  }
}


TEST(BignumDtoaGayFixed) {
  char buffer_container[kBufferSize];
  Vector<char> buffer(buffer_container, kBufferSize);
  int length;
  int point;

  Vector<const PrecomputedFixed> precomputed =
      PrecomputedFixedRepresentations();
  for (int i = 0; i < precomputed.length(); ++i) {
    const PrecomputedFixed current_test = precomputed[i];
    double v = current_test.v;
    int number_digits = current_test.number_digits;
    BignumDtoa(v, BIGNUM_DTOA_FIXED, number_digits, buffer, &length, &point);
    CHECK_EQ(current_test.decimal_point, point);
    CHECK_GE(number_digits, length - point);
    TrimRepresentation(buffer);
    CHECK_EQ(current_test.representation, buffer.start());
  }
}


TEST(BignumDtoaGayPrecision) {
  char buffer_container[kBufferSize];
  Vector<char> buffer(buffer_container, kBufferSize);
  int length;
  int point;

  Vector<const PrecomputedPrecision> precomputed =
      PrecomputedPrecisionRepresentations();
  for (int i = 0; i < precomputed.length(); ++i) {
    const PrecomputedPrecision current_test = precomputed[i];
    double v = current_test.v;
    int number_digits = current_test.number_digits;
    BignumDtoa(v, BIGNUM_DTOA_PRECISION, number_digits,
               buffer, &length, &point);
    CHECK_EQ(current_test.decimal_point, point);
    CHECK_GE(number_digits, length);
    TrimRepresentation(buffer);
    CHECK_EQ(current_test.representation, buffer.start());
  }
}
