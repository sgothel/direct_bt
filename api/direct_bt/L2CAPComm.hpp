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

    class L2CAPServer; // fwd

    /**
     * L2CAP read/write communication channel to remote device
     */
    class L2CAPComm {
        public:
            enum class Defaults : int {
                L2CAP_CONNECT_MAX_RETRY = 3
            };
            static constexpr int number(const Defaults d) noexcept { return static_cast<int>(d); }

            /**
             * Exit code for read() and write() operations
             */
            enum class RWExitCode : jau::snsize_t {
                SUCCESS             =  0, /**< SUCCESS */
                NOT_OPEN            = -1, /**< NOT_OPEN */
                INTERRUPTED         = -2, /**< INTERRUPTED */
                INVALID_SOCKET_DD   = -3, /**< INVALID_SOCKET_DD */
                POLL_ERROR          = -10,/**< POLL_ERROR */
                POLL_TIMEOUT        = -11,/**< POLL_TIMEOUT */
                READ_ERROR          = -20,/**< READ_ERROR */
                WRITE_ERROR         = -30 /**< WRITE_ERROR */
            };
            static constexpr jau::snsize_t number(const RWExitCode rhs) noexcept {
                return static_cast<jau::snsize_t>(rhs);
            }
            static constexpr RWExitCode toRWExitCode(const jau::snsize_t rhs) noexcept {
                return rhs >= 0 ? RWExitCode::SUCCESS : static_cast<RWExitCode>(rhs);
            }
            static std::string getRWExitCodeString(const RWExitCode ec) noexcept;
            static std::string getRWExitCodeString(const jau::snsize_t ecn) noexcept {
                return getRWExitCodeString( toRWExitCode( ecn ) );
            }

            static std::string getStateString(bool isOpen, bool hasIOError) noexcept {
                return "State[open "+std::to_string(isOpen)+
                        ", ioError "+std::to_string(hasIOError)+
                        ", errno "+std::to_string(errno)+" ("+std::string(strerror(errno))+")]";
            }
            static std::string getStateString(bool isOpen, bool isInterrupted, bool hasIOError) noexcept {
                return "State[open "+std::to_string(isOpen)+
                       ", isIRQed "+std::to_string(isInterrupted)+
                       ", ioError "+std::to_string(hasIOError)+
                       ", errno "+std::to_string(errno)+" ("+std::string(strerror(errno))+")]";
            }

        private:
            friend class L2CAPServer;

            static int l2cap_open_dev(const BDAddressAndType & adapterAddressAndType, const L2CAP_PSM psm, const L2CAP_CID cid) noexcept;
            static int l2cap_close_dev(int dd) noexcept;

            const L2CAPEnv & env;

        public:
            const BDAddressAndType localAddressAndType;
            const L2CAP_PSM psm;
            const L2CAP_CID cid;

        private:
            std::recursive_mutex mtx_write;
            BDAddressAndType remoteAddressAndType;
            std::atomic<int> client_socket; // the l2cap socket
            std::atomic<bool> is_open; // reflects state
            std::atomic<bool> has_ioerror;  // reflects state
            std::atomic<bool> interrupt_flag; // for forced disconnect
            std::atomic<pthread_t> tid_connect;
            std::atomic<pthread_t> tid_read;

            bool setBTSecurityLevelImpl(const BTSecurityLevel sec_level) noexcept;
            BTSecurityLevel getBTSecurityLevelImpl() noexcept;

        public:
            /**
             * Constructing a non connected L2CAP channel instance for the pre-defined PSM and CID.
             */
            L2CAPComm(const BDAddressAndType& adapterAddressAndType, const L2CAP_PSM psm, const L2CAP_CID cid) noexcept;

            /**
             * Constructing a connected L2CAP channel instance for the pre-defined PSM and CID.
             */
            L2CAPComm(const BDAddressAndType& adapterAddressAndType, const L2CAP_PSM psm, const L2CAP_CID cid,
                      const BDAddressAndType& remoteAddressAndType, int client_socket) noexcept;

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
            bool open(const BTDevice& device, const BTSecurityLevel sec_level=BTSecurityLevel::NONE) noexcept;

            bool isOpen() const noexcept { return is_open; }

            const BDAddressAndType& getRemoteAddressAndType() const noexcept { return remoteAddressAndType; }

            /** Closing the L2CAP channel, locking {@link #mutex_write()}. */
            bool close() noexcept;

            /** Return this L2CAP socket descriptor. */
            inline int getSocket() const noexcept { return client_socket; }

            bool hasIOError() const noexcept { return has_ioerror; }
            std::string getStateString() const noexcept { return getStateString(is_open, interrupt_flag, has_ioerror); }

            /** Return the recursive write mutex for multithreading access. */
            std::recursive_mutex & mutex_write() noexcept { return mtx_write; }

            /**
             * If sec_level > ::BTSecurityLevel::UNSET, sets the BlueZ's L2CAP socket BT_SECURITY sec_level, determining the SMP security mode per connection.
             *
             * To unset security, the L2CAP socket should be closed and opened again.
             *
             * If setting the security level fails, close() will be called.
             *
             * @param sec_level sec_level == ::BTSecurityLevel::UNSET will not set security level and returns true.
             * @return true if a security level > ::BTSecurityLevel::UNSET has been set successfully, false if no security level has been set or if it failed.
             */
            bool setBTSecurityLevel(const BTSecurityLevel sec_level) noexcept;

            /**
             * Fetches the current BlueZ's L2CAP socket BT_SECURITY sec_level.
             *
             * @return ::BTSecurityLevel  sec_level value, ::BTSecurityLevel::UNSET if failure
             */
            BTSecurityLevel getBTSecurityLevel() noexcept;

            /**
             * Generic read, w/o locking suitable for a unique ringbuffer sink. Using L2CAPEnv::L2CAP_READER_POLL_TIMEOUT.
             * @param buffer
             * @param capacity
             * @return number of bytes read if >= 0, otherwise L2CAPComm::ExitCode error code.
             */
            jau::snsize_t read(uint8_t* buffer, const jau::nsize_t capacity) noexcept;

            /**
             * Generic write, locking {@link #mutex_write()}.
             * @param buffer
             * @param length
             * @return number of bytes written if >= 0, otherwise L2CAPComm::ExitCode error code.
             */
            jau::snsize_t write(const uint8_t *buffer, const jau::nsize_t length) noexcept;

            std::string toString();
    };

    /**
     * L2CAP server socket to listen for connecting remote devices
     */
    class L2CAPServer {
        private:
            const BDAddressAndType localAddressAndType;
            const L2CAP_PSM psm;
            const L2CAP_CID cid;

            std::recursive_mutex mtx_open;
            std::atomic<int> server_socket; // the server socket
            std::atomic<bool> is_open; // reflects state
            std::atomic<bool> interrupt_flag; // for forced disconnect
            std::atomic<pthread_t> tid_accept;

        public:
            L2CAPServer(const BDAddressAndType& localAddressAndType, const L2CAP_PSM psm, const L2CAP_CID cid) noexcept;

            L2CAPServer(const L2CAPServer&) = delete;
            void operator=(const L2CAPServer&) = delete;

            /** Destructor closing the L2CAP channel, see {@link #disconnect()}. */
            ~L2CAPServer() noexcept { close(); }

            bool open() noexcept;

            bool isOpen() const noexcept { return is_open; }

            bool close() noexcept;

            /** Return this L2CAP socket descriptor. */
            inline int getSocket() const noexcept { return server_socket; }

            std::unique_ptr<L2CAPComm> accept() noexcept;

            std::string toString();
    };

} // namespace direct_bt

#endif /* L2CAP_COMM_HPP_ */
