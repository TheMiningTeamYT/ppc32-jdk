/*
 * Copyright (c) 1999, 2013, Oracle and/or its affiliates. All rights reserved.
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

#ifndef CPU_PPC_VM_GLOBALDEFINITIONS_PPC_HPP
#define CPU_PPC_VM_GLOBALDEFINITIONS_PPC_HPP

// Size of PPC Instructions
const int BytesPerInstWord = 4;

const int StackAlignmentInBytes = 16;

// Indicates whether the C calling conventions require that
// 32-bit integer argument values are properly extended to 64 bits.
// If set, SharedRuntime::c_calling_convention() must adapt
// signatures accordingly.
const bool CCallingConventionRequiresIntsAsLongs = true;

#ifdef PPC64
#define SUPPORTS_NATIVE_CX8
#endif // PPC64

// The PPC CPUs are NOT multiple-copy-atomic.
#define CPU_NOT_MULTIPLE_COPY_ATOMIC

// The expected size in bytes of a cache line, used to pad data structures.
//#define DEFAULT_CACHE_LINE_SIZE 128

#ifdef __SPE__
// ABI is SPE, important for native and runtime calls
#define SPE_ABI
// Platform have SPE and therefore evldd/evstdd 8 byte atomic load/store
// instructions. Use them for atomics, not potentially emulated VFP instructions.
#define SPE_ATOMIC
#endif // __SPE__

// USE_VFP_WITH_SPE should be passed from command line to force VFP
// instructions with SPE ABI.
// The VM should check USE_SPE flag by #ifdef/#else to decide which
// instructions use.
#if defined(__SPE__) && !defined(USE_VFP_WITH_SPE)
#define USE_SPE
#endif

#ifdef USE_SPE
#define USE_SPE_ONLY(x) x
#define NOT_USE_SPE(x)
// enable SOFT FP behavior in the shared code
#define __SOFTFP__
#else
#define USE_SPE_ONLY(x)
#define NOT_USE_SPE(x) x
#endif // USE_SPE_ONLY

#if defined(USE_SPE) && defined(DEBUG)
#define SPE_EV_CHECK_ALIGN
#endif

#endif // CPU_PPC_VM_GLOBALDEFINITIONS_PPC_HPP
