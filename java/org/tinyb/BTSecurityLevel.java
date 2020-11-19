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
 * Bluetooth Security Level.
 * <p>
 * This BTSecurityLevel is natively compatible
 * with Linux/BlueZ's BT_SECURITY values 1-4.
 * </p>
 * <p>
 * See {@link #get(byte)} for its native integer mapping.
 * </p>
 * @since 2.1.0
 */
public enum BTSecurityLevel {
    /** Security Level not set. */
    UNSET         ((byte)0),
    /** No encryption and no authentication. Also known as BT_SECURITY_LOW 1. */
    NONE          ((byte)1),
    /** Encryption and no authentication (no MITM). Also known as BT_SECURITY_MEDIUM 2. */
    ENC_ONLY      ((byte)2),
    /** Encryption and authentication (MITM). Also known as BT_SECURITY_HIGH 3. */
    ENC_AUTH      ((byte)3),
    /** Authenticated Secure Connections. Also known as BT_SECURITY_FIPS 4. */
    ENC_AUTH_FIPS ((byte)4);

    public final byte value;

    /**
     * Maps the specified name to a constant of BTSecurityLevel.
     * <p>
     * Implementation simply returns {@link #valueOf(String)}.
     * This maps the constant names itself to their respective constant.
     * </p>
     * @param name the string name to be mapped to a constant of this enum type.
     * @return the corresponding constant of this enum type.
     * @throws IllegalArgumentException if the specified name can't be mapped to a constant of this enum type
     *                                  as described above.
     */
    public static BTSecurityLevel get(final String name) throws IllegalArgumentException {
        return valueOf(name);
    }

    /**
     * Maps the specified integer value to a constant of {@link BTSecurityLevel}.
     * @param value the integer value to be mapped to a constant of this enum type.
     * @return the corresponding constant of this enum type, using {@link #UNSET} if not supported.
     */
    public static BTSecurityLevel get(final byte value) {
        switch(value) {
            case (byte)0x01: return NONE;
            case (byte)0x02: return ENC_ONLY;
            case (byte)0x03: return ENC_AUTH;
            case (byte)0x04: return ENC_AUTH_FIPS;
            default: return UNSET;
        }
    }

    BTSecurityLevel(final byte v) {
        value = v;
    }
}
