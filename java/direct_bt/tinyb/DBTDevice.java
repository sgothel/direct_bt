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

package direct_bt.tinyb;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.atomic.AtomicBoolean;

import org.tinyb.AdapterStatusListener;
import org.tinyb.BLERandomAddressType;
import org.tinyb.BTSecurityLevel;
import org.tinyb.BluetoothAddressType;
import org.tinyb.BluetoothDevice;
import org.tinyb.BluetoothException;
import org.tinyb.BluetoothGattCharacteristic;
import org.tinyb.BluetoothGattDescriptor;
import org.tinyb.BluetoothGattService;
import org.tinyb.BluetoothNotification;
import org.tinyb.BluetoothObject;
import org.tinyb.BluetoothType;
import org.tinyb.EIRDataTypeSet;
import org.tinyb.GATTCharacteristicListener;
import org.tinyb.HCIStatusCode;
import org.tinyb.PairingMode;
import org.tinyb.SMPIOCapability;
import org.tinyb.SMPPairingState;

public class DBTDevice extends DBTObject implements BluetoothDevice
{
    private static final boolean DEBUG = DBTManager.DEBUG;

    /** Device's adapter weak back-reference */
    private final WeakReference<DBTAdapter> wbr_adapter;

    private final String address;
    private final BluetoothAddressType addressType;
    private final BLERandomAddressType leRandomAddressType;
    private final long ts_creation;
    private volatile String name;
    volatile long ts_last_discovery;
    volatile long ts_last_update;
    volatile short hciConnHandle;
    /* pp */ final List<WeakReference<DBTGattService>> serviceCache = new ArrayList<WeakReference<DBTGattService>>();

    private final AtomicBoolean isClosing = new AtomicBoolean(false);

    private final Object userCallbackLock = new Object();

    private final long blockedNotificationRef = 0;
    private BluetoothNotification<Boolean> userBlockedNotificationsCB = null;
    private final AtomicBoolean isBlocked = new AtomicBoolean(false);

    private final long pairedNotificationRef = 0;
    private BluetoothNotification<Boolean> userPairedNotificationsCB = null;
    private final AtomicBoolean isPaired = new AtomicBoolean(false);

    private final long trustedNotificationRef = 0;
    private BluetoothNotification<Boolean> userTrustedNotificationsCB = null;
    private final AtomicBoolean isTrusted = new AtomicBoolean(false);

    private BluetoothNotification<Boolean> userConnectedNotificationsCB = null;
    private final AtomicBoolean isConnected = new AtomicBoolean(false);

    private BluetoothNotification<Short> userRSSINotificationsCB = null;
    private BluetoothNotification<Map<Short, byte[]> > userManufDataNotificationsCB = null;
    private BluetoothNotification<Boolean> userServicesResolvedNotificationsCB = null;
    private final AtomicBoolean servicesResolved = new AtomicBoolean(false);
    private short appearance = 0;

    final AdapterStatusListener statusListener = new AdapterStatusListener() {
        @Override
        public void deviceUpdated(final BluetoothDevice device, final EIRDataTypeSet updateMask, final long timestamp) {
            final boolean nameUpdated = updateMask.isSet( EIRDataTypeSet.DataType.NAME );
            final boolean rssiUpdated = updateMask.isSet( EIRDataTypeSet.DataType.RSSI );
            final boolean mdUpdated = updateMask.isSet( EIRDataTypeSet.DataType.MANUF_DATA );
            if( nameUpdated ) {
                final String oldName = DBTDevice.this.name;
                DBTDevice.this.name = getNameImpl();
                if( DEBUG ) {
                    System.err.println("Device.StatusListener.UPDATED: NAME: '"+oldName+"' -> '"+DBTDevice.this.name+"'");
                }
            }
            if( rssiUpdated || mdUpdated ) {
                synchronized(userCallbackLock) {
                    if( rssiUpdated && null != userRSSINotificationsCB ) {
                        userRSSINotificationsCB.run(getRSSI());
                    }
                    if( mdUpdated && null != userManufDataNotificationsCB ) {
                        userManufDataNotificationsCB.run(getManufacturerData());
                    }
                }
            }
        }
        @Override
        public void deviceConnected(final BluetoothDevice device, final short handle, final long timestamp) {
            if( isConnected.compareAndSet(false, true) ) {
                synchronized(userCallbackLock) {
                    if( null != userConnectedNotificationsCB ) {
                        userConnectedNotificationsCB.run(Boolean.TRUE);
                    }
                    if( servicesResolved.compareAndSet(false, true) ) {
                        if( null != userServicesResolvedNotificationsCB ) {
                            userServicesResolvedNotificationsCB.run(Boolean.TRUE);
                        }
                    }
                }
            }
        }
        @Override
        public void deviceDisconnected(final BluetoothDevice device, final HCIStatusCode reason, final short handle, final long timestamp) {
            devicePaired(false);

            if( isConnected.compareAndSet(true, false) ) {
                clearServiceCache();
                synchronized(userCallbackLock) {
                    if( servicesResolved.compareAndSet(true, false) ) {
                        if( null != userServicesResolvedNotificationsCB ) {
                            userServicesResolvedNotificationsCB.run(Boolean.FALSE);
                        }
                    }
                    if( null != userConnectedNotificationsCB ) {
                        userConnectedNotificationsCB.run(Boolean.FALSE);
                    }
                }
            }
        }
    };

    final private BluetoothNotification<Boolean> blockedNotificationsCB = new BluetoothNotification<Boolean>() {
        @Override
        public void run(final Boolean value) {
            if( DEBUG ) {
                System.err.println("Device.BlockedNotification: "+isBlocked+" -> "+value+" on "+DBTDevice.this.toString());
            }
            final boolean _isBlocked = value.booleanValue();
            if( isBlocked.compareAndSet(!_isBlocked, _isBlocked) ) {
                synchronized(userCallbackLock) {
                    if( null != userBlockedNotificationsCB ) {
                        userBlockedNotificationsCB.run(value);
                    }
                }
            }
        }
    };

    final private void devicePaired(final boolean _isPaired) {
        if( DEBUG ) {
            System.err.println("Device.PairedNotification: "+isPaired+" -> "+_isPaired+" on "+DBTDevice.this.toString());
        }
        if( isPaired.compareAndSet(!_isPaired, _isPaired) ) {
            synchronized(userCallbackLock) {
                if( null != userPairedNotificationsCB ) {
                    userPairedNotificationsCB.run(_isPaired);
                }
            }
        }
    }
    final private BluetoothNotification<Boolean> pairedNotificationsCB = new BluetoothNotification<Boolean>() {
        @Override
        public void run(final Boolean value) {
            devicePaired( value.booleanValue() );
        }
    };

    final private BluetoothNotification<Boolean> trustedNotificationsCB = new BluetoothNotification<Boolean>() {
        @Override
        public void run(final Boolean value) {
            if( DEBUG ) {
                System.err.println("Device.TrustedNotification: "+isTrusted+" -> "+value+" on "+DBTDevice.this.toString());
            }
            final boolean _isTrusted = value.booleanValue();
            if( isTrusted.compareAndSet(!_isTrusted, _isTrusted) ) {
                synchronized(userCallbackLock) {
                    if( null != userTrustedNotificationsCB ) {
                        userTrustedNotificationsCB.run(value);
                    }
                }
            }
        }
    };

    /* pp */ DBTDevice(final long nativeInstance, final DBTAdapter adptr,
                       final String address,
                       final int intAddressType, final int intBLERandomAddressType,
                       final String name, final long ts_creation)
    {
        super(nativeInstance, address.hashCode());
        this.wbr_adapter = new WeakReference<DBTAdapter>(adptr);
        this.address = address;
        this.addressType = BluetoothAddressType.get(intAddressType);
        if( BluetoothAddressType.BDADDR_UNDEFINED == addressType ) {
            throw new IllegalArgumentException("Unsupported given native addresstype "+intAddressType);
        }
        this.leRandomAddressType = BLERandomAddressType.get(intBLERandomAddressType);
        if( BluetoothAddressType.BDADDR_LE_RANDOM == addressType ) {
            if( BLERandomAddressType.UNDEFINED == leRandomAddressType ) {
                throw new IllegalArgumentException("BDADDR_LE_RANDOM: Invalid given native BLERandomAddressType "+intBLERandomAddressType+": "+toString());
            }
        } else {
            if( BLERandomAddressType.UNDEFINED != leRandomAddressType ) {
                throw new IllegalArgumentException("Not BDADDR_LE_RANDOM: Invalid given native BLERandomAddressType "+leRandomAddressType+": "+toString());
            }
        }
        this.ts_creation = ts_creation;
        this.name = name;
        ts_last_discovery = ts_creation;
        ts_last_update = ts_creation;
        hciConnHandle = 0;
        appearance = 0;
        initImpl();
        adptr.addStatusListener(statusListener, this); // only for this device
        enableBlockedNotificationsImpl(blockedNotificationsCB);
        enablePairedNotificationsImpl(pairedNotificationsCB);
        // FIXME enableTrustedNotificationsImpl(trustedNotificationsCB);
    }

    @Override
    public void close() {
        if( !isValid() ) {
            return;
        }
        if( !isClosing.compareAndSet(false, true) ) {
            return;
        }
        disableConnectedNotifications();
        disableRSSINotifications();
        disableManufacturerDataNotifications();
        disableServicesResolvedNotifications();

        disableBlockedNotifications();
        disableBlockedNotificationsImpl();
        disablePairedNotifications();
        disablePairedNotificationsImpl();
        disableServiceDataNotifications();
        disableTrustedNotifications();
        // FIXME disableTrustedNotificationsImpl();

        clearServiceCache();

        final DBTAdapter a = getAdapter();
        if( null != a ) {
            a.removeStatusListener(statusListener);
            a.removeDiscoveredDevice(this);
        }
        super.close();
    }

    @Override
    public boolean equals(final Object obj)
    {
        if (obj == null || !(obj instanceof DBTDevice)) {
            return false;
        }
        final DBTDevice other = (DBTDevice)obj;
        return address.equals(other.address) && addressType.equals(other.addressType);
    }

    @Override
    public final long getCreationTimestamp() { return ts_creation; }

    @Override
    public final long getLastDiscoveryTimestamp() { return ts_last_discovery; }

    @Override
    public final long getLastUpdateTimestamp() { return ts_last_update; }

    @Override
    public DBTAdapter getAdapter() { return wbr_adapter.get(); }

    @Override
    public String getAddress() { return address; }

    @Override
    public BluetoothAddressType getAddressType() { return addressType; }

    @Override
    public BLERandomAddressType getBLERandomAddressType() { return leRandomAddressType; }

    @Override
    public String getName() { return name; }

    private native String getNameImpl();

    @Override
    public BluetoothType getBluetoothType() { return class_type(); }

    static BluetoothType class_type() { return BluetoothType.DEVICE; }

    @Override
    public BluetoothGattService find(final String UUID, final long timeoutMS) {
        return (DBTGattService) findInCache(UUID, BluetoothType.GATT_SERVICE);
    }

    @Override
    public BluetoothGattService find(final String UUID) {
        return find(UUID, 0);
    }

    /* Unsupported */

    @Override
    public int getBluetoothClass() { throw new UnsupportedOperationException(); } // FIXME

    @Override
    public final BluetoothDevice clone() { throw new UnsupportedOperationException(); } // FIXME

    @Override
    public final boolean pair() throws BluetoothException {
        return false;
    }

    @Override
    public final boolean  setConnSecurityLevel(final BTSecurityLevel sec_level) {
        return setConnSecurityLevelImpl(sec_level.value);
    }
    private final native boolean setConnSecurityLevelImpl(final byte sec_level);

    @Override
    public final BTSecurityLevel getConnSecurityLevel() {
        return BTSecurityLevel.get( getConnSecurityLevelImpl() );
    }
    private final native byte getConnSecurityLevelImpl();

    @Override
    public final boolean setConnIOCapability(final SMPIOCapability io_cap, final boolean blocking) {
        return setConnIOCapabilityImpl(io_cap.value, blocking);
    }
    private final native boolean setConnIOCapabilityImpl(final byte io_cap, final boolean blocking);

    @Override
    public final boolean setConnSecurity(final BTSecurityLevel sec_level, final SMPIOCapability io_cap, final boolean blocking) {
        return setConnSecurityImpl(sec_level.value, io_cap.value, blocking);
    }
    private final native boolean setConnSecurityImpl(final byte sec_level, final byte io_cap, final boolean blocking);

    @Override
    public final SMPIOCapability getConnIOCapability() {
        return SMPIOCapability.get( getConnIOCapabilityImpl() );
    }
    private final native byte getConnIOCapabilityImpl();

    @Override
    public HCIStatusCode setPairingPasskey(final int passkey) {
        return HCIStatusCode.get( setPairingPasskeyImpl(passkey) );
    }
    private native byte setPairingPasskeyImpl(final int passkey) throws BluetoothException;

    @Override
    public HCIStatusCode setPairingNumericComparison(final boolean equal) {
        return HCIStatusCode.get( setPairingNumericComparisonImpl(equal) );
    }
    private native byte setPairingNumericComparisonImpl(final boolean equal);

    @Override
    public PairingMode getPairingMode() {
        return PairingMode.get( getPairingModeImpl() );
    }
    private native byte getPairingModeImpl();

    @Override
    public SMPPairingState getPairingState() {
        return SMPPairingState.get( getPairingStateImpl() );
    }
    private native byte getPairingStateImpl();

    @Override
    public final boolean cancelPairing() throws BluetoothException {
        // FIXME: Not supporter (yet)
        return false;
    }

    @Override
    public String getAlias() { return null; } // FIXME

    @Override
    public void setAlias(final String value) { throw new UnsupportedOperationException(); } // FIXME

    /* internal */

    private native void initImpl();

    /* DBT method calls: Connection */

    @Override
    public final void enableConnectedNotifications(final BluetoothNotification<Boolean> callback) {
        synchronized(userCallbackLock) {
            userConnectedNotificationsCB = callback;
        }
    }
    @Override
    public final void disableConnectedNotifications() {
        synchronized(userCallbackLock) {
            userConnectedNotificationsCB = null;
        }
    }

    @Override
    public final boolean getConnected() { return isConnected.get(); }

    @Override
    public final short getConnectionHandle() { return hciConnHandle; }

    @Override
    public final HCIStatusCode disconnect() throws BluetoothException {
        if( isConnected.get() ) {
            return HCIStatusCode.get( disconnectImpl() ); // event callbacks will be generated by implementation
        }
        return HCIStatusCode.CONNECTION_TERMINATED_BY_LOCAL_HOST;
    }
    private native byte disconnectImpl() throws BluetoothException;

    @Override
    public final HCIStatusCode connect() throws BluetoothException {
        if( !isConnected.get() ) {
            return HCIStatusCode.get( connectDefaultImpl() ); // event callbacks will be generated by implementation
        }
        return HCIStatusCode.CONNECTION_ALREADY_EXISTS;
    }
    private native byte connectDefaultImpl() throws BluetoothException;

    @Override
    public HCIStatusCode connectLE(final short le_scan_interval, final short le_scan_window,
                                   final short conn_interval_min, final short conn_interval_max,
                                   final short conn_latency, final short timeout) {
        if( !isConnected.get() ) {
            // event callbacks will be generated by implementation
            if( 0 > le_scan_interval  || 0 > le_scan_window || 0 > conn_interval_min ||
                0 > conn_interval_max || 0 > conn_latency   || 0 > timeout ) {
                return HCIStatusCode.get( connectLEImpl0() );
            } else {
                return HCIStatusCode.get( connectLEImpl1(le_scan_interval, le_scan_window, conn_interval_min, conn_interval_max, conn_latency, timeout) );
            }
        }
        return HCIStatusCode.CONNECTION_ALREADY_EXISTS;
    }
    private native byte connectLEImpl0();
    private native byte connectLEImpl1(final short le_scan_interval, final short le_scan_window,
                                       final short conn_interval_min, final short conn_interval_max,
                                       final short conn_latency, final short timeout);

    /* DBT Java callbacks */

    @Override
    public final void enableRSSINotifications(final BluetoothNotification<Short> callback) {
        synchronized(userCallbackLock) {
            userRSSINotificationsCB = callback;
        }
    }

    @Override
    public final void disableRSSINotifications() {
        synchronized(userCallbackLock) {
            userRSSINotificationsCB = null;
        }
    }


    @Override
    public final void enableManufacturerDataNotifications(final BluetoothNotification<Map<Short, byte[]> > callback) {
        synchronized(userCallbackLock) {
            userManufDataNotificationsCB = callback;
        }
    }

    @Override
    public final void disableManufacturerDataNotifications() {
        synchronized(userCallbackLock) {
            userManufDataNotificationsCB = null;
        }
    }


    @Override
    public final void enableServicesResolvedNotifications(final BluetoothNotification<Boolean> callback) {
        synchronized(userCallbackLock) {
            userServicesResolvedNotificationsCB = callback;
        }
    }

    @Override
    public void disableServicesResolvedNotifications() {
        synchronized(userCallbackLock) {
            userServicesResolvedNotificationsCB = null;
        }
    }

    @Override
    public boolean getServicesResolved () { return servicesResolved.get(); }

    @Override
    public short getAppearance() { return appearance; }

    @Override
    public void enableBlockedNotifications(final BluetoothNotification<Boolean> callback) {
        synchronized(userCallbackLock) {
            userBlockedNotificationsCB = callback;
        }
    }
    @Override
    public void disableBlockedNotifications() {
        synchronized(userCallbackLock) {
            userBlockedNotificationsCB = null;
        }
    }
    @Override
    public boolean getBlocked() { return isBlocked.get(); }

    @Override
    public void setBlocked(final boolean value) {
        setBlockedImpl(value);
    }

    @Override
    public void enableServiceDataNotifications(final BluetoothNotification<Map<String, byte[]> > callback) {
        // FIXME: Isn't this GATTCharacteristic data notification/indication? Then map it or drop!
    }

    @Override
    public Map<String, byte[]> getServiceData() {
        return null; // FIXME
    }

    @Override
    public void disableServiceDataNotifications() {
        // FIXME
    }

    @Override
    public void enablePairedNotifications(final BluetoothNotification<Boolean> callback) {
        synchronized(userCallbackLock) {
            userPairedNotificationsCB = callback;
        }
    }

    @Override
    public void disablePairedNotifications() {
        synchronized(userCallbackLock) {
            userPairedNotificationsCB = null;
        }
    }

    @Override
    public final boolean getPaired() { return isPaired.get(); }

    @Override
    public void enableTrustedNotifications(final BluetoothNotification<Boolean> callback) {
        synchronized(userCallbackLock) {
            userTrustedNotificationsCB = callback;
        }
    }

    @Override
    public void disableTrustedNotifications() {
        synchronized(userCallbackLock) {
            userTrustedNotificationsCB = null;
        }
    }

    @Override
    public boolean getTrusted() { return isTrusted.get(); }

    @Override
    public void setTrusted(final boolean value) {
        setTrustedImpl(value);
    }

    @Override
    public native boolean getLegacyPairing();

    @Override
    public final String toString() {
        if( !isValid() ) {
            // UTF-8 271D = Cross
            final String leRandomStr;
            if( BLERandomAddressType.UNDEFINED != this.leRandomAddressType ) {
                leRandomStr = ", random "+leRandomAddressType.toString();
            } else {
                leRandomStr = "";
            }
            return "Device" + "\u271D" + "[address["+address+", "+addressType.toString()+leRandomStr+"], '"+name+
                    "', connected["+isConnected.get()+", 0x"+Integer.toHexString(hciConnHandle)+"]]";
        }
        return toStringImpl();
    }

    /* DBT native callbacks */

    private native String toStringImpl();

    private native void enableBlockedNotificationsImpl(BluetoothNotification<Boolean> callback);
    private native void disableBlockedNotificationsImpl();
    private native void setBlockedImpl(final boolean value);

    // Note that this is only called natively for unpaired, i.e. paired:=false. Using deviceConnected for paired:=true.
    private native void enablePairedNotificationsImpl(BluetoothNotification<Boolean> callback);
    private native void disablePairedNotificationsImpl();

    /**
     * FIXME: How to implement trusted ?
     *
    private native void enableTrustedNotificationsImpl(BluetoothNotification<Boolean> callback);
    private native void disableTrustedNotificationsImpl();
     */
    private native void setTrustedImpl(boolean value);

    /* DBT native method calls: */

    @Override
    public native boolean connectProfile(String arg_UUID) throws BluetoothException;

    @Override
    public native boolean disconnectProfile(String arg_UUID) throws BluetoothException;

    /**
     * {@inheritDoc}
     */
    @Override
    public final boolean remove() throws BluetoothException {
        // close: clear java-listener, super.close()
        //        -> DBTNativeDownlink.delete(): deleteNativeJavaObject(..), deleteImpl(..) -> DBTDevice::remove()
        close();
        // return removeImpl();
        return true;
    }
    private native boolean removeImpl() throws BluetoothException;

    @Override
    public List<BluetoothGattService> getServices() {
        try {
            final List<BluetoothGattService> services = getServicesImpl();
            updateServiceCache(services);
            return services;
        } catch (final Throwable t) {
            System.err.println("DBTDevice.getServices(): Caught "+t.getMessage()+" on thread "+Thread.currentThread().toString()+" on "+toString());
            if(DEBUG) {
                t.printStackTrace();
            }
        }
        return null;
    }
    private native List<BluetoothGattService> getServicesImpl();

    @Override
    public boolean pingGATT() {
        try {
            return pingGATTImpl();
        } catch (final Throwable t) {
            System.err.println("DBTDevice.pingGATT(): Caught "+t.getMessage()+" on thread "+Thread.currentThread().toString()+" on "+toString());
            if(DEBUG) {
                t.printStackTrace();
            }
        }
        return false;
    }
    private native boolean pingGATTImpl();

    /* property accessors: */

    @Override
    public native String getIcon();

    @Override
    public native short getRSSI();

    @Override
    public native String[] getUUIDs();

    @Override
    public native String getModalias();

    @Override
    public native Map<Short, byte[]> getManufacturerData();

    @Override
    public native short getTxPower ();

    /**
     * {@inheritDoc}
     * <p>
     * Native implementation calls DBTDevice::remove()
     * </p>
     */
    @Override
    protected native void deleteImpl(long nativeInstance);

    @Override
    public boolean addCharacteristicListener(final GATTCharacteristicListener listener) {
        return addCharacteristicListener(listener, (DBTGattCharacteristic)listener.getAssociatedCharacteristic());
    }
    private native boolean addCharacteristicListener(final GATTCharacteristicListener listener, final DBTGattCharacteristic associatedCharacteristic);

    @Override
    public native boolean removeCharacteristicListener(final GATTCharacteristicListener l);

    @Override
    public native int removeAllAssociatedCharacteristicListener(final BluetoothGattCharacteristic associatedCharacteristic);

    @Override
    public native int removeAllCharacteristicListener();

    /* local functionality */

    private void clearServiceCache() {
        synchronized(serviceCache) {
            for(int i = serviceCache.size() - 1; i >= 0; i-- ) {
                serviceCache.get(i).clear();
                serviceCache.remove(i);
            }
        }
    }
    private void updateServiceCache(final List<BluetoothGattService> services) {
        synchronized(serviceCache) {
            clearServiceCache();
            if( null != services ) {
                for(final BluetoothGattService service : services) {
                    serviceCache.add( new WeakReference<DBTGattService>( (DBTGattService)service ) );
                }
            }
        }
    }

    /* pp */ boolean checkServiceCache(final boolean getServices) {
        synchronized(serviceCache) {
            if( serviceCache.isEmpty() ) {
                if( getServices ) {
                    getServices();
                    if( serviceCache.isEmpty() ) {
                        return false;
                    }
                } else {
                    return false;
                }
            }
            return true;
        }
    }

    /**
     * Returns the matching {@link DBTObject} from the internal cache if found,
     * otherwise {@code null}.
     * <p>
     * The returned {@link DBTObject} may be of type
     * <ul>
     *   <li>{@link DBTGattService}</li>
     *   <li>{@link DBTGattCharacteristic}</li>
     *   <li>{@link DBTGattDescriptor}</li>
     * </ul>
     * or alternatively in {@link BluetoothObject} space
     * <ul>
     *   <li>{@link BluetoothType#GATT_SERVICE} -> {@link BluetoothGattService}</li>
     *   <li>{@link BluetoothType#GATT_CHARACTERISTIC} -> {@link BluetoothGattCharacteristic}</li>
     *   <li>{@link BluetoothType#GATT_DESCRIPTOR} -> {@link BluetoothGattDescriptor}</li>
     * </ul>
     * </p>
     * @param uuid UUID of the desired {@link BluetoothType#GATT_SERVICE service},
     * {@link BluetoothType#GATT_CHARACTERISTIC characteristic} or {@link BluetoothType#GATT_DESCRIPTOR descriptor} to be found.
     * Maybe {@code null}, in which case the first object of the desired type is being returned - if existing.
     * @param type specify the type of the object to be found, either
     * {@link BluetoothType#GATT_SERVICE service}, {@link BluetoothType#GATT_CHARACTERISTIC characteristic}
     * or {@link BluetoothType#GATT_DESCRIPTOR descriptor}.
     * {@link BluetoothType#NONE none} means anything.
     */
    /* pp */ DBTObject findInCache(final String uuid, final BluetoothType type) {
        final boolean anyType = BluetoothType.NONE == type;
        final boolean serviceType = BluetoothType.GATT_SERVICE == type;
        final boolean charType = BluetoothType.GATT_CHARACTERISTIC== type;
        final boolean descType = BluetoothType.GATT_DESCRIPTOR == type;

        if( !anyType && !serviceType && !charType && !descType ) {
            return null;
        }
        synchronized(serviceCache) {
            if( !checkServiceCache(true) ) {
                return null;
            }

            if( null == uuid && ( anyType || serviceType ) ) {
                // special case for 1st valid service ref
                while( !serviceCache.isEmpty() ) {
                    final DBTGattService service = serviceCache.get(0).get();
                    if( null == service ) {
                        serviceCache.remove(0);
                    } else {
                        return service;
                    }
                }
                return null; // empty valid service refs
            }
            for(int srvIdx = serviceCache.size() - 1; srvIdx >= 0; srvIdx-- ) {
                final DBTGattService service = serviceCache.get(srvIdx).get();
                if( null == service ) {
                    serviceCache.remove(srvIdx); // remove dead ref
                    continue; // cont w/ next service
                }
                if( ( anyType || serviceType ) && service.getUUID().equals(uuid) ) {
                    return service;
                }
                if( anyType || charType || descType ) {
                    final DBTObject dbtObj = service.findInCache(uuid, type);
                    if( null != dbtObj ) {
                        return dbtObj;
                    }
                }
            }
            return null;
        }
    }
}
