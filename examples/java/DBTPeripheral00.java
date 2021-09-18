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
import java.util.Iterator;
import java.util.List;

import org.direct_bt.AdapterSettings;
import org.direct_bt.AdapterStatusListener;
import org.direct_bt.BDAddressAndType;
import org.direct_bt.BTMode;
import org.direct_bt.BTRole;
import org.direct_bt.BTAdapter;
import org.direct_bt.BTDevice;
import org.direct_bt.BTException;
import org.direct_bt.BTFactory;
import org.direct_bt.BTManager;
import org.direct_bt.BTSecurityRegistry;
import org.direct_bt.BTUtils;
import org.direct_bt.EIRDataTypeSet;
import org.direct_bt.HCIStatusCode;
import org.direct_bt.PairingMode;
import org.direct_bt.SMPKeyBin;
import org.direct_bt.SMPPairingState;
import org.direct_bt.ScanType;
import org.jau.net.EUI48;

/**
 * This Java peripheral {@link BTRole::Slave} example uses the Direct-BT fully event driven workflow.
 */
public class DBTPeripheral00 {
    static final String KEY_PATH = "keys";

    long timestamp_t0;

    EUI48 useAdapter = EUI48.ALL_DEVICE;
    BTMode btMode = BTMode.DUAL;
    String adapter_name = "TestDev001_J";
    String adapter_short_name = "TDev001J";

    boolean SHOW_UPDATE_EVENTS = false;

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
            BTUtils.println(System.err, "****** READY-1: NOP " + device.toString());
        }

        @Override
        public void deviceDisconnected(final BTDevice device, final HCIStatusCode reason, final short handle, final long timestamp) {
            BTUtils.println(System.err, "****** DISCONNECTED: Reason "+reason+", old handle 0x"+Integer.toHexString(handle)+": "+device+" on "+device.getAdapter());
        }

        @Override
        public String toString() {
            return "AdapterStatusListener[user, per-adapter]";
        }
    };

    static final short adv_interval_min=(short)0x0800;
    static final short adv_interval_max=(short)0x0800;
    static final byte adv_type=(byte)0; // AD_PDU_Type::ADV_IND;
    static final byte adv_chan_map=(byte)0x07;
    static final byte filter_policy=(byte)0x00;

    private boolean startAdvertising(final BTAdapter adapter, final String msg) {
        if( !useAdapter.equals(EUI48.ALL_DEVICE) && !useAdapter.equals(adapter.getAddressAndType().address) ) {
            BTUtils.fprintf_td(System.err, "****** Start advertising (%s): Adapter not selected: %s\n", msg, adapter.toString());
            return false;
        }
        final HCIStatusCode status = adapter.startAdvertising(adv_interval_min, adv_interval_max,
                                                              adv_type, adv_chan_map, filter_policy);
        BTUtils.println(System.err, "****** Start advertising ("+msg+") result: "+status);
        return HCIStatusCode.SUCCESS == status;
    }

    private boolean initAdapter(final BTAdapter adapter) {
        if( !useAdapter.equals(EUI48.ALL_DEVICE) && !useAdapter.equals(adapter.getAddressAndType().address) ) {
            BTUtils.fprintf_td(System.err, "initAdapter: Adapter not selected: %s\n", adapter.toString());
            return false;
        }
        // setName(..) ..
        if( adapter.setPowered(false) ) {
            final HCIStatusCode status = adapter.setName(adapter_name, adapter_short_name);
            if( HCIStatusCode.SUCCESS == status ) {
                BTUtils.fprintf_td(System.err, "initAdapter: setLocalName OK: %s\n", adapter.toString());
            } else {
                BTUtils.fprintf_td(System.err, "initAdapter: setLocalName failed: %s\n", adapter.toString());
            }
        } else {
            BTUtils.fprintf_td(System.err, "initAdapter: setPowered failed: %s\n", adapter.toString());
        }
        // Initialize with defaults and power-on
        {
            final HCIStatusCode status = adapter.initialize( btMode );
            if( HCIStatusCode.SUCCESS != status ) {
                BTUtils.fprintf_td(System.err, "initAdapter: Adapter initialization failed: %s: %s\n",
                        status.toString(), adapter.toString());
                return false;
            }
        }
        // Even if adapter is not yet powered, listen to it and act when it gets powered-on
        adapter.addStatusListener(new MyAdapterStatusListener() );
        // Flush discovered devices after registering our status listener.
        // This avoids discovered devices before we have registered!
        adapter.removeDiscoveredDevices();

        if( !adapter.isPowered() ) { // should have been covered above
            BTUtils.println(System.err, "initAdapter: Adapter not powered (2): "+adapter.toString());
            return false;
        }

        if( !startAdvertising(adapter, "initAdapter") ) {
            return false;
        }
        return true;
    }

    private final BTManager.ChangedAdapterSetListener myChangedAdapterSetListener =
            new BTManager.ChangedAdapterSetListener() {
                @Override
                public void adapterAdded(final BTAdapter adapter) {
                    if( initAdapter( adapter ) ) {
                        BTUtils.println(System.err, "****** Adapter ADDED__: InitOK. " + adapter);
                    } else {
                        BTUtils.println(System.err, "****** Adapter ADDED__: Ignored " + adapter);
                    }
                }

                @Override
                public void adapterRemoved(final BTAdapter adapter) {
                    BTUtils.println(System.err, "****** Adapter REMOVED: " + adapter);
                }
    };

    public void runTest(final BTManager manager) {
        timestamp_t0 = BTUtils.currentTimeMillis();

        final boolean done = false;

        manager.addChangedAdapterSetListener(myChangedAdapterSetListener);

        while( !done ) {
            try {
                Thread.sleep(2000);
            } catch (final InterruptedException e) {
                e.printStackTrace();
            }
        }
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
                } else if( arg.equals("-adapter") && args.length > (i+1) ) {
                    test.useAdapter = new EUI48( args[++i] );
                } else if( arg.equals("-name") && args.length > (i+1) ) {
                    test.adapter_name = args[++i];
                } else if( arg.equals("-short_name") && args.length > (i+1) ) {
                    test.adapter_short_name = args[++i];
                }
            }
            BTUtils.println(System.err, "Run with '[-btmode LE|BREDR|DUAL] "+
                    "[-adapter <adapter_address>] "+
                    "[-name <adapter_name>] "+
                    "[-short_name <adapter_short_name>] "+
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
        BTUtils.println(System.err, "btmode' to "+test.btMode.toString());
        BTUtils.fprintf_td(System.err, "name %s (short %s)\n", test.adapter_name, test.adapter_short_name);

        if( waitForEnter ) {
            BTUtils.println(System.err, "Press ENTER to continue\n");
            try{ System.in.read();
            } catch(final Exception e) { }
        }
        test.runTest(manager);
    }
}
