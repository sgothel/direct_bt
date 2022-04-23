/**
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2022 Gothel Software e.K.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

package test.org.direct_bt;

import java.io.IOException;
import java.net.URISyntaxException;
import java.security.NoSuchAlgorithmException;

import org.jau.sec.SHASum;
import org.jau.util.VersionUtil;
import org.junit.Assert;
import org.junit.FixMethodOrder;
import org.junit.Test;
import org.junit.runners.MethodSorters;
import org.direct_bt.DirectBTVersion;

import jau.test.junit.util.SingletonJunitCase;

@FixMethodOrder(MethodSorters.NAME_ASCENDING)
public class TestVersionInfo extends SingletonJunitCase {
    static boolean VERBOSE = false;

    @Test
    public void test01Info() {
        System.err.println(VersionUtil.getPlatformInfo());
        System.err.println("Version Info:");
        System.err.println(DirectBTVersion.getInstance());
        System.err.println("");
        System.err.println("Full Manifest:");
        System.err.println(DirectBTVersion.getInstance().getFullManifestInfo(null));
    }

    // @Test // FIXME: Add SHA signature in build system!
    public void test02ValidateSHA()
            throws IllegalArgumentException, IOException, URISyntaxException, SecurityException, NoSuchAlgorithmException
    {
        final DirectBTVersion info = DirectBTVersion.getInstance();
        final String shaClassesThis = info.getImplementationSHAClassesThis();
        System.err.println("SHA CLASSES.this (build-time): "+shaClassesThis);

        final DirectBTVersion.JarSHASum shaSum = new DirectBTVersion.JarSHASum();
        final byte[] shasum = shaSum.compute(VERBOSE);
        final String shaClasses = SHASum.toHexString(shasum, null).toString();
        System.err.println("SHA CLASSES.this (now): "+shaClasses);
        Assert.assertEquals("SHA not equal", shaClassesThis, shaClasses);
    }

    public static void main(final String args[]) throws IOException {
        // VERBOSE = true;
        final String tstname = TestVersionInfo.class.getName();
        org.junit.runner.JUnitCore.main(tstname);
    }

}
