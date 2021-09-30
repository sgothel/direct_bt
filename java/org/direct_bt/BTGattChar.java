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

/**
  * Provides access to Bluetooth GATT characteristic.
  *
  * @see [Bluetooth Specification](https://www.bluetooth.com/specifications/bluetooth-core-specification/)
  * @see [BlueZ GATT API](http://git.kernel.org/cgit/bluetooth/bluez.git/tree/doc/gatt-api.txt)
  */
public interface BTGattChar extends BTObject
{
    /**
     * {@link BTGattChar} event listener for notification and indication events.
     * <p>
     * This listener instance is attached to a {@link BTGattChar} via
     * {@link BTGattChar#addCharListener(Listener)} or {@link BTGattChar#addCharListener(Listener, boolean[])}
     * to listen to events associated with the {@link BTGattChar} instance.
     * </p>
     * <p>
     * The listener manager maintains a unique set of listener instances without duplicates.
     * </p>
     * <p>
     * Implementation will utilize a {@link BTGattCharListener) for the listener manager,
     * delegating matching {@link BTGattChar} events to this instance.
     * </p>
     */
    static public interface Listener {
        /**
         * Called from native BLE stack, initiated by a received notification associated
         * with the given {@link BTGattChar}.
         * @param charDecl {@link BTGattChar} related to this notification
         * @param value the notification value
         * @param timestamp monotonic timestamp at reception, see {@link BTUtils#currentTimeMillis()}
         */
        void notificationReceived(final BTGattChar charDecl,
                                  final byte[] value, final long timestamp);

        /**
         * Called from native BLE stack, initiated by a received indication associated
         * with the given {@link BTGattChar}.
         * @param charDecl {@link BTGattChar} related to this indication
         * @param value the indication value
         * @param timestamp monotonic timestamp at reception, see {@link BTUtils#currentTimeMillis()}
         * @param confirmationSent if true, the native stack has sent the confirmation, otherwise user is required to do so.
         */
        void indicationReceived(final BTGattChar charDecl,
                                final byte[] value, final long timestamp,
                                final boolean confirmationSent);
    };

    /**
     * Find a {@link BTGattDesc} by its desc_uuid.
     *
     * @parameter desc_uuid the UUID of the desired {@link BTGattDesc}
     * @return The matching descriptor or null if not found
     */
    BTGattDesc findGattDesc(final String desc_uuid);

    /** Reads the value of this characteristic.
      * @return A std::vector<unsgined char> containing the value of this characteristic.
      */
    byte[] readValue() throws BTException;

    /**
     * BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.3 Client Characteristic Configuration
     *
     * Method enables notification and/or indication for this characteristic at BLE level.
     *
     * Implementation masks this Characteristic properties PropertyBitVal::Notify and PropertyBitVal::Indicate
     * with the respective user request parameters, hence removes unsupported requests.
     *
     * Notification and/or indication configuration is only performed per characteristic if changed.
     *
     * It is recommended to utilize notification over indication, as its link-layer handshake
     * and higher potential bandwidth may deliver material higher performance.
     *
     * @param enableNotification
     * @param enableIndication
     * @param enabledState array of size 2, holding the resulting enabled state for notification and indication.
     * @return false if this characteristic has no PropertyBitVal::Notify or PropertyBitVal::Indication present,
     * or there is no GATTDescriptor of type ClientCharacteristicConfiguration, or if the operation has failed.
     * Otherwise returns true.
     *
     * @throws IllegalStateException if notification or indication is set to be enabled
     * and the {@link BTDevice}'s GATTHandler is null, i.e. not connected
     *
     * @see #disableIndicationNotification()
     * @see #enableNotificationOrIndication(boolean[])
     * @see #configNotificationIndication(boolean, boolean, boolean[])
     * @see #addCharListener(Listener)
     * @see #removeCharListener(Listener)
     * @see #removeAllAssociatedCharListener(boolean)
     * @since 2.0.0
     */
    boolean configNotificationIndication(final boolean enableNotification, final boolean enableIndication, final boolean enabledState[/*2*/])
            throws IllegalStateException;

    /**
     * BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.3 Client Characteristic Configuration
     *
     * Method will attempt to enable notification on the BLE level, if available,
     * otherwise indication if available.
     *
     * Notification and/or indication configuration is only performed per characteristic if changed.
     *
     * It is recommended to utilize notification over indication, as its link-layer handshake
     * and higher potential bandwidth may deliver material higher performance.
     *
     * @param enabledState array of size 2, holding the resulting enabled state for notification and indication.
     * @return false if this characteristic has no PropertyBitVal::Notify or PropertyBitVal::Indication present,
     * or there is no GATTDescriptor of type ClientCharacteristicConfiguration, or if the operation has failed.
     * Otherwise returns true.
     *
     * @throws IllegalStateException if notification or indication is set to be enabled
     * and the {@link BTDevice}'s GATTHandler is null, i.e. not connected
     *
     * @see #disableIndicationNotification()
     * @see #enableNotificationOrIndication(boolean[])
     * @see #configNotificationIndication(boolean, boolean, boolean[])
     * @see #addCharListener(Listener)
     * @see #removeCharListener(Listener)
     * @see #removeAllAssociatedCharListener(boolean)
     * @since 2.0.0
     */
    boolean enableNotificationOrIndication(final boolean enabledState[/*2*/])
            throws IllegalStateException;

    /**
     * BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.3 Client Characteristic Configuration
     *
     * Method will attempt to disable notification and indication on the BLE level.
     *
     * Notification and/or indication configuration is only performed per characteristic if changed.
     *
     * @return false if this characteristic has no PropertyBitVal::Notify or PropertyBitVal::Indication present,
     * or there is no BTGattDesc of type ClientCharacteristicConfiguration, or if the operation has failed.
     * Otherwise returns true.
     *
     * @throws IllegalStateException if notification or indication is set to be enabled
     * and the {@link BTDevice}'s GATTHandler is null, i.e. not connected
     *
     * @see #disableIndicationNotification()
     * @see #enableNotificationOrIndication(boolean[])
     * @see #configNotificationIndication(boolean, boolean, boolean[])
     * @see #addCharListener(Listener)
     * @see #removeCharListener(Listener)
     * @see #removeAllAssociatedCharListener(boolean)
     * @since 2.4.0
     */
    boolean disableIndicationNotification() throws IllegalStateException;

    /**
     * Add the given {@link BTGattChar.Listener} to the listener list if not already present.
     *
     * Occurring notifications and indications for this characteristic,
     * if enabled via {@link #configNotificationIndication(boolean, boolean, boolean[])}
     * or {@link #enableNotificationOrIndication(boolean[])},
     * will call the respective {@link BTGattChar.Listener} callback method.
     *
     * Implementation wraps given {@link BTGattChar.Listener} into a {@link BTGattCharListener}
     * to restrict the listener to listen only to this BTGattChar instance.
     *
     * {@link #removeCharListener(BTGattCharListener)} must be utilized with the returned {@link BTGattCharListener}.
     *
     * @param listener A {@link BTGattChar.Listener} instance, listening to this {@link BTGattChar}'s events
     * @return if successful, {@link BTGattCharListener} instance wrapping the given {@link BTGattChar.Listener} is returned, otherwise null.
     * @throws IllegalStateException if the DBTDevice's GATTHandler is null, i.e. not connected
     * @throws IllegalStateException if the given {@link BTGattChar.Listener} is already in use, i.e. added.
     * @see #disableIndicationNotification()
     * @see #enableNotificationOrIndication(boolean[])
     * @see #configNotificationIndication(boolean, boolean, boolean[])
     * @see #addCharListener(Listener)
     * @see #removeCharListener(Listener)
     * @see #removeAllAssociatedCharListener(boolean)
     * @since 2.4.0
     */
    BTGattCharListener addCharListener(final Listener listener)
            throws IllegalStateException;

    /**
     * Add the given {@link BTGattChar.Listener} to the listener list if not already present
     * and if enabling the notification <i>or</i> indication for this characteristic at BLE level was successful.<br>
     * Notification and/or indication configuration is only performed per characteristic if changed.
     *
     * Implementation will enable notification if available,
     * otherwise indication will be enabled if available. <br>
     * Implementation uses {@link #enableNotificationOrIndication(boolean[])} to enable either.
     *
     * Occurring notifications and indications for this characteristic
     * will call the respective {@link BTGattChar.Listener} callback method.
     *
     * Implementation wraps given {@link BTGattChar.Listener} into a {@link BTGattCharListener}
     * to restrict the listener to listen only to this BTGattChar instance.
     *
     * {@link #removeCharListener(BTGattCharListener)} must be utilized with the returned {@link BTGattCharListener}.
     *
     * @param listener A {@link BTGattChar.Listener} instance, listening to this {@link BTGattChar}'s events
     * @param enabledState array of size 2, holding the resulting enabled state for notification and indication
     * using {@link #enableNotificationOrIndication(boolean[])}
     * @return if enabling the notification and/or indication was successful
     * and if the given listener is not element of the list and has been newly added,
     * {@link BTGattCharListener} instance wrapping the given {@link BTGattChar.Listener} is returned, otherwise null.
     * @throws IllegalStateException if the {@link BTDevice}'s GATTHandler is null, i.e. not connected
     * @throws IllegalStateException if the given {@link BTGattChar.Listener} is already in use, i.e. added.
     * @see #disableIndicationNotification()
     * @see #enableNotificationOrIndication(boolean[])
     * @see #configNotificationIndication(boolean, boolean, boolean[])
     * @see #addCharListener(Listener)
     * @see #removeCharListener(Listener)
     * @see #removeAllAssociatedCharListener(boolean)
     * @since 2.4.0
     */
    BTGattCharListener addCharListener(final Listener listener, final boolean enabledState[/*2*/])
            throws IllegalStateException;

    /**
     * Remove the given associated {@link BTGattCharListener} from the listener list if present.
     *
     * To disables the notification and/or indication for this characteristic at BLE level
     * use {@link #disableIndicationNotification()} when desired.
     *
     * @param listener returned {@link BTGattCharListener} from {@link #addCharListener(Listener)} ...
     * @return true if successful, otherwise false.
     *
     * @throws IllegalStateException if the {@link BTDevice's}'s {@link BTGattHandler} is null, i.e. not connected
     *
     * @see #disableIndicationNotification()
     * @see #enableNotificationOrIndication(boolean[])
     * @see #configNotificationIndication(boolean, boolean, boolean[])
     * @see #addCharListener(Listener)
     * @see #removeCharListener(Listener)
     * @see #removeAllAssociatedCharListener(boolean)
     * @since 2.4.0
     */
    boolean removeCharListener(final BTGattCharListener listener);

    /**
     * Disables the notification and/or indication for this characteristic BLE level
     * if {@code disableIndicationNotification == true}
     * and removes all associated {@link BTGattChar.Listener} or {@link BTGattCharListener} from the listener list,
     * which are associated with this characteristic instance.
     * <p>
     * If the DBTDevice's GATTHandler is null, i.e. not connected, {@code false} is being returned.
     * </p>
     *
     * @param shallDisableIndicationNotification if true, disables the notification and/or indication for this characteristic
     * using {@link #disableIndicationNotification()}
     * @return number of removed listener.
     *
     * @see #disableIndicationNotification()
     * @see #enableNotificationOrIndication(boolean[])
     * @see #configNotificationIndication(boolean, boolean, boolean[])
     * @see #addCharListener(Listener)
     * @see #removeCharListener(Listener)
     * @see #removeAllAssociatedCharListener(boolean)
     * @since 2.0.0
     */
    int removeAllAssociatedCharListener(final boolean shallDisableIndicationNotification);

    /**
     * Writes the value of this characteristic,
     * using one of the following methods depending on {@code withResponse}
     * <pre>
     * BT Core Spec v5.2: Vol 3, Part G GATT: 4.9.3 Write Characteristic Value
     * BT Core Spec v5.2: Vol 3, Part G GATT: 4.9.1 Write Characteristic Value Without Response
     * </pre>
     * @param[in] arg_value The data as vector<uchar>
     * to be written packed in a GBytes struct
     * @param withResponse if {@code true} a subsequent ATT_WRITE_RSP is expected, otherwise not.
     * @return TRUE if value was written successfully
     * @since 2.0.0
     * @implNote {@code withResponse} parameter has been added since 2.0.0
     */
    boolean writeValue(byte[] argValue, boolean withResponse) throws BTException;

    /* D-Bus property accessors: */

    /** Get the UUID of this characteristic.
      * @return The 128 byte UUID of this characteristic, NULL if an error occurred
      */
    String getUUID();

    /** Returns the service to which this characteristic belongs to.
      * @return The service.
      */
    BTGattService getService();

    /** Returns true if notification for changes of this characteristic are activated.
      * @param enabledState array of size 2, storage for the current enabled state for notification and indication.
      * @return True if either notification or indication is enabled
      */
    boolean getNotifying(final boolean enabledState[/*2*/]);

    /**
     * Returns the properties of this characteristic.
     * <p>
     * BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.1.1 Characteristic Properties
     * </p>
     */
    GattCharPropertySet getProperties();

    /** Returns a list of BluetoothGattDescriptors this characteristic exposes.
      * @return A list of BluetoothGattDescriptors exposed by this characteristic
      * NULL if an error occurred
      */
    List<BTGattDesc> getDescriptors();
}
