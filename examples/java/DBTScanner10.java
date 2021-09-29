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
import java.util.concurrent.atomic.AtomicInteger;
import java.util.function.Consumer;

import org.direct_bt.AdapterSettings;
import org.direct_bt.AdapterStatusListener;
import org.direct_bt.BDAddressAndType;
import org.direct_bt.BDAddressType;
import org.direct_bt.BLERandomAddressType;
import org.direct_bt.BTMode;
import org.direct_bt.BTRole;
import org.direct_bt.BTSecurityLevel;
import org.direct_bt.BTAdapter;
import org.direct_bt.BTDevice;
import org.direct_bt.BTDeviceRegistry;
import org.direct_bt.BTException;
import org.direct_bt.BTFactory;
import org.direct_bt.BTGattChar;
import org.direct_bt.BTGattDesc;
import org.direct_bt.BTGattService;
import org.direct_bt.BTManager;
import org.direct_bt.BTSecurityRegistry;
import org.direct_bt.BTType;
import org.direct_bt.BTUtils;
import org.direct_bt.EIRDataTypeSet;
import org.direct_bt.GattCharPropertySet;
import org.direct_bt.HCIStatusCode;
import org.direct_bt.HCIWhitelistConnectType;
import org.direct_bt.LE_Features;
import org.direct_bt.LE_PHYs;
import org.direct_bt.PairingMode;
import org.direct_bt.SMPIOCapability;
import org.direct_bt.SMPKeyBin;
import org.direct_bt.SMPPairingState;
import org.direct_bt.ScanType;
import org.jau.net.EUI48;

import jau.direct_bt.DBTManager;

/**
 * This Java scanner {@link BTRole::Master} example uses the Direct-BT fully event driven workflow
 * and adds multithreading, i.e. one thread processes each found device found
 * as notified via the event listener.
 * <p>
 * This example represents the recommended utilization of Direct-BT.
 * </p>
 * <p>
 * See `dbt_scanner10.cpp` for invocation examples, since both apps are fully compatible.
 * </p>
 */
public class DBTScanner10 {
    static final String KEY_PATH = "keys";

    long timestamp_t0;

    EUI48 useAdapter = EUI48.ALL_DEVICE;
    BTMode btMode = BTMode.DUAL;
    BTAdapter chosenAdapter = null;

    int RESET_ADAPTER_EACH_CONN = 0;
    AtomicInteger deviceReadyCount = new AtomicInteger(0);

    AtomicInteger MULTI_MEASUREMENTS = new AtomicInteger(8);
    boolean KEEP_CONNECTED = true;

    boolean GATT_PING_ENABLED = false;
    boolean REMOVE_DEVICE = true;
    boolean USE_WHITELIST = false;
    final List<BDAddressAndType> whitelist = new ArrayList<BDAddressAndType>();

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

    class MyAdapterStatusListener extends AdapterStatusListener {
        @Override
        public void adapterSettingsChanged(final BTAdapter adapter, final AdapterSettings oldmask,
                                           final AdapterSettings newmask, final AdapterSettings changedmask, final long timestamp) {
            final boolean initialSetting = oldmask.isEmpty();
            if( initialSetting ) {
                BTUtils.println(System.err, "****** SETTINGS: "+oldmask+" -> "+newmask+", initial "+changedmask);
            } else {
                BTUtils.println(System.err, "****** SETTINGS: "+oldmask+" -> "+newmask+", changed "+changedmask);
            }
            BTUtils.println(System.err, "Status Adapter:");
            BTUtils.println(System.err, adapter.toString());
            if( !initialSetting &&
                changedmask.isSet(AdapterSettings.SettingType.POWERED) &&
                newmask.isSet(AdapterSettings.SettingType.POWERED) )
            {

                executeOffThread( () -> { startDiscovery(adapter, "powered-on"); },
                                  "DBT-StartDiscovery-"+adapter.getAddressAndType(), true /* detach */);
            }
        }

        @Override
        public void discoveringChanged(final BTAdapter adapter, final ScanType currentMeta, final ScanType changedType, final boolean changedEnabled, final boolean keepAlive, final long timestamp) {
            BTUtils.println(System.err, "****** DISCOVERING: meta "+currentMeta+", changed["+changedType+", enabled "+changedEnabled+", keepAlive "+keepAlive+"] on "+adapter);
        }

        @Override
        public boolean deviceFound(final BTDevice device, final long timestamp) {
            BTUtils.println(System.err, "****** FOUND__: "+device.toString());

            if( !BTDeviceRegistry.isDeviceProcessing( device.getAddressAndType() ) &&
                ( !BTDeviceRegistry.isWaitingForAnyDevice() ||
                  ( BTDeviceRegistry.isWaitingForDevice(device.getAddressAndType().address, device.getName()) &&
                    ( 0 < MULTI_MEASUREMENTS.get() || !BTDeviceRegistry.isDeviceProcessed(device.getAddressAndType()) )
                  )
                )
              )
            {
                BTUtils.println(System.err, "****** FOUND__-0: Connecting "+device.toString());
                {
                    final long td = BTUtils.currentTimeMillis() - timestamp_t0; // adapter-init -> now
                    BTUtils.println(System.err, "PERF: adapter-init -> FOUND__-0 " + td + " ms");
                }
                executeOffThread( () -> { connectDiscoveredDevice(device); },
                                  "DBT-Connect-"+device.getAddressAndType(), true /* detach */);
                return true;
            } else {
                BTUtils.println(System.err, "****** FOUND__-1: NOP "+device.toString());
                return false;
            }
        }

        @Override
        public void deviceUpdated(final BTDevice device, final EIRDataTypeSet updateMask, final long timestamp) {
            if( SHOW_UPDATE_EVENTS ) {
                BTUtils.println(System.err, "****** UPDATED: "+updateMask+" of "+device);
            }
        }

        @Override
        public void deviceConnected(final BTDevice device, final short handle, final long timestamp) {
            BTUtils.println(System.err, "****** CONNECTED: "+device.toString());
        }

        @Override
        public void devicePairingState(final BTDevice device, final SMPPairingState state, final PairingMode mode, final long timestamp) {
            BTUtils.println(System.err, "****** PAIRING_STATE: state "+state+", mode "+mode+": "+device);
            switch( state ) {
                case NONE:
                    // next: deviceReady(..)
                    break;
                case FAILED: {
                    final boolean res  = SMPKeyBin.remove(KEY_PATH, device.getAddressAndType());
                    BTUtils.println(System.err, "****** PAIRING_STATE: state "+state+"; Remove key file "+SMPKeyBin.getFilename(KEY_PATH, device.getAddressAndType())+", res "+res);
                    // next: deviceReady() or deviceDisconnected(..)
                } break;
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
                    final BTSecurityRegistry.Entry sec = BTSecurityRegistry.getStartOf(device.getAddressAndType().address, "");
                    if( null != sec && sec.getPairingPasskey() != BTSecurityRegistry.NO_PASSKEY ) {
                        executeOffThread( () -> { device.setPairingPasskey( sec.getPairingPasskey() ); }, "DBT-SetPasskey-"+device.getAddressAndType(), true /* detach */);
                    } else {
                        executeOffThread( () -> { device.setPairingPasskey( 0 ); }, "DBT-SetPasskey-"+device.getAddressAndType(), true /* detach */);
                        // 3s disconnect: executeOffThread( () -> { device.setPairingPasskeyNegative(); }, "DBT-SetPasskeyNegative-"+device.getAddressAndType(), true /* detach */);
                    }
                    // next: KEY_DISTRIBUTION or FAILED
                  } break;
                case NUMERIC_COMPARE_EXPECTED: {
                    final BTSecurityRegistry.Entry sec = BTSecurityRegistry.getStartOf(device.getAddressAndType().address, "");
                    if( null != sec ) {
                        executeOffThread( () -> { device.setPairingNumericComparison( sec.getPairingNumericComparison() ); }, "DBT-SetNumericComp-"+device.getAddressAndType(), true /* detach */);
                    } else {
                        executeOffThread( () -> { device.setPairingNumericComparison( false ); }, "DBT-SetNumericCompFalse-"+device.getAddressAndType(), true /* detach */);
                    }
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
            if( !BTDeviceRegistry.isDeviceProcessing( device.getAddressAndType() ) &&
                ( !BTDeviceRegistry.isWaitingForAnyDevice() ||
                  ( BTDeviceRegistry.isWaitingForDevice(device.getAddressAndType().address, device.getName()) &&
                    ( 0 < MULTI_MEASUREMENTS.get() || !BTDeviceRegistry.isDeviceProcessed(device.getAddressAndType()) )
                  )
                )
              )
            {
                deviceReadyCount.incrementAndGet();
                BTUtils.println(System.err, "****** READY-0: Processing["+deviceReadyCount.get()+"] "+device.toString());
                {
                    final long td = BTUtils.currentTimeMillis() - timestamp_t0; // adapter-init -> now
                    BTUtils.println(System.err, "PERF: adapter-init -> READY-0 " + td + " ms");
                }
                BTDeviceRegistry.addToProcessingDevices(device.getAddressAndType(), device.getName());
                processReadyDevice(device); // AdapterStatusListener::deviceReady() explicitly allows prolonged and complex code execution!
            } else {
                BTUtils.println(System.err, "****** READY-1: NOP " + device.toString());
            }
        }

        @Override
        public void deviceDisconnected(final BTDevice device, final HCIStatusCode reason, final short handle, final long timestamp) {
            BTUtils.println(System.err, "****** DISCONNECTED: Reason "+reason+", old handle 0x"+Integer.toHexString(handle)+": "+device+" on "+device.getAdapter());

            if( REMOVE_DEVICE ) {
                executeOffThread( () -> { removeDevice(device); }, "DBT-Remove-"+device.getAddressAndType(), true /* detach */);
            } else {
                BTDeviceRegistry.removeFromProcessingDevices(device.getAddressAndType());
            }
            if( 0 < RESET_ADAPTER_EACH_CONN && 0 == deviceReadyCount.get() % RESET_ADAPTER_EACH_CONN ) {
                executeOffThread( () -> { resetAdapter(device.getAdapter(), 1); },
                                  "DBT-Reset-"+device.getAdapter().getAddressAndType(), true /* detach */ );
            }
        }

        @Override
        public String toString() {
            return "AdapterStatusListener[user, per-adapter]";
        }
    };

    class MyGATTEventListener implements BTGattChar.Listener {
        private final int i, j;

        public MyGATTEventListener(final int i_, final int j_) { i=i_; j=j_; }

        @Override
        public void notificationReceived(final BTGattChar charDecl,
                final byte[] value, final long timestamp) {
            final long tR = BTUtils.currentTimeMillis();
            printf("**[%02d.%02d] Characteristic-Notify: UUID %s, td %d ******\n",
                    i, j, charDecl.getUUID(), (tR-timestamp));
            printf("**[%02d.%02d]     Characteristic: %s ******\n", i, j, charDecl.toString());
            printf("**[%02d.%02d]     Value R: size %d, ro: %s ******\n", i, j, value.length, BTUtils.bytesHexString(value, 0, -1, true));

            shutdownTest();
        }

        @Override
        public void indicationReceived(final BTGattChar charDecl,
                final byte[] value, final long timestamp, final boolean confirmationSent) {
            final long tR = BTUtils.currentTimeMillis();
            printf("**[%02d.%02d] Characteristic-Indication: UUID %s, td %d, confirmed %b ******\n",
                    i, j, charDecl.getUUID(), (tR-timestamp), confirmationSent);
            printf("**[%02d.%02d]     Characteristic: %s ******\n", i, j, charDecl.toString());
            printf("**[%02d.%02d]     Value R: size %d, ro: %s ******\n", i, j, value.length, BTUtils.bytesHexString(value, 0, -1, true));

            shutdownTest();
        }
    }

    private void connectDiscoveredDevice(final BTDevice device) {
        BTUtils.println(System.err, "****** Connecting Device: Start " + device.toString());

        // Testing listener lifecycle @ device dtor
        device.addStatusListener(new AdapterStatusListener() {
            @Override
            public void deviceUpdated(final BTDevice device, final EIRDataTypeSet updateMask, final long timestamp) {
                if( SHOW_UPDATE_EVENTS ) {
                    BTUtils.println(System.err, "****** UPDATED(2): "+updateMask+" of "+device);
                }
            }
            @Override
            public void deviceConnected(final BTDevice device, final short handle, final long timestamp) {
                BTUtils.println(System.err, "****** CONNECTED(2): "+device.toString());
            }

            @Override
            public String toString() {
                return "TempAdapterStatusListener[user, device "+device.getAddressAndType().toString()+"]";
            } });

        {
            final HCIStatusCode r = device.unpair();
            BTUtils.println(System.err, "****** Connecting Device: Unpair-Pre result: "+r);
        }

        {
            final HCIStatusCode r = device.getAdapter().stopDiscovery();
            BTUtils.println(System.err, "****** Connecting Device: stopDiscovery result "+r);
        }

        final BTSecurityRegistry.Entry sec = BTSecurityRegistry.getStartOf(device.getAddressAndType().address, device.getName());
        if( null != sec ) {
            BTUtils.println(System.err, "****** Connecting Device: Found SecurityDetail "+sec.toString()+" for "+device.toString());
        } else {
            BTUtils.println(System.err, "****** Connecting Device: No SecurityDetail for "+device.toString());
        }
        final BTSecurityLevel req_sec_level = null != sec ? sec.getSecLevel() : BTSecurityLevel.UNSET;
        HCIStatusCode res = SMPKeyBin.readAndApply(KEY_PATH, device, req_sec_level, true /* verbose */);
        BTUtils.fprintf_td(System.err, "****** Connecting Device: SMPKeyBin::readAndApply(..) result %s\n", res.toString());
        if( HCIStatusCode.SUCCESS != res ) {
            if( null != sec ) {
                if( sec.isSecurityAutoEnabled() ) {
                    final boolean r = device.setConnSecurityAuto( sec.getSecurityAutoIOCap() );
                    BTUtils.println(System.err, "****** Connecting Device: Using SecurityDetail.SEC AUTO "+sec+" -> set OK "+r);
                } else if( sec.isSecLevelOrIOCapSet() ) {
                    final boolean r = device.setConnSecurityBest(sec.getSecLevel(), sec.getIOCap());
                    BTUtils.println(System.err, "****** Connecting Device: Using SecurityDetail.Level+IOCap "+sec+" -> set OK "+r);
                } else {
                    final boolean r = device.setConnSecurityAuto( SMPIOCapability.KEYBOARD_ONLY );
                    BTUtils.println(System.err, "****** Connecting Device: Setting SEC AUTO security detail w/ KEYBOARD_ONLY ("+sec+") -> set OK "+r);
                }
            } else {
                final boolean r = device.setConnSecurityAuto( SMPIOCapability.KEYBOARD_ONLY );
                BTUtils.println(System.err, "****** Connecting Device: Setting SEC AUTO security detail w/ KEYBOARD_ONLY -> set OK "+r);
            }
        }

        if( !USE_WHITELIST ) {
            res = device.connect();
        } else {
            res = HCIStatusCode.SUCCESS;
        }
        BTUtils.println(System.err, "****** Connecting Device Command, res "+res+": End result "+res+" of " + device.toString());

        if( !USE_WHITELIST && 0 == BTDeviceRegistry.getProcessingDeviceCount() && HCIStatusCode.SUCCESS != res ) {
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
        BTUtils.println(System.err, "****** Processing Ready Device: Start " + device.toString());
        {
            // make sure for pending connections on failed connect*(..) command
            final HCIStatusCode r = device.getAdapter().stopDiscovery();
            BTUtils.println(System.err, "****** Processing Ready Device: stopDiscovery result "+r);
        }

        final long t1 = BTUtils.currentTimeMillis();

        SMPKeyBin.createAndWrite(device, KEY_PATH, false /* overwrite */, true /* verbose */);

        boolean success = false;

        {
            final LE_PHYs Tx = new LE_PHYs(LE_PHYs.PHY.LE_2M);
            final LE_PHYs Rx = new LE_PHYs(LE_PHYs.PHY.LE_2M);

            final HCIStatusCode res = device.setConnectedLE_PHY(true /* tryTx */, true /* tryRx */, Tx, Rx);
            BTUtils.fprintf_td(System.err, "****** Set Connected LE PHY: status %s: Tx %s, Rx %s\n",
                                res.toString(), Tx.toString(), Rx.toString());
        }
        {
            final LE_PHYs resTx[] = { new LE_PHYs() };
            final LE_PHYs resRx[] = { new LE_PHYs() };
            final HCIStatusCode res = device.getConnectedLE_PHY(resTx, resRx);
            BTUtils.fprintf_td(System.err, "****** Got Connected LE PHY: status %s: Tx %s, Rx %s\n",
                    res.toString(), resTx[0].toString(), resRx[0].toString());
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
                BTUtils.println(System.err, System.lineSeparator()+System.lineSeparator());
                BTUtils.println(System.err, "PERF: GATT primary-services completed\n");
                BTUtils.println(System.err, "PERF:  adapter-init to processing-start " + td01 + " ms,"+System.lineSeparator()+
                        "PERF:  get-gatt-services " + td15 + " ms,"+System.lineSeparator()+
                        "PERF:  discovered to gatt-complete " + tdc5 + " ms (connect " + (tdc5 - td15) + " ms),"+System.lineSeparator()+
                        "PERF:  adapter-init to gatt-complete " + td05 + " ms"+System.lineSeparator());
            }

            try {
                int i=0;
                for(final Iterator<BTGattService> srvIter = primServices.iterator(); srvIter.hasNext(); i++) {
                    final BTGattService primService = srvIter.next();
                    if( !QUIET ) {
                        printf("  [%02d] Service UUID %s\n", i, primService.getUUID());
                        printf("  [%02d]         %s\n", i, primService.toString());
                    }
                    int j=0;
                    final List<BTGattChar> serviceCharacteristics = primService.getChars();
                    for(final Iterator<BTGattChar> charIter = serviceCharacteristics.iterator(); charIter.hasNext(); j++) {
                        final BTGattChar serviceChar = charIter.next();
                        if( !QUIET ) {
                            printf("  [%02d.%02d] Characteristic: UUID %s\n", i, j, serviceChar.getUUID());
                            printf("  [%02d.%02d]     %s\n", i, j, serviceChar.toString());
                        }
                        final GattCharPropertySet properties = serviceChar.getProperties();
                        if( properties.isSet(GattCharPropertySet.Type.Read) ) {
                            final byte[] value = serviceChar.readValue();
                            final String svalue = BTUtils.decodeUTF8String(value, 0, value.length);
                            if( !QUIET ) {
                                printf("  [%02d.%02d]     value: %s ('%s')\n", i, j, BTUtils.bytesHexString(value, 0, -1, true), svalue);
                            }
                        }
                        int k=0;
                        final List<BTGattDesc> charDescList = serviceChar.getDescriptors();
                        for(final Iterator<BTGattDesc> descIter = charDescList.iterator(); descIter.hasNext(); k++) {
                            final BTGattDesc charDesc = descIter.next();
                            if( !QUIET ) {
                                printf("  [%02d.%02d.%02d] Descriptor: UUID %s\n", i, j, k, charDesc.getUUID());
                                printf("  [%02d.%02d.%02d]     %s\n", i, j, k, charDesc.toString());
                            }
                        }
                        final boolean cccdEnableResult[] = { false, false };
                        final boolean cccdRet = null != serviceChar.addCharListener( new MyGATTEventListener(i, j), cccdEnableResult );
                        if( !QUIET ) {
                            printf("  [%02d.%02d] Characteristic-Listener: Notification(%b), Indication(%b): Added %b\n",
                                    i, j, cccdEnableResult[0], cccdEnableResult[1], cccdRet);
                            printf("\n");
                        }
                    }
                    printf("\n");
                }
            } catch( final Exception ex) {
                BTUtils.println(System.err, "Caught "+ex.getMessage());
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
            BTUtils.println(System.err, "****** Processing Ready Device: Exception caught for " + device.toString() + ": "+t.getMessage());
            t.printStackTrace();
        }

        BTUtils.println(System.err, "****** Processing Ready Device: End-1: Success " + success +
                           " on " + device.toString() + "; devInProc "+BTDeviceRegistry.getProcessingDeviceCount());

        BTDeviceRegistry.removeFromProcessingDevices( device.getAddressAndType() );

        if( !USE_WHITELIST && 0 == BTDeviceRegistry.getProcessingDeviceCount() ) {
            startDiscovery(device.getAdapter(), "post-processing-1");
        }

        if( KEEP_CONNECTED && GATT_PING_ENABLED && success ) {
            while( device.pingGATT() ) {
                BTUtils.println(System.err, "****** Processing Ready Device: pingGATT OK: "+device.getAddressAndType());
                try {
                    Thread.sleep(1000);
                } catch (final InterruptedException e) {
                    e.printStackTrace();
                }
            }
            BTUtils.println(System.err, "****** Processing Ready Device: pingGATT failed, waiting for disconnect: "+device.getAddressAndType());
            // Even w/ GATT_PING_ENABLED, we utilize disconnect event to clean up -> remove
        }

        BTUtils.println(System.err, "****** Processing Ready Device: End-2: Success " + success +
                           " on " + device.toString() + "; devInProc "+BTDeviceRegistry.getProcessingDeviceCount());
        if( success ) {
            BTDeviceRegistry.addToProcessedDevices(device.getAddressAndType(), device.getName());
        }
        device.removeAllCharListener();

        if( !KEEP_CONNECTED ) {

            {
                final HCIStatusCode unpair_res = device.unpair();
                BTUtils.println(System.err, "****** Processing Ready Device: Unpair-Post result: "+unpair_res);
            }

            device.remove();

            if( 0 < RESET_ADAPTER_EACH_CONN && 0 == deviceReadyCount.get() % RESET_ADAPTER_EACH_CONN ) {
                resetAdapter(device.getAdapter(), 2);
            } else if( !USE_WHITELIST && 0 == BTDeviceRegistry.getProcessingDeviceCount() ) {
                startDiscovery(device.getAdapter(), "post-processing-2");
            }
        }

        if( 0 < MULTI_MEASUREMENTS.get() ) {
            MULTI_MEASUREMENTS.decrementAndGet();
            BTUtils.println(System.err, "****** Processing Ready Device: MULTI_MEASUREMENTS left "+MULTI_MEASUREMENTS.get()+": "+device.getAddressAndType());
        }
    }

    private void removeDevice(final BTDevice device) {
        BTUtils.println(System.err, "****** Remove Device: removing: "+device.getAddressAndType());
        device.getAdapter().stopDiscovery();

        BTDeviceRegistry.removeFromProcessingDevices(device.getAddressAndType());

        device.remove();

        if( !USE_WHITELIST && 0 == BTDeviceRegistry.getProcessingDeviceCount() ) {
            startDiscovery(device.getAdapter(), "post-remove-device");
        }
    }

    private void resetAdapter(final BTAdapter adapter, final int mode) {
        BTUtils.println(System.err, "****** Reset Adapter: reset["+mode+"] start: "+adapter.toString());
        final HCIStatusCode res = adapter.reset();
        BTUtils.println(System.err, "****** Reset Adapter: reset["+mode+"] end: "+res+", "+adapter.toString());
    }

    static boolean le_scan_active = true; // default value
    static final short le_scan_interval = (short)24; // default value
    static final short le_scan_window = (short)24; // default value
    static final byte filter_policy = (byte)0; // default value

    private boolean startDiscovery(final BTAdapter adapter, final String msg) {
        if( !useAdapter.equals(EUI48.ALL_DEVICE) && !useAdapter.equals(adapter.getAddressAndType().address) ) {
            BTUtils.fprintf_td(System.err, "****** Start discovery (%s): Adapter not selected: %s\n", msg, adapter.toString());
            return false;
        }
        final HCIStatusCode status = adapter.startDiscovery( true, le_scan_active, le_scan_interval, le_scan_window, filter_policy );
        BTUtils.println(System.err, "****** Start discovery ("+msg+") result: "+status);
        return HCIStatusCode.SUCCESS == status;
    }

    private boolean initAdapter(final BTAdapter adapter) {
        if( !useAdapter.equals(EUI48.ALL_DEVICE) && !useAdapter.equals(adapter.getAddressAndType().address) ) {
            BTUtils.fprintf_td(System.err, "initAdapter: Adapter not selected: %s\n", adapter.toString());
            return false;
        }
        // Initialize with defaults and power-on
        if( !adapter.isInitialized() ) {
            final HCIStatusCode status = adapter.initialize( btMode );
            if( HCIStatusCode.SUCCESS != status ) {
                BTUtils.fprintf_td(System.err, "initAdapter: Adapter initialization failed: %s: %s\n",
                        status.toString(), adapter.toString());
                return false;
            }
        } else if( !adapter.setPowered( true ) ) {
            BTUtils.fprintf_td(System.err, "initAdapter: Already initialized adapter power-on failed:: %s\n", adapter.toString());
            return false;
        }
        // adapter is powered-on
        BTUtils.fprintf_td(System.err, "initAdapter: %s\n", adapter.toString());
        {
            final LE_Features le_feats = adapter.getLEFeatures();
            BTUtils.fprintf_td(System.err, "initAdapter: LE_Features %s\n", le_feats.toString());
        }
        {
            final LE_PHYs Tx = new LE_PHYs(LE_PHYs.PHY.LE_2M);
            final LE_PHYs Rx = new LE_PHYs(LE_PHYs.PHY.LE_2M);

            final HCIStatusCode res = adapter.setDefaultLE_PHY(true /* tryTx */, true /* tryRx */, Tx, Rx);
            BTUtils.fprintf_td(System.err, "initAdapter: Set Default LE PHY: status %s: Tx %s, Rx %s\n",
                                res.toString(), Tx.toString(), Rx.toString());
        }
        final AdapterStatusListener asl = new MyAdapterStatusListener();
        adapter.addStatusListener( asl );
        // Flush discovered devices after registering our status listener.
        // This avoids discovered devices before we have registered!
        adapter.removeDiscoveredDevices();

        if( USE_WHITELIST ) {
            for(final Iterator<BDAddressAndType> wliter = whitelist.iterator(); wliter.hasNext(); ) {
                final BDAddressAndType addr = wliter.next();
                final boolean res = adapter.addDeviceToWhitelist(addr, HCIWhitelistConnectType.HCI_AUTO_CONN_ALWAYS);
                BTUtils.println(System.err, "initAdapter: Added to whitelist: res "+res+", address "+addr);
            }
        } else {
            if( !startDiscovery(adapter, "initAdapter") ) {
                adapter.removeStatusListener( asl );
                return false;
            }
        }
        return true;
    }

    private final BTManager.ChangedAdapterSetListener myChangedAdapterSetListener =
            new BTManager.ChangedAdapterSetListener() {
                @Override
                public void adapterAdded(final BTAdapter adapter) {
                    if( null == chosenAdapter ) {
                        if( initAdapter( adapter ) ) {
                            chosenAdapter = adapter;
                            BTUtils.println(System.err, "****** Adapter ADDED__: InitOK: " + adapter);
                        } else {
                            BTUtils.println(System.err, "****** Adapter ADDED__: Ignored: " + adapter);
                        }
                    } else {
                        BTUtils.println(System.err, "****** Adapter ADDED__: Ignored (other): " + adapter);
                    }
                }

                @Override
                public void adapterRemoved(final BTAdapter adapter) {
                    if( null != chosenAdapter && adapter == chosenAdapter ) {
                        chosenAdapter = null;
                        BTUtils.println(System.err, "****** Adapter REMOVED: " + adapter);
                    } else {
                        BTUtils.println(System.err, "****** Adapter REMOVED (other): " + adapter);
                    }
                }
    };

    public void runTest(final BTManager manager) {
        timestamp_t0 = BTUtils.currentTimeMillis();

        boolean done = false;

        manager.addChangedAdapterSetListener(myChangedAdapterSetListener);

        while( !done ) {
            if( 0 == MULTI_MEASUREMENTS.get() ||
                ( -1 == MULTI_MEASUREMENTS.get() && BTDeviceRegistry.isWaitingForAnyDevice() && BTDeviceRegistry.areAllDevicesProcessed() )
              )
            {
                BTUtils.println(System.err, "****** EOL Test MULTI_MEASUREMENTS left "+MULTI_MEASUREMENTS.get()+
                                   ", processed "+BTDeviceRegistry.getProcessedDeviceCount()+"/"+BTDeviceRegistry.getWaitForDevicesCount());
                BTUtils.println(System.err, "****** WaitForDevices "+BTDeviceRegistry.getWaitForDevicesString());
                BTUtils.println(System.err, "****** DevicesProcessed "+BTDeviceRegistry.getProcessedDevicesString());
                done = true;
            } else {
                try {
                    Thread.sleep(2000);
                } catch (final InterruptedException e) {
                    e.printStackTrace();
                }
            }
        }
        chosenAdapter = null;

        //
        // just a manually controlled pull down to show status, not required
        //
        final List<BTAdapter> adapters = manager.getAdapters();

        adapters.forEach(new Consumer<BTAdapter>() {
            @Override
            public void accept(final BTAdapter a) {
                BTUtils.println(System.err, "****** EOL Adapter's Devices - pre_ close: " + a);
            } } );
        {
            final int count = manager.removeChangedAdapterSetListener(myChangedAdapterSetListener);
            BTUtils.println(System.err, "****** EOL Removed ChangedAdapterSetCallback " + count);

            // All implicit via destructor or shutdown hook!
            manager.shutdown(); /* implies: adapter.close(); */
        }
        adapters.forEach(new Consumer<BTAdapter>() {
            @Override
            public void accept(final BTAdapter a) {
                BTUtils.println(System.err, "****** EOL Adapter's Devices - post close: " + a);
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
            }
        }
        final BTManager manager;
        try {
            manager = BTFactory.getDirectBTManager();
        } catch (BTException | NoSuchMethodException | SecurityException
                | IllegalAccessException | IllegalArgumentException
                | InvocationTargetException | ClassNotFoundException e) {
            System.err.println("Unable to instantiate DirectBT BluetoothManager");
            e.printStackTrace();
            System.exit(-1);
            return;
        }
        BTUtils.println(System.err, "DirectBT BluetoothManager initialized!");
        BTUtils.println(System.err, "DirectBT Native Version "+BTFactory.getNativeVersion()+" (API "+BTFactory.getNativeAPIVersion()+")");
        BTUtils.println(System.err, "DirectBT Java Version "+BTFactory.getImplVersion()+" (API "+BTFactory.getAPIVersion()+")");

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
                } else if( arg.equals("-scanPassive") ) {
                    le_scan_active = false;
                } else if( arg.equals("-shutdown") && args.length > (i+1) ) {
                    test.shutdownTest = Integer.valueOf(args[++i]).intValue();
                } else if( arg.equals("-btmode") && args.length > (i+1) ) {
                    test.btMode = BTMode.get(args[++i]);
                } else if( arg.equals("-adapter") && args.length > (i+1) ) {
                    test.useAdapter = new EUI48( args[++i] );
                } else if( arg.equals("-dev") && args.length > (i+1) ) {
                    BTDeviceRegistry.addToWaitForDevices( args[++i] );
                } else if( arg.equals("-wl") && args.length > (i+1) ) {
                    final BDAddressAndType wle = new BDAddressAndType(new EUI48(args[++i]), BDAddressType.BDADDR_LE_PUBLIC);
                    BTUtils.println(System.err, "Whitelist + "+wle);
                    test.whitelist.add(wle);
                    test.USE_WHITELIST = true;
                } else if( arg.equals("-passkey") && args.length > (i+2) ) {
                    final String addrOrNameSub = args[++i];
                    final BTSecurityRegistry.Entry sec = BTSecurityRegistry.getOrCreate(addrOrNameSub);
                    sec.passkey = Integer.valueOf(args[++i]).intValue();
                    System.err.println("Set passkey in "+sec);
                } else if( arg.equals("-seclevel") && args.length > (i+2) ) {
                    final String addrOrNameSub = args[++i];
                    final BTSecurityRegistry.Entry sec = BTSecurityRegistry.getOrCreate(addrOrNameSub);
                    final int sec_level_i = Integer.valueOf(args[++i]).intValue();
                    sec.sec_level = BTSecurityLevel.get( (byte)( sec_level_i & 0xff ) );
                    System.err.println("Set sec_level "+sec_level_i+" in "+sec);
                } else if( arg.equals("-iocap") && args.length > (i+2) ) {
                    final String addrOrNameSub = args[++i];
                    final BTSecurityRegistry.Entry sec = BTSecurityRegistry.getOrCreate(addrOrNameSub);
                    final int io_cap_i = Integer.valueOf(args[++i]).intValue();
                    sec.io_cap = SMPIOCapability.get( (byte)( io_cap_i & 0xff ) );
                    System.err.println("Set io_cap "+io_cap_i+" in "+sec);
                } else if( arg.equals("-secauto") && args.length > (i+2) ) {
                    final String addrOrNameSub = args[++i];
                    final BTSecurityRegistry.Entry sec = BTSecurityRegistry.getOrCreate(addrOrNameSub);
                    final int io_cap_i = Integer.valueOf(args[++i]).intValue();
                    sec.io_cap_auto = SMPIOCapability.get( (byte)( io_cap_i & 0xff ) );
                    System.err.println("Set SEC AUTO security io_cap "+io_cap_i+" in "+sec);
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
            BTUtils.println(System.err, "Run with '[-btmode LE|BREDR|DUAL] "+
                    "[-disconnect] [-enableGATTPing] [-count <number>] [-single] [-show_update_events] [-quiet] "+
                    "[-scanPassive]"+
                    "[-resetEachCon connectionCount] "+
                    "[-adapter <adapter_address>] "+
                    "(-dev <device_[address|name]_sub>)* (-wl <device_address>)* "+
                    "(-seclevel <device_[address|name]_sub> <int_sec_level>)* "+
                    "(-iocap <device_[address|name]_sub> <int_iocap>)* "+
                    "(-secauto <device_[address|name]_sub> <int_iocap>)* "+
                    "(-passkey <device_[address|name]_sub> <digits>)* "+
                    "[-verbose] [-debug] "+
                    "[-dbt_verbose true|false] "+
                    "[-dbt_debug true|false|adapter.event,gatt.data,hci.event,hci.scan_ad_eir,mgmt.event] "+
                    "[-dbt_mgmt cmd.timeout=3000,ringsize=64,...] "+
                    "[-dbt_hci cmd.complete.timeout=10000,cmd.status.timeout=3000,ringsize=64,...] "+
                    "[-dbt_gatt cmd.read.timeout=500,cmd.write.timeout=500,cmd.init.timeout=2500,ringsize=128,...] "+
                    "[-dbt_l2cap reader.timeout=10000,restart.count=0,...] "+
                    "[-shutdown <int>]'");
        }

        BTUtils.println(System.err, "MULTI_MEASUREMENTS "+test.MULTI_MEASUREMENTS.get());
        BTUtils.println(System.err, "KEEP_CONNECTED "+test.KEEP_CONNECTED);
        BTUtils.println(System.err, "RESET_ADAPTER_EACH_CONN "+test.RESET_ADAPTER_EACH_CONN);
        BTUtils.println(System.err, "GATT_PING_ENABLED "+test.GATT_PING_ENABLED);
        BTUtils.println(System.err, "REMOVE_DEVICE "+test.REMOVE_DEVICE);
        BTUtils.println(System.err, "USE_WHITELIST "+test.USE_WHITELIST);
        BTUtils.println(System.err, "SHOW_UPDATE_EVENTS "+test.SHOW_UPDATE_EVENTS);
        BTUtils.println(System.err, "QUIET "+test.QUIET);
        BTUtils.println(System.err, "adapter "+test.useAdapter);
        BTUtils.println(System.err, "btmode' to "+test.btMode.toString());

        BTUtils.println(System.err, "security-details: "+BTSecurityRegistry.allToString() );
        BTUtils.println(System.err, "waitForDevices: "+BTDeviceRegistry.getWaitForDevicesString());

        if( waitForEnter ) {
            BTUtils.println(System.err, "Press ENTER to continue\n");
            try{ System.in.read();
            } catch(final Exception e) { }
        }
        test.runTest(manager);
    }
}
