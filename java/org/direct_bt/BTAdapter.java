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
 *
 */
package org.direct_bt;

import java.util.List;

/**
  * BTAdapter represents one local Bluetooth Controller.
  * <p>
  * @anchor BTAdapterRoles
  * Local BTAdapter roles (see {@link #getRole()}):
  *
  * - {@link BTRole::Master}: The local adapter is discovering remote {@link BTRole::Slave} {@link BTDevice}s and may initiate connections. Enabled via {@link #startDiscovery(boolean, boolean, short, short, byte) startDiscovery(..)}, but also per default at construction.
  * - {@link BTRole::Slave}: The local adapter is advertising to remote {@link BTRole::Master} {@link BTDevice}s and may accept connections. Enabled explicitly via {@link #startAdvertising(short, short, byte, byte, byte) startAdvertising(..)} until {@link #startDiscovery(boolean, boolean, short, short, byte) startDiscovery(..)}.
  *
  * Note the remote {@link BTDevice}'s [opposite role](@ref BTDeviceRoles).
  * </p>
  *
  * @see BTDevice
  * @see [BTDevice roles](@ref BTDeviceRoles).
  * @see [Bluetooth Specification](https://www.bluetooth.com/specifications/bluetooth-core-specification/)
  */
public interface BTAdapter extends BTObject
{
    /**
     * Returns the used singleton {@link BTManager} instance, used to create this adapter.
     */
    BTManager getManager();

    /**
     * Return the current {@link BTRole} of this adapter.
     * @see @ref BTAdapterRoles
     * @since 2.4.0
     */
    BTRole getRole();

    /**
     * Returns the current {@link BTMode} of this adapter.
     * @since 2.4.0
     */
    BTMode getBTMode();

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
    BTDevice find(String name, BDAddressAndType addressAndType, long timeoutMS);

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
    BTDevice find(String name, BDAddressAndType addressAndType);

    /* Bluetooth specific API */

    /**
     * Returns true, if the adapter's device is already whitelisted.
     * @since 2.0.0
     */
    boolean isDeviceWhitelisted(final BDAddressAndType addressAndType);

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
    boolean addDeviceToWhitelist(final BDAddressAndType addressAndType,
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
    boolean addDeviceToWhitelist(final BDAddressAndType addressAndType,
                                 final HCIWhitelistConnectType ctype);


    /**
     * Remove the given device from the adapter's autoconnect whitelist.
     * @since 2.0.0
     */
    boolean removeDeviceFromWhitelist(final BDAddressAndType addressAndType);


    /**
     * Starts discovery using all default arguments, see {@link #startDiscovery(boolean, boolean, short, short, byte)} for details.
     *
     * @param keepAlive
     * @param le_scan_active true enables delivery of active scanning PDUs like EIR w/ device name (default), otherwise no scanning PDUs shall be sent.
     * @return {@link HCIStatusCode#SUCCESS} if successful, otherwise the {@link HCIStatusCode} error state
     * @throws BTException
     * @since 2.2.8
     * @see #startDiscovery(boolean, boolean, int, int, byte)
     * @see #getDiscovering()
     */
    HCIStatusCode startDiscovery(final boolean keepAlive, final boolean le_scan_active) throws BTException;

    /**
     * Starts discovery.
     *
     * Returns {@link HCIStatusCode#SUCCESS} if successful, otherwise the {@link HCIStatusCode} error state;
     *
     * if `keepAlive` is `true`, discovery state will be re-enabled
     * in case the underlying Bluetooth implementation disables it.
     * Default is `true`.
     *
     * Using {@link #startDiscovery(boolean, boolean, short, short, byte) startDiscovery(keepAlive=true, ...) and {@link #stopDiscovery()}
     * is the recommended workflow for a reliable discovery process.
     *
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
     * Default parameter values are chosen for using public address resolution
     * and usual discovery intervals etc.
     *
     * Method will always clear previous discovered devices via {@link #removeDiscoveredDevices()}.
     *
     * Method fails if isAdvertising().
     *
     * If successful, method also changes [this adapter's role](@ref BTAdapterRoles) to {@link BTRole#Master}.
     *
     * @param keepAlive see above
     * @param le_scan_active true enables delivery of active scanning PDUs like EIR w/ device name (default), otherwise no scanning PDUs shall be sent
     * @param le_scan_interval in units of 0.625ms, default value 24 for 15ms; Value range [4 .. 0x4000] for [2.5ms .. 10.24s]
     * @param le_scan_window in units of 0.625ms, default value 24 for 15ms; Value range [4 .. 0x4000] for [2.5ms .. 10.24s]. Shall be <= le_scan_interval
     * @param filter_policy 0x00 accepts all PDUs (default), 0x01 only of whitelisted, ...
     * @return {@link HCIStatusCode#SUCCESS} if successful, otherwise the {@link HCIStatusCode} error state
     * @throws BTException
     * @since 2.2.8
     * @see #startDiscovery(boolean, boolean)
     * @see #isDiscovering()
     * @see isAdvertising()
     * @see @ref BTAdapterRoles
     */
    HCIStatusCode startDiscovery(final boolean keepAlive, final boolean le_scan_active,
                                 final short le_scan_interval, final short le_scan_window,
                                 final byte filter_policy) throws BTException;

    /**
     * Turns off device discovery if it is enabled.
     * @return {@link HCIStatusCode#SUCCESS} if successful, otherwise the {@link HCIStatusCode} error state
     * @apiNote return {@link HCIStatusCode} since 2.0.0
     * @since 2.0.0
     */
    HCIStatusCode stopDiscovery() throws BTException;

    /** Returns a list of discovered BluetoothDevices from this adapter.
      * @return A list of discovered BluetoothDevices on this adapter,
      * NULL if an error occurred
      */
    List<BTDevice> getDiscoveredDevices();

    /**
     * Remove all the discovered devices found on this adapter.
     *
     * @return The number of removed discovered devices on this adapter
     * @throws BTException
     * @since 2.2.0
     * @implNote Changed from 'removeDiscoveredDevices()' for clarity since version 2.2.0
     */
    int removeDiscoveredDevices() throws BTException;

    /**
     * Discards matching discovered devices.
     * @return {@code true} if found and removed, otherwise false.
     * @since 2.2.0
     */
    boolean removeDiscoveredDevice(final BDAddressAndType addressAndType);


    /**
     * Starts advertising
     * <pre>
     * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.53 LE Set Extended Advertising Parameters command (Bluetooth 5.0)
     * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.54 LE Set Extended Advertising Data command (Bluetooth 5.0)
     * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.55 LE Set Extended Scan Response Data command (Bluetooth 5.0)
     * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.56 LE Set Extended Advertising Enable command (Bluetooth 5.0)
     *
     * if available, otherwise using
     *
     * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.5 LE Set Advertising Parameters command
     * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.7 LE Set Advertising Data command
     * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.8 LE Set Scan Response Data command
     * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.9 LE Set Advertising Enable command
     * </pre>
     *
     * Method fails if isDiscovering() or has any open or pending connected remote {@link BTDevice}s.
     *
     * If successful, method also changes [this adapter's role](@ref BTAdapterRoles) to ::BTRole::Slave.
     *
     * @param gattServerData_ the {@link DBGattServer} data to be advertised and offered via GattHandler as ::GATTRole::Server.
     *        Its handles will be setup via DBGattServer::setServicesHandles().
     *        Reference is held until next disconnect.
     * @param adv_interval_min in units of 0.625ms, default value 0x0800 for 1.28s; Value range [0x0020 .. 0x4000] for [20ms .. 10.24s]
     * @param adv_interval_max in units of 0.625ms, default value 0x0800 for 1.28s; Value range [0x0020 .. 0x4000] for [20ms .. 10.24s]
     * @param adv_type see AD_PDU_Type, default 0x00, i.e. ::AD_PDU_Type::ADV_IND
     * @param adv_chan_map bit 0: chan 37, bit 1: chan 38, bit 2: chan 39, default is 0x07 (all 3 channels enabled)
     * @param filter_policy 0x00 accepts all PDUs (default), 0x01 only of whitelisted, ...
     * @return HCIStatusCode::SUCCESS if successful, otherwise the HCIStatusCode error state
     * @see #startAdvertising()
     * @see #stopAdvertising()
     * @see #isAdvertising()
     * @see isDiscovering()
     * @see @ref BTAdapterRoles
     * @since 2.4.0
     */
    HCIStatusCode startAdvertising(final DBGattServer gattServerData,
                                   final short adv_interval_min, final short adv_interval_max,
                                   final byte adv_type, final byte adv_chan_map, final byte filter_policy);

    /**
     * Starts advertising using all default arguments, see {@link #startAdvertising(short, short, byte, byte, byte)} for details.
     *
     * @param gattServerData_ the {@link DBGattServer} data to be advertised and offered via GattHandler as ::GATTRole::Server.
     *        Its handles will be setup via DBGattServer::setServicesHandles().
     *        Reference is held until next disconnect.
     * @return HCIStatusCode::SUCCESS if successful, otherwise the HCIStatusCode error state
     * @see #startAdvertising(short, short, byte, byte, byte)
     * @see #stopAdvertising()
     * @see #isAdvertising()
     * @see isDiscovering()
     * @see @ref BTAdapterRoles
     * @since 2.4.0
     */
    HCIStatusCode startAdvertising(final DBGattServer gattServerData); /* {
        return startAdvertising(gattServerData, (short)0x0800, (short)0x0800, (byte)0, // AD_PDU_Type::ADV_IND,
                                (byte)0x07, (byte)0x00);
    } */

    /**
     * Ends advertising.
     * <pre>
     * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.56 LE Set Extended Advertising Enable command (Bluetooth 5.0)
     *
     * if available, otherwise using
     *
     * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.9 LE Set Advertising Enable command
     * </pre>
     * @return HCIStatusCode::SUCCESS if successful, otherwise the HCIStatusCode error state
     * @see #startAdvertising(short, short, byte, byte, byte)
     * @see #isAdvertising()
     * @since 2.4.0
     */
    HCIStatusCode stopAdvertising();

    /**
     * Returns the adapter's current advertising state. It can be modified through startAdvertising(..) and stopAdvertising().
     * @see #startAdvertising(short, short, byte, byte, byte)
     * @see #stopAdvertising()
     * @since 2.4.0
     */
    boolean isAdvertising();

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

    /**
     * Returns the BluetoothAdapter's internal temporary device id
     * <p>
     * The internal device id is constant across the adapter lifecycle,
     * but may change after its destruction.
     * </p>
     * @since 2.0.0
     */
    int getDevID();

    /**
     * Returns the name.
     * <p>
     * Can be changed via {@link #setName(String, String)} while powered-off.
     * </p>
     * @see #setName(String, String)
     */
    String getName();

    /**
     * Returns the short name.
     * <p>
     * Can be changed via {@link #setName(String, String)} while powered-off.
     * </p>
     * @see #setName(String, String)
     * @since 2.4.0
     */
    String getShortName();

    /**
     * Sets the name and short-name.
     * <p>
     * Shall be performed while powered-off.
     * </p>
     * <p>
     * The corresponding management event will change the name and short-name.
     * </p>
     * @see #getName()
     * @see #getShortName()
     * @since 2.4.0
     */
    HCIStatusCode setName(String name, String short_name);

    /**
     * Returns whether the adapter is valid, plugged in and powered.
     * @return true if {@link #isValid()}, HCI channel open and {@link AdapterSettings.SettingType#POWERED POWERED} state is set.
     * @see #isSuspended()
     * @see #isValid()
     * @since 2.0.0
     */
    boolean isPowered();

    /**
     * Returns whether the adapter is suspended, i.e. valid and plugged in, but not powered.
     * @return true if {@link #isValid()}, HCI channel open and {@link AdapterSettings.SettingType#POWERED POWERED} state is not set.
     * @see #isPowered()
     * @see #isValid()
     */
    boolean isSuspended();

    /**
     * Returns whether the adapter is valid, i.e. reference is valid, plugged in and generally operational,
     * but not necessarily {@link #isPowered()}.
     * @return true if this adapter references are valid and hadn't been {@link #close()}'ed
     * @see #isPowered()
     * @see #isSuspended()
     * @since 2.0.0
     */
    boolean isValid();

    /**
     * Return {@link LE_Features} for this controller.
     * <pre>
     * BT Core Spec v5.2: Vol 6, Part B, 4.6 (LE LL) Feature Support
     * </pre>
     * @since 2.4.0
     */
    LE_Features getLEFeatures();

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
    boolean getPoweredState();

    /**
     * Sets the power state the adapter.
     *
     * In case current power state is already as desired, method will not change the power state.
     *
     * @param power_on true will power on this adapter if it is powered-off and vice versa.
     * @return true if successfully powered-on, -off or unchanged, false on failure
     *
     * @apiNote return value boolean since 2.0.0
     * @see #close()
     * @see #initialize(BTMode)
     * @see #isInitialized()
     * @since 2.0.0
     */
    boolean setPowered(final boolean power_on);

    /**
     * Returns whether Secure Connections (SC) is enabled.
     * @see #setSecureConnections(boolean)
     * @since 2.4.0
     */
    boolean getSecureConnectionsEnabled();

    /**
     * Enable or disable Secure Connections (SC) of the adapter.
     *
     * By default, Secure Connections (SC) is enabled if supported.
     *
     * @param enable
     * @return true if successful, otherwise false
     * @see #getSecureConnectionsEnabled()
     * @since 2.4.0
     */
    boolean setSecureConnections(final boolean enable);

    /**
     * Set the adapter's persistent storage directory for {@link SMPKeyBin} files.
     * - if set, all {@link SMPKeyBin} instances will be managed and persistent.
     * - if not set, all {@link SMPKeyBin} instances will be transient only.
     *
     * When called, all keys within the path will be loaded,
     * i.e. issuing {@link BTDevice#uploadKeys(SMPKeyBin, BTSecurityLevel) for all keys belonging to this BTAdapter.
     *
     * Persistent {@link SMPKeyBin} management is only functional when {@link BTAdapter} is in {@link BTRole#Slave} peripheral mode.
     *
     * For each {@link SMPKeyBin} file one shared {@link BTDevice} in {@link BTRole#Master} will be instantiated
     * when uploadKeys() is called.
     *
     * @param path persistent storage path to {@link SMPKeyBin} files
     * @see BTDevice#uploadKeys(SMPKeyBin, BTSecurityLevel)
     */
    void setSMPKeyPath(final String path);

    /**
     * Initialize the adapter with default values, including power-on.
     * <p>
     * Method shall be issued on the desired adapter found via {@link BTManager.ChangedAdapterSetListener}.
     * </p>
     * <p>
     * While initialization, the adapter is first powered-off, setup and then powered-on.
     * </p>
     * <p>
     * Calling the method will allow close() to power-off the adapter,
     * if not powered on before.
     * </p>
     * <p>
     * This override uses {@link BTMode#DUAL}
     * </p>
     * @return {@link HCIStatusCode#SUCCESS} or an error state on failure (e.g. power-on)
     * @see #initialize(BTMode)
     * @see #isInitialized()
     * @see #close()
     * @see #setPowered(boolean)
     * @since 2.4.0
     */
    HCIStatusCode initialize();

    /**
     * Initialize the adapter with default values, including power-on.
     * <p>
     * Method shall be issued on the desired adapter found via {@link BTManager.ChangedAdapterSetListener}.
     * </p>
     * <p>
     * While initialization, the adapter is first powered-off, setup and then powered-on.
     * </p>
     * <p>
     * Calling the method will allow close() to power-off the adapter,
     * if not powered on before.
     * </p>
     * @param btMode the desired adapter's {@link BTMode}, default shall be {@link BTMode#DUAL}
     * @return {@link HCIStatusCode#SUCCESS} or an error state on failure (e.g. power-on)
     * @see #isInitialized()
     * @see #initialize()
     * @see #close()
     * @see #setPowered(boolean)
     * @since 2.4.0
     */
    HCIStatusCode initialize(final BTMode btMode);

    /**
     * Returns true, if {@link #initialize(BTMode)} has already been called for this adapter, otherwise false.
     *
     * @see #initialize(BTMode)
     * @since 2.4.0
     */
    boolean isInitialized();

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
    HCIStatusCode reset();

    /**
     * Sets default preference of LE_PHYs.
     *
     * BT Core Spec v5.2: Vol 4, Part E, 7.8.49 LE Set PHY command
     *
     * @param Tx transmitter LE_PHYs bit mask of preference if not set to zero {@link LE_PHYs#mask} (ignored).
     * @param Rx receiver LE_PHYs bit mask of preference if not set to zero {@link LE_PHYs#mask} (ignored).
     * @return
     * @see BTDevice#getTxPhys()
     * @see BTDevice#getRxPhys()
     * @see BTDevice#getConnectedLE_PHY(LE_PHYs[], LE_PHYs[])
     * @see BTDevice#setConnectedLE_PHY(LE_PHYs, LE_PHYs)
     * @since 2.4.0
     */
    HCIStatusCode setDefaultLE_PHY(final LE_PHYs Tx, final LE_PHYs Rx);

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
    BTDevice connectDevice(BDAddressAndType addressAndType);

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
    ScanType getCurrentScanType();

    /**
     * Returns true if the meta discovering state is not {@link ScanType#NONE}.
     * It can be modified through {@link #startDiscovery(boolean, boolean)} and {@link #stopDiscovery()}.
     * @see startDiscovery()
     * @see stopDiscovery()
     * @since 2.4.0
     */
    boolean isDiscovering();

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
    boolean addStatusListener(final AdapterStatusListener listener);

    /**
     * Remove the given {@link AdapterStatusListener} from the list.
     * @param listener A {@link AdapterStatusListener} instance
     * @return true if the given listener is an element of the list and has been removed, otherwise false.
     * @since 2.0.0
     */
    boolean removeStatusListener(final AdapterStatusListener l);

    /**
     * Remove all {@link AdapterStatusListener} from the list.
     * @return number of removed listener.
     * @since 2.0.0
     */
    int removeAllStatusListener();

    /**
     * Return the user's DBGattServer shared reference if in {@link BTRole#Slave} mode
     * as set via {@link #startAdvertising(DBGattServer) and valid until subsequent disconnect.
     *
     * Returns nullptr if in {@link BTRole#Master} mode.
     */
    DBGattServer getGATTServerData();

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
    void printDeviceLists();

}
