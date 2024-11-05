/*
 * Copyright (c) 2002, 2013, Oracle and/or its affiliates. All rights reserved.
 * Copyright 2012, 2014 SAP AG. All rights reserved.
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

#ifndef CPU_PPC_VM_MACROASSEMBLER_PPC_INLINE_HPP
#define CPU_PPC_VM_MACROASSEMBLER_PPC_INLINE_HPP

#include "asm/assembler.inline.hpp"
#include "asm/macroAssembler.hpp"
#include "asm/codeBuffer.hpp"
#include "code/codeCache.hpp"

inline bool MacroAssembler::is_ld_largeoffset(address a) {
  const int inst1 = *(int *)a;
  const int inst2 = *(int *)(a+4);
  return (is_ld(inst1)) ||
         (is_addis(inst1) && is_ld(inst2) && inv_ra_field(inst2) == inv_rt_field(inst1));
}

inline int MacroAssembler::get_ld_largeoffset_offset(address a) {
  assert(MacroAssembler::is_ld_largeoffset(a), "must be l with large offset");

  const int inst1 = *(int *)a;
  if (is_ld(inst1)) {
    return inv_d1_field(inst1);
  } else {
    const int inst2 = *(int *)(a+4);
    return (inv_d1_field(inst1) << 16) + inv_d1_field(inst2);
  }
}

inline void MacroAssembler::round_to(Register r, int modulus) {
  assert(is_power_of_2_long((jlong)modulus), "must be power of 2");
  addi(r, r, modulus-1);
  clrri(r, r, log2_long((jlong)modulus));
}

// Move register if destination register and target register are different.
inline void MacroAssembler::mr_if_needed(Register rd, Register rs) {
  if (rs != rd) mr(rd, rs);
}
#ifndef USE_SPE
inline void MacroAssembler::fmr_if_needed(FloatRegister rd, FloatRegister rs) {
  if (rs != rd) fmr(rd, rs);
}
#endif
inline void MacroAssembler::endgroup_if_needed(bool needed) {
  if (needed) {
    endgroup();
  }
}

inline void MacroAssembler::membar(int bits) {
  // TODO: use elemental_membar(bits) for Power 8 and disable optimization of acquire-release
  // (Matcher::post_membar_release where we use PPC64_ONLY(xop == Op_MemBarRelease ||))
  if ((bits & StoreLoad) || (!VM_Version::has_lwsync())) sync(); else lwsync();
}
inline void MacroAssembler::release() { membar(LoadStore | StoreStore); }
inline void MacroAssembler::acquire() { membar(LoadLoad | LoadStore); }
inline void MacroAssembler::fence()   { membar(LoadLoad | LoadStore | StoreLoad | StoreStore); }

// Address of the global TOC.
inline address MacroAssembler::global_toc() {
  return CodeCache::low_bound();
}

// Offset of given address to the global TOC.
inline int MacroAssembler::offset_to_global_toc(const address addr) {
  intptr_t offset = (intptr_t)addr - (intptr_t)MacroAssembler::global_toc();
  assert(Assembler::is_simm(offset, 31) && offset >= 0, "must be in range");
  return (int)offset;
}

// Address of current method's TOC.
inline address MacroAssembler::method_toc() {
  return code()->consts()->start();
}

// Offset of given address to current method's TOC.
inline int MacroAssembler::offset_to_method_toc(address addr) {
  intptr_t offset = (intptr_t)addr - (intptr_t)method_toc();
  assert(is_simm(offset, 31) && offset >= 0, "must be in range");
  return (int)offset;
}

inline bool MacroAssembler::is_calculate_address_from_global_toc_at(address a, address bound) {
  const address inst2_addr = a;
  const int inst2 = *(int *) a;

  // The relocation points to the second instruction, the addi.
  if (!is_addi(inst2)) return false;

  // The addi reads and writes the same register dst.
  const int dst = inv_rt_field(inst2);
  if (inv_ra_field(inst2) != dst) return false;

  // Now, find the preceding addis which writes to dst.
  int inst1 = 0;
  address inst1_addr = inst2_addr - BytesPerInstWord;
  while (inst1_addr >= bound) {
    inst1 = *(int *) inst1_addr;
    if (is_addis(inst1) && inv_rt_field(inst1) == dst) {
      // stop, found the addis which writes dst
      break;
    }
    inst1_addr -= BytesPerInstWord;
  }

  if (!(inst1 == 0 || inv_ra_field(inst1) == 29 /* R29 */)) return false;
  return is_addis(inst1);
}

#ifdef _LP64
// Detect narrow oop constants.
inline bool MacroAssembler::is_set_narrow_oop(address a, address bound) {
  const address inst2_addr = a;
  const int inst2 = *(int *)a;
  // The relocation points to the second instruction, the ori.
  if (!is_ori(inst2)) return false;

  // The ori reads and writes the same register dst.
  const int dst = inv_rta_field(inst2);
  if (inv_rs_field(inst2) != dst) return false;

  // Now, find the preceding addis which writes to dst.
  int inst1 = 0;
  address inst1_addr = inst2_addr - BytesPerInstWord;
  while (inst1_addr >= bound) {
    inst1 = *(int *) inst1_addr;
    if (is_lis(inst1) && inv_rs_field(inst1) == dst) return true;
    inst1_addr -= BytesPerInstWord;
  }
  return false;
}
#endif


inline bool MacroAssembler::is_load_const_at(address a) {
  const int* p_inst = (int *) a;
#ifdef PPC64
  bool b = is_lis(*p_inst++);
  if (is_ori(*p_inst)) {
    p_inst++;
    b = b && is_rldicr(*p_inst++); // TODO: could be made more precise: `sli'!
    b = b && is_oris(*p_inst++);
    b = b && is_ori(*p_inst);
  } else if (is_lis(*p_inst)) {
    p_inst++;
    b = b && is_ori(*p_inst++);
    b = b && is_ori(*p_inst);
    // TODO: could enhance reliability by adding is_insrdi
  } else return false;
  return b;
#else
  return is_lis(*p_inst) && is_ori(*(p_inst + 1));
#endif
}

inline void MacroAssembler::set_oop_constant(jobject obj, Register d) {
  set_oop(constant_oop_address(obj), d);
}

inline void MacroAssembler::set_oop(AddressLiteral obj_addr, Register d) {
  assert(obj_addr.rspec().type() == relocInfo::oop_type, "must be an oop reloc");
  load_const(d, obj_addr);
}

inline void MacroAssembler::pd_patch_instruction(address branch, address target) {
  jint& stub_inst = *(jint*) branch;
  stub_inst = patched_branch(target - branch, stub_inst, 0);
}

// Relocation of conditional far branches.
inline bool MacroAssembler::is_bc_far_variant1_at(address instruction_addr) {
  // Variant 1, the 1st instruction contains the destination address:
  //
  //    bcxx  DEST
  //    nop
  //
  const int instruction_1 = *(int*)(instruction_addr);
  const int instruction_2 = *(int*)(instruction_addr + 4);
  return is_bcxx(instruction_1) &&
         (inv_bd_field(instruction_1, (intptr_t)instruction_addr) != (intptr_t)(instruction_addr + 2*4)) &&
         is_nop(instruction_2);
}

// Relocation of conditional far branches.
inline bool MacroAssembler::is_bc_far_variant2_at(address instruction_addr) {
  // Variant 2, the 2nd instruction contains the destination address:
  //
  //    b!cxx SKIP
  //    bxx   DEST
  //  SKIP:
  //
  const int instruction_1 = *(int*)(instruction_addr);
  const int instruction_2 = *(int*)(instruction_addr + 4);
  return is_bcxx(instruction_1) &&
         (inv_bd_field(instruction_1, (intptr_t)instruction_addr) == (intptr_t)(instruction_addr + 2*4)) &&
         is_bxx(instruction_2);
}

// Relocation for conditional branches
inline bool MacroAssembler::is_bc_far_variant3_at(address instruction_addr) {
  // Variant 3, far cond branch to the next instruction, already patched to nops:
  //
  //    nop
  //    endgroup
  //  SKIP/DEST:
  //
  const int instruction_1 = *(int*)(instruction_addr);
  const int instruction_2 = *(int*)(instruction_addr + 4);
  return is_nop(instruction_1) &&
         is_endgroup(instruction_2);
}


// Convenience bc_far versions
inline void MacroAssembler::blt_far(ConditionRegister crx, Label& L, int optimize) { MacroAssembler::bc_far(bcondCRbiIs1, bi0(crx, less), L, optimize); }
inline void MacroAssembler::bgt_far(ConditionRegister crx, Label& L, int optimize) { MacroAssembler::bc_far(bcondCRbiIs1, bi0(crx, greater), L, optimize); }
inline void MacroAssembler::beq_far(ConditionRegister crx, Label& L, int optimize) { MacroAssembler::bc_far(bcondCRbiIs1, bi0(crx, equal), L, optimize); }
inline void MacroAssembler::bso_far(ConditionRegister crx, Label& L, int optimize) { MacroAssembler::bc_far(bcondCRbiIs1, bi0(crx, summary_overflow), L, optimize); }
inline void MacroAssembler::bge_far(ConditionRegister crx, Label& L, int optimize) { MacroAssembler::bc_far(bcondCRbiIs0, bi0(crx, less), L, optimize); }
inline void MacroAssembler::ble_far(ConditionRegister crx, Label& L, int optimize) { MacroAssembler::bc_far(bcondCRbiIs0, bi0(crx, greater), L, optimize); }
inline void MacroAssembler::bne_far(ConditionRegister crx, Label& L, int optimize) { MacroAssembler::bc_far(bcondCRbiIs0, bi0(crx, equal), L, optimize); }
inline void MacroAssembler::bns_far(ConditionRegister crx, Label& L, int optimize) { MacroAssembler::bc_far(bcondCRbiIs0, bi0(crx, summary_overflow), L, optimize); }

inline address MacroAssembler::call_stub(Register function_entry) {
  mtctr(function_entry);
  bctrl();
  return pc();
}

inline void MacroAssembler::call_stub_and_return_to(Register function_entry, Register return_pc) {
  assert_different_registers(function_entry, return_pc);
  mtlr(return_pc);
  mtctr(function_entry);
  bctr();
}

// Get the pc where the last emitted call will return to.
inline address MacroAssembler::last_calls_return_pc() {
  return _last_calls_return_pc;
}

// Read from the polling page, its address is already in a register.
inline void MacroAssembler::load_from_polling_page(Register polling_page_address, int offset) {
  l(R0, offset, polling_page_address);
}

// Trap-instruction-based checks.

inline void MacroAssembler::trap_null_check(Register a, trap_to_bits cmp) {
  assert(TrapBasedNullChecks, "sanity");
  ti(cmp, a/*reg a*/, 0);
}
inline void MacroAssembler::trap_zombie_not_entrant() {
  ti(traptoUnconditional, 0/*reg 0*/, 1);
}
inline void MacroAssembler::trap_should_not_reach_here() {
  tdi_unchecked(traptoUnconditional, 0/*reg 0*/, 2);
}

inline void MacroAssembler::trap_ic_miss_check(Register a, Register b) {
  t(traptoGreaterThanUnsigned | traptoLessThanUnsigned, a, b);
}

// Do an explicit null check if access to a+offset will not raise a SIGSEGV.
// Either issue a trap instruction that raises SIGTRAP, or do a compare that
// branches to exception_entry.
// No support for compressed oops (base page of heap). Does not distinguish
// loads and stores.
inline void MacroAssembler::null_check_throw(Register a, int offset, Register temp_reg,
                                             address exception_entry) {
  if (!ImplicitNullChecks || needs_explicit_null_check(offset) || !os::zero_page_read_protected()) {
    if (TrapBasedNullChecks) {
      assert(UseSIGTRAP, "sanity");
      trap_null_check(a);
    } else {
      Label ok;
      cmpi(CCR0, a, 0);
      bne(CCR0, ok);
      load_const_optimized(temp_reg, exception_entry);
      mtctr(temp_reg);
      bctr();
      bind(ok);
    }
  }
}

inline void MacroAssembler::null_check(Register a, int offset, Label *Lis_null) {
  if (!ImplicitNullChecks || needs_explicit_null_check(offset) || !os::zero_page_read_protected()) {
    if (TrapBasedNullChecks) {
      assert(UseSIGTRAP, "sanity");
      trap_null_check(a);
    } else if (Lis_null){
      Label ok;
      cmpi(CCR0, a, 0);
      beq(CCR0, *Lis_null);
    }
  }
}

inline void MacroAssembler::load_heap_oop_not_null(Register d, RegisterOrConstant offs, Register s1) {
  if (UseCompressedOops) {
    lwz(d, offs, s1);
    // Attention: no null check here!
    decode_heap_oop_not_null(d);
  } else {
    l(d, offs, s1);
  }
}

inline void MacroAssembler::store_heap_oop_not_null(Register d, RegisterOrConstant offs, Register s1, Register tmp) {
  if (UseCompressedOops) {
    Register compressedOop = encode_heap_oop_not_null((tmp != noreg) ? tmp : d, d);
    stw(compressedOop, offs, s1);
  } else {
    st(d, offs, s1);
  }
}

inline void MacroAssembler::load_heap_oop(Register d, RegisterOrConstant offs, Register s1, Label *is_null) {
  if (UseCompressedOops) {
    lwz(d, offs, s1);
    if (is_null != NULL) {
      cmpwi(CCR0, d, 0);
      beq(CCR0, *is_null);
      decode_heap_oop_not_null(d);
    } else {
      decode_heap_oop(d);
    }
  } else {
    l(d, offs, s1);
    if (is_null != NULL) {
      cmpi(CCR0, d, 0);
      beq(CCR0, *is_null);
    }
  }
}

inline Register MacroAssembler::encode_heap_oop_not_null(Register d, Register src) {
#ifdef PPC64
  Register current = (src!=noreg) ? src : d; // Compressed oop is in d if no src provided.
  if (Universe::narrow_oop_base() != NULL) {
    sub(d, current, R30);
    current = d;
  }
  if (Universe::narrow_oop_shift() != 0) {
    sri(d, current, LogMinObjAlignmentInBytes);
    current = d;
  }
  return current; // Encoded oop is in this register.
#else
  return src != noreg ? src : d;
#endif
}

inline Register MacroAssembler::encode_heap_oop(Register d, Register src) {
  if (Universe::narrow_oop_base() != NULL) {
    if (VM_Version::has_isel()) {
      cmpi(CCR0, src, 0);
      Register co = encode_heap_oop_not_null(d, src);
      assert(co == d, "sanity");
      isel_0(d, CCR0, Assembler::equal);
    } else {
      Label isNull;
      or_(d, src, src); // move and compare 0
      beq(CCR0, isNull);
      encode_heap_oop_not_null(d, src);
      bind(isNull);
    }
    return d;
  } else {
    return encode_heap_oop_not_null(d, src);
  }
}

inline void MacroAssembler::decode_heap_oop_not_null(Register d) {
#ifdef PPC64
  if (Universe::narrow_oop_shift() != 0) {
    assert (LogMinObjAlignmentInBytes == Universe::narrow_oop_shift(), "decode alg wrong");
    sli(d, d, LogMinObjAlignmentInBytes);
  }
  if (Universe::narrow_oop_base() != NULL) {
    add(d, d, R30);
  }
#endif
}

inline void MacroAssembler::decode_heap_oop(Register d) {
#ifdef PPC64
  Label isNull;
  if (Universe::narrow_oop_base() != NULL) {
    cmpwi(CCR0, d, 0);
    beq(CCR0, isNull);
  }
  if (Universe::narrow_oop_shift() != 0) {
    assert (LogMinObjAlignmentInBytes == Universe::narrow_oop_shift(), "decode alg wrong");
    sli(d, d, LogMinObjAlignmentInBytes);
  }
  if (Universe::narrow_oop_base() != NULL) {
    add(d, d, R30);
  }
  bind(isNull);
#endif
}

// SIGTRAP-based range checks for arrays.
inline void MacroAssembler::trap_range_check_l(Register a, Register b) {
  tw (traptoLessThanUnsigned,                  a/*reg a*/, b/*reg b*/);
}
inline void MacroAssembler::trap_range_check_l(Register a, int si16) {
  twi(traptoLessThanUnsigned,                  a/*reg a*/, si16);
}
inline void MacroAssembler::trap_range_check_le(Register a, int si16) {
  twi(traptoEqual | traptoLessThanUnsigned,    a/*reg a*/, si16);
}
inline void MacroAssembler::trap_range_check_g(Register a, int si16) {
  twi(traptoGreaterThanUnsigned,               a/*reg a*/, si16);
}
inline void MacroAssembler::trap_range_check_ge(Register a, Register b) {
  tw (traptoEqual | traptoGreaterThanUnsigned, a/*reg a*/, b/*reg b*/);
}
inline void MacroAssembler::trap_range_check_ge(Register a, int si16) {
  twi(traptoEqual | traptoGreaterThanUnsigned, a/*reg a*/, si16);
}

#if !defined(PPC64) || defined(ABI_ELFv2)
inline address MacroAssembler::function_entry() { return pc(); }
#else
inline address MacroAssembler::function_entry() { return emit_fd(); }
#endif

// 64/32 bit invariant metainstructions
inline void MacroAssembler::mull(Register d, Register a, Register b) {
  PPC64_ONLY(mulld(d, a, b));
  NOT_PPC64(unimplemented("128bit mul"));
}

inline void MacroAssembler::mull_(Register d, Register a, Register b) {
  PPC64_ONLY(mulld_(d, a, b));
  NOT_PPC64(unimplemented("128bit mul"));
}

inline void MacroAssembler::mulh(Register d, Register a, Register b) {
  PPC64_ONLY(mulhd(d, a, b));
  NOT_PPC64(unimplemented("128bit mul")); ;
}

inline void MacroAssembler::mulh_(Register d, Register a, Register b) {
  PPC64_ONLY(mulhd_(d, a, b));
  NOT_PPC64(unimplemented("128bit mul"));
}

inline void MacroAssembler::mulhu(Register d, Register a, Register b) {
  PPC64_ONLY(mulhdu(d, a, b));
  NOT_PPC64(unimplemented("128bit mul"));
}

inline void MacroAssembler::mulhu_(Register d, Register a, Register b) {
  PPC64_ONLY(mulhdu_(d, a, b));
  NOT_PPC64(unimplemented("128bit mul"));
}

inline void MacroAssembler::div(Register d, Register a, Register b) {
  PPC64_ONLY(divd(d, a, b));
  NOT_PPC64(unimplemented("64bit by 64bit div"));
}

inline void MacroAssembler::div_(Register d, Register a, Register b) {
  PPC64_ONLY(divd_(d, a, b));
  NOT_PPC64(unimplemented("64bit by 64bit div"));
}

inline void MacroAssembler::extsw(Register a, Register s) {
  PPC64_ONLY(Assembler::extsw(a, s));
  NOT_PPC64(mr_if_needed(a, s));
}

inline void MacroAssembler::cntlz(Register a, Register s) {
  PPC64_ONLY(cntlzd(a, s));
  NOT_PPC64(cntlzw(a, s));
}

/// Shift
//Shift Left (Doubleword)
inline void MacroAssembler::sl(Register a, Register s, Register b) {
  PPC64_ONLY(sld(a, s, b));
  NOT_PPC64(slw(a, s, b));
}
inline void MacroAssembler::sl_(Register a, Register s, Register b) {
  PPC64_ONLY(sld_(a, s, b));
  NOT_PPC64(slw_(a, s, b));
}
//Shift Right (Doubleword)
inline void MacroAssembler::sr(Register a, Register s, Register b) {
  PPC64_ONLY(srd(a, s, b));
  NOT_PPC64(srw(a, s, b));
}
inline void MacroAssembler::sr_(Register a, Register s, Register b) {
  PPC64_ONLY(srd_(a, s, b));
  NOT_PPC64(srw_(a, s, b));
}
//Shift Right Algebraic (Doubleword)
inline void MacroAssembler::sra(Register a, Register s, Register b) {
  PPC64_ONLY(srad(a, s, b));
  NOT_PPC64(sraw(a, s, b));
}
inline void MacroAssembler::sra_(Register a, Register s, Register b) {
  PPC64_ONLY(srad_(a, s, b));
  NOT_PPC64(sraw_(a, s, b));
}
//Shift Right Algebraic (Doubleword) Immediate
inline void MacroAssembler::srai(Register a, Register s, int sh6) {
  PPC64_ONLY(sradi(a, s, sh6));
  NOT_PPC64(srawi(a, s, sh6));
}
inline void MacroAssembler::srai_(Register a, Register s, int sh6) {
  PPC64_ONLY(sradi_(a, s, sh6));
  NOT_PPC64(srawi_(a, s, sh6));
}

// constants used for shift are unchecked, since constraints are depends on
// implementation. For example, extended mnemonic extrwi takes n > 0, because
// it's implemented as rlwinm(a, s, b+n, 32-n, 31); and rlwinm requires it's
// 4th argument to be in [0, 31]. So arguments are just passed to assembler,
// which should check constant correctness on emitting.

/// Rotate
//Rotate Left (Doubleword) Immediate then Clear:
//rlic RA,RS,SH,MB
//The contents of register RS are rotated 64 left SH bits. A
//mask is generated having 1-bits from bit MB through bit
//63-SH and 0-bits elsewhere. The rotated data are
//ANDed with the generated mask and the result is
//placed into register RA.
inline void MacroAssembler::rlic(Register a, Register s, int sh6, int mb6) {
  PPC64_ONLY(rldic(a, s, sh6, mb6));
  NOT_PPC64(sh6 &= 0x1f; rlwinm (a, s, sh6, mb6-32, 31-sh6));
}
inline void MacroAssembler::rlic_(Register a, Register s, int sh6, int mb6) {
  PPC64_ONLY(rldic_(a, s, sh6, mb6));
  NOT_PPC64(sh6 &= 0x1f; rlwinm_(a, s, sh6, mb6-32, 31-sh6));
}

//Rotate Left (Doubleword) Immediate then Clear Right:
//rlicr RA,RS,SH,ME
//The contents of register RS are rotated 64 left SH bits. A
//mask is generated having 1-bits from bit 0 through bit
//ME and 0-bits elsewhere. The rotated data are ANDed
//with the generated mask and the result is placed into
//register RA.
inline void MacroAssembler::rlicr(Register a, Register s, int sh6, int me6) {
  PPC64_ONLY(rldicr(a, s, sh6, me6));
  NOT_PPC64(rlwinm(a, s, sh6 & 0x1f, 0, me6-32));
}
inline void MacroAssembler::rlicr_(Register a, Register s, int sh6, int me6) {
  PPC64_ONLY(rldicr_(a, s, sh6, me6));
  NOT_PPC64(rlwinm_(a, s, sh6 & 0x1f, 0, me6-32));
}

//Rotate Left (Doubleword) Immediate then Clear Left
//rlicl RA,RS,SH,MB
//The contents of register RS are rotated 64 left SH bits. A
//mask is generated having 1-bits from bit MB through bit
//63 and 0-bits elsewhere. The rotated data are ANDed
//with the generated mask and the result is placed into
//register RA.
inline void MacroAssembler::rlicl(Register a, Register s, int sh6, int mb6) {
  PPC64_ONLY(rldicl(a, s, sh6, mb6));
  NOT_PPC64(rlwinm(a, s, sh6 & 0x1f, mb6-32, 31));
}
inline void MacroAssembler::rlicl_(Register a, Register s, int sh6, int mb6) {
  PPC64_ONLY(rldicl_(a, s, sh6, mb6));
  NOT_PPC64(rlwinm_(a, s, sh6 & 0x1f, mb6-32, 31));
}

//Rotate Left (Doubleword) Immediate then Mask Insert
//rlimi RA,RS,SH,MB
//The contents of register RS are rotated 64 left SH bits. A
//mask is generated having 1-bits from bit MB through bit
//63-SH and 0-bits elsewhere. The rotated data are
//inserted into register RA under control of the generated
//mask.
inline void MacroAssembler::rlimi(Register a, Register s, int sh6, int mb6) {
  PPC64_ONLY(rldimi(a, s, sh6, mb6));
  NOT_PPC64(sh6 &= 0x1f; Assembler::rlwimi (a, s, sh6, mb6-32, 31-sh6));
}
inline void MacroAssembler::rlimi_(Register a, Register s, int sh6, int mb6) {
  PPC64_ONLY(rldimi_(a, s, sh6, mb6));
  NOT_PPC64(sh6 &= 0x1f; Assembler::rlwimi_(a, s, sh6, mb6-32, 31-sh6));
}

//Shift left immediate: sli ra,rs,n (n < 64) rlicr ra,rs,n,63-n
inline void MacroAssembler::sli(Register a, Register s, int sh6) {
  PPC64_ONLY(sldi(a, s, sh6));
  NOT_PPC64(slwi (a, s, sh6));
}
inline void MacroAssembler::sli_(Register a, Register s, int sh6) {
  PPC64_ONLY(sldi_(a, s, sh6));
  NOT_PPC64(slwi_(a, s, sh6));
}
//Shift right immediate: sri ra,rs,n (n < 64) rlicl ra,rs,64-n,n
inline void MacroAssembler::sri(Register a, Register s, int sh6) {
  PPC64_ONLY(srdi(a, s, sh6));
  NOT_PPC64(srwi (a, s, sh6));
}
inline void MacroAssembler::sri_(Register a, Register s, int sh6) {
  PPC64_ONLY(srdi_(a, s, sh6));
  NOT_PPC64(srwi_(a, s, sh6));
}
//Clear right immediate: clrri ra,rs,n (n < 64) rlicr ra,rs,0,63-n
inline void MacroAssembler::clrri(Register a, Register s, int ui6) {
  PPC64_ONLY(clrrdi(a, s, ui6));
  NOT_PPC64(clrrwi (a, s, ui6));
}
inline void MacroAssembler::clrri_(Register a, Register s, int ui6) {
  PPC64_ONLY(clrrdi_(a, s, ui6));
  NOT_PPC64(clrrwi_(a, s, ui6));
}
//Clear left immediate: clrli ra,rs,n (n < 64) rlicl ra,rs,0,n
inline void MacroAssembler::clrli(Register a, Register s, int ui6) {
  PPC64_ONLY(clrldi(a, s, ui6));
  NOT_PPC64(clrlwi(a, s, ui6-32));
}
inline void MacroAssembler::clrli_(Register a, Register s, int ui6) {
  PPC64_ONLY(clrldi_(a, s, ui6));
  NOT_PPC64(clrlwi_(a, s, ui6-32));
}
//Clear left and shift left immediate: clrlsli ra,rs,b,n (n <= b < 64) rlic ra,rs,n,b-n
inline void MacroAssembler::clrlsli(Register a, Register s, int clrl6, int shl6) {
  PPC64_ONLY(clrlsldi(a, s, clrl6, shl6));
  NOT_PPC64(clrlslwi (a, s, clrl6-32, shl6 & 0x1f));
}
inline void MacroAssembler::clrlsli_(Register a, Register s, int clrl6, int shl6) {
  PPC64_ONLY(clrlsldi_(a, s, clrl6, shl6));
  NOT_PPC64(clrlslwi_(a, s, clrl6-32, shl6 & 0x1f));
}
//Extract and right justify immediate: extri ra,rs,n,b (n > 0) rlicl ra,rs,b+n,64-n
inline void MacroAssembler::extri(Register a, Register s, int n, int b) {
  PPC64_ONLY(extrdi(a, s, n, b));
  NOT_PPC64(extrwi(a, s, n, b-32));
}
//Rotate left immediate: rotld ra,rs,rb rldcl ra,rs,rb,0
inline void MacroAssembler::rotli(Register a, Register s, int n) {
  PPC64_ONLY(rotldi(a, s, n));
  NOT_PPC64(rotlwi(a, s, n));
}
//Rotate right immediate: rotri ra,rs,n rlicl ra,rs,64-n,0
inline void MacroAssembler::rotri(Register a, Register s, int n) {
  PPC64_ONLY(rotrdi(a, s, n));
  NOT_PPC64(rotrwi(a, s, n & 0x1f));
}

// Insert from right immediate insri ra,rs,n,b (n > 0) rlimi ra,rs,64-(b+n),b
inline void MacroAssembler::insri(Register a, Register s, int n,   int b) {
  PPC64_ONLY(insrdi(a, s, n, b));
  NOT_PPC64(insrwi(a, s, n, b-32));
}

inline void MacroAssembler::lwax(Register d, Register s1, Register s2) {
  PPC64_ONLY(Assembler::lwax(d, s1, s2));
  NOT_PPC64(lwzx(d, s1, s2));
}
inline void MacroAssembler::lwa(Register d, int si16,    Register s1) {
  PPC64_ONLY(Assembler::lwa(d, si16, s1));
  NOT_PPC64(lwz(d, si16, s1));
}
inline void MacroAssembler::l(Register d, int si16,    Register s1) {
  PPC64_ONLY(ld(d, si16, s1));
  NOT_PPC64(lwz(d, si16, s1));
}
inline void MacroAssembler::lu(Register d, int si16,    Register s1) {
  PPC64_ONLY(ldu(d, si16, s1));
  NOT_PPC64(lwzu(d, si16, s1));
}
inline void MacroAssembler::lx(Register d, Register s1, Register s2) {
  PPC64_ONLY(ldx(d, s1, s2));
  NOT_PPC64(lwzx(d, s1, s2));
}
inline void MacroAssembler::st(Register d, int si16,    Register s1) {
  PPC64_ONLY(std(d, si16, s1));
  NOT_PPC64(stw(d, si16, s1));
}
inline void MacroAssembler::stx(Register d, Register s1, Register s2) {
  PPC64_ONLY(stdx(d, s1, s2));
  NOT_PPC64(stwx(d, s1, s2));
}
inline void MacroAssembler::stu(Register d, int si16,    Register s1) {
  PPC64_ONLY(stdu(d, si16, s1));
  NOT_PPC64(stwu(d, si16, s1));
}
inline void MacroAssembler::stux(Register s, Register a,  Register b) {
  PPC64_ONLY(stdux(s, a, b));
  NOT_PPC64(stwux(s, a, b));
}
inline void MacroAssembler::larx(Register d, Register a, Register b, bool hint_ea) {
  PPC64_ONLY(ldarx(d, a, b, hint_ea));
  NOT_PPC64(lwarx(d, a, b, hint_ea));
}
inline void MacroAssembler::stcx_(Register s, Register a, Register b) {
  PPC64_ONLY(stdcx_(s, a, b));
  NOT_PPC64(
    // Wii U Espresso Patch
    dcbst(a, b);
    stwcx_(s, a, b)
  );
}
inline void MacroAssembler::ti(int tobits, Register a, int si16) {
  PPC64_ONLY(tdi(tobits, a, si16));
  NOT_PPC64(twi_unchecked(tobits, a, si16));
}
inline void MacroAssembler::t(int tobits, Register a, Register b) {
  PPC64_ONLY(td(tobits, a, b));
  NOT_PPC64(tw(tobits, a, b));
}
inline void MacroAssembler::popcnt(Register a, Register s) {
  PPC64_ONLY(popcntd(a, s));
  NOT_PPC64(popcntw(a, s));
}
inline void MacroAssembler::lwax(Register d, Register s2) {
  PPC64_ONLY(Assembler::lwax(d, s2));
  NOT_PPC64(lwzx(d, s2));
}
inline void MacroAssembler::lwa(Register d, int si16) {
  PPC64_ONLY(Assembler::lwa(d, si16));
  NOT_PPC64(lwz(d, si16));
}
inline void MacroAssembler::l(Register d, int si16) {
  PPC64_ONLY(ld(d, si16));
  NOT_PPC64(lwz(d, si16));
}
inline void MacroAssembler::lx(Register d, Register s2) {
  PPC64_ONLY(ldx(d, s2));
  NOT_PPC64(lwzx(d, s2));
}
inline void MacroAssembler::st(Register d, int si16) {
  PPC64_ONLY(std(d, si16));
  NOT_PPC64(stw(d, si16));
}
inline void MacroAssembler::stx(Register d, Register s2) {
  PPC64_ONLY(stdx(d, s2));
  NOT_PPC64(stwx(d, s2));
}
inline void MacroAssembler::larx(Register d, Register b, bool hint_ea) {
  PPC64_ONLY(ldarx(d, b, hint_ea));
  NOT_PPC64(lwarx(d, b, hint_ea));
}
inline void MacroAssembler::stcx_(Register s, Register b) {
  PPC64_ONLY(stdcx_(s, b));
  NOT_PPC64(
    // Wii U Espresso Patch
    dcbst(b);
    stwcx_(s, b)
  );
}
inline void MacroAssembler::testbitdi(ConditionRegister cr, Register a, Register s, int ui6) {
  // copypaste from assembler, change rlicr(a, s, 63-ui, 0) to rlicl below
  if (cr == CCR0) {
    rlicl_(a, s, 63 - ui6 + 1, 63);
  } else {
    rlicl_(a, s, 63 - ui6 + 1, 63);
    cmpi(cr, a, 0);
  }
}

inline void MacroAssembler::l(Register d, RegisterOrConstant roc, Register s1) {
  PPC64_ONLY(ld(d, roc, s1));
  NOT_PPC64(lwz(d, roc, s1));
}
inline void MacroAssembler::lwa(Register d, RegisterOrConstant roc, Register s1) {
  PPC64_ONLY(Assembler::lwa(d, roc, s1));
  NOT_PPC64(lwz(d, roc, s1));
}
inline void MacroAssembler::st(Register d, RegisterOrConstant roc, Register s1, Register tmp) {
  PPC64_ONLY(std(d, roc, s1, tmp));
  NOT_PPC64(stw(d, roc, s1, tmp));
}

inline void MacroAssembler::cmpi(ConditionRegister crx, Register a, int si16) {
  PPC64_ONLY(cmpdi(crx, a, si16));
  NOT_PPC64(cmpwi(crx, a, si16));
}

inline void MacroAssembler::cmp(ConditionRegister crx, Register a, Register b) {
  PPC64_ONLY(cmpd(crx, a, b));
  NOT_PPC64(cmpw(crx, a, b));
}

inline void MacroAssembler::cmpli(ConditionRegister crx, Register a, int ui16) {
  PPC64_ONLY(cmpldi(crx, a, ui16));
  NOT_PPC64(cmplwi(crx, a, ui16));
}

inline void MacroAssembler::cmpl(ConditionRegister crx, Register a, Register b) {
  PPC64_ONLY(cmpld(crx, a, b));
  NOT_PPC64(cmplw(crx, a, b));
}

#ifdef __SPE__

inline bool MacroAssembler::ev_check_aligned(Register temp, int si16, Register Ra, Register Rb, int code) {
#ifdef SPE_EV_CHECK_ALIGN
  bool ret = true;
  if (temp == Ra || temp == Rb) {
    if (R0 == Ra || R0 == Rb) {
      // can't get temp register, don't check
      return false;
    }
    temp = R0;
    ret = false;
  }

  assert_different_registers(temp, Ra);
  assert_different_registers(temp, Rb);

  addi(temp, Ra, si16);
  if (Rb != noreg) {
    add(temp, temp, Rb);
  }

  andi_(temp, temp, 7);
  Label Lskip;
  beq(CCR0, Lskip);

  // use forever loop as fail because inserting
  // real error message takes too much space
  Label Lfailed;
  bind(Lfailed);
  b(Lfailed);

  bind(Lskip);
  return ret;
#else
  return false;
#endif
}

inline void MacroAssembler::evstdd_aligned(Register s, int si16, Register a) {
  evstdd(s, si16, a);
#ifdef ASSERT
  if (ev_check_aligned(s, si16, a, noreg, 0x205)) {
    // `s` trashed, reloading
    evldd(s, si16, a);
  }
#endif
}

inline void MacroAssembler::evstddx_aligned(Register s, Register a, Register b) {
  evstddx(s, a, b);
#ifdef ASSERT
  if (ev_check_aligned(s, 0, a, b, 0x206)) {
    // `s` trashed, reloading
    evlddx(s, a, b);
  }
#endif
}

inline void MacroAssembler::evldd_aligned(Register d, int si16, Register a) {
#ifdef ASSERT
  ev_check_aligned(d, si16, a, noreg, 0x207);
#endif
  evldd(d, si16, a);
}

inline void MacroAssembler::evlddx_aligned(Register d, Register a, Register b) {
#ifdef ASSERT
  ev_check_aligned(d, 0, a, b, 0x208);
#endif
  evlddx(d, a, b);
}

inline void MacroAssembler::evstdd_unaligned(Register s, int si16, Register a, Register scratch) {
  assert_different_registers(scratch, a);

  stw(s, si16 + BytesPerWord, a);
  evmergehi(scratch, scratch, s);
  stw(scratch, si16, a);
}

inline void MacroAssembler::evstddx_unaligned(Register s, Register a, Register b, Register scratch) {
  assert_different_registers(scratch, s);
  assert_different_registers(scratch, a);
  assert_different_registers(scratch, b);

  evmergehi(scratch, scratch, s);
  stwx(scratch, a, b);
  addi(scratch, a, wordSize);
  stwx(s, scratch, b);
}

inline void MacroAssembler::evldd_unaligned(Register d, int si16, Register a, Register scratch) {
  assert_different_registers(a, d);
  assert_different_registers(scratch, d);
  lwz(d, si16 + BytesPerWord, a);
  lwz(scratch, si16, a);
  evmergelo(d, scratch, d);
}

inline void MacroAssembler::evlddx_unaligned(Register d, Register a, Register b, Register scratch) {
  assert_different_registers(scratch, d);
  // scratch and b could be same
  assert_different_registers(scratch, a);
  assert_different_registers(d, a);
  assert_different_registers(d, b);

  lwzx(d, a, b);
  addi(scratch, b, wordSize);
  lwzx(scratch, a, scratch);
  evmergelo(d, d, scratch);
}

#endif // __SPE__

#endif // CPU_PPC_VM_MACROASSEMBLER_PPC_INLINE_HPP
