/*
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

#ifndef L2CAP_COMM_HPP_
#define L2CAP_COMM_HPP_

#include <cstring>
#include <string>
#include <memory>
#include <cstdint>
#include <vector>

#include <mutex>
#include <atomic>

#include <jau/environment.hpp>

#include "UUID.hpp"
#include "BTTypes.hpp"

/**
 * - - - - - - - - - - - - - - -
 *
 * Module L2CAPComm:
 *
 * - BT Core Spec v5.2: Vol 3, Part A: BT Logical Link Control and Adaption Protocol (L2CAP)
 */
namespace direct_bt {

    class DBTDevice; // forward

    /**
     * L2CAP Singleton runtime environment properties
     * <p>
     * Also see {@link DBTEnv::getExplodingProperties(const std::string & prefixDomain)}.
     * </p>
     */
    class L2CAPEnv : public jau::root_environment {
        private:
            L2CAPEnv() noexcept;

            const bool exploding; // just to trigger exploding properties

        public:
            /**
             * L2CAP poll timeout for reading, defaults to 10s.
             * <p>
             * Environment variable is 'direct_bt.l2cap.reader.timeout'.
             * </p>
             */
            const int32_t L2CAP_READER_POLL_TIMEOUT;

            /**
             * Debugging facility: L2CAP restart count on transmission errors, defaults to 5 attempts.
             * <p>
             * If negative, L2CAPComm will abort() the program.
             * </p>
             * <p>
             * Environment variable is 'direct_bt.l2cap.restart.count'.
             * </p>
             */
            const int32_t L2CAP_RESTART_COUNT_ON_ERROR;

            /**
             * Debug all GATT Data communication
             * <p>
             * Environment variable is 'direct_bt.debug.l2cap.data'.
             * </p>
             */
            const bool DEBUG_DATA;

        public:
            static L2CAPEnv& get() noexcept {
                /**
                 * Thread safe starting with C++11 6.7:
                 *
                 * If control enters the declaration concurrently while the variable is being initialized,
                 * the concurrent execution shall wait for completion of the initialization.
                 *
                 * (Magic Statics)
                 *
                 * Avoiding non-working double checked locking.
                 */
                static L2CAPEnv e;
                return e;
            }
    };

    /**
     * Read/Write L2CAP communication channel.
     */
    class L2CAPComm {
        public:
            enum class Defaults : int {
                L2CAP_CONNECT_MAX_RETRY = 3
            };
            static constexpr int number(const Defaults d) { return static_cast<int>(d); }

            static std::string getStateString(bool isConnected, bool hasIOError) {
                return "State[connected "+std::to_string(isConnected)+", ioError "+std::to_string(hasIOError)+"]";
            }

        private:
            static int l2cap_open_dev(const EUI48 & adapterAddress, const uint16_t psm, const uint16_t cid, const BDAddressType addrType);
            static int l2cap_close_dev(int dd);

            const L2CAPEnv & env;

            std::recursive_mutex mtx_write;
            const std::string deviceString;
            const uint16_t psm;
            const uint16_t cid;
            std::atomic<int> socket_descriptor; // the l2cap socket
            std::atomic<bool> is_connected; // reflects state
            std::atomic<bool> has_ioerror;  // reflects state
            std::atomic<bool> interrupt_flag; // for forced disconnect
            std::atomic<pthread_t> tid_connect;
            std::atomic<pthread_t> tid_read;

        public:
            /**
             * Constructing a newly opened and connected L2CAP channel instance.
             * <p>
             * BT Core Spec v5.2: Vol 3, Part A: L2CAP_CONNECTION_REQ
             * </p>
             */
            L2CAPComm(const DBTDevice& device, const uint16_t psm, const uint16_t cid);

            L2CAPComm(const L2CAPComm&) = delete;
            void operator=(const L2CAPComm&) = delete;

            /** Destructor closing the L2CAP channel, see {@link #disconnect()}. */
            ~L2CAPComm() noexcept { disconnect(); }

            bool isConnected() const { return is_connected; }
            bool hasIOError() const { return has_ioerror; }
            std::string getStateString() const { return getStateString(is_connected, has_ioerror); }

            /** Closing the L2CAP channel, locking {@link #mutex_write()}. */
            bool disconnect() noexcept;

            /** Return the recursive write mutex for multithreading access. */
            std::recursive_mutex & mutex_write() { return mtx_write; }

            /** Generic read, w/o locking suitable for a unique ringbuffer sink. Using L2CAPEnv::L2CAP_READER_POLL_TIMEOUT.*/
            jau::snsize_t read(uint8_t* buffer, const jau::nsize_t capacity);

            /** Generic write, locking {@link #mutex_write()}. */
            jau::snsize_t write(const uint8_t *buffer, const jau::nsize_t length);
    };

} // namespace direct_bt

#endif /* L2CAP_COMM_HPP_ */
