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

#ifndef SMP_HANDLER_HPP_
#define SMP_HANDLER_HPP_

#include <cstring>
#include <string>
#include <cstdint>
#include <array>

#include <mutex>
#include <atomic>
#include <thread>

#include <jau/environment.hpp>
#include <jau/ringbuffer.hpp>
#include <jau/function_def.hpp>
#include <jau/cow_vector.hpp>

#include "UUID.hpp"
#include "BTTypes.hpp"
#include "L2CAPComm.hpp"
#include "SMPTypes.hpp"

/**
 * Linux/BlueZ prohibits access to the existing SMP implementation via L2CAP (socket).
 */
#ifdef __linux__
    #define SMP_SUPPORTED_BY_OS 0
    #define USE_LINUX_BT_SECURITY 1
#else
    #define SMP_SUPPORTED_BY_OS 1
    #define USE_LINUX_BT_SECURITY 0
#endif

/**
 * - - - - - - - - - - - - - - -
 *
 * SMPHandler.hpp Module for SMPHandler using SMPPDUMsg types
 *
 * - BT Core Spec v5.2: Vol 3, Part H Security Manager Specification (SM): 2 Security Manager (SM)
 * - BT Core Spec v5.2: Vol 3, Part H Security Manager Specification (SM): 3 Security Manager Protocol (SMP)
 *
 */
namespace direct_bt {

    /**
     * SMP Singleton runtime environment properties
     * <p>
     * Also see {@link DBTEnv::getExplodingProperties(const std::string & prefixDomain)}.
     * </p>
     */
    class SMPEnv : public jau::root_environment {
        private:
            SMPEnv() noexcept;

            const bool exploding; // just to trigger exploding properties

        public:
            /**
             * Timeout for SMP read command replies, defaults to 500ms.
             * <p>
             * Environment variable is 'direct_bt.smp.cmd.read.timeout'.
             * </p>
             */
            const int32_t SMP_READ_COMMAND_REPLY_TIMEOUT;

            /**
             * Timeout for SMP write command replies, defaults to 500ms.
             * <p>
             * Environment variable is 'direct_bt.smp.cmd.write.timeout'.
             * </p>
             */
            const int32_t SMP_WRITE_COMMAND_REPLY_TIMEOUT;

            /**
             * Medium ringbuffer capacity, defaults to 128 messages.
             * <p>
             * Environment variable is 'direct_bt.smp.ringsize'.
             * </p>
             */
            const int32_t SMPPDU_RING_CAPACITY;

            /**
             * Debug all SMP Data communication
             * <p>
             * Environment variable is 'direct_bt.debug.smp.data'.
             * </p>
             */
            const bool DEBUG_DATA;

        public:
            static SMPEnv& get() noexcept {
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
                static SMPEnv e;
                return e;
            }
    };


    typedef jau::FunctionDef<bool, std::shared_ptr<const SMPPDUMsg>> SMPSecurityReqCallback;
    typedef jau::cow_vector<SMPSecurityReqCallback> SMPSecurityReqCallbackList;

    /**
     * A thread safe SMP handler associated to one device via one L2CAP connection.
     * <p>
     * Implementation utilizes a lock free ringbuffer receiving data within its separate thread.
     * </p>
     * <p>
     * Controlling Environment variables, see {@link SMPEnv}.
     * </p>
     * See
     *
     * - BT Core Spec v5.2: Vol 3, Part H Security Manager Specification (SM): 2 Security Manager (SM)
     * - BT Core Spec v5.2: Vol 3, Part H Security Manager Specification (SM): 3 Security Manager Protocol (SMP)
     */
    class SMPHandler {
        public:
            /**
             * Linux/BlueZ prohibits access to the existing SMP implementation via L2CAP (socket).
             */
            static bool IS_SUPPORTED_BY_OS;

            enum class Defaults : int32_t {
                /* Vol 3 (Host), Part H (SM): 3 (SMP), 3.2 Security Manager Channel Over L2CAP */
                MIN_SMP_MTU = 23,

                /* Vol 3 (Host), Part H (SM): 3 (SMP), 3.2 Security Manager Channel Over L2CAP */
                LE_SECURE_SMP_MTU = 65,

                SMP_MTU_BUFFER_SZ = 128
            };
            static constexpr int number(const Defaults d) { return static_cast<int>(d); }

        private:
            const SMPEnv & env;

            /** GATTHandle's device weak back-reference */
            std::weak_ptr<DBTDevice> wbr_device;

            const std::string deviceString;
            std::recursive_mutex mtx_command;
            POctets rbuffer;

            L2CAPComm l2cap;
            jau::sc_atomic_bool is_connected; // reflects state
            jau::relaxed_atomic_bool has_ioerror;  // reflects state

            jau::ringbuffer<std::shared_ptr<const SMPPDUMsg>, nullptr, jau::nsize_t> smpPDURing;
            jau::sc_atomic_bool l2capReaderShallStop;

            std::mutex mtx_l2capReaderLifecycle;
            std::condition_variable cv_l2capReaderInit;
            pthread_t l2capReaderThreadId;
            jau::relaxed_atomic_bool l2capReaderRunning;

            SMPSecurityReqCallbackList smpSecurityReqCallbackList;

            uint16_t mtu;

            bool validateConnected() noexcept;

            void l2capReaderThreadImpl();

            void send(const SMPPDUMsg & msg);
            std::shared_ptr<const SMPPDUMsg> sendWithReply(const SMPPDUMsg & msg, const int timeout);

            void clearAllCallbacks() noexcept;

        public:
            SMPHandler(const std::shared_ptr<DBTDevice> & device) noexcept;

            SMPHandler(const SMPHandler&) = delete;
            void operator=(const SMPHandler&) = delete;

            /** Destructor closing this instance including L2CAP channel, see {@link #disconnect()}. */
            ~SMPHandler() noexcept;

            std::shared_ptr<DBTDevice> getDeviceUnchecked() const noexcept { return wbr_device.lock(); }
            std::shared_ptr<DBTDevice> getDeviceChecked() const;

            bool isConnected() const noexcept { return is_connected ; }
            bool hasIOError() const noexcept { return has_ioerror; }
            std::string getStateString() const noexcept { return L2CAPComm::getStateString(is_connected, has_ioerror); }

            /**
             * If sec_level > BT_SECURITY_LOW, establish security level per L2CAP connection.
             *
             * @param sec_level BT_SECURITY_LOW, BT_SECURITY_MEDIUM, BT_SECURITY_HIGH or BT_SECURITY_FIPS. sec_level <= BT_SECURITY_LOW leads to not set security level.
             * @return true if a security level > BT_SECURITY_LOW has been set successfully, false if no security level has been set or if it failed.
             */
            bool establishSecurity(const uint8_t sec_level);

            /**
             * Disconnect this GATTHandler and optionally the associated device
             * @param disconnectDevice if true, associated device will also be disconnected, otherwise not.
             * @param ioErrorCause if true, reason for disconnection is an IO error
             * @return true if successful, otherwise false
             */
            bool disconnect(const bool disconnectDevice, const bool ioErrorCause) noexcept;

            void addSMPSecurityReqCallback(const SMPSecurityReqCallback & l);
            int removeSMPSecurityReqCallback(const SMPSecurityReqCallback & l);
    };

} // namespace direct_bt

#endif /* SMP_HANDLER_HPP_ */

