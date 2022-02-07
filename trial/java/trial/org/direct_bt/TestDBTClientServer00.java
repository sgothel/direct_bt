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

import org.direct_bt.BTMode;
import org.direct_bt.BTRole;
import org.direct_bt.BTSecurityLevel;
import org.direct_bt.BTAdapter;
import org.direct_bt.BTException;
import org.direct_bt.BTFactory;
import org.direct_bt.BTManager;
import org.direct_bt.BTUtils;
import org.direct_bt.HCIStatusCode;
import org.jau.net.EUI48;
import org.junit.Assert;
import org.junit.BeforeClass;
import org.junit.FixMethodOrder;
import org.junit.Test;
import org.junit.runners.MethodSorters;

/**
 * Basic client and server Bluetooth tests, requiring one BT adapter.
 */
@FixMethodOrder(MethodSorters.NAME_ASCENDING)
public class TestDBTClientServer00 extends BaseDBTClientServer {
    static final boolean DEBUG = false;

    @BeforeClass
    public static final void setupAllLocal() {
        BTFactory.initDirectBTLibrary();

        final Class<?> ThisClazz = MethodHandles.lookup().lookupClass();
        BTUtils.println(System.err, "++++ Test "+ThisClazz.getSimpleName()+".setupAllLocal()");

        DBTUtils.printVersionInfo();
    }

    /**
     * Testing BTManager bring up and
     * - test that at least two adapter are present
     * - validating basic default adapter status
     */
    @Test(timeout = 5000)
    public final void test01_ManagerBringup() {
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

    /**
     * Testing start and stop advertising (server mode) using a full DBGattServer,
     * having the adapter in BTRole::Slave.
     *
     * Thereafter start and stop discovery (client mode),
     * having the adapter in BTRole::Client.
     */
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

        final BTManager.ChangedAdapterSetListener myChangedAdapterSetListener =
            new BTManager.ChangedAdapterSetListener() {
                @Override
                public void adapterAdded(final BTAdapter adapter) {
                    if( null == server.getAdapter() ) {
                        if( server.initAdapter( adapter ) ) {
                            server.setAdapter(adapter);
                            BTUtils.println(System.err, "****** Adapter-Server ADDED__: InitOK: " + adapter);
                            return;
                        }
                    }
                    BTUtils.println(System.err, "****** Adapter ADDED__: Ignored: " + adapter);
                }

                @Override
                public void adapterRemoved(final BTAdapter adapter) {
                    if( null != server.getAdapter() && adapter == server.getAdapter() ) {
                        server.setAdapter(null);
                        BTUtils.println(System.err, "****** Adapter-Server REMOVED: " + adapter);
                        return;
                    }
                    BTUtils.println(System.err, "****** Adapter REMOVED: Ignored " + adapter);
                }
            };

        manager.addChangedAdapterSetListener(myChangedAdapterSetListener);
        Assert.assertNotNull("No server adapter found", server.getAdapter());

        //
        // Server start
        //
        Assert.assertTrue( server.getAdapter().isInitialized() );
        Assert.assertTrue( server.getAdapter().isPowered() );
        Assert.assertEquals( BTRole.Master, server.getAdapter().getRole() );
        Assert.assertTrue( 4 <= server.getAdapter().getBTMajorVersion() );
        {
            Assert.assertFalse(server.getAdapter().isAdvertising());
            Assert.assertFalse(server.getAdapter().isDiscovering());

            Assert.assertEquals( HCIStatusCode.SUCCESS, server.startAdvertising(server.getAdapter(), "test10_startAdvertising") );
            Assert.assertTrue(server.getAdapter().isAdvertising());
            Assert.assertFalse(server.getAdapter().isDiscovering());
            Assert.assertEquals( BTRole.Slave, server.getAdapter().getRole() );
            Assert.assertEquals( serverName, server.getAdapter().getName() );
        }

        //
        // Server stop
        //
        {
            Assert.assertTrue(server.getAdapter().isAdvertising());
            Assert.assertFalse(server.getAdapter().isDiscovering());

            Assert.assertEquals( HCIStatusCode.SUCCESS, server.stopAdvertising(server.getAdapter(), "test10_stopAdvertising") );
            Assert.assertFalse(server.getAdapter().isAdvertising());
            Assert.assertFalse(server.getAdapter().isDiscovering());
            Assert.assertEquals( BTRole.Slave, server.getAdapter().getRole() ); // keeps role
        }

        //
        // Now reuse adapter for client mode -> Start discovery + Stop Discovery
        //
        {
            Assert.assertFalse(server.getAdapter().isAdvertising());
            Assert.assertFalse(server.getAdapter().isDiscovering());

            final BTAdapter adapter = server.getAdapter();
            {
                final int r = adapter.removeAllStatusListener();
                Assert.assertTrue("Not > 0 removed listener, but "+r, 0 < r );
            }

            Assert.assertEquals( HCIStatusCode.SUCCESS, adapter.startDiscovery() ); // pending action
            while( !adapter.isDiscovering() ) {
                try { Thread.sleep(100); } catch (final InterruptedException e) { e.printStackTrace(); }
            }
            Assert.assertFalse(server.getAdapter().isAdvertising());
            Assert.assertTrue(server.getAdapter().isDiscovering());
            Assert.assertEquals( BTRole.Master, server.getAdapter().getRole() ); // changed role

            Assert.assertEquals( HCIStatusCode.SUCCESS, adapter.stopDiscovery() ); // pending action
            while( adapter.isDiscovering() ) {
                try { Thread.sleep(100); } catch (final InterruptedException e) { e.printStackTrace(); }
            }
            Assert.assertFalse(server.getAdapter().isAdvertising());
            Assert.assertFalse(server.getAdapter().isDiscovering());
            Assert.assertEquals( BTRole.Master, server.getAdapter().getRole() ); // keeps role

        }

        final int count = manager.removeChangedAdapterSetListener(myChangedAdapterSetListener);
        BTUtils.println(System.err, "****** EOL Removed ChangedAdapterSetCallback " + count);
    }

    public static void main(final String args[]) {
        org.junit.runner.JUnitCore.main(TestDBTClientServer00.class.getName());
    }
}
