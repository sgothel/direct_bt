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

/**
 * LE Transport PHY bit values (bitmask)
 * <pre>
 * BT Core Spec v5.2: Vol 4, Part E, 7.8.47 LE Read PHY command (we transfer the sequential value to this bitmask for unification)
 * BT Core Spec v5.2: Vol 4, Part E, 7.8.48 LE Set Default PHY command
 * </pre>
 *
 * @since 2.4.0
 */
public class LE_PHYs {

    /**
     * Each enum represents a 'LE Transport PHY' bit value.
     *
     * @since 2.4.0
     */
    public enum PHY {
        LE_1M(0),
        LE_2M(1),
        LE_CODED(2);

        PHY(final int v) {
            value = (byte)( 1 << v );
        }
        public final byte value;
    }

    public byte mask;

    public LE_PHYs() {
        mask = 0;
    }
    public LE_PHYs(final byte v) {
        mask = v;
    }
    public LE_PHYs(final PHY v) {
        mask = v.value;
    }

    public boolean isSet(final PHY bit) { return 0 != ( mask & bit.value ); }
    public void set(final PHY bit) { mask = (byte) ( mask | bit.value ); }

    @Override
    public String toString() {
        int count = 0;
        final StringBuilder out = new StringBuilder();
        for (final PHY f : PHY.values()) {
            if( isSet(f) ) {
                if( 0 < count ) { out.append(", "); }
                out.append(f.name()); count++;
            }
        }
        if( 1 < count ) {
            out.insert(0, "[");
            out.append("]");
        }
        return out.toString();
    }
}
