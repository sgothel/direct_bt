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
import java.util.List;

import org.direct_bt.BTException;
import org.direct_bt.BTFactory;
import org.direct_bt.BTGattChar;
import org.direct_bt.BTGattDesc;
import org.direct_bt.BTGattService;
import org.direct_bt.BTNotification;
import org.direct_bt.BTObject;
import org.direct_bt.BTType;
import org.direct_bt.BTUtils;
import org.direct_bt.BTGattCharListener;

public class DBTGattChar extends DBTObject implements BTGattChar
{
    private static final boolean DEBUG = DBTManager.DEBUG;

    /** Characteristics's service weak back-reference */
    final WeakReference<DBTGattService> wbr_service;

    /**
     * Characteristic Handle of this instance.
     * <p>
     * Attribute handles are unique for each device (server) (BT Core Spec v5.2: Vol 3, Part F Protocol..: 3.2.2 Attribute Handle).
     * </p>
     */
    private final short handle;

    /* Characteristics Property */
    private final String[] properties;
    private final boolean hasNotify;
    private final boolean hasIndicate;

    /* Characteristics Value Type UUID */
    private final String value_type_uuid;

    /**
     * Characteristics Value Handle.
     * <p>
     * Attribute handles are unique for each device (server) (BT Core Spec v5.2: Vol 3, Part F Protocol..: 3.2.2 Attribute Handle).
     * </p>
     */
    private final short value_handle;

    /* Optional Client Characteristic Configuration index within descriptorList */
    private final int clientCharacteristicsConfigIndex;

    /* pp */ final List<BTGattDesc> descriptorList;

    private final boolean supCharValueCacheNotification;

    boolean enabledNotifyState = false;
    boolean enabledIndicateState = false;

    private byte[] cachedValue = null;
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

   /* pp */ DBTGattChar(final long nativeInstance, final DBTGattService service,
                                  final short handle, final String[] properties,
                                  final boolean hasNotify, final boolean hasIndicate,
                                  final String value_type_uuid, final short value_handle,
                                  final int clientCharacteristicsConfigIndex)
    {
        super(nativeInstance, handle /* hash */);
        this.wbr_service = new WeakReference<DBTGattService>(service);
        this.handle = handle;

        this.properties = properties;
        this.hasNotify = hasNotify;
        this.hasIndicate = hasIndicate;
        this.value_type_uuid = value_type_uuid;
        this.value_handle = value_handle;
        this.clientCharacteristicsConfigIndex = clientCharacteristicsConfigIndex;
        this.descriptorList = getDescriptorsImpl();
        this.supCharValueCacheNotification = DBTManager.getManager().getSettings().isCharValueCacheNotificationSupported();

        if( ( BTFactory.DEBUG || supCharValueCacheNotification ) &&
            ( hasNotify || hasIndicate )
          )
        {
            // This characteristicListener serves TinyB 'enableValueNotification(..)' and 'getValue()' (cached value)
            // backwards compatibility only!
            final Listener characteristicListener = new Listener() {
                @Override
                public void notificationReceived(final BTGattChar charDecl, final byte[] value, final long timestamp) {
                    final DBTGattChar cd = (DBTGattChar)charDecl;
                    if( !cd.equals(DBTGattChar.this) ) {
                        throw new InternalError("Filtered GATTCharacteristicListener.notificationReceived: Wrong Characteristic: Got "+charDecl+
                                                ", expected "+DBTGattChar.this.toString());
                    }
                    final boolean valueChanged;
                    if( supCharValueCacheNotification ) {
                        valueChanged = updateCachedValue(value, true);
                    } else {
                        valueChanged = true;
                    }
                    if( DEBUG ) {
                        System.err.println("GATTCharacteristicListener.notificationReceived: "+charDecl+
                                           ", value[changed "+valueChanged+", len "+value.length+": "+BTUtils.bytesHexString(value, 0, -1, true)+"]");
                    }
                }
                @Override
                public void indicationReceived(final BTGattChar charDecl, final byte[] value, final long timestamp,
                                               final boolean confirmationSent) {
                    final DBTGattChar cd = (DBTGattChar)charDecl;
                    if( !cd.equals(DBTGattChar.this) ) {
                        throw new InternalError("Filtered GATTCharacteristicListener.indicationReceived: Wrong Characteristic: Got "+charDecl+
                                                ", expected "+DBTGattChar.this.toString());
                    }
                    final boolean valueChanged;
                    if( supCharValueCacheNotification ) {
                        valueChanged = updateCachedValue(value, true);
                    } else {
                        valueChanged = true;
                    }
                    if( DEBUG ) {
                        System.err.println("GATTCharacteristicListener.indicationReceived: "+charDecl+
                                           ", value[changed "+valueChanged+", len "+value.length+": "+BTUtils.bytesHexString(value, 0, -1, true)+
                                           "], confirmationSent "+confirmationSent);
                    }
                }
            };
            this.addCharListener(characteristicListener); // silent, don't enable native GATT ourselves
        }
    }

    @Override
    public synchronized void close() {
        if( !isValid() ) {
            return;
        }
        removeAllAssociatedCharListener(true);
        super.close();
    }

    @Override
    public boolean equals(final Object obj)
    {
        if (obj == null || !(obj instanceof DBTGattChar)) {
            return false;
        }
        final DBTGattChar other = (DBTGattChar)obj;
        return handle == other.handle; /** unique attribute handles */
    }

    @Override
    public String getUUID() { return value_type_uuid; }

    @Override
    public BTType getBluetoothType() { return class_type(); }

    static BTType class_type() { return BTType.GATT_CHARACTERISTIC; }

    @Override
    public BTGattChar clone()
    { throw new UnsupportedOperationException(); } // FIXME

    @Override
    public BTGattDesc find(final String UUID, final long timeoutMS) {
        if( !checkServiceCache() ) {
            return null;
        }
        return (DBTGattDesc) findInCache(UUID, BTType.GATT_DESCRIPTOR);
    }

    @Override
    public BTGattDesc find(final String UUID) {
        return find(UUID, 0);
    }

    @Override
    public final BTGattService getService() { return wbr_service.get(); }

    @Override
    public final String[] getFlags() { return properties; }

    @Override
    public final byte[] getValue() { return cachedValue; }

    @Override
    public final byte[] readValue() throws BTException {
        if( supCharValueCacheNotification ) {
            final byte[] value = readValueImpl();
            updateCachedValue(value, true);
            return cachedValue;
        } else {
            return readValueImpl();
        }
    }

    @Override
    public final boolean writeValue(final byte[] value, final boolean withResponse) throws BTException {
        final boolean res = writeValueImpl(value, withResponse);
        if( supCharValueCacheNotification && res ) {
            updateCachedValue(value, false);
        }
        return res;
    }

    @Override
    public final List<BTGattDesc> getDescriptors() { return descriptorList; }

    @Override
    public final synchronized boolean configNotificationIndication(final boolean enableNotification, final boolean enableIndication, final boolean enabledState[/*2*/])
            throws IllegalStateException
    {
        if( hasNotify || hasIndicate ) {
            final boolean resEnableNotification = hasNotify && enableNotification;
            final boolean resEnableIndication = hasIndicate && enableIndication;

            if( resEnableNotification == enabledNotifyState &&
                resEnableIndication == enabledIndicateState )
            {
                enabledState[0] = resEnableNotification;
                enabledState[1] = resEnableIndication;
                if( DEBUG ) {
                    System.err.printf("GATTCharacteristic.configNotificationIndication: Unchanged: notification[shall %b, has %b: %b == %b], indication[shall %b, has %b: %b == %b]\n",
                        enableNotification, hasNotify, enabledNotifyState, resEnableNotification,
                        enableIndication, hasIndicate, enabledIndicateState, resEnableIndication);
                }
                return true;
            }

            final boolean res = configNotificationIndicationImpl(enableNotification, enableIndication, enabledState);
            if( DEBUG ) {
                System.err.printf("GATTCharacteristic.configNotificationIndication: res %b, notification[shall %b, has %b: %b -> %b], indication[shall %b, has %b: %b -> %b]\n",
                        res,
                        enableNotification, hasNotify, enabledNotifyState, resEnableNotification,
                        enableIndication, hasIndicate, enabledIndicateState, resEnableIndication);
            }
            if( res ) {
                enabledNotifyState = resEnableNotification;
                enabledIndicateState = resEnableIndication;
            }
            return res;
        } else {
            enabledState[0] = false;
            enabledState[1] = false;
            if( DEBUG ) {
                System.err.println("GATTCharacteristic.configNotificationIndication: FALSE*: hasNotify "+hasNotify+", hasIndicate "+hasIndicate);
            }
            return false;
        }
    }
    private native boolean configNotificationIndicationImpl(boolean enableNotification, boolean enableIndication, final boolean enabledState[/*2*/])
            throws IllegalStateException;

    @Override
    public boolean enableNotificationOrIndication(final boolean enabledState[/*2*/])
            throws IllegalStateException
    {
        final boolean enableNotification = hasNotify;
        final boolean enableIndication = !enableNotification && hasIndicate;

        return configNotificationIndication(enableNotification, enableIndication, enabledState);
    }

    static private class DelegatedBTGattCharListener extends BTGattCharListener {
        private final Listener delegate;

        public DelegatedBTGattCharListener(final BTGattChar characteristicMatch, final Listener l) {
            super(characteristicMatch);
            delegate = l;
        }

        @Override
        public void notificationReceived(final BTGattChar charDecl,
                                         final byte[] value, final long timestamp) {
            delegate.notificationReceived(charDecl, value, timestamp);
        }

        @Override
        public void indicationReceived(final BTGattChar charDecl,
                                       final byte[] value, final long timestamp,
                                       final boolean confirmationSent) {
            delegate.indicationReceived(charDecl, value, timestamp, confirmationSent);
        }
    };

    @Override
    public final boolean addCharListener(final Listener listener) {
        return getService().getDevice().addCharListener( new DelegatedBTGattCharListener(this, listener) );
    }

    @Override
    public final boolean addCharListener(final Listener listener, final boolean enabledState[/*2*/]) {
        if( !enableNotificationOrIndication(enabledState) ) {
            return false;
        }
        return addCharListener( listener );
    }

    @Override
    public final int removeAllAssociatedCharListener(final boolean disableIndicationNotification) {
        if( disableIndicationNotification ) {
            configNotificationIndication(false /* enableNotification */, false /* enableIndication */, new boolean[2]);
        }
        valueNotificationCB = null;
        return getService().getDevice().removeAllAssociatedCharListener(this);
    }

    @Override
    public final synchronized void enableValueNotifications(final BTNotification<byte[]> callback) {
        if( !configNotificationIndication(true /* enableNotification */, true /* enableIndication */, new boolean[2]) ) {
            valueNotificationCB = null;
        } else {
            valueNotificationCB = callback;
        }
    }

    @Override
    public final synchronized void disableValueNotifications() {
        configNotificationIndication(false /* enableNotification */, false /* enableIndication */, new boolean[2]);
        valueNotificationCB = null;
    }

    @Override
    public final boolean getNotifying() {
        return null != valueNotificationCB;
    }

    /**
     * Characteristic Handle of this instance.
     * <p>
     * Attribute handles are unique for each device (server) (BT Core Spec v5.2: Vol 3, Part F Protocol..: 3.2.2 Attribute Handle).
     * </p>
     */
    public final short getHandle() { return handle; }

    /**
     * Returns Characteristics Value Handle.
     * <p>
     * Attribute handles are unique for each device (server) (BT Core Spec v5.2: Vol 3, Part F Protocol..: 3.2.2 Attribute Handle).
     * </p>
     */
    public final short getValueHandle() { return value_handle; }

    /** Returns optional Client Characteristic Configuration index within descriptorList */
    public final int getClientCharacteristicsConfigIndex() { return clientCharacteristicsConfigIndex; }

    @Override
    public final String toString() {
        if( !isValid() ) {
            return "Characteristic" + "\u271D" + "[uuid "+getUUID()+", handle 0x"+Integer.toHexString(handle)+"]";
        }
        return toStringImpl();
    }

    /* Native method calls: */

    private native String toStringImpl();

    private native byte[] readValueImpl() throws BTException;

    private native boolean writeValueImpl(byte[] argValue, boolean withResponse) throws BTException;

    private native List<BTGattDesc> getDescriptorsImpl();

    @Override
    protected native void deleteImpl(long nativeInstance);

    /* local functionality */

    /* pp */ boolean checkServiceCache() {
        final DBTGattService service = wbr_service.get();
        if( null == service ) {
            return false;
        }
        final DBTDevice device = service.wbr_device.get();
        return null != device && device.checkServiceCache(false);
    }

    /**
     * Returns the matching {@link DBTObject} from the internal cache if found,
     * otherwise {@code null}.
     * <p>
     * The returned {@link DBTObject} may be of type
     * <ul>
     *   <li>{@link DBTGattDesc}</li>
     * </ul>
     * or alternatively in {@link BTObject} space
     * <ul>
     *   <li>{@link BTType#GATT_DESCRIPTOR} -> {@link BTGattDesc}</li>
     * </ul>
     * </p>
     * @param uuid UUID of the desired
     * {@link BTType#GATT_DESCRIPTOR descriptor} to be found.
     * Maybe {@code null}, in which case the first object of the desired type is being returned - if existing.
     * @param type specify the type of the object to be found, a {@link BTType#GATT_DESCRIPTOR descriptor}.
     * {@link BTType#NONE none} means anything.
     */
    /* pp */ DBTObject findInCache(final String uuid, final BTType type) {
        final boolean anyType = BTType.NONE == type;
        final boolean descType = BTType.GATT_DESCRIPTOR == type;

        if( !anyType && !descType ) {
            return null;
        }
        final int size = descriptorList.size();
        for(int i = 0; i < size; i++ ) {
            final DBTGattDesc descr = (DBTGattDesc) descriptorList.get(i);
            if( null == uuid || descr.getUUID().equals(uuid) ) {
                return descr;
            }
        }
        return null;
    }
}
