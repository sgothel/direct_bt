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

std::shared_ptr<DBTDevice> DBTAdapter::findDevice(device_list_t & devices, const EUI48 & address, const BDAddressType addressType) noexcept {
    const jau::nsize_t size = devices.size();
    for (jau::nsize_t i = 0; i < size; ++i) {
        std::shared_ptr<DBTDevice> & e = devices[i];
        if ( nullptr != e && address == e->getAddressAndType().address && addressType == e->getAddressAndType().type) {
            return e;
        }
    }
    return nullptr;
}

std::shared_ptr<DBTDevice> DBTAdapter::findDevice(device_list_t & devices, DBTDevice const & device) noexcept {
    const jau::nsize_t size = devices.size();
    for (jau::nsize_t i = 0; i < size; ++i) {
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
    auto end = connectedDevices.end();
    for (auto it = connectedDevices.begin(); it != end; ++it) {
        if ( nullptr != *it && device == **it ) {
            connectedDevices.erase(it);
            return true;
        }
    }
    return false;
}

int DBTAdapter::disconnectAllDevices(const HCIStatusCode reason) noexcept {
    device_list_t devices;
    {
        const std::lock_guard<std::mutex> lock(mtx_connectedDevices); // RAII-style acquire and relinquish via destructor
        devices = connectedDevices; // copy!
    }
    const int count = devices.size();
    auto end = devices.end();
    for (auto it = devices.begin(); it != end; ++it) {
        if( nullptr != *it ) {
            (*it)->disconnect(reason); // will erase device from list via removeConnectedDevice(..) above
        }
    }
    return count;
}

std::shared_ptr<DBTDevice> DBTAdapter::findConnectedDevice (const EUI48 & address, const BDAddressType & addressType) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_connectedDevices); // RAII-style acquire and relinquish via destructor
    return findDevice(connectedDevices, address, addressType);
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
    ok = mgmt.addMgmtEventCallback(dev_id, MgmtEvent::Opcode::PAIR_DEVICE_COMPLETE, jau::bindMemberFunc(this, &DBTAdapter::mgmtEvPairDeviceCompleteMgmt));
    ok = mgmt.addMgmtEventCallback(dev_id, MgmtEvent::Opcode::NEW_LONG_TERM_KEY, jau::bindMemberFunc(this, &DBTAdapter::mgmtEvNewLongTermKeyMgmt));

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
    ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::HCI_LE_REMOTE_USR_FEATURES, jau::bindMemberFunc(this, &DBTAdapter::mgmtEvHCILERemoteUserFeaturesHCI)) && ok;
    ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::HCI_ENC_CHANGED, jau::bindMemberFunc(this, &DBTAdapter::mgmtEvHCIEncryptionChangedHCI)) && ok;
    ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::HCI_ENC_KEY_REFRESH_COMPLETE, jau::bindMemberFunc(this, &DBTAdapter::mgmtEvHCIEncryptionKeyRefreshCompleteHCI)) && ok;

    if( !ok ) {
        ERR_PRINT("Could not add all required MgmtEventCallbacks to HCIHandler: %s of %s", hci.toString().c_str(), toString().c_str());
        return false; // dtor local HCIHandler w/ closing
    }
    hci.addSMPMsgCallback(jau::bindMemberFunc(this, &DBTAdapter::hciSMPMsgCallback));

    return true;

errout0:
    adapterInfo = std::make_shared<AdapterInfo>(dev_id, EUI48::ANY_DEVICE, 0, 0,
                            AdapterSetting::NONE, AdapterSetting::NONE, 0, "invalid", "invalid");
    return false;
}

DBTAdapter::DBTAdapter() noexcept
: debug_event(jau::environment::getBooleanProperty("direct_bt.debug.adapter.event", false)),
  debug_lock(jau::environment::getBooleanProperty("direct_bt.debug.adapter.lock", false)),
  mgmt( DBTManager::get(BTMode::NONE /* use env default */) ),
  dev_id( mgmt.getDefaultAdapterDevID() ),
  hci( dev_id )
{
    valid = validateDevInfo();
}

DBTAdapter::DBTAdapter(EUI48 &mac) noexcept
: debug_event(jau::environment::getBooleanProperty("direct_bt.debug.adapter.event", false)),
  debug_lock(jau::environment::getBooleanProperty("direct_bt.debug.adapter.lock", false)),
  mgmt( DBTManager::get(BTMode::NONE /* use env default */) ),
  dev_id( mgmt.findAdapterInfoDevId(mac) ),
  hci( dev_id )
{
    valid = validateDevInfo();
}

DBTAdapter::DBTAdapter(const int _dev_id) noexcept
: debug_event(jau::environment::getBooleanProperty("direct_bt.debug.adapter.event", false)),
  debug_lock(jau::environment::getBooleanProperty("direct_bt.debug.adapter.lock", false)),
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

    // ensure all hci states are reset.
    hci.clearAllStates();

    unlockConnectAny();

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
    AdapterSetting current_settings { AdapterSetting::NONE } ;
    return MgmtStatus::SUCCESS == mgmt.setDiscoverable(dev_id, value ? 0x01 : 0x00, 10 /* timeout seconds */, current_settings);
}

bool DBTAdapter::setBondable(bool value) noexcept {
    AdapterSetting current_settings { AdapterSetting::NONE } ;
    return mgmt.setMode(dev_id, MgmtCommand::Opcode::SET_BONDABLE, value ? 1 : 0, current_settings);
}

bool DBTAdapter::setPowered(bool value) noexcept {
    AdapterSetting current_settings { AdapterSetting::NONE } ;
    return mgmt.setMode(dev_id, MgmtCommand::Opcode::SET_POWERED, value ? 1 : 0, current_settings);
}

bool DBTAdapter::lockConnect(const DBTDevice & device, const bool wait, const SMPIOCapability io_cap) noexcept {
    std::unique_lock<std::mutex> lock(mtx_single_conn_device); // RAII-style acquire and relinquish via destructor
    const uint32_t timeout_ms = 10000; // FIXME: Configurable?

    if( nullptr != single_conn_device_ptr ) {
        if( device == *single_conn_device_ptr ) {
            COND_PRINT(debug_lock, "DBTAdapter::lockConnect: Success: Already locked, same device: %s", device.toString(false).c_str());
            return true; // already set, same device: OK, locked
        }
        if( wait ) {
            while( nullptr != single_conn_device_ptr ) {
                std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
                std::cv_status s = cv_single_conn_device.wait_until(lock, t0 + std::chrono::milliseconds(timeout_ms));
                if( std::cv_status::timeout == s && nullptr != single_conn_device_ptr ) {
                    if( debug_lock ) {
                        jau::PLAIN_PRINT(true, "DBTAdapter::lockConnect: Failed: Locked (waited)");
                        jau::PLAIN_PRINT(true, " - locked-by-other-device %s", single_conn_device_ptr->toString(false).c_str());
                        jau::PLAIN_PRINT(true, " - lock-failed-for %s", device.toString(false).c_str());
                    }
                    return false;
                }
            }
            // lock was released
        } else {
            if( debug_lock ) {
                jau::PLAIN_PRINT(true, "DBTAdapter::lockConnect: Failed: Locked (no-wait)");
                jau::PLAIN_PRINT(true, " - locked-by-other-device %s", single_conn_device_ptr->toString(false).c_str());
                jau::PLAIN_PRINT(true, " - lock-failed-for %s", device.toString(false).c_str());
            }
            return false; // already set, not waiting, blocked
        }
    }
    single_conn_device_ptr = &device;

    if( SMPIOCapability::UNSET != io_cap ) {
#if USE_LINUX_BT_SECURITY
        SMPIOCapability pre_io_cap { SMPIOCapability::UNSET };
        const bool res_iocap = mgmt.setIOCapability(dev_id, io_cap, pre_io_cap);
        if( res_iocap ) {
            iocap_defaultval  = pre_io_cap;
            COND_PRINT(debug_lock, "DBTAdapter::lockConnect: Success: New lock, setIOCapability[%s -> %s], %s",
                getSMPIOCapabilityString(pre_io_cap).c_str(), getSMPIOCapabilityString(io_cap).c_str(),
                device.toString(false).c_str());
            return true;
        } else {
            // failed, unlock and exit
            COND_PRINT(debug_lock, "DBTAdapter::lockConnect: Failed: setIOCapability[%s], %s",
                getSMPIOCapabilityString(io_cap).c_str(), device.toString(false).c_str());
            single_conn_device_ptr = nullptr;
            cv_single_conn_device.notify_all(); // notify waiting getter
            return false;
        }
#else
        COND_PRINT(debug_lock, "DBTAdapter::lockConnect: Success: New lock, ignored io-cap: %s, %s",
                getSMPIOCapabilityString(io_cap).c_str()
                device.toString(false).c_str());
        return true;
#endif
    } else {
        COND_PRINT(debug_lock, "DBTAdapter::lockConnect: Success: New lock, no io-cap: %s", device.toString(false).c_str());
        return true;
    }
}

bool DBTAdapter::unlockConnect(const DBTDevice & device) noexcept {
    std::unique_lock<std::mutex> lock(mtx_single_conn_device); // RAII-style acquire and relinquish via destructor

    if( nullptr != single_conn_device_ptr && device == *single_conn_device_ptr ) {
        const SMPIOCapability v = iocap_defaultval;
        iocap_defaultval  = SMPIOCapability::UNSET;
        if( SMPIOCapability::UNSET != v ) {
            // Unreachable: !USE_LINUX_BT_SECURITY
            SMPIOCapability o;
            const bool res = mgmt.setIOCapability(dev_id, v, o);
            COND_PRINT(debug_lock, "DBTAdapter::unlockConnect: Success: setIOCapability[res %d: %s -> %s], %s",
                res, getSMPIOCapabilityString(o).c_str(), getSMPIOCapabilityString(v).c_str(),
                single_conn_device_ptr->toString(false).c_str());
        } else {
            COND_PRINT(debug_lock, "DBTAdapter::unlockConnect: Success: %s",
                single_conn_device_ptr->toString(false).c_str());
        }
        single_conn_device_ptr = nullptr;
        cv_single_conn_device.notify_all(); // notify waiting getter
        return true;
    } else {
        if( debug_lock ) {
            const std::string other_device_str = nullptr != single_conn_device_ptr ? single_conn_device_ptr->toString(false) : "null";
            jau::PLAIN_PRINT(true, "DBTAdapter::unlockConnect: Not locked:");
            jau::PLAIN_PRINT(true, " - locked-by-other-device %s", other_device_str.c_str());
            jau::PLAIN_PRINT(true, " - unlock-failed-for %s", device.toString(false).c_str());
        }
        return false;
    }
}

bool DBTAdapter::unlockConnectAny() noexcept {
    std::unique_lock<std::mutex> lock(mtx_single_conn_device); // RAII-style acquire and relinquish via destructor

    if( nullptr != single_conn_device_ptr ) {
        const SMPIOCapability v = iocap_defaultval;
        iocap_defaultval  = SMPIOCapability::UNSET;
        if( SMPIOCapability::UNSET != v ) {
            // Unreachable: !USE_LINUX_BT_SECURITY
            SMPIOCapability o;
            const bool res = mgmt.setIOCapability(dev_id, v, o);
            COND_PRINT(debug_lock, "DBTAdapter::unlockConnectAny: Success: setIOCapability[res %d: %s -> %s]; %s",
                res, getSMPIOCapabilityString(o).c_str(), getSMPIOCapabilityString(v).c_str(),
                single_conn_device_ptr->toString(false).c_str());
        } else {
            COND_PRINT(debug_lock, "DBTAdapter::unlockConnectAny: Success: %s",
                single_conn_device_ptr->toString(false).c_str());
        }
        single_conn_device_ptr = nullptr;
        cv_single_conn_device.notify_all(); // notify waiting getter
        return true;
    } else {
        iocap_defaultval = SMPIOCapability::UNSET;
        COND_PRINT(debug_lock, "DBTAdapter::unlockConnectAny: Not locked");
        return false;
    }
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

bool DBTAdapter::isDeviceWhitelisted(const BDAddressAndType & addressAndType) noexcept {
    return mgmt.isDeviceWhitelisted(dev_id, addressAndType);
}

bool DBTAdapter::addDeviceToWhitelist(const BDAddressAndType & addressAndType, const HCIWhitelistConnectType ctype,
                                      const uint16_t conn_interval_min, const uint16_t conn_interval_max,
                                      const uint16_t conn_latency, const uint16_t timeout) {
    if( !isPowered() ) {
        ERR_PRINT("DBTAdapter::startDiscovery: Adapter not powered: %s", toString().c_str());
        return false;
    }
    if( mgmt.isDeviceWhitelisted(dev_id, addressAndType) ) {
        ERR_PRINT("DBTAdapter::addDeviceToWhitelist: device already listed: dev_id %d, address%s", dev_id, addressAndType.toString().c_str());
        return true;
    }

    if( !mgmt.uploadConnParam(dev_id, addressAndType, conn_interval_min, conn_interval_max, conn_latency, timeout) ) {
        ERR_PRINT("DBTAdapter::addDeviceToWhitelist: uploadConnParam(dev_id %d, address%s, interval[%u..%u], latency %u, timeout %u): Failed",
                dev_id, addressAndType.toString().c_str(), conn_interval_min, conn_interval_max, conn_latency, timeout);
    }
    return mgmt.addDeviceToWhitelist(dev_id, addressAndType, ctype);
}

bool DBTAdapter::removeDeviceFromWhitelist(const BDAddressAndType & addressAndType) {
    return mgmt.removeDeviceFromWhitelist(dev_id, addressAndType);
}

static jau::cow_darray<std::shared_ptr<AdapterStatusListener>>::equal_comparator _adapterStatusListenerRefEqComparator =
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
    {
        auto begin = statusListenerList.begin(); // lock mutex and copy_store
        while ( !begin.is_end() ) {
            if ( **begin == *l ) {
                begin.erase();
                begin.write_back();
                return true;
            } else {
                ++begin;
            }
        }
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
        const MgmtEvtDiscovering e(dev_id, ScanType::LE, false);
        mgmtEvDeviceDiscoveringHCI( e );
    }
    DBG_PRINT("DBTAdapter::stopDiscovery: End: Result %s, keepAlive %d, currentScanType[native %s, meta %s], le_scan_temp_disabled %d ...",
            getHCIStatusCodeString(status).c_str(), keep_le_scan_alive.load(),
            getScanTypeString(hci.getCurrentScanType()).c_str(), getScanTypeString(currentMetaScanType).c_str(), le_scan_temp_disabled);

    checkDiscoveryState();

    return status;
}

// *************************************************

std::shared_ptr<DBTDevice> DBTAdapter::findDiscoveredDevice (const EUI48 & address, const BDAddressType addressType) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_discoveredDevices); // RAII-style acquire and relinquish via destructor
    return findDevice(discoveredDevices, address, addressType);
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

bool DBTAdapter::removeDiscoveredDevice(const BDAddressAndType & addressAndType) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_discoveredDevices); // RAII-style acquire and relinquish via destructor
    for (auto it = discoveredDevices.begin(); it != discoveredDevices.end(); ) {
        if ( nullptr != *it && addressAndType == (*it)->addressAndType ) {
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

jau::darray<std::shared_ptr<DBTDevice>> DBTAdapter::getDiscoveredDevices() const noexcept {
    const std::lock_guard<std::mutex> lock(const_cast<DBTAdapter*>(this)->mtx_discoveredDevices); // RAII-style acquire and relinquish via destructor
    device_list_t res = discoveredDevices;
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

std::shared_ptr<DBTDevice> DBTAdapter::findSharedDevice (const EUI48 & address, const BDAddressType addressType) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_sharedDevices); // RAII-style acquire and relinquish via destructor
    return findDevice(sharedDevices, address, addressType);
}

void DBTAdapter::removeDevice(DBTDevice & device) noexcept {
    WORDY_PRINT("DBTAdapter::removeDevice: Start %s", toString(false).c_str());
    const HCIStatusCode status = device.disconnect(HCIStatusCode::REMOTE_USER_TERMINATED_CONNECTION);
    WORDY_PRINT("DBTAdapter::removeDevice: disconnect %s, %s", getHCIStatusCodeString(status).c_str(), toString(false).c_str());
    unlockConnect(device);
    removeConnectedDevice(device); // usually done in DBTAdapter::mgmtEvDeviceDisconnectedHCI
    removeDiscoveredDevice(device.addressAndType); // usually done in DBTAdapter::mgmtEvDeviceDisconnectedHCI
    WORDY_PRINT("DBTAdapter::removeDevice: End %s", toString(false).c_str());
    removeSharedDevice(device);
}

std::string DBTAdapter::toString(bool includeDiscoveredDevices) const noexcept {
    std::string out("Adapter[BTMode "+getBTModeString(btMode)+", "+getAddressString()+", '"+getName()+"', id "+std::to_string(dev_id)+
                    ", curSettings"+getAdapterSettingMaskString(adapterInfo->getCurrentSettingMask())+
                    ", scanType[native "+getScanTypeString(hci.getCurrentScanType())+", meta "+getScanTypeString(currentMetaScanType)+"]"
                    ", valid "+std::to_string(isValid())+", open[mgmt, "+std::to_string(mgmt.isOpen())+", hci "+std::to_string(hci.isOpen())+
                    "], "+javaObjectToString()+"]");
    device_list_t devices = getDiscoveredDevices();
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

void DBTAdapter::sendAdapterSettingsChanged(const AdapterSetting old_settings_, const AdapterSetting current_settings, AdapterSetting changes,
                                            const uint64_t timestampMS) noexcept
{
    int i=0;
    jau::for_each_fidelity(statusListenerList, [&](std::shared_ptr<AdapterStatusListener> &l) {
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
    jau::for_each_fidelity(statusListenerList, [&](std::shared_ptr<AdapterStatusListener> &l) {
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

bool DBTAdapter::mgmtEvDeviceDiscoveringHCI(const MgmtEvent& e) noexcept {
    return mgmtEvDeviceDiscoveringAny(e, true /* hciSourced */ );
}

bool DBTAdapter::mgmtEvDeviceDiscoveringMgmt(const MgmtEvent& e) noexcept {
    return mgmtEvDeviceDiscoveringAny(e, false /* hciSourced */ );
}

bool DBTAdapter::mgmtEvDeviceDiscoveringAny(const MgmtEvent& e, const bool hciSourced) noexcept {
    const std::string srctkn = hciSourced ? "hci" : "mgmt";
    const MgmtEvtDiscovering &event = *static_cast<const MgmtEvtDiscovering *>(&e);
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
    jau::for_each_fidelity(statusListenerList, [&](std::shared_ptr<AdapterStatusListener> &l) {
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

bool DBTAdapter::mgmtEvNewSettingsMgmt(const MgmtEvent& e) noexcept {
    COND_PRINT(debug_event, "DBTAdapter:mgmt:NewSettings: %s", e.toString().c_str());
    const MgmtEvtNewSettings &event = *static_cast<const MgmtEvtNewSettings *>(&e);
    const AdapterSetting new_settings = adapterInfo->setCurrentSettingMask(event.getSettings()); // probably done by mgmt callback already
    {
        const BTMode _btMode = getAdapterSettingsBTMode(new_settings);
        if( BTMode::NONE != _btMode ) {
            btMode = _btMode;
        }
    }
    const AdapterSetting old_settings_ = old_settings;

    const AdapterSetting changes = getAdapterSettingMaskDiff(new_settings, old_settings_);

    const bool justPoweredOn = isAdapterSettingBitSet(changes, AdapterSetting::POWERED) &&
                               isAdapterSettingBitSet(new_settings, AdapterSetting::POWERED);

    const bool justPoweredOff = isAdapterSettingBitSet(changes, AdapterSetting::POWERED) &&
                                !isAdapterSettingBitSet(new_settings, AdapterSetting::POWERED);

    old_settings = new_settings;

    COND_PRINT(debug_event, "DBTAdapter::mgmt:NewSettings: %s -> %s, changes %s: %s",
            getAdapterSettingMaskString(old_settings_).c_str(),
            getAdapterSettingMaskString(new_settings).c_str(),
            getAdapterSettingMaskString(changes).c_str(), toString(false).c_str() );

    if( justPoweredOn ) {
        // Adapter has been powered on, ensure all hci states are reset.
        hci.clearAllStates();
    }
    sendAdapterSettingsChanged(old_settings_, new_settings, changes, event.getTimestamp());

    if( justPoweredOff ) {
        // Adapter has been powered off, close connections and cleanup off-thread.
        std::thread bg(&DBTAdapter::poweredOff, this); // @suppress("Invalid arguments")
        bg.detach();
    }

    return true;
}

bool DBTAdapter::mgmtEvLocalNameChangedMgmt(const MgmtEvent& e) noexcept {
    COND_PRINT(debug_event, "DBTAdapter:mgmt:LocalNameChanged: %s", e.toString().c_str());
    const MgmtEvtLocalNameChanged &event = *static_cast<const MgmtEvtLocalNameChanged *>(&e);
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

bool DBTAdapter::mgmtEvDeviceConnectedHCI(const MgmtEvent& e) noexcept {
    COND_PRINT(debug_event, "DBTAdapter:hci:DeviceConnected(dev_id %d): %s", dev_id, e.toString().c_str());
    const MgmtEvtDeviceConnected &event = *static_cast<const MgmtEvtDeviceConnected *>(&e);
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
        if( nullptr != device ) {
            addSharedDevice(device); // connected devices must be in shared + discovered list
            new_connect = 1;
        }
    }
    if( nullptr == device ) {
        device = findSharedDevice(event.getAddress(), event.getAddressType());
        if( nullptr != device ) {
            addDiscoveredDevice(device); // connected devices must be in shared + discovered list
            new_connect = 2;
        }
    }
    if( nullptr == device ) {
        // a whitelist auto-connect w/o previous discovery
        // private ctor: device = std::make_shared<DBTDevice>(*this, ad_report);
        device = std::shared_ptr<DBTDevice>(new DBTDevice(*this, ad_report));
        addDiscoveredDevice(device);
        addSharedDevice(device);
        new_connect = 3;
    }

    const SMPIOCapability io_cap_conn = mgmt.getIOCapability(dev_id);

    EIRDataType updateMask = device->update(ad_report);
    if( 0 == new_connect ) {
        WARN_PRINT("DBTAdapter::EventHCI:DeviceConnected(dev_id %d, already connected, updated %s): %s, handle %s -> %s,\n    %s,\n    -> %s",
            dev_id, getEIRDataMaskString(updateMask).c_str(), event.toString().c_str(),
            jau::uint16HexString(device->getConnectionHandle()).c_str(), jau::uint16HexString(event.getHCIHandle()).c_str(),
            ad_report.toString().c_str(),
            device->toString().c_str());
    } else {
        addConnectedDevice(device); // track device, if not done yet
        if( 2 <= new_connect ) {
            device->ts_last_discovery = ad_report.getTimestamp();
        }
        COND_PRINT(debug_event, "DBTAdapter::EventHCI:DeviceConnected(dev_id %d, new_connect %d, updated %s): %s, handle %s -> %s,\n    %s,\n    -> %s",
            dev_id, new_connect, getEIRDataMaskString(updateMask).c_str(), event.toString().c_str(),
            jau::uint16HexString(device->getConnectionHandle()).c_str(), jau::uint16HexString(event.getHCIHandle()).c_str(),
            ad_report.toString().c_str(),
            device->toString().c_str());
    }
    device->notifyConnected(device, event.getHCIHandle(), io_cap_conn);

    int i=0;
    jau::for_each_fidelity(statusListenerList, [&](std::shared_ptr<AdapterStatusListener> &l) {
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

bool DBTAdapter::mgmtEvConnectFailedHCI(const MgmtEvent& e) noexcept {
    COND_PRINT(debug_event, "DBTAdapter::EventHCI:ConnectFailed: %s", e.toString().c_str());
    const MgmtEvtDeviceConnectFailed &event = *static_cast<const MgmtEvtDeviceConnectFailed *>(&e);

    std::shared_ptr<DBTDevice> device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr != device ) {
        const uint16_t handle = device->getConnectionHandle();
        COND_PRINT(debug_event, "DBTAdapter::EventHCI:ConnectFailed(dev_id %d): %s, handle %s -> zero,\n    -> %s",
            dev_id, event.toString().c_str(), jau::uint16HexString(handle).c_str(),
            device->toString().c_str());

        unlockConnect(*device);
        device->notifyDisconnected();
        removeConnectedDevice(*device);

        int i=0;
        jau::for_each_fidelity(statusListenerList, [&](std::shared_ptr<AdapterStatusListener> &l) {
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
        removeDiscoveredDevice(device->addressAndType); // ensure device will cause a deviceFound event after disconnect
    } else {
        WORDY_PRINT("DBTAdapter::EventHCI:DeviceDisconnected(dev_id %d): Device not tracked: %s",
            dev_id, event.toString().c_str());
    }
    return true;
}

bool DBTAdapter::mgmtEvHCIEncryptionChangedHCI(const MgmtEvent& e) noexcept {
    const MgmtEvtHCIEncryptionChanged &event = *static_cast<const MgmtEvtHCIEncryptionChanged *>(&e);

    std::shared_ptr<DBTDevice> device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr != device ) {
        // BT Core Spec v5.2: Vol 4, Part E HCI: 7.7.8 HCIEventType::ENCRYPT_CHANGE
        const HCIStatusCode evtStatus = event.getHCIStatus();
        const bool ok = HCIStatusCode::SUCCESS == evtStatus && 0 != event.getEncEnabled();
        const SMPPairingState pstate = ok ? SMPPairingState::COMPLETED : SMPPairingState::FAILED;
        device->updatePairingState(device, e, evtStatus, pstate);
    } else {
        WORDY_PRINT("DBTAdapter::EventHCI:EncryptionChanged(dev_id %d): Device not tracked: %s",
            dev_id, event.toString().c_str());
    }
    return true;
}
bool DBTAdapter::mgmtEvHCIEncryptionKeyRefreshCompleteHCI(const MgmtEvent& e) noexcept {
    const MgmtEvtHCIEncryptionKeyRefreshComplete &event = *static_cast<const MgmtEvtHCIEncryptionKeyRefreshComplete *>(&e);

    std::shared_ptr<DBTDevice> device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr != device ) {
        // BT Core Spec v5.2: Vol 4, Part E HCI: 7.7.39 HCIEventType::ENCRYPT_KEY_REFRESH_COMPLETE
        const HCIStatusCode evtStatus = event.getHCIStatus();
        // const bool ok = HCIStatusCode::SUCCESS == evtStatus;
        // const SMPPairingState pstate = ok ? SMPPairingState::COMPLETED : SMPPairingState::FAILED;
        const SMPPairingState pstate = SMPPairingState::NONE;
        device->updatePairingState(device, e, evtStatus, pstate);
    } else {
        WORDY_PRINT("DBTAdapter::EventHCI:EncryptionKeyRefreshComplete(dev_id %d): Device not tracked: %s",
            dev_id, event.toString().c_str());
    }
    return true;
}

bool DBTAdapter::mgmtEvHCILERemoteUserFeaturesHCI(const MgmtEvent& e) noexcept {
    const MgmtEvtHCILERemoteUserFeatures &event = *static_cast<const MgmtEvtHCILERemoteUserFeatures *>(&e);

    std::shared_ptr<DBTDevice> device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr != device ) {
        COND_PRINT(debug_event, "DBTAdapter::EventHCI:LERemoteUserFeatures(dev_id %d): %s, %s",
            dev_id, event.toString().c_str(), device->toString().c_str());

        device->notifyLEFeatures(device, event.getFeatures());

    } else {
        WORDY_PRINT("DBTAdapter::EventHCI:LERemoteUserFeatures(dev_id %d): Device not tracked: %s",
            dev_id, event.toString().c_str());
    }
    return true;
}

bool DBTAdapter::mgmtEvDeviceDisconnectedHCI(const MgmtEvent& e) noexcept {
    const MgmtEvtDeviceDisconnected &event = *static_cast<const MgmtEvtDeviceDisconnected *>(&e);

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

        unlockConnect(*device);
        device->notifyDisconnected();
        removeConnectedDevice(*device);

        int i=0;
        jau::for_each_fidelity(statusListenerList, [&](std::shared_ptr<AdapterStatusListener> &l) {
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
        removeDiscoveredDevice(device->addressAndType); // ensure device will cause a deviceFound event after disconnect
    } else {
        WORDY_PRINT("DBTAdapter::EventHCI:DeviceDisconnected(dev_id %d): Device not tracked: %s",
            dev_id, event.toString().c_str());
    }
    return true;
}

bool DBTAdapter::mgmtEvDeviceDisconnectedMgmt(const MgmtEvent& e) noexcept {
    COND_PRINT(debug_event, "DBTAdapter:mgmt:DeviceDisconnected: %s", e.toString().c_str());
    const MgmtEvtDeviceDisconnected &event = *static_cast<const MgmtEvtDeviceDisconnected *>(&e);
    (void)event;
    return true;
}

bool DBTAdapter::mgmtEvPairDeviceCompleteMgmt(const MgmtEvent& e) noexcept {
    const MgmtEvtPairDeviceComplete &event = *static_cast<const MgmtEvtPairDeviceComplete *>(&e);

    std::shared_ptr<DBTDevice> device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr != device ) {
        const HCIStatusCode evtStatus = getHCIStatusCode( event.getStatus() );
        const bool ok = HCIStatusCode::ALREADY_PAIRED == evtStatus;
        const SMPPairingState pstate = ok ? SMPPairingState::COMPLETED : SMPPairingState::NONE;
        device->updatePairingState(device, e, evtStatus, pstate);
    } else {
        WORDY_PRINT("DBTAdapter::mgmt:PairDeviceComplete(dev_id %d): Device not tracked: %s",
            dev_id, event.toString().c_str());
    }
    return true;
}

bool DBTAdapter::mgmtEvNewLongTermKeyMgmt(const MgmtEvent& e) noexcept {
    const MgmtEvtNewLongTermKey& event = *static_cast<const MgmtEvtNewLongTermKey *>(&e);
    const MgmtLongTermKeyInfo& ltk_info = event.getLongTermKey();
    std::shared_ptr<DBTDevice> device = findConnectedDevice(ltk_info.address, ltk_info.address_type);
    if( nullptr != device ) {
        const bool ok = ltk_info.enc_size > 0 && ltk_info.key_type != MgmtLTKType::NONE;
        if( ok ) {
            device->updatePairingState(device, e, HCIStatusCode::SUCCESS, SMPPairingState::COMPLETED);
        } else {
            WORDY_PRINT("DBTAdapter::mgmt:NewLongTermKey(dev_id %d): Invalid LTK: %s",
                dev_id, event.toString().c_str());
        }
    } else {
        WORDY_PRINT("DBTAdapter::mgmt:NewLongTermKey(dev_id %d): Device not tracked: %s",
            dev_id, event.toString().c_str());
    }
    return true;
}

bool DBTAdapter::mgmtEvDeviceFoundHCI(const MgmtEvent& e) noexcept {
    COND_PRINT(debug_event, "DBTAdapter:hci:DeviceFound(dev_id %d): %s", dev_id, e.toString().c_str());
    const MgmtEvtDeviceFound &deviceFoundEvent = *static_cast<const MgmtEvtDeviceFound *>(&e);

    std::shared_ptr<EInfoReport> eir = deviceFoundEvent.getEIR();
    if( nullptr == eir ) {
        // Sourced from Linux Mgmt or otherwise ...
        eir = std::make_shared<EInfoReport>();
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
                dev->getAddressAndType().toString().c_str(), eir->toString().c_str());
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
                dev->getAddressAndType().toString().c_str(), eir->toString().c_str());

        int i=0;
        bool device_used = false;
        jau::for_each_fidelity(statusListenerList, [&](std::shared_ptr<AdapterStatusListener> &l) {
            try {
                if( l->matchDevice(*dev) ) {
                    device_used = l->deviceFound(dev, eir->getTimestamp()) || device_used;
                }
            } catch (std::exception &except) {
                ERR_PRINT("DBTAdapter:hci:DeviceFound: %d/%zd: %s of %s: Caught exception %s",
                        i+1, statusListenerList.size(),
                        l->toString().c_str(), dev->toString().c_str(), except.what());
            }
            i++;
        });
        if( !device_used ) {
            // keep to avoid duplicate finds: removeDiscoveredDevice(dev->addressAndType);
            // and still allowing usage, as connecting will re-add to shared list
            removeSharedDevice(*dev); // pending dtor if discovered is flushed
        } else if( EIRDataType::NONE != updateMask ) {
            sendDeviceUpdated("SharedDeviceFound", dev, eir->getTimestamp(), updateMask);
        }
        return true;
    }

    //
    // new device
    //
    // private ctor: dev = std::make_shared<DBTDevice>(*this, *eir);
    dev = std::shared_ptr<DBTDevice>(new DBTDevice(*this, *eir));
    addDiscoveredDevice(dev);
    addSharedDevice(dev);
    COND_PRINT(debug_event, "DBTAdapter:hci:DeviceFound: Use new %s, %s",
            dev->getAddressAndType().toString().c_str(), eir->toString().c_str());

    int i=0;
    bool device_used = false;
    jau::for_each_fidelity(statusListenerList, [&](std::shared_ptr<AdapterStatusListener> &l) {
        try {
            if( l->matchDevice(*dev) ) {
                device_used = l->deviceFound(dev, eir->getTimestamp()) || device_used;
            }
        } catch (std::exception &except) {
            ERR_PRINT("DBTAdapter:hci:DeviceFound-CBs %d/%zd: %s of %s: Caught exception %s",
                    i+1, statusListenerList.size(),
                    l->toString().c_str(), dev->toString().c_str(), except.what());
        }
        i++;
    });
    if( !device_used ) {
        // keep to avoid duplicate finds: removeDiscoveredDevice(dev->addressAndType);
        // and still allowing usage, as connecting will re-add to shared list
        removeSharedDevice(*dev); // pending dtor if discovered is flushed
    }
    return true;
}

bool DBTAdapter::mgmtEvDeviceUnpairedMgmt(const MgmtEvent& e) noexcept {
    const MgmtEvtDeviceUnpaired &event = *static_cast<const MgmtEvtDeviceUnpaired *>(&e);
    DBG_PRINT("DBTAdapter:mgmt:DeviceUnpaired: %s", event.toString().c_str());
    return true;
}
bool DBTAdapter::mgmtEvPinCodeRequestMgmt(const MgmtEvent& e) noexcept {
    const MgmtEvtPinCodeRequest &event = *static_cast<const MgmtEvtPinCodeRequest *>(&e);
    DBG_PRINT("DBTAdapter:mgmt:PinCodeRequest: %s", event.toString().c_str());
    return true;
}
bool DBTAdapter::mgmtEvAuthFailedMgmt(const MgmtEvent& e) noexcept {
    const MgmtEvtAuthFailed &event = *static_cast<const MgmtEvtAuthFailed *>(&e);

    std::shared_ptr<DBTDevice> device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr == device ) {
        WORDY_PRINT("DBTAdapter:hci:SMP: dev_id %d: Device not tracked: address[%s, %s], %s",
                dev_id, event.getAddress().toString().c_str(), getBDAddressTypeString(event.getAddressType()).c_str(),
                event.toString().c_str());
        return true;
    }
    const HCIStatusCode evtStatus = getHCIStatusCode( event.getStatus() );
    device->updatePairingState(device, e, evtStatus, SMPPairingState::FAILED);
    return true;
}
bool DBTAdapter::mgmtEvUserConfirmRequestMgmt(const MgmtEvent& e) noexcept {
    const MgmtEvtUserConfirmRequest &event = *static_cast<const MgmtEvtUserConfirmRequest *>(&e);

    std::shared_ptr<DBTDevice> device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr == device ) {
        WORDY_PRINT("DBTAdapter:hci:SMP: dev_id %d: Device not tracked: address[%s, %s], %s",
                dev_id, event.getAddress().toString().c_str(), getBDAddressTypeString(event.getAddressType()).c_str(),
                event.toString().c_str());
        return true;
    }
    // FIXME: Pass confirm_hint and value?
    device->updatePairingState(device, e, HCIStatusCode::SUCCESS, SMPPairingState::NUMERIC_COMPARE_EXPECTED);
    return true;
}
bool DBTAdapter::mgmtEvUserPasskeyRequestMgmt(const MgmtEvent& e) noexcept {
    const MgmtEvtUserPasskeyRequest &event = *static_cast<const MgmtEvtUserPasskeyRequest *>(&e);

    std::shared_ptr<DBTDevice> device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr == device ) {
        WORDY_PRINT("DBTAdapter:hci:SMP: dev_id %d: Device not tracked: address[%s, %s], %s",
                dev_id, event.getAddress().toString().c_str(), getBDAddressTypeString(event.getAddressType()).c_str(),
                event.toString().c_str());
        return true;
    }
    device->updatePairingState(device, e, HCIStatusCode::SUCCESS, SMPPairingState::PASSKEY_EXPECTED);
    return true;
}

bool DBTAdapter::hciSMPMsgCallback(const BDAddressAndType & addressAndType,
                                   const SMPPDUMsg& msg, const HCIACLData::l2cap_frame& source) noexcept {
    std::shared_ptr<DBTDevice> device = findConnectedDevice(addressAndType.address, addressAndType.type);
    if( nullptr == device ) {
        WORDY_PRINT("DBTAdapter:hci:SMP: dev_id %d: Device not tracked: address%s: %s, %s",
                dev_id, addressAndType.toString().c_str(),
                msg.toString().c_str(), source.toString().c_str());
        return true;
    }
    if( device->getConnectionHandle() != source.handle ) {
        WORDY_PRINT("DBTAdapter:hci:SMP: dev_id %d: ConnHandle mismatch address%s: %s, %s\n    -> %s",
                dev_id, addressAndType.toString().c_str(),
                msg.toString().c_str(), source.toString().c_str(), device->toString().c_str());
        return true;
    }

    device->hciSMPMsgCallback(device, msg, source);

    return true;
}

void DBTAdapter::sendDevicePairingState(std::shared_ptr<DBTDevice> device, const SMPPairingState state, const PairingMode mode, uint64_t timestamp) noexcept
{
    int i=0;
    jau::for_each_fidelity(statusListenerList, [&](std::shared_ptr<AdapterStatusListener> &l) {
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
    jau::for_each_fidelity(statusListenerList, [&](std::shared_ptr<AdapterStatusListener> &l) {
        try {
            // Only issue if valid && received connected confirmation (HCI) && not have called disconnect yet.
            if( device->isValid() && device->getConnected() && device->allowDisconnect ) {
                if( l->matchDevice(*device) ) {
                    l->deviceReady(device, timestamp);
                }
            }
        } catch (std::exception &except) {
            ERR_PRINT("DBTAdapter::sendDeviceReady: %d/%zd: %s of %s: Caught exception %s",
                    i+1, statusListenerList.size(),
                    l->toString().c_str(), device->toString().c_str(), except.what());
        }
        i++;
    });
}
