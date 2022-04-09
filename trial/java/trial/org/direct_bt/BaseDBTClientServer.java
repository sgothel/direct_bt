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

package trial.org.direct_bt;

import java.lang.invoke.MethodHandles;
import java.lang.reflect.InvocationTargetException;
import java.util.List;

import org.direct_bt.BTAdapter;
import org.direct_bt.BTDeviceRegistry;
import org.direct_bt.BTException;
import org.direct_bt.BTFactory;
import org.direct_bt.BTManager;
import org.direct_bt.BTSecurityRegistry;
import org.direct_bt.BTUtils;
import org.junit.After;
import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.FixMethodOrder;
import org.junit.runners.MethodSorters;

import jau.test.junit.util.SingletonJunitCase;

@FixMethodOrder(MethodSorters.NAME_ASCENDING)
public abstract class BaseDBTClientServer extends SingletonJunitCase {
    static boolean DEBUG = false;

    @BeforeClass
    public static final void setupAll() {
        if( DEBUG ) {
            System.setProperty("direct_bt.debug", "true"); // native code
            // System.setProperty("direct_bt.debug", "true,gatt.data"); // native code
            System.setProperty("org.direct_bt.debug", "true"); // java
        }
        BTFactory.initDirectBTLibrary();

        final Class<?> ThisClazz = MethodHandles.lookup().lookupClass();
        BTUtils.println(System.err, "++++ Test "+ThisClazz.getSimpleName()+".setupAll()");

        Assert.assertTrue( DBTUtils.rmKeyFolder() );
        Assert.assertTrue( DBTUtils.mkdirKeyFolder() );
    }

    /**
     * Ensure
     * - all adapter are powered off
     * - manager being shutdown
     */
    @AfterClass
    public static final void cleanupAll() {
        final Class<?> ThisClazz = MethodHandles.lookup().lookupClass();
        BTUtils.println(System.err, "++++ Test "+ThisClazz.getSimpleName()+".cleanupAll()");

        BTManager manager = null;
        try {
            manager = BTFactory.getDirectBTManager();
        } catch (BTException | NoSuchMethodException | SecurityException
                | IllegalAccessException | IllegalArgumentException
                | InvocationTargetException | ClassNotFoundException e) {
            e.printStackTrace();
            BTUtils.println(System.err, "Unable to instantiate DirectBT BluetoothManager: "+e.getMessage());
            e.printStackTrace();
        }
        if( null != manager ) {
            final List<BTAdapter> adapters = manager.getAdapters();
            for(final BTAdapter a : adapters) {
                a.stopAdvertising();
                a.stopDiscovery();
                a.setPowered(false);
            }
            // All implicit via destructor or shutdown hook!
            manager.shutdown(); /* implies: adapter.close(); */
        }
    }

    /**
     * Ensure
     * - all adapter are powered off
     */
    @Before
    public final void setupTest() {
        final Class<?> ThisClazz = MethodHandles.lookup().lookupClass();
        BTUtils.println(System.err, "++++ Test "+ThisClazz.getSimpleName()+".setupTest()");

        BTManager manager = null;
        try {
            manager = BTFactory.getDirectBTManager();
        } catch (BTException | NoSuchMethodException | SecurityException
                | IllegalAccessException | IllegalArgumentException
                | InvocationTargetException | ClassNotFoundException e) {
            e.printStackTrace();
            BTUtils.println(System.err, "Unable to instantiate DirectBT BluetoothManager: "+e.getMessage());
            e.printStackTrace();
        }
        if( null != manager ) {
            final List<BTAdapter> adapters = manager.getAdapters();
            for(final BTAdapter a : adapters) {
                a.stopAdvertising();
                a.stopDiscovery();
                Assert.assertTrue( a.setPowered(false) );
            }
        }
        BTDeviceRegistry.clearWaitForDevices();
        BTDeviceRegistry.clearProcessedDevices();
        BTDeviceRegistry.clearProcessingDevices();
        BTSecurityRegistry.clear();
    }

    /**
     * Ensure
     * - remove all status listener from all adapter
     * - all adapter are powered off
     * - clear BTDeviceRegistry
     * - clear BTSecurityRegistry
     */
    @After
    public final void cleanupTest() {
        final Class<?> ThisClazz = MethodHandles.lookup().lookupClass();
        BTUtils.println(System.err, "++++ Test "+ThisClazz.getSimpleName()+".cleanupTest()");

        BTManager manager = null;
        try {
            manager = BTFactory.getDirectBTManager();
        } catch (BTException | NoSuchMethodException | SecurityException
                | IllegalAccessException | IllegalArgumentException
                | InvocationTargetException | ClassNotFoundException e) {
            e.printStackTrace();
            BTUtils.println(System.err, "Unable to instantiate DirectBT BluetoothManager: "+e.getMessage());
            e.printStackTrace();
        }
        if( null != manager ) {
            final List<BTAdapter> adapters = manager.getAdapters();
            for(final BTAdapter a : adapters) {
                {
                    final int r = a.removeAllStatusListener();
                    Assert.assertTrue("Not >= 0 removed listener, but "+r, 0 <= r );
                }
                a.stopAdvertising();
                a.stopDiscovery();
                Assert.assertTrue( a.setPowered(false) );
            }
        }
        BTDeviceRegistry.clearWaitForDevices();
        BTDeviceRegistry.clearProcessedDevices();
        BTDeviceRegistry.clearProcessingDevices();
        BTSecurityRegistry.clear();
    }
}
