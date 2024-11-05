/*
 * Copyright (c) 2000, 2019, Oracle and/or its affiliates. All rights reserved.
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
#include "c1/c1_Compilation.hpp"
#include "c1/c1_LIRAssembler.hpp"
#include "c1/c1_MacroAssembler.hpp"
#include "c1/c1_Runtime1.hpp"
#include "c1/c1_ValueStack.hpp"
#include "ci/ciArrayKlass.hpp"
#include "ci/ciInstance.hpp"
#include "gc_interface/collectedHeap.hpp"
#include "memory/barrierSet.hpp"
#include "memory/cardTableModRefBS.hpp"
#include "nativeInst_ppc.hpp"
#include "oops/objArrayKlass.hpp"
#include "runtime/sharedRuntime.hpp"
#include "register_ppc.hpp"

#define __ _masm->

void LIR_Assembler::prefetchr(LIR_Opr src) {
  ShouldNotReachHere();
}


void LIR_Assembler::prefetchw(LIR_Opr src) {
  ShouldNotReachHere();
}

const ConditionRegister LIR_Assembler::BOOL_RESULT = CCR5;


bool LIR_Assembler::is_small_constant(LIR_Opr opr) {
  Unimplemented(); return false; // Currently not used on this platform.
}


LIR_Opr LIR_Assembler::receiverOpr() {
  return FrameMap::R3_oop_opr;
}


LIR_Opr LIR_Assembler::osrBufferPointer() {
  return FrameMap::R3_opr;
}


// This specifies the stack pointer decrement needed to build the frame.
int LIR_Assembler::initial_frame_size_in_bytes() const {
  return in_bytes(frame_map()->framesize_in_bytes());
}


// Inline cache check: the inline cached class is in inline_cache_reg;
// we fetch the class of the receiver and compare it with the cached class.
// If they do not match we jump to slow case.
int LIR_Assembler::check_icache() {
  int offset = __ offset();
  __ inline_cache_check(R3_ARG1, R19_inline_cache_reg);
  return offset;
}


void LIR_Assembler::osr_entry() {
  // On-stack-replacement entry sequence:
  //
  //   1. Create a new compiled activation.
  //   2. Initialize local variables in the compiled activation. The expression
  //      stack must be empty at the osr_bci; it is not initialized.
  //   3. Jump to the continuation address in compiled code to resume execution.

  // OSR entry point
  offsets()->set_value(CodeOffsets::OSR_Entry, code_offset());
  BlockBegin* osr_entry = compilation()->hir()->osr_entry();
  ValueStack* entry_state = osr_entry->end()->state();
  int number_of_locks = entry_state->locks_size();

  // Create a frame for the compiled activation.
  __ build_frame(initial_frame_size_in_bytes(), bang_size_in_bytes());

  // OSR buffer is
  //
  // locals[nlocals-1..0]
  // monitors[number_of_locks-1..0]
  //
  // Locals is a direct copy of the interpreter frame so in the osr buffer
  // the first slot in the local array is the last local from the interpreter
  // and the last slot is local[0] (receiver) from the interpreter.
  //
  // Similarly with locks. The first lock slot in the osr buffer is the nth lock
  // from the interpreter frame, the nth lock slot in the osr buffer is 0th lock
  // in the interpreter frame (the method lock if a sync method).

  // Initialize monitors in the compiled activation.
  //   R3: pointer to osr buffer
  //
  // All other registers are dead at this point and the locals will be
  // copied into place by code emitted in the IR.

  Register OSR_buf = osrBufferPointer()->as_register();
  {
    // Buffer have monitors packed, packed size should not be equal
    // to interpreter_monitor_size (which is padded until stack_alignment)
    // I don't know why ppc and other cpus are requiring it
    // assert(frame::interpreter_frame_monitor_size() == BasicObjectLock::size(), "adjust code below");
    int monitor_offset = BytesPerWord * method()->max_locals() +
      (2 * BytesPerWord) * (number_of_locks - 1);
    // SharedRuntime::OSR_migration_begin() packs BasicObjectLocks in
    // the OSR buffer using 2 word entries: first the lock and then
    // the oop.
    for (int i = 0; i < number_of_locks; i++) {
      int slot_offset = monitor_offset - ((i * 2) * BytesPerWord);
#ifdef ASSERT
      // Verify the interpreter's monitor has a non-null object.
      {
        Label L;
        __ l(R0, slot_offset + 1*BytesPerWord, OSR_buf);
        __ cmpi(CCR0, R0, 0);
        __ bne(CCR0, L);
        __ stop("locked object is NULL");
        __ bind(L);
      }
#endif // ASSERT
      // Copy the lock field into the compiled activation.
      Address ml = frame_map()->address_for_monitor_lock(i),
              mo = frame_map()->address_for_monitor_object(i);
      assert(ml.index() == noreg && mo.index() == noreg, "sanity");
      __ l(R0, slot_offset + 0, OSR_buf);
      __ st(R0, ml.disp(), ml.base());
      __ l(R0, slot_offset + 1*BytesPerWord, OSR_buf);
      __ st(R0, mo.disp(), mo.base());
    }
  }
}


int LIR_Assembler::emit_exception_handler() {
  // If the last instruction is a call (typically to do a throw which
  // is coming at the end after block reordering) the return address
  // must still point into the code area in order to avoid assertion
  // failures when searching for the corresponding bci => add a nop
  // (was bug 5/14/1999 - gri).
  __ nop();

  // Generate code for the exception handler.
  address handler_base = __ start_a_stub(exception_handler_size);

  if (handler_base == NULL) {
    // Not enough space left for the handler.
    bailout("exception handler overflow");
    return -1;
  }

  int offset = code_offset();
  address entry_point = CAST_FROM_FN_PTR(address, Runtime1::entry_for(Runtime1::handle_exception_from_callee_id));
#ifdef PPC64
  __ add_const_optimized(R0, R29_TOC, MacroAssembler::offset_to_global_toc(entry_point));
#else
  __ load_const_optimized(R0, entry_point);
#endif
  __ mtctr(R0);
  __ bctr();

  guarantee(code_offset() - offset <= exception_handler_size, "overflow");
  __ end_a_stub();

  return offset;
}


// Emit the code to remove the frame from the stack in the exception
// unwind path.
int LIR_Assembler::emit_unwind_handler() {
  _masm->block_comment("Unwind handler");

  int offset = code_offset();
  bool preserve_exception = method()->is_synchronized() || compilation()->env()->dtrace_method_probes();
  const Register Rexception = R3 /*LIRGenerator::exceptionOopOpr()*/, Rexception_save = R31;

  // Fetch the exception from TLS and clear out exception related thread state.
  __ l(Rexception, in_bytes(JavaThread::exception_oop_offset()), R16_thread);
  __ li(R0, 0);
  __ st(R0, in_bytes(JavaThread::exception_oop_offset()), R16_thread);
  __ st(R0, in_bytes(JavaThread::exception_pc_offset()), R16_thread);

  __ bind(_unwind_handler_entry);
  __ verify_not_null_oop(Rexception);
  if (preserve_exception) { __ mr(Rexception_save, Rexception); }

  // Perform needed unlocking
  MonitorExitStub* stub = NULL;
  if (method()->is_synchronized()) {
    monitor_address(0, FrameMap::R4_opr);
    stub = new MonitorExitStub(FrameMap::R4_opr, true, 0);
    __ unlock_object(R5, R6, R4, *stub->entry());
    __ bind(*stub->continuation());
  }

  if (compilation()->env()->dtrace_method_probes()) {
    Unimplemented();
  }

  // Dispatch to the unwind logic.
  address unwind_stub = Runtime1::entry_for(Runtime1::unwind_exception_id);
#ifdef PPC64
  __ add_const_optimized(R0, R29_TOC, MacroAssembler::offset_to_global_toc(unwind_stub));
#else
  __ load_const_optimized(R0, unwind_stub);
#endif
  if (preserve_exception) { __ mr(Rexception, Rexception_save); }
  __ mtctr(R0);
  __ bctr();

  // Emit the slow path assembly.
  if (stub != NULL) {
    stub->emit_code(this);
  }

  return offset;
}


int LIR_Assembler::emit_deopt_handler() {
  // If the last instruction is a call (typically to do a throw which
  // is coming at the end after block reordering) the return address
  // must still point into the code area in order to avoid assertion
  // failures when searching for the corresponding bci => add a nop
  // (was bug 5/14/1999 - gri).
  __ nop();

  // Generate code for deopt handler.
  address handler_base = __ start_a_stub(deopt_handler_size);

  if (handler_base == NULL) {
    // Not enough space left for the handler.
    bailout("deopt handler overflow");
    return -1;
  }

  int offset = code_offset();
  __ bl64_patchable(SharedRuntime::deopt_blob()->unpack(), relocInfo::runtime_call_type);

  guarantee(code_offset() - offset <= deopt_handler_size, "overflow");
  __ end_a_stub();

  return offset;
}


void LIR_Assembler::jobject2reg(jobject o, Register reg) {
  if (o == NULL) {
    __ li(reg, 0);
  } else {
    AddressLiteral addrlit = __ constant_oop_address(o);
    __ load_const(reg, addrlit, (reg != R0) ? R0 : noreg);
  }
}


void LIR_Assembler::jobject2reg_with_patching(Register reg, CodeEmitInfo *info) {
  // Allocate a new index in table to hold the object once it's been patched.
  int oop_index = __ oop_recorder()->allocate_oop_index(NULL);
  PatchingStub* patch = new PatchingStub(_masm, patching_id(info), oop_index);

  AddressLiteral addrlit((address)NULL, oop_Relocation::spec(oop_index));
  __ load_const(reg, addrlit, R0);

  patching_epilog(patch, lir_patch_normal, reg, info);
}


void LIR_Assembler::metadata2reg(Metadata* o, Register reg) {
  AddressLiteral md = __ constant_metadata_address(o); // Notify OOP recorder (don't need the relocation)
  __ load_const_optimized(reg, md.value(), (reg != R0) ? R0 : noreg);
}


void LIR_Assembler::klass2reg_with_patching(Register reg, CodeEmitInfo *info) {
  // Allocate a new index in table to hold the klass once it's been patched.
  int index = __ oop_recorder()->allocate_metadata_index(NULL);
  PatchingStub* patch = new PatchingStub(_masm, PatchingStub::load_klass_id, index);

  AddressLiteral addrlit((address)NULL, metadata_Relocation::spec(index));
  assert(addrlit.rspec().type() == relocInfo::metadata_type, "must be an metadata reloc");
  __ load_const(reg, addrlit, R0);

  patching_epilog(patch, lir_patch_normal, reg, info);
}


void LIR_Assembler::emit_op3(LIR_Op3* op) {
  const bool is_int = op->result_opr()->is_single_cpu();
  Register Rdividend = is_int ? op->in_opr1()->as_register() : op->in_opr1()->as_register_lo();
#ifndef PPC64
  Register Rdividend_hi = op->in_opr1()->is_single_cpu() ? noreg : op->in_opr1()->as_register_hi();
#endif
  Register Rdivisor  = noreg;
  Register Rscratch  = op->in_opr3()->is_single_cpu() ? op->in_opr3()->as_register() : op->in_opr3()->as_register_lo();
#ifndef PPC64
  Register Rscratch2 = op->in_opr3()->is_single_cpu() ? noreg : op->in_opr3()->as_register_hi();
#endif

  Register Rresult   = op->result_opr()->is_single_cpu() ? op->result_opr()->as_register() : op->result_opr()->as_register_lo();
#ifndef PPC64
  Register Rresult_hi = op->result_opr()->is_single_cpu() ? noreg : op->result_opr()->as_register_hi();
#endif
  long divisor = -1;
  long absDivisor = 1;

  if (op->in_opr2()->is_register()) {
    Rdivisor = is_int ? op->in_opr2()->as_register() : op->in_opr2()->as_register_lo();
  } else {
    divisor = is_int ? op->in_opr2()->as_constant_ptr()->as_jint()
                     : op->in_opr2()->as_constant_ptr()->as_jlong();
    absDivisor = divisor<0?-divisor:divisor;
  }

  assert(Rdividend != Rscratch, "");
  assert(Rdivisor  != Rscratch, "");
  assert(op->code() == lir_idiv || op->code() == lir_irem, "Must be irem or idiv");

  if (Rdivisor == noreg) {
    if (absDivisor == 1) { // stupid, but can happen
      if (op->code() == lir_idiv) {
        if (divisor > 0) {
          __ mr_if_needed(Rresult, Rdividend);
#ifndef PPC64
          if (Rresult_hi != noreg)
            __ mr_if_needed(Rresult_hi, Rdividend_hi);
#endif
        } else {
          if (PPC64_ONLY(true) NOT_PPC64(is_int)) {
            __ neg(Rresult, Rdividend);
#ifndef PPC64
          } else {
            __ subfic(Rresult, Rdividend, 0);
            __ subfze(Rresult_hi, Rdividend_hi);
#endif
          }
        }
      } else {
        __ li(Rresult, 0);
#ifndef PPC64
        if (Rresult_hi != noreg)
          __ li(Rresult_hi, 0);
#endif
      }
    } else if (is_power_of_2(absDivisor)) {
      // Convert division by a power of two into some shifts and logical operations.
      int log2 = log2_intptr((intptr_t)absDivisor);

      // Round towards 0.
      if (absDivisor == 2) {
        if (is_int) {
          __ srwi(Rscratch, Rdividend, 31);
        } else {
#ifdef PPC64
          __ srdi(Rscratch, Rdividend, 63);
#else
          __ srwi(Rscratch, Rdividend_hi, 31);
#endif
        }
      } else {
        if (is_int) {
          __ srawi(Rscratch, Rdividend, 31);
        } else {
#ifdef PPC64
          __ sradi(Rscratch, Rdividend, 63);
#else
          __ srawi(Rscratch, Rdividend_hi, 31);
#endif
        }
#ifdef PPC64
        __ clrldi(Rscratch, Rscratch, 64-log2);
#else
        __ rlwinm(Rscratch, Rscratch, log2, 32-log2, 31);
#endif
      }
      if (PPC64_ONLY(true) NOT_PPC64(is_int)) {
        __ add(Rscratch, Rdividend, Rscratch);
#ifndef PPC64
      } else {
        assert(Rscratch2 != noreg, "second temporary register is required");
        __ addc(Rscratch, Rdividend, Rscratch);
        __ addze(Rscratch2, Rdividend_hi);
#endif
      }

      if (op->code() == lir_idiv) {
        if (is_int) {
          __ srawi(Rresult, Rscratch, log2);
          if (divisor<0) {
            __ neg(Rresult, Rresult);
          }
        } else {
#ifdef PPC64
          __ sradi(Rresult, Rscratch, log2);
          if (divisor<0) {
            __ neg(Rresult, Rresult);
          }
#else
          __ rlwinm(Rresult, Rscratch, 32-log2, log2, 31);
          __ rlwimi(Rresult, Rscratch2, 32-log2, 0, log2-1);
          __ srawi(Rresult_hi, Rscratch2, log2);
          if (divisor<0) {
            __ subfic(Rresult, Rresult, 0);
            __ subfze(Rresult_hi, Rresult_hi);
          }
#endif
        }
      } else { // lir_irem
        __ clrri(Rscratch, Rscratch, log2);
        if (PPC64_ONLY(true) NOT_PPC64(is_int)) {
          __ sub(Rresult, Rdividend, Rscratch);
#ifndef PPC64
        } else {
          __ subfc(Rresult, Rscratch, Rdividend);
          __ subfe(Rresult_hi, Rscratch2, Rdividend_hi);
#endif
        }
      }
    } else {
      __ load_const_optimized(Rscratch, divisor);
      if (op->code() == lir_idiv) {
        if (is_int) {
          __ divw(Rresult, Rdividend, Rscratch); // Can't divide minint/-1.
        } else {
#ifdef PPC64
          __ div(Rresult, Rdividend, Rscratch); // Can't divide minint/-1.
#else
          ShouldNotReachHere();
#endif
        }
      } else {
        assert(Rscratch != R0, "need both");
        if (is_int) {
          __ divw(R0, Rdividend, Rscratch); // Can't divide minint/-1.
          __ mullw(Rscratch, R0, Rscratch);
        } else {
#ifdef PPC64
          __ div(R0, Rdividend, Rscratch); // Can't divide minint/-1.
          __ mull(Rscratch, R0, Rscratch);
#else
          ShouldNotReachHere();
#endif
        }
        __ sub(Rresult, Rdividend, Rscratch);
      }

    }
    return;
  }

  Label regular, done;
  if (is_int) {
    __ cmpwi(CCR0, Rdivisor, -1);
  } else {
#ifdef PPC64
    __ cmpi(CCR0, Rdivisor, -1);
#else
      ShouldNotReachHere();
#endif
  }
  __ bne(CCR0, regular);
  if (op->code() == lir_idiv) {
    __ neg(Rresult, Rdividend);
    __ b(done);
    __ bind(regular);
    if (is_int) {
      __ divw(Rresult, Rdividend, Rdivisor); // Can't divide minint/-1.
    } else {
      __ div(Rresult, Rdividend, Rdivisor); // Can't divide minint/-1.
    }
  } else { // lir_irem
    __ li(Rresult, 0);
    __ b(done);
    __ bind(regular);
    if (is_int) {
      __ divw(Rscratch, Rdividend, Rdivisor); // Can't divide minint/-1.
      __ mullw(Rscratch, Rscratch, Rdivisor);
    } else {
      __ div(Rscratch, Rdividend, Rdivisor); // Can't divide minint/-1.
      __ mull(Rscratch, Rscratch, Rdivisor);
    }
    __ sub(Rresult, Rdividend, Rscratch);
  }
  __ bind(done);
}


void LIR_Assembler::emit_opBranch(LIR_OpBranch* op) {
#ifdef ASSERT
  assert(op->block() == NULL || op->block()->label() == op->label(), "wrong label");
  if (op->block() != NULL)  _branch_target_blocks.append(op->block());
  if (op->ublock() != NULL) _branch_target_blocks.append(op->ublock());
  assert(op->info() == NULL, "shouldn't have CodeEmitInfo");
#endif

  Label *L = op->label();
  if (op->cond() == lir_cond_always) {
    __ b(*L);
  } else {
    Label done;
    bool is_unordered = false;
    if (op->code() == lir_cond_float_branch) {
      assert(op->ublock() != NULL, "must have unordered successor");
      is_unordered = true;
    } else {
      assert(op->code() == lir_branch, "just checking");
    }

    bool positive = false;
    Assembler::Condition cond = Assembler::equal;
    switch (op->cond()) {
      case lir_cond_equal:        positive = true ; cond = Assembler::equal  ; is_unordered = false; break;
      case lir_cond_notEqual:     positive = false; cond = Assembler::equal  ; is_unordered = false; break;
      case lir_cond_less:         positive = true ; cond = Assembler::less   ; break;
      case lir_cond_belowEqual:   assert(op->code() != lir_cond_float_branch, ""); // fallthru
      case lir_cond_lessEqual:    positive = false; cond = Assembler::greater; break;
      case lir_cond_greater:      positive = true ; cond = Assembler::greater; break;
      case lir_cond_aboveEqual:   assert(op->code() != lir_cond_float_branch, ""); // fallthru
      case lir_cond_greaterEqual: positive = false; cond = Assembler::less   ; break;
      default:                    ShouldNotReachHere();
    }
    int bo = positive ? Assembler::bcondCRbiIs1 : Assembler::bcondCRbiIs0;
    int bi = Assembler::bi0(BOOL_RESULT, cond);
    if (is_unordered) {
      if (positive) {
        if (op->ublock() == op->block()) {
          __ bc_far_optimized(Assembler::bcondCRbiIs1, __ bi0(BOOL_RESULT, Assembler::summary_overflow), *L);
        }
      } else {
        if (op->ublock() != op->block()) { __ bso(BOOL_RESULT, done); }
      }
    }
    __ bc_far_optimized(bo, bi, *L);
    __ bind(done);
  }
}


void LIR_Assembler::emit_opConvert(LIR_OpConvert* op) {
  Bytecodes::Code code = op->bytecode();
  LIR_Opr src = op->in_opr(),
          dst = op->result_opr();

  switch(code) {
    case Bytecodes::_i2l: {
#ifdef PPC64
      __ extsw(dst->as_register_lo(), src->as_register());
#else
      __ mr_if_needed(dst->as_register_lo(), src->as_register());
      __ srawi(dst->as_register_hi(), src->as_register(), 31);
#endif
      break;
    }
    case Bytecodes::_l2i: {
      __ mr_if_needed(dst->as_register(), src->as_register_lo()); // high bits are garbage
      break;
    }
    case Bytecodes::_i2b: {
      __ extsb(dst->as_register(), src->as_register());
      break;
    }
    case Bytecodes::_i2c: {
#ifdef PPC64
      __ clrldi(dst->as_register(), src->as_register(), 64-16);
#else
      __ clrlwi(dst->as_register(), src->as_register(), 32-16);
#endif
      break;
    }
    case Bytecodes::_i2s: {
      __ extsh(dst->as_register(), src->as_register());
      break;
    }
    case Bytecodes::_i2d: {
#ifdef PPC64
      __ fcfid(dst->as_double_reg(), src->as_double_reg()); // via mem
#else
#ifdef USE_SPE
      assert(dst->is_double_cpu(), "dst must be double cpu reg");
      assert(dst->as_register_hi() == dst->as_register_lo(), "dst in 1 gpr registers");
      __ efdcfsi(dst->as_register_lo(), src->as_register());
#else
      ShouldNotReachHere();
#endif
#endif
      break;
    }
    case Bytecodes::_l2d: {
#ifdef PPC64
      __ fcfid(dst->as_double_reg(), src->as_double_reg()); // via mem
#else
      ShouldNotReachHere();
#endif
      break;
    }
    case Bytecodes::_i2f: {
#ifdef PPC64
      FloatRegister rdst = dst->as_float_reg();
      FloatRegister rsrc = src->as_double_reg(); // via mem
      if (VM_Version::has_fcfids()) {
        __ fcfids(rdst, rsrc);
      } else {
        __ fcfid(rdst, rsrc);
        __ frsp(rdst, rdst);
      }
#else
#ifdef USE_SPE
      __ efscfsi(dst->as_register(), src->as_register());
#else
      ShouldNotReachHere();
#endif
#endif
      break;
    }
    case Bytecodes::_l2f: { // >= Power7
#ifdef PPC64
      assert(VM_Version::has_fcfids(), "fcfid+frsp needs fixup code to avoid rounding incompatibility");
      __ fcfids(dst->as_float_reg(), src->as_double_reg()); // via mem
#else
      ShouldNotReachHere();
#endif
      break;
    }
    case Bytecodes::_f2d: {
#ifndef USE_SPE
      __ fmr_if_needed(dst->as_double_reg(), src->as_float_reg());
#else
      assert(dst->is_double_cpu(), "dst must be double cpu reg");
      assert(dst->as_register_hi() == dst->as_register_lo(), "dst in 1 gpr registers");
      __ efdcfs(dst->as_register_lo(), src->as_register());
#endif
      break;
    }
    case Bytecodes::_d2f: {
#ifndef USE_SPE
      __ frsp(dst->as_float_reg(), src->as_double_reg());
#else
      assert(src->is_double_cpu(), "src must be double cpu reg");
      assert(src->as_register_hi() == src->as_register_lo(), "src in 1 gpr registers");
      __ efscfd(dst->as_register(), src->as_register_lo());
#endif
      break;
    }
#ifndef USE_SPE
    case Bytecodes::_d2i:
    case Bytecodes::_f2i: {
      FloatRegister rsrc = (code == Bytecodes::_d2i) ? src->as_double_reg() : src->as_float_reg();
      Address       addr = frame_map()->address_for_slot(dst->double_stack_ix());
      Label L;
      // Result must be 0 if value is NaN; test by comparing value to itself.
      __ fcmpu(CCR0, rsrc, rsrc);
      __ li(R0, 0); // 0 in case of NAN
      __ st(R0, addr.disp(), addr.base());
      __ bso(CCR0, L);
      __ fctiwz(rsrc, rsrc); // USE_KILL
      __ stfd(rsrc, addr.disp(), addr.base());
      __ bind(L);
      break;
    }
#else
    case Bytecodes::_d2i: {
      assert(src->as_register_hi() == src->as_register_lo(), "src in 1 gpr registers");

      Label LNaNorInfinity, Ldone;
      __ rotrwi(R0,src->as_register_hi(),20);
      __ clrlwi(R0,R0,21);
      __ cmpwi(CCR0, R0, 0x7FF);
      __ beq(CCR0, LNaNorInfinity);

      __ efdctsiz(dst->as_register(), src->as_register_lo());
      __ b(Ldone);

      __ bind(LNaNorInfinity);
      {
        const Register tmp = R11; // Will be preserved.
        const int nbytes_save = MacroAssembler::volatile_regs_size;
        __ save_volatile_gprs(R1_SP, -nbytes_save);
        __ save_LR(tmp);

        __ push_frame_reg_args(nbytes_save, tmp);

        __ mr_if_needed(R4_ARG2, src->as_register_lo());
        __ evmergehi(R3_ARG1, R3_ARG1, src->as_register_lo());
        __ call_c(CAST_FROM_FN_PTR(address, SharedRuntime::d2i), relocInfo::none);
        __ mr(R0, R3_RET);
        __ pop_frame();
        __ restore_LR(tmp);

        __ restore_volatile_gprs(R1_SP, -nbytes_save);
        __ mr(dst->as_register(), R0);
        __ bind(Ldone);
      }
      break;
    }
    case Bytecodes::_f2i: {
      Label LNaNorInfinity, Ldone;
      __ rotrwi(R0,src->as_register(),23);
      __ clrlwi(R0,R0,24);
      __ cmpwi(CCR0, R0, 0xFF);
      __ beq(CCR0, LNaNorInfinity);

      __ efsctsiz(dst->as_register(), src->as_register());
      __ b(Ldone);

      __ bind(LNaNorInfinity);
      {
        const Register tmp = R11; // Will be preserved.
        const int nbytes_save = MacroAssembler::volatile_regs_size;
        __ save_volatile_gprs(R1_SP, -nbytes_save);
        __ save_LR(tmp);

        __ push_frame_reg_args(nbytes_save, tmp);
        __ mr_if_needed(R3_ARG1, src->as_register());

        __ call_c(CAST_FROM_FN_PTR(address, SharedRuntime::f2i), relocInfo::none);
        __ mr(R0, R3_RET);
        __ pop_frame();
        __ restore_LR(tmp);

        __ restore_volatile_gprs(R1_SP, -nbytes_save);
        __ mr(dst->as_register(), R0);
        __ bind(Ldone);
      }
      break;
    }
#endif
    case Bytecodes::_d2l:
    case Bytecodes::_f2l: {
#ifndef USE_SPE
      FloatRegister rsrc = (code == Bytecodes::_d2l) ? src->as_double_reg() : src->as_float_reg();
      Address       addr = frame_map()->address_for_slot(dst->double_stack_ix());
      Label L;
      // Result must be 0 if value is NaN; test by comparing value to itself.
      __ fcmpu(CCR0, rsrc, rsrc);
      __ li(R0, 0); // 0 in case of NAN
      __ st(R0, addr.disp(), addr.base());
      __ bso(CCR0, L);
      __ fctidz(rsrc, rsrc); // USE_KILL
      __ stfd(rsrc, addr.disp(), addr.base());
      __ bind(L);
#else
      Unimplemented();
#endif
      break;
    }

    default: ShouldNotReachHere();
  }
}


void LIR_Assembler::align_call(LIR_Code) {
  // do nothing since all instructions are word aligned on ppc
}


bool LIR_Assembler::emit_trampoline_stub_for_call(address target, Register Rtoc) {
  int start_offset = __ offset();
  // Put the entry point as a constant into the constant pool.
  const address entry_point_toc_addr   = __ address_constant(target, RelocationHolder::none);
  if (entry_point_toc_addr == NULL) {
    bailout("const section overflow");
    return false;
  }
  const int     entry_point_toc_offset = __ offset_to_method_toc(entry_point_toc_addr);

  // Emit the trampoline stub which will be related to the branch-and-link below.
  address stub = __ emit_trampoline_stub(entry_point_toc_offset, start_offset, Rtoc);
  if (!stub) {
    bailout("no space for trampoline stub");
    return false;
  }
  return true;
}


void LIR_Assembler::call(LIR_OpJavaCall* op, relocInfo::relocType rtype) {
  assert(rtype==relocInfo::opt_virtual_call_type || rtype==relocInfo::static_call_type, "unexpected rtype");

  bool success = emit_trampoline_stub_for_call(op->addr());
  if (!success) { return; }

  __ relocate(rtype);
  // Note: At this point we do not have the address of the trampoline
  // stub, and the entry point might be too far away for bl, so __ pc()
  // serves as dummy and the bl will be patched later.
  __ code()->set_insts_mark();
  __ bl(__ pc());
  add_call_info(code_offset(), op->info());
}


void LIR_Assembler::ic_call(LIR_OpJavaCall* op) {
  __ calculate_address_from_global_toc(R11_scratch1, __ method_toc());

  // Virtual call relocation will point to ic load.
  address virtual_call_meta_addr = __ pc();
  // Load a clear inline cache.
  AddressLiteral empty_ic((address) Universe::non_oop_word());
  bool success = __ load_const_from_method_toc(R19_inline_cache_reg, empty_ic, R11_scratch1);
  if (!success) {
    bailout("const section overflow");
    return;
  }
  // Call to fixup routine. Fixup routine uses ScopeDesc info
  // to determine who we intended to call.
  __ relocate(virtual_call_Relocation::spec(virtual_call_meta_addr));

  success = emit_trampoline_stub_for_call(op->addr(), R11_scratch1);
  if (!success) { return; }

  // Note: At this point we do not have the address of the trampoline
  // stub, and the entry point might be too far away for bl, so __ pc()
  // serves as dummy and the bl will be patched later.
  __ bl(__ pc());
  add_call_info(code_offset(), op->info());
}


void LIR_Assembler::vtable_call(LIR_OpJavaCall* op) {
  ShouldNotReachHere(); // ic_call is used instead.
}


void LIR_Assembler::explicit_null_check(Register addr, CodeEmitInfo* info) {
  ImplicitNullCheckStub* stub = new ImplicitNullCheckStub(code_offset(), info);
  __ null_check(addr, stub->entry());
  append_code_stub(stub);
}


// Attention: caller must encode oop if needed
int LIR_Assembler::store(LIR_Opr from_reg, Register base, int offset, BasicType type, bool wide, bool unaligned) {
  int store_offset;
  if (!Assembler::is_simm16(offset)
      USE_SPE_ONLY( || (type == T_DOUBLE && !Assembler::is_ev_uimm8_5(offset)))) {
    // For offsets larger than a simm16 we setup the offset.
    assert((wide || UseCompressedOops) && !from_reg->is_same_register(FrameMap::R0_opr), "large offset only supported in special case");
    __ load_const_optimized(R0, offset);
    store_offset = store(from_reg, base, R0, type, wide);
  } else {
    store_offset = code_offset();
    switch (type) {
      case T_BOOLEAN: // fall through
      case T_BYTE  : __ stb(from_reg->as_register(), offset, base); break;
      case T_CHAR  :
      case T_SHORT : __ sth(from_reg->as_register(), offset, base); break;
      case T_INT   : __ stw(from_reg->as_register(), offset, base); break;
      case T_LONG  :
#ifdef _LP64
        __ std(from_reg->as_register_lo(), offset, base);
#else
        __ stw(from_reg->as_register_hi(), offset+hi_word_offset_in_bytes, base);
        __ stw(from_reg->as_register_lo(), offset+lo_word_offset_in_bytes, base);
#endif
        break;
      case T_ADDRESS:
      case T_METADATA: __ st(from_reg->as_register(), offset, base); break;
      case T_ARRAY : // fall through
      case T_OBJECT:
        {
          if (UseCompressedOops && !wide) {
            // Encoding done in caller
            __ stw(from_reg->as_register(), offset, base);
          } else {
            __ st(from_reg->as_register(), offset, base);
          }
          __ verify_oop(from_reg->as_register());
          break;
        }
      case T_FLOAT :
#ifndef USE_SPE
        __ stfs(from_reg->as_float_reg(), offset, base);
#else
        __ stw(from_reg->as_register(), offset, base);
#endif
        break;
      case T_DOUBLE:
#ifndef USE_SPE
        __ stfd(from_reg->as_double_reg(), offset, base);
#else
        assert(from_reg->is_double_cpu(), "src must be double cpu");
        assert(from_reg->as_register_hi() == from_reg->as_register_lo(), "from_reg in single gpr");
        if (unaligned) {
          guarantee(R0 != base, "Unimplemented");
          __ stw(from_reg->as_register_lo(), offset+lo_word_offset_in_bytes, base);
          __ evmergehi(R0, R0, from_reg->as_register_lo());
          __ stw(R0, offset+hi_word_offset_in_bytes, base);
        } else {
          __ evstdd_aligned(from_reg->as_register_lo(), offset, base);
        }
#endif
        break;
      default      : ShouldNotReachHere();
    }
  }
  return store_offset;
}


// Attention: caller must encode oop if needed
int LIR_Assembler::store(LIR_Opr from_reg, Register base, Register disp, BasicType type, bool wide) {
  int store_offset = code_offset();
  switch (type) {
    case T_BOOLEAN: // fall through
    case T_BYTE  : __ stbx(from_reg->as_register(), base, disp); break;
    case T_CHAR  :
    case T_SHORT : __ sthx(from_reg->as_register(), base, disp); break;
    case T_INT   : __ stwx(from_reg->as_register(), base, disp); break;
    case T_LONG  :
#ifdef _LP64
      __ stdx(from_reg->as_register_lo(), base, disp);
#else
      // adjust base, not disp. R0 could be used as disp,
      // turning `addi` below to `li`. Use R0 as base is less likely.
      assert(base != R0, "can't do adjustments below");
      __ stwx(from_reg->as_register_hi(), base, disp);
      __ addi(base, base, pd_lo_word_offset_in_bytes);
      __ stwx(from_reg->as_register_lo(), base, disp);
      __ addi(base, base, -pd_lo_word_offset_in_bytes);
#endif
      break;
    case T_ADDRESS:
      __ stx(from_reg->as_register(), base, disp);
      break;
    case T_ARRAY : // fall through
    case T_OBJECT:
      {
        if (UseCompressedOops && !wide) {
          // Encoding done in caller.
          __ stwx(from_reg->as_register(), base, disp);
        } else {
          __ stx(from_reg->as_register(), base, disp);
        }
        __ verify_oop(from_reg->as_register()); // kills R0
        break;
      }
    case T_FLOAT :
#ifndef USE_SPE
      __ stfsx(from_reg->as_float_reg(), base, disp);
#else
      __ stwx(from_reg->as_register(), base, disp);
#endif
      break;
    case T_DOUBLE:
#ifndef USE_SPE
      __ stfdx(from_reg->as_double_reg(), base, disp);
#else
      assert(from_reg->is_double_cpu(), "src must be double cpu");
      assert(from_reg->as_register_hi() == from_reg->as_register_lo(), "from_reg in single gpr");
      __ evstddx_aligned(from_reg->as_register_lo(), base, disp);
#endif
      break;
    default      : ShouldNotReachHere();
  }
  return store_offset;
}


int LIR_Assembler::load(Register base, int offset, LIR_Opr to_reg, BasicType type, bool wide, bool unaligned) {
  int load_offset;
  if (!Assembler::is_simm16(offset)
      USE_SPE_ONLY( || (type == T_DOUBLE && !Assembler::is_ev_uimm8_5(offset)))) {
    // For offsets larger than a simm16 we setup the offset.
    __ load_const_optimized(R0, offset);
    load_offset = load(base, R0, to_reg, type, wide);
  } else {
    load_offset = code_offset();
    switch(type) {
      case T_BOOLEAN: // fall through
      case T_BYTE  :   __ lbz(to_reg->as_register(), offset, base);
                       __ extsb(to_reg->as_register(), to_reg->as_register()); break;
      case T_CHAR  :   __ lhz(to_reg->as_register(), offset, base); break;
      case T_SHORT :   __ lha(to_reg->as_register(), offset, base); break;
      case T_INT   :   __ lwa(to_reg->as_register(), offset, base); break;
      case T_LONG  :
#ifdef _LP64
        __ ld(to_reg->as_register_lo(), offset, base);
#else
        if (to_reg->as_register_hi() != base) {
          __ lwa(to_reg->as_register_hi(), offset+hi_word_offset_in_bytes, base);
          __ lwa(to_reg->as_register_lo(), offset+lo_word_offset_in_bytes, base);
        } else {
          assert(to_reg->as_register_lo() != base, "base == hi, hi != lo");
          __ lwa(to_reg->as_register_lo(), offset+lo_word_offset_in_bytes, base);
          __ lwa(to_reg->as_register_hi(), offset+hi_word_offset_in_bytes, base);
        }
#endif
        break;
      case T_METADATA: __ l(to_reg->as_register(), offset, base); break;
      case T_ADDRESS:
        if (offset == oopDesc::klass_offset_in_bytes() && UseCompressedClassPointers) {
          __ lwz(to_reg->as_register(), offset, base);
          __ decode_klass_not_null(to_reg->as_register());
        } else {
          __ l(to_reg->as_register(), offset, base);
        }
        break;
      case T_ARRAY : // fall through
      case T_OBJECT:
        {
          if (UseCompressedOops && !wide) {
            __ lwz(to_reg->as_register(), offset, base);
            __ decode_heap_oop(to_reg->as_register());
          } else {
            __ l(to_reg->as_register(), offset, base);
          }
          __ verify_oop(to_reg->as_register());
          break;
        }
      case T_FLOAT:
#ifndef USE_SPE
        __ lfs(to_reg->as_float_reg(), offset, base);
#else
        __ lwa(to_reg->as_register(), offset, base);
#endif
        break;
      case T_DOUBLE:
#ifndef USE_SPE
        __ lfd(to_reg->as_double_reg(), offset, base);
#else
        assert(to_reg->is_double_cpu(), " dst must be double cpu");
        if (to_reg->as_register_hi() == to_reg->as_register_lo()) {
          if (unaligned) {
            guarantee(base != R0, "Unimplemented");
            __ lwz(R0, offset+hi_word_offset_in_bytes, base);
            __ lwz(to_reg->as_register_lo(), offset+lo_word_offset_in_bytes, base);
            __ evmergelo(to_reg->as_register_lo(), R0, to_reg->as_register_lo());
          } else {
            __ evldd_aligned(to_reg->as_register_lo(), offset, base);
          }
        } else {
          if (to_reg->as_register_hi() != base) {
            __ lwz(to_reg->as_register_hi(), offset+hi_word_offset_in_bytes, base);
            __ lwz(to_reg->as_register_lo(), offset+lo_word_offset_in_bytes, base);
          } else {
            assert(to_reg->as_register_lo() != base, "base == hi, hi != lo");
            __ lwz(to_reg->as_register_lo(), offset+lo_word_offset_in_bytes, base);
            __ lwz(to_reg->as_register_hi(), offset+hi_word_offset_in_bytes, base);
          }
        }

#endif
        break;
      default      : ShouldNotReachHere();
    }
  }
  return load_offset;
}


int LIR_Assembler::load(Register base, Register disp, LIR_Opr to_reg, BasicType type, bool wide) {
  int load_offset = code_offset();
  switch(type) {
    case T_BOOLEAN: // fall through
    case T_BYTE  :  __ lbzx(to_reg->as_register(), base, disp);
                    __ extsb(to_reg->as_register(), to_reg->as_register()); break;
    case T_CHAR  :  __ lhzx(to_reg->as_register(), base, disp); break;
    case T_SHORT :  __ lhax(to_reg->as_register(), base, disp); break;
    case T_INT   :  __ lwax(to_reg->as_register(), base, disp); break;
    case T_ADDRESS: __ lx(to_reg->as_register(), base, disp); break;
    case T_ARRAY : // fall through
    case T_OBJECT:
      {
        if (UseCompressedOops && !wide) {
          __ lwzx(to_reg->as_register(), base, disp);
          __ decode_heap_oop(to_reg->as_register());
        } else {
          __ lx(to_reg->as_register(), base, disp);
        }
        __ verify_oop(to_reg->as_register());
        break;
      }
    case T_FLOAT:
#ifndef USE_SPE
      __ lfsx(to_reg->as_float_reg() , base, disp);
#else
      __ lwax(to_reg->as_register(), base, disp);
#endif
      break;
    case T_DOUBLE:
#ifndef USE_SPE
      __ lfdx(to_reg->as_double_reg(), base, disp);
#else
      assert(to_reg->is_double_cpu(), "dst must be double cpu");
      __ evlddx_aligned(to_reg->as_register_lo(), base, disp);
      if (to_reg->as_register_hi() != to_reg->as_register_lo()) {
        __ evmergehi(to_reg->as_register_hi(), to_reg->as_register_hi(), to_reg->as_register_lo());
      }
#endif
      break;
    case T_LONG  :
#ifdef _LP64
      __ ldx(to_reg->as_register_lo(), base, disp);
#else
      {
        const Register to_lo = to_reg->as_register_lo();
        const Register to_hi = to_reg->as_register_hi();
        __ add(to_hi, base, disp);
        __ lwz(to_lo, pd_lo_word_offset_in_bytes, to_hi);
        __ lwz(to_hi, 0, to_hi);
      }
#endif
      break;
    default      : ShouldNotReachHere();
  }
  return load_offset;
}


void LIR_Assembler::const2stack(LIR_Opr src, LIR_Opr dest) {
  LIR_Const* c = src->as_constant_ptr();
  Register src_reg = R0;
  switch (c->type()) {
    case T_INT:
    case T_FLOAT: {
      int value = c->as_jint_bits();
      __ load_const_optimized(src_reg, value);
      Address addr = frame_map()->address_for_slot(dest->single_stack_ix());
      __ stw(src_reg, addr.disp(), addr.base());
      break;
    }
    case T_ADDRESS: {
      int value = c->as_jint_bits();
      __ load_const_optimized(src_reg, value);
      Address addr = frame_map()->address_for_slot(dest->single_stack_ix());
      __ st(src_reg, addr.disp(), addr.base());
      break;
    }
    case T_OBJECT: {
      jobject2reg(c->as_jobject(), src_reg);
      Address addr = frame_map()->address_for_slot(dest->single_stack_ix());
      __ st(src_reg, addr.disp(), addr.base());
      break;
    }
    case T_LONG:
    case T_DOUBLE: {
#ifdef PPC64
      int value = c->as_jlong_bits();
      __ load_const_optimized(src_reg, value);
      Address addr = frame_map()->address_for_double_slot(dest->double_stack_ix());
      __ st(src_reg, addr.disp(), addr.base());
#else
      __ load_const_optimized(src_reg, c->as_jint_hi_bits());
      Address addr = frame_map()->address_for_double_slot(dest->double_stack_ix());
      __ st(src_reg, addr.disp()+hi_word_offset_in_bytes, addr.base());
      __ load_const_optimized(src_reg, c->as_jint_lo_bits());
      __ st(src_reg, addr.disp()+lo_word_offset_in_bytes, addr.base());
#endif
      break;
    }
    default:
      Unimplemented();
  }
}


void LIR_Assembler::const2mem(LIR_Opr src, LIR_Opr dest, BasicType type, CodeEmitInfo* info, bool wide) {
  LIR_Const* c = src->as_constant_ptr();
  LIR_Address* addr = dest->as_address_ptr();
  Register base = addr->base()->as_pointer_register();
  LIR_Opr tmp = LIR_OprFact::illegalOpr;
  int offset = -1;
  // Null check for large offsets in LIRGenerator::do_StoreField.
  bool needs_explicit_null_check = !ImplicitNullChecks;

  if (info != NULL && needs_explicit_null_check) {
    explicit_null_check(base, info);
  }

  switch (c->type()) {
    case T_FLOAT: type = T_INT;
    case T_INT:
    case T_ADDRESS: {
      tmp = FrameMap::R0_opr;
      __ load_const_optimized(tmp->as_register(), c->as_jint_bits());
      break;
    }
    case T_DOUBLE: type = T_LONG;
    case T_LONG: {
#ifdef _LP64
      tmp = FrameMap::R0_long_opr;
      __ load_const_optimized(tmp->as_register_lo(), c->as_jlong_bits());
#else
      assert(!addr->index()->is_valid(), "can't handle reg reg address here");
      tmp = FrameMap::R0_opr;
      __ load_const_optimized(tmp->as_register(), c->as_jint_hi_bits());
      offset = store(tmp, base, addr->disp() + hi_word_offset_in_bytes, T_INT, wide, false);
      __ load_const_optimized(tmp->as_register(), c->as_jint_lo_bits());
      store(tmp, base, addr->disp() + lo_word_offset_in_bytes, T_INT, wide, false);
#endif
      break;
    }
    case T_OBJECT: {
      tmp = FrameMap::R0_opr;
      if (UseCompressedOops && !wide && c->as_jobject() != NULL) {
        AddressLiteral oop_addr = __ constant_oop_address(c->as_jobject());
        __ lis(R0, oop_addr.value() >> 16); // Don't care about sign extend (will use stw).
        __ relocate(oop_addr.rspec(), /*compressed format*/ 1);
        __ ori(R0, R0, oop_addr.value() & 0xffff);
      } else {
        jobject2reg(c->as_jobject(), R0);
      }
      break;
    }
    default:
      Unimplemented();
  }

  // Handle either reg+reg or reg+disp address.
  if (offset == -1) {
    if (addr->index()->is_valid()) {
      assert(addr->disp() == 0, "must be zero");
      offset = store(tmp, base, addr->index()->as_pointer_register(), type, wide);
    } else {
      assert(Assembler::is_simm16(addr->disp()), "can't handle larger addresses");
      offset = store(tmp, base, addr->disp(), type, wide, false);
    }
  }

  if (info != NULL) {
    assert(offset != -1, "offset should've been set");
    if (!needs_explicit_null_check) {
      add_debug_info_for_null_check(offset, info);
    }
  }
}


void LIR_Assembler::const2reg(LIR_Opr src, LIR_Opr dest, LIR_PatchCode patch_code, CodeEmitInfo* info) {
  LIR_Const* c = src->as_constant_ptr();
  LIR_Opr to_reg = dest;

  switch (c->type()) {
    case T_INT: {
      assert(patch_code == lir_patch_none, "no patching handled here");
      __ load_const_optimized(dest->as_register(), c->as_jint(), R0);
      break;
    }
    case T_ADDRESS: {
      assert(patch_code == lir_patch_none, "no patching handled here");
      __ load_const_optimized(dest->as_register(), c->as_jint(), R0);  // Yes, as_jint ...
      break;
    }
    case T_LONG: {
      assert(patch_code == lir_patch_none, "no patching handled here");
#ifdef PPC64
      __ load_const_optimized(dest->as_register_lo(), c->as_jlong(), R0);
#else
      __ load_const_optimized(dest->as_register_lo(), c->as_jlong() & 0xFFFFFFFF, R0);
      __ load_const_optimized(dest->as_register_hi(), c->as_jlong() >> 32, R0);
#endif
      break;
    }

    case T_OBJECT: {
      if (patch_code == lir_patch_none) {
        jobject2reg(c->as_jobject(), to_reg->as_register());
      } else {
        jobject2reg_with_patching(to_reg->as_register(), info);
      }
      break;
    }

    case T_METADATA:
      {
        if (patch_code == lir_patch_none) {
          metadata2reg(c->as_metadata(), to_reg->as_register());
        } else {
          klass2reg_with_patching(to_reg->as_register(), info);
        }
      }
      break;

    case T_FLOAT:
      {
        if (to_reg->is_single_fpu()) {
#ifndef USE_SPE
          address const_addr = __ float_constant(c->as_jfloat());
          if (const_addr == NULL) {
            bailout("const section overflow");
            break;
          }
          RelocationHolder rspec = internal_word_Relocation::spec(const_addr);
          __ relocate(rspec);
          __ load_const(R0, const_addr);
          __ lfsx(to_reg->as_float_reg(), R0);
#else
          ShouldNotReachHere();
#endif
        } else {
          assert(to_reg->is_single_cpu(), "Must be a cpu register.");
          __ load_const_optimized(to_reg->as_register(), jint_cast(c->as_jfloat()), R0);
        }
      }
      break;

    case T_DOUBLE:
      {
        if (to_reg->is_double_fpu()) {
#ifndef USE_SPE
          address const_addr = __ double_constant(c->as_jdouble());
          if (const_addr == NULL) {
            bailout("const section overflow");
            break;
          }
          RelocationHolder rspec = internal_word_Relocation::spec(const_addr);
          __ relocate(rspec);
          __ load_const(R0, const_addr);
          __ lfdx(to_reg->as_double_reg(), R0);
#else
          ShouldNotReachHere();
#endif
        } else {
          assert(to_reg->is_double_cpu(), "Must be a long register.");
#if defined(PPC64)
          __ load_const_optimized(to_reg->as_register_lo(), jlong_cast(c->as_jdouble()), R0);
#elif !defined(USE_SPE)
          ShouldNotReachHere();
#else
          if (dest->as_register_hi() == dest->as_register_lo()) {
            __ load_const_optimized(dest->as_register_lo(), jlong_cast(c->as_jdouble()) >> 32);
            __ evmergelo(dest->as_register_lo(), dest->as_register_lo(), dest->as_register_lo());
            __ load_const_optimized(dest->as_register_lo(), jlong_cast(c->as_jdouble()) & 0xFFFFFFFF, R0);
          } else {
            __ load_const_optimized(dest->as_register_lo(), jlong_cast(c->as_jdouble()) & 0xFFFFFFFF, R0);
            __ load_const_optimized(dest->as_register_hi(), jlong_cast(c->as_jdouble()) >> 32);
          }
#endif
        }
      }
      break;

    default:
      ShouldNotReachHere();
  }
}


Address LIR_Assembler::as_Address(LIR_Address* addr) {
  Unimplemented(); return Address();
}


inline RegisterOrConstant index_or_disp(LIR_Address* addr) {
  if (addr->index()->is_illegal()) {
    return (RegisterOrConstant)(addr->disp());
  } else {
    return (RegisterOrConstant)(addr->index()->as_pointer_register());
  }
}


void LIR_Assembler::stack2stack(LIR_Opr src, LIR_Opr dest, BasicType type) {
  const Register tmp = R0;
  switch (type) {
    case T_INT:
    case T_FLOAT: {
      Address from = frame_map()->address_for_slot(src->single_stack_ix());
      Address to   = frame_map()->address_for_slot(dest->single_stack_ix());
      __ lwz(tmp, from.disp(), from.base());
      __ stw(tmp, to.disp(), to.base());
      break;
    }
    case T_ADDRESS:
    case T_OBJECT: {
      Address from = frame_map()->address_for_slot(src->single_stack_ix());
      Address to   = frame_map()->address_for_slot(dest->single_stack_ix());
      __ l(tmp, from.disp(), from.base());
      __ st(tmp, to.disp(), to.base());
      break;
    }
    case T_LONG:
    case T_DOUBLE: {
      Address from = frame_map()->address_for_double_slot(src->double_stack_ix());
      Address to   = frame_map()->address_for_double_slot(dest->double_stack_ix());
      __ l(tmp, from.disp(), from.base());
      __ st(tmp, to.disp(), to.base());
#ifndef PPC64
      __ l(tmp, from.disp()+BytesPerWord, from.base());
      __ st(tmp, to.disp()+BytesPerWord, to.base());
#endif
      break;
    }

    default:
      ShouldNotReachHere();
  }
}


Address LIR_Assembler::as_Address_hi(LIR_Address* addr) {
  Unimplemented(); return Address();
}


Address LIR_Assembler::as_Address_lo(LIR_Address* addr) {
  Unimplemented(); return Address();
}


void LIR_Assembler::mem2reg(LIR_Opr src_opr, LIR_Opr dest, BasicType type,
                            LIR_PatchCode patch_code, CodeEmitInfo* info, bool wide, bool unaligned) {

  assert(type != T_METADATA, "load of metadata ptr not supported");
  LIR_Address* addr = src_opr->as_address_ptr();
  LIR_Opr to_reg = dest;

  Register src = addr->base()->as_pointer_register();
  Register disp_reg = noreg;
  int disp_value = addr->disp();
  bool needs_patching = (patch_code != lir_patch_none);
  // null check for large offsets in LIRGenerator::do_LoadField
  bool needs_explicit_null_check = !os::zero_page_read_protected() || !ImplicitNullChecks;

  if (info != NULL && needs_explicit_null_check) {
    explicit_null_check(src, info);
  }

  if (addr->base()->type() == T_OBJECT) {
    __ verify_oop(src);
  }

  PatchingStub* patch = NULL;
  if (needs_patching) {
    patch = new PatchingStub(_masm, PatchingStub::access_field_id);
    assert(!to_reg->is_double_cpu() ||
           patch_code == lir_patch_none ||
           patch_code == lir_patch_normal, "patching doesn't match register");
  }

  if (addr->index()->is_illegal()) {
    if (!Assembler::is_simm16(disp_value)) {
      if (needs_patching) {
        __ load_const32(R0, 0); // patchable int
      } else {
        __ load_const_optimized(R0, disp_value);
      }
      disp_reg = R0;
    }
  } else {
    disp_reg = addr->index()->as_pointer_register();
    assert(disp_value == 0, "can't handle 3 operand addresses");
  }

  // Remember the offset of the load. The patching_epilog must be done
  // before the call to add_debug_info, otherwise the PcDescs don't get
  // entered in increasing order.
  int offset;

  if (disp_reg == noreg) {
    assert(Assembler::is_simm16(disp_value), "should have set this up");
    offset = load(src, disp_value, to_reg, type, wide, unaligned);
  } else {
    assert(!unaligned, "unexpected");
    offset = load(src, disp_reg, to_reg, type, wide);
  }

  if (patch != NULL) {
    patching_epilog(patch, patch_code, src, info);
  }
  if (info != NULL && !needs_explicit_null_check) {
    add_debug_info_for_null_check(offset, info);
  }
}


void LIR_Assembler::stack2reg(LIR_Opr src, LIR_Opr dest, BasicType type) {
  Address addr;
  if (src->is_single_word()) {
    addr = frame_map()->address_for_slot(src->single_stack_ix());
  } else if (src->is_double_word())  {
    addr = frame_map()->address_for_double_slot(src->double_stack_ix());
  }

  bool unaligned = (addr.disp() - STACK_BIAS) % 8 != 0;
  load(addr.base(), addr.disp(), dest, dest->type(), true /*wide*/, unaligned);
}


void LIR_Assembler::reg2stack(LIR_Opr from_reg, LIR_Opr dest, BasicType type, bool pop_fpu_stack) {
  Address addr;
  if (dest->is_single_word()) {
    addr = frame_map()->address_for_slot(dest->single_stack_ix());
  } else if (dest->is_double_word())  {
    addr = frame_map()->address_for_slot(dest->double_stack_ix());
  }
  bool unaligned = (addr.disp() - STACK_BIAS) % 8 != 0;
  store(from_reg, addr.base(), addr.disp(), from_reg->type(), true /*wide*/, unaligned);
}


void LIR_Assembler::reg2reg(LIR_Opr from_reg, LIR_Opr to_reg) {
  if (from_reg->is_float_kind() && to_reg->is_float_kind()) {
#ifndef USE_SPE
    if (from_reg->is_double_fpu()) {
      // double to double moves
      assert(to_reg->is_double_fpu(), "should match");
      __ fmr_if_needed(to_reg->as_double_reg(), from_reg->as_double_reg());
    } else {
      // float to float moves
      assert(to_reg->is_single_fpu(), "should match");
      __ fmr_if_needed(to_reg->as_float_reg(), from_reg->as_float_reg());
    }
#else // USE_SPE
    if (from_reg->type() == T_FLOAT) {
      __ mr_if_needed(to_reg->as_register(), from_reg->as_register());
    } else {
      assert(from_reg->type() == T_DOUBLE, "no others");

      if (from_reg->as_register_hi() == from_reg->as_register_lo()) {
        if (to_reg->as_register_hi() == to_reg->as_register_lo()) {
          if (from_reg->as_register_lo() != to_reg->as_register_lo()) {
            __ evmergehilo(to_reg->as_register_lo(), from_reg->as_register_lo(), from_reg->as_register_lo());
          }
        } else {
          if (to_reg->as_register_hi() != from_reg->as_register_lo()) {
            __ evmergehi(to_reg->as_register_hi(), from_reg->as_register_lo(), from_reg->as_register_lo());
            __ mr_if_needed(to_reg->as_register_lo(), from_reg->as_register_lo());
          } else {
            __ mr_if_needed(to_reg->as_register_lo(), from_reg->as_register_lo());
            __ evmergehi(to_reg->as_register_hi(), from_reg->as_register_lo(), from_reg->as_register_lo());
          }
        }
      } else {
        if (to_reg->as_register_hi() == to_reg->as_register_lo()) {
          __ evmergelo(to_reg->as_register_lo(), from_reg->as_register_hi(), from_reg->as_register_lo());
        } else {
          if (to_reg->as_register_lo() != from_reg->as_register_hi()) {
            __ mr_if_needed(to_reg->as_register_lo(), from_reg->as_register_lo());
            __ mr_if_needed(to_reg->as_register_hi(), from_reg->as_register_hi());
          } else if (to_reg->as_register_hi() != from_reg->as_register_lo()) {
            __ mr_if_needed(to_reg->as_register_hi(), from_reg->as_register_hi());
            __ mr_if_needed(to_reg->as_register_lo(), from_reg->as_register_lo());
          } else {
            __ mr(R0, from_reg->as_register_lo());
            __ mr_if_needed(to_reg->as_register_hi(), from_reg->as_register_hi());
            __ mr(to_reg->as_register_lo(), R0);
          }
        }
      }
    }
#endif // USE_SPE
  } else if (!from_reg->is_float_kind() && !to_reg->is_float_kind()) {
    if (from_reg->is_double_cpu()) {
#ifdef PPC64
        __ mr_if_needed(to_reg->as_pointer_register(), from_reg->as_pointer_register());
#else // PPC64
        assert(to_reg->is_double_cpu(), "must match");
        Register f_lo = from_reg->as_register_lo();
        Register f_hi = from_reg->as_register_hi();
        Register t_lo = to_reg->as_register_lo();
        Register t_hi = to_reg->as_register_hi();
        assert(f_hi->encoding() != f_lo->encoding(), "must be different");
        assert(t_hi->encoding() != t_lo->encoding(), "must be different");
        if (f_hi->encoding() != t_lo->encoding()) {
          __ mr_if_needed(t_lo, f_lo);
          __ mr_if_needed(t_hi, f_hi);
        } else {
          assert(t_hi->encoding() != f_lo->encoding(), "source overwritten");
          __ mr_if_needed(t_hi, f_hi);
          __ mr_if_needed(t_lo, f_lo);
        }
#endif // PPC64
    } else if (to_reg->is_double_cpu()) {
      // int to long moves
      __ mr_if_needed(to_reg->as_register_lo(), from_reg->as_register());
#ifndef PPC64
      __ li(to_reg->as_register_hi(), 0);
#endif
    } else {
      // int to int moves
      __ mr_if_needed(to_reg->as_register(), from_reg->as_register());
    }
  } else {
    ShouldNotReachHere();
  }
  if (to_reg->type() == T_OBJECT || to_reg->type() == T_ARRAY) {
    __ verify_oop(to_reg->as_register());
  }
}


void LIR_Assembler::reg2mem(LIR_Opr from_reg, LIR_Opr dest, BasicType type,
                            LIR_PatchCode patch_code, CodeEmitInfo* info, bool pop_fpu_stack,
                            bool wide, bool unaligned) {
  assert(type != T_METADATA, "store of metadata ptr not supported");
  LIR_Address* addr = dest->as_address_ptr();

  Register src = addr->base()->as_pointer_register();
  Register disp_reg = noreg;
  int disp_value = addr->disp();
  bool needs_patching = (patch_code != lir_patch_none);
  bool compress_oop = (type == T_ARRAY || type == T_OBJECT) && UseCompressedOops && !wide &&
                      Universe::narrow_oop_mode() != Universe::UnscaledNarrowOop;
  bool load_disp = addr->index()->is_illegal() && !Assembler::is_simm16(disp_value);
  // use_R29 is always false on PPC32
  bool use_R29 = compress_oop && load_disp; // Avoid register conflict, also do null check before killing R29.
  // Null check for large offsets in LIRGenerator::do_StoreField.
  bool needs_explicit_null_check = !ImplicitNullChecks || use_R29;

  if (info != NULL && needs_explicit_null_check) {
    explicit_null_check(src, info);
  }

  if (addr->base()->is_oop_register()) {
    __ verify_oop(src);
  }

  PatchingStub* patch = NULL;
  if (needs_patching) {
    patch = new PatchingStub(_masm, PatchingStub::access_field_id);
    assert(!from_reg->is_double_cpu() ||
           patch_code == lir_patch_none ||
           patch_code == lir_patch_normal, "patching doesn't match register");
  }

  if (addr->index()->is_illegal()) {
    if (load_disp) {
      disp_reg = use_R29 ? R29_TOC : R0;
      if (needs_patching) {
        __ load_const32(disp_reg, 0); // patchable int
      } else {
        __ load_const_optimized(disp_reg, disp_value);
      }
    }
  } else {
    disp_reg = addr->index()->as_pointer_register();
    assert(disp_value == 0, "can't handle 3 operand addresses");
  }

  // remember the offset of the store. The patching_epilog must be done
  // before the call to add_debug_info_for_null_check, otherwise the PcDescs don't get
  // entered in increasing order.
  int offset;

  if (compress_oop) {
    Register co = __ encode_heap_oop(R0, from_reg->as_register());
    from_reg = FrameMap::as_opr(co);
  }

  if (disp_reg == noreg) {
    assert(Assembler::is_simm16(disp_value), "should have set this up");
    offset = store(from_reg, src, disp_value, type, wide, unaligned);
  } else {
    assert(!unaligned, "unexpected");
    offset = store(from_reg, src, disp_reg, type, wide);
  }

  if (use_R29) {
    __ load_const_optimized(R29_TOC, MacroAssembler::global_toc(), R0); // reinit
  }

  if (patch != NULL) {
    patching_epilog(patch, patch_code, src, info);
  }

  if (info != NULL && !needs_explicit_null_check) {
    add_debug_info_for_null_check(offset, info);
  }
}


void LIR_Assembler::return_op(LIR_Opr result) {
  const Register return_pc        = R11;
  const Register polling_page     = R12;

  // Pop the stack before the safepoint code.
  int frame_size = initial_frame_size_in_bytes();
  if (Assembler::is_simm(frame_size, 16)) {
    __ addi(R1_SP, R1_SP, frame_size);
  } else {
    __ pop_frame();
  }

  if (LoadPollAddressFromThread) {
    // TODO: PPC port __ l(polling_page, in_bytes(JavaThread::poll_address_offset()), R16_thread);
    Unimplemented();
  } else {
    __ load_const_optimized(polling_page, (long)(address) os::get_polling_page(), R0); // TODO: PPC port: get_standard_polling_page()
  }

  // Restore return pc relative to callers' sp.
  __ l(return_pc, _abi(lr), R1_SP);
  // Move return pc to LR.
  __ mtlr(return_pc);

  // We need to mark the code position where the load from the safepoint
  // polling page was emitted as relocInfo::poll_return_type here.
  __ relocate(relocInfo::poll_return_type);
  __ load_from_polling_page(polling_page);

  // Return.
  __ blr();
}


int LIR_Assembler::safepoint_poll(LIR_Opr tmp, CodeEmitInfo* info) {

  if (LoadPollAddressFromThread) {
    const Register poll_addr = tmp->as_register();
    // TODO: PPC port __ l(poll_addr, in_bytes(JavaThread::poll_address_offset()), R16_thread);
    Unimplemented();
    __ relocate(relocInfo::poll_type); // XXX
    guarantee(info != NULL, "Shouldn't be NULL");
    int offset = __ offset();
    add_debug_info_for_branch(info);
    __ load_from_polling_page(poll_addr);
    return offset;
  }

  __ load_const_optimized(tmp->as_register(), (intptr_t)os::get_polling_page(), R0); // TODO: PPC port: get_standard_polling_page()
  if (info != NULL) {
    add_debug_info_for_branch(info);
  }
  int offset = __ offset();
  __ relocate(relocInfo::poll_type);
  __ load_from_polling_page(tmp->as_register());

  return offset;
}


void LIR_Assembler::emit_static_call_stub() {
  address call_pc = __ pc();
  address stub = __ start_a_stub(max_static_call_stub_size);
  if (stub == NULL) {
    bailout("static call stub overflow");
    return;
  }

  // For java_to_interp stubs we use R11_scratch1 as scratch register
  // and in call trampoline stubs we use R12_scratch2. This way we
  // can distinguish them (see is_NativeCallTrampolineStub_at()).
  const Register reg_scratch = R11_scratch1;

  // Create a static stub relocation which relates this stub
  // with the call instruction at insts_call_instruction_offset in the
  // instructions code-section.
  int start = __ offset();
  __ relocate(static_stub_Relocation::spec(call_pc));

  // Now, create the stub's code:
  // - load the TOC
  // - load the inline cache oop from the constant pool
  // - load the call target from the constant pool
  // - call
  __ calculate_address_from_global_toc(reg_scratch, __ method_toc());
  AddressLiteral ic = __ allocate_metadata_address((Metadata *)NULL);
  bool success = __ load_const_from_method_toc(R19_inline_cache_reg, ic, reg_scratch, /*fixed_size*/ true);

  if (ReoptimizeCallSequences) {
    __ b64_patchable((address)-1, relocInfo::none);
  } else {
    AddressLiteral a((address)-1);
    success = success && __ load_const_from_method_toc(reg_scratch, a, reg_scratch, /*fixed_size*/ true);
    __ mtctr(reg_scratch);
    __ bctr();
  }
  if (!success) {
    bailout("const section overflow");
    return;
  }

  assert(__ offset() - start <= max_static_call_stub_size, "stub too big");
  __ end_a_stub();
}


void LIR_Assembler::comp_op(LIR_Condition condition, LIR_Opr opr1, LIR_Opr opr2, LIR_Op2* op) {
  bool unsigned_comp = (condition == lir_cond_belowEqual || condition == lir_cond_aboveEqual);
  if (opr1->is_single_fpu() || opr1->type() == T_FLOAT) {
#ifndef USE_SPE
    __ fcmpu(BOOL_RESULT, opr1->as_float_reg(), opr2->as_float_reg());
#else
    Label done;
    int res_so = __ condition_register_bit(BOOL_RESULT, __ summary_overflow),
        cr0_gt = __ condition_register_bit(CCR0, __ positive);

    __ crxor(BOOL_RESULT, __ less, BOOL_RESULT, __ less);
    __ crxor(BOOL_RESULT, __ greater, BOOL_RESULT, __ greater);
    __ crxor(BOOL_RESULT, __ equal, BOOL_RESULT, __ equal);
    __ creqv(BOOL_RESULT, __ summary_overflow, BOOL_RESULT, __ summary_overflow);
    __ efscmpeq(CCR0, opr1->as_register(), opr2->as_register());
    __ cror(BOOL_RESULT, __ equal, CCR0, __ positive);
    __ crandc(res_so, res_so, cr0_gt);
    __ bgt(CCR0, done);
    __ efscmplt(CCR0, opr1->as_register(), opr2->as_register());
    __ cror(BOOL_RESULT, __ less, CCR0, __ positive);
    __ crandc(res_so, res_so, cr0_gt);
    __ bgt(CCR0, done);
    __ efscmpgt(CCR0, opr1->as_register(), opr2->as_register());
    __ cror(BOOL_RESULT, __ greater, CCR0, __ positive);
    __ crandc(res_so, res_so, cr0_gt);
    __ bind(done);
#endif
  } else if (opr1->is_double_fpu() || opr1->type()==T_DOUBLE) {
#ifndef USE_SPE
    __ fcmpu(BOOL_RESULT, opr1->as_double_reg(), opr2->as_double_reg());
#else
    Label done;
    int res_so = __ condition_register_bit(BOOL_RESULT, __ summary_overflow),
        cr0_gt = __ condition_register_bit(CCR0, __ positive);

    __ crxor(BOOL_RESULT, __ less, BOOL_RESULT, __ less);
    __ crxor(BOOL_RESULT, __ greater, BOOL_RESULT, __ greater);
    __ crxor(BOOL_RESULT, __ equal, BOOL_RESULT, __ equal);
    __ creqv(BOOL_RESULT, __ summary_overflow, BOOL_RESULT, __ summary_overflow);
    __ efdcmpeq(CCR0, opr1->as_register_lo(), opr2->as_register_lo());
    __ cror(BOOL_RESULT, __ equal, CCR0, __ positive);
    __ crandc(res_so, res_so, cr0_gt);
    __ bgt(CCR0, done);
    __ efdcmplt(CCR0, opr1->as_register_lo(), opr2->as_register_lo());
    __ cror(BOOL_RESULT, __ less, CCR0, __ positive);
    __ crandc(res_so, res_so, cr0_gt);
    __ bgt(CCR0, done);
    __ efdcmpgt(CCR0, opr1->as_register_lo(), opr2->as_register_lo());
    __ cror(BOOL_RESULT, __ greater, CCR0, __ positive);
    __ crandc(res_so, res_so, cr0_gt);
    __ bind(done);
#endif
  } else if (opr1->is_single_cpu()) {
    if (opr2->is_constant()) {
      switch (opr2->as_constant_ptr()->type()) {
        case T_INT:
          {
            jint con = opr2->as_constant_ptr()->as_jint();
            if (unsigned_comp) {
              if (Assembler::is_uimm(con, 16)) {
                __ cmplwi(BOOL_RESULT, opr1->as_register(), con);
              } else {
                __ load_const_optimized(R0, con);
                __ cmplw(BOOL_RESULT, opr1->as_register(), R0);
              }
            } else {
              if (Assembler::is_simm(con, 16)) {
                __ cmpwi(BOOL_RESULT, opr1->as_register(), con);
              } else {
                __ load_const_optimized(R0, con);
                __ cmpw(BOOL_RESULT, opr1->as_register(), R0);
              }
            }
          }
          break;

        case T_OBJECT:
          // There are only equal/notequal comparisons on objects.
          {
            assert(condition == lir_cond_equal || condition == lir_cond_notEqual, "oops");
            jobject con = opr2->as_constant_ptr()->as_jobject();
            if (con == NULL) {
              __ cmpi(BOOL_RESULT, opr1->as_register(), 0);
            } else {
              jobject2reg(con, R0);
              __ cmp(BOOL_RESULT, opr1->as_register(), R0);
            }
          }
          break;

        default:
          ShouldNotReachHere();
          break;
      }
    } else {
      if (opr2->is_address()) {
        DEBUG_ONLY( Unimplemented(); ) // Seems to be unused at the moment.
        LIR_Address *addr = opr2->as_address_ptr();
        BasicType type = addr->type();
        if (type == T_OBJECT) { __ l(R0, index_or_disp(addr), addr->base()->as_register()); }
        else                  { __ lwa(R0, index_or_disp(addr), addr->base()->as_register()); }
        __ cmp(BOOL_RESULT, opr1->as_register(), R0);
      } else {
        if (unsigned_comp) {
          __ cmplw(BOOL_RESULT, opr1->as_register(), opr2->as_register());
        } else {
          __ cmpw(BOOL_RESULT, opr1->as_register(), opr2->as_register());
        }
      }
    }
  } else if (opr1->is_double_cpu()) {
    if (opr2->is_constant()) {
      Label exit;
#ifdef PPC64
      const jlong con = opr2->as_constant_ptr()->as_jlong();
#else
      jlong con = opr2->as_constant_ptr()->as_jlong() >> 32;
      if (unsigned_comp) {
          if (Assembler::is_uimm(con, 16)) {
              __ cmpli(BOOL_RESULT, opr1->as_register_hi(), con);
          } else {
              __ load_const_optimized(R0, con);
              __ cmpl(BOOL_RESULT, opr1->as_register_hi(), R0);
          }
      } else {
          if (Assembler::is_simm(con, 16)) {
              __ cmpi(BOOL_RESULT, opr1->as_register_hi(), con);
          } else {
              __ load_const_optimized(R0, con);
              __ cmp(BOOL_RESULT, opr1->as_register_hi(), R0);
          }
      }
      __ bne(BOOL_RESULT, exit);
      con =  opr2->as_constant_ptr()->as_jlong() & 0xFFFFFFFF;
 #endif
      if (unsigned_comp) {
        if (Assembler::is_uimm(con, 16)) {
          __ cmpli(BOOL_RESULT, opr1->as_register_lo(), con);
        } else {
          __ load_const_optimized(R0, con);
          __ cmpl(BOOL_RESULT, opr1->as_register_lo(), R0);
        }
      } else {
        if (Assembler::is_simm(con, 16)) {
          __ cmpi(BOOL_RESULT, opr1->as_register_lo(), con);
        } else {
          __ load_const_optimized(R0, con);
          __ cmp(BOOL_RESULT, opr1->as_register_lo(), R0);
        }
      }
      __ bind(exit);
   } else if (opr2->is_register()) {
      Label exit;
      if (unsigned_comp) {
#ifndef PPC64
        __ cmpl(BOOL_RESULT, opr1->as_register_hi(), opr2->as_register_hi());
        __ bne(BOOL_RESULT, exit);
#endif
        __ cmpl(BOOL_RESULT, opr1->as_register_lo(), opr2->as_register_lo());
      } else {
#ifndef PPC64
        __ cmp(BOOL_RESULT, opr1->as_register_hi(), opr2->as_register_hi());
        __ bne(BOOL_RESULT, exit);
        __ cmpl(BOOL_RESULT, opr1->as_register_lo(), opr2->as_register_lo());
#else
        __ cmp(BOOL_RESULT, opr1->as_register_lo(), opr2->as_register_lo());
#endif
      }
      __ bind(exit);
    } else {
      ShouldNotReachHere();
    }
  } else if (opr1->is_address()) {
    DEBUG_ONLY( Unimplemented(); ) // Seems to be unused at the moment.
    LIR_Address * addr = opr1->as_address_ptr();
    BasicType type = addr->type();
    assert (opr2->is_constant(), "Checking");
    if (type == T_OBJECT) { __ l(R0, index_or_disp(addr), addr->base()->as_register()); }
    else                  { __ lwa(R0, index_or_disp(addr), addr->base()->as_register()); }
    __ cmpi(BOOL_RESULT, R0, opr2->as_constant_ptr()->as_jint());
  } else {
    ShouldNotReachHere();
  }
}


void LIR_Assembler::comp_fl2i(LIR_Code code, LIR_Opr left, LIR_Opr right, LIR_Opr dst, LIR_Op2* op){
  const Register Rdst = dst->as_register();
  Label done;
  if (code == lir_cmp_fd2i || code == lir_ucmp_fd2i) {
    bool is_unordered_less = (code == lir_ucmp_fd2i);
#ifndef USE_SPE
    if (left->is_single_fpu()) {
      __ fcmpu(CCR0, left->as_float_reg(), right->as_float_reg());
    } else if (left->is_double_fpu()) {
      __ fcmpu(CCR0, left->as_double_reg(), right->as_double_reg());
    } else {
      ShouldNotReachHere();
    }
    __ li(Rdst, is_unordered_less ? -1 : 1);
    __ bso(CCR0, done);
#else
    Label exit;
    Register Rtmp = R0;
    // note: result of ef*cmpeq, ef*cmplt, ef*cmpgt commands is stored in the
    // bit 1 of the condition registr. To verify result of these operations
    // bgt mnemonic should be used
    if (left->type() == T_FLOAT) {
      __ li(Rtmp, 0);
      __ efscmpeq(CCR0, left->as_register(), right->as_register());
      __ bgt(CCR0, exit);
      if (!is_unordered_less) {
          __ li(Rtmp, -1);
          __ efscmplt(CCR0, left->as_register(), right->as_register());
          __ bgt(CCR0, exit);
          __ li(Rtmp, 1);
      } else {
          __ li(Rtmp, 1);
          __ efscmpgt(CCR0, left->as_register(), right->as_register());
          __ bgt(CCR0, exit);
          __ li(Rtmp, -1);
      }
    } else if(left->type() == T_DOUBLE) {
      assert(left->is_double_cpu(), "left must be double cpu");
      assert(left->as_register_hi() == left->as_register_lo(), "left in 1 gpr registers");
      assert(right->is_double_cpu(), "right must be double cpu");
      assert(right->as_register_hi() == right->as_register_lo(), "right in 1 gpr registers");
      __ li(Rtmp, 0);
      __ efdcmpeq(CCR0, left->as_register_lo(), right->as_register_lo());
      __ bgt(CCR0, exit);
      if (!is_unordered_less) {
          __ li(Rtmp, -1);
          __ efdcmplt(CCR0, left->as_register_lo(), right->as_register_lo());
          __ bgt(CCR0, exit);
          __ li(Rtmp, 1);
      } else {
          __ li(Rtmp, 1);
          __ efdcmpgt(CCR0, left->as_register_lo(), right->as_register_lo());
          __ bgt(CCR0, exit);
          __ li(Rtmp, -1);
      }
    } else {
      ShouldNotReachHere();
    }
    __ bind(exit);
    __ mr(Rdst, Rtmp);
    return;
#endif
  } else if (code == lir_cmp_l2i) {
#ifndef PPC64
      Label exit;
    __ cmp(CCR0, left->as_register_hi(), right->as_register_hi());
    __ bne(CCR0, exit);
    __ cmpl(CCR0, left->as_register_lo(), right->as_register_lo());
    __ bind(exit);
#else
    __ cmp(CCR0, left->as_register_lo(), right->as_register_lo());
#endif
  } else {
    ShouldNotReachHere();
  }
  __ mfcr(R0); // set bit 32..33 as follows: <: 0b10, =: 0b00, >: 0b01
  __ srwi(Rdst, R0, 30);
  __ srawi(R0, R0, 31);
  __ orr(Rdst, R0, Rdst); // set result as follows: <: -1, =: 0, >: 1
  __ bind(done);
}


inline void load_to_reg(LIR_Assembler *lasm, LIR_Opr src, LIR_Opr dst) {
  if (src->is_constant()) {
    lasm->const2reg(src, dst, lir_patch_none, NULL);
  } else if (src->is_register()) {
    lasm->reg2reg(src, dst);
  } else if (src->is_stack()) {
    lasm->stack2reg(src, dst, dst->type());
  } else {
    ShouldNotReachHere();
  }
}

// ppc32: check if long opr can be loaded with isel op
// ppc64: always OK
static bool check_for_isel(LIR_Opr first, LIR_Opr second, LIR_Opr result) {
#ifndef _LP64
  if (result->is_double_cpu()) {
    const Register result_lo = result->as_register_lo();
    const Register first_lo  = first->as_register_lo();
    const Register second_lo = second->as_register_lo();
    const Register result_hi = result->as_register_hi();
    const Register first_hi  = first->as_register_hi();
    const Register second_hi = second->as_register_hi();

    if ( (result_lo == first_hi || result_lo == second_hi) &&
         (result_hi == first_lo || result_hi == second_lo)) return false;
  }
#endif
  return true;
}

void LIR_Assembler::cmove(LIR_Condition condition, LIR_Opr opr1, LIR_Opr opr2, LIR_Opr result, BasicType type) {
  if (opr1->is_equal(opr2) || opr1->is_same_register(opr2)) {
    load_to_reg(this, opr1, result); // Condition doesn't matter.
    return;
  }

  bool positive = false;
  Assembler::Condition cond = Assembler::equal;
  switch (condition) {
    case lir_cond_equal:        positive = true ; cond = Assembler::equal  ; break;
    case lir_cond_notEqual:     positive = false; cond = Assembler::equal  ; break;
    case lir_cond_less:         positive = true ; cond = Assembler::less   ; break;
    case lir_cond_belowEqual:
    case lir_cond_lessEqual:    positive = false; cond = Assembler::greater; break;
    case lir_cond_greater:      positive = true ; cond = Assembler::greater; break;
    case lir_cond_aboveEqual:
    case lir_cond_greaterEqual: positive = false; cond = Assembler::less   ; break;
    default:                    ShouldNotReachHere();
  }

  // Try to use isel on >=Power7.
  if (VM_Version::has_isel() && result->is_cpu_register()) {
    const bool o1_is_reg = opr1->is_cpu_register();
    const bool o2_is_reg = opr2->is_cpu_register();
    // We can use result to load one operand if not already in register.
    const LIR_Opr first = o1_is_reg ? opr1 : result;
    const LIR_Opr second = o2_is_reg ? opr2 : result;
    const bool ok_for_isel = check_for_isel(first, second, result);

    if (first != second && ok_for_isel) {
      if (!o1_is_reg) {
        load_to_reg(this, opr1, result);
      }

      if (!o2_is_reg) {
        load_to_reg(this, opr2, result);
      }

      if (result->is_single_cpu()) {
        __ isel(result->as_register(), BOOL_RESULT, cond, !positive, first->as_register(), second->as_register());
      } else {
        const Register result_lo = result->as_register_lo();
        const Register first_lo  = first->as_register_lo();
        const Register second_lo = second->as_register_lo();
#ifndef PPC64
        const Register result_hi = result->as_register_hi();
        const Register first_hi  = first->as_register_hi();
        const Register second_hi = second->as_register_hi();
#endif

#ifdef PPC64
        __ isel(result_lo, BOOL_RESULT, cond, !positive, first_lo, second_lo);
#else
        if (result_lo != first_hi && result_lo != second_hi)  {
          __ isel(result_lo, BOOL_RESULT, cond, !positive, first_lo, second_lo);
          __ isel(result_hi, BOOL_RESULT, cond, !positive, first_hi, second_hi);
        } else {
          __ isel(result_hi, BOOL_RESULT, cond, !positive, first_hi, second_hi);
          __ isel(result_lo, BOOL_RESULT, cond, !positive, first_lo, second_lo);
        }
#endif
      }
      return;
    }
  } // isel

  load_to_reg(this, opr1, result);

  Label skip;
  int bo = positive ? Assembler::bcondCRbiIs1 : Assembler::bcondCRbiIs0;
  int bi = Assembler::bi0(BOOL_RESULT, cond);
  __ bc(bo, bi, skip);

  load_to_reg(this, opr2, result);
  __ bind(skip);
}


void LIR_Assembler::arith_op(LIR_Code code, LIR_Opr left, LIR_Opr right, LIR_Opr dest,
                             CodeEmitInfo* info, bool pop_fpu_stack
#ifdef OPENJDK_PPC32
                             , LIR_Opr tmp
#endif
                             ) {
  assert(info == NULL, "unused on this code path");
  assert(left->is_register(), "wrong items state");
  assert(dest->is_register(), "wrong items state");

  if (right->is_register()) {
    // ladd, lsub and fadd, fsub are not supplied with temp register
    OPENJDK_PPC32_ONLY(Register temp = tmp->is_valid()?tmp->as_register() : R0);
    if (dest->is_float_kind()){
#ifndef USE_SPE
      FloatRegister lreg, rreg, res;
      if (right->is_single_fpu()) {
        lreg = left->as_float_reg();
        rreg = right->as_float_reg();
        res  = dest->as_float_reg();
        switch (code) {
          case lir_add: __ fadds(res, lreg, rreg); break;
          case lir_sub: __ fsubs(res, lreg, rreg); break;
          case lir_mul: // fall through
          case lir_mul_strictfp: __ fmuls(res, lreg, rreg); break;
          case lir_div: // fall through
          case lir_div_strictfp: __ fdivs(res, lreg, rreg); break;
          default: ShouldNotReachHere();
        }
      } else {
        lreg = left->as_double_reg();
        rreg = right->as_double_reg();
        res  = dest->as_double_reg();
        switch (code) {
          case lir_add: __ fadd(res, lreg, rreg); break;
          case lir_sub: __ fsub(res, lreg, rreg); break;
          case lir_mul: // fall through
          case lir_mul_strictfp: __ fmul(res, lreg, rreg); break;
          case lir_div: // fall through
          case lir_div_strictfp: __ fdiv(res, lreg, rreg); break;
          default: ShouldNotReachHere();
        }
      }
#else
      Register lreg, rreg, res;
      if (right->is_single_cpu()) {
        lreg = left->as_register();
        rreg = right->as_register();
        res  = dest->as_register();
        switch (code) {
          case lir_add:
            if (VM_Version::is_incorrect_single_fp()) {
              __ efdcfs(temp, lreg);
              __ efdcfs(res, rreg);
              __ efdadd(res, temp, res);
              __ efscfd(res, res);
            } else {
              __ efsadd(res, lreg, rreg);
            }
            break;
          case lir_sub:
            if (VM_Version::is_incorrect_single_fp()) {
              __ efdcfs(temp, lreg);
              __ efdcfs(res, rreg);
              __ efdsub(res, temp, res);
              __ efscfd(res, res);
            } else {
              __ efssub(res, lreg, rreg);
            }
            break;
          case lir_mul: // fall through
          case lir_mul_strictfp: __ efsmul(res, lreg, rreg); break;
          case lir_div: // fall through
          case lir_div_strictfp: __ efsdiv(res, lreg, rreg); break;
          default: ShouldNotReachHere();
        }
      } else {
        assert(dest->as_register_lo() == dest->as_register_hi(), "dest in 1 gpr registers");
        assert(right->as_register_lo() == right->as_register_hi(), "right in 1 gpr registers");
        assert(left->as_register_lo() == left->as_register_hi(), "left in 1 gpr registers");
        lreg = left->as_register_lo();
        rreg = right->as_register_lo();
        res  = dest->as_register_lo();
        switch (code) {
          case lir_add: __ efdadd(res, lreg, rreg); break;
          case lir_sub: __ efdsub(res, lreg, rreg); break;
          case lir_mul: // fall through
          case lir_mul_strictfp: __ efdmul(res, lreg, rreg); break;
          case lir_div: // fall through
          case lir_div_strictfp: __ efddiv(res, lreg, rreg); break;
          default: ShouldNotReachHere();
        }
      }
#endif
    } else if (dest->is_double_cpu()) {
#ifdef PPC64
      Register dst_lo = dest->as_register_lo();
      Register op1_lo = left->as_pointer_register();
      Register op2_lo = right->as_pointer_register();

      switch (code) {
        case lir_add: __ add(dst_lo, op1_lo, op2_lo); break;
        case lir_sub: __ sub(dst_lo, op1_lo, op2_lo); break;
        case lir_mul: __ mull(dst_lo, op1_lo, op2_lo); break;
        default: ShouldNotReachHere();
      }
#else // PPC64
      Register res_lo  = dest->as_register_lo();
      Register res_hi  = dest->as_register_hi();
      Register rreg_lo = right->as_register_lo();
      Register rreg_hi = right->as_register_hi();
      Register lreg_lo = left->as_register_lo();
      Register lreg_hi = left->as_register_hi();
      switch (code) {
        case lir_sub: {
          if(res_lo != rreg_hi && res_lo != lreg_hi) {
            temp = res_lo;
          } else {
            assert_different_registers(temp, res_lo);
          }
          __ subfc(temp, rreg_lo, lreg_lo);
          __ subfe(res_hi, rreg_hi, lreg_hi);
          __ mr_if_needed(res_lo, temp);
          break;
        }
        case lir_add: {
          if(res_lo != rreg_hi && res_lo != lreg_hi) {
            temp = res_lo;
          } else {
            assert_different_registers(temp, res_lo);
          }
          __ addc(temp, lreg_lo, rreg_lo);
          __ adde(res_hi, lreg_hi, rreg_hi);
          __ mr_if_needed(res_lo, temp);
          break;
        }
        case lir_mul:{
          const Register temp2 = res_hi;
          assert_different_registers(temp, temp2, rreg_lo, rreg_hi);
          assert_different_registers(temp, temp2, lreg_lo, lreg_hi);
          __ mullw(temp, lreg_hi, rreg_lo);
          __ mullw(temp2, lreg_lo, rreg_hi);
          __ add(temp2, temp, temp2);
          __ mulhwu(temp, lreg_lo, rreg_lo);
          __ mullw(res_lo, lreg_lo, rreg_lo);
          __ add(res_hi, temp2, temp);
          break;
        }
        default: ShouldNotReachHere();
      }
#endif // PPC64
    } else {  // destination is single
      assert (right->is_single_cpu(), "Just Checking");
      Register res  = dest->as_register();
      Register rreg = right->as_register();
      Register lreg = left->as_register();
      switch (code) {
        case lir_add:  __ add  (res, lreg, rreg); break;
        case lir_sub:  __ sub  (res, lreg, rreg); break;
        case lir_mul:  __ mullw(res, lreg, rreg); break;
        default: ShouldNotReachHere();
      }
    }
  } else {
    assert (right->is_constant(), "must be constant");
    if (dest->is_single_cpu()) {
      // dst is single, left is single, right is const
      Register lreg = left->as_register();
      Register res  = dest->as_register();
      int    simm16 = right->as_constant_ptr()->as_jint();
      switch (code) {
        case lir_sub:  assert(Assembler::is_simm16(-simm16), "cannot encode"); // see do_ArithmeticOp_Int
                       simm16 = -simm16;
        case lir_add:  if (res == lreg && simm16 == 0) break;
                       __ addi(res, lreg, simm16); break;
        case lir_mul:  if (res == lreg && simm16 == 1) break;
                       __ mulli(res, lreg, simm16); break;
        default: ShouldNotReachHere();
      }
    } else {
      jlong con = right->as_constant_ptr()->as_jlong();
      assert(Assembler::is_simm16(con), "must be simm16");
#ifdef PPC64
      Register lreg = left->as_pointer_register();
      Register res  = dest->as_register_lo();
      switch (code) {
        case lir_sub:  assert(Assembler::is_simm16(-con), "cannot encode");  // see do_ArithmeticOp_Long
                       con = -con;
        case lir_add:  if (res == lreg && con == 0) break;
                       __ addi(res, lreg, (int)con); break;
        case lir_mul:  if (res == lreg && con == 1) break;
                       __ mulli(res, lreg, (int)con); break;
        default: ShouldNotReachHere();
      }
#else
      Register res_lo  = dest->as_register_lo();
      Register res_hi  = dest->as_register_hi();
      Register temp = tmp->is_valid()?tmp->as_register():R0;
      // dst is double, left is single, right is const
      Register lreg_lo = left->as_register_lo();
      Register lreg_hi = left->as_register_hi();
      assert_different_registers(temp, lreg_lo, lreg_hi);
      switch (code) {
        case lir_sub:  assert(Assembler::is_simm16(-con), "cannot encode");  // see do_ArithmeticOp_Long
          con = -con;
        case lir_add: {
          if (res_lo == lreg_lo && res_hi == lreg_hi && con == 0) break;
          if(res_lo != lreg_hi) {
            temp = res_lo;
          } else {
            assert_different_registers(temp, res_lo);
          }

          __ addic_(temp, lreg_lo, con);
          if (con >= 0) {
            __ addze(res_hi, lreg_hi);
          } else {
            __ addme(res_hi, lreg_hi);
          }
          __ mr_if_needed(res_lo, temp);
          break;
        }
        case lir_mul: {
          if (res_lo == lreg_lo && res_hi == lreg_hi && con == 1) break;
          assert_different_registers(temp, res_lo, res_hi);
          assert_different_registers(res_hi, lreg_lo);
          __ load_const_optimized(temp, con);
          __ mulli(res_hi, lreg_hi, con);
          __ mulhwu(temp, lreg_lo, temp);
          __ add(res_hi, res_hi, temp);
          if (con < 0) {
            __ sub(res_hi, res_hi, lreg_lo);
          }
          __ mulli(res_lo, lreg_lo, con);
          break;
        }
        default: ShouldNotReachHere();
      }
#endif
    }
  }
}


void LIR_Assembler::fpop() {
  Unimplemented();
  // do nothing
}


void LIR_Assembler::intrinsic_op(LIR_Code code, LIR_Opr value, LIR_Opr thread, LIR_Opr dest, LIR_Op* op) {
  switch (code) {
    case lir_sqrt: {
#ifndef USE_SPE
        __ fsqrt(dest->as_double_reg(), value->as_double_reg());
#else
        ShouldNotReachHere();
#endif
      break;
    }
    case lir_abs: {
#ifndef USE_SPE
      __ fabs(dest->as_double_reg(), value->as_double_reg());
#else
      assert(value->as_register_lo() == value->as_register_hi(), "value in 1 gpr registers");
      assert(dest->as_register_lo() == dest->as_register_hi(), "dest in 1 gpr registers");
      __ efdabs(dest->as_register_lo(), value->as_register_lo());
#endif
      break;
    }
    default: {
      ShouldNotReachHere();
      break;
    }
  }
}


void LIR_Assembler::logic_op(LIR_Code code, LIR_Opr left, LIR_Opr right, LIR_Opr dest) {
  if (right->is_constant()) { // see do_LogicOp
    PPC64_ONLY(long) NOT_PPC64(long long) uimm;
    Register d_lo, d_hi, l_lo, l_hi;
    if (dest->is_single_cpu()) {
      uimm = right->as_constant_ptr()->as_jint();
      d_lo = dest->as_register();
      d_hi = noreg;
      l_lo = left->as_register();
      l_hi = noreg;
    } else {
      uimm = right->as_constant_ptr()->as_jlong();
      d_lo = dest->as_register_lo();
      d_hi = dest->as_register_hi();
      l_lo = left->as_register_lo();
      l_hi = left->as_register_hi();
    }

#if defined(PPC64)
    long uimms   = (unsigned long)uimm >> 16;
    long uimmss  = (unsigned long)uimm >> 32;
#else
    const long long WordMask = (1LL << BitsPerWord) - 1;

    const unsigned long i0  = uimm;
    const unsigned long i0m = i0 & 0xFFFF;
    const unsigned long i1  = uimm >> 16;
    const unsigned long i1m = i1 & 0xFFFF;
    const unsigned long i2  = uimm >> 32;
    const unsigned long i2m = i2 & 0xFFFF;
    const unsigned long i3  = uimm >> 48;
    const unsigned long i3m = i3 & 0xFFFF;
    Register temp = d_lo;
    if (dest->is_double_cpu() && (d_lo == l_hi)) {
      temp = R0;
    }
#endif

    switch (code) {
      case lir_logic_and:
#if defined(PPC64)
        if (uimmss != 0 || (uimms != 0 && (uimm & 0xFFFF) != 0) || is_power_of_2_long(uimm)) {
          __ andi(d_lo, l_lo, uimm); // special cases
        } else if (uimms != 0) {
            __ andis_(d_lo, l_lo, uimms);
        }
        else { __ andi_(d_lo, l_lo, uimm); }
#else
        if (i1m) {
          assert(i0m == 0, "sanity");
          __ andis_(temp, l_lo, i1m);
        } else {
          __ andi_(temp, l_lo, i0m);
        }

        if (dest->is_single_cpu()) {
          assert(i2 == 0, "const should be int size");
        } else {
          if (i3m) {
            assert(i2m == 0, "sanity");
            __ andis_(d_hi, l_hi, i3m);
          } else {
            __ andi_(d_hi, l_hi, i2m);
          }
        }
#endif
        break;

      case lir_logic_or:
#if defined(PPC64)
        if (uimms != 0) { assert((uimm & 0xFFFF) == 0, "sanity"); __ oris(d_lo, l_lo, uimms); }
        else { __ ori(d_lo, l_lo, uimm); }
#else
        if (i1m) {
          assert(i0m == 0, "sanity");
          __ oris(temp, l_lo, i1m);
        } else {
          __ ori(temp, l_lo, i0m);
        }

        if (dest->is_single_cpu()) {
          assert(i2 == 0, "const should be int size");
        } else {
          if (i3m) {
            assert(i2m == 0, "sanity");
            __ oris(d_hi, l_hi, i3m);
          } else {
            __ ori(d_hi, l_hi, i2m);
          }
        }
#endif
        break;

      case lir_logic_xor:
#if defined(PPC64)
        if (uimm == -1) { __ nand(d_lo, l_lo, l_lo); } // special case
        else if (uimms != 0) { assert((uimm & 0xFFFF) == 0, "sanity"); __ xoris(d_lo, l_lo, uimms); }
        else { __ xori(d_lo, l_lo, uimm); }
#else
        if (i0m == 0xFFFF && i1m == 0xFFFF) {
          __ nand(temp, l_lo, l_lo);
        } else if (i1m) {
          assert(i0m == 0, "sanity");
          __ xoris(temp, l_lo, i1m);
        } else {
          __ xori(temp, l_lo, i0m);
        }

        if (dest->is_single_cpu()) {
          assert(i2 == 0, "const should be int size");
        } else {
          if (i2 == 0xFFFFFFFF) {
            __ nand(d_hi, l_hi, l_hi);
          } else if (i3m) {
            assert(i2m == 0, "sanity");
            __ xoris(d_hi, l_hi, i3m);
          } else {
            __ xori(d_hi, l_hi, i2m);
          }
        }
#endif
        break;

      default: ShouldNotReachHere();
    }
#ifndef PPC64
    __ mr_if_needed(d_lo, temp);
#endif
  } else {
    assert(right->is_register(), "right should be in register");

    if (dest->is_single_cpu()) {
      switch (code) {
        case lir_logic_and: __ andr(dest->as_register(), left->as_register(), right->as_register()); break;
        case lir_logic_or:  __ orr (dest->as_register(), left->as_register(), right->as_register()); break;
        case lir_logic_xor: __ xorr(dest->as_register(), left->as_register(), right->as_register()); break;
        default: ShouldNotReachHere();
      }
    } else {
      Register temp = dest->as_register_lo();
#if defined(PPC64)
      Register l_lo = (left->is_single_cpu() && left->is_oop_register()) ? left->as_register() :
                                                                        left->as_register_lo();
      Register r_lo = (right->is_single_cpu() && right->is_oop_register()) ? right->as_register() :
                                                                          right->as_register_lo();
      Register d_lo = dest->as_register_lo();
#else
      Register l_lo = left->as_register_lo();
      Register l_hi = left->as_register_hi();
      Register r_lo = right->as_register_lo();
      Register r_hi = right->as_register_hi();
      Register d_lo = dest->as_register_lo();
      Register d_hi = dest->as_register_hi();
      if (d_lo == l_hi || d_lo == r_hi) {
        temp = R0;
      }
#endif
      switch (code) {
        case lir_logic_and:
            __ andr(temp, l_lo, r_lo);
            NOT_PPC64(__ andr(dest->as_register_hi(), l_hi, r_hi));
        break;
        case lir_logic_or:
            __ orr (temp, l_lo, r_lo);
            NOT_PPC64(__ orr(dest->as_register_hi(), l_hi, r_hi));
        break;
        case lir_logic_xor:
            __ xorr(temp, l_lo, r_lo);
            NOT_PPC64(__ xorr(dest->as_register_hi(), l_hi, r_hi));
        break;
        default: ShouldNotReachHere();
      }
      __ mr_if_needed(d_lo, temp);
    }
  }
}


int LIR_Assembler::shift_amount(BasicType t) {
  int elem_size = type2aelembytes(t);
  switch (elem_size) {
    case 1 : return 0;
    case 2 : return 1;
    case 4 : return 2;
    case 8 : return 3;
  }
  ShouldNotReachHere();
  return -1;
}


void LIR_Assembler::throw_op(LIR_Opr exceptionPC, LIR_Opr exceptionOop, CodeEmitInfo* info) {
  info->add_register_oop(exceptionOop);

  // Reuse the debug info from the safepoint poll for the throw op itself.
  address pc_for_athrow = __ pc();
  int pc_for_athrow_offset = __ offset();
  //RelocationHolder rspec = internal_word_Relocation::spec(pc_for_athrow);
  //__ relocate(rspec);
  //__ load_const(exceptionPC->as_register(), pc_for_athrow, R0);
  __ calculate_address_from_global_toc(exceptionPC->as_register(), pc_for_athrow, true, true, /*add_relocation*/ true);
  add_call_info(pc_for_athrow_offset, info); // for exception handler

  address stub = Runtime1::entry_for(compilation()->has_fpu_code() ? Runtime1::handle_exception_id
                                                                   : Runtime1::handle_exception_nofpu_id);
#ifdef PPC64
  __ add_const_optimized(R0, R29_TOC, MacroAssembler::offset_to_global_toc(stub));
#else
  __ load_const_optimized(R0, stub);
#endif
  __ mtctr(R0);
  __ bctr();
}


void LIR_Assembler::unwind_op(LIR_Opr exceptionOop) {
  // Note: Not used with EnableDebuggingOnDemand.
  assert(exceptionOop->as_register() == R3, "should match");
  __ b(_unwind_handler_entry);
}


void LIR_Assembler::emit_arraycopy(LIR_OpArrayCopy* op) {
  Register src = op->src()->as_register();
  Register dst = op->dst()->as_register();
  Register src_pos = op->src_pos()->as_register();
  Register dst_pos = op->dst_pos()->as_register();
  Register length  = op->length()->as_register();
  Register tmp = op->tmp()->as_register();
  Register tmp2 = R0;

  int flags = op->flags();
  ciArrayKlass* default_type = op->expected_type();
  BasicType basic_type = default_type != NULL ? default_type->element_type()->basic_type() : T_ILLEGAL;
  if (basic_type == T_ARRAY) basic_type = T_OBJECT;

  // Set up the arraycopy stub information.
  ArrayCopyStub* stub = op->stub();
  const int frame_resize = frame::abi_reg_args_size - sizeof(frame::jit_abi); // C calls need larger frame.

  // Always do stub if no type information is available. It's ok if
  // the known type isn't loaded since the code sanity checks
  // in debug mode and the type isn't required when we know the exact type
  // also check that the type is an array type.
  if (op->expected_type() == NULL) {
    assert(src->is_nonvolatile() && src_pos->is_nonvolatile() && dst->is_nonvolatile() && dst_pos->is_nonvolatile() &&
           length->is_nonvolatile(), "must preserve");
    // 3 parms are int. Convert to long.
    __ mr(R3_ARG1, src);
    __ extsw(R4_ARG2, src_pos);
    __ mr(R5_ARG3, dst);
    __ extsw(R6_ARG4, dst_pos);
    __ extsw(R7_ARG5, length);
    address copyfunc_addr = StubRoutines::generic_arraycopy();

    if (copyfunc_addr == NULL) { // Use C version if stub was not generated.
      address entry = CAST_FROM_FN_PTR(address, Runtime1::arraycopy);
      __ call_c_with_frame_resize(entry, frame_resize);
    } else {
#ifndef PRODUCT
      if (PrintC1Statistics) {
        address counter = (address)&Runtime1::_generic_arraycopystub_cnt;
        int simm16_offs = __ load_const_optimized(tmp, counter, tmp2, true);
        __ lwz(R11_scratch1, simm16_offs, tmp);
        __ addi(R11_scratch1, R11_scratch1, 1);
        __ stw(R11_scratch1, simm16_offs, tmp);
      }
#endif
      __ call_c_with_frame_resize(copyfunc_addr, /*stub does not need resized frame*/ 0);

      __ nand(tmp, R3_RET, R3_RET);
      __ subf(length, tmp, length);
      __ add(src_pos, tmp, src_pos);
      __ add(dst_pos, tmp, dst_pos);
    }

    __ cmpwi(CCR0, R3_RET, 0);
    __ bc_far_optimized(Assembler::bcondCRbiIs1, __ bi0(CCR0, Assembler::less), *stub->entry());
    __ bind(*stub->continuation());
    return;
  }

  assert(default_type != NULL && default_type->is_array_klass(), "must be true at this point");
  Label cont, slow, copyfunc;

  bool simple_check_flag_set = flags & (LIR_OpArrayCopy::src_null_check |
                                        LIR_OpArrayCopy::dst_null_check |
                                        LIR_OpArrayCopy::src_pos_positive_check |
                                        LIR_OpArrayCopy::dst_pos_positive_check |
                                        LIR_OpArrayCopy::length_positive_check);

  // Use only one conditional branch for simple checks.
  if (simple_check_flag_set) {
    ConditionRegister combined_check = CCR1, tmp_check = CCR1;

    // Make sure src and dst are non-null.
    if (flags & LIR_OpArrayCopy::src_null_check) {
      __ cmpi(combined_check, src, 0);
      tmp_check = CCR0;
    }

    if (flags & LIR_OpArrayCopy::dst_null_check) {
      __ cmpi(tmp_check, dst, 0);
      if (tmp_check != combined_check) {
        __ cror(combined_check, Assembler::equal, tmp_check, Assembler::equal);
      }
      tmp_check = CCR0;
    }

    // Clear combined_check.eq if not already used.
    if (tmp_check == combined_check) {
      __ crandc(combined_check, Assembler::equal, combined_check, Assembler::equal);
      tmp_check = CCR0;
    }

    if (flags & LIR_OpArrayCopy::src_pos_positive_check) {
      // Test src_pos register.
      __ cmpwi(tmp_check, src_pos, 0);
      __ cror(combined_check, Assembler::equal, tmp_check, Assembler::less);
    }

    if (flags & LIR_OpArrayCopy::dst_pos_positive_check) {
      // Test dst_pos register.
      __ cmpwi(tmp_check, dst_pos, 0);
      __ cror(combined_check, Assembler::equal, tmp_check, Assembler::less);
    }

    if (flags & LIR_OpArrayCopy::length_positive_check) {
      // Make sure length isn't negative.
      __ cmpwi(tmp_check, length, 0);
      __ cror(combined_check, Assembler::equal, tmp_check, Assembler::less);
    }

    __ beq(combined_check, slow);
  }

  // Higher 32bits must be null.
  __ extsw(length, length);

  __ extsw(src_pos, src_pos);
  if (flags & LIR_OpArrayCopy::src_range_check) {
    __ lwz(tmp2, arrayOopDesc::length_offset_in_bytes(), src);
    __ add(tmp, length, src_pos);
    __ cmpl(CCR0, tmp2, tmp);
    __ blt(CCR0, slow);
  }

  __ extsw(dst_pos, dst_pos);
  if (flags & LIR_OpArrayCopy::dst_range_check) {
    __ lwz(tmp2, arrayOopDesc::length_offset_in_bytes(), dst);
    __ add(tmp, length, dst_pos);
    __ cmpl(CCR0, tmp2, tmp);
    __ blt(CCR0, slow);
  }

  int shift = shift_amount(basic_type);

  if (!(flags & LIR_OpArrayCopy::type_check)) {
    __ b(cont);
  } else {
    // We don't know the array types are compatible.
    if (basic_type != T_OBJECT) {
      // Simple test for basic type arrays.
      if (UseCompressedClassPointers) {
        // We don't need decode because we just need to compare.
        __ lwz(tmp, oopDesc::klass_offset_in_bytes(), src);
        __ lwz(tmp2, oopDesc::klass_offset_in_bytes(), dst);
        __ cmpw(CCR0, tmp, tmp2);
      } else {
        __ l(tmp, oopDesc::klass_offset_in_bytes(), src);
        __ l(tmp2, oopDesc::klass_offset_in_bytes(), dst);
        __ cmp(CCR0, tmp, tmp2);
     }
      __ beq(CCR0, cont);
    } else {
      // For object arrays, if src is a sub class of dst then we can
      // safely do the copy.
      address copyfunc_addr = StubRoutines::checkcast_arraycopy();

      const Register sub_klass = R5, super_klass = R4; // like CheckCast/InstanceOf
      assert_different_registers(tmp, tmp2, sub_klass, super_klass);

      __ load_klass(sub_klass, src);
      __ load_klass(super_klass, dst);

      __ check_klass_subtype_fast_path(sub_klass, super_klass, tmp, tmp2,
                                       &cont, copyfunc_addr != NULL ? &copyfunc : &slow, NULL);

      address slow_stc = Runtime1::entry_for(Runtime1::slow_subtype_check_id);
      //__ load_const_optimized(tmp, slow_stc, tmp2);
      __ calculate_address_from_global_toc(tmp, slow_stc, true, true, false);
      __ mtctr(tmp);
      __ bctrl(); // sets CR0
      __ beq(CCR0, cont);

      if (copyfunc_addr != NULL) { // Use stub if available.
        __ bind(copyfunc);
        // Src is not a sub class of dst so we have to do a
        // per-element check.
        int mask = LIR_OpArrayCopy::src_objarray|LIR_OpArrayCopy::dst_objarray;
        if ((flags & mask) != mask) {
          assert(flags & mask, "one of the two should be known to be an object array");

          if (!(flags & LIR_OpArrayCopy::src_objarray)) {
            __ load_klass(tmp, src);
          } else if (!(flags & LIR_OpArrayCopy::dst_objarray)) {
            __ load_klass(tmp, dst);
          }

          __ lwz(tmp2, in_bytes(Klass::layout_helper_offset()), tmp);

          jint objArray_lh = Klass::array_layout_helper(T_OBJECT);
          __ load_const_optimized(tmp, objArray_lh);
          __ cmpw(CCR0, tmp, tmp2);
          __ bne(CCR0, slow);
        }

        Register src_ptr = R3_ARG1;
        Register dst_ptr = R4_ARG2;
        Register len     = R5_ARG3;
        Register chk_off = R6_ARG4;
        Register super_k = R7_ARG5;

        __ addi(src_ptr, src, arrayOopDesc::base_offset_in_bytes(basic_type));
        __ addi(dst_ptr, dst, arrayOopDesc::base_offset_in_bytes(basic_type));
        if (shift == 0) {
          __ add(src_ptr, src_pos, src_ptr);
          __ add(dst_ptr, dst_pos, dst_ptr);
        } else {
          __ sli(tmp, src_pos, shift);
          __ sli(tmp2, dst_pos, shift);
          __ add(src_ptr, tmp, src_ptr);
          __ add(dst_ptr, tmp2, dst_ptr);
        }

        __ load_klass(tmp, dst);
        __ mr(len, length);

        int ek_offset = in_bytes(ObjArrayKlass::element_klass_offset());
        __ l(super_k, ek_offset, tmp);

        int sco_offset = in_bytes(Klass::super_check_offset_offset());
        __ lwz(chk_off, sco_offset, super_k);

        __ call_c_with_frame_resize(copyfunc_addr, /*stub does not need resized frame*/ 0);

#ifndef PRODUCT
        if (PrintC1Statistics) {
          Label failed;
          __ cmpwi(CCR0, R3_RET, 0);
          __ bne(CCR0, failed);
          address counter = (address)&Runtime1::_arraycopy_checkcast_cnt;
          int simm16_offs = __ load_const_optimized(tmp, counter, tmp2, true);
          __ lwz(R11_scratch1, simm16_offs, tmp);
          __ addi(R11_scratch1, R11_scratch1, 1);
          __ stw(R11_scratch1, simm16_offs, tmp);
          __ bind(failed);
        }
#endif

        __ nand(tmp, R3_RET, R3_RET);
        __ cmpwi(CCR0, R3_RET, 0);
        __ beq(CCR0, *stub->continuation());

#ifndef PRODUCT
        if (PrintC1Statistics) {
          address counter = (address)&Runtime1::_arraycopy_checkcast_attempt_cnt;
          int simm16_offs = __ load_const_optimized(tmp, counter, tmp2, true);
          __ lwz(R11_scratch1, simm16_offs, tmp);
          __ addi(R11_scratch1, R11_scratch1, 1);
          __ stw(R11_scratch1, simm16_offs, tmp);
        }
#endif

        __ subf(length, tmp, length);
        __ add(src_pos, tmp, src_pos);
        __ add(dst_pos, tmp, dst_pos);
      }
    }
  }
  __ bind(slow);
  __ b(*stub->entry());
  __ bind(cont);

#ifdef ASSERT
  if (basic_type != T_OBJECT || !(flags & LIR_OpArrayCopy::type_check)) {
    // Sanity check the known type with the incoming class. For the
    // primitive case the types must match exactly with src.klass and
    // dst.klass each exactly matching the default type. For the
    // object array case, if no type check is needed then either the
    // dst type is exactly the expected type and the src type is a
    // subtype which we can't check or src is the same array as dst
    // but not necessarily exactly of type default_type.
    Label known_ok, halt;
    metadata2reg(op->expected_type()->constant_encoding(), tmp);
    if (UseCompressedClassPointers) {
      // Tmp holds the default type. It currently comes uncompressed after the
      // load of a constant, so encode it.
      __ encode_klass_not_null(tmp);
      // Load the raw value of the dst klass, since we will be comparing
      // uncompressed values directly.
      __ lwz(tmp2, oopDesc::klass_offset_in_bytes(), dst);
      __ cmpw(CCR0, tmp, tmp2);
      if (basic_type != T_OBJECT) {
        __ bne(CCR0, halt);
        // Load the raw value of the src klass.
        __ lwz(tmp2, oopDesc::klass_offset_in_bytes(), src);
        __ cmpw(CCR0, tmp, tmp2);
        __ beq(CCR0, known_ok);
      } else {
        __ beq(CCR0, known_ok);
        __ cmpw(CCR0, src, dst);
        __ beq(CCR0, known_ok);
      }
    } else {
      __ l(tmp2, oopDesc::klass_offset_in_bytes(), dst);
      __ cmp(CCR0, tmp, tmp2);
      if (basic_type != T_OBJECT) {
        __ bne(CCR0, halt);
        // Load the raw value of the src klass.
        __ l(tmp2, oopDesc::klass_offset_in_bytes(), src);
        __ cmp(CCR0, tmp, tmp2);
        __ beq(CCR0, known_ok);
      } else {
        __ beq(CCR0, known_ok);
        __ cmp(CCR0, src, dst);
        __ beq(CCR0, known_ok);
      }
    }
    __ bind(halt);
    __ stop("incorrect type information in arraycopy");
    __ bind(known_ok);
  }
#endif

#ifndef PRODUCT
  if (PrintC1Statistics) {
    address counter = Runtime1::arraycopy_count_address(basic_type);
    int simm16_offs = __ load_const_optimized(tmp, counter, tmp2, true);
    __ lwz(R11_scratch1, simm16_offs, tmp);
    __ addi(R11_scratch1, R11_scratch1, 1);
    __ stw(R11_scratch1, simm16_offs, tmp);
  }
#endif

  Register src_ptr = R3_ARG1;
  Register dst_ptr = R4_ARG2;
  Register len     = R5_ARG3;

  __ addi(src_ptr, src, arrayOopDesc::base_offset_in_bytes(basic_type));
  __ addi(dst_ptr, dst, arrayOopDesc::base_offset_in_bytes(basic_type));
  if (shift == 0) {
    __ add(src_ptr, src_pos, src_ptr);
    __ add(dst_ptr, dst_pos, dst_ptr);
  } else {
    __ sli(tmp, src_pos, shift);
    __ sli(tmp2, dst_pos, shift);
    __ add(src_ptr, tmp, src_ptr);
    __ add(dst_ptr, tmp2, dst_ptr);
  }

  bool disjoint = (flags & LIR_OpArrayCopy::overlapping) == 0;
  bool aligned = (flags & LIR_OpArrayCopy::unaligned) == 0;
  const char *name;
  address entry = StubRoutines::select_arraycopy_function(basic_type, aligned, disjoint, name, false);

  // Arraycopy stubs takes a length in number of elements, so don't scale it.
  __ mr(len, length);
  __ call_c_with_frame_resize(entry, /*stub does not need resized frame*/ 0);

  __ bind(*stub->continuation());
}


void LIR_Assembler::shift_op(LIR_Code code, LIR_Opr left, LIR_Opr count, LIR_Opr dest, LIR_Opr tmp) {
  if (dest->is_single_cpu()) {
#ifdef PPC64
#ifdef _LP64
    __ rlicl(tmp->as_register(), count->as_register(), 0, 64-5);
    if (left->type() == T_OBJECT) {
      switch (code) {
        case lir_shl:  __ sld(dest->as_register(), left->as_register(), tmp->as_register()); break;
        case lir_shr:  __ srad(dest->as_register(), left->as_register(), tmp->as_register()); break;
        case lir_ushr: __ srd(dest->as_register(), left->as_register(), tmp->as_register()); break;
        default: ShouldNotReachHere();
      }
    } else
#endif // _LP64
      switch (code) {
        case lir_shl:  __ slw(dest->as_register(), left->as_register(), tmp->as_register()); break;
        case lir_shr:  __ sraw(dest->as_register(), left->as_register(), tmp->as_register()); break;
        case lir_ushr: __ srw(dest->as_register(), left->as_register(), tmp->as_register()); break;
        default: ShouldNotReachHere();
      }
#else // PPC64
      const Register r_lft = left->as_register();
      const Register r_dst = dest->as_register();
      const Register r_cnt = count->as_register();
      const Register r_tmp = tmp->as_register();

      __ clrlwi(r_tmp, r_cnt, 32-5);
      switch (code) {
        case lir_shl:  __ slw(r_dst, r_lft, r_tmp); break;
        case lir_shr:  __ sraw(r_dst, r_lft, r_tmp); break;
        case lir_ushr: __ srw(r_dst, r_lft, r_tmp); break;
        default: ShouldNotReachHere();
      }
#endif // PPC64

  } else {
#ifdef PPC64
    __ rldicl(tmp->as_register(), count->as_register(), 0, 64-6);
    switch (code) {
      case lir_shl:  __ sld(dest->as_register_lo(), left->as_register_lo(), tmp->as_register()); break;
      case lir_shr:  __ srad(dest->as_register_lo(), left->as_register_lo(), tmp->as_register()); break;
      case lir_ushr: __ srd(dest->as_register_lo(), left->as_register_lo(), tmp->as_register()); break;
      default: ShouldNotReachHere();
    }
#else
      const Register lreg = left->as_register_lo();
      const Register dreg = dest->as_register_lo();
      const Register lreg_hi = left->as_register_hi();
      const Register dreg_hi = dest->as_register_hi();
      const Register cnt = count->as_register();
      const Register temp = tmp->as_register();
      const Register temp2 = R0;
      assert(temp->is_valid(), "shift_op need tmp reg");
      assert_different_registers(temp, temp2);
      assert_different_registers(temp, lreg);
      assert_different_registers(temp, lreg_hi);
      const int word_bits = 8 * wordSize;
      __ clrlwi(temp, cnt,  32-6);
      switch (code) {
        case lir_shl: {
          assert_different_registers(temp, dreg_hi, lreg);
          Label Llt32, Ldone;

          __ cmpi(CCR0, temp, 32);
          __ blt(CCR0, Llt32);

          __ addi(temp, temp, -32);
          __ slw(dreg_hi, lreg, temp);
          __ li(dreg, 0);
          __ b(Ldone);

          __ bind(Llt32);

          __ slw(dreg_hi, lreg_hi, temp);
          __ subfic(dreg, temp, 32);
          __ srw(temp2, lreg, dreg);
          __ orr(dreg_hi, dreg_hi, temp2);
          __ slw(dreg, lreg, temp);

          __ bind(Ldone);
          break;
        }
        case lir_shr: {
          assert_different_registers(temp, dreg, lreg_hi);
          Label Llt32, Ldone;

          __ cmpi(CCR0, temp, 32);
          __ blt(CCR0, Llt32);

          __ addi(temp, temp, -32);
          __ sraw(dreg, lreg_hi, temp);
          __ srawi(dreg_hi, lreg_hi, 31);
          __ b(Ldone);

          __ bind(Llt32);

          __ srw(dreg, lreg, temp);
          __ subfic(dreg_hi, temp, 32);
          __ slw(temp2, lreg_hi, dreg_hi);
          __ sraw(dreg_hi, lreg_hi, temp);
          __ orr(dreg, dreg, temp2);

          __ bind(Ldone);

          break;
        }
        case lir_ushr: {
          assert_different_registers(temp, dreg, lreg_hi);
          Label Llt32, Ldone;

          __ cmpi(CCR0, temp, 32);
          __ blt(CCR0, Llt32);

          __ addi(temp, temp, -32);
          __ srw(dreg, lreg_hi, temp);
          __ li(dreg_hi, 0);
          __ b(Ldone);

          __ bind(Llt32);

          __ srw(dreg, lreg, temp);
          __ subfic(dreg_hi, temp, 32);
          __ slw(temp2, lreg_hi, dreg_hi);
          __ srw(dreg_hi, lreg_hi, temp);
          __ orr(dreg, dreg, temp2);

          __ bind(Ldone);

          break;
        }
        default:
          ShouldNotReachHere();
          break;
      }
#endif
  }
}


void LIR_Assembler::shift_op(LIR_Code code, LIR_Opr left, jint count, LIR_Opr dest) {
#ifdef _LP64
  if (left->type() == T_OBJECT) {
    count = count & 63;  // Shouldn't shift by more than sizeof(intptr_t).
    if (count == 0) { __ mr_if_needed(dest->as_register_lo(), left->as_register()); }
    else {
      switch (code) {
        case lir_shl:  __ sli(dest->as_register_lo(), left->as_register(), count); break;
        case lir_shr:  __ srai(dest->as_register_lo(), left->as_register(), count); break;
        case lir_ushr: __ sri(dest->as_register_lo(), left->as_register(), count); break;
        default: ShouldNotReachHere();
      }
    }
    return;
  }
#endif
  const int word_bits = 8 * wordSize;
  if (dest->is_single_cpu()) {
    count = count & 0x1F; // Java spec
    if (count == 0) { __ mr_if_needed(dest->as_register(), left->as_register()); }
    else {
      switch (code) {
        case lir_shl: __ slwi(dest->as_register(), left->as_register(), count); break;
        case lir_shr:  __ srawi(dest->as_register(), left->as_register(), count); break;
        case lir_ushr: __ srwi(dest->as_register(), left->as_register(), count); break;
        default: ShouldNotReachHere();
      }
    }
  } else if (dest->is_double_cpu()) {
    count = count & 63; // Java spec
#ifdef PPC64
    if (count == 0) { __ mr_if_needed(dest->as_pointer_register(), left->as_pointer_register()); }
    else {
      switch (code) {
        case lir_shl:  __ sli(dest->as_pointer_register(), left->as_pointer_register(), count); break;
        case lir_shr:  __ srai(dest->as_pointer_register(), left->as_pointer_register(), count); break;
        case lir_ushr: __ sri(dest->as_pointer_register(), left->as_pointer_register(), count); break;
        default: ShouldNotReachHere();
      }
    }
#else
    if (count == 0) {
      guarantee(dest->as_register_lo() != left->as_register_hi(), "Must be different!");
      __ mr_if_needed(dest->as_register_lo(), left->as_register_lo());
      __ mr_if_needed(dest->as_register_hi(), left->as_register_hi());
    } else {
      switch (code) {
        case lir_shl:
          if (count >= word_bits) {
            __ slwi(dest->as_register_hi(), left->as_register_lo(), count - word_bits);
            __ load_const32(dest->as_register_lo(), 0);
          } else {
            guarantee(dest->as_register_hi() != left->as_register_lo(), "Must be different!");
            __ sli(dest->as_register_hi(), left->as_register_hi(), count);
            __ rlwimi(dest->as_register_hi(), left->as_register_lo(), count, word_bits - count, word_bits - 1);
            __ sli(dest->as_register_lo(), left->as_register_lo(), count);
          }
          break;
        case lir_shr:
          if (count >= word_bits) {
            __ srawi(dest->as_register_lo(), left->as_register_hi(), count - word_bits);
            __ srawi(dest->as_register_hi(), left->as_register_hi(), word_bits - 1);
          } else {
            guarantee(dest->as_register_lo() != left->as_register_hi(), "Must be different!");
            __ sri(dest->as_register_lo(), left->as_register_lo(), count);
            __ rlwimi(dest->as_register_lo(), left->as_register_hi(), word_bits - count, 0, count - 1);
            __ srai(dest->as_register_hi(), left->as_register_hi(), count);
          }
          break;
        case lir_ushr:
          if (count >= word_bits) {
            __ srwi(dest->as_register_lo(), left->as_register_hi(), count - word_bits);
            __ load_const32(dest->as_register_hi(), 0);
          } else {
            guarantee(dest->as_register_lo() != left->as_register_hi(), "Must be different!");
            __ sri(dest->as_register_lo(), left->as_register_lo(), count);
            __ rlwimi(dest->as_register_lo(), left->as_register_hi(), word_bits - count, 0, count - 1);
            __ sri(dest->as_register_hi(), left->as_register_hi(), count);
          }
          break;
        default: ShouldNotReachHere();
      }
    }
#endif
  } else {
    ShouldNotReachHere();
  }
}


void LIR_Assembler::emit_alloc_obj(LIR_OpAllocObj* op) {
  if (op->init_check()) {
    if (!os::zero_page_read_protected() || !ImplicitNullChecks) {
      explicit_null_check(op->klass()->as_register(), op->stub()->info());
    } else {
      add_debug_info_for_null_check_here(op->stub()->info());
    }
    __ lbz(op->tmp1()->as_register(),
           in_bytes(InstanceKlass::init_state_offset()), op->klass()->as_register());
    __ cmpwi(CCR0, op->tmp1()->as_register(), InstanceKlass::fully_initialized);
    __ bc_far_optimized(Assembler::bcondCRbiIs0, __ bi0(CCR0, Assembler::equal), *op->stub()->entry());
  }
  __ allocate_object(op->obj()->as_register(),
                     op->tmp1()->as_register(),
                     op->tmp2()->as_register(),
                     op->tmp3()->as_register(),
                     op->header_size(),
                     op->object_size(),
                     op->klass()->as_register(),
                     *op->stub()->entry());

  __ bind(*op->stub()->continuation());
  __ verify_oop(op->obj()->as_register());
}


void LIR_Assembler::emit_alloc_array(LIR_OpAllocArray* op) {
  LP64_ONLY( __ extsw(op->len()->as_register(), op->len()->as_register()); )
  if (UseSlowPath ||
      (!UseFastNewObjectArray && (op->type() == T_OBJECT || op->type() == T_ARRAY)) ||
      (!UseFastNewTypeArray   && (op->type() != T_OBJECT && op->type() != T_ARRAY))) {
    __ b(*op->stub()->entry());
  } else {
    __ allocate_array(op->obj()->as_register(),
                      op->len()->as_register(),
                      op->tmp1()->as_register(),
                      op->tmp2()->as_register(),
                      op->tmp3()->as_register(),
                      arrayOopDesc::header_size(op->type()),
                      type2aelembytes(op->type()),
                      op->klass()->as_register(),
                      *op->stub()->entry());
  }
  __ bind(*op->stub()->continuation());
}


void LIR_Assembler::type_profile_helper(Register mdo, int mdo_offset_bias,
                                        ciMethodData *md, ciProfileData *data,
                                        Register recv, Register tmp1, Label* update_done) {
  uint i;
  for (i = 0; i < VirtualCallData::row_limit(); i++) {
    Label next_test;
    // See if the receiver is receiver[n].
    __ l(tmp1, md->byte_offset_of_slot(data, ReceiverTypeData::receiver_offset(i)) - mdo_offset_bias, mdo);
    __ verify_klass_ptr(tmp1);
    __ cmp(CCR0, recv, tmp1);
    __ bne(CCR0, next_test);

    __ l(tmp1, md->byte_offset_of_slot(data, ReceiverTypeData::receiver_count_offset(i)) - mdo_offset_bias, mdo);
    __ addi(tmp1, tmp1, DataLayout::counter_increment);
    __ st(tmp1, md->byte_offset_of_slot(data, ReceiverTypeData::receiver_count_offset(i)) - mdo_offset_bias, mdo);
    __ b(*update_done);

    __ bind(next_test);
  }

  // Didn't find receiver; find next empty slot and fill it in.
  for (i = 0; i < VirtualCallData::row_limit(); i++) {
    Label next_test;
    __ l(tmp1, md->byte_offset_of_slot(data, ReceiverTypeData::receiver_offset(i)) - mdo_offset_bias, mdo);
    __ cmpi(CCR0, tmp1, 0);
    __ bne(CCR0, next_test);
    __ li(tmp1, DataLayout::counter_increment);
    __ st(recv, md->byte_offset_of_slot(data, ReceiverTypeData::receiver_offset(i)) - mdo_offset_bias, mdo);
    __ st(tmp1, md->byte_offset_of_slot(data, ReceiverTypeData::receiver_count_offset(i)) - mdo_offset_bias, mdo);
    __ b(*update_done);

    __ bind(next_test);
  }
}


void LIR_Assembler::setup_md_access(ciMethod* method, int bci,
                                    ciMethodData*& md, ciProfileData*& data, int& mdo_offset_bias) {
  md = method->method_data_or_null();
  assert(md != NULL, "Sanity");
  data = md->bci_to_data(bci);
  assert(data != NULL,       "need data for checkcast");
  assert(data->is_ReceiverTypeData(), "need ReceiverTypeData for type check");
  if (!Assembler::is_simm16(md->byte_offset_of_slot(data, DataLayout::header_offset()) + data->size_in_bytes())) {
    // The offset is large so bias the mdo by the base of the slot so
    // that the ld can use simm16s to reference the slots of the data.
    mdo_offset_bias = md->byte_offset_of_slot(data, DataLayout::header_offset());
  }
}


void LIR_Assembler::emit_typecheck_helper(LIR_OpTypeCheck *op, Label* success, Label* failure, Label* obj_is_null) {
  const Register obj = op->object()->as_register(); // Needs to live in this register at safepoint (patching stub).
  Register k_RInfo = op->tmp1()->as_register();
  Register klass_RInfo = op->tmp2()->as_register();
  Register Rtmp1 = op->tmp3()->as_register();
  Register dst = op->result_opr()->as_register();
  ciKlass* k = op->klass();
  bool should_profile = op->should_profile();
  // Attention: do_temp(opTypeCheck->_object) is not used, i.e. obj may be same as one of the temps.
  bool reg_conflict = false;
  if (obj == k_RInfo) {
    k_RInfo = dst;
    reg_conflict = true;
  } else if (obj == klass_RInfo) {
    klass_RInfo = dst;
    reg_conflict = true;
  } else if (obj == Rtmp1) {
    Rtmp1 = dst;
    reg_conflict = true;
  }
  assert_different_registers(obj, k_RInfo, klass_RInfo, Rtmp1);

  __ cmpi(CCR0, obj, 0);

  ciMethodData* md;
  ciProfileData* data;
  int mdo_offset_bias = 0;
  if (should_profile) {
    ciMethod* method = op->profiled_method();
    assert(method != NULL, "Should have method");
    setup_md_access(method, op->profiled_bci(), md, data, mdo_offset_bias);

    Register mdo      = k_RInfo;
    Register data_val = Rtmp1;
    Label not_null;
    __ bne(CCR0, not_null);
    metadata2reg(md->constant_encoding(), mdo);
    __ add_const_optimized(mdo, mdo, mdo_offset_bias, R0);
    __ lbz(data_val, md->byte_offset_of_slot(data, DataLayout::flags_offset()) - mdo_offset_bias, mdo);
    __ ori(data_val, data_val, BitData::null_seen_byte_constant());
    __ stb(data_val, md->byte_offset_of_slot(data, DataLayout::flags_offset()) - mdo_offset_bias, mdo);
    __ b(*obj_is_null);
    __ bind(not_null);
  } else {
    __ beq(CCR0, *obj_is_null);
  }

  // get object class
  __ load_klass(klass_RInfo, obj);

  if (k->is_loaded()) {
    metadata2reg(k->constant_encoding(), k_RInfo);
  } else {
    klass2reg_with_patching(k_RInfo, op->info_for_patch());
  }

  Label profile_cast_failure, failure_restore_obj, profile_cast_success;
  Label *failure_target = should_profile ? &profile_cast_failure : failure;
  Label *success_target = should_profile ? &profile_cast_success : success;

  if (op->fast_check()) {
    assert_different_registers(klass_RInfo, k_RInfo);
    __ cmp(CCR0, k_RInfo, klass_RInfo);
    if (should_profile) {
      __ bne(CCR0, *failure_target);
      // Fall through to success case.
    } else {
      __ beq(CCR0, *success);
      // Fall through to failure case.
    }
  } else {
    bool need_slow_path = true;
    if (k->is_loaded()) {
      if ((int) k->super_check_offset() != in_bytes(Klass::secondary_super_cache_offset())) {
        need_slow_path = false;
      }
      // Perform the fast part of the checking logic.
      __ check_klass_subtype_fast_path(klass_RInfo, k_RInfo, Rtmp1, R0, (need_slow_path ? success_target : NULL),
                                       failure_target, NULL, RegisterOrConstant(k->super_check_offset()));
    } else {
      // Perform the fast part of the checking logic.
      __ check_klass_subtype_fast_path(klass_RInfo, k_RInfo, Rtmp1, R0, success_target, failure_target);
    }
    if (!need_slow_path) {
      if (!should_profile) { __ b(*success); }
    } else {
      // Call out-of-line instance of __ check_klass_subtype_slow_path(...):
      address entry = Runtime1::entry_for(Runtime1::slow_subtype_check_id);
      // Stub needs fixed registers (tmp1-3).
      Register original_k_RInfo = op->tmp1()->as_register();
      Register original_klass_RInfo = op->tmp2()->as_register();
      Register original_Rtmp1 = op->tmp3()->as_register();
      bool keep_obj_alive = reg_conflict && (op->code() == lir_checkcast);
      bool keep_klass_RInfo_alive = (obj == original_klass_RInfo) && should_profile;
      if (keep_obj_alive && (obj != original_Rtmp1)) { __ mr(R0, obj); }
      __ mr_if_needed(original_k_RInfo, k_RInfo);
      __ mr_if_needed(original_klass_RInfo, klass_RInfo);
      if (keep_obj_alive) { __ mr(dst, (obj == original_Rtmp1) ? obj : R0); }
      //__ load_const_optimized(original_Rtmp1, entry, R0);
      __ calculate_address_from_global_toc(original_Rtmp1, entry, true, true, false);
      __ mtctr(original_Rtmp1);
      __ bctrl(); // sets CR0
      if (keep_obj_alive) {
        if (keep_klass_RInfo_alive) { __ mr(R0, obj); }
        __ mr(obj, dst);
      }
      if (should_profile) {
        __ bne(CCR0, *failure_target);
        if (keep_klass_RInfo_alive) { __ mr(klass_RInfo, keep_obj_alive ? R0 : obj); }
        // Fall through to success case.
      } else {
        __ beq(CCR0, *success);
        // Fall through to failure case.
      }
    }
  }

  if (should_profile) {
    Register mdo = k_RInfo, recv = klass_RInfo;
    assert_different_registers(mdo, recv, Rtmp1);
    __ bind(profile_cast_success);
    metadata2reg(md->constant_encoding(), mdo);
    __ add_const_optimized(mdo, mdo, mdo_offset_bias, R0);
    type_profile_helper(mdo, mdo_offset_bias, md, data, recv, Rtmp1, success);
    __ b(*success);

    // Cast failure case.
    __ bind(profile_cast_failure);
    metadata2reg(md->constant_encoding(), mdo);
    __ add_const_optimized(mdo, mdo, mdo_offset_bias, R0);
    __ l(Rtmp1, md->byte_offset_of_slot(data, CounterData::count_offset()) - mdo_offset_bias, mdo);
    __ addi(Rtmp1, Rtmp1, -DataLayout::counter_increment);
    __ st(Rtmp1, md->byte_offset_of_slot(data, CounterData::count_offset()) - mdo_offset_bias, mdo);
  }

  __ bind(*failure);
}


void LIR_Assembler::emit_opTypeCheck(LIR_OpTypeCheck* op) {
  LIR_Code code = op->code();
  if (code == lir_store_check) {
    Register value = op->object()->as_register();
    Register array = op->array()->as_register();
    Register k_RInfo = op->tmp1()->as_register();
    Register klass_RInfo = op->tmp2()->as_register();
    Register Rtmp1 = op->tmp3()->as_register();
    bool should_profile = op->should_profile();

    __ verify_oop(value);
    CodeStub* stub = op->stub();
    // Check if it needs to be profiled.
    ciMethodData* md;
    ciProfileData* data;
    int mdo_offset_bias = 0;
    if (should_profile) {
      ciMethod* method = op->profiled_method();
      assert(method != NULL, "Should have method");
      setup_md_access(method, op->profiled_bci(), md, data, mdo_offset_bias);
    }
    Label profile_cast_success, failure, done;
    Label *success_target = should_profile ? &profile_cast_success : &done;

    __ cmpi(CCR0, value, 0);
    if (should_profile) {
      Label not_null;
      __ bne(CCR0, not_null);
      guarantee(k_RInfo !=  array, "must be different");
      Register mdo      = k_RInfo;
      Register data_val = Rtmp1;
      metadata2reg(md->constant_encoding(), mdo);
      __ add_const_optimized(mdo, mdo, mdo_offset_bias, R0);
      __ lbz(data_val, md->byte_offset_of_slot(data, DataLayout::flags_offset()) - mdo_offset_bias, mdo);
      __ ori(data_val, data_val, BitData::null_seen_byte_constant());
      __ stb(data_val, md->byte_offset_of_slot(data, DataLayout::flags_offset()) - mdo_offset_bias, mdo);
      __ b(done);
      __ bind(not_null);
    } else {
      __ beq(CCR0, done);
    }
    if (!os::zero_page_read_protected() || !ImplicitNullChecks) {
      explicit_null_check(array, op->info_for_exception());
    } else {
      add_debug_info_for_null_check_here(op->info_for_exception());
    }
    __ load_klass(k_RInfo, array);
    __ load_klass(klass_RInfo, value);

    // Get instance klass.
    __ l(k_RInfo, in_bytes(ObjArrayKlass::element_klass_offset()), k_RInfo);
    // Perform the fast part of the checking logic.
    __ check_klass_subtype_fast_path(klass_RInfo, k_RInfo, Rtmp1, R0, success_target, &failure, NULL);

    // Call out-of-line instance of __ check_klass_subtype_slow_path(...):
    const address slow_path = Runtime1::entry_for(Runtime1::slow_subtype_check_id);
#ifdef PPC64
    __ add_const_optimized(R0, R29_TOC, MacroAssembler::offset_to_global_toc(slow_path));
#else
    __ load_const_optimized(R0, slow_path);
#endif
    __ mtctr(R0);
    __ bctrl(); // sets CR0
    if (!should_profile) {
      __ beq(CCR0, done);
      __ bind(failure);
    } else {
      __ bne(CCR0, failure);
      // Fall through to the success case.

      Register mdo  = klass_RInfo, recv = k_RInfo, tmp1 = Rtmp1;
      assert_different_registers(value, mdo, recv, tmp1);
      __ bind(profile_cast_success);
      metadata2reg(md->constant_encoding(), mdo);
      __ add_const_optimized(mdo, mdo, mdo_offset_bias, R0);
      __ load_klass(recv, value);
      type_profile_helper(mdo, mdo_offset_bias, md, data, recv, tmp1, &done);
      __ b(done);

      // Cast failure case.
      __ bind(failure);
      metadata2reg(md->constant_encoding(), mdo);
      __ add_const_optimized(mdo, mdo, mdo_offset_bias, R0);
      Address data_addr(mdo, md->byte_offset_of_slot(data, CounterData::count_offset()) - mdo_offset_bias);
      __ l(tmp1, md->byte_offset_of_slot(data, CounterData::count_offset()) - mdo_offset_bias, mdo);
      __ addi(tmp1, tmp1, -DataLayout::counter_increment);
      __ st(tmp1, md->byte_offset_of_slot(data, CounterData::count_offset()) - mdo_offset_bias, mdo);
    }
    __ b(*stub->entry());
    __ bind(done);

  } else if (code == lir_checkcast) {
    Label success, failure;
    emit_typecheck_helper(op, &success, /*fallthru*/&failure, &success);
    __ b(*op->stub()->entry());
    __ align(32, 12);
    __ bind(success);
    __ mr_if_needed(op->result_opr()->as_register(), op->object()->as_register());
  } else if (code == lir_instanceof) {
    Register dst = op->result_opr()->as_register();
    Label success, failure, done;
    emit_typecheck_helper(op, &success, /*fallthru*/&failure, &failure);
    __ li(dst, 0);
    __ b(done);
    __ align(32, 12);
    __ bind(success);
    __ li(dst, 1);
    __ bind(done);
  } else {
    ShouldNotReachHere();
  }
}


void LIR_Assembler::emit_compare_and_swap(LIR_OpCompareAndSwap* op) {
  Register addr = op->addr()->as_pointer_register();
  Register cmp_value = noreg, new_value = noreg;
  bool is_64bit = false;

  if (op->code() == lir_cas_long) {
    cmp_value = op->cmp_value()->as_register_lo();
    new_value = op->new_value()->as_register_lo();
    is_64bit = true;
  } else if (op->code() == lir_cas_int || op->code() == lir_cas_obj) {
    cmp_value = op->cmp_value()->as_register();
    new_value = op->new_value()->as_register();
#ifdef PPC64
    if (op->code() == lir_cas_obj) {
      if (UseCompressedOops) {
        Register t1 = op->tmp1()->as_register();
        Register t2 = op->tmp2()->as_register();
        cmp_value = __ encode_heap_oop(t1, cmp_value);
        new_value = __ encode_heap_oop(t2, new_value);
      } else {
        is_64bit = true;
      }
    }
#endif
  } else {
    Unimplemented();
  }

  if (is_64bit) {
#ifndef PPC64
    ShouldNotReachHere();
#endif
    __ cmpxchgd(BOOL_RESULT, /*current_value=*/R0, cmp_value, new_value, addr,
                MacroAssembler::MemBarFenceAfter,
                MacroAssembler::cmpxchgx_hint_atomic_update(),
                noreg, NULL, /*check without ldarx first*/true);
  } else {
    __ cmpxchgw(BOOL_RESULT, /*current_value=*/R0, cmp_value, new_value, addr,
                MacroAssembler::MemBarFenceAfter,
                MacroAssembler::cmpxchgx_hint_atomic_update(),
                noreg, /*check without ldarx first*/true);
  }
}


void LIR_Assembler::set_24bit_FPU() {
  Unimplemented();
}

void LIR_Assembler::reset_FPU() {
  Unimplemented();
}


void LIR_Assembler::breakpoint() {
  __ illtrap();
}


void LIR_Assembler::push(LIR_Opr opr) {
  Unimplemented();
}

void LIR_Assembler::pop(LIR_Opr opr) {
  Unimplemented();
}


void LIR_Assembler::monitor_address(int monitor_no, LIR_Opr dst_opr) {
  Address mon_addr = frame_map()->address_for_monitor_lock(monitor_no);
  Register dst = dst_opr->as_register();
  Register reg = mon_addr.base();
  int offset = mon_addr.disp();
  // Compute pointer to BasicLock.
  __ add_const_optimized(dst, reg, offset);
}


void LIR_Assembler::emit_lock(LIR_OpLock* op) {
  Register obj = op->obj_opr()->as_register();
  Register hdr = op->hdr_opr()->as_register();
  Register lock = op->lock_opr()->as_register();

  // Obj may not be an oop.
  if (op->code() == lir_lock) {
    MonitorEnterStub* stub = (MonitorEnterStub*)op->stub();
    if (UseFastLocking) {
      assert(BasicLock::displaced_header_offset_in_bytes() == 0, "lock_reg must point to the displaced header");
      // Add debug info for NullPointerException only if one is possible.
      if (op->info() != NULL) {
        if (!os::zero_page_read_protected() || !ImplicitNullChecks) {
          explicit_null_check(obj, op->info());
        } else {
          add_debug_info_for_null_check_here(op->info());
        }
      }
      __ lock_object(hdr, obj, lock, op->scratch_opr()->as_register(), *op->stub()->entry());
    } else {
      // always do slow locking
      // note: The slow locking code could be inlined here, however if we use
      //       slow locking, speed doesn't matter anyway and this solution is
      //       simpler and requires less duplicated code - additionally, the
      //       slow locking code is the same in either case which simplifies
      //       debugging.
      __ b(*op->stub()->entry());
    }
  } else {
    assert (op->code() == lir_unlock, "Invalid code, expected lir_unlock");
    if (UseFastLocking) {
      assert(BasicLock::displaced_header_offset_in_bytes() == 0, "lock_reg must point to the displaced header");
      __ unlock_object(hdr, obj, lock, *op->stub()->entry());
    } else {
      // always do slow unlocking
      // note: The slow unlocking code could be inlined here, however if we use
      //       slow unlocking, speed doesn't matter anyway and this solution is
      //       simpler and requires less duplicated code - additionally, the
      //       slow unlocking code is the same in either case which simplifies
      //       debugging.
      __ b(*op->stub()->entry());
    }
  }
  __ bind(*op->stub()->continuation());
}


void LIR_Assembler::emit_profile_call(LIR_OpProfileCall* op) {
  ciMethod* method = op->profiled_method();
  int bci          = op->profiled_bci();
  ciMethod* callee = op->profiled_callee();

  // Update counter for all call types.
  ciMethodData* md = method->method_data_or_null();
  assert(md != NULL, "Sanity");
  ciProfileData* data = md->bci_to_data(bci);
  assert(data->is_CounterData(), "need CounterData for calls");
  assert(op->mdo()->is_single_cpu(),  "mdo must be allocated");
  Register mdo = op->mdo()->as_register();
#ifdef _LP64
  assert(op->tmp1()->is_double_cpu(), "tmp1 must be allocated");
  Register tmp1 = op->tmp1()->as_register_lo();
#else
  assert(op->tmp1()->is_single_cpu(), "tmp1 must be allocated");
  Register tmp1 = op->tmp1()->as_register();
#endif
  metadata2reg(md->constant_encoding(), mdo);
  int mdo_offset_bias = 0;
  if (!Assembler::is_simm16(md->byte_offset_of_slot(data, CounterData::count_offset()) +
                            data->size_in_bytes())) {
    // The offset is large so bias the mdo by the base of the slot so
    // that the ld can use simm16s to reference the slots of the data.
    mdo_offset_bias = md->byte_offset_of_slot(data, CounterData::count_offset());
    __ add_const_optimized(mdo, mdo, mdo_offset_bias, R0);
  }

  Bytecodes::Code bc = method->java_code_at_bci(bci);
  const bool callee_is_static = callee->is_loaded() && callee->is_static();
  // Perform additional virtual call profiling for invokevirtual and
  // invokeinterface bytecodes.
  if ((bc == Bytecodes::_invokevirtual || bc == Bytecodes::_invokeinterface) &&
      !callee_is_static &&  // Required for optimized MH invokes.
      C1ProfileVirtualCalls) {
    assert(op->recv()->is_single_cpu(), "recv must be allocated");
    Register recv = op->recv()->as_register();
    assert_different_registers(mdo, tmp1, recv);
    assert(data->is_VirtualCallData(), "need VirtualCallData for virtual calls");
    ciKlass* known_klass = op->known_holder();
    if (C1OptimizeVirtualCallProfiling && known_klass != NULL) {
      // We know the type that will be seen at this call site; we can
      // statically update the MethodData* rather than needing to do
      // dynamic tests on the receiver type.

      // NOTE: we should probably put a lock around this search to
      // avoid collisions by concurrent compilations.
      ciVirtualCallData* vc_data = (ciVirtualCallData*) data;
      uint i;
      for (i = 0; i < VirtualCallData::row_limit(); i++) {
        ciKlass* receiver = vc_data->receiver(i);
        if (known_klass->equals(receiver)) {
          __ l(tmp1, md->byte_offset_of_slot(data, VirtualCallData::receiver_count_offset(i)) - mdo_offset_bias, mdo);
          __ addi(tmp1, tmp1, DataLayout::counter_increment);
          __ st(tmp1, md->byte_offset_of_slot(data, VirtualCallData::receiver_count_offset(i)) - mdo_offset_bias, mdo);
          return;
        }
      }

      // Receiver type not found in profile data; select an empty slot.

      // Note that this is less efficient than it should be because it
      // always does a write to the receiver part of the
      // VirtualCallData rather than just the first time.
      for (i = 0; i < VirtualCallData::row_limit(); i++) {
        ciKlass* receiver = vc_data->receiver(i);
        if (receiver == NULL) {
          metadata2reg(known_klass->constant_encoding(), tmp1);
          __ st(tmp1, md->byte_offset_of_slot(data, VirtualCallData::receiver_offset(i)) - mdo_offset_bias, mdo);

          __ l(tmp1, md->byte_offset_of_slot(data, VirtualCallData::receiver_count_offset(i)) - mdo_offset_bias, mdo);
          __ addi(tmp1, tmp1, DataLayout::counter_increment);
          __ st(tmp1, md->byte_offset_of_slot(data, VirtualCallData::receiver_count_offset(i)) - mdo_offset_bias, mdo);
          return;
        }
      }
    } else {
      __ load_klass(recv, recv);
      Label update_done;
      type_profile_helper(mdo, mdo_offset_bias, md, data, recv, tmp1, &update_done);
      // Receiver did not match any saved receiver and there is no empty row for it.
      // Increment total counter to indicate polymorphic case.
      __ l(tmp1, md->byte_offset_of_slot(data, CounterData::count_offset()) - mdo_offset_bias, mdo);
      __ addi(tmp1, tmp1, DataLayout::counter_increment);
      __ st(tmp1, md->byte_offset_of_slot(data, CounterData::count_offset()) - mdo_offset_bias, mdo);

      __ bind(update_done);
    }
  } else {
    // Static call
    __ l(tmp1, md->byte_offset_of_slot(data, CounterData::count_offset()) - mdo_offset_bias, mdo);
    __ addi(tmp1, tmp1, DataLayout::counter_increment);
    __ st(tmp1, md->byte_offset_of_slot(data, CounterData::count_offset()) - mdo_offset_bias, mdo);
  }
}


void LIR_Assembler::align_backward_branch_target() {
  __ align(32, 12); // Insert up to 3 nops to align with 32 byte boundary.
}


void LIR_Assembler::emit_delay(LIR_OpDelay* op) {
  Unimplemented();
}


void LIR_Assembler::negate(LIR_Opr left, LIR_Opr dest) {
  assert(left->is_register(), "can only handle registers");

  if (left->is_single_cpu()) {
#ifdef USE_SPE
    if(left->is_float_kind()) {
      __ efsneg(dest->as_register(), left->as_register());
    } else
#endif
      __ neg(dest->as_register(), left->as_register());
  } else if (left->is_single_fpu()) {
#ifndef USE_SPE
    __ fneg(dest->as_float_reg(), left->as_float_reg());
#else
    ShouldNotReachHere();
#endif
  } else if (left->is_double_fpu()) {
#ifndef USE_SPE
    __ fneg(dest->as_double_reg(), left->as_double_reg());
#else
    ShouldNotReachHere();
#endif
  } else {
    assert (left->is_double_cpu(), "Must be a long");
#ifdef PPC64
    __ neg(dest->as_register_lo(), left->as_register_lo());
#else
#ifdef USE_SPE
    if(left->is_float_kind()) {
      assert(left->as_register_lo() == left->as_register_hi(), "left in 1 gpr registers");
      assert(dest->as_register_lo() == dest->as_register_hi(), "dest in 1 gpr registers");
      __ efdneg(dest->as_register_lo(), left->as_register_lo());
    } else
#endif
    {
      Register temp = dest->as_register_lo();
      if (dest->as_register_lo() == left->as_register_hi())
        temp = R0;
      __ subfic(temp, left->as_register_lo(), 0);
      __ subfze(dest->as_register_hi(), left->as_register_hi());
      __ mr_if_needed(dest->as_register_lo(), temp);
    }
#endif
  }
}


void LIR_Assembler::fxch(int i) {
  Unimplemented();
}

void LIR_Assembler::fld(int i) {
  Unimplemented();
}

void LIR_Assembler::ffree(int i) {
  Unimplemented();
}


void LIR_Assembler::rt_call(LIR_Opr result, address dest,
                            const LIR_OprList* args, LIR_Opr tmp, CodeEmitInfo* info) {
  // Stubs: Called via rt_call, but dest is a stub address (no function descriptor).
  if (dest == Runtime1::entry_for(Runtime1::register_finalizer_id) ||
      dest == Runtime1::entry_for(Runtime1::new_multi_array_id   )) {
#ifdef PPC64
    __ add_const_optimized(R0, R29_TOC, MacroAssembler::offset_to_global_toc(dest));
#else
    __ load_const_optimized(R0, dest);
#endif
    __ mtctr(R0);
    __ bctrl();
    assert(info != NULL, "sanity");
    add_call_info_here(info);
    return;
  }

  __ call_c_with_frame_resize(dest, /*no resizing*/ 0);
  if (info != NULL) {
    add_call_info_here(info);
  }
}


void LIR_Assembler::volatile_move_op(LIR_Opr src, LIR_Opr dest, BasicType type, CodeEmitInfo* info) {
#if defined _LP64 || !defined USE_SPE
    ShouldNotReachHere();
#else
  if (type == T_LONG) {
    LIR_Address* mem_addr = dest->is_address() ? dest->as_address_ptr() : src->as_address_ptr();

    Register idx = noreg;
    int disp = mem_addr->disp();
    if (mem_addr->index() != LIR_OprFact::illegalOpr) {
      assert(disp == 0, "not both indexed and disp");
      idx = mem_addr->index()->as_register();
    }

    int null_check_offset = -1;

    Register base = mem_addr->base()->as_register();
    if (src->is_register() && dest->is_address()) {
      __ evmergelo(src->as_register_lo(), src->as_register_hi(), src->as_register_lo());
      null_check_offset = __ offset();
      if (idx == noreg) {
        __ evstdd(src->as_register_lo(), disp, base);
      } else {
        __ evstddx(src->as_register_lo(), idx, base);
      }
    } else if (src->is_address() && dest->is_register()) {
      null_check_offset = __ offset();
      if (idx == noreg) {
        __ evldd(dest->as_register_lo(), disp, base);
      } else {
        __ evlddx(dest->as_register_lo(), idx, base);
      }
      __ evmergehi(dest->as_register_hi(), dest->as_register_hi(), dest->as_register_lo());
    }
    if (info != NULL) {
      add_debug_info_for_null_check(null_check_offset, info);
    }

  } else {
    ShouldNotReachHere();
  }
#endif
}

void LIR_Assembler::membar() {
  __ fence();
}

void LIR_Assembler::membar_acquire() {
  __ acquire();
}

void LIR_Assembler::membar_release() {
  __ release();
}

void LIR_Assembler::membar_loadload() {
  __ membar(Assembler::LoadLoad);
}

void LIR_Assembler::membar_storestore() {
  __ membar(Assembler::StoreStore);
}

void LIR_Assembler::membar_loadstore() {
  __ membar(Assembler::LoadStore);
}

void LIR_Assembler::membar_storeload() {
  __ membar(Assembler::StoreLoad);
}


void LIR_Assembler::leal(LIR_Opr addr_opr, LIR_Opr dest) {
  LIR_Address* addr = addr_opr->as_address_ptr();
  assert(addr->scale() == LIR_Address::times_1, "no scaling on this platform");
  if (addr->index()->is_illegal()) {
    __ add_const_optimized(dest->as_pointer_register(), addr->base()->as_pointer_register(), addr->disp());
  } else {
    assert(addr->disp() == 0, "can't have both: index and disp");
    __ add(dest->as_pointer_register(), addr->index()->as_pointer_register(), addr->base()->as_pointer_register());
  }
}


void LIR_Assembler::get_thread(LIR_Opr result_reg) {
  ShouldNotReachHere();
}


#ifdef ASSERT
// Emit run-time assertion.
void LIR_Assembler::emit_assert(LIR_OpAssert* op) {
  Unimplemented();
}
#endif


void LIR_Assembler::peephole(LIR_List* lir) {
  // Optimize instruction pairs before emitting.
  LIR_OpList* inst = lir->instructions_list();
  for (int i = 1; i < inst->length(); i++) {
    LIR_Op* op = inst->at(i);

    // 2 register-register-moves
    if (op->code() == lir_move) {
      LIR_Opr in2  = ((LIR_Op1*)op)->in_opr(),
              res2 = ((LIR_Op1*)op)->result_opr();
      if (in2->is_register() && res2->is_register()) {
        LIR_Op* prev = inst->at(i - 1);
        if (prev && prev->code() == lir_move) {
          LIR_Opr in1  = ((LIR_Op1*)prev)->in_opr(),
                  res1 = ((LIR_Op1*)prev)->result_opr();
          if (in1->is_same_register(res2) && in2->is_same_register(res1)
#ifndef PPC64
              // 2 long registers could overlap on phys regs on PPC32. For example,
              // move [R3R4] [R4R5]
              // move [R4R5] [R3R4]
              // Eliminating second move is not possible
              && (!in2->is_double_cpu()
                 || (in2->as_register_lo() != res2->as_register_hi()
                     && in2->as_register_hi() != res2->as_register_lo()))

#endif
                ) {
            inst->remove_at(i);
          }
        }
      }
    }

  }
  return;
}


void LIR_Assembler::atomic_op(LIR_Code code, LIR_Opr src, LIR_Opr data, LIR_Opr dest, LIR_Opr tmp) {
  const Register Rptr = src->as_pointer_register(),
                 Rtmp = tmp->as_register();
  Register Rco = noreg;
  if (UseCompressedOops && data->is_oop()) {
    Rco = __ encode_heap_oop(Rtmp, data->as_register());
  }

  Label Lretry;
  __ bind(Lretry);

  if (data->type() == T_INT) {
    const Register Rold = dest->as_register(),
                   Rsrc = data->as_register();
    assert_different_registers(Rptr, Rtmp, Rold, Rsrc);
    __ lwarx(Rold, Rptr, MacroAssembler::cmpxchgx_hint_atomic_update());
    if (code == lir_xadd) {
      __ add(Rtmp, Rsrc, Rold);
      // Wii U Espresso Patch
      __ dcbst(Rptr);
      __ stwcx_(Rtmp, Rptr);
    } else {
      // Wii U Espresso Patch
      __ dcbst(Rptr);
      __ stwcx_(Rsrc, Rptr);
    }
  } else if (data->is_oop()) {
    assert(code == lir_xchg, "xadd for oops");
    const Register Rold = dest->as_register();
#ifdef PPC64
    if (UseCompressedOops) {
      assert_different_registers(Rptr, Rold, Rco);
      __ lwarx(Rold, Rptr, MacroAssembler::cmpxchgx_hint_atomic_update());
      __ stwcx_(Rco, Rptr);
    } else {
      const Register Robj = data->as_register();
      assert_different_registers(Rptr, Rold, Robj);
      __ ldarx(Rold, Rptr, MacroAssembler::cmpxchgx_hint_atomic_update());
      __ stdcx_(Robj, Rptr);
    }
#else
      const Register Robj = data->as_register();
      assert_different_registers(Rptr, Rold, Robj);
      __ lwarx(Rold, Rptr, MacroAssembler::cmpxchgx_hint_atomic_update());
      // Wii U Espresso Patch
      __ dcbst(Rptr);
      __ stwcx_(Robj, Rptr);
#endif
  } else if (data->type() == T_LONG) {
#ifdef PPC64
    const Register Rold = dest->as_register_lo(),
                   Rsrc = data->as_register_lo();
    assert_different_registers(Rptr, Rtmp, Rold, Rsrc);
    __ ldarx(Rold, Rptr, MacroAssembler::cmpxchgx_hint_atomic_update());
    if (code == lir_xadd) {
      __ add(Rtmp, Rsrc, Rold);
      __ stdcx_(Rtmp, Rptr);
    } else {
      __ stdcx_(Rsrc, Rptr);
    }
#else
    ShouldNotReachHere();
#endif
  } else {
    ShouldNotReachHere();
  }

  if (UseStaticBranchPredictionInCompareAndSwapPPC64) {
    __ bne_predict_not_taken(CCR0, Lretry);
  } else {
    __ bne(                  CCR0, Lretry);
  }

  if (UseCompressedOops && data->is_oop()) {
    __ decode_heap_oop(dest->as_register());
  }
}


void LIR_Assembler::emit_profile_type(LIR_OpProfileType* op) {
  Register obj = op->obj()->as_register();
  Register tmp = op->tmp()->as_pointer_register();
  LIR_Address* mdo_addr = op->mdp()->as_address_ptr();
  ciKlass* exact_klass = op->exact_klass();
  intptr_t current_klass = op->current_klass();
  bool not_null = op->not_null();
  bool no_conflict = op->no_conflict();

  Label Lupdate, Ldo_update, Ldone;

  bool do_null = !not_null;
  bool exact_klass_set = exact_klass != NULL && ciTypeEntries::valid_ciklass(current_klass) == exact_klass;
  bool do_update = !TypeEntries::is_type_unknown(current_klass) && !exact_klass_set;

  assert(do_null || do_update, "why are we here?");
  assert(!TypeEntries::was_null_seen(current_klass) || do_update, "why are we here?");

  __ verify_oop(obj);

  if (do_null) {
    if (!TypeEntries::was_null_seen(current_klass)) {
      __ cmpi(CCR0, obj, 0);
      __ bne(CCR0, Lupdate);
      __ l(R0, index_or_disp(mdo_addr), mdo_addr->base()->as_pointer_register());
      __ ori(R0, R0, TypeEntries::null_seen);
      if (do_update) {
        __ b(Ldo_update);
      } else {
        __ st(R0, index_or_disp(mdo_addr), mdo_addr->base()->as_pointer_register());
      }
    } else {
      if (do_update) {
        __ cmpi(CCR0, obj, 0);
        __ beq(CCR0, Ldone);
      }
    }
#ifdef ASSERT
  } else {
    __ cmpi(CCR0, obj, 0);
    __ bne(CCR0, Lupdate);
    __ stop("unexpect null obj", 0x9652);
#endif
  }

  __ bind(Lupdate);
  if (do_update) {
    Label Lnext;
    const Register klass = R29_TOC; // kill and reload
    bool klass_reg_used = false;
#ifdef ASSERT
    if (exact_klass != NULL) {
      Label ok;
      klass_reg_used = true;
      __ load_klass(klass, obj);
      metadata2reg(exact_klass->constant_encoding(), R0);
      __ cmp(CCR0, klass, R0);
      __ beq(CCR0, ok);
      __ stop("exact klass and actual klass differ", 0x8564);
      __ bind(ok);
    }
#endif

    if (!no_conflict) {
      if (exact_klass == NULL || TypeEntries::is_type_none(current_klass)) {
        klass_reg_used = true;
        if (exact_klass != NULL) {
          __ l(tmp, index_or_disp(mdo_addr), mdo_addr->base()->as_pointer_register());
          metadata2reg(exact_klass->constant_encoding(), klass);
        } else {
          __ load_klass(klass, obj);
          __ l(tmp, index_or_disp(mdo_addr), mdo_addr->base()->as_pointer_register()); // may kill obj
        }

        // Like InterpreterMacroAssembler::profile_obj_type
        __ clrri(R0, tmp, exact_log2(-TypeEntries::type_klass_mask));
        // Basically same as andi(R0, tmp, TypeEntries::type_klass_mask);
        __ cmp(CCR1, R0, klass);
        // Klass seen before, nothing to do (regardless of unknown bit).
        //beq(CCR1, do_nothing);

        __ andi_(R0, klass, TypeEntries::type_unknown);
        // Already unknown. Nothing to do anymore.
        //bne(CCR0, do_nothing);
        __ crorc(CCR0, Assembler::equal, CCR1, Assembler::equal); // cr0 eq = cr1 eq or cr0 ne
        __ beq(CCR0, Lnext);

        if (TypeEntries::is_type_none(current_klass)) {
          __ clrri_(R0, tmp, exact_log2(-TypeEntries::type_mask));
          __ orr(R0, klass, tmp); // Combine klass and null_seen bit (only used if (tmp & type_mask)==0).
          __ beq(CCR0, Ldo_update); // First time here. Set profile type.
        }

      } else {
        assert(ciTypeEntries::valid_ciklass(current_klass) != NULL &&
               ciTypeEntries::valid_ciklass(current_klass) != exact_klass, "conflict only");

        __ l(tmp, index_or_disp(mdo_addr), mdo_addr->base()->as_pointer_register());
        __ andi_(R0, tmp, TypeEntries::type_unknown);
        // Already unknown. Nothing to do anymore.
        __ bne(CCR0, Lnext);
      }

      // Different than before. Cannot keep accurate profile.
      __ ori(R0, tmp, TypeEntries::type_unknown);
    } else {
      // There's a single possible klass at this profile point
      assert(exact_klass != NULL, "should be");
      __ l(tmp, index_or_disp(mdo_addr), mdo_addr->base()->as_pointer_register());

      if (TypeEntries::is_type_none(current_klass)) {
        klass_reg_used = true;
        metadata2reg(exact_klass->constant_encoding(), klass);

        __ clrri(R0, tmp, exact_log2(-TypeEntries::type_klass_mask));
        // Basically same as andi(R0, tmp, TypeEntries::type_klass_mask);
        __ cmp(CCR1, R0, klass);
        // Klass seen before, nothing to do (regardless of unknown bit).
        __ beq(CCR1, Lnext);
#ifdef ASSERT
        {
          Label ok;
          __ clrri_(R0, tmp, exact_log2(-TypeEntries::type_mask));
          __ beq(CCR0, ok); // First time here.

          __ stop("unexpected profiling mismatch", 0x7865);
          __ bind(ok);
        }
#endif
        // First time here. Set profile type.
        __ orr(R0, klass, tmp); // Combine klass and null_seen bit (only used if (tmp & type_mask)==0).
      } else {
        assert(ciTypeEntries::valid_ciklass(current_klass) != NULL &&
               ciTypeEntries::valid_ciklass(current_klass) != exact_klass, "inconsistent");

        // Already unknown. Nothing to do anymore.
        __ andi_(R0, tmp, TypeEntries::type_unknown);
        __ bne(CCR0, Lnext);

        // Different than before. Cannot keep accurate profile.
        __ ori(R0, tmp, TypeEntries::type_unknown);
      }
    }

    __ bind(Ldo_update);
    __ st(R0, index_or_disp(mdo_addr), mdo_addr->base()->as_pointer_register());

    __ bind(Lnext);
    if (klass_reg_used) { __ load_const_optimized(R29_TOC, MacroAssembler::global_toc(), R0); } // reinit
  }
  __ bind(Ldone);
}


void LIR_Assembler::emit_updatecrc32(LIR_OpUpdateCRC32* op) {
  assert(op->crc()->is_single_cpu(), "crc must be register");
  assert(op->val()->is_single_cpu(), "byte value must be register");
  assert(op->result_opr()->is_single_cpu(), "result must be register");
  Register crc = op->crc()->as_register();
  Register val = op->val()->as_register();
  Register res = op->result_opr()->as_register();

  assert_different_registers(val, crc, res);

  __ load_const_optimized(res, StubRoutines::crc_table_addr(), R0);
  __ nand(crc, crc, crc); // ~crc
#ifdef PPC64
  __ update_byte_crc32(crc, val, res);
#else
  // undefined symbol: _ZN14MacroAssembler17update_byte_crc32EP12RegisterImplS1_S1_
  Unimplemented();
#endif
  __ nand(res, crc, crc); // ~crc
}

#undef __
