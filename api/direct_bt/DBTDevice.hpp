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

#ifndef DBT_DEVICE_HPP_
#define DBT_DEVICE_HPP_

#include <cstring>
#include <string>
#include <memory>
#include <cstdint>
#include <vector>

#include <mutex>

#include "DBTTypes.hpp"

#include "HCIIoctl.hpp"
#include "HCIComm.hpp"

#include "SMPHandler.hpp"
#include "GATTHandler.hpp"

namespace direct_bt {

    // *************************************************
    // *************************************************
    // *************************************************

    class DBTAdapter; // forward

    class DBTDevice : public DBTObject
    {
        friend DBTAdapter; // managing us: ctor and update(..) during discovery
        friend GATTHandler; // may issue detailed disconnect(..)

        private:
            DBTAdapter & adapter;
            L2CAPComm l2cap_att;
            uint64_t ts_last_discovery;
            uint64_t ts_last_update;
            std::string name;
            int8_t rssi = 127; // The core spec defines 127 as the "not available" value
            int8_t tx_power = 127; // The core spec defines 127 as the "not available" value
            AppearanceCat appearance = AppearanceCat::UNKNOWN;
            jau::relaxed_atomic_uint16 hciConnHandle;
            jau::ordered_atomic<LEFeatures, std::memory_order_relaxed> le_features;
            std::shared_ptr<ManufactureSpecificData> advMSD = nullptr;
            std::vector<std::shared_ptr<uuid_t>> advServices;
#if SMP_SUPPORTED_BY_OS
            std::shared_ptr<SMPHandler> smpHandler = nullptr;
            std::recursive_mutex mtx_smpHandler;
#endif
            std::shared_ptr<GATTHandler> gattHandler = nullptr;
            std::recursive_mutex mtx_gattHandler;
            std::recursive_mutex mtx_connect;
            std::recursive_mutex mtx_data;
            std::atomic<bool> isConnected;
            std::atomic<bool> allowDisconnect; // allowDisconnect = isConnected || 'isConnectIssued'

            struct PairingData {
                jau::ordered_atomic<SMPPairingState, std::memory_order_relaxed> state;
                jau::ordered_atomic<PairingMode, std::memory_order_relaxed> mode;
                SMPAuthReqs     authReqs_init, authReqs_resp;
                SMPIOCapability ioCap_init,    ioCap_resp;
                SMPOOBDataFlag  oobFlag_init,  oobFlag_resp;
                uint8_t         maxEncsz_init, maxEncsz_resp;
            };
            PairingData pairing_data;
            std::mutex mtx_pairing;


            DBTDevice(DBTAdapter & adapter, EInfoReport const & r);

            /** Add advertised service (GAP discovery) */
            bool addAdvService(std::shared_ptr<uuid_t> const &uuid) noexcept;
            /** Add advertised service (GAP discovery) */
            bool addAdvServices(std::vector<std::shared_ptr<uuid_t>> const & services) noexcept;
            /**
             * Find advertised service (GAP discovery) index
             * @return index >= 0 if found, otherwise -1
             */
            int findAdvService(std::shared_ptr<uuid_t> const &uuid) const noexcept;

            EIRDataType update(EInfoReport const & data) noexcept;
            EIRDataType update(GattGenericAccessSvc const &data, const uint64_t timestamp) noexcept;

            void notifyDisconnected() noexcept;
            void notifyConnected(const uint16_t handle) noexcept;
            void notifyLEFeatures(const LEFeatures features) noexcept;

            /**
             * Returns a newly established GATT connection.
             * <p>
             * Will be performed after connectLE(..) via notifyConnected(), processNotifyConnectedOffThread().
             * </p>
             * <p>
             * The GATTHandler is managed by this device instance and closed via disconnectGATT().
             * </p>
             */
            bool connectGATT() noexcept;

            bool updatePairingState_locked(SMPPairingState state, PairingMode& current_mode) noexcept;

            /**
             * Forwarded from HCIHandler -> DBTAdapter -> this DBTDevice
             */
            void hciSMPMsgCallback(std::shared_ptr<DBTDevice> sthis, std::shared_ptr<const SMPPDUMsg> msg, const HCIACLData::l2cap_frame& source) noexcept;

            /**
             * Will be performed within disconnect() and notifyDisconnected().
             */
            void disconnectGATT(int caller) noexcept;

            /**
             * Returns a newly established SMP host connection.
             * <p>
             * Will be performed after connectLE(..) via notifyConnected(), processNotifyConnectedOffThread().
             * </p>
             * <p>
             * The SMPHandler is managed by this device instance and closed via disconnectSMP().
             * </p>
             */
            bool connectSMP() noexcept;

            /**
             * Will be performed within disconnect() and notifyDisconnected().
             */
            void disconnectSMP(int caller) noexcept;

            void clearSMPStates() noexcept;

            /**
             * Will be performed after connectLE(..) via notifyConnected(),
             * issuing connectSMP() off thread.
             */
            void processNotifyConnected();

            /**
             * Will be performed after connectLE(..) via notifyConnected() or after pairing via hciSMPMsgCallback(..),
             * issuing connectGATT() off thread.
             */
            void processDeviceReady(std::shared_ptr<DBTDevice> sthis, const uint64_t timestamp);

        public:
            const uint64_t ts_creation;
            /** Device mac address */
            const EUI48 address;
            const BDAddressType addressType;
            const BLERandomAddressType leRandomAddressType;

            DBTDevice(const DBTDevice&) = delete;
            void operator=(const DBTDevice&) = delete;

            /**
             * Releases this instance after calling {@link #remove()}.
             */
            ~DBTDevice() noexcept;

            std::string get_java_class() const noexcept override {
                return java_class();
            }
            static std::string java_class() noexcept {
                return std::string(JAVA_DBT_PACKAGE "DBTDevice");
            }

            /** Returns the managing adapter */
            DBTAdapter & getAdapter() const { return adapter; }

            /** Returns the shared pointer of this instance managed by the adapter. */
            std::shared_ptr<DBTDevice> getSharedInstance() const noexcept;

            /**
             * Returns the timestamp in monotonic milliseconds when this device instance has been created,
             * either via its initial discovery or its initial direct connection.
             * @see BasicTypes::getCurrentMilliseconds()
             */
            uint64_t getCreationTimestamp() const noexcept { return ts_creation; }

            /**
             * Returns the timestamp in monotonic milliseconds when this device instance has
             * discovered or connected directly the last time.
             * @see BasicTypes::getCurrentMilliseconds()
             */
            uint64_t getLastDiscoveryTimestamp() const noexcept { return ts_last_discovery; }

            /**
             * Returns the timestamp in monotonic milliseconds when this device instance underlying data
             * has been updated the last time.
             * @see BasicTypes::getCurrentMilliseconds()
             */
            uint64_t getLastUpdateTimestamp() const noexcept { return ts_last_update; }

            /**
             * @see getLastUpdateTimestamp()
             */
            uint64_t getLastUpdateAge(const uint64_t ts_now) const noexcept { return ts_now - ts_last_update; }

            EUI48 const & getAddress() const noexcept { return address; }
            std::string getAddressString() const noexcept { return address.toString(); }
            constexpr BDAddressType getAddressType() const noexcept { return addressType; }
            constexpr bool isLEAddressType() const noexcept { return direct_bt::isLEAddressType(addressType); }
            constexpr bool isBREDRAddressType() const noexcept { return direct_bt::isBREDRAddressType(addressType); }

            /**
             * Returns the {@link BLERandomAddressType}.
             * <p>
             * If {@link #getAddressType()} is {@link BDAddressType::BDADDR_LE_RANDOM},
             * method shall return a valid value other than {@link BLERandomAddressType::UNDEFINED}.
             * </p>
             * <p>
             * If {@link #getAddressType()} is not {@link BDAddressType::BDADDR_LE_RANDOM},
             * method shall return {@link BLERandomAddressType::UNDEFINED}.
             * </p>
             * @since 2.0.0
             */
            constexpr BLERandomAddressType getBLERandomAddressType() const noexcept { return leRandomAddressType; }

            /** Return RSSI of device as recognized at discovery and connect. */
            int8_t getRSSI() const noexcept { return rssi; }

            /** Return Tx Power of device as recognized at discovery and connect. */
            int8_t getTxPower() const noexcept { return tx_power; }

            /** Return AppearanceCat of device as recognized at discovery, connect and GATT discovery. */
            AppearanceCat getAppearance() const noexcept { return appearance; }

            std::string const getName() const noexcept;

            /** Return shared ManufactureSpecificData as recognized at discovery, pre GATT discovery. */
            std::shared_ptr<ManufactureSpecificData> const getManufactureSpecificData() const noexcept;

            /**
             * Return a list of advertised services as recognized at discovery, pre GATT discovery.
             * <p>
             * To receive a complete list of GATT services including characteristics etc,
             * use {@link #getGATTServices()}.
             * </p>
             */
            std::vector<std::shared_ptr<uuid_t>> getAdvertisedServices() const noexcept;

            std::string toString() const noexcept override { return toString(false); }

            std::string toString(bool includeDiscoveredServices) const noexcept;

            /**
             * Retrieves the current connection info for this device and returns the ConnectionInfo reference if successful,
             * otherwise returns nullptr.
             * <p>
             * Before this method returns, the internal rssi and tx_power will be updated if any changed
             * and therefore all DBTAdapterStatusListener's deviceUpdated(..) method called for notification.
             * </p>
             */
            std::shared_ptr<ConnectionInfo> getConnectionInfo() noexcept;

            /**
             * Return true if the device has been successfully connected, otherwise false.
             */
            bool getConnected() noexcept { return isConnected.load(); }

            /**
             * Establish a HCI BDADDR_LE_PUBLIC or BDADDR_LE_RANDOM connection to this device.
             * <p>
             * BT Core Spec v5.2: Vol 4, Part E HCI: 7.8.12 LE Create Connection command
             * </p>
             * <p>
             * If this device's addressType is not BDADDR_LE_PUBLIC or BDADDR_LE_RANDOM,
             * HCIStatusCode::UNACCEPTABLE_CONNECTION_PARAM is being returned.
             * </p>
             * <p>
             * The actual new connection handle will be delivered asynchronous and
             * the connection event can be caught via AdapterStatusListener::deviceConnected(..),
             * or if failed via AdapterStatusListener::deviceDisconnected(..).
             * </p>
             * <p>
             * The device is tracked by the managing adapter.
             * </p>
             * <p>
             * Default parameter values are chosen for using public address resolution
             * and usual connection latency, interval etc.
             * </p>
             * <p>
             * Set window to the same value as the interval, enables continuous scanning.
             * </p>
             * <p>
             * The associated DBTAdapter's HCIHandler instance is used to connect,
             * see HCIHandler::le_create_conn().
             * </p>
             *
             * @param le_scan_interval in units of 0.625ms, default value 24 for 15ms; Value range [4 .. 0x4000] for [2.5ms .. 10.24s]
             * @param le_scan_window in units of 0.625ms, default value 24 for 15ms; Value range [4 .. 0x4000] for [2.5ms .. 10.24s]. Shall be <= le_scan_interval
             * @param conn_interval_min in units of 1.25ms, default value 12 for 15ms; Value range [6 .. 3200] for [7.5ms .. 4000ms]
             * @param conn_interval_max in units of 1.25ms, default value 12 for 15ms; Value range [6 .. 3200] for [7.5ms .. 4000ms]
             * @param conn_latency slave latency in units of connection events, default value 0; Value range [0 .. 0x01F3].
             * @param supervision_timeout in units of 10ms, default value >= 10 x conn_interval_max, we use HCIConstInt::LE_CONN_MIN_TIMEOUT_MS minimum; Value range [0xA-0x0C80] for [100ms - 32s].
             * @return HCIStatusCode::SUCCESS if the command has been accepted, otherwise HCIStatusCode may disclose reason for rejection.
             */
            HCIStatusCode connectLE(const uint16_t le_scan_interval=24, const uint16_t le_scan_window=24,
                                    const uint16_t conn_interval_min=12, const uint16_t conn_interval_max=12,
                                    const uint16_t conn_latency=0, const uint16_t supervision_timeout=getHCIConnSupervisorTimeout(0, 15));

            /**
             * Establish a HCI BDADDR_BREDR connection to this device.
             * <p>
             * BT Core Spec v5.2: Vol 4, Part E HCI: 7.1.5 Create Connection command
             * </p>
             * <p>
             * If this device's addressType is not BDADDR_BREDR,
             * HCIStatusCode::UNACCEPTABLE_CONNECTION_PARAM is being returned.
             * </p>
             * <p>
             * The actual new connection handle will be delivered asynchronous and
             * the connection event can be caught via AdapterStatusListener::deviceConnected(..),
             * or if failed via AdapterStatusListener::deviceDisconnected(..).
             * </p>
             * <p>
             * The device is tracked by the managing adapter.
             * </p>
             * <p>
             * The associated DBTAdapter's HCIHandler instance is used to connect,
             * see HCIHandler::create_conn().
             * </p>
             * @return HCIStatusCode::SUCCESS if the command has been accepted, otherwise HCIStatusCode may disclose reason for rejection.
             */
            HCIStatusCode connectBREDR(const uint16_t pkt_type=HCI_DM1 | HCI_DM3 | HCI_DM5 | HCI_DH1 | HCI_DH3 | HCI_DH5,
                                       const uint16_t clock_offset=0x0000, const uint8_t role_switch=0x01);

            /**
             * Establish a default HCI connection to this device, using certain default parameter.
             * <p>
             * BT Core Spec v5.2: Vol 4, Part E HCI: 7.8.12 LE Create Connection command <br>
             * BT Core Spec v5.2: Vol 4, Part E HCI: 7.1.5 Create Connection command
             * </p>
             * <p>
             * Depending on this device's addressType,
             * either a BREDR (BDADDR_BREDR) or LE (BDADDR_LE_PUBLIC, BDADDR_LE_RANDOM) connection is attempted.<br>
             * If unacceptable, HCIStatusCode::UNACCEPTABLE_CONNECTION_PARAM is being returned.
             * </p>
             * <p>
             * The actual new connection handle will be delivered asynchronous and
             * the connection event can be caught via AdapterStatusListener::deviceConnected(..),
             * or if failed via AdapterStatusListener::deviceDisconnected(..).
             * <p>
             * The device is tracked by the managing adapter.
             * </p>
             * <p>
             * See connectLE() and connectBREDR() for more details.
             * </p>
             * @return HCIStatusCode::SUCCESS if the command has been accepted, otherwise HCIStatusCode may disclose reason for rejection.
             */
            HCIStatusCode connectDefault();


            /** Return the HCI connection handle to the LE or BREDR peer, zero if not connected. */
            uint16_t getConnectionHandle() const noexcept { return hciConnHandle; }

            /**
             * Disconnect the LE or BREDR peer's GATT and HCI connection.
             * <p>
             * BT Core Spec v5.2: Vol 4, Part E HCI: 7.1.6 Disconnect command
             * </p>
             * <p>
             * The actual disconnect event will be delivered asynchronous and
             * the connection event can be caught via AdapterStatusListener::deviceDisconnected(..).
             * </p>
             * <p>
             * The device will be removed from the managing adapter's connected devices
             * when AdapterStatusListener::deviceDisconnected(..) has been received.
             * </p>
             * <p>
             * An open GATTHandler will also be closed.<br>
             * The connection to this device is removed, removing all connected profiles.
             * </p>
             * <p>
             * An application using one thread per device and rapid connect, should either use disconnect() or remove(),
             * but never issue remove() after disconnect(). Doing so would eventually delete the device being already
             * in use by another thread due to discovery post disconnect!
             * </p>
             * <p>
             * The associated DBTAdapter's HCIHandler instance is used to disconnect,
             * see HCIHandler::disconnect().
             * </p>
             * @return HCIStatusCode::SUCCESS if the command has been accepted, otherwise HCIStatusCode may disclose reason for rejection.
             */
            HCIStatusCode disconnect(const HCIStatusCode reason=HCIStatusCode::REMOTE_USER_TERMINATED_CONNECTION ) noexcept;

            /**
             * Method sets the given passkey entry, see PairingMode::PASSKEY_ENTRY.
             * <p>
             * Call this method if the device shall be securely paired with PairingMode::PASSKEY_ENTRY,
             * when notified via AdapterStatusListener::devicePairingState().
             * <p>
             * If returning HCIStatusCode::SUCCESS, caller shall continue listening to AdapterStatusListener::devicePairingState()
             * to wait for either SMPPairingState::PROCESS_COMPLETED or SMPPairingState::FAILED.
             * </p>
             * @param passkey used for PairingMode::PASSKEY_ENTRY method.
             *        Will be encrypted before sending to counter-party.
             *
             * @return HCIStatusCode::SUCCESS if the command has been accepted, otherwise HCIStatusCode may disclose reason for rejection.
             * @see PairingMode
             * @see SMPPairingState
             * @see AdapterStatusListener::devicePairingState()
             * @see setPairingPasskey()
             * @see setPairingNumericComparison()
             * @see getPairingMode()
             * @see getPairingState()
             */
            HCIStatusCode setPairingPasskey(const uint32_t passkey) noexcept;

            HCIStatusCode setPairingPasskeyNegative() noexcept;

            /**
             * Method sets the numeric comparison result, see PairingMode::NUMERIC_COMPARISON.
             * <p>
             * Call this method if the device shall be securely paired with PairingMode::NUMERIC_COMPARISON,
             * when notified via AdapterStatusListener::devicePairingState().
             * <p>
             * If returning HCIStatusCode::SUCCESS, caller shall continue listening to AdapterStatusListener::devicePairingState()
             * to wait for either SMPPairingState::PROCESS_COMPLETED or SMPPairingState::FAILED.
             * </p>
             * @param equal used for PairingMode::NUMERIC_COMPARISON method.
             *        Will be encrypted before sending to counter-party.
             *
             * @return HCIStatusCode::SUCCESS if the command has been accepted, otherwise HCIStatusCode may disclose reason for rejection.
             * @see PairingMode
             * @see SMPPairingState
             * @see AdapterStatusListener::devicePairingState()
             * @see setPairingPasskey()
             * @see setPairingNumericComparison()
             * @see getPairingMode()
             * @see getPairingState()
             */
            HCIStatusCode setPairingNumericComparison(const bool equal) noexcept;

            /**
             * Returns the current PairingMode used by the device.
             * <p>
             * If the device is not paired, the current mode is PairingMode::NONE.
             * </p>
             * <p>
             * If the Pairing Feature Exchange is completed, i.e. SMPPairingState::FEATURE_EXCHANGE_COMPLETED,
             * as notified by AdapterStatusListener::devicePairingState(),
             * the current mode reflects the currently used PairingMode.
             * </p>
             * <p>
             * In case the Pairing Feature Exchange is in progress, the current mode is PairingMode::NEGOTIATING.
             * </p>
             * @return current PairingMode.
             * @see PairingMode
             * @see SMPPairingState
             * @see AdapterStatusListener::devicePairingState()
             * @see setPairingPasskey()
             * @see setPairingNumericComparison()
             * @see getPairingMode()
             * @see getPairingState()
             */
            PairingMode getPairingMode() const noexcept { return pairing_data.mode; }

            /**
             * Returns the current SMPPairingState.
             * <p>
             * If the device is not paired, the current state is SMPPairingState::NONE.
             * </p>
             * @see PairingMode
             * @see SMPPairingState
             * @see AdapterStatusListener::devicePairingState()
             * @see setPairingPasskey()
             * @see setPairingNumericComparison()
             * @see getPairingMode()
             * @see getPairingState()
             */
            SMPPairingState getPairingState() const noexcept { return pairing_data.state; }

            /**
             * Disconnects this device via disconnect(..) if getConnected()==true
             * and explicitly removes its shared references from the Adapter:
             * connected-devices, discovered-devices and shared-devices.
             * <p>
             * This method shall be issued to ensure no device reference will
             * be leaked in a long lived adapter,
             * as only its reference within connected-devices and discovered-devices are removed at disconnect.
             * </p>
             * <p>
             * After calling this method, this instance is destroyed and shall not be used anymore!
             * </p>
             * <p>
             * This method is automatically called @ destructor.
             * </p>
             * <p>
             * This method is an atomic operation.
             * </p>
             * <p>
             * An application using one thread per device and rapid connect, should either use disconnect() or remove(),
             * but never issue remove() after disconnect() if the device is in use.
             * </p>
             */
            void remove() noexcept;

            /** Returns the connected GATTHandler or nullptr, see connectGATT(), getGATTServices() and disconnect(). */
            std::shared_ptr<GATTHandler> getGATTHandler() noexcept;

            /**
             * Returns a list of shared GATTService available on this device if successful,
             * otherwise returns an empty list if an error occurred.
             * <p>
             * The HCI connectLE(..) or connectBREDR(..) must be performed first, see {@link #connectDefault()}.
             * </p>
             * <p>
             * If this method has been called for the first time or no services has been detected yet,
             * a list of GATTService will be discovered.
             * <br>
             * In case no GATT connection has been established it will be created via connectGATT().
             * </p>
             */
            std::vector<std::shared_ptr<GATTService>> getGATTServices() noexcept;

            /**
             * Returns the matching GATTService for the given uuid.
             * <p>
             * Implementation calls getGATTServices().
             * </p>
             */
            std::shared_ptr<GATTService> findGATTService(std::shared_ptr<uuid_t> const &uuid);

            /** Returns the shared GenericAccess instance, retrieved by {@link #getGATTServices()} or nullptr if not available. */
            std::shared_ptr<GattGenericAccessSvc> getGATTGenericAccess();

            /**
             * Issues a GATT ping to the device, validating whether it is still reachable.
             * <p>
             * This method could be periodically utilized to shorten the underlying OS disconnect period
             * after turning the device off, which lies within 7-13s.
             * </p>
             * <p>
             * In case the device is no more reachable, the GATTHandler will initiate disconnect due to the occurring IO error.
             * A disconnect will finally being issued.
             * </p>
             * <p>
             * GATT services must have been initialized via {@link #getGATTServices()}, otherwise {@code false} is being returned.
             * </p>
             * @return {@code true} if successful, otherwise false in case no GATT services exists or is not connected .. etc.
             */
            bool pingGATT() noexcept;

            /**
             * Add the given GATTCharacteristicListener to the listener list if not already present.
             * <p>
             * Convenience delegation call to GATTHandler
             * </p>
             * <p>
             * To enable the actual BLE notification and/or indication, one needs to call
             * GATTCharacteristic::configNotificationIndication(bool, bool, bool[])
             * or GATTCharacteristic::enableNotificationOrIndication(bool enabledState[2]).
             * </p>
             * @param listener A GATTCharacteristicListener instance, listening to all BluetoothGattCharacteristic events of this device
             * @return true if the given listener is not element of the list and has been newly added, otherwise false.
             * @throws IllegalStateException if the GATTHandler is null, i.e. not connected
             */
            bool addCharacteristicListener(std::shared_ptr<GATTCharacteristicListener> l);

            /**
             * Remove the given {@link GATTCharacteristicListener} from the listener list.
             * <p>
             * If the GATTHandler is null, i.e. not connected, {@code false} is being returned.
             * </p>
             * @param listener A {@link GATTCharacteristicListener} instance
             * @return true if the given listener is an element of the list and has been removed, otherwise false.
             */
            bool removeCharacteristicListener(std::shared_ptr<GATTCharacteristicListener> l) noexcept;

            /**
             * Remove all {@link GATTCharacteristicListener} from the list, which are associated to the given {@link GATTCharacteristic}.
             * <p>
             * Implementation tests all listener's GATTCharacteristicListener::match(const GATTCharacteristic & characteristic)
             * to match with the given associated characteristic.
             * </p>
             * @param associatedCharacteristic the match criteria to remove any GATTCharacteristicListener from the list
             * @return number of removed listener.
             */
            int removeAllAssociatedCharacteristicListener(std::shared_ptr<GATTCharacteristic> associatedCharacteristic) noexcept;

            /**
             * Remove all {@link GATTCharacteristicListener} from the list.
             * @return number of removed listener.
             */
            int removeAllCharacteristicListener() noexcept;
    };

    inline bool operator<(const DBTDevice& lhs, const DBTDevice& rhs) noexcept
    { return lhs.address < rhs.address; }

    inline bool operator==(const DBTDevice& lhs, const DBTDevice& rhs) noexcept
    { return lhs.address == rhs.address && lhs.addressType == rhs.addressType; }

    inline bool operator!=(const DBTDevice& lhs, const DBTDevice& rhs) noexcept
    { return !(lhs == rhs); }

} // namespace direct_bt

#endif /* DBT_DEVICE_HPP_ */
