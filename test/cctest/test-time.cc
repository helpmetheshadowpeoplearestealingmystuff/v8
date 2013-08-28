// Copyright 2013 the V8 project authors. All rights reserved.
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

#include <cstdlib>

#include "v8.h"

#include "cctest.h"
#if V8_OS_WIN
#include "win32-headers.h"
#endif

using namespace v8::internal;


TEST(TimeDeltaFromAndIn) {
  CHECK(TimeDelta::FromDays(2) == TimeDelta::FromHours(48));
  CHECK(TimeDelta::FromHours(3) == TimeDelta::FromMinutes(180));
  CHECK(TimeDelta::FromMinutes(2) == TimeDelta::FromSeconds(120));
  CHECK(TimeDelta::FromSeconds(2) == TimeDelta::FromMilliseconds(2000));
  CHECK(TimeDelta::FromMilliseconds(2) == TimeDelta::FromMicroseconds(2000));
  CHECK_EQ(static_cast<int>(13), TimeDelta::FromDays(13).InDays());
  CHECK_EQ(static_cast<int>(13), TimeDelta::FromHours(13).InHours());
  CHECK_EQ(static_cast<int>(13), TimeDelta::FromMinutes(13).InMinutes());
  CHECK_EQ(static_cast<int64_t>(13), TimeDelta::FromSeconds(13).InSeconds());
  CHECK_EQ(13.0, TimeDelta::FromSeconds(13).InSecondsF());
  CHECK_EQ(static_cast<int64_t>(13),
           TimeDelta::FromMilliseconds(13).InMilliseconds());
  CHECK_EQ(13.0, TimeDelta::FromMilliseconds(13).InMillisecondsF());
  CHECK_EQ(static_cast<int64_t>(13),
           TimeDelta::FromMicroseconds(13).InMicroseconds());
}


TEST(TimeJsTime) {
  Time t = Time::FromJsTime(700000.3);
  CHECK_EQ(700000.3, t.ToJsTime());
}


#if V8_OS_POSIX
TEST(TimeFromTimeVal) {
  Time null;
  CHECK(null.IsNull());
  CHECK(null == Time::FromTimeval(null.ToTimeval()));
  Time now = Time::Now();
  CHECK(now == Time::FromTimeval(now.ToTimeval()));
  Time now_sys = Time::NowFromSystemTime();
  CHECK(now_sys == Time::FromTimeval(now_sys.ToTimeval()));
  Time unix_epoch = Time::UnixEpoch();
  CHECK(unix_epoch == Time::FromTimeval(unix_epoch.ToTimeval()));
  Time max = Time::Max();
  CHECK(max.IsMax());
  CHECK(max == Time::FromTimeval(max.ToTimeval()));
}
#endif


#if V8_OS_WIN
TEST(TimeFromFiletime) {
  Time null;
  CHECK(null.IsNull());
  CHECK(null == Time::FromFiletime(null.ToFiletime()));
  Time now = Time::Now();
  CHECK(now == Time::FromFiletime(now.ToFiletime()));
  Time now_sys = Time::NowFromSystemTime();
  CHECK(now_sys == Time::FromFiletime(now_sys.ToFiletime()));
  Time unix_epoch = Time::UnixEpoch();
  CHECK(unix_epoch == Time::FromFiletime(unix_epoch.ToFiletime()));
  Time max = Time::Max();
  CHECK(max.IsMax());
  CHECK(max == Time::FromFiletime(max.ToFiletime()));
}
#endif


TEST(TimeTicksIsMonotonic) {
  TimeTicks previous_normal_ticks;
  TimeTicks previous_highres_ticks;
  ElapsedTimer timer;
  timer.Start();
  while (!timer.HasExpired(TimeDelta::FromMilliseconds(100))) {
    TimeTicks normal_ticks = TimeTicks::Now();
    TimeTicks highres_ticks = TimeTicks::HighResNow();
    CHECK_GE(normal_ticks, previous_normal_ticks);
    CHECK_GE((normal_ticks - previous_normal_ticks).InMicroseconds(), 0);
    CHECK_GE(highres_ticks, previous_highres_ticks);
    CHECK_GE((highres_ticks - previous_highres_ticks).InMicroseconds(), 0);
    previous_normal_ticks = normal_ticks;
    previous_highres_ticks = highres_ticks;
  }
}
