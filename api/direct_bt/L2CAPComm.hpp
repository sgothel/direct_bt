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
#include <jau/function_def.hpp>

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
     * L2CAP client/server socket abstract base class to listen for connecting remote devices
     */
    class L2CAPComm {
        public:
            static std::string getStateString(bool isOpen, bool hasIOError) noexcept;
            static std::string getStateString(bool isOpen, bool isInterrupted, bool hasIOError) noexcept;

            /** Utilized to query for external interruption, whether device is still connected etc. */
            typedef jau::FunctionDef<bool, int /* dummy*/> get_boolean_callback_t;

        protected:
            static int l2cap_open_dev(const BDAddressAndType & adapterAddressAndType, const L2CAP_PSM psm, const L2CAP_CID cid) noexcept;
            static int l2cap_close_dev(int dd) noexcept;

            const L2CAPEnv & env;

        public:
            /** Corresponding BTAdapter device id */
            const uint16_t adev_id;
            /** Corresponding BTAdapter local BTAddressAndType */
            const BDAddressAndType localAddressAndType;
            /** Corresponding L2CAP_PSM for the channel. */
            const L2CAP_PSM psm;
            /** Corresponding L2CAP_CID for the channel. */
            const L2CAP_CID cid;

        protected:
            std::recursive_mutex mtx_open;
            jau::relaxed_atomic_int socket_; // the native socket
            jau::sc_atomic_bool is_open_; // reflects state
            jau::sc_atomic_bool interrupted_intern; // for forced disconnect and read/accept interruption via close()
            get_boolean_callback_t is_interrupted_extern; // for forced disconnect and read/accept interruption via external event

            bool setBTSecurityLevelImpl(const BTSecurityLevel sec_level, const BDAddressAndType& remoteAddressAndType) noexcept;
            BTSecurityLevel getBTSecurityLevelImpl(const BDAddressAndType& remoteAddressAndType) noexcept;

        public:
            L2CAPComm(const uint16_t adev_id, const BDAddressAndType& localAddressAndType, const L2CAP_PSM psm, const L2CAP_CID cid) noexcept;

            /** Destructor specialization shall close the L2CAP socket, see {@link #close()}. */
            virtual ~L2CAPComm() noexcept {}

            L2CAPComm(const L2CAPComm&) = delete;
            void operator=(const L2CAPComm&) = delete;

            bool is_open() const noexcept { return is_open_; }

            /** The external `is interrupted` callback is used until close(), thereafter it is removed. */
            void set_interrupted_query(get_boolean_callback_t is_interrupted_cb) { is_interrupted_extern = is_interrupted_cb; }

            /** Returns true if interrupted by internal or external cause, hence shall stop connecting and reading. */
            bool interrupted() const noexcept { return interrupted_intern || ( !is_interrupted_extern.isNullType() && is_interrupted_extern(0/*dummy*/) ); }

            /** Closing the L2CAP socket, see specializations. */
            virtual bool close() noexcept = 0;

            /** Return this L2CAP socket descriptor. */
            inline int socket() const noexcept { return socket_; }

            virtual std::string getStateString() const noexcept = 0;

            virtual std::string toString() const noexcept = 0;
    };

    /**
     * L2CAP read/write communication channel to remote device
     */
    class L2CAPClient : public L2CAPComm {
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

        private:
            std::recursive_mutex mtx_write;
            BDAddressAndType remoteAddressAndType;
            std::atomic<bool> has_ioerror;  // reflects state
            std::atomic<pthread_t> tid_connect;
            std::atomic<pthread_t> tid_read;

        public:
            /**
             * Constructing a non connected L2CAP channel instance for the pre-defined PSM and CID.
             */
            L2CAPClient(const uint16_t adev_id, const BDAddressAndType& adapterAddressAndType, const L2CAP_PSM psm, const L2CAP_CID cid) noexcept;

            /**
             * Constructing a connected L2CAP channel instance for the pre-defined PSM and CID.
             */
            L2CAPClient(const uint16_t adev_id, const BDAddressAndType& adapterAddressAndType, const L2CAP_PSM psm, const L2CAP_CID cid,
                        const BDAddressAndType& remoteAddressAndType, int client_socket) noexcept;

            /** Destructor closing the L2CAP channel, see {@link #close()}. */
            ~L2CAPClient() noexcept { close(); }

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

            const BDAddressAndType& getRemoteAddressAndType() const noexcept { return remoteAddressAndType; }

            /** Closing the L2CAP channel, locking {@link #mutex_write()}. */
            bool close() noexcept override;

            bool hasIOError() const noexcept { return has_ioerror; }
            std::string getStateString() const noexcept override { return L2CAPComm::getStateString(is_open_, interrupted(), has_ioerror); }

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

            std::string toString() const noexcept override;
    };

    /**
     * L2CAP server socket to listen for connecting remote devices
     */
    class L2CAPServer : public L2CAPComm {
        private:
            std::atomic<pthread_t> tid_accept;

        public:
            L2CAPServer(const uint16_t adev_id, const BDAddressAndType& localAddressAndType, const L2CAP_PSM psm, const L2CAP_CID cid) noexcept;

            /** Destructor closing the L2CAP channel, see {@link #close()}. */
            ~L2CAPServer() noexcept { close(); }

            bool open() noexcept;

            bool close() noexcept override;

            std::unique_ptr<L2CAPClient> accept() noexcept;

            std::string getStateString() const noexcept override { return L2CAPComm::getStateString(is_open_, interrupted(), false /* has_ioerror */); }

            std::string toString() const noexcept override;
    };

} // namespace direct_bt

#endif /* L2CAP_COMM_HPP_ */
