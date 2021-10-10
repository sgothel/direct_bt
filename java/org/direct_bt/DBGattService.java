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
 * Representing a Gatt Service object from the ::GATTRole::Server perspective.
 *
 * BT Core Spec v5.2: Vol 3, Part G GATT: 3.1 Service Definition
 *
 * Includes a complete [Primary] Service Declaration
 * including its list of Characteristic Declarations,
 * which also may include its client config if available.
 *
 * @since 2.4.0
 */
public class DBGattService
{
    /**
     * Indicate whether this service is a primary service.
     */
    public native boolean isPrimary();

    /**
     * Service start handle
     * <p>
     * Attribute handles are unique for each device (server) (BT Core Spec v5.2: Vol 3, Part F Protocol..: 3.2.2 Attribute Handle).
     * </p>
     */
    public native short getHandle();

    /**
     * Service end handle, inclusive.
     * <p>
     * Attribute handles are unique for each device (server) (BT Core Spec v5.2: Vol 3, Part F Protocol..: 3.2.2 Attribute Handle).
     * </p>
     */
    public native short getEndHandle();

    /** Service type UUID */
    public native String getUUID();

    /** List of Characteristic Declarations. */
    public native List<DBGattChar> getChars();

    public DBGattChar findGattChar(final String char_uuid) {
        for(final DBGattChar c : getChars()) {
            if( char_uuid.equals(c.getValueUUID()) ) {
                return c;
            }
        }
        return null;
    }
}
