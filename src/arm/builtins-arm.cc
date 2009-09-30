// Copyright 2006-2009 the V8 project authors. All rights reserved.
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

#include "codegen-inl.h"
#include "debug.h"
#include "runtime.h"

namespace v8 {
namespace internal {


#define __ ACCESS_MASM(masm)


void Builtins::Generate_Adaptor(MacroAssembler* masm, CFunctionId id) {
  // TODO(428): Don't pass the function in a static variable.
  __ mov(ip, Operand(ExternalReference::builtin_passed_function()));
  __ str(r1, MemOperand(ip, 0));

  // The actual argument count has already been loaded into register
  // r0, but JumpToRuntime expects r0 to contain the number of
  // arguments including the receiver.
  __ add(r0, r0, Operand(1));
  __ JumpToRuntime(ExternalReference(id));
}


// Load the built-in Array function from the current context.
static void GenerateLoadArrayFunction(MacroAssembler* masm, Register result) {
  // Load the global context.

  __ ldr(result, MemOperand(cp, Context::SlotOffset(Context::GLOBAL_INDEX)));
  __ ldr(result,
         FieldMemOperand(result, GlobalObject::kGlobalContextOffset));
  // Load the Array function from the global context.
  __ ldr(result,
         MemOperand(result,
                    Context::SlotOffset(Context::ARRAY_FUNCTION_INDEX)));
}


// This constant has the same value as JSArray::kPreallocatedArrayElements and
// if JSArray::kPreallocatedArrayElements is changed handling of loop unfolding
// below should be reconsidered.
static const int kLoopUnfoldLimit = 4;


// Allocate an empty JSArray. The allocated array is put into the result
// register. An elements backing store is allocated with size initial_capacity
// and filled with the hole values.
static void AllocateEmptyJSArray(MacroAssembler* masm,
                                 Register array_function,
                                 Register result,
                                 Register scratch1,
                                 Register scratch2,
                                 Register scratch3,
                                 int initial_capacity,
                                 Label* gc_required) {
  ASSERT(initial_capacity > 0);
  // Load the initial map from the array function.
  __ ldr(scratch1, FieldMemOperand(array_function,
                                   JSFunction::kPrototypeOrInitialMapOffset));

  // Allocate the JSArray object together with space for a fixed array with the
  // requested elements.
  int size = JSArray::kSize + FixedArray::SizeFor(initial_capacity);
  __ AllocateInNewSpace(size / kPointerSize,
                        result,
                        scratch2,
                        scratch3,
                        gc_required,
                        TAG_OBJECT);

  // Allocated the JSArray. Now initialize the fields except for the elements
  // array.
  // result: JSObject
  // scratch1: initial map
  // scratch2: start of next object
  __ str(scratch1, FieldMemOperand(result, JSObject::kMapOffset));
  __ LoadRoot(scratch1, Heap::kEmptyFixedArrayRootIndex);
  __ str(scratch1, FieldMemOperand(result, JSArray::kPropertiesOffset));
  // Field JSArray::kElementsOffset is initialized later.
  __ mov(scratch3,  Operand(0));
  __ str(scratch3, FieldMemOperand(result, JSArray::kLengthOffset));

  // Calculate the location of the elements array and set elements array member
  // of the JSArray.
  // result: JSObject
  // scratch2: start of next object
  __ lea(scratch1, MemOperand(result, JSArray::kSize));
  __ str(scratch1, FieldMemOperand(result, JSArray::kElementsOffset));

  // Clear the heap tag on the elements array.
  __ and_(scratch1, scratch1, Operand(~kHeapObjectTagMask));

  // Initialize the FixedArray and fill it with holes. FixedArray length is not
  // stored as a smi.
  // result: JSObject
  // scratch1: elements array (untagged)
  // scratch2: start of next object
  __ LoadRoot(scratch3, Heap::kFixedArrayMapRootIndex);
  ASSERT_EQ(0 * kPointerSize, FixedArray::kMapOffset);
  __ str(scratch3, MemOperand(scratch1, kPointerSize, PostIndex));
  __ mov(scratch3,  Operand(initial_capacity));
  ASSERT_EQ(1 * kPointerSize, FixedArray::kLengthOffset);
  __ str(scratch3, MemOperand(scratch1, kPointerSize, PostIndex));

  // Fill the FixedArray with the hole value.
  ASSERT_EQ(2 * kPointerSize, FixedArray::kHeaderSize);
  ASSERT(initial_capacity <= kLoopUnfoldLimit);
  __ LoadRoot(scratch3, Heap::kTheHoleValueRootIndex);
  for (int i = 0; i < initial_capacity; i++) {
    __ str(scratch3, MemOperand(scratch1, kPointerSize, PostIndex));
  }
}

// Allocate a JSArray with the number of elements stored in a register. The
// register array_function holds the built-in Array function and the register
// array_size holds the size of the array as a smi. The allocated array is put
// into the result register and beginning and end of the FixedArray elements
// storage is put into registers elements_array_storage and elements_array_end
// (see  below for when that is not the case). If the parameter fill_with_holes
// is true the allocated elements backing store is filled with the hole values
// otherwise it is left uninitialized. When the backing store is filled the
// register elements_array_storage is scratched.
static void AllocateJSArray(MacroAssembler* masm,
                            Register array_function,  // Array function.
                            Register array_size,  // As a smi.
                            Register result,
                            Register elements_array_storage,
                            Register elements_array_end,
                            Register scratch1,
                            Register scratch2,
                            bool fill_with_hole,
                            Label* gc_required) {
  Label not_empty, allocated;

  // Load the initial map from the array function.
  __ ldr(elements_array_storage,
         FieldMemOperand(array_function,
                         JSFunction::kPrototypeOrInitialMapOffset));

  // Check whether an empty sized array is requested.
  __ tst(array_size, array_size);
  __ b(nz, &not_empty);

  // If an empty array is requested allocate a small elements array anyway. This
  // keeps the code below free of special casing for the empty array.
  int size = JSArray::kSize +
             FixedArray::SizeFor(JSArray::kPreallocatedArrayElements);
  __ AllocateInNewSpace(size / kPointerSize,
                        result,
                        elements_array_end,
                        scratch1,
                        gc_required,
                        TAG_OBJECT);
  __ jmp(&allocated);

  // Allocate the JSArray object together with space for a FixedArray with the
  // requested number of elements.
  __ bind(&not_empty);
  ASSERT(kSmiTagSize == 1 && kSmiTag == 0);
  __ mov(elements_array_end,
         Operand((JSArray::kSize + FixedArray::kHeaderSize) / kPointerSize));
  __ add(elements_array_end,
         elements_array_end,
         Operand(array_size, ASR, kSmiTagSize));
  __ AllocateInNewSpace(elements_array_end,
                        result,
                        scratch1,
                        scratch2,
                        gc_required,
                        TAG_OBJECT);

  // Allocated the JSArray. Now initialize the fields except for the elements
  // array.
  // result: JSObject
  // elements_array_storage: initial map
  // array_size: size of array (smi)
  __ bind(&allocated);
  __ str(elements_array_storage, FieldMemOperand(result, JSObject::kMapOffset));
  __ LoadRoot(elements_array_storage, Heap::kEmptyFixedArrayRootIndex);
  __ str(elements_array_storage,
         FieldMemOperand(result, JSArray::kPropertiesOffset));
  // Field JSArray::kElementsOffset is initialized later.
  __ str(array_size, FieldMemOperand(result, JSArray::kLengthOffset));

  // Calculate the location of the elements array and set elements array member
  // of the JSArray.
  // result: JSObject
  // array_size: size of array (smi)
  __ add(elements_array_storage, result, Operand(JSArray::kSize));
  __ str(elements_array_storage,
         FieldMemOperand(result, JSArray::kElementsOffset));

  // Clear the heap tag on the elements array.
  __ and_(elements_array_storage,
          elements_array_storage,
          Operand(~kHeapObjectTagMask));
  // Initialize the fixed array and fill it with holes. FixedArray length is not
  // stored as a smi.
  // result: JSObject
  // elements_array_storage: elements array (untagged)
  // array_size: size of array (smi)
  ASSERT(kSmiTag == 0);
  __ LoadRoot(scratch1, Heap::kFixedArrayMapRootIndex);
  ASSERT_EQ(0 * kPointerSize, FixedArray::kMapOffset);
  __ str(scratch1, MemOperand(elements_array_storage, kPointerSize, PostIndex));
  // Convert array_size from smi to value.
  __ mov(array_size,
         Operand(array_size, ASR, kSmiTagSize));
  __ tst(array_size, array_size);
  // Length of the FixedArray is the number of pre-allocated elements if
  // the actual JSArray has length 0 and the size of the JSArray for non-empty
  // JSArrays. The length of a FixedArray is not stored as a smi.
  __ mov(array_size, Operand(JSArray::kPreallocatedArrayElements), LeaveCC, eq);
  ASSERT_EQ(1 * kPointerSize, FixedArray::kLengthOffset);
  __ str(array_size,
         MemOperand(elements_array_storage, kPointerSize, PostIndex));

  // Calculate elements array and elements array end.
  // result: JSObject
  // elements_array_storage: elements array element storage
  // array_size: size of elements array
  __ add(elements_array_end,
         elements_array_storage,
         Operand(array_size, LSL, kPointerSizeLog2));

  // Fill the allocated FixedArray with the hole value if requested.
  // result: JSObject
  // elements_array_storage: elements array element storage
  // elements_array_end: start of next object
  if (fill_with_hole) {
    Label loop, entry;
    __ LoadRoot(scratch1, Heap::kTheHoleValueRootIndex);
    __ jmp(&entry);
    __ bind(&loop);
    __ str(scratch1,
           MemOperand(elements_array_storage, kPointerSize, PostIndex));
    __ bind(&entry);
    __ cmp(elements_array_storage, elements_array_end);
    __ b(lt, &loop);
  }
}

// Create a new array for the built-in Array function. This function allocates
// the JSArray object and the FixedArray elements array and initializes these.
// If the Array cannot be constructed in native code the runtime is called. This
// function assumes the following state:
//   r0: argc
//   r1: constructor (built-in Array function)
//   lr: return address
//   sp[0]: last argument
// This function is used for both construct and normal calls of Array. The only
// difference between handling a construct call and a normal call is that for a
// construct call the constructor function in r1 needs to be preserved for
// entering the generic code. In both cases argc in r0 needs to be preserved.
// Both registers are preserved by this code so no need to differentiate between
// construct call and normal call.
static void ArrayNativeCode(MacroAssembler* masm,
                            Label *call_generic_code) {
  Label argc_one_or_more, argc_two_or_more;

  // Check for array construction with zero arguments or one.
  __ cmp(r0, Operand(0));
  __ b(ne, &argc_one_or_more);

  // Handle construction of an empty array.
  AllocateEmptyJSArray(masm,
                       r1,
                       r2,
                       r3,
                       r4,
                       r5,
                       JSArray::kPreallocatedArrayElements,
                       call_generic_code);
  __ IncrementCounter(&Counters::array_function_native, 1, r3, r4);
  // Setup return value, remove receiver from stack and return.
  __ mov(r0, r2);
  __ add(sp, sp, Operand(kPointerSize));
  __ Jump(lr);

  // Check for one argument. Bail out if argument is not smi or if it is
  // negative.
  __ bind(&argc_one_or_more);
  __ cmp(r0, Operand(1));
  __ b(ne, &argc_two_or_more);
  ASSERT(kSmiTag == 0);
  __ ldr(r2, MemOperand(sp));  // Get the argument from the stack.
  __ and_(r3, r2, Operand(kIntptrSignBit | kSmiTagMask), SetCC);
  __ b(ne, call_generic_code);

  // Handle construction of an empty array of a certain size. Bail out if size
  // is too large to actually allocate an elements array.
  ASSERT(kSmiTag == 0);
  __ cmp(r2, Operand(JSObject::kInitialMaxFastElementArray << kSmiTagSize));
  __ b(ge, call_generic_code);

  // r0: argc
  // r1: constructor
  // r2: array_size (smi)
  // sp[0]: argument
  AllocateJSArray(masm,
                  r1,
                  r2,
                  r3,
                  r4,
                  r5,
                  r6,
                  r7,
                  true,
                  call_generic_code);
  __ IncrementCounter(&Counters::array_function_native, 1, r2, r4);
  // Setup return value, remove receiver and argument from stack and return.
  __ mov(r0, r3);
  __ add(sp, sp, Operand(2 * kPointerSize));
  __ Jump(lr);

  // Handle construction of an array from a list of arguments.
  __ bind(&argc_two_or_more);
  __ mov(r2, Operand(r0, LSL, kSmiTagSize));  // Convet argc to a smi.

  // r0: argc
  // r1: constructor
  // r2: array_size (smi)
  // sp[0]: last argument
  AllocateJSArray(masm,
                  r1,
                  r2,
                  r3,
                  r4,
                  r5,
                  r6,
                  r7,
                  false,
                  call_generic_code);
  __ IncrementCounter(&Counters::array_function_native, 1, r2, r6);

  // Fill arguments as array elements. Copy from the top of the stack (last
  // element) to the array backing store filling it backwards. Note:
  // elements_array_end points after the backing store therefore PreIndex is
  // used when filling the backing store.
  // r0: argc
  // r3: JSArray
  // r4: elements_array storage start (untagged)
  // r5: elements_array_end (untagged)
  // sp[0]: last argument
  Label loop, entry;
  __ jmp(&entry);
  __ bind(&loop);
  __ ldr(r2, MemOperand(sp, kPointerSize, PostIndex));
  __ str(r2, MemOperand(r5, -kPointerSize, PreIndex));
  __ bind(&entry);
  __ cmp(r4, r5);
  __ b(lt, &loop);

  // Remove caller arguments and receiver from the stack, setup return value and
  // return.
  // r0: argc
  // r3: JSArray
  // sp[0]: receiver
  __ add(sp, sp, Operand(kPointerSize));
  __ mov(r0, r3);
  __ Jump(lr);
}


void Builtins::Generate_ArrayCode(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0     : number of arguments
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------
  Label generic_array_code, one_or_more_arguments, two_or_more_arguments;

  // Get the Array function.
  GenerateLoadArrayFunction(masm, r1);

  if (FLAG_debug_code) {
    // Initial map for the builtin Array function shoud be a map.
    __ ldr(r2, FieldMemOperand(r1, JSFunction::kPrototypeOrInitialMapOffset));
    __ tst(r2, Operand(kSmiTagMask));
    __ Assert(ne, "Unexpected initial map for Array function");
    __ CompareObjectType(r2, r3, r4, MAP_TYPE);
    __ Assert(eq, "Unexpected initial map for Array function");
  }

  // Run the native code for the Array function called as a normal function.
  ArrayNativeCode(masm, &generic_array_code);

  // Jump to the generic array code if the specialized code cannot handle
  // the construction.
  __ bind(&generic_array_code);
  Code* code = Builtins::builtin(Builtins::ArrayCodeGeneric);
  Handle<Code> array_code(code);
  __ Jump(array_code, RelocInfo::CODE_TARGET);
}


void Builtins::Generate_ArrayConstructCode(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0     : number of arguments
  //  -- r1     : constructor function
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------
  Label generic_constructor;

  if (FLAG_debug_code) {
    // The array construct code is only set for the builtin Array function which
    // always have a map.
    GenerateLoadArrayFunction(masm, r2);
    __ cmp(r1, r2);
    __ Assert(eq, "Unexpected Array function");
    // Initial map for the builtin Array function should be a map.
    __ ldr(r2, FieldMemOperand(r1, JSFunction::kPrototypeOrInitialMapOffset));
    __ tst(r2, Operand(kSmiTagMask));
    __ Assert(ne, "Unexpected initial map for Array function");
    __ CompareObjectType(r2, r3, r4, MAP_TYPE);
    __ Assert(eq, "Unexpected initial map for Array function");
  }

  // Run the native code for the Array function called as a constructor.
  ArrayNativeCode(masm, &generic_constructor);

  // Jump to the generic construct code in case the specialized code cannot
  // handle the construction.
  __ bind(&generic_constructor);
  Code* code = Builtins::builtin(Builtins::JSConstructStubGeneric);
  Handle<Code> generic_construct_stub(code);
  __ Jump(generic_construct_stub, RelocInfo::CODE_TARGET);
}


void Builtins::Generate_JSConstructCall(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0     : number of arguments
  //  -- r1     : constructor function
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  Label non_function_call;
  // Check that the function is not a smi.
  __ tst(r1, Operand(kSmiTagMask));
  __ b(eq, &non_function_call);
  // Check that the function is a JSFunction.
  __ CompareObjectType(r1, r2, r2, JS_FUNCTION_TYPE);
  __ b(ne, &non_function_call);

  // Jump to the function-specific construct stub.
  __ ldr(r2, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
  __ ldr(r2, FieldMemOperand(r2, SharedFunctionInfo::kConstructStubOffset));
  __ add(pc, r2, Operand(Code::kHeaderSize - kHeapObjectTag));

  // r0: number of arguments
  // r1: called object
  __ bind(&non_function_call);

  // Set expected number of arguments to zero (not changing r0).
  __ mov(r2, Operand(0));
  __ GetBuiltinEntry(r3, Builtins::CALL_NON_FUNCTION_AS_CONSTRUCTOR);
  __ Jump(Handle<Code>(builtin(ArgumentsAdaptorTrampoline)),
          RelocInfo::CODE_TARGET);
}


void Builtins::Generate_JSConstructStubGeneric(MacroAssembler* masm) {
  // Enter a construct frame.
  __ EnterConstructFrame();

  // Preserve the two incoming parameters on the stack.
  __ mov(r0, Operand(r0, LSL, kSmiTagSize));
  __ push(r0);  // Smi-tagged arguments count.
  __ push(r1);  // Constructor function.

  // Use r7 for holding undefined which is used in several places below.
  __ LoadRoot(r7, Heap::kUndefinedValueRootIndex);

  // Try to allocate the object without transitioning into C code. If any of the
  // preconditions is not met, the code bails out to the runtime call.
  Label rt_call, allocated;
  if (FLAG_inline_new) {
    Label undo_allocation;
#ifdef ENABLE_DEBUGGER_SUPPORT
    ExternalReference debug_step_in_fp =
        ExternalReference::debug_step_in_fp_address();
    __ mov(r2, Operand(debug_step_in_fp));
    __ ldr(r2, MemOperand(r2));
    __ tst(r2, r2);
    __ b(nz, &rt_call);
#endif

    // Load the initial map and verify that it is in fact a map.
    // r1: constructor function
    // r7: undefined
    __ ldr(r2, FieldMemOperand(r1, JSFunction::kPrototypeOrInitialMapOffset));
    __ tst(r2, Operand(kSmiTagMask));
    __ b(eq, &rt_call);
    __ CompareObjectType(r2, r3, r4, MAP_TYPE);
    __ b(ne, &rt_call);

    // Check that the constructor is not constructing a JSFunction (see comments
    // in Runtime_NewObject in runtime.cc). In which case the initial map's
    // instance type would be JS_FUNCTION_TYPE.
    // r1: constructor function
    // r2: initial map
    // r7: undefined
    __ CompareInstanceType(r2, r3, JS_FUNCTION_TYPE);
    __ b(eq, &rt_call);

    // Now allocate the JSObject on the heap.
    // r1: constructor function
    // r2: initial map
    // r7: undefined
    __ ldrb(r3, FieldMemOperand(r2, Map::kInstanceSizeOffset));
    __ AllocateInNewSpace(r3, r4, r5, r6, &rt_call, NO_ALLOCATION_FLAGS);

    // Allocated the JSObject, now initialize the fields. Map is set to initial
    // map and properties and elements are set to empty fixed array.
    // r1: constructor function
    // r2: initial map
    // r3: object size
    // r4: JSObject (not tagged)
    // r7: undefined
    __ LoadRoot(r6, Heap::kEmptyFixedArrayRootIndex);
    __ mov(r5, r4);
    ASSERT_EQ(0 * kPointerSize, JSObject::kMapOffset);
    __ str(r2, MemOperand(r5, kPointerSize, PostIndex));
    ASSERT_EQ(1 * kPointerSize, JSObject::kPropertiesOffset);
    __ str(r6, MemOperand(r5, kPointerSize, PostIndex));
    ASSERT_EQ(2 * kPointerSize, JSObject::kElementsOffset);
    __ str(r6, MemOperand(r5, kPointerSize, PostIndex));

    // Fill all the in-object properties with undefined.
    // r1: constructor function
    // r2: initial map
    // r3: object size (in words)
    // r4: JSObject (not tagged)
    // r5: First in-object property of JSObject (not tagged)
    // r7: undefined
    __ add(r6, r4, Operand(r3, LSL, kPointerSizeLog2));  // End of object.
    ASSERT_EQ(3 * kPointerSize, JSObject::kHeaderSize);
    { Label loop, entry;
      __ b(&entry);
      __ bind(&loop);
      __ str(r7, MemOperand(r5, kPointerSize, PostIndex));
      __ bind(&entry);
      __ cmp(r5, Operand(r6));
      __ b(lt, &loop);
    }

    // Add the object tag to make the JSObject real, so that we can continue and
    // jump into the continuation code at any time from now on. Any failures
    // need to undo the allocation, so that the heap is in a consistent state
    // and verifiable.
    __ add(r4, r4, Operand(kHeapObjectTag));

    // Check if a non-empty properties array is needed. Continue with allocated
    // object if not fall through to runtime call if it is.
    // r1: constructor function
    // r4: JSObject
    // r5: start of next object (not tagged)
    // r7: undefined
    __ ldrb(r3, FieldMemOperand(r2, Map::kUnusedPropertyFieldsOffset));
    // The field instance sizes contains both pre-allocated property fields and
    // in-object properties.
    __ ldr(r0, FieldMemOperand(r2, Map::kInstanceSizesOffset));
    __ and_(r6,
            r0,
            Operand(0x000000FF << Map::kPreAllocatedPropertyFieldsByte * 8));
    __ add(r3, r3, Operand(r6, LSR, Map::kPreAllocatedPropertyFieldsByte * 8));
    __ and_(r6, r0, Operand(0x000000FF << Map::kInObjectPropertiesByte * 8));
    __ sub(r3, r3, Operand(r6, LSR, Map::kInObjectPropertiesByte * 8), SetCC);

    // Done if no extra properties are to be allocated.
    __ b(eq, &allocated);
    __ Assert(pl, "Property allocation count failed.");

    // Scale the number of elements by pointer size and add the header for
    // FixedArrays to the start of the next object calculation from above.
    // r1: constructor
    // r3: number of elements in properties array
    // r4: JSObject
    // r5: start of next object
    // r7: undefined
    __ add(r0, r3, Operand(FixedArray::kHeaderSize / kPointerSize));
    __ AllocateInNewSpace(r0,
                          r5,
                          r6,
                          r2,
                          &undo_allocation,
                          RESULT_CONTAINS_TOP);

    // Initialize the FixedArray.
    // r1: constructor
    // r3: number of elements in properties array
    // r4: JSObject
    // r5: FixedArray (not tagged)
    // r7: undefined
    __ LoadRoot(r6, Heap::kFixedArrayMapRootIndex);
    __ mov(r2, r5);
    ASSERT_EQ(0 * kPointerSize, JSObject::kMapOffset);
    __ str(r6, MemOperand(r2, kPointerSize, PostIndex));
    ASSERT_EQ(1 * kPointerSize, Array::kLengthOffset);
    __ str(r3, MemOperand(r2, kPointerSize, PostIndex));

    // Initialize the fields to undefined.
    // r1: constructor function
    // r2: First element of FixedArray (not tagged)
    // r3: number of elements in properties array
    // r4: JSObject
    // r5: FixedArray (not tagged)
    // r7: undefined
    __ add(r6, r2, Operand(r3, LSL, kPointerSizeLog2));  // End of object.
    ASSERT_EQ(2 * kPointerSize, FixedArray::kHeaderSize);
    { Label loop, entry;
      __ b(&entry);
      __ bind(&loop);
      __ str(r7, MemOperand(r2, kPointerSize, PostIndex));
      __ bind(&entry);
      __ cmp(r2, Operand(r6));
      __ b(lt, &loop);
    }

    // Store the initialized FixedArray into the properties field of
    // the JSObject
    // r1: constructor function
    // r4: JSObject
    // r5: FixedArray (not tagged)
    __ add(r5, r5, Operand(kHeapObjectTag));  // Add the heap tag.
    __ str(r5, FieldMemOperand(r4, JSObject::kPropertiesOffset));

    // Continue with JSObject being successfully allocated
    // r1: constructor function
    // r4: JSObject
    __ jmp(&allocated);

    // Undo the setting of the new top so that the heap is verifiable. For
    // example, the map's unused properties potentially do not match the
    // allocated objects unused properties.
    // r4: JSObject (previous new top)
    __ bind(&undo_allocation);
    __ UndoAllocationInNewSpace(r4, r5);
  }

  // Allocate the new receiver object using the runtime call.
  // r1: constructor function
  __ bind(&rt_call);
  __ push(r1);  // argument for Runtime_NewObject
  __ CallRuntime(Runtime::kNewObject, 1);
  __ mov(r4, r0);

  // Receiver for constructor call allocated.
  // r4: JSObject
  __ bind(&allocated);
  __ push(r4);

  // Push the function and the allocated receiver from the stack.
  // sp[0]: receiver (newly allocated object)
  // sp[1]: constructor function
  // sp[2]: number of arguments (smi-tagged)
  __ ldr(r1, MemOperand(sp, kPointerSize));
  __ push(r1);  // Constructor function.
  __ push(r4);  // Receiver.

  // Reload the number of arguments from the stack.
  // r1: constructor function
  // sp[0]: receiver
  // sp[1]: constructor function
  // sp[2]: receiver
  // sp[3]: constructor function
  // sp[4]: number of arguments (smi-tagged)
  __ ldr(r3, MemOperand(sp, 4 * kPointerSize));

  // Setup pointer to last argument.
  __ add(r2, fp, Operand(StandardFrameConstants::kCallerSPOffset));

  // Setup number of arguments for function call below
  __ mov(r0, Operand(r3, LSR, kSmiTagSize));

  // Copy arguments and receiver to the expression stack.
  // r0: number of arguments
  // r2: address of last argument (caller sp)
  // r1: constructor function
  // r3: number of arguments (smi-tagged)
  // sp[0]: receiver
  // sp[1]: constructor function
  // sp[2]: receiver
  // sp[3]: constructor function
  // sp[4]: number of arguments (smi-tagged)
  Label loop, entry;
  __ b(&entry);
  __ bind(&loop);
  __ ldr(ip, MemOperand(r2, r3, LSL, kPointerSizeLog2 - 1));
  __ push(ip);
  __ bind(&entry);
  __ sub(r3, r3, Operand(2), SetCC);
  __ b(ge, &loop);

  // Call the function.
  // r0: number of arguments
  // r1: constructor function
  ParameterCount actual(r0);
  __ InvokeFunction(r1, actual, CALL_FUNCTION);

  // Pop the function from the stack.
  // sp[0]: constructor function
  // sp[2]: receiver
  // sp[3]: constructor function
  // sp[4]: number of arguments (smi-tagged)
  __ pop();

  // Restore context from the frame.
  // r0: result
  // sp[0]: receiver
  // sp[1]: constructor function
  // sp[2]: number of arguments (smi-tagged)
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));

  // If the result is an object (in the ECMA sense), we should get rid
  // of the receiver and use the result; see ECMA-262 section 13.2.2-7
  // on page 74.
  Label use_receiver, exit;

  // If the result is a smi, it is *not* an object in the ECMA sense.
  // r0: result
  // sp[0]: receiver (newly allocated object)
  // sp[1]: constructor function
  // sp[2]: number of arguments (smi-tagged)
  __ tst(r0, Operand(kSmiTagMask));
  __ b(eq, &use_receiver);

  // If the type of the result (stored in its map) is less than
  // FIRST_JS_OBJECT_TYPE, it is not an object in the ECMA sense.
  __ CompareObjectType(r0, r3, r3, FIRST_JS_OBJECT_TYPE);
  __ b(ge, &exit);

  // Throw away the result of the constructor invocation and use the
  // on-stack receiver as the result.
  __ bind(&use_receiver);
  __ ldr(r0, MemOperand(sp));

  // Remove receiver from the stack, remove caller arguments, and
  // return.
  __ bind(&exit);
  // r0: result
  // sp[0]: receiver (newly allocated object)
  // sp[1]: constructor function
  // sp[2]: number of arguments (smi-tagged)
  __ ldr(r1, MemOperand(sp, 2 * kPointerSize));
  __ LeaveConstructFrame();
  __ add(sp, sp, Operand(r1, LSL, kPointerSizeLog2 - 1));
  __ add(sp, sp, Operand(kPointerSize));
  __ IncrementCounter(&Counters::constructed_objects, 1, r1, r2);
  __ Jump(lr);
}


static void Generate_JSEntryTrampolineHelper(MacroAssembler* masm,
                                             bool is_construct) {
  // Called from Generate_JS_Entry
  // r0: code entry
  // r1: function
  // r2: receiver
  // r3: argc
  // r4: argv
  // r5-r7, cp may be clobbered

  // Clear the context before we push it when entering the JS frame.
  __ mov(cp, Operand(0));

  // Enter an internal frame.
  __ EnterInternalFrame();

  // Set up the context from the function argument.
  __ ldr(cp, FieldMemOperand(r1, JSFunction::kContextOffset));

  // Set up the roots register.
  ExternalReference roots_address = ExternalReference::roots_address();
  __ mov(r10, Operand(roots_address));

  // Push the function and the receiver onto the stack.
  __ push(r1);
  __ push(r2);

  // Copy arguments to the stack in a loop.
  // r1: function
  // r3: argc
  // r4: argv, i.e. points to first arg
  Label loop, entry;
  __ add(r2, r4, Operand(r3, LSL, kPointerSizeLog2));
  // r2 points past last arg.
  __ b(&entry);
  __ bind(&loop);
  __ ldr(r0, MemOperand(r4, kPointerSize, PostIndex));  // read next parameter
  __ ldr(r0, MemOperand(r0));  // dereference handle
  __ push(r0);  // push parameter
  __ bind(&entry);
  __ cmp(r4, Operand(r2));
  __ b(ne, &loop);

  // Initialize all JavaScript callee-saved registers, since they will be seen
  // by the garbage collector as part of handlers.
  __ LoadRoot(r4, Heap::kUndefinedValueRootIndex);
  __ mov(r5, Operand(r4));
  __ mov(r6, Operand(r4));
  __ mov(r7, Operand(r4));
  if (kR9Available == 1) {
    __ mov(r9, Operand(r4));
  }

  // Invoke the code and pass argc as r0.
  __ mov(r0, Operand(r3));
  if (is_construct) {
    __ Call(Handle<Code>(Builtins::builtin(Builtins::JSConstructCall)),
            RelocInfo::CODE_TARGET);
  } else {
    ParameterCount actual(r0);
    __ InvokeFunction(r1, actual, CALL_FUNCTION);
  }

  // Exit the JS frame and remove the parameters (except function), and return.
  // Respect ABI stack constraint.
  __ LeaveInternalFrame();
  __ Jump(lr);

  // r0: result
}


void Builtins::Generate_JSEntryTrampoline(MacroAssembler* masm) {
  Generate_JSEntryTrampolineHelper(masm, false);
}


void Builtins::Generate_JSConstructEntryTrampoline(MacroAssembler* masm) {
  Generate_JSEntryTrampolineHelper(masm, true);
}


void Builtins::Generate_FunctionCall(MacroAssembler* masm) {
  // 1. Make sure we have at least one argument.
  // r0: actual number of argument
  { Label done;
    __ tst(r0, Operand(r0));
    __ b(ne, &done);
    __ LoadRoot(r2, Heap::kUndefinedValueRootIndex);
    __ push(r2);
    __ add(r0, r0, Operand(1));
    __ bind(&done);
  }

  // 2. Get the function to call from the stack.
  // r0: actual number of argument
  { Label done, non_function, function;
    __ ldr(r1, MemOperand(sp, r0, LSL, kPointerSizeLog2));
    __ tst(r1, Operand(kSmiTagMask));
    __ b(eq, &non_function);
    __ CompareObjectType(r1, r2, r2, JS_FUNCTION_TYPE);
    __ b(eq, &function);

    // Non-function called: Clear the function to force exception.
    __ bind(&non_function);
    __ mov(r1, Operand(0));
    __ b(&done);

    // Change the context eagerly because it will be used below to get the
    // right global object.
    __ bind(&function);
    __ ldr(cp, FieldMemOperand(r1, JSFunction::kContextOffset));

    __ bind(&done);
  }

  // 3. Make sure first argument is an object; convert if necessary.
  // r0: actual number of arguments
  // r1: function
  { Label call_to_object, use_global_receiver, patch_receiver, done;
    __ add(r2, sp, Operand(r0, LSL, kPointerSizeLog2));
    __ ldr(r2, MemOperand(r2, -kPointerSize));

    // r0: actual number of arguments
    // r1: function
    // r2: first argument
    __ tst(r2, Operand(kSmiTagMask));
    __ b(eq, &call_to_object);

    __ LoadRoot(r3, Heap::kNullValueRootIndex);
    __ cmp(r2, r3);
    __ b(eq, &use_global_receiver);
    __ LoadRoot(r3, Heap::kUndefinedValueRootIndex);
    __ cmp(r2, r3);
    __ b(eq, &use_global_receiver);

    __ CompareObjectType(r2, r3, r3, FIRST_JS_OBJECT_TYPE);
    __ b(lt, &call_to_object);
    __ cmp(r3, Operand(LAST_JS_OBJECT_TYPE));
    __ b(le, &done);

    __ bind(&call_to_object);
    __ EnterInternalFrame();

    // Store number of arguments and function across the call into the runtime.
    __ mov(r0, Operand(r0, LSL, kSmiTagSize));
    __ push(r0);
    __ push(r1);

    __ push(r2);
    __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_JS);
    __ mov(r2, r0);

    // Restore number of arguments and function.
    __ pop(r1);
    __ pop(r0);
    __ mov(r0, Operand(r0, ASR, kSmiTagSize));

    __ LeaveInternalFrame();
    __ b(&patch_receiver);

    // Use the global receiver object from the called function as the receiver.
    __ bind(&use_global_receiver);
    const int kGlobalIndex =
        Context::kHeaderSize + Context::GLOBAL_INDEX * kPointerSize;
    __ ldr(r2, FieldMemOperand(cp, kGlobalIndex));
    __ ldr(r2, FieldMemOperand(r2, GlobalObject::kGlobalReceiverOffset));

    __ bind(&patch_receiver);
    __ add(r3, sp, Operand(r0, LSL, kPointerSizeLog2));
    __ str(r2, MemOperand(r3, -kPointerSize));

    __ bind(&done);
  }

  // 4. Shift stuff one slot down the stack
  // r0: actual number of arguments (including call() receiver)
  // r1: function
  { Label loop;
    // Calculate the copy start address (destination). Copy end address is sp.
    __ add(r2, sp, Operand(r0, LSL, kPointerSizeLog2));
    __ add(r2, r2, Operand(kPointerSize));  // copy receiver too

    __ bind(&loop);
    __ ldr(ip, MemOperand(r2, -kPointerSize));
    __ str(ip, MemOperand(r2));
    __ sub(r2, r2, Operand(kPointerSize));
    __ cmp(r2, sp);
    __ b(ne, &loop);
  }

  // 5. Adjust the actual number of arguments and remove the top element.
  // r0: actual number of arguments (including call() receiver)
  // r1: function
  __ sub(r0, r0, Operand(1));
  __ add(sp, sp, Operand(kPointerSize));

  // 6. Get the code for the function or the non-function builtin.
  //    If number of expected arguments matches, then call. Otherwise restart
  //    the arguments adaptor stub.
  // r0: actual number of arguments
  // r1: function
  { Label invoke;
    __ tst(r1, r1);
    __ b(ne, &invoke);
    __ mov(r2, Operand(0));  // expected arguments is 0 for CALL_NON_FUNCTION
    __ GetBuiltinEntry(r3, Builtins::CALL_NON_FUNCTION);
    __ Jump(Handle<Code>(builtin(ArgumentsAdaptorTrampoline)),
                         RelocInfo::CODE_TARGET);

    __ bind(&invoke);
    __ ldr(r3, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
    __ ldr(r2,
           FieldMemOperand(r3,
                           SharedFunctionInfo::kFormalParameterCountOffset));
    __ ldr(r3,
           MemOperand(r3, SharedFunctionInfo::kCodeOffset - kHeapObjectTag));
    __ add(r3, r3, Operand(Code::kHeaderSize - kHeapObjectTag));
    __ cmp(r2, r0);  // Check formal and actual parameter counts.
    __ Jump(Handle<Code>(builtin(ArgumentsAdaptorTrampoline)),
                         RelocInfo::CODE_TARGET, ne);

    // 7. Jump to the code in r3 without checking arguments.
    ParameterCount expected(0);
    __ InvokeCode(r3, expected, expected, JUMP_FUNCTION);
  }
}


void Builtins::Generate_FunctionApply(MacroAssembler* masm) {
  const int kIndexOffset    = -5 * kPointerSize;
  const int kLimitOffset    = -4 * kPointerSize;
  const int kArgsOffset     =  2 * kPointerSize;
  const int kRecvOffset     =  3 * kPointerSize;
  const int kFunctionOffset =  4 * kPointerSize;

  __ EnterInternalFrame();

  __ ldr(r0, MemOperand(fp, kFunctionOffset));  // get the function
  __ push(r0);
  __ ldr(r0, MemOperand(fp, kArgsOffset));  // get the args array
  __ push(r0);
  __ InvokeBuiltin(Builtins::APPLY_PREPARE, CALL_JS);

  Label no_preemption, retry_preemption;
  __ bind(&retry_preemption);
  ExternalReference stack_guard_limit_address =
      ExternalReference::address_of_stack_guard_limit();
  __ mov(r2, Operand(stack_guard_limit_address));
  __ ldr(r2, MemOperand(r2));
  __ cmp(sp, r2);
  __ b(hi, &no_preemption);

  // We have encountered a preemption or stack overflow already before we push
  // the array contents.  Save r0 which is the Smi-tagged length of the array.
  __ push(r0);

  // Runtime routines expect at least one argument, so give it a Smi.
  __ mov(r0, Operand(Smi::FromInt(0)));
  __ push(r0);
  __ CallRuntime(Runtime::kStackGuard, 1);

  // Since we returned, it wasn't a stack overflow.  Restore r0 and try again.
  __ pop(r0);
  __ b(&retry_preemption);

  __ bind(&no_preemption);

  // Eagerly check for stack-overflow before starting to push the arguments.
  // r0: number of arguments.
  // r2: stack limit.
  Label okay;
  __ sub(r2, sp, r2);

  __ cmp(r2, Operand(r0, LSL, kPointerSizeLog2 - kSmiTagSize));
  __ b(hi, &okay);

  // Out of stack space.
  __ ldr(r1, MemOperand(fp, kFunctionOffset));
  __ push(r1);
  __ push(r0);
  __ InvokeBuiltin(Builtins::APPLY_OVERFLOW, CALL_JS);

  // Push current limit and index.
  __ bind(&okay);
  __ push(r0);  // limit
  __ mov(r1, Operand(0));  // initial index
  __ push(r1);

  // Change context eagerly to get the right global object if necessary.
  __ ldr(r0, MemOperand(fp, kFunctionOffset));
  __ ldr(cp, FieldMemOperand(r0, JSFunction::kContextOffset));

  // Compute the receiver.
  Label call_to_object, use_global_receiver, push_receiver;
  __ ldr(r0, MemOperand(fp, kRecvOffset));
  __ tst(r0, Operand(kSmiTagMask));
  __ b(eq, &call_to_object);
  __ LoadRoot(r1, Heap::kNullValueRootIndex);
  __ cmp(r0, r1);
  __ b(eq, &use_global_receiver);
  __ LoadRoot(r1, Heap::kUndefinedValueRootIndex);
  __ cmp(r0, r1);
  __ b(eq, &use_global_receiver);

  // Check if the receiver is already a JavaScript object.
  // r0: receiver
  __ CompareObjectType(r0, r1, r1, FIRST_JS_OBJECT_TYPE);
  __ b(lt, &call_to_object);
  __ cmp(r1, Operand(LAST_JS_OBJECT_TYPE));
  __ b(le, &push_receiver);

  // Convert the receiver to a regular object.
  // r0: receiver
  __ bind(&call_to_object);
  __ push(r0);
  __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_JS);
  __ b(&push_receiver);

  // Use the current global receiver object as the receiver.
  __ bind(&use_global_receiver);
  const int kGlobalOffset =
      Context::kHeaderSize + Context::GLOBAL_INDEX * kPointerSize;
  __ ldr(r0, FieldMemOperand(cp, kGlobalOffset));
  __ ldr(r0, FieldMemOperand(r0, GlobalObject::kGlobalReceiverOffset));

  // Push the receiver.
  // r0: receiver
  __ bind(&push_receiver);
  __ push(r0);

  // Copy all arguments from the array to the stack.
  Label entry, loop;
  __ ldr(r0, MemOperand(fp, kIndexOffset));
  __ b(&entry);

  // Load the current argument from the arguments array and push it to the
  // stack.
  // r0: current argument index
  __ bind(&loop);
  __ ldr(r1, MemOperand(fp, kArgsOffset));
  __ push(r1);
  __ push(r0);

  // Call the runtime to access the property in the arguments array.
  __ CallRuntime(Runtime::kGetProperty, 2);
  __ push(r0);

  // Use inline caching to access the arguments.
  __ ldr(r0, MemOperand(fp, kIndexOffset));
  __ add(r0, r0, Operand(1 << kSmiTagSize));
  __ str(r0, MemOperand(fp, kIndexOffset));

  // Test if the copy loop has finished copying all the elements from the
  // arguments object.
  __ bind(&entry);
  __ ldr(r1, MemOperand(fp, kLimitOffset));
  __ cmp(r0, r1);
  __ b(ne, &loop);

  // Invoke the function.
  ParameterCount actual(r0);
  __ mov(r0, Operand(r0, ASR, kSmiTagSize));
  __ ldr(r1, MemOperand(fp, kFunctionOffset));
  __ InvokeFunction(r1, actual, CALL_FUNCTION);

  // Tear down the internal frame and remove function, receiver and args.
  __ LeaveInternalFrame();
  __ add(sp, sp, Operand(3 * kPointerSize));
  __ Jump(lr);
}


static void EnterArgumentsAdaptorFrame(MacroAssembler* masm) {
  __ mov(r0, Operand(r0, LSL, kSmiTagSize));
  __ mov(r4, Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ stm(db_w, sp, r0.bit() | r1.bit() | r4.bit() | fp.bit() | lr.bit());
  __ add(fp, sp, Operand(3 * kPointerSize));
}


static void LeaveArgumentsAdaptorFrame(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0 : result being passed through
  // -----------------------------------
  // Get the number of arguments passed (as a smi), tear down the frame and
  // then tear down the parameters.
  __ ldr(r1, MemOperand(fp, -3 * kPointerSize));
  __ mov(sp, fp);
  __ ldm(ia_w, sp, fp.bit() | lr.bit());
  __ add(sp, sp, Operand(r1, LSL, kPointerSizeLog2 - kSmiTagSize));
  __ add(sp, sp, Operand(kPointerSize));  // adjust for receiver
}


void Builtins::Generate_ArgumentsAdaptorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0 : actual number of arguments
  //  -- r1 : function (passed through to callee)
  //  -- r2 : expected number of arguments
  //  -- r3 : code entry to call
  // -----------------------------------

  Label invoke, dont_adapt_arguments;

  Label enough, too_few;
  __ cmp(r0, Operand(r2));
  __ b(lt, &too_few);
  __ cmp(r2, Operand(SharedFunctionInfo::kDontAdaptArgumentsSentinel));
  __ b(eq, &dont_adapt_arguments);

  {  // Enough parameters: actual >= expected
    __ bind(&enough);
    EnterArgumentsAdaptorFrame(masm);

    // Calculate copy start address into r0 and copy end address into r2.
    // r0: actual number of arguments as a smi
    // r1: function
    // r2: expected number of arguments
    // r3: code entry to call
    __ add(r0, fp, Operand(r0, LSL, kPointerSizeLog2 - kSmiTagSize));
    // adjust for return address and receiver
    __ add(r0, r0, Operand(2 * kPointerSize));
    __ sub(r2, r0, Operand(r2, LSL, kPointerSizeLog2));

    // Copy the arguments (including the receiver) to the new stack frame.
    // r0: copy start address
    // r1: function
    // r2: copy end address
    // r3: code entry to call

    Label copy;
    __ bind(&copy);
    __ ldr(ip, MemOperand(r0, 0));
    __ push(ip);
    __ cmp(r0, r2);  // Compare before moving to next argument.
    __ sub(r0, r0, Operand(kPointerSize));
    __ b(ne, &copy);

    __ b(&invoke);
  }

  {  // Too few parameters: Actual < expected
    __ bind(&too_few);
    EnterArgumentsAdaptorFrame(masm);

    // Calculate copy start address into r0 and copy end address is fp.
    // r0: actual number of arguments as a smi
    // r1: function
    // r2: expected number of arguments
    // r3: code entry to call
    __ add(r0, fp, Operand(r0, LSL, kPointerSizeLog2 - kSmiTagSize));

    // Copy the arguments (including the receiver) to the new stack frame.
    // r0: copy start address
    // r1: function
    // r2: expected number of arguments
    // r3: code entry to call
    Label copy;
    __ bind(&copy);
    // Adjust load for return address and receiver.
    __ ldr(ip, MemOperand(r0, 2 * kPointerSize));
    __ push(ip);
    __ cmp(r0, fp);  // Compare before moving to next argument.
    __ sub(r0, r0, Operand(kPointerSize));
    __ b(ne, &copy);

    // Fill the remaining expected arguments with undefined.
    // r1: function
    // r2: expected number of arguments
    // r3: code entry to call
    __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
    __ sub(r2, fp, Operand(r2, LSL, kPointerSizeLog2));
    __ sub(r2, r2, Operand(4 * kPointerSize));  // Adjust for frame.

    Label fill;
    __ bind(&fill);
    __ push(ip);
    __ cmp(sp, r2);
    __ b(ne, &fill);
  }

  // Call the entry point.
  __ bind(&invoke);
  __ Call(r3);

  // Exit frame and return.
  LeaveArgumentsAdaptorFrame(masm);
  __ Jump(lr);


  // -------------------------------------------
  // Dont adapt arguments.
  // -------------------------------------------
  __ bind(&dont_adapt_arguments);
  __ Jump(r3);
}


#undef __

} }  // namespace v8::internal
