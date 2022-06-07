// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-array-grouping

assertEquals(Array.prototype[Symbol.unscopables].group, true);

var array = [-0, 1, 0, 2];
var group = () => {
  let result = array.group(v => v > 0);
  result = Array.from(Object.entries(result));
  return result;
}

// entry order matters
assertEquals(group(), [
  ['false', [-0, 0]],
  ['true', [1, 2]],
]);

Object.defineProperty(array, 4, {
  enumerable: true,
  configurable: true,
  writable: true,
  value: 3,
});
assertEquals(group(), [
  ['false', [-0, 0]],
  ['true', [1, 2, 3]],
]);

Object.defineProperty(array, 5, {
  enumerable: true,
  configurable: true,
  get: () => 4,
});
var result = group();
assertEquals(result, [
  ['false', [-0, 0]],
  ['true', [1, 2, 3, 4]],
]);
assertSame(result[0][1][0], -0);


// fairly large result array
var length = 20000;
var array = new Array(length);
for (var idx = 0; idx < length; idx++) {
  array[idx] = idx;
}
var group = () => {
  let result = array.group(v => v % 2);
  result = Array.from(Object.entries(result));
  return result;
}
var result = group();
assertEquals(result, [
  ['0', array.filter(v => v % 2 === 0)],
  ['1', array.filter(v => v % 2 === 1)],
]);

// check array changed by callbackfn
var array = [-0, 0, 1, 2];
group = () => {
  let result = array.group((v, idx) => {
    if (idx === 1) {
      array[2] = {a: 'b'};
    }
    return v > 0;
  });
  result = Array.from(Object.entries(result));
  return result;
}

assertEquals(group(), [
  ['false', [-0, 0, {a: 'b'}]],
  ['true', [2]],
]);


// check array with holes
var array = [1, , 2, , 3, , 4];
var group = () => {
  let result = array.group(v => v % 2 === 0 ? 'even' : 'not_even');
  result = Array.from(Object.entries(result));
  return result;
};
function checkNoHoles(arr) {
  for (let idx = 0; idx < arr.length; idx++) {
    assertTrue(Object.getOwnPropertyDescriptor(arr, idx) !== undefined);
  }
}
var result = group();
assertEquals(result, [
  ['not_even', [1, undefined, undefined, 3, undefined]],
  ['even', [2, 4]],
]);
checkNoHoles(result[0][1]);
checkNoHoles(result[1][1]);

var array = [1, undefined, 2, undefined, 3, undefined, 4];
result = group();
assertEquals(result, [
  ['not_even', [1, undefined, undefined, 3, undefined]],
  ['even', [2, 4]],
]);
checkNoHoles(result[0][1]);
checkNoHoles(result[1][1]);

// array like objects
var arrayLikeObjects = [
  {
    '0': -1,
    '1': 1,
    '2': 2,
    length: 3,
  },
  (function () { return arguments })(-1, 1, 2),
  Int8Array.from([-1, 1, 2]),
  Float32Array.from([-1, 1, 2]),
];
var group = () => {
  let result = Array.prototype.group.call(array, v => v > 0);
  result = Array.from(Object.entries(result));
  return result;
};
for (var array of arrayLikeObjects) {
  assertEquals(group(), [
    ['false', [-1]],
    ['true', [1, 2]],
  ]);
}


// check proto elements
var array = [,];
var group = () => {
  let result = array.group(v => v);
  result = Array.from(Object.entries(result));
  return result;
}

assertEquals(group(), [
  ['undefined', [,]],
]);

array.__proto__.push(6);
assertEquals(group(), [
  ['6', [6]],
]);


// callbackfn throws
var array = [-0, 1, 0, 2];
assertThrows(
  () => array.group(() => { throw new Error('foobar'); }),
  Error,
  'foobar'
);


// ToPropertyKey throws
var array = [-0, 1, 0, 2];
assertThrows(
  () => array.group(() => {
    return {
      toString() {
        throw new Error('foobar');
      },
    };
  }),
  Error,
  'foobar'
);


// callbackfn is not callable
var array = [-0, 1, 0, 2];
assertThrows(
  () => array.group('foobar'),
  TypeError,
);
