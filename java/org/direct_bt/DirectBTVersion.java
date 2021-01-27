/**
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2021 Gothel Software e.K.
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

package org.direct_bt;

import java.util.jar.Manifest;

import org.jau.util.JauVersion;
import org.jau.util.VersionUtil;

/**
 * This {@code jaulib} derived version info class is only
 * usable when having {@code jaulib} available, naturally.
 */
public class DirectBTVersion extends JauVersion {

    protected DirectBTVersion(final String packageName, final Manifest mf) {
        super(packageName, mf);
    }

    /**
     * Returns a transient new instance.
     */
    public static DirectBTVersion getInstance() {
            final String packageNameCompileTime = "org.direct_bt";
            final String packageNameRuntime = "org.direct_bt";
            Manifest mf = VersionUtil.getManifest(DirectBTVersion.class.getClassLoader(), packageNameRuntime);
            if(null != mf) {
                return new DirectBTVersion(packageNameRuntime, mf);
            } else {
                mf = VersionUtil.getManifest(DirectBTVersion.class.getClassLoader(), packageNameCompileTime);
                return new DirectBTVersion(packageNameCompileTime, mf);
            }
        }
    }

    public static void main(final String args[]) {
        System.err.println(VersionUtil.getPlatformInfo());
        System.err.println("Version Info:");
        final DirectBTVersion v = DirectBTVersion.getInstance();
        System.err.println(v);
        System.err.println("");
        System.err.println("Full Manifest:");
        System.err.println(v.getFullManifestInfo(null));
    }
}
