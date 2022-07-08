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

import org.jau.util.BasicTypes;

/**
 * A copy of the native GATT value of DBGattChar or DBGattDesc.
 *
 * Its {@link #capacity()} defines the maximum writable variable length
 * and its {@link #size()} defines the maximum writable fixed length.
 * See {@link #hasVariableLength()}.
 *
 * @since 2.4.0
 */
public class DBGattValue
{
    private final byte[] value_;

    private final int capacity_;

    private boolean variable_length_;

    /**
     * Constructor
     * @param value the data, which length defines the maximum writable fixed length if variable length is false.
     *        If variable length is true, capacity limits the maximum writable length.
     * @param capacity_ defines the maximum writable variable length, if variable length is true
     * @param variable_length_ defaults to false.
     */
    public DBGattValue(final byte[] value, final int capacity, final boolean variable_length)
    {
        this.value_ = value;
        this.capacity_ = capacity;
        this.variable_length_ = variable_length;
    }

    /**
     * Constructor, using default variable_length = false.
     * @param value the data, which length defines the maximum writable fixed length, if variable length is disabled.
     * @param capacity_ defines the maximum writable variable length, if variable length is true
     */
    public DBGattValue(final byte[] value, final int capacity)
    {
        this(value, capacity, false /* variable_length*/);
    }

    /**
     * Returns true if this value has variable length.
     *
     * Variable length impacts GATT value write behavior.
     *
     * @see #capacity()
     * @see #size()
     */
    public boolean hasVariableLength() { return variable_length_; }

    public void setVariableLength(final boolean v) { variable_length_=v; }

    /**
     * Return the set capacity for this value.
     *
     * Capacity defines the maximum writable variable length,
     * if variable length is enabled.
     *
     * @see #hasVariableLength()
     */
    public int capacity() { return capacity_; }

    /**
     * Return the size of this value, i.e. byte[] length.
     *
     * Size defines the maximum writable fixed length,
     * if variable length is disabled.
     *
     * @see #hasVariableLength()
     */
    public int size() { return value_.length; }

    /**
     * Returns the actual data of this value.
     */
    public byte[] data() { return value_; };

    /** Fill value with zero bytes. */
    public void bzero() {
        for(int i=value_.length-1; 0 <= i; --i) { // anything more efficient?
            value_[i]=0;
        }
    }

    /**
     * Only compares the actual value, not {@link #hasVariableLength()} nor {@link #capacity()}.
     * <p>
     * Parent description:
     * </p>
     * {@inheritDoc}
     */
    @Override
    public boolean equals(final Object other) {
        if( this == other ) {
            return true;
        }
        if( !(other instanceof DBGattValue) ) {
            return false;
        }
        final DBGattValue o = (DBGattValue)other;
        if( value_.length != o.value_.length ) {
            return false;
        }
        for(int i=0; i<value_.length; i++) {
            if( value_[i] != o.value_[i] ) {
                return false;
            }
        }
        // we intentionally ignore capacity
        return true;
    }

    @Override
    public String toString() {
        final String len = hasVariableLength() ? "var" : "fixed";
        return "len "+len+", size "+size()+", capacity "+capacity()+", "+
               BasicTypes.bytesHexString(value_, 0, size(), true /* lsbFirst */) +
               " '"+BTUtils.decodeUTF8String(value_, 0, size())+"'";
    }
}
