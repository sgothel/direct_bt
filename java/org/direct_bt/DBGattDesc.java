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
 * Representing a Gatt Characteristic Descriptor object from the GATT server perspective.
 *
 * BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3 Characteristic Descriptor
 *
 * @since 2.4.0
 */
public final class DBGattDesc implements AutoCloseable
{
    private volatile long nativeInstance;
    /* pp */ long getNativeInstance() { return nativeInstance; }

    public static class UUID16 {
        /* BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.1 Characteristic Extended Properties */
        public static final String EXT_PROP  = "2900";
        /* BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.2 Characteristic User Description (Characteristic Descriptor, optional, single, string) */
        public static final String USER_DESC = "2901";
        /* BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.3 Client Characteristic Configuration (Characteristic Descriptor, optional, single, uint16_t bitfield) */
        public static final String CCC_DESC  = "2902";
    }

    /**
     * Characteristic Descriptor Handle
     * <p>
     * Attribute handles are unique for each device (server) (BT Core Spec v5.2: Vol 3, Part F Protocol..: 3.2.2 Attribute Handle).
     * </p>
     */
    public native short getHandle();

    private final String type;

    /** Type of descriptor UUID (lower-case) */
    public String getType() { return type; }

    /**
     * Return a copy of this characteristic descriptor's native {@link DBGattValue} value.
     *
     * Its capacity defines the maximum writable variable length
     * and its size defines the maximum writable fixed length.
     */
    public native DBGattValue getValue();

    /**
     * Set this characteristic descriptor's native value.
     *
     * Methods won't exceed the value's capacity if it is of hasVariableLength()
     * or the value's size otherwise.
     *
     * @param source data to be written to this value
     * @param source_len length of the source data to be written
     * @param dest_pos position where the source data shall be written to the value
     * @return true if successful, otherwise false for exceeding the value's limits or passing invalid parameter.
     */
    public native boolean setValue(final byte[] source, final int source_pos, final int source_len, final int dest_pos);

    /**
     *
     * The value's {@link DBGattValue#hasVariableLength()} is forced to false if {@link #isExtendedProperties()} or {@link #isClientCharConfig()}.
     * @param type_
     * @param value_
     */
    public DBGattDesc(final String type_, final DBGattValue value_)
    {
        type = type_;

        if( value_.hasVariableLength() && ( isExtendedProperties() || isClientCharConfig() ) ) {
            value_.setVariableLength(false);
        }
        nativeInstance = ctorImpl(type_, value_.data(), value_.capacity(), value_.hasVariableLength());
    }
    private native long ctorImpl(final String type,
                                 final byte[] value, final int capacity, boolean variable_length);

    @Override
    public void close() {
        final long handle;
        synchronized( this ) {
            handle = nativeInstance;
            nativeInstance = 0;
        }
        if( 0 != handle ) {
            dtorImpl(handle);
        }
    }
    private static native void dtorImpl(final long nativeInstance);

    @Override
    public void finalize() {
        close();
    }

    /** Fill value with zero bytes. */
    public native void bzero();

    /**
     * Return a newly constructed Client Characteristic Configuration
     * with a zero uint16_t value of fixed length.
     * @see isClientCharConfig()
     */
    public static DBGattDesc createClientCharConfig() {
        final byte[] p = { (byte)0, (byte)0 };
        return new DBGattDesc( UUID16.CCC_DESC, new DBGattValue(p, p.length, false /* variable_length */) );
    }

    /** Value is uint16_t bitfield */
    public boolean isExtendedProperties() { return UUID16.EXT_PROP.equals(type); }

    /* BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.3 Client Characteristic Configuration (Characteristic Descriptor, optional, single, uint16_t bitfield) */
    public boolean isClientCharConfig() { return UUID16.CCC_DESC.equals(type); }

    /* BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.2 Characteristic User Description */
    public boolean isUserDescription() { return UUID16.USER_DESC.equals(type); }

    @Override
    public boolean equals(final Object other) {
        if( this == other ) {
            return true;
        }
        if( !(other instanceof DBGattDesc) ) {
            return false;
        }
        final DBGattDesc o = (DBGattDesc)other;
        return getHandle() == o.getHandle(); /** unique attribute handles */
    }

    @Override
    public native String toString();
}
