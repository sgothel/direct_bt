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

import java.util.ArrayList;
import java.util.List;

/**
 * Representing a complete list of Gatt Service objects from the GATT server perspective,
 * i.e. the Gatt Server database.
 *
 * One instance shall be attached to BTAdapter
 * when operating in Gatt Server mode.
 *
 * @since 2.4.0
 */
public class DBGattServer
{
    /** List of Services. */
    public List<DBGattService> services;

    public DBGattServer(final List<DBGattService> services_) {
        services = services_;
    }

    public DBGattService findGattService(final String service_uuid) {
        for(final DBGattService s : services) {
            if( service_uuid.equals(s.getUUID()) ) {
                return s;
            }
        }
        return null;
    }

    public DBGattChar findGattChar(final String service_uuid, final String char_uuid) {
        final DBGattService service = findGattService(service_uuid);
        if( null == service ) {
            return null;
        }
        return service.findGattChar(char_uuid);
    }

}
