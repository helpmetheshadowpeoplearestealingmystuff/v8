// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/bigint/bigint-internal.h"

namespace v8 {
namespace bigint {

ProcessorImpl::ProcessorImpl(Platform* platform) : platform_(platform) {}

ProcessorImpl::~ProcessorImpl() { delete platform_; }

Status ProcessorImpl::get_and_clear_status() {
  Status result = status_;
  status_ = Status::kOk;
  return result;
}

Processor* Processor::New(Platform* platform) {
  ProcessorImpl* impl = new ProcessorImpl(platform);
  return static_cast<Processor*>(impl);
}

void Processor::Destroy() { delete static_cast<ProcessorImpl*>(this); }

void ProcessorImpl::Multiply(RWDigits Z, Digits X, Digits Y) {
  X.Normalize();
  Y.Normalize();
  if (X.len() == 0 || Y.len() == 0) return Z.Clear();
  if (X.len() < Y.len()) std::swap(X, Y);
  if (Y.len() == 1) return MultiplySingle(Z, X, Y[0]);
  if (Y.len() < kKaratsubaThreshold) return MultiplySchoolbook(Z, X, Y);
  return MultiplyKaratsuba(Z, X, Y);
}

void ProcessorImpl::Divide(RWDigits Q, Digits A, Digits B) {
  A.Normalize();
  B.Normalize();
  DCHECK(B.len() > 0);  // NOLINT(readability/check)
  int cmp = Compare(A, B);
  if (cmp < 0) return Q.Clear();
  if (cmp == 0) {
    Q[0] = 1;
    for (int i = 1; i < Q.len(); i++) Q[i] = 0;
    return;
  }
  if (B.len() == 1) {
    digit_t remainder;
    return DivideSingle(Q, &remainder, A, B[0]);
  }
  return DivideSchoolbook(Q, RWDigits(nullptr, 0), A, B);
}

void ProcessorImpl::Modulo(RWDigits R, Digits A, Digits B) {
  A.Normalize();
  B.Normalize();
  DCHECK(B.len() > 0);  // NOLINT(readability/check)
  int cmp = Compare(A, B);
  if (cmp < 0) {
    for (int i = 0; i < B.len(); i++) R[i] = B[i];
    for (int i = B.len(); i < R.len(); i++) R[i] = 0;
    return;
  }
  if (cmp == 0) return R.Clear();
  if (B.len() == 1) {
    digit_t remainder;
    DivideSingle(RWDigits(nullptr, 0), &remainder, A, B[0]);
    R[0] = remainder;
    for (int i = 1; i < R.len(); i++) R[i] = 0;
    return;
  }
  return DivideSchoolbook(RWDigits(nullptr, 0), R, A, B);
}

Status Processor::Multiply(RWDigits Z, Digits X, Digits Y) {
  ProcessorImpl* impl = static_cast<ProcessorImpl*>(this);
  impl->Multiply(Z, X, Y);
  return impl->get_and_clear_status();
}

Status Processor::Divide(RWDigits Q, Digits A, Digits B) {
  ProcessorImpl* impl = static_cast<ProcessorImpl*>(this);
  impl->Divide(Q, A, B);
  return impl->get_and_clear_status();
}

Status Processor::Modulo(RWDigits R, Digits A, Digits B) {
  ProcessorImpl* impl = static_cast<ProcessorImpl*>(this);
  impl->Modulo(R, A, B);
  return impl->get_and_clear_status();
}

}  // namespace bigint
}  // namespace v8
