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
import java.util.Iterator;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

import org.direct_bt.AdapterSettings;
import org.direct_bt.AdapterStatusListener;
import org.direct_bt.BDAddressAndType;
import org.direct_bt.BDAddressType;
import org.direct_bt.BTAdapter;
import org.direct_bt.BTDevice;
import org.direct_bt.BTException;
import org.direct_bt.BTGattChar;
import org.direct_bt.BTGattDesc;
import org.direct_bt.BTGattService;
import org.direct_bt.BTManager;
import org.direct_bt.BTMode;
import org.direct_bt.BTObject;
import org.direct_bt.BTType;
import org.direct_bt.BTUtils;
import org.direct_bt.EIRDataTypeSet;
import org.direct_bt.EUI48;
import org.direct_bt.HCIStatusCode;
import org.direct_bt.HCIWhitelistConnectType;
import org.direct_bt.PairingMode;
import org.direct_bt.SMPPairingState;
import org.direct_bt.ScanType;

public class DBTAdapter extends DBTObject implements BTAdapter
{
    private static final boolean DEBUG = DBTManager.DEBUG;
    /* pp */ static final boolean PRINT_DEVICE_LISTS = false;

    private static AtomicInteger globThreadID = new AtomicInteger(0);
    private static int discoverTimeoutMS = 100;

    private final int dev_id;
    /**
     * The adapter's initially reported address by the system, which always reflects its public address, i.e. BDAddressType::BDADDR_LE_PUBLIC.
     */
    private final BDAddressAndType addressAndType;
    private final String name;
    /**
     * Either the adapter's initially reported public address or a random address setup via HCI before scanning / discovery.
     */
    private final BDAddressAndType visibleAddressAndType;

    private final Object discoveryLock = new Object();
    private final Object discoveredDevicesLock = new Object();

    private final AtomicBoolean isClosing = new AtomicBoolean(false);

    private final AtomicBoolean powered_state = new AtomicBoolean(false); // AdapterSettings
    private final AtomicBoolean isDiscoverable = new AtomicBoolean(false); // AdapterSettings

    private final AtomicReference<ScanType> currentMetaScanType = new AtomicReference<ScanType>(ScanType.NONE); // AdapterStatusListener and powerdOff

    private final List<WeakReference<BTDevice>> discoveredDevices = new ArrayList<WeakReference<BTDevice>>();

    /* pp */ DBTAdapter(final long nativeInstance,
                        final byte byteAddress[/*6*/],
                        final byte byteAddressType,
                        final String name, final int dev_id)
    {
        super(nativeInstance, compHash(java.util.Arrays.hashCode(byteAddress), 31+byteAddressType));
        this.dev_id = dev_id;
        this.addressAndType = new BDAddressAndType(new EUI48(byteAddress), BDAddressType.get(byteAddressType));
        this.name = name;
        this.visibleAddressAndType = addressAndType;
        addStatusListener(this.statusListener);
    }

    @Override
    public final BTManager getManager() { return DBTManager.getManager(); }

    @Override
    public void close() {
        final DBTManager mngr = (DBTManager)DBTManager.getManager();
        if( !isValid() ) {
            return;
        }
        if( !isClosing.compareAndSet(false, true) ) {
            return;
        }
        // mute all listener first
        removeAllStatusListener();

        stopDiscovery();

        final List<BTDevice> devices = getDiscoveredDevices();
        for(final Iterator<BTDevice> id = devices.iterator(); id.hasNext(); ) {
            final DBTDevice d = (DBTDevice) id.next();
            d.close();
        }

        // done in native dtor: removeDevicesImpl();

        poweredOff();

        mngr.removeAdapter(this); // remove this instance from manager

        super.close();
    }

    private final void poweredOff() {
        powered_state.set(false);
        currentMetaScanType.set(ScanType.NONE);
    }

    @Override
    public boolean equals(final Object obj)
    {
        if(this == obj) {
            return true;
        }
        if (obj == null || !(obj instanceof DBTAdapter)) {
            return false;
        }
        final DBTAdapter other = (DBTAdapter)obj;
        return dev_id == other.dev_id && addressAndType.equals(other.addressAndType);
    }

    @Override
    public BDAddressAndType getAddressAndType() { return addressAndType; }

    @Override
    public BDAddressAndType getVisibleAddressAndType() { return visibleAddressAndType; }

    @Override
    public String getName() { return name; }

    @Override
    public int getDevID() { return dev_id; }

    @Override
    public BTType getBluetoothType() { return class_type(); }

    static BTType class_type() { return BTType.ADAPTER; }

    @Override
    public BTDevice find(final String name, final BDAddressAndType addressAndType, final long timeoutMS) {
        return findDeviceInCache(name, addressAndType);
    }

    @Override
    public BTDevice find(final String name, final BDAddressAndType addressAndType) {
        return find(name, addressAndType, 0);
    }

    @Override
    public final boolean isDeviceWhitelisted(final BDAddressAndType addressAndType) { return isDeviceWhitelistedImpl(addressAndType.address.b, addressAndType.type.value); }
    private final native boolean isDeviceWhitelistedImpl(final byte[] address, byte address_type);

    @Override
    public boolean addDeviceToWhitelist(final BDAddressAndType addressAndType,
                                        final HCIWhitelistConnectType ctype,
                                        final short conn_interval_min, final short conn_interval_max,
                                        final short conn_latency, final short timeout) {
        return addDeviceToWhitelistImpl1(addressAndType.address.b, addressAndType.type.value, ctype.value,
                                    conn_interval_min, conn_interval_max, conn_latency, timeout);
    }
    private native boolean addDeviceToWhitelistImpl1(final byte[] address, final byte address_type, final int ctype,
                                        final short conn_interval_min, final short conn_interval_max,
                                        final short conn_latency, final short timeout);

    @Override
    public boolean addDeviceToWhitelist(final BDAddressAndType addressAndType,
                                        final HCIWhitelistConnectType ctype) {
        return addDeviceToWhitelistImpl2(addressAndType.address.b, addressAndType.type.value, ctype.value);
    }
    private native boolean addDeviceToWhitelistImpl2(final byte[] address, final byte address_type, final int ctype);

    @Override
    public boolean removeDeviceFromWhitelist(final BDAddressAndType addressAndType) {
        return removeDeviceFromWhitelistImpl(addressAndType.address.b, addressAndType.type.value);
    }
    private native boolean removeDeviceFromWhitelistImpl(final byte[] address, final byte address_type);

    /* Unsupported */

    @Override
    public boolean getDiscoverable() { return isDiscoverable.get(); }

    @Override
    public final ScanType getCurrentScanType() {
        return currentMetaScanType.get();
    }

    @Override
    public String toString() {
        if( !isValid() ) {
            return "Adapter" + "\u271D" + "["+addressAndType+", '"+name+"', id "+dev_id+"]";
        }
        return toStringImpl();
    }

    /* Native callbacks */

    /* Native functionality / properties */

    private native String toStringImpl();

    @Override
    public native boolean setPowered(boolean value);

    @Override
    public final HCIStatusCode initialize() {
        return initialize(BTMode.DUAL);
    }

    @Override
    public final HCIStatusCode initialize(final BTMode btMode) {
        return HCIStatusCode.get( initializeImpl(btMode.value) );
    }
    private native byte initializeImpl(final byte btModeInt);

    @Override
    public final HCIStatusCode reset() {
        return HCIStatusCode.get( resetImpl() );
    }
    private native byte resetImpl();

    @Override
    public native String getAlias();

    @Override
    public native void setAlias(final String value);

    @Override
    public native boolean setDiscoverable(boolean value);

    @Override
    public final BTDevice connectDevice(final BDAddressAndType addressAndType) {
        return connectDeviceImpl(addressAndType.address.b, addressAndType.type.value);
    }
    private native BTDevice connectDeviceImpl(byte[] address, byte addressType);

    @Override
    public native boolean setPairable(boolean value);

    @Override
    public boolean getPoweredState() { return powered_state.get(); }

    @Override
    public final boolean isPowered() { return isValid() && isPoweredImpl(); }
    public native boolean isPoweredImpl();

    @Override
    public final boolean isSuspended() { return isValid() && isSuspendedImpl(); }
    public native boolean isSuspendedImpl();

    @Override
    public final boolean isValid() { return super.isValid() && isValidImpl(); }
    public native boolean isValidImpl();

    /* internal */

    @Override
    protected native void deleteImpl(long nativeInstance);

    @Override
    public HCIStatusCode startDiscovery(final boolean keepAlive, final boolean le_scan_active) throws BTException {
        return startDiscovery(keepAlive, le_scan_active, (short)24, (short)24, (byte)0);
    }
    @Override
    public HCIStatusCode startDiscovery(final boolean keepAlive, final boolean le_scan_active,
                                        final short le_scan_interval, final short le_scan_window,
                                        final byte filter_policy) throws BTException {
        synchronized( discoveryLock ) {
            // Ignoring 'isDiscovering', as native implementation also handles change of 'keepAlive'.
            // The discoveredDevices shall always get cleared.
            removeDiscoveredDevicesImpl2j();
            final HCIStatusCode res = HCIStatusCode.get( startDiscoveryImpl(keepAlive, le_scan_active, le_scan_interval, le_scan_window, filter_policy) ); // event callbacks will be generated by implementation
            if( PRINT_DEVICE_LISTS || DEBUG ) {
                BTUtils.fprintf_td(System.err, "BTAdapter::startDiscovery: res %s, %s", res, toString());
                printDeviceLists();
            }
            return res;
        }
    }
    private native byte startDiscoveryImpl(final boolean keepAlive, final boolean le_scan_active,
                                           final short le_scan_interval, final short le_scan_window,
                                           final byte filter_policy) throws BTException;

    @Override
    public HCIStatusCode stopDiscovery() throws BTException {
        synchronized( discoveryLock ) {
            // Ignoring 'isDiscovering', be consistent with startDiscovery
            final HCIStatusCode res = HCIStatusCode.get( stopDiscoveryImpl() );  // event callbacks will be generated by implementation
            if( PRINT_DEVICE_LISTS || DEBUG ) {
                BTUtils.fprintf_td(System.err, "BTAdapter::stopDiscovery: res %s, %s", res, toString());
                printDeviceLists();
            }
            return res;
        }
    }
    private native byte stopDiscoveryImpl() throws BTException;

    // std::vector<std::shared_ptr<direct_bt::HCIDevice>> discoveredDevices = adapter.getDiscoveredDevices();
    private native List<BTDevice> getDiscoveredDevicesImpl();

    @Override
    public int removeDiscoveredDevices() throws BTException {
        final int cj = removeDiscoveredDevicesImpl2j();
        final int cn = removeDiscoveredDevicesImpl1();
        if( DEBUG ) {
            if( cj != cn ) {
                System.err.println("DBTAdapter::removeDevices: Unexpected discovered device count: Native "+cn+", callback "+cj);
            }
        }
        return cn;
    }
    private native int removeDiscoveredDevicesImpl1() throws BTException;
    private int removeDiscoveredDevicesImpl2j() {
        synchronized(discoveredDevicesLock) {
            final int n = discoveredDevices.size();
            discoveredDevices.clear();
            return n;
        }
    }
    /* pp */ boolean removeDiscoveredDevice(final BTDevice device) {
        return removeDiscoveredDeviceImpl2j( device.getAddressAndType() );
    }

    @Override
    public boolean removeDiscoveredDevice(final BDAddressAndType addressAndType) {
        final boolean cj = removeDiscoveredDeviceImpl2j(addressAndType);
        final boolean cn = removeDiscoveredDeviceImpl1(addressAndType.address.b, addressAndType.type.value);
        if( DEBUG ) {
            if( cj != cn ) {
                System.err.println("DBTAdapter::removeDevices("+addressAndType+"): Unexpected discovered device count: Native "+cn+", callback "+cj);
            }
        }
        return cn;
    }
    private native boolean removeDiscoveredDeviceImpl1(final byte[] address, final byte addressType);

    private boolean removeDiscoveredDeviceImpl2j(final BDAddressAndType addressAndType) {
        synchronized(discoveredDevicesLock) {
            for(final Iterator<WeakReference<BTDevice>> it = discoveredDevices.iterator(); it.hasNext();) {
                final BTDevice d = it.next().get();
                if( null == d ) {
                    it.remove();
                } else if( d.getAddressAndType().equals(addressAndType) ) {
                    it.remove();
                    return true;
                }
            }
        }
        return false;
    }
    @Override
    public List<BTDevice> getDiscoveredDevices() {
        final ArrayList<BTDevice> res = new ArrayList<BTDevice>();
        synchronized(discoveredDevicesLock) {
            for(final Iterator<WeakReference<BTDevice>> it = discoveredDevices.iterator(); it.hasNext();) {
                final BTDevice d = it.next().get();
                if( null == d ) {
                    it.remove();
                } else {
                    res.add(d);
                }
            }
        }
        return res;
    }
    private void cleanDiscoveredDevice() {
        synchronized(discoveredDevicesLock) {
            for(final Iterator<WeakReference<BTDevice>> it = discoveredDevices.iterator(); it.hasNext();) {
                final BTDevice d = it.next().get();
                if( null == d ) {
                    it.remove();
                }
            }
        }
    }

    @Override
    public final boolean addStatusListener(final AdapterStatusListener l) {
        final boolean added = addStatusListenerImpl(null, l);
        if( PRINT_DEVICE_LISTS || DEBUG ) {
            BTUtils.fprintf_td(System.err, "BTAdapter::addStatusListener: added %b, %s", added, toString());
            printDeviceLists();
        }
        return added;
    }
    /* pp */ native boolean addStatusListenerImpl(final BTDevice deviceOwnerAndMatch, final AdapterStatusListener l);

    @Override
    public boolean removeStatusListener(final AdapterStatusListener l) {
        boolean res = false;
        if( !isClosing.get() ) {
            res = removeStatusListenerImpl(l);
        }
        if( PRINT_DEVICE_LISTS || DEBUG ) {
            BTUtils.fprintf_td(System.err, "BTAdapter::removeStatusListener: removed %b, %s", res, toString());
            printDeviceLists();
        }
        return false;
    }
    /* pp */ native boolean removeStatusListenerImpl(final AdapterStatusListener l);

    @Override
    public native int removeAllStatusListener();

    @Override
    public final void printDeviceLists() {
        printDeviceListsImpl();
        List<WeakReference<BTDevice>> _discoveredDevices;
        synchronized(discoveredDevicesLock) {
            // Shallow (but expensive) copy to avoid java.util.ConcurrentModificationException while iterating: debug mode only
            _discoveredDevices = new ArrayList<WeakReference<BTDevice>>(discoveredDevices);
        }
        final int sz = _discoveredDevices.size();
        BTUtils.fprintf_td(System.err, "- BTAdapter::DiscoveredDevicesJ: %d elements%s", sz, System.lineSeparator());
        int idx = 0;
        for(final Iterator<WeakReference<BTDevice>> it = _discoveredDevices.iterator(); it.hasNext(); ++idx) {
            final BTDevice d = it.next().get();
            if( null == d ) {
                BTUtils.fprintf_td(System.err, "  - %d / %d: nil%s", (idx+1), sz, System.lineSeparator());
            } else {
                BTUtils.fprintf_td(System.err, "  - %d / %d: %s, name '%s'%s", (idx+1), sz,
                        d.getAddressAndType().toString(), d.getName(), System.lineSeparator());
            }
        }
    }
    private final native void printDeviceListsImpl();

    ////////////////////////////////////

    private final AdapterStatusListener statusListener = new AdapterStatusListener() {
        @Override
        public void adapterSettingsChanged(final BTAdapter a, final AdapterSettings oldmask, final AdapterSettings newmask,
                                           final AdapterSettings changedmask, final long timestamp) {
            final boolean initialSetting = oldmask.isEmpty();
            if( DEBUG ) {
                if( initialSetting ) {
                    System.err.println("Adapter.StatusListener.SETTINGS: "+oldmask+" -> "+newmask+", initial "+changedmask+" on "+a);
                } else {
                    System.err.println("Adapter.StatusListener.SETTINGS: "+oldmask+" -> "+newmask+", changed "+changedmask+" on "+a);
                }
            }
            if( initialSetting ) {
                powered_state.set( newmask.isSet(AdapterSettings.SettingType.POWERED) );
                isDiscoverable.set( newmask.isSet(AdapterSettings.SettingType.DISCOVERABLE) );
                return;
            }
            if( changedmask.isSet(AdapterSettings.SettingType.POWERED) ) {
                final boolean _isPowered = newmask.isSet(AdapterSettings.SettingType.POWERED);
                if( powered_state.compareAndSet(!_isPowered, _isPowered) ) {
                    if( !_isPowered ) {
                        poweredOff();
                    }
                }
            }
            if( changedmask.isSet(AdapterSettings.SettingType.DISCOVERABLE) ) {
                final boolean _isDiscoverable = newmask.isSet(AdapterSettings.SettingType.DISCOVERABLE);
                if( isDiscoverable.compareAndSet(!_isDiscoverable, _isDiscoverable) ) {
                }
            }
        }
        @Override
        public void discoveringChanged(final BTAdapter adapter, final ScanType currentMeta, final ScanType changedType, final boolean changedEnabled, final boolean keepAlive, final long timestamp) {
            if( DEBUG ) {
                System.err.println("Adapter.StatusListener.DISCOVERING: meta "+currentMeta+", changed["+changedType+", enabled "+changedEnabled+", keepAlive "+keepAlive+"] on "+adapter);
            }
            // meta ignores changes on temp disabled discovery
            currentMetaScanType.set(currentMeta);
        }
        @Override
        public boolean deviceFound(final BTDevice device, final long timestamp) {
            synchronized(discoveredDevicesLock) {
                cleanDiscoveredDevice();
                discoveredDevices.add(new WeakReference<BTDevice>(device));
            }
            if( PRINT_DEVICE_LISTS || DEBUG ) {
                System.err.println("Adapter.FOUND: discoveredDevices "+ discoveredDevices.size() + ": "+device+", on "+device.getAdapter());
            }
            return false;
        }

        @Override
        public void deviceUpdated(final BTDevice device, final EIRDataTypeSet updateMask, final long timestamp) {
            final boolean rssiUpdated = updateMask.isSet( EIRDataTypeSet.DataType.RSSI );
            final boolean mdUpdated = updateMask.isSet( EIRDataTypeSet.DataType.MANUF_DATA );
            if( DEBUG && !rssiUpdated && !mdUpdated) {
                System.err.println("Adapter.UPDATED: "+updateMask+" of "+device+" on "+device.getAdapter());
            }
            // nop on discoveredDevices
        }

        @Override
        public void deviceConnected(final BTDevice device, final short handle, final long timestamp) {
            if( DEBUG ) {
                System.err.println("Adapter.CONNECTED: "+device+" on "+device.getAdapter());
            }
        }

        @Override
        public void devicePairingState(final BTDevice device, final SMPPairingState state, final PairingMode mode, final long timestamp) {
            if( DEBUG ) {
                System.err.println("Adapter.PAIRING_STATE: state "+state+", mode "+mode+": "+device);
            }
        }

        @Override
        public void deviceReady(final BTDevice device, final long timestamp) {
            if( DEBUG ) {
                System.err.println("Adapter.READY: "+device);
            }
        }

        @Override
        public void deviceDisconnected(final BTDevice device, final HCIStatusCode reason, final short handle, final long timestamp) {
            if( DEBUG ) {
                System.err.println("Adapter.DISCONNECTED: Reason "+reason+", old handle 0x"+Integer.toHexString(handle)+": "+device+" on "+device.getAdapter());
            }
        }

        @Override
        public String toString() {
            return "AdapterStatusListener[adapter "+addressAndType.toString()+"]";
        }

    };

    /**
     * Returns the matching {@link DBTObject} from the internal cache if found,
     * otherwise {@code null}.
     * <p>
     * The returned {@link DBTObject} may be of type
     * <ul>
     *   <li>{@link DBTDevice}</li>
     *   <li>{@link DBTGattService}</li>
     *   <li>{@link DBTGattChar}</li>
     *   <li>{@link DBTGattDesc}</li>
     * </ul>
     * or alternatively in {@link BTObject} space
     * <ul>
     *   <li>{@link BTType#DEVICE} -> {@link BTDevice}</li>
     *   <li>{@link BTType#GATT_SERVICE} -> {@link BTGattService}</li>
     *   <li>{@link BTType#GATT_CHARACTERISTIC} -> {@link BTGattChar}</li>
     *   <li>{@link BTType#GATT_DESCRIPTOR} -> {@link BTGattDesc}</li>
     * </ul>
     * </p>
     * @param name name of the desired {@link BTType#DEVICE device}.
     * Maybe {@code null}.
     * @param identifier EUI48 address of the desired {@link BTType#DEVICE device}
     * or UUID of the desired {@link BTType#GATT_SERVICE service},
     * {@link BTType#GATT_CHARACTERISTIC characteristic} or {@link BTType#GATT_DESCRIPTOR descriptor} to be found.
     * Maybe {@code null}, in which case the first object of the desired type is being returned - if existing.
     * @param type specify the type of the object to be found, either
     * {@link BTType#DEVICE device},
     * {@link BTType#GATT_SERVICE service}, {@link BTType#GATT_CHARACTERISTIC characteristic}
     * or {@link BTType#GATT_DESCRIPTOR descriptor}.
     * {@link BTType#NONE none} means anything.
     */
    /* pp */ DBTObject findInCache(final String name, final String identifier, final BTType type) {
        final boolean anyType = BTType.NONE == type;
        final boolean deviceType = BTType.DEVICE == type;
        final boolean serviceType = BTType.GATT_SERVICE == type;
        final boolean charType = BTType.GATT_CHARACTERISTIC== type;
        final boolean descType = BTType.GATT_DESCRIPTOR == type;

        if( !anyType && !deviceType && !serviceType && !charType && !descType ) {
            return null;
        }
        synchronized(discoveredDevicesLock) {
            cleanDiscoveredDevice();

            if( null == name && null == identifier && ( anyType || deviceType ) ) {
                // special case for 1st valid device
                if( discoveredDevices.size() > 0 ) {
                    return (DBTDevice) discoveredDevices.get(0).get();
                }
                return null; // no device
            }
            for(int devIdx = discoveredDevices.size() - 1; devIdx >= 0; devIdx-- ) {
                final DBTDevice device = (DBTDevice) discoveredDevices.get(devIdx).get();
                if( ( anyType || deviceType ) ) {
                    if( null != name && null != identifier &&
                        device.getName().equals(name) &&
                        device.getAddressAndType().address.toString().equals(identifier)
                      )
                    {
                        return device;
                    }
                    if( null != identifier &&
                        device.getAddressAndType().address.toString().equals(identifier)
                      )
                    {
                        return device;
                    }
                    if( null != name &&
                        device.getName().equals(name)
                      )
                    {
                        return device;
                    }
                }
                if( anyType || serviceType || charType || descType ) {
                    final DBTObject dbtObj = device.findInCache(identifier, type);
                    if( null != dbtObj ) {
                        return dbtObj;
                    }
                }
            }
            return null;
        }
    }

    /* pp */ DBTDevice findDeviceInCache(final String name, final BDAddressAndType addressAndType) {
        synchronized(discoveredDevicesLock) {
            cleanDiscoveredDevice();

            if( null == name && null == addressAndType ) {
                // special case for 1st valid device
                if( discoveredDevices.size() > 0 ) {
                    return (DBTDevice) discoveredDevices.get(0).get();
                }
                return null; // no device
            }
            for(int devIdx = discoveredDevices.size() - 1; devIdx >= 0; devIdx-- ) {
                final DBTDevice device = (DBTDevice) discoveredDevices.get(devIdx).get();
                if( null != name && null != addressAndType &&
                    device.getName().equals(name) &&
                    device.getAddressAndType().equals(addressAndType)
                  )
                {
                    return device;
                }
                if( null != addressAndType &&
                    device.getAddressAndType().equals(addressAndType)
                  )
                {
                    return device;
                }
                if( null != name &&
                    device.getName().equals(name)
                  )
                {
                    return device;
                }
            }
            return null;
        }
    }
}
