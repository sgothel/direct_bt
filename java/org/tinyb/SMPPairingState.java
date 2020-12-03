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
 * SMP Pairing Process state definition
 * <pre>
 * Vol 3, Part H (SM): APPENDIX C MESSAGE SEQUENCE CHARTS
 * </pre>
 * <p>
 * See {@link #get(byte)} for its native integer mapping.
 * </p>
 * @see PairingMode
 * @since 2.1.0
 */
public enum SMPPairingState {
    /** No pairing in process. Current {@link PairingMode} shall be {@link PairingMode#NONE}. */
    NONE((byte)0),

    /** Pairing failed. Current {@link PairingMode} shall be {@link PairingMode#NONE}. */
    FAILED((byte)1),

    /**
     * Phase 0: Pairing requested by responding (slave)  device via SMPSecurityReqMsg.<br>
     * Signals initiating (host) device to start the Pairing Feature Exchange.<br>
     * Current {@link PairingMode} shall be {@link PairingMode#NEGOTIATING}.
     */
    REQUESTED_BY_RESPONDER((byte)2),

    /**
     * Phase 1: Pairing requested by initiating (master) device via SMPPairingMsg.<br>
     * Starts the Pairing Feature Exchange.<br>
     * Current {@link PairingMode} shall be {@link PairingMode#NEGOTIATING}.
     */
    FEATURE_EXCHANGE_STARTED((byte)3),

    /**
     * Phase 1: Pairing responded by responding (slave)  device via SMPPairingMsg.<br>
     * Completes the Pairing Feature Exchange. Optional user input shall be given for Phase 2.<br>
     * Current {@link PairingMode} shall be set to a definitive value.
     */
    FEATURE_EXCHANGE_COMPLETED((byte)4),

    /** Phase 2: Authentication (MITM) PASSKEY expected, see {@link PairingMode#PASSKEY_ENTRY_ini} */
    PASSKEY_EXPECTED((byte)5),
    /** Phase 2: Authentication (MITM) Numeric Comparison Reply expected, see {@link PairingMode#NUMERIC_COMPARE_ini} */
    NUMERIC_COMPARE_EXPECTED((byte)6),
    /** Phase 2: Authentication (MITM) OOB data expected, see {@link PairingMode#OUT_OF_BAND} */
    OOB_EXPECTED((byte)7),

    /** Phase 3: Key & value distribution started after SMPPairConfirmMsg or SMPPairPubKeyMsg (LE Secure Connection) exchange between initiating (master) and responding (slave) device. */
    KEY_DISTRIBUTION((byte)8),

    /**
     * Phase 3: Key & value distribution completed by responding (slave) device sending SMPIdentInfoMsg (#1) , SMPIdentAddrInfoMsg (#2) or SMPSignInfoMsg (#3),<br>
     * depending on the key distribution field SMPKeyDistFormat SMPPairingMsg::getInitKeyDist() and SMPPairingMsg::getRespKeyDist()
     * The link is assumed to be encrypted from here on.
     */
    COMPLETED((byte)9);

    public final byte value;

    /**
     * Maps the specified name to a constant of {@link SMPPairingState}.
     * <p>
     * Implementation simply returns {@link #valueOf(String)}.
     * This maps the constant names itself to their respective constant.
     * </p>
     * @param name the string name to be mapped to a constant of this enum type.
     * @return the corresponding constant of this enum type.
     * @throws IllegalArgumentException if the specified name can't be mapped to a constant of this enum type
     *                                  as described above.
     */
    public static SMPPairingState get(final String name) throws IllegalArgumentException {
        return valueOf(name);
    }

    /**
     * Maps the specified integer value to a constant of {@link SMPPairingState}.
     * @param value the integer value to be mapped to a constant of this enum type.
     * @return the corresponding constant of this enum type, using {@link #NONE} if not supported.
     */
    public static SMPPairingState get(final byte value) {
        switch(value) {
            case (byte) 0x01: return FAILED;
            case (byte) 0x02: return REQUESTED_BY_RESPONDER;
            case (byte) 0x03: return FEATURE_EXCHANGE_STARTED;
            case (byte) 0x04: return FEATURE_EXCHANGE_COMPLETED;
            case (byte) 0x05: return PASSKEY_EXPECTED;
            case (byte) 0x06: return NUMERIC_COMPARE_EXPECTED;
            case (byte) 0x07: return OOB_EXPECTED;
            case (byte) 0x08: return KEY_DISTRIBUTION;
            case (byte) 0x09: return COMPLETED;
            default: return NONE;
        }
    }

    SMPPairingState(final byte v) {
        value = v;
    }
}
