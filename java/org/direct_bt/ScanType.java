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
 * Meta ScanType as derived from {@link BTMode}
 * with defined value mask consisting of {@link BDAddressType} bits.
 * <p>
 * This ScanType is natively compatible with DBTManager's implementation
 * for start and stop discovery.
 * </p>
 * @since 2.0.0
 */
public enum ScanType {
    NONE  ((byte) 0),
    /** BREDR scanning only, 1 << BluetoothAddressType.BDADDR_BREDR */
    BREDR ((byte) 1 /*  1 << BluetoothAddressType.BDADDR_BREDR.value */ ),
    /** LE scanning only, ( 1 << BluetoothAddressType.BDADDR_LE_PUBLIC ) | ( 1 << BluetoothAddressType.BDADDR_LE_RANDOM ) */
    LE    ((byte) 6 /*  ( 1 << BluetoothAddressType.BDADDR_LE_PUBLIC.value ) | ( 1 << BluetoothAddressType.BDADDR_LE_RANDOM.value ) */ ),
    /** Dual scanning, BREDR | LE */
    DUAL  ((byte) 7 /*  BREDR.value | LE.value */ );

    public final byte value;

    public boolean hasScanType(final ScanType testType) {
        return testType.value == ( value & testType.value );
    }

    public static final ScanType changeScanType(final ScanType current, final ScanType changeType, final boolean changeEnable) {
        return changeEnable ? ScanType.get( (byte)( current.value | changeType.value ) ) : ScanType.get( (byte)( current.value & ~changeType.value ) );
    }

    /**
     * Maps the specified integer value to a constant of {@link ScanType}.
     * @param value the integer value to be mapped to a constant of this enum type.
     * @return the corresponding constant of this enum type, using {@link #NONE} if not supported.
     */
    public static ScanType get(final byte value) {
        switch(value) {
            case (byte) 1: return BREDR;
            case (byte) 6: return LE;
            case (byte) 7: return DUAL;
            default: return NONE;
        }
    }

    ScanType(final byte v) {
        value = v;
    }
}
