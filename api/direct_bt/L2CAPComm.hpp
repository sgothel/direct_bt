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

#include <mutex>
#include <atomic>

#include <jau/environment.hpp>
#include <jau/uuid.hpp>

#include "BTTypes0.hpp"

/**
 * - - - - - - - - - - - - - - -
 *
 * Module L2CAPComm:
 *
 * - BT Core Spec v5.2: Vol 3, Part A: BT Logical Link Control and Adaption Protocol (L2CAP)
 */
namespace direct_bt {

    class BTDevice; // forward

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

            enum class ExitCode : jau::snsize_t {
                SUCCESS             =  0,
                NOT_OPEN            = -1,
                INTERRUPTED         = -2,
                INVALID_SOCKET_DD   = -3,
                POLL_ERROR          = -10,
                POLL_TIMEOUT        = -11,
                READ_ERROR          = -20,
                WRITE_ERROR         = -30
            };
            static constexpr jau::snsize_t number(const ExitCode rhs) noexcept {
                return static_cast<jau::snsize_t>(rhs);
            }
            static constexpr ExitCode toExitCode(const jau::snsize_t rhs) noexcept {
                return rhs >= 0 ? ExitCode::SUCCESS : static_cast<ExitCode>(rhs);
            }
            static std::string getExitCodeString(const ExitCode ec) noexcept;
            static std::string getExitCodeString(const jau::snsize_t ecn) noexcept {
                return getExitCodeString( toExitCode( ecn ) );
            }

            static std::string getStateString(bool isOpen, bool hasIOError) {
                return "State[open "+std::to_string(isOpen)+
                        ", ioError "+std::to_string(hasIOError)+
                        ", errno "+std::to_string(errno)+" ("+std::string(strerror(errno))+")]";
            }
            static std::string getStateString(bool isOpen, bool isInterrupted, bool hasIOError) {
                return "State[open "+std::to_string(isOpen)+
                       ", isIRQed "+std::to_string(isInterrupted)+
                       ", ioError "+std::to_string(hasIOError)+
                       ", errno "+std::to_string(errno)+" ("+std::string(strerror(errno))+")]";
            }

        private:
            static int l2cap_open_dev(const BDAddressAndType & adapterAddressAndType, const L2CAP_PSM psm, const L2CAP_CID cid);
            static int l2cap_close_dev(int dd);

            const L2CAPEnv & env;
            const BDAddressAndType adapterAddressAndType;
            const L2CAP_PSM psm;
            const L2CAP_CID cid;

            std::recursive_mutex mtx_write;
            BDAddressAndType deviceAddressAndType;
            std::atomic<int> socket_descriptor; // the l2cap socket
            std::atomic<bool> is_open; // reflects state
            std::atomic<bool> has_ioerror;  // reflects state
            std::atomic<bool> interrupt_flag; // for forced disconnect
            std::atomic<pthread_t> tid_connect;
            std::atomic<pthread_t> tid_read;

            bool setBTSecurityLevelImpl(const BTSecurityLevel sec_level);
            BTSecurityLevel getBTSecurityLevelImpl();

        public:
            /**
             * Constructing a non connected L2CAP channel instance for the pre-defined PSM and CID.
             */
            L2CAPComm(const BDAddressAndType& adapterAddressAndType, const L2CAP_PSM psm, const L2CAP_CID cid);

            L2CAPComm(const L2CAPComm&) = delete;
            void operator=(const L2CAPComm&) = delete;

            /** Destructor closing the L2CAP channel, see {@link #disconnect()}. */
            ~L2CAPComm() noexcept { close(); }

            /**
             * Opens and connects the L2CAP channel, locking {@link #mutex_write()}.
             * <p>
             * BT Core Spec v5.2: Vol 3, Part A: L2CAP_CONNECTION_REQ
             * </p>
             *
             * @param device the remote device to establish this L2CAP connection
             * @param sec_level sec_level < BTSecurityLevel::NONE will not set security level
             * @return true if connection has been established, otherwise false
             */
            bool open(const BTDevice& device, const BTSecurityLevel sec_level=BTSecurityLevel::NONE);

            bool isOpen() const { return is_open; }

            /** Closing the L2CAP channel, locking {@link #mutex_write()}. */
            bool close() noexcept;

            /** Return this L2CAP socket descriptor. */
            inline int getSocketDescriptor() const noexcept { return socket_descriptor; }

            bool hasIOError() const { return has_ioerror; }
            std::string getStateString() const { return getStateString(is_open, interrupt_flag, has_ioerror); }

            /** Return the recursive write mutex for multithreading access. */
            std::recursive_mutex & mutex_write() { return mtx_write; }

            /**
             * If sec_level > ::BTSecurityLevel::UNSET, sets the BlueZ's L2CAP socket BT_SECURITY sec_level, determining the SMP security mode per connection.
             * <p>
             * To unset security, the L2CAP socket should be closed and opened again.
             * </p>
             *
             * @param sec_level sec_level < ::BTSecurityLevel::NONE will not set security level and returns false.
             * @return true if a security level > ::BTSecurityLevel::UNSET has been set successfully, false if no security level has been set or if it failed.
             */
            bool setBTSecurityLevel(const BTSecurityLevel sec_level);

            /**
             * Fetches the current BlueZ's L2CAP socket BT_SECURITY sec_level.
             *
             * @return ::BTSecurityLevel  sec_level value, ::BTSecurityLevel::UNSET if failure
             */
            BTSecurityLevel getBTSecurityLevel();

            /**
             * Generic read, w/o locking suitable for a unique ringbuffer sink. Using L2CAPEnv::L2CAP_READER_POLL_TIMEOUT.
             * @param buffer
             * @param capacity
             * @return number of bytes read if >= 0, otherwise L2CAPComm::ExitCode error code.
             */
            jau::snsize_t read(uint8_t* buffer, const jau::nsize_t capacity);

            /**
             * Generic write, locking {@link #mutex_write()}.
             * @param buffer
             * @param length
             * @return number of bytes written if >= 0, otherwise L2CAPComm::ExitCode error code.
             */
            jau::snsize_t write(const uint8_t *buffer, const jau::nsize_t length);
    };

} // namespace direct_bt

#endif /* L2CAP_COMM_HPP_ */
