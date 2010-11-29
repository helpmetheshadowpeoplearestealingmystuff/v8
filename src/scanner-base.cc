// Copyright 2010 the V8 project authors. All rights reserved.
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

// Features shared by parsing and pre-parsing scanners.

#include "../include/v8stdint.h"
#include "scanner-base.h"
#include "char-predicates-inl.h"

namespace v8 {
namespace internal {

// ----------------------------------------------------------------------------
// UTF16Buffer

UTF16Buffer::UTF16Buffer()
    : pos_(0), end_(kNoEndPosition) { }

// ----------------------------------------------------------------------------
// LiteralCollector

LiteralCollector::LiteralCollector()
    : buffer_(kInitialCapacity), recording_(false) { }


LiteralCollector::~LiteralCollector() {}


void LiteralCollector::AddCharSlow(uc32 c) {
  ASSERT(static_cast<unsigned>(c) > unibrow::Utf8::kMaxOneByteChar);
  int length = unibrow::Utf8::Length(c);
  Vector<char> block = buffer_.AddBlock(length, '\0');
#ifdef DEBUG
  int written_length = unibrow::Utf8::Encode(block.start(), c);
  CHECK_EQ(length, written_length);
#else
  unibrow::Utf8::Encode(block.start(), c);
#endif
}

// ----------------------------------------------------------------------------
// Character predicates

unibrow::Predicate<IdentifierStart, 128> ScannerConstants::kIsIdentifierStart;
unibrow::Predicate<IdentifierPart, 128> ScannerConstants::kIsIdentifierPart;
unibrow::Predicate<unibrow::WhiteSpace, 128> ScannerConstants::kIsWhiteSpace;
unibrow::Predicate<unibrow::LineTerminator, 128>
  ScannerConstants::kIsLineTerminator;

StaticResource<ScannerConstants::Utf8Decoder> ScannerConstants::utf8_decoder_;

// Compound predicates.

bool ScannerConstants::IsIdentifier(unibrow::CharacterStream* buffer) {
  // Checks whether the buffer contains an identifier (no escape).
  if (!buffer->has_more()) return false;
  if (!kIsIdentifierStart.get(buffer->GetNext())) {
    return false;
  }
  while (buffer->has_more()) {
    if (!kIsIdentifierPart.get(buffer->GetNext())) {
      return false;
    }
  }
  return true;
}

// ----------------------------------------------------------------------------
// Scanner

Scanner::Scanner() : source_(NULL) {}


uc32 Scanner::ScanHexEscape(uc32 c, int length) {
  ASSERT(length <= 4);  // prevent overflow

  uc32 digits[4];
  uc32 x = 0;
  for (int i = 0; i < length; i++) {
    digits[i] = c0_;
    int d = HexValue(c0_);
    if (d < 0) {
      // According to ECMA-262, 3rd, 7.8.4, page 18, these hex escapes
      // should be illegal, but other JS VMs just return the
      // non-escaped version of the original character.

      // Push back digits read, except the last one (in c0_).
      for (int j = i-1; j >= 0; j--) {
        PushBack(digits[j]);
      }
      // Notice: No handling of error - treat it as "\u"->"u".
      return c;
    }
    x = x * 16 + d;
    Advance();
  }

  return x;
}


// Octal escapes of the forms '\0xx' and '\xxx' are not a part of
// ECMA-262. Other JS VMs support them.
uc32 Scanner::ScanOctalEscape(uc32 c, int length) {
  uc32 x = c - '0';
  for (int i = 0; i < length; i++) {
    int d = c0_ - '0';
    if (d < 0 || d > 7) break;
    int nx = x * 8 + d;
    if (nx >= 256) break;
    x = nx;
    Advance();
  }
  return x;
}


// ----------------------------------------------------------------------------
// JavaScriptScanner

JavaScriptScanner::JavaScriptScanner()
    : has_line_terminator_before_next_(false) {}


Token::Value JavaScriptScanner::Next() {
  current_ = next_;
  has_line_terminator_before_next_ = false;
  Scan();
  return current_.token;
}


static inline bool IsByteOrderMark(uc32 c) {
  // The Unicode value U+FFFE is guaranteed never to be assigned as a
  // Unicode character; this implies that in a Unicode context the
  // 0xFF, 0xFE byte pattern can only be interpreted as the U+FEFF
  // character expressed in little-endian byte order (since it could
  // not be a U+FFFE character expressed in big-endian byte
  // order). Nevertheless, we check for it to be compatible with
  // Spidermonkey.
  return c == 0xFEFF || c == 0xFFFE;
}


bool JavaScriptScanner::SkipWhiteSpace() {
  int start_position = source_pos();

  while (true) {
    // We treat byte-order marks (BOMs) as whitespace for better
    // compatibility with Spidermonkey and other JavaScript engines.
    while (ScannerConstants::kIsWhiteSpace.get(c0_) || IsByteOrderMark(c0_)) {
      // IsWhiteSpace() includes line terminators!
      if (ScannerConstants::kIsLineTerminator.get(c0_)) {
        // Ignore line terminators, but remember them. This is necessary
        // for automatic semicolon insertion.
        has_line_terminator_before_next_ = true;
      }
      Advance();
    }

    // If there is an HTML comment end '-->' at the beginning of a
    // line (with only whitespace in front of it), we treat the rest
    // of the line as a comment. This is in line with the way
    // SpiderMonkey handles it.
    if (c0_ == '-' && has_line_terminator_before_next_) {
      Advance();
      if (c0_ == '-') {
        Advance();
        if (c0_ == '>') {
          // Treat the rest of the line as a comment.
          SkipSingleLineComment();
          // Continue skipping white space after the comment.
          continue;
        }
        PushBack('-');  // undo Advance()
      }
      PushBack('-');  // undo Advance()
    }
    // Return whether or not we skipped any characters.
    return source_pos() != start_position;
  }
}


Token::Value JavaScriptScanner::SkipSingleLineComment() {
  Advance();

  // The line terminator at the end of the line is not considered
  // to be part of the single-line comment; it is recognized
  // separately by the lexical grammar and becomes part of the
  // stream of input elements for the syntactic grammar (see
  // ECMA-262, section 7.4, page 12).
  while (c0_ >= 0 && !ScannerConstants::kIsLineTerminator.get(c0_)) {
    Advance();
  }

  return Token::WHITESPACE;
}


Token::Value JavaScriptScanner::SkipMultiLineComment() {
  ASSERT(c0_ == '*');
  Advance();

  while (c0_ >= 0) {
    char ch = c0_;
    Advance();
    // If we have reached the end of the multi-line comment, we
    // consume the '/' and insert a whitespace. This way all
    // multi-line comments are treated as whitespace - even the ones
    // containing line terminators. This contradicts ECMA-262, section
    // 7.4, page 12, that says that multi-line comments containing
    // line terminators should be treated as a line terminator, but it
    // matches the behaviour of SpiderMonkey and KJS.
    if (ch == '*' && c0_ == '/') {
      c0_ = ' ';
      return Token::WHITESPACE;
    }
  }

  // Unterminated multi-line comment.
  return Token::ILLEGAL;
}


Token::Value JavaScriptScanner::ScanHtmlComment() {
  // Check for <!-- comments.
  ASSERT(c0_ == '!');
  Advance();
  if (c0_ == '-') {
    Advance();
    if (c0_ == '-') return SkipSingleLineComment();
    PushBack('-');  // undo Advance()
  }
  PushBack('!');  // undo Advance()
  ASSERT(c0_ == '!');
  return Token::LT;
}


void JavaScriptScanner::Scan() {
  next_.literal_chars = Vector<const char>();
  Token::Value token;
  do {
    // Remember the position of the next token
    next_.location.beg_pos = source_pos();

    switch (c0_) {
      case ' ':
      case '\t':
        Advance();
        token = Token::WHITESPACE;
        break;

      case '\n':
        Advance();
        has_line_terminator_before_next_ = true;
        token = Token::WHITESPACE;
        break;

      case '"': case '\'':
        token = ScanString();
        break;

      case '<':
        // < <= << <<= <!--
        Advance();
        if (c0_ == '=') {
          token = Select(Token::LTE);
        } else if (c0_ == '<') {
          token = Select('=', Token::ASSIGN_SHL, Token::SHL);
        } else if (c0_ == '!') {
          token = ScanHtmlComment();
        } else {
          token = Token::LT;
        }
        break;

      case '>':
        // > >= >> >>= >>> >>>=
        Advance();
        if (c0_ == '=') {
          token = Select(Token::GTE);
        } else if (c0_ == '>') {
          // >> >>= >>> >>>=
          Advance();
          if (c0_ == '=') {
            token = Select(Token::ASSIGN_SAR);
          } else if (c0_ == '>') {
            token = Select('=', Token::ASSIGN_SHR, Token::SHR);
          } else {
            token = Token::SAR;
          }
        } else {
          token = Token::GT;
        }
        break;

      case '=':
        // = == ===
        Advance();
        if (c0_ == '=') {
          token = Select('=', Token::EQ_STRICT, Token::EQ);
        } else {
          token = Token::ASSIGN;
        }
        break;

      case '!':
        // ! != !==
        Advance();
        if (c0_ == '=') {
          token = Select('=', Token::NE_STRICT, Token::NE);
        } else {
          token = Token::NOT;
        }
        break;

      case '+':
        // + ++ +=
        Advance();
        if (c0_ == '+') {
          token = Select(Token::INC);
        } else if (c0_ == '=') {
          token = Select(Token::ASSIGN_ADD);
        } else {
          token = Token::ADD;
        }
        break;

      case '-':
        // - -- --> -=
        Advance();
        if (c0_ == '-') {
          Advance();
          if (c0_ == '>' && has_line_terminator_before_next_) {
            // For compatibility with SpiderMonkey, we skip lines that
            // start with an HTML comment end '-->'.
            token = SkipSingleLineComment();
          } else {
            token = Token::DEC;
          }
        } else if (c0_ == '=') {
          token = Select(Token::ASSIGN_SUB);
        } else {
          token = Token::SUB;
        }
        break;

      case '*':
        // * *=
        token = Select('=', Token::ASSIGN_MUL, Token::MUL);
        break;

      case '%':
        // % %=
        token = Select('=', Token::ASSIGN_MOD, Token::MOD);
        break;

      case '/':
        // /  // /* /=
        Advance();
        if (c0_ == '/') {
          token = SkipSingleLineComment();
        } else if (c0_ == '*') {
          token = SkipMultiLineComment();
        } else if (c0_ == '=') {
          token = Select(Token::ASSIGN_DIV);
        } else {
          token = Token::DIV;
        }
        break;

      case '&':
        // & && &=
        Advance();
        if (c0_ == '&') {
          token = Select(Token::AND);
        } else if (c0_ == '=') {
          token = Select(Token::ASSIGN_BIT_AND);
        } else {
          token = Token::BIT_AND;
        }
        break;

      case '|':
        // | || |=
        Advance();
        if (c0_ == '|') {
          token = Select(Token::OR);
        } else if (c0_ == '=') {
          token = Select(Token::ASSIGN_BIT_OR);
        } else {
          token = Token::BIT_OR;
        }
        break;

      case '^':
        // ^ ^=
        token = Select('=', Token::ASSIGN_BIT_XOR, Token::BIT_XOR);
        break;

      case '.':
        // . Number
        Advance();
        if (IsDecimalDigit(c0_)) {
          token = ScanNumber(true);
        } else {
          token = Token::PERIOD;
        }
        break;

      case ':':
        token = Select(Token::COLON);
        break;

      case ';':
        token = Select(Token::SEMICOLON);
        break;

      case ',':
        token = Select(Token::COMMA);
        break;

      case '(':
        token = Select(Token::LPAREN);
        break;

      case ')':
        token = Select(Token::RPAREN);
        break;

      case '[':
        token = Select(Token::LBRACK);
        break;

      case ']':
        token = Select(Token::RBRACK);
        break;

      case '{':
        token = Select(Token::LBRACE);
        break;

      case '}':
        token = Select(Token::RBRACE);
        break;

      case '?':
        token = Select(Token::CONDITIONAL);
        break;

      case '~':
        token = Select(Token::BIT_NOT);
        break;

      default:
        if (ScannerConstants::kIsIdentifierStart.get(c0_)) {
          token = ScanIdentifierOrKeyword();
        } else if (IsDecimalDigit(c0_)) {
          token = ScanNumber(false);
        } else if (SkipWhiteSpace()) {
          token = Token::WHITESPACE;
        } else if (c0_ < 0) {
          token = Token::EOS;
        } else {
          token = Select(Token::ILLEGAL);
        }
        break;
    }

    // Continue scanning for tokens as long as we're just skipping
    // whitespace.
  } while (token == Token::WHITESPACE);

  next_.location.end_pos = source_pos();
  next_.token = token;
}


void JavaScriptScanner::SeekForward(int pos) {
  source_->SeekForward(pos - 1);
  Advance();
  // This function is only called to seek to the location
  // of the end of a function (at the "}" token). It doesn't matter
  // whether there was a line terminator in the part we skip.
  has_line_terminator_before_next_ = false;
  Scan();
}


void JavaScriptScanner::ScanEscape() {
  uc32 c = c0_;
  Advance();

  // Skip escaped newlines.
  if (ScannerConstants::kIsLineTerminator.get(c)) {
    // Allow CR+LF newlines in multiline string literals.
    if (IsCarriageReturn(c) && IsLineFeed(c0_)) Advance();
    // Allow LF+CR newlines in multiline string literals.
    if (IsLineFeed(c) && IsCarriageReturn(c0_)) Advance();
    return;
  }

  switch (c) {
    case '\'':  // fall through
    case '"' :  // fall through
    case '\\': break;
    case 'b' : c = '\b'; break;
    case 'f' : c = '\f'; break;
    case 'n' : c = '\n'; break;
    case 'r' : c = '\r'; break;
    case 't' : c = '\t'; break;
    case 'u' : c = ScanHexEscape(c, 4); break;
    case 'v' : c = '\v'; break;
    case 'x' : c = ScanHexEscape(c, 2); break;
    case '0' :  // fall through
    case '1' :  // fall through
    case '2' :  // fall through
    case '3' :  // fall through
    case '4' :  // fall through
    case '5' :  // fall through
    case '6' :  // fall through
    case '7' : c = ScanOctalEscape(c, 2); break;
  }

  // According to ECMA-262, 3rd, 7.8.4 (p 18ff) these
  // should be illegal, but they are commonly handled
  // as non-escaped characters by JS VMs.
  AddLiteralChar(c);
}


Token::Value JavaScriptScanner::ScanString() {
  uc32 quote = c0_;
  Advance();  // consume quote

  LiteralScope literal(this, kLiteralString);
  while (c0_ != quote && c0_ >= 0
         && !ScannerConstants::kIsLineTerminator.get(c0_)) {
    uc32 c = c0_;
    Advance();
    if (c == '\\') {
      if (c0_ < 0) return Token::ILLEGAL;
      ScanEscape();
    } else {
      AddLiteralChar(c);
    }
  }
  if (c0_ != quote) return Token::ILLEGAL;
  literal.Complete();

  Advance();  // consume quote
  return Token::STRING;
}


void JavaScriptScanner::ScanDecimalDigits() {
  while (IsDecimalDigit(c0_))
    AddLiteralCharAdvance();
}


Token::Value JavaScriptScanner::ScanNumber(bool seen_period) {
  ASSERT(IsDecimalDigit(c0_));  // the first digit of the number or the fraction

  enum { DECIMAL, HEX, OCTAL } kind = DECIMAL;

  LiteralScope literal(this, kLiteralNumber);
  if (seen_period) {
    // we have already seen a decimal point of the float
    AddLiteralChar('.');
    ScanDecimalDigits();  // we know we have at least one digit

  } else {
    // if the first character is '0' we must check for octals and hex
    if (c0_ == '0') {
      AddLiteralCharAdvance();

      // either 0, 0exxx, 0Exxx, 0.xxx, an octal number, or a hex number
      if (c0_ == 'x' || c0_ == 'X') {
        // hex number
        kind = HEX;
        AddLiteralCharAdvance();
        if (!IsHexDigit(c0_)) {
          // we must have at least one hex digit after 'x'/'X'
          return Token::ILLEGAL;
        }
        while (IsHexDigit(c0_)) {
          AddLiteralCharAdvance();
        }
      } else if ('0' <= c0_ && c0_ <= '7') {
        // (possible) octal number
        kind = OCTAL;
        while (true) {
          if (c0_ == '8' || c0_ == '9') {
            kind = DECIMAL;
            break;
          }
          if (c0_  < '0' || '7'  < c0_) break;
          AddLiteralCharAdvance();
        }
      }
    }

    // Parse decimal digits and allow trailing fractional part.
    if (kind == DECIMAL) {
      ScanDecimalDigits();  // optional
      if (c0_ == '.') {
        AddLiteralCharAdvance();
        ScanDecimalDigits();  // optional
      }
    }
  }

  // scan exponent, if any
  if (c0_ == 'e' || c0_ == 'E') {
    ASSERT(kind != HEX);  // 'e'/'E' must be scanned as part of the hex number
    if (kind == OCTAL) return Token::ILLEGAL;  // no exponent for octals allowed
    // scan exponent
    AddLiteralCharAdvance();
    if (c0_ == '+' || c0_ == '-')
      AddLiteralCharAdvance();
    if (!IsDecimalDigit(c0_)) {
      // we must have at least one decimal digit after 'e'/'E'
      return Token::ILLEGAL;
    }
    ScanDecimalDigits();
  }

  // The source character immediately following a numeric literal must
  // not be an identifier start or a decimal digit; see ECMA-262
  // section 7.8.3, page 17 (note that we read only one decimal digit
  // if the value is 0).
  if (IsDecimalDigit(c0_) || ScannerConstants::kIsIdentifierStart.get(c0_))
    return Token::ILLEGAL;

  literal.Complete();

  return Token::NUMBER;
}


uc32 JavaScriptScanner::ScanIdentifierUnicodeEscape() {
  Advance();
  if (c0_ != 'u') return unibrow::Utf8::kBadChar;
  Advance();
  uc32 c = ScanHexEscape('u', 4);
  // We do not allow a unicode escape sequence to start another
  // unicode escape sequence.
  if (c == '\\') return unibrow::Utf8::kBadChar;
  return c;
}


Token::Value JavaScriptScanner::ScanIdentifierOrKeyword() {
  ASSERT(ScannerConstants::kIsIdentifierStart.get(c0_));
  LiteralScope literal(this, kLiteralIdentifier);
  KeywordMatcher keyword_match;
  // Scan identifier start character.
  if (c0_ == '\\') {
    uc32 c = ScanIdentifierUnicodeEscape();
    // Only allow legal identifier start characters.
    if (!ScannerConstants::kIsIdentifierStart.get(c)) return Token::ILLEGAL;
    AddLiteralChar(c);
    return ScanIdentifierSuffix(&literal);
  }

  uc32 first_char = c0_;
  Advance();
  AddLiteralChar(first_char);
  if (!keyword_match.AddChar(first_char)) {
    return ScanIdentifierSuffix(&literal);
  }

  // Scan the rest of the identifier characters.
  while (ScannerConstants::kIsIdentifierPart.get(c0_)) {
    if (c0_ != '\\') {
      uc32 next_char = c0_;
      Advance();
      AddLiteralChar(next_char);
      if (keyword_match.AddChar(next_char)) continue;
    }
    // Fallthrough if no loner able to complete keyword.
    return ScanIdentifierSuffix(&literal);
  }
  literal.Complete();

  return keyword_match.token();
}


Token::Value JavaScriptScanner::ScanIdentifierSuffix(LiteralScope* literal) {
  // Scan the rest of the identifier characters.
  while (ScannerConstants::kIsIdentifierPart.get(c0_)) {
    if (c0_ == '\\') {
      uc32 c = ScanIdentifierUnicodeEscape();
      // Only allow legal identifier part characters.
      if (!ScannerConstants::kIsIdentifierPart.get(c)) return Token::ILLEGAL;
      AddLiteralChar(c);
    } else {
      AddLiteralChar(c0_);
      Advance();
    }
  }
  literal->Complete();

  return Token::IDENTIFIER;
}


bool JavaScriptScanner::ScanRegExpPattern(bool seen_equal) {
  // Scan: ('/' | '/=') RegularExpressionBody '/' RegularExpressionFlags
  bool in_character_class = false;

  // Previous token is either '/' or '/=', in the second case, the
  // pattern starts at =.
  next_.location.beg_pos = source_pos() - (seen_equal ? 2 : 1);
  next_.location.end_pos = source_pos() - (seen_equal ? 1 : 0);

  // Scan regular expression body: According to ECMA-262, 3rd, 7.8.5,
  // the scanner should pass uninterpreted bodies to the RegExp
  // constructor.
  LiteralScope literal(this, kLiteralRegExp);
  if (seen_equal)
    AddLiteralChar('=');

  while (c0_ != '/' || in_character_class) {
    if (ScannerConstants::kIsLineTerminator.get(c0_) || c0_ < 0) return false;
    if (c0_ == '\\') {  // escaped character
      AddLiteralCharAdvance();
      if (ScannerConstants::kIsLineTerminator.get(c0_) || c0_ < 0) return false;
      AddLiteralCharAdvance();
    } else {  // unescaped character
      if (c0_ == '[') in_character_class = true;
      if (c0_ == ']') in_character_class = false;
      AddLiteralCharAdvance();
    }
  }
  Advance();  // consume '/'

  literal.Complete();

  return true;
}


bool JavaScriptScanner::ScanRegExpFlags() {
  // Scan regular expression flags.
  LiteralScope literal(this, kLiteralRegExpFlags);
  while (ScannerConstants::kIsIdentifierPart.get(c0_)) {
    if (c0_ == '\\') {
      uc32 c = ScanIdentifierUnicodeEscape();
      if (c != static_cast<uc32>(unibrow::Utf8::kBadChar)) {
        // We allow any escaped character, unlike the restriction on
        // IdentifierPart when it is used to build an IdentifierName.
        AddLiteralChar(c);
        continue;
      }
    }
    AddLiteralCharAdvance();
  }
  literal.Complete();

  next_.location.end_pos = source_pos() - 1;
  return true;
}

// ----------------------------------------------------------------------------
// Keyword Matcher

KeywordMatcher::FirstState KeywordMatcher::first_states_[] = {
  { "break",  KEYWORD_PREFIX, Token::BREAK },
  { NULL,     C,              Token::ILLEGAL },
  { NULL,     D,              Token::ILLEGAL },
  { "else",   KEYWORD_PREFIX, Token::ELSE },
  { NULL,     F,              Token::ILLEGAL },
  { NULL,     UNMATCHABLE,    Token::ILLEGAL },
  { NULL,     UNMATCHABLE,    Token::ILLEGAL },
  { NULL,     I,              Token::ILLEGAL },
  { NULL,     UNMATCHABLE,    Token::ILLEGAL },
  { NULL,     UNMATCHABLE,    Token::ILLEGAL },
  { NULL,     UNMATCHABLE,    Token::ILLEGAL },
  { NULL,     UNMATCHABLE,    Token::ILLEGAL },
  { NULL,     N,              Token::ILLEGAL },
  { NULL,     UNMATCHABLE,    Token::ILLEGAL },
  { NULL,     UNMATCHABLE,    Token::ILLEGAL },
  { NULL,     UNMATCHABLE,    Token::ILLEGAL },
  { "return", KEYWORD_PREFIX, Token::RETURN },
  { "switch", KEYWORD_PREFIX, Token::SWITCH },
  { NULL,     T,              Token::ILLEGAL },
  { NULL,     UNMATCHABLE,    Token::ILLEGAL },
  { NULL,     V,              Token::ILLEGAL },
  { NULL,     W,              Token::ILLEGAL }
};


void KeywordMatcher::Step(unibrow::uchar input) {
  switch (state_) {
    case INITIAL: {
      // matching the first character is the only state with significant fanout.
      // Match only lower-case letters in range 'b'..'w'.
      unsigned int offset = input - kFirstCharRangeMin;
      if (offset < kFirstCharRangeLength) {
        state_ = first_states_[offset].state;
        if (state_ == KEYWORD_PREFIX) {
          keyword_ = first_states_[offset].keyword;
          counter_ = 1;
          keyword_token_ = first_states_[offset].token;
        }
        return;
      }
      break;
    }
    case KEYWORD_PREFIX:
      if (static_cast<unibrow::uchar>(keyword_[counter_]) == input) {
        counter_++;
        if (keyword_[counter_] == '\0') {
          state_ = KEYWORD_MATCHED;
          token_ = keyword_token_;
        }
        return;
      }
      break;
    case KEYWORD_MATCHED:
      token_ = Token::IDENTIFIER;
      break;
    case C:
      if (MatchState(input, 'a', CA)) return;
      if (MatchState(input, 'o', CO)) return;
      break;
    case CA:
      if (MatchKeywordStart(input, "case", 2, Token::CASE)) return;
      if (MatchKeywordStart(input, "catch", 2, Token::CATCH)) return;
      break;
    case CO:
      if (MatchState(input, 'n', CON)) return;
      break;
    case CON:
      if (MatchKeywordStart(input, "const", 3, Token::CONST)) return;
      if (MatchKeywordStart(input, "continue", 3, Token::CONTINUE)) return;
      break;
    case D:
      if (MatchState(input, 'e', DE)) return;
      if (MatchKeyword(input, 'o', KEYWORD_MATCHED, Token::DO)) return;
      break;
    case DE:
      if (MatchKeywordStart(input, "debugger", 2, Token::DEBUGGER)) return;
      if (MatchKeywordStart(input, "default", 2, Token::DEFAULT)) return;
      if (MatchKeywordStart(input, "delete", 2, Token::DELETE)) return;
      break;
    case F:
      if (MatchKeywordStart(input, "false", 1, Token::FALSE_LITERAL)) return;
      if (MatchKeywordStart(input, "finally", 1, Token::FINALLY)) return;
      if (MatchKeywordStart(input, "for", 1, Token::FOR)) return;
      if (MatchKeywordStart(input, "function", 1, Token::FUNCTION)) return;
      break;
    case I:
      if (MatchKeyword(input, 'f', KEYWORD_MATCHED, Token::IF)) return;
      if (MatchKeyword(input, 'n', IN, Token::IN)) return;
      break;
    case IN:
      token_ = Token::IDENTIFIER;
      if (MatchKeywordStart(input, "instanceof", 2, Token::INSTANCEOF)) return;
      break;
    case N:
      if (MatchKeywordStart(input, "native", 1, Token::NATIVE)) return;
      if (MatchKeywordStart(input, "new", 1, Token::NEW)) return;
      if (MatchKeywordStart(input, "null", 1, Token::NULL_LITERAL)) return;
      break;
    case T:
      if (MatchState(input, 'h', TH)) return;
      if (MatchState(input, 'r', TR)) return;
      if (MatchKeywordStart(input, "typeof", 1, Token::TYPEOF)) return;
      break;
    case TH:
      if (MatchKeywordStart(input, "this", 2, Token::THIS)) return;
      if (MatchKeywordStart(input, "throw", 2, Token::THROW)) return;
      break;
    case TR:
      if (MatchKeywordStart(input, "true", 2, Token::TRUE_LITERAL)) return;
      if (MatchKeyword(input, 'y', KEYWORD_MATCHED, Token::TRY)) return;
      break;
    case V:
      if (MatchKeywordStart(input, "var", 1, Token::VAR)) return;
      if (MatchKeywordStart(input, "void", 1, Token::VOID)) return;
      break;
    case W:
      if (MatchKeywordStart(input, "while", 1, Token::WHILE)) return;
      if (MatchKeywordStart(input, "with", 1, Token::WITH)) return;
      break;
    case UNMATCHABLE:
      break;
  }
  // On fallthrough, it's a failure.
  state_ = UNMATCHABLE;
}

} }  // namespace v8::internal
