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

/**
 * SMP Key Type for Distribution, indicates keys distributed in the Transport Specific Key Distribution phase.
 * <p>
 * {@link SMPKeyMask} {@link SMPKeyMask.KeyType} Bit Mask
 * </p>
 * <pre>
 * Field format and usage: Vol 3, Part H, 3.6.1 SMP - LE Security - Key distribution and generation.
 * See also Vol 3, Part H, 2.4.3 SM - LE Security - Distribution of keys.
 * </pre>
 * </p>
 * Layout LSB -> MSB
 * <pre>
 * uint8_t EncKey : 1, IdKey : 1, SignKey : 1, LinkKey : 1, RFU : 4;
 * </pre>
 * @since 2.2.0
 */
public class SMPKeyMask {
    /**
     * {@link SMPKeyMask} Key Type
     */
    static public enum KeyType {
        NONE        ((byte)0),
        /**
         * LE legacy pairing: Indicates device shall distribute LTK using the Encryption Information command,
         * followed by EDIV and Rand using the Master Identification command.
         * <p>
         * LE Secure Connections pairing (SMP on LE transport): Ignored,
         * EDIV and Rand shall be zero and shall not be distributed.
         * </p>
         * <p>
         * SMP on BR/EDR transport: Indicates device likes to derive LTK from BR/EDR Link Key.<br>
         * When EncKey is set to 1 by both devices in the initiator and responder Key Distribution / Generation fields,
         * the procedures for calculating the LTK from the BR/EDR Link Key shall be used.
         * </p>
         */
        ENC_KEY     ((byte) 0b00000001),
        /**
         * Indicates that the device shall distribute IRK using the Identity Information command
         * followed by its public device or status random address using Identity Address Information.
         */
        ID_KEY      ((byte) 0b00000010),
        /**
         * Indicates that the device shall distribute CSRK using the Signing Information command.
         */
        SIGN_KEY    ((byte) 0b00000100),
        /**
         * SMP on the LE transport: Indicate that the device would like to derive the Link Key from the LTK.<br>
         * When LinkKey is set to 1 by both devices in the initiator and responder Key Distribution / Generation fields,
         * the procedures for calculating the BR/EDR link key from the LTK shall be used.<br>
         * Devices not supporting LE Secure Connections shall set this bit to zero and ignore it on reception.
         * <p>
         * SMP on BR/EDR transport: Reserved for future use.
         * </p>
         */
        LINK_KEY    ((byte) 0b00001000),
        /** Reserved for future use */
        RFU_1       ((byte) 0b00010000),
        /** Reserved for future use */
        RFU_2       ((byte) 0b00100000),
        /** Reserved for future use */
        RFU_3       ((byte) 0b01000000),
        /** Reserved for future use */
        RFU_4       ((byte) 0b10000000);

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
                case (byte) 0b00000001: return ENC_KEY;
                case (byte) 0b00000010: return ID_KEY;
                case (byte) 0b00000100: return SIGN_KEY;
                case (byte) 0b00001000: return LINK_KEY;
                case (byte) 0b00010000: return RFU_1;
                case (byte) 0b00100000: return RFU_2;
                case (byte) 0b01000000: return RFU_3;
                case (byte) 0b10000000: return RFU_4;
                default: return NONE;
            }
        }

        KeyType(final byte v) {
            value = v;
        }
    }

    /** The {@link KeyType} bit mask */
    public byte mask;

    public SMPKeyMask(final byte v) {
        mask = v;
    }

    public SMPKeyMask() {
        mask = (byte)0;
    }

    public boolean isEmpty() { return 0 == mask; }
    public boolean isSet(final KeyType bit) { return 0 != ( mask & bit.value ); }
    public void set(final KeyType bit) { mask = (byte) ( mask | bit.value ); }

    @Override
    public String toString() {
        boolean has_pre = false;
        final StringBuilder out = new StringBuilder();
        out.append("[");
        for(int i=0; i<8; i++) {
            final KeyType key_type = KeyType.get( (byte) ( 1 << i ) );
            if( isSet( key_type ) ) {
                if( has_pre ) { out.append(", "); }
                out.append( key_type.toString() );
                has_pre = true;
            }
        }
        out.append("]");
        return out.toString();
    }

};
