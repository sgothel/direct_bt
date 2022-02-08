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

import org.jau.net.EUI48;

/**
 * Unique Bluetooth {@link EUI48} address and {@link BDAddressType} tuple.
 * <p>
 * Bluetooth EUI48 address itself is not unique as it requires the {@link BDAddressType} bits.<br>
 * E.g. there could be two devices with the same {@link EUI48} address, one using {@link BDAddressType#BDADDR_LE_PUBLIC}
 * and one using {@link BDAddressType#BDADDR_LE_RANDOM} being a {@link BLERandomAddressType#RESOLVABLE_PRIVAT}.
 * </p>
 */
public class BDAddressAndType {
    /** Using {@link EUI48#ANY_DEVICE} and {@link BDAddressType#BDADDR_BREDR} to match any BREDR device. */
    public static final BDAddressAndType ANY_BREDR_DEVICE = new BDAddressAndType( EUI48.ANY_DEVICE, BDAddressType.BDADDR_BREDR );

    /**
     * Using {@link EUI48#ANY_DEVICE} and {@link BDAddressType#BDADDR_UNDEFINED} to match any device.<br>
     * This constant is suitable to {@link #matches(BDAddressAndType) any device.
     */
    public static final BDAddressAndType ANY_DEVICE = new BDAddressAndType( EUI48.ANY_DEVICE, BDAddressType.BDADDR_UNDEFINED );

    public EUI48 address;
    public BDAddressType type;

    private volatile int hash; // default 0, cache

    public BDAddressAndType(final EUI48 address, final BDAddressType type) {
        this.address = address;
        this.type = type;
    }

    public BDAddressAndType() {
        this.address = new EUI48();
        this.type = BDAddressType.BDADDR_UNDEFINED;
    }

    /**
     * Returns true if the {@link BDAddressType} is a LE address type.
     */
    public boolean isLEAddress() {
        return BDAddressType.BDADDR_LE_PUBLIC == type || BDAddressType.BDADDR_LE_RANDOM == type;
    }

    /**
     * Returns true if the given {@link BDAddressType} is a BREDR address type.
     */
    public boolean isBREDRAddress() { return BDAddressType.BDADDR_BREDR == type; }

    /**
     * Returns the {@link BLERandomAddressType}.
     * <p>
     * If {@link BDAddressType} is {@link BDAddressType::BDADDR_LE_RANDOM},
     * method shall return a valid value other than {@link BLERandomAddressType::UNDEFINED}.
     * </p>
     * <p>
     * If {@link BDAddressType} is not {@link BDAddressType::BDADDR_LE_RANDOM},
     * method shall return {@link BLERandomAddressType::UNDEFINED}.
     * </p>
     * @since 2.2.0
     */
    public static BLERandomAddressType getBLERandomAddressType(final EUI48 address, final BDAddressType addressType) {
        if( BDAddressType.BDADDR_LE_RANDOM != addressType ) {
            return BLERandomAddressType.UNDEFINED;
        }
        final byte high2 = (byte) ( ( address.b[5] >> 6 ) & 0x03 );
        return BLERandomAddressType.get(high2);
    }

    /**
     * Returns the {@link BLERandomAddressType}.
     * <p>
     * If {@link #type} is {@link BDAddressType::BDADDR_LE_RANDOM},
     * method shall return a valid value other than {@link BLERandomAddressType::UNDEFINED}.
     * </p>
     * <p>
     * If {@link #type} is not {@link BDAddressType::BDADDR_LE_RANDOM},
     * method shall return {@link BLERandomAddressType::UNDEFINED}.
     * </p>
     * @since 2.0.0
     */
    public BLERandomAddressType getBLERandomAddressType() {
        return getBLERandomAddressType(address, type);
    }

    /**
     * Returns true if both devices match, i.e. equal address
     * and equal type or at least one type is {@link BDAddressType#BDADDR_UNDEFINED}.
     */
    public final boolean matches(final BDAddressAndType o) {
        if(this == o) {
            return true;
        }
        if (o == null) {
            return false;
        }
        return address.equals(o.address) &&
               ( type == o.type ||
                 BDAddressType.BDADDR_UNDEFINED == type ||
                 BDAddressType.BDADDR_UNDEFINED == o.type );
    }

    /**
     * If both types are of {@link BDAddressAndType}, it compares their {@link EUI48} address and {@link BDAddressType}.
     * <p>
     * {@inheritDoc}
     * </p>
     */
    @Override
    public final boolean equals(final Object obj) {
        if(this == obj) {
            return true;
        }
        if (obj == null || !(obj instanceof BDAddressAndType)) {
            return false;
        }
        final BDAddressAndType o =  (BDAddressAndType)obj;
        return address.equals(o.address) && type == o.type;
    }

    /**
     * {@inheritDoc}
     * <p>
     * Implementation uses a lock-free volatile cache.
     * </p>
     * @see #clearHash()
     */
    @Override
    public final int hashCode() {
        int h = hash;
        if( 0 == h ) {
            // 31 * x == (x << 5) - x
            h = address.hashCode();
            h = ( ( h << 5 ) - h ) + type.value;
            hash = h;
        }
        return h;
    }

    /**
     * Method clears the cached hash value.
     * @see #clear()
     */
    public void clearHash() { hash = 0; }

    /**
     * Method clears the underlying byte array {@link #b} and cached hash value.
     * @see #clearHash()
     */
    public void clear() {
        hash = 0;
        address.clear();
        type = BDAddressType.BDADDR_UNDEFINED;
    }

    @Override
    public final String toString() {
        final BLERandomAddressType leRandomAddressType = getBLERandomAddressType();
        final String leaddrtype;
        if( BLERandomAddressType.UNDEFINED != leRandomAddressType ) {
            leaddrtype = ", random "+leRandomAddressType;
        } else {
            leaddrtype = "";
        }
        return "["+address.toString()+", "+type+leaddrtype+"]";
    }
}
