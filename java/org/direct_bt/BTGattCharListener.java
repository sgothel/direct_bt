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

import jau.direct_bt.DBTNativeDownlink;

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
 * The listener manager maintains a unique set of listener instances without duplicates.
 * </p>
 */
public abstract class BTGattCharListener extends DBTNativeDownlink {
    public BTGattCharListener() {
        super(); // pending native ctor
        if( BTFactory.isInitialized() ) {
            initDownlink(ctorImpl());
        } else {
            System.err.println("BTGattCharListener.ctor: BTFactory not initialized, no nativeInstance");
        }
    }
    private native long ctorImpl();

    @Override
    protected native void deleteImpl(long nativeInstance);

    /**
     * Returns true if native instance is valid, otherwise false.
     */
    public final boolean isValid() { return isNativeValid(); }

    /**
     * Called from native BLE stack, initiated by a received notification associated
     * with the given {@link BTGattChar}.
     * @param charDecl {@link BTGattChar} related to this notification
     * @param value the notification value
     * @param timestamp monotonic timestamp at reception, see {@link BTUtils#currentTimeMillis()}
     */
    public void notificationReceived(final BTGattChar charDecl,
                                     final byte[] value, final long timestamp) {
    }

    /**
     * Called from native BLE stack, initiated by a received indication associated
     * with the given {@link BTGattChar}.
     * @param charDecl {@link BTGattChar} related to this indication
     * @param value the indication value
     * @param timestamp monotonic timestamp at reception, see {@link BTUtils#currentTimeMillis()}
     * @param confirmationSent if true, the native stack has sent the confirmation, otherwise user is required to do so.
     */
    public void indicationReceived(final BTGattChar charDecl,
                                   final byte[] value, final long timestamp,
                                   final boolean confirmationSent) {
    }

    @Override
    public String toString() {
        return "BTGattCharListener[nativeValid "+isValid()+"]";
    }
};
