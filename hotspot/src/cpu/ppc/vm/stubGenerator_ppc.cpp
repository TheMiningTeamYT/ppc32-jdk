/*
 * Copyright (c) 1997, 2019, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2012, 2019, SAP SE. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */
// This file is a derivative work resulting from (and including) modifications
// made by Azul Systems, Inc.  The dates of such changes are 2013-2016.
// Copyright 2013-2016 Azul Systems, Inc.  All Rights Reserved.
//
// Please contact Azul Systems, 385 Moffett Park Drive, Suite 115, Sunnyvale,
// CA 94089 USA or visit www.azul.com if you need additional information or
// have any questions.

#include "precompiled.hpp"
#include "asm/macroAssembler.inline.hpp"
#include "interpreter/interpreter.hpp"
#include "nativeInst_ppc.hpp"
#include "oops/instanceOop.hpp"
#include "oops/method.hpp"
#include "oops/objArrayKlass.hpp"
#include "oops/oop.inline.hpp"
#include "prims/methodHandles.hpp"
#include "runtime/frame.inline.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/stubCodeGenerator.hpp"
#include "runtime/stubRoutines.hpp"
#include "utilities/top.hpp"
#include "runtime/thread.inline.hpp"

#define __ _masm->

#ifdef PRODUCT
#define BLOCK_COMMENT(str) // nothing
#else
#define BLOCK_COMMENT(str) __ block_comment(str)
#endif

#if !defined(PPC64) || defined(ABI_ELFv2)
#define STUB_ENTRY(name) StubRoutines::name()
#else
#define STUB_ENTRY(name) ((FunctionDescriptor*)StubRoutines::name())->entry()
#endif

class StubGenerator: public StubCodeGenerator {
 private:

  // Call stubs are used to call Java from C
  //
  // Arguments:
  //
  //   R3  - call wrapper address     : address
  //   R4  - result                   : intptr_t*
  //   R5  - result type              : BasicType
  //   R6  - method                   : Method
  //   R7  - frame mgr entry point    : address
  //   R8  - parameter block          : intptr_t*
  //   R9  - parameter count in words : int
  //   R10 - thread                   : Thread*
  //
  address generate_call_stub(address& return_address) {
    // Setup a new c frame, copy java arguments, call frame manager or
    // native_entry, and process result.

    StubCodeMark mark(this, "StubRoutines", "call_stub");

    address start = __ function_entry();

    // some sanity checks
    assert((sizeof(frame::abi_minframe) % 16) == 0,           "unaligned");
    assert((sizeof(frame::abi_reg_args) % 16) == 0,           "unaligned");
    assert((sizeof(frame::spill_nonvolatiles) % 16) == 0,     "unaligned");
    assert((sizeof(frame::parent_ijava_frame_abi) % 16) == 0, "unaligned");
    assert((sizeof(frame::entry_frame_locals) % 16) == 0,     "unaligned");

    Register r_arg_call_wrapper_addr        = R3;
    Register r_arg_result_addr              = R4;
    Register r_arg_result_type              = R5;
    Register r_arg_method                   = R6;
    Register r_arg_entry                    = R7;
    Register r_arg_thread                   = R10;

    Register r_temp                         = R24;
    Register r_top_of_arguments_addr        = R25;
    Register r_entryframe_fp                = R26;

    {
      // Stack on entry to call_stub:
      //
      //      F1      [C_FRAME]
      //              ...

      Register r_arg_argument_addr          = R8;
      Register r_arg_argument_count         = R9;
      Register r_frame_alignment_in_bytes   = R27;
      Register r_argument_addr              = R28;
      Register r_argumentcopy_addr          = R29;
      Register r_argument_size_in_bytes     = R30;
      Register r_frame_size                 = R23;

      Label arguments_copied;

      // Save LR(/CR) to caller's C_FRAME.
#ifdef PPC64
      __ save_LR_CR(R0);
#else
      __ save_LR(R0);
#endif

      // Zero extend arg_argument_count.
      __ clrli(r_arg_argument_count, r_arg_argument_count, 32);

      // Save non-volatiles GPRs to ENTRY_FRAME (not yet pushed, but it's safe).
      __ save_nonvolatile_gprs(R1_SP, _spill_nonvolatiles_neg(r14));

      // Keep copy of our frame pointer (caller's SP).
      __ mr(r_entryframe_fp, R1_SP);

      BLOCK_COMMENT("Push ENTRY_FRAME including arguments");
      // Push ENTRY_FRAME including arguments:
      //
      //      F0      [TOP_IJAVA_FRAME_ABI]
      //              alignment (optional)
      //              [outgoing Java arguments]
      //              [ENTRY_FRAME_LOCALS]
      //      F1      [C_FRAME]
      //              ...

      // calculate frame size

      // unaligned size of arguments
      __ sli(r_argument_size_in_bytes,
                  r_arg_argument_count, Interpreter::logStackElementSize);
      // arguments alignment (max 1 slot)
      // FIXME: use round_to() here
#ifdef PPC64
      __ andi_(r_frame_alignment_in_bytes, r_arg_argument_count, 1);
#else
      __ andi(r_frame_alignment_in_bytes, r_arg_argument_count, 3);
      __ subfic(r_frame_alignment_in_bytes, r_frame_alignment_in_bytes, 4);
#endif
      __ sli(r_frame_alignment_in_bytes,
              r_frame_alignment_in_bytes, Interpreter::logStackElementSize);

      // size = unaligned size of arguments + top abi's size
      __ addi(r_frame_size, r_argument_size_in_bytes,
              frame::top_ijava_frame_abi_size);
      // size += arguments alignment
      __ add(r_frame_size,
             r_frame_size, r_frame_alignment_in_bytes);
      // size += size of call_stub locals
      __ addi(r_frame_size,
              r_frame_size, frame::entry_frame_locals_size);

      // push ENTRY_FRAME
      __ push_frame(r_frame_size, r_temp);

      // initialize call_stub locals (step 1)
      __ st(r_arg_call_wrapper_addr,
             _entry_frame_locals_neg(call_wrapper_address), r_entryframe_fp);
      __ st(r_arg_result_addr,
             _entry_frame_locals_neg(result_address), r_entryframe_fp);
      __ st(r_arg_result_type,
             _entry_frame_locals_neg(result_type), r_entryframe_fp);
      // we will save arguments_tos_address later


      BLOCK_COMMENT("Copy Java arguments");
      // copy Java arguments

      // Calculate top_of_arguments_addr which will be R17_tos (not prepushed) later.
      // FIXME: why not simply use SP+frame::top_ijava_frame_size?
      __ addi(r_top_of_arguments_addr,
              R1_SP, frame::top_ijava_frame_abi_size);
      __ add(r_top_of_arguments_addr,
             r_top_of_arguments_addr, r_frame_alignment_in_bytes);

      // any arguments to copy?
      __ cmpi(CCR0, r_arg_argument_count, 0);
      __ beq(CCR0, arguments_copied);

      // prepare loop and copy arguments in reverse order
      {
        // init CTR with arg_argument_count
        __ mtctr(r_arg_argument_count);

        // let r_argumentcopy_addr point to last outgoing Java arguments P
        __ mr(r_argumentcopy_addr, r_top_of_arguments_addr);

        // let r_argument_addr point to last incoming java argument
        __ add(r_argument_addr,
                   r_arg_argument_addr, r_argument_size_in_bytes);
        __ addi(r_argument_addr, r_argument_addr, -BytesPerWord);

        // now loop while CTR > 0 and copy arguments
        {
          Label next_argument;
          __ bind(next_argument);

          __ l(r_temp, 0, r_argument_addr);
          // argument_addr--;
          __ addi(r_argument_addr, r_argument_addr, -BytesPerWord);
          __ st(r_temp, 0, r_argumentcopy_addr);
          // argumentcopy_addr++;
          __ addi(r_argumentcopy_addr, r_argumentcopy_addr, BytesPerWord);

          __ bdnz(next_argument);
        }
      }

      // Arguments copied, continue.
      __ bind(arguments_copied);
    }

    {
      BLOCK_COMMENT("Call frame manager or native entry.");
      // Call frame manager or native entry.
      Register r_new_arg_entry = R14;
      assert_different_registers(r_new_arg_entry, r_top_of_arguments_addr,
                                 r_arg_method, r_arg_thread);

      __ mr(r_new_arg_entry, r_arg_entry);

      // Register state on entry to frame manager / native entry:
      //
      //   tos         -  intptr_t*    sender tos (prepushed) Lesp = (SP) + copied_arguments_offset - 8
      //   R19_method  -  Method
      //   R16_thread  -  JavaThread*

      // Tos must point to last argument - element_size.
#ifdef CC_INTERP
      const Register tos = R17_tos;
#else
      const Register tos = R15_esp;
#endif
      __ addi(tos, r_top_of_arguments_addr, -Interpreter::stackElementSize);

      // initialize call_stub locals (step 2)
      // now save tos as arguments_tos_address
      __ st(tos, _entry_frame_locals_neg(arguments_tos_address), r_entryframe_fp);

      // load argument registers for call
      __ mr(R19_method, r_arg_method);
      __ mr(R16_thread, r_arg_thread);
      assert(tos != r_arg_method, "trashed r_arg_method");
      assert(tos != r_arg_thread && R19_method != r_arg_thread, "trashed r_arg_thread");

      // Set R15_prev_state to 0 for simplifying checks in callee.
#ifdef CC_INTERP
      __ li(R15_prev_state, 0);
#else
      __ load_const_optimized(R25_templateTableBase, (address)Interpreter::dispatch_table((TosState)0), R11_scratch1);
#endif
      // Stack on entry to frame manager / native entry:
      //
      //      F0      [TOP_IJAVA_FRAME_ABI]
      //              alignment (optional)
      //              [outgoing Java arguments]
      //              [ENTRY_FRAME_LOCALS]
      //      F1      [C_FRAME]
      //              ...
      //

      // global toc register
      __ load_const(R29, MacroAssembler::global_toc(), R11_scratch1);

#ifdef PPC64
      // Load narrow oop base.
      __ reinit_heapbase(R30, R11_scratch1);
#endif

      // Remember the senderSP so we interpreter can pop c2i arguments off of the stack
      // when called via a c2i.

      // Pass initial_caller_sp to framemanager.
      __ mr(R21_tmp1, R1_SP);

      // Do a light-weight C-call here, r_new_arg_entry holds the address
      // of the interpreter entry point (frame manager or native entry)
      // and save runtime-value of LR in return_address.
      assert(r_new_arg_entry != tos && r_new_arg_entry != R19_method && r_new_arg_entry != R16_thread,
             "trashed r_new_arg_entry");
      return_address = __ call_stub(r_new_arg_entry);
    }

    {
      BLOCK_COMMENT("Returned from frame manager or native entry.");
      // Returned from frame manager or native entry.
      // Now pop frame, process result, and return to caller.

      // Stack on exit from frame manager / native entry:
      //
      //      F0      [ABI]
      //              ...
      //              [ENTRY_FRAME_LOCALS]
      //      F1      [C_FRAME]
      //              ...
      //
      // Just pop the topmost frame ...
      //

      Label ret_is_object;
      Label ret_is_long;
      Label ret_is_float;
      Label ret_is_double;

      Register r_entryframe_fp = R30;
      Register r_lr            = R7_ARG5;
      Register r_cr            = R8_ARG6;
      // r_arg_result_addr is R4 which is part of return value on PPC32.
      // Lets take any free volatile register at this point
      Register r_safe_arg_result_addr = R9_ARG7;

      // Reload some volatile registers which we've spilled before the call
      // to frame manager / native entry.
      // Access all locals via frame pointer, because we know nothing about
      // the topmost frame's size.
      __ l(r_entryframe_fp, _abi(callers_sp), R1_SP);
      assert_different_registers(r_entryframe_fp, R3_RET, r_safe_arg_result_addr, r_arg_result_type, r_cr, r_lr);
#ifndef PPC64
      assert_different_registers(r_entryframe_fp, R4_RET2, r_safe_arg_result_addr, r_arg_result_type, r_cr, r_lr);
#endif
      __ l(r_safe_arg_result_addr,
            _entry_frame_locals_neg(result_address), r_entryframe_fp);
      __ l(r_arg_result_type,
            _entry_frame_locals_neg(result_type), r_entryframe_fp);
#ifdef PPC64
      __ l(r_cr, _abi(cr), r_entryframe_fp);
#endif
      __ l(r_lr, _abi(lr), r_entryframe_fp);

      // pop frame and restore non-volatiles, LR and CR
      __ mr(R1_SP, r_entryframe_fp);
#ifdef PPC64
      __ mtcr(r_cr);
#endif
      __ mtlr(r_lr);

      // Store result depending on type. Everything that is not
      // T_OBJECT, T_LONG, T_FLOAT, or T_DOUBLE is treated as T_INT.
      __ cmpwi(CCR0, r_arg_result_type, T_OBJECT);
      __ cmpwi(CCR1, r_arg_result_type, T_LONG);
      __ cmpwi(CCR5, r_arg_result_type, T_FLOAT);
      __ cmpwi(CCR6, r_arg_result_type, T_DOUBLE);

      // restore non-volatile registers
      __ restore_nonvolatile_gprs(R1_SP, _spill_nonvolatiles_neg(r14));

      // Stack on exit from call_stub:
      //
      //      0       [C_FRAME]
      //              ...
      //
      //  no call_stub frames left.

      // All non-volatiles have been restored at this point!!
      assert(R3_RET == R3, "R3_RET should be R3");

      __ beq(CCR0, ret_is_object);
      __ beq(CCR1, ret_is_long);
      __ beq(CCR5, ret_is_float);
      __ beq(CCR6, ret_is_double);

      // default:
      __ stw(R3_RET, 0, r_safe_arg_result_addr);
      __ blr(); // return to caller

      // case T_OBJECT:
      __ bind(ret_is_object);
      __ st(R3_RET, 0, r_safe_arg_result_addr);
      __ blr(); // return to caller

      // case T_LONG:
      __ bind(ret_is_long);
#ifdef PPC64
      __ std(R3_RET, 0, r_safe_arg_result_addr);
#else
      // R3:R4 are already in BigEndian order
      __ stw(R3_RET, 0, r_safe_arg_result_addr);
      __ stw(R4_RET2, 4, r_safe_arg_result_addr);
#endif
      __ blr(); // return to caller

      // case T_FLOAT:
      __ bind(ret_is_float);
#ifndef USE_SPE
      __ stfs(F1_RET, 0, r_safe_arg_result_addr);
#else
      __ stw(R3_RET, 0, r_safe_arg_result_addr);
#endif
      __ blr(); // return to caller

      // case T_DOUBLE:
      __ bind(ret_is_double);
#ifndef USE_SPE
      __ stfd(F1_RET, 0, r_safe_arg_result_addr);
#else
      __ evstdd_aligned(R3_RET, 0, r_safe_arg_result_addr);
#endif
      __ blr(); // return to caller
    }

    return start;
  }

  // Return point for a Java call if there's an exception thrown in
  // Java code.  The exception is caught and transformed into a
  // pending exception stored in JavaThread that can be tested from
  // within the VM.
  //
  address generate_catch_exception() {
    StubCodeMark mark(this, "StubRoutines", "catch_exception");

    address start = __ pc();

    // Registers alive
    //
    //  R16_thread
    //  R3_ARG1 - address of pending exception
    //  R4_ARG2 - return address in call stub

    const Register exception_file = R21_tmp1;
    const Register exception_line = R22_tmp2;

    __ load_const(exception_file, (void*)__FILE__);
    __ load_const(exception_line, (void*)__LINE__);

    __ st(R3_ARG1, thread_(pending_exception));
    // store into `char *'
    __ st(exception_file, thread_(exception_file));
    // store into `int'
    __ stw(exception_line, thread_(exception_line));

    // complete return to VM
    assert(StubRoutines::_call_stub_return_address != NULL, "must have been generated before");

    __ mtlr(R4_ARG2);
    // continue in call stub
    __ blr();

    return start;
  }

  // Continuation point for runtime calls returning with a pending
  // exception.  The pending exception check happened in the runtime
  // or native call stub.  The pending exception in Thread is
  // converted into a Java-level exception.
  //
  address generate_forward_exception() {
    StubCodeMark mark(this, "StubRoutines", "forward_exception");
    address start = __ pc();

#if !defined(PRODUCT)
    if (VerifyOops) {
      // Get pending exception oop.
      __ l(R3_ARG1,
                in_bytes(Thread::pending_exception_offset()),
                R16_thread);
      // Make sure that this code is only executed if there is a pending exception.
      {
        Label L;
        __ cmpi(CCR0, R3_ARG1, 0);
        __ bne(CCR0, L);
        __ stop("StubRoutines::forward exception: no pending exception (1)");
        __ bind(L);
      }
      __ verify_oop(R3_ARG1, "StubRoutines::forward exception: not an oop");
    }
#endif

    // Save LR(/CR) and copy exception pc (LR) into R4_ARG2.
#ifdef PPC64
    __ save_LR_CR(R4_ARG2);
#else
    __ save_LR(R4_ARG2);
#endif
    __ push_frame_reg_args(0, R0);
    // Find exception handler.
    __ call_VM_leaf(CAST_FROM_FN_PTR(address,
                     SharedRuntime::exception_handler_for_return_address),
                    R16_thread,
                    R4_ARG2);
    // Copy handler's address.
    __ mtctr(R3_RET);
    __ pop_frame();
#ifdef PPC64
    __ restore_LR_CR(R0);
#else
    __ restore_LR(R0);
#endif

    // Set up the arguments for the exception handler:
    //  - R3_ARG1: exception oop
    //  - R4_ARG2: exception pc.

    // Load pending exception oop.
    __ l(R3_ARG1,
              in_bytes(Thread::pending_exception_offset()),
              R16_thread);

    // The exception pc is the return address in the caller.
    // Must load it into R4_ARG2.
    __ mflr(R4_ARG2);

#ifdef ASSERT
    // Make sure exception is set.
    {
      Label L;
      __ cmpi(CCR0, R3_ARG1, 0);
      __ bne(CCR0, L);
      __ stop("StubRoutines::forward exception: no pending exception (2)");
      __ bind(L);
    }
#endif

    // Clear the pending exception.
    __ li(R0, 0);
    __ st(R0,
               in_bytes(Thread::pending_exception_offset()),
               R16_thread);
    // Jump to exception handler.
    __ bctr();

    return start;
  }

#undef __
#define __ masm->
  // Continuation point for throwing of implicit exceptions that are
  // not handled in the current activation. Fabricates an exception
  // oop and initiates normal exception dispatching in this
  // frame. Only callee-saved registers are preserved (through the
  // normal register window / RegisterMap handling).  If the compiler
  // needs all registers to be preserved between the fault point and
  // the exception handler then it must assume responsibility for that
  // in AbstractCompiler::continuation_for_implicit_null_exception or
  // continuation_for_implicit_division_by_zero_exception. All other
  // implicit exceptions (e.g., NullPointerException or
  // AbstractMethodError on entry) are either at call sites or
  // otherwise assume that stack unwinding will be initiated, so
  // caller saved registers were assumed volatile in the compiler.
  //
  // Note that we generate only this stub into a RuntimeStub, because
  // it needs to be properly traversed and ignored during GC, so we
  // change the meaning of the "__" macro within this method.
  //
  // Note: the routine set_pc_not_at_call_for_caller in
  // SharedRuntime.cpp requires that this code be generated into a
  // RuntimeStub.
  address generate_throw_exception(const char* name, address runtime_entry, bool restore_saved_exception_pc,
                                   Register arg1 = noreg, Register arg2 = noreg) {
    CodeBuffer code(name, 1024 DEBUG_ONLY(+ 512), 0);
    MacroAssembler* masm = new MacroAssembler(&code);

    OopMapSet* oop_maps  = new OopMapSet();
    int frame_size_in_bytes = frame::abi_reg_args_size;
    OopMap* map = new OopMap(frame_size_in_bytes / sizeof(jint), 0);

    StubCodeMark mark(this, "StubRoutines", "throw_exception");

    address start = __ pc();

#ifdef PPC64
    __ save_LR_CR(R11_scratch1);
#else
    __ save_LR(R11_scratch1);
#endif

    // Push a frame.
    __ push_frame_reg_args(0, R11_scratch1);

    address frame_complete_pc = __ pc();

    if (restore_saved_exception_pc) {
      __ unimplemented("StubGenerator::throw_exception with restore_saved_exception_pc", 74);
    }

    // Note that we always have a runtime stub frame on the top of
    // stack by this point. Remember the offset of the instruction
    // whose address will be moved to R11_scratch1.
    address gc_map_pc = __ get_PC_trash_LR(R11_scratch1);

    __ set_last_Java_frame(/*sp*/R1_SP, /*pc*/R11_scratch1);

    __ mr(R3_ARG1, R16_thread);
    if (arg1 != noreg) {
      __ mr(R4_ARG2, arg1);
    }
    if (arg2 != noreg) {
      __ mr(R5_ARG3, arg2);
    }
#if !defined(PPC64) || defined(ABI_ELFv2)
    __ call_c(runtime_entry, relocInfo::none);
#else
    __ call_c(CAST_FROM_FN_PTR(FunctionDescriptor*, runtime_entry), relocInfo::none);
#endif

    // Set an oopmap for the call site.
    oop_maps->add_gc_map((int)(gc_map_pc - start), map);

    __ reset_last_Java_frame();

#ifdef ASSERT
    // Make sure that this code is only executed if there is a pending
    // exception.
    {
      Label L;
      __ l(R0,
                in_bytes(Thread::pending_exception_offset()),
                R16_thread);
      __ cmpi(CCR0, R0, 0);
      __ bne(CCR0, L);
      __ stop("StubRoutines::throw_exception: no pending exception");
      __ bind(L);
    }
#endif

    // Pop frame.
    __ pop_frame();

#ifdef PPC64
    __ restore_LR_CR(R11_scratch1);
#else
    __ restore_LR(R11_scratch1);
#endif

    __ load_const(R11_scratch1, StubRoutines::forward_exception_entry());
    __ mtctr(R11_scratch1);
    __ bctr();

    // Create runtime stub with OopMap.
    RuntimeStub* stub =
      RuntimeStub::new_runtime_stub(name, &code,
                                    /*frame_complete=*/ (int)(frame_complete_pc - start),
                                    frame_size_in_bytes/wordSize,
                                    oop_maps,
                                    false);
    return stub->entry_point();
  }
#undef __

#define __ _masm->

  //  Generate G1 pre-write barrier for array.
  //
  //  Input:
  //     from     - register containing src address (only needed for spilling)
  //     to       - register containing starting address
  //     count    - register containing element count
  //     tmp      - scratch register
  //
  //  Kills:
  //     nothing
  //
  void gen_write_ref_array_pre_barrier(Register from, Register to, Register count, bool dest_uninitialized, Register Rtmp1,
                                       Register preserve1 = noreg, Register preserve2 = noreg) {
    BarrierSet* const bs = Universe::heap()->barrier_set();
    switch (bs->kind()) {
#if INCLUDE_ALL_GCS
      case BarrierSet::G1SATBCT:
      case BarrierSet::G1SATBCTLogging:
        // With G1, don't generate the call if we statically know that the target in uninitialized
        if (!dest_uninitialized) {
          int spill_slots = 3;
          if (preserve1 != noreg) { spill_slots++; }
          if (preserve2 != noreg) { spill_slots++; }
          const int frame_size = align_size_up(frame::abi_reg_args_size + spill_slots * BytesPerWord, frame::alignment_in_bytes);
          Label filtered;

          // Is marking active?
          if (in_bytes(PtrQueue::byte_width_of_active()) == 4) {
            __ lwz(Rtmp1, in_bytes(JavaThread::satb_mark_queue_offset() + PtrQueue::byte_offset_of_active()), R16_thread);
          } else {
            guarantee(in_bytes(PtrQueue::byte_width_of_active()) == 1, "Assumption");
            __ lbz(Rtmp1, in_bytes(JavaThread::satb_mark_queue_offset() + PtrQueue::byte_offset_of_active()), R16_thread);
          }
          __ cmpi(CCR0, Rtmp1, 0);
          __ beq(CCR0, filtered);

#ifdef PPC64
          __ save_LR_CR(R0);
#else
          __ save_LR(R0);
#endif
          __ push_frame(frame_size, R0);
          int slot_nr = 0;
          __ st(from,  frame_size - (++slot_nr) * wordSize, R1_SP);
          __ st(to,    frame_size - (++slot_nr) * wordSize, R1_SP);
          __ st(count, frame_size - (++slot_nr) * wordSize, R1_SP);
          if (preserve1 != noreg) { __ st(preserve1, frame_size - (++slot_nr) * wordSize, R1_SP); }
          if (preserve2 != noreg) { __ st(preserve2, frame_size - (++slot_nr) * wordSize, R1_SP); }

          __ call_VM_leaf(CAST_FROM_FN_PTR(address, BarrierSet::static_write_ref_array_pre), to, count);

          slot_nr = 0;
          __ l(from,  frame_size - (++slot_nr) * wordSize, R1_SP);
          __ l(to,    frame_size - (++slot_nr) * wordSize, R1_SP);
          __ l(count, frame_size - (++slot_nr) * wordSize, R1_SP);
          if (preserve1 != noreg) { __ l(preserve1, frame_size - (++slot_nr) * wordSize, R1_SP); }
          if (preserve2 != noreg) { __ l(preserve2, frame_size - (++slot_nr) * wordSize, R1_SP); }
          __ addi(R1_SP, R1_SP, frame_size); // pop_frame()
#ifdef PPC64
          __ restore_LR_CR(R0);
#else
          __ restore_LR(R0);
#endif

          __ bind(filtered);
        }
        break;
#endif // INCLUDE_ALL_GCS
      case BarrierSet::CardTableModRef:
      case BarrierSet::CardTableExtension:
      case BarrierSet::ModRef:
        break;
      default:
        ShouldNotReachHere();
    }
  }

  //  Generate CMS/G1 post-write barrier for array.
  //
  //  Input:
  //     addr     - register containing starting address
  //     count    - register containing element count
  //     tmp      - scratch register
  //
  //  The input registers and R0 are overwritten.
  //
  void gen_write_ref_array_post_barrier(Register addr, Register count, Register tmp, Register preserve = noreg) {
    BarrierSet* const bs = Universe::heap()->barrier_set();

    switch (bs->kind()) {
#if INCLUDE_ALL_GCS
      case BarrierSet::G1SATBCT:
      case BarrierSet::G1SATBCTLogging:
        {
          int spill_slots = (preserve != noreg) ? 1 : 0;
          const int frame_size = align_size_up(frame::abi_reg_args_size + spill_slots * BytesPerWord, frame::alignment_in_bytes);

#ifdef PPC64
          __ save_LR_CR(R0);
#else
          __ save_LR(R0);
#endif
          __ push_frame(frame_size, R0);
          if (preserve != noreg) { __ st(preserve, frame_size - 1 * wordSize, R1_SP); }
          __ call_VM_leaf(CAST_FROM_FN_PTR(address, BarrierSet::static_write_ref_array_post), addr, count);
          if (preserve != noreg) { __ l(preserve, frame_size - 1 * wordSize, R1_SP); }
          __ addi(R1_SP, R1_SP, frame_size); // pop_frame();
#ifdef PPC64
          __ restore_LR_CR(R0);
#else
          __ restore_LR(R0);
#endif
        }
        break;
#endif // INCLUDE_ALL_GCS
      case BarrierSet::CardTableModRef:
      case BarrierSet::CardTableExtension:
        {
          Label Lskip_loop, Lstore_loop;
          if (UseConcMarkSweepGC) {
            // TODO PPC port: contribute optimization / requires shared changes
            __ release();
          }

          CardTableModRefBS* const ct = (CardTableModRefBS*)bs;
          assert(sizeof(*ct->byte_map_base) == sizeof(jbyte), "adjust this code");
          assert_different_registers(addr, count, tmp);

          __ sli(count, count, LogBytesPerHeapOop);
          __ addi(count, count, -BytesPerHeapOop);
          __ add(count, addr, count);
          // Use two shifts to clear out those low order two bits! (Cannot opt. into 1.)
          __ sri(addr, addr, CardTableModRefBS::card_shift);
          __ sri(count, count, CardTableModRefBS::card_shift);
          __ subf(count, addr, count);
          assert_different_registers(R0, addr, count, tmp);
          __ load_const(tmp, (address)ct->byte_map_base);
          __ addic_(count, count, 1);
          __ beq(CCR0, Lskip_loop);
          __ li(R0, 0);
          __ mtctr(count);
          // Byte store loop
          __ bind(Lstore_loop);
          __ stbx(R0, tmp, addr);
          __ addi(addr, addr, 1);
          __ bdnz(Lstore_loop);
          __ bind(Lskip_loop);
        }
      break;
      case BarrierSet::ModRef:
        break;
      default:
        ShouldNotReachHere();
    }
  }

  // Support for void zero_words_aligned8(HeapWord* to, size_t count)
  //
  // Arguments:
  //   to:
  //   count:
  //
  // Destroys:
  //
  address generate_zero_words_aligned8() {
    StubCodeMark mark(this, "StubRoutines", "zero_words_aligned8");

    // Implemented as in ClearArray.
    address start = __ function_entry();

    Register base_ptr_reg   = R3_ARG1; // tohw (needs to be 8b aligned)
    Register cnt_dwords_reg = R4_ARG2; // count (in dwords)
    Register tmp1_reg       = R5_ARG3;
    Register tmp2_reg       = R6_ARG4;
    Register zero_reg       = R7_ARG5;

    // Procedure for large arrays (uses data cache block zero instruction).
    Label dwloop, fast, fastloop, restloop, lastdword, done;
    int cl_size = VM_Version::get_cache_line_size();
    int cl_dwords = cl_size >> 3;
    int cl_dwordaddr_bits = exact_log2(cl_dwords);
    int min_dcbz = 2; // Needs to be positive, apply dcbz only to at least min_dcbz cache lines.

    // Clear up to 128byte boundary if long enough, dword_cnt=(16-(base>>3))%16.
    __ dcbtst(base_ptr_reg);                    // Indicate write access to first cache line ...
    __ andi(tmp2_reg, cnt_dwords_reg, 1);       // to check if number of dwords is even.
    __ sri_(tmp1_reg, cnt_dwords_reg, 1);       // number of double dwords
    __ load_const_optimized(zero_reg, 0L);      // Use as zero register.

    __ cmpi(CCR1, tmp2_reg, 0);                 // cnt_dwords even?
    __ beq(CCR0, lastdword);                    // size <= 1
    __ mtctr(tmp1_reg);                         // Speculatively preload counter for rest loop (>0).
    __ cmpi(CCR0, cnt_dwords_reg, (min_dcbz+1)*cl_dwords-1); // Big enough to ensure >=min_dcbz cache lines are included?
    __ neg(tmp1_reg, base_ptr_reg);             // bit 0..58: bogus, bit 57..60: (16-(base>>3))%16, bit 61..63: 000

    __ blt(CCR0, restloop);                     // Too small. (<31=(2*cl_dwords)-1 is sufficient, but bigger performs better.)
    __ rlicl_(tmp1_reg, tmp1_reg, 64-3, 64-cl_dwordaddr_bits); // Extract number of dwords to 128byte boundary=(16-(base>>3))%16.

    __ beq(CCR0, fast);                         // already 128byte aligned
    __ mtctr(tmp1_reg);                         // Set ctr to hit 128byte boundary (0<ctr<cnt).
    __ subf(cnt_dwords_reg, tmp1_reg, cnt_dwords_reg); // rest (>0 since size>=256-8)

    // Clear in first cache line dword-by-dword if not already 128byte aligned.
    __ bind(dwloop);
      __ st(zero_reg, 0, base_ptr_reg);        // Clear 8byte aligned block.
#ifndef PPC64
      __ st(zero_reg, 4, base_ptr_reg);
#endif
      __ addi(base_ptr_reg, base_ptr_reg, 8);
    __ bdnz(dwloop);

    // clear 128byte blocks
    __ bind(fast);
    __ sri(tmp1_reg, cnt_dwords_reg, cl_dwordaddr_bits); // loop count for 128byte loop (>0 since size>=256-8)
    __ andi(tmp2_reg, cnt_dwords_reg, 1);       // to check if rest even

    __ mtctr(tmp1_reg);                         // load counter
    __ cmpi(CCR1, tmp2_reg, 0);                 // rest even?
    __ rlicl_(tmp1_reg, cnt_dwords_reg, 63, 65-cl_dwordaddr_bits); // rest in double dwords

    __ bind(fastloop);
      __ dcbz(base_ptr_reg);                    // Clear 128byte aligned block.
      __ addi(base_ptr_reg, base_ptr_reg, cl_size);
    __ bdnz(fastloop);

    //__ dcbtst(base_ptr_reg);                  // Indicate write access to last cache line.
    __ beq(CCR0, lastdword);                    // rest<=1
    __ mtctr(tmp1_reg);                         // load counter

    // Clear rest.
    __ bind(restloop);
      __ st(zero_reg, 0, base_ptr_reg);        // Clear 8byte aligned block.
      __ st(zero_reg, 8, base_ptr_reg);        // Clear 8byte aligned block.
#ifndef PPC64
      __ st(zero_reg, 4, base_ptr_reg);
      __ st(zero_reg, 12, base_ptr_reg);
#endif
      __ addi(base_ptr_reg, base_ptr_reg, 16);
    __ bdnz(restloop);

    __ bind(lastdword);
    __ beq(CCR1, done);
    __ st(zero_reg, 0, base_ptr_reg);
    __ bind(done);
    __ blr();                                   // return

    return start;
  }

  // The following routine generates a subroutine to throw an asynchronous
  // UnknownError when an unsafe access gets a fault that could not be
  // reasonably prevented by the programmer.  (Example: SIGBUS/OBJERR.)
  //
  address generate_handler_for_unsafe_access() {
    StubCodeMark mark(this, "StubRoutines", "handler_for_unsafe_access");
    address start = __ function_entry();
    __ unimplemented("StubRoutines::handler_for_unsafe_access", 93);
    return start;
  }

#if !defined(PRODUCT)
  // Wrapper which calls oopDesc::is_oop_or_null()
  // Only called by MacroAssembler::verify_oop
  static void verify_oop_helper(const char* message, oop o) {
    if (!o->is_oop_or_null()) {
      fatal(message);
    }
    ++ StubRoutines::_verify_oop_count;
  }
#endif

  // Return address of code to be called from code generated by
  // MacroAssembler::verify_oop.
  //
  // Don't generate, rather use C++ code.
  address generate_verify_oop() {
    StubCodeMark mark(this, "StubRoutines", "verify_oop");

    // this is actually a `FunctionDescriptor*'.
    address start = 0;

#if !defined(PRODUCT)
    start = CAST_FROM_FN_PTR(address, verify_oop_helper);
#endif

    return start;
  }

  // Fairer handling of safepoints for native methods.
  //
  // Generate code which reads from the polling page. This special handling is needed as the
  // linux-ppc64 kernel before 2.6.6 doesn't set si_addr on some segfaults in 64bit mode
  // (cf. http://www.kernel.org/pub/linux/kernel/v2.6/ChangeLog-2.6.6), especially when we try
  // to read from the safepoint polling page.
  address generate_load_from_poll() {
    StubCodeMark mark(this, "StubRoutines", "generate_load_from_poll");
    address start = __ function_entry();
    __ unimplemented("StubRoutines::verify_oop", 95);  // TODO PPC port
    return start;
  }

  // -XX:+OptimizeFill : convert fill/copy loops into intrinsic
  //
  // The code is implemented(ported from sparc) as we believe it benefits JVM98, however
  // tracing(-XX:+TraceOptimizeFill) shows the intrinsic replacement doesn't happen at all!
  //
  // Source code in function is_range_check_if() shows that OptimizeFill relaxed the condition
  // for turning on loop predication optimization, and hence the behavior of "array range check"
  // and "loop invariant check" could be influenced, which potentially boosted JVM98.
  //
  // Generate stub for disjoint short fill. If "aligned" is true, the
  // "to" address is assumed to be heapword aligned.
  //
  // Arguments for generated stub:
  //   to:    R3_ARG1
  //   value: R4_ARG2
  //   count: R5_ARG3 treated as signed
  //
  address generate_fill(BasicType t, bool aligned, const char* name) {
    StubCodeMark mark(this, "StubRoutines", name);
    address start = __ function_entry();
#ifndef PPC64
    // Unimplemented. StubRoutines::initialize2 have a (disabled) test for
    // this function. Enable that when this get implementation.
    __ unimplemented(__PRETTY_FUNCTION__);
#else
    const Register to    = R3_ARG1;   // source array address
    const Register value = R4_ARG2;   // fill value
    const Register count = R5_ARG3;   // elements count
    const Register temp  = R6_ARG4;   // temp register

    //assert_clean_int(count, O3);    // Make sure 'count' is clean int.

    Label L_exit, L_skip_align1, L_skip_align2, L_fill_byte;
    Label L_fill_2_bytes, L_fill_4_bytes, L_fill_elements, L_fill_32_bytes;

    int shift = -1;
    switch (t) {
       case T_BYTE:
        shift = 2;
        // Clone bytes (zero extend not needed because store instructions below ignore high order bytes).
        __ rlimi(value, value, 8, 48);     // 8 bit -> 16 bit
        __ cmpi(CCR0, count, 2<<shift);    // Short arrays (< 8 bytes) fill by element.
        __ blt(CCR0, L_fill_elements);
        __ rlimi(value, value, 16, 32);    // 16 bit -> 32 bit
        break;
       case T_SHORT:
        shift = 1;
        // Clone bytes (zero extend not needed because store instructions below ignore high order bytes).
        __ rlimi(value, value, 16, 32);    // 16 bit -> 32 bit
        __ cmpi(CCR0, count, 2<<shift);    // Short arrays (< 8 bytes) fill by element.
        __ blt(CCR0, L_fill_elements);
        break;
      case T_INT:
        shift = 0;
        __ cmpi(CCR0, count, 2<<shift);    // Short arrays (< 8 bytes) fill by element.
        __ blt(CCR0, L_fill_4_bytes);
        break;
      default: ShouldNotReachHere();
    }

    if (!aligned && (t == T_BYTE || t == T_SHORT)) {
      // Align source address at 4 bytes address boundary.
      if (t == T_BYTE) {
        // One byte misalignment happens only for byte arrays.
        __ andi_(temp, to, 1);
        __ beq(CCR0, L_skip_align1);
        __ stb(value, 0, to);
        __ addi(to, to, 1);
        __ addi(count, count, -1);
        __ bind(L_skip_align1);
      }
      // Two bytes misalignment happens only for byte and short (char) arrays.
      __ andi_(temp, to, 2);
      __ beq(CCR0, L_skip_align2);
      __ sth(value, 0, to);
      __ addi(to, to, 2);
      __ addi(count, count, -(1 << (shift - 1)));
      __ bind(L_skip_align2);
    }

    if (!aligned) {
      // Align to 8 bytes, we know we are 4 byte aligned to start.
      __ andi_(temp, to, 7);
      __ beq(CCR0, L_fill_32_bytes);
      __ stw(value, 0, to);
      __ addi(to, to, 4);
      __ addi(count, count, -(1 << shift));
      __ bind(L_fill_32_bytes);
    }

    __ li(temp, 8<<shift);                  // Prepare for 32 byte loop.
    // Clone bytes int->long as above.
    __ rlimi(value, value, 32, 0);         // 32 bit -> 64 bit

    Label L_check_fill_8_bytes;
    // Fill 32-byte chunks.
    __ subf_(count, temp, count);
    __ blt(CCR0, L_check_fill_8_bytes);

    Label L_fill_32_bytes_loop;
    __ align(32);
    __ bind(L_fill_32_bytes_loop);

    __ st(value, 0, to);
    __ st(value, 8, to);
    __ subf_(count, temp, count);           // Update count.
    __ st(value, 16, to);
    __ st(value, 24, to);

    __ addi(to, to, 32);
    __ bge(CCR0, L_fill_32_bytes_loop);

    __ bind(L_check_fill_8_bytes);
    __ add_(count, temp, count);
    __ beq(CCR0, L_exit);
    __ addic_(count, count, -(2 << shift));
    __ blt(CCR0, L_fill_4_bytes);

    //
    // Length is too short, just fill 8 bytes at a time.
    //
    Label L_fill_8_bytes_loop;
    __ bind(L_fill_8_bytes_loop);
    __ st(value, 0, to);
    __ addic_(count, count, -(2 << shift));
    __ addi(to, to, 8);
    __ bge(CCR0, L_fill_8_bytes_loop);

    // Fill trailing 4 bytes.
    __ bind(L_fill_4_bytes);
    __ andi_(temp, count, 1<<shift);
    __ beq(CCR0, L_fill_2_bytes);

    __ stw(value, 0, to);
    if (t == T_BYTE || t == T_SHORT) {
      __ addi(to, to, 4);
      // Fill trailing 2 bytes.
      __ bind(L_fill_2_bytes);
      __ andi_(temp, count, 1<<(shift-1));
      __ beq(CCR0, L_fill_byte);
      __ sth(value, 0, to);
      if (t == T_BYTE) {
        __ addi(to, to, 2);
        // Fill trailing byte.
        __ bind(L_fill_byte);
        __ andi_(count, count, 1);
        __ beq(CCR0, L_exit);
        __ stb(value, 0, to);
      } else {
        __ bind(L_fill_byte);
      }
    } else {
      __ bind(L_fill_2_bytes);
    }
    __ bind(L_exit);
    __ blr();

    // Handle copies less than 8 bytes. Int is handled elsewhere.
    if (t == T_BYTE) {
      __ bind(L_fill_elements);
      Label L_fill_2, L_fill_4;
      __ andi_(temp, count, 1);
      __ beq(CCR0, L_fill_2);
      __ stb(value, 0, to);
      __ addi(to, to, 1);
      __ bind(L_fill_2);
      __ andi_(temp, count, 2);
      __ beq(CCR0, L_fill_4);
      __ stb(value, 0, to);
      __ stb(value, 1, to);
      __ addi(to, to, 2);
      __ bind(L_fill_4);
      __ andi_(temp, count, 4);
      __ beq(CCR0, L_exit);
      __ stb(value, 0, to);
      __ stb(value, 1, to);
      __ stb(value, 2, to);
      __ stb(value, 3, to);
      __ blr();
    }

    if (t == T_SHORT) {
      Label L_fill_2;
      __ bind(L_fill_elements);
      __ andi_(temp, count, 1);
      __ beq(CCR0, L_fill_2);
      __ sth(value, 0, to);
      __ addi(to, to, 2);
      __ bind(L_fill_2);
      __ andi_(temp, count, 2);
      __ beq(CCR0, L_exit);
      __ sth(value, 0, to);
      __ sth(value, 2, to);
      __ blr();
    }
#endif
    return start;
  }

  inline void assert_positive_int(Register count) {
#ifdef ASSERT
    __ sri_(R0, count, 31);
    __ asm_assert_eq("missing zero extend", 0xAFFE);
#endif
  }

  // Generate overlap test for array copy stubs.
  //
  // Input:
  //   R3_ARG1    -  from
  //   R4_ARG2    -  to
  //   R5_ARG3    -  element count
  //
  void array_overlap_test(address no_overlap_target, int log2_elem_size) {
    Register tmp1 = R6_ARG4;
    Register tmp2 = R7_ARG5;

    assert_positive_int(R5_ARG3);

    __ subf(tmp1, R3_ARG1, R4_ARG2); // distance in bytes
    __ sli(tmp2, R5_ARG3, log2_elem_size); // size in bytes
    __ cmpl(CCR0, R3_ARG1, R4_ARG2); // Use unsigned comparison!
    __ cmpl(CCR1, tmp1, tmp2); // unsigned cmp too, so negative values
                               // (to is below from) will be forward copied,
                               // even if overlaps (strictly speaking)
    __ crnand(CCR0, Assembler::less, CCR1, Assembler::less);
    // Overlaps if Src before dst and distance smaller than size.
    // Branch to forward copy routine otherwise (within range of 32kB).
    __ bc(Assembler::bcondCRbiIs1, Assembler::bi0(CCR0, Assembler::less), no_overlap_target);

    // need to copy backwards
  }

  // The guideline in the implementations of generate_disjoint_xxx_copy
  // (xxx=byte,short,int,long,oop) is to copy as many elements as possible with
  // single instructions, but to avoid alignment interrupts (see subsequent
  // comment). Furthermore, we try to minimize misaligned access, even
  // though they cause no alignment interrupt.
  //
  // In Big-Endian mode, the PowerPC architecture requires implementations to
  // handle automatically misaligned integer halfword and word accesses,
  // word-aligned integer doubleword accesses, and word-aligned floating-point
  // accesses. Other accesses may or may not generate an Alignment interrupt
  // depending on the implementation.
  // Alignment interrupt handling may require on the order of hundreds of cycles,
  // so every effort should be made to avoid misaligned memory values.
  //
  //
  // Generate stub for disjoint byte copy.  If "aligned" is true, the
  // "from" and "to" addresses are assumed to be heapword aligned.
  //
  // Arguments for generated stub:
  //      from:  R3_ARG1
  //      to:    R4_ARG2
  //      count: R5_ARG3 treated as signed
  //
  address generate_disjoint_byte_copy(bool aligned, const char * name) {
    StubCodeMark mark(this, "StubRoutines", name);
    address start = __ function_entry();
    assert_positive_int(R5_ARG3);

    Register tmp1 = R6_ARG4;
    Register tmp2 = R7_ARG5;
    Register tmp3 = R8_ARG6;
    Register tmp4 = R9_ARG7;

    VectorSRegister tmp_vsr1  = VSR1;
    VectorSRegister tmp_vsr2  = VSR2;

    Label l_1, l_2, l_3, l_4, l_5, l_6, l_7, l_8, l_9, l_10, l_opt;

    // Don't try anything fancy if arrays don't have many elements.
    __ li(tmp3, 0);
    __ cmpwi(CCR0, R5_ARG3, 17);
    __ ble(CCR0, l_6); // copy 4 at a time

    if (!aligned) {
      __ xorr(tmp1, R3_ARG1, R4_ARG2);
      __ andi_(tmp1, tmp1, 3);
      __ bne(CCR0, l_6); // If arrays don't have the same alignment mod 4, do 4 element copy.

      // Copy elements if necessary to align to 4 bytes.
      __ neg(tmp1, R3_ARG1); // Compute distance to alignment boundary.
      __ andi_(tmp1, tmp1, 3);
      __ beq(CCR0, l_2);

      __ subf(R5_ARG3, tmp1, R5_ARG3);
      __ bind(l_9);
      __ lbz(tmp2, 0, R3_ARG1);
      __ addic_(tmp1, tmp1, -1);
      __ stb(tmp2, 0, R4_ARG2);
      __ addi(R3_ARG1, R3_ARG1, 1);
      __ addi(R4_ARG2, R4_ARG2, 1);
      __ bne(CCR0, l_9);

      __ bind(l_2);
    }

    // copy 8 elements at a time
    __ xorr(tmp2, R3_ARG1, R4_ARG2); // skip if src & dest have differing alignment mod 8
    __ andi_(tmp1, tmp2, 7);
#ifdef USE_SPE
    __ cmpwi(CCR1, tmp1, 0);
#endif
    __ bne(CCR0, l_7); // not same alignment -> to or from is aligned -> copy 8

    // copy a 2-element word if necessary to align to 8 bytes
    __ andi_(R0, R3_ARG1, 7);
    __ beq(CCR0, l_7);

    __ lwzx(tmp2, R3_ARG1, tmp3);
    __ addi(R5_ARG3, R5_ARG3, -4);
    __ stwx(tmp2, R4_ARG2, tmp3);
    { // FasterArrayCopy
      __ addi(R3_ARG1, R3_ARG1, 4);
      __ addi(R4_ARG2, R4_ARG2, 4);
    }
    __ bind(l_7);

    { // FasterArrayCopy
      __ cmpwi(CCR0, R5_ARG3, 31);
      __ ble(CCR0, l_6); // copy 2 at a time if less than 32 elements remain

      __ sri(tmp1, R5_ARG3, 5);
      __ andi_(R5_ARG3, R5_ARG3, 31);
      __ mtctr(tmp1);

     if (!VM_Version::has_vsx()) {
#ifdef USE_SPE
      // go to optimized code if to and from alignment mod 8
      __ beq(CCR1, l_opt);
#endif

      __ bind(l_8);
      // Use unrolled version for mass copying (copy 32 elements a time)
      // Load feeding store gets zero latency on Power6, however not on Power5.
      // Therefore, the following sequence is made for the good of both.
#ifdef PPC64
      __ l(tmp1, 0, R3_ARG1);
      __ l(tmp2, 8, R3_ARG1);
      __ l(tmp3, 16, R3_ARG1);
      __ l(tmp4, 24, R3_ARG1);
      __ st(tmp1, 0, R4_ARG2);
      __ st(tmp2, 8, R4_ARG2);
      __ st(tmp3, 16, R4_ARG2);
      __ st(tmp4, 24, R4_ARG2);
#else
      __ l(tmp1, 0, R3_ARG1);
      __ l(tmp2, 4, R3_ARG1);
      __ l(tmp3, 8, R3_ARG1);
      __ l(tmp4, 12, R3_ARG1);
      __ st(tmp1, 0, R4_ARG2);
      __ st(tmp2, 4, R4_ARG2);
      __ st(tmp3, 8, R4_ARG2);
      __ st(tmp4, 12, R4_ARG2);
      __ l(tmp1, 16, R3_ARG1);
      __ l(tmp2, 20, R3_ARG1);
      __ l(tmp3, 24, R3_ARG1);
      __ l(tmp4, 28, R3_ARG1);
      __ st(tmp1, 16, R4_ARG2);
      __ st(tmp2, 20, R4_ARG2);
      __ st(tmp3, 24, R4_ARG2);
      __ st(tmp4, 28, R4_ARG2);
#endif
      __ addi(R3_ARG1, R3_ARG1, 32);
      __ addi(R4_ARG2, R4_ARG2, 32);
      __ bdnz(l_8);
#ifdef USE_SPE
      __ b(l_6);

      __ bind(l_opt);

      // SPE optimization if both from and to alignment mod 8
      __ evldd(tmp1, 0, R3_ARG1);
      __ evldd(tmp2, 8, R3_ARG1);
      __ evldd(tmp3, 16, R3_ARG1);
      __ evldd(tmp4, 24, R3_ARG1);
      __ evstdd(tmp1, 0, R4_ARG2);
      __ evstdd(tmp2, 8, R4_ARG2);
      __ evstdd(tmp3, 16, R4_ARG2);
      __ evstdd(tmp4, 24, R4_ARG2);

      __ addi(R3_ARG1, R3_ARG1, 32);
      __ addi(R4_ARG2, R4_ARG2, 32);
      __ bdnz(l_opt);
#endif

    } else { // Processor supports VSX, so use it to mass copy.

      // Prefetch the data into the L2 cache.
      __ dcbt(R3_ARG1, 0);

      // If supported set DSCR pre-fetch to deepest.
      if (VM_Version::has_mfdscr()) {
        __ load_const_optimized(tmp2, VM_Version::_dscr_val | 7);
        __ mtdscr(tmp2);
      }

      __ li(tmp1, 16);

      // Backbranch target aligned to 32-byte. Not 16-byte align as
      // loop contains < 8 instructions that fit inside a single
      // i-cache sector.
      __ align(32);

      __ bind(l_10);
      // Use loop with VSX load/store instructions to
      // copy 32 elements a time.
      __ lxvd2x(tmp_vsr1, R3_ARG1);        // Load src
      __ stxvd2x(tmp_vsr1, R4_ARG2);       // Store to dst
      __ lxvd2x(tmp_vsr2, tmp1, R3_ARG1);  // Load src + 16
      __ stxvd2x(tmp_vsr2, tmp1, R4_ARG2); // Store to dst + 16
      __ addi(R3_ARG1, R3_ARG1, 32);       // Update src+=32
      __ addi(R4_ARG2, R4_ARG2, 32);       // Update dsc+=32
      __ bdnz(l_10);                       // Dec CTR and loop if not zero.

      // Restore DSCR pre-fetch value.
      if (VM_Version::has_mfdscr()) {
        __ load_const_optimized(tmp2, VM_Version::_dscr_val);
        __ mtdscr(tmp2);
      }

    } // VSX
   } // FasterArrayCopy

    __ bind(l_6);

    // copy 4 elements at a time
    __ cmpwi(CCR0, R5_ARG3, 4);
    __ blt(CCR0, l_1);
    __ sri(tmp1, R5_ARG3, 2);
    __ mtctr(tmp1); // is > 0
    __ andi_(R5_ARG3, R5_ARG3, 3);

    { // FasterArrayCopy
      __ addi(R3_ARG1, R3_ARG1, -4);
      __ addi(R4_ARG2, R4_ARG2, -4);
      __ bind(l_3);
      __ lwzu(tmp2, 4, R3_ARG1);
      __ stwu(tmp2, 4, R4_ARG2);
      __ bdnz(l_3);
      __ addi(R3_ARG1, R3_ARG1, 4);
      __ addi(R4_ARG2, R4_ARG2, 4);
    }

    // do single element copy
    __ bind(l_1);
    __ cmpwi(CCR0, R5_ARG3, 0);
    __ beq(CCR0, l_4);

    { // FasterArrayCopy
      __ mtctr(R5_ARG3);
      __ addi(R3_ARG1, R3_ARG1, -1);
      __ addi(R4_ARG2, R4_ARG2, -1);

      __ bind(l_5);
      __ lbzu(tmp2, 1, R3_ARG1);
      __ stbu(tmp2, 1, R4_ARG2);
      __ bdnz(l_5);
    }

    __ bind(l_4);
    __ li(R3_RET, 0); // return 0
    __ blr();

    return start;
  }

  // Generate stub for conjoint byte copy.  If "aligned" is true, the
  // "from" and "to" addresses are assumed to be heapword aligned.
  //
  // Arguments for generated stub:
  //      from:  R3_ARG1
  //      to:    R4_ARG2
  //      count: R5_ARG3 treated as signed
  //
  address generate_conjoint_byte_copy(bool aligned, const char * name) {
    StubCodeMark mark(this, "StubRoutines", name);
    address start = __ function_entry();

    Register tmp1 = R6_ARG4;
    Register tmp2 = R7_ARG5;
    Register tmp3 = R8_ARG6;

#if !defined(PPC64) || defined(ABI_ELFv2)
     address nooverlap_target = aligned ?
       StubRoutines::arrayof_jbyte_disjoint_arraycopy() :
       StubRoutines::jbyte_disjoint_arraycopy();
#else
    address nooverlap_target = aligned ?
      ((FunctionDescriptor*)StubRoutines::arrayof_jbyte_disjoint_arraycopy())->entry() :
      ((FunctionDescriptor*)StubRoutines::jbyte_disjoint_arraycopy())->entry();
#endif

    array_overlap_test(nooverlap_target, 0);
    // Do reverse copy. We assume the case of actual overlap is rare enough
    // that we don't have to optimize it.
    Label l_1, l_2;

    __ b(l_2);
    __ bind(l_1);
    __ stbx(tmp1, R4_ARG2, R5_ARG3);
    __ bind(l_2);
    __ addic_(R5_ARG3, R5_ARG3, -1);
    __ lbzx(tmp1, R3_ARG1, R5_ARG3);
    __ bge(CCR0, l_1);

    __ li(R3_RET, 0); // return 0
    __ blr();

    return start;
  }

  // Generate stub for disjoint short copy.  If "aligned" is true, the
  // "from" and "to" addresses are assumed to be heapword aligned.
  //
  // Arguments for generated stub:
  //      from:  R3_ARG1
  //      to:    R4_ARG2
  //  elm.count: R5_ARG3 treated as signed
  //
  // Strategy for aligned==true:
  //
  //  If length <= 9:
  //     1. copy 2 elements at a time (l_6)
  //     2. copy last element if original element count was odd (l_1)
  //
  //  If length > 9:
  //     1. copy 4 elements at a time until less than 4 elements are left (l_7)
  //     2. copy 2 elements at a time until less than 2 elements are left (l_6)
  //     3. copy last element if one was left in step 2. (l_1)
  //
  //
  // Strategy for aligned==false:
  //
  //  If length <= 9: same as aligned==true case, but NOTE: load/stores
  //                  can be unaligned (see comment below)
  //
  //  If length > 9:
  //     1. continue with step 6. if the alignment of from and to mod 4
  //        is different.
  //     2. align from and to to 4 bytes by copying 1 element if necessary
  //     3. at l_2 from and to are 4 byte aligned; continue with
  //        5. if they cannot be aligned to 8 bytes because they have
  //        got different alignment mod 8.
  //     4. at this point we know that both, from and to, have the same
  //        alignment mod 8, now copy one element if necessary to get
  //        8 byte alignment of from and to.
  //     5. copy 4 elements at a time until less than 4 elements are
  //        left; depending on step 3. all load/stores are aligned or
  //        either all loads or all stores are unaligned.
  //     6. copy 2 elements at a time until less than 2 elements are
  //        left (l_6); arriving here from step 1., there is a chance
  //        that all accesses are unaligned.
  //     7. copy last element if one was left in step 6. (l_1)
  //
  //  There are unaligned data accesses using integer load/store
  //  instructions in this stub. POWER allows such accesses.
  //
  //  According to the manuals (PowerISA_V2.06_PUBLIC, Book II,
  //  Chapter 2: Effect of Operand Placement on Performance) unaligned
  //  integer load/stores have good performance. Only unaligned
  //  floating point load/stores can have poor performance.
  //
  //  TODO:
  //
  //  1. check if aligning the backbranch target of loops is beneficial
  //
  address generate_disjoint_short_copy(bool aligned, const char * name) {
    StubCodeMark mark(this, "StubRoutines", name);

    Register tmp1 = R6_ARG4;
    Register tmp2 = R7_ARG5;
    Register tmp3 = R8_ARG6;
    Register tmp4 = R9_ARG7;

    VectorSRegister tmp_vsr1  = VSR1;
    VectorSRegister tmp_vsr2  = VSR2;

    address start = __ function_entry();
    assert_positive_int(R5_ARG3);

    Label l_1, l_2, l_3, l_4, l_5, l_6, l_7, l_8, l_9, l_opt;

    // don't try anything fancy if arrays don't have many elements
    __ li(tmp3, 0);
    __ cmpwi(CCR0, R5_ARG3, 9);
    __ ble(CCR0, l_6); // copy 2 at a time

    if (!aligned) {
      __ xorr(tmp1, R3_ARG1, R4_ARG2);
      __ andi_(tmp1, tmp1, 3);
      __ bne(CCR0, l_6); // if arrays don't have the same alignment mod 4, do 2 element copy

      // At this point it is guaranteed that both, from and to have the same alignment mod 4.

      // Copy 1 element if necessary to align to 4 bytes.
      __ andi_(tmp1, R3_ARG1, 3);
      __ beq(CCR0, l_2);

      __ lhz(tmp2, 0, R3_ARG1);
      __ addi(R3_ARG1, R3_ARG1, 2);
      __ sth(tmp2, 0, R4_ARG2);
      __ addi(R4_ARG2, R4_ARG2, 2);
      __ addi(R5_ARG3, R5_ARG3, -1);
      __ bind(l_2);

#ifndef _LP64
    }
    {
#endif
      // At this point the positions of both, from and to, are at least 4 byte aligned.

      // Copy 4 elements at a time.
      // Align to 8 bytes, but only if both, from and to, have same alignment mod 8.
      __ xorr(tmp2, R3_ARG1, R4_ARG2);
      __ andi_(tmp1, tmp2, 7);
#ifdef USE_SPE
      __ cmpwi(CCR1, tmp1, 0);
#endif
      __ bne(CCR0, l_7); // not same alignment mod 8 -> copy 4, either from or to will be unaligned

      // Copy a 2-element word if necessary to align to 8 bytes.
      __ andi_(R0, R3_ARG1, 7);
      __ beq(CCR0, l_7);

      __ lwzx(tmp2, R3_ARG1, tmp3);
      __ addi(R5_ARG3, R5_ARG3, -2);
      __ stwx(tmp2, R4_ARG2, tmp3);
      { // FasterArrayCopy
        __ addi(R3_ARG1, R3_ARG1, 4);
        __ addi(R4_ARG2, R4_ARG2, 4);
      }
    }

    __ bind(l_7);

    // Copy 4 elements at a time; either the loads or the stores can
    // be unaligned if aligned == false.

    { // FasterArrayCopy
      __ cmpwi(CCR0, R5_ARG3, 15);
      __ ble(CCR0, l_6); // copy 2 at a time if less than 16 elements remain

      __ sri(tmp1, R5_ARG3, 4);
      __ andi_(R5_ARG3, R5_ARG3, 15);
      __ mtctr(tmp1);

      if (!VM_Version::has_vsx()) {
#ifdef USE_SPE
      // go to optimized code if to and from alignment mod 8
      __ beq(CCR1, l_opt);
#endif

      __ bind(l_8);
      // Use unrolled version for mass copying (copy 16 elements a time).
      // Load feeding store gets zero latency on Power6, however not on Power5.
      // Therefore, the following sequence is made for the good of both.
#ifdef PPC64
      __ l(tmp1, 0, R3_ARG1);
      __ l(tmp2, 8, R3_ARG1);
      __ l(tmp3, 16, R3_ARG1);
      __ l(tmp4, 24, R3_ARG1);
      __ st(tmp1, 0, R4_ARG2);
      __ st(tmp2, 8, R4_ARG2);
      __ st(tmp3, 16, R4_ARG2);
      __ st(tmp4, 24, R4_ARG2);
#else
      __ l(tmp1, 0, R3_ARG1);
      __ l(tmp2, 4, R3_ARG1);
      __ l(tmp3, 8, R3_ARG1);
      __ l(tmp4, 12, R3_ARG1);
      __ st(tmp1, 0, R4_ARG2);
      __ st(tmp2, 4, R4_ARG2);
      __ st(tmp3, 8, R4_ARG2);
      __ st(tmp4, 12, R4_ARG2);
      __ l(tmp1, 16, R3_ARG1);
      __ l(tmp2, 20, R3_ARG1);
      __ l(tmp3, 24, R3_ARG1);
      __ l(tmp4, 28, R3_ARG1);
      __ st(tmp1, 16, R4_ARG2);
      __ st(tmp2, 20, R4_ARG2);
      __ st(tmp3, 24, R4_ARG2);
      __ st(tmp4, 28, R4_ARG2);
#endif
      __ addi(R3_ARG1, R3_ARG1, 32);
      __ addi(R4_ARG2, R4_ARG2, 32);
      __ bdnz(l_8);

#ifdef USE_SPE
      __ b(l_6);

      __ bind(l_opt);

      // SPE optimization if both from and to alignment mod 8
      __ evldd(tmp1, 0, R3_ARG1);
      __ evldd(tmp2, 8, R3_ARG1);
      __ evldd(tmp3, 16, R3_ARG1);
      __ evldd(tmp4, 24, R3_ARG1);
      __ evstdd(tmp1, 0, R4_ARG2);
      __ evstdd(tmp2, 8, R4_ARG2);
      __ evstdd(tmp3, 16, R4_ARG2);
      __ evstdd(tmp4, 24, R4_ARG2);

      __ addi(R3_ARG1, R3_ARG1, 32);
      __ addi(R4_ARG2, R4_ARG2, 32);
      __ bdnz(l_opt);
#endif
     } else { // Processor supports VSX, so use it to mass copy.

        // Prefetch src data into L2 cache.
        __ dcbt(R3_ARG1, 0);

        // If supported set DSCR pre-fetch to deepest.
        if (VM_Version::has_mfdscr()) {
          __ load_const_optimized(tmp2, VM_Version::_dscr_val | 7);
          __ mtdscr(tmp2);
        }
        __ li(tmp1, 16);

        // Backbranch target aligned to 32-byte. It's not aligned 16-byte
        // as loop contains < 8 instructions that fit inside a single
        // i-cache sector.
        __ align(32);

        __ bind(l_9);
        // Use loop with VSX load/store instructions to
        // copy 16 elements a time.
        __ lxvd2x(tmp_vsr1, R3_ARG1);        // Load from src.
        __ stxvd2x(tmp_vsr1, R4_ARG2);       // Store to dst.
        __ lxvd2x(tmp_vsr2, R3_ARG1, tmp1);  // Load from src + 16.
        __ stxvd2x(tmp_vsr2, R4_ARG2, tmp1); // Store to dst + 16.
        __ addi(R3_ARG1, R3_ARG1, 32);       // Update src+=32.
        __ addi(R4_ARG2, R4_ARG2, 32);       // Update dsc+=32.
        __ bdnz(l_9);                        // Dec CTR and loop if not zero.

        // Restore DSCR pre-fetch value.
        if (VM_Version::has_mfdscr()) {
          __ load_const_optimized(tmp2, VM_Version::_dscr_val);
          __ mtdscr(tmp2);
        }

      }
    } // FasterArrayCopy
    __ bind(l_6);

    // copy 2 elements at a time
    { // FasterArrayCopy
      __ cmpwi(CCR0, R5_ARG3, 2);
      __ blt(CCR0, l_1);
      __ sri(tmp1, R5_ARG3, 1);
      __ andi_(R5_ARG3, R5_ARG3, 1);

      __ addi(R3_ARG1, R3_ARG1, -4);
      __ addi(R4_ARG2, R4_ARG2, -4);
      __ mtctr(tmp1);

      __ bind(l_3);
      __ lwzu(tmp2, 4, R3_ARG1);
      __ stwu(tmp2, 4, R4_ARG2);
      __ bdnz(l_3);

      __ addi(R3_ARG1, R3_ARG1, 4);
      __ addi(R4_ARG2, R4_ARG2, 4);
    }

    // do single element copy
    __ bind(l_1);
    __ cmpwi(CCR0, R5_ARG3, 0);
    __ beq(CCR0, l_4);

    { // FasterArrayCopy
      __ mtctr(R5_ARG3);
      __ addi(R3_ARG1, R3_ARG1, -2);
      __ addi(R4_ARG2, R4_ARG2, -2);

      __ bind(l_5);
      __ lhzu(tmp2, 2, R3_ARG1);
      __ sthu(tmp2, 2, R4_ARG2);
      __ bdnz(l_5);
    }
    __ bind(l_4);
    __ li(R3_RET, 0); // return 0
    __ blr();

    return start;
  }

  // Generate stub for conjoint short copy.  If "aligned" is true, the
  // "from" and "to" addresses are assumed to be heapword aligned.
  //
  // Arguments for generated stub:
  //      from:  R3_ARG1
  //      to:    R4_ARG2
  //      count: R5_ARG3 treated as signed
  //
  address generate_conjoint_short_copy(bool aligned, const char * name) {
    StubCodeMark mark(this, "StubRoutines", name);
    address start = __ function_entry();

    Register tmp1 = R6_ARG4;
    Register tmp2 = R7_ARG5;
    Register tmp3 = R8_ARG6;

#if !defined(PPC64) || defined(ABI_ELFv2)
    address nooverlap_target = aligned ?
        StubRoutines::arrayof_jshort_disjoint_arraycopy() :
        StubRoutines::jshort_disjoint_arraycopy();
#else
    address nooverlap_target = aligned ?
        ((FunctionDescriptor*)StubRoutines::arrayof_jshort_disjoint_arraycopy())->entry() :
        ((FunctionDescriptor*)StubRoutines::jshort_disjoint_arraycopy())->entry();
#endif

    array_overlap_test(nooverlap_target, 1);

    Label l_1, l_2;
    __ sli(tmp1, R5_ARG3, 1);
    __ b(l_2);
    __ bind(l_1);
    __ sthx(tmp2, R4_ARG2, tmp1);
    __ bind(l_2);
    __ addic_(tmp1, tmp1, -2);
    __ lhzx(tmp2, R3_ARG1, tmp1);
    __ bge(CCR0, l_1);

    __ li(R3_RET, 0); // return 0
    __ blr();

    return start;
  }

  // Generate core code for disjoint int copy (and oop copy on 32-bit).  If "aligned"
  // is true, the "from" and "to" addresses are assumed to be heapword aligned.
  //
  // Arguments:
  //      from:  R3_ARG1
  //      to:    R4_ARG2
  //      count: R5_ARG3 treated as signed
  //
  void generate_disjoint_int_copy_core(bool aligned) {
    Register tmp1 = R6_ARG4;
    Register tmp2 = R7_ARG5;
    Register tmp3 = R8_ARG6;
    Register tmp4 = R0;

    assert_positive_int(R5_ARG3);

    VectorSRegister tmp_vsr1  = VSR1;
    VectorSRegister tmp_vsr2  = VSR2;

    Label l_1, l_2, l_3, l_4, l_5, l_6, l_7, l_opt;

    // for short arrays, just do single element copy
    __ li(tmp3, 0);
    __ cmpwi(CCR0, R5_ARG3, 5);
    __ ble(CCR0, l_2);

#ifdef _LP64
    if (!aligned)
#endif
      {
        // check if arrays have same alignment mod 8.
        __ xorr(tmp1, R3_ARG1, R4_ARG2);
        __ andi_(R0, tmp1, 7);
#ifdef USE_SPE
        __ cmpwi(CCR1, tmp1, 0);
#endif
        // Not the same alignment, but l and st just need to be 4 byte aligned.
        __ bne(CCR0, l_4); // to OR from is 8 byte aligned -> copy 2 at a time

        // copy 1 element to align to and from on an 8 byte boundary
        __ andi_(R0, R3_ARG1, 7);
        __ beq(CCR0, l_4);

        __ lwzx(tmp2, R3_ARG1, tmp3);
        __ addi(R5_ARG3, R5_ARG3, -1);
        __ stwx(tmp2, R4_ARG2, tmp3);
        { // FasterArrayCopy
          __ addi(R3_ARG1, R3_ARG1, 4);
          __ addi(R4_ARG2, R4_ARG2, 4);
        }
        __ bind(l_4);
      }

    { // FasterArrayCopy
      __ cmpwi(CCR0, R5_ARG3, 7);
      __ ble(CCR0, l_2); // copy 1 at a time if less than 8 elements remain

      __ sri(tmp1, R5_ARG3, 3);
      __ andi_(R5_ARG3, R5_ARG3, 7);
      __ mtctr(tmp1);

     if (!VM_Version::has_vsx()) {
#ifdef USE_SPE
      // go to optimized code if to and from alignment mod 8
      __ beq(CCR1, l_opt);
#endif

      __ bind(l_6);
      // Use unrolled version for mass copying (copy 8 elements a time).
      // Load feeding store gets zero latency on power6, however not on power 5.
      // Therefore, the following sequence is made for the good of both.
#ifdef PPC64
      __ l(tmp1, 0, R3_ARG1);
      __ l(tmp2, 8, R3_ARG1);
      __ l(tmp3, 16, R3_ARG1);
      __ l(tmp4, 24, R3_ARG1);
      __ st(tmp1, 0, R4_ARG2);
      __ st(tmp2, 8, R4_ARG2);
      __ st(tmp3, 16, R4_ARG2);
      __ st(tmp4, 24, R4_ARG2);
#else
      __ l(tmp1, 0, R3_ARG1);
      __ l(tmp2, 4, R3_ARG1);
      __ l(tmp3, 8, R3_ARG1);
      __ l(tmp4, 12, R3_ARG1);
      __ st(tmp1, 0, R4_ARG2);
      __ st(tmp2, 4, R4_ARG2);
      __ st(tmp3, 8, R4_ARG2);
      __ st(tmp4, 12, R4_ARG2);
      __ l(tmp1, 16, R3_ARG1);
      __ l(tmp2, 20, R3_ARG1);
      __ l(tmp3, 24, R3_ARG1);
      __ l(tmp4, 28, R3_ARG1);
      __ st(tmp1, 16, R4_ARG2);
      __ st(tmp2, 20, R4_ARG2);
      __ st(tmp3, 24, R4_ARG2);
      __ st(tmp4, 28, R4_ARG2);
#endif
      __ addi(R3_ARG1, R3_ARG1, 32);
      __ addi(R4_ARG2, R4_ARG2, 32);
      __ bdnz(l_6);
#ifdef USE_SPE
      __ b(l_2);

      __ bind(l_opt);

      // SPE optimization if both from and to alignment mod 8
      __ evldd(tmp1, 0, R3_ARG1);
      __ evldd(tmp2, 8, R3_ARG1);
      __ evldd(tmp3, 16, R3_ARG1);
      __ evldd(tmp4, 24, R3_ARG1);
      __ evstdd(tmp1, 0, R4_ARG2);
      __ evstdd(tmp2, 8, R4_ARG2);
      __ evstdd(tmp3, 16, R4_ARG2);
      __ evstdd(tmp4, 24, R4_ARG2);

      __ addi(R3_ARG1, R3_ARG1, 32);
      __ addi(R4_ARG2, R4_ARG2, 32);
      __ bdnz(l_opt);
#endif

    } else { // Processor supports VSX, so use it to mass copy.

      // Prefetch the data into the L2 cache.
      __ dcbt(R3_ARG1, 0);

      // If supported set DSCR pre-fetch to deepest.
      if (VM_Version::has_mfdscr()) {
        __ load_const_optimized(tmp2, VM_Version::_dscr_val | 7);
        __ mtdscr(tmp2);
      }

      __ li(tmp1, 16);

      // Backbranch target aligned to 32-byte. Not 16-byte align as
      // loop contains < 8 instructions that fit inside a single
      // i-cache sector.
      __ align(32);

      __ bind(l_7);
      // Use loop with VSX load/store instructions to
      // copy 8 elements a time.
      __ lxvd2x(tmp_vsr1, R3_ARG1);        // Load src
      __ stxvd2x(tmp_vsr1, R4_ARG2);       // Store to dst
      __ lxvd2x(tmp_vsr2, tmp1, R3_ARG1);  // Load src + 16
      __ stxvd2x(tmp_vsr2, tmp1, R4_ARG2); // Store to dst + 16
      __ addi(R3_ARG1, R3_ARG1, 32);       // Update src+=32
      __ addi(R4_ARG2, R4_ARG2, 32);       // Update dsc+=32
      __ bdnz(l_7);                        // Dec CTR and loop if not zero.

      // Restore DSCR pre-fetch value.
      if (VM_Version::has_mfdscr()) {
        __ load_const_optimized(tmp2, VM_Version::_dscr_val);
        __ mtdscr(tmp2);
      }

    } // VSX
   } // FasterArrayCopy

    // copy 1 element at a time
    __ bind(l_2);
    __ cmpwi(CCR0, R5_ARG3, 0);
    __ beq(CCR0, l_1);

    { // FasterArrayCopy
      __ mtctr(R5_ARG3);
      __ addi(R3_ARG1, R3_ARG1, -4);
      __ addi(R4_ARG2, R4_ARG2, -4);

      __ bind(l_3);
      __ lwzu(tmp2, 4, R3_ARG1);
      __ stwu(tmp2, 4, R4_ARG2);
      __ bdnz(l_3);
    }

    __ bind(l_1);
    return;
  }

  // Generate stub for disjoint int copy.  If "aligned" is true, the
  // "from" and "to" addresses are assumed to be heapword aligned.
  //
  // Arguments for generated stub:
  //      from:  R3_ARG1
  //      to:    R4_ARG2
  //      count: R5_ARG3 treated as signed
  //
  address generate_disjoint_int_copy(bool aligned, const char * name) {
    StubCodeMark mark(this, "StubRoutines", name);
    address start = __ function_entry();
    generate_disjoint_int_copy_core(aligned);
    __ li(R3_RET, 0); // return 0
    __ blr();
    return start;
  }

  // Generate core code for conjoint int copy (and oop copy on
  // 32-bit).  If "aligned" is true, the "from" and "to" addresses
  // are assumed to be heapword aligned.
  //
  // Arguments:
  //      from:  R3_ARG1
  //      to:    R4_ARG2
  //      count: R5_ARG3 treated as signed
  //
  void generate_conjoint_int_copy_core(bool aligned) {
    // Do reverse copy.  We assume the case of actual overlap is rare enough
    // that we don't have to optimize it.

    Label l_1, l_2, l_3, l_4, l_5, l_6, l_7, l_opt;

    Register tmp1 = R6_ARG4;
    Register tmp2 = R7_ARG5;
    Register tmp3 = R8_ARG6;
    Register tmp4 = R0;

    VectorSRegister tmp_vsr1  = VSR1;
    VectorSRegister tmp_vsr2  = VSR2;

    { // FasterArrayCopy
      __ cmpwi(CCR0, R5_ARG3, 0);
      __ beq(CCR0, l_6);

      __ sli(R5_ARG3, R5_ARG3, 2);
      __ add(R3_ARG1, R3_ARG1, R5_ARG3);
      __ add(R4_ARG2, R4_ARG2, R5_ARG3);
      __ sri(R5_ARG3, R5_ARG3, 2);

      if (!aligned) {
        // check if arrays have same alignment mod 8.
        __ xorr(tmp1, R3_ARG1, R4_ARG2);
        __ andi_(R0, tmp1, 7);
        // Not the same alignment, but ld and std just need to be 4 byte aligned.
        __ bne(CCR0, l_1); // to OR from is 8 byte aligned -> copy 2 at a time

        // copy 1 element to align to and from on an 8 byte boundary
        __ andi_(R0, R3_ARG1, 7);
        __ beq(CCR0, l_1);

        __ addi(R3_ARG1, R3_ARG1, -4);
        __ addi(R4_ARG2, R4_ARG2, -4);
        __ addi(R5_ARG3, R5_ARG3, -1);
        __ lwzx(tmp2, R3_ARG1);
        __ stwx(tmp2, R4_ARG2);
        __ bind(l_1);
      }

      __ cmpwi(CCR0, R5_ARG3, 7);
      __ ble(CCR0, l_5); // copy 1 at a time if less than 8 elements remain

      __ sri(tmp1, R5_ARG3, 3);
      __ andi(R5_ARG3, R5_ARG3, 7);
      __ mtctr(tmp1);

     if (!VM_Version::has_vsx()) {
#ifdef USE_SPE
      // Align to 8 bytes, but only if both, from and to, have same alignment mod 8.
      __ andi_(R0, R3_ARG1, 7);
      __ bne(CCR0, l_4);
      __ andi_(R0, R4_ARG2, 7);
      __ bne(CCR0, l_4);
      __ b(l_opt);
#endif
      __ bind(l_4);
      // Use unrolled version for mass copying (copy 4 elements a time).
      // Load feeding store gets zero latency on Power6, however not on Power5.
      // Therefore, the following sequence is made for the good of both.
      __ addi(R3_ARG1, R3_ARG1, -32);
      __ addi(R4_ARG2, R4_ARG2, -32);
#ifdef PPC64
      __ l(tmp4, 24, R3_ARG1);
      __ l(tmp3, 16, R3_ARG1);
      __ l(tmp2, 8, R3_ARG1);
      __ l(tmp1, 0, R3_ARG1);
      __ st(tmp4, 24, R4_ARG2);
      __ st(tmp3, 16, R4_ARG2);
      __ st(tmp2, 8, R4_ARG2);
      __ st(tmp1, 0, R4_ARG2);
#else
      __ l(tmp4, 28, R3_ARG1);
      __ l(tmp3, 24, R3_ARG1);
      __ l(tmp2, 20, R3_ARG1);
      __ l(tmp1, 16, R3_ARG1);
      __ st(tmp4, 28, R4_ARG2);
      __ st(tmp3, 24, R4_ARG2);
      __ st(tmp2, 20, R4_ARG2);
      __ st(tmp1, 16, R4_ARG2);
      __ l(tmp4, 12, R3_ARG1);
      __ l(tmp3, 8, R3_ARG1);
      __ l(tmp2, 4, R3_ARG1);
      __ l(tmp1, 0, R3_ARG1);
      __ st(tmp4, 12, R4_ARG2);
      __ st(tmp3, 8, R4_ARG2);
      __ st(tmp2, 4, R4_ARG2);
      __ st(tmp1, 0, R4_ARG2);
#endif
      __ bdnz(l_4);
#ifdef USE_SPE
      __ b(l_7);

      __ bind(l_opt);

      __ addi(R3_ARG1, R3_ARG1, -32);
      __ addi(R4_ARG2, R4_ARG2, -32);
      // SPE optimization if both from and to alignment mod 8
      __ evldd(tmp1, 24, R3_ARG1);
      __ evldd(tmp2, 16, R3_ARG1);
      __ evldd(tmp3, 8, R3_ARG1);
      __ evldd(tmp4, 0, R3_ARG1);
      __ evstdd(tmp1, 24, R4_ARG2);
      __ evstdd(tmp2, 16, R4_ARG2);
      __ evstdd(tmp3, 8, R4_ARG2);
      __ evstdd(tmp4, 0, R4_ARG2);

      __ bdnz(l_opt);

      __ bind(l_7);
#endif
     } else {  // Processor supports VSX, so use it to mass copy.
      // Prefetch the data into the L2 cache.
      __ dcbt(R3_ARG1, 0);

      // If supported set DSCR pre-fetch to deepest.
      if (VM_Version::has_mfdscr()) {
        __ load_const_optimized(tmp2, VM_Version::_dscr_val | 7);
        __ mtdscr(tmp2);
      }

      __ li(tmp1, 16);

      // Backbranch target aligned to 32-byte. Not 16-byte align as
      // loop contains < 8 instructions that fit inside a single
      // i-cache sector.
      __ align(32);

      __ bind(l_4);
      // Use loop with VSX load/store instructions to
      // copy 8 elements a time.
      __ addi(R3_ARG1, R3_ARG1, -32);      // Update src-=32
      __ addi(R4_ARG2, R4_ARG2, -32);      // Update dsc-=32
      __ lxvd2x(tmp_vsr2, tmp1, R3_ARG1);  // Load src+16
      __ lxvd2x(tmp_vsr1, R3_ARG1);        // Load src
      __ stxvd2x(tmp_vsr2, tmp1, R4_ARG2); // Store to dst+16
      __ stxvd2x(tmp_vsr1, R4_ARG2);       // Store to dst
      __ bdnz(l_4);

      // Restore DSCR pre-fetch value.
      if (VM_Version::has_mfdscr()) {
        __ load_const_optimized(tmp2, VM_Version::_dscr_val);
        __ mtdscr(tmp2);
      }
     }

      __ cmpwi(CCR0, R5_ARG3, 0);
      __ beq(CCR0, l_6);

      __ bind(l_5);
      __ mtctr(R5_ARG3);
      __ bind(l_3);
      __ lwz(R0, -4, R3_ARG1);
      __ stw(R0, -4, R4_ARG2);
      __ addi(R3_ARG1, R3_ARG1, -4);
      __ addi(R4_ARG2, R4_ARG2, -4);
      __ bdnz(l_3);

      __ bind(l_6);
    }
  }

  // Generate stub for conjoint int copy.  If "aligned" is true, the
  // "from" and "to" addresses are assumed to be heapword aligned.
  //
  // Arguments for generated stub:
  //      from:  R3_ARG1
  //      to:    R4_ARG2
  //      count: R5_ARG3 treated as signed
  //
  address generate_conjoint_int_copy(bool aligned, const char * name) {
    StubCodeMark mark(this, "StubRoutines", name);
    address start = __ function_entry();

#if !defined(PPC64) || defined(ABI_ELFv2)
    address nooverlap_target = aligned ?
      StubRoutines::arrayof_jint_disjoint_arraycopy() :
      StubRoutines::jint_disjoint_arraycopy();
#else
    address nooverlap_target = aligned ?
      ((FunctionDescriptor*)StubRoutines::arrayof_jint_disjoint_arraycopy())->entry() :
      ((FunctionDescriptor*)StubRoutines::jint_disjoint_arraycopy())->entry();
#endif

    array_overlap_test(nooverlap_target, 2);

    generate_conjoint_int_copy_core(aligned);

    __ li(R3_RET, 0); // return 0
    __ blr();

    return start;
  }

  // Generate core code for disjoint long copy (and oop copy on
  // 64-bit).  If "aligned" is true, the "from" and "to" addresses
  // are assumed to be heapword aligned.
  //
  // Arguments:
  //      from:  R3_ARG1
  //      to:    R4_ARG2
  //      count: R5_ARG3 treated as signed
  //
  void generate_disjoint_long_copy_core(bool aligned) {
    Register tmp1 = R6_ARG4;
    Register tmp2 = R7_ARG5;
    Register tmp3 = R8_ARG6;
    Register tmp4 = R0;

    Label l_1, l_2, l_3, l_4, l_5, l_opt;

    VectorSRegister tmp_vsr1  = VSR1;
    VectorSRegister tmp_vsr2  = VSR2;

    { // FasterArrayCopy
      __ cmpwi(CCR0, R5_ARG3, 3);
      __ ble(CCR0, l_3); // copy 1 at a time if less than 4 elements remain

      __ sri(tmp1, R5_ARG3, 2);
      __ andi_(R5_ARG3, R5_ARG3, 3);
      __ mtctr(tmp1);
#ifdef USE_SPE
      // Align to 8 bytes, but only if both, from and to, have same alignment mod 8.
      __ andi_(R0, R3_ARG1, 7);
      __ bne(CCR0, l_4);
      __ andi_(R0, R4_ARG2, 7);
      __ bne(CCR0, l_4);
      __ b(l_opt);
#endif

    if (!VM_Version::has_vsx()) {
      __ bind(l_4);
      // Use unrolled version for mass copying (copy 4 elements a time).
      // Load feeding store gets zero latency on Power6, however not on Power5.
      // Therefore, the following sequence is made for the good of both.
#ifdef PPC64
      __ l(tmp1, 0, R3_ARG1);
      __ l(tmp2, 8, R3_ARG1);
      __ l(tmp3, 16, R3_ARG1);
      __ l(tmp4, 24, R3_ARG1);
      __ st(tmp1, 0, R4_ARG2);
      __ st(tmp2, 8, R4_ARG2);
      __ st(tmp3, 16, R4_ARG2);
      __ st(tmp4, 24, R4_ARG2);
#else
      __ l(tmp1, 0, R3_ARG1);
      __ l(tmp2, 4, R3_ARG1);
      __ l(tmp3, 8, R3_ARG1);
      __ l(tmp4, 12, R3_ARG1);
      __ st(tmp1, 0, R4_ARG2);
      __ st(tmp2, 4, R4_ARG2);
      __ st(tmp3, 8, R4_ARG2);
      __ st(tmp4, 12, R4_ARG2);
      __ l(tmp1, 16, R3_ARG1);
      __ l(tmp2, 20, R3_ARG1);
      __ l(tmp3, 24, R3_ARG1);
      __ l(tmp4, 28, R3_ARG1);
      __ st(tmp1, 16, R4_ARG2);
      __ st(tmp2, 20, R4_ARG2);
      __ st(tmp3, 24, R4_ARG2);
      __ st(tmp4, 28, R4_ARG2);
#endif
      __ addi(R3_ARG1, R3_ARG1, 32);
      __ addi(R4_ARG2, R4_ARG2, 32);
      __ bdnz(l_4);
#ifdef USE_SPE
      __ b(l_3);

      __ bind(l_opt);

      // SPE optimization if both from and to alignment mod 8
      __ evldd(tmp1, 0, R3_ARG1);
      __ evldd(tmp2, 8, R3_ARG1);
      __ evldd(tmp3, 16, R3_ARG1);
      __ evldd(tmp4, 24, R3_ARG1);
      __ evstdd(tmp1, 0, R4_ARG2);
      __ evstdd(tmp2, 8, R4_ARG2);
      __ evstdd(tmp3, 16, R4_ARG2);
      __ evstdd(tmp4, 24, R4_ARG2);

      __ addi(R3_ARG1, R3_ARG1, 32);
      __ addi(R4_ARG2, R4_ARG2, 32);
      __ bdnz(l_opt);

      __ bind(l_3);
#endif


    } else { // Processor supports VSX, so use it to mass copy.

      // Prefetch the data into the L2 cache.
      __ dcbt(R3_ARG1, 0);

      // If supported set DSCR pre-fetch to deepest.
      if (VM_Version::has_mfdscr()) {
        __ load_const_optimized(tmp2, VM_Version::_dscr_val | 7);
        __ mtdscr(tmp2);
      }

      __ li(tmp1, 16);

      // Backbranch target aligned to 32-byte. Not 16-byte align as
      // loop contains < 8 instructions that fit inside a single
      // i-cache sector.
      __ align(32);

      __ bind(l_5);
      // Use loop with VSX load/store instructions to
      // copy 4 elements a time.
      __ lxvd2x(tmp_vsr1, R3_ARG1);        // Load src
      __ stxvd2x(tmp_vsr1, R4_ARG2);       // Store to dst
      __ lxvd2x(tmp_vsr2, tmp1, R3_ARG1);  // Load src + 16
      __ stxvd2x(tmp_vsr2, tmp1, R4_ARG2); // Store to dst + 16
      __ addi(R3_ARG1, R3_ARG1, 32);       // Update src+=32
      __ addi(R4_ARG2, R4_ARG2, 32);       // Update dsc+=32
      __ bdnz(l_5);                        // Dec CTR and loop if not zero.

      // Restore DSCR pre-fetch value.
      if (VM_Version::has_mfdscr()) {
        __ load_const_optimized(tmp2, VM_Version::_dscr_val);
        __ mtdscr(tmp2);
      }

    } // VSX
   } // FasterArrayCopy

    // copy 1 element at a time
    __ bind(l_3);
    __ cmpwi(CCR0, R5_ARG3, 0);
    __ beq(CCR0, l_1);

    { // FasterArrayCopy
      __ mtctr(R5_ARG3);
      __ addi(R3_ARG1, R3_ARG1, -8);
      __ addi(R4_ARG2, R4_ARG2, -8);

      __ bind(l_2);
#ifdef PPC64
      __ lu(R0, 8, R3_ARG1);
      __ stu(R0, 8, R4_ARG2);
#else
      __ l(R0,  12, R3_ARG1);
      __ st(R0, 12 , R4_ARG2);
      __ lu(R0,  8, R3_ARG1);
      __ stu(R0, 8, R4_ARG2);
#endif
      __ bdnz(l_2);

    }
    __ bind(l_1);
  }

  // Generate stub for disjoint long copy.  If "aligned" is true, the
  // "from" and "to" addresses are assumed to be heapword aligned.
  //
  // Arguments for generated stub:
  //      from:  R3_ARG1
  //      to:    R4_ARG2
  //      count: R5_ARG3 treated as signed
  //
  address generate_disjoint_long_copy(bool aligned, const char * name) {
    StubCodeMark mark(this, "StubRoutines", name);
    address start = __ function_entry();
    assert_positive_int(R5_ARG3);
    generate_disjoint_long_copy_core(aligned);
    __ li(R3_RET, 0); // return 0
    __ blr();

    return start;
  }

  // Generate core code for conjoint long copy (and oop copy on
  // 64-bit).  If "aligned" is true, the "from" and "to" addresses
  // are assumed to be heapword aligned.
  //
  // Arguments:
  //      from:  R3_ARG1
  //      to:    R4_ARG2
  //      count: R5_ARG3 treated as signed
  //
  void generate_conjoint_long_copy_core(bool aligned) {
    Register tmp1 = R6_ARG4;
    Register tmp2 = R7_ARG5;
    Register tmp3 = R8_ARG6;
    Register tmp4 = R0;

    VectorSRegister tmp_vsr1  = VSR1;
    VectorSRegister tmp_vsr2  = VSR2;

    Label l_1, l_2, l_3, l_4, l_5, l_6, l_opt;


    __ cmpwi(CCR0, R5_ARG3, 0);
    __ beq(CCR0, l_1);

    { // FasterArrayCopy
      __ sli(R5_ARG3, R5_ARG3, 3);
      __ add(R3_ARG1, R3_ARG1, R5_ARG3);
      __ add(R4_ARG2, R4_ARG2, R5_ARG3);
      __ sri(R5_ARG3, R5_ARG3, 3);

      __ cmpwi(CCR0, R5_ARG3, 3);
      __ ble(CCR0, l_5); // copy 1 at a time if less than 4 elements remain

      __ sri(tmp1, R5_ARG3, 2);
      __ andi(R5_ARG3, R5_ARG3, 3);
      __ mtctr(tmp1);

     if (!VM_Version::has_vsx()) {
#ifdef USE_SPE
      // Align to 8 bytes, but only if both, from and to, have same alignment mod 8.
      __ andi_(R0, R3_ARG1, 7);
      __ bne(CCR0, l_4);
      __ andi_(R0, R4_ARG2, 7);
      __ bne(CCR0, l_4);
      __ b(l_opt);
#endif

      __ bind(l_4);
      // Use unrolled version for mass copying (copy 4 elements a time).
      // Load feeding store gets zero latency on Power6, however not on Power5.
      // Therefore, the following sequence is made for the good of both.
      __ addi(R3_ARG1, R3_ARG1, -32);
      __ addi(R4_ARG2, R4_ARG2, -32);
#ifdef PPC64
      __ l(tmp4, 24, R3_ARG1);
      __ l(tmp3, 16, R3_ARG1);
      __ l(tmp2, 8, R3_ARG1);
      __ l(tmp1, 0, R3_ARG1);
      __ st(tmp4, 24, R4_ARG2);
      __ st(tmp3, 16, R4_ARG2);
      __ st(tmp2, 8, R4_ARG2);
      __ st(tmp1, 0, R4_ARG2);
#else
      __ l(tmp4,  28, R3_ARG1);
      __ l(tmp3,  24, R3_ARG1);
      __ l(tmp2,  20, R3_ARG1);
      __ l(tmp1,  16, R3_ARG1);
      __ st(tmp4, 28, R4_ARG2);
      __ st(tmp3, 24, R4_ARG2);
      __ st(tmp2, 20, R4_ARG2);
      __ st(tmp1, 16, R4_ARG2);
      __ l(tmp4,  12, R3_ARG1);
      __ l(tmp3,  8, R3_ARG1);
      __ l(tmp2,  4, R3_ARG1);
      __ l(tmp1,  0, R3_ARG1);
      __ st(tmp4, 12, R4_ARG2);
      __ st(tmp3, 8, R4_ARG2);
      __ st(tmp2, 4, R4_ARG2);
      __ st(tmp1, 0, R4_ARG2);
#endif
      __ bdnz(l_4);
#ifdef USE_SPE
      __ b(l_6);

      __ bind(l_opt);

      __ addi(R3_ARG1, R3_ARG1, -32);
      __ addi(R4_ARG2, R4_ARG2, -32);
      // SPE optimization if both from and to alignment mod 8
      __ evldd(tmp1, 24, R3_ARG1);
      __ evldd(tmp2, 16, R3_ARG1);
      __ evldd(tmp3, 8, R3_ARG1);
      __ evldd(tmp4, 0, R3_ARG1);
      __ evstdd(tmp1, 24, R4_ARG2);
      __ evstdd(tmp2, 16, R4_ARG2);
      __ evstdd(tmp3, 8, R4_ARG2);
      __ evstdd(tmp4, 0, R4_ARG2);

      __ bdnz(l_opt);

      __ bind(l_6);
#endif
     } else { // Processor supports VSX, so use it to mass copy.
      // Prefetch the data into the L2 cache.
      __ dcbt(R3_ARG1, 0);

      // If supported set DSCR pre-fetch to deepest.
      if (VM_Version::has_mfdscr()) {
        __ load_const_optimized(tmp2, VM_Version::_dscr_val | 7);
        __ mtdscr(tmp2);
      }

      __ li(tmp1, 16);

      // Backbranch target aligned to 32-byte. Not 16-byte align as
      // loop contains < 8 instructions that fit inside a single
      // i-cache sector.
      __ align(32);

      __ bind(l_4);
      // Use loop with VSX load/store instructions to
      // copy 4 elements a time.
      __ addi(R3_ARG1, R3_ARG1, -32);      // Update src-=32
      __ addi(R4_ARG2, R4_ARG2, -32);      // Update dsc-=32
      __ lxvd2x(tmp_vsr2, tmp1, R3_ARG1);  // Load src+16
      __ lxvd2x(tmp_vsr1, R3_ARG1);        // Load src
      __ stxvd2x(tmp_vsr2, tmp1, R4_ARG2); // Store to dst+16
      __ stxvd2x(tmp_vsr1, R4_ARG2);       // Store to dst
      __ bdnz(l_4);

      // Restore DSCR pre-fetch value.
      if (VM_Version::has_mfdscr()) {
        __ load_const_optimized(tmp2, VM_Version::_dscr_val);
        __ mtdscr(tmp2);
      }
     }

      __ cmpwi(CCR0, R5_ARG3, 0);
      __ beq(CCR0, l_1);

      __ bind(l_5);
      __ mtctr(R5_ARG3);
      __ bind(l_3);
#ifdef PPC64
      __ l(R0, -8, R3_ARG1);
      __ st(R0, -8, R4_ARG2);
#else
      __ l(R0,  -8, R3_ARG1);
      __ st(R0, -8, R4_ARG2);
      __ l(R0,  -4, R3_ARG1);
      __ st(R0, -4, R4_ARG2);
#endif
      __ addi(R3_ARG1, R3_ARG1, -8);
      __ addi(R4_ARG2, R4_ARG2, -8);
      __ bdnz(l_3);

    }
    __ bind(l_1);
  }

  // Generate stub for conjoint long copy.  If "aligned" is true, the
  // "from" and "to" addresses are assumed to be heapword aligned.
  //
  // Arguments for generated stub:
  //      from:  R3_ARG1
  //      to:    R4_ARG2
  //      count: R5_ARG3 treated as signed
  //
  address generate_conjoint_long_copy(bool aligned, const char * name) {
    StubCodeMark mark(this, "StubRoutines", name);
    address start = __ function_entry();

#if !defined(PPC64) || defined(ABI_ELFv2)
    address nooverlap_target = aligned ?
      StubRoutines::arrayof_jlong_disjoint_arraycopy() :
      StubRoutines::jlong_disjoint_arraycopy();
#else
    address nooverlap_target = aligned ?
      ((FunctionDescriptor*)StubRoutines::arrayof_jlong_disjoint_arraycopy())->entry() :
      ((FunctionDescriptor*)StubRoutines::jlong_disjoint_arraycopy())->entry();
#endif

    array_overlap_test(nooverlap_target, 3);
    generate_conjoint_long_copy_core(aligned);

    __ li(R3_RET, 0); // return 0
    __ blr();

    return start;
  }

  // Generate stub for conjoint oop copy.  If "aligned" is true, the
  // "from" and "to" addresses are assumed to be heapword aligned.
  //
  // Arguments for generated stub:
  //      from:  R3_ARG1
  //      to:    R4_ARG2
  //      count: R5_ARG3 treated as signed
  //      dest_uninitialized: G1 support
  //
  address generate_conjoint_oop_copy(bool aligned, const char * name, bool dest_uninitialized) {
    StubCodeMark mark(this, "StubRoutines", name);

    address start = __ function_entry();

#if !defined(PPC64) || defined(ABI_ELFv2)
    address nooverlap_target = aligned ?
      StubRoutines::arrayof_oop_disjoint_arraycopy() :
      StubRoutines::oop_disjoint_arraycopy();
#else
    address nooverlap_target = aligned ?
      ((FunctionDescriptor*)StubRoutines::arrayof_oop_disjoint_arraycopy())->entry() :
      ((FunctionDescriptor*)StubRoutines::oop_disjoint_arraycopy())->entry();
#endif

    gen_write_ref_array_pre_barrier(R3_ARG1, R4_ARG2, R5_ARG3, dest_uninitialized, R9_ARG7);

    // Save arguments.
    __ mr(R9_ARG7, R4_ARG2);
    __ mr(R10_ARG8, R5_ARG3);

#if defined(PPC64)
    if (UseCompressedOops) {
      array_overlap_test(nooverlap_target, 2);
      generate_conjoint_int_copy_core(aligned);
    } else {
      array_overlap_test(nooverlap_target, 3);
      generate_conjoint_long_copy_core(aligned);
    }
#else
      array_overlap_test(nooverlap_target, 2);
      generate_conjoint_int_copy_core(aligned);
#endif

    gen_write_ref_array_post_barrier(R9_ARG7, R10_ARG8, R11_scratch1);
    __ li(R3_RET, 0); // return 0
    __ blr();
    return start;
  }

  // Generate stub for disjoint oop copy.  If "aligned" is true, the
  // "from" and "to" addresses are assumed to be heapword aligned.
  //
  // Arguments for generated stub:
  //      from:  R3_ARG1
  //      to:    R4_ARG2
  //      count: R5_ARG3 treated as signed
  //      dest_uninitialized: G1 support
  //
  address generate_disjoint_oop_copy(bool aligned, const char * name, bool dest_uninitialized) {
    StubCodeMark mark(this, "StubRoutines", name);
    address start = __ function_entry();

    gen_write_ref_array_pre_barrier(R3_ARG1, R4_ARG2, R5_ARG3, dest_uninitialized, R9_ARG7);

    // save some arguments, disjoint_long_copy_core destroys them.
    // needed for post barrier
    __ mr(R9_ARG7, R4_ARG2);
    __ mr(R10_ARG8, R5_ARG3);

#ifdef PPC64
    if (UseCompressedOops) {
      generate_disjoint_int_copy_core(aligned);
    } else {
      generate_disjoint_long_copy_core(aligned);
    }
#else
    generate_disjoint_int_copy_core(aligned);
#endif

    gen_write_ref_array_post_barrier(R9_ARG7, R10_ARG8, R11_scratch1);
    __ li(R3_RET, 0); // return 0
    __ blr();

    return start;
  }


  // Helper for generating a dynamic type check.
  // Smashes only the given temp registers.
  void generate_type_check(Register sub_klass,
                           Register super_check_offset,
                           Register super_klass,
                           Register temp,
                           Label& L_success) {
    assert_different_registers(sub_klass, super_check_offset, super_klass);

    BLOCK_COMMENT("type_check:");

    Label L_miss;

    __ check_klass_subtype_fast_path(sub_klass, super_klass, temp, R0, &L_success, &L_miss, NULL,
                                     super_check_offset);
    __ check_klass_subtype_slow_path(sub_klass, super_klass, temp, R0, &L_success, NULL);

    // Fall through on failure!
    __ bind(L_miss);
  }


  //  Generate stub for checked oop copy.
  //
  // Arguments for generated stub:
  //      from:  R3
  //      to:    R4
  //      count: R5 treated as signed
  //      ckoff: R6 (super_check_offset)
  //      ckval: R7 (super_klass)
  //      ret:   R3 zero for success; (-1^K) where K is partial transfer count
  //
  address generate_checkcast_copy(const char *name, bool dest_uninitialized) {

    const Register R3_from   = R3_ARG1;      // source array address
    const Register R4_to     = R4_ARG2;      // destination array address
    const Register R5_count  = R5_ARG3;      // elements count
    const Register R6_ckoff  = R6_ARG4;      // super_check_offset
    const Register R7_ckval  = R7_ARG5;      // super_klass

    const Register R8_offset = R8_ARG6;      // loop var, with stride wordSize
    const Register R9_remain = R9_ARG7;      // loop var, with stride -1
    const Register R10_oop   = R10_ARG8;     // actual oop copied
    const Register R11_klass = R11_scratch1; // oop._klass
    const Register R12_tmp   = R12_scratch2;
#ifdef PPC64
    const Register R2_minus1 = R2;
#endif
    //__ align(CodeEntryAlignment);
    StubCodeMark mark(this, "StubRoutines", name);
    address start = __ function_entry();

    // Assert that int is 64 bit sign extended and arrays are not conjoint.
#ifdef ASSERT
    {
    assert_positive_int(R5_ARG3);
    const Register tmp1 = R11_scratch1, tmp2 = R12_scratch2;
    Label no_overlap;
    __ subf(tmp1, R3_ARG1, R4_ARG2); // distance in bytes
    __ sli(tmp2, R5_ARG3, 2); // size in bytes
    __ cmpl(CCR0, R3_ARG1, R4_ARG2); // Use unsigned comparison!
    __ cmpl(CCR1, tmp1, tmp2); // unsigned cmp too, so negative values
                               // (to is below from) will be forward copied,
                               // even if overlaps (strictly speaking)
    __ crnand(CCR0, Assembler::less, CCR1, Assembler::less);
    // Overlaps if Src before dst and distance smaller than size.
    // Branch to forward copy routine otherwise (within range of 32kB).
    __ blt(CCR0, no_overlap);
    __ stop("overlap in checkcast_copy", 0x9543);
    __ bind(no_overlap);
    }
#endif

    gen_write_ref_array_pre_barrier(R3_from, R4_to, R5_count, dest_uninitialized, R12_tmp, /* preserve: */ R6_ckoff, R7_ckval);

    //inc_counter_np(SharedRuntime::_checkcast_array_copy_ctr, R12_tmp, R3_RET);

    Label load_element, store_element, store_null, success, do_card_marks;
    __ or_(R9_remain, R5_count, R5_count); // Initialize loop index, and test it.
    __ li(R8_offset, 0);                   // Offset from start of arrays.
#ifdef PPC64
    __ li(R2_minus1, -1);
#endif
    __ bne(CCR0, load_element);

    // Empty array: Nothing to do.
    __ li(R3_RET, 0);           // Return 0 on (trivial) success.
    __ blr();

    // ======== begin loop ========
    // (Entry is load_element.)
    __ align(OptoLoopAlignment);
    __ bind(store_element);
    if (UseCompressedOops) {
      __ encode_heap_oop_not_null(R10_oop);
      __ bind(store_null);
      __ stw(R10_oop, R8_offset, R4_to);
    } else {
      __ bind(store_null);
      __ st(R10_oop, R8_offset, R4_to);
    }

    __ addi(R8_offset, R8_offset, heapOopSize);   // Step to next offset.
#ifdef PPC64
    __ add_(R9_remain, R2_minus1, R9_remain);     // Decrement the count.
#else
    __ addic_(R9_remain, R9_remain, -1);     // Decrement the count.
#endif
    __ beq(CCR0, success);

    // ======== loop entry is here ========
    __ bind(load_element);
    __ load_heap_oop(R10_oop, R8_offset, R3_from, &store_null);  // Load the oop.

    __ load_klass(R11_klass, R10_oop); // Query the object klass.

    generate_type_check(R11_klass, R6_ckoff, R7_ckval, R12_tmp,
                        // Branch to this on success:
                        store_element);
    // ======== end loop ========

    // It was a real error; we must depend on the caller to finish the job.
    // Register R9_remain has number of *remaining* oops, R5_count number of *total* oops.
    // Emit GC store barriers for the oops we have copied (R5_count minus R9_remain),
    // and report their number to the caller.
    __ subf_(R5_count, R9_remain, R5_count);
    __ nand(R3_RET, R5_count, R5_count);   // report (-1^K) to caller
    __ bne(CCR0, do_card_marks);
    __ blr();

    __ bind(success);
    __ li(R3_RET, 0);

    __ bind(do_card_marks);
    // Store check on R4_to[0..R5_count-1].
    gen_write_ref_array_post_barrier(R4_to, R5_count, R12_tmp, /* preserve: */ R3_RET);
    __ blr();
    return start;
  }


  //  Generate 'unsafe' array copy stub.
  //  Though just as safe as the other stubs, it takes an unscaled
  //  size_t argument instead of an element count.
  //
  // Arguments for generated stub:
  //      from:  R3
  //      to:    R4
  //      count: R5 byte count, treated as ssize_t, can be zero
  //
  // Examines the alignment of the operands and dispatches
  // to a long, int, short, or byte copy loop.
  //
  address generate_unsafe_copy(const char* name,
                               address byte_copy_entry,
                               address short_copy_entry,
                               address int_copy_entry,
                               address long_copy_entry) {

    const Register R3_from   = R3_ARG1;      // source array address
    const Register R4_to     = R4_ARG2;      // destination array address
    const Register R5_count  = R5_ARG3;      // elements count (as long on PPC64)

    const Register R6_bits   = R6_ARG4;      // test copy of low bits
    const Register R7_tmp    = R7_ARG5;

    //__ align(CodeEntryAlignment);
    StubCodeMark mark(this, "StubRoutines", name);
    address start = __ function_entry();

    // Bump this on entry, not on exit:
    //inc_counter_np(SharedRuntime::_unsafe_array_copy_ctr, R6_bits, R7_tmp);

    Label short_copy, int_copy, long_copy;

    __ orr(R6_bits, R3_from, R4_to);
    __ orr(R6_bits, R6_bits, R5_count);
    __ andi_(R0, R6_bits, (BytesPerLong-1));
    __ beq(CCR0, long_copy);

    __ andi_(R0, R6_bits, (BytesPerInt-1));
    __ beq(CCR0, int_copy);

    __ andi_(R0, R6_bits, (BytesPerShort-1));
    __ beq(CCR0, short_copy);

    // byte_copy:
    __ b(byte_copy_entry);

    __ bind(short_copy);
    __ srwi(R5_count, R5_count, LogBytesPerShort);
    __ b(short_copy_entry);

    __ bind(int_copy);
    __ srwi(R5_count, R5_count, LogBytesPerInt);
    __ b(int_copy_entry);

    __ bind(long_copy);
    __ srwi(R5_count, R5_count, LogBytesPerLong);
    __ b(long_copy_entry);

    return start;
  }


  // Perform range checks on the proposed arraycopy.
  // Kills the two temps, but nothing else.
  // Also, clean the sign bits of src_pos and dst_pos.
  void arraycopy_range_checks(Register src,     // source array oop
                              Register src_pos, // source position
                              Register dst,     // destination array oop
                              Register dst_pos, // destination position
                              Register length,  // length of copy
                              Register temp1, Register temp2,
                              Label& L_failed) {
    BLOCK_COMMENT("arraycopy_range_checks:");

    const Register array_length = temp1;  // scratch
    const Register end_pos      = temp2;  // scratch

    //  if (src_pos + length > arrayOop(src)->length() ) FAIL;
    __ lwa(array_length, arrayOopDesc::length_offset_in_bytes(), src);
    __ add(end_pos, src_pos, length);  // src_pos + length
    __ cmp(CCR0, end_pos, array_length);
    __ bgt(CCR0, L_failed);

    //  if (dst_pos + length > arrayOop(dst)->length() ) FAIL;
    __ lwa(array_length, arrayOopDesc::length_offset_in_bytes(), dst);
    __ add(end_pos, dst_pos, length);  // src_pos + length
    __ cmp(CCR0, end_pos, array_length);
    __ bgt(CCR0, L_failed);

    BLOCK_COMMENT("arraycopy_range_checks done");
  }


  //
  //  Generate generic array copy stubs
  //
  //  Input:
  //    R3    -  src oop
  //    R4    -  src_pos
  //    R5    -  dst oop
  //    R6    -  dst_pos
  //    R7    -  element count
  //
  //  Output:
  //    R3 ==  0  -  success
  //    R3 == -1  -  need to call System.arraycopy
  //
  address generate_generic_copy(const char *name,
                                address entry_jbyte_arraycopy,
                                address entry_jshort_arraycopy,
                                address entry_jint_arraycopy,
                                address entry_oop_arraycopy,
                                address entry_disjoint_oop_arraycopy,
                                address entry_jlong_arraycopy,
                                address entry_checkcast_arraycopy) {
    Label L_failed, L_objArray;

    // Input registers
    const Register src       = R3_ARG1;  // source array oop
    const Register src_pos   = R4_ARG2;  // source position
    const Register dst       = R5_ARG3;  // destination array oop
    const Register dst_pos   = R6_ARG4;  // destination position
    const Register length    = R7_ARG5;  // elements count

    // registers used as temp
    const Register src_klass = R8_ARG6;  // source array klass
    const Register dst_klass = R9_ARG7;  // destination array klass
    const Register lh        = R10_ARG8; // layout handler
    const Register temp      = PPC64_ONLY(R2) NOT_PPC64(R11);

    //__ align(CodeEntryAlignment);
    StubCodeMark mark(this, "StubRoutines", name);
    address start = __ function_entry();

    // Bump this on entry, not on exit:
    //inc_counter_np(SharedRuntime::_generic_array_copy_ctr, lh, temp);

    // In principle, the int arguments could be dirty.

    //-----------------------------------------------------------------------
    // Assembler stubs will be used for this call to arraycopy
    // if the following conditions are met:
    //
    // (1) src and dst must not be null.
    // (2) src_pos must not be negative.
    // (3) dst_pos must not be negative.
    // (4) length  must not be negative.
    // (5) src klass and dst klass should be the same and not NULL.
    // (6) src and dst should be arrays.
    // (7) src_pos + length must not exceed length of src.
    // (8) dst_pos + length must not exceed length of dst.
    BLOCK_COMMENT("arraycopy initial argument checks");
    __ cmpi(CCR1, src, 0);      // if (src == NULL) return -1;
#ifdef PPC64
    __ extsw_(src_pos, src_pos); // if (src_pos < 0) return -1;
#else
    __ or_(temp, src_pos, src_pos); // if (src_pos < 0) return -1;
#endif
    __ cmpi(CCR5, dst, 0);      // if (dst == NULL) return -1;
    __ cror(CCR1, Assembler::equal, CCR0, Assembler::less);
#ifdef PPC64
    __ extsw_(dst_pos, dst_pos); // if (dst_pos < 0) return -1;
#else
    __ or_(temp, dst_pos, dst_pos); // if (dst_pos < 0) return -1;
#endif
    __ cror(CCR5, Assembler::equal, CCR0, Assembler::less);
#ifdef PPC64
    __ extsw_(length, length);   // if (length < 0) return -1;
#else
    __ or_(temp, length, length);   // if (length < 0) return -1;
#endif
    __ cror(CCR1, Assembler::equal, CCR5, Assembler::equal);
    __ cror(CCR1, Assembler::equal, CCR0, Assembler::less);
    __ beq(CCR1, L_failed);

    BLOCK_COMMENT("arraycopy argument klass checks");
    __ load_klass(src_klass, src);
    __ load_klass(dst_klass, dst);

    // Load layout helper
    //
    //  |array_tag|     | header_size | element_type |     |log2_element_size|
    // 32        30    24            16              8     2                 0
    //
    //   array_tag: typeArray = 0x3, objArray = 0x2, non-array = 0x0
    //

    int lh_offset = in_bytes(Klass::layout_helper_offset());

    // Load 32-bits signed value. Use br() instruction with it to check icc.
    __ lwz(lh, lh_offset, src_klass);

    // Handle objArrays completely differently...
    jint objArray_lh = Klass::array_layout_helper(T_OBJECT);
    __ load_const_optimized(temp, objArray_lh, R0);
    __ cmpw(CCR0, lh, temp);
    __ beq(CCR0, L_objArray);

    __ cmp(CCR5, src_klass, dst_klass);          // if (src->klass() != dst->klass()) return -1;
    __ cmpwi(CCR6, lh, Klass::_lh_neutral_value); // if (!src->is_Array()) return -1;

    __ crnand(CCR5, Assembler::equal, CCR6, Assembler::less);
    __ beq(CCR5, L_failed);

    // At this point, it is known to be a typeArray (array_tag 0x3).
#ifdef ASSERT
    { Label L;
      jint lh_prim_tag_in_place = (Klass::_lh_array_tag_type_value << Klass::_lh_array_tag_shift);
      __ load_const_optimized(temp, lh_prim_tag_in_place, R0);
      __ cmpw(CCR0, lh, temp);
      __ bge(CCR0, L);
      __ stop("must be a primitive array");
      __ bind(L);
    }
#endif

    arraycopy_range_checks(src, src_pos, dst, dst_pos, length,
                           temp, dst_klass, L_failed);

    // TypeArrayKlass
    //
    // src_addr = (src + array_header_in_bytes()) + (src_pos << log2elemsize);
    // dst_addr = (dst + array_header_in_bytes()) + (dst_pos << log2elemsize);
    //

    const Register offset = dst_klass;    // array offset
    const Register elsize = src_klass;    // log2 element size

    __ rlicl(offset, lh, 64 - Klass::_lh_header_size_shift, 64 - exact_log2(Klass::_lh_header_size_mask + 1));
    __ andi(elsize, lh, Klass::_lh_log2_element_size_mask);
    __ add(src, offset, src);       // src array offset
    __ add(dst, offset, dst);       // dst array offset

    // Next registers should be set before the jump to corresponding stub.
    const Register from     = R3_ARG1;  // source array address
    const Register to       = R4_ARG2;  // destination array address
    const Register count    = R5_ARG3;  // elements count

    // 'from', 'to', 'count' registers should be set in this order
    // since they are the same as 'src', 'src_pos', 'dst'.

    BLOCK_COMMENT("scale indexes to element size");
    __ sl(src_pos, src_pos, elsize);
    __ sl(dst_pos, dst_pos, elsize);
    __ add(from, src_pos, src);  // src_addr
    __ add(to, dst_pos, dst);    // dst_addr
    __ mr(count, length);        // length

    BLOCK_COMMENT("choose copy loop based on element size");
    // Using conditional branches with range 32kB.
    const int bo = Assembler::bcondCRbiIs1, bi = Assembler::bi0(CCR0, Assembler::equal);
    __ cmpwi(CCR0, elsize, 0);
    __ bc(bo, bi, entry_jbyte_arraycopy);
    __ cmpwi(CCR0, elsize, LogBytesPerShort);
    __ bc(bo, bi, entry_jshort_arraycopy);
    __ cmpwi(CCR0, elsize, LogBytesPerInt);
    __ bc(bo, bi, entry_jint_arraycopy);
#ifdef ASSERT
    { Label L;
      __ cmpwi(CCR0, elsize, LogBytesPerLong);
      __ beq(CCR0, L);
      __ stop("must be long copy, but elsize is wrong");
      __ bind(L);
    }
#endif
    __ b(entry_jlong_arraycopy);

    // ObjArrayKlass
  __ bind(L_objArray);
    // live at this point:  src_klass, dst_klass, src[_pos], dst[_pos], length

    Label L_disjoint_plain_copy, L_checkcast_copy;
    //  test array classes for subtyping
    __ cmp(CCR0, src_klass, dst_klass);         // usual case is exact equality
    __ bne(CCR0, L_checkcast_copy);

    // Identically typed arrays can be copied without element-wise checks.
    arraycopy_range_checks(src, src_pos, dst, dst_pos, length,
                           temp, lh, L_failed);

    __ addi(src, src, arrayOopDesc::base_offset_in_bytes(T_OBJECT)); //src offset
    __ addi(dst, dst, arrayOopDesc::base_offset_in_bytes(T_OBJECT)); //dst offset
    __ sli(src_pos, src_pos, LogBytesPerHeapOop);
    __ sli(dst_pos, dst_pos, LogBytesPerHeapOop);
    __ add(from, src_pos, src);  // src_addr
    __ add(to, dst_pos, dst);    // dst_addr
    __ mr(count, length);        // length
    __ b(entry_oop_arraycopy);

  __ bind(L_checkcast_copy);
    // live at this point:  src_klass, dst_klass
    {
      // Before looking at dst.length, make sure dst is also an objArray.
      __ lwz(temp, lh_offset, dst_klass);
      __ cmpw(CCR0, lh, temp);
      __ bne(CCR0, L_failed);

      // It is safe to examine both src.length and dst.length.
      arraycopy_range_checks(src, src_pos, dst, dst_pos, length,
                             temp, lh, L_failed);

      // Marshal the base address arguments now, freeing registers.
      __ addi(src, src, arrayOopDesc::base_offset_in_bytes(T_OBJECT)); //src offset
      __ addi(dst, dst, arrayOopDesc::base_offset_in_bytes(T_OBJECT)); //dst offset
      __ sli(src_pos, src_pos, LogBytesPerHeapOop);
      __ sli(dst_pos, dst_pos, LogBytesPerHeapOop);
      __ add(from, src_pos, src);  // src_addr
      __ add(to, dst_pos, dst);    // dst_addr
      __ mr(count, length);        // length

      Register sco_temp = R6_ARG4;             // This register is free now.
      assert_different_registers(from, to, count, sco_temp,
                                 dst_klass, src_klass);

      // Generate the type check.
      int sco_offset = in_bytes(Klass::super_check_offset_offset());
      __ lwz(sco_temp, sco_offset, dst_klass);
      generate_type_check(src_klass, sco_temp, dst_klass,
                          temp, L_disjoint_plain_copy);

      // Fetch destination element klass from the ObjArrayKlass header.
      int ek_offset = in_bytes(ObjArrayKlass::element_klass_offset());

      // The checkcast_copy loop needs two extra arguments:
      __ l(R7_ARG5, ek_offset, dst_klass);   // dest elem klass
      __ lwz(R6_ARG4, sco_offset, R7_ARG5);   // sco of elem klass
      __ b(entry_checkcast_arraycopy);
    }

    __ bind(L_disjoint_plain_copy);
    __ b(entry_disjoint_oop_arraycopy);

  __ bind(L_failed);
    __ li(R3_RET, -1); // return -1
    __ blr();
    return start;
  }


  // Arguments for generated stub:
  //   R3_ARG1   - source byte array address
  //   R4_ARG2   - destination byte array address
  //   R5_ARG3   - round key array
  address generate_aescrypt_encryptBlock() {
    assert(UseAES, "need AES instructions and misaligned SSE support");
    StubCodeMark mark(this, "StubRoutines", "aescrypt_encryptBlock");

    address start = __ function_entry();

    Label L_doLast;

    Register from           = R3_ARG1;  // source array address
    Register to             = R4_ARG2;  // destination array address
    Register key            = R5_ARG3;  // round key array

    Register keylen         = R8;
    Register temp           = R9;
    Register keypos         = R10;
    Register fifteen        = R12;

    VectorRegister vRet     = VR0;

    VectorRegister vKey1    = VR1;
    VectorRegister vKey2    = VR2;
    VectorRegister vKey3    = VR3;
    VectorRegister vKey4    = VR4;

    VectorRegister fromPerm = VR5;
    VectorRegister keyPerm  = VR6;
    VectorRegister toPerm   = VR7;
    VectorRegister fSplt    = VR8;

    VectorRegister vTmp1    = VR9;
    VectorRegister vTmp2    = VR10;
    VectorRegister vTmp3    = VR11;
    VectorRegister vTmp4    = VR12;

    __ li              (fifteen, 15);

    // load unaligned from[0-15] to vsRet
    __ lvx             (vRet, from);
    __ lvx             (vTmp1, fifteen, from);
    __ lvsl            (fromPerm, from);
#ifdef VM_LITTLE_ENDIAN
    __ vspltisb        (fSplt, 0x0f);
    __ vxor            (fromPerm, fromPerm, fSplt);
#endif
    __ vperm           (vRet, vRet, vTmp1, fromPerm);

    // load keylen (44 or 52 or 60)
    __ lwz             (keylen, arrayOopDesc::length_offset_in_bytes() - arrayOopDesc::base_offset_in_bytes(T_INT), key);

    // to load keys
    __ load_perm       (keyPerm, key);
#ifdef VM_LITTLE_ENDIAN
    __ vspltisb        (vTmp2, -16);
    __ vrld            (keyPerm, keyPerm, vTmp2);
    __ vrld            (keyPerm, keyPerm, vTmp2);
    __ vsldoi          (keyPerm, keyPerm, keyPerm, 8);
#endif

    // load the 1st round key to vTmp1
    __ lvx             (vTmp1, key);
    __ li              (keypos, 16);
    __ lvx             (vKey1, keypos, key);
    __ vec_perm        (vTmp1, vKey1, keyPerm);

    // 1st round
    __ vxor            (vRet, vRet, vTmp1);

    // load the 2nd round key to vKey1
    __ li              (keypos, 32);
    __ lvx             (vKey2, keypos, key);
    __ vec_perm        (vKey1, vKey2, keyPerm);

    // load the 3rd round key to vKey2
    __ li              (keypos, 48);
    __ lvx             (vKey3, keypos, key);
    __ vec_perm        (vKey2, vKey3, keyPerm);

    // load the 4th round key to vKey3
    __ li              (keypos, 64);
    __ lvx             (vKey4, keypos, key);
    __ vec_perm        (vKey3, vKey4, keyPerm);

    // load the 5th round key to vKey4
    __ li              (keypos, 80);
    __ lvx             (vTmp1, keypos, key);
    __ vec_perm        (vKey4, vTmp1, keyPerm);

    // 2nd - 5th rounds
    __ vcipher         (vRet, vRet, vKey1);
    __ vcipher         (vRet, vRet, vKey2);
    __ vcipher         (vRet, vRet, vKey3);
    __ vcipher         (vRet, vRet, vKey4);

    // load the 6th round key to vKey1
    __ li              (keypos, 96);
    __ lvx             (vKey2, keypos, key);
    __ vec_perm        (vKey1, vTmp1, vKey2, keyPerm);

    // load the 7th round key to vKey2
    __ li              (keypos, 112);
    __ lvx             (vKey3, keypos, key);
    __ vec_perm        (vKey2, vKey3, keyPerm);

    // load the 8th round key to vKey3
    __ li              (keypos, 128);
    __ lvx             (vKey4, keypos, key);
    __ vec_perm        (vKey3, vKey4, keyPerm);

    // load the 9th round key to vKey4
    __ li              (keypos, 144);
    __ lvx             (vTmp1, keypos, key);
    __ vec_perm        (vKey4, vTmp1, keyPerm);

    // 6th - 9th rounds
    __ vcipher         (vRet, vRet, vKey1);
    __ vcipher         (vRet, vRet, vKey2);
    __ vcipher         (vRet, vRet, vKey3);
    __ vcipher         (vRet, vRet, vKey4);

    // load the 10th round key to vKey1
    __ li              (keypos, 160);
    __ lvx             (vKey2, keypos, key);
    __ vec_perm        (vKey1, vTmp1, vKey2, keyPerm);

    // load the 11th round key to vKey2
    __ li              (keypos, 176);
    __ lvx             (vTmp1, keypos, key);
    __ vec_perm        (vKey2, vTmp1, keyPerm);

    // if all round keys are loaded, skip next 4 rounds
    __ cmpwi           (CCR0, keylen, 44);
    __ beq             (CCR0, L_doLast);

    // 10th - 11th rounds
    __ vcipher         (vRet, vRet, vKey1);
    __ vcipher         (vRet, vRet, vKey2);

    // load the 12th round key to vKey1
    __ li              (keypos, 192);
    __ lvx             (vKey2, keypos, key);
    __ vec_perm        (vKey1, vTmp1, vKey2, keyPerm);

    // load the 13th round key to vKey2
    __ li              (keypos, 208);
    __ lvx             (vTmp1, keypos, key);
    __ vec_perm        (vKey2, vTmp1, keyPerm);

    // if all round keys are loaded, skip next 2 rounds
    __ cmpwi           (CCR0, keylen, 52);
    __ beq             (CCR0, L_doLast);

    // 12th - 13th rounds
    __ vcipher         (vRet, vRet, vKey1);
    __ vcipher         (vRet, vRet, vKey2);

    // load the 14th round key to vKey1
    __ li              (keypos, 224);
    __ lvx             (vKey2, keypos, key);
    __ vec_perm        (vKey1, vTmp1, vKey2, keyPerm);

    // load the 15th round key to vKey2
    __ li              (keypos, 240);
    __ lvx             (vTmp1, keypos, key);
    __ vec_perm        (vKey2, vTmp1, keyPerm);

    __ bind(L_doLast);

    // last two rounds
    __ vcipher         (vRet, vRet, vKey1);
    __ vcipherlast     (vRet, vRet, vKey2);

    // store result (unaligned)
#ifdef VM_LITTLE_ENDIAN
    __ lvsl            (toPerm, to);
#else
    __ lvsr            (toPerm, to);
#endif
    __ vspltisb        (vTmp3, -1);
    __ vspltisb        (vTmp4, 0);
    __ lvx             (vTmp1, to);
    __ lvx             (vTmp2, fifteen, to);
#ifdef VM_LITTLE_ENDIAN
    __ vperm           (vTmp3, vTmp3, vTmp4, toPerm); // generate select mask
    __ vxor            (toPerm, toPerm, fSplt);       // swap bytes
#else
    __ vperm           (vTmp3, vTmp4, vTmp3, toPerm); // generate select mask
#endif
    __ vperm           (vTmp4, vRet, vRet, toPerm);   // rotate data
    __ vsel            (vTmp2, vTmp4, vTmp2, vTmp3);
    __ vsel            (vTmp1, vTmp1, vTmp4, vTmp3);
    __ stvx            (vTmp2, fifteen, to);          // store this one first (may alias)
    __ stvx            (vTmp1, to);

    __ blr();
     return start;
  }

  // Arguments for generated stub:
  //   R3_ARG1   - source byte array address
  //   R4_ARG2   - destination byte array address
  //   R5_ARG3   - K (key) in little endian int array
  address generate_aescrypt_decryptBlock() {
    assert(UseAES, "need AES instructions and misaligned SSE support");
    StubCodeMark mark(this, "StubRoutines", "aescrypt_decryptBlock");

    address start = __ function_entry();

    Label L_doLast;
    Label L_do44;
    Label L_do52;
    Label L_do60;

    Register from           = R3_ARG1;  // source array address
    Register to             = R4_ARG2;  // destination array address
    Register key            = R5_ARG3;  // round key array

    Register keylen         = R8;
    Register temp           = R9;
    Register keypos         = R10;
    Register fifteen        = R12;

    VectorRegister vRet     = VR0;

    VectorRegister vKey1    = VR1;
    VectorRegister vKey2    = VR2;
    VectorRegister vKey3    = VR3;
    VectorRegister vKey4    = VR4;
    VectorRegister vKey5    = VR5;

    VectorRegister fromPerm = VR6;
    VectorRegister keyPerm  = VR7;
    VectorRegister toPerm   = VR8;
    VectorRegister fSplt    = VR9;

    VectorRegister vTmp1    = VR10;
    VectorRegister vTmp2    = VR11;
    VectorRegister vTmp3    = VR12;
    VectorRegister vTmp4    = VR13;

    __ li              (fifteen, 15);

    // load unaligned from[0-15] to vsRet
    __ lvx             (vRet, from);
    __ lvx             (vTmp1, fifteen, from);
    __ lvsl            (fromPerm, from);
#ifdef VM_LITTLE_ENDIAN
    __ vspltisb        (fSplt, 0x0f);
    __ vxor            (fromPerm, fromPerm, fSplt);
#endif
    __ vperm           (vRet, vRet, vTmp1, fromPerm); // align [and byte swap in LE]

    // load keylen (44 or 52 or 60)
    __ lwz             (keylen, arrayOopDesc::length_offset_in_bytes() - arrayOopDesc::base_offset_in_bytes(T_INT), key);

    // to load keys
    __ load_perm       (keyPerm, key);
#ifdef VM_LITTLE_ENDIAN
    __ vxor            (vTmp2, vTmp2, vTmp2);
    __ vspltisb        (vTmp2, -16);
    __ vrld            (keyPerm, keyPerm, vTmp2);
    __ vrld            (keyPerm, keyPerm, vTmp2);
    __ vsldoi          (keyPerm, keyPerm, keyPerm, 8);
#endif

    __ cmpwi           (CCR0, keylen, 44);
    __ beq             (CCR0, L_do44);

    __ cmpwi           (CCR0, keylen, 52);
    __ beq             (CCR0, L_do52);

    // load the 15th round key to vKey1
    __ li              (keypos, 240);
    __ lvx             (vKey1, keypos, key);
    __ li              (keypos, 224);
    __ lvx             (vKey2, keypos, key);
    __ vec_perm        (vKey1, vKey2, vKey1, keyPerm);

    // load the 14th round key to vKey2
    __ li              (keypos, 208);
    __ lvx             (vKey3, keypos, key);
    __ vec_perm        (vKey2, vKey3, vKey2, keyPerm);

    // load the 13th round key to vKey3
    __ li              (keypos, 192);
    __ lvx             (vKey4, keypos, key);
    __ vec_perm        (vKey3, vKey4, vKey3, keyPerm);

    // load the 12th round key to vKey4
    __ li              (keypos, 176);
    __ lvx             (vKey5, keypos, key);
    __ vec_perm        (vKey4, vKey5, vKey4, keyPerm);

    // load the 11th round key to vKey5
    __ li              (keypos, 160);
    __ lvx             (vTmp1, keypos, key);
    __ vec_perm        (vKey5, vTmp1, vKey5, keyPerm);

    // 1st - 5th rounds
    __ vxor            (vRet, vRet, vKey1);
    __ vncipher        (vRet, vRet, vKey2);
    __ vncipher        (vRet, vRet, vKey3);
    __ vncipher        (vRet, vRet, vKey4);
    __ vncipher        (vRet, vRet, vKey5);

    __ b               (L_doLast);

    __ bind            (L_do52);

    // load the 13th round key to vKey1
    __ li              (keypos, 208);
    __ lvx             (vKey1, keypos, key);
    __ li              (keypos, 192);
    __ lvx             (vKey2, keypos, key);
    __ vec_perm        (vKey1, vKey2, vKey1, keyPerm);

    // load the 12th round key to vKey2
    __ li              (keypos, 176);
    __ lvx             (vKey3, keypos, key);
    __ vec_perm        (vKey2, vKey3, vKey2, keyPerm);

    // load the 11th round key to vKey3
    __ li              (keypos, 160);
    __ lvx             (vTmp1, keypos, key);
    __ vec_perm        (vKey3, vTmp1, vKey3, keyPerm);

    // 1st - 3rd rounds
    __ vxor            (vRet, vRet, vKey1);
    __ vncipher        (vRet, vRet, vKey2);
    __ vncipher        (vRet, vRet, vKey3);

    __ b               (L_doLast);

    __ bind            (L_do44);

    // load the 11th round key to vKey1
    __ li              (keypos, 176);
    __ lvx             (vKey1, keypos, key);
    __ li              (keypos, 160);
    __ lvx             (vTmp1, keypos, key);
    __ vec_perm        (vKey1, vTmp1, vKey1, keyPerm);

    // 1st round
    __ vxor            (vRet, vRet, vKey1);

    __ bind            (L_doLast);

    // load the 10th round key to vKey1
    __ li              (keypos, 144);
    __ lvx             (vKey2, keypos, key);
    __ vec_perm        (vKey1, vKey2, vTmp1, keyPerm);

    // load the 9th round key to vKey2
    __ li              (keypos, 128);
    __ lvx             (vKey3, keypos, key);
    __ vec_perm        (vKey2, vKey3, vKey2, keyPerm);

    // load the 8th round key to vKey3
    __ li              (keypos, 112);
    __ lvx             (vKey4, keypos, key);
    __ vec_perm        (vKey3, vKey4, vKey3, keyPerm);

    // load the 7th round key to vKey4
    __ li              (keypos, 96);
    __ lvx             (vKey5, keypos, key);
    __ vec_perm        (vKey4, vKey5, vKey4, keyPerm);

    // load the 6th round key to vKey5
    __ li              (keypos, 80);
    __ lvx             (vTmp1, keypos, key);
    __ vec_perm        (vKey5, vTmp1, vKey5, keyPerm);

    // last 10th - 6th rounds
    __ vncipher        (vRet, vRet, vKey1);
    __ vncipher        (vRet, vRet, vKey2);
    __ vncipher        (vRet, vRet, vKey3);
    __ vncipher        (vRet, vRet, vKey4);
    __ vncipher        (vRet, vRet, vKey5);

    // load the 5th round key to vKey1
    __ li              (keypos, 64);
    __ lvx             (vKey2, keypos, key);
    __ vec_perm        (vKey1, vKey2, vTmp1, keyPerm);

    // load the 4th round key to vKey2
    __ li              (keypos, 48);
    __ lvx             (vKey3, keypos, key);
    __ vec_perm        (vKey2, vKey3, vKey2, keyPerm);

    // load the 3rd round key to vKey3
    __ li              (keypos, 32);
    __ lvx             (vKey4, keypos, key);
    __ vec_perm        (vKey3, vKey4, vKey3, keyPerm);

    // load the 2nd round key to vKey4
    __ li              (keypos, 16);
    __ lvx             (vKey5, keypos, key);
    __ vec_perm        (vKey4, vKey5, vKey4, keyPerm);

    // load the 1st round key to vKey5
    __ lvx             (vTmp1, key);
    __ vec_perm        (vKey5, vTmp1, vKey5, keyPerm);

    // last 5th - 1th rounds
    __ vncipher        (vRet, vRet, vKey1);
    __ vncipher        (vRet, vRet, vKey2);
    __ vncipher        (vRet, vRet, vKey3);
    __ vncipher        (vRet, vRet, vKey4);
    __ vncipherlast    (vRet, vRet, vKey5);

    // store result (unaligned)
#ifdef VM_LITTLE_ENDIAN
    __ lvsl            (toPerm, to);
#else
    __ lvsr            (toPerm, to);
#endif
    __ vspltisb        (vTmp3, -1);
    __ vspltisb        (vTmp4, 0);
    __ lvx             (vTmp1, to);
    __ lvx             (vTmp2, fifteen, to);
#ifdef VM_LITTLE_ENDIAN
    __ vperm           (vTmp3, vTmp3, vTmp4, toPerm); // generate select mask
    __ vxor            (toPerm, toPerm, fSplt);       // swap bytes
#else
    __ vperm           (vTmp3, vTmp4, vTmp3, toPerm); // generate select mask
#endif
    __ vperm           (vTmp4, vRet, vRet, toPerm);   // rotate data
    __ vsel            (vTmp2, vTmp4, vTmp2, vTmp3);
    __ vsel            (vTmp1, vTmp1, vTmp4, vTmp3);
    __ stvx            (vTmp2, fifteen, to);          // store this one first (may alias)
    __ stvx            (vTmp1, to);

    __ blr();
     return start;
  }

  address generate_sha256_implCompress(bool multi_block, const char *name) {
    assert(UseSHA, "need SHA instructions");
    StubCodeMark mark(this, "StubRoutines", name);
    address start = __ function_entry();
#ifdef PPC64
    __ sha256 (multi_block);
#else
    __ asm_assert(false, "sha256 is not implemented on PPC32", 0x34);
#endif
    __ blr();
    return start;
  }

  address generate_sha512_implCompress(bool multi_block, const char *name) {
    assert(UseSHA, "need SHA instructions");
    StubCodeMark mark(this, "StubRoutines", name);
    address start = __ function_entry();
#ifdef PPC64
    __ sha512 (multi_block);
#else
    __ asm_assert(false, "sha512 is not implemented on PPC32", 0x12);
#endif
    __ blr();
    return start;
  }

  void generate_arraycopy_stubs() {
    // Note: the disjoint stubs must be generated first, some of
    // the conjoint stubs use them.

    // non-aligned disjoint versions
    StubRoutines::_jbyte_disjoint_arraycopy       = generate_disjoint_byte_copy(false, "jbyte_disjoint_arraycopy");
    StubRoutines::_jshort_disjoint_arraycopy      = generate_disjoint_short_copy(false, "jshort_disjoint_arraycopy");
    StubRoutines::_jint_disjoint_arraycopy        = generate_disjoint_int_copy(false, "jint_disjoint_arraycopy");
    StubRoutines::_jlong_disjoint_arraycopy       = generate_disjoint_long_copy(false, "jlong_disjoint_arraycopy");
    StubRoutines::_oop_disjoint_arraycopy         = generate_disjoint_oop_copy(false, "oop_disjoint_arraycopy", false);
    StubRoutines::_oop_disjoint_arraycopy_uninit  = generate_disjoint_oop_copy(false, "oop_disjoint_arraycopy_uninit", true);

    // aligned disjoint versions
    StubRoutines::_arrayof_jbyte_disjoint_arraycopy      = generate_disjoint_byte_copy(true, "arrayof_jbyte_disjoint_arraycopy");
    StubRoutines::_arrayof_jshort_disjoint_arraycopy     = generate_disjoint_short_copy(true, "arrayof_jshort_disjoint_arraycopy");
    StubRoutines::_arrayof_jint_disjoint_arraycopy       = generate_disjoint_int_copy(true, "arrayof_jint_disjoint_arraycopy");
    StubRoutines::_arrayof_jlong_disjoint_arraycopy      = generate_disjoint_long_copy(true, "arrayof_jlong_disjoint_arraycopy");
    StubRoutines::_arrayof_oop_disjoint_arraycopy        = generate_disjoint_oop_copy(true, "arrayof_oop_disjoint_arraycopy", false);
    StubRoutines::_arrayof_oop_disjoint_arraycopy_uninit = generate_disjoint_oop_copy(true, "oop_disjoint_arraycopy_uninit", true);

    // non-aligned conjoint versions
    StubRoutines::_jbyte_arraycopy      = generate_conjoint_byte_copy(false, "jbyte_arraycopy");
    StubRoutines::_jshort_arraycopy     = generate_conjoint_short_copy(false, "jshort_arraycopy");
    StubRoutines::_jint_arraycopy       = generate_conjoint_int_copy(false, "jint_arraycopy");
    StubRoutines::_jlong_arraycopy      = generate_conjoint_long_copy(false, "jlong_arraycopy");
    StubRoutines::_oop_arraycopy        = generate_conjoint_oop_copy(false, "oop_arraycopy", false);
    StubRoutines::_oop_arraycopy_uninit = generate_conjoint_oop_copy(false, "oop_arraycopy_uninit", true);

    // aligned conjoint versions
    StubRoutines::_arrayof_jbyte_arraycopy      = generate_conjoint_byte_copy(true, "arrayof_jbyte_arraycopy");
    StubRoutines::_arrayof_jshort_arraycopy     = generate_conjoint_short_copy(true, "arrayof_jshort_arraycopy");
    StubRoutines::_arrayof_jint_arraycopy       = generate_conjoint_int_copy(true, "arrayof_jint_arraycopy");
    StubRoutines::_arrayof_jlong_arraycopy      = generate_conjoint_long_copy(true, "arrayof_jlong_arraycopy");
    StubRoutines::_arrayof_oop_arraycopy        = generate_conjoint_oop_copy(true, "arrayof_oop_arraycopy", false);
    StubRoutines::_arrayof_oop_arraycopy_uninit = generate_conjoint_oop_copy(true, "arrayof_oop_arraycopy", true);

    // special/generic versions
    StubRoutines::_checkcast_arraycopy        = generate_checkcast_copy("checkcast_arraycopy", false);
    StubRoutines::_checkcast_arraycopy_uninit = generate_checkcast_copy("checkcast_arraycopy_uninit", true);

    StubRoutines::_unsafe_arraycopy  = generate_unsafe_copy("unsafe_arraycopy",
                                                            STUB_ENTRY(jbyte_arraycopy),
                                                            STUB_ENTRY(jshort_arraycopy),
                                                            STUB_ENTRY(jint_arraycopy),
                                                            STUB_ENTRY(jlong_arraycopy));
    StubRoutines::_generic_arraycopy = generate_generic_copy("generic_arraycopy",
                                                             STUB_ENTRY(jbyte_arraycopy),
                                                             STUB_ENTRY(jshort_arraycopy),
                                                             STUB_ENTRY(jint_arraycopy),
                                                             STUB_ENTRY(oop_arraycopy),
                                                             STUB_ENTRY(oop_disjoint_arraycopy),
                                                             STUB_ENTRY(jlong_arraycopy),
                                                             STUB_ENTRY(checkcast_arraycopy));

    // fill routines
    StubRoutines::_jbyte_fill          = generate_fill(T_BYTE,  false, "jbyte_fill");
    StubRoutines::_jshort_fill         = generate_fill(T_SHORT, false, "jshort_fill");
    StubRoutines::_jint_fill           = generate_fill(T_INT,   false, "jint_fill");
    StubRoutines::_arrayof_jbyte_fill  = generate_fill(T_BYTE,  true, "arrayof_jbyte_fill");
    StubRoutines::_arrayof_jshort_fill = generate_fill(T_SHORT, true, "arrayof_jshort_fill");
    StubRoutines::_arrayof_jint_fill   = generate_fill(T_INT,   true, "arrayof_jint_fill");
  }

  // Safefetch stubs.
  void generate_safefetch(const char* name, int size, address* entry, address* fault_pc, address* continuation_pc) {
    // safefetch signatures:
    //   int      SafeFetch32(int*      adr, int      errValue);
    //   intptr_t SafeFetchN (intptr_t* adr, intptr_t errValue);
    //
    // arguments:
    //   R3_ARG1 = adr
    //   R4_ARG2 = errValue
    //
    // result:
    //   R3_RET  = *adr or errValue

    StubCodeMark mark(this, "StubRoutines", name);

    // Entry point, pc or function descriptor.
    *entry = __ function_entry();

    // Load *adr into R4_ARG2, may fault.
    *fault_pc = __ pc();
    switch (size) {
      case 4:
        // int32_t, signed extended
        __ lwa(R4_ARG2, 0, R3_ARG1);
        break;
      case 8:
        // int64_t
        __ l(R4_ARG2, 0, R3_ARG1);
        break;
      default:
        ShouldNotReachHere();
    }

    // return errValue or *adr
    *continuation_pc = __ pc();
    __ mr(R3_RET, R4_ARG2);
    __ blr();
  }
#ifdef PPC64
  /**
   * Arguments:
   *
   * Inputs:
   *   R3_ARG1    - int   crc
   *   R4_ARG2    - byte* buf
   *   R5_ARG3    - int   length (of buffer)
   *
   * scratch:
   *   R2, R6-R12
   *
   * Ouput:
   *   R3_RET     - int   crc result
   */
  // Compute CRC32 function.
  address generate_CRC32_updateBytes(const char* name) {
    __ align(CodeEntryAlignment);
    StubCodeMark mark(this, "StubRoutines", name);
    address start = __ function_entry();  // Remember stub start address (is rtn value).

    // arguments to kernel_crc32:
    const Register crc     = R3_ARG1;  // Current checksum, preset by caller or result from previous call.
    const Register data    = R4_ARG2;  // source byte array
    const Register dataLen = R5_ARG3;  // #bytes to process

    const Register table   = R6;       // crc table address

#ifdef VM_LITTLE_ENDIAN
    if (VM_Version::has_vpmsumb()) {
      const Register constants    = R2;  // constants address
      const Register bconstants   = R8;  // barret table address

      const Register t0      = R9;
      const Register t1      = R10;
      const Register t2      = R11;
      const Register t3      = R12;
      const Register t4      = R7;

      BLOCK_COMMENT("Stub body {");
      assert_different_registers(crc, data, dataLen, table);

      StubRoutines::ppc64::generate_load_crc_table_addr(_masm, table);
      StubRoutines::ppc64::generate_load_crc_constants_addr(_masm, constants);
      StubRoutines::ppc64::generate_load_crc_barret_constants_addr(_masm, bconstants);

      __ kernel_crc32_1word_vpmsumd(crc, data, dataLen, table, constants, bconstants, t0, t1, t2, t3, t4);

      BLOCK_COMMENT("return");
      __ mr_if_needed(R3_RET, crc);      // Updated crc is function result. No copying required (R3_ARG1 == R3_RET).
      __ blr();

      BLOCK_COMMENT("} Stub body");
    } else
#endif
    {
      const Register t0      = R2;
      const Register t1      = R7;
      const Register t2      = R8;
      const Register t3      = R9;
      const Register tc0     = R10;
      const Register tc1     = R11;
      const Register tc2     = R12;

      BLOCK_COMMENT("Stub body {");
      assert_different_registers(crc, data, dataLen, table);

      StubRoutines::ppc64::generate_load_crc_table_addr(_masm, table);

      __ kernel_crc32_1word(crc, data, dataLen, table, t0, t1, t2, t3, tc0, tc1, tc2, table);

      BLOCK_COMMENT("return");
      __ mr_if_needed(R3_RET, crc);      // Updated crc is function result. No copying required (R3_ARG1 == R3_RET).
      __ blr();

      BLOCK_COMMENT("} Stub body");
    }

    return start;
  }
#endif //PPC64
  // Initialization
  void generate_initial() {
    // Generates all stubs and initializes the entry points

    // Entry points that exist in all platforms.
    // Note: This is code that could be shared among different platforms - however the
    // benefit seems to be smaller than the disadvantage of having a
    // much more complicated generator structure. See also comment in
    // stubRoutines.hpp.

    StubRoutines::_forward_exception_entry          = generate_forward_exception();
    StubRoutines::_call_stub_entry                  = generate_call_stub(StubRoutines::_call_stub_return_address);
    StubRoutines::_catch_exception_entry            = generate_catch_exception();

    // Build this early so it's available for the interpreter.
    StubRoutines::_throw_StackOverflowError_entry   =
      generate_throw_exception("StackOverflowError throw_exception",
                               CAST_FROM_FN_PTR(address, SharedRuntime::throw_StackOverflowError), false);

    // CRC32 Intrinsics.
    if (UseCRC32Intrinsics) {
#ifdef PPC64
      StubRoutines::_crc_table_adr    = (address)StubRoutines::ppc64::_crc_table;
      StubRoutines::_updateBytesCRC32 = generate_CRC32_updateBytes("CRC32_updateBytes");
#endif
    }
  }

  void generate_all() {
    // Generates all stubs and initializes the entry points

    // These entry points require SharedInfo::stack0 to be set up in
    // non-core builds
    StubRoutines::_throw_AbstractMethodError_entry         = generate_throw_exception("AbstractMethodError throw_exception",          CAST_FROM_FN_PTR(address, SharedRuntime::throw_AbstractMethodError),  false);
    // Handle IncompatibleClassChangeError in itable stubs.
    StubRoutines::_throw_IncompatibleClassChangeError_entry= generate_throw_exception("IncompatibleClassChangeError throw_exception", CAST_FROM_FN_PTR(address, SharedRuntime::throw_IncompatibleClassChangeError),  false);
    StubRoutines::_throw_NullPointerException_at_call_entry= generate_throw_exception("NullPointerException at call throw_exception", CAST_FROM_FN_PTR(address, SharedRuntime::throw_NullPointerException_at_call), false);

    StubRoutines::_handler_for_unsafe_access_entry         = generate_handler_for_unsafe_access();

    // support for verify_oop (must happen after universe_init)
    StubRoutines::_verify_oop_subroutine_entry             = generate_verify_oop();

    // arraycopy stubs used by compilers
    generate_arraycopy_stubs();

    // Safefetch stubs.
    generate_safefetch("SafeFetch32", sizeof(int),     &StubRoutines::_safefetch32_entry,
                                                       &StubRoutines::_safefetch32_fault_pc,
                                                       &StubRoutines::_safefetch32_continuation_pc);
    generate_safefetch("SafeFetchN", sizeof(intptr_t), &StubRoutines::_safefetchN_entry,
                                                       &StubRoutines::_safefetchN_fault_pc,
                                                       &StubRoutines::_safefetchN_continuation_pc);

    if (UseAESIntrinsics) {
      StubRoutines::_aescrypt_encryptBlock = generate_aescrypt_encryptBlock();
      StubRoutines::_aescrypt_decryptBlock = generate_aescrypt_decryptBlock();
    }

#ifdef COMPILER2
    if (UseMontgomeryMultiplyIntrinsic) {
      StubRoutines::_montgomeryMultiply
        = CAST_FROM_FN_PTR(address, SharedRuntime::montgomery_multiply);
    }
    if (UseMontgomerySquareIntrinsic) {
      StubRoutines::_montgomerySquare
        = CAST_FROM_FN_PTR(address, SharedRuntime::montgomery_square);
    }
#endif // COMPILER2

    if (UseSHA256Intrinsics) {
      StubRoutines::_sha256_implCompress   = generate_sha256_implCompress(false, "sha256_implCompress");
      StubRoutines::_sha256_implCompressMB = generate_sha256_implCompress(true,  "sha256_implCompressMB");
    }
    if (UseSHA512Intrinsics) {
      StubRoutines::_sha512_implCompress   = generate_sha512_implCompress(false, "sha512_implCompress");
      StubRoutines::_sha512_implCompressMB = generate_sha512_implCompress(true, "sha512_implCompressMB");
    }
  }

 public:
  StubGenerator(CodeBuffer* code, bool all) : StubCodeGenerator(code) {
    // replace the standard masm with a special one:
    _masm = new MacroAssembler(code);
    if (all) {
      generate_all();
    } else {
      generate_initial();
    }
  }
};

void StubGenerator_generate(CodeBuffer* code, bool all) {
  StubGenerator g(code, all);
}
