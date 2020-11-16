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

#include <algorithm>

#include <jau/debug.hpp>

#include <jau/basic_algos.hpp>

#include "BTIoctl.hpp"
#include "HCIIoctl.hpp"
#include "HCIComm.hpp"

#include "DBTAdapter.hpp"

extern "C" {
    #include <inttypes.h>
    #include <unistd.h>
    #include <poll.h>
}

using namespace direct_bt;

int DBTAdapter::findDeviceIdx(std::vector<std::shared_ptr<DBTDevice>> & devices, EUI48 const & mac, const BDAddressType macType) noexcept {
    const size_t size = devices.size();
    for (size_t i = 0; i < size; i++) {
        std::shared_ptr<DBTDevice> & e = devices[i];
        if ( nullptr != e && mac == e->getAddress() && macType == e->getAddressType() ) {
            return i;
        }
    }
    return -1;
}

std::shared_ptr<DBTDevice> DBTAdapter::findDevice(std::vector<std::shared_ptr<DBTDevice>> & devices, EUI48 const & mac, const BDAddressType macType) noexcept {
    const size_t size = devices.size();
    for (size_t i = 0; i < size; i++) {
        std::shared_ptr<DBTDevice> & e = devices[i];
        if ( nullptr != e && mac == e->getAddress() && macType == e->getAddressType() ) {
            return e;
        }
    }
    return nullptr;
}

std::shared_ptr<DBTDevice> DBTAdapter::findDevice(std::vector<std::shared_ptr<DBTDevice>> & devices, DBTDevice const & device) noexcept {
    const size_t size = devices.size();
    for (size_t i = 0; i < size; i++) {
        std::shared_ptr<DBTDevice> & e = devices[i];
        if ( nullptr != e && device == *e ) {
            return e;
        }
    }
    return nullptr;
}

bool DBTAdapter::addConnectedDevice(const std::shared_ptr<DBTDevice> & device) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_connectedDevices); // RAII-style acquire and relinquish via destructor
    if( nullptr != findDevice(connectedDevices, *device) ) {
        return false;
    }
    connectedDevices.push_back(device);
    return true;
}

bool DBTAdapter::removeConnectedDevice(const DBTDevice & device) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_connectedDevices); // RAII-style acquire and relinquish via destructor
    for (auto it = connectedDevices.begin(); it != connectedDevices.end(); ) {
        if ( nullptr != *it && device == **it ) {
            it = connectedDevices.erase(it);
            return true;
        } else {
            ++it;
        }
    }
    return false;
}

int DBTAdapter::disconnectAllDevices(const HCIStatusCode reason) noexcept {
    std::vector<std::shared_ptr<DBTDevice>> devices;
    {
        const std::lock_guard<std::mutex> lock(mtx_connectedDevices); // RAII-style acquire and relinquish via destructor
        devices = connectedDevices; // copy!
    }
    const int count = devices.size();
    for (auto it = devices.begin(); it != devices.end(); ++it) {
        if( nullptr != *it ) {
            (*it)->disconnect(reason); // will erase device from list via removeConnectedDevice(..) above
        }
    }
    return count;
}

std::shared_ptr<DBTDevice> DBTAdapter::findConnectedDevice (EUI48 const & mac, const BDAddressType macType) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_connectedDevices); // RAII-style acquire and relinquish via destructor
    return findDevice(connectedDevices, mac, macType);
}


// *************************************************
// *************************************************
// *************************************************

bool DBTAdapter::validateDevInfo() noexcept {
    bool ok = false;
    currentMetaScanType = ScanType::NONE;
    keep_le_scan_alive = false;

    if( 0 > dev_id ) {
        ERR_PRINT("DBTAdapter::validateDevInfo: Invalid negative dev_id %d", dev_id);
        goto errout0;
    }
    if( !mgmt.isOpen() ) {
        ERR_PRINT("DBTAdapter::validateDevInfo: Adapter[%d]: Manager not open", dev_id);
        goto errout0;
    }
    if( !hci.isOpen() ) {
        ERR_PRINT("DBTAdapter::validateDevInfo: Adapter[%d]: HCIHandler closed", dev_id);
        goto errout0;
    }

    adapterInfo = mgmt.getAdapterInfo(dev_id);
    if( nullptr == adapterInfo ) {
        // fill in a dummy AdapterInfo for the sake of de-referencing throughout this adapter instance
        ERR_PRINT("DBTAdapter::validateDevInfo: Adapter[%d]: Not existent", dev_id);
        goto errout0;
    }
    old_settings = adapterInfo->getCurrentSettingMask();

    btMode = adapterInfo->getCurrentBTMode();
    if( BTMode::NONE == btMode ) {
        ERR_PRINT("DBTAdapter::validateDevInfo: Adapter[%d]: BTMode invalid, BREDR nor LE set: %s", dev_id, adapterInfo->toString().c_str());
        return false;
    }
    hci.setBTMode(btMode);

    if( adapterInfo->isCurrentSettingBitSet(AdapterSetting::POWERED) ) {
        HCILocalVersion version;
        HCIStatusCode status = hci.getLocalVersion(version);
        if( HCIStatusCode::SUCCESS != status ) {
            ERR_PRINT("DBTAdapter::validateDevInfo: Adapter[%d]: POWERED, LocalVersion failed %s - %s",
                    dev_id, getHCIStatusCodeString(status).c_str(), adapterInfo->toString().c_str());
            return false;
        } else {
            WORDY_PRINT("DBTAdapter::validateDevInfo: Adapter[%d]: POWERED, %s - %s",
                    dev_id, version.toString().c_str(), adapterInfo->toString().c_str());
        }
    } else {
        WORDY_PRINT("DBTAdapter::validateDevInfo: Adapter[%d]: Not POWERED: %s", dev_id, adapterInfo->toString().c_str());
    }
    ok = true;
    ok = mgmt.addMgmtEventCallback(dev_id, MgmtEvent::Opcode::DISCOVERING, jau::bindMemberFunc(this, &DBTAdapter::mgmtEvDeviceDiscoveringMgmt)) && ok;
    ok = mgmt.addMgmtEventCallback(dev_id, MgmtEvent::Opcode::NEW_SETTINGS, jau::bindMemberFunc(this, &DBTAdapter::mgmtEvNewSettingsMgmt)) && ok;
    ok = mgmt.addMgmtEventCallback(dev_id, MgmtEvent::Opcode::LOCAL_NAME_CHANGED, jau::bindMemberFunc(this, &DBTAdapter::mgmtEvLocalNameChangedMgmt)) && ok;
    ok = mgmt.addMgmtEventCallback(dev_id, MgmtEvent::Opcode::PIN_CODE_REQUEST, jau::bindMemberFunc(this, &DBTAdapter::mgmtEvPinCodeRequestMgmt));
    ok = mgmt.addMgmtEventCallback(dev_id, MgmtEvent::Opcode::USER_CONFIRM_REQUEST, jau::bindMemberFunc(this, &DBTAdapter::mgmtEvUserConfirmRequestMgmt));
    ok = mgmt.addMgmtEventCallback(dev_id, MgmtEvent::Opcode::USER_PASSKEY_REQUEST, jau::bindMemberFunc(this, &DBTAdapter::mgmtEvUserPasskeyRequestMgmt));
    ok = mgmt.addMgmtEventCallback(dev_id, MgmtEvent::Opcode::AUTH_FAILED, jau::bindMemberFunc(this, &DBTAdapter::mgmtEvAuthFailedMgmt));
    ok = mgmt.addMgmtEventCallback(dev_id, MgmtEvent::Opcode::DEVICE_UNPAIRED, jau::bindMemberFunc(this, &DBTAdapter::mgmtEvDeviceUnpairedMgmt));

    if( !ok ) {
        ERR_PRINT("Could not add all required MgmtEventCallbacks to DBTManager: %s", toString().c_str());
        return false;
    }

#if 0
    mgmt.addMgmtEventCallback(dev_id, MgmtEvent::Opcode::DEVICE_DISCONNECTED, bindMemberFunc(this, &DBTAdapter::mgmtEvDeviceDisconnectedMgmt));
#endif

    ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::DISCOVERING, jau::bindMemberFunc(this, &DBTAdapter::mgmtEvDeviceDiscoveringHCI)) && ok;
    ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::DEVICE_CONNECTED, jau::bindMemberFunc(this, &DBTAdapter::mgmtEvDeviceConnectedHCI)) && ok;
    ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::CONNECT_FAILED, jau::bindMemberFunc(this, &DBTAdapter::mgmtEvConnectFailedHCI)) && ok;
    ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::DEVICE_DISCONNECTED, jau::bindMemberFunc(this, &DBTAdapter::mgmtEvDeviceDisconnectedHCI)) && ok;
    ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::DEVICE_FOUND, jau::bindMemberFunc(this, &DBTAdapter::mgmtEvDeviceFoundHCI)) && ok;
    ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::LE_REMOTE_USER_FEATURES, jau::bindMemberFunc(this, &DBTAdapter::mgmtEvLERemoteUserFeaturesHCI)) && ok;

    if( !ok ) {
        ERR_PRINT("Could not add all required MgmtEventCallbacks to HCIHandler: %s of %s", hci.toString().c_str(), toString().c_str());
        return false; // dtor local HCIHandler w/ closing
    }
    hci.addSMPMsgCallback(jau::bindMemberFunc(this, &DBTAdapter::hciSMPMsgCallback));

    return true;

errout0:
    adapterInfo = std::shared_ptr<AdapterInfo>( new AdapterInfo(dev_id, EUI48_ANY_DEVICE, 0, 0,
                            AdapterSetting::NONE, AdapterSetting::NONE, 0, "invalid", "invalid"));
    return false;
}

DBTAdapter::DBTAdapter() noexcept
: debug_event(jau::environment::getBooleanProperty("direct_bt.debug.adapter.event", false)),
  mgmt( DBTManager::get(BTMode::NONE /* use env default */) ),
  dev_id( mgmt.getDefaultAdapterDevID() ),
  hci( dev_id )
{
    valid = validateDevInfo();
}

DBTAdapter::DBTAdapter(EUI48 &mac) noexcept
: debug_event(jau::environment::getBooleanProperty("direct_bt.debug.adapter.event", false)),
  mgmt( DBTManager::get(BTMode::NONE /* use env default */) ),
  dev_id( mgmt.findAdapterInfoDevId(mac) ),
  hci( dev_id )
{
    valid = validateDevInfo();
}

DBTAdapter::DBTAdapter(const int _dev_id) noexcept
: debug_event(jau::environment::getBooleanProperty("direct_bt.debug.adapter.event", false)),
  mgmt( DBTManager::get(BTMode::NONE /* use env default */) ),
  dev_id( 0 <= _dev_id ? _dev_id : mgmt.getDefaultAdapterDevID() ),
  hci( dev_id )
{
    valid = validateDevInfo();
}

DBTAdapter::~DBTAdapter() noexcept {
    if( !isValid() ) {
        DBG_PRINT("DBTAdapter::dtor: dev_id %d, invalid, %p", dev_id, this);
        return;
    }
    DBG_PRINT("DBTAdapter::dtor: ... %p %s", this, toString().c_str());
    close();
    DBG_PRINT("DBTAdapter::dtor: XXX");
}

void DBTAdapter::close() noexcept {
    if( !isValid() ) {
        // Native user app could have destroyed this instance already from
        DBG_PRINT("DBTAdapter::close: dev_id %d, invalid, %p", dev_id, this);
        return;
    }
    DBG_PRINT("DBTAdapter::close: ... %p %s", this, toString().c_str());
    keep_le_scan_alive = false;

    // mute all listener first
    {
        int count = mgmt.removeMgmtEventCallback(dev_id);
        DBG_PRINT("DBTAdapter::close removeMgmtEventCallback: %d callbacks", count);
    }
    statusListenerList.clear();

    poweredOff();

    DBG_PRINT("DBTAdapter::close: closeHCI: ...");
    hci.close();
    DBG_PRINT("DBTAdapter::close: closeHCI: XXX");

    {
        const std::lock_guard<std::mutex> lock(mtx_discoveredDevices); // RAII-style acquire and relinquish via destructor
        discoveredDevices.clear();
    }
    {
        const std::lock_guard<std::mutex> lock(mtx_connectedDevices); // RAII-style acquire and relinquish via destructor
        connectedDevices.clear();;
    }
    {
        const std::lock_guard<std::mutex> lock(mtx_sharedDevices); // RAII-style acquire and relinquish via destructor
        sharedDevices.clear();
    }
    valid = false;
    DBG_PRINT("DBTAdapter::close: XXX");
}

void DBTAdapter::poweredOff() noexcept {
    if( !isValid() ) {
        DBG_PRINT("DBTAdapter::poweredOff: dev_id %d, invalid, %p", dev_id, this);
        return;
    }
    DBG_PRINT("DBTAdapter::poweredOff: ... %p %s", this, toString(false).c_str());
    keep_le_scan_alive = false;

    stopDiscovery();

    // Removes all device references from the lists: connectedDevices, discoveredDevices, sharedDevices
    disconnectAllDevices();
    removeDiscoveredDevices();

    hci.setCurrentScanType(ScanType::NONE);
    currentMetaScanType = ScanType::NONE;

    DBG_PRINT("DBTAdapter::poweredOff: XXX");
}

void DBTAdapter::printSharedPtrListOfDevices() noexcept {
    {
        const std::lock_guard<std::mutex> lock0(mtx_sharedDevices);
        jau::printSharedPtrList("SharedDevices", sharedDevices);
    }
    {
        const std::lock_guard<std::mutex> lock0(mtx_discoveredDevices);
        jau::printSharedPtrList("DiscoveredDevices", discoveredDevices);
    }
    {
        const std::lock_guard<std::mutex> lock0(mtx_connectedDevices);
        jau::printSharedPtrList("ConnectedDevices", connectedDevices);
    }
}

std::shared_ptr<NameAndShortName> DBTAdapter::setLocalName(const std::string &name, const std::string &short_name) noexcept {
    return mgmt.setLocalName(dev_id, name, short_name);
}

bool DBTAdapter::setDiscoverable(bool value) noexcept {
    AdapterSetting current_settings;
    return MgmtStatus::SUCCESS == mgmt.setDiscoverable(dev_id, value ? 0x01 : 0x00, 10 /* timeout seconds */, current_settings);
}

bool DBTAdapter::setBondable(bool value) noexcept {
    AdapterSetting current_settings;
    return mgmt.setMode(dev_id, MgmtCommand::Opcode::SET_BONDABLE, value ? 1 : 0, current_settings);
}

bool DBTAdapter::setPowered(bool value) noexcept {
    AdapterSetting current_settings;
    return mgmt.setMode(dev_id, MgmtCommand::Opcode::SET_POWERED, value ? 1 : 0, current_settings);
}

HCIStatusCode DBTAdapter::reset() noexcept {
    if( !isValid() ) {
        ERR_PRINT("DBTAdapter::reset(): Adapter invalid: %s, %s", aptrHexString(this).c_str(), toString().c_str());
        return HCIStatusCode::UNSPECIFIED_ERROR;
    }
    if( !hci.isOpen() ) {
        ERR_PRINT("DBTAdapter::reset(): HCI closed: %s, %s", aptrHexString(this).c_str(), toString().c_str());
        return HCIStatusCode::UNSPECIFIED_ERROR;
    }
#if 0
    const bool wasPowered = isPowered();
    // Adapter will be reset, close connections and cleanup off-thread.
    preReset();

    HCIStatusCode status = hci->reset();
    if( HCIStatusCode::SUCCESS != status ) {
        ERR_PRINT("DBTAdapter::reset: reset failed: %s", getHCIStatusCodeString(status).c_str());
    } else if( wasPowered ) {
        if( !setPowered(true) ) {
            ERR_PRINT("DBTAdapter::reset: setPowered(true) failed");
            status = HCIStatusCode::UNSPECIFIED_ERROR;
        }
    }
    return status;
#else
    return hci.resetAdapter();
#endif
}

bool DBTAdapter::isDeviceWhitelisted(const EUI48 &address) noexcept {
    return mgmt.isDeviceWhitelisted(dev_id, address);
}

bool DBTAdapter::addDeviceToWhitelist(const EUI48 &address, const BDAddressType address_type, const HCIWhitelistConnectType ctype,
                                      const uint16_t conn_interval_min, const uint16_t conn_interval_max,
                                      const uint16_t conn_latency, const uint16_t timeout) {
    if( !isPowered() ) {
        ERR_PRINT("DBTAdapter::startDiscovery: Adapter not powered: %s", toString().c_str());
        return false;
    }
    if( mgmt.isDeviceWhitelisted(dev_id, address) ) {
        ERR_PRINT("DBTAdapter::addDeviceToWhitelist: device already listed: dev_id %d, address %s", dev_id, address.toString().c_str());
        return true;
    }

    if( !mgmt.uploadConnParam(dev_id, address, address_type, conn_interval_min, conn_interval_max, conn_latency, timeout) ) {
        ERR_PRINT("DBTAdapter::addDeviceToWhitelist: uploadConnParam(dev_id %d, address %s, interval[%u..%u], latency %u, timeout %u): Failed",
                dev_id, address.toString().c_str(), conn_interval_min, conn_interval_max, conn_latency, timeout);
    }
    return mgmt.addDeviceToWhitelist(dev_id, address, address_type, ctype);
}

bool DBTAdapter::removeDeviceFromWhitelist(const EUI48 &address, const BDAddressType address_type) {
    return mgmt.removeDeviceFromWhitelist(dev_id, address, address_type);
}

static jau::cow_vector<std::shared_ptr<AdapterStatusListener>>::equal_comparator _adapterStatusListenerRefEqComparator =
        [](const std::shared_ptr<AdapterStatusListener> &a, const std::shared_ptr<AdapterStatusListener> &b) -> bool { return *a == *b; };

bool DBTAdapter::addStatusListener(std::shared_ptr<AdapterStatusListener> l) {
    if( nullptr == l ) {
        throw jau::IllegalArgumentException("DBTAdapterStatusListener ref is null", E_FILE_LINE);
    }
    const bool added = statusListenerList.push_back_unique(l, _adapterStatusListenerRefEqComparator);
    if( added ) {
        sendAdapterSettingsInitial(*l, jau::getCurrentMilliseconds());
    }
    return true;
}

bool DBTAdapter::removeStatusListener(std::shared_ptr<AdapterStatusListener> l) {
    if( nullptr == l ) {
        throw jau::IllegalArgumentException("DBTAdapterStatusListener ref is null", E_FILE_LINE);
    }
    const int count = statusListenerList.erase_matching(l, false /* all_matching */, _adapterStatusListenerRefEqComparator);
    return count > 0;
}

bool DBTAdapter::removeStatusListener(const AdapterStatusListener * l) {
    if( nullptr == l ) {
        throw jau::IllegalArgumentException("DBTAdapterStatusListener ref is null", E_FILE_LINE);
    }
    const std::lock_guard<std::recursive_mutex> lock(statusListenerList.get_write_mutex());
    std::shared_ptr<std::vector<std::shared_ptr<AdapterStatusListener>>> store = statusListenerList.copy_store();
    int count = 0;
    for(auto it = store->begin(); it != store->end(); ) {
        if ( **it == *l ) {
            it = store->erase(it);
            count++;
            break;
        } else {
            ++it;
        }
    }
    if( 0 < count ) {
        statusListenerList.set_store(std::move(store));
        return true;
    }
    return false;
}

int DBTAdapter::removeAllStatusListener() {
    int count = statusListenerList.size();
    statusListenerList.clear();
    return count;
}

void DBTAdapter::checkDiscoveryState() noexcept {
    const ScanType currentNativeScanType = hci.getCurrentScanType();
    // Check LE scan state
    if( keep_le_scan_alive == false ) {
        if( hasScanType(currentMetaScanType, ScanType::LE) != hasScanType(currentNativeScanType, ScanType::LE) ) {
            std::string msg("Invalid DiscoveryState: keepAlive "+std::to_string(keep_le_scan_alive.load())+
                    ", currentScanType*[native "+
                    getScanTypeString(currentNativeScanType)+" != meta "+
                    getScanTypeString(currentMetaScanType)+"]");
            ERR_PRINT(msg.c_str());
            // ABORT?
        }
    } else {
        if( !hasScanType(currentMetaScanType, ScanType::LE) && hasScanType(currentNativeScanType, ScanType::LE) ) {
            std::string msg("Invalid DiscoveryState: keepAlive "+std::to_string(keep_le_scan_alive.load())+
                    ", currentScanType*[native "+
                    getScanTypeString(currentNativeScanType)+", meta "+
                    getScanTypeString(currentMetaScanType)+"]");
            ERR_PRINT(msg.c_str());
            // ABORT?
        }
    }
}

HCIStatusCode DBTAdapter::startDiscovery(const bool keepAlive, const HCILEOwnAddressType own_mac_type,
                                         const uint16_t le_scan_interval, const uint16_t le_scan_window)
{
    // FIXME: Respect DBTAdapter::btMode, i.e. BTMode::BREDR, BTMode::LE or BTMode::DUAL to setup BREDR, LE or DUAL scanning!
    // ERR_PRINT("Test");
    // throw jau::RuntimeException("Test", E_FILE_LINE);

    if( !isPowered() ) {
        WARN_PRINT("DBTAdapter::startDiscovery: Adapter not powered: %s", toString().c_str());
        return HCIStatusCode::UNSPECIFIED_ERROR;
    }
    const std::lock_guard<std::mutex> lock(mtx_discovery); // RAII-style acquire and relinquish via destructor

    const ScanType currentNativeScanType = hci.getCurrentScanType();

    if( hasScanType(currentNativeScanType, ScanType::LE) ) {
        removeDiscoveredDevices();
        if( keep_le_scan_alive == keepAlive ) {
            DBG_PRINT("DBTAdapter::startDiscovery: Already discovering, unchanged keepAlive %d -> %d, currentScanType[native %s, meta %s] ...",
                    keep_le_scan_alive.load(), keepAlive,
                    getScanTypeString(currentNativeScanType).c_str(), getScanTypeString(currentMetaScanType).c_str());
        } else {
            DBG_PRINT("DBTAdapter::startDiscovery: Already discovering, changed keepAlive %d -> %d, currentScanType[native %s, meta %s] ...",
                    keep_le_scan_alive.load(), keepAlive,
                    getScanTypeString(currentNativeScanType).c_str(), getScanTypeString(currentMetaScanType).c_str());
            keep_le_scan_alive = keepAlive;
        }
        checkDiscoveryState();
        return HCIStatusCode::SUCCESS;
    }

    DBG_PRINT("DBTAdapter::startDiscovery: Start: keepAlive %d -> %d, currentScanType[native %s, meta %s] ...",
            keep_le_scan_alive.load(), keepAlive,
            getScanTypeString(currentNativeScanType).c_str(), getScanTypeString(currentMetaScanType).c_str());

    removeDiscoveredDevices();
    keep_le_scan_alive = keepAlive;

    // if le_enable_scan(..) is successful, it will issue 'mgmtEvDeviceDiscoveringHCI(..)' immediately, which updates currentMetaScanType.
    const HCIStatusCode status = hci.le_start_scan(true /* filter_dup */, own_mac_type, le_scan_interval, le_scan_window);

    DBG_PRINT("DBTAdapter::startDiscovery: End: Result %s, keepAlive %d -> %d, currentScanType[native %s, meta %s] ...",
            getHCIStatusCodeString(status).c_str(), keep_le_scan_alive.load(), keepAlive,
            getScanTypeString(hci.getCurrentScanType()).c_str(), getScanTypeString(currentMetaScanType).c_str());

    checkDiscoveryState();

    return status;
}

void DBTAdapter::startDiscoveryBackground() noexcept {
    // FIXME: Respect DBTAdapter::btMode, i.e. BTMode::BREDR, BTMode::LE or BTMode::DUAL to setup BREDR, LE or DUAL scanning!
    if( !isPowered() ) {
        WARN_PRINT("DBTAdapter::startDiscoveryBackground: Adapter not powered: %s", toString().c_str());
        return;
    }
    const std::lock_guard<std::mutex> lock(mtx_discovery); // RAII-style acquire and relinquish via destructor
    if( !hasScanType(hci.getCurrentScanType(), ScanType::LE) && keep_le_scan_alive ) { // still?
        // if le_enable_scan(..) is successful, it will issue 'mgmtEvDeviceDiscoveringHCI(..)' immediately, which updates currentMetaScanType.
        const HCIStatusCode status = hci.le_enable_scan(true /* enable */);
        if( HCIStatusCode::SUCCESS != status ) {
            ERR_PRINT("DBTAdapter::startDiscoveryBackground: le_enable_scan failed: %s", getHCIStatusCodeString(status).c_str());
        }
        checkDiscoveryState();
    }
}

HCIStatusCode DBTAdapter::stopDiscovery() noexcept {
    // We allow !isEnabled, to utilize method for adjusting discovery state and notifying listeners
    // FIXME: Respect DBTAdapter::btMode, i.e. BTMode::BREDR, BTMode::LE or BTMode::DUAL to stop BREDR, LE or DUAL scanning!
    const std::lock_guard<std::mutex> lock(mtx_discovery); // RAII-style acquire and relinquish via destructor
    /**
     * Need to send mgmtEvDeviceDiscoveringMgmt(..)
     * as manager/hci won't produce such event having temporarily disabled discovery.
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
     * [4] current -> [5] post stopDiscovery == sendEvent
     */
    const ScanType currentNativeScanType = hci.getCurrentScanType();
    const bool le_scan_temp_disabled = hasScanType(currentMetaScanType, ScanType::LE) &&    // true
                                       !hasScanType(currentNativeScanType, ScanType::LE) && // false
                                       keep_le_scan_alive;                                  // true

    DBG_PRINT("DBTAdapter::stopDiscovery: Start: keepAlive %d, currentScanType[native %s, meta %s], le_scan_temp_disabled %d ...",
            keep_le_scan_alive.load(),
            getScanTypeString(currentNativeScanType).c_str(), getScanTypeString(currentMetaScanType).c_str(),
            le_scan_temp_disabled);

    keep_le_scan_alive = false;
    if( !hasScanType(currentMetaScanType, ScanType::LE) ) {
        DBG_PRINT("DBTAdapter::stopDiscovery: Already disabled, keepAlive %d, currentScanType[native %s, meta %s] ...",
                keep_le_scan_alive.load(),
                getScanTypeString(currentNativeScanType).c_str(), getScanTypeString(currentMetaScanType).c_str());
        checkDiscoveryState();
        return HCIStatusCode::SUCCESS;
    }

    HCIStatusCode status;
    if( !adapterInfo->isCurrentSettingBitSet(AdapterSetting::POWERED) ) {
        WARN_PRINT("DBTAdapter::stopDiscovery: Powered off: %s", toString().c_str());
        hci.setCurrentScanType(ScanType::NONE);
        currentMetaScanType = ScanType::NONE;
        status = HCIStatusCode::UNSPECIFIED_ERROR;
        goto exit;
    }
    if( !hci.isOpen() ) {
        ERR_PRINT("DBTAdapter::stopDiscovery: HCI closed: %s", toString().c_str());
        status = HCIStatusCode::UNSPECIFIED_ERROR;
        goto exit;
    }

    if( le_scan_temp_disabled ) {
        // meta state transition [4] -> [5], w/o native disabling
        // Will issue 'mgmtEvDeviceDiscoveringHCI(..)' immediately, which updates currentMetaScanType
        // currentMetaScanType = changeScanType(currentMetaScanType, false, ScanType::LE);
        status = HCIStatusCode::SUCCESS; // send event: discoveryTempDisabled
    } else {
        // if le_enable_scan(..) is successful, it will issue 'mgmtEvDeviceDiscoveringHCI(..)' immediately, which updates currentMetaScanType.
        status = hci.le_enable_scan(false /* enable */);
        if( HCIStatusCode::SUCCESS != status ) {
            ERR_PRINT("DBTAdapter::stopDiscovery: le_enable_scan failed: %s", getHCIStatusCodeString(status).c_str());
        }
    }

exit:
    if( le_scan_temp_disabled || HCIStatusCode::SUCCESS != status ) {
        // In case of discoveryTempDisabled, power-off, le_enable_scane failure
        // or already closed HCIHandler, send the event directly.
        mgmtEvDeviceDiscoveringHCI( std::shared_ptr<MgmtEvent>( new MgmtEvtDiscovering(dev_id, ScanType::LE, false) ) );
    }
    DBG_PRINT("DBTAdapter::stopDiscovery: End: Result %s, keepAlive %d, currentScanType[native %s, meta %s], le_scan_temp_disabled %d ...",
            getHCIStatusCodeString(status).c_str(), keep_le_scan_alive.load(),
            getScanTypeString(hci.getCurrentScanType()).c_str(), getScanTypeString(currentMetaScanType).c_str(), le_scan_temp_disabled);

    checkDiscoveryState();

    return status;
}

// *************************************************

std::shared_ptr<DBTDevice> DBTAdapter::findDiscoveredDevice (EUI48 const & mac, const BDAddressType macType) noexcept {
    const std::lock_guard<std::mutex> lock(const_cast<DBTAdapter*>(this)->mtx_discoveredDevices); // RAII-style acquire and relinquish via destructor
    return findDevice(discoveredDevices, mac, macType);
}

bool DBTAdapter::addDiscoveredDevice(std::shared_ptr<DBTDevice> const &device) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_discoveredDevices); // RAII-style acquire and relinquish via destructor
    if( nullptr != findDevice(discoveredDevices, *device) ) {
        // already discovered
        return false;
    }
    discoveredDevices.push_back(device);
    return true;
}

bool DBTAdapter::removeDiscoveredDevice(const DBTDevice & device) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_discoveredDevices); // RAII-style acquire and relinquish via destructor
    for (auto it = discoveredDevices.begin(); it != discoveredDevices.end(); ) {
        if ( nullptr != *it && device == **it ) {
            it = discoveredDevices.erase(it);
            return true;
        } else {
            ++it;
        }
    }
    return false;
}


int DBTAdapter::removeDiscoveredDevices() noexcept {
    const std::lock_guard<std::mutex> lock(mtx_discoveredDevices); // RAII-style acquire and relinquish via destructor
    int res = discoveredDevices.size();
    discoveredDevices.clear();
    return res;
}

std::vector<std::shared_ptr<DBTDevice>> DBTAdapter::getDiscoveredDevices() const noexcept {
    const std::lock_guard<std::mutex> lock(const_cast<DBTAdapter*>(this)->mtx_discoveredDevices); // RAII-style acquire and relinquish via destructor
    std::vector<std::shared_ptr<DBTDevice>> res = discoveredDevices;
    return res;
}

bool DBTAdapter::addSharedDevice(std::shared_ptr<DBTDevice> const &device) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_sharedDevices); // RAII-style acquire and relinquish via destructor
    if( nullptr != findDevice(sharedDevices, *device) ) {
        // already shared
        return false;
    }
    sharedDevices.push_back(device);
    return true;
}

std::shared_ptr<DBTDevice> DBTAdapter::getSharedDevice(const DBTDevice & device) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_sharedDevices); // RAII-style acquire and relinquish via destructor
    return findDevice(sharedDevices, device);
}

void DBTAdapter::removeSharedDevice(const DBTDevice & device) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_sharedDevices); // RAII-style acquire and relinquish via destructor
    for (auto it = sharedDevices.begin(); it != sharedDevices.end(); ) {
        if ( nullptr != *it && device == **it ) {
            it = sharedDevices.erase(it);
            return; // unique set
        } else {
            ++it;
        }
    }
}

std::shared_ptr<DBTDevice> DBTAdapter::findSharedDevice (EUI48 const & mac, const BDAddressType macType) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_sharedDevices); // RAII-style acquire and relinquish via destructor
    return findDevice(sharedDevices, mac, macType);
}

void DBTAdapter::removeDevice(DBTDevice & device) noexcept {
    WORDY_PRINT("DBTAdapter::removeDevice: Start %s", toString(false).c_str());
    const HCIStatusCode status = device.disconnect(HCIStatusCode::REMOTE_USER_TERMINATED_CONNECTION);
    WORDY_PRINT("DBTAdapter::removeDevice: disconnect %s, %s", getHCIStatusCodeString(status).c_str(), toString(false).c_str());
    removeConnectedDevice(device); // usually done in DBTAdapter::mgmtEvDeviceDisconnectedHCI
    removeDiscoveredDevice(device); // usually done in DBTAdapter::mgmtEvDeviceDisconnectedHCI
    WORDY_PRINT("DBTAdapter::removeDevice: End %s", toString(false).c_str());
    removeSharedDevice(device);
}

std::string DBTAdapter::toString(bool includeDiscoveredDevices) const noexcept {
    std::string out("Adapter[BTMode "+getBTModeString(btMode)+", "+getAddressString()+", '"+getName()+"', id "+std::to_string(dev_id)+
                    ", curSettings"+getAdapterSettingMaskString(adapterInfo->getCurrentSettingMask())+
                    ", scanType[native "+getScanTypeString(hci.getCurrentScanType())+", meta "+getScanTypeString(currentMetaScanType)+"]"
                    ", valid "+std::to_string(isValid())+", open[mgmt, "+std::to_string(mgmt.isOpen())+", hci "+std::to_string(hci.isOpen())+
                    "], "+javaObjectToString()+"]");
    std::vector<std::shared_ptr<DBTDevice>> devices = getDiscoveredDevices();
    if( includeDiscoveredDevices && devices.size() > 0 ) {
        out.append("\n");
        for(auto it = devices.begin(); it != devices.end(); it++) {
            std::shared_ptr<DBTDevice> p = *it;
            if( nullptr != p ) {
                out.append("  ").append(p->toString()).append("\n");
            }
        }
    }
    return out;
}

// *************************************************

void DBTAdapter::sendAdapterSettingsChanged(const AdapterSetting old_settings_, const AdapterSetting current_settings,
                                            const uint64_t timestampMS) noexcept
{
    AdapterSetting changes = getAdapterSettingMaskDiff(current_settings, old_settings_);
    COND_PRINT(debug_event, "DBTAdapter::sendAdapterSettingsChanged: %s -> %s, changes %s: %s",
            getAdapterSettingMaskString(old_settings_).c_str(),
            getAdapterSettingMaskString(current_settings).c_str(),
            getAdapterSettingMaskString(changes).c_str(), toString(false).c_str() );
    int i=0;
    jau::for_each_cow(statusListenerList, [&](std::shared_ptr<AdapterStatusListener> &l) {
        try {
            l->adapterSettingsChanged(*this, old_settings_, current_settings, changes, timestampMS);
        } catch (std::exception &e) {
            ERR_PRINT("DBTAdapter:CB:NewSettings-CBs %d/%zd: %s of %s: Caught exception %s",
                    i+1, statusListenerList.size(),
                    l->toString().c_str(), toString(false).c_str(), e.what());
        }
        i++;
    });
}

void DBTAdapter::sendAdapterSettingsInitial(AdapterStatusListener & asl, const uint64_t timestampMS) noexcept
{
    const AdapterSetting current_settings = adapterInfo->getCurrentSettingMask();
    COND_PRINT(debug_event, "DBTAdapter::sendAdapterSettingsInitial: NONE -> %s, changes NONE: %s",
            getAdapterSettingMaskString(current_settings).c_str(), toString(false).c_str() );
    try {
        asl.adapterSettingsChanged(*this, AdapterSetting::NONE, current_settings, AdapterSetting::NONE, timestampMS);
    } catch (std::exception &e) {
        ERR_PRINT("DBTAdapter::sendAdapterSettingsChanged-CB: %s of %s: Caught exception %s",
                asl.toString().c_str(), toString(false).c_str(), e.what());
    }
}

void DBTAdapter::sendDeviceUpdated(std::string cause, std::shared_ptr<DBTDevice> device, uint64_t timestamp, EIRDataType updateMask) noexcept {
    int i=0;
    jau::for_each_cow(statusListenerList, [&](std::shared_ptr<AdapterStatusListener> &l) {
        try {
            if( l->matchDevice(*device) ) {
                l->deviceUpdated(device, updateMask, timestamp);
            }
        } catch (std::exception &e) {
            ERR_PRINT("DBTAdapter::sendDeviceUpdated-CBs (%s) %d/%zd: %s of %s: Caught exception %s",
                    cause.c_str(), i+1, statusListenerList.size(),
                    l->toString().c_str(), device->toString().c_str(), e.what());
        }
        i++;
    });
}

// *************************************************

bool DBTAdapter::mgmtEvDeviceDiscoveringHCI(std::shared_ptr<MgmtEvent> e) noexcept {
    return mgmtEvDeviceDiscoveringAny(e, true /* hciSourced */ );
}

bool DBTAdapter::mgmtEvDeviceDiscoveringMgmt(std::shared_ptr<MgmtEvent> e) noexcept {
    return mgmtEvDeviceDiscoveringAny(e, false /* hciSourced */ );
}

bool DBTAdapter::mgmtEvDeviceDiscoveringAny(std::shared_ptr<MgmtEvent> e, const bool hciSourced) noexcept {
    const std::string srctkn = hciSourced ? "hci" : "mgmt";
    const MgmtEvtDiscovering &event = *static_cast<const MgmtEvtDiscovering *>(e.get());
    const ScanType eventScanType = event.getScanType();
    const bool eventEnabled = event.getEnabled();
    ScanType currentNativeScanType = hci.getCurrentScanType();

    // FIXME: Respect DBTAdapter::btMode, i.e. BTMode::BREDR, BTMode::LE or BTMode::DUAL to setup BREDR, LE or DUAL scanning!
    //
    // Also catches case where discovery changes w/o user interaction [start/stop]Discovery(..)
    // if sourced from mgmt channel (!hciSourced)

    ScanType nextMetaScanType;
    if( eventEnabled ) {
        // enabled eventScanType
        nextMetaScanType = changeScanType(currentMetaScanType, eventScanType, true);
    } else {
        // disabled eventScanType
        if( hasScanType(eventScanType, ScanType::LE) && keep_le_scan_alive ) {
            // Unchanged meta for disabled-LE && keep_le_scan_alive
            nextMetaScanType = currentMetaScanType;
        } else {
            nextMetaScanType = changeScanType(currentMetaScanType, eventScanType, false);
        }
    }

    if( !hciSourced ) {
        // update HCIHandler's currentNativeScanType from other source
        const ScanType nextNativeScanType = changeScanType(currentNativeScanType, eventScanType, eventEnabled);
        DBG_PRINT("DBTAdapter:%s:DeviceDiscovering: dev_id %d, keepDiscoveringAlive %d: scanType[native %s -> %s, meta %s -> %s]): %s",
            srctkn.c_str(), dev_id, keep_le_scan_alive.load(),
            getScanTypeString(currentNativeScanType).c_str(), getScanTypeString(nextNativeScanType).c_str(),
            getScanTypeString(currentMetaScanType).c_str(), getScanTypeString(nextMetaScanType).c_str(),
            event.toString().c_str());
        currentNativeScanType = nextNativeScanType;
        hci.setCurrentScanType(currentNativeScanType);
    } else {
        DBG_PRINT("DBTAdapter:%s:DeviceDiscovering: dev_id %d, keepDiscoveringAlive %d: scanType[native %s, meta %s -> %s]): %s",
            srctkn.c_str(), dev_id, keep_le_scan_alive.load(),
            getScanTypeString(currentNativeScanType).c_str(),
            getScanTypeString(currentMetaScanType).c_str(), getScanTypeString(nextMetaScanType).c_str(),
            event.toString().c_str());
    }
    currentMetaScanType = nextMetaScanType;

    checkDiscoveryState();

    int i=0;
    jau::for_each_cow(statusListenerList, [&](std::shared_ptr<AdapterStatusListener> &l) {
        try {
            l->discoveringChanged(*this, currentMetaScanType, eventScanType, eventEnabled, keep_le_scan_alive, event.getTimestamp());
        } catch (std::exception &except) {
            ERR_PRINT("DBTAdapter:%s:DeviceDiscovering-CBs %d/%zd: %s of %s: Caught exception %s",
                    srctkn.c_str(), i+1, statusListenerList.size(),
                    l->toString().c_str(), toString().c_str(), except.what());
        }
        i++;
    });

    if( !hasScanType(currentNativeScanType, ScanType::LE) && keep_le_scan_alive ) {
        std::thread bg(&DBTAdapter::startDiscoveryBackground, this); // @suppress("Invalid arguments")
        bg.detach();
    }
    return true;
}

bool DBTAdapter::mgmtEvNewSettingsMgmt(std::shared_ptr<MgmtEvent> e) noexcept {
    COND_PRINT(debug_event, "DBTAdapter:mgmt:NewSettings: %s", e->toString().c_str());
    const MgmtEvtNewSettings &event = *static_cast<const MgmtEvtNewSettings *>(e.get());
    const AdapterSetting new_settings = adapterInfo->setCurrentSettingMask(event.getSettings()); // probably done by mgmt callback already
    {
        const BTMode _btMode = getAdapterSettingsBTMode(new_settings);
        if( BTMode::NONE != _btMode ) {
            btMode = _btMode;
        }
    }
    sendAdapterSettingsChanged(old_settings, new_settings, event.getTimestamp());

    old_settings = new_settings;

    if( !isAdapterSettingBitSet(new_settings, AdapterSetting::POWERED) ) {
        // Adapter has been powered off, close connections and cleanup off-thread.
        std::thread bg(&DBTAdapter::poweredOff, this); // @suppress("Invalid arguments")
        bg.detach();
    }

    return true;
}

bool DBTAdapter::mgmtEvLocalNameChangedMgmt(std::shared_ptr<MgmtEvent> e) noexcept {
    COND_PRINT(debug_event, "DBTAdapter:mgmt:LocalNameChanged: %s", e->toString().c_str());
    const MgmtEvtLocalNameChanged &event = *static_cast<const MgmtEvtLocalNameChanged *>(e.get());
    std::string old_name = localName.getName();
    std::string old_shortName = localName.getShortName();
    bool nameChanged = old_name != event.getName();
    bool shortNameChanged = old_shortName != event.getShortName();
    if( nameChanged ) {
        localName.setName(event.getName());
    }
    if( shortNameChanged ) {
        localName.setShortName(event.getShortName());
    }
    COND_PRINT(debug_event, "DBTAdapter:mgmt:LocalNameChanged: Local name: %d: '%s' -> '%s'; short_name: %d: '%s' -> '%s'",
            nameChanged, old_name.c_str(), localName.getName().c_str(),
            shortNameChanged, old_shortName.c_str(), localName.getShortName().c_str());
    (void)nameChanged;
    (void)shortNameChanged;
    return true;
}

bool DBTAdapter::mgmtEvDeviceConnectedHCI(std::shared_ptr<MgmtEvent> e) noexcept {
    COND_PRINT(debug_event, "DBTAdapter:hci:DeviceConnected(dev_id %d): %s", dev_id, e->toString().c_str());
    const MgmtEvtDeviceConnected &event = *static_cast<const MgmtEvtDeviceConnected *>(e.get());
    EInfoReport ad_report;
    {
        ad_report.setSource(EInfoReport::Source::EIR);
        ad_report.setTimestamp(event.getTimestamp());
        ad_report.setAddressType(event.getAddressType());
        ad_report.setAddress( event.getAddress() );
        ad_report.read_data(event.getData(), event.getDataSize());
    }
    int new_connect = 0;
    std::shared_ptr<DBTDevice> device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr == device ) {
        device = findDiscoveredDevice(event.getAddress(), event.getAddressType());
        new_connect = nullptr != device ? 1 : 0;
    }
    if( nullptr == device ) {
        device = findSharedDevice(event.getAddress(), event.getAddressType());
        if( nullptr != device ) {
            addDiscoveredDevice(device);
            new_connect = 2;
        }
    }
    if( nullptr == device ) {
        // a whitelist auto-connect w/o previous discovery
        device = std::shared_ptr<DBTDevice>(new DBTDevice(*this, ad_report));
        addDiscoveredDevice(device);
        addSharedDevice(device);
        new_connect = 3;
    }

    EIRDataType updateMask = device->update(ad_report);
    if( addConnectedDevice(device) ) { // track device, if not done yet
        if( 0 == new_connect ) {
            new_connect = 4; // unknown reason...
        }
    }
    if( 2 <= new_connect ) {
        device->ts_last_discovery = ad_report.getTimestamp();
    }
    if( 0 == new_connect ) {
        WARN_PRINT("DBTAdapter::EventHCI:DeviceConnected(dev_id %d, already connected, updated %s): %s, handle %s -> %s,\n    %s,\n    -> %s",
            dev_id, getEIRDataMaskString(updateMask).c_str(), event.toString().c_str(),
            jau::uint16HexString(device->getConnectionHandle()).c_str(), jau::uint16HexString(event.getHCIHandle()).c_str(),
            ad_report.toString().c_str(),
            device->toString().c_str());
    } else {
        COND_PRINT(debug_event, "DBTAdapter::EventHCI:DeviceConnected(dev_id %d, new_connect %d, updated %s): %s, handle %s -> %s,\n    %s,\n    -> %s",
            dev_id, new_connect, getEIRDataMaskString(updateMask).c_str(), event.toString().c_str(),
            jau::uint16HexString(device->getConnectionHandle()).c_str(), jau::uint16HexString(event.getHCIHandle()).c_str(),
            ad_report.toString().c_str(),
            device->toString().c_str());
    }

    device->notifyConnected(event.getHCIHandle());

    int i=0;
    jau::for_each_cow(statusListenerList, [&](std::shared_ptr<AdapterStatusListener> &l) {
        try {
            if( l->matchDevice(*device) ) {
                if( EIRDataType::NONE != updateMask ) {
                    l->deviceUpdated(device, updateMask, ad_report.getTimestamp());
                }
                if( 0 < new_connect ) {
                    l->deviceConnected(device, event.getHCIHandle(), event.getTimestamp());
                }
            }
        } catch (std::exception &except) {
            ERR_PRINT("DBTAdapter::EventHCI:DeviceConnected-CBs %d/%zd: %s of %s: Caught exception %s",
                    i+1, statusListenerList.size(),
                    l->toString().c_str(), device->toString().c_str(), except.what());
        }
        i++;
    });
    return true;
}

bool DBTAdapter::mgmtEvConnectFailedHCI(std::shared_ptr<MgmtEvent> e) noexcept {
    COND_PRINT(debug_event, "DBTAdapter::EventHCI:ConnectFailed: %s", e->toString().c_str());
    const MgmtEvtDeviceConnectFailed &event = *static_cast<const MgmtEvtDeviceConnectFailed *>(e.get());

    std::shared_ptr<DBTDevice> device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr != device ) {
        const uint16_t handle = device->getConnectionHandle();
        COND_PRINT(debug_event, "DBTAdapter::EventHCI:ConnectFailed(dev_id %d): %s, handle %s -> zero,\n    -> %s",
            dev_id, event.toString().c_str(), jau::uint16HexString(handle).c_str(),
            device->toString().c_str());

        device->notifyDisconnected();
        removeConnectedDevice(*device);

        int i=0;
        jau::for_each_cow(statusListenerList, [&](std::shared_ptr<AdapterStatusListener> &l) {
            try {
                if( l->matchDevice(*device) ) {
                    l->deviceDisconnected(device, event.getHCIStatus(), handle, event.getTimestamp());
                }
            } catch (std::exception &except) {
                ERR_PRINT("DBTAdapter::EventHCI:DeviceDisconnected-CBs %d/%zd: %s of %s: Caught exception %s",
                        i+1, statusListenerList.size(),
                        l->toString().c_str(), device->toString().c_str(), except.what());
            }
            i++;
        });
        removeDiscoveredDevice(*device); // ensure device will cause a deviceFound event after disconnect
    } else {
        WORDY_PRINT("DBTAdapter::EventHCI:DeviceDisconnected(dev_id %d): Device not tracked: %s",
            dev_id, event.toString().c_str());
    }
    return true;
}

bool DBTAdapter::mgmtEvLERemoteUserFeaturesHCI(std::shared_ptr<MgmtEvent> e) noexcept {
    const MgmtEvtLERemoteUserFeatures &event = *static_cast<const MgmtEvtLERemoteUserFeatures *>(e.get());

    std::shared_ptr<DBTDevice> device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr != device ) {
        COND_PRINT(debug_event, "DBTAdapter::EventHCI:LERemoteUserFeatures(dev_id %d): %s, %s",
            dev_id, event.toString().c_str(), device->toString().c_str());

        device->notifyLEFeatures(event.getFeatures());

    } else {
        WORDY_PRINT("DBTAdapter::EventHCI:LERemoteUserFeatures(dev_id %d): Device not tracked: %s",
            dev_id, event.toString().c_str());
    }
    return true;
}

bool DBTAdapter::mgmtEvDeviceDisconnectedHCI(std::shared_ptr<MgmtEvent> e) noexcept {
    const MgmtEvtDeviceDisconnected &event = *static_cast<const MgmtEvtDeviceDisconnected *>(e.get());

    std::shared_ptr<DBTDevice> device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr != device ) {
        if( device->getConnectionHandle() != event.getHCIHandle() ) {
            WORDY_PRINT("DBTAdapter::EventHCI:DeviceDisconnected(dev_id %d): ConnHandle mismatch %s\n    -> %s",
                dev_id, event.toString().c_str(), device->toString().c_str());
            return true;
        }
        COND_PRINT(debug_event, "DBTAdapter::EventHCI:DeviceDisconnected(dev_id %d): %s, handle %s -> zero,\n    -> %s",
            dev_id, event.toString().c_str(), jau::uint16HexString(event.getHCIHandle()).c_str(),
            device->toString().c_str());

        device->notifyDisconnected();
        removeConnectedDevice(*device);

        int i=0;
        jau::for_each_cow(statusListenerList, [&](std::shared_ptr<AdapterStatusListener> &l) {
            try {
                if( l->matchDevice(*device) ) {
                    l->deviceDisconnected(device, event.getHCIReason(), event.getHCIHandle(), event.getTimestamp());
                }
            } catch (std::exception &except) {
                ERR_PRINT("DBTAdapter::EventHCI:DeviceDisconnected-CBs %d/%zd: %s of %s: Caught exception %s",
                        i+1, statusListenerList.size(),
                        l->toString().c_str(), device->toString().c_str(), except.what());
            }
            i++;
        });
        removeDiscoveredDevice(*device); // ensure device will cause a deviceFound event after disconnect
    } else {
        WORDY_PRINT("DBTAdapter::EventHCI:DeviceDisconnected(dev_id %d): Device not tracked: %s",
            dev_id, event.toString().c_str());
    }
    return true;
}

bool DBTAdapter::mgmtEvDeviceDisconnectedMgmt(std::shared_ptr<MgmtEvent> e) noexcept {
    COND_PRINT(debug_event, "DBTAdapter:mgmt:DeviceDisconnected: %s", e->toString().c_str());
    const MgmtEvtDeviceDisconnected &event = *static_cast<const MgmtEvtDeviceDisconnected *>(e.get());
    (void)event;
    return true;
}

bool DBTAdapter::mgmtEvDeviceFoundHCI(std::shared_ptr<MgmtEvent> e) noexcept {
    COND_PRINT(debug_event, "DBTAdapter:hci:DeviceFound(dev_id %d): %s", dev_id, e->toString().c_str());
    const MgmtEvtDeviceFound &deviceFoundEvent = *static_cast<const MgmtEvtDeviceFound *>(e.get());

    std::shared_ptr<EInfoReport> eir = deviceFoundEvent.getEIR();
    if( nullptr == eir ) {
        // Sourced from Linux Mgmt or otherwise ...
        eir = std::shared_ptr<EInfoReport>(new EInfoReport());
        eir->setSource(EInfoReport::Source::EIR_MGMT);
        eir->setTimestamp(deviceFoundEvent.getTimestamp());
        eir->setEvtType(AD_PDU_Type::ADV_IND);
        eir->setAddressType(deviceFoundEvent.getAddressType());
        eir->setAddress( deviceFoundEvent.getAddress() );
        eir->setRSSI( deviceFoundEvent.getRSSI() );
        eir->read_data(deviceFoundEvent.getData(), deviceFoundEvent.getDataSize());
    } // else: Sourced from HCIHandler via LE_ADVERTISING_REPORT (default!)

    std::shared_ptr<DBTDevice> dev = findDiscoveredDevice(eir->getAddress(), eir->getAddressType());
    if( nullptr != dev ) {
        //
        // drop existing device
        //
        EIRDataType updateMask = dev->update(*eir);
        COND_PRINT(debug_event, "DBTAdapter:hci:DeviceFound: Drop already discovered %s, %s",
                dev->getAddressString().c_str(), eir->toString().c_str());
        if( EIRDataType::NONE != updateMask ) {
            sendDeviceUpdated("DiscoveredDeviceFound", dev, eir->getTimestamp(), updateMask);
        }
        return true;
    }

    dev = findSharedDevice(eir->getAddress(), eir->getAddressType());
    if( nullptr != dev ) {
        //
        // active shared device, but flushed from discovered devices
        // - update device
        // - issue deviceFound, allowing receivers to recognize the re-discovered device
        // - issue deviceUpdate if data has changed, allowing receivers to act upon
        //
        EIRDataType updateMask = dev->update(*eir);
        addDiscoveredDevice(dev); // re-add to discovered devices!
        dev->ts_last_discovery = eir->getTimestamp();
        COND_PRINT(debug_event, "DBTAdapter:hci:DeviceFound: Use already shared %s, %s",
                dev->getAddressString().c_str(), eir->toString().c_str());

        int i=0;
        jau::for_each_cow(statusListenerList, [&](std::shared_ptr<AdapterStatusListener> &l) {
            try {
                if( l->matchDevice(*dev) ) {
                    l->deviceFound(dev, eir->getTimestamp());
                }
            } catch (std::exception &except) {
                ERR_PRINT("DBTAdapter:hci:DeviceFound: %d/%zd: %s of %s: Caught exception %s",
                        i+1, statusListenerList.size(),
                        l->toString().c_str(), dev->toString().c_str(), except.what());
            }
            i++;
        });
        if( EIRDataType::NONE != updateMask ) {
            sendDeviceUpdated("SharedDeviceFound", dev, eir->getTimestamp(), updateMask);
        }
        return true;
    }

    //
    // new device
    //
    dev = std::shared_ptr<DBTDevice>(new DBTDevice(*this, *eir));
    addDiscoveredDevice(dev);
    addSharedDevice(dev);
    COND_PRINT(debug_event, "DBTAdapter:hci:DeviceFound: Use new %s, %s",
            dev->getAddressString().c_str(), eir->toString().c_str());

    int i=0;
    jau::for_each_cow(statusListenerList, [&](std::shared_ptr<AdapterStatusListener> &l) {
        try {
            if( l->matchDevice(*dev) ) {
                l->deviceFound(dev, eir->getTimestamp());
            }
        } catch (std::exception &except) {
            ERR_PRINT("DBTAdapter:hci:DeviceFound-CBs %d/%zd: %s of %s: Caught exception %s",
                    i+1, statusListenerList.size(),
                    l->toString().c_str(), dev->toString().c_str(), except.what());
        }
        i++;
    });

    return true;
}

bool DBTAdapter::mgmtEvDeviceUnpairedMgmt(std::shared_ptr<MgmtEvent> e) noexcept {
    const MgmtEvtDeviceUnpaired &event = *static_cast<const MgmtEvtDeviceUnpaired *>(e.get());
    DBG_PRINT("DBTAdapter:mgmt:DeviceUnpaired: %s", event.toString().c_str());
    return true;
}
bool DBTAdapter::mgmtEvPinCodeRequestMgmt(std::shared_ptr<MgmtEvent> e) noexcept {
    const MgmtEvtPinCodeRequest &event = *static_cast<const MgmtEvtPinCodeRequest *>(e.get());
    DBG_PRINT("DBTAdapter:mgmt:PinCodeRequest: %s", event.toString().c_str());
    return true;
}
bool DBTAdapter::mgmtEvAuthFailedMgmt(std::shared_ptr<MgmtEvent> e) noexcept {
    const MgmtEvtAuthFailed &event = *static_cast<const MgmtEvtAuthFailed *>(e.get());

    std::shared_ptr<DBTDevice> device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr == device ) {
        WORDY_PRINT("DBTAdapter:hci:SMP: dev_id %d: Device not tracked: address[%s, %s], %s",
                dev_id, event.getAddress().toString().c_str(), getBDAddressTypeString(event.getAddressType()).c_str(),
                event.toString().c_str());
        return true;
    }
    DBG_PRINT("DBTAdapter:mgmt:SMP: dev_id %d: address[%s, %s]: %s",
        dev_id, event.getAddress().toString().c_str(), getBDAddressTypeString(event.getAddressType()).c_str(),
        event.toString().c_str());
    return true;
}
bool DBTAdapter::mgmtEvUserConfirmRequestMgmt(std::shared_ptr<MgmtEvent> e) noexcept {
    const MgmtEvtUserConfirmRequest &event = *static_cast<const MgmtEvtUserConfirmRequest *>(e.get());

    std::shared_ptr<DBTDevice> device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr == device ) {
        WORDY_PRINT("DBTAdapter:hci:SMP: dev_id %d: Device not tracked: address[%s, %s], %s",
                dev_id, event.getAddress().toString().c_str(), getBDAddressTypeString(event.getAddressType()).c_str(),
                event.toString().c_str());
        return true;
    }
    // FIXME: Pass confirm_hint and value
    const SMPPairingState pstate  = SMPPairingState::NUMERIC_COMPARISON_EXPECTED;

    DBG_PRINT("DBTAdapter:mgmt:SMP: dev_id %d: address[%s, %s]: state %s, %s",
        dev_id, event.getAddress().toString().c_str(), getBDAddressTypeString(event.getAddressType()).c_str(),
        getSMPPairingStateString(pstate).c_str(),
        event.toString().c_str());

    updatePairingState(device, pstate, e->getTimestamp());
    return true;
}
bool DBTAdapter::mgmtEvUserPasskeyRequestMgmt(std::shared_ptr<MgmtEvent> e) noexcept {
    const MgmtEvtUserPasskeyRequest &event = *static_cast<const MgmtEvtUserPasskeyRequest *>(e.get());

    std::shared_ptr<DBTDevice> device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr == device ) {
        WORDY_PRINT("DBTAdapter:hci:SMP: dev_id %d: Device not tracked: address[%s, %s], %s",
                dev_id, event.getAddress().toString().c_str(), getBDAddressTypeString(event.getAddressType()).c_str(),
                event.toString().c_str());
        return true;
    }
    const SMPPairingState pstate  = SMPPairingState::PASSKEY_EXPECTED;

    DBG_PRINT("DBTAdapter:mgmt:SMP: dev_id %d: address[%s, %s]: state %s, %s",
        dev_id, event.getAddress().toString().c_str(), getBDAddressTypeString(event.getAddressType()).c_str(),
        getSMPPairingStateString(pstate).c_str(),
        event.toString().c_str());

    updatePairingState(device, pstate, e->getTimestamp());
    return true;
}

void DBTAdapter::updatePairingState(std::shared_ptr<DBTDevice> device, const SMPPairingState pstate, uint64_t timestamp) noexcept
{
    PairingMode pmode;
    if( device->updatePairingState_locked(pstate, pmode) ) {
        sendDevicePairingState(device, pstate, pmode, timestamp);
    } else {
        WORDY_PRINT("DBTAdapter::updatePairingState: dev_id %d: Unchanged: state %s, %s", dev_id,
                getSMPPairingStateString(pstate).c_str(),
                device->toString().c_str());
    }
}

bool DBTAdapter::hciSMPMsgCallback(const EUI48& address, BDAddressType addressType,
                                   std::shared_ptr<const SMPPDUMsg> msg, const HCIACLData::l2cap_frame& source) noexcept {
    std::shared_ptr<DBTDevice> device = findConnectedDevice(address, addressType);
    if( nullptr == device ) {
        WORDY_PRINT("DBTAdapter:hci:SMP: dev_id %d: Device not tracked: address[%s, %s]: %s, %s",
                dev_id, address.toString().c_str(), getBDAddressTypeString(addressType).c_str(),
                msg->toString().c_str(), source.toString().c_str());
        return true;
    }
    if( device->getConnectionHandle() != source.handle ) {
        WORDY_PRINT("DBTAdapter:hci:SMP: dev_id %d: ConnHandle mismatch address[%s, %s]: %s, %s\n    -> %s",
                dev_id, address.toString().c_str(), getBDAddressTypeString(addressType).c_str(),
                msg->toString().c_str(), source.toString().c_str(), device->toString().c_str());
        return true;
    }

    device->hciSMPMsgCallback(device, msg, source);

    return true;
}

void DBTAdapter::sendDevicePairingState(std::shared_ptr<DBTDevice> device, const SMPPairingState state, const PairingMode mode, uint64_t timestamp) noexcept
{
    int i=0;
    jau::for_each_cow(statusListenerList, [&](std::shared_ptr<AdapterStatusListener> &l) {
        try {
            if( l->matchDevice(*device) ) {
                l->devicePairingState(device, state, mode, timestamp);
            }
        } catch (std::exception &except) {
            ERR_PRINT("DBTAdapter::sendDevicePairingState: %d/%zd: %s of %s: Caught exception %s",
                    i+1, statusListenerList.size(),
                    l->toString().c_str(), device->toString().c_str(), except.what());
        }
        i++;
    });
}

void DBTAdapter::sendDeviceReady(std::shared_ptr<DBTDevice> device, uint64_t timestamp) noexcept {
    int i=0;
    jau::for_each_cow(statusListenerList, [&](std::shared_ptr<AdapterStatusListener> &l) {
        try {
            if( l->matchDevice(*device) ) {
                l->deviceReady(device, timestamp);
            }
        } catch (std::exception &except) {
            ERR_PRINT("DBTAdapter::sendDeviceReady: %d/%zd: %s of %s: Caught exception %s",
                    i+1, statusListenerList.size(),
                    l->toString().c_str(), device->toString().c_str(), except.what());
        }
        i++;
    });
}
