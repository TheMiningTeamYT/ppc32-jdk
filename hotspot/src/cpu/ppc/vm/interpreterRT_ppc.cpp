/*
 * Copyright (c) 1997, 2013, Oracle and/or its affiliates. All rights reserved.
 * Copyright 2012, 2013 SAP AG. All rights reserved.
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
#include "asm/assembler.inline.hpp"
#include "asm/macroAssembler.inline.hpp"
#include "interpreter/interpreter.hpp"
#include "interpreter/interpreterRuntime.hpp"
#include "memory/allocation.inline.hpp"
#include "memory/universe.inline.hpp"
#include "oops/method.hpp"
#include "oops/oop.inline.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/icache.hpp"
#include "runtime/interfaceSupport.hpp"
#include "runtime/signature.hpp"

#define __ _masm->

// Access macros for Java and C arguments.
// The first Java argument is at index -1.
#define locals_j_arg_at(index)    (Interpreter::local_offset_in_bytes(index)), R18_locals
#ifdef PPC64
// The first C argument is at index 0.
#define sp_c_arg_at(index)        ((index)*wordSize + _abi(carg_1)), R1_SP
#else // PPC64
// FIXME
// On PPC32 on-stack arguments should be located right after lr.
// But abi_minframe reserves the place for CR save area. We are calculating
// proper stack offset in such ugly way in assumption that no CR will be saved
// (by save_LR_CR for example) until stack arguments will stale.
#define sp_c_arg_at(index)        (((index)+1)*wordSize + _abi(lr)), R1_SP
#endif // PPC64

// Implementation of SignatureHandlerGenerator

#ifdef PPC64
void InterpreterRuntime::SignatureHandlerGenerator::pass_int() {
  Argument jni_arg(jni_offset());
  Register r = jni_arg.is_register() ? jni_arg.as_register() : R0;

  __ lwa(r, locals_j_arg_at(offset())); // sign extension of integer
  if (DEBUG_ONLY(true ||) !jni_arg.is_register()) {
    __ st(r, sp_c_arg_at(jni_arg.number()));
  }
}

void InterpreterRuntime::SignatureHandlerGenerator::pass_long() {
  Argument jni_arg(jni_offset());
  Register r = jni_arg.is_register() ? jni_arg.as_register() : R0;

  __ l(r, locals_j_arg_at(offset()+1)); // long resides in upper slot
  if (DEBUG_ONLY(true ||) !jni_arg.is_register()) {
    __ st(r, sp_c_arg_at(jni_arg.number()));
  }
}

void InterpreterRuntime::SignatureHandlerGenerator::pass_float() {
  FloatRegister fp_reg = (_num_used_fp_arg_regs < 13/*max_fp_register_arguments*/)
                         ? as_FloatRegister((_num_used_fp_arg_regs++) + F1_ARG1->encoding())
                         : F0;

  __ lfs(fp_reg, locals_j_arg_at(offset()));
  if (DEBUG_ONLY(true ||) jni_offset() > 8) {
    __ stfs(fp_reg, sp_c_arg_at(jni_offset()));
  }
}

void InterpreterRuntime::SignatureHandlerGenerator::pass_double() {
  FloatRegister fp_reg = (_num_used_fp_arg_regs < 13/*max_fp_register_arguments*/)
                         ? as_FloatRegister((_num_used_fp_arg_regs++) + F1_ARG1->encoding())
                         : F0;

  __ lfd(fp_reg, locals_j_arg_at(offset()+1));
  if (DEBUG_ONLY(true ||) jni_offset() > 8) {
    __ stfd(fp_reg, sp_c_arg_at(jni_offset()));
  }
}

void InterpreterRuntime::SignatureHandlerGenerator::pass_object() {
  Argument jni_arg(jni_offset());
  Register r = jni_arg.is_register() ? jni_arg.as_register() : R11_scratch1;

  // The handle for a receiver will never be null.
  bool do_NULL_check = offset() != 0 || is_static();

  Label do_null;
  if (do_NULL_check) {
    __ l(R0, locals_j_arg_at(offset()));
    __ cmpi(CCR0, R0, 0);
    __ li(r, 0);
    __ beq(CCR0, do_null);
  }
  __ addir(r, locals_j_arg_at(offset()));
  __ bind(do_null);
  if (DEBUG_ONLY(true ||) !jni_arg.is_register()) {
    __ st(r, sp_c_arg_at(jni_arg.number()));
  }
}

#else // PPC64

void InterpreterRuntime::SignatureHandlerGenerator::pass_int() {
  if (on_regs(T_INT)) {
    __ lwz(get_gp_regs(T_INT), locals_j_arg_at(offset()));
  } else {
    __ lwz(R0, locals_j_arg_at(offset()));
    __ stw(R0, sp_c_arg_at(get_stack_offset(T_INT)));
  }
}

void InterpreterRuntime::SignatureHandlerGenerator::pass_long() {
  align_reg_start(T_LONG);
  if (on_regs(T_LONG)) {
    Register r1 = get_gp_regs(T_LONG);
    Register r2 = r1->successor();
    __ l(r1, locals_j_arg_at(offset()+1)); // lower-addressed word of the long long into gr
    __ l(r2, locals_j_arg_at(offset())); // and the higher-addressed word into gr+1
  } else {
    const int stack_offset = get_stack_offset(T_LONG);
    // locals_j_arg_at maps higher indexes to lower addresses,
    // while sp_c_arg_at - higher indexes to higher addresses,
    // so long parts aren't reversed here
    __ l(R11_scratch1, locals_j_arg_at(offset()+1));
    __ l(R12_scratch2, locals_j_arg_at(offset()));
    __ st(R11_scratch1, sp_c_arg_at(stack_offset));
    __ st(R12_scratch2, sp_c_arg_at(stack_offset+1));
  }
}

#ifndef SPE_ABI

void InterpreterRuntime::SignatureHandlerGenerator::pass_float() {
  if (on_regs(T_FLOAT)) {
    __ lfs(get_fp_reg(), locals_j_arg_at(offset()));
  } else {
    __ lfs(F0, locals_j_arg_at(offset()));
    __ stfs(F0, sp_c_arg_at(get_stack_offset(T_FLOAT)));
  }
}

void InterpreterRuntime::SignatureHandlerGenerator::pass_double() {
  if (on_regs(T_DOUBLE)) {
    __ lfd(get_fp_reg(), locals_j_arg_at(offset()+1));
  } else {
    __ lfd(F0, locals_j_arg_at(offset()+1));
    __ stfd(F0, sp_c_arg_at(get_stack_offset(T_DOUBLE)));
  }
}

#endif // SPE_ABI

void InterpreterRuntime::SignatureHandlerGenerator::pass_object() {
  const bool pass_on_reg = on_regs(T_OBJECT);
  Register r = R11_scratch1;
  if (pass_on_reg) {
    r = get_gp_regs(T_OBJECT);
  }

  // The handle for a receiver will never be null.
  bool do_NULL_check = offset() != 0 || is_static();

  Label do_null;
  if (do_NULL_check) {
    __ l(R0, locals_j_arg_at(offset()));
    __ cmpwi(CCR0, R0, 0);
    __ li(r, 0);
    __ beq(CCR0, do_null);
  }
  __ addir(r, locals_j_arg_at(offset()));
  __ bind(do_null);

  if (!pass_on_reg) {
    __ stw(r, sp_c_arg_at(get_stack_offset(T_OBJECT)));
  }
}
#endif // PPC64

void InterpreterRuntime::SignatureHandlerGenerator::generate(uint64_t fingerprint) {
#if defined(PPC64) && !defined(ABI_ELFv2)
  // Emit fd for current codebuffer. Needs patching!
  __ emit_fd();
#endif

  // Generate code to handle arguments.
  iterate(fingerprint);

  // Return the result handler.
  __ load_const(R3_RET, AbstractInterpreter::result_handler(method()->result_type()));
  __ blr();

  __ flush();
}

#undef __

// Implementation of SignatureHandlerLibrary

void SignatureHandlerLibrary::pd_set_handler(address handler) {
#if defined(PPC64) && !defined(ABI_ELFv2)
  // patch fd here.
  FunctionDescriptor* fd = (FunctionDescriptor*) handler;

  fd->set_entry(handler + (int)sizeof(FunctionDescriptor));
  assert(fd->toc() == (address)0xcafe, "need to adjust TOC here");
#endif
}


// Access function to get the signature.
IRT_ENTRY(address, InterpreterRuntime::get_signature(JavaThread* thread, Method* method))
  methodHandle m(thread, method);
  assert(m->is_native(), "sanity check");
  Symbol *s = m->signature();
  return (address) s->base();
IRT_END

IRT_ENTRY(address, InterpreterRuntime::get_result_handler(JavaThread* thread, Method* method))
  methodHandle m(thread, method);
  assert(m->is_native(), "sanity check");
  return AbstractInterpreter::result_handler(m->result_type());
IRT_END
