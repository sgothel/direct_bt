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

#ifndef HCI_HANDLER_HPP_
#define HCI_HANDLER_HPP_

#include <cstring>
#include <string>
#include <cstdint>
#include <array>

#include <mutex>
#include <atomic>
#include <thread>

#include "DBTEnv.hpp"
#include "BTTypes.hpp"
#include "BTIoctl.hpp"
#include "OctetTypes.hpp"
#include "HCIComm.hpp"
#include "JavaUplink.hpp"
#include "HCITypes.hpp"
#include "MgmtTypes.hpp"
#include "LFRingbuffer.hpp"

/**
 * - - - - - - - - - - - - - - -
 *
 * Module HCIHandler:
 *
 * - BT Core Spec v5.2: Vol 4, Part E Host Controller Interface (HCI)
 */
namespace direct_bt {

    class HCIConnection {
        private:
            EUI48 address; // immutable
            BDAddressType addressType; // immutable
            uint16_t handle; // mutable

        public:
            HCIConnection(const EUI48 &address, const BDAddressType addressType, const uint16_t handle)
            : address(address), addressType(addressType), handle(handle) {}

            HCIConnection(const HCIConnection &o) = default;
            HCIConnection(HCIConnection &&o) = default;
            HCIConnection& operator=(const HCIConnection &o) = default;
            HCIConnection& operator=(HCIConnection &&o) = default;

            const EUI48 & getAddress() const { return address; }
            BDAddressType getAddressType() const { return addressType; }
            uint16_t getHandle() const { return handle; }

            void setHandle(uint16_t newHandle) { handle = newHandle; }

            bool equals(const EUI48 & otherAddress, const BDAddressType otherAddressType) const
            { return address == otherAddress && addressType == otherAddressType; }

            bool operator==(const HCIConnection& rhs) const
            { return address == rhs.address && addressType == rhs.addressType; }

            bool operator!=(const HCIConnection& rhs) const
            { return !(*this == rhs); }

            std::string toString() const {
                return "HCIConnection[handle "+uint16HexString(handle)+
                       ", address="+address.toString()+", addressType "+getBDAddressTypeString(addressType)+"]";
            }
    };
    typedef std::shared_ptr<HCIConnection> HCIConnectionRef;

    class HCIHandler; // forward

    /**
     * HCI Singleton runtime environment properties
     * <p>
     * Also see {@link DBTEnv::getExplodingProperties(const std::string & prefixDomain)}.
     * </p>
     */
    class HCIEnv : public DBTEnvrionment {
        friend class HCIHandler;

        private:
            HCIEnv() noexcept;

            const bool exploding; // just to trigger exploding properties

        public:
            /**
             * Poll timeout for HCI reader thread, defaults to 10s.
             * <p>
             * Environment variable is 'direct_bt.hci.reader.timeout'.
             * </p>
             */
            const int32_t HCI_READER_THREAD_POLL_TIMEOUT;

            /**
             * Timeout for HCI command status replies, excluding command complete, defaults to 3s.
             * <p>
             * Environment variable is 'direct_bt.hci.cmd.status.timeout'.
             * </p>
             */
            const int32_t HCI_COMMAND_STATUS_REPLY_TIMEOUT;

            /**
             * Timeout for HCI command complete replies, defaults to 10s.
             * This timeout is rather longer, as it may include waiting for pending command complete.
             * <p>
             * Environment variable is 'direct_bt.hci.cmd.complete.timeout'.
             * </p>
             */
            const int32_t HCI_COMMAND_COMPLETE_REPLY_TIMEOUT;

            /**
             * Small ringbuffer capacity for synchronized commands, defaults to 64 messages.
             * <p>
             * Environment variable is 'direct_bt.hci.ringsize'.
             * </p>
             */
            const int32_t HCI_EVT_RING_CAPACITY;

            /**
             * Debug all HCI event communication
             * <p>
             * Environment variable is 'direct_bt.debug.hci.event'.
             * </p>
             */
            const bool DEBUG_EVENT;

        private:
            /** Maximum number of packets to wait for until matching a sequential command. Won't block as timeout will limit. */
            const int32_t HCI_READ_PACKET_MAX_RETRY;

        public:
            static HCIEnv& get() noexcept {
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
                static HCIEnv e;
                return e;
            }
    };

    /**
     * A thread safe singleton handler of the HCI control channel to one controller (BT adapter)
     * <p>
     * Implementation utilizes a lock free ringbuffer receiving data within its separate thread.
     * </p>
     * <p>
     * Controlling Environment variables, see {@link HCIEnv}.
     * </p>
     */
    class HCIHandler {
        public:
            enum Defaults : int32_t {
                HCI_MAX_MTU = static_cast<uint8_t>(HCIConstU8::PACKET_MAX_SIZE)
            };

            static const pid_t pidSelf;

        private:
            static MgmtEvent::Opcode translate(HCIEventType evt, HCIMetaEventType met) noexcept;

            const HCIEnv & env;
            const BTMode btMode;
            const uint16_t dev_id;
            POctets rbuffer;
            HCIComm comm;
            std::recursive_mutex mtx;
            hci_ufilter filter_mask;
            std::atomic<uint32_t> metaev_filter_mask;
            std::atomic<uint64_t> opcbit_filter_mask;

            inline bool filter_test_metaev(HCIMetaEventType mec) noexcept { return 0 != test_bit_uint32(number(mec)-1, metaev_filter_mask); }
            inline void filter_put_metaevs(const uint32_t mask) noexcept { metaev_filter_mask=mask; }

            constexpr static void filter_clear_metaevs(uint32_t &mask) noexcept { mask=0; }
            constexpr static void filter_all_metaevs(uint32_t &mask) noexcept { mask=0xffffffffU; }
            inline static void filter_set_metaev(HCIMetaEventType mec, uint32_t &mask) noexcept { set_bit_uint32(number(mec)-1, mask); }

            inline bool filter_test_opcbit(HCIOpcodeBit opcbit) noexcept { return 0 != test_bit_uint64(number(opcbit), opcbit_filter_mask); }
            inline void filter_put_opcbit(const uint64_t mask) noexcept { opcbit_filter_mask=mask; }

            constexpr static void filter_clear_opcbit(uint64_t &mask) noexcept { mask=0; }
            constexpr static void filter_all_opcbit(uint64_t &mask) noexcept { mask=0xffffffffffffffffUL; }
            inline static void filter_set_opcbit(HCIOpcodeBit opcbit, uint64_t &mask) noexcept { set_bit_uint64(number(opcbit), mask); }

            LFRingbuffer<std::shared_ptr<HCIEvent>, nullptr> hciEventRing;
            std::atomic<pthread_t> hciReaderThreadId;
            std::atomic<bool> hciReaderRunning;
            std::atomic<bool> hciReaderShallStop;
            std::mutex mtx_hciReaderLifecycle;
            std::condition_variable cv_hciReaderInit;
            std::recursive_mutex mtx_sendReply; // for sendWith*Reply, process*Command, ..

            std::atomic<bool> allowClose;

            std::vector<HCIConnectionRef> connectionList;
            std::recursive_mutex mtx_connectionList;
            /**
             * Returns a newly added HCIConnectionRef tracker connection with given parameters, if not existing yet.
             * <p>
             * In case the HCIConnectionRef tracker connection already exists,
             * its handle will be updated (see below) and reference returned.
             * <p>
             * Overwrite existing tracked connection handle with given _valid_ handle only, i.e. non zero!
             * </p>
             * @param address key to matching connection
             * @param addrType key to matching connection
             * @param handle ignored for existing tracker _if_ invalid, i.e. zero.
             */
            HCIConnectionRef addOrUpdateTrackerConnection(const EUI48 & address, BDAddressType addrType, const uint16_t handle) noexcept;
            HCIConnectionRef findTrackerConnection(const EUI48 & address, BDAddressType addrType) noexcept;
            HCIConnectionRef findTrackerConnection(const uint16_t handle) noexcept;
            HCIConnectionRef removeTrackerConnection(const HCIConnectionRef conn) noexcept;
            HCIConnectionRef removeTrackerConnection(const uint16_t handle) noexcept;

            /** One MgmtAdapterEventCallbackList per event type, allowing multiple callbacks to be invoked for each event */
            std::array<MgmtEventCallbackList, static_cast<uint16_t>(MgmtEvent::Opcode::MGMT_EVENT_TYPE_COUNT)> mgmtEventCallbackLists;
            std::recursive_mutex mtx_callbackLists;
            inline bool isValidMgmtEventCallbackListsIndex(const MgmtEvent::Opcode opc) const noexcept {
                return static_cast<uint16_t>(opc) < mgmtEventCallbackLists.size();
            }

            std::shared_ptr<MgmtEvent> translate(std::shared_ptr<HCIEvent> ev) noexcept;

            void hciReaderThreadImpl() noexcept;

            bool sendCommand(HCICommand &req) noexcept;
            std::shared_ptr<HCIEvent> getNextReply(HCICommand &req, int32_t & retryCount, const int32_t replyTimeoutMS) noexcept;

            std::shared_ptr<HCIEvent> sendWithCmdCompleteReply(HCICommand &req, HCICommandCompleteEvent **res) noexcept;

            std::shared_ptr<HCIEvent> processCommandStatus(HCICommand &req, HCIStatusCode *status) noexcept;

            template<typename hci_cmd_event_struct>
            std::shared_ptr<HCIEvent> processCommandComplete(HCICommand &req,
                                                             const hci_cmd_event_struct **res, HCIStatusCode *status) noexcept;

            template<typename hci_cmd_event_struct>
            const hci_cmd_event_struct* getReplyStruct(std::shared_ptr<HCIEvent> event, HCIEventType evc, HCIStatusCode *status) noexcept;

            template<typename hci_cmd_event_struct>
            const hci_cmd_event_struct* getMetaReplyStruct(std::shared_ptr<HCIEvent> event, HCIMetaEventType mec, HCIStatusCode *status) noexcept;

            HCIHandler(const HCIHandler&) = delete;
            void operator=(const HCIHandler&) = delete;

        public:
            HCIHandler(const BTMode btMode, const uint16_t dev_id) noexcept;

            /**
             * Releases this instance after issuing {@link #close()}.
             */
            ~HCIHandler() noexcept { close(); }

            void close() noexcept;

            constexpr BTMode getBTMode() noexcept { return btMode; }

            /** Returns true if this mgmt instance is open and hence valid, otherwise false */
            bool isOpen() const noexcept {
                return comm.isOpen();
            }

            std::string toString() const noexcept { return "HCIHandler[BTMode "+getBTModeString(btMode)+", dev_id "+std::to_string(dev_id)+"]"; }

            /**
             * BT Core Spec v5.2: Vol 4, Part E HCI: 7.3.2 Reset command
             */
            HCIStatusCode reset() noexcept;

            /**
             * Sets LE scanning parameters.
             * <p>
             * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.10 LE Set Scan Parameters command
             * BT Core Spec v5.2: Vol 6 LE, Part B Link Layer: 4.4.3 Scanning State
             * </p>
             * Should not be called while scanning is active.
             * <p>
             * Scan parameters control advertising (AD) Protocol Data Unit (PDU) delivery behavior.
             * </p>
             *
             * @param le_scan_active true enables delivery of active scanning PDUs, otherwise no scanning PDUs shall be sent (default)
             * @param own_mac_type HCILEOwnAddressType::PUBLIC (default) or random/private.
             * @param le_scan_interval in units of 0.625ms, default value 24 for 15ms; Value range [4 .. 0x4000] for [2.5ms .. 10.24s]
             * @param le_scan_window in units of 0.625ms, default value 24 for 15ms; Value range [4 .. 0x4000] for [2.5ms .. 10.24s]. Shall be <= le_scan_interval
             * @param filter_policy 0x00 accepts all PDUs (default), 0x01 only of whitelisted, ...
             */
            HCIStatusCode le_set_scan_param(const bool le_scan_active=false,
                                            const HCILEOwnAddressType own_mac_type=HCILEOwnAddressType::PUBLIC,
                                            const uint16_t le_scan_interval=24, const uint16_t le_scan_window=24,
                                            const uint8_t filter_policy=0x00) noexcept;

            /**
             * Starts or stops LE scanning.
             * <p>
             * BT Core Spec v5.2: Vol 4, Part E HCI: 7.8.11 LE Set Scan Enable command
             * </p>
             * @param enable true to enable discovery, otherwise false
             * @param filter_dup true to filter out duplicate AD PDUs (default), otherwise all will be reported.
             */
            HCIStatusCode le_enable_scan(const bool enable, const bool filter_dup=true) noexcept;

            /**
             * Establish a connection to the given LE peer.
             * <p>
             * BT Core Spec v5.2: Vol 4, Part E HCI: 7.8.12 LE Create Connection command
             * </p>
             * <p>
             * Set window to the same value as the interval, enables continuous scanning.
             * </p>
             * <p>
             * The supervising timeout period is the time it takes before a devices gives up on the link if no packets are received.
             * Hence this parameter influences the responsiveness on a link loss.
             * A too small number may render the link too unstable, it should be at least 6 times of the connection interval.
             * <br>
             * To detect a link loss one can also send a regular ping to check whether the peripheral is still responding, see GATTHandler::ping().
             * </p>
             *
             * @param peer_bdaddr
             * @param peer_mac_type
             * @param own_mac_type
             * @param le_scan_interval in units of 0.625ms, default value 24 for 15ms; Value range [4 .. 0x4000] for [2.5ms .. 10.24s]
             * @param le_scan_window in units of 0.625ms, default value 24 for 15ms; Value range [4 .. 0x4000] for [2.5ms .. 10.24s]. Shall be <= le_scan_interval
             * @param conn_interval_min in units of 1.25ms, default value 12 for 15ms; Value range [6 .. 3200] for [7.5ms .. 4000ms]
             * @param conn_interval_max in units of 1.25ms, default value 12 for 15ms; Value range [6 .. 3200] for [7.5ms .. 4000ms]
             * @param conn_latency slave latency in units of connection events, default value 0; Value range [0 .. 0x01F3].
             * @param supervision_timeout in units of 10ms, default value >= 10 x conn_interval_max, we use HCIConstInt::LE_CONN_MIN_TIMEOUT_MS minimum; Value range [0xA-0x0C80] for [100ms - 32s].
             * @return
             */
            HCIStatusCode le_create_conn(const EUI48 &peer_bdaddr,
                                        const HCILEPeerAddressType peer_mac_type=HCILEPeerAddressType::PUBLIC,
                                        const HCILEOwnAddressType own_mac_type=HCILEOwnAddressType::PUBLIC,
                                        const uint16_t le_scan_interval=24, const uint16_t le_scan_window=24,
                                        const uint16_t conn_interval_min=12, const uint16_t conn_interval_max=12,
                                        const uint16_t conn_latency=0, const uint16_t supervision_timeout=getHCIConnSupervisorTimeout(0, 15)) noexcept;

            /**
             * Establish a connection to the given BREDR (non LE).
             * <p>
             * BT Core Spec v5.2: Vol 4, Part E HCI: 7.1.5 Create Connection command
             * </p>
             */
            HCIStatusCode create_conn(const EUI48 &bdaddr,
                                     const uint16_t pkt_type=HCI_DM1 | HCI_DM3 | HCI_DM5 | HCI_DH1 | HCI_DH3 | HCI_DH5,
                                     const uint16_t clock_offset=0x0000, const uint8_t role_switch=0x01) noexcept;

            /**
             * Disconnect an established connection.
             * <p>
             * BT Core Spec v5.2: Vol 4, Part E HCI: 7.1.6 Disconnect command
             * </p>
             */
            HCIStatusCode disconnect(const uint16_t conn_handle, const EUI48 &peer_bdaddr, const BDAddressType peer_mac_type,
                                     const HCIStatusCode reason=HCIStatusCode::REMOTE_USER_TERMINATED_CONNECTION) noexcept;

            /** MgmtEventCallback handling  */

            /**
             * Appends the given MgmtEventCallback to the named MgmtEvent::Opcode list,
             * if it is not present already (opcode + callback).
             * @param opc opcode index for callback list, the callback shall be added to
             * @param cb the to be added callback
             * @return true if newly added or already existing, false if given MgmtEvent::Opcode is out of supported range.
             */
            bool addMgmtEventCallback(const MgmtEvent::Opcode opc, const MgmtEventCallback &cb) noexcept;
            /** Returns count of removed given MgmtEventCallback from the named MgmtEvent::Opcode list. */
            int removeMgmtEventCallback(const MgmtEvent::Opcode opc, const MgmtEventCallback &cb) noexcept;
            /** Removes all MgmtEventCallbacks from the to the named MgmtEvent::Opcode list. */
            void clearMgmtEventCallbacks(const MgmtEvent::Opcode opc) noexcept;
            /** Removes all MgmtEventCallbacks from all MgmtEvent::Opcode lists. */
            void clearAllMgmtEventCallbacks() noexcept;

            /** Manually send a MgmtEvent to all of its listeners. */
            void sendMgmtEvent(std::shared_ptr<MgmtEvent> event) noexcept;

            /**
             * FIXME / TODO: Privacy Mode / Pairing / Bonding
             *
             * LE Secure Connections:
             * <pre>
             * BT Core Spec v5.2: Vol 1, Part A Architecture: 5.4 LE Security
             * BT Core Spec v5.2: Vol 3, Part C GAP: 10.2 LE SECURITY MODES
             * BT Core Spec v5.2: Vol 3, Part H SM: 2 Security Manager
             * BT Core Spec v5.2: Vol 3, Part H SM: 2.3.5 Pairing: 2.3.5.6 LE Secure Connections pairing phase 2
             * BT Core Spec v5.2: Vol 3, Part H SM: 2.3.5 Pairing: 2.3.5.6.3 LE Authentication stage 1 – Passkey Entry
             * BT Core Spec v5.2: Vol 3, Part H SM: 3 Security Manager Protocol (SMP) fixed channel over L2CAP
             *
             *
             * LE Legacy Pairing** similar to BREDR like: Secure Simple Pairing (SSP)
             * LE Secure Connections functional equivalent to SSP, using 128 bit Long Term Key (LTK)
             * </pre>
             *
             * <p>
             * BT Core Spec v5.2: Vol 1, Part A Architecture: 5 Security architecture
             * BT Core Spec v5.2: Vol 1, Part A Architecture: 5.4 LE Security
             * BT Core Spec v5.2: Vol 1, Part A Architecture: 5.4.5 LE Privacy feature
             * - device privacy mode (mixed mode, also accept other peer address)
             * - network privacy mode (only private address - default!)
             * -> add device to resolving list, implying being added to device white list!
             *
             * BT Core Spec v5.2: Vol 3, Part C GAP: 10.2 LE SECURITY MODES
             *
             * BT Core Spec v5.2: Vol 4, Part E HCI: 7.8.77 LE Set Privacy Mode command
             * BT Core Spec v5.2: Vol 6 LE Adapter, Part B Link Layer Spec: 4.7 Resolving List
             * </p>
             * FIXME: TODO
             *
             * @return
             * HCIStatusCode le_set_privacy_mode();
             *
             * Fills buffer with random bytes, usually 16 bytes for 128 bit key.
             * FIXME: TODO
             * static bool generateIRK(uint8_t *buffer, int size);
             */

    };

} // namespace direct_bt

#endif /* DBT_HANDLER_HPP_ */

