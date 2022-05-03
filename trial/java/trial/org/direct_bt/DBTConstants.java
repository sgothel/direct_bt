/**
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2022 Gothel Software e.K.
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

package trial.org.direct_bt;

public class DBTConstants {
    public static final int max_connections_per_session = 5;

    public static final String CLIENT_KEY_PATH = "client_keys";

    public static final String SERVER_KEY_PATH = "server_keys";

    public static final String DataServiceUUID = "d0ca6bf3-3d50-4760-98e5-fc5883e93712";
    public static final String StaticDataUUID  = "d0ca6bf3-3d51-4760-98e5-fc5883e93712";
    public static final String CommandUUID     = "d0ca6bf3-3d52-4760-98e5-fc5883e93712";
    public static final String ResponseUUID    = "d0ca6bf3-3d53-4760-98e5-fc5883e93712";
    public static final String PulseDataUUID   = "d0ca6bf3-3d54-4760-98e5-fc5883e93712";


    /**
     * Success handshake command data, where client is signaling successful completion of test to server.
     */
    public static final byte[] SuccessHandshakeCommandData = { (byte)0xaa, (byte)0xff, (byte)0xff, (byte)0xee };

    /**
     * Fail handshake command data, where client is signaling unsuccessful completion of test to server.
     */
    public static final byte[] FailHandshakeCommandData = { (byte)0x00, (byte)0xea, (byte)0xea, (byte)0xff };
}
