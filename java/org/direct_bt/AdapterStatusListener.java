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

/**
 * {@link BTAdapter} status listener for {@link BTDevice} discovery events: Added, updated and removed;
 * as well as for certain {@link BTAdapter} events.
 * <p>
 * User implementations shall return as early as possible to avoid blocking the event-handler thread,
 * if not specified within the methods otherwise (see {@link #deviceReady(BTDevice, long)}).<br>
 * Especially complex mutable operations on DBTDevice or DBTAdapter should be issued off-thread!
 * </p>
 * <p>
 * A listener instance may be attached to a {@link BTAdapter} via
 * {@link BTAdapter#addStatusListener(AdapterStatusListener, BTDevice)}.
 * </p>
 * <p>
 * One {@link AdapterStatusListener} instance can only be attached to a listener receiver once at a time,
 * i.e. you cannot attach the same instance more than once to a {@link BTAdapter}.
 * <br>
 * To attach multiple listener, one instance per attachment must be created.
 * <br>
 * This restriction is due to implementation semantics of strictly associating
 * one Java {@link AdapterStatusListener} instance to one C++ {@code AdapterStatusListener} instance.
 * The latter will be added to the native list of listeners.
 * This class's {@code nativeInstance} field links the Java instance to mentioned C++ listener.
 * <br>
 * Since the listener receiver maintains a unique set of listener instances without duplicates,
 * this restriction is more esoteric.
 * </p>
 * @since 2.0.0
 */
public abstract class AdapterStatusListener {
    @SuppressWarnings("unused")
    private volatile long nativeInstance;

    /**
     * Called from native JNIAdapterStatusListener dtor
     * i.e. native instance destructed in native land.
     */
    @SuppressWarnings("unused")
    private final void notifyDeleted() {
        nativeInstance = 0;
    }

    /**
     * {@link BTAdapter} setting(s) changed.
     * @param adapter the adapter which settings have changed.
     * @param oldmask the previous settings mask. {@link AdapterSettings#isEmpty()} indicates the initial setting notification,
     *        see {@link BTAdapter#addStatusListener(AdapterStatusListener, BTDevice) addStatusListener(..)}.
     * @param newmask the new settings mask
     * @param changedmask the changes settings mask. {@link AdapterSettings#isEmpty()} indicates the initial setting notification,
     *        see {@link BTAdapter#addStatusListener(AdapterStatusListener, BTDevice) addStatusListener(..)}.
     * @param timestamp the time in monotonic milliseconds when this event occurred. See {@link BTUtils#currentTimeMillis()}.
     */
    public void adapterSettingsChanged(final BTAdapter adapter,
                                       final AdapterSettings oldmask, final AdapterSettings newmask,
                                       final AdapterSettings changedmask, final long timestamp) { }

    /**
     * {@link BTAdapter}'s discovery state has changed, i.e. enabled or disabled.
     * @param adapter the adapter which discovering state has changed.
     * @param currentMeta the current meta {@link ScanType}
     * @param changedType denotes the changed {@link ScanType}
     * @param changedEnabled denotes whether the changed {@link ScanType} has been enabled or disabled
     * @param keepAlive if {@code true}, the denoted changed {@link ScanType} will be re-enabled if disabled by the underlying Bluetooth implementation.
     * @param timestamp the time in monotonic milliseconds when this event occurred. See {@link BTUtils#currentTimeMillis()}.
     */
    public void discoveringChanged(final BTAdapter adapter, final ScanType currentMeta, final ScanType changedType, final boolean changedEnabled, final boolean keepAlive, final long timestamp) { }

    /**
     * A {@link BTDevice} has been newly discovered.
     * <p>
     * The boolean return value informs the adapter whether the device shall be made persistent for connection {@code true},
     * or that it can be discarded {@code false}.<br>
     * If no registered {@link AdapterStatusListener#deviceFound(BTDevice, long) deviceFound(..)} implementation returns {@code true},
     * the device instance will be removed from all internal lists and can no longer being used.<br>
     * If any registered {@link AdapterStatusListener#deviceFound(BTDevice, long) deviceFound(..)} implementation returns {@code true},
     * the device will be made persistent, is ready to connect and {@link BTDevice#remove() remove} shall be called after usage.
     * </p>
     * @param device the found device
     * @param timestamp the time in monotonic milliseconds when this event occurred. See {@link BTUtils#currentTimeMillis()}.
     * @return true if the device shall be made persistent and {@link BTDevice#remove() remove} issued later. Otherwise false to remove device right away.
     */
    public boolean deviceFound(final BTDevice device, final long timestamp) { return false; }

    /**
     * An already discovered {@link BTDevice} has been updated.
     * @param device the updated device
     * @param updateMask the update mask of changed data
     * @param timestamp the time in monotonic milliseconds when this event occurred. See {@link BTUtils#currentTimeMillis()}.
     */
    public void deviceUpdated(final BTDevice device, final EIRDataTypeSet updateMask, final long timestamp) { }

    /**
     * {@link BTDevice} got connected.
     * @param device the device which has been connected, holding the new connection handle.
     * @param handle the new connection handle, which has been assigned to the device already
     * @param timestamp the time in monotonic milliseconds when this event occurred. See {@link BTUtils#currentTimeMillis()}.
     */
    public void deviceConnected(final BTDevice device, final short handle, final long timestamp) { }

    /**
     * An already connected {@link BTDevice}'s {@link SMPPairingState} has changed.
     * @param device the device which {@link PairingMode} has been changed.
     * @param state the current {@link SMPPairingState} of the connected device, see DBTDevice::getCurrentPairingState()
     * @param mode the current {@link PairingMode} of the connected device, see DBTDevice::getCurrentPairingMode()
     * @param timestamp the time in monotonic milliseconds when this event occurred. See {@link BTUtils#currentTimeMillis()}.
     * @see BTDevice#setPairingPasskey(int)
     * @see BTDevice#setPairingNumericComparison(boolean)
     */
    public void devicePairingState(final BTDevice device, final SMPPairingState state, final PairingMode mode, final long timestamp) {}

    /**
     * {@link BTDevice} is ready for user (GATT) processing, i.e. already connected, optionally paired and ATT MTU size negotiated via connected GATT.
     * <p>
     * Method is being called from a dedicated native thread, hence restrictions on method duration and complex mutable operations don't apply here.
     * </p>
     * @param device the device ready to use
     * @param timestamp the time in monotonic milliseconds when this event occurred. See BasicTypes::getCurrentMilliseconds().
     * @see {@link SMPPairingState#COMPLETED}
     */
    public void deviceReady(final BTDevice device, final long timestamp) {}

    /**
     * {@link BTDevice} got disconnected.
     * @param device the device which has been disconnected with zeroed connection handle.
     * @param reason the {@link HCIStatusCode} reason for disconnection
     * @param handle the disconnected connection handle, which has been unassigned from the device already
     * @param timestamp the time in monotonic milliseconds when this event occurred. See {@link BTUtils#currentTimeMillis()}.
     */
    public void deviceDisconnected(final BTDevice device, final HCIStatusCode reason, final short handle, final long timestamp) { }

    @Override
    public String toString() {
        return "AdapterStatusListener[anonymous]";
    }
};
