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

/**
  * Provides access to Bluetooth GATT descriptor.
  *
  * @see [Bluetooth Specification](https://www.bluetooth.com/specifications/bluetooth-core-specification/)
  * @see [BlueZ GATT API](http://git.kernel.org/cgit/bluetooth/bluez.git/tree/doc/gatt-api.txt)
  */
public interface BTGattDesc extends BTObject
{
    @Override
    public BTGattDesc clone();

    /* D-Bus method calls: */

    /** Reads the value of this descriptor
      * @return A vector<uchar> containing data from this descriptor
      */
    public byte[] readValue();

    /** Writes the value of this descriptor.
      * @param[in] arg_value The data as vector<uchar>
      * to be written packed in a GBytes struct
      * @return TRUE if value was written succesfully
      */
    public boolean writeValue(byte[] argValue) throws BTException;

    /**
     * Enables notifications for the value and calls run function of the BluetoothNotification
     * object.
     * @param callback A BluetoothNotification<byte[]> object. Its run function will be called
     * when a notification is issued. The run function will deliver the new value of the value
     * property.
     */
    public void enableValueNotifications(BTNotification<byte[]> callback);
    /**
     * Disables notifications of the value and unregisters the callback object
     * passed through the corresponding enable method.
     */
    public void disableValueNotifications();

    /* D-Bus property accessors: */

    /** Get the UUID of this descriptor.
      * @return The 128 byte UUID of this descriptor, NULL if an error occurred
      */
    public String getUUID();

    /** Returns the characteristic to which this descriptor belongs to.
      * @return The characteristic.
      */
    public BTGattChar getCharacteristic();

    /** Returns the cached value of this descriptor, if any.
      * @return The cached value of this descriptor.
      */
    public byte[] getValue();
}
