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
import org.jau.net.EUI48;
import org.junit.FixMethodOrder;
import org.junit.Test;
import org.junit.runners.MethodSorters;

/**
 * Testing w/o client filtering processing device and hence not blocking deviceFound.
 *
 * In other words, relying on BTAdapter to filter out:
 * - already discovered devices
 * - already connected devices
 *
 * Further, the server will issue a disconnect once only 300 ms after 1st MTU exchange,
 * disrupting the client's getGATTServices().
 */
@FixMethodOrder(MethodSorters.NAME_ASCENDING)
public class TestDBTProvokeClientServer_i470 extends DBTClientServer1x {

    @Test(timeout = 10000)
    public final void test_i470_a()
    {
        final boolean serverSC = true;
        final String suffix = "i470_a";
        final int protocolSessionCount = 10;
        final int max_connections_per_session = 200;
        final boolean expSuccess = false;
        final boolean server_client_order = true;
        final ExpectedPairing serverExpPairing = ExpectedPairing.DONT_CARE;
        final ExpectedPairing clientExpPairing = ExpectedPairing.DONT_CARE;
        final boolean client_do_disconnect = true;
        final boolean server_do_disconnect = false;

        final DBTServerTest server = new DBTServer01("S-"+suffix, EUI48.ALL_DEVICE, BTMode.DUAL, serverSC, BTSecurityLevel.ENC_ONLY, server_do_disconnect);
        final DBTClientTest client = new DBTClient01("C-"+suffix, EUI48.ALL_DEVICE, BTMode.DUAL, client_do_disconnect);

        test8x_fullCycle(10000, suffix,
                         protocolSessionCount, max_connections_per_session, expSuccess,
                         server_client_order,
                         server, BTSecurityLevel.ENC_ONLY, serverExpPairing,
                         client, BTSecurityLevel.ENC_ONLY, clientExpPairing);
    }

    @Test(timeout = 10000)
    public final void test_i470_b()
    {
        final boolean serverSC = true;
        final String suffix = "i470_b";
        final int protocolSessionCount = 10;
        final int max_connections_per_session = 200;
        final boolean expSuccess = false;
        final boolean server_client_order = true;
        final ExpectedPairing serverExpPairing = ExpectedPairing.DONT_CARE;
        final ExpectedPairing clientExpPairing = ExpectedPairing.DONT_CARE;
        final boolean client_do_disconnect = false;
        final boolean server_do_disconnect = true;

        final DBTServerTest server = new DBTServer01("S-"+suffix, EUI48.ALL_DEVICE, BTMode.DUAL, serverSC, BTSecurityLevel.ENC_ONLY, server_do_disconnect);
        final DBTClientTest client = new DBTClient01("C-"+suffix, EUI48.ALL_DEVICE, BTMode.DUAL, client_do_disconnect);

        test8x_fullCycle(10000, suffix,
                         protocolSessionCount, max_connections_per_session, expSuccess,
                         server_client_order,
                         server, BTSecurityLevel.ENC_ONLY, serverExpPairing,
                         client, BTSecurityLevel.ENC_ONLY, clientExpPairing);
    }

    public static void main(final String args[]) {
        org.junit.runner.JUnitCore.main(TestDBTProvokeClientServer_i470.class.getName());
    }
}
