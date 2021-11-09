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
 * Bit mask of GATT Characteristic Properties
 * <pre>
 * BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.1.1 Characteristic Properties
 * </pre>
 *
 * @since 2.3
 */
public class GattCharPropertySet {

    /**
     * BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.1.1 Characteristic Properties
     *
     * @since 2.3
     */
    public enum Type {
        NONE            (     0),
        Broadcast       (1 << 0),
        Read            (1 << 1),
        WriteNoAck      (1 << 2),
        WriteWithAck    (1 << 3),
        Notify          (1 << 4),
        Indicate        (1 << 5),
        AuthSignedWrite (1 << 6),
        ExtProps        (1 << 7);

        Type(final int v) { value = (byte)v; }
        public final byte value;
    }

    public byte mask;

    public GattCharPropertySet(final byte v) {
        mask = v;
    }
    public GattCharPropertySet(final Type bit) {
        mask = bit.value;
    }

    public boolean isSet(final Type bit) { return 0 != ( mask & bit.value ); }
    public GattCharPropertySet set(final Type bit) { mask = (byte) ( mask | bit.value ); return this; }

    @Override
    public String toString() {
        int count = 0;
        final StringBuilder out = new StringBuilder();
        if( isSet(Type.Broadcast) ) {
            out.append(Type.Broadcast.name()); count++;
        }
        if( isSet(Type.Read) ) {
            out.append(Type.Read.name()); count++;
        }
        if( isSet(Type.WriteNoAck) ) {
            if( 0 < count ) { out.append(", "); }
            out.append(Type.WriteNoAck.name()); count++;
        }
        if( isSet(Type.WriteWithAck) ) {
            if( 0 < count ) { out.append(", "); }
            out.append(Type.WriteWithAck.name()); count++;
        }
        if( isSet(Type.Notify) ) {
            if( 0 < count ) { out.append(", "); }
            out.append(Type.Notify.name()); count++;
        }
        if( isSet(Type.Indicate) ) {
            if( 0 < count ) { out.append(", "); }
            out.append(Type.Indicate.name()); count++;
        }
        if( isSet(Type.AuthSignedWrite) ) {
            if( 0 < count ) { out.append(", "); }
            out.append(Type.AuthSignedWrite.name()); count++;
        }
        if( isSet(Type.ExtProps) ) {
            if( 0 < count ) { out.append(", "); }
            out.append(Type.ExtProps.name()); count++;
        }
        if( 1 < count ) {
            out.insert(0, "[");
            out.append("]");
        }
        return out.toString();
    }
}
