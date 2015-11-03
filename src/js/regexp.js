// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

var FLAG_harmony_regexps;
var FLAG_harmony_tolength;
var FLAG_harmony_unicode_regexps;
var GlobalObject = global.Object;
var GlobalRegExp = global.RegExp;
var InternalPackedArray = utils.InternalPackedArray;
var MakeTypeError;
var regExpFlagsSymbol = utils.ImportNow("regexp_flags_symbol");
var regExpSourceSymbol = utils.ImportNow("regexp_source_symbol");

utils.ImportFromExperimental(function(from) {
  FLAG_harmony_regexps = from.FLAG_harmony_regexps;
  FLAG_harmony_tolength = from.FLAG_harmony_tolength;
  FLAG_harmony_unicode_regexps = from.FLAG_harmony_unicode_regexps;
});

utils.Import(function(from) {
  MakeTypeError = from.MakeTypeError;
});

// -------------------------------------------------------------------

// Property of the builtins object for recording the result of the last
// regexp match.  The property RegExpLastMatchInfo includes the matchIndices
// array of the last successful regexp match (an array of start/end index
// pairs for the match and all the captured substrings), the invariant is
// that there are at least two capture indeces.  The array also contains
// the subject string for the last successful match.
var RegExpLastMatchInfo = new InternalPackedArray(
 2,                 // REGEXP_NUMBER_OF_CAPTURES
 "",                // Last subject.
 UNDEFINED,         // Last input - settable with RegExpSetInput.
 0,                 // REGEXP_FIRST_CAPTURE + 0
 0                  // REGEXP_FIRST_CAPTURE + 1
);

// -------------------------------------------------------------------

// A recursive descent parser for Patterns according to the grammar of
// ECMA-262 15.10.1, with deviations noted below.
function DoConstructRegExp(object, pattern, flags) {
  // RegExp : Called as constructor; see ECMA-262, section 15.10.4.
  if (IS_REGEXP(pattern)) {
    if (!IS_UNDEFINED(flags)) throw MakeTypeError(kRegExpFlags);
    flags = (REGEXP_GLOBAL(pattern) ? 'g' : '')
        + (REGEXP_IGNORE_CASE(pattern) ? 'i' : '')
        + (REGEXP_MULTILINE(pattern) ? 'm' : '');
    if (FLAG_harmony_unicode_regexps)
        flags += (REGEXP_UNICODE(pattern) ? 'u' : '');
    if (FLAG_harmony_regexps)
        flags += (REGEXP_STICKY(pattern) ? 'y' : '');
    pattern = REGEXP_SOURCE(pattern);
  }

  pattern = IS_UNDEFINED(pattern) ? '' : TO_STRING(pattern);
  flags = IS_UNDEFINED(flags) ? '' : TO_STRING(flags);

  %RegExpInitializeAndCompile(object, pattern, flags);
}


function RegExpConstructor(pattern, flags) {
  if (%_IsConstructCall()) {
    DoConstructRegExp(this, pattern, flags);
  } else {
    // RegExp : Called as function; see ECMA-262, section 15.10.3.1.
    if (IS_REGEXP(pattern) && IS_UNDEFINED(flags)) {
      return pattern;
    }
    return new GlobalRegExp(pattern, flags);
  }
}

// Deprecated RegExp.prototype.compile method.  We behave like the constructor
// were called again.  In SpiderMonkey, this method returns the regexp object.
// In JSC, it returns undefined.  For compatibility with JSC, we match their
// behavior.
function RegExpCompileJS(pattern, flags) {
  // Both JSC and SpiderMonkey treat a missing pattern argument as the
  // empty subject string, and an actual undefined value passed as the
  // pattern as the string 'undefined'.  Note that JSC is inconsistent
  // here, treating undefined values differently in
  // RegExp.prototype.compile and in the constructor, where they are
  // the empty string.  For compatibility with JSC, we match their
  // behavior.
  if (this == GlobalRegExp.prototype) {
    // We don't allow recompiling RegExp.prototype.
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'RegExp.prototype.compile', this);
  }
  if (IS_UNDEFINED(pattern) && %_ArgumentsLength() != 0) {
    DoConstructRegExp(this, 'undefined', flags);
  } else {
    DoConstructRegExp(this, pattern, flags);
  }
}


function DoRegExpExec(regexp, string, index) {
  var result = %_RegExpExec(regexp, string, index, RegExpLastMatchInfo);
  return result;
}


// This is kind of performance sensitive, so we want to avoid unnecessary
// type checks on inputs. But we also don't want to inline it several times
// manually, so we use a macro :-)
macro RETURN_NEW_RESULT_FROM_MATCH_INFO(MATCHINFO, STRING)
  var numResults = NUMBER_OF_CAPTURES(MATCHINFO) >> 1;
  var start = MATCHINFO[CAPTURE0];
  var end = MATCHINFO[CAPTURE1];
  // Calculate the substring of the first match before creating the result array
  // to avoid an unnecessary write barrier storing the first result.
  var first = %_SubString(STRING, start, end);
  var result = %_RegExpConstructResult(numResults, start, STRING);
  result[0] = first;
  if (numResults == 1) return result;
  var j = REGEXP_FIRST_CAPTURE + 2;
  for (var i = 1; i < numResults; i++) {
    start = MATCHINFO[j++];
    if (start != -1) {
      end = MATCHINFO[j];
      result[i] = %_SubString(STRING, start, end);
    }
    j++;
  }
  return result;
endmacro


function RegExpExecNoTests(regexp, string, start) {
  // Must be called with RegExp, string and positive integer as arguments.
  var matchInfo = %_RegExpExec(regexp, string, start, RegExpLastMatchInfo);
  if (matchInfo !== null) {
    // ES6 21.2.5.2.2 step 18.
    if (FLAG_harmony_regexps && regexp.sticky) {
      regexp.lastIndex = matchInfo[CAPTURE1];
    }
    RETURN_NEW_RESULT_FROM_MATCH_INFO(matchInfo, string);
  }
  regexp.lastIndex = 0;
  return null;
}


function RegExpExecJS(string) {
  if (!IS_REGEXP(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'RegExp.prototype.exec', this);
  }

  string = TO_STRING(string);
  var lastIndex = this.lastIndex;

  // Conversion is required by the ES2015 specification (RegExpBuiltinExec
  // algorithm, step 4) even if the value is discarded for non-global RegExps.
  var i = TO_LENGTH_OR_INTEGER(lastIndex);

  var updateLastIndex = REGEXP_GLOBAL(this) ||
                        (FLAG_harmony_regexps && REGEXP_STICKY(this));
  if (updateLastIndex) {
    if (i < 0 || i > string.length) {
      this.lastIndex = 0;
      return null;
    }
  } else {
    i = 0;
  }

  // matchIndices is either null or the RegExpLastMatchInfo array.
  var matchIndices = %_RegExpExec(this, string, i, RegExpLastMatchInfo);

  if (IS_NULL(matchIndices)) {
    this.lastIndex = 0;
    return null;
  }

  // Successful match.
  if (updateLastIndex) {
    this.lastIndex = RegExpLastMatchInfo[CAPTURE1];
  }
  RETURN_NEW_RESULT_FROM_MATCH_INFO(matchIndices, string);
}


// One-element cache for the simplified test regexp.
var regexp_key;
var regexp_val;

// Section 15.10.6.3 doesn't actually make sense, but the intention seems to be
// that test is defined in terms of String.prototype.exec. However, it probably
// means the original value of String.prototype.exec, which is what everybody
// else implements.
function RegExpTest(string) {
  if (!IS_REGEXP(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'RegExp.prototype.test', this);
  }
  string = TO_STRING(string);

  var lastIndex = this.lastIndex;

  // Conversion is required by the ES2015 specification (RegExpBuiltinExec
  // algorithm, step 4) even if the value is discarded for non-global RegExps.
  var i = TO_LENGTH_OR_INTEGER(lastIndex);

  if (REGEXP_GLOBAL(this) || (FLAG_harmony_regexps && REGEXP_STICKY(this))) {
    if (i < 0 || i > string.length) {
      this.lastIndex = 0;
      return false;
    }
    // matchIndices is either null or the RegExpLastMatchInfo array.
    var matchIndices = %_RegExpExec(this, string, i, RegExpLastMatchInfo);
    if (IS_NULL(matchIndices)) {
      this.lastIndex = 0;
      return false;
    }
    this.lastIndex = RegExpLastMatchInfo[CAPTURE1];
    return true;
  } else {
    // Non-global, non-sticky regexp.
    // Remove irrelevant preceeding '.*' in a test regexp.  The expression
    // checks whether this.source starts with '.*' and that the third char is
    // not a '?'.  But see https://code.google.com/p/v8/issues/detail?id=3560
    var regexp = this;
    var source = REGEXP_SOURCE(regexp);
    if (regexp.length >= 3 &&
        %_StringCharCodeAt(regexp, 0) == 46 &&  // '.'
        %_StringCharCodeAt(regexp, 1) == 42 &&  // '*'
        %_StringCharCodeAt(regexp, 2) != 63) {  // '?'
      regexp = TrimRegExp(regexp);
    }
    // matchIndices is either null or the RegExpLastMatchInfo array.
    var matchIndices = %_RegExpExec(regexp, string, 0, RegExpLastMatchInfo);
    if (IS_NULL(matchIndices)) {
      this.lastIndex = 0;
      return false;
    }
    return true;
  }
}

function TrimRegExp(regexp) {
  if (!%_ObjectEquals(regexp_key, regexp)) {
    regexp_key = regexp;
    regexp_val =
      new GlobalRegExp(
          %_SubString(REGEXP_SOURCE(regexp), 2, REGEXP_SOURCE(regexp).length),
          (REGEXP_IGNORE_CASE(regexp) ? REGEXP_MULTILINE(regexp) ? "im" : "i"
                                      : REGEXP_MULTILINE(regexp) ? "m" : ""));
  }
  return regexp_val;
}


function RegExpToString() {
  if (!IS_REGEXP(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'RegExp.prototype.toString', this);
  }
  var result = '/' + REGEXP_SOURCE(this) + '/';
  if (REGEXP_GLOBAL(this)) result += 'g';
  if (REGEXP_IGNORE_CASE(this)) result += 'i';
  if (REGEXP_MULTILINE(this)) result += 'm';
  if (FLAG_harmony_unicode_regexps && REGEXP_UNICODE(this)) result += 'u';
  if (FLAG_harmony_regexps && REGEXP_STICKY(this)) result += 'y';
  return result;
}


// Getters for the static properties lastMatch, lastParen, leftContext, and
// rightContext of the RegExp constructor.  The properties are computed based
// on the captures array of the last successful match and the subject string
// of the last successful match.
function RegExpGetLastMatch() {
  var regExpSubject = LAST_SUBJECT(RegExpLastMatchInfo);
  return %_SubString(regExpSubject,
                     RegExpLastMatchInfo[CAPTURE0],
                     RegExpLastMatchInfo[CAPTURE1]);
}


function RegExpGetLastParen() {
  var length = NUMBER_OF_CAPTURES(RegExpLastMatchInfo);
  if (length <= 2) return '';  // There were no captures.
  // We match the SpiderMonkey behavior: return the substring defined by the
  // last pair (after the first pair) of elements of the capture array even if
  // it is empty.
  var regExpSubject = LAST_SUBJECT(RegExpLastMatchInfo);
  var start = RegExpLastMatchInfo[CAPTURE(length - 2)];
  var end = RegExpLastMatchInfo[CAPTURE(length - 1)];
  if (start != -1 && end != -1) {
    return %_SubString(regExpSubject, start, end);
  }
  return "";
}


function RegExpGetLeftContext() {
  var start_index;
  var subject;
  start_index = RegExpLastMatchInfo[CAPTURE0];
  subject = LAST_SUBJECT(RegExpLastMatchInfo);
  return %_SubString(subject, 0, start_index);
}


function RegExpGetRightContext() {
  var start_index;
  var subject;
  start_index = RegExpLastMatchInfo[CAPTURE1];
  subject = LAST_SUBJECT(RegExpLastMatchInfo);
  return %_SubString(subject, start_index, subject.length);
}


// The properties $1..$9 are the first nine capturing substrings of the last
// successful match, or ''.  The function RegExpMakeCaptureGetter will be
// called with indices from 1 to 9.
function RegExpMakeCaptureGetter(n) {
  return function foo() {
    var index = n * 2;
    if (index >= NUMBER_OF_CAPTURES(RegExpLastMatchInfo)) return '';
    var matchStart = RegExpLastMatchInfo[CAPTURE(index)];
    var matchEnd = RegExpLastMatchInfo[CAPTURE(index + 1)];
    if (matchStart == -1 || matchEnd == -1) return '';
    return %_SubString(LAST_SUBJECT(RegExpLastMatchInfo), matchStart, matchEnd);
  };
}


// ES6 21.2.5.4, 21.2.5.5, 21.2.5.7, 21.2.5.12, 21.2.5.15.
function GetRegExpFlagGetter(name, mask) {
  var getter = function() {
    if (!IS_SPEC_OBJECT(this)) {
      throw MakeTypeError(kRegExpNonObject, name, TO_STRING(this));
    }
    var flags = this[regExpFlagsSymbol];
    if (IS_UNDEFINED(flags)) {
      throw MakeTypeError(kRegExpNonRegExp, TO_STRING(this));
    }
    return !!(flags & mask);
  };
  %FunctionSetName(getter, name);
  %SetNativeFlag(getter);
  return getter;
}


// ES6 21.2.5.10.
function RegExpGetSource() {
  if (!IS_SPEC_OBJECT(this)) {
    throw MakeTypeError(kRegExpNonObject, "RegExp.prototype.source",
                        TO_STRING(this));
  }
  var source = this[regExpSourceSymbol];
  if (IS_UNDEFINED(source)) {
    throw MakeTypeError(kRegExpNonRegExp, TO_STRING(this));
  }
  return source;
}

%SetNativeFlag(RegExpGetSource);

// -------------------------------------------------------------------

%FunctionSetInstanceClassName(GlobalRegExp, 'RegExp');
%FunctionSetPrototype(GlobalRegExp, new GlobalObject());
%AddNamedProperty(
    GlobalRegExp.prototype, 'constructor', GlobalRegExp, DONT_ENUM);
%SetCode(GlobalRegExp, RegExpConstructor);

utils.InstallFunctions(GlobalRegExp.prototype, DONT_ENUM, [
  "exec", RegExpExecJS,
  "test", RegExpTest,
  "toString", RegExpToString,
  "compile", RegExpCompileJS
]);

%DefineGetterPropertyUnchecked(GlobalRegExp.prototype, "global",
    GetRegExpFlagGetter("RegExp.prototype.global", REGEXP_GLOBAL_MASK),
    DONT_ENUM);
%DefineGetterPropertyUnchecked(GlobalRegExp.prototype, "ignoreCase",
    GetRegExpFlagGetter("RegExp.prototype.ignoreCase", REGEXP_IGNORE_CASE_MASK),
    DONT_ENUM);
%DefineGetterPropertyUnchecked(GlobalRegExp.prototype, "multiline",
    GetRegExpFlagGetter("RegExp.prototype.multiline", REGEXP_MULTILINE_MASK),
    DONT_ENUM);
%DefineGetterPropertyUnchecked(GlobalRegExp.prototype, "source",
    RegExpGetSource, DONT_ENUM);

// The length of compile is 1 in SpiderMonkey.
%FunctionSetLength(GlobalRegExp.prototype.compile, 1);

// The properties `input` and `$_` are aliases for each other.  When this
// value is set the value it is set to is coerced to a string.
// Getter and setter for the input.
var RegExpGetInput = function() {
  var regExpInput = LAST_INPUT(RegExpLastMatchInfo);
  return IS_UNDEFINED(regExpInput) ? "" : regExpInput;
};
var RegExpSetInput = function(string) {
  LAST_INPUT(RegExpLastMatchInfo) = TO_STRING(string);
};

%OptimizeObjectForAddingMultipleProperties(GlobalRegExp, 22);
%DefineAccessorPropertyUnchecked(GlobalRegExp, 'input', RegExpGetInput,
                                 RegExpSetInput, DONT_DELETE);
%DefineAccessorPropertyUnchecked(GlobalRegExp, '$_', RegExpGetInput,
                                 RegExpSetInput, DONT_ENUM | DONT_DELETE);

// The properties multiline and $* are aliases for each other.  When this
// value is set in SpiderMonkey, the value it is set to is coerced to a
// boolean.  We mimic that behavior with a slight difference: in SpiderMonkey
// the value of the expression 'RegExp.multiline = null' (for instance) is the
// boolean false (i.e., the value after coercion), while in V8 it is the value
// null (i.e., the value before coercion).

// Getter and setter for multiline.
var multiline = false;
var RegExpGetMultiline = function() { return multiline; };
var RegExpSetMultiline = function(flag) { multiline = flag ? true : false; };

%DefineAccessorPropertyUnchecked(GlobalRegExp, 'multiline', RegExpGetMultiline,
                                 RegExpSetMultiline, DONT_DELETE);
%DefineAccessorPropertyUnchecked(GlobalRegExp, '$*', RegExpGetMultiline,
                                 RegExpSetMultiline,
                                 DONT_ENUM | DONT_DELETE);


var NoOpSetter = function(ignored) {};


// Static properties set by a successful match.
%DefineAccessorPropertyUnchecked(GlobalRegExp, 'lastMatch', RegExpGetLastMatch,
                                 NoOpSetter, DONT_DELETE);
%DefineAccessorPropertyUnchecked(GlobalRegExp, '$&', RegExpGetLastMatch,
                                 NoOpSetter, DONT_ENUM | DONT_DELETE);
%DefineAccessorPropertyUnchecked(GlobalRegExp, 'lastParen', RegExpGetLastParen,
                                 NoOpSetter, DONT_DELETE);
%DefineAccessorPropertyUnchecked(GlobalRegExp, '$+', RegExpGetLastParen,
                                 NoOpSetter, DONT_ENUM | DONT_DELETE);
%DefineAccessorPropertyUnchecked(GlobalRegExp, 'leftContext',
                                 RegExpGetLeftContext, NoOpSetter,
                                 DONT_DELETE);
%DefineAccessorPropertyUnchecked(GlobalRegExp, '$`', RegExpGetLeftContext,
                                 NoOpSetter, DONT_ENUM | DONT_DELETE);
%DefineAccessorPropertyUnchecked(GlobalRegExp, 'rightContext',
                                 RegExpGetRightContext, NoOpSetter,
                                 DONT_DELETE);
%DefineAccessorPropertyUnchecked(GlobalRegExp, "$'", RegExpGetRightContext,
                                 NoOpSetter, DONT_ENUM | DONT_DELETE);

for (var i = 1; i < 10; ++i) {
  %DefineAccessorPropertyUnchecked(GlobalRegExp, '$' + i,
                                   RegExpMakeCaptureGetter(i), NoOpSetter,
                                   DONT_DELETE);
}
%ToFastProperties(GlobalRegExp);

// -------------------------------------------------------------------
// Exports

utils.Export(function(to) {
  to.GetRegExpFlagGetter = GetRegExpFlagGetter;
  to.RegExpExec = DoRegExpExec;
  to.RegExpExecNoTests = RegExpExecNoTests;
  to.RegExpLastMatchInfo = RegExpLastMatchInfo;
  to.RegExpTest = RegExpTest;
});

})
