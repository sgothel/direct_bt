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

import java.util.Iterator;
import java.util.List;

/**
  * Provides access to Bluetooth GATT characteristic.
  *
  * @see [Bluetooth Specification](https://www.bluetooth.com/specifications/bluetooth-core-specification/)
  * @see [BlueZ GATT API](http://git.kernel.org/cgit/bluetooth/bluez.git/tree/doc/gatt-api.txt)
  */
public interface BTGattService extends BTObject
{
    @Override
    public BTGattService clone();

    /** Find a BluetoothGattCharacteristic. If parameter UUID is not null,
      * the returned object will have to match it.
      * It will first check for existing objects. It will not turn on discovery
      * or connect to devices.
      * @parameter UUID optionally specify the UUID of the BluetoothGattCharacteristic you are
      * waiting for
      * @parameter timeoutMS the function will return after timeout time in milliseconds, a
      * value of zero means wait forever. If object is not found during this time null will be returned.
      * @return An object matching the UUID or null if not found before
      * timeout expires or event is canceled.
      */
    public BTGattChar find(String UUID, long timeoutMS);

    /** Find a BluetoothGattCharacteristic. If parameter UUID is not null,
      * the returned object will have to match it.
      * It will first check for existing objects. It will not turn on discovery
      * or connect to devices.
      * @parameter UUID optionally specify the UUID of the BluetoothGattDescriptor you are
      * waiting for
      * @return An object matching the UUID or null if not found before
      * timeout expires or event is canceled.
      */
    public BTGattChar find(String UUID);

    /* D-Bus property accessors: */

    /** Get the UUID of this service
      * @return The 128 byte UUID of this service, NULL if an error occurred
      */
    public String getUUID();

    /** Returns the device to which this service belongs to.
      * @return The device.
      */
    public BTDevice getDevice();

    /** Returns true if this service is a primary service, false if secondary.
      * @return true if this service is a primary service, false if secondary.
      */
    public boolean getPrimary();

    /** Returns a list of BTGattChar this service exposes.
      * @return A list of BTGattChar exposed by this service
      */
    public List<BTGattChar> getChars();

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
     * @implNote not implemented in tinyb.dbus
     * @see BTGattChar#enableNotificationOrIndication(boolean[])
     * @see BTDevice#addCharacteristicListener(BTGattCharListener, BTGattChar)
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
     * @implNote not implemented in tinyb.dbus
     * @see BTGattChar#configNotificationIndication(boolean, boolean, boolean[])
     * @see BTDevice#removeCharacteristicListener(BTGattCharListener)
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
     * @implNote not implemented in tinyb.dbus
     * @see BTGattChar#configNotificationIndication(boolean, boolean, boolean[])
     * @see BTDevice#removeAllCharacteristicListener()
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
}
