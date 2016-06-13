// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_IEEE754_H_
#define V8_BASE_IEEE754_H_

namespace v8 {
namespace base {
namespace ieee754 {

// Returns the natural logarithm of |x|.
double log(double x);

// Returns a value equivalent to |log(1+x)|, but computed in a way that is
// accurate even if the value of |x| is near zero.
double log1p(double x);

}  // namespace ieee754
}  // namespace base
}  // namespace v8

#endif  // V8_BASE_IEEE754_H_
