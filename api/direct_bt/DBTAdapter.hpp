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

#ifndef DBT_ADAPTER_HPP_
#define DBT_ADAPTER_HPP_

#include <cstring>
#include <string>
#include <memory>
#include <cstdint>
#include <vector>

#include <mutex>
#include <atomic>

#include <jau/cow_vector.hpp>

#include "DBTTypes.hpp"

#include "DBTDevice.hpp"

#include "HCIHandler.hpp"
#include "DBTManager.hpp"

namespace direct_bt {

    class DBTAdapter; // forward

    /**
     * {@link DBTAdapter} status listener for {@link DBTDevice} discovery events: Added, updated and removed;
     * as well as for certain {@link DBTAdapter} events.
     * <p>
     * User implementations shall return as early as possible to avoid blocking the event-handler thread,
     * if not specified within the methods otherwise (see AdapterStatusListener::deviceReady()).<br>
     * Especially complex mutable operations on DBTDevice or DBTAdapter should be issued off-thread!
     * </p>
     * <p>
     * A listener instance may be attached to a {@link DBTAdapter} via
     * {@link DBTAdapter::addStatusListener(std::shared_ptr<AdapterStatusListener>)}.
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
            virtual bool matchDevice(const DBTDevice & device) {
                (void)device;
                return true;
            }

            /**
             * DBTAdapter setting(s) changed.
             * @param adapter the adapter which settings have changed.
             * @param oldmask the previous settings mask. AdapterSetting::NONE indicates the initial setting notification, see DBTAdapter::addStatusListener().
             * @param newmask the new settings mask
             * @param changedmask the changes settings mask. AdapterSetting::NONE indicates the initial setting notification, see DBTAdapter::addStatusListener().
             * @param timestamp the time in monotonic milliseconds when this event occurred. See BasicTypes::getCurrentMilliseconds().
             */
            virtual void adapterSettingsChanged(DBTAdapter &adapter, const AdapterSetting oldmask, const AdapterSetting newmask,
                                                const AdapterSetting changedmask, const uint64_t timestamp) = 0;

            /**
             * DBTAdapter's discovery state has changed, i.e. enabled or disabled.
             * @param adapter the adapter which discovering state has changed.
             * @param currentMeta the current meta ScanType
             * @param changedType denotes the changed ScanType
             * @param changedEnabled denotes whether the changed ScanType has been enabled or disabled
             * @param keepAlive if {@code true}, the denoted changed ScanType will be re-enabled if disabled by the underlying Bluetooth implementation.
             * @param timestamp the time in monotonic milliseconds when this event occurred. See BasicTypes::getCurrentMilliseconds().
             *
             * changeScanType(const ScanType current, const bool enable, const ScanType enableChanged) noexcept {
             */
            virtual void discoveringChanged(DBTAdapter &adapter, const ScanType currentMeta, const ScanType changedType, const bool changedEnabled, const bool keepAlive, const uint64_t timestamp) = 0;

            /**
             * A DBTDevice has been newly discovered.
             * @param device the found device
             * @param timestamp the time in monotonic milliseconds when this event occurred. See BasicTypes::getCurrentMilliseconds().
             */
            virtual void deviceFound(std::shared_ptr<DBTDevice> device, const uint64_t timestamp) = 0;

            /**
             * An already discovered DBTDevice has been updated.
             * @param device the updated device
             * @param updateMask the update mask of changed data
             * @param timestamp the time in monotonic milliseconds when this event occurred. See BasicTypes::getCurrentMilliseconds().
             */
            virtual void deviceUpdated(std::shared_ptr<DBTDevice> device, const EIRDataType updateMask, const uint64_t timestamp) = 0;

            /**
             * DBTDevice got connected
             * @param device the device which has been connected, holding the new connection handle.
             * @param handle the new connection handle, which has been assigned to the device already
             * @param timestamp the time in monotonic milliseconds when this event occurred. See BasicTypes::getCurrentMilliseconds().
             */
            virtual void deviceConnected(std::shared_ptr<DBTDevice> device, const uint16_t handle, const uint64_t timestamp) = 0;

            /**
             * An already connected DBTDevice's ::SMPPairingState has changed.
             * @param device the device which PairingMode has been changed.
             * @param state the current ::SMPPairingState of the connected device, see DBTDevice::getCurrentPairingState()
             * @param mode the current ::PairingMode of the connected device, see DBTDevice::getCurrentPairingMode()
             * @param timestamp the time in monotonic milliseconds when this event occurred. See BasicTypes::getCurrentMilliseconds().
             * @see DBTDevice::setPairingPasskey()
             * @see DBTDevice::setPairingNumericComparison()
             */
            virtual void devicePairingState(std::shared_ptr<DBTDevice> device, const SMPPairingState state, const PairingMode mode, const uint64_t timestamp) = 0;

            /**
             * DBTDevice is ready for user (GATT) processing, i.e. already connected, optionally paired and ATT MTU size negotiated via connected GATT.
             * <p>
             * Method is being called from a dedicated native thread, hence restrictions on method duration and complex mutable operations don't apply here.
             * </p>
             * @param device the device ready to use
             * @param timestamp the time in monotonic milliseconds when this event occurred. See BasicTypes::getCurrentMilliseconds().
             * @see ::SMPPairingState::COMPLETED
             */
            virtual void deviceReady(std::shared_ptr<DBTDevice> device, const uint64_t timestamp) = 0;

            /**
             * DBTDevice got disconnected
             * @param device the device which has been disconnected with zeroed connection handle.
             * @param reason the HCIStatusCode reason for disconnection
             * @param handle the disconnected connection handle, which has been unassigned from the device already
             * @param timestamp the time in monotonic milliseconds when this event occurred. See BasicTypes::getCurrentMilliseconds().
             */
            virtual void deviceDisconnected(std::shared_ptr<DBTDevice> device, const HCIStatusCode reason, const uint16_t handle, const uint64_t timestamp) = 0;

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

    /**
     * DBTAdapter represents one Bluetooth Controller.
     * <p>
     * Controlling Environment variables:
     * <pre>
     * - 'direct_bt.debug.adapter.event': Debug messages about events, see debug_events
     * </pre>
     * </p>
     */
    class DBTAdapter : public DBTObject
    {
        private:
            const bool debug_event, debug_lock;
            DBTManager& mgmt;

        public:
            /**
             * Adapter's internal temporary device id.
             * <p>
             * The internal device id is constant across the adapter lifecycle,
             * but may change after its destruction.
             */
            const int dev_id;

        private:
            HCIHandler hci;

            std::atomic<AdapterSetting> old_settings;
            std::shared_ptr<AdapterInfo> adapterInfo;
            std::atomic<BTMode> btMode = BTMode::NONE;
            NameAndShortName localName;
            std::atomic<ScanType> currentMetaScanType; // = ScanType::NONE
            std::atomic<bool> keep_le_scan_alive; //  = false;

            SMPIOCapability  iocap_defaultval = SMPIOCapability::UNSET;
            const DBTDevice* single_conn_device_ptr = nullptr;
            std::mutex mtx_single_conn_device;
            std::condition_variable cv_single_conn_device;

            std::vector<std::shared_ptr<DBTDevice>> connectedDevices;
            std::vector<std::shared_ptr<DBTDevice>> discoveredDevices; // all discovered devices
            std::vector<std::shared_ptr<DBTDevice>> sharedDevices; // All active shared devices. Final holder of DBTDevice lifecycle!
            jau::cow_vector<std::shared_ptr<AdapterStatusListener>> statusListenerList;
            std::mutex mtx_discoveredDevices;
            std::mutex mtx_connectedDevices;
            std::mutex mtx_discovery;
            std::mutex mtx_sharedDevices; // final mutex of all DBTDevice lifecycle

            bool validateDevInfo() noexcept;

            /** Returns index >= 0 if found, otherwise -1 */
            static int findDeviceIdx(std::vector<std::shared_ptr<DBTDevice>> & devices, EUI48 const & mac, const BDAddressType macType) noexcept;
            static std::shared_ptr<DBTDevice> findDevice(std::vector<std::shared_ptr<DBTDevice>> & devices, EUI48 const & mac, const BDAddressType macType) noexcept;
            std::shared_ptr<DBTDevice> findDevice(std::vector<std::shared_ptr<DBTDevice>> & devices, DBTDevice const & device) noexcept;

            /**
             * Closes all device connections, stops discovery and cleans up all references.
             * <p>
             * To be called at destructor or when powered off.
             * </p>
             */
            void poweredOff() noexcept;

            friend std::shared_ptr<DBTDevice> DBTDevice::getSharedInstance() const noexcept;
            friend std::shared_ptr<ConnectionInfo> DBTDevice::getConnectionInfo() noexcept;
            friend HCIStatusCode DBTDevice::disconnect(const HCIStatusCode reason) noexcept;
            friend void DBTDevice::remove() noexcept;
            friend HCIStatusCode DBTDevice::connectLE(uint16_t interval, uint16_t window,
                                                      uint16_t min_interval, uint16_t max_interval,
                                                      uint16_t latency, uint16_t supervision_timeout);
            friend HCIStatusCode DBTDevice::connectBREDR(const uint16_t pkt_type, const uint16_t clock_offset, const uint8_t role_switch);
            friend void DBTDevice::processL2CAPSetup(std::shared_ptr<DBTDevice> sthis);
            friend bool DBTDevice::updatePairingState(std::shared_ptr<DBTDevice> sthis, std::shared_ptr<MgmtEvent> evt, const HCIStatusCode evtStatus, SMPPairingState claimed_state) noexcept;
            friend void DBTDevice::hciSMPMsgCallback(std::shared_ptr<DBTDevice> sthis, std::shared_ptr<const SMPPDUMsg> msg, const HCIACLData::l2cap_frame& source) noexcept;
            friend void DBTDevice::processDeviceReady(std::shared_ptr<DBTDevice> sthis, const uint64_t timestamp);
            friend std::vector<std::shared_ptr<GATTService>> DBTDevice::getGATTServices() noexcept;

            bool lockConnect(const DBTDevice & device, const bool wait, const SMPIOCapability io_cap) noexcept;
            bool unlockConnect(const DBTDevice & device) noexcept;
            bool unlockConnectAny() noexcept;

            bool addConnectedDevice(const std::shared_ptr<DBTDevice> & device) noexcept;
            bool removeConnectedDevice(const DBTDevice & device) noexcept;
            int disconnectAllDevices(const HCIStatusCode reason=HCIStatusCode::REMOTE_USER_TERMINATED_CONNECTION ) noexcept;
            std::shared_ptr<DBTDevice> findConnectedDevice (EUI48 const & mac, const BDAddressType macType) noexcept;

            bool addDiscoveredDevice(std::shared_ptr<DBTDevice> const &device) noexcept;
            bool removeDiscoveredDevice(const DBTDevice & device) noexcept;

            void removeDevice(DBTDevice & device) noexcept;

            bool addSharedDevice(std::shared_ptr<DBTDevice> const &device) noexcept;
            std::shared_ptr<DBTDevice> getSharedDevice(const DBTDevice & device) noexcept;
            void removeSharedDevice(const DBTDevice & device) noexcept;
            std::shared_ptr<DBTDevice> findSharedDevice (EUI48 const & mac, const BDAddressType macType) noexcept;

            bool mgmtEvNewSettingsMgmt(std::shared_ptr<MgmtEvent> e) noexcept;
            bool mgmtEvDeviceDiscoveringMgmt(std::shared_ptr<MgmtEvent> e) noexcept;
            bool mgmtEvLocalNameChangedMgmt(std::shared_ptr<MgmtEvent> e) noexcept;
            bool mgmtEvDeviceFoundHCI(std::shared_ptr<MgmtEvent> e) noexcept;
            bool mgmtEvDeviceDisconnectedMgmt(std::shared_ptr<MgmtEvent> e) noexcept;
            bool mgmtEvPairDeviceCompleteMgmt(std::shared_ptr<MgmtEvent> e) noexcept;
            bool mgmtEvNewLongTermKeyMgmt(std::shared_ptr<MgmtEvent> e) noexcept;

            bool mgmtEvDeviceDiscoveringHCI(std::shared_ptr<MgmtEvent> e) noexcept;
            bool mgmtEvDeviceConnectedHCI(std::shared_ptr<MgmtEvent> e) noexcept;
            bool mgmtEvConnectFailedHCI(std::shared_ptr<MgmtEvent> e) noexcept;
            bool mgmtEvHCIEncryptionChangedHCI(std::shared_ptr<MgmtEvent> e) noexcept;
            bool mgmtEvHCIEncryptionKeyRefreshCompleteHCI(std::shared_ptr<MgmtEvent> e) noexcept;
            bool mgmtEvHCILERemoteUserFeaturesHCI(std::shared_ptr<MgmtEvent> e) noexcept;
            bool mgmtEvDeviceDisconnectedHCI(std::shared_ptr<MgmtEvent> e) noexcept;


            bool mgmtEvDeviceDiscoveringAny(std::shared_ptr<MgmtEvent> e, const bool hciSourced) noexcept;

            bool mgmtEvPinCodeRequestMgmt(std::shared_ptr<MgmtEvent> e) noexcept;
            bool mgmtEvUserConfirmRequestMgmt(std::shared_ptr<MgmtEvent> e) noexcept;
            bool mgmtEvUserPasskeyRequestMgmt(std::shared_ptr<MgmtEvent> e) noexcept;
            bool mgmtEvAuthFailedMgmt(std::shared_ptr<MgmtEvent> e) noexcept;
            bool mgmtEvDeviceUnpairedMgmt(std::shared_ptr<MgmtEvent> e) noexcept;

            bool hciSMPMsgCallback(const EUI48& address, BDAddressType addressType,
                                   std::shared_ptr<const SMPPDUMsg> msg, const HCIACLData::l2cap_frame& source) noexcept;
            void sendDevicePairingState(std::shared_ptr<DBTDevice> device, const SMPPairingState state, const PairingMode mode, uint64_t timestamp) noexcept;
            void sendDeviceReady(std::shared_ptr<DBTDevice> device, uint64_t timestamp) noexcept;

            void startDiscoveryBackground() noexcept;
            void checkDiscoveryState() noexcept;

            void sendAdapterSettingsChanged(const AdapterSetting old_settings_, const AdapterSetting current_settings, AdapterSetting changes,
                                            const uint64_t timestampMS) noexcept;
            void sendAdapterSettingsInitial(AdapterStatusListener & asl, const uint64_t timestampMS) noexcept;

            void sendDeviceUpdated(std::string cause, std::shared_ptr<DBTDevice> device, uint64_t timestamp, EIRDataType updateMask) noexcept;

        public:

            /**
             * Using the default adapter device
             * <p>
             * The default adapter is either the first POWERED adapter,
             * or none - in which case this instance !isValid()
             * </p>
             */
            DBTAdapter() noexcept;

            /**
             * Using the identified adapter with given mac address.
             *
             * @param[in] mac address
             */
            DBTAdapter(EUI48 &mac) noexcept;

            /**
             * Using the identified adapter with given dev_id,
             * or the default adapter device if dev_id < 0.
             * <p>
             * The default adapter is either the first POWERED adapter,
             * or none - in which case this instance !isValid().
             * </p>
             *
             * @param[in] dev_id an already identified HCI device id
             *            or use -1 to choose the default adapter.
             */
            DBTAdapter(const int dev_id) noexcept;

            DBTAdapter(const DBTAdapter&) = delete;
            void operator=(const DBTAdapter&) = delete;

            /**
             * Releases this instance.
             */
            ~DBTAdapter() noexcept;

            /**
             * Closes this instance, usually being called by destructor or when this adapter is being removed.
             * <p>
             * Renders this adapter's DBTAdapter#isValid() state to false.
             * </p>
             */
            void close() noexcept;

            std::string get_java_class() const noexcept override {
                return java_class();
            }
            static std::string java_class() noexcept {
                return std::string(JAVA_DBT_PACKAGE "DBTAdapter");
            }

            bool hasDevId() const noexcept { return 0 <= dev_id; }

            /**
             * Returns whether the adapter is valid, plugged in and powered.
             * @return true if DBTAdapter::isValid(), HCIHandler::isOpen() and AdapterSetting::POWERED state is set.
             * @see #isSuspended()
             * @see #isValid()
             */
            bool isPowered() const noexcept {
                return isValid() && hci.isOpen() && adapterInfo->isCurrentSettingBitSet(AdapterSetting::POWERED);
            }

            /**
             * Returns whether the adapter is suspended, i.e. valid and plugged in, but not powered.
             * @return true if DBTAdapter::isValid(), HCIHandler::isOpen() and AdapterSetting::POWERED state is not set.
             * @see #isPowered()
             * @see #isValid()
             */
            bool isSuspended() const noexcept {
                return isValid() && hci.isOpen() && !adapterInfo->isCurrentSettingBitSet(AdapterSetting::POWERED);
            }

            bool hasSecureConnections() const noexcept {
                return adapterInfo->isCurrentSettingBitSet(AdapterSetting::SECURE_CONN);
            }

            bool hasSecureSimplePairing() const noexcept {
                return adapterInfo->isCurrentSettingBitSet(AdapterSetting::SSP);
            }

            /**
             * Returns whether the adapter is valid, i.e. reference is valid, plugged in and generally operational,
             * but not necessarily DBTAdapter::isPowered() powered.
             * @return true if this adapter references are valid and hasn't been DBTAdapter::close() 'ed
             * @see #isPowered()
             * @see #isSuspended()
             */
            bool isValid() const noexcept {
                return DBTObject::isValid();
            }

            EUI48 const & getAddress() const noexcept { return adapterInfo->address; }
            std::string getAddressString() const noexcept { return adapterInfo->address.toString(); }

            /**
             * Returns the system name.
             */
            std::string getName() const noexcept { return adapterInfo->getName(); }

            /**
             * Returns the short system name.
             */
            std::string getShortName() const noexcept { return adapterInfo->getShortName(); }

            /**
             * Returns the local friendly name and short_name. Contains empty strings if not set.
             * <p>
             * The value is being updated via SET_LOCAL_NAME management event reply.
             * </p>
             */
            const NameAndShortName & getLocalName() const noexcept { return localName; }

            /**
             * Sets the local friendly name.
             * <p>
             * Returns the immediate SET_LOCAL_NAME reply if successful, otherwise nullptr.
             * The corresponding management event will be received separately.
             * </p>
             */
            std::shared_ptr<NameAndShortName> setLocalName(const std::string &name, const std::string &short_name) noexcept;

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
             */
            bool setPowered(bool value) noexcept;

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
             * Returns a reference to the used singleton DBTManager instance, used to create this adapter.
             */
            DBTManager& getManager() const noexcept { return mgmt; }

            /**
             * Returns a reference to the aggregated HCIHandler instance.
             */
            HCIHandler& getHCI() noexcept { return hci; }

            /**
             * Returns true, if the adapter's device is already whitelisted.
             */
            bool isDeviceWhitelisted(const EUI48 &address) noexcept;

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
            bool addDeviceToWhitelist(const EUI48 &address, const BDAddressType address_type,
                                      const HCIWhitelistConnectType ctype,
                                      const uint16_t conn_interval_min=12, const uint16_t conn_interval_max=12,
                                      const uint16_t conn_latency=0, const uint16_t supervision_timeout=getHCIConnSupervisorTimeout(0, 15));


            /** Remove the given device from the adapter's autoconnect whitelist. */
            bool removeDeviceFromWhitelist(const EUI48 &address, const BDAddressType address_type);

            // device discovery aka device scanning

            /**
             * Add the given listener to the list if not already present.
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
             */
            bool addStatusListener(std::shared_ptr<AdapterStatusListener> l);

            /**
             * Remove the given listener from the list.
             * <p>
             * Returns true if the given listener is an element of the list and has been removed,
             * otherwise false.
             * </p>
             */
            bool removeStatusListener(std::shared_ptr<AdapterStatusListener> l);

            /**
             * Remove the given listener from the list.
             * <p>
             * Returns true if the given listener is an element of the list and has been removed,
             * otherwise false.
             * </p>
             */
            bool removeStatusListener(const AdapterStatusListener * l);

            /**
             * Remove all status listener from the list.
             * <p>
             * Returns the number of removed event listener.
             * </p>
             */
            int removeAllStatusListener();

            /**
             * Starts a new discovery session.
             * <p>
             * Returns HCIStatusCode::SUCCESS if successful, otherwise the HCIStatusCode error state;
             * </p>
             * <p>
             * if {@code keepAlive} is {@code  true}, discovery state will be re-enabled
             * in case the underlying Bluetooth implementation (BlueZ, ..) disabled it.
             * Default is {@code true}.
             * </p>
             * <p>
             * Using startDiscovery(keepAlive=true) and stopDiscovery()
             * is the recommended workflow for a reliable discovery process.
             * </p>
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
             * <p>
             * Remaining default parameter values are chosen for using public address resolution
             * and usual discovery intervals etc.
             * </p>
             * <p>
             * This adapter's HCIHandler instance is used to initiate scanning,
             * see HCIHandler::le_set_scan_param() and HCIHandler::le_enable_scan().
             * </p>
             * <p>
             * Method will always clear previous discovered devices via removeDiscoveredDevices().
             * </p>
             * @param keepAlive
             * @param own_mac_type
             * @param le_scan_interval in units of 0.625ms, default value 24 for 15ms; Value range [4 .. 0x4000] for [2.5ms .. 10.24s]
             * @param le_scan_window in units of 0.625ms, default value 24 for 15ms; Value range [4 .. 0x4000] for [2.5ms .. 10.24s]. Shall be <= le_scan_interval
             * @return HCIStatusCode::SUCCESS if successful, otherwise the HCIStatusCode error state
             */
            HCIStatusCode startDiscovery(const bool keepAlive=true, const HCILEOwnAddressType own_mac_type=HCILEOwnAddressType::PUBLIC,
                                         const uint16_t le_scan_interval=24, const uint16_t le_scan_window=24);

            /**
             * Closes the discovery session.
             * <p>
             * This adapter's HCIHandler instance is used to stop scanning,
             * see HCIHandler::le_enable_scan().
             * </p>
             * @return HCIStatusCode::SUCCESS if successful, otherwise the HCIStatusCode error state
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
             * Returns the adapter's current native discovering ScanType. It can be modified through startDiscovery(..) and stopDiscovery().
             * @see startDiscovery()
             * @see stopDiscovery()
             */
            ScanType getCurrentNativeScanType() const noexcept{
                return hci.getCurrentScanType();
            }

            /**
             * Returns the meta discovering state. It can be modified through startDiscovery(..) and stopDiscovery().
             * @see startDiscovery()
             * @see stopDiscovery()
             */
            bool getDiscovering() const noexcept {
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
            std::vector<std::shared_ptr<DBTDevice>> getDiscoveredDevices() const noexcept;

            /** Discards all discovered devices. Returns number of removed discovered devices. */
            int removeDiscoveredDevices() noexcept;

            /** Returns shared DBTDevice if found, otherwise nullptr */
            std::shared_ptr<DBTDevice> findDiscoveredDevice (EUI48 const & mac, const BDAddressType macType) noexcept;

            std::string toString() const noexcept override { return toString(true); }
            std::string toString(bool includeDiscoveredDevices) const noexcept;

            /**
             * This is a debug facility only, to observe consistency
             * of the internally maintained lists of shared_ptr<DBTDevice>.
             */
            void printSharedPtrListOfDevices() noexcept;
    };

} // namespace direct_bt

#endif /* DBT_ADAPTER_HPP_ */
