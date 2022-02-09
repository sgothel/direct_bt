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
import java.util.Arrays;
import java.util.List;

import org.direct_bt.BTMode;
import org.direct_bt.BTRole;
import org.direct_bt.BTSecurityLevel;
import org.direct_bt.BTAdapter;
import org.direct_bt.BTException;
import org.direct_bt.BTFactory;
import org.direct_bt.BTManager;
import org.direct_bt.BTUtils;
import org.jau.net.EUI48;
import org.junit.Assert;
import org.junit.BeforeClass;
import org.junit.FixMethodOrder;
import org.junit.Test;
import org.junit.runners.MethodSorters;

/**
 * Basic client and server Bluetooth tests, requiring one BT adapter:
 * - start server advertising
 * - server stop advertising
 * - reuse server-adapter for client-mode discovery (just toggle on/off)
 */
@FixMethodOrder(MethodSorters.NAME_ASCENDING)
public class TestDBTClientServer00 extends BaseDBTClientServer {
    static final boolean DEBUG = false;

    /**
     * Testing BTManager bring up and
     * - test that at least two adapter are present
     * - validating basic default adapter status
     */
    @Test(timeout = 5000)
    public final void test01_ManagerBringup() {
        DBTUtils.printVersionInfo();

        BTManager manager = null;
        try {
            manager = BTFactory.getDirectBTManager();
        } catch (BTException | NoSuchMethodException | SecurityException
                | IllegalAccessException | IllegalArgumentException
                | InvocationTargetException | ClassNotFoundException e) {
            e.printStackTrace();
            Assert.assertNull("Unable to instantiate DirectBT BluetoothManager: "+e.getMessage(), e);
        }
        if( null == manager ) {
            return;
        }
        final List<BTAdapter> adapters = manager.getAdapters();
        BTUtils.println(System.err, "Adapter: Count "+adapters.size()+": "+adapters.toString());
        Assert.assertTrue("Adapter count not >= 1 but "+adapters.size(), adapters.size() >= 1);

        for(final BTAdapter a : adapters) {
            Assert.assertFalse( a.isInitialized() );
            Assert.assertFalse( a.isPowered() );
            Assert.assertEquals( BTRole.Master, a.getRole() ); // default role
            Assert.assertTrue( 4 <= a.getBTMajorVersion() );
        }
    }

    @Test(timeout = 5000)
    public final void test10_ServerStartStop_and_ToggleDiscovery() {
        BTManager manager = null;
        try {
            manager = BTFactory.getDirectBTManager();
        } catch (BTException | NoSuchMethodException | SecurityException
                | IllegalAccessException | IllegalArgumentException
                | InvocationTargetException | ClassNotFoundException e) {
            e.printStackTrace();
            Assert.assertNull("Unable to instantiate DirectBT BluetoothManager: "+e.getMessage(), e);
        }
        if( null == manager ) {
            return;
        }

        final String serverName = "TestDBTCS00-T10";
        final DBTServer00 server = new DBTServer00(EUI48.ALL_DEVICE, BTMode.DUAL, true /* SC */, serverName, BTSecurityLevel.NONE);

        final DBTEndpoint.ChangedAdapterSetListener myChangedAdapterSetListener =
                DBTEndpoint.initChangedAdapterSetListener(manager, Arrays.asList(server));

        //
        // Server start
        //
        DBTEndpoint.checkInitializedState(server);
        DBTServerTest.startAdvertising(server, false /* current_exp_advertising_state */, "test10_startAdvertising");

        //
        // Server stop
        //
        DBTServerTest.stopAdvertising(server, true /* current_exp_advertising_state */, "test10_stopAdvertising");

        //
        // Now reuse adapter for client mode -> Start discovery + Stop Discovery
        //
        {
            final BTAdapter adapter = server.getAdapter();
            {
                final int r = adapter.removeAllStatusListener();
                Assert.assertTrue("Not > 0 removed listener, but "+r, 0 < r );
            }

            DBTEndpoint.startDiscovery(adapter, false /* current_exp_discovering_state */);

            DBTEndpoint.stopDiscovery(adapter, true /* current_exp_discovering_state */);
        }

        final int count = manager.removeChangedAdapterSetListener(myChangedAdapterSetListener);
        BTUtils.println(System.err, "****** EOL Removed ChangedAdapterSetCallback " + count);
    }

    public static void main(final String args[]) {
        org.junit.runner.JUnitCore.main(TestDBTClientServer00.class.getName());
    }
}
