/*
 * Copyright (c) 1997, 2013, Oracle and/or its affiliates. All rights reserved.
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

#include "precompiled.hpp"
#include "asm/macroAssembler.inline.hpp"
#include "asm/codeBuffer.hpp"
#include "memory/metaspaceShared.hpp"
#include "register_ppc.hpp"
#include "macroAssembler_ppc.hpp"

// Generate the self-patching vtable method:
//
// This method will be called (as any other Klass virtual method) with
// the Klass itself as the first argument.  Example:
//
//   oop obj;
//   int size = obj->klass()->klass_part()->oop_size(this);
//
// for which the virtual method call is Klass::oop_size();
//
// The dummy method is called with the Klass object as the first
// operand, and an object as the second argument.
//

//=====================================================================

// All of the dummy methods in the vtable are essentially identical,
// differing only by an ordinal constant, and they bear no releationship
// to the original method which the caller intended. Also, there needs
// to be 'vtbl_list_size' instances of the vtable in order to
// differentiate between the 'vtable_list_size' original Klass objects.

#define __ masm->

// Function Descriptor for ELFv1 defined in the next way:
//
//  struct {
//    void *(func_ptr)();   /* the address of the function */
//    void *toc_value;      /* TOC value for the function  */
//    void *env;            /* environment pointer         */
//  }
//
// That is requiring to save 3 words for one function descriptor instead of 1 as at other
// platforms/ABIs. Additionally, there is an extra level of indirection as shown below
//
//     object             vtable     function descriptors array
//  [ vtable ptr ]---->[ fd ptr #1 ]---->[ method #1 ptr ]
//  [ .......... ]     [ fd ptr #2 ].    [ toc  value    ]
//  [ .......... ]     [ ......... ]  \  [ env. value    ]
//  [ .......... ]     [ ......... ]    >[ method #2 ptr ]
//  [ .......... ]     [ ......... ]     [ toc  value    ]
//  [ .......... ]     [ ......... ]     [ env. value    ]
//  [ .......... ]     [ fd ptr #n ]     [ ............. ]
//

void MetaspaceShared::generate_vtable_methods(void** vtbl_list,
                                              void** vtable,
                                              char** md_top,
                                              char* md_end,
                                              char** mc_top,
                                              char* mc_end) {

  intptr_t vtable_bytes = (num_virtuals * vtbl_list_size) * sizeof(void*);
  *(intptr_t *)(*md_top) = vtable_bytes;
  *md_top += sizeof(intptr_t);
  void** dummy_vtable = (void**)*md_top;
  *vtable = dummy_vtable;
  *md_top += vtable_bytes;

#if defined(PPC64) && !defined(ABI_ELFv2) // ABI_ELFv1
  // Creating extra fd table in code block (not in data one) to prevent asserts in shared code
  const int fd_size_in_words = 3;
  intptr_t vtable_elfv1_bytes = (num_virtuals * vtbl_list_size) * fd_size_in_words * sizeof(void*);
  void** dummy_vtable_elfv1 = (void**)*mc_top;
  *mc_top += vtable_elfv1_bytes;
#endif

  // Get ready to generate dummy methods.

  CodeBuffer cb((unsigned char*)*mc_top, mc_end - *mc_top);
  MacroAssembler* masm = new MacroAssembler(&cb);

  Label common_code;
  const int shift_const = 8;
#if defined(PPC64) && !defined(ABI_ELFv2) // ABI_ELFv1
  for (int i = 0; i < vtbl_list_size; ++i) {
    for (int j = 0; j < num_virtuals; ++j) {
      dummy_vtable[num_virtuals * i + j] = &dummy_vtable_elfv1[(num_virtuals * i + j)*fd_size_in_words];
    }
  }
#endif
  for (int i = 0; i < vtbl_list_size; ++i) {
    for (int j = 0; j < num_virtuals; ++j) {
#if !defined(PPC64) || defined(ABI_ELFv2)
      dummy_vtable[num_virtuals * i + j] = (void*)masm->pc();
#else // ABI_ELFv1
      dummy_vtable_elfv1[(num_virtuals * i + j)*fd_size_in_words] = (void*)masm->pc(); // function pointer
      dummy_vtable_elfv1[(num_virtuals * i + j)*fd_size_in_words+1] = 0;               // TOC
      dummy_vtable_elfv1[(num_virtuals * i + j)*fd_size_in_words+2] = 0;               // Environment
#endif

      // Load R0, with a value indicating vtable/offset pair.
      // -- bits[ 7..0]  (8 bits) which virtual method in table?
      // -- bits[12..8]  (5 bits) which virtual method table?
      __ li(R0, (i << shift_const) + j);
      __ b(common_code);
    }
  }

  __ bind(common_code);

  // Expecting to be called with the "this" pointer in R3 (where "this" is
  // a klass object). In addition, R0 was set (above) to indentify the method
  // and table.

  Register r_tmp1, r_tmp2, r_mptr;

  r_mptr = R0;
  r_tmp1 = R4;
  r_tmp2 = R5;

  __ st(R4, -BytesPerWord, R1_SP);                               // save prev values of tmp regs
  __ st(R5, -2*BytesPerWord, R1_SP);

  __ load_const_optimized(r_tmp2, (intptr_t)vtbl_list, r_tmp1);

  __ srwi(r_tmp1, R0, shift_const);                              // isolate vtable identifier.
  __ slwi(r_tmp1, r_tmp1, LogBytesPerWord);

  __ lx(r_tmp1, r_tmp2, r_tmp1);                                 // get correct vtable address.

  __ st(r_tmp1, 0, R3_ARG1);                                     // update vtable pointer.

  __ andi(r_mptr, R0, (1<<shift_const) - 1 /* 0x00ff */);        // isolate vtable method index
  __ slwi(r_mptr, r_mptr, LogBytesPerWord);                      // *= sizeof(void*)

  __ add(r_tmp2, r_tmp1, r_mptr);                                // address of real method pointer.

#if defined(PPC64) && !defined(ABI_ELFv2) // ABI_ELFv1
  __ l(r_tmp2, 0, r_tmp2);                                       // load function descriptor pointer
  __ l(R2 /* RTOC */,       BytesPerWord, r_tmp2);               // restore RTOC
  __ l(R11/* ENV reg. */, 2*BytesPerWord, r_tmp2);               // load environment
#endif
  __ l(r_mptr, 0, r_tmp2);                                       // get real method pointer.

  __ l(R4, -BytesPerWord, R1_SP);                                // restore previous value of tmp regs
  __ l(R5, -2*BytesPerWord, R1_SP);

  __ mtctr(r_mptr);                                              // jump to real method pointer.
  __ bctr();

  __ flush();
  *mc_top = (char*)__ pc();

  guarantee(*mc_top <= mc_end, "Insufficient space for method wrappers.");
}

