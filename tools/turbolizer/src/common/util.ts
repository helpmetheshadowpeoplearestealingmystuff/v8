// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export function anyToString(x: any): string {
  return `${x}`;
}

export function snakeToCamel(str: string): string {
  return str.toLowerCase().replace(/([-_][a-z])/g, group =>
    group
      .toUpperCase()
      .replace('-', '')
      .replace('_', ''));
}

export function camelize(obj: any): any {
  if (Array.isArray(obj)) {
    return obj.map(value => camelize(value));
  } else if (obj !== null && obj.constructor === Object) {
    return Object.keys(obj).reduce((result, key) => ({
        ...result,
        [snakeToCamel(key)]: camelize(obj[key])
      }), {},
    );
  }
  return obj;
}

export function sortUnique<T>(arr: Array<T>, f: (a: T, b: T) => number, equal: (a: T, b: T) => boolean): Array<T> {
  if (arr.length == 0) return arr;
  arr = arr.sort(f);
  const uniqueArr = [arr[0]];
  for (let i = 1; i < arr.length; i++) {
    if (!equal(arr[i - 1], arr[i])) {
      uniqueArr.push(arr[i]);
    }
  }
  return uniqueArr;
}

// Partial application without binding the receiver
export function partial(func: any, ...arguments1: Array<any>) {
  return function (this: any, ...arguments2: Array<any>) {
    func.apply(this, [...arguments1, ...arguments2]);
  };
}

export function isIterable(obj: any): obj is Iterable<any> {
  return obj !== null && obj !== undefined
    && typeof obj !== "string" && typeof obj[Symbol.iterator] === "function";
}

export function alignUp(raw: number, multiple: number): number {
  return Math.floor((raw + multiple - 1) / multiple) * multiple;
}

export function measureText(text: string): { width: number, height: number } {
  const textMeasure = document.getElementById("text-measure");
  if (textMeasure instanceof SVGTSpanElement) {
    textMeasure.textContent = text;
    return {
      width: textMeasure.getBBox().width,
      height: textMeasure.getBBox().height,
    };
  }
  return { width: 0, height: 0 };
}

// Interpolate between the given start and end values by a fraction of val/max.
export function interpolate(val: number, max: number, start: number, end: number): number {
  return start + (end - start) * (val / max);
}

export function createElement(tag: string, cls: string, content?: string): HTMLElement {
  const el = document.createElement(tag);
  el.className = cls;
  if (content !== undefined) el.innerText = content;
  return el;
}
