/*
 * Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.
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

package sun.security.ssl;

import java.io.IOException;
import java.security.AccessController;
import java.security.AccessControlContext;
import java.security.Principal;
import java.security.PrivilegedAction;
import java.security.SecureRandom;
import java.lang.reflect.InvocationTargetException;
import sun.security.ssl.SSLHandshake.HandshakeMessage;

import java.nio.ByteBuffer;
/**
 * A helper class that calls the KerberosClientKeyExchange implementation.
 */
public class KerberosClientKeyExchange extends HandshakeMessage {

    private static final String IMPL_CLASS =
        "sun.security.ssl.krb5.KerberosClientKeyExchangeImpl";

    private static final Class<?> implClass = AccessController.doPrivileged(
            new PrivilegedAction<Class<?>>() {
                @Override
                public Class<?> run() {
                    try {
                        return Class.forName(IMPL_CLASS, true, null);
                    } catch (ClassNotFoundException cnf) {
                        return null;
                    }
                }
            }
        );
    private final KerberosClientKeyExchange impl;

    private KerberosClientKeyExchange createImpl(HandshakeContext context) {
        if (implClass != null &&
                getClass() == KerberosClientKeyExchange.class) {
            try {
                Class[] cArg = { HandshakeContext.class };
                return (KerberosClientKeyExchange)implClass.
                        getDeclaredConstructor(cArg).newInstance(context);
            } catch (InstantiationException | IllegalAccessException |
                    NoSuchMethodException | InvocationTargetException e) {
                throw new AssertionError(e);
            }
        }
        return null;
    }

    // This constructor will be called when constructing an instance of its
    // subclass -- KerberosClientKeyExchangeImpl.  Please won't check the
    // value of impl variable in this constructor.
    protected KerberosClientKeyExchange(HandshakeContext context) {
        super(context);
        impl = createImpl(context);
        // please won't check the value of impl variable
    }

    public KerberosClientKeyExchange(HandshakeContext context, String serverName,
        AccessControlContext acc, ProtocolVersion protocolVersion,
        SecureRandom rand) throws IOException {
        this(context);

        if (impl != null) {
            init(serverName, acc, protocolVersion, rand);
        } else {
            throw new IllegalStateException("Kerberos is unavailable");
        }
    }

    public KerberosClientKeyExchange(HandshakeContext context, ProtocolVersion protocolVersion,
            ProtocolVersion clientVersion, SecureRandom rand,
            ByteBuffer input, AccessControlContext acc,
            Object serverKeys) throws IOException {
        this(context);

        if (impl != null) {
            init(protocolVersion, clientVersion, rand, input, acc, serverKeys);
        } else {
            throw new IllegalStateException("Kerberos is unavailable");
        }
    }

    @Override
    SSLHandshake handshakeType() {
        return SSLHandshake.CLIENT_KEY_EXCHANGE;
    }

    @Override
    public int messageLength() {
        return impl.messageLength();
    }

    @Override
    public void send(HandshakeOutStream s) throws IOException {
        impl.send(s);
    }

    public void print() throws IOException {
        impl.print();
    }

    public void init(String serverName,
        AccessControlContext acc, ProtocolVersion protocolVersion,
        SecureRandom rand) throws IOException {

        if (impl != null) {
            impl.init(serverName, acc, protocolVersion, rand);
        }
    }

    public void init(ProtocolVersion protocolVersion,
            ProtocolVersion clientVersion, SecureRandom rand,
            ByteBuffer input, AccessControlContext acc,
            Object serviceCreds) throws IOException {

        if (impl != null) {
            impl.init(protocolVersion, clientVersion,
                                    rand, input, acc, serviceCreds);
        }
    }

    public byte[] getUnencryptedPreMasterSecret() {
        return impl.getUnencryptedPreMasterSecret();
    }

    public Principal getPeerPrincipal(){
        return impl.getPeerPrincipal();
    }

    public Principal getLocalPrincipal(){
        return impl.getLocalPrincipal();
    }
}
