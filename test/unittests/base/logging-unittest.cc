// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#include "src/base/logging.h"
#include "src/objects.h"
#include "testing/gtest-support.h"

namespace v8 {
namespace base {

namespace {

#define CHECK_SUCCEED(NAME, lhs, rhs)                                      \
  {                                                                        \
    std::string* error_message =                                           \
        Check##NAME##Impl<decltype(lhs), decltype(rhs)>((lhs), (rhs), ""); \
    EXPECT_EQ(nullptr, error_message);                                     \
  }

#define CHECK_FAIL(NAME, lhs, rhs)                                         \
  {                                                                        \
    std::string* error_message =                                           \
        Check##NAME##Impl<decltype(lhs), decltype(rhs)>((lhs), (rhs), ""); \
    EXPECT_NE(nullptr, error_message);                                     \
    delete error_message;                                                  \
  }

}  // namespace

TEST(LoggingTest, CheckEQImpl) {
  CHECK_SUCCEED(EQ, 0.0, 0.0)
  CHECK_SUCCEED(EQ, 0.0, -0.0)
  CHECK_SUCCEED(EQ, -0.0, 0.0)
  CHECK_SUCCEED(EQ, -0.0, -0.0)
}

TEST(LoggingTest, CompareSignedMismatch) {
  CHECK_SUCCEED(EQ, static_cast<int32_t>(14), static_cast<uint32_t>(14))
  CHECK_FAIL(EQ, static_cast<int32_t>(14), static_cast<uint32_t>(15))
  CHECK_FAIL(EQ, static_cast<int32_t>(-1), static_cast<uint32_t>(-1))
  CHECK_SUCCEED(LT, static_cast<int32_t>(-1), static_cast<uint32_t>(0))
  CHECK_SUCCEED(LT, static_cast<int32_t>(-1), static_cast<uint32_t>(-1))
  CHECK_SUCCEED(LE, static_cast<int32_t>(-1), static_cast<uint32_t>(0))
  CHECK_SUCCEED(LE, static_cast<int32_t>(55), static_cast<uint32_t>(55))
  CHECK_SUCCEED(LT, static_cast<int32_t>(55), static_cast<uint32_t>(0x7fffff00))
  CHECK_SUCCEED(LE, static_cast<int32_t>(55), static_cast<uint32_t>(0x7fffff00))
  CHECK_SUCCEED(GE, static_cast<uint32_t>(0x7fffff00), static_cast<int32_t>(55))
  CHECK_SUCCEED(GT, static_cast<uint32_t>(0x7fffff00), static_cast<int32_t>(55))
  CHECK_SUCCEED(GT, static_cast<uint32_t>(-1), static_cast<int32_t>(-1))
  CHECK_SUCCEED(GE, static_cast<uint32_t>(0), static_cast<int32_t>(-1))
  CHECK_SUCCEED(LT, static_cast<int8_t>(-1), static_cast<uint32_t>(0))
  CHECK_SUCCEED(GT, static_cast<uint64_t>(0x7f01010101010101), 0)
  CHECK_SUCCEED(LE, static_cast<int64_t>(0xff01010101010101),
                static_cast<uint8_t>(13))
}

TEST(LoggingTest, CompareAgainstStaticConstPointer) {
  // These used to produce link errors before http://crrev.com/2524093002.
  CHECK_FAIL(EQ, v8::internal::Smi::kZero, v8::internal::Smi::FromInt(17));
  CHECK_SUCCEED(GT, 0, v8::internal::Smi::kMinValue);
}

#define CHECK_BOTH(name, lhs, rhs) \
  CHECK_##name(lhs, rhs);          \
  DCHECK_##name(lhs, rhs)

namespace {
std::string FailureMessage(const char* msg, const char* debug_msg) {
  std::string regexp(msg);
#ifdef DEBUG
  regexp.append(" (").append(debug_msg).append(")");
#endif
  size_t last_pos = 0;
  do {
    size_t pos = regexp.find_first_of("(){}+*", last_pos);
    if (pos == std::string::npos) break;
    regexp.insert(pos, "\\");
    last_pos = pos + 2;
  } while (true);
  return regexp;
}
}  // namespace

TEST(LoggingTest, CompareWithDifferentSignedness) {
  int32_t i32 = 10;
  uint32_t u32 = 20;
  int64_t i64 = 30;
  uint64_t u64 = 40;

  // All these checks should compile (!) and succeed.
  CHECK_BOTH(EQ, i32 + 10, u32);
  CHECK_BOTH(LT, i32, u64);
  CHECK_BOTH(LE, u32, i64);
  CHECK_BOTH(IMPLIES, i32, i64);
  CHECK_BOTH(IMPLIES, u32, i64);
  CHECK_BOTH(IMPLIES, !u32, !i64);

  // Check that the values are output correctly on error.
  ASSERT_DEATH_IF_SUPPORTED(
      ([&] { CHECK_GT(i32, u64); })(),
      FailureMessage("Check failed: i32 > u64", "10 vs. 40"));
}

TEST(LoggingTest, CompareWithReferenceType) {
  int32_t i32 = 10;
  uint32_t u32 = 20;
  int64_t i64 = 30;
  uint64_t u64 = 40;

  // All these checks should compile (!) and succeed.
  CHECK_BOTH(EQ, i32 + 10, *&u32);
  CHECK_BOTH(LT, *&i32, u64);
  CHECK_BOTH(IMPLIES, *&i32, i64);
  CHECK_BOTH(IMPLIES, *&i32, u64);

  // Check that the values are output correctly on error.
  ASSERT_DEATH_IF_SUPPORTED(
      ([&] { CHECK_GT(*&i32, u64); })(),
      FailureMessage("Check failed: *&i32 > u64", "10 vs. 40"));
}

enum TestEnum1 { ONE, TWO };
enum TestEnum2 : uint16_t { FOO = 14, BAR = 5 };
enum class TestEnum3 { A, B };
enum class TestEnum4 : uint8_t { FIRST, SECOND };

TEST(LoggingTest, CompareEnumTypes) {
  // All these checks should compile (!) and succeed.
  CHECK_BOTH(EQ, ONE, ONE);
  CHECK_BOTH(LT, ONE, TWO);
  CHECK_BOTH(EQ, BAR, 5);
  CHECK_BOTH(LT, BAR, FOO);
  CHECK_BOTH(EQ, TestEnum3::A, TestEnum3::A);
  CHECK_BOTH(LT, TestEnum3::A, TestEnum3::B);
  CHECK_BOTH(EQ, TestEnum4::FIRST, TestEnum4::FIRST);
  CHECK_BOTH(LT, TestEnum4::FIRST, TestEnum4::SECOND);
}

class TestClass1 {
 public:
  bool operator==(const TestClass1&) const { return true; }
  bool operator!=(const TestClass1&) const { return false; }
};
class TestClass2 {
 public:
  explicit TestClass2(int val) : val_(val) {}
  bool operator<(const TestClass2& other) const { return val_ < other.val_; }
  int val() const { return val_; }

 private:
  int val_;
};
std::ostream& operator<<(std::ostream& str, const TestClass2& val) {
  return str << "TestClass2(" << val.val() << ")";
}

TEST(LoggingTest, CompareClassTypes) {
  // All these checks should compile (!) and succeed.
  CHECK_BOTH(EQ, TestClass1{}, TestClass1{});
  CHECK_BOTH(LT, TestClass2{2}, TestClass2{7});

  // Check that the values are output correctly on error.
  ASSERT_DEATH_IF_SUPPORTED(
      ([&] { CHECK_NE(TestClass1{}, TestClass1{}); })(),
      FailureMessage("Check failed: TestClass1{} != TestClass1{}",
                     "<unprintable> vs. <unprintable>"));
  ASSERT_DEATH_IF_SUPPORTED(
      ([&] { CHECK_LT(TestClass2{4}, TestClass2{3}); })(),
      FailureMessage("Check failed: TestClass2{4} < TestClass2{3}",
                     "TestClass2(4) vs. TestClass2(3)"));
}

TEST(LoggingDeathTest, OutputEnumValues) {
  ASSERT_DEATH_IF_SUPPORTED(
      ([&] { CHECK_EQ(ONE, TWO); })(),
      FailureMessage("Check failed: ONE == TWO", "0 vs. 1"));
  ASSERT_DEATH_IF_SUPPORTED(
      ([&] { CHECK_NE(BAR, 2 + 3); })(),
      FailureMessage("Check failed: BAR != 2 + 3", "5 vs. 5"));
  ASSERT_DEATH_IF_SUPPORTED(
      ([&] { CHECK_EQ(TestEnum3::A, TestEnum3::B); })(),
      FailureMessage("Check failed: TestEnum3::A == TestEnum3::B", "0 vs. 1"));
  ASSERT_DEATH_IF_SUPPORTED(
      ([&] { CHECK_GE(TestEnum4::FIRST, TestEnum4::SECOND); })(),
      FailureMessage("Check failed: TestEnum4::FIRST >= TestEnum4::SECOND",
                     "0 vs. 1"));
}

enum TestEnum5 { TEST_A, TEST_B };
enum class TestEnum6 { TEST_C, TEST_D };
std::ostream& operator<<(std::ostream& str, TestEnum5 val) {
  return str << (val == TEST_A ? "A" : "B");
}
void operator<<(std::ostream& str, TestEnum6 val) {
  str << (val == TestEnum6::TEST_C ? "C" : "D");
}

TEST(LoggingDeathTest, OutputEnumWithOutputOperator) {
  ASSERT_DEATH_IF_SUPPORTED(
      ([&] { CHECK_EQ(TEST_A, TEST_B); })(),
      FailureMessage("Check failed: TEST_A == TEST_B", "A vs. B"));
  ASSERT_DEATH_IF_SUPPORTED(
      ([&] { CHECK_GE(TestEnum6::TEST_C, TestEnum6::TEST_D); })(),
      FailureMessage("Check failed: TestEnum6::TEST_C >= TestEnum6::TEST_D",
                     "C vs. D"));
}

TEST(LoggingDeathTest, FatalKills) {
  ASSERT_DEATH_IF_SUPPORTED(FATAL("Dread pirate"), "Dread pirate");
}

TEST(LoggingDeathTest, DcheckIsOnlyFatalInDebug) {
#ifdef DEBUG
  ASSERT_DEATH_IF_SUPPORTED(DCHECK(false && "Dread pirate"), "Dread pirate");
#else
  // DCHECK should be non-fatal if DEBUG is undefined.
  DCHECK(false && "I'm a benign teapot");
#endif
}

namespace {
void DcheckOverrideFunction(const char*, int, const char*) {}
}  // namespace

TEST(LoggingDeathTest, V8_DcheckCanBeOverridden) {
  // Default DCHECK state should be fatal.
  ASSERT_DEATH_IF_SUPPORTED(V8_Dcheck(__FILE__, __LINE__, "Dread pirate"),
                            "Dread pirate");

  ASSERT_DEATH_IF_SUPPORTED(
      {
        v8::base::SetDcheckFunction(&DcheckOverrideFunction);
        // This should be non-fatal.
        V8_Dcheck(__FILE__, __LINE__, "I'm a benign teapot.");

        // Restore default behavior, and assert on lethality.
        v8::base::SetDcheckFunction(nullptr);
        V8_Dcheck(__FILE__, __LINE__, "Dread pirate");
      },
      "Dread pirate");
}

}  // namespace base
}  // namespace v8
