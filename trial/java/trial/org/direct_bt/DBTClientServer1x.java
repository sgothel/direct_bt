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
import java.util.Arrays;
import java.util.List;

import org.direct_bt.BTMode;
import org.direct_bt.BTSecurityLevel;
import org.direct_bt.AdapterSettings;
import org.direct_bt.AdapterStatusListener;
import org.direct_bt.BTAdapter;
import org.direct_bt.BTDevice;
import org.direct_bt.BTDeviceRegistry;
import org.direct_bt.BTException;
import org.direct_bt.BTFactory;
import org.direct_bt.BTManager;
import org.direct_bt.BTSecurityRegistry;
import org.direct_bt.DiscoveryPolicy;
import org.direct_bt.EIRDataTypeSet;
import org.direct_bt.EInfoReport;
import org.direct_bt.HCIStatusCode;
import org.direct_bt.PairingMode;
import org.direct_bt.SMPKeyBin;
import org.jau.io.PrintUtil;
import org.jau.net.EUI48;
import org.jau.sys.Clock;
import org.junit.Assert;

/**
 * Testing a full Bluetooth server and client lifecycle of operations, requiring two BT adapter:
 * - start server advertising
 * - start client discovery and connect to server when discovered
 * - client/server processing of connection when ready
 * - client disconnect
 * - server stop advertising
 * - security-level: NONE, ENC_ONLY freshly-paired and ENC_ONLY pre-paired
 * - reuse server-adapter for client-mode discovery (just toggle on/off)
 */
public abstract class DBTClientServer1x extends BaseDBTClientServer {
    // timeout check: timeout_value < test_duration + timeout_preempt_diff; // let's timeout here before our timeout timer
    static final long timeout_preempt_diff = 500;

    final Object mtx_sync = new Object();
    BTDevice lastCompletedDevice = null;
    PairingMode lastCompletedDevicePairingMode = PairingMode.NONE;
    BTSecurityLevel lastCompletedDeviceSecurityLevel = BTSecurityLevel.NONE;
    EInfoReport lastCompletedDeviceEIR = null;

    final void test8x_fullCycle(final long timeout_value,
                                final String suffix, final int protocolSessionCount, final boolean server_client_order,
                                final boolean serverSC, final BTSecurityLevel secLevelServer, final ExpectedPairing serverExpPairing,
                                final BTSecurityLevel secLevelClient, final ExpectedPairing clientExpPairing)
    {
        final DBTServer01 server = new DBTServer01("S-"+suffix, EUI48.ALL_DEVICE, BTMode.DUAL, serverSC, secLevelServer);
        final DBTClient01 client = new DBTClient01("C-"+suffix, EUI48.ALL_DEVICE, BTMode.DUAL);

        server.setProtocolSessionsLeft( protocolSessionCount );

        client.setProtocolSessionsLeft( protocolSessionCount );
        client.setDisconnectDeviceed( true ); // default, auto-disconnect after work is done
        client.setRemoveDevice( false ); // default, test side-effects
        client.setDiscoveryPolicy( DiscoveryPolicy.PAUSE_CONNECTED_UNTIL_DISCONNECTED );

        test8x_fullCycle(timeout_value, suffix,
                         DBTConstants.max_connections_per_session, true /* expSuccess */, server_client_order,
                         server,
                         secLevelServer, serverExpPairing, client,
                         secLevelClient, clientExpPairing);
    }

    final void test8x_fullCycle(final long timeout_value, final String suffix,
                                final int max_connections_per_session, final boolean expSuccess, final boolean server_client_order,
                                final DBTServerTest server,
                                final BTSecurityLevel secLevelServer, final ExpectedPairing serverExpPairing, final DBTClientTest client,
                                final BTSecurityLevel secLevelClient, final ExpectedPairing clientExpPairing)
    {
        final int protocolSessionCount = Math.min(server.getProtocolSessionsLeft(), client.getProtocolSessionsLeft());
        final long t0 = Clock.currentTimeMillis();

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
        {
            final List<BTAdapter> adapters = manager.getAdapters();
            PrintUtil.println(System.err, "Adapter: Count "+adapters.size()+": "+adapters.toString());
            Assert.assertTrue("Adapter count not >= 2 but "+adapters.size(), adapters.size() >= 2);
        }

        final DBTEndpoint.ChangedAdapterSetListener myChangedAdapterSetListener =
                DBTEndpoint.initChangedAdapterSetListener(manager, server_client_order ? Arrays.asList(server, client) : Arrays.asList(client, server));

        final String serverName = server.getName();
        BTDeviceRegistry.addToWaitForDevices( serverName );
        {
            final BTSecurityRegistry.Entry sec = BTSecurityRegistry.getOrCreate(serverName);
            sec.sec_level = secLevelClient;
        }

        synchronized( mtx_sync ) {
            lastCompletedDevice = null;
            lastCompletedDevicePairingMode = PairingMode.NONE;
            lastCompletedDeviceSecurityLevel = BTSecurityLevel.NONE;
            lastCompletedDeviceEIR = null;
        }
        final AdapterStatusListener clientAdapterStatusListener = new AdapterStatusListener() {
            @Override
            public void deviceReady(final BTDevice device, final long timestamp) {
                synchronized( mtx_sync ) {
                    lastCompletedDevice = device;
                    lastCompletedDevicePairingMode = device.getPairingMode();
                    lastCompletedDeviceSecurityLevel = device.getConnSecurityLevel();
                    lastCompletedDeviceEIR = device.getEIR().clone();
                    PrintUtil.println(System.err, "XXXXXX Client Ready: "+device);
                }
            }
        };
        Assert.assertTrue( client.getAdapter().addStatusListener(clientAdapterStatusListener) );

        //
        // Server start
        //
        DBTEndpoint.checkInitializedState(server);
        DBTServerTest.startAdvertising(server, false /* current_exp_advertising_state */, "test"+suffix+"_startAdvertising");

        //
        // Client start
        //
        DBTEndpoint.checkInitializedState(client);
        DBTClientTest.startDiscovery(client, false /* current_exp_discovering_state */, "test"+suffix+"_startDiscovery");

        long test_duration = 0;
        boolean done = false;
        boolean timeout = false;
        boolean max_connections_hit = false;
        do {
            synchronized( mtx_sync ) {
                done = ! ( protocolSessionCount > server.getProtocolSessionsDoneSuccess() ||
                           protocolSessionCount > client.getProtocolSessionsDoneSuccess() ||
                           null == lastCompletedDevice ||
                           lastCompletedDevice.getConnected() );
            }
            max_connections_hit = ( protocolSessionCount * max_connections_per_session ) <= server.getDisconnectCount();
            test_duration = Clock.currentTimeMillis() - t0;
            timeout = 0 < timeout_value && timeout_value <= test_duration + timeout_preempt_diff; // let's timeout here before our timeout timer
            if( !done && !max_connections_hit && !timeout ) {
                try { Thread.sleep(88); } catch (final InterruptedException e) { e.printStackTrace(); }
            }
        } while( !done && !max_connections_hit && !timeout );
        test_duration = Clock.currentTimeMillis() - t0;

        PrintUtil.fprintf_td(System.err, "\n\n");
        PrintUtil.fprintf_td(System.err, "****** Test Stats: duration %d ms, timeout[hit %b, value %d ms], max_connections hit %b\n",
                test_duration, timeout, timeout_value, max_connections_hit);
        PrintUtil.fprintf_td(System.err, "  Server ProtocolSessions[success %d/%d total, requested %d], disconnects %d of %d max\n",
                server.getProtocolSessionsDoneSuccess(), server.getProtocolSessionsDoneTotal(), protocolSessionCount,
                server.getDisconnectCount(), ( protocolSessionCount * max_connections_per_session ));
        PrintUtil.fprintf_td(System.err, "  Client ProtocolSessions[success %d/%d total, requested %d], disconnects %d of %d max\n",
                client.getProtocolSessionsDoneSuccess(), client.getProtocolSessionsDoneTotal(), protocolSessionCount,
                client.getDisconnectCount(), ( protocolSessionCount * max_connections_per_session ));
        PrintUtil.fprintf_td(System.err, "\n\n");

        if( expSuccess ) {
            Assert.assertFalse( max_connections_hit );
            Assert.assertFalse( timeout );

            synchronized( mtx_sync ) {
                Assert.assertTrue(protocolSessionCount <= server.getProtocolSessionsDoneTotal());
                Assert.assertEquals(protocolSessionCount, server.getProtocolSessionsDoneSuccess());
                Assert.assertTrue(protocolSessionCount <= client.getProtocolSessionsDoneTotal());
                Assert.assertEquals(protocolSessionCount, client.getProtocolSessionsDoneSuccess());
                Assert.assertNotNull(lastCompletedDevice);
                Assert.assertNotNull(lastCompletedDeviceEIR);
                Assert.assertFalse(lastCompletedDevice.getConnected());
                Assert.assertTrue( ( 1 * max_connections_per_session ) > server.getDisconnectCount() );
            }
        }

        //
        // Client stop
        //
        final boolean current_exp_discovering_state = expSuccess ? true : client.getAdapter().isDiscovering();
        DBTClientTest.stopDiscovery(client, current_exp_discovering_state, "test"+suffix+"_stopDiscovery");
        client.close("test"+suffix+"_close");

        //
        // Server stop
        //
        DBTServerTest.stop(server, "test"+suffix+"_stopAdvertising");

        if( expSuccess ) {
            //
            // Validating Security Mode
            //
            final SMPKeyBin clientKeys = SMPKeyBin.read(DBTConstants.CLIENT_KEY_PATH, lastCompletedDevice, true /* verbose */);
            Assert.assertTrue(clientKeys.isValid());
            final BTSecurityLevel clientKeysSecLevel = clientKeys.getSecLevel();
            Assert.assertEquals(secLevelClient, clientKeysSecLevel);
            {
                if( ExpectedPairing.PREPAIRED == clientExpPairing && BTSecurityLevel.NONE.value < secLevelClient.value ) {
                    // Using encryption: pre-paired
                    Assert.assertEquals(PairingMode.PRE_PAIRED, lastCompletedDevicePairingMode);
                    Assert.assertEquals(BTSecurityLevel.ENC_ONLY, lastCompletedDeviceSecurityLevel); // pre-paired fixed level, no auth
                } else if( ExpectedPairing.NEW_PAIRING == clientExpPairing && BTSecurityLevel.NONE.value < secLevelClient.value ) {
                    // Using encryption: Newly paired
                    Assert.assertNotEquals(PairingMode.PRE_PAIRED, lastCompletedDevicePairingMode);
                    Assert.assertTrue("PairingMode client "+lastCompletedDevicePairingMode+" not > NONE", PairingMode.NONE.value < lastCompletedDevicePairingMode.value);
                    Assert.assertTrue("SecurityLevel client "+lastCompletedDeviceSecurityLevel+" not >= "+secLevelClient, secLevelClient.value <= lastCompletedDeviceSecurityLevel.value);
                } else if( ExpectedPairing.DONT_CARE == clientExpPairing && BTSecurityLevel.NONE.value < secLevelClient.value ) {
                    // Any encryption, any pairing
                    Assert.assertTrue("PairingMode client "+lastCompletedDevicePairingMode+" not > NONE", PairingMode.NONE.value < lastCompletedDevicePairingMode.value);
                    Assert.assertTrue("SecurityLevel client "+lastCompletedDeviceSecurityLevel+" not >= "+secLevelClient, secLevelClient.value <= lastCompletedDeviceSecurityLevel.value);
                } else {
                    // No encryption: No pairing
                    Assert.assertEquals(PairingMode.NONE, lastCompletedDevicePairingMode);
                    Assert.assertEquals(BTSecurityLevel.NONE, lastCompletedDeviceSecurityLevel);
                }
            }

            //
            // Validating EIR
            //
            synchronized( mtx_sync ) {
                PrintUtil.println(System.err, "lastCompletedDevice.connectedEIR: "+lastCompletedDeviceEIR.toString());
                Assert.assertNotEquals(0, lastCompletedDeviceEIR.getEIRDataMask().mask);
                Assert.assertTrue( lastCompletedDeviceEIR.isSet(EIRDataTypeSet.DataType.FLAGS) );
                Assert.assertTrue( lastCompletedDeviceEIR.isSet(EIRDataTypeSet.DataType.SERVICE_UUID) );
                Assert.assertTrue( lastCompletedDeviceEIR.isSet(EIRDataTypeSet.DataType.NAME) );
                Assert.assertTrue( lastCompletedDeviceEIR.isSet(EIRDataTypeSet.DataType.CONN_IVAL) );
                Assert.assertEquals(serverName, lastCompletedDeviceEIR.getName());
                {
                    final EInfoReport eir = lastCompletedDevice.getEIR().clone();
                    PrintUtil.println(System.err, "lastCompletedDevice.currentEIR: "+eir.toString());
                    Assert.assertEquals(0, eir.getEIRDataMask().mask);
                    Assert.assertEquals(0, eir.getName().length());
                }
            }

            //
            // Now reuse adapter for client mode -> Start discovery + Stop Discovery
            //
            {
                final BTAdapter adapter = server.getAdapter();
                { // if( false ) {
                    adapter.removeAllStatusListener();
                }

                DBTEndpoint.startDiscovery(adapter, false /* current_exp_discovering_state */);

                DBTEndpoint.stopDiscovery(adapter, true /* current_exp_discovering_state */);
            }
        }
        final int count = manager.removeChangedAdapterSetListener(myChangedAdapterSetListener);
        PrintUtil.println(System.err, "****** EOL Removed ChangedAdapterSetCallback " + count);
    }
}
