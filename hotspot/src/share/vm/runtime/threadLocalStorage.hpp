/*
 * Copyright (c) 1997, 2011, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_RUNTIME_THREADLOCALSTORAGE_HPP
#define SHARE_VM_RUNTIME_THREADLOCALSTORAGE_HPP

#include "gc_implementation/shared/gcUtil.hpp"
#include "runtime/os.hpp"
#include "utilities/top.hpp"

// Interface for thread local storage

// Fast variant of ThreadLocalStorage::get_thread_slow
extern "C" Thread*   get_thread();

// Get raw thread id: e.g., %g7 on sparc, fs or gs on x86
extern "C" uintptr_t _raw_thread_id();

class ThreadLocalStorage : AllStatic {

 // Exported API
 public:
  static void    set_thread(Thread* thread);
  static Thread* get_thread_slow();
  static void    invalidate_all() { pd_invalidate_all(); }
  static void    init();
  static bool    is_initialized();

  // Machine dependent stuff
#ifdef TARGET_OS_ARCH_linux_x86
# include "threadLS_linux_x86.hpp"
#endif
#ifdef TARGET_OS_ARCH_linux_aarch64
# include "threadLS_linux_aarch64.hpp"
#endif
#ifdef TARGET_OS_ARCH_linux_mips
# include "threadLS_linux_mips.hpp"
#endif
#ifdef TARGET_OS_ARCH_linux_sparc
# include "threadLS_linux_sparc.hpp"
#endif
#ifdef TARGET_OS_ARCH_linux_zero
# include "threadLS_linux_zero.hpp"
#endif
#ifdef TARGET_OS_ARCH_linux_arm
# include "threadLS_linux_arm.hpp"
#endif
#ifdef TARGET_OS_ARCH_linux_ppc
# include "threadLS_linux_ppc.hpp"
#endif
#ifdef TARGET_OS_ARCH_linux_aarch32
# include "threadLS_linux_aarch32.hpp"
#endif
#ifdef TARGET_OS_ARCH_solaris_x86
# include "threadLS_solaris_x86.hpp"
#endif
#ifdef TARGET_OS_ARCH_solaris_sparc
# include "threadLS_solaris_sparc.hpp"
#endif
#ifdef TARGET_OS_ARCH_windows_x86
# include "threadLS_windows_x86.hpp"
#endif
#ifdef TARGET_OS_ARCH_aix_ppc
# include "threadLS_aix_ppc.hpp"
#endif
#ifdef TARGET_OS_ARCH_bsd_x86
# include "threadLS_bsd_x86.hpp"
#endif
#ifdef TARGET_OS_ARCH_bsd_zero
# include "threadLS_bsd_zero.hpp"
#endif

#if !defined(SOLARIS) && !defined(AARCH32)
 public:
  // Accessor
  static inline int  thread_index()              { return _thread_index; }
  static inline void set_thread_index(int index) { _thread_index = index; }

 private:
  static int     _thread_index;

  static void    generate_code_for_get_thread();

  // Processor dependent parts of set_thread and initialization
  static void pd_set_thread(Thread* thread);
  static void pd_init();

#endif // !defined(SOLARIS) && !defined(AARCH32)

  // Invalidate any thread cacheing or optimization schemes.
  static void pd_invalidate_all();

};

#endif // SHARE_VM_RUNTIME_THREADLOCALSTORAGE_HPP
