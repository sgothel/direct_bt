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
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Iterator;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.function.Consumer;

import org.direct_bt.AdapterSettings;
import org.direct_bt.AdapterStatusListener;
import org.direct_bt.BTMode;
import org.direct_bt.BTSecurityLevel;
import org.direct_bt.BTAdapter;
import org.direct_bt.BTDevice;
import org.direct_bt.BTDeviceRegistry;
import org.direct_bt.BTException;
import org.direct_bt.BTFactory;
import org.direct_bt.BTGattChar;
import org.direct_bt.BTGattCharListener;
import org.direct_bt.BTGattCmd;
import org.direct_bt.BTGattDesc;
import org.direct_bt.BTGattService;
import org.direct_bt.BTManager;
import org.direct_bt.BTSecurityRegistry;
import org.direct_bt.BTUtils;
import org.direct_bt.DBGattChar;
import org.direct_bt.DBGattDesc;
import org.direct_bt.DBGattServer;
import org.direct_bt.DBGattService;
import org.direct_bt.DBGattValue;
import org.direct_bt.DiscoveryPolicy;
import org.direct_bt.EIRDataTypeSet;
import org.direct_bt.EInfoReport;
import org.direct_bt.GattCharPropertySet;
import org.direct_bt.HCIStatusCode;
import org.direct_bt.LE_Features;
import org.direct_bt.LE_PHYs;
import org.direct_bt.PairingMode;
import org.direct_bt.SMPIOCapability;
import org.direct_bt.SMPKeyBin;
import org.direct_bt.SMPPairingState;
import org.direct_bt.ScanType;
import org.jau.io.PrintUtil;
import org.jau.net.EUI48;
import org.jau.sys.Clock;
import org.jau.util.BasicTypes;

import jau.direct_bt.DBTManager;

/**
 * This Java example demonstrates a client connecting to `Avalun's LabPad device`.
 *
 * It differs from _dbt_scanner10_ as follows:
 *
 * * Employs a minimal GattServer supplying `Generic Access` service
 *
 * * Performing one simple Gatt write and indication listener test
 *
 * * Uses pre-set `-dev LabPad` device name and SMPIOCapability::KEYBOARD_ONLY + BTSecurityLevel::ENC_AUTH
 *
 * * Commandline `-passkey <int>` uses `LabPad` implicitly, i.e. user only needs to pass the integer w/o device name.
 *
 * Other than that, please refer to _dbt_scanner10_ as a general example.
 */
public class DBTLabPadClient01 {
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

    // Default from dbt_peripheral00.cpp or DBTPeripheral00.java
    String cmd_uuid = null; // "d0ca6bf3-3d52-4760-98e5-fc5883e93712";
    String cmd_rsp_uuid = null; // "d0ca6bf3-3d53-4760-98e5-fc5883e93712";
    byte cmd_arg = (byte)0x44;

    boolean SHOW_UPDATE_EVENTS = false;
    boolean QUIET = false;

    int shutdownTest = 0;

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

    private final DBGattServer dbGattServer = new DBGattServer(
            /* services: */
            Arrays.asList( // DBGattService
              new DBGattService ( true /* primary */,
                  DBGattService.UUID16.GENERIC_ACCESS /* type_ */,
                  Arrays.asList( // DBGattChar
                      new DBGattChar( DBGattChar.UUID16.DEVICE_NAME /* value_type_ */,
                                  new GattCharPropertySet(GattCharPropertySet.Type.Read),
                                  new ArrayList<DBGattDesc>(/* intentionally w/o Desc */ ),
                                  DBGattValue.make("Jausoft_Dev", 128) /* value */ ),
                      new DBGattChar( DBGattChar.UUID16.APPEARANCE /* value_type_ */,
                                  new GattCharPropertySet(GattCharPropertySet.Type.Read),
                                  new ArrayList<DBGattDesc>(/* intentionally w/o Desc */ ),
                                  DBGattValue.make((short)0) /* value */ )
                  ) ),
              new DBGattService ( true /* primary */,
                  DBGattService.UUID16.DEVICE_INFORMATION /* type_ */,
                  Arrays.asList( // DBGattChar
                      new DBGattChar( DBGattChar.UUID16.MANUFACTURER_NAME_STRING /* value_type_ */,
                                  new GattCharPropertySet(GattCharPropertySet.Type.Read),
                                  new ArrayList<DBGattDesc>(/* intentionally w/o Desc */ ),
                                  DBGattValue.make("Gothel Software") /* value */ ),
                      new DBGattChar( DBGattChar.UUID16.MODEL_NUMBER_STRING /* value_type_ */,
                                  new GattCharPropertySet(GattCharPropertySet.Type.Read),
                                  new ArrayList<DBGattDesc>(/* intentionally w/o Desc */ ),
                                  DBGattValue.make("2.4.0-pre") /* value */ ),
                      new DBGattChar( DBGattChar.UUID16.SERIAL_NUMBER_STRING /* value_type_ */,
                                  new GattCharPropertySet(GattCharPropertySet.Type.Read),
                                  new ArrayList<DBGattDesc>(/* intentionally w/o Desc */ ),
                                  DBGattValue.make("sn:0123456789") /* value */ ),
                      new DBGattChar( DBGattChar.UUID16.HARDWARE_REVISION_STRING /* value_type_ */,
                                  new GattCharPropertySet(GattCharPropertySet.Type.Read),
                                  new ArrayList<DBGattDesc>(/* intentionally w/o Desc */ ),
                                  DBGattValue.make("hw:0123456789") /* value */ ),
                      new DBGattChar( DBGattChar.UUID16.FIRMWARE_REVISION_STRING /* value_type_ */,
                                  new GattCharPropertySet(GattCharPropertySet.Type.Read),
                                  new ArrayList<DBGattDesc>(/* intentionally w/o Desc */ ),
                                  DBGattValue.make("fw:0123456789") /* value */ ),
                      new DBGattChar( DBGattChar.UUID16.SOFTWARE_REVISION_STRING /* value_type_ */,
                                  new GattCharPropertySet(GattCharPropertySet.Type.Read),
                                  new ArrayList<DBGattDesc>(/* intentionally w/o Desc */ ),
                                  DBGattValue.make("sw:0123456789") /* value */ )
                  ) )
            ) );

    class MyAdapterStatusListener extends AdapterStatusListener {
        @Override
        public void adapterSettingsChanged(final BTAdapter adapter, final AdapterSettings oldmask,
                                           final AdapterSettings newmask, final AdapterSettings changedmask, final long timestamp) {
            final boolean initialSetting = oldmask.isEmpty();
            if( initialSetting ) {
                PrintUtil.println(System.err, "****** SETTINGS: "+oldmask+" -> "+newmask+", initial "+changedmask);
            } else {
                PrintUtil.println(System.err, "****** SETTINGS: "+oldmask+" -> "+newmask+", changed "+changedmask);
            }
            PrintUtil.println(System.err, "Status Adapter:");
            PrintUtil.println(System.err, adapter.toString());
            if( !initialSetting &&
                changedmask.isSet(AdapterSettings.SettingType.POWERED) &&
                newmask.isSet(AdapterSettings.SettingType.POWERED) )
            {

                executeOffThread( () -> { startDiscovery(adapter, "powered-on"); },
                                  "DBT-StartDiscovery-"+adapter.getAddressAndType(), true /* detach */);
            }
        }

        @Override
        public void discoveringChanged(final BTAdapter adapter, final ScanType currentMeta, final ScanType changedType, final boolean changedEnabled, final DiscoveryPolicy policy, final long timestamp) {
            PrintUtil.println(System.err, "****** DISCOVERING: meta "+currentMeta+", changed["+changedType+", enabled "+changedEnabled+", policy "+policy+"] on "+adapter);
        }

        @Override
        public boolean deviceFound(final BTDevice device, final long timestamp) {
            if( BTDeviceRegistry.isWaitingForAnyDevice() ||
                ( BTDeviceRegistry.isWaitingForDevice(device.getAddressAndType().address, device.getName()) &&
                  ( 0 < MULTI_MEASUREMENTS.get() || !BTDeviceRegistry.isDeviceProcessed(device.getAddressAndType()) )
                )
              )
            {
                PrintUtil.println(System.err, "****** FOUND__-0: Connecting "+device.toString());
                {
                    final long td = Clock.currentTimeMillis() - timestamp_t0; // adapter-init -> now
                    PrintUtil.println(System.err, "PERF: adapter-init -> FOUND__-0 " + td + " ms");
                }
                executeOffThread( () -> { connectDiscoveredDevice(device); },
                                  "DBT-Connect-"+device.getAddressAndType(), true /* detach */);
                return true;
            } else {
                if( !QUIET ) {
                    PrintUtil.println(System.err, "****** FOUND__-1: NOP "+device.toString());
                }
                return false;
            }
        }

        @Override
        public void deviceUpdated(final BTDevice device, final EIRDataTypeSet updateMask, final long timestamp) {
            if( !QUIET && SHOW_UPDATE_EVENTS ) {
                PrintUtil.println(System.err, "****** UPDATED: "+updateMask+" of "+device);
            }
        }

        @Override
        public void deviceConnected(final BTDevice device, final boolean discovered, final long timestamp) {
            PrintUtil.println(System.err, "****** CONNECTED (discovered "+discovered+"): "+device.toString());
        }

        @Override
        public void devicePairingState(final BTDevice device, final SMPPairingState state, final PairingMode mode, final long timestamp) {
            PrintUtil.println(System.err, "****** PAIRING_STATE: state "+state+", mode "+mode+": "+device);
            switch( state ) {
                case NONE:
                    // next: deviceReady(..)
                    break;
                case FAILED: {
                    final boolean res  = SMPKeyBin.remove(DBTConstants.CLIENT_KEY_PATH, device);
                    PrintUtil.println(System.err, "****** PAIRING_STATE: state "+state+"; Remove key file "+SMPKeyBin.getFilename(DBTConstants.CLIENT_KEY_PATH, device)+", res "+res);
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
                    final BTSecurityRegistry.Entry sec = BTSecurityRegistry.getStartOf(device.getAddressAndType().address, device.getName());
                    if( null != sec && sec.getPairingPasskey() != BTSecurityRegistry.NO_PASSKEY ) {
                        executeOffThread( () -> { device.setPairingPasskey( sec.getPairingPasskey() ); }, "DBT-SetPasskey-"+device.getAddressAndType(), true /* detach */);
                    } else {
                        executeOffThread( () -> { device.setPairingPasskey( 0 ); }, "DBT-SetPasskey-"+device.getAddressAndType(), true /* detach */);
                        // 3s disconnect: executeOffThread( () -> { device.setPairingPasskeyNegative(); }, "DBT-SetPasskeyNegative-"+device.getAddressAndType(), true /* detach */);
                    }
                    // next: KEY_DISTRIBUTION or FAILED
                  } break;
                case NUMERIC_COMPARE_EXPECTED: {
                    final BTSecurityRegistry.Entry sec = BTSecurityRegistry.getStartOf(device.getAddressAndType().address, device.getName());
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
            deviceReadyCount.incrementAndGet();
            PrintUtil.println(System.err, "****** READY-0: Processing["+deviceReadyCount.get()+"] "+device.toString());
            {
                final long td = Clock.currentTimeMillis() - timestamp_t0; // adapter-init -> now
                PrintUtil.println(System.err, "PERF: adapter-init -> READY-0 " + td + " ms");
            }
            processReadyDevice(device); // AdapterStatusListener::deviceReady() explicitly allows prolonged and complex code execution!
        }

        @Override
        public void deviceDisconnected(final BTDevice device, final HCIStatusCode reason, final short handle, final long timestamp) {
            PrintUtil.println(System.err, "****** DISCONNECTED: Reason "+reason+", old handle 0x"+Integer.toHexString(handle)+": "+device+" on "+device.getAdapter());

            if( REMOVE_DEVICE ) {
                executeOffThread( () -> { removeDevice(device); }, "DBT-Remove-"+device.getAddressAndType(), true /* detach */);
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

    class MyGATTEventListener extends BTGattCharListener {
        private final int i, j;

        public MyGATTEventListener(final int i_, final int j_) { i=i_; j=j_; }

        @Override
        public void notificationReceived(final BTGattChar charDecl,
                                         final byte[] value, final long timestamp) {
            final long tR = Clock.currentTimeMillis();
            PrintUtil.fprintf_td(System.err, "**[%02d.%02d] Characteristic-Notify: UUID %s, td %d ******\n",
                    i, j, charDecl.getUUID(), (tR-timestamp));
            PrintUtil.fprintf_td(System.err, "**[%02d.%02d]     Characteristic: %s ******\n", i, j, charDecl.toString());
            PrintUtil.fprintf_td(System.err, "**[%02d.%02d]     Value R: size %d, ro: %s ******\n", i, j, value.length, BasicTypes.bytesHexString(value, 0, -1, true));
            PrintUtil.fprintf_td(System.err, "**[%02d.%02d]     Value S: %s ******\n", i, j, BTUtils.decodeUTF8String(value, 0, value.length));

            shutdownTest();
        }

        @Override
        public void indicationReceived(final BTGattChar charDecl,
                                       final byte[] value, final long timestamp, final boolean confirmationSent) {
            final long tR = Clock.currentTimeMillis();
            PrintUtil.fprintf_td(System.err, "**[%02d.%02d] Characteristic-Indication: UUID %s, td %d, confirmed %b ******\n",
                    i, j, charDecl.getUUID(), (tR-timestamp), confirmationSent);
            PrintUtil.fprintf_td(System.err, "**[%02d.%02d]     Characteristic: %s ******\n", i, j, charDecl.toString());
            PrintUtil.fprintf_td(System.err, "**[%02d.%02d]     Value R: size %d, ro: %s ******\n", i, j, value.length, BasicTypes.bytesHexString(value, 0, -1, true));
            PrintUtil.fprintf_td(System.err, "**[%02d.%02d]     Value S: %s ******\n", i, j, BTUtils.decodeUTF8String(value, 0, value.length));

            shutdownTest();
        }
    }

    private void connectDiscoveredDevice(final BTDevice device) {
        PrintUtil.println(System.err, "****** Connecting Device: Start " + device.toString());

        // Testing listener lifecycle @ device dtor
        device.addStatusListener(new AdapterStatusListener() {
            @Override
            public void deviceUpdated(final BTDevice device, final EIRDataTypeSet updateMask, final long timestamp) {
                if( SHOW_UPDATE_EVENTS ) {
                    PrintUtil.println(System.err, "****** UPDATED(2): "+updateMask+" of "+device);
                }
            }
            @Override
            public void deviceConnected(final BTDevice device, final boolean discovered, final long timestamp) {
                PrintUtil.println(System.err, "****** CONNECTED(2) (discovered "+discovered+"): "+device.toString());
            }

            @Override
            public String toString() {
                return "TempAdapterStatusListener[user, device "+device.getAddressAndType().toString()+"]";
            } });

        final BTSecurityRegistry.Entry sec = BTSecurityRegistry.getStartOf(device.getAddressAndType().address, device.getName());
        if( null != sec ) {
            PrintUtil.println(System.err, "****** Connecting Device: Found SecurityDetail "+sec.toString()+" for "+device.toString());
        } else {
            PrintUtil.println(System.err, "****** Connecting Device: No SecurityDetail for "+device.toString());
        }
        final BTSecurityLevel req_sec_level = null != sec ? sec.getSecLevel() : BTSecurityLevel.UNSET;
        HCIStatusCode res = device.uploadKeys(DBTConstants.CLIENT_KEY_PATH, req_sec_level, true /* verbose_ */);
        PrintUtil.fprintf_td(System.err, "****** Connecting Device: BTDevice::uploadKeys(...) result %s\n", res.toString());
        if( HCIStatusCode.SUCCESS != res ) {
            if( null != sec ) {
                if( sec.isSecurityAutoEnabled() ) {
                    final boolean r = device.setConnSecurityAuto( sec.getSecurityAutoIOCap() );
                    PrintUtil.println(System.err, "****** Connecting Device: Using SecurityDetail.SEC AUTO "+sec+" -> set OK "+r);
                } else if( sec.isSecLevelOrIOCapSet() ) {
                    final boolean r = device.setConnSecurity(sec.getSecLevel(), sec.getIOCap());
                    PrintUtil.println(System.err, "****** Connecting Device: Using SecurityDetail.Level+IOCap "+sec+" -> set OK "+r);
                } else {
                    final boolean r = device.setConnSecurityAuto( SMPIOCapability.KEYBOARD_ONLY );
                    PrintUtil.println(System.err, "****** Connecting Device: Setting SEC AUTO security detail w/ KEYBOARD_ONLY ("+sec+") -> set OK "+r);
                }
            } else {
                final boolean r = device.setConnSecurityAuto( SMPIOCapability.KEYBOARD_ONLY );
                PrintUtil.println(System.err, "****** Connecting Device: Setting SEC AUTO security detail w/ KEYBOARD_ONLY -> set OK "+r);
            }
        }
        final EInfoReport eir = device.getEIR();
        PrintUtil.println(System.err, "EIR-1 "+device.getEIRInd().toString());
        PrintUtil.println(System.err, "EIR-2 "+device.getEIRScanRsp().toString());
        PrintUtil.println(System.err, "EIR-+ "+eir.toString());

        short conn_interval_min  = (short)8;  // 10ms
        short conn_interval_max  = (short)12; // 15ms
        final short conn_latency  = (short)0;
        if( eir.isSet(EIRDataTypeSet.DataType.CONN_IVAL) ) {
            final short[] minmax = new short[2];
            eir.getConnInterval(minmax);
            conn_interval_min = minmax[0];
            conn_interval_max = minmax[1];
        }
        final short supervision_timeout = BTUtils.getHCIConnSupervisorTimeout(conn_latency, (int) ( conn_interval_max * 1.25 ) /* ms */);
        res = device.connectLE(le_scan_interval, le_scan_window, conn_interval_min, conn_interval_max, conn_latency, supervision_timeout);
        PrintUtil.println(System.err, "****** Connecting Device Command, res "+res+": End result "+res+" of " + device.toString());
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
        PrintUtil.println(System.err, "****** Processing Ready Device: Start " + device.toString());
        final long t1 = Clock.currentTimeMillis();

        SMPKeyBin.createAndWrite(device, DBTConstants.CLIENT_KEY_PATH, true /* verbose */);
        final long t2 = Clock.currentTimeMillis();

        boolean success = false;

        if( device.getAdapter().getBTMajorVersion() > 4 ) {
            final LE_PHYs Tx = new LE_PHYs(LE_PHYs.PHY.LE_2M);
            final LE_PHYs Rx = new LE_PHYs(LE_PHYs.PHY.LE_2M);

            final HCIStatusCode res = device.setConnectedLE_PHY(Tx, Rx);
            PrintUtil.fprintf_td(System.err, "****** Set Connected LE PHY: status %s: Tx %s, Rx %s\n",
                                res.toString(), Tx.toString(), Rx.toString());
        }
        {
            final LE_PHYs resTx[] = { new LE_PHYs() };
            final LE_PHYs resRx[] = { new LE_PHYs() };
            final HCIStatusCode res = device.getConnectedLE_PHY(resTx, resRx);
            PrintUtil.fprintf_td(System.err, "****** Got Connected LE PHY: status %s: Tx %s, Rx %s\n",
                    res.toString(), resTx[0].toString(), resRx[0].toString());
        }
        final long t3 = Clock.currentTimeMillis();

        //
        // GATT Service Processing
        //
        try {
            final List<BTGattService> primServices = device.getGattServices();
            if( null == primServices || 0 == primServices.size() ) {
                // Cheating the flow, but avoiding: goto, do-while-false and lastly unreadable intendations
                // And it is an error case nonetheless ;-)
                throw new RuntimeException("Processing Ready Device: getServices() failed " + device.toString());
            }
            final long t5 = Clock.currentTimeMillis();
            {
                final long td00 = device.getLastDiscoveryTimestamp() - timestamp_t0; // adapter-init to discovered
                final long td01 = t1 - timestamp_t0; // adapter-init to processing-start
                final long td05 = t5 - timestamp_t0; // adapter-init -> gatt-complete
                final long tdc1 = t1 - device.getLastDiscoveryTimestamp(); // discovered to processing-start
                final long tdc5 = t5 - device.getLastDiscoveryTimestamp(); // discovered to gatt-complete
                final long td12 = t2 - t1; // SMPKeyBin
                final long td23 = t3 - t2; // LE_PHY
                final long td13 = t3 - t1; // SMPKeyBin + LE_PHY
                final long td35 = t5 - t3; // get-gatt-services
                PrintUtil.println(System.err, System.lineSeparator()+System.lineSeparator());
                PrintUtil.println(System.err, "PERF: GATT primary-services completed"+System.lineSeparator()+
                        "PERF:  adapter-init to discovered " + td00 + " ms,"+System.lineSeparator()+
                        "PERF:  adapter-init to processing-start " + td01 + " ms,"+System.lineSeparator()+
                        "PERF:  adapter-init to gatt-complete " + td05 + " ms,"+System.lineSeparator()+
                        "PERF:  discovered to processing-start " + tdc1 + " ms,"+System.lineSeparator()+
                        "PERF:  discovered to gatt-complete " + tdc5 + " ms,"+System.lineSeparator()+
                        "PERF:  SMPKeyBin + LE_PHY " + td13 + " ms (SMPKeyBin "+td12+"ms, LE_PHY "+td23+"ms), "+System.lineSeparator()+
                        "PERF:  get-gatt-services " + td35 + " ms,"+System.lineSeparator());
            }

            if( null != cmd_uuid ) {
                final BTGattCmd cmd = null != cmd_rsp_uuid ? new BTGattCmd(device, "TestCmd", null /* service_uuid */, cmd_uuid, cmd_rsp_uuid)
                                                           : new BTGattCmd(device, "TestCmd", null /* service_uuid */, cmd_uuid);
                cmd.setVerbose(true);
                final boolean cmd_resolved = cmd.isResolved();
                PrintUtil.println(System.err, "Command test: "+cmd.toString()+", resolved "+cmd_resolved);
                final byte[] cmd_data = { cmd_arg };
                final HCIStatusCode cmd_res = cmd.send(true /* prefNoAck */, cmd_data, 3000 /* timeoutMS */);
                if( HCIStatusCode.SUCCESS == cmd_res ) {
                    if( cmd.hasResponseSet() ) {
                        final byte[] resp = cmd.getResponse();
                        if( 1 == resp.length && resp[0] == cmd_arg ) {
                            PrintUtil.fprintf_td(System.err, "Success: %s -> %s (echo response)\n", cmd.toString(), BasicTypes.bytesHexString(resp, 0, resp.length, true /* lsb */));
                        } else {
                            PrintUtil.fprintf_td(System.err, "Success: %s -> %s (different response)\n", cmd.toString(), BasicTypes.bytesHexString(resp, 0, resp.length, true /* lsb */));
                        }
                    } else {
                        PrintUtil.fprintf_td(System.err, "Success: %s -> no response\n", cmd.toString());
                    }
                } else {
                    PrintUtil.fprintf_td(System.err, "Failure: %s -> %s\n", cmd.toString(), cmd_res.toString());
                }
                cmd.close();
            }

            try {
                int i=0;
                for(final Iterator<BTGattService> srvIter = primServices.iterator(); srvIter.hasNext(); i++) {
                    final BTGattService primService = srvIter.next();
                    {
                        PrintUtil.fprintf_td(System.err, "  [%02d] Service UUID %s\n", i, primService.getUUID());
                        PrintUtil.fprintf_td(System.err, "  [%02d]         %s\n", i, primService.toString());
                    }
                    int j=0;
                    final List<BTGattChar> serviceCharacteristics = primService.getChars();
                    for(final Iterator<BTGattChar> charIter = serviceCharacteristics.iterator(); charIter.hasNext(); j++) {
                        final BTGattChar serviceChar = charIter.next();
                        {
                            PrintUtil.fprintf_td(System.err, "  [%02d.%02d] Characteristic: UUID %s\n", i, j, serviceChar.getUUID());
                            PrintUtil.fprintf_td(System.err, "  [%02d.%02d]     %s\n", i, j, serviceChar.toString());
                        }
                        final GattCharPropertySet properties = serviceChar.getProperties();
                        if( properties.isSet(GattCharPropertySet.Type.Read) ) {
                            final byte[] value = serviceChar.readValue();
                            final String svalue = BTUtils.decodeUTF8String(value, 0, value.length);
                            {
                                PrintUtil.fprintf_td(System.err, "  [%02d.%02d]     value: %s ('%s')\n", i, j, BasicTypes.bytesHexString(value, 0, -1, true), svalue);
                            }
                        }
                        int k=0;
                        final List<BTGattDesc> charDescList = serviceChar.getDescriptors();
                        for(final Iterator<BTGattDesc> descIter = charDescList.iterator(); descIter.hasNext(); k++) {
                            final BTGattDesc charDesc = descIter.next();
                            {
                                PrintUtil.fprintf_td(System.err, "  [%02d.%02d.%02d] Descriptor: UUID %s\n", i, j, k, charDesc.getUUID());
                                PrintUtil.fprintf_td(System.err, "  [%02d.%02d.%02d]     %s\n", i, j, k, charDesc.toString());
                            }
                        }
                        final boolean cccdEnableResult[] = { false, false };
                        if( serviceChar.enableNotificationOrIndication( cccdEnableResult ) ) {
                            // ClientCharConfigDescriptor (CCD) is available
                            final boolean clAdded = serviceChar.addCharListener( new MyGATTEventListener(i, j) );
                            {
                                PrintUtil.fprintf_td(System.err, "  [%02d.%02d] Characteristic-Listener: Notification(%b), Indication(%b): Added %b\n",
                                        i, j, cccdEnableResult[0], cccdEnableResult[1], clAdded);
                                PrintUtil.fprintf_td(System.err, "\n");
                            }
                        }
                    }
                    PrintUtil.fprintf_td(System.err, "\n");
                }
            } catch( final Exception ex) {
                PrintUtil.println(System.err, "Caught "+ex.getMessage());
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
            PrintUtil.println(System.err, "****** Processing Ready Device: Exception caught for " + device.toString() + ": "+t.getMessage());
            t.printStackTrace();
        }

        PrintUtil.println(System.err, "****** Processing Ready Device: End-1: Success " + success + " on " + device.toString());

        if( DiscoveryPolicy.PAUSE_CONNECTED_UNTIL_DISCONNECTED == discoveryPolicy ) {
            device.getAdapter().removeDevicePausingDiscovery(device);
        }

        if( KEEP_CONNECTED && GATT_PING_ENABLED && success ) {
            while( device.pingGATT() ) {
                PrintUtil.println(System.err, "****** Processing Ready Device: pingGATT OK: "+device.getAddressAndType());
                try {
                    Thread.sleep(1000);
                } catch (final InterruptedException e) {
                    e.printStackTrace();
                }
            }
            PrintUtil.println(System.err, "****** Processing Ready Device: pingGATT failed, waiting for disconnect: "+device.getAddressAndType());
            // Even w/ GATT_PING_ENABLED, we utilize disconnect event to clean up -> remove
        }

        PrintUtil.println(System.err, "****** Processing Ready Device: End-2: Success " + success + " on " + device.toString());
        if( success ) {
            BTDeviceRegistry.addToProcessedDevices(device.getAddressAndType(), device.getName());
        }
        device.removeAllCharListener();

        if( !KEEP_CONNECTED ) {

            device.remove();

            if( 0 < RESET_ADAPTER_EACH_CONN && 0 == deviceReadyCount.get() % RESET_ADAPTER_EACH_CONN ) {
                resetAdapter(device.getAdapter(), 2);
            }
        }

        if( 0 < MULTI_MEASUREMENTS.get() ) {
            MULTI_MEASUREMENTS.decrementAndGet();
            PrintUtil.println(System.err, "****** Processing Ready Device: MULTI_MEASUREMENTS left "+MULTI_MEASUREMENTS.get()+": "+device.getAddressAndType());
        }
    }

    private void removeDevice(final BTDevice device) {
        PrintUtil.println(System.err, "****** Remove Device: removing: "+device.getAddressAndType());

        device.remove();
    }

    private void resetAdapter(final BTAdapter adapter, final int mode) {
        PrintUtil.println(System.err, "****** Reset Adapter: reset["+mode+"] start: "+adapter.toString());
        final HCIStatusCode res = adapter.reset();
        PrintUtil.println(System.err, "****** Reset Adapter: reset["+mode+"] end: "+res+", "+adapter.toString());
    }

    DiscoveryPolicy discoveryPolicy = DiscoveryPolicy.PAUSE_CONNECTED_UNTIL_READY; // default value
    boolean le_scan_active = true; // default value
    static final short le_scan_interval = (short)24; // default value
    static final short le_scan_window = (short)24; // default value
    static final byte filter_policy = (byte)0; // default value
    static final boolean filter_dup = true; // default value

    @SuppressWarnings("unused")
    private boolean startDiscovery(final BTAdapter adapter, final String msg) {
        if( !useAdapter.equals(EUI48.ALL_DEVICE) && !useAdapter.equals(adapter.getAddressAndType().address) ) {
            PrintUtil.fprintf_td(System.err, "****** Start discovery (%s): Adapter not selected: %s\n", msg, adapter.toString());
            return false;
        }

        if( false ) {
            final DBGattChar gattDevNameChar = dbGattServer.findGattChar(DBGattService.UUID16.GENERIC_ACCESS, DBGattChar.UUID16.DEVICE_NAME);
            if( null != gattDevNameChar ) {
                final byte[] aname_bytes = adapter.getName().getBytes(StandardCharsets.UTF_8);
                gattDevNameChar.setValue(aname_bytes, 0, aname_bytes.length, 0);
            }
        }

        final HCIStatusCode status = adapter.startDiscovery(dbGattServer, discoveryPolicy, le_scan_active, le_scan_interval, le_scan_window, filter_policy, filter_dup );
        PrintUtil.println(System.err, "****** Start discovery ("+msg+") result: "+status);
        PrintUtil.println(System.err, dbGattServer.toFullString());
        return HCIStatusCode.SUCCESS == status;
    }

    private boolean initAdapter(final BTAdapter adapter) {
        if( !useAdapter.equals(EUI48.ALL_DEVICE) && !useAdapter.equals(adapter.getAddressAndType().address) ) {
            PrintUtil.fprintf_td(System.err, "initAdapter: Adapter not selected: %s\n", adapter.toString());
            return false;
        }
        // Initialize with defaults and power-on
        if( !adapter.isInitialized() ) {
            final HCIStatusCode status = adapter.initialize( btMode, true );
            if( HCIStatusCode.SUCCESS != status ) {
                PrintUtil.fprintf_td(System.err, "initAdapter: Adapter initialization failed: %s: %s\n",
                        status.toString(), adapter.toString());
                return false;
            }
        } else if( !adapter.setPowered( true ) ) {
            PrintUtil.fprintf_td(System.err, "initAdapter: Already initialized adapter power-on failed:: %s\n", adapter.toString());
            return false;
        }
        // adapter is powered-on
        PrintUtil.fprintf_td(System.err, "initAdapter: %s\n", adapter.toString());
        {
            final LE_Features le_feats = adapter.getLEFeatures();
            PrintUtil.fprintf_td(System.err, "initAdapter: LE_Features %s\n", le_feats.toString());
        }
        if( adapter.getBTMajorVersion() > 4 ) {
            final LE_PHYs Tx = new LE_PHYs(LE_PHYs.PHY.LE_2M);
            final LE_PHYs Rx = new LE_PHYs(LE_PHYs.PHY.LE_2M);

            final HCIStatusCode res = adapter.setDefaultLE_PHY(Tx, Rx);
            PrintUtil.fprintf_td(System.err, "initAdapter: Set Default LE PHY: status %s: Tx %s, Rx %s\n",
                                res.toString(), Tx.toString(), Rx.toString());
        }
        final AdapterStatusListener asl = new MyAdapterStatusListener();
        adapter.addStatusListener( asl );

        if( !startDiscovery(adapter, "initAdapter") ) {
            adapter.removeStatusListener( asl );
            return false;
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
                            PrintUtil.println(System.err, "****** Adapter ADDED__: InitOK: " + adapter);
                        } else {
                            PrintUtil.println(System.err, "****** Adapter ADDED__: Ignored: " + adapter);
                        }
                    } else {
                        PrintUtil.println(System.err, "****** Adapter ADDED__: Ignored (other): " + adapter);
                    }
                }

                @Override
                public void adapterRemoved(final BTAdapter adapter) {
                    if( null != chosenAdapter && adapter == chosenAdapter ) {
                        chosenAdapter = null;
                        PrintUtil.println(System.err, "****** Adapter REMOVED: " + adapter);
                    } else {
                        PrintUtil.println(System.err, "****** Adapter REMOVED (other): " + adapter);
                    }
                }
    };

    public void runTest(final BTManager manager) {
        timestamp_t0 = Clock.currentTimeMillis();

        boolean done = false;

        manager.addChangedAdapterSetListener(myChangedAdapterSetListener);

        while( !done ) {
            if( 0 == MULTI_MEASUREMENTS.get() ||
                ( -1 == MULTI_MEASUREMENTS.get() && !BTDeviceRegistry.isWaitingForAnyDevice() && BTDeviceRegistry.areAllDevicesProcessed() )
              )
            {
                PrintUtil.println(System.err, "****** EOL Test MULTI_MEASUREMENTS left "+MULTI_MEASUREMENTS.get()+
                                   ", processed "+BTDeviceRegistry.getProcessedDeviceCount()+"/"+BTDeviceRegistry.getWaitForDevicesCount());
                PrintUtil.println(System.err, "****** WaitForDevices "+BTDeviceRegistry.getWaitForDevicesString());
                PrintUtil.println(System.err, "****** DevicesProcessed "+BTDeviceRegistry.getProcessedDevicesString());
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
                PrintUtil.println(System.err, "****** EOL Adapter's Devices - pre_ close: " + a);
            } } );
        {
            final int count = manager.removeChangedAdapterSetListener(myChangedAdapterSetListener);
            PrintUtil.println(System.err, "****** EOL Removed ChangedAdapterSetCallback " + count);

            // All implicit via destructor or shutdown hook!
            manager.shutdown(); /* implies: adapter.close(); */
        }
        adapters.forEach(new Consumer<BTAdapter>() {
            @Override
            public void accept(final BTAdapter a) {
                PrintUtil.println(System.err, "****** EOL Adapter's Devices - post close: " + a);
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
        PrintUtil.println(System.err, "Direct-BT BluetoothManager initialized!");
        PrintUtil.println(System.err, "Direct-BT Native Version "+BTFactory.getNativeVersion()+" (API "+BTFactory.getNativeAPIVersion()+")");
        PrintUtil.println(System.err, "Direct-BT Java Version "+BTFactory.getImplVersion()+" (API "+BTFactory.getAPIVersion()+")");

        final DBTLabPadClient01 test = new DBTLabPadClient01();

        // Add defaults for Avalun's LabPad device, announcing its device name as 'LabPad[0-9]+'
        final String dev_name_prefix = "LabPad";
        {
            BTDeviceRegistry.addToWaitForDevices( dev_name_prefix );
            final BTSecurityRegistry.Entry sec = BTSecurityRegistry.getOrCreate(dev_name_prefix);
            sec.io_cap = SMPIOCapability.KEYBOARD_ONLY;
            sec.sec_level = BTSecurityLevel.ENC_AUTH;
        }

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
                } else if( arg.equals("-discoveryPolicy") && args.length > (i+1) ) {
                    test.discoveryPolicy = DiscoveryPolicy.get((byte) Integer.valueOf(args[++i]).intValue() );
                } else if( arg.equals("-scanPassive") ) {
                    test.le_scan_active = false;
                } else if( arg.equals("-btmode") && args.length > (i+1) ) {
                    test.btMode = BTMode.get(args[++i]);
                } else if( arg.equals("-adapter") && args.length > (i+1) ) {
                    test.useAdapter = new EUI48( args[++i] );
                } else if( arg.equals("-passkey") && args.length > (i+1) ) {
                    final BTSecurityRegistry.Entry sec = BTSecurityRegistry.getOrCreate(dev_name_prefix);
                    sec.passkey = Integer.valueOf(args[++i]).intValue();
                    System.err.println("Set passkey in "+sec);
                }
            }
            PrintUtil.println(System.err, "Run with '[-btmode LE|BREDR|DUAL] "+
                    "[-disconnect] [-show_update_events] [-quiet] "+
                    "[-discoveryPolicy <0-4>] "+
                    "[-scanPassive] "+
                    "[-adapter <adapter_address>] "+
                    "(-passkey <digits>)* "+
                    "[-verbose] [-debug] "+
                    "[-dbt_verbose true|false] "+
                    "[-dbt_debug true|false|adapter.event,gatt.data,hci.event,hci.scan_ad_eir,mgmt.event] "+
                    "[-dbt_mgmt cmd.timeout=3000,ringsize=64,...] "+
                    "[-dbt_hci cmd.complete.timeout=10000,cmd.status.timeout=3000,ringsize=64,...] "+
                    "[-dbt_gatt cmd.read.timeout=500,cmd.write.timeout=500,cmd.init.timeout=2500,ringsize=128,...] "+
                    "[-dbt_l2cap reader.timeout=10000,restart.count=0,...] ");
        }

        PrintUtil.println(System.err, "MULTI_MEASUREMENTS "+test.MULTI_MEASUREMENTS.get());
        PrintUtil.println(System.err, "KEEP_CONNECTED "+test.KEEP_CONNECTED);
        PrintUtil.println(System.err, "RESET_ADAPTER_EACH_CONN "+test.RESET_ADAPTER_EACH_CONN);
        PrintUtil.println(System.err, "GATT_PING_ENABLED "+test.GATT_PING_ENABLED);
        PrintUtil.println(System.err, "REMOVE_DEVICE "+test.REMOVE_DEVICE);
        PrintUtil.println(System.err, "SHOW_UPDATE_EVENTS "+test.SHOW_UPDATE_EVENTS);
        PrintUtil.println(System.err, "QUIET "+test.QUIET);
        PrintUtil.println(System.err, "adapter "+test.useAdapter);
        PrintUtil.println(System.err, "btmode "+test.btMode.toString());
        PrintUtil.println(System.err, "discoveryPolicy "+test.discoveryPolicy.toString());
        PrintUtil.println(System.err, "le_scan_active "+test.le_scan_active);
        PrintUtil.println(System.err, "security-details: "+BTSecurityRegistry.allToString() );
        PrintUtil.println(System.err, "waitForDevices: "+BTDeviceRegistry.getWaitForDevicesString());

        if( waitForEnter ) {
            PrintUtil.println(System.err, "Press ENTER to continue\n");
            try{ System.in.read();
            } catch(final Exception e) { }
        }
        test.runTest(manager);
    }
}
