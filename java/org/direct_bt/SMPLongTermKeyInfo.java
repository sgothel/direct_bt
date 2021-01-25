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

/**
 * SMP Long Term Key Info, used for platform agnostic persistence.
 * <p>
 * Notable: No endian wise conversion shall occur on this data,
 *          since the encryption values are interpreted as a byte stream.
 * </p>
 * <p>
 * Byte layout must be synchronized with native direct_bt::SMPLongTermKeyInfo
 * </p>
 * @since 2.2.0
 */
public class SMPLongTermKeyInfo {
    /**
     * {@link SMPLongTermKeyInfo} Property Bits
     */
    static public enum PropertyType {
        /** No specific property */
        NONE((byte)0),
        /** Responder Key (LL slave). Absence indicates Initiator Key (LL master). */
        RESPONDER((byte)0x01),
        /** Authentication used. */
        AUTH((byte)0x02),
        /** Secure Connection used. */
        SC((byte)0x04);

        public final byte value;

        /**
         * Maps the specified name to a constant of {@link PropertyType}.
         * <p>
         * Implementation simply returns {@link #valueOf(String)}.
         * This maps the constant names itself to their respective constant.
         * </p>
         * @param name the string name to be mapped to a constant of this enum type.
         * @return the corresponding constant of this enum type.
         * @throws IllegalArgumentException if the specified name can't be mapped to a constant of this enum type
         *                                  as described above.
         */
        public static PropertyType get(final String name) throws IllegalArgumentException {
            return valueOf(name);
        }

        /**
         * Maps the specified integer value to a constant of {@link PropertyType}.
         * @param value the integer value to be mapped to a constant of this enum type.
         * @return the corresponding constant of this enum type, using {@link #NONE} if not supported.
         */
        public static PropertyType get(final byte value) {
            switch(value) {
                case (byte) 0x01: return RESPONDER;
                case (byte) 0x02: return AUTH;
                case (byte) 0x04: return SC;
                default: return NONE;
            }
        }

        PropertyType(final byte v) {
            value = v;
        }
    }

    /**
     * {@link SMPLongTermKeyInfo} {@link PropertyType} Bit Mask
     */
    static public class Properties {
        /** The {@link PropertyType} bit mask */
        public byte mask;

        public Properties(final byte v) {
            mask = v;
        }

        public boolean isEmpty() { return 0 == mask; }
        public boolean isSet(final PropertyType bit) { return 0 != ( mask & bit.value ); }
        public void set(final PropertyType bit) { mask = (byte) ( mask | bit.value ); }

        @Override
        public String toString() {
            int count = 0;
            final StringBuilder out = new StringBuilder();
            if( isSet(PropertyType.RESPONDER) ) {
                out.append(PropertyType.RESPONDER.name()); count++;
            }
            if( isSet(PropertyType.AUTH) ) {
                if( 0 < count ) { out.append(", "); }
                out.append(PropertyType.AUTH.name()); count++;
            }
            if( isSet(PropertyType.SC) ) {
                if( 0 < count ) { out.append(", "); }
                out.append(PropertyType.SC.name()); count++;
            }
            return "["+out.toString()+"]";
        }
    }

    /** {@link Properties} bit mask. 1 octet or 8 bits. */
    public Properties properties;

    /** Encryption Size, 1 octets or 8 bits. Is zero if key is invalid. */
    public byte enc_size;
    /** Encryption Diversifier, 2 octets or 16 bits. */
    public byte ediv[/*2*/];
    /** Random Number, 8 octets or 64 bits. */
    public byte rand[/*8*/];
    /** Long Term Key (LTK), 16 octets or 128 bits. */
    public byte ltk[/*16*/];

    /**
     * Size of the byte stream representation in bytes
     * @see #getStream(byte[], int)
     */
    public static final int byte_size = 1+1+2+8+16;

    /** Construct instance via given source byte array */
    public SMPLongTermKeyInfo(final byte source[], final int pos) {
        if( byte_size > ( source.length - pos ) ) {
            throw new IllegalArgumentException("Stream ( "+source.length+" - "+pos+" ) < "+byte_size+" bytes");
        }
        ediv       = new byte[2];
        rand       = new byte[8];
        ltk        = new byte[16];
        putStream(source, pos);
    }

    /** Construct emoty unset instance. */
    public SMPLongTermKeyInfo() {
        properties = new Properties((byte)0);
        enc_size   = (byte)0;
        ediv       = new byte[2];
        rand       = new byte[8];
        ltk        = new byte[16];
    }

    /**
     * Method transfers all bytes representing a SMPLongTermKeyInfo from the given
     * source array at the given position into this instance.
     * <p>
     * Implementation is consistent with {@link #getStream(byte[], int)}.
     * </p>
     * @param source the source array
     * @param pos starting position in the source array
     * @see #getStream(byte[], int)
     */
    public void putStream(final byte[] source, int pos) {
        if( byte_size > ( source.length - pos ) ) {
            throw new IllegalArgumentException("Stream ( "+source.length+" - "+pos+" ) < "+byte_size+" bytes");
        }
        properties = new Properties(source[pos++]);
        enc_size   = source[pos++];
        ediv[0]    = source[pos++];
        ediv[1]    = source[pos++];
        System.arraycopy(source, pos, rand, 0,  8); pos+=8;
        System.arraycopy(source, pos, ltk,  0, 16); pos+=16;
    }

    /**
     * Method transfers all bytes representing this instance into the given
     * destination array at the given position.
     * <p>
     * Implementation is consistent with {@link #SMPLongTermKeyInfo(byte[], int)}.
     * </p>
     * @param sink the destination array
     * @param pos starting position in the destination array
     * @see #SMPLongTermKeyInfo(byte[], int)
     * @see #putStream(byte[], int)
     */
    public final void getStream(final byte[] sink, int pos) {
        if( byte_size > ( sink.length - pos ) ) {
            throw new IllegalArgumentException("Stream ( "+sink.length+" - "+pos+" ) < "+byte_size+" bytes");
        }
        sink[pos++] = properties.mask;
        sink[pos++] = enc_size;
        sink[pos++] = ediv[0];
        sink[pos++] = ediv[1];
        System.arraycopy(rand, 0, sink, pos,  8); pos+=8;
        System.arraycopy(ltk,  0, sink, pos, 16); pos+=16;
    }

    public final boolean isValid() { return 0 != enc_size; }

    @Override
    public String toString() { // hex-fmt aligned with btmon
        return "LTK[props "+properties.toString()+", enc_size "+enc_size+
               ", ediv "+BluetoothUtils.bytesHexString(ediv, 0, -1, false /* lsbFirst */, true /* leading0X */, true /* lowerCase */)+
               ", rand "+BluetoothUtils.bytesHexString(rand, 0, -1, false /* lsbFirst */, true /* leading0X */, true /* lowerCase */)+
               ", ltk "+BluetoothUtils.bytesHexString(ltk, 0, -1, true /* lsbFirst */, false /* leading0X */, true /* lowerCase */)+
               "]";
    }

};
