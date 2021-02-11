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
import java.util.Map;

import org.direct_bt.BDAddressAndType;
import org.direct_bt.BDAddressType;
import org.direct_bt.BTSecurityLevel;
import org.direct_bt.BTDevice;
import org.direct_bt.BTException;
import org.direct_bt.BTGattChar;
import org.direct_bt.BTGattService;
import org.direct_bt.BTManager;
import org.direct_bt.BTNotification;
import org.direct_bt.BTType;
import org.direct_bt.BTUtils;
import org.direct_bt.EUI48;
import org.direct_bt.BTGattCharListener;
import org.direct_bt.HCIStatusCode;
import org.direct_bt.PairingMode;
import org.direct_bt.SMPIOCapability;
import org.direct_bt.SMPKeyMask;
import org.direct_bt.SMPLongTermKeyInfo;
import org.direct_bt.SMPPairingState;
import org.direct_bt.SMPSignatureResolvingKeyInfo;

public class DBusDevice extends DBusObject implements BTDevice
{
    @Override
    public final long getCreationTimestamp() { return ts_creation; }

    @Override
    public final long getLastDiscoveryTimestamp() { return ts_creation; } // FIXME

    @Override
    public final long getLastUpdateTimestamp() { return ts_creation; } // FIXME

    @Override
    public native BTType getBluetoothType();
    @Override
    public native DBusDevice clone();

    static BTType class_type() { return BTType.DEVICE; }

    @Override
    public BTGattService find(final String UUID, final long timeoutMS) {
        final BTManager manager = DBusManager.getManager();
        return (BTGattService) manager.find(BTType.GATT_SERVICE,
                null, UUID, this, timeoutMS);
    }

    @Override
    public BTGattService find(final String UUID) {
        return find(UUID, 0);
    }

    /* D-Bus method calls: */

    @Override
    public final HCIStatusCode disconnect() throws BTException {
        return disconnectImpl() ? HCIStatusCode.SUCCESS : HCIStatusCode.UNSPECIFIED_ERROR ;
    }
    private native boolean disconnectImpl() throws BTException;


    @Override
    public final HCIStatusCode connect() throws BTException {
        return connectImpl() ? HCIStatusCode.SUCCESS : HCIStatusCode.UNSPECIFIED_ERROR ;
    }
    private native boolean connectImpl() throws BTException;

    @Override
    public HCIStatusCode connectLE(final short interval, final short window,
                                   final short min_interval, final short max_interval,
                                   final short latency, final short timeout) {
        return connect(); // FIXME connection params ...
    }

    @Override
    public native boolean connectProfile(String arg_UUID) throws BTException;

    @Override
    public native boolean disconnectProfile(String arg_UUID) throws BTException;

    @Override
    public final SMPKeyMask getAvailableSMPKeys(final boolean responder) { return new SMPKeyMask(); }

    @Override
    public final SMPLongTermKeyInfo getLongTermKeyInfo(final boolean responder) { return new SMPLongTermKeyInfo(); } // FIXME

    @Override
    public final HCIStatusCode setLongTermKeyInfo(final SMPLongTermKeyInfo ltk) { return HCIStatusCode.NOT_SUPPORTED; } // FIXME

    @Override
    public final SMPSignatureResolvingKeyInfo getSignatureResolvingKeyInfo(final boolean responder) { return new SMPSignatureResolvingKeyInfo(); } // FIXME

    @Override
    public native boolean pair() throws BTException;

    @Override
    public final HCIStatusCode unpair() { return HCIStatusCode.NOT_SUPPORTED; } // FIXME

    @Override
    public final boolean setConnSecurityLevel(final BTSecurityLevel sec_level) { return false; } // FIXME

    @Override
    public final BTSecurityLevel getConnSecurityLevel() { return BTSecurityLevel.UNSET; } // FIXME

    @Override
    public final boolean setConnIOCapability(final SMPIOCapability io_cap) { return false; } // FIXME

    @Override
    public final SMPIOCapability getConnIOCapability() { return SMPIOCapability.UNSET; } // FIXME

    @Override
    public final boolean setConnSecurity(final BTSecurityLevel sec_level, final SMPIOCapability io_cap) { return false; } // FIXME

    @Override
    public final boolean setConnSecurityBest(final BTSecurityLevel sec_level, final SMPIOCapability io_cap) { return false; } // FIXME

    @Override
    public final boolean setConnSecurityAuto(final SMPIOCapability iocap_auto) { return false; } // FIXME

    @Override
    public final boolean isConnSecurityAutoEnabled() { return false; } // FIXME

    @Override
    public HCIStatusCode setPairingPasskey(final int passkey) { return HCIStatusCode.INTERNAL_FAILURE; }

    @Override
    public HCIStatusCode setPairingPasskeyNegative() { return HCIStatusCode.INTERNAL_FAILURE; }

    @Override
    public HCIStatusCode setPairingNumericComparison(final boolean equal) { return HCIStatusCode.INTERNAL_FAILURE; }

    @Override
    public PairingMode getPairingMode() { return PairingMode.NONE; }

    @Override
    public SMPPairingState getPairingState() { return SMPPairingState.NONE; }

    @Override
    public native boolean remove() throws BTException;

    @Override
    public final boolean isValid() { return super.isValid(); }

    @Override
    public native boolean cancelPairing() throws BTException;

    @Override
    public native List<BTGattService> getServices();

    @Override
    public boolean pingGATT() { return true; } // FIXME

    /* D-Bus property accessors: */

    @Override
    public BDAddressAndType getAddressAndType() { return new BDAddressAndType(new EUI48(getAddressString()), BDAddressType.BDADDR_LE_PUBLIC /* FIXME */); }

    @Override
    public native String getAddressString();

    @Override
    public native String getName();

    @Override
    public native String getAlias();

    @Override
    public native void setAlias(String value);

    @Override
    public native int getBluetoothClass();

    @Override
    public native short getAppearance();

    @Override
    public native String getIcon();

    @Override
    public native boolean getPaired();

    @Override
    public native void enablePairedNotifications(BTNotification<Boolean> callback);

    @Override
    public native void disablePairedNotifications();

    @Override
    public native boolean getTrusted();

    @Override
    public native void enableTrustedNotifications(BTNotification<Boolean> callback);

    @Override
    public native void disableTrustedNotifications();

    @Override
    public native void setTrusted(boolean value);

    @Override
    public native boolean getBlocked();

    @Override
    public native void enableBlockedNotifications(BTNotification<Boolean> callback);

    @Override
    public native void disableBlockedNotifications();

    @Override
    public native void setBlocked(boolean value);

    @Override
    public native boolean getLegacyPairing();

    @Override
    public native short getRSSI();

    @Override
    public native void enableRSSINotifications(BTNotification<Short> callback);

    @Override
    public native void disableRSSINotifications();

    @Override
    public native boolean getConnected();

    @Override
    public final short getConnectionHandle() { return 0; /* FIXME */ }

    @Override
    public native void enableConnectedNotifications(BTNotification<Boolean> callback);

     @Override
    public native void disableConnectedNotifications();

    @Override
    public native String[] getUUIDs();

    @Override
    public native String getModalias();

    @Override
    public native DBusAdapter getAdapter();

    @Override
    public native Map<Short, byte[]> getManufacturerData();

    @Override
    public native void enableManufacturerDataNotifications(BTNotification<Map<Short, byte[]> > callback);

    @Override
    public native void disableManufacturerDataNotifications();


    @Override
    public native Map<String, byte[]> getServiceData();

    @Override
    public native void enableServiceDataNotifications(BTNotification<Map<String, byte[]> > callback);

    @Override
    public native void disableServiceDataNotifications();

    @Override
    public native short getTxPower ();

    @Override
    public native boolean getServicesResolved ();

    @Override
    public native void enableServicesResolvedNotifications(BTNotification<Boolean> callback);

    @Override
    public native void disableServicesResolvedNotifications();

    @Override
    public boolean addCharListener(final BTGattCharListener listener) {
        return false; // FIXME
    }

    @Override
    public boolean removeCharListener(final BTGattCharListener l) {
        return false; // FIXME
    }

    @Override
    public int removeAllAssociatedCharListener(final BTGattChar associatedCharacteristic) {
        return 0; // FIXME
    }

    @Override
    public int removeAllCharListener() {
        return 0; // FIXME
    }

    private native void delete();

    private DBusDevice(final long instance)
    {
        super(instance);
        ts_creation = BTUtils.currentTimeMillis();
    }
    final long ts_creation;

    @Override
    public String toString() {
        return "Device[address"+getAddressAndType()+", '"+getName()+"']";
    }
}
