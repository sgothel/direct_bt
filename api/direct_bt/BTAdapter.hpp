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

#include "BTTypes1.hpp"

#include "BTDevice.hpp"

#include "HCIHandler.hpp"

namespace direct_bt {

    class BTAdapter; // forward
    class BTManager; // forward

    /**
     * {@link BTAdapter} status listener for {@link BTDevice} discovery events: Added, updated and removed;
     * as well as for certain {@link BTAdapter} events.
     * <p>
     * User implementations shall return as early as possible to avoid blocking the event-handler thread,
     * if not specified within the methods otherwise (see AdapterStatusListener::deviceReady()).<br>
     * Especially complex mutable operations on BTDevice or BTAdapter should be issued off-thread!
     * </p>
     * <p>
     * A listener instance may be attached to a {@link BTAdapter} via
     * {@link BTAdapter::addStatusListener(std::shared_ptr<AdapterStatusListener>)}.
     * </p>
     * <p>
     * The listener receiver maintains a unique set of listener instances without duplicates.
     * </p>
     */
    class AdapterStatusListener {
        public:
            /**
             * Custom filter for all 'device*' notification methods,
             * which will not be called if this method returns false.
             * <p>
             * User may override this method to test whether the 'device*' methods shall be called
             * for the given device.
             * </p>
             * <p>
             * Defaults to true;
             * </p>
             */
            virtual bool matchDevice(const BTDevice & device) {
                (void)device;
                return true;
            }

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
             * @param changedType denotes the changed ScanType
             * @param changedEnabled denotes whether the changed ScanType has been enabled or disabled
             * @param keepAlive if `true`, the denoted changed ScanType will be re-enabled if disabled by the underlying Bluetooth implementation.
             * @param timestamp the time in monotonic milliseconds when this event occurred. See BasicTypes::getCurrentMilliseconds().
             *
             * changeScanType(const ScanType current, const bool enable, const ScanType enableChanged) noexcept {
             */
            virtual void discoveringChanged(BTAdapter &adapter, const ScanType currentMeta, const ScanType changedType, const bool changedEnabled, const bool keepAlive, const uint64_t timestamp) {
                (void)adapter;
                (void)currentMeta;
                (void)changedType;
                (void)changedEnabled;
                (void)keepAlive;
                (void)timestamp;
            }

            /**
             * A BTDevice has been newly discovered.
             * <p>
             * The boolean return value informs the adapter whether the device shall be made persistent for connection `true`,
             * or that it can be discarded `false`.<br>
             * If no registered AdapterStatusListener::deviceFound() implementation returns `true`,
             * the device instance will be removed from all internal lists and can no longer being used.<br>
             * If any registered AdapterStatusListener::deviceFound() implementation returns `true`,
             * the device will be made persistent, is ready to connect and BTDevice::remove() shall be called after usage.
             * </p>
             * @param device the found device
             * @param timestamp the time in monotonic milliseconds when this event occurred. See BasicTypes::getCurrentMilliseconds().
             * @return true if the device shall be made persistent and BTDevice::remove() issued later. Otherwise false to remove device right away.
             */
            virtual bool deviceFound(std::shared_ptr<BTDevice> device, const uint64_t timestamp) {
                (void)device;
                (void)timestamp;
                return false;
            }

            /**
             * An already discovered BTDevice has been updated.
             * @param device the updated device
             * @param updateMask the update mask of changed data
             * @param timestamp the time in monotonic milliseconds when this event occurred. See BasicTypes::getCurrentMilliseconds().
             */
            virtual void deviceUpdated(std::shared_ptr<BTDevice> device, const EIRDataType updateMask, const uint64_t timestamp) {
                (void)device;
                (void)updateMask;
                (void)timestamp;
            }

            /**
             * BTDevice got connected
             * @param device the device which has been connected, holding the new connection handle.
             * @param handle the new connection handle, which has been assigned to the device already
             * @param timestamp the time in monotonic milliseconds when this event occurred. See BasicTypes::getCurrentMilliseconds().
             */
            virtual void deviceConnected(std::shared_ptr<BTDevice> device, const uint16_t handle, const uint64_t timestamp) {
                (void)device;
                (void)handle;
                (void)timestamp;
            }

            /**
             * An already connected BTDevice's ::SMPPairingState has changed.
             * @param device the device which PairingMode has been changed.
             * @param state the current ::SMPPairingState of the connected device, see BTDevice::getCurrentPairingState()
             * @param mode the current ::PairingMode of the connected device, see BTDevice::getCurrentPairingMode()
             * @param timestamp the time in monotonic milliseconds when this event occurred. See BasicTypes::getCurrentMilliseconds().
             * @see BTDevice::setPairingPasskey()
             * @see BTDevice::setPairingNumericComparison()
             */
            virtual void devicePairingState(std::shared_ptr<BTDevice> device, const SMPPairingState state, const PairingMode mode, const uint64_t timestamp) {
                (void)device;
                (void)state;
                (void)mode;
                (void)timestamp;
            }

            /**
             * BTDevice is ready for user (GATT) processing, i.e. already connected, optionally paired and ATT MTU size negotiated via connected GATT.
             * <p>
             * Method is being called from a dedicated native thread, hence restrictions on method duration and complex mutable operations don't apply here.
             * </p>
             * @param device the device ready to use
             * @param timestamp the time in monotonic milliseconds when this event occurred. See BasicTypes::getCurrentMilliseconds().
             * @see ::SMPPairingState::COMPLETED
             */
            virtual void deviceReady(std::shared_ptr<BTDevice> device, const uint64_t timestamp) {
                (void)device;
                (void)timestamp;
            }

            /**
             * BTDevice got disconnected
             * @param device the device which has been disconnected with zeroed connection handle.
             * @param reason the HCIStatusCode reason for disconnection
             * @param handle the disconnected connection handle, which has been unassigned from the device already
             * @param timestamp the time in monotonic milliseconds when this event occurred. See BasicTypes::getCurrentMilliseconds().
             */
            virtual void deviceDisconnected(std::shared_ptr<BTDevice> device, const HCIStatusCode reason, const uint16_t handle, const uint64_t timestamp) {
                (void)device;
                (void)reason;
                (void)handle;
                (void)timestamp;
            }

            virtual ~AdapterStatusListener() {}

            virtual std::string toString() const = 0;

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

    // *************************************************
    // *************************************************
    // *************************************************

    namespace impl {
        struct StatusListenerPair {
            /** The actual listener */
            std::shared_ptr<AdapterStatusListener> listener;
            /** The optional weak device reference. Weak, b/c it shall not block destruction */
            std::weak_ptr<BTDevice> wbr_device;
        };
    }

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
     * <pre>
     * - 'direct_bt.debug.adapter.event': Debug messages about events, see debug_events
     * </pre>
     *
     * @see BTDevice
     * @see [BTDevice roles](@ref BTDeviceRoles).
     * @see [BTGattHandler roles](@ref BTGattHandlerRoles).
     * @see [Bluetooth Specification](https://www.bluetooth.com/specifications/bluetooth-core-specification/)
     */
    class BTAdapter : public BTObject
    {
        private:
            friend BTManager;

            const bool debug_event, debug_lock;
            BTManager& mgmt;
            AdapterInfo adapterInfo;
            jau::sc_atomic_bool adapter_initialized;
            LE_Features le_features;
            bool hci_uses_ext_scan;
            bool hci_uses_ext_conn;

            /**
             * Either the adapter's initially reported public address or a random address setup via HCI before discovery or advertising.
             */
            BDAddressAndType visibleAddressAndType;

        public:
            /**
             * Adapter's internal temporary device id.
             * <p>
             * The internal device id is constant across the adapter lifecycle,
             * but may change after its destruction.
             */
            const uint16_t dev_id;

        private:
            std::atomic<BTRole> btRole; // = BTRole::Master (default)
            HCIHandler hci;

            std::atomic<AdapterSetting> old_settings;
            std::atomic<ScanType> currentMetaScanType; // = ScanType::NONE
            std::atomic<bool> keep_le_scan_alive; //  = false;

            SMPIOCapability  iocap_defaultval = SMPIOCapability::UNSET;
            const BTDevice* single_conn_device_ptr = nullptr;
            std::mutex mtx_single_conn_device;
            std::condition_variable cv_single_conn_device;

            typedef jau::darray<std::shared_ptr<BTDevice>> device_list_t;
            /** All discovered devices: Transient until removeDiscoveredDevices(), startDiscovery(). */
            device_list_t discoveredDevices;
            /** All connected devices: Transient until disconnect or removal. */
            device_list_t connectedDevices;
            /** All persistent shared devices: Persistent until removal. */
            device_list_t sharedDevices; // All active shared devices. Final holder of BTDevice lifecycle!
            typedef jau::cow_darray<impl::StatusListenerPair> statusListenerList_t;
            statusListenerList_t statusListenerList;
            mutable std::mutex mtx_discoveredDevices;
            mutable std::mutex mtx_connectedDevices;
            mutable std::mutex mtx_discovery;
            mutable std::mutex mtx_sharedDevices; // final mutex of all BTDevice lifecycle
            mutable jau::sc_atomic_bool sync_data;

            bool updateDataFromHCI() noexcept;
            bool updateDataFromAdapterInfo() noexcept;
            bool initialSetup() noexcept;

            static std::shared_ptr<BTDevice> findDevice(device_list_t & devices, const EUI48 & address, const BDAddressType addressType) noexcept;
            static std::shared_ptr<BTDevice> findDevice(device_list_t & devices, BTDevice const & device) noexcept;
            static void printDeviceList(const std::string& prefix, const BTAdapter::device_list_t& list) noexcept;

            /** Private class only for private make_shared(). */
            class ctor_cookie { friend BTAdapter; ctor_cookie(const uint16_t secret) { (void)secret; } };

            /** Private std::make_shared<BTAdapter>(..) vehicle for friends. */
            static std::shared_ptr<BTAdapter> make_shared(BTManager& mgmt_, const AdapterInfo& adapterInfo_) {
                return std::make_shared<BTAdapter>(BTAdapter::ctor_cookie(0), mgmt_, adapterInfo_);
            }

            /**
             * Closes all device connections, stops discovery and cleans up all references.
             * <p>
             * To be called at
             *
             * - destructor or when powered off (active = true)
             * - when `!isPowered()` is detected in methods (active = false)
             * </p>
             * @param active true to actively stopDiscovery and disconnect devices etc, otherwise this is an all passive operation
             */
            void poweredOff(const bool active) noexcept;

            friend std::shared_ptr<BTDevice> BTDevice::getSharedInstance() const noexcept;
            friend void BTDevice::remove() noexcept;
            friend BTDevice::~BTDevice() noexcept;

            friend std::shared_ptr<ConnectionInfo> BTDevice::getConnectionInfo() noexcept;
            friend void BTDevice::sendMgmtEvDeviceDisconnected(std::unique_ptr<MgmtEvent> evt) noexcept;
            friend HCIStatusCode BTDevice::disconnect(const HCIStatusCode reason) noexcept;
            friend HCIStatusCode BTDevice::connectLE(uint16_t interval, uint16_t window,
                                                      uint16_t min_interval, uint16_t max_interval,
                                                      uint16_t latency, uint16_t supervision_timeout) noexcept;
            friend HCIStatusCode BTDevice::connectBREDR(const uint16_t pkt_type, const uint16_t clock_offset, const uint8_t role_switch) noexcept;
            friend void BTDevice::processL2CAPSetup(std::shared_ptr<BTDevice> sthis);
            friend bool BTDevice::updatePairingState(std::shared_ptr<BTDevice> sthis, const MgmtEvent& evt, const HCIStatusCode evtStatus, SMPPairingState claimed_state) noexcept;
            friend void BTDevice::hciSMPMsgCallback(std::shared_ptr<BTDevice> sthis, const SMPPDUMsg& msg, const HCIACLData::l2cap_frame& source) noexcept;
            friend void BTDevice::processDeviceReady(std::shared_ptr<BTDevice> sthis, const uint64_t timestamp);
            friend jau::darray<std::shared_ptr<BTGattService>> BTDevice::getGattServices() noexcept;

            bool lockConnect(const BTDevice & device, const bool wait, const SMPIOCapability io_cap) noexcept;
            bool unlockConnect(const BTDevice & device) noexcept;
            bool unlockConnectAny() noexcept;

            bool addConnectedDevice(const std::shared_ptr<BTDevice> & device) noexcept;
            bool removeConnectedDevice(const BTDevice & device) noexcept;
            int disconnectAllDevices(const HCIStatusCode reason=HCIStatusCode::REMOTE_USER_TERMINATED_CONNECTION ) noexcept;
            std::shared_ptr<BTDevice> findConnectedDevice (const EUI48 & address, const BDAddressType & addressType) noexcept;
            int getConnectedDeviceCount() const noexcept;

            bool addDiscoveredDevice(std::shared_ptr<BTDevice> const &device) noexcept;

            void removeDevice(BTDevice & device) noexcept;

            bool addSharedDevice(std::shared_ptr<BTDevice> const &device) noexcept;
            std::shared_ptr<BTDevice> getSharedDevice(const BTDevice & device) noexcept;
            void removeSharedDevice(const BTDevice & device) noexcept;

            bool mgmtEvNewSettingsMgmt(const MgmtEvent& e) noexcept;
            bool updateAdapterSettings(const AdapterSetting new_settings, const bool sendEvent, const uint64_t timestamp) noexcept;
            bool mgmtEvDeviceDiscoveringMgmt(const MgmtEvent& e) noexcept;
            bool mgmtEvLocalNameChangedMgmt(const MgmtEvent& e) noexcept;
            bool mgmtEvDeviceFoundHCI(const MgmtEvent& e) noexcept;
            bool mgmtEvDeviceDisconnectedMgmt(const MgmtEvent& e) noexcept;
            bool mgmtEvPairDeviceCompleteMgmt(const MgmtEvent& e) noexcept;
            bool mgmtEvNewLongTermKeyMgmt(const MgmtEvent& e) noexcept;

            bool mgmtEvDeviceDiscoveringHCI(const MgmtEvent& e) noexcept;
            bool mgmtEvDeviceConnectedHCI(const MgmtEvent& e) noexcept;
            bool mgmtEvConnectFailedHCI(const MgmtEvent& e) noexcept;
            bool mgmtEvHCIEncryptionChangedHCI(const MgmtEvent& e) noexcept;
            bool mgmtEvHCIEncryptionKeyRefreshCompleteHCI(const MgmtEvent& e) noexcept;
            bool mgmtEvHCILERemoteUserFeaturesHCI(const MgmtEvent& e) noexcept;
            bool mgmtEvDeviceDisconnectedHCI(const MgmtEvent& e) noexcept;


            bool mgmtEvDeviceDiscoveringAny(const MgmtEvent& e, const bool hciSourced) noexcept;

            bool mgmtEvPinCodeRequestMgmt(const MgmtEvent& e) noexcept;
            bool mgmtEvUserConfirmRequestMgmt(const MgmtEvent& e) noexcept;
            bool mgmtEvUserPasskeyRequestMgmt(const MgmtEvent& e) noexcept;
            bool mgmtEvAuthFailedMgmt(const MgmtEvent& e) noexcept;
            bool mgmtEvDeviceUnpairedMgmt(const MgmtEvent& e) noexcept;

            bool hciSMPMsgCallback(const BDAddressAndType & addressAndType,
                                   const SMPPDUMsg& msg, const HCIACLData::l2cap_frame& source) noexcept;
            void sendDevicePairingState(std::shared_ptr<BTDevice> device, const SMPPairingState state, const PairingMode mode, uint64_t timestamp) noexcept;
            void sendDeviceReady(std::shared_ptr<BTDevice> device, uint64_t timestamp) noexcept;

            void startDiscoveryBackground() noexcept;
            void checkDiscoveryState() noexcept;

            void sendAdapterSettingsChanged(const AdapterSetting old_settings_, const AdapterSetting current_settings, AdapterSetting changes,
                                            const uint64_t timestampMS) noexcept;
            void sendAdapterSettingsInitial(AdapterStatusListener & asl, const uint64_t timestampMS) noexcept;

            void sendDeviceUpdated(std::string cause, std::shared_ptr<BTDevice> device, uint64_t timestamp, EIRDataType updateMask) noexcept;

            int removeAllStatusListener(const BTDevice& d);

        public:

            /** Private ctor for private BTAdapter::make_shared() intended for friends. */
            BTAdapter(const BTAdapter::ctor_cookie& cc, BTManager& mgmt_, const AdapterInfo& adapterInfo_) noexcept;

            BTAdapter(const BTAdapter&) = delete;
            void operator=(const BTAdapter&) = delete;

            /**
             * Releases this instance.
             */
            ~BTAdapter() noexcept;

            /**
             * Closes this instance, usually being called by destructor or when this adapter is being removed
             * as recognized and handled by BTManager.
             * <p>
             * In case initialize() or setPowered(true) has been called, i.e. this adapter has been powered-on by the user,
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

            /** Returns true if HCI_LE_Set_Extended_Scan_Parameters and HCI_LE_Set_Extended_Scan_Enable is supported (Bluetooth 5.0). */
            constexpr bool hasHCIExtScan() const noexcept { return hci_uses_ext_scan; }

            /** Returns true if HCI_LE_Extended_Create_Connection is supported (Bluetooth 5.0). */
            constexpr bool hasHCIExtConn() const noexcept { return hci_uses_ext_conn; }

            /**
             * Returns whether the adapter is valid, i.e. reference is valid, plugged in and generally operational,
             * but not necessarily BTAdapter::isPowered() powered.
             * @return true if this adapter references are valid and hasn't been BTAdapter::close() 'ed
             * @see #isPowered()
             * @see #isSuspended()
             */
            bool isValid() const noexcept {
                return BTObject::isValid();
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
             * <p>
             * Shall be performed while powered-off.
             * </p>
             * <p>
             * The corresponding management event will change the name and short-name.
             * </p>
             * @see getName()
             * @see getShortName()
             * @since 2.4.0
             */
            HCIStatusCode setName(const std::string &name, const std::string &short_name) noexcept;

            /**
             * Set the discoverable state of the adapter.
             */
            bool setDiscoverable(bool value) noexcept;

            /**
             * Set the bondable (aka pairable) state of the adapter.
             */
            bool setBondable(bool value) noexcept;

            /**
             * Set the power state of the adapter.
             * <p>
             * Calling the method to power-on this adapter will allow close() to power-off the adapter.
             * </p>
             * @param value true will power on this adapter, otherwise this adapter will be powered-off.
             * @return true for success
             * @see close()
             * @see initialize()
             */
            bool setPowered(bool value) noexcept;

            /**
             * Initialize the adapter with default values, including power-on.
             * <p>
             * Method shall be issued on the desired adapter found via ChangedAdapterSetFunc.
             * </p>
             * <p>
             * While initialization, the adapter is first powered-off, setup and then powered-on.
             * </p>
             * <p>
             * Calling the method will allow close() to power-off the adapter.
             * </p>
             * @param btMode the desired adapter's BTMode, defaults to BTMode::DUAL
             * @return HCIStatusCode::SUCCESS or an error state
             * @see close()
             * @see setPowered()
             * @since 2.4.0
             */
            HCIStatusCode initialize(const BTMode btMode=BTMode::DUAL) noexcept;

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
             * Returns a reference to the used singleton BTManager instance, used to create this adapter.
             */
            BTManager& getManager() const noexcept { return mgmt; }

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
            bool addStatusListener(std::shared_ptr<AdapterStatusListener> l);

            /**
             * Please use BTDevice::addStatusListener() for clarity, merely existing here to allow JNI access.
             */
            bool addStatusListener(const BTDevice& d, std::shared_ptr<AdapterStatusListener> l);

            /**
             * Remove the given listener from the list.
             * <p>
             * Returns true if the given listener is an element of the list and has been removed,
             * otherwise false.
             * </p>
             * @see BTDevice::removeStatusListener()
             * @see addStatusListener()
             */
            bool removeStatusListener(std::shared_ptr<AdapterStatusListener> l);

            /**
             * Remove the given listener from the list.
             * <p>
             * Returns true if the given listener is an element of the list and has been removed,
             * otherwise false.
             * </p>
             * @see BTDevice::removeStatusListener()
             * @see addStatusListener()
             */
            bool removeStatusListener(const AdapterStatusListener * l);

            /**
             * Remove all status listener from the list.
             * <p>
             * Returns the number of removed status listener.
             * </p>
             */
            int removeAllStatusListener();

            /**
             * Starts discovery.
             *
             * Returns HCIStatusCode::SUCCESS if successful, otherwise the HCIStatusCode error state;
             *
             * if `keepAlive` is `true`, discovery state will be re-enabled
             * in case the underlying Bluetooth implementation disables it.
             * Default is `true`.
             *
             * Using startDiscovery(keepAlive=true) and stopDiscovery()
             * is the recommended workflow for a reliable discovery process.
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
             * @param keepAlive
             * @param le_scan_active true enables delivery of active scanning PDUs, otherwise no scanning PDUs shall be sent (default)
             * @param le_scan_interval in units of 0.625ms, default value 24 for 15ms; Value range [4 .. 0x4000] for [2.5ms .. 10.24s]
             * @param le_scan_window in units of 0.625ms, default value 24 for 15ms; Value range [4 .. 0x4000] for [2.5ms .. 10.24s]. Shall be <= le_scan_interval
             * @param filter_policy 0x00 accepts all PDUs (default), 0x01 only of whitelisted, ...
             * @return HCIStatusCode::SUCCESS if successful, otherwise the HCIStatusCode error state
             * @see stopDiscovery()
             * @see isDiscovering()
             * @see isAdvertising()
             * @see @ref BTAdapterRoles
             */
            HCIStatusCode startDiscovery(const bool keepAlive=true,
                                         const bool le_scan_active=false,
                                         const uint16_t le_scan_interval=24, const uint16_t le_scan_window=24,
                                         const uint8_t filter_policy=0x00);

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
            jau::darray<std::shared_ptr<BTDevice>> getDiscoveredDevices() const noexcept;

            /** Discards all discovered devices. Returns number of removed discovered devices. */
            int removeDiscoveredDevices() noexcept;

            /** Discards matching discovered devices. Returns `true` if found and removed, otherwise false. */
            bool removeDiscoveredDevice(const BDAddressAndType & addressAndType) noexcept;

            /** Returns shared BTDevice if found, otherwise nullptr */
            std::shared_ptr<BTDevice> findDiscoveredDevice (const EUI48 & address, const BDAddressType addressType) noexcept;

            /** Returns shared BTDevice if found, otherwise nullptr */
            std::shared_ptr<BTDevice> findSharedDevice (const EUI48 & address, const BDAddressType addressType) noexcept;

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
             *
             * Method fails if isDiscovering() or has any open or pending connected remote {@link BTDevice}s.
             *
             *
             * If successful, method also changes [this adapter's role](@ref BTAdapterRoles) to ::BTRole::Slave.
             *
             *
             * This adapter's HCIHandler instance is used to initiate scanning,
             * see HCIHandler::le_start_adv().
             *
             *
             * TODO:
             * - Random address for privacy if desired!
             * - Consider SMP (security)
             *
             * @param adv_interval_min in units of 0.625ms, default value 0x0800 for 1.28s; Value range [0x0020 .. 0x4000] for [20ms .. 10.24s]
             * @param adv_interval_max in units of 0.625ms, default value 0x0800 for 1.28s; Value range [0x0020 .. 0x4000] for [20ms .. 10.24s]
             * @param adv_type see AD_PDU_Type, default ::AD_PDU_Type::ADV_IND
             * @param adv_chan_map bit 0: chan 37, bit 1: chan 38, bit 2: chan 39, default is 0x07 (all 3 channels enabled)
             * @param filter_policy 0x00 accepts all PDUs (default), 0x01 only of whitelisted, ...
             * @return HCIStatusCode::SUCCESS if successful, otherwise the HCIStatusCode error state
             * @see stopAdvertising()
             * @see isAdvertising()
             * @see isDiscovering()
             * @see @ref BTAdapterRoles
             * @since 2.4.0
             */
            HCIStatusCode startAdvertising(const uint16_t adv_interval_min=0x0800, const uint16_t adv_interval_max=0x0800,
                                           const AD_PDU_Type adv_type=AD_PDU_Type::ADV_IND,
                                           const uint8_t adv_chan_map=0x07,
                                           const uint8_t filter_policy=0x00) noexcept;

            /**
             * Ends advertising.
             * <pre>
             * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.56 LE Set Extended Advertising Enable command (Bluetooth 5.0)
             *
             * if available, otherwise using
             *
             * BT Core Spec v5.2: Vol 4 HCI, Part E HCI Functional: 7.8.9 LE Set Advertising Enable command
             * </pre>
             * <p>
             * This adapter's HCIHandler instance is used to stop scanning,
             * see HCIHandler::le_enable_adv().
             * </p>
             * @return HCIStatusCode::SUCCESS if successful, otherwise the HCIStatusCode error state
             * @see startAdvertising()
             * @see isAdvertising()
             * @since 2.4.0
             */
            HCIStatusCode stopAdvertising() noexcept;

            /**
             * Returns the adapter's current advertising state. It can be modified through startAdvertising(..) and stopAdvertising().
             * @see startAdvertising()
             * @see stopAdvertising()
             * @since 2.4.0
             */
            bool isAdvertising() const noexcept {
                return hci.isAdvertising();
            }

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

} // namespace direct_bt

#endif /* BT_ADAPTER_HPP_ */
