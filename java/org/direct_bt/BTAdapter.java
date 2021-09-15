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
 *
 */
package org.direct_bt;

import java.util.List;
import java.util.UUID;

/**
  * Provides access to Bluetooth adapters.
  *
  * @see [Bluetooth Specification](https://www.bluetooth.com/specifications/bluetooth-core-specification/)
  * @see [BlueZ adapter API](http://git.kernel.org/cgit/bluetooth/bluez.git/tree/doc/adapter-api.txt)
  */
public interface BTAdapter extends BTObject
{
    /**
     * Returns the used singleton {@link BTManager} instance, used to create this adapter.
     */
    public BTManager getManager();

    /** Find a BluetoothDevice. If parameters name and address are not null,
      * the returned object will have to match them.
      * It will first check for existing objects. It will not turn on discovery
      * or connect to devices.
      * @parameter name optionally specify the name of the object you are
      * waiting for
      * @parameter address optionally specify the MAC address of the device you are
      * waiting for
      * @parameter timeoutMS the function will return after timeout time in milliseconds, a
      * value of zero means wait forever. If object is not found during this time null will be returned.
      * @return An object matching the name and address or null if not found before
      * timeout expires.
      */
    public BTDevice find(String name, BDAddressAndType addressAndType, long timeoutMS);

    /** Find a BluetoothDevice. If parameters name and address are not null,
      * the returned object will have to match them.
      * It will first check for existing objects. It will not turn on discovery
      * or connect to devices.
      * @parameter name optionally specify the name of the object you are
      * waiting for
      * @parameter address optionally specify the MAC address of the device you are
      * waiting for
      * @return An object matching the name and address.
      */
    public BTDevice find(String name, BDAddressAndType addressAndType);

    /* Bluetooth specific API */

    /**
     * Returns true, if the adapter's device is already whitelisted.
     * @since 2.0.0
     */
    public boolean isDeviceWhitelisted(final BDAddressAndType addressAndType);

    /**
     * Add the given device to the adapter's autoconnect whitelist.
     * <p>
     * The given LE connection parameter will be uploaded to the kernel for the given device first,
     * if the device is of type {@link BDAddressType#BDADDR_LE_PUBLIC} or {@link BDAddressType#BDADDR_LE_RANDOM}.
     * </p>
     * <p>
     * Method will reject duplicate devices, in which case it should be removed first.
     * </p>
     *
     * @param address
     * @param address_type
     * @param ctype
     * @param conn_interval_min default value 0x000F
     * @param conn_interval_max default value 0x000F
     * @param conn_latency default value 0x0000
     * @param timeout in units of 10ms, default value 1000 for 10000ms or 10s.
     * @return {@code true} if successful, otherwise {@code false}.
     *
     * @see #addDeviceToWhitelist(String, BDAddressType, HCIWhitelistConnectType)
     * @since 2.0.0
     */
    public boolean addDeviceToWhitelist(final BDAddressAndType addressAndType,
                                        final HCIWhitelistConnectType ctype,
                                        final short conn_interval_min, final short conn_interval_max,
                                        final short conn_latency, final short timeout);

    /**
     * Add the given device to the adapter's autoconnect whitelist.
     * <p>
     * This variant of {@link #addDeviceToWhitelist(String, BDAddressType, HCIWhitelistConnectType, short, short, short, short)}
     * uses default connection parameter, which will be uploaded to the kernel for the given device first.
     * </p>
     * <p>
     * Method will reject duplicate devices, in which case it should be removed first.
     * </p>
     *
     * @param address
     * @param address_type
     * @param ctype
     * @return {@code true} if successful, otherwise {@code false}.
     *
     * @see #addDeviceToWhitelist(String, BDAddressType, HCIWhitelistConnectType, short, short, short, short)
     * @since 2.0.0
     */
    public boolean addDeviceToWhitelist(final BDAddressAndType addressAndType,
                                        final HCIWhitelistConnectType ctype);


    /**
     * Remove the given device from the adapter's autoconnect whitelist.
     * @since 2.0.0
     */
    public boolean removeDeviceFromWhitelist(final BDAddressAndType addressAndType);


    /**
     * Turns on device discovery if it is disabled.
     * <p>
     * {@code jau.direct_bt}'s implementation will always issue {@link #removeDiscoveredDevices()},
     * ensuring all scanned devices will be found after calling this method. Regardless whether
     * discovery is already running.
     * </p>
     * <pre>
     * + --+-------+--------+-----------+----------------------------------------------------+
     * | # | meta  | native | keepAlive | Note
     * +---+-------+--------+-----------+----------------------------------------------------+
     * | 1 | true  | true   | false     | -
     * | 2 | false | false  | false     | -
     * +---+-------+--------+-----------+----------------------------------------------------+
     * | 3 | true  | true   | true      | -
     * | 4 | true  | false  | true      | temporarily disabled -> startDiscoveryBackground()
     * | 5 | false | false  | true      | [4] -> [5] requires manual DISCOVERING event
     * +---+-------+--------+-----------+----------------------------------------------------+
     * </pre>
     *
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
     * @see #startDiscovery(boolean, boolean, int, int, byte)
     * @see #getDiscovering()
     */
    public HCIStatusCode startDiscovery(final boolean keepAlive, final boolean le_scan_active) throws BTException;

    /**
     * Shares same implementation as {@link #startDiscovery(boolean, boolean)}, but allows setting custom scan values.
     * @param keepAlive
     * @param le_scan_active true enables delivery of active scanning PDUs, otherwise no scanning PDUs shall be sent (default)
     * @param le_scan_interval in units of 0.625ms, default value 24 for 15ms; Value range [4 .. 0x4000] for [2.5ms .. 10.24s]
     * @param le_scan_window in units of 0.625ms, default value 24 for 15ms; Value range [4 .. 0x4000] for [2.5ms .. 10.24s]. Shall be <= le_scan_interval
     * @param filter_policy 0x00 accepts all PDUs (default), 0x01 only of whitelisted, ...
     * @return {@link HCIStatusCode#SUCCESS} if successful, otherwise the {@link HCIStatusCode} error state
     * @throws BTException
     * @since 2.2.8
     * @see #startDiscovery(boolean, boolean)
     * @see #getDiscovering()
     */
    public HCIStatusCode startDiscovery(final boolean keepAlive, final boolean le_scan_active,
                                        final short le_scan_interval, final short le_scan_window,
                                        final byte filter_policy) throws BTException;

    /**
     * Turns off device discovery if it is enabled.
     * @return {@link HCIStatusCode#SUCCESS} if successful, otherwise the {@link HCIStatusCode} error state
     * @apiNote return {@link HCIStatusCode} since 2.0.0
     * @since 2.0.0
     */
    public HCIStatusCode stopDiscovery() throws BTException;

    /** Returns a list of discovered BluetoothDevices from this adapter.
      * @return A list of discovered BluetoothDevices on this adapter,
      * NULL if an error occurred
      */
    public List<BTDevice> getDiscoveredDevices();

    /**
     * Remove all the discovered devices found on this adapter.
     *
     * @return The number of removed discovered devices on this adapter
     * @throws BTException
     * @since 2.2.0
     * @implNote Changed from 'removeDiscoveredDevices()' for clarity since version 2.2.0
     */
    public int removeDiscoveredDevices() throws BTException;

    /**
     * Discards matching discovered devices.
     * @return {@code true} if found and removed, otherwise false.
     * @since 2.2.0
     */
    public boolean removeDiscoveredDevice(final BDAddressAndType addressAndType);

    /* D-Bus property accessors: */

    /**
     * Returns the adapter's public BDAddressAndType.
     * <p>
     * The adapter's address as initially reported by the system is always its public address, i.e. {@link BDAddressType#BDADDR_LE_PUBLIC}.
     * </p>
     * @since 2.2.8
     * @see #getVisibleAddressAndType()
     */
    BDAddressAndType getAddressAndType();

    /**
     * Returns the adapter's currently visible BDAddressAndType.
     * <p>
     * The adapter's address as initially reported by the system is always its public address, i.e. {@link BDAddressType#BDADDR_LE_PUBLIC}.
     * </p>
     * <p>
     * The adapter's visible BDAddressAndType might be set to {@link BDAddressType#BDADDR_LE_RANDOM} before scanning / discovery mode (TODO).
     * </p>
     * @since 2.2.8
     * @see #getAddressAndType()
     */
    BDAddressAndType getVisibleAddressAndType();

    /** Returns the system name of this adapter.
      * @return The system name of this adapter.
      */
    public String getName();

    /**
     * Returns the BluetoothAdapter's internal temporary device id
     * <p>
     * The internal device id is constant across the adapter lifecycle,
     * but may change after its destruction.
     * </p>
     * @since 2.0.0
     */
    public int getDevID();

    /** Returns the friendly name of this adapter.
      * @return The friendly name of this adapter, or NULL if not set.
      */
    public String getAlias();

    /** Sets the friendly name of this adapter.
      */
    public void setAlias(String value);

    /**
     * Returns whether the adapter is valid, plugged in and powered.
     * @return true if {@link #isValid()}, HCI channel open and {@link AdapterSettings.SettingType#POWERED POWERED} state is set.
     * @see #isSuspended()
     * @see #isValid()
     * @since 2.0.0
     */
    public boolean isPowered();

    /**
     * Returns whether the adapter is suspended, i.e. valid and plugged in, but not powered.
     * @return true if {@link #isValid()}, HCI channel open and {@link AdapterSettings.SettingType#POWERED POWERED} state is not set.
     * @see #isPowered()
     * @see #isValid()
     */
    public boolean isSuspended();

    /**
     * Returns whether the adapter is valid, i.e. reference is valid, plugged in and generally operational,
     * but not necessarily {@link #isPowered()}.
     * @return true if this adapter references are valid and hadn't been {@link #close()}'ed
     * @see #isPowered()
     * @see #isSuspended()
     * @since 2.0.0
     */
    public boolean isValid();

    /**
     * Returns the power state the adapter.
     * <p>
     * Consider using {@link #isPowered()}
     * </p>
     * @return The power state of the adapter.
     * @since 2.0.0 Renamed from getPowered() to getPoweredState()
     * @see #isPowered()
     * @see #isSuspended()
     * @see #isValid()
     */
    public boolean getPoweredState();

    /**
     * Sets the power state the adapter.
     * <p>
     * Calling the method to power-on this adapter will allow close() to power-off the adapter.
     * </p>
     * @apiNote return value boolean since 2.0.0
     * @see #close()
     * @see #initialize(BTMode)
     * @since 2.0.0
     */
    public boolean setPowered(boolean value);

    /**
     * Initialize the adapter with default values, including power-on.
     * <p>
     * Method shall be issued on the desired adapter found via {@link BTManager.ChangedAdapterSetListener}.
     * </p>
     * <p>
     * While initialization, the adapter is first powered-off, setup and then powered-on.
     * </p>
     * <p>
     * Calling the method will allow close() to power-off the adapter.
     * </p>
     * <p>
     * This override uses {@link BTMode#DUAL}
     * </p>
     * @return {@link HCIStatusCode#SUCCESS} or an error state
     * @see #initialize(BTMode)
     * @see #close()
     * @see #setPowered(boolean)
     * @since 2.4.0
     */
    public HCIStatusCode initialize();

    /**
     * Initialize the adapter with default values, including power-on.
     * <p>
     * Method shall be issued on the desired adapter found via {@link BTManager.ChangedAdapterSetListener}.
     * </p>
     * <p>
     * While initialization, the adapter is first powered-off, setup and then powered-on.
     * </p>
     * <p>
     * Calling the method will allow close() to power-off the adapter.
     * </p>
     * @param btMode the desired adapter's {@link BTMode}, default shall be {@link BTMode#DUAL}
     * @return {@link HCIStatusCode#SUCCESS} or an error state
     * @see #initialize()
     * @see #close()
     * @see #setPowered(boolean)
     * @since 2.4.0
     */
    public HCIStatusCode initialize(final BTMode btMode);

    /**
     * Reset the adapter.
     * <p>
     * The semantics are specific to the HCI host implementation,
     * however, it shall comply at least with the HCI Reset command
     * and bring up the device from standby into a POWERED functional state afterwards.
     * </p>
     * <pre>
     * BT Core Spec v5.2: Vol 4, Part E HCI: 7.3.2 Reset command
     * </pre>
     * @since 2.0.0
     */
    public HCIStatusCode reset();

    /** Returns the discoverable state the adapter.
      * @return The discoverable state of the adapter.
      */
    public boolean getDiscoverable();

    /**
     * Sets the discoverable state the adapter.
     * @apiNote return value boolean since 2.0.0
     * @since 2.0.0
     */
    public boolean setDiscoverable(boolean value);

    /**
     * This method connects to device without need of
     * performing General Discovery. Connection mechanism is
     * similar to Connect method from Device1 interface with
     * exception that this method returns success when physical
     * connection is established. After this method returns,
     * services discovery will continue and any supported
     * profile will be connected. There is no need for calling
     * Connect on Device1 after this call. If connection was
     * successful this method returns object path to created
     * device object.
     *
     * @param address The Bluetooth device address of the remote
     *                device. This parameter is mandatory.
     * @param addressType The Bluetooth device Address Type. This is
     *                address type that should be used for initial
     *                connection. If this parameter is not present
     *                BR/EDR device is created.
     *                Possible values:
     *                <ul>
     *                <li>{@code public} - Public address</li>
     *                <li>{@code random} - Random address</li>
     *                </ul>
     */
    public BTDevice connectDevice(BDAddressAndType addressAndType);

    /**
     * Sets the discoverable state the adapter.
     * @apiNote return value boolean since 2.0.0
     * @since 2.0.0
     */
    public boolean setPairable(boolean value);

    /**
     * Returns the current meta discovering {@link ScanType}.
     * It can be modified through {@link #startDiscovery(boolean, boolean)} and {@link #stopDiscovery()}.
     * <p>
     * Note that if {@link #startDiscovery(boolean, boolean)} has been issued with keepAlive==true,
     * the meta {@link ScanType} will still keep the desired {@link ScanType} enabled
     * even if it has been temporarily disabled.
     * </p>
     * @see #startDiscovery(boolean)
     * @see #stopDiscovery()
     * @since 2.0.0
     */
    public ScanType getCurrentScanType();

    /**
     * Add the given {@link AdapterStatusListener} to the list if not already present.
     * <p>
     * In case the {@link AdapterStatusListener}'s lifecycle and event delivery
     * shall be constrained to this device, please use
     * {@link BTDevice#addStatusListener(AdapterStatusListener)}.
     * </p>
     * <p>
     * The newly added {@link AdapterStatusListener} will receive an initial
     * {@link AdapterStatusListener#adapterSettingsChanged(BTAdapter, AdapterSettings, AdapterSettings, AdapterSettings, long) adapterSettingsChanged}
     * event, passing an {@link AdapterSettings empty oldMask and changedMask}, as well as {@link AdapterSettings current newMask}. <br>
     * This allows the receiver to be aware of this adapter's current settings.
     * </p>
     * @param listener A {@link AdapterStatusListener} instance
     * @return true if the given listener is not element of the list and has been newly added, otherwise false.
     * @since 2.3.0
     * @see {@link BTDevice#addStatusListener(AdapterStatusListener)}
     * @see {@link #removeStatusListener(AdapterStatusListener)}
     * @see {@link #removeAllStatusListener()}
     */
    public boolean addStatusListener(final AdapterStatusListener listener);

    /**
     * Remove the given {@link AdapterStatusListener} from the list.
     * @param listener A {@link AdapterStatusListener} instance
     * @return true if the given listener is an element of the list and has been removed, otherwise false.
     * @since 2.0.0
     */
    public boolean removeStatusListener(final AdapterStatusListener l);

    /**
     * Remove all {@link AdapterStatusListener} from the list.
     * @return number of removed listener.
     * @since 2.0.0
     */
    public int removeAllStatusListener();

    /**
     * Print the internally maintained BTDevice lists to stderr:
     * - sharedDevices
     * - connectedDevice
     * - discoveredDevices
     * - StatusListener
     *
     * This is intended as a debug facility.
     * @since 2.3.0
     */
    public void printDeviceLists();

}
