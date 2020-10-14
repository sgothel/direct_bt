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
#include <dbt_debug.hpp>

#include "HCIComm.hpp"

#include "DBTDevice.hpp"
#include "DBTAdapter.hpp"

using namespace direct_bt;

DBTDevice::DBTDevice(DBTAdapter & a, EInfoReport const & r)
: adapter(a), ts_creation(r.getTimestamp()),
  address(r.getAddress()), addressType(r.getAddressType()),
  leRandomAddressType(address.getBLERandomAddressType(addressType))
{
    ts_last_discovery = ts_creation;
    hciConnHandle = 0;
    isConnected = false;
    allowDisconnect = false;
    if( !r.isSet(EIRDataType::BDADDR) ) {
        throw IllegalArgumentException("Address not set: "+r.toString(), E_FILE_LINE);
    }
    if( !r.isSet(EIRDataType::BDADDR_TYPE) ) {
        throw IllegalArgumentException("AddressType not set: "+r.toString(), E_FILE_LINE);
    }
    update(r);

    if( BDAddressType::BDADDR_LE_RANDOM == addressType ) {
        if( BLERandomAddressType::UNDEFINED == leRandomAddressType ) {
            throw IllegalArgumentException("BDADDR_LE_RANDOM: Invalid BLERandomAddressType "+
                    getBLERandomAddressTypeString(leRandomAddressType)+": "+toString(), E_FILE_LINE);
        }
    } else {
        if( BLERandomAddressType::UNDEFINED != leRandomAddressType ) {
            throw IllegalArgumentException("Not BDADDR_LE_RANDOM: Invalid given native BLERandomAddressType "+
                    getBLERandomAddressTypeString(leRandomAddressType)+": "+toString(), E_FILE_LINE);
        }
    }
}

DBTDevice::~DBTDevice() noexcept {
    DBG_PRINT("DBTDevice::dtor: ... %p %s", this, getAddressString().c_str());
    remove();
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
    const uint64_t t0 = getCurrentMilliseconds();
    std::string leaddrtype;
    if( BLERandomAddressType::UNDEFINED != leRandomAddressType ) {
        leaddrtype = ", random "+getBLERandomAddressTypeString(leRandomAddressType);
    }
    std::string msdstr = nullptr != advMSD ? advMSD->toString() : "MSD[null]";
    std::string out("Device[address["+getAddressString()+", "+getBDAddressTypeString(getAddressType())+leaddrtype+"], name['"+name+
            "'], age[total "+std::to_string(t0-ts_creation)+", ldisc "+std::to_string(t0-ts_last_discovery)+", lup "+std::to_string(t0-ts_last_update)+
            "]ms, connected["+std::to_string(allowDisconnect)+"/"+std::to_string(isConnected)+", "+uint16HexString(hciConnHandle)+"], rssi "+std::to_string(getRSSI())+
            ", tx-power "+std::to_string(tx_power)+
            ", appearance "+uint16HexString(static_cast<uint16_t>(appearance))+" ("+getAppearanceCatString(appearance)+
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
                    data.toString().c_str(), this->toString().c_str());
        }
    }
    if( data.isSet(EIRDataType::BDADDR_TYPE) ) {
        if( data.getAddressType() != this->addressType ) {
            WARN_PRINT("DBTDevice::update:: BDADDR_TYPE update not supported: %s for %s",
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
                ERR_PRINT("DBTDevice::getConnectionInfo: Device unknown to adapter and not tracked: %s", toString().c_str());
            } else {
                adapter.sendDeviceUpdated("getConnectionInfo", sharedInstance, getCurrentMilliseconds(), updateMask);
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
    adapter.checkValid();

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
                                getBLERandomAddressTypeString(leRandomAddressType).c_str(), toString().c_str());
                        return HCIStatusCode::UNACCEPTABLE_CONNECTION_PARAM;
                    case BLERandomAddressType::RESOLVABLE_PRIVAT:
                        hci_peer_mac_type = HCILEPeerAddressType::PUBLIC_IDENTITY;
                        hci_own_mac_type = HCILEOwnAddressType::RESOLVABLE_OR_PUBLIC;
                        ERR_PRINT("LE Random address type '%s' not supported yet: %s",
                                getBLERandomAddressTypeString(leRandomAddressType).c_str(), toString().c_str());
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
                                getBLERandomAddressTypeString(leRandomAddressType).c_str(), toString().c_str());
                        return HCIStatusCode::UNACCEPTABLE_CONNECTION_PARAM;
                    }
                }
            } break;
        default: {
                ERR_PRINT("Can't connectLE to address type '%s': %s", getBDAddressTypeString(addressType).c_str(), toString().c_str());
                return HCIStatusCode::UNACCEPTABLE_CONNECTION_PARAM;
            }
    }

    if( isConnected ) {
        ERR_PRINT("DBTDevice::connectLE: Already connected: %s", toString().c_str());
        return HCIStatusCode::CONNECTION_ALREADY_EXISTS;
    }

    HCIHandler &hci = adapter.getHCI();
    if( !hci.isOpen() ) {
        ERR_PRINT("DBTDevice::connectLE: HCI closed: %s", toString().c_str());
        return HCIStatusCode::INTERNAL_FAILURE;
    }
    HCIStatusCode status = hci.le_create_conn(address,
                                      hci_peer_mac_type, hci_own_mac_type,
                                      le_scan_interval, le_scan_window, conn_interval_min, conn_interval_max,
                                      conn_latency, supervision_timeout);
    allowDisconnect = true;
#if 0
    if( HCIStatusCode::CONNECTION_ALREADY_EXISTS == status ) {
        WORDY_PRINT("DBTDevice::connectLE: Connection already exists: status 0x%2.2X (%s) on %s",
                static_cast<uint8_t>(status), getHCIStatusCodeString(status).c_str(), toString().c_str());
        std::shared_ptr<DBTDevice> sharedInstance = getSharedInstance();
        if( nullptr == sharedInstance ) {
            throw InternalError("DBTDevice::connectLE: Device unknown to adapter and not tracked: "+toString(), E_FILE_LINE);
        }
        adapter.performDeviceConnected(sharedInstance, getCurrentMilliseconds());
        return 0;
    }
#endif
    if( HCIStatusCode::COMMAND_DISALLOWED == status ) {
        WARN_PRINT("DBTDevice::connectLE: Could not yet create connection: status 0x%2.2X (%s), errno %d, hci-atype[peer %s, own %s] %s on %s",
                static_cast<uint8_t>(status), getHCIStatusCodeString(status).c_str(), errno, strerror(errno),
                getHCILEPeerAddressTypeString(hci_peer_mac_type).c_str(),
                getHCILEOwnAddressTypeString(hci_own_mac_type).c_str(),
                toString().c_str());
    } else if ( HCIStatusCode::SUCCESS != status ) {
        ERR_PRINT("DBTDevice::connectLE: Could not create connection: status 0x%2.2X (%s), errno %d %s, hci-atype[peer %s, own %s] on %s",
                static_cast<uint8_t>(status), getHCIStatusCodeString(status).c_str(), errno, strerror(errno),
                getHCILEPeerAddressTypeString(hci_peer_mac_type).c_str(),
                getHCILEOwnAddressTypeString(hci_own_mac_type).c_str(),
                toString().c_str());
    }
    return status;
}

HCIStatusCode DBTDevice::connectBREDR(const uint16_t pkt_type, const uint16_t clock_offset, const uint8_t role_switch)
{
    const std::lock_guard<std::recursive_mutex> lock_conn(mtx_connect); // RAII-style acquire and relinquish via destructor
    adapter.checkValid();

    if( isConnected ) {
        ERR_PRINT("DBTDevice::connectBREDR: Already connected: %s", toString().c_str());
        return HCIStatusCode::CONNECTION_ALREADY_EXISTS;
    }
    if( !isBREDRAddressType() ) {
        ERR_PRINT("DBTDevice::connectBREDR: Not a BDADDR_BREDR address: %s", toString().c_str());
        return HCIStatusCode::UNACCEPTABLE_CONNECTION_PARAM;
    }

    HCIHandler &hci = adapter.getHCI();
    if( !hci.isOpen() ) {
        ERR_PRINT("DBTDevice::connectBREDR: HCI closed: %s", toString().c_str());
        return HCIStatusCode::INTERNAL_FAILURE;
    }
    HCIStatusCode status = hci.create_conn(address, pkt_type, clock_offset, role_switch);
    allowDisconnect = true;
    if ( HCIStatusCode::SUCCESS != status ) {
        ERR_PRINT("DBTDevice::connectBREDR: Could not create connection: status 0x%2.2X (%s), errno %d %s on %s",
                static_cast<uint8_t>(status), getHCIStatusCodeString(status).c_str(), errno, strerror(errno), toString().c_str());
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
            ERR_PRINT("DBTDevice::connectDefault: Not a valid address type: %s", toString().c_str());
            return HCIStatusCode::UNACCEPTABLE_CONNECTION_PARAM;
    }
}

void DBTDevice::notifyConnected(const uint16_t handle) noexcept {
    // coming from connected callback, update state and spawn-off connectGATT in background if appropriate (LE)
    DBG_PRINT("DBTDevice::notifyConnected: handle %s -> %s, %s",
            uint16HexString(hciConnHandle).c_str(), uint16HexString(handle).c_str(), toString().c_str());
    allowDisconnect = true;
    isConnected = true;
    hciConnHandle = handle;
    if( isLEAddressType() ) {
        std::thread bg(&DBTDevice::connectGATT, this); // @suppress("Invalid arguments")
        bg.detach();
    }
}

void DBTDevice::notifyDisconnected() noexcept {
    // coming from disconnect callback, ensure cleaning up!
    DBG_PRINT("DBTDevice::notifyDisconnected: handle %s -> zero, %s",
            uint16HexString(hciConnHandle).c_str(), toString().c_str());
    allowDisconnect = false;
    isConnected = false;
    hciConnHandle = 0;
    disconnectGATT(1);
}

void DBTDevice::disconnectGATT(int caller) noexcept {
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

HCIStatusCode DBTDevice::disconnect(const HCIStatusCode reason) noexcept {
    // Avoid disconnect re-entry lock-free
    bool expConn = true; // C++11, exp as value since C++20
    if( !allowDisconnect.compare_exchange_strong(expConn, false) ) {
        // Not connected or disconnect already in process.
        DBG_PRINT("DBTDevice::disconnect: Not connected: isConnected %d/%d, reason 0x%X (%s), gattHandler %d, hciConnHandle %s",
                allowDisconnect.load(), isConnected.load(),
                static_cast<uint8_t>(reason), getHCIStatusCodeString(reason).c_str(),
                (nullptr != gattHandler), uint16HexString(hciConnHandle).c_str());
        return HCIStatusCode::CONNECTION_TERMINATED_BY_LOCAL_HOST;
    }
    if( !isConnected ) { // should not happen
        WARN_PRINT("DBTDevice::disconnect: allowConnect true -> false, but !isConnected on %s", toString(false).c_str());
        return HCIStatusCode::SUCCESS;
    }

    // Disconnect GATT before device, keeping reversed initialization order intact if possible.
    // This outside mtx_connect, keeping same mutex lock order intact as well
    disconnectGATT(0);

    // Lock to avoid other threads connecting while disconnecting
    const std::lock_guard<std::recursive_mutex> lock_conn(mtx_connect); // RAII-style acquire and relinquish via destructor

    WORDY_PRINT("DBTDevice::disconnect: Start: isConnected %d/%d, reason 0x%X (%s), gattHandler %d, hciConnHandle %s",
            allowDisconnect.load(), isConnected.load(),
            static_cast<uint8_t>(reason), getHCIStatusCodeString(reason).c_str(),
            (nullptr != gattHandler), uint16HexString(hciConnHandle).c_str());

    HCIHandler &hci = adapter.getHCI();
    HCIStatusCode res = HCIStatusCode::SUCCESS;

    if( 0 == hciConnHandle ) {
        res = HCIStatusCode::UNSPECIFIED_ERROR;
        goto exit;
    }

    if( !hci.isOpen() ) {
        ERR_PRINT("DBTDevice::disconnect: Skip disconnect: HCI closed: %s", toString().c_str());
        res = HCIStatusCode::UNSPECIFIED_ERROR; // powered-off?
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
            getHCIStatusCodeString(res).c_str(), hciConnHandle.load(),
            allowDisconnect.load(), isConnected.load(),
            toString(false).c_str());

    return res;
}

std::vector<PairingMode> DBTDevice::getSupportedPairingModes() {
    std::vector<PairingMode> pairingModes; // FIXME: Implement LE Secure Connections
    return pairingModes;
}

std::vector<PairingMode> DBTDevice::getRequiredPairingModes() {
    std::vector<PairingMode> pairingModes; // FIXME: Implement LE Secure Connections
    return pairingModes;
}

HCIStatusCode DBTDevice::pair(const std::string & passkey) {
    (void) passkey;
    return HCIStatusCode::INTERNAL_FAILURE; // FIXME: Implement LE Secure Connections
}

void DBTDevice::remove() noexcept {
    adapter.removeDevice(*this);
}

bool DBTDevice::connectGATT() noexcept {
    if( !isConnected || !allowDisconnect) {
        ERR_PRINT("DBTDevice::connectGATT: Device not connected: %s", toString().c_str());
        return false;
    }

    std::shared_ptr<DBTDevice> sharedInstance = getSharedInstance();
    if( nullptr == sharedInstance ) {
        ERR_PRINT("DBTDevice::connectGATT: Device unknown to adapter and not tracked: %s", toString().c_str());
        return false;
    }

    const std::lock_guard<std::recursive_mutex> lock_conn(mtx_gattHandler);
    if( nullptr != gattHandler ) {
        if( gattHandler->isConnected() ) {
            return true;
        }
        gattHandler = nullptr;
    }

    gattHandler = std::shared_ptr<GATTHandler>(new GATTHandler(sharedInstance));
    if( !gattHandler->isConnected() ) {
        ERR_PRINT("DBTDevice::connectGATT: Connection failed");
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

        // discovery success, retrieve and parse GenericAccess
        gattGenericAccess = gh->getGenericAccess(gattServices);
        if( nullptr != gattGenericAccess ) {
            const uint64_t ts = getCurrentMilliseconds();
            EIRDataType updateMask = update(*gattGenericAccess, ts);
            DBG_PRINT("DBTDevice::getGATTServices: updated %s:\n    %s\n    -> %s",
                getEIRDataMaskString(updateMask).c_str(), gattGenericAccess->toString().c_str(), toString().c_str());
            if( EIRDataType::NONE != updateMask ) {
                std::shared_ptr<DBTDevice> sharedInstance = getSharedInstance();
                if( nullptr == sharedInstance ) {
                    ERR_PRINT("DBTDevice::getGATTServices: Device unknown to adapter and not tracked: %s", toString().c_str());
                } else {
                    adapter.sendDeviceUpdated("getGATTServices", sharedInstance, ts, updateMask);
                }
            }
        }
    } catch (std::exception &e) {
        WARN_PRINT("DBTDevice::getGATTServices: Caught exception: '%s' on %s", e.what(), toString().c_str());
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
        INFO_PRINT("DBTDevice::pingGATT: GATTHandler not connected -> disconnected on %s", toString().c_str());
        disconnect(HCIStatusCode::REMOTE_USER_TERMINATED_CONNECTION);
        return false;
    }
    try {
        return gh->ping();
    } catch (std::exception &e) {
        IRQ_PRINT("DBTDevice::pingGATT: Potential disconnect, exception: '%s' on %s", e.what(), toString().c_str());
    }
    return false;
}

std::shared_ptr<GattGenericAccessSvc> DBTDevice::getGATTGenericAccess() {
    return gattGenericAccess;
}

bool DBTDevice::addCharacteristicListener(std::shared_ptr<GATTCharacteristicListener> l) {
    std::shared_ptr<GATTHandler> gatt = getGATTHandler();
    if( nullptr == gatt ) {
        throw IllegalStateException("Device's GATTHandle not connected: "+
                toString(), E_FILE_LINE);
    }
    return gatt->addCharacteristicListener(l);
}

bool DBTDevice::removeCharacteristicListener(std::shared_ptr<GATTCharacteristicListener> l) noexcept {
    std::shared_ptr<GATTHandler> gatt = getGATTHandler();
    if( nullptr == gatt ) {
        // OK to have GATTHandler being shutdown @ disable
        DBG_PRINT("Device's GATTHandle not connected: %s", toString().c_str());
        return false;
    }
    return gatt->removeCharacteristicListener(l);
}

int DBTDevice::removeAllAssociatedCharacteristicListener(std::shared_ptr<GATTCharacteristic> associatedCharacteristic) noexcept {
    std::shared_ptr<GATTHandler> gatt = getGATTHandler();
    if( nullptr == gatt ) {
        // OK to have GATTHandler being shutdown @ disable
        DBG_PRINT("Device's GATTHandle not connected: %s", toString().c_str());
        return false;
    }
    return gatt->removeAllAssociatedCharacteristicListener( associatedCharacteristic );
}

int DBTDevice::removeAllCharacteristicListener() noexcept {
    std::shared_ptr<GATTHandler> gatt = getGATTHandler();
    if( nullptr == gatt ) {
        // OK to have GATTHandler being shutdown @ disable
        DBG_PRINT("Device's GATTHandle not connected: %s", toString().c_str());
        return 0;
    }
    return gatt->removeAllCharacteristicListener();
}
