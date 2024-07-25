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

LDFLAGS="--sysroot=$RASPI_ROOT"  \
CFLAGS="-marm -march=armv7-a --sysroot=$RASPI_ROOT"  \
CXXFLAGS="-marm -march=armv7-a --sysroot=$RASPI_ROOT"  \
PKG_CONFIG_PATH=$PKG_CONFIG_PATH:"$RASPI_ROOT/usr/lib/arm-linux-gnueabihf/pkgconfig"  \
PKG_CONFIG=$PWD/cross-pkg-config \
SYSROOT=$RASPI_ROOT \
bash configure  \
	CC=arm-linux-gnueabihf-gcc  \
	CXX=arm-linux-gnueabihf-g++  \
	BUILD_CC=gcc  \
	BUILD_LD=gcc  \
	--with-sys-root=$RASPI_ROOT \
	--with-freetype-include=$RASPI_ROOT/usr/include/freetype2/  \
	--with-freetype-lib=$RASPI_ROOT/usr/lib/arm-linux-gnueabihf  \
	--x-includes=$RASPI_ROOT/usr/include/  \
	--x-libraries=$RASPI_ROOT/usr/lib  \
	--disable-precompiled-headers \
	--with-extra-cflags=--sysroot=$RASPI_ROOT \
	--with-extra-cxxflags=--sysroot=$RASPI_ROOT \
	--with-extra-ldflags=--sysroot=$RASPI_ROOT \
	--openjdk-target=aarch32-linux-gnueabihf \
	"$@"

