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
 * Bluetooth address type constants.
 * <p>
 * See {@link #get(String)} for its string mapping.
 * </p>
 * <p>
 * See {@link #get(byte)} for its native byte mapping.
 * </p>
 * <p>
 * BT Core Spec v5.2:  Vol 3, Part C Generic Access Profile (GAP): 15.1.1.1 Public Bluetooth address
 * </p>
 * <pre>
 * 1) BT public address used as BD_ADDR for BR/EDR physical channel is defined in Vol 2, Part B 1.2
 * - EUI-48 or MAC (6 octets)
 *
 * 2) BT public address used as BD_ADDR for the LE physical channel is defined in Vol 6, Part B 1.3
 * </pre>
 * <p>
 * BT Core Spec v5.2:  Vol 3, Part C Generic Access Profile (GAP): 15.1.1.2 Random Bluetooth address
 * </p>
 * <pre>
 * 3) BT random address used as BD_ADDR on the LE physical channel is defined in Vol 3, Part C 10.8
 * </pre>
 *
 * @since 2.0.0
 */
public enum BDAddressType {
    /** Bluetooth BREDR address */
    BDADDR_BREDR      ((byte)0x00),
    /** Bluetooth LE public address */
    BDADDR_LE_PUBLIC  ((byte)0x01),
    /** Bluetooth LE random address, see {@link BLERandomAddressType} */
    BDADDR_LE_RANDOM  ((byte)0x02),
    /** Undefined */
    BDADDR_UNDEFINED  ((byte)0xff);

    public final byte value;

    private static String _bredr = "";
    private static String _public = "public";
    private static String _random = "random";
    private static String _undef = "undefined";

    public String toDbusString() {
        if( this == BDADDR_BREDR ) {
            return _bredr;
        }
        if( this == BDADDR_LE_PUBLIC ) {
            return _public;
        }
        if( this == BDADDR_LE_RANDOM ) {
            return _random;
        }
        return _undef;
    }

    /**
     * Maps the specified name to a constant of {@link BDAddressType}.
     * <p>
     * According to BlueZ's D-Bus protocol,
     * the following mappings are valid:
     * <ul>
     *   <li>"{@code public}" -> {@link #BDADDR_LE_PUBLIC}</li>
     *   <li>"{@code random}" -> {@link #BDADDR_LE_RANDOM}</li>
     *   <li>{@code null or empty} -> {@link #BDADDR_BREDR}</li>
     * </ul>
     * </p>
     * <p>
     * If the above mappings are not resolving,
     * {@link #valueOf(String)} is being returned.
     * This maps the constant names itself to their respective constant.
     * </p>
     * @param name the string name to be mapped to a constant of this enum type.
     * @return the corresponding constant of this enum type.
     * @throws IllegalArgumentException if the specified name can't be mapped to a constant of this enum type
     *                                  as described above.
     */
    public static BDAddressType get(final String name) throws IllegalArgumentException {
        if( null == name || name.length() == 0 ) {
            return BDADDR_BREDR;
        }
        if( _public.equals(name) ) {
            return BDADDR_LE_PUBLIC;
        }
        if( _random.equals(name) ) {
            return BDADDR_LE_RANDOM;
        }
        return valueOf(name);
    }

    /**
     * Maps the specified byte value to a constant of {@link BDAddressType}.
     * @param value the byte value to be mapped to a constant of this enum type.
     * @return the corresponding constant of this enum type, using {@link #BDADDR_UNDEFINED} if not supported.
     */
    public static BDAddressType get(final byte value) {
        switch(value) {
            case (byte)0x00: return BDADDR_BREDR;
            case (byte)0x01: return BDADDR_LE_PUBLIC;
            case (byte)0x02: return BDADDR_LE_RANDOM;
            default: return BDADDR_UNDEFINED;
        }
    }

    BDAddressType(final byte v) {
        value = v;
    }
}
