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

#ifndef BT_DEVICE_HPP_
#define BT_DEVICE_HPP_

#include <cstring>
#include <string>
#include <memory>
#include <cstdint>

#include <mutex>

#include <jau/darray.hpp>

#include "BTTypes1.hpp"

#include "HCIIoctl.hpp"
#include "HCIComm.hpp"

#include "MgmtTypes.hpp"
#include "SMPHandler.hpp"
#include "BTGattHandler.hpp"
#include "SMPKeyBin.hpp"

namespace direct_bt {

    // *************************************************
    // *************************************************
    // *************************************************

    class BTAdapter; // forward
    class AdapterStatusListener; // forward
    typedef std::shared_ptr<AdapterStatusListener> AdapterStatusListenerRef; // forward

    /**
     * BTDevice represents one remote Bluetooth device.
     *
     * @anchor BTDeviceRoles
     * Invariable remote BTDevice roles (see getRole()):
     *
     * - {@link BTRole::Master}: The remote device has discovered us and maybe is connected to us. The remote device acts as a ::GATTRole::Client.
     * - {@link BTRole::Slave}: The remote device has advertised and maybe we are connected to it. The remote device acts as a ::GATTRole::Server.
     *
     * Note the local {@link BTAdapter}'s [opposite role](@ref BTAdapterRoles).
     *
     * @see BTAdapter
     * @see [BTAdapter roles](@ref BTAdapterRoles).
     * @see [BTGattHandler roles](@ref BTGattHandlerRoles).
     * @see [Bluetooth Specification](https://www.bluetooth.com/specifications/bluetooth-core-specification/)
     */
    class BTDevice : public BTObject
    {
        friend BTAdapter; // managing us: ctor and update(..) during discovery
        friend BTGattHandler; // may issue detailed disconnect(..)

        private:
            BTAdapter & adapter;
            BTRole btRole;
            std::unique_ptr<L2CAPClient> l2cap_att;
            uint64_t ts_last_discovery;
            uint64_t ts_last_update;
            std::string name;
            int8_t rssi = 127; // The core spec defines 127 as the "not available" value
            int8_t tx_power = 127; // The core spec defines 127 as the "not available" value
            std::shared_ptr<EInfoReport> eir; // Merged EIR (using shared_ptr to allow CoW style update)
            std::shared_ptr<EInfoReport> eir_ind; // AD_IND EIR
            std::shared_ptr<EInfoReport> eir_scan_rsp; // AD_SCAN_RSP EIR
            jau::relaxed_atomic_uint16 hciConnHandle;
            jau::ordered_atomic<LE_Features, std::memory_order_relaxed> le_features;
            jau::ordered_atomic<LE_PHYs, std::memory_order_relaxed> le_phy_tx;
            jau::ordered_atomic<LE_PHYs, std::memory_order_relaxed> le_phy_rx;
            std::shared_ptr<SMPHandler> smpHandler = nullptr;
            std::recursive_mutex mtx_smpHandler;
            std::shared_ptr<BTGattHandler> gattHandler = nullptr;
            mutable std::recursive_mutex mtx_gattHandler;
            mutable std::recursive_mutex mtx_connect;
            mutable std::mutex mtx_eir;
            jau::sc_atomic_bool isConnected;
            jau::sc_atomic_bool allowDisconnect; // allowDisconnect = isConnected || 'isConnectIssued'
            jau::relaxed_atomic_int32 supervision_timeout; // [ms]
            jau::relaxed_atomic_uint32 smp_events; // registering smp events until next BTAdapter::smp_watchdog periodic timeout check

            struct PairingData {
                SMPIOCapability ioCap_conn     = SMPIOCapability::UNSET;
                SMPIOCapability ioCap_user     = SMPIOCapability::UNSET;
                BTSecurityLevel sec_level_conn = BTSecurityLevel::UNSET;
                BTSecurityLevel sec_level_user = BTSecurityLevel::UNSET;
                SMPIOCapability ioCap_auto     = SMPIOCapability::UNSET; // not cleared by clearSMPStates()

                SMPPairingState state;
                PairingMode mode;
                bool res_requested_sec;
                bool use_sc;
                bool encryption_enabled;

                SMPAuthReqs     authReqs_init, authReqs_resp;
                SMPIOCapability ioCap_init,    ioCap_resp;
                SMPOOBDataFlag  oobFlag_init,  oobFlag_resp;
                uint8_t         maxEncsz_init, maxEncsz_resp;
                SMPKeyType      keys_init_exp, keys_resp_exp;
                SMPKeyType      keys_init_has, keys_resp_has;

                // LTK: Set of Long Term Key data: ltk, ediv + rand
                SMPLongTermKey  ltk_init, ltk_resp;

                // IRK
                SMPIdentityResolvingKey  irk_init,  irk_resp;

                // Identity Address Information
                BDAddressAndType id_address_init, id_address_resp;

                // CSRK
                SMPSignatureResolvingKey csrk_init, csrk_resp;

                // Link Key
                SMPLinkKey lk_init, lk_resp;

                /**
                 * Return verbose string representation of PairingData
                 * @param addressAndType remote address of the BTDevice
                 * @param role remote role of the BTDevice
                 */
                std::string toString(const uint16_t dev_id, const BDAddressAndType& addressAndType, const BTRole& role) const;
            };
            PairingData pairing_data;
            mutable std::recursive_mutex mtx_pairing;
            std::condition_variable_any cv_pairing_state_changed;
            mutable jau::sc_atomic_bool sync_data;

            /** Private class only for private make_shared(). */
            class ctor_cookie { friend BTDevice; ctor_cookie(const uint16_t secret) { (void)secret; } };

            /** Private std::make_shared<BTDevice>(..) vehicle for friends. */
            static std::shared_ptr<BTDevice> make_shared(BTAdapter & adapter, EInfoReport const & r) {
                return std::make_shared<BTDevice>(BTDevice::ctor_cookie(0), adapter, r);
            }

            void clearData() noexcept;

            EIRDataType update(EInfoReport const & data) noexcept;
            EIRDataType update(GattGenericAccessSvc const &data, const uint64_t timestamp) noexcept;

            void notifyDisconnected() noexcept;
            void notifyConnected(std::shared_ptr<BTDevice> sthis, const uint16_t handle, const SMPIOCapability io_cap) noexcept;
            void notifyLEFeatures(std::shared_ptr<BTDevice> sthis, const LE_Features features) noexcept;
            void notifyLEPhyUpdateComplete(const HCIStatusCode status, const LE_PHYs Tx, const LE_PHYs Rx) noexcept;

            /**
             * Setup L2CAP channel connection to device incl. optional security encryption level off-thread.
             * <p>
             * Will be performed after connectLE(..), i.e. notifyConnected() and notifyLEFeatures(),
             * initiated by the latter.
             * </p>
             */
            void processL2CAPSetup(std::shared_ptr<BTDevice> sthis);

            /**
             * Established SMP host connection and security for L2CAP connection if sec_level > BTSecurityLevel::NONE.
             * <p>
             * Will be performed after connectLE(..), i.e. notifyConnected() and notifyLEFeatures().<br>
             * Called from processL2CAPSetup, if supported.
             * </p>
             * <p>
             * If sec_level > BTSecurityLevel::NONE, sets the BlueZ's L2CAP socket BT_SECURITY sec_level, determining the SMP security mode per connection.
             * </p>
             * <p>
             * The SMPHandler is managed by this device instance and closed via disconnectSMP().
             * </p>
             *
             * @param sec_level sec_level <= BTSecurityLevel::NONE will not set security level and returns false.
             * @return true if a security level > BTSecurityLevel::NONE has been set successfully, false if no security level has been set or if it failed.
             */
            bool connectSMP(std::shared_ptr<BTDevice> sthis, const BTSecurityLevel sec_level) noexcept;

            bool checkPairingKeyDistributionComplete() const noexcept;

            bool updatePairingState(std::shared_ptr<BTDevice> sthis, const MgmtEvent& evt, const HCIStatusCode evtStatus, SMPPairingState claimed_state) noexcept;

            /**
             * Forwarded from HCIHandler -> BTAdapter -> this BTDevice
             * <p>
             * Will be initiated by processL2CAPSetup()'s security_level setup after connectLE(..), i.e. notifyConnected() and notifyLEFeatures().
             * </p>
             */
            void hciSMPMsgCallback(std::shared_ptr<BTDevice> sthis, const SMPPDUMsg& msg, const HCIACLData::l2cap_frame& source) noexcept;

            /**
             * Setup GATT via connectGATT() off-thread.
             * <p>
             * <p>
             * Will be performed after connectLE(..), i.e. notifyConnected() and notifyLEFeatures().<br>
             * Called from either processL2CAPSetup() w/o security or with SMP security readiness from hciSMPMsgCallback().
             * </p>
             */
            void processDeviceReady(std::shared_ptr<BTDevice> sthis, const uint64_t timestamp);

            /**
             * Returns a newly established GATT connection.
             * <p>
             * Will be performed after connectLE(..) via notifyConnected(), processNotifyConnectedOffThread().
             * </p>
             * <p>
             * The GATTHandler is managed by this device instance and closed via disconnectGATT().
             * </p>
             */
            bool connectGATT(std::shared_ptr<BTDevice> sthis) noexcept;

            /**
             * Will be performed within disconnect() and notifyDisconnected().
             */
            void disconnectGATT(const int caller) noexcept;

            /**
             * Will be performed within disconnect() and notifyDisconnected().
             */
            void disconnectSMP(const int caller) noexcept;

            void clearSMPStates(const bool connected) noexcept;

            void sendMgmtEvDeviceDisconnected(std::unique_ptr<MgmtEvent> evt) noexcept;

        public:
            const uint64_t ts_creation;
            /** Device's unique mac address and type tuple. */
            const BDAddressAndType addressAndType; // FIXME: Mutable for resolvable -> identity during pairing?

            /** Private ctor for private BTDevice::make_shared() intended for friends. */
            BTDevice(const BTDevice::ctor_cookie& cc, BTAdapter & adapter, EInfoReport const & r);

            BTDevice(const BTDevice&) = delete;
            void operator=(const BTDevice&) = delete;

            /**
             * Releases this instance after calling {@link #remove()}.
             */
            ~BTDevice() noexcept override;

            std::string get_java_class() const noexcept override {
                return java_class();
            }
            static std::string java_class() noexcept {
                return std::string(JAVA_DBT_PACKAGE "DBTDevice");
            }

            /** Returns the managing adapter */
            BTAdapter & getAdapter() const { return adapter; }

            /** Returns the shared pointer of this instance managed by the adapter. */
            std::shared_ptr<BTDevice> getSharedInstance() const noexcept;

            /**
             * Return the fixed BTRole of this remote BTDevice.
             * @see BTRole
             * @see @ref BTDeviceRoles
             * @since 2.4.0
             */
            BTRole getRole() const noexcept { return btRole; }

            /**
             * Return the local GATTRole operating for the remote BTDevice.
             * @see @ref BTGattHandlerRoles
             * @see @ref BTDeviceRoles
             * @since 2.4.0
             */
            GATTRole getLocalGATTRole() const noexcept;

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

            /**
             * Returns the unique device EUI48 address and BDAddressType type.
             * @since 2.2.0
             */
            constexpr BDAddressAndType const & getAddressAndType() const noexcept { return addressAndType; }

            /** Return RSSI of device as recognized at discovery and connect. */
            int8_t getRSSI() const noexcept { return rssi; }

            /** Return Tx Power of device as recognized at discovery and connect. */
            int8_t getTxPower() const noexcept { return tx_power; }

            /**
             * Returns the remote device name.
             *
             * The name has been set by the advertised EInfoReport if available,
             * otherwise by the GATT GenericAccess data post connection.
             */
            std::string const getName() const noexcept;

            /**
             * Return the merged advertised EInfoReport for this remote device.
             *
             * The EInfoReport is updated by new scan-reports (update) and when disconnected (empty).
             * @since 2.5.3
             */
            EInfoReportRef getEIR() noexcept;

            /**
             * Return the latest advertised EInfoReport AD_IND variant for this remote device.
             *
             * The EInfoReport is replaced by new scan-reports only.
             * @since 2.6.6
             */
            EInfoReportRef getEIRInd() noexcept;

            /**
             * Return the latest advertised EInfoReport AD_SCAN_RSP for this remote device.
             *
             * The EInfoReport is replaced by new scan-reports only.
             * @since 2.6.6
             */
            EInfoReportRef getEIRScanRsp() noexcept;

            std::string toString() const noexcept override { return toString(false); }

            std::string toString(bool includeDiscoveredServices) const noexcept;

            /**
             * Add the given AdapterStatusListener to the list if not already present,
             * intended to listen only for events matching this device.
             *
             * User needs to implement AdapterStatusListener::matchDevice() for the latter.
             *
             * The AdapterStatusListener is released at remove() or this device's destruction.
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
             * @since 2.3.0
             * @see BTAdapter::addStatusListener()
             * @see BTAdapter::removeStatusListener()
             * @see BTAdapter::removeAllStatusListener()
             * @see removeStatusListener()
             */
            bool addStatusListener(const AdapterStatusListenerRef& l) noexcept;

            /**
             * Remove the given listener from the list.
             * <p>
             * Returns true if the given listener is an element of the list and has been removed,
             * otherwise false.
             * </p>
             * @since 2.3.0
             * @see BTAdapter::removeStatusListener()
             * @see BTAdapter::removeStatusListener()
             * @see BTAdapter::removeAllStatusListener()
             * @see addStatusListener()
             */
            bool removeStatusListener(const AdapterStatusListenerRef& l) noexcept;

            /**
             * Retrieves the current connection info for this device and returns the ConnectionInfo reference if successful,
             * otherwise returns nullptr.
             * <p>
             * Before this method returns, the internal rssi and tx_power will be updated if any changed
             * and therefore all BTAdapterStatusListener's deviceUpdated(..) method called for notification.
             * </p>
             */
            std::shared_ptr<ConnectionInfo> getConnectionInfo() noexcept;

            /**
             * Return true if the device has been successfully connected, otherwise false.
             */
            bool getConnected() noexcept { return isConnected; }

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
             * The associated BTAdapter's HCIHandler instance is used to connect,
             * see HCIHandler::le_create_conn().
             * </p>
             *
             * @param le_scan_interval in units of 0.625ms, default value 24 for 15ms; Value range [4 .. 0x4000] for [2.5ms .. 10.24s]
             * @param le_scan_window in units of 0.625ms, default value 24 for 15ms; Value range [4 .. 0x4000] for [2.5ms .. 10.24s]. Shall be <= le_scan_interval
             * @param conn_interval_min in units of 1.25ms, default value 8 for 10ms; Value range [6 .. 3200] for [7.5ms .. 4000ms]
             * @param conn_interval_max in units of 1.25ms, default value 12 for 15ms; Value range [6 .. 3200] for [7.5ms .. 4000ms]
             * @param conn_latency slave latency in units of connection events, default value 0; Value range [0 .. 0x01F3]. See Range of [0 - getHCIMaxConnLatency()].
             * @param conn_supervision_timeout in units of 10ms, default value >= 10 x conn_interval_max; Value range [0xA-0x0C80] for [100ms - 32s]. We use 500ms minimum, i.e. getHCIConnSupervisorTimeout(0, 15, ::HCIConstInt::LE_CONN_MIN_TIMEOUT_MS).
             * @return HCIStatusCode::SUCCESS if the command has been accepted, otherwise HCIStatusCode may disclose reason for rejection.
             */
            HCIStatusCode connectLE(const uint16_t le_scan_interval=24, const uint16_t le_scan_window=24,
                                    const uint16_t conn_interval_min=8, const uint16_t conn_interval_max=12,
                                    const uint16_t conn_latency=0, const uint16_t conn_supervision_timeout=getHCIConnSupervisorTimeout(0, 15)) noexcept;

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
             * The associated BTAdapter's HCIHandler instance is used to connect,
             * see HCIHandler::create_conn().
             * </p>
             * @return HCIStatusCode::SUCCESS if the command has been accepted, otherwise HCIStatusCode may disclose reason for rejection.
             */
            HCIStatusCode connectBREDR(const uint16_t pkt_type=HCI_DM1 | HCI_DM3 | HCI_DM5 | HCI_DH1 | HCI_DH3 | HCI_DH5,
                                       const uint16_t clock_offset=0x0000, const uint8_t role_switch=0x01) noexcept;

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
            HCIStatusCode connectDefault() noexcept;


            /** Return the HCI connection handle to the LE or BREDR peer, zero if not connected. */
            uint16_t getConnectionHandle() const noexcept { return hciConnHandle; }

            /**
             * Request and return LE_PHYs bit for the given connection.
             * <pre>
             * BT Core Spec v5.2: Vol 4, Part E, 7.8.47 LE Read PHY command
             * </pre>
             * @param resTx reference for the resulting transmitter LE_PHYs bit
             * @param resRx reference for the resulting receiver LE_PHYs bit
             * @return HCIStatusCode
             * @see getTxPhys()
             * @see getRxPhys()
             * @see getConnectedLE_PHY()
             * @see setConnectedLE_PHY()
             * @see BTAdapter::setDefaultLE_PHY()
             * @since 2.4.0
             */
            HCIStatusCode getConnectedLE_PHY(LE_PHYs& resTx, LE_PHYs& resRx) noexcept;

            /**
             * Return the Tx LE_PHYs as notified via HCIMetaEventType::LE_PHY_UPDATE_COMPLETE
             * or retrieved via getConnectedLE_PHY()
             * @see getTxPhys()
             * @see getRxPhys()
             * @see getConnectedLE_PHY()
             * @see setConnectedLE_PHY()
             * @see BTAdapter::setDefaultLE_PHY()
             * @since 2.4.0
             */
            LE_PHYs getTxPhys() const noexcept { return le_phy_tx; }

            /**
             * Return the Rx LE_PHYs as notified via HCIMetaEventType::LE_PHY_UPDATE_COMPLETE
             * or retrieved via getConnectedLE_PHY()
             * @see getTxPhys()
             * @see getRxPhys()
             * @see getConnectedLE_PHY()
             * @see setConnectedLE_PHY()
             * @see BTAdapter::setDefaultLE_PHY()
             * @since 2.4.0
             */
            LE_PHYs getRxPhys() const noexcept { return le_phy_rx; }

            /**
             * Sets preference of used LE_PHYs for the given connection.
             *
             * - BT Core Spec v5.2: Vol 4, Part E, 7.8.49 LE Set PHY command
             * - BT Core Spec v5.2: Vol 4, Part E, 7.7.65.12 LE PHY Update Complete event
             *
             * @param Tx transmitter LE_PHYs bit mask of preference if not set to LE_PHYs::NONE (ignored).
             * @param Rx receiver LE_PHYs bit mask of preference if not set to LE_PHYs::NONE (ignored).
             * @return
             * @see getTxPhys()
             * @see getRxPhys()
             * @see getConnectedLE_PHY()
             * @see setConnectedLE_PHY()
             * @see BTAdapter::setDefaultLE_PHY()
             * @since 2.4.0
             */
            HCIStatusCode setConnectedLE_PHY(const LE_PHYs Tx, const LE_PHYs Rx) noexcept;

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
             * The associated BTAdapter's HCIHandler instance is used to disconnect,
             * see HCIHandler::disconnect().
             * </p>
             * @return HCIStatusCode::SUCCESS if the command has been accepted, otherwise HCIStatusCode may disclose reason for rejection.
             */
            HCIStatusCode disconnect(const HCIStatusCode reason=HCIStatusCode::REMOTE_USER_TERMINATED_CONNECTION ) noexcept;

            /**
             * Returns the available ::SMPKeyType mask for the responder (LL slave) or initiator (LL master).
             * @param responder if true, queries the responder (LL slave) key, otherwise the initiator (LL master) key.
             * @return ::SMPKeyType mask result
             * @see ::SMPPairingState::COMPLETED
             * @see AdapterStatusListener::deviceReady()
             */
            SMPKeyType getAvailableSMPKeys(const bool responder) const noexcept;

            /**
             * Copy all keys from the given SMPKeyBin into this BTDevice.
             *
             * Issue uploadKeys() to upload all SMP keys to the adapter
             * before connecting to enable pre-pairing.
             *
             * If SMPKeyBin::isValid() and initiator or responder LTK available,
             * the following procedure will be applied to this BTDevice:
             *
             * - If BTSecurityLevel _is_ BTSecurityLevel::NONE
             *   + Setting security to ::BTSecurityLevel::NONE and ::SMPIOCapability::NO_INPUT_NO_OUTPUT via BTDevice::setConnSecurity()
             * - else if BTSecurityLevel > BTSecurityLevel::NONE
             *   + Setting security to ::BTSecurityLevel::ENC_ONLY and ::SMPIOCapability::NO_INPUT_NO_OUTPUT via BTDevice::setConnSecurity()
             * - Copying all keys from SMPKeyBin to this device, without uploading to the adapter
             *
             * ::BTSecurityLevel::ENC_ONLY is set to avoid a new SMP ::PairingMode negotiation,
             * which is undesired as this instances' stored LTK shall be used for ::PairingMode::PRE_PAIRED.
             *
             * @param bin
             * @return true if successful, false if pairing is currently in progress
             * @see setLongTermKey()
             * @see setIdentityResolvingKey()
             * @see setSignatureResolvingKey()
             * @see setLinkKey()
             * @see uploadKeys()
             * @since 2.4.0
             */
            bool setSMPKeyBin(const SMPKeyBin& bin) noexcept;

            /**
             * Upload all set keys to the adapter for pre-pairing.
             *
             * Must be called before connecting to this device, otherwise HCIStatusCode::CONNECTION_ALREADY_EXISTS will be returned.
             *
             * @return ::HCIStatusCode::SUCCESS if successful, otherwise the appropriate error code.
             * @see setLongTermKey()
             * @see setIdentityResolvingKey()
             * @see setSignatureResolvingKey()
             * @see setLinkKey()
             * @see setSMPKeyBin()
             * @since 2.4.0
             */
            HCIStatusCode uploadKeys() noexcept;

            /**
             * Convenient combination of setSMPKeyBin() and uploadKeys()
             * after validating given SMPKeyBin file and SMPKeyBin::getSecLevel() > req_min_level.
             * @param bin the SMPKeyBin file
             * @param req_min_level SMPKeyBin::getSecLevel() shall be greater or equal to this required minimum
             * @return ::HCIStatusCode::SUCCESS if successful, otherwise the appropriate error code.
             * @see setSMPKeyBin()
             * @see uploadKeys()
             * @since 2.4.0
             */
            HCIStatusCode uploadKeys(const SMPKeyBin& bin, const BTSecurityLevel req_min_level) noexcept {
                if( bin.isValid() && bin.getSecLevel() >= req_min_level && setSMPKeyBin(bin) ) {
                    return uploadKeys();
                } else {
                    return HCIStatusCode::INVALID_PARAMS;
                }
            }

            /**
             * Convenient combination of SMPKeyBin::read(), setSMPKeyBin() and uploadKeys()
             * after validating given SMPKeyBin file and SMPKeyBin::getSecLevel() > req_min_level.
             * @param smp_key_bin_path director for the SMPKeyBin file, derived by this BTDevice
             * @param req_min_level SMPKeyBin::getSecLevel() shall be greater or equal to this required minimum
             * @return ::HCIStatusCode::SUCCESS if successful, otherwise the appropriate error code.
             * @see SMPKeyBin::read()
             * @see setSMPKeyBin()
             * @see uploadKeys()
             * @since 2.4.0
             */
            HCIStatusCode uploadKeys(const std::string& smp_key_bin_path, const BTSecurityLevel req_min_level, const bool verbose_) noexcept {
                return uploadKeys(SMPKeyBin::read(smp_key_bin_path, *this, verbose_), req_min_level);
            }

            /**
             * Returns a copy of the Long Term Key (LTK), valid after connection and SMP pairing has been completed.
             * @param responder true will return the responder's LTK info (remote device, LL slave), otherwise the initiator's (the LL master).
             * @return the resulting key. SMPLongTermKeyInfo::enc_size will be zero if invalid.
             * @see ::SMPPairingState::COMPLETED
             * @see AdapterStatusListener::deviceReady()
             */
            SMPLongTermKey getLongTermKey(const bool responder) const noexcept;

            /**
             * Sets the Long Term Key (LTK) of this device for pre-paired encryption.
             *
             * Issue uploadKeys() to upload all SMP keys to the adapter
             * before connecting to enable pre-pairing.
             *
             * @param ltk the pre-paired encryption LTK
             * @see setSMPKeyBin()
             * @see uploadKeys()
             * @since 2.4.0
             */
            void setLongTermKey(const SMPLongTermKey& ltk) noexcept;

            /**
             * Returns a copy of the Identity Resolving Key (IRK), valid after connection and SMP pairing has been completed.
             * @param responder true will return the responder's IRK info (remote device, LL slave), otherwise the initiator's (the LL master).
             * @return the resulting key
             * @see ::SMPPairingState::COMPLETED
             * @see AdapterStatusListener::deviceReady()
             */
            SMPIdentityResolvingKey getIdentityResolvingKey(const bool responder) const noexcept;

            /**
             * Sets the Identity Resolving Key (IRK) of this device for pre-paired encryption.
             *
             * Issue uploadKeys() to upload all SMP keys to the adapter
             * before connecting to enable pre-pairing.
             *
             * @param irk the Identity Resolving Key (IRK)
             * @see setSMPKeyBin()
             * @see uploadKeys()
             * @since 2.4.0
             */
            void setIdentityResolvingKey(const SMPIdentityResolvingKey& irk) noexcept;

            /**
             * Returns a copy of the Signature Resolving Key (CSRK), valid after connection and SMP pairing has been completed.
             * @param responder true will return the responder's CSRK info (remote device, LL slave), otherwise the initiator's (the LL master).
             * @return the resulting key
             * @see ::SMPPairingState::COMPLETED
             * @see AdapterStatusListener::deviceReady()
             */
            SMPSignatureResolvingKey getSignatureResolvingKey(const bool responder) const noexcept;

            /**
             * Sets the Signature Resolving Key (CSRK) of this device for pre-paired encryption.
             *
             * Issue uploadKeys() to upload all SMP keys to the adapter
             * before connecting to enable pre-pairing.
             *
             * @param csrk the Signature Resolving Key (CSRK)
             * @see setSMPKeyBin()
             * @see uploadKeys()
             * @since 2.4.0
             */
            void setSignatureResolvingKey(const SMPSignatureResolvingKey& csrk) noexcept;

            /**
             * Returns a copy of the Link Key (LK), valid after connection and SMP pairing has been completed.
             * @param responder true will return the responder's LTK info (remote device, LL slave), otherwise the initiator's (the LL master).
             * @return the resulting key
             * @see ::SMPPairingState::COMPLETED
             * @see AdapterStatusListener::deviceReady()
             * @since 2.4.0
             */
            SMPLinkKey getLinkKey(const bool responder) const noexcept;

            /**
             * Sets the Link Key (LK) of this device for pre-paired encryption.
             *
             * Issue uploadKeys() to upload all SMP keys to the adapter
             * before connecting to enable pre-pairing.
             *
             * @param lk the pre-paired encryption LK
             * @see setSMPKeyBin()
             * @see uploadKeys()
             * @since 2.4.0
             */
            void setLinkKey(const SMPLinkKey& lk) noexcept;

            /**
             * Unpair this device from the adapter while staying connected.
             * <p>
             * All keys will be cleared within the adapter and host implementation.<br>
             * Should rarely being used by user.<br>
             * Internally being used to re-start pairing if GATT connection fails
             * in PairingMode::PRE_PAIRED mode.
             * </p>
             *
             * Unpair is performed by directly for a consistent and stable security workflow:
             * - when a BTRole::Slave BTDevice is discovered, see AdapterStatusListener::deviceFound().
             * - when a BTRole::Slave BTDevice is disconnected, see AdapterStatusListener::deviceDisconnected().
             * - when a BTRole::Master BTDevice gets connected, see AdapterStatusListener::deviceConnected().
             *
             * @return HCIStatusCode::SUCCESS or an appropriate error status.
             * @see AdapterStatusListener::deviceFound()
             * @see AdapterStatusListener::deviceDisconnected()
             * @see AdapterStatusListener::deviceConnected()
             */
            HCIStatusCode unpair() noexcept;

            /**
             * Return the ::BTSecurityLevel, determined when the connection is established.
             * @see ::BTSecurityLevel
             * @see ::SMPIOCapability
             * @see getConnIOCapability()
             * @see setConnSecurity()
             * @see setConnSecurityAuto()
             */
            BTSecurityLevel getConnSecurityLevel() const noexcept;

            /**
             * Return the set ::SMPIOCapability value, determined when the connection is established.
             * @see ::BTSecurityLevel
             * @see ::SMPIOCapability
             * @see getConnSecurityLevel()
             * @see setConnSecurity()
             * @see setConnSecurityAuto()
             */
            SMPIOCapability getConnIOCapability() const noexcept;

            /**
             * Sets the given ::BTSecurityLevel and ::SMPIOCapability used to connect to this device on the upcoming connection.
             *
             * Implementation using following pseudo-code, validating the user settings:
             * <pre>
             *   if( BTSecurityLevel::UNSET < sec_level && SMPIOCapability::UNSET != io_cap ) {
             *      USING: sec_level, io_cap
             *   } else if( BTSecurityLevel::UNSET < sec_level ) {
             *       if( BTSecurityLevel::ENC_ONLY >= sec_level ) {
             *           USING: sec_level, SMPIOCapability::NO_INPUT_NO_OUTPUT
             *       } else {
             *           USING: sec_level, SMPIOCapability::UNSET
             *       }
             *   } else if( SMPIOCapability::UNSET != io_cap ) {
             *       USING BTSecurityLevel::UNSET, io_cap
             *   } else {
             *       USING BTSecurityLevel::UNSET, SMPIOCapability::UNSET
             *   }
             * </pre>
             * <p>
             * Method returns false if this device has already being connected,
             * or BTDevice::connectLE() or BTDevice::connectBREDR() has been issued already.
             * </p>
             * <p>
             * Method either changes both parameter for the upcoming connection or none at all.
             * </p>
             * @param[in] sec_level ::BTSecurityLevel to be applied.
             * @param[in] io_cap ::SMPIOCapability to be applied.
             * @see ::BTSecurityLevel
             * @see ::SMPIOCapability
             * @see getConnSecurityLevel()
             * @see getConnIOCapability()
             * @see setConnSecurityAuto()
             */
            bool setConnSecurity(const BTSecurityLevel sec_level, const SMPIOCapability io_cap=SMPIOCapability::UNSET) noexcept;

            /**
             * Set automatic security negotiation of BTSecurityLevel and SMPIOCapability pairing mode.
             * <p>
             * Disabled by default and if set to ::SMPIOCapability::UNSET
             * </p>
             * Implementation iterates through below setup from highest security to lowest,
             * while performing a full connection attempt for each.
             * <pre>
             * BTSecurityLevel::ENC_AUTH_FIPS, iocap_auto*
             * BTSecurityLevel::ENC_AUTH,      iocap_auto*
             * BTSecurityLevel::ENC_ONLY,      SMPIOCapability::NO_INPUT_NO_OUTPUT
             * BTSecurityLevel::NONE,          SMPIOCapability::NO_INPUT_NO_OUTPUT
             *
             * (*): user SMPIOCapability choice of for authentication IO, skipped if ::SMPIOCapability::NO_INPUT_NO_OUTPUT
             * </pre>
             * <p>
             * Implementation may perform multiple connection and disconnect actions
             * until successful pairing or failure.
             * </p>
             * <p>
             * Intermediate AdapterStatusListener::deviceConnected() and AdapterStatusListener::deviceDisconnected()
             * callbacks are not delivered while negotiating. This avoids any interference by the user application.
             * </p>
             * @param auth_io_cap user SMPIOCapability choice for negotiation
             * @see isConnSecurityAutoEnabled()
             * @see ::BTSecurityLevel
             * @see ::SMPIOCapability
             * @see setConnSecurity()
             */
            bool setConnSecurityAuto(const SMPIOCapability iocap_auto) noexcept;

            /**
             * Returns true if automatic security negotiation has been enabled via setConnSecurityAuto(),
             * otherwise false.
             * @see setConnSecurityAuto()
             */
            bool isConnSecurityAutoEnabled() const noexcept;

            /**
             * Method sets the given passkey entry, see ::PairingMode::PASSKEY_ENTRY_ini.
             * <p>
             * Call this method if the device shall be securely paired with ::PairingMode::PASSKEY_ENTRY_ini,
             * i.e. when notified via AdapterStatusListener::devicePairingState() in state ::SMPPairingState::PASSKEY_EXPECTED.
             * </p>
             *
             * @param passkey used for ::PairingMode::PASSKEY_ENTRY_ini method.
             *        Will be encrypted before sending to counter-party.
             *
             * @return HCIStatusCode::SUCCESS if the command has been accepted, otherwise ::HCIStatusCode may disclose reason for rejection.
             * @see PairingMode
             * @see SMPPairingState
             * @see AdapterStatusListener::devicePairingState()
             * @see setPairingPasskey()
             * @see setPairingNumericComparison()
             * @see getPairingMode()
             * @see getPairingState()
             */
            HCIStatusCode setPairingPasskey(const uint32_t passkey) noexcept;

            /**
             * Method replies with a negative passkey response, i.e. rejection, see ::PairingMode::PASSKEY_ENTRY_ini.
             * <p>
             * You may call this method if the device shall be securely paired with ::PairingMode::PASSKEY_ENTRY_ini,
             * i.e. when notified via AdapterStatusListener::devicePairingState() in state ::SMPPairingState::PASSKEY_EXPECTED.
             * </p>
             * <p>
             * Current experience exposed a roughly 3s immediate disconnect handshake with certain devices and/or Kernel BlueZ code.
             *
             * Hence using setPairingPasskey() with `passkey = 0` is recommended, especially when utilizing
             * automatic security negotiation via setConnSecurityAuto()!
             * </p>
             *
             * @return HCIStatusCode::SUCCESS if the command has been accepted, otherwise ::HCIStatusCode may disclose reason for rejection.
             * @see PairingMode
             * @see SMPPairingState
             * @see AdapterStatusListener::devicePairingState()
             * @see setPairingPasskey()
             * @see setPairingNumericComparison()
             * @see getPairingMode()
             * @see getPairingState()
             */
            HCIStatusCode setPairingPasskeyNegative() noexcept;

            /**
             * Method sets the numeric comparison result, see ::PairingMode::NUMERIC_COMPARE_ini.
             * <p>
             * Call this method if the device shall be securely paired with ::PairingMode::NUMERIC_COMPARE_ini,
             * i.e. when notified via AdapterStatusListener::devicePairingState() in state ::SMPPairingState::NUMERIC_COMPARE_EXPECTED.
             * </p>
             *
             * @param equal used for ::PairingMode::NUMERIC_COMPARE_ini method.
             *        Will be encrypted before sending to counter-party.
             *
             * @return HCIStatusCode::SUCCESS if the command has been accepted, otherwise ::HCIStatusCode may disclose reason for rejection.
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
             * Returns the current ::PairingMode used by the device.
             * <p>
             * If the device is not paired, the current mode is ::PairingMode::NONE.
             * </p>
             * <p>
             * If the Pairing Feature Exchange is completed, i.e. ::SMPPairingState::FEATURE_EXCHANGE_COMPLETED,
             * as notified by AdapterStatusListener::devicePairingState(),
             * the current mode reflects the currently used PairingMode.
             * </p>
             * <p>
             * In case the Pairing Feature Exchange is in progress, the current mode is ::PairingMode::NEGOTIATING.
             * </p>
             * @return current ::PairingMode.
             * @see PairingMode
             * @see SMPPairingState
             * @see AdapterStatusListener::devicePairingState()
             * @see setPairingPasskey()
             * @see setPairingNumericComparison()
             * @see getPairingMode()
             * @see getPairingState()
             */
            PairingMode getPairingMode() const noexcept;

            /**
             * Returns the current ::SMPPairingState.
             * <p>
             * If the device is not paired, the current state is ::SMPPairingState::NONE.
             * </p>
             * @see PairingMode
             * @see SMPPairingState
             * @see AdapterStatusListener::devicePairingState()
             * @see setPairingPasskey()
             * @see setPairingNumericComparison()
             * @see getPairingMode()
             * @see getPairingState()
             */
            SMPPairingState getPairingState() const noexcept;

            /**
             * Disconnects this device via disconnect(..) if getConnected()==true
             * and explicitly removes its shared references from the Adapter:
             * connected-devices, discovered-devices and shared-devices.
             * <p>
             * All added AdapterStatusListener associated with this instance
             * will be removed via BTAdapter::removeAllStatusListener().
             * </p>
             * <p>
             * This method shall be issued to ensure no device reference will
             * be leaked in a long lived adapter,
             * as only its reference within connected-devices and discovered-devices are removed at disconnect.
             * </p>
             * <p>
             * After calling this method, this instance is destroyed and shall not be used anymore!
             * </p>
             * <p>
             * This method is an atomic operation.
             * </p>
             * <p>
             * An application using one thread per device and rapid connect, should either use disconnect() or remove(),
             * but never issue remove() after disconnect() if the device is in use.
             * </p>
             * @see BTAdapter::removeAllStatusListener()
             */
            void remove() noexcept;

            /**
             * Returns the connected GATTHandler or nullptr, see connectGATT(), getGattServices() and disconnect().
             *
             * @return
             * @see connectGATT()
             * @see getGattServices()
             * @see disconnect()
             */
            std::shared_ptr<BTGattHandler> getGattHandler() noexcept;

            /**
             * Returns a list of shared GATTService available on this device if successful,
             * otherwise returns an empty list if an error occurred.
             *
             * Method is only functional on a remote BTDevice in BTRole::Slave, a GATT server (GATTRole::Server),
             * i.e. the local BTAdapter acting as a BTRole::Master GATT client.
             *
             * The HCI connectLE(..) or connectBREDR(..) must be performed first, see {@link #connectDefault()}.
             *
             * If this method has been called for the first time or no services have been detected yet:
             * - the client MTU exchange will be performed
             * - a list of GATTService will be retrieved
             *
             * A GATT connection will be created via connectGATT() if not established yet.
             */
            jau::darray<BTGattServiceRef> getGattServices() noexcept;

            /**
             * Returns the shared GenericAccess instance, retrieved by getGattServices() or nullptr if not available.
             *
             * @return
             * @see getGattServices()
             */
            std::shared_ptr<GattGenericAccessSvc> getGattGenericAccess();

            /**
             * Find a BTGattService by its service_uuid.
             *
             * It will check objects of a connected device using getGattServices().
             *
             * It will not turn on discovery or connect to this remote device.
             *
             * @parameter service_uuid the jau::uuid_t of the desired BTGattService
             * @return The matching service or null if not found
             * @see findGattChar()
             */
            BTGattServiceRef findGattService(const jau::uuid_t& service_uuid) noexcept;

            /**
             * Find a BTGattChar by its service_uuid and char_uuid.
             *
             * It will check objects of this connected device using getGattServices().
             *
             * It will not turn on discovery or connect to this remote device.
             *
             * @parameter service_uuid the jau::uuid_t of the intermediate BTGattService
             * @parameter char_uuid the jau::uuid_t of the desired BTGattChar, within the intermediate BTGattService.
             * @return The matching characteristic or null if not found
             * @since 2.4.0
             * @see findGattService()
             */
            BTGattCharRef findGattChar(const jau::uuid_t& service_uuid, const jau::uuid_t& char_uuid) noexcept;

            /**
             * Find a BTGattChar by its char_uuid only.
             *
             * It will check objects of this connected device using getGattServices().
             *
             * It will not turn on discovery or connect to this remote device.
             *
             * This variation is less efficient than findGattChar() by service_uuid and char_uuid,
             * since it has to traverse through all services.
             *
             * @parameter char_uuid the jau::uuid_t of the desired BTGattChar, within the intermediate BTGattService.
             * @return The matching characteristic or null if not found
             * @since 2.4.0
             * @see findGattService()
             */
            BTGattCharRef findGattChar(const jau::uuid_t& char_uuid) noexcept;

            /**
             * Send a notification event consisting out of the given `value` representing the given characteristic value handle
             * to the connected BTRole::Master.
             *
             * This command is only valid if this BTGattHandler is in role GATTRole::Server.
             *
             * Implementation is not receiving any reply after sending out the indication and returns immediately.
             *
             * @param char_value_handle valid characteristic value handle, must be sourced from referenced DBGattServer
             * @param value the octets to be send
             * @return true if successful, otherwise false
             */
            bool sendNotification(const uint16_t char_value_handle, const jau::TROOctets & value) noexcept;

            /**
             * Send an indication event consisting out of the given `value` representing the given characteristic value handle
             * to the connected BTRole::Master.
             *
             * This command is only valid if this BTGattHandler is in role GATTRole::Server.
             *
             * Implementation awaits the indication reply after sending out the indication.
             *
             * @param char_value_handle valid characteristic value handle, must be sourced from referenced DBGattServer
             * @param value the octets to be send
             * @return true if successful, otherwise false
             */
            bool sendIndication(const uint16_t char_value_handle, const jau::TROOctets & value) noexcept;

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
             * GATT services must have been initialized via getGattServices(), otherwise `false` is being returned.
             * </p>
             * @return `true` if successful, otherwise false in case no GATT services exists or is not connected .. etc.
             */
            bool pingGATT() noexcept;

            /**
             * Add the given BTGattCharListener to the listener list if not already present.
             * <p>
             * Convenience delegation call to GATTHandler
             * </p>
             * <p>
             * To enable the actual BLE notification and/or indication, one needs to call
             * BTGattChar::configNotificationIndication(bool, bool, bool[])
             * or BTGattChar::enableNotificationOrIndication(bool enabledState[2]).
             * </p>
             * @param listener A BTGattCharListener instance, listening to all BluetoothGattCharacteristic events of this device
             * @return true if the given listener is not element of the list and has been newly added, otherwise false.
             * @throws IllegalStateException if the GATTHandler is null, i.e. not connected
             */
            bool addCharListener(const BTGattCharListenerRef& l) noexcept;

            /**
             * Please use BTGattChar::addCharListener() for clarity, merely existing here to allow JNI access.
             */
            bool addCharListener(const BTGattCharListenerRef& l, const BTGattCharRef& d) noexcept;

            /**
             * Remove the given {@link BTGattCharListener} from the listener list.
             * <p>
             * If the GATTHandler is null, i.e. not connected, `false` is being returned.
             * </p>
             * @param listener A {@link BTGattCharListener} instance
             * @return true if the given listener is an element of the list and has been removed, otherwise false.
             */
            bool removeCharListener(const BTGattCharListenerRef& l) noexcept;

            /**
             * Remove all {@link BTGattCharListener} from the list, which are associated to the given {@link BTGattChar}.
             * <p>
             * Implementation tests all listener's BTGattCharListener::match(const BTGattChar & characteristic)
             * to match with the given associated characteristic.
             * </p>
             * @param associatedCharacteristic the match criteria to remove any BTGattCharListener from the list
             * @return number of removed listener.
             */
            int removeAllAssociatedCharListener(const BTGattCharRef& associatedCharacteristic) noexcept;

            int removeAllAssociatedCharListener(const BTGattChar * associatedCharacteristic) noexcept;

            /**
             * Remove all {@link BTGattCharListener} from the list.
             * @return number of removed listener.
             */
            int removeAllCharListener() noexcept;
    };

    inline bool operator==(const BTDevice& lhs, const BTDevice& rhs) noexcept
    { return lhs.getAddressAndType() == rhs.getAddressAndType(); }

    inline bool operator!=(const BTDevice& lhs, const BTDevice& rhs) noexcept
    { return !(lhs == rhs); }

    typedef std::shared_ptr<BTDevice> BTDeviceRef;

} // namespace direct_bt

#endif /* BT_DEVICE_HPP_ */
