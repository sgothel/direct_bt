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

import java.security.AccessController;
import java.security.PrivilegedAction;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.function.Consumer;
import java.util.function.Predicate;

import org.direct_bt.BTAdapter;
import org.direct_bt.BTDevice;
import org.direct_bt.BTException;
import org.direct_bt.BTFactory;
import org.direct_bt.BTGattChar;
import org.direct_bt.BTGattDesc;
import org.direct_bt.BTGattService;
import org.direct_bt.BTManager;
import org.direct_bt.BTObject;
import org.direct_bt.BTType;
import org.direct_bt.HCIStatusCode;

public class DBTManager implements BTManager
{
    protected static final boolean DEBUG = BTFactory.DEBUG;
    protected static final boolean VERBOSE = BTFactory.VERBOSE;

    private static volatile boolean isJVMShuttingDown = false;
    private static final List<Runnable> userShutdownHooks = new ArrayList<Runnable>();
    private static boolean unifyUUID128Bit = true;

    static {
        AccessController.doPrivileged(new PrivilegedAction<Object>() {
            @Override
            public Object run() {
                Runtime.getRuntime().addShutdownHook(
                    new Thread(null, new Runnable() {
                                @Override
                                public void run() {
                                    DBTManager.shutdownImpl(true);
                                } }, "DBTManager_ShutdownHook" ) ) ;
                return null;
            } } ) ;
    }

    private static synchronized void shutdownImpl(final boolean _isJVMShuttingDown) {
        isJVMShuttingDown = _isJVMShuttingDown;
        if(DEBUG) {
            System.err.println("DBTManager.shutdown() START: JVM Shutdown "+isJVMShuttingDown+", on thread "+Thread.currentThread().getName());
        }
        synchronized(userShutdownHooks) {
            final int cshCount = userShutdownHooks.size();
            for(int i=0; i < cshCount; i++) {
                try {
                    if( DEBUG ) {
                        System.err.println("DBTManager.shutdown - userShutdownHook #"+(i+1)+"/"+cshCount);
                    }
                    userShutdownHooks.get(i).run();
                } catch(final Throwable t) {
                    System.err.println("DBTManager.shutdown: Caught "+t.getClass().getName()+" during userShutdownHook #"+(i+1)+"/"+cshCount);
                    if( DEBUG ) {
                        t.printStackTrace();
                    }
                }
            }
            userShutdownHooks.clear();
        }
        if(DEBUG) {
            System.err.println("DBTManager.shutdown(): Post userShutdownHook");
        }

        try {
            final BTManager mgmt = getManager();
            mgmt.shutdown();
        } catch(final Throwable t) {
            System.err.println("DBTManager.shutdown: Caught "+t.getClass().getName()+" during DBTManager.shutdown()");
            if( DEBUG ) {
                t.printStackTrace();
            }
        }

        if(DEBUG) {
            System.err.println(Thread.currentThread().getName()+" - DBTManager.shutdown() END JVM Shutdown "+isJVMShuttingDown);
        }
    }

    /** Returns true if the JVM is shutting down, otherwise false. */
    public static final boolean isJVMShuttingDown() { return isJVMShuttingDown; }

    /**
     * Add a shutdown hook to be performed at JVM shutdown before shutting down DBTManager instance.
     *
     * @param head if true add runnable at the start, otherwise at the end
     * @param runnable runnable to be added.
     */
    public static void addShutdownHook(final boolean head, final Runnable runnable) {
        synchronized( userShutdownHooks ) {
            if( !userShutdownHooks.contains( runnable ) ) {
                if( head ) {
                    userShutdownHooks.add(0, runnable);
                } else {
                    userShutdownHooks.add( runnable );
                }
            }
        }
    }

    /**
     * Returns whether uuid128_t consolidation is enabled
     * for native uuid16_t and uuid32_t values before string conversion.
     * @see #setUnifyUUID128Bit(boolean)
     */
    public static boolean getUnifyUUID128Bit() { return unifyUUID128Bit; }

    /**
     * Enables or disables uuid128_t consolidation
     * for native uuid16_t and uuid32_t values before string conversion.
     * <p>
     * Default is {@code true}, as this represent compatibility with original TinyB D-Bus behavior.
     * </p>
     * <p>
     * If desired, this value should be set once before the first call of {@link #getManager()}!
     * </p>
     * @see #getUnifyUUID128Bit()
     */
    public static void setUnifyUUID128Bit(final boolean v) { unifyUUID128Bit=v; }

    private long nativeInstance;
    private final List<BTAdapter> adapters = new CopyOnWriteArrayList<BTAdapter>();
    private final List<ChangedAdapterSetListener> changedAdapterSetListenerList = new CopyOnWriteArrayList<ChangedAdapterSetListener>();

    private final Settings settings;

    @Override
    public final Settings getSettings() { return settings; }

    public BTType getBluetoothType() { return BTType.NONE; }

    @Override
    public DBTObject find(final BTType type, final String name, final String identifier, final BTObject parent, final long timeoutMS) {
        return findInCache((DBTObject)parent, type, name, identifier);
    }

    @Override
    public DBTObject find(final BTType type, final String name, final String identifier, final BTObject parent) {
        return find(type, name, identifier, parent, 0);
    }

    @Override
    public <T extends BTObject>  T find(final String name, final String identifier, final BTObject parent, final long timeoutMS) {
        // Due to generic type erasure, we cannot determine the matching BluetoothType for the return parameter,
        // hence this orig TinyB API method is rather misleading than useful.
        throw new UnsupportedOperationException("Generic return type 'find' won't be implemented.");
        // return (T) find(BluetoothType.NONE, name, identifier, parent, timeoutMS);
    }

    @Override
    public <T extends BTObject>  T find(final String name, final String identifier, final BTObject parent) {
        // Due to generic type erasure, we cannot determine the matching BluetoothType for the return parameter,
        // hence this orig TinyB API method is rather misleading than useful.
        throw new UnsupportedOperationException("Generic return type 'find' won't be implemented.");
        // return (T) find(BluetoothType.NONE, name, identifier, parent, 0);
    }

    @Override
    public BTObject getObject(final BTType type, final String name,
                                final String identifier, final BTObject parent) {
        return getObject(type.ordinal(), name, identifier, parent);
    }
    private BTObject getObject(final int type, final String name, final String identifier, final BTObject parent)
    { throw new UnsupportedOperationException(); } // FIXME

    @Override
    public List<BTObject> getObjects(final BTType type, final String name,
                                    final String identifier, final BTObject parent) {
        return getObjects(type.ordinal(), name, identifier, parent);
    }
    private List<BTObject> getObjects(final int type, final String name, final String identifier, final BTObject parent)
    { throw new UnsupportedOperationException(); } // FIXME

    @Override
    public List<BTAdapter> getAdapters() { return new ArrayList<BTAdapter>(adapters); }

    @Override
    public BTAdapter getAdapter(final int dev_id) {
        for(final Iterator<BTAdapter> iter = adapters.iterator(); iter.hasNext(); ) {
            final BTAdapter a = iter.next();
            if( dev_id == a.getDevID() ) {
                return a;
            }
        }
        return null;
    }

    @Override
    public List<BTDevice> getDevices() { return getDefaultAdapter().getDiscoveredDevices(); }

    /**
     * {@inheritDoc}
     * <p>
     * This call could be a quite expensive service query, see below.
     * </p>
     * <p>
     * This implementation returns all {@link BTGattService} from all {@link BTDevice}s
     * from the {@link #getDefaultAdapter()} using {@link BTDevice#getServices()}.
     * </p>
     * <p>
     * This implementation does not {@link BTAdapter#startDiscovery() start} an explicit discovery,
     * but previous {@link BTAdapter#getDiscoveredDevices() discovered devices} are being queried.
     * </p>
     */
    @Override
    public List<BTGattService> getServices() {
        final List<BTGattService> res = new ArrayList<BTGattService>();
        for(final Iterator<BTAdapter> iterA=adapters.iterator(); iterA.hasNext(); ) {
            final BTAdapter adapter = iterA.next();
            final List<BTDevice> devices = adapter.getDiscoveredDevices();
            for(final Iterator<BTDevice> iterD=devices.iterator(); iterD.hasNext(); ) {
                final BTDevice device = iterD.next();
                final List<BTGattService> devServices = device.getServices();
                if( null != devServices ) {
                    res.addAll(devServices);
                }
            }
        }
        return res;
    }

    @Override
    public boolean setDefaultAdapter(final BTAdapter adapter) {
        return false;
    }

    @Override
    public BTAdapter getDefaultAdapter() {
        for(final Iterator<BTAdapter> iter = adapters.iterator(); iter.hasNext(); ) {
            final BTAdapter a = iter.next();
            if( a.isPowered() ) {
                return a;
            }
        }
        return null;
    }

    @Override
    public boolean startDiscovery() throws BTException { return HCIStatusCode.SUCCESS == startDiscovery(true); }

    @Override
    public HCIStatusCode startDiscovery(final boolean keepAlive) throws BTException { return getDefaultAdapter().startDiscovery(keepAlive); }

    @Override
    public HCIStatusCode stopDiscovery() throws BTException { return getDefaultAdapter().stopDiscovery(); }

    @SuppressWarnings("deprecation")
    @Override
    public boolean getDiscovering() throws BTException { return getDefaultAdapter().getDiscovering(); }

    @Override
    public final void addChangedAdapterSetListener(final ChangedAdapterSetListener l) {
        changedAdapterSetListenerList.add(l);

        adapters.forEach(new Consumer<BTAdapter>() {
            @Override
            public void accept(final BTAdapter adapter) {
                l.adapterAdded(adapter);
            }
        });
    }

    @Override
    public final int removeChangedAdapterSetListener(final ChangedAdapterSetListener l) {
        // Using removeIf allows performing test on all object and remove within one write-lock cycle
        final int count[] = { 0 };
        changedAdapterSetListenerList.removeIf(new Predicate<ChangedAdapterSetListener>() {
            @Override
            public boolean test(final ChangedAdapterSetListener t) {
                if( t.equals(l) ) {
                    count[0]++;
                    return true;
                }
                return false;
            }
        });
        return count[0];
    }

    private native List<BTAdapter> getAdapterListImpl();
    private native BTAdapter getAdapterImpl(int dev_id);

    /**
     * Removal entry for DBTAdapter.close()
     * @param adapter ref to the close'ed adapter
     * @return true if contained and removed, otherwise false
     */
    /* pp */ final boolean removeAdapter(final DBTAdapter adapter) {
        if( adapters.remove(adapter) ) {
            if( DEBUG ) {
                System.err.println("DBTManager.removeAdapter: Removed "+adapter);
            }
            return true;
        } else {
            if( DEBUG ) {
                System.err.println("DBTManager.removeAdapter: Not found "+adapter);
            }
            return false;
        }
    }

    /** callback from native adapter remove */
    /* pp */ final void removeAdapterCB(final int dev_id, final int opc_reason) {
        final BTAdapter[] removed = { null };
        final int count[] = { 0 };
        adapters.removeIf(new Predicate<BTAdapter>() {
            @Override
            public boolean test(final BTAdapter a) {
                if( 0 == count[0] && dev_id == a.getDevID() ) {
                    removed[0] = a;
                    count[0]++;
                    return true;
                } else {
                    return false;
                }
            }
        });
        if( null != removed[0] ) {
            if( DEBUG ) {
                System.err.println("DBTManager.removeAdapterCB[dev_id "+dev_id+", opc 0x"+Integer.toHexString(opc_reason)+
                        "]: removing "+removed[0].toString());
            }
            if( 0x0005 == opc_reason ) {
                // MgmtEvent::Opcode::INDEX_REMOVED = 0x0005
                changedAdapterSetListenerList.forEach(new Consumer<ChangedAdapterSetListener>() {
                    @Override
                    public void accept(final ChangedAdapterSetListener t) {
                        t.adapterRemoved( removed[0] );
                    } } );
            }
            removed[0] = null; // DBTAdapter::close() issued by DBTManager after all callbacks
        }
        if( DEBUG ) {
            System.err.println("DBTManager.removeAdapterCB[dev_id "+dev_id+", opc 0x"+Integer.toHexString(opc_reason)+
                    "]: removed "+count[0]+" adapter, size "+adapters.size());
        }
    }
    /** callback from native adapter add or POWERED on */
    private final void updatedAdapterCB(final int dev_id, final int opc_reason) {
        final BTAdapter preInstance = getAdapter(dev_id);
        if( null != preInstance ) {
            if( DEBUG ) {
                System.err.println("DBTManager.updatedAdapterCB[dev_id "+dev_id+", opc 0x"+Integer.toHexString(opc_reason)+
                        "]: existing "+preInstance.toString()+", size "+adapters.size());
            }
            return;
        }
        final BTAdapter newInstance = getAdapterImpl(dev_id);
        if( null == newInstance ) {
            if( DEBUG ) {
                System.err.println("DBTManager.updatedAdapterCB[dev_id "+dev_id+", opc 0x"+Integer.toHexString(opc_reason)+
                        "]: Adapter not found, size "+adapters.size());
            }
            return;
        }
        final boolean added = adapters.add(newInstance);
        if( DEBUG ) {
            System.err.println("DBTManager.updatedAdapterCB[dev_id "+dev_id+", opc 0x"+Integer.toHexString(opc_reason)+
                    "]: added "+added+": new "+newInstance.toString()+", size "+adapters.size());
        }
        if( added && 0x0004 == opc_reason ) {
            // MgmtEvent::Opcode::INDEX_ADDED = 0x0004
            changedAdapterSetListenerList.forEach(new Consumer<ChangedAdapterSetListener>() {
                @Override
                public void accept(final ChangedAdapterSetListener t) {
                    t.adapterAdded(newInstance);
                } } );
        }
    }

    private native void initImpl(final boolean unifyUUID128Bit, final int btMode) throws BTException;
    private native void deleteImpl(long nativeInstance);
    private DBTManager()
    {
        initImpl(unifyUUID128Bit, BTFactory.DEFAULT_BTMODE.value);
        try {
            adapters.addAll(getAdapterListImpl());
        } catch (final BTException be) {
            be.printStackTrace();
        }
        final boolean supCharValCacheNotify;
        {
            final String v = System.getProperty("jau.direct_bt.characteristic.compat", "false");
            supCharValCacheNotify = Boolean.valueOf(v);
        }
        settings = new Settings() {
            @Override
            public final boolean isDirectBT() {
                return true;
            }
            @Override
            public boolean isTinyB() {
                return false;
            }
            @Override
            public boolean isCharValueCacheNotificationSupported() {
                return supCharValCacheNotify;
            }
            @Override
            public String toString() {
                return "Settings[dbt true, tinyb false, charValueCacheNotify "+isCharValueCacheNotificationSupported()+"]";
            }
        };
    }

    /** Returns an instance of BluetoothManager, to be used instead of constructor.
      * @return An initialized BluetoothManager instance.
      */
    public static BTManager getManager() throws RuntimeException, BTException {
        return LazySingletonHolder.singleton;
    }
    /** Initialize-On-Demand Holder Class, similar to C++11's "Magic Statics". */
    private static class LazySingletonHolder {
        private static final DBTManager singleton = new DBTManager();
    }

    @Override
    protected void finalize() {
        shutdown();
    }

    @Override
    public void shutdown() {
        for(final Iterator<BTAdapter> ia= adapters.iterator(); ia.hasNext(); ) {
            final DBTAdapter a = (DBTAdapter)ia.next();
            a.close();
        }
        adapters.clear();
        deleteImpl(nativeInstance);
    }

    /**
     * Returns the matching {@link DBTObject} from the internal cache if found,
     * otherwise {@code null}.
     * <p>
     * The returned {@link DBTObject} may be of type
     * <ul>
     *   <li>{@link DBTAdapter}</li>
     *   <li>{@link DBTDevice}</li>
     *   <li>{@link DBTGattService}</li>
     *   <li>{@link DBTGattChar}</li>
     *   <li>{@link DBTGattDesc}</li>
     * </ul>
     * or alternatively in {@link BTObject} space
     * <ul>
     *   <li>{@link BTType#ADAPTER} -> {@link BTAdapter}</li>
     *   <li>{@link BTType#DEVICE} -> {@link BTDevice}</li>
     *   <li>{@link BTType#GATT_SERVICE} -> {@link BTGattService}</li>
     *   <li>{@link BTType#GATT_CHARACTERISTIC} -> {@link BTGattChar}</li>
     *   <li>{@link BTType#GATT_DESCRIPTOR} -> {@link BTGattDesc}</li>
     * </ul>
     * </p>
     * @param name name of the desired {@link BTType#ADAPTER adapter} or {@link BTType#DEVICE device}.
     * Maybe {@code null}.
     * @param identifier EUI48 address of the desired {@link BTType#ADAPTER adapter} or {@link BTType#DEVICE device}
     * or UUID of the desired {@link BTType#GATT_SERVICE service},
     * {@link BTType#GATT_CHARACTERISTIC characteristic} or {@link BTType#GATT_DESCRIPTOR descriptor} to be found.
     * Maybe {@code null}, in which case the first object of the desired type is being returned - if existing.
     * @param type specify the type of the object to be found, either
     * {@link BTType#ADAPTER adapter}, {@link BTType#DEVICE device},
     * {@link BTType#GATT_SERVICE service}, {@link BTType#GATT_CHARACTERISTIC characteristic}
     * or {@link BTType#GATT_DESCRIPTOR descriptor}.
     * {@link BTType#NONE none} means anything.
     */
    /* pp */ DBTObject findInCache(final String name, final String identifier, final BTType type) {
        final boolean anyType = BTType.NONE == type;
        final boolean adapterType = BTType.ADAPTER == type;

        if( null == name && null == identifier && ( anyType || adapterType ) ) {
            // special case for 1st valid adapter
            if( adapters.size() > 0 ) {
                return (DBTAdapter) adapters.get(0);
            }
            return null; // no adapter
        }
        for(final Iterator<BTAdapter> iter = adapters.iterator(); iter.hasNext(); ) {
            final DBTAdapter adapter = (DBTAdapter) iter.next();
            if( !adapter.isValid() ) {
                continue;
            }
            if( ( anyType || adapterType ) ) {
                if( null != name && null != identifier &&
                    adapter.getName().equals(name) &&
                    adapter.getAddressString().equals(identifier)
                  )
                {
                    return adapter;
                }
                if( null != identifier &&
                    adapter.getAddressString().equals(identifier)
                  )
                {
                    return adapter;
                }
                if( null != name &&
                    adapter.getName().equals(name)
                  )
                {
                    return adapter;
                }
            }
            final DBTObject dbtObj = adapter.findInCache(name, identifier, type);
            if( null != dbtObj ) {
                return dbtObj;
            }
        }
        return null;
    }

    /* pp */ DBTObject findInCache(final DBTObject parent, final BTType type, final String name, final String identifier) {
        if( null == parent ) {
            return findInCache(name, identifier, type);
        }
        final boolean anyType = BTType.NONE == type;
        final boolean deviceType = BTType.DEVICE == type;
        final boolean serviceType = BTType.GATT_SERVICE == type;
        final boolean charType = BTType.GATT_CHARACTERISTIC== type;
        final boolean descType = BTType.GATT_DESCRIPTOR == type;

        final BTType parentType = parent.getBluetoothType();

        if( BTType.ADAPTER == parentType &&
            ( anyType || deviceType || serviceType || charType || descType )
          )
        {
            return ((DBTAdapter) parent).findInCache(name, identifier, type);
        }
        if( BTType.DEVICE == parentType &&
            ( anyType || serviceType || charType || descType )
          )
        {
            return ((DBTDevice) parent).findInCache(identifier, type);
        }
        if( BTType.GATT_SERVICE == parentType &&
            ( anyType || charType || descType )
          )
        {
            final DBTGattService service = (DBTGattService) parent;
            if( !service.checkServiceCache() ) {
                return null;
            }
            return service.findInCache(identifier, type);
        }
        if( BTType.GATT_CHARACTERISTIC == parentType &&
            ( anyType || descType )
          )
        {
            final DBTGattChar characteristic = (DBTGattChar) parent;
            if( !characteristic.checkServiceCache() ) {
                return null;
            }
            return characteristic.findInCache(identifier, type);
        }
        return null;
    }

}
