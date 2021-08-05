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
    /**
     * Specifies devices queries to act upon.
     */
    public static class DeviceQuery {
        /**
         * {@link DeviceQuery} type, i.e. {@link EUI48Sub} or a {@link String} name.
         */
        public static enum Type {
            /** {@link DeviceQuery} type, using a sensor device {@link EUI48Sub}. */
            EUI48SUB,
            /** {@link DeviceQuery} type, using a sensor device {@link String} name. */
            NAME
        }

        public final Type type;
        public final EUI48Sub addressSub;
        public final String nameSub;

        public DeviceQuery(final EUI48Sub as) {
            type = Type.EUI48SUB;
            addressSub = as;
            nameSub = as.toString();
        }
        public DeviceQuery(final String ns) {
            type = Type.NAME;
            addressSub = EUI48Sub.ANY_DEVICE;
            nameSub = ns;
        }

        public final boolean isEUI48Sub() { return Type.EUI48SUB == type; }

        @Override
        public String toString() {
            if( Type.EUI48SUB == type ) {
                return "[a: "+addressSub.toString()+"]";
            } else {
                return "[n: '"+nameSub+"']";
            }
        }
    };
    private static List<DeviceQuery> waitForDevices = new ArrayList<DeviceQuery>();

    /**
     * Specifies unique device identities,
     * using {@link BDAddressAndType} as key.
     */
    public static class DeviceID {
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
            waitForDevices.add( new DeviceQuery( addr1 ) );
        } else {
            addr1.clear();
            waitForDevices.add( new DeviceQuery( addrOrNameSub ) );
        }
    }
    public static boolean isWaitingForAnyDevice() {
        return waitForDevices.size()>0;
    }
    public static int getWaitForDevicesCount() {
        return waitForDevices.size();
    }
    public static String getWaitForDevicesString() {
        return Arrays.toString(waitForDevices.toArray());
    }
    /**
     * Returns the reference of the current list of {@link DeviceQuery}, not a copy.
     */
    public static List<DeviceQuery> getWaitForDevices() {
        return waitForDevices;
    }
    /**
     * Clears internal list
     */
    public static void clearWaitForDevices() {
        waitForDevices.clear();
    }

    public static void addToProcessedDevices(final BDAddressAndType a, final String n) {
        devicesProcessed.add( new DeviceID(a, n) );
    }
    public static boolean isDeviceProcessed(final BDAddressAndType a) {
        return devicesProcessed.contains( new DeviceID(a, null) );
    }
    public static int getProcessedDeviceCount() {
        return devicesProcessed.size();
    }
    public static String getProcessedDevicesString() {
        return Arrays.toString(devicesProcessed.toArray());
    }
    /**
     * Returns a copy of the current collection of processed {@link DeviceID}.
     */
    public static List<DeviceID> getProcessedDevices() {
        return new ArrayList<DeviceID>(devicesProcessed);
    }
    /**
     * Clears internal list
     */
    public static void clearProcessedDevices() {
        devicesProcessed.clear();
    }

    /**
     * Interface for user defined {@link DeviceQuery} matching criteria and algorithm.
     */
    public static interface DeviceQueryMatch {
        /**
         * Return {@code true} if the given {@code address} and/or {@code name} matches
         * with the {@link DeviceQuery}'s {@link DeviceQuery#addressSub addressSub} and/or
         * {@link DeviceQuery#nameSub nameSub}.
         * <p>
         * Example (lambda):
         * <pre>
         *    (final EUI48 a, final String n, final DeviceQuery q) -> {
         *     return q.isEUI48Sub() ? a.contains(q.addressSub) : n.indexOf(q.nameSub) >= 0;
         *     }
         * </pre>
         * </p>
         */
        public boolean match(final EUI48 address, final String name, final DeviceQuery q);
    }

    /**
     * Returns {@code true} if the given {@code address} and/or {@code name}
     * {@link DeviceQueryMatch#match(EUI48, String, DeviceQuery) matches}
     * any of the {@link #addToWaitForDevices(String) awaited devices}.
     * <p>
     * Matching criteria and algorithm is defined by the given {@link DeviceQueryMatch}.
     * </p>
     * @see #isWaitingForDevice(EUI48, String)
     */
    public static boolean isWaitingForDevice(final EUI48 address, final String name,
                                             final DeviceQueryMatch m) {
        for(final Iterator<DeviceQuery> it=waitForDevices.iterator(); it.hasNext(); ) {
            final DeviceQuery q = it.next();
            if( m.match(address, name, q) ) {
                return true;
            }
        }
        return false;
    }
    /**
     * Returns {@code true} if the given {@code address} and/or {@code name}
     * {@link DeviceQueryMatch#match(EUI48, String, DeviceQuery) matches}
     * any of the {@link #addToWaitForDevices(String) awaited devices}.
     * <p>
     * Matching criteria is either the awaited device's {@link DeviceQuery#addressSub}
     * or {@link DeviceQuery#nameSub}, whichever is set.
     * </p>
     * <p>
     * Matching algorithm is a simple {@code contains} pattern match,
     * i.e. the given {@code address} or {@code name} contains the corresponding {@link DeviceQuery} element.
     * </p>
     * @see #isWaitingForDevice(EUI48, String, DeviceQueryMatch)
     */
    public static boolean isWaitingForDevice(final EUI48 address, final String name) {
        return isWaitingForDevice( address, name,
                                   (final EUI48 a, final String n, final DeviceQuery q) -> {
                                    return q.isEUI48Sub() ? a.contains(q.addressSub) : n.indexOf(q.nameSub) >= 0;
                                 });
    }

    /**
     * Returns {@code true} if all {@link #addToWaitForDevices(String) awaited devices}
     * have been {@link #addToProcessedDevices(BDAddressAndType, String) processed}.
     * <p>
     * Matching criteria and algorithm is defined by the given {@link DeviceQueryMatch}.
     * </p>
     * @see #areAllDevicesProcessed()
     */
    public static boolean areAllDevicesProcessed(final DeviceQueryMatch m) {
        for(final Iterator<DeviceQuery> it1=waitForDevices.iterator(); it1.hasNext(); ) {
            final DeviceQuery q = it1.next();
            final Iterator<DeviceID> it2=devicesProcessed.iterator();
            while( it2.hasNext() ) {
                final DeviceID id = it2.next();
                if( m.match(id.addressAndType.address, id.name, q) ) {
                    break;
                }
            }
            if( !it2.hasNext() ) {
                return false;
            }
        }
        return true;
    }
    /**
     * Returns {@code true} if all {@link #addToWaitForDevices(String) awaited devices}
     * have been {@link #addToProcessedDevices(BDAddressAndType, String) processed}.
     * <p>
     * Matching criteria is either the awaited device's {@link DeviceQuery#addressSub}
     * or {@link DeviceQuery#nameSub}, whichever is set.
     * </p>
     * <p>
     * Matching algorithm is a simple {@code contains} pattern match,
     * i.e. the processed {@link DeviceID} contains one element of {@link DeviceQuery}.
     * </p>
     * @see #areAllDevicesProcessed(DeviceQueryMatch)
     */
    public static boolean areAllDevicesProcessed() {
        return areAllDevicesProcessed( (final EUI48 a, final String n, final DeviceQuery q) -> {
                                        return q.isEUI48Sub() ? a.contains(q.addressSub) : n.indexOf(q.nameSub) >= 0;
                                     });
    }


    public static void addToProcessingDevices(final BDAddressAndType a, final String n) {
        devicesInProcessing.add( new DeviceID(a, n) );
    }
    public static boolean removeFromProcessingDevices(final BDAddressAndType a) {
        return devicesInProcessing.remove( new DeviceID(a, null) );
    }
    public static boolean isDeviceProcessing(final BDAddressAndType a) {
        return devicesInProcessing.contains( new DeviceID(a, null) );
    }
    public static int getProcessingDeviceCount() {
        return devicesInProcessing.size();
    }
    /**
     * Returns a copy of the current collection of processing {@link DeviceID}.
     */
    public static List<DeviceID> getProcessingDevices() {
        return new ArrayList<DeviceID>(devicesInProcessing);
    }
    /**
     * Clears internal list
     */
    public static void clearProcessingDevices() {
        devicesInProcessing.clear();
    }
}
