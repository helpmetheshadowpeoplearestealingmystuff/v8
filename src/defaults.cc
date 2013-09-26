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

#include "../include/v8-defaults.h"

#include "platform.h"
#include "globals.h"
#include "v8.h"

namespace v8 {

bool ConfigureResourceConstraintsForCurrentPlatform(
    ResourceConstraints* constraints) {
  if (constraints == NULL) {
    return false;
  }

  uint64_t physical_memory = i::OS::TotalPhysicalMemory();
  int lump_of_memory = (i::kPointerSize / 4) * i::MB;

  // The young_space_size should be a power of 2 and old_generation_size should
  // be a multiple of Page::kPageSize.
  if (physical_memory > 2ul * i::GB) {
    constraints->set_max_young_space_size(8 * lump_of_memory);
    constraints->set_max_old_space_size(700 * lump_of_memory);
    constraints->set_max_executable_size(256 * lump_of_memory);
  } else if (physical_memory > 512ul * i::MB) {
    constraints->set_max_young_space_size(4 * lump_of_memory);
    constraints->set_max_old_space_size(192 * lump_of_memory);
    constraints->set_max_executable_size(192 * lump_of_memory);
  } else /* (physical_memory <= 512GB) */ {
    constraints->set_max_young_space_size(1 * lump_of_memory);
    constraints->set_max_old_space_size(96 * lump_of_memory);
    constraints->set_max_executable_size(96 * lump_of_memory);
  }
  return true;
}


bool SetDefaultResourceConstraintsForCurrentPlatform() {
  ResourceConstraints constraints;
  if (!ConfigureResourceConstraintsForCurrentPlatform(&constraints))
    return false;
  return SetResourceConstraints(&constraints);
}

}  // namespace v8::internal
