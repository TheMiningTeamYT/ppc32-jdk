/*
 * Copyright (c) 1998, 2018, Oracle and/or its affiliates. All rights reserved.
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
 * @bug 8048603
 * @summary Check if doFinal and update operation result in same Mac
 * @author Yu-Ching Valerie Peng, Bill Situ, Alexander Fomin
 * @library /lib / ..
 * @modules jdk.crypto.cryptoki
 * @run main/othervm MacSameTest
 * @run main/othervm MacSameTest sm
 * @key randomness
 */

import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import java.security.NoSuchProviderException;
import java.security.Provider;
import java.security.SecureRandom;
import java.util.List;
import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;

public class MacSameTest extends PKCS11Test {

    private static final int MESSAGE_SIZE = 25;
    private static final int OFFSET = 5;
    private static final int KEY_SIZE = 70;

    /**
     * Initialize a message, instantiate a Mac object,
     * initialize the object with a SecretKey,
     * feed the message into the Mac object
     * all at once and get the output MAC as result1.
     * Reset the Mac object, chop the message into three pieces,
     * feed into the Mac object sequentially, and get the output MAC as result2.
     * Finally, compare result1 and result2 and see if they are the same.
     *
     * @param args the command line arguments
     */
    public static void main(String[] args) throws Exception {
        main(new MacSameTest(), args);
    }

    @Override
    public void main(Provider p) {
        List<String> algorithms = getSupportedAlgorithms("Mac", "Hmac", p);
        boolean success = true;
        for (String alg : algorithms) {
            try {
                doTest(alg, p);
            } catch (Exception e) {
                System.out.println("Unexpected exception: " + e);
                e.printStackTrace();
                success = false;
            }
        }

        if (!success) {
            throw new RuntimeException("Test failed");
        }
    }

    private void doTest(String algo, Provider provider)
            throws NoSuchAlgorithmException, NoSuchProviderException,
            InvalidKeyException {
        System.out.println("Test " + algo);
        Mac mac;
        try {
            mac = Mac.getInstance(algo, provider);
        } catch (NoSuchAlgorithmException nsae) {
            if ("SunPKCS11-Solaris".equals(provider.getName())) {
                // depending on Solaris configuration,
                // it can support HMAC or not with Mac
                System.out.println("Expected NoSuchAlgorithmException thrown: "
                        + nsae);
                return;
            }
            throw nsae;
        }

        byte[] plain = new byte[MESSAGE_SIZE];
        for (int i = 0; i < MESSAGE_SIZE; i++) {
            plain[i] = (byte) (i % 256);
        }

        byte[] tail = new byte[plain.length - OFFSET];
        System.arraycopy(plain, OFFSET, tail, 0, tail.length);

        SecureRandom srdm = new SecureRandom();
        byte[] keyVal = new byte[KEY_SIZE];
        srdm.nextBytes(keyVal);
        SecretKeySpec keySpec = new SecretKeySpec(keyVal, "HMAC");

        mac.init(keySpec);
        byte[] result1 = mac.doFinal(plain);

        mac.reset();
        mac.update(plain[0]);
        mac.update(plain, 1, OFFSET - 1);
        byte[] result2 = mac.doFinal(tail);

        if (!java.util.Arrays.equals(result1, result2)) {
            throw new RuntimeException("result1 and result2 are not the same");
        }
    }

}
