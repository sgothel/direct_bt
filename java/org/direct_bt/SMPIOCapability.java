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
package org.tinyb;

/**
 * SMP IO Capability value.
 * <p>
 * Vol 3, Part H, 2.3.2 IO capabilities
 * </p>
 * <p>
 * See {@link #get(byte)} for its native integer mapping.
 * </p>
 * @since 2.1.0
 */
public enum SMPIOCapability {
    /** Display output only, value 0. */
    DISPLAY_ONLY        ((byte)0),
    /** Display output and boolean confirmation input keys only, value 1. */
    DISPLAY_YES_NO      ((byte)1),
    /** Keyboard input only, value 2. */
    KEYBOARD_ONLY       ((byte)2),
    /** No input not output, value 3. */
    NO_INPUT_NO_OUTPUT  ((byte)3),
    /** Display output and keyboard input, value 4. */
    KEYBOARD_DISPLAY    ((byte)4),
    /** Denoting unset value, i.e. not defined. */
    UNSET               ((byte)0xff);

    public final byte value;

    /**
     * Maps the specified name to a constant of SMPIOCapability.
     * <p>
     * Implementation simply returns {@link #valueOf(String)}.
     * This maps the constant names itself to their respective constant.
     * </p>
     * @param name the string name to be mapped to a constant of this enum type.
     * @return the corresponding constant of this enum type.
     * @throws IllegalArgumentException if the specified name can't be mapped to a constant of this enum type
     *                                  as described above.
     */
    public static SMPIOCapability get(final String name) throws IllegalArgumentException {
        return valueOf(name);
    }

    /**
     * Maps the specified integer value to a constant of {@link SMPIOCapability}.
     * @param value the integer value to be mapped to a constant of this enum type.
     * @return the corresponding constant of this enum type, using {@link #UNSET} if not supported.
     */
    public static SMPIOCapability get(final byte value) {
        switch(value) {
            case (byte)0x00: return DISPLAY_ONLY;
            case (byte)0x01: return DISPLAY_YES_NO;
            case (byte)0x02: return KEYBOARD_ONLY;
            case (byte)0x03: return NO_INPUT_NO_OUTPUT;
            case (byte)0x04: return KEYBOARD_DISPLAY;
            default: return UNSET;
        }
    }

    SMPIOCapability(final byte v) {
        value = v;
    }
}
