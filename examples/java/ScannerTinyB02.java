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

import java.lang.reflect.InvocationTargetException;
import java.util.Iterator;
import java.util.List;

import org.direct_bt.AdapterSettings;
import org.direct_bt.AdapterStatusListener;
import org.direct_bt.BDAddressAndType;
import org.direct_bt.BDAddressType;
import org.direct_bt.BTAdapter;
import org.direct_bt.BTDevice;
import org.direct_bt.BTException;
import org.direct_bt.BTFactory;
import org.direct_bt.BTManager;
import org.direct_bt.BTNotification;
import org.direct_bt.BTUtils;
import org.direct_bt.EIRDataTypeSet;
import org.direct_bt.EUI48;
import org.direct_bt.HCIStatusCode;
import org.direct_bt.ScanType;

/**
 * Test and debugging application for certain situation.
 * <p>
 * Code will in 'in flux' and is not intended as an example.
 * </p>
 */
public class ScannerTinyB02 {
    static {
        System.setProperty("org.tinyb.verbose", "true");
    }
    /** 10,000 milliseconds */
    static long TO_DISCOVER = 10000;

    /** 300 milliseconds */
    static long TO_CONNECT = 300;

    static BDAddressAndType waitForDevice = BDAddressAndType.ANY_DEVICE;

    public static void main(final String[] args) throws InterruptedException {
        final boolean waitForEnter=false;
        long t0_discovery = TO_DISCOVER;
        int factory = 0;
        int dev_id = 0; // default
        int mode = 3;
        int max_loops = 5;
        boolean forever = false;
        {
            for(int i=0; i< args.length; i++) {
                final String arg = args[i];

                if( arg.equals("-dev_id") && args.length > (i+1) ) {
                    dev_id = Integer.valueOf(args[++i]).intValue();
                } else if( arg.equals("-mac") && args.length > (i+1) ) {
                    waitForDevice = new BDAddressAndType(new EUI48(args[++i]), BDAddressType.BDADDR_LE_PUBLIC);
                } else if( arg.equals("-mode") && args.length > (i+1) ) {
                    mode = Integer.valueOf(args[++i]).intValue();
                } else if( arg.equals("-factory") && args.length > (i+1) ) {
                    factory = Integer.valueOf(args[++i]).intValue();
                } else if( arg.equals("-t0_discovery") && args.length > (i+1) ) {
                    t0_discovery = Long.valueOf(args[++i]).longValue();
                } else if( arg.equals("-forever") ) {
                    forever = true;
                } else if( arg.equals("-loops") && args.length > (i+1) ) {
                    max_loops = Integer.valueOf(args[++i]).intValue();
                }
            }

            System.err.println("Run with '[-dev_id <adapter-index>] [-mac <device_address>] [-mode <mode>] [-factory <BluetoothManager-Factory-Implementation-Class>]'");
        }

        System.err.println("dev_id "+dev_id);
        System.err.println("waitForDevice: "+waitForDevice);

        if( waitForEnter ) {
            System.err.println("Press ENTER to continue\n");
            try{ System.in.read();
            } catch(final Exception e) { }
        }

        final BTFactory.ImplementationIdentifier implID = 0 == factory ? BTFactory.DirectBTImplementationID : BTFactory.DBusImplementationID;
        final BTManager manager;
        {
            BTManager _manager = null;
            try {
                _manager = BTFactory.getBTManager( implID );
            } catch (BTException | NoSuchMethodException | SecurityException
                    | IllegalAccessException | IllegalArgumentException
                    | InvocationTargetException | ClassNotFoundException e) {
                System.err.println("Unable to instantiate BluetoothManager via "+implID);
                e.printStackTrace();
                System.exit(-1);
            }
            manager = _manager;
        }
        final BTAdapter adapter;
        {
            final List<BTAdapter> adapters = manager.getAdapters();
            for(int i=0; i < adapters.size(); i++) {
                System.err.println("Adapter["+i+"]: "+adapters.get(i));
            }
            if( adapters.size() <= dev_id ) {
                System.err.println("No adapter dev_id "+dev_id+" available, adapter count "+adapters.size());
                System.exit(-1);
            }
            adapter = adapters.get(dev_id);
            if( !adapter.isPowered() ) {
                System.err.println("Adapter not enabled: device "+adapter.getName()+", address "+adapter.getAddress()+": "+adapter.toString());
                System.exit(-1);
            }
        }

        final BTDevice[] matchingDiscoveredDeviceBucket = { null };

        final AdapterStatusListener statusListener = new AdapterStatusListener() {
            @Override
            public void adapterSettingsChanged(final BTAdapter adapter, final AdapterSettings oldmask,
                                               final AdapterSettings newmask, final AdapterSettings changedmask, final long timestamp) {
                System.err.println("****** SETTINGS: "+oldmask+" -> "+newmask+", changed "+changedmask);
                System.err.println("Status Adapter:");
                System.err.println(adapter.toString());
            }

            @Override
            public void discoveringChanged(final BTAdapter adapter, final ScanType currentMeta, final ScanType changedType, final boolean changedEnabled, final boolean keepAlive, final long timestamp) {
                System.err.println("****** DISCOVERING: meta "+currentMeta+", changed["+changedType+", enabled "+changedEnabled+", keepAlive "+keepAlive+"] on "+adapter);
                System.err.println("Status Adapter:");
                System.err.println(adapter.toString());
            }

            @Override
            public boolean deviceFound(final BTDevice device, final long timestamp) {
                final boolean matches = BDAddressAndType.ANY_DEVICE.matches(waitForDevice) || device.getAddressAndType().equals(waitForDevice);
                System.err.println("****** FOUND__: "+device.toString()+" - match "+matches);
                System.err.println("Status Adapter:");
                System.err.println(device.getAdapter().toString());

                if( matches ) {
                    synchronized(matchingDiscoveredDeviceBucket) {
                        matchingDiscoveredDeviceBucket[0] = device;
                        matchingDiscoveredDeviceBucket.notifyAll();
                    }
                    return true;
                } else {
                    return false;
                }
            }

            @Override
            public void deviceUpdated(final BTDevice device, final EIRDataTypeSet updateMask, final long timestamp) {
                final boolean matches = BDAddressAndType .ANY_DEVICE.matches(waitForDevice) || device.getAddressAndType().equals(waitForDevice);
                System.err.println("****** UPDATED: "+updateMask+" of "+device+" - match "+matches);
            }

            @Override
            public void deviceConnected(final BTDevice device, final short handle, final long timestamp) {
                final boolean matches = BDAddressAndType .ANY_DEVICE.matches(waitForDevice) || device.getAddressAndType().equals(waitForDevice);
                System.err.println("****** CONNECTED: "+device+" - matches "+matches);
            }

            @Override
            public void deviceDisconnected(final BTDevice device, final HCIStatusCode reason, final short handle, final long timestamp) {
                System.err.println("****** DISCONNECTED: Reason "+reason+", old handle 0x"+Integer.toHexString(handle)+": "+device+" on "+device.getAdapter());
            }

            @Override
            public String toString() {
                return "AdapterStatusListener[user, per-adapter]";
            }
        };
        adapter.addStatusListener(statusListener);

        final long timestamp_t0 = BTUtils.currentTimeMillis();

        adapter.enableDiscoverableNotifications(new BooleanNotification("Discoverable", timestamp_t0));

        adapter.enableDiscoveringNotifications(new BooleanNotification("Discovering", timestamp_t0));

        adapter.enablePairableNotifications(new BooleanNotification("Pairable", timestamp_t0));

        adapter.enablePoweredNotifications(new BooleanNotification("Powered", timestamp_t0));

        int loop = 0;
        try {
            while( forever || loop < max_loops ) {
                loop++;
                System.err.println("****** Loop "+loop);

                final long t0 = BTUtils.currentTimeMillis();

                final boolean discoveryStarted = true; // adapter.startDiscovery(true);
                {
                    final Thread lalaTask = new Thread( new Runnable() {
                        @Override
                        public void run() {
                            adapter.startDiscovery(true);
                        }
                    }, "lala");
                    lalaTask.setDaemon(true); // detach thread
                    lalaTask.start();
                }

                System.err.println("The discovery started: " + (discoveryStarted ? "true" : "false") + " for mac "+waitForDevice+", mode "+mode);
                if( !discoveryStarted ) {
                    break;
                }
                BTDevice sensor = null;

                if( 0 == mode ) {
                    synchronized(matchingDiscoveredDeviceBucket) {
                        boolean timeout = false;
                        while( !timeout && null == matchingDiscoveredDeviceBucket[0] ) {
                            matchingDiscoveredDeviceBucket.wait(t0_discovery);
                            final long tn = BTUtils.currentTimeMillis();
                            timeout = ( tn - t0 ) > t0_discovery;
                        }
                        sensor = matchingDiscoveredDeviceBucket[0];
                        matchingDiscoveredDeviceBucket[0] = null;
                    }
                } else if( 1 == mode ) {
                    sensor = adapter.find(null, waitForDevice, t0_discovery);
                } else {
                    boolean timeout = false;
                    while( null == sensor && !timeout ) {
                        final List<BTDevice> devices = adapter.getDiscoveredDevices();
                        int i=0;
                        for(final Iterator<BTDevice> id = devices.iterator(); id.hasNext() && !timeout; ) {
                            final BTDevice d = id.next();
                            final boolean match = BDAddressAndType .ANY_DEVICE.matches(waitForDevice) || d.getAddressAndType().equals(waitForDevice);
                            System.err.println("****** Has "+i+"/"+devices.size()+": match "+match+": "+d.toString());
                            i++;
                            if( match ) {
                                sensor = d;
                                break;
                            }
                        }
                        if( null == sensor ) {
                            final long tn = BTUtils.currentTimeMillis();
                            timeout = ( tn - t0 ) > t0_discovery;
                            System.err.print(".");
                            Thread.sleep(60);
                        }
                    }
                }
                final long t1 = BTUtils.currentTimeMillis();
                if (sensor == null) {
                    System.err.println("No sensor found within "+(t1-t0)+" ms");
                    continue; // forever loop
                }
                System.err.println("Found device in "+(t1-t0)+" ms: ");
                printDevice(sensor);

                // adapter.stopDiscovery();
                {
                    final Thread lalaTask = new Thread( new Runnable() {
                        @Override
                        public void run() {
                            adapter.stopDiscovery();
                        }
                    }, "lala");
                    lalaTask.setDaemon(true); // detach thread
                    lalaTask.start();
                }

                final BooleanNotification connectedNotification = new BooleanNotification("Connected", t1);
                final BooleanNotification servicesResolvedNotification = new BooleanNotification("ServicesResolved", t1);
                sensor.enableConnectedNotifications(connectedNotification);
                sensor.enableServicesResolvedNotifications(servicesResolvedNotification);

                final long t2 = BTUtils.currentTimeMillis();
                final long t3;
                HCIStatusCode res;
                if ( (res = sensor.connect() ) == HCIStatusCode.SUCCESS ) {
                    t3 = BTUtils.currentTimeMillis();
                    System.err.println("Sensor connect issued: "+(t3-t2)+" ms, total "+(t3-t0)+" ms");
                    System.err.println("Sensor connectedNotification: "+connectedNotification.getValue());
                } else {
                    t3 = BTUtils.currentTimeMillis();
                    System.out.println("connect command failed, res "+res+": "+(t3-t2)+" ms, total "+(t3-t0)+" ms");
                    // we tolerate the failed immediate connect, as it might happen at a later time
                }

                synchronized( servicesResolvedNotification ) {
                    while( !servicesResolvedNotification.getValue() ) {
                        final long tn = BTUtils.currentTimeMillis();
                        if( tn - t3 > TO_CONNECT ) {
                            break;
                        }
                        servicesResolvedNotification.wait(100);
                    }
                }
                final long t4;
                if ( servicesResolvedNotification.getValue() ) {
                    t4 = BTUtils.currentTimeMillis();
                    System.err.println("Sensor servicesResolved: "+(t4-t3)+" ms, total "+(t4-t0)+" ms");
                } else {
                    t4 = BTUtils.currentTimeMillis();
                    System.out.println("Could not connect device: "+(t4-t3)+" ms, total "+(t4-t0)+" ms");
                    // System.exit(-1);
                }

                final BTDevice _sensor = sensor;
                final Thread lalaTask = new Thread( new Runnable() {
                    @Override
                    public void run() {
                        _sensor.disconnect();
                    }
                }, "lala");
                lalaTask.setDaemon(true); // detach thread
                lalaTask.start();

                // Thread.sleep(60);
                // sensor.connect();
                continue;
            }
        } catch (final Throwable t) {
            System.err.println("Caught: "+t.getMessage());
            t.printStackTrace();
        }

        System.err.println("ScannerTinyB01 02 clear listener etc .. ");
        adapter.removeStatusListener(statusListener);
        adapter.disableDiscoverableNotifications();
        adapter.disableDiscoveringNotifications();
        adapter.disablePairableNotifications();
        adapter.disablePoweredNotifications();

        System.err.println("ScannerTinyB01 03 close: "+adapter);
        adapter.close();
        System.err.println("ScannerTinyB01 04");
        manager.shutdown();
        System.err.println("ScannerTinyB01 XX");
    }
    private static void printDevice(final BTDevice device) {
        System.err.println("Address = " + device.getAddressAndType());
        System.err.println("  Name = " + device.getName());
        System.err.println("  Connected = " + device.getConnected());
        System.err.println();
    }
    static class BooleanNotification implements BTNotification<Boolean> {
        private final long t0;
        private final String name;
        private boolean v;

        public BooleanNotification(final String name, final long t0) {
            this.t0 = t0;
            this.name = name;
            this.v = false;
        }

        @Override
        public void run(final Boolean v) {
            synchronized(this) {
                final long t1 = BTUtils.currentTimeMillis();
                this.v = v.booleanValue();
                System.out.println("###### "+name+": "+v+" in td "+(t1-t0)+" ms!");
                this.notifyAll();
            }
        }
        public boolean getValue() {
            synchronized(this) {
                return v;
            }
        }
    }
}
