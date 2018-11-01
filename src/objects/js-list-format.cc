// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include "src/objects/js-list-format.h"

#include <memory>
#include <vector>

#include "src/elements.h"
#include "src/heap/factory.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/objects/intl-objects.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/js-list-format-inl.h"
#include "src/objects/managed.h"
#include "unicode/fieldpos.h"
#include "unicode/fpositer.h"
#include "unicode/listformatter.h"
#include "unicode/ulistformatter.h"

namespace v8 {
namespace internal {

namespace {
const char* kStandard = "standard";
const char* kOr = "or";
const char* kUnit = "unit";
const char* kStandardShort = "standard-short";
const char* kUnitShort = "unit-short";
const char* kUnitNarrow = "unit-narrow";

const char* GetIcuStyleString(JSListFormat::Style style,
                              JSListFormat::Type type) {
  switch (type) {
    case JSListFormat::Type::CONJUNCTION:
      switch (style) {
        case JSListFormat::Style::LONG:
          return kStandard;
        case JSListFormat::Style::SHORT:
          return kStandardShort;
        // NARROW is now not allowed if type is not unit
        // It is impossible to reach because we've already thrown a RangeError
        // when style is "narrow" and type is not "unit".
        case JSListFormat::Style::NARROW:
        case JSListFormat::Style::COUNT:
          UNREACHABLE();
      }
    case JSListFormat::Type::DISJUNCTION:
      switch (style) {
        // Currently, ListFormat::createInstance on "or-short"
        // will fail so we use "or" here.
        // See https://unicode.org/cldr/trac/ticket/11254
        // TODO(ftang): change to return kOr or kOrShort depend on
        // style after the above issue fixed in CLDR/ICU.
        // CLDR bug: https://unicode.org/cldr/trac/ticket/11254
        // ICU bug: https://unicode-org.atlassian.net/browse/ICU-20014
        case JSListFormat::Style::LONG:
        case JSListFormat::Style::SHORT:
          return kOr;
        // NARROW is now not allowed if type is not unit
        // It is impossible to reach because we've already thrown a RangeError
        // when style is "narrow" and type is not "unit".
        case JSListFormat::Style::NARROW:
        case JSListFormat::Style::COUNT:
          UNREACHABLE();
      }
    case JSListFormat::Type::UNIT:
      switch (style) {
        case JSListFormat::Style::LONG:
          return kUnit;
        case JSListFormat::Style::SHORT:
          return kUnitShort;
        case JSListFormat::Style::NARROW:
          return kUnitNarrow;
        case JSListFormat::Style::COUNT:
          UNREACHABLE();
      }
    case JSListFormat::Type::COUNT:
      UNREACHABLE();
  }
}

}  // namespace

JSListFormat::Style get_style(const char* str) {
  switch (str[0]) {
    case 'n':
      if (strcmp(&str[1], "arrow") == 0) return JSListFormat::Style::NARROW;
      break;
    case 'l':
      if (strcmp(&str[1], "ong") == 0) return JSListFormat::Style::LONG;
      break;
    case 's':
      if (strcmp(&str[1], "hort") == 0) return JSListFormat::Style::SHORT;
      break;
  }
  UNREACHABLE();
}

JSListFormat::Type get_type(const char* str) {
  switch (str[0]) {
    case 'c':
      if (strcmp(&str[1], "onjunction") == 0)
        return JSListFormat::Type::CONJUNCTION;
      break;
    case 'd':
      if (strcmp(&str[1], "isjunction") == 0)
        return JSListFormat::Type::DISJUNCTION;
      break;
    case 'u':
      if (strcmp(&str[1], "nit") == 0) return JSListFormat::Type::UNIT;
      break;
  }
  UNREACHABLE();
}

MaybeHandle<JSListFormat> JSListFormat::Initialize(
    Isolate* isolate, Handle<JSListFormat> list_format, Handle<Object> locales,
    Handle<Object> input_options) {
  list_format->set_flags(0);

  Handle<JSReceiver> options;
  // 3. Let requestedLocales be ? CanonicalizeLocaleList(locales).
  Maybe<std::vector<std::string>> maybe_requested_locales =
      Intl::CanonicalizeLocaleList(isolate, locales);
  MAYBE_RETURN(maybe_requested_locales, Handle<JSListFormat>());
  std::vector<std::string> requested_locales =
      maybe_requested_locales.FromJust();

  // 4. If options is undefined, then
  if (input_options->IsUndefined(isolate)) {
    // 4. a. Let options be ObjectCreate(null).
    options = isolate->factory()->NewJSObjectWithNullProto();
    // 5. Else
  } else {
    // 5. a. Let options be ? ToObject(options).
    ASSIGN_RETURN_ON_EXCEPTION(isolate, options,
                               Object::ToObject(isolate, input_options),
                               JSListFormat);
  }

  // Note: No need to create a record. It's not observable.
  // 6. Let opt be a new Record.

  // 7. Let t be GetOption(options, "type", "string", «"conjunction",
  //    "disjunction", "unit"», "conjunction").
  std::unique_ptr<char[]> type_str = nullptr;
  std::vector<const char*> type_values = {"conjunction", "disjunction", "unit"};
  Maybe<bool> maybe_found_type = Intl::GetStringOption(
      isolate, options, "type", type_values, "Intl.ListFormat", &type_str);
  Type type_enum = Type::CONJUNCTION;
  MAYBE_RETURN(maybe_found_type, MaybeHandle<JSListFormat>());
  if (maybe_found_type.FromJust()) {
    DCHECK_NOT_NULL(type_str.get());
    type_enum = get_type(type_str.get());
  }

  // 8. Set listFormat.[[Type]] to t.
  list_format->set_type(type_enum);

  // 9. Let s be ? GetOption(options, "style", "string",
  //                          «"long", "short", "narrow"», "long").
  std::unique_ptr<char[]> style_str = nullptr;
  std::vector<const char*> style_values = {"long", "short", "narrow"};
  Maybe<bool> maybe_found_style = Intl::GetStringOption(
      isolate, options, "style", style_values, "Intl.ListFormat", &style_str);
  Style style_enum = Style::LONG;
  MAYBE_RETURN(maybe_found_style, MaybeHandle<JSListFormat>());
  if (maybe_found_style.FromJust()) {
    DCHECK_NOT_NULL(style_str.get());
    style_enum = get_style(style_str.get());
  }
  // 10. Set listFormat.[[Style]] to s.
  list_format->set_style(style_enum);

  // 12. Let matcher be ? GetOption(options, "localeMatcher", "string", «
  // "lookup", "best fit" », "best fit").
  Maybe<Intl::MatcherOption> maybe_locale_matcher =
      Intl::GetLocaleMatcher(isolate, options, "Intl.ListFormat");
  MAYBE_RETURN(maybe_locale_matcher, MaybeHandle<JSListFormat>());
  Intl::MatcherOption matcher = maybe_locale_matcher.FromJust();

  // 14. If style is "narrow" and type is not "unit", throw a RangeError
  // exception.
  if (style_enum == Style::NARROW && type_enum != Type::UNIT) {
    THROW_NEW_ERROR(
        isolate, NewRangeError(MessageTemplate::kIllegalTypeWhileStyleNarrow),
        JSListFormat);
  }

  // 15. Let r be ResolveLocale(%ListFormat%.[[AvailableLocales]],
  // requestedLocales, opt, undefined, localeData).
  Intl::ResolvedLocale r =
      Intl::ResolveLocale(isolate, JSListFormat::GetAvailableLocales(),
                          requested_locales, matcher, {});

  // 24. Set listFormat.[[Locale]] to r.[[Locale]].
  Handle<String> locale_str =
      isolate->factory()->NewStringFromAsciiChecked(r.locale.c_str());
  list_format->set_locale(*locale_str);

  icu::Locale icu_locale = r.icu_locale;
  UErrorCode status = U_ZERO_ERROR;
  icu::ListFormatter* formatter = icu::ListFormatter::createInstance(
      icu_locale, GetIcuStyleString(style_enum, type_enum), status);
  if (U_FAILURE(status)) {
    delete formatter;
    FATAL("Failed to create ICU list formatter, are ICU data files missing?");
  }
  CHECK_NOT_NULL(formatter);

  Handle<Managed<icu::ListFormatter>> managed_formatter =
      Managed<icu::ListFormatter>::FromRawPtr(isolate, 0, formatter);

  list_format->set_icu_formatter(*managed_formatter);
  return list_format;
}

// ecma402 #sec-intl.pluralrules.prototype.resolvedoptions
Handle<JSObject> JSListFormat::ResolvedOptions(Isolate* isolate,
                                               Handle<JSListFormat> format) {
  Factory* factory = isolate->factory();
  // 4. Let options be ! ObjectCreate(%ObjectPrototype%).
  Handle<JSObject> result = factory->NewJSObject(isolate->object_function());

  // 5.  For each row of Table 1, except the header row, do
  //  Table 1: Resolved Options of ListFormat Instances
  //  Internal Slot    Property
  //  [[Locale]]       "locale"
  //  [[Type]]         "type"
  //  [[Style]]        "style"
  Handle<String> locale(format->locale(), isolate);
  JSObject::AddProperty(isolate, result, factory->locale_string(), locale,
                        NONE);
  JSObject::AddProperty(isolate, result, factory->type_string(),
                        format->TypeAsString(), NONE);
  JSObject::AddProperty(isolate, result, factory->style_string(),
                        format->StyleAsString(), NONE);
  // 6. Return options.
  return result;
}

Handle<String> JSListFormat::StyleAsString() const {
  switch (style()) {
    case Style::LONG:
      return GetReadOnlyRoots().long_string_handle();
    case Style::SHORT:
      return GetReadOnlyRoots().short_string_handle();
    case Style::NARROW:
      return GetReadOnlyRoots().narrow_string_handle();
    case Style::COUNT:
      UNREACHABLE();
  }
}

Handle<String> JSListFormat::TypeAsString() const {
  switch (type()) {
    case Type::CONJUNCTION:
      return GetReadOnlyRoots().conjunction_string_handle();
    case Type::DISJUNCTION:
      return GetReadOnlyRoots().disjunction_string_handle();
    case Type::UNIT:
      return GetReadOnlyRoots().unit_string_handle();
    case Type::COUNT:
      UNREACHABLE();
  }
}

namespace {

MaybeHandle<JSArray> GenerateListFormatParts(
    Isolate* isolate, const icu::UnicodeString& formatted,
    const std::vector<icu::FieldPosition>& positions) {
  Factory* factory = isolate->factory();
  Handle<JSArray> array =
      factory->NewJSArray(static_cast<int>(positions.size()));
  int index = 0;
  int prev_item_end_index = 0;
  Handle<String> substring;
  for (const icu::FieldPosition pos : positions) {
    CHECK(pos.getBeginIndex() >= prev_item_end_index);
    CHECK(pos.getField() == ULISTFMT_ELEMENT_FIELD);
    if (pos.getBeginIndex() != prev_item_end_index) {
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, substring,
          Intl::ToString(isolate, formatted, prev_item_end_index,
                         pos.getBeginIndex()),
          JSArray);
      Intl::AddElement(isolate, array, index++, factory->literal_string(),
                       substring);
    }
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, substring,
        Intl::ToString(isolate, formatted, pos.getBeginIndex(),
                       pos.getEndIndex()),
        JSArray);
    Intl::AddElement(isolate, array, index++, factory->element_string(),
                     substring);
    prev_item_end_index = pos.getEndIndex();
  }
  if (prev_item_end_index != formatted.length()) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, substring,
        Intl::ToString(isolate, formatted, prev_item_end_index,
                       formatted.length()),
        JSArray);
    Intl::AddElement(isolate, array, index++, factory->literal_string(),
                     substring);
  }
  return array;
}

// Get all the FieldPosition into a vector from FieldPositionIterator and return
// them in output order.
std::vector<icu::FieldPosition> GenerateFieldPosition(
    icu::FieldPositionIterator iter) {
  std::vector<icu::FieldPosition> positions;
  icu::FieldPosition pos;
  while (iter.next(pos)) {
    // Only take the information of the ULISTFMT_ELEMENT_FIELD field.
    if (pos.getField() == ULISTFMT_ELEMENT_FIELD) {
      positions.push_back(pos);
    }
  }
  // Because the format may reoder the items, ICU FieldPositionIterator
  // keep the order for FieldPosition based on the order of the input items.
  // But the formatToParts API in ECMA402 expects in formatted output order.
  // Therefore we have to sort based on beginIndex of the FieldPosition.
  // Example of such is in the "ur" (Urdu) locale with type: "unit", where the
  // main text flows from right to left, the formatted list of unit should flow
  // from left to right and therefore in the memory the formatted result will
  // put the first item on the last in the result string according the current
  // CLDR patterns.
  // See 'listPattern' pattern in
  // third_party/icu/source/data/locales/ur_IN.txt
  std::sort(positions.begin(), positions.end(),
            [](icu::FieldPosition a, icu::FieldPosition b) {
              return a.getBeginIndex() < b.getBeginIndex();
            });
  return positions;
}

// Extract String from JSArray into array of UnicodeString
Maybe<bool> ToUnicodeStringArray(Isolate* isolate, Handle<JSArray> array,
                                 icu::UnicodeString items[], uint32_t length) {
  Factory* factory = isolate->factory();
  // In general, ElementsAccessor::Get actually isn't guaranteed to give us the
  // elements in order. But given that it was created by a builtin we control,
  // it shouldn't be possible for it to be problematic. Add DCHECK to ensure
  // that.
  DCHECK(array->HasFastPackedElements());
  auto* accessor = array->GetElementsAccessor();
  DCHECK(length == accessor->NumberOfElements(*array));
  // ecma402 #sec-createpartsfromlist
  // 2. If list contains any element value such that Type(value) is not String,
  // throw a TypeError exception.
  //
  // Per spec it looks like we're supposed to throw a TypeError exception if the
  // item isn't already a string, rather than coercing to a string. Moreover,
  // the way the spec's written it looks like we're supposed to run through the
  // whole list to check that they're all strings before going further.
  for (uint32_t i = 0; i < length; i++) {
    Handle<Object> item = accessor->Get(array, i);
    DCHECK(!item.is_null());
    if (!item->IsString()) {
      THROW_NEW_ERROR_RETURN_VALUE(
          isolate,
          NewTypeError(MessageTemplate::kArrayItemNotType,
                       factory->list_string(), factory->NewNumber(i),
                       factory->String_string()),
          Nothing<bool>());
    }
  }
  for (uint32_t i = 0; i < length; i++) {
    Handle<String> string = Handle<String>::cast(accessor->Get(array, i));
    items[i] = Intl::ToICUUnicodeString(isolate, string);
  }
  return Just(true);
}

}  // namespace

Maybe<bool> FormatListCommon(Isolate* isolate, Handle<JSArray> list,
                             uint32_t* length,
                             std::unique_ptr<icu::UnicodeString[]>& array) {
  DCHECK(!list->IsUndefined());
  *length = list->GetElementsAccessor()->NumberOfElements(*list);
  array.reset(new icu::UnicodeString[*length]);

  // ecma402 #sec-createpartsfromlist
  // 2. If list contains any element value such that Type(value) is not String,
  // throw a TypeError exception.
  MAYBE_RETURN(ToUnicodeStringArray(isolate, list, array.get(), *length),
               Nothing<bool>());

  return Just(true);
}

// ecma402 #sec-formatlist
MaybeHandle<String> JSListFormat::FormatList(Isolate* isolate,
                                             Handle<JSListFormat> format,
                                             Handle<JSArray> list) {
  DCHECK(!list->IsUndefined());
  uint32_t length = list->GetElementsAccessor()->NumberOfElements(*list);
  std::unique_ptr<icu::UnicodeString[]> array(new icu::UnicodeString[length]);

  // ecma402 #sec-createpartsfromlist
  // 2. If list contains any element value such that Type(value) is not String,
  // throw a TypeError exception.
  MAYBE_RETURN(ToUnicodeStringArray(isolate, list, array.get(), length),
               Handle<String>());

  icu::ListFormatter* formatter = format->icu_formatter()->raw();
  CHECK_NOT_NULL(formatter);

  UErrorCode status = U_ZERO_ERROR;
  icu::UnicodeString formatted;
  formatter->format(array.get(), length, formatted, status);
  DCHECK(U_SUCCESS(status));

  return Intl::ToString(isolate, formatted);
}

std::set<std::string> JSListFormat::GetAvailableLocales() {
  int32_t num_locales = 0;
  // TODO(ftang): for now just use
  // icu::Locale::getAvailableLocales(count) until we migrate to
  // Intl::GetAvailableLocales().
  // ICU FR at https://unicode-org.atlassian.net/browse/ICU-20015
  const icu::Locale* icu_available_locales =
      icu::Locale::getAvailableLocales(num_locales);
  return Intl::BuildLocaleSet(icu_available_locales, num_locales);
}

// ecma42 #sec-formatlisttoparts
MaybeHandle<JSArray> JSListFormat::FormatListToParts(
    Isolate* isolate, Handle<JSListFormat> format, Handle<JSArray> list) {
  DCHECK(!list->IsUndefined());
  uint32_t length = list->GetElementsAccessor()->NumberOfElements(*list);
  std::unique_ptr<icu::UnicodeString[]> array(new icu::UnicodeString[length]);

  // ecma402 #sec-createpartsfromlist
  // 2. If list contains any element value such that Type(value) is not String,
  // throw a TypeError exception.
  MAYBE_RETURN(ToUnicodeStringArray(isolate, list, array.get(), length),
               Handle<JSArray>());

  icu::ListFormatter* formatter = format->icu_formatter()->raw();
  CHECK_NOT_NULL(formatter);

  UErrorCode status = U_ZERO_ERROR;
  icu::UnicodeString formatted;
  icu::FieldPositionIterator iter;
  formatter->format(array.get(), length, formatted, &iter, status);
  DCHECK(U_SUCCESS(status));

  std::vector<icu::FieldPosition> field_positions = GenerateFieldPosition(iter);
  return GenerateListFormatParts(isolate, formatted, field_positions);
}
}  // namespace internal
}  // namespace v8
