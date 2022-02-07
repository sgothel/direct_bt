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

import java.lang.reflect.InvocationTargetException;
import java.util.List;

import org.direct_bt.BTMode;
import org.direct_bt.BTRole;
import org.direct_bt.BTSecurityLevel;
import org.direct_bt.AdapterStatusListener;
import org.direct_bt.BTAdapter;
import org.direct_bt.BTDevice;
import org.direct_bt.BTDeviceRegistry;
import org.direct_bt.BTException;
import org.direct_bt.BTFactory;
import org.direct_bt.BTManager;
import org.direct_bt.BTSecurityRegistry;
import org.direct_bt.BTUtils;
import org.direct_bt.DiscoveryPolicy;
import org.direct_bt.EIRDataTypeSet;
import org.direct_bt.EInfoReport;
import org.direct_bt.HCIStatusCode;
import org.direct_bt.PairingMode;
import org.direct_bt.SMPKeyBin;
import org.jau.net.EUI48;
import org.junit.Assert;
import org.junit.FixMethodOrder;
import org.junit.Test;
import org.junit.runners.MethodSorters;

/**
 * Testing a full Bluetooth server and client lifecycle of operations, requiring two BT adapter:
 * - start server advertising
 * - start client discovery and connect to server when discovered
 * - client/server processing of connection when ready
 * - client disconnect
 * - server stop advertising
 * - security-level: NONE, ENC_ONLY freshly-paired and ENC_ONLY pre-paired
 */
@FixMethodOrder(MethodSorters.NAME_ASCENDING)
public class TestDBTClientServer10 extends BaseDBTClientServer {
    static final boolean DEBUG = false;

    @Test(timeout = 20000)
    public final void test00_FullCycle_EncNone() {
        test8x_fullCycle("00", BTSecurityLevel.NONE, false /* serverShallHaveKeys */, BTSecurityLevel.NONE, false /* clientShallHaveKeys */);
    }

    @Test(timeout = 20000)
    public final void test01_FullCycle_EncOnlyNo1() {
        test8x_fullCycle("01", BTSecurityLevel.ENC_ONLY, false /* serverShallHaveKeys */, BTSecurityLevel.ENC_ONLY, false /* clientShallHaveKeys */);
    }

    @Test(timeout = 30000)
    public final void test02_FullCycle_EncOnlyNo2() {
        test8x_fullCycle("02", BTSecurityLevel.ENC_ONLY, true /* serverShallHaveKeys */, BTSecurityLevel.ENC_ONLY, true /* clientShallHaveKeys */);
    }

    volatile BTDevice lastCompletedDevice = null;
    volatile PairingMode lastCompletedDevicePairingMode = PairingMode.NONE;
    volatile BTSecurityLevel lastCompletedDeviceSecurityLevel = BTSecurityLevel.NONE;
    volatile EInfoReport lastCompletedDeviceEIR = null;

    final void test8x_fullCycle(final String suffix,
                                final BTSecurityLevel secLevelServer, final boolean serverShallHaveKeys,
                                final BTSecurityLevel secLevelClient, final boolean clientShallHaveKeys)
    {
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
        Assert.assertTrue("Adapter count not >= 2 but "+adapters.size(), adapters.size() >= 2);

        final String serverName = "TestDBTCS00-T"+suffix;
        final DBTServer00 server = new DBTServer00(EUI48.ALL_DEVICE, BTMode.DUAL, true /* SC */, serverName, secLevelServer);

        final DBTClient00 client = new DBTClient00(EUI48.ALL_DEVICE, BTMode.DUAL);
        BTDeviceRegistry.addToWaitForDevices( serverName );
        {
            final BTSecurityRegistry.Entry sec = BTSecurityRegistry.getOrCreate(serverName);
            sec.sec_level = secLevelClient;
        }
        client.KEEP_CONNECTED = false; // default
        client.REMOVE_DEVICE = false; // default and test side-effects
        client.measurementsLeft.set(1);
        client.discoveryPolicy = DiscoveryPolicy.PAUSE_CONNECTED_UNTIL_DISCONNECTED;

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
                    if( null == client.getAdapter() ) {
                        if( client.initAdapter( adapter ) ) {
                            client.setAdapter(adapter);
                            BTUtils.println(System.err, "****** Adapter-Client ADDED__: InitOK: " + adapter);
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
                    if( null != client.getAdapter() && adapter == client.getAdapter() ) {
                        client.setAdapter(null);
                        BTUtils.println(System.err, "****** Adapter-Client REMOVED: " + adapter);
                        return;
                    }
                    BTUtils.println(System.err, "****** Adapter REMOVED: Ignored " + adapter);
                }
            };

        manager.addChangedAdapterSetListener(myChangedAdapterSetListener);
        Assert.assertNotNull("No server adapter found", server.getAdapter());
        Assert.assertNotNull("No client adapter found", client.getAdapter());

        lastCompletedDevice = null;
        lastCompletedDevicePairingMode = PairingMode.NONE;
        lastCompletedDeviceSecurityLevel = BTSecurityLevel.NONE;
        lastCompletedDeviceEIR = null;
        final AdapterStatusListener clientAdapterStatusListener = new AdapterStatusListener() {
            @Override
            public void deviceReady(final BTDevice device, final long timestamp) {
                lastCompletedDevice = device;
                lastCompletedDevicePairingMode = device.getPairingMode();
                lastCompletedDeviceSecurityLevel = device.getConnSecurityLevel();
                lastCompletedDeviceEIR = device.getEIR();
                BTUtils.println(System.err, "XXXXXX Client Ready: "+device);
            }
        };
        Assert.assertTrue( client.getAdapter().addStatusListener(clientAdapterStatusListener) );

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

            Assert.assertEquals( HCIStatusCode.SUCCESS, server.startAdvertising(server.getAdapter(), "test"+suffix+"_startAdvertising") );
            Assert.assertTrue(server.getAdapter().isAdvertising());
            Assert.assertFalse(server.getAdapter().isDiscovering());
            Assert.assertEquals( BTRole.Slave, server.getAdapter().getRole() );
            Assert.assertEquals( serverName, server.getAdapter().getName() );
        }

        //
        // Client start
        //
        Assert.assertTrue( client.getAdapter().isInitialized() );
        Assert.assertTrue( client.getAdapter().isPowered() );
        Assert.assertEquals( BTRole.Master, client.getAdapter().getRole() );
        Assert.assertTrue( 4 <= client.getAdapter().getBTMajorVersion() );
        {
            Assert.assertFalse(client.getAdapter().isAdvertising());
            Assert.assertFalse(client.getAdapter().isDiscovering());

            Assert.assertEquals( HCIStatusCode.SUCCESS, client.startDiscovery(client.getAdapter(), "test"+suffix+"_startDiscovery") );
            Assert.assertFalse(client.getAdapter().isAdvertising());
            Assert.assertTrue(client.getAdapter().isDiscovering());
            Assert.assertEquals( BTRole.Master, client.getAdapter().getRole() );
        }

        while( 1 > server.servedConnections.get() ||
               1 > client.completedMeasurements.get() ||
               null == lastCompletedDevice ||
               lastCompletedDevice.getConnected() )
        {
            try { Thread.sleep(500); } catch (final InterruptedException e) { e.printStackTrace(); }
        }
        Assert.assertEquals(1, server.servedConnections.get());
        Assert.assertEquals(1, client.completedMeasurements.get());
        Assert.assertNotNull(lastCompletedDevice);
        Assert.assertNotNull(lastCompletedDeviceEIR);
        Assert.assertFalse(lastCompletedDevice.getConnected());
        Assert.assertEquals( HCIStatusCode.SUCCESS, client.stopDiscovery(client.getAdapter(), "test"+suffix+"_stopDiscovery") );

        {
            Assert.assertFalse(server.getAdapter().isAdvertising()); // stopped by connection
            Assert.assertFalse(server.getAdapter().isDiscovering());

            // Stopping advertising wven if stopped must be OK!
            Assert.assertEquals( HCIStatusCode.SUCCESS, server.stopAdvertising(server.getAdapter(), "test"+suffix+"_stopAdvertising") );
            Assert.assertFalse(server.getAdapter().isAdvertising());
            Assert.assertFalse(server.getAdapter().isDiscovering());
            Assert.assertEquals( BTRole.Slave, server.getAdapter().getRole() ); // kept
        }

        final SMPKeyBin clientKeys = SMPKeyBin.read(DBTConstants.CLIENT_KEY_PATH, lastCompletedDevice, true /* verbose */);
        Assert.assertTrue(clientKeys.isValid());
        final BTSecurityLevel clientKeysSecLevel = clientKeys.getSecLevel();
        Assert.assertEquals(secLevelClient, clientKeysSecLevel);
        {
            if( clientShallHaveKeys ) {
                // Using encryption: pre-paired
                Assert.assertEquals(PairingMode.PRE_PAIRED, lastCompletedDevicePairingMode);
                Assert.assertEquals(BTSecurityLevel.ENC_ONLY, lastCompletedDeviceSecurityLevel); // pre-paired fixed level, no auth
            } else if( BTSecurityLevel.NONE.value < secLevelClient.value ) {
                // Using encryption: Newly paired
                Assert.assertNotEquals(PairingMode.PRE_PAIRED, lastCompletedDevicePairingMode);
                Assert.assertTrue("PairingMode client "+lastCompletedDevicePairingMode+" not > NONE", PairingMode.NONE.value < lastCompletedDevicePairingMode.value);
                Assert.assertTrue("SecurityLevel client "+lastCompletedDeviceSecurityLevel+" not >= "+secLevelClient, secLevelClient.value <= lastCompletedDeviceSecurityLevel.value);
            } else {
                // No encryption: No pairing
                Assert.assertEquals(PairingMode.NONE, lastCompletedDevicePairingMode);
                Assert.assertEquals(BTSecurityLevel.NONE, lastCompletedDeviceSecurityLevel);
            }
        }

        Assert.assertNotEquals(0, lastCompletedDeviceEIR.getEIRDataMask().mask);
        Assert.assertTrue( lastCompletedDeviceEIR.isSet(EIRDataTypeSet.DataType.FLAGS) );
        Assert.assertTrue( lastCompletedDeviceEIR.isSet(EIRDataTypeSet.DataType.SERVICE_UUID) );
        Assert.assertTrue( lastCompletedDeviceEIR.isSet(EIRDataTypeSet.DataType.NAME) );
        Assert.assertTrue( lastCompletedDeviceEIR.isSet(EIRDataTypeSet.DataType.CONN_IVAL) );
        Assert.assertEquals(serverName, lastCompletedDeviceEIR.getName());
        {
            final EInfoReport eir = lastCompletedDevice.getEIR();
            Assert.assertEquals(0, eir.getEIRDataMask().mask);
            Assert.assertEquals(0, lastCompletedDeviceEIR.getName().length());
        }

        final int count = manager.removeChangedAdapterSetListener(myChangedAdapterSetListener);
        BTUtils.println(System.err, "****** EOL Removed ChangedAdapterSetCallback " + count);
    }

    public static void main(final String args[]) {
        org.junit.runner.JUnitCore.main(TestDBTClientServer10.class.getName());
    }
}
