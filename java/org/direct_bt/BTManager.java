/**
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2020 Gothel Software e.K.
 * Copyright (c) 2020 ZAFENA AB
 *
 * Author: Andrei Vasiliu <andrei.vasiliu@intel.com>
 * Copyright (c) 2016 Intel Corporation.
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
     * Interface allowing to retrieve certain settings
     * of the implementation.
     * <p>
     * In case this interface will have to add a multitude of settings in the future,
     * we will revise the API using a more efficient bitfield approach.
     * </p>
     */
    public static interface Settings {
        /**
         * Returns true if underlying implementation is Direct-BT.
         */
        boolean isDirectBT();

        /**
         * Returns true if underlying implementation is TinyB.
         */
        boolean isTinyB();

        /**
         * Returns whether {@link BTGattChar} API: {@link BTGattChar#getValue() value cache} and
         * {@link BTGattChar#enableValueNotifications(BluetoothNotification) value notification}
         * is supported.
         * <p>
         * This is enabled using {@link #isTinyB() TinyB}, but disabled by default using {@link #isDirectBT() Direct-BT}.
         * </p>
         * <p>
         * If using {@link #isDirectBT() Direct-BT}, user are encouraged to
         * {@link BTGattChar#addCharListener(BTGattCharListener, boolean[]) utilize BTGattCharListener}
         * to handle value notifications when they occur w/o caching.
         * </p>
         * <p>
         * If using {@link #isDirectBT() Direct-BT}, users can enable this TinyB compatibility
         * by setting the System property {@code jau.direct_bt.characteristic.compat} to {@code true}.
         * It defaults to {@code false}, i.e. disabled.
         * </p>
         */
        boolean isCharValueCacheNotificationSupported();

        @Override
        String toString();
    }


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
     * @implNote Not implemented on tinyb.dbus
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

    /**
     * Returns this implmentation's {@link Settings}.
     */
    public Settings getSettings();

    /** Find a BluetoothObject of a type matching type. If parameters name,
      * identifier and parent are not null, the returned object will have to
      * match them.
      * It will first check for existing objects. It will not turn on discovery
      * or connect to devices.
      * @parameter type specify the type of the object you are
      * waiting for, NONE means anything.
      * @parameter name optionally specify the name of the object you are
      * waiting for (for Adapter or Device)
      * @parameter identifier optionally specify the identifier of the object you are
      * waiting for (UUID for GattService, GattCharacteristic or GattDescriptor, address
      * for Adapter or Device)
      * @parameter parent optionally specify the parent of the object you are
      * waiting for
      * @parameter timeoutMS the function will return after timeout time in milliseconds, a
      * value of zero means wait forever. If object is not found during this time null will be returned.
      * @return An object matching the name, identifier, parent or null if not found before
      * timeout expires or event is canceled.
      */
    public BTObject find(BTType type, String name, String identifier, BTObject parent, long timeoutMS);


    /** Find a BluetoothObject of a type matching type. If parameters name,
      * identifier and parent are not null, the returned object will have to
      * match them.
      * It will first check for existing objects. It will not turn on discovery
      * or connect to devices.
      * @parameter type specify the type of the object you are
      * waiting for, NONE means anything.
      * @parameter name optionally specify the name of the object you are
      * waiting for (for Adapter or Device)
      * @parameter identifier optionally specify the identifier of the object you are
      * waiting for (UUID for GattService, GattCharacteristic or GattDescriptor, address
      * for Adapter or Device)
      * @parameter parent optionally specify the parent of the object you are
      * waiting for
      * @return An object matching the name, identifier and parent.
      */
    public BTObject  find(BTType type, String name, String identifier, BTObject parent);

    /** Find a BluetoothObject of type T. If parameters name, identifier and
      * parent are not null, the returned object will have to match them.
      * It will first check for existing objects. It will not turn on discovery
      * or connect to devices.
      * @parameter name optionally specify the name of the object you are
      * waiting for (for Adapter or Device)
      * @parameter identifier optionally specify the identifier of the object you are
      * waiting for (UUID for GattService, GattCharacteristic or GattDescriptor, address
      * for Adapter or Device)
      * @parameter parent optionally specify the parent of the object you are
      * waiting for
      * @parameter timeoutMS the function will return after timeout time in milliseconds, a
      * value of zero means wait forever. If object is not found during this time null will be returned.
      * @return An object matching the name, identifier, parent or null if not found before
      * timeout expires or event is canceled.
      */
    public <T extends BTObject>  T find(String name, String identifier, BTObject parent, long timeoutMS);

    /** Find a BluetoothObject of type T. If parameters name, identifier and
      * parent are not null, the returned object will have to match them.
      * It will first check for existing objects. It will not turn on discovery
      * or connect to devices.
      * @parameter name optionally specify the name of the object you are
      * waiting for (for Adapter or Device)
      * @parameter identifier optionally specify the identifier of the object you are
      * waiting for (UUID for GattService, GattCharacteristic or GattDescriptor, address
      * for Adapter or Device)
      * @parameter parent optionally specify the parent of the object you are
      * waiting for
      * @return An object matching the name, identifier and parent.
      */
    public <T extends BTObject>  T find(String name, String identifier, BTObject parent);

    /** Return a BluetoothObject of a type matching type. If parameters name,
      * identifier and parent are not null, the returned object will have to
      * match them. Only objects which are already in the system will be returned.
      * @parameter type specify the type of the object you are
      * waiting for, NONE means anything.
      * @parameter name optionally specify the name of the object you are
      * waiting for (for Adapter or Device)
      * @parameter identifier optionally specify the identifier of the object you are
      * waiting for (UUID for GattService, GattCharacteristic or GattDescriptor, address
      * for Adapter or Device)
      * @parameter parent optionally specify the parent of the object you are
      * waiting for
      * @return An object matching the name, identifier, parent or null if not found.
      */
    public BTObject getObject(BTType type, String name,
                                String identifier, BTObject parent);

    /** Return a List of BluetoothObject of a type matching type. If parameters name,
      * identifier and parent are not null, the returned object will have to
      * match them. Only objects which are already in the system will be returned.
      * @parameter type specify the type of the object you are
      * waiting for, NONE means anything.
      * @parameter name optionally specify the name of the object you are
      * waiting for (for Adapter or Device)
      * @parameter identifier optionally specify the identifier of the object you are
      * waiting for (UUID for GattService, GattCharacteristic or GattDescriptor, address
      * for Adapter or Device)
      * @parameter parent optionally specify the parent of the object you are
      * waiting for
      * @return A vector of object matching the name, identifier, parent.
      */
    public List<BTObject> getObjects(BTType type, String name,
                                    String identifier, BTObject parent);

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
     * @implNote Not implemented on tinyb.dbus
     */
    public BTAdapter getAdapter(final int dev_id);

    /** Returns a list of discovered BluetoothDevices
      * @return A list of discovered BluetoothDevices
      */
    public List<BTDevice> getDevices();

    /** Returns a list of available BluetoothGattServices
      * @return A list of available BluetoothGattServices
      */
    public List<BTGattService> getServices();

    /**
     * Sets a default adapter to use for discovery.
     * @return TRUE if the device was set
     * @implNote not implemented for jau.direct_bt
     */
    public boolean setDefaultAdapter(BTAdapter adapter);

    /**
     * Gets the default adapter to use for discovery.
     * <p>
     * {@code jau.direct_bt}: The default adapter is either the first {@link BTAdapter#isPowered() powered} {@link BTAdapter},
     * or function returns nullptr if none is enabled.
     * </p>
     * <p>
     * <i>tinyb.dbus</i>: System default is the last detected adapter at initialization.
     * </p>
     * @return the used default adapter
     */
    public BTAdapter getDefaultAdapter();

    /** Turns on device discovery on the default adapter if it is disabled.
      * @return TRUE if discovery was successfully enabled
      * @deprecated since 2.0.0, use {@link #startDiscovery(boolean)}.
      */
    @Deprecated
    public boolean startDiscovery() throws BTException;

    /**
     * Turns on device discovery on the default adapter if it is disabled.
     * @param keepAlive if {@code true}, indicates that discovery shall be restarted
     *        if stopped by the underlying Bluetooth implementation (BlueZ, ..).
     *        Using {@link #startDiscovery(boolean) startDiscovery}({@code keepAlive=true})
     *        and {@link #stopDiscovery()} is the recommended workflow
     *        for a reliable discovery process.
     * @param le_scan_active true enables delivery of active scanning PDUs, otherwise no scanning PDUs shall be sent (default)
     * @return {@link HCIStatusCode#SUCCESS} if successful, otherwise the {@link HCIStatusCode} error state
     * @throws BTException
     * @since 2.0.0
     * @since 2.2.8
     * @implNote {@code keepAlive} not implemented in tinyb.dbus
     */
    public HCIStatusCode startDiscovery(final boolean keepAlive, final boolean le_scan_active) throws BTException;

    /**
     * Turns off device discovery on the default adapter if it is enabled.
     * @return {@link HCIStatusCode#SUCCESS} if successful, otherwise the {@link HCIStatusCode} error state
     * @apiNote return {@link HCIStatusCode} since 2.0.0
     * @since 2.0.0
     */
    public HCIStatusCode stopDiscovery() throws BTException;

    /** Returns if the discovers is running or not.
      * @return TRUE if discovery is running
      */
    public boolean getDiscovering() throws BTException;

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
     * @implNote Not implemented on tinyb.dbus
     */
    void addChangedAdapterSetListener(final ChangedAdapterSetListener l);

    /**
     * Remove the given {@link ChangedAdapterSetListener} from this manager.
     * @param l the to be removed element
     * @return the number of removed elements
     * @since 2.0.0
     * @implNote Not implemented on tinyb.dbus
     */
    int removeChangedAdapterSetListener(final ChangedAdapterSetListener l);

    /**
     * Release the native memory associated with this object and all related Bluetooth resources.
     * The object should not be used following a call to close
     * <p>
     * Shutdown method is intended to allow a clean Bluetooth state at program exist.
     * </p>
     */
    public void shutdown();
}
