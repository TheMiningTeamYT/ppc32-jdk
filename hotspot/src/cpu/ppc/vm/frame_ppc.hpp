/*
 * Copyright (c) 2000, 2015, Oracle and/or its affiliates. All rights reserved.
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

#ifndef CPU_PPC_VM_FRAME_PPC_HPP
#define CPU_PPC_VM_FRAME_PPC_HPP

#include "runtime/synchronizer.hpp"
#include "utilities/top.hpp"

  //  C frame layout on PPC-64.
  //
  //  In this figure the stack grows upwards, while memory grows
  //  downwards. See "64-bit PowerPC ELF ABI Supplement Version 1.7",
  //  IBM Corp. (2003-10-29)
  //  (http://math-atlas.sourceforge.net/devel/assembly/PPC-elf64abi-1.7.pdf).
  //
  //  Square brackets denote stack regions possibly larger
  //  than a single 64 bit slot.
  //
  //  STACK:
  //    0       [C_FRAME]               <-- SP after prolog (mod 16 = 0)
  //            [C_FRAME]               <-- SP before prolog
  //            ...
  //            [C_FRAME]
  //
  //  C_FRAME:
  //    0       [ABI_REG_ARGS]
  //    112     CARG_9: outgoing arg 9 (arg_1 ... arg_8 via gpr_3 ... gpr_{10})
  //            ...
  //    40+M*8  CARG_M: outgoing arg M (M is the maximum of outgoing args taken over all call sites in the procedure)
  //            local 1
  //            ...
  //            local N
  //            spill slot for vector reg (16 bytes aligned)
  //            ...
  //            spill slot for vector reg
  //            alignment       (4 or 12 bytes)
  //    V       SR_VRSAVE
  //    V+4     spill slot for GR
  //    ...     ...
  //            spill slot for GR
  //            spill slot for FR
  //            ...
  //            spill slot for FR
  //
  //  ABI_48:
  //    0       caller's SP
  //    8       space for condition register (CR) for next call
  //    16      space for link register (LR) for next call
  //    24      reserved
  //    32      reserved
  //    40      space for TOC (=R2) register for next call
  //
  //  ABI_REG_ARGS:
  //    0       [ABI_48]
  //    48      CARG_1: spill slot for outgoing arg 1. used by next callee.
  //    ...     ...
  //    104     CARG_8: spill slot for outgoing arg 8. used by next callee.
  //

 public:

  // C frame layout

  enum {
    // stack alignment
    alignment_in_bytes = 16,
    // log_2(16*8 bits) = 7.
    log_2_of_alignment_in_bits = 7
  };

  // ABI_MINFRAME:
  struct abi_minframe {
#if defined(PPC64)
    uintptr_t callers_sp; // 1
    uintptr_t cr;         // 2
    uintptr_t lr;         // 3
#if !defined(ABI_ELFv2)
    uintptr_t reserved1;  // 4
    uintptr_t reserved2;  // 5
#endif // ABI_ELFv2
    uintptr_t toc;        // 6 or 4
    // nothing to add here!
    // aligned to frame::alignment_in_bytes (16): (6 or 4)*8
#else // PPC64
    /* PPC32: frame map on v4 ABI is next
       (http://ftp.zut.edu.pl/dsk0/sources.redhat.com/binutils/ppc-docs/ppc-eabi-calling-sequence)
         SP---->+---------------------------------------+
                | back chain to caller                  | 0
                +---------------------------------------+
                | saved LR                              | 4
                +---------------------------------------+
                | Parameter save area (P)               | 8
                +---------------------------------------+
                | Alloca space (A)                      | 8+P
                +---------------------------------------+
                | Local variable space (L)              | 8+P+A
                +---------------------------------------+
                | saved CR (C)                          | 8+P+A+L
                +---------------------------------------+
                | Save area for GP registers (G)        | 8+P+A+L+C
                +---------------------------------------+
                | Save area for FP registers (F)        | 8+P+A+L+C+G
                +---------------------------------------+
        old SP->| back chain to caller's caller         |
                +---------------------------------------+

     note that nonvolatile fields of CR are not used by PPC32 port so
     it's not saved since it's optional in the PPC32 ABI
    */

    uintptr_t callers_sp; // 1
    uintptr_t lr;         // 2
    uintptr_t pad1;       // 3
    uintptr_t pad2;       // 4
    // aligned to frame::alignment_in_bytes (16): 4*4
    // note that pad1 and pad2 fields belong to F, G, C or L space
    // so the frame size can be less than sizeof(minframe)+sizeof(locals)
#endif // PPC64
  };

  enum {
    abi_minframe_size = sizeof(abi_minframe)
  };

  struct abi_reg_args : abi_minframe {
    // PPC64 API requires to mirror the parameter values into callee stack frame. PPC32 does not
#ifdef PPC64
    uintptr_t carg_1; // 1
    uintptr_t carg_2; // 2
    uintptr_t carg_3; // 3
    uintptr_t carg_4; // 4
    uintptr_t carg_5; // 5
    uintptr_t carg_6; // 6
    uintptr_t carg_7; // 7
    uintptr_t carg_8; // 8
    // aligned to frame::alignment_in_bytes (16): 8*8 or 8*4
#endif
  };

  enum {
    abi_reg_args_size = sizeof(abi_reg_args)
  };

  #define _abi(_component) \
          (offset_of(frame::abi_reg_args, _component))

  struct abi_reg_args_spill : abi_reg_args {
    // additional spill slots
#ifdef PPC64
    uint64_t spill_ret1; // 8
    double   spill_fret; // 16
#else // PPC64
#ifndef USE_SPE
    uint32_t spill_ret1; // 4
    uint32_t spill_ret2; // 8
    double   spill_fret; // 16
#else // USE_SPE
    uint64_t spill_ret1; // 8
    uint32_t spill_ret2; // 12
    uint32_t pad;        // 16
#endif // USE_SPE
#endif // PPC64
    // aligned to frame::alignment_in_bytes (16)
  };

  enum {
    abi_reg_args_spill_size = sizeof(abi_reg_args_spill)
  };

  #define _abi_reg_args_spill(_component) \
          (offset_of(frame::abi_reg_args_spill, _component))

  // non-volatile GPRs:

  struct spill_nonvolatiles {
    uintptr_t r14; // 1
    uintptr_t r15; // 2
    uintptr_t r16; // 3
    uintptr_t r17; // 4
    uintptr_t r18; // 5
    uintptr_t r19; // 6
    uintptr_t r20; // 7
    uintptr_t r21; // 8
    uintptr_t r22; // 9
    uintptr_t r23; // 10
    uintptr_t r24; // 11
    uintptr_t r25; // 12
    uintptr_t r26; // 13
    uintptr_t r27; // 14
    uintptr_t r28; // 15
    uintptr_t r29; // 16
    uintptr_t r30; // 17
    uintptr_t r31; // 18
#ifndef PPC64
    uintptr_t pad1; // 19
    uintptr_t pad2; // 20
#endif
    // aligned to frame::alignment_in_bytes (16): 18*8 or 20*4

    double f14;
    double f15;
    double f16;
    double f17;
    double f18;
    double f19;
    double f20;
    double f21;
    double f22;
    double f23;
    double f24;
    double f25;
    double f26;
    double f27;
    double f28;
    double f29;
    double f30;
    double f31;

    // aligned to frame::alignment_in_bytes (16)
  };

  enum {
    spill_nonvolatiles_size = sizeof(spill_nonvolatiles)
  };

  #define _spill_nonvolatiles_neg(_component) \
     (int)(-frame::spill_nonvolatiles_size + offset_of(frame::spill_nonvolatiles, _component))



#ifndef CC_INTERP
  //  Frame layout for the Java template interpreter on PPC64.
  //
  //  Diffs to the CC_INTERP are marked with 'X'.
  //
  //  TOP_IJAVA_FRAME:
  //
  //    0       [TOP_IJAVA_FRAME_ABI]
  //            alignment (optional)
  //            [operand stack]
  //            [monitors] (optional)
  //           X[IJAVA_STATE]
  //            note: own locals are located in the caller frame.
  //
  //  PARENT_IJAVA_FRAME:
  //
  //    0       [PARENT_IJAVA_FRAME_ABI]
  //            alignment (optional)
  //            [callee's Java result]
  //            [callee's locals w/o arguments]
  //            [outgoing arguments]
  //            [used part of operand stack w/o arguments]
  //            [monitors]      (optional)
  //           X[IJAVA_STATE]
  //

  struct parent_ijava_frame_abi : abi_minframe {
  };

  enum {
    parent_ijava_frame_abi_size = sizeof(parent_ijava_frame_abi)
  };

#define _parent_ijava_frame_abi(_component) \
        (offset_of(frame::parent_ijava_frame_abi, _component))

  struct top_ijava_frame_abi : abi_reg_args {
  };

  enum {
    top_ijava_frame_abi_size = sizeof(top_ijava_frame_abi)
  };

#define _top_ijava_frame_abi(_component) \
        (offset_of(frame::top_ijava_frame_abi, _component))

  struct ijava_state {
#ifdef ASSERT
    uintptr_t ijava_reserved;  // Used for assertion.
    uintptr_t ijava_reserved2; // Inserted for alignment.
#ifndef PPC64
    uintptr_t ijava_reserved3; // Inserted for alignment.
    uintptr_t ijava_reserved4; // Inserted for alignment.
#endif
#endif
    uintptr_t method;       // 1
    uintptr_t locals;       // 2
    uintptr_t monitors;     // 3
    uintptr_t cpoolCache;   // 4
    uintptr_t bcp;          // 5
    uintptr_t esp;          // 6
    uintptr_t mdx;          // 7
    uintptr_t top_frame_sp; // 8, maybe define parent_frame_abi and move there.
    uintptr_t sender_sp;    // 9
    // Slots only needed for native calls. Maybe better to move elsewhere.
    uintptr_t oop_tmp;      // 10
#ifdef PPC64
    uintptr_t lresult;      // 11
    uintptr_t fresult;      // 12
    // Aligned to frame::alignment_in_bytes (16): 12*8
#else
    uintptr_t pad1;         // 11
    uintptr_t pad2;         // 12
    uint64_t  lresult;      // 13-14
#ifndef USE_SPE
    double    fresult;      // 15-16
#else
    uint64_t  pad3;         // 15-16
#endif

    // Aligned to frame::alignment_in_bytes (16): 16*4
#endif

  };

  enum {
    ijava_state_size = sizeof(ijava_state)
  };

#define _ijava_state_neg(_component) \
        (int) (-frame::ijava_state_size + offset_of(frame::ijava_state, _component))

#else // CC_INTERP:

  //  Frame layout for the Java C++ interpreter on PPC64.
  //
  //  This frame layout provides a C-like frame for every Java frame.
  //
  //  In these figures the stack grows upwards, while memory grows
  //  downwards. Square brackets denote regions possibly larger than
  //  single 64 bit slots.
  //
  //  STACK (no JNI, no compiled code, no library calls,
  //         interpreter-loop is active):
  //    0       [InterpretMethod]
  //            [TOP_IJAVA_FRAME]
  //            [PARENT_IJAVA_FRAME]
  //            ...
  //            [PARENT_IJAVA_FRAME]
  //            [ENTRY_FRAME]
  //            [C_FRAME]
  //            ...
  //            [C_FRAME]
  //
  //  TOP_IJAVA_FRAME:
  //    0       [TOP_IJAVA_FRAME_ABI]
  //            alignment (optional)
  //            [operand stack]
  //            [monitors] (optional)
  //            [cInterpreter object]
  //            result, locals, and arguments are in parent frame!
  //
  //  PARENT_IJAVA_FRAME:
  //    0       [PARENT_IJAVA_FRAME_ABI]
  //            alignment (optional)
  //            [callee's Java result]
  //            [callee's locals w/o arguments]
  //            [outgoing arguments]
  //            [used part of operand stack w/o arguments]
  //            [monitors] (optional)
  //            [cInterpreter object]
  //
  //  ENTRY_FRAME:
  //    0       [PARENT_IJAVA_FRAME_ABI]
  //            alignment (optional)
  //            [callee's Java result]
  //            [callee's locals w/o arguments]
  //            [outgoing arguments]
  //            [ENTRY_FRAME_LOCALS]
  //
  //  PARENT_IJAVA_FRAME_ABI:
  //    0       [ABI_MINFRAME]
  //            top_frame_sp
  //            initial_caller_sp
  //
  //  TOP_IJAVA_FRAME_ABI:
  //    0       [PARENT_IJAVA_FRAME_ABI]
  //            carg_3_unused
  //            carg_4_unused
  //            carg_5_unused
  //            carg_6_unused
  //            carg_7_unused
  //            frame_manager_lr
  //

  // PARENT_IJAVA_FRAME_ABI

  struct parent_ijava_frame_abi : abi_minframe {
    // SOE registers.
    // C2i adapters spill their top-frame stack-pointer here.
    uintptr_t top_frame_sp;      // 1, carg_1
    // Sp of calling compiled frame before it was resized by the c2i
    // adapter or sp of call stub. Does not contain a valid value for
    // non-initial frames.
    uintptr_t initial_caller_sp; // 2
    uintptr_t carg_3_unused;     // 3
    uintptr_t carg_4_unused;     // 4
    // aligned to frame::alignment_in_bytes (16)
  };

  enum {
    parent_ijava_frame_abi_size = sizeof(parent_ijava_frame_abi)
  };

  #define _parent_ijava_frame_abi(_component) \
          (offset_of(frame::parent_ijava_frame_abi, _component))

  // TOP_IJAVA_FRAME_ABI

  struct top_ijava_frame_abi : parent_ijava_frame_abi {
    uintptr_t carg_5_unused;    // 1
    uintptr_t carg_6_unused;    // 2
    uintptr_t carg_7_unused;    // 3
    // Use arg8 for storing frame_manager_lr. The size of
    // top_ijava_frame_abi must match abi_reg_args.
    uintptr_t frame_manager_lr; // 4
    // nothing to add here!
    // aligned to frame::alignment_in_bytes (16)
  };

  enum {
    top_ijava_frame_abi_size = sizeof(top_ijava_frame_abi)
  };

  #define _top_ijava_frame_abi(_component) \
          (offset_of(frame::top_ijava_frame_abi, _component))

#endif // CC_INTERP

  // ENTRY_FRAME

  struct entry_frame_locals {
    uintptr_t call_wrapper_address;  // 1
    uintptr_t result_address;        // 2
    uintptr_t result_type;           // 3
    uintptr_t arguments_tos_address; // 4
    // aligned to frame::alignment_in_bytes (16): 4*8 or 4*4
    uintptr_t r[spill_nonvolatiles_size/sizeof(uintptr_t)];
  };

  enum {
    entry_frame_locals_size = sizeof(entry_frame_locals)
  };

  #define _entry_frame_locals_neg(_component) \
    (int)(-frame::entry_frame_locals_size + offset_of(frame::entry_frame_locals, _component))


  //  Frame layout for JIT generated methods
  //
  //  In these figures the stack grows upwards, while memory grows
  //  downwards. Square brackets denote regions possibly larger than single
  //  64 bit slots.
  //
  //  STACK (interpreted Java calls JIT generated Java):
  //          [JIT_FRAME]                                <-- SP (mod 16 = 0)
  //          [TOP_IJAVA_FRAME]
  //         ...
  //
  //  JIT_FRAME (is a C frame according to PPC-64 ABI):
  //          [out_preserve]
  //          [out_args]
  //          [spills]
  //          [pad_1]
  //          [monitor] (optional)
  //       ...
  //          [monitor] (optional)
  //          [pad_2]
  //          [in_preserve] added / removed by prolog / epilog
  //

  // JIT_ABI (TOP and PARENT)

#if defined(PPC64)
  struct jit_abi {
    uintptr_t callers_sp; // 1
    uintptr_t cr;         // 2
    uintptr_t lr;         // 3
    uintptr_t toc;        // 4
    // Nothing to add here!
    // NOT ALIGNED to frame::alignment_in_bytes (16).
  };
#else
  struct jit_abi {
    uintptr_t callers_sp; // 1
    uintptr_t lr;         // 2
  };
#endif

  struct jit_out_preserve : jit_abi {
    // Nothing to add here!
  };

  struct jit_in_preserve {
    // Nothing to add here!
  };

  enum {
    jit_out_preserve_size = sizeof(jit_out_preserve),
    jit_in_preserve_size  = sizeof(jit_in_preserve)
  };

  struct jit_monitor {
    uintptr_t monitor[1];
  };

  enum {
    jit_monitor_size = sizeof(jit_monitor),
  };

 private:

  //  STACK:
  //            ...
  //            [THIS_FRAME]             <-- this._sp (stack pointer for this frame)
  //            [CALLER_FRAME]           <-- this.fp() (_sp of caller's frame)
  //            ...
  //

  // frame pointer for this frame
  intptr_t* _fp;

  // The frame's stack pointer before it has been extended by a c2i adapter;
  // needed by deoptimization
  intptr_t* _unextended_sp;
  void adjust_unextended_sp();


 public:

  // Accessors for fields
  intptr_t* fp() const { return _fp; }

  // Accessors for ABIs
  inline abi_minframe* own_abi()     const { return (abi_minframe*) _sp; }
  inline abi_minframe* callers_abi() const { return (abi_minframe*) _fp; }

 private:

  // Find codeblob and set deopt_state.
  inline void find_codeblob_and_set_pc_and_deopt_state(address pc);

 public:

  // Constructors
  inline frame(intptr_t* sp);
  frame(intptr_t* sp, address pc);
  inline frame(intptr_t* sp, address pc, intptr_t* unextended_sp);

 private:

  intptr_t* compiled_sender_sp(CodeBlob* cb) const;
  address*  compiled_sender_pc_addr(CodeBlob* cb) const;
  address*  sender_pc_addr(void) const;

 public:

#ifdef CC_INTERP
  // Additional interface for interpreter frames:
  inline interpreterState get_interpreterState() const;
#else
  inline ijava_state* get_ijava_state() const;
  // Some convenient register frame setters/getters for deoptimization.
  inline intptr_t* interpreter_frame_esp() const;
  inline void interpreter_frame_set_cpcache(ConstantPoolCache* cp);
  inline void interpreter_frame_set_esp(intptr_t* esp);
  inline void interpreter_frame_set_top_frame_sp(intptr_t* top_frame_sp);
  inline void interpreter_frame_set_sender_sp(intptr_t* sender_sp);
#endif // CC_INTERP

  // Size of a monitor in bytes.
  static int interpreter_frame_monitor_size_in_bytes();

  // The size of a cInterpreter object.
  static inline int interpreter_frame_cinterpreterstate_size_in_bytes();

 private:

  ConstantPoolCache** interpreter_frame_cpoolcache_addr() const;

 public:

  // Additional interface for entry frames:
  inline entry_frame_locals* get_entry_frame_locals() const {
    return (entry_frame_locals*) (((address) fp()) - entry_frame_locals_size);
  }

  enum {
    // normal return address is 1 bundle past PC
    pc_return_offset = 0
  };

#endif // CPU_PPC_VM_FRAME_PPC_HPP
