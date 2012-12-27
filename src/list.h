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

#ifndef V8_LIST_H_
#define V8_LIST_H_

#include "utils.h"

namespace v8 {
namespace internal {


// ----------------------------------------------------------------------------
// The list is a template for very light-weight lists. We are not
// using the STL because we want full control over space and speed of
// the code. This implementation is based on code by Robert Griesemer
// and Rob Pike.
//
// The list is parameterized by the type of its elements (T) and by an
// allocation policy (P). The policy is used for allocating lists in
// the C free store or the zone; see zone.h.

// Forward defined as
// template <typename T,
//           class AllocationPolicy = FreeStoreAllocationPolicy> class List;
template <typename T, class AllocationPolicy>
class List {
 public:
  explicit List(AllocationPolicy allocator = AllocationPolicy()) {
    Initialize(0, allocator);
  }
  INLINE(explicit List(int capacity,
                       AllocationPolicy allocator = AllocationPolicy())) {
    Initialize(capacity, allocator);
  }
  INLINE(~List()) { DeleteData(data_); }

  // Deallocates memory used by the list and leaves the list in a consistent
  // empty state.
  void Free() {
    DeleteData(data_);
    Initialize(0);
  }

  INLINE(void* operator new(size_t size,
                            AllocationPolicy allocator = AllocationPolicy())) {
    return allocator.New(static_cast<int>(size));
  }
  INLINE(void operator delete(void* p)) {
    AllocationPolicy::Delete(p);
  }

  // Please the MSVC compiler.  We should never have to execute this.
  INLINE(void operator delete(void* p, AllocationPolicy allocator)) {
    UNREACHABLE();
  }

  // Returns a reference to the element at index i.  This reference is
  // not safe to use after operations that can change the list's
  // backing store (e.g. Add).
  inline T& operator[](int i) const {
    ASSERT(0 <= i);
    ASSERT(i < length_);
    return data_[i];
  }
  inline T& at(int i) const { return operator[](i); }
  inline T& last() const { return at(length_ - 1); }
  inline T& first() const { return at(0); }

  INLINE(bool is_empty() const) { return length_ == 0; }
  INLINE(int length() const) { return length_; }
  INLINE(int capacity() const) { return capacity_; }

  Vector<T> ToVector() const { return Vector<T>(data_, length_); }

  Vector<const T> ToConstVector() { return Vector<const T>(data_, length_); }

  // Adds a copy of the given 'element' to the end of the list,
  // expanding the list if necessary.
  void Add(const T& element, AllocationPolicy allocator = AllocationPolicy());

  // Add all the elements from the argument list to this list.
  void AddAll(const List<T, AllocationPolicy>& other,
              AllocationPolicy allocator = AllocationPolicy());

  // Add all the elements from the vector to this list.
  void AddAll(const Vector<T>& other,
              AllocationPolicy allocator = AllocationPolicy());

  // Inserts the element at the specific index.
  void InsertAt(int index, const T& element,
                AllocationPolicy allocator = AllocationPolicy());

  // Added 'count' elements with the value 'value' and returns a
  // vector that allows access to the elements.  The vector is valid
  // until the next change is made to this list.
  Vector<T> AddBlock(T value, int count,
                     AllocationPolicy allocator = AllocationPolicy());

  // Removes the i'th element without deleting it even if T is a
  // pointer type; moves all elements above i "down". Returns the
  // removed element.  This function's complexity is linear in the
  // size of the list.
  T Remove(int i);

  // Remove the given element from the list. Returns whether or not
  // the input is included in the list in the first place.
  bool RemoveElement(const T& elm);

  // Removes the last element without deleting it even if T is a
  // pointer type. Returns the removed element.
  INLINE(T RemoveLast()) { return Remove(length_ - 1); }

  // Deletes current list contents and allocates space for 'length' elements.
  INLINE(void Allocate(int length,
                       AllocationPolicy allocator = AllocationPolicy()));

  // Clears the list by setting the length to zero. Even if T is a
  // pointer type, clearing the list doesn't delete the entries.
  INLINE(void Clear());

  // Drops all but the first 'pos' elements from the list.
  INLINE(void Rewind(int pos));

  // Drop the last 'count' elements from the list.
  INLINE(void RewindBy(int count)) { Rewind(length_ - count); }

  bool Contains(const T& elm) const;
  int CountOccurrences(const T& elm, int start, int end) const;

  // Iterate through all list entries, starting at index 0.
  void Iterate(void (*callback)(T* x));
  template<class Visitor>
  void Iterate(Visitor* visitor);

  // Sort all list entries (using QuickSort)
  void Sort(int (*cmp)(const T* x, const T* y));
  void Sort();

  INLINE(void Initialize(int capacity,
                         AllocationPolicy allocator = AllocationPolicy()));

 private:
  T* data_;
  int capacity_;
  int length_;

  INLINE(T* NewData(int n, AllocationPolicy allocator))  {
    return static_cast<T*>(allocator.New(n * sizeof(T)));
  }
  INLINE(void DeleteData(T* data))  {
    AllocationPolicy::Delete(data);
  }

  // Increase the capacity of a full list, and add an element.
  // List must be full already.
  void ResizeAdd(const T& element, AllocationPolicy allocator);

  // Inlined implementation of ResizeAdd, shared by inlined and
  // non-inlined versions of ResizeAdd.
  void ResizeAddInternal(const T& element, AllocationPolicy allocator);

  // Resize the list.
  void Resize(int new_capacity, AllocationPolicy allocator);

  DISALLOW_COPY_AND_ASSIGN(List);
};

class Map;
class Code;
template<typename T> class Handle;
typedef List<Map*> MapList;
typedef List<Code*> CodeList;
typedef List<Handle<Map> > MapHandleList;
typedef List<Handle<Code> > CodeHandleList;

// Perform binary search for an element in an already sorted
// list. Returns the index of the element of -1 if it was not found.
// |cmp| is a predicate that takes a pointer to an element of the List
// and returns +1 if it is greater, -1 if it is less than the element
// being searched.
template <typename T, class P>
int SortedListBSearch(const List<T>& list, P cmp);
template <typename T>
int SortedListBSearch(const List<T>& list, T elem);


} }  // namespace v8::internal


#endif  // V8_LIST_H_
