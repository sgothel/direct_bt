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

package jau.direct_bt;

import java.lang.ref.WeakReference;
import java.util.Arrays;

import org.direct_bt.BTException;
import org.direct_bt.BTGattDesc;
import org.direct_bt.BTNotification;
import org.direct_bt.BTType;

public class DBTGattDesc extends DBTObject implements BTGattDesc
{
    /** Descriptor's characteristic weak back-reference */
    final WeakReference<DBTGattChar> wbr_characteristic;

    /** Type of Descriptor */
    private final String type_uuid;

    /**
     * Characteristic Descriptor Handle
     * <p>
     * Attribute handles are unique for each device (server) (BT Core Spec v5.2: Vol 3, Part F Protocol..: 3.2.2 Attribute Handle).
     * </p>
     */
    private final short handle;

    private byte[] cachedValue;
    private BTNotification<byte[]> valueNotificationCB = null;

    private boolean updateCachedValue(final byte[] value, final boolean notify) {
        boolean valueChanged = false;
        if( null == cachedValue || cachedValue.length != value.length ) {
            cachedValue = new byte[value.length];
            valueChanged = true;
        } else if( !Arrays.equals(value, cachedValue) ) {
            valueChanged = true;
        }
        if( valueChanged ) {
            System.arraycopy(value, 0, cachedValue, 0, value.length);
            if( notify && null != valueNotificationCB ) {
                valueNotificationCB.run(cachedValue);
            }
        }
        return valueChanged;
    }

   /* pp */ DBTGattDesc(final long nativeInstance, final DBTGattChar characteristic,
                              final String type_uuid, final short handle, final byte[] value)
    {
        super(nativeInstance, handle /* hash */);
        this.wbr_characteristic = new WeakReference<DBTGattChar>(characteristic);
        this.type_uuid = type_uuid;
        this.handle = handle;
        this.cachedValue = value;
    }

    @Override
    public synchronized void close() {
        if( !isValid() ) {
            return;
        }
        disableValueNotifications();
        super.close();
    }

    @Override
    public boolean equals(final Object obj)
    {
        if (obj == null || !(obj instanceof DBTGattDesc)) {
            return false;
        }
        final DBTGattDesc other = (DBTGattDesc)obj;
        return handle == other.handle; /** unique attribute handles */
    }

    @Override
    public String getUUID() { return type_uuid; }

    @Override
    public BTType getBluetoothType() { return class_type(); }

    static BTType class_type() { return BTType.GATT_DESCRIPTOR; }

    @Override
    public final BTGattDesc clone()
    { throw new UnsupportedOperationException(); } // FIXME

    @Override
    public final DBTGattChar getCharacteristic() { return wbr_characteristic.get(); }

    @Override
    public final byte[] getValue() { return cachedValue; }

    @Override
    public final byte[] readValue() {
        final byte[] value = readValueImpl();
        updateCachedValue(value, true);
        return cachedValue;
    }

    @Override
    public final boolean writeValue(final byte[] value) throws BTException {
        final boolean res = writeValueImpl(value);
        if( res ) {
            updateCachedValue(value, false);
        }
        return res;
    }

    @Override
    public final synchronized void enableValueNotifications(final BTNotification<byte[]> callback) {
        valueNotificationCB = callback;
    }

    @Override
    public final synchronized void disableValueNotifications() {
        valueNotificationCB = null;
    }

    /**
     * Characteristic Descriptor Handle
     * <p>
     * Attribute handles are unique for each device (server) (BT Core Spec v5.2: Vol 3, Part F Protocol..: 3.2.2 Attribute Handle).
     * </p>
     */
    public final short getHandle() { return handle; }

    @Override
    public final String toString() {
        if( !isValid() ) {
            return "Descriptor" + "\u271D" + "[uuid "+getUUID()+", handle 0x"+Integer.toHexString(handle)+"]";
        }
        return toStringImpl();
    }

    /* Native method calls: */

    private native String toStringImpl();

    private native byte[] readValueImpl();

    private native boolean writeValueImpl(byte[] argValue) throws BTException;

    @Override
    protected native void deleteImpl(long nativeInstance);
}
