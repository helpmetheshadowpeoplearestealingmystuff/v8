// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class MyUint8Array extends Uint8Array {};
class MyFloat32Array extends Float32Array {};
class MyBigInt64Array extends BigInt64Array {};

const builtinCtors = [
  Uint8Array,
  Int8Array,
  Uint16Array,
  Int16Array,
  Uint32Array,
  Int32Array,
  Float32Array,
  Float64Array,
  Uint8ClampedArray,
  BigUint64Array,
  BigInt64Array
];

const ctors = [
  ...builtinCtors,
  MyUint8Array,
  MyFloat32Array,
  MyBigInt64Array,
];

const floatCtors = [
  Float32Array,
  Float64Array,
  MyFloat32Array
];

const intCtors = [
  Uint8Array,
  Int8Array,
  Uint16Array,
  Int16Array,
  Uint32Array,
  Int32Array,
  BigUint64Array,
  BigInt64Array,
  MyUint8Array,
  MyBigInt64Array,
];

// Each element of the following array is [getter, setter, size, isBigInt].
const dataViewAccessorsAndSizes = [[DataView.prototype.getUint8,
                                    DataView.prototype.setUint8, 1, false],
                                   [DataView.prototype.getInt8,
                                    DataView.prototype.setInt8, 1, false],
                                   [DataView.prototype.getUint16,
                                    DataView.prototype.setUint16, 2, false],
                                   [DataView.prototype.getInt16,
                                    DataView.prototype.setInt16, 2, false],
                                   [DataView.prototype.getInt32,
                                    DataView.prototype.setInt32, 4, false],
                                   [DataView.prototype.getFloat32,
                                    DataView.prototype.setFloat32, 4, false],
                                   [DataView.prototype.getFloat64,
                                    DataView.prototype.setFloat64, 8, false],
                                   [DataView.prototype.getBigUint64,
                                    DataView.prototype.setBigUint64, 8, true],
                                   [DataView.prototype.getBigInt64,
                                    DataView.prototype.setBigInt64, 8, true]];

function CreateResizableArrayBuffer(byteLength, maxByteLength) {
  return new ArrayBuffer(byteLength, {maxByteLength: maxByteLength});
}

function CreateGrowableSharedArrayBuffer(byteLength, maxByteLength) {
  return new SharedArrayBuffer(byteLength, {maxByteLength: maxByteLength});
}

function IsBigIntTypedArray(ta) {
  return (ta instanceof BigInt64Array) || (ta instanceof BigUint64Array);
}

function AllBigIntMatchedCtorCombinations(test) {
  for (let targetCtor of ctors) {
    for (let sourceCtor of ctors) {
      if (IsBigIntTypedArray(new targetCtor()) !=
          IsBigIntTypedArray(new sourceCtor())) {
        // Can't mix BigInt and non-BigInt types.
        continue;
      }
      test(targetCtor, sourceCtor);
    }
  }
}

function ReadDataFromBuffer(ab, ctor) {
  let result = [];
  const ta = new ctor(ab, 0, ab.byteLength / ctor.BYTES_PER_ELEMENT);
  for (let item of ta) {
    result.push(Number(item));
  }
  return result;
}

function WriteToTypedArray(array, index, value) {
  if (array instanceof BigInt64Array ||
      array instanceof BigUint64Array) {
    array[index] = BigInt(value);
  } else {
    array[index] = value;
  }
}

function ToNumbers(array) {
  let result = [];
  for (let item of array) {
    if (typeof item == 'bigint') {
      result.push(Number(item));
    } else {
      result.push(item);
    }
  }
  return result;
}

function TypedArrayEntriesHelper(ta) {
  return ta.entries();
}

function ArrayEntriesHelper(ta) {
  return Array.prototype.entries.call(ta);
}

function ValuesFromTypedArrayEntries(ta) {
  let result = [];
  let expectedKey = 0;
  for (let [key, value] of ta.entries()) {
    assertEquals(expectedKey, key);
    ++expectedKey;
    result.push(Number(value));
  }
  return result;
}

function ValuesFromArrayEntries(ta) {
  let result = [];
  let expectedKey = 0;
  for (let [key, value] of Array.prototype.entries.call(ta)) {
    assertEquals(expectedKey, key);
    ++expectedKey;
    result.push(Number(value));
  }
  return result;
}

function TypedArrayKeysHelper(ta) {
  return ta.keys();
}

function ArrayKeysHelper(ta) {
  return Array.prototype.keys.call(ta);
}

function TypedArrayValuesHelper(ta) {
  return ta.values();
}

function ArrayValuesHelper(ta) {
  return Array.prototype.values.call(ta);
}

function ValuesFromTypedArrayValues(ta) {
  let result = [];
  for (let value of ta.values()) {
    result.push(Number(value));
  }
  return result;
}

function ValuesFromArrayValues(ta) {
  let result = [];
  for (let value of Array.prototype.values.call(ta)) {
    result.push(Number(value));
  }
  return result;
}

function AtHelper(array, index) {
  let result = array.at(index);
  if (typeof result == 'bigint') {
    return Number(result);
  }
  return result;
}

function TypedArrayFillHelper(ta, n, start, end) {
  if (ta instanceof BigInt64Array || ta instanceof BigUint64Array) {
    ta.fill(BigInt(n), start, end);
  } else {
    ta.fill(n, start, end);
  }
}

function ArrayFillHelper(ta, n, start, end) {
  if (ta instanceof BigInt64Array || ta instanceof BigUint64Array) {
    Array.prototype.fill.call(ta, BigInt(n), start, end);
  } else {
    Array.prototype.fill.call(ta, n, start, end);
  }
}

function TypedArrayFindHelper(ta, p) {
  return ta.find(p);
}

function ArrayFindHelper(ta, p) {
  return Array.prototype.find.call(ta, p);
}

function TypedArrayFindIndexHelper(ta, p) {
  return ta.findIndex(p);
}

function ArrayFindIndexHelper(ta, p) {
  return Array.prototype.findIndex.call(ta, p);
}

function TypedArrayFindLastHelper(ta, p) {
  return ta.findLast(p);
}

function ArrayFindLastHelper(ta, p) {
  return Array.prototype.findLast.call(ta, p);
}

function TypedArrayFindLastIndexHelper(ta, p) {
  return ta.findLastIndex(p);
}

function ArrayFindLastIndexHelper(ta, p) {
  return Array.prototype.findLastIndex.call(ta, p);
}

function IncludesHelper(array, n, fromIndex) {
  if (typeof n == 'number' &&
      (array instanceof BigInt64Array || array instanceof BigUint64Array)) {
    return array.includes(BigInt(n), fromIndex);
  }
  return array.includes(n, fromIndex);
}

function ArrayIncludesHelper(array, n, fromIndex) {
  if (typeof n == 'number' &&
      (array instanceof BigInt64Array || array instanceof BigUint64Array)) {
    return Array.prototype.includes.call(array, BigInt(n), fromIndex);
  }
  return Array.prototype.includes.call(array, n, fromIndex);
}

function TypedArrayIndexOfHelper(ta, n, fromIndex) {
  if (typeof n == 'number' &&
      (ta instanceof BigInt64Array || ta instanceof BigUint64Array)) {
    if (fromIndex == undefined) {
      // Technically, passing fromIndex here would still result in the correct
      // behavior, since "undefined" gets converted to 0 which is a good
      // "default" index. This is to test the "only 1 argument passed" code
      // path, too.
      return ta.indexOf(BigInt(n));
    }
    return ta.indexOf(BigInt(n), fromIndex);
  }
  if (fromIndex == undefined) {
    return ta.indexOf(n);
  }
  return ta.indexOf(n, fromIndex);
}

function ArrayIndexOfHelper(ta, n, fromIndex) {
  if (typeof n == 'number' &&
      (ta instanceof BigInt64Array || ta instanceof BigUint64Array)) {
    if (fromIndex == undefined) {
      // Technically, passing fromIndex here would still result in the correct
      // behavior, since "undefined" gets converted to 0 which is a good
      // "default" index. This is to test the "only 1 argument passed" code
      // path, too.
      return Array.prototype.indexOf.call(ta, BigInt(n));
    }
    return Array.prototype.indexOf.call(ta, BigInt(n), fromIndex);
  }
  if (fromIndex == undefined) {
    return Array.prototype.indexOf.call(ta, n);
  }
  return Array.prototype.indexOf.call(ta, n, fromIndex);
}

function TypedArrayLastIndexOfHelper(ta, n, fromIndex) {
  if (typeof n == 'number' &&
      (ta instanceof BigInt64Array || ta instanceof BigUint64Array)) {
    if (fromIndex == undefined) {
      // Shouldn't pass fromIndex here, since passing "undefined" is not the
      // same as not passing the parameter at all. "Undefined" will get
      // converted to 0 which is not a good "default" index, since lastIndexOf
      // iterates from the index downwards.
      return ta.lastIndexOf(BigInt(n));
    }
    return ta.lastIndexOf(BigInt(n), fromIndex);
  }
  if (fromIndex == undefined) {
    return ta.lastIndexOf(n);
  }
  return ta.lastIndexOf(n, fromIndex);
}

function ArrayLastIndexOfHelper(ta, n, fromIndex) {
  if (typeof n == 'number' &&
      (ta instanceof BigInt64Array || ta instanceof BigUint64Array)) {
    if (fromIndex == undefined) {
      // Shouldn't pass fromIndex here, since passing "undefined" is not the
      // same as not passing the parameter at all. "Undefined" will get
      // converted to 0 which is not a good "default" index, since lastIndexOf
      // iterates from the index downwards.
      return Array.prototype.lastIndexOf.call(ta, BigInt(n));
    }
    return Array.prototype.lastIndexOf.call(ta, BigInt(n), fromIndex);
  }
  if (fromIndex == undefined) {
    return Array.prototype.lastIndexOf.call(ta, n);
  }
  return Array.prototype.lastIndexOf.call(ta, n, fromIndex);
}

function SetHelper(target, source, offset) {
  if (target instanceof BigInt64Array || target instanceof BigUint64Array) {
    const bigIntSource = [];
    for (s of source) {
      bigIntSource.push(BigInt(s));
    }
    source = bigIntSource;
  }
  if (offset == undefined) {
    return target.set(source);
  }
  return target.set(source, offset);
}

function testDataViewMethodsUpToSize(view, bufferSize) {
  for (const [getter, setter, size, isBigInt] of dataViewAccessorsAndSizes) {
    for (let i = 0; i <= bufferSize - size; ++i) {
      if (isBigInt) {
        setter.call(view, i, 3n);
      } else {
        setter.call(view, i, 3);
      }
      assertEquals(3, Number(getter.call(view, i)));
    }
    if (isBigInt) {
      assertThrows(() => setter.call(view, bufferSize - size + 1, 0n),
                   RangeError);
    } else {
      assertThrows(() => setter.call(view, bufferSize - size + 1, 0),
                   RangeError);
    }
    assertThrows(() => getter.call(view, bufferSize - size + 1), RangeError);
  }
}

function assertAllDataViewMethodsThrow(view, index, errorType) {
  for (const [getter, setter, size, isBigInt] of dataViewAccessorsAndSizes) {
    if (isBigInt) {
      assertThrows(() => { setter.call(view, index, 3n); }, errorType);
    } else {
      assertThrows(() => { setter.call(view, index, 3); }, errorType);
    }
    assertThrows(() => { getter.call(view, index); }, errorType);
  }
}

function ObjectDefinePropertyHelper(ta, index, value) {
  if (ta instanceof BigInt64Array || ta instanceof BigUint64Array) {
    Object.defineProperty(ta, index, {value: BigInt(value)});
  } else {
    Object.defineProperty(ta, index, {value: value});
  }
}

function ObjectDefinePropertiesHelper(ta, index, value) {
  const values = {};
  if (ta instanceof BigInt64Array || ta instanceof BigUint64Array) {
    values[index] = {value: BigInt(value)};
  } else {
    values[index] = {value: value};
  }
  Object.defineProperties(ta, values);
}

function TestAtomicsOperations(ta, index) {
  const one = IsBigIntTypedArray(ta) ? 1n : 1;
  const two = IsBigIntTypedArray(ta) ? 2n : 2;
  const three = IsBigIntTypedArray(ta) ? 3n : 3;

  Atomics.store(ta, index, one);
  assertEquals(one, Atomics.load(ta, index));
  assertEquals(one, Atomics.exchange(ta, index, two));
  assertEquals(two, Atomics.load(ta, index));
  assertEquals(two, Atomics.compareExchange(ta, index, two, three));
  assertEquals(three, Atomics.load(ta, index));

  assertEquals(three, Atomics.sub(ta, index, two));  // 3 - 2 = 1
  assertEquals(one, Atomics.load(ta, index));

  assertEquals(one, Atomics.add(ta, index, one));  // 1 + 1 = 2
  assertEquals(two, Atomics.load(ta, index));

  assertEquals(two, Atomics.or(ta, index, one));  // 2 | 1 = 3
  assertEquals(three, Atomics.load(ta, index));

  assertEquals(three, Atomics.xor(ta, index, one));  // 3 ^ 1 = 2
  assertEquals(two, Atomics.load(ta, index));

  assertEquals(two, Atomics.and(ta, index, three));  // 2 & 3 = 2
  assertEquals(two, Atomics.load(ta, index));
}

function AssertAtomicsOperationsThrow(ta, index, error) {
  const one = IsBigIntTypedArray(ta) ? 1n : 1;
  assertThrows(() => { Atomics.store(ta, index, one); }, error);
  assertThrows(() => { Atomics.load(ta, index); }, error);
  assertThrows(() => { Atomics.exchange(ta, index, one); }, error);
  assertThrows(() => { Atomics.compareExchange(ta, index, one, one); },
               error);
  assertThrows(() => { Atomics.add(ta, index, one); }, error);
  assertThrows(() => { Atomics.sub(ta, index, one); }, error);
  assertThrows(() => { Atomics.and(ta, index, one); }, error);
  assertThrows(() => { Atomics.or(ta, index, one); }, error);
  assertThrows(() => { Atomics.xor(ta, index, one); }, error);
}

const CopyWithinHelper = (ta, ...rest) => { ta.copyWithin(...rest); };
const ArrayCopyWithinHelper = (ta, ...rest) => {
    Array.prototype.copyWithin.call(ta, ...rest); };

const TypedArrayReverseHelper = (ta) => { ta.reverse(); }
const ArrayReverseHelper = (ta) => { Array.prototype.reverse.call(ta); };

const TypedArraySortHelper = (ta, ...rest) => { ta.sort(...rest); }
const ArraySortHelper = (ta, ...rest) => {
    Array.prototype.sort.call(ta, ...rest); };
