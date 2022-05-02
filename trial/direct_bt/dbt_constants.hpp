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

#include <cinttypes>
#include <cstring>
#include <memory>

#include <jau/uuid.hpp>

class DBTConstants {
    /**
     * C++20 we could use `constexpr std::string`
     *
     * C++17 we have to use `const char *`, `std::string_view` or `extern const std::string`.
     */
    inline constexpr const char CLIENT_KEY_PATH[] = "client_keys";

    inline constexpr const char SERVER_KEY_PATH[] = "server_keys";

    static const jau::uuid128_t DataServiceUUID = jau::uuid128_t("d0ca6bf3-3d50-4760-98e5-fc5883e93712");
    static const jau::uuid128_t StaticDataUUID  = jau::uuid128_t("d0ca6bf3-3d51-4760-98e5-fc5883e93712");
    static const jau::uuid128_t CommandUUID     = jau::uuid128_t("d0ca6bf3-3d52-4760-98e5-fc5883e93712");
    static const jau::uuid128_t ResponseUUID    = jau::uuid128_t("d0ca6bf3-3d53-4760-98e5-fc5883e93712");
    static const jau::uuid128_t PulseDataUUID   = jau::uuid128_t("d0ca6bf3-3d54-4760-98e5-fc5883e93712");


    /**
     * Success handshake command data, where client is signaling successful completion of test to server.
     */
    inline constexpr const std::vector<uint8_t> SuccessHandshakeCommandData = { 0xaa, 0xff, 0xff, 0xee };

    /**
     * Fail handshake command data, where client is signaling unsuccessful completion of test to server.
     */
    inline constexpr const std::vector<uint8_t> FailHandshakeCommandData = { 0x00, 0xea, 0xea, 0xff };
};
