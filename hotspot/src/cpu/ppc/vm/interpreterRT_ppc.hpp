/*
 * Copyright (c) 2002, 2013, Oracle and/or its affiliates. All rights reserved.
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

#ifndef CPU_PPC_VM_INTERPRETERRT_PPC_HPP
#define CPU_PPC_VM_INTERPRETERRT_PPC_HPP

#include "memory/allocation.hpp"

// native method calls

class SignatureHandlerGenerator: public NativeSignatureIterator {
 private:
  MacroAssembler* _masm;
#ifdef PPC64
  // number of already used floating-point argument registers
  int _num_used_fp_arg_regs;
#else // PPC64
#ifndef USE_SPE
  int _next_fp_arg_reg;
#endif
  int _next_gp_arg_reg;
  int _next_stack_arg_offset;

  bool on_regs(BasicType type) {
    switch(type) {
    case T_DOUBLE:
#ifndef USE_SPE
    case T_FLOAT:
      return _next_fp_arg_reg <= 8;
#endif
    case T_LONG:
      return _next_gp_arg_reg <= 9;
    default:
      break;
    }
    return _next_gp_arg_reg <= 10;
  }

  void align_reg_start(BasicType type) {
    if (type == T_LONG) {
      if (is_even(_next_gp_arg_reg)) {
        ++_next_gp_arg_reg;
      }
    }
  }

  Register get_gp_regs(BasicType type) {
    assert(on_regs(type), "should be");

    if (type != T_LONG) {
      return as_Register(_next_gp_arg_reg++);
    }

    assert(is_odd(_next_gp_arg_reg), "should be aligned");

    const int start_reg = _next_gp_arg_reg;
    _next_gp_arg_reg += 2;
    return as_Register(start_reg);
  }

#ifndef USE_SPE
  FloatRegister get_fp_reg() {
    assert(on_regs(T_DOUBLE), "should be");
    return as_FloatRegister(_next_fp_arg_reg++);
  }
#endif

  int get_stack_offset(BasicType type) {
    assert(!on_regs(type), "should be");
    const int type_size = (type == T_LONG) || (type == T_DOUBLE) ? 2 : 1;
    if ((type_size == 2) && is_odd(_next_stack_arg_offset)) {
      ++_next_stack_arg_offset;
    }

    const int offset = _next_stack_arg_offset;
    _next_stack_arg_offset += type_size;
    return offset;
  }
#endif // PPC64

  void pass_int();
  void pass_long();
#ifndef SPE_ABI
  void pass_double();
  void pass_float();
#else // SPE_ABI
  void pass_double() {
      pass_long();
  }
  void pass_float() {
      pass_int();
  }
#endif //SPE_ABI
  void pass_object();

 public:
  // Creation
  SignatureHandlerGenerator(methodHandle method, CodeBuffer* buffer) : NativeSignatureIterator(method) {
    _masm = new MacroAssembler(buffer);
#ifdef PPC64
    _num_used_fp_arg_regs = 0;
#else
#ifndef USE_SPE
    _next_fp_arg_reg = F1->encoding();
#endif
    _next_gp_arg_reg = R3->encoding() + jni_offset();
    _next_stack_arg_offset = 0;
#endif
  }

  // Code generation
  void generate(uint64_t fingerprint);
};

// Support for generate_slow_signature_handler.
static address get_result_handler(JavaThread* thread, Method* method);

// A function to get the signature.
static address get_signature(JavaThread* thread, Method* method);

#endif // CPU_PPC_VM_INTERPRETERRT_PPC_HPP
