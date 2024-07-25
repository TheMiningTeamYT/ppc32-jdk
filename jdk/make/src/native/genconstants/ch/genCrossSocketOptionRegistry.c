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

#include <stdio.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif

/**
 * Generates sun.nio.ch.SocketOptionRegistry, a class that maps Java-level
 * socket options to the platform specific level and option.
 */

static const char* sonames[]={
    // level
    "SOL_SOCKET",
    "IPPROTO_TCP",
    "IPPROTO_IP",
    "IPPROTO_IPV6",
    // options
    "SO_BROADCAST",
    "SO_KEEPALIVE",
    "SO_LINGER",
    "SO_SNDBUF",
    "SO_RCVBUF",
    "SO_REUSEADDR",
    "SO_OOBINLINE",
    "TCP_NODELAY",
    "IP_TOS",
    "IP_MULTICAST_IF",
    "IP_MULTICAST_TTL",
    "IP_MULTICAST_LOOP",
    "IPV6_TCLASS",
    "IPV6_MULTICAST_IF",
    "IPV6_MULTICAST_HOPS",
    "IPV6_MULTICAST_LOOP"
};

static void out(const char* s) {
    printf("%s\n", s);
}

static void declTemp(const char* name) {
    printf("    private static int %s=0;\n",name);
}

static void init() {
    out("    private static native void init();");
    out("    static {");
    out("        AccessController.doPrivileged(new PrivilegedAction<Void>() {");
    out("            public Void run() {");
    out("                System.loadLibrary(\"nio\");");
    out("                return null;");
    out("        }});");
    out("        init();");
    out("    }");
}

static void emit(const char *name, char * family, const char *level, const char *optname) {
    printf("            map.put(new RegistryKey(%s, %s),", name, family);
    printf(" new OptionKey(%s, %s));\n", level, optname);
}

static void emit_unspec(const char *name, const char *level, const char *optname) {
    emit(name, "Net.UNSPEC", level, optname);
}

static  void emit_inet(const char *name, const char *level, const char *optname) {
    emit(name, "StandardProtocolFamily.INET", level, optname);
}

static void emit_inet6(const char *name, const char *level, const char *optname) {
    emit(name, "StandardProtocolFamily.INET6", level, optname);
}

int main(int argc, const char* argv[]) {
    int i;
    out("// AUTOMATICALLY GENERATED FILE - DO NOT EDIT                                  ");
    out("package sun.nio.ch;                                                            ");
    out("import java.net.SocketOption;                                                  ");
    out("import java.net.StandardSocketOptions;                                         ");
    out("import java.net.ProtocolFamily;                                                ");
    out("import java.net.StandardProtocolFamily;                                        ");
    out("import java.util.Map;                                                          ");
    out("import java.util.HashMap;                                                      ");
    out("import java.security.AccessController;                                         ");
    out("import java.security.PrivilegedAction;                                         ");

    out("class SocketOptionRegistry {                                                   ");
    out("    private SocketOptionRegistry() { }                                         ");

    // define private intermediate constants
    for(i=0; i<(int)sizeof(sonames)/sizeof(sonames[0]);i++)
        declTemp(sonames[i]);

    init();

    out("    private static class RegistryKey {                                         ");
    out("        private final SocketOption<?> name;                                    ");
    out("        private final ProtocolFamily family;                                   ");
    out("        RegistryKey(SocketOption<?> name, ProtocolFamily family) {             ");
    out("            this.name = name;                                                  ");
    out("            this.family = family;                                              ");
    out("        }                                                                      ");
    out("        public int hashCode() {                                                ");
    out("            return name.hashCode() + family.hashCode();                        ");
    out("        }                                                                      ");
    out("        public boolean equals(Object ob) {                                     ");
    out("            if (ob == null) return false;                                      ");
    out("            if (!(ob instanceof RegistryKey)) return false;                    ");
    out("            RegistryKey other = (RegistryKey)ob;                               ");
    out("            if (this.name != other.name) return false;                         ");
    out("            if (this.family != other.family) return false;                     ");
    out("            return true;                                                       ");
    out("        }                                                                      ");
    out("    }                                                                          ");
    out("    private static class LazyInitialization {                                  ");
    out("        static final Map<RegistryKey,OptionKey> options = options();           ");
    out("        private static Map<RegistryKey,OptionKey> options() {                  ");
    out("            Map<RegistryKey,OptionKey> map =                                   ");
    out("                new HashMap<RegistryKey,OptionKey>();                          ");

    emit_unspec("StandardSocketOptions.SO_BROADCAST", "SOL_SOCKET", "SO_BROADCAST");
    emit_unspec("StandardSocketOptions.SO_KEEPALIVE", "SOL_SOCKET", "SO_KEEPALIVE");
    emit_unspec("StandardSocketOptions.SO_LINGER",    "SOL_SOCKET", "SO_LINGER");
    emit_unspec("StandardSocketOptions.SO_SNDBUF",    "SOL_SOCKET", "SO_SNDBUF");
    emit_unspec("StandardSocketOptions.SO_RCVBUF",    "SOL_SOCKET", "SO_RCVBUF");
    emit_unspec("StandardSocketOptions.SO_REUSEADDR", "SOL_SOCKET", "SO_REUSEADDR");
    emit_unspec("StandardSocketOptions.TCP_NODELAY",  "IPPROTO_TCP", "TCP_NODELAY");

    emit_inet("StandardSocketOptions.IP_TOS",            "IPPROTO_IP",     "IP_TOS");
    emit_inet("StandardSocketOptions.IP_MULTICAST_IF",   "IPPROTO_IP",     "IP_MULTICAST_IF");
    emit_inet("StandardSocketOptions.IP_MULTICAST_TTL",  "IPPROTO_IP",     "IP_MULTICAST_TTL");
    emit_inet("StandardSocketOptions.IP_MULTICAST_LOOP", "IPPROTO_IP",     "IP_MULTICAST_LOOP");

    emit_inet6("StandardSocketOptions.IP_TOS",            "IPPROTO_IPV6",  "IPV6_TCLASS");
    emit_inet6("StandardSocketOptions.IP_MULTICAST_IF",   "IPPROTO_IPV6",  "IPV6_MULTICAST_IF");
    emit_inet6("StandardSocketOptions.IP_MULTICAST_TTL",  "IPPROTO_IPV6",  "IPV6_MULTICAST_HOPS");
    emit_inet6("StandardSocketOptions.IP_MULTICAST_LOOP", "IPPROTO_IPV6",  "IPV6_MULTICAST_LOOP");

    emit_unspec("ExtendedSocketOption.SO_OOBINLINE", "SOL_SOCKET", "SO_OOBINLINE");

    out("            return map;                                                        ");
    out("        }                                                                      ");
    out("    }                                                                          ");
    out("    public static OptionKey findOption(SocketOption<?> name, ProtocolFamily family) { ");
    out("        RegistryKey key = new RegistryKey(name, family);                       ");
    out("        return LazyInitialization.options.get(key);                            ");
    out("    }                                                                          ");
    out("}                                                                              ");

    return 0;
}
