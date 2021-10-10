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
 * Representing a Gatt Characteristic Descriptor object from the GATT server perspective.
 *
 * BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3 Characteristic Descriptor
 *
 * @since 2.4.0
 */
public class DBGattDesc
{
    /**
     * Characteristic Descriptor Handle
     * <p>
     * Attribute handles are unique for each device (server) (BT Core Spec v5.2: Vol 3, Part F Protocol..: 3.2.2 Attribute Handle).
     * </p>
     */
    public native short getHandle();

    /** Type of descriptor */
    public native String getUUID();

    /* Characteristics Descriptor's Value */
    public native byte[] getValue();

    @Override
    public native String toString();

    /** Value is uint16_t bitfield */
    // public boolean isExtendedProperties() { return BTGattDesc::TYPE_EXT_PROP == getUUID(); }

    /* BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.3 Client Characteristic Configuration (Characteristic Descriptor, optional, single, uint16_t bitfield) */
    // public boolean isClientCharConfig() { return BTGattDesc::TYPE_CCC_DESC == getUUID(); }

}
