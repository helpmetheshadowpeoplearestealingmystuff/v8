// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include "src/runtime/runtime-utils.h"

#include <cmath>
#include <memory>

#include "src/api-inl.h"
#include "src/api-natives.h"
#include "src/arguments.h"
#include "src/date.h"
#include "src/global-handles.h"
#include "src/heap/factory.h"
#include "src/intl.h"
#include "src/isolate-inl.h"
#include "src/messages.h"
#include "src/objects/intl-objects-inl.h"
#include "src/objects/intl-objects.h"
#include "src/objects/managed.h"
#include "src/utils.h"

#include "unicode/brkiter.h"
#include "unicode/calendar.h"
#include "unicode/coll.h"
#include "unicode/curramt.h"
#include "unicode/datefmt.h"
#include "unicode/dcfmtsym.h"
#include "unicode/decimfmt.h"
#include "unicode/dtfmtsym.h"
#include "unicode/dtptngen.h"
#include "unicode/locid.h"
#include "unicode/numfmt.h"
#include "unicode/numsys.h"
#include "unicode/plurrule.h"
#include "unicode/rbbi.h"
#include "unicode/smpdtfmt.h"
#include "unicode/timezone.h"
#include "unicode/uchar.h"
#include "unicode/ucol.h"
#include "unicode/ucurr.h"
#include "unicode/uloc.h"
#include "unicode/unistr.h"
#include "unicode/unum.h"
#include "unicode/uversion.h"


namespace v8 {
namespace internal {

// ECMA 402 6.2.3
RUNTIME_FUNCTION(Runtime_CanonicalizeLanguageTag) {
  HandleScope scope(isolate);

  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, locale, 0);

  RETURN_RESULT_OR_FAILURE(isolate,
                           Intl::CanonicalizeLanguageTag(isolate, locale));
}

RUNTIME_FUNCTION(Runtime_AvailableLocalesOf) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, service, 0);
  Handle<JSObject> locales;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, locales, Intl::AvailableLocalesOf(isolate, service));
  return *locales;
}

RUNTIME_FUNCTION(Runtime_GetDefaultICULocale) {
  HandleScope scope(isolate);

  DCHECK_EQ(0, args.length());
  return *Intl::DefaultLocale(isolate);
}

RUNTIME_FUNCTION(Runtime_IsWellFormedCurrencyCode) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, currency, 0);
  return *(isolate->factory()->ToBoolean(
      Intl::IsWellFormedCurrencyCode(isolate, currency)));
}

RUNTIME_FUNCTION(Runtime_DefineWEProperty) {
  HandleScope scope(isolate);

  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSObject, target, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, key, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  Intl::DefineWEProperty(isolate, target, key, value);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_IsInitializedIntlObjectOfType) {
  HandleScope scope(isolate);

  DCHECK_EQ(2, args.length());

  CONVERT_ARG_HANDLE_CHECKED(Object, input, 0);
  CONVERT_SMI_ARG_CHECKED(expected_type_int, 1);

  Intl::Type expected_type = Intl::TypeFromInt(expected_type_int);

  return isolate->heap()->ToBoolean(
      Intl::IsObjectOfType(isolate, input, expected_type));
}

RUNTIME_FUNCTION(Runtime_MarkAsInitializedIntlObjectOfType) {
  HandleScope scope(isolate);

  DCHECK_EQ(2, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSObject, input, 0);
  CONVERT_ARG_HANDLE_CHECKED(Smi, type, 1);

#ifdef DEBUG
  // TypeFromSmi does correctness checks.
  Intl::Type type_intl = Intl::TypeFromSmi(*type);
  USE(type_intl);
#endif

  Handle<Symbol> marker = isolate->factory()->intl_initialized_marker_symbol();
  JSObject::SetProperty(isolate, input, marker, type, LanguageMode::kStrict)
      .Assert();

  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_CreateDateTimeFormat) {
  HandleScope scope(isolate);

  DCHECK_EQ(3, args.length());

  CONVERT_ARG_HANDLE_CHECKED(String, locale, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, options, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, resolved, 2);

  Handle<JSFunction> constructor(
      isolate->native_context()->intl_date_time_format_function(), isolate);

  Handle<JSObject> local_object;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, local_object,
                                     JSObject::New(constructor, constructor));

  // Set date time formatter as embedder field of the resulting JS object.
  icu::SimpleDateFormat* date_format =
      DateFormat::InitializeDateTimeFormat(isolate, locale, options, resolved);
  CHECK_NOT_NULL(date_format);

  local_object->SetEmbedderField(0, reinterpret_cast<Smi*>(date_format));

  // Make object handle weak so we can delete the data format once GC kicks in.
  Handle<Object> wrapper = isolate->global_handles()->Create(*local_object);
  GlobalHandles::MakeWeak(wrapper.location(), wrapper.location(),
                          DateFormat::DeleteDateFormat,
                          WeakCallbackType::kInternalFields);
  return *local_object;
}

RUNTIME_FUNCTION(Runtime_InternalDateFormat) {
  HandleScope scope(isolate);

  DCHECK_EQ(2, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSObject, date_format_holder, 0);
  CONVERT_NUMBER_ARG_HANDLE_CHECKED(date, 1);

  double date_value = DateCache::TimeClip(date->Number());
  if (std::isnan(date_value)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kInvalidTimeValue));
  }

  icu::SimpleDateFormat* date_format =
      DateFormat::UnpackDateFormat(date_format_holder);
  CHECK_NOT_NULL(date_format);

  icu::UnicodeString result;
  date_format->format(date_value, result);

  RETURN_RESULT_OR_FAILURE(
      isolate, isolate->factory()->NewStringFromTwoByte(Vector<const uint16_t>(
                   reinterpret_cast<const uint16_t*>(result.getBuffer()),
                   result.length())));
}

RUNTIME_FUNCTION(Runtime_CreateNumberFormat) {
  HandleScope scope(isolate);

  DCHECK_EQ(3, args.length());

  CONVERT_ARG_HANDLE_CHECKED(String, locale, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, options, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, resolved, 2);
  RETURN_RESULT_OR_FAILURE(
      isolate, Intl::CreateNumberFormat(isolate, locale, options, resolved));
}

RUNTIME_FUNCTION(Runtime_InternalNumberFormat) {
  HandleScope scope(isolate);

  DCHECK_EQ(2, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSObject, number_format_holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 1);

  Handle<Object> number_obj;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, number_obj,
                                     Object::ToNumber(isolate, value));

  double number = number_obj->Number();
  RETURN_RESULT_OR_FAILURE(isolate, NumberFormat::FormatNumber(
                                        isolate, number_format_holder, number));
}

RUNTIME_FUNCTION(Runtime_CurrencyDigits) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, currency, 0);
  return *Intl::CurrencyDigits(isolate, currency);
}

RUNTIME_FUNCTION(Runtime_CreateCollator) {
  HandleScope scope(isolate);

  DCHECK_EQ(3, args.length());

  CONVERT_ARG_HANDLE_CHECKED(String, locale, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, options, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, resolved, 2);

  Handle<JSFunction> constructor(
      isolate->native_context()->intl_collator_function(), isolate);

  Handle<JSObject> collator_holder;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, collator_holder,
                                     JSObject::New(constructor, constructor));

  icu::Collator* collator =
      Collator::InitializeCollator(isolate, locale, options, resolved);
  CHECK_NOT_NULL(collator);

  Handle<Managed<icu::Collator>> managed =
      Managed<icu::Collator>::FromRawPtr(isolate, 0, collator);
  collator_holder->SetEmbedderField(0, *managed);

  return *collator_holder;
}

RUNTIME_FUNCTION(Runtime_InternalCompare) {
  HandleScope scope(isolate);

  DCHECK_EQ(3, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSObject, collator_holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, string1, 1);
  CONVERT_ARG_HANDLE_CHECKED(String, string2, 2);

  icu::Collator* collator = Collator::UnpackCollator(collator_holder);
  CHECK_NOT_NULL(collator);

  string1 = String::Flatten(isolate, string1);
  string2 = String::Flatten(isolate, string2);

  UCollationResult result;
  UErrorCode status = U_ZERO_ERROR;
  {
    DisallowHeapAllocation no_gc;
    int32_t length1 = string1->length();
    int32_t length2 = string2->length();
    String::FlatContent flat1 = string1->GetFlatContent();
    String::FlatContent flat2 = string2->GetFlatContent();
    std::unique_ptr<uc16[]> sap1;
    std::unique_ptr<uc16[]> sap2;
    icu::UnicodeString string_val1(
        FALSE, GetUCharBufferFromFlat(flat1, &sap1, length1), length1);
    icu::UnicodeString string_val2(
        FALSE, GetUCharBufferFromFlat(flat2, &sap2, length2), length2);
    result = collator->compare(string_val1, string_val2, status);
  }
  if (U_FAILURE(status)) return isolate->ThrowIllegalOperation();

  return *isolate->factory()->NewNumberFromInt(result);
}

RUNTIME_FUNCTION(Runtime_CreatePluralRules) {
  HandleScope scope(isolate);

  DCHECK_EQ(3, args.length());

  CONVERT_ARG_HANDLE_CHECKED(String, locale, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, options, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, resolved, 2);

  Handle<JSFunction> constructor(
      isolate->native_context()->intl_plural_rules_function(), isolate);

  Handle<JSObject> local_object;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, local_object,
                                     JSObject::New(constructor, constructor));

  // Set pluralRules as internal field of the resulting JS object.
  icu::PluralRules* plural_rules;
  icu::DecimalFormat* decimal_format;
  PluralRules::InitializePluralRules(isolate, locale, options, resolved,
                                     &plural_rules, &decimal_format);
  CHECK_NOT_NULL(plural_rules);
  CHECK_NOT_NULL(decimal_format);

  local_object->SetEmbedderField(0, reinterpret_cast<Smi*>(plural_rules));
  local_object->SetEmbedderField(1, reinterpret_cast<Smi*>(decimal_format));

  Handle<Object> wrapper = isolate->global_handles()->Create(*local_object);
  GlobalHandles::MakeWeak(wrapper.location(), wrapper.location(),
                          PluralRules::DeletePluralRules,
                          WeakCallbackType::kInternalFields);
  return *local_object;
}

RUNTIME_FUNCTION(Runtime_PluralRulesSelect) {
  HandleScope scope(isolate);

  DCHECK_EQ(2, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSObject, plural_rules_holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, number, 1);

  icu::PluralRules* plural_rules =
      PluralRules::UnpackPluralRules(plural_rules_holder);
  CHECK_NOT_NULL(plural_rules);

  icu::DecimalFormat* number_format =
      PluralRules::UnpackNumberFormat(plural_rules_holder);
  CHECK_NOT_NULL(number_format);

  // Currently, PluralRules doesn't implement all the options for rounding that
  // the Intl spec provides; format and parse the number to round to the
  // appropriate amount, then apply PluralRules.
  //
  // TODO(littledan): If a future ICU version supports an extended API to avoid
  // this step, then switch to that API. Bug thread:
  // http://bugs.icu-project.org/trac/ticket/12763
  icu::UnicodeString rounded_string;
  number_format->format(number->Number(), rounded_string);

  icu::Formattable formattable;
  UErrorCode status = U_ZERO_ERROR;
  number_format->parse(rounded_string, formattable, status);
  if (!U_SUCCESS(status)) return isolate->ThrowIllegalOperation();

  double rounded = formattable.getDouble(status);
  if (!U_SUCCESS(status)) return isolate->ThrowIllegalOperation();

  icu::UnicodeString result = plural_rules->select(rounded);
  return *isolate->factory()
              ->NewStringFromTwoByte(Vector<const uint16_t>(
                  reinterpret_cast<const uint16_t*>(result.getBuffer()),
                  result.length()))
              .ToHandleChecked();
}

RUNTIME_FUNCTION(Runtime_CreateBreakIterator) {
  HandleScope scope(isolate);

  DCHECK_EQ(3, args.length());

  CONVERT_ARG_HANDLE_CHECKED(String, locale, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, options, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, resolved, 2);

  Handle<JSFunction> constructor(
      isolate->native_context()->intl_v8_break_iterator_function(), isolate);

  Handle<JSObject> local_object;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, local_object,
                                     JSObject::New(constructor, constructor));

  // Set break iterator as embedder field of the resulting JS object.
  icu::BreakIterator* break_iterator = V8BreakIterator::InitializeBreakIterator(
      isolate, locale, options, resolved);
  CHECK_NOT_NULL(break_iterator);

  if (!break_iterator) return isolate->ThrowIllegalOperation();

  local_object->SetEmbedderField(0, reinterpret_cast<Smi*>(break_iterator));
  // Make sure that the pointer to adopted text is nullptr.
  local_object->SetEmbedderField(1, static_cast<Smi*>(nullptr));

  // Make object handle weak so we can delete the break iterator once GC kicks
  // in.
  Handle<Object> wrapper = isolate->global_handles()->Create(*local_object);
  GlobalHandles::MakeWeak(wrapper.location(), wrapper.location(),
                          V8BreakIterator::DeleteBreakIterator,
                          WeakCallbackType::kInternalFields);
  return *local_object;
}

RUNTIME_FUNCTION(Runtime_BreakIteratorAdoptText) {
  HandleScope scope(isolate);

  DCHECK_EQ(2, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSObject, break_iterator_holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, text, 1);

  icu::BreakIterator* break_iterator =
      V8BreakIterator::UnpackBreakIterator(break_iterator_holder);
  CHECK_NOT_NULL(break_iterator);

  icu::UnicodeString* u_text = reinterpret_cast<icu::UnicodeString*>(
      break_iterator_holder->GetEmbedderField(1));
  delete u_text;

  int length = text->length();
  text = String::Flatten(isolate, text);
  DisallowHeapAllocation no_gc;
  String::FlatContent flat = text->GetFlatContent();
  std::unique_ptr<uc16[]> sap;
  const UChar* text_value = GetUCharBufferFromFlat(flat, &sap, length);
  u_text = new icu::UnicodeString(text_value, length);
  break_iterator_holder->SetEmbedderField(1, reinterpret_cast<Smi*>(u_text));

  break_iterator->setText(*u_text);

  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_BreakIteratorFirst) {
  HandleScope scope(isolate);

  DCHECK_EQ(1, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSObject, break_iterator_holder, 0);

  icu::BreakIterator* break_iterator =
      V8BreakIterator::UnpackBreakIterator(break_iterator_holder);
  CHECK_NOT_NULL(break_iterator);

  return *isolate->factory()->NewNumberFromInt(break_iterator->first());
}

RUNTIME_FUNCTION(Runtime_BreakIteratorNext) {
  HandleScope scope(isolate);

  DCHECK_EQ(1, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSObject, break_iterator_holder, 0);

  icu::BreakIterator* break_iterator =
      V8BreakIterator::UnpackBreakIterator(break_iterator_holder);
  CHECK_NOT_NULL(break_iterator);

  return *isolate->factory()->NewNumberFromInt(break_iterator->next());
}

RUNTIME_FUNCTION(Runtime_BreakIteratorCurrent) {
  HandleScope scope(isolate);

  DCHECK_EQ(1, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSObject, break_iterator_holder, 0);

  icu::BreakIterator* break_iterator =
      V8BreakIterator::UnpackBreakIterator(break_iterator_holder);
  CHECK_NOT_NULL(break_iterator);

  return *isolate->factory()->NewNumberFromInt(break_iterator->current());
}

RUNTIME_FUNCTION(Runtime_BreakIteratorBreakType) {
  HandleScope scope(isolate);

  DCHECK_EQ(1, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSObject, break_iterator_holder, 0);

  icu::BreakIterator* break_iterator =
      V8BreakIterator::UnpackBreakIterator(break_iterator_holder);
  CHECK_NOT_NULL(break_iterator);

  // TODO(cira): Remove cast once ICU fixes base BreakIterator class.
  icu::RuleBasedBreakIterator* rule_based_iterator =
      static_cast<icu::RuleBasedBreakIterator*>(break_iterator);
  int32_t status = rule_based_iterator->getRuleStatus();
  // Keep return values in sync with JavaScript BreakType enum.
  if (status >= UBRK_WORD_NONE && status < UBRK_WORD_NONE_LIMIT) {
    return *isolate->factory()->NewStringFromStaticChars("none");
  } else if (status >= UBRK_WORD_NUMBER && status < UBRK_WORD_NUMBER_LIMIT) {
    return ReadOnlyRoots(isolate).number_string();
  } else if (status >= UBRK_WORD_LETTER && status < UBRK_WORD_LETTER_LIMIT) {
    return *isolate->factory()->NewStringFromStaticChars("letter");
  } else if (status >= UBRK_WORD_KANA && status < UBRK_WORD_KANA_LIMIT) {
    return *isolate->factory()->NewStringFromStaticChars("kana");
  } else if (status >= UBRK_WORD_IDEO && status < UBRK_WORD_IDEO_LIMIT) {
    return *isolate->factory()->NewStringFromStaticChars("ideo");
  } else {
    return *isolate->factory()->NewStringFromStaticChars("unknown");
  }
}

RUNTIME_FUNCTION(Runtime_StringToLowerCaseIntl) {
  HandleScope scope(isolate);
  DCHECK_EQ(args.length(), 1);
  CONVERT_ARG_HANDLE_CHECKED(String, s, 0);
  s = String::Flatten(isolate, s);
  return ConvertToLower(s, isolate);
}

RUNTIME_FUNCTION(Runtime_StringToUpperCaseIntl) {
  HandleScope scope(isolate);
  DCHECK_EQ(args.length(), 1);
  CONVERT_ARG_HANDLE_CHECKED(String, s, 0);
  s = String::Flatten(isolate, s);
  return ConvertToUpper(s, isolate);
}

RUNTIME_FUNCTION(Runtime_DateCacheVersion) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  if (isolate->serializer_enabled())
    return ReadOnlyRoots(isolate).undefined_value();
  if (!isolate->eternal_handles()->Exists(EternalHandles::DATE_CACHE_VERSION)) {
    Handle<FixedArray> date_cache_version =
        isolate->factory()->NewFixedArray(1, TENURED);
    date_cache_version->set(0, Smi::kZero);
    isolate->eternal_handles()->CreateSingleton(
        isolate, *date_cache_version, EternalHandles::DATE_CACHE_VERSION);
  }
  Handle<FixedArray> date_cache_version =
      Handle<FixedArray>::cast(isolate->eternal_handles()->GetSingleton(
          EternalHandles::DATE_CACHE_VERSION));
  return date_cache_version->get(0);
}

RUNTIME_FUNCTION(Runtime_IntlUnwrapReceiver) {
  HandleScope scope(isolate);
  DCHECK_EQ(5, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, receiver, 0);
  CONVERT_SMI_ARG_CHECKED(type_int, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, constructor, 2);
  CONVERT_ARG_HANDLE_CHECKED(String, method, 3);
  CONVERT_BOOLEAN_ARG_CHECKED(check_legacy_constructor, 4);

  RETURN_RESULT_OR_FAILURE(
      isolate, Intl::UnwrapReceiver(isolate, receiver, constructor,
                                    Intl::TypeFromInt(type_int), method,
                                    check_legacy_constructor));
}

}  // namespace internal
}  // namespace v8
