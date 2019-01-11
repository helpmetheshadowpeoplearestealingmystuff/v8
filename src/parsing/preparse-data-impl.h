// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_PREPARSE_DATA_IMPL_H_
#define V8_PARSING_PREPARSE_DATA_IMPL_H_

#include "src/parsing/preparse-data.h"

#include "src/assert-scope.h"

namespace v8 {
namespace internal {

// Classes which are internal to prepared-scope-data.cc, but are exposed in
// a header for tests.

struct PreparseByteDataConstants {
#ifdef DEBUG
  static constexpr int kMagicValue = 0xC0DE0DE;

  static constexpr size_t kUint32Size = 5;
  static constexpr size_t kUint8Size = 2;
  static constexpr size_t kQuarterMarker = 0;
  static constexpr size_t kPlaceholderSize = kUint32Size;
#else
  static constexpr size_t kUint32Size = 4;
  static constexpr size_t kUint8Size = 1;
  static constexpr size_t kPlaceholderSize = 0;
#endif

  static const size_t kSkippableFunctionDataSize =
      4 * kUint32Size + 1 * kUint8Size;
};

class PreparseDataBuilder::ByteData : public ZoneObject,
                                      public PreparseByteDataConstants {
 public:
  explicit ByteData(Zone* zone)
      : free_quarters_in_last_byte_(0), backing_store_(zone) {}
  void WriteUint32(uint32_t data);
  void WriteUint8(uint8_t data);
  void WriteQuarter(uint8_t data);

#ifdef DEBUG
  // For overwriting previously written data at position 0.
  void OverwriteFirstUint32(uint32_t data);
#endif

  void StoreInto(PreparseData data);

  size_t size() const { return backing_store_.size(); }

  ZoneChunkList<uint8_t>::iterator begin() { return backing_store_.begin(); }

  ZoneChunkList<uint8_t>::iterator end() { return backing_store_.end(); }

 private:
  uint8_t free_quarters_in_last_byte_;
  ZoneChunkList<uint8_t> backing_store_;
};

// Wraps a ZoneVector<uint8_t> to have with functions named the same as
// PodArray<uint8_t>.
class ZoneVectorWrapper {
 public:
  ZoneVectorWrapper() = default;
  explicit ZoneVectorWrapper(ZoneVector<uint8_t>* data) : data_(data) {}

  int data_length() const { return static_cast<int>(data_->size()); }

  uint8_t get(int index) const { return data_->at(index); }

 private:
  ZoneVector<uint8_t>* data_ = nullptr;
};

template <class Data>
class BaseConsumedPreparseData : public ConsumedPreparseData {
 public:
  class ByteData : public PreparseByteDataConstants {
   public:
    ByteData() {}

    // Reading from the ByteData is only allowed when a ReadingScope is on the
    // stack. This ensures that we have a DisallowHeapAllocation in place
    // whenever ByteData holds a raw pointer into the heap.
    class ReadingScope {
     public:
      ReadingScope(ByteData* consumed_data, Data data)
          : consumed_data_(consumed_data) {
        consumed_data->data_ = data;
#ifdef DEBUG
        consumed_data->has_data_ = true;
#endif
      }
      explicit ReadingScope(BaseConsumedPreparseData<Data>* parent)
          : ReadingScope(parent->scope_data_.get(), parent->GetScopeData()) {}
      ~ReadingScope() {
#ifdef DEBUG
        consumed_data_->has_data_ = false;
#endif
      }

     private:
      ByteData* consumed_data_;
      DISALLOW_HEAP_ALLOCATION(no_gc);
    };

    void SetPosition(int position) {
      DCHECK_LE(position, data_.data_length());
      index_ = position;
    }

    size_t RemainingBytes() const {
      DCHECK(has_data_);
      DCHECK_LE(index_, data_.data_length());
      return data_.data_length() - index_;
    }

    bool HasRemainingBytes(size_t bytes) const {
      DCHECK(has_data_);
      return index_ <= data_.data_length() && bytes <= RemainingBytes();
    }

    int32_t ReadUint32() {
      DCHECK(HasRemainingBytes(kUint32Size));
      // Check that there indeed is an integer following.
      DCHECK_EQ(data_.get(index_++), kUint32Size);
      int32_t result = data_.get(index_) + (data_.get(index_ + 1) << 8) +
                       (data_.get(index_ + 2) << 16) +
                       (data_.get(index_ + 3) << 24);
      index_ += 4;
      stored_quarters_ = 0;
      return result;
    }

    uint8_t ReadUint8() {
      DCHECK(has_data_);
      DCHECK(HasRemainingBytes(kUint8Size));
      // Check that there indeed is a byte following.
      DCHECK_EQ(data_.get(index_++), kUint8Size);
      stored_quarters_ = 0;
      return data_.get(index_++);
    }

    uint8_t ReadQuarter() {
      DCHECK(has_data_);
      if (stored_quarters_ == 0) {
        DCHECK(HasRemainingBytes(kUint8Size));
        // Check that there indeed are quarters following.
        DCHECK_EQ(data_.get(index_++), kQuarterMarker);
        stored_byte_ = data_.get(index_++);
        stored_quarters_ = 4;
      }
      // Read the first 2 bits from stored_byte_.
      uint8_t result = (stored_byte_ >> 6) & 3;
      DCHECK_LE(result, 3);
      --stored_quarters_;
      stored_byte_ <<= 2;
      return result;
    }

   private:
    Data data_ = {};
    int index_ = 0;
    uint8_t stored_quarters_ = 0;
    uint8_t stored_byte_ = 0;
#ifdef DEBUG
    bool has_data_ = false;
#endif
  };

  BaseConsumedPreparseData() : scope_data_(new ByteData()), child_index_(0) {}

  virtual Data GetScopeData() = 0;

  virtual ProducedPreparseData* GetChildData(Zone* zone, int child_index) = 0;

  ProducedPreparseData* GetDataForSkippableFunction(
      Zone* zone, int start_position, int* end_position, int* num_parameters,
      int* num_inner_functions, bool* uses_super_property,
      LanguageMode* language_mode) final;

  void RestoreScopeAllocationData(DeclarationScope* scope) final;

#ifdef DEBUG
  void VerifyDataStart();
#endif

 private:
  void RestoreDataForScope(Scope* scope);
  void RestoreDataForVariable(Variable* var);
  void RestoreDataForInnerScopes(Scope* scope);

  std::unique_ptr<ByteData> scope_data_;
  // When consuming the data, these indexes point to the data we're going to
  // consume next.
  int child_index_;

  DISALLOW_COPY_AND_ASSIGN(BaseConsumedPreparseData);
};

// Implementation of ConsumedPreparseData for on-heap data.
class OnHeapConsumedPreparseData final
    : public BaseConsumedPreparseData<PreparseData> {
 public:
  OnHeapConsumedPreparseData(Isolate* isolate, Handle<PreparseData> data);

  PreparseData GetScopeData() final;
  ProducedPreparseData* GetChildData(Zone* zone, int index) final;

 private:
  Isolate* isolate_;
  Handle<PreparseData> data_;
};

// A serialized PreparseData in zone memory (as apposed to being on-heap).
class ZonePreparseData : public ZoneObject {
 public:
  ZonePreparseData(Zone* zone, PreparseDataBuilder::ByteData* byte_data,
                   int child_length);

  Handle<PreparseData> Serialize(Isolate* isolate);

  int child_length() const { return static_cast<int>(children_.size()); }

  ZonePreparseData* get_child(int index) { return children_[index]; }

  void set_child(int index, ZonePreparseData* child) {
    children_[index] = child;
  }

  ZoneVector<uint8_t>* byte_data() { return &byte_data_; }

 private:
  ZoneVector<uint8_t> byte_data_;
  ZoneVector<ZonePreparseData*> children_;

  DISALLOW_COPY_AND_ASSIGN(ZonePreparseData);
};

// Implementation of ConsumedPreparseData for PreparseData
// serialized into zone memory.
class ZoneConsumedPreparseData final
    : public BaseConsumedPreparseData<ZoneVectorWrapper> {
 public:
  ZoneConsumedPreparseData(Zone* zone, ZonePreparseData* data);

  ZoneVectorWrapper GetScopeData() final;
  ProducedPreparseData* GetChildData(Zone* zone, int child_index) final;

 private:
  ZonePreparseData* data_;
  ZoneVectorWrapper scope_data_wrapper_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_PREPARSE_DATA_IMPL_H_
