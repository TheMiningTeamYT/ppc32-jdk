/*
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
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
 */
// This file is a derivative work resulting from (and including) modifications
// made by Azul Systems, Inc.  The dates of such changes are 2013-2017.
// Copyright 2013-2018 Azul Systems, Inc.  All Rights Reserved.
//
// Please contact Azul Systems, 385 Moffett Park Drive, Suite 115, Sunnyvale,
// CA 94089 USA or visit www.azul.com if you need additional information or
// have any questions.

#include <netinet/in.h>
#include <netinet/tcp.h>
#include "jni.h"

#define INIT_CONST(ENV, CLS, VAL)                                           \
{                                                                           \
    jfieldID fID = (*(ENV))->GetStaticFieldID((ENV), (CLS), #VAL, "I");     \
    if(fID != 0) {                                                          \
        (*(ENV))->SetStaticIntField((ENV), (CLS), fID, (VAL));              \
    }                                                                       \
}                                                                           \

/**
 * Initialization of the SocketOptionRegistry fields: Socket options
 */
JNIEXPORT void JNICALL
Java_sun_nio_ch_SocketOptionRegistry_init(JNIEnv* env, jclass cls) {
    // levels
    INIT_CONST(env, cls, SOL_SOCKET);
    INIT_CONST(env, cls, IPPROTO_TCP);
    INIT_CONST(env, cls, IPPROTO_IP);
#ifdef AF_INET6
    INIT_CONST(env, cls, IPPROTO_IPV6);
#endif

    // options
    INIT_CONST(env, cls, SO_BROADCAST);
    INIT_CONST(env, cls, SO_KEEPALIVE);
    INIT_CONST(env, cls, SO_LINGER);
    INIT_CONST(env, cls, SO_SNDBUF);
    INIT_CONST(env, cls, SO_RCVBUF);
    INIT_CONST(env, cls, SO_REUSEADDR);
    INIT_CONST(env, cls, SO_OOBINLINE);

    INIT_CONST(env, cls, TCP_NODELAY);

    INIT_CONST(env, cls, IP_TOS);
    INIT_CONST(env, cls, IP_MULTICAST_IF);
    INIT_CONST(env, cls, IP_MULTICAST_TTL);
    INIT_CONST(env, cls, IP_MULTICAST_LOOP);

#ifdef AF_INET6
    INIT_CONST(env, cls, IPV6_TCLASS);
    INIT_CONST(env, cls, IPV6_MULTICAST_IF);
    INIT_CONST(env, cls, IPV6_MULTICAST_HOPS);
    INIT_CONST(env, cls, IPV6_MULTICAST_LOOP);
#endif
}

