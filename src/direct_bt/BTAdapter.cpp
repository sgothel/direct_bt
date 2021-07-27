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

#include "BTAdapter.hpp"
#include "BTManager.hpp"

extern "C" {
    #include <inttypes.h>
    #include <unistd.h>
    #include <poll.h>
}

using namespace direct_bt;

constexpr static const bool _print_device_lists = false;

std::shared_ptr<BTDevice> BTAdapter::findDevice(device_list_t & devices, const EUI48 & address, const BDAddressType addressType) noexcept {
    const jau::nsize_t size = devices.size();
    for (jau::nsize_t i = 0; i < size; ++i) {
        std::shared_ptr<BTDevice> & e = devices[i];
        if ( nullptr != e && address == e->getAddressAndType().address && addressType == e->getAddressAndType().type) {
            return e;
        }
    }
    return nullptr;
}

std::shared_ptr<BTDevice> BTAdapter::findDevice(device_list_t & devices, BTDevice const & device) noexcept {
    const jau::nsize_t size = devices.size();
    for (jau::nsize_t i = 0; i < size; ++i) {
        std::shared_ptr<BTDevice> & e = devices[i];
        if ( nullptr != e && device == *e ) {
            return e;
        }
    }
    return nullptr;
}

bool BTAdapter::addConnectedDevice(const std::shared_ptr<BTDevice> & device) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_connectedDevices); // RAII-style acquire and relinquish via destructor
    jau::sc_atomic_critical sync(sync_data); // redundant due to mutex-lock cache-load operation, leaving it for doc
    if( nullptr != findDevice(connectedDevices, *device) ) {
        return false;
    }
    connectedDevices.push_back(device);
    return true;
}

bool BTAdapter::removeConnectedDevice(const BTDevice & device) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_connectedDevices); // RAII-style acquire and relinquish via destructor
    jau::sc_atomic_critical sync(sync_data); // redundant due to mutex-lock cache-load operation, leaving it for doc
    auto end = connectedDevices.end();
    for (auto it = connectedDevices.begin(); it != end; ++it) {
        if ( nullptr != *it && device == **it ) {
            connectedDevices.erase(it);
            return true;
        }
    }
    return false;
}

int BTAdapter::disconnectAllDevices(const HCIStatusCode reason) noexcept {
    device_list_t devices;
    {
        jau::sc_atomic_critical sync(sync_data); // redundant due to mutex-lock cache-load operation, leaving it for doc
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

std::shared_ptr<BTDevice> BTAdapter::findConnectedDevice (const EUI48 & address, const BDAddressType & addressType) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_connectedDevices); // RAII-style acquire and relinquish via destructor
    jau::sc_atomic_critical sync(sync_data); // redundant due to mutex-lock cache-load operation, leaving it for doc
    return findDevice(connectedDevices, address, addressType);
}

// *************************************************
// *************************************************
// *************************************************

bool BTAdapter::validateDevInfo() noexcept {
    bool ok = false;
    currentMetaScanType = ScanType::NONE;
    keep_le_scan_alive = false;

    if( !mgmt.isOpen() ) {
        ERR_PRINT("BTAdapter::validateDevInfo: Adapter[%d]: Manager not open", dev_id);
        goto errout0;
    }
    if( !hci.isOpen() ) {
        ERR_PRINT("BTAdapter::validateDevInfo: Adapter[%d]: HCIHandler closed", dev_id);
        goto errout0;
    }

    old_settings = adapterInfo.getCurrentSettingMask();

    btMode = getBTMode();
    if( BTMode::NONE == btMode ) {
        ERR_PRINT("BTAdapter::validateDevInfo: Adapter[%d]: BTMode invalid, BREDR nor LE set: %s", dev_id, adapterInfo.toString().c_str());
        return false;
    }
    hci.setBTMode(btMode);

    if( adapterInfo.isCurrentSettingBitSet(AdapterSetting::POWERED) ) {
        HCILocalVersion version;
        HCIStatusCode status = hci.getLocalVersion(version);
        if( HCIStatusCode::SUCCESS != status ) {
            ERR_PRINT("BTAdapter::validateDevInfo: Adapter[%d]: POWERED, LocalVersion failed %s - %s",
                    dev_id, to_string(status).c_str(), adapterInfo.toString().c_str());
            return false;
        } else {
            WORDY_PRINT("BTAdapter::validateDevInfo: Adapter[%d]: POWERED, %s - %s",
                    dev_id, version.toString().c_str(), adapterInfo.toString().c_str());
        }
    } else {
        WORDY_PRINT("BTAdapter::validateDevInfo: Adapter[%d]: Not POWERED: %s", dev_id, adapterInfo.toString().c_str());
    }
    ok = true;
    ok = mgmt.addMgmtEventCallback(dev_id, MgmtEvent::Opcode::DISCOVERING, jau::bindMemberFunc(this, &BTAdapter::mgmtEvDeviceDiscoveringMgmt)) && ok;
    ok = mgmt.addMgmtEventCallback(dev_id, MgmtEvent::Opcode::NEW_SETTINGS, jau::bindMemberFunc(this, &BTAdapter::mgmtEvNewSettingsMgmt)) && ok;
    ok = mgmt.addMgmtEventCallback(dev_id, MgmtEvent::Opcode::LOCAL_NAME_CHANGED, jau::bindMemberFunc(this, &BTAdapter::mgmtEvLocalNameChangedMgmt)) && ok;
    ok = mgmt.addMgmtEventCallback(dev_id, MgmtEvent::Opcode::PIN_CODE_REQUEST, jau::bindMemberFunc(this, &BTAdapter::mgmtEvPinCodeRequestMgmt));
    ok = mgmt.addMgmtEventCallback(dev_id, MgmtEvent::Opcode::USER_CONFIRM_REQUEST, jau::bindMemberFunc(this, &BTAdapter::mgmtEvUserConfirmRequestMgmt));
    ok = mgmt.addMgmtEventCallback(dev_id, MgmtEvent::Opcode::USER_PASSKEY_REQUEST, jau::bindMemberFunc(this, &BTAdapter::mgmtEvUserPasskeyRequestMgmt));
    ok = mgmt.addMgmtEventCallback(dev_id, MgmtEvent::Opcode::AUTH_FAILED, jau::bindMemberFunc(this, &BTAdapter::mgmtEvAuthFailedMgmt));
    ok = mgmt.addMgmtEventCallback(dev_id, MgmtEvent::Opcode::DEVICE_UNPAIRED, jau::bindMemberFunc(this, &BTAdapter::mgmtEvDeviceUnpairedMgmt));
    ok = mgmt.addMgmtEventCallback(dev_id, MgmtEvent::Opcode::PAIR_DEVICE_COMPLETE, jau::bindMemberFunc(this, &BTAdapter::mgmtEvPairDeviceCompleteMgmt));
    ok = mgmt.addMgmtEventCallback(dev_id, MgmtEvent::Opcode::NEW_LONG_TERM_KEY, jau::bindMemberFunc(this, &BTAdapter::mgmtEvNewLongTermKeyMgmt));

    if( !ok ) {
        ERR_PRINT("Could not add all required MgmtEventCallbacks to DBTManager: %s", toString().c_str());
        return false;
    }

#if 0
    mgmt.addMgmtEventCallback(dev_id, MgmtEvent::Opcode::DEVICE_DISCONNECTED, bindMemberFunc(this, &BTAdapter::mgmtEvDeviceDisconnectedMgmt));
#endif

    ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::DISCOVERING, jau::bindMemberFunc(this, &BTAdapter::mgmtEvDeviceDiscoveringHCI)) && ok;
    ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::DEVICE_CONNECTED, jau::bindMemberFunc(this, &BTAdapter::mgmtEvDeviceConnectedHCI)) && ok;
    ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::CONNECT_FAILED, jau::bindMemberFunc(this, &BTAdapter::mgmtEvConnectFailedHCI)) && ok;
    ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::DEVICE_DISCONNECTED, jau::bindMemberFunc(this, &BTAdapter::mgmtEvDeviceDisconnectedHCI)) && ok;
    ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::DEVICE_FOUND, jau::bindMemberFunc(this, &BTAdapter::mgmtEvDeviceFoundHCI)) && ok;
    ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::HCI_LE_REMOTE_USR_FEATURES, jau::bindMemberFunc(this, &BTAdapter::mgmtEvHCILERemoteUserFeaturesHCI)) && ok;
    ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::HCI_ENC_CHANGED, jau::bindMemberFunc(this, &BTAdapter::mgmtEvHCIEncryptionChangedHCI)) && ok;
    ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::HCI_ENC_KEY_REFRESH_COMPLETE, jau::bindMemberFunc(this, &BTAdapter::mgmtEvHCIEncryptionKeyRefreshCompleteHCI)) && ok;

    if( !ok ) {
        ERR_PRINT("Could not add all required MgmtEventCallbacks to HCIHandler: %s of %s", hci.toString().c_str(), toString().c_str());
        return false; // dtor local HCIHandler w/ closing
    }
    hci.addSMPMsgCallback(jau::bindMemberFunc(this, &BTAdapter::hciSMPMsgCallback));

    return true;

errout0:
    return false;
}

BTAdapter::BTAdapter(const BTAdapter::ctor_cookie& cc, BTManager& mgmt_, const AdapterInfo& adapterInfo_) noexcept
: debug_event(jau::environment::getBooleanProperty("direct_bt.debug.adapter.event", false)),
  debug_lock(jau::environment::getBooleanProperty("direct_bt.debug.adapter.lock", false)),
  mgmt( mgmt_ ),
  adapterInfo( adapterInfo_ ),
  dev_id( adapterInfo.dev_id ),
  hci( dev_id )
{
    (void)cc;
    valid = validateDevInfo();
}

BTAdapter::~BTAdapter() noexcept {
    if( !isValid() ) {
        DBG_PRINT("BTAdapter::dtor: dev_id %d, invalid, %p", dev_id, this);
        mgmt.removeAdapter(this); // remove this instance from manager
        return;
    }
    DBG_PRINT("BTAdapter::dtor: ... %p %s", this, toString().c_str());
    close();

    mgmt.removeAdapter(this); // remove this instance from manager

    DBG_PRINT("BTAdapter::dtor: XXX");
}

void BTAdapter::close() noexcept {
    if( !isValid() ) {
        // Native user app could have destroyed this instance already from
        DBG_PRINT("BTAdapter::close: dev_id %d, invalid, %p", dev_id, this);
        return;
    }
    DBG_PRINT("BTAdapter::close: ... %p %s", this, toString().c_str());
    keep_le_scan_alive = false;

    // mute all listener first
    {
        int count = mgmt.removeMgmtEventCallback(dev_id);
        DBG_PRINT("BTAdapter::close removeMgmtEventCallback: %d callbacks", count);
    }
    statusListenerList.clear();

    poweredOff();

    DBG_PRINT("BTAdapter::close: closeHCI: ...");
    hci.close();
    DBG_PRINT("BTAdapter::close: closeHCI: XXX");

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
    DBG_PRINT("BTAdapter::close: XXX");
}

void BTAdapter::poweredOff() noexcept {
    if( !isValid() ) {
        DBG_PRINT("BTAdapter::poweredOff: dev_id %d, invalid, %p", dev_id, this);
        return;
    }
    DBG_PRINT("BTAdapter::poweredOff: ... %p %s", this, toString().c_str());
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

    DBG_PRINT("BTAdapter::poweredOff: XXX");
}

void BTAdapter::printDeviceList(const std::string& prefix, const BTAdapter::device_list_t& list) noexcept {
    const size_t sz = list.size();
    jau::PLAIN_PRINT(true, "- BTAdapter::%s: %zu elements", prefix.c_str(), sz);
    int idx = 0;
    for (auto it = list.begin(); it != list.end(); ++idx, ++it) {
        jau::PLAIN_PRINT(true, "  - %d / %zu: %s, name '%s'", (idx+1), sz,
                (*it)->getAddressAndType().toString().c_str(),
                (*it)->getName().c_str() );
    }
}
void BTAdapter::printDeviceLists() noexcept {
    device_list_t _sharedDevices, _discoveredDevices, _connectedDevices;
    {
        jau::sc_atomic_critical sync(sync_data); // lock-free simple cache-load 'snapshot'
        // Using an expensive copy here: debug mode only
        _sharedDevices = sharedDevices;
        _discoveredDevices = discoveredDevices;
        _connectedDevices = connectedDevices;
    }
    printDeviceList("SharedDevices     ", _sharedDevices);
    printDeviceList("ConnectedDevices  ", _connectedDevices);
    printDeviceList("DiscoveredDevices ", _discoveredDevices);
    printStatusListenerList();
}

void BTAdapter::printStatusListenerList() noexcept {
    auto begin = statusListenerList.begin(); // lock mutex and copy_store
    jau::PLAIN_PRINT(true, "- BTAdapter::StatusListener    : %zu elements", (size_t)begin.size());
    for(int ii=0; !begin.is_end(); ++ii, ++begin ) {
        jau::PLAIN_PRINT(true, "  - %d / %zu: %s", (ii+1), (size_t)begin.size(), begin->listener->toString().c_str());
    }
}

std::shared_ptr<NameAndShortName> BTAdapter::setLocalName(const std::string &name, const std::string &short_name) noexcept {
    return mgmt.setLocalName(dev_id, name, short_name);
}

bool BTAdapter::setDiscoverable(bool value) noexcept {
    AdapterSetting current_settings { AdapterSetting::NONE } ;
    return MgmtStatus::SUCCESS == mgmt.setDiscoverable(dev_id, value ? 0x01 : 0x00, 10 /* timeout seconds */, current_settings);
}

bool BTAdapter::setBondable(bool value) noexcept {
    AdapterSetting current_settings { AdapterSetting::NONE } ;
    return mgmt.setMode(dev_id, MgmtCommand::Opcode::SET_BONDABLE, value ? 1 : 0, current_settings);
}

bool BTAdapter::setPowered(bool value) noexcept {
    AdapterSetting current_settings { AdapterSetting::NONE } ;
    return mgmt.setMode(dev_id, MgmtCommand::Opcode::SET_POWERED, value ? 1 : 0, current_settings);
}

bool BTAdapter::lockConnect(const BTDevice & device, const bool wait, const SMPIOCapability io_cap) noexcept {
    std::unique_lock<std::mutex> lock(mtx_single_conn_device); // RAII-style acquire and relinquish via destructor
    const uint32_t timeout_ms = 10000; // FIXME: Configurable?

    if( nullptr != single_conn_device_ptr ) {
        if( device == *single_conn_device_ptr ) {
            COND_PRINT(debug_lock, "BTAdapter::lockConnect: Success: Already locked, same device: %s", device.toString().c_str());
            return true; // already set, same device: OK, locked
        }
        if( wait ) {
            while( nullptr != single_conn_device_ptr ) {
                std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
                std::cv_status s = cv_single_conn_device.wait_until(lock, t0 + std::chrono::milliseconds(timeout_ms));
                if( std::cv_status::timeout == s && nullptr != single_conn_device_ptr ) {
                    if( debug_lock ) {
                        jau::PLAIN_PRINT(true, "BTAdapter::lockConnect: Failed: Locked (waited)");
                        jau::PLAIN_PRINT(true, " - locked-by-other-device %s", single_conn_device_ptr->toString().c_str());
                        jau::PLAIN_PRINT(true, " - lock-failed-for %s", device.toString().c_str());
                    }
                    return false;
                }
            }
            // lock was released
        } else {
            if( debug_lock ) {
                jau::PLAIN_PRINT(true, "BTAdapter::lockConnect: Failed: Locked (no-wait)");
                jau::PLAIN_PRINT(true, " - locked-by-other-device %s", single_conn_device_ptr->toString().c_str());
                jau::PLAIN_PRINT(true, " - lock-failed-for %s", device.toString().c_str());
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
            COND_PRINT(debug_lock, "BTAdapter::lockConnect: Success: New lock, setIOCapability[%s -> %s], %s",
                to_string(pre_io_cap).c_str(), to_string(io_cap).c_str(),
                device.toString().c_str());
            return true;
        } else {
            // failed, unlock and exit
            COND_PRINT(debug_lock, "BTAdapter::lockConnect: Failed: setIOCapability[%s], %s",
                to_string(io_cap).c_str(), device.toString().c_str());
            single_conn_device_ptr = nullptr;
            cv_single_conn_device.notify_all(); // notify waiting getter
            return false;
        }
#else
        COND_PRINT(debug_lock, "BTAdapter::lockConnect: Success: New lock, ignored io-cap: %s, %s",
                to_string(io_cap).c_str()
                device.toString().c_str());
        return true;
#endif
    } else {
        COND_PRINT(debug_lock, "BTAdapter::lockConnect: Success: New lock, no io-cap: %s", device.toString().c_str());
        return true;
    }
}

bool BTAdapter::unlockConnect(const BTDevice & device) noexcept {
    std::unique_lock<std::mutex> lock(mtx_single_conn_device); // RAII-style acquire and relinquish via destructor

    if( nullptr != single_conn_device_ptr && device == *single_conn_device_ptr ) {
        const SMPIOCapability v = iocap_defaultval;
        iocap_defaultval  = SMPIOCapability::UNSET;
        if( SMPIOCapability::UNSET != v ) {
            // Unreachable: !USE_LINUX_BT_SECURITY
            SMPIOCapability o;
            const bool res = mgmt.setIOCapability(dev_id, v, o);
            COND_PRINT(debug_lock, "BTAdapter::unlockConnect: Success: setIOCapability[res %d: %s -> %s], %s",
                res, to_string(o).c_str(), to_string(v).c_str(),
                single_conn_device_ptr->toString().c_str());
        } else {
            COND_PRINT(debug_lock, "BTAdapter::unlockConnect: Success: %s",
                single_conn_device_ptr->toString().c_str());
        }
        single_conn_device_ptr = nullptr;
        cv_single_conn_device.notify_all(); // notify waiting getter
        return true;
    } else {
        if( debug_lock ) {
            const std::string other_device_str = nullptr != single_conn_device_ptr ? single_conn_device_ptr->toString() : "null";
            jau::PLAIN_PRINT(true, "BTAdapter::unlockConnect: Not locked:");
            jau::PLAIN_PRINT(true, " - locked-by-other-device %s", other_device_str.c_str());
            jau::PLAIN_PRINT(true, " - unlock-failed-for %s", device.toString().c_str());
        }
        return false;
    }
}

bool BTAdapter::unlockConnectAny() noexcept {
    std::unique_lock<std::mutex> lock(mtx_single_conn_device); // RAII-style acquire and relinquish via destructor

    if( nullptr != single_conn_device_ptr ) {
        const SMPIOCapability v = iocap_defaultval;
        iocap_defaultval  = SMPIOCapability::UNSET;
        if( SMPIOCapability::UNSET != v ) {
            // Unreachable: !USE_LINUX_BT_SECURITY
            SMPIOCapability o;
            const bool res = mgmt.setIOCapability(dev_id, v, o);
            COND_PRINT(debug_lock, "BTAdapter::unlockConnectAny: Success: setIOCapability[res %d: %s -> %s]; %s",
                res, to_string(o).c_str(), to_string(v).c_str(),
                single_conn_device_ptr->toString().c_str());
        } else {
            COND_PRINT(debug_lock, "BTAdapter::unlockConnectAny: Success: %s",
                single_conn_device_ptr->toString().c_str());
        }
        single_conn_device_ptr = nullptr;
        cv_single_conn_device.notify_all(); // notify waiting getter
        return true;
    } else {
        iocap_defaultval = SMPIOCapability::UNSET;
        COND_PRINT(debug_lock, "BTAdapter::unlockConnectAny: Not locked");
        return false;
    }
}

HCIStatusCode BTAdapter::reset() noexcept {
    if( !isValid() ) {
        ERR_PRINT("BTAdapter::reset(): Adapter invalid: %s, %s", to_hexstring(this).c_str(), toString().c_str());
        return HCIStatusCode::UNSPECIFIED_ERROR;
    }
    if( !hci.isOpen() ) {
        ERR_PRINT("BTAdapter::reset(): HCI closed: %s, %s", to_hexstring(this).c_str(), toString().c_str());
        return HCIStatusCode::UNSPECIFIED_ERROR;
    }
#if 0
    const bool wasPowered = isPowered();
    // Adapter will be reset, close connections and cleanup off-thread.
    preReset();

    HCIStatusCode status = hci->reset();
    if( HCIStatusCode::SUCCESS != status ) {
        ERR_PRINT("BTAdapter::reset: reset failed: %s", to_string(status).c_str());
    } else if( wasPowered ) {
        if( !setPowered(true) ) {
            ERR_PRINT("BTAdapter::reset: setPowered(true) failed");
            status = HCIStatusCode::UNSPECIFIED_ERROR;
        }
    }
    return status;
#else
    return hci.resetAdapter();
#endif
}

bool BTAdapter::isDeviceWhitelisted(const BDAddressAndType & addressAndType) noexcept {
    return mgmt.isDeviceWhitelisted(dev_id, addressAndType);
}

bool BTAdapter::addDeviceToWhitelist(const BDAddressAndType & addressAndType, const HCIWhitelistConnectType ctype,
                                      const uint16_t conn_interval_min, const uint16_t conn_interval_max,
                                      const uint16_t conn_latency, const uint16_t timeout) {
    if( !isPowered() ) {
        ERR_PRINT("BTAdapter::startDiscovery: Adapter not powered: %s", toString().c_str());
        return false;
    }
    if( mgmt.isDeviceWhitelisted(dev_id, addressAndType) ) {
        ERR_PRINT("BTAdapter::addDeviceToWhitelist: device already listed: dev_id %d, address%s", dev_id, addressAndType.toString().c_str());
        return true;
    }

    if( !mgmt.uploadConnParam(dev_id, addressAndType, conn_interval_min, conn_interval_max, conn_latency, timeout) ) {
        ERR_PRINT("BTAdapter::addDeviceToWhitelist: uploadConnParam(dev_id %d, address%s, interval[%u..%u], latency %u, timeout %u): Failed",
                dev_id, addressAndType.toString().c_str(), conn_interval_min, conn_interval_max, conn_latency, timeout);
    }
    return mgmt.addDeviceToWhitelist(dev_id, addressAndType, ctype);
}

bool BTAdapter::removeDeviceFromWhitelist(const BDAddressAndType & addressAndType) {
    return mgmt.removeDeviceFromWhitelist(dev_id, addressAndType);
}

static jau::cow_darray<impl::StatusListenerPair>::equal_comparator _adapterStatusListenerRefEqComparator =
        [](const impl::StatusListenerPair &a, const impl::StatusListenerPair &b) -> bool { return *a.listener == *b.listener; };

bool BTAdapter::addStatusListener(std::shared_ptr<AdapterStatusListener> l) {
    if( nullptr == l ) {
        throw jau::IllegalArgumentException("AdapterStatusListener ref is null", E_FILE_LINE);
    }
    const bool added = statusListenerList.push_back_unique(impl::StatusListenerPair{l, std::weak_ptr<BTDevice>{} },
                                                           _adapterStatusListenerRefEqComparator);
    if( added ) {
        sendAdapterSettingsInitial(*l, jau::getCurrentMilliseconds());
    }
    if( _print_device_lists || jau::environment::get().verbose ) {
        jau::PLAIN_PRINT(true, "BTAdapter::addStatusListener.1: added %d, %s", added, toString().c_str());
        printDeviceLists();
    }
    return added;
}

bool BTAdapter::addStatusListener(const BTDevice& d, std::shared_ptr<AdapterStatusListener> l) {
    if( nullptr == l ) {
        throw jau::IllegalArgumentException("AdapterStatusListener ref is null", E_FILE_LINE);
    }

    std::shared_ptr<BTDevice> sd = getSharedDevice(d);
    if( nullptr == sd ) {
        throw jau::IllegalArgumentException("Device not shared: "+d.toString(), E_FILE_LINE);
    }
    const bool added = statusListenerList.push_back_unique(impl::StatusListenerPair{l, sd},
                                                           _adapterStatusListenerRefEqComparator);
    if( added ) {
        sendAdapterSettingsInitial(*l, jau::getCurrentMilliseconds());
    }
    if( _print_device_lists || jau::environment::get().verbose ) {
        jau::PLAIN_PRINT(true, "BTAdapter::addStatusListener.2: added %d, %s", added, toString().c_str());
        printDeviceLists();
    }
    return added;
}

bool BTAdapter::removeStatusListener(std::shared_ptr<AdapterStatusListener> l) {
    if( nullptr == l ) {
        throw jau::IllegalArgumentException("AdapterStatusListener ref is null", E_FILE_LINE);
    }
    const int count = statusListenerList.erase_matching(impl::StatusListenerPair{l, std::weak_ptr<BTDevice>{}},
                                                        false /* all_matching */,
                                                        _adapterStatusListenerRefEqComparator);
    if( _print_device_lists || jau::environment::get().verbose ) {
        jau::PLAIN_PRINT(true, "BTAdapter::removeStatusListener.1: res %d, %s", count>0, toString().c_str());
        printDeviceLists();
    }
    return count > 0;
}

bool BTAdapter::removeStatusListener(const AdapterStatusListener * l) {
    if( nullptr == l ) {
        throw jau::IllegalArgumentException("AdapterStatusListener ref is null", E_FILE_LINE);
    }
    bool res = false;
    {
        auto begin = statusListenerList.begin(); // lock mutex and copy_store
        while ( !begin.is_end() ) {
            if ( *begin->listener == *l ) {
                begin.erase();
                res = true;
                break;
            } else {
                ++begin;
            }
        }
        if( res ) {
            begin.write_back();
        }
    }
    if( _print_device_lists || jau::environment::get().verbose ) {
        jau::PLAIN_PRINT(true, "BTAdapter::removeStatusListener.2: res %d, %s", res, toString().c_str());
        printDeviceLists();
    }
    return res;
}

int BTAdapter::removeAllStatusListener(const BTDevice& d) {
    int count = 0;

    int res = statusListenerList.size();
    if( 0 < res ) {
        auto begin = statusListenerList.begin();
        auto it = begin.end();
        do {
            --it;
            std::shared_ptr<BTDevice> sda = it->wbr_device.lock();
            if ( nullptr != sda && *sda == d ) {
                it.erase();
                ++count;
            }
        } while( it != begin );

        if( 0 < count ) {
            begin.write_back();
        }
    }
    return count;
}

int BTAdapter::removeAllStatusListener() {
    int count = statusListenerList.size();
    statusListenerList.clear();
    return count;
}

void BTAdapter::checkDiscoveryState() noexcept {
    const ScanType currentNativeScanType = hci.getCurrentScanType();
    // Check LE scan state
    if( keep_le_scan_alive == false ) {
        if( hasScanType(currentMetaScanType, ScanType::LE) != hasScanType(currentNativeScanType, ScanType::LE) ) {
            std::string msg("Invalid DiscoveryState: keepAlive "+std::to_string(keep_le_scan_alive.load())+
                    ", currentScanType*[native "+
                    to_string(currentNativeScanType)+" != meta "+
                    to_string(currentMetaScanType)+"]");
            ERR_PRINT(msg.c_str());
            // ABORT?
        }
    } else {
        if( !hasScanType(currentMetaScanType, ScanType::LE) && hasScanType(currentNativeScanType, ScanType::LE) ) {
            std::string msg("Invalid DiscoveryState: keepAlive "+std::to_string(keep_le_scan_alive.load())+
                    ", currentScanType*[native "+
                    to_string(currentNativeScanType)+", meta "+
                    to_string(currentMetaScanType)+"]");
            ERR_PRINT(msg.c_str());
            // ABORT?
        }
    }
}

HCIStatusCode BTAdapter::startDiscovery(const bool keepAlive, const HCILEOwnAddressType own_mac_type,
                                         const uint16_t le_scan_interval, const uint16_t le_scan_window)
{
    // FIXME: Respect BTAdapter::btMode, i.e. BTMode::BREDR, BTMode::LE or BTMode::DUAL to setup BREDR, LE or DUAL scanning!
    // ERR_PRINT("Test");
    // throw jau::RuntimeException("Test", E_FILE_LINE);

    if( !isPowered() ) {
        WARN_PRINT("BTAdapter::startDiscovery: Adapter not powered: %s", toString(true).c_str());
        return HCIStatusCode::UNSPECIFIED_ERROR;
    }
    const std::lock_guard<std::mutex> lock(mtx_discovery); // RAII-style acquire and relinquish via destructor

    const ScanType currentNativeScanType = hci.getCurrentScanType();

    if( hasScanType(currentNativeScanType, ScanType::LE) ) {
        removeDiscoveredDevices();
        if( keep_le_scan_alive == keepAlive ) {
            DBG_PRINT("BTAdapter::startDiscovery: Already discovering, unchanged keepAlive %d -> %d, currentScanType[native %s, meta %s] ...\n- %s",
                    keep_le_scan_alive.load(), keepAlive,
                    to_string(currentNativeScanType).c_str(), to_string(currentMetaScanType).c_str(), toString(true).c_str());
        } else {
            DBG_PRINT("BTAdapter::startDiscovery: Already discovering, changed keepAlive %d -> %d, currentScanType[native %s, meta %s] ...\n- %s",
                    keep_le_scan_alive.load(), keepAlive,
                    to_string(currentNativeScanType).c_str(), to_string(currentMetaScanType).c_str(), toString(true).c_str());
            keep_le_scan_alive = keepAlive;
        }
        checkDiscoveryState();
        return HCIStatusCode::SUCCESS;
    }

    if( _print_device_lists || jau::environment::get().verbose ) {
        jau::PLAIN_PRINT(true, "BTAdapter::startDiscovery: Start: keepAlive %d -> %d, currentScanType[native %s, meta %s] ...\n- %s",
                keep_le_scan_alive.load(), keepAlive,
                to_string(currentNativeScanType).c_str(), to_string(currentMetaScanType).c_str(), toString().c_str());
    }

    removeDiscoveredDevices();
    keep_le_scan_alive = keepAlive;

    // if le_enable_scan(..) is successful, it will issue 'mgmtEvDeviceDiscoveringHCI(..)' immediately, which updates currentMetaScanType.
    const HCIStatusCode status = hci.le_start_scan(true /* filter_dup */, own_mac_type, le_scan_interval, le_scan_window);

    if( _print_device_lists || jau::environment::get().verbose ) {
        jau::PLAIN_PRINT(true, "BTAdapter::startDiscovery: End: Result %s, keepAlive %d -> %d, currentScanType[native %s, meta %s] ...\n- %s",
                to_string(status).c_str(), keep_le_scan_alive.load(), keepAlive,
                to_string(hci.getCurrentScanType()).c_str(), to_string(currentMetaScanType).c_str(), toString().c_str());
        printDeviceLists();
    }

    checkDiscoveryState();

    return status;
}

void BTAdapter::startDiscoveryBackground() noexcept {
    // FIXME: Respect BTAdapter::btMode, i.e. BTMode::BREDR, BTMode::LE or BTMode::DUAL to setup BREDR, LE or DUAL scanning!
    if( !isPowered() ) {
        WARN_PRINT("BTAdapter::startDiscoveryBackground: Adapter not powered: %s", toString(true).c_str());
        return;
    }
    const std::lock_guard<std::mutex> lock(mtx_discovery); // RAII-style acquire and relinquish via destructor
    if( !hasScanType(hci.getCurrentScanType(), ScanType::LE) && keep_le_scan_alive ) { // still?
        // if le_enable_scan(..) is successful, it will issue 'mgmtEvDeviceDiscoveringHCI(..)' immediately, which updates currentMetaScanType.
        const HCIStatusCode status = hci.le_enable_scan(true /* enable */);
        if( HCIStatusCode::SUCCESS != status ) {
            ERR_PRINT("BTAdapter::startDiscoveryBackground: le_enable_scan failed: %s", to_string(status).c_str());
        }
        checkDiscoveryState();
    }
}

HCIStatusCode BTAdapter::stopDiscovery() noexcept {
    // We allow !isEnabled, to utilize method for adjusting discovery state and notifying listeners
    // FIXME: Respect BTAdapter::btMode, i.e. BTMode::BREDR, BTMode::LE or BTMode::DUAL to stop BREDR, LE or DUAL scanning!
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

    DBG_PRINT("BTAdapter::stopDiscovery: Start: keepAlive %d, currentScanType[native %s, meta %s], le_scan_temp_disabled %d ...",
            keep_le_scan_alive.load(),
            to_string(currentNativeScanType).c_str(), to_string(currentMetaScanType).c_str(),
            le_scan_temp_disabled);

    keep_le_scan_alive = false;
    if( !hasScanType(currentMetaScanType, ScanType::LE) ) {
        DBG_PRINT("BTAdapter::stopDiscovery: Already disabled, keepAlive %d, currentScanType[native %s, meta %s] ...",
                keep_le_scan_alive.load(),
                to_string(currentNativeScanType).c_str(), to_string(currentMetaScanType).c_str());
        checkDiscoveryState();
        return HCIStatusCode::SUCCESS;
    }

    HCIStatusCode status;
    if( !adapterInfo.isCurrentSettingBitSet(AdapterSetting::POWERED) ) {
        WARN_PRINT("BTAdapter::stopDiscovery: Powered off: %s", toString(true).c_str());
        hci.setCurrentScanType(ScanType::NONE);
        currentMetaScanType = ScanType::NONE;
        status = HCIStatusCode::UNSPECIFIED_ERROR;
        goto exit;
    }
    if( !hci.isOpen() ) {
        ERR_PRINT("BTAdapter::stopDiscovery: HCI closed: %s", toString(true).c_str());
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
            ERR_PRINT("BTAdapter::stopDiscovery: le_enable_scan failed: %s", to_string(status).c_str());
        }
    }

exit:
    if( le_scan_temp_disabled || HCIStatusCode::SUCCESS != status ) {
        // In case of discoveryTempDisabled, power-off, le_enable_scane failure
        // or already closed HCIHandler, send the event directly.
        const MgmtEvtDiscovering e(dev_id, ScanType::LE, false);
        mgmtEvDeviceDiscoveringHCI( e );
    }
    if( _print_device_lists || jau::environment::get().verbose ) {
        jau::PLAIN_PRINT(true, "BTAdapter::stopDiscovery: End: Result %s, keepAlive %d, currentScanType[native %s, meta %s], le_scan_temp_disabled %d ...\n- %s",
                to_string(status).c_str(), keep_le_scan_alive.load(),
                to_string(hci.getCurrentScanType()).c_str(), to_string(currentMetaScanType).c_str(), le_scan_temp_disabled,
                toString().c_str());
        printDeviceLists();
    }

    checkDiscoveryState();

    return status;
}

// *************************************************

std::shared_ptr<BTDevice> BTAdapter::findDiscoveredDevice (const EUI48 & address, const BDAddressType addressType) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_discoveredDevices); // RAII-style acquire and relinquish via destructor
    jau::sc_atomic_critical sync(sync_data); // redundant due to mutex-lock cache-load operation, leaving it for doc
    return findDevice(discoveredDevices, address, addressType);
}

bool BTAdapter::addDiscoveredDevice(std::shared_ptr<BTDevice> const &device) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_discoveredDevices); // RAII-style acquire and relinquish via destructor
    jau::sc_atomic_critical sync(sync_data); // redundant due to mutex-lock cache-load operation, leaving it for doc
    if( nullptr != findDevice(discoveredDevices, *device) ) {
        // already discovered
        return false;
    }
    discoveredDevices.push_back(device);
    return true;
}

bool BTAdapter::removeDiscoveredDevice(const BDAddressAndType & addressAndType) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_discoveredDevices); // RAII-style acquire and relinquish via destructor
    jau::sc_atomic_critical sync(sync_data); // redundant due to mutex-lock cache-load operation, leaving it for doc
    for (auto it = discoveredDevices.begin(); it != discoveredDevices.end(); ++it) {
        const BTDevice& device = **it;
        if ( nullptr != *it && addressAndType == device.addressAndType ) {
            if( nullptr == getSharedDevice( device ) ) {
                removeAllStatusListener( device );
            }
            discoveredDevices.erase(it);
            return true;
        }
    }
    return false;
}


int BTAdapter::removeDiscoveredDevices() noexcept {
    const std::lock_guard<std::mutex> lock(mtx_discoveredDevices); // RAII-style acquire and relinquish via destructor
    jau::sc_atomic_critical sync(sync_data); // redundant due to mutex-lock cache-load operation, leaving it for doc

    int res = discoveredDevices.size();
    if( 0 < res ) {
        auto it = discoveredDevices.end();
        do {
            --it;
            const BTDevice& device = **it;
            if( nullptr == getSharedDevice( device ) ) {
                removeAllStatusListener( device );
            }
            discoveredDevices.erase(it);
        } while( it != discoveredDevices.begin() );
    }

    if( _print_device_lists || jau::environment::get().verbose ) {
        jau::PLAIN_PRINT(true, "BTAdapter::removeDiscoveredDevices: End: %d, %s", res, toString().c_str());
        printDeviceLists();
    }
    return res;
}

jau::darray<std::shared_ptr<BTDevice>> BTAdapter::getDiscoveredDevices() const noexcept {
    jau::sc_atomic_critical sync(sync_data); // lock-free simple cache-load 'snapshot'
    device_list_t res = discoveredDevices;
    return res;
}

bool BTAdapter::addSharedDevice(std::shared_ptr<BTDevice> const &device) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_sharedDevices); // RAII-style acquire and relinquish via destructor
    if( nullptr != findDevice(sharedDevices, *device) ) {
        // already shared
        return false;
    }
    sharedDevices.push_back(device);
    return true;
}

std::shared_ptr<BTDevice> BTAdapter::getSharedDevice(const BTDevice & device) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_sharedDevices); // RAII-style acquire and relinquish via destructor
    return findDevice(sharedDevices, device);
}

void BTAdapter::removeSharedDevice(const BTDevice & device) noexcept {
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

std::shared_ptr<BTDevice> BTAdapter::findSharedDevice (const EUI48 & address, const BDAddressType addressType) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_sharedDevices); // RAII-style acquire and relinquish via destructor
    return findDevice(sharedDevices, address, addressType);
}

void BTAdapter::removeDevice(BTDevice & device) noexcept {
    WORDY_PRINT("DBTAdapter::removeDevice: Start %s", toString().c_str());
    removeAllStatusListener(device);

    const HCIStatusCode status = device.disconnect(HCIStatusCode::REMOTE_USER_TERMINATED_CONNECTION);
    WORDY_PRINT("BTAdapter::removeDevice: disconnect %s, %s", to_string(status).c_str(), toString().c_str());
    unlockConnect(device);
    removeConnectedDevice(device); // usually done in BTAdapter::mgmtEvDeviceDisconnectedHCI
    removeDiscoveredDevice(device.addressAndType); // usually done in BTAdapter::mgmtEvDeviceDisconnectedHCI
    removeSharedDevice(device);

    if( _print_device_lists || jau::environment::get().verbose ) {
        jau::PLAIN_PRINT(true, "BTAdapter::removeDevice: End %s, %s", device.getAddressAndType().toString().c_str(), toString().c_str());
        printDeviceLists();
    }
}

std::string BTAdapter::toString(bool includeDiscoveredDevices) const noexcept {
    std::string out("Adapter[BTMode "+to_string(btMode)+", "+adapterInfo.address.toString()+", '"+getName()+"', id "+std::to_string(dev_id)+
                    ", curSettings"+to_string(adapterInfo.getCurrentSettingMask())+
                    ", scanType[native "+to_string(hci.getCurrentScanType())+", meta "+to_string(currentMetaScanType)+"]"
                    ", valid "+std::to_string(isValid())+", open[mgmt, "+std::to_string(mgmt.isOpen())+", hci "+std::to_string(hci.isOpen())+
                    "], "+javaObjectToString()+"]");
    if( includeDiscoveredDevices ) {
        device_list_t devices = getDiscoveredDevices();
        if( devices.size() > 0 ) {
            out.append("\n");
            for(auto it = devices.begin(); it != devices.end(); it++) {
                std::shared_ptr<BTDevice> p = *it;
                if( nullptr != p ) {
                    out.append("  ").append(p->toString()).append("\n");
                }
            }
        }
    }
    return out;
}

// *************************************************

void BTAdapter::sendAdapterSettingsChanged(const AdapterSetting old_settings_, const AdapterSetting current_settings, AdapterSetting changes,
                                            const uint64_t timestampMS) noexcept
{
    int i=0;
    jau::for_each_fidelity(statusListenerList, [&](impl::StatusListenerPair &p) {
        try {
            p.listener->adapterSettingsChanged(*this, old_settings_, current_settings, changes, timestampMS);
        } catch (std::exception &e) {
            ERR_PRINT("BTAdapter:CB:NewSettings-CBs %d/%zd: %s of %s: Caught exception %s",
                    i+1, statusListenerList.size(),
                    p.listener->toString().c_str(), toString().c_str(), e.what());
        }
        i++;
    });
}

void BTAdapter::sendAdapterSettingsInitial(AdapterStatusListener & asl, const uint64_t timestampMS) noexcept
{
    const AdapterSetting current_settings = adapterInfo.getCurrentSettingMask();
    COND_PRINT(debug_event, "BTAdapter::sendAdapterSettingsInitial: NONE -> %s, changes NONE: %s",
            to_string(current_settings).c_str(), toString().c_str() );
    try {
        asl.adapterSettingsChanged(*this, AdapterSetting::NONE, current_settings, AdapterSetting::NONE, timestampMS);
    } catch (std::exception &e) {
        ERR_PRINT("BTAdapter::sendAdapterSettingsChanged-CB: %s of %s: Caught exception %s",
                asl.toString().c_str(), toString().c_str(), e.what());
    }
}

void BTAdapter::sendDeviceUpdated(std::string cause, std::shared_ptr<BTDevice> device, uint64_t timestamp, EIRDataType updateMask) noexcept {
    int i=0;
    jau::for_each_fidelity(statusListenerList, [&](impl::StatusListenerPair &p) {
        try {
            if( p.listener->matchDevice(*device) ) {
                p.listener->deviceUpdated(device, updateMask, timestamp);
            }
        } catch (std::exception &e) {
            ERR_PRINT("BTAdapter::sendDeviceUpdated-CBs (%s) %d/%zd: %s of %s: Caught exception %s",
                    cause.c_str(), i+1, statusListenerList.size(),
                    p.listener->toString().c_str(), device->toString().c_str(), e.what());
        }
        i++;
    });
}

// *************************************************

bool BTAdapter::mgmtEvDeviceDiscoveringHCI(const MgmtEvent& e) noexcept {
    return mgmtEvDeviceDiscoveringAny(e, true /* hciSourced */ );
}

bool BTAdapter::mgmtEvDeviceDiscoveringMgmt(const MgmtEvent& e) noexcept {
    return mgmtEvDeviceDiscoveringAny(e, false /* hciSourced */ );
}

bool BTAdapter::mgmtEvDeviceDiscoveringAny(const MgmtEvent& e, const bool hciSourced) noexcept {
    const std::string srctkn = hciSourced ? "hci" : "mgmt";
    const MgmtEvtDiscovering &event = *static_cast<const MgmtEvtDiscovering *>(&e);
    const ScanType eventScanType = event.getScanType();
    const bool eventEnabled = event.getEnabled();
    ScanType currentNativeScanType = hci.getCurrentScanType();

    // FIXME: Respect BTAdapter::btMode, i.e. BTMode::BREDR, BTMode::LE or BTMode::DUAL to setup BREDR, LE or DUAL scanning!
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
        DBG_PRINT("BTAdapter:%s:DeviceDiscovering: dev_id %d, keepDiscoveringAlive %d: scanType[native %s -> %s, meta %s -> %s]): %s",
            srctkn.c_str(), dev_id, keep_le_scan_alive.load(),
            to_string(currentNativeScanType).c_str(), to_string(nextNativeScanType).c_str(),
            to_string(currentMetaScanType).c_str(), to_string(nextMetaScanType).c_str(),
            event.toString().c_str());
        currentNativeScanType = nextNativeScanType;
        hci.setCurrentScanType(currentNativeScanType);
    } else {
        DBG_PRINT("BTAdapter:%s:DeviceDiscovering: dev_id %d, keepDiscoveringAlive %d: scanType[native %s, meta %s -> %s]): %s",
            srctkn.c_str(), dev_id, keep_le_scan_alive.load(),
            to_string(currentNativeScanType).c_str(),
            to_string(currentMetaScanType).c_str(), to_string(nextMetaScanType).c_str(),
            event.toString().c_str());
    }
    currentMetaScanType = nextMetaScanType;

    checkDiscoveryState();

    int i=0;
    jau::for_each_fidelity(statusListenerList, [&](impl::StatusListenerPair &p) {
        try {
            p.listener->discoveringChanged(*this, currentMetaScanType, eventScanType, eventEnabled, keep_le_scan_alive, event.getTimestamp());
        } catch (std::exception &except) {
            ERR_PRINT("BTAdapter:%s:DeviceDiscovering-CBs %d/%zd: %s of %s: Caught exception %s",
                    srctkn.c_str(), i+1, statusListenerList.size(),
                    p.listener->toString().c_str(), toString().c_str(), except.what());
        }
        i++;
    });

    if( !hasScanType(currentNativeScanType, ScanType::LE) && keep_le_scan_alive ) {
        std::thread bg(&BTAdapter::startDiscoveryBackground, this); // @suppress("Invalid arguments")
        bg.detach();
    }
    return true;
}

bool BTAdapter::mgmtEvNewSettingsMgmt(const MgmtEvent& e) noexcept {
    COND_PRINT(debug_event, "BTAdapter:mgmt:NewSettings: %s", e.toString().c_str());
    const MgmtEvtNewSettings &event = *static_cast<const MgmtEvtNewSettings *>(&e);
    const AdapterSetting new_settings = adapterInfo.setCurrentSettingMask(event.getSettings()); // probably done by mgmt callback already
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

    COND_PRINT(debug_event, "BTAdapter::mgmt:NewSettings: %s -> %s, changes %s: %s",
            to_string(old_settings_).c_str(),
            to_string(new_settings).c_str(),
            to_string(changes).c_str(), toString().c_str() );

    if( justPoweredOn ) {
        // Adapter has been powered on, ensure all hci states are reset.
        hci.clearAllStates();
    }
    sendAdapterSettingsChanged(old_settings_, new_settings, changes, event.getTimestamp());

    if( justPoweredOff ) {
        // Adapter has been powered off, close connections and cleanup off-thread.
        std::thread bg(&BTAdapter::poweredOff, this); // @suppress("Invalid arguments")
        bg.detach();
    }

    return true;
}

bool BTAdapter::mgmtEvLocalNameChangedMgmt(const MgmtEvent& e) noexcept {
    COND_PRINT(debug_event, "BTAdapter:mgmt:LocalNameChanged: %s", e.toString().c_str());
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
    COND_PRINT(debug_event, "BTAdapter:mgmt:LocalNameChanged: Local name: %d: '%s' -> '%s'; short_name: %d: '%s' -> '%s'",
            nameChanged, old_name.c_str(), localName.getName().c_str(),
            shortNameChanged, old_shortName.c_str(), localName.getShortName().c_str());
    (void)nameChanged;
    (void)shortNameChanged;
    return true;
}

bool BTAdapter::mgmtEvDeviceConnectedHCI(const MgmtEvent& e) noexcept {
    COND_PRINT(debug_event, "BTAdapter:hci:DeviceConnected(dev_id %d): %s", dev_id, e.toString().c_str());
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
    std::shared_ptr<BTDevice> device = findConnectedDevice(event.getAddress(), event.getAddressType());
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
        device = BTDevice::make_shared(*this, ad_report);
        addDiscoveredDevice(device);
        addSharedDevice(device);
        new_connect = 3;
    }

    const SMPIOCapability io_cap_conn = mgmt.getIOCapability(dev_id);

    EIRDataType updateMask = device->update(ad_report);
    if( 0 == new_connect ) {
        WARN_PRINT("BTAdapter::EventHCI:DeviceConnected(dev_id %d, already connected, updated %s): %s, handle %s -> %s,\n    %s,\n    -> %s",
            dev_id, to_string(updateMask).c_str(), event.toString().c_str(),
            jau::to_hexstring(device->getConnectionHandle()).c_str(), jau::to_hexstring(event.getHCIHandle()).c_str(),
            ad_report.toString().c_str(),
            device->toString().c_str());
    } else {
        addConnectedDevice(device); // track device, if not done yet
        if( 2 <= new_connect ) {
            device->ts_last_discovery = ad_report.getTimestamp();
        }
        COND_PRINT(debug_event, "BTAdapter::EventHCI:DeviceConnected(dev_id %d, new_connect %d, updated %s): %s, handle %s -> %s,\n    %s,\n    -> %s",
            dev_id, new_connect, to_string(updateMask).c_str(), event.toString().c_str(),
            jau::to_hexstring(device->getConnectionHandle()).c_str(), jau::to_hexstring(event.getHCIHandle()).c_str(),
            ad_report.toString().c_str(),
            device->toString().c_str());
    }
    device->notifyConnected(device, event.getHCIHandle(), io_cap_conn);

    if( device->isConnSecurityAutoEnabled() ) {
        new_connect = 0; // disable deviceConnected() events
    }
    int i=0;
    jau::for_each_fidelity(statusListenerList, [&](impl::StatusListenerPair &p) {
        try {
            if( p.listener->matchDevice(*device) ) {
                if( EIRDataType::NONE != updateMask ) {
                    p.listener->deviceUpdated(device, updateMask, ad_report.getTimestamp());
                }
                if( 0 < new_connect ) {
                    p.listener->deviceConnected(device, event.getHCIHandle(), event.getTimestamp());
                }
            }
        } catch (std::exception &except) {
            ERR_PRINT("BTAdapter::EventHCI:DeviceConnected-CBs %d/%zd: %s of %s: Caught exception %s",
                    i+1, statusListenerList.size(),
                    p.listener->toString().c_str(), device->toString().c_str(), except.what());
        }
        i++;
    });
    return true;
}

bool BTAdapter::mgmtEvConnectFailedHCI(const MgmtEvent& e) noexcept {
    COND_PRINT(debug_event, "BTAdapter::EventHCI:ConnectFailed: %s", e.toString().c_str());
    const MgmtEvtDeviceConnectFailed &event = *static_cast<const MgmtEvtDeviceConnectFailed *>(&e);

    std::shared_ptr<BTDevice> device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr != device ) {
        const uint16_t handle = device->getConnectionHandle();
        COND_PRINT(debug_event, "BTAdapter::EventHCI:ConnectFailed(dev_id %d): %s, handle %s -> zero,\n    -> %s",
            dev_id, event.toString().c_str(), jau::to_hexstring(handle).c_str(),
            device->toString().c_str());

        unlockConnect(*device);
        device->notifyDisconnected();
        removeConnectedDevice(*device);

        if( !device->isConnSecurityAutoEnabled() ) {
            int i=0;
            jau::for_each_fidelity(statusListenerList, [&](impl::StatusListenerPair &p) {
                try {
                    if( p.listener->matchDevice(*device) ) {
                        p.listener->deviceDisconnected(device, event.getHCIStatus(), handle, event.getTimestamp());
                    }
                } catch (std::exception &except) {
                    ERR_PRINT("BTAdapter::EventHCI:DeviceDisconnected-CBs %d/%zd: %s of %s: Caught exception %s",
                            i+1, statusListenerList.size(),
                            p.listener->toString().c_str(), device->toString().c_str(), except.what());
                }
                i++;
            });
            removeDiscoveredDevice(device->addressAndType); // ensure device will cause a deviceFound event after disconnect
        }
    } else {
        WORDY_PRINT("BTAdapter::EventHCI:DeviceDisconnected(dev_id %d): Device not tracked: %s",
            dev_id, event.toString().c_str());
    }
    return true;
}

bool BTAdapter::mgmtEvHCIEncryptionChangedHCI(const MgmtEvent& e) noexcept {
    const MgmtEvtHCIEncryptionChanged &event = *static_cast<const MgmtEvtHCIEncryptionChanged *>(&e);

    std::shared_ptr<BTDevice> device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr != device ) {
        // BT Core Spec v5.2: Vol 4, Part E HCI: 7.7.8 HCIEventType::ENCRYPT_CHANGE
        const HCIStatusCode evtStatus = event.getHCIStatus();
        const bool ok = HCIStatusCode::SUCCESS == evtStatus && 0 != event.getEncEnabled();
        const SMPPairingState pstate = ok ? SMPPairingState::COMPLETED : SMPPairingState::FAILED;
        device->updatePairingState(device, e, evtStatus, pstate);
    } else {
        WORDY_PRINT("BTAdapter::EventHCI:EncryptionChanged(dev_id %d): Device not tracked: %s",
            dev_id, event.toString().c_str());
    }
    return true;
}
bool BTAdapter::mgmtEvHCIEncryptionKeyRefreshCompleteHCI(const MgmtEvent& e) noexcept {
    const MgmtEvtHCIEncryptionKeyRefreshComplete &event = *static_cast<const MgmtEvtHCIEncryptionKeyRefreshComplete *>(&e);

    std::shared_ptr<BTDevice> device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr != device ) {
        // BT Core Spec v5.2: Vol 4, Part E HCI: 7.7.39 HCIEventType::ENCRYPT_KEY_REFRESH_COMPLETE
        const HCIStatusCode evtStatus = event.getHCIStatus();
        // const bool ok = HCIStatusCode::SUCCESS == evtStatus;
        // const SMPPairingState pstate = ok ? SMPPairingState::COMPLETED : SMPPairingState::FAILED;
        const SMPPairingState pstate = SMPPairingState::NONE;
        device->updatePairingState(device, e, evtStatus, pstate);
    } else {
        WORDY_PRINT("BTAdapter::EventHCI:EncryptionKeyRefreshComplete(dev_id %d): Device not tracked: %s",
            dev_id, event.toString().c_str());
    }
    return true;
}

bool BTAdapter::mgmtEvHCILERemoteUserFeaturesHCI(const MgmtEvent& e) noexcept {
    const MgmtEvtHCILERemoteUserFeatures &event = *static_cast<const MgmtEvtHCILERemoteUserFeatures *>(&e);

    std::shared_ptr<BTDevice> device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr != device ) {
        COND_PRINT(debug_event, "BTAdapter::EventHCI:LERemoteUserFeatures(dev_id %d): %s, %s",
            dev_id, event.toString().c_str(), device->toString().c_str());

        device->notifyLEFeatures(device, event.getFeatures());

    } else {
        WORDY_PRINT("BTAdapter::EventHCI:LERemoteUserFeatures(dev_id %d): Device not tracked: %s",
            dev_id, event.toString().c_str());
    }
    return true;
}

bool BTAdapter::mgmtEvDeviceDisconnectedHCI(const MgmtEvent& e) noexcept {
    const MgmtEvtDeviceDisconnected &event = *static_cast<const MgmtEvtDeviceDisconnected *>(&e);

    std::shared_ptr<BTDevice> device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr != device ) {
        if( device->getConnectionHandle() != event.getHCIHandle() ) {
            WORDY_PRINT("BTAdapter::EventHCI:DeviceDisconnected(dev_id %d): ConnHandle mismatch %s\n    -> %s",
                dev_id, event.toString().c_str(), device->toString().c_str());
            return true;
        }
        COND_PRINT(debug_event, "BTAdapter::EventHCI:DeviceDisconnected(dev_id %d): %s, handle %s -> zero,\n    -> %s",
            dev_id, event.toString().c_str(), jau::to_hexstring(event.getHCIHandle()).c_str(),
            device->toString().c_str());

        unlockConnect(*device);
        device->notifyDisconnected();
        removeConnectedDevice(*device);

        if( !device->isConnSecurityAutoEnabled() ) {
            int i=0;
            jau::for_each_fidelity(statusListenerList, [&](impl::StatusListenerPair &p) {
                try {
                    if( p.listener->matchDevice(*device) ) {
                        p.listener->deviceDisconnected(device, event.getHCIReason(), event.getHCIHandle(), event.getTimestamp());
                    }
                } catch (std::exception &except) {
                    ERR_PRINT("BTAdapter::EventHCI:DeviceDisconnected-CBs %d/%zd: %s of %s: Caught exception %s",
                            i+1, statusListenerList.size(),
                            p.listener->toString().c_str(), device->toString().c_str(), except.what());
                }
                i++;
            });
            removeDiscoveredDevice(device->addressAndType); // ensure device will cause a deviceFound event after disconnect
        }
    } else {
        WORDY_PRINT("BTAdapter::EventHCI:DeviceDisconnected(dev_id %d): Device not tracked: %s",
            dev_id, event.toString().c_str());
    }
    return true;
}

bool BTAdapter::mgmtEvDeviceDisconnectedMgmt(const MgmtEvent& e) noexcept {
    COND_PRINT(debug_event, "BTAdapter:mgmt:DeviceDisconnected: %s", e.toString().c_str());
    const MgmtEvtDeviceDisconnected &event = *static_cast<const MgmtEvtDeviceDisconnected *>(&e);
    (void)event;
    return true;
}

bool BTAdapter::mgmtEvPairDeviceCompleteMgmt(const MgmtEvent& e) noexcept {
    const MgmtEvtPairDeviceComplete &event = *static_cast<const MgmtEvtPairDeviceComplete *>(&e);

    std::shared_ptr<BTDevice> device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr != device ) {
        const HCIStatusCode evtStatus = to_HCIStatusCode( event.getStatus() );
        const bool ok = HCIStatusCode::ALREADY_PAIRED == evtStatus;
        const SMPPairingState pstate = ok ? SMPPairingState::COMPLETED : SMPPairingState::NONE;
        device->updatePairingState(device, e, evtStatus, pstate);
    } else {
        WORDY_PRINT("BTAdapter::mgmt:PairDeviceComplete(dev_id %d): Device not tracked: %s",
            dev_id, event.toString().c_str());
    }
    return true;
}

bool BTAdapter::mgmtEvNewLongTermKeyMgmt(const MgmtEvent& e) noexcept {
    const MgmtEvtNewLongTermKey& event = *static_cast<const MgmtEvtNewLongTermKey *>(&e);
    const MgmtLongTermKeyInfo& ltk_info = event.getLongTermKey();
    std::shared_ptr<BTDevice> device = findConnectedDevice(ltk_info.address, ltk_info.address_type);
    if( nullptr != device ) {
        const bool ok = ltk_info.enc_size > 0 && ltk_info.key_type != MgmtLTKType::NONE;
        if( ok ) {
            device->updatePairingState(device, e, HCIStatusCode::SUCCESS, SMPPairingState::COMPLETED);
        } else {
            WORDY_PRINT("BTAdapter::mgmt:NewLongTermKey(dev_id %d): Invalid LTK: %s",
                dev_id, event.toString().c_str());
        }
    } else {
        WORDY_PRINT("BTAdapter::mgmt:NewLongTermKey(dev_id %d): Device not tracked: %s",
            dev_id, event.toString().c_str());
    }
    return true;
}

bool BTAdapter::mgmtEvDeviceFoundHCI(const MgmtEvent& e) noexcept {
    COND_PRINT(debug_event, "BTAdapter:hci:DeviceFound(dev_id %d): %s", dev_id, e.toString().c_str());
    const MgmtEvtDeviceFound &deviceFoundEvent = *static_cast<const MgmtEvtDeviceFound *>(&e);

    const EInfoReport* eir = deviceFoundEvent.getEIR();
    if( nullptr == eir ) {
        // Sourced from Linux Mgmt, which we don't support
        ABORT("BTAdapter:hci:DeviceFound: Not sourced from LE_ADVERTISING_REPORT: %s", deviceFoundEvent.toString().c_str());
    } // else: Sourced from HCIHandler via LE_ADVERTISING_REPORT (default!)

    std::shared_ptr<BTDevice> dev = findDiscoveredDevice(eir->getAddress(), eir->getAddressType());
    if( nullptr != dev ) {
        //
        // drop existing device
        //
        EIRDataType updateMask = dev->update(*eir);
        COND_PRINT(debug_event, "BTAdapter:hci:DeviceFound: Drop already discovered %s, %s",
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
        COND_PRINT(debug_event, "BTAdapter:hci:DeviceFound: Use already shared %s, %s",
                dev->getAddressAndType().toString().c_str(), eir->toString().c_str());

        int i=0;
        bool device_used = false;
        jau::for_each_fidelity(statusListenerList, [&](impl::StatusListenerPair &p) {
            try {
                if( p.listener->matchDevice(*dev) ) {
                    device_used = p.listener->deviceFound(dev, eir->getTimestamp()) || device_used;
                }
            } catch (std::exception &except) {
                ERR_PRINT("BTAdapter:hci:DeviceFound: %d/%zd: %s of %s: Caught exception %s",
                        i+1, statusListenerList.size(),
                        p.listener->toString().c_str(), dev->toString().c_str(), except.what());
            }
            i++;
        });
        if( !device_used ) {
            // keep to avoid duplicate finds: removeDiscoveredDevice(dev->addressAndType);
            // and still allowing usage, as connecting will re-add to shared list
            removeSharedDevice(*dev); // pending dtor until discovered is flushed
        } else if( EIRDataType::NONE != updateMask ) {
            sendDeviceUpdated("SharedDeviceFound", dev, eir->getTimestamp(), updateMask);
        }
        return true;
    }

    //
    // new device
    //
    dev = BTDevice::make_shared(*this, *eir);
    addDiscoveredDevice(dev);
    addSharedDevice(dev);
    COND_PRINT(debug_event, "BTAdapter:hci:DeviceFound: Use new %s, %s",
            dev->getAddressAndType().toString().c_str(), eir->toString().c_str());

    int i=0;
    bool device_used = false;
    jau::for_each_fidelity(statusListenerList, [&](impl::StatusListenerPair &p) {
        try {
            if( p.listener->matchDevice(*dev) ) {
                device_used = p.listener->deviceFound(dev, eir->getTimestamp()) || device_used;
            }
        } catch (std::exception &except) {
            ERR_PRINT("BTAdapter:hci:DeviceFound-CBs %d/%zd: %s of %s: Caught exception %s",
                    i+1, statusListenerList.size(),
                    p.listener->toString().c_str(), dev->toString().c_str(), except.what());
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

bool BTAdapter::mgmtEvDeviceUnpairedMgmt(const MgmtEvent& e) noexcept {
    const MgmtEvtDeviceUnpaired &event = *static_cast<const MgmtEvtDeviceUnpaired *>(&e);
    DBG_PRINT("BTAdapter:mgmt:DeviceUnpaired: %s", event.toString().c_str());
    return true;
}
bool BTAdapter::mgmtEvPinCodeRequestMgmt(const MgmtEvent& e) noexcept {
    const MgmtEvtPinCodeRequest &event = *static_cast<const MgmtEvtPinCodeRequest *>(&e);
    DBG_PRINT("BTAdapter:mgmt:PinCodeRequest: %s", event.toString().c_str());
    return true;
}
bool BTAdapter::mgmtEvAuthFailedMgmt(const MgmtEvent& e) noexcept {
    const MgmtEvtAuthFailed &event = *static_cast<const MgmtEvtAuthFailed *>(&e);

    std::shared_ptr<BTDevice> device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr == device ) {
        WORDY_PRINT("BTAdapter:hci:SMP: dev_id %d: Device not tracked: address[%s, %s], %s",
                dev_id, event.getAddress().toString().c_str(), to_string(event.getAddressType()).c_str(),
                event.toString().c_str());
        return true;
    }
    const HCIStatusCode evtStatus = to_HCIStatusCode( event.getStatus() );
    device->updatePairingState(device, e, evtStatus, SMPPairingState::FAILED);
    return true;
}
bool BTAdapter::mgmtEvUserConfirmRequestMgmt(const MgmtEvent& e) noexcept {
    const MgmtEvtUserConfirmRequest &event = *static_cast<const MgmtEvtUserConfirmRequest *>(&e);

    std::shared_ptr<BTDevice> device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr == device ) {
        WORDY_PRINT("BTAdapter:hci:SMP: dev_id %d: Device not tracked: address[%s, %s], %s",
                dev_id, event.getAddress().toString().c_str(), to_string(event.getAddressType()).c_str(),
                event.toString().c_str());
        return true;
    }
    // FIXME: Pass confirm_hint and value?
    device->updatePairingState(device, e, HCIStatusCode::SUCCESS, SMPPairingState::NUMERIC_COMPARE_EXPECTED);
    return true;
}
bool BTAdapter::mgmtEvUserPasskeyRequestMgmt(const MgmtEvent& e) noexcept {
    const MgmtEvtUserPasskeyRequest &event = *static_cast<const MgmtEvtUserPasskeyRequest *>(&e);

    std::shared_ptr<BTDevice> device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr == device ) {
        WORDY_PRINT("BTAdapter:hci:SMP: dev_id %d: Device not tracked: address[%s, %s], %s",
                dev_id, event.getAddress().toString().c_str(), to_string(event.getAddressType()).c_str(),
                event.toString().c_str());
        return true;
    }
    device->updatePairingState(device, e, HCIStatusCode::SUCCESS, SMPPairingState::PASSKEY_EXPECTED);
    return true;
}

bool BTAdapter::hciSMPMsgCallback(const BDAddressAndType & addressAndType,
                                   const SMPPDUMsg& msg, const HCIACLData::l2cap_frame& source) noexcept {
    std::shared_ptr<BTDevice> device = findConnectedDevice(addressAndType.address, addressAndType.type);
    if( nullptr == device ) {
        WORDY_PRINT("BTAdapter:hci:SMP: dev_id %d: Device not tracked: address%s: %s, %s",
                dev_id, addressAndType.toString().c_str(),
                msg.toString().c_str(), source.toString().c_str());
        return true;
    }
    if( device->getConnectionHandle() != source.handle ) {
        WORDY_PRINT("BTAdapter:hci:SMP: dev_id %d: ConnHandle mismatch address%s: %s, %s\n    -> %s",
                dev_id, addressAndType.toString().c_str(),
                msg.toString().c_str(), source.toString().c_str(), device->toString().c_str());
        return true;
    }

    device->hciSMPMsgCallback(device, msg, source);

    return true;
}

void BTAdapter::sendDevicePairingState(std::shared_ptr<BTDevice> device, const SMPPairingState state, const PairingMode mode, uint64_t timestamp) noexcept
{
    int i=0;
    jau::for_each_fidelity(statusListenerList, [&](impl::StatusListenerPair &p) {
        try {
            if( p.listener->matchDevice(*device) ) {
                p.listener->devicePairingState(device, state, mode, timestamp);
            }
        } catch (std::exception &except) {
            ERR_PRINT("BTAdapter::sendDevicePairingState: %d/%zd: %s of %s: Caught exception %s",
                    i+1, statusListenerList.size(),
                    p.listener->toString().c_str(), device->toString().c_str(), except.what());
        }
        i++;
    });
}

void BTAdapter::sendDeviceReady(std::shared_ptr<BTDevice> device, uint64_t timestamp) noexcept {
    int i=0;
    jau::for_each_fidelity(statusListenerList, [&](impl::StatusListenerPair &p) {
        try {
            // Only issue if valid && received connected confirmation (HCI) && not have called disconnect yet.
            if( device->isValid() && device->getConnected() && device->allowDisconnect ) {
                if( p.listener->matchDevice(*device) ) {
                    p.listener->deviceReady(device, timestamp);
                }
            }
        } catch (std::exception &except) {
            ERR_PRINT("BTAdapter::sendDeviceReady: %d/%zd: %s of %s: Caught exception %s",
                    i+1, statusListenerList.size(),
                    p.listener->toString().c_str(), device->toString().c_str(), except.what());
        }
        i++;
    });
}
