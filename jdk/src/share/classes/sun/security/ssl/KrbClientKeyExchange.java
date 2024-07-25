/*
 * Copyright (c) 2020, Azul Systems, Inc. All rights reserved.
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
import java.nio.ByteBuffer;
import java.security.*;
import javax.crypto.SecretKey;
import javax.net.ssl.SNIHostName;
import javax.net.ssl.StandardConstants;
import sun.security.ssl.SSLHandshake.HandshakeMessage;

public class KrbClientKeyExchange {
    static final SSLConsumer krbHandshakeConsumer =
            new KrbClientKeyExchange.KrbClientKeyExchangeConsumer();
    static final HandshakeProducer krbHandshakeProducer =
            new KrbClientKeyExchange.KrbClientKeyExchangeProducer();

    /**
     * The KRB5 "ClientKeyExchange" handshake message producer.
     */
    private static final
    class KrbClientKeyExchangeProducer implements HandshakeProducer {
        // Prevent instantiation of this class.
        private KrbClientKeyExchangeProducer() {
            // blank
        }

        @Override
        public byte[] produce(ConnectionContext context,
                              HandshakeMessage message) throws IOException {
            ClientHandshakeContext chc = (ClientHandshakeContext)context;
            // This happens in client side only.
            KerberosClientKeyExchange kerberosMsg = null;
            String hostName = null;
            if (chc.negotiatedServerName != null) {
                // use first requested SNI hostname
                if (chc.negotiatedServerName.getType() == StandardConstants.SNI_HOST_NAME) {
                    SNIHostName sniHostName = null;
                    if (chc.negotiatedServerName instanceof SNIHostName) {
                        sniHostName = (SNIHostName) chc.negotiatedServerName;
                    } else {
                        try {
                            sniHostName = new SNIHostName(chc.negotiatedServerName.getEncoded());
                        } catch (IllegalArgumentException iae) {
                            // unlikely to happen, just in case ...
                            if (SSLLogger.isOn && SSLLogger.isOn("ssl,trustmanager")) {
                                SSLLogger.fine("Illegal server name: " + chc.negotiatedServerName);
                            }
                        }
                    }
                    if (sniHostName != null)
                        hostName = sniHostName.getAsciiName();
                }
            } else {
                hostName = chc.handshakeSession.getPeerHost();
            }
            try {
                kerberosMsg = new KerberosClientKeyExchange(chc,
                        hostName, chc.conContext.acc, chc.negotiatedProtocol,
                        chc.sslContext.getSecureRandom());
                KrbKeyExchange.KrbPremasterSecret premasterSecret =
                        new KrbKeyExchange.KrbPremasterSecret(kerberosMsg.getUnencryptedPreMasterSecret());
                chc.handshakePossessions.add(premasterSecret);
            } catch (IOException e) {
                // fallback to using hostname
                if (SSLLogger.isOn && SSLLogger.isOn("handshake")) {
                    SSLLogger.fine(
                            "Warning, cannot use Server Name Indication: "
                                    + e.getMessage());
                }
            }
            // Output the handshake message.
            kerberosMsg.write(chc.handshakeOutput);
            chc.handshakeOutput.flush();

            // Record the principals involved in exchange
            chc.handshakeSession.setPeerPrincipal(kerberosMsg.getPeerPrincipal());
            chc.handshakeSession.setLocalPrincipal(kerberosMsg.getLocalPrincipal());
            // update the states
            SSLKeyExchange ke = SSLKeyExchange.valueOf(
                    chc.negotiatedCipherSuite.keyExchange,
                    chc.negotiatedProtocol);
            if (ke == null) {
                // unlikely
                throw chc.conContext.fatal(Alert.INTERNAL_ERROR,
                        "Not supported key exchange type");
            } else {
                SSLKeyDerivation masterKD = ke.createKeyDerivation(chc);
                SecretKey masterSecret =
                        masterKD.deriveKey("MasterSecret", null);

                chc.handshakeSession.setMasterSecret(masterSecret);

                SSLTrafficKeyDerivation kd =
                        SSLTrafficKeyDerivation.valueOf(chc.negotiatedProtocol);
                if (kd == null) {
                    // unlikely
                    throw chc.conContext.fatal(Alert.INTERNAL_ERROR,
                            "Not supported key derivation: " +
                                    chc.negotiatedProtocol);
                } else {
                    chc.handshakeKeyDerivation =
                            kd.createKeyDerivation(chc, masterSecret);
                }
            }

            // The handshake message has been delivered.
            return null;
        }
    }


    /**
     * The Krb "ClientKeyExchange" handshake message consumer.
     */
    private static final
    class KrbClientKeyExchangeConsumer implements SSLConsumer {
        // Prevent instantiation of this class.
        private KrbClientKeyExchangeConsumer() {
            // blank
        }

        @Override
        public void consume(ConnectionContext context,
                            ByteBuffer message) throws IOException {
            // The consuming happens in server side only.
            ServerHandshakeContext shc = (ServerHandshakeContext)context;
            KerberosClientKeyExchange kerberosMsg =
                    new KerberosClientKeyExchange(shc,
                            shc.negotiatedProtocol,
                            ProtocolVersion.valueOf(shc.clientHelloVersion),
                            shc.sslContext.getSecureRandom(),
                            message,
                            shc.conContext.acc,
                            setupKerberosKeys(shc));
            shc.handshakeSession.setPeerPrincipal(kerberosMsg.getPeerPrincipal());
            shc.handshakeSession.setLocalPrincipal(kerberosMsg.getLocalPrincipal());
            KrbKeyExchange.KrbPremasterSecret premasterSecret =
                    new KrbKeyExchange.KrbPremasterSecret(kerberosMsg.getUnencryptedPreMasterSecret());
            shc.handshakeCredentials.add(premasterSecret);
            if (SSLLogger.isOn && SSLLogger.isOn("handshake")) {
                kerberosMsg.print();
            }

            // update the states
            SSLKeyExchange ke = SSLKeyExchange.valueOf(
                    shc.negotiatedCipherSuite.keyExchange,
                    shc.negotiatedProtocol);
            if (ke == null) {   // unlikely
                throw shc.conContext.fatal(Alert.INTERNAL_ERROR,
                        "Not supported key exchange type");
            } else {
                SSLKeyDerivation masterKD = ke.createKeyDerivation(shc);
                SecretKey masterSecret =
                        masterKD.deriveKey("MasterSecret", null);

                // update the states
                shc.handshakeSession.setMasterSecret(masterSecret);
                SSLTrafficKeyDerivation kd =
                        SSLTrafficKeyDerivation.valueOf(shc.negotiatedProtocol);
                if (kd == null) {       // unlikely
                    throw shc.conContext.fatal(Alert.INTERNAL_ERROR,
                            "Not supported key derivation: " +
                                    shc.negotiatedProtocol);
                } else {
                    shc.handshakeKeyDerivation =
                            kd.createKeyDerivation(shc, masterSecret);
                }
            }
        }

        /**
         * Retrieve the Kerberos key for the specified server principal
         * from the JAAS configuration file.
         *
         * @return true if successful, false if not available or invalid
         */
        Object setupKerberosKeys(ServerHandshakeContext shc) {
            Object serviceCreds = null;
            try {
                final AccessControlContext acc = shc.conContext.acc;
                serviceCreds = AccessController.doPrivileged(
                        // Eliminate dependency on KerberosKey
                        new PrivilegedExceptionAction<Object>() {
                            @Override
                            public Object run() throws Exception {
                                // get kerberos key for the default principal
                                return Krb5Helper.getServiceCreds(acc);
                            }});

                // check permission to access and use the secret key of the
                // Kerberized "host" service
                if (serviceCreds != null) {
                    if (SSLLogger.isOn && SSLLogger.isOn("handshake")) {
                        SSLLogger.fine("Using Kerberos creds");
                    }
                    String serverPrincipal =
                            Krb5Helper.getServerPrincipalName(serviceCreds);
                    if (serverPrincipal != null) {
                        // When service is bound, we check ASAP. Otherwise,
                        // will check after client request is received
                        // in in Kerberos ClientKeyExchange
                        SecurityManager sm = System.getSecurityManager();
                        try {
                            if (sm != null) {
                                // Eliminate dependency on ServicePermission
                                sm.checkPermission(Krb5Helper.getServicePermission(
                                        serverPrincipal, "accept"), acc);
                            }
                        } catch (SecurityException se) {
                            serviceCreds = null;
                            // Do not destroy keys. Will affect Subject
                            if (SSLLogger.isOn && SSLLogger.isOn("handshake")) {
                                SSLLogger.fine("Permission to access Kerberos"
                                        + " secret key denied");
                            }
                            return null;
                        }
                    }
                }
                return serviceCreds;
            } catch (PrivilegedActionException e) {
                // Likely exception here is LoginExceptin
                if (SSLLogger.isOn && SSLLogger.isOn("handshake")) {
                    SSLLogger.fine("Attempt to obtain Kerberos key failed: "
                            + e.toString());
                }
                return null;
            }
        }

    }

}
