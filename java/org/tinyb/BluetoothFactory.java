/**
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2020 Gothel Software e.K.
 * Copyright (c) 2020 ZAFENA AB
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
package org.tinyb;

import java.io.IOException;
import java.io.InputStream;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.URL;
import java.util.Enumeration;
import java.util.Iterator;
import java.util.Set;
import java.util.jar.Attributes;
import java.util.jar.Manifest;

public class BluetoothFactory {

    /**
     * Identifier names, allowing {@link BluetoothFactory#getBluetoothManager(ImplementationIdentifier)}
     * to initialize the required native libraries and to instantiate the root {@link BluetoothManager} instance.
     */
    public static class ImplementationIdentifier {
        /** Fully qualified class name for the {@link BluetoothManager} implementation */
        public final String BluetoothManagerClassName;
        /** Native library basename for the implementation native library */
        public final String ImplementationNativeLibraryBasename;
        /** Native library basename for the Java binding native library */
        public final String JavaNativeLibraryBasename;

        public ImplementationIdentifier(final String BluetoothManagerClassName,
                                 final String ImplementationNativeLibraryBasename,
                                 final String JavaNativeLibraryBasename) {
            this.BluetoothManagerClassName = BluetoothManagerClassName;
            this.ImplementationNativeLibraryBasename = ImplementationNativeLibraryBasename;
            this.JavaNativeLibraryBasename = JavaNativeLibraryBasename;
        }

        @Override
        public String toString() {
            return "ImplementationIdentifier[class "+BluetoothManagerClassName+
                    ", implLib "+ImplementationNativeLibraryBasename+
                    ", javaLib "+JavaNativeLibraryBasename+"]";
        }
    }

    /**
     * {@link ImplementationIdentifier} for D-Bus implementation: {@value}
     * <p>
     * This value is exposed for convenience, user implementations are welcome.
     * </p>
     */
    public static final ImplementationIdentifier DBusImplementationID = new ImplementationIdentifier("tinyb.dbus.DBusManager", "tinyb", "javatinyb");

    /**
     * {@link ImplementationIdentifier} for direct_bt implementation: {@value}
     * <p>
     * This value is exposed for convenience, user implementations are welcome.
     * </p>
     */
    public static final ImplementationIdentifier DirectBTImplementationID = new ImplementationIdentifier("direct_bt.tinyb.DBTManager", "direct_bt", "javadirect_bt");

    /**
     * Manifest's {@link Attributes.Name#SPECIFICATION_VERSION} or {@code null} if not available.
     */
    public static final String getAPIVersion() { return APIVersion; }
    private static String APIVersion;

    /**
     * Manifest's {@link Attributes.Name#IMPLEMENTATION_VERSION} or {@code null} if not available.
     */
    public static final String getImplVersion() { return ImplVersion; }
    private static String ImplVersion;

    static final boolean VERBOSE;

    static {
        {
            final String v = System.getProperty("org.tinyb.verbose", "false");
            VERBOSE = Boolean.valueOf(v);
        }
    }

    private static ImplementationIdentifier initializedID = null;

    public static synchronized void checkInitialized() {
        if( null == initializedID ) {
            throw new IllegalStateException("BluetoothFactory not initialized.");
        }
    }

    private static synchronized void initLibrary(final ImplementationIdentifier id) {
        if( null != initializedID ) {
            if( id != initializedID ) {
                throw new IllegalStateException("BluetoothFactory already initialized with "+initializedID+", can't override by "+id);
            }
            return;
        }

        try {
            System.loadLibrary(id.ImplementationNativeLibraryBasename);
        } catch (final Throwable e) {
            System.err.println("Failed to load native library "+id.ImplementationNativeLibraryBasename);
            e.printStackTrace();
            throw e; // fwd exception - end here
        }
        try {
            System.loadLibrary(id.JavaNativeLibraryBasename);
        } catch (final Throwable  e) {
            System.err.println("Failed to load native library "+id.JavaNativeLibraryBasename);
            e.printStackTrace();
            throw e; // fwd exception - end here
        }

        try {
            final Manifest manifest = getManifest(BluetoothFactory.class.getClassLoader(), new String[] { "org.tinyb" } );
            final Attributes mfAttributes = null != manifest ? manifest.getMainAttributes() : null;

            // major.minor must match!
            final String NAPIVersion = getNativeAPIVersion();
            final String JAPIVersion = null != mfAttributes ? mfAttributes.getValue(Attributes.Name.SPECIFICATION_VERSION) : null;
            if ( null != JAPIVersion && JAPIVersion.equals(NAPIVersion) == false) {
                final String[] NAPIVersionCode = NAPIVersion.split("\\D");
                final String[] JAPIVersionCode = JAPIVersion.split("\\D");
                if (JAPIVersionCode[0].equals(NAPIVersionCode[0]) == false) {
                    if (Integer.valueOf(JAPIVersionCode[0]) < Integer.valueOf(NAPIVersionCode[0])) {
                        throw new RuntimeException("Java library "+JAPIVersion+" < native library "+NAPIVersion+". Please update the Java library.");
                    } else {
                        throw new RuntimeException("Native library "+NAPIVersion+" < java library "+JAPIVersion+". Please update the native library.");
                    }
                } else if (JAPIVersionCode[1].equals(NAPIVersionCode[1]) == false) {
                    if (Integer.valueOf(JAPIVersionCode[1]) < Integer.valueOf(NAPIVersionCode[1])) {
                        throw new RuntimeException("Java library "+JAPIVersion+" < native library "+NAPIVersion+". Please update the Java library.");
                    } else {
                        throw new RuntimeException("Native library "+NAPIVersion+" < java library "+JAPIVersion+". Please update the native library.");
                    }
                }
            }
            initializedID = id; // initialized!

            APIVersion = JAPIVersion;
            ImplVersion = null != mfAttributes ? mfAttributes.getValue(Attributes.Name.IMPLEMENTATION_VERSION) : null;
            if( VERBOSE ) {
                System.err.println("tinyb2 loaded "+id);
                System.err.println("tinyb2 java api version "+JAPIVersion);
                System.err.println("tinyb2 native api version "+NAPIVersion);
                if( null != mfAttributes ) {
                    final Attributes.Name[] versionAttributeNames = new Attributes.Name[] {
                            Attributes.Name.SPECIFICATION_TITLE,
                            Attributes.Name.SPECIFICATION_VENDOR,
                            Attributes.Name.SPECIFICATION_VERSION,
                            Attributes.Name.IMPLEMENTATION_TITLE,
                            Attributes.Name.IMPLEMENTATION_VENDOR,
                            Attributes.Name.IMPLEMENTATION_VERSION,
                            new Attributes.Name("Implementation-Commit") };
                    for( final Attributes.Name an : versionAttributeNames ) {
                        System.err.println("  "+an+": "+mfAttributes.getValue(an));
                    }
                } else {
                    System.err.println("  No Manifest available;");
                }
            }
        } catch (final Throwable e) {
            System.err.println("Error querying manifest information.");
            e.printStackTrace();
            throw e; // fwd exception - end here
        }
    }

    private static synchronized BluetoothManager getBluetoothManager(final Class<?> factoryImplClass)
            throws BluetoothException, NoSuchMethodException, SecurityException,
                   IllegalAccessException, IllegalArgumentException, InvocationTargetException
    {
        final Method m = factoryImplClass.getMethod("getBluetoothManager");
        return (BluetoothManager)m.invoke(null);
    }

    /**
     * Returns an initialized BluetoothManager instance using the given {@code factoryImplClass}.
     * <p>
     * The {@code factoryImplClass} must provide the static method
     * <pre>
     * public static synchronized BluetoothManager getBluetoothManager() throws BluetoothException { .. }
     * </pre>
     * </p>
     * @param id the fully qualified factory implementation class name
     * @throws BluetoothException
     * @throws NoSuchMethodException
     * @throws SecurityException
     * @throws IllegalAccessException
     * @throws IllegalArgumentException
     * @throws InvocationTargetException
     * @throws ClassNotFoundException
     * @see {@link #DBusFactoryImplClassName}
     * @see {@link #DirectBTFactoryImplClassName}
     */
    public static synchronized BluetoothManager getBluetoothManager(final ImplementationIdentifier id)
            throws BluetoothException, NoSuchMethodException, SecurityException,
                   IllegalAccessException, IllegalArgumentException, InvocationTargetException, ClassNotFoundException
    {
        initLibrary(id);
        final Class<?> factoryImpl = Class.forName(id.BluetoothManagerClassName);
        return getBluetoothManager(factoryImpl);
    }

    /**
     * Returns an initialized BluetoothManager instance using a D-Bus implementation.
     * <p>
     * Issues {@link #getBluetoothManager(ImplementationIdentifier)} using {@link #DBusImplementationID}.
     * </p>
     * @throws BluetoothException
     * @throws NoSuchMethodException
     * @throws SecurityException
     * @throws IllegalAccessException
     * @throws IllegalArgumentException
     * @throws InvocationTargetException
     * @throws ClassNotFoundException
     */
    public static synchronized BluetoothManager getDBusBluetoothManager()
            throws BluetoothException, NoSuchMethodException, SecurityException,
                   IllegalAccessException, IllegalArgumentException, InvocationTargetException, ClassNotFoundException
    {
        return getBluetoothManager(DBusImplementationID);
    }

    /**
     * Returns an initialized BluetoothManager instance using the DirectBT implementation.
     * <p>
     * Issues {@link #getBluetoothManager(ImplementationIdentifier)} using {@link #DirectBTImplementationID}.
     * </p>
     * @throws BluetoothException
     * @throws NoSuchMethodException
     * @throws SecurityException
     * @throws IllegalAccessException
     * @throws IllegalArgumentException
     * @throws InvocationTargetException
     * @throws ClassNotFoundException
     */
    public static synchronized BluetoothManager getDirectBTBluetoothManager()
            throws BluetoothException, NoSuchMethodException, SecurityException,
                   IllegalAccessException, IllegalArgumentException, InvocationTargetException, ClassNotFoundException
    {
        return getBluetoothManager(DirectBTImplementationID);
    }

    private static final boolean debug = false;

    private static final Manifest getManifest(final ClassLoader cl, final String[] extensions) {
        final Manifest[] extManifests = new Manifest[extensions.length];
        try {
            final Enumeration<URL> resources = cl.getResources("META-INF/MANIFEST.MF");
            while (resources.hasMoreElements()) {
                final URL resURL = resources.nextElement();
                if( debug ) {
                    System.err.println("resource: "+resURL);
                }
                final InputStream is = resURL.openStream();
                final Manifest manifest;
                try {
                    manifest = new Manifest(is);
                } finally {
                    try {
                        is.close();
                    } catch (final IOException e) {}
                }
                final Attributes attributes = manifest.getMainAttributes();
                if(attributes != null) {
                    final String attributesExtName = attributes.getValue( Attributes.Name.EXTENSION_NAME );
                    if( debug ) {
                        System.err.println("resource: "+resURL+", attributes extName "+attributesExtName+", count "+attributes.size());
                        final Set<Object> keys = attributes.keySet();
                        for(final Iterator<Object> iter=keys.iterator(); iter.hasNext(); ) {
                            final Attributes.Name key = (Attributes.Name) iter.next();
                            final String val = attributes.getValue(key);
                            System.err.println("  "+key+": "+val);
                        }
                    }
                    for(int i=0; i < extensions.length && null == extManifests[i]; i++) {
                        final String extension = extensions[i];
                        if( extension.equals( attributesExtName ) ) {
                            if( 0 == i ) {
                                return manifest; // 1st one has highest prio - done
                            }
                            extManifests[i] = manifest;
                        }
                    }
                }
            }
        } catch (final IOException ex) {
            throw new RuntimeException("Unable to read manifest.", ex);
        }
        for(int i=1; i<extManifests.length; i++) {
            if( null != extManifests[i] ) {
                return extManifests[i];
            }
        }
        return null;
    }

    private native static String getNativeAPIVersion();
}
