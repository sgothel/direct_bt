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

import java.lang.ref.WeakReference;

/**
 * {@link BTGattChar} event listener for notification and indication events.
 * <p>
 * A listener instance may be attached to a {@link BTGattChar} via
 * {@link BTGattChar#addCharListener(org.direct_bt.BTGattChar.Listener)} to listen to its events.
 * </p>
 * <p>
 * A listener instance may be attached to a {@link BTDevice} via
 * {@link BTDevice#addCharListener(BTGattCharListener, BTGattChar)}
 * to listen to all events of the device or the matching filtered events.
 * </p>
 * <p>
 * One {@link BTGattCharListener} instance can only be attached to a listener manager once at a time,
 * i.e. you cannot attach the same instance more than once to a {@link BTDevice}
 * or {@link BTGattChar}.
 * <br>
 * To attach multiple listener, one instance per attachment must be created.
 * <br>
 * This restriction is due to implementation semantics of strictly associating
 * one Java {@link BTGattCharListener} instance to one C++ {@code BTGattCharListener} instance.
 * The latter will be added to the native list of listeners.
 * This class's {@code nativeInstance} field links the Java instance to mentioned C++ listener.
 * <br>
 * Since the listener manager maintains a unique set of listener instances without duplicates,
 * this restriction is more esoteric.
 * </p>
 */
public abstract class BTGattCharListener {
    @SuppressWarnings("unused")
    private long nativeInstance;
    private final WeakReference<BTGattChar> associatedChar;

    /**
     * Returns the weakly associated {@link BTGattChar} to this listener instance.
     * <p>
     * Returns {@code null} if no association has been made
     * or if the associated {@link BTGattChar} has been garbage collected.
     * </p>
     */
    public final BTGattChar getAssociatedChar() {
        return null != associatedChar ? associatedChar.get() : null;
    }

    /**
     * @param associatedCharacteristic weakly associates this listener instance to one {@link BTGattChar},
     *        may be {@code null} for no association.
     * @see #getAssociatedChar()
     */
    public BTGattCharListener(final BTGattChar associatedCharacteristic) {
        if( null != associatedCharacteristic ) {
            this.associatedChar = new WeakReference<BTGattChar>(associatedCharacteristic);
        } else {
            this.associatedChar = null;
        }
    }

    /**
     * Called from native BLE stack, initiated by a received notification associated
     * with the given {@link BTGattChar}.
     * @param charDecl {@link BTGattChar} related to this notification
     * @param value the notification value
     * @param timestamp the indication monotonic timestamp, see {@link BTUtils#currentTimeMillis()}
     */
    public void notificationReceived(final BTGattChar charDecl,
                                     final byte[] value, final long timestamp) {
    }

    /**
     * Called from native BLE stack, initiated by a received indication associated
     * with the given {@link BTGattChar}.
     * @param charDecl {@link BTGattChar} related to this indication
     * @param value the indication value
     * @param timestamp the indication monotonic timestamp, see {@link BTUtils#currentTimeMillis()}
     * @param confirmationSent if true, the native stack has sent the confirmation, otherwise user is required to do so.
     */
    public void indicationReceived(final BTGattChar charDecl,
                                   final byte[] value, final long timestamp,
                                   final boolean confirmationSent) {
    }

    @Override
    public String toString() {
        final BTGattChar c = getAssociatedChar();
        final String cs = null != c ? c.toString() : "null";
        return "BTGattCharListener[associated "+cs+"]";
    }
};
