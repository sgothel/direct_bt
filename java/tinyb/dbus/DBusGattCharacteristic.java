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

package tinyb.dbus;

import java.util.List;

import org.direct_bt.BTException;
import org.direct_bt.BTGattChar;
import org.direct_bt.BTGattDesc;
import org.direct_bt.BTGattService;
import org.direct_bt.BTManager;
import org.direct_bt.BTNotification;
import org.direct_bt.BTType;

public class DBusGattCharacteristic extends DBusObject implements BTGattChar
{
    @Override
    public native BTType getBluetoothType();
    @Override
    public native DBusGattCharacteristic clone();

    static BTType class_type() { return BTType.GATT_CHARACTERISTIC; }

    @Override
    public BTGattDesc find(final String UUID, final long timeoutMS) {
        final BTManager manager = DBusManager.getManager();
        return (BTGattDesc) manager.find(BTType.GATT_DESCRIPTOR,
                null, UUID, this, timeoutMS);
    }

    @Override
    public BTGattDesc find(final String UUID) {
        return find(UUID, 0);
    }

    /* D-Bus method calls: */

    @Override
    public native byte[] readValue() throws BTException;

    @Override
    public native void enableValueNotifications(BTNotification<byte[]> callback);

    @Override
    public native void disableValueNotifications();

    @Override
    public boolean writeValue(final byte[] argValue, final boolean withResponse) throws BTException {
        if( withResponse ) {
            throw new DBusBluetoothException("writeValue with response not yet supported");
        }
        return writeValueImpl(argValue);
    }
    private native boolean writeValueImpl(byte[] argValue) throws BTException;

    /* D-Bus property accessors: */

    @Override
    public native String getUUID();

    @Override
    public native BTGattService getService();

    @Override
    public native byte[] getValue();

    @Override
    public native boolean getNotifying();

    @Override
    public native String[] getFlags();

    @Override
    public native List<BTGattDesc> getDescriptors();

    private native void init(DBusGattCharacteristic obj);

    private native void delete();

    private DBusGattCharacteristic(final long instance)
    {
        super(instance);
    }

    @Override
    public boolean addCharListener(final Listener listener) {
        return false; // FIXME
    }
    @Override
    public boolean configNotificationIndication(final boolean enableNotification, final boolean enableIndication, final boolean[] enabledState)
            throws IllegalStateException
    {
        return false; // FIXME
    }
    @Override
    public boolean enableNotificationOrIndication(final boolean[] enabledState)
            throws IllegalStateException
    {
        return false; // FIXME
    }
    @Override
    public boolean addCharListener(final Listener listener, final boolean[] enabledState)
            throws IllegalStateException
    {
        return false; // FIXME
    }
    @Override
    public int removeAllAssociatedCharListener(final boolean disableIndicationNotification) {
        return 0; // FIXME
    }

    @Override
    public String toString() {
        return "Characteristic[uuid "+getUUID()+"]";
    }
}
