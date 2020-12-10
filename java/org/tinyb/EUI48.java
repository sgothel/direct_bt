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

package org.tinyb;

import java.util.Arrays;

/**
 * A packed 48 bit EUI-48 identifier, formerly known as MAC-48
 * or simply network device MAC address (Media Access Control address).
 */
public class EUI48 {
    /** EUI48 MAC address matching any device, i.e. '0:0:0:0:0:0'. */
    public static final EUI48 ANY_DEVICE = new EUI48( new byte[] { (byte)0x00, (byte)0x00, (byte)0x00, (byte)0x00, (byte)0x00, (byte)0x00 } );
    /** EUI48 MAC address matching all device, i.e. 'ff:ff:ff:ff:ff:ff'. */
    public static final EUI48 ALL_DEVICE = new EUI48( new byte[] { (byte)0xff, (byte)0xff, (byte)0xff, (byte)0xff, (byte)0xff, (byte)0xff } );
    /** EUI48 MAC address matching local device, i.e. '0:0:0:ff:ff:ff'. */
    public static final EUI48 LOCAL_DEVICE = new EUI48( new byte[] { (byte)0x00, (byte)0x00, (byte)0x00, (byte)0xff, (byte)0xff, (byte)0xff } );

    /** The 6 byte EUI48 address */
    public final byte b[/* 6 octets */];

    /**
     * Size of the byte stream representation in bytes
     * @see #getStream(byte[], int)
     */
    private static final int byte_size = 6;

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

    @Override
    public final boolean equals(final Object obj)
    {
        if(this == obj) {
            return true;
        }
        if (obj == null || !(obj instanceof EUI48)) {
            return false;
        }
        final EUI48 other = (EUI48)obj;
        return Arrays.equals(b, other.b);
    }

    @Override
    public final int hashCode() { return java.util.Arrays.hashCode(b); }

    /**
     * Method transfers all bytes representing this instance into the given
     * destination array at the given position.
     * <p>
     * Implementation is consistent with {@link #EUI48(byte[], int)}.
     * </p>
     * @param sink the destination array
     * @param pos starting position in the destination array
     * @return the given destination array for chaining
     * @see #EUI48(byte[], int)
     */
    public final byte[] getStream(final byte[] sink, final int pos) {
        if( byte_size > ( sink.length - pos ) ) {
            throw new IllegalArgumentException("Stream ( "+sink.length+" - "+pos+" ) < "+byte_size+" bytes");
        }
        System.arraycopy(b, 0, sink, pos, byte_size);
        return sink;
    }

    public BLERandomAddressType getBLERandomAddressType(final BluetoothAddressType addressType) {
        if( BluetoothAddressType.BDADDR_LE_RANDOM != addressType ) {
            return BLERandomAddressType.UNDEFINED;
        }
        final byte high2 = (byte) ( ( b[5] >> 6 ) & 0x03 );
        return BLERandomAddressType.get(high2);
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
            BluetoothUtils.byteHexString(sb, b[i], false /* lowerCase */);
            if( 0 < i ) {
                sb.append(":");
            }
        }
        return sb.toString();
    }
}
