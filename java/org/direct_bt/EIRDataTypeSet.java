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
 * Bit mask of 'Extended Inquiry Response' (EIR) data fields,
 * indicating a set of related data.
 *
 * @since 2.0.0
 *
 * @see {@link EInfoReport}
 */
public class EIRDataTypeSet {

    /**
     * Each enum represents a 'Extended Inquiry Response' (EIR) data field type bit.
     *
     * @since 2.0.0
     *
     * @see {@link EIRDataTypeSet}
     * @see {@link EInfoReport}
     */
    public enum DataType {
        NONE         (     0),
        EVT_TYPE     (1 << 0),
        EXT_EVT_TYPE (1 << 1),
        BDADDR_TYPE  (1 << 2),
        BDADDR       (1 << 3),
        FLAGS        (1 << 4),
        NAME         (1 << 5),
        NAME_SHORT   (1 << 6),
        RSSI         (1 << 7),
        TX_POWER     (1 << 8),
        MANUF_DATA   (1 << 9),
        DEVICE_CLASS (1 << 10),
        APPEARANCE   (1 << 11),
        HASH         (1 << 12),
        RANDOMIZER   (1 << 13),
        DEVICE_ID    (1 << 14),
        CONN_IVAL    (1 << 15),
        SERVICE_UUID (1 << 30);

        DataType(final int v) { value = v; }
        public final int value;
    }

    public int mask;

    public EIRDataTypeSet(final int v) {
        mask = v;
    }
    public EIRDataTypeSet() {
        mask = 0;
    }

    public boolean isSet(final DataType bit) { return 0 != ( mask & bit.value ); }
    public void set(final DataType bit) { mask = mask | bit.value; }

    @Override
    public String toString() {
        int count = 0;
        final StringBuilder out = new StringBuilder();
        for (final DataType dt : DataType.values()) {
            if( isSet(dt) ) {
                if( 0 < count ) { out.append(", "); }
                out.append(dt.name()); count++;
            }
        }
        if( 1 < count ) {
            out.insert(0, "[");
            out.append("]");
        }
        return out.toString();
    }
}
