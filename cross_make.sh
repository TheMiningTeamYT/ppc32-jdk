#!/bin/sh
# Copyright 2013-2017 Azul Systems, Inc.  All Rights Reserved.
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#
# This code is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License version 2 only, as published by
# the Free Software Foundation.
#
# This code is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
# A PARTICULAR PURPOSE.  See the GNU General Public License version 2 for more
# details (a copy is included in the LICENSE file that accompanied this code).
#
# You should have received a copy of the GNU General Public License version 2
# along with this work; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
#
# Please contact Azul Systems, 385 Moffett Park Drive, Suite 115, Sunnyvale,
# CA 94089 USA or visit www.azul.com if you need additional information or
# have any questions.

set -v

CFLAGS="-marm -march=armv7-a --sysroot=$RASPI_ROOT"  \
CXXFLAGS="-marm -march=armv7-a --sysroot=$RASPI_ROOT"  \
LDFLAGS="--sysroot=$RASPI_ROOT"  \
BUILD_LD=gcc \
make \
    OBJCOPY=/opt/raspi/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/bin/arm-linux-gnueabihf-objcopy \
    STRIP=/opt/raspi/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/bin/arm-linux-gnueabihf-strip \
    POST_STRIP_CMD="/opt/raspi/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/bin/arm-linux-gnueabihf-strip -g" \
	BUILD_LD=gcc \
	"$@"
