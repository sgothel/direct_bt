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

import org.jau.util.BasicTypes;

/**
 * Local SMP Link Key, used for platform agnostic persistence,
 * mapping to platform specific link keys format.
 * <p>
 * Notable: No endian wise conversion shall occur on this data,
 *          since the encryption values are interpreted as a byte stream.
 * </p>
 * <p>
 * Byte layout must be synchronized with native direct_bt::SMPLinkKeyInfo
 * </p>
 * @since 2.4.0
 */
public final class SMPLinkKey {
    /**
     * Link Key Types compatible with Mgmt's MgmtLinkKeyType and hence MgmtLinkKeyInfo
     */
    static public enum KeyType {
        /** Combination key */
        COMBI((byte)0x00),
        /** Local Unit key */
        LOCAL_UNIT((byte)0x01),
        /** Remote Unit key */
        REMOTE_UNIT((byte)0x02),
        /** Debug Combination key */
        DBG_COMBI((byte)0x03),
        /** Unauthenticated Combination key from P-192 */
        UNAUTH_COMBI_P192((byte)0x04),
        /** Authenticated Combination key from P-192 */
        AUTH_COMBI_P192((byte)0x05),
        /** Changed Combination key */
        CHANGED_COMBI((byte)0x06),
        /** Unauthenticated Combination key from P-256 */
        UNAUTH_COMBI_P256((byte)0x07),
        /** Authenticated Combination key from P-256 */
        AUTH_COMBI_P256((byte)0x08),
        /** Denoting no or invalid link key type */
        NONE((byte)0xff);

        public final byte value;

        /**
         * Maps the specified name to a constant of {@link KeyType}.
         * <p>
         * Implementation simply returns {@link #valueOf(String)}.
         * This maps the constant names itself to their respective constant.
         * </p>
         * @param name the string name to be mapped to a constant of this enum type.
         * @return the corresponding constant of this enum type.
         * @throws IllegalArgumentException if the specified name can't be mapped to a constant of this enum type
         *                                  as described above.
         */
        public static KeyType get(final String name) throws IllegalArgumentException {
            return valueOf(name);
        }

        /**
         * Maps the specified integer value to a constant of {@link KeyType}.
         * @param value the integer value to be mapped to a constant of this enum type.
         * @return the corresponding constant of this enum type, using {@link #NONE} if not supported.
         */
        public static KeyType get(final byte value) {
            switch(value) {
                case (byte) 0x00: return COMBI;
                case (byte) 0x01: return LOCAL_UNIT;
                case (byte) 0x02: return REMOTE_UNIT;
                case (byte) 0x03: return DBG_COMBI;
                case (byte) 0x04: return UNAUTH_COMBI_P192;
                case (byte) 0x05: return AUTH_COMBI_P192;
                case (byte) 0x06: return CHANGED_COMBI;
                case (byte) 0x07: return UNAUTH_COMBI_P256;
                case (byte) 0x08: return AUTH_COMBI_P256;
                default: return NONE;
            }
        }

        KeyType(final byte v) {
            value = v;
        }
    }

    /** Responder (ll slave) flag. */
    public boolean responder;

    /** {@link KeyType} value. */
    public KeyType type;

    /** Link key. */
    public byte key[/*16*/];

    /** Pin length */
    public byte pin_length;

    /**
     * Size of the byte stream representation in bytes
     * @see #put(byte[], int)
     */
    public static final int byte_size = 1+1+16+1;

    /** Construct instance via given source byte array */
    public SMPLinkKey(final byte source[], final int pos) {
        key = new byte[16];
        get(source, pos);
    }

    /** Construct emoty unset instance. */
    public SMPLinkKey() {
        responder = false;
        type = KeyType.NONE;
        key = new byte[16];
        pin_length = 0;
    }

    /**
     * Method transfers all bytes representing a SMPLinkKeyInfo from the given
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
        responder = (byte)0 != source[pos++];
        type = KeyType.get(source[pos++]);
        System.arraycopy(source, pos, key, 0, 16); pos+=16;
        pin_length = source[pos++];
    }

    /**
     * Method transfers all bytes representing this instance into the given
     * destination array at the given position.
     * <p>
     * Implementation is consistent with {@link #SMPLinkKeyInfo(byte[], int)}.
     * </p>
     * @param sink the destination array
     * @param pos starting position in the destination array
     * @see #SMPLinkKeyInfo(byte[], int)
     * @see #get(byte[], int)
     */
    public final void put(final byte[] sink, int pos) {
        if( byte_size > ( sink.length - pos ) ) {
            throw new IllegalArgumentException("Stream ( "+sink.length+" - "+pos+" ) < "+byte_size+" bytes");
        }
        sink[pos++] = responder ? (byte)1 : (byte)0;
        sink[pos++] = type.value;
        System.arraycopy(key,  0, sink, pos, 16); pos+=16;
        sink[pos++] = pin_length;
    }

    public final boolean isValid() { return KeyType.NONE != type; }

    public final boolean isResponder() { return responder; }

    @Override
    public String toString() { // hex-fmt aligned with btmon
        return "LK[res "+responder+", type "+type.toString()+
               ", key "+BasicTypes.bytesHexString(key, 0, -1, true /* lsbFirst */)+
               ", plen "+pin_length+
               "]";
    }

};
