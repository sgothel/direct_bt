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
import java.util.Map;

/**
  * Provides access to Bluetooth adapters.
  *
  * @see [Bluetooth Specification](https://www.bluetooth.com/specifications/bluetooth-core-specification/)
  * @see [BlueZ device API](http://git.kernel.org/cgit/bluetooth/bluez.git/tree/doc/device-api.txt)
  *
  */
public interface BTDevice extends BTObject
{
    @Override
    public BTDevice clone();

    /** Find a BluetoothGattService. If parameter UUID is not null,
      * the returned object will have to match it.
      * It will first check for existing objects. It will not turn on discovery
      * or connect to devices.
      * @parameter UUID optionally specify the UUID of the BluetoothGattService you are
      * waiting for
      * @parameter timeoutMS the function will return after timeout time in milliseconds, a
      * value of zero means wait forever. If object is not found during this time null will be returned.
      * @return An object matching the UUID or null if not found before
      * timeout expires or event is canceled.
      */
    BTGattService find(String UUID, long timeoutMS);

    /** Find a BluetoothGattService. If parameter UUID is not null,
      * the returned object will have to match it.
      * It will first check for existing objects. It will not turn on discovery
      * or connect to devices.
      * @parameter UUID optionally specify the UUID of the BluetoothGattService you are
      * waiting for
      * @return An object matching the UUID or null if not found before
      * timeout expires or event is canceled.
      */
    BTGattService find(String UUID);

    /* Bluetooth method calls: */

    /**
     * {@code jau.direct_bt}: Disconnect the LE or BREDR peer's GATT and HCI connection.
     * <p>
     * BT Core Spec v5.2: Vol 4, Part E HCI: 7.1.6 Disconnect command
     * </p>
     * <p>
     * The actual disconnect event will be delivered asynchronous and
     * the connection event can be caught via AdapterStatusListener::deviceDisconnected(..).
     * If unacceptable, {@link HCIStatusCode#UNACCEPTABLE_CONNECTION_PARAM} is being returned.
     * </p>
     * <p>
     * The device will be removed from the managing adapter's connected devices
     * when {@link AdapterStatusListener#deviceDisconnected(BTDevice, HCIStatusCode, long)} has been received.
     * </p>
     * <p>
     * An open GATT connection will also be closed.<br>
     * The connection to this device is removed, removing all connected profiles.
     * </p>
     * <p>
     * An application using one thread per device and rapid connect, should either use {@link #disconnect()} or {@link #remove()},
     * but never issue remove() after disconnect(). Doing so would eventually delete the device being already
     * in use by another thread due to discovery post disconnect!
     * </p>
     * @return {@link HCIStatusCode#SUCCESS} if the command has been accepted, otherwise {@link HCIStatusCode} may disclose reason for rejection.
     * @since 2.1.0 change API, i.e. return value from boolean to HCIStatusCode in favor of <i>direct_bt</i>
     * @implNote {@code jau.direct_bt} does not throw a BTException on error, only a 'general' exception in case of fatality like NPE etc (FIXME: Remove throws)
     */
    HCIStatusCode disconnect() throws BTException;

    /**
     * {@code jau.direct_bt}: Establish a default HCI connection to this device, using certain default parameter.
     * <p>
     * BT Core Spec v5.2: Vol 4, Part E HCI: 7.8.12 LE Create Connection command <br>
     * BT Core Spec v5.2: Vol 4, Part E HCI: 7.1.5 Create Connection command
     * </p>
     * <p>
     * Depending on this device's addressType,
     * either a BREDR (BDADDR_BREDR) or LE (BDADDR_LE_PUBLIC, BDADDR_LE_RANDOM) connection is attempted.<br>
     * If unacceptable, {@link HCIStatusCode#UNACCEPTABLE_CONNECTION_PARAM} is being returned.
     * </p>
     * <p>
     * The actual new connection handle will be delivered asynchronous and
     * the connection event can be caught via {@link AdapterStatusListener#deviceConnected(BTDevice, long)},
     * or if failed via {@link AdapterStatusListener#deviceDisconnected(BTDevice, HCIStatusCode, long)}.
     * </p>
     * <p>
     * The device is tracked by the managing adapter.
     * </p>
     * <p>
     * {@code tinyb.dbus}: A connection to this device is established, connecting each profile
     * flagged as auto-connectable.
     * </p>
     * @return {@link HCIStatusCode#SUCCESS} if the command has been accepted, otherwise {@link HCIStatusCode} may disclose reason for rejection.
     * @see #connectLE(short, short, short, short, short, short)
     * @since 2.1.0 change API, i.e. return value from boolean to HCIStatusCode in favor of {@code jau.direct_bt}.
     * @implNote {@code jau.direct_bt} does not throw a BTException on error, only a 'general' exception in case of fatality like NPE etc (FIXME: Remove throws)
     */
    HCIStatusCode connect() throws BTException;

    /**
     * Establish a HCI BDADDR_LE_PUBLIC or BDADDR_LE_RANDOM connection to this device.
     * <p>
     * BT Core Spec v5.2: Vol 4, Part E HCI: 7.8.12 LE Create Connection command
     * </p>
     * <p>
     * If this device's {@link #getAddressType()} is not BDADDR_LE_PUBLIC or BDADDR_LE_RANDOM,
     * {@link HCIStatusCode#UNACCEPTABLE_CONNECTION_PARAM} is being returned.
     * </p>
     * <p>
     * The actual new connection handle will be delivered asynchronous and
     * the connection event can be caught via {@link AdapterStatusListener#deviceConnected(BTDevice, long)},
     * or if failed via {@link AdapterStatusListener#deviceDisconnected(BTDevice, HCIStatusCode, long)}.
     * </p>
     * <p>
     * The device is tracked by the managing adapter.
     * </p>
     * <p>
     * Default parameter are used if {@code -1} has been passed for any of the arguments.
     * </p>
     * <p>
     * Default parameter values are chosen for using public address resolution
     * and usual connection latency, interval etc.
     * </p>
     * <p>
     * Set window to the same value as the interval, enables continuous scanning.
     * </p>
     *
     * @param le_scan_interval in units of 0.625ms, default value 24 for 15ms; Value range [4 .. 0x4000] for [2.5ms .. 10.24s]
     * @param le_scan_window in units of 0.625ms, default value 24 for 15ms; Value range [4 .. 0x4000] for [2.5ms .. 10.24s]. Shall be <= le_scan_interval
     * @param conn_interval_min in units of 1.25ms, default value 12 for 15ms; Value range [6 .. 3200] for [7.5ms .. 4000ms]
     * @param conn_interval_max in units of 1.25ms, default value 12 for 15ms; Value range [6 .. 3200] for [7.5ms .. 4000ms]
     * @param conn_latency slave latency in units of connection events, default value 0; Value range [0 .. 0x01F3].
     * @param supervision_timeout in units of 10ms, default value >= 10 x conn_interval_max, we use 500 ms minimum; Value range [0xA-0x0C80] for [100ms - 32s].
     * @return {@link HCIStatusCode#SUCCESS} if the command has been accepted, otherwise {@link HCIStatusCode} may disclose reason for rejection.
     * @see BTUtils#getHCIConnSupervisorTimeout(int, int, int, int)
     * @see #connect()
     * @since 2.1.0 change API, i.e. return value from boolean to HCIStatusCode in favor of <i>direct_bt</i>
     * @implNote not implemented in {@code tinyb.dbus}
     */
    HCIStatusCode connectLE(final short le_scan_interval, final short le_scan_window,
                            final short conn_interval_min, final short conn_interval_max,
                            final short conn_latency, final short supervision_timeout);


    /** Connects a specific profile available on the device, given by UUID
      * @param arg_UUID The UUID of the profile to be connected
      * @return TRUE if the profile connected successfully
      */
    boolean connectProfile(String arg_UUID) throws BTException;

    /** Disconnects a specific profile available on the device, given by UUID
      * @param arg_UUID The UUID of the profile to be disconnected
      * @return TRUE if the profile disconnected successfully
      */
    boolean disconnectProfile(String arg_UUID) throws BTException;

    /**
     * Returns the available {@link SMPKeyMask.KeyType} {@link SMPKeyMask} for the responder (LL slave) or initiator (LL master).
     * @param responder if true, queries the responder (LL slave) key, otherwise the initiator (LL master) key.
     * @return {@link SMPKeyMask.KeyType} {@link SMPKeyMask} result
     * @since 2.2.0
     * @implNote not implemented in {@code tinyb.dbus}
     */
    SMPKeyMask getAvailableSMPKeys(final boolean responder);

    /**
     * Returns a copy of the long term key (LTK) info, valid after connection and SMP pairing has been completed.
     * @param responder true will return the responder's LTK info (remote device, LL slave), otherwise the initiator's (the LL master).
     * @return the resulting key. {@link SMPLongTermKeyInfo#enc_size} will be zero if invalid.
     * @see {@link SMPPairingState#COMPLETED}
     * @see {@link AdapterStatusListener#deviceReady(BTDevice, long)}
     * @since 2.2.0
     * @implNote not implemented in {@code tinyb.dbus}
     */
    SMPLongTermKeyInfo getLongTermKeyInfo(final boolean responder);

    /**
     * Sets the long term ket (LTK) info of this device to reuse pre-paired encryption.
     * <p>
     * Must be called before connecting to this device, otherwise {@link HCIStatusCode#CONNECTION_ALREADY_EXISTS} will be returned.
     * </p>
     * @param ltk the pre-paired encryption LTK
     * @return {@link HCIStatusCode#SUCCESS} if successful, otherwise the appropriate error code.
     * @since 2.2.0
     * @implNote not implemented in {@code tinyb.dbus}
     */
    HCIStatusCode setLongTermKeyInfo(final SMPLongTermKeyInfo ltk);

    /**
     * Returns a copy of the Signature Resolving Key (LTK) info, valid after connection and SMP pairing has been completed.
     * @param responder true will return the responder's LTK info (remote device, LL slave), otherwise the initiator's (the LL master).
     * @return the resulting key
     * @see {@link SMPPairingState#COMPLETED}
     * @see {@link AdapterStatusListener#deviceReady(BTDevice, long)}
     * @since 2.2.0
     * @implNote not implemented in {@code tinyb.dbus}
     */
    SMPSignatureResolvingKeyInfo getSignatureResolvingKeyInfo(final boolean responder);

    /**
     * A secure connection to this device is established, and the device is then paired.
     * <p>
     * For direct_bt use {@link #setConnSecurity(BTSecurityLevel, SMPIOCapability) setConnSecurity(..) or its variants}
     * and {@link #connectLE(short, short, short, short, short, short) connectLE(..)}.
     * </p>
     * @return TRUE if the device connected and paired
     * @implNote not implemented in {@code jau.direct_bt}
     */
    boolean pair() throws BTException;

    /**
     * Unpairs this device from the adapter while staying connected.
     * <p>
     * All keys will be cleared within the adapter and host implementation.<br>
     * Should rarely being used by user.<br>
     * Internally being used to re-start pairing if GATT connection fails
     * in {@link PairingMode#PRE_PAIRED} mode.
     * </p>
     * @return {@link HCIStatusCode#SUCCESS} or an appropriate error status.
     * @since 2.1.0
     * @implNote not implemented in {@code tinyb.dbus}
     */
    HCIStatusCode unpair();

    /**
     * Set the {@link BTSecurityLevel} used to connect to this device on the upcoming connection.
     * <p>
     * Method returns false if {@link BTSecurityLevel#UNSET} has been given,
     * operation fails, this device has already being connected,
     * or {@link #connectLE(short, short, short, short, short, short) connectLE} or {@link #connect()} has been issued already.
     * </p>
     * <p>
     * To ensure a consistent authentication setup,
     * it is advised to set {@link SMPIOCapability#NO_INPUT_NO_OUTPUT} for sec_level <= {@link BTSecurityLevel#ENC_ONLY}
     * using {@link #setConnSecurity(BTSecurityLevel, SMPIOCapability, boolean) setConnSecurity(..)}
     * as well as an IO capable {@link SMPIOCapability} value
     * for {@link BTSecurityLevel#ENC_AUTH} or {@link BTSecurityLevel#ENC_AUTH_FIPS}.<br>
     * You may like to consider using {@link #setConnSecurityBest(BTSecurityLevel, SMPIOCapability)}.
     * </p>
     * @param sec_level {@link BTSecurityLevel} to be applied, {@link BTSecurityLevel#UNSET} will be ignored and method fails.
     * @return
     * @since 2.1.0
     * @implNote not implemented in {@code tinyb.dbus}
     * @see BTSecurityLevel
     * @see SMPIOCapability
     * @see #getConnSecurityLevel()
     * @see #setConnIOCapability(SMPIOCapability)
     * @see #getConnIOCapability()
     * @see #setConnSecurity(BTSecurityLevel, SMPIOCapability)
     * @see #setConnSecurityBest(BTSecurityLevel, SMPIOCapability)
     */
    boolean setConnSecurityLevel(final BTSecurityLevel sec_level);

    /**
     * Return the {@link BTSecurityLevel}, determined when the connection is established.
     * @since 2.1.0
     * @implNote not implemented in {@code tinyb.dbus}
     * @see BTSecurityLevel
     * @see SMPIOCapability
     * @see #setConnSecurityLevel(BTSecurityLevel)
     * @see #setConnIOCapability(SMPIOCapability, boolean)
     * @see #getConnIOCapability()
     * @see #setConnSecurity(BTSecurityLevel, SMPIOCapability, boolean)
     * @see #setConnSecurityBest(BTSecurityLevel, SMPIOCapability)
     */
    BTSecurityLevel getConnSecurityLevel();

    /**
     * Sets the given {@link SMPIOCapability} used to connect to this device on the upcoming connection.
     * <p>
     * Method returns false if {@link SMPIOCapability#UNSET} has been given,
     * operation fails, this device has already being connected,
     * or {@link #connectLE(short, short, short, short, short, short) connectLE} or {@link #connect()} has been issued already.
     * </p>
     * @param io_cap {@link SMPIOCapability} to be applied, {@link SMPIOCapability#UNSET} will be ignored and method fails.
     * @since 2.1.0
     * @implNote not implemented in {@code tinyb.dbus}
     * @see BTSecurityLevel
     * @see SMPIOCapability
     * @see #setConnSecurityLevel(BTSecurityLevel)
     * @see #getConnSecurityLevel()
     * @see #getConnIOCapability()
     * @see #setConnSecurity(BTSecurityLevel, SMPIOCapability)
     * @see #setConnSecurityBest(BTSecurityLevel, SMPIOCapability)
     */
    boolean setConnIOCapability(final SMPIOCapability io_cap);

    /**
     * Return the {@link SMPIOCapability} value, determined when the connection is established.
     * @since 2.1.0
     * @implNote not implemented in {@code tinyb.dbus}
     * @see BTSecurityLevel
     * @see SMPIOCapability
     * @see #setConnSecurityLevel(BTSecurityLevel)
     * @see #getConnSecurityLevel()
     * @see #setConnIOCapability(SMPIOCapability)
     * @see #setConnSecurity(BTSecurityLevel, SMPIOCapability)
     * @see #setConnSecurityBest(BTSecurityLevel, SMPIOCapability)
     */
    SMPIOCapability getConnIOCapability();

    /**
     * Sets the given {@link BTSecurityLevel} and {@link SMPIOCapability} used to connect to this device on the upcoming connection.
     * <p>
     * Method returns false if this device has already being connected,
     * or {@link #connectLE(short, short, short, short, short, short) connectLE} or {@link #connect()} has been issued already.
     * </p>
     * <p>
     * Method either changes both parameter for the upcoming connection or none at all.
     * </p>
     * @param sec_level {@link BTSecurityLevel} to be applied.
     * @param io_cap {@link SMPIOCapability} to be applied.
     * @since 2.1.0
     * @implNote not implemented in {@code tinyb.dbus}
     * @see BTSecurityLevel
     * @see SMPIOCapability
     * @see #setConnSecurityLevel(BTSecurityLevel)
     * @see #getConnSecurityLevel()
     * @see #setConnIOCapability(SMPIOCapability)
     * @see #getConnIOCapability()
     * @see #setConnSecurityBest(BTSecurityLevel, SMPIOCapability)
     */
    boolean setConnSecurity(final BTSecurityLevel sec_level, final SMPIOCapability io_cap);

    /**
     * Convenience method to determine the best practice {@link BTSecurityLevel} and {@link SMPIOCapability}
     * based on the given arguments, used to connect to this device on the upcoming connection.
     * <pre>
     *   if( BTSecurityLevel::UNSET < sec_level && SMPIOCapability::UNSET != io_cap ) {
     *      return setConnSecurity(sec_level, io_cap);
     *   } else if( BTSecurityLevel::UNSET < sec_level ) {
     *       if( BTSecurityLevel::ENC_ONLY >= sec_level ) {
     *           return setConnSecurity(sec_level, SMPIOCapability::NO_INPUT_NO_OUTPUT);
     *       } else {
     *           return setConnSecurityLevel(sec_level);
     *       }
     *   } else if( SMPIOCapability::UNSET != io_cap ) {
     *       return setConnIOCapability(io_cap);
     *   } else {
     *       return false;
     *   }
     * </pre>
     * <p>
     * Method returns false if {@link BTSecurityLevel#UNSET} and {@link SMPIOCapability#UNSET} has been given,
     * operation fails, this device has already being connected,
     * or {@link #connectLE(short, short, short, short, short, short) connectLE} or {@link #connect()} has been issued already.
     * </p>
     * @param sec_level {@link BTSecurityLevel} to be applied.
     * @param io_cap {@link SMPIOCapability} to be applied.
     * @since 2.1.0
     * @implNote not implemented in {@code tinyb.dbus}
     * @see BTSecurityLevel
     * @see SMPIOCapability
     * @see #setConnSecurityLevel(BTSecurityLevel)
     * @see #getConnSecurityLevel()
     * @see #setConnIOCapability(SMPIOCapability)
     * @see #getConnIOCapability()
     * @see #setConnSecurity(BTSecurityLevel, SMPIOCapability)
     */
    boolean setConnSecurityBest(final BTSecurityLevel sec_level, final SMPIOCapability io_cap);

    /**
     * Method sets the given passkey entry, see {@link PairingMode#PASSKEY_ENTRY_ini}.
     * <p>
     * Call this method if the device shall be securely paired with {@link PairingMode#PASSKEY_ENTRY_ini},
     * i.e. when notified via {@link AdapterStatusListener#devicePairingState(BTDevice, SMPPairingState, PairingMode, long) devicePairingState}
     * in state {@link SMPPairingState#PASSKEY_EXPECTED}.
     * </p>
     *
     * @param passkey used for {@link PairingMode#PASSKEY_ENTRY_ini} method.
     *        Will be encrypted before sending to counter-party.
     *
     * @return {@link HCIStatusCode#SUCCESS} if the command has been accepted, otherwise {@link HCIStatusCode} may disclose reason for rejection.
     * @see PairingMode
     * @see SMPPairingState
     * @see AdapterStatusListener#devicePairingState(BTDevice, SMPPairingState, PairingMode, long)
     * @see #setPairingPasskey(int)
     * @see #setPairingNumericComparison(boolean)
     * @see #getPairingMode()
     * @see #getPairingState()
     * @since 2.1.0
     * @implNote not implemented in {@code tinyb.dbus}
     */
    HCIStatusCode setPairingPasskey(final int passkey);

    /**
     * Method sets the numeric comparison result, see {@link PairingMode#NUMERIC_COMPARE_ini}.
     * <p>
     * Call this method if the device shall be securely paired with {@link PairingMode#NUMERIC_COMPARE_ini},
     * i.e. when notified via {@link AdapterStatusListener#devicePairingState(BTDevice, SMPPairingState, PairingMode, long) devicePairingState}
     * in state {@link SMPPairingState#NUMERIC_COMPARE_EXPECTED}.
     * </p>
     *
     * @param equal used for {@link PairingMode#NUMERIC_COMPARE_ini} method.
     *        Will be encrypted before sending to counter-party.
     *
     * @return {@link HCIStatusCode#SUCCESS} if the command has been accepted, otherwise {@link HCIStatusCode} may disclose reason for rejection.
     * @see PairingMode
     * @see SMPPairingState
     * @see AdapterStatusListener#devicePairingState(BTDevice, SMPPairingState, PairingMode, long)
     * @see #setPairingPasskey(int)
     * @see #setPairingNumericComparison(boolean)
     * @see #getPairingMode()
     * @see #getPairingState()
     * @since 2.1.0
     * @implNote not implemented in {@code tinyb.dbus}
     */
    HCIStatusCode setPairingNumericComparison(final boolean equal);

    /**
     * Returns the current {@link PairingMode} used by the device.
     * <p>
     * If the device is not paired, the current mode is {@link PairingMode#NONE}.
     * </p>
     * <p>
     * If the Pairing Feature Exchange is completed, i.e. {@link SMPPairingState#FEATURE_EXCHANGE_COMPLETED}
     * as notified by
     * {@link AdapterStatusListener#devicePairingState(BTDevice, SMPPairingState, PairingMode, long) devicePairingState}
     * the current mode reflects the currently used {@link PairingMode}.
     * </p>
     * <p>
     * In case the Pairing Feature Exchange is in progress, the current mode is {@link PairingMode#NEGOTIATING}.
     * </p>
     * @return current PairingMode.
     * @see PairingMode
     * @see SMPPairingState
     * @see AdapterStatusListener#devicePairingState(BTDevice, SMPPairingState, PairingMode, long)
     * @see #setPairingPasskey(int)
     * @see #setPairingNumericComparison(boolean)
     * @see #getPairingMode()
     * @see #getPairingState()
     * @since 2.1.0
     * @implNote not implemented in {@code tinyb.dbus}
     */
    PairingMode getPairingMode();

    /**
     * Returns the current {@link SMPPairingState}.
     * <p>
     * If the device is not paired, the current state is {@link SMPPairingState#NONE}.
     * </p>
     * @see PairingMode
     * @see SMPPairingState
     * @see AdapterStatusListener#devicePairingState(BTDevice, SMPPairingState, PairingMode, long)
     * @see #setPairingPasskey(int)
     * @see #setPairingNumericComparison(boolean)
     * @see #getPairingMode()
     * @see #getPairingState()
     * @since 2.1.0
     * @implNote not implemented in {@code tinyb.dbus}
     */
    SMPPairingState getPairingState();

    /**
     * Remove this device from the system (like an unpair).
     * <p>
     * Direct-BT: Disconnects this device via disconnect(..) if getConnected()==true
     * and explicitly removes its shared references from the Adapter:
     * connected-devices, discovered-devices and shared-devices.
     * </p>
     * <p>
     * This method shall be issued to ensure no device reference will
     * be leaked in a long lived adapter,
     * as only its reference within connected-devices and discovered-devices are removed at disconnect.
     * </p>
     * <p>
     * After calling this method, this instance is destroyed and shall not be used anymore!
     * </p>
     * <p>
     * This method is automatically called @ destructor.
     * </p>
     * <p>
     * This method is an atomic operation.
     * </p>
     * <p>
     * An application using one thread per device and rapid connect, should either use disconnect() or remove(),
     * but never issue remove() after disconnect() if the device is in use.
     * </p>
     * @return TRUE if the device has been removed
     * @throws BTException
     */
    boolean remove() throws BTException;

    /** Cancels an initiated pairing operation
      * @return TRUE if the paring is cancelled successfully
      */
    boolean cancelPairing() throws BTException;

    /** Returns a list of BluetoothGattServices available on this device.
      * @return A list of BluetoothGattServices available on this device,
      * NULL if an error occurred
      */
    List<BTGattService> getServices();

    /**
     * Issues a GATT ping to the device, validating whether it is still reachable.
     * <p>
     * This method could be periodically utilized to shorten the underlying OS disconnect period
     * after turning the device off, which lies within 7-13s.
     * </p>
     * <p>
     * In case the device is no more reachable, disconnect will be initiated due to the occurring IO error.
     * </p>
     * <p>
     * GATT services must have been initialized via {@link #getServices()}, otherwise {@code false} is being returned.
     * </p>
     * @return {@code true} if successful or not implemented, otherwise false in case no GATT services exists or is not connected..
     * @since 2.0.0
     * @implNote not implemented in {@code tinyb.dbus}.
     */
    boolean pingGATT();

    /**
     * Returns the timestamp in monotonic milliseconds when this device instance has been created,
     * either via its initial discovery or its initial direct connection.
     *
     * @see BTUtils#currentTimeMillis()
     * @since 2.0.0
     */
    long getCreationTimestamp();

    /**
     * Returns the timestamp in monotonic milliseconds when this device instance has
     * discovered or connected directly the last time.
     *
     * @see BTUtils#currentTimeMillis()
     * @since 2.0.0
     * @implNote not implemented in {@code tinyb.dbus}, returns {@link #getCreationTimestamp()}
     */
    long getLastDiscoveryTimestamp();

    /**
     * Returns the timestamp in monotonic milliseconds when this device instance underlying data
     * has been updated the last time.
     *
     * @see BTUtils#currentTimeMillis()
     * @since 2.0.0
     * @implNote not implemented in {@code tinyb.dbus}, returns {@link #getCreationTimestamp()}
     */
    long getLastUpdateTimestamp();

    /**
     * Returns the unique device {@link EUI48} address and {@link BDAddressType} type.
     * @since 2.2.0
     * @implNote not fully implemented in {@code tinyb.dbus}, uses {@link BDAddressType#BDADDR_LE_PUBLIC}
     */
    BDAddressAndType getAddressAndType();

    /**
     * Returns the hardware address of this device in its string representation.
     * @return The hardware address of this device as a string.
     * @since 2.2.0
     * @deprecated Use {@link #getAddressAndType()}
     */
    @Deprecated
    String getAddressString();

    /** Returns the remote friendly name of this device.
      * @return The remote friendly name of this device, or NULL if not set.
      */
    String getName();

    /** Returns an alternative friendly name of this device.
      * @return The alternative friendly name of this device, or NULL if not set.
      */
    String getAlias();

    /** Sets an alternative friendly name of this device.
      */
    void setAlias(String value);

    /** Returns the Bluetooth class of the device.
      * @return The Bluetooth class of the device.
      */
    int getBluetoothClass();

    /** Returns the appearance of the device, as found by GAP service.
      * @return The appearance of the device, as found by GAP service.
      */
    short getAppearance();

    /** Returns the proposed icon name of the device.
      * @return The proposed icon name, or NULL if not set.
      */
    String getIcon();

    /** Returns the paired state the device.
      * @return The paired state of the device.
      */
    boolean getPaired();

    /**
     * Enables notifications for the paired property and calls run function of the
     * BluetoothNotification object.
     * @param callback A BluetoothNotification<Boolean> object. Its run function will be called
     * when a notification is issued. The run function will deliver the new value of the paired
     * property.
     */
    void enablePairedNotifications(BTNotification<Boolean> callback);

    /**
     * Disables notifications of the paired property and unregisters the callback
     * object passed through the corresponding enable method.
     */
    void disablePairedNotifications();

    /** Returns the trusted state the device.
      * @return The trusted state of the device.
      */
    boolean getTrusted();

    /**
     * Enables notifications for the trusted property and calls run function of the
     * BluetoothNotification object.
     * @param callback A BluetoothNotification<Boolean> object. Its run function will be called
     * when a notification is issued. The run function will deliver the new value of the trusted
     * property.
     */
    void enableTrustedNotifications(BTNotification<Boolean> callback);

    /**
     * Disables notifications of the trusted property and unregisters the callback
     * object passed through the corresponding enable method.
     */
    void disableTrustedNotifications();

    /** Sets the trusted state the device.
      */
    void setTrusted(boolean value);

    /** Returns the blocked state the device.
      * @return The blocked state of the device.
      */
    boolean getBlocked();

    /**
     * Enables notifications for the blocked property and calls run function of the
     * BluetoothNotification object.
     * @param callback A BluetoothNotification<Boolean> object. Its run function will be called
     * when a notification is issued. The run function will deliver the new value of the blocked
     * property.
     */
    void enableBlockedNotifications(BTNotification<Boolean> callback);

    /**
     * Disables notifications of the blocked property and unregisters the callback
     * object passed through the corresponding enable method.
     */
    void disableBlockedNotifications();

    /** Sets the blocked state the device.
      */
    void setBlocked(boolean value);

    /** Returns if device uses only pre-Bluetooth 2.1 pairing mechanism.
      * @return True if device uses only pre-Bluetooth 2.1 pairing mechanism.
      */
    boolean getLegacyPairing();

    /** Returns the Received Signal Strength Indicator of the device.
      * @return The Received Signal Strength Indicator of the device.
      */
    short getRSSI();

    /**
     * Enables notifications for the RSSI property and calls run function of the
     * BluetoothNotification object.
     * @param callback A BluetoothNotification<Short> object. Its run function will be called
     * when a notification is issued. The run function will deliver the new value of the RSSI
     * property.
     */
    void enableRSSINotifications(BTNotification<Short> callback);

    /**
     * Disables notifications of the RSSI property and unregisters the callback
     * object passed through the corresponding enable method.
     */
    void disableRSSINotifications();

    /** Returns the connected state of the device.
      * @return The connected state of the device.
      */
    boolean getConnected();

    /**
     * Return the HCI connection handle to the LE or BREDR peer, zero if not connected.
     * @since 2.1.0
     * @implNote not implemented in {@code tinyb.dbus}
     */
    short getConnectionHandle();

    /**
     * Enables notifications for the connected property and calls run function of the
     * BluetoothNotification object.
     * @param callback A BluetoothNotification<Boolean> object. Its run function will be called
     * when a notification is issued. The run function will deliver the new value of the connected
     * property.
     */
    void enableConnectedNotifications(BTNotification<Boolean> callback);

    /**
     * Disables notifications of the connected property and unregisters the callback
     * object passed through the corresponding enable method.
     */
    void disableConnectedNotifications();

    /** Returns the UUIDs of the device.
      * @return Array containing the UUIDs of the device, ends with NULL.
      */
    String[] getUUIDs();

    /** Returns the local ID of the adapter.
      * @return The local ID of the adapter.
      */
    String getModalias();

    /** Returns the adapter on which this device was discovered or
      * connected.
      * @return The adapter.
      */
    BTAdapter getAdapter();

    /** Returns a map containing manufacturer specific advertisement data.
      * An entry has a uint16_t key and an array of bytes.
      * @return manufacturer specific advertisement data.
      */
    Map<Short, byte[]> getManufacturerData();

    /**
     * Enables notifications for the manufacturer data property and calls run function of the
     * BluetoothNotification object.
     * @param callback A BluetoothNotification<Map<Short, byte[]> > object. Its run function will be called
     * when a notification is issued. The run function will deliver the new value of the manufacturer data
     * property.
     */
    void enableManufacturerDataNotifications(
            BTNotification<Map<Short, byte[]>> callback);

    /**
     * Disables notifications of the manufacturer data property and unregisters the callback
     * object passed through the corresponding enable method.
     */
    void disableManufacturerDataNotifications();

    /** Returns a map containing service advertisement data.
      * An entry has a UUID string key and an array of bytes.
      * @return service advertisement data.
      */
    Map<String, byte[]> getServiceData();

    /**
     * Enables notifications for the service data property and calls run function of the
     * BluetoothNotification object.
     * @param callback A BluetoothNotification<Map<String, byte[]> > object. Its run function will be called
     * when a notification is issued. The run function will deliver the new value of the service data
     * property.
     */
    void enableServiceDataNotifications(
            BTNotification<Map<String, byte[]>> callback);

    /**
     * Disables notifications of the service data property and unregisters the callback
     * object passed through the corresponding enable method.
     */
    void disableServiceDataNotifications();

    /** Returns the transmission power level (0 means unknown).
      * @return the transmission power level (0 means unknown).
      */
    short getTxPower();

    /** Returns true if service discovery has ended.
      * @return true if the service discovery has ended.
      */
    boolean getServicesResolved();

    /**
     * Enables notifications for the services resolved property and calls run function of the
     * BluetoothNotification object.
     * @param callback A BluetoothNotification<Boolean> object. Its run function will be called
     * when a notification is issued. The run function will deliver the new value of the services resolved
     * property.
     */
    void enableServicesResolvedNotifications(
            BTNotification<Boolean> callback);

    /**
     * Disables notifications of the services resolved property and unregisters the callback
     * object passed through the corresponding enable method.
     */
    void disableServicesResolvedNotifications();

    /**
     * Add the given {@link BTGattCharListener} to the listener list if not already present.
     * <p>
     * To enable the actual BLE notification and/or indication, one needs to call
     * {@link BTGattChar#configNotificationIndication(boolean, boolean, boolean[])}
     * or {@link BTGattChar#enableNotificationOrIndication(boolean[])}.
     * </p>
     * @param listener A {@link BTGattCharListener} instance, listening to all {@link BTGattChar} events of this device
     * @return true if the given listener is not element of the list and has been newly added, otherwise false.
     * @throws IllegalStateException if the {@link BTDevice}'s BTGattHandler is null, i.e. not connected
     * @throws IllegalStateException if the given {@link BTGattCharListener} is already in use, i.e. added.
     * @see BTGattChar#configNotificationIndication(boolean, boolean, boolean[])
     * @see BTGattChar#enableNotificationOrIndication(boolean[])
     * @since 2.0.0
     * @implNote not implemented in {@code tinyb.dbus}
     */
    public boolean addCharListener(final BTGattCharListener listener)
        throws IllegalStateException;

    /**
     * Remove the given {@link BTGattCharListener} from the listener list.
     * <p>
     * If the {@link BTDevice}'s BTGattHandler is null, i.e. not connected, {@code false} is being returned.
     * </p>
     * @param listener A {@link BTGattCharListener} instance
     * @return true if the given listener is an element of the list and has been removed, otherwise false.
     * @since 2.0.0
     * @implNote not implemented in {@code tinyb.dbus}
     */
    public boolean removeCharListener(final BTGattCharListener l);

    /**
     * Remove all {@link BTGattCharListener} from the list, which are associated to the given {@link BTGattChar}.
     * <p>
     * Implementation tests all listener's {@link BTGattCharListener#getAssociatedChar()}
     * to match with the given associated characteristic.
     * </p>
     * @param associatedCharacteristic the match criteria to remove any BTGattCharListener from the list
     * @return number of removed listener.
     * @since 2.0.0
     * @implNote not implemented in {@code tinyb.dbus}
     */
    public int removeAllAssociatedCharListener(final BTGattChar associatedCharacteristic);

    /**
     * Remove all {@link BTGattCharListener} from the list.
     * @return number of removed listener.
     * @since 2.0.0
     * @implNote not implemented in {@code tinyb.dbus}
     */
    public int removeAllCharListener();
}
