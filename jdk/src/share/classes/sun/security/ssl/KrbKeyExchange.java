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

import javax.crypto.SecretKey;
import javax.crypto.spec.SecretKeySpec;
import javax.net.ssl.SSLHandshakeException;
import java.io.IOException;
import java.security.spec.AlgorithmParameterSpec;

public class KrbKeyExchange {
    static final SSLPossessionGenerator poGenerator =
            new KrbPossessionGenerator();
    static final SSLKeyAgreementGenerator kaGenerator =
            new KrbKAGenerator();

    private static final class KrbPossessionGenerator
            implements SSLPossessionGenerator {
        // Prevent instantiation of this class.
        private KrbPossessionGenerator() {
            // blank
        }

        @Override
        public SSLPossession createPossession(HandshakeContext context) {
            // returns dummy possession to resolve cipher suite
            return new KrbPremasterSecret(null);
        }
    }

    static final
    class KrbPremasterSecret implements SSLPossession, SSLCredentials {
        final byte[] premasterSecret;

        KrbPremasterSecret(byte[] premasterSecret) {
            this.premasterSecret = premasterSecret;
        }
    }

    private static final class KrbKAGenerator implements SSLKeyAgreementGenerator {
        // Prevent instantiation of this class.
        private KrbKAGenerator() {
            // blank
        }

        @Override
        public SSLKeyDerivation createKeyDerivation(
                HandshakeContext context) throws IOException {

            KrbPremasterSecret premaster = null;
            if (context instanceof ClientHandshakeContext) {
                for (SSLPossession possession : context.handshakePossessions) {
                    if (possession instanceof KrbPremasterSecret) {
                        premaster = (KrbPremasterSecret)possession;
                        break;
                    }
                }
            } else {
                for (SSLCredentials credential : context.handshakeCredentials) {
                    if (credential instanceof KrbPremasterSecret) {
                        premaster = (KrbPremasterSecret)credential;
                        break;
                    }
                }
            }

            if (premaster == null) {
                throw context.conContext.fatal(Alert.HANDSHAKE_FAILURE,
                        "No sufficient KRB key agreement parameters negotiated");
            }

            return new KRBKAKeyDerivation(context, premaster.premasterSecret);

        }

        private static final
        class KRBKAKeyDerivation implements SSLKeyDerivation {
            private final HandshakeContext context;
            private final byte[] secretBytes;

            KRBKAKeyDerivation(HandshakeContext context,
                               byte[] secret) {
                this.context = context;
                this.secretBytes = secret;
            }

            @Override
            public SecretKey deriveKey(String algorithm,
                                       AlgorithmParameterSpec params) throws IOException {
                try {
                    SecretKey preMasterSecret = new SecretKeySpec(secretBytes,
                            "TlsPremasterSecret");

                    SSLMasterKeyDerivation mskd =
                            SSLMasterKeyDerivation.valueOf(
                                    context.negotiatedProtocol);
                    if (mskd == null) {
                        // unlikely
                        throw new SSLHandshakeException(
                                "No expected master key derivation for protocol: " +
                                        context.negotiatedProtocol.name);
                    }
                    SSLKeyDerivation kd = mskd.createKeyDerivation(
                            context, preMasterSecret);
                    return kd.deriveKey("MasterSecret", params);
                } catch (Exception gse) {
                    throw (SSLHandshakeException) new SSLHandshakeException(
                            "Could not generate secret").initCause(gse);
                }
            }
        }
    }
}
