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
 * Discovery policy defines the {@link BTAdapter} discovery mode after connecting a remote {@link BTDevice}:
 * - turned-off ({@link DiscoveryPolicy#AUTO_OFF})
 * - paused until all connected BTDevice become disconnected, effectively until {@link AdapterStatusListener#deviceDisconnected(BTDevice, HCIStatusCode, short, long)} ({@link DiscoveryPolicy#PAUSE_CONNECTED_UNTIL_DISCONNECTED}).
 * - paused until all connected devices reach readiness inclusive optional SMP pairing (~120ms) and GATT service discovery (~700ms), effectively until {@link AdapterStatusListener#deviceReady(BTDevice, long)}. ({@link DiscoveryPolicy#PAUSE_CONNECTED_UNTIL_READY}, default)
 * - paused until all connected devices are optionally SMP paired (~120ms), exclusive GATT service discovery (~700ms -> ~1200ms, {@link DiscoveryPolicy#PAUSE_CONNECTED_UNTIL_PAIRED})
 * - always enabled, i.e. re-enabled if automatically turned-off by HCI host OS as soon as possible ({@link DiscoveryPolicy#ALWAYS_ON})
 *
 * Policy is set via {@link BTAdapter#startDiscovery(DiscoveryPolicy, boolean, short, short, byte, boolean)}.
 *
 * Default is {@link DiscoveryPolicy#PAUSE_CONNECTED_UNTIL_READY}, as it has been shown that continuous advertising
 * reduces the bandwidth for the initial bring-up time including GATT service discovery considerably.
 * Continuous advertising would increase the readiness lag of the remote device until {@link AdapterStatusListener#deviceReady(BTDevice, long)}.
 *
 * In case users favors faster parallel discovery of new remote devices and hence a slower readiness,
 * {@link DiscoveryPolicy#PAUSE_CONNECTED_UNTIL_PAIRED} or even {@link DiscoveryPolicy#ALWAYS_ON} can be used.
 *
 * @since 2.5.0
 */
public enum DiscoveryPolicy {
    /** Turn off discovery when a BTDevice gets connected and leave discovery disabled, if turned off by host system. */
    AUTO_OFF                     ((byte) 0),
    /**
     * Pause discovery until all connected {@link BTDevice} become disconnected,
     * effectively until {@link AdapterStatusListener#deviceDisconnected(BTDevice, HCIStatusCode, short, long)}.
     */
    PAUSE_CONNECTED_UNTIL_DISCONNECTED ((byte) 1),
    /**
     * Pause discovery until all connected {@link BTDevice} reach readiness inclusive optional SMP pairing (~120ms) without GATT service discovery (~700ms),
     * effectively until {@link AdapterStatusListener#deviceReady(BTDevice, long)}. This is the default!
     */
    PAUSE_CONNECTED_UNTIL_READY  ((byte) 2),
    /** Pause discovery until all connected {@link BTDevice} are optionally SMP paired (~120ms) without GATT service discovery (~700ms). */
    PAUSE_CONNECTED_UNTIL_PAIRED ((byte) 3),
    /** Always keep discovery enabled, i.e. re-enabled if automatically turned-off by HCI host OS as soon as possible. */
    ALWAYS_ON                    ((byte) 4);

    public final byte value;

    /**
     * Maps the specified integer value to a constant of {@link DiscoveryPolicy}.
     * @param value the integer value to be mapped to a constant of this enum type.
     * @return the corresponding constant of this enum type, using {@link #AUTO_OFF} if not supported.
     */
    public static DiscoveryPolicy get(final byte value) {
        switch(value) {
            case (byte) 1: return PAUSE_CONNECTED_UNTIL_DISCONNECTED;
            case (byte) 2: return PAUSE_CONNECTED_UNTIL_READY;
            case (byte) 3: return PAUSE_CONNECTED_UNTIL_PAIRED;
            case (byte) 4: return ALWAYS_ON;
            default: return AUTO_OFF;
        }
    }

    DiscoveryPolicy(final byte v) {
        value = v;
    }
}
