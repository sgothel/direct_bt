/**
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2021 Gothel Software e.K.
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

import java.util.Arrays;

/**
 * A 48 bit EUI-48 sub-identifier, see {@link EUI48}.
 */
public class EUI48Sub {
    /** EUI48Sub MAC address matching any device, i.e. '0:0:0:0:0:0'. */
    public static final EUI48Sub ANY_DEVICE = new EUI48Sub( new byte[] { (byte)0x00, (byte)0x00, (byte)0x00, (byte)0x00, (byte)0x00, (byte)0x00 }, 0, 6 );
    /** EUI48Sub MAC address matching all device, i.e. 'ff:ff:ff:ff:ff:ff'. */
    public static final EUI48Sub ALL_DEVICE = new EUI48Sub( new byte[] { (byte)0xff, (byte)0xff, (byte)0xff, (byte)0xff, (byte)0xff, (byte)0xff }, 0, 6 );
    /** EUI48Sub MAC address matching local device, i.e. '0:0:0:ff:ff:ff'. */
    public static final EUI48Sub LOCAL_DEVICE = new EUI48Sub( new byte[] { (byte)0x00, (byte)0x00, (byte)0x00, (byte)0xff, (byte)0xff, (byte)0xff }, 0, 6 );

    /**
     * The EUI48 sub-address, always 6 bytes reserved.
     */
    public final byte b[/* 6 octets */];

    private volatile int hash; // default 0, cache

    /**
     * The actual length in bytes of the EUI48 sub-address, less or equal 6 bytes.
     */
    public int length;

    /**
     * Fills given EUI48Sub instance via given string representation.
     * <p>
     * Implementation is consistent with {@link #toString()}.
     * </p>
     * @param str a string of less or equal of 17 characters representing less or equal of 6 bytes as hexadecimal numbers separated via colon,
     * e.g. {@code "01:02:03:0A:0B:0C"}, {@code "01:02:03:0A"}, {@code ":"}, {@code ""}.
     * @param dest EUI48Sub to set its value
     * @param errmsg error parsing message if returning false
     * @return true if successful, otherwise false
     * @see #EUI48Sub(String)
     * @see #toString()
     */
    public static boolean scanEUI48Sub(final String str, final EUI48Sub dest, final StringBuilder errmsg) {
        final int str_len = str.length();
        dest.clear();

        if( 17 < str_len ) { // not exceeding EUI48.byte_size
            errmsg.append("EUI48 sub-string must be less or equal length 17 but "+str.length()+": "+str);
            return false;
        }
        final byte b_[] = new byte[ 6 ]; // intermediate result high -> low
        int len_ = 0;
        int j=0;
        try {
            boolean exp_colon = false;
            while( j+1 < str_len /* && byte_count_ < byte_size */ ) { // min 2 chars left
                final boolean is_colon = ':' == str.charAt(j);
                if( exp_colon && !is_colon ) {
                    errmsg.append("EUI48Sub sub-string not in format '01:02:03:0A:0B:0C', but '"+str+"', colon missing, pos "+j+", len "+str_len);
                    return false;
                } else if( is_colon ) {
                    ++j;
                    exp_colon = false;
                } else {
                    b_[len_] = Integer.valueOf(str.substring(j, j+2), 16).byteValue(); // b_: high->low
                    j += 2;
                    ++len_;
                    exp_colon = true;
                }
            }
            dest.length = len_;
            for(j=0; j<len_; ++j) { // swap low->high
                dest.b[j] = b_[len_-1-j];
            }
        } catch (final NumberFormatException e) {
            errmsg.append("EUI48 sub-string not in format '01:02:03:0A:0B:0C' but "+str+", pos "+j+", len "+str_len);
            return false;
        }
        return true;
    }

    /** Construct empty unset instance. */
    public EUI48Sub() {
        b = new byte[6];
        length = 0;
    }

    /**
     * Construct a sub EUI48 via given string representation.
     * <p>
     * Implementation is consistent with {@link #toString()}.
     * </p>
     * @param str a string of less or equal of 17 characters representing less or equal of 6 bytes as hexadecimal numbers separated via colon,
     * e.g. {@code "01:02:03:0A:0B:0C"}, {@code "01:02:03:0A"}, {@code ":"}, {@code ""}.
     * @see #toString()
     * @throws IllegalArgumentException if given string doesn't comply with EUI48
     */
    public EUI48Sub(final String str) throws IllegalArgumentException {
        final StringBuilder errmsg = new StringBuilder();
        b = new byte[ 6 ];
        if( !scanEUI48Sub(str, this, errmsg) ) {
            throw new IllegalArgumentException(errmsg.toString());
        }
    }

    /** Construct instance via given source byte array */
    public EUI48Sub(final byte stream[], final int pos, final int len_) {
        if( len_ > EUI48.byte_size || pos + len_ > stream.length ) {
            throw new IllegalArgumentException("EUI48 stream ( pos "+pos+", len "+len_+" > EUI48 size "+EUI48.byte_size+" or stream.length "+stream.length);
        }
        b = new byte[6];
        System.arraycopy(stream, pos, b, 0,  len_);
        length = len_;
    }

    @Override
    public final boolean equals(final Object obj) {
        if(this == obj) {
            return true;
        }
        if (obj == null || !(obj instanceof EUI48Sub)) {
            return false;
        }
        final int length2 = ((EUI48Sub)obj).length;
        if( length != length2 ) {
            return false;
        }
        final byte[] b2 = ((EUI48Sub)obj).b;
        return Arrays.equals(b, 0, length, b2, 0, length2);
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
            h = length;
            for(int i=0; i<length; i++) {
                h = ( ( h << 5 ) - h ) + b[i];
            }
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
     * Method clears the underlying byte array {@link #b} and sets length to zero. The cached hash value is also cleared.
     * @see #clearHash()
     */
    public void clear() {
        hash = 0;
        b[0] = 0; b[1] = 0; b[2] = 0;
        b[3] = 0; b[4] = 0; b[5] = 0;
        length = 0;
    }

    /**
     * Find index of needle within haystack.
     * @param haystack_b haystack data
     * @param haystack_length haystack length
     * @param needle_b needle data
     * @param needle_length needle length
     * @return index of first element of needle within haystack or -1 if not found. If the needle length is zero, 0 (found) is returned.
     */
    public static int indexOf(final byte haystack_b[], final int haystack_length,
                              final byte needle_b[], final int needle_length) {
        if( 0 == needle_length ) {
            return 0;
        }
        if( haystack_length < needle_length ) {
            return -1;
        }
        final byte first = needle_b[0];
        final int outerEnd = haystack_length - needle_length + 1; // exclusive

        for (int i = 0; i < outerEnd; i++) {
            // find first char of other
            while( haystack_b[i] != first ) {
                if( ++i == outerEnd ) {
                    return -1;
                }
            }
            if( i < outerEnd ) { // otherLen chars left to match?
                // continue matching other chars
                final int innerEnd = i + needle_length; // exclusive
                int j = i, k=0;
                do {
                    if( ++j == innerEnd ) {
                        return i; // gotcha
                    }
                } while( haystack_b[j] == needle_b[++k] );
            }
        }
        return -1;
    }

    /**
     * Finds the index of given EUI48Sub needle within this instance haystack.
     * @param needle
     * @return index of first element of needle within this instance haystack or -1 if not found. If the needle length is zero, 0 (found) is returned.
     */
    public int indexOf(final EUI48Sub needle) {
        return indexOf(b, length, needle.b, needle.length);
    }

    /**
     * Returns true, if given EUI48Sub needle is contained in this instance haystack.
     * <p>
     * If the sub is zero, true is returned.
     * </p>
     */
    public boolean contains(final EUI48Sub needle) {
        return 0 <= indexOf(needle);
    }

    /**
     * {@inheritDoc}
     * <p>
     * Returns the EUI48 sub-string representation,
     * less or equal 17 characters representing less or equal 6 bytes as upper case hexadecimal numbers separated via colon,
     * e.g. {@code "01:02:03:0A:0B:0C"}, {@code "01:02:03:0A"}, {@code ":"}, {@code ""}.
     * </p>
     */
    @Override
    public final String toString() {
        // str_len = 2 * len + ( len - 1 )
        // str_len = 3 * len - 1
        // len = ( str_len + 1 ) / 3
        if( 0 < length ) {
            final StringBuilder sb = new StringBuilder(3 * length - 1);
            for(int i=length-1; 0 <= i; i--) {
                BTUtils.byteHexString(sb, b[i], false /* lowerCase */);
                if( 0 < i ) {
                    sb.append(":");
                }
            }
            return sb.toString();
        } else {
            return new String(":");
        }
    }
}
