// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/ast/ast-value-factory.h"
#include "src/handles-inl.h"
#include "src/heap/factory.h"
#include "src/interpreter/constant-array-builder.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace interpreter {

class ConstantArrayBuilderTest : public TestWithIsolateAndZone {
 public:
  ConstantArrayBuilderTest() {}
  ~ConstantArrayBuilderTest() override {}

  static const size_t k8BitCapacity = ConstantArrayBuilder::k8BitCapacity;
  static const size_t k16BitCapacity = ConstantArrayBuilder::k16BitCapacity;
};

STATIC_CONST_MEMBER_DEFINITION const size_t
    ConstantArrayBuilderTest::k16BitCapacity;
STATIC_CONST_MEMBER_DEFINITION const size_t
    ConstantArrayBuilderTest::k8BitCapacity;

TEST_F(ConstantArrayBuilderTest, AllocateAllEntries) {
  CanonicalHandleScope canonical(isolate());
  ConstantArrayBuilder builder(zone());
  AstValueFactory ast_factory(zone(), isolate()->ast_string_constants(),
                              isolate()->heap()->HashSeed());
  for (size_t i = 0; i < k16BitCapacity; i++) {
    builder.Insert(i + 0.5);
  }
  CHECK_EQ(builder.size(), k16BitCapacity);
  ast_factory.Internalize(isolate(), Handle<String>());
  for (size_t i = 0; i < k16BitCapacity; i++) {
    CHECK_EQ(
        Handle<HeapNumber>::cast(builder.At(i, isolate()).ToHandleChecked())
            ->value(),
        i + 0.5);
  }
}

TEST_F(ConstantArrayBuilderTest, ToFixedArray) {
  CanonicalHandleScope canonical(isolate());
  ConstantArrayBuilder builder(zone());
  static const int kNumberOfElements = 37;
  for (int i = 0; i < kNumberOfElements; i++) {
    builder.Insert(i + 0.5);
  }
  Handle<FixedArray> constant_array = builder.ToFixedArray(isolate());
  ASSERT_EQ(kNumberOfElements, constant_array->length());
  for (int i = 0; i < kNumberOfElements; i++) {
    Handle<Object> actual(constant_array->get(i), isolate());
    Handle<Object> expected = builder.At(i, isolate()).ToHandleChecked();
    ASSERT_EQ(expected->Number(), actual->Number()) << "Failure at index " << i;
  }
}

TEST_F(ConstantArrayBuilderTest, ToLargeFixedArray) {
  CanonicalHandleScope canonical(isolate());
  ConstantArrayBuilder builder(zone());
  static const int kNumberOfElements = 37373;
  for (int i = 0; i < kNumberOfElements; i++) {
    builder.Insert(i + 0.5);
  }
  Handle<FixedArray> constant_array = builder.ToFixedArray(isolate());
  ASSERT_EQ(kNumberOfElements, constant_array->length());
  for (int i = 0; i < kNumberOfElements; i++) {
    Handle<Object> actual(constant_array->get(i), isolate());
    Handle<Object> expected = builder.At(i, isolate()).ToHandleChecked();
    ASSERT_EQ(expected->Number(), actual->Number()) << "Failure at index " << i;
  }
}

TEST_F(ConstantArrayBuilderTest, ToLargeFixedArrayWithReservations) {
  CanonicalHandleScope canonical(isolate());
  ConstantArrayBuilder builder(zone());
  AstValueFactory ast_factory(zone(), isolate()->ast_string_constants(),
                              isolate()->heap()->HashSeed());
  static const int kNumberOfElements = 37373;
  for (int i = 0; i < kNumberOfElements; i++) {
    builder.CommitReservedEntry(builder.CreateReservedEntry(), Smi::FromInt(i));
  }
  ast_factory.Internalize(isolate(), Handle<String>());
  Handle<FixedArray> constant_array = builder.ToFixedArray(isolate());
  ASSERT_EQ(kNumberOfElements, constant_array->length());
  for (int i = 0; i < kNumberOfElements; i++) {
    Handle<Object> actual(constant_array->get(i), isolate());
    Handle<Object> expected = builder.At(i, isolate()).ToHandleChecked();
    ASSERT_EQ(expected->Number(), actual->Number()) << "Failure at index " << i;
  }
}

TEST_F(ConstantArrayBuilderTest, AllocateEntriesWithIdx8Reservations) {
  CanonicalHandleScope canonical(isolate());
  for (size_t reserved = 1; reserved < k8BitCapacity; reserved *= 3) {
    ConstantArrayBuilder builder(zone());
    AstValueFactory ast_factory(zone(), isolate()->ast_string_constants(),
                                isolate()->heap()->HashSeed());
    for (size_t i = 0; i < reserved; i++) {
      OperandSize operand_size = builder.CreateReservedEntry();
      CHECK_EQ(operand_size, OperandSize::kByte);
    }
    for (size_t i = 0; i < 2 * k8BitCapacity; i++) {
      builder.CommitReservedEntry(builder.CreateReservedEntry(),
                                  Smi::FromInt(static_cast<int>(i)));
      if (i + reserved < k8BitCapacity) {
        CHECK_LE(builder.size(), k8BitCapacity);
        CHECK_EQ(builder.size(), i + 1);
      } else {
        CHECK_GE(builder.size(), k8BitCapacity);
        CHECK_EQ(builder.size(), i + reserved + 1);
      }
    }
    CHECK_EQ(builder.size(), 2 * k8BitCapacity + reserved);

    // Commit reserved entries with duplicates and check size does not change.
    DCHECK_EQ(reserved + 2 * k8BitCapacity, builder.size());
    size_t duplicates_in_idx8_space =
        std::min(reserved, k8BitCapacity - reserved);
    for (size_t i = 0; i < duplicates_in_idx8_space; i++) {
      builder.CommitReservedEntry(OperandSize::kByte,
                                  Smi::FromInt(static_cast<int>(i)));
      DCHECK_EQ(reserved + 2 * k8BitCapacity, builder.size());
    }

    // Now make reservations, and commit them with unique entries.
    for (size_t i = 0; i < duplicates_in_idx8_space; i++) {
      OperandSize operand_size = builder.CreateReservedEntry();
      CHECK_EQ(operand_size, OperandSize::kByte);
    }
    for (size_t i = 0; i < duplicates_in_idx8_space; i++) {
      Smi* value = Smi::FromInt(static_cast<int>(2 * k8BitCapacity + i));
      size_t index = builder.CommitReservedEntry(OperandSize::kByte, value);
      CHECK_EQ(index, k8BitCapacity - reserved + i);
    }

    // Clear any remaining uncommited reservations.
    for (size_t i = 0; i < reserved - duplicates_in_idx8_space; i++) {
      builder.DiscardReservedEntry(OperandSize::kByte);
    }

    ast_factory.Internalize(isolate(), Handle<String>());
    Handle<FixedArray> constant_array = builder.ToFixedArray(isolate());
    CHECK_EQ(constant_array->length(),
             static_cast<int>(2 * k8BitCapacity + reserved));

    // Check all committed values match expected
    for (size_t i = 0; i < k8BitCapacity - reserved; i++) {
      Object* value = constant_array->get(static_cast<int>(i));
      Smi* smi = Smi::FromInt(static_cast<int>(i));
      CHECK(value->SameValue(smi));
    }
    for (size_t i = k8BitCapacity; i < 2 * k8BitCapacity + reserved; i++) {
      Object* value = constant_array->get(static_cast<int>(i));
      Smi* smi = Smi::FromInt(static_cast<int>(i - reserved));
      CHECK(value->SameValue(smi));
    }
  }
}

TEST_F(ConstantArrayBuilderTest, AllocateEntriesWithWideReservations) {
  CanonicalHandleScope canonical(isolate());
  for (size_t reserved = 1; reserved < k8BitCapacity; reserved *= 3) {
    ConstantArrayBuilder builder(zone());
    AstValueFactory ast_factory(zone(), isolate()->ast_string_constants(),
                                isolate()->heap()->HashSeed());
    for (size_t i = 0; i < k8BitCapacity; i++) {
      builder.CommitReservedEntry(builder.CreateReservedEntry(),
                                  Smi::FromInt(static_cast<int>(i)));
      CHECK_EQ(builder.size(), i + 1);
    }
    for (size_t i = 0; i < reserved; i++) {
      OperandSize operand_size = builder.CreateReservedEntry();
      CHECK_EQ(operand_size, OperandSize::kShort);
      CHECK_EQ(builder.size(), k8BitCapacity);
    }
    for (size_t i = 0; i < reserved; i++) {
      builder.DiscardReservedEntry(OperandSize::kShort);
      CHECK_EQ(builder.size(), k8BitCapacity);
    }
    for (size_t i = 0; i < reserved; i++) {
      OperandSize operand_size = builder.CreateReservedEntry();
      CHECK_EQ(operand_size, OperandSize::kShort);
      builder.CommitReservedEntry(operand_size,
                                  Smi::FromInt(static_cast<int>(i)));
      CHECK_EQ(builder.size(), k8BitCapacity);
    }
    for (size_t i = k8BitCapacity; i < k8BitCapacity + reserved; i++) {
      OperandSize operand_size = builder.CreateReservedEntry();
      CHECK_EQ(operand_size, OperandSize::kShort);
      builder.CommitReservedEntry(operand_size,
                                  Smi::FromInt(static_cast<int>(i)));
      CHECK_EQ(builder.size(), i + 1);
    }

    ast_factory.Internalize(isolate(), Handle<String>());
    Handle<FixedArray> constant_array = builder.ToFixedArray(isolate());
    CHECK_EQ(constant_array->length(),
             static_cast<int>(k8BitCapacity + reserved));
    for (size_t i = 0; i < k8BitCapacity + reserved; i++) {
      Object* value = constant_array->get(static_cast<int>(i));
      CHECK(value->SameValue(*isolate()->factory()->NewNumberFromSize(i)));
    }
  }
}

TEST_F(ConstantArrayBuilderTest, GapFilledWhenLowReservationCommitted) {
  CanonicalHandleScope canonical(isolate());
  ConstantArrayBuilder builder(zone());
  AstValueFactory ast_factory(zone(), isolate()->ast_string_constants(),
                              isolate()->heap()->HashSeed());
  for (size_t i = 0; i < k8BitCapacity; i++) {
    OperandSize operand_size = builder.CreateReservedEntry();
    CHECK_EQ(OperandSize::kByte, operand_size);
    CHECK_EQ(builder.size(), 0u);
  }
  for (size_t i = 0; i < k8BitCapacity; i++) {
    builder.CommitReservedEntry(builder.CreateReservedEntry(),
                                Smi::FromInt(static_cast<int>(i)));
    CHECK_EQ(builder.size(), i + k8BitCapacity + 1);
  }
  for (size_t i = 0; i < k8BitCapacity; i++) {
    builder.CommitReservedEntry(OperandSize::kByte,
                                Smi::FromInt(static_cast<int>(i)));
    CHECK_EQ(builder.size(), 2 * k8BitCapacity);
  }
  ast_factory.Internalize(isolate(), Handle<String>());
  Handle<FixedArray> constant_array = builder.ToFixedArray(isolate());
  CHECK_EQ(constant_array->length(), static_cast<int>(2 * k8BitCapacity));
  for (size_t i = 0; i < k8BitCapacity; i++) {
    Object* original = constant_array->get(static_cast<int>(k8BitCapacity + i));
    Object* duplicate = constant_array->get(static_cast<int>(i));
    CHECK(original->SameValue(duplicate));
    Handle<Object> reference = isolate()->factory()->NewNumberFromSize(i);
    CHECK(original->SameValue(*reference));
  }
}

TEST_F(ConstantArrayBuilderTest, GapNotFilledWhenLowReservationDiscarded) {
  CanonicalHandleScope canonical(isolate());
  ConstantArrayBuilder builder(zone());
  for (size_t i = 0; i < k8BitCapacity; i++) {
    OperandSize operand_size = builder.CreateReservedEntry();
    CHECK_EQ(OperandSize::kByte, operand_size);
    CHECK_EQ(builder.size(), 0u);
  }
  double values[k8BitCapacity];
  for (size_t i = 0; i < k8BitCapacity; i++) {
    values[i] = i + 0.5;
  }

  for (size_t i = 0; i < k8BitCapacity; i++) {
    builder.Insert(values[i]);
    CHECK_EQ(builder.size(), i + k8BitCapacity + 1);
  }
  for (size_t i = 0; i < k8BitCapacity; i++) {
    builder.DiscardReservedEntry(OperandSize::kByte);
    builder.Insert(values[i]);
    CHECK_EQ(builder.size(), 2 * k8BitCapacity);
  }
  for (size_t i = 0; i < k8BitCapacity; i++) {
    Handle<Object> reference = isolate()->factory()->NewNumber(i + 0.5);
    Handle<Object> original =
        builder.At(k8BitCapacity + i, isolate()).ToHandleChecked();
    CHECK(original->SameValue(*reference));
    MaybeHandle<Object> duplicate = builder.At(i, isolate());
    CHECK(duplicate.is_null());
  }
}

TEST_F(ConstantArrayBuilderTest, HolesWithUnusedReservations) {
  CanonicalHandleScope canonical(isolate());
  static int kNumberOfHoles = 128;
  static int k8BitCapacity = ConstantArrayBuilder::k8BitCapacity;
  ConstantArrayBuilder builder(zone());
  AstValueFactory ast_factory(zone(), isolate()->ast_string_constants(),
                              isolate()->heap()->HashSeed());
  for (int i = 0; i < kNumberOfHoles; ++i) {
    CHECK_EQ(builder.CreateReservedEntry(), OperandSize::kByte);
  }
  // Values are placed before the reserved entries in the same slice.
  for (int i = 0; i < k8BitCapacity - kNumberOfHoles; ++i) {
    CHECK_EQ(builder.Insert(i + 0.5), static_cast<size_t>(i));
  }
  // The next value is pushed into the next slice.
  CHECK_EQ(builder.Insert(k8BitCapacity + 0.5), k8BitCapacity);

  // Discard the reserved entries.
  for (int i = 0; i < kNumberOfHoles; ++i) {
    builder.DiscardReservedEntry(OperandSize::kByte);
  }

  ast_factory.Internalize(isolate(), Handle<String>());
  Handle<FixedArray> constant_array = builder.ToFixedArray(isolate());
  CHECK_EQ(constant_array->length(), k8BitCapacity + 1);
  for (int i = kNumberOfHoles; i < k8BitCapacity; i++) {
    CHECK(constant_array->get(i)->SameValue(
        *isolate()->factory()->the_hole_value()));
  }
  CHECK(!constant_array->get(kNumberOfHoles - 1)
             ->SameValue(*isolate()->factory()->the_hole_value()));
  CHECK(!constant_array->get(k8BitCapacity)
             ->SameValue(*isolate()->factory()->the_hole_value()));
}

TEST_F(ConstantArrayBuilderTest, ReservationsAtAllScales) {
  CanonicalHandleScope canonical(isolate());
  ConstantArrayBuilder builder(zone());
  AstValueFactory ast_factory(zone(), isolate()->ast_string_constants(),
                              isolate()->heap()->HashSeed());
  for (int i = 0; i < 256; i++) {
    CHECK_EQ(builder.CreateReservedEntry(), OperandSize::kByte);
  }
  for (int i = 256; i < 65536; ++i) {
    CHECK_EQ(builder.CreateReservedEntry(), OperandSize::kShort);
  }
  for (int i = 65536; i < 131072; ++i) {
    CHECK_EQ(builder.CreateReservedEntry(), OperandSize::kQuad);
  }
  CHECK_EQ(builder.CommitReservedEntry(OperandSize::kByte, Smi::FromInt(1)),
           0u);
  CHECK_EQ(builder.CommitReservedEntry(OperandSize::kShort, Smi::FromInt(2)),
           256u);
  CHECK_EQ(builder.CommitReservedEntry(OperandSize::kQuad, Smi::FromInt(3)),
           65536u);
  for (int i = 1; i < 256; i++) {
    builder.DiscardReservedEntry(OperandSize::kByte);
  }
  for (int i = 257; i < 65536; ++i) {
    builder.DiscardReservedEntry(OperandSize::kShort);
  }
  for (int i = 65537; i < 131072; ++i) {
    builder.DiscardReservedEntry(OperandSize::kQuad);
  }

  ast_factory.Internalize(isolate(), Handle<String>());
  Handle<FixedArray> constant_array = builder.ToFixedArray(isolate());
  CHECK_EQ(constant_array->length(), 65537);
  int count = 1;
  for (int i = 0; i < constant_array->length(); ++i) {
    Handle<Object> expected;
    if (i == 0 || i == 256 || i == 65536) {
      expected = isolate()->factory()->NewNumber(count++);
    } else {
      expected = isolate()->factory()->the_hole_value();
    }
    CHECK(constant_array->get(i)->SameValue(*expected));
  }
}

TEST_F(ConstantArrayBuilderTest, AllocateEntriesWithFixedReservations) {
  CanonicalHandleScope canonical(isolate());
  ConstantArrayBuilder builder(zone());
  for (size_t i = 0; i < k16BitCapacity; i++) {
    if ((i % 2) == 0) {
      CHECK_EQ(i, builder.InsertDeferred());
    } else {
      builder.Insert(Smi::FromInt(static_cast<int>(i)));
    }
  }
  CHECK_EQ(builder.size(), k16BitCapacity);

  // Check values before reserved entries are inserted.
  for (size_t i = 0; i < k16BitCapacity; i++) {
    if ((i % 2) == 0) {
      // Check reserved values are null.
      MaybeHandle<Object> empty = builder.At(i, isolate());
      CHECK(empty.is_null());
    } else {
      CHECK_EQ(Handle<Smi>::cast(builder.At(i, isolate()).ToHandleChecked())
                   ->value(),
               static_cast<int>(i));
    }
  }

  // Insert reserved entries.
  for (size_t i = 0; i < k16BitCapacity; i += 2) {
    builder.SetDeferredAt(i,
                          handle(Smi::FromInt(static_cast<int>(i)), isolate()));
  }

  // Check values after reserved entries are inserted.
  for (size_t i = 0; i < k16BitCapacity; i++) {
    CHECK_EQ(
        Handle<Smi>::cast(builder.At(i, isolate()).ToHandleChecked())->value(),
        static_cast<int>(i));
  }
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
