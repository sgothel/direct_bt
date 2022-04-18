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

import java.util.ArrayList;
import java.util.List;

import org.direct_bt.BTAdapter;
import org.direct_bt.BTManager;
import org.direct_bt.BTRole;
import org.direct_bt.BTUtils;
import org.direct_bt.HCIStatusCode;
import org.junit.Assert;

public interface DBTEndpoint {

    /**
     * Return name of this endpoint,
     * which becomes the adapter's name.
     */
    String getName();

    /**
     * Set the server adapter for this endpoint.
     *
     * This is done in {@link ChangedAdapterSetListener#adapterAdded(BTAdapter)}
     * if {@link #initAdapter(BTAdapter)} returned true.
     *
     * @param a the associate adapter for this endpoint.
     */
    void setAdapter(BTAdapter a);

    /**
     * Return the adapter for this endpoint.
     */
    BTAdapter getAdapter();

    void close(final String msg);

    /**
     * Initialize the given adapter for this endpoint.
     *
     * The matching and successfully initialized adapter
     * will become this endpoint's associated adapter via {@link #setAdapter(BTAdapter)},
     * as performed in in {@link ChangedAdapterSetListener#adapterAdded(BTAdapter)}.
     *
     * @param adapter the potential associated adapter for this endpoint.
     * @return true if successful and associated
     */
    boolean initAdapter(BTAdapter adapter);

    public static void checkInitializedState(final DBTEndpoint endp) {
        final BTAdapter adapter = endp.getAdapter();
        Assert.assertTrue( adapter.isInitialized() );
        Assert.assertTrue( adapter.isPowered() );
        Assert.assertEquals( BTRole.Master, adapter.getRole() );
        Assert.assertTrue( 4 <= adapter.getBTMajorVersion() );
    }

    public static ChangedAdapterSetListener initChangedAdapterSetListener(final BTManager manager, final List<DBTEndpoint> endpts) {
        final ChangedAdapterSetListener casl = new ChangedAdapterSetListener(endpts);
        manager.addChangedAdapterSetListener(casl);
        for(final DBTEndpoint endpt : endpts ) {
            Assert.assertNotNull("No adapter found for "+endpt.getClass().getSimpleName(), endpt.getAdapter());
        }
        return casl;
    }
    public static class ChangedAdapterSetListener implements BTManager.ChangedAdapterSetListener {
        List<DBTEndpoint> endpts = new ArrayList<DBTEndpoint>();

        public ChangedAdapterSetListener() { }
        public ChangedAdapterSetListener(final List<DBTEndpoint> el) {
            endpts.addAll(el);
        }
        public boolean add(final DBTEndpoint e) { return endpts.add(e); }

        @Override
        public void adapterAdded(final BTAdapter adapter) {
            for(final DBTEndpoint endpt : endpts ) {
                if( null == endpt.getAdapter() ) {
                    if( endpt.initAdapter( adapter ) ) {
                        endpt.setAdapter(adapter);
                        BTUtils.println(System.err, "****** Adapter-"+endpt.getClass().getSimpleName()+" ADDED__: InitOK: " + adapter);
                        return;
                    }
                }
            }
            BTUtils.println(System.err, "****** Adapter ADDED__: Ignored: " + adapter);
        }

        @Override
        public void adapterRemoved(final BTAdapter adapter) {
            for(final DBTEndpoint endpt : endpts ) {
                if( null != endpt.getAdapter() && adapter == endpt.getAdapter() ) {
                    endpt.setAdapter(null);
                    BTUtils.println(System.err, "****** Adapter-"+endpt.getClass().getSimpleName()+" REMOVED: " + adapter);
                    return;
                }
            }
            BTUtils.println(System.err, "****** Adapter REMOVED: Ignored " + adapter);
        }
    };

    public static void startDiscovery(final BTAdapter adapter, final boolean current_exp_discovering_state) {
        Assert.assertFalse(adapter.isAdvertising());
        Assert.assertEquals(current_exp_discovering_state, adapter.isDiscovering());

        Assert.assertEquals( HCIStatusCode.SUCCESS, adapter.startDiscovery() );
        while( !adapter.isDiscovering() ) { // pending action
            try { Thread.sleep(100); } catch (final InterruptedException e) { e.printStackTrace(); }
        }
        Assert.assertFalse(adapter.isAdvertising());
        Assert.assertTrue(adapter.isDiscovering());
        Assert.assertEquals( BTRole.Master, adapter.getRole() );
    }

    public static void stopDiscovery(final BTAdapter adapter, final boolean current_exp_discovering_state) {
        Assert.assertFalse(adapter.isAdvertising());
        Assert.assertEquals(current_exp_discovering_state, adapter.isDiscovering());
        Assert.assertEquals( BTRole.Master, adapter.getRole() );

        Assert.assertEquals( HCIStatusCode.SUCCESS, adapter.stopDiscovery() ); // pending action
        while( adapter.isDiscovering() ) { // pending action
            try { Thread.sleep(100); } catch (final InterruptedException e) { e.printStackTrace(); }
        }
        Assert.assertFalse(adapter.isAdvertising());
        Assert.assertFalse(adapter.isDiscovering());
        Assert.assertEquals( BTRole.Master, adapter.getRole() );
    }

}