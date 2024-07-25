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

#ifndef CPU_PPC_VM_JNITYPES_PPC_HPP
#define CPU_PPC_VM_JNITYPES_PPC_HPP

#include "memory/allocation.hpp"
#include "oops/oop.hpp"
#include "prims/jni.h"

// This file holds platform-dependent routines used to write primitive
// jni types to the array of arguments passed into JavaCalls::call.

class JNITypes : AllStatic {
  // These functions write a java primitive type (in native format) to
  // a java stack slot array to be passed as an argument to
  // JavaCalls:calls.  I.e., they are functionally 'push' operations
  // if they have a 'pos' formal parameter.  Note that jlong's and
  // jdouble's are written _in reverse_ of the order in which they
  // appear in the interpreter stack.  This is because call stubs (see
  // stubGenerator_ppc.cpp) reverse the argument list constructed by
  // JavaCallArguments (see javaCalls.hpp).

 private:

#ifndef PPC64
  // 32bit Helper routines, put 2 ints in reverse order
  static inline void    put_int2r(jint *from, intptr_t *to)           { *(jint *)(to++) = from[1];
                                                                        *(jint *)(to  ) = from[0]; }
  static inline void    put_int2r(jint *from, intptr_t *to, int& pos) { put_int2r(from, to + pos); pos += 2; }
#endif // PPC64

 public:
  // Ints are stored in native format in one JavaCallArgument slot at *to.
  static inline void put_int(jint  from, intptr_t *to)           { *(jint *)(to +   0  ) =  from; }
  static inline void put_int(jint  from, intptr_t *to, int& pos) { *(jint *)(to + pos++) =  from; }
  static inline void put_int(jint *from, intptr_t *to, int& pos) { *(jint *)(to + pos++) = *from; }

#ifdef PPC64
  // Longs are stored in native format in one JavaCallArgument slot at
  // *(to+1).
  static inline void put_long(jlong  from, intptr_t *to) {
    *(jlong*) (to + 1) = from;
  }

  static inline void put_long(jlong  from, intptr_t *to, int& pos) {
    *(jlong*) (to + 1 + pos) = from;
    pos += 2;
  }

  static inline void put_long(jlong *from, intptr_t *to, int& pos) {
    *(jlong*) (to + 1 + pos) = *from;
    pos += 2;
  }
#else
  // Longs are stored in reversed native word format in two JavaCallArgument slots at *to.
  // The high half is in *(to+1) and the low half in *to.
  static inline void put_long(jlong  from, intptr_t *to)           { put_int2r((jint *)&from, to); }
  static inline void put_long(jlong  from, intptr_t *to, int& pos) { put_int2r((jint *)&from, to, pos); }
  static inline void put_long(jlong *from, intptr_t *to, int& pos) { put_int2r((jint *) from, to, pos); }
#endif // PPC64

  // Oops are stored in native format in one JavaCallArgument slot at *to.
  static inline void put_obj(oop  from, intptr_t *to)           { *(oop *)(to +   0  ) =  from; }
  static inline void put_obj(oop  from, intptr_t *to, int& pos) { *(oop *)(to + pos++) =  from; }
  static inline void put_obj(oop *from, intptr_t *to, int& pos) { *(oop *)(to + pos++) = *from; }

  // Floats are stored in native format in one JavaCallArgument slot at *to.
  static inline void put_float(jfloat  from, intptr_t *to)           { *(jfloat *)(to +   0  ) =  from;  }
  static inline void put_float(jfloat  from, intptr_t *to, int& pos) { *(jfloat *)(to + pos++) =  from; }
  static inline void put_float(jfloat *from, intptr_t *to, int& pos) { *(jfloat *)(to + pos++) = *from; }

#undef _JNI_SLOT_OFFSET
#ifdef PPC64
#define _JNI_SLOT_OFFSET 1
  // Doubles are stored in native word format in one JavaCallArgument
  // slot at *(to+1).
  static inline void put_double(jdouble  from, intptr_t *to) {
    *(jdouble*) (to + 1) = from;
  }

  static inline void put_double(jdouble  from, intptr_t *to, int& pos) {
    *(jdouble*) (to + 1 + pos) = from;
    pos += 2;
  }

  static inline void put_double(jdouble *from, intptr_t *to, int& pos) {
    *(jdouble*) (to + 1 + pos) = *from;
    pos += 2;
  }
#else
#define _JNI_SLOT_OFFSET 0
  // Doubles are stored in reversed native word format in two JavaCallArgument slots at *to.
  static inline void    put_double(jdouble  from, intptr_t *to)           { put_int2r((jint *)&from, to); }
  static inline void    put_double(jdouble  from, intptr_t *to, int& pos) { put_int2r((jint *)&from, to, pos); }
  static inline void    put_double(jdouble *from, intptr_t *to, int& pos) { put_int2r((jint *) from, to, pos); }
#endif // PPC64

  // The get_xxx routines, on the other hand, actually _do_ fetch
  // java primitive types from the interpreter stack.
  // Values on stack in proper order, not reversing.
  // FIXME Need to check alignment on PPC32
  static inline jint    get_int   (intptr_t *from) { return *(jint *)    from; }
  static inline jlong   get_long  (intptr_t *from) { return *(jlong *)  (from + _JNI_SLOT_OFFSET); }
  static inline oop     get_obj   (intptr_t *from) { return *(oop *)     from; }
  static inline jfloat  get_float (intptr_t *from) { return *(jfloat *)  from; }
  static inline jdouble get_double(intptr_t *from) { return *(jdouble *)(from + _JNI_SLOT_OFFSET); }
};

#endif // CPU_PPC_VM_JNITYPES_PPC_HPP
