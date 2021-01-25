/**
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2021 Gothel Software e.K.
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

/**
 * A packed 48 bit EUI-48 identifier, formerly known as MAC-48
 * or simply network device MAC address (Media Access Control address).
 * <p>
 * Implementation caches the hash value {@link #hashCode()},
 * hence users shall take special care when mutating the
 * underlying data {@link #b}, read its API notes.
 * </p>
 */
public class EUI48 {
    /** EUI48 MAC address matching any device, i.e. '0:0:0:0:0:0'. */
    public static final EUI48 ANY_DEVICE = new EUI48( new byte[] { (byte)0x00, (byte)0x00, (byte)0x00, (byte)0x00, (byte)0x00, (byte)0x00 } );
    /** EUI48 MAC address matching all device, i.e. 'ff:ff:ff:ff:ff:ff'. */
    public static final EUI48 ALL_DEVICE = new EUI48( new byte[] { (byte)0xff, (byte)0xff, (byte)0xff, (byte)0xff, (byte)0xff, (byte)0xff } );
    /** EUI48 MAC address matching local device, i.e. '0:0:0:ff:ff:ff'. */
    public static final EUI48 LOCAL_DEVICE = new EUI48( new byte[] { (byte)0x00, (byte)0x00, (byte)0x00, (byte)0xff, (byte)0xff, (byte)0xff } );

    /**
     * The 6 byte EUI48 address.
     * <p>
     * If modifying, it is the user's responsibility to avoid data races.<br>
     * Further, call {@link #clearHash()} after mutation is complete.
     * </p>
     */
    public final byte b[/* 6 octets */];

    private volatile int hash; // default 0, cache

    /**
     * Size of the byte stream representation in bytes
     * @see #getStream(byte[], int)
     */
    /* pp */ static final int byte_size = 6;

    /**
     * Construct instance via given string representation.
     * <p>
     * Implementation is consistent with {@link #toString()}.
     * </p>
     * @param str a string of exactly 17 characters representing 6 bytes as hexadecimal numbers separated via colon {@code "01:02:03:0A:0B:0C"}.
     * @see #toString()
     */
    public EUI48(final String str) throws IllegalArgumentException {
        if( 17 != str.length() ) {
            throw new IllegalArgumentException("EUI48 string not of length 17 but "+str.length()+": "+str);
        }
        b = new byte[byte_size];
        try {
            for(int i=0; i<byte_size; i++) {
                b[byte_size-1-i] = Integer.valueOf(str.substring(i*2+i, i*2+i+2), 16).byteValue();
            }
        } catch (final NumberFormatException e) {
            throw new IllegalArgumentException("EUI48 string not in format '01:02:03:0A:0B:0C' but "+str, e);
        }
    }

    /** Construct instance via given source byte array */
    public EUI48(final byte stream[], final int pos) {
        if( byte_size > ( stream.length - pos ) ) {
            throw new IllegalArgumentException("EUI48 stream ( "+stream.length+" - "+pos+" ) < "+byte_size+" bytes");
        }
        b = new byte[byte_size];
        System.arraycopy(stream, pos, b, 0,  byte_size);
    }

    /** Construct instance using the given address of the byte array */
    public EUI48(final byte address[]) {
        if( byte_size != address.length ) {
            throw new IllegalArgumentException("EUI48 stream "+address.length+" != "+byte_size+" bytes");
        }
        b = address;
    }

    /** Construct emoty unset instance. */
    public EUI48() {
        b = new byte[byte_size];
    }

    @Override
    public final boolean equals(final Object obj) {
        if(this == obj) {
            return true;
        }
        if (obj == null || !(obj instanceof EUI48)) {
            return false;
        }
        final byte[] b2 = ((EUI48)obj).b;
        return b[0] == b2[0] &&
               b[1] == b2[1] &&
               b[2] == b2[2] &&
               b[3] == b2[3] &&
               b[4] == b2[4] &&
               b[5] == b2[5];
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
            /**
            // final int p = 92821; // alternative with less collisions?
            final int p = 31; // traditional prime
            h = b[0];
            h = p * h + b[1];
            h = p * h + b[2];
            h = p * h + b[3];
            h = p * h + b[4];
            h = p * h + b[5];
            */
            // 31 * x == (x << 5) - x
            h = b[0];
            h = ( ( h << 5 ) - h ) + b[1];
            h = ( ( h << 5 ) - h ) + b[2];
            h = ( ( h << 5 ) - h ) + b[3];
            h = ( ( h << 5 ) - h ) + b[4];
            h = ( ( h << 5 ) - h ) + b[5];
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
        b[0] = 0;
        b[1] = 0;
        b[2] = 0;
        b[3] = 0;
        b[4] = 0;
        b[5] = 0;
    }

    /**
     * Method transfers all bytes representing a EUI48 from the given
     * source array at the given position into this instance and clears its cached hash value.
     * <p>
     * Implementation is consistent with {@link #EUI48(byte[], int)}.
     * </p>
     * @param source the source array
     * @param pos starting position in the source array
     * @see #EUI48(byte[], int)
     * @see #clearHash()
     */
    public final void putStream(final byte[] source, final int pos) {
        if( byte_size > ( source.length - pos ) ) {
            throw new IllegalArgumentException("Stream ( "+source.length+" - "+pos+" ) < "+byte_size+" bytes");
        }
        hash = 0;
        System.arraycopy(source, pos, b, 0, byte_size);
    }

    /**
     * Method transfers all bytes representing this instance into the given
     * destination array at the given position.
     * <p>
     * Implementation is consistent with {@link #EUI48(byte[], int)}.
     * </p>
     * @param sink the destination array
     * @param pos starting position in the destination array
     * @see #EUI48(byte[], int)
     */
    public final void getStream(final byte[] sink, final int pos) {
        if( byte_size > ( sink.length - pos ) ) {
            throw new IllegalArgumentException("Stream ( "+sink.length+" - "+pos+" ) < "+byte_size+" bytes");
        }
        System.arraycopy(b, 0, sink, pos, byte_size);
    }

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
    public BLERandomAddressType getBLERandomAddressType(final BDAddressType addressType) {
        if( BDAddressType.BDADDR_LE_RANDOM != addressType ) {
            return BLERandomAddressType.UNDEFINED;
        }
        final byte high2 = (byte) ( ( b[5] >> 6 ) & 0x03 );
        return BLERandomAddressType.get(high2);
    }

    /**
     * Finds the index of given EUI48Sub.
     */
    public int indexOf(final EUI48Sub other) {
        if( 0 == other.length ) {
            return 0;
        }
        final byte first = other.b[0];
        final int outerEnd = 6 - other.length + 1; // exclusive

        for (int i = 0; i < outerEnd; i++) {
            // find first char of other
            while( b[i] != first ) {
                if( ++i == outerEnd ) {
                    return -1;
                }
            }
            if( i < outerEnd ) { // otherLen chars left to match?
                // continue matching other chars
                final int innerEnd = i + other.length; // exclusive
                int j = i, k=0;
                do {
                    if( ++j == innerEnd ) {
                        return i; // gotcha
                    }
                } while( b[j] == other.b[++k] );
            }
        }
        return -1;
    }

    /**
     * Returns true, if given EUI48Sub is contained in here.
     * <p>
     * If the sub is zero, true is returned.
     * </p>
     */
    public boolean contains(final EUI48Sub other) {
        return 0 <= indexOf(other);
    }

    /**
     * {@inheritDoc}
     * <p>
     * Returns the EUI48 string representation,
     * exactly 17 characters representing 6 bytes as upper case hexadecimal numbers separated via colon {@code "01:02:03:0A:0B:0C"}.
     * </p>
     * @see #EUI48(String)
     */
    @Override
    public final String toString() {
        final StringBuilder sb = new StringBuilder(17);
        for(int i=byte_size-1; 0 <= i; i--) {
            BTUtils.byteHexString(sb, b[i], false /* lowerCase */);
            if( 0 < i ) {
                sb.append(":");
            }
        }
        return sb.toString();
    }
}
