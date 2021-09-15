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
 * BTDevice represents one Bluetooth device.
 *
 * @see [Bluetooth Specification](https://www.bluetooth.com/specifications/bluetooth-core-specification/)
 */
public interface BTDevice extends BTObject
{
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
     * Add the given {@link AdapterStatusListener} to the list if not already present,
     * listening only for events matching this device.
     *
     * The AdapterStatusListener is released at remove() or this device's destruction.
     * <p>
     * The newly added {@link AdapterStatusListener} will receive an initial
     * {@link AdapterStatusListener#adapterSettingsChanged(BTAdapter, AdapterSettings, AdapterSettings, AdapterSettings, long) adapterSettingsChanged}
     * event, passing an {@link AdapterSettings empty oldMask and changedMask}, as well as {@link AdapterSettings current newMask}. <br>
     * This allows the receiver to be aware of this adapter's current settings.
     * </p>
     * @param listener A {@link AdapterStatusListener} instance
     * @param deviceMatch Optional {@link BTDevice} to be matched before calling any
     *        {@link AdapterStatusListener} {@code device*} methods. Pass {@code null} for no filtering.
     * @return true if the given listener is not element of the list and has been newly added, otherwise false.
     * @since 2.3.0
     * @see {@link BTDevice#addStatusListener(AdapterStatusListener, BTDevice)}
     * @see {@link #removeStatusListener(AdapterStatusListener)}
     * @see {@link #removeAllStatusListener()}
     */
    public boolean addStatusListener(final AdapterStatusListener listener);

    /**
     * Remove the given {@link AdapterStatusListener} from the list.
     * @param listener A {@link AdapterStatusListener} instance
     * @return true if the given listener is an element of the list and has been removed, otherwise false.
     * @since 2.3.0
     */
    public boolean removeStatusListener(final AdapterStatusListener l);

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
     */
    HCIStatusCode connectLE(final short le_scan_interval, final short le_scan_window,
                            final short conn_interval_min, final short conn_interval_max,
                            final short conn_latency, final short supervision_timeout);


    /**
     * Returns the available {@link SMPKeyMask.KeyType} {@link SMPKeyMask} for the responder (LL slave) or initiator (LL master).
     * @param responder if true, queries the responder (LL slave) key, otherwise the initiator (LL master) key.
     * @return {@link SMPKeyMask.KeyType} {@link SMPKeyMask} result
     * @since 2.2.0
     */
    SMPKeyMask getAvailableSMPKeys(final boolean responder);

    /**
     * Returns a copy of the long term key (LTK) info, valid after connection and SMP pairing has been completed.
     * @param responder true will return the responder's LTK info (remote device, LL slave), otherwise the initiator's (the LL master).
     * @return the resulting key. {@link SMPLongTermKeyInfo#enc_size} will be zero if invalid.
     * @see {@link SMPPairingState#COMPLETED}
     * @see {@link AdapterStatusListener#deviceReady(BTDevice, long)}
     * @since 2.2.0
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
     */
    HCIStatusCode setLongTermKeyInfo(final SMPLongTermKeyInfo ltk);

    /**
     * Returns a copy of the Signature Resolving Key (LTK) info, valid after connection and SMP pairing has been completed.
     * @param responder true will return the responder's LTK info (remote device, LL slave), otherwise the initiator's (the LL master).
     * @return the resulting key
     * @see {@link SMPPairingState#COMPLETED}
     * @see {@link AdapterStatusListener#deviceReady(BTDevice, long)}
     * @since 2.2.0
     */
    SMPSignatureResolvingKeyInfo getSignatureResolvingKeyInfo(final boolean responder);

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
     * @see BTSecurityLevel
     * @see SMPIOCapability
     * @see #setConnSecurityLevel(BTSecurityLevel)
     * @see #getConnSecurityLevel()
     * @see #setConnIOCapability(SMPIOCapability)
     * @see #getConnIOCapability()
     * @see #setConnSecurity(BTSecurityLevel, SMPIOCapability)
     * @see #setConnSecurityAuto(SMPIOCapability)
     */
    boolean setConnSecurityBest(final BTSecurityLevel sec_level, final SMPIOCapability io_cap);

    /**
     * Set automatic security negotiation of {@link BTSecurityLevel} and {@link SMPIOCapability} pairing mode.
     * <p>
     * Disabled by default and if set to {@link SMPIOCapability#UNSET}
     * </p>
     * Implementation iterates through below setup from highest security to lowest,
     * while performing a full connection attempt for each.
     * <pre>
     * BTSecurityLevel::ENC_AUTH_FIPS, iocap_auto*
     * BTSecurityLevel::ENC_AUTH,      iocap_auto*
     * BTSecurityLevel::ENC_ONLY,      SMPIOCapability::NO_INPUT_NO_OUTPUT
     * BTSecurityLevel::NONE,          SMPIOCapability::NO_INPUT_NO_OUTPUT
     *
     * (*): user SMPIOCapability choice of for authentication IO, skipped if ::SMPIOCapability::NO_INPUT_NO_OUTPUT
     * </pre>
     * <p>
     * Implementation may perform multiple connection and disconnect actions
     * until successful pairing or failure.
     * </p>
     * <p>
     * Intermediate {@link AdapterStatusListener#deviceConnected(BTDevice, short, long) deviceConnected(..)} and
     * {@link AdapterStatusListener#deviceDisconnected(BTDevice, HCIStatusCode, short, long) deviceDisconnected(..)}
     * callbacks are not delivered while negotiating. This avoids any interference by the user application.
     * </p>
     * @param auth_io_cap user {@link SMPIOCapability} choice for negotiation
     * @since 2.2.0
     * @see #isConnSecurityAutoEnabled()
     * @see BTSecurityLevel
     * @see SMPIOCapability
     */
    boolean setConnSecurityAuto(final SMPIOCapability iocap_auto);

    /**
     * Returns true if automatic security negotiation has been enabled via {@link #setConnSecurityAuto(SMPIOCapability)},
     * otherwise false.
     * @since 2.2.0
     * @see #setConnSecurityAuto(SMPIOCapability)
     */
    boolean isConnSecurityAutoEnabled();

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
     */
    HCIStatusCode setPairingPasskey(final int passkey);

    /**
     * Method replies with a negative passkey response, i.e. rejection, see {@link PairingMode#PASSKEY_ENTRY_ini}.
     * <p>
     * You may call this method if the device shall be securely paired with {@link PairingMode#PASSKEY_ENTRY_ini},
     * i.e. when notified via {@link AdapterStatusListener#devicePairingState(BTDevice, SMPPairingState, PairingMode, long) devicePairingState}
     * in state {@link SMPPairingState#PASSKEY_EXPECTED}.
     * </p>
     * <p>
     * Current experience exposed a roughly 3s immediate disconnect handshake with certain devices and/or Kernel BlueZ code.
     *
     * Hence using {@link #setPairingPasskey(int)} with {@code passkey = 0} is recommended, especially when utilizing
     * automatic security negotiation via {@link #setConnSecurityAuto()}!
     * </p>
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
     */
    HCIStatusCode setPairingPasskeyNegative();

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

    /**
     * Returns whether the device is valid, i.e. reference is valid
     * but not necessarily {@link #getConnected() connected}.
     * @return true if this device's references are valid and hasn't been {@link #remove()}'ed
     * @see #remove()
     * @since 2.2.0
     */
    public boolean isValid();

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
     */
    long getLastDiscoveryTimestamp();

    /**
     * Returns the timestamp in monotonic milliseconds when this device instance underlying data
     * has been updated the last time.
     *
     * @see BTUtils#currentTimeMillis()
     * @since 2.0.0
     */
    long getLastUpdateTimestamp();

    /**
     * Returns the unique device {@link EUI48} address and {@link BDAddressType} type.
     * @since 2.2.0
     */
    BDAddressAndType getAddressAndType();

    /** Returns the remote friendly name of this device.
      * @return The remote friendly name of this device, or NULL if not set.
      */
    String getName();

    /** Returns the Received Signal Strength Indicator of the device.
      * @return The Received Signal Strength Indicator of the device.
      */
    short getRSSI();

    /** Returns the connected state of the device.
      * @return The connected state of the device.
      */
    boolean getConnected();

    /**
     * Return the HCI connection handle to the LE or BREDR peer, zero if not connected.
     * @since 2.1.0
     */
    short getConnectionHandle();

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

    /** Returns the transmission power level (0 means unknown).
      * @return the transmission power level (0 means unknown).
      */
    short getTxPower();

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
     */
    public int removeAllAssociatedCharListener(final BTGattChar associatedCharacteristic);

    /**
     * Remove all {@link BTGattCharListener} from the list.
     * @return number of removed listener.
     * @since 2.0.0
     */
    public int removeAllCharListener();
}
