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

#ifndef DBT_MANAGER_HPP_
#define DBT_MANAGER_HPP_

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
#include <jau/cow_vector.hpp>

#include "BTTypes.hpp"
#include "BTIoctl.hpp"
#include "OctetTypes.hpp"
#include "HCIComm.hpp"
#include "MgmtTypes.hpp"

namespace direct_bt {

    class DBTManager; // forward

    /**
     * Managment Singleton runtime environment properties
     * <p>
     * Also see {@link DBTEnv::getExplodingProperties(const std::string & prefixDomain)}.
     * </p>
     */
    class MgmtEnv : public jau::root_environment {
        friend class DBTManager;

        private:
            MgmtEnv() noexcept;

        public:
            /** Global Debug flag, retrieved first to triggers DBTEnv initialization. */
            const bool DEBUG_GLOBAL;

        private:
            const bool exploding; // just to trigger exploding properties
            static BTMode getEnvBTMode();

        public:
            /**
             * Poll timeout for mgmt reader thread, defaults to 10s.
             * <p>
             * Environment variable is 'direct_bt.mgmt.reader.timeout'.
             * </p>
             */
            const int32_t MGMT_READER_THREAD_POLL_TIMEOUT;

            /**
             * Timeout for mgmt command replies, defaults to 3s.
             * <p>
             * Environment variable is 'direct_bt.mgmt.cmd.timeout'.
             * </p>
             */
            const int32_t MGMT_COMMAND_REPLY_TIMEOUT;

            /**
             * Small ringbuffer capacity for synchronized commands, defaults to 64 messages.
             * <p>
             * Environment variable is 'direct_bt.mgmt.ringsize'.
             * </p>
             */
            const int32_t MGMT_EVT_RING_CAPACITY;

            /**
             * Debug all Mgmt event communication
             * <p>
             * Environment variable is 'direct_bt.debug.mgmt.event'.
             * </p>
             */
            const bool DEBUG_EVENT;

            /**
             * Default {@link BTMode} when initializing new adapter
             * <p>
             * Environment variable is 'direct_bt.mgmt.btmode' first, then try 'org.tinyb.btmode'.
             * </p>
             * <p>
             * Default is BTMode::LE, if non of the above environment variable is set.
             * </p>
             */
            const BTMode DEFAULT_BTMODE;

        private:
            /** Maximum number of packets to wait for until matching a sequential command. Won't block as timeout will limit. */
            const int32_t MGMT_READ_PACKET_MAX_RETRY;

        public:
            static MgmtEnv& get() noexcept {
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
                static MgmtEnv e;
                return e;
            }
    };

    /**
     * Callback function to receive change events regarding the system's adapter set,
     * e.g. a removed or added adapter due to user interaction or 'cold reset'.
     * <p>
     * When a new callback is added, all available adapter's will be reported as added,
     * this allows a fully event driven workflow.
     * </p>
     * <p>
     * The callback is performed on a dedicated thread,
     * allowing the user to perform complex operations.
     * </p>
     *
     * @param added true if adapter was newly added, otherwise removed from system
     * @param adapterInfo the adapter's AdapterInfo, inclusive the dev_id
     * @return ignored
     * @see ChangedAdapterSetCallback
     * @see DBTManager::addChangedAdapterSetCallback()
     * @see DBTManager::removeChangedAdapterSetCallback()
     */
    typedef bool (*ChangedAdapterSetFunc)(bool added, const AdapterInfo& adapterInfo);

    /**
     * Callback jau::FunctionDef to receive change events regarding the system's adapter set,
     * e.g. a removed or added adapter due to user interaction or 'cold reset'.
     * <p>
     * When a new callback is added, all available adapter's will be reported as added,
     * this allows a fully event driven workflow.
     * </p>
     * <p>
     * The callback is performed on a dedicated thread,
     * allowing the user to perform complex operations.
     * </p>
     *
     * @param added true if adapter was newly added, otherwise removed from system
     * @param adapterInfo the adapter's AdapterInfo, inclusive the dev_id
     * @return ignored
     * @see ChangedAdapterSetFunc
     * @see DBTManager::addChangedAdapterSetCallback()
     * @see DBTManager::removeChangedAdapterSetCallback()
     */
    typedef jau::FunctionDef<bool, bool, const AdapterInfo&> ChangedAdapterSetCallback;
    typedef jau::cow_vector<ChangedAdapterSetCallback> ChangedAdapterSetCallbackList;

    /**
     * A thread safe singleton handler of the Linux Kernel's BlueZ manager control channel.
     * <p>
     * Implementation utilizes a lock free ringbuffer receiving data within its separate thread.
     * </p>
     * <p>
     * Controlling Environment variables, see {@link MgmtEnv}.
     * </p>
     */
    class DBTManager : public jau::JavaUplink {
        public:
            enum Defaults : int32_t {
                /* BT Core Spec v5.2: Vol 3, Part F 3.2.8: Maximum length of an attribute value. */
                ClientMaxMTU = 512
            };

            static const pid_t pidSelf;

        private:
            static std::mutex mtx_singleton;

            struct WhitelistElem {
                uint16_t dev_id;
                EUI48 address;
                BDAddressType address_type;
                HCIWhitelistConnectType ctype;
            };
            std::vector<std::shared_ptr<WhitelistElem>> whitelist;

            const MgmtEnv & env;
            const BTMode defaultBTMode;
            POctets rbuffer;
            HCIComm comm;

            jau::ringbuffer<std::shared_ptr<MgmtEvent>, nullptr, jau::nsize_t> mgmtEventRing;
            jau::sc_atomic_bool mgmtReaderShallStop;

            std::mutex mtx_mgmtReaderLifecycle;
            std::condition_variable cv_mgmtReaderInit;
            pthread_t mgmtReaderThreadId;
            jau::relaxed_atomic_bool mgmtReaderRunning;

            std::recursive_mutex mtx_sendReply; // for sendWithReply

            jau::sc_atomic_bool allowClose;

            /** One MgmtAdapterEventCallbackList per event type, allowing multiple callbacks to be invoked for each event */
            std::array<MgmtAdapterEventCallbackList, static_cast<uint16_t>(MgmtEvent::Opcode::MGMT_EVENT_TYPE_COUNT)> mgmtAdapterEventCallbackLists;
            inline bool isValidMgmtEventCallbackListsIndex(const MgmtEvent::Opcode opc) const noexcept {
                return static_cast<uint16_t>(opc) < mgmtAdapterEventCallbackLists.size();
            }

            ChangedAdapterSetCallbackList mgmtChangedAdapterSetCallbackList;

            jau::cow_vector<std::shared_ptr<AdapterInfo>> adapterInfos;
            void mgmtReaderThreadImpl() noexcept;

            /**
             * In case response size check or devID and optional opcode validation fails,
             * function returns NULL.
             */
            std::shared_ptr<MgmtEvent> sendWithReply(MgmtCommand &req) noexcept;

            /**
             * Instantiate singleton.
             * @param btMode default {@link BTMode} when initializing new adapter. If BTMode::NONE given, MgmtEnv::DEFAULT_BTMODE is being used.
             */
            DBTManager(const BTMode defaultBTMode) noexcept;

            DBTManager(const DBTManager&) = delete;
            void operator=(const DBTManager&) = delete;

            void setAdapterMode(const uint16_t dev_id, const uint8_t ssp, const uint8_t bredr, const uint8_t le) noexcept;
            std::shared_ptr<AdapterInfo> initAdapter(const uint16_t dev_id, const BTMode btMode) noexcept;
            void shutdownAdapter(const uint16_t dev_id) noexcept;

            void processAdapterAdded(std::shared_ptr<MgmtEvent> e) noexcept;
            void processAdapterRemoved(std::shared_ptr<MgmtEvent> e) noexcept;
            bool mgmtEvNewSettingsCB(std::shared_ptr<MgmtEvent> e) noexcept;
            bool mgmtEventAnyCB(std::shared_ptr<MgmtEvent> e) noexcept;
            bool mgmtEvControllerErrorCB(std::shared_ptr<MgmtEvent> e) noexcept;
            bool mgmtEvNewLinkKeyCB(std::shared_ptr<MgmtEvent> e) noexcept;
            bool mgmtEvNewLongTermKeyCB(std::shared_ptr<MgmtEvent> e) noexcept;
            bool mgmtEvDeviceUnpairedCB(std::shared_ptr<MgmtEvent> e) noexcept;
            bool mgmtEvPinCodeRequestCB(std::shared_ptr<MgmtEvent> e) noexcept;
            bool mgmtEvAuthFailedCB(std::shared_ptr<MgmtEvent> e) noexcept;
            bool mgmtEvUserConfirmRequestCB(std::shared_ptr<MgmtEvent> e) noexcept;
            bool mgmtEvUserPasskeyRequestCB(std::shared_ptr<MgmtEvent> e) noexcept;

            bool mgmtEvClassOfDeviceChangedCB(std::shared_ptr<MgmtEvent> e) noexcept;
            bool mgmtEvDeviceDiscoveringCB(std::shared_ptr<MgmtEvent> e) noexcept;
            bool mgmtEvDeviceFoundCB(std::shared_ptr<MgmtEvent> e) noexcept;
            bool mgmtEvDeviceDisconnectedCB(std::shared_ptr<MgmtEvent> e)noexcept;
            bool mgmtEvDeviceConnectedCB(std::shared_ptr<MgmtEvent> e) noexcept;
            bool mgmtEvConnectFailedCB(std::shared_ptr<MgmtEvent> e) noexcept;
            bool mgmtEvDeviceBlockedCB(std::shared_ptr<MgmtEvent> e) noexcept;
            bool mgmtEvDeviceUnblockedCB(std::shared_ptr<MgmtEvent> e) noexcept;
            bool mgmtEvNewConnectionParamCB(std::shared_ptr<MgmtEvent> e) noexcept;
            bool mgmtEvDeviceWhitelistAddedCB(std::shared_ptr<MgmtEvent> e) noexcept;
            bool mgmtEvDeviceWhilelistRemovedCB(std::shared_ptr<MgmtEvent> e) noexcept;

            /**
             * Adds the given AdapterInfo if representing a new dev_id.
             * @return true if newly added dev_id, otherwise false if dev_id already exists.
             */
            bool addAdapterInfo(std::shared_ptr<AdapterInfo> ai) noexcept;

            /**
             * Removes the AdapterInfo with the given dev_id
             * @return the removed instance or nullptr if not found.
             */
            std::shared_ptr<AdapterInfo> removeAdapterInfo(const uint16_t dev_id) noexcept;

        public:
            /**
             * Retrieves the singleton instance.
             * <p>
             * First call will open and initialize the bluetooth kernel.
             * </p>
             * @param btMode default {@link BTMode} when initializing new adapter. If BTMode::NONE given, MgmtEnv::DEFAULT_BTMODE is being used.
             * @return singleton instance.
             */
            static DBTManager& get(const BTMode defaultBTMode) {
                const std::lock_guard<std::mutex> lock(mtx_singleton); // ensure thread safety
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
                static DBTManager s(defaultBTMode);
                return s;
            }
            ~DBTManager() noexcept { close(); }

            void close() noexcept;

            std::string get_java_class() const noexcept override {
                return java_class();
            }
            static std::string java_class() noexcept {
                return std::string(JAVA_DBT_PACKAGE "DBTManager");
            }

            /** Returns the default {@link BTMode}, adapters are tried to be initialized. */
            BTMode getDefaultBTMode() noexcept { return defaultBTMode; }

            /** Returns true if this mgmt instance is open and hence valid, otherwise false */
            bool isOpen() const noexcept {
                return comm.isOpen();
            }

            std::string toString() const noexcept override {
                return "MgmtHandler[BTMode "+getBTModeString(defaultBTMode)+", "+std::to_string(adapterInfos.size())+" adapter, "+javaObjectToString()+"]";
            }

            /** retrieve information gathered at startup */

            /**
             * Returns AdapterInfo count in list
             */
            int getAdapterCount() const noexcept { return adapterInfos.size(); }

            /**
             * Returns the AdapterInfo dev_id with the given address or -1 if not found.
             */
            int findAdapterInfoDevId(const EUI48 &mac) const noexcept;

            /**
             * Returns the AdapterInfo with the given address or nullptr if not found.
             */
            std::shared_ptr<AdapterInfo> findAdapterInfo(const EUI48 &mac) const noexcept;

            /**
             * Returns the AdapterInfo with the given dev_id, or nullptr if not found.
             */
            std::shared_ptr<AdapterInfo> getAdapterInfo(const uint16_t dev_id) const noexcept;

            /**
             * Returns the current BTMode of given adapter dev_idx or BTMode::NONE if dev_id adapter is not available.
             */
            BTMode getCurrentBTMode(uint16_t dev_id) const noexcept;

            /**
             * Returns the default AdapterInfo.
             * <p>
             * The default adapter is either the first AdapterSetting::POWERED adapter,
             * or function returns nullptr if none is AdapterSetting::POWERED.
             * </p>
             */
            std::shared_ptr<AdapterInfo> getDefaultAdapterInfo() const noexcept;

            /**
             * Returns the default adapter dev_id (index).
             * <p>
             * The default adapter is either the first AdapterSetting::POWERED adapter,
             * or function returns -1 if none is AdapterSetting::POWERED.
             * </p>
             */
            int getDefaultAdapterDevID() const noexcept;

            bool setMode(const uint16_t dev_id, const MgmtCommand::Opcode opc, const uint8_t mode, AdapterSetting& current_settings) noexcept;
            MgmtStatus setDiscoverable(const uint16_t dev_id, const uint8_t state, const uint16_t timeout, AdapterSetting& current_settings) noexcept;

            /** Start discovery on given adapter dev_id with a ScanType matching the given BTMode. Returns set ScanType. */
            ScanType startDiscovery(const uint16_t dev_id, const BTMode btMode) noexcept;
            /** Start discovery on given adapter dev_id with given ScanType. Returns set ScanType. */
            ScanType startDiscovery(const uint16_t dev_id, const ScanType type) noexcept;
            /** Stop discovery on given adapter dev_id. */
            bool stopDiscovery(const uint16_t dev_id, const ScanType type) noexcept;

            /**
             * Uploads given connection parameter for given device to the kernel.
             *
             * @param dev_id
             * @param address
             * @param address_type
             * @param conn_interval_min in units of 1.25ms, default value 12 for 15ms; Value range [6 .. 3200] for [7.5ms .. 4000ms]
             * @param conn_interval_max in units of 1.25ms, default value 12 for 15ms; Value range [6 .. 3200] for [7.5ms .. 4000ms]
             * @param conn_latency slave latency in units of connection events, default value 0; Value range [0 .. 0x01F3].
             * @param supervision_timeout in units of 10ms, default value >= 10 x conn_interval_max, we use HCIConstInt::LE_CONN_MIN_TIMEOUT_MS minimum; Value range [0xA-0x0C80] for [100ms - 32s].
             * @return
             */
            bool uploadConnParam(const uint16_t dev_id, const EUI48 &address, const BDAddressType address_type,
                                 const uint16_t conn_interval_min=12, const uint16_t conn_interval_max=12,
                                 const uint16_t conn_latency=0, const uint16_t supervision_timeout=getHCIConnSupervisorTimeout(0, 15)) noexcept;

            /**
             * Returns true, if the adapter's device is already whitelisted.
             */
            bool isDeviceWhitelisted(const uint16_t dev_id, const EUI48 &address) noexcept;

            /**
             * Add the given device to the adapter's autoconnect whitelist.
             * <p>
             * Make sure {@link uploadConnParam(..)} is invoked first, otherwise performance will lack.
             * </p>
             * <p>
             * Method will reject duplicate devices, in which case it should be removed first.
             * </p>
             */
            bool addDeviceToWhitelist(const uint16_t dev_id, const EUI48 &address, const BDAddressType address_type, const HCIWhitelistConnectType ctype) noexcept;

            /** Remove the given device from the adapter's autoconnect whitelist. */
            bool removeDeviceFromWhitelist(const uint16_t dev_id, const EUI48 &address, const BDAddressType address_type) noexcept;

            /** Remove all previously added devices from the autoconnect whitelist. Returns number of removed devices. */
            int removeAllDevicesFromWhitelist() noexcept;

            bool disconnect(const bool ioErrorCause,
                            const uint16_t dev_id, const EUI48 &peer_bdaddr, const BDAddressType peer_mac_type,
                            const HCIStatusCode reason=HCIStatusCode::REMOTE_USER_TERMINATED_CONNECTION ) noexcept;

            std::shared_ptr<ConnectionInfo> getConnectionInfo(const uint16_t dev_id, const EUI48 &address, const BDAddressType address_type) noexcept;
            std::shared_ptr<NameAndShortName> setLocalName(const uint16_t dev_id, const std::string & name, const std::string & short_name) noexcept;

            /** Security commands */

            MgmtStatus uploadLinkKey(const uint16_t dev_id, const bool debug_keys, const MgmtLinkKey &key) noexcept;
            MgmtStatus uploadLongTermKey(const uint16_t dev_id, const MgmtLongTermKey &key) noexcept;
            MgmtStatus userPasskeyReply(const uint16_t dev_id, const EUI48 &address, const BDAddressType addressType, const uint32_t passkey) noexcept;
            MgmtStatus userPasskeyNegativeReply(const uint16_t dev_id, const EUI48 &address, const BDAddressType addressType) noexcept;

            /** MgmtEventCallback handling  */

            /**
             * Appends the given MgmtEventCallback for the given adapter dev_id to the named MgmtEvent::Opcode list,
             * if it is not present already (dev_id + opcode + callback).
             * <p>
             * The adapter dev_id allows filtering the events only directed to the given adapter.
             * Use dev_id <code>-1</code> to receive the event for all adapter.
             * </p>
             * @param dev_id the associated adapter dev_id
             * @param opc opcode index for callback list, the callback shall be added to
             * @param cb the to be added callback
             * @return true if newly added or already existing, false if given MgmtEvent::Opcode is out of supported range.
             */
            bool addMgmtEventCallback(const int dev_id, const MgmtEvent::Opcode opc, const MgmtEventCallback &cb) noexcept;
            /** Returns count of removed given MgmtEventCallback from the named MgmtEvent::Opcode list. */
            int removeMgmtEventCallback(const MgmtEvent::Opcode opc, const MgmtEventCallback &cb) noexcept;
            /** Returns count of removed MgmtEventCallback from the named MgmtEvent::Opcode list matching the given adapter dev_id . */
            int removeMgmtEventCallback(const int dev_id) noexcept;
            /** Removes all MgmtEventCallbacks from the to the named MgmtEvent::Opcode list. */
            void clearMgmtEventCallbacks(const MgmtEvent::Opcode opc) noexcept;
            /** Removes all MgmtEventCallbacks from all MgmtEvent::Opcode lists. */
            void clearAllCallbacks() noexcept;

            /** Manually send a MgmtEvent to all of its listeners. */
            void sendMgmtEvent(std::shared_ptr<MgmtEvent> event) noexcept;

            /** ChangedAdapterSetCallback handling */

            /**
             * Adds the given ChangedAdapterSetCallback to this manager.
             * <p>
             * When a new callback is added, all available adapter's will be reported as added,
             * this allows a fully event driven workflow.
             * </p>
             * <p>
             * The callback is performed on a dedicated thread,
             * allowing the user to perform complex operations.
             * </p>
             */
            void addChangedAdapterSetCallback(const ChangedAdapterSetCallback & l);

            /**
             * Remove the given ChangedAdapterSetCallback from this manager.
             * @param l the to be removed element
             * @return the number of removed elements
             */
            int removeChangedAdapterSetCallback(const ChangedAdapterSetCallback & l);

            /**
             * Adds the given ChangedAdapterSetFunc to this manager.
             * <p>
             * When a new callback is added, all available adapter's will be reported as added,
             * this allows a fully event driven workflow.
             * </p>
             * <p>
             * The callback is performed on a dedicated thread,
             * allowing the user to perform complex operations.
             * </p>
             */
            void addChangedAdapterSetCallback(ChangedAdapterSetFunc f);

            /**
             * Remove the given ChangedAdapterSetFunc from this manager.
             * @param l the to be removed element
             * @return the number of removed elements
             */
            int removeChangedAdapterSetCallback(ChangedAdapterSetFunc f);
    };

} // namespace direct_bt

#endif /* DBT_MANAGER_HPP_ */

