/*
 * Copyright (c) 2005, 2015, Oracle and/or its affiliates. All rights reserved.
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

#ifndef CPU_PPC_VM_C1_LINEARSCAN_PPC_HPP
#define CPU_PPC_VM_C1_LINEARSCAN_PPC_HPP


inline bool LinearScan::is_processed_reg_num(int reg_num) {
#define COMMA ,
  const LIR_Opr processed[] = { FrameMap::R0_opr, FrameMap::R1_opr, NOT_PPC64(FrameMap::R2_opr COMMA)
    FrameMap::R13_opr, FrameMap::R16_opr, FrameMap::R29_opr };
#undef COMMA
  const int processed_n = sizeof(processed)/sizeof(processed[0]);

  for (int i = 0; i < processed_n; ++i) {
    assert(processed[i]->cpu_regnr() == FrameMap::last_cpu_reg() + 1 + i, "wrong assumption below");
  }

  return reg_num <= FrameMap::last_cpu_reg() || reg_num >= pd_nof_cpu_regs_frame_map;
}

inline int LinearScan::num_physical_regs(BasicType type) {
#ifndef PPC64
  if (type == T_LONG) {
    return 2;
  }
#endif
  return 1;
}


inline bool LinearScan::requires_adjacent_regs(BasicType type) {
  return false;
}

inline bool LinearScan::is_caller_save(int assigned_reg) {
  return true; // assigned_reg < pd_first_callee_saved_reg;
}


inline void LinearScan::pd_add_temps(LIR_Op* op) {
 if (op->code() == lir_shl || op->code() == lir_shr || op->code() == lir_ushr) {
    LIR_Op2* op2 = op->as_Op2();
    if (op2) {
      if (op2->in_opr1()->is_valid()) {
          add_temp(op2->in_opr1(), op->id(), mustHaveRegister);
      }
    }
  } else if (op->code() == lir_mul) {
    LIR_Op2* op2 = op->as_Op2();
    if (op2) {
      if (op2->in_opr1()->is_valid()) {
          add_temp(op2->in_opr1(), op->id(), mustHaveRegister);
      }
      if (op2->in_opr2()->is_register()) {
          add_temp(op2->in_opr2(), op->id(), mustHaveRegister);
      }
    }

  } else if (op->code() == lir_idiv || op->code() == lir_irem) {
    // irem/idiv could reuse input register, prevent mixing with temps
    LIR_Op3* op3 = op->as_Op3();
    if (op3) {
      if (op3->in_opr1()->is_valid()) {
          add_temp(op3->in_opr1(), op->id(), mustHaveRegister);
      }
    }
  } else if (GenerateCompilerNullChecks && !TrapBasedNullChecks && op->code() == lir_null_check) {
    // see c1_MacroAssembler_ppc.cpp, null_check
    add_temp(FrameMap::R12_opr, op->id(), mustHaveRegister);
  }
}


inline bool LinearScanWalker::pd_init_regs_for_alloc(Interval* cur) {
  if (allocator()->gen()->is_vreg_flag_set(cur->reg_num(), LIRGenerator::callee_saved)) {
    assert(cur->type() != T_FLOAT && cur->type() != T_DOUBLE, "cpu regs only");
    _first_reg = pd_first_callee_saved_reg;
    _last_reg = pd_last_callee_saved_reg;
    ShouldNotReachHere(); // Currently no callee saved regs.
    return true;
  } else if (cur->type() == T_INT || cur->type() == T_LONG || cur->type() == T_OBJECT ||
#ifdef USE_SPE
            cur->type() == T_FLOAT || cur->type() == T_DOUBLE ||
#endif
             cur->type() == T_ADDRESS || cur->type() == T_METADATA) {
    _first_reg = pd_first_cpu_reg;
    _last_reg = pd_last_cpu_reg;
    return true;
  }
  return false;
}

#endif // CPU_PPC_VM_C1_LINEARSCAN_PPC_HPP
