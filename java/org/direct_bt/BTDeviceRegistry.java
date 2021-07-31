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

package org.direct_bt;

import java.io.PrintStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;

/**
 * Application toolkit providing BT device registration of processed and awaited devices.
 * The latter on a pattern matching basis, i.e. EUI48Sub or name-sub.
 */
public class BTDeviceRegistry {
    private static class DeviceQuery {
        public final EUI48Sub addressSub;
        public final String nameSub;
        public DeviceQuery(final EUI48Sub as, final String ns) {
            addressSub = as;
            nameSub = ns;
        }
        @Override
        public String toString() {
            if( addressSub.length>0 ) {
                return addressSub.toString();
            } else {
                return "'"+nameSub+"'";
            }
        }
    };
    private static List<DeviceQuery> waitForDevices = new ArrayList<DeviceQuery>();

    private static class DeviceID {
        public final BDAddressAndType addressAndType;
        public final String name;
        public DeviceID(final BDAddressAndType a, final String n) {
            addressAndType = a;
            name = n;
        }

        /**
         * {@inheritDoc}
         * <p>
         * Implementation simply tests the {@link BDAddressAndType} fields for equality,
         * `name` is ignored.
         * </p>
         */
        @Override
        public final boolean equals(final Object obj) {
            if(this == obj) {
                return true;
            }
            if (obj == null || !(obj instanceof DeviceID)) {
                return false;
            }
            return addressAndType.equals(((DeviceID)obj).addressAndType);
        }

        /**
         * {@inheritDoc}
         * <p>
         * Implementation simply returns the {@link BDAddressAndType} hash code,
         * `name` is ignored.
         * </p>
         */
        @Override
        public final int hashCode() {
            return addressAndType.hashCode();
        }

        @Override
        public String toString() {
            return "["+addressAndType+", "+name+"]";
        }
    };
    private static Collection<DeviceID> devicesInProcessing = Collections.synchronizedCollection(new HashSet<DeviceID>());
    private static Collection<DeviceID> devicesProcessed = Collections.synchronizedCollection(new HashSet<DeviceID>());

    public static void addToWaitForDevices(final String addrOrNameSub) {
        final EUI48Sub addr1 = new EUI48Sub();
        final StringBuilder errmsg = new StringBuilder();
        if( EUI48Sub.scanEUI48Sub(addrOrNameSub, addr1, errmsg) ) {
            waitForDevices.add( new DeviceQuery( addr1, "" ) );
        } else {
            addr1.clear();
            waitForDevices.add( new DeviceQuery( addr1, addrOrNameSub ) );
        }
    }
    public static boolean isWaitingForDevice(final BDAddressAndType mac, final String name) {
        for(final Iterator<DeviceQuery> it=waitForDevices.iterator(); it.hasNext(); ) {
            final DeviceQuery q = it.next();
            if( ( q.addressSub.length>0 && mac.address.contains(q.addressSub) ) ||
                ( q.nameSub.length()>0 && name.indexOf(q.nameSub) >= 0 ) ) {
                return true;
            }
        }
        return false;
    }
    public static boolean isWaitingForAnyDevice() {
        return waitForDevices.size()>0;
    }
    public static int getWaitForDevicesCount() {
        return waitForDevices.size();
    }
    public static void printWaitForDevices(final PrintStream out, final String msg) {
        BTUtils.println(out, msg+" "+Arrays.toString(waitForDevices.toArray()));
    }

    public static void addToDevicesProcessed(final BDAddressAndType a, final String n) {
        devicesProcessed.add( new DeviceID(a, n) );
    }
    public static boolean isDeviceProcessed(final BDAddressAndType a) {
        return devicesProcessed.contains( new DeviceID(a, null) );
    }
    public static int getDeviceProcessedCount() {
        return devicesProcessed.size();
    }
    public static boolean allDevicesProcessed() {
        for(final Iterator<DeviceQuery> it1=waitForDevices.iterator(); it1.hasNext(); ) {
            final DeviceQuery q = it1.next();
            final Iterator<DeviceID> it2=devicesProcessed.iterator();
            while( it2.hasNext() ) {
                final DeviceID id = it2.next();
                if( ( q.addressSub.length>0 && id.addressAndType.address.contains(q.addressSub) ) ||
                    ( q.nameSub.length()>0 && id.name.indexOf(q.nameSub) >= 0 ) ) {
                    break;
                }
            }
            if( !it2.hasNext() ) {
                return false;
            }
        }
        return true;
    }
    public static void printDevicesProcessed(final PrintStream out, final String msg) {
        BTUtils.println(out, msg+" "+Arrays.toString(devicesProcessed.toArray()));
    }

    public static void addToDevicesProcessing(final BDAddressAndType a, final String n) {
        devicesInProcessing.add( new DeviceID(a, n) );
    }
    public static boolean removeFromDevicesProcessing(final BDAddressAndType a) {
        return devicesInProcessing.remove( new DeviceID(a, null) );
    }
    public static boolean isDeviceProcessing(final BDAddressAndType a) {
        return devicesInProcessing.contains( new DeviceID(a, null) );
    }
    public static int getDeviceProcessingCount() {
        return devicesInProcessing.size();
    }
}
