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

import java.lang.reflect.InvocationTargetException;
import java.util.List;

import org.direct_bt.BTRole;
import org.direct_bt.BTSecurityRegistry;
import org.direct_bt.BTAdapter;
import org.direct_bt.BTDeviceRegistry;
import org.direct_bt.BTException;
import org.direct_bt.BTFactory;
import org.direct_bt.BTManager;
import org.direct_bt.BTUtils;
import org.direct_bt.DirectBTVersion;
import org.junit.Assert;
import org.junit.FixMethodOrder;
import org.junit.Test;
import org.junit.runners.MethodSorters;

import jau.test.junit.util.SingletonJunitCase;

/**
 * Testing BTManager bring up using fat `Direct-BT Jaulib Fat Jar` and
 * - test loading native libraries
 * - show all installed adapter
 * - no extra permissions required
 */
@FixMethodOrder(MethodSorters.NAME_ASCENDING)
public class TestBringup00 extends SingletonJunitCase {
    static final boolean DEBUG = false;

    private void resetStates() {
        BTManager manager = null;
        try {
            manager = BTFactory.getDirectBTManager();
        } catch (BTException | NoSuchMethodException | SecurityException
                | IllegalAccessException | IllegalArgumentException
                | InvocationTargetException | ClassNotFoundException e) {
            e.printStackTrace();
            BTUtils.println(System.err, "Unable to instantiate Direct-BT BluetoothManager: "+e.getMessage());
            e.printStackTrace();
        }
        if( null != manager ) {
            final List<BTAdapter> adapters = manager.getAdapters();
            for(final BTAdapter a : adapters) {
                // test runs w/o elevated permissions
                a.removeAllStatusListener();
                // a.stopAdvertising();
                // a.stopDiscovery();
                // Assert.assertTrue( a.setPowered(false) );
            }
        }
        manager.removeAllChangedAdapterSetListener();
        BTDeviceRegistry.clearWaitForDevices();
        BTDeviceRegistry.clearProcessedDevices();
        BTSecurityRegistry.clear();
    }

    @Test(timeout = 40000) // long timeout for valgrind
    public final void test01_ManagerBringup() {
        {
            // System.setProperty("direct_bt.debug", "true"); // native code
            // System.setProperty("direct_bt.debug", "true,gatt.data"); // native code
            // System.setProperty("org.direct_bt.debug", "true"); // java
            // System.setProperty("jau.debug", "true"); // java
            // System.setProperty("jau.verbose", "true"); // java
        }
        BTFactory.initDirectBTLibrary();
        DirectBTVersion.printVersionInfo(System.err);
        resetStates();

        BTManager manager = null;
        try {
            manager = BTFactory.getDirectBTManager();
        } catch (BTException | NoSuchMethodException | SecurityException
                | IllegalAccessException | IllegalArgumentException
                | InvocationTargetException | ClassNotFoundException e) {
            e.printStackTrace();
            Assert.assertNull("Unable to instantiate Direct-BT BluetoothManager: "+e.getMessage(), e);
        }
        if( null == manager ) {
            return;
        }
        final List<BTAdapter> adapters = manager.getAdapters();
        BTUtils.println(System.err, "Adapter: Count "+adapters.size());
        for(int i=0; i<adapters.size(); i++) {
            BTUtils.println(System.err, i+": "+adapters.get(i).toString());
        }
        for(final BTAdapter a : adapters) {
            // test runs w/o elevated permissions
            Assert.assertFalse( a.isInitialized() );
            // Assert.assertFalse( a.isPowered() );
            Assert.assertEquals( BTRole.Master, a.getRole() ); // default role
            Assert.assertTrue( 4 <= a.getBTMajorVersion() );
        }
        // All implicit via destructor or shutdown hook!
        adapters.clear();
        resetStates();
        manager.shutdown(); /* implies: adapter.close(); */
    }

    public static void main(final String args[]) {
        org.junit.runner.JUnitCore.main(TestBringup00.class.getName());
    }
}
