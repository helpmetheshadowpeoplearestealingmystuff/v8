// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// -------------------------------------------------------------------

(function(global, utils) {

"use strict";

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

var GlobalDate = global.Date;
var GlobalObject = global.Object;
var InternalArray = utils.InternalArray;
var IsFinite;
var MakeRangeError;
var MathAbs;
var MathFloor;
var NaN = %GetRootNaN();

utils.Import(function(from) {
  IsFinite = from.IsFinite;
  MakeRangeError = from.MakeRangeError;
  MathAbs = from.MathAbs;
  MathFloor = from.MathFloor;
});

// -------------------------------------------------------------------

// This file contains date support implemented in JavaScript.

var timezone_cache_time = NaN;
var timezone_cache_timezone;

function LocalTimezone(t) {
  if (NUMBER_IS_NAN(t)) return "";
  CheckDateCacheCurrent();
  if (t == timezone_cache_time) {
    return timezone_cache_timezone;
  }
  var timezone = %DateLocalTimezone(t);
  timezone_cache_time = t;
  timezone_cache_timezone = timezone;
  return timezone;
}


// ECMA 262 - 15.9.1.11
function MakeTime(hour, min, sec, ms) {
  if (!IsFinite(hour)) return NaN;
  if (!IsFinite(min)) return NaN;
  if (!IsFinite(sec)) return NaN;
  if (!IsFinite(ms)) return NaN;
  return TO_INTEGER(hour) * msPerHour
      + TO_INTEGER(min) * msPerMinute
      + TO_INTEGER(sec) * msPerSecond
      + TO_INTEGER(ms);
}


// Compute number of days given a year, month, date.
// Note that month and date can lie outside the normal range.
//   For example:
//     MakeDay(2007, -4, 20) --> MakeDay(2006, 8, 20)
//     MakeDay(2007, -33, 1) --> MakeDay(2004, 3, 1)
//     MakeDay(2007, 14, -50) --> MakeDay(2007, 8, 11)
function MakeDay(year, month, date) {
  if (!IsFinite(year) || !IsFinite(month) || !IsFinite(date)) return NaN;

  // Convert to integer and map -0 to 0.
  year = TO_INTEGER_MAP_MINUS_ZERO(year);
  month = TO_INTEGER_MAP_MINUS_ZERO(month);
  date = TO_INTEGER_MAP_MINUS_ZERO(date);

  if (year < kMinYear || year > kMaxYear ||
      month < kMinMonth || month > kMaxMonth) {
    return NaN;
  }

  // Now we rely on year and month being SMIs.
  return %DateMakeDay(year | 0, month | 0) + date - 1;
}


// ECMA 262 - 15.9.1.13
function MakeDate(day, time) {
  var time = day * msPerDay + time;
  // Some of our runtime funtions for computing UTC(time) rely on
  // times not being significantly larger than MAX_TIME_MS. If there
  // is no way that the time can be within range even after UTC
  // conversion we return NaN immediately instead of relying on
  // TimeClip to do it.
  if (MathAbs(time) > MAX_TIME_BEFORE_UTC) return NaN;
  return time;
}


// ECMA 262 - 15.9.1.14
function TimeClip(time) {
  if (!IsFinite(time)) return NaN;
  if (MathAbs(time) > MAX_TIME_MS) return NaN;
  return TO_INTEGER(time) + 0;
}


var WeekDays = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'];
var Months = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun',
              'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];


function TwoDigitString(value) {
  return value < 10 ? "0" + value : "" + value;
}


function DateString(date) {
  CHECK_DATE(date);
  return WeekDays[LOCAL_WEEKDAY(date)] + ' '
      + Months[LOCAL_MONTH(date)] + ' '
      + TwoDigitString(LOCAL_DAY(date)) + ' '
      + LOCAL_YEAR(date);
}


var LongWeekDays = ['Sunday', 'Monday', 'Tuesday', 'Wednesday',
    'Thursday', 'Friday', 'Saturday'];
var LongMonths = ['January', 'February', 'March', 'April', 'May', 'June',
    'July', 'August', 'September', 'October', 'November', 'December'];


function LongDateString(date) {
  CHECK_DATE(date);
  return LongWeekDays[LOCAL_WEEKDAY(date)] + ', '
      + LongMonths[LOCAL_MONTH(date)] + ' '
      + TwoDigitString(LOCAL_DAY(date)) + ', '
      + LOCAL_YEAR(date);
}


function TimeString(date) {
  CHECK_DATE(date);
  return TwoDigitString(LOCAL_HOUR(date)) + ':'
      + TwoDigitString(LOCAL_MIN(date)) + ':'
      + TwoDigitString(LOCAL_SEC(date));
}


function TimeStringUTC(date) {
  CHECK_DATE(date);
  return TwoDigitString(UTC_HOUR(date)) + ':'
      + TwoDigitString(UTC_MIN(date)) + ':'
      + TwoDigitString(UTC_SEC(date));
}


function LocalTimezoneString(date) {
  CHECK_DATE(date);
  var timezone = LocalTimezone(UTC_DATE_VALUE(date));

  var timezoneOffset = -TIMEZONE_OFFSET(date);
  var sign = (timezoneOffset >= 0) ? 1 : -1;
  var hours = MathFloor((sign * timezoneOffset)/60);
  var min   = MathFloor((sign * timezoneOffset)%60);
  var gmt = ' GMT' + ((sign == 1) ? '+' : '-') +
      TwoDigitString(hours) + TwoDigitString(min);
  return gmt + ' (' +  timezone + ')';
}


function DatePrintString(date) {
  CHECK_DATE(date);
  return DateString(date) + ' ' + TimeString(date);
}

// -------------------------------------------------------------------

// ECMA 262 - 15.9.5.2
function DateToString() {
  CHECK_DATE(this);
  var t = UTC_DATE_VALUE(this)
  if (NUMBER_IS_NAN(t)) return kInvalidDate;
  var time_zone_string = LocalTimezoneString(this)
  return DatePrintString(this) + time_zone_string;
}


// ECMA 262 - 15.9.5.3
function DateToDateString() {
  CHECK_DATE(this);
  var t = UTC_DATE_VALUE(this);
  if (NUMBER_IS_NAN(t)) return kInvalidDate;
  return DateString(this);
}


// ECMA 262 - 15.9.5.4
function DateToTimeString() {
  CHECK_DATE(this);
  var t = UTC_DATE_VALUE(this);
  if (NUMBER_IS_NAN(t)) return kInvalidDate;
  var time_zone_string = LocalTimezoneString(this);
  return TimeString(this) + time_zone_string;
}


// ECMA 262 - 15.9.5.5
function DateToLocaleString() {
  CHECK_DATE(this);
  return %_Call(DateToString, this);
}


// ECMA 262 - 15.9.5.6
function DateToLocaleDateString() {
  CHECK_DATE(this);
  var t = UTC_DATE_VALUE(this);
  if (NUMBER_IS_NAN(t)) return kInvalidDate;
  return LongDateString(this);
}


// ECMA 262 - 15.9.5.7
function DateToLocaleTimeString() {
  CHECK_DATE(this);
  var t = UTC_DATE_VALUE(this);
  if (NUMBER_IS_NAN(t)) return kInvalidDate;
  return TimeString(this);
}


// ECMA 262 - 15.9.5.27
function DateSetTime(ms) {
  CHECK_DATE(this);
  SET_UTC_DATE_VALUE(this, TO_NUMBER(ms));
  return UTC_DATE_VALUE(this);
}


// ECMA 262 - 15.9.5.28
function DateSetMilliseconds(ms) {
  CHECK_DATE(this);
  var t = LOCAL_DATE_VALUE(this);
  ms = TO_NUMBER(ms);
  var time = MakeTime(LOCAL_HOUR(this), LOCAL_MIN(this), LOCAL_SEC(this), ms);
  return SET_LOCAL_DATE_VALUE(this, MakeDate(LOCAL_DAYS(this), time));
}


// ECMA 262 - 15.9.5.29
function DateSetUTCMilliseconds(ms) {
  CHECK_DATE(this);
  var t = UTC_DATE_VALUE(this);
  ms = TO_NUMBER(ms);
  var time = MakeTime(UTC_HOUR(this),
                      UTC_MIN(this),
                      UTC_SEC(this),
                      ms);
  return SET_UTC_DATE_VALUE(this, MakeDate(UTC_DAYS(this), time));
}


// ECMA 262 - 15.9.5.30
function DateSetSeconds(sec, ms) {
  CHECK_DATE(this);
  var t = LOCAL_DATE_VALUE(this);
  sec = TO_NUMBER(sec);
  ms = %_ArgumentsLength() < 2 ? LOCAL_MS(this) : TO_NUMBER(ms);
  var time = MakeTime(LOCAL_HOUR(this), LOCAL_MIN(this), sec, ms);
  return SET_LOCAL_DATE_VALUE(this, MakeDate(LOCAL_DAYS(this), time));
}


// ECMA 262 - 15.9.5.31
function DateSetUTCSeconds(sec, ms) {
  CHECK_DATE(this);
  var t = UTC_DATE_VALUE(this);
  sec = TO_NUMBER(sec);
  ms = %_ArgumentsLength() < 2 ? UTC_MS(this) : TO_NUMBER(ms);
  var time = MakeTime(UTC_HOUR(this), UTC_MIN(this), sec, ms);
  return SET_UTC_DATE_VALUE(this, MakeDate(UTC_DAYS(this), time));
}


// ECMA 262 - 15.9.5.33
function DateSetMinutes(min, sec, ms) {
  CHECK_DATE(this);
  var t = LOCAL_DATE_VALUE(this);
  min = TO_NUMBER(min);
  var argc = %_ArgumentsLength();
  sec = argc < 2 ? LOCAL_SEC(this) : TO_NUMBER(sec);
  ms = argc < 3 ? LOCAL_MS(this) : TO_NUMBER(ms);
  var time = MakeTime(LOCAL_HOUR(this), min, sec, ms);
  return SET_LOCAL_DATE_VALUE(this, MakeDate(LOCAL_DAYS(this), time));
}


// ECMA 262 - 15.9.5.34
function DateSetUTCMinutes(min, sec, ms) {
  CHECK_DATE(this);
  var t = UTC_DATE_VALUE(this);
  min = TO_NUMBER(min);
  var argc = %_ArgumentsLength();
  sec = argc < 2 ? UTC_SEC(this) : TO_NUMBER(sec);
  ms = argc < 3 ? UTC_MS(this) : TO_NUMBER(ms);
  var time = MakeTime(UTC_HOUR(this), min, sec, ms);
  return SET_UTC_DATE_VALUE(this, MakeDate(UTC_DAYS(this), time));
}


// ECMA 262 - 15.9.5.35
function DateSetHours(hour, min, sec, ms) {
  CHECK_DATE(this);
  var t = LOCAL_DATE_VALUE(this);
  hour = TO_NUMBER(hour);
  var argc = %_ArgumentsLength();
  min = argc < 2 ? LOCAL_MIN(this) : TO_NUMBER(min);
  sec = argc < 3 ? LOCAL_SEC(this) : TO_NUMBER(sec);
  ms = argc < 4 ? LOCAL_MS(this) : TO_NUMBER(ms);
  var time = MakeTime(hour, min, sec, ms);
  return SET_LOCAL_DATE_VALUE(this, MakeDate(LOCAL_DAYS(this), time));
}


// ECMA 262 - 15.9.5.34
function DateSetUTCHours(hour, min, sec, ms) {
  CHECK_DATE(this);
  var t = UTC_DATE_VALUE(this);
  hour = TO_NUMBER(hour);
  var argc = %_ArgumentsLength();
  min = argc < 2 ? UTC_MIN(this) : TO_NUMBER(min);
  sec = argc < 3 ? UTC_SEC(this) : TO_NUMBER(sec);
  ms = argc < 4 ? UTC_MS(this) : TO_NUMBER(ms);
  var time = MakeTime(hour, min, sec, ms);
  return SET_UTC_DATE_VALUE(this, MakeDate(UTC_DAYS(this), time));
}


// ECMA 262 - 15.9.5.36
function DateSetDate(date) {
  CHECK_DATE(this);
  var t = LOCAL_DATE_VALUE(this);
  date = TO_NUMBER(date);
  var day = MakeDay(LOCAL_YEAR(this), LOCAL_MONTH(this), date);
  return SET_LOCAL_DATE_VALUE(this, MakeDate(day, LOCAL_TIME_IN_DAY(this)));
}


// ECMA 262 - 15.9.5.37
function DateSetUTCDate(date) {
  CHECK_DATE(this);
  var t = UTC_DATE_VALUE(this);
  date = TO_NUMBER(date);
  var day = MakeDay(UTC_YEAR(this), UTC_MONTH(this), date);
  return SET_UTC_DATE_VALUE(this, MakeDate(day, UTC_TIME_IN_DAY(this)));
}


// ECMA 262 - 15.9.5.38
function DateSetMonth(month, date) {
  CHECK_DATE(this);
  var t = LOCAL_DATE_VALUE(this);
  month = TO_NUMBER(month);
  date = %_ArgumentsLength() < 2 ? LOCAL_DAY(this) : TO_NUMBER(date);
  var day = MakeDay(LOCAL_YEAR(this), month, date);
  return SET_LOCAL_DATE_VALUE(this, MakeDate(day, LOCAL_TIME_IN_DAY(this)));
}


// ECMA 262 - 15.9.5.39
function DateSetUTCMonth(month, date) {
  CHECK_DATE(this);
  var t = UTC_DATE_VALUE(this);
  month = TO_NUMBER(month);
  date = %_ArgumentsLength() < 2 ? UTC_DAY(this) : TO_NUMBER(date);
  var day = MakeDay(UTC_YEAR(this), month, date);
  return SET_UTC_DATE_VALUE(this, MakeDate(day, UTC_TIME_IN_DAY(this)));
}


// ECMA 262 - 15.9.5.40
function DateSetFullYear(year, month, date) {
  CHECK_DATE(this);
  var t = LOCAL_DATE_VALUE(this);
  year = TO_NUMBER(year);
  var argc = %_ArgumentsLength();
  var time ;
  if (NUMBER_IS_NAN(t)) {
    month = argc < 2 ? 0 : TO_NUMBER(month);
    date = argc < 3 ? 1 : TO_NUMBER(date);
    time = 0;
  } else {
    month = argc < 2 ? LOCAL_MONTH(this) : TO_NUMBER(month);
    date = argc < 3 ? LOCAL_DAY(this) : TO_NUMBER(date);
    time = LOCAL_TIME_IN_DAY(this);
  }
  var day = MakeDay(year, month, date);
  return SET_LOCAL_DATE_VALUE(this, MakeDate(day, time));
}


// ECMA 262 - 15.9.5.41
function DateSetUTCFullYear(year, month, date) {
  CHECK_DATE(this);
  var t = UTC_DATE_VALUE(this);
  year = TO_NUMBER(year);
  var argc = %_ArgumentsLength();
  var time ;
  if (NUMBER_IS_NAN(t)) {
    month = argc < 2 ? 0 : TO_NUMBER(month);
    date = argc < 3 ? 1 : TO_NUMBER(date);
    time = 0;
  } else {
    month = argc < 2 ? UTC_MONTH(this) : TO_NUMBER(month);
    date = argc < 3 ? UTC_DAY(this) : TO_NUMBER(date);
    time = UTC_TIME_IN_DAY(this);
  }
  var day = MakeDay(year, month, date);
  return SET_UTC_DATE_VALUE(this, MakeDate(day, time));
}


// ECMA 262 - 15.9.5.42
function DateToUTCString() {
  CHECK_DATE(this);
  var t = UTC_DATE_VALUE(this);
  if (NUMBER_IS_NAN(t)) return kInvalidDate;
  // Return UTC string of the form: Sat, 31 Jan 1970 23:00:00 GMT
  return WeekDays[UTC_WEEKDAY(this)] + ', '
      + TwoDigitString(UTC_DAY(this)) + ' '
      + Months[UTC_MONTH(this)] + ' '
      + UTC_YEAR(this) + ' '
      + TimeStringUTC(this) + ' GMT';
}


// ECMA 262 - B.2.4
function DateGetYear() {
  CHECK_DATE(this);
  return LOCAL_YEAR(this) - 1900;
}


// ECMA 262 - B.2.5
function DateSetYear(year) {
  CHECK_DATE(this);
  year = TO_NUMBER(year);
  if (NUMBER_IS_NAN(year)) return SET_UTC_DATE_VALUE(this, NaN);
  year = (0 <= TO_INTEGER(year) && TO_INTEGER(year) <= 99)
      ? 1900 + TO_INTEGER(year) : year;
  var t = LOCAL_DATE_VALUE(this);
  var month, date, time;
  if (NUMBER_IS_NAN(t))  {
    month = 0;
    date = 1;
    time = 0;
  } else {
    month = LOCAL_MONTH(this);
    date = LOCAL_DAY(this);
    time = LOCAL_TIME_IN_DAY(this);
  }
  var day = MakeDay(year, month, date);
  return SET_LOCAL_DATE_VALUE(this, MakeDate(day, time));
}


// ECMA 262 - B.2.6
//
// Notice that this does not follow ECMA 262 completely.  ECMA 262
// says that toGMTString should be the same Function object as
// toUTCString.  JSC does not do this, so for compatibility we do not
// do that either.  Instead, we create a new function whose name
// property will return toGMTString.
function DateToGMTString() {
  return %_Call(DateToUTCString, this);
}


// 20.3.4.37 Date.prototype.toJSON ( key )
function DateToJSON(key) {
  var o = TO_OBJECT(this);
  var tv = TO_PRIMITIVE_NUMBER(o);
  if (IS_NUMBER(tv) && !NUMBER_IS_FINITE(tv)) {
    return null;
  }
  return o.toISOString();
}


var date_cache_version_holder;
var date_cache_version = NaN;


function CheckDateCacheCurrent() {
  if (!date_cache_version_holder) {
    date_cache_version_holder = %DateCacheVersion();
    if (!date_cache_version_holder) return;
  }
  if (date_cache_version_holder[0] == date_cache_version) {
    return;
  }
  date_cache_version = date_cache_version_holder[0];

  // Reset the timezone cache:
  timezone_cache_time = NaN;
  timezone_cache_timezone = UNDEFINED;
}

// -------------------------------------------------------------------

%FunctionSetPrototype(GlobalDate, new GlobalObject());

// Set up non-enumerable functions of the Date prototype object and
// set their names.
utils.InstallFunctions(GlobalDate.prototype, DONT_ENUM, [
  "toString", DateToString,
  "toDateString", DateToDateString,
  "toTimeString", DateToTimeString,
  "toLocaleString", DateToLocaleString,
  "toLocaleDateString", DateToLocaleDateString,
  "toLocaleTimeString", DateToLocaleTimeString,
  "setTime", DateSetTime,
  "setMilliseconds", DateSetMilliseconds,
  "setUTCMilliseconds", DateSetUTCMilliseconds,
  "setSeconds", DateSetSeconds,
  "setUTCSeconds", DateSetUTCSeconds,
  "setMinutes", DateSetMinutes,
  "setUTCMinutes", DateSetUTCMinutes,
  "setHours", DateSetHours,
  "setUTCHours", DateSetUTCHours,
  "setDate", DateSetDate,
  "setUTCDate", DateSetUTCDate,
  "setMonth", DateSetMonth,
  "setUTCMonth", DateSetUTCMonth,
  "setFullYear", DateSetFullYear,
  "setUTCFullYear", DateSetUTCFullYear,
  "toGMTString", DateToGMTString,
  "toUTCString", DateToUTCString,
  "getYear", DateGetYear,
  "setYear", DateSetYear,
  "toJSON", DateToJSON
]);

})
