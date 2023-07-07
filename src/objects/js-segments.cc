// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include "src/objects/js-segments.h"

#include <map>
#include <memory>
#include <string>

#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/objects/intl-objects.h"
#include "src/objects/js-segment-iterator.h"
#include "src/objects/js-segmenter-inl.h"
#include "src/objects/js-segments-inl.h"
#include "src/objects/managed-inl.h"
#include "src/objects/objects-inl.h"
#include "unicode/brkiter.h"

namespace v8 {
namespace internal {

// ecma402 #sec-createsegmentsobject
MaybeHandle<JSSegments> JSSegments::Create(Isolate* isolate,
                                           Handle<JSSegmenter> segmenter,
                                           Handle<String> string) {
  icu::BreakIterator* break_iterator =
      segmenter->icu_break_iterator().raw()->clone();
  DCHECK_NOT_NULL(break_iterator);

  Handle<Managed<icu::UnicodeString>> unicode_string =
      Intl::SetTextToBreakIterator(isolate, string, break_iterator);
  Handle<Managed<icu::BreakIterator>> managed_break_iterator =
      Managed<icu::BreakIterator>::FromRawPtr(isolate, 0, break_iterator);

  // 1. Let internalSlotsList be « [[SegmentsSegmenter]], [[SegmentsString]] ».
  // 2. Let segments be ! ObjectCreate(%Segments.prototype%, internalSlotsList).
  Handle<Map> map(isolate->native_context()->intl_segments_map(), isolate);
  Handle<JSObject> result = isolate->factory()->NewJSObjectFromMap(map);

  Handle<JSSegments> segments = Handle<JSSegments>::cast(result);
  segments->set_flags(0);

  // 3. Set segments.[[SegmentsSegmenter]] to segmenter.
  segments->set_icu_break_iterator(*managed_break_iterator);
  segments->set_granularity(segmenter->granularity());

  // 4. Set segments.[[SegmentsString]] to string.
  segments->set_unicode_string(*unicode_string);

  // 5. Return segments.
  return segments;
}

// ecma402 #sec-%segmentsprototype%.containing
MaybeHandle<Object> JSSegments::Containing(Isolate* isolate,
                                           Handle<JSSegments> segments,
                                           double n_double) {
  // 5. Let len be the length of string.
  int32_t len = segments->unicode_string().raw()->length();

  // 7. If n < 0 or n ≥ len, return undefined.
  if (n_double < 0 || n_double >= len) {
    return isolate->factory()->undefined_value();
  }

  int32_t n = static_cast<int32_t>(n_double);
  // n may point to the surrogate tail- adjust it back to the lead.
  n = segments->unicode_string().raw()->getChar32Start(n);

  icu::BreakIterator* break_iterator = segments->icu_break_iterator().raw();
  // 8. Let startIndex be ! FindBoundary(segmenter, string, n, before).
  int32_t start_index =
      break_iterator->isBoundary(n) ? n : break_iterator->preceding(n);

  // 9. Let endIndex be ! FindBoundary(segmenter, string, n, after).
  int32_t end_index = break_iterator->following(n);

  // 10. Return ! CreateSegmentDataObject(segmenter, string, startIndex,
  // endIndex).
  return CreateSegmentDataObject(
      isolate, segments->granularity(), break_iterator,
      *(segments->unicode_string().raw()), start_index, end_index);
}

namespace {

bool CurrentSegmentIsWordLike(icu::BreakIterator* break_iterator) {
  int32_t rule_status = break_iterator->getRuleStatus();
  return (rule_status >= UBRK_WORD_NUMBER &&
          rule_status < UBRK_WORD_NUMBER_LIMIT) ||
         (rule_status >= UBRK_WORD_LETTER &&
          rule_status < UBRK_WORD_LETTER_LIMIT) ||
         (rule_status >= UBRK_WORD_KANA &&
          rule_status < UBRK_WORD_KANA_LIMIT) ||
         (rule_status >= UBRK_WORD_IDEO && rule_status < UBRK_WORD_IDEO_LIMIT);
}

}  // namespace

// ecma402 #sec-createsegmentdataobject
MaybeHandle<Object> JSSegments::CreateSegmentDataObject(
    Isolate* isolate, JSSegmenter::Granularity granularity,
    icu::BreakIterator* break_iterator, const icu::UnicodeString& string,
    int32_t start_index, int32_t end_index) {
  Factory* factory = isolate->factory();

  // 1. Let len be the length of string.
  // 2. Assert: startIndex ≥ 0.
  DCHECK_GE(start_index, 0);
  // 3. Assert: endIndex ≤ len.
  DCHECK_LE(end_index, string.length());
  // 4. Assert: startIndex < endIndex.
  DCHECK_LT(start_index, end_index);

  // 5. Let result be ! ObjectCreate(%ObjectPrototype%).
  Handle<JSObject> result = factory->NewJSObject(isolate->object_function());

  // 6. Let segment be the String value equal to the substring of string
  // consisting of the code units at indices startIndex (inclusive) through
  // endIndex (exclusive).
  Handle<String> segment;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, segment, Intl::ToString(isolate, string, start_index, end_index),
      JSObject);

  // 7. Perform ! CreateDataPropertyOrThrow(result, "segment", segment).
  Maybe<bool> maybe_create_segment = JSReceiver::CreateDataProperty(
      isolate, result, factory->segment_string(), segment, Just(kDontThrow));
  DCHECK(maybe_create_segment.FromJust());
  USE(maybe_create_segment);

  // 8. Perform ! CreateDataPropertyOrThrow(result, "index", startIndex).
  Maybe<bool> maybe_create_index = JSReceiver::CreateDataProperty(
      isolate, result, factory->index_string(),
      factory->NewNumberFromInt(start_index), Just(kDontThrow));
  DCHECK(maybe_create_index.FromJust());
  USE(maybe_create_index);

  // 9. Perform ! CreateDataPropertyOrThrow(result, "input", string).
  Handle<String> input_string;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, input_string,
                             Intl::ToString(isolate, string), JSObject);
  Maybe<bool> maybe_create_input = JSReceiver::CreateDataProperty(
      isolate, result, factory->input_string(), input_string, Just(kDontThrow));
  DCHECK(maybe_create_input.FromJust());
  USE(maybe_create_input);

  Handle<Object> is_word_like;
  // 10. Let granularity be segmenter.[[SegmenterGranularity]].
  // 11. If granularity is "word", then
  if (granularity == JSSegmenter::Granularity::WORD) {
    // a. Let isWordLike be a Boolean value indicating whether the word segment
    //    segment in string is "word-like" according to locale
    //    segmenter.[[Locale]].
    is_word_like = factory->ToBoolean(CurrentSegmentIsWordLike(break_iterator));
    // b. Perform ! CreateDataPropertyOrThrow(result, "isWordLike", isWordLike).
    Maybe<bool> maybe_create_is_word_like = JSReceiver::CreateDataProperty(
        isolate, result, factory->isWordLike_string(), is_word_like,
        Just(kDontThrow));
    DCHECK(maybe_create_is_word_like.FromJust());
    USE(maybe_create_is_word_like);
  }
  return result;
}

Handle<String> JSSegments::GranularityAsString(Isolate* isolate) const {
  return JSSegmenter::GetGranularityString(isolate, granularity());
}

}  // namespace internal
}  // namespace v8
