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

import org.direct_bt.BTAdapter;
import org.direct_bt.BTDevice;
import org.direct_bt.BTException;
import org.direct_bt.BTGattService;
import org.direct_bt.BTManager;
import org.direct_bt.BTObject;
import org.direct_bt.BTType;
import org.direct_bt.HCIStatusCode;
import org.direct_bt.BTManager.ChangedAdapterSetListener;

public class DBusManager implements BTManager
{
    private long nativeInstance;
    private final Settings settings;

    private native static String getNativeAPIVersion();

    public native BTType getBluetoothType();

    private native DBusObject find(int type, String name, String identifier, BTObject parent, long milliseconds);

    @Override
    public final Settings getSettings() {
        return settings;
    }

    @Override
    public DBusObject find(final BTType type, final String name, final String identifier, final BTObject parent, final long timeoutMS) {
        return find(type.ordinal(), name, identifier, parent, timeoutMS);
    }

    @Override
    public DBusObject find(final BTType type, final String name, final String identifier, final BTObject parent) {
        return find(type, name, identifier, parent, 0);
    }

    @SuppressWarnings("unchecked")
    @Override
    public <T extends BTObject>  T find(final String name, final String identifier, final BTObject parent, final long timeoutMS) {
        return (T) find(DBusObject.class_type().ordinal(), name, identifier, parent, timeoutMS);
    }

    @SuppressWarnings("unchecked")
    @Override
    public <T extends BTObject>  T find(final String name, final String identifier, final BTObject parent) {
        return (T) find(name, identifier, parent, 0);
    }

    @Override
    public BTObject getObject(final BTType type, final String name,
                                final String identifier, final BTObject parent) {
        return getObject(type.ordinal(), name, identifier, parent);
    }
    private native BTObject getObject(int type, String name, String identifier, BTObject parent);

    @Override
    public List<BTObject> getObjects(final BTType type, final String name,
                                    final String identifier, final BTObject parent) {
        return getObjects(type.ordinal(), name, identifier, parent);
    }
    private native List<BTObject> getObjects(int type, String name, String identifier, BTObject parent);

    @Override
    public native List<BTAdapter> getAdapters();

    @Override
    public BTAdapter getAdapter(final int dev_id) { return null; } // FIXME

    @Override
    public native List<BTDevice> getDevices();

    @Override
    public native List<BTGattService> getServices();

    @Override
    public native boolean setDefaultAdapter(BTAdapter adapter);

    @Override
    public native BTAdapter getDefaultAdapter();

    @Override
    public native boolean startDiscovery() throws BTException;

    @Override
    public HCIStatusCode startDiscovery(final boolean keepAlive, final boolean le_scan_active) throws BTException {
        return startDiscovery() ? HCIStatusCode.SUCCESS : HCIStatusCode.INTERNAL_FAILURE; // FIXME keepAlive, le_scan_active
    }

    @Override
    public HCIStatusCode stopDiscovery() throws BTException {
        return stopDiscoveryImpl() ? HCIStatusCode.SUCCESS : HCIStatusCode.INTERNAL_FAILURE;
    }
    private native boolean stopDiscoveryImpl() throws BTException;

    @Override
    public native boolean getDiscovering() throws BTException;

    private native void init() throws BTException;
    private native void delete();
    private DBusManager()
    {
        init();
        settings = new Settings() {
            @Override
            public final boolean isDirectBT() {
                return false;
            }
            @Override
            public boolean isTinyB() {
                return true;
            }
            @Override
            public boolean isCharValueCacheNotificationSupported() {
                return true;
            }
            @Override
            public String toString() {
                return "Settings[dbt false, tinyb true, charValueCacheNotify "+isCharValueCacheNotificationSupported()+"]";
            }
        };
        System.err.println("DBusManager: Using "+settings.toString());
    }

    /** Returns an instance of BluetoothManager, to be used instead of constructor.
      * @return An initialized BluetoothManager instance.
      */
    public static synchronized BTManager getManager() throws RuntimeException, BTException {
        return LazySingletonHolder.singleton;
    }
    /** Initialize-On-Demand Holder Class, similar to C++11's "Magic Statics". */
    private static class LazySingletonHolder {
        private static final DBusManager singleton = new DBusManager();
    }

    @Override
    protected void finalize()
    {
        delete();
    }

    @Override
    public void shutdown() {
        delete();
    }

    @Override
    public final void addChangedAdapterSetListener(final ChangedAdapterSetListener l) {} // FIXME

    @Override
    public final int removeChangedAdapterSetListener(final ChangedAdapterSetListener l) { return 0; } // FIXME
}
