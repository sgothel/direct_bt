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
package org.direct_bt;

import java.io.IOException;
import java.io.InputStream;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.URL;
import java.security.PrivilegedAction;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.Iterator;
import java.util.List;
import java.util.Properties;
import java.util.Set;
import java.util.concurrent.atomic.AtomicReference;
import java.util.jar.Attributes;
import java.util.jar.Manifest;

import org.jau.util.VersionUtil;

/**
 * One stop {@link BTManager} API entry point.
 * <p>
 * Further provides access to certain property settings,
 * see {@link #DEBUG}, {@link #VERBOSE}, {@link #DEFAULT_BTMODE} and {@link BTManager.Settings}.
 * </p>
 */
public class BTFactory {

    /**
     * Identifier names, allowing {@link BTFactory#getBTManager(ImplementationIdentifier)}
     * to initialize the required native libraries and to instantiate the root {@link BTManager} instance.
     * <p>
     * The implementation class must provide the static factory method
     * <pre>
     * public static synchronized BluetoothManager getBluetoothManager() throws BluetoothException { .. }
     * </pre>
     * </p>
     */
    public static class ImplementationIdentifier {
        /**
         * Fully qualified class name for the {@link BTManager} implementation
         * <p>
         * The implementation class must provide the static factory method
         * <pre>
         * public static synchronized BluetoothManager getBluetoothManager() throws BluetoothException { .. }
         * </pre>
         * </p>
         */
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

        /**
         * <p>
         * Implementation compares {@link #BluetoothManagerClassName} only for equality.
         * </p>
         * {@inheritDoc}
         */
        @Override
        public boolean equals(final Object other) {
            if( null == other || !(other instanceof ImplementationIdentifier) ) {
                return false;
            }
            final ImplementationIdentifier o = (ImplementationIdentifier)other;
            return BluetoothManagerClassName.equals( o.BluetoothManagerClassName );
        }

        @Override
        public String toString() {
            return "ImplementationIdentifier[class "+BluetoothManagerClassName+
                    ", implLib "+ImplementationNativeLibraryBasename+
                    ", javaLib "+JavaNativeLibraryBasename+"]";
        }
    }

    /**
     * {@link ImplementationIdentifier} for direct_bt implementation: {@value}
     * <p>
     * This value is exposed for convenience, user implementations are welcome.
     * </p>
     */
    public static final ImplementationIdentifier DirectBTImplementationID = new ImplementationIdentifier("jau.direct_bt.DBTManager", "direct_bt", "javadirect_bt");

    private static final List<ImplementationIdentifier> implIDs = new ArrayList<ImplementationIdentifier>();

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

    /**
     * Verbose logging enabled or disabled.
     * <p>
     * System property {@code org.direct_bt.verbose}, boolean, default {@code false}.
     * </p>
     */
    public static final boolean VERBOSE;

    /**
     * Debug logging enabled or disabled.
     * <p>
     * System property {@code org.direct_bt.debug}, boolean, default {@code false}.
     * </p>
     */
    public static final boolean DEBUG;

    /**
     * True if jaulib {@link org.jau.sys.PlatformProps} has been detected.
     */
    public static final boolean JAULIB_AVAILABLE;

    /**
     * True if jaulib {@link #JAULIB_AVAILABLE} and its {@link org.jau.sys.PlatformProps#USE_TEMP_JAR_CACHE} is true,
     * i.e. the jar cache is available, enabled and in use.
     */
    public static final boolean JAULIB_JARCACHE_USED;

    /**
     * Deprecated call to {@link java.security.AccessController#doPrivileged(PrivilegedAction)} w/o warnings.
     * @param <T>
     * @param o
     * @return
     */
    @SuppressWarnings({ "deprecation", "removal" })
    public static <T> T doPrivileged(final PrivilegedAction<T> o) {
        return java.security.AccessController.doPrivileged( o );
    }

    static {
        {
            final String v = System.getProperty("org.direct_bt.debug", "false");
            DEBUG = Boolean.valueOf(v);
        }
        if( DEBUG ) {
            VERBOSE = true;
        } else  {
            final String v = System.getProperty("org.direct_bt.verbose", "false");
            VERBOSE = Boolean.valueOf(v);
        }
        implIDs.add(DirectBTImplementationID);

        boolean isJaulibAvail = false;
        try {
            isJaulibAvail = null != Class.forName("org.jau.sys.PlatformProps", true /* initializeClazz */, BTFactory.class.getClassLoader());
        } catch( final Throwable t ) {
            if( DEBUG ) {
                System.err.println("BTFactory Caught: "+t.getMessage());
                t.printStackTrace();
            }
        }
        JAULIB_AVAILABLE = isJaulibAvail;

        if( isJaulibAvail ) {
            JAULIB_JARCACHE_USED = org.jau.sys.PlatformProps.USE_TEMP_JAR_CACHE;
        } else {
            JAULIB_JARCACHE_USED = false;
        }
        if( VERBOSE ) {
            System.err.println("Jaulib available: "+JAULIB_AVAILABLE+", JarCache in use: "+JAULIB_JARCACHE_USED);
        }
    }

    private static AtomicReference<ImplementationIdentifier> initializedID = new AtomicReference<ImplementationIdentifier>(null);

    public static void checkInitialized() {
        if( null == initializedID.get() ) {
            throw new IllegalStateException("Direct-BT not initialized.");
        }
    }
    public static boolean isInitialized() {
        return null != initializedID.get();
    }

    private static synchronized void initLibrary(final ImplementationIdentifier id) {
        if( null != initializedID.get() ) {
            if( id != initializedID.get() ) {
                throw new IllegalStateException("Direct-BT already initialized with "+initializedID.get()+", can't override by "+id);
            }
            return;
        }

        final ClassLoader cl = BTFactory.class.getClassLoader();
        boolean libsLoaded = false;
        if( JAULIB_AVAILABLE ) {
            if( JAULIB_JARCACHE_USED ) {
                try {
                    org.jau.pkg.JNIJarLibrary.addNativeJarLibs(new Class<?>[] { BTFactory.class }, null);
                } catch (final Exception e0) {
                    System.err.println("BTFactory Caught "+e0.getClass().getSimpleName()+": "+e0.getMessage()+", while JNILibLoaderBase.addNativeJarLibs(..)");
                    if( DEBUG ) {
                        e0.printStackTrace();
                    }
                }
            }
            try {
                if( null != org.jau.sys.dl.NativeLibrary.open(id.ImplementationNativeLibraryBasename,
                                               true /* searchSystemPath */, false /* searchSystemPathFirst */, cl, true /* global */) )
                {
                    org.jau.sys.JNILibrary.loadLibrary(id.JavaNativeLibraryBasename, false, cl);
                    libsLoaded = true;
                }
            } catch (final Throwable t) {
                System.err.println("Caught "+t.getClass().getSimpleName()+": "+t.getMessage()+", while loading libs..");
                if( DEBUG ) {
                    t.printStackTrace();
                }
            }
            if( DEBUG ) {
                System.err.println("Jaulib: Native libs loaded: "+ libsLoaded);
            }
        }
        if( !libsLoaded ) {
            try {
                final Throwable[] t = { null };
                if( !PlatformToolkit.loadLibrary(id.ImplementationNativeLibraryBasename, cl, t) ) {
                    throw new RuntimeException("Couldn't load native library with basename <"+id.ImplementationNativeLibraryBasename+">", t[0]);
                }
                if( !PlatformToolkit.loadLibrary(id.JavaNativeLibraryBasename, cl, t) ) {
                    throw new RuntimeException("Couldn't load native library with basename <"+id.JavaNativeLibraryBasename+">", t[0]);
                }
            } catch (final Throwable e) {
                System.err.println("Caught "+e.getClass().getSimpleName()+": "+e.getMessage()+", while loading libs (2) ..");
                if( DEBUG ) {
                    e.printStackTrace();
                }
                throw e; // fwd exception - end here
            }
        }

        // Map all Java properties '[org.]direct_bt.*' and 'direct_bt.*' to native environment.
        try {
            if( DEBUG ) {
                System.err.println("BlootoothFactory: Mapping '[org.|jau.]direct_bt.*' properties to native environment");
            }
            final Properties props = doPrivileged(new PrivilegedAction<Properties>() {
                  @Override
                  public Properties run() {
                      return System.getProperties();
                  } });

            final Enumeration<?> enums = props.propertyNames();
            while (enums.hasMoreElements()) {
              final String key = (String) enums.nextElement();
              if( key.startsWith("org.direct_bt.") || key.startsWith("jau.direct_bt.") ||
                  key.startsWith("direct_bt.") )
              {
                  final String value = props.getProperty(key);
                  if( DEBUG ) {
                      System.err.println("  <"+key+"> := <"+value+">");
                  }
                  setenv(key, value, true /* overwrite */);
              }
            }
        } catch (final Throwable e) {
            System.err.println("Caught exception while forwarding system properties: "+e.getMessage());
            e.printStackTrace();
        }

        try {
            final Manifest manifest = getManifest(cl, new String[] { "org.direct_bt" } );
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
            initializedID.set( id ); // initialized!

            APIVersion = JAPIVersion;
            ImplVersion = null != mfAttributes ? mfAttributes.getValue(Attributes.Name.IMPLEMENTATION_VERSION) : null;
            if( VERBOSE ) {
                System.err.println("Direct-BT loaded "+id);
                System.err.println("Direct-BT java api version "+JAPIVersion);
                System.err.println("Direct-BT native api version "+NAPIVersion);
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

    private static synchronized BTManager getBTManager(final Class<?> factoryImplClass)
            throws BTException, NoSuchMethodException, SecurityException,
                   IllegalAccessException, IllegalArgumentException, InvocationTargetException
    {
        final Method m = factoryImplClass.getMethod("getManager");
        return (BTManager)m.invoke(null);
    }

    /**
     * Registers a new {@link ImplementationIdentifier} to the internal list.
     * The {@code id} is only added if not registered yet.
     * @param id the {@link ImplementationIdentifier} to register
     * @return {@code true} if the given {@link ImplementationIdentifier} has been newly added,
     * otherwise {@code false}.
     */
    public static synchronized boolean registerImplementationIdentifier(final ImplementationIdentifier id) {
        if( null == id ) {
            return false;
        }
        if( implIDs.contains(id) ) {
            return false;
        }
        return implIDs.add(id);
    }

    /**
     * Returns the matching {@link ImplementationIdentifier} from the internal list or {@code null} if not found.
     * @param fqBluetoothManagerImplementationClassName fully qualified class name for the {@link BTManager} implementation
     */
    public static synchronized ImplementationIdentifier getImplementationIdentifier(final String fqBluetoothManagerImplementationClassName) {
        for(final ImplementationIdentifier id : implIDs) {
            if( id.BluetoothManagerClassName.equals(fqBluetoothManagerImplementationClassName) ) {
                return id;
            }
        }
        return null;
    }

    /**
     * Returns an initialized BluetoothManager instance using the given {@code fqBluetoothManagerImplementationClassName}
     * to lookup a registered {@link ImplementationIdentifier}.
     * <p>
     * If found, method returns {@link #getBTManager(ImplementationIdentifier)}, otherwise {@code null}.
     * </p>
     * <p>
     * The chosen implementation can't be changed within a running implementation, an exception is thrown if tried.
     * </p>
     *
     * @param fqBluetoothManagerImplementationClassName fully qualified class name for the {@link BTManager} implementation
     * @throws BTException
     * @throws NoSuchMethodException
     * @throws SecurityException
     * @throws IllegalAccessException
     * @throws IllegalArgumentException
     * @throws InvocationTargetException
     * @throws ClassNotFoundException
     * @see {@link #DBusFactoryImplClassName}
     * @see {@link #DirectBTFactoryImplClassName}
     */
    public static synchronized BTManager getBTManager(final String fqBluetoothManagerImplementationClassName)
            throws BTException, NoSuchMethodException, SecurityException,
                   IllegalAccessException, IllegalArgumentException, InvocationTargetException, ClassNotFoundException
    {
        final ImplementationIdentifier id = getImplementationIdentifier(fqBluetoothManagerImplementationClassName);
        if( null != id ) {
            return getBTManager(id);
        }
        return null;
    }

    /**
     * Returns an initialized BluetoothManager instance using the given {@link ImplementationIdentifier}.
     * <p>
     * If the {@link ImplementationIdentifier} has not been {@link #registerImplementationIdentifier(ImplementationIdentifier)},
     * it will be added to the list.
     * </p>
     * <p>
     * The chosen implementation can't be changed within a running implementation, an exception is thrown if tried.
     * </p>
     * @param id the specific {@link ImplementationIdentifier}
     * @throws BTException
     * @throws NoSuchMethodException
     * @throws SecurityException
     * @throws IllegalAccessException
     * @throws IllegalArgumentException
     * @throws InvocationTargetException
     * @throws ClassNotFoundException
     * @see {@link #DBusFactoryImplClassName}
     * @see {@link #DirectBTFactoryImplClassName}
     */
    public static synchronized BTManager getBTManager(final ImplementationIdentifier id)
            throws BTException, NoSuchMethodException, SecurityException,
                   IllegalAccessException, IllegalArgumentException, InvocationTargetException, ClassNotFoundException
    {
        registerImplementationIdentifier(id);
        initLibrary(id);
        final Class<?> factoryImpl = Class.forName(id.BluetoothManagerClassName);
        return getBTManager(factoryImpl);
    }

    /**
     * Returns an initialized BluetoothManager instance using the DirectBT implementation.
     * <p>
     * Issues {@link #getBTManager(ImplementationIdentifier)} using {@link #DirectBTImplementationID}.
     * </p>
     * <p>
     * The chosen implementation can't be changed within a running implementation, an exception is thrown if tried.
     * </p>
     * @throws BTException
     * @throws NoSuchMethodException
     * @throws SecurityException
     * @throws IllegalAccessException
     * @throws IllegalArgumentException
     * @throws InvocationTargetException
     * @throws ClassNotFoundException
     */
    public static synchronized BTManager getDirectBTManager()
            throws BTException, NoSuchMethodException, SecurityException,
                   IllegalAccessException, IllegalArgumentException, InvocationTargetException, ClassNotFoundException
    {
        return getBTManager(DirectBTImplementationID);
    }

    /**
     * Preloads the DirectBT native library w/o instantiating BTManager.
     */
    public static synchronized void initDirectBTLibrary() {
        initLibrary(DirectBTImplementationID);
    }

    /**
     * Helper function to retrieve a Manifest instance.
     */
    public static final Manifest getManifest(final ClassLoader cl, final String[] extensions) {
        final Manifest[] extManifests = new Manifest[extensions.length];
        try {
            final Enumeration<URL> resources = cl.getResources("META-INF/MANIFEST.MF");
            while (resources.hasMoreElements()) {
                final URL resURL = resources.nextElement();
                if( DEBUG ) {
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

    public static void main(final String args[]) {
        System.err.println("Jaulib: Available "+JAULIB_AVAILABLE+", JarCache in use "+JAULIB_JARCACHE_USED);
        if( JAULIB_AVAILABLE ) {
            System.err.println(VersionUtil.getPlatformInfo());
            System.err.println("Version Info:");
            final DirectBTVersion v = DirectBTVersion.getInstance();
            System.err.println(v);
            System.err.println("");
            System.err.println("Full Manifest:");
            System.err.println(v.getFullManifestInfo(null));
        } else {
            System.err.println("Full Manifest:");
            final Manifest manifest = getManifest(BTFactory.class.getClassLoader(), new String[] { "org.direct_bt" } );
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
            System.err.println(sb);
        }
        try {
            final BTManager mngr = getDirectBTManager();
            final List<BTAdapter> adapters = mngr.getAdapters();
            System.err.println("BTManager: adapters "+adapters.size());
            int i=0;
            for(final Iterator<BTAdapter> iter = adapters.iterator(); iter.hasNext(); ++i) {
                System.err.println("BTAdapter["+i+"]: "+iter.next().toString()); // has full toString()
            }
        } catch (BTException | NoSuchMethodException | SecurityException
                | IllegalAccessException | IllegalArgumentException
                | InvocationTargetException | ClassNotFoundException e) {
            e.printStackTrace();
        }
    }

    public native static String getNativeVersion();
    public native static String getNativeAPIVersion();
    private native static void setenv(String name, String value, boolean overwrite);
}

/** \example DBTScanner10.java
 * This Java scanner {@link BTRole::Master} GATT client example uses an event driven workflow
 * and multithreading, i.e. one thread processes each found device when notified.
 * <p>
 * This example represents the recommended utilization of Direct-BT.
 * </p>
 * <p>
 * See `dbt_scanner10.cpp` for invocation examples, since both apps are fully compatible.
 * </p>
 */

/** \example DBTPeripheral00.java
 * This Java peripheral {@link BTRole::Slave} GATT server example uses an event driven workflow.
 * <p>
 * See `dbt_peripheral00.cpp` for invocation examples, since both apps are fully compatible.
 * </p>
 */

/** \example TestDBTClientServer00.java
 * Unit test, trial using actual BT adapter.
 *
 * Basic client and server Bluetooth tests, requiring one BT adapter:
 * - start server advertising
 * - server stop advertising
 * - reuse server-adapter for client-mode discovery (just toggle on/off)
 */

/** \example TestDBTClientServer10.java
 * Unit test, trial using actual BT adapter.
 *
 * Testing a full Bluetooth server and client lifecycle of operations, requiring two BT adapter:
 * - start server advertising
 * - start client discovery and connect to server when discovered
 * - client/server processing of connection when ready
 * - client disconnect
 * - server stop advertising
 * - security-level: NONE, ENC_ONLY freshly-paired and ENC_ONLY pre-paired
 * - reuse server-adapter for client-mode discovery (just toggle on/off)
 */
