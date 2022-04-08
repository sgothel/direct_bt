/**
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2022 Gothel Software e.K.
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

package trial.org.direct_bt;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Iterator;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;

import org.direct_bt.AdapterSettings;
import org.direct_bt.AdapterStatusListener;
import org.direct_bt.BDAddressAndType;
import org.direct_bt.BTMode;
import org.direct_bt.BTSecurityLevel;
import org.direct_bt.BTAdapter;
import org.direct_bt.BTDevice;
import org.direct_bt.BTDeviceRegistry;
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
import org.direct_bt.GAPFlags;
import org.direct_bt.GattCharPropertySet;
import org.direct_bt.HCIStatusCode;
import org.direct_bt.LE_Features;
import org.direct_bt.LE_PHYs;
import org.direct_bt.PairingMode;
import org.direct_bt.SMPIOCapability;
import org.direct_bt.SMPPairingState;
import org.direct_bt.ScanType;
import org.jau.net.EUI48;

/**
 * This Java peripheral {@link BTRole::Slave} test case working with {@link DBTClient00}.
 */
public class DBTServer00 implements DBTServerTest {
    final boolean GATT_VERBOSE = false;
    boolean SHOW_UPDATE_EVENTS = false;

    EUI48 useAdapter = EUI48.ALL_DEVICE;
    BTMode btMode = BTMode.DUAL;
    boolean use_SC = true;
    String adapterName = "TestDev001_J";
    final String adapterShortName = "TDev001J";
    BTSecurityLevel adapterSecurityLevel = BTSecurityLevel.UNSET;
    BTAdapter serverAdapter = null;

    public AtomicInteger servingConnectionsLeft = new AtomicInteger(1);
    public AtomicInteger servedConnections = new AtomicInteger(0);

    public DBTServer00(final EUI48 useAdapter, final BTMode btMode, final boolean use_SC, final String adapterName, final BTSecurityLevel adapterSecurityLevel) {
        this.useAdapter = useAdapter;
        this.btMode = btMode;
        this.use_SC = use_SC;
        this.adapterName = adapterName;
        this.adapterSecurityLevel = adapterSecurityLevel;

        final MyGATTServerListener listener = new MyGATTServerListener();
        dbGattServer.addListener( listener );
    }
    public DBTServer00(final EUI48 useAdapter, final String adapterName, final BTSecurityLevel adapterSecurityLevel) {
        this(useAdapter, BTMode.DUAL, true /* SC */, adapterName, adapterSecurityLevel);
    }
    public DBTServer00(final String adapterName, final BTSecurityLevel adapterSecurityLevel) {
        this(EUI48.ALL_DEVICE, BTMode.DUAL, true /* SC */, adapterName, adapterSecurityLevel);
    }

    @Override
    public String getName() { return adapterName; }

    @Override
    public BTSecurityLevel getSecurityLevel() { return adapterSecurityLevel; }

    @Override
    public void setAdapter(final BTAdapter serverAdapter) {
        this.serverAdapter = serverAdapter;
    }
    @Override
    public BTAdapter getAdapter() { return serverAdapter; }

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
    static DBGattValue make_gvalue(final String name, final int capacity) {
        final byte[] p = name.getBytes(StandardCharsets.UTF_8);
        return new DBGattValue(p, Math.max(capacity, p.length), capacity > p.length/* variable_length */);
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
                                  make_gvalue(adapterName, 128) /* value */ ),
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
                  DBTConstants.DataServiceUUID /* type_ */,
                  Arrays.asList( // DBGattChar
                      new DBGattChar( DBTConstants.StaticDataUUID /* value_type_ */,
                                  new GattCharPropertySet(GattCharPropertySet.Type.Read),
                                  Arrays.asList( // DBGattDesc
                                      new DBGattDesc( DBGattDesc.UUID16.USER_DESC, make_gvalue("DATA_STATIC") )
                                  ),
                                make_gvalue("Proprietary Static Data 0x00010203") /* value */ ),
                      new DBGattChar( DBTConstants.CommandUUID /* value_type_ */,
                                  new GattCharPropertySet(GattCharPropertySet.Type.WriteNoAck).set(GattCharPropertySet.Type.WriteWithAck),
                                  Arrays.asList( // DBGattDesc
                                      new DBGattDesc( DBGattDesc.UUID16.USER_DESC, make_gvalue("COMMAND") )
                                  ),
                                  make_gvalue(128, 64) /* value */ ),
                      new DBGattChar( DBTConstants.ResponseUUID /* value_type_ */,
                                  new GattCharPropertySet(GattCharPropertySet.Type.Notify).set(GattCharPropertySet.Type.Indicate),
                                  Arrays.asList( // DBGattDesc
                                      new DBGattDesc( DBGattDesc.UUID16.USER_DESC, make_gvalue("RESPONSE") ),
                                      DBGattDesc.createClientCharConfig()
                                  ),
                                  make_gvalue((short)0) /* value */ ),
                      new DBGattChar( DBTConstants.PulseDataUUID /* value_type_ */,
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
                BTUtils.println(System.err, "****** Server SETTINGS: "+oldmask+" -> "+newmask+", initial "+changedmask);
            } else {
                BTUtils.println(System.err, "****** Server SETTINGS: "+oldmask+" -> "+newmask+", changed "+changedmask);
            }
            BTUtils.println(System.err, "Status Adapter:");
            BTUtils.println(System.err, adapter.toString());
        }

        @Override
        public void discoveringChanged(final BTAdapter adapter, final ScanType currentMeta, final ScanType changedType, final boolean changedEnabled, final DiscoveryPolicy policy, final long timestamp) {
            BTUtils.println(System.err, "****** Server DISCOVERING: meta "+currentMeta+", changed["+changedType+", enabled "+changedEnabled+", policy "+policy+"] on "+adapter);
        }

        @Override
        public boolean deviceFound(final BTDevice device, final long timestamp) {
            BTUtils.println(System.err, "****** Server FOUND__-1: NOP "+device.toString());
            return false;
        }

        @Override
        public void deviceUpdated(final BTDevice device, final EIRDataTypeSet updateMask, final long timestamp) {
            if( SHOW_UPDATE_EVENTS ) {
                BTUtils.println(System.err, "****** Server UPDATED: "+updateMask+" of "+device);
            }
        }

        @Override
        public void deviceConnected(final BTDevice device, final short handle, final long timestamp) {
            BTUtils.println(System.err, "****** Server CONNECTED (served "+servedConnections.get()+", left "+servingConnectionsLeft.get()+"): handle 0x"+Integer.toHexString(handle)+": "+device+" on "+device.getAdapter());
        }

        @Override
        public void devicePairingState(final BTDevice device, final SMPPairingState state, final PairingMode mode, final long timestamp) {
            BTUtils.println(System.err, "****** Server PAIRING_STATE: state "+state+", mode "+mode+": "+device);
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
            BTUtils.println(System.err, "****** Server READY-1: NOP " + device.toString());
        }

        @Override
        public void deviceDisconnected(final BTDevice device, final HCIStatusCode reason, final short handle, final long timestamp) {
            BTUtils.println(System.err, "****** Server DISCONNECTED (served "+servedConnections.get()+", left "+servingConnectionsLeft.get()+"): Reason "+reason+", old handle 0x"+Integer.toHexString(handle)+": "+device+" on "+device.getAdapter());

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

            dbGattServer.resetGattClientCharConfig(DBTConstants.DataServiceUUID, DBTConstants.PulseDataUUID);
            dbGattServer.resetGattClientCharConfig(DBTConstants.DataServiceUUID, DBTConstants.ResponseUUID);

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
                                if( GATT_VERBOSE ) {
                                    BTUtils.fprintf_td(System.err, "****** GATT::sendNotification: PULSE to %s\n", connectedDevice.toString());
                                }
                                connectedDevice.sendNotification(handlePulseDataNotify, v);
                            }
                            if( 0 != handlePulseDataIndicate ) {
                                if( GATT_VERBOSE ) {
                                    BTUtils.fprintf_td(System.err, "****** GATT::sendIndication: PULSE to %s\n", connectedDevice.toString());
                                }
                                connectedDevice.sendIndication(handlePulseDataIndicate, v);
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
                        if( GATT_VERBOSE ) {
                            BTUtils.fprintf_td(System.err, "****** GATT::sendNotification: %s to %s\n",
                                    BTUtils.bytesHexString(data, 0, data.length, true /* lsb */), connectedDevice.toString());
                        }
                        connectedDevice.sendNotification(handleResponseDataNotify, data);
                    }
                    if( 0 != handleResponseDataIndicate ) {
                        if( GATT_VERBOSE ) {
                            BTUtils.fprintf_td(System.err, "****** GATT::sendIndication: %s to %s\n",
                                    BTUtils.bytesHexString(data, 0, data.length, true /* lsb */), connectedDevice.toString());
                        }
                        connectedDevice.sendIndication(handleResponseDataIndicate, data);
                    }
                }
            }
        }

        public MyGATTServerListener() {
            pulseSenderThread = executeOffThread( () -> { pulseSender(); }, "GattServer-PulseSender", false /* detach */);
        }

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
            super.close();
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
            final int usedMTU_old = usedMTU;
            if( match ) {
                usedMTU = mtu;
            }
            BTUtils.fprintf_td(System.err, "****** GATT::mtuChanged(match %b, served %d, left %d): %d -> %d, %s\n",
                    match, servedConnections.get(), servingConnectionsLeft.get(),
                    match ? usedMTU_old : 0, mtu, device.toString());
        }

        @Override
        public boolean readCharValue(final BTDevice device, final DBGattService s, final DBGattChar c) {
            final boolean match = matches(device);
            if( GATT_VERBOSE ) {
                BTUtils.fprintf_td(System.err, "****** GATT::readCharValue(match %b): to %s, from\n  %s\n    %s\n",
                        match, device.toString(), s.toString(), c.toString());
            }
            return match;
        }

        @Override
        public boolean readDescValue(final BTDevice device, final DBGattService s, final DBGattChar c, final DBGattDesc d) {
            final boolean match = matches(device);
            if( GATT_VERBOSE ) {
                BTUtils.fprintf_td(System.err, "****** GATT::readDescValue(match %b): to %s, from\n  %s\n    %s\n      %s\n",
                        match, device.toString(), s.toString(), c.toString(), d.toString());
            }
            return match;
        }

        @Override
        public boolean writeCharValue(final BTDevice device, final DBGattService s, final DBGattChar c, final byte[] value, final int value_offset) {
            final boolean match = matches(device);
            if( GATT_VERBOSE ) {
                final String value_s = BTUtils.bytesHexString(value, 0, value.length, true /* lsbFirst */)+
                                       " '"+BTUtils.decodeUTF8String(value, 0, value.length)+"'";
                BTUtils.fprintf_td(System.err, "****** GATT::writeCharValue(match %b): %s @ %d from %s, to\n  %s\n    %s\n",
                        match, value_s, value_offset,
                        device.toString(), s.toString(), c.toString());
            }
            return match;
        }

        @Override
        public void writeCharValueDone(final BTDevice device, final DBGattService s, final DBGattChar c) {
            final boolean match = matches(device);
            final DBGattValue value = c.getValue();
            final byte[] data = value.data();
            boolean isSuccessHandshake = false;

            if( match &&
                c.getValueType().equals( DBTConstants.CommandUUID ) &&
                ( 0 != handleResponseDataNotify || 0 != handleResponseDataIndicate ) )
            {
                isSuccessHandshake = Arrays.equals(DBTConstants.SuccessHandshakeCommandData, data);

                if( isSuccessHandshake ) {
                    servedConnections.addAndGet(1); // we assume this to be server, i.e. connected, SMP done and GATT action ..
                    if( servingConnectionsLeft.get() > 0 ) {
                        servingConnectionsLeft.decrementAndGet(); // ditto
                    }
                }
                executeOffThread( () -> { sendResponse( data ); }, true /* detach */);
            }
            if( GATT_VERBOSE || isSuccessHandshake ) {
                BTUtils.fprintf_td(System.err, "****** GATT::writeCharValueDone(match %b, successCmd %b, served %d, left %d): From %s, to\n  %s\n    %s\n    Char-Value: %s\n",
                        match, isSuccessHandshake, servedConnections.get(), servingConnectionsLeft.get(),
                        device.toString(), s.toString(), c.toString(), value.toString());
            }
        }

        @Override
        public boolean writeDescValue(final BTDevice device, final DBGattService s, final DBGattChar c, final DBGattDesc d,
                                      final byte[] value, final int value_offset)
        {
            final boolean match = matches(device);
            if( GATT_VERBOSE ) {
                final String value_s = BTUtils.bytesHexString(value, 0, value.length, true /* lsbFirst */)+
                                       " '"+BTUtils.decodeUTF8String(value, 0, value.length)+"'";
                BTUtils.fprintf_td(System.err, "****** GATT::writeDescValue(match %b): %s @ %d from %s\n  %s\n    %s\n      %s\n",
                        match, value_s, value_offset,
                        device.toString(), s.toString(), c.toString(), d.toString());
            }
            return match;
        }

        @Override
        public void writeDescValueDone(final BTDevice device, final DBGattService s, final DBGattChar c, final DBGattDesc d) {
            if( GATT_VERBOSE ) {
                final boolean match = matches(device);
                final DBGattValue value = d.getValue();
                BTUtils.fprintf_td(System.err, "****** GATT::writeDescValueDone(match %b): From %s\n  %s\n    %s\n      %s\n      Desc-Value: %s\n",
                        match, device.toString(), s.toString(), c.toString(), d.toString(), value.toString());
            }
        }

        @Override
        public void clientCharConfigChanged(final BTDevice device, final DBGattService s, final DBGattChar c, final DBGattDesc d,
                                            final boolean notificationEnabled, final boolean indicationEnabled)
        {
            final boolean match = matches(device);
            if( GATT_VERBOSE ) {
                final DBGattValue value = d.getValue();
                BTUtils.fprintf_td(System.err, "****** GATT::clientCharConfigChanged(match %b): notify %b, indicate %b from %s\n  %s\n    %s\n      %s\n      Desc-Value: %s\n",
                        match, notificationEnabled, indicationEnabled,
                        device.toString(), s.toString(), c.toString(), d.toString(), value.toString());
            }
            if( match ) {
                final boolean local = sync_data; // SC-DRF acquire via sc_atomic_bool::load()
                final String value_type = c.getValueType();
                final short value_handle = c.getValueHandle();
                if( value_type.equals( DBTConstants.PulseDataUUID ) ) {
                    handlePulseDataNotify = notificationEnabled ? value_handle : 0;
                    handlePulseDataIndicate = indicationEnabled ? value_handle : 0;
                } else if( value_type.equals( DBTConstants.ResponseUUID ) ) {
                    handleResponseDataNotify = notificationEnabled ? value_handle : 0;
                    handleResponseDataIndicate = indicationEnabled ? value_handle : 0;
                }
                sync_data = local; // SC-DRF release via sc_atomic_bool::store()
            }
        }
    }
    static final short adv_interval_min=(short)640;
    static final short adv_interval_max=(short)640;
    static final byte adv_type=(byte)0; // AD_PDU_Type::ADV_IND;
    static final byte adv_chan_map=(byte)0x07;
    static final byte filter_policy=(byte)0x00;

    @Override
    public HCIStatusCode stopAdvertising(final BTAdapter adapter, final String msg) {
        if( !useAdapter.equals(EUI48.ALL_DEVICE) && !useAdapter.equals(adapter.getAddressAndType().address) ) {
            BTUtils.fprintf_td(System.err, "****** Stop advertising (%s): Adapter not selected: %s\n", msg, adapter.toString());
            return HCIStatusCode.FAILED;
        }
        final HCIStatusCode status = adapter.stopAdvertising();
        BTUtils.println(System.err, "****** Stop advertising ("+msg+") result: "+status+": "+adapter.toString());
        return status;
    }

    @Override
    public HCIStatusCode startAdvertising(final BTAdapter adapter, final String msg) {
        if( !useAdapter.equals(EUI48.ALL_DEVICE) && !useAdapter.equals(adapter.getAddressAndType().address) ) {
            BTUtils.fprintf_td(System.err, "****** Start advertising (%s): Adapter not selected: %s\n", msg, adapter.toString());
            return HCIStatusCode.FAILED;
        }
        final EInfoReport eir = new EInfoReport();
        final EIRDataTypeSet adv_mask = new EIRDataTypeSet();
        final EIRDataTypeSet scanrsp_mask = new EIRDataTypeSet();

        adv_mask.set(EIRDataTypeSet.DataType.FLAGS);
        adv_mask.set(EIRDataTypeSet.DataType.SERVICE_UUID);

        scanrsp_mask.set(EIRDataTypeSet.DataType.NAME);
        scanrsp_mask.set(EIRDataTypeSet.DataType.CONN_IVAL);

        eir.addFlag(GAPFlags.Bit.LE_Gen_Disc);
        eir.addFlag(GAPFlags.Bit.BREDR_UNSUP);

        eir.addService(DBTConstants.DataServiceUUID);
        eir.setServicesComplete(false);

        eir.setName(adapter.getName());
        eir.setConnInterval((short)8, (short)24); // 10ms - 30ms

        final DBGattChar gattDevNameChar = dbGattServer.findGattChar(DBGattService.UUID16.GENERIC_ACCESS, DBGattChar.UUID16.DEVICE_NAME);
        if( null != gattDevNameChar ) {
            final byte[] aname_bytes = adapter.getName().getBytes(StandardCharsets.UTF_8);
            gattDevNameChar.setValue(aname_bytes, 0, aname_bytes.length, 0);
        }

        BTUtils.println(System.err, "****** Start advertising ("+msg+"): EIR "+eir.toString());
        BTUtils.println(System.err, "****** Start advertising ("+msg+"): adv "+adv_mask.toString()+", scanrsp "+scanrsp_mask.toString());

        final HCIStatusCode status = adapter.startAdvertising(dbGattServer, eir, adv_mask, scanrsp_mask,
                                                              adv_interval_min, adv_interval_max,
                                                              adv_type, adv_chan_map, filter_policy);
        BTUtils.println(System.err, "****** Start advertising ("+msg+") result: "+status+": "+adapter.toString());
        if( GATT_VERBOSE ) {
            BTUtils.println(System.err, dbGattServer.toFullString());
        }
        return status;
    }

    private void processDisconnectedDevice(final BTDevice device) {
        BTUtils.println(System.err, "****** Disconnected Device (served "+servedConnections.get()+", left "+servingConnectionsLeft.get()+"): Start "+device.toString());

        // already unpaired
        stopAdvertising(device.getAdapter(), "device-disconnected");
        device.remove();
        BTDeviceRegistry.removeFromProcessingDevices(device.getAddressAndType());

        try {
            Thread.sleep(100); // wait a little (FIXME: Fast restart of advertising error)
        } catch (final InterruptedException e) { }

        if( servingConnectionsLeft.get() > 0 ) {
            startAdvertising(device.getAdapter(), "device-disconnected");
        }

        BTUtils.println(System.err, "****** Disonnected Device: End "+device.toString());
    }

    @Override
    public boolean initAdapter(final BTAdapter adapter) {
        if( !useAdapter.equals(EUI48.ALL_DEVICE) && !useAdapter.equals(adapter.getAddressAndType().address) ) {
            BTUtils.fprintf_td(System.err, "initServerAdapter: Adapter not selected: %s\n", adapter.toString());
            return false;
        }
        if( !adapter.isInitialized() ) {
            // Initialize with defaults and power-on
            final HCIStatusCode status = adapter.initialize( btMode );
            if( HCIStatusCode.SUCCESS != status ) {
                BTUtils.fprintf_td(System.err, "initServerAdapter: initialization failed: %s: %s\n",
                        status.toString(), adapter.toString());
                return false;
            }
        } else if( !adapter.setPowered( true ) ) {
            BTUtils.fprintf_td(System.err, "initServerAdapter: setPower.1 on failed: %s\n", adapter.toString());
            return false;
        }
        // adapter is powered-on
        BTUtils.println(System.err, "initServerAdapter.1: "+adapter.toString());

        if( adapter.setPowered(false) ) {
            HCIStatusCode status = adapter.setName(adapterName, adapterShortName);
            if( HCIStatusCode.SUCCESS == status ) {
                BTUtils.fprintf_td(System.err, "initServerAdapter: setLocalName OK: %s\n", adapter.toString());
            } else {
                BTUtils.fprintf_td(System.err, "initServerAdapter: setLocalName failed: %s\n", adapter.toString());
                return false;
            }

            status = adapter.setSecureConnections( use_SC );
            if( HCIStatusCode.SUCCESS == status ) {
                BTUtils.fprintf_td(System.err, "initServerAdapter: setSecureConnections OK: %s\n", adapter.toString());
            } else {
                BTUtils.fprintf_td(System.err, "initServerAdapter: setSecureConnections failed: %s\n", adapter.toString());
                return false;
            }

            final short conn_min_interval = 8;  // 10ms
            final short conn_max_interval = 40; // 50ms
            final short conn_latency = 0;
            final short supervision_timeout = 50; // 500ms
            status = adapter.setDefaultConnParam(conn_min_interval, conn_max_interval, conn_latency, supervision_timeout);
            if( HCIStatusCode.SUCCESS == status ) {
                BTUtils.fprintf_td(System.err, "initServerAdapter: setDefaultConnParam OK: %s\n", adapter.toString());
            } else {
                BTUtils.fprintf_td(System.err, "initServerAdapter: setDefaultConnParam failed: %s\n", adapter.toString());
                return false;
            }

            if( !adapter.setPowered( true ) ) {
                BTUtils.fprintf_td(System.err, "initServerAdapter: setPower.2 on failed: %s\n", adapter.toString());
                return false;
            }
        } else {
            BTUtils.fprintf_td(System.err, "initServerAdapter: setPowered.2 off failed: %s\n", adapter.toString());
        }
        BTUtils.println(System.err, "initServerAdapter.2: "+adapter.toString());

        {
            final LE_Features le_feats = adapter.getLEFeatures();
            BTUtils.fprintf_td(System.err, "initServerAdapter: LE_Features %s\n", le_feats.toString());
        }
        {
            final LE_PHYs Tx = new LE_PHYs( LE_PHYs.PHY.LE_2M );
            final LE_PHYs Rx = new LE_PHYs( LE_PHYs.PHY.LE_2M );
            final HCIStatusCode res = adapter.setDefaultLE_PHY(Tx, Rx);
            BTUtils.fprintf_td(System.err, "initServerAdapter: Set Default LE PHY: status %s: Tx %s, Rx %s\n",
                                res.toString(), Tx.toString(), Rx.toString());
        }
        adapter.setSMPKeyPath(DBTConstants.SERVER_KEY_PATH);

        // adapter is powered-on
        final AdapterStatusListener asl = new MyAdapterStatusListener();
        adapter.addStatusListener( asl );
        // Flush discovered devices after registering our status listener.
        // This avoids discovered devices before we have registered!
        adapter.removeDiscoveredDevices();

        adapter.setServerConnSecurity(adapterSecurityLevel, SMPIOCapability.UNSET);

        return true;
    }
}
