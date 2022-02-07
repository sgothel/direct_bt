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
import org.direct_bt.HCIStatusCode;
import org.junit.Assert;

public interface DBTClientTest extends DBTEndpoint {

    @Override
    boolean initAdapter(BTAdapter adapter);

    @Override
    void setAdapter(BTAdapter clientAdapter);

    @Override
    BTAdapter getAdapter();

    HCIStatusCode startDiscovery(BTAdapter adapter, String msg);

    HCIStatusCode stopDiscovery(BTAdapter adapter, String msg);

    public static void startDiscovery(final DBTClientTest client, final boolean current_exp_discovering_state, final String msg) {
        final BTAdapter adapter = client.getAdapter();
        Assert.assertFalse(adapter.isAdvertising());
        Assert.assertEquals(current_exp_discovering_state, adapter.isDiscovering());

        Assert.assertEquals( HCIStatusCode.SUCCESS, client.startDiscovery(adapter, msg) );
        while( !adapter.isDiscovering() ) { // pending action
            try { Thread.sleep(100); } catch (final InterruptedException e) { e.printStackTrace(); }
        }
        Assert.assertFalse(adapter.isAdvertising());
        Assert.assertTrue(adapter.isDiscovering());
        Assert.assertEquals( BTRole.Master, adapter.getRole() );
    }

    public static void stopDiscovery(final DBTClientTest client, final boolean current_exp_discovering_state, final String msg) {
        final BTAdapter adapter = client.getAdapter();
        Assert.assertFalse(adapter.isAdvertising());
        Assert.assertEquals(current_exp_discovering_state, adapter.isDiscovering());
        Assert.assertEquals( BTRole.Master, adapter.getRole() );

        Assert.assertEquals( HCIStatusCode.SUCCESS, client.stopDiscovery(client.getAdapter(), msg) );
        while( adapter.isDiscovering() ) { // pending action
            try { Thread.sleep(100); } catch (final InterruptedException e) { e.printStackTrace(); }
        }
        Assert.assertFalse(adapter.isAdvertising());
        Assert.assertFalse(adapter.isDiscovering());
        Assert.assertEquals( BTRole.Master, adapter.getRole() );
    }
}