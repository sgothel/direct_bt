/**
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2026 Gothel Software e.K.
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
package org.direct_bt.osgi;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;

import org.osgi.framework.BundleActivator;
import org.osgi.framework.BundleContext;

/**
 * OSGi {@link BundleActivator} for the Direct-BT bundle.
 * <p>
 * On bundle start it extracts the Direct-BT and jaulib native libraries bundled in this jar (under
 * {@code natives/<arch>/}) onto a writable directory on the JVM {@code java.library.path}, so the
 * Direct-BT loader ({@code org.jau.sys.JNILibrary} / {@code org.direct_bt.PlatformToolkit}) can find
 * and {@code System.load} them by basename. The jau {@code TempJarCache} loader is disabled, since it
 * cannot locate its own jar inside an OSGi framework.
 * <p>
 * The native libraries are loaded by classes of this bundle, so the JNI binding is tied to this
 * bundle's class loader and survives a refresh of any dependent bundle. The activator does not
 * {@code System.load} by absolute path, which would not satisfy the later load-by-basename and would
 * fail with {@code UnsatisfiedLinkError}; it only places the files where the loader looks.
 */
public final class DirectBTActivator implements BundleActivator {

    // Dependency order: base lib first, then its JNI shims, then direct_bt, then its JNI binding.
    private static final String[] LIBS = { "libjaulib.so", "libjaulib_pkg_jni.so", "libjaulib_jni_jni.so",
            "libdirect_bt.so", "libjavadirect_bt.so" };

    @Override
    public void start(final BundleContext context) throws Exception {
        System.setProperty("jau.pkg.UseTempJarCache", "false");
        final String arch = getNativeArch();
        final Path libDir = resolveLibraryPathDir();
        Files.createDirectories(libDir);
        final ClassLoader cl = DirectBTActivator.class.getClassLoader();
        for (final String lib : LIBS) {
            final String resource = "natives/" + arch + "/" + lib;
            try (InputStream in = cl.getResourceAsStream(resource)) {
                if (in == null) {
                    throw new IOException("Bundled native library not found: " + resource);
                }
                final Path target = libDir.resolve(lib);
                Files.copy(in, target, StandardCopyOption.REPLACE_EXISTING);
            }
        }
    }

    @Override
    public void stop(final BundleContext context) throws Exception {
        // Native libraries cannot be unloaded and the BTManager is a process singleton, so leave the
        // extracted files and loaded JNI in place; a dependent bundle refresh re-acquires them.
    }

    /**
     * @return a writable directory on {@code java.library.path}; the first entry may be a non-writable
     *         system directory, so the first writable one is chosen, falling back to {@code java.io.tmpdir}.
     */
    private static Path resolveLibraryPathDir() {
        final String libPath = System.getProperty("java.library.path");
        if (libPath != null) {
            for (final String entry : libPath.split(File.pathSeparator)) {
                if (!entry.isBlank()) {
                    final File dir = new File(entry);
                    if ((dir.isDirectory() && dir.canWrite()) || (!dir.exists() && canCreate(dir))) {
                        return dir.toPath();
                    }
                }
            }
        }
        final String tmp = System.getProperty("java.io.tmpdir");
        return new File(tmp != null ? tmp : "/tmp").toPath();
    }

    private static boolean canCreate(final File dir) {
        final File parent = dir.getParentFile();
        return parent != null && parent.isDirectory() && parent.canWrite();
    }

    private static String getNativeArch() {
        final String os = System.getProperty("os.name", "").toLowerCase();
        final String arch = System.getProperty("os.arch", "").toLowerCase();
        if (!os.startsWith("linux")) {
            throw new UnsupportedOperationException("Direct-BT supports Linux only, found: " + os);
        }
        if (arch.equals("amd64") || arch.equals("x86_64")) {
            return "linux-amd64";
        }
        if (arch.equals("aarch64") || arch.equals("arm64")) {
            return "linux-arm64";
        }
        if (arch.startsWith("arm")) {
            return "linux-arm32";
        }
        throw new UnsupportedOperationException("No bundled Direct-BT natives for architecture: " + arch);
    }
}
