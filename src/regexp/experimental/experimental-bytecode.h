// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGEXP_EXPERIMENTAL_EXPERIMENTAL_BYTECODE_H_
#define V8_REGEXP_EXPERIMENTAL_EXPERIMENTAL_BYTECODE_H_

#include <ios>

#include "src/regexp/regexp-ast.h"
#include "src/utils/vector.h"

// ----------------------------------------------------------------------------
// Definition and semantics of the EXPERIMENTAL bytecode.
// Background:
// - Russ Cox's blog post series on regular expression matching, in particular
//   https://swtch.com/~rsc/regexp/regexp2.html
// - The re2 regular regexp library: https://github.com/google/re2
//
// This comment describes the bytecode used by the experimental regexp engine
// and its abstract semantics in terms of a VM.  An implementation of the
// semantics that avoids exponential runtime can be found in `NfaInterpreter`.
//
// The experimental bytecode describes a non-deterministic finite automaton. It
// runs on a multithreaded virtual machine (VM), i.e. in several threads
// concurrently.  (These "threads" don't need to be actual operating system
// threads.)  Apart from a list of threads, the VM maintains an immutable
// shared input string which threads can read from.  Each thread is given by a
// program counter (PC, index of the current instruction), a fixed number of
// registers of indices into the input string, and a monotonically increasing
// index which represents the current position within the input string.
//
// For the precise encoding of the instruction set, see the definition `struct
// RegExpInstruction` below.  Currently we support the following instructions:
// - CONSUME_RANGE: Check whether the codepoint of the current character is
//   contained in a non-empty closed interval [min, max] specified in the
//   instruction payload.  Abort this thread if false, otherwise advance the
//   input position by 1 and continue with the next instruction.
// - ACCEPT: Stop this thread and signify the end of a match at the current
//   input position.
// - FORK: If executed by a thread t, spawn a new thread t0 whose register
//   values and input position agree with those of t, but whose PC value is set
//   to the value specified in the instruction payload.  The register values of
//   t and t0 agree directly after the FORK, but they can diverge.  Thread t
//   continues with the instruction directly after the current FORK
//   instruction.
// - JMP: Instead of incrementing the PC value after execution of this
//   instruction by 1, set PC of this thread to the value specified in the
//   instruction payload and continue there.
// - SET_REGISTER_TO_CP: Set a register specified in the paylod to the current
//   position (CP) within the input, then continue with the next instruction.
// - CLEAR_REGISTER: Clear the register specified in the payload by resetting
//   it to the initial value -1.
//
// Special care must be exercised with respect to thread priority.  It is
// possible that more than one thread executes an ACCEPT statement.  The output
// of the program is given by the contents of the matching thread's registers,
// so this is ambiguous in case of multiple matches.  To resolve the ambiguity,
// every implementation of the VM  must output the match that a backtracking
// implementation would output (i.e. behave the same as Irregexp).
//
// A backtracking implementation of the VM maintains a stack of postponed
// threads.  Upon encountering a FORK statement, this VM will create a copy of
// the current thread, set the copy's PC value according to the instruction
// payload, and push it to the stack of postponed threads.  The VM will then
// continue execution of the current thread.
//
// If at some point a thread t executes a MATCH statement, the VM stops and
// outputs the registers of t.  Postponed threads are discarded.  On the other
// hand, if a thread t is aborted because some input character didn't pass a
// check, then the VM pops the topmost postponed thread and continues execution
// with this thread.  If there are no postponed threads, then the VM outputs
// failure, i.e. no matches.
//
// Equivalently, we can describe the behavior of the backtracking VM in terms
// of priority: Threads are linearly ordered by priority, and matches generated
// by threads with high priority must be preferred over matches generated by
// threads with low priority, regardless of the chronological order in which
// matches were found.  If a thread t executes a FORK statement and spawns a
// thread t0, then the priority of t0 is such that the following holds:
// * t0 < t, i.e. t0 has lower priority than t.
// * For all threads u such that u != t and u != t0, we have t0 < u iff t < u,
//   i.e. the t0 compares to other threads the same as t.
// For example, if there are currently 3 threads s, t, u such that s < t < u,
// then after t executes a fork, the thread priorities will be s < t0 < t < u.

namespace v8 {
namespace internal {

// Bytecode format.
// Currently very simple fixed-size: The opcode is encoded in the first 4
// bytes, the payload takes another 4 bytes.
struct RegExpInstruction {
  enum Opcode : int32_t {
    ACCEPT,
    ASSERTION,
    CLEAR_REGISTER,
    CONSUME_RANGE,
    FORK,
    JMP,
    SET_REGISTER_TO_CP,
  };

  struct Uc16Range {
    uc16 min;  // Inclusive.
    uc16 max;  // Inclusive.
  };

  static RegExpInstruction ConsumeRange(Uc16Range consume_range) {
    RegExpInstruction result;
    result.opcode = CONSUME_RANGE;
    result.payload.consume_range = consume_range;
    return result;
  }

  static RegExpInstruction Fork(int32_t alt_index) {
    RegExpInstruction result;
    result.opcode = FORK;
    result.payload.pc = alt_index;
    return result;
  }

  static RegExpInstruction Jmp(int32_t alt_index) {
    RegExpInstruction result;
    result.opcode = JMP;
    result.payload.pc = alt_index;
    return result;
  }

  static RegExpInstruction Accept() {
    RegExpInstruction result;
    result.opcode = ACCEPT;
    return result;
  }

  static RegExpInstruction SetRegisterToCp(int32_t register_index) {
    RegExpInstruction result;
    result.opcode = SET_REGISTER_TO_CP;
    result.payload.register_index = register_index;
    return result;
  }

  static RegExpInstruction ClearRegister(int32_t register_index) {
    RegExpInstruction result;
    result.opcode = CLEAR_REGISTER;
    result.payload.register_index = register_index;
    return result;
  }

  static RegExpInstruction Assertion(RegExpAssertion::AssertionType t) {
    RegExpInstruction result;
    result.opcode = ASSERTION;
    result.payload.assertion_type = t;
    return result;
  }

  Opcode opcode;
  union {
    // Payload of CONSUME_RANGE:
    Uc16Range consume_range;
    // Payload of FORK and JMP, the next/forked program counter (pc):
    int32_t pc;
    // Payload of SET_REGISTER_TO_CP and CLEAR_REGISTER:
    int32_t register_index;
    // Payload of ASSERTION:
    RegExpAssertion::AssertionType assertion_type;
  } payload;
  STATIC_ASSERT(sizeof(payload) == 4);
};
STATIC_ASSERT(sizeof(RegExpInstruction) == 8);
// TODO(mbid,v8:10765): This is rather wasteful.  We can fit the opcode in 2-3
// bits, so the remaining 29/30 bits can be used as payload.  Problem: The
// payload of CONSUME_RANGE consists of two 16-bit values `min` and `max`, so
// this wouldn't fit.  We could encode the payload of a CONSUME_RANGE
// instruction by the start of the interval and its length instead, and then
// only allows lengths that fit into 14/13 bits.  A longer range can then be
// encoded as a disjunction of smaller ranges.
//
// Another thought: CONSUME_RANGEs are only valid if the payloads are such that
// min <= max. Thus there are
//
//     2^16 + 2^16 - 1 + ... + 1
//   = 2^16 * (2^16 + 1) / 2
//   = 2^31 + 2^15
//
// valid payloads for a CONSUME_RANGE instruction.  If we want to fit
// instructions into 4 bytes, we would still have almost 2^31 instructions left
// over if we encode everything as tight as possible.  For example, we could
// use another 2^29 values for JMP, another 2^29 for FORK, 1 value for ACCEPT,
// and then still have almost 2^30 instructions left over for something like
// zero-width assertions and captures.

std::ostream& operator<<(std::ostream& os, const RegExpInstruction& inst);
std::ostream& operator<<(std::ostream& os,
                         Vector<const RegExpInstruction> insts);

}  // namespace internal
}  // namespace v8

#endif  // V8_REGEXP_EXPERIMENTAL_EXPERIMENTAL_BYTECODE_H_
