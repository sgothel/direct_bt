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

import java.util.ArrayList;
import java.util.Collections;
import java.util.Iterator;
import java.util.List;
import java.util.UUID;

import org.direct_bt.AdapterStatusListener;
import org.direct_bt.BDAddressAndType;
import org.direct_bt.BDAddressType;
import org.direct_bt.BTAdapter;
import org.direct_bt.BTDevice;
import org.direct_bt.BTException;
import org.direct_bt.BTManager;
import org.direct_bt.BTNotification;
import org.direct_bt.BTType;
import org.direct_bt.EUI48;
import org.direct_bt.HCIStatusCode;
import org.direct_bt.HCIWhitelistConnectType;
import org.direct_bt.ScanType;
import org.direct_bt.TransportType;

public class DBusAdapter extends DBusObject implements BTAdapter
{
    @Override
    public native BTType getBluetoothType();

    @Override
    public native BTAdapter clone();

    static BTType class_type() { return BTType.ADAPTER; }

    @Override
    public BTDevice find(final String name, final BDAddressAndType addressAndType, final long timeoutMS) {
        final BTManager manager = DBusManager.getManager();
        return (BTDevice) manager.find(BTType.DEVICE, name, addressAndType.address.toString(), this, timeoutMS);
    }

    @Override
    public BTDevice find(final String name, final BDAddressAndType addressAndType) {
            return find(name, addressAndType, 0);
    }

    @Override
    public boolean isPowered() { return isValid() && getPoweredState(); }

    @Override
    public final boolean isSuspended() { return isValid() && !getPoweredState(); }

    @Override
    public final boolean isValid() { return super.isValid(); }

    @Override
    public boolean isDeviceWhitelisted(final BDAddressAndType addressAndType) {
        return false; // FIXME
    }

    @Override
    public boolean addDeviceToWhitelist(final BDAddressAndType addressAndType,
                                        final HCIWhitelistConnectType ctype,
                                        final short min_interval, final short max_interval,
                                        final short latency, final short timeout) {
        return false; // FIXME
    }

    @Override
    public boolean addDeviceToWhitelist(final BDAddressAndType addressAndType,
                                        final HCIWhitelistConnectType ctype) {
        return false; // FIXME
    }

    @Override
    public boolean removeDeviceFromWhitelist(final BDAddressAndType addressAndType) {
        return false; // FIXME
    }

    @Override
    public final BTManager getManager() { return DBusManager.getManager(); }

    /* D-Bus method calls: */

    @Override
    @Deprecated
    public native boolean startDiscovery() throws BTException;

    @Override
    public synchronized HCIStatusCode startDiscovery(final boolean keepAlive, final boolean le_scan_active) throws BTException {
        return startDiscovery() ? HCIStatusCode.SUCCESS : HCIStatusCode.INTERNAL_FAILURE; // FIXME keepAlive, le_scan_active
    }
    @Override
    public HCIStatusCode startDiscovery(final boolean keepAlive, final boolean le_scan_active,
                                        final short le_scan_interval, final short le_scan_window,
                                        final byte filter_policy) throws BTException {
        return startDiscovery() ? HCIStatusCode.SUCCESS : HCIStatusCode.INTERNAL_FAILURE; // FIXME keepAlive, le_scan_active, ...
    }

    @Override
    public HCIStatusCode stopDiscovery() throws BTException {
        return stopDiscoveryImpl() ? HCIStatusCode.SUCCESS : HCIStatusCode.INTERNAL_FAILURE;
    }
    private native boolean stopDiscoveryImpl() throws BTException;

    @Override
    public native List<BTDevice> getDiscoveredDevices();

    @Override
    public native int removeDiscoveredDevices() throws BTException;

    @Override
    public boolean removeDiscoveredDevice(final BDAddressAndType addressAndType) {
        return false; // FIXME
    }

    /* D-Bus property accessors: */

    @Override
    public native String getAddressString();

    @Override
    public EUI48 getAddress() { return new EUI48(getAddressString()); }

    @Override
    public BDAddressAndType getAddressAndType() { return new BDAddressAndType(getAddress(), BDAddressType.BDADDR_LE_PUBLIC); }

    @Override
    public BDAddressAndType getVisibleAddressAndType() { return getAddressAndType(); }

    @Override
    public native String getName();

    @Override
    public int getDevID() { return 0; } // FIXME

    @Override
    public native String getAlias();

    @Override
    public native void setAlias(String value);

    @Override
    public native long getBluetoothClass();

    @Override
    public native boolean getPoweredState();

    @Override
    public native void enablePoweredNotifications(BTNotification<Boolean> callback);

    @Override
    public native void disablePoweredNotifications();

    @Override
    public native boolean setPowered(boolean value);

    @Override
    public final HCIStatusCode reset() { return HCIStatusCode.INTERNAL_FAILURE; }

    @Override
    public native boolean getDiscoverable();

    @Override
    public native void enableDiscoverableNotifications(BTNotification<Boolean> callback);

    @Override
    public native void disableDiscoverableNotifications();

    @Override
    public native boolean setDiscoverable(boolean value);

    @Override
    public native long getDiscoverableTimeout();

    @Override
    public native boolean setDiscoverableTimout(long value);

    @Override
    public BTDevice connectDevice(final BDAddressAndType addressAndType) {
        return connectDeviceImpl(addressAndType.address.toString(), addressAndType.type.toDbusString());
    }
    private native BTDevice connectDeviceImpl(String address, String addressType);

    @Override
    public native boolean getPairable();

    @Override
    public native void enablePairableNotifications(BTNotification<Boolean> callback);

    @Override
    public native void disablePairableNotifications();

    @Override
    public native boolean setPairable(boolean value);

    @Override
    public native long getPairableTimeout();

    @Override
    public native boolean setPairableTimeout(long value);

    @Override
    public final ScanType getCurrentScanType() {
        return getDiscovering() ? ScanType.LE : ScanType.NONE;
    }
    @Override
    public native boolean getDiscovering();

    @Override
    public boolean addStatusListener(final AdapterStatusListener l) {
        return false; // FIXME
    }

    @Override
    public boolean removeStatusListener(final AdapterStatusListener l) {
        return false; // FIXME
    }

    @Override
    public int removeAllStatusListener() {
        return 0; // FIXME
    }

    @Override
    public native void enableDiscoveringNotifications(BTNotification<Boolean> callback);

    @Override
    public native void disableDiscoveringNotifications();

    @Override
    public native String[] getUUIDs();

    @Override
    public native String getModalias();

    @Override
    public void setDiscoveryFilter(final List<UUID> uuids, final int rssi, final int pathloss, final TransportType transportType) {
        final List<String> uuidsFmt = new ArrayList<>(uuids.size());
        for (final UUID uuid : uuids) {
            uuidsFmt.add(uuid.toString());
        }
        setDiscoveryFilter(uuidsFmt, rssi, pathloss, transportType.ordinal());
    }

    @SuppressWarnings("unchecked")
    public void setRssiDiscoveryFilter(final int rssi) {
        setDiscoveryFilter(Collections.EMPTY_LIST, rssi, 0, TransportType.AUTO);
    }

    @Override
    public String getInterfaceName() {
        final String[] path = getObjectPath().split("/");
        return path[path.length-1];
    }

    private native void delete();

    private native void setDiscoveryFilter(List<String> uuids, int rssi, int pathloss, int transportType);

    private DBusAdapter(final long instance)
    {
        super(instance);
    }

    @Override
    public String toString() {
        return "Adapter["+getAddress()+", '"+getName()+"']";
    }

    @Override
    public final void printDeviceLists() {
        // FIXME
    }
}
