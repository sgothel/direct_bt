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

#ifndef DBT_CONSTANTS_HPP
#define DBT_CONSTANTS_HPP

#include <cinttypes>
#include <cstring>
#include <memory>

#include <direct_bt/DirectBT.hpp>

class DBTConstants {
    public:
        /**
         * C++20 we could use `constexpr std::string`
         *
         * C++17 we have to use `const char *`, `std::string_view` or `extern const std::string`.
         */
        static constexpr const char CLIENT_KEY_PATH[] = "client_keys";

        static constexpr const char SERVER_KEY_PATH[] = "server_keys";

        static const jau::uuid128_t DataServiceUUID;
        static const jau::uuid128_t StaticDataUUID;
        static const jau::uuid128_t CommandUUID;
        static const jau::uuid128_t ResponseUUID;
        static const jau::uuid128_t PulseDataUUID;


        /**
         * Success handshake command data, where client is signaling successful completion of test to server.
         */
        static const std::vector<uint8_t> SuccessHandshakeCommandData;

        /**
         * Fail handshake command data, where client is signaling unsuccessful completion of test to server.
         */
        static const std::vector<uint8_t> FailHandshakeCommandData;
};

const jau::uuid128_t DBTConstants::DataServiceUUID = jau::uuid128_t("d0ca6bf3-3d50-4760-98e5-fc5883e93712");
const jau::uuid128_t DBTConstants::StaticDataUUID  = jau::uuid128_t("d0ca6bf3-3d51-4760-98e5-fc5883e93712");
const jau::uuid128_t DBTConstants::CommandUUID     = jau::uuid128_t("d0ca6bf3-3d52-4760-98e5-fc5883e93712");
const jau::uuid128_t DBTConstants::ResponseUUID    = jau::uuid128_t("d0ca6bf3-3d53-4760-98e5-fc5883e93712");
const jau::uuid128_t DBTConstants::PulseDataUUID   = jau::uuid128_t("d0ca6bf3-3d54-4760-98e5-fc5883e93712");

const std::vector<uint8_t> DBTConstants::SuccessHandshakeCommandData = { 0xaa, 0xff, 0xff, 0xee };
const std::vector<uint8_t> DBTConstants::FailHandshakeCommandData = { 0x00, 0xea, 0xea, 0xff };

#endif /* DBT_CONSTANTS_HPP */
