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

// Platform specific code for Solaris 10 goes here. For the POSIX comaptible
// parts the implementation is in platform-posix.cc.

#ifdef __sparc
# error "V8 does not support the SPARC CPU architecture."
#endif

#include <sys/stack.h>  // for stack alignment
#include <unistd.h>  // getpagesize(), usleep()
#include <sys/mman.h>  // mmap()
#include <ucontext.h>  // walkstack(), getcontext()
#include <dlfcn.h>     // dladdr
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <sys/time.h>  // gettimeofday(), timeradd()
#include <errno.h>
#include <ieeefp.h>  // finite()
#include <signal.h>  // sigemptyset(), etc
#include <sys/regset.h>


#undef MAP_TYPE

#include "v8.h"

#include "platform-posix.h"
#include "platform.h"
#include "v8threads.h"
#include "vm-state-inl.h"


// It seems there is a bug in some Solaris distributions (experienced in
// SunOS 5.10 Generic_141445-09) which make it difficult or impossible to
// access signbit() despite the availability of other C99 math functions.
#ifndef signbit
namespace std {
// Test sign - usually defined in math.h
int signbit(double x) {
  // We need to take care of the special case of both positive and negative
  // versions of zero.
  if (x == 0) {
    return fpclass(x) & FP_NZERO;
  } else {
    // This won't detect negative NaN but that should be okay since we don't
    // assume that behavior.
    return x < 0;
  }
}
}  // namespace std
#endif  // signbit

namespace v8 {
namespace internal {


const char* OS::LocalTimezone(double time) {
  if (std::isnan(time)) return "";
  time_t tv = static_cast<time_t>(floor(time/msPerSecond));
  struct tm* t = localtime(&tv);
  if (NULL == t) return "";
  return tzname[0];  // The location of the timezone string on Solaris.
}


double OS::LocalTimeOffset() {
  tzset();
  return -static_cast<double>(timezone * msPerSecond);
}


void OS::DumpBacktrace() {
  // Currently unsupported.
}


class PosixMemoryMappedFile : public OS::MemoryMappedFile {
 public:
  PosixMemoryMappedFile(FILE* file, void* memory, int size)
    : file_(file), memory_(memory), size_(size) { }
  virtual ~PosixMemoryMappedFile();
  virtual void* memory() { return memory_; }
  virtual int size() { return size_; }
 private:
  FILE* file_;
  void* memory_;
  int size_;
};


OS::MemoryMappedFile* OS::MemoryMappedFile::open(const char* name) {
  FILE* file = fopen(name, "r+");
  if (file == NULL) return NULL;

  fseek(file, 0, SEEK_END);
  int size = ftell(file);

  void* memory =
      mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fileno(file), 0);
  return new PosixMemoryMappedFile(file, memory, size);
}


OS::MemoryMappedFile* OS::MemoryMappedFile::create(const char* name, int size,
    void* initial) {
  FILE* file = fopen(name, "w+");
  if (file == NULL) return NULL;
  int result = fwrite(initial, size, 1, file);
  if (result < 1) {
    fclose(file);
    return NULL;
  }
  void* memory =
      mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fileno(file), 0);
  return new PosixMemoryMappedFile(file, memory, size);
}


PosixMemoryMappedFile::~PosixMemoryMappedFile() {
  if (memory_) munmap(memory_, size_);
  fclose(file_);
}


void OS::LogSharedLibraryAddresses() {
}


void OS::SignalCodeMovingGC() {
}


struct StackWalker {
  Vector<OS::StackFrame>& frames;
  int index;
};


static int StackWalkCallback(uintptr_t pc, int signo, void* data) {
  struct StackWalker* walker = static_cast<struct StackWalker*>(data);
  Dl_info info;

  int i = walker->index;

  walker->frames[i].address = reinterpret_cast<void*>(pc);

  // Make sure line termination is in place.
  walker->frames[i].text[OS::kStackWalkMaxTextLen - 1] = '\0';

  Vector<char> text = MutableCStrVector(walker->frames[i].text,
                                        OS::kStackWalkMaxTextLen);

  if (dladdr(reinterpret_cast<void*>(pc), &info) == 0) {
    OS::SNPrintF(text, "[0x%p]", pc);
  } else if ((info.dli_fname != NULL && info.dli_sname != NULL)) {
    // We have symbol info.
    OS::SNPrintF(text, "%s'%s+0x%x", info.dli_fname, info.dli_sname, pc);
  } else {
    // No local symbol info.
    OS::SNPrintF(text,
                 "%s'0x%p [0x%p]",
                 info.dli_fname,
                 pc - reinterpret_cast<uintptr_t>(info.dli_fbase),
                 pc);
  }
  walker->index++;
  return 0;
}


int OS::StackWalk(Vector<OS::StackFrame> frames) {
  ucontext_t ctx;
  struct StackWalker walker = { frames, 0 };

  if (getcontext(&ctx) < 0) return kStackWalkError;

  if (!walkcontext(&ctx, StackWalkCallback, &walker)) {
    return kStackWalkError;
  }

  return walker.index;
}

} }  // namespace v8::internal
