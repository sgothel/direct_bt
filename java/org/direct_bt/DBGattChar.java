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

import java.util.List;

/**
 * Representing a Gatt Characteristic object from the GATT server perspective.
 *
 * BT Core Spec v5.2: Vol 3, Part G GATT: 3.3 Characteristic Definition
 *
 * handle -> CDAV value
 *
 * BT Core Spec v5.2: Vol 3, Part G GATT: 4.6.1 Discover All Characteristics of a Service
 *
 * The handle represents a service's characteristics-declaration
 * and the value the Characteristics Property, Characteristics Value Handle _and_ Characteristics UUID.
 */
public class DBGattChar
{
    /**
     * Characteristic Handle of this instance.
     * <p>
     * Attribute handles are unique for each device (server) (BT Core Spec v5.2: Vol 3, Part F Protocol..: 3.2.2 Attribute Handle).
     * </p>
     */
    public native short getHandle();

    /**
     * Characteristics Value Handle.
     * <p>
     * Attribute handles are unique for each device (server) (BT Core Spec v5.2: Vol 3, Part F Protocol..: 3.2.2 Attribute Handle).
     * </p>
     */
    public native short getValueHandle();

    /* Characteristics Value Type UUID */
    public native String getValueUUID();

    /**
     * Returns the properties of this characteristic.
     * <p>
     * BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.1.1 Characteristic Properties
     * </p>
     */
    public native GattCharPropertySet getProperties();

    /** List of Characteristic Descriptions. */
    public native List<DBGattDesc> getDescriptors();

    /* Characteristics's Value */
    public native byte[] getValue();

    @Override
    public native String toString();
}
