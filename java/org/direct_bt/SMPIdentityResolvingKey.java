/**
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2021 Gothel Software e.K.
 * Copyright (c) 2021 ZAFENA AB
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

import org.jau.net.EUI48;
import org.jau.util.BasicTypes;

/**
 * SMP Identity Resolving Key, used for platform agnostic persistence.
 * <p>
 * Notable: No endian wise conversion shall occur on this data,
 *          since the encryption values are interpreted as a byte stream.
 * </p>
 * <p>
 * Byte layout must be synchronized with native direct_bt::SMPIdentityResolvingKeyInfo
 * </p>
 * @since 2.4.0
 */
public final class SMPIdentityResolvingKey {
    /**
     * {@link SMPIdentityResolvingKey} Property Bits
     */
    static public enum PropertyType {
        /** No specific property */
        NONE((byte)0),
        /** Responder Key (LL slave). Absence indicates Initiator Key (LL master). */
        RESPONDER((byte)0x01),
        /** Authentication used. */
        AUTH((byte)0x02);

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
                default: return NONE;
            }
        }

        PropertyType(final byte v) {
            value = v;
        }
    }

    /**
     * {@link SMPIdentityResolvingKey} {@link PropertyType} Bit Mask
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
            return "["+out.toString()+"]";
        }
    }

    /** {@link Properties} bit mask. 1 octet or 8 bits. */
    public Properties properties;

    /** Identity Resolving Key (IRK) */
    public byte irk[/*16*/];

    /** Identity Address for the IRK */
    public EUI48 id_address;

    /**
     * Size of the byte stream representation in bytes
     * @see #put(byte[], int)
     */
    public static final int byte_size = 1+16+6;

    /** Construct instance via given source byte array */
    public SMPIdentityResolvingKey(final byte source[], final int pos) {
        irk = new byte[16];
        get(source, pos);
    }

    /** Construct emoty unset instance. */
    public SMPIdentityResolvingKey() {
        properties = new Properties((byte)0);
        irk        = new byte[16];
        id_address = new EUI48();
    }

    /**
     * Method transfers all bytes representing a SMPIdentityResolvingKeyInfo from the given
     * source array at the given position into this instance.
     * <p>
     * Implementation is consistent with {@link #put(byte[], int)}.
     * </p>
     * @param source the source array
     * @param pos starting position in the source array
     * @see #put(byte[], int)
     */
    public void get(final byte[] source, int pos) {
        if( byte_size > ( source.length - pos ) ) {
            throw new IllegalArgumentException("Stream ( "+source.length+" - "+pos+" ) < "+byte_size+" bytes");
        }
        properties = new Properties(source[pos++]);
        System.arraycopy(source, pos, irk, 0, 16); pos+=16;
        id_address = new EUI48(source, pos, ByteOrder.nativeOrder()); pos+=6;
    }

    /**
     * Method transfers all bytes representing this instance into the given
     * destination array at the given position.
     * <p>
     * Implementation is consistent with {@link #SMPIdentityResolvingKeyInfo(byte[], int)}.
     * </p>
     * @param sink the destination array
     * @param pos starting position in the destination array
     * @see #SMPIdentityResolvingKeyInfo(byte[], int)
     * @see #get(byte[], int)
     */
    public final void put(final byte[] sink, int pos) {
        if( byte_size > ( sink.length - pos ) ) {
            throw new IllegalArgumentException("Stream ( "+sink.length+" - "+pos+" ) < "+byte_size+" bytes");
        }
        sink[pos++] = properties.mask;
        System.arraycopy(irk,  0, sink, pos, 16); pos+=16;
        System.arraycopy(id_address.b, 0, sink, pos, 6); pos+=6;
    }

    public final boolean isResponder() { return properties.isSet(PropertyType.RESPONDER); }

    /** Returns true if this IRK matches the given random private address (RPA). */
    public native boolean matches(final EUI48 rpa);

    @Override
    public String toString() { // hex-fmt aligned with btmon
        return "IRK[props "+properties.toString()+
               ", id "+id_address+", irk "+BasicTypes.bytesHexString(irk, 0, -1, true /* lsbFirst */)+
               "]";
    }

};
