/*
 * Copyright (c) 1997, 2017, Oracle and/or its affiliates. All rights reserved.
 * Copyright 2012, 2017 SAP AG. All rights reserved.
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
#include "asm/assembler.hpp"
#include "asm/macroAssembler.inline.hpp"
#include "code/vtableStubs.hpp"
#include "interp_masm_ppc_64.hpp"
#include "memory/resourceArea.hpp"
#include "oops/compiledICHolder.hpp"
#include "oops/instanceKlass.hpp"
#include "oops/klassVtable.hpp"
#include "runtime/sharedRuntime.hpp"
#include "vmreg_ppc.inline.hpp"
#ifdef COMPILER2
#include "opto/runtime.hpp"
#endif

#define __ masm->

#ifdef PRODUCT
#define BLOCK_COMMENT(str) // nothing
#else
#define BLOCK_COMMENT(str) __ block_comment(str)
#endif
#define BIND(label) bind(label); BLOCK_COMMENT(#label ":")

#ifndef PRODUCT
extern "C" void bad_compiled_vtable_index(JavaThread* thread, oopDesc* receiver, int index);
#endif

// Used by compiler only; may use only caller saved, non-argument
// registers.
VtableStub* VtableStubs::create_vtable_stub(int vtable_index) {
  // PPC port: use fixed size.
  const int code_length = VtableStub::pd_code_size_limit(true);
  VtableStub* s = new (code_length) VtableStub(true, vtable_index);

  // Can be NULL if there is no free space in the code cache.
  if (s == NULL) {
    return NULL;
  }

  ResourceMark rm;
  CodeBuffer cb(s->entry_point(), code_length);
  MacroAssembler* masm = new MacroAssembler(&cb);

#ifndef PRODUCT
  if (CountCompiledCalls) {
    int offs = __ load_const_optimized(R11_scratch1, SharedRuntime::nof_megamorphic_calls_addr(), R12_scratch2, true);
    __ lwz(R12_scratch2, offs, R11_scratch1);
    __ addi(R12_scratch2, R12_scratch2, 1);
    __ stw(R12_scratch2, offs, R11_scratch1);
  }
#endif

  assert(VtableStub::receiver_location() == R3_ARG1->as_VMReg(), "receiver expected in R3_ARG1");

  // Get receiver klass.
  const Register rcvr_klass = R11_scratch1;

  // We might implicit NULL fault here.
  address npe_addr = __ pc(); // npe = null pointer exception
  __ null_check(R3, oopDesc::klass_offset_in_bytes(), /*implicit only*/NULL);
  __ load_klass(rcvr_klass, R3);

 // Set method (in case of interpreted method), and destination address.
  int entry_offset = InstanceKlass::vtable_start_offset() + vtable_index*vtableEntry::size();

#ifndef PRODUCT
  if (DebugVtables) {
    Label L;
    // Check offset vs vtable length.
    const Register vtable_len = R12_scratch2;
    __ lwz(vtable_len, InstanceKlass::vtable_length_offset()*wordSize, rcvr_klass);
    __ cmpwi(CCR0, vtable_len, vtable_index*vtableEntry::size());
    __ bge(CCR0, L);
    __ li(R12_scratch2, vtable_index);
    __ call_VM(noreg, CAST_FROM_FN_PTR(address, bad_compiled_vtable_index), R3_ARG1, R12_scratch2, false);
    __ bind(L);
  }
#endif

  int v_off = entry_offset*wordSize + vtableEntry::method_offset_in_bytes();

  __ l(R19_method, v_off, rcvr_klass);

#ifndef PRODUCT
  if (DebugVtables) {
    Label L;
    __ cmpi(CCR0, R19_method, 0);
    __ bne(CCR0, L);
    __ stop("Vtable entry is ZERO", 102);
    __ bind(L);
  }
#endif

  // If the vtable entry is null, the method is abstract.
  address ame_addr = __ pc(); // ame = abstract method error
  __ null_check(R19_method, in_bytes(Method::from_compiled_offset()), /*implicit only*/NULL);
  __ l(R12_scratch2, in_bytes(Method::from_compiled_offset()), R19_method);
  __ mtctr(R12_scratch2);
  __ bctr();

  masm->flush();

  guarantee(__ pc() <= s->code_end(), "overflowed buffer");

  s->set_exception_points(npe_addr, ame_addr);

  return s;
}

VtableStub* VtableStubs::create_itable_stub(int itable_index) {
  // PPC port: use fixed size.
  const int code_length = VtableStub::pd_code_size_limit(false);
  VtableStub* s = new (code_length) VtableStub(false, itable_index);

  // Can be NULL if there is no free space in the code cache.
  if (s == NULL) {
    return NULL;
  }

  ResourceMark rm;
  CodeBuffer cb(s->entry_point(), code_length);
  MacroAssembler* masm = new MacroAssembler(&cb);
  address start_pc;

#ifndef PRODUCT
  if (CountCompiledCalls) {
    int offs = __ load_const_optimized(R11_scratch1, SharedRuntime::nof_megamorphic_calls_addr(), R12_scratch2, true);
    __ lwz(R12_scratch2, offs, R11_scratch1);
    __ addi(R12_scratch2, R12_scratch2, 1);
    __ stw(R12_scratch2, offs, R11_scratch1);
  }
#endif

  assert(VtableStub::receiver_location() == R3_ARG1->as_VMReg(), "receiver expected in R3_ARG1");

  // Entry arguments:
  //  R19_method: Interface
  //  R3_ARG1:    Receiver

  Label L_no_such_interface;
  const Register rcvr_klass = R11_scratch1,
                 interface  = R12_scratch2,
                 tmp1       = R21_tmp1,
                 tmp2       = R22_tmp2;

  address npe_addr = __ pc(); // npe = null pointer exception
  __ null_check(R3_ARG1, oopDesc::klass_offset_in_bytes(), /*implicit only*/NULL);
  __ load_klass(rcvr_klass, R3_ARG1);

  // Receiver subtype check against REFC.
  __ l(interface, CompiledICHolder::holder_klass_offset(), R19_method);
  __ lookup_interface_method(rcvr_klass, interface, noreg,
                             R0, tmp1, tmp2,
                             L_no_such_interface, /*return_method=*/ false);

  // Get Method* and entrypoint for compiler
  __ l(interface, CompiledICHolder::holder_metadata_offset(), R19_method);
  __ lookup_interface_method(rcvr_klass, interface, itable_index,
                             R19_method, tmp1, tmp2,
                             L_no_such_interface, /*return_method=*/ true);

#ifndef PRODUCT
  if (DebugVtables) {
    Label ok;
    __ cmp(CCR0, R19_method, 0);
    __ bne(CCR0, ok);
    __ stop("method is null", 103);
    __ bind(ok);
  }
#endif

  // If the vtable entry is null, the method is abstract.
  address ame_addr = __ pc(); // ame = abstract method error

  // Must do an explicit check if implicit checks are disabled.
  assert(!MacroAssembler::needs_explicit_null_check(in_bytes(Method::from_compiled_offset())), "sanity");
  if (!ImplicitNullChecks || !os::zero_page_read_protected()) {
    if (TrapBasedNullChecks) {
      __ trap_null_check(R19_method);
    } else {
      __ cmpi(CCR0, R19_method, 0);
      __ beq(CCR0, L_no_such_interface);
    }
  }
  __ l(R12_scratch2, in_bytes(Method::from_compiled_offset()), R19_method);
  __ mtctr(R12_scratch2);
  __ bctr();

  // Handle IncompatibleClassChangeError in itable stubs.
  // More detailed error message.
  // We force resolving of the call site by jumping to the "handle
  // wrong method" stub, and so let the interpreter runtime do all the
  // dirty work.
  __ bind(L_no_such_interface);
  __ load_const_optimized(R11_scratch1, SharedRuntime::get_handle_wrong_method_stub(), R12_scratch2);
  __ mtctr(R11_scratch1);
  __ bctr();

  masm->flush();

  guarantee(__ pc() <= s->code_end(), "overflowed buffer");

  s->set_exception_points(npe_addr, ame_addr);
  return s;
}

int VtableStub::pd_code_size_limit(bool is_vtable_stub) {
  if (TraceJumps || DebugVtables || CountCompiledCalls || VerifyOops) {
    return 1000;
  }
  int size = is_vtable_stub ? 20 + 8 : 164 + 20; // Plain + safety
  if (UseCompressedClassPointers) {
    size += MacroAssembler::instr_size_for_decode_klass_not_null();
  }
  if (!ImplicitNullChecks || !os::zero_page_read_protected()) {
    size += is_vtable_stub ? 8 : 12;
  }
  return size;
}

int VtableStub::pd_code_alignment() {
  const unsigned int icache_line_size = 32;
  return icache_line_size;
}
