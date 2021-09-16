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
 * Bluetooth roles from the perspective of the link layer (connection initiator).
 * <p>
 * See {@link #get(byte)} for its native integer mapping.
 * </p>
 * @see [BTAdapter roles](@ref BTAdapterRoles).
 * @see [BTDevice roles](@ref BTDeviceRoles).
 * @since 2.4.0
 */
public enum BTRole {
    /** Undefined role. */
    None      ((byte)0),
    /** Master or *central* role, discovering remote devices and initiating connection. This is a GATT client. */
    Master    ((byte)1),
    /** Slave or *peripheral* role, advertising and waiting for connections to accept. This is a GATT server. */
    Slave     ((byte)2);

    public final byte value;

    /**
     * Maps the specified name to a constant of BTRole.
     * <p>
     * Implementation simply returns {@link #valueOf(String)}.
     * This maps the constant names itself to their respective constant.
     * </p>
     * @param name the string name to be mapped to a constant of this enum type.
     * @return the corresponding constant of this enum type.
     * @throws IllegalArgumentException if the specified name can't be mapped to a constant of this enum type
     *                                  as described above.
     */
    public static BTRole get(final String name) throws IllegalArgumentException {
        return valueOf(name);
    }

    /**
     * Maps the specified integer value to a constant of {@link BTRole}.
     * @param value the integer value to be mapped to a constant of this enum type.
     * @return the corresponding constant of this enum type, using {@link #None} if not supported.
     */
    public static BTRole get(final byte value) {
        switch(value) {
            case (byte)0x01: return Master;
            case (byte)0x02: return Slave;
            default: return None;
        }
    }

    BTRole(final byte v) {
        value = v;
    }
}
