#
# Copyright (c) 2004, 2013, Oracle and/or its affiliates. All rights reserved.
# Copyright 2012, 2013 SAP AG. All rights reserved.
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#
# This code is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 only, as
# published by the Free Software Foundation.
#
# This code is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# version 2 for more details (a copy is included in the LICENSE file that
# accompanied this code).
#
# You should have received a copy of the GNU General Public License version
# 2 along with this work; if not, write to the Free Software Foundation,
# Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
#
# Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
# or visit www.oracle.com if you need additional information or have any
# questions.
#
#

ifeq ($(origin OPENJDK_TARGET_CPU_ENDIAN),undefined)
  # This can happen during hotspot standalone build. Set endianness from
  # uname. We assume build and target machines are the same.
  OPENJDK_TARGET_CPU_ENDIAN:=big
endif

ifeq ($(filter $(OPENJDK_TARGET_CPU_ENDIAN),big little),)
  $(error OPENJDK_TARGET_CPU_ENDIAN value should be 'big' or 'little')
endif
