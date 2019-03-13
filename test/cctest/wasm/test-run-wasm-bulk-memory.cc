// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/cctest.h"
#include "test/cctest/wasm/wasm-run-utils.h"
#include "test/common/wasm/wasm-macro-gen.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace test_run_wasm_bulk_memory {

namespace {
void CheckMemoryEquals(TestingModuleBuilder& builder, size_t index,
                       const std::vector<byte>& expected) {
  const byte* mem_start = builder.raw_mem_start<byte>();
  const byte* mem_end = builder.raw_mem_end<byte>();
  size_t mem_size = mem_end - mem_start;
  CHECK_LE(index, mem_size);
  CHECK_LE(index + expected.size(), mem_size);
  for (size_t i = 0; i < expected.size(); ++i) {
    CHECK_EQ(expected[i], mem_start[index + i]);
  }
}

void CheckMemoryEqualsZero(TestingModuleBuilder& builder, size_t index,
                           size_t length) {
  const byte* mem_start = builder.raw_mem_start<byte>();
  const byte* mem_end = builder.raw_mem_end<byte>();
  size_t mem_size = mem_end - mem_start;
  CHECK_LE(index, mem_size);
  CHECK_LE(index + length, mem_size);
  for (size_t i = 0; i < length; ++i) {
    CHECK_EQ(0, mem_start[index + i]);
  }
}

void CheckMemoryEqualsFollowedByZeroes(TestingModuleBuilder& builder,
                                       const std::vector<byte>& expected) {
  CheckMemoryEquals(builder, 0, expected);
  CheckMemoryEqualsZero(builder, expected.size(),
                        builder.mem_size() - expected.size());
}
}  // namespace

WASM_EXEC_TEST(MemoryInit) {
  EXPERIMENTAL_FLAG_SCOPE(bulk_memory);
  WasmRunner<uint32_t, uint32_t, uint32_t, uint32_t> r(execution_tier);
  r.builder().AddMemory(kWasmPageSize);
  const byte data[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  r.builder().AddPassiveDataSegment(Vector<const byte>(data));
  BUILD(r,
        WASM_MEMORY_INIT(0, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1),
                         WASM_GET_LOCAL(2)),
        kExprI32Const, 0);

  // All zeroes.
  CheckMemoryEqualsZero(r.builder(), 0, kWasmPageSize);

  // Copy all bytes from data segment 0, to memory at [10, 20).
  CHECK_EQ(0, r.Call(10, 0, 10));
  CheckMemoryEqualsFollowedByZeroes(
      r.builder(),
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9});

  // Copy bytes in range [5, 10) from data segment 0, to memory at [0, 5).
  CHECK_EQ(0, r.Call(0, 5, 5));
  CheckMemoryEqualsFollowedByZeroes(
      r.builder(),
      {5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9});

  // Copy 0 bytes does nothing.
  CHECK_EQ(0, r.Call(10, 1, 0));
  CheckMemoryEqualsFollowedByZeroes(
      r.builder(),
      {5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9});

  // Copy 0 at end of memory region or data segment is OK.
  CHECK_EQ(0, r.Call(kWasmPageSize, 0, 0));
  CHECK_EQ(0, r.Call(0, sizeof(data), 0));
}

WASM_EXEC_TEST(MemoryInitOutOfBoundsData) {
  EXPERIMENTAL_FLAG_SCOPE(bulk_memory);
  WasmRunner<uint32_t, uint32_t, uint32_t, uint32_t> r(execution_tier);
  r.builder().AddMemory(kWasmPageSize);
  const byte data[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  r.builder().AddPassiveDataSegment(Vector<const byte>(data));
  BUILD(r,
        WASM_MEMORY_INIT(0, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1),
                         WASM_GET_LOCAL(2)),
        kExprI32Const, 0);

  const uint32_t last_5_bytes = kWasmPageSize - 5;

  // Write all values up to the out-of-bounds write.
  CHECK_EQ(0xDEADBEEF, r.Call(kWasmPageSize - 5, 0, 6));
  CheckMemoryEquals(r.builder(), last_5_bytes, {0, 1, 2, 3, 4});

  // Write all values up to the out-of-bounds read.
  r.builder().BlankMemory();
  CHECK_EQ(0xDEADBEEF, r.Call(0, 5, 6));
  CheckMemoryEqualsFollowedByZeroes(r.builder(), {5, 6, 7, 8, 9});
}

WASM_EXEC_TEST(MemoryInitOutOfBounds) {
  EXPERIMENTAL_FLAG_SCOPE(bulk_memory);
  WasmRunner<uint32_t, uint32_t, uint32_t, uint32_t> r(execution_tier);
  r.builder().AddMemory(kWasmPageSize);
  const byte data[kWasmPageSize] = {};
  r.builder().AddPassiveDataSegment(Vector<const byte>(data));
  BUILD(r,
        WASM_MEMORY_INIT(0, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1),
                         WASM_GET_LOCAL(2)),
        kExprI32Const, 0);

  // OK, copy the full data segment to memory.
  r.Call(0, 0, kWasmPageSize);

  // Source range must not be out of bounds.
  CHECK_EQ(0xDEADBEEF, r.Call(0, 1, kWasmPageSize));
  CHECK_EQ(0xDEADBEEF, r.Call(0, 1000, kWasmPageSize));
  CHECK_EQ(0xDEADBEEF, r.Call(0, kWasmPageSize, 1));

  // Destination range must not be out of bounds.
  CHECK_EQ(0xDEADBEEF, r.Call(1, 0, kWasmPageSize));
  CHECK_EQ(0xDEADBEEF, r.Call(1000, 0, kWasmPageSize));
  CHECK_EQ(0xDEADBEEF, r.Call(kWasmPageSize, 0, 1));

  // Copy 0 out-of-bounds fails.
  CHECK_EQ(0xDEADBEEF, r.Call(kWasmPageSize + 1, 0, 0));
  CHECK_EQ(0xDEADBEEF, r.Call(0, kWasmPageSize + 1, 0));

  // Make sure bounds aren't checked with 32-bit wrapping.
  CHECK_EQ(0xDEADBEEF, r.Call(1, 1, 0xFFFFFFFF));
}

WASM_EXEC_TEST(MemoryCopy) {
  EXPERIMENTAL_FLAG_SCOPE(bulk_memory);
  WasmRunner<uint32_t, uint32_t, uint32_t, uint32_t> r(execution_tier);
  byte* mem = r.builder().AddMemory(kWasmPageSize);
  BUILD(
      r,
      WASM_MEMORY_COPY(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1), WASM_GET_LOCAL(2)),
      kExprI32Const, 0);

  const byte initial[] = {0, 11, 22, 33, 44, 55, 66, 77};
  memcpy(mem, initial, sizeof(initial));

  // Copy from [1, 8] to [10, 16].
  CHECK_EQ(0, r.Call(10, 1, 8));
  CheckMemoryEqualsFollowedByZeroes(
      r.builder(),
      {0, 11, 22, 33, 44, 55, 66, 77, 0, 0, 11, 22, 33, 44, 55, 66, 77});

  // Copy 0 bytes does nothing.
  CHECK_EQ(0, r.Call(10, 2, 0));
  CheckMemoryEqualsFollowedByZeroes(
      r.builder(),
      {0, 11, 22, 33, 44, 55, 66, 77, 0, 0, 11, 22, 33, 44, 55, 66, 77});

  // Copy 0 at end of memory region is OK.
  CHECK_EQ(0, r.Call(kWasmPageSize, 0, 0));
  CHECK_EQ(0, r.Call(0, kWasmPageSize, 0));
}

WASM_EXEC_TEST(MemoryCopyOverlapping) {
  EXPERIMENTAL_FLAG_SCOPE(bulk_memory);
  WasmRunner<uint32_t, uint32_t, uint32_t, uint32_t> r(execution_tier);
  byte* mem = r.builder().AddMemory(kWasmPageSize);
  BUILD(
      r,
      WASM_MEMORY_COPY(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1), WASM_GET_LOCAL(2)),
      kExprI32Const, 0);

  const byte initial[] = {10, 20, 30};
  memcpy(mem, initial, sizeof(initial));

  // Copy from [0, 3] -> [2, 5]. The copy must not overwrite 30 before copying
  // it (i.e. cannot copy forward in this case).
  CHECK_EQ(0, r.Call(2, 0, 3));
  CheckMemoryEqualsFollowedByZeroes(r.builder(), {10, 20, 10, 20, 30});

  // Copy from [2, 5] -> [0, 3]. The copy must not write the first 10 (i.e.
  // cannot copy backward in this case).
  CHECK_EQ(0, r.Call(0, 2, 3));
  CheckMemoryEqualsFollowedByZeroes(r.builder(), {10, 20, 30, 20, 30});
}

WASM_EXEC_TEST(MemoryCopyOutOfBoundsData) {
  EXPERIMENTAL_FLAG_SCOPE(bulk_memory);
  WasmRunner<uint32_t, uint32_t, uint32_t, uint32_t> r(execution_tier);
  byte* mem = r.builder().AddMemory(kWasmPageSize);
  BUILD(
      r,
      WASM_MEMORY_COPY(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1), WASM_GET_LOCAL(2)),
      kExprI32Const, 0);

  const byte data[] = {11, 22, 33, 44, 55, 66, 77, 88};
  memcpy(mem, data, sizeof(data));

  const uint32_t last_5_bytes = kWasmPageSize - 5;

  // Write all values up to the out-of-bounds access.
  CHECK_EQ(0xDEADBEEF, r.Call(last_5_bytes, 0, 6));
  CheckMemoryEquals(r.builder(), last_5_bytes, {11, 22, 33, 44, 55});

  // Copy overlapping with destination < source. Copy will happen forwards, up
  // to the out-of-bounds access.
  r.builder().BlankMemory();
  memcpy(mem + last_5_bytes, data, 5);
  CHECK_EQ(0xDEADBEEF, r.Call(0, last_5_bytes, kWasmPageSize));
  CheckMemoryEquals(r.builder(), 0, {11, 22, 33, 44, 55});

  // Copy overlapping with source < destination. Copy would happen backwards,
  // but the first byte to copy is out-of-bounds, so no data should be written.
  r.builder().BlankMemory();
  memcpy(mem, data, 5);
  CHECK_EQ(0xDEADBEEF, r.Call(last_5_bytes, 0, kWasmPageSize));
  CheckMemoryEquals(r.builder(), last_5_bytes, {0, 0, 0, 0, 0});
}

WASM_EXEC_TEST(MemoryCopyOutOfBounds) {
  EXPERIMENTAL_FLAG_SCOPE(bulk_memory);
  WasmRunner<uint32_t, uint32_t, uint32_t, uint32_t> r(execution_tier);
  r.builder().AddMemory(kWasmPageSize);
  BUILD(
      r,
      WASM_MEMORY_COPY(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1), WASM_GET_LOCAL(2)),
      kExprI32Const, 0);

  // Copy full range is OK.
  CHECK_EQ(0, r.Call(0, 0, kWasmPageSize));

  // Source range must not be out of bounds.
  CHECK_EQ(0xDEADBEEF, r.Call(0, 1, kWasmPageSize));
  CHECK_EQ(0xDEADBEEF, r.Call(0, 1000, kWasmPageSize));
  CHECK_EQ(0xDEADBEEF, r.Call(0, kWasmPageSize, 1));

  // Destination range must not be out of bounds.
  CHECK_EQ(0xDEADBEEF, r.Call(1, 0, kWasmPageSize));
  CHECK_EQ(0xDEADBEEF, r.Call(1000, 0, kWasmPageSize));
  CHECK_EQ(0xDEADBEEF, r.Call(kWasmPageSize, 0, 1));

  // Copy 0 out-of-bounds fails.
  CHECK_EQ(0xDEADBEEF, r.Call(kWasmPageSize + 1, 0, 0));
  CHECK_EQ(0xDEADBEEF, r.Call(0, kWasmPageSize + 1, 0));

  // Make sure bounds aren't checked with 32-bit wrapping.
  CHECK_EQ(0xDEADBEEF, r.Call(1, 1, 0xFFFFFFFF));
}

WASM_EXEC_TEST(MemoryFill) {
  EXPERIMENTAL_FLAG_SCOPE(bulk_memory);
  WasmRunner<uint32_t, uint32_t, uint32_t, uint32_t> r(execution_tier);
  r.builder().AddMemory(kWasmPageSize);
  BUILD(
      r,
      WASM_MEMORY_FILL(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1), WASM_GET_LOCAL(2)),
      kExprI32Const, 0);
  CHECK_EQ(0, r.Call(1, 33, 5));
  CheckMemoryEqualsFollowedByZeroes(r.builder(), {0, 33, 33, 33, 33, 33});

  CHECK_EQ(0, r.Call(4, 66, 4));
  CheckMemoryEqualsFollowedByZeroes(r.builder(),
                                    {0, 33, 33, 33, 66, 66, 66, 66});

  // Fill 0 bytes does nothing.
  CHECK_EQ(0, r.Call(4, 66, 0));
  CheckMemoryEqualsFollowedByZeroes(r.builder(),
                                    {0, 33, 33, 33, 66, 66, 66, 66});

  // Fill 0 at end of memory region is OK.
  CHECK_EQ(0, r.Call(kWasmPageSize, 66, 0));
}

WASM_EXEC_TEST(MemoryFillValueWrapsToByte) {
  EXPERIMENTAL_FLAG_SCOPE(bulk_memory);
  WasmRunner<uint32_t, uint32_t, uint32_t, uint32_t> r(execution_tier);
  r.builder().AddMemory(kWasmPageSize);
  BUILD(
      r,
      WASM_MEMORY_FILL(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1), WASM_GET_LOCAL(2)),
      kExprI32Const, 0);
  CHECK_EQ(0, r.Call(0, 1000, 3));
  const byte expected = 1000 & 255;
  CheckMemoryEqualsFollowedByZeroes(r.builder(),
                                    {expected, expected, expected});
}

WASM_EXEC_TEST(MemoryFillOutOfBoundsData) {
  EXPERIMENTAL_FLAG_SCOPE(bulk_memory);
  WasmRunner<uint32_t, uint32_t, uint32_t, uint32_t> r(execution_tier);
  r.builder().AddMemory(kWasmPageSize);
  BUILD(
      r,
      WASM_MEMORY_FILL(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1), WASM_GET_LOCAL(2)),
      kExprI32Const, 0);
  const byte v = 123;
  CHECK_EQ(0xDEADBEEF, r.Call(kWasmPageSize - 5, v, 999));
  CheckMemoryEquals(r.builder(), kWasmPageSize - 6, {0, v, v, v, v, v});
}

WASM_EXEC_TEST(MemoryFillOutOfBounds) {
  EXPERIMENTAL_FLAG_SCOPE(bulk_memory);
  WasmRunner<uint32_t, uint32_t, uint32_t, uint32_t> r(execution_tier);
  r.builder().AddMemory(kWasmPageSize);
  BUILD(
      r,
      WASM_MEMORY_FILL(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1), WASM_GET_LOCAL(2)),
      kExprI32Const, 0);

  const byte v = 123;

  // Destination range must not be out of bounds.
  CHECK_EQ(0xDEADBEEF, r.Call(1, v, kWasmPageSize));
  CHECK_EQ(0xDEADBEEF, r.Call(1000, v, kWasmPageSize));
  CHECK_EQ(0xDEADBEEF, r.Call(kWasmPageSize, v, 1));

  // Fill 0 out-of-bounds fails.
  CHECK_EQ(0xDEADBEEF, r.Call(kWasmPageSize + 1, v, 0));

  // Make sure bounds aren't checked with 32-bit wrapping.
  CHECK_EQ(0xDEADBEEF, r.Call(1, v, 0xFFFFFFFF));
}

WASM_EXEC_TEST(DataDropTwice) {
  EXPERIMENTAL_FLAG_SCOPE(bulk_memory);
  WasmRunner<uint32_t> r(execution_tier);
  r.builder().AddMemory(kWasmPageSize);
  const byte data[] = {0};
  r.builder().AddPassiveDataSegment(Vector<const byte>(data));
  BUILD(r, WASM_DATA_DROP(0), kExprI32Const, 0);

  CHECK_EQ(0, r.Call());
  CHECK_EQ(0xDEADBEEF, r.Call());
}

WASM_EXEC_TEST(DataDropThenMemoryInit) {
  EXPERIMENTAL_FLAG_SCOPE(bulk_memory);
  WasmRunner<uint32_t> r(execution_tier);
  r.builder().AddMemory(kWasmPageSize);
  const byte data[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  r.builder().AddPassiveDataSegment(Vector<const byte>(data));
  BUILD(r, WASM_DATA_DROP(0),
        WASM_MEMORY_INIT(0, WASM_I32V_1(0), WASM_I32V_1(1), WASM_I32V_1(2)),
        kExprI32Const, 0);

  CHECK_EQ(0xDEADBEEF, r.Call());
}

}  // namespace test_run_wasm_bulk_memory
}  // namespace wasm
}  // namespace internal
}  // namespace v8
