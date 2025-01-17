/*
 * Copyright (c) 2002, 2011, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_INTERPRETER_BYTECODEINTERPRETER_INLINE_HPP
#define SHARE_VM_INTERPRETER_BYTECODEINTERPRETER_INLINE_HPP

#include "interpreter/bytecodeInterpreter.hpp"
#include "runtime/stubRoutines.hpp"

// This file holds platform-independent bodies of inline functions for the C++ based interpreter

#ifdef CC_INTERP

#ifdef ASSERT
#define VERIFY_OOP(o_) \
      if (VerifyOops) { \
        assert((oop(o_))->is_oop_or_null(), "Not an oop!"); \
        StubRoutines::_verify_oop_count++;  \
      }
#else
#define VERIFY_OOP(o)
#endif

// Platform dependent data manipulation
#ifdef TARGET_ARCH_x86
# include "bytecodeInterpreter_x86.inline.hpp"
#endif
#ifdef TARGET_ARCH_aarch64
# include "bytecodeInterpreter_aarch64.inline.hpp"
#endif
#ifdef TARGET_ARCH_mips
# include "bytecodeInterpreter_mips.inline.hpp"
#endif
#ifdef TARGET_ARCH_sparc
# include "bytecodeInterpreter_sparc.inline.hpp"
#endif
#ifdef TARGET_ARCH_zero
# include "bytecodeInterpreter_zero.inline.hpp"
#endif
#ifdef TARGET_ARCH_arm
# include "bytecodeInterpreter_arm.inline.hpp"
#endif
#ifdef TARGET_ARCH_ppc
# include "bytecodeInterpreter_ppc.inline.hpp"
#endif
#ifdef TARGET_ARCH_aarch32
# include "bytecodeInterpreter_aarch32.inline.hpp"
#endif

#endif // CC_INTERP

#endif // SHARE_VM_INTERPRETER_BYTECODEINTERPRETER_INLINE_HPP
