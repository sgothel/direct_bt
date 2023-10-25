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
#include <jau/octets.hpp>
#include <jau/service_runner.hpp>

#include "BTTypes0.hpp"
#include "BTIoctl.hpp"
#include "HCIComm.hpp"
#include "MgmtTypes.hpp"
#include "BTAdapter.hpp"
#include "jau/int_types.hpp"

namespace direct_bt {

    /** \addtogroup DBTUserAPI
     *
     *  @{
     */

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
            MgmtEnv() noexcept; // NOLINT(modernize-use-equals-delete)

        public:
            /** Global Debug flag, retrieved first to triggers DBTEnv initialization. */
            const bool DEBUG_GLOBAL;

        private:
            const bool exploding; // just to trigger exploding properties

        public:
            /**
             * Poll timeout for mgmt reader thread, defaults to 10s.
             * <p>
             * Environment variable is 'direct_bt.mgmt.reader.timeout'.
             * </p>
             */
            const jau::fraction_i64 MGMT_READER_THREAD_POLL_TIMEOUT;

            /**
             * Timeout for mgmt command replies, defaults to 3s.
             * <p>
             * Environment variable is 'direct_bt.mgmt.cmd.timeout'.
             * </p>
             */
            const jau::fraction_i64 MGMT_COMMAND_REPLY_TIMEOUT;

            /**
             * Timeout for mgmt SET_POWER command reply, defaults to max(MGMT_COMMAND_REPLY_TIMEOUT, 6s).
             * <p>
             * Environment variable is 'direct_bt.mgmt.setpower.timeout'.
             * </p>
             */
            const jau::fraction_i64 MGMT_SET_POWER_COMMAND_TIMEOUT;

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
     * @see ChangedAdapterSetCallback
     * @see BTManager::addChangedAdapterSetCallback()
     * @see BTManager::removeChangedAdapterSetCallback()
     * @see [Direct-BT Overview](namespacedirect__bt.html#details)
     */
    typedef void (*ChangedAdapterSetFunc)(bool added, std::shared_ptr<BTAdapter>& adapter);

    /**
     * Callback jau::function to receive change events regarding the system's adapter set,
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
     * @see ChangedAdapterSetFunc
     * @see BTManager::addChangedAdapterSetCallback()
     * @see BTManager::removeChangedAdapterSetCallback()
     * @see [Direct-BT Overview](namespacedirect__bt.html#details)
     */
    typedef jau::function<void(bool, std::shared_ptr<BTAdapter>&)> ChangedAdapterSetCallback;
    typedef jau::cow_darray<ChangedAdapterSetCallback> ChangedAdapterSetCallbackList;

    /**
     * A thread safe singleton handler of the BTAdapter manager, e.g. Linux Kernel's BlueZ manager control channel.
     *
     * Implementation utilizes a lock free ringbuffer receiving data within its separate thread.
     *
     * Controlling Environment variables, see {@link MgmtEnv}.
     *
     * @see [Direct-BT Overview](namespacedirect__bt.html#details)
     */
    class BTManager : public jau::jni::JavaUplink {
        public:
            enum Defaults : int32_t {
                /* BT Core Spec v5.2: Vol 3, Part F 3.2.8: Maximum length of an attribute value. */
                ClientMaxMTU = 512
            };

            typedef jau::nsize_t size_type;
            
        private:
            friend BTAdapter;

            /** Default initialization with ::SMPIOCapability::NO_INPUT_NO_OUTPUT for PairingMode::JUST_WORKS. */
            constexpr static const SMPIOCapability defaultIOCapability = USE_LINUX_BT_SECURITY ? SMPIOCapability::NO_INPUT_NO_OUTPUT : SMPIOCapability::UNSET;

            struct WhitelistElem {
                uint16_t dev_id;
                BDAddressAndType address_and_type;
                HCIWhitelistConnectType ctype;

                WhitelistElem(uint16_t dev_id_, BDAddressAndType address_and_type_, HCIWhitelistConnectType ctype_)
                : dev_id(dev_id_), address_and_type(std::move(address_and_type_)), ctype(ctype_) { }
            };
            jau::darray<std::shared_ptr<WhitelistElem>> whitelist;

            const MgmtEnv & env;

            jau::POctets rbuffer;
            HCIComm comm;

            jau::service_runner mgmt_reader_service;
            jau::ringbuffer<std::unique_ptr<MgmtEvent>, jau::nsize_t> mgmtEventRing;

            std::recursive_mutex mtx_sendReply; // for send() and sendWithReply()

            jau::sc_atomic_bool allowClose;

            /** One MgmtAdapterEventCallbackList per event type, allowing multiple callbacks to be invoked for each event */
            std::array<MgmtAdapterEventCallbackList, static_cast<uint16_t>(MgmtEvent::Opcode::MGMT_EVENT_TYPE_COUNT)> mgmtAdapterEventCallbackLists;
            inline bool isValidMgmtEventCallbackListsIndex(const MgmtEvent::Opcode opc) const noexcept {
                return static_cast<uint16_t>(opc) < mgmtAdapterEventCallbackLists.size();
            }

            ChangedAdapterSetCallbackList mgmtChangedAdapterSetCallbackList;

            typedef jau::cow_darray<std::shared_ptr<BTAdapter>, size_type> adapters_t;
            adapters_t adapters;

            /**
             * Using defaultIOCapability on added AdapterInfo.
             * Sharing same dev_id <-> index mapping of adapterInfos using findAdapterInfoIndex().
             * Piggy back reusing adapterInfos.get_write_mutex().
             */
            jau::darray<SMPIOCapability> adapterIOCapability;

            void mgmtReaderWork(jau::service_runner& sr) noexcept;
            void mgmtReaderEndLocked(jau::service_runner& sr) noexcept;

            /**
             * In case response size check or devID and optional opcode validation fails,
             * function returns NULL.
             *
             * @param req the command request
             * @param timeout timeout in fractions of seconds
             * @return the resulting event or nullptr on failure (timeout)
             */
            std::unique_ptr<MgmtEvent> sendWithReply(MgmtCommand &req, const jau::fraction_i64& timeout) noexcept;

            /**
             * In case response size check or devID and optional opcode validation fails,
             * function returns NULL.
             *
             * This override uses a timeout of MgmtEnv::MGMT_COMMAND_REPLY_TIMEOUT (usually 3s).
             *
             * @param req the command request
             * @return the resulting event or nullptr on failure (timeout)
             */
            std::unique_ptr<MgmtEvent> sendWithReply(MgmtCommand &req) noexcept {
                return sendWithReply(req, env.MGMT_COMMAND_REPLY_TIMEOUT);
            }

            bool send(MgmtCommand &req) noexcept;

            /**
             * Instantiate singleton.
             */
            BTManager() noexcept;
            bool initialize(const std::shared_ptr<BTManager>& self) noexcept;

            static const std::shared_ptr<BTManager> make_shared() noexcept {
                try {
                    std::shared_ptr<BTManager> s( new BTManager() );
                    s->initialize(s);
                    return s;
                } catch (const std::bad_alloc &e) {
                    ABORT("Error: bad_alloc: BTManager allocation failed");
                    return nullptr; // unreachable
                }
            }

            std::unique_ptr<AdapterInfo> readAdapterInfo(const uint16_t dev_id) noexcept;

            void processAdapterAdded(std::unique_ptr<MgmtEvent> e) noexcept;
            void processAdapterRemoved(std::unique_ptr<MgmtEvent> e) noexcept;
            void mgmtEvNewSettingsCB(const MgmtEvent& e) noexcept;
            void mgmtEventAnyCB(const MgmtEvent& e) noexcept;

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

            MgmtStatus handleCurrentSettingsReply(std::unique_ptr<MgmtEvent>&& reply, AdapterSetting& current_settings) noexcept;

        public:
            BTManager(const BTManager&) = delete;
            void operator=(const BTManager&) = delete;

            /**
             * Retrieves the singleton instance.
             * <p>
             * First call will open and initialize the bluetooth kernel.
             * </p>
             * @return singleton instance.
             */
            static const std::shared_ptr<BTManager>& get() noexcept {
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
                static std::shared_ptr<BTManager> s = make_shared();
                return s;
            }

            ~BTManager() noexcept override;

            void close() noexcept;

            std::string get_java_class() const noexcept override {
                return java_class();
            }
            static std::string java_class() noexcept {
                return std::string(JAVA_DBT_PACKAGE "DBTManager");
            }

            /** Returns true if this mgmt instance is open and hence valid, otherwise false */
            bool isOpen() const noexcept {
                return comm.is_open();
            }

            std::string toString() const noexcept override {
                return "MgmtHandler["+std::to_string(adapters.size())+" adapter, "+javaObjectToString()+"]";
            }

            /** retrieve information gathered at startup */

            /**
             * Returns AdapterInfo count in list
             */
            size_type getAdapterCount() const noexcept { return adapters.size(); }

            /**
             * Returns a list of currently added DBTAdapter.
             */
            jau::darray<std::shared_ptr<BTAdapter>> getAdapters() { return *adapters.snapshot(); }

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

            /**
             * Initialize the adapter with default values, including power-on.
             * <p>
             * Method shall be issued on the desired adapter found via ChangedAdapterSetFunc.
             * </p>
             * <p>
             * While initialization, the adapter is first powered-off, setup and then powered-on.
             * </p>
             * @param adapterInfo reference for the AdapterInfo, updated to reflect the new initialized values.
             * @param dev_id the adapter's device id
             * @param btMode the desired adapter's BTMode
             * @param powerOn true to leave adapter powered-on, otherwise leave it off
             * @return HCIStatusCode::SUCCESS or an error state on failure (e.g. power-on)
             * @since 3.2.0
             */
            HCIStatusCode initializeAdapter(AdapterInfo& adapterInfo, const uint16_t dev_id,
                                            const BTMode btMode, const bool powerOn) noexcept;

            HCIStatusCode setPrivacy(const uint16_t dev_id, const uint8_t privacy, const jau::uint128_t& irk, AdapterSetting& current_settings) noexcept;

            /**
             * Read default connection parameter for given adapter to the kernel.
             *
             * @param dev_id
             * @return list of MgmtDefaultParam if successful, empty if command failed.
             * @since 2.6.3
             */
            std::vector<MgmtDefaultParam> readDefaultSysParam(const uint16_t dev_id) noexcept;

            /**
             * Set default connection parameter for given adapter to the kernel.
             *
             * @param dev_id
             * @param conn_interval_min in units of 1.25ms, default value 8 for 10ms; Value range [6 .. 3200] for [7.5ms .. 4000ms].
             * @param conn_interval_max in units of 1.25ms, default value 40 for 50ms; Value range [6 .. 3200] for [7.5ms .. 4000ms]
             * @param conn_latency slave latency in units of connection events, default value 0; Value range [0 .. 0x01F3].
             * @param supervision_timeout in units of 10ms, default value 500ms >= 10 x conn_interval_max, we use HCIConstInt::LE_CONN_MIN_TIMEOUT_MS minimum; Value range [0xA-0x0C80] for [100ms - 32s].
             * @return
             * @since 2.5.3
             */
            HCIStatusCode setDefaultConnParam(const uint16_t dev_id,
                                              const uint16_t conn_interval_min=8, const uint16_t conn_interval_max=40,
                                              const uint16_t conn_latency=0, const uint16_t supervision_timeout=getHCIConnSupervisorTimeout(0, 50)) noexcept;

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
            HCIStatusCode uploadConnParam(const uint16_t dev_id, const BDAddressAndType & addressAndType,
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
            size_type removeAllDevicesFromWhitelist() noexcept;

            std::shared_ptr<ConnectionInfo> getConnectionInfo(const uint16_t dev_id, const BDAddressAndType& addressAndType) noexcept;
            std::shared_ptr<NameAndShortName> setLocalName(const uint16_t dev_id, const std::string & name, const std::string & short_name) noexcept;

            /** Security commands */

            /**
             * Linux Kernel `load_long_term_keys(..)` (mgmt.c) require either `BDAddressType::BDADDR_LE_PUBLIC` or
             * BDAddressType::BDADDR_LE_RANDOM and BLERandomAddressType::STATIC_PUBLIC in ltk_is_valid(..) (mgmt.c).
             *
             * The Linux kernel will reject unresolvable random addresses and resolvable random addresses.
             *
             * @return true if complying to above address-and-type requirements or if not using LINUX, otherwise false.
             */
            bool isValidLongTermKeyAddressAndType(const EUI48 &address, const BDAddressType &address_type) const noexcept;

            HCIStatusCode uploadLongTermKey(const uint16_t dev_id, const jau::darray<MgmtLongTermKey> &keys) noexcept;
            HCIStatusCode uploadLongTermKey(const BTRole adapterRole,
                                            const uint16_t dev_id, const BDAddressAndType & addressAndType, const jau::darray<SMPLongTermKey>& ltks) noexcept;

            HCIStatusCode uploadIdentityResolvingKey(const uint16_t dev_id, const jau::darray<MgmtIdentityResolvingKey> &keys) noexcept;
            HCIStatusCode uploadIdentityResolvingKey(const uint16_t dev_id, const jau::darray<SMPIdentityResolvingKey>& irks) noexcept;
            HCIStatusCode clearIdentityResolvingKeys(const uint16_t dev_id) noexcept;

            HCIStatusCode uploadLinkKey(const uint16_t dev_id, const MgmtLinkKeyInfo &key) noexcept;
            HCIStatusCode uploadLinkKey(const uint16_t dev_id, const BDAddressAndType & addressAndType, const SMPLinkKey& lk) noexcept;

            MgmtStatus userPINCodeReply(const uint16_t dev_id, const BDAddressAndType & addressAndType, const std::string& pinCode) noexcept;
            MgmtStatus userPINCodeNegativeReply(const uint16_t dev_id, const BDAddressAndType & addressAndType) noexcept;
            MgmtStatus userPasskeyReply(const uint16_t dev_id, const BDAddressAndType & addressAndType, const uint32_t passkey) noexcept;
            MgmtStatus userPasskeyNegativeReply(const uint16_t dev_id, const BDAddressAndType & addressAndType) noexcept;
            MgmtStatus userConfirmReply(const uint16_t dev_id, const BDAddressAndType & addressAndType, const bool positive) noexcept;

            HCIStatusCode unpairDevice(const uint16_t dev_id, const BDAddressAndType & addressAndType, const bool disconnect) noexcept;

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
            size_type removeMgmtEventCallback(const MgmtEvent::Opcode opc, const MgmtEventCallback &cb) noexcept;
            /** Returns count of removed MgmtEventCallback from the named MgmtEvent::Opcode list matching the given adapter dev_id . */
            size_type removeMgmtEventCallback(const int dev_id) noexcept;
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
            size_type removeChangedAdapterSetCallback(const ChangedAdapterSetCallback & l);

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
            size_type removeChangedAdapterSetCallback(ChangedAdapterSetFunc f);

            /**
             * Remove all added ChangedAdapterSetCallback entries from this manager.
             * @return the number of removed elements
             * @since 2.7.0
             */
            size_type removeAllChangedAdapterSetCallbacks() noexcept;

    };
    typedef std::shared_ptr<BTManager> BTManagerRef;

    /**@}*/

} // namespace direct_bt

#endif /* DBT_MANAGER_HPP_ */

