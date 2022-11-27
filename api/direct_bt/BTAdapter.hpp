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

#ifndef BT_ADAPTER_HPP_
#define BT_ADAPTER_HPP_

#include <cstring>
#include <string>
#include <memory>
#include <cstdint>

#include <mutex>
#include <atomic>

#include <jau/darray.hpp>
#include <jau/cow_darray.hpp>
#include <jau/simple_timer.hpp>
#include "BTTypes1.hpp"

#include "BTDevice.hpp"

#include "HCIHandler.hpp"

#include "DBGattServer.hpp"

#include "SMPKeyBin.hpp"
#include "jau/int_types.hpp"

namespace direct_bt {

    /** @defgroup DBTUserAPI Direct-BT General User Level API
     *  General User level Direct-BT API types and functionality, [see Direct-BT Overview](namespacedirect__bt.html#details).
     *
     *  @{
     */

    class BTAdapter; // forward
    class BTManager; // forward
    typedef std::shared_ptr<BTManager> BTManagerRef;

    /**
     * Discovery policy defines the BTAdapter discovery mode after connecting a remote BTDevice:
     * - turned-off (DiscoveryPolicy::AUTO_OFF)
     * - paused until all connected BTDevice become disconnected, effectively until AdapterStatusListener::deviceDisconnected() (DiscoveryPolicy::PAUSE_CONNECTED_UNTIL_DISCONNECTED).
     * - paused until all connected devices reach readiness inclusive optional SMP pairing (~120ms) and GATT service discovery (~700ms), effectively until AdapterStatusListener::deviceReady(). (DiscoveryPolicy::PAUSE_CONNECTED_UNTIL_READY, default)
     * - paused until all connected devices are optionally SMP paired (~120ms), exclusive GATT service discovery (~700ms -> ~1200ms, DiscoveryPolicy::PAUSE_CONNECTED_UNTIL_PAIRED)
     * - always enabled, i.e. re-enabled if automatically turned-off by HCI host OS as soon as possible (DiscoveryPolicy::ALWAYS_ON)
     *
     * Policy is set via BTAdapter::startDiscovery()
     *
     * Default is DiscoveryPolicy::PAUSE_CONNECTED_UNTIL_READY, as it has been shown that continuous advertising
     * reduces the bandwidth for the initial bring-up time including GATT service discovery considerably.
     * Continuous advertising would increase the readiness lag of the remote device until AdapterStatusListener::deviceReady().
     *
     * In case users favors faster parallel discovery of new remote devices and hence a slower readiness,
     * DiscoveryPolicy::PAUSE_CONNECTED_UNTIL_PAIRED or even DiscoveryPolicy::ALWAYS_ON can be used.
     *
     * @since 2.5.0
     */
    enum class DiscoveryPolicy : uint8_t {
        /** Turn off discovery when connected and leave discovery disabled, if turned off by host system. */
        AUTO_OFF                     = 0,
        /**
         * Pause discovery until all connected BTDevice become disconnected,
         * effectively until AdapterStatusListener::deviceDisconnected().
         */
        PAUSE_CONNECTED_UNTIL_DISCONNECTED  = 1,
        /**
         * Pause discovery until all connected BTDevice reach readiness inclusive optional SMP pairing (~120ms) without GATT service discovery (~700ms),
         * effectively until AdapterStatusListener::deviceReady(). This is the default!
         */
        PAUSE_CONNECTED_UNTIL_READY  = 2,
        /** Pause discovery until all connected BTDevice are optionally SMP paired (~120ms) without GATT service discovery (~700ms). */
        PAUSE_CONNECTED_UNTIL_PAIRED = 3,
        /** Always keep discovery enabled, i.e. re-enabled if automatically turned-off by HCI host OS as soon as possible. */
        ALWAYS_ON                    = 4
    };
    constexpr uint8_t number(const DiscoveryPolicy rhs) noexcept {
        return static_cast<uint8_t>(rhs);
    }
    constexpr DiscoveryPolicy to_DiscoveryPolicy(const uint8_t v) noexcept {
        if( 1 <= v && v <= 4 ) {
            return static_cast<DiscoveryPolicy>(v);
        }
        return DiscoveryPolicy::AUTO_OFF;
    }
    std::string to_string(const DiscoveryPolicy v) noexcept;

    /**
     * {@link BTAdapter} status listener for remote {@link BTDevice} discovery events: Added, updated and removed;
     * as well as for certain {@link BTAdapter} events.
     * <p>
     * User implementations shall return as early as possible to avoid blocking the event-handler thread,
     * if not specified within the methods otherwise (see AdapterStatusListener::deviceReady()).<br>
     * Especially complex mutable operations on BTDevice or BTAdapter should be issued off-thread!
     * </p>
     * <p>
     * A listener instance may be attached to a {@link BTAdapter} via
     * {@link BTAdapter::addStatusListener(const AdapterStatusListenerRef&)}.
     * </p>
     * <p>
     * The listener receiver maintains a unique set of listener instances without duplicates.
     * </p>
     */
    class AdapterStatusListener : public jau::jni::JavaUplink {
        public:
            /**
             * BTAdapter setting(s) changed.
             * @param adapter the adapter which settings have changed.
             * @param oldmask the previous settings mask. AdapterSetting::NONE indicates the initial setting notification, see BTAdapter::addStatusListener().
             * @param newmask the new settings mask
             * @param changedmask the changes settings mask. AdapterSetting::NONE indicates the initial setting notification, see BTAdapter::addStatusListener().
             * @param timestamp the time in monotonic milliseconds when this event occurred. See BasicTypes::getCurrentMilliseconds().
             */
            virtual void adapterSettingsChanged(BTAdapter &adapter, const AdapterSetting oldmask, const AdapterSetting newmask,
                                                const AdapterSetting changedmask, const uint64_t timestamp) {
                (void)adapter;
                (void)oldmask;
                (void)newmask;
                (void)changedmask;
                (void)timestamp;
            }

            /**
             * BTAdapter's discovery state has changed, i.e. enabled or disabled.
             * @param adapter the adapter which discovering state has changed.
             * @param currentMeta the current meta ScanType
             * @param changedType denotes the changed native ScanType
             * @param changedEnabled denotes whether the changed native ScanType has been enabled or disabled
             * @param policy the current DiscoveryPolicy of the BTAdapter, chosen via BTAdapter::startDiscovery()
             * @param timestamp the time in monotonic milliseconds when this event occurred. See BasicTypes::getCurrentMilliseconds().
             *
             * changeScanType(const ScanType current, const bool enable, const ScanType enableChanged) noexcept {
             */
            virtual void discoveringChanged(BTAdapter &adapter, const ScanType currentMeta, const ScanType changedType, const bool changedEnabled, const DiscoveryPolicy policy, const uint64_t timestamp) {
                (void)adapter;
                (void)currentMeta;
                (void)changedType;
                (void)changedEnabled;
                (void)policy;
                (void)timestamp;
            }

            /**
             * A remote BTDevice has been newly discovered.
             * <p>
             * The boolean return value informs the adapter whether the device shall be made persistent for connection `true`,
             * or that it can be discarded `false`.<br>
             * If no registered AdapterStatusListener::deviceFound() implementation returns `true`,
             * the device instance will be removed from all internal lists and can no longer being used.<br>
             * If any registered AdapterStatusListener::deviceFound() implementation returns `true`,
             * the device will be made persistent, is ready to connect and BTDevice::remove() shall be called after usage.
             * </p>
             *
             * BTDevice::unpair() has been called already.
             *
             * @param device the found remote device
             * @param timestamp the time in monotonic milliseconds when this event occurred. See BasicTypes::getCurrentMilliseconds().
             * @return true if the device shall be made persistent and BTDevice::remove() issued later. Otherwise false to remove device right away.
             * @see BTDevice::unpair()
             * @see BTDevice::getEIR()
             */
            virtual bool deviceFound(BTDeviceRef device, const uint64_t timestamp) {
                (void)device;
                (void)timestamp;
                return false;
            }

            /**
             * An already discovered remote BTDevice has been updated.
             * @param device the updated remote device
             * @param updateMask the update mask of changed data
             * @param timestamp the time in monotonic milliseconds when this event occurred. See BasicTypes::getCurrentMilliseconds().
             *
             * @see BTDevice::getEIR()
             */
            virtual void deviceUpdated(BTDeviceRef device, const EIRDataType updateMask, const uint64_t timestamp) {
                (void)device;
                (void)updateMask;
                (void)timestamp;
            }

            /**
             * Remote BTDevice got connected
             *
             * If a BTRole::Master BTDevice gets connected, BTDevice::unpair() has been called already.
             *
             * @param device the remote device which has been connected, holding the new connection handle.
             * @param discovered `true` if discovered before connected and deviceFound() has been sent (default), otherwise `false`.
             * @param timestamp the time in monotonic milliseconds when this event occurred. See BasicTypes::getCurrentMilliseconds().
             * @see BTDevice::unpair()
             * @since 2.6.6
             */
            virtual void deviceConnected(BTDeviceRef device, const bool discovered, const uint64_t timestamp) {
                (void)device;
                (void)discovered;
                (void)timestamp;
            }

            /**
             * An already connected remote BTDevice's ::SMPPairingState has changed.
             * @param device the remote device which PairingMode has been changed.
             * @param state the current ::SMPPairingState of the connected device, see BTDevice::getCurrentPairingState()
             * @param mode the current ::PairingMode of the connected device, see BTDevice::getCurrentPairingMode()
             * @param timestamp the time in monotonic milliseconds when this event occurred. See BasicTypes::getCurrentMilliseconds().
             * @see BTDevice::setPairingPasskey()
             * @see BTDevice::setPairingNumericComparison()
             */
            virtual void devicePairingState(BTDeviceRef device, const SMPPairingState state, const PairingMode mode, const uint64_t timestamp) {
                (void)device;
                (void)state;
                (void)mode;
                (void)timestamp;
            }

            /**
             * Remote BTDevice is ready for user (GATT) processing, i.e. already connected, optionally (SMP) paired.
             *
             * In case of a LE connection to a remote BTDevice in BTRole::Slave, a GATT server (GATTRole::Server),
             * user needs to call BTDevice::getGattServices() to have GATT MTU size negotiated and GATT services discovered.
             * <p>
             * Method is being called from a dedicated native thread, hence restrictions on method duration and complex mutable operations don't apply here.
             * </p>
             * @param device the remote device ready to use
             * @param timestamp the time in monotonic milliseconds when this event occurred. See BasicTypes::getCurrentMilliseconds().
             * @see ::SMPPairingState::COMPLETED
             * @see BTDevice::getGattServices()
             */
            virtual void deviceReady(BTDeviceRef device, const uint64_t timestamp) {
                (void)device;
                (void)timestamp;
            }

            /**
             * Remote BTDevice got disconnected
             *
             * BTDevice::unpair() has been called already.
             *
             * @param device the remote device which has been disconnected with zeroed connection handle.
             * @param reason the HCIStatusCode reason for disconnection
             * @param handle the disconnected connection handle, which has been unassigned from the device already
             * @param timestamp the time in monotonic milliseconds when this event occurred. See BasicTypes::getCurrentMilliseconds().
             * @see BTDevice::unpair()
             */
            virtual void deviceDisconnected(BTDeviceRef device, const HCIStatusCode reason, const uint16_t handle, const uint64_t timestamp) {
                (void)device;
                (void)reason;
                (void)handle;
                (void)timestamp;
            }

            ~AdapterStatusListener() noexcept override = default;

            std::string toString() const noexcept override { return "AdapterStatusListener["+jau::to_hexstring(this)+"]"; }

            std::string get_java_class() const noexcept override {
                return java_class();
            }
            static std::string java_class() noexcept {
                return std::string(JAVA_MAIN_PACKAGE "AdapterStatusListener");
            }

            /**
             * Default comparison operator, merely testing for same memory reference.
             * <p>
             * Specializations may override.
             * </p>
             */
            virtual bool operator==(const AdapterStatusListener& rhs) const
            { return this == &rhs; }

            bool operator!=(const AdapterStatusListener& rhs) const
            { return !(*this == rhs); }
    };
    typedef std::shared_ptr<AdapterStatusListener> AdapterStatusListenerRef;

    // *************************************************
    // *************************************************
    // *************************************************

    /**
     * BTAdapter represents one local Bluetooth Controller.
     *
     * @anchor BTAdapterRoles
     * Local BTAdapter roles (see getRole()):
     *
     * - {@link BTRole::Master}: The local adapter is discovering remote ::BTRole::Slave {@link BTDevice}s and may initiate connections. Enabled via startDiscovery(), but also per default at construction.
     * - {@link BTRole::Slave}: The local adapter is advertising to remote ::BTRole::Master {@link BTDevice}s and may accept connections. Enabled explicitly via startAdvertising() until startDiscovery().
     *
     * Note the remote {@link BTDevice}'s [opposite role](@ref BTDeviceRoles).
     *
     * Controlling Environment variables:
     * - 'direct_bt.debug.adapter.event': Debug messages about events, see debug_events
     *
     * @see BTDevice
     * @see @ref BTDeviceRoles
     * @see @ref BTGattHandlerRoles
     * @see [Bluetooth Specification](https://www.bluetooth.com/specifications/bluetooth-core-specification/)
     * @see [Direct-BT Overview](namespacedirect__bt.html#details)
     */
    class BTAdapter : public BTObject
    {
        private:
            friend BTManager;

            const bool debug_event, debug_lock;
            BTManagerRef mgmt;
            std::atomic_bool adapter_operational;
            AdapterInfo adapterInfo;

            /** Flag signaling whether initialize() has been called, regardless of success. */
            jau::sc_atomic_bool adapter_initialized;
            /** Flag signaling whether initialize() has powered-on this adapter. */
            jau::sc_atomic_bool adapter_poweredon_at_init;

            LE_Features le_features;

            /** BT5: True if HCI_LE_Set_Extended_Scan_Parameters and HCI_LE_Set_Extended_Scan_Enable is supported (Bluetooth 5.0). */
            bool hci_uses_ext_scan;
            /** BT5: True if HCI_LE_Extended_Create_Connection is supported (Bluetooth 5.0). */
            bool hci_uses_ext_conn;
            /** BT5: True if HCI_LE_Extended_Advertising Data is supported (Bluetooth 5.0). */
            bool hci_uses_ext_adv;

            /**
             * Either the adapter's initially reported public address or a random address setup via HCI before discovery or advertising.
             */
            BDAddressAndType visibleAddressAndType;

        public:
            typedef jau::nsize_t size_type;

            /**
             * Adapter's internal temporary device id.
             * <p>
             * The internal device id is constant across the adapter lifecycle,
             * but may change after its destruction.
             */
            const uint16_t dev_id;

        private:
            jau::ordered_atomic<BTRole, std::memory_order::memory_order_relaxed> btRole; // = BTRole::Master (default)
            HCIHandler hci;

            jau::ordered_atomic<AdapterSetting, std::memory_order::memory_order_relaxed> old_settings;
            jau::ordered_atomic<ScanType, std::memory_order::memory_order_relaxed> currentMetaScanType; // = ScanType::NONE
            jau::ordered_atomic<DiscoveryPolicy, std::memory_order::memory_order_relaxed> discovery_policy; // = DiscoveryPolicy::PAUSE_CONNECTED_UNTIL_READY

            jau::relaxed_atomic_bool scan_filter_dup; //  = true;

            SMPIOCapability  iocap_defaultval = SMPIOCapability::UNSET;
            const BTDevice* single_conn_device_ptr = nullptr;
            std::mutex mtx_single_conn_device;
            std::condition_variable cv_single_conn_device;

            typedef jau::darray<BTDeviceRef, size_type> device_list_t;
            typedef jau::darray<std::weak_ptr<BTDevice>, size_type> weak_device_list_t;

            /** All discovered devices: Transient until removeDiscoveredDevices(), startDiscovery(). */
            device_list_t discoveredDevices;
            /** All connected devices: Transient until disconnect or removal. */
            device_list_t connectedDevices;
            /** All active shared devices: Persistent until removal. Final holder of BTDevice lifecycle! */
            device_list_t sharedDevices;
            /** All connected devices for which discovery has been paused. */
            weak_device_list_t pausing_discovery_devices;
            /** An SMP event watchdog for each device in pairing state */
            jau::simple_timer smp_watchdog;
            jau::fraction_i64 smp_timeoutfunc(jau::simple_timer& timer);

            struct StatusListenerPair {
                /** The actual listener */
                AdapterStatusListenerRef listener;
                /** The optional weak device reference. Weak, b/c it shall not block destruction */
                std::weak_ptr<BTDevice> wbr_device;

                bool match(const BTDeviceRef& device) const noexcept {
                    BTDeviceRef sda = wbr_device.lock();
                    if( nullptr != sda && nullptr != device ) {
                        return *sda == *device;
                    } else {
                        return true;
                    }
                }
            };
            typedef jau::cow_darray<StatusListenerPair, size_type> statusListenerList_t;
            static statusListenerList_t::equal_comparator adapterStatusListenerRefEqComparator;
            statusListenerList_t statusListenerList;

            // Storing SMPKeyBin entries, referenced by their remote address, i.e. BTDevice address.
            std::string key_path;
            typedef std::shared_ptr<SMPKeyBin> SMPKeyBinRef;
            typedef jau::darray<SMPKeyBinRef, size_type> key_list_t;
            key_list_t key_list;
            BTSecurityLevel sec_level_server = BTSecurityLevel::UNSET;
            SMPIOCapability io_cap_server = SMPIOCapability::UNSET;

            DBGattServerRef gattServerData = nullptr;

            mutable std::mutex mtx_discoveredDevices;
            mutable std::mutex mtx_connectedDevices;
            mutable std::mutex mtx_pausingDiscoveryDevices;
            mutable std::mutex mtx_discovery;
            mutable std::mutex mtx_sharedDevices; // final mutex of all BTDevice lifecycle
            mutable std::mutex mtx_keys;
            mutable jau::sc_atomic_bool sync_data;

            bool updateDataFromHCI() noexcept;
            bool updateDataFromAdapterInfo() noexcept;
            bool initialSetup() noexcept;
            bool enableListening(const bool enable) noexcept;

            static BTDeviceRef findDevice(device_list_t & devices, const EUI48 & address, const BDAddressType addressType) noexcept;
            static BTDeviceRef findDevice(device_list_t & devices, BTDevice const & device) noexcept;
            static BTDeviceRef findWeakDevice(weak_device_list_t & devices, const EUI48 & address, const BDAddressType addressType) noexcept;
            static BTDeviceRef findWeakDevice(weak_device_list_t & devices, BTDevice const & device) noexcept;
            static void printDeviceList(const std::string& prefix, const BTAdapter::device_list_t& list) noexcept;
            static void printWeakDeviceList(const std::string& prefix, BTAdapter::weak_device_list_t& list) noexcept;

            /** Private class only for private make_shared(). */
            class ctor_cookie { friend BTAdapter; ctor_cookie(const uint16_t secret) { (void)secret; } };

            /** Private std::make_shared<BTAdapter>(..) vehicle for friends. */
            static std::shared_ptr<BTAdapter> make_shared(const BTManagerRef& mgmt_, const AdapterInfo& adapterInfo_) {
                return std::make_shared<BTAdapter>(BTAdapter::ctor_cookie(0), mgmt_, adapterInfo_);
            }

            /**
             * Closes all device connections, stops discovery and cleans up all references.
             * <p>
             * To be called at
             *
             * - destructor or when powered off (active = true)
             * - AdapterSetting changed, POWERED disabled, just powered off (active = false)
             * - when `!isPowered()` is detected in methods (active = false)
             * </p>
             * @param active true if still powered and actively stopDiscovery and disconnect devices, otherwise this is a passive operation
             */
            void poweredOff(bool active, const std::string& msg) noexcept;

            friend BTDeviceRef BTDevice::getSharedInstance() const noexcept;
            friend void BTDevice::remove() noexcept;
            friend BTDevice::~BTDevice() noexcept;

            friend std::shared_ptr<ConnectionInfo> BTDevice::getConnectionInfo() noexcept;
            friend void BTDevice::sendMgmtEvDeviceDisconnected(std::unique_ptr<MgmtEvent> evt) noexcept;
            friend HCIStatusCode BTDevice::disconnect(const HCIStatusCode reason) noexcept;
            friend HCIStatusCode BTDevice::connectLE(uint16_t interval, uint16_t window,
                                                      uint16_t min_interval, uint16_t max_interval,
                                                      uint16_t latency, uint16_t supervision_timeout) noexcept;
            friend HCIStatusCode BTDevice::connectBREDR(const uint16_t pkt_type, const uint16_t clock_offset, const uint8_t role_switch) noexcept;
            friend void BTDevice::processL2CAPSetup(BTDeviceRef sthis);
            friend bool BTDevice::updatePairingState(BTDeviceRef sthis, const MgmtEvent& evt, const HCIStatusCode evtStatus, SMPPairingState claimed_state) noexcept;
            friend void BTDevice::hciSMPMsgCallback(BTDeviceRef sthis, const SMPPDUMsg& msg, const HCIACLData::l2cap_frame& source) noexcept;
            friend void BTDevice::processDeviceReady(BTDeviceRef sthis, const uint64_t timestamp);
            friend bool BTDevice::connectGATT(std::shared_ptr<BTDevice> sthis) noexcept;
            friend jau::darray<BTGattServiceRef> BTDevice::getGattServices() noexcept;

            bool lockConnect(const BTDevice & device, const bool wait, const SMPIOCapability io_cap) noexcept;
            bool unlockConnect(const BTDevice & device) noexcept;
            bool unlockConnectAny() noexcept;

            bool addDevicePausingDiscovery(const BTDeviceRef & device) noexcept;
            BTDeviceRef findDevicePausingDiscovery (const EUI48 & address, const BDAddressType & addressType) noexcept;
            void clearDevicesPausingDiscovery() noexcept;
            jau::nsize_t getDevicesPausingDiscoveryCount() noexcept;

            bool addConnectedDevice(const BTDeviceRef & device) noexcept;
            bool removeConnectedDevice(const BTDevice & device) noexcept;
            size_type disconnectAllDevices(const HCIStatusCode reason=HCIStatusCode::REMOTE_USER_TERMINATED_CONNECTION ) noexcept;
            BTDeviceRef findConnectedDevice (const EUI48 & address, const BDAddressType & addressType) noexcept;
            jau::nsize_t getConnectedDeviceCount() const noexcept;

            bool addDiscoveredDevice(BTDeviceRef const &device) noexcept;

            void removeDevice(BTDevice & device) noexcept;

            bool addSharedDevice(BTDeviceRef const &device) noexcept;
            BTDeviceRef getSharedDevice(const BTDevice & device) noexcept;
            void removeSharedDevice(const BTDevice & device) noexcept;

            static SMPKeyBinRef findSMPKeyBin(key_list_t & keys, BDAddressAndType const & remoteAddress) noexcept;
            static bool removeSMPKeyBin(key_list_t & keys, BDAddressAndType const & remoteAddress, const bool remove_file, const std::string key_path_) noexcept;
            SMPKeyBinRef findSMPKeyBin(BDAddressAndType const & remoteAddress) noexcept;
            /** Adding a SMPKeyBin will remove previous entry. */
            bool addSMPKeyBin(const SMPKeyBinRef& key, const bool write_file) noexcept;
            bool removeSMPKeyBin(BDAddressAndType const & remoteAddress, const bool remove_file) noexcept;

            L2CAPServer l2cap_att_srv;
            jau::service_runner l2cap_service;
            std::unique_ptr<L2CAPClient> l2cap_att;
            std::mutex mtx_l2cap_att;
            std::condition_variable cv_l2cap_att;
            void l2capServerWork(jau::service_runner& sr) noexcept;
            void l2capServerInit(jau::service_runner& sr) noexcept;
            void l2capServerEnd(jau::service_runner& sr) noexcept;
            std::unique_ptr<L2CAPClient> get_l2cap_connection(std::shared_ptr<BTDevice> device);

            void mgmtEvNewSettingsMgmt(const MgmtEvent& e) noexcept;
            void updateAdapterSettings(const bool off_thread, const AdapterSetting new_settings, const bool sendEvent, const uint64_t timestamp) noexcept;
            void mgmtEvDeviceDiscoveringMgmt(const MgmtEvent& e) noexcept;
            void mgmtEvLocalNameChangedMgmt(const MgmtEvent& e) noexcept;
            void mgmtEvDeviceFoundHCI(const MgmtEvent& e) noexcept;
            void mgmtEvPairDeviceCompleteMgmt(const MgmtEvent& e) noexcept;
            void mgmtEvNewLongTermKeyMgmt(const MgmtEvent& e) noexcept;
            void mgmtEvNewLinkKeyMgmt(const MgmtEvent& e) noexcept;

            void mgmtEvHCIAnyHCI(const MgmtEvent& e) noexcept;
            void mgmtEvDeviceDiscoveringHCI(const MgmtEvent& e) noexcept;
            void mgmtEvDeviceConnectedHCI(const MgmtEvent& e) noexcept;

            void mgmtEvConnectFailedHCI(const MgmtEvent& e) noexcept;
            void mgmtEvHCILERemoteUserFeaturesHCI(const MgmtEvent& e) noexcept;
            void mgmtEvHCILEPhyUpdateCompleteHCI(const MgmtEvent& e) noexcept;
            void mgmtEvDeviceDisconnectedHCI(const MgmtEvent& e) noexcept;

            // Local BTRole::Slave
            void mgmtEvLELTKReqEventHCI(const MgmtEvent& e) noexcept;
            void mgmtEvLELTKReplyAckCmdHCI(const MgmtEvent& e) noexcept;
            void mgmtEvLELTKReplyRejCmdHCI(const MgmtEvent& e) noexcept;

            // Local BTRole::Master
            void mgmtEvLEEnableEncryptionCmdHCI(const MgmtEvent& e) noexcept;
            void mgmtEvHCIEncryptionChangedHCI(const MgmtEvent& e) noexcept;
            void mgmtEvHCIEncryptionKeyRefreshCompleteHCI(const MgmtEvent& e) noexcept;

            void updateDeviceDiscoveringState(const ScanType eventScanType, const bool eventEnabled) noexcept;
            void mgmtEvDeviceDiscoveringAny(const ScanType eventScanType, const bool eventEnabled, const uint64_t eventTimestamp,
                                            const bool hciSourced) noexcept;

            void mgmtEvPinCodeRequestMgmt(const MgmtEvent& e) noexcept;
            void mgmtEvUserConfirmRequestMgmt(const MgmtEvent& e) noexcept;
            void mgmtEvUserPasskeyRequestMgmt(const MgmtEvent& e) noexcept;
            void mgmtEvAuthFailedMgmt(const MgmtEvent& e) noexcept;
            void mgmtEvDeviceUnpairedMgmt(const MgmtEvent& e) noexcept;

            void hciSMPMsgCallback(const BDAddressAndType & addressAndType,
                                   const SMPPDUMsg& msg, const HCIACLData::l2cap_frame& source) noexcept;
            void sendDevicePairingState(BTDeviceRef device, const SMPPairingState state, const PairingMode mode, uint64_t timestamp) noexcept;
            void notifyPairingStageDone(BTDeviceRef device, uint64_t timestamp) noexcept;
            void sendDeviceReady(BTDeviceRef device, uint64_t timestamp) noexcept;

            jau::service_runner discovery_service;
            void discoveryServerWork(jau::service_runner& sr) noexcept;
            void checkDiscoveryState() noexcept;

            void sendAdapterSettingsChanged(const AdapterSetting old_settings_, const AdapterSetting current_settings, AdapterSetting changes,
                                            const uint64_t timestampMS) noexcept;
            void sendAdapterSettingsInitial(AdapterStatusListener & asl, const uint64_t timestampMS) noexcept;

            void sendDeviceUpdated(std::string cause, BTDeviceRef device, uint64_t timestamp, EIRDataType updateMask) noexcept;

            size_type removeAllStatusListener(const BTDevice& d) noexcept;

        public:

            /** Private ctor for private BTAdapter::make_shared() intended for friends. */
            BTAdapter(const BTAdapter::ctor_cookie& cc, BTManagerRef mgmt_, AdapterInfo adapterInfo_) noexcept;

            BTAdapter(const BTAdapter&) = delete;
            void operator=(const BTAdapter&) = delete;

            /**
             * Releases this instance.
             */
            ~BTAdapter() noexcept override;

            /**
             * Closes this instance, usually being called by destructor or when this adapter is being removed
             * as recognized and handled by BTManager.
             * <p>
             * In case initialize() has powered-on this adapter and was not powered-on before,
             * it will be powered-off.
             * </p>
             * <p>
             * Renders this adapter's BTAdapter#isValid() state to false.
             * </p>
             * @see initialize()
             * @see setPowered()
             */
            void close() noexcept;

            std::string get_java_class() const noexcept override {
                return java_class();
            }
            static std::string java_class() noexcept {
                return std::string(JAVA_DBT_PACKAGE "DBTAdapter");
            }

            /**
             * Returns whether the adapter is valid, plugged in and powered.
             * @return true if BTAdapter::isValid(), HCIHandler::isOpen() and AdapterSetting::POWERED state is set.
             * @see #isSuspended()
             * @see #isValid()
             */
            bool isPowered() const noexcept {
                return isValid() && hci.isOpen() && adapterInfo.isCurrentSettingBitSet(AdapterSetting::POWERED);
            }

            /**
             * Returns whether the adapter is suspended, i.e. valid and plugged in, but not powered.
             * @return true if BTAdapter::isValid(), HCIHandler::isOpen() and AdapterSetting::POWERED state is not set.
             * @see #isPowered()
             * @see #isValid()
             */
            bool isSuspended() const noexcept {
                return isValid() && hci.isOpen() && !adapterInfo.isCurrentSettingBitSet(AdapterSetting::POWERED);
            }

            bool hasSecureConnections() const noexcept {
                return adapterInfo.isCurrentSettingBitSet(AdapterSetting::SECURE_CONN);
            }

            bool hasSecureSimplePairing() const noexcept {
                return adapterInfo.isCurrentSettingBitSet(AdapterSetting::SSP);
            }

            /**
             * Return LE_Features for this controller.
             * <pre>
             * BT Core Spec v5.2: Vol 6, Part B, 4.6 (LE LL) Feature Support
             * </pre>
             */
            constexpr LE_Features getLEFeatures() const noexcept { return le_features; }

            /** Returns the Bluetooth major version of this adapter. Currently either `4` or `5`. */
            constexpr uint16_t getBTMajorVersion() const noexcept { return ( hci_uses_ext_scan && hci_uses_ext_conn && hci_uses_ext_adv ) ? 5 : 4; }

            /**
             * Returns whether the adapter is valid, i.e. reference is valid, plugged in and generally operational,
             * but not necessarily BTAdapter::isPowered() powered.
             * @return true if this adapter references are valid and hasn't been BTAdapter::close() 'ed
             * @see #isPowered()
             * @see #isSuspended()
             */
            bool isValid() const noexcept {
                return BTObject::isValidInstance() && adapter_operational;
            }

            /**
             * Return the current BTRole of this adapter.
             * @see BTRole
             * @see @ref BTAdapterRoles
             * @since 2.4.0
             */
            BTRole getRole() const noexcept { return btRole; }

            /**
             * Returns the current BTMode of this adapter.
             */
            BTMode getBTMode() const noexcept { return adapterInfo.getCurrentBTMode(); }

            /**
             * Returns the adapter's public BDAddressAndType.
             * <p>
             * The adapter's address as initially reported by the system is always its public address, i.e. BDAddressType::BDADDR_LE_PUBLIC.
             * </p>
             * @since 2.2.8
             * @see #getVisibleAddressAndType()
             */
            BDAddressAndType const & getAddressAndType() const noexcept { return adapterInfo.addressAndType; }

            /**
             * Returns the adapter's currently visible BDAddressAndType.
             * <p>
             * The adapter's address as initially reported by the system is always its public address, i.e. BDAddressType::BDADDR_LE_PUBLIC.
             * </p>
             * <p>
             * The adapter's visible BDAddressAndType might be set to BDAddressType::BDADDR_LE_RANDOM before scanning / discovery mode (TODO).
             * </p>
             * @since 2.2.8
             * @see #getAddressAndType()
             */
            BDAddressAndType const & getVisibleAddressAndType() const noexcept { return visibleAddressAndType; }

            /**
             * Returns the name.
             * <p>
             * Can be changed via setName() while powered-off.
             * </p>
             * @see setName()
             */
            std::string getName() const noexcept { return adapterInfo.getName(); }

            /**
             * Returns the short name.
             * <p>
             * Can be changed via setName() while powered-off.
             * </p>
             * @see setName()
             */
            std::string getShortName() const noexcept { return adapterInfo.getShortName(); }

            /**
             * Sets the name and short-name.
             *
             * The corresponding management event will change the name and short-name.
             *
             * Shall be called while adapter is powered off, see setPowered().
             * If adapter is powered, method returns HCIStatusCode::COMMAND_DISALLOWED.
             *
             * @param name
             * @param short_name
             * @return HCIStatusCode::SUCCESS or an error state on failure
             * @see getName()
             * @see getShortName()
             * @see setPowered()
             * @since 2.4.0
             */
            HCIStatusCode setName(const std::string &name, const std::string &short_name) noexcept;

            /**
             * Set the power state of the adapter.
             *
             * In case current power state is already as desired, method will not change the power state.
             *
             * @param power_on true will power on this adapter if it is powered-off and vice versa.
             * @return true if successfully powered-on, -off or unchanged, false on failure
             * @see isInitialized()
             * @see close()
             * @see initialize()
             */
            bool setPowered(const bool power_on) noexcept;

            /**
             * Returns whether Secure Connections (SC) is enabled.
             * @see setSecureConnections()
             * @since 2.4.0
             */
            bool getSecureConnectionsEnabled() const noexcept {
                return adapterInfo.isCurrentSettingBitSet(AdapterSetting::SECURE_CONN);
            }

            /**
             * Enable or disable Secure Connections (SC) of the adapter.
             *
             * By default, Secure Connections (SC) is enabled if supported.
             *
             * Shall be called while adapter is powered off, see setPowered().
             * If adapter is powered, method returns HCIStatusCode::COMMAND_DISALLOWED.
             *
             * @param enable
             * @return HCIStatusCode::SUCCESS or an error state on failure
             * @see getSecureConnectionsEnabled()
             * @see setPowered()
             * @since 2.5.3
             */
            HCIStatusCode setSecureConnections(const bool enable) noexcept;

            /**
             * Set default connection parameter of incoming connections for this adapter when in server mode, i.e. BTRole::Slave.
             *
             * In case the incoming connection's parameter don't lie within the given default values,
             * a reconnect is being requested.
             *
             * Shall be called while adapter is powered off, see setPowered().
             * If adapter is powered, method returns HCIStatusCode::COMMAND_DISALLOWED.
             *
             * BlueZ/Linux LE connection defaults are
             * - conn_interval_min 24 -> 30ms
             * - conn_interval_max 40 -> 50ms
             * - conn_latency 0
             * - supervision_timeout 42 -> 420ms
             *
             * Supported on GNU/Linux since kernel 5.9.
             *
             * @param dev_id
             * @param conn_interval_min in units of 1.25ms, default value 8 for 10ms; Value range [6 .. 3200] for [7.5ms .. 4000ms].
             * @param conn_interval_max in units of 1.25ms, default value 40 for 50ms; Value range [6 .. 3200] for [7.5ms .. 4000ms]
             * @param conn_latency slave latency in units of connection events, default value 0; Value range [0 .. 0x01F3].
             * @param supervision_timeout in units of 10ms, default value 500ms >= 10 x conn_interval_max, we use HCIConstInt::LE_CONN_MIN_TIMEOUT_MS minimum; Value range [0xA-0x0C80] for [100ms - 32s].
             * @return HCIStatusCode::SUCCESS or an error state on failure
             * @see setPowered()
             * @since 2.5.3
             */
            HCIStatusCode setDefaultConnParam(const uint16_t conn_interval_min=8, const uint16_t conn_interval_max=40,
                                              const uint16_t conn_latency=0, const uint16_t supervision_timeout=getHCIConnSupervisorTimeout(0, 50)) noexcept;

            /**
             * Sets the given ::BTSecurityLevel and ::SMPIOCapability for connecting device when in server (peripheral) mode, see [adapter's role](@ref BTAdapterRoles).
             * <p>
             * Method either changes both parameter for the upcoming connection or none at all.
             * </p>
             * @param[in] sec_level ::BTSecurityLevel to be applied.
             * @param[in] io_cap ::SMPIOCapability to be applied.
             * @see ::BTSecurityLevel
             * @see ::SMPIOCapability
             * @see BTDevice::setConnSecurity()
             * @see startAdvertising()
             */
            void setServerConnSecurity(const BTSecurityLevel sec_level, const SMPIOCapability io_cap) noexcept;

            /**
             * Set the adapter's persistent storage directory for SMPKeyBin files.
             * - if set, all SMPKeyBin instances will be managed and persistent.
             * - if not set, all SMPKeyBin instances will be transient only.
             *
             * When called, all keys within the path will be loaded,
             * i.e. issuing uploadKeys() for all keys belonging to this BTAdapter.
             *
             * Persistent SMPKeyBin management is only functional when BTAdapter is in BTRole::Slave peripheral mode.
             *
             * For each SMPKeyBin file one shared BTDevice in BTRole::Master will be instantiated
             * when uploadKeys() is called.
             *
             * @param path persistent storage path to SMPKeyBin files
             * @see uploadKeys()
             */
            void setSMPKeyPath(const std::string path) noexcept;

            /**
             * Associate the given SMPKeyBin with the contained remote address, i.e. SMPKeyBin::getRemoteAddrAndType().
             *
             * Further uploads the Long Term Key (LTK) and Link Key (LK) for a potential upcoming connection,
             * if they are contained in the given SMPKeyBin file.
             *
             * This method is provided to support BTRole::Slave peripheral adapter mode,
             * allowing user to inject all required keys after initialize()
             *
             * One shared BTDevice in BTRole::Master is instantiated.
             *
             * @param bin SMPKeyBin instance, might be persistent in filesystem
             * @param write if true, write file to persistent storage, otherwise not
             * @return HCIStatusCode::SUCCESS or an error state on failure
             * @see setSMPKeyPath()
             * @see BTDevice::uploadKeys()
             */
            HCIStatusCode uploadKeys(SMPKeyBin& bin, const bool write) noexcept;

            /**
             * Initialize the adapter with default values, including power-on.
             * <p>
             * Method shall be issued on the desired adapter found via ChangedAdapterSetFunc.
             * </p>
             * <p>
             * While initialization, the adapter is first powered-off, setup and then powered-on.
             * </p>
             * <p>
             * Calling the method will allow close() to power-off the adapter,
             * if not powered on before.
             * </p>
             * @param btMode the desired adapter's BTMode, defaults to BTMode::DUAL
             * @return HCIStatusCode::SUCCESS or an error state on failure (e.g. power-on)
             * @see isInitialized()
             * @see close()
             * @see setPowered()
             * @since 2.4.0
             */
            HCIStatusCode initialize(const BTMode btMode=BTMode::DUAL) noexcept;

            /**
             * Returns true, if initialize() has already been called for this adapter, otherwise false.
             *
             * @see initialize()
             * @since 2.4.0
             */
            bool isInitialized() const noexcept { return adapter_initialized.load(); }

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
            HCIStatusCode reset() noexcept;

            /**
             * Sets default preference of LE_PHYs.
             *
             * BT Core Spec v5.2: Vol 4, Part E, 7.8.49 LE Set PHY command
             *
             * @param Tx transmitter LE_PHYs bit mask of preference if not set to LE_PHYs::NONE (ignored).
             * @param Rx receiver LE_PHYs bit mask of preference if not set to LE_PHYs::NONE (ignored).
             * @return
             * @see BTDevice::getTxPhys()
             * @see BTDevice::getRxPhys()
             * @see BTDevice::getConnectedLE_PHY()
             * @see BTDevice::setConnectedLE_PHY()
             * @since 2.4.0
             */
            HCIStatusCode setDefaultLE_PHY(const LE_PHYs Tx, const LE_PHYs Rx) noexcept;

            /**
             * Returns a reference to the used singleton BTManager instance, used to create this adapter.
             */
            const BTManagerRef& getManager() const noexcept { return mgmt; }

            /**
             * Returns a reference to the aggregated HCIHandler instance.
             */
            HCIHandler& getHCI() noexcept { return hci; }

            /**
             * Returns true, if the adapter's device is already whitelisted.
             */
            bool isDeviceWhitelisted(const BDAddressAndType & addressAndType) noexcept;

            /**
             * Add the given device to the adapter's autoconnect whitelist.
             * <p>
             * The given connection parameter will be uploaded to the kernel for the given device first.
             * </p>
             * <p>
             * Method will reject duplicate devices, in which case it should be removed first.
             * </p>
             *
             * @param address
             * @param address_type
             * @param ctype
             * @param conn_interval_min in units of 1.25ms, default value 12 for 15ms; Value range [6 .. 3200] for [7.5ms .. 4000ms]
             * @param conn_interval_max in units of 1.25ms, default value 12 for 15ms; Value range [6 .. 3200] for [7.5ms .. 4000ms]
             * @param conn_latency slave latency in units of connection events, default value 0; Value range [0 .. 0x01F3].
             * @param supervision_timeout in units of 10ms, default value >= 10 x conn_interval_max, we use HCIConstInt::LE_CONN_MIN_TIMEOUT_MS minimum; Value range [0xA-0x0C80] for [100ms - 32s].
             * @return true if the device was already added or has been newly added to the adapter's whitelist.
             */
            bool addDeviceToWhitelist(const BDAddressAndType & addressAndType,
                                      const HCIWhitelistConnectType ctype,
                                      const uint16_t conn_interval_min=12, const uint16_t conn_interval_max=12,
                                      const uint16_t conn_latency=0, const uint16_t supervision_timeout=getHCIConnSupervisorTimeout(0, 15));


            /** Remove the given device from the adapter's autoconnect whitelist. */
            bool removeDeviceFromWhitelist(const BDAddressAndType & addressAndType);

            // device discovery aka device scanning

            /**
             * Add the given listener to the list if not already present.
             * <p>
             * In case the AdapterStatusListener's lifecycle and event delivery
             * shall be constrained to this device, please use
             * BTDevice::addStatusListener().
             * </p>
             * <p>
             * Returns true if the given listener is not element of the list and has been newly added,
             * otherwise false.
             * </p>
             * <p>
             * The newly added AdapterStatusListener will receive an initial
             * AdapterStatusListener::adapterSettingsChanged(..) event,
             * passing an empty AdapterSetting::NONE oldMask and changedMask, as well as current AdapterSetting newMask. <br>
             * This allows the receiver to be aware of this adapter's current settings.
             * </p>
             * @see BTDevice::addStatusListener()
             * @see removeStatusListener()
             * @see removeAllStatusListener()
             */
            bool addStatusListener(const AdapterStatusListenerRef& l) noexcept;

            /**
             * Please use BTDevice::addStatusListener() for clarity, merely existing here to allow JNI access.
             */
            bool addStatusListener(const BTDeviceRef& d, const AdapterStatusListenerRef& l) noexcept;

            bool addStatusListener(const BTDevice& d, const AdapterStatusListenerRef& l) noexcept;

            /**
             * Remove the given listener from the list.
             * <p>
             * Returns true if the given listener is an element of the list and has been removed,
             * otherwise false.
             * </p>
             * @see BTDevice::removeStatusListener()
             * @see addStatusListener()
             */
            bool removeStatusListener(const AdapterStatusListenerRef& l) noexcept;

            /**
             * Remove the given listener from the list.
             * <p>
             * Returns true if the given listener is an element of the list and has been removed,
             * otherwise false.
             * </p>
             * @see BTDevice::removeStatusListener()
             * @see addStatusListener()
             */
            bool removeStatusListener(const AdapterStatusListener * l) noexcept;

            /**
             * Remove all status listener from the list.
             * <p>
             * Returns the number of removed status listener.
             * </p>
             */
            size_type removeAllStatusListener() noexcept;

            /**
             * Starts discovery.
             *
             * Returns HCIStatusCode::SUCCESS if successful, otherwise the HCIStatusCode error state;
             *
             * Depending on given DiscoveryPolicy `policy`, the discovery mode may be turned-off,
             * paused until a certain readiness stage has been reached or preserved at all times.
             * Default is DiscoveryPolicy::PAUSE_CONNECTED_UNTIL_READY.
             *
             * <pre>
             * + --+-------+--------+-----------+----------------------------------------------------+
             * | # | meta  | native | keepAlive | Note
             * +---+-------+--------+-----------+----------------------------------------------------+
             * | 1 | true  | true   | false     | -
             * | 2 | false | false  | false     | -
             * +---+-------+--------+-----------+----------------------------------------------------+
             * | 3 | true  | true   | true      | -
             * | 4 | true  | false  | true      | temporarily disabled -> startDiscoveryBackground()
             * | 5 | false | false  | true      | [4] -> [5] requires manual DISCOVERING event
             * +---+-------+--------+-----------+----------------------------------------------------+
             * </pre>
             *
             * Default parameter values are chosen for using public address resolution
             * and usual discovery intervals etc.
             *
             * Method will always clear previous discovered devices via removeDiscoveredDevices().
             *
             * Method fails if isAdvertising().
             *
             * If successful, method also changes [this adapter's role](@ref BTAdapterRoles) to ::BTRole::Master.
             *
             * This adapter's HCIHandler instance is used to initiate scanning,
             * see HCIHandler::le_start_scan().
             *
             * @param policy defaults to DiscoveryPolicy::PAUSE_CONNECTED_UNTIL_READY, see DiscoveryPolicy
             * @param le_scan_active true enables delivery of active scanning PDUs like EIR w/ device name (default), otherwise no scanning PDUs shall be sent.
             * @param le_scan_interval in units of 0.625ms, default value 24 for 15ms; Value range [4 .. 0x4000] for [2.5ms .. 10.24s]
             * @param le_scan_window in units of 0.625ms, default value 24 for 15ms; Value range [4 .. 0x4000] for [2.5ms .. 10.24s]. Shall be <= le_scan_interval
             * @param filter_policy 0x00 accepts all PDUs (default), 0x01 only of whitelisted, ...
             * @param filter_dup true to filter out duplicate AD PDUs (default), otherwise all will be reported.
             * @return HCIStatusCode::SUCCESS if successful, otherwise the HCIStatusCode error state
             * @see stopDiscovery()
             * @see isDiscovering()
             * @see isAdvertising()
             * @see DiscoveryPolicy
             * @see @ref BTAdapterRoles
             * @since 2.5.0
             */
            HCIStatusCode startDiscovery(const DiscoveryPolicy policy=DiscoveryPolicy::PAUSE_CONNECTED_UNTIL_READY,
                                         const bool le_scan_active=true,
                                         const uint16_t le_scan_interval=24, const uint16_t le_scan_window=24,
                                         const uint8_t filter_policy=0x00,
                                         const bool filter_dup=true) noexcept;

        private:
            HCIStatusCode stopDiscoveryImpl(const bool forceDiscoveringEvent, const bool temporary) noexcept;

        public:
            /**
             * Ends discovery.
             * <p>
             * This adapter's HCIHandler instance is used to stop scanning,
             * see HCIHandler::le_enable_scan().
             * </p>
             * @return HCIStatusCode::SUCCESS if successful, otherwise the HCIStatusCode error state
             * @see startDiscovery()
             * @see isDiscovering()
             */
            HCIStatusCode stopDiscovery() noexcept;

            /**
             * Return the current DiscoveryPolicy, set via startDiscovery().
             * @since 2.5.1
             */
            DiscoveryPolicy getCurrentDiscoveryPolicy() const noexcept { return discovery_policy; }

            /**
             * Manual DiscoveryPolicy intervention point, allowing user to remove the ready device from
             * the queue of pausing-discovery devices.
             *
             * Manual intervention might be desired, if using DiscoveryPolicy::PAUSE_CONNECTED_UNTIL_DISCONNECTED,
             * but allowing discovery at an earlier processing step from AdapterStatusListener::deviceReady().
             *
             * Re-enabling discovery is performed on the current thread.
             *
             * @param device the BTDevice to remove from the pausing-discovery queue
             * @return true if this was the last BTDevice, re-enabling discovery. Otherwise false.
             * @since 2.5.1
             */
            bool removeDevicePausingDiscovery(const BTDevice & device) noexcept;

            /**
             * Returns the current meta discovering ScanType. It can be modified through startDiscovery(..) and stopDiscovery().
             * <p>
             * Note that if startDiscovery(..) has been issued with keepAlive==true,
             * the meta ScanType will still keep the desired ScanType enabled
             * even if it has been temporarily disabled.
             * </p>
             * @see startDiscovery()
             * @see stopDiscovery()
             */
            ScanType getCurrentScanType() const noexcept {
                return currentMetaScanType;
            }

            /**
             * Returns the adapter's current native discovering ScanType.
             * It can be modified through startDiscovery(..) and stopDiscovery().
             * @see startDiscovery()
             * @see stopDiscovery()
             */
            ScanType getCurrentNativeScanType() const noexcept{
                return hci.getCurrentScanType();
            }

            /**
             * Returns true if the meta discovering state is not ::ScanType::NONE.
             * It can be modified through startDiscovery(..) and stopDiscovery().
             * @see startDiscovery()
             * @see stopDiscovery()
             * @since 2.4.0
             */
            bool isDiscovering() const noexcept {
                return ScanType::NONE != currentMetaScanType;
            }

            /**
             * Returns discovered devices from the last discovery.
             * <p>
             * Note that this list will be cleared when a new discovery is started over via startDiscovery().
             * </p>
             * <p>
             * Note that devices in this list might be no more available,
             * use 'DeviceStatusListener::deviceFound(..)' callback.
             * </p>
             */
            jau::darray<BTDeviceRef> getDiscoveredDevices() const noexcept;

            /** Discards all discovered devices. Returns number of removed discovered devices. */
            size_type removeDiscoveredDevices() noexcept;

            /** Discards matching discovered devices. Returns `true` if found and removed, otherwise false. */
            bool removeDiscoveredDevice(const BDAddressAndType & addressAndType) noexcept;

            /** Returns shared BTDevice if found, otherwise nullptr */
            BTDeviceRef findDiscoveredDevice (const EUI48 & address, const BDAddressType addressType) noexcept;

            /** Returns shared BTDevice if found, otherwise nullptr */
            BTDeviceRef findSharedDevice (const EUI48 & address, const BDAddressType addressType) noexcept;

            /**
             * Starts advertising
             * - BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.53 LE Set Extended Advertising Parameters command (Bluetooth 5.0)
             * - BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.54 LE Set Extended Advertising Data command (Bluetooth 5.0)
             * - BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.55 LE Set Extended Scan Response Data command (Bluetooth 5.0)
             * - BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.56 LE Set Extended Advertising Enable command (Bluetooth 5.0)
             *
             * if available, otherwise using
             *
             * - BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.5 LE Set Advertising Parameters command
             * - BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.7 LE Set Advertising Data command
             * - BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.8 LE Set Scan Response Data command
             * - BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.9 LE Set Advertising Enable command
             *
             * Method fails if isDiscovering() or has any open or pending connected remote {@link BTDevice}s.
             *
             * If successful, method also changes [this adapter's role](@ref BTAdapterRoles) to ::BTRole::Slave
             * and treat connected BTDevice as ::BTRole::Master while service ::GATTRole::Server.
             *
             * Advertising is active until either disabled via stopAdvertising() or a connection has been made, see isAdvertising().
             *
             * This adapter's HCIHandler instance is used to initiate scanning, see HCIHandler::le_start_adv().
             *
             * The given ADV EIR EInfoReport will be updated with getName() and at least GAPFlags::LE_Gen_Disc set.
             *
             * The given adv_mask and scanrsp_mask will be updated to have at least EIRDataType::FLAGS and EIRDataType::NAME
             * set in total.
             *
             * @param gattServerData_ the DBGattServer data to be advertised and offered via GattHandler as ::GATTRole::Server.
             *        Its handles will be setup via DBGattServer::setServicesHandles().
             *        Reference is held until next disconnect.
             * @param eir Full ADV EIR EInfoReport, will be updated with getName() and at least GAPFlags::LE_Gen_Disc set.
             * @param adv_mask EIRDataType mask for EInfoReport to select advertisement EIR PDU data, defaults to EIRDataType::FLAGS | EIRDataType::SERVICE_UUID
             * @param scanrsp_mask EIRDataType mask for EInfoReport to select scan-response (active scanning) EIR PDU data, defaults to EIRDataType::NAME | EIRDataType::CONN_IVAL
             * @param adv_interval_min in units of 0.625ms, default value 160 for 100ms; Value range [0x0020 .. 0x4000] for [20ms .. 10.24s]
             * @param adv_interval_max in units of 0.625ms, default value 480 for 300ms; Value range [0x0020 .. 0x4000] for [20ms .. 10.24s]
             * @param adv_type see AD_PDU_Type, default ::AD_PDU_Type::ADV_IND
             * @param adv_chan_map bit 0: chan 37, bit 1: chan 38, bit 2: chan 39, default is 0x07 (all 3 channels enabled)
             * @param filter_policy 0x00 accepts all PDUs (default), 0x01 only of whitelisted, ...
             * @return HCIStatusCode::SUCCESS if successful, otherwise the HCIStatusCode error state
             * @see stopAdvertising()
             * @see isAdvertising()
             * @see isDiscovering()
             * @see @ref BTAdapterRoles
             * @see DBGattServer::setServicesHandles()
             * @since 2.5.3
             */
            HCIStatusCode startAdvertising(DBGattServerRef gattServerData_,
                                           EInfoReport& eir,
                                           EIRDataType adv_mask = EIRDataType::FLAGS | EIRDataType::SERVICE_UUID,
                                           EIRDataType scanrsp_mask = EIRDataType::NAME | EIRDataType::CONN_IVAL,
                                           const uint16_t adv_interval_min=160, const uint16_t adv_interval_max=480,
                                           const AD_PDU_Type adv_type=AD_PDU_Type::ADV_IND,
                                           const uint8_t adv_chan_map=0x07,
                                           const uint8_t filter_policy=0x00) noexcept;

            /**
             * Starts advertising
             * - BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.53 LE Set Extended Advertising Parameters command (Bluetooth 5.0)
             * - BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.54 LE Set Extended Advertising Data command (Bluetooth 5.0)
             * - BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.55 LE Set Extended Scan Response Data command (Bluetooth 5.0)
             * - BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.56 LE Set Extended Advertising Enable command (Bluetooth 5.0)
             *
             * if available, otherwise using
             *
             * - BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.5 LE Set Advertising Parameters command
             * - BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.7 LE Set Advertising Data command
             * - BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.8 LE Set Scan Response Data command
             * - BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.9 LE Set Advertising Enable command
             *
             * Method fails if isDiscovering() or has any open or pending connected remote {@link BTDevice}s.
             *
             * If successful, method also changes [this adapter's role](@ref BTAdapterRoles) to ::BTRole::Slave
             * and treat connected BTDevice as ::BTRole::Master while service ::GATTRole::Server.
             *
             * Advertising is active until either disabled via stopAdvertising() or a connection has been made, see isAdvertising().
             *
             * This adapter's HCIHandler instance is used to initiate scanning, see HCIHandler::le_start_adv().
             *
             * The ADV EIR EInfoReport will be generated on the default
             * EIRDataType adv_mask using EIRDataType::FLAGS | EIRDataType::SERVICE_UUID
             * and EIRDataType scanrsp_mask using scan-response (active scanning) EIRDataType::NAME | EIRDataType::CONN_IVAL.
             *
             * @param gattServerData_ the DBGattServer data to be advertised and offered via GattHandler as ::GATTRole::Server.
             *        Its handles will be setup via DBGattServer::setServicesHandles().
             *        Reference is held until next disconnect.
             * @param adv_interval_min in units of 0.625ms, default value 160 for 100ms; Value range [0x0020 .. 0x4000] for [20ms .. 10.24s]
             * @param adv_interval_max in units of 0.625ms, default value 480 for 300ms; Value range [0x0020 .. 0x4000] for [20ms .. 10.24s]
             * @param adv_type see AD_PDU_Type, default ::AD_PDU_Type::ADV_IND
             * @param adv_chan_map bit 0: chan 37, bit 1: chan 38, bit 2: chan 39, default is 0x07 (all 3 channels enabled)
             * @param filter_policy 0x00 accepts all PDUs (default), 0x01 only of whitelisted, ...
             * @return HCIStatusCode::SUCCESS if successful, otherwise the HCIStatusCode error state
             * @see stopAdvertising()
             * @see isAdvertising()
             * @see isDiscovering()
             * @see @ref BTAdapterRoles
             * @see DBGattServer::setServicesHandles()
             * @since 2.4.0
             */
            HCIStatusCode startAdvertising(DBGattServerRef gattServerData_,
                                           const uint16_t adv_interval_min=160, const uint16_t adv_interval_max=480,
                                           const AD_PDU_Type adv_type=AD_PDU_Type::ADV_IND,
                                           const uint8_t adv_chan_map=0x07,
                                           const uint8_t filter_policy=0x00) noexcept;

            /**
             * Ends advertising.
             * - BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.56 LE Set Extended Advertising Enable command (Bluetooth 5.0)
             *
             * if available, otherwise using
             *
             * - BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.9 LE Set Advertising Enable command
             *
             * Advertising is active until either disabled via stopAdvertising() or a connection has been made, see isAdvertising().
             *
             * This adapter's HCIHandler instance is used to stop scanning, see HCIHandler::le_enable_adv().
             *
             * @return HCIStatusCode::SUCCESS if successful, otherwise the HCIStatusCode error state
             * @see startAdvertising()
             * @see isAdvertising()
             * @since 2.4.0
             */
            HCIStatusCode stopAdvertising() noexcept;

            /**
             * Returns the adapter's current advertising state. It can be modified through startAdvertising(..) and stopAdvertising().
             *
             * Advertising is active until either disabled via stopAdvertising() or a connection has been made.
             *
             * @see startAdvertising()
             * @see stopAdvertising()
             * @since 2.4.0
             */
            bool isAdvertising() const noexcept {
                return hci.isAdvertising();
            }

            /**
             * Return the user's DBGattServer shared reference if in ::BTRole::Slave mode
             * as set via startAdvertising() and valid until subsequent disconnect.
             *
             * Returns nullptr if in ::BTRole::Master mode.
             */
            DBGattServerRef getGATTServerData() { return gattServerData; }

            std::string toString() const noexcept override { return toString(false); }
            std::string toString(bool includeDiscoveredDevices) const noexcept;

            /**
             * Print the internally maintained BTDevice lists to stderr:
             * - sharedDevices
             * - connectedDevice
             * - discoveredDevices
             * - StatusListener
             *
             * This is intended as a debug facility.
             */
            void printDeviceLists() noexcept;

            void printStatusListenerList() noexcept;
    };

    inline bool operator==(const BTAdapter& lhs, const BTAdapter& rhs) noexcept
    { return lhs.getAddressAndType() == rhs.getAddressAndType(); }

    inline bool operator!=(const BTAdapter& lhs, const BTAdapter& rhs) noexcept
    { return !(lhs == rhs); }

    typedef std::shared_ptr<BTAdapter> BTAdapterRef;

    /**@}*/

} // namespace direct_bt

#endif /* BT_ADAPTER_HPP_ */
