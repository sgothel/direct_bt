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
    /**
     * Listener to remote master device's operations on the local GATT-Server.
     *
     * All methods shall return as soon as possible to not block GATT event processing.
     */
    public static abstract class Listener {
        @SuppressWarnings("unused")
        private volatile long nativeInstance;

        /**
         * Notification that device got connected.
         *
         * Convenient user entry, allowing to setup resources.
         *
         * @param device the connected device
         * @param initialMTU initial used minimum MTU until negotiated.
         */
        abstract void connected(BTDevice device, final int initialMTU);

        /**
         * Notification that device got disconnected.
         *
         * Convenient user entry, allowing to clean up resources.
         *
         * @param device the disconnected device.
         */
        abstract void disconnected(BTDevice device);

        /**
         * Notification that the MTU has changed.
         *
         * @param device the device for which the MTU has changed
         * @param mtu the new negotiated MTU
         */
        abstract void mtuChanged(BTDevice device, final int mtu);

        /**
         *
         * @param device
         * @param s
         * @param c
         * @return true if master read has been accepted by GATT-Server listener, otherwise false. Only if all listener return true, the read action will be allowed.
         */
        abstract boolean readCharValue(BTDevice device, DBGattService s, DBGattChar c);

        /**
         *
         * @param device
         * @param s
         * @param c
         * @param d
         * @return true if master read has been accepted by GATT-Server listener, otherwise false. Only if all listener return true, the read action will be allowed.
         */
        abstract boolean readDescValue(BTDevice device, DBGattService s, DBGattChar c, DBGattDesc d);

        /**
         *
         * @param device
         * @param s
         * @param c
         * @param value
         * @param value_offset
         * @return true if master write has been accepted by GATT-Server listener, otherwise false. Only if all listener return true, the write action will be allowed.
         */
        abstract boolean writeCharValue(BTDevice device, DBGattService s, DBGattChar c, final byte[] value, final int value_offset);

        /**
         *
         * @param device
         * @param s
         * @param c
         * @param d
         * @param value
         * @param value_offset
         * @return true if master write has been accepted by GATT-Server listener, otherwise false. Only if all listener return true, the write action will be allowed.
         */
        abstract boolean writeDescValue(final BTDevice device, DBGattService s, DBGattChar c, DBGattDesc d,
                                        final byte[] value, final int value_offset);

        /**
         *
         * @param device
         * @param s
         * @param c
         * @param d
         * @param notificationEnabled
         * @param indicationEnabled
         */
        abstract void clientCharConfigChanged(final BTDevice device, DBGattService s, DBGattChar c, DBGattDesc d,
                                              final boolean notificationEnabled, final boolean indicationEnabled);

        /**
         * Default comparison operator, merely testing for same memory reference.
         * <p>
         * Specializations may override.
         * </p>
         */
        @Override
        public boolean equals(final Object other) {
            return this == other;
        }
    }
    // private final List<Listener> listenerList = new ArrayList<Listener>();

    /** Used maximum server Rx ATT_MTU */
    public int max_att_mtu = 512 + 1; // BTGattHandler::Defaults::MAX_ATT_MTU;

    /** List of Services. */
    public List<DBGattService> services;

    public DBGattServer() {
        services = new ArrayList<DBGattService>();
    }
    public DBGattServer(final List<DBGattService> services_) {
        services = services_;
    }

    public DBGattService findGattService(final String service_uuid) {
        for(final DBGattService s : services) {
            if( service_uuid.equals( s.type ) ) {
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

    public synchronized boolean addListener(final Listener l) {
        return addListenerImpl(l);
    }
    private native boolean addListenerImpl(Listener l);

    public synchronized boolean removeListener(final Listener l) {
        return removeListenerImpl(l);
    }
    private native boolean removeListenerImpl(Listener l);

    public String toFullString() {
        final String newline = System.lineSeparator();
        final StringBuilder res = new StringBuilder(toString()+newline);
        for(final DBGattService s : services) {
            res.append("  ").append(s.toString()).append(newline);
            for(final DBGattChar c : s.characteristics) {
                res.append("    ").append(c.toString()).append(newline);
                for(final DBGattDesc d : c.descriptors) {
                    res.append("      ").append(d.toString()).append(newline);
                }
            }
        }
        return res.toString();
    }

    @Override
    public String toString() {
        return "DBSrv[max mtu "+max_att_mtu+", "+services.size()+" services]";

    }
}
