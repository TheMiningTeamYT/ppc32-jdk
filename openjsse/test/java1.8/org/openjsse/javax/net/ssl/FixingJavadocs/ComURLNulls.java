/*
 * Copyright (c) 2001, 2015, Oracle and/or its affiliates. All rights reserved.
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
 */

/*
 * @test
 * @bug 4387882 4451038
 * @summary Need to revisit the javadocs for JSSE, especially the
 *      promoted classes, and HttpsURLConnection.getCipherSuite throws
 *      NullPointerException
 * @modules java.base/com.sun.net.ssl
 *          java.base/com.sun.net.ssl.internal.www.protocol.https
 * @run main/othervm ComURLNulls
 *
 *     SunJSSE does not support dynamic system properties, no way to re-use
 *     system properties in samevm/agentvm mode.
 * @author Brad Wetmore
 */

import java.net.*;
import java.io.*;
import javax.net.ssl.*;
import org.openjsse.com.sun.net.ssl.HttpsURLConnection;
import org.openjsse.com.sun.net.ssl.HostnameVerifier;

/*
 * Tests that the com null argument changes made it in ok.
 */

public class ComURLNulls {

    private static class ComSunHTTPSHandlerFactory implements URLStreamHandlerFactory {
        private static String SUPPORTED_PROTOCOL = "https";

        public URLStreamHandler createURLStreamHandler(String protocol) {
            if (!protocol.equalsIgnoreCase(SUPPORTED_PROTOCOL))
                return null;

            return new org.openjsse.com.sun.net.ssl.internal.www.protocol.https.Handler();
        }
    }

    public static void main(String[] args) throws Exception {
        HostnameVerifier reservedHV =
            HttpsURLConnection.getDefaultHostnameVerifier();
        try {
            URL.setURLStreamHandlerFactory(new ComSunHTTPSHandlerFactory());

            /**
             * This test does not establish any connection to the specified
             * URL, hence a dummy URL is used.
             */
            URL foobar = new URL("https://example.com/");

            HttpsURLConnection urlc =
                (HttpsURLConnection) foobar.openConnection();

            try {
                urlc.getCipherSuite();
            } catch (IllegalStateException e) {
                System.out.print("Caught proper exception: ");
                System.out.println(e.getMessage());
            }

            try {
                urlc.getServerCertificates();
            } catch (IllegalStateException e) {
                System.out.print("Caught proper exception: ");
                System.out.println(e.getMessage());
            }

            try {
                urlc.setDefaultHostnameVerifier(null);
            } catch (IllegalArgumentException e) {
                System.out.print("Caught proper exception: ");
                System.out.println(e.getMessage());
            }

            try {
                urlc.setHostnameVerifier(null);
            } catch (IllegalArgumentException e) {
                System.out.print("Caught proper exception: ");
                System.out.println(e.getMessage());
            }

            try {
                urlc.setDefaultSSLSocketFactory(null);
            } catch (IllegalArgumentException e) {
                System.out.print("Caught proper exception: ");
                System.out.println(e.getMessage());
            }

            try {
                urlc.setSSLSocketFactory(null);
            } catch (IllegalArgumentException e) {
                System.out.print("Caught proper exception");
                System.out.println(e.getMessage());
            }
            System.out.println("TESTS PASSED");
        } finally {
            HttpsURLConnection.setDefaultHostnameVerifier(reservedHV);
        }
    }
}
