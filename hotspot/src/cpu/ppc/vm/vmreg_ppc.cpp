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

#include "precompiled.hpp"
#include "asm/assembler.hpp"
#include "code/vmreg.hpp"

void VMRegImpl::set_regName() {
  Register reg = ::as_Register(0);
  int i;
  for (i = 0; i < ConcreteRegisterImpl::max_gpr; ) {
    regName[i++] = reg->name();
    regName[i++] = reg->name();
    if (reg->encoding() < RegisterImpl::number_of_registers-1)
      reg = reg->successor();
  }
#ifndef USE_SPE
  FloatRegister freg = ::as_FloatRegister(0);
  for ( ; i < ConcreteRegisterImpl::max_fpr; ) {
    regName[i++] = freg->name();
    regName[i++] = freg->name();
    if (freg->encoding() < FloatRegisterImpl::number_of_registers-1)
      freg = freg->successor();
  }
#endif
  for ( ; i < ConcreteRegisterImpl::number_of_registers; i++) {
    regName[i] = "NON-GPR-FPR";
  }
}

