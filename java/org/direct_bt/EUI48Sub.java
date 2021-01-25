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

/**
 * A 48 bit EUI-48 sub-identifier, see {@link EUI48}.
 */
public class EUI48Sub {
    /**
     * The <= 6 byte EUI48 sub-address.
     */
    public final byte b[];

    /**
     * The actual length in bytes of the EUI48 sub-address, less or equal 6 bytes.
     */
    public final int length;

    /**
     * Construct a sub EUI48 via given string representation.
     * <p>
     * Implementation is consistent with {@link #toString()}.
     * </p>
     * @param str a string of less or equal of 17 characters representing less or equal of 6 bytes as hexadecimal numbers separated via colon,
     * e.g. {@code "01:02:03:0A:0B:0C"}, {@code "01:02:03:0A"}, {@code ":"}, {@code ""}.
     * @see #toString()
     */
    public EUI48Sub(final String str) throws IllegalArgumentException {
        final int str_len = str.length();
        if( 17 < str_len ) { // not exceeding EUI48.byte_size
            throw new IllegalArgumentException("EUI48 sub-string must be less or equal length 17 but "+str.length()+": "+str);
        }
        final byte b_[] = new byte[ 6 ]; // intermediate result high -> low
        int len_ = 0;
        try {
            int j=0;
            while( j+1 < str_len /* && byte_count_ < byte_size */ ) { // min 2 chars left
                if( ':' == str.charAt(j) ) {
                    ++j;
                } else {
                    b_[len_] = Integer.valueOf(str.substring(j, j+2), 16).byteValue(); // b_: high->low
                    j += 2;
                    ++len_;
                }
            }
            length = len_;
            b = new byte[ length ];
            for(j=0; j<length; ++j) { // swap low->high
                b[j] = b_[length-1-j];
            }
        } catch (final NumberFormatException e) {
            throw new IllegalArgumentException("EUI48 sub-string not in format '01:02:03:0A:0B:0C' but "+str, e);
        }
    }

    /** Construct instance via given source byte array */
    public EUI48Sub(final byte stream[], final int pos, final int len_) {
        if( len_ > EUI48.byte_size || pos + len_ > stream.length ) {
            throw new IllegalArgumentException("EUI48 stream ( pos "+pos+", len "+len_+" > EUI48 size "+EUI48.byte_size+" or stream.length "+stream.length);
        }
        b = new byte[len_];
        System.arraycopy(stream, pos, b, 0,  len_);
        length = len_;
    }

    /**
     * {@inheritDoc}
     * <p>
     * Returns the EUI48 sub-string representation,
     * less or equal 17 characters representing less or equal 6 bytes as upper case hexadecimal numbers separated via colon,
     * e.g. {@code "01:02:03:0A:0B:0C"}, {@code "01:02:03:0A"}, {@code ""}.
     * </p>
     */
    @Override
    public final String toString() {
        final StringBuilder sb = new StringBuilder(17);
        for(int i=length-1; 0 <= i; i--) {
            BTUtils.byteHexString(sb, b[i], false /* lowerCase */);
            if( 0 < i ) {
                sb.append(":");
            }
        }
        return sb.toString();
    }
}
