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
import java.util.ArrayList;
import java.util.Arrays;
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
import org.direct_bt.BTGattChar;
import org.direct_bt.BTGattService;
import org.direct_bt.BTManager;
import org.direct_bt.BTNotification;
import org.direct_bt.BTType;
import org.direct_bt.BTUtils;
import org.direct_bt.EIRDataTypeSet;
import org.direct_bt.EUI48;
import org.direct_bt.BTGattCharListener;
import org.direct_bt.HCIStatusCode;
import org.direct_bt.ScanType;

/**
 * This Java scanner example is a TinyB backward compatible and not fully event driven.
 * It simply polls found devices and shows certain results.
 * However, the AdapterStatusListener is attached if supported.
 * <p>
 * This example does not represent the recommended utilization of Direct-BT.
 * </p>
 */
public class ScannerTinyB01 {
    static {
        System.setProperty("org.tinyb.verbose", "true");
    }
    /** 10,000 milliseconds */
    static long TO_DISCOVER = 10000;

    /** 20000 milliseconds */
    static long TO_CONNECT_AND_RESOLVE = 20000;

    static BDAddressAndType waitForDevice = BDAddressAndType.ANY_DEVICE;
    static List<String> characteristicList = new ArrayList<String>();

    public static void main(final String[] args) throws InterruptedException {
        final boolean waitForEnter=false;
        long t0_discovery = TO_DISCOVER;
        String bluetoothManagerClazzName = BTFactory.DirectBTImplementationID.BluetoothManagerClassName;
        int dev_id = 0; // default
        int mode = 0;
        int max_loops = 1;
        boolean forever = false;
        {
            for(int i=0; i< args.length; i++) {
                final String arg = args[i];

                if( arg.equals("-dev_id") && args.length > (i+1) ) {
                    dev_id = Integer.valueOf(args[++i]).intValue();
                } else if( arg.equals("-mac") && args.length > (i+1) ) {
                    waitForDevice = new BDAddressAndType(new EUI48(args[++i]), BDAddressType.BDADDR_LE_PUBLIC);
                } else if( arg.equals("-char") && args.length > (i+1) ) {
                    characteristicList.add(args[++i]);
                } else if( arg.equals("-mode") && args.length > (i+1) ) {
                    mode = Integer.valueOf(args[++i]).intValue();
                } else if( arg.equals("-bluetoothManager") && args.length > (i+1) ) {
                    bluetoothManagerClazzName = args[++i];
                } else if( arg.equals("-t0_discovery") && args.length > (i+1) ) {
                    t0_discovery = Long.valueOf(args[++i]).longValue();
                } else if( arg.equals("-forever") ) {
                    forever = true;
                } else if( arg.equals("-loops") && args.length > (i+1) ) {
                    max_loops = Integer.valueOf(args[++i]).intValue();
                }
            }

            System.err.println("Run with '[-dev_id <adapter-index>] [-mac <device_address>] (-char <uuid>)* [-mode <mode>] [-bluetoothManager <BluetoothManager-Implementation-Class-Name>]'");
        }
        // Enable BluetoothGattCharacteristic value cache and notification compatibility using direct_bt.
        System.setProperty("direct_bt.tinyb.characteristic.compat", "true");

        System.err.println("BluetoothManager "+bluetoothManagerClazzName);
        System.err.println("dev_id "+dev_id);
        System.err.println("waitForDevice: "+waitForDevice);
        System.err.println("characteristicList: "+Arrays.toString(characteristicList.toArray()));
        System.err.println("mode "+mode);

        if( waitForEnter ) {
            System.err.println("Press ENTER to continue\n");
            try{ System.in.read();
            } catch(final Exception e) { }
        }

        final boolean isDirectBT;
        final BTManager manager;
        {
            BTManager _manager = null;
            final BTFactory.ImplementationIdentifier implID = BTFactory.getImplementationIdentifier(bluetoothManagerClazzName);
            if( null == implID ) {
                System.err.println("Unable to find BluetoothManager "+bluetoothManagerClazzName);
                System.exit(-1);
            }
            isDirectBT = BTFactory.DirectBTImplementationID.equals(implID);
            System.err.println("Using BluetoothManager "+bluetoothManagerClazzName);
            System.err.println("Using Implementation "+implID+", isDirectBT "+isDirectBT);
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
                final boolean matches = BDAddressAndType.ANY_DEVICE.matches(waitForDevice) || device.getAddressAndType().matches(waitForDevice);
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
                final boolean matches = BDAddressAndType.ANY_DEVICE.matches(waitForDevice) || device.getAddressAndType().equals(waitForDevice);
                System.err.println("****** UPDATED: "+updateMask+" of "+device+" - match "+matches);
            }

            @Override
            public void deviceConnected(final BTDevice device, final short handle, final long timestamp) {
                final boolean matches = BDAddressAndType.ANY_DEVICE.matches(waitForDevice) || device.getAddressAndType().equals(waitForDevice);
                System.err.println("****** CONNECTED: "+device+" - matches "+matches);
            }

            @Override
            public void deviceDisconnected(final BTDevice device, final HCIStatusCode reason, final short handle, final long timestamp) {
                System.err.println("****** DISCONNECTED: Reason "+reason+", old handle 0x"+Integer.toHexString(handle)+": "+device+" on "+device.getAdapter());
            }
        };
        adapter.addStatusListener(statusListener, null);

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

                final HCIStatusCode discoveryStatus = adapter.startDiscovery(true);

                System.err.println("The discovery started: " + discoveryStatus + " for mac "+waitForDevice+", mode "+mode);
                if( HCIStatusCode.SUCCESS != discoveryStatus ) {
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
                    boolean timeout = false;
                    while( null == sensor && !timeout ) {
                        sensor = adapter.find(null, waitForDevice, t0_discovery);
                        if( null == sensor ) {
                            final long tn = BTUtils.currentTimeMillis();
                            timeout = ( tn - t0 ) > t0_discovery;
                            Thread.sleep(60);
                        }
                    }
                } else {
                    boolean timeout = false;
                    while( null == sensor && !timeout ) {
                        final List<BTDevice> devices = adapter.getDiscoveredDevices();
                        for(final Iterator<BTDevice> id = devices.iterator(); id.hasNext() && !timeout; ) {
                            final BTDevice d = id.next();
                            if( BDAddressAndType.ANY_DEVICE.matches(waitForDevice) || d.getAddressAndType().equals(waitForDevice) ) {
                                sensor = d;
                                break;
                            }
                        }
                        if( null == sensor ) {
                            final long tn = BTUtils.currentTimeMillis();
                            timeout = ( tn - t0 ) > t0_discovery;
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

                adapter.stopDiscovery();

                final BooleanNotification connectedNotification = new BooleanNotification("Connected", t1);
                final BooleanNotification servicesResolvedNotification = new BooleanNotification("ServicesResolved", t1);
                sensor.enableConnectedNotifications(connectedNotification);
                sensor.enableServicesResolvedNotifications(servicesResolvedNotification);

                final long t2 = BTUtils.currentTimeMillis();
                final long t3;
                HCIStatusCode res;
                if ( ( res = sensor.connect() ) == HCIStatusCode.SUCCESS ) {
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
                        if( tn - t3 > TO_CONNECT_AND_RESOLVE ) {
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
                    System.out.println("Sensor service not resolved: "+(t4-t3)+" ms, total "+(t4-t0)+" ms");
                    System.exit(-1);
                }

                final List<BTGattService> primServices = sensor.getServices();
                if ( null == primServices || primServices.isEmpty() ) {
                    System.err.println("No BluetoothGattService found!");
                } else {
                    {
                        for(final String characteristic : characteristicList) {
                            final BTGattChar char0 = (BTGattChar)
                                    manager.find(BTType.GATT_CHARACTERISTIC, null, characteristic, null, 1000);
                            final BTGattChar char1 = (BTGattChar)
                                    manager.find(BTType.GATT_CHARACTERISTIC, null, characteristic, sensor.getAdapter(), 1000);
                            final BTGattChar char2 = (BTGattChar)
                                    manager.find(BTType.GATT_CHARACTERISTIC, null, characteristic, sensor, 1000);
                            System.err.println("Char UUID "+characteristic);
                            System.err.println("  over manager: "+char0);
                            System.err.println("  over adapter: "+char1);
                            System.err.println("  over device : "+char2);
                        }
                    }

                    final BTGattCharListener myCharacteristicListener = new BTGattCharListener(null) {
                        @Override
                        public void notificationReceived(final BTGattChar charDecl,
                                final byte[] value, final long timestamp) {
                            System.err.println("****** GATT notificationReceived: "+charDecl+
                                    ", value "+BTUtils.bytesHexString(value, 0, -1, true));
                        }

                        @Override
                        public void indicationReceived(final BTGattChar charDecl,
                                final byte[] value, final long timestamp, final boolean confirmationSent) {
                            System.err.println("****** GATT indicationReceived: "+charDecl+
                                    ", value "+BTUtils.bytesHexString(value, 0, -1, true));
                        }
                    };
                    final boolean addedCharacteristicListenerRes =
                            BTGattService.addCharListenerToAll(sensor, primServices, myCharacteristicListener);
                    System.err.println("Added GATTCharacteristicListener: "+addedCharacteristicListenerRes);

                    int i=0, j=0;
                    for(final Iterator<BTGattService> srvIter = primServices.iterator(); srvIter.hasNext(); i++) {
                        final BTGattService primService = srvIter.next();
                        System.err.printf("  [%02d] Service %s\n", i, primService.toString());
                        System.err.printf("  [%02d] Service Characteristics\n", i);
                        final List<BTGattChar> serviceCharacteristics = primService.getChars();
                        for(final Iterator<BTGattChar> charIter = serviceCharacteristics.iterator(); charIter.hasNext(); j++) {
                            final BTGattChar serviceChar = charIter.next();
                            System.err.printf("  [%02d.%02d] Decla: %s\n", i, j, serviceChar.toString());
                            final List<String> properties = Arrays.asList(serviceChar.getFlags());
                            if( properties.contains("read") ) {
                                try {
                                    final byte[] value = serviceChar.readValue();
                                    final String svalue = BTUtils.decodeUTF8String(value, 0, value.length);
                                    System.err.printf("  [%02d.%02d] Value: %s ('%s')\n",
                                            i, j, BTUtils.bytesHexString(value, 0, -1, true), svalue);
                                } catch( final Exception ex) {
                                    System.err.println("Caught "+ex.getMessage());
                                    ex.printStackTrace();
                                }
                            }
                        }
                    }
                    Thread.sleep(1000); // FIXME: Wait for notifications

                    final boolean remRes = BTGattService.removeCharListenerFromAll(sensor, primServices, myCharacteristicListener);
                    System.err.println("Removed GATTCharacteristicListener: "+remRes);
                }
                sensor.disconnect();
                System.err.println("ScannerTinyB01 04 ...: "+adapter);
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
