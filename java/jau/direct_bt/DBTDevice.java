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
import org.direct_bt.BTObject;
import org.direct_bt.BTRole;
import org.direct_bt.BTType;
import org.direct_bt.BTUtils;
import org.direct_bt.EIRDataTypeSet;
import org.direct_bt.BTGattCharListener;
import org.direct_bt.HCIStatusCode;
import org.direct_bt.LE_PHYs;
import org.direct_bt.PairingMode;
import org.direct_bt.SMPIOCapability;
import org.direct_bt.SMPKeyMask;
import org.direct_bt.SMPLinkKeyInfo;
import org.direct_bt.SMPLongTermKeyInfo;
import org.direct_bt.SMPPairingState;
import org.direct_bt.SMPSignatureResolvingKeyInfo;
import org.jau.net.EUI48;

public class DBTDevice extends DBTObject implements BTDevice
{
    private static final boolean DEBUG = DBTManager.DEBUG;

    /** Device's adapter weak back-reference */
    private final WeakReference<DBTAdapter> wbr_adapter;

    private final BDAddressAndType addressAndType;
    private final long ts_creation;
    volatile long ts_last_discovery;
    volatile long ts_last_update;
    volatile short hciConnHandle;
    private volatile String name_cached;
    /* pp */ final List<WeakReference<DBTGattService>> serviceCache = new ArrayList<WeakReference<DBTGattService>>();

    private final AtomicBoolean isClosing = new AtomicBoolean(false);

    private final AtomicBoolean isConnected = new AtomicBoolean(false);

    final AdapterStatusListener statusListener = new AdapterStatusListener() {
        @Override
        public void deviceUpdated(final BTDevice device, final EIRDataTypeSet updateMask, final long timestamp) {
        }
        @Override
        public void deviceConnected(final BTDevice device, final short handle, final long timestamp) {
            if( isConnected.compareAndSet(false, true) ) {
                // nop
            }
        }
        @Override
        public void deviceDisconnected(final BTDevice device, final HCIStatusCode reason, final short handle, final long timestamp) {
            if( isConnected.compareAndSet(true, false) ) {
                clearServiceCache();
            }
        }

        @Override
        public String toString() {
            return "AdapterStatusListener[device "+addressAndType.toString()+"]";
        }
    };

    /* pp */ DBTDevice(final long nativeInstance, final DBTAdapter adptr,
                       final byte byteAddress[/*6*/],
                       final byte byteAddressType,
                       final long ts_creation, final String name)
    {
        super(nativeInstance, compHash(java.util.Arrays.hashCode(byteAddress), 31+byteAddressType));
        this.wbr_adapter = new WeakReference<DBTAdapter>(adptr);
        this.addressAndType = new BDAddressAndType(new EUI48(byteAddress), BDAddressType.get(byteAddressType));
        if( BDAddressType.BDADDR_UNDEFINED == addressAndType.type ) {
            throw new IllegalArgumentException("Unsupported given native addresstype "+byteAddressType);
        }
        this.ts_creation = ts_creation;
        ts_last_discovery = ts_creation;
        ts_last_update = ts_creation;
        hciConnHandle = 0;
        name_cached = name;
        initImpl();
        addStatusListener(statusListener); // associated events and lifecycle with this device
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
    public final BTRole getRole() {
        return BTRole.get(getRoleImpl());
    }
    private native byte getRoleImpl();

    @Override
    public BDAddressAndType getAddressAndType() { return addressAndType; }

    @Override
    public String getName() {
        if( !isValid() ) {
            return name_cached;
        } else {
            final String v = getNameImpl();
            name_cached = v;
            return v;
        }
    }
    private native String getNameImpl();

    @Override
    public BTType getBluetoothType() { return class_type(); }

    static BTType class_type() { return BTType.DEVICE; }

    @Override
    public BTGattService findGattService(final String service_uuid) {
        return (BTGattService) findInCache(service_uuid, BTType.GATT_SERVICE);
    }

    @Override
    public BTGattChar findGattChar(final String service_uuid, final String char_uuid) {
        final BTGattService s = findGattService(service_uuid);
        if( null == s ) {
            return null;
        }
        return s.findGattChar(char_uuid);
    }

    /* internal */

    private native void initImpl();

    /* DBT method calls: Connection */

    @Override
    public final boolean addStatusListener(final AdapterStatusListener l) {
        return getAdapter().addStatusListenerImpl(this, l);
    }

    @Override
    public boolean removeStatusListener(final AdapterStatusListener l) {
        return getAdapter().removeStatusListenerImpl(l);
    }

    @Override
    public final boolean getConnected() { return isConnected.get(); }

    @Override
    public final short getConnectionHandle() { return hciConnHandle; }

    @Override
    public HCIStatusCode getConnectedLE_PHY(final LE_PHYs[] resTx, final LE_PHYs[] resRx) {
        // pre-init
        resTx[0] = new LE_PHYs(LE_PHYs.PHY.LE_1M);
        resRx[0] = new LE_PHYs(LE_PHYs.PHY.LE_1M);
        final byte[] t = { resTx[0].mask };
        final byte[] r = { resRx[0].mask };
        final HCIStatusCode res = HCIStatusCode.get( getConnectedLE_PHYImpl(t, r) );
        resTx[0] = new LE_PHYs(t[0]);
        resRx[0] = new LE_PHYs(r[0]);
        return res;
    }
    private native byte getConnectedLE_PHYImpl(byte[] resTx, byte[] resRx);

    @Override
    public HCIStatusCode setConnectedLE_PHY(final boolean tryTx, final boolean tryRx,
                                            final LE_PHYs Tx, final LE_PHYs Rx) {
        return HCIStatusCode.get( setConnectedLE_PHYImpl(tryTx, tryRx, Tx.mask, Rx.mask) );
    }
    private native byte setConnectedLE_PHYImpl(final boolean tryTx, final boolean tryRx,
                                               final byte Tx, final byte Rx);

    @Override
    public final LE_PHYs getTxPhys() {
        return new LE_PHYs( getTxPhysImpl() );
    }
    private native byte getTxPhysImpl();

    @Override
    public final LE_PHYs getRxPhys() {
        return new LE_PHYs( getRxPhysImpl() );
    }
    private native byte getRxPhysImpl();

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

    /* SC SMP */

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
    public final SMPLinkKeyInfo getLinkKeyInfo(final boolean responder) {
        final byte[] stream = new byte[SMPLinkKeyInfo.byte_size];
        getLinkKeyInfoImpl(responder, stream);
        return new SMPLinkKeyInfo(stream, 0);
    }
    private final native void getLinkKeyInfoImpl(final boolean responder, final byte[] sink);

    @Override
    public final HCIStatusCode setLinkKeyInfo(final SMPLinkKeyInfo lk) {
        final byte[] stream = new byte[SMPLinkKeyInfo.byte_size];
        lk.getStream(stream, 0);
        return HCIStatusCode.get( setLinkKeyInfoImpl(stream) );
    }
    private final native byte setLinkKeyInfoImpl(final byte[] source);

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
    public final String toString() {
        if( !isValid() ) {
            // UTF-8 271D = Cross
            return "Device" + "\u271D" + "[address"+addressAndType+", '"+name_cached+
                    "', connected["+isConnected.get()+", 0x"+Integer.toHexString(hciConnHandle)+"]]";
        }
        return toStringImpl();
    }
    private native String toStringImpl();

    /**
     * {@inheritDoc}
     */
    @Override
    public final boolean remove() throws BTException {
        // close: clear java-listener, super.close()
        //        -> DBTNativeDownlink.delete(): deleteNativeJavaObject(..), deleteImpl(..) -> DBTDevice::remove()
        close();
        // return removeImpl();
        if( DBTAdapter.PRINT_DEVICE_LISTS || DEBUG ) {
            BTUtils.fprintf_td(System.err, "BTDevice::remove: %s", toString());
            getAdapter().printDeviceLists();
        }
        return true;
    }
    private native boolean removeImpl() throws BTException;

    @Override
    public final boolean isValid() { return super.isValid() /* && isValidImpl() */; }
    // private native boolean isValidImpl();

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
    public native short getRSSI();

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
