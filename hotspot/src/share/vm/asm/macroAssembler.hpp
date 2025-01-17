/*
 * Copyright (c) 2012, 2013, Oracle and/or its affiliates. All rights reserved.
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
// made by Azul Systems, Inc.  The dates of such changes are 2013-2017.
// Copyright 2013-2017 Azul Systems, Inc.  All Rights Reserved.
//
// Please contact Azul Systems, 385 Moffett Park Drive, Suite 115, Sunnyvale,
// CA 94089 USA or visit www.azul.com if you need additional information or
// have any questions.

/*
 * This file has been modified by Loongson Technology in 2015. These
 * modifications are Copyright (c) 2015 Loongson Technology, and are made
 * available on the same license terms set forth above.
 */

#ifndef SHARE_VM_ASM_MACROASSEMBLER_HPP
#define SHARE_VM_ASM_MACROASSEMBLER_HPP

#include "asm/assembler.hpp"

#ifdef TARGET_ARCH_x86
# include "macroAssembler_x86.hpp"
#endif
#ifdef TARGET_ARCH_sparc
# include "macroAssembler_sparc.hpp"
#endif
#ifdef TARGET_ARCH_zero
# include "assembler_zero.hpp"
#endif
#ifdef TARGET_ARCH_arm
# include "macroAssembler_arm.hpp"
#endif
#ifdef TARGET_ARCH_ppc
# include "macroAssembler_ppc.hpp"
#endif
#ifdef TARGET_ARCH_aarch32
# include "macroAssembler_aarch32.hpp"
#endif
#ifdef TARGET_ARCH_aarch64
# include "macroAssembler_aarch64.hpp"
#endif
#ifdef TARGET_ARCH_mips
# include "macroAssembler_mips.hpp"
#endif

#endif // SHARE_VM_ASM_MACROASSEMBLER_HPP
