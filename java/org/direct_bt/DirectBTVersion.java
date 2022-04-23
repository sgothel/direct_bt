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

import java.io.IOException;
import java.io.PrintStream;
import java.net.URISyntaxException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Set;
import java.util.jar.Attributes;
import java.util.jar.Manifest;
import java.util.regex.Pattern;

import org.jau.base.JaulibVersion;
import org.jau.pkg.TempJarSHASum;
import org.jau.sec.SHASum;
import org.jau.util.JauVersion;
import org.jau.util.VersionUtil;

/**
 * This {@code jaulib} derived version info class is only
 * usable when having {@code jaulib} available, naturally.
 */
public class DirectBTVersion extends JauVersion {

    /**
     * Print full Direct-BT version information.
     *
     * {@link BTFactory#initDirectBTLibrary()} is being called.
     *
     * @param out the output stream used
     */
    public static final void printVersionInfo(final PrintStream out) {
        BTFactory.initDirectBTLibrary();

        BTUtils.println(out, "BTFactory: Jaulib: Available "+BTFactory.JAULIB_AVAILABLE+", JarCache in use "+BTFactory.JAULIB_JARCACHE_USED);
        if( BTFactory.JAULIB_AVAILABLE ) {
            out.println(VersionUtil.getPlatformInfo());
            BTUtils.println(out, "Version Info:");
            final DirectBTVersion v = DirectBTVersion.getInstance();
            out.println(v.toString());
            BTUtils.println(out, "");
            BTUtils.println(out, "Full Manifest:");
            out.println(v.getFullManifestInfo(null).toString());
        } else {
            BTUtils.println(out, "Full Manifest:");
            final Manifest manifest = BTFactory.getManifest(BTFactory.class.getClassLoader(), new String[] { "org.direct_bt" } );
            final Attributes attr = manifest.getMainAttributes();
            final Set<Object> keys = attr.keySet();
            final StringBuilder sb = new StringBuilder();
            for(final Iterator<Object> iter=keys.iterator(); iter.hasNext(); ) {
                final Attributes.Name key = (Attributes.Name) iter.next();
                final String val = attr.getValue(key);
                sb.append(" ");
                sb.append(key);
                sb.append(" = ");
                sb.append(val);
                sb.append(System.lineSeparator());
            }
            out.println(sb.toString());
        }

        BTUtils.println(out, "Direct-BT Native Version "+BTFactory.getNativeVersion()+" (API "+BTFactory.getNativeAPIVersion()+")");
        BTUtils.println(out, "Direct-BT Java Version   "+BTFactory.getImplVersion()+" (API "+BTFactory.getAPIVersion()+")");
    }

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

    /**
     * Direct-BT definition of {@link TempJarSHASum}'s specialization of {@link SHASum}.
     * <p>
     * Implementation uses {@link org.jau.pkg.cache.TempJarCache}.
     * </p>
     * <p>
     * Constructor defines the includes and excludes as used for Direct-BT {@link SHASum} computation.
     * </p>
     */
    static public class JarSHASum extends TempJarSHASum {
        /**
         * See {@link DirectBTJarSHASum}
         * @throws SecurityException
         * @throws IllegalArgumentException
         * @throws NoSuchAlgorithmException
         * @throws IOException
         * @throws URISyntaxException
         */
        public JarSHASum()
                throws SecurityException, IllegalArgumentException, NoSuchAlgorithmException, IOException, URISyntaxException
        {
            super(MessageDigest.getInstance("SHA-256"), JaulibVersion.class, new ArrayList<Pattern>(), new ArrayList<Pattern>());
            final List<Pattern> includes = getIncludes();
            final String origin = getOrigin();
            includes.add(Pattern.compile(origin+"/org/direct_bt/.*"));
            includes.add(Pattern.compile(origin+"/jau/direct_bt/.*"));
        }
    }
    public static void main(final String args[]) {
        BTFactory.main(args);
    }
}
