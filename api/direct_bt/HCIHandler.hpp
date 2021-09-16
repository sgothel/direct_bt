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

#include <mutex>
#include <atomic>
#include <thread>

#include <jau/darray.hpp>
#include <jau/environment.hpp>
#include <jau/ringbuffer.hpp>
#include <jau/java_uplink.hpp>

#include "BTTypes0.hpp"
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

            /**
             * Debug all scanned HCI 'Advertising Data' (AD) 'Extended Inquiry Response' (EIR) packages.
             * <p>
             * Environment variable is 'direct_bt.debug.hci.scan_ad_eir'.
             * </p>
             */
            const bool DEBUG_SCAN_AD_EIR;

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

    typedef jau::FunctionDef<bool, const BDAddressAndType& /* addressAndType */,
                                   const SMPPDUMsg&, const HCIACLData::l2cap_frame& /* source */> HCISMPMsgCallback;
    typedef jau::cow_darray<HCISMPMsgCallback> HCISMPMsgCallbackList;

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

            const HCIEnv & env;

        private:
            class HCIConnection {
                private:
                    BDAddressAndType addressAndType; // immutable
                    uint16_t handle; // mutable

                public:
                    HCIConnection(const BDAddressAndType& addressAndType_, const uint16_t handle_)
                    : addressAndType(addressAndType_), handle(handle_) {}

                    HCIConnection(const HCIConnection &o) = default;
                    HCIConnection(HCIConnection &&o) = default;
                    HCIConnection& operator=(const HCIConnection &o) = default;
                    HCIConnection& operator=(HCIConnection &&o) = default;

                    const BDAddressAndType & getAddressAndType() const { return addressAndType; }
                    uint16_t getHandle() const { return handle; }

                    void setHandle(uint16_t newHandle) { handle = newHandle; }

                    bool equals(const BDAddressAndType & other) const
                    { return addressAndType == other; }

                    bool operator==(const HCIConnection& rhs) const {
                        if( this == &rhs ) {
                            return true;
                        }
                        return addressAndType == rhs.addressAndType;
                    }

                    bool operator!=(const HCIConnection& rhs) const
                    { return !(*this == rhs); }

                    std::size_t hash_code() const noexcept {
                        return addressAndType.hash_code();
                    }

                    std::string toString() const {
                        return "HCIConnection[handle "+jau::to_hexstring(handle)+
                               ", address "+addressAndType.toString()+"]";
                    }
            };
            typedef std::shared_ptr<HCIConnection> HCIConnectionRef;

            static MgmtEvent::Opcode translate(HCIEventType evt, HCIMetaEventType met) noexcept;

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

            jau::ringbuffer<std::unique_ptr<HCIEvent>, std::nullptr_t, jau::nsize_t> hciEventRing;
            jau::sc_atomic_bool hciReaderShallStop;

            std::mutex mtx_hciReaderLifecycle;
            std::condition_variable cv_hciReaderInit;
            pthread_t hciReaderThreadId;
            jau::relaxed_atomic_bool hciReaderRunning;

            std::recursive_mutex mtx_sendReply; // for sendWith*Reply, process*Command, ..; Recurses from many..

            LE_Features le_ll_feats;
            /**
             * Cached bitfield of Local Supported Commands, 64 octets
             * <pre>
             * BT Core Spec v5.2: Vol 4, Part E, 6.27 (HCI) Supported Commands
             * BT Core Spec v5.2: Vol 4, Part E, 7.4.2 Read Local Supported Commands command
             * </pre>
             */
            uint8_t sup_commands[64];
            jau::relaxed_atomic_bool sup_commands_set;

            jau::sc_atomic_bool allowClose;
            std::atomic<BTMode> btMode;

            std::atomic<ScanType> currentScanType;
            jau::sc_atomic_bool advertisingEnabled;

            jau::darray<HCIConnectionRef> connectionList;
            jau::darray<HCIConnectionRef> disconnectCmdList;
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
            HCIConnectionRef addOrUpdateHCIConnection(jau::darray<HCIConnectionRef> &list,
                                                      const BDAddressAndType& addressAndType, const uint16_t handle) noexcept;
            HCIConnectionRef addOrUpdateTrackerConnection(const BDAddressAndType& addressAndType, const uint16_t handle) noexcept {
                return addOrUpdateHCIConnection(connectionList, addressAndType, handle);
            }
            HCIConnectionRef addOrUpdateDisconnectCmd(const BDAddressAndType& addressAndType, const uint16_t handle) noexcept {
                return addOrUpdateHCIConnection(disconnectCmdList, addressAndType, handle);
            }

            HCIConnectionRef findHCIConnection(jau::darray<HCIConnectionRef> &list, const BDAddressAndType& addressAndType) noexcept;
            HCIConnectionRef findTrackerConnection(const BDAddressAndType& addressAndType) noexcept {
                return findHCIConnection(connectionList, addressAndType);
            }
            HCIConnectionRef findDisconnectCmd(const BDAddressAndType& addressAndType) noexcept {
                return findHCIConnection(disconnectCmdList, addressAndType);
            }

            HCIConnectionRef findTrackerConnection(const uint16_t handle) noexcept;
            HCIConnectionRef removeTrackerConnection(const HCIConnectionRef conn) noexcept;
            int countPendingTrackerConnections() noexcept;
            int getTrackerConnectionCount() noexcept;

            HCIConnectionRef removeHCIConnection(jau::darray<HCIConnectionRef> &list, const uint16_t handle) noexcept;
            HCIConnectionRef removeTrackerConnection(const uint16_t handle) noexcept {
                return removeHCIConnection(connectionList, handle);
            }
            HCIConnectionRef removeDisconnectCmd(const uint16_t handle) noexcept {
                return removeHCIConnection(disconnectCmdList, handle);
            }

            /** One MgmtAdapterEventCallbackList per event type, allowing multiple callbacks to be invoked for each event */
            std::array<MgmtEventCallbackList, static_cast<uint16_t>(MgmtEvent::Opcode::MGMT_EVENT_TYPE_COUNT)> mgmtEventCallbackLists;
            inline bool isValidMgmtEventCallbackListsIndex(const MgmtEvent::Opcode opc) const noexcept {
                return static_cast<uint16_t>(opc) < mgmtEventCallbackLists.size();
            }

            HCISMPMsgCallbackList hciSMPMsgCallbackList;

            std::unique_ptr<MgmtEvent> translate(HCIEvent& ev) noexcept;

            std::unique_ptr<const SMPPDUMsg> getSMPPDUMsg(const HCIACLData::l2cap_frame & l2cap, const uint8_t * l2cap_data) const noexcept;
            void hciReaderThreadImpl() noexcept;

            bool sendCommand(HCICommand &req, const bool quiet=false) noexcept;
            std::unique_ptr<HCIEvent> getNextReply(HCICommand &req, int32_t & retryCount, const int32_t replyTimeoutMS) noexcept;
            std::unique_ptr<HCIEvent> getNextCmdCompleteReply(HCICommand &req, HCICommandCompleteEvent **res) noexcept;

            std::unique_ptr<HCIEvent> processCommandStatus(HCICommand &req, HCIStatusCode *status, const bool quiet=false) noexcept;

            template<typename hci_cmd_event_struct>
            std::unique_ptr<HCIEvent> processCommandComplete(HCICommand &req,
                                                             const hci_cmd_event_struct **res, HCIStatusCode *status,
                                                             const bool quiet=false) noexcept;
            template<typename hci_cmd_event_struct>
            std::unique_ptr<HCIEvent> receiveCommandComplete(HCICommand &req,
                                                             const hci_cmd_event_struct **res, HCIStatusCode *status,
                                                             const bool quiet=false) noexcept;

            template<typename hci_cmd_event_struct>
            const hci_cmd_event_struct* getReplyStruct(HCIEvent& event, HCIEventType evc, HCIStatusCode *status) noexcept;

            template<typename hci_cmd_event_struct>
            const hci_cmd_event_struct* getMetaReplyStruct(HCIEvent& event, HCIMetaEventType mec, HCIStatusCode *status) noexcept;

        public:
            HCIHandler(const uint16_t dev_id, const BTMode btMode=BTMode::NONE) noexcept;

        private:
            void zeroSupCommands() noexcept;
            bool initSupCommands() noexcept;

        public:
            /**
             * Reset all internal states, i.e. connection and disconnect lists.
             * <p>
             * Must be explicitly called with `powered_on=true` when adapter is powered on!
             * </p>
             * @param powered_on indicates whether the adapter is powered on or not
             * @see initSupCommands()
             */
            bool resetAllStates(const bool powered_on) noexcept;

            HCIHandler(const HCIHandler&) = delete;
            void operator=(const HCIHandler&) = delete;

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

            /** Use extended scanning if HCI_LE_Set_Extended_Scan_Parameters and HCI_LE_Set_Extended_Scan_Enable is supported (Bluetooth 5.0). */
            bool use_ext_scan() const noexcept {
                return 0 != ( sup_commands[37] & ( 1 << 5 ) ) &&
                       0 != ( sup_commands[37] & ( 1 << 6 ) );
            }

            /** Use extended connection if HCI_LE_Extended_Create_Connection is supported (Bluetooth 5.0). */
            bool use_ext_conn() const noexcept {
                return 0 != ( sup_commands[37] & ( 1 << 7 ) );
            }

            /** Use extended advertising if LE_Features::LE_Ext_Adv is set (Bluetooth 5.0). */
            bool use_ext_adv() const noexcept {
                return isLEFeaturesBitSet(le_ll_feats, LE_Features::LE_Ext_Adv);
            }

            ScanType getCurrentScanType() const noexcept { return currentScanType.load(); }
            void setCurrentScanType(const ScanType v) noexcept { currentScanType = v; }

            bool isAdvertising() const noexcept { return advertisingEnabled.load(); }

            std::string toString() const noexcept;

        private:
            /**
             * Bring up this adapter into a POWERED functional state.
             * <p>
             * Currently used in resetAdapter() only.
             * </p>
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
             * <p>
             * Currently used in resetAdapter() only.
             * </p>
             */
            HCIStatusCode stopAdapter();

        public:
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
             * HCI Reset Command
             * <pre>
             * BT Core Spec v5.2: Vol 4, Part E HCI: 7.3.2 Reset command
             * </pre>
             */
            HCIStatusCode reset() noexcept;

            HCIStatusCode getLocalVersion(HCILocalVersion &version) noexcept;

            /**
             * Return previously fetched LE_Features for the controller via initSupCommands() via resetAllStates()
             * <pre>
             * BT Core Spec v5.2: Vol 6, Part B, 4.6 (LE LL) Feature Support
             *
             * BT Core Spec v5.2: Vol 4, Part E, 7.8.3 LE Read Local Supported Features command
             * </pre>
             * @param res reference for the resulting LE_Features
             * @return HCIStatusCode
             * @see initSupCommands()
             * @see resetAllStates()
             */
            LE_Features le_get_local_features() noexcept { return le_ll_feats; }

            /**
             * Request supported LE_Features from remote device.
             * <pre>
             * BT Core Spec v5.2: Vol 6, Part B, 4.6 (LE LL) Feature Support
             *
             * BT Core Spec v5.2: Vol 4, Part E, 7.8.21 LE Read Remote Features command
             * </pre>
             *
             * Method returns immediate without result.
             *
             * Result is being delivered off-thread via HCIMetaEventType::LE_REMOTE_FEAT_COMPLETE, see
             * <pre>
             * BT Core Spec v5.2: Vol 4, Part E, 7.7.65.4 LE Read Remote Features Complete event
             * </pre>
             *
             *
             * @return HCIStatusCode
             */
            HCIStatusCode le_read_remote_features(const uint16_t conn_handle, const BDAddressAndType& peerAddressAndType) noexcept;

        private:
            /**
             * Sets LE scanning parameters.
             * <pre>
             * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.64 LE Set Extended Scan Parameters command (Bluetooth 5.0)
             *
             * if available, otherwise using
             *
             * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.10 LE Set Scan Parameters command
             *
             *
             * BT Core Spec v5.2: Vol 6 LE, Part B Link Layer: 4.4.3 Scanning State
             * </pre>
             * <p>
             * Scan parameters control advertising (AD) Protocol Data Unit (PDU) delivery behavior.
             * </p>
             * <p>
             * Should not be called while LE scanning is active, otherwise HCIStatusCode::COMMAND_DISALLOWED will be returned.
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

        public:
            /**
             * Starts or stops LE scanning.
             * <pre>
             * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.65 LE Set Extended Scan Enable command (Bluetooth 5.0)
             *
             * if available, otherwise using
             *
             * BT Core Spec v5.2: Vol 4, Part E HCI: 7.8.11 LE Set Scan Enable command
             * </pre>
             * @param enable true to enable discovery, otherwise false
             * @param filter_dup true to filter out duplicate AD PDUs (default), otherwise all will be reported.
             */
            HCIStatusCode le_enable_scan(const bool enable, const bool filter_dup=true) noexcept;

            /**
             * Start LE scanning, i.e. performs le_set_scan_param() and le_enable_scan() in one atomic operation.
             * <pre>
             * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.64 LE Set Extended Scan Parameters command (Bluetooth 5.0)
             * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.10 LE Set Scan Parameters command
             * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.65 LE Set Extended Scan Enable command (Bluetooth 5.0)
             * BT Core Spec v5.2: Vol 4, Part E HCI: 7.8.11 LE Set Scan Enable command
             * </pre>
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
             * @param le_scan_active true enables delivery of active scanning PDUs, otherwise no scanning PDUs shall be sent (default)
             * @param own_mac_type HCILEOwnAddressType::PUBLIC (default) or random/private.
             * @param le_scan_interval in units of 0.625ms, default value 24 for 15ms; Value range [4 .. 0x4000] for [2.5ms .. 10.24s]
             * @param le_scan_window in units of 0.625ms, default value 24 for 15ms; Value range [4 .. 0x4000] for [2.5ms .. 10.24s]. Shall be <= le_scan_interval
             * @param filter_policy 0x00 accepts all PDUs (default), 0x01 only of whitelisted, ...
             * @see le_read_local_features()
             */
            HCIStatusCode le_start_scan(const bool filter_dup=true,
                                        const bool le_scan_active=false,
                                        const HCILEOwnAddressType own_mac_type=HCILEOwnAddressType::PUBLIC,
                                        const uint16_t le_scan_interval=24, const uint16_t le_scan_window=24,
                                        const uint8_t filter_policy=0x00) noexcept;

            /**
             * Establish a connection to the given LE peer.
             * <pre>
             * BT Core Spec v5.2: Vol 4, Part E HCI: 7.8.66 LE Extended Create Connection command (Bluetooth 5.0)
             *
             * if available, otherwise using
             *
             * BT Core Spec v5.2: Vol 4, Part E HCI: 7.8.12 LE Create Connection command
             * </pre>
             * <p>
             * Set window to the same value as the interval, enables continuous scanning.
             * </p>
             * <p>
             * The supervising timeout period is the time it takes before a devices gives up on the link if no packets are received.
             * Hence this parameter influences the responsiveness on a link loss.
             * A too small number may render the link too unstable, it should be at least 6 times of the connection interval.
             * <br>
             * To detect a link loss one can also send a regular ping to check whether the peripheral is still responding, see BTGattHandler::ping().
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
             * <pre>
             * BT Core Spec v5.2: Vol 4, Part E HCI: 7.1.5 Create Connection command
             * </pre>
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
             * <pre>
             * BT Core Spec v5.2: Vol 4, Part E HCI: 7.1.6 Disconnect command
             * </pre>
             */
            HCIStatusCode disconnect(const uint16_t conn_handle, const BDAddressAndType& peerAddressAndType,
                                     const HCIStatusCode reason=HCIStatusCode::REMOTE_USER_TERMINATED_CONNECTION) noexcept;

            /**
             * Request and return LE_PHYs bit for the given connection.
             * <pre>
             * BT Core Spec v5.2: Vol 4, Part E, 7.8.47 LE Read PHY command (we transfer the sequential value to this bitmask for unification)
             * </pre>
             * @param conn_handle
             * @param addressAndType
             * @param resRx reference for the resulting receiver LE_PHYs bit
             * @param resTx reference for the resulting transmitter LE_PHYs bit
             * @return HCIStatusCode
             */
            HCIStatusCode le_read_phy(const uint16_t conn_handle, const BDAddressAndType& peerAddressAndType,
                                      LE_PHYs& resRx, LE_PHYs& resTx) noexcept;

        private:
            /**
             * Sets LE advertising parameters.
             * <pre>
             * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.53 LE Set Extended Advertising Parameters command (Bluetooth 5.0)
             *
             * if available, otherwise using
             *
             * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.5 LE Set Advertising Parameters command
             * </pre>
             * <p>
             * Scan parameters control advertising (AD) Protocol Data Unit (PDU) delivery behavior.
             * </p>
             * <p>
             * Should not be called while LE scanning is active, otherwise HCIStatusCode::COMMAND_DISALLOWED will be returned.
             * </p>
             *
             * @param peer_bdaddr EUI48 of directed peer, defaults to EUI48::ANY_DEVICE (zero address)
             * @param own_mac_type HCILEOwnAddressType::PUBLIC (default) or random/private.
             * @param peer_mac_type HCILEOwnAddressType::PUBLIC (default) or random/private.
             * @param adv_interval_min in units of 0.625ms, default value 0x0800 for 1.28s; Value range [0x0020 .. 0x4000] for [20ms .. 10.24s]
             * @param adv_interval_max in units of 0.625ms, default value 0x0800 for 1.28s; Value range [0x0020 .. 0x4000] for [20ms .. 10.24s]
             * @param adv_type see AD_PDU_Type, default ::AD_PDU_Type::ADV_IND
             * @param adv_chan_map bit 0: chan 37, bit 1: chan 38, bit 2: chan 39, default is 0x07 (all 3 channels enabled)
             * @param filter_policy 0x00 accepts all PDUs (default), 0x01 only of whitelisted, ...
             */
            HCIStatusCode le_set_adv_param(const EUI48 &peer_bdaddr=EUI48::ANY_DEVICE,
                                           const HCILEOwnAddressType own_mac_type=HCILEOwnAddressType::PUBLIC,
                                           const HCILEOwnAddressType peer_mac_type=HCILEOwnAddressType::PUBLIC,
                                           const uint16_t adv_interval_min=0x0800, const uint16_t adv_interval_max=0x0800,
                                           const AD_PDU_Type adv_type=AD_PDU_Type::ADV_IND,
                                           const uint8_t adv_chan_map=0x07,
                                           const uint8_t filter_policy=0x00) noexcept;

            /**
             * Sets LE advertising data
             * <pre>
             * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.54 LE Set Extended Advertising Data command (Bluetooth 5.0)
             *
             * if available, otherwise using
             *
             * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.7 LE Set Advertising Data command
             * </pre>
             *
             * @param eir EInfoReport full ADV EIR
             * @param mask EIRDataType mask for EInfoReport to select advertisement EIR PDU data, defaults to EIRDataType::FLAGS | EIRDataType::NAME | EIRDataType::MANUF_DATA
             * @return HCIStatusCode::SUCCESS if successful, otherwise the HCIStatusCode error state
             */
            HCIStatusCode le_set_adv_data(const EInfoReport &eir,
                                          const EIRDataType mask = EIRDataType::FLAGS | EIRDataType::NAME | EIRDataType::MANUF_DATA) noexcept;

            /**
             * Sets LE scan-response data (active scanning)
             * <pre>
             * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.55 LE Set Extended Scan Response Data command (Bluetooth 5.0)
             *
             * if available, otherwise using
             *
             * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.8 LE Set Scan Response Data command
             * </pre>
             *
             * @param eir EInfoReport full ADV EIR
             * @param mask EIRDataType mask for EInfoReport to select scan-response EIR PDU data, defaults to EIRDataType::SERVICE_UUID
             * @return HCIStatusCode::SUCCESS if successful, otherwise the HCIStatusCode error state
             */
            HCIStatusCode le_set_scanrsp_data(const EInfoReport &eir,
                                              const EIRDataType mask = EIRDataType::SERVICE_UUID) noexcept;

        public:
            /**
             * Enables or disabled advertising.
             * <pre>
             * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.56 LE Set Extended Advertising Enable command (Bluetooth 5.0)
             *
             * if available, otherwise using
             *
             * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.9 LE Set Advertising Enable command
             * </pre>
             * @param enable
             * @return HCIStatusCode::SUCCESS if successful, otherwise the HCIStatusCode error state
             * @since 2.4.0
             */
            HCIStatusCode le_enable_adv(const bool enable) noexcept;

            /**
             * Starts advertising
             * <pre>
             * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.53 LE Set Extended Advertising Parameters command (Bluetooth 5.0)
             * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.54 LE Set Extended Advertising Data command (Bluetooth 5.0)
             * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.55 LE Set Extended Scan Response Data command (Bluetooth 5.0)
             * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.56 LE Set Extended Advertising Enable command (Bluetooth 5.0)
             *
             * if available, otherwise using
             *
             * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.5 LE Set Advertising Parameters command
             * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.7 LE Set Advertising Data command
             * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.8 LE Set Scan Response Data command
             * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.9 LE Set Advertising Enable command
             * </pre>
             * <p>
             * TODO:
             * - Random address for privacy if desired!
             * - Consider SMP (security)
             * </p>
             *
             * @param eir EInfoReport full ADV EIR
             * @param adv_mask EIRDataType mask for EInfoReport to select advertisement EIR PDU data, defaults to EIRDataType::FLAGS | EIRDataType::NAME | EIRDataType::MANUF_DATA
             * @param scanrsp_mask EIRDataType mask for EInfoReport to select scan-response (active scanning) EIR PDU data, defaults to EIRDataType::SERVICE_UUID
             * @param peer_bdaddr EUI48 of directed peer, defaults to EUI48::ANY_DEVICE (zero address)
             * @param own_mac_type HCILEOwnAddressType::PUBLIC (default) or random/private.
             * @param peer_mac_type HCILEOwnAddressType::PUBLIC (default) or random/private.
             * @param adv_interval_min in units of 0.625ms, default value 0x0800 for 1.28s; Value range [0x0020 .. 0x4000] for [20ms .. 10.24s]
             * @param adv_interval_max in units of 0.625ms, default value 0x0800 for 1.28s; Value range [0x0020 .. 0x4000] for [20ms .. 10.24s]
             * @param adv_type see AD_PDU_Type, default ::AD_PDU_Type::ADV_IND
             * @param adv_chan_map bit 0: chan 37, bit 1: chan 38, bit 2: chan 39, default is 0x07 (all 3 channels enabled)
             * @param filter_policy 0x00 accepts all PDUs (default), 0x01 only of whitelisted, ...
             * @return HCIStatusCode::SUCCESS if successful, otherwise the HCIStatusCode error state
             * @since 2.4.0
             */
            HCIStatusCode le_start_adv(const EInfoReport &eir,
                                       const EIRDataType adv_mask = EIRDataType::FLAGS | EIRDataType::NAME | EIRDataType::MANUF_DATA,
                                       const EIRDataType scanrsp_mask = EIRDataType::SERVICE_UUID,
                                       const EUI48 &peer_bdaddr=EUI48::ANY_DEVICE,
                                       const HCILEOwnAddressType own_mac_type=HCILEOwnAddressType::PUBLIC,
                                       const HCILEOwnAddressType peer_mac_type=HCILEOwnAddressType::PUBLIC,
                                       const uint16_t adv_interval_min=0x0800, const uint16_t adv_interval_max=0x0800,
                                       const AD_PDU_Type adv_type=AD_PDU_Type::ADV_IND,
                                       const uint8_t adv_chan_map=0x07,
                                       const uint8_t filter_policy=0x00) noexcept;

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

            void addSMPMsgCallback(const HCISMPMsgCallback & l);
            int removeSMPMsgCallback(const HCISMPMsgCallback & l);

            /** Removes all MgmtEventCallbacks from all MgmtEvent::Opcode lists and all SMPSecurityReqCallbacks. */
            void clearAllCallbacks() noexcept;

            /** Manually send a MgmtEvent to all of its listeners. */
            void sendMgmtEvent(const MgmtEvent& event) noexcept;
    };

} // namespace direct_bt

#if 0
// Injecting specialization of std::hash to namespace std of our types above
// Would need to make direct_bt::HCIHandler::HCIConnection accessible
namespace std
{
    template<> struct hash<direct_bt::HCIHandler::HCIConnection> {
        std::size_t operator()(direct_bt::HCIHandler::HCIConnection const& a) const noexcept {
            return a.hash_code();
        }
    };
}
#endif

#endif /* HCI_HANDLER_HPP_ */

