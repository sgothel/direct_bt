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

namespace direct_bt {

    // *************************************************
    // *************************************************
    // *************************************************

    class BTAdapter; // forward

    class BTDevice : public BTObject
    {
        friend BTAdapter; // managing us: ctor and update(..) during discovery
        friend BTGattHandler; // may issue detailed disconnect(..)

        private:
            BTAdapter & adapter;
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
            jau::darray<std::shared_ptr<uuid_t>> advServices;
#if SMP_SUPPORTED_BY_OS
            std::shared_ptr<SMPHandler> smpHandler = nullptr;
            std::recursive_mutex mtx_smpHandler;
#endif
            std::shared_ptr<BTGattHandler> gattHandler = nullptr;
            mutable std::recursive_mutex mtx_gattHandler;
            mutable std::recursive_mutex mtx_connect;
            mutable std::recursive_mutex mtx_data;
            std::atomic<bool> isConnected;
            std::atomic<bool> allowDisconnect; // allowDisconnect = isConnected || 'isConnectIssued'

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

                SMPAuthReqs     authReqs_init, authReqs_resp;
                SMPIOCapability ioCap_init,    ioCap_resp;
                SMPOOBDataFlag  oobFlag_init,  oobFlag_resp;
                uint8_t         maxEncsz_init, maxEncsz_resp;
                SMPKeyType      keys_init_exp, keys_resp_exp;
                SMPKeyType      keys_init_has, keys_resp_has;

                // LTK: Set of Long Term Key data: ltk, ediv + rand
                SMPLongTermKeyInfo ltk_init, ltk_resp;

                // IRK
                jau::uint128_t  irk_init,      irk_resp;
                EUI48           address;
                bool            is_static_random_address;

                // CSRK
                SMPSignatureResolvingKeyInfo csrk_init, csrk_resp;
            };
            PairingData pairing_data;
            mutable std::mutex mtx_pairing;
            mutable jau::sc_atomic_bool sync_pairing;
            std::condition_variable cv_pairing_state_changed;

            /** Private class only for private make_shared(). */
            class ctor_cookie { friend BTDevice; ctor_cookie(const uint16_t secret) { (void)secret; } };

            /** Private std::make_shared<BTDevice>(..) vehicle for friends. */
            static std::shared_ptr<BTDevice> make_shared(BTAdapter & adapter, EInfoReport const & r) {
                return std::make_shared<BTDevice>(BTDevice::ctor_cookie(0), adapter, r);
            }

            /** Add advertised service (GAP discovery) */
            bool addAdvService(std::shared_ptr<uuid_t> const &uuid) noexcept;
            /** Add advertised service (GAP discovery) */
            bool addAdvServices(jau::darray<std::shared_ptr<uuid_t>> const & services) noexcept;
            /**
             * Find advertised service (GAP discovery) index
             * @return index >= 0 if found, otherwise -1
             */
            int findAdvService(std::shared_ptr<uuid_t> const &uuid) const noexcept;

            EIRDataType update(EInfoReport const & data) noexcept;
            EIRDataType update(GattGenericAccessSvc const &data, const uint64_t timestamp) noexcept;

            void notifyDisconnected() noexcept;
            void notifyConnected(std::shared_ptr<BTDevice> sthis, const uint16_t handle, const SMPIOCapability io_cap) noexcept;
            void notifyLEFeatures(std::shared_ptr<BTDevice> sthis, const LEFeatures features) noexcept;

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

            bool checkPairingKeyDistributionComplete(const std::string& timestamp) const noexcept;

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
            ~BTDevice() noexcept;

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

            /** Return AppearanceCat of device as recognized at discovery, connect and GATT discovery. */
            AppearanceCat getAppearance() const noexcept { return appearance; }

            std::string const getName() const noexcept;

            /** Return shared ManufactureSpecificData as recognized at discovery, pre GATT discovery. */
            std::shared_ptr<ManufactureSpecificData> const getManufactureSpecificData() const noexcept;

            /**
             * Return a list of advertised services as recognized at discovery, pre GATT discovery.
             * <p>
             * To receive a complete list of GATT services including characteristics etc,
             * use {@link #getGattService()}.
             * </p>
             */
            jau::darray<std::shared_ptr<uuid_t>> getAdvertisedServices() const noexcept;

            std::string toString() const noexcept override { return toString(false); }

            std::string toString(bool includeDiscoveredServices) const noexcept;

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
             * The associated BTAdapter's HCIHandler instance is used to connect,
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
                                    const uint16_t conn_latency=0, const uint16_t supervision_timeout=getHCIConnSupervisorTimeout(0, 15)) noexcept;

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
             */
            SMPKeyType getAvailableSMPKeys(const bool responder) const noexcept;

            /**
             * Returns a copy of the Long Term Key (LTK) info, valid after connection and SMP pairing has been completed.
             * @param responder true will return the responder's LTK info (remote device, LL slave), otherwise the initiator's (the LL master).
             * @return the resulting key. SMPLongTermKeyInfo::enc_size will be zero if invalid.
             * @see ::SMPPairingState::COMPLETED
             * @see AdapterStatusListener::deviceReady()
             */
            SMPLongTermKeyInfo getLongTermKeyInfo(const bool responder) const noexcept;

            /**
             * Sets the long term ket (LTK) info of this device to reuse pre-paired encryption.
             * <p>
             * Must be called before connecting to this device, otherwise HCIStatusCode::CONNECTION_ALREADY_EXISTS will be returned.
             * </p>
             * @param ltk the pre-paired encryption LTK
             * @return ::HCIStatusCode::SUCCESS if successful, otherwise the appropriate error code.
             */
            HCIStatusCode setLongTermKeyInfo(const SMPLongTermKeyInfo& ltk) noexcept;

            /**
             * Returns a copy of the Signature Resolving Key (LTK) info, valid after connection and SMP pairing has been completed.
             * @param responder true will return the responder's LTK info (remote device, LL slave), otherwise the initiator's (the LL master).
             * @return the resulting key
             * @see ::SMPPairingState::COMPLETED
             * @see AdapterStatusListener::deviceReady()
             */
            SMPSignatureResolvingKeyInfo getSignatureResolvingKeyInfo(const bool responder) const noexcept;

            /**
             * Unpairs this device from the adapter while staying connected.
             * <p>
             * All keys will be cleared within the adapter and host implementation.<br>
             * Should rarely being used by user.<br>
             * Internally being used to re-start pairing if GATT connection fails
             * in PairingMode::PRE_PAIRED mode.
             * </p>
             * @return HCIStatusCode::SUCCESS or an appropriate error status.
             */
            HCIStatusCode unpair() noexcept;

            /**
             * Experimental only.
             * <pre>
             *   adapter.stopDiscovery(): Renders pairDevice(..) to fail: Busy!
             *   pairDevice(..) behaves quite instable within our connected workflow: Not used!
             * </pre>
             */
            HCIStatusCode pair(const SMPIOCapability io_cap) noexcept;

            /**
             * Set the ::BTSecurityLevel used to connect to this device on the upcoming connection.
             * <p>
             * Method returns false if ::BTSecurityLevel::UNSET has been given,
             * operation fails, this device has already being connected,
             * or BTDevice::connectLE() or BTDevice::connectBREDR() has been issued already.
             * </p>
             * <p>
             * To ensure a consistent authentication setup,
             * it is advised to set ::SMPIOCapability::NO_INPUT_NO_OUTPUT for sec_level <= ::BTSecurityLevel::ENC_ONLY
             * using setConnSecurity() as well as an IO capable ::SMPIOCapability value
             * for ::BTSecurityLevel::ENC_AUTH or ::BTSecurityLevel::ENC_AUTH_FIPS.<br>
             * You may like to consider using setConnSecurityBest().
             * </p>
             * @param sec_level ::BTSecurityLevel to be applied, ::BTSecurityLevel::UNSET will be ignored and method fails.
             * @see ::BTSecurityLevel
             * @see ::SMPIOCapability
             * @see getConnSecurityLevel()
             * @see setConnIOCapability()
             * @see getConnIOCapability()
             * @see setConnSecurity()
             * @see setConnSecurityBest()
             */
            bool setConnSecurityLevel(const BTSecurityLevel sec_level) noexcept;

            /**
             * Return the ::BTSecurityLevel, determined when the connection is established.
             * @see ::BTSecurityLevel
             * @see ::SMPIOCapability
             * @see setConnSecurityLevel()
             * @see setConnIOCapability()
             * @see getConnIOCapability()
             * @see setConnSecurity()
             * @see setConnSecurityBest()
             */
            BTSecurityLevel getConnSecurityLevel() const noexcept;

            /**
             * Sets the given ::SMPIOCapability used to connect to this device on the upcoming connection.
             * <p>
             * Method returns false if ::SMPIOCapability::UNSET has been given,
             * operation fails, this device has already being connected,
             * or BTDevice::connectLE() or BTDevice::connectBREDR() has been issued already.
             * </p>
             * @param[in] io_cap ::SMPIOCapability to be applied, ::SMPIOCapability::UNSET will be ignored and method fails.
             * @see ::BTSecurityLevel
             * @see ::SMPIOCapability
             * @see setConnSecurityLevel()
             * @see getConnSecurityLevel()
             * @see getConnIOCapability()
             * @see setConnSecurity()
             * @see setConnSecurityBest()
             */
            bool setConnIOCapability(const SMPIOCapability io_cap) noexcept;

            /**
             * Return the set ::SMPIOCapability value, determined when the connection is established.
             * @see ::BTSecurityLevel
             * @see ::SMPIOCapability
             * @see setConnSecurityLevel()
             * @see getConnSecurityLevel()
             * @see setConnIOCapability()
             * @see setConnSecurity()
             * @see setConnSecurityBest()
             */
            SMPIOCapability getConnIOCapability() const noexcept;

            /**
             * Sets the given ::BTSecurityLevel and ::SMPIOCapability used to connect to this device on the upcoming connection.
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
             * @see setConnSecurityLevel()
             * @see getConnSecurityLevel()
             * @see setConnIOCapability()
             * @see getConnIOCapability()
             * @see setConnSecurityBest()
             */
            bool setConnSecurity(const BTSecurityLevel sec_level, const SMPIOCapability io_cap) noexcept;

            /**
             * Convenience method to determine the best practice ::BTSecurityLevel and ::SMPIOCapability
             * based on the given arguments, used to connect to this device on the upcoming connection.
             * <pre>
             *   if( BTSecurityLevel::UNSET < sec_level && SMPIOCapability::UNSET != io_cap ) {
             *      return setConnSecurity(sec_level, io_cap);
             *   } else if( BTSecurityLevel::UNSET < sec_level ) {
             *       if( BTSecurityLevel::ENC_ONLY >= sec_level ) {
             *           return setConnSecurity(sec_level, SMPIOCapability::NO_INPUT_NO_OUTPUT);
             *       } else {
             *           return setConnSecurityLevel(sec_level);
             *       }
             *   } else if( SMPIOCapability::UNSET != io_cap ) {
             *       return setConnIOCapability(io_cap);
             *   } else {
             *       return false;
             *   }
             * </pre>
             * <p>
             * Method returns false if ::BTSecurityLevel::UNSET and ::SMPIOCapability::UNSET has been given,
             * operation fails, this device has already being connected,
             * or BTDevice::connectLE() or BTDevice::connectBREDR() has been issued already.
             * </p>
             * <p>
             * Method either changes both parameter for the upcoming connection or none at all.
             * </p>
             * @param[in] sec_level ::BTSecurityLevel to be applied.
             * @param[in] io_cap ::SMPIOCapability to be applied.
             * @see ::BTSecurityLevel
             * @see ::SMPIOCapability
             * @see setConnSecurityLevel()
             * @see getConnSecurityLevel()
             * @see setConnIOCapability()
             * @see getConnIOCapability()
             * @see setConnSecurityBest()
             * @see setConnSecurityAuto()
             */
            bool setConnSecurityBest(const BTSecurityLevel sec_level, const SMPIOCapability io_cap) noexcept;

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
             */
            void remove() noexcept;

            /** Returns the connected GATTHandler or nullptr, see connectGATT(), getGattService() and disconnect(). */
            std::shared_ptr<BTGattHandler> getGattHandler() noexcept;

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
            jau::darray<std::shared_ptr<BTGattService>> getGattServices() noexcept;

            /**
             * Returns the matching GATTService for the given uuid.
             * <p>
             * Implementation calls getGattService().
             * </p>
             */
            std::shared_ptr<BTGattService> findGattService(std::shared_ptr<uuid_t> const &uuid);

            /** Returns the shared GenericAccess instance, retrieved by {@link #getGattService()} or nullptr if not available. */
            std::shared_ptr<GattGenericAccessSvc> getGattGenericAccess();

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
             * GATT services must have been initialized via {@link #getGattService()}, otherwise `false` is being returned.
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
            bool addCharListener(std::shared_ptr<BTGattCharListener> l);

            /**
             * Remove the given {@link BTGattCharListener} from the listener list.
             * <p>
             * If the GATTHandler is null, i.e. not connected, `false` is being returned.
             * </p>
             * @param listener A {@link BTGattCharListener} instance
             * @return true if the given listener is an element of the list and has been removed, otherwise false.
             */
            bool removeCharListener(std::shared_ptr<BTGattCharListener> l) noexcept;

            /**
             * Remove all {@link BTGattCharListener} from the list, which are associated to the given {@link BTGattChar}.
             * <p>
             * Implementation tests all listener's BTGattCharListener::match(const BTGattChar & characteristic)
             * to match with the given associated characteristic.
             * </p>
             * @param associatedCharacteristic the match criteria to remove any BTGattCharListener from the list
             * @return number of removed listener.
             */
            int removeAllAssociatedCharListener(std::shared_ptr<BTGattChar> associatedCharacteristic) noexcept;

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

} // namespace direct_bt

#endif /* BT_DEVICE_HPP_ */
