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
public final class DBGattChar implements AutoCloseable
{
    private volatile long nativeInstance;
    /* pp */ long getNativeInstance() { return nativeInstance; }

    private final boolean enabledNotifyState = false;
    private final boolean enabledIndicateState = false;

    public static class UUID16 {
        //
        // GENERIC_ACCESS
        //
        public static String DEVICE_NAME                                 = "2a00";
        public static String APPEARANCE                                  = "2a01";
        public static String PERIPHERAL_PRIVACY_FLAG                     = "2a02";
        public static String RECONNECTION_ADDRESS                        = "2a03";
        public static String PERIPHERAL_PREFERRED_CONNECTION_PARAMETERS  = "2a04";

        //
        // GENERIC_ATTRIBUTE
        //
        public static String SERVICE_CHANGED                             = "2a05";

        //
        // DEVICE_INFORMATION
        //
        /** Mandatory: uint40 */
        public static String SYSTEM_ID                                   = "2a23";
        public static String MODEL_NUMBER_STRING                         = "2a24";
        public static String SERIAL_NUMBER_STRING                        = "2a25";
        public static String FIRMWARE_REVISION_STRING                    = "2a26";
        public static String HARDWARE_REVISION_STRING                    = "2a27";
        public static String SOFTWARE_REVISION_STRING                    = "2a28";
        public static String MANUFACTURER_NAME_STRING                    = "2a29";
        public static String REGULATORY_CERT_DATA_LIST                   = "2a2a";
        public static String PNP_ID                                      = "2a50";
    }

    /**
     * Characteristic Handle of this instance.
     * <p>
     * Attribute handles are unique for each device (server) (BT Core Spec v5.2: Vol 3, Part F Protocol..: 3.2.2 Attribute Handle).
     * </p>
     */
    public native short getHandle();

    /**
     * Characteristic end handle, inclusive.
     * <p>
     * Attribute handles are unique for each device (server) (BT Core Spec v5.2: Vol 3, Part F Protocol..: 3.2.2 Attribute Handle).
     * </p>
     */
    public native short getEndHandle();

    /**
     * Characteristics Value Handle.
     * <p>
     * Attribute handles are unique for each device (server) (BT Core Spec v5.2: Vol 3, Part F Protocol..: 3.2.2 Attribute Handle).
     * </p>
     */
    public native short getValueHandle();

    private final String value_type;

    /* Characteristics Value Type UUID (lower-case)*/
    public String getValueType() { return value_type; }

    private final GattCharPropertySet properties;

    /* Characteristics Property */
    public GattCharPropertySet getProperties() { return properties; }

    /* pp */ final List<DBGattDesc> descriptors;

    /** List of Characteristic Descriptions. */
    public final List<DBGattDesc> getDescriptors() { return descriptors; }

    /**
     * Return a copy of this characteristic's native {@link DBGattValue} value.
     *
     * Its capacity defines the maximum writable variable length
     * and its size defines the maximum writable fixed length.
     */
    public native DBGattValue getValue();

    /**
     * Set this characteristic's native value.
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

    /* Optional Client Characteristic Configuration index within descriptorList */
    public final int clientCharConfigIndex;

    /* Optional Characteristic User Description index within descriptorList */
    public final int userDescriptionIndex;

    public DBGattChar(final String value_type_,
                      final GattCharPropertySet properties_,
                      final List<DBGattDesc> descriptors_,
                      final DBGattValue value_)
    {
        value_type = value_type_;
        properties = properties_;
        descriptors = descriptors_;

        {
            int clientCharConfigIndex_ = -1;
            int userDescriptionIndex_ = -1;
            int i=0;
            for(final DBGattDesc d : descriptors) {
                if( 0 > clientCharConfigIndex_ && d.isClientCharConfig() ) {
                    clientCharConfigIndex_=i;
                } else if( 0 > userDescriptionIndex_ && d.isUserDescription() ) {
                    userDescriptionIndex_=i;
                }
                ++i;
            }
            clientCharConfigIndex = clientCharConfigIndex_;
            userDescriptionIndex = userDescriptionIndex_;
        }

        final long[] nativeDescriptors = new long[descriptors_.size()];
        for(int i=0; i < nativeDescriptors.length; i++) {
            nativeDescriptors[i] = descriptors_.get(i).getNativeInstance();
        }
        nativeInstance = ctorImpl(value_type_, properties_.mask, nativeDescriptors, value_.data(), value_.capacity(), value_.hasVariableLength());
    }
    private native long ctorImpl(final String type,
                                 final byte properties, final long[] descriptors,
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

    public boolean hasProperties(final GattCharPropertySet.Type bit) {
        return properties.isSet(bit);
    }

    /** Fill value with zero bytes. */
    public native void bzero();

    public DBGattDesc getClientCharConfig() {
        if( 0 > clientCharConfigIndex ) {
            return null;
        }
        return descriptors.get(clientCharConfigIndex); // abort if out of bounds
    }

    public DBGattDesc getUserDescription() {
        if( 0 > userDescriptionIndex ) {
            return null;
        }
        return descriptors.get(userDescriptionIndex); // abort if out of bounds
    }

    @Override
    public boolean equals(final Object other) {
        if( this == other ) {
            return true;
        }
        if( !(other instanceof DBGattChar) ) {
            return false;
        }
        final DBGattChar o = (DBGattChar)other;
        return getHandle() == o.getHandle() && getEndHandle() == o.getEndHandle(); /** unique attribute handles */
    }

    @Override
    public native String toString();
}
