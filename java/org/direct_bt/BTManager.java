/**
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2020 Gothel Software e.K.
 * Copyright (c) 2020 ZAFENA AB
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

package org.direct_bt;

import java.util.List;

public interface BTManager
{
    /**
     * Event listener to receive change events regarding the system's {@link BTAdapter} set,
     * e.g. a removed or added {@link BTAdapter} due to user interaction or 'cold reset'.
     * <p>
     * When a new callback is added, all available adapter's will be reported as added,
     * this allows a fully event driven workflow.
     * </p>
     * <p>
     * The callback is performed on a dedicated thread,
     * allowing the user to perform complex operations.
     * </p>
     * @since 2.0.0
     * @see BTManager#addChangedAdapterSetListener(ChangedAdapterSetListener)
     * @see BTManager#removeChangedAdapterSetListener(ChangedAdapterSetListener)
     */
    public static interface ChangedAdapterSetListener {
        /**
         * {@link BTAdapter} was added to the system.
         * @param adapter the newly added {@link BTAdapter} to the system
         */
        void adapterAdded(final BTAdapter adapter);

        /**
         * {@link BTAdapter} was removed from the system.
         * <p>
         * {@link BTAdapter#close()} is being called by the native manager after issuing all
         * {@link #adapterRemoved(BTAdapter)} calls.
         * </p>
         * @param adapter the removed {@link BTAdapter} from the system
         */
        void adapterRemoved(final BTAdapter adapter);
    }

    /** Returns a list of BluetoothAdapters available in the system
      * @return A list of BluetoothAdapters available in the system
      */
    public List<BTAdapter> getAdapters();

    /**
     * Returns the BluetoothAdapter matching the given dev_id or null if not found.
     * <p>
     * The adapters internal device id is constant across the adapter lifecycle,
     * but may change after its destruction.
     * </p>
     * @param dev_id the internal temporary adapter device id
     * @since 2.0.0
     */
    public BTAdapter getAdapter(final int dev_id);

    /**
     * Sets a default adapter to use for discovery.
     * @return TRUE if the device was set
     * @implNote not implemented for jau.direct_bt
     */
    public boolean setDefaultAdapter(BTAdapter adapter);

    /**
     * Gets the default adapter to use for discovery.
     * <p>
     * The default adapter is either the first {@link BTAdapter#isPowered() powered} {@link BTAdapter},
     * or function returns nullptr if none is enabled.
     * </p>
     * @return the used default adapter
     */
    public BTAdapter getDefaultAdapter();

    /**
     * Add the given {@link ChangedAdapterSetListener} to this manager.
     * <p>
     * When a new callback is added, all available adapter's will be reported as added,
     * this allows a fully event driven workflow.
     * </p>
     * <p>
     * The callback is performed on a dedicated thread,
     * allowing the user to perform complex operations.
     * </p>
     * @since 2.0.0
     */
    void addChangedAdapterSetListener(final ChangedAdapterSetListener l);

    /**
     * Remove the given {@link ChangedAdapterSetListener} from this manager.
     * @param l the to be removed element
     * @return the number of removed elements
     * @since 2.0.0
     */
    int removeChangedAdapterSetListener(final ChangedAdapterSetListener l);

    /**
     * Remove all added {@link ChangedAdapterSetListener} entries from this manager.
     * @return the number of removed elements
     * @since 2.7.0
     */
    int removeAllChangedAdapterSetListener();

    /**
     * Release the native memory associated with this object and all related Bluetooth resources.
     * The object should not be used following a call to close
     * <p>
     * Shutdown method is intended to allow a clean Bluetooth state at program exist.
     * </p>
     */
    public void shutdown();
}
