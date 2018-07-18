// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include <cmath>

#include "src/builtins/builtins-intl.h"
#include "src/builtins/builtins-utils.h"
#include "src/builtins/builtins.h"
#include "src/date.h"
#include "src/intl.h"
#include "src/objects-inl.h"
#include "src/objects/intl-objects.h"
#include "src/objects/js-list-format-inl.h"
#include "src/objects/js-locale-inl.h"
#include "src/objects/js-relative-time-format-inl.h"

#include "unicode/datefmt.h"
#include "unicode/decimfmt.h"
#include "unicode/fieldpos.h"
#include "unicode/fpositer.h"
#include "unicode/normalizer2.h"
#include "unicode/numfmt.h"
#include "unicode/reldatefmt.h"
#include "unicode/smpdtfmt.h"
#include "unicode/udat.h"
#include "unicode/ufieldpositer.h"
#include "unicode/unistr.h"
#include "unicode/ureldatefmt.h"
#include "unicode/ustring.h"

namespace v8 {
namespace internal {

BUILTIN(StringPrototypeToUpperCaseIntl) {
  HandleScope scope(isolate);
  TO_THIS_STRING(string, "String.prototype.toUpperCase");
  string = String::Flatten(isolate, string);
  return ConvertCase(string, true, isolate);
}

BUILTIN(StringPrototypeNormalizeIntl) {
  HandleScope handle_scope(isolate);
  TO_THIS_STRING(string, "String.prototype.normalize");

  Handle<Object> form_input = args.atOrUndefined(isolate, 1);
  const char* form_name;
  UNormalization2Mode form_mode;
  if (form_input->IsUndefined(isolate)) {
    // default is FNC
    form_name = "nfc";
    form_mode = UNORM2_COMPOSE;
  } else {
    Handle<String> form;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, form,
                                       Object::ToString(isolate, form_input));

    if (String::Equals(isolate, form, isolate->factory()->NFC_string())) {
      form_name = "nfc";
      form_mode = UNORM2_COMPOSE;
    } else if (String::Equals(isolate, form,
                              isolate->factory()->NFD_string())) {
      form_name = "nfc";
      form_mode = UNORM2_DECOMPOSE;
    } else if (String::Equals(isolate, form,
                              isolate->factory()->NFKC_string())) {
      form_name = "nfkc";
      form_mode = UNORM2_COMPOSE;
    } else if (String::Equals(isolate, form,
                              isolate->factory()->NFKD_string())) {
      form_name = "nfkc";
      form_mode = UNORM2_DECOMPOSE;
    } else {
      Handle<String> valid_forms =
          isolate->factory()->NewStringFromStaticChars("NFC, NFD, NFKC, NFKD");
      THROW_NEW_ERROR_RETURN_FAILURE(
          isolate,
          NewRangeError(MessageTemplate::kNormalizationForm, valid_forms));
    }
  }

  int length = string->length();
  string = String::Flatten(isolate, string);
  icu::UnicodeString result;
  std::unique_ptr<uc16[]> sap;
  UErrorCode status = U_ZERO_ERROR;
  {
    DisallowHeapAllocation no_gc;
    String::FlatContent flat = string->GetFlatContent();
    const UChar* src = GetUCharBufferFromFlat(flat, &sap, length);
    icu::UnicodeString input(false, src, length);
    // Getting a singleton. Should not free it.
    const icu::Normalizer2* normalizer =
        icu::Normalizer2::getInstance(nullptr, form_name, form_mode, status);
    DCHECK(U_SUCCESS(status));
    CHECK_NOT_NULL(normalizer);
    int32_t normalized_prefix_length =
        normalizer->spanQuickCheckYes(input, status);
    // Quick return if the input is already normalized.
    if (length == normalized_prefix_length) return *string;
    icu::UnicodeString unnormalized =
        input.tempSubString(normalized_prefix_length);
    // Read-only alias of the normalized prefix.
    result.setTo(false, input.getBuffer(), normalized_prefix_length);
    // copy-on-write; normalize the suffix and append to |result|.
    normalizer->normalizeSecondAndAppend(result, unnormalized, status);
  }

  if (U_FAILURE(status)) {
    return ReadOnlyRoots(isolate).undefined_value();
  }

  RETURN_RESULT_OR_FAILURE(
      isolate, isolate->factory()->NewStringFromTwoByte(Vector<const uint16_t>(
                   reinterpret_cast<const uint16_t*>(result.getBuffer()),
                   result.length())));
}

namespace {

// The list comes from third_party/icu/source/i18n/unicode/unum.h.
// They're mapped to NumberFormat part types mentioned throughout
// https://tc39.github.io/ecma402/#sec-partitionnumberpattern .
Handle<String> IcuNumberFieldIdToNumberType(int32_t field_id, double number,
                                            Isolate* isolate) {
  switch (static_cast<UNumberFormatFields>(field_id)) {
    case UNUM_INTEGER_FIELD:
      if (std::isfinite(number)) return isolate->factory()->integer_string();
      if (std::isnan(number)) return isolate->factory()->nan_string();
      return isolate->factory()->infinity_string();
    case UNUM_FRACTION_FIELD:
      return isolate->factory()->fraction_string();
    case UNUM_DECIMAL_SEPARATOR_FIELD:
      return isolate->factory()->decimal_string();
    case UNUM_GROUPING_SEPARATOR_FIELD:
      return isolate->factory()->group_string();
    case UNUM_CURRENCY_FIELD:
      return isolate->factory()->currency_string();
    case UNUM_PERCENT_FIELD:
      return isolate->factory()->percentSign_string();
    case UNUM_SIGN_FIELD:
      return number < 0 ? isolate->factory()->minusSign_string()
                        : isolate->factory()->plusSign_string();

    case UNUM_EXPONENT_SYMBOL_FIELD:
    case UNUM_EXPONENT_SIGN_FIELD:
    case UNUM_EXPONENT_FIELD:
      // We should never get these because we're not using any scientific
      // formatter.
      UNREACHABLE();
      return Handle<String>();

    case UNUM_PERMILL_FIELD:
      // We're not creating any permill formatter, and it's not even clear how
      // that would be possible with the ICU API.
      UNREACHABLE();
      return Handle<String>();

    default:
      UNREACHABLE();
      return Handle<String>();
  }
}

// The list comes from third_party/icu/source/i18n/unicode/udat.h.
// They're mapped to DateTimeFormat components listed at
// https://tc39.github.io/ecma402/#sec-datetimeformat-abstracts .

Handle<String> IcuDateFieldIdToDateType(int32_t field_id, Isolate* isolate) {
  switch (field_id) {
    case -1:
      return isolate->factory()->literal_string();
    case UDAT_YEAR_FIELD:
    case UDAT_EXTENDED_YEAR_FIELD:
    case UDAT_YEAR_NAME_FIELD:
      return isolate->factory()->year_string();
    case UDAT_MONTH_FIELD:
    case UDAT_STANDALONE_MONTH_FIELD:
      return isolate->factory()->month_string();
    case UDAT_DATE_FIELD:
      return isolate->factory()->day_string();
    case UDAT_HOUR_OF_DAY1_FIELD:
    case UDAT_HOUR_OF_DAY0_FIELD:
    case UDAT_HOUR1_FIELD:
    case UDAT_HOUR0_FIELD:
      return isolate->factory()->hour_string();
    case UDAT_MINUTE_FIELD:
      return isolate->factory()->minute_string();
    case UDAT_SECOND_FIELD:
      return isolate->factory()->second_string();
    case UDAT_DAY_OF_WEEK_FIELD:
    case UDAT_DOW_LOCAL_FIELD:
    case UDAT_STANDALONE_DAY_FIELD:
      return isolate->factory()->weekday_string();
    case UDAT_AM_PM_FIELD:
      return isolate->factory()->dayperiod_string();
    case UDAT_TIMEZONE_FIELD:
    case UDAT_TIMEZONE_RFC_FIELD:
    case UDAT_TIMEZONE_GENERIC_FIELD:
    case UDAT_TIMEZONE_SPECIAL_FIELD:
    case UDAT_TIMEZONE_LOCALIZED_GMT_OFFSET_FIELD:
    case UDAT_TIMEZONE_ISO_FIELD:
    case UDAT_TIMEZONE_ISO_LOCAL_FIELD:
      return isolate->factory()->timeZoneName_string();
    case UDAT_ERA_FIELD:
      return isolate->factory()->era_string();
    default:
      // Other UDAT_*_FIELD's cannot show up because there is no way to specify
      // them via options of Intl.DateTimeFormat.
      UNREACHABLE();
      // To prevent MSVC from issuing C4715 warning.
      return Handle<String>();
  }
}

MaybeHandle<JSObject> InnerAddElement(Isolate* isolate, Handle<JSArray> array,
                                      int index,
                                      Handle<String> field_type_string,
                                      const icu::UnicodeString& formatted,
                                      int32_t begin, int32_t end) {
  Factory* factory = isolate->factory();
  Handle<JSObject> element = factory->NewJSObject(isolate->object_function());
  Handle<String> value;
  JSObject::AddProperty(isolate, element, factory->type_string(),
                        field_type_string, NONE);

  icu::UnicodeString field(formatted.tempSubStringBetween(begin, end));
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, value,
      factory->NewStringFromTwoByte(Vector<const uint16_t>(
          reinterpret_cast<const uint16_t*>(field.getBuffer()),
          field.length())),
      JSObject);

  JSObject::AddProperty(isolate, element, factory->value_string(), value, NONE);
  JSObject::AddDataElement(array, index, element, NONE);
  return element;
}

Maybe<bool> AddElement(Isolate* isolate, Handle<JSArray> array, int index,
                       Handle<String> field_type_string,
                       const icu::UnicodeString& formatted, int32_t begin,
                       int32_t end) {
  RETURN_ON_EXCEPTION_VALUE(
      isolate,
      InnerAddElement(isolate, array, index, field_type_string, formatted,
                      begin, end),
      Nothing<bool>());
  return Just(true);
}

Maybe<bool> AddElement(Isolate* isolate, Handle<JSArray> array, int index,
                       Handle<String> field_type_string,
                       const icu::UnicodeString& formatted, int32_t begin,
                       int32_t end, Handle<String> name, Handle<String> value) {
  Handle<JSObject> element;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, element,
      InnerAddElement(isolate, array, index, field_type_string, formatted,
                      begin, end),
      Nothing<bool>());
  JSObject::AddProperty(isolate, element, name, value, NONE);
  return Just(true);
}

bool cmp_NumberFormatSpan(const NumberFormatSpan& a,
                          const NumberFormatSpan& b) {
  // Regions that start earlier should be encountered earlier.
  if (a.begin_pos < b.begin_pos) return true;
  if (a.begin_pos > b.begin_pos) return false;
  // For regions that start in the same place, regions that last longer should
  // be encountered earlier.
  if (a.end_pos < b.end_pos) return false;
  if (a.end_pos > b.end_pos) return true;
  // For regions that are exactly the same, one of them must be the "literal"
  // backdrop we added, which has a field_id of -1, so consider higher field_ids
  // to be later.
  return a.field_id < b.field_id;
}

Object* FormatNumberToParts(Isolate* isolate, icu::NumberFormat* fmt,
                            double number) {
  Factory* factory = isolate->factory();

  icu::UnicodeString formatted;
  icu::FieldPositionIterator fp_iter;
  UErrorCode status = U_ZERO_ERROR;
  fmt->format(number, formatted, &fp_iter, status);
  if (U_FAILURE(status)) return ReadOnlyRoots(isolate).undefined_value();

  Handle<JSArray> result = factory->NewJSArray(0);
  int32_t length = formatted.length();
  if (length == 0) return *result;

  std::vector<NumberFormatSpan> regions;
  // Add a "literal" backdrop for the entire string. This will be used if no
  // other region covers some part of the formatted string. It's possible
  // there's another field with exactly the same begin and end as this backdrop,
  // in which case the backdrop's field_id of -1 will give it lower priority.
  regions.push_back(NumberFormatSpan(-1, 0, formatted.length()));

  {
    icu::FieldPosition fp;
    while (fp_iter.next(fp)) {
      regions.push_back(NumberFormatSpan(fp.getField(), fp.getBeginIndex(),
                                         fp.getEndIndex()));
    }
  }

  std::vector<NumberFormatSpan> parts = FlattenRegionsToParts(&regions);

  int index = 0;
  for (auto it = parts.begin(); it < parts.end(); it++) {
    NumberFormatSpan part = *it;
    Handle<String> field_type_string =
        part.field_id == -1
            ? isolate->factory()->literal_string()
            : IcuNumberFieldIdToNumberType(part.field_id, number, isolate);
    Maybe<bool> maybe_added_element =
        AddElement(isolate, result, index, field_type_string, formatted,
                   part.begin_pos, part.end_pos);
    MAYBE_RETURN(maybe_added_element, ReadOnlyRoots(isolate).undefined_value());
    ++index;
  }
  JSObject::ValidateElements(*result);

  return *result;
}

Object* FormatDateToParts(Isolate* isolate, icu::DateFormat* format,
                          double date_value) {
  Factory* factory = isolate->factory();

  icu::UnicodeString formatted;
  icu::FieldPositionIterator fp_iter;
  icu::FieldPosition fp;
  UErrorCode status = U_ZERO_ERROR;
  format->format(date_value, formatted, &fp_iter, status);
  if (U_FAILURE(status)) return ReadOnlyRoots(isolate).undefined_value();

  Handle<JSArray> result = factory->NewJSArray(0);
  int32_t length = formatted.length();
  if (length == 0) return *result;

  int index = 0;
  int32_t previous_end_pos = 0;
  while (fp_iter.next(fp)) {
    int32_t begin_pos = fp.getBeginIndex();
    int32_t end_pos = fp.getEndIndex();

    if (previous_end_pos < begin_pos) {
      Maybe<bool> maybe_added_element = AddElement(
          isolate, result, index, IcuDateFieldIdToDateType(-1, isolate),
          formatted, previous_end_pos, begin_pos);
      MAYBE_RETURN(maybe_added_element,
                   ReadOnlyRoots(isolate).undefined_value());
      ++index;
    }
    Maybe<bool> maybe_added_element =
        AddElement(isolate, result, index,
                   IcuDateFieldIdToDateType(fp.getField(), isolate), formatted,
                   begin_pos, end_pos);
    MAYBE_RETURN(maybe_added_element, ReadOnlyRoots(isolate).undefined_value());
    previous_end_pos = end_pos;
    ++index;
  }
  if (previous_end_pos < length) {
    Maybe<bool> maybe_added_element = AddElement(
        isolate, result, index, IcuDateFieldIdToDateType(-1, isolate),
        formatted, previous_end_pos, length);
    MAYBE_RETURN(maybe_added_element, ReadOnlyRoots(isolate).undefined_value());
  }
  JSObject::ValidateElements(*result);
  return *result;
}

}  // namespace

// Flattens a list of possibly-overlapping "regions" to a list of
// non-overlapping "parts". At least one of the input regions must span the
// entire space of possible indexes. The regions parameter will sorted in-place
// according to some criteria; this is done for performance to avoid copying the
// input.
std::vector<NumberFormatSpan> FlattenRegionsToParts(
    std::vector<NumberFormatSpan>* regions) {
  // The intention of this algorithm is that it's used to translate ICU "fields"
  // to JavaScript "parts" of a formatted string. Each ICU field and JavaScript
  // part has an integer field_id, which corresponds to something like "grouping
  // separator", "fraction", or "percent sign", and has a begin and end
  // position. Here's a diagram of:

  // var nf = new Intl.NumberFormat(['de'], {style:'currency',currency:'EUR'});
  // nf.formatToParts(123456.78);

  //               :       6
  //  input regions:    0000000211 7
  // ('-' means -1):    ------------
  // formatted string: "123.456,78 €"
  // output parts:      0006000211-7

  // To illustrate the requirements of this algorithm, here's a contrived and
  // convoluted example of inputs and expected outputs:

  //              :          4
  //              :      22 33    3
  //              :      11111   22
  // input regions:     0000000  111
  //              :     ------------
  // formatted string: "abcdefghijkl"
  // output parts:      0221340--231
  // (The characters in the formatted string are irrelevant to this function.)

  // We arrange the overlapping input regions like a mountain range where
  // smaller regions are "on top" of larger regions, and we output a birds-eye
  // view of the mountains, so that smaller regions take priority over larger
  // regions.
  std::sort(regions->begin(), regions->end(), cmp_NumberFormatSpan);
  std::vector<size_t> overlapping_region_index_stack;
  // At least one item in regions must be a region spanning the entire string.
  // Due to the sorting above, the first item in the vector will be one of them.
  overlapping_region_index_stack.push_back(0);
  NumberFormatSpan top_region = regions->at(0);
  size_t region_iterator = 1;
  int32_t entire_size = top_region.end_pos;

  std::vector<NumberFormatSpan> out_parts;

  // The "climber" is a cursor that advances from left to right climbing "up"
  // and "down" the mountains. Whenever the climber moves to the right, that
  // represents an item of output.
  int32_t climber = 0;
  while (climber < entire_size) {
    int32_t next_region_begin_pos;
    if (region_iterator < regions->size()) {
      next_region_begin_pos = regions->at(region_iterator).begin_pos;
    } else {
      // finish off the rest of the input by proceeding to the end.
      next_region_begin_pos = entire_size;
    }

    if (climber < next_region_begin_pos) {
      while (top_region.end_pos < next_region_begin_pos) {
        if (climber < top_region.end_pos) {
          // step down
          out_parts.push_back(NumberFormatSpan(top_region.field_id, climber,
                                               top_region.end_pos));
          climber = top_region.end_pos;
        } else {
          // drop down
        }
        overlapping_region_index_stack.pop_back();
        top_region = regions->at(overlapping_region_index_stack.back());
      }
      if (climber < next_region_begin_pos) {
        // cross a plateau/mesa/valley
        out_parts.push_back(NumberFormatSpan(top_region.field_id, climber,
                                             next_region_begin_pos));
        climber = next_region_begin_pos;
      }
    }
    if (region_iterator < regions->size()) {
      overlapping_region_index_stack.push_back(region_iterator++);
      top_region = regions->at(overlapping_region_index_stack.back());
    }
  }
  return out_parts;
}

BUILTIN(NumberFormatPrototypeFormatToParts) {
  const char* const method = "Intl.NumberFormat.prototype.formatToParts";
  HandleScope handle_scope(isolate);
  CHECK_RECEIVER(JSObject, number_format_holder, method);

  if (!Intl::IsObjectOfType(isolate, number_format_holder,
                            Intl::Type::kNumberFormat)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate,
        NewTypeError(MessageTemplate::kIncompatibleMethodReceiver,
                     isolate->factory()->NewStringFromAsciiChecked(method),
                     number_format_holder));
  }

  Handle<Object> x;
  if (args.length() >= 2) {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, x,
                                       Object::ToNumber(isolate, args.at(1)));
  } else {
    x = isolate->factory()->nan_value();
  }

  icu::DecimalFormat* number_format =
      NumberFormat::UnpackNumberFormat(number_format_holder);
  CHECK_NOT_NULL(number_format);

  Object* result = FormatNumberToParts(isolate, number_format, x->Number());
  return result;
}

BUILTIN(DateTimeFormatPrototypeFormatToParts) {
  const char* const method = "Intl.DateTimeFormat.prototype.formatToParts";
  HandleScope handle_scope(isolate);
  CHECK_RECEIVER(JSObject, date_format_holder, method);
  Factory* factory = isolate->factory();

  if (!Intl::IsObjectOfType(isolate, date_format_holder,
                            Intl::Type::kDateTimeFormat)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kIncompatibleMethodReceiver,
                              factory->NewStringFromAsciiChecked(method),
                              date_format_holder));
  }

  Handle<Object> x = args.atOrUndefined(isolate, 1);
  if (x->IsUndefined(isolate)) {
    x = factory->NewNumber(JSDate::CurrentTimeValue(isolate));
  } else {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, x,
                                       Object::ToNumber(isolate, args.at(1)));
  }

  double date_value = DateCache::TimeClip(x->Number());
  if (std::isnan(date_value)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kInvalidTimeValue));
  }

  icu::SimpleDateFormat* date_format =
      DateFormat::UnpackDateFormat(date_format_holder);
  CHECK_NOT_NULL(date_format);

  return FormatDateToParts(isolate, date_format, date_value);
}

BUILTIN(NumberFormatPrototypeFormatNumber) {
  const char* const method = "get Intl.NumberFormat.prototype.format";
  HandleScope scope(isolate);

  // 1. Let nf be the this value.
  // 2. If Type(nf) is not Object, throw a TypeError exception.
  CHECK_RECEIVER(JSReceiver, receiver, method);

  // 3. Let nf be ? UnwrapNumberFormat(nf).
  Handle<JSObject> number_format_holder;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, number_format_holder,
      NumberFormat::Unwrap(isolate, receiver, method));

  DCHECK(Intl::IsObjectOfType(isolate, number_format_holder,
                              Intl::Type::kNumberFormat));

  Handle<Object> bound_format = Handle<Object>(
      number_format_holder->GetEmbedderField(NumberFormat::kBoundFormatIndex),
      isolate);

  // 4. If nf.[[BoundFormat]] is undefined, then
  if (!bound_format->IsUndefined(isolate)) {
    DCHECK(bound_format->IsJSFunction());
    // 5. Return nf.[[BoundFormat]].
    return *bound_format;
  }

  Handle<Context> native_context =
      Handle<Context>(isolate->context()->native_context(), isolate);

  Handle<Context> context = isolate->factory()->NewBuiltinContext(
      native_context, NumberFormat::ContextSlot::kLength);

  // 4. b. Set F.[[NumberFormat]] to nf.
  context->set(NumberFormat::ContextSlot::kNumberFormat, *number_format_holder);

  Handle<SharedFunctionInfo> info = Handle<SharedFunctionInfo>(
      native_context->number_format_internal_format_number_shared_fun(),
      isolate);

  Handle<Map> map = isolate->strict_function_without_prototype_map();

  // 4. a. Let F be a new built-in function object as defined in
  // Number Format Functions (11.1.4).
  Handle<JSFunction> new_bound_format_function =
      isolate->factory()->NewFunctionFromSharedFunctionInfo(map, info, context);

  // 4. c. Set nf.[[BoundFormat]] to F.
  number_format_holder->SetEmbedderField(NumberFormat::kBoundFormatIndex,
                                         *new_bound_format_function);

  // 5. Return nf.[[BoundFormat]].
  return *new_bound_format_function;
}

BUILTIN(NumberFormatInternalFormatNumber) {
  HandleScope scope(isolate);

  Handle<Context> context = Handle<Context>(isolate->context(), isolate);

  // 1. Let nf be F.[[NumberFormat]].
  Handle<JSObject> number_format_holder = Handle<JSObject>(
      JSObject::cast(context->get(NumberFormat::ContextSlot::kNumberFormat)),
      isolate);

  // 2. Assert: Type(nf) is Object and nf has an
  //    [[InitializedNumberFormat]] internal slot.
  DCHECK(Intl::IsObjectOfType(isolate, number_format_holder,
                              Intl::Type::kNumberFormat));

  // 3. If value is not provided, let value be undefined.
  Handle<Object> value = args.atOrUndefined(isolate, 1);

  // 4. Let x be ? ToNumber(value).
  Handle<Object> number_obj;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, number_obj,
                                     Object::ToNumber(isolate, value));

  // Spec treats -0 as 0.
  if (number_obj->IsMinusZero()) {
    number_obj = Handle<Smi>(Smi::kZero, isolate);
  }

  double number = number_obj->Number();
  // Return FormatNumber(nf, x).
  RETURN_RESULT_OR_FAILURE(isolate, NumberFormat::FormatNumber(
                                        isolate, number_format_holder, number));
}

BUILTIN(ListFormatConstructor) {
  HandleScope scope(isolate);
  // 1. If NewTarget is undefined, throw a TypeError exception.
  if (args.new_target()->IsUndefined(isolate)) {  // [[Call]]
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kConstructorNotFunction,
                              isolate->factory()->NewStringFromStaticChars(
                                  "Intl.ListFormat")));
  }
  // [[Construct]]
  Handle<JSFunction> target = args.target();
  Handle<JSReceiver> new_target = Handle<JSReceiver>::cast(args.new_target());

  Handle<JSObject> result;
  // 2. Let listFormat be OrdinaryCreateFromConstructor(NewTarget,
  //    "%ListFormatPrototype%").
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     JSObject::New(target, new_target));
  Handle<JSListFormat> format = Handle<JSListFormat>::cast(result);
  format->set_flags(0);

  Handle<Object> locales = args.atOrUndefined(isolate, 1);
  Handle<Object> options = args.atOrUndefined(isolate, 2);

  // 3. Return InitializeListFormat(listFormat, locales, options).
  RETURN_RESULT_OR_FAILURE(isolate, JSListFormat::InitializeListFormat(
                                        isolate, format, locales, options));
}

namespace {

MaybeHandle<JSLocale> CreateLocale(Isolate* isolate,
                                   Handle<JSFunction> constructor,
                                   Handle<JSReceiver> new_target,
                                   Handle<Object> tag, Handle<Object> options) {
  Handle<JSObject> result;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, result,
                             JSObject::New(constructor, new_target), JSLocale);

  // First parameter is a locale, as a string/object. Can't be empty.
  if (!tag->IsName() && !tag->IsJSReceiver()) {
    THROW_NEW_ERROR(isolate, NewTypeError(MessageTemplate::kLocaleNotEmpty),
                    JSLocale);
  }

  Handle<String> locale_string;
  if (tag->IsJSLocale() && Handle<JSLocale>::cast(tag)->locale()->IsString()) {
    locale_string =
        Handle<String>(Handle<JSLocale>::cast(tag)->locale(), isolate);
  } else {
    ASSIGN_RETURN_ON_EXCEPTION(isolate, locale_string,
                               Object::ToString(isolate, tag), JSLocale);
  }

  Handle<JSReceiver> options_object;
  if (options->IsNullOrUndefined(isolate)) {
    // Make empty options bag.
    options_object = isolate->factory()->NewJSObjectWithNullProto();
  } else {
    ASSIGN_RETURN_ON_EXCEPTION(isolate, options_object,
                               Object::ToObject(isolate, options), JSLocale);
  }

  return JSLocale::InitializeLocale(isolate, Handle<JSLocale>::cast(result),
                                    locale_string, options_object);
}

}  // namespace

// Intl.Locale implementation
BUILTIN(LocaleConstructor) {
  HandleScope scope(isolate);
  if (args.new_target()->IsUndefined(isolate)) {  // [[Call]]
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kConstructorNotFunction,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "Intl.Locale")));
  }
  // [[Construct]]
  Handle<JSFunction> target = args.target();
  Handle<JSReceiver> new_target = Handle<JSReceiver>::cast(args.new_target());

  Handle<Object> tag = args.atOrUndefined(isolate, 1);
  Handle<Object> options = args.atOrUndefined(isolate, 2);

  RETURN_RESULT_OR_FAILURE(
      isolate, CreateLocale(isolate, target, new_target, tag, options));
}

BUILTIN(LocalePrototypeMaximize) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSLocale, locale_holder, "Intl.Locale.prototype.maximize");
  Handle<JSFunction> constructor(
      isolate->native_context()->intl_locale_function(), isolate);
  RETURN_RESULT_OR_FAILURE(
      isolate,
      CreateLocale(isolate, constructor, constructor,
                   JSLocale::Maximize(isolate, locale_holder->locale()),
                   isolate->factory()->NewJSObjectWithNullProto()));
}

BUILTIN(LocalePrototypeMinimize) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSLocale, locale_holder, "Intl.Locale.prototype.minimize");
  Handle<JSFunction> constructor(
      isolate->native_context()->intl_locale_function(), isolate);
  RETURN_RESULT_OR_FAILURE(
      isolate,
      CreateLocale(isolate, constructor, constructor,
                   JSLocale::Minimize(isolate, locale_holder->locale()),
                   isolate->factory()->NewJSObjectWithNullProto()));
}

namespace {

MaybeHandle<JSArray> GenerateRelativeTimeFormatParts(
    Isolate* isolate, icu::UnicodeString formatted,
    icu::UnicodeString integer_part, Handle<String> unit) {
  Factory* factory = isolate->factory();
  Handle<JSArray> array = factory->NewJSArray(0);
  int32_t found = formatted.indexOf(integer_part);

  if (found < 0) {
    // Cannot find the integer_part in the formatted.
    // Return [{'type': 'literal', 'value': formatted}]
    Maybe<bool> maybe_added_element =
        AddElement(isolate, array,
                   0,                          // index
                   factory->literal_string(),  // field_type_string
                   formatted,
                   0,                    // begin
                   formatted.length());  // end
    MAYBE_RETURN(maybe_added_element, MaybeHandle<JSArray>());

  } else {
    // Found the formatted integer in the result.
    int index = 0;

    // array.push({
    //     'type': 'literal',
    //     'value': formatted.substring(0, found)})
    if (found > 0) {
      Maybe<bool> maybe_added_element =
          AddElement(isolate, array, index++,
                     factory->literal_string(),  // field_type_string
                     formatted,
                     0,       // begin
                     found);  // end
      MAYBE_RETURN(maybe_added_element, MaybeHandle<JSArray>());
    }

    // array.push({
    //     'type': 'integer',
    //     'value': formatted.substring(found, found + integer_part.length),
    //     'unit': unit})
    Maybe<bool> maybe_added_element =
        AddElement(isolate, array, index++,
                   factory->integer_string(),  // field_type_string
                   formatted,
                   found,                          // begin
                   found + integer_part.length(),  // end
                   factory->unit_string(), unit);
    MAYBE_RETURN(maybe_added_element, MaybeHandle<JSArray>());

    // array.push({
    //     'type': 'literal',
    //     'value': formatted.substring(
    //         found + integer_part.length, formatted.length)})
    if (found + integer_part.length() < formatted.length()) {
      Maybe<bool> maybe_added_element =
          AddElement(isolate, array, index,
                     factory->literal_string(),  // field_type_string
                     formatted,
                     found + integer_part.length(),  // begin
                     formatted.length());            // end
      MAYBE_RETURN(maybe_added_element, MaybeHandle<JSArray>());
    }
  }
  return array;
}

bool GetURelativeDateTimeUnit(Handle<String> unit,
                              URelativeDateTimeUnit* unit_enum) {
  std::unique_ptr<char[]> unit_str = unit->ToCString();
  if ((strcmp("second", unit_str.get()) == 0) ||
      (strcmp("seconds", unit_str.get()) == 0)) {
    *unit_enum = UDAT_REL_UNIT_SECOND;
  } else if ((strcmp("minute", unit_str.get()) == 0) ||
             (strcmp("minutes", unit_str.get()) == 0)) {
    *unit_enum = UDAT_REL_UNIT_MINUTE;
  } else if ((strcmp("hour", unit_str.get()) == 0) ||
             (strcmp("hours", unit_str.get()) == 0)) {
    *unit_enum = UDAT_REL_UNIT_HOUR;
  } else if ((strcmp("day", unit_str.get()) == 0) ||
             (strcmp("days", unit_str.get()) == 0)) {
    *unit_enum = UDAT_REL_UNIT_DAY;
  } else if ((strcmp("week", unit_str.get()) == 0) ||
             (strcmp("weeks", unit_str.get()) == 0)) {
    *unit_enum = UDAT_REL_UNIT_WEEK;
  } else if ((strcmp("month", unit_str.get()) == 0) ||
             (strcmp("months", unit_str.get()) == 0)) {
    *unit_enum = UDAT_REL_UNIT_MONTH;
  } else if ((strcmp("quarter", unit_str.get()) == 0) ||
             (strcmp("quarters", unit_str.get()) == 0)) {
    *unit_enum = UDAT_REL_UNIT_QUARTER;
  } else if ((strcmp("year", unit_str.get()) == 0) ||
             (strcmp("years", unit_str.get()) == 0)) {
    *unit_enum = UDAT_REL_UNIT_YEAR;
  } else {
    return false;
  }
  return true;
}

Object* RelativeTimeFormatPrototypeFormatCommon(
    BuiltinArguments args, Isolate* isolate,
    Handle<JSRelativeTimeFormat> format_holder, const char* func_name,
    bool to_parts) {
  Factory* factory = isolate->factory();
  Handle<Object> value_obj = args.atOrUndefined(isolate, 1);
  Handle<Object> unit_obj = args.atOrUndefined(isolate, 2);

  // 3. Let value be ? ToNumber(value).
  Handle<Object> value;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, value,
                                     Object::ToNumber(isolate, value_obj));
  double number = value->Number();
  // 4. Let unit be ? ToString(unit).
  Handle<String> unit;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, unit,
                                     Object::ToString(isolate, unit_obj));

  // 4. If isFinite(value) is false, then throw a RangeError exception.
  if (!std::isfinite(number)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(
                     MessageTemplate::kNotFiniteNumber,
                     isolate->factory()->NewStringFromAsciiChecked(func_name)));
  }

  icu::RelativeDateTimeFormatter* formatter =
      JSRelativeTimeFormat::UnpackFormatter(format_holder);
  CHECK_NOT_NULL(formatter);

  URelativeDateTimeUnit unit_enum;
  if (!GetURelativeDateTimeUnit(unit, &unit_enum)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate,
        NewRangeError(MessageTemplate::kInvalidUnit,
                      isolate->factory()->NewStringFromAsciiChecked(func_name),
                      unit));
  }

  UErrorCode status = U_ZERO_ERROR;
  icu::UnicodeString formatted;
  if (unit_enum == UDAT_REL_UNIT_QUARTER) {
    // ICU have not yet implement UDAT_REL_UNIT_QUARTER.
  } else {
    if (format_holder->numeric() == JSRelativeTimeFormat::Numeric::ALWAYS) {
      formatter->formatNumeric(number, unit_enum, formatted, status);
    } else {
      DCHECK_EQ(JSRelativeTimeFormat::Numeric::AUTO, format_holder->numeric());
      formatter->format(number, unit_enum, formatted, status);
    }
  }

  if (U_FAILURE(status)) {
    // Internal ICU error.
    return ReadOnlyRoots(isolate).undefined_value();
  }

  if (to_parts) {
    icu::UnicodeString integer;
    icu::FieldPosition pos;
    formatter->getNumberFormat().format(std::abs(number), integer, pos, status);
    if (U_FAILURE(status)) {
      // Internal ICU error.
      return ReadOnlyRoots(isolate).undefined_value();
    }
    Handle<JSArray> elements;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, elements,
        GenerateRelativeTimeFormatParts(isolate, formatted, integer, unit));
    return *elements;
  }

  RETURN_RESULT_OR_FAILURE(
      isolate, factory->NewStringFromTwoByte(Vector<const uint16_t>(
                   reinterpret_cast<const uint16_t*>(formatted.getBuffer()),
                   formatted.length())));
}

}  // namespace

BUILTIN(RelativeTimeFormatPrototypeFormat) {
  HandleScope scope(isolate);
  // 1. Let relativeTimeFormat be the this value.
  // 2. If Type(relativeTimeFormat) is not Object or relativeTimeFormat does not
  //    have an [[InitializedRelativeTimeFormat]] internal slot whose value is
  //    true, throw a TypeError exception.
  CHECK_RECEIVER(JSRelativeTimeFormat, format_holder,
                 "Intl.RelativeTimeFormat.prototype.format");
  return RelativeTimeFormatPrototypeFormatCommon(args, isolate, format_holder,
                                                 "format", false);
}

BUILTIN(RelativeTimeFormatPrototypeFormatToParts) {
  HandleScope scope(isolate);
  // 1. Let relativeTimeFormat be the this value.
  // 2. If Type(relativeTimeFormat) is not Object or relativeTimeFormat does not
  //    have an [[InitializedRelativeTimeFormat]] internal slot whose value is
  //    true, throw a TypeError exception.
  CHECK_RECEIVER(JSRelativeTimeFormat, format_holder,
                 "Intl.RelativeTimeFormat.prototype.formatToParts");
  return RelativeTimeFormatPrototypeFormatCommon(args, isolate, format_holder,
                                                 "formatToParts", true);
}

// Locale getters.
BUILTIN(LocalePrototypeLanguage) {
  HandleScope scope(isolate);
  // CHECK_RECEIVER will case locale_holder to JSLocale.
  CHECK_RECEIVER(JSLocale, locale_holder, "Intl.Locale.prototype.language");

  return locale_holder->language();
}

BUILTIN(LocalePrototypeScript) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSLocale, locale_holder, "Intl.Locale.prototype.script");

  return locale_holder->script();
}

BUILTIN(LocalePrototypeRegion) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSLocale, locale_holder, "Intl.Locale.prototype.region");

  return locale_holder->region();
}

BUILTIN(LocalePrototypeBaseName) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSLocale, locale_holder, "Intl.Locale.prototype.baseName");

  return locale_holder->base_name();
}

BUILTIN(LocalePrototypeCalendar) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSLocale, locale_holder, "Intl.Locale.prototype.calendar");

  return locale_holder->calendar();
}

BUILTIN(LocalePrototypeCaseFirst) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSLocale, locale_holder, "Intl.Locale.prototype.caseFirst");

  return locale_holder->case_first();
}

BUILTIN(LocalePrototypeCollation) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSLocale, locale_holder, "Intl.Locale.prototype.collation");

  return locale_holder->collation();
}

BUILTIN(LocalePrototypeHourCycle) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSLocale, locale_holder, "Intl.Locale.prototype.hourCycle");

  return locale_holder->hour_cycle();
}

BUILTIN(LocalePrototypeNumeric) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSLocale, locale_holder, "Intl.Locale.prototype.numeric");

  return locale_holder->numeric();
}

BUILTIN(LocalePrototypeNumberingSystem) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSLocale, locale_holder,
                 "Intl.Locale.prototype.numberingSystem");

  return locale_holder->numbering_system();
}

BUILTIN(LocalePrototypeToString) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSLocale, locale_holder, "Intl.Locale.prototype.toString");

  return locale_holder->locale();
}

BUILTIN(RelativeTimeFormatConstructor) {
  HandleScope scope(isolate);
  // 1. If NewTarget is undefined, throw a TypeError exception.
  if (args.new_target()->IsUndefined(isolate)) {  // [[Call]]
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kConstructorNotFunction,
                              isolate->factory()->NewStringFromStaticChars(
                                  "Intl.RelativeTimeFormat")));
  }
  // [[Construct]]
  Handle<JSFunction> target = args.target();
  Handle<JSReceiver> new_target = Handle<JSReceiver>::cast(args.new_target());

  Handle<JSObject> result;
  // 2. Let relativeTimeFormat be
  //    ! OrdinaryCreateFromConstructor(NewTarget,
  //                                    "%RelativeTimeFormatPrototype%").
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     JSObject::New(target, new_target));
  Handle<JSRelativeTimeFormat> format =
      Handle<JSRelativeTimeFormat>::cast(result);
  format->set_flags(0);

  Handle<Object> locales = args.atOrUndefined(isolate, 1);
  Handle<Object> options = args.atOrUndefined(isolate, 2);

  // 3. Return ? InitializeRelativeTimeFormat(relativeTimeFormat, locales,
  //                                          options).
  RETURN_RESULT_OR_FAILURE(isolate,
                           JSRelativeTimeFormat::InitializeRelativeTimeFormat(
                               isolate, format, locales, options));
}

BUILTIN(RelativeTimeFormatPrototypeResolvedOptions) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSRelativeTimeFormat, format_holder,
                 "Intl.RelativeTimeFormat.prototype.resolvedOptions");
  return *JSRelativeTimeFormat::ResolvedOptions(isolate, format_holder);
}

}  // namespace internal
}  // namespace v8
