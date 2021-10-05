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

import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Iterator;
import java.util.List;

import org.jau.net.EUI48;
import org.jau.net.EUI48Sub;

/**
 * Application toolkit providing BT security setup and its device association
 * on a pattern matching basis, i.e. EUI48Sub or name-sub.
 */
public class BTSecurityRegistry {
    public static final int NO_PASSKEY = -1;

    /**
     * Entry specifying SMP security details associated to a device query via {@link EUI48Sub} or {@code nameSub}.
     */
    public static class Entry {
        public EUI48Sub addrSub;
        public String nameSub;

        public BTSecurityLevel sec_level = BTSecurityLevel.UNSET;
        public SMPIOCapability io_cap = SMPIOCapability.UNSET;
        public SMPIOCapability io_cap_auto = SMPIOCapability.UNSET;
        public int passkey = NO_PASSKEY;

        public Entry(final EUI48Sub addrSub) {
            this.addrSub = addrSub;
            this.nameSub = "";
        }
        public Entry(final String nameSub) {
            this.addrSub = EUI48Sub.ALL_DEVICE;
            this.nameSub = nameSub;
        }

        public boolean isSecLevelOrIOCapSet() {
            return SMPIOCapability.UNSET != io_cap ||  BTSecurityLevel.UNSET != sec_level;
        }
        public BTSecurityLevel getSecLevel() { return sec_level; }
        public SMPIOCapability getIOCap() { return io_cap; }

        public boolean isSecurityAutoEnabled() { return SMPIOCapability.UNSET != io_cap_auto; }
        public SMPIOCapability getSecurityAutoIOCap() { return io_cap_auto; }

        public int getPairingPasskey() { return passkey; }

        public boolean getPairingNumericComparison() { return true; }

        @Override
        public String toString() {
            final String id = EUI48Sub.ALL_DEVICE.equals(addrSub) ? "'"+nameSub+"'" : addrSub.toString();
            return "BTSecurityDetail["+id+", lvl "+sec_level+", io "+io_cap+", auto-io "+io_cap_auto+", passkey "+passkey+"]";
        }
    }
    static private List<BTSecurityRegistry.Entry> devicesSecDetails = new ArrayList<BTSecurityRegistry.Entry>();

    /**
     * Interface for user defined {@link EUI48} address and name {@link BTSecurityRegistry.Entry} matching criteria and algorithm.
     */
    public static interface AddressNameEntryMatch {
        /**
         * Return {@code true} if the given {@code address} or {@code name} matches
         * with the {@link BTSecurityRegistry.Entry}.
         *
         * @param address {@link EUI48} address
         * @param name optional name, maybe empty
         * @param e {@link BTSecurityRegistry.Entry} entry
         */
        public boolean match(final EUI48 address, final String name, final BTSecurityRegistry.Entry e);
    }
    /**
     * Interface for user defined {@link EUI48Sub} addressSub and name {@link BTSecurityRegistry.Entry} matching criteria and algorithm.
     */
    public static interface AddressSubNameEntryMatch {
        /**
         * Return {@code true} if the given {@code addressSub} or {@code name} matches
         * with the {@link BTSecurityRegistry.Entry}.
         *
         * @param addressSub {@link EUI48Sub} address
         * @param name optional name, maybe empty
         * @param e {@link BTSecurityRegistry.Entry} entry
         */
        public boolean match(final EUI48Sub addressSub, final String name, final BTSecurityRegistry.Entry e);
    }
    /**
     * Interface for user defined name {@link BTSecurityRegistry.Entry} matching criteria and algorithm.
     */
    public static interface NameEntryMatch {
        /**
         * Return {@code true} if the given {@code name} matches
         * with the {@link BTSecurityRegistry.Entry}.
         *
         * @param name
         * @param e {@link BTSecurityRegistry.Entry} entry
         */
        public boolean match(final String name, final BTSecurityRegistry.Entry e);
    }

    /**
     * Returns a matching {@link BTSecurityRegistry.Entry} with the given {@code addr} and/or {@code name}
     * <p>
     * Matching criteria and algorithm is defined by the given {@link AddressNameEntryMatch}.
     * </p>
     */
    public static BTSecurityRegistry.Entry get(final EUI48 addr, final String name, final AddressNameEntryMatch m) {
        for(final Iterator<BTSecurityRegistry.Entry> i=devicesSecDetails.iterator(); i.hasNext(); ) {
            final BTSecurityRegistry.Entry sd = i.next();
            if( m.match(addr, name, sd) ) {
                return sd;
            }
        }
        return null;
    }
    /**
     * Returns a matching {@link BTSecurityRegistry.Entry} with the given {@code addrSub} and/or {@code name}.
     * <p>
     * Matching criteria and algorithm is defined by the given {@link AddressSubNameEntryMatch}.
     * </p>
     */
    public static BTSecurityRegistry.Entry get(final EUI48Sub addrSub, final String name, final AddressSubNameEntryMatch m) {
        for(final Iterator<BTSecurityRegistry.Entry> i=devicesSecDetails.iterator(); i.hasNext(); ) {
            final BTSecurityRegistry.Entry sd = i.next();
            if( m.match(addrSub, name, sd) ) {
                return sd;
            }
        }
        return null;
    }
    /**
     * Returns a matching {@link BTSecurityRegistry.Entry} with the given {@code name}.
     * <p>
     * Matching criteria and algorithm is defined by the given {@link NameEntryMatch}.
     * </p>
     */
    public static BTSecurityRegistry.Entry get(final String name, final NameEntryMatch m) {
        for(final Iterator<BTSecurityRegistry.Entry> i=devicesSecDetails.iterator(); i.hasNext(); ) {
            final BTSecurityRegistry.Entry sd = i.next();
            if( m.match(name, sd) ) {
                return sd;
            }
        }
        return null;
    }

    /**
     * Returns a matching {@link BTSecurityRegistry.Entry},
     * - which {@link BTSecurityRegistry.Entry#addrSub} is set and the given {@code addr} starts with {@link BTSecurityRegistry.Entry#addrSub}, or
     * - which {@link BTSecurityRegistry.Entry#nameSub} is set and the given {@code name} starts with {@link BTSecurityRegistry.Entry#nameSub}.
     *
     * Otherwise {@code null} is returned.
     */
    public static BTSecurityRegistry.Entry getStartOf(final EUI48 addr, final String name) {
        return get(addr, name, (final EUI48 a, final String n, final BTSecurityRegistry.Entry e) -> {
           return ( e.addrSub.length > 0 && 0 == a.indexOf(e.addrSub, ByteOrder.BIG_ENDIAN) ) ||
                  (e.nameSub.length() > 0 && n.startsWith(e.nameSub) );
        });
    }
    /**
     * Returns a matching {@link BTSecurityRegistry.Entry},
     * - which {@link BTSecurityRegistry.Entry#addrSub} is set and the given {@code addrSub} starts with {@link BTSecurityRegistry.Entry#addrSub}, or
     * - which {@link BTSecurityRegistry.Entry#nameSub} is set and the given {@code name} starts with {@link BTSecurityRegistry.Entry#nameSub}.
     *
     * Otherwise {@code null} is returned.
     */
    public static BTSecurityRegistry.Entry getStartOf(final EUI48Sub addrSub, final String name) {
        return get(addrSub, name, (final EUI48Sub as, final String n, final BTSecurityRegistry.Entry e) -> {
           return ( e.addrSub.length > 0 && 0 == as.indexOf(e.addrSub, ByteOrder.BIG_ENDIAN) ) ||
                  ( e.nameSub.length() > 0 && n.startsWith(e.nameSub) );
        });
    }
    /**
     * Returns a matching {@link BTSecurityRegistry.Entry},
     * - which {@link BTSecurityRegistry.Entry#nameSub} is set and the given {@code name} starts with {@link BTSecurityRegistry.Entry#nameSub}.
     *
     * Otherwise {@code null} is returned.
     */
    public static BTSecurityRegistry.Entry getStartOf(final String name) {
        return get(name, (final String n, final BTSecurityRegistry.Entry e) -> {
           return e.nameSub.length() > 0 && n.startsWith(e.nameSub);
        });
    }

    /**
     * Returns a matching {@link BTSecurityRegistry.Entry},
     * - which {@link BTSecurityRegistry.Entry#addrSub} is set and the given {@code addrSub} is equal with {@link BTSecurityRegistry.Entry#addrSub}, or
     * - which {@link BTSecurityRegistry.Entry#nameSub} is set and the given {@code name} is equal with {@link BTSecurityRegistry.Entry#nameSub}.
     *
     * Otherwise {@code null} is returned.
     */
    public static BTSecurityRegistry.Entry getEqual(final EUI48Sub addrSub, final String name) {
        return get(addrSub, name, (final EUI48Sub as, final String n, final BTSecurityRegistry.Entry e) -> {
           return ( e.addrSub.length > 0 && as.equals(e.addrSub) ) ||
                  ( e.nameSub.length() > 0 && n.equals(e.nameSub) );
        });
    }
    /**
     * Returns a matching {@link BTSecurityRegistry.Entry},
     * - which {@link BTSecurityRegistry.Entry#nameSub} is set and the given {@code name} is equal with {@link BTSecurityRegistry.Entry#nameSub}.
     *
     * Otherwise {@code null} is returned.
     */
    public static BTSecurityRegistry.Entry getEqual(final String name) {
        return get(name, (final String n, final BTSecurityRegistry.Entry e) -> {
           return e.nameSub.length() > 0 && n.equals(e.nameSub);
        });
    }

    /**
     * Returns the reference of the current list of {@link BTSecurityRegistry.Entry}, not a copy.
     */
    public static List<BTSecurityRegistry.Entry> getEntries() {
        return devicesSecDetails;
    }

    /**
     * Determines whether the given {@code addrOrNameSub} is a {@link EUI48Sub} or just a {@code name}
     * and retrieves an entry. If no entry exists, creates a new entry.
     * <p>
     * Implementation uses {@link #getEqual(EUI48Sub, String)} to find a pre-existing entry.
     * </p>
     * @param addrOrNameSub either a {@link EUI48Sub} or just a name
     * @return new or existing instance
     */
    public static BTSecurityRegistry.Entry getOrCreate(final String addrOrNameSub) {
        final EUI48Sub addr1 = new EUI48Sub();
        final StringBuilder errmsg = new StringBuilder();
        BTSecurityRegistry.Entry sec = null;
        if( EUI48Sub.scanEUI48Sub(addrOrNameSub, addr1, errmsg) ) {
            sec = getEqual(addr1, "");
            if( null == sec ) {
                sec = new BTSecurityRegistry.Entry( addr1 );
                devicesSecDetails.add(sec);
            }
        } else {
            sec = getEqual(addrOrNameSub);
            if( null == sec ) {
                sec = new BTSecurityRegistry.Entry( addrOrNameSub );
                devicesSecDetails.add(sec);
            }
        }
        return sec;
    }

    /**
     * Clears internal list
     */
    public static void clear() {
        devicesSecDetails.clear();
    }

    public static String allToString() {
        return Arrays.toString( devicesSecDetails.toArray() );
    }
}
