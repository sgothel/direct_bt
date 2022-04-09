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

import org.direct_bt.BTAdapter;
import org.direct_bt.BTRole;
import org.direct_bt.BTSecurityLevel;
import org.direct_bt.HCIStatusCode;
import org.junit.Assert;

public interface DBTServerTest extends DBTEndpoint {

    BTSecurityLevel getSecurityLevel();

    HCIStatusCode stopAdvertising(BTAdapter adapter, String msg);

    HCIStatusCode startAdvertising(BTAdapter adapter, String msg);

    public static void startAdvertising(final DBTServerTest server, final boolean current_exp_advertising_state, final String msg) {
        final BTAdapter adapter = server.getAdapter();
        Assert.assertEquals(current_exp_advertising_state, adapter.isAdvertising());
        Assert.assertFalse(adapter.isDiscovering());

        Assert.assertEquals( HCIStatusCode.SUCCESS, server.startAdvertising(adapter, msg) );
        Assert.assertTrue(adapter.isAdvertising());
        Assert.assertFalse(adapter.isDiscovering());
        Assert.assertEquals( BTRole.Slave, adapter.getRole() );
        Assert.assertEquals( server.getName(), adapter.getName() );

    }

    public static void stopAdvertising(final DBTServerTest server, final boolean current_exp_advertising_state, final String msg) {
        final BTAdapter adapter = server.getAdapter();
        Assert.assertEquals(current_exp_advertising_state, adapter.isAdvertising());
        Assert.assertFalse(adapter.isDiscovering());
        Assert.assertEquals( BTRole.Slave, adapter.getRole() ); // kept

        // Stopping advertising even if stopped must be OK!
        Assert.assertEquals( HCIStatusCode.SUCCESS, server.stopAdvertising(adapter, msg) );
        Assert.assertFalse(adapter.isAdvertising());
        Assert.assertFalse(adapter.isDiscovering());
        Assert.assertEquals( BTRole.Slave, adapter.getRole() ); // kept
    }
}