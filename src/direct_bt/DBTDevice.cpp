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

#include <cstring>
#include <string>
#include <memory>
#include <cstdint>
#include <vector>
#include <cstdio>

#include  <algorithm>

// #define VERBOSE_ON 1
#include <jau/environment.hpp>
#include <jau/debug.hpp>

#include "HCIComm.hpp"

#include "DBTDevice.hpp"
#include "DBTAdapter.hpp"

using namespace direct_bt;

DBTDevice::DBTDevice(DBTAdapter & a, EInfoReport const & r)
: adapter(a),
  l2cap_att(adapter.getAddress(), L2CAP_PSM_UNDEF, L2CAP_CID_ATT),
  ts_creation(r.getTimestamp()),
  address(r.getAddress()), addressType(r.getAddressType()),
  leRandomAddressType(address.getBLERandomAddressType(addressType))
{
    ts_last_discovery = ts_creation;
    hciConnHandle = 0;
    le_features = LEFeatures::NONE;
    isConnected = false;
    allowDisconnect = false;
    if( !r.isSet(EIRDataType::BDADDR) ) {
        throw jau::IllegalArgumentException("Address not set: "+r.toString(false), E_FILE_LINE);
    }
    if( !r.isSet(EIRDataType::BDADDR_TYPE) ) {
        throw jau::IllegalArgumentException("AddressType not set: "+r.toString(false), E_FILE_LINE);
    }
    update(r);

    if( BDAddressType::BDADDR_LE_RANDOM == addressType ) {
        if( BLERandomAddressType::UNDEFINED == leRandomAddressType ) {
            throw jau::IllegalArgumentException("BDADDR_LE_RANDOM: Invalid BLERandomAddressType "+
                    getBLERandomAddressTypeString(leRandomAddressType)+": "+toString(false), E_FILE_LINE);
        }
    } else {
        if( BLERandomAddressType::UNDEFINED != leRandomAddressType ) {
            throw jau::IllegalArgumentException("Not BDADDR_LE_RANDOM: Invalid given native BLERandomAddressType "+
                    getBLERandomAddressTypeString(leRandomAddressType)+": "+toString(false), E_FILE_LINE);
        }
    }
}

DBTDevice::~DBTDevice() noexcept {
    DBG_PRINT("DBTDevice::dtor: ... %p %s", this, getAddressString().c_str());
    advServices.clear();
    advMSD = nullptr;
    DBG_PRINT("DBTDevice::dtor: XXX %p %s", this, getAddressString().c_str());
}

std::shared_ptr<DBTDevice> DBTDevice::getSharedInstance() const noexcept {
    return adapter.getSharedDevice(*this);
}

bool DBTDevice::addAdvService(std::shared_ptr<uuid_t> const &uuid) noexcept
{
    if( 0 > findAdvService(uuid) ) {
        advServices.push_back(uuid);
        return true;
    }
    return false;
}
bool DBTDevice::addAdvServices(std::vector<std::shared_ptr<uuid_t>> const & services) noexcept
{
    bool res = false;
    for(size_t j=0; j<services.size(); j++) {
        const std::shared_ptr<uuid_t> uuid = services.at(j);
        res = addAdvService(uuid) || res;
    }
    return res;
}

int DBTDevice::findAdvService(std::shared_ptr<uuid_t> const &uuid) const noexcept
{
    const size_t size = advServices.size();
    for (size_t i = 0; i < size; i++) {
        const std::shared_ptr<uuid_t> & e = advServices[i];
        if ( nullptr != e && *uuid == *e ) {
            return i;
        }
    }
    return -1;
}

std::string const DBTDevice::getName() const noexcept {
    const std::lock_guard<std::recursive_mutex> lock(const_cast<DBTDevice*>(this)->mtx_data); // RAII-style acquire and relinquish via destructor
    return name;
}

std::shared_ptr<ManufactureSpecificData> const DBTDevice::getManufactureSpecificData() const noexcept {
    const std::lock_guard<std::recursive_mutex> lock(const_cast<DBTDevice*>(this)->mtx_data); // RAII-style acquire and relinquish via destructor
    return advMSD;
}

std::vector<std::shared_ptr<uuid_t>> DBTDevice::getAdvertisedServices() const noexcept {
    const std::lock_guard<std::recursive_mutex> lock(const_cast<DBTDevice*>(this)->mtx_data); // RAII-style acquire and relinquish via destructor
    return advServices;
}

std::string DBTDevice::toString(bool includeDiscoveredServices) const noexcept {
    const std::lock_guard<std::recursive_mutex> lock(const_cast<DBTDevice*>(this)->mtx_data); // RAII-style acquire and relinquish via destructor
    const uint64_t t0 = jau::getCurrentMilliseconds();
    std::string leaddrtype;
    if( BLERandomAddressType::UNDEFINED != leRandomAddressType ) {
        leaddrtype = ", random "+getBLERandomAddressTypeString(leRandomAddressType);
    }
    jau::sc_atomic_critical sync(const_cast<DBTDevice*>(this)->sync_pairing);
    std::string msdstr = nullptr != advMSD ? advMSD->toString() : "MSD[null]";
    std::string out("Device[address["+getAddressString()+", "+getBDAddressTypeString(getAddressType())+leaddrtype+"], name['"+name+
            "'], age[total "+std::to_string(t0-ts_creation)+", ldisc "+std::to_string(t0-ts_last_discovery)+", lup "+std::to_string(t0-ts_last_update)+
            "]ms, connected["+std::to_string(allowDisconnect)+"/"+std::to_string(isConnected)+", handle "+jau::uint16HexString(hciConnHandle)+
            ", sec[lvl "+getBTSecurityLevelString(pairing_data.sec_level_conn).c_str()+", io "+getSMPIOCapabilityString(pairing_data.ioCap_conn).c_str()+
            ", pairing "+getPairingModeString(pairing_data.mode).c_str()+", state "+getSMPPairingStateString(pairing_data.state).c_str()+"]], rssi "+std::to_string(getRSSI())+
            ", tx-power "+std::to_string(tx_power)+
            ", appearance "+jau::uint16HexString(static_cast<uint16_t>(appearance))+" ("+getAppearanceCatString(appearance)+
            "), "+msdstr+", "+javaObjectToString()+"]");
    if(includeDiscoveredServices && advServices.size() > 0 ) {
        out.append("\n");
        const size_t size = advServices.size();
        for (size_t i = 0; i < size; i++) {
            const std::shared_ptr<uuid_t> & e = advServices[i];
            if( 0 < i ) {
                out.append("\n");
            }
            out.append("  ").append(e->toUUID128String()).append(", ").append(std::to_string(static_cast<int>(e->getTypeSize()))).append(" bytes");
        }
    }
    return out;
}

EIRDataType DBTDevice::update(EInfoReport const & data) noexcept {
    const std::lock_guard<std::recursive_mutex> lock(mtx_data); // RAII-style acquire and relinquish via destructor

    EIRDataType res = EIRDataType::NONE;
    ts_last_update = data.getTimestamp();
    if( data.isSet(EIRDataType::BDADDR) ) {
        if( data.getAddress() != this->address ) {
            WARN_PRINT("DBTDevice::update:: BDADDR update not supported: %s for %s",
                    data.toString().c_str(), this->toString(false).c_str());
        }
    }
    if( data.isSet(EIRDataType::BDADDR_TYPE) ) {
        if( data.getAddressType() != this->addressType ) {
            WARN_PRINT("DBTDevice::update:: BDADDR_TYPE update not supported: %s for %s",
                    data.toString().c_str(), this->toString(false).c_str());
        }
    }
    if( data.isSet(EIRDataType::NAME) ) {
        if( 0 == name.length() || data.getName().length() > name.length() ) {
            name = data.getName();
            setEIRDataTypeSet(res, EIRDataType::NAME);
        }
    }
    if( data.isSet(EIRDataType::NAME_SHORT) ) {
        if( 0 == name.length() ) {
            name = data.getShortName();
            setEIRDataTypeSet(res, EIRDataType::NAME_SHORT);
        }
    }
    if( data.isSet(EIRDataType::RSSI) ) {
        if( rssi != data.getRSSI() ) {
            rssi = data.getRSSI();
            setEIRDataTypeSet(res, EIRDataType::RSSI);
        }
    }
    if( data.isSet(EIRDataType::TX_POWER) ) {
        if( tx_power != data.getTxPower() ) {
            tx_power = data.getTxPower();
            setEIRDataTypeSet(res, EIRDataType::TX_POWER);
        }
    }
    if( data.isSet(EIRDataType::APPEARANCE) ) {
        if( appearance != data.getAppearance() ) {
            appearance = data.getAppearance();
            setEIRDataTypeSet(res, EIRDataType::APPEARANCE);
        }
    }
    if( data.isSet(EIRDataType::MANUF_DATA) ) {
        if( advMSD != data.getManufactureSpecificData() ) {
            advMSD = data.getManufactureSpecificData();
            setEIRDataTypeSet(res, EIRDataType::MANUF_DATA);
        }
    }
    if( addAdvServices( data.getServices() ) ) {
        setEIRDataTypeSet(res, EIRDataType::SERVICE_UUID);
    }
    return res;
}

EIRDataType DBTDevice::update(GattGenericAccessSvc const &data, const uint64_t timestamp) noexcept {
    const std::lock_guard<std::recursive_mutex> lock(mtx_data); // RAII-style acquire and relinquish via destructor

    EIRDataType res = EIRDataType::NONE;
    ts_last_update = timestamp;
    if( 0 == name.length() || data.deviceName.length() > name.length() ) {
        name = data.deviceName;
        setEIRDataTypeSet(res, EIRDataType::NAME);
    }
    if( appearance != data.appearance ) {
        appearance = data.appearance;
        setEIRDataTypeSet(res, EIRDataType::APPEARANCE);
    }
    return res;
}

std::shared_ptr<ConnectionInfo> DBTDevice::getConnectionInfo() noexcept {
    DBTManager & mgmt = adapter.getManager();
    std::shared_ptr<ConnectionInfo> connInfo = mgmt.getConnectionInfo(adapter.dev_id, address, addressType);
    if( nullptr != connInfo ) {
        EIRDataType updateMask = EIRDataType::NONE;
        if( rssi != connInfo->getRSSI() ) {
            rssi = connInfo->getRSSI();
            setEIRDataTypeSet(updateMask, EIRDataType::RSSI);
        }
        if( tx_power != connInfo->getTxPower() ) {
            tx_power = connInfo->getTxPower();
            setEIRDataTypeSet(updateMask, EIRDataType::TX_POWER);
        }
        if( EIRDataType::NONE != updateMask ) {
            std::shared_ptr<DBTDevice> sharedInstance = getSharedInstance();
            if( nullptr == sharedInstance ) {
                ERR_PRINT("DBTDevice::getConnectionInfo: Device unknown to adapter and not tracked: %s", toString(false).c_str());
            } else {
                adapter.sendDeviceUpdated("getConnectionInfo", sharedInstance, jau::getCurrentMilliseconds(), updateMask);
            }
        }
    }
    return connInfo;
}

HCIStatusCode DBTDevice::connectLE(uint16_t le_scan_interval, uint16_t le_scan_window,
                                   uint16_t conn_interval_min, uint16_t conn_interval_max,
                                   uint16_t conn_latency, uint16_t supervision_timeout)
{
    const std::lock_guard<std::recursive_mutex> lock_conn(mtx_connect); // RAII-style acquire and relinquish via destructor
    if( !adapter.isPowered() ) {
        WARN_PRINT("DBTDevice::connectLE: Adapter not powered: %s", adapter.toString().c_str());
        return HCIStatusCode::UNSPECIFIED_ERROR;
    }
    HCILEOwnAddressType hci_own_mac_type;
    HCILEPeerAddressType hci_peer_mac_type;

    switch( addressType ) {
        case BDAddressType::BDADDR_LE_PUBLIC:
            hci_peer_mac_type = HCILEPeerAddressType::PUBLIC;
            hci_own_mac_type = HCILEOwnAddressType::PUBLIC;
            break;
        case BDAddressType::BDADDR_LE_RANDOM: {
                switch( leRandomAddressType ) {
                    case BLERandomAddressType::UNRESOLVABLE_PRIVAT:
                        hci_peer_mac_type = HCILEPeerAddressType::RANDOM;
                        hci_own_mac_type = HCILEOwnAddressType::RANDOM;
                        ERR_PRINT("LE Random address type '%s' not supported yet: %s",
                                getBLERandomAddressTypeString(leRandomAddressType).c_str(), toString(false).c_str());
                        return HCIStatusCode::UNACCEPTABLE_CONNECTION_PARAM;
                    case BLERandomAddressType::RESOLVABLE_PRIVAT:
                        hci_peer_mac_type = HCILEPeerAddressType::PUBLIC_IDENTITY;
                        hci_own_mac_type = HCILEOwnAddressType::RESOLVABLE_OR_PUBLIC;
                        ERR_PRINT("LE Random address type '%s' not supported yet: %s",
                                getBLERandomAddressTypeString(leRandomAddressType).c_str(), toString(false).c_str());
                        return HCIStatusCode::UNACCEPTABLE_CONNECTION_PARAM;
                    case BLERandomAddressType::STATIC_PUBLIC:
                        // FIXME: This only works for a static random address not changing at all,
                        // i.e. between power-cycles - hence a temporary hack.
                        // We need to use 'resolving list' and/or LE Set Privacy Mode (HCI) for all devices.
                        hci_peer_mac_type = HCILEPeerAddressType::RANDOM;
                        hci_own_mac_type = HCILEOwnAddressType::PUBLIC;
                        break;
                    default: {
                        ERR_PRINT("Can't connectLE to LE Random address type '%s': %s",
                                getBLERandomAddressTypeString(leRandomAddressType).c_str(), toString(false).c_str());
                        return HCIStatusCode::UNACCEPTABLE_CONNECTION_PARAM;
                    }
                }
            } break;
        default: {
                ERR_PRINT("Can't connectLE to address type '%s': %s", getBDAddressTypeString(addressType).c_str(), toString(false).c_str());
                return HCIStatusCode::UNACCEPTABLE_CONNECTION_PARAM;
            }
    }

    if( isConnected ) {
        ERR_PRINT("DBTDevice::connectLE: Already connected: %s", toString(false).c_str());
        return HCIStatusCode::CONNECTION_ALREADY_EXISTS;
    }

    HCIHandler &hci = adapter.getHCI();
    if( !hci.isOpen() ) {
        ERR_PRINT("DBTDevice::connectLE: HCI closed: %s", toString(false).c_str());
        return HCIStatusCode::INTERNAL_FAILURE;
    }

    {
        jau::sc_atomic_critical sync(sync_pairing);
        if( !adapter.lockConnect(*this, true /* wait */, pairing_data.ioCap_user) ) {
            ERR_PRINT("DBTDevice::connectLE: adapter::lockConnect() failed: %s", toString(false).c_str());
            return HCIStatusCode::INTERNAL_FAILURE;
        }
    }
    HCIStatusCode status = hci.le_create_conn(address,
                                      hci_peer_mac_type, hci_own_mac_type,
                                      le_scan_interval, le_scan_window, conn_interval_min, conn_interval_max,
                                      conn_latency, supervision_timeout);
    allowDisconnect = true;
    if( HCIStatusCode::COMMAND_DISALLOWED == status ) {
        WARN_PRINT("DBTDevice::connectLE: Could not yet create connection: status 0x%2.2X (%s), errno %d, hci-atype[peer %s, own %s] %s on %s",
                static_cast<uint8_t>(status), getHCIStatusCodeString(status).c_str(), errno, strerror(errno),
                getHCILEPeerAddressTypeString(hci_peer_mac_type).c_str(),
                getHCILEOwnAddressTypeString(hci_own_mac_type).c_str(),
                toString(false).c_str());
        adapter.unlockConnect(*this);
    } else if ( HCIStatusCode::SUCCESS != status ) {
        ERR_PRINT("DBTDevice::connectLE: Could not create connection: status 0x%2.2X (%s), errno %d %s, hci-atype[peer %s, own %s] on %s",
                static_cast<uint8_t>(status), getHCIStatusCodeString(status).c_str(), errno, strerror(errno),
                getHCILEPeerAddressTypeString(hci_peer_mac_type).c_str(),
                getHCILEOwnAddressTypeString(hci_own_mac_type).c_str(),
                toString(false).c_str());
        adapter.unlockConnect(*this);
    }
    return status;
}

HCIStatusCode DBTDevice::connectBREDR(const uint16_t pkt_type, const uint16_t clock_offset, const uint8_t role_switch)
{
    const std::lock_guard<std::recursive_mutex> lock_conn(mtx_connect); // RAII-style acquire and relinquish via destructor
    if( !adapter.isPowered() ) {
        WARN_PRINT("DBTDevice::connectBREDR: Adapter not powered: %s", adapter.toString().c_str());
        return HCIStatusCode::UNSPECIFIED_ERROR;
    }

    if( isConnected ) {
        ERR_PRINT("DBTDevice::connectBREDR: Already connected: %s", toString(false).c_str());
        return HCIStatusCode::CONNECTION_ALREADY_EXISTS;
    }
    if( !isBREDRAddressType() ) {
        ERR_PRINT("DBTDevice::connectBREDR: Not a BDADDR_BREDR address: %s", toString(false).c_str());
        return HCIStatusCode::UNACCEPTABLE_CONNECTION_PARAM;
    }

    HCIHandler &hci = adapter.getHCI();
    if( !hci.isOpen() ) {
        ERR_PRINT("DBTDevice::connectBREDR: HCI closed: %s", toString(false).c_str());
        return HCIStatusCode::INTERNAL_FAILURE;
    }

    {
        jau::sc_atomic_critical sync(sync_pairing);
        if( !adapter.lockConnect(*this, true /* wait */, pairing_data.ioCap_user) ) {
            ERR_PRINT("DBTDevice::connectBREDR: adapter::lockConnect() failed: %s", toString(false).c_str());
            return HCIStatusCode::INTERNAL_FAILURE;
        }
    }
    HCIStatusCode status = hci.create_conn(address, pkt_type, clock_offset, role_switch);
    allowDisconnect = true;
    if ( HCIStatusCode::SUCCESS != status ) {
        ERR_PRINT("DBTDevice::connectBREDR: Could not create connection: status 0x%2.2X (%s), errno %d %s on %s",
                static_cast<uint8_t>(status), getHCIStatusCodeString(status).c_str(), errno, strerror(errno), toString(false).c_str());
        adapter.unlockConnect(*this);
    }
    return status;
}

HCIStatusCode DBTDevice::connectDefault()
{
    switch( addressType ) {
        case BDAddressType::BDADDR_LE_PUBLIC:
            [[fallthrough]];
        case BDAddressType::BDADDR_LE_RANDOM:
            return connectLE();
        case BDAddressType::BDADDR_BREDR:
            return connectBREDR();
        default:
            ERR_PRINT("DBTDevice::connectDefault: Not a valid address type: %s", toString(false).c_str());
            return HCIStatusCode::UNACCEPTABLE_CONNECTION_PARAM;
    }
}

void DBTDevice::notifyConnected(std::shared_ptr<DBTDevice> sthis, const uint16_t handle, const SMPIOCapability io_cap) noexcept {
    // coming from connected callback, update state and spawn-off connectGATT in background if appropriate (LE)
    jau::sc_atomic_critical sync(sync_pairing);
    DBG_PRINT("DBTDevice::notifyConnected: handle %s -> %s, io %s -> %s, %s",
              jau::uint16HexString(hciConnHandle).c_str(), jau::uint16HexString(handle).c_str(),
              getSMPIOCapabilityString(pairing_data.ioCap_conn).c_str(), getSMPIOCapabilityString(io_cap).c_str(),
              toString(false).c_str());
    clearSMPStates(true /* connected */);
    allowDisconnect = true;
    isConnected = true;
    hciConnHandle = handle;
    if( SMPIOCapability::UNSET == pairing_data.ioCap_conn ) {
        pairing_data.ioCap_conn = io_cap;
    }
    (void)sthis; // not used yet
}

void DBTDevice::notifyLEFeatures(std::shared_ptr<DBTDevice> sthis, const LEFeatures features) noexcept {
    DBG_PRINT("DBTDevice::notifyLEFeatures: LE_Encryption %d, %s",
            isLEFeaturesBitSet(features, LEFeatures::LE_Encryption), toString(false).c_str());
    le_features = features;

    if( isLEAddressType() && !l2cap_att.isOpen() ) {
        std::thread bg(&DBTDevice::processL2CAPSetup, this, sthis); // @suppress("Invalid arguments")
        bg.detach();
    }
}

void DBTDevice::processL2CAPSetup(std::shared_ptr<DBTDevice> sthis) {
    const std::lock_guard<std::mutex> lock(mtx_pairing); // RAII-style acquire and relinquish via destructor
    jau::sc_atomic_critical sync(sync_pairing);

    DBG_PRINT("DBTDevice::processL2CAPSetup: Start %s", toString(false).c_str());
    if( isLEAddressType() && !l2cap_att.isOpen() ) {
        const BTSecurityLevel sec_level_user = pairing_data.sec_level_user;
        const SMPIOCapability io_cap_conn = pairing_data.ioCap_conn;
        BTSecurityLevel sec_level { BTSecurityLevel::UNSET };

        const bool responderLikesEncryption = pairing_data.res_requested_sec || isLEFeaturesBitSet(le_features, LEFeatures::LE_Encryption);
        if( BTSecurityLevel::UNSET != sec_level_user ) {
            sec_level = sec_level_user;
        } else if( SMPIOCapability::NO_INPUT_NO_OUTPUT == io_cap_conn ) {
            sec_level = BTSecurityLevel::ENC_ONLY; // no auth w/o I/O
        } else {
            if( responderLikesEncryption && adapter.hasSecureConnections() ) {
                sec_level = BTSecurityLevel::ENC_AUTH_FIPS;
            } else if( responderLikesEncryption ) {
                sec_level = BTSecurityLevel::ENC_AUTH;
            } else {
                sec_level = BTSecurityLevel::NONE;
            }
        }

        pairing_data.sec_level_conn = sec_level;
        DBG_PRINT("DBTDevice::processL2CAPSetup: sec_level_user %s, io_cap_conn %s -> sec_level %s",
                getBTSecurityLevelString(sec_level_user).c_str(),
                getSMPIOCapabilityString(io_cap_conn).c_str(),
                getBTSecurityLevelString(sec_level).c_str());

        const bool l2cap_open = l2cap_att.open(*this, sec_level); // initiates hciSMPMsgCallback() if sec_level > BT_SECURITY_LOW
        const bool l2cap_enc = l2cap_open && BTSecurityLevel::NONE < sec_level;
#if SMP_SUPPORTED_BY_OS
        const bool smp_enc = connectSMP(sthis, sec_level) && BTSecurityLevel::NONE < sec_level;
#else
        const bool smp_enc = false;
#endif
        DBG_PRINT("DBTDevice::processL2CAPSetup: lvl %s, connect[smp_enc %d, l2cap[open %d, enc %d]]",
                getBTSecurityLevelString(sec_level).c_str(), smp_enc, l2cap_open, l2cap_enc);

        adapter.unlockConnect(*this);

        if( !l2cap_open ) {
            pairing_data.sec_level_conn = BTSecurityLevel::NONE;
            disconnect(HCIStatusCode::INTERNAL_FAILURE);
        } else if( !l2cap_enc ) {
            processDeviceReady(sthis, jau::getCurrentMilliseconds());
        }
    }
    DBG_PRINT("DBTDevice::processL2CAPSetup: End %s", toString(false).c_str());
}

void DBTDevice::processDeviceReady(std::shared_ptr<DBTDevice> sthis, const uint64_t timestamp) {
    DBG_PRINT("DBTDevice::processDeviceReady: %s", toString(false).c_str());
    PairingMode pmode;
    {
        jau::sc_atomic_critical sync(sync_pairing);
        pmode = pairing_data.mode;
    }
    HCIStatusCode unpair_res = HCIStatusCode::UNKNOWN;

    if( PairingMode::PRE_PAIRED == pmode ) {
        // Delay GATT processing when re-using encryption keys.
        // Here we lack of further processing / state indication
        // and a too fast GATT access leads to disconnection.
        // (Empirical delay figured by accident.)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    const bool res1 = connectGATT(sthis); // may close connection and hence clear pairing_data

    if( !res1 && PairingMode::PRE_PAIRED == pmode ) {
        // Need to repair as GATT communication failed
        unpair_res = unpair();
    }
    DBG_PRINT("DBTDevice::processDeviceReady: ready[GATT %d, unpair %s], %s",
            res1, getHCIStatusCodeString(unpair_res).c_str(), toString(false).c_str());
    if( res1 ) {
        adapter.sendDeviceReady(sthis, timestamp);
    }
}


bool DBTDevice::updatePairingState(std::shared_ptr<DBTDevice> sthis, std::shared_ptr<MgmtEvent> evt, const HCIStatusCode evtStatus, SMPPairingState claimed_state) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_pairing); // RAII-style acquire and relinquish via destructor
    jau::sc_atomic_critical sync(sync_pairing);

    const MgmtEvent::Opcode mgmtEvtOpcode = evt->getOpcode();
    PairingMode mode = pairing_data.mode;
    bool is_device_ready = false;

    if( pairing_data.state != claimed_state ) {
        // Potentially force update PairingMode by forced state change, assuming being the initiator.
        // FIXME: Initiator and responder role might need more specific determination and documentation.
        switch( claimed_state ) {
            case SMPPairingState::NONE:
                // no change
                claimed_state = pairing_data.state;
                break;
            case SMPPairingState::FAILED: {
                // ignore here, let hciSMPMsgCallback() handle auth failure.
                claimed_state = pairing_data.state;
            } break;
            case SMPPairingState::PASSKEY_EXPECTED:
                mode = PairingMode::PASSKEY_ENTRY_ini;
                break;
            case SMPPairingState::NUMERIC_COMPARE_EXPECTED:
                mode = PairingMode::NUMERIC_COMPARE_ini;
                break;
            case SMPPairingState::OOB_EXPECTED:
                mode = PairingMode::OUT_OF_BAND;
                break;
            case SMPPairingState::COMPLETED:
                if( MgmtEvent::Opcode::HCI_ENC_CHANGED == mgmtEvtOpcode &&
                    HCIStatusCode::SUCCESS == evtStatus &&
                    SMPPairingState::FEATURE_EXCHANGE_STARTED > pairing_data.state )
                {
                    // No SMP pairing in process (maybe REQUESTED_BY_RESPONDER at maximum),
                    // i.e. already paired, reusing keys and usable connection
                    mode = PairingMode::PRE_PAIRED;
                    is_device_ready = true;
                } else if( MgmtEvent::Opcode::PAIR_DEVICE_COMPLETE == mgmtEvtOpcode &&
                           HCIStatusCode::ALREADY_PAIRED == evtStatus &&
                           SMPPairingState::FEATURE_EXCHANGE_STARTED > pairing_data.state )
                {
                    // No SMP pairing in process (maybe REQUESTED_BY_RESPONDER at maximum),
                    // i.e. already paired, reusing keys and usable connection
                    mode = PairingMode::PRE_PAIRED;
                    is_device_ready = true;
                } else {
                    // Ignore: Undesired event or SMP pairing is in process, which needs to be completed.
                    claimed_state = pairing_data.state;
                }
                break;
            default: // use given state as-is
                break;
        }
    }

    if( pairing_data.state != claimed_state ) {
        DBG_PRINT("DBTDevice::updatePairingState.0: state %s -> %s, mode %s -> %s, ready %d, %s",
            getSMPPairingStateString(pairing_data.state).c_str(), getSMPPairingStateString(claimed_state).c_str(),
            getPairingModeString(pairing_data.mode).c_str(), getPairingModeString(mode).c_str(),
            is_device_ready, evt->toString().c_str());

        pairing_data.mode = mode;
        pairing_data.state = claimed_state;

        adapter.sendDevicePairingState(sthis, claimed_state, mode, evt->getTimestamp());

        if( is_device_ready ) {
            std::thread dc(&DBTDevice::processDeviceReady, this, sthis, evt->getTimestamp()); // @suppress("Invalid arguments")
            dc.detach();
        }
        DBG_PRINT("DBTDevice::updatePairingState.2: End Complete: state %s, %s",
                getSMPPairingStateString(claimed_state).c_str(), toString(false).c_str());
        return true;
    } else {
        DBG_PRINT("DBTDevice::updatePairingState.3: End Unchanged: state %s, %s, %s",
            getSMPPairingStateString(pairing_data.state).c_str(),
            evt->toString().c_str(), toString(false).c_str());
    }
    return false;
}

void DBTDevice::hciSMPMsgCallback(std::shared_ptr<DBTDevice> sthis, std::shared_ptr<const SMPPDUMsg> msg, const HCIACLData::l2cap_frame& source) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_pairing); // RAII-style acquire and relinquish via destructor
    jau::sc_atomic_critical sync(sync_pairing);

    const SMPKeyDist key_mask_legacy = SMPKeyDist::ENC_KEY | SMPKeyDist::ID_KEY | SMPKeyDist::SIGN_KEY;
    const SMPKeyDist key_mask_sc     =                       SMPKeyDist::ID_KEY | SMPKeyDist::SIGN_KEY | SMPKeyDist::LINK_KEY;
    const SMPPairingState old_pstate = pairing_data.state;
    const PairingMode old_pmode = pairing_data.mode;
    const std::string timestamp = jau::uint64DecString(jau::environment::getElapsedMillisecond(), ',', 9);

    SMPPairingState pstate = old_pstate;
    PairingMode pmode = old_pmode;
    bool is_device_ready = false;

    if( jau::environment::get().debug ) {
        jau::PLAIN_PRINT(false, "[%s] Debug: DBTDevice:hci:SMP.0: address[%s, %s]",
            timestamp.c_str(),
            address.toString().c_str(), getBDAddressTypeString(addressType).c_str());
        jau::PLAIN_PRINT(false, "[%s] - %s", timestamp.c_str(), msg->toString().c_str());
        jau::PLAIN_PRINT(false, "[%s] - %s", timestamp.c_str(), source.toString().c_str());
        jau::PLAIN_PRINT(false, "[%s] - %s", timestamp.c_str(), toString(false).c_str());
    }

    const SMPPDUMsg::Opcode opc = msg->getOpcode();

    switch( opc ) {
        // Phase 1: SMP Negotiation phase

        case SMPPDUMsg::Opcode::SECURITY_REQUEST:
            pmode = PairingMode::NEGOTIATING;
            pstate = SMPPairingState::REQUESTED_BY_RESPONDER;
            pairing_data.res_requested_sec = true;
            break;

        case SMPPDUMsg::Opcode::PAIRING_REQUEST:
            if( HCIACLData::l2cap_frame::PBFlag::START_NON_AUTOFLUSH_HOST == source.pb_flag ) { // from initiator (master)
                const SMPPairingMsg & msg1 = *static_cast<const SMPPairingMsg *>( msg.get() );
                pairing_data.authReqs_init = msg1.getAuthReqMask();
                pairing_data.ioCap_init    = msg1.getIOCapability();
                pairing_data.oobFlag_init  = msg1.getOOBDataFlag();
                pairing_data.maxEncsz_init = msg1.getMaxEncryptionKeySize();
                pairing_data.keys_init_exp = msg1.getInitKeyDist();
                pmode = PairingMode::NEGOTIATING;
                pstate = SMPPairingState::FEATURE_EXCHANGE_STARTED;
            }
            break;

        case SMPPDUMsg::Opcode::PAIRING_RESPONSE: {
            if( HCIACLData::l2cap_frame::PBFlag::START_AUTOFLUSH == source.pb_flag ) { // from responder (slave)
                const SMPPairingMsg & msg1 = *static_cast<const SMPPairingMsg *>( msg.get() );
                pairing_data.authReqs_resp = msg1.getAuthReqMask();
                pairing_data.ioCap_resp    = msg1.getIOCapability();
                pairing_data.oobFlag_resp  = msg1.getOOBDataFlag();
                pairing_data.maxEncsz_resp = msg1.getMaxEncryptionKeySize();
                pairing_data.keys_init_exp = msg1.getInitKeyDist(); // responding device overrides initiator's request!
                pairing_data.keys_resp_exp = msg1.getRespKeyDist();

                const bool use_sc = isSMPAuthReqBitSet( pairing_data.authReqs_init, SMPAuthReqs::SECURE_CONNECTIONS ) &&
                                    isSMPAuthReqBitSet( pairing_data.authReqs_resp, SMPAuthReqs::SECURE_CONNECTIONS );
                pairing_data.use_sc = use_sc;

                pmode = ::getPairingMode(use_sc,
                                         pairing_data.authReqs_init, pairing_data.ioCap_init, pairing_data.oobFlag_init,
                                         pairing_data.authReqs_resp, pairing_data.ioCap_resp, pairing_data.oobFlag_resp);

                pstate = SMPPairingState::FEATURE_EXCHANGE_COMPLETED;

                if( jau::environment::get().debug ) {
                    jau::PLAIN_PRINT(false, "[%s] ", timestamp.c_str());
                    jau::PLAIN_PRINT(false, "[%s] Debug: DBTDevice:hci:SMP.2: address[%s, %s]: State %s, Mode %s, using SC %d:", timestamp.c_str() ,
                        address.toString().c_str(), getBDAddressTypeString(addressType).c_str(),
                        getSMPPairingStateString(pstate).c_str(), getPairingModeString(pmode).c_str(), use_sc);
                    jau::PLAIN_PRINT(false, "[%s] - oob:   init %s", timestamp.c_str(), getSMPOOBDataFlagString(pairing_data.oobFlag_init).c_str());
                    jau::PLAIN_PRINT(false, "[%s] - oob:   resp %s", timestamp.c_str(), getSMPOOBDataFlagString(pairing_data.oobFlag_resp).c_str());
                    jau::PLAIN_PRINT(false, "[%s] ", timestamp.c_str());
                    jau::PLAIN_PRINT(false, "[%s] - auth:  init %s", timestamp.c_str(), getSMPAuthReqMaskString(pairing_data.authReqs_init).c_str());
                    jau::PLAIN_PRINT(false, "[%s] - auth:  resp %s", timestamp.c_str(), getSMPAuthReqMaskString(pairing_data.authReqs_resp).c_str());
                    jau::PLAIN_PRINT(false, "[%s] ", timestamp.c_str());
                    jau::PLAIN_PRINT(false, "[%s] - iocap: init %s", timestamp.c_str(), getSMPIOCapabilityString(pairing_data.ioCap_init).c_str());
                    jau::PLAIN_PRINT(false, "[%s] - iocap: resp %s", timestamp.c_str(), getSMPIOCapabilityString(pairing_data.ioCap_resp).c_str());
                    jau::PLAIN_PRINT(false, "[%s] ", timestamp.c_str());
                    jau::PLAIN_PRINT(false, "[%s] - encsz: init %d", timestamp.c_str(), (int)pairing_data.maxEncsz_init);
                    jau::PLAIN_PRINT(false, "[%s] - encsz: resp %d", timestamp.c_str(), (int)pairing_data.maxEncsz_resp);
                    jau::PLAIN_PRINT(false, "[%s] ", timestamp.c_str());
                    jau::PLAIN_PRINT(false, "[%s] - keys:  init %s", timestamp.c_str(), getSMPKeyDistMaskString(pairing_data.keys_init_exp).c_str());
                    jau::PLAIN_PRINT(false, "[%s] - keys:  resp %s", timestamp.c_str(), getSMPKeyDistMaskString(pairing_data.keys_resp_exp).c_str());
                }
            }
        } break;

        // Phase 2: SMP Authentication and Encryption

        case SMPPDUMsg::Opcode::PAIRING_CONFIRM:
            [[fallthrough]];
        case SMPPDUMsg::Opcode::PAIRING_PUBLIC_KEY:
            [[fallthrough]];
        case SMPPDUMsg::Opcode::PAIRING_RANDOM:
            pmode = old_pmode;
            pstate = SMPPairingState::KEY_DISTRIBUTION;
            break;

        case SMPPDUMsg::Opcode::PAIRING_FAILED: {
            pmode = PairingMode::NONE;
            pstate = SMPPairingState::FAILED;
            pairing_data.res_requested_sec = false;

            // After a failed encryption/authentication, we try without security!
            const BTSecurityLevel sec_level = BTSecurityLevel::NONE;
            pairing_data.sec_level_conn = sec_level;
            const bool l2cap_close = l2cap_att.close();
            const bool l2cap_open = l2cap_att.open(*this, sec_level);
            is_device_ready = l2cap_open;

            if( jau::environment::get().debug ) {
                jau::PLAIN_PRINT(false, "[%s] Debug: DBTDevice:hci:SMP.1: l2cap ATT reopen: ready %d, sec_level %s, l2cap[close %d, open %d]",
                        timestamp.c_str(),
                        is_device_ready, getBTSecurityLevelString(sec_level).c_str(), l2cap_close, l2cap_open);
            }
          } break;

        // Phase 3: SMP Key & Value Distribution phase

        case SMPPDUMsg::Opcode::ENCRYPTION_INFORMATION: {     /* Legacy: 1 */
            // LTK: First part for SMPKeyDistFormat::ENC_KEY, followed by MASTER_IDENTIFICATION (EDIV + RAND)
            const SMPEncInfoMsg & msg1 = *static_cast<const SMPEncInfoMsg *>( msg.get() );
            if( HCIACLData::l2cap_frame::PBFlag::START_AUTOFLUSH == source.pb_flag ) {
                // from responder (LL slave)
                if( BTSecurityLevel::ENC_AUTH <= pairing_data.sec_level_conn ) {
                    pairing_data.ltk_resp.properties |= SMPLongTermKeyInfo::Property::AUTH;
                }
                if( pairing_data.use_sc ) {
                    pairing_data.ltk_resp.properties |= SMPLongTermKeyInfo::Property::SC;
                }
                pairing_data.ltk_resp.enc_size = pairing_data.maxEncsz_resp;
                pairing_data.ltk_resp.ltk = msg1.getLTK();
            } else {
                // from initiator (LL master)
                if( BTSecurityLevel::ENC_AUTH <= pairing_data.sec_level_conn ) {
                    pairing_data.ltk_init.properties |= SMPLongTermKeyInfo::Property::AUTH;
                }
                if( pairing_data.use_sc ) {
                    pairing_data.ltk_init.properties |= SMPLongTermKeyInfo::Property::SC;
                }
                pairing_data.ltk_init.enc_size = pairing_data.maxEncsz_init;
                pairing_data.ltk_init.ltk = msg1.getLTK();
            }
        }   break;

        case SMPPDUMsg::Opcode::MASTER_IDENTIFICATION: {      /* Legacy: 2 */
            // EDIV + RAND, completing SMPKeyDistFormat::ENC_KEY
            const SMPMasterIdentMsg & msg1 = *static_cast<const SMPMasterIdentMsg *>( msg.get() );
            if( HCIACLData::l2cap_frame::PBFlag::START_AUTOFLUSH == source.pb_flag ) {
                // from responder (LL slave)
                pairing_data.keys_resp_has |= SMPKeyDist::ENC_KEY;
                pairing_data.ltk_resp.ediv = msg1.getEDIV();
                pairing_data.ltk_resp.rand = msg1.getRand();
            } else {
                // from initiator (LL master)
                pairing_data.keys_init_has |= SMPKeyDist::ENC_KEY;
                pairing_data.ltk_init.ediv = msg1.getEDIV();
                pairing_data.ltk_init.rand = msg1.getRand();
            }
        }   break;

        case SMPPDUMsg::Opcode::IDENTITY_INFORMATION: {       /* Legacy: 3; SC: 1 */
            // IRK: First part of SMPKeyDist::ID_KEY, followed by IDENTITY_ADDRESS_INFORMATION
            const SMPIdentInfoMsg & msg1 = *static_cast<const SMPIdentInfoMsg *>( msg.get() );
            if( HCIACLData::l2cap_frame::PBFlag::START_AUTOFLUSH == source.pb_flag ) {
                // from responder (LL slave)
                pairing_data.irk_resp = msg1.getIRK();
            } else {
                // from initiator (LL master)
                pairing_data.irk_init = msg1.getIRK();
            }
        }   break;

        case SMPPDUMsg::Opcode::IDENTITY_ADDRESS_INFORMATION:{/* Lecacy: 4; SC: 2 */
            // Public device or static random, completing SMPKeyDist::ID_KEY
            const SMPIdentAddrInfoMsg & msg1 = *static_cast<const SMPIdentAddrInfoMsg *>( msg.get() );
            if( HCIACLData::l2cap_frame::PBFlag::START_AUTOFLUSH == source.pb_flag ) {
                // from responder (LL slave)
                pairing_data.keys_resp_has |= SMPKeyDist::ID_KEY;
                pairing_data.address = msg1.getAddress();
                pairing_data.is_static_random_address = msg1.isStaticRandomAddress();
            } else {
                // from initiator (LL master)
                pairing_data.keys_init_has |= SMPKeyDist::ID_KEY;
                pairing_data.address = msg1.getAddress();
                pairing_data.is_static_random_address = msg1.isStaticRandomAddress();
            }
        }   break;

        case SMPPDUMsg::Opcode::SIGNING_INFORMATION: {        /* Legacy: 5; SC: 3; Last value. */
            // CSRK
            const SMPSignInfoMsg & msg1 = *static_cast<const SMPSignInfoMsg *>( msg.get() );
            if( HCIACLData::l2cap_frame::PBFlag::START_AUTOFLUSH == source.pb_flag ) {
                // from responder (LL slave)
                pairing_data.keys_resp_has |= SMPKeyDist::SIGN_KEY;
                pairing_data.csrk_resp = msg1.getCSRK();
            } else {
                // from initiator (LL master)
                pairing_data.keys_init_has |= SMPKeyDist::SIGN_KEY;
                pairing_data.csrk_init = msg1.getCSRK();
            }
        }   break;

        default:
            break;
    }

    if( SMPPairingState::KEY_DISTRIBUTION == old_pstate ) {
        // Spec allows responder to not distribute the keys,
        // hence distribution is complete with initiator (LL master) keys!
        // Impact of missing responder keys: Requires new pairing each connection.
        if( pairing_data.use_sc ) {
            if( pairing_data.keys_init_has == ( pairing_data.keys_init_exp & key_mask_sc ) ) {
                // pairing_data.keys_resp_has == ( pairing_data.keys_resp_exp & key_mask_sc )
                pstate = SMPPairingState::COMPLETED;
                is_device_ready = true;
            }
        } else {
            if( pairing_data.keys_init_has == ( pairing_data.keys_init_exp & key_mask_legacy ) ) {
                // pairing_data.keys_resp_has == ( pairing_data.keys_resp_exp & key_mask_legacy )
                pstate = SMPPairingState::COMPLETED;
                is_device_ready = true;
            }
        }
    }

    if( jau::environment::get().debug ) {
        if( old_pstate == pstate /* && old_pmode == pmode */ ) {
            jau::PLAIN_PRINT(false, "[%s] Debug: DBTDevice:hci:SMP.4: Unchanged: address[%s, %s]",
                timestamp.c_str(),
                address.toString().c_str(), getBDAddressTypeString(addressType).c_str());
        } else {
            jau::PLAIN_PRINT(false, "[%s] Debug: DBTDevice:hci:SMP.5:   Updated: address[%s, %s]",
                timestamp.c_str(),
                address.toString().c_str(), getBDAddressTypeString(addressType).c_str());
        }
        jau::PLAIN_PRINT(false, "[%s] - state %s -> %s, mode %s -> %s, ready %d",
            timestamp.c_str(),
            getSMPPairingStateString(old_pstate).c_str(), getSMPPairingStateString(pstate).c_str(),
            getPairingModeString(old_pmode).c_str(), getPairingModeString(pmode).c_str(),
            is_device_ready);
        jau::PLAIN_PRINT(false, "[%s] - keys[init %s / %s, resp %s / %s]",
            timestamp.c_str(),
            getSMPKeyDistMaskString(pairing_data.keys_init_has).c_str(),
            getSMPKeyDistMaskString(pairing_data.keys_init_exp).c_str(),
            getSMPKeyDistMaskString(pairing_data.keys_resp_has).c_str(),
            getSMPKeyDistMaskString(pairing_data.keys_resp_exp).c_str());
    }

    if( old_pstate == pstate /* && old_pmode == pmode */ ) {
        return;
    }

    pairing_data.mode = pmode;
    pairing_data.state = pstate;

    adapter.sendDevicePairingState(sthis, pstate, pmode, msg->ts_creation);

    if( is_device_ready ) {
        std::thread dc(&DBTDevice::processDeviceReady, this, sthis, msg->ts_creation); // @suppress("Invalid arguments")
        dc.detach();
    }
    if( jau::environment::get().debug ) {
        jau::PLAIN_PRINT(false, "[%s] Debug: DBTDevice:hci:SMP.6: End", timestamp.c_str());
        jau::PLAIN_PRINT(false, "[%s] - %s", timestamp.c_str(), toString(false).c_str());
    }
}

SMPLongTermKeyInfo DBTDevice::getLongTermKeyInfo(const bool responder) const noexcept {
    jau::sc_atomic_critical sync(const_cast<DBTDevice*>(this)->sync_pairing);
    return responder ? pairing_data.ltk_resp : pairing_data.ltk_init;
}

HCIStatusCode DBTDevice::setLongTermKeyInfo(const SMPLongTermKeyInfo& ltk, const bool responder) noexcept {
    jau::sc_atomic_critical sync(sync_pairing);
    DBTManager & mngr = adapter.getManager();
    HCIStatusCode res = mngr.uploadLongTermKeyInfo(adapter.dev_id, address, addressType, ltk, responder);
    return res;
}

HCIStatusCode DBTDevice::pair(const SMPIOCapability io_cap) noexcept {
    /**
     * Experimental only.
     * <pre>
     *   adapter.stopDiscovery(): Renders pairDevice(..) to fail: Busy!
     *   pairDevice(..) behaves quite instable within our connected workflow: Not used!
     * </pre>
     */
    if( SMPIOCapability::UNSET == io_cap ) {
        DBG_PRINT("DBTDevice::pairDevice: io %s, invalid value.", getSMPIOCapabilityString(io_cap).c_str());
        return HCIStatusCode::INVALID_PARAMS;
    }
    DBTManager& mngr = adapter.getManager();

    DBG_PRINT("DBTDevice::pairDevice: Start: io %s, %s", getSMPIOCapabilityString(io_cap).c_str(), toString(false).c_str());
    mngr.uploadConnParam(adapter.dev_id, address, addressType);

    jau::sc_atomic_critical sync(sync_pairing);
    pairing_data.ioCap_conn = io_cap;
    const bool res = mngr.pairDevice(adapter.dev_id, address, addressType, io_cap);
    if( !res ) {
        pairing_data.ioCap_conn = SMPIOCapability::UNSET;
    }
    DBG_PRINT("DBTDevice::pairDevice: End: io %s, %s", getSMPIOCapabilityString(io_cap).c_str(), toString(false).c_str());
    return res ? HCIStatusCode::SUCCESS : HCIStatusCode::FAILED;
}

bool DBTDevice::setConnSecurityLevel(const BTSecurityLevel sec_level) noexcept {
    if( BTSecurityLevel::UNSET == sec_level ) {
        DBG_PRINT("DBTAdapter::setConnSecurityLevel: lvl %s, invalid value.", getBTSecurityLevelString(sec_level).c_str());
        return false;
    }

    if( !isValid() || isConnected || allowDisconnect ) {
        DBG_PRINT("DBTDevice::setConnSecurityLevel: lvl %s failed, invalid state %s",
            getBTSecurityLevelString(sec_level).c_str(), toString(false).c_str());
        return false;
    }
    jau::sc_atomic_critical sync(sync_pairing);
    const bool res = true;
    pairing_data.sec_level_user = sec_level;

    DBG_PRINT("DBTDevice::setConnSecurityLevel: result %d: lvl %s, %s", res,
        getBTSecurityLevelString(sec_level).c_str(),
        toString(false).c_str());
    return res;
}

BTSecurityLevel DBTDevice::getConnSecurityLevel() const noexcept {
    jau::sc_atomic_critical sync(const_cast<DBTDevice*>(this)->sync_pairing);
    return pairing_data.sec_level_conn;
}

bool DBTDevice::setConnIOCapability(const SMPIOCapability io_cap) noexcept {
    if( SMPIOCapability::UNSET == io_cap ) {
        DBG_PRINT("DBTDevice::setConnIOCapability: io %s, invalid value.", getSMPIOCapabilityString(io_cap).c_str());
        return false;
    }

    if( !isValid() || isConnected || allowDisconnect ) {
        DBG_PRINT("DBTDevice::setConnIOCapability: io %s failed, invalid state %s",
                getSMPIOCapabilityString(io_cap).c_str(), toString(false).c_str());
        return false;
    }
    jau::sc_atomic_critical sync(sync_pairing);
    const bool res = true;
    pairing_data.ioCap_user = io_cap;

    DBG_PRINT("DBTDevice::setConnIOCapability: result %d: io %s, %s", res,
        getSMPIOCapabilityString(io_cap).c_str(),
        toString(false).c_str());

    return res;
}

SMPIOCapability DBTDevice::getConnIOCapability() const noexcept {
    jau::sc_atomic_critical sync(const_cast<DBTDevice*>(this)->sync_pairing);
    return pairing_data.ioCap_conn;
}

bool DBTDevice::setConnSecurity(const BTSecurityLevel sec_level, const SMPIOCapability io_cap) noexcept {
    if( BTSecurityLevel::UNSET == sec_level ) {
        DBG_PRINT("DBTAdapter::setConnSecurity: lvl %s, invalid value.", getBTSecurityLevelString(sec_level).c_str());
        return false;
    }
    if( SMPIOCapability::UNSET == io_cap ) {
        DBG_PRINT("DBTDevice::setConnSecurity: io %s, invalid value.", getSMPIOCapabilityString(io_cap).c_str());
        return false;
    }

    if( !isValid() || isConnected || allowDisconnect ) {
        DBG_PRINT("DBTDevice::setConnSecurity: lvl %s, io %s failed, invalid state %s",
                getBTSecurityLevelString(sec_level).c_str(),
                getSMPIOCapabilityString(io_cap).c_str(), toString(false).c_str());
        return false;
    }
    jau::sc_atomic_critical sync(sync_pairing);
    const bool res = true;
    pairing_data.ioCap_user = io_cap;
    pairing_data.sec_level_user = sec_level;

    DBG_PRINT("DBTDevice::setConnSecurity: result %d: lvl %s, io %s, %s", res,
        getBTSecurityLevelString(sec_level).c_str(),
        getSMPIOCapabilityString(io_cap).c_str(),
        toString(false).c_str());

    return res;
}

bool DBTDevice::setConnSecurityBest(const BTSecurityLevel sec_level, const SMPIOCapability io_cap) noexcept {
    if( BTSecurityLevel::UNSET < sec_level && SMPIOCapability::UNSET != io_cap ) {
        return setConnSecurity(sec_level, io_cap);
    } else if( BTSecurityLevel::UNSET < sec_level ) {
        if( BTSecurityLevel::ENC_ONLY >= sec_level ) {
            return setConnSecurity(sec_level, SMPIOCapability::NO_INPUT_NO_OUTPUT);
        } else {
            return setConnSecurityLevel(sec_level);
        }
    } else if( SMPIOCapability::UNSET != io_cap ) {
        return setConnIOCapability(io_cap);
    } else {
        return false;
    }
}

HCIStatusCode DBTDevice::setPairingPasskey(const uint32_t passkey) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_pairing); // RAII-style acquire and relinquish via destructor
    jau::sc_atomic_critical sync(sync_pairing);

    if( SMPPairingState::PASSKEY_EXPECTED == pairing_data.state ) {
        DBTManager& mngr = adapter.getManager();
        MgmtStatus res = mngr.userPasskeyReply(adapter.dev_id, address, addressType, passkey);
        DBG_PRINT("DBTDevice:mgmt:SMP: PASSKEY '%d', state %s, result %s",
            passkey, getSMPPairingStateString(pairing_data.state).c_str(), getMgmtStatusString(res).c_str());
        return HCIStatusCode::SUCCESS;
    } else {
        DBG_PRINT("DBTDevice:mgmt:SMP: PASSKEY '%d', state %s, SKIPPED (wrong state)",
            passkey, getSMPPairingStateString(pairing_data.state).c_str());
        return HCIStatusCode::UNKNOWN;
    }
}

HCIStatusCode DBTDevice::setPairingPasskeyNegative() noexcept {
    const std::lock_guard<std::mutex> lock(mtx_pairing); // RAII-style acquire and relinquish via destructor
    jau::sc_atomic_critical sync(sync_pairing);

    if( SMPPairingState::PASSKEY_EXPECTED == pairing_data.state ) {
        DBTManager& mngr = adapter.getManager();
        MgmtStatus res = mngr.userPasskeyNegativeReply(adapter.dev_id, address, addressType);
        DBG_PRINT("DBTDevice:mgmt:SMP: PASSKEY NEGATIVE, state %s, result %s",
            getSMPPairingStateString(pairing_data.state).c_str(), getMgmtStatusString(res).c_str());
        return HCIStatusCode::SUCCESS;
    } else {
        DBG_PRINT("DBTDevice:mgmt:SMP: PASSKEY NEGATIVE, state %s, SKIPPED (wrong state)",
            getSMPPairingStateString(pairing_data.state).c_str());
        return HCIStatusCode::UNKNOWN;
    }
}

HCIStatusCode DBTDevice::setPairingNumericComparison(const bool positive) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_pairing); // RAII-style acquire and relinquish via destructor
    jau::sc_atomic_critical sync(sync_pairing);

    if( SMPPairingState::NUMERIC_COMPARE_EXPECTED == pairing_data.state ) {
        DBTManager& mngr = adapter.getManager();
        MgmtStatus res = mngr.userConfirmReply(adapter.dev_id, address, addressType, positive);
        DBG_PRINT("DBTDevice:mgmt:SMP: CONFIRM '%d', state %s, result %s",
            positive, getSMPPairingStateString(pairing_data.state).c_str(), getMgmtStatusString(res).c_str());
        return HCIStatusCode::SUCCESS;
    } else {
        DBG_PRINT("DBTDevice:mgmt:SMP: CONFIRM '%d', state %s, SKIPPED (wrong state)",
            positive, getSMPPairingStateString(pairing_data.state).c_str());
        return HCIStatusCode::UNKNOWN;
    }
}

PairingMode DBTDevice::getPairingMode() const noexcept {
    jau::sc_atomic_critical sync(const_cast<DBTDevice*>(this)->sync_pairing);
    return pairing_data.mode;
}

SMPPairingState DBTDevice::getPairingState() const noexcept {
    jau::sc_atomic_critical sync(const_cast<DBTDevice*>(this)->sync_pairing);
    return pairing_data.state;
}

void DBTDevice::clearSMPStates(const bool connected) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_pairing); // RAII-style acquire and relinquish via destructor
    jau::sc_atomic_critical sync(sync_pairing);

    if( !connected ) {
        // needs to survive connected, or will be set right @ connected
        pairing_data.ioCap_user = SMPIOCapability::UNSET;
        pairing_data.ioCap_conn = SMPIOCapability::UNSET;
        pairing_data.sec_level_user = BTSecurityLevel::UNSET;
    }
    pairing_data.sec_level_conn = BTSecurityLevel::UNSET;

    pairing_data.state = SMPPairingState::NONE;
    pairing_data.mode = PairingMode::NONE;
    pairing_data.res_requested_sec = false;
    pairing_data.use_sc = false;

    pairing_data.authReqs_resp = SMPAuthReqs::NONE;
    pairing_data.ioCap_resp    = SMPIOCapability::NO_INPUT_NO_OUTPUT;
    pairing_data.oobFlag_resp  = SMPOOBDataFlag::OOB_AUTH_DATA_NOT_PRESENT;
    pairing_data.maxEncsz_resp = 0;
    pairing_data.keys_resp_exp = SMPKeyDist::NONE;
    pairing_data.keys_resp_has = SMPKeyDist::NONE;

    pairing_data.authReqs_init = SMPAuthReqs::NONE;
    pairing_data.ioCap_init    = SMPIOCapability::NO_INPUT_NO_OUTPUT;
    pairing_data.oobFlag_init  = SMPOOBDataFlag::OOB_AUTH_DATA_NOT_PRESENT;
    pairing_data.maxEncsz_init = 0;
    pairing_data.keys_init_exp = SMPKeyDist::NONE;
    pairing_data.keys_init_has = SMPKeyDist::NONE;
}

void DBTDevice::disconnectSMP(const int caller) noexcept {
  #if SMP_SUPPORTED_BY_OS
    const std::lock_guard<std::recursive_mutex> lock_conn(mtx_smpHandler);
    if( nullptr != smpHandler ) {
        DBG_PRINT("DBTDevice::disconnectSMP: start (has smpHandler, caller %d)", caller);
        smpHandler->disconnect(false /* disconnectDevice */, false /* ioErrorCause */);
    } else {
        DBG_PRINT("DBTDevice::disconnectSMP: start (nil smpHandler, caller %d)", caller);
    }
    smpHandler = nullptr;
    DBG_PRINT("DBTDevice::disconnectSMP: end");
  #else
    (void)caller;
  #endif
}

bool DBTDevice::connectSMP(std::shared_ptr<DBTDevice> sthis, const BTSecurityLevel sec_level) noexcept {
  #if SMP_SUPPORTED_BY_OS
    if( !isConnected || !allowDisconnect) {
        ERR_PRINT("DBTDevice::connectSMP(%u): Device not connected: %s", sec_level, toString(false).c_str());
        return false;
    }

    if( !SMPHandler::IS_SUPPORTED_BY_OS ) {
        DBG_PRINT("DBTDevice::connectSMP(%u): SMP Not supported by OS (1): %s", sec_level, toString(false).c_str());
        return false;
    }

    if( BTSecurityLevel::NONE >= sec_level ) {
        return false;
    }

    const std::lock_guard<std::recursive_mutex> lock_conn(mtx_gattHandler);
    if( nullptr != smpHandler ) {
        if( smpHandler->isConnected() ) {
            return smpHandler->establishSecurity(sec_level);
        }
        smpHandler = nullptr;
    }

    smpHandler = std::shared_ptr<SMPHandler>(new SMPHandler(sthis));
    if( !smpHandler->isConnected() ) {
        ERR_PRINT("DBTDevice::connectSMP: Connection failed");
        smpHandler = nullptr;
        return false;
    }
    return smpHandler->establishSecurity(sec_level);
  #else
    DBG_PRINT("DBTDevice::connectSMP: SMP Not supported by OS (0): %s", toString(false).c_str());
    (void)sthis;
    (void)sec_level;
    return false;
  #endif
}

void DBTDevice::disconnectGATT(const int caller) noexcept {
    const std::lock_guard<std::recursive_mutex> lock_conn(mtx_gattHandler);
    if( nullptr != gattHandler ) {
        DBG_PRINT("DBTDevice::disconnectGATT: start (has gattHandler, caller %d)", caller);
        gattHandler->disconnect(false /* disconnectDevice */, false /* ioErrorCause */);
    } else {
        DBG_PRINT("DBTDevice::disconnectGATT: start (nil gattHandler, caller %d)", caller);
    }
    gattHandler = nullptr;
    DBG_PRINT("DBTDevice::disconnectGATT: end");
}

bool DBTDevice::connectGATT(std::shared_ptr<DBTDevice> sthis) noexcept {
    if( !isConnected || !allowDisconnect) {
        ERR_PRINT("DBTDevice::connectGATT: Device not connected: %s", toString(false).c_str());
        return false;
    }
    if( !l2cap_att.isOpen() ) {
        ERR_PRINT("DBTDevice::connectGATT: L2CAP not open: %s", toString(false).c_str());
        return false;
    }

    const std::lock_guard<std::recursive_mutex> lock_conn(mtx_gattHandler);
    if( nullptr != gattHandler ) {
        if( gattHandler->isConnected() ) {
            return true;
        }
        gattHandler = nullptr;
    }

    gattHandler = std::shared_ptr<GATTHandler>(new GATTHandler(sthis, l2cap_att));
    if( !gattHandler->isConnected() ) {
        ERR_PRINT2("DBTDevice::connectGATT: Connection failed");
        gattHandler = nullptr;
        return false;
    }
    return true;
}

std::shared_ptr<GATTHandler> DBTDevice::getGATTHandler() noexcept {
    const std::lock_guard<std::recursive_mutex> lock_conn(mtx_gattHandler);
    return gattHandler;
}

std::vector<std::shared_ptr<GATTService>> DBTDevice::getGATTServices() noexcept {
    std::shared_ptr<GATTHandler> gh = getGATTHandler();
    if( nullptr == gh ) {
        ERR_PRINT("DBTDevice::getGATTServices: GATTHandler nullptr");
        return std::vector<std::shared_ptr<GATTService>>();
    }
    std::vector<std::shared_ptr<GATTService>> & gattServices = gh->getServices(); // reference of the GATTHandler's list
    if( gattServices.size() > 0 ) { // reuse previous discovery result
        return gattServices;
    }
    try {
        gattServices = gh->discoverCompletePrimaryServices(gh); // same reference of the GATTHandler's list
        if( gattServices.size() == 0 ) { // nothing discovered
            return gattServices;
        }

        // discovery success, parse GenericAccess
        std::shared_ptr<GattGenericAccessSvc> gattGenericAccess = gh->getGenericAccess();
        if( nullptr != gattGenericAccess ) {
            const uint64_t ts = jau::getCurrentMilliseconds();
            EIRDataType updateMask = update(*gattGenericAccess, ts);
            DBG_PRINT("DBTDevice::getGATTServices: updated %s:\n    %s\n    -> %s",
                getEIRDataMaskString(updateMask).c_str(), gattGenericAccess->toString().c_str(), toString(false).c_str());
            if( EIRDataType::NONE != updateMask ) {
                std::shared_ptr<DBTDevice> sharedInstance = getSharedInstance();
                if( nullptr == sharedInstance ) {
                    ERR_PRINT("DBTDevice::getGATTServices: Device unknown to adapter and not tracked: %s", toString(false).c_str());
                } else {
                    adapter.sendDeviceUpdated("getGATTServices", sharedInstance, ts, updateMask);
                }
            }
        }
    } catch (std::exception &e) {
        WARN_PRINT("DBTDevice::getGATTServices: Caught exception: '%s' on %s", e.what(), toString(false).c_str());
    }
    return gattServices;
}

std::shared_ptr<GATTService> DBTDevice::findGATTService(std::shared_ptr<uuid_t> const &uuid) {
    const std::vector<std::shared_ptr<GATTService>> & gattServices = getGATTServices(); // reference of the GATTHandler's list
    const size_t size = gattServices.size();
    for (size_t i = 0; i < size; i++) {
        const std::shared_ptr<GATTService> & e = gattServices[i];
        if ( nullptr != e && *uuid == *(e->type) ) {
            return e;
        }
    }
    return nullptr;
}

bool DBTDevice::pingGATT() noexcept {
    std::shared_ptr<GATTHandler> gh = getGATTHandler();
    if( nullptr == gh || !gh->isConnected() ) {
        jau::INFO_PRINT("DBTDevice::pingGATT: GATTHandler not connected -> disconnected on %s", toString(false).c_str());
        disconnect(HCIStatusCode::REMOTE_USER_TERMINATED_CONNECTION);
        return false;
    }
    try {
        return gh->ping();
    } catch (std::exception &e) {
        IRQ_PRINT("DBTDevice::pingGATT: Potential disconnect, exception: '%s' on %s", e.what(), toString(false).c_str());
    }
    return false;
}

std::shared_ptr<GattGenericAccessSvc> DBTDevice::getGATTGenericAccess() {
    std::shared_ptr<GATTHandler> gh = getGATTHandler();
    if( nullptr == gh ) {
        ERR_PRINT("DBTDevice::getGATTGenericAccess: GATTHandler nullptr");
        return nullptr;
    }
    return gh->getGenericAccess();
}

bool DBTDevice::addCharacteristicListener(std::shared_ptr<GATTCharacteristicListener> l) {
    std::shared_ptr<GATTHandler> gatt = getGATTHandler();
    if( nullptr == gatt ) {
        throw jau::IllegalStateException("Device's GATTHandle not connected: "+
                toString(false), E_FILE_LINE);
    }
    return gatt->addCharacteristicListener(l);
}

bool DBTDevice::removeCharacteristicListener(std::shared_ptr<GATTCharacteristicListener> l) noexcept {
    std::shared_ptr<GATTHandler> gatt = getGATTHandler();
    if( nullptr == gatt ) {
        // OK to have GATTHandler being shutdown @ disable
        DBG_PRINT("Device's GATTHandle not connected: %s", toString(false).c_str());
        return false;
    }
    return gatt->removeCharacteristicListener(l);
}

int DBTDevice::removeAllAssociatedCharacteristicListener(std::shared_ptr<GATTCharacteristic> associatedCharacteristic) noexcept {
    std::shared_ptr<GATTHandler> gatt = getGATTHandler();
    if( nullptr == gatt ) {
        // OK to have GATTHandler being shutdown @ disable
        DBG_PRINT("Device's GATTHandle not connected: %s", toString(false).c_str());
        return false;
    }
    return gatt->removeAllAssociatedCharacteristicListener( associatedCharacteristic );
}

int DBTDevice::removeAllCharacteristicListener() noexcept {
    std::shared_ptr<GATTHandler> gatt = getGATTHandler();
    if( nullptr == gatt ) {
        // OK to have GATTHandler being shutdown @ disable
        DBG_PRINT("Device's GATTHandle not connected: %s", toString(false).c_str());
        return 0;
    }
    return gatt->removeAllCharacteristicListener();
}

void DBTDevice::notifyDisconnected() noexcept {
    // coming from disconnect callback, ensure cleaning up!
    DBG_PRINT("DBTDevice::notifyDisconnected: handle %s -> zero, %s",
              jau::uint16HexString(hciConnHandle).c_str(), toString(false).c_str());
    clearSMPStates(false /* connected */);
    allowDisconnect = false;
    isConnected = false;
    hciConnHandle = 0;
    disconnectGATT(1);
    disconnectSMP(1);
    l2cap_att.close();
}

HCIStatusCode DBTDevice::disconnect(const HCIStatusCode reason) noexcept {
    // Avoid disconnect re-entry lock-free
    bool expConn = true; // C++11, exp as value since C++20
    if( !allowDisconnect.compare_exchange_strong(expConn, false) ) {
        // Not connected or disconnect already in process.
        DBG_PRINT("DBTDevice::disconnect: Not connected: isConnected %d/%d, reason 0x%X (%s), gattHandler %d, hciConnHandle %s",
                allowDisconnect.load(), isConnected.load(),
                static_cast<uint8_t>(reason), getHCIStatusCodeString(reason).c_str(),
                (nullptr != gattHandler), jau::uint16HexString(hciConnHandle).c_str());
        return HCIStatusCode::CONNECTION_TERMINATED_BY_LOCAL_HOST;
    }
    if( !isConnected ) { // should not happen
        WARN_PRINT("DBTDevice::disconnect: allowConnect true -> false, but !isConnected on %s", toString(false).c_str());
        return HCIStatusCode::SUCCESS;
    }

    // Disconnect GATT and SMP before device, keeping reversed initialization order intact if possible.
    // This outside mtx_connect, keeping same mutex lock order intact as well
    disconnectGATT(0);
    disconnectSMP(0);

    // Lock to avoid other threads connecting while disconnecting
    const std::lock_guard<std::recursive_mutex> lock_conn(mtx_connect); // RAII-style acquire and relinquish via destructor

    WORDY_PRINT("DBTDevice::disconnect: Start: isConnected %d/%d, reason 0x%X (%s), gattHandler %d, hciConnHandle %s",
            allowDisconnect.load(), isConnected.load(),
            static_cast<uint8_t>(reason), getHCIStatusCodeString(reason).c_str(),
            (nullptr != gattHandler), jau::uint16HexString(hciConnHandle).c_str());

    HCIHandler &hci = adapter.getHCI();
    HCIStatusCode res = HCIStatusCode::SUCCESS;

    if( 0 == hciConnHandle ) {
        res = HCIStatusCode::UNSPECIFIED_ERROR;
        goto exit;
    }

    if( !adapter.isPowered() ) {
        WARN_PRINT("DBTDevice::disconnect: Adapter not powered: %s", toString(false).c_str());
        res = HCIStatusCode::UNSPECIFIED_ERROR; // powered-off
        goto exit;
    }

    res = hci.disconnect(hciConnHandle.load(), address, addressType, reason);
    if( HCIStatusCode::SUCCESS != res ) {
        ERR_PRINT("DBTDevice::disconnect: status %s, handle 0x%X, isConnected %d/%d: errno %d %s on %s",
                getHCIStatusCodeString(res).c_str(), hciConnHandle.load(),
                allowDisconnect.load(), isConnected.load(),
                errno, strerror(errno),
                toString(false).c_str());
    }

exit:
    if( HCIStatusCode::SUCCESS != res ) {
        // In case of an already pulled or disconnected HCIHandler (e.g. power-off)
        // or in case the hci->disconnect() itself fails,
        // send the DISCONN_COMPLETE event directly.
        // SEND_EVENT: Perform off-thread to avoid potential deadlock w/ application callbacks (similar when sent from HCIHandler's reader-thread)
        std::thread bg(&DBTAdapter::mgmtEvDeviceDisconnectedHCI, &adapter, std::shared_ptr<MgmtEvent>( // @suppress("Invalid arguments")
                 new MgmtEvtDeviceDisconnected(adapter.dev_id, address, addressType, reason, hciConnHandle.load()) ) );
        bg.detach();
        // adapter.mgmtEvDeviceDisconnectedHCI( std::shared_ptr<MgmtEvent>( new MgmtEvtDeviceDisconnected(adapter.dev_id, address, addressType, reason, hciConnHandle.load()) ) );
    }
    WORDY_PRINT("DBTDevice::disconnect: End: status %s, handle 0x%X, isConnected %d/%d on %s",
            getHCIStatusCodeString(res).c_str(),
            hciConnHandle.load(), allowDisconnect.load(), isConnected.load(),
            toString(false).c_str());

    return res;
}

HCIStatusCode DBTDevice::unpair() noexcept {
#if USE_LINUX_BT_SECURITY
    const MgmtStatus res = adapter.getManager().unpairDevice(adapter.dev_id, address, addressType, false /* disconnect */);
    clearSMPStates(false /* connected */);
    return getHCIStatusCode(res);
#elif SMP_SUPPORTED_BY_OS
    return HCIStatusCode::NOT_SUPPORTED;
#else
    return HCIStatusCode::NOT_SUPPORTED;
#endif
}

void DBTDevice::remove() noexcept {
    clearSMPStates(false /* connected */);
    adapter.removeDevice(*this);
}
