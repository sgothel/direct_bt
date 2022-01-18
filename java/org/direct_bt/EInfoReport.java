/**
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2022 Gothel Software e.K.
 * Copyright (c) 2022 ZAFENA AB
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

import java.nio.ByteOrder;
import java.util.List;
import java.util.Map;

import org.jau.net.EUI48;

/**
 * Collection of 'Extended Advertising Data' (EAD), 'Advertising Data' (AD)
 * or 'Extended Inquiry Response' (EIR) information.
 *
 * References:
 *
 * - BT Core Spec v5.2: Vol 4, Part E, 7.7.65.2 LE Advertising Report event
 * - BT Core Spec v5.2: Vol 4, Part E, 7.7.65.13 LE Extended Advertising Report event
 * - BT Core Spec v5.2: Vol 3, Part C, 11 ADVERTISING AND SCAN RESPONSE DATA FORMAT
 * - BT Core Spec v5.2: Vol 3, Part C, 8  EXTENDED INQUIRY RESPONSE DATA FORMAT
 * - BT Core Spec Supplement v9, Part A: Section 1 + 2 Examples, p25..
 * - [Assigned Numbers - Generic Access Profile](https://www.bluetooth.com/specifications/assigned-numbers/generic-access-profile/)
 *
 * @since 2.5.3
 */
public final class EInfoReport implements AutoCloseable
{
    private volatile long nativeInstance;
    /* pp */ long getNativeInstance() { return nativeInstance; }

    public EInfoReport() {
        nativeInstance = ctorImpl();
    }
    private native long ctorImpl();

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

    /**
     * Reset all data fields.
     */
    public final native void clear();

    /**
     * Merge all fields from given EInfoReport if set and different.
     * @param eir
     * @return The changed fields, i.e. EIRDataType bit field
     */
    public final EIRDataTypeSet set(final EInfoReport eir) {
        return new EIRDataTypeSet(setImpl(eir));
    }
    private native int setImpl(final EInfoReport eir);

    // public native void setSource(Source s);
    // public native void setTimestamp(uint64_t ts);
    // public native void setEvtType(AD_PDU_Type et);
    // public native void setExtEvtType(EAD_Event_Type eadt);
    public final void setAddressType(final BDAddressType at) {
        setAddressTypeImpl(at.value);
    }
    private native void setAddressTypeImpl(final byte at);

    public final void setAddress(final EUI48 a) {
        setAddressImpl(a.b);
    }
    private native void setAddressImpl(final byte[] a);

    public native void setRSSI(final byte v);
    public native void setTxPower(final byte v);

    public final void setFlags(final GAPFlags f) {
        setFlagsImpl(f.mask);
    }
    private native void setFlagsImpl(final byte f);

    public final void addFlag(final GAPFlags.Bit f) {
        addFlagImpl(f.value);
    }
    private native void addFlagImpl(final byte f);

    public native void setName(final String name);
    public native void setShortName(final String name_short);

    // public native void setManufactureSpecificData(const ManufactureSpecificData& msd_);
    public native void addService(final String uuid);
    public native void setServicesComplete(final boolean v);
    public native void setDeviceClass(final int c);
    // public native void setAppearance(AppearanceCat a);
    // public native void setHash(const uint8_t * h);
    // public native void setRandomizer(const uint8_t * r);
    public native void setDeviceID(final short source, final short vendor, final short product, final short version);

    /**
     * Set slave connection interval range.
     *
     * Bluetooth Supplement, Part A, section 1.9
     *
     * @param min conn_interval_min in units of 1.25ms, default value 10 for 12.5ms; Value range [6 .. 3200] for [7.5ms .. 4000ms]
     * @param max conn_interval_max in units of 1.25ms, default value 24 for 30.0ms; Value range [6 .. 3200] for [7.5ms .. 4000ms]
     */
    public native void setConnInterval(final short min, final short max);

    // public native Source getSource();
    public native long getTimestamp();

    public final EIRDataTypeSet getEIRDataMask() {
        return new EIRDataTypeSet(getEIRDataMaskImpl());
    }
    private native int getEIRDataMaskImpl();

    public final boolean isSet(final EIRDataTypeSet.DataType bit) { return getEIRDataMask().isSet(bit); }

    // public native AD_PDU_Type getEvtType();
    // public native EAD_Event_Type getExtEvtType();
    public final GAPFlags getFlags() {
        return new GAPFlags(getFlagsImpl());
    }
    private native byte getFlagsImpl();

    public native byte getADAddressType();

    public final BDAddressType getAddressType() {
        return BDAddressType.get(getAddressTypeImpl());
    }
    private native byte getAddressTypeImpl();

    public final EUI48 getAddress() {
        return new EUI48(getAddressImpl(), ByteOrder.nativeOrder());
    }
    private native byte[/*6*/] getAddressImpl();

    public native String getName();
    public native String getShortName();
    public native byte getRSSI();
    public native byte getTxPower();

    /**
     * Returns a map containing manufacturer specific advertisement data.
     * An entry has a uint16_t key (company) and an array of bytes (data).
     * @return manufacturer specific advertisement data.
     */
    public native Map<Short, byte[]> getManufacturerData();

    public native List<String> getServices();
    public native boolean getServicesComplete();

    public native int getDeviceClass();
    // public native AppearanceCat getAppearance();
    // public native jau::TROOctets & getHash();
    // public native jau::TROOctets & getRandomizer();
    public native short getDeviceIDSource();
    public native short getDeviceIDVendor();
    public native short getDeviceIDProduct();
    public native short getDeviceIDVersion();

    /**
     * Get slave connection interval range.
     *
     * Bluetooth Supplement, Part A, section 1.9
     *
     * @param min conn_interval_min in units of 1.25ms, default value 10 for 12.5ms; Value range [6 .. 3200] for [7.5ms .. 4000ms]
     * @param max conn_interval_max in units of 1.25ms, default value 24 for 30.0ms; Value range [6 .. 3200] for [7.5ms .. 4000ms]
     */
    public native void getConnInterval(final short minmax[/*2*/]);

    public native String getDeviceIDModalias();
    public native String eirDataMaskToString();
    public native String toString(final boolean includeServices);

    @Override
    public final String toString() { return toString(true); }
}
