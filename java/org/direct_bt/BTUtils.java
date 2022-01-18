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

import java.io.PrintStream;

import org.jau.util.BasicTypes;

public class BTUtils {
    private static long t0;
    static {
        t0 = startupTimeMillisImpl();
    }
    private static native long startupTimeMillisImpl();

    /**
     * Returns current monotonic time in milliseconds.
     */
    public static native long currentTimeMillis();

    /**
     * Returns current wall-clock system `time of day` in seconds since Unix Epoch
     * `00:00:00 UTC on 1 January 1970`.
     */
    public static native long wallClockSeconds();

    /**
     * Returns the startup time in monotonic time in milliseconds of the native module.
     */
    public static long startupTimeMillis() { return t0; }

    /**
     * Returns current elapsed monotonic time in milliseconds since module startup, see {@link #startupTimeMillis()}.
     */
    public static long elapsedTimeMillis() { return currentTimeMillis() - t0; }

    /**
     * Returns elapsed monotonic time in milliseconds since module startup comparing against the given timestamp, see {@link #startupTimeMillis()}.
     */
    public static long elapsedTimeMillis(final long current_ts) { return current_ts - t0; }

    /**
     * Convenient {@link PrintStream#printf(String, Object...)} invocation, prepending the {@link #elapsedTimeMillis()} timestamp.
     * @param out the output stream
     * @param format the format
     * @param args the arguments
     */
    public static void fprintf_td(final PrintStream out, final String format, final Object ... args) {
        out.printf("[%,9d] ", elapsedTimeMillis());
        out.printf(format, args);
    }
    /**
     * Convenient {@link PrintStream#println(String)} invocation, prepending the {@link #elapsedTimeMillis()} timestamp.
     * @param out the output stream
     * @param msg the string message
     */
    public static void println(final PrintStream out, final String msg) {
        out.printf("[%,9d] %s%s", elapsedTimeMillis(), msg, System.lineSeparator());
    }
    /**
     * Convenient {@link PrintStream#print(String)} invocation, prepending the {@link #elapsedTimeMillis()} timestamp.
     * @param out the output stream
     * @param msg the string message
     */
    public static void print(final PrintStream out, final String msg) {
        out.printf("[%,9d] %s", elapsedTimeMillis(), msg);
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
     * Produce a lower-case hexadecimal string representation of the given byte values.
     * <p>
     * If lsbFirst is true, orders LSB left -> MSB right, usual for byte streams.<br>
     * Otherwise orders MSB left -> LSB right, usual for readable integer values.
     * </p>
     * @param bytes the byte array to represent
     * @param offset offset in byte array to the first byte to print.
     * @param length number of bytes to print. If negative, will use {@code bytes.length - offset}.
     * @param lsbFirst true having the least significant byte printed first (lowest addressed byte to highest),
     *                 otherwise have the most significant byte printed first (highest addressed byte to lowest).
     * @return the hex-string representation of the data
     */
    public static String bytesHexString(final byte[] bytes, final int offset, final int length,
                                        final boolean lsbFirst)
    {
        return BasicTypes.bytesHexString(bytes, offset, length, lsbFirst);
    }

    /**
     * Produce a hexadecimal string representation of the given byte value.
     * @param sb the StringBuilder destination to append
     * @param value the byte value to represent
     * @param lowerCase true to use lower case hex-chars, otherwise capital letters are being used.
     * @return the given StringBuilder for chaining
     */
    public static StringBuilder byteHexString(final StringBuilder sb, final byte value, final boolean lowerCase)
    {
        return BasicTypes.byteHexString(sb, value, lowerCase);
    }

    /**
     * Returns all valid consecutive UTF-8 characters within buffer
     * in the range offset -> size or until EOS.
     * <p>
     * In case a non UTF-8 character has been detected,
     * the content will be cut off and the decoding loop ends.
     * </p>
     * <p>
     * Method utilizes a finite state machine detecting variable length UTF-8 codes.
     * See <a href="http://bjoern.hoehrmann.de/utf-8/decoder/dfa/">Bjoern Hoehrmann's site</a> for details.
     * </p>
     */
    public static native String decodeUTF8String(final byte[] buffer, final int offset, final int size);

}
