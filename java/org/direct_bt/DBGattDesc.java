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
public class DBGattDesc
{
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
    public short handle;

    /** Type of descriptor UUID (lower-case) */
    public String type;

    /**
     * Characteristic Descriptor's Value.
     *
     * Its capacity defines the maximum writable variable length
     * and its size defines the maximum writable fixed length.
     *
     * FIXME: Needs capacity and length (or size)
     */
    public byte[] value;

    /**
     * True if value is of variable length, otherwise fixed length.
     */
    public boolean variable_length;

    private void setup(final String type_,
                       final byte[] value_, final boolean variable_length_)
    {
        handle = 0;
        type = type_;
        value = value_;
        variable_length = variable_length_;

        if( variable_length && ( isExtendedProperties() || isClientCharConfig() ) ) {
            variable_length = false;
        }
    }

    /**
     *
     * @param type_
     * @param value_
     * @param variable_length_ defaults to true, but forced to false if isExtendedProperties() or isClientCharConfig().
     */
    public DBGattDesc(final String type_, final byte[] value_, final boolean variable_length_)
    {
        setup(type_, value_, variable_length_);
    }

    /**
     *
     * @param type_
     * @param value_
     * @param variable_length_ defaults to true, but forced to false if isExtendedProperties() or isClientCharConfig().
     */
    public DBGattDesc(final String type_, final byte[] value_)
    {
        setup(type_, value_, true /* variable_length_ */);
    }

    /** Fill value with zero bytes. */
    public void bzero() {
        for(int i=0; i<value.length; i++) { // anything more efficient?
            value[i]=0;
        }
    }

    /**
     * Return a newly constructed Client Characteristic Configuration
     * with a zero uint16_t value of fixed length.
     * @see isClientCharConfig()
     */
    public static DBGattDesc createClientCharConfig() {
        final byte[] p = { (byte)0, (byte)0 };
        return new DBGattDesc( UUID16.CCC_DESC, p, false /* variable_length */ );
    }

    @Override
    public String toString() {
        final String len = variable_length ? "var" : "fixed";
        return "Desc[type 0x"+type+", handle 0x"+Integer.toHexString(handle)+
               ", value[len "+len+", "+BTUtils.bytesHexString(value, 0, value.length, true /* lsbFirst */)+"]]";
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
        return handle == o.handle; /** unique attribute handles */
    }
}
