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

import org.direct_bt.BTMode;
import org.direct_bt.BTSecurityLevel;
import org.direct_bt.DiscoveryPolicy;
import org.jau.net.EUI48;
import org.junit.FixMethodOrder;
import org.junit.Test;
import org.junit.runners.MethodSorters;

/**
 * Testing a full Bluetooth server and client lifecycle of operations including adapter reset, requiring two BT adapter:
 * - trigger client adapter reset
 * - operating w/o encryption
 * - start server advertising
 * - start client discovery and connect to server when discovered
 * - client/server processing of connection when ready
 * - client disconnect
 * - server stop advertising
 * - security-level: NONE, ENC_ONLY freshly-paired and ENC_ONLY pre-paired
 * - reuse server-adapter for client-mode discovery (just toggle on/off)
 */
@FixMethodOrder(MethodSorters.NAME_ASCENDING)
public class TestDBTClientServer40_Reset extends DBTClientServer1x {
    static final boolean serverSC = true;

    @Test(timeout = 20000)
    public final void test40_ClientReset01() {
        final String suffix = "40";
        final int protocolSessionCount = 1;
        final int max_connections_per_session = DBTConstants.max_connections_per_session;
        final boolean expSuccess = true;
        final boolean server_client_order = true;
        final BTSecurityLevel secLevelServer = BTSecurityLevel.NONE;
        final BTSecurityLevel secLevelClient = BTSecurityLevel.NONE;
        final ExpectedPairing serverExpPairing = ExpectedPairing.DONT_CARE;
        final ExpectedPairing clientExpPairing = ExpectedPairing.DONT_CARE;

        final DBTServerTest server = new DBTServer01("S-"+suffix, EUI48.ALL_DEVICE, BTMode.DUAL, serverSC, secLevelServer);
        final DBTClientTest client = new DBTClient01("C-"+suffix, EUI48.ALL_DEVICE, BTMode.DUAL);

        server.setProtocolSessionsLeft( protocolSessionCount );

        client.setProtocolSessionsLeft( protocolSessionCount );
        client.setDisconnectDeviceed( true ); // default, auto-disconnect after work is done
        client.setRemoveDevice( false ); // default, test side-effects
        client.setDiscoveryPolicy( DiscoveryPolicy.PAUSE_CONNECTED_UNTIL_DISCONNECTED );

        set_client_reset_at_ready(true);

        test8x_fullCycle(20000, suffix,
                         max_connections_per_session, expSuccess, server_client_order,
                         server, secLevelServer, serverExpPairing,
                         client, secLevelClient, clientExpPairing);
    }

    public static void main(final String args[]) {
        org.junit.runner.JUnitCore.main(TestDBTClientServer40_Reset.class.getName());
    }
}
