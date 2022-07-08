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

import java.nio.charset.StandardCharsets;

public class BTUtils {
    /**
     * Base UUID128 used to express a UUID16, etc.
     */
    public static final String UUID128_BASE  = "00000000-0000-1000-8000-00805f9b34fb";

    /**
     * Converts the given 4 digits uuid16 value to UUID128 representation.
     * @param uuid16 the 4 characters long uuid16 value to convert
     * @return UUID128 representation if uuid16 is 4 characters long, otherwise {@link #UUID128_BASE}.
     */
    public static final String toUUID128(final String uuid16) {
        if( 4 == uuid16.length() ) {
            return "0000" + uuid16 + UUID128_BASE.substring(8);
        } else {
            return UUID128_BASE;
        }
    }

    /**
     * Defining the supervising timeout for LE connections to be a multiple of the maximum connection interval as follows:
     * <pre>
     *  ( 1 + conn_latency ) * conn_interval_max_ms * max(2, multiplier) [ms]
     * </pre>
     * If above result is smaller than the given min_result_ms, min_result_ms/10 will be returned.
     * @param conn_latency the connection latency
     * @param conn_interval_max_ms the maximum connection interval in [ms]
     * @param min_result_ms the minimum resulting supervisor timeout, defaults to 500ms.
     *        If above formula results in a smaller value, min_result_ms/10 will be returned.
     * @param multiplier recommendation is 6, we use 10 as default for safety.
     * @return the resulting supervising timeout in 1/10 [ms], suitable for the {@link BTDevice#connectLE(short, short, short, short, short, short)}.
     * @see BTDevice#connectLE(short, short, short, short, short, short)
     */
    public static short getHCIConnSupervisorTimeout(final int conn_latency, final int conn_interval_max_ms,
                                                    final int min_result_ms, final int multiplier) {
        return (short) ( Math.max(min_result_ms,
                                  ( 1 + conn_latency ) * conn_interval_max_ms * Math.max(2, multiplier)
                         ) / 10 );
    }
    /**
     * Defining the supervising timeout for LE connections to be a multiple of the maximum connection interval as follows:
     * <pre>
     *  ( 1 + conn_latency ) * conn_interval_max_ms * max(2, multiplier) [ms]
     * </pre>
     *
     * If above result is smaller than the given min_result_ms, min_result_ms/10 will be returned.
     *
     * - Using minimum resulting supervisor timeout of 500ms.
     * - Using multiplier of 10 as default for safety.
     *
     * @param conn_latency the connection latency
     * @param conn_interval_max_ms the maximum connection interval in [ms]
     * @return the resulting supervising timeout in 1/10 [ms], suitable for the {@link BTDevice#connectLE(short, short, short, short, short, short)}.
     * @see BTDevice#connectLE(short, short, short, short, short, short)
     */
    public static short getHCIConnSupervisorTimeout(final int conn_latency, final int conn_interval_max_ms) {
        return getHCIConnSupervisorTimeout(conn_latency, conn_interval_max_ms, 500 /* min_result_ms */, 10);
    }

    /**
     * Decodes the given consecutive UTF-8 characters within buffer to String.
     */
    public static String decodeUTF8String(final byte[] buffer, final int offset, final int size) {
        return new String(buffer, offset, size, StandardCharsets.UTF_8);
    }
}
