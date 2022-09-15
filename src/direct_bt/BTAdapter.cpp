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
#include "DBTConst.hpp"

extern "C" {
    #include <inttypes.h>
    #include <unistd.h>
    #include <poll.h>
}

using namespace direct_bt;
using namespace jau::fractions_i64_literals;

constexpr static const bool _print_device_lists = false;

std::string direct_bt::to_string(const DiscoveryPolicy v) noexcept {
    switch(v) {
        case DiscoveryPolicy::AUTO_OFF: return "AUTO_OFF";
        case DiscoveryPolicy::PAUSE_CONNECTED_UNTIL_DISCONNECTED: return "PAUSE_CONNECTED_UNTIL_DISCONNECTED";
        case DiscoveryPolicy::PAUSE_CONNECTED_UNTIL_READY: return "PAUSE_CONNECTED_UNTIL_READY";
        case DiscoveryPolicy::PAUSE_CONNECTED_UNTIL_PAIRED: return "PAUSE_CONNECTED_UNTIL_PAIRED";
        case DiscoveryPolicy::ALWAYS_ON: return "ALWAYS_ON";
    }
    return "Unknown DiscoveryPolicy "+jau::to_hexstring(number(v));
}

BTDeviceRef BTAdapter::findDevice(device_list_t & devices, const EUI48 & address, const BDAddressType addressType) noexcept {
    const jau::nsize_t size = devices.size();
    for (jau::nsize_t i = 0; i < size; ++i) {
        BTDeviceRef & e = devices[i];
        if ( nullptr != e && address == e->getAddressAndType().address &&
             ( addressType == e->getAddressAndType().type || addressType == BDAddressType::BDADDR_UNDEFINED )
           )
        {
            return e;
        }
    }
    return nullptr;
}

BTDeviceRef BTAdapter::findDevice(device_list_t & devices, BTDevice const & device) noexcept {
    const jau::nsize_t size = devices.size();
    for (jau::nsize_t i = 0; i < size; ++i) {
        BTDeviceRef & e = devices[i];
        if ( nullptr != e && device == *e ) {
            return e;
        }
    }
    return nullptr;
}

BTDeviceRef BTAdapter::findWeakDevice(weak_device_list_t & devices, const EUI48 & address, const BDAddressType addressType) noexcept {
    auto end = devices.end();
    for (auto it = devices.begin(); it != end; ) {
        std::weak_ptr<BTDevice> & w = *it;
        BTDeviceRef e = w.lock();
        if( nullptr == e ) {
            devices.erase(it); // erase and move it to next element
        } else if ( address == e->getAddressAndType().address &&
                    ( addressType == e->getAddressAndType().type || addressType == BDAddressType::BDADDR_UNDEFINED )
                  )
        {
            return e;
        } else {
            ++it; // move it to next element
        }
    }
    return nullptr;
}

BTDeviceRef BTAdapter::findWeakDevice(weak_device_list_t & devices, BTDevice const & device) noexcept {
    auto end = devices.end();
    for (auto it = devices.begin(); it != end; ) {
        std::weak_ptr<BTDevice> & w = *it;
        BTDeviceRef e = w.lock();
        if( nullptr == e ) {
            devices.erase(it); // erase and move it to next element
        } else if ( device == *e ) {
            return e;
        } else {
            ++it; // move it to next element
        }
    }
    return nullptr;
}

BTDeviceRef BTAdapter::findDevicePausingDiscovery (const EUI48 & address, const BDAddressType & addressType) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_pausingDiscoveryDevices); // RAII-style acquire and relinquish via destructor
    return findWeakDevice(pausing_discovery_devices, address, addressType);
}

bool BTAdapter::addDevicePausingDiscovery(const BTDeviceRef & device) noexcept {
    bool added_first = false;
    {
        const std::lock_guard<std::mutex> lock(mtx_pausingDiscoveryDevices); // RAII-style acquire and relinquish via destructor
        if( nullptr != findWeakDevice(pausing_discovery_devices, *device) ) {
            return false;
        }
        added_first = 0 == pausing_discovery_devices.size();
        pausing_discovery_devices.push_back(device);
    }
    if( added_first ) {
        if constexpr ( SCAN_DISABLED_POST_CONNECT ) {
            updateDeviceDiscoveringState(ScanType::LE, false /* eventEnabled */);
        } else {
            std::thread bg(&BTAdapter::stopDiscoveryImpl, this, false /* forceDiscoveringEvent */, true /* temporary */); // @suppress("Invalid arguments")
            bg.detach();
        }
        return true;
    } else {
        return false;
    }
}

bool BTAdapter::removeDevicePausingDiscovery(const BTDevice & device) noexcept {
    bool removed_last = false;
    {
        const std::lock_guard<std::mutex> lock(mtx_pausingDiscoveryDevices); // RAII-style acquire and relinquish via destructor
        auto end = pausing_discovery_devices.end();
        for (auto it = pausing_discovery_devices.begin(); it != end; ) {
            std::weak_ptr<BTDevice> & w = *it;
            BTDeviceRef e = w.lock();
            if( nullptr == e ) {
                pausing_discovery_devices.erase(it); // erase and move it to next element
            } else if ( device == *e ) {
                pausing_discovery_devices.erase(it);
                removed_last = 0 == pausing_discovery_devices.size();
                break; // done
            } else {
                ++it; // move it to next element
            }
        }
    }
    if( removed_last ) {
        discovery_service.start();
        return true;
    } else {
        return false;
    }
}

void BTAdapter::clearDevicesPausingDiscovery() noexcept {
    const std::lock_guard<std::mutex> lock(mtx_pausingDiscoveryDevices); // RAII-style acquire and relinquish via destructor
    pausing_discovery_devices.clear();
}

jau::nsize_t BTAdapter::getDevicesPausingDiscoveryCount() noexcept {
    const std::lock_guard<std::mutex> lock(mtx_pausingDiscoveryDevices); // RAII-style acquire and relinquish via destructor
    return pausing_discovery_devices.size();
}

bool BTAdapter::addConnectedDevice(const BTDeviceRef & device) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_connectedDevices); // RAII-style acquire and relinquish via destructor
    if( nullptr != findDevice(connectedDevices, *device) ) {
        return false;
    }
    connectedDevices.push_back(device);
    return true;
}

bool BTAdapter::removeConnectedDevice(const BTDevice & device) noexcept {
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

int BTAdapter::disconnectAllDevices(const HCIStatusCode reason) noexcept {
    device_list_t devices;
    {
        jau::sc_atomic_critical sync(sync_data); // SC-DRF via atomic acquire & release
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

BTDeviceRef BTAdapter::findConnectedDevice (const EUI48 & address, const BDAddressType & addressType) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_connectedDevices); // RAII-style acquire and relinquish via destructor
    return findDevice(connectedDevices, address, addressType);
}

jau::nsize_t BTAdapter::getConnectedDeviceCount() const noexcept {
    jau::sc_atomic_critical sync(sync_data); // SC-DRF via atomic acquire & release
    return connectedDevices.size();
}

// *************************************************
// *************************************************
// *************************************************

bool BTAdapter::updateDataFromHCI() noexcept {
    HCILocalVersion version;
    HCIStatusCode status = hci.getLocalVersion(version);
    if( HCIStatusCode::SUCCESS != status ) {
        ERR_PRINT("Adapter[%d]: POWERED, LocalVersion failed %s - %s",
                dev_id, to_string(status).c_str(), adapterInfo.toString().c_str());
        return false;
    }
    le_features = hci.le_get_local_features();
    hci_uses_ext_scan = hci.use_ext_scan();
    hci_uses_ext_conn = hci.use_ext_conn();
    hci_uses_ext_adv  = hci.use_ext_adv();

    WORDY_PRINT("BTAdapter::updateDataFromHCI: Adapter[%d]: POWERED, %s - %s, hci_ext[scan %d, conn %d], features: %s",
            dev_id, version.toString().c_str(), adapterInfo.toString().c_str(),
            hci_uses_ext_scan, hci_uses_ext_conn,
            direct_bt::to_string(le_features).c_str());
    return true;
}

bool BTAdapter::updateDataFromAdapterInfo() noexcept {
    BTMode btMode = getBTMode();
    if( BTMode::NONE == btMode ) {
        WARN_PRINT("Adapter[%d]: BTMode invalid, BREDR nor LE set: %s", dev_id, adapterInfo.toString().c_str());
        return false;
    }
    hci.setBTMode(btMode);
    return true;
}

bool BTAdapter::initialSetup() noexcept {
    if( !mgmt->isOpen() ) {
        ERR_PRINT("Adapter[%d]: Manager not open", dev_id);
        return false;
    }
    if( !hci.isOpen() ) {
        ERR_PRINT("Adapter[%d]: HCIHandler closed", dev_id);
        return false;
    }

    old_settings = adapterInfo.getCurrentSettingMask();

    if( !updateDataFromAdapterInfo() ) {
        return false;
    }

    if( adapterInfo.isCurrentSettingBitSet(AdapterSetting::POWERED) ) {
        if( !hci.resetAllStates(true) ) {
            return false;
        }
        if( !updateDataFromHCI() ) {
            return false;
        }
    } else {
        hci.resetAllStates(false);
        WORDY_PRINT("BTAdapter::initialSetup: Adapter[%d]: Not POWERED: %s", dev_id, adapterInfo.toString().c_str());
    }
    WORDY_PRINT("BTAdapter::initialSetup: Adapter[%d]: Done: %s - %s", dev_id, adapterInfo.toString().c_str(), toString().c_str());

    return true;
}

bool BTAdapter::enableListening(const bool enable) noexcept {
    if( enable ) {
        if( !mgmt->isOpen() ) {
            ERR_PRINT("Adapter[%d]: Manager not open", dev_id);
            return false;
        }
        if( !hci.isOpen() ) {
            ERR_PRINT("Adapter[%d]: HCIHandler closed", dev_id);
            return false;
        }

        // just be sure ..
        mgmt->removeMgmtEventCallback(dev_id);
        hci.clearAllCallbacks();

        bool ok = true;
        ok = mgmt->addMgmtEventCallback(dev_id, MgmtEvent::Opcode::DISCOVERING, jau::bindMemberFunc(this, &BTAdapter::mgmtEvDeviceDiscoveringMgmt)) && ok;
        ok = mgmt->addMgmtEventCallback(dev_id, MgmtEvent::Opcode::NEW_SETTINGS, jau::bindMemberFunc(this, &BTAdapter::mgmtEvNewSettingsMgmt)) && ok;
        ok = mgmt->addMgmtEventCallback(dev_id, MgmtEvent::Opcode::LOCAL_NAME_CHANGED, jau::bindMemberFunc(this, &BTAdapter::mgmtEvLocalNameChangedMgmt)) && ok;
        ok = mgmt->addMgmtEventCallback(dev_id, MgmtEvent::Opcode::PIN_CODE_REQUEST, jau::bindMemberFunc(this, &BTAdapter::mgmtEvPinCodeRequestMgmt));
        ok = mgmt->addMgmtEventCallback(dev_id, MgmtEvent::Opcode::USER_CONFIRM_REQUEST, jau::bindMemberFunc(this, &BTAdapter::mgmtEvUserConfirmRequestMgmt));
        ok = mgmt->addMgmtEventCallback(dev_id, MgmtEvent::Opcode::USER_PASSKEY_REQUEST, jau::bindMemberFunc(this, &BTAdapter::mgmtEvUserPasskeyRequestMgmt));
        ok = mgmt->addMgmtEventCallback(dev_id, MgmtEvent::Opcode::AUTH_FAILED, jau::bindMemberFunc(this, &BTAdapter::mgmtEvAuthFailedMgmt));
        ok = mgmt->addMgmtEventCallback(dev_id, MgmtEvent::Opcode::DEVICE_UNPAIRED, jau::bindMemberFunc(this, &BTAdapter::mgmtEvDeviceUnpairedMgmt));
        ok = mgmt->addMgmtEventCallback(dev_id, MgmtEvent::Opcode::PAIR_DEVICE_COMPLETE, jau::bindMemberFunc(this, &BTAdapter::mgmtEvPairDeviceCompleteMgmt));
        ok = mgmt->addMgmtEventCallback(dev_id, MgmtEvent::Opcode::NEW_LONG_TERM_KEY, jau::bindMemberFunc(this, &BTAdapter::mgmtEvNewLongTermKeyMgmt));
        ok = mgmt->addMgmtEventCallback(dev_id, MgmtEvent::Opcode::NEW_LINK_KEY, jau::bindMemberFunc(this, &BTAdapter::mgmtEvNewLinkKeyMgmt));

        if( !ok ) {
            ERR_PRINT("Could not add all required MgmtEventCallbacks to DBTManager: %s", toString().c_str());
            return false;
        }

    #if 0
        mgmt->addMgmtEventCallback(dev_id, MgmtEvent::Opcode::DEVICE_DISCONNECTED, bindMemberFunc(this, &BTAdapter::mgmtEvDeviceDisconnectedMgmt));
    #endif

        ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::DISCOVERING, jau::bindMemberFunc(this, &BTAdapter::mgmtEvDeviceDiscoveringHCI)) && ok;
        ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::DEVICE_CONNECTED, jau::bindMemberFunc(this, &BTAdapter::mgmtEvDeviceConnectedHCI)) && ok;
        ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::CONNECT_FAILED, jau::bindMemberFunc(this, &BTAdapter::mgmtEvConnectFailedHCI)) && ok;
        ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::DEVICE_DISCONNECTED, jau::bindMemberFunc(this, &BTAdapter::mgmtEvDeviceDisconnectedHCI)) && ok;
        ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::DEVICE_FOUND, jau::bindMemberFunc(this, &BTAdapter::mgmtEvDeviceFoundHCI)) && ok;
        ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::HCI_LE_REMOTE_FEATURES, jau::bindMemberFunc(this, &BTAdapter::mgmtEvHCILERemoteUserFeaturesHCI)) && ok;
        ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::HCI_LE_PHY_UPDATE_COMPLETE, jau::bindMemberFunc(this, &BTAdapter::mgmtEvHCILEPhyUpdateCompleteHCI)) && ok;

        ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::HCI_ENC_CHANGED, jau::bindMemberFunc(this, &BTAdapter::mgmtEvHCIEncryptionChangedHCI)) && ok;
        ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::HCI_ENC_KEY_REFRESH_COMPLETE, jau::bindMemberFunc(this, &BTAdapter::mgmtEvHCIEncryptionKeyRefreshCompleteHCI)) && ok;
        if constexpr ( CONSIDER_HCI_CMD_FOR_SMP_STATE ) {
            ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::HCI_LE_LTK_REQUEST, jau::bindMemberFunc(this, &BTAdapter::mgmtEvLELTKReqEventHCI)) && ok;
            ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::HCI_LE_LTK_REPLY_ACK, jau::bindMemberFunc(this, &BTAdapter::mgmtEvLELTKReplyAckCmdHCI)) && ok;
            ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::HCI_LE_LTK_REPLY_REJ, jau::bindMemberFunc(this, &BTAdapter::mgmtEvLELTKReplyRejCmdHCI)) && ok;
            ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::HCI_LE_ENABLE_ENC, jau::bindMemberFunc(this, &BTAdapter::mgmtEvLEEnableEncryptionCmdHCI)) && ok;
        }
        if( !ok ) {
            ERR_PRINT("Could not add all required MgmtEventCallbacks to HCIHandler: %s of %s", hci.toString().c_str(), toString().c_str());
            return false; // dtor local HCIHandler w/ closing
        }
        hci.addSMPMsgCallback(jau::bindMemberFunc(this, &BTAdapter::hciSMPMsgCallback));
    } else {
        mgmt->removeMgmtEventCallback(dev_id);
        hci.clearAllCallbacks();
    }
    WORDY_PRINT("BTAdapter::enableListening: Adapter[%d]: Done: %s - %s", dev_id, adapterInfo.toString().c_str(), toString().c_str());

    return true;
}

BTAdapter::BTAdapter(const BTAdapter::ctor_cookie& cc, const BTManagerRef& mgmt_, const AdapterInfo& adapterInfo_) noexcept
: debug_event(jau::environment::getBooleanProperty("direct_bt.debug.adapter.event", false)),
  debug_lock(jau::environment::getBooleanProperty("direct_bt.debug.adapter.lock", false)),
  mgmt( mgmt_ ),
  adapterInfo( adapterInfo_ ),
  adapter_initialized( false ), adapter_poweredon_at_init( false ),
  le_features( LE_Features::NONE ),
  hci_uses_ext_scan( false ), hci_uses_ext_conn( false ), hci_uses_ext_adv( false ),
  visibleAddressAndType( adapterInfo_.addressAndType ),
  dev_id( adapterInfo.dev_id ),
  btRole ( BTRole::Master ),
  hci( dev_id ),
  currentMetaScanType( ScanType::NONE ),
  discovery_policy ( DiscoveryPolicy::AUTO_OFF ),
  scan_filter_dup( true ),
  smp_watchdog("adapter"+std::to_string(dev_id)+"_smp_watchdog", THREAD_SHUTDOWN_TIMEOUT_MS),
  l2cap_att_srv(dev_id, adapterInfo_.addressAndType, L2CAP_PSM::UNDEFINED, L2CAP_CID::ATT),
  l2cap_service("BTAdapter::l2capServer", THREAD_SHUTDOWN_TIMEOUT_MS,
                jau::bindMemberFunc(this, &BTAdapter::l2capServerWork),
                jau::bindMemberFunc(this, &BTAdapter::l2capServerInit),
                jau::bindMemberFunc(this, &BTAdapter::l2capServerEnd)),
  discovery_service("BTAdapter::discoveryServer", 400_ms,
                jau::bindMemberFunc(this, &BTAdapter::discoveryServerWork))

{
    (void)cc;

    adapter_operational = initialSetup();
    if( isValid() ) {
        const bool r = smp_watchdog.start(SMP_NEXT_EVENT_TIMEOUT_MS, jau::bindMemberFunc(this, &BTAdapter::smp_timeoutfunc));
        DBG_PRINT("BTAdapter::ctor: dev_id %d: smp_watchdog.smp_timeoutfunc started %d", dev_id, r);
    }
}

BTAdapter::~BTAdapter() noexcept {
    if( !isValid() ) {
        DBG_PRINT("BTAdapter::dtor: dev_id %d, invalid, %p", dev_id, this);
        smp_watchdog.stop();
        mgmt->removeAdapter(this); // remove this instance from manager
        hci.clearAllCallbacks();
        return;
    }
    DBG_PRINT("BTAdapter::dtor: ... %p %s", this, toString().c_str());
    close();

    mgmt->removeAdapter(this); // remove this instance from manager

    DBG_PRINT("BTAdapter::dtor: XXX");
}

void BTAdapter::close() noexcept {
    smp_watchdog.stop();
    if( !isValid() ) {
        // Native user app could have destroyed this instance already from
        DBG_PRINT("BTAdapter::close: dev_id %d, invalid, %p", dev_id, this);
        return;
    }
    DBG_PRINT("BTAdapter::close: ... %p %s", this, toString().c_str());
    discovery_policy = DiscoveryPolicy::AUTO_OFF;

    // mute all listener first
    {
        int count = mgmt->removeMgmtEventCallback(dev_id);
        DBG_PRINT("BTAdapter::close removeMgmtEventCallback: %d callbacks", count);
    }
    hci.clearAllCallbacks();
    statusListenerList.clear();

    poweredOff(true /* active */, "close");

    if( adapter_poweredon_at_init ) {
        adapter_poweredon_at_init = false;
        if( isPowered() ) {
            setPowered(false);
        }
    }

    DBG_PRINT("BTAdapter::close: close[HCI, l2cap_srv]: ...");
    hci.close();
    l2cap_service.stop();
    l2cap_att_srv.close();
    discovery_service.stop();
    DBG_PRINT("BTAdapter::close: close[HCI, l2cap_srv, discovery_srv]: XXX");

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
    {
        const std::lock_guard<std::mutex> lock(mtx_keys); // RAII-style acquire and relinquish via destructor
        key_list.clear();
        key_path.clear();
    }
    adapter_operational = false;
    DBG_PRINT("BTAdapter::close: XXX");
}

void BTAdapter::poweredOff(bool active, const std::string& msg) noexcept {
    if( !isValid() ) {
        ERR_PRINT("BTAdapter invalid: dev_id %d, %p", dev_id, this);
        return;
    }
    DBG_PRINT("BTAdapter::poweredOff(active %d, %s): ... %p, %s", active, msg.c_str(), this, toString().c_str());
    if( jau::environment::get().debug ) {
        if( !active ) {
            jau::print_backtrace(true /* skip_anon_frames */, 4 /* max_frames */, 2 /* skip_frames: print_b*() + get_b*() */);
        }
    }
    if( !hci.isOpen() ) {
        jau::INFO_PRINT("BTAdapter::poweredOff: HCI closed: active %d -> 0: %s", active, toString().c_str());
        active = false;
    } else if( active && !adapterInfo.isCurrentSettingBitSet(AdapterSetting::POWERED) ) {
        DBG_PRINT("BTAdapter::poweredOff: !POWERED: active %d -> 0: %s", active, toString().c_str());
        active = false;
    }
    discovery_policy = DiscoveryPolicy::PAUSE_CONNECTED_UNTIL_READY;

    if( active ) {
        stopDiscoveryImpl(true /* forceDiscoveringEvent */, false /* temporary */);
    }

    // Removes all device references from the lists: connectedDevices, discoveredDevices, sharedDevices
    disconnectAllDevices();
    removeDiscoveredDevices();

    // ensure all hci states are reset.
    hci.resetAllStates(false);

    currentMetaScanType = ScanType::NONE;
    btRole = BTRole::Master;

    unlockConnectAny();

    DBG_PRINT("BTAdapter::poweredOff(active %d, %s): XXX %s", active, msg.c_str(), toString().c_str());
}

void BTAdapter::printDeviceList(const std::string& prefix, const BTAdapter::device_list_t& list) noexcept {
    const size_t sz = list.size();
    jau::PLAIN_PRINT(true, "- BTAdapter::%s: %zu elements", prefix.c_str(), sz);
    int idx = 0;
    for (auto it = list.begin(); it != list.end(); ++idx, ++it) {
        if( nullptr != (*it) ) {
            jau::PLAIN_PRINT(true, "  - %d / %zu: null", (idx+1), sz);
        } else if( (*it)->isValidInstance() ) {
            jau::PLAIN_PRINT(true, "  - %d / %zu: invalid", (idx+1), sz);
        } else {
            jau::PLAIN_PRINT(true, "  - %d / %zu: %s, name '%s'", (idx+1), sz,
                    (*it)->getAddressAndType().toString().c_str(),
                    (*it)->getName().c_str() );
        }
    }
}
void BTAdapter::printWeakDeviceList(const std::string& prefix, BTAdapter::weak_device_list_t& list) noexcept {
    const size_t sz = list.size();
    jau::PLAIN_PRINT(true, "- BTAdapter::%s: %zu elements", prefix.c_str(), sz);
    int idx = 0;
    for (auto it = list.begin(); it != list.end(); ++idx, ++it) {
        std::weak_ptr<BTDevice> & w = *it;
        BTDeviceRef e = w.lock();
        if( nullptr == e ) {
            jau::PLAIN_PRINT(true, "  - %d / %zu: null", (idx+1), sz);
        } else if( !e->isValidInstance() ) {
            jau::PLAIN_PRINT(true, "  - %d / %zu: invalid", (idx+1), sz);
        } else {
            jau::PLAIN_PRINT(true, "  - %d / %zu: %s, name '%s'", (idx+1), sz,
                    e->getAddressAndType().toString().c_str(),
                    e->getName().c_str() );
        }
    }
}
void BTAdapter::printDeviceLists() noexcept {
    // Using expensive copies here: debug mode only
    weak_device_list_t _sharedDevices, _discoveredDevices, _connectedDevices, _pausingDiscoveryDevice;
    {
        const std::lock_guard<std::mutex> lock(mtx_sharedDevices); // RAII-style acquire and relinquish via destructor
        for(BTDeviceRef d : sharedDevices) { _sharedDevices.push_back(d); }
    }
    {
        const std::lock_guard<std::mutex> lock(mtx_discoveredDevices); // RAII-style acquire and relinquish via destructor
        for(BTDeviceRef d : discoveredDevices) { _discoveredDevices.push_back(d); }
    }
    {
        const std::lock_guard<std::mutex> lock(mtx_connectedDevices); // RAII-style acquire and relinquish via destructor
        for(BTDeviceRef d : connectedDevices) { _connectedDevices.push_back(d); }
    }
    {
        const std::lock_guard<std::mutex> lock(mtx_pausingDiscoveryDevices); // RAII-style acquire and relinquish via destructor
        _pausingDiscoveryDevice = pausing_discovery_devices;
    }
    printWeakDeviceList("SharedDevices     ", _sharedDevices);
    printWeakDeviceList("ConnectedDevices  ", _connectedDevices);
    printWeakDeviceList("DiscoveredDevices ", _discoveredDevices);
    printWeakDeviceList("PausingDiscoveryDevices ", _pausingDiscoveryDevice);
    printStatusListenerList();
}

void BTAdapter::printStatusListenerList() noexcept {
    auto begin = statusListenerList.begin(); // lock mutex and copy_store
    jau::PLAIN_PRINT(true, "- BTAdapter::StatusListener    : %zu elements", (size_t)begin.size());
    for(int ii=0; !begin.is_end(); ++ii, ++begin ) {
        jau::PLAIN_PRINT(true, "  - %d / %zu: %p, %s", (ii+1), (size_t)begin.size(), begin->listener.get(), begin->listener->toString().c_str());
    }
}

HCIStatusCode BTAdapter::setName(const std::string &name, const std::string &short_name) noexcept {
    if( isAdapterSettingBitSet(adapterInfo.getCurrentSettingMask(), AdapterSetting::POWERED) ) {
        return HCIStatusCode::COMMAND_DISALLOWED;
    }
    std::shared_ptr<NameAndShortName> res = mgmt->setLocalName(dev_id, name, short_name);
    return nullptr != res ? HCIStatusCode::SUCCESS : HCIStatusCode::FAILED;
}

bool BTAdapter::setPowered(const bool power_on) noexcept {
    AdapterSetting settings = adapterInfo.getCurrentSettingMask();
    if( power_on == isAdapterSettingBitSet(settings, AdapterSetting::POWERED) ) {
        // unchanged
        return true;
    }
    if( !mgmt->setMode(dev_id, MgmtCommand::Opcode::SET_POWERED, power_on ? 1 : 0, settings) ) {
        return false;
    }
    const AdapterSetting new_settings = adapterInfo.setCurrentSettingMask(settings);
    updateAdapterSettings(false /* off_thread */, new_settings, false /* sendEvent */, 0);
    return power_on == isAdapterSettingBitSet(new_settings, AdapterSetting::POWERED);
}

HCIStatusCode BTAdapter::setSecureConnections(const bool enable) noexcept {
    AdapterSetting settings = adapterInfo.getCurrentSettingMask();
    if( isAdapterSettingBitSet(settings, AdapterSetting::POWERED) ) {
        return HCIStatusCode::COMMAND_DISALLOWED;
    }
    if( enable == isAdapterSettingBitSet(settings, AdapterSetting::SECURE_CONN) ) {
        // unchanged
        return HCIStatusCode::SUCCESS;
    }
    if( !mgmt->setMode(dev_id, MgmtCommand::Opcode::SET_SECURE_CONN, enable ? 1 : 0, settings) ) {
        return HCIStatusCode::FAILED;
    }
    const AdapterSetting new_settings = adapterInfo.setCurrentSettingMask(settings);
    updateAdapterSettings(false /* off_thread */, new_settings, false /* sendEvent */, 0);
    return ( enable == isAdapterSettingBitSet(new_settings, AdapterSetting::SECURE_CONN) ) ? HCIStatusCode::SUCCESS : HCIStatusCode::FAILED;
}

HCIStatusCode BTAdapter::setDefaultConnParam(const uint16_t conn_interval_min, const uint16_t conn_interval_max,
                                             const uint16_t conn_latency, const uint16_t supervision_timeout) noexcept {
    if( isAdapterSettingBitSet(adapterInfo.getCurrentSettingMask(), AdapterSetting::POWERED) ) {
        return HCIStatusCode::COMMAND_DISALLOWED;
    }
    return mgmt->setDefaultConnParam(dev_id, conn_interval_min, conn_interval_max, conn_latency, supervision_timeout);
}

void BTAdapter::setServerConnSecurity(const BTSecurityLevel sec_level, const SMPIOCapability io_cap) noexcept {
    sec_level_server = sec_level;
    io_cap_server = io_cap;
}

void BTAdapter::setSMPKeyPath(const std::string path) noexcept {
    jau::sc_atomic_critical sync(sync_data);
    key_path = path;

    std::vector<SMPKeyBin> keys = SMPKeyBin::readAllForLocalAdapter(getAddressAndType(), key_path, jau::environment::get().debug /* verbose_ */);
    for(SMPKeyBin f : keys) {
        uploadKeys(f, false /* write */);
    }
}

HCIStatusCode BTAdapter::uploadKeys(SMPKeyBin& bin, const bool write) noexcept {
    if( bin.getLocalAddrAndType() != adapterInfo.addressAndType ) {
        if( bin.getVerbose() ) {
            jau::PLAIN_PRINT(true, "BTAdapter::setSMPKeyBin: Adapter address not matching: %s, %s",
                    bin.toString().c_str(), toString().c_str());
        }
        return HCIStatusCode::INVALID_PARAMS;
    }
    EInfoReport ad_report;
    {
        ad_report.setSource( EInfoReport::Source::NA, false );
        ad_report.setTimestamp( jau::getCurrentMilliseconds() );
        ad_report.setAddressType( bin.getRemoteAddrAndType().type );
        ad_report.setAddress( bin.getRemoteAddrAndType().address );
    }
    // Enforce BTRole::Master on new device,
    // since this functionality is only for local being BTRole::Slave peripheral!
    BTDeviceRef device = BTDevice::make_shared(*this, ad_report);
    device->btRole = BTRole::Master;
    addSharedDevice(device);

    HCIStatusCode res;
    res = mgmt->unpairDevice(dev_id, bin.getRemoteAddrAndType(), false /* disconnect */);
    if( HCIStatusCode::SUCCESS != res && HCIStatusCode::NOT_PAIRED != res ) {
        ERR_PRINT("(dev_id %d): Unpair device failed %s of %s: %s",
                dev_id, to_string(res).c_str(), bin.getRemoteAddrAndType().toString().c_str(),
                toString().c_str());
    }

    res = device->uploadKeys(bin, BTSecurityLevel::NONE);
    if( HCIStatusCode::SUCCESS != res ) {
        WARN_PRINT("(dev_id %d): Upload SMPKeyBin failed %s, %s (removing file)",
                dev_id, to_string(res).c_str(), bin.toString().c_str());
        if( key_path.size() > 0 ) {
            bin.remove(key_path);
        }
        return res;
    } else {
        DBG_PRINT("BTAdapter::setSMPKeyBin(dev_id %d): Upload OK: %s, %s",
                dev_id, bin.toString().c_str(), toString().c_str());
    }
    addSMPKeyBin( std::make_shared<SMPKeyBin>(bin), write); // PERIPHERAL_ADAPTER_MANAGES_SMP_KEYS
    return HCIStatusCode::SUCCESS;
}

HCIStatusCode BTAdapter::initialize(const BTMode btMode) noexcept {
    const bool was_powered = adapterInfo.isCurrentSettingBitSet(AdapterSetting::POWERED);
    adapter_initialized = true;

    // Also fails if unable to power-on and not powered-on!
    HCIStatusCode status = mgmt->initializeAdapter(adapterInfo, dev_id, BTRole::None /* unused */, btMode);
    if( HCIStatusCode::SUCCESS != status ) {
        WARN_PRINT("Adapter[%d]: Failed initializing (1): res0 %s, powered[before %d, now %d], %s - %s",
                dev_id, to_string(status).c_str(),
                was_powered, adapterInfo.isCurrentSettingBitSet(AdapterSetting::POWERED),
                adapterInfo.toString().c_str(), toString().c_str());
        return status;
    }
    const bool is_powered = adapterInfo.isCurrentSettingBitSet(AdapterSetting::POWERED);
    if( !was_powered ) {
        adapter_poweredon_at_init = true;
    }
    if( !enableListening(true) ) {
        return HCIStatusCode::INTERNAL_FAILURE;
    }
    updateAdapterSettings(false /* off_thread */, adapterInfo.getCurrentSettingMask(), false /* sendEvent */, 0);

    WORDY_PRINT("BTAdapter::initialize: Adapter[%d]: OK: powered[before %d, init_on %d, now %d], %s",
            dev_id, was_powered, adapter_poweredon_at_init.load(), is_powered,
            toString().c_str());
    return HCIStatusCode::SUCCESS;
}

bool BTAdapter::lockConnect(const BTDevice & device, const bool wait, const SMPIOCapability io_cap) noexcept {
    std::unique_lock<std::mutex> lock(mtx_single_conn_device); // RAII-style acquire and relinquish via destructor
    const jau::fraction_i64 timeout = 10_s; // FIXME: Configurable?

    if( nullptr != single_conn_device_ptr ) {
        if( device == *single_conn_device_ptr ) {
            COND_PRINT(debug_lock, "BTAdapter::lockConnect: Success: Already locked, same device: %s", device.toString().c_str());
            return true; // already set, same device: OK, locked
        }
        if( wait ) {
            const jau::fraction_timespec timeout_time = jau::getMonotonicTime() + jau::fraction_timespec(timeout);
            while( nullptr != single_conn_device_ptr ) {
                std::cv_status s = wait_until(cv_single_conn_device, lock, timeout_time);
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
        if constexpr ( USE_LINUX_BT_SECURITY ) {
            SMPIOCapability pre_io_cap { SMPIOCapability::UNSET };
            const bool res_iocap = mgmt->setIOCapability(dev_id, io_cap, pre_io_cap);
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
                lock.unlock(); // unlock mutex before notify_all to avoid pessimistic re-block of notified wait() thread.
                cv_single_conn_device.notify_all(); // notify waiting getter
                return false;
            }
        } else {
            COND_PRINT(debug_lock, "BTAdapter::lockConnect: Success: New lock, ignored io-cap: %s, %s",
                    to_string(io_cap).c_str(), device.toString().c_str());
            return true;
        }
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
        if( USE_LINUX_BT_SECURITY && SMPIOCapability::UNSET != v ) {
            // Unreachable: !USE_LINUX_BT_SECURITY
            SMPIOCapability o;
            const bool res = mgmt->setIOCapability(dev_id, v, o);
            COND_PRINT(debug_lock, "BTAdapter::unlockConnect: Success: setIOCapability[res %d: %s -> %s], %s",
                res, to_string(o).c_str(), to_string(v).c_str(),
                single_conn_device_ptr->toString().c_str());
        } else {
            COND_PRINT(debug_lock, "BTAdapter::unlockConnect: Success: %s",
                single_conn_device_ptr->toString().c_str());
        }
        single_conn_device_ptr = nullptr;
        lock.unlock(); // unlock mutex before notify_all to avoid pessimistic re-block of notified wait() thread.
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
        if( USE_LINUX_BT_SECURITY && SMPIOCapability::UNSET != v ) {
            // Unreachable: !USE_LINUX_BT_SECURITY
            SMPIOCapability o;
            const bool res = mgmt->setIOCapability(dev_id, v, o);
            COND_PRINT(debug_lock, "BTAdapter::unlockConnectAny: Success: setIOCapability[res %d: %s -> %s]; %s",
                res, to_string(o).c_str(), to_string(v).c_str(),
                single_conn_device_ptr->toString().c_str());
        } else {
            COND_PRINT(debug_lock, "BTAdapter::unlockConnectAny: Success: %s",
                single_conn_device_ptr->toString().c_str());
        }
        single_conn_device_ptr = nullptr;
        lock.unlock(); // unlock mutex before notify_all to avoid pessimistic re-block of notified wait() thread.
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
        ERR_PRINT("Adapter invalid: %s, %s", jau::to_hexstring(this).c_str(), toString().c_str());
        return HCIStatusCode::UNSPECIFIED_ERROR;
    }
    if( !hci.isOpen() ) {
        ERR_PRINT("HCI closed: %s, %s", jau::to_hexstring(this).c_str(), toString().c_str());
        return HCIStatusCode::UNSPECIFIED_ERROR;
    }
#if 0
    const bool wasPowered = isPowered();
    // Adapter will be reset, close connections and cleanup off-thread.
    preReset();

    HCIStatusCode status = hci->reset();
    if( HCIStatusCode::SUCCESS != status ) {
        ERR_PRINT("reset failed: %s", to_string(status).c_str());
    } else if( wasPowered ) {
        if( !setPowered(true) ) {
            ERR_PRINT("setPowered(true) failed");
            status = HCIStatusCode::UNSPECIFIED_ERROR;
        }
    }
    return status;
#else
    return hci.resetAdapter();
#endif
}


HCIStatusCode BTAdapter::setDefaultLE_PHY(const LE_PHYs Tx, const LE_PHYs Rx) noexcept {
    if( !isPowered() ) { // isValid() && hci.isOpen() && POWERED
        poweredOff(false /* active */, "setDefaultLE_PHY.np");
        return HCIStatusCode::NOT_POWERED;
    }
    return hci.le_set_default_phy(Tx, Rx);
}

bool BTAdapter::isDeviceWhitelisted(const BDAddressAndType & addressAndType) noexcept {
    return mgmt->isDeviceWhitelisted(dev_id, addressAndType);
}

bool BTAdapter::addDeviceToWhitelist(const BDAddressAndType & addressAndType, const HCIWhitelistConnectType ctype,
                                      const uint16_t conn_interval_min, const uint16_t conn_interval_max,
                                      const uint16_t conn_latency, const uint16_t timeout) {
    if( !isPowered() ) { // isValid() && hci.isOpen() && POWERED
        poweredOff(false /* active */, "addDeviceToWhitelist.np");
        return false;
    }
    if( mgmt->isDeviceWhitelisted(dev_id, addressAndType) ) {
        ERR_PRINT("device already listed: dev_id %d, address%s", dev_id, addressAndType.toString().c_str());
        return true;
    }

    HCIStatusCode res = mgmt->uploadConnParam(dev_id, addressAndType, conn_interval_min, conn_interval_max, conn_latency, timeout);
    if( HCIStatusCode::SUCCESS != res ) {
        ERR_PRINT("uploadConnParam(dev_id %d, address%s, interval[%u..%u], latency %u, timeout %u): Failed %s",
                dev_id, addressAndType.toString().c_str(), conn_interval_min, conn_interval_max, conn_latency, timeout, to_string(res).c_str());
    }
    return mgmt->addDeviceToWhitelist(dev_id, addressAndType, ctype);
}

bool BTAdapter::removeDeviceFromWhitelist(const BDAddressAndType & addressAndType) {
    return mgmt->removeDeviceFromWhitelist(dev_id, addressAndType);
}

BTAdapter::statusListenerList_t::equal_comparator BTAdapter::adapterStatusListenerRefEqComparator =
        [](const StatusListenerPair &a, const StatusListenerPair &b) -> bool { return *a.listener == *b.listener; };

bool BTAdapter::addStatusListener(const AdapterStatusListenerRef& l) noexcept {
    if( nullptr == l ) {
        ERR_PRINT("AdapterStatusListener ref is null");
        return false;
    }
    const bool added = statusListenerList.push_back_unique(StatusListenerPair{l, std::weak_ptr<BTDevice>{} },
                                                           adapterStatusListenerRefEqComparator);
    if( added ) {
        sendAdapterSettingsInitial(*l, jau::getCurrentMilliseconds());
    }
    if( _print_device_lists || jau::environment::get().verbose ) {
        jau::PLAIN_PRINT(true, "BTAdapter::addStatusListener.1: added %d, %s", added, toString().c_str());
        printDeviceLists();
    }
    return added;
}

bool BTAdapter::addStatusListener(const BTDeviceRef& d, const AdapterStatusListenerRef& l) noexcept {
    if( nullptr == l ) {
        ERR_PRINT("AdapterStatusListener ref is null");
        return false;
    }
    if( nullptr == d ) {
        ERR_PRINT("Device ref is null");
        return false;
    }
    const bool added = statusListenerList.push_back_unique(StatusListenerPair{l, d},
                                                           adapterStatusListenerRefEqComparator);
    if( added ) {
        sendAdapterSettingsInitial(*l, jau::getCurrentMilliseconds());
    }
    if( _print_device_lists || jau::environment::get().verbose ) {
        jau::PLAIN_PRINT(true, "BTAdapter::addStatusListener.2: added %d, %s", added, toString().c_str());
        printDeviceLists();
    }
    return added;
}

bool BTAdapter::addStatusListener(const BTDevice& d, const AdapterStatusListenerRef& l) noexcept {
    return addStatusListener(getSharedDevice(d), l);
}

bool BTAdapter::removeStatusListener(const AdapterStatusListenerRef& l) noexcept {
    if( nullptr == l ) {
        ERR_PRINT("AdapterStatusListener ref is null");
        return false;
    }
    const int count = statusListenerList.erase_matching(StatusListenerPair{l, std::weak_ptr<BTDevice>{}},
                                                        false /* all_matching */,
                                                        adapterStatusListenerRefEqComparator);
    if( _print_device_lists || jau::environment::get().verbose ) {
        jau::PLAIN_PRINT(true, "BTAdapter::removeStatusListener.1: res %d, %s", count>0, toString().c_str());
        printDeviceLists();
    }
    return count > 0;
}

bool BTAdapter::removeStatusListener(const AdapterStatusListener * l) noexcept {
    if( nullptr == l ) {
        ERR_PRINT("AdapterStatusListener ref is null");
        return false;
    }
    bool res = false;
    {
        auto it = statusListenerList.begin(); // lock mutex and copy_store
        for (; !it.is_end(); ++it ) {
            if ( *it->listener == *l ) {
                it.erase();
                it.write_back();
                res = true;
                break;
            }
        }
    }
    if( _print_device_lists || jau::environment::get().verbose ) {
        jau::PLAIN_PRINT(true, "BTAdapter::removeStatusListener.2: res %d, %s", res, toString().c_str());
        printDeviceLists();
    }
    return res;
}

int BTAdapter::removeAllStatusListener(const BTDevice& d) noexcept {
    int count = 0;

    int res = statusListenerList.size();
    if( 0 < res ) {
        auto begin = statusListenerList.begin();
        auto it = begin.end();
        do {
            --it;
            BTDeviceRef sda = it->wbr_device.lock();
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

int BTAdapter::removeAllStatusListener() noexcept {
    int count = statusListenerList.size();
    statusListenerList.clear();
    return count;
}

void BTAdapter::checkDiscoveryState() noexcept {
    const ScanType currentNativeScanType = hci.getCurrentScanType();
    // Check LE scan state
    if( DiscoveryPolicy::AUTO_OFF == discovery_policy ) {
        if( is_set(currentMetaScanType, ScanType::LE) != is_set(currentNativeScanType, ScanType::LE) ) {
            std::string msg("Invalid DiscoveryState: policy "+to_string(discovery_policy)+
                    ", currentScanType*[native "+
                    to_string(currentNativeScanType)+" != meta "+
                    to_string(currentMetaScanType)+"], "+toString());
            ERR_PRINT(msg.c_str());
            // ABORT?
        }
    } else {
        if( !is_set(currentMetaScanType, ScanType::LE) && is_set(currentNativeScanType, ScanType::LE) ) {
            std::string msg("Invalid DiscoveryState: policy "+to_string(discovery_policy)+
                    ", currentScanType*[native "+
                    to_string(currentNativeScanType)+", meta "+
                    to_string(currentMetaScanType)+"], "+toString());
            ERR_PRINT(msg.c_str());
            // ABORT?
        }
    }
}

HCIStatusCode BTAdapter::startDiscovery(const DiscoveryPolicy policy, const bool le_scan_active,
                                        const uint16_t le_scan_interval, const uint16_t le_scan_window,
                                        const uint8_t filter_policy,
                                        const bool filter_dup) noexcept
{
    // FIXME: Respect BTAdapter::btMode, i.e. BTMode::BREDR, BTMode::LE or BTMode::DUAL to setup BREDR, LE or DUAL scanning!
    // ERR_PRINT("Test");
    // throw jau::RuntimeException("Test", E_FILE_LINE);

    clearDevicesPausingDiscovery();

    if( !isPowered() ) { // isValid() && hci.isOpen() && POWERED
        poweredOff(false /* active */, "startDiscovery.np");
        return HCIStatusCode::NOT_POWERED;
    }

    const std::lock_guard<std::mutex> lock(mtx_discovery); // RAII-style acquire and relinquish via destructor

    if( isAdvertising() ) {
        WARN_PRINT("Adapter in advertising mode: %s", toString(true).c_str());
        return HCIStatusCode::COMMAND_DISALLOWED;
    }

    l2cap_service.stop();

    removeDiscoveredDevices();

    scan_filter_dup = filter_dup; // cache for background scan

    const ScanType currentNativeScanType = hci.getCurrentScanType();

    if( is_set(currentNativeScanType, ScanType::LE) ) {
        btRole = BTRole::Master;
        if( discovery_policy == policy ) {
            DBG_PRINT("BTAdapter::startDiscovery: Already discovering, unchanged policy %s -> %s, currentScanType[native %s, meta %s] ...\n- %s",
                    to_string(discovery_policy).c_str(), to_string(policy).c_str(),
                    to_string(currentNativeScanType).c_str(), to_string(currentMetaScanType).c_str(), toString(true).c_str());
        } else {
            DBG_PRINT("BTAdapter::startDiscovery: Already discovering, changed policy %s -> %s, currentScanType[native %s, meta %s] ...\n- %s",
                    to_string(discovery_policy).c_str(), to_string(policy).c_str(),
                    to_string(currentNativeScanType).c_str(), to_string(currentMetaScanType).c_str(), toString(true).c_str());
            discovery_policy = policy;
        }
        checkDiscoveryState();
        return HCIStatusCode::SUCCESS;
    }

    if( _print_device_lists || jau::environment::get().verbose ) {
        jau::PLAIN_PRINT(true, "BTAdapter::startDiscovery: Start: policy %s -> %s, currentScanType[native %s, meta %s] ...\n- %s",
                to_string(discovery_policy).c_str(), to_string(policy).c_str(),
                to_string(currentNativeScanType).c_str(), to_string(currentMetaScanType).c_str(), toString().c_str());
    }

    discovery_policy = policy;

    // TODO: Potential changing adapter address mode to random and updating 'visibleAddressAndType'!
    const BDAddressType usedAddrType = BDAddressType::BDADDR_LE_PUBLIC;
    const HCILEOwnAddressType own_mac_type=to_HCILEOwnAddressType(usedAddrType);
    // if le_enable_scan(..) is successful, it will issue 'mgmtEvDeviceDiscoveringHCI(..)' immediately, which updates currentMetaScanType.
    const HCIStatusCode status = hci.le_start_scan(filter_dup, le_scan_active, own_mac_type,
                                                   le_scan_interval, le_scan_window, filter_policy);

    if( _print_device_lists || jau::environment::get().verbose ) {
        jau::PLAIN_PRINT(true, "BTAdapter::startDiscovery: End: Result %s, policy %s -> %s, currentScanType[native %s, meta %s] ...\n- %s",
                to_string(status).c_str(),
                to_string(discovery_policy).c_str(), to_string(policy).c_str(),
                to_string(hci.getCurrentScanType()).c_str(), to_string(currentMetaScanType).c_str(), toString().c_str());
        printDeviceLists();
    }

    checkDiscoveryState();

    return status;
}

void BTAdapter::discoveryServerWork(jau::service_runner& sr) noexcept {
    static jau::nsize_t trial_count = 0;
    bool retry = false;

    // FIXME: Respect BTAdapter::btMode, i.e. BTMode::BREDR, BTMode::LE or BTMode::DUAL to setup BREDR, LE or DUAL scanning!
    if( !isPowered() ) { // isValid() && hci.isOpen() && POWERED
        poweredOff(false /* active */, "discoveryServerWork.np");
    } else {
        {
            const std::lock_guard<std::mutex> lock(mtx_discovery); // RAII-style acquire and relinquish via destructor
            const ScanType currentNativeScanType = hci.getCurrentScanType();

            if( !is_set(currentNativeScanType, ScanType::LE) &&
                DiscoveryPolicy::AUTO_OFF != discovery_policy &&
                0 == getDevicesPausingDiscoveryCount() ) // still required to start discovery ???
            {
                // if le_enable_scan(..) is successful, it will issue 'mgmtEvDeviceDiscoveringHCI(..)' immediately, which updates currentMetaScanType.
                DBG_PRINT("BTAdapter::startDiscoveryBackground[%u/%u]: Policy %s, currentScanType[native %s, meta %s] ... %s",
                        trial_count+1, MAX_BACKGROUND_DISCOVERY_RETRY,
                        to_string(discovery_policy).c_str(),
                        to_string(currentNativeScanType).c_str(), to_string(currentMetaScanType).c_str(), toString().c_str());
                const HCIStatusCode status = hci.le_enable_scan(true /* enable */, scan_filter_dup);
                if( HCIStatusCode::SUCCESS != status ) {
                    ERR_PRINT2("le_enable_scan failed[%u/%u]: %s - %s",
                            trial_count+1, MAX_BACKGROUND_DISCOVERY_RETRY,
                            to_string(status).c_str(), toString().c_str());
                    if( trial_count < MAX_BACKGROUND_DISCOVERY_RETRY ) {
                        trial_count++;
                        retry = true;
                    }
                }
                checkDiscoveryState();
            }
        }
        if( retry && !sr.shall_stop() ) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100)); // wait a little (FIXME)
        }
    }
    if( !retry ) {
        trial_count=0;
        sr.set_shall_stop();
    }
}

HCIStatusCode BTAdapter::stopDiscovery() noexcept {
    clearDevicesPausingDiscovery();

    return stopDiscoveryImpl(false /* forceDiscoveringEvent */, false /* temporary */);
}

HCIStatusCode BTAdapter::stopDiscoveryImpl(const bool forceDiscoveringEvent, const bool temporary) noexcept {
    // We allow !isEnabled, to utilize method for adjusting discovery state and notifying listeners
    // FIXME: Respect BTAdapter::btMode, i.e. BTMode::BREDR, BTMode::LE or BTMode::DUAL to stop BREDR, LE or DUAL scanning!

    if( !isValid() ) {
        ERR_PRINT("Adapter invalid: %s, %s", jau::to_hexstring(this).c_str(), toString().c_str());
        return HCIStatusCode::UNSPECIFIED_ERROR;
    }
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
    const bool le_scan_temp_disabled = is_set(currentMetaScanType, ScanType::LE) &&    // true
                                       !is_set(currentNativeScanType, ScanType::LE) && // false
                                       DiscoveryPolicy::AUTO_OFF != discovery_policy;       // true

    DBG_PRINT("BTAdapter::stopDiscovery: Start: policy %s, currentScanType[native %s, meta %s], le_scan_temp_disabled %d, forceDiscEvent %d ...",
            to_string(discovery_policy).c_str(),
            to_string(currentNativeScanType).c_str(), to_string(currentMetaScanType).c_str(),
            le_scan_temp_disabled, forceDiscoveringEvent);

    if( !temporary ) {
        discovery_policy = DiscoveryPolicy::AUTO_OFF;
    }

    if( !is_set(currentMetaScanType, ScanType::LE) ) {
        DBG_PRINT("BTAdapter::stopDiscovery: Already disabled, policy %s, currentScanType[native %s, meta %s] ...",
                to_string(discovery_policy).c_str(),
                to_string(currentNativeScanType).c_str(), to_string(currentMetaScanType).c_str());
        checkDiscoveryState();
        return HCIStatusCode::SUCCESS;
    }

    HCIStatusCode status;
    if( !isPowered() ) { // isValid() && hci.isOpen() && POWERED
        poweredOff(false /* active */, "stopDiscoveryImpl.np");
        status = HCIStatusCode::NOT_POWERED;
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
            ERR_PRINT("le_enable_scan failed: %s", to_string(status).c_str());
        }
    }

exit:
    if( HCIStatusCode::SUCCESS != status ) {
        // Sync nativeDiscoveryState with  currentMetaScanType,
        // the latter git set to NONE via mgmtEvDeviceDiscoveringHCI(..) below.
        // Resolves checkDiscoveryState error message @ HCI command failure.
        hci.setCurrentScanType( ScanType::NONE );
    }
    if( le_scan_temp_disabled || forceDiscoveringEvent || HCIStatusCode::SUCCESS != status ) {
        // In case of discoveryTempDisabled, power-off, le_enable_scan failure
        // or already closed HCIHandler, send the event directly.
        const MgmtEvtDiscovering e(dev_id, ScanType::LE, false);
        mgmtEvDeviceDiscoveringHCI( e );
    }
    if( _print_device_lists || jau::environment::get().verbose ) {
        jau::PLAIN_PRINT(true, "BTAdapter::stopDiscovery: End: Result %s, policy %s, currentScanType[native %s, meta %s], le_scan_temp_disabled %d ...\n- %s",
                to_string(status).c_str(), to_string(discovery_policy).c_str(),
                to_string(hci.getCurrentScanType()).c_str(), to_string(currentMetaScanType).c_str(), le_scan_temp_disabled,
                toString().c_str());
        printDeviceLists();
    }

    checkDiscoveryState();

    return status;
}

// *************************************************

BTDeviceRef BTAdapter::findDiscoveredDevice (const EUI48 & address, const BDAddressType addressType) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_discoveredDevices); // RAII-style acquire and relinquish via destructor
    return findDevice(discoveredDevices, address, addressType);
}

bool BTAdapter::addDiscoveredDevice(BTDeviceRef const &device) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_discoveredDevices); // RAII-style acquire and relinquish via destructor
    if( nullptr != findDevice(discoveredDevices, *device) ) {
        // already discovered
        return false;
    }
    discoveredDevices.push_back(device);
    return true;
}

bool BTAdapter::removeDiscoveredDevice(const BDAddressAndType & addressAndType) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_discoveredDevices); // RAII-style acquire and relinquish via destructor
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
    int res;
    {
        const std::lock_guard<std::mutex> lock(mtx_discoveredDevices); // RAII-style acquire and relinquish via destructor

        res = discoveredDevices.size();
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
    }
    if( _print_device_lists || jau::environment::get().verbose ) {
        jau::PLAIN_PRINT(true, "BTAdapter::removeDiscoveredDevices: End: %d, %s", res, toString().c_str());
        printDeviceLists();
    }
    return res;
}

jau::darray<BTDeviceRef> BTAdapter::getDiscoveredDevices() const noexcept {
    jau::sc_atomic_critical sync(sync_data); // lock-free simple cache-load 'snapshot'
    device_list_t res = discoveredDevices;
    return res;
}

// *************************************************

bool BTAdapter::addSharedDevice(BTDeviceRef const &device) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_sharedDevices); // RAII-style acquire and relinquish via destructor
    if( nullptr != findDevice(sharedDevices, *device) ) {
        // already shared
        return false;
    }
    sharedDevices.push_back(device);
    return true;
}

BTDeviceRef BTAdapter::getSharedDevice(const BTDevice & device) noexcept {
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

BTDeviceRef BTAdapter::findSharedDevice (const EUI48 & address, const BDAddressType addressType) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_sharedDevices); // RAII-style acquire and relinquish via destructor
    return findDevice(sharedDevices, address, addressType);
}

// *************************************************

void BTAdapter::removeDevice(BTDevice & device) noexcept {
    WORDY_PRINT("DBTAdapter::removeDevice: Start %s", toString().c_str());
    removeAllStatusListener(device);

    const HCIStatusCode status = device.disconnect(HCIStatusCode::REMOTE_USER_TERMINATED_CONNECTION);
    WORDY_PRINT("BTAdapter::removeDevice: disconnect %s, %s", to_string(status).c_str(), toString().c_str());
    unlockConnect(device);
    removeConnectedDevice(device); // usually done in BTAdapter::mgmtEvDeviceDisconnectedHCI
    removeDiscoveredDevice(device.addressAndType); // usually done in BTAdapter::mgmtEvDeviceDisconnectedHCI
    removeDevicePausingDiscovery(device);
    removeSharedDevice(device);

    if( _print_device_lists || jau::environment::get().verbose ) {
        jau::PLAIN_PRINT(true, "BTAdapter::removeDevice: End %s, %s", device.getAddressAndType().toString().c_str(), toString().c_str());
        printDeviceLists();
    }
}

// *************************************************

BTAdapter::SMPKeyBinRef BTAdapter::findSMPKeyBin(key_list_t & keys, BDAddressAndType const & remoteAddress) noexcept {
    const jau::nsize_t size = keys.size();
    for (jau::nsize_t i = 0; i < size; ++i) {
        SMPKeyBinRef& k = keys[i];
        if ( nullptr != k && remoteAddress == k->getRemoteAddrAndType() ) {
            return k;
        }
    }
    return nullptr;
}
bool BTAdapter::removeSMPKeyBin(key_list_t & keys, BDAddressAndType const & remoteAddress, const bool remove_file, const std::string key_path_) noexcept {
    for (auto it = keys.begin(); it != keys.end(); ++it) {
        const SMPKeyBinRef& k = *it;
        if ( nullptr != k && remoteAddress == k->getRemoteAddrAndType() ) {
            DBG_PRINT("BTAdapter::removeSMPKeyBin(file %d): %s", remove_file, k->toString().c_str());
            if( remove_file && key_path_.size() > 0 ) {
                if( !k->remove(key_path_) ) {
                    WARN_PRINT("Failed removal of SMPKeyBin file: %s", k->getFilename(key_path_).c_str());
                }
            }
            keys.erase(it);
            return true;
        }
    }
    return false;
}
BTAdapter::SMPKeyBinRef BTAdapter::findSMPKeyBin(BDAddressAndType const & remoteAddress) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_keys); // RAII-style acquire and relinquish via destructor
    return findSMPKeyBin(key_list, remoteAddress);
}
bool BTAdapter::addSMPKeyBin(const SMPKeyBinRef& key, const bool write_file) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_keys); // RAII-style acquire and relinquish via destructor
    removeSMPKeyBin(key_list, key->getRemoteAddrAndType(), write_file /* remove_file */, key_path);
    if( jau::environment::get().debug ) {
        key->setVerbose(true);
        DBG_PRINT("BTAdapter::addSMPKeyBin(file %d): %s", write_file, key->toString().c_str());
    }
    key_list.push_back( key );
    if( write_file && key_path.size() > 0 ) {
        if( !key->write(key_path, true /* overwrite */) ) {
            WARN_PRINT("Failed write of SMPKeyBin file: %s", key->getFilename(key_path).c_str());
        }
    }
    return true;
}
bool BTAdapter::removeSMPKeyBin(BDAddressAndType const & remoteAddress, const bool remove_file) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_keys); // RAII-style acquire and relinquish via destructor
    return removeSMPKeyBin(key_list, remoteAddress, remove_file, key_path);
}

// *************************************************

HCIStatusCode BTAdapter::startAdvertising(DBGattServerRef gattServerData_,
                               EInfoReport& eir, EIRDataType adv_mask, EIRDataType scanrsp_mask,
                               const uint16_t adv_interval_min, const uint16_t adv_interval_max,
                               const AD_PDU_Type adv_type,
                               const uint8_t adv_chan_map,
                               const uint8_t filter_policy) noexcept {
    if( !isPowered() ) { // isValid() && hci.isOpen() && POWERED
        poweredOff(false /* active */, "startAdvertising.np");
        return HCIStatusCode::NOT_POWERED;
    }

    if( isDiscovering() ) {
        WARN_PRINT("Not allowed (scan enabled): %s", toString(true).c_str());
        return HCIStatusCode::COMMAND_DISALLOWED;
    }
    const jau::nsize_t connCount = getConnectedDeviceCount();
    if( 0 < connCount ) { // FIXME: May shall not be a restriction
        WARN_PRINT("Not allowed (%d connections open/pending): %s", connCount, toString(true).c_str());
        return HCIStatusCode::COMMAND_DISALLOWED;
    }
    if( jau::environment::get().debug ) {
        std::vector<MgmtDefaultParam> params = mgmt->readDefaultSysParam(dev_id);
        DBG_PRINT("BTAdapter::startAdvertising[%d]: SysParam: %zd", dev_id, params.size());
        for(size_t i=0; i<params.size(); ++i) {
            jau::PLAIN_PRINT(true, "[%2.2zd]: %s", i, params[i].toString().c_str());
        }
    }
    if constexpr ( USE_LINUX_BT_SECURITY ) {
        SMPIOCapability pre_io_cap { SMPIOCapability::UNSET };
        const bool res_iocap = mgmt->setIOCapability(dev_id, SMPIOCapability::NO_INPUT_NO_OUTPUT, pre_io_cap);
        DBG_PRINT("BTAdapter::startAdvertising: dev_id %u, setIOCapability[%s -> %s]: result %d",
            dev_id, to_string(pre_io_cap).c_str(), to_string(SMPIOCapability::NO_INPUT_NO_OUTPUT).c_str(), res_iocap);
    }
    // FIXME?? std::this_thread::sleep_for(std::chrono::milliseconds(100)); // wait a little (FIXME: Fast restart of advertising error)
    l2cap_service.start();

    // set minimum ...
    eir.addFlags(GAPFlags::LE_Gen_Disc);
    eir.setName(getName());
    if( EIRDataType::NONE == ( adv_mask & EIRDataType::FLAGS ) ||
        EIRDataType::NONE == ( scanrsp_mask & EIRDataType::FLAGS ) ) {
        adv_mask = adv_mask | EIRDataType::FLAGS;
    }
    if( EIRDataType::NONE == ( adv_mask & EIRDataType::NAME ) ||
        EIRDataType::NONE == ( scanrsp_mask & EIRDataType::NAME ) ) {
        scanrsp_mask = scanrsp_mask | EIRDataType::NAME;
    }

    if( nullptr != gattServerData_ ) {
        gattServerData_->setServicesHandles();
    }

    const EUI48 peer_bdaddr=EUI48::ANY_DEVICE;
    const HCILEOwnAddressType own_mac_type=HCILEOwnAddressType::PUBLIC;
    const HCILEOwnAddressType peer_mac_type=HCILEOwnAddressType::PUBLIC;

    HCIStatusCode status = hci.le_start_adv(eir, adv_mask, scanrsp_mask,
                                            peer_bdaddr, own_mac_type, peer_mac_type,
                                            adv_interval_min, adv_interval_max, adv_type, adv_chan_map, filter_policy);
    if( HCIStatusCode::SUCCESS != status ) {
        ERR_PRINT("le_start_adv failed: %s - %s", to_string(status).c_str(), toString(true).c_str());
        l2cap_service.stop();
    } else {
        gattServerData = gattServerData_;
        btRole = BTRole::Slave;
    }
    return status;
}

HCIStatusCode BTAdapter::startAdvertising(DBGattServerRef gattServerData_,
                               const uint16_t adv_interval_min, const uint16_t adv_interval_max,
                               const AD_PDU_Type adv_type,
                               const uint8_t adv_chan_map,
                               const uint8_t filter_policy) noexcept {
    EInfoReport eir;
    EIRDataType adv_mask = EIRDataType::FLAGS | EIRDataType::SERVICE_UUID;
    EIRDataType scanrsp_mask = EIRDataType::NAME | EIRDataType::CONN_IVAL;

    eir.setFlags(GAPFlags::LE_Gen_Disc);
    eir.setName(getName());
    eir.setConnInterval(10, 24); // default
    if( nullptr != gattServerData_ ) {
        for(DBGattServiceRef& s : gattServerData_->getServices()) {
            eir.addService(s->getType());
        }
    }

    return startAdvertising(gattServerData_,
                            eir, adv_mask, scanrsp_mask,
                            adv_interval_min, adv_interval_max,
                            adv_type,
                            adv_chan_map,
                            filter_policy);
}

/**
 * Closes the advertising session.
 * <p>
 * This adapter's HCIHandler instance is used to stop advertising,
 * see HCIHandler::le_enable_adv().
 * </p>
 * @return HCIStatusCode::SUCCESS if successful, otherwise the HCIStatusCode error state
 */
HCIStatusCode BTAdapter::stopAdvertising() noexcept {
    if( !isPowered() ) { // isValid() && hci.isOpen() && POWERED
        poweredOff(false /* active */, "stopAdvertising.np");
        return HCIStatusCode::NOT_POWERED;
    }

    l2cap_service.stop();

    HCIStatusCode status = hci.le_enable_adv(false /* enable */);
    if( HCIStatusCode::SUCCESS != status ) {
        ERR_PRINT("le_enable_adv failed: %s", to_string(status).c_str());
    }
    return status;
}

// *************************************************

std::string BTAdapter::toString(bool includeDiscoveredDevices) const noexcept {
    std::string random_address_info = adapterInfo.addressAndType != visibleAddressAndType ? " ("+visibleAddressAndType.toString()+")" : "";
    std::string out("Adapter[BT "+std::to_string(getBTMajorVersion())+", BTMode "+to_string(getBTMode())+", "+to_string(getRole())+", "+adapterInfo.addressAndType.toString()+random_address_info+
                    ", '"+getName()+"', id "+std::to_string(dev_id)+
                    ", curSettings"+to_string(adapterInfo.getCurrentSettingMask())+
                    ", valid "+std::to_string(isValid())+
                    ", adv "+std::to_string(hci.isAdvertising())+
                    ", scanType[native "+to_string(hci.getCurrentScanType())+", meta "+to_string(currentMetaScanType)+"]"
                    ", open[mgmt, "+std::to_string(mgmt->isOpen())+", hci "+std::to_string(hci.isOpen())+
                    "], "+l2cap_att_srv.toString()+", "+javaObjectToString()+"]");
    if( includeDiscoveredDevices ) {
        device_list_t devices = getDiscoveredDevices();
        if( devices.size() > 0 ) {
            out.append("\n");
            for(auto it = devices.begin(); it != devices.end(); it++) {
                BTDeviceRef p = *it;
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
    jau::for_each_fidelity(statusListenerList, [&](StatusListenerPair &p) {
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

void BTAdapter::sendDeviceUpdated(std::string cause, BTDeviceRef device, uint64_t timestamp, EIRDataType updateMask) noexcept {
    int i=0;
    jau::for_each_fidelity(statusListenerList, [&](StatusListenerPair &p) {
        try {
            if( p.match(device) ) {
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

bool BTAdapter::mgmtEvHCIAnyHCI(const MgmtEvent& e) noexcept {
    DBG_PRINT("BTAdapter:hci::Any: %s", e.toString().c_str());
    (void)e;
    return true;
}

bool BTAdapter::mgmtEvDeviceDiscoveringHCI(const MgmtEvent& e) noexcept {
    const MgmtEvtDiscovering &event = *static_cast<const MgmtEvtDiscovering *>(&e);
    return mgmtEvDeviceDiscoveringAny(event.getScanType(), event.getEnabled(), event.getTimestamp(), true /* hciSourced */);
}

bool BTAdapter::mgmtEvDeviceDiscoveringMgmt(const MgmtEvent& e) noexcept {
    const MgmtEvtDiscovering &event = *static_cast<const MgmtEvtDiscovering *>(&e);
    return mgmtEvDeviceDiscoveringAny(event.getScanType(), event.getEnabled(), event.getTimestamp(), false /* hciSourced */);
}

void BTAdapter::updateDeviceDiscoveringState(const ScanType eventScanType, const bool eventEnabled) noexcept {
    mgmtEvDeviceDiscoveringAny(eventScanType, eventEnabled, jau::getCurrentMilliseconds(), false /* hciSourced */);
}

bool BTAdapter::mgmtEvDeviceDiscoveringAny(const ScanType eventScanType, const bool eventEnabled, const uint64_t eventTimestamp,
                                           const bool hciSourced) noexcept {
    const std::string srctkn = hciSourced ? "hci" : "mgmt";
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
        if( is_set(eventScanType, ScanType::LE) && DiscoveryPolicy::AUTO_OFF != discovery_policy ) {
            // Unchanged meta for disabled-LE && keep_le_scan_alive
            nextMetaScanType = currentMetaScanType;
        } else {
            nextMetaScanType = changeScanType(currentMetaScanType, eventScanType, false);
        }
    }

    if( !hciSourced ) {
        // update HCIHandler's currentNativeScanType from other source
        const ScanType nextNativeScanType = changeScanType(currentNativeScanType, eventScanType, eventEnabled);
        DBG_PRINT("BTAdapter:%s:DeviceDiscovering: dev_id %d, policy %s: scanType[native %s -> %s, meta %s -> %s])",
            srctkn.c_str(), dev_id, to_string(discovery_policy).c_str(),
            to_string(currentNativeScanType).c_str(), to_string(nextNativeScanType).c_str(),
            to_string(currentMetaScanType).c_str(), to_string(nextMetaScanType).c_str());
        currentNativeScanType = nextNativeScanType;
        hci.setCurrentScanType(currentNativeScanType);
    } else {
        DBG_PRINT("BTAdapter:%s:DeviceDiscovering: dev_id %d, policy %d: scanType[native %s, meta %s -> %s])",
            srctkn.c_str(), dev_id, to_string(discovery_policy).c_str(),
            to_string(currentNativeScanType).c_str(),
            to_string(currentMetaScanType).c_str(), to_string(nextMetaScanType).c_str());
    }
    currentMetaScanType = nextMetaScanType;
    if( isDiscovering() ) {
        btRole = BTRole::Master;
    }

    checkDiscoveryState();

    int i=0;
    jau::for_each_fidelity(statusListenerList, [&](StatusListenerPair &p) {
        try {
            p.listener->discoveringChanged(*this, currentMetaScanType, eventScanType, eventEnabled, discovery_policy, eventTimestamp);
        } catch (std::exception &except) {
            ERR_PRINT("BTAdapter:%s:DeviceDiscovering-CBs %d/%zd: %s of %s: Caught exception %s",
                    srctkn.c_str(), i+1, statusListenerList.size(),
                    p.listener->toString().c_str(), toString().c_str(), except.what());
        }
        i++;
    });

    if( !is_set(currentNativeScanType, ScanType::LE) &&
        DiscoveryPolicy::AUTO_OFF != discovery_policy &&
        0 == getDevicesPausingDiscoveryCount() )
    {
        discovery_service.start();
    }
    return true;
}

bool BTAdapter::mgmtEvNewSettingsMgmt(const MgmtEvent& e) noexcept {
    COND_PRINT(debug_event, "BTAdapter:mgmt:NewSettings: %s", e.toString().c_str());
    const MgmtEvtNewSettings &event = *static_cast<const MgmtEvtNewSettings *>(&e);
    const AdapterSetting new_settings = adapterInfo.setCurrentSettingMask(event.getSettings()); // probably done by mgmt callback already

    updateAdapterSettings(true /* off_thread */, new_settings, true /* sendEvent */, event.getTimestamp());
    return true;
}

void BTAdapter::updateAdapterSettings(const bool off_thread, const AdapterSetting new_settings, const bool sendEvent, const uint64_t timestamp) noexcept {
    const AdapterSetting old_settings_ = old_settings;

    const AdapterSetting changes = getAdapterSettingMaskDiff(new_settings, old_settings_);

    const bool justPoweredOn = isAdapterSettingBitSet(changes, AdapterSetting::POWERED) &&
                               isAdapterSettingBitSet(new_settings, AdapterSetting::POWERED);

    const bool justPoweredOff = isAdapterSettingBitSet(changes, AdapterSetting::POWERED) &&
                                !isAdapterSettingBitSet(new_settings, AdapterSetting::POWERED);

    old_settings = new_settings;

    COND_PRINT(debug_event, "BTAdapter::updateAdapterSettings: %s -> %s, changes %s: %s, sendEvent %d, offThread %d",
            to_string(old_settings_).c_str(),
            to_string(new_settings).c_str(),
            to_string(changes).c_str(), toString().c_str(), sendEvent, off_thread );

    updateDataFromAdapterInfo();

    if( justPoweredOn ) {
        // Adapter has been powered on, ensure all hci states are reset.
        if( hci.resetAllStates(true) ) {
            updateDataFromHCI();
        }
    }
    if( sendEvent && AdapterSetting::NONE != changes ) {
        sendAdapterSettingsChanged(old_settings_, new_settings, changes, timestamp);
    }

    if( justPoweredOff ) {
        // Adapter has been powered off, close connections and cleanup off-thread.
        if( off_thread ) {
            std::thread bg(&BTAdapter::poweredOff, this, false, "powered_off.0"); // @suppress("Invalid arguments")
            bg.detach();
        } else {
            poweredOff(false, "powered_off.1");
        }
    }
}

bool BTAdapter::mgmtEvLocalNameChangedMgmt(const MgmtEvent& e) noexcept {
    COND_PRINT(debug_event, "BTAdapter:mgmt:LocalNameChanged: %s", e.toString().c_str());
    const MgmtEvtLocalNameChanged &event = *static_cast<const MgmtEvtLocalNameChanged *>(&e);
    std::string old_name = getName();
    std::string old_shortName = getShortName();
    const bool nameChanged = old_name != event.getName();
    const bool shortNameChanged = old_shortName != event.getShortName();
    if( nameChanged ) {
        adapterInfo.setName(event.getName());
    }
    if( shortNameChanged ) {
        adapterInfo.setShortName(event.getShortName());
    }
    COND_PRINT(debug_event, "BTAdapter:mgmt:LocalNameChanged: Local name: %d: '%s' -> '%s'; short_name: %d: '%s' -> '%s'",
            nameChanged, old_name.c_str(), getName().c_str(),
            shortNameChanged, old_shortName.c_str(), getShortName().c_str());
    (void)nameChanged;
    (void)shortNameChanged;
    return true;
}

void BTAdapter::l2capServerInit(jau::service_runner& sr0) noexcept {
    (void)sr0;

    l2cap_att_srv.set_interrupted_query( jau::bindMemberFunc(&l2cap_service, &jau::service_runner::shall_stop2) );

    if( !l2cap_att_srv.open() ) {
        ERR_PRINT("Adapter[%d]: L2CAP ATT open failed: %s", dev_id, l2cap_att_srv.toString().c_str());
    }
}

void BTAdapter::l2capServerEnd(jau::service_runner& sr) noexcept {
    (void)sr;
    if( !l2cap_att_srv.close() ) {
        ERR_PRINT("Adapter[%d]: L2CAP ATT close failed: %s", dev_id, l2cap_att_srv.toString().c_str());
    }
}

void BTAdapter::l2capServerWork(jau::service_runner& sr) noexcept {
    (void)sr;
    std::unique_ptr<L2CAPClient> l2cap_att_ = l2cap_att_srv.accept();
    if( BTRole::Slave == getRole() && nullptr != l2cap_att_ ) {
        DBG_PRINT("L2CAP-ACCEPT: BTAdapter::l2capServer connected.1: %s", l2cap_att_->toString().c_str());

        std::unique_lock<std::mutex> lock(mtx_l2cap_att); // RAII-style acquire and relinquish via destructor
        l2cap_att = std::move( l2cap_att_ );
        lock.unlock(); // unlock mutex before notify_all to avoid pessimistic re-block of notified wait() thread.
        cv_l2cap_att.notify_all(); // notify waiting getter

    } else if( nullptr != l2cap_att_ ) {
        DBG_PRINT("L2CAP-ACCEPT: BTAdapter::l2capServer connected.2: %s", l2cap_att_->toString().c_str());
    } else {
        DBG_PRINT("L2CAP-ACCEPT: BTAdapter::l2capServer connected.0: nullptr");
    }
}

std::unique_ptr<L2CAPClient> BTAdapter::get_l2cap_connection(std::shared_ptr<BTDevice> device) {
    if( BTRole::Slave == getRole() ) {
        const BDAddressAndType& clientAddrAndType = device->getAddressAndType();
        const jau::fraction_i64 timeout = L2CAP_CLIENT_CONNECT_TIMEOUT_MS;

        std::unique_lock<std::mutex> lock(mtx_l2cap_att); // RAII-style acquire and relinquish via destructor
        const jau::fraction_timespec timeout_time = jau::getMonotonicTime() + jau::fraction_timespec(timeout);
        while( device->getConnected() && ( nullptr == l2cap_att || l2cap_att->getRemoteAddressAndType() != clientAddrAndType ) ) {
            std::cv_status s = wait_until(cv_l2cap_att, lock, timeout_time);
            if( std::cv_status::timeout == s && ( nullptr == l2cap_att || l2cap_att->getRemoteAddressAndType() != clientAddrAndType ) ) {
                DBG_PRINT("L2CAP-ACCEPT: BTAdapter:get_l2cap_connection(dev_id %d): l2cap_att TIMEOUT", dev_id);
                return nullptr;
            }
        }
        if( nullptr != l2cap_att ) {
            std::unique_ptr<L2CAPClient> l2cap_att_ = std::move( l2cap_att );
            DBG_PRINT("L2CAP-ACCEPT: BTAdapter:get_l2cap_connection(dev_id %d): l2cap_att %s", dev_id, l2cap_att_->toString().c_str());
            return l2cap_att_; // copy elision
        } else {
            DBG_PRINT("L2CAP-ACCEPT: BTAdapter:get_l2cap_connection(dev_id %d): Might got disconnected", dev_id);
            return nullptr;
        }
    } else {
        DBG_PRINT("L2CAP-ACCEPT: BTAdapter:get_l2cap_connection(dev_id %d): Not in server mode", dev_id);
        return nullptr;
    }
}

jau::fraction_i64 BTAdapter::smp_timeoutfunc(jau::simple_timer& timer) {
    if( timer.shall_stop() ) {
        return 0_s;
    }
    device_list_t failed_devices;
    {
        const std::lock_guard<std::mutex> lock(mtx_connectedDevices); // RAII-style acquire and relinquish via destructor
        jau::for_each_fidelity(connectedDevices, [&](BTDeviceRef& device) {
            if( device->isValidInstance() && device->getConnected() &&
                BTSecurityLevel::NONE < device->getConnSecurityLevel() &&
                SMPPairingState::KEY_DISTRIBUTION == device->pairing_data.state )
                // isSMPPairingActive( device->pairing_data.state ) &&
                // !isSMPPairingUserInteraction( device->pairing_data.state ) )
            {
                // actively within SMP negotiations, excluding user interaction
                const uint32_t smp_events = device->smp_events;
                if( 0 == smp_events ) {
                    DBG_PRINT("BTAdapter::smp_timeoutfunc(dev_id %d): SMP Timeout: Pairing-Failed %u: %s", dev_id, smp_events, device->toString().c_str());
                    failed_devices.push_back(device);
                } else {
                    DBG_PRINT("BTAdapter::smp_timeoutfunc(dev_id %d): SMP Timeout: Ignore-2 %u -> 0: %s", dev_id, smp_events, device->toString().c_str());
                    device->smp_events = 0;
                }
            } else {
                const uint32_t smp_events = device->smp_events;
                DBG_PRINT("BTAdapter::smp_timeoutfunc(dev_id %d): SMP Timeout: Ignore-1 %u: %s", dev_id, smp_events, device->toString().c_str());
            }
        });
    }
    jau::for_each_fidelity(failed_devices, [&](BTDeviceRef& device) {
        const bool smp_auto = device->isConnSecurityAutoEnabled();
        IRQ_PRINT("BTAdapter(dev_id %d): SMP Timeout: Start: smp_auto %d, %s", dev_id, smp_auto, device->toString().c_str());
        const SMPPairFailedMsg msg(SMPPairFailedMsg::ReasonCode::UNSPECIFIED_REASON);
        const HCIACLData::l2cap_frame source(device->getConnectionHandle(),
                HCIACLData::l2cap_frame::PBFlag::START_NON_AUTOFLUSH_HOST, 0 /* bc_flag */,
                L2CAP_CID::SMP, L2CAP_PSM::UNDEFINED, 0 /* len */);
        // BTAdapter::sendDevicePairingState() will delete device-key if server and issue disconnect if !smp_auto
        device->hciSMPMsgCallback(device, msg, source);
        DBG_PRINT("BTAdapter::smp_timeoutfunc(dev_id %d): SMP Timeout: Done: smp_auto %d, %s", dev_id, smp_auto, device->toString().c_str());
    });
    return timer.shall_stop() ? 0_s : SMP_NEXT_EVENT_TIMEOUT_MS; // keep going until BTAdapter closes
}

bool BTAdapter::mgmtEvDeviceConnectedHCI(const MgmtEvent& e) noexcept {
    const MgmtEvtDeviceConnected &event = *static_cast<const MgmtEvtDeviceConnected *>(&e);
    EInfoReport ad_report;
    {
        ad_report.setSource(EInfoReport::Source::EIR, false);
        ad_report.setTimestamp(event.getTimestamp());
        ad_report.setAddressType(event.getAddressType());
        ad_report.setAddress( event.getAddress() );
        ad_report.read_data(event.getData(), event.getDataSize());
    }

    int new_connect = 0;
    bool device_discovered = true;
    bool slave_unpair = false;
    BTDeviceRef device = findConnectedDevice(event.getAddress(), event.getAddressType());
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
            slave_unpair = BTRole::Slave == getRole();
            // Re device_discovered: Even though device was not in discoveredDevices list above,
            // it once was hence the device is in the shared device list.
            // Note that discoveredDevices is flushed w/ startDiscovery()!
        }
    }
    if( nullptr == device ) {
        // (new_connect = 3) a whitelist auto-connect w/o previous discovery, or
        // (new_connect = 4) we are a peripheral being connected by a remote client
        device_discovered = false;
        device = BTDevice::make_shared(*this, ad_report);
        addDiscoveredDevice(device);
        addSharedDevice(device);
        new_connect = BTRole::Master == getRole() ? 3 : 4;
        slave_unpair = BTRole::Slave == getRole();
    }
    bool has_smp_keys;
    if( BTRole::Slave == getRole() ) {
        has_smp_keys = nullptr != findSMPKeyBin( device->getAddressAndType() ); // PERIPHERAL_ADAPTER_MANAGES_SMP_KEYS
    } else {
        has_smp_keys = false;
    }

    DBG_PRINT("BTAdapter:hci:DeviceConnected(dev_id %d): state[role %s, new %d, discovered %d, unpair %d, has_keys %d], %s: %s",
            dev_id, to_string(getRole()).c_str(), new_connect, device_discovered, slave_unpair, has_smp_keys,
            e.toString().c_str(), ad_report.toString().c_str());

    if( slave_unpair ) {
        /**
         * Without unpair in SC mode (or key pre-pairing), the peripheral fails the DHKey Check.
         */
        if( !has_smp_keys ) {
            // No pre-pairing -> unpair
            HCIStatusCode  res = mgmt->unpairDevice(dev_id, device->getAddressAndType(), false /* disconnect */);
            if( HCIStatusCode::SUCCESS != res && HCIStatusCode::NOT_PAIRED != res ) {
                WARN_PRINT("(dev_id %d, new_connect %d): Unpair device failed %s of %s",
                        dev_id, new_connect, to_string(res).c_str(), device->getAddressAndType().toString().c_str());
            }
        }
    }

    const SMPIOCapability io_cap_conn = mgmt->getIOCapability(dev_id);

    EIRDataType updateMask = device->update(ad_report);
    if( 0 == new_connect ) {
        WARN_PRINT("(dev_id %d, already connected, updated %s): %s, handle %s -> %s,\n    %s,\n    -> %s",
            dev_id, to_string(updateMask).c_str(), event.toString().c_str(),
            jau::to_hexstring(device->getConnectionHandle()).c_str(), jau::to_hexstring(event.getHCIHandle()).c_str(),
            ad_report.toString().c_str(),
            device->toString().c_str());
    } else {
        addConnectedDevice(device); // track device, if not done yet
        COND_PRINT(debug_event, "BTAdapter::hci:DeviceConnected(dev_id %d, new_connect %d, updated %s): %s, handle %s -> %s,\n    %s,\n    -> %s",
            dev_id, new_connect, to_string(updateMask).c_str(), event.toString().c_str(),
            jau::to_hexstring(device->getConnectionHandle()).c_str(), jau::to_hexstring(event.getHCIHandle()).c_str(),
            ad_report.toString().c_str(),
            device->toString().c_str());
    }

    if( BTRole::Slave == getRole() && !has_smp_keys ) {
        // Skipped if has_smp_key,
        // where BTDevice::uploadKeys() -> BTDevice::setSMPKeyBin() was performed at disconnected tuning the security setup.
        device->setConnSecurity(sec_level_server, io_cap_server);
    }
    device->notifyConnected(device, event.getHCIHandle(), io_cap_conn);

    if( device->isConnSecurityAutoEnabled() ) {
        new_connect = 0; // disable deviceConnected() events for BTRole::Master for SMP-Auto
    }

    int i=0;
    jau::for_each_fidelity(statusListenerList, [&](StatusListenerPair &p) {
        try {
            if( p.match(device) ) {
                if( EIRDataType::NONE != updateMask ) {
                    p.listener->deviceUpdated(device, updateMask, ad_report.getTimestamp());
                }
                if( 0 < new_connect ) {
                    p.listener->deviceConnected(device, device_discovered, event.getTimestamp());
                }
            }
        } catch (std::exception &except) {
            ERR_PRINT("BTAdapter::hci:DeviceConnected-CBs %d/%zd: %s of %s: Caught exception %s",
                    i+1, statusListenerList.size(),
                    p.listener->toString().c_str(), device->toString().c_str(), except.what());
        }
        i++;
    });
    if( BTRole::Slave == getRole() ) {
        // For BTRole::Master, BlueZ Kernel does request LE_Features already
        // Hence we have to trigger sending LE_Features for BTRole::Slave ourselves
        //
        // This is mandatory to trigger BTDevice::notifyLEFeatures() via our mgmtEvHCILERemoteUserFeaturesHCI()
        // to proceed w/ post-connection and eventually issue deviceRead().

        // Replied with: Command Status 0x0f (timeout) and Status 0x0c (disallowed),
        // despite core-spec states valid for both master and slave.
        // hci.le_read_remote_features(event.getHCIHandle(), device->getAddressAndType());

        // Hence .. induce it right after connect
        device->notifyLEFeatures(device, LE_Features::LE_Encryption);
    }
    return true;
}

bool BTAdapter::mgmtEvConnectFailedHCI(const MgmtEvent& e) noexcept {
    const MgmtEvtDeviceConnectFailed &event = *static_cast<const MgmtEvtDeviceConnectFailed *>(&e);

    BTDeviceRef device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr != device ) {
        const uint16_t handle = device->getConnectionHandle();
        DBG_PRINT("BTAdapter::hci:ConnectFailed(dev_id %d): %s, handle %s -> zero,\n    -> %s",
            dev_id, event.toString().c_str(), jau::to_hexstring(handle).c_str(),
            device->toString().c_str());

        unlockConnect(*device);
        device->notifyDisconnected();
        removeConnectedDevice(*device);

        if( !device->isConnSecurityAutoEnabled() ) {
            int i=0;
            jau::for_each_fidelity(statusListenerList, [&](StatusListenerPair &p) {
                try {
                    if( p.match(device) ) {
                        p.listener->deviceDisconnected(device, event.getHCIStatus(), handle, event.getTimestamp());
                    }
                } catch (std::exception &except) {
                    ERR_PRINT("BTAdapter::hci:DeviceDisconnected-CBs %d/%zd: %s of %s: Caught exception %s",
                            i+1, statusListenerList.size(),
                            p.listener->toString().c_str(), device->toString().c_str(), except.what());
                }
                i++;
            });
            device->clearData();
            removeDiscoveredDevice(device->addressAndType); // ensure device will cause a deviceFound event after disconnect
        }
    } else {
        WORDY_PRINT("BTAdapter::hci:DeviceDisconnected(dev_id %d): Device not tracked: %s",
            dev_id, event.toString().c_str());
    }
    return true;
}

bool BTAdapter::mgmtEvHCILERemoteUserFeaturesHCI(const MgmtEvent& e) noexcept {
    const MgmtEvtHCILERemoteFeatures &event = *static_cast<const MgmtEvtHCILERemoteFeatures *>(&e);

    BTDeviceRef device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr != device ) {
        COND_PRINT(debug_event, "BTAdapter::hci:LERemoteUserFeatures(dev_id %d): %s, %s",
            dev_id, event.toString().c_str(), device->toString().c_str());

        if( BTRole::Master == getRole() ) {
            const DiscoveryPolicy policy = discovery_policy;
            if( DiscoveryPolicy::AUTO_OFF == policy ) {
                if constexpr ( SCAN_DISABLED_POST_CONNECT ) {
                    updateDeviceDiscoveringState(ScanType::LE, false /* eventEnabled */);
                } else {
                    std::thread bg(&BTAdapter::stopDiscoveryImpl, this, false /* forceDiscoveringEvent */, true /* temporary */); // @suppress("Invalid arguments")
                    bg.detach();
                }
            } else if( DiscoveryPolicy::ALWAYS_ON == policy ) {
                if constexpr ( SCAN_DISABLED_POST_CONNECT ) {
                    updateDeviceDiscoveringState(ScanType::LE, false /* eventEnabled */);
                } else {
                    discovery_service.start();
                }
            } else {
                addDevicePausingDiscovery(device);
            }
        }
        if( HCIStatusCode::SUCCESS == event.getHCIStatus() ) {
            device->notifyLEFeatures(device, event.getFeatures());
            // Performs optional SMP pairing, then one of
            // - sendDeviceReady()
            // - disconnect() .. eventually
        } // else: disconnect will occur
    } else {
        WORDY_PRINT("BTAdapter::hci:LERemoteUserFeatures(dev_id %d): Device not tracked: %s",
            dev_id, event.toString().c_str());
    }
    return true;
}

bool BTAdapter::mgmtEvHCILEPhyUpdateCompleteHCI(const MgmtEvent& e) noexcept {
    const MgmtEvtHCILEPhyUpdateComplete &event = *static_cast<const MgmtEvtHCILEPhyUpdateComplete *>(&e);

    BTDeviceRef device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr != device ) {
        COND_PRINT(debug_event, "BTAdapter::hci:LEPhyUpdateComplete(dev_id %d): %s, %s",
            dev_id, event.toString().c_str(), device->toString().c_str());

        device->notifyLEPhyUpdateComplete(event.getHCIStatus(), event.getTx(), event.getRx());
    } else {
        WORDY_PRINT("BTAdapter::hci:LEPhyUpdateComplete(dev_id %d): Device not tracked: %s",
            dev_id, event.toString().c_str());
    }
    return true;
}

bool BTAdapter::mgmtEvDeviceDisconnectedHCI(const MgmtEvent& e) noexcept {
    const MgmtEvtDeviceDisconnected &event = *static_cast<const MgmtEvtDeviceDisconnected *>(&e);

    BTDeviceRef device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr != device ) {
        if( device->getConnectionHandle() != event.getHCIHandle() ) {
            WORDY_PRINT("BTAdapter::hci:DeviceDisconnected(dev_id %d): ConnHandle mismatch %s\n    -> %s",
                dev_id, event.toString().c_str(), device->toString().c_str());
            return true;
        }
        DBG_PRINT("BTAdapter::hci:DeviceDisconnected(dev_id %d): %s, handle %s -> zero,\n    -> %s",
            dev_id, event.toString().c_str(), jau::to_hexstring(event.getHCIHandle()).c_str(),
            device->toString().c_str());

        unlockConnect(*device);
        device->notifyDisconnected(); // -> unpair()
        removeConnectedDevice(*device);
        gattServerData = nullptr;

        if( !device->isConnSecurityAutoEnabled() ) {
            int i=0;
            jau::for_each_fidelity(statusListenerList, [&](StatusListenerPair &p) {
                try {
                    if( p.match(device) ) {
                        p.listener->deviceDisconnected(device, event.getHCIReason(), event.getHCIHandle(), event.getTimestamp());
                    }
                } catch (std::exception &except) {
                    ERR_PRINT("BTAdapter::hci:DeviceDisconnected-CBs %d/%zd: %s of %s: Caught exception %s",
                            i+1, statusListenerList.size(),
                            p.listener->toString().c_str(), device->toString().c_str(), except.what());
                }
                i++;
            });
            device->clearData();
            removeDiscoveredDevice(device->addressAndType); // ensure device will cause a deviceFound event after disconnect
        }
        if( BTRole::Slave == getRole() ) {
            // PERIPHERAL_ADAPTER_MANAGES_SMP_KEYS
            if( HCIStatusCode::AUTHENTICATION_FAILURE == event.getHCIReason() ||
                HCIStatusCode::PAIRING_WITH_UNIT_KEY_NOT_SUPPORTED == event.getHCIReason() )
            {
                // Pairing failed on the remote client side
                removeSMPKeyBin(device->getAddressAndType(), true /* remove_file */);
            } else {
                SMPKeyBinRef key = findSMPKeyBin(device->getAddressAndType());
                if( nullptr != key ) {
                    HCIStatusCode res;
                    #if 0
                        res = getManager().unpairDevice(dev_id, device->getAddressAndType(), true /* disconnect */);
                        if( HCIStatusCode::SUCCESS != res && HCIStatusCode::NOT_PAIRED != res ) {
                            WARN_PRINT("(dev_id %d): Unpair device failed %s of %s",
                                    dev_id, to_string(res).c_str(), device->getAddressAndType().toString().c_str());
                        }
                        res = device->uploadKeys(*key, BTSecurityLevel::NONE);
                    #else
                        res = device->uploadKeys(*key, BTSecurityLevel::NONE);
                    #endif
                    if( HCIStatusCode::SUCCESS != res ) {
                        WARN_PRINT("(dev_id %d): Upload SMPKeyBin failed %s, %s (removing file)",
                                dev_id, to_string(res).c_str(), key->toString().c_str());
                        removeSMPKeyBin(device->getAddressAndType(), true /* remove_file */);
                    }
                }
            }
        }
        removeDevicePausingDiscovery(*device);
    } else {
        DBG_PRINT("BTAdapter::hci:DeviceDisconnected(dev_id %d): Device not connected: %s",
            dev_id, event.toString().c_str());
        if( _print_device_lists || jau::environment::get().verbose ) {
            printDeviceLists();
        }
        device = findDevicePausingDiscovery(event.getAddress(), event.getAddressType());
        if( nullptr != device ) {
            removeDevicePausingDiscovery(*device);
        }
    }
    return true;
}

// Local BTRole::Slave
bool BTAdapter::mgmtEvLELTKReqEventHCI(const MgmtEvent& e) noexcept {
    const MgmtEvtHCILELTKReq &event = *static_cast<const MgmtEvtHCILELTKReq *>(&e);

    BTDeviceRef device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr != device ) {
        // BT Core Spec v5.2: Vol 4, Part E HCI: 7.7.65.5 LE Long Term Key Request event
        device->updatePairingState(device, e, HCIStatusCode::SUCCESS, SMPPairingState::COMPLETED);
    } else {
        WORDY_PRINT("BTAdapter::hci:LE_LTK_Request(dev_id %d): Device not tracked: %s",
            dev_id, event.toString().c_str());
    }
    return true;
}
bool BTAdapter::mgmtEvLELTKReplyAckCmdHCI(const MgmtEvent& e) noexcept {
    const MgmtEvtHCILELTKReplyAckCmd &event = *static_cast<const MgmtEvtHCILELTKReplyAckCmd *>(&e);

    BTDeviceRef device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr != device ) {
        // BT Core Spec v5.2: Vol 4, Part E HCI: 7.8.25 LE Long Term Key Request Reply command
        device->updatePairingState(device, e, HCIStatusCode::SUCCESS, SMPPairingState::COMPLETED);
    } else {
        WORDY_PRINT("BTAdapter::hci:LE_LTK_REPLY_ACK(dev_id %d): Device not tracked: %s",
            dev_id, event.toString().c_str());
    }
    return true;
}
bool BTAdapter::mgmtEvLELTKReplyRejCmdHCI(const MgmtEvent& e) noexcept {
    const MgmtEvtHCILELTKReplyRejCmd &event = *static_cast<const MgmtEvtHCILELTKReplyRejCmd *>(&e);

    BTDeviceRef device = findConnectedDevice(event.getAddress(), event.getAddressType());
    DBG_PRINT("BTAdapter::hci:LE_LTK_REPLY_REJ(dev_id %d): Ignored: %s (tracked %d)",
            dev_id, event.toString().c_str(), (nullptr!=device));
    return true;
}

// Local BTRole::Master
bool BTAdapter::mgmtEvLEEnableEncryptionCmdHCI(const MgmtEvent& e) noexcept {
    const MgmtEvtHCILEEnableEncryptionCmd &event = *static_cast<const MgmtEvtHCILEEnableEncryptionCmd *>(&e);

    BTDeviceRef device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr != device ) {
        // BT Core Spec v5.2: Vol 4, Part E HCI: 7.8.24 LE Enable Encryption command
        device->updatePairingState(device, e, HCIStatusCode::SUCCESS, SMPPairingState::COMPLETED);
    } else {
        WORDY_PRINT("BTAdapter::hci:LE_ENABLE_ENC(dev_id %d): Device not tracked: %s",
            dev_id, event.toString().c_str());
    }
    return true;
}
// On BTRole::Master (reply to MgmtEvtHCILEEnableEncryptionCmd) and BTRole::Slave
bool BTAdapter::mgmtEvHCIEncryptionChangedHCI(const MgmtEvent& e) noexcept {
    const MgmtEvtHCIEncryptionChanged &event = *static_cast<const MgmtEvtHCIEncryptionChanged *>(&e);

    BTDeviceRef device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr != device ) {
        // BT Core Spec v5.2: Vol 4, Part E HCI: 7.7.8 HCIEventType::ENCRYPT_CHANGE
        const HCIStatusCode evtStatus = event.getHCIStatus();
        const bool ok = HCIStatusCode::SUCCESS == evtStatus && 0 != event.getEncEnabled();
        const SMPPairingState pstate = ok ? SMPPairingState::COMPLETED : SMPPairingState::FAILED;
        device->updatePairingState(device, e, evtStatus, pstate);
    } else {
        WORDY_PRINT("BTAdapter::hci:ENC_CHANGED(dev_id %d): Device not tracked: %s",
            dev_id, event.toString().c_str());
    }
    return true;
}
// On BTRole::Master (reply to MgmtEvtHCILEEnableEncryptionCmd) and BTRole::Slave
bool BTAdapter::mgmtEvHCIEncryptionKeyRefreshCompleteHCI(const MgmtEvent& e) noexcept {
    const MgmtEvtHCIEncryptionKeyRefreshComplete &event = *static_cast<const MgmtEvtHCIEncryptionKeyRefreshComplete *>(&e);

    BTDeviceRef device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr != device ) {
        // BT Core Spec v5.2: Vol 4, Part E HCI: 7.7.39 HCIEventType::ENCRYPT_KEY_REFRESH_COMPLETE
        const HCIStatusCode evtStatus = event.getHCIStatus();
        const bool ok = HCIStatusCode::SUCCESS == evtStatus;
        const SMPPairingState pstate = ok ? SMPPairingState::COMPLETED : SMPPairingState::FAILED;
        device->updatePairingState(device, e, evtStatus, pstate);
    } else {
        WORDY_PRINT("BTAdapter::hci:ENC_KEY_REFRESH_COMPLETE(dev_id %d): Device not tracked: %s",
            dev_id, event.toString().c_str());
    }
    return true;
}

bool BTAdapter::mgmtEvPairDeviceCompleteMgmt(const MgmtEvent& e) noexcept {
    const MgmtEvtPairDeviceComplete &event = *static_cast<const MgmtEvtPairDeviceComplete *>(&e);

    BTDeviceRef device = findConnectedDevice(event.getAddress(), event.getAddressType());
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
    BTDeviceRef device = findConnectedDevice(ltk_info.address, ltk_info.address_type);
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

bool BTAdapter::mgmtEvNewLinkKeyMgmt(const MgmtEvent& e) noexcept {
    const MgmtEvtNewLinkKey& event = *static_cast<const MgmtEvtNewLinkKey *>(&e);
    const MgmtLinkKeyInfo& lk_info = event.getLinkKey();
    // lk_info.address_type might be wrongly reported by mgmt, i.e. BDADDR_BREDR, use any.
    BTDeviceRef device = findConnectedDevice(lk_info.address, BDAddressType::BDADDR_UNDEFINED);
    if( nullptr != device ) {
        const bool ok = lk_info.key_type != MgmtLinkKeyType::NONE;
        if( ok ) {
            device->updatePairingState(device, e, HCIStatusCode::SUCCESS, SMPPairingState::COMPLETED);
        } else {
            WORDY_PRINT("BTAdapter::mgmt:NewLinkKey(dev_id %d): Invalid LK: %s",
                dev_id, event.toString().c_str());
        }
    } else {
        WORDY_PRINT("BTAdapter::mgmt:NewLinkKey(dev_id %d): Device not tracked: %s",
            dev_id, event.toString().c_str());
    }
    return true;
}

bool BTAdapter::mgmtEvDeviceFoundHCI(const MgmtEvent& e) noexcept {
    // COND_PRINT(debug_event, "BTAdapter:hci:DeviceFound(dev_id %d): %s", dev_id, e.toString().c_str());
    const MgmtEvtDeviceFound &deviceFoundEvent = *static_cast<const MgmtEvtDeviceFound *>(&e);

    const EInfoReport* eir = deviceFoundEvent.getEIR();
    if( nullptr == eir ) {
        // Sourced from Linux Mgmt, which we don't support
        ABORT("BTAdapter:hci:DeviceFound: Not sourced from LE_ADVERTISING_REPORT: %s", deviceFoundEvent.toString().c_str());
    } // else: Sourced from HCIHandler via LE_ADVERTISING_REPORT (default!)

    /**
     * + ------+-----------+------------+----------+----------+-------------------------------------------+
     * | #     | connected | discovered | shared   | update   |
     * +-------+-----------+------------+----------+----------+-------------------------------------------+
     * | 1.0   | true      | any        | any      | ignored  | Already connected device -> Drop(1)
     * | 1.1   | false     | false      | false    | ignored  | New undiscovered/unshared -> deviceFound(..)
     * | 1.2   | false     | false      | true     | ignored  | Undiscovered but shared -> deviceFound(..) [deviceUpdated(..)]
     * | 2.1.1 | false     | true       | false    | name     | Discovered but unshared, name changed -> deviceFound(..)
     * | 2.1.2 | false     | true       | false    | !name    | Discovered but unshared, no name change -> Drop(2)
     * | 2.2.1 | false     | true       | true     | any      | Discovered and shared, updated -> deviceUpdated(..)
     * | 2.2.2 | false     | true       | true     | none     | Discovered and shared, not-updated -> Drop(3)
     * +-------+-----------+------------+----------+----------+-------------------------------------------+
     */
    BTDeviceRef dev_connected = findConnectedDevice(eir->getAddress(), eir->getAddressType());
    BTDeviceRef dev_discovered = findDiscoveredDevice(eir->getAddress(), eir->getAddressType());
    BTDeviceRef dev_shared = findSharedDevice(eir->getAddress(), eir->getAddressType());
    if( nullptr != dev_connected ) {
        // already connected device shall be suppressed
        DBG_PRINT("BTAdapter:hci:DeviceFound(1.0, dev_id %d): Discovered but already connected %s [discovered %d, shared %d] -> Drop(1) %s",
                  dev_id, dev_connected->getAddressAndType().toString().c_str(),
                  nullptr != dev_discovered, nullptr != dev_shared, eir->toString().c_str());
    } else if( nullptr == dev_discovered ) { // nullptr == dev_connected && nullptr == dev_discovered
        if( nullptr == dev_shared ) {
            //
            // All new discovered device
            //
            dev_shared = BTDevice::make_shared(*this, *eir);
            addDiscoveredDevice(dev_shared);
            addSharedDevice(dev_shared);
            DBG_PRINT("BTAdapter:hci:DeviceFound(1.1, dev_id %d): New undiscovered/unshared %s -> deviceFound(..) %s",
                    dev_id, dev_shared->getAddressAndType().toString().c_str(), eir->toString().c_str());

            {
                const HCIStatusCode res = mgmt->unpairDevice(dev_id, dev_shared->getAddressAndType(), false /* disconnect */);
                if( HCIStatusCode::SUCCESS != res && HCIStatusCode::NOT_PAIRED != res ) {
                    WARN_PRINT("(dev_id %d): Unpair device failed %s of %s",
                            dev_id, to_string(res).c_str(), dev_shared->getAddressAndType().toString().c_str());
                }
            }
            int i=0;
            bool device_used = false;
            jau::for_each_fidelity(statusListenerList, [&](StatusListenerPair &p) {
                try {
                    if( p.match(dev_shared) ) {
                        device_used = p.listener->deviceFound(dev_shared, eir->getTimestamp()) || device_used;
                    }
                } catch (std::exception &except) {
                    ERR_PRINT("BTAdapter:hci:DeviceFound-CBs %d/%zd: %s of %s: Caught exception %s",
                            i+1, statusListenerList.size(),
                            p.listener->toString().c_str(), dev_shared->toString().c_str(), except.what());
                }
                i++;
            });
            if( !device_used ) {
                // keep to avoid duplicate finds: removeDiscoveredDevice(dev_discovered->addressAndType);
                // and still allowing usage, as connecting will re-add to shared list
                removeSharedDevice(*dev_shared); // pending dtor if discovered is flushed
            }
        } else { // nullptr != dev_shared
            //
            // Active shared device, but flushed from discovered devices
            // - update device
            // - issue deviceFound(..),  allowing receivers to recognize the re-discovered device
            // - issue deviceUpdate(..), if at least one deviceFound(..) returned true and data has changed, allowing receivers to act upon
            // - removeSharedDevice(..), if non deviceFound(..) returned true
            //
            EIRDataType updateMask = dev_shared->update(*eir);
            addDiscoveredDevice(dev_shared); // re-add to discovered devices!
            dev_shared->ts_last_discovery = eir->getTimestamp();
            DBG_PRINT("BTAdapter:hci:DeviceFound(1.2, dev_id %d): Undiscovered but shared %s -> deviceFound(..) [deviceUpdated(..)] %s",
                    dev_id, dev_shared->getAddressAndType().toString().c_str(), eir->toString().c_str());

            {
                HCIStatusCode res = dev_shared->unpair();
                if( HCIStatusCode::SUCCESS != res && HCIStatusCode::NOT_PAIRED != res ) {
                    WARN_PRINT("(dev_id %d): Unpair device failed: %s, %s",
                                dev_id, to_string(res).c_str(), dev_shared->getAddressAndType().toString().c_str());
                }
            }
            int i=0;
            bool device_used = false;
            jau::for_each_fidelity(statusListenerList, [&](StatusListenerPair &p) {
                try {
                    if( p.match(dev_shared) ) {
                        device_used = p.listener->deviceFound(dev_shared, eir->getTimestamp()) || device_used;
                    }
                } catch (std::exception &except) {
                    ERR_PRINT("BTAdapter:hci:DeviceFound: %d/%zd: %s of %s: Caught exception %s",
                            i+1, statusListenerList.size(),
                            p.listener->toString().c_str(), dev_shared->toString().c_str(), except.what());
                }
                i++;
            });
            if( !device_used ) {
                // keep to avoid duplicate finds: removeDiscoveredDevice(dev->addressAndType);
                // and still allowing usage, as connecting will re-add to shared list
                removeSharedDevice(*dev_shared); // pending dtor until discovered is flushed
            } else if( EIRDataType::NONE != updateMask ) {
                sendDeviceUpdated("SharedDeviceFound", dev_shared, eir->getTimestamp(), updateMask);
            }
        }
    } else { // nullptr == dev_connected && nullptr != dev_discovered
        //
        // Already discovered device
        //
        const EIRDataType updateMask = dev_discovered->update(*eir);
        dev_discovered->ts_last_discovery = eir->getTimestamp();
        if( nullptr == dev_shared ) {
            //
            // Discovered but not a shared device,
            // i.e. all user deviceFound(..) returned false - no interest/rejected.
            //
            if( EIRDataType::NONE != ( updateMask & EIRDataType::NAME ) ) {
                // Name got updated, send out deviceFound(..) again
                DBG_PRINT("BTAdapter:hci:DeviceFound(2.1.1, dev_id %d): Discovered but unshared %s, name changed %s -> deviceFound(..) %s",
                        dev_id, dev_discovered->getAddressAndType().toString().c_str(),
                        direct_bt::to_string(updateMask).c_str(), eir->toString().c_str());
                addSharedDevice(dev_discovered); // re-add to shared devices!
                int i=0;
                bool device_used = false;
                jau::for_each_fidelity(statusListenerList, [&](StatusListenerPair &p) {
                    try {
                        if( p.match(dev_discovered) ) {
                            device_used = p.listener->deviceFound(dev_discovered, eir->getTimestamp()) || device_used;
                        }
                    } catch (std::exception &except) {
                        ERR_PRINT("BTAdapter:hci:DeviceFound: %d/%zd: %s of %s: Caught exception %s",
                                i+1, statusListenerList.size(),
                                p.listener->toString().c_str(), dev_discovered->toString().c_str(), except.what());
                    }
                    i++;
                });
                if( !device_used ) {
                    // keep to avoid duplicate finds: removeDiscoveredDevice(dev_discovered->addressAndType);
                    // and still allowing usage, as connecting will re-add to shared list
                    removeSharedDevice(*dev_discovered); // pending dtor if discovered is flushed
                }
            } else {
                // Drop: NAME didn't change
                COND_PRINT(debug_event, "BTAdapter:hci:DeviceFound(2.1.2, dev_id %d): Discovered but unshared %s, no name change -> Drop(2) %s",
                        dev_id, dev_discovered->getAddressAndType().toString().c_str(), eir->toString().c_str());
            }
        } else { // nullptr != dev_shared
            //
            // Discovered and shared device,
            // i.e. at least one deviceFound(..) returned true - interest/picked.
            //
            if( EIRDataType::NONE != updateMask ) {
                COND_PRINT(debug_event, "BTAdapter:hci:DeviceFound(2.2.1, dev_id %d): Discovered and shared %s, updated %s -> deviceUpdated(..) %s",
                        dev_id, dev_shared->getAddressAndType().toString().c_str(),
                        direct_bt::to_string(updateMask).c_str(), eir->toString().c_str());
                sendDeviceUpdated("DiscoveredDeviceFound", dev_shared, eir->getTimestamp(), updateMask);
            } else {
                // Drop: No update
                COND_PRINT(debug_event, "BTAdapter:hci:DeviceFound(2.2.2, dev_id %d): Discovered and shared %s, not-updated -> Drop(3) %s",
                        dev_id, dev_shared->getAddressAndType().toString().c_str(), eir->toString().c_str());
            }
        }
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

    BTDeviceRef device = findConnectedDevice(event.getAddress(), event.getAddressType());
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

    BTDeviceRef device = findConnectedDevice(event.getAddress(), event.getAddressType());
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

    BTDeviceRef device = findConnectedDevice(event.getAddress(), event.getAddressType());
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
    BTDeviceRef device = findConnectedDevice(addressAndType.address, addressAndType.type);
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

void BTAdapter::sendDevicePairingState(BTDeviceRef device, const SMPPairingState state, const PairingMode mode, uint64_t timestamp) noexcept
{
    if( BTRole::Slave == getRole() ) {
        // PERIPHERAL_ADAPTER_MANAGES_SMP_KEYS
        if( SMPPairingState::COMPLETED == state ) {
            // Pairing completed
            if( PairingMode::PRE_PAIRED != mode ) {
                // newly paired -> store keys
                SMPKeyBin key = SMPKeyBin::create(*device);
                if( key.isValid() ) {
                    addSMPKeyBin( std::make_shared<SMPKeyBin>(key), true /* write_file */ );
                }
            } else {
                // pre-paired, refresh PairingData of BTDevice (perhaps a new instance)
                const SMPKeyBinRef key = findSMPKeyBin( device->getAddressAndType() );
                if( nullptr != key ) {
                    bool res = device->setSMPKeyBin(*key);
                    if( !res ) {
                        WARN_PRINT("(dev_id %d): device::setSMPKeyBin() failed %d, %s", res, key->toString().c_str());
                    }
                }
            }
        } else if( SMPPairingState::FAILED == state ) {
            // Pairing failed on this server side
            removeSMPKeyBin(device->getAddressAndType(), true /* remove_file */);
        }
    }
    int i=0;
    jau::for_each_fidelity(statusListenerList, [&](StatusListenerPair &p) {
        try {
            if( p.match(device) ) {
                p.listener->devicePairingState(device, state, mode, timestamp);
            }
        } catch (std::exception &except) {
            ERR_PRINT("BTAdapter::sendDevicePairingState: %d/%zd: %s of %s: Caught exception %s",
                    i+1, statusListenerList.size(),
                    p.listener->toString().c_str(), device->toString().c_str(), except.what());
        }
        i++;
    });
    if( SMPPairingState::FAILED == state && !device->isConnSecurityAutoEnabled() ) {
        // Don't rely on receiving a disconnect
        std::thread dc(&BTDevice::disconnect, device.get(), HCIStatusCode::AUTHENTICATION_FAILURE);
        dc.detach();
    }
}

void BTAdapter::notifyPairingStageDone(BTDeviceRef device, uint64_t timestamp) noexcept {
    if( DiscoveryPolicy::PAUSE_CONNECTED_UNTIL_PAIRED == discovery_policy ) {
        removeDevicePausingDiscovery(*device);
    }
    (void)timestamp;
}

void BTAdapter::sendDeviceReady(BTDeviceRef device, uint64_t timestamp) noexcept {
    if( DiscoveryPolicy::PAUSE_CONNECTED_UNTIL_READY == discovery_policy ) {
        removeDevicePausingDiscovery(*device);
    }
    int i=0;
    jau::for_each_fidelity(statusListenerList, [&](StatusListenerPair &p) {
        try {
            // Only issue if valid && received connected confirmation (HCI) && not have called disconnect yet.
            if( device->isValidInstance() && device->getConnected() && device->allowDisconnect ) {
                if( p.match(device) ) {
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
