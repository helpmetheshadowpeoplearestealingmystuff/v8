// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_ISOLATE_DATA_H_
#define V8_EXECUTION_ISOLATE_DATA_H_

#include "src/builtins/builtins.h"
#include "src/codegen/constants-arch.h"
#include "src/codegen/external-reference-table.h"
#include "src/execution/stack-guard.h"
#include "src/execution/thread-local-top.h"
#include "src/heap/linear-allocation-area.h"
#include "src/roots/roots.h"
#include "src/sandbox/external-pointer-table.h"
#include "src/utils/utils.h"
#include "testing/gtest/include/gtest/gtest_prod.h"  // nogncheck

namespace v8 {
namespace internal {

class Isolate;

// IsolateData fields, defined as: V(Offset, Size, Name)
#define ISOLATE_DATA_FIELDS(V)                                                \
  /* Misc. fields. */                                                         \
  V(kCageBaseOffset, kSystemPointerSize, cage_base)                           \
  V(kStackGuardOffset, StackGuard::kSizeInBytes, stack_guard)                 \
  /* Tier 0 tables (small but fast access). */                                \
  V(kBuiltinTier0EntryTableOffset,                                            \
    Builtins::kBuiltinTier0Count* kSystemPointerSize,                         \
    builtin_tier0_entry_table)                                                \
  V(kBuiltinsTier0TableOffset,                                                \
    Builtins::kBuiltinTier0Count* kSystemPointerSize, builtin_tier0_table)    \
  /* Misc. fields. */                                                         \
  V(kEmbedderDataOffset, Internals::kNumIsolateDataSlots* kSystemPointerSize, \
    embedder_data)                                                            \
  V(kFastCCallCallerFPOffset, kSystemPointerSize, fast_c_call_caller_fp)      \
  V(kFastCCallCallerPCOffset, kSystemPointerSize, fast_c_call_caller_pc)      \
  V(kFastApiCallTargetOffset, kSystemPointerSize, fast_api_call_target)       \
  V(kLongTaskStatsCounterOffset, kSizetSize, long_task_stats_counter)         \
  /* Full tables (arbitrary size, potentially slower access). */              \
  V(kRootsTableOffset, RootsTable::kEntriesCount* kSystemPointerSize,         \
    roots_table)                                                              \
  V(kExternalReferenceTableOffset, ExternalReferenceTable::kSizeInBytes,      \
    external_reference_table)                                                 \
  V(kThreadLocalTopOffset, ThreadLocalTop::kSizeInBytes, thread_local_top)    \
  V(kBuiltinEntryTableOffset, Builtins::kBuiltinCount* kSystemPointerSize,    \
    builtin_entry_table)                                                      \
  V(kBuiltinTableOffset, Builtins::kBuiltinCount* kSystemPointerSize,         \
    builtin_table)                                                            \
  /* Linear allocation areas for the heap's new and old space */              \
  V(kNewAllocationInfo, LinearAllocationArea::kSize, new_allocation_info)     \
  V(kOldAllocationInfo, LinearAllocationArea::kSize, old_allocation_info)     \
  ISOLATE_DATA_FIELDS_EXTERNAL_CODE_SPACE(V)                                  \
  ISOLATE_DATA_FIELDS_HEAP_SANDBOX(V)                                         \
  V(kStackIsIterableOffset, kUInt8Size, stack_is_iterable)

#ifdef V8_EXTERNAL_CODE_SPACE
#define ISOLATE_DATA_FIELDS_EXTERNAL_CODE_SPACE(V) \
  V(kBuiltinCodeDataContainerTableOffset,          \
    Builtins::kBuiltinCount* kSystemPointerSize,   \
    builtin_code_data_container_table)
#else
#define ISOLATE_DATA_FIELDS_EXTERNAL_CODE_SPACE(V)
#endif  // V8_EXTERNAL_CODE_SPACE

#ifdef V8_SANDBOXED_EXTERNAL_POINTERS
#define ISOLATE_DATA_FIELDS_HEAP_SANDBOX(V) \
  V(kExternalPointerTableOffset, kSystemPointerSize * 3, external_pointer_table)
#else
#define ISOLATE_DATA_FIELDS_HEAP_SANDBOX(V)
#endif  // V8_SANDBOXED_EXTERNAL_POINTERS

// This class contains a collection of data accessible from both C++ runtime
// and compiled code (including builtins, interpreter bytecode handlers and
// optimized code). The compiled code accesses the isolate data fields
// indirectly via the root register.
class IsolateData final {
 public:
  IsolateData(Isolate* isolate, Address cage_base)
      : cage_base_(cage_base), stack_guard_(isolate) {}

  IsolateData(const IsolateData&) = delete;
  IsolateData& operator=(const IsolateData&) = delete;

  static constexpr intptr_t kIsolateRootBias = kRootRegisterBias;

  // The value of the kRootRegister.
  Address isolate_root() const {
    return reinterpret_cast<Address>(this) + kIsolateRootBias;
  }

  // Root-register-relative offsets.

#define V(Offset, Size, Name) \
  static constexpr int Name##_offset() { return Offset - kIsolateRootBias; }
  ISOLATE_DATA_FIELDS(V)
#undef V

  static constexpr int root_slot_offset(RootIndex root_index) {
    return roots_table_offset() + RootsTable::offset_of(root_index);
  }

  static constexpr int BuiltinEntrySlotOffset(Builtin id) {
    DCHECK(Builtins::IsBuiltinId(id));
    return (Builtins::IsTier0(id) ? builtin_tier0_entry_table_offset()
                                  : builtin_entry_table_offset()) +
           Builtins::ToInt(id) * kSystemPointerSize;
  }
  // TODO(ishell): remove in favour of typified id version.
  static constexpr int builtin_slot_offset(int builtin_index) {
    return BuiltinSlotOffset(Builtins::FromInt(builtin_index));
  }
  static constexpr int BuiltinSlotOffset(Builtin id) {
    return (Builtins::IsTier0(id) ? builtin_tier0_table_offset()
                                  : builtin_table_offset()) +
           Builtins::ToInt(id) * kSystemPointerSize;
  }

  static int BuiltinCodeDataContainerSlotOffset(Builtin id) {
#ifdef V8_EXTERNAL_CODE_SPACE
    // TODO(v8:11880): implement table tiering once the builtin table containing
    // Code objects is no longer used.
    return builtin_code_data_container_table_offset() +
           Builtins::ToInt(id) * kSystemPointerSize;
#else
    UNREACHABLE();
#endif  // V8_EXTERNAL_CODE_SPACE
  }

#define V(Offset, Size, Name) \
  Address Name##_address() { return reinterpret_cast<Address>(&Name##_); }
  ISOLATE_DATA_FIELDS(V)
#undef V

  Address fast_c_call_caller_fp() const { return fast_c_call_caller_fp_; }
  Address fast_c_call_caller_pc() const { return fast_c_call_caller_pc_; }
  Address fast_api_call_target() const { return fast_api_call_target_; }
  // The value of kPointerCageBaseRegister.
  Address cage_base() const { return cage_base_; }
  StackGuard* stack_guard() { return &stack_guard_; }
  Address* builtin_tier0_entry_table() { return builtin_tier0_entry_table_; }
  Address* builtin_tier0_table() { return builtin_tier0_table_; }
  RootsTable& roots() { return roots_table_; }
  const RootsTable& roots() const { return roots_table_; }
  ExternalReferenceTable* external_reference_table() {
    return &external_reference_table_;
  }
  ThreadLocalTop& thread_local_top() { return thread_local_top_; }
  ThreadLocalTop const& thread_local_top() const { return thread_local_top_; }
  Address* builtin_entry_table() { return builtin_entry_table_; }
  Address* builtin_table() { return builtin_table_; }
  Address* builtin_code_data_container_table() {
#ifdef V8_EXTERNAL_CODE_SPACE
    return builtin_code_data_container_table_;
#else
    UNREACHABLE();
#endif
  }
  uint8_t stack_is_iterable() const { return stack_is_iterable_; }

  // Returns true if this address points to data stored in this instance. If
  // it's the case then the value can be accessed indirectly through the root
  // register.
  bool contains(Address address) const {
    STATIC_ASSERT(std::is_unsigned<Address>::value);
    Address start = reinterpret_cast<Address>(this);
    return (address - start) < sizeof(*this);
  }

 private:
  // Static layout definition.
  //
  // Note: The location of fields within IsolateData is significant. The
  // closer they are to the value of kRootRegister (i.e.: isolate_root()), the
  // cheaper it is to access them. See also: https://crbug.com/993264.
  // The recommended guideline is to put frequently-accessed fields close to
  // the beginning of IsolateData.
#define FIELDS(V)                                                      \
  ISOLATE_DATA_FIELDS(V)                                               \
  /* This padding aligns IsolateData size by 8 bytes. */               \
  V(kPaddingOffset,                                                    \
    8 + RoundUp<8>(static_cast<int>(kPaddingOffset)) - kPaddingOffset) \
  /* Total size. */                                                    \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(0, FIELDS)
#undef FIELDS

  const Address cage_base_;

  // Fields related to the system and JS stack. In particular, this contains
  // the stack limit used by stack checks in generated code.
  StackGuard stack_guard_;

  // Tier 0 tables. See also builtin_entry_table_ and builtin_table_.
  Address builtin_tier0_entry_table_[Builtins::kBuiltinTier0Count] = {};
  Address builtin_tier0_table_[Builtins::kBuiltinTier0Count] = {};

  // These fields are accessed through the API, offsets must be kept in sync
  // with v8::internal::Internals (in include/v8-internal.h) constants. The
  // layout consistency is verified in Isolate::CheckIsolateLayout() using
  // runtime checks.
  void* embedder_data_[Internals::kNumIsolateDataSlots] = {};

  // Stores the state of the caller for TurboAssembler::CallCFunction so that
  // the sampling CPU profiler can iterate the stack during such calls. These
  // are stored on IsolateData so that they can be stored to with only one move
  // instruction in compiled code.
  //
  // The FP and PC that are saved right before TurboAssembler::CallCFunction.
  Address fast_c_call_caller_fp_ = kNullAddress;
  Address fast_c_call_caller_pc_ = kNullAddress;
  // The address of the fast API callback right before it's executed from
  // generated code.
  Address fast_api_call_target_ = kNullAddress;

  // Used for implementation of LongTaskStats. Counts the number of potential
  // long tasks.
  size_t long_task_stats_counter_ = 0;

  RootsTable roots_table_;
  ExternalReferenceTable external_reference_table_;

  ThreadLocalTop thread_local_top_;

  // The entry points for builtins. This corresponds to
  // Code::InstructionStart() for each Code object in the builtins table below.
  // The entry table is in IsolateData for easy access through kRootRegister.
  Address builtin_entry_table_[Builtins::kBuiltinCount] = {};

  // The entries in this array are tagged pointers to Code objects.
  Address builtin_table_[Builtins::kBuiltinCount] = {};

  LinearAllocationArea new_allocation_info_;
  LinearAllocationArea old_allocation_info_;

#ifdef V8_EXTERNAL_CODE_SPACE
  Address builtin_code_data_container_table_[Builtins::kBuiltinCount] = {};
#endif

  // Table containing pointers to external objects.
#ifdef V8_SANDBOXED_EXTERNAL_POINTERS
  ExternalPointerTable external_pointer_table_;
#endif

  // Whether the SafeStackFrameIterator can successfully iterate the current
  // stack. Only valid values are 0 or 1.
  uint8_t stack_is_iterable_ = 1;

  // Ensure the size is 8-byte aligned in order to make alignment of the field
  // following the IsolateData field predictable. This solves the issue with
  // C++ compilers for 32-bit platforms which are not consistent at aligning
  // int64_t fields.
  // In order to avoid dealing with zero-size arrays the padding size is always
  // in the range [8, 15).
  STATIC_ASSERT(kPaddingOffsetEnd + 1 - kPaddingOffset >= 8);
  char padding_[kPaddingOffsetEnd + 1 - kPaddingOffset];

  V8_INLINE static void AssertPredictableLayout();

  friend class Isolate;
  friend class Heap;
  FRIEND_TEST(HeapTest, ExternalLimitDefault);
  FRIEND_TEST(HeapTest, ExternalLimitStaysAboveDefaultForExplicitHandling);
};

// IsolateData object must have "predictable" layout which does not change when
// cross-compiling to another platform. Otherwise there may be compatibility
// issues because of different compilers used for snapshot generator and
// actual V8 code.
void IsolateData::AssertPredictableLayout() {
  STATIC_ASSERT(std::is_standard_layout<RootsTable>::value);
  STATIC_ASSERT(std::is_standard_layout<ThreadLocalTop>::value);
  STATIC_ASSERT(std::is_standard_layout<ExternalReferenceTable>::value);
  STATIC_ASSERT(std::is_standard_layout<IsolateData>::value);
#define V(Offset, Size, Name) \
  STATIC_ASSERT(offsetof(IsolateData, Name##_) == Offset);
  ISOLATE_DATA_FIELDS(V)
#undef V
  STATIC_ASSERT(sizeof(IsolateData) == IsolateData::kSize);
}

#undef ISOLATE_DATA_FIELDS_HEAP_SANDBOX
#undef ISOLATE_DATA_FIELDS

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_ISOLATE_DATA_H_
