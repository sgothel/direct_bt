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

#include <mutex>
#include <atomic>
#include <thread>

#include <jau/environment.hpp>
#include <jau/ringbuffer.hpp>
#include <jau/java_uplink.hpp>
#include <jau/darray.hpp>
#include <jau/cow_darray.hpp>

#include "BTTypes0.hpp"
#include "BTIoctl.hpp"
#include "OctetTypes.hpp"
#include "HCIComm.hpp"
#include "MgmtTypes.hpp"
#include "BTAdapter.hpp"

namespace direct_bt {

    class BTManager; // forward

    /**
     * Managment Singleton runtime environment properties
     * <p>
     * Also see {@link DBTEnv::getExplodingProperties(const std::string & prefixDomain)}.
     * </p>
     */
    class MgmtEnv : public jau::root_environment {
        friend class BTManager;

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
             * Default is BTMode::DUAL, if non of the above environment variable is set.
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
     * <p>
     * If an adapter is being removed from the system,
     * DBTAdapter::close() is being called by BTManager after issuing all
     * ChangedAdapterSetFunc calls.
     * </p>
     *
     * @param added true if adapter was newly added, otherwise removed from system
     * @param adapter the shared DBTAdapter reference
     * @return ignored
     * @see ChangedAdapterSetCallback
     * @see BTManager::addChangedAdapterSetCallback()
     * @see BTManager::removeChangedAdapterSetCallback()
     */
    typedef bool (*ChangedAdapterSetFunc)(bool added, std::shared_ptr<BTAdapter>& adapter);

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
     * <p>
     * If an adapter is being removed from the system,
     * DBTAdapter::close() is being called by BTManager after issuing all
     * ChangedAdapterSetFunc calls.
     * </p>
     *
     * @param added true if adapter was newly added, otherwise removed from system
     * @param adapter the shared DBTAdapter reference
     * @return ignored
     * @see ChangedAdapterSetFunc
     * @see BTManager::addChangedAdapterSetCallback()
     * @see BTManager::removeChangedAdapterSetCallback()
     */
    typedef jau::FunctionDef<bool, bool, std::shared_ptr<BTAdapter>&> ChangedAdapterSetCallback;
    typedef jau::cow_darray<ChangedAdapterSetCallback> ChangedAdapterSetCallbackList;

    /**
     * A thread safe singleton handler of the Linux Kernel's BlueZ manager control channel.
     * <p>
     * Implementation utilizes a lock free ringbuffer receiving data within its separate thread.
     * </p>
     * <p>
     * Controlling Environment variables, see {@link MgmtEnv}.
     * </p>
     */
    class BTManager : public jau::JavaUplink {
        public:
            enum Defaults : int32_t {
                /* BT Core Spec v5.2: Vol 3, Part F 3.2.8: Maximum length of an attribute value. */
                ClientMaxMTU = 512
            };

            static const pid_t pidSelf;

        private:
            friend BTAdapter::~BTAdapter() noexcept;

#if USE_LINUX_BT_SECURITY
            /** Default initialization with ::SMPIOCapability::NO_INPUT_NO_OUTPUT for PairingMode::JUST_WORKS. */
            constexpr static const SMPIOCapability defaultIOCapability = SMPIOCapability::NO_INPUT_NO_OUTPUT;
#else
            /** Default initialization with ::SMPIOCapability::NO_INPUT_NO_OUTPUT for PairingMode::JUST_WORKS. */
            constexpr static const SMPIOCapability defaultIOCapability = SMPIOCapability::UNSET;
#endif
            static std::mutex mtx_singleton;

            struct WhitelistElem {
                uint16_t dev_id;
                BDAddressAndType address_and_type;
                HCIWhitelistConnectType ctype;

                WhitelistElem(uint16_t dev_id_, BDAddressAndType address_and_type_, HCIWhitelistConnectType ctype_)
                : dev_id(dev_id_), address_and_type(address_and_type_), ctype(ctype_) { }
            };
            jau::darray<std::shared_ptr<WhitelistElem>> whitelist;

            const MgmtEnv & env;
            const BTMode defaultBTMode;

            POctets rbuffer;
            HCIComm comm;

            jau::ringbuffer<std::unique_ptr<MgmtEvent>, std::nullptr_t, jau::nsize_t> mgmtEventRing;
            jau::sc_atomic_bool mgmtReaderShallStop;

            std::mutex mtx_mgmtReaderLifecycle;
            std::condition_variable cv_mgmtReaderInit;
            pthread_t mgmtReaderThreadId;
            jau::relaxed_atomic_bool mgmtReaderRunning;

            std::recursive_mutex mtx_sendReply; // for send() and sendWithReply()

            jau::sc_atomic_bool allowClose;

            /** One MgmtAdapterEventCallbackList per event type, allowing multiple callbacks to be invoked for each event */
            std::array<MgmtAdapterEventCallbackList, static_cast<uint16_t>(MgmtEvent::Opcode::MGMT_EVENT_TYPE_COUNT)> mgmtAdapterEventCallbackLists;
            inline bool isValidMgmtEventCallbackListsIndex(const MgmtEvent::Opcode opc) const noexcept {
                return static_cast<uint16_t>(opc) < mgmtAdapterEventCallbackLists.size();
            }

            ChangedAdapterSetCallbackList mgmtChangedAdapterSetCallbackList;

            typedef jau::cow_darray<std::shared_ptr<BTAdapter>> adapters_t;
            adapters_t adapters;

            /**
             * Using defaultIOCapability on added AdapterInfo.
             * Sharing same dev_id <-> index mapping of adapterInfos using findAdapterInfoIndex().
             * Piggy back reusing adapterInfos.get_write_mutex().
             */
            jau::darray<SMPIOCapability> adapterIOCapability;

            void mgmtReaderThreadImpl() noexcept;

            /**
             * In case response size check or devID and optional opcode validation fails,
             * function returns NULL.
             */
            std::unique_ptr<MgmtEvent> sendWithReply(MgmtCommand &req) noexcept;

            bool send(MgmtCommand &req) noexcept;

            /**
             * Instantiate singleton.
             * @param btMode default {@link BTMode} when initializing new adapter. If BTMode::NONE given, MgmtEnv::DEFAULT_BTMODE is being used.
             */
            BTManager(const BTMode defaultBTMode) noexcept;

            BTManager(const BTManager&) = delete;
            void operator=(const BTManager&) = delete;

            void setAdapterMode(const uint16_t dev_id, const uint8_t ssp, const uint8_t bredr, const uint8_t le) noexcept;
            std::unique_ptr<AdapterInfo> initAdapter(const uint16_t dev_id, const BTMode btMode) noexcept;
            void shutdownAdapter(BTAdapter& adapter) noexcept;

            void processAdapterAdded(std::unique_ptr<MgmtEvent> e) noexcept;
            void processAdapterRemoved(std::unique_ptr<MgmtEvent> e) noexcept;
            bool mgmtEvNewSettingsCB(const MgmtEvent& e) noexcept;
            bool mgmtEventAnyCB(const MgmtEvent& e) noexcept;

            std::shared_ptr<BTAdapter> addAdapter(const AdapterInfo& ai) noexcept;

            /**
             * Removes the AdapterInfo with the given dev_id
             * @return the removed instance or nullptr if not found.
             */
            std::shared_ptr<BTAdapter> removeAdapter(const uint16_t dev_id) noexcept;

            /**
             * Removal entry for DBTAdapter::~DBTAdapter
             * @param adapter pointer to the dtor'ed adapter
             * @return true if contained and removed, otherwise false
             */
            bool removeAdapter(BTAdapter* adapter) noexcept;

        public:
            /**
             * Retrieves the singleton instance.
             * <p>
             * First call will open and initialize the bluetooth kernel.
             * </p>
             * @param btMode default {@link BTMode} when initializing new adapter. If BTMode::NONE given (default), MgmtEnv::DEFAULT_BTMODE is being used.
             * @return singleton instance.
             */
            static BTManager& get(const BTMode defaultBTMode=BTMode::NONE) {
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
                static BTManager s(defaultBTMode);
                return s;
            }
            ~BTManager() noexcept { close(); }

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
                return "MgmtHandler[BTMode "+to_string(defaultBTMode)+", "+std::to_string(adapters.size())+" adapter, "+javaObjectToString()+"]";
            }

            /** retrieve information gathered at startup */

            /**
             * Returns AdapterInfo count in list
             */
            int getAdapterCount() const noexcept { return adapters.size(); }

            /**
             * Returns a list of currently added DBTAdapter.
             */
            jau::darray<std::shared_ptr<BTAdapter>> getAdapters() { return *adapters.snapshot(); }

            /**
             * Returns the DBTAdapter with the given address or nullptr if not found.
             */
            std::shared_ptr<BTAdapter> getAdapter(const EUI48 &mac) const noexcept;

            /**
             * Returns the DBTAdapter with the given dev_id, or nullptr if not found.
             */
            std::shared_ptr<BTAdapter> getAdapter(const uint16_t dev_id) const noexcept;

            /**
             * Returns the default AdapterInfo.
             * <p>
             * The default adapter is either the first AdapterSetting::POWERED adapter,
             * or function returns nullptr if none is AdapterSetting::POWERED.
             * </p>
             */
            std::shared_ptr<BTAdapter> getDefaultAdapter() const noexcept;

            bool setIOCapability(const uint16_t dev_id, const SMPIOCapability io_cap, SMPIOCapability& pre_io_cap) noexcept;
            SMPIOCapability getIOCapability(const uint16_t dev_id) const noexcept;

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
            bool uploadConnParam(const uint16_t dev_id, const BDAddressAndType & addressAndType,
                                 const uint16_t conn_interval_min=12, const uint16_t conn_interval_max=12,
                                 const uint16_t conn_latency=0, const uint16_t supervision_timeout=getHCIConnSupervisorTimeout(0, 15)) noexcept;

            /**
             * Returns true, if the adapter's device is already whitelisted.
             */
            bool isDeviceWhitelisted(const uint16_t dev_id, const BDAddressAndType & addressAndType) noexcept;

            /**
             * Add the given device to the adapter's autoconnect whitelist.
             * <p>
             * Make sure {@link uploadConnParam(..)} is invoked first, otherwise performance will lack.
             * </p>
             * <p>
             * Method will reject duplicate devices, in which case it should be removed first.
             * </p>
             */
            bool addDeviceToWhitelist(const uint16_t dev_id, const BDAddressAndType & addressAndType, const HCIWhitelistConnectType ctype) noexcept;

            /** Remove the given device from the adapter's autoconnect whitelist. */
            bool removeDeviceFromWhitelist(const uint16_t dev_id, const BDAddressAndType & addressAndType) noexcept;

            /** Remove all previously added devices from the autoconnect whitelist. Returns number of removed devices. */
            int removeAllDevicesFromWhitelist() noexcept;

            bool disconnect(const bool ioErrorCause,
                            const uint16_t dev_id, const BDAddressAndType & addressAndType,
                            const HCIStatusCode reason=HCIStatusCode::REMOTE_USER_TERMINATED_CONNECTION ) noexcept;

            std::shared_ptr<ConnectionInfo> getConnectionInfo(const uint16_t dev_id, const BDAddressAndType& addressAndType) noexcept;
            std::shared_ptr<NameAndShortName> setLocalName(const uint16_t dev_id, const std::string & name, const std::string & short_name) noexcept;

            /** Security commands */

            MgmtStatus uploadLinkKey(const uint16_t dev_id, const bool debug_keys, const MgmtLinkKeyInfo &key) noexcept;

            HCIStatusCode uploadLongTermKey(const uint16_t dev_id, const MgmtLongTermKeyInfo &key) noexcept;
            HCIStatusCode uploadLongTermKeyInfo(const uint16_t dev_id, const BDAddressAndType & addressAndType, const SMPLongTermKeyInfo& ltk) noexcept;

            MgmtStatus userPasskeyReply(const uint16_t dev_id, const BDAddressAndType & addressAndType, const uint32_t passkey) noexcept;
            MgmtStatus userPasskeyNegativeReply(const uint16_t dev_id, const BDAddressAndType & addressAndType) noexcept;
            MgmtStatus userConfirmReply(const uint16_t dev_id, const BDAddressAndType & addressAndType, const bool positive) noexcept;

            bool pairDevice(const uint16_t dev_id, const BDAddressAndType & addressAndType, const SMPIOCapability iocap) noexcept;
            MgmtStatus unpairDevice(const uint16_t dev_id, const BDAddressAndType & addressAndType, const bool disconnect) noexcept;

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
            void sendMgmtEvent(const MgmtEvent& event) noexcept;

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

