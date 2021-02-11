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
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.atomic.AtomicBoolean;

import org.direct_bt.AdapterStatusListener;
import org.direct_bt.BDAddressAndType;
import org.direct_bt.BDAddressType;
import org.direct_bt.BTSecurityLevel;
import org.direct_bt.BTDevice;
import org.direct_bt.BTException;
import org.direct_bt.BTGattChar;
import org.direct_bt.BTGattDesc;
import org.direct_bt.BTGattService;
import org.direct_bt.BTNotification;
import org.direct_bt.BTObject;
import org.direct_bt.BTType;
import org.direct_bt.EIRDataTypeSet;
import org.direct_bt.EUI48;
import org.direct_bt.BTGattCharListener;
import org.direct_bt.HCIStatusCode;
import org.direct_bt.PairingMode;
import org.direct_bt.SMPIOCapability;
import org.direct_bt.SMPKeyMask;
import org.direct_bt.SMPLongTermKeyInfo;
import org.direct_bt.SMPPairingState;
import org.direct_bt.SMPSignatureResolvingKeyInfo;

public class DBTDevice extends DBTObject implements BTDevice
{
    private static final boolean DEBUG = DBTManager.DEBUG;

    /** Device's adapter weak back-reference */
    private final WeakReference<DBTAdapter> wbr_adapter;

    private final BDAddressAndType addressAndType;
    private final long ts_creation;
    private volatile String name;
    volatile long ts_last_discovery;
    volatile long ts_last_update;
    volatile short hciConnHandle;
    /* pp */ final List<WeakReference<DBTGattService>> serviceCache = new ArrayList<WeakReference<DBTGattService>>();

    private final AtomicBoolean isClosing = new AtomicBoolean(false);

    private final Object userCallbackLock = new Object();

    private final long blockedNotificationRef = 0;
    private BTNotification<Boolean> userBlockedNotificationsCB = null;
    private final AtomicBoolean isBlocked = new AtomicBoolean(false);

    private final long pairedNotificationRef = 0;
    private BTNotification<Boolean> userPairedNotificationsCB = null;
    private final AtomicBoolean isPaired = new AtomicBoolean(false);

    private final long trustedNotificationRef = 0;
    private BTNotification<Boolean> userTrustedNotificationsCB = null;
    private final AtomicBoolean isTrusted = new AtomicBoolean(false);

    private BTNotification<Boolean> userConnectedNotificationsCB = null;
    private final AtomicBoolean isConnected = new AtomicBoolean(false);

    private BTNotification<Short> userRSSINotificationsCB = null;
    private BTNotification<Map<Short, byte[]> > userManufDataNotificationsCB = null;
    private BTNotification<Boolean> userServicesResolvedNotificationsCB = null;
    private final AtomicBoolean servicesResolved = new AtomicBoolean(false);
    private short appearance = 0;

    final AdapterStatusListener statusListener = new AdapterStatusListener() {
        @Override
        public void deviceUpdated(final BTDevice device, final EIRDataTypeSet updateMask, final long timestamp) {
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
        public void deviceConnected(final BTDevice device, final short handle, final long timestamp) {
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
        public void deviceDisconnected(final BTDevice device, final HCIStatusCode reason, final short handle, final long timestamp) {
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

    final private BTNotification<Boolean> blockedNotificationsCB = new BTNotification<Boolean>() {
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
    final private BTNotification<Boolean> pairedNotificationsCB = new BTNotification<Boolean>() {
        @Override
        public void run(final Boolean value) {
            devicePaired( value.booleanValue() );
        }
    };

    final private BTNotification<Boolean> trustedNotificationsCB = new BTNotification<Boolean>() {
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
                       final byte byteAddress[/*6*/],
                       final byte byteAddressType,
                       final String name, final long ts_creation)
    {
        super(nativeInstance, compHash(java.util.Arrays.hashCode(byteAddress), 31+byteAddressType));
        this.wbr_adapter = new WeakReference<DBTAdapter>(adptr);
        this.addressAndType = new BDAddressAndType(new EUI48(byteAddress), BDAddressType.get(byteAddressType));
        if( BDAddressType.BDADDR_UNDEFINED == addressAndType.type ) {
            throw new IllegalArgumentException("Unsupported given native addresstype "+byteAddressType);
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
        if(this == obj) {
            return true;
        }
        if (obj == null || !(obj instanceof DBTDevice)) {
            return false;
        }
        final DBTDevice other = (DBTDevice)obj;
        return addressAndType.equals(other.addressAndType);
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
    public String getAddressString() { return addressAndType.address.toString(); }

    @Override
    public BDAddressAndType getAddressAndType() { return addressAndType; }

    @Override
    public String getName() { return name; }

    private native String getNameImpl();

    @Override
    public BTType getBluetoothType() { return class_type(); }

    static BTType class_type() { return BTType.DEVICE; }

    @Override
    public BTGattService find(final String UUID, final long timeoutMS) {
        return (DBTGattService) findInCache(UUID, BTType.GATT_SERVICE);
    }

    @Override
    public BTGattService find(final String UUID) {
        return find(UUID, 0);
    }

    /* Unsupported */

    @Override
    public int getBluetoothClass() { throw new UnsupportedOperationException(); } // FIXME

    @Override
    public final BTDevice clone() { throw new UnsupportedOperationException(); } // FIXME

    @Override
    public final SMPKeyMask getAvailableSMPKeys(final boolean responder) {
        return new SMPKeyMask(getAvailableSMPKeysImpl(responder));
    }
    private final native byte getAvailableSMPKeysImpl(final boolean responder);

    @Override
    public final SMPLongTermKeyInfo getLongTermKeyInfo(final boolean responder) {
        final byte[] stream = new byte[SMPLongTermKeyInfo.byte_size];
        getLongTermKeyInfoImpl(responder, stream);
        return new SMPLongTermKeyInfo(stream, 0);
    }
    private final native void getLongTermKeyInfoImpl(final boolean responder, final byte[] sink);

    @Override
    public final HCIStatusCode setLongTermKeyInfo(final SMPLongTermKeyInfo ltk) {
        final byte[] stream = new byte[SMPLongTermKeyInfo.byte_size];
        ltk.getStream(stream, 0);
        return HCIStatusCode.get( setLongTermKeyInfoImpl(stream) );
    }
    private final native byte setLongTermKeyInfoImpl(final byte[] source);

    @Override
    public final SMPSignatureResolvingKeyInfo getSignatureResolvingKeyInfo(final boolean responder) {
        final byte[] stream = new byte[SMPSignatureResolvingKeyInfo.byte_size];
        getSignatureResolvingKeyInfoImpl(responder, stream);
        return new SMPSignatureResolvingKeyInfo(stream, 0);
    }
    private final native void getSignatureResolvingKeyInfoImpl(final boolean responder, final byte[] sink);

    @Override
    public final boolean pair() throws BTException {
        return false;
    }

    @Override
    public final HCIStatusCode unpair() {
        return HCIStatusCode.get( unpairImpl() );
    }
    private final native byte unpairImpl();

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
    public final boolean setConnIOCapability(final SMPIOCapability io_cap) {
        return setConnIOCapabilityImpl(io_cap.value);
    }
    private final native boolean setConnIOCapabilityImpl(final byte io_cap);

    @Override
    public final SMPIOCapability getConnIOCapability() {
        return SMPIOCapability.get( getConnIOCapabilityImpl() );
    }
    private final native byte getConnIOCapabilityImpl();

    @Override
    public final boolean setConnSecurity(final BTSecurityLevel sec_level, final SMPIOCapability io_cap) {
        return setConnSecurityImpl(sec_level.value, io_cap.value);
    }
    private final native boolean setConnSecurityImpl(final byte sec_level, final byte io_cap);

    @Override
    public final boolean setConnSecurityBest(final BTSecurityLevel sec_level, final SMPIOCapability io_cap) {
        if( BTSecurityLevel.UNSET.value < sec_level.value && SMPIOCapability.UNSET.value != io_cap.value ) {
            return setConnSecurity(sec_level, io_cap);
        } else if( BTSecurityLevel.UNSET.value < sec_level.value ) {
            if( BTSecurityLevel.ENC_ONLY.value >= sec_level.value ) {
                return setConnSecurity(sec_level, SMPIOCapability.NO_INPUT_NO_OUTPUT);
            } else {
                return setConnSecurityLevel(sec_level);
            }
        } else if( SMPIOCapability.UNSET.value != io_cap.value ) {
            return setConnIOCapability(io_cap);
        } else {
            return false;
        }
    }

    @Override
    public final boolean setConnSecurityAuto(final SMPIOCapability iocap_auto) {
        return setConnSecurityAutoImpl( iocap_auto.value );
    }
    private final native boolean setConnSecurityAutoImpl(final byte io_cap);

    @Override
    public final native boolean isConnSecurityAutoEnabled();

    @Override
    public HCIStatusCode setPairingPasskey(final int passkey) {
        return HCIStatusCode.get( setPairingPasskeyImpl(passkey) );
    }
    private native byte setPairingPasskeyImpl(final int passkey) throws BTException;

    @Override
    public HCIStatusCode setPairingPasskeyNegative() {
        return HCIStatusCode.get( setPairingPasskeyNegativeImpl() );
    }
    private native byte setPairingPasskeyNegativeImpl() throws BTException;

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
    public final boolean cancelPairing() throws BTException {
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
    public final void enableConnectedNotifications(final BTNotification<Boolean> callback) {
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
    public final HCIStatusCode disconnect() {
        if( isConnected.get() ) {
            return HCIStatusCode.get( disconnectImpl() ); // event callbacks will be generated by implementation
        }
        return HCIStatusCode.CONNECTION_TERMINATED_BY_LOCAL_HOST;
    }
    private native byte disconnectImpl();

    @Override
    public final HCIStatusCode connect() {
        if( !isConnected.get() ) {
            return HCIStatusCode.get( connectDefaultImpl() ); // event callbacks will be generated by implementation
        }
        return HCIStatusCode.CONNECTION_ALREADY_EXISTS;
    }
    private native byte connectDefaultImpl();

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
    public final void enableRSSINotifications(final BTNotification<Short> callback) {
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
    public final void enableManufacturerDataNotifications(final BTNotification<Map<Short, byte[]> > callback) {
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
    public final void enableServicesResolvedNotifications(final BTNotification<Boolean> callback) {
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
    public void enableBlockedNotifications(final BTNotification<Boolean> callback) {
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
    public void enableServiceDataNotifications(final BTNotification<Map<String, byte[]> > callback) {
        // FIXME: Isn't this BTGattChar data notification/indication? Then map it or drop!
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
    public void enablePairedNotifications(final BTNotification<Boolean> callback) {
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
    public void enableTrustedNotifications(final BTNotification<Boolean> callback) {
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
            return "Device" + "\u271D" + "[address"+addressAndType+", '"+name+
                    "', connected["+isConnected.get()+", 0x"+Integer.toHexString(hciConnHandle)+"]]";
        }
        return toStringImpl();
    }

    /* DBT native callbacks */

    private native String toStringImpl();

    private native void enableBlockedNotificationsImpl(BTNotification<Boolean> callback);
    private native void disableBlockedNotificationsImpl();
    private native void setBlockedImpl(final boolean value);

    // Note that this is only called natively for unpaired, i.e. paired:=false. Using deviceConnected for paired:=true.
    private native void enablePairedNotificationsImpl(BTNotification<Boolean> callback);
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
    public native boolean connectProfile(String arg_UUID) throws BTException;

    @Override
    public native boolean disconnectProfile(String arg_UUID) throws BTException;

    /**
     * {@inheritDoc}
     */
    @Override
    public final boolean remove() throws BTException {
        // close: clear java-listener, super.close()
        //        -> DBTNativeDownlink.delete(): deleteNativeJavaObject(..), deleteImpl(..) -> DBTDevice::remove()
        close();
        // return removeImpl();
        return true;
    }
    private native boolean removeImpl() throws BTException;

    @Override
    public final boolean isValid() { return super.isValid() /* && isValidImpl() */; }
    // public native boolean isValidImpl();

    @Override
    public List<BTGattService> getServices() {
        try {
            final List<BTGattService> services = getServicesImpl();
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
    private native List<BTGattService> getServicesImpl();

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
    public boolean addCharListener(final BTGattCharListener listener) {
        return addCharListener(listener, (DBTGattChar)listener.getAssociatedChar());
    }
    private native boolean addCharListener(final BTGattCharListener listener, final DBTGattChar associatedCharacteristic);

    @Override
    public native boolean removeCharListener(final BTGattCharListener l);

    @Override
    public native int removeAllAssociatedCharListener(final BTGattChar associatedCharacteristic);

    @Override
    public native int removeAllCharListener();

    /* local functionality */

    private void clearServiceCache() {
        synchronized(serviceCache) {
            for(int i = serviceCache.size() - 1; i >= 0; i-- ) {
                serviceCache.get(i).clear();
                serviceCache.remove(i);
            }
        }
    }
    private void updateServiceCache(final List<BTGattService> services) {
        synchronized(serviceCache) {
            clearServiceCache();
            if( null != services ) {
                for(final BTGattService service : services) {
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
     *   <li>{@link DBTGattChar}</li>
     *   <li>{@link DBTGattDesc}</li>
     * </ul>
     * or alternatively in {@link BTObject} space
     * <ul>
     *   <li>{@link BTType#GATT_SERVICE} -> {@link BTGattService}</li>
     *   <li>{@link BTType#GATT_CHARACTERISTIC} -> {@link BTGattChar}</li>
     *   <li>{@link BTType#GATT_DESCRIPTOR} -> {@link BTGattDesc}</li>
     * </ul>
     * </p>
     * @param uuid UUID of the desired {@link BTType#GATT_SERVICE service},
     * {@link BTType#GATT_CHARACTERISTIC characteristic} or {@link BTType#GATT_DESCRIPTOR descriptor} to be found.
     * Maybe {@code null}, in which case the first object of the desired type is being returned - if existing.
     * @param type specify the type of the object to be found, either
     * {@link BTType#GATT_SERVICE service}, {@link BTType#GATT_CHARACTERISTIC characteristic}
     * or {@link BTType#GATT_DESCRIPTOR descriptor}.
     * {@link BTType#NONE none} means anything.
     */
    /* pp */ DBTObject findInCache(final String uuid, final BTType type) {
        final boolean anyType = BTType.NONE == type;
        final boolean serviceType = BTType.GATT_SERVICE == type;
        final boolean charType = BTType.GATT_CHARACTERISTIC== type;
        final boolean descType = BTType.GATT_DESCRIPTOR == type;

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
