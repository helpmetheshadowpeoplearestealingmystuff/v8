// Copyright 2006-2007 Google Inc. All Rights Reserved.
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

// This file contains support for URI manipulations written in
// JavaScript.

// Expect $String = global.String;

function URIAddEncodedOctetToBuffer(octet, result, index) {
  result[index++] = 37; // Char code of '%'.
  result[index++] = hexCharCodeArray[octet >> 4];
  result[index++] = hexCharCodeArray[octet & 0x0F];
  return index;
};


function URIEncodeOctets(octets, result, index) {
  index = URIAddEncodedOctetToBuffer(octets[0], result, index);
  if (octets[1]) index = URIAddEncodedOctetToBuffer(octets[1], result, index);
  if (octets[2]) index = URIAddEncodedOctetToBuffer(octets[2], result, index);
  if (octets[3]) index = URIAddEncodedOctetToBuffer(octets[3], result, index);
  return index;
};


function URIEncodeSingle(cc, result, index) {
  var x = (cc >> 12) & 0xF;
  var y = (cc >> 6) & 63;
  var z = cc & 63;
  var octets = new $Array(3);
  if (cc <= 0x007F) {
    octets[0] = cc;
  } else if (cc <= 0x07FF) {
    octets[0] = y + 192;
    octets[1] = z + 128;
  } else {
    octets[0] = x + 224;
    octets[1] = y + 128;
    octets[2] = z + 128;
  }
  return URIEncodeOctets(octets, result, index);
};


function URIEncodePair(cc1 , cc2, result, index) {
  var u = ((cc1 >> 6) & 0xF) + 1;
  var w = (cc1 >> 2) & 0xF;
  var x = cc1 & 3;
  var y = (cc2 >> 6) & 0xF;
  var z = cc2 & 63;
  var octets = new $Array(4);
  octets[0] = (u >> 2) + 240;
  octets[1] = (((u & 3) << 4) | w) + 128;
  octets[2] = ((x << 4) | y) + 128;
  octets[3] = z + 128;
  return URIEncodeOctets(octets, result, index);
};


function URIHexCharsToCharCode(ch1, ch2) {
  if (HexValueOf(ch1) == -1 || HexValueOf(ch2) == -1) {
    throw new $URIError("URI malformed");
  }
  return HexStrToCharCode(ch1 + ch2);
};


function URIDecodeOctets(octets, result, index) {
  if (octets[3]) {
    var x = (octets[2] >> 4) & 3;
    var y = octets[2] & 0xF;
    var z = octets[3] & 63;
    var v = (((octets[0] & 7) << 2) | ((octets[1] >> 4) & 3)) - 1;
    var w = octets[1] & 0xF;
    result[index++] = 55296 | (v << 6) | (w << 2) | x;
    result[index++] = 56320 | (y << 6) | z;
    return index;
  }
  if (octets[2]) {
    var x = octets[0] & 0xF;
    var y = octets[1] & 63;
    var z = octets[2] & 63;
    result[index++] = (x << 12) | (y << 6) | z;
    return index;
  }
  var z = octets[1] & 63;
  var y = octets[0] & 31;
  result[index++] = (y << 6) | z;
  return index;
};


// ECMA-262, section 15.1.3
function Encode(uri, unescape) {
  var uriLength = uri.length;
  var result = new $Array(uriLength);
  var index = 0;
  for (var k = 0; k < uriLength; k++) {
    var cc1 = uri.charCodeAt(k);
    if (unescape(cc1)) {
      result[index++] = cc1;
    } else {
      if (cc1 >= 0xDC00 && cc1 <= 0xDFFF) throw new $URIError("URI malformed");
      if (cc1 < 0xD800 || cc1 > 0xDBFF) {
        index = URIEncodeSingle(cc1, result, index);
      } else {
        k++;
        if (k == uriLength) throw new $URIError("URI malformed");
        var cc2 = uri.charCodeAt(k);
        if (cc2 < 0xDC00 || cc2 > 0xDFFF) throw new $URIError("URI malformed");
        index = URIEncodePair(cc1, cc2, result, index);
      }
    }
  }
  return %StringFromCharCodeArray(result);
};


// ECMA-262, section 15.1.3
function Decode(uri, reserved) {
  var uriLength = uri.length;
  var result = new $Array(uriLength);
  var index = 0;
  for (var k = 0; k < uriLength; k++) {
    var ch = uri.charAt(k);
    if (ch == '%') {
      if (k + 2 >= uriLength) throw new $URIError("URI malformed");
      var cc = URIHexCharsToCharCode(uri.charAt(++k), uri.charAt(++k));
      if (cc >> 7) {
        var n = 0;
        while (((cc << ++n) & 0x80) != 0) ;
        if (n == 1 || n > 4) throw new $URIError("URI malformed");
        var octets = new $Array(n);
        octets[0] = cc;
        if (k + 3 * (n - 1) >= uriLength) throw new $URIError("URI malformed");
        for (var i = 1; i < n; i++) {
          k++;
          octets[i] = URIHexCharsToCharCode(uri.charAt(++k), uri.charAt(++k));
        }
        index = URIDecodeOctets(octets, result, index);
      } else {
        if (reserved(cc)) {
          result[index++] = 37; // Char code of '%'.
          result[index++] = uri.charCodeAt(k - 1);
          result[index++] = uri.charCodeAt(k);
        } else {
          result[index++] = cc;
        }
      }
    } else {
      result[index++] = ch.charCodeAt(0);
    }
  }
  result.length = index;
  return %StringFromCharCodeArray(result);
};


// ECMA-262 - 15.1.3.1.
function URIDecode(uri) {
  function reservedPredicate(cc) {
    // #$
    if (35 <= cc && cc <= 36) return true;
    // &
    if (cc == 38) return true;
    // +,
    if (43 <= cc && cc <= 44) return true;
    // /
    if (cc == 47) return true;
    // :;
    if (58 <= cc && cc <= 59) return true;
    // =
    if (cc == 61) return true;
    // ?@
    if (63 <= cc && cc <= 64) return true;
    
    return false;
  };
  var string = ToString(uri);
  return Decode(string, reservedPredicate);
};


// ECMA-262 - 15.1.3.2.
function URIDecodeComponent(component) {
  function reservedPredicate(cc) { return false; };
  var string = ToString(component);
  return Decode(string, reservedPredicate);
};


// Does the char code correspond to an alpha-numeric char.
function isAlphaNumeric(cc) {
  // a - z
  if (97 <= cc && cc <= 122) return true;
  // A - Z
  if (65 <= cc && cc <= 90) return true;
  // 0 - 9
  if (48 <= cc && cc <= 57) return true;
  
  return false;
};


// ECMA-262 - 15.1.3.3.
function URIEncode(uri) {
  function unescapePredicate(cc) {
    if (isAlphaNumeric(cc)) return true;
    // !
    if (cc == 33) return true;
    // #$
    if (35 <= cc && cc <= 36) return true;
    // &'()*+,-./
    if (38 <= cc && cc <= 47) return true;
    // :;
    if (58 <= cc && cc <= 59) return true;
    // =
    if (cc == 61) return true;
    // ?@
    if (63 <= cc && cc <= 64) return true;
    // _
    if (cc == 95) return true;
    // ~
    if (cc == 126) return true;
    
    return false;
  };

  var string = ToString(uri);
  return Encode(string, unescapePredicate);
};


// ECMA-262 - 15.1.3.4
function URIEncodeComponent(component) {
  function unescapePredicate(cc) {
    if (isAlphaNumeric(cc)) return true;
    // !
    if (cc == 33) return true;
    // '()*
    if (39 <= cc && cc <= 42) return true;
    // -.
    if (45 <= cc && cc <= 46) return true;
    // _
    if (cc == 95) return true;
    // ~
    if (cc == 126) return true;
    
    return false;
  };

  var string = ToString(component);
  return Encode(string, unescapePredicate);
};


const hexCharArray = ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
                      "A", "B", "C", "D", "E", "F"];

const hexCharCodeArray = [48, 49, 50, 51, 52, 53, 54, 55, 56, 57,
                          65, 66, 67, 68, 69, 70];


function HexValueOf(c) {
  var code = c.charCodeAt(0);
  
  // 0-9
  if (code >= 48 && code <= 57) return code - 48;
  // A-F
  if (code >= 65 && code <= 70) return code - 55;
  // a-f
  if (code >= 97 && code <= 102) return code - 87;
  
  return -1;
};


// Convert a character code to 4-digit hex string representation
// 64 -> 0040, 62234 -> F31A.
function CharCodeToHex4Str(cc) {
  var r = "";
  for (var i = 0; i < 4; ++i) {
    var c = hexCharArray[cc & 0x0F];
    r = c + r;
    cc = cc >>> 4;
  }
  return r;
};


// Converts hex string to char code. Not efficient.
function HexStrToCharCode(s) {
  var m = 0;
  var r = 0;
  for (var i = s.length - 1; i >= 0; --i) {
    r = r + (HexValueOf(s.charAt(i)) << m);
    m = m + 4;
  }
  return r;
};


// Returns true if all digits in string s are valid hex numbers
function IsValidHex(s) {
  for (var i = 0; i < s.length; ++i) {
    var cc = s.charCodeAt(i);
    if ((48 <= cc && cc <= 57) || (65 <= cc && cc <= 70) || (97 <= cc && cc <= 102)) {
      // '0'..'9', 'A'..'F' and 'a' .. 'f'.
    } else {
      return false;
    }
  }
  return true;
};


// ECMA-262 - B.2.1.
function URIEscape(str) {
  var s = ToString(str);
  return %URIEscape(s);
};


// ECMA-262 - B.2.2.
function URIUnescape(str) {
  var s = ToString(str);
  return %URIUnescape(s);
}


// -------------------------------------------------------------------

function SetupURI() {
  // Setup non-enumerable URI properties of the global object.
  InstallProperties(global, DONT_ENUM, {
    escape: URIEscape,
    unescape: URIUnescape,
    decodeURI: URIDecode,
    decodeURIComponent: URIDecodeComponent,
    encodeURI: URIEncode,
    encodeURIComponent: URIEncodeComponent
  });
};

SetupURI();

