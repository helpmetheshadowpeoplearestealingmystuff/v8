// Copyright 2007-2008 the V8 project authors. All rights reserved.
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

#include "v8.h"

#include "counters.h"
#include "platform.h"

namespace v8 { namespace internal {

CounterLookupCallback StatsTable::lookup_function_ = NULL;

StatsCounterTimer::StatsCounterTimer(const wchar_t* name)
  : start_time_(0),  // initialize to avoid compiler complaints
    stop_time_(0) {  // initialize to avoid compiler complaints
  int len = wcslen(name);
  // we prepend the name with 'c.' to indicate that it is a counter.
  name_ = Vector<wchar_t>::New(len+3);
  OS::WcsCpy(name_, L"t:");
  OS::WcsCpy(name_ + 2, name);
}

// Start the timer.
void StatsCounterTimer::Start() {
  if (!Enabled())
    return;
  stop_time_ = 0;
  start_time_ = OS::Ticks();
}

// Stop the timer and record the results.
void StatsCounterTimer::Stop() {
  if (!Enabled())
    return;
  stop_time_ = OS::Ticks();
  Record();
}

} }  // namespace v8::internal
