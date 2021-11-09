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
public class DBGattChar
{
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
    public short handle;

    /**
     * Characteristic end handle, inclusive.
     * <p>
     * Attribute handles are unique for each device (server) (BT Core Spec v5.2: Vol 3, Part F Protocol..: 3.2.2 Attribute Handle).
     * </p>
     */
    public short end_handle;

    /**
     * Characteristics Value Handle.
     * <p>
     * Attribute handles are unique for each device (server) (BT Core Spec v5.2: Vol 3, Part F Protocol..: 3.2.2 Attribute Handle).
     * </p>
     */
    public short value_handle;

    /* Characteristics Value Type UUID (lower-case)*/
    public String value_type;

    /* Characteristics Property */
    GattCharPropertySet properties;

    /** List of Characteristic Descriptions. */
    public List<DBGattDesc> descriptors;

    /**
     * Characteristics's Value.
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

    /* Optional Client Characteristic Configuration index within descriptorList */
    public int clientCharConfigIndex;

    /* Optional Characteristic User Description index within descriptorList */
    public int userDescriptionIndex;

    private void setup(final String value_type_,
                       final GattCharPropertySet properties_,
                       final List<DBGattDesc> descriptors_,
                       final byte[] value_, final boolean variable_length_)
    {
        handle = 0;
        end_handle = 0;
        value_handle = 0;
        value_type = value_type_;
        properties = properties_;
        descriptors = descriptors_;
        value = value_;
        variable_length = variable_length_;
        clientCharConfigIndex = -1;
        userDescriptionIndex = -1;

        int i=0;
        for(final DBGattDesc d : descriptors) {
            if( 0 > clientCharConfigIndex && d.isClientCharConfig() ) {
                clientCharConfigIndex=i;
            } else if( 0 > userDescriptionIndex && d.isUserDescription() ) {
                userDescriptionIndex=i;
            }
            ++i;
        }
    }

    public DBGattChar(final String value_type_,
                      final GattCharPropertySet properties_,
                      final List<DBGattDesc> descriptors_,
                      final byte[] value_, final boolean variable_length_)
    {
        setup(value_type_, properties_, descriptors_, value_, variable_length_);
    }

    public DBGattChar(final String value_type_,
                      final GattCharPropertySet properties_,
                      final List<DBGattDesc> descriptors_,
                      final byte[] value_)
    {
        setup(value_type_, properties_, descriptors_, value_, true /* variable_length_ */);
    }

    public boolean hasProperties(final GattCharPropertySet.Type bit) {
        return properties.isSet(bit);
    }

    /** Fill value with zero bytes. */
    public void bzero() {
        for(int i=0; i<value.length; i++) { // anything more efficient?
            value[i]=0;
        }
    }

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
        return handle == o.handle; /** unique attribute handles */
    }

    @Override
    public String toString() {
        final String char_name;
        final String notify_str;
        {
            final DBGattDesc ud = getUserDescription();
            if( null != ud ) {
                char_name = ", '" + BTUtils.decodeUTF8String(ud.value, 0, ud.value.length) + "'";
            } else {
                char_name = "";
            }
        }
        if( hasProperties(GattCharPropertySet.Type.Notify) || hasProperties(GattCharPropertySet.Type.Indicate) ) {
            notify_str = ", enabled[notify "+enabledNotifyState+", indicate "+enabledIndicateState+"]";
        } else {
            notify_str = "";
        }
        final String len = variable_length ? "var" : "fixed";
        return "Char[handle [0x"+Integer.toHexString(handle)+"..0x"+Integer.toHexString(end_handle)+
               "], props 0x"+Integer.toHexString(properties.mask)+" "+properties.toString()+
               char_name+", value[type 0x"+value_type+", handle 0x"+Integer.toHexString(value_handle)+", len "+len+
               ", "+BTUtils.bytesHexString(value, 0, value.length, true /* lsbFirst */)+
               " '"+BTUtils.decodeUTF8String(value, 0, value.length)+"'"+
               "], ccd-idx "+clientCharConfigIndex+notify_str+"]";
    }

}
