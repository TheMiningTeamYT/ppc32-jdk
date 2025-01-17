/*
 * Copyright (c) 1999, 2015, Oracle and/or its affiliates. All rights reserved.
 * Copyright 2012, 2015 SAP AG. All rights reserved.
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
#include "c1/c1_Defs.hpp"
#include "c1/c1_MacroAssembler.hpp"
#include "c1/c1_Runtime1.hpp"
#include "interpreter/interpreter.hpp"
#include "nativeInst_ppc.hpp"
#include "oops/compiledICHolder.hpp"
#include "oops/oop.inline.hpp"
#include "prims/jvmtiExport.hpp"
#include "register_ppc.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/signature.hpp"
#include "runtime/vframeArray.hpp"
#include "utilities/macros.hpp"
#include "vmreg_ppc.inline.hpp"
#if INCLUDE_ALL_GCS
#include "gc_implementation/g1/g1SATBCardTableModRefBS.hpp"
#endif

#define STORE_WORD USE_SPE_ONLY(evstdd_aligned) NOT_USE_SPE(st)
#define LOAD_WORD  USE_SPE_ONLY(evldd_aligned)  NOT_USE_SPE(l)
#define WORD_SIZE  USE_SPE_ONLY(BytesPerLong)   NOT_USE_SPE(BytesPerWord)

// Implementation of StubAssembler

int StubAssembler::call_RT(Register oop_result1, Register metadata_result,
                           address entry_point, int number_of_arguments) {
  set_num_rt_args(0); // Nothing on stack
  assert(!(oop_result1->is_valid() || metadata_result->is_valid()) ||
         oop_result1 != metadata_result, "registers must be different");

  // Currently no stack banging. We assume that there are enough
  // StackShadowPages (which have been banged in generate_stack_overflow_check)
  // for the stub frame and the runtime frames.

  set_last_Java_frame(R1_SP, noreg);

  // ARG1 must hold thread address.
  mr(R3_ARG1, R16_thread);

  address return_pc = call_c_with_frame_resize(entry_point, /*No resize, we have a C compatible frame.*/0);

  reset_last_Java_frame();

  // Check for pending exceptions.
  {
    l(R0, in_bytes(Thread::pending_exception_offset()), R16_thread);
    cmpi(CCR0, R0, 0);

    // This used to conditionally jump to forward_exception however it is
    // possible if we relocate that the branch will not reach. So we must jump
    // around so we can always reach.

    Label ok;
    beq(CCR0, ok);

    // Make sure that the vm_results are cleared.
    if (oop_result1->is_valid() || metadata_result->is_valid()) {
      li(R0, 0);
      if (oop_result1->is_valid()) {
        st(R0, in_bytes(JavaThread::vm_result_offset()), R16_thread);
      }
      if (metadata_result->is_valid()) {
        st(R0, in_bytes(JavaThread::vm_result_2_offset()), R16_thread);
      }
    }

    if (frame_size() == no_frame_size) {
      ShouldNotReachHere(); // We always have a frame size.
      //pop_frame(); // pop the stub frame
      //l(R0, _abi(lr), R1_SP);
      //mtlr(R0);
      //load_const_optimized(R0, StubRoutines::forward_exception_entry());
      //mtctr(R0);
      //bctr();
    } else if (_stub_id == Runtime1::forward_exception_id) {
      should_not_reach_here();
    } else {
      // keep stub frame for next call_RT
#ifdef PPC64
      add_const_optimized(R0, R29_TOC, MacroAssembler::offset_to_global_toc(Runtime1::entry_for(Runtime1::forward_exception_id)));
#else
      load_const_optimized(R0, Runtime1::entry_for(Runtime1::forward_exception_id));
#endif
      mtctr(R0);
      bctr();
    }

    bind(ok);
  }

  // Get oop results if there are any and reset the values in the thread.
  if (oop_result1->is_valid()) {
    get_vm_result(oop_result1);
  }
  if (metadata_result->is_valid()) {
    get_vm_result_2(metadata_result);
  }

  return (int)(return_pc - code_section()->start());
}


int StubAssembler::call_RT(Register oop_result1, Register metadata_result, address entry, Register arg1) {
  mr_if_needed(R4_ARG2, arg1);
  return call_RT(oop_result1, metadata_result, entry, 1);
}


int StubAssembler::call_RT(Register oop_result1, Register metadata_result, address entry, Register arg1, Register arg2) {
  mr_if_needed(R4_ARG2, arg1);
  mr_if_needed(R5_ARG3, arg2); assert(arg2 != R4_ARG2, "smashed argument");
  return call_RT(oop_result1, metadata_result, entry, 2);
}


int StubAssembler::call_RT(Register oop_result1, Register metadata_result, address entry, Register arg1, Register arg2, Register arg3) {
  mr_if_needed(R4_ARG2, arg1);
  mr_if_needed(R5_ARG3, arg2); assert(arg2 != R4_ARG2, "smashed argument");
  mr_if_needed(R6_ARG4, arg3); assert(arg3 != R4_ARG2 && arg3 != R5_ARG3, "smashed argument");
  return call_RT(oop_result1, metadata_result, entry, 3);
}


// Implementation of Runtime1

#define __ sasm->

static int cpu_reg_save_offsets[FrameMap::nof_cpu_regs];
#ifndef USE_SPE
static int fpu_reg_save_offsets[FrameMap::nof_fpu_regs];
#endif
static int frame_size_in_bytes = -1;

static OopMap* generate_oop_map(StubAssembler* sasm, bool save_fpu_registers) {
  assert(frame_size_in_bytes > frame::abi_reg_args_size, "init");
  sasm->set_frame_size(frame_size_in_bytes / BytesPerWord);
  int frame_size_in_slots = frame_size_in_bytes / sizeof(jint);
  OopMap* oop_map = new OopMap(frame_size_in_slots, 0);

  int i;
  for (i = 0; i < FrameMap::nof_cpu_regs; i++) {
    Register r = as_Register(i);
    if (FrameMap::reg_needs_save(r)) {
      int sp_offset = cpu_reg_save_offsets[i];
#ifndef USE_SPE
      oop_map->set_callee_saved(VMRegImpl::stack2reg(sp_offset>>2), r->as_VMReg());
#ifdef PPC64
      oop_map->set_callee_saved(VMRegImpl::stack2reg((sp_offset>>2) + 1), r->as_VMReg()->next());
#endif // PPC64
#else // USE_SPE
      oop_map->set_callee_saved(VMRegImpl::stack2reg(sp_offset>>2), r->as_VMReg()->next());
      oop_map->set_callee_saved(VMRegImpl::stack2reg((sp_offset>>2) + 1), r->as_VMReg());
#endif
    }
  }

#ifndef USE_SPE
  if (save_fpu_registers) {
    for (i = 0; i < FrameMap::nof_fpu_regs; i++) {
      FloatRegister r = as_FloatRegister(i);
      int sp_offset = fpu_reg_save_offsets[i];
      oop_map->set_callee_saved(VMRegImpl::stack2reg(sp_offset>>2), r->as_VMReg());
      oop_map->set_callee_saved(VMRegImpl::stack2reg((sp_offset>>2) + 1), r->as_VMReg()->next());
    }
  }
#endif

  return oop_map;
}

static OopMap* save_live_registers(StubAssembler* sasm, bool save_fpu_registers = true,
                                   Register ret_pc = noreg, int stack_preserve = 0) {
  if (ret_pc == noreg) {
    ret_pc = R0;
    __ mflr(ret_pc);
  }
  __ st(ret_pc, _abi(lr), R1_SP); // C code needs pc in C1 method.
  __ push_frame(frame_size_in_bytes + stack_preserve, R0);

  // Record volatile registers as callee-save values in an OopMap so
  // their save locations will be propagated to the caller frame's
  // RegisterMap during StackFrameStream construction (needed for
  // deoptimization; see compiledVFrame::create_stack_value).
  // OopMap frame sizes are in c2 stack slot sizes (sizeof(jint)).

  int i;
  for (i = 0; i < FrameMap::nof_cpu_regs; i++) {
    Register r = as_Register(i);
    if (FrameMap::reg_needs_save(r)) {
      int sp_offset = cpu_reg_save_offsets[i];
      __ STORE_WORD(r, sp_offset + STACK_BIAS, R1_SP);
    }
  }

#ifndef USE_SPE
  if (save_fpu_registers) {
    for (i = 0; i < FrameMap::nof_fpu_regs; i++) {
      FloatRegister r = as_FloatRegister(i);
      int sp_offset = fpu_reg_save_offsets[i];
      __ stfd(r, sp_offset + STACK_BIAS, R1_SP);
    }
  }
#endif

  return generate_oop_map(sasm, save_fpu_registers);
}

static void restore_live_registers(StubAssembler* sasm, Register result1, Register result2,
                                   bool restore_fpu_registers = true) {
  for (int i = 0; i < FrameMap::nof_cpu_regs; i++) {
    Register r = as_Register(i);
    if (FrameMap::reg_needs_save(r) && r != result1 && r != result2) {
      int sp_offset = cpu_reg_save_offsets[i];
      __ LOAD_WORD(r, sp_offset + STACK_BIAS, R1_SP);
    }
  }

#ifndef USE_SPE
  if (restore_fpu_registers) {
    for (int i = 0; i < FrameMap::nof_fpu_regs; i++) {
      FloatRegister r = as_FloatRegister(i);
      int sp_offset = fpu_reg_save_offsets[i];
      __ lfd(r, sp_offset + STACK_BIAS, R1_SP);
    }
  }
#endif

  __ pop_frame();
  __ l(R0, _abi(lr), R1_SP);
  __ mtlr(R0);
}


void Runtime1::initialize_pd() {
  int i;
  int sp_offset = frame::abi_reg_args_size;

  for (i = 0; i < FrameMap::nof_cpu_regs; i++) {
    Register r = as_Register(i);
    if (FrameMap::reg_needs_save(r)) {
      cpu_reg_save_offsets[i] = sp_offset;
      sp_offset += WORD_SIZE;
    }
  }
#ifndef USE_SPE
  for (i = 0; i < FrameMap::nof_fpu_regs; i++) {
    fpu_reg_save_offsets[i] = sp_offset;
    sp_offset += BytesPerLong;
  }
#endif
  frame_size_in_bytes = align_size_up(sp_offset, frame::alignment_in_bytes);
}


OopMapSet* Runtime1::generate_exception_throw(StubAssembler* sasm, address target, bool has_argument) {
  // Make a frame and preserve the caller's caller-save registers.
  OopMap* oop_map = save_live_registers(sasm);

  int call_offset;
  if (!has_argument) {
    call_offset = __ call_RT(noreg, noreg, target);
  } else {
    call_offset = __ call_RT(noreg, noreg, target, R4_ARG2);
  }
  OopMapSet* oop_maps = new OopMapSet();
  oop_maps->add_gc_map(call_offset, oop_map);

  __ should_not_reach_here();
  return oop_maps;
}

static OopMapSet* generate_exception_throw_with_stack_parms(StubAssembler* sasm, address target,
                                                            int stack_parms) {
  // Make a frame and preserve the caller's caller-save registers.
  const int parm_size_in_bytes = align_size_up(stack_parms << LogBytesPerWord, frame::alignment_in_bytes);
  const int padding = parm_size_in_bytes - (stack_parms << LogBytesPerWord);
  OopMap* oop_map = save_live_registers(sasm, true, noreg, parm_size_in_bytes);

  int call_offset = 0;
  switch (stack_parms) {
    case 3:
    __ l(R6_ARG4, frame_size_in_bytes + padding + 2*BytesPerWord, R1_SP);
    case 2:
    __ l(R5_ARG3, frame_size_in_bytes + padding + BytesPerWord, R1_SP);
    case 1:
    __ l(R4_ARG2, frame_size_in_bytes + padding + 0, R1_SP);
    call_offset = __ call_RT(noreg, noreg, target);
    break;
    default: Unimplemented(); break;
  }
  OopMapSet* oop_maps = new OopMapSet();
  oop_maps->add_gc_map(call_offset, oop_map);

  __ should_not_reach_here();
  return oop_maps;
}


OopMapSet* Runtime1::generate_stub_call(StubAssembler* sasm, Register result, address target,
                                        Register arg1, Register arg2, Register arg3) {
  // Make a frame and preserve the caller's caller-save registers.
  OopMap* oop_map = save_live_registers(sasm);

  int call_offset;
  if (arg1 == noreg) {
    call_offset = __ call_RT(result, noreg, target);
  } else if (arg2 == noreg) {
    call_offset = __ call_RT(result, noreg, target, arg1);
  } else if (arg3 == noreg) {
    call_offset = __ call_RT(result, noreg, target, arg1, arg2);
  } else {
    call_offset = __ call_RT(result, noreg, target, arg1, arg2, arg3);
  }
  OopMapSet* oop_maps = new OopMapSet();
  oop_maps->add_gc_map(call_offset, oop_map);

  restore_live_registers(sasm, result, noreg);
  __ blr();
  return oop_maps;
}

static OopMapSet* stub_call_with_stack_parms(StubAssembler* sasm, Register result, address target,
                                             int stack_parms, bool do_return = true) {
  // Make a frame and preserve the caller's caller-save registers.
  const int parm_size_in_bytes = align_size_up(stack_parms << LogBytesPerWord, frame::alignment_in_bytes);
  const int padding = parm_size_in_bytes - (stack_parms << LogBytesPerWord);
  OopMap* oop_map = save_live_registers(sasm, true, noreg, parm_size_in_bytes);

  int call_offset = 0;
  switch (stack_parms) {
    case 3:
    __ l(R6_ARG4, frame_size_in_bytes + padding + 2*BytesPerWord, R1_SP);
    case 2:
    __ l(R5_ARG3, frame_size_in_bytes + padding + BytesPerWord, R1_SP);
    case 1:
    __ l(R4_ARG2, frame_size_in_bytes + padding + 0, R1_SP);
    call_offset = __ call_RT(result, noreg, target);
    break;
    default: Unimplemented(); break;
  }
  OopMapSet* oop_maps = new OopMapSet();
  oop_maps->add_gc_map(call_offset, oop_map);

  restore_live_registers(sasm, result, noreg);
  if (do_return) __ blr();
  return oop_maps;
}


OopMapSet* Runtime1::generate_patching(StubAssembler* sasm, address target) {
  // Make a frame and preserve the caller's caller-save registers.
  OopMap* oop_map = save_live_registers(sasm);

  // Call the runtime patching routine, returns non-zero if nmethod got deopted.
  int call_offset = __ call_RT(noreg, noreg, target);
  OopMapSet* oop_maps = new OopMapSet();
  oop_maps->add_gc_map(call_offset, oop_map);
  __ cmpi(CCR0, R3_RET, 0);

  // Re-execute the patched instruction or, if the nmethod was deoptmized,
  // return to the deoptimization handler entry that will cause re-execution
  // of the current bytecode.
  DeoptimizationBlob* deopt_blob = SharedRuntime::deopt_blob();
  assert(deopt_blob != NULL, "deoptimization blob must have been created");

  // Return to the deoptimization handler entry for unpacking and rexecute.
  // If we simply returned the we'd deopt as if any call we patched had just
  // returned.

  restore_live_registers(sasm, noreg, noreg);
  // Return if patching routine returned 0.
  __ bclr(Assembler::bcondCRbiIs1, Assembler::bi0(CCR0, Assembler::equal), Assembler::bhintbhBCLRisReturn);

  address stub = deopt_blob->unpack_with_reexecution();
#ifdef PPC64
  __ add_const_optimized(R0, R29_TOC, MacroAssembler::offset_to_global_toc(stub));
#else
  __ load_const_optimized(R0, stub);
#endif
  __ mtctr(R0);
  __ bctr();

  return oop_maps;
}

OopMapSet* Runtime1::generate_code_for(StubID id, StubAssembler* sasm) {
  OopMapSet* oop_maps = NULL;

  // For better readability.
  const bool must_gc_arguments = true;
  const bool dont_gc_arguments = false;

  // Stub code & info for the different stubs.
  switch (id) {
    case forward_exception_id:
      {
        oop_maps = generate_handle_exception(id, sasm);
      }
      break;

    case new_instance_id:
    case fast_new_instance_id:
    case fast_new_instance_init_check_id:
      {
        if (id == new_instance_id) {
          __ set_info("new_instance", dont_gc_arguments);
        } else if (id == fast_new_instance_id) {
          __ set_info("fast new_instance", dont_gc_arguments);
        } else {
          assert(id == fast_new_instance_init_check_id, "bad StubID");
          __ set_info("fast new_instance init check", dont_gc_arguments);
        }
        // We don't support eden allocation.
//        if ((id == fast_new_instance_id || id == fast_new_instance_init_check_id) &&
//            UseTLAB && FastTLABRefill) {
//          if (id == fast_new_instance_init_check_id) {
//            // make sure the klass is initialized
//            __ lbz(R0, in_bytes(InstanceKlass::init_state_offset()), R3_ARG1);
//            __ cmpwi(CCR0, R0, InstanceKlass::fully_initialized);
//            __ bne(CCR0, slow_path);
//          }
//#ifdef ASSERT
//          // assert object can be fast path allocated
//          {
//            Label ok, not_ok;
//          __ lwz(R0, in_bytes(Klass::layout_helper_offset()), R3_ARG1);
//          // make sure it's an instance (LH > 0)
//          __ cmpwi(CCR0, R0, 0);
//          __ ble(CCR0, not_ok);
//          __ testbitdi(CCR0, R0, R0, Klass::_lh_instance_slow_path_bit);
//          __ beq(CCR0, ok);
//
//          __ bind(not_ok);
//          __ stop("assert(can be fast path allocated)");
//          __ bind(ok);
//          }
//#endif // ASSERT
//          // We don't support eden allocation.
//          __ bind(slow_path);
//        }
        oop_maps = generate_stub_call(sasm, R3_RET, CAST_FROM_FN_PTR(address, new_instance), R4_ARG2);
      }
      break;

    case counter_overflow_id:
        // Bci and method are on stack.
        oop_maps = stub_call_with_stack_parms(sasm, noreg, CAST_FROM_FN_PTR(address, counter_overflow), 2);
      break;

    case new_type_array_id:
    case new_object_array_id:
      {
        if (id == new_type_array_id) {
          __ set_info("new_type_array", dont_gc_arguments);
        } else {
          __ set_info("new_object_array", dont_gc_arguments);
        }

#ifdef ASSERT
        // Assert object type is really an array of the proper kind.
        {
          int tag = (id == new_type_array_id) ? Klass::_lh_array_tag_type_value : Klass::_lh_array_tag_obj_value;
          Label ok;
          __ lwz(R0, in_bytes(Klass::layout_helper_offset()), R4_ARG2);
          __ srawi(R0, R0, Klass::_lh_array_tag_shift);
          __ cmpwi(CCR0, R0, tag);
          __ beq(CCR0, ok);
          __ stop("assert(is an array klass)");
          __ should_not_reach_here();
          __ bind(ok);
        }
#endif // ASSERT

        // We don't support eden allocation.

        if (id == new_type_array_id) {
          oop_maps = generate_stub_call(sasm, R3_RET, CAST_FROM_FN_PTR(address, new_type_array), R4_ARG2, R5_ARG3);
        } else {
          oop_maps = generate_stub_call(sasm, R3_RET, CAST_FROM_FN_PTR(address, new_object_array), R4_ARG2, R5_ARG3);
        }
      }
      break;

    case new_multi_array_id:
      {
        // R4: klass
        // R5: rank
        // R6: address of 1st dimension
        __ set_info("new_multi_array", dont_gc_arguments);
        oop_maps = generate_stub_call(sasm, R3_RET, CAST_FROM_FN_PTR(address, new_multi_array), R4_ARG2, R5_ARG3, R6_ARG4);
      }
      break;

    case register_finalizer_id:
      {
        __ set_info("register_finalizer", dont_gc_arguments);
        // This code is called via rt_call. Hence, caller-save registers have been saved.
        Register t = R11_scratch1;

        // Load the klass and check the has finalizer flag.
        __ load_klass(t, R3_ARG1);
        __ lwz(t, in_bytes(Klass::access_flags_offset()), t);
        __ testbitdi(CCR0, R0, t, exact_log2(JVM_ACC_HAS_FINALIZER));
        // Return if has_finalizer bit == 0 (CR0.eq).
        __ bclr(Assembler::bcondCRbiIs1, Assembler::bi0(CCR0, Assembler::equal), Assembler::bhintbhBCLRisReturn);

        __ mflr(R0);
        __ st(R0, _abi(lr), R1_SP);
        __ push_frame(frame::abi_reg_args_size, R0); // Empty dummy frame (no callee-save regs).
        sasm->set_frame_size(frame::abi_reg_args_size / BytesPerWord);
        OopMap* oop_map = new OopMap(frame::abi_reg_args_size / sizeof(jint), 0);
        int call_offset = __ call_RT(noreg, noreg,
                                     CAST_FROM_FN_PTR(address, SharedRuntime::register_finalizer), R3_ARG1);
        oop_maps = new OopMapSet();
        oop_maps->add_gc_map(call_offset, oop_map);

        __ pop_frame();
        __ l(R0, _abi(lr), R1_SP);
        __ mtlr(R0);
        __ blr();
      }
      break;

    case throw_range_check_failed_id:
      {
        __ set_info("range_check_failed", dont_gc_arguments); // Arguments will be discarded.
        __ st(R0, -BytesPerWord, R1_SP); // Pass index on stack.
        oop_maps = generate_exception_throw_with_stack_parms(sasm, CAST_FROM_FN_PTR(address, throw_range_check_exception), 1);
      }
      break;

    case throw_index_exception_id:
      {
        __ set_info("index_range_check_failed", dont_gc_arguments); // Arguments will be discarded.
        oop_maps = generate_exception_throw(sasm, CAST_FROM_FN_PTR(address, throw_index_exception), true);
      }
      break;

    case throw_div0_exception_id:
      {
        __ set_info("throw_div0_exception", dont_gc_arguments);
        oop_maps = generate_exception_throw(sasm, CAST_FROM_FN_PTR(address, throw_div0_exception), false);
      }
      break;

    case throw_null_pointer_exception_id:
      {
        __ set_info("throw_null_pointer_exception", dont_gc_arguments);
        oop_maps = generate_exception_throw(sasm, CAST_FROM_FN_PTR(address, throw_null_pointer_exception), false);
      }
      break;

    case handle_exception_nofpu_id:
    case handle_exception_id:
      {
        __ set_info("handle_exception", dont_gc_arguments);
        oop_maps = generate_handle_exception(id, sasm);
      }
      break;

    case handle_exception_from_callee_id:
      {
        __ set_info("handle_exception_from_callee", dont_gc_arguments);
        oop_maps = generate_handle_exception(id, sasm);
      }
      break;

    case unwind_exception_id:
      {
        const Register Rexception    = R3 /*LIRGenerator::exceptionOopOpr()*/,
                       Rexception_pc = R4 /*LIRGenerator::exceptionPcOpr()*/,
                       Rexception_save = R31, Rcaller_sp = R30;
        __ set_info("unwind_exception", dont_gc_arguments);

        __ l(Rcaller_sp, 0, R1_SP);
        __ push_frame_reg_args(0, R0); // dummy frame for C call
        __ mr(Rexception_save, Rexception); // save over C call
        __ l(Rexception_pc, _abi(lr), Rcaller_sp); // return pc
        __ call_VM_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::exception_handler_for_return_address), R16_thread, Rexception_pc);
        __ verify_not_null_oop(Rexception_save);
        __ mtctr(R3_RET);
        __ l(Rexception_pc, _abi(lr), Rcaller_sp); // return pc
        __ mr(R1_SP, Rcaller_sp); // Pop both frames at once.
        __ mr(Rexception, Rexception_save); // restore
        __ mtlr(Rexception_pc);
        __ bctr();
      }
      break;

    case throw_array_store_exception_id:
      {
        __ set_info("throw_array_store_exception", dont_gc_arguments);
        oop_maps = generate_exception_throw(sasm, CAST_FROM_FN_PTR(address, throw_array_store_exception), true);
      }
      break;

    case throw_class_cast_exception_id:
      {
        __ set_info("throw_class_cast_exception", dont_gc_arguments);
        oop_maps = generate_exception_throw(sasm, CAST_FROM_FN_PTR(address, throw_class_cast_exception), true);
      }
      break;

    case throw_incompatible_class_change_error_id:
      {
        __ set_info("throw_incompatible_class_cast_exception", dont_gc_arguments);
        oop_maps = generate_exception_throw(sasm, CAST_FROM_FN_PTR(address, throw_incompatible_class_change_error), false);
      }
      break;

    case slow_subtype_check_id:
      { // Support for uint StubRoutine::partial_subtype_check( Klass sub, Klass super );
        const Register sub_klass = R5,
                       super_klass = R4,
                       temp1_reg = R6,
                       temp2_reg = R0;
        __ check_klass_subtype_slow_path(sub_klass, super_klass, temp1_reg, temp2_reg); // returns with CR0.eq if successful
        __ crandc(CCR0, Assembler::equal, CCR0, Assembler::equal); // failed: CR0.ne
        __ blr();
      }
      break;

    case monitorenter_nofpu_id:
    case monitorenter_id:
      {
        __ set_info("monitorenter", dont_gc_arguments);

        int save_fpu_registers = (id == monitorenter_id);
        // Make a frame and preserve the caller's caller-save registers.
        OopMap* oop_map = save_live_registers(sasm, save_fpu_registers);

        int call_offset = __ call_RT(noreg, noreg, CAST_FROM_FN_PTR(address, monitorenter), R4_ARG2, R5_ARG3);

        oop_maps = new OopMapSet();
        oop_maps->add_gc_map(call_offset, oop_map);

        restore_live_registers(sasm, noreg, noreg, save_fpu_registers);
        __ blr();
      }
      break;

    case monitorexit_nofpu_id:
    case monitorexit_id:
      {
        // note: Really a leaf routine but must setup last java sp
        //       => use call_RT for now (speed can be improved by
        //       doing last java sp setup manually).
        __ set_info("monitorexit", dont_gc_arguments);

        int save_fpu_registers = (id == monitorexit_id);
        // Make a frame and preserve the caller's caller-save registers.
        OopMap* oop_map = save_live_registers(sasm, save_fpu_registers);

        int call_offset = __ call_RT(noreg, noreg, CAST_FROM_FN_PTR(address, monitorexit), R4_ARG2);

        oop_maps = new OopMapSet();
        oop_maps->add_gc_map(call_offset, oop_map);

        restore_live_registers(sasm, noreg, noreg, save_fpu_registers);
        __ blr();
      }
      break;

    case deoptimize_id:
      {
        __ set_info("deoptimize", dont_gc_arguments);
        __ st(R0, -8, R1_SP); // Pass trap_request on stack.
        oop_maps = stub_call_with_stack_parms(sasm, noreg, CAST_FROM_FN_PTR(address, deoptimize), 1, /*do_return*/ false);

        DeoptimizationBlob* deopt_blob = SharedRuntime::deopt_blob();
        assert(deopt_blob != NULL, "deoptimization blob must have been created");
        address stub = deopt_blob->unpack_with_reexecution();
#ifdef PPC64
        __ add_const_optimized(R0, R29_TOC, MacroAssembler::offset_to_global_toc(stub));
#else
        __ load_const_optimized(R0, stub);
#endif
        __ mtctr(R0);
        __ bctr();
      }
      break;

    case access_field_patching_id:
      {
        __ set_info("access_field_patching", dont_gc_arguments);
        oop_maps = generate_patching(sasm, CAST_FROM_FN_PTR(address, access_field_patching));
      }
      break;

    case load_klass_patching_id:
      {
        __ set_info("load_klass_patching", dont_gc_arguments);
        oop_maps = generate_patching(sasm, CAST_FROM_FN_PTR(address, move_klass_patching));
      }
      break;

    case load_mirror_patching_id:
      {
        __ set_info("load_mirror_patching", dont_gc_arguments);
        oop_maps = generate_patching(sasm, CAST_FROM_FN_PTR(address, move_mirror_patching));
      }
      break;

    case load_appendix_patching_id:
      {
        __ set_info("load_appendix_patching", dont_gc_arguments);
        oop_maps = generate_patching(sasm, CAST_FROM_FN_PTR(address, move_appendix_patching));
      }
      break;

    case dtrace_object_alloc_id:
      { // O0: object
        __ unimplemented("stub dtrace_object_alloc_id");
        __ set_info("dtrace_object_alloc", dont_gc_arguments);
//        // We can't gc here so skip the oopmap but make sure that all
//        // the live registers get saved.
//        save_live_registers(sasm);
//
//        __ save_thread(L7_thread_cache);
//        __ call(CAST_FROM_FN_PTR(address, SharedRuntime::dtrace_object_alloc),
//                relocInfo::runtime_call_type);
//        __ delayed()->mov(I0, O0);
//        __ restore_thread(L7_thread_cache);
//
//        restore_live_registers(sasm);
//        __ ret();
//        __ delayed()->restore();
      }
      break;

#if INCLUDE_ALL_GCS
    case g1_pre_barrier_slow_id:
      {
        BarrierSet* bs = Universe::heap()->barrier_set();
        if (bs->kind() != BarrierSet::G1SATBCTLogging) {
          goto unimplemented_entry;
        }

        __ set_info("g1_pre_barrier_slow_id", dont_gc_arguments);

        // Using stack slots: pre_val (pre-pushed), spill tmp, spill tmp2.
        // ppc32: alignment required to push/pop stack_slots with volatile regs
        const int stack_slots = PPC64_ONLY(3) NOT_PPC64(4);
        Register pre_val = R0; // previous value of memory
        Register tmp  = R14;
        Register tmp2 = R15;

        Label refill, restart;
        int satb_q_index_byte_offset =
          in_bytes(JavaThread::satb_mark_queue_offset() +
                   PtrQueue::byte_offset_of_index());
        int satb_q_buf_byte_offset =
          in_bytes(JavaThread::satb_mark_queue_offset() +
                   PtrQueue::byte_offset_of_buf());

        // Spill
        __ st(tmp, -2*BytesPerWord, R1_SP);
        __ st(tmp2, -3*BytesPerWord, R1_SP);

        __ bind(restart);
        // Load the index into the SATB buffer. PtrQueue::_index is a
        // size_t so ld_ptr is appropriate.
        __ l(tmp, satb_q_index_byte_offset, R16_thread);

        // index == 0?
        __ cmpi(CCR0, tmp, 0);
        __ beq(CCR0, refill);

        __ l(tmp2, satb_q_buf_byte_offset, R16_thread);
        __ l(pre_val, -BytesPerWord, R1_SP); // Load from stack.
        __ addi(tmp, tmp, -oopSize);

        __ st(tmp, satb_q_index_byte_offset, R16_thread);
        __ stx(pre_val, tmp2, tmp); // [_buf + index] := <address_of_card>

        // Restore temp registers and return-from-leaf.
        __ l(tmp2, -3*BytesPerWord, R1_SP);
        __ l(tmp, -2*BytesPerWord, R1_SP);
        __ blr();

        __ bind(refill);
        const int nbytes_save = MacroAssembler::volatile_regs_size + stack_slots* BytesPerWord;
        __ save_volatile_gprs(R1_SP, -nbytes_save); // except R0
        __ mflr(R0);
        __ st(R0, _abi(lr), R1_SP);
        __ push_frame_reg_args(nbytes_save, R0); // dummy frame for C call
        __ call_VM_leaf(CAST_FROM_FN_PTR(address, SATBMarkQueueSet::handle_zero_index_for_thread), R16_thread);
        __ pop_frame();
        __ l(R0, _abi(lr), R1_SP);
        __ mtlr(R0);
        __ restore_volatile_gprs(R1_SP, -nbytes_save); // except R0
        __ b(restart);
      }
      break;

  case g1_post_barrier_slow_id:
    {
        BarrierSet* bs = Universe::heap()->barrier_set();
        if (bs->kind() != BarrierSet::G1SATBCTLogging) {
          goto unimplemented_entry;
        }

        __ set_info("g1_post_barrier_slow_id", dont_gc_arguments);

        // Using stack slots: spill addr, spill tmp2
        const int stack_slots = 2;
        Register tmp = R0;
        Register addr = R14;
        Register tmp2 = R15;
        jbyte* byte_map_base = ((CardTableModRefBS*)bs)->byte_map_base;

        Label restart, refill, ret;

        // Spill
        __ st(addr, -BytesPerWord, R1_SP);
        __ st(tmp2, -2*BytesPerWord, R1_SP);

        __ sri(addr, R0, CardTableModRefBS::card_shift); // Addr is passed in R0.
        __ load_const_optimized(/*cardtable*/ tmp2, byte_map_base, tmp);
        __ add(addr, tmp2, addr);
        __ lbz(tmp, 0, addr); // tmp := [addr + cardtable]

        // Return if young card.
        __ cmpwi(CCR0, tmp, G1SATBCardTableModRefBS::g1_young_card_val());
        __ beq(CCR0, ret);

        // Return if sequential consistent value is already dirty.
        __ membar(Assembler::StoreLoad);
        __ lbz(tmp, 0, addr); // tmp := [addr + cardtable]

        __ cmpwi(CCR0, tmp, G1SATBCardTableModRefBS::dirty_card_val());
        __ beq(CCR0, ret);

        // Not dirty.

        // First, dirty it.
        __ li(tmp, G1SATBCardTableModRefBS::dirty_card_val());
        __ stb(tmp, 0, addr);

        int dirty_card_q_index_byte_offset =
          in_bytes(JavaThread::dirty_card_queue_offset() +
                   DirtyCardQueue::byte_offset_of_index());
        int dirty_card_q_buf_byte_offset =
          in_bytes(JavaThread::dirty_card_queue_offset() +
                   DirtyCardQueue::byte_offset_of_buf());

        __ bind(restart);

        // Get the index into the update buffer. DirtyCardQueue::_index is
        // a size_t so ld_ptr is appropriate here.
        __ l(tmp2, dirty_card_q_index_byte_offset, R16_thread);

        // index == 0?
        __ cmpi(CCR0, tmp2, 0);
        __ beq(CCR0, refill);

        __ l(tmp, dirty_card_q_buf_byte_offset, R16_thread);
        __ addi(tmp2, tmp2, -oopSize);

        __ st(tmp2, dirty_card_q_index_byte_offset, R16_thread);
        __ add(tmp2, tmp, tmp2);
        __ st(addr, 0, tmp2); // [_buf + index] := <address_of_card>

        // Restore temp registers and return-from-leaf.
        __ bind(ret);
        __ l(tmp2, -2*BytesPerWord, R1_SP);
        __ l(addr, -BytesPerWord, R1_SP);
        __ blr();

        __ bind(refill);
        const int nbytes_save = MacroAssembler::volatile_regs_size + stack_slots* BytesPerWord;
        __ save_volatile_gprs(R1_SP, -nbytes_save); // except R0
        __ mflr(R0);
        __ st(R0, _abi(lr), R1_SP);
        __ push_frame_reg_args(nbytes_save, R0); // dummy frame for C call
        __ call_VM_leaf(CAST_FROM_FN_PTR(address, DirtyCardQueueSet::handle_zero_index_for_thread), R16_thread);
        __ pop_frame();
        __ l(R0, _abi(lr), R1_SP);
        __ mtlr(R0);
        __ restore_volatile_gprs(R1_SP, -nbytes_save); // except R0
        __ b(restart);
      }
      break;
#endif // INCLUDE_ALL_GCS

    case predicate_failed_trap_id:
      {
        __ set_info("predicate_failed_trap", dont_gc_arguments);
        OopMap* oop_map = save_live_registers(sasm);

        int call_offset = __ call_RT(noreg, noreg, CAST_FROM_FN_PTR(address, predicate_failed_trap));

        oop_maps = new OopMapSet();
        oop_maps->add_gc_map(call_offset, oop_map);

        DeoptimizationBlob* deopt_blob = SharedRuntime::deopt_blob();
        assert(deopt_blob != NULL, "deoptimization blob must have been created");
        restore_live_registers(sasm, noreg, noreg);

        address stub = deopt_blob->unpack_with_reexecution();
#ifdef PPC64
        __ add_const_optimized(R0, R29_TOC, MacroAssembler::offset_to_global_toc(stub));
#else
        __ load_const_optimized(R0, stub);
#endif
        __ mtctr(R0);
        __ bctr();
      }
      break;

  default:
  unimplemented_entry:
      {
        __ set_info("unimplemented entry", dont_gc_arguments);
        __ mflr(R0);
        __ st(R0, _abi(lr), R1_SP);
        __ push_frame(frame::abi_reg_args_size, R0); // empty dummy frame
        sasm->set_frame_size(frame::abi_reg_args_size / BytesPerWord);
        OopMap* oop_map = new OopMap(frame::abi_reg_args_size / sizeof(jint), 0);

        __ load_const_optimized(R4_ARG2, (int)id);
        int call_offset = __ call_RT(noreg, noreg, CAST_FROM_FN_PTR(address, unimplemented_entry), R4_ARG2);

        oop_maps = new OopMapSet();
        oop_maps->add_gc_map(call_offset, oop_map);
        __ should_not_reach_here();
      }
      break;
  }
  return oop_maps;
}


OopMapSet* Runtime1::generate_handle_exception(StubID id, StubAssembler* sasm) {
  __ block_comment("generate_handle_exception");

  // Save registers, if required.
  OopMapSet* oop_maps = new OopMapSet();
  OopMap* oop_map = NULL;
  const Register Rexception    = R3 /*LIRGenerator::exceptionOopOpr()*/,
                 Rexception_pc = R4 /*LIRGenerator::exceptionPcOpr()*/;

  switch (id) {
  case forward_exception_id:
    // We're handling an exception in the context of a compiled frame.
    // The registers have been saved in the standard places. Perform
    // an exception lookup in the caller and dispatch to the handler
    // if found. Otherwise unwind and dispatch to the callers
    // exception handler.
    oop_map = generate_oop_map(sasm, true);
    // Transfer the pending exception to the exception_oop.
    // Also load the PC which is typically at SP + frame_size_in_bytes + _abi(lr),
    // but we support additional slots in the frame for parameter passing.
    __ l(Rexception_pc, 0, R1_SP);
    __ l(Rexception, in_bytes(JavaThread::pending_exception_offset()), R16_thread);
    __ li(R0, 0);
    __ l(Rexception_pc, _abi(lr), Rexception_pc);
    __ st(R0, in_bytes(JavaThread::pending_exception_offset()), R16_thread);
    break;
  case handle_exception_nofpu_id:
  case handle_exception_id:
    // At this point all registers MAY be live.
    oop_map = save_live_registers(sasm, id != handle_exception_nofpu_id, Rexception_pc);
    break;
  case handle_exception_from_callee_id:
    // At this point all registers except exception oop and exception pc are dead.
    oop_map = new OopMap(frame_size_in_bytes / sizeof(jint), 0);
    sasm->set_frame_size(frame_size_in_bytes / BytesPerWord);
    __ st(Rexception_pc, _abi(lr), R1_SP);
    __ push_frame(frame_size_in_bytes, R0);
    break;
  default:  ShouldNotReachHere();
  }

  __ verify_not_null_oop(Rexception);

#ifdef ASSERT
  // Check that fields in JavaThread for exception oop and issuing pc are
  // empty before writing to them.
  __ l(R0, in_bytes(JavaThread::exception_oop_offset()), R16_thread);
  __ cmpi(CCR0, R0, 0);
  __ asm_assert_eq("exception oop already set", 0x963);
  __ l(R0, in_bytes(JavaThread::exception_pc_offset() ), R16_thread);
  __ cmpi(CCR0, R0, 0);
  __ asm_assert_eq("exception pc already set", 0x962);
#endif

  // Save the exception and issuing pc in the thread.
  __ st(Rexception,    in_bytes(JavaThread::exception_oop_offset()), R16_thread);
  __ st(Rexception_pc, in_bytes(JavaThread::exception_pc_offset() ), R16_thread);

  int call_offset = __ call_RT(noreg, noreg, CAST_FROM_FN_PTR(address, exception_handler_for_pc));
  oop_maps->add_gc_map(call_offset, oop_map);

  __ mtctr(R3_RET);

  // Note: if nmethod has been deoptimized then regardless of
  // whether it had a handler or not we will deoptimize
  // by entering the deopt blob with a pending exception.

  // Restore the registers that were saved at the beginning, remove
  // the frame and jump to the exception handler.
  switch (id) {
  case forward_exception_id:
  case handle_exception_nofpu_id:
  case handle_exception_id:
    restore_live_registers(sasm, noreg, noreg, id != handle_exception_nofpu_id);
    __ bctr();
    break;
  case handle_exception_from_callee_id: {
    __ pop_frame();
    __ l(Rexception_pc, _abi(lr), R1_SP);
    __ mtlr(Rexception_pc);
    __ bctr();
    break;
  }
  default:  ShouldNotReachHere();
  }

  return oop_maps;
}

const char *Runtime1::pd_name_for_address(address entry) {
  return "<unknown function>";
}

#undef __
