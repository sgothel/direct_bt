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

import java.util.Iterator;
import java.util.List;

/**
 * Representing a Gatt Service object from the GATT client perspective.
 *
 * BT Core Spec v5.2: Vol 3, Part G GATT: 3.1 Service Definition
 *
 * Includes a complete [Primary] Service Declaration
 * including its list of Characteristic Declarations,
 * which also may include its client config if available.
 */
public interface BTGattService extends BTObject
{
    /**
     * Find a {@link BTGattChar} by its char_uuid.
     *
     * @parameter char_uuid the UUID of the desired {@link BTGattChar}
     * @return The matching characteristic or null if not found
     */
    BTGattChar findGattChar(String char_uuid);

    /** Get the UUID of this service
      * @return The 128 byte UUID of this service, NULL if an error occurred
      */
    String getUUID();

    /** Returns the device to which this service belongs to.
      * @return The device.
      */
    BTDevice getDevice();

    /** Returns true if this service is a primary service, false if secondary.
      * @return true if this service is a primary service, false if secondary.
      */
    boolean getPrimary();

    /** Returns a list of BTGattChar this service exposes.
      * @return A list of BTGattChar exposed by this service
      */
    List<BTGattChar> getChars();

    /**
     * Adds the given {@link BTGattCharListener} to the {@link BTDevice}
     * and {@link BTGattChar#enableNotificationOrIndication(boolean[])} for all {@link BTGattChar} instances.
     * @param listener {@link BTGattCharListener} to add to the {@link BTDevice}.
     *        It is important to have hte listener's {@link BTGattCharListener#getAssociatedChar() associated characteristic} == null,
     *        otherwise the listener can't be used for all characteristics.
     * @return true if successful, otherwise false
     * @throws IllegalArgumentException if listener's {@link BTGattCharListener#getAssociatedChar() associated characteristic}
     * is not null.
     * @since 2.0.0
     * @see BTGattChar#enableNotificationOrIndication(boolean[])
     * @see BTDevice#addCharListener(BTGattCharListener, BTGattChar)
     */
    public static boolean addCharListenerToAll(final BTDevice device, final List<BTGattService> services,
                                               final BTGattCharListener listener) {
        if( null == listener ) {
            throw new IllegalArgumentException("listener argument null");
        }
        if( null != listener.getAssociatedChar() ) {
            throw new IllegalArgumentException("listener's associated characteristic is not null");
        }
        final boolean res = device.addCharListener(listener);
        for(final Iterator<BTGattService> is = services.iterator(); is.hasNext(); ) {
            final BTGattService s = is.next();
            final List<BTGattChar> characteristics = s.getChars();
            for(final Iterator<BTGattChar> ic = characteristics.iterator(); ic.hasNext(); ) {
                ic.next().enableNotificationOrIndication(new boolean[2]);
            }
        }
        return res;
    }

    /**
     * Removes the given {@link BTGattCharListener} from the {@link BTDevice}.
     * @param listener {@link BTGattCharListener} to remove from the {@link BTDevice}.
     * @return true if successful, otherwise false
     * @since 2.0.0
     * @see BTGattChar#configNotificationIndication(boolean, boolean, boolean[])
     * @see BTDevice#removeCharListener(BTGattCharListener)
     */
    public static boolean removeCharListenerFromAll(final BTDevice device, final List<BTGattService> services,
                                                    final BTGattCharListener listener) {
        for(final Iterator<BTGattService> is = services.iterator(); is.hasNext(); ) {
            final BTGattService s = is.next();
            final List<BTGattChar> characteristics = s.getChars();
            for(final Iterator<BTGattChar> ic = characteristics.iterator(); ic.hasNext(); ) {
                ic.next().configNotificationIndication(false /* enableNotification */, false /* enableIndication */, new boolean[2]);
            }
        }
        return device.removeCharListener(listener);
    }

    /**
     * Removes all {@link BTGattCharListener} from the {@link BTDevice}.
     * @return count of removed {@link BTGattCharListener}
     * @since 2.0.0
     * @see BTGattChar#configNotificationIndication(boolean, boolean, boolean[])
     * @see BTDevice#removeAllCharListener()
     */
    public static int removeAllCharListener(final BTDevice device, final List<BTGattService> services) {
        for(final Iterator<BTGattService> is = services.iterator(); is.hasNext(); ) {
            final BTGattService s = is.next();
            final List<BTGattChar> characteristics = s.getChars();
            for(final Iterator<BTGattChar> ic = characteristics.iterator(); ic.hasNext(); ) {
                ic.next().configNotificationIndication(false /* enableNotification */, false /* enableIndication */, new boolean[2]);
            }
        }
        return device.removeAllCharListener();
    }

    @Override
    String toString();
}
