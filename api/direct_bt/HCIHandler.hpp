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

#include <jau/environment.hpp>
#include <jau/ringbuffer.hpp>
#include <jau/java_uplink.hpp>

#include "BTTypes.hpp"
#include "BTIoctl.hpp"
#include "OctetTypes.hpp"
#include "HCIComm.hpp"
#include "HCITypes.hpp"
#include "MgmtTypes.hpp"

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
            HCIConnection(const EUI48 &address_, const BDAddressType addressType_, const uint16_t handle_)
            : address(address_), addressType(addressType_), handle(handle_) {}

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
                return "HCIConnection[handle "+jau::uint16HexString(handle)+
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
    class HCIEnv : public jau::root_environment {
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
             * Poll period for certain HCI commands actively waiting for clearance, defaults to 125ms.
             * <p>
             * Used for LE_Create_Connection or Create_Connection
             * when waiting for any pending connection commands or the addressed device's disconnect command to been completed
             * up to HCI_COMMAND_COMPLETE_REPLY_TIMEOUT.
             * </p>
             * <p>
             * Environment variable is 'direct_bt.hci.cmd.complete.timeout'.
             * </p>
             */
            const int32_t HCI_COMMAND_POLL_PERIOD;

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
            enum DefaultsSizeT : jau::nsize_t {
                HCI_MAX_MTU = static_cast<jau::nsize_t>(HCIConstSizeT::PACKET_MAX_SIZE)
            };

            static const pid_t pidSelf;

        private:
            static MgmtEvent::Opcode translate(HCIEventType evt, HCIMetaEventType met) noexcept;

            const HCIEnv & env;
            const uint16_t dev_id;
            POctets rbuffer;
            HCIComm comm;
            hci_ufilter filter_mask;
            std::atomic<uint32_t> metaev_filter_mask;
            std::atomic<uint64_t> opcbit_filter_mask;

            inline bool filter_test_metaev(HCIMetaEventType mec) noexcept { return 0 != jau::test_bit_uint32(number(mec)-1, metaev_filter_mask); }
            inline void filter_put_metaevs(const uint32_t mask) noexcept { metaev_filter_mask=mask; }

            constexpr static void filter_clear_metaevs(uint32_t &mask) noexcept { mask=0; }
            constexpr static void filter_all_metaevs(uint32_t &mask) noexcept { mask=0xffffffffU; }
            inline static void filter_set_metaev(HCIMetaEventType mec, uint32_t &mask) noexcept { jau::set_bit_uint32(number(mec)-1, mask); }

            inline bool filter_test_opcbit(HCIOpcodeBit opcbit) noexcept { return 0 != jau::test_bit_uint64(number(opcbit), opcbit_filter_mask); }
            inline void filter_put_opcbit(const uint64_t mask) noexcept { opcbit_filter_mask=mask; }

            constexpr static void filter_clear_opcbit(uint64_t &mask) noexcept { mask=0; }
            constexpr static void filter_all_opcbit(uint64_t &mask) noexcept { mask=0xffffffffffffffffUL; }
            inline static void filter_set_opcbit(HCIOpcodeBit opcbit, uint64_t &mask) noexcept { jau::set_bit_uint64(number(opcbit), mask); }

            jau::ringbuffer<std::shared_ptr<HCIEvent>, nullptr, jau::nsize_t> hciEventRing;
            jau::sc_atomic_bool hciReaderShallStop;

            std::mutex mtx_hciReaderLifecycle;
            std::condition_variable cv_hciReaderInit;
            pthread_t hciReaderThreadId;
            jau::relaxed_atomic_bool hciReaderRunning;

            std::recursive_mutex mtx_sendReply; // for sendWith*Reply, process*Command, ..; Recurses from many..

            jau::sc_atomic_bool allowClose;
            std::atomic<BTMode> btMode;

            std::atomic<ScanType> currentScanType;

            std::vector<HCIConnectionRef> connectionList;
            std::vector<HCIConnectionRef> disconnectCmdList;
            std::recursive_mutex mtx_connectionList; // Recurses from disconnect -> findTrackerConnection, addOrUpdateTrackerConnection

            /** Exclusive [le] connection command (status + pending completed) one at a time */
            std::mutex mtx_connect_cmd;

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
            HCIConnectionRef addOrUpdateHCIConnection(std::vector<HCIConnectionRef> &list,
                                                      const EUI48 & address, BDAddressType addrType, const uint16_t handle) noexcept;
            HCIConnectionRef addOrUpdateTrackerConnection(const EUI48 & address, BDAddressType addrType, const uint16_t handle) noexcept {
                return addOrUpdateHCIConnection(connectionList, address, addrType, handle);
            }
            HCIConnectionRef addOrUpdateDisconnectCmd(const EUI48 & address, BDAddressType addrType, const uint16_t handle) noexcept {
                return addOrUpdateHCIConnection(disconnectCmdList, address, addrType, handle);
            }

            HCIConnectionRef findHCIConnection(std::vector<HCIConnectionRef> &list, const EUI48 & address, BDAddressType addrType) noexcept;
            HCIConnectionRef findTrackerConnection(const EUI48 & address, BDAddressType addrType) noexcept {
                return findHCIConnection(connectionList, address, addrType);
            }
            HCIConnectionRef findDisconnectCmd(const EUI48 & address, BDAddressType addrType) noexcept {
                return findHCIConnection(disconnectCmdList, address, addrType);
            }

            HCIConnectionRef findTrackerConnection(const uint16_t handle) noexcept;
            HCIConnectionRef removeTrackerConnection(const HCIConnectionRef conn) noexcept;
            int countPendingTrackerConnections() noexcept;

            HCIConnectionRef removeHCIConnection(std::vector<HCIConnectionRef> &list, const uint16_t handle) noexcept;
            HCIConnectionRef removeTrackerConnection(const uint16_t handle) noexcept {
                return removeHCIConnection(connectionList, handle);
            }
            HCIConnectionRef removeDisconnectCmd(const uint16_t handle) noexcept {
                return removeHCIConnection(disconnectCmdList, handle);
            }

            void clearConnectionLists() noexcept;

            /** One MgmtAdapterEventCallbackList per event type, allowing multiple callbacks to be invoked for each event */
            std::array<MgmtEventCallbackList, static_cast<uint16_t>(MgmtEvent::Opcode::MGMT_EVENT_TYPE_COUNT)> mgmtEventCallbackLists;
            inline bool isValidMgmtEventCallbackListsIndex(const MgmtEvent::Opcode opc) const noexcept {
                return static_cast<uint16_t>(opc) < mgmtEventCallbackLists.size();
            }

            std::shared_ptr<MgmtEvent> translate(std::shared_ptr<HCIEvent> ev) noexcept;

            void hciReaderThreadImpl() noexcept;

            bool sendCommand(HCICommand &req) noexcept;
            std::shared_ptr<HCIEvent> getNextReply(HCICommand &req, int32_t & retryCount, const int32_t replyTimeoutMS) noexcept;
            std::shared_ptr<HCIEvent> getNextCmdCompleteReply(HCICommand &req, HCICommandCompleteEvent **res) noexcept;

            std::shared_ptr<HCIEvent> processCommandStatus(HCICommand &req, HCIStatusCode *status) noexcept;

            template<typename hci_cmd_event_struct>
            std::shared_ptr<HCIEvent> processCommandComplete(HCICommand &req,
                                                             const hci_cmd_event_struct **res, HCIStatusCode *status) noexcept;
            template<typename hci_cmd_event_struct>
            std::shared_ptr<HCIEvent> receiveCommandComplete(HCICommand &req,
                                                             const hci_cmd_event_struct **res, HCIStatusCode *status) noexcept;

            template<typename hci_cmd_event_struct>
            const hci_cmd_event_struct* getReplyStruct(std::shared_ptr<HCIEvent> event, HCIEventType evc, HCIStatusCode *status) noexcept;

            template<typename hci_cmd_event_struct>
            const hci_cmd_event_struct* getMetaReplyStruct(std::shared_ptr<HCIEvent> event, HCIMetaEventType mec, HCIStatusCode *status) noexcept;

            HCIHandler(const HCIHandler&) = delete;
            void operator=(const HCIHandler&) = delete;

        public:
            HCIHandler(const uint16_t dev_id, const BTMode btMode=BTMode::NONE) noexcept;

            /**
             * Releases this instance after issuing {@link #close()}.
             */
            ~HCIHandler() noexcept { close(); }

            void close() noexcept;

            inline BTMode getBTMode() const noexcept { return btMode; }

            inline void setBTMode(const BTMode mode) noexcept { btMode = mode; }

            /** Returns true if this mgmt instance is open, connected and hence valid, otherwise false */
            bool isOpen() const noexcept {
                return true == allowClose.load() && comm.isOpen();
            }

            ScanType getCurrentScanType() const noexcept { return currentScanType.load(); }
            void setCurrentScanType(const ScanType v) noexcept { currentScanType = v; }

            std::string toString() const noexcept;

            /**
             * Bring up this adapter into a POWERED functional state.
             */
            HCIStatusCode startAdapter();

            /**
             * Bring down this adapter into a non-POWERED non-functional state.
             * <p>
             * All allocated resources should be freed and the internal state being reset
             * in compliance to
             * <pre>
             * BT Core Spec v5.2: Vol 4, Part E HCI: 7.3.2 Reset command
             * </pre>
             * </p>
             */
            HCIStatusCode stopAdapter();

            /**
             * Reset the adapter.
             * <p>
             * The semantics are specific to the HCI host implementation,
             * however, it shall comply at least with the HCI Reset command
             * and bring up the device from standby into a POWERED functional state afterwards.
             * </p>
             * <pre>
             * BT Core Spec v5.2: Vol 4, Part E HCI: 7.3.2 Reset command
             * </pre>
             */
            HCIStatusCode resetAdapter();

            /**
             * BT Core Spec v5.2: Vol 4, Part E HCI: 7.3.2 Reset command
             */
            HCIStatusCode reset() noexcept;

            HCIStatusCode getLocalVersion(HCILocalVersion &version) noexcept;

            /**
             * Sets LE scanning parameters.
             * <p>
             * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.10 LE Set Scan Parameters command
             * BT Core Spec v5.2: Vol 6 LE, Part B Link Layer: 4.4.3 Scanning State
             * </p>
             * <p>
             * Scan parameters control advertising (AD) Protocol Data Unit (PDU) delivery behavior.
             * </p>
             * <p>
             * Should not be called while LE scanning is active, otherwise HCIStatusCode::COMMAND_DISALLOWED will be returned.
             * </p>
             *
             * @param own_mac_type HCILEOwnAddressType::PUBLIC (default) or random/private.
             * @param le_scan_interval in units of 0.625ms, default value 24 for 15ms; Value range [4 .. 0x4000] for [2.5ms .. 10.24s]
             * @param le_scan_window in units of 0.625ms, default value 24 for 15ms; Value range [4 .. 0x4000] for [2.5ms .. 10.24s]. Shall be <= le_scan_interval
             * @param filter_policy 0x00 accepts all PDUs (default), 0x01 only of whitelisted, ...
             * @param le_scan_active true enables delivery of active scanning PDUs, otherwise no scanning PDUs shall be sent (default)
             */
            HCIStatusCode le_set_scan_param(const HCILEOwnAddressType own_mac_type=HCILEOwnAddressType::PUBLIC,
                                            const uint16_t le_scan_interval=24, const uint16_t le_scan_window=24,
                                            const uint8_t filter_policy=0x00, const bool le_scan_active=false) noexcept;

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
             * Start LE scanning, i.e. performs le_set_scan_param() and le_enable_scan() in one atomic operation.
             * <p>
             * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.10 LE Set Scan Parameters command
             * BT Core Spec v5.2: Vol 4, Part E HCI: 7.8.11 LE Set Scan Enable command
             * </p>
             * <p>
             * Scan parameters control advertising (AD) Protocol Data Unit (PDU) delivery behavior.
             * </p>
             * <p>
             * Should not be called while LE scanning is active, otherwise HCIStatusCode::COMMAND_DISALLOWED will be returned.
             * </p>
             * <p>
             * Method will report errors.
             * </p>
             *
             * @param filter_dup true to filter out duplicate AD PDUs (default), otherwise all will be reported.
             * @param own_mac_type HCILEOwnAddressType::PUBLIC (default) or random/private.
             * @param le_scan_interval in units of 0.625ms, default value 24 for 15ms; Value range [4 .. 0x4000] for [2.5ms .. 10.24s]
             * @param le_scan_window in units of 0.625ms, default value 24 for 15ms; Value range [4 .. 0x4000] for [2.5ms .. 10.24s]. Shall be <= le_scan_interval
             * @param filter_policy 0x00 accepts all PDUs (default), 0x01 only of whitelisted, ...
             * @param le_scan_active true enables delivery of active scanning PDUs, otherwise no scanning PDUs shall be sent (default)
             */
            HCIStatusCode le_start_scan(const bool filter_dup=true,
                                        const HCILEOwnAddressType own_mac_type=HCILEOwnAddressType::PUBLIC,
                                        const uint16_t le_scan_interval=24, const uint16_t le_scan_window=24,
                                        const uint8_t filter_policy=0x00, const bool le_scan_active=false) noexcept;

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
             * <p>
             * Implementation tries to mitigate HCIStatusCode::COMMAND_DISALLOWED failure due to any pending connection commands,
             * waiting actively up to HCIEnv::HCI_COMMAND_COMPLETE_REPLY_TIMEOUT, testing every HCIEnv::HCI_COMMAND_POLL_PERIOD if resolved.<br>
             * <br>
             * In case of no resolution, i.e. another HCI_LE_Create_Connection command is pending,
             * HCIStatusCode::COMMAND_DISALLOWED will be returned by the underlying HCI host implementation.
             * </p>
             * <p>
             * Implementation tries to mitigate HCIStatusCode::CONNECTION_ALREADY_EXISTS failure due to a specific pending disconnect command,
             * waiting actively up to HCIEnv::HCI_COMMAND_COMPLETE_REPLY_TIMEOUT, testing every HCIEnv::HCI_COMMAND_POLL_PERIOD if resolved.<br>
             * <br>
             * In case of no resolution, i.e. the connection persists,
             * HCIStatusCode::CONNECTION_ALREADY_EXISTS will be returned by the underlying HCI host implementation.
             * </p>
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
             * <p>
             * Implementation tries to mitigate HCIStatusCode::COMMAND_DISALLOWED failure due to any pending connection commands,
             * waiting actively up to HCIEnv::HCI_COMMAND_COMPLETE_REPLY_TIMEOUT, testing every HCIEnv::HCI_COMMAND_POLL_PERIOD if resolved.<br>
             * <br>
             * In case of no resolution, i.e. another HCI_Create_Connection command is pending,
             * HCIStatusCode::COMMAND_DISALLOWED will be returned by the underlying HCI host implementation.
             * </p>
             * <p>
             * Implementation tries to mitigate HCIStatusCode::CONNECTION_ALREADY_EXISTS failure due to a specific pending disconnect command,
             * waiting actively up to HCIEnv::HCI_COMMAND_COMPLETE_REPLY_TIMEOUT, testing every HCIEnv::HCI_COMMAND_POLL_PERIOD if resolved.<br>
             * <br>
             * In case of no resolution, i.e. the connection persists,
             * HCIStatusCode::CONNECTION_ALREADY_EXISTS will be returned by the underlying HCI host implementation.
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

