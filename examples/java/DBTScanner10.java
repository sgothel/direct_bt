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

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.reflect.InvocationTargetException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.function.Consumer;
import java.util.function.Predicate;

import org.direct_bt.AdapterSettings;
import org.direct_bt.AdapterStatusListener;
import org.direct_bt.BDAddressAndType;
import org.direct_bt.BDAddressType;
import org.direct_bt.BLERandomAddressType;
import org.direct_bt.BTMode;
import org.direct_bt.BTSecurityLevel;
import org.direct_bt.BTAdapter;
import org.direct_bt.BTDevice;
import org.direct_bt.BTException;
import org.direct_bt.BTFactory;
import org.direct_bt.BTGattChar;
import org.direct_bt.BTGattDesc;
import org.direct_bt.BTGattService;
import org.direct_bt.BTManager;
import org.direct_bt.BTNotification;
import org.direct_bt.BTType;
import org.direct_bt.BTUtils;
import org.direct_bt.EIRDataTypeSet;
import org.direct_bt.EUI48;
import org.direct_bt.BTGattCharListener;
import org.direct_bt.HCIStatusCode;
import org.direct_bt.HCIWhitelistConnectType;
import org.direct_bt.PairingMode;
import org.direct_bt.SMPIOCapability;
import org.direct_bt.SMPKeyMask;
import org.direct_bt.SMPLongTermKeyInfo;
import org.direct_bt.SMPPairingState;
import org.direct_bt.SMPSignatureResolvingKeyInfo;
import org.direct_bt.ScanType;

import jau.direct_bt.DBTManager;

/**
 * This Java scanner example uses the Direct-BT fully event driven workflow
 * and adds multithreading, i.e. one thread processes each found device found
 * as notified via the event listener.
 * <p>
 * This example represents the recommended utilization of Direct-BT.
 * </p>
 */
public class DBTScanner10 {
    final List<BDAddressAndType> waitForDevices = new ArrayList<BDAddressAndType>();

    static final int NO_PASSKEY = -1;
    int pairing_passkey = NO_PASSKEY;
    BTSecurityLevel sec_level = BTSecurityLevel.UNSET;
    SMPIOCapability io_capabilities = SMPIOCapability.UNSET;

    long timestamp_t0;

    int RESET_ADAPTER_EACH_CONN = 0;
    AtomicInteger deviceReadyCount = new AtomicInteger(0);

    AtomicInteger MULTI_MEASUREMENTS = new AtomicInteger(8);
    boolean KEEP_CONNECTED = true;
    boolean UNPAIR_DEVICE_PRE = false;
    boolean UNPAIR_DEVICE_POST = false;

    boolean GATT_PING_ENABLED = false;
    boolean REMOVE_DEVICE = true;
    boolean USE_WHITELIST = false;
    final List<BDAddressAndType> whitelist = new ArrayList<BDAddressAndType>();

    String charIdentifier = null;
    int charValue = 0;

    boolean SHOW_UPDATE_EVENTS = false;
    boolean QUIET = false;

    int shutdownTest = 0;

    boolean matches(final List<BDAddressAndType> cont, final BDAddressAndType mac) {
        for(final Iterator<BDAddressAndType> it = cont.iterator(); it.hasNext(); ) {
            if( it.next().matches(mac) ) {
                return true;
            }
        }
        return false;
    }

    static void printf(final String format, final Object... args) {
        final Object[] args2 = new Object[args.length+1];
        args2[0] = BTUtils.elapsedTimeMillis();
        System.arraycopy(args, 0, args2, 1, args.length);
        System.err.printf("[%,9d] "+format, args2);
        // System.err.printf("[%,9d] ", BluetoothUtils.getElapsedMillisecond());
        // System.err.printf(format, args);
    }
    static void println(final String msg) {
        System.err.printf("[%,9d] %s%s", BTUtils.elapsedTimeMillis(), msg, System.lineSeparator());
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

    static public class MyLongTermKeyInfo {
        BDAddressAndType address_and_type;
        SMPLongTermKeyInfo smp_ltk;

        MyLongTermKeyInfo(final BDAddressAndType address_and_type, final SMPLongTermKeyInfo smp_ltk) {
            this.address_and_type = address_and_type;
            this.smp_ltk = smp_ltk;
        }
        MyLongTermKeyInfo() {
            address_and_type = new BDAddressAndType();
            smp_ltk = new SMPLongTermKeyInfo();
        }

        boolean write(final String filename) {
            if( !smp_ltk.isValid() ) {
                println("****** WRITE LTK ["+address_and_type+", invalid, skipped]: "+smp_ltk+" (write)");
                return false;
            }
            final File file = new File(filename);
            OutputStream out = null;
            try {
                file.delete(); // alternative to truncate, if existing
                out = new FileOutputStream(file);
                out.write(address_and_type.address.b);
                out.write(address_and_type.type.value);
                final byte[] smp_ltk_b = new byte[SMPLongTermKeyInfo.byte_size];
                smp_ltk.getStream(smp_ltk_b, 0);
                out.write(smp_ltk_b);
                println("****** WRITE LTK ["+address_and_type+", valid, written]: "+smp_ltk);
                return true;
            } catch (final Exception ex) {
                ex.printStackTrace();
            } finally {
                try {
                    if( null != out ) {
                        out.close();
                    }
                } catch (final IOException e) {
                    e.printStackTrace();
                }
            }
            return false;
        }

        boolean read(final String filename) {
            final File file = new File(filename);
            InputStream in = null;
            try {
                final byte[] buffer = new byte[6 + 1 + SMPLongTermKeyInfo.byte_size];
                in = new FileInputStream(file);
                final int read_count = in.read(buffer, 0, buffer.length);
                if( read_count != buffer.length ) {
                    throw new IOException("Couldn't read "+buffer.length+" bytes, only "+read_count+" from "+filename);
                }
                address_and_type.address.putStream(buffer, 0);
                address_and_type.type = BDAddressType.get(buffer[6]);
                smp_ltk.putStream(buffer, 6+1);
                println("****** READ LTK ["+address_and_type+", valid "+smp_ltk.isValid()+"]: "+smp_ltk);
                return smp_ltk.isValid();
            } catch (final Exception ex) {
                ex.printStackTrace();
            } finally {
                try {
                    if( null != in ) {
                        in.close();
                    }
                } catch (final IOException e) {
                    e.printStackTrace();
                }
            }
            return false;
        }
    }
    static public class MySignatureResolvingKeyInfo {
        BDAddressAndType address_and_type;
        SMPSignatureResolvingKeyInfo smp_csrk;

        MySignatureResolvingKeyInfo(final BDAddressAndType address_and_type, final SMPSignatureResolvingKeyInfo smp_csrk) {
            this.address_and_type = address_and_type;
            this.smp_csrk = smp_csrk;
        }
        MySignatureResolvingKeyInfo() {
            address_and_type = new BDAddressAndType();
            smp_csrk = new SMPSignatureResolvingKeyInfo();
        }

        boolean write(final String filename) {
            final File file = new File(filename);
            OutputStream out = null;
            try {
                file.delete(); // alternative to truncate, if existing
                out = new FileOutputStream(file);
                out.write(address_and_type.address.b);
                out.write(address_and_type.type.value);
                final byte[] smp_ltk_b = new byte[SMPSignatureResolvingKeyInfo.byte_size];
                smp_csrk.getStream(smp_ltk_b, 0);
                out.write(smp_ltk_b);
                println("****** WRITE CSRK ["+address_and_type+", written]: "+smp_csrk);
                return true;
            } catch (final Exception ex) {
                ex.printStackTrace();
            } finally {
                try {
                    if( null != out ) {
                        out.close();
                    }
                } catch (final IOException e) {
                    e.printStackTrace();
                }
            }
            return false;
        }

        boolean read(final String filename) {
            final File file = new File(filename);
            InputStream in = null;
            try {
                final byte[] buffer = new byte[6 + 1 + SMPSignatureResolvingKeyInfo.byte_size];
                in = new FileInputStream(file);
                final int read_count = in.read(buffer, 0, buffer.length);
                if( read_count != buffer.length ) {
                    throw new IOException("Couldn't read "+buffer.length+" bytes, only "+read_count+" from "+filename);
                }
                address_and_type.address.putStream(buffer, 0);
                address_and_type.type = BDAddressType.get(buffer[6]);
                smp_csrk.putStream(buffer, 6+1);
                println("****** READ CSRK ["+address_and_type+"]: "+smp_csrk);
                return true;
            } catch (final Exception ex) {
                ex.printStackTrace();
            } finally {
                try {
                    if( null != in ) {
                        in.close();
                    }
                } catch (final IOException e) {
                    e.printStackTrace();
                }
            }
            return false;
        }
    }
    Collection<BDAddressAndType> devicesInProcessing = Collections.synchronizedCollection(new HashSet<>());
    Collection<BDAddressAndType> devicesProcessed = Collections.synchronizedCollection(new HashSet<>());

    final AdapterStatusListener statusListener = new AdapterStatusListener() {
        @Override
        public void adapterSettingsChanged(final BTAdapter adapter, final AdapterSettings oldmask,
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
        public void discoveringChanged(final BTAdapter adapter, final ScanType currentMeta, final ScanType changedType, final boolean changedEnabled, final boolean keepAlive, final long timestamp) {
            println("****** DISCOVERING: meta "+currentMeta+", changed["+changedType+", enabled "+changedEnabled+", keepAlive "+keepAlive+"] on "+adapter);
        }

        @Override
        public boolean deviceFound(final BTDevice device, final long timestamp) {
            println("****** FOUND__: "+device.toString());

            if( BDAddressType.BDADDR_LE_PUBLIC != device.getAddressAndType().type
                && BLERandomAddressType.STATIC_PUBLIC != device.getAddressAndType().getBLERandomAddressType() ) {
                // Requires BREDR or LE Secure Connection support: WIP
                println("****** FOUND__-2: Skip non 'public LE' and non 'random static public LE' "+device.toString());
                return false;
            }
            if( !devicesInProcessing.contains( device.getAddressAndType() ) &&
                ( waitForDevices.isEmpty() ||
                  ( matches(waitForDevices, device.getAddressAndType() ) &&
                    ( 0 < MULTI_MEASUREMENTS.get() || !devicesProcessed.contains( device.getAddressAndType() ) )
                  )
                )
              )
            {
                println("****** FOUND__-0: Connecting "+device.toString());
                {
                    final long td = BTUtils.currentTimeMillis() - timestamp_t0; // adapter-init -> now
                    println("PERF: adapter-init -> FOUND__-0 " + td + " ms");
                }
                executeOffThread( () -> { connectDiscoveredDevice(device); },
                                  "DBT-Connect-"+device.getAddressAndType(), true /* detach */);
                return true;
            } else {
                println("****** FOUND__-1: NOP "+device.toString());
                return false;
            }
        }

        @Override
        public void deviceUpdated(final BTDevice device, final EIRDataTypeSet updateMask, final long timestamp) {
            if( SHOW_UPDATE_EVENTS ) {
                println("****** UPDATED: "+updateMask+" of "+device);
            }
        }

        @Override
        public void deviceConnected(final BTDevice device, final short handle, final long timestamp) {
            println("****** CONNECTED-0: "+device.toString());
        }

        @Override
        public void devicePairingState(final BTDevice device, final SMPPairingState state, final PairingMode mode, final long timestamp) {
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
                    // next: PASSKEY_EXPECTED... or KEY_DISTRIBUTION
                    break;
                case PASSKEY_EXPECTED: {
                    if( pairing_passkey != NO_PASSKEY ) {
                        executeOffThread( () -> { device.setPairingPasskey(pairing_passkey); }, "DBT-SetPasskey-"+device.getAddressAndType(), true /* detach */);
                    }
                    // next: KEY_DISTRIBUTION or FAILED
                  } break;
                case NUMERIC_COMPARE_EXPECTED: {
                    executeOffThread( () -> { device.setPairingNumericComparison(true); }, "DBT-SetNumericComp-"+device.getAddressAndType(), true /* detach */);
                    // next: KEY_DISTRIBUTION or FAILED
                  } break;
                case OOB_EXPECTED:
                    // FIXME: ABORT
                    break;
                case KEY_DISTRIBUTION:
                    // next: COMPLETED or FAILED
                    break;
                case COMPLETED:
                    // next: deviceReady(..)
                    break;
                default: // nop
                    break;
            }
        }

        @Override
        public void deviceReady(final BTDevice device, final long timestamp) {
            if( !devicesInProcessing.contains( device.getAddressAndType() ) &&
                ( waitForDevices.isEmpty() ||
                  ( matches(waitForDevices, device.getAddressAndType() ) &&
                    ( 0 < MULTI_MEASUREMENTS.get() || !devicesProcessed.contains( device.getAddressAndType() ) )
                  )
                )
              )
            {
                deviceReadyCount.incrementAndGet();
                println("****** READY-0: Processing["+deviceReadyCount.get()+"] "+device.toString());
                {
                    final long td = BTUtils.currentTimeMillis() - timestamp_t0; // adapter-init -> now
                    println("PERF: adapter-init -> READY-0 " + td + " ms");
                }
                devicesInProcessing.add(device.getAddressAndType());
                processReadyDevice(device); // AdapterStatusListener::deviceReady() explicitly allows prolonged and complex code execution!
            } else {
                println("****** READY-1: NOP " + device.toString());
            }
        }

        @Override
        public void deviceDisconnected(final BTDevice device, final HCIStatusCode reason, final short handle, final long timestamp) {
            println("****** DISCONNECTED: Reason "+reason+", old handle 0x"+Integer.toHexString(handle)+": "+device+" on "+device.getAdapter());

            if( REMOVE_DEVICE ) {
                executeOffThread( () -> { removeDevice(device); }, "DBT-Remove-"+device.getAddressAndType(), true /* detach */);
            } else {
                devicesInProcessing.remove(device.getAddressAndType());
            }
            if( 0 < RESET_ADAPTER_EACH_CONN && 0 == deviceReadyCount.get() % RESET_ADAPTER_EACH_CONN ) {
                executeOffThread( () -> { resetAdapter(device.getAdapter(), 1); },
                                  "DBT-Reset-"+device.getAdapter().getAddress(), true /* detach */ );
            }
        }
    };

    private void connectDiscoveredDevice(final BTDevice device) {
        println("****** Connecting Device: Start " + device.toString());

        if( UNPAIR_DEVICE_PRE ) {
            final HCIStatusCode unpair_res = device.unpair();
            println("****** Connecting Device: Unpair-Pre result: "+unpair_res);
        }

        {
            final HCIStatusCode r = device.getAdapter().stopDiscovery();
            println("****** Connecting Device: stopDiscovery result "+r);
        }

        {
            final MyLongTermKeyInfo my_ltk_resp = new MyLongTermKeyInfo();
            final MyLongTermKeyInfo my_ltk_init = new MyLongTermKeyInfo();
            if( my_ltk_init.read(device.getAddressAndType().toString()+".init.ltk") &&
                my_ltk_resp.read(device.getAddressAndType().toString()+".resp.ltk") &&
                HCIStatusCode.SUCCESS == device.setLongTermKeyInfo(my_ltk_init.smp_ltk) &&
                HCIStatusCode.SUCCESS == device.setLongTermKeyInfo(my_ltk_resp.smp_ltk) ) {
                println("****** Connecting Device: Loaded LTKs from file successfully\n");
            } else {
                println("****** Connecting Device: Error loading LTKs from file\n");
            }
        }
        device.setConnSecurityBest(sec_level, io_capabilities);

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

    private void processReadyDevice(final BTDevice device) {
        println("****** Processing Ready Device: Start " + device.toString());
        {
            // make sure for pending connections on failed connect*(..) command
            final HCIStatusCode r = device.getAdapter().stopDiscovery();
            println("****** Processing Ready Device: stopDiscovery result "+r);
        }

        final long t1 = BTUtils.currentTimeMillis();
        boolean success = false;

        {
            final SMPPairingState pstate = device.getPairingState();
            final PairingMode pmode = device.getPairingMode(); // Skip PairingMode::PRE_PAIRED (write again)
            if( SMPPairingState.COMPLETED == pstate && PairingMode.PRE_PAIRED != pmode ) {
                final SMPKeyMask keys_resp = device.getAvailableSMPKeys(true /* responder */);
                final SMPKeyMask keys_init = device.getAvailableSMPKeys(false /* responder */);

                if( keys_init.isSet(SMPKeyMask.KeyType.ENC_KEY) ) {
                    final MyLongTermKeyInfo my_ltk = new MyLongTermKeyInfo(device.getAddressAndType(),
                                                                           device.getLongTermKeyInfo(false /* responder */));
                    my_ltk.write(my_ltk.address_and_type.toString()+".init.ltk");
                }
                if( keys_resp.isSet(SMPKeyMask.KeyType.ENC_KEY) ) {
                    final MyLongTermKeyInfo my_ltk = new MyLongTermKeyInfo(device.getAddressAndType(),
                                                                           device.getLongTermKeyInfo(true /* responder */));
                    my_ltk.write(my_ltk.address_and_type.toString()+".resp.ltk");
                }

                if( keys_init.isSet(SMPKeyMask.KeyType.SIGN_KEY) ) {
                    final MySignatureResolvingKeyInfo my_csrk = new MySignatureResolvingKeyInfo(device.getAddressAndType(),
                                                                                               device.getSignatureResolvingKeyInfo(false /* responder */));
                    my_csrk.write(my_csrk.address_and_type.toString()+".init.csrk");
                }
                if( keys_resp.isSet(SMPKeyMask.KeyType.SIGN_KEY) ) {
                    final MySignatureResolvingKeyInfo my_csrk = new MySignatureResolvingKeyInfo(device.getAddressAndType(),
                                                                                               device.getSignatureResolvingKeyInfo(true /* responder */));
                    my_csrk.write(my_csrk.address_and_type.toString()+".resp.csrk");
                }
            }
        }

        //
        // GATT Service Processing
        //
        try {
            final List<BTGattService> primServices = device.getServices(); // implicit GATT connect...
            if( null == primServices || 0 == primServices.size() ) {
                // Cheating the flow, but avoiding: goto, do-while-false and lastly unreadable intendations
                // And it is an error case nonetheless ;-)
                throw new RuntimeException("Processing Ready Device: getServices() failed " + device.toString());
            }
            final long t5 = BTUtils.currentTimeMillis();
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
                final BTManager manager = device.getAdapter().getManager();
                if( null != charIdentifier && charIdentifier.length() > 0 ) {
                    final BTGattChar char2 = (BTGattChar)
                            manager.find(BTType.GATT_CHARACTERISTIC, null, charIdentifier, device);
                    println("Char UUID "+charIdentifier);
                    println("  over device : "+char2);
                    if( null != char2 ) {
                        final BTGattCharListener charPingPongListener = new BTGattCharListener(null) {
                            @Override
                            public void notificationReceived(final BTGattChar charDecl,
                                                             final byte[] value, final long timestamp) {
                                println("****** PingPong GATT notificationReceived: "+charDecl+
                                        ", value "+BTUtils.bytesHexString(value, 0, -1, true, true, true));
                            }

                            @Override
                            public void indicationReceived(final BTGattChar charDecl,
                                                           final byte[] value, final long timestamp, final boolean confirmationSent) {
                                println("****** PingPong GATT indicationReceived: "+charDecl+
                                        ", value "+BTUtils.bytesHexString(value, 0, -1, true, true, true));
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
                final BTGattCharListener myCharacteristicListener = new BTGattCharListener(null) {
                    @Override
                    public void notificationReceived(final BTGattChar charDecl,
                                                     final byte[] value, final long timestamp) {
                        println("****** GATT notificationReceived: "+charDecl+
                                ", value "+BTUtils.bytesHexString(value, 0, -1, true, true, true));
                        shutdownTest();

                    }

                    @Override
                    public void indicationReceived(final BTGattChar charDecl,
                                                   final byte[] value, final long timestamp, final boolean confirmationSent) {
                        println("****** GATT indicationReceived: "+charDecl+
                                ", value "+BTUtils.bytesHexString(value, 0, -1, true, true, true));
                        shutdownTest();
                    }
                };
                final boolean addedCharacteristicListenerRes =
                  BTGattService.addCharListenerToAll(device, primServices, myCharacteristicListener);
                if( !QUIET ) {
                    println("Added GATTCharacteristicListener: "+addedCharacteristicListenerRes);
                }
            }

            try {
                int i=0;
                for(final Iterator<BTGattService> srvIter = primServices.iterator(); srvIter.hasNext(); i++) {
                    final BTGattService primService = srvIter.next();
                    if( !QUIET ) {
                        printf("  [%02d] Service %s, uuid %s\n", i, primService.toString(), primService.getUUID());
                        printf("  [%02d] Service Characteristics\n", i);
                    }
                    int j=0;
                    final List<BTGattChar> serviceCharacteristics = primService.getChars();
                    for(final Iterator<BTGattChar> charIter = serviceCharacteristics.iterator(); charIter.hasNext(); j++) {
                        final BTGattChar serviceChar = charIter.next();
                        if( !QUIET ) {
                            printf("  [%02d.%02d] CharDef: %s, uuid %s\n", i, j, serviceChar.toString(), serviceChar.getUUID());
                        }
                        final List<String> properties = Arrays.asList(serviceChar.getFlags());
                        if( properties.contains("read") ) {
                            final byte[] value = serviceChar.readValue();
                            final String svalue = BTUtils.decodeUTF8String(value, 0, value.length);
                            if( !QUIET ) {
                                printf("  [%02d.%02d] CharVal: %s ('%s')\n",
                                        i, j, BTUtils.bytesHexString(value, 0, -1, true, true, true), svalue);
                            }
                        }
                        int k=0;
                        final List<BTGattDesc> charDescList = serviceChar.getDescriptors();
                        for(final Iterator<BTGattDesc> descIter = charDescList.iterator(); descIter.hasNext(); k++) {
                            final BTGattDesc charDesc = descIter.next();
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
            println("****** Processing Ready Device: Exception caught for " + device.toString() + ": "+t.getMessage());
            t.printStackTrace();
        }

        println("****** Processing Ready Device: End-1: Success " + success +
                           " on " + device.toString() + "; devInProc "+devicesInProcessing.size());

        devicesInProcessing.remove(device.getAddressAndType());

        if( !USE_WHITELIST && 0 == devicesInProcessing.size() ) {
            startDiscovery(device.getAdapter(), "post-processing-1");
        }

        if( KEEP_CONNECTED && GATT_PING_ENABLED && success ) {
            while( device.pingGATT() ) {
                println("****** Processing Ready Device: pingGATT OK: "+device.getAddressAndType());
                try {
                    Thread.sleep(1000);
                } catch (final InterruptedException e) {
                    e.printStackTrace();
                }
            }
            println("****** Processing Ready Device: pingGATT failed, waiting for disconnect: "+device.getAddressAndType());
            // Even w/ GATT_PING_ENABLED, we utilize disconnect event to clean up -> remove
        }

        println("****** Processing Ready Device: End-2: Success " + success +
                           " on " + device.toString() + "; devInProc "+devicesInProcessing.size());
        if( success ) {
            devicesProcessed.add(device.getAddressAndType());
        }

        if( !KEEP_CONNECTED ) {

            if( UNPAIR_DEVICE_POST ) {
                final HCIStatusCode unpair_res = device.unpair();
                println("****** Processing Ready Device: Unpair-Post result: "+unpair_res);
            }

            device.remove();

            if( 0 < RESET_ADAPTER_EACH_CONN && 0 == deviceReadyCount.get() % RESET_ADAPTER_EACH_CONN ) {
                resetAdapter(device.getAdapter(), 2);
            } else if( !USE_WHITELIST && 0 == devicesInProcessing.size() ) {
                startDiscovery(device.getAdapter(), "post-processing-2");
            }
        }

        if( 0 < MULTI_MEASUREMENTS.get() ) {
            MULTI_MEASUREMENTS.decrementAndGet();
            println("****** Processing Ready Device: MULTI_MEASUREMENTS left "+MULTI_MEASUREMENTS.get()+": "+device.getAddressAndType());
        }
    }

    private void removeDevice(final BTDevice device) {
        println("****** Remove Device: removing: "+device.getAddressAndType());
        device.getAdapter().stopDiscovery();

        devicesInProcessing.remove(device.getAddressAndType());

        device.remove();

        if( !USE_WHITELIST && 0 == devicesInProcessing.size() ) {
            startDiscovery(device.getAdapter(), "post-remove-device");
        }
    }

    private void resetAdapter(final BTAdapter adapter, final int mode) {
        println("****** Reset Adapter: reset["+mode+"] start: "+adapter.toString());
        final HCIStatusCode res = adapter.reset();
        println("****** Reset Adapter: reset["+mode+"] end: "+res+", "+adapter.toString());
    }

    private boolean startDiscovery(final BTAdapter adapter, final String msg) {
        final HCIStatusCode status = adapter.startDiscovery( true );
        println("****** Start discovery ("+msg+") result: "+status);
        return HCIStatusCode.SUCCESS == status;
    }

    private boolean initAdapter(final BTAdapter adapter) {
        if( !adapter.isPowered() ) { // should have been covered above
            println("Adapter not powered (2): "+adapter.toString());
            return false;
        }

        adapter.addStatusListener(statusListener, null);
        adapter.enableDiscoverableNotifications(new BooleanNotification("Discoverable", timestamp_t0));

        adapter.enableDiscoveringNotifications(new BooleanNotification("Discovering", timestamp_t0));

        adapter.enablePairableNotifications(new BooleanNotification("Pairable", timestamp_t0));

        adapter.enablePoweredNotifications(new BooleanNotification("Powered", timestamp_t0));

        // Flush discovered devices after registering our status listener.
        // This avoids discovered devices before we have registered!
        if( 0 == waitForDevices.size() ) {
            // we accept all devices, so flush all discovered devices
            adapter.removeDiscoveredDevices();
        } else {
            // only flush discovered devices we intend to listen to
            for( final Iterator<BDAddressAndType> iter=waitForDevices.iterator(); iter.hasNext(); ) {
                adapter.removeDiscoveredDevice( iter.next() );
            }
        }

        if( USE_WHITELIST ) {
            for(final Iterator<BDAddressAndType> wliter = whitelist.iterator(); wliter.hasNext(); ) {
                final BDAddressAndType addr = wliter.next();
                final boolean res = adapter.addDeviceToWhitelist(addr, HCIWhitelistConnectType.HCI_AUTO_CONN_ALWAYS);
                println("Added to whitelist: res "+res+", address "+addr);
            }
        } else {
            if( !startDiscovery(adapter, "kick-off") ) {
                return false;
            }
        }
        return true;
    }

    private final BTManager.ChangedAdapterSetListener myChangedAdapterSetListener =
            new BTManager.ChangedAdapterSetListener() {
                @Override
                public void adapterAdded(final BTAdapter adapter) {
                    if( initAdapter( adapter ) ) {
                        println("****** Adapter ADDED__: InitOK. " + adapter);
                    } else {
                        println("****** Adapter ADDED__: Ignored " + adapter);
                    }
                }

                @Override
                public void adapterRemoved(final BTAdapter adapter) {
                    println("****** Adapter REMOVED: " + adapter);
                }
    };

    public void runTest(final BTManager manager) {
        timestamp_t0 = BTUtils.currentTimeMillis();

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

        //
        // just a manually controlled pull down to show status, not required
        //
        final List<BTAdapter> adapters = manager.getAdapters();

        adapters.forEach(new Consumer<BTAdapter>() {
            @Override
            public void accept(final BTAdapter a) {
                println("****** EOL Adapter's Devices - pre_ close: " + a);
            } } );
        {
            final int count = manager.removeChangedAdapterSetListener(myChangedAdapterSetListener);
            println("****** EOL Removed ChangedAdapterSetCallback " + count);

            // All implicit via destructor or shutdown hook!
            manager.shutdown(); /* implies: adapter.close(); */
        }
        adapters.forEach(new Consumer<BTAdapter>() {
            @Override
            public void accept(final BTAdapter a) {
                println("****** EOL Adapter's Devices - post close: " + a);
            } } );

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
        final BTManager manager;
        try {
            manager = BTFactory.getDirectBTBluetoothManager();
        } catch (BTException | NoSuchMethodException | SecurityException
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
                    final BDAddressAndType a = new BDAddressAndType(new EUI48(args[++i]), BDAddressType.BDADDR_UNDEFINED);
                    test.waitForDevices.add(a);
                } else if( arg.equals("-wl") && args.length > (i+1) ) {
                    final BDAddressAndType wle = new BDAddressAndType(new EUI48(args[++i]), BDAddressType.BDADDR_LE_PUBLIC);
                    println("Whitelist + "+wle);
                    test.whitelist.add(wle);
                    test.USE_WHITELIST = true;
                } else if( arg.equals("-passkey") && args.length > (i+1) ) {
                    test.pairing_passkey = Integer.valueOf(args[++i]).intValue();
                } else if( arg.equals("-seclevel") && args.length > (i+1) ) {
                    test.sec_level = BTSecurityLevel.get( (byte)Integer.valueOf(args[++i]).intValue() );
                } else if( arg.equals("-iocap") && args.length > (i+1) ) {
                    test.io_capabilities = SMPIOCapability.get( (byte)Integer.valueOf(args[++i]).intValue() );
                } else if( arg.equals("-unpairPre") ) {
                    test.UNPAIR_DEVICE_PRE = true;
                } else if( arg.equals("-unpairPost") ) {
                    test.UNPAIR_DEVICE_POST = true;
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
                    "[-disconnect] [-enableGATTPing] [-count <number>] [-single] [-show_update_events] [-quiet] "+
                    "[-resetEachCon connectionCount] "+
                    "(-mac <device_address>)* (-wl <device_address>)* "+
                    "[-seclevel <int>] [-iocap <int>] [-passkey <digits>] " +
                    "[-unpairPre] [-unpairPost] "+
                    "[-charid <uuid>] [-charval <byte-val>] "+
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
        println("iocap "+test.io_capabilities);
        println("UNPAIR_DEVICE_PRE "+ test.UNPAIR_DEVICE_PRE);
        println("UNPAIR_DEVICE_POST "+ test.UNPAIR_DEVICE_POST);
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
