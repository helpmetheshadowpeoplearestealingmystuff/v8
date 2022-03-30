// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_CODE_KIND_H_
#define V8_OBJECTS_CODE_KIND_H_

#include "src/base/bounds.h"
#include "src/base/flags.h"
#include "src/flags/flags.h"

namespace v8 {
namespace internal {

// The order of INTERPRETED_FUNCTION to TURBOFAN is important. We use it to
// check the relative ordering of the tiers when fetching / installing optimized
// code.
#define CODE_KIND_LIST(V)  \
  V(BYTECODE_HANDLER)      \
  V(FOR_TESTING)           \
  V(BUILTIN)               \
  V(REGEXP)                \
  V(WASM_FUNCTION)         \
  V(WASM_TO_CAPI_FUNCTION) \
  V(WASM_TO_JS_FUNCTION)   \
  V(JS_TO_WASM_FUNCTION)   \
  V(JS_TO_JS_FUNCTION)     \
  V(C_WASM_ENTRY)          \
  V(INTERPRETED_FUNCTION)  \
  V(BASELINE)              \
  V(MAGLEV)                \
  V(TURBOFAN)

enum class CodeKind : uint8_t {
#define DEFINE_CODE_KIND_ENUM(name) name,
  CODE_KIND_LIST(DEFINE_CODE_KIND_ENUM)
#undef DEFINE_CODE_KIND_ENUM
};
STATIC_ASSERT(CodeKind::INTERPRETED_FUNCTION < CodeKind::BASELINE);
STATIC_ASSERT(CodeKind::BASELINE < CodeKind::TURBOFAN);

#define V(...) +1
static constexpr int kCodeKindCount = CODE_KIND_LIST(V);
#undef V
// Unlikely, but just to be safe:
STATIC_ASSERT(kCodeKindCount <= std::numeric_limits<uint8_t>::max());

const char* CodeKindToString(CodeKind kind);

const char* CodeKindToMarker(CodeKind kind);

inline constexpr bool CodeKindIsInterpretedJSFunction(CodeKind kind) {
  return kind == CodeKind::INTERPRETED_FUNCTION;
}

inline constexpr bool CodeKindIsBaselinedJSFunction(CodeKind kind) {
  return kind == CodeKind::BASELINE;
}

inline constexpr bool CodeKindIsStaticallyCompiled(CodeKind kind) {
  return kind == CodeKind::BYTECODE_HANDLER || kind == CodeKind::BUILTIN;
}

inline constexpr bool CodeKindIsUnoptimizedJSFunction(CodeKind kind) {
  STATIC_ASSERT(static_cast<int>(CodeKind::INTERPRETED_FUNCTION) + 1 ==
                static_cast<int>(CodeKind::BASELINE));
  return base::IsInRange(kind, CodeKind::INTERPRETED_FUNCTION,
                         CodeKind::BASELINE);
}

inline constexpr bool CodeKindIsOptimizedJSFunction(CodeKind kind) {
  STATIC_ASSERT(static_cast<int>(CodeKind::MAGLEV) + 1 ==
                static_cast<int>(CodeKind::TURBOFAN));
  return base::IsInRange(kind, CodeKind::MAGLEV, CodeKind::TURBOFAN);
}

inline constexpr bool CodeKindIsJSFunction(CodeKind kind) {
  STATIC_ASSERT(static_cast<int>(CodeKind::BASELINE) + 1 ==
                static_cast<int>(CodeKind::MAGLEV));
  return base::IsInRange(kind, CodeKind::INTERPRETED_FUNCTION,
                         CodeKind::TURBOFAN);
}

inline constexpr bool CodeKindIsBuiltinOrJSFunction(CodeKind kind) {
  return kind == CodeKind::BUILTIN || CodeKindIsJSFunction(kind);
}

inline constexpr bool CodeKindCanDeoptimize(CodeKind kind) {
  return CodeKindIsOptimizedJSFunction(kind);
}

inline constexpr bool CodeKindCanOSR(CodeKind kind) {
  return kind == CodeKind::TURBOFAN;
}

inline constexpr bool CodeKindCanTierUp(CodeKind kind) {
  return CodeKindIsUnoptimizedJSFunction(kind);
}

// TODO(jgruber): Rename or remove this predicate. Currently it means 'is this
// kind stored either in the FeedbackVector cache, or in the OSR cache?'.
inline constexpr bool CodeKindIsStoredInOptimizedCodeCache(CodeKind kind) {
  return kind == CodeKind::TURBOFAN;
}

inline CodeKind CodeKindForTopTier() { return CodeKind::TURBOFAN; }
inline CodeKind CodeKindForOSR() { return CodeKind::TURBOFAN; }

// The dedicated CodeKindFlag enum represents all code kinds in a format
// suitable for bit sets.
enum class CodeKindFlag {
#define V(name) name = 1 << static_cast<int>(CodeKind::name),
  CODE_KIND_LIST(V)
#undef V
};
STATIC_ASSERT(kCodeKindCount <= kInt32Size * kBitsPerByte);

inline constexpr CodeKindFlag CodeKindToCodeKindFlag(CodeKind kind) {
#define V(name) kind == CodeKind::name ? CodeKindFlag::name:
  return CODE_KIND_LIST(V) CodeKindFlag::INTERPRETED_FUNCTION;
#undef V
}

// CodeKinds represents a set of CodeKind.
using CodeKinds = base::Flags<CodeKindFlag>;
DEFINE_OPERATORS_FOR_FLAGS(CodeKinds)

static constexpr CodeKinds kJSFunctionCodeKindsMask{
    CodeKindFlag::INTERPRETED_FUNCTION | CodeKindFlag::BASELINE |
    CodeKindFlag::MAGLEV | CodeKindFlag::TURBOFAN};
static constexpr CodeKinds kOptimizedJSFunctionCodeKindsMask{
    CodeKindFlag::MAGLEV | CodeKindFlag::TURBOFAN};

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_CODE_KIND_H_
