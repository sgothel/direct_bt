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
 * Bluetooth secure pairing mode
 * <pre>
 * BT Core Spec v5.2: Vol 1, Part A, 5 Security Overview
 * BT Core Spec v5.2: Vol 1, Part A, 5.4 LE SECURITY
 * </pre>
 * <p>
 * See {@link #get(byte)} for its native integer mapping.
 * </p>
 * @see SMPPairingState
 * @since 2.1.0
 */
public enum PairingMode {
    /** No pairing mode, implying no secure connections, no encryption and no MITM protection. */
    NONE                ((byte)0),
    /** Pairing mode in negotiating, i.e. Pairing Feature Exchange in progress. */
    NEGOTIATING         ((byte)1),
    /** Just Works. Random key exchange with encryption but no MITM protection. */
    JUST_WORKS          ((byte)2),
    /** Passkey Entry input by initiator. Responder produces and display artifact. A known digit sequence (PIN) must be given as a secret to be validated on the device. Random key exchange with additional secret (PIN) and encryption and MITM protection. */
    PASSKEY_ENTRY_ini   ((byte)3),
    /** Passkey Entry input by responder. Initiator produces and display artifact. A known digit sequence (PIN) must be given as a secret to be validated on the device. Random key exchange with additional secret (PIN) and encryption and MITM protection. */
    PASSKEY_ENTRY_res   ((byte)4),
    /** Visual comparison of digit sequence (PIN) input by initiator, shown on both devices. Responder produces and display artifact. Random key exchange with additional secret (PIN) and encryption and MITM protection. */
    NUMERIC_COMPARE_ini ((byte)5),
    /** Visual comparison of digit sequence (PIN) input by responder, shown on both devices. Initiator produces and displays artifact. Random key exchange with additional secret (PIN) and encryption and MITM protection. */
    NUMERIC_COMPARE_res ((byte)6),
    /** Utilizing a second factor secret to be used as a secret, e.g. NFC field. Random key exchange with additional secret (2FA) and encryption and potential MITM protection. */
    OUT_OF_BAND         ((byte)7);

    public final byte value;

    /**
     * Maps the specified name to a constant of PairingMode.
     * <p>
     * Implementation simply returns {@link #valueOf(String)}.
     * This maps the constant names itself to their respective constant.
     * </p>
     * @param name the string name to be mapped to a constant of this enum type.
     * @return the corresponding constant of this enum type.
     * @throws IllegalArgumentException if the specified name can't be mapped to a constant of this enum type
     *                                  as described above.
     */
    public static PairingMode get(final String name) throws IllegalArgumentException {
        return valueOf(name);
    }

    /**
     * Maps the specified integer value to a constant of {@link PairingMode}.
     * @param value the integer value to be mapped to a constant of this enum type.
     * @return the corresponding constant of this enum type, using {@link #NONE} if not supported.
     */
    public static PairingMode get(final byte value) {
        switch(value) {
            case (byte) 0x01: return NEGOTIATING;
            case (byte) 0x02: return JUST_WORKS;
            case (byte) 0x03: return PASSKEY_ENTRY_ini;
            case (byte) 0x04: return PASSKEY_ENTRY_res;
            case (byte) 0x05: return NUMERIC_COMPARE_ini;
            case (byte) 0x06: return NUMERIC_COMPARE_res;
            case (byte) 0x07: return OUT_OF_BAND;
            default: return NONE;
        }
    }

    PairingMode(final byte v) {
        value = v;
    }
}
