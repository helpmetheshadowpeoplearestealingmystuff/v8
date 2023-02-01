// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_INSTANCE_TYPE_INL_H_
#define V8_OBJECTS_INSTANCE_TYPE_INL_H_

#include "src/base/bounds.h"
#include "src/execution/isolate-utils-inl.h"
#include "src/objects/instance-type.h"
#include "src/objects/map-inl.h"
#include "src/roots/static-roots.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

namespace InstanceTypeChecker {

// INSTANCE_TYPE_CHECKERS macro defines some "types" that do not have
// respective C++ classes (see TypedArrayConstructor, FixedArrayExact) or
// the respective C++ counterpart is actually a template (see HashTable).
// So in order to be able to customize IsType() implementations for specific
// types, we declare a parallel set of "types" that can be compared using
// std::is_same<>.
namespace InstanceTypeTraits {

#define DECL_TYPE(type, ...) class type;
INSTANCE_TYPE_CHECKERS(DECL_TYPE)
TORQUE_INSTANCE_CHECKERS_MULTIPLE_FULLY_DEFINED(DECL_TYPE)
TORQUE_INSTANCE_CHECKERS_MULTIPLE_ONLY_DECLARED(DECL_TYPE)
HEAP_OBJECT_TYPE_LIST(DECL_TYPE)
#undef DECL_TYPE

}  // namespace InstanceTypeTraits

template <class type>
inline constexpr base::Optional<RootIndex> UniqueMapOfInstanceType() {
  return {};
}

#define INSTANCE_TYPE_MAP(V, rootIndexName, rootAccessorName, class_name) \
  template <>                                                             \
  inline constexpr base::Optional<RootIndex>                              \
  UniqueMapOfInstanceType<InstanceTypeTraits::class_name>() {             \
    return {RootIndex::k##rootIndexName};                                 \
  }
UNIQUE_INSTANCE_TYPE_MAP_LIST_GENERATOR(INSTANCE_TYPE_MAP, _)
#undef INSTANCE_TYPE_MAP

inline constexpr base::Optional<RootIndex> UniqueMapOfInstanceType(
    InstanceType type) {
  switch (type) {
#define INSTANCE_TYPE_CHECK(it, forinstancetype)         \
  case forinstancetype:                                  \
    return InstanceTypeChecker::UniqueMapOfInstanceType< \
        InstanceTypeChecker::InstanceTypeTraits::it>();  \
    INSTANCE_TYPE_CHECKERS_SINGLE(INSTANCE_TYPE_CHECK);
#undef INSTANCE_TYPE_CHECK
    default: {
    }
  }
  return {};
}

#if V8_STATIC_ROOTS_BOOL

inline bool CheckInstanceMap(RootIndex expected, Map map) {
  return V8HeapCompressionScheme::CompressTagged(map.ptr()) ==
         StaticReadOnlyRootsPointerTable[static_cast<size_t>(expected)];
}

inline bool CheckInstanceMapRange(std::pair<RootIndex, RootIndex> expected,
                                  Map map) {
  Tagged_t ptr = V8HeapCompressionScheme::CompressTagged(map.ptr());
  Tagged_t first =
      StaticReadOnlyRootsPointerTable[static_cast<size_t>(expected.first)];
  Tagged_t last =
      StaticReadOnlyRootsPointerTable[static_cast<size_t>(expected.second)];
  return ptr >= first && ptr <= last;
}

#endif  // V8_STATIC_ROOTS_BOOL

// Define type checkers for classes with single instance type.
// INSTANCE_TYPE_CHECKER1 is to be used if the instance type is already loaded.
// INSTANCE_TYPE_CHECKER2 is preferred since it can sometimes avoid loading the
// instance type from the map, if the checked instance type corresponds to a
// known map or range of maps.

#define INSTANCE_TYPE_CHECKER1(type, forinstancetype)             \
  V8_INLINE constexpr bool Is##type(InstanceType instance_type) { \
    return instance_type == forinstancetype;                      \
  }

#if V8_STATIC_ROOTS_BOOL

#define INSTANCE_TYPE_CHECKER2(type, forinstancetype)              \
  V8_INLINE bool Is##type(Map map_object) {                        \
    if (base::Optional<RootIndex> expected =                       \
            UniqueMapOfInstanceType<InstanceTypeTraits::type>()) { \
      bool res = CheckInstanceMap(*expected, map_object);          \
      SLOW_DCHECK(Is##type(map_object.instance_type()) == res);    \
      return res;                                                  \
    }                                                              \
    if (base::Optional<std::pair<RootIndex, RootIndex>> range =    \
            StaticReadOnlyRootMapRange(forinstancetype)) {         \
      bool res = CheckInstanceMapRange(*range, map_object);        \
      SLOW_DCHECK(Is##type(map_object.instance_type()) == res);    \
      return res;                                                  \
    }                                                              \
    return Is##type(map_object.instance_type());                   \
  }

#else

#define INSTANCE_TYPE_CHECKER2(type, forinstancetype) \
  V8_INLINE bool Is##type(Map map_object) {           \
    return Is##type(map_object.instance_type());      \
  }

#endif  // V8_STATIC_ROOTS_BOOL

INSTANCE_TYPE_CHECKERS_SINGLE(INSTANCE_TYPE_CHECKER1)
INSTANCE_TYPE_CHECKERS_SINGLE(INSTANCE_TYPE_CHECKER2)
#undef INSTANCE_TYPE_CHECKER1
#undef INSTANCE_TYPE_CHECKER2

// Checks if value is in range [lower_limit, higher_limit] using a single
// branch. Assumes that the input instance type is valid.
template <InstanceType lower_limit, InstanceType upper_limit>
struct InstanceRangeChecker {
  static constexpr bool Check(InstanceType value) {
    return base::IsInRange(value, lower_limit, upper_limit);
  }
};
template <InstanceType upper_limit>
struct InstanceRangeChecker<FIRST_TYPE, upper_limit> {
  static constexpr bool Check(InstanceType value) {
    DCHECK_LE(FIRST_TYPE, value);
    return value <= upper_limit;
  }
};
template <InstanceType lower_limit>
struct InstanceRangeChecker<lower_limit, LAST_TYPE> {
  static constexpr bool Check(InstanceType value) {
    DCHECK_GE(LAST_TYPE, value);
    return value >= lower_limit;
  }
};

// Define type checkers for classes with ranges of instance types.
// INSTANCE_TYPE_CHECKER_RANGE1 is to be used if the instance type is already
// loaded. INSTANCE_TYPE_CHECKER_RANGE2 is preferred since it can sometimes
// avoid loading the instance type from the map, if the checked instance type
// range corresponds to a known range of maps.

#define INSTANCE_TYPE_CHECKER_RANGE1(type, first_instance_type,            \
                                     last_instance_type)                   \
  V8_INLINE constexpr bool Is##type(InstanceType instance_type) {          \
    return InstanceRangeChecker<first_instance_type,                       \
                                last_instance_type>::Check(instance_type); \
  }

#if V8_STATIC_ROOTS_BOOL

#define INSTANCE_TYPE_CHECKER_RANGE2(type, first_instance_type, \
                                     last_instance_type)        \
  V8_INLINE bool Is##type(Map map_object) {                     \
    if (base::Optional<std::pair<RootIndex, RootIndex>> range = \
            StaticReadOnlyRootMapRange(first_instance_type,     \
                                       last_instance_type)) {   \
      return CheckInstanceMapRange(*range, map_object);         \
    }                                                           \
    return Is##type(map_object.instance_type());                \
  }

#else

#define INSTANCE_TYPE_CHECKER_RANGE2(type, first_instance_type, \
                                     last_instance_type)        \
  V8_INLINE bool Is##type(Map map_object) {                     \
    return Is##type(map_object.instance_type());                \
  }

#endif  // V8_STATIC_ROOTS_BOOL

INSTANCE_TYPE_CHECKERS_RANGE(INSTANCE_TYPE_CHECKER_RANGE1)
INSTANCE_TYPE_CHECKERS_RANGE(INSTANCE_TYPE_CHECKER_RANGE2)
#undef INSTANCE_TYPE_CHECKER_RANGE1
#undef INSTANCE_TYPE_CHECKER_RANGE2

V8_INLINE constexpr bool IsHeapObject(InstanceType instance_type) {
  return true;
}

V8_INLINE constexpr bool IsInternalizedString(InstanceType instance_type) {
  static_assert(kNotInternalizedTag != 0);
  return (instance_type & (kIsNotStringMask | kIsNotInternalizedMask)) ==
         (kStringTag | kInternalizedTag);
}

V8_INLINE bool IsInternalizedString(Map map_object) {
#if V8_STATIC_ROOTS_BOOL
  return CheckInstanceMapRange(
      *StaticReadOnlyRootMapRange(INTERNALIZED_STRING_TYPE), map_object);
#else
  return IsInternalizedString(map_object.instance_type());
#endif
}

V8_INLINE constexpr bool IsExternalString(InstanceType instance_type) {
  return (instance_type & (kIsNotStringMask | kStringRepresentationMask)) ==
         kExternalStringTag;
}

V8_INLINE bool IsExternalString(Map map_object) {
  return IsExternalString(map_object.instance_type());
}

V8_INLINE constexpr bool IsThinString(InstanceType instance_type) {
  return (instance_type & kStringRepresentationMask) == kThinStringTag;
}

V8_INLINE bool IsThinString(Map map_object) {
  return IsThinString(map_object.instance_type());
}

V8_INLINE constexpr bool IsGcSafeCode(InstanceType instance_type) {
  return IsCode(instance_type);
}

V8_INLINE bool IsGcSafeCode(Map map_object) { return IsCode(map_object); }

V8_INLINE constexpr bool IsAbstractCode(InstanceType instance_type) {
  return IsBytecodeArray(instance_type) || IsCode(instance_type);
}

V8_INLINE bool IsAbstractCode(Map map_object) {
  return IsAbstractCode(map_object.instance_type());
}

V8_INLINE constexpr bool IsFreeSpaceOrFiller(InstanceType instance_type) {
  return instance_type == FREE_SPACE_TYPE || instance_type == FILLER_TYPE;
}

V8_INLINE bool IsFreeSpaceOrFiller(Map map_object) {
  return IsFreeSpaceOrFiller(map_object.instance_type());
}

}  // namespace InstanceTypeChecker

#define TYPE_CHECKER(type, ...)                                                \
  bool HeapObject::Is##type() const {                                          \
    /* In general, parameterless IsBlah() must not be used for objects */      \
    /* that might be located in external code space. Note that this version */ \
    /* is still called from Blah::cast() methods but it's fine because in */   \
    /* production builds these checks are not enabled anyway and debug */      \
    /* builds are allowed to be a bit slower. */                               \
    PtrComprCageBase cage_base = GetPtrComprCageBaseSlow(*this);               \
    return HeapObject::Is##type(cage_base);                                    \
  }                                                                            \
  /* The cage_base passed here is must to be the base of the pointer */        \
  /* compression cage where the Map space is allocated. */                     \
  bool HeapObject::Is##type(PtrComprCageBase cage_base) const {                \
    Map map_object = map(cage_base);                                           \
    return InstanceTypeChecker::Is##type(map_object);                          \
  }

INSTANCE_TYPE_CHECKERS(TYPE_CHECKER)
#undef TYPE_CHECKER

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_INSTANCE_TYPE_INL_H_
