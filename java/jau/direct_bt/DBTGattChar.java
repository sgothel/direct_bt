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
import java.util.List;

import org.direct_bt.BTException;
import org.direct_bt.BTGattChar;
import org.direct_bt.BTGattDesc;
import org.direct_bt.BTGattService;
import org.direct_bt.BTObject;
import org.direct_bt.BTType;
import org.direct_bt.BTUtils;
import org.direct_bt.GattCharPropertySet;
import org.direct_bt.BTGattChar.Listener;
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

    private final GattCharPropertySet properties;

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

    boolean enabledNotifyState = false;
    boolean enabledIndicateState = false;

   /* pp */ DBTGattChar(final long nativeInstance, final DBTGattService service,
                        final short handle, final GattCharPropertySet properties,
                        final String value_type_uuid, final short value_handle,
                        final int clientCharacteristicsConfigIndex)
    {
        super(nativeInstance, handle /* hash */);
        this.wbr_service = new WeakReference<DBTGattService>(service);
        this.handle = handle;

        this.properties = properties;
        this.value_type_uuid = value_type_uuid;
        this.value_handle = value_handle;
        this.clientCharacteristicsConfigIndex = clientCharacteristicsConfigIndex;
        this.descriptorList = getDescriptorsImpl();

        if( DEBUG ) {
            final boolean hasNotify = properties.isSet(GattCharPropertySet.Type.Notify);
            final boolean hasIndicate = properties.isSet(GattCharPropertySet.Type.Indicate);

            if( hasNotify || hasIndicate ) {
                final Listener characteristicListener = new Listener() {
                    @Override
                    public void notificationReceived(final BTGattChar charDecl, final byte[] value, final long timestamp) {
                        System.err.println("GATTCharacteristicListener.notificationReceived: "+charDecl+
                                           ", value[len "+value.length+": "+BTUtils.bytesHexString(value, 0, -1, true)+"]");
                    }
                    @Override
                    public void indicationReceived(final BTGattChar charDecl, final byte[] value, final long timestamp,
                                                   final boolean confirmationSent) {
                        System.err.println("GATTCharacteristicListener.indicationReceived: "+charDecl+
                                           ", value[len "+value.length+": "+BTUtils.bytesHexString(value, 0, -1, true)+
                                           "], confirmationSent "+confirmationSent);
                    }
                };
                this.addCharListener(characteristicListener); // silent, don't enable native GATT ourselves
            }
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
    public BTGattDesc findGattDesc(final String desc_uuid) {
        if( !checkServiceCache() ) {
            return null;
        }
        return (DBTGattDesc) findInCache(desc_uuid, BTType.GATT_DESCRIPTOR);
    }

    @Override
    public final BTGattService getService() { return wbr_service.get(); }

    @Override
    public final boolean getNotifying(final boolean enabledState[/*2*/]) {
        enabledState[0] = enabledNotifyState;
        enabledState[1] = enabledIndicateState;
        return enabledNotifyState || enabledIndicateState;
    }

    @Override
    public final GattCharPropertySet getProperties() { return properties; }

    @Override
    public final byte[] readValue() throws BTException {
        return readValueImpl();
    }

    @Override
    public final boolean writeValue(final byte[] value, final boolean withResponse) throws BTException {
        return writeValueImpl(value, withResponse);
    }

    @Override
    public final List<BTGattDesc> getDescriptors() { return descriptorList; }

    @Override
    public final synchronized boolean configNotificationIndication(final boolean enableNotification, final boolean enableIndication, final boolean enabledState[/*2*/])
            throws IllegalStateException
    {
        final boolean hasNotify = properties.isSet(GattCharPropertySet.Type.Notify);
        final boolean hasIndicate = properties.isSet(GattCharPropertySet.Type.Indicate);

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
            if( !res ) {
                enabledState[0] = false;
                enabledState[1] = false;
            }
            if( DEBUG ) {
                System.err.printf("GATTCharacteristic.configNotificationIndication: res %b, notification[shall %b, has %b: %b -> %b], indication[shall %b, has %b: %b -> %b]\n",
                        res,
                        enableNotification, hasNotify, enabledNotifyState, enabledState[0],
                        enableIndication, hasIndicate, enabledIndicateState, enabledState[1]);
            }
            enabledNotifyState = enabledState[0];
            enabledIndicateState = enabledState[1];
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
        final boolean enableNotification = properties.isSet(GattCharPropertySet.Type.Notify);
        final boolean enableIndication = !enableNotification && properties.isSet(GattCharPropertySet.Type.Indicate);

        return configNotificationIndication(enableNotification, enableIndication, enabledState);
    }

    @Override
    public boolean disableIndicationNotification() throws IllegalStateException {
        return configNotificationIndication(false /* enableNotification */, false /* enableIndication */, new boolean[2]);
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
    public final BTGattCharListener addCharListener(final Listener listener) {
        final BTGattCharListener wl = new DelegatedBTGattCharListener(this, listener);
        return getService().getDevice().addCharListener( wl ) ? wl : null;
    }

    @Override
    public final BTGattCharListener addCharListener(final Listener listener, final boolean enabledState[/*2*/]) {
        if( !enableNotificationOrIndication(enabledState) ) {
            return null;
        }
        return addCharListener( listener );
    }

    @Override
    public final boolean removeCharListener(final BTGattCharListener listener) {
        return getService().getDevice().removeCharListener( listener );
    }

    @Override
    public final int removeAllAssociatedCharListener(final boolean shallDisableIndicationNotification) {
        if( shallDisableIndicationNotification ) {
            disableIndicationNotification();
        }
        return getService().getDevice().removeAllAssociatedCharListener(this);
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
