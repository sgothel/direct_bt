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
#include <cstdio>

#include  <algorithm>

// #define VERBOSE_ON 1
#include <jau/environment.hpp>
#include <jau/debug.hpp>

#include "HCIComm.hpp"

#include "BTAdapter.hpp"
#include "BTDevice.hpp"
#include "BTManager.hpp"


using namespace direct_bt;

BTDevice::BTDevice(const ctor_cookie& cc, BTAdapter & a, EInfoReport const & r)
: adapter(a),
  l2cap_att(adapter.getAddressAndType(), L2CAP_PSM::UNDEFINED, L2CAP_CID::ATT),
  ts_creation(r.getTimestamp()),
  addressAndType{r.getAddress(), r.getAddressType()}
{
    (void)cc;
    ts_last_discovery = ts_creation;
    hciConnHandle = 0;
    le_features = LEFeatures::NONE;
    isConnected = false;
    allowDisconnect = false;
    clearSMPStates(false /* connected */);
    if( !r.isSet(EIRDataType::BDADDR) ) {
        throw jau::IllegalArgumentException("Address not set: "+r.toString(), E_FILE_LINE);
    }
    if( !r.isSet(EIRDataType::BDADDR_TYPE) ) {
        throw jau::IllegalArgumentException("AddressType not set: "+r.toString(), E_FILE_LINE);
    }
    update(r);

    const BLERandomAddressType leRandomAddressType = addressAndType.getBLERandomAddressType();
    if( BDAddressType::BDADDR_LE_RANDOM == addressAndType.type ) {
        if( BLERandomAddressType::UNDEFINED == leRandomAddressType ) {
            throw jau::IllegalArgumentException("BDADDR_LE_RANDOM: Invalid BLERandomAddressType "+
                    to_string(leRandomAddressType)+": "+toString(), E_FILE_LINE);
        }
    } else {
        if( BLERandomAddressType::UNDEFINED != leRandomAddressType ) {
            throw jau::IllegalArgumentException("Not BDADDR_LE_RANDOM: Invalid given native BLERandomAddressType "+
                    to_string(leRandomAddressType)+": "+toString(), E_FILE_LINE);
        }
    }
}

BTDevice::~BTDevice() noexcept {
    DBG_PRINT("BTDevice::dtor: ... %p %s", this, addressAndType.toString().c_str());
    advServices.clear();
    advMSD = nullptr;
    adapter.removeAllStatusListener(*this);
    DBG_PRINT("BTDevice::dtor: XXX %p %s", this, addressAndType.toString().c_str());
}

std::shared_ptr<BTDevice> BTDevice::getSharedInstance() const noexcept {
    return adapter.getSharedDevice(*this);
}

bool BTDevice::addAdvService(std::shared_ptr<uuid_t> const &uuid) noexcept
{
    if( 0 > findAdvService(uuid) ) {
        advServices.push_back(uuid);
        return true;
    }
    return false;
}
bool BTDevice::addAdvServices(jau::darray<std::shared_ptr<uuid_t>> const & services) noexcept
{
    bool res = false;
    for(size_t j=0; j<services.size(); j++) {
        const std::shared_ptr<uuid_t> uuid = services.at(j);
        res = addAdvService(uuid) || res;
    }
    return res;
}

int BTDevice::findAdvService(std::shared_ptr<uuid_t> const &uuid) const noexcept
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

std::string const BTDevice::getName() const noexcept {
    jau::sc_atomic_critical sync(sync_data);
    std::string res = name;
    return res;
}

std::shared_ptr<ManufactureSpecificData> const BTDevice::getManufactureSpecificData() const noexcept {
    jau::sc_atomic_critical sync(sync_data);
    std::shared_ptr<ManufactureSpecificData> res = advMSD;
    return res;
}

jau::darray<std::shared_ptr<uuid_t>> BTDevice::getAdvertisedServices() const noexcept {
    jau::sc_atomic_critical sync(sync_data);
    jau::darray<std::shared_ptr<uuid_t>> res = advServices;
    return res;
}

std::string BTDevice::toString(bool includeDiscoveredServices) const noexcept {
    const uint64_t t0 = jau::getCurrentMilliseconds();
    jau::sc_atomic_critical sync(sync_data);
    std::string msdstr = nullptr != advMSD ? advMSD->toString() : "MSD[null]";
    std::string out("Device["+addressAndType.toString()+", name['"+name+
            "'], age[total "+std::to_string(t0-ts_creation)+", ldisc "+std::to_string(t0-ts_last_discovery)+", lup "+std::to_string(t0-ts_last_update)+
            "]ms, connected["+std::to_string(allowDisconnect)+"/"+std::to_string(isConnected)+", handle "+jau::to_hexstring(hciConnHandle)+
            ", sec[lvl "+to_string(pairing_data.sec_level_conn)+", io "+to_string(pairing_data.ioCap_conn)+
            ", auto "+to_string(pairing_data.ioCap_auto)+", pairing "+to_string(pairing_data.mode)+", state "+to_string(pairing_data.state)+"]], rssi "+std::to_string(getRSSI())+
            ", tx-power "+std::to_string(tx_power)+
            ", appearance "+jau::to_hexstring(static_cast<uint16_t>(appearance))+" ("+to_string(appearance)+
            "), "+msdstr+", "+javaObjectToString()+"]");
    if( includeDiscoveredServices ) {
        jau::darray<std::shared_ptr<uuid_t>> _advServices = getAdvertisedServices();
        if( _advServices.size() > 0 ) {
            out.append("\n");
            const size_t size = _advServices.size();
            for (size_t i = 0; i < size; i++) {
                const std::shared_ptr<uuid_t> & e = _advServices[i];
                if( 0 < i ) {
                    out.append("\n");
                }
                out.append("  ").append(e->toUUID128String()).append(", ").append(std::to_string(static_cast<int>(e->getTypeSize()))).append(" bytes");
            }
        }
    }
    return out;
}

EIRDataType BTDevice::update(EInfoReport const & data) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_data); // RAII-style acquire and relinquish via destructor
    jau::sc_atomic_critical sync(sync_data); // redundant due to mutex-lock cache-load operation, leaving it for doc

    EIRDataType res = EIRDataType::NONE;
    ts_last_update = data.getTimestamp();
    if( data.isSet(EIRDataType::BDADDR) ) {
        if( data.getAddress() != this->addressAndType.address ) {
            WARN_PRINT("BTDevice::update:: BDADDR update not supported: %s for %s",
                    data.toString().c_str(), this->toString().c_str());
        }
    }
    if( data.isSet(EIRDataType::BDADDR_TYPE) ) {
        if( data.getAddressType() != this->addressAndType.type ) {
            WARN_PRINT("BTDevice::update:: BDADDR_TYPE update not supported: %s for %s",
                    data.toString().c_str(), this->toString().c_str());
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

EIRDataType BTDevice::update(GattGenericAccessSvc const &data, const uint64_t timestamp) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_data); // RAII-style acquire and relinquish via destructor
    jau::sc_atomic_critical sync(sync_data); // redundant due to mutex-lock cache-load operation, leaving it for doc

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

bool BTDevice::addStatusListener(std::shared_ptr<AdapterStatusListener> l) {
    return adapter.addStatusListener(*this, l);
}

bool BTDevice::removeStatusListener(std::shared_ptr<AdapterStatusListener> l) {
    return adapter.removeStatusListener(l);
}

std::shared_ptr<ConnectionInfo> BTDevice::getConnectionInfo() noexcept {
    BTManager & mgmt = adapter.getManager();
    std::shared_ptr<ConnectionInfo> connInfo = mgmt.getConnectionInfo(adapter.dev_id, addressAndType);
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
            std::shared_ptr<BTDevice> sharedInstance = getSharedInstance();
            if( nullptr == sharedInstance ) {
                ERR_PRINT("BTDevice::getConnectionInfo: Device unknown to adapter and not tracked: %s", toString().c_str());
            } else {
                adapter.sendDeviceUpdated("getConnectionInfo", sharedInstance, jau::getCurrentMilliseconds(), updateMask);
            }
        }
    }
    return connInfo;
}

// #define TEST_NOENC 1

HCIStatusCode BTDevice::connectLE(uint16_t le_scan_interval, uint16_t le_scan_window,
                                   uint16_t conn_interval_min, uint16_t conn_interval_max,
                                   uint16_t conn_latency, uint16_t supervision_timeout) noexcept
{
    const std::lock_guard<std::recursive_mutex> lock_conn(mtx_connect); // RAII-style acquire and relinquish via destructor
    if( !adapter.isPowered() ) {
        WARN_PRINT("BTDevice::connectLE: Adapter not powered: %s", adapter.toString().c_str());
        return HCIStatusCode::UNSPECIFIED_ERROR;
    }
    HCILEOwnAddressType hci_own_mac_type;
    HCILEPeerAddressType hci_peer_mac_type;

    switch( addressAndType.type ) {
        case BDAddressType::BDADDR_LE_PUBLIC:
            hci_peer_mac_type = HCILEPeerAddressType::PUBLIC;
            hci_own_mac_type = HCILEOwnAddressType::PUBLIC;
            break;
        case BDAddressType::BDADDR_LE_RANDOM: {
            const BLERandomAddressType leRandomAddressType = addressAndType.getBLERandomAddressType();
            switch( leRandomAddressType ) {
                case BLERandomAddressType::UNRESOLVABLE_PRIVAT:
                    hci_peer_mac_type = HCILEPeerAddressType::RANDOM;
                    hci_own_mac_type = HCILEOwnAddressType::RANDOM;
                    ERR_PRINT("LE Random address type '%s' not supported yet: %s",
                            to_string(leRandomAddressType).c_str(), toString().c_str());
                    return HCIStatusCode::UNACCEPTABLE_CONNECTION_PARAM;
                case BLERandomAddressType::RESOLVABLE_PRIVAT:
                    hci_peer_mac_type = HCILEPeerAddressType::PUBLIC_IDENTITY;
                    hci_own_mac_type = HCILEOwnAddressType::RESOLVABLE_OR_PUBLIC;
                    ERR_PRINT("LE Random address type '%s' not supported yet: %s",
                            to_string(leRandomAddressType).c_str(), toString().c_str());
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
                            to_string(leRandomAddressType).c_str(), toString().c_str());
                    return HCIStatusCode::UNACCEPTABLE_CONNECTION_PARAM;
                }
            }
        } break;
        default: {
            ERR_PRINT("Can't connectLE to address type '%s': %s", to_string(addressAndType.type).c_str(), toString().c_str());
            return HCIStatusCode::UNACCEPTABLE_CONNECTION_PARAM;
        }
    }

    if( isConnected ) {
        ERR_PRINT("BTDevice::connectLE: Already connected: %s", toString().c_str());
        return HCIStatusCode::CONNECTION_ALREADY_EXISTS;
    }

    HCIHandler &hci = adapter.getHCI();
    if( !hci.isOpen() ) {
        ERR_PRINT("BTDevice::connectLE: HCI closed: %s", toString().c_str());
        return HCIStatusCode::INTERNAL_FAILURE;
    }

    HCIStatusCode statusConnect; // HCI le_create_conn result
    const SMPIOCapability smp_auto_io_cap = pairing_data.ioCap_auto; // cache against clearSMPState
    const bool smp_auto = SMPIOCapability::UNSET != smp_auto_io_cap; // logical cached state
    bool smp_auto_done = !smp_auto;
    int smp_auto_count = 0;
    BTSecurityLevel sec_level = pairing_data.sec_level_user; // iterate down
    SMPIOCapability io_cap = pairing_data.ioCap_user; // iterate down
    SMPPairingState pstate = SMPPairingState::NONE;

    do {
        smp_auto_count++;

        if( !smp_auto_done ) {
            BTSecurityLevel sec_level_pre = sec_level;
            SMPIOCapability io_cap_pre = io_cap;
            if( BTSecurityLevel::UNSET == sec_level && SMPIOCapability::NO_INPUT_NO_OUTPUT != smp_auto_io_cap ) {
                sec_level = BTSecurityLevel::ENC_AUTH_FIPS;
                io_cap    = smp_auto_io_cap;
            } else if( BTSecurityLevel::ENC_AUTH_FIPS == sec_level && SMPIOCapability::NO_INPUT_NO_OUTPUT != smp_auto_io_cap ) {
                sec_level = BTSecurityLevel::ENC_AUTH;
                io_cap    = smp_auto_io_cap;
            } else if( BTSecurityLevel::ENC_AUTH == sec_level || BTSecurityLevel::UNSET == sec_level ) {
#if TEST_NOENC
                sec_level = BTSecurityLevel::NONE;
                io_cap    = SMPIOCapability::NO_INPUT_NO_OUTPUT;
                smp_auto_done = true;
#else
                sec_level = BTSecurityLevel::ENC_ONLY;
                io_cap    = SMPIOCapability::NO_INPUT_NO_OUTPUT;
#endif
            } else if( BTSecurityLevel::ENC_ONLY == sec_level ) {
                sec_level = BTSecurityLevel::NONE;
                io_cap    = SMPIOCapability::NO_INPUT_NO_OUTPUT;
                smp_auto_done = true;
            }
            pairing_data.ioCap_auto = smp_auto_io_cap; // reload against clearSMPState
            pairing_data.sec_level_user = sec_level;
            pairing_data.ioCap_user = io_cap;
            DBG_PRINT("BTDevice::connectLE: SEC AUTO.%d.1: lvl %s -> %s, io %s -> %s, %s", smp_auto_count,
                to_string(sec_level_pre).c_str(), to_string(sec_level).c_str(),
                to_string(io_cap_pre).c_str(), to_string(io_cap).c_str(),
                toString().c_str());
        }

        {
            jau::sc_atomic_critical sync(sync_data);
            if( !adapter.lockConnect(*this, true /* wait */, pairing_data.ioCap_user) ) {
                ERR_PRINT("BTDevice::connectLE: adapter::lockConnect() failed: %s", toString().c_str());
                return HCIStatusCode::INTERNAL_FAILURE;
            }
        }
        statusConnect = hci.le_create_conn(addressAndType.address,
                                    hci_peer_mac_type, hci_own_mac_type,
                                    le_scan_interval, le_scan_window, conn_interval_min, conn_interval_max,
                                    conn_latency, supervision_timeout);
        allowDisconnect = true;
        if( HCIStatusCode::COMMAND_DISALLOWED == statusConnect ) {
            WARN_PRINT("BTDevice::connectLE: Could not yet create connection: status 0x%2.2X (%s), errno %d, hci-atype[peer %s, own %s] %s on %s",
                    static_cast<uint8_t>(statusConnect), to_string(statusConnect).c_str(), errno, strerror(errno),
                    to_string(hci_peer_mac_type).c_str(),
                    to_string(hci_own_mac_type).c_str(),
                    toString().c_str());
            adapter.unlockConnect(*this);
            smp_auto_done = true; // premature end of potential SMP auto-negotiation
        } else if ( HCIStatusCode::SUCCESS != statusConnect ) {
            ERR_PRINT("BTDevice::connectLE: Could not create connection: status 0x%2.2X (%s), errno %d %s, hci-atype[peer %s, own %s] on %s",
                    static_cast<uint8_t>(statusConnect), to_string(statusConnect).c_str(), errno, strerror(errno),
                    to_string(hci_peer_mac_type).c_str(),
                    to_string(hci_own_mac_type).c_str(),
                    toString().c_str());
            adapter.unlockConnect(*this);
            smp_auto_done = true; // premature end of potential SMP auto-negotiation
        } else if( smp_auto ) { // implies HCIStatusCode::SUCCESS
            // Waiting for PairingState
            bool pairing_timeout = false;
            {
                std::unique_lock<std::mutex> lock(mtx_pairing); // RAII-style acquire and relinquish via destructor
                jau::sc_atomic_critical sync1(sync_data);

                while( !hasSMPPairingFinished( pairing_data.state ) ) {
                    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
                    std::cv_status s = cv_pairing_state_changed.wait_until(lock, t0 + std::chrono::milliseconds(hci.env.HCI_COMMAND_COMPLETE_REPLY_TIMEOUT));
                    DBG_PRINT("BTDevice::connectLE: SEC AUTO.%d.2c Wait for SMPPairing: state %s, %s",
                            smp_auto_count, to_string(pairing_data.state).c_str(), toString().c_str());
                    if( std::cv_status::timeout == s && !hasSMPPairingFinished( pairing_data.state ) ) {
                        // timeout
                        ERR_PRINT("BTDevice::connectLE: SEC AUTO.%d.X Timeout SMPPairing: Disconnecting %s", smp_auto_count, toString().c_str());
                        smp_auto_done = true;
                        pairing_data.ioCap_auto = SMPIOCapability::UNSET;
                        pairing_timeout = true;
                        break;
                    }
                }
                pstate = pairing_data.state;
                DBG_PRINT("BTDevice::connectLE: SEC AUTO.%d.2d Wait for SMPPairing: state %s, %s",
                        smp_auto_count, to_string(pstate).c_str(), toString().c_str());
            }
            if( pairing_timeout ) {
                pairing_data.ioCap_auto = SMPIOCapability::UNSET;
                disconnect(HCIStatusCode::REMOTE_USER_TERMINATED_CONNECTION);
                statusConnect = HCIStatusCode::INTERNAL_TIMEOUT;
                adapter.unlockConnect(*this);
                smp_auto_done = true;
            } else if( SMPPairingState::COMPLETED == pstate ) {
                DBG_PRINT("BTDevice::connectLE: SEC AUTO.%d.X Done: %s", smp_auto_count, toString().c_str());
                smp_auto_done = true;
                break;
            } else if( SMPPairingState::FAILED == pstate ) {
                if( !smp_auto_done ) { // not last one
                    // disconnect for next smp_auto mode test
                    int32_t td_disconnect = 0;
                    DBG_PRINT("BTDevice::connectLE: SEC AUTO.%d.3 Failed SMPPairing -> Disconnect: %s", smp_auto_count, toString().c_str());
                    HCIStatusCode dres = disconnect(HCIStatusCode::REMOTE_USER_TERMINATED_CONNECTION);
                    if( HCIStatusCode::SUCCESS == dres ) {
                        while( isConnected && hci.env.HCI_COMMAND_COMPLETE_REPLY_TIMEOUT > td_disconnect ) {
                            jau::sc_atomic_critical sync2(sync_data);
                            td_disconnect += hci.env.HCI_COMMAND_POLL_PERIOD;
                            std::this_thread::sleep_for(std::chrono::milliseconds(hci.env.HCI_COMMAND_POLL_PERIOD));
                        }
                    }
                    if( hci.env.HCI_COMMAND_COMPLETE_REPLY_TIMEOUT <= td_disconnect ) {
                        // timeout
                        ERR_PRINT("BTDevice::connectLE: SEC AUTO.%d.4 Timeout Disconnect td_pairing %d ms: %s",
                                smp_auto_count, td_disconnect, toString().c_str());
                        pairing_data.ioCap_auto = SMPIOCapability::UNSET;
                        statusConnect = HCIStatusCode::INTERNAL_TIMEOUT;
                        adapter.unlockConnect(*this);
                        smp_auto_done = true;
                    }
                }
            }
        }
    } while( !smp_auto_done );
    if( smp_auto ) {
        jau::sc_atomic_critical sync(sync_data);
        if( HCIStatusCode::SUCCESS == statusConnect && SMPPairingState::FAILED == pstate ) {
            ERR_PRINT("BTDevice::connectLE: SEC AUTO.%d.X Failed SMPPairing -> Disconnect: %s", smp_auto_count, toString().c_str());
            pairing_data.ioCap_auto = SMPIOCapability::UNSET;
            disconnect(HCIStatusCode::REMOTE_USER_TERMINATED_CONNECTION);
            statusConnect = HCIStatusCode::AUTH_FAILED;
        }
        pairing_data.ioCap_auto = SMPIOCapability::UNSET; // always clear post-auto-action: Allow normal notification.
    }
    return statusConnect;
}

HCIStatusCode BTDevice::connectBREDR(const uint16_t pkt_type, const uint16_t clock_offset, const uint8_t role_switch) noexcept
{
    const std::lock_guard<std::recursive_mutex> lock_conn(mtx_connect); // RAII-style acquire and relinquish via destructor
    if( !adapter.isPowered() ) {
        WARN_PRINT("BTDevice::connectBREDR: Adapter not powered: %s", adapter.toString().c_str());
        return HCIStatusCode::UNSPECIFIED_ERROR;
    }

    if( isConnected ) {
        ERR_PRINT("BTDevice::connectBREDR: Already connected: %s", toString().c_str());
        return HCIStatusCode::CONNECTION_ALREADY_EXISTS;
    }
    if( !addressAndType.isBREDRAddress() ) {
        ERR_PRINT("BTDevice::connectBREDR: Not a BDADDR_BREDR address: %s", toString().c_str());
        return HCIStatusCode::UNACCEPTABLE_CONNECTION_PARAM;
    }

    HCIHandler &hci = adapter.getHCI();
    if( !hci.isOpen() ) {
        ERR_PRINT("BTDevice::connectBREDR: HCI closed: %s", toString().c_str());
        return HCIStatusCode::INTERNAL_FAILURE;
    }

    {
        jau::sc_atomic_critical sync(sync_data);
        if( !adapter.lockConnect(*this, true /* wait */, pairing_data.ioCap_user) ) {
            ERR_PRINT("BTDevice::connectBREDR: adapter::lockConnect() failed: %s", toString().c_str());
            return HCIStatusCode::INTERNAL_FAILURE;
        }
    }
    HCIStatusCode status = hci.create_conn(addressAndType.address, pkt_type, clock_offset, role_switch);
    allowDisconnect = true;
    if ( HCIStatusCode::SUCCESS != status ) {
        ERR_PRINT("BTDevice::connectBREDR: Could not create connection: status 0x%2.2X (%s), errno %d %s on %s",
                static_cast<uint8_t>(status), to_string(status).c_str(), errno, strerror(errno), toString().c_str());
        adapter.unlockConnect(*this);
    }
    return status;
}

HCIStatusCode BTDevice::connectDefault() noexcept
{
    switch( addressAndType.type ) {
        case BDAddressType::BDADDR_LE_PUBLIC:
            [[fallthrough]];
        case BDAddressType::BDADDR_LE_RANDOM:
            return connectLE();
        case BDAddressType::BDADDR_BREDR:
            return connectBREDR();
        default:
            ERR_PRINT("BTDevice::connectDefault: Not a valid address type: %s", toString().c_str());
            return HCIStatusCode::UNACCEPTABLE_CONNECTION_PARAM;
    }
}

void BTDevice::notifyConnected(std::shared_ptr<BTDevice> sthis, const uint16_t handle, const SMPIOCapability io_cap) noexcept {
    // coming from connected callback, update state and spawn-off connectGATT in background if appropriate (LE)
    jau::sc_atomic_critical sync(sync_data);
    DBG_PRINT("BTDevice::notifyConnected: handle %s -> %s, io %s -> %s, %s",
              jau::to_hexstring(hciConnHandle).c_str(), jau::to_hexstring(handle).c_str(),
              to_string(pairing_data.ioCap_conn).c_str(), to_string(io_cap).c_str(),
              toString().c_str());
    clearSMPStates(true /* connected */);
    allowDisconnect = true;
    isConnected = true;
    hciConnHandle = handle;
    if( SMPIOCapability::UNSET == pairing_data.ioCap_conn ) {
        pairing_data.ioCap_conn = io_cap;
    }
    (void)sthis; // not used yet
}

void BTDevice::notifyLEFeatures(std::shared_ptr<BTDevice> sthis, const LEFeatures features) noexcept {
    DBG_PRINT("BTDevice::notifyLEFeatures: LE_Encryption %d, %s",
            isLEFeaturesBitSet(features, LEFeatures::LE_Encryption), toString().c_str());
    le_features = features;

    if( addressAndType.isLEAddress() && !l2cap_att.isOpen() ) {
        std::thread bg(&BTDevice::processL2CAPSetup, this, sthis); // @suppress("Invalid arguments")
        bg.detach();
    }
}

void BTDevice::processL2CAPSetup(std::shared_ptr<BTDevice> sthis) {
    bool callProcessDeviceReady = false;

    if( addressAndType.isLEAddress() && !l2cap_att.isOpen() ) {
        const std::unique_lock<std::mutex> lock(mtx_pairing); // RAII-style acquire and relinquish via destructor
        jau::sc_atomic_critical sync(sync_data);

        DBG_PRINT("BTDevice::processL2CAPSetup: Start %s", toString().c_str());

        const BTSecurityLevel sec_level_user = pairing_data.sec_level_user;
        const SMPIOCapability io_cap_conn = pairing_data.ioCap_conn;
        BTSecurityLevel sec_level { BTSecurityLevel::UNSET };

        const bool responderLikesEncryption = pairing_data.res_requested_sec || isLEFeaturesBitSet(le_features, LEFeatures::LE_Encryption);
        if( BTSecurityLevel::UNSET != sec_level_user ) {
            sec_level = sec_level_user;
        } else if( responderLikesEncryption && SMPIOCapability::UNSET != io_cap_conn ) {
            if( SMPIOCapability::NO_INPUT_NO_OUTPUT == io_cap_conn ) {
                sec_level = BTSecurityLevel::ENC_ONLY; // no auth w/o I/O
            } else if( adapter.hasSecureConnections() ) {
                sec_level = BTSecurityLevel::ENC_AUTH_FIPS;
            } else if( responderLikesEncryption ) {
                sec_level = BTSecurityLevel::ENC_AUTH;
            }
        } else {
            sec_level = BTSecurityLevel::NONE;
        }
        pairing_data.sec_level_conn = sec_level;
        DBG_PRINT("BTDevice::processL2CAPSetup: sec_level_user %s, io_cap_conn %s -> sec_level %s",
                to_string(sec_level_user).c_str(),
                to_string(io_cap_conn).c_str(),
                to_string(sec_level).c_str());

        const bool l2cap_open = l2cap_att.open(*this, sec_level); // initiates hciSMPMsgCallback() if sec_level > BT_SECURITY_LOW
        const bool l2cap_enc = l2cap_open && BTSecurityLevel::NONE < sec_level;
#if SMP_SUPPORTED_BY_OS
        const bool smp_enc = connectSMP(sthis, sec_level) && BTSecurityLevel::NONE < sec_level;
#else
        const bool smp_enc = false;
#endif
        DBG_PRINT("BTDevice::processL2CAPSetup: lvl %s, connect[smp_enc %d, l2cap[open %d, enc %d]]",
                to_string(sec_level).c_str(), smp_enc, l2cap_open, l2cap_enc);

        adapter.unlockConnect(*this);

        if( !l2cap_open ) {
            pairing_data.sec_level_conn = BTSecurityLevel::NONE;
            disconnect(HCIStatusCode::INTERNAL_FAILURE);
        } else if( !l2cap_enc ) {
            callProcessDeviceReady = true;
        }
    } else {
        DBG_PRINT("BTDevice::processL2CAPSetup: Skipped (not LE) %s", toString().c_str());
    }
    if( callProcessDeviceReady ) {
        // call out of scope of locked mtx_pairing
        processDeviceReady(sthis, jau::getCurrentMilliseconds());
    }
    DBG_PRINT("BTDevice::processL2CAPSetup: End %s", toString().c_str());
}

void BTDevice::processDeviceReady(std::shared_ptr<BTDevice> sthis, const uint64_t timestamp) {
    DBG_PRINT("BTDevice::processDeviceReady: %s", toString().c_str());
    PairingMode pmode;
    {
        jau::sc_atomic_critical sync(sync_data);
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
    DBG_PRINT("BTDevice::processDeviceReady: ready[GATT %d, unpair %s], %s",
            res1, to_string(unpair_res).c_str(), toString().c_str());
    if( res1 ) {
        adapter.sendDeviceReady(sthis, timestamp);
    }
}


static const SMPKeyType _key_mask_legacy = SMPKeyType::ENC_KEY | SMPKeyType::ID_KEY | SMPKeyType::SIGN_KEY;
static const SMPKeyType _key_mask_sc     =                       SMPKeyType::ID_KEY | SMPKeyType::SIGN_KEY | SMPKeyType::LINK_KEY;

bool BTDevice::checkPairingKeyDistributionComplete(const std::string& timestamp) const noexcept {
    bool res = false;

    if( SMPPairingState::KEY_DISTRIBUTION == pairing_data.state ) {
        // Spec allows responder to not distribute the keys,
        // hence distribution is complete with initiator (LL master) keys!
        // Impact of missing responder keys: Requires new pairing each connection.
        if( pairing_data.use_sc ) {
            if( pairing_data.keys_init_has == ( pairing_data.keys_init_exp & _key_mask_sc ) ) {
                // pairing_data.keys_resp_has == ( pairing_data.keys_resp_exp & key_mask_sc )
                res = true;
            }
        } else {
            if( pairing_data.keys_init_has == ( pairing_data.keys_init_exp & _key_mask_legacy ) ) {
                // pairing_data.keys_resp_has == ( pairing_data.keys_resp_exp & key_mask_legacy )
                res = true;
            }
        }

        if( jau::environment::get().debug ) {
            jau::PLAIN_PRINT(false, "[%s] Debug: BTDevice:SMP:KEY_DISTRIBUTION: done %d, address%s",
                timestamp.c_str(), res,
                addressAndType.toString().c_str());
            jau::PLAIN_PRINT(false, "[%s] - keys[init %s / %s, resp %s / %s]",
                timestamp.c_str(),
                to_string(pairing_data.keys_init_has).c_str(),
                to_string(pairing_data.keys_init_exp).c_str(),
                to_string(pairing_data.keys_resp_has).c_str(),
                to_string(pairing_data.keys_resp_exp).c_str());
        }
    }

    return res;
}

bool BTDevice::updatePairingState(std::shared_ptr<BTDevice> sthis, const MgmtEvent& evt, const HCIStatusCode evtStatus, SMPPairingState claimed_state) noexcept {
    const std::unique_lock<std::mutex> lock(mtx_pairing); // RAII-style acquire and relinquish via destructor
    jau::sc_atomic_critical sync(sync_data);

    const SMPIOCapability iocap = pairing_data.ioCap_conn;
    const MgmtEvent::Opcode mgmtEvtOpcode = evt.getOpcode();
    PairingMode mode = pairing_data.mode;
    bool is_device_ready = false;

    if( pairing_data.state != claimed_state ) {
        // Potentially force update PairingMode by forced state change, assuming being the initiator.
        switch( claimed_state ) {
            case SMPPairingState::NONE:
                // no change
                claimed_state = pairing_data.state;
                break;
            case SMPPairingState::FAILED: { /* Next: disconnect(..) by user or auto-mode */
                mode = PairingMode::NONE;
            } break;
            case SMPPairingState::PASSKEY_EXPECTED:
                if( hasSMPIOCapabilityFullInput( iocap ) ) {
                    mode = PairingMode::PASSKEY_ENTRY_ini;
                } else {
                    // BT core requesting passkey input w/o full input caps is nonsense (bug?)
                    // Reply with a default value '0' off-thread ASAP
                    DBG_PRINT("BTDevice::updatePairingState.1a: state %s [ignored %s, sending dummy reply], mode %s, %s",
                        to_string(pairing_data.state).c_str(), to_string(claimed_state).c_str(),
                        to_string(pairing_data.mode).c_str(), evt.toString().c_str());
                    claimed_state = pairing_data.state; // suppress
                    std::thread dc(&BTDevice::setPairingPasskey, sthis, 0);
                    dc.detach();
                }
                break;
            case SMPPairingState::NUMERIC_COMPARE_EXPECTED:
                if( hasSMPIOCapabilityBinaryInput( iocap ) ) {
                    mode = PairingMode::NUMERIC_COMPARE_ini;
                } else {
                    // BT core requesting binary input w/o input caps is nonsense (bug?)
                    // Reply with a default value 'true' off-thread ASAP
                    DBG_PRINT("BTDevice::updatePairingState.1b: state %s [ignored %s, sending dummy reply], mode %s, %s",
                        to_string(pairing_data.state).c_str(), to_string(claimed_state).c_str(),
                        to_string(pairing_data.mode).c_str(), evt.toString().c_str());
                    claimed_state = pairing_data.state; // suppress
                    std::thread dc(&BTDevice::setPairingNumericComparison, sthis, true);
                    dc.detach();
                }
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
                } else if( MgmtEvent::Opcode::NEW_LONG_TERM_KEY == mgmtEvtOpcode &&
                           HCIStatusCode::SUCCESS == evtStatus &&
                           SMPPairingState::KEY_DISTRIBUTION == pairing_data.state )
                {
                    // SMP pairing has started, mngr issued new LTK key command
                    const MgmtEvtNewLongTermKey& event = *static_cast<const MgmtEvtNewLongTermKey *>(&evt);
                    const MgmtLongTermKeyInfo& ltk_info = event.getLongTermKey();
                    const SMPLongTermKeyInfo smp_ltk = ltk_info.toSMPLongTermKeyInfo();
                    if( smp_ltk.isValid() ) {
                        const std::string timestamp = jau::to_decstring(jau::environment::getElapsedMillisecond(evt.getTimestamp()), ',', 9);

                        if( smp_ltk.isResponder() ) {
                            if( ( SMPKeyType::ENC_KEY & pairing_data.keys_resp_has ) == SMPKeyType::NONE ) { // no overwrite
                                if( jau::environment::get().debug ) {
                                    jau::PLAIN_PRINT(false, "[%s] BTDevice::updatePairingState.2a: ENC_KEY responder set", timestamp.c_str());
                                    jau::PLAIN_PRINT(false, "[%s] - old %s", timestamp.c_str(), pairing_data.ltk_resp.toString().c_str());
                                    jau::PLAIN_PRINT(false, "[%s] - new %s", timestamp.c_str(), smp_ltk.toString().c_str());
                                }
                                pairing_data.ltk_resp = smp_ltk;
                                pairing_data.keys_resp_has |= SMPKeyType::ENC_KEY;
                                if( checkPairingKeyDistributionComplete(timestamp) ) {
                                    is_device_ready = true;
                                }
                            }
                        } else {
                            if( ( SMPKeyType::ENC_KEY & pairing_data.keys_init_has ) == SMPKeyType::NONE ) { // no overwrite
                                if( jau::environment::get().debug ) {
                                    jau::PLAIN_PRINT(false, "[%s] BTDevice::updatePairingState.2b: ENC_KEY initiator set", timestamp.c_str());
                                    jau::PLAIN_PRINT(false, "[%s] - old %s", timestamp.c_str(), pairing_data.ltk_init.toString().c_str());
                                    jau::PLAIN_PRINT(false, "[%s] - new %s", timestamp.c_str(), smp_ltk.toString().c_str());
                                }
                                pairing_data.ltk_init = smp_ltk;
                                pairing_data.keys_init_has |= SMPKeyType::ENC_KEY;
                                if( checkPairingKeyDistributionComplete(timestamp) ) {
                                    is_device_ready = true;
                                }
                            }
                        }
                        if( !is_device_ready ) {
                            claimed_state = pairing_data.state; // not yet
                        }
                    }
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
        DBG_PRINT("BTDevice::updatePairingState.3: state %s -> %s, mode %s -> %s, ready %d, %s",
            to_string(pairing_data.state).c_str(), to_string(claimed_state).c_str(),
            to_string(pairing_data.mode).c_str(), to_string(mode).c_str(),
            is_device_ready, evt.toString().c_str());

        pairing_data.mode = mode;
        pairing_data.state = claimed_state;

        adapter.sendDevicePairingState(sthis, claimed_state, mode, evt.getTimestamp());

        if( is_device_ready ) {
            std::thread dc(&BTDevice::processDeviceReady, this, sthis, evt.getTimestamp()); // @suppress("Invalid arguments")
            dc.detach();
        }
        DBG_PRINT("BTDevice::updatePairingState.4: End Complete: state %s, %s",
                to_string(claimed_state).c_str(), toString().c_str());

        cv_pairing_state_changed.notify_all();

        return true;
    } else {
        DBG_PRINT("BTDevice::updatePairingState.5: End Unchanged: state %s, %s, %s",
            to_string(pairing_data.state).c_str(),
            evt.toString().c_str(), toString().c_str());
    }
    return false;
}

void BTDevice::hciSMPMsgCallback(std::shared_ptr<BTDevice> sthis, const SMPPDUMsg& msg, const HCIACLData::l2cap_frame& source) noexcept {
    const std::unique_lock<std::mutex> lock(mtx_pairing); // RAII-style acquire and relinquish via destructor
    jau::sc_atomic_critical sync(sync_data);

    const SMPPairingState old_pstate = pairing_data.state;
    const PairingMode old_pmode = pairing_data.mode;
    const std::string timestamp = jau::to_decstring(jau::environment::getElapsedMillisecond(msg.getTimestamp()), ',', 9);

    SMPPairingState pstate = old_pstate;
    PairingMode pmode = old_pmode;
    bool is_device_ready = false;

    if( jau::environment::get().debug ) {
        jau::PLAIN_PRINT(false, "[%s] Debug: BTDevice:hci:SMP.0: address%s",
            timestamp.c_str(),
            addressAndType.toString().c_str());
        jau::PLAIN_PRINT(false, "[%s] - %s", timestamp.c_str(), msg.toString().c_str());
        jau::PLAIN_PRINT(false, "[%s] - %s", timestamp.c_str(), source.toString().c_str());
        jau::PLAIN_PRINT(false, "[%s] - %s", timestamp.c_str(), toString().c_str());
    }

    const SMPPDUMsg::Opcode opc = msg.getOpcode();

    switch( opc ) {
        // Phase 1: SMP Negotiation phase

        case SMPPDUMsg::Opcode::SECURITY_REQUEST:
            pmode = PairingMode::NEGOTIATING;
            pstate = SMPPairingState::REQUESTED_BY_RESPONDER;
            pairing_data.res_requested_sec = true;
            break;

        case SMPPDUMsg::Opcode::PAIRING_REQUEST:
            if( HCIACLData::l2cap_frame::PBFlag::START_NON_AUTOFLUSH_HOST == source.pb_flag ) { // from initiator (master)
                const SMPPairingMsg & msg1 = *static_cast<const SMPPairingMsg *>( &msg );
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
                const SMPPairingMsg & msg1 = *static_cast<const SMPPairingMsg *>( &msg );
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
                    jau::PLAIN_PRINT(false, "[%s] Debug: BTDevice:hci:SMP.2: address%s: State %s, Mode %s, using SC %d:", timestamp.c_str() ,
                        addressAndType.toString().c_str(),
                        to_string(pstate).c_str(), to_string(pmode).c_str(), use_sc);
                    jau::PLAIN_PRINT(false, "[%s] - oob:   init %s", timestamp.c_str(), to_string(pairing_data.oobFlag_init).c_str());
                    jau::PLAIN_PRINT(false, "[%s] - oob:   resp %s", timestamp.c_str(), to_string(pairing_data.oobFlag_resp).c_str());
                    jau::PLAIN_PRINT(false, "[%s] ", timestamp.c_str());
                    jau::PLAIN_PRINT(false, "[%s] - auth:  init %s", timestamp.c_str(), to_string(pairing_data.authReqs_init).c_str());
                    jau::PLAIN_PRINT(false, "[%s] - auth:  resp %s", timestamp.c_str(), to_string(pairing_data.authReqs_resp).c_str());
                    jau::PLAIN_PRINT(false, "[%s] ", timestamp.c_str());
                    jau::PLAIN_PRINT(false, "[%s] - iocap: init %s", timestamp.c_str(), to_string(pairing_data.ioCap_init).c_str());
                    jau::PLAIN_PRINT(false, "[%s] - iocap: resp %s", timestamp.c_str(), to_string(pairing_data.ioCap_resp).c_str());
                    jau::PLAIN_PRINT(false, "[%s] ", timestamp.c_str());
                    jau::PLAIN_PRINT(false, "[%s] - encsz: init %d", timestamp.c_str(), (int)pairing_data.maxEncsz_init);
                    jau::PLAIN_PRINT(false, "[%s] - encsz: resp %d", timestamp.c_str(), (int)pairing_data.maxEncsz_resp);
                    jau::PLAIN_PRINT(false, "[%s] ", timestamp.c_str());
                    jau::PLAIN_PRINT(false, "[%s] - keys:  init %s", timestamp.c_str(), to_string(pairing_data.keys_init_exp).c_str());
                    jau::PLAIN_PRINT(false, "[%s] - keys:  resp %s", timestamp.c_str(), to_string(pairing_data.keys_resp_exp).c_str());
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

        case SMPPDUMsg::Opcode::PAIRING_FAILED: { /* Next: disconnect(..) by user or auto-mode */
            pmode = PairingMode::NONE;
            pstate = SMPPairingState::FAILED;
          } break;

        // Phase 3: SMP Key & Value Distribution phase

        case SMPPDUMsg::Opcode::ENCRYPTION_INFORMATION: {     /* Legacy: 1 */
            // LTK: First part for SMPKeyDistFormat::ENC_KEY, followed by MASTER_IDENTIFICATION (EDIV + RAND)
            const SMPEncInfoMsg & msg1 = *static_cast<const SMPEncInfoMsg *>( &msg );
            if( HCIACLData::l2cap_frame::PBFlag::START_AUTOFLUSH == source.pb_flag ) {
                // from responder (LL slave)
                pairing_data.ltk_resp.properties |= SMPLongTermKeyInfo::Property::RESPONDER;
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
                // pairing_data.ltk_resp.properties |= SMPLongTermKeyInfo::Property::INITIATOR;
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
            const SMPMasterIdentMsg & msg1 = *static_cast<const SMPMasterIdentMsg *>( &msg );
            if( HCIACLData::l2cap_frame::PBFlag::START_AUTOFLUSH == source.pb_flag ) {
                // from responder (LL slave)
                pairing_data.keys_resp_has |= SMPKeyType::ENC_KEY;
                pairing_data.ltk_resp.ediv = msg1.getEDIV();
                pairing_data.ltk_resp.rand = msg1.getRand();
            } else {
                // from initiator (LL master)
                pairing_data.keys_init_has |= SMPKeyType::ENC_KEY;
                pairing_data.ltk_init.ediv = msg1.getEDIV();
                pairing_data.ltk_init.rand = msg1.getRand();
            }
        }   break;

        case SMPPDUMsg::Opcode::IDENTITY_INFORMATION: {       /* Legacy: 3; SC: 1 */
            // IRK: First part of SMPKeyDist::ID_KEY, followed by IDENTITY_ADDRESS_INFORMATION
            const SMPIdentInfoMsg & msg1 = *static_cast<const SMPIdentInfoMsg *>( &msg );
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
            const SMPIdentAddrInfoMsg & msg1 = *static_cast<const SMPIdentAddrInfoMsg *>( &msg );
            if( HCIACLData::l2cap_frame::PBFlag::START_AUTOFLUSH == source.pb_flag ) {
                // from responder (LL slave)
                pairing_data.keys_resp_has |= SMPKeyType::ID_KEY;
                pairing_data.address = msg1.getAddress();
                pairing_data.is_static_random_address = msg1.isStaticRandomAddress();
            } else {
                // from initiator (LL master)
                pairing_data.keys_init_has |= SMPKeyType::ID_KEY;
                pairing_data.address = msg1.getAddress();
                pairing_data.is_static_random_address = msg1.isStaticRandomAddress();
            }
        }   break;

        case SMPPDUMsg::Opcode::SIGNING_INFORMATION: {        /* Legacy: 5; SC: 3; Last value. */
            // CSRK
            const SMPSignInfoMsg & msg1 = *static_cast<const SMPSignInfoMsg *>( &msg );
            if( HCIACLData::l2cap_frame::PBFlag::START_AUTOFLUSH == source.pb_flag ) {
                // from responder (LL slave)
                pairing_data.keys_resp_has |= SMPKeyType::SIGN_KEY;

                pairing_data.csrk_resp.properties |= SMPSignatureResolvingKeyInfo::Property::RESPONDER;
                if( BTSecurityLevel::ENC_AUTH <= pairing_data.sec_level_conn ) {
                    pairing_data.csrk_resp.properties |= SMPSignatureResolvingKeyInfo::Property::AUTH;
                }
                pairing_data.csrk_resp.csrk = msg1.getCSRK();
            } else {
                // from initiator (LL master)
                pairing_data.keys_init_has |= SMPKeyType::SIGN_KEY;

                // pairing_data.csrk_init.properties |= SMPSignatureResolvingKeyInfo::Property::INITIATOR;
                if( BTSecurityLevel::ENC_AUTH <= pairing_data.sec_level_conn ) {
                    pairing_data.csrk_init.properties |= SMPSignatureResolvingKeyInfo::Property::AUTH;
                }
                pairing_data.csrk_init.csrk = msg1.getCSRK();
            }
        }   break;

        default:
            break;
    }

    if( checkPairingKeyDistributionComplete(timestamp) ) {
        pstate = SMPPairingState::COMPLETED;
        is_device_ready = true;
    }

    if( jau::environment::get().debug ) {
        if( old_pstate == pstate /* && old_pmode == pmode */ ) {
            jau::PLAIN_PRINT(false, "[%s] Debug: BTDevice:hci:SMP.4: Unchanged: address%s",
                timestamp.c_str(),
                addressAndType.toString().c_str());
        } else {
            jau::PLAIN_PRINT(false, "[%s] Debug: BTDevice:hci:SMP.5:   Updated: address%s",
                timestamp.c_str(),
                addressAndType.toString().c_str());
        }
        jau::PLAIN_PRINT(false, "[%s] - state %s -> %s, mode %s -> %s, ready %d",
            timestamp.c_str(),
            to_string(old_pstate).c_str(), to_string(pstate).c_str(),
            to_string(old_pmode).c_str(), to_string(pmode).c_str(),
            is_device_ready);
        jau::PLAIN_PRINT(false, "[%s] - keys[init %s / %s, resp %s / %s]",
            timestamp.c_str(),
            to_string(pairing_data.keys_init_has).c_str(),
            to_string(pairing_data.keys_init_exp).c_str(),
            to_string(pairing_data.keys_resp_has).c_str(),
            to_string(pairing_data.keys_resp_exp).c_str());
    }

    if( old_pstate == pstate /* && old_pmode == pmode */ ) {
        return;
    }

    pairing_data.mode = pmode;
    pairing_data.state = pstate;

    adapter.sendDevicePairingState(sthis, pstate, pmode, msg.getTimestamp());

    if( is_device_ready ) {
        std::thread dc(&BTDevice::processDeviceReady, this, sthis, msg.getTimestamp()); // @suppress("Invalid arguments")
        dc.detach();
    }
    if( jau::environment::get().debug ) {
        jau::PLAIN_PRINT(false, "[%s] Debug: BTDevice:hci:SMP.6: End", timestamp.c_str());
        jau::PLAIN_PRINT(false, "[%s] - %s", timestamp.c_str(), toString().c_str());
    }
    cv_pairing_state_changed.notify_all();
}

SMPKeyType BTDevice::getAvailableSMPKeys(const bool responder) const noexcept {
    jau::sc_atomic_critical sync(sync_data);
    if( responder ) {
        return pairing_data.keys_resp_has;
    } else {
        return pairing_data.keys_init_has;
    }
}

SMPLongTermKeyInfo BTDevice::getLongTermKeyInfo(const bool responder) const noexcept {
    jau::sc_atomic_critical sync(sync_data);
    return responder ? pairing_data.ltk_resp : pairing_data.ltk_init;
}

HCIStatusCode BTDevice::setLongTermKeyInfo(const SMPLongTermKeyInfo& ltk) noexcept {
    if( isConnected ) {
        ERR_PRINT("BTDevice::setLongTermKeyInfo: Already connected: %s", toString().c_str());
        return HCIStatusCode::CONNECTION_ALREADY_EXISTS;
    }
    const std::unique_lock<std::mutex> lock(mtx_pairing); // RAII-style acquire and relinquish via destructor
    jau::sc_atomic_critical sync(sync_data);
    if( ltk.isResponder() ) {
        pairing_data.ltk_resp = ltk;
    } else {
        pairing_data.ltk_init = ltk;
    }
#if USE_LINUX_BT_SECURITY
    BTManager & mngr = adapter.getManager();
    HCIStatusCode res = mngr.uploadLongTermKeyInfo(adapter.dev_id, addressAndType, ltk);
    return res;
#elif SMP_SUPPORTED_BY_OS
    return HCIStatusCode::NOT_SUPPORTED;
#else
    return HCIStatusCode::NOT_SUPPORTED;
#endif
}

SMPSignatureResolvingKeyInfo BTDevice::getSignatureResolvingKeyInfo(const bool responder) const noexcept {
    jau::sc_atomic_critical sync(sync_data);
    return responder ? pairing_data.csrk_resp : pairing_data.csrk_init;
}

HCIStatusCode BTDevice::pair(const SMPIOCapability io_cap) noexcept {
    /**
     * Experimental only.
     * <pre>
     *   adapter.stopDiscovery(): Renders pairDevice(..) to fail: Busy!
     *   pairDevice(..) behaves quite instable within our connected workflow: Not used!
     * </pre>
     */
    if( SMPIOCapability::UNSET == io_cap ) {
        DBG_PRINT("BTDevice::pairDevice: io %s, invalid value.", to_string(io_cap).c_str());
        return HCIStatusCode::INVALID_PARAMS;
    }
#if USE_LINUX_BT_SECURITY
    BTManager& mngr = adapter.getManager();

    DBG_PRINT("BTDevice::pairDevice: Start: io %s, %s", to_string(io_cap).c_str(), toString().c_str());
    mngr.uploadConnParam(adapter.dev_id, addressAndType);

    jau::sc_atomic_critical sync(sync_data);
    pairing_data.ioCap_conn = io_cap;
    const bool res = mngr.pairDevice(adapter.dev_id, addressAndType, io_cap);
    if( !res ) {
        pairing_data.ioCap_conn = SMPIOCapability::UNSET;
    }
    DBG_PRINT("BTDevice::pairDevice: End: io %s, %s", to_string(io_cap).c_str(), toString().c_str());
    return res ? HCIStatusCode::SUCCESS : HCIStatusCode::FAILED;
#elif SMP_SUPPORTED_BY_OS
    return HCIStatusCode::NOT_SUPPORTED;
#else
    return HCIStatusCode::NOT_SUPPORTED;
#endif
}

bool BTDevice::setConnSecurityLevel(const BTSecurityLevel sec_level) noexcept {
    if( BTSecurityLevel::UNSET == sec_level ) {
        DBG_PRINT("DBTAdapter::setConnSecurityLevel: lvl %s, invalid value.", to_string(sec_level).c_str());
        return false;
    }

    if( !isValid() || isConnected || allowDisconnect ) {
        DBG_PRINT("BTDevice::setConnSecurityLevel: lvl %s failed, invalid state %s",
            to_string(sec_level).c_str(), toString().c_str());
        return false;
    }
    jau::sc_atomic_critical sync(sync_data);
    const bool res = true;
    pairing_data.sec_level_user = sec_level;
    pairing_data.ioCap_auto = SMPIOCapability::UNSET; // disable auto

    DBG_PRINT("BTDevice::setConnSecurityLevel: result %d: lvl %s, %s", res,
        to_string(sec_level).c_str(),
        toString().c_str());
    return res;
}

BTSecurityLevel BTDevice::getConnSecurityLevel() const noexcept {
    jau::sc_atomic_critical sync(sync_data);
    return pairing_data.sec_level_conn;
}

bool BTDevice::setConnIOCapability(const SMPIOCapability io_cap) noexcept {
    if( SMPIOCapability::UNSET == io_cap ) {
        DBG_PRINT("BTDevice::setConnIOCapability: io %s, invalid value.", to_string(io_cap).c_str());
        return false;
    }

    if( !isValid() || isConnected || allowDisconnect ) {
        DBG_PRINT("BTDevice::setConnIOCapability: io %s failed, invalid state %s",
                to_string(io_cap).c_str(), toString().c_str());
        return false;
    }
    jau::sc_atomic_critical sync(sync_data);
    const bool res = true;
    pairing_data.ioCap_user = io_cap;
    pairing_data.ioCap_auto = SMPIOCapability::UNSET; // disable auto

    DBG_PRINT("BTDevice::setConnIOCapability: result %d: io %s, %s", res,
        to_string(io_cap).c_str(),
        toString().c_str());

    return res;
}

SMPIOCapability BTDevice::getConnIOCapability() const noexcept {
    jau::sc_atomic_critical sync(sync_data);
    return pairing_data.ioCap_conn;
}

bool BTDevice::setConnSecurity(const BTSecurityLevel sec_level, const SMPIOCapability io_cap) noexcept {
    if( !isValid() || isConnected || allowDisconnect ) {
        DBG_PRINT("BTDevice::setConnSecurity: lvl %s, io %s failed, invalid state %s",
                to_string(sec_level).c_str(),
                to_string(io_cap).c_str(), toString().c_str());
        return false;
    }
    jau::sc_atomic_critical sync(sync_data);
    const bool res = true;
    pairing_data.ioCap_user = io_cap;
    pairing_data.sec_level_user = sec_level;
    pairing_data.ioCap_auto = SMPIOCapability::UNSET; // disable auto

    DBG_PRINT("BTDevice::setConnSecurity: result %d: lvl %s, io %s, %s", res,
        to_string(sec_level).c_str(),
        to_string(io_cap).c_str(),
        toString().c_str());

    return res;
}

bool BTDevice::setConnSecurityBest(const BTSecurityLevel sec_level, const SMPIOCapability io_cap) noexcept {
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

bool BTDevice::setConnSecurityAuto(const SMPIOCapability iocap_auto) noexcept {
    if( !isValid() || isConnected || allowDisconnect ) {
        DBG_PRINT("BTDevice::setConnSecurityAuto: io %s failed, invalid state %s",
                to_string(iocap_auto).c_str(), toString().c_str());
        return false;
    }
    if( BTSecurityLevel::UNSET != pairing_data.sec_level_user ||
        SMPIOCapability::UNSET != pairing_data.ioCap_user )
    {
        DBG_PRINT("BTDevice::setConnSecurityAuto: io %s failed, user connection sec_level %s or io %s set %s",
                to_string(iocap_auto).c_str(),
                to_string(pairing_data.sec_level_user).c_str(),
                to_string(pairing_data.ioCap_user).c_str(),
                toString().c_str());
        return false;
    }

    jau::sc_atomic_critical sync(sync_data);
    const bool res = true;
    pairing_data.ioCap_auto = iocap_auto;
    DBG_PRINT("BTDevice::setConnSecurityAuto: result %d: io %s, %s", res,
            to_string(iocap_auto).c_str(), toString().c_str());
    return res;
}

bool BTDevice::isConnSecurityAutoEnabled() const noexcept {
    jau::sc_atomic_critical sync(sync_data);
    return SMPIOCapability::UNSET != pairing_data.ioCap_auto;
}

HCIStatusCode BTDevice::setPairingPasskey(const uint32_t passkey) noexcept {
    const std::unique_lock<std::mutex> lock(mtx_pairing); // RAII-style acquire and relinquish via destructor
    jau::sc_atomic_critical sync(sync_data);

    if( isSMPPairingAllowingInput(pairing_data.state, SMPPairingState::PASSKEY_EXPECTED) ) {
#if USE_LINUX_BT_SECURITY
        BTManager& mngr = adapter.getManager();
        MgmtStatus res = mngr.userPasskeyReply(adapter.dev_id, addressAndType, passkey);
        DBG_PRINT("BTDevice:mgmt:SMP: PASSKEY '%d', state %s, result %s",
            passkey, to_string(pairing_data.state).c_str(), to_string(res).c_str());
        return HCIStatusCode::SUCCESS;
#elif SMP_SUPPORTED_BY_OS
    return HCIStatusCode::NOT_SUPPORTED;
#else
    return HCIStatusCode::NOT_SUPPORTED;
#endif
    } else {
        ERR_PRINT("BTDevice:mgmt:SMP: PASSKEY '%d', state %s, SKIPPED (wrong state)",
            passkey, to_string(pairing_data.state).c_str());
        return HCIStatusCode::UNKNOWN;
    }
}

HCIStatusCode BTDevice::setPairingPasskeyNegative() noexcept {
    const std::unique_lock<std::mutex> lock(mtx_pairing); // RAII-style acquire and relinquish via destructor
    jau::sc_atomic_critical sync(sync_data);

    if( isSMPPairingAllowingInput(pairing_data.state, SMPPairingState::PASSKEY_EXPECTED) ) {
#if USE_LINUX_BT_SECURITY
        BTManager& mngr = adapter.getManager();
        MgmtStatus res = mngr.userPasskeyNegativeReply(adapter.dev_id, addressAndType);
        DBG_PRINT("BTDevice:mgmt:SMP: PASSKEY NEGATIVE, state %s, result %s",
            to_string(pairing_data.state).c_str(), to_string(res).c_str());
        return HCIStatusCode::SUCCESS;
#elif SMP_SUPPORTED_BY_OS
    return HCIStatusCode::NOT_SUPPORTED;
#else
    return HCIStatusCode::NOT_SUPPORTED;
#endif
    } else {
        ERR_PRINT("BTDevice:mgmt:SMP: PASSKEY NEGATIVE, state %s, SKIPPED (wrong state)",
            to_string(pairing_data.state).c_str());
        return HCIStatusCode::UNKNOWN;
    }
}

HCIStatusCode BTDevice::setPairingNumericComparison(const bool positive) noexcept {
    const std::unique_lock<std::mutex> lock(mtx_pairing); // RAII-style acquire and relinquish via destructor
    jau::sc_atomic_critical sync(sync_data);

    if( isSMPPairingAllowingInput(pairing_data.state, SMPPairingState::NUMERIC_COMPARE_EXPECTED) ) {
#if USE_LINUX_BT_SECURITY
        BTManager& mngr = adapter.getManager();
        MgmtStatus res = mngr.userConfirmReply(adapter.dev_id, addressAndType, positive);
        DBG_PRINT("BTDevice:mgmt:SMP: CONFIRM '%d', state %s, result %s",
            positive, to_string(pairing_data.state).c_str(), to_string(res).c_str());
        return HCIStatusCode::SUCCESS;
#elif SMP_SUPPORTED_BY_OS
    return HCIStatusCode::NOT_SUPPORTED;
#else
    return HCIStatusCode::NOT_SUPPORTED;
#endif
    } else {
        ERR_PRINT("BTDevice:mgmt:SMP: CONFIRM '%d', state %s, SKIPPED (wrong state)",
            positive, to_string(pairing_data.state).c_str());
        return HCIStatusCode::UNKNOWN;
    }
}

PairingMode BTDevice::getPairingMode() const noexcept {
    jau::sc_atomic_critical sync(sync_data);
    return pairing_data.mode;
}

SMPPairingState BTDevice::getPairingState() const noexcept {
    jau::sc_atomic_critical sync(sync_data);
    return pairing_data.state;
}

void BTDevice::clearSMPStates(const bool connected) noexcept {
    const std::unique_lock<std::mutex> lock(mtx_pairing); // RAII-style acquire and relinquish via destructor
    jau::sc_atomic_critical sync(sync_data);

    DBG_PRINT("BTDevice::clearSMPStates: Had: %s", toString().c_str());

    if( !connected ) {
        // needs to survive connected, or will be set right @ connected
        pairing_data.ioCap_user = SMPIOCapability::UNSET;
        pairing_data.ioCap_conn = SMPIOCapability::UNSET;
        pairing_data.sec_level_user = BTSecurityLevel::UNSET;
        // Keep alive: pairing_data.ioCap_auto = SMPIOCapability::UNSET;
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
    pairing_data.keys_resp_exp = SMPKeyType::NONE;
    pairing_data.keys_resp_has = SMPKeyType::NONE;
    pairing_data.ltk_resp.clear();
    pairing_data.irk_resp.clear();
    // pairing_data.address;
    // pairing_data.is_static_random_address;
    pairing_data.csrk_resp.clear();

    pairing_data.authReqs_init = SMPAuthReqs::NONE;
    pairing_data.ioCap_init    = SMPIOCapability::NO_INPUT_NO_OUTPUT;
    pairing_data.oobFlag_init  = SMPOOBDataFlag::OOB_AUTH_DATA_NOT_PRESENT;
    pairing_data.maxEncsz_init = 0;
    pairing_data.keys_init_exp = SMPKeyType::NONE;
    pairing_data.keys_init_has = SMPKeyType::NONE;
    pairing_data.ltk_init.clear();
    pairing_data.irk_init.clear();
    pairing_data.csrk_init.clear();
}

void BTDevice::disconnectSMP(const int caller) noexcept {
  #if SMP_SUPPORTED_BY_OS
    const std::lock_guard<std::recursive_mutex> lock_conn(mtx_smpHandler);
    if( nullptr != smpHandler ) {
        DBG_PRINT("BTDevice::disconnectSMP: start (has smpHandler, caller %d)", caller);
        smpHandler->disconnect(false /* disconnectDevice */, false /* ioErrorCause */);
    } else {
        DBG_PRINT("BTDevice::disconnectSMP: start (nil smpHandler, caller %d)", caller);
    }
    smpHandler = nullptr;
    DBG_PRINT("BTDevice::disconnectSMP: end");
  #else
    (void)caller;
  #endif
}

bool BTDevice::connectSMP(std::shared_ptr<BTDevice> sthis, const BTSecurityLevel sec_level) noexcept {
  #if SMP_SUPPORTED_BY_OS
    if( !isConnected || !allowDisconnect) {
        ERR_PRINT("BTDevice::connectSMP(%u): Device not connected: %s", sec_level, toString().c_str());
        return false;
    }

    if( !SMPHandler::IS_SUPPORTED_BY_OS ) {
        DBG_PRINT("BTDevice::connectSMP(%u): SMP Not supported by OS (1): %s", sec_level, toString().c_str());
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

    smpHandler = std::make_shared<SMPHandler>(sthis);
    if( !smpHandler->isConnected() ) {
        ERR_PRINT("BTDevice::connectSMP: Connection failed");
        smpHandler = nullptr;
        return false;
    }
    return smpHandler->establishSecurity(sec_level);
  #else
    DBG_PRINT("BTDevice::connectSMP: SMP Not supported by OS (0): %s", toString().c_str());
    (void)sthis;
    (void)sec_level;
    return false;
  #endif
}

void BTDevice::disconnectGATT(const int caller) noexcept {
    const std::lock_guard<std::recursive_mutex> lock_conn(mtx_gattHandler);
    if( nullptr != gattHandler ) {
        DBG_PRINT("BTDevice::disconnectGATT: start (has gattHandler, caller %d)", caller);
        gattHandler->disconnect(false /* disconnectDevice */, false /* ioErrorCause */);
    } else {
        DBG_PRINT("BTDevice::disconnectGATT: start (nil gattHandler, caller %d)", caller);
    }
    gattHandler = nullptr;
    DBG_PRINT("BTDevice::disconnectGATT: end");
}

bool BTDevice::connectGATT(std::shared_ptr<BTDevice> sthis) noexcept {
    if( !isConnected || !allowDisconnect) {
        ERR_PRINT("BTDevice::connectGATT: Device not connected: %s", toString().c_str());
        return false;
    }
    if( !l2cap_att.isOpen() ) {
        ERR_PRINT("BTDevice::connectGATT: L2CAP not open: %s", toString().c_str());
        return false;
    }

    const std::lock_guard<std::recursive_mutex> lock_conn(mtx_gattHandler);
    if( nullptr != gattHandler ) {
        if( gattHandler->isConnected() ) {
            return true;
        }
        gattHandler = nullptr;
    }

    gattHandler = std::make_shared<BTGattHandler>(sthis, l2cap_att);
    if( !gattHandler->isConnected() ) {
        ERR_PRINT2("BTDevice::connectGATT: Connection failed");
        gattHandler = nullptr;
        return false;
    }
    return true;
}

std::shared_ptr<BTGattHandler> BTDevice::getGattHandler() noexcept {
    const std::lock_guard<std::recursive_mutex> lock_conn(mtx_gattHandler);
    return gattHandler;
}

jau::darray<std::shared_ptr<BTGattService>> BTDevice::getGattServices() noexcept {
    std::shared_ptr<BTGattHandler> gh = getGattHandler();
    if( nullptr == gh ) {
        ERR_PRINT("BTDevice::getGATTServices: GATTHandler nullptr");
        return jau::darray<std::shared_ptr<BTGattService>>();
    }
    jau::darray<std::shared_ptr<BTGattService>> gattServices = gh->getServices();
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
            DBG_PRINT("BTDevice::getGATTServices: updated %s:\n    %s\n    -> %s",
                to_string(updateMask).c_str(), gattGenericAccess->toString().c_str(), toString().c_str());
            if( EIRDataType::NONE != updateMask ) {
                std::shared_ptr<BTDevice> sharedInstance = getSharedInstance();
                if( nullptr == sharedInstance ) {
                    ERR_PRINT("BTDevice::getGATTServices: Device unknown to adapter and not tracked: %s", toString().c_str());
                } else {
                    adapter.sendDeviceUpdated("getGATTServices", sharedInstance, ts, updateMask);
                }
            }
        }
    } catch (std::exception &e) {
        WARN_PRINT("BTDevice::getGATTServices: Caught exception: '%s' on %s", e.what(), toString().c_str());
    }
    return gattServices;
}

std::shared_ptr<BTGattService> BTDevice::findGattService(std::shared_ptr<uuid_t> const &uuid) {
    const jau::darray<std::shared_ptr<BTGattService>> & gattServices = getGattServices(); // reference of the GATTHandler's list
    const size_t size = gattServices.size();
    for (size_t i = 0; i < size; i++) {
        const std::shared_ptr<BTGattService> & e = gattServices[i];
        if ( nullptr != e && *uuid == *(e->type) ) {
            return e;
        }
    }
    return nullptr;
}

bool BTDevice::pingGATT() noexcept {
    std::shared_ptr<BTGattHandler> gh = getGattHandler();
    if( nullptr == gh || !gh->isConnected() ) {
        jau::INFO_PRINT("BTDevice::pingGATT: GATTHandler not connected -> disconnected on %s", toString().c_str());
        disconnect(HCIStatusCode::REMOTE_USER_TERMINATED_CONNECTION);
        return false;
    }
    try {
        return gh->ping();
    } catch (std::exception &e) {
        IRQ_PRINT("BTDevice::pingGATT: Potential disconnect, exception: '%s' on %s", e.what(), toString().c_str());
    }
    return false;
}

std::shared_ptr<GattGenericAccessSvc> BTDevice::getGattGenericAccess() {
    std::shared_ptr<BTGattHandler> gh = getGattHandler();
    if( nullptr == gh ) {
        ERR_PRINT("BTDevice::getGATTGenericAccess: GATTHandler nullptr");
        return nullptr;
    }
    return gh->getGenericAccess();
}

bool BTDevice::addCharListener(std::shared_ptr<BTGattCharListener> l) {
    std::shared_ptr<BTGattHandler> gatt = getGattHandler();
    if( nullptr == gatt ) {
        throw jau::IllegalStateException("Device's GATTHandle not connected: "+
                toString(), E_FILE_LINE);
    }
    return gatt->addCharListener(l);
}

bool BTDevice::removeCharListener(std::shared_ptr<BTGattCharListener> l) noexcept {
    std::shared_ptr<BTGattHandler> gatt = getGattHandler();
    if( nullptr == gatt ) {
        // OK to have GATTHandler being shutdown @ disable
        DBG_PRINT("Device's GATTHandle not connected: %s", toString().c_str());
        return false;
    }
    return gatt->removeCharListener(l);
}

int BTDevice::removeAllAssociatedCharListener(std::shared_ptr<BTGattChar> associatedCharacteristic) noexcept {
    std::shared_ptr<BTGattHandler> gatt = getGattHandler();
    if( nullptr == gatt ) {
        // OK to have GATTHandler being shutdown @ disable
        DBG_PRINT("Device's GATTHandle not connected: %s", toString().c_str());
        return false;
    }
    return gatt->removeAllAssociatedCharListener( associatedCharacteristic );
}

int BTDevice::removeAllCharListener() noexcept {
    std::shared_ptr<BTGattHandler> gatt = getGattHandler();
    if( nullptr == gatt ) {
        // OK to have GATTHandler being shutdown @ disable
        DBG_PRINT("Device's GATTHandle not connected: %s", toString().c_str());
        return 0;
    }
    return gatt->removeAllCharListener();
}

void BTDevice::notifyDisconnected() noexcept {
    // coming from disconnect callback, ensure cleaning up!
    DBG_PRINT("BTDevice::notifyDisconnected: handle %s -> zero, %s",
              jau::to_hexstring(hciConnHandle).c_str(), toString().c_str());
    clearSMPStates(false /* connected */);
    allowDisconnect = false;
    isConnected = false;
    hciConnHandle = 0;
    disconnectGATT(1);
    disconnectSMP(1);
    l2cap_att.close();
}

void BTDevice::sendMgmtEvDeviceDisconnected(std::unique_ptr<MgmtEvent> evt) noexcept {
    adapter.mgmtEvDeviceDisconnectedHCI(*evt);
}

HCIStatusCode BTDevice::disconnect(const HCIStatusCode reason) noexcept {
    // Avoid disconnect re-entry lock-free
    bool expConn = true; // C++11, exp as value since C++20
    if( !allowDisconnect.compare_exchange_strong(expConn, false) ) {
        // Not connected or disconnect already in process.
        DBG_PRINT("BTDevice::disconnect: Not connected: isConnected %d/%d, reason 0x%X (%s), gattHandler %d, hciConnHandle %s",
                allowDisconnect.load(), isConnected.load(),
                static_cast<uint8_t>(reason), to_string(reason).c_str(),
                (nullptr != gattHandler), jau::to_hexstring(hciConnHandle).c_str());
        return HCIStatusCode::CONNECTION_TERMINATED_BY_LOCAL_HOST;
    }
    if( !isConnected ) { // should not happen
        WARN_PRINT("BTDevice::disconnect: allowConnect true -> false, but !isConnected on %s", toString().c_str());
        return HCIStatusCode::SUCCESS;
    }

    // Disconnect GATT and SMP before device, keeping reversed initialization order intact if possible.
    // This outside mtx_connect, keeping same mutex lock order intact as well
    disconnectGATT(0);
    disconnectSMP(0);

    // Lock to avoid other threads connecting while disconnecting
    const std::lock_guard<std::recursive_mutex> lock_conn(mtx_connect); // RAII-style acquire and relinquish via destructor

    WORDY_PRINT("BTDevice::disconnect: Start: isConnected %d/%d, reason 0x%X (%s), gattHandler %d, hciConnHandle %s",
            allowDisconnect.load(), isConnected.load(),
            static_cast<uint8_t>(reason), to_string(reason).c_str(),
            (nullptr != gattHandler), jau::to_hexstring(hciConnHandle).c_str());

    HCIHandler &hci = adapter.getHCI();
    HCIStatusCode res = HCIStatusCode::SUCCESS;

    if( 0 == hciConnHandle ) {
        res = HCIStatusCode::UNSPECIFIED_ERROR;
        goto exit;
    }

    if( !adapter.isPowered() ) {
        WARN_PRINT("BTDevice::disconnect: Adapter not powered: %s", toString().c_str());
        res = HCIStatusCode::UNSPECIFIED_ERROR; // powered-off
        goto exit;
    }

    res = hci.disconnect(hciConnHandle.load(), addressAndType, reason);
    if( HCIStatusCode::SUCCESS != res ) {
        ERR_PRINT("BTDevice::disconnect: status %s, handle 0x%X, isConnected %d/%d: errno %d %s on %s",
                to_string(res).c_str(), hciConnHandle.load(),
                allowDisconnect.load(), isConnected.load(),
                errno, strerror(errno),
                toString().c_str());
    }

exit:
    if( HCIStatusCode::SUCCESS != res ) {
        // In case of an already pulled or disconnected HCIHandler (e.g. power-off)
        // or in case the hci->disconnect() itself fails,
        // send the DISCONN_COMPLETE event directly.
        // SEND_EVENT: Perform off-thread to avoid potential deadlock w/ application callbacks (similar when sent from HCIHandler's reader-thread)
        std::thread bg(&BTDevice::sendMgmtEvDeviceDisconnected, this, // @suppress("Invalid arguments")
                       std::make_unique<MgmtEvtDeviceDisconnected>(adapter.dev_id, addressAndType, reason, hciConnHandle.load()) );
        bg.detach();
        // adapter.mgmtEvDeviceDisconnectedHCI( std::unique_ptr<MgmtEvent>( new MgmtEvtDeviceDisconnected(adapter.dev_id, address, addressType, reason, hciConnHandle.load()) ) );
    }
    WORDY_PRINT("BTDevice::disconnect: End: status %s, handle 0x%X, isConnected %d/%d on %s",
            to_string(res).c_str(),
            hciConnHandle.load(), allowDisconnect.load(), isConnected.load(),
            toString().c_str());

    return res;
}

HCIStatusCode BTDevice::unpair() noexcept {
#if USE_LINUX_BT_SECURITY
    const MgmtStatus res = adapter.getManager().unpairDevice(adapter.dev_id, addressAndType, false /* disconnect */);
    clearSMPStates(false /* connected */);
    return to_HCIStatusCode(res);
#elif SMP_SUPPORTED_BY_OS
    return HCIStatusCode::NOT_SUPPORTED;
#else
    return HCIStatusCode::NOT_SUPPORTED;
#endif
}

void BTDevice::remove() noexcept {
    clearSMPStates(false /* connected */);
    adapter.removeDevice(*this);
}
