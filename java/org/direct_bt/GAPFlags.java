/**
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2022 Gothel Software e.K.
 * Copyright (c) 2022 ZAFENA AB
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
 * @since 2.5.3
 *
 * @see {@link EInfoReport}
 */
public class GAPFlags {

    /**
     * Each enum represents a 'Extended Inquiry Response' (EIR) data field type bit.
     *
     * @since 2.5.3
     *
     * @see {@link GAPFlags}
     * @see {@link EInfoReport}
     */
    public enum Bit {
        NONE          ((byte)0),
        LE_Ltd_Disc   ((byte)(1 << 0)),
        LE_Gen_Disc   ((byte)(1 << 1)),
        BREDR_UNSUP   ((byte)(1 << 2)),
        Dual_SameCtrl ((byte)(1 << 3)),
        Dual_SameHost ((byte)(1 << 4)),
        RESERVED1     ((byte)(1 << 5)),
        RESERVED2     ((byte)(1 << 6)),
        RESERVED3     ((byte)(1 << 7));

        Bit(final byte v) { value = v; }
        public final byte value;
    }

    public byte mask;

    public GAPFlags(final byte v) {
        mask = v;
    }

    public boolean isSet(final Bit bit) { return 0 != ( mask & bit.value ); }
    public void set(final Bit bit) { mask = (byte)(mask | bit.value); }

    @Override
    public String toString() {
        int count = 0;
        final StringBuilder out = new StringBuilder();
        for (final Bit dt : Bit.values()) {
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
