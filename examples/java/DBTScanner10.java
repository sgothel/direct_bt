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
import java.util.Collection;
import java.util.Collections;
import java.util.Iterator;
import java.util.List;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.function.Consumer;
import java.util.function.Predicate;

import org.tinyb.AdapterSettings;
import org.tinyb.BluetoothAdapter;
import org.tinyb.BluetoothAddressType;
import org.tinyb.BluetoothDevice;
import org.tinyb.AdapterStatusListener;
import org.tinyb.BLERandomAddressType;
import org.tinyb.BTMode;
import org.tinyb.BTSecurityLevel;
import org.tinyb.BluetoothException;
import org.tinyb.BluetoothFactory;
import org.tinyb.BluetoothGattCharacteristic;
import org.tinyb.BluetoothGattDescriptor;
import org.tinyb.BluetoothGattService;
import org.tinyb.BluetoothManager;
import org.tinyb.BluetoothNotification;
import org.tinyb.BluetoothType;
import org.tinyb.BluetoothUtils;
import org.tinyb.EIRDataTypeSet;
import org.tinyb.GATTCharacteristicListener;
import org.tinyb.HCIStatusCode;
import org.tinyb.HCIWhitelistConnectType;
import org.tinyb.PairingMode;
import org.tinyb.SMPPairingState;
import org.tinyb.ScanType;
import org.tinyb.BluetoothManager.ChangedAdapterSetListener;

import direct_bt.tinyb.DBTManager;

/**
 * This Java scanner example uses the Direct-BT fully event driven workflow
 * and adds multithreading, i.e. one thread processes each found device found
 * as notified via the event listener.
 * <p>
 * This example represents the recommended utilization of Direct-BT.
 * </p>
 */
public class DBTScanner10 {
    static final String EUI48_ANY_DEVICE = "00:00:00:00:00:00";

    final List<String> waitForDevices = new ArrayList<String>();

    static final int NO_PASSKEY = 0xffffffff;
    int pairing_passkey = NO_PASSKEY;
    BTSecurityLevel sec_level = BTSecurityLevel.UNSET;

    long timestamp_t0;

    int RESET_ADAPTER_EACH_CONN = 0;
    AtomicInteger deviceReadyCount = new AtomicInteger(0);

    AtomicInteger MULTI_MEASUREMENTS = new AtomicInteger(8);
    boolean KEEP_CONNECTED = true;
    boolean GATT_PING_ENABLED = false;
    boolean REMOVE_DEVICE = true;
    boolean USE_WHITELIST = false;
    final List<String> whitelist = new ArrayList<String>();

    String charIdentifier = null;
    int charValue = 0;

    boolean SHOW_UPDATE_EVENTS = false;
    boolean QUIET = false;

    int shutdownTest = 0;

    static void printf(final String format, final Object... args) {
        final Object[] args2 = new Object[args.length+1];
        args2[0] = BluetoothUtils.elapsedTimeMillis();
        System.arraycopy(args, 0, args2, 1, args.length);
        System.err.printf("[%,9d] "+format, args2);
        // System.err.printf("[%,9d] ", BluetoothUtils.getElapsedMillisecond());
        // System.err.printf(format, args);
    }
    static void println(final String msg) {
        System.err.printf("[%,9d] %s%s", BluetoothUtils.elapsedTimeMillis(), msg, System.lineSeparator());
    }

    static void executeOffThread(final Runnable runobj, final String threadName, final boolean detach) {
        final Thread t = new Thread( runobj, threadName );
        if( detach ) {
            t.setDaemon(true); // detach thread
        }
        t.start();
    }
    static void execute(final Runnable task, final boolean offThread) {
        if( offThread ) {
            final Thread t = new Thread(task);
            t.setDaemon(true);
            t.start();
        } else {
            task.run();
        }
    }

    Collection<String> devicesInProcessing = Collections.synchronizedCollection(new ArrayList<>());
    Collection<String> devicesProcessed = Collections.synchronizedCollection(new ArrayList<>());

    final AdapterStatusListener statusListener = new AdapterStatusListener() {
        @Override
        public void adapterSettingsChanged(final BluetoothAdapter adapter, final AdapterSettings oldmask,
                                           final AdapterSettings newmask, final AdapterSettings changedmask, final long timestamp) {
            final boolean initialSetting = oldmask.isEmpty();
            if( initialSetting ) {
                println("****** SETTINGS: "+oldmask+" -> "+newmask+", initial "+changedmask);
            } else {
                println("****** SETTINGS: "+oldmask+" -> "+newmask+", changed "+changedmask);
            }
            println("Status Adapter:");
            println(adapter.toString());
            if( !initialSetting &&
                changedmask.isSet(AdapterSettings.SettingType.POWERED) &&
                newmask.isSet(AdapterSettings.SettingType.POWERED) )
            {

                executeOffThread( () -> { startDiscovery(adapter, "powered-on"); },
                                  "DBT-StartDiscovery-"+adapter.getAddress(), true /* detach */);
            }
        }

        @Override
        public void discoveringChanged(final BluetoothAdapter adapter, final ScanType currentMeta, final ScanType changedType, final boolean changedEnabled, final boolean keepAlive, final long timestamp) {
            println("****** DISCOVERING: meta "+currentMeta+", changed["+changedType+", enabled "+changedEnabled+", keepAlive "+keepAlive+"] on "+adapter);
        }

        @Override
        public void deviceFound(final BluetoothDevice device, final long timestamp) {
            println("****** FOUND__: "+device.toString());

            if( BluetoothAddressType.BDADDR_LE_PUBLIC != device.getAddressType()
                && BLERandomAddressType.STATIC_PUBLIC != device.getBLERandomAddressType() ) {
                // Requires BREDR or LE Secure Connection support: WIP
                println("****** FOUND__-2: Skip non 'public LE' and non 'random static public LE' "+device.toString());
                return;
            }
            if( !devicesInProcessing.contains( device.getAddress() ) &&
                ( waitForDevices.isEmpty() ||
                  ( waitForDevices.contains( device.getAddress() ) &&
                    ( 0 < MULTI_MEASUREMENTS.get() || !devicesProcessed.contains( device.getAddress() ) )
                  )
                )
              )
            {
                println("****** FOUND__-0: Connecting "+device.toString());
                {
                    final long td = BluetoothUtils.currentTimeMillis() - timestamp_t0; // adapter-init -> now
                    println("PERF: adapter-init -> FOUND__-0 " + td + " ms");
                }
                executeOffThread( () -> { connectDiscoveredDevice(device); },
                                  "DBT-Connect-"+device.getAddress(), true /* detach */);
            } else {
                println("****** FOUND__-1: NOP "+device.toString());
            }
        }

        @Override
        public void deviceUpdated(final BluetoothDevice device, final EIRDataTypeSet updateMask, final long timestamp) {
            if( SHOW_UPDATE_EVENTS ) {
                println("****** UPDATED: "+updateMask+" of "+device);
            }
        }

        @Override
        public void deviceConnected(final BluetoothDevice device, final short handle, final long timestamp) {
            println("****** CONNECTED-0: "+device.toString());
        }

        @Override
        public void devicePairingState(final BluetoothDevice device, final SMPPairingState state, final PairingMode mode, final long timestamp) {
            println("****** PAIRING_STATE: state "+state+", mode "+mode+": "+device);
            switch( state ) {
                case NONE:
                    // next: deviceReady(..)
                    break;
                case FAILED:
                    // next: deviceReady() or deviceDisconnected(..)
                    break;
                case REQUESTED_BY_RESPONDER:
                    // next: FEATURE_EXCHANGE_STARTED
                    break;
                case FEATURE_EXCHANGE_STARTED:
                    // next: FEATURE_EXCHANGE_COMPLETED
                    break;
                case FEATURE_EXCHANGE_COMPLETED:
                    // next: PASSKEY_EXPECTED... or PROCESS_STARTED
                    break;
                case PASSKEY_EXPECTED: {
                    if( pairing_passkey != NO_PASSKEY ) {
                        executeOffThread( () -> { device.setPairingPasskey(pairing_passkey); }, "DBT-SetPasskey-"+device.getAddress(), true /* detach */);
                    }
                    // next: PROCESS_STARTED or FAILED
                  } break;
                case NUMERIC_COMPARE_EXPECTED: {
                    executeOffThread( () -> { device.setPairingNumericComparison(true); }, "DBT-SetNumericComp-"+device.getAddress(), true /* detach */);
                    // next: PROCESS_STARTED or FAILED
                  } break;
                case OOB_EXPECTED:
                    // FIXME: ABORT
                    break;
                case PROCESS_STARTED:
                    // next: PROCESS_COMPLETED or FAILED
                    break;
                case PROCESS_COMPLETED:
                    // next: deviceReady(..)
                    break;
                default: // nop
                    break;
            }
        }

        @Override
        public void deviceReady(final BluetoothDevice device, final long timestamp) {
            if( !devicesInProcessing.contains( device.getAddress() ) &&
                ( waitForDevices.isEmpty() ||
                  ( waitForDevices.contains( device.getAddress() ) &&
                    ( 0 < MULTI_MEASUREMENTS.get() || !devicesProcessed.contains( device.getAddress() ) )
                  )
                )
              )
            {
                deviceReadyCount.incrementAndGet();
                println("****** READY-0: Processing["+deviceReadyCount.get()+"] "+device.toString());
                {
                    final long td = BluetoothUtils.currentTimeMillis() - timestamp_t0; // adapter-init -> now
                    println("PERF: adapter-init -> READY-0 " + td + " ms");
                }
                devicesInProcessing.add(device.getAddress());
                processConnectedDevice(device); // AdapterStatusListener::deviceReady() explicitly allows prolonged and complex code execution!
            } else {
                println("****** READY-1: NOP " + device.toString());
            }
        }

        @Override
        public void deviceDisconnected(final BluetoothDevice device, final HCIStatusCode reason, final short handle, final long timestamp) {
            println("****** DISCONNECTED: Reason "+reason+", old handle 0x"+Integer.toHexString(handle)+": "+device+" on "+device.getAdapter());

            if( REMOVE_DEVICE ) {
                executeOffThread( () -> { removeDevice(device); }, "DBT-Remove-"+device.getAddress(), true /* detach */);
            } else {
                devicesInProcessing.remove(device.getAddress());
            }
            if( 0 < RESET_ADAPTER_EACH_CONN && 0 == deviceReadyCount.get() % RESET_ADAPTER_EACH_CONN ) {
                executeOffThread( () -> { resetAdapter(device.getAdapter(), 1); },
                                  "DBT-Reset-"+device.getAdapter().getAddress(), true /* detach */ );
            }
        }
    };

    private void connectDiscoveredDevice(final BluetoothDevice device) {
        println("****** Connecting Device: Start " + device.toString());
        {
            final HCIStatusCode r = device.getAdapter().stopDiscovery();
            println("****** Connecting Device: stopDiscovery result "+r);
        }
        if( BTSecurityLevel.UNSET.value < sec_level.value ) {
            device.setSecurityLevel(sec_level);
        }
        HCIStatusCode res;
        if( !USE_WHITELIST ) {
            res = device.connect();
        } else {
            res = HCIStatusCode.SUCCESS;
        }
        println("****** Connecting Device Command, res "+res+": End result "+res+" of " + device.toString());
        if( !USE_WHITELIST && 0 == devicesInProcessing.size() && HCIStatusCode.SUCCESS != res ) {
            startDiscovery(device.getAdapter(), "post-connect");
        }
    }

    void shutdownTest() {
        switch( shutdownTest ) {
            case 1:
                shutdownTest01();
                break;
            case 2:
                shutdownTest02();
                break;
            default:
                // nop
        }
    }
    void shutdownTest01() {
        execute( () -> {
            final DBTManager mngr = (DBTManager) DBTManager.getManager();
            mngr.shutdown();
        }, true);
    }
    void shutdownTest02() {
        execute( () -> {
            System.exit(1);
        }, true);
    }

    private void processConnectedDevice(final BluetoothDevice device) {
        println("****** Processing Device: Start " + device.toString());
        {
            // make sure for pending connections on failed connect*(..) command
            final HCIStatusCode r = device.getAdapter().stopDiscovery();
            println("****** Processing Device: stopDiscovery result "+r);
        }

        final long t1 = BluetoothUtils.currentTimeMillis();
        boolean success = false;

        //
        // GATT Service Processing
        //
        try {
            final List<BluetoothGattService> primServices = device.getServices(); // implicit GATT connect...
            if( null == primServices || 0 == primServices.size() ) {
                // Cheating the flow, but avoiding: goto, do-while-false and lastly unreadable intendations
                // And it is an error case nonetheless ;-)
                throw new RuntimeException("Processing Device: getServices() failed " + device.toString());
            }
            final long t5 = BluetoothUtils.currentTimeMillis();
            if( !QUIET ) {
                final long td01 = t1 - timestamp_t0; // adapter-init -> processing-start
                final long td15 = t5 - t1; // get-gatt-services
                final long tdc5 = t5 - device.getLastDiscoveryTimestamp(); // discovered to gatt-complete
                final long td05 = t5 - timestamp_t0; // adapter-init -> gatt-complete
                println(System.lineSeparator()+System.lineSeparator());
                println("PERF: GATT primary-services completed\n");
                println("PERF:  adapter-init to processing-start " + td01 + " ms,"+System.lineSeparator()+
                        "PERF:  get-gatt-services " + td15 + " ms,"+System.lineSeparator()+
                        "PERF:  discovered to gatt-complete " + tdc5 + " ms (connect " + (tdc5 - td15) + " ms),"+System.lineSeparator()+
                        "PERF:  adapter-init to gatt-complete " + td05 + " ms"+System.lineSeparator());
            }
            {
                // WIP: Implement a simple Characteristic ping-pong writeValue <-> notify transmission for stress testing.
                final BluetoothManager manager = device.getAdapter().getManager();
                if( null != charIdentifier ) {
                    final BluetoothGattCharacteristic char2 = (BluetoothGattCharacteristic)
                            manager.find(BluetoothType.GATT_CHARACTERISTIC, null, charIdentifier, device);
                    println("Char UUID "+charIdentifier);
                    println("  over device : "+char2);
                    if( null != char2 ) {
                        final GATTCharacteristicListener charPingPongListener = new GATTCharacteristicListener(null) {
                            @Override
                            public void notificationReceived(final BluetoothGattCharacteristic charDecl,
                                                             final byte[] value, final long timestamp) {
                                println("****** PingPong GATT notificationReceived: "+charDecl+
                                        ", value "+BluetoothUtils.bytesHexString(value, true, true));
                            }

                            @Override
                            public void indicationReceived(final BluetoothGattCharacteristic charDecl,
                                                           final byte[] value, final long timestamp, final boolean confirmationSent) {
                                println("****** PingPong GATT indicationReceived: "+charDecl+
                                        ", value "+BluetoothUtils.bytesHexString(value, true, true));
                            }
                        };
                        final boolean enabledState[] = { false, false };
                        final boolean addedCharPingPongListenerRes = char2.addCharacteristicListener(charPingPongListener, enabledState);
                        if( !QUIET ) {
                            println("Added CharPingPongListenerRes: "+addedCharPingPongListenerRes+", enabledState "+Arrays.toString(enabledState));
                        }
                        if( addedCharPingPongListenerRes ) {
                            final byte[] cmd = { (byte)charValue }; // request device model
                            final boolean wres = char2.writeValue(cmd, false /* withResponse */);
                            if( !QUIET ) {
                                println("Write response: "+wres);
                            }
                        }
                    }
                }
            }
            {
                final GATTCharacteristicListener myCharacteristicListener = new GATTCharacteristicListener(null) {
                    @Override
                    public void notificationReceived(final BluetoothGattCharacteristic charDecl,
                                                     final byte[] value, final long timestamp) {
                        println("****** GATT notificationReceived: "+charDecl+
                                ", value "+BluetoothUtils.bytesHexString(value, true, true));
                        shutdownTest();

                    }

                    @Override
                    public void indicationReceived(final BluetoothGattCharacteristic charDecl,
                                                   final byte[] value, final long timestamp, final boolean confirmationSent) {
                        println("****** GATT indicationReceived: "+charDecl+
                                ", value "+BluetoothUtils.bytesHexString(value, true, true));
                        shutdownTest();
                    }
                };
                final boolean addedCharacteristicListenerRes =
                  BluetoothGattService.addCharacteristicListenerToAll(device, primServices, myCharacteristicListener);
                if( !QUIET ) {
                    println("Added GATTCharacteristicListener: "+addedCharacteristicListenerRes);
                }
            }

            try {
                int i=0;
                for(final Iterator<BluetoothGattService> srvIter = primServices.iterator(); srvIter.hasNext(); i++) {
                    final BluetoothGattService primService = srvIter.next();
                    if( !QUIET ) {
                        printf("  [%02d] Service %s, uuid %s\n", i, primService.toString(), primService.getUUID());
                        printf("  [%02d] Service Characteristics\n", i);
                    }
                    int j=0;
                    final List<BluetoothGattCharacteristic> serviceCharacteristics = primService.getCharacteristics();
                    for(final Iterator<BluetoothGattCharacteristic> charIter = serviceCharacteristics.iterator(); charIter.hasNext(); j++) {
                        final BluetoothGattCharacteristic serviceChar = charIter.next();
                        if( !QUIET ) {
                            printf("  [%02d.%02d] CharDef: %s, uuid %s\n", i, j, serviceChar.toString(), serviceChar.getUUID());
                        }
                        final List<String> properties = Arrays.asList(serviceChar.getFlags());
                        if( properties.contains("read") ) {
                            final byte[] value = serviceChar.readValue();
                            final String svalue = BluetoothUtils.decodeUTF8String(value, 0, value.length);
                            if( !QUIET ) {
                                printf("  [%02d.%02d] CharVal: %s ('%s')\n",
                                        i, j, BluetoothUtils.bytesHexString(value, true, true), svalue);
                            }
                        }
                        int k=0;
                        final List<BluetoothGattDescriptor> charDescList = serviceChar.getDescriptors();
                        for(final Iterator<BluetoothGattDescriptor> descIter = charDescList.iterator(); descIter.hasNext(); k++) {
                            final BluetoothGattDescriptor charDesc = descIter.next();
                            if( !QUIET ) {
                                printf("  [%02d.%02d.%02d] Desc: %s, uuid %s\n", i, j, k, charDesc.toString(), charDesc.getUUID());
                            }
                        }
                    }
                }
            } catch( final Exception ex) {
                println("Caught "+ex.getMessage());
                ex.printStackTrace();
            }
            // FIXME sleep 1s for potential callbacks ..
            try {
                Thread.sleep(1000);
            } catch (final InterruptedException e) {
                e.printStackTrace();
            }
            success = true;
        } catch (final Throwable t ) {
            println("****** Processing Device: Exception caught for " + device.toString() + ": "+t.getMessage());
            t.printStackTrace();
        }

        if( !USE_WHITELIST && 0 == devicesInProcessing.size() ) {
            startDiscovery(device.getAdapter(), "post-processing-1");
        }

        if( KEEP_CONNECTED && GATT_PING_ENABLED && success ) {
            while( device.pingGATT() ) {
                println("****** Processing Device: pingGATT OK: "+device.getAddress());
                try {
                    Thread.sleep(1000);
                } catch (final InterruptedException e) {
                    e.printStackTrace();
                }
            }
            println("****** Processing Device: pingGATT failed, waiting for disconnect: "+device.getAddress());
            // Even w/ GATT_PING_ENABLED, we utilize disconnect event to clean up -> remove
        }

        println("****** Processing Device: End: Success " + success +
                           " on " + device.toString() + "; devInProc "+devicesInProcessing.size());
        if( success ) {
            devicesProcessed.add(device.getAddress());
        }

        if( !KEEP_CONNECTED ) {
            devicesInProcessing.remove(device.getAddress());

            device.remove();

            if( 0 < RESET_ADAPTER_EACH_CONN && 0 == deviceReadyCount.get() % RESET_ADAPTER_EACH_CONN ) {
                resetAdapter(device.getAdapter(), 2);
            } else if( !USE_WHITELIST && 0 == devicesInProcessing.size() ) {
                startDiscovery(device.getAdapter(), "post-processing-2");
            }
        }

        if( 0 < MULTI_MEASUREMENTS.get() ) {
            MULTI_MEASUREMENTS.decrementAndGet();
            println("****** Processing Device: MULTI_MEASUREMENTS left "+MULTI_MEASUREMENTS.get()+": "+device.getAddress());
        }
    }

    private void removeDevice(final BluetoothDevice device) {
        println("****** Remove Device: removing: "+device.getAddress());
        device.getAdapter().stopDiscovery();

        devicesInProcessing.remove(device.getAddress());

        device.remove();

        if( !USE_WHITELIST && 0 == devicesInProcessing.size() ) {
            startDiscovery(device.getAdapter(), "post-remove-device");
        }
    }

    private void resetAdapter(final BluetoothAdapter adapter, final int mode) {
        println("****** Reset Adapter: reset["+mode+"] start: "+adapter.toString());
        final HCIStatusCode res = adapter.reset();
        println("****** Reset Adapter: reset["+mode+"] end: "+res+", "+adapter.toString());
    }

    private boolean startDiscovery(final BluetoothAdapter adapter, final String msg) {
        final HCIStatusCode status = adapter.startDiscovery( true );
        println("****** Start discovery ("+msg+") result: "+status);
        return HCIStatusCode.SUCCESS == status;
    }

    private boolean initAdapter(final BluetoothAdapter adapter) {
        if( !adapter.isPowered() ) { // should have been covered above
            println("Adapter not powered (2): "+adapter.toString());
            return false;
        }
        println("Using adapter: "+adapter.toString());

        adapter.addStatusListener(statusListener, null);
        adapter.enableDiscoverableNotifications(new BooleanNotification("Discoverable", timestamp_t0));

        adapter.enableDiscoveringNotifications(new BooleanNotification("Discovering", timestamp_t0));

        adapter.enablePairableNotifications(new BooleanNotification("Pairable", timestamp_t0));

        adapter.enablePoweredNotifications(new BooleanNotification("Powered", timestamp_t0));

        if( USE_WHITELIST ) {
            for(final Iterator<String> wliter = whitelist.iterator(); wliter.hasNext(); ) {
                final String addr = wliter.next();
                final boolean res = adapter.addDeviceToWhitelist(addr, BluetoothAddressType.BDADDR_LE_PUBLIC, HCIWhitelistConnectType.HCI_AUTO_CONN_ALWAYS);
                println("Added to whitelist: res "+res+", address "+addr);
            }
        } else {
            if( !startDiscovery(adapter, "kick-off") ) {
                return false;
            }
        }
        return true;
    }

    private final List<BluetoothAdapter> adapters = new CopyOnWriteArrayList<BluetoothAdapter>();

    @SuppressWarnings("unused")
    private BluetoothAdapter getAdapter(final int dev_id) {
        for( int i=0; i<adapters.size(); i++) {
            final BluetoothAdapter a = adapters.get(i);
            if( dev_id == a.getDevID() ) {
                return a;
            }
        }
        return null;
    }

    private int removeAdapter(final int dev_id) {
        // Using removeIf allows performing test on all object and remove within one write-lock cycle
        final int count[] = { 0 };
        adapters.removeIf(new Predicate<BluetoothAdapter>() {
            @Override
            public boolean test(final BluetoothAdapter t) {
                if( dev_id == t.getDevID() ) {
                    count[0]++;
                    return true;
                }
                return false;
            }
        });
        return count[0];
    }

    private final BluetoothManager.ChangedAdapterSetListener myChangedAdapterSetListener =
            new BluetoothManager.ChangedAdapterSetListener() {
                @Override
                public void adapterAdded(final BluetoothAdapter adapter) {
                    if( adapter.isPowered() ) {
                        if( initAdapter( adapter ) ) {
                            adapters.add(adapter);
                            println("****** Adapter ADDED__: NewInit " + adapter);
                        } else {
                            println("****** Adapter ADDED__: Failed_ " + adapter);
                        }
                    } else {
                        println("****** Adapter ADDED__: Ignored " + adapter);
                    }
                }

                @Override
                public void adapterRemoved(final BluetoothAdapter adapter) {
                    final int count = removeAdapter(adapter.getDevID());
                    println("****** Adapter REMOVED: count "+count+", " + adapter);
                }
    };

    public void runTest(final BluetoothManager manager) {
        timestamp_t0 = BluetoothUtils.currentTimeMillis();

        boolean done = false;

        manager.addChangedAdapterSetListener(myChangedAdapterSetListener);

        while( !done ) {
            if( 0 == MULTI_MEASUREMENTS.get() ||
                ( -1 == MULTI_MEASUREMENTS.get() && !waitForDevices.isEmpty() && devicesProcessed.containsAll(waitForDevices) )
              )
            {
                println("****** EOL Test MULTI_MEASUREMENTS left "+MULTI_MEASUREMENTS.get()+
                                   ", processed "+devicesProcessed.size()+"/"+waitForDevices.size());
                println("****** WaitForDevices "+Arrays.toString(waitForDevices.toArray()));
                println("****** DevicesProcessed "+Arrays.toString(devicesProcessed.toArray()));
                done = true;
            } else {
                try {
                    Thread.sleep(2000);
                } catch (final InterruptedException e) {
                    e.printStackTrace();
                }
            }
        }

        adapters.forEach(new Consumer<BluetoothAdapter>() {
            @Override
            public void accept(final BluetoothAdapter a) {
                println("****** EOL Adapter's Devices - pre_ close: " + a);
                a.close();
                println("****** EOL Adapter's Devices - post close: " + a);
            } } );

        {
            final int count = manager.removeChangedAdapterSetListener(myChangedAdapterSetListener);
            println("****** EOL Removed ChangedAdapterSetCallback " + count);
        }

        // All implicit via destructor or shutdown hook!
        // manager.shutdown(); /* implies: adapter.close(); */
    }

    public static void main(final String[] args) throws InterruptedException {
        for(int i=0; i< args.length; i++) {
            final String arg = args[i];
            if( arg.equals("-debug") ) {
                System.setProperty("org.tinyb.verbose", "true");
                System.setProperty("org.tinyb.debug", "true");
            } else if( arg.equals("-verbose") ) {
                System.setProperty("org.tinyb.verbose", "true");
            } else if( arg.equals("-dbt_debug") && args.length > (i+1) ) {
                System.setProperty("direct_bt.debug", args[++i]);
            } else if( arg.equals("-dbt_verbose") && args.length > (i+1) ) {
                System.setProperty("direct_bt.verbose", args[++i]);
            } else if( arg.equals("-dbt_gatt") && args.length > (i+1) ) {
                System.setProperty("direct_bt.gatt", args[++i]);
            } else if( arg.equals("-dbt_l2cap") && args.length > (i+1) ) {
                System.setProperty("direct_bt.l2cap", args[++i]);
            } else if( arg.equals("-dbt_hci") && args.length > (i+1) ) {
                System.setProperty("direct_bt.hci", args[++i]);
            } else if( arg.equals("-dbt_mgmt") && args.length > (i+1) ) {
                System.setProperty("direct_bt.mgmt", args[++i]);
            } else if( arg.equals("-btmode") && args.length > (i+1) ) {
                final BTMode btmode = BTMode.get(args[++i]);
                System.setProperty("org.tinyb.btmode", btmode.toString());
                System.err.println("Setting 'org.tinyb.btmode' to "+btmode.toString());
            }
        }
        final BluetoothManager manager;
        try {
            manager = BluetoothFactory.getDirectBTBluetoothManager();
        } catch (BluetoothException | NoSuchMethodException | SecurityException
                | IllegalAccessException | IllegalArgumentException
                | InvocationTargetException | ClassNotFoundException e) {
            System.err.println("Unable to instantiate DirectBT BluetoothManager");
            e.printStackTrace();
            System.exit(-1);
            return;
        }
        println("DirectBT BluetoothManager initialized!");
        final DBTScanner10 test = new DBTScanner10();

        boolean waitForEnter=false;
        {
            for(int i=0; i< args.length; i++) {
                final String arg = args[i];

                if( arg.equals("-wait") ) {
                    waitForEnter = true;
                } else if( arg.equals("-show_update_events") ) {
                    test.SHOW_UPDATE_EVENTS = true;
                } else if( arg.equals("-quiet") ) {
                    test.QUIET = true;
                } else if( arg.equals("-shutdown") && args.length > (i+1) ) {
                    test.shutdownTest = Integer.valueOf(args[++i]).intValue();
                } else if( arg.equals("-mac") && args.length > (i+1) ) {
                    test.waitForDevices.add(args[++i]);
                } else if( arg.equals("-wl") && args.length > (i+1) ) {
                    final String addr = args[++i];
                    println("Whitelist + "+addr);
                    test.whitelist.add(addr);
                    test.USE_WHITELIST = true;
                } else if( arg.equals("-passkey") && args.length > (i+1) ) {
                    test.pairing_passkey = Integer.valueOf(args[++i]).intValue();
                } else if( arg.equals("-seclevel") && args.length > (i+1) ) {
                    test.sec_level = BTSecurityLevel.get( (byte)Integer.valueOf(args[++i]).intValue() );
                } else if( arg.equals("-charid") && args.length > (i+1) ) {
                    test.charIdentifier = args[++i];
                } else if( arg.equals("-charval") && args.length > (i+1) ) {
                    test.charValue = Integer.valueOf(args[++i]).intValue();
                } else if( arg.equals("-disconnect") ) {
                    test.KEEP_CONNECTED = false;
                } else if( arg.equals("-enableGATTPing") ) {
                    test.GATT_PING_ENABLED = true;
                } else if( arg.equals("-keepDevice") ) {
                    test.REMOVE_DEVICE = false;
                } else if( arg.equals("-count")  && args.length > (i+1) ) {
                    test.MULTI_MEASUREMENTS.set(Integer.valueOf(args[++i]).intValue());
                } else if( arg.equals("-single") ) {
                    test.MULTI_MEASUREMENTS.set(-1);
                } else if( arg.equals("-resetEachCon")  && args.length > (i+1) ) {
                    test.RESET_ADAPTER_EACH_CONN = Integer.valueOf(args[++i]).intValue();
                }
            }
            println("Run with '[-btmode LE|BREDR|DUAL] "+
                    "[-bluetoothManager <BluetoothManager-Implementation-Class-Name>] "+
                    "[-disconnect] [-enableGATTPing] [-count <number>] [-single] [-show_update_events] [-quiet]  "+
                    "[-resetEachCon connectionCount] "+
                    "(-mac <device_address>)* (-wl <device_address>)* "+
                    "[-passkey <digits>]  [-seclevel <int>]" +
                    "[-charid <uuid>] [-charval <byte-val>]"+
                    "[-verbose] [-debug] "+
                    "[-dbt_verbose true|false] "+
                    "[-dbt_debug true|false|adapter.event,gatt.data,hci.event,mgmt.event] "+
                    "[-dbt_mgmt cmd.timeout=3000,ringsize=64,...] "+
                    "[-dbt_hci cmd.complete.timeout=10000,cmd.status.timeout=3000,ringsize=64,...] "+
                    "[-dbt_gatt cmd.read.timeout=500,cmd.write.timeout=500,cmd.init.timeout=2500,ringsize=128,...] "+
                    "[-dbt_l2cap reader.timeout=10000,restart.count=0,...] "+
                    "[-shutdown <int>]'");
        }

        println("MULTI_MEASUREMENTS "+test.MULTI_MEASUREMENTS.get());
        println("KEEP_CONNECTED "+test.KEEP_CONNECTED);
        println("RESET_ADAPTER_EACH_CONN "+test.RESET_ADAPTER_EACH_CONN);
        println("GATT_PING_ENABLED "+test.GATT_PING_ENABLED);
        println("REMOVE_DEVICE "+test.REMOVE_DEVICE);
        println("USE_WHITELIST "+test.USE_WHITELIST);
        println("SHOW_UPDATE_EVENTS "+test.SHOW_UPDATE_EVENTS);
        println("QUIET "+test.QUIET);
        println("passkey "+test.pairing_passkey);
        println("seclevel "+test.sec_level);
        println("characteristic-id: "+test.charIdentifier);
        println("characteristic-value: "+test.charValue);

        println("waitForDevice: "+Arrays.toString(test.waitForDevices.toArray()));

        if( waitForEnter ) {
            println("Press ENTER to continue\n");
            try{ System.in.read();
            } catch(final Exception e) { }
        }
        test.runTest(manager);
    }

    static class BooleanNotification implements BluetoothNotification<Boolean> {
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
                final long t1 = BluetoothUtils.currentTimeMillis();
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
