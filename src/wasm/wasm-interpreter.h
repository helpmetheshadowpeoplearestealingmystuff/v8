// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_INTERPRETER_H_
#define V8_WASM_INTERPRETER_H_

#include "src/wasm/wasm-opcodes.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace base {
class AccountingAllocator;
}

namespace internal {
class WasmInstanceObject;

namespace wasm {

// forward declarations.
struct ModuleBytesEnv;
struct WasmFunction;
struct WasmModule;
class WasmInterpreterInternals;

using pc_t = size_t;
using sp_t = size_t;
using pcdiff_t = int32_t;
using spdiff_t = uint32_t;

constexpr pc_t kInvalidPc = 0x80000000;

struct ControlTransferEntry {
  // Distance from the instruction to the label to jump to (forward, but can be
  // negative).
  pcdiff_t pc_diff;
  // Delta by which to decrease the stack height.
  spdiff_t sp_diff;
  // Arity of the block we jump to.
  uint32_t target_arity;
};

using ControlTransferMap = ZoneMap<pc_t, ControlTransferEntry>;

// Macro for defining union members.
#define FOREACH_UNION_MEMBER(V) \
  V(i32, kWasmI32, int32_t)     \
  V(u32, kWasmI32, uint32_t)    \
  V(i64, kWasmI64, int64_t)     \
  V(u64, kWasmI64, uint64_t)    \
  V(f32, kWasmF32, float)       \
  V(f64, kWasmF64, double)

// Representation of values within the interpreter.
struct WasmVal {
  ValueType type;
  union {
#define DECLARE_FIELD(field, localtype, ctype) ctype field;
    FOREACH_UNION_MEMBER(DECLARE_FIELD)
#undef DECLARE_FIELD
  } val;

  WasmVal() : type(kWasmStmt) {}

#define DECLARE_CONSTRUCTOR(field, localtype, ctype) \
  explicit WasmVal(ctype v) : type(localtype) { val.field = v; }
  FOREACH_UNION_MEMBER(DECLARE_CONSTRUCTOR)
#undef DECLARE_CONSTRUCTOR

  bool operator==(const WasmVal& other) const {
    if (type != other.type) return false;
#define CHECK_VAL_EQ(field, localtype, ctype) \
  if (type == localtype) {                    \
    return val.field == other.val.field;      \
  }
    FOREACH_UNION_MEMBER(CHECK_VAL_EQ)
#undef CHECK_VAL_EQ
    UNREACHABLE();
    return false;
  }

  template <typename T>
  inline T to() const {
    UNREACHABLE();
  }

  template <typename T>
  inline T to_unchecked() const {
    UNREACHABLE();
  }
};

#define DECLARE_CAST(field, localtype, ctype)  \
  template <>                                  \
  inline ctype WasmVal::to_unchecked() const { \
    return val.field;                          \
  }                                            \
  template <>                                  \
  inline ctype WasmVal::to() const {           \
    CHECK_EQ(localtype, type);                 \
    return val.field;                          \
  }
FOREACH_UNION_MEMBER(DECLARE_CAST)
#undef DECLARE_CAST

// Representation of frames within the interpreter.
//
// Layout of a frame:
// -----------------
// stack slot #N  ‾\.
// ...             |  stack entries: GetStackHeight(); GetStackValue()
// stack slot #0  _/·
// local #L       ‾\.
// ...             |  locals: GetLocalCount(); GetLocalValue()
// local #P+1      |
// param #P        |   ‾\.
// ...             |    | parameters: GetParameterCount(); GetLocalValue()
// param #0       _/·  _/·
// -----------------
//
class InterpretedFrame {
 public:
  const WasmFunction* function() const;
  int pc() const;

  int GetParameterCount() const;
  int GetLocalCount() const;
  int GetStackHeight() const;
  WasmVal GetLocalValue(int index) const;
  WasmVal GetStackValue(int index) const;

 private:
  friend class WasmInterpreter;
  // Don't instante InterpretedFrames; they will be allocated as
  // InterpretedFrameImpl in the interpreter implementation.
  InterpretedFrame() = delete;
  DISALLOW_COPY_AND_ASSIGN(InterpretedFrame);
};

// An interpreter capable of executing WASM.
class V8_EXPORT_PRIVATE WasmInterpreter {
 public:
  // State machine for a Thread:
  //                         +---------Run()/Step()--------+
  //                         V                             |
  // STOPPED ---Run()-->  RUNNING  ------Pause()-----+-> PAUSED
  //  ^                   | | | |                   /
  //  +- HandleException -+ | | +--- Breakpoint ---+
  //                        | |
  //                        | +---------- Trap --------------> TRAPPED
  //                        +----------- Finish -------------> FINISHED
  enum State { STOPPED, RUNNING, PAUSED, FINISHED, TRAPPED };

  // Tells a thread to pause after certain instructions.
  enum BreakFlag : uint8_t {
    None = 0,
    AfterReturn = 1 << 0,
    AfterCall = 1 << 1
  };

  // Representation of a thread in the interpreter.
  class V8_EXPORT_PRIVATE Thread {
    // Don't instante Threads; they will be allocated as ThreadImpl in the
    // interpreter implementation.
    Thread() = delete;

   public:
    enum ExceptionHandlingResult { HANDLED, UNWOUND };

    // Execution control.
    State state();
    void InitFrame(const WasmFunction* function, WasmVal* args);
    // Pass -1 as num_steps to run till completion, pause or breakpoint.
    State Run(int num_steps = -1);
    State Step() { return Run(1); }
    void Pause();
    void Reset();
    // Handle the pending exception in the passed isolate. Unwind the stack
    // accordingly. Return whether the exception was handled inside wasm.
    ExceptionHandlingResult HandleException(Isolate* isolate);

    // Stack inspection and modification.
    pc_t GetBreakpointPc();
    // TODO(clemensh): Make this uint32_t.
    int GetFrameCount();
    // The InterpretedFrame is only valid as long as the Thread is paused.
    std::unique_ptr<InterpretedFrame> GetFrame(int index);
    WasmVal GetReturnValue(int index = 0);
    TrapReason GetTrapReason();

    // Returns true if the thread executed an instruction which may produce
    // nondeterministic results, e.g. float div, float sqrt, and float mul,
    // where the sign bit of a NaN is nondeterministic.
    bool PossibleNondeterminism();

    // Returns the number of calls / function frames executed on this thread.
    uint64_t NumInterpretedCalls();

    // Thread-specific breakpoints.
    // TODO(wasm): Implement this once we support multiple threads.
    // bool SetBreakpoint(const WasmFunction* function, int pc, bool enabled);
    // bool GetBreakpoint(const WasmFunction* function, int pc);

    void AddBreakFlags(uint8_t flags);
    void ClearBreakFlags();

    // Each thread can have multiple activations, each represented by a portion
    // of the stack frames of this thread. StartActivation returns the id
    // (counting from 0 up) of the started activation.
    // Activations must be properly stacked, i.e. if FinishActivation is called,
    // the given id must the the latest activation on the stack.
    uint32_t NumActivations();
    uint32_t StartActivation();
    void FinishActivation(uint32_t activation_id);
    // Return the frame base of the given activation, i.e. the number of frames
    // when this activation was started.
    uint32_t ActivationFrameBase(uint32_t activation_id);
  };

  WasmInterpreter(Isolate* isolate, const ModuleBytesEnv& env);
  ~WasmInterpreter();

  //==========================================================================
  // Execution controls.
  //==========================================================================
  void Run();
  void Pause();

  // Set a breakpoint at {pc} in {function} to be {enabled}. Returns the
  // previous state of the breakpoint at {pc}.
  bool SetBreakpoint(const WasmFunction* function, pc_t pc, bool enabled);

  // Gets the current state of the breakpoint at {function}.
  bool GetBreakpoint(const WasmFunction* function, pc_t pc);

  // Enable or disable tracing for {function}. Return the previous state.
  bool SetTracing(const WasmFunction* function, bool enabled);

  // Set the associated wasm instance object.
  // If the instance object has been set, some tables stored inside it are used
  // instead of the tables stored in the WasmModule struct. This allows to call
  // back and forth between the interpreter and outside code (JS or wasm
  // compiled) without repeatedly copying information.
  void SetInstanceObject(WasmInstanceObject*);

  //==========================================================================
  // Thread iteration and inspection.
  //==========================================================================
  int GetThreadCount();
  Thread* GetThread(int id);

  //==========================================================================
  // Memory access.
  //==========================================================================
  size_t GetMemorySize();
  WasmVal ReadMemory(size_t offset);
  void WriteMemory(size_t offset, WasmVal val);
  // Update the memory region, e.g. after external GrowMemory.
  void UpdateMemory(byte* mem_start, uint32_t mem_size);

  //==========================================================================
  // Testing functionality.
  //==========================================================================
  // Manually adds a function to this interpreter. The func_index of the
  // function must match the current number of functions.
  void AddFunctionForTesting(const WasmFunction* function);
  // Manually adds code to the interpreter for the given function.
  void SetFunctionCodeForTesting(const WasmFunction* function,
                                 const byte* start, const byte* end);

  // Computes the control transfers for the given bytecode. Used internally in
  // the interpreter, but exposed for testing.
  static ControlTransferMap ComputeControlTransfersForTesting(
      Zone* zone, const WasmModule* module, const byte* start, const byte* end);

 private:
  Zone zone_;
  WasmInterpreterInternals* internals_;
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_INTERPRETER_H_
