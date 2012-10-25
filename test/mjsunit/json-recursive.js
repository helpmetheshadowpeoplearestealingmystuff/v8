// Copyright 2012 the V8 project authors. All rights reserved.
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


var a = {};
for (i = 0; i < 10000; i++) {
  var current = {};
  current.a = a;
  a = current;
}

function rec(a,b,c,d,e,f,g,h,i,j,k,l,m,n) {
  JSON.stringify(a);
  rec(a,b,c,d,e,f,g,h,i,j,k,l,m,n);
}

assertThrows(function() { rec(1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4) },
             RangeError);


var depth1 = 1500;
var depth2 = 10000;
var deepArray = [];
for (var i = 0; i < depth1; i++) deepArray = [deepArray];
JSON.stringify(deepArray);
for (var i = depth1; i < depth2; i++) deepArray = [deepArray];
assertThrows(function() { JSON.stringify(deepArray); }, RangeError);


var deepObject = {};
for (var i = 0; i < depth1; i++) deepObject = { next: deepObject };
JSON.stringify(deepObject);
for (var i = depth1; i < depth2; i++) deepObject = { next: deepObject };
assertThrows(function() { JSON.stringify(deepObject); }, RangeError);


var str = "[1]";
for (var i = 0; i < 100000; i++) {
  str = "[1," + str + "]";
}

assertThrows(function() { JSON.parse(str); }, RangeError);
