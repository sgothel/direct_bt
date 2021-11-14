/**
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2021 Gothel Software e.K.
 * Copyright (c) 2021 ZAFENA AB
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

import org.direct_bt.AdapterSettings;
import org.direct_bt.AdapterStatusListener;
import org.direct_bt.BDAddressAndType;
import org.direct_bt.BTMode;
import org.direct_bt.BTAdapter;
import org.direct_bt.BTDevice;
import org.direct_bt.BTDeviceRegistry;
import org.direct_bt.BTException;
import org.direct_bt.BTFactory;
import org.direct_bt.BTManager;
import org.direct_bt.BTSecurityRegistry;
import org.direct_bt.BTUtils;
import org.direct_bt.DBGattChar;
import org.direct_bt.DBGattDesc;
import org.direct_bt.DBGattServer;
import org.direct_bt.DBGattService;
import org.direct_bt.DBGattValue;
import org.direct_bt.EIRDataTypeSet;
import org.direct_bt.GattCharPropertySet;
import org.direct_bt.HCIStatusCode;
import org.direct_bt.LE_Features;
import org.direct_bt.LE_PHYs;
import org.direct_bt.PairingMode;
import org.direct_bt.SMPKeyBin;
import org.direct_bt.SMPPairingState;
import org.direct_bt.ScanType;
import org.jau.net.EUI48;

/**
 * This Java peripheral {@link BTRole::Slave} example uses the Direct-BT fully event driven workflow.
 */
public class DBTPeripheral00 {
    long timestamp_t0;

    EUI48 useAdapter = EUI48.ALL_DEVICE;
    BTMode btMode = BTMode.DUAL;
    BTAdapter chosenAdapter = null;

    boolean use_SC = true;
    String adapter_name = "TestDev001_J";
    String adapter_short_name = "TDev001J";

    boolean SHOW_UPDATE_EVENTS = false;
    boolean RUN_ONLY_ONCE = false;

    volatile int servedConnections = 0;

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

    static Thread executeOffThread(final Runnable runobj, final String threadName, final boolean detach) {
        final Thread t = new Thread( runobj, threadName );
        if( detach ) {
            t.setDaemon(true); // detach thread
        }
        t.start();
        return t;
    }
    static Thread executeOffThread(final Runnable runobj, final boolean detach) {
        final Thread t = new Thread( runobj );
        if( detach ) {
            t.setDaemon(true); // detach thread
        }
        t.start();
        return t;
    }

    static DBGattValue make_gvalue(final String name) {
        final byte[] p = name.getBytes(StandardCharsets.UTF_8);
        return new DBGattValue(p, p.length);
    }
    static DBGattValue make_gvalue(final short v) {
        final byte[] p = { (byte)0, (byte)0 };
        p[0] = (byte)(v);
        p[1] = (byte)(v >> 8);
        return new DBGattValue(p, p.length);
    }
    static DBGattValue make_gvalue(final int capacity, final int size) {
        final byte[] p = new byte[size];
        return new DBGattValue(p, capacity, true /* variable_length */);
    }

    static String DataServiceUUID = "d0ca6bf3-3d50-4760-98e5-fc5883e93712";
    static String StaticDataUUID  = "d0ca6bf3-3d51-4760-98e5-fc5883e93712";
    static String CommandUUID     = "d0ca6bf3-3d52-4760-98e5-fc5883e93712";
    static String ResponseUUID    = "d0ca6bf3-3d53-4760-98e5-fc5883e93712";
    static String PulseDataUUID   = "d0ca6bf3-3d54-4760-98e5-fc5883e93712";

    // DBGattServerRef dbGattServer = std::make_shared<DBGattServer>(
    private final DBGattServer dbGattServer = new DBGattServer(
            /* services: */
            Arrays.asList( // DBGattService
              new DBGattService ( true /* primary */,
                  DBGattService.UUID16.GENERIC_ACCESS /* type_ */,
                  Arrays.asList( // DBGattChar
                      new DBGattChar( DBGattChar.UUID16.DEVICE_NAME /* value_type_ */,
                                  new GattCharPropertySet(GattCharPropertySet.Type.Read),
                                  new ArrayList<DBGattDesc>(/* intentionally w/o Desc */ ),
                                  make_gvalue("Synthethic Sensor 01") /* value */ ),
                      new DBGattChar( DBGattChar.UUID16.APPEARANCE /* value_type_ */,
                                  new GattCharPropertySet(GattCharPropertySet.Type.Read),
                                  new ArrayList<DBGattDesc>(/* intentionally w/o Desc */ ),
                                  make_gvalue((short)0) /* value */ )
                  ) ),
              new DBGattService ( true /* primary */,
                  DBGattService.UUID16.DEVICE_INFORMATION /* type_ */,
                  Arrays.asList( // DBGattChar
                      new DBGattChar( DBGattChar.UUID16.MANUFACTURER_NAME_STRING /* value_type_ */,
                                  new GattCharPropertySet(GattCharPropertySet.Type.Read),
                                  new ArrayList<DBGattDesc>(/* intentionally w/o Desc */ ),
                                  make_gvalue("Gothel Software") /* value */ ),
                      new DBGattChar( DBGattChar.UUID16.MODEL_NUMBER_STRING /* value_type_ */,
                                  new GattCharPropertySet(GattCharPropertySet.Type.Read),
                                  new ArrayList<DBGattDesc>(/* intentionally w/o Desc */ ),
                                  make_gvalue("2.4.0-pre") /* value */ ),
                      new DBGattChar( DBGattChar.UUID16.SERIAL_NUMBER_STRING /* value_type_ */,
                                  new GattCharPropertySet(GattCharPropertySet.Type.Read),
                                  new ArrayList<DBGattDesc>(/* intentionally w/o Desc */ ),
                                  make_gvalue("sn:0123456789") /* value */ ),
                      new DBGattChar( DBGattChar.UUID16.HARDWARE_REVISION_STRING /* value_type_ */,
                                  new GattCharPropertySet(GattCharPropertySet.Type.Read),
                                  new ArrayList<DBGattDesc>(/* intentionally w/o Desc */ ),
                                  make_gvalue("hw:0123456789") /* value */ ),
                      new DBGattChar( DBGattChar.UUID16.FIRMWARE_REVISION_STRING /* value_type_ */,
                                  new GattCharPropertySet(GattCharPropertySet.Type.Read),
                                  new ArrayList<DBGattDesc>(/* intentionally w/o Desc */ ),
                                  make_gvalue("fw:0123456789") /* value */ ),
                      new DBGattChar( DBGattChar.UUID16.SOFTWARE_REVISION_STRING /* value_type_ */,
                                  new GattCharPropertySet(GattCharPropertySet.Type.Read),
                                  new ArrayList<DBGattDesc>(/* intentionally w/o Desc */ ),
                                  make_gvalue("sw:0123456789") /* value */ )
                  ) ),
              new DBGattService ( true /* primary */,
                  DataServiceUUID /* type_ */,
                  Arrays.asList( // DBGattChar
                      new DBGattChar( StaticDataUUID /* value_type_ */,
                                  new GattCharPropertySet(GattCharPropertySet.Type.Read),
                                  Arrays.asList( // DBGattDesc
                                      new DBGattDesc( DBGattDesc.UUID16.USER_DESC, make_gvalue("DATA_STATIC") )
                                  ),
                                make_gvalue("Proprietary Static Data 0x00010203") /* value */ ),
                      new DBGattChar( CommandUUID /* value_type_ */,
                                  new GattCharPropertySet(GattCharPropertySet.Type.WriteNoAck).set(GattCharPropertySet.Type.WriteWithAck),
                                  Arrays.asList( // DBGattDesc
                                      new DBGattDesc( DBGattDesc.UUID16.USER_DESC, make_gvalue("COMMAND") )
                                  ),
                                  make_gvalue(128, 64) /* value */ ),
                      new DBGattChar( ResponseUUID /* value_type_ */,
                                  new GattCharPropertySet(GattCharPropertySet.Type.Notify).set(GattCharPropertySet.Type.Indicate),
                                  Arrays.asList( // DBGattDesc
                                      new DBGattDesc( DBGattDesc.UUID16.USER_DESC, make_gvalue("RESPONSE") ),
                                      DBGattDesc.createClientCharConfig()
                                  ),
                                  make_gvalue((short)0) /* value */ ),
                      new DBGattChar( PulseDataUUID /* value_type_ */,
                                  new GattCharPropertySet(GattCharPropertySet.Type.Notify).set(GattCharPropertySet.Type.Indicate),
                                  Arrays.asList( // DBGattDesc
                                      new DBGattDesc( DBGattDesc.UUID16.USER_DESC, make_gvalue("DATA_PULSE") ),
                                      DBGattDesc.createClientCharConfig()
                                  ),
                                  make_gvalue("Synthethic Sensor 01") /* value */ )
                  ) )
            ) );


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

                executeOffThread( () -> { startAdvertising(adapter, "powered-on"); },
                                  "DBT-StartAdvertising-"+adapter.getAddressAndType(), true /* detach */);
            }
        }

        @Override
        public void discoveringChanged(final BTAdapter adapter, final ScanType currentMeta, final ScanType changedType, final boolean changedEnabled, final boolean keepAlive, final long timestamp) {
            BTUtils.println(System.err, "****** DISCOVERING: meta "+currentMeta+", changed["+changedType+", enabled "+changedEnabled+", keepAlive "+keepAlive+"] on "+adapter);
        }

        @Override
        public boolean deviceFound(final BTDevice device, final long timestamp) {
            BTUtils.println(System.err, "****** FOUND__-1: NOP "+device.toString());
            return false;
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
            BTUtils.println(System.err, "****** READY-1: NOP " + device.toString());
        }

        @Override
        public void deviceDisconnected(final BTDevice device, final HCIStatusCode reason, final short handle, final long timestamp) {
            servedConnections++;
            BTUtils.println(System.err, "****** DISCONNECTED (count "+servedConnections+"): Reason "+reason+", old handle 0x"+Integer.toHexString(handle)+": "+device+" on "+device.getAdapter());

            executeOffThread( () -> { processDisconnectedDevice(device); },
                              "DBT-Disconnected-"+device.getAdapter().getAddressAndType(), true /* detach */);
        }

        @Override
        public String toString() {
            return "AdapterStatusListener[user, per-adapter]";
        }
    };

    class MyGATTServerListener extends DBGattServer.Listener implements AutoCloseable {
        private volatile boolean sync_data;
        private final Thread pulseSenderThread;
        private volatile boolean stopPulseSender = false;

        private volatile short handlePulseDataNotify = 0;
        private volatile short handlePulseDataIndicate = 0;
        private volatile short handleResponseDataNotify = 0;
        private volatile short handleResponseDataIndicate = 0;

        private BTDevice connectedDevice;
        private int usedMTU = 23; // BTGattHandler::number(BTGattHandler::Defaults::MIN_ATT_MTU);

        private boolean matches(final BTDevice device) {
            final boolean local = sync_data; // SC-DRF acquire via sc_atomic_bool::load()
            final boolean res = null != connectedDevice ? connectedDevice.equals(device) : false;
            sync_data = local; // SC-DRF release via sc_atomic_bool::store()
            return res;
        }

        private void clear() {
            final boolean local = sync_data; // SC-DRF acquire via sc_atomic_bool::load()

            handlePulseDataNotify = 0;
            handlePulseDataIndicate = 0;
            handleResponseDataNotify = 0;
            handleResponseDataIndicate = 0;
            connectedDevice = null;

            dbGattServer.resetGattClientCharConfig(DataServiceUUID, PulseDataUUID);
            dbGattServer.resetGattClientCharConfig(DataServiceUUID, ResponseUUID);

            sync_data = local; // SC-DRF release via sc_atomic_bool::store()
        }

        private void pulseSender() {
            while( !stopPulseSender ) {
                {
                    final boolean local = sync_data; // SC-DRF acquire via sc_atomic_bool::load()
                    if( null != connectedDevice && connectedDevice.getConnected() ) {
                        if( 0 != handlePulseDataNotify || 0 != handlePulseDataIndicate ) {
                            final String data = String.format("Dynamic Data Example. Elapsed Milliseconds: %,9d", BTUtils.elapsedTimeMillis());
                            final byte[] v = data.getBytes(StandardCharsets.UTF_8);
                            if( 0 != handlePulseDataNotify ) {
                                connectedDevice.sendNotification(handlePulseDataNotify, v);
                            }
                            if( 0 != handlePulseDataIndicate ) {
                                connectedDevice.sendIndication(handlePulseDataNotify, v);
                            }
                        }
                    }
                    sync_data = local; // SC-DRF release via sc_atomic_bool::store()
                }
                try {
                    Thread.sleep(500); // 500ms
                } catch (final InterruptedException e) { }
            }
        }

        void sendResponse(final byte[] data) {
            if( null != connectedDevice && connectedDevice.getConnected() ) {
                if( 0 != handleResponseDataNotify || 0 != handleResponseDataIndicate ) {
                    if( 0 != handleResponseDataNotify ) {
                        connectedDevice.sendNotification(handleResponseDataNotify, data);
                    }
                    if( 0 != handleResponseDataIndicate ) {
                        connectedDevice.sendIndication(handleResponseDataNotify, data);
                    }
                }
            }
        }

        public MyGATTServerListener() {
            pulseSenderThread = executeOffThread( () -> { pulseSender(); }, "GattServer-PulseSender", false /* detach */);
        }

        @Override
        public void finalize() { close(); }

        @Override
        public void close() {
            {
                final boolean local = sync_data; // SC-DRF acquire via sc_atomic_bool::load()
                stopPulseSender = true;
                connectedDevice = null;
                sync_data = local; // SC-DRF release via sc_atomic_bool::store()
            }
            if( !pulseSenderThread.isDaemon() ) {
                try {
                    pulseSenderThread.join(1000);
                } catch (final InterruptedException e) { }
            }
        }

        @Override
        public void connected(final BTDevice device, final int initialMTU) {
            final boolean local = sync_data; // SC-DRF acquire via sc_atomic_bool::load()
            final boolean available = null == connectedDevice;
            BTUtils.fprintf_td(System.err, "****** GATT::connected(available %b): initMTU %d, %s\n",
                    available, initialMTU, device.toString());
            if( available ) {
                connectedDevice = device;
                usedMTU = initialMTU;
            }
            sync_data = local; // SC-DRF release via sc_atomic_bool::store()
        }

        @Override
        public void disconnected(final BTDevice device) {
            final boolean local = sync_data; // SC-DRF acquire via sc_atomic_bool::load()
            final boolean match = null != connectedDevice ? connectedDevice.equals(device) : false;
            BTUtils.fprintf_td(System.err, "****** GATT::disconnected(match %b): %s\n", match, device.toString());
            if( match ) {
                clear();
            }
            sync_data = local; // SC-DRF release via sc_atomic_bool::store()
        }

        @Override
        public void mtuChanged(final BTDevice device, final int mtu) {
            final boolean match = matches(device);
            BTUtils.fprintf_td(System.err, "****** GATT::mtuChanged(match %b): %d -> %d, %s\n",
                    match, match ? (int)usedMTU : 0, mtu, device.toString());
            if( match ) {
                usedMTU = mtu;
            }
        }

        @Override
        public boolean readCharValue(final BTDevice device, final DBGattService s, final DBGattChar c) {
            final boolean match = matches(device);
            BTUtils.fprintf_td(System.err, "****** GATT::readCharValue(match %b): to %s, from\n  %s\n    %s\n",
                    match, device.toString(), s.toString(), c.toString());
            return match;
        }

        @Override
        public boolean readDescValue(final BTDevice device, final DBGattService s, final DBGattChar c, final DBGattDesc d) {
            final boolean match = matches(device);
            BTUtils.fprintf_td(System.err, "****** GATT::readDescValue(match %b): to %s, from\n  %s\n    %s\n      %s\n",
                    match, device.toString(), s.toString(), c.toString(), d.toString());
            return match;
        }

        @Override
        public boolean writeCharValue(final BTDevice device, final DBGattService s, final DBGattChar c, final byte[] value, final int value_offset) {
            final boolean match = matches(device);
            BTUtils.fprintf_td(System.err, "****** GATT::writeCharValue(match %b): %s @ 0x%s from %s, to\n  %s\n    %s\n",
                    match, value.toString(), Integer.toHexString(value_offset),
                    device.toString(), s.toString(), c.toString());

            if( match &&
                c.getValueType().equals( CommandUUID ) &&
                ( 0 != handleResponseDataNotify || 0 != handleResponseDataIndicate ) )
            {
                executeOffThread( () -> { sendResponse(value); }, true /* detach */);
            }
            return match;
        }

        @Override
        public boolean writeDescValue(final BTDevice device, final DBGattService s, final DBGattChar c, final DBGattDesc d,
                                      final byte[] value, final int value_offset)
        {
            final boolean match = matches(device);
            BTUtils.fprintf_td(System.err, "****** GATT::writeDescValue(match %b): %s @ 0x%s from %s\n  %s\n    %s\n      %s\n",
                    match, value.toString(), Integer.toHexString(value_offset),
                    device.toString(), s.toString(), c.toString(), d.toString());
            return match;
        }

        @Override
        public void clientCharConfigChanged(final BTDevice device, final DBGattService s, final DBGattChar c, final DBGattDesc d,
                                            final boolean notificationEnabled, final boolean indicationEnabled)
        {
            final boolean match = matches(device);
            BTUtils.fprintf_td(System.err, "****** GATT::clientCharConfigChanged(match %b): notify %b, indicate %b from %s\n  %s\n    %s\n      %s\n",
                    match, notificationEnabled, indicationEnabled,
                    device.toString(), s.toString(), c.toString(), d.toString());

            if( match ) {
                final boolean local = sync_data; // SC-DRF acquire via sc_atomic_bool::load()
                final String value_type = c.getValueType();
                final short value_handle = c.getValueHandle();
                if( value_type.equals( PulseDataUUID ) ) {
                    handlePulseDataNotify = notificationEnabled ? value_handle : 0;
                    handlePulseDataIndicate = indicationEnabled ? value_handle : 0;
                } else if( value_type.equals( ResponseUUID ) ) {
                    handleResponseDataNotify = notificationEnabled ? value_handle : 0;
                    handleResponseDataIndicate = indicationEnabled ? value_handle : 0;
                }
                sync_data = local; // SC-DRF release via sc_atomic_bool::store()
            }
        }
    }
    static final short adv_interval_min=(short)0x0800;
    static final short adv_interval_max=(short)0x0800;
    static final byte adv_type=(byte)0; // AD_PDU_Type::ADV_IND;
    static final byte adv_chan_map=(byte)0x07;
    static final byte filter_policy=(byte)0x00;

    private boolean stopAdvertising(final BTAdapter adapter, final String msg) {
        if( !useAdapter.equals(EUI48.ALL_DEVICE) && !useAdapter.equals(adapter.getAddressAndType().address) ) {
            BTUtils.fprintf_td(System.err, "****** Stop advertising (%s): Adapter not selected: %s\n", msg, adapter.toString());
            return false;
        }
        final HCIStatusCode status = adapter.stopAdvertising();
        BTUtils.println(System.err, "****** Stop advertising ("+msg+") result: "+status+": "+adapter.toString());
        return HCIStatusCode.SUCCESS == status;
    }

    private boolean startAdvertising(final BTAdapter adapter, final String msg) {
        if( !useAdapter.equals(EUI48.ALL_DEVICE) && !useAdapter.equals(adapter.getAddressAndType().address) ) {
            BTUtils.fprintf_td(System.err, "****** Start advertising (%s): Adapter not selected: %s\n", msg, adapter.toString());
            return false;
        }
        final HCIStatusCode status = adapter.startAdvertising(dbGattServer,
                                                              adv_interval_min, adv_interval_max,
                                                              adv_type, adv_chan_map, filter_policy);
        BTUtils.println(System.err, "****** Start advertising ("+msg+") result: "+status+": "+adapter.toString());
        BTUtils.println(System.err, dbGattServer.toFullString());
        return HCIStatusCode.SUCCESS == status;
    }

    private void processDisconnectedDevice(final BTDevice device) {
        BTUtils.println(System.err, "****** Disconnected Device (count "+servedConnections+"): Start "+device.toString());

        // already unpaired
        stopAdvertising(device.getAdapter(), "device-disconnected");
        BTDeviceRegistry.removeFromProcessingDevices(device.getAddressAndType());

        if( !RUN_ONLY_ONCE ) {
            startAdvertising(device.getAdapter(), "device-disconnected");
        }

        BTUtils.println(System.err, "****** Disonnected Device: End "+device.toString());
    }

    private boolean initAdapter(final BTAdapter adapter) {
        if( !useAdapter.equals(EUI48.ALL_DEVICE) && !useAdapter.equals(adapter.getAddressAndType().address) ) {
            BTUtils.fprintf_td(System.err, "initAdapter: Adapter not selected: %s\n", adapter.toString());
            return false;
        }
        if( !adapter.isInitialized() ) {
            // Initialize with defaults and power-on
            final HCIStatusCode status = adapter.initialize( btMode );
            if( HCIStatusCode.SUCCESS != status ) {
                BTUtils.fprintf_td(System.err, "initAdapter: initialization failed: %s: %s\n",
                        status.toString(), adapter.toString());
                return false;
            }
        } else if( !adapter.setPowered( true ) ) {
            BTUtils.fprintf_td(System.err, "initAdapter: setPower.1 on failed: %s\n", adapter.toString());
            return false;
        }
        // adapter is powered-on
        BTUtils.println(System.err, "initAdapter.1: "+adapter.toString());

        if( adapter.setPowered(false) ) {
            final HCIStatusCode status = adapter.setName(adapter_name, adapter_short_name);
            if( HCIStatusCode.SUCCESS == status ) {
                BTUtils.fprintf_td(System.err, "initAdapter: setLocalName OK: %s\n", adapter.toString());
            } else {
                BTUtils.fprintf_td(System.err, "initAdapter: setLocalName failed: %s\n", adapter.toString());
            }
            if( adapter.setSecureConnections( use_SC ) ) {
                BTUtils.fprintf_td(System.err, "initAdapter: setSecureConnections OK: %s\n", adapter.toString());
            } else {
                BTUtils.fprintf_td(System.err, "initAdapter: setSecureConnections failed: %s\n", adapter.toString());
            }
            if( !adapter.setPowered( true ) ) {
                BTUtils.fprintf_td(System.err, "initAdapter: setPower.2 on failed: %s\n", adapter.toString());
                return false;
            }
        } else {
            BTUtils.fprintf_td(System.err, "initAdapter: setPowered.2 off failed: %s\n", adapter.toString());
        }
        BTUtils.println(System.err, "initAdapter.2: "+adapter.toString());

        {
            final LE_Features le_feats = adapter.getLEFeatures();
            BTUtils.fprintf_td(System.err, "initAdapter: LE_Features %s\n", le_feats.toString());
        }
        {
            final LE_PHYs Tx = new LE_PHYs( LE_PHYs.PHY.LE_2M );
            final LE_PHYs Rx = new LE_PHYs( LE_PHYs.PHY.LE_2M );
            final HCIStatusCode res = adapter.setDefaultLE_PHY(Tx, Rx);
            BTUtils.fprintf_td(System.err, "initAdapter: Set Default LE PHY: status %s: Tx %s, Rx %s\n",
                                res.toString(), Tx.toString(), Rx.toString());
        }
        adapter.setSMPKeyPath(DBTConstants.ADAPTER_KEY_PATH);

        // adapter is powered-on
        final AdapterStatusListener asl = new MyAdapterStatusListener();
        adapter.addStatusListener( asl );
        // Flush discovered devices after registering our status listener.
        // This avoids discovered devices before we have registered!
        adapter.removeDiscoveredDevices();

        if( !startAdvertising(adapter, "initAdapter") ) {
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

        BTUtils.println(System.err, "****** Test Start");

        final MyGATTServerListener listener = new MyGATTServerListener();
        dbGattServer.addListener( listener );

        manager.addChangedAdapterSetListener(myChangedAdapterSetListener);

        while( !RUN_ONLY_ONCE || 0 == servedConnections ) {
            try {
                Thread.sleep(2000);
            } catch (final InterruptedException e) {
                e.printStackTrace();
            }
        }

        BTUtils.println(System.err, "****** Shutdown.01 (DBGattServer.remove-listener)");
        dbGattServer.removeListener( listener );

        BTUtils.println(System.err, "****** Test Shutdown.02 (listener.close)");
        listener.close();

        BTUtils.println(System.err, "****** Test Shutdown.03 (DBGattServer.close)");
        dbGattServer.close();

        // chosenAdapter = null;

        BTUtils.println(System.err, "****** Test End");
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

        final DBTPeripheral00 test = new DBTPeripheral00();

        boolean waitForEnter=false;
        {
            for(int i=0; i< args.length; i++) {
                final String arg = args[i];

                if( arg.equals("-wait") ) {
                    waitForEnter = true;
                } else if( arg.equals("-show_update_events") ) {
                    test.SHOW_UPDATE_EVENTS = true;
                } else if( arg.equals("-btmode") && args.length > (i+1) ) {
                    test.btMode = BTMode.get(args[++i]);
                } else if( arg.equals("-use_sc") && args.length > (i+1) ) {
                    test.use_SC = 0 != Integer.valueOf(args[++i]);
                } else if( arg.equals("-adapter") && args.length > (i+1) ) {
                    test.useAdapter = new EUI48( args[++i] );
                } else if( arg.equals("-name") && args.length > (i+1) ) {
                    test.adapter_name = args[++i];
                } else if( arg.equals("-short_name") && args.length > (i+1) ) {
                    test.adapter_short_name = args[++i];
                } else if( arg.equals("-mtu") && args.length > (i+1) ) {
                    test.dbGattServer.setMaxAttMTU( Integer.valueOf( args[++i] ) );
                } else if( arg.equals("-once") ) {
                    test.RUN_ONLY_ONCE = true;
                }
            }
            BTUtils.println(System.err, "Run with '[-btmode LE|BREDR|DUAL] [-use_sc 0|1] "+
                    "[-adapter <adapter_address>] "+
                    "[-name <adapter_name>] "+
                    "[-short_name <adapter_short_name>] "+
                    "[-mtu <max att_mtu>] " +
                    "[-once] "+
                    "[-verbose] [-debug] "+
                    "[-dbt_verbose true|false] "+
                    "[-dbt_debug true|false|adapter.event,gatt.data,hci.event,hci.scan_ad_eir,mgmt.event] "+
                    "[-dbt_mgmt cmd.timeout=3000,ringsize=64,...] "+
                    "[-dbt_hci cmd.complete.timeout=10000,cmd.status.timeout=3000,ringsize=64,...] "+
                    "[-dbt_gatt cmd.read.timeout=500,cmd.write.timeout=500,cmd.init.timeout=2500,ringsize=128,...] "+
                    "[-dbt_l2cap reader.timeout=10000,restart.count=0,...] "+
                    "[-shutdown <int>]'");
        }

        BTUtils.println(System.err, "SHOW_UPDATE_EVENTS "+test.SHOW_UPDATE_EVENTS);
        BTUtils.println(System.err, "adapter "+test.useAdapter);
        BTUtils.println(System.err, "btmode "+test.btMode.toString());
        BTUtils.println(System.err, "use SC "+test.use_SC);
        BTUtils.fprintf_td(System.err, "name %s (short %s)\n", test.adapter_name, test.adapter_short_name);
        BTUtils.println(System.err, "mtu "+test.dbGattServer.getMaxAttMTU());
        BTUtils.println(System.err, "once "+test.RUN_ONLY_ONCE);
        BTUtils.println(System.err, "GattServer "+test.dbGattServer.toString());

        if( waitForEnter ) {
            BTUtils.println(System.err, "Press ENTER to continue\n");
            try{ System.in.read();
            } catch(final Exception e) { }
        }
        test.runTest(manager);
    }
}
