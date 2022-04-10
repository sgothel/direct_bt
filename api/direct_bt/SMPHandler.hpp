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

#include <mutex>
#include <atomic>
#include <thread>

#include <jau/environment.hpp>
#include <jau/ringbuffer.hpp>
#include <jau/function_def.hpp>
#include <jau/darray.hpp>
#include <jau/cow_darray.hpp>
#include <jau/uuid.hpp>

#include "BTTypes0.hpp"
#include "L2CAPComm.hpp"
#include "SMPTypes.hpp"

/**
 * Linux/BlueZ prohibits access to the existing SMP implementation via L2CAP (socket).
 */
#ifdef __linux__
    inline constexpr const bool SMP_SUPPORTED_BY_OS = false;
    inline constexpr const bool USE_LINUX_BT_SECURITY = true;
    inline constexpr const bool CONSIDER_HCI_CMD_FOR_SMP_STATE = false;
    inline constexpr const bool SCAN_DISABLED_POST_CONNECT = true;
#else
    inline constexpr const bool SMP_SUPPORTED_BY_OS = true;
    inline constexpr const bool USE_LINUX_BT_SECURITY = false;
    inline constexpr const bool CONSIDER_HCI_CMD_FOR_SMP_STATE = true;
    inline constexpr const bool SCAN_DISABLED_POST_CONNECT = false;
#endif

/**
 * - - - - - - - - - - - - - - -
 *
 * SMPHandler.hpp Module for SMPHandler using SMPPDUMsg types
 *
 * - BT Core Spec v5.2: Vol 3, Part H Security Manager Specification (SM): 2 Security Manager (SM)
 * - BT Core Spec v5.2: Vol 3, Part H Security Manager Specification (SM): 3 Security Manager Protocol (SMP)
 *
 * Overall bookmark regarding BT Security
 *
 * - BT Core Spec v5.2: Vol 1, Part A Architecture: 5 Security architecture
 * - BT Core Spec v5.2: Vol 1, Part A Architecture: 5.4 LE Security
 * - BT Core Spec v5.2: Vol 1, Part A Architecture: 5.4.5 LE Privacy feature
 *   - device privacy mode (mixed mode, also accept other peer address)
 *   - network privacy mode (only private address - default!)
 *   - add device to resolving list, implying being added to device white list!
 *
 * - BT Core Spec v5.2: Vol 3, Part C GAP: 10.2 LE SECURITY MODES
 *
 * - BT Core Spec v5.2: Vol 3, Part H Security Manager Specification (SM): 2 Security Manager (SM)
 *   - 2.3.5 Pairing: 2.3.5.6 LE Secure Connections pairing phase 2
 *   - 2.3.5 Pairing: 2.3.5.6.3 LE Authentication stage 1 – Passkey Entry
 * - BT Core Spec v5.2: Vol 3, Part H Security Manager Specification (SM): 3 Security Manager Protocol (SMP)
 *   - fixed channel over L2CAP
 *
 * - BT Core Spec v5.2: Vol 4, Part E HCI: 7.8.77 LE Set Privacy Mode command
 *
 * - BT Core Spec v5.2: Vol 6 LE Adapter, Part B Link Layer Spec: 4.7 Resolving List
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


    typedef jau::FunctionDef<bool, const SMPPDUMsg&> SMPSecurityReqCallback;
    typedef jau::cow_darray<SMPSecurityReqCallback> SMPSecurityReqCallbackList;

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
            std::weak_ptr<BTDevice> wbr_device;

            const std::string deviceString;
            std::recursive_mutex mtx_command;
            jau::POctets rbuffer;

            L2CAPClient l2cap;
            jau::sc_atomic_bool is_connected; // reflects state
            jau::relaxed_atomic_bool has_ioerror;  // reflects state

            jau::ringbuffer<std::unique_ptr<const SMPPDUMsg>, jau::nsize_t> smpPDURing;
            jau::sc_atomic_bool l2capReaderShallStop;

            std::mutex mtx_l2capReaderLifecycle;
            std::condition_variable cv_l2capReaderInit;
            pthread_t l2capReaderThreadId;
            jau::sc_atomic_bool l2capReaderRunning;

            SMPSecurityReqCallbackList smpSecurityReqCallbackList;

            uint16_t mtu;

            bool validateConnected() noexcept;

            void l2capReaderThreadImpl();

            void send(const SMPPDUMsg & msg);
            std::unique_ptr<const SMPPDUMsg> sendWithReply(const SMPPDUMsg & msg, const int timeout);

            void clearAllCallbacks() noexcept;

        public:
            SMPHandler(const std::shared_ptr<BTDevice> & device) noexcept;

            SMPHandler(const SMPHandler&) = delete;
            void operator=(const SMPHandler&) = delete;

            /** Destructor closing this instance including L2CAP channel, see {@link #disconnect()}. */
            ~SMPHandler() noexcept;

            std::shared_ptr<BTDevice> getDeviceUnchecked() const noexcept { return wbr_device.lock(); }
            std::shared_ptr<BTDevice> getDeviceChecked() const;

            bool isConnected() const noexcept { return is_connected ; }
            bool hasIOError() const noexcept { return has_ioerror; }
            std::string getStateString() const noexcept { return L2CAPComm::getStateString(is_connected, has_ioerror); }

            /**
             * If sec_level > ::BTSecurityLevel::UNSET, change security level per L2CAP connection.
             *
             * @param sec_level sec_level < ::BTSecurityLevel::NONE will not set security level and returns false.
             * @return true if a security level > ::BTSecurityLevel::UNSET has been set successfully, false if no security level has been set or if it failed.
             */
            bool establishSecurity(const BTSecurityLevel sec_level);

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

