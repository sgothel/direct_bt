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
#include "BTGattService.hpp"

using namespace direct_bt;
using namespace jau::fractions_i64_literals;

BTDevice::BTDevice(const ctor_cookie& cc, BTAdapter & a, EInfoReport const & r)
: adapter(a), btRole(!a.getRole()),
  l2cap_att( std::make_unique<L2CAPClient>(adapter.dev_id, adapter.getAddressAndType(), L2CAP_PSM::UNDEFINED, L2CAP_CID::ATT) ), // copy elision, not copy-ctor
  ts_last_discovery(r.getTimestamp()),
  ts_last_update(ts_last_discovery),
  name(),
  eir( std::make_shared<EInfoReport>() ),
  eir_ind( std::make_shared<EInfoReport>() ),
  eir_scan_rsp( std::make_shared<EInfoReport>() ),
  hciConnHandle(0),
  le_features(LE_Features::NONE),
  le_phy_tx(LE_PHYs::NONE),
  le_phy_rx(LE_PHYs::NONE),
  isConnected(false),
  allowDisconnect(false),
  supervision_timeout(0),
  smp_events(0),
  pairing_data { },
  ts_creation(ts_last_discovery),
  visibleAddressAndType{r.getAddress(), r.getAddressType()},
  addressAndType(visibleAddressAndType)
{
    (void)cc;

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
            // NOLINTNEXTLINE(clang-analyzer-optin.cplusplus.VirtualCall)
            throw jau::IllegalArgumentException("BDADDR_LE_RANDOM: Invalid BLERandomAddressType "+to_string(leRandomAddressType)+": "+toString(), E_FILE_LINE);
        }
    } else {
        if( BLERandomAddressType::UNDEFINED != leRandomAddressType ) {
            // NOLINTNEXTLINE(clang-analyzer-optin.cplusplus.VirtualCall)
            throw jau::IllegalArgumentException("Not BDADDR_LE_RANDOM: Invalid given native BLERandomAddressType "+to_string(leRandomAddressType)+": "+toString(), E_FILE_LINE);
        }
    }
}

BTDevice::~BTDevice() noexcept {
    DBG_PRINT("BTDevice::dtor: ... %p %s", this, addressAndType.toString().c_str());
    adapter.removeAllStatusListener(*this);
    DBG_PRINT("BTDevice::dtor: XXX %p %s", this, addressAndType.toString().c_str());
}

std::shared_ptr<BTDevice> BTDevice::getSharedInstance() const noexcept {
    return adapter.getSharedDevice(*this);
}

GATTRole BTDevice::getLocalGATTRole() const noexcept {
    switch( btRole ) {
        case BTRole::Master: return GATTRole::Server;
        case BTRole::Slave: return GATTRole::Client;
        default: return GATTRole::None;
    }
}

std::string const BTDevice::getName() const noexcept {
    jau::sc_atomic_critical sync(sync_data);
    std::string res = name;
    return res;
}

EInfoReportRef BTDevice::getEIR() noexcept {
    const std::lock_guard<std::mutex> lock(mtx_eir); // RAII-style acquire and relinquish via destructor
    return eir;
}

EInfoReportRef BTDevice::getEIRInd() noexcept {
    const std::lock_guard<std::mutex> lock(mtx_eir); // RAII-style acquire and relinquish via destructor
    return eir_ind;
}

EInfoReportRef BTDevice::getEIRScanRsp() noexcept {
    const std::lock_guard<std::mutex> lock(mtx_eir); // RAII-style acquire and relinquish via destructor
    return eir_scan_rsp;
}

std::string BTDevice::toString(bool includeDiscoveredServices) const noexcept {
    const uint64_t t0 = jau::getCurrentMilliseconds();
    jau::sc_atomic_critical sync(sync_data);
    std::string resaddr_s = visibleAddressAndType != addressAndType ? ", visible "+visibleAddressAndType.toString() : "";
    std::shared_ptr<const EInfoReport> eir_ = eir;
    std::string eir_s = BTRole::Slave == getRole() ? ", "+eir_->toString( includeDiscoveredServices ) : "";
    std::string out("Device["+to_string(getRole())+", "+addressAndType.toString()+resaddr_s+", name['"+name+
            "'], age[total "+std::to_string(t0-ts_creation)+", ldisc "+std::to_string(t0-ts_last_discovery)+", lup "+std::to_string(t0-ts_last_update)+
            "]ms, connected["+std::to_string(allowDisconnect)+"/"+std::to_string(isConnected)+", handle "+jau::to_hexstring(hciConnHandle)+
            ", phy[Tx "+direct_bt::to_string(le_phy_tx)+", Rx "+direct_bt::to_string(le_phy_rx)+
            "], sec[enc "+std::to_string(pairing_data.encryption_enabled)+", lvl "+to_string(pairing_data.sec_level_conn)+", io "+to_string(pairing_data.io_cap_conn)+
            ", auto "+to_string(pairing_data.io_cap_auto)+", pairing "+to_string(pairing_data.mode)+", state "+to_string(pairing_data.state)+
            ", sc "+std::to_string(pairing_data.use_sc)+"]], rssi "+std::to_string(getRSSI())+
            ", tx-power "+std::to_string(tx_power)+eir_s+
            ", "+javaObjectToString()+"]");
    return out;
}

void BTDevice::clearData() noexcept {
    // already done == performed on previous notidyDisconnected() !
    if( getConnected() ) {
        ERR_PRINT("Device still connected: %s", toString().c_str());
        return;
    }
    btRole = !adapter.getRole(); // update role
    // l2cap_att->close(); // already done
    // ts_last_discovery = 0; // leave
    // ts_last_update = 0; // leave
    name.clear();
    rssi = 127; // The core spec defines 127 as the "not available" value
    tx_power = 127; // The core spec defines 127 as the "not available" value
    {
        const std::lock_guard<std::mutex> lock(mtx_eir); // RAII-style acquire and relinquish via destructor
        eir = std::make_shared<EInfoReport>();
        eir_ind = std::make_shared<EInfoReport>();
        eir_scan_rsp = std::make_shared<EInfoReport>();
    }
    // hciConnHandle = 0; // already done
    le_features = LE_Features::NONE;
    le_phy_tx = LE_PHYs::NONE;
    le_phy_rx = LE_PHYs::NONE;
    // isConnected = false; // already done
    // allowDisconnect = false; // already done
    // supervision_timeout = 0; // already done
    // clearSMPStates( false  /* connected */); // already done
}

bool BTDevice::updateIdentityAddress(BDAddressAndType const & identityAddress, bool sendEvent) noexcept {
    if( BDAddressType::BDADDR_LE_PUBLIC != addressAndType.type && addressAndType != identityAddress ) {
        addressAndType  = identityAddress;
        adapter.hci.setResolvHCIConnectionAddr(visibleAddressAndType, addressAndType);
        if( sendEvent ) {
            std::shared_ptr<BTDevice> sharedInstance = getSharedInstance();
            if( nullptr == sharedInstance ) {
                ERR_PRINT("Device unknown to adapter and not tracked: %s", toString().c_str());
            } else {
                adapter.sendDeviceUpdated("Address Resolution", sharedInstance, jau::getCurrentMilliseconds(), EIRDataType::BDADDR | EIRDataType::BDADDR_TYPE);
            }
        }
        return true;
    } else {
        return false;
    }
}

bool BTDevice::updateVisibleAddress(BDAddressAndType const & randomPrivateAddress) noexcept {
    if( visibleAddressAndType != randomPrivateAddress ) {
        visibleAddressAndType = randomPrivateAddress;
        return true;
    } else {
        return false;
    }
}

EIRDataType BTDevice::update(EInfoReport const & data) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_eir); // RAII-style acquire and relinquish via destructor

    btRole = !adapter.getRole(); // update role

    // Update eir CoW style
    std::shared_ptr<EInfoReport> eir_new( std::make_shared<EInfoReport>( *eir ) );
    EIRDataType res0 = eir_new->set(data);
    if( EIRDataType::NONE != res0 ) {
        eir = eir_new;
    }
    if( EInfoReport::Source::AD_IND ==  data.getSource() ) {
        eir_ind = std::make_shared<EInfoReport>( data );
    } else if( EInfoReport::Source::AD_SCAN_RSP ==  data.getSource() ) {
        eir_scan_rsp = std::make_shared<EInfoReport>( data );
    }

    ts_last_update = data.getTimestamp();
    if( data.isSet(EIRDataType::BDADDR) ) {
        if( data.getAddress() != this->addressAndType.address ) {
            // NOLINTNEXTLINE(clang-analyzer-optin.cplusplus.VirtualCall)
            WARN_PRINT("BDADDR update not supported: %s for %s", data.toString().c_str(), this->toString().c_str());
        }
    }
    if( data.isSet(EIRDataType::BDADDR_TYPE) ) {
        if( data.getAddressType() != this->addressAndType.type ) {
            // NOLINTNEXTLINE(clang-analyzer-optin.cplusplus.VirtualCall)
            WARN_PRINT("BDADDR_TYPE update not supported: %s for %s", data.toString().c_str(), this->toString().c_str());
        }
    }
    if( data.isSet(EIRDataType::NAME) ) {
        if( data.getName().length() > 0 ) {
            name = data.getName();
        }
    }
    if( data.isSet(EIRDataType::NAME_SHORT) ) {
        if( 0 == name.length() ) {
            name = data.getShortName();
        }
    }
    if( data.isSet(EIRDataType::RSSI) ) {
        if( rssi != data.getRSSI() ) {
            rssi = data.getRSSI();
        }
    }
    if( data.isSet(EIRDataType::TX_POWER) ) {
        if( tx_power != data.getTxPower() ) {
            tx_power = data.getTxPower();
        }
    }
    return res0;
}

EIRDataType BTDevice::update(GattGenericAccessSvc const &data, const uint64_t timestamp) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_eir); // RAII-style acquire and relinquish via destructor

    // Update eir CoW style
    std::shared_ptr<EInfoReport> eir_new( std::make_shared<EInfoReport>( *eir ) );
    bool mod = false;

    EIRDataType res = EIRDataType::NONE;
    ts_last_update = timestamp;
    if( 0 == name.length() && data.deviceName.length() > 0 ) {
        name = data.deviceName;
        eir_new->setName( name );
        set(res, EIRDataType::NAME);
        mod = true;
    }
    if( eir_new->getAppearance() != data.appearance && AppearanceCat::UNKNOWN != data.appearance) {
        eir_new->setAppearance( data.appearance );
        set(res, EIRDataType::APPEARANCE);
        mod = true;
    }
    if( mod ) {
        eir = eir_new;
    }
    return res;
}

bool BTDevice::addStatusListener(const AdapterStatusListenerRef& l) noexcept {
    return adapter.addStatusListener(*this, l);
}

bool BTDevice::removeStatusListener(const AdapterStatusListenerRef& l) noexcept {
    return adapter.removeStatusListener(l);
}

std::shared_ptr<ConnectionInfo> BTDevice::getConnectionInfo() noexcept {
    const BTManagerRef& mgmt = adapter.getManager();
    std::shared_ptr<ConnectionInfo> connInfo = mgmt->getConnectionInfo(adapter.dev_id, addressAndType);
    if( nullptr != connInfo ) {
        EIRDataType updateMask = EIRDataType::NONE;
        if( rssi != connInfo->getRSSI() ) {
            rssi = connInfo->getRSSI();
            set(updateMask, EIRDataType::RSSI);
        }
        if( tx_power != connInfo->getTxPower() ) {
            tx_power = connInfo->getTxPower();
            set(updateMask, EIRDataType::TX_POWER);
        }
        if( EIRDataType::NONE != updateMask ) {
            std::shared_ptr<BTDevice> sharedInstance = getSharedInstance();
            if( nullptr == sharedInstance ) {
                ERR_PRINT("Device unknown to adapter and not tracked: %s", toString().c_str());
            } else {
                adapter.sendDeviceUpdated("getConnectionInfo", sharedInstance, jau::getCurrentMilliseconds(), updateMask);
            }
        }
    }
    return connInfo;
}

// #define TEST_NOENC 1

HCIStatusCode BTDevice::connectLE(const uint16_t le_scan_interval, const uint16_t le_scan_window,
                                  const uint16_t conn_interval_min, const uint16_t conn_interval_max,
                                  const uint16_t conn_latency, const uint16_t conn_supervision_timeout) noexcept
{
    const std::lock_guard<std::recursive_mutex> lock_conn(mtx_connect); // RAII-style acquire and relinquish via destructor
    if( !adapter.isPowered() ) { // isValid() && hci.isOpen() && POWERED
        WARN_PRINT("Adapter not powered: %s, %s", adapter.toString().c_str(), toString().c_str());
        return HCIStatusCode::NOT_POWERED;
    }
    HCILEOwnAddressType hci_own_mac_type = adapter.visibleMACType;
    HCILEPeerAddressType hci_peer_mac_type;

    switch( addressAndType.type ) {
        case BDAddressType::BDADDR_LE_PUBLIC:
            hci_peer_mac_type = HCILEPeerAddressType::PUBLIC;
            break;
        case BDAddressType::BDADDR_LE_RANDOM: {
            // TODO: Shall we support 'resolving list' and/or LE Set Privacy Mode (HCI) ?
            const BLERandomAddressType leRandomAddressType = addressAndType.getBLERandomAddressType();
            switch( leRandomAddressType ) {
                case BLERandomAddressType::UNRESOLVABLE_PRIVAT:
                    // TODO: OK to not be able to resolve?
                    hci_peer_mac_type = HCILEPeerAddressType::RANDOM;
                    break;
                case BLERandomAddressType::RESOLVABLE_PRIVAT:
                    // TODO: Shall we resolve this address using IRK to set HCILEPeerAddressType::PUBLIC_IDENTITY ?
                    hci_peer_mac_type = HCILEPeerAddressType::RANDOM;
                    break;
                case BLERandomAddressType::STATIC_PUBLIC:
                    // Static random address is not changing between power-cycles.
                    hci_peer_mac_type = HCILEPeerAddressType::RANDOM;
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
        ERR_PRINT("Already connected: %s", toString().c_str());
        return HCIStatusCode::CONNECTION_ALREADY_EXISTS;
    }

    HCIHandler &hci = adapter.getHCI();
    if( !hci.isOpen() ) {
        ERR_PRINT("HCI closed: %s", toString().c_str());
        return HCIStatusCode::INTERNAL_FAILURE;
    }

    HCIStatusCode statusConnect; // HCI le_create_conn result
    const SMPIOCapability smp_auto_io_cap = pairing_data.io_cap_auto; // cache against clearSMPState
    const bool smp_auto = SMPIOCapability::UNSET != smp_auto_io_cap; // logical cached state
    bool smp_auto_done = !smp_auto;
    int smp_auto_count = 0;
    BTSecurityLevel sec_level = pairing_data.sec_level_user; // iterate down
    SMPIOCapability io_cap = pairing_data.io_cap_user; // iterate down
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
            pairing_data.io_cap_auto = smp_auto_io_cap; // reload against clearSMPState
            pairing_data.sec_level_user = sec_level;
            pairing_data.io_cap_user = io_cap;
            DBG_PRINT("BTDevice::connectLE: SEC AUTO.%d.1: lvl %s -> %s, io %s -> %s, %s", smp_auto_count,
                to_string(sec_level_pre).c_str(), to_string(sec_level).c_str(),
                to_string(io_cap_pre).c_str(), to_string(io_cap).c_str(),
                toString().c_str());
        }

        {
            jau::sc_atomic_critical sync(sync_data);
            if( !adapter.lockConnect(*this, true /* wait */, pairing_data.io_cap_user) ) {
                ERR_PRINT("adapter::lockConnect() failed: %s", toString().c_str());
                return HCIStatusCode::INTERNAL_FAILURE;
            }
        }
        statusConnect = hci.le_create_conn(addressAndType.address,
                                    hci_peer_mac_type, hci_own_mac_type,
                                    le_scan_interval, le_scan_window, conn_interval_min, conn_interval_max,
                                    conn_latency, conn_supervision_timeout);
        supervision_timeout = 10 * conn_supervision_timeout; // [ms] = 10 * [ms/10]
        allowDisconnect = true;
        if( HCIStatusCode::COMMAND_DISALLOWED == statusConnect ) {
            WARN_PRINT("Could not yet create connection: status 0x%2.2X (%s), errno %d, hci-atype[peer %s, own %s] %s on %s",
                    static_cast<uint8_t>(statusConnect), to_string(statusConnect).c_str(), errno, strerror(errno),
                    to_string(hci_peer_mac_type).c_str(),
                    to_string(hci_own_mac_type).c_str(),
                    toString().c_str());
            adapter.unlockConnect(*this);
            smp_auto_done = true; // premature end of potential SMP auto-negotiation
        } else if ( HCIStatusCode::SUCCESS != statusConnect ) {
            ERR_PRINT("Could not create connection: status 0x%2.2X (%s), errno %d %s, hci-atype[peer %s, own %s] on %s",
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
                std::unique_lock<std::recursive_mutex> lock_pairing(mtx_pairing); // RAII-style acquire and relinquish via destructor
                const std::chrono::steady_clock::time_point timeout_time = std::chrono::steady_clock::now() + hci.env.HCI_COMMAND_COMPLETE_REPLY_TIMEOUT.to_duration(std::chrono::milliseconds::zero());
                while( !hasSMPPairingFinished( pairing_data.state ) ) {
                    std::cv_status s = cv_pairing_state_changed.wait_until(lock_pairing, timeout_time);
                    DBG_PRINT("BTDevice::connectLE: SEC AUTO.%d.2c Wait for SMPPairing: state %s, %s",
                            smp_auto_count, to_string(pairing_data.state).c_str(), toString().c_str());
                    if( std::cv_status::timeout == s && !hasSMPPairingFinished( pairing_data.state ) ) {
                        // timeout
                        ERR_PRINT("SEC AUTO.%d.X Timeout SMPPairing: Disconnecting %s", smp_auto_count, toString().c_str());
                        smp_auto_done = true;
                        pairing_data.io_cap_auto = SMPIOCapability::UNSET;
                        pairing_timeout = true;
                        break;
                    }
                }
                pstate = pairing_data.state;
                DBG_PRINT("BTDevice::connectLE: SEC AUTO.%d.2d Wait for SMPPairing: state %s, %s",
                        smp_auto_count, to_string(pstate).c_str(), toString().c_str());
            }
            if( pairing_timeout ) {
                pairing_data.io_cap_auto = SMPIOCapability::UNSET;
                disconnect(HCIStatusCode::AUTHENTICATION_FAILURE);
                statusConnect = HCIStatusCode::INTERNAL_TIMEOUT;
                adapter.unlockConnect(*this);
                smp_auto_done = true;
            } else if( SMPPairingState::COMPLETED == pstate ) {
                DBG_PRINT("BTDevice::connectLE: SEC AUTO.%d.X Done: %s", smp_auto_count, toString().c_str());
                smp_auto_done = true;
                (void)smp_auto_done;
                break;
            } else if( SMPPairingState::FAILED == pstate ) {
                if( !smp_auto_done ) { // not last one
                    // disconnect for next smp_auto mode test
                    jau::fraction_i64 td_disconnect = 0_s;
                    DBG_PRINT("BTDevice::connectLE: SEC AUTO.%d.3 Failed SMPPairing -> Disconnect: %s", smp_auto_count, toString().c_str());
                    HCIStatusCode dres = disconnect(HCIStatusCode::AUTHENTICATION_FAILURE);
                    if( HCIStatusCode::SUCCESS == dres ) {
                        while( isConnected && hci.env.HCI_COMMAND_COMPLETE_REPLY_TIMEOUT > td_disconnect ) {
                            jau::sc_atomic_critical sync2(sync_data);
                            td_disconnect += hci.env.HCI_COMMAND_POLL_PERIOD;
                            sleep_for(hci.env.HCI_COMMAND_POLL_PERIOD);
                        }
                    }
                    if( hci.env.HCI_COMMAND_COMPLETE_REPLY_TIMEOUT <= td_disconnect ) {
                        // timeout
                        ERR_PRINT("SEC AUTO.%d.4 Timeout Disconnect td_pairing %" PRIi64 " ms: %s",
                                smp_auto_count, td_disconnect.to_ms(), toString().c_str());
                        pairing_data.io_cap_auto = SMPIOCapability::UNSET;
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
            ERR_PRINT("SEC AUTO.%d.X Failed SMPPairing -> Disconnect: %s", smp_auto_count, toString().c_str());
            pairing_data.io_cap_auto = SMPIOCapability::UNSET;
            disconnect(HCIStatusCode::AUTHENTICATION_FAILURE);
            statusConnect = HCIStatusCode::AUTH_FAILED;
        }
        pairing_data.io_cap_auto = SMPIOCapability::UNSET; // always clear post-auto-action: Allow normal notification.
    }
    return statusConnect;
}

HCIStatusCode BTDevice::connectBREDR(const uint16_t pkt_type, const uint16_t clock_offset, const uint8_t role_switch) noexcept
{
    const std::lock_guard<std::recursive_mutex> lock_conn(mtx_connect); // RAII-style acquire and relinquish via destructor
    if( !adapter.isPowered() ) { // isValid() && hci.isOpen() && POWERED
        WARN_PRINT("Adapter not powered: %s, %s", adapter.toString().c_str(), toString().c_str());
        return HCIStatusCode::NOT_POWERED;
    }

    if( isConnected ) {
        ERR_PRINT("Already connected: %s", toString().c_str());
        return HCIStatusCode::CONNECTION_ALREADY_EXISTS;
    }
    if( !addressAndType.isBREDRAddress() ) {
        ERR_PRINT("Not a BDADDR_BREDR address: %s", toString().c_str());
        return HCIStatusCode::UNACCEPTABLE_CONNECTION_PARAM;
    }

    HCIHandler &hci = adapter.getHCI();
    if( !hci.isOpen() ) {
        ERR_PRINT("HCI closed: %s", toString().c_str());
        return HCIStatusCode::INTERNAL_FAILURE;
    }

    {
        jau::sc_atomic_critical sync(sync_data);
        if( !adapter.lockConnect(*this, true /* wait */, pairing_data.io_cap_user) ) {
            ERR_PRINT("adapter::lockConnect() failed: %s", toString().c_str());
            return HCIStatusCode::INTERNAL_FAILURE;
        }
    }
    HCIStatusCode status = hci.create_conn(addressAndType.address, pkt_type, clock_offset, role_switch);
    allowDisconnect = true;
    if ( HCIStatusCode::SUCCESS != status ) {
        ERR_PRINT("Could not create connection: status 0x%2.2X (%s), errno %d %s on %s",
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
            ERR_PRINT("Not a valid address type: %s", toString().c_str());
            return HCIStatusCode::UNACCEPTABLE_CONNECTION_PARAM;
    }
}

void BTDevice::notifyConnected(const std::shared_ptr<BTDevice>& sthis, const uint16_t handle, const SMPIOCapability io_cap_has) noexcept {
    // coming from connected callback, update states including pairing_data.ioCap_conn
    jau::sc_atomic_critical sync(sync_data);
    DBG_PRINT("BTDevice::notifyConnected: Start: handle %s -> %s, io %s / %s -> %s, %s",
              jau::to_hexstring(hciConnHandle).c_str(), jau::to_hexstring(handle).c_str(),
              to_string(pairing_data.io_cap_conn).c_str(), to_string(io_cap_has).c_str(), to_string(pairing_data.io_cap_user).c_str(),
              toString().c_str());
    allowDisconnect = true;
    isConnected = true;
    hciConnHandle = handle;
    SMPIOCapability io_cap_pre = pairing_data.io_cap_conn;
    if( SMPIOCapability::UNSET == pairing_data.io_cap_conn ) { // Exclusion for smp-auto mode
        pairing_data.io_cap_conn = io_cap_has;
    }
    DBG_PRINT("BTDevice::notifyConnected: End: io_cap %s: %s / %s -> %s, %s",
            to_string(pairing_data.io_cap_user).c_str(),
            to_string(io_cap_pre).c_str(),
            to_string(io_cap_has).c_str(),
            to_string(pairing_data.io_cap_conn).c_str(),
            toString().c_str());
    (void)sthis; // not used yet
}

void BTDevice::notifyLEFeatures(const std::shared_ptr<BTDevice>& sthis, const LE_Features features) noexcept {
    const bool is_local_server = BTRole::Master == btRole; // -> local GattRole::Server
    bool enc_done, using_auth, is_pre_paired;
    getSMPEncStatus(enc_done, using_auth, is_pre_paired);

    DBG_PRINT("BTDevice::notifyLEFeatures: start[local_server %d, enc_done %d, auth %d, pre_paired %d]: %s -> %s, %s",
            is_local_server, enc_done, using_auth, is_pre_paired,
            direct_bt::to_string(le_features).c_str(),
            direct_bt::to_string(features).c_str(),
            toString().c_str());

    le_features = features;
    if( addressAndType.isLEAddress() && ( !l2cap_att->is_open() || is_local_server ) ) {
        std::thread bg(&BTDevice::processL2CAPSetup, this, sthis); // @suppress("Invalid arguments")
        bg.detach();
    }
}

void BTDevice::notifyLEPhyUpdateComplete(const HCIStatusCode status, const LE_PHYs Tx, const LE_PHYs Rx) noexcept {
    DBG_PRINT("BTDevice::notifyLEPhyUpdateComplete: %s: [Tx %s, Rx %s], %s",
            direct_bt::to_string(status).c_str(),
            direct_bt::to_string(Tx).c_str(), direct_bt::to_string(Rx).c_str(), toString().c_str());
    if( HCIStatusCode::SUCCESS == status ) {
        le_phy_tx = Tx;
        le_phy_rx = Rx;
    }
}

void BTDevice::processL2CAPSetup(std::shared_ptr<BTDevice> sthis) { // NOLINT(performance-unnecessary-value-param): Pass-by-value out-of-thread
    bool callProcessDeviceReady = false;
    bool callDisconnect = false;
    bool smp_auto = false;
    const bool is_local_server = BTRole::Master == btRole; // -> local GattRole::Server

    if( addressAndType.isLEAddress() && ( is_local_server || !l2cap_att->is_open() ) ) {
        std::unique_lock<std::recursive_mutex> lock_pairing(mtx_pairing); // RAII-style acquire and relinquish via destructor

        DBG_PRINT("BTDevice::processL2CAPSetup: Start dev_id %u, %s", adapter.dev_id, toString().c_str());

        BTSecurityLevel sec_level;
        SMPIOCapability io_cap;
        validateConnectedSecParam(sec_level, io_cap);

        pairing_data.sec_level_conn = sec_level;
        // pairing_data.io_cap_conn = io_cap; // unchanged

        bool l2cap_open;
        if( is_local_server ) {
            const uint64_t t0 = ( jau::environment::get().debug ) ? jau::getCurrentMilliseconds() : 0;
            std::unique_ptr<L2CAPClient> l2cap_att_new = adapter.get_l2cap_connection(sthis);
            const uint64_t td = ( jau::environment::get().debug ) ? jau::getCurrentMilliseconds() - t0 : 0;
            if( nullptr == l2cap_att_new ) {
                DBG_PRINT("L2CAP-ACCEPT: BTDevice::processL2CAPSetup: dev_id %d, td %" PRIu64 "ms, NULL l2cap_att", adapter.dev_id, td);
                l2cap_open = false;
            } else {
                l2cap_att = std::move(l2cap_att_new);
                DBG_PRINT("L2CAP-ACCEPT: BTDevice::processL2CAPSetup: dev_id %d, td %" PRIu64 "ms, l2cap_att %s", adapter.dev_id, td, l2cap_att->toString().c_str());
                if( BTSecurityLevel::UNSET < sec_level && sec_level < BTSecurityLevel::ENC_AUTH ) { // authentication must be left alone in server mode
                    l2cap_open = l2cap_att->setBTSecurityLevel(sec_level);
                } else {
                    l2cap_open = true;
                }
            }
        } else {
            l2cap_open = l2cap_att->open(*this, sec_level); // initiates hciSMPMsgCallback() if sec_level > BT_SECURITY_LOW
        }
        const bool l2cap_enc = l2cap_open && BTSecurityLevel::NONE < sec_level;

        const bool own_smp = SMP_SUPPORTED_BY_OS ? connectSMP(sthis, sec_level) && BTSecurityLevel::NONE < sec_level : false;

        DBG_PRINT("BTDevice::processL2CAPSetup: dev_id %u, lvl %s, connect[own_smp %d, l2cap[open %d, enc %d]]",
                adapter.dev_id, to_string(sec_level).c_str(), own_smp, l2cap_open, l2cap_enc);

        adapter.unlockConnect(*this);

        if( !l2cap_open ) {
            pairing_data.sec_level_conn = BTSecurityLevel::NONE;
            const SMPIOCapability smp_auto_io_cap = pairing_data.io_cap_auto; // cache against clearSMPState
            smp_auto = SMPIOCapability::UNSET != smp_auto_io_cap; // logical cached state
            if( smp_auto ) {
                pairing_data.mode = PairingMode::NONE;
                pairing_data.state = SMPPairingState::FAILED;
                lock_pairing.unlock(); // unlock mutex before notify_all to avoid pessimistic re-block of notified wait() thread.
                cv_pairing_state_changed.notify_all();
            } else {
                callDisconnect = true;
                disconnect(HCIStatusCode::INTERNAL_FAILURE);
            }
        } else if( !l2cap_enc ) {
            callProcessDeviceReady = true;
            lock_pairing.unlock(); // unlock mutex before notifying and `processDeviceReady()`
            const uint64_t ts = jau::getCurrentMilliseconds();
            adapter.notifyPairingStageDone(sthis, ts);
            processDeviceReady(sthis, ts);
        }
    } else {
        DBG_PRINT("BTDevice::processL2CAPSetup: Skipped (not LE) dev_id %u, %s", adapter.dev_id, toString().c_str());
    }
    DBG_PRINT("BTDevice::processL2CAPSetup: End [dev_id %u, disconnect %d, deviceReady %d, smp_auto %d], %s",
            adapter.dev_id, callDisconnect, callProcessDeviceReady, smp_auto, toString().c_str());
}

void BTDevice::getSMPEncStatus(bool& enc_done, bool& using_auth, bool& is_pre_paired) {
    BTSecurityLevel sec_level;
    PairingMode pmode;
    SMPPairingState pstate;
    {
        jau::sc_atomic_critical sync(sync_data);
        sec_level = pairing_data.sec_level_conn;
        pmode = pairing_data.mode;
        pstate = pairing_data.state;
    }
    is_pre_paired = PairingMode::PRE_PAIRED == pmode;
    enc_done = is_pre_paired || SMPPairingState::COMPLETED == pstate;
    using_auth = BTSecurityLevel::ENC_AUTH <= sec_level;
}

void BTDevice::processDeviceReady(std::shared_ptr<BTDevice> sthis, const uint64_t timestamp) { // NOLINT(performance-unnecessary-value-param): Pass-by-value out-of-thread
    const bool is_local_server = BTRole::Master == btRole; // -> local GattRole::Server
    bool enc_done, using_auth, is_pre_paired;
    getSMPEncStatus(enc_done, using_auth, is_pre_paired);

    DBG_PRINT("BTDevice::processDeviceReady: start[local_server %d, enc_done %d, auth %d, pre_paired %d], %s",
            is_local_server, enc_done, using_auth, is_pre_paired, toString().c_str());

    if( BTRole::Slave == btRole ) { // -> local GattRole::Client
        /**
         * Give remote slave (peripheral, Gatt-Server) 'some time'
         * to complete connection and listening to our Gatt-Client requests.
         *
         * We give the Gatt-Server a slightly longer period
         * after newly paired encryption keys.
         */
        if( enc_done && !is_pre_paired ) {
            // newly paired keys
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
        } else {
            // pre-paired or no encryption
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    HCIStatusCode unpair_res = HCIStatusCode::UNKNOWN;
    const bool gatt_res = connectGATT(sthis);

    if( !gatt_res && enc_done ) {
        // Need to repair as GATT communication failed
        unpair_res = unpair();
    }
    DBG_PRINT("BTDevice::processDeviceReady: done[GATT %d, unpair %s], %s",
            gatt_res, to_string(unpair_res).c_str(), toString().c_str());

    if( gatt_res ) {
        adapter.sendDeviceReady(sthis, timestamp);
    }
}


// Pre-Pair minimum requirement: ENC_KEY and LINK_KEY (if available for SC)!
static const SMPKeyType _key_mask_legacy = SMPKeyType::ENC_KEY | SMPKeyType::ID_KEY | SMPKeyType::SIGN_KEY;
static const SMPKeyType _key_mask_sc     = SMPKeyType::ENC_KEY | SMPKeyType::ID_KEY | SMPKeyType::SIGN_KEY | SMPKeyType::LINK_KEY;

bool BTDevice::checkPairingKeyDistributionComplete() const noexcept {
    bool res = false;

    if( pairing_data.encryption_enabled &&
        SMPPairingState::KEY_DISTRIBUTION == pairing_data.state )
    {
        const SMPKeyType key_mask = pairing_data.use_sc ? _key_mask_sc : _key_mask_legacy;

        if( ( pairing_data.keys_init_has & key_mask ) == ( pairing_data.keys_init_exp & key_mask ) &&
            ( pairing_data.keys_resp_has & key_mask ) == ( pairing_data.keys_resp_exp & key_mask ) ) {
            res = true;
        }
    }
    return res;
}

std::string BTDevice::PairingData::toString(const uint16_t dev_id, const BDAddressAndType& addressAndType, const BTRole& role) const {
    std::string res = "PairingData[dev_id "+std::to_string(dev_id)+", Remote ["+addressAndType.toString()+", role "+to_string(role)+"], \n";
    res.append("  Status: Encrypted "+std::to_string(encryption_enabled)+
                  ", State "+to_string(state)+", Mode "+to_string(mode)+
                  ", Responder-Req "+std::to_string(res_requested_sec)+"\n");
    res.append("  Setup:\n");
    res.append("  - SC "+std::to_string(use_sc)+"\n");
    res.append("  - Pre-Paired "+std::to_string(is_pre_paired)+"\n");
    res.append("  - IOCap conn "+to_string(io_cap_conn)+", user "+to_string(io_cap_user)+", auto "+to_string(io_cap_auto)+"\n");
    res.append("  - Level conn "+to_string(sec_level_conn)+", user "+to_string(sec_level_user)+"\n");
    res.append("  Initiator (master) Set:\n");
    res.append("  - OOB   "+to_string(oobFlag_init)+"\n");
    res.append("  - Auth  "+to_string(authReqs_init)+"\n");
    res.append("  - IOCap "+to_string(ioCap_init)+"\n");
    res.append("  - EncSz "+std::to_string(maxEncsz_init)+"\n");
    res.append("  - Keys  "+to_string(keys_init_has)+" / "+to_string(keys_init_exp)+"\n");
    res.append("    - "+ltk_init.toString()+"\n");
    res.append("    - "+lk_init.toString()+"\n");
    res.append("    - "+irk_init.toString()+"\n");
    res.append("    - "+csrk_init.toString()+"\n");
    res.append("  - IdAdr "+id_address_init.toString()+"\n");
    res.append("  Responder (slave) Set:\n");
    res.append("  - OOB   "+to_string(oobFlag_resp)+"\n");
    res.append("  - Auth  "+to_string(authReqs_resp)+"\n");
    res.append("  - IOCap "+to_string(ioCap_resp)+"\n");
    res.append("  - EncSz "+std::to_string(maxEncsz_resp)+"\n");
    res.append("  - Keys  "+to_string(keys_resp_has)+" / "+to_string(keys_resp_exp)+"\n");
    res.append("    - PassKey "+toPassKeyString(passKey_resp)+"\n");
    res.append("    - "+ltk_resp.toString()+"\n");
    res.append("    - "+lk_resp.toString()+"\n");
    res.append("    - "+irk_resp.toString()+"\n");
    res.append("    - "+csrk_resp.toString()+"\n");
    res.append("  - IdAdr "+id_address_resp.toString()+" ]");
    return res;
}

bool BTDevice::updatePairingState(const std::shared_ptr<BTDevice>& sthis, const MgmtEvent& evt, const HCIStatusCode evtStatus, SMPPairingState claimed_state) noexcept {
    std::unique_lock<std::recursive_mutex> lock_pairing(mtx_pairing); // RAII-style acquire and relinquish via destructor
    const std::string timestamp = jau::to_decstring(jau::environment::getElapsedMillisecond(evt.getTimestamp()), ',', 9);

    if( jau::environment::get().debug ) {
        jau::PLAIN_PRINT(false, "[%s] BTDevice::updatePairingState.0: state %s -> claimed %s, mode %s",
            timestamp.c_str(),
            to_string(pairing_data.state).c_str(), to_string(claimed_state).c_str(), to_string(pairing_data.mode).c_str());
        jau::PLAIN_PRINT(false, "[%s] %s", timestamp.c_str(), evt.toString().c_str());
    }

    const SMPIOCapability iocap = pairing_data.io_cap_conn;
    const MgmtEvent::Opcode mgmtEvtOpcode = evt.getOpcode();
    PairingMode mode = pairing_data.mode;
    bool check_pairing_complete = false;
    bool is_device_ready = false;

    smp_events++;

    // No encryption key will be overwritten by this update method, only initial setting allowed.
    // SMP encryption information always overrules.

    if( SMPPairingState::FAILED == pairing_data.state ) { // no recovery from FAILED via SMP
        claimed_state = SMPPairingState::FAILED;
    }
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
                    DBG_PRINT("BTDevice::updatePairingState.1a: state %s [ignored %s, sending PASSKEY 0 reply], mode %s",
                        to_string(pairing_data.state).c_str(), to_string(claimed_state).c_str(),
                        to_string(pairing_data.mode).c_str());
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
                    DBG_PRINT("BTDevice::updatePairingState.1b: state %s [ignored %s, sending CONFIRM reply], mode %s",
                        to_string(pairing_data.state).c_str(), to_string(claimed_state).c_str(),
                        to_string(pairing_data.mode).c_str());
                    claimed_state = pairing_data.state; // suppress
                    std::thread dc(&BTDevice::setPairingNumericComparison, sthis, true);
                    dc.detach();
                }
                break;
            case SMPPairingState::PASSKEY_NOTIFY:
                if( MgmtEvent::Opcode::PASSKEY_NOTIFY == mgmtEvtOpcode ) {
                    // we must be in slave/responder mode, i.e. peripheral/GATT-server providing auth passkey
                    const MgmtEvtPasskeyNotify& event = *static_cast<const MgmtEvtPasskeyNotify *>(&evt);
                    mode = PairingMode::PASSKEY_ENTRY_ini;
                    pairing_data.passKey_resp = event.getPasskey();
                }
                break;
            case SMPPairingState::OOB_EXPECTED:
                // 2
                mode = PairingMode::OUT_OF_BAND;
                break;
            case SMPPairingState::COMPLETED:
                if( SMPPairingState::FEATURE_EXCHANGE_STARTED > pairing_data.state ) {
                    //
                    // No SMP pairing in process (maybe REQUESTED_BY_RESPONDER at maximum),
                    //
                    if constexpr ( CONSIDER_HCI_CMD_FOR_SMP_STATE ) {
                        if( MgmtEvent::Opcode::HCI_LE_ENABLE_ENC == mgmtEvtOpcode &&
                            HCIStatusCode::SUCCESS == evtStatus )
                        {
                            // 3a
                            // No SMP pairing in process (maybe REQUESTED_BY_RESPONDER at maximum),
                            // i.e. prepairing or already paired, reusing keys and usable connection
                            //
                            // Local BTRole::Master initiator
                            // Encryption key is associated with the remote device having role BTRole::Slave (responder).
                            const MgmtEvtHCILEEnableEncryptionCmd& event = *static_cast<const MgmtEvtHCILEEnableEncryptionCmd *>(&evt);
                            const bool use_auth = BTSecurityLevel::ENC_AUTH <= pairing_data.sec_level_conn;
                            const SMPLongTermKey smp_ltk = event.toSMPLongTermKey(pairing_data.use_sc, use_auth);
                            if( smp_ltk.isValid() ) {
                                if( pairing_data.use_sc ) { // in !SC mode, SMP will deliver different keys!
                                    // Secure Connections (SC) use AES sync key for both, initiator and responder.
                                    // true == smp_ltk.isResponder()
                                    if( ( SMPKeyType::ENC_KEY & pairing_data.keys_resp_has ) == SMPKeyType::NONE ) { // no overwrite
                                        pairing_data.ltk_resp = smp_ltk;
                                        pairing_data.keys_resp_has |= SMPKeyType::ENC_KEY;
                                        if( pairing_data.use_sc &&
                                            ( SMPKeyType::ENC_KEY & pairing_data.keys_init_has ) == SMPKeyType::NONE ) // no overwrite
                                        {
                                            pairing_data.ltk_init = smp_ltk;
                                            pairing_data.ltk_init.properties &= ~SMPLongTermKey::Property::RESPONDER; // enforce for SC
                                            pairing_data.keys_init_has |= SMPKeyType::ENC_KEY;
                                        }
                                    }
                                }
                                mode = PairingMode::PRE_PAIRED;
                                // Waiting for HCI_ENC_CHANGED or HCI_ENC_KEY_REFRESH_COMPLETE
                            }
                            claimed_state = pairing_data.state; // not yet
                            break; // case SMPPairingState::COMPLETED:
                        }
                    }
                    if( MgmtEvent::Opcode::HCI_ENC_CHANGED == mgmtEvtOpcode &&
                        HCIStatusCode::SUCCESS == evtStatus )
                    {
                        // 3b
                        // No SMP pairing in process (maybe REQUESTED_BY_RESPONDER at maximum),
                        // i.e. prepairing or already paired, reusing keys and usable connection
                        mode = PairingMode::PRE_PAIRED;
                        pairing_data.encryption_enabled = true;
                        is_device_ready = true;
                    } else if( MgmtEvent::Opcode::PAIR_DEVICE_COMPLETE == mgmtEvtOpcode &&
                               HCIStatusCode::ALREADY_PAIRED == evtStatus )
                    {
                        // 3c
                        // No SMP pairing in process (maybe REQUESTED_BY_RESPONDER at maximum),
                        // i.e. already paired, reusing keys and usable connection
                        mode = PairingMode::PRE_PAIRED;
                        pairing_data.encryption_enabled = true;
                        is_device_ready = true;
                    }
                } else if( HCIStatusCode::SUCCESS == evtStatus &&
                           SMPPairingState::KEY_DISTRIBUTION == pairing_data.state )
                {
                    //
                    // SMPPairingState::KEY_DISTRIBUTION
                    //
                    if constexpr ( CONSIDER_HCI_CMD_FOR_SMP_STATE ) {
                        if( MgmtEvent::Opcode::HCI_LE_ENABLE_ENC == mgmtEvtOpcode ) {
                            // 4b
                            // Local BTRole::Master initiator
                            // Encryption key is associated with the remote device having role BTRole::Slave (responder).
                            const MgmtEvtHCILEEnableEncryptionCmd& event = *static_cast<const MgmtEvtHCILEEnableEncryptionCmd *>(&evt);
                            const bool use_auth = BTSecurityLevel::ENC_AUTH <= pairing_data.sec_level_conn;
                            const SMPLongTermKey smp_ltk = event.toSMPLongTermKey(pairing_data.use_sc, use_auth);
                            if( smp_ltk.isValid() ) {
                                if( pairing_data.use_sc ) { // in !SC mode, SMP will deliver different keys!
                                    // Secure Connections (SC) use AES sync key for both, initiator and responder.
                                    // true == smp_ltk.isResponder()
                                    if( ( SMPKeyType::ENC_KEY & pairing_data.keys_resp_has ) == SMPKeyType::NONE ) { // no overwrite
                                        pairing_data.ltk_resp = smp_ltk;
                                        pairing_data.keys_resp_has |= SMPKeyType::ENC_KEY;
                                        if( pairing_data.use_sc &&
                                            ( SMPKeyType::ENC_KEY & pairing_data.keys_init_has ) == SMPKeyType::NONE ) // no overwrite
                                        {
                                            pairing_data.ltk_init = smp_ltk;
                                            pairing_data.ltk_init.properties &= ~SMPLongTermKey::Property::RESPONDER; // enforce for SC
                                            pairing_data.keys_init_has |= SMPKeyType::ENC_KEY;
                                        }
                                    }
                                }
                                // Waiting for HCI_ENC_CHANGED or HCI_ENC_KEY_REFRESH_COMPLETE
                            }
                            claimed_state = pairing_data.state; // not yet
                            break; // case SMPPairingState::COMPLETED:
                        } else if( MgmtEvent::Opcode::HCI_LE_LTK_REQUEST == mgmtEvtOpcode ) {
                            // 4c
                            // Local BTRole::Slave responder
                            const MgmtEvtHCILELTKReq& event = *static_cast<const MgmtEvtHCILELTKReq *>(&evt);
                            const bool use_auth = BTSecurityLevel::ENC_AUTH <= pairing_data.sec_level_conn;
                            const SMPLongTermKey smp_ltk = event.toSMPLongTermKey(pairing_data.use_sc, use_auth);
                            { // if( smp_ltk.isValid() ) // not yet valid
                                if( pairing_data.use_sc ) { // in !SC mode, SMP will deliver different keys!
                                    // true == smp_ltk.isResponder()
                                    if( ( SMPKeyType::ENC_KEY & pairing_data.keys_init_has ) == SMPKeyType::NONE ) { // no overwrite
                                        pairing_data.ltk_resp = smp_ltk;
                                        // pairing_data.keys_resp_has |= SMPKeyType::ENC_KEY; // not yet -> LE_LTK_REPLY_ACK LTK { LTK }
                                    }
                                }
                                // Waiting for LE_LTK_REPLY_ACK { LTK }
                            }
                            claimed_state = pairing_data.state; // not yet
                            break; // case SMPPairingState::COMPLETED:
                        } else if( MgmtEvent::Opcode::HCI_LE_LTK_REPLY_ACK == mgmtEvtOpcode ) {
                            // 4d
                            // Local BTRole::Slave responder
                            const MgmtEvtHCILELTKReplyAckCmd& event = *static_cast<const MgmtEvtHCILELTKReplyAckCmd *>(&evt);
                            SMPLongTermKey smp_ltk = pairing_data.ltk_resp;
                            smp_ltk.enc_size = 16; // valid now;
                            smp_ltk.ltk = event.getLTK();
                            if( smp_ltk.isValid() ) {
                                if( pairing_data.use_sc ) { // in !SC mode, SMP will deliver different keys!
                                    // Secure Connections (SC) use AES sync key for both, initiator and responder.
                                    // true == smp_ltk.isResponder()
                                    if( ( SMPKeyType::ENC_KEY & pairing_data.keys_resp_has ) == SMPKeyType::NONE ) { // no overwrite
                                        pairing_data.ltk_resp = smp_ltk;
                                        pairing_data.keys_resp_has |= SMPKeyType::ENC_KEY;
                                        if( pairing_data.use_sc &&
                                            ( SMPKeyType::ENC_KEY & pairing_data.keys_init_has ) == SMPKeyType::NONE ) // no overwrite
                                        {
                                            pairing_data.ltk_init = smp_ltk;
                                            pairing_data.ltk_init.properties &= ~SMPLongTermKey::Property::RESPONDER; // enforce for SC
                                            pairing_data.keys_init_has |= SMPKeyType::ENC_KEY;
                                        }
                                        check_pairing_complete = true;
                                    }
                                }
                            }
                            if( !check_pairing_complete ) {
                                claimed_state = pairing_data.state; // invalid smp_ltk or no overwrite
                            }
                            break; // case SMPPairingState::COMPLETED:
                        }
                    }
                    if( MgmtEvent::Opcode::HCI_ENC_CHANGED == mgmtEvtOpcode ||
                        MgmtEvent::Opcode::HCI_ENC_KEY_REFRESH_COMPLETE == mgmtEvtOpcode ) {
                        // 4a
                        pairing_data.encryption_enabled = true;
                        check_pairing_complete = true;
                    } else if( MgmtEvent::Opcode::NEW_LONG_TERM_KEY == mgmtEvtOpcode ) { /* Legacy: 2; SC: 2 (synthetic by mgmt) */
                        // 4e
                        // SMP pairing has started, mngr issued new LTK command
                        const MgmtEvtNewLongTermKey& event = *static_cast<const MgmtEvtNewLongTermKey *>(&evt);
                        const MgmtLongTermKey& mgmt_ltk = event.getLongTermKey();
                        const SMPLongTermKey smp_ltk = mgmt_ltk.toSMPLongTermKey(!btRole);
                        if( smp_ltk.isValid() ) {
                            // Secure Connections (SC) use AES sync key for both, initiator and responder.
                            if( pairing_data.use_sc || smp_ltk.isResponder() ) {
                                if( ( SMPKeyType::ENC_KEY & pairing_data.keys_resp_has ) == SMPKeyType::NONE ) { // no overwrite
                                    pairing_data.ltk_resp = smp_ltk;
                                    pairing_data.ltk_resp.properties |= SMPLongTermKey::Property::RESPONDER; // enforce for SC
                                    pairing_data.keys_resp_has |= SMPKeyType::ENC_KEY;
                                    check_pairing_complete = true;
                                }
                            }
                            if( pairing_data.use_sc || !smp_ltk.isResponder() ) {
                                if( ( SMPKeyType::ENC_KEY & pairing_data.keys_init_has ) == SMPKeyType::NONE ) { // no overwrite
                                    pairing_data.ltk_init = smp_ltk;
                                    pairing_data.ltk_init.properties &= ~SMPLongTermKey::Property::RESPONDER; // enforce for SC
                                    pairing_data.keys_init_has |= SMPKeyType::ENC_KEY;
                                    check_pairing_complete = true;
                                }
                            }
                        }
                        if( !check_pairing_complete ) {
                            claimed_state = pairing_data.state; // invalid smp_ltk or no overwrite
                        }
                    } else if( MgmtEvent::Opcode::NEW_LINK_KEY == mgmtEvtOpcode ) { /* Legacy: N/A; SC: 4 (last value) */
                        // 4f
                        // SMP pairing has started, mngr issued new LinkKey command
                        // Link Key is for this host only!
                        const MgmtEvtNewLinkKey& event = *static_cast<const MgmtEvtNewLinkKey *>(&evt);
                        const MgmtLinkKeyInfo& mgmt_lk = event.getLinkKey();
                        const BTRole hostRole = !btRole;
                        const SMPLinkKey smp_lk = mgmt_lk.toSMPLinkKeyInfo( BTRole::Slave == hostRole /* isResponder */ );
                        if( smp_lk.isValid() ) {
                            if( smp_lk.isResponder() ) {
                                if( ( SMPKeyType::LINK_KEY & pairing_data.keys_resp_has ) == SMPKeyType::NONE ) { // no overwrite
                                    pairing_data.lk_resp = smp_lk;
                                    pairing_data.keys_resp_has |= SMPKeyType::LINK_KEY;
                                    check_pairing_complete = true;
                                }
                                if( smp_lk.isCombiKey() &&
                                    ( SMPKeyType::LINK_KEY & pairing_data.keys_init_has ) == SMPKeyType::NONE ) // no overwrite
                                {
                                    pairing_data.lk_init = smp_lk;
                                    pairing_data.lk_init.responder = false;
                                    pairing_data.keys_init_has |= SMPKeyType::LINK_KEY;
                                }
                            } else {
                                if( ( SMPKeyType::LINK_KEY & pairing_data.keys_init_has ) == SMPKeyType::NONE ) { // no overwrite
                                    pairing_data.lk_init = smp_lk;
                                    pairing_data.keys_init_has |= SMPKeyType::LINK_KEY;
                                    check_pairing_complete = true;
                                }
                                if( smp_lk.isCombiKey() &&
                                    ( SMPKeyType::LINK_KEY & pairing_data.keys_resp_has ) == SMPKeyType::NONE ) // no overwrite
                                {
                                    pairing_data.lk_resp = smp_lk;
                                    pairing_data.lk_resp.responder = true;
                                    pairing_data.keys_resp_has |= SMPKeyType::LINK_KEY;
                                }
                            }
                        }
                        if( !check_pairing_complete ) {
                            claimed_state = pairing_data.state; // invalid smp_lk or no overwrite
                        }
                    } /*  if ( X == mgmtEvtOpcode ) */
                    else {
                        // Ignore: Undesired event or SMP pairing is in process, which needs to be completed.
                        claimed_state = pairing_data.state;
                    }
                } /* if( HCIStatusCode::SUCCESS == evtStatus && SMPPairingState::KEY_DISTRIBUTION == pairing_data.state ) */
                else {
                    // Ignore: Undesired event or SMP pairing is in process, which needs to be completed.
                    claimed_state = pairing_data.state;
                }
                break; // case SMPPairingState::COMPLETED:
            default: // use given state as-is
                break;
        } // switch( claimed_state )

        if( !is_device_ready && check_pairing_complete ) {
            is_device_ready = checkPairingKeyDistributionComplete();
            if( !is_device_ready ) {
                claimed_state = pairing_data.state; // not yet
            }
        }
        if( is_device_ready ) {
            pairing_data.is_pre_paired = true;
            pairing_data.sec_level_conn = l2cap_att->getBTSecurityLevel();
        }
    } // if( pairing_data.state != claimed_state )

    // 5
    if( pairing_data.state != claimed_state ) {
        if( is_device_ready && BTRole::Master == btRole &&
            pairing_data.sec_level_user >= BTSecurityLevel::ENC_ONLY ) {
            // Validate encryption and authentication requirements in server mode!
            if( ( pairing_data.sec_level_user == BTSecurityLevel::ENC_ONLY &&
                  pairing_data.sec_level_conn < BTSecurityLevel::ENC_ONLY ) ||

                ( pairing_data.sec_level_user >= BTSecurityLevel::ENC_AUTH &&
                  pairing_data.sec_level_conn < BTSecurityLevel::ENC_AUTH )
              )
            {
                claimed_state = SMPPairingState::FAILED;
                is_device_ready = false;
                DBG_PRINT("BTDevice:updatePairingState:Sec-Failure: Requested Sec-Level %s > Actual %s",
                        to_string(pairing_data.sec_level_user).c_str(),
                        to_string(pairing_data.sec_level_conn).c_str());
            }
        }

        // 5b
        if( jau::environment::get().debug ) {
            jau::PLAIN_PRINT(false, "[%s] BTDevice::updatePairingState.5b: state %s -> %s, mode %s -> %s, ready %d, checkedPState %d",
                timestamp.c_str(),
                to_string(pairing_data.state).c_str(), to_string(claimed_state).c_str(),
                to_string(pairing_data.mode).c_str(), to_string(mode).c_str(), is_device_ready, check_pairing_complete);
            jau::PLAIN_PRINT(false, "[%s] %s", timestamp.c_str(), evt.toString().c_str());
        }
        pairing_data.mode = mode;
        pairing_data.state = claimed_state;

        if( jau::environment::get().debug ) {
            jau::PLAIN_PRINT(false, "[%s] ", timestamp.c_str());
            jau::PLAIN_PRINT(false, "[%s] BTDevice::updatePairingState.5b: %s", timestamp.c_str(),
                    pairing_data.toString(adapter.dev_id, addressAndType, btRole).c_str());
            jau::PLAIN_PRINT(false, "[%s] ", timestamp.c_str());
        }

        adapter.sendDevicePairingState(sthis, claimed_state, mode, evt.getTimestamp());

        if( is_device_ready ) {
            smp_events = 0;
            adapter.notifyPairingStageDone(sthis, evt.getTimestamp());
            std::thread dc(&BTDevice::processDeviceReady, this, sthis, evt.getTimestamp()); // @suppress("Invalid arguments")
            dc.detach();
        }

        if( jau::environment::get().debug ) {
            jau::PLAIN_PRINT(false, "[%s] BTDevice::updatePairingState.5b: End: state %s",
                    timestamp.c_str(), to_string(pairing_data.state).c_str());
            jau::PLAIN_PRINT(false, "[%s] %s", timestamp.c_str(), toString().c_str());
        }

        lock_pairing.unlock(); // unlock mutex before notify_all to avoid pessimistic re-block of notified wait() thread.
        cv_pairing_state_changed.notify_all();

        return true;
    } else {
        // 5a
        if( jau::environment::get().debug ) {
            jau::PLAIN_PRINT(false, "[%s] BTDevice::updatePairingState.5a: state %s == %s, mode %s -> %s, ready %d, checkedPState %d",
                timestamp.c_str(),
                to_string(pairing_data.state).c_str(), to_string(claimed_state).c_str(),
                to_string(pairing_data.mode).c_str(), to_string(mode).c_str(), is_device_ready, check_pairing_complete);
            jau::PLAIN_PRINT(false, "[%s] %s", timestamp.c_str(), evt.toString().c_str());
        }
        pairing_data.mode = mode;
        if( jau::environment::get().debug ) {
            jau::PLAIN_PRINT(false, "[%s] ", timestamp.c_str());
            jau::PLAIN_PRINT(false, "[%s] BTDevice::updatePairingState.5a: %s", timestamp.c_str(),
                    pairing_data.toString(adapter.dev_id, addressAndType, btRole).c_str());
            jau::PLAIN_PRINT(false, "[%s] ", timestamp.c_str());
            jau::PLAIN_PRINT(false, "[%s] BTDevice::updatePairingState.5a: End: state %s",
                    timestamp.c_str(), to_string(pairing_data.state).c_str());
            jau::PLAIN_PRINT(false, "[%s] %s", timestamp.c_str(), toString().c_str());
        }
    }
    return false;
}

void BTDevice::hciSMPMsgCallback(const std::shared_ptr<BTDevice>& sthis, const SMPPDUMsg& msg, const HCIACLData::l2cap_frame& source) noexcept {
    std::unique_lock<std::recursive_mutex> lock_pairing(mtx_pairing); // RAII-style acquire and relinquish via destructor
    const bool msg_sent = HCIACLData::l2cap_frame::PBFlag::START_NON_AUTOFLUSH_HOST == source.pb_flag; // from from Host to Controller
    const std::string msg_sent_s = msg_sent ? "sent" : "received";
    const BTRole localRole = !btRole; // local adapter role, opposite of device role
    const bool msg_from_initiator = ( msg_sent && BTRole::Master == localRole ) || ( !msg_sent && BTRole::Slave == localRole );
    const std::string msg_dir_s = msg_from_initiator ? "from_initiator_master" : "from_responder_slave";
    const SMPPairingState old_pstate = pairing_data.state;
    const PairingMode old_pmode = pairing_data.mode;
    const std::string timestamp = jau::to_decstring(jau::environment::getElapsedMillisecond(msg.getTimestamp()), ',', 9);

    SMPPairingState pstate = old_pstate;
    PairingMode pmode = old_pmode;
    bool check_pairing_complete = false;
    bool is_device_ready = false;

    smp_events++;

    if( jau::environment::get().debug ) {
        jau::PLAIN_PRINT(false, "[%s] BTDevice:hci:SMP.0: %s: msg %s, local %s, remote %s @ address%s",
            timestamp.c_str(), msg_sent_s.c_str(), msg_dir_s.c_str(), to_string(localRole).c_str(),
            to_string(btRole).c_str(), addressAndType.toString().c_str());
        jau::PLAIN_PRINT(false, "[%s] - %s", timestamp.c_str(), msg.toString().c_str());
        jau::PLAIN_PRINT(false, "[%s] - %s", timestamp.c_str(), source.toString().c_str());
        jau::PLAIN_PRINT(false, "[%s] - %s", timestamp.c_str(), toString().c_str());
    }

    const SMPPDUMsg::Opcode opc = msg.getOpcode();

    // SMP encryption information always overrules and will overwrite.

    switch( opc ) {
        // Phase 1: SMP Negotiation phase

        case SMPPDUMsg::Opcode::SECURITY_REQUEST: // from responder (slave)
            // 1a
            if( SMPPairingState::FAILED >= pstate ) { // ignore otherwise
                pmode = PairingMode::NEGOTIATING;
                pstate = SMPPairingState::REQUESTED_BY_RESPONDER;
                pairing_data.res_requested_sec = true;
            }
            break;

        case SMPPDUMsg::Opcode::PAIRING_REQUEST: // from initiator (master)
            // 1b
            if( msg_from_initiator ) {
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

        case SMPPDUMsg::Opcode::PAIRING_RESPONSE: { // from responder (slave)
            // 1c
            if( !msg_from_initiator ) {
                const SMPPairingMsg & msg1 = *static_cast<const SMPPairingMsg *>( &msg );
                pairing_data.authReqs_resp = msg1.getAuthReqMask();
                pairing_data.ioCap_resp    = msg1.getIOCapability();
                pairing_data.oobFlag_resp  = msg1.getOOBDataFlag();
                pairing_data.maxEncsz_resp = msg1.getMaxEncryptionKeySize();
                pairing_data.keys_init_exp = msg1.getInitKeyDist(); // responding device overrides initiator's request!
                pairing_data.keys_resp_exp = msg1.getRespKeyDist();

                const bool use_sc = is_set( pairing_data.authReqs_init, SMPAuthReqs::SECURE_CONNECTIONS ) &&
                                    is_set( pairing_data.authReqs_resp, SMPAuthReqs::SECURE_CONNECTIONS );
                pairing_data.use_sc = use_sc;

                pmode = ::getPairingMode(use_sc,
                                         pairing_data.authReqs_init, pairing_data.ioCap_init, pairing_data.oobFlag_init,
                                         pairing_data.authReqs_resp, pairing_data.ioCap_resp, pairing_data.oobFlag_resp);

                pstate = SMPPairingState::FEATURE_EXCHANGE_COMPLETED;
            }
        } break;

        // Phase 2: SMP Authentication and Encryption

        case SMPPDUMsg::Opcode::PAIRING_CONFIRM:
            // 2a
            [[fallthrough]];
        case SMPPDUMsg::Opcode::PAIRING_PUBLIC_KEY:
            // 2b
            [[fallthrough]];
        case SMPPDUMsg::Opcode::PAIRING_RANDOM:
            // 2c
            [[fallthrough]];
        case SMPPDUMsg::Opcode::PAIRING_DHKEY_CHECK:
            // 2d
            pmode = old_pmode;
            pstate = SMPPairingState::KEY_DISTRIBUTION;
            break;

        case SMPPDUMsg::Opcode::PAIRING_FAILED: { /* Next: disconnect(..) by user or auto-mode */
            // 2e
            pmode = PairingMode::NONE;
            pstate = SMPPairingState::FAILED;
          } break;

        // Phase 3: SMP Key & Value Distribution phase

        case SMPPDUMsg::Opcode::ENCRYPTION_INFORMATION: {     /* Legacy: 1 */
            // 3a
            // LTK: First part for SMPKeyDistFormat::ENC_KEY, followed by MASTER_IDENTIFICATION (EDIV + RAND)
            const SMPEncInfoMsg & msg1 = *static_cast<const SMPEncInfoMsg *>( &msg );
            if( !msg_from_initiator ) {
                // from responder (LL slave)
                pairing_data.ltk_resp.properties |= SMPLongTermKey::Property::RESPONDER;
                if( BTSecurityLevel::ENC_AUTH <= pairing_data.sec_level_conn ) {
                    pairing_data.ltk_resp.properties |= SMPLongTermKey::Property::AUTH;
                }
                if( pairing_data.use_sc ) {
                    pairing_data.ltk_resp.properties |= SMPLongTermKey::Property::SC;
                }
                pairing_data.ltk_resp.enc_size = pairing_data.maxEncsz_resp;
                pairing_data.ltk_resp.ltk = msg1.getLTK();
            } else {
                // from initiator (LL master)
                // pairing_data.ltk_resp.properties |= SMPLongTermKeyInfo::Property::INITIATOR;
                if( BTSecurityLevel::ENC_AUTH <= pairing_data.sec_level_conn ) {
                    pairing_data.ltk_init.properties |= SMPLongTermKey::Property::AUTH;
                }
                if( pairing_data.use_sc ) {
                    pairing_data.ltk_init.properties |= SMPLongTermKey::Property::SC;
                }
                pairing_data.ltk_init.enc_size = pairing_data.maxEncsz_init;
                pairing_data.ltk_init.ltk = msg1.getLTK();
            }
        }   break;

        case SMPPDUMsg::Opcode::MASTER_IDENTIFICATION: {      /* Legacy: 2 */
            // 3b
            // EDIV + RAND, completing SMPKeyDistFormat::ENC_KEY
            const SMPMasterIdentMsg & msg1 = *static_cast<const SMPMasterIdentMsg *>( &msg );
            if( !msg_from_initiator ) {
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
            check_pairing_complete = true;
        }   break;

        case SMPPDUMsg::Opcode::IDENTITY_INFORMATION: {       /* Legacy: 3; SC: 1 */
            // 3c
            // IRK: SMPKeyDist::ID_KEY, followed by IDENTITY_ADDRESS_INFORMATION
            const SMPIdentInfoMsg & msg1 = *static_cast<const SMPIdentInfoMsg *>( &msg );
            if( !msg_from_initiator ) {
                // from responder (LL slave)
                pairing_data.keys_resp_has |= SMPKeyType::ID_KEY;
                pairing_data.irk_resp.properties |= SMPIdentityResolvingKey::Property::RESPONDER;
                pairing_data.irk_resp.irk = msg1.getIRK();
                if( BDAddressType::BDADDR_UNDEFINED != pairing_data.id_address_resp.type ) {
                    pairing_data.irk_resp.id_address = pairing_data.id_address_resp.address;
                }
            } else {
                // from initiator (LL master)
                pairing_data.keys_init_has |= SMPKeyType::ID_KEY;
                pairing_data.irk_init.irk = msg1.getIRK();
                if( BDAddressType::BDADDR_UNDEFINED != pairing_data.id_address_init.type ) {
                    pairing_data.irk_init.id_address = pairing_data.id_address_init.address;
                }
            }
        }   break;

        case SMPPDUMsg::Opcode::IDENTITY_ADDRESS_INFORMATION:{/* Lecacy: 4; SC: 2 */
            // 3d
            // Public device or static random, complementing IRK SMPKeyDist::ID_KEY
            const SMPIdentAddrInfoMsg& msg1 = *static_cast<const SMPIdentAddrInfoMsg *>( &msg );
            const BDAddressAndType msg_addr(msg1.getAddress(), BDAddressType::BDADDR_LE_PUBLIC);

            // self_is_responder == true: responder's IRK info (LL slave), else the initiator's (LL master)
            const bool self_is_responder = BTRole::Slave == btRole;
            const BDAddressAndType& responderAddress = self_is_responder ? msg_addr : adapter.getAddressAndType(); // msg_addr, b/c its the ID address
            const BDAddressAndType& initiatorAddress = self_is_responder ? adapter.getAddressAndType() : msg_addr; // while addressAndType may still be an rpa

            if( !msg_from_initiator ) {
                // from responder (LL slave)
                // pairing_data.keys_resp_has |= SMPKeyType::ID_KEY;
                if( msg_addr != responderAddress ) {
                    DBG_PRINT("BTDevice:hci:SMP.id: Responder ID Address Mismatch: msg %s != responder %s", msg1.toString().c_str(), responderAddress.toString().c_str());
                } else {
                    pairing_data.id_address_resp = msg_addr;
                    if( is_set(pairing_data.keys_resp_has, SMPKeyType::ID_KEY) ) {
                        pairing_data.irk_resp.id_address = pairing_data.id_address_resp.address;
                    }
                    if( !msg_sent ) { // received
                        updateIdentityAddress(pairing_data.id_address_resp, true /* sendEvent */); // update to resolved static/public address
                    }
                }
            } else {
                // from initiator (LL master)
                // pairing_data.keys_init_has |= SMPKeyType::ID_KEY;
                if( msg_addr != initiatorAddress ) {
                    DBG_PRINT("BTDevice:hci:SMP.id: Initiator ID Address Mismatch: msg %s != initiator %s", msg1.toString().c_str(), initiatorAddress.toString().c_str());
                } else {
                    pairing_data.id_address_init = msg_addr;
                    if( is_set(pairing_data.keys_init_has, SMPKeyType::ID_KEY) ) {
                        pairing_data.irk_init.id_address = pairing_data.id_address_init.address;
                    }
                    if( !msg_sent ) { // received
                        updateIdentityAddress(pairing_data.id_address_init, true /* sendEvent */); // update to resolved static/public address
                    }
                }
            }
        }   break;

        case SMPPDUMsg::Opcode::SIGNING_INFORMATION: {        /* Legacy: 5 (last value); SC: 3 */
            // 3e
            // CSRK
            const SMPSignInfoMsg & msg1 = *static_cast<const SMPSignInfoMsg *>( &msg );
            if( !msg_from_initiator ) {
                // from responder (LL slave)
                pairing_data.keys_resp_has |= SMPKeyType::SIGN_KEY;

                pairing_data.csrk_resp.properties |= SMPSignatureResolvingKey::Property::RESPONDER;
                if( BTSecurityLevel::ENC_AUTH <= pairing_data.sec_level_conn ) {
                    pairing_data.csrk_resp.properties |= SMPSignatureResolvingKey::Property::AUTH;
                }
                pairing_data.csrk_resp.csrk = msg1.getCSRK();
            } else {
                // from initiator (LL master)
                pairing_data.keys_init_has |= SMPKeyType::SIGN_KEY;

                // pairing_data.csrk_init.properties |= SMPSignatureResolvingKeyInfo::Property::INITIATOR;
                if( BTSecurityLevel::ENC_AUTH <= pairing_data.sec_level_conn ) {
                    pairing_data.csrk_init.properties |= SMPSignatureResolvingKey::Property::AUTH;
                }
                pairing_data.csrk_init.csrk = msg1.getCSRK();
            }
            check_pairing_complete = true;
        }   break;

        default:
            break;
    }

    if( SMPPairingState::FAILED == old_pstate ) { // no recovery from FAILED via SMP
        pstate = SMPPairingState::FAILED;
        check_pairing_complete = false;
    }

    // 4
    if( check_pairing_complete && checkPairingKeyDistributionComplete() ) {
        pstate = SMPPairingState::COMPLETED;
        is_device_ready = true;
        pairing_data.is_pre_paired = true;
        pairing_data.sec_level_conn = l2cap_att->getBTSecurityLevel();
    }

    if( jau::environment::get().debug ) {
        if( old_pstate == pstate /* && old_pmode == pmode */ ) {
            jau::PLAIN_PRINT(false, "[%s] BTDevice:hci:SMP.4a: Unchanged: address%s",
                timestamp.c_str(),
                addressAndType.toString().c_str());
        } else {
            jau::PLAIN_PRINT(false, "[%s] BTDevice:hci:SMP.4b:   Updated: address%s",
                timestamp.c_str(),
                addressAndType.toString().c_str());
        }
        jau::PLAIN_PRINT(false, "[%s] - state %s -> %s, mode %s -> %s, ready %d",
            timestamp.c_str(),
            to_string(old_pstate).c_str(), to_string(pstate).c_str(),
            to_string(old_pmode).c_str(), to_string(pmode).c_str(),
            is_device_ready);
    }

    // 5
    if( old_pstate == pstate /* && old_pmode == pmode */ ) {
        // 5a
        if( jau::environment::get().debug ) {
            jau::PLAIN_PRINT(false, "[%s] ", timestamp.c_str());
            jau::PLAIN_PRINT(false, "[%s] BTDevice:hci:SMP.5a: %s", timestamp.c_str(),
                    pairing_data.toString(adapter.dev_id, addressAndType, btRole).c_str());
            jau::PLAIN_PRINT(false, "[%s] ", timestamp.c_str());
        }
        return;
    }

    if( is_device_ready && BTRole::Master == btRole &&
        pairing_data.sec_level_user >= BTSecurityLevel::ENC_ONLY ) {
        // Validate encryption and authentication requirements in server mode!
        if( ( pairing_data.sec_level_user == BTSecurityLevel::ENC_ONLY &&
              pairing_data.sec_level_conn < BTSecurityLevel::ENC_ONLY ) ||

            ( pairing_data.sec_level_user >= BTSecurityLevel::ENC_AUTH &&
              pairing_data.sec_level_conn < BTSecurityLevel::ENC_AUTH )
          )
        {
            pstate = SMPPairingState::FAILED;
            is_device_ready = false;
            DBG_PRINT("BTDevice:hci:SMP:Sec-Failure: Requested Sec-Level %s > Actual %s",
                    to_string(pairing_data.sec_level_user).c_str(),
                    to_string(pairing_data.sec_level_conn).c_str());
        }
    }

    // 5b
    pairing_data.mode = pmode;
    pairing_data.state = pstate;

    if( jau::environment::get().debug ) {
        jau::PLAIN_PRINT(false, "[%s] ", timestamp.c_str());
        jau::PLAIN_PRINT(false, "[%s] BTDevice:hci:SMP.5b: %s", timestamp.c_str(),
                pairing_data.toString(adapter.dev_id, addressAndType, btRole).c_str());
        jau::PLAIN_PRINT(false, "[%s] ", timestamp.c_str());
    }

    adapter.sendDevicePairingState(sthis, pstate, pmode, msg.getTimestamp());

    if( is_device_ready ) {
        smp_events = 0;
        adapter.notifyPairingStageDone(sthis, msg.getTimestamp());
        std::thread dc(&BTDevice::processDeviceReady, this, sthis, msg.getTimestamp()); // @suppress("Invalid arguments")
        dc.detach();
    }

    if( jau::environment::get().debug ) {
        jau::PLAIN_PRINT(false, "[%s] Debug: BTDevice:hci:SMP.5b: End", timestamp.c_str());
        jau::PLAIN_PRINT(false, "[%s] - %s", timestamp.c_str(), toString().c_str());
    }
    lock_pairing.unlock(); // unlock mutex before notify_all to avoid pessimistic re-block of notified wait() thread.
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

bool BTDevice::setSMPKeyBin(const SMPKeyBin& bin) noexcept {
    const std::unique_lock<std::recursive_mutex> lock_pairing(mtx_pairing); // RAII-style acquire and relinquish via destructor

    if( !isValidInstance() ) {
        ERR_PRINT("Device invalid: %p", jau::to_hexstring((void*)this).c_str());
        return false;
    }

    if( bin.getLocalAddrAndType() != getAdapter().getAddressAndType() ) {
         DBG_PRINT("SMPKeyBin::readAndApply: Local address mismatch: Has %s, SMPKeyBin %s: %s",
                    getAdapter().getAddressAndType().toString().c_str(),
                    bin.getLocalAddrAndType().toString().c_str(),
                    bin.toString().c_str());
        return false;
    }
    if( bin.getRemoteAddrAndType() !=  getAddressAndType() ) {
        DBG_PRINT("SMPKeyBin::readAndApply: Remote address mismatch: Has %s, SMPKeyBin %s: %s",
                getAddressAndType().toString().c_str(),
                bin.getRemoteAddrAndType().toString().c_str(),
                bin.toString().c_str());
        return false;
    }

    // Must be a valid SMPKeyBin instance and at least one LTK key if using encryption.
    // Also validates IRKs' id_address, if contained
    if( !bin.isValid() || ( BTSecurityLevel::NONE != bin.getSecLevel() && !bin.hasLTKInit() && !bin.hasLTKResp() ) ) {
         DBG_PRINT("BTDevice::setSMPKeyBin(): Apply SMPKeyBin failed, all invalid or sec level w/o LTK: %s, %s",
                 bin.toString().c_str(), toString().c_str());
        return false;
    }

    const BTRole btRoleAdapter = !btRole;
    if( btRoleAdapter != bin.getLocalRole() ) {
        DBG_PRINT("BTDevice::setSMPKeyBin(): Apply SMPKeyBin failed, local adapter role %s mismatch: %s, %s",
                to_string(btRoleAdapter).c_str(), bin.toString().c_str(), toString().c_str());
       return false;
    }

    {
        if( SMPIOCapability::UNSET != pairing_data.io_cap_auto ||
            ( SMPPairingState::COMPLETED != pairing_data.state &&
              SMPPairingState::NONE != pairing_data.state ) )
        {
            DBG_PRINT("BTDevice::setSMPKeyBin: Failure, pairing in progress: %s, %s",
                    pairing_data.toString(adapter.dev_id, addressAndType, btRole).c_str(), toString().c_str());
            return false;
        }

        if( getConnected() ) {
            DBG_PRINT("BTDevice::setSMPKeyBin: Failure, device connected: %s", toString().c_str());
            return false;
        }

        const BTSecurityLevel applySecLevel = BTSecurityLevel::NONE == bin.getSecLevel() ?
                                              BTSecurityLevel::NONE : BTSecurityLevel::ENC_ONLY;
        if( !setConnSecurity(applySecLevel, SMPIOCapability::NO_INPUT_NO_OUTPUT) ) {
            DBG_PRINT("BTDevice::setSMPKeyBin: Setting security failed: Device Connected/ing: %s, %s", bin.toString().c_str(), toString().c_str());
            return false;
        }
    }
    pairing_data.use_sc = bin.uses_SC();

    if( bin.hasLTKInit() ) {
        setLongTermKey( bin.getLTKInit() );
    }
    if( bin.hasLTKResp() ) {
        setLongTermKey( bin.getLTKResp() );
    }

    if( bin.hasIRKInit() ) {
        setIdentityResolvingKey( bin.getIRKInit() );
    }
    if( bin.hasIRKResp() ) {
        setIdentityResolvingKey( bin.getIRKResp() );
    }

    if( bin.hasCSRKInit() ) {
        setSignatureResolvingKey( bin.getCSRKInit() );
    }
    if( bin.hasCSRKResp() ) {
        setSignatureResolvingKey( bin.getCSRKResp() );
    }

    if( bin.hasLKInit() ) {
        setLinkKey( bin.getLKInit() );
    }
    if( bin.hasLKResp() ) {
        setLinkKey( bin.getLKResp() );
    }
    DBG_PRINT("BTDevice::setSMPKeyBin.OK: %s", pairing_data.toString(adapter.dev_id, addressAndType, btRole).c_str());
    return true;
}

HCIStatusCode BTDevice::uploadKeys() noexcept {
    if( isConnected ) {
        ERR_PRINT("Already connected: %s", toString().c_str());
        return HCIStatusCode::CONNECTION_ALREADY_EXISTS;
    }
    const std::unique_lock<std::recursive_mutex> lock_pairing(mtx_pairing); // RAII-style acquire and relinquish via destructor
    if constexpr ( USE_LINUX_BT_SECURITY ) {
        const BTManagerRef& mngr = adapter.getManager();
        HCIStatusCode res = HCIStatusCode::SUCCESS;

        // LTKs
        {
            jau::darray<SMPLongTermKey> ltks;
            if( is_set(pairing_data.keys_init_has, SMPKeyType::ENC_KEY) ) {
                ltks.push_back(pairing_data.ltk_init);
            }
            if( is_set(pairing_data.keys_resp_has, SMPKeyType::ENC_KEY) ) {
                ltks.push_back(pairing_data.ltk_resp);
            }
            if( ltks.size() > 0 ) {
                res = mngr->uploadLongTermKey(!btRole, adapter.dev_id, addressAndType, ltks);
                DBG_PRINT("BTDevice::uploadKeys.LTK[adapter %s]: %s", to_string(!btRole).c_str(), to_string(res).c_str());
                if( HCIStatusCode::SUCCESS != res ) {
                    return res;
                }
            }
        }

        // IRKs
        {
            jau::darray<SMPIdentityResolvingKey> irks;
            if( is_set(pairing_data.keys_init_has, SMPKeyType::ID_KEY) ) {
                irks.push_back(pairing_data.irk_init);
            }
            if( is_set(pairing_data.keys_resp_has, SMPKeyType::ID_KEY) ) {
                irks.push_back(pairing_data.irk_resp);
            }
            if( irks.size() > 0 ) {
                res = mngr->uploadIdentityResolvingKey(adapter.dev_id, irks);
                DBG_PRINT("BTDevice::uploadKeys.IRK: %s", to_string(res).c_str());
                if( HCIStatusCode::SUCCESS != res ) {
                    return res;
                }
            }
        }

        if( BDAddressType::BDADDR_BREDR != addressAndType.type ) {
            // Not supported
            DBG_PRINT("BTDevice::uploadKeys: Upload LK for LE address not supported -> ignored: %s", toString().c_str());
            pairing_data.is_pre_paired = true;
            return HCIStatusCode::SUCCESS;
        }

        if( BTRole::Slave == btRole ) {
            // Remote device is slave (peripheral, responder), we are master (initiator)
            if( is_set(pairing_data.keys_init_has, SMPKeyType::LINK_KEY) ) {
                res = mngr->uploadLinkKey(adapter.dev_id, addressAndType, pairing_data.lk_init);
                DBG_PRINT("BTDevice::uploadKeys.LK[adapter master]: %s", to_string(res).c_str());
            }
        } else {
            // Remote device is master (initiator), we are slave (peripheral, responder)
            if( is_set(pairing_data.keys_resp_has, SMPKeyType::LINK_KEY) ) {
                res = mngr->uploadLinkKey(adapter.dev_id, addressAndType, pairing_data.lk_resp);
                DBG_PRINT("BTDevice::uploadKeys.LK[adapter slave]: %s", to_string(res).c_str());
            }
        }
        if( HCIStatusCode::SUCCESS == res ) {
            pairing_data.is_pre_paired = true;
        }
        return res;
    } else if constexpr ( SMP_SUPPORTED_BY_OS ) {
        return HCIStatusCode::NOT_SUPPORTED;
    } else {
        return HCIStatusCode::NOT_SUPPORTED;
    }
}

SMPLongTermKey BTDevice::getLongTermKey(const bool responder) const noexcept {
    jau::sc_atomic_critical sync(sync_data);
    return responder ? pairing_data.ltk_resp : pairing_data.ltk_init;
}

void BTDevice::setLongTermKey(const SMPLongTermKey& ltk) noexcept {
    jau::sc_atomic_critical sync(sync_data);
    if( ltk.isResponder() ) {
        pairing_data.ltk_resp = ltk;
        pairing_data.keys_resp_has |= SMPKeyType::ENC_KEY;
        pairing_data.keys_resp_exp |= SMPKeyType::ENC_KEY;
    } else {
        pairing_data.ltk_init = ltk;
        pairing_data.keys_init_has |= SMPKeyType::ENC_KEY;
        pairing_data.keys_init_exp |= SMPKeyType::ENC_KEY;
    }
}

SMPIdentityResolvingKey BTDevice::getIdentityResolvingKey(const bool responder) const noexcept {
    jau::sc_atomic_critical sync(sync_data);
    return responder ? pairing_data.irk_resp : pairing_data.irk_init;
}

void BTDevice::setIdentityResolvingKey(const SMPIdentityResolvingKey& irk) noexcept {
    jau::sc_atomic_critical sync(sync_data);
    if( irk.isResponder() ) {
        pairing_data.irk_resp = irk;
        pairing_data.keys_resp_has |= SMPKeyType::ID_KEY;
        pairing_data.keys_resp_exp |= SMPKeyType::ID_KEY;
        pairing_data.id_address_resp = BDAddressAndType(irk.id_address, BDAddressType::BDADDR_LE_PUBLIC);
    } else {
        pairing_data.irk_init = irk;
        pairing_data.keys_init_has |= SMPKeyType::ID_KEY;
        pairing_data.keys_init_exp |= SMPKeyType::ID_KEY;
        pairing_data.id_address_init = BDAddressAndType(irk.id_address, BDAddressType::BDADDR_LE_PUBLIC);
    }
}

bool BTDevice::matches_irk(BDAddressAndType rpa) noexcept {
    // self_is_responder == true: responder's IRK info (LL slave), else the initiator's (LL master)
    const bool self_is_responder = BTRole::Slave == btRole;
    if( is_set(self_is_responder ? pairing_data.keys_resp_has : pairing_data.keys_init_has, SMPKeyType::ID_KEY) ) {
        SMPIdentityResolvingKey irk = getIdentityResolvingKey(self_is_responder);
        return irk.matches(rpa.address); // irk.id_address == this->addressAndType
    } else {
        return false;
    }
}

SMPSignatureResolvingKey BTDevice::getSignatureResolvingKey(const bool responder) const noexcept {
    jau::sc_atomic_critical sync(sync_data);
    return responder ? pairing_data.csrk_resp : pairing_data.csrk_init;
}

void BTDevice::setSignatureResolvingKey(const SMPSignatureResolvingKey& csrk) noexcept {
    jau::sc_atomic_critical sync(sync_data);
    if( csrk.isResponder() ) {
        pairing_data.csrk_resp = csrk;
        pairing_data.keys_resp_has |= SMPKeyType::SIGN_KEY;
        pairing_data.keys_resp_exp |= SMPKeyType::SIGN_KEY;
    } else {
        pairing_data.csrk_init = csrk;
        pairing_data.keys_init_has |= SMPKeyType::SIGN_KEY;
        pairing_data.keys_init_exp |= SMPKeyType::SIGN_KEY;
    }
}

SMPLinkKey BTDevice::getLinkKey(const bool responder) const noexcept {
    jau::sc_atomic_critical sync(sync_data);
    return responder ? pairing_data.lk_resp : pairing_data.lk_init;
}

void BTDevice::setLinkKey(const SMPLinkKey& lk) noexcept {
    jau::sc_atomic_critical sync(sync_data);
    if( lk.isResponder() ) {
        pairing_data.lk_resp = lk;
        pairing_data.keys_resp_has |= SMPKeyType::LINK_KEY;
        pairing_data.keys_resp_exp |= SMPKeyType::LINK_KEY;
    } else {
        pairing_data.lk_init = lk;
        pairing_data.keys_init_has |= SMPKeyType::LINK_KEY;
        pairing_data.keys_init_exp |= SMPKeyType::LINK_KEY;
    }
}

BTSecurityLevel BTDevice::getConnSecurityLevel() const noexcept {
    jau::sc_atomic_critical sync(sync_data);
    return pairing_data.sec_level_conn;
}

SMPIOCapability BTDevice::getConnIOCapability() const noexcept {
    jau::sc_atomic_critical sync(sync_data);
    return pairing_data.io_cap_conn;
}

void BTDevice::validateConnectedSecParam(BTSecurityLevel& res_sec_level, SMPIOCapability& res_io_cap) const noexcept {
    const BTSecurityLevel sec_level_conn = pairing_data.sec_level_conn;
    const SMPIOCapability io_cap_conn = pairing_data.io_cap_conn;

    // Connection is already established, hence io_cap immutable
    res_io_cap = io_cap_conn;

    if( sec_level_conn != BTSecurityLevel::UNSET ) {
        DBG_PRINT("BTDevice::validateConnectedSecParam: dev_id %u, sec_lvl %s, io_cap %s (preset)", adapter.dev_id,
                to_string(sec_level_conn).c_str(),
                to_string(io_cap_conn).c_str());
        res_sec_level = sec_level_conn;
        return;
    }
    const BTSecurityLevel sec_level_user = pairing_data.sec_level_user;
    const SMPIOCapability io_cap_user = pairing_data.io_cap_user;

    const bool responderLikesEncryption = pairing_data.res_requested_sec || is_set(le_features, LE_Features::LE_Encryption);
    if( BTSecurityLevel::UNSET != sec_level_user ) {
        // Prio set and validated user values even over remote device caps
        res_sec_level = sec_level_user;
    } else if( responderLikesEncryption ) {
        // No preset, but remote likes encryption
        if( hasSMPIOCapabilityAnyIO( io_cap_conn ) ) {
            if( adapter.hasSecureConnections() ) {
                res_sec_level = BTSecurityLevel::ENC_AUTH_FIPS;
            } else {
                res_sec_level = BTSecurityLevel::ENC_AUTH;
            }
        } else {
            res_sec_level = BTSecurityLevel::ENC_ONLY;
        }
    } else {
        res_sec_level = BTSecurityLevel::NONE;
    }

    DBG_PRINT("BTDevice::validateConnectedSecParam: dev_id %u, user[sec_lvl %s, io_cap %s], conn[sec_lvl %s, io_cap %s] -> sec_lvl %s, io_cap %s",
            adapter.dev_id,
            to_string(sec_level_user).c_str(), to_string(io_cap_user).c_str(),
            to_string(sec_level_conn).c_str(), to_string(io_cap_conn).c_str(),
            to_string(res_sec_level).c_str(), to_string(res_io_cap).c_str());
}

void BTDevice::validateSecParam(const BTSecurityLevel sec_level, const SMPIOCapability io_cap,
                                BTSecurityLevel& res_sec_level, SMPIOCapability& res_io_cap) noexcept
{
    if( BTSecurityLevel::UNSET < sec_level ) {
        if( BTSecurityLevel::NONE == sec_level ||
            BTSecurityLevel::ENC_ONLY == sec_level )
        {
            // No authentication, maybe encryption
            res_sec_level = sec_level;
            res_io_cap = SMPIOCapability::NO_INPUT_NO_OUTPUT;
        } else if( hasSMPIOCapabilityAnyIO( io_cap ) ) {
            // Authentication w/ IO
            res_sec_level = sec_level;
            res_io_cap = io_cap;
        } else if( SMPIOCapability::NO_INPUT_NO_OUTPUT == io_cap ) {
            // Fall back: auto -> encryption only
            res_sec_level = BTSecurityLevel::ENC_ONLY;
            res_io_cap = SMPIOCapability::NO_INPUT_NO_OUTPUT;
        } else {
            // Use auth w/ SMPIOCapability::UNSET
            res_sec_level = sec_level;
            res_io_cap = io_cap;
        }
    } else {
        res_sec_level = BTSecurityLevel::UNSET;
        res_io_cap = io_cap;
    }
    DBG_PRINT("BTDevice::validateSecurityParams: lvl %s -> %s, io %s -> %s",
        to_string(sec_level).c_str(), to_string(res_sec_level).c_str(),
        to_string(io_cap).c_str(), to_string(res_io_cap).c_str());
}

bool BTDevice::setConnSecurity(const BTSecurityLevel sec_level, const SMPIOCapability io_cap) noexcept {
    if( !isValidInstance() ) {
        ERR_PRINT("Device invalid: %p", jau::to_hexstring((void*)this).c_str());
        return false;
    }
    if( isConnected || allowDisconnect ) {
        ERR_PRINT("Invalid State: Connected: dev_id %u, lvl %s, io %s failed, %s",
                adapter.dev_id, to_string(sec_level).c_str(),
                to_string(io_cap).c_str(), toString().c_str());
        return false;
    }
    jau::sc_atomic_critical sync(sync_data);

    if( !pairing_data.is_pre_paired ) {
        validateSecParam(sec_level, io_cap, pairing_data.sec_level_user, pairing_data.io_cap_user);
    }
    pairing_data.io_cap_auto = SMPIOCapability::UNSET; // disable auto

    DBG_PRINT("BTDevice::setConnSecurity: dev_id %u, pre-paired %d: lvl %s -> %s, io %s -> %s, %s",
        adapter.dev_id, pairing_data.is_pre_paired,
        to_string(sec_level).c_str(), to_string(pairing_data.sec_level_user).c_str(),
        to_string(io_cap).c_str(), to_string(pairing_data.io_cap_user).c_str(),
        toString().c_str());

    return true;
}

bool BTDevice::setConnSecurityAuto(const SMPIOCapability iocap_auto) noexcept {
    if( !isValidInstance() ) {
        ERR_PRINT("Device invalid: %p", jau::to_hexstring((void*)this).c_str());
        return false;
    }
    if( isConnected || allowDisconnect ) {
        ERR_PRINT("Invalid State: Connected: dev_id %d, io %s failed, %s",
                adapter.dev_id, to_string(iocap_auto).c_str(), toString().c_str());
        return false;
    }
    if( pairing_data.is_pre_paired ) {
        DBG_PRINT("BTDevice::setConnSecurityAuto: io %s failed, is pre-paired: %s",
                to_string(iocap_auto).c_str(),
                toString().c_str());
        return false;
    }
    if( BTSecurityLevel::UNSET != pairing_data.sec_level_user ||
        SMPIOCapability::UNSET != pairing_data.io_cap_user )
    {
        DBG_PRINT("BTDevice::setConnSecurityAuto: io %s failed, user connection sec_level %s or io %s set %s",
                to_string(iocap_auto).c_str(),
                to_string(pairing_data.sec_level_user).c_str(),
                to_string(pairing_data.io_cap_user).c_str(),
                toString().c_str());
        return false;
    }
    if( BTRole::Master == getRole() ) {
        DBG_PRINT("BTDevice::setConnSecurityAuto: Not allowed with remote device in master mode: %s",
                to_string(iocap_auto).c_str(), toString().c_str());
        return false;
    }

    jau::sc_atomic_critical sync(sync_data);
    const bool res = true;
    pairing_data.io_cap_auto = iocap_auto;
    DBG_PRINT("BTDevice::setConnSecurityAuto: result %d: io %s, %s", res,
            to_string(iocap_auto).c_str(), toString().c_str());
    return res;
}

bool BTDevice::isConnSecurityAutoEnabled() const noexcept {
    jau::sc_atomic_critical sync(sync_data);
    return SMPIOCapability::UNSET != pairing_data.io_cap_auto;
}

HCIStatusCode BTDevice::setPairingPINCode(const std::string& pinCode) noexcept {
    const std::unique_lock<std::recursive_mutex> lock_pairing(mtx_pairing); // RAII-style acquire and relinquish via destructor

    if( !isSMPPairingAllowingInput(pairing_data.state, SMPPairingState::PASSKEY_EXPECTED) &&
        !isSMPPairingAllowingInput(pairing_data.state, SMPPairingState::KEY_DISTRIBUTION) )
    {
        WARN_PRINT("BTDevice:mgmt:SMP: PINCODE '%s', state %s, wrong state", pinCode.c_str(), to_string(pairing_data.state).c_str());
    }
    if constexpr ( USE_LINUX_BT_SECURITY ) {
        const BTManagerRef& mngr = adapter.getManager();
        MgmtStatus res = mngr->userPINCodeReply(adapter.dev_id, addressAndType, pinCode);
        DBG_PRINT("BTDevice:mgmt:SMP: PINCODE '%s', state %s, result %s",
            pinCode.c_str(), to_string(pairing_data.state).c_str(), to_string(res).c_str());
        return HCIStatusCode::SUCCESS;
    } else if constexpr ( SMP_SUPPORTED_BY_OS ) {
        return HCIStatusCode::NOT_SUPPORTED;
    } else {
        return HCIStatusCode::NOT_SUPPORTED;
    }
}

HCIStatusCode BTDevice::setPairingPINCodeNegative() noexcept {
    const std::unique_lock<std::recursive_mutex> lock_pairing(mtx_pairing); // RAII-style acquire and relinquish via destructor

    if( !isSMPPairingAllowingInput(pairing_data.state, SMPPairingState::PASSKEY_EXPECTED) &&
        !isSMPPairingAllowingInput(pairing_data.state, SMPPairingState::KEY_DISTRIBUTION) )
    {
        WARN_PRINT("BTDevice:mgmt:SMP: PINCODE_NEGATIVE, state %s, wrong state", to_string(pairing_data.state).c_str());
    }
    if constexpr ( USE_LINUX_BT_SECURITY ) {
        const BTManagerRef& mngr = adapter.getManager();
        MgmtStatus res = mngr->userPINCodeNegativeReply(adapter.dev_id, addressAndType);
        DBG_PRINT("BTDevice:mgmt:SMP: PINCODE NEGATIVE, state %s, result %s",
            to_string(pairing_data.state).c_str(), to_string(res).c_str());
        return HCIStatusCode::SUCCESS;
    } else if constexpr ( SMP_SUPPORTED_BY_OS ) {
        return HCIStatusCode::NOT_SUPPORTED;
    } else {
        return HCIStatusCode::NOT_SUPPORTED;
    }
}

HCIStatusCode BTDevice::setPairingPasskey(const uint32_t passkey) noexcept {
    const std::unique_lock<std::recursive_mutex> lock_pairing(mtx_pairing); // RAII-style acquire and relinquish via destructor

    if( !isSMPPairingAllowingInput(pairing_data.state, SMPPairingState::PASSKEY_EXPECTED) &&
        !isSMPPairingAllowingInput(pairing_data.state, SMPPairingState::KEY_DISTRIBUTION) )
    {
        WARN_PRINT("BTDevice:mgmt:SMP: PASSKEY '%u', state %s, wrong state", passkey, to_string(pairing_data.state).c_str());
    }
    if constexpr ( USE_LINUX_BT_SECURITY ) {
        const BTManagerRef& mngr = adapter.getManager();
        MgmtStatus res = mngr->userPasskeyReply(adapter.dev_id, addressAndType, passkey);
        DBG_PRINT("BTDevice:mgmt:SMP: PASSKEY '%u', state %s, result %s",
            passkey, to_string(pairing_data.state).c_str(), to_string(res).c_str());
        return HCIStatusCode::SUCCESS;
    } else if constexpr ( SMP_SUPPORTED_BY_OS ) {
        return HCIStatusCode::NOT_SUPPORTED;
    } else {
        return HCIStatusCode::NOT_SUPPORTED;
    }
}

HCIStatusCode BTDevice::setPairingPasskeyNegative() noexcept {
    const std::unique_lock<std::recursive_mutex> lock_pairing(mtx_pairing); // RAII-style acquire and relinquish via destructor

    if( !isSMPPairingAllowingInput(pairing_data.state, SMPPairingState::PASSKEY_EXPECTED) &&
        !isSMPPairingAllowingInput(pairing_data.state, SMPPairingState::KEY_DISTRIBUTION) )
    {
        WARN_PRINT("BTDevice:mgmt:SMP: PASSKEY_NEGATIVE, state %s, wrong state", to_string(pairing_data.state).c_str());
    }
    if constexpr ( USE_LINUX_BT_SECURITY ) {
        const BTManagerRef& mngr = adapter.getManager();
        MgmtStatus res = mngr->userPasskeyNegativeReply(adapter.dev_id, addressAndType);
        DBG_PRINT("BTDevice:mgmt:SMP: PASSKEY NEGATIVE, state %s, result %s",
            to_string(pairing_data.state).c_str(), to_string(res).c_str());
        return HCIStatusCode::SUCCESS;
    } else if constexpr ( SMP_SUPPORTED_BY_OS ) {
        return HCIStatusCode::NOT_SUPPORTED;
    } else {
        return HCIStatusCode::NOT_SUPPORTED;
    }
}

HCIStatusCode BTDevice::setPairingNumericComparison(const bool positive) noexcept {
    const std::unique_lock<std::recursive_mutex> lock_pairing(mtx_pairing); // RAII-style acquire and relinquish via destructor

    if( !isSMPPairingAllowingInput(pairing_data.state, SMPPairingState::NUMERIC_COMPARE_EXPECTED) &&
        !isSMPPairingAllowingInput(pairing_data.state, SMPPairingState::KEY_DISTRIBUTION) )
    {
        WARN_PRINT("BTDevice:mgmt:SMP: CONFIRM '%d', state %s, wrong state", positive, to_string(pairing_data.state).c_str());
    }
    if constexpr ( USE_LINUX_BT_SECURITY ) {
        const BTManagerRef& mngr = adapter.getManager();
        MgmtStatus res = mngr->userConfirmReply(adapter.dev_id, addressAndType, positive);
        DBG_PRINT("BTDevice:mgmt:SMP: CONFIRM '%d', state %s, result %s",
            positive, to_string(pairing_data.state).c_str(), to_string(res).c_str());
        return HCIStatusCode::SUCCESS;
    } else if constexpr ( SMP_SUPPORTED_BY_OS ) {
        return HCIStatusCode::NOT_SUPPORTED;
    } else {
        return HCIStatusCode::NOT_SUPPORTED;
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
    // Issued at ctor(), manual unpair() and notifyDisconnect()
    // notifyDisconnect() will be called at all times, even if disconnect() fails!
    const std::unique_lock<std::recursive_mutex> lock_pairing(mtx_pairing); // RAII-style acquire and relinquish via destructor

    // NOLINTNEXTLINE(clang-analyzer-optin.cplusplus.VirtualCall)
    DBG_PRINT("BTDevice::clearSMPStates(connected %d): %s", connected, toString().c_str());

    if( !connected ) {
        // needs to survive connected, or will be set right @ connected
        pairing_data.io_cap_user = SMPIOCapability::UNSET;
        pairing_data.io_cap_conn = SMPIOCapability::UNSET;
        pairing_data.sec_level_user = BTSecurityLevel::UNSET;
        // Keep alive: pairing_data.ioCap_auto = SMPIOCapability::UNSET;
    }
    pairing_data.sec_level_conn = BTSecurityLevel::UNSET;

    pairing_data.is_pre_paired = false;
    pairing_data.state = SMPPairingState::NONE;
    pairing_data.mode = PairingMode::NONE;
    pairing_data.res_requested_sec = false;
    pairing_data.use_sc = false;
    pairing_data.encryption_enabled = false;

    pairing_data.passKey_resp = 0;

    pairing_data.authReqs_resp = SMPAuthReqs::NONE;
    pairing_data.ioCap_resp    = SMPIOCapability::NO_INPUT_NO_OUTPUT;
    pairing_data.oobFlag_resp  = SMPOOBDataFlag::OOB_AUTH_DATA_NOT_PRESENT;
    pairing_data.maxEncsz_resp = 0;
    pairing_data.keys_resp_exp = SMPKeyType::NONE;
    pairing_data.keys_resp_has = SMPKeyType::NONE;
    pairing_data.ltk_resp.clear();
    pairing_data.irk_resp.clear();
    pairing_data.id_address_resp.clear();
    pairing_data.csrk_resp.clear();
    pairing_data.lk_resp.clear();

    pairing_data.authReqs_init = SMPAuthReqs::NONE;
    pairing_data.ioCap_init    = SMPIOCapability::NO_INPUT_NO_OUTPUT;
    pairing_data.oobFlag_init  = SMPOOBDataFlag::OOB_AUTH_DATA_NOT_PRESENT;
    pairing_data.maxEncsz_init = 0;
    pairing_data.keys_init_exp = SMPKeyType::NONE;
    pairing_data.keys_init_has = SMPKeyType::NONE;
    pairing_data.ltk_init.clear();
    pairing_data.irk_init.clear();
    pairing_data.id_address_init.clear();
    pairing_data.csrk_init.clear();
    pairing_data.lk_init.clear();
}

void BTDevice::disconnectSMP(const int caller) noexcept {
    if constexpr ( SMP_SUPPORTED_BY_OS ) {
        const std::lock_guard<std::recursive_mutex> lock_conn(mtx_smpHandler);
        if( nullptr != smpHandler ) {
            DBG_PRINT("BTDevice::disconnectSMP: start (has smpHandler, caller %d)", caller);
            smpHandler->disconnect(false /* disconnect_device */, false /* ioerr_cause */);
        } else {
            DBG_PRINT("BTDevice::disconnectSMP: start (nil smpHandler, caller %d)", caller);
        }
        smpHandler = nullptr;
        DBG_PRINT("BTDevice::disconnectSMP: end");
    } else {
        (void)caller;
    }
}

bool BTDevice::connectSMP(std::shared_ptr<BTDevice> sthis, const BTSecurityLevel sec_level) noexcept {
    if constexpr ( SMP_SUPPORTED_BY_OS ) {
        if( !isConnected || !allowDisconnect) {
            ERR_PRINT("connectSMP(%u): Device not connected: %s", sec_level, toString().c_str());
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
            ERR_PRINT("Connection failed");
            smpHandler = nullptr;
            return false;
        }
        return smpHandler->establishSecurity(sec_level);
    } else {
        DBG_PRINT("BTDevice::connectSMP: SMP Not supported by OS (0): %s", toString().c_str());
        (void)sthis;
        (void)sec_level;
        return false;
    }
}

void BTDevice::disconnectGATT(const int caller) noexcept {
    const std::lock_guard<std::recursive_mutex> lock_conn(mtx_gattHandler);
    if( nullptr != gattHandler ) {
        DBG_PRINT("BTDevice::disconnectGATT: start (has gattHandler, caller %d)", caller);
        gattHandler->disconnect(false /* disconnect_device */, false /* ioerr_cause */);
    } else {
        DBG_PRINT("BTDevice::disconnectGATT: start (nil gattHandler, caller %d)", caller);
    }
    gattHandler = nullptr;
    DBG_PRINT("BTDevice::disconnectGATT: end");
}

bool BTDevice::connectGATT(const std::shared_ptr<BTDevice>& sthis) noexcept {
    if( !isConnected || !allowDisconnect) {
        ERR_PRINT("Device not connected: %s", toString().c_str());
        return false;
    }
    if( !l2cap_att->is_open() ) {
        ERR_PRINT("L2CAP not open: %s", toString().c_str());
        return false;
    }

    const std::lock_guard<std::recursive_mutex> lock_conn(mtx_gattHandler);
    if( nullptr != gattHandler ) {
        if( gattHandler->isConnected() ) {
            return true;
        }
        gattHandler = nullptr;
    }

    DBG_PRINT("BTDevice::connectGATT: Start: %s", toString().c_str());

    // GATT MTU only consumes around 20ms - 100ms
    gattHandler = std::make_shared<BTGattHandler>(sthis, *l2cap_att, supervision_timeout);
    if( !gattHandler->isConnected() ) {
        ERR_PRINT2("Connection failed");
        gattHandler = nullptr;
        return false;
    } else if ( BTRole::Master == btRole ) {
        DBG_PRINT("BTDevice::connectGATT: Local GATT Server: Done: %s", toString().c_str());
    } else {
        DBG_PRINT("BTDevice::connectGATT: Local GATT Client: Done: %s", toString().c_str());
    }
    return true;
}

std::shared_ptr<BTGattHandler> BTDevice::getGattHandler() noexcept {
    const std::lock_guard<std::recursive_mutex> lock_conn(mtx_gattHandler);
    return gattHandler;
}

BTDevice::GattServiceList_t BTDevice::getGattServices() noexcept {
    std::shared_ptr<BTGattHandler> gh = getGattHandler();
    if( nullptr == gh ) {
        ERR_PRINT("GATTHandler nullptr: %s", toString().c_str());
        return jau::darray<std::shared_ptr<BTGattService>>();
    }
    if( BTRole::Slave != getRole() ) {
        // Remote device is not a slave (peripheral, responder) - hence no GATT services
        ERR_PRINT("Remote device not a GATT server: ", toString().c_str());
        return jau::darray<std::shared_ptr<BTGattService>>();
    }

    bool gatt_already_init = false;
    if( !gh->initClientGatt(gh, gatt_already_init) ) {
        ERR_PRINT2("Client GATT Initialization failed");
        return jau::darray<BTGattServiceRef>(); // return zero size
    }
    if( gatt_already_init ) {
        return gh->getServices(); // copy previous discovery result
    }

    GattServiceList_t result = gh->getServices(); // copy
    if( result.size() == 0 ) { // nothing discovered, actually a redundant check done @ BTGattHandler::initClientGatt() 1st
        ERR_PRINT2("No primary services discovered");
        return jau::darray<BTGattServiceRef>(); // return zero size
    }

    // discovery success, parse GenericAccess
    std::shared_ptr<GattGenericAccessSvc> gattGenericAccess = gh->getGenericAccess();
    if( nullptr == gattGenericAccess ) {
        // no GenericAccess discovered, actually a redundant check done @ BTGattHandler::initClientGatt() 1st
        ERR_PRINT2("No GenericAccess: %s", toString().c_str());
        return jau::darray<BTGattServiceRef>(); // return zero size
    }

    const uint64_t ts = jau::getCurrentMilliseconds();
    EIRDataType updateMask = update(*gattGenericAccess, ts);
    DBG_PRINT("BTDevice::getGattServices: GenericAccess updated %s:\n    %s\n    -> %s",
        to_string(updateMask).c_str(), gattGenericAccess->toString().c_str(), toString().c_str());
    if( EIRDataType::NONE != updateMask ) {
        std::shared_ptr<BTDevice> sharedInstance = getSharedInstance();
        if( nullptr == sharedInstance ) {
            ERR_PRINT("Device unknown to adapter and not tracked: %s", toString().c_str());
        } else {
            adapter.sendDeviceUpdated("getGattServices", sharedInstance, ts, updateMask);
        }
    }
    return result; // return the copy, copy elision shall be used
}

std::shared_ptr<GattGenericAccessSvc> BTDevice::getGattGenericAccess() {
    std::shared_ptr<BTGattHandler> gh = getGattHandler();
    if( nullptr == gh ) {
        ERR_PRINT("GATTHandler nullptr");
        return nullptr;
    }
    return gh->getGenericAccess();
}

BTGattServiceRef BTDevice::findGattService(const jau::uuid_t& service_uuid) noexcept {
    const jau::darray<std::shared_ptr<BTGattService>> & services = getGattServices(); // reference of the GATTHandler's list
    for(const BTGattServiceRef& s : services) {
        if ( nullptr != s && service_uuid.equivalent( *(s->type) ) ) {
            return s;
        }
    }
    return nullptr;
}

BTGattCharRef BTDevice::findGattChar(const jau::uuid_t&  service_uuid, const jau::uuid_t& char_uuid) noexcept {
    BTGattServiceRef service = findGattService(service_uuid);
    if( nullptr == service ) {
        return nullptr;
    }
    return service->findGattChar(char_uuid);
}

BTGattCharRef BTDevice::findGattChar(const jau::uuid_t& char_uuid) noexcept {
    const jau::darray<std::shared_ptr<BTGattService>> & services = getGattServices(); // reference of the GATTHandler's list
    for(const BTGattServiceRef& s : services) {
        if ( nullptr != s ) {
            BTGattCharRef c = s->findGattChar(char_uuid);
            if ( nullptr != c ) {
                return c;
            }
        }
    }
    return nullptr;
}

bool BTDevice::sendNotification(const uint16_t char_value_handle, const jau::TROOctets & value) noexcept {
    if( !isValidInstance() ) {
        ERR_PRINT("Device invalid: %p", jau::to_hexstring((void*)this).c_str());
        return false;
    }
    std::shared_ptr<BTGattHandler> gh = getGattHandler();
    if( nullptr == gh || !gh->isConnected() ) {
        WARN_PRINT("GATTHandler not connected -> disconnected on %s", toString().c_str());
        return false;
    }
    return gh->sendNotification(char_value_handle, value);
}

bool BTDevice::sendIndication(const uint16_t char_value_handle, const jau::TROOctets & value) noexcept {
    if( !isValidInstance() ) {
        ERR_PRINT("Device invalid: %p", jau::to_hexstring((void*)this).c_str());
        return false;
    }
    std::shared_ptr<BTGattHandler> gh = getGattHandler();
    if( nullptr == gh || !gh->isConnected() ) {
        WARN_PRINT("GATTHandler not connected -> disconnected on %s", toString().c_str());
        return false;
    }
    return gh->sendIndication(char_value_handle, value);
}


bool BTDevice::pingGATT() noexcept {
    std::shared_ptr<BTGattHandler> gh = getGattHandler();
    if( nullptr == gh || !gh->isConnected() ) {
        jau::INFO_PRINT("BTDevice::pingGATT: GATTHandler not connected -> disconnected on %s", toString().c_str());
        disconnect(HCIStatusCode::REMOTE_USER_TERMINATED_CONNECTION);
        return false;
    }
    return gh->ping();
}

bool BTDevice::addCharListener(const BTGattCharListenerRef& l) noexcept {
    std::shared_ptr<BTGattHandler> gatt = getGattHandler();
    if( nullptr == gatt ) {
        ERR_PRINT("Device's GATTHandle not connected: %s", toString().c_str());
        return false;
    }
    return gatt->addCharListener(l);
}

bool BTDevice::addCharListener(const BTGattCharListenerRef& l, const BTGattCharRef& d) noexcept {
    std::shared_ptr<BTGattHandler> gatt = getGattHandler();
    if( nullptr == gatt ) {
        ERR_PRINT("Device's GATTHandle not connected: %s", toString().c_str());
        return false;
    }
    return gatt->addCharListener(l, d);
}

bool BTDevice::removeCharListener(const BTGattCharListenerRef& l) noexcept {
    std::shared_ptr<BTGattHandler> gatt = getGattHandler();
    if( nullptr == gatt ) {
        // OK to have GATTHandler being shutdown @ disable
        DBG_PRINT("Device's GATTHandle not connected: %s", toString().c_str());
        return false;
    }
    return gatt->removeCharListener(l);
}

BTDevice::size_type BTDevice::removeAllAssociatedCharListener(const BTGattCharRef& associatedCharacteristic) noexcept {
    std::shared_ptr<BTGattHandler> gatt = getGattHandler();
    if( nullptr == gatt ) {
        // OK to have GATTHandler being shutdown @ disable
        DBG_PRINT("Device's GATTHandle not connected: %s", toString().c_str());
        return false;
    }
    return gatt->removeAllAssociatedCharListener( associatedCharacteristic );
}

BTDevice::size_type BTDevice::removeAllAssociatedCharListener(const BTGattChar * associatedCharacteristic) noexcept {
    std::shared_ptr<BTGattHandler> gatt = getGattHandler();
    if( nullptr == gatt ) {
        // OK to have GATTHandler being shutdown @ disable
        DBG_PRINT("Device's GATTHandle not connected: %s", toString().c_str());
        return false;
    }
    return gatt->removeAllAssociatedCharListener( associatedCharacteristic );
}

BTDevice::size_type BTDevice::removeAllCharListener() noexcept {
    std::shared_ptr<BTGattHandler> gatt = getGattHandler();
    if( nullptr == gatt ) {
        // OK to have GATTHandler being shutdown @ disable
        DBG_PRINT("Device's GATTHandle not connected: %s", toString().c_str());
        return 0;
    }
    return gatt->removeAllCharListener();
}

HCIStatusCode BTDevice::getConnectedLE_PHY(LE_PHYs& resTx, LE_PHYs& resRx) noexcept {
    const std::lock_guard<std::recursive_mutex> lock_conn(mtx_connect); // RAII-style acquire and relinquish via destructor

    if( !isConnected ) { // should not happen
        return HCIStatusCode::DISCONNECTED;
    }

    if( 0 == hciConnHandle ) {
        return HCIStatusCode::UNSPECIFIED_ERROR;
    }

    if( !adapter.isPowered() ) { // isValid() && hci.isOpen() && POWERED
        return HCIStatusCode::NOT_POWERED; // powered-off
    }

    HCIHandler &hci = adapter.getHCI();
    HCIStatusCode res = hci.le_read_phy(hciConnHandle, addressAndType, resTx, resRx);
    if( HCIStatusCode::SUCCESS == res ) {
        le_phy_tx = resTx;
        le_phy_rx = resRx;
    }
    return res;
}

HCIStatusCode BTDevice::setConnectedLE_PHY(const LE_PHYs Tx, const LE_PHYs Rx) noexcept {
    const std::lock_guard<std::recursive_mutex> lock_conn(mtx_connect); // RAII-style acquire and relinquish via destructor

    if( !isConnected ) { // should not happen
        return HCIStatusCode::DISCONNECTED;
    }

    if( 0 == hciConnHandle ) {
        return HCIStatusCode::UNSPECIFIED_ERROR;
    }

    if( !adapter.isPowered() ) { // isValid() && hci.isOpen() && POWERED
        return HCIStatusCode::NOT_POWERED; // powered-off
    }

    HCIHandler &hci = adapter.getHCI();
    return hci.le_set_phy(hciConnHandle, addressAndType, Tx, Rx);
}

void BTDevice::notifyDisconnected() noexcept {
    // coming from disconnect callback, ensure cleaning up!
    DBG_PRINT("BTDevice::notifyDisconnected: handle %s -> zero, %s",
              jau::to_hexstring(hciConnHandle).c_str(), toString().c_str());
    allowDisconnect = false;
    supervision_timeout = 0;
    isConnected = false;
    hciConnHandle = 0;
    smp_events = 0;
    unpair(); // -> clearSMPStates(false /* connected */);
    disconnectGATT(1);
    disconnectSMP(1);
    l2cap_att->close();
    // clearData(); to be performed after notifying listener and if !isConnSecurityAutoEnabled()
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
        WARN_PRINT("allowConnect true -> false, but !isConnected on %s", toString().c_str());
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

    if( !adapter.isPowered() ) { // isValid() && hci.isOpen() && POWERED
        WARN_PRINT("Adapter not powered: %s, %s", adapter.toString().c_str(), toString().c_str());
        res = HCIStatusCode::NOT_POWERED; // powered-off
        goto exit;
    }

    res = hci.disconnect(hciConnHandle, addressAndType, reason);
    if( HCIStatusCode::SUCCESS != res ) {
        ERR_PRINT("status %s, handle 0x%X, isConnected %d/%d: errno %d %s on %s",
                to_string(res).c_str(), hciConnHandle.load(),
                allowDisconnect.load(), isConnected.load(),
                errno, strerror(errno),
                toString().c_str());
    }

exit:
    {
        // Start resolving from scratch
        HCIStatusCode res2 = hci.le_del_from_resolv_list(addressAndType);
        if( HCIStatusCode::SUCCESS != res2 ) {
            jau::INFO_PRINT("BTDevice::disconnect: DEL FROM RESOLV LIST failed %s for %s",
                    to_string(res2).c_str(), toString().c_str());
        }
    }

    if( HCIStatusCode::SUCCESS != res ) {
        // In case of an already pulled or disconnected HCIHandler (e.g. power-off)
        // or in case the hci->disconnect() itself fails,
        // send the DISCONN_COMPLETE event directly.
        // SEND_EVENT: Perform off-thread to avoid potential deadlock w/ application callbacks (similar when sent from HCIHandler's reader-thread)
        std::thread bg(&BTDevice::sendMgmtEvDeviceDisconnected, this, // @suppress("Invalid arguments")
                       std::make_unique<MgmtEvtDeviceDisconnected>(adapter.dev_id, addressAndType, reason, hciConnHandle) );
        bg.detach();
        // adapter.mgmtEvDeviceDisconnectedHCI( std::unique_ptr<MgmtEvent>( new MgmtEvtDeviceDisconnected(adapter.dev_id, address, addressType, reason, hciConnHandle) ) );
    }
    WORDY_PRINT("BTDevice::disconnect: End: status %s, handle 0x%X, isConnected %d/%d on %s",
            to_string(res).c_str(),
            hciConnHandle.load(), allowDisconnect.load(), isConnected.load(),
            toString().c_str());

    return res;
}

std::uint32_t BTDevice::getResponderSMPPassKey() const noexcept {
    return pairing_data.passKey_resp;
}

HCIStatusCode BTDevice::unpair() noexcept {
    if constexpr ( USE_LINUX_BT_SECURITY ) {
        const BTManagerRef& mngr = adapter.getManager();
        const HCIStatusCode res = mngr->unpairDevice(adapter.dev_id, addressAndType, false /* disconnect */);
        if( HCIStatusCode::SUCCESS != res && HCIStatusCode::NOT_PAIRED != res ) {
            DBG_PRINT("BTDevice::unpair(): Unpair device failed: %s, %s", to_string(res).c_str(), toString().c_str());
        }
        clearSMPStates(getConnected() /* connected */);
        return res;
    } else if constexpr ( SMP_SUPPORTED_BY_OS ) {
        return HCIStatusCode::NOT_SUPPORTED;
    } else {
        return HCIStatusCode::NOT_SUPPORTED;
    }
}

void BTDevice::remove() noexcept {
    adapter.removeDevice(*this);
}
