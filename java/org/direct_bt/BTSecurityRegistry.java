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
import java.util.Iterator;
import java.util.List;

/**
 * Application toolkit providing BT security setup and its device association
 * on a pattern matching basis, i.e. EUI48Sub or name-sub.
 */
public class BTSecurityRegistry {
    public static final int NO_PASSKEY = -1;

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

        public boolean matches(final EUI48Sub addressSub) {
            return addrSub.length > 0 && addressSub.contains(addrSub);
        }
        public boolean matches(final EUI48 address) {
            return addrSub.length > 0 && address.contains(addrSub);
        }
        public boolean matches(final String name) {
            return nameSub.length() > 0 && name.indexOf(nameSub) >= 0;
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

    public static BTSecurityRegistry.Entry get(final EUI48 addr) {
        for(final Iterator<BTSecurityRegistry.Entry> i=devicesSecDetails.iterator(); i.hasNext(); ) {
            final BTSecurityRegistry.Entry sd = i.next();
            if( sd.matches(addr) ) {
                return sd;
            }
        }
        return null;
    }
    public static BTSecurityRegistry.Entry get(final EUI48Sub addrSub) {
        for(final Iterator<BTSecurityRegistry.Entry> i=devicesSecDetails.iterator(); i.hasNext(); ) {
            final BTSecurityRegistry.Entry sd = i.next();
            if( sd.matches(addrSub) ) {
                return sd;
            }
        }
        return null;
    }
    public static BTSecurityRegistry.Entry get(final String nameSub) {
        for(final Iterator<BTSecurityRegistry.Entry> i=devicesSecDetails.iterator(); i.hasNext(); ) {
            final BTSecurityRegistry.Entry sd = i.next();
            if( sd.matches(nameSub) ) {
                return sd;
            }
        }
        return null;
    }

    public static BTSecurityRegistry.Entry getOrCreate(final String addrOrNameSub) {
        final EUI48Sub addr1 = new EUI48Sub();
        final StringBuilder errmsg = new StringBuilder();
        BTSecurityRegistry.Entry sec = null;
        if( EUI48Sub.scanEUI48Sub(addrOrNameSub, addr1, errmsg) ) {
            sec = get(addr1);
            if( null == sec ) {
                sec = new BTSecurityRegistry.Entry( addr1 );
                devicesSecDetails.add(sec);
            }
        } else {
            sec = get(addrOrNameSub);
            if( null == sec ) {
                sec = new BTSecurityRegistry.Entry( addrOrNameSub );
                devicesSecDetails.add(sec);
            }
        }
        return sec;
    }

    public static String allToString() {
        return Arrays.toString( devicesSecDetails.toArray() );
    }
}
