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

import org.direct_bt.BTSecurityLevel;
import org.junit.FixMethodOrder;
import org.junit.Test;
import org.junit.runners.MethodSorters;

/**
 * Testing a full Bluetooth server and client lifecycle of operations, requiring two BT adapter:
 * - operating in legacy non SC mode
 * - start server advertising
 * - start client discovery and connect to server when discovered
 * - client/server processing of connection when ready
 * - client disconnect
 * - server stop advertising
 * - security-level: NONE, ENC_ONLY freshly-paired and ENC_ONLY pre-paired
 * - reuse server-adapter for client-mode discovery (just toggle on/off)
 */
@FixMethodOrder(MethodSorters.NAME_ASCENDING)
public class TestDBTClientServer11_SC0 extends DBTClientServer1x {
    static final boolean serverSC = false;

    // @Test(timeout = 30000)
    public final void test01_FullCycle_EncNone() {
        final boolean serverShallHaveKeys = false;
        final boolean clientShallHaveKeys = false;
        test8x_fullCycle("01", false /* server_client_order */, serverSC,
                         BTSecurityLevel.NONE, serverShallHaveKeys, BTSecurityLevel.NONE, clientShallHaveKeys);
    }

    @Test(timeout = 40000)
    public final void test11_FullCycle_EncOnlyNo1() {
        final boolean serverShallHaveKeys = false;
        final boolean clientShallHaveKeys = false;
        test8x_fullCycle("11", false /* server_client_order */, serverSC,
                         BTSecurityLevel.ENC_ONLY, serverShallHaveKeys, BTSecurityLevel.ENC_ONLY, clientShallHaveKeys);
    }

    @Test(timeout = 40000)
    public final void test21_FullCycle_EncOnlyNo2() {
        final boolean serverShallHaveKeys = true;
        final boolean clientShallHaveKeys = true;
        test8x_fullCycle("21", false /* server_client_order */, serverSC,
                         BTSecurityLevel.ENC_ONLY, serverShallHaveKeys, BTSecurityLevel.ENC_ONLY, clientShallHaveKeys);
    }

    public static void main(final String args[]) {
        org.junit.runner.JUnitCore.main(TestDBTClientServer11_SC0.class.getName());
    }
}
