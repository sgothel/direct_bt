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

import java.security.AccessController;
import java.security.PrivilegedAction;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

import org.tinyb.BluetoothAdapter;
import org.tinyb.BluetoothDevice;
import org.tinyb.BluetoothException;
import org.tinyb.BluetoothGattService;
import org.tinyb.BluetoothObject;
import org.tinyb.BluetoothManager;
import org.tinyb.BluetoothType;

public class DBTManager implements BluetoothManager
{
    protected static final boolean DEBUG = true;

    private static volatile boolean isJVMShuttingDown = false;
    private static final List<Runnable> userShutdownHooks = new ArrayList<Runnable>();

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
            final BluetoothManager mgmt = getBluetoothManager();
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

    private long nativeInstance;
    private static DBTManager inst;
    private final List<BluetoothAdapter> adapters = new ArrayList<BluetoothAdapter>();
    private int defaultAdapterIndex = 0;

    public BluetoothType getBluetoothType() { return BluetoothType.NONE; }

    private DBTObject find(final int type, final String name, final String identifier, final BluetoothObject parent, final long milliseconds)
    { throw new UnsupportedOperationException(); } // FIXME

    @Override
    public DBTObject find(final BluetoothType type, final String name, final String identifier, final BluetoothObject parent, final long timeoutMS) {
        return find(type.ordinal(), name, identifier, parent, timeoutMS);
    }

    @Override
    public DBTObject find(final BluetoothType type, final String name, final String identifier, final BluetoothObject parent) {
        return find(type, name, identifier, parent, 0);
    }

    @SuppressWarnings("unchecked")
    @Override
    public <T extends BluetoothObject>  T find(final String name, final String identifier, final BluetoothObject parent, final long timeoutMS) {
        return (T) find(DBTObject.class_type().ordinal(), name, identifier, parent, timeoutMS);
    }

    @SuppressWarnings("unchecked")
    @Override
    public <T extends BluetoothObject>  T find(final String name, final String identifier, final BluetoothObject parent) {
        return (T) find(name, identifier, parent, 0);
    }

    @Override
    public BluetoothObject getObject(final BluetoothType type, final String name,
                                final String identifier, final BluetoothObject parent) {
        return getObject(type.ordinal(), name, identifier, parent);
    }
    private BluetoothObject getObject(final int type, final String name, final String identifier, final BluetoothObject parent)
    { throw new UnsupportedOperationException(); } // FIXME

    @Override
    public List<BluetoothObject> getObjects(final BluetoothType type, final String name,
                                    final String identifier, final BluetoothObject parent) {
        return getObjects(type.ordinal(), name, identifier, parent);
    }
    private List<BluetoothObject> getObjects(final int type, final String name, final String identifier, final BluetoothObject parent)
    { throw new UnsupportedOperationException(); } // FIXME

    @Override
    public List<BluetoothAdapter> getAdapters() { return adapters; }

    @Override
    public List<BluetoothDevice> getDevices() { return getDefaultAdapter().getDevices(); }

    @Override
    public List<BluetoothGattService> getServices() { throw new UnsupportedOperationException(); } // FIXME

    @Override
    public boolean setDefaultAdapter(final BluetoothAdapter adapter) {
        final int idx = adapters.indexOf(adapter);
        if( 0 <= idx ) {
            defaultAdapterIndex = idx;
            return true;
        }
        return false;
    }


    @Override
    public BluetoothAdapter getDefaultAdapter() { return adapters.get(defaultAdapterIndex); }

    @Override
    public boolean startDiscovery() throws BluetoothException { return getDefaultAdapter().startDiscovery(); }

    @Override
    public boolean stopDiscovery() throws BluetoothException { return getDefaultAdapter().stopDiscovery(); }

    @Override
    public boolean getDiscovering() throws BluetoothException { return getDefaultAdapter().getDiscovering(); }

    private native List<BluetoothAdapter> getAdapterListImpl();

    private native void initImpl() throws BluetoothException;
    private native void deleteImpl();
    private DBTManager()
    {
        initImpl();
        try {
            adapters.addAll(getAdapterListImpl());
        } catch (final BluetoothException be) {
            be.printStackTrace();
        }
    }

    /** Returns an instance of BluetoothManager, to be used instead of constructor.
      * @return An initialized BluetoothManager instance.
      */
    public static synchronized BluetoothManager getBluetoothManager() throws RuntimeException, BluetoothException
    {
        if (inst == null)
        {
            inst = new DBTManager();
        }
        return inst;
    }

    @Override
    protected void finalize() {
        shutdown();
    }

    @Override
    public void shutdown() {
        for(final Iterator<BluetoothAdapter> ia= adapters.iterator(); ia.hasNext(); ) {
            final BluetoothAdapter a = ia.next();
            a.close();
        }
        adapters.clear();
        deleteImpl();
    }

}
