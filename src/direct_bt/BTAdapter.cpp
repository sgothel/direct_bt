/*
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2020-2026 Gothel Software e.K.
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
#include <exception>
#include <string>
#include <memory>
#include <cstdint>
#include <cstdio>

#include <random>

#include <jau/debug.hpp>

#include <jau/basic_algos.hpp>

#include "BTAdapter.hpp"
#include "BTManager.hpp"
#include "DBTConst.hpp"
#include "jau/cpp_lang_util.hpp"

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
    return jau_format_string("Unknown DiscoveryPolicy %#x", *v);
}

BTDeviceRef BTAdapter::findDevice(HCIHandler& hci, device_list_t & devices, const EUI48 & address, const BDAddressType addressType) noexcept {
    BDAddressAndType rpa(address, addressType);
    const jau::nsize_t size = devices.size();
    for (jau::nsize_t i = 0; i < size; ++i) {
        BTDeviceRef & e = devices[i];
        if ( nullptr != e &&
             (
               ( address == e->getAddressAndType().address &&
                 ( addressType == e->getAddressAndType().type || addressType == BDAddressType::BDADDR_UNDEFINED )
               ) ||
               ( address == e->getVisibleAddressAndType().address &&
                 ( addressType == e->getVisibleAddressAndType().type || addressType == BDAddressType::BDADDR_UNDEFINED )
               )
             )
           )
        {
            if( !rpa.isIdentityAddress() ) {
                e->updateVisibleAddress(rpa);
                hci.setResolvHCIConnectionAddr(rpa, e->getAddressAndType());
            }
            return e;
        }
    }
    if( !rpa.isIdentityAddress() ) {
        for (jau::nsize_t i = 0; i < size; ++i) {
            BTDeviceRef & e = devices[i];
            if ( nullptr != e && e->matches_irk(rpa) ) {
                e->updateVisibleAddress(rpa);
                hci.setResolvHCIConnectionAddr(rpa, e->getAddressAndType());
                return e;
            }
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
        } else if ( ( address == e->getAddressAndType().address &&
                      ( addressType == e->getAddressAndType().type || addressType == BDAddressType::BDADDR_UNDEFINED )
                    ) ||
                    ( address == e->getVisibleAddressAndType().address &&
                      ( addressType == e->getVisibleAddressAndType().type || addressType == BDAddressType::BDADDR_UNDEFINED )
                    )
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

bool BTAdapter::addDevicePausingDiscovery(const BTDeviceRef & device) {
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
        bool done = false;
        for (auto it = pausing_discovery_devices.begin(); it != end && !done; ) {
            std::weak_ptr<BTDevice> & w = *it;
            BTDeviceRef e = w.lock();
            try {
                if( nullptr == e ) {
                    pausing_discovery_devices.erase(it); // erase and move it to next element
                    done = true;
                } else if ( device == *e ) {
                    pausing_discovery_devices.erase(it);
                    removed_last = 0 == pausing_discovery_devices.size();
                    done = true;
                } else {
                    ++it; // move it to next element
                }
            } catch( ... ) {
                jau::fput_exception(stderr, std::current_exception(), E_FILE_LINE);
                jau_ERR_PRINT3("Exception caught while removing paused discovery device for %s", toString());
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

bool BTAdapter::addConnectedDevice(const BTDeviceRef & device) {
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
            try {
                connectedDevices.erase(it);
            } catch (...) {
                jau::fput_exception(stderr, std::current_exception(), E_FILE_LINE);
                jau_ERR_PRINT3("Exception caught while removing device: %s", (*it)->toString());
                return false;
            }
            return true;
        }
    }
    return false;
}

BTAdapter::size_type BTAdapter::disconnectAllDevices(const HCIStatusCode reason) noexcept {
    device_list_t devices;
    {
        jau::sc_atomic_critical sync(sync_data); // SC-DRF via atomic acquire & release
        try {
            devices = connectedDevices; // copy!
        } catch( ... ) {
            jau::fput_exception(stderr, std::current_exception(), E_FILE_LINE);
            jau_ERR_PRINT3("Exception caught while disconnecting all devices for %s", toString());
            return 0;
        }
    }
    const size_type count = devices.size();
    auto end = devices.end();
    for (auto it = devices.begin(); it != end; ++it) {
        if( nullptr != *it ) {
            BTDevice& dev = **it;
            dev.disconnect(reason); // will erase device from list via removeConnectedDevice(..) above, if successful
            removeConnectedDevice(dev); // just in case disconnect didn't went through, e.g. power-off
        }
    }
    return count;
}

BTDeviceRef BTAdapter::findConnectedDevice (const EUI48 & address, const BDAddressType & addressType) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_connectedDevices); // RAII-style acquire and relinquish via destructor
    return findDevice(hci, connectedDevices, address, addressType);
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
        jau_ERR_PRINT("Adapter[%d]: POWERED, LocalVersion failed %s - %s", dev_id, status, adapterInfo);
        return false;
    }
    le_features = hci.le_get_local_features();
    hci_uses_ext_scan = hci.use_ext_scan();
    hci_uses_ext_conn = hci.use_ext_conn();
    hci_uses_ext_adv  = hci.use_ext_adv();

    status = hci.le_set_addr_resolv_enable(true);
    if( HCIStatusCode::SUCCESS != status ) {
        jau_INFO_PRINT("Adapter[%d]: ENABLE RESOLV LIST: %s", dev_id, status);
    }
    status = hci.le_clear_resolv_list();
    if( HCIStatusCode::SUCCESS != status ) {
        jau_INFO_PRINT("Adapter[%d]: CLEAR RESOLV LIST: %s", dev_id, status);
    }

    jau_WORDY_PRINT("BTAdapter::updateDataFromHCI: Adapter[%d]: POWERED, %s - %s, hci_ext[scan %d, conn %d], features: %s",
            dev_id, version, adapterInfo, hci_uses_ext_scan, hci_uses_ext_conn, le_features);
    return true;
}

bool BTAdapter::updateDataFromAdapterInfo() noexcept {
    BTMode btMode = getBTMode();
    if( BTMode::NONE == btMode ) {
        jau_WARN_PRINT("Adapter[%d]: BTMode invalid, BREDR nor LE set: %s", dev_id, adapterInfo);
        return false;
    }
    hci.setBTMode(btMode);
    return true;
}

bool BTAdapter::initialSetup() noexcept {
    if( !mgmt->isOpen() ) {
        jau_ERR_PRINT("Adapter[%d]: Manager not open", dev_id);
        return false;
    }
    if( !hci.isOpen() ) {
        jau_ERR_PRINT("Adapter[%d]: HCIHandler closed", dev_id);
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
        jau_WORDY_PRINT("BTAdapter::initialSetup: Adapter[%d]: Not POWERED: %s", dev_id, adapterInfo);
    }
    // NOLINTNEXTLINE(clang-analyzer-optin.cplusplus.VirtualCall)
    jau_WORDY_PRINT("BTAdapter::initialSetup: Adapter[%d]: Done: %s - %s", dev_id, adapterInfo, toString());

    return true;
}

bool BTAdapter::enableListening(const bool enable) noexcept {
    if( enable ) {
        if( !mgmt->isOpen() ) {
            jau_ERR_PRINT("Adapter[%d]: Manager not open", dev_id);
            return false;
        }
        if( !hci.isOpen() ) {
            jau_ERR_PRINT("Adapter[%d]: HCIHandler closed", dev_id);
            return false;
        }

        // just be sure ..
        mgmt->removeMgmtEventCallback(dev_id);
        hci.clearAllCallbacks();

        bool ok = true;
        // ok = mgmt->addMgmtEventCallback(dev_id, MgmtEvent::Opcode::DISCOVERING, jau::bind_member(this, &BTAdapter::mgmtEvDeviceDiscoveringMgmt)) && ok;
        ok = mgmt->addMgmtEventCallback(dev_id, MgmtEvent::Opcode::NEW_SETTINGS, jau::bind_member(this, &BTAdapter::mgmtEvNewSettingsMgmt)) && ok;
        ok = mgmt->addMgmtEventCallback(dev_id, MgmtEvent::Opcode::LOCAL_NAME_CHANGED, jau::bind_member(this, &BTAdapter::mgmtEvLocalNameChangedMgmt)) && ok;
        ok = mgmt->addMgmtEventCallback(dev_id, MgmtEvent::Opcode::PIN_CODE_REQUEST, jau::bind_member(this, &BTAdapter::mgmtEvPinCodeRequestMgmt)) && ok;
        ok = mgmt->addMgmtEventCallback(dev_id, MgmtEvent::Opcode::USER_CONFIRM_REQUEST, jau::bind_member(this, &BTAdapter::mgmtEvUserConfirmRequestMgmt)) && ok;
        ok = mgmt->addMgmtEventCallback(dev_id, MgmtEvent::Opcode::USER_PASSKEY_REQUEST, jau::bind_member(this, &BTAdapter::mgmtEvUserPasskeyRequestMgmt)) && ok;
        ok = mgmt->addMgmtEventCallback(dev_id, MgmtEvent::Opcode::PASSKEY_NOTIFY, jau::bind_member(this, &BTAdapter::mgmtEvPasskeyNotifyMgmt)) && ok;
        ok = mgmt->addMgmtEventCallback(dev_id, MgmtEvent::Opcode::AUTH_FAILED, jau::bind_member(this, &BTAdapter::mgmtEvAuthFailedMgmt)) && ok;
        ok = mgmt->addMgmtEventCallback(dev_id, MgmtEvent::Opcode::DEVICE_UNPAIRED, jau::bind_member(this, &BTAdapter::mgmtEvDeviceUnpairedMgmt)) && ok;
        ok = mgmt->addMgmtEventCallback(dev_id, MgmtEvent::Opcode::PAIR_DEVICE_COMPLETE, jau::bind_member(this, &BTAdapter::mgmtEvPairDeviceCompleteMgmt)) && ok;
        ok = mgmt->addMgmtEventCallback(dev_id, MgmtEvent::Opcode::NEW_LONG_TERM_KEY, jau::bind_member(this, &BTAdapter::mgmtEvNewLongTermKeyMgmt)) && ok;
        ok = mgmt->addMgmtEventCallback(dev_id, MgmtEvent::Opcode::NEW_LINK_KEY, jau::bind_member(this, &BTAdapter::mgmtEvNewLinkKeyMgmt)) && ok;
        ok = mgmt->addMgmtEventCallback(dev_id, MgmtEvent::Opcode::NEW_IRK, jau::bind_member(this, &BTAdapter::mgmtEvNewIdentityResolvingKeyMgmt)) && ok;

        if( debug_event || jau::environment::get().debug ) {
            ok = mgmt->addMgmtEventCallback(dev_id, MgmtEvent::Opcode::DEVICE_CONNECTED, jau::bind_member(this, &BTAdapter::mgmtEvDeviceConnectedMgmt)) && ok;
            ok = mgmt->addMgmtEventCallback(dev_id, MgmtEvent::Opcode::DEVICE_DISCONNECTED, jau::bind_member(this, &BTAdapter::mgmtEvMgmtAnyMgmt)) && ok;
        }

        if( !ok ) {
            jau_ERR_PRINT("Could not add all required MgmtEventCallbacks to DBTManager: %s", toString());
            return false;
        }

    #if 0
        mgmt->addMgmtEventCallback(dev_id, MgmtEvent::Opcode::DEVICE_DISCONNECTED, bind_member(this, &BTAdapter::mgmtEvDeviceDisconnectedMgmt));
    #endif

        ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::DISCOVERING, jau::bind_member(this, &BTAdapter::mgmtEvDeviceDiscoveringHCI)) && ok;
        ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::DEVICE_CONNECTED, jau::bind_member(this, &BTAdapter::mgmtEvDeviceConnectedHCI)) && ok;
        ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::CONNECT_FAILED, jau::bind_member(this, &BTAdapter::mgmtEvConnectFailedHCI)) && ok;
        ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::DEVICE_DISCONNECTED, jau::bind_member(this, &BTAdapter::mgmtEvDeviceDisconnectedHCI)) && ok;
        ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::DEVICE_FOUND, jau::bind_member(this, &BTAdapter::mgmtEvDeviceFoundHCI)) && ok;
        ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::HCI_LE_REMOTE_FEATURES, jau::bind_member(this, &BTAdapter::mgmtEvHCILERemoteUserFeaturesHCI)) && ok;
        ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::HCI_LE_PHY_UPDATE_COMPLETE, jau::bind_member(this, &BTAdapter::mgmtEvHCILEPhyUpdateCompleteHCI)) && ok;

        ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::HCI_ENC_CHANGED, jau::bind_member(this, &BTAdapter::mgmtEvHCIEncryptionChangedHCI)) && ok;
        ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::HCI_ENC_KEY_REFRESH_COMPLETE, jau::bind_member(this, &BTAdapter::mgmtEvHCIEncryptionKeyRefreshCompleteHCI)) && ok;
        if constexpr ( CONSIDER_HCI_CMD_FOR_SMP_STATE ) {
            ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::HCI_LE_LTK_REQUEST, jau::bind_member(this, &BTAdapter::mgmtEvLELTKReqEventHCI)) && ok;
            ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::HCI_LE_LTK_REPLY_ACK, jau::bind_member(this, &BTAdapter::mgmtEvLELTKReplyAckCmdHCI)) && ok;
            ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::HCI_LE_LTK_REPLY_REJ, jau::bind_member(this, &BTAdapter::mgmtEvLELTKReplyRejCmdHCI)) && ok;
            ok = hci.addMgmtEventCallback(MgmtEvent::Opcode::HCI_LE_ENABLE_ENC, jau::bind_member(this, &BTAdapter::mgmtEvLEEnableEncryptionCmdHCI)) && ok;
        }
        if( !ok ) {
            jau_ERR_PRINT("Could not add all required MgmtEventCallbacks to HCIHandler: %s of %s", hci, toString());
            return false; // dtor local HCIHandler w/ closing
        }
        if (!hci.addSMPMsgCallback(jau::bind_member(this, &BTAdapter::hciSMPMsgCallback))) {
            jau_ERR_PRINT("Could not add SMP Callback to HCIHandler: %s of %s", hci, toString());
            return false; // dtor local HCIHandler w/ closing
        }
    } else {
        mgmt->removeMgmtEventCallback(dev_id);
        hci.clearAllCallbacks();
    }
    jau_WORDY_PRINT("BTAdapter::enableListening: Adapter[%d]: Done: %s - %s", dev_id, adapterInfo, toString());

    return true;
}

BTAdapter::BTAdapter(const BTAdapter::ctor_cookie& cc, BTManagerRef mgmt_, AdapterInfo adapterInfo_)
: debug_event(jau::environment::getBooleanProperty("direct_bt.debug.adapter.event", false)),
  debug_lock(jau::environment::getBooleanProperty("direct_bt.debug.adapter.lock", false)),
  mgmt( std::move(mgmt_) ),
  adapterInfo( std::move(adapterInfo_) ),
  adapter_initialized( false ), adapter_poweredoff_at_init( true ),
  le_features( LE_Features::NONE ),
  hci_uses_ext_scan( false ), hci_uses_ext_conn( false ), hci_uses_ext_adv( false ),
  visibleAddressAndType( adapterInfo.addressAndType ),
  visibleMACType( HCILEOwnAddressType::PUBLIC ),
  privacyIRK(),
  dev_id( adapterInfo.dev_id ),
  btRole ( BTRole::Master ),
  hci( dev_id ),
  currentMetaScanType( ScanType::NONE ),
  discovery_policy ( DiscoveryPolicy::AUTO_OFF ),
  scan_filter_dup( true ),
  smp_watchdog(jau::format_string("adapter%u_smp_watchdog", dev_id), THREAD_SHUTDOWN_TIMEOUT_MS),
  l2cap_att_srv(dev_id, adapterInfo.addressAndType, L2CAP_PSM::UNDEFINED, L2CAP_CID::ATT),
  l2cap_service("BTAdapter::l2capServer", THREAD_SHUTDOWN_TIMEOUT_MS,
                jau::bind_member(this, &BTAdapter::l2capServerWork),
                jau::bind_member(this, &BTAdapter::l2capServerInit),
                jau::bind_member(this, &BTAdapter::l2capServerEnd)),
  discovery_service("BTAdapter::discoveryServer", 400_ms,
                jau::bind_member(this, &BTAdapter::discoveryServerWork))

{
    (void)cc;

    adapter_operational = initialSetup();
    if( isValid() ) {
        const bool r = smp_watchdog.start(SMP_NEXT_EVENT_TIMEOUT_MS, jau::bind_member(this, &BTAdapter::smp_timeoutfunc));
        jau_DBG_PRINT("BTAdapter::ctor: dev_id %u: smp_watchdog.smp_timeoutfunc started %d", dev_id, r);
    }
}

BTAdapter::~BTAdapter() noexcept {
    if( !isValid() ) {
        jau_DBG_PRINT("BTAdapter::dtor: dev_id %u, invalid, %p", dev_id, this);
        smp_watchdog.stop();
        mgmt->removeAdapter(this); // remove this instance from manager
        hci.clearAllCallbacks();
        return;
    }
    // NOLINTNEXTLINE(clang-analyzer-optin.cplusplus.VirtualCall)
    jau_DBG_PRINT("BTAdapter::dtor: ... %p %s", this, toString());
    close();

    mgmt->removeAdapter(this); // remove this instance from manager

    jau_DBG_PRINT("BTAdapter::dtor: XXX");
}

void BTAdapter::close() noexcept {
    smp_watchdog.stop();
    if( !isValid() ) {
        // Native user app could have destroyed this instance already from
        jau_DBG_PRINT("BTAdapter::close: dev_id %u, invalid, %p", dev_id, this);
        return;
    }
    // NOLINTNEXTLINE(clang-analyzer-optin.cplusplus.VirtualCall)
    jau_DBG_PRINT("BTAdapter::close: ... %p %s", this, toString());
    discovery_policy = DiscoveryPolicy::AUTO_OFF;

    // mute all listener first
    {
        size_type count = mgmt->removeMgmtEventCallback(dev_id);
        jau_DBG_PRINT("BTAdapter::close removeMgmtEventCallback: %zu callbacks", (size_t)count);
    }
    hci.clearAllCallbacks();
    statusListenerList.clear();

    poweredOff(true /* active */, "close");

    if( adapter_poweredoff_at_init && isPowered() ) {
        setPowered(false);
    }

    jau_DBG_PRINT("BTAdapter::close: close[HCI, l2cap_srv]: ...");
    hci.close();
    l2cap_service.stop();
    l2cap_att_srv.close();
    discovery_service.stop();
    jau_DBG_PRINT("BTAdapter::close: close[HCI, l2cap_srv, discovery_srv]: XXX");

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
    jau_DBG_PRINT("BTAdapter::close: XXX");
}

void BTAdapter::poweredOff(bool active, const std::string& msg) noexcept {
    if( !isValid() ) {
        jau_ERR_PRINT("BTAdapter invalid: dev_id %u, %p", dev_id, this);
        return;
    }
    jau_DBG_PRINT("BTAdapter::poweredOff(active %d, %s).0: ... %p, %s", active, msg, this, toString());
    if( jau::environment::get().debug ) {
        if( !active ) {
            jau::print_backtrace(true /* skip_anon_frames */, 4 /* max_frames */, 2 /* skip_frames: print_b*() + get_b*() */);
        }
    }
    if( !hci.isOpen() ) {
        jau_INFO_PRINT("BTAdapter::poweredOff: HCI closed: active %d -> 0: %s", active, toString());
        active = false;
    } else if( active && !adapterInfo.isCurrentSettingBitSet(AdapterSetting::POWERED) ) {
        jau_DBG_PRINT("BTAdapter::poweredOff: !POWERED: active %d -> 0: %s", active, toString());
        active = false;
    }
    discovery_policy = DiscoveryPolicy::PAUSE_CONNECTED_UNTIL_READY;

    if( active ) {
        stopDiscoveryImpl(true /* forceDiscoveringEvent */, false /* temporary */);
    }

    // Removes all device references from the lists: connectedDevices, discoveredDevices
    disconnectAllDevices(HCIStatusCode::NOT_POWERED);
    removeDiscoveredDevices();

    // ensure all hci states are reset.
    hci.resetAllStates(false);

    currentMetaScanType = ScanType::NONE;
    btRole = BTRole::Master;

    unlockConnectAny();

    jau_DBG_PRINT("BTAdapter::poweredOff(active %d, %s).X: %s", active, msg, toString());
}

void BTAdapter::printDeviceList(const std::string& prefix, const BTAdapter::device_list_t& list) noexcept {
    const size_t sz = list.size();
    jau_PLAIN_PRINT(true, "- BTAdapter::%s: %zu elements", prefix, sz);
    int idx = 0;
    for (auto it = list.begin(); it != list.end(); ++idx, ++it) {
        /**
         * TODO
         *
         * g++ (Debian 12.2.0-3) 12.2.0, Debian 12 Bookworm 2022-10-17
         * g++ bug: False positive of '-Werror=stringop-overflow=' using std::atomic_bool::load()
         *
 In member function ‘std::__atomic_base<_IntTp>::__int_type std::__atomic_base<_IntTp>::load(std::memory_order) const [with _ITp = bool]’,
    inlined from ‘bool std::atomic<bool>::load(std::memory_order) const’ at /usr/include/c++/12/atomic:112:26,
    inlined from ‘bool direct_bt::BTObject::isValidInstance() const’ at direct_bt/api/direct_bt/BTTypes1.hpp:66:86,
    inlined from ‘static void direct_bt::BTAdapter::printDeviceList(const std::string&, const device_list_t&)’ at direct_bt/BTAdapter.cpp:526:42:
/usr/include/c++/12/bits/atomic_base.h:488:31: error:
 *     ‘unsigned char __atomic_load_1(const volatile void*, int)’ writing 1 byte into a region of size 0 overflows the destination [-Werror=stringop-overflow=]
  488 |         return __atomic_load_n(&_M_i, int(__m));
      |                ~~~~~~~~~~~~~~~^~~~~~~~~~~~~~~~~
         */
        PRAGMA_DISABLE_WARNING_PUSH
        PRAGMA_DISABLE_WARNING_STRINGOP_OVERFLOW
        if( nullptr != (*it) ) {
            jau_PLAIN_PRINT(true, "  - %d / %zu: null", (idx+1), sz);
        } else if( (*it)->isValidInstance() /** TODO: See above */ ) {
            jau_PLAIN_PRINT(true, "  - %d / %zu: invalid", (idx+1), sz);
        } else {
            jau_PLAIN_PRINT(true, "  - %d / %zu: %s, name '%s', visible %s", (idx+1), sz,
                (*it)->getAddressAndType(), (*it)->getName(), (*it)->getVisibleAddressAndType());
        }
        PRAGMA_DISABLE_WARNING_POP
    }
}
void BTAdapter::printWeakDeviceList(const std::string& prefix, BTAdapter::weak_device_list_t& list) noexcept {
    const size_t sz = list.size();
    jau_PLAIN_PRINT(true, "- BTAdapter::%s: %zu elements", prefix, sz);
    int idx = 0;
    for (auto it = list.begin(); it != list.end(); ++idx, ++it) {
        std::weak_ptr<BTDevice> & w = *it;
        BTDeviceRef e = w.lock();
        if( nullptr == e ) {
            jau_PLAIN_PRINT(true, "  - %d / %zu: null", (idx+1), sz);
        } else if( !e->isValidInstance() ) {
            jau_PLAIN_PRINT(true, "  - %d / %zu: invalid", (idx+1), sz);
        } else {
            jau_PLAIN_PRINT(true, "  - %d / %zu: %s, name '%s', visible %s", (idx+1), sz,
                e->getAddressAndType(), e->getName(), e->getVisibleAddressAndType());
        }
    }
}
void BTAdapter::printDeviceLists() noexcept {
    try {
        // Using expensive copies here: debug mode only
        weak_device_list_t _sharedDevices, _discoveredDevices, _connectedDevices, _pausingDiscoveryDevice;
        {
            const std::lock_guard<std::mutex> lock(mtx_sharedDevices); // RAII-style acquire and relinquish via destructor
            for(const BTDeviceRef& d : sharedDevices) { _sharedDevices.push_back(d); }
        }
        {
            const std::lock_guard<std::mutex> lock(mtx_discoveredDevices); // RAII-style acquire and relinquish via destructor
            for(const BTDeviceRef& d : discoveredDevices) { _discoveredDevices.push_back(d); }
        }
        {
            const std::lock_guard<std::mutex> lock(mtx_connectedDevices); // RAII-style acquire and relinquish via destructor
            for(const BTDeviceRef& d : connectedDevices) { _connectedDevices.push_back(d); }
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
    } catch(const std::exception &e) {
        jau_ERR_PRINT2("%s\n", e.what());
    }
}

void BTAdapter::printStatusListenerList() noexcept {
    try {
        auto begin = statusListenerList.begin(); // lock mutex and copy_store
        jau_PLAIN_PRINT(true, "- BTAdapter::StatusListener    : %zu elements", (size_t)begin.size());
        for(int ii=0; !begin.is_end(); ++ii, ++begin ) {
            jau_PLAIN_PRINT(true, "  - %d / %zu: %p, %s", (ii+1), (size_t)begin.size(), begin->listener.get(), begin->listener->toString());
        }
    } catch(const std::exception &e) {
        jau_ERR_PRINT2("%s\n", e.what());
    }
}

HCIStatusCode BTAdapter::setName(const std::string &name, const std::string &short_name) {
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

HCIStatusCode BTAdapter::setPrivacy(const bool enable) noexcept {
    jau::uint128dp_t irk;
    if( enable ) {
        std::unique_ptr<std::random_device> rng_hw = std::make_unique<std::random_device>();
        for(int i=0; i<4; ++i) {
            std::uint32_t v = (*rng_hw)();
            irk.data[4*i+0] = v & 0x000000ffu;
            irk.data[4*i+1] = ( v & 0x0000ff00u ) >>  8;
            irk.data[4*i+2] = ( v & 0x00ff0000u ) >> 16;
            irk.data[4*i+3] = ( v & 0xff000000u ) >> 24;
        }
    } else {
        irk.clear();
    }
    AdapterSetting settings = adapterInfo.getCurrentSettingMask();
    HCIStatusCode res = mgmt->setPrivacy(dev_id, enable ? 0x01 : 0x00, irk, settings);
    if( HCIStatusCode::SUCCESS == res ) {
        if( enable ) {
            // FIXME: Not working for LE set scan param etc ..
            // TODO visibleAddressAndType = ???
            // visibleMACType = HCILEOwnAddressType::RESOLVABLE_OR_RANDOM;
            visibleMACType = HCILEOwnAddressType::RESOLVABLE_OR_PUBLIC;
            // visibleMACType = HCILEOwnAddressType::RANDOM;
        } else {
            visibleAddressAndType = adapterInfo.addressAndType;
            visibleMACType = HCILEOwnAddressType::PUBLIC;
            privacyIRK.address = adapterInfo.addressAndType.address;
            privacyIRK.address_type = adapterInfo.addressAndType.type;
            privacyIRK.irk.clear();
        }
    }
    const AdapterSetting new_settings = adapterInfo.setCurrentSettingMask(settings);
    updateAdapterSettings(false /* off_thread */, new_settings, true /* sendEvent */, 0);
    return res;
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
    BTDevice::validateSecParam(sec_level, io_cap, sec_level_server, io_cap_server);
}

void BTAdapter::setSMPKeyPath(std::string path) {
    jau::sc_atomic_critical sync(sync_data);
    key_path = std::move(path);

    std::vector<SMPKeyBin> keys = SMPKeyBin::readAllForLocalAdapter(getAddressAndType(), key_path, jau::environment::get().debug /* verbose_ */);
    for(SMPKeyBin f : keys) {
        uploadKeys(f, false /* write */);
    }
}

HCIStatusCode BTAdapter::uploadKeys(SMPKeyBin& bin, const bool write) {
    if( bin.getLocalAddrAndType() != adapterInfo.addressAndType ) {
        if( bin.getVerbose() ) {
            jau_PLAIN_PRINT(true, "BTAdapter::setSMPKeyBin: Adapter address not matching: %s, %s", bin, toString());
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
        jau_ERR_PRINT("(dev_id %u): Unpair device failed %s of %s: %s", dev_id, res, bin.getRemoteAddrAndType(), toString());
    }

    res = device->uploadKeys(bin, BTSecurityLevel::NONE);
    if( HCIStatusCode::SUCCESS != res ) {
        jau_WARN_PRINT("(dev_id %u): Upload SMPKeyBin failed %s, %s (removing file)", dev_id, res, bin);
        if( key_path.size() > 0 ) {
            bin.remove(key_path);
        }
        return res;
    } else {
        jau_DBG_PRINT("BTAdapter::setSMPKeyBin(dev_id %u): Upload OK: %s, %s", dev_id, bin, toString());
    }
    addSMPKeyBin( std::make_shared<SMPKeyBin>(bin), write); // PERIPHERAL_ADAPTER_MANAGES_SMP_KEYS
    return HCIStatusCode::SUCCESS;
}

HCIStatusCode BTAdapter::initialize(const BTMode btMode, const bool powerOn) noexcept {
    const bool was_powered = adapterInfo.isCurrentSettingBitSet(AdapterSetting::POWERED);
    adapter_initialized = true;
    adapter_poweredoff_at_init = was_powered;

    // Also fails if unable to power-on and not powered-on!
    HCIStatusCode status = mgmt->initializeAdapter(adapterInfo, dev_id, btMode, powerOn);
    if( HCIStatusCode::SUCCESS != status ) {
        jau_WARN_PRINT("Adapter[%d]: Failed initializing (1): res0 %s, powered[before %d, now %d], %s - %s",
            dev_id, status, was_powered, adapterInfo.isCurrentSettingBitSet(AdapterSetting::POWERED), adapterInfo, toString());
        return status;
    }
    const bool is_powered = adapterInfo.isCurrentSettingBitSet(AdapterSetting::POWERED);
    if( !enableListening(true) ) {
        return HCIStatusCode::INTERNAL_FAILURE;
    }
    updateAdapterSettings(false /* off_thread */, adapterInfo.getCurrentSettingMask(), false /* sendEvent */, 0);

    jau_WORDY_PRINT("BTAdapter::initialize: Adapter[%d]: OK: powered[%d -> %d], %s", dev_id, was_powered, is_powered, toString());
    return HCIStatusCode::SUCCESS;
}

bool BTAdapter::lockConnect(const BTDevice & device, const bool wait, const SMPIOCapability io_cap) noexcept {
    std::unique_lock<std::mutex> lock(mtx_single_conn_device); // RAII-style acquire and relinquish via destructor
    const jau::fraction_i64 timeout = 10_s; // FIXME: Configurable?

    if( nullptr != single_conn_device_ptr ) {
        if( device == *single_conn_device_ptr ) {
            jau_COND_PRINT(debug_lock, "BTAdapter::lockConnect: Success: Already locked, same device: %s", device);
            return true; // already set, same device: OK, locked
        }
        if( wait ) {
            const jau::fraction_timespec timeout_time = jau::getMonotonicTime() + jau::fraction_timespec(timeout);
            while( nullptr != single_conn_device_ptr ) {
                std::cv_status s = wait_until(cv_single_conn_device, lock, timeout_time);
                if( std::cv_status::timeout == s && nullptr != single_conn_device_ptr ) {
                    if( debug_lock ) {
                        jau_PLAIN_PRINT(true, "BTAdapter::lockConnect: Failed: Locked (waited)");
                        jau_PLAIN_PRINT(true, " - locked-by-other-device %s", single_conn_device_ptr->toString());
                        jau_PLAIN_PRINT(true, " - lock-failed-for %s", device);
                    }
                    return false;
                }
            }
            // lock was released
        } else {
            if( debug_lock ) {
                jau_PLAIN_PRINT(true, "BTAdapter::lockConnect: Failed: Locked (no-wait)");
                jau_PLAIN_PRINT(true, " - locked-by-other-device %s", single_conn_device_ptr->toString());
                jau_PLAIN_PRINT(true, " - lock-failed-for %s", device);
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
                jau_COND_PRINT(debug_lock, "BTAdapter::lockConnect: Success: New lock, setIOCapability[%s -> %s], %s",
                    pre_io_cap, io_cap, device);
                return true;
            } else {
                // failed, unlock and exit
                jau_COND_PRINT(debug_lock, "BTAdapter::lockConnect: Failed: setIOCapability[%s], %s", io_cap, device);
                single_conn_device_ptr = nullptr;
                lock.unlock(); // unlock mutex before notify_all to avoid pessimistic re-block of notified wait() thread.
                cv_single_conn_device.notify_all(); // notify waiting getter
                return false;
            }
        } else {
            jau_COND_PRINT(debug_lock, "BTAdapter::lockConnect: Success: New lock, ignored io-cap: %s, %s", io_cap, device);
            return true;
        }
    } else {
        jau_COND_PRINT(debug_lock, "BTAdapter::lockConnect: Success: New lock, no io-cap: %s", device);
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
            jau_COND_PRINT(debug_lock, "BTAdapter::unlockConnect: Success: setIOCapability[res %d: %s -> %s], %s",
                res, o, v, single_conn_device_ptr->toString());
        } else {
            jau_COND_PRINT(debug_lock, "BTAdapter::unlockConnect: Success: %s", single_conn_device_ptr->toString());
        }
        single_conn_device_ptr = nullptr;
        lock.unlock(); // unlock mutex before notify_all to avoid pessimistic re-block of notified wait() thread.
        cv_single_conn_device.notify_all(); // notify waiting getter
        return true;
    } else {
        if( debug_lock ) {
            jau_PLAIN_PRINT(true, "BTAdapter::unlockConnect: Not locked:");
            jau_PLAIN_PRINT(true, " - locked-by-other-device %s",
                (nullptr != single_conn_device_ptr ? single_conn_device_ptr->toString() : "null"));
            jau_PLAIN_PRINT(true, " - unlock-failed-for %s", device);
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
            jau_COND_PRINT(debug_lock, "BTAdapter::unlockConnectAny: Success: setIOCapability[res %d: %s -> %s]; %s",
                res, o, v, single_conn_device_ptr->toString());
        } else {
            jau_COND_PRINT(debug_lock, "BTAdapter::unlockConnectAny: Success: %s", single_conn_device_ptr->toString());
        }
        single_conn_device_ptr = nullptr;
        lock.unlock(); // unlock mutex before notify_all to avoid pessimistic re-block of notified wait() thread.
        cv_single_conn_device.notify_all(); // notify waiting getter
        return true;
    } else {
        iocap_defaultval = SMPIOCapability::UNSET;
        jau_COND_PRINT(debug_lock, "BTAdapter::unlockConnectAny: Not locked");
        return false;
    }
}

HCIStatusCode BTAdapter::reset() noexcept {
    if( !isValid() ) {
        jau_ERR_PRINT("Adapter invalid: %s, %s", jau::toHexString(this), toString());
        return HCIStatusCode::UNSPECIFIED_ERROR;
    }
    if( !hci.isOpen() ) {
        jau_ERR_PRINT("HCI closed: %s, %s", jau::toHexString(this), toString());
        return HCIStatusCode::UNSPECIFIED_ERROR;
    }
    jau_DBG_PRINT("BTAdapter::reset.0: %s", toString());
    HCIStatusCode res = hci.resetAdapter( [&]() noexcept -> HCIStatusCode {
        jau::nsize_t connCount = getConnectedDeviceCount();
        if( 0 < connCount ) {
            const jau::fraction_i64 timeout = hci.env.HCI_COMMAND_COMPLETE_REPLY_TIMEOUT;
            const jau::fraction_i64 poll_period = hci.env.HCI_COMMAND_POLL_PERIOD;
            jau_DBG_PRINT("BTAdapter::reset: %zu connections pending - %s", connCount, toString());
            jau::fraction_i64 td = 0_s;
            while( timeout > td && 0 < connCount ) {
                sleep_for( poll_period );
                td += poll_period;
                connCount = getConnectedDeviceCount();
            }
            if( 0 < connCount ) {
                jau_WARN_PRINT("%zu connections pending after %" PRIi64 " ms - %s", connCount, td.to_ms(), toString());
            } else {
                jau_DBG_PRINT("BTAdapter::reset: pending connections resolved after %" PRIi64 " ms - %s", td.to_ms(), toString());
            }
        }
        return HCIStatusCode::SUCCESS; // keep going
    });
    jau_DBG_PRINT("BTAdapter::reset.X: %s - %s", res, toString());
    return res;
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
                                     const uint16_t conn_latency, const uint16_t timeout) noexcept {
    if( !isPowered() ) { // isValid() && hci.isOpen() && POWERED
        poweredOff(false /* active */, "addDeviceToWhitelist.np");
        return false;
    }
    if( mgmt->isDeviceWhitelisted(dev_id, addressAndType) ) {
        jau_ERR_PRINT("device already listed: dev_id %u, address%s", dev_id, addressAndType);
        return true;
    }

    HCIStatusCode res = mgmt->uploadConnParam(dev_id, addressAndType, conn_interval_min, conn_interval_max, conn_latency, timeout);
    if( HCIStatusCode::SUCCESS != res ) {
        jau_ERR_PRINT("uploadConnParam(dev_id %u, address%s, interval[%u..%u], latency %u, timeout %u): Failed %s",
                dev_id, addressAndType, conn_interval_min, conn_interval_max, conn_latency, timeout, res);
    }
    return mgmt->addDeviceToWhitelist(dev_id, addressAndType, ctype);
}

bool BTAdapter::removeDeviceFromWhitelist(const BDAddressAndType & addressAndType) {
    return mgmt->removeDeviceFromWhitelist(dev_id, addressAndType);
}

BTAdapter::statusListenerList_t::equal_comparator BTAdapter::adapterStatusListenerRefEqComparator =
        [](const StatusListenerPair &a, const StatusListenerPair &b) noexcept -> bool { return *a.listener == *b.listener; };

bool BTAdapter::addStatusListener(const AdapterStatusListenerRef& l) {
    if( nullptr == l ) {
        jau_ERR_PRINT("AdapterStatusListener ref is null");
        return false;
    }
    const bool added = statusListenerList.push_back_unique(StatusListenerPair{l, std::weak_ptr<BTDevice>{} },
                                                           adapterStatusListenerRefEqComparator);
    if( added ) {
        sendAdapterSettingsInitial(*l, jau::getCurrentMilliseconds());
    }
    if( _print_device_lists || jau::environment::get().verbose ) {
        jau_PLAIN_PRINT(true, "BTAdapter::addStatusListener.1: added %d, %s", added, toString());
        printDeviceLists();
    }
    return added;
}

bool BTAdapter::addStatusListener(const BTDeviceRef& d, const AdapterStatusListenerRef& l) {
    if( nullptr == l ) {
        jau_ERR_PRINT("AdapterStatusListener ref is null");
        return false;
    }
    if( nullptr == d ) {
        jau_ERR_PRINT("Device ref is null");
        return false;
    }
    const bool added = statusListenerList.push_back_unique(StatusListenerPair{l, d},
                                                           adapterStatusListenerRefEqComparator);
    if( added ) {
        sendAdapterSettingsInitial(*l, jau::getCurrentMilliseconds());
    }
    if( _print_device_lists || jau::environment::get().verbose ) {
        jau_PLAIN_PRINT(true, "BTAdapter::addStatusListener.2: added %d, %s", added, toString());
        printDeviceLists();
    }
    return added;
}

bool BTAdapter::addStatusListener(const BTDevice& d, const AdapterStatusListenerRef& l) {
    return addStatusListener(getSharedDevice(d), l);
}

bool BTAdapter::removeStatusListener(const AdapterStatusListenerRef& l) noexcept {
    if( nullptr == l ) {
        jau_ERR_PRINT("AdapterStatusListener ref is null");
        return false;
    }
    try {
        const size_type count = statusListenerList.erase_matching(StatusListenerPair{l, std::weak_ptr<BTDevice>{}},
                                                            false /* all_matching */,
                                                            adapterStatusListenerRefEqComparator);
        if( _print_device_lists || jau::environment::get().verbose ) {
            jau_PLAIN_PRINT(true, "BTAdapter::removeStatusListener.1: res %d, %s", count>0, toString());
            printDeviceLists();
        }
        return count > 0;
    } catch (...) {
        jau::fput_exception(stderr, std::current_exception(), E_FILE_LINE);
        jau_ERR_PRINT3("Exception caught while removing StatusListener: %s", l->toString());
        return false;
    }
}

bool BTAdapter::removeStatusListener(const AdapterStatusListener * l) noexcept {
    if( nullptr == l ) {
        jau_ERR_PRINT("AdapterStatusListener ref is null");
        return false;
    }
    bool res = false;
    try {
        auto it = statusListenerList.begin(); // lock mutex and copy_store
        for (; !it.is_end(); ++it ) {
            if ( *it->listener == *l ) {
                try {
                    it.erase(); // may throw
                    it.write_back();
                    res = true;
                } catch (...) {
                    jau::fput_exception(stderr, std::current_exception(), E_FILE_LINE);
                    jau_ERR_PRINT3("Exception caught while removing StatusListener: %p, %s", l, l->toString());
                    res = false;
                }
                break;
            }
        }
    } catch (...) {
        jau::fput_exception(stderr, std::current_exception(), E_FILE_LINE);
        jau_ERR_PRINT3("Exception caught while removing StatusListener %p for %s", l, toString());
    }
    if( _print_device_lists || jau::environment::get().verbose ) {
        jau_PLAIN_PRINT(true, "BTAdapter::removeStatusListener.2: res %d, %s", res, toString());
        printDeviceLists();
    }
    return res;
}

BTAdapter::size_type BTAdapter::removeAllStatusListener(const BTDevice& d) noexcept {
    size_type count = 0;

    if( 0 < statusListenerList.size() ) {
        try {
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
        } catch (...) {
            jau::fput_exception(stderr, std::current_exception(), E_FILE_LINE);
            jau_ERR_PRINT3("Exception caught while removing all StatusListener (count %zu) for %s", count, toString());
        }
    }
    return count;
}

BTAdapter::size_type BTAdapter::removeAllStatusListener() noexcept {
    const size_type count = statusListenerList.size();
    statusListenerList.clear();
    return count;
}

void BTAdapter::checkDiscoveryState() noexcept {
    const ScanType currentNativeScanType = hci.getCurrentScanType();
    // Check LE scan state
    if( DiscoveryPolicy::AUTO_OFF == discovery_policy ) {
        if( is_set(currentMetaScanType, ScanType::LE) != is_set(currentNativeScanType, ScanType::LE) ) {
            jau_ERR_PRINT("Invalid DiscoveryState: policy %s, currentScanType*[native %s != meta %s], %s",
                    discovery_policy, currentNativeScanType, currentMetaScanType, toString());
            // jau_ABORT?
        }
    } else {
        if( !is_set(currentMetaScanType, ScanType::LE) && is_set(currentNativeScanType, ScanType::LE) ) {
            jau_ERR_PRINT("Invalid DiscoveryState: policy %s, currentScanType*[native %s, meta %s], %s",
                    discovery_policy, currentNativeScanType, currentMetaScanType, toString());
            // jau_ABORT?
        }
    }
}

HCIStatusCode BTAdapter::startDiscovery(const DBGattServerRef& gattServerData_,
                                        const DiscoveryPolicy policy, const bool le_scan_active,
                                        const uint16_t le_scan_interval, const uint16_t le_scan_window,
                                        const uint8_t filter_policy,
                                        const bool filter_dup) noexcept
{
    // FIXME: Respect BTAdapter::btMode, i.e. BTMode::BREDR, BTMode::LE or BTMode::DUAL to setup BREDR, LE or DUAL scanning!
    // jau_ERR_PRINT("Test");
    // throw jau::RuntimeException("Test", E_FILE_LINE);

    clearDevicesPausingDiscovery();

    if( !isPowered() ) { // isValid() && hci.isOpen() && POWERED
        poweredOff(false /* active */, "startDiscovery.np");
        return HCIStatusCode::NOT_POWERED;
    }

    const std::lock_guard<std::mutex> lock(mtx_discovery); // RAII-style acquire and relinquish via destructor

    if( isAdvertising() ) {
        jau_WARN_PRINT("Adapter in advertising mode: %s", toString(true));
        return HCIStatusCode::COMMAND_DISALLOWED;
    }

    l2cap_service.stop();

    removeDiscoveredDevices();

    scan_filter_dup = filter_dup; // cache for background scan

    const ScanType currentNativeScanType = hci.getCurrentScanType();

    if( is_set(currentNativeScanType, ScanType::LE) ) {
        btRole = BTRole::Master;
        if( discovery_policy == policy ) {
            jau_DBG_PRINT("BTAdapter::startDiscovery: Already discovering, unchanged policy %s -> %s, currentScanType[native %s, meta %s] ...\n- %s",
                discovery_policy, policy, currentNativeScanType, currentMetaScanType, toString(true));
        } else {
            jau_DBG_PRINT("BTAdapter::startDiscovery: Already discovering, changed policy %s -> %s, currentScanType[native %s, meta %s] ...\n- %s",
                discovery_policy, policy, currentNativeScanType, currentMetaScanType, toString(true));
            discovery_policy = policy;
        }
        gattServerData = gattServerData_;
        if( _print_device_lists || jau::environment::get().verbose ) {
            jau_PLAIN_PRINT(true, "BTAdapter::startDiscovery: End.0: Result %s, policy %s -> %s, currentScanType[native %s, meta %s] ...\n- %s",
                HCIStatusCode::SUCCESS, discovery_policy, policy, hci.getCurrentScanType(), currentMetaScanType, toString());
            printDeviceLists();
        }
        checkDiscoveryState();
        return HCIStatusCode::SUCCESS;
    }

    if( _print_device_lists || jau::environment::get().verbose ) {
        jau_PLAIN_PRINT(true, "BTAdapter::startDiscovery: Start: policy %s -> %s, currentScanType[native %s, meta %s] ...\n- %s",
            discovery_policy, policy, currentNativeScanType, currentMetaScanType, toString());
    }

    discovery_policy = policy;

    if( nullptr != gattServerData_ ) {
        gattServerData_->setServicesHandles();
    }

    // if le_enable_scan(..) is successful, it will issue 'mgmtEvDeviceDiscoveringHCI(..)' immediately, which updates currentMetaScanType.
    const HCIStatusCode status = hci.le_start_scan(filter_dup, le_scan_active, visibleMACType,
                                                   le_scan_interval, le_scan_window, filter_policy);

    if( HCIStatusCode::SUCCESS != status ) {
        gattServerData = nullptr;
    } else {
        gattServerData = gattServerData_;
    }

    if( _print_device_lists || jau::environment::get().verbose ) {
        jau_PLAIN_PRINT(true, "BTAdapter::startDiscovery: End.1: Result %s, policy %s -> %s, currentScanType[native %s, meta %s] ...\n- %s",
            status, discovery_policy, policy, hci.getCurrentScanType(), currentMetaScanType, toString());
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
                jau_DBG_PRINT("BTAdapter::startDiscoveryBackground[%zu/%zu]: Policy %s, currentScanType[native %s, meta %s] ... %s",
                    trial_count+1, MAX_BACKGROUND_DISCOVERY_RETRY, discovery_policy, currentNativeScanType, currentMetaScanType, toString());
                const HCIStatusCode status = hci.le_enable_scan(true /* enable */, scan_filter_dup);
                if( HCIStatusCode::SUCCESS != status ) {
                    jau_ERR_PRINT2("le_enable_scan failed[%zu/%zu]: %s - %s",
                        trial_count+1, MAX_BACKGROUND_DISCOVERY_RETRY, status, toString());
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

HCIStatusCode BTAdapter::stopDiscovery() {
    clearDevicesPausingDiscovery();

    return stopDiscoveryImpl(false /* forceDiscoveringEvent */, false /* temporary */);
}

HCIStatusCode BTAdapter::stopDiscoveryImpl(const bool forceDiscoveringEvent, const bool temporary) {
    // We allow !isEnabled, to utilize method for adjusting discovery state and notifying listeners
    // FIXME: Respect BTAdapter::btMode, i.e. BTMode::BREDR, BTMode::LE or BTMode::DUAL to stop BREDR, LE or DUAL scanning!

    if( !isValid() ) {
        jau_ERR_PRINT("Adapter invalid: %s, %s", jau::toHexString(this), toString());
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

    jau_DBG_PRINT("BTAdapter::stopDiscovery: Start: policy %s, currentScanType[native %s, meta %s], le_scan_temp_disabled %d, forceDiscEvent %d ...",
        discovery_policy, currentNativeScanType, currentMetaScanType, le_scan_temp_disabled, forceDiscoveringEvent);

    if( !temporary ) {
        discovery_policy = DiscoveryPolicy::AUTO_OFF;
    }

    if( !is_set(currentMetaScanType, ScanType::LE) ) {
        jau_DBG_PRINT("BTAdapter::stopDiscovery: Already disabled, policy %s, currentScanType[native %s, meta %s] ...",
            discovery_policy, currentNativeScanType, currentMetaScanType);
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
            jau_ERR_PRINT("le_enable_scan failed: %s", status);
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
        jau_PLAIN_PRINT(true, "BTAdapter::stopDiscovery: End: Result %s, policy %s, currentScanType[native %s, meta %s], le_scan_temp_disabled %d ...\n- %s",
            status, discovery_policy, hci.getCurrentScanType(), currentMetaScanType, le_scan_temp_disabled, toString());
        printDeviceLists();
    }

    checkDiscoveryState();

    return status;
}

// *************************************************

BTDeviceRef BTAdapter::findDiscoveredDevice (const EUI48 & address, const BDAddressType addressType) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_discoveredDevices); // RAII-style acquire and relinquish via destructor
    return findDevice(hci, discoveredDevices, address, addressType);
}

bool BTAdapter::addDiscoveredDevice(BTDeviceRef const &device) {
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
            try {
                discoveredDevices.erase(it);
            } catch (...) {
                jau::fput_exception(stderr, std::current_exception(), E_FILE_LINE);
                jau_ERR_PRINT3("Exception caught while removing discovered device: %s", (*it)->toString());
                return false;
            }
            return true;
        }
    }
    return false;
}

BTAdapter::size_type BTAdapter::removeDiscoveredDevices() {
    size_type res;
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
        jau_PLAIN_PRINT(true, "BTAdapter::removeDiscoveredDevices: End: %zu, %s", (size_t)res, toString());
        printDeviceLists();
    }
    return res;
}

jau::darray<BTDeviceRef> BTAdapter::getDiscoveredDevices() const noexcept {
    try {
        jau::sc_atomic_critical sync(sync_data); // lock-free simple cache-load 'snapshot'
        return discoveredDevices;
    } catch( ... ) {
        jau::fput_exception(stderr, std::current_exception(), E_FILE_LINE);
        jau_ERR_PRINT3("Exception caught while removing paused discovery device for %s", toString());
        return device_list_t();
    }
}

// *************************************************

bool BTAdapter::addSharedDevice(BTDeviceRef const &device) {
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
            try {
                sharedDevices.erase(it);
            } catch (...) {
                jau::fput_exception(stderr, std::current_exception(), E_FILE_LINE);
                jau_ERR_PRINT3("Exception caught while removing shared device: %s", (*it)->toString());
            }
            return; // unique set
        } else {
            ++it;
        }
    }
}

BTDeviceRef BTAdapter::findSharedDevice (const EUI48 & address, const BDAddressType addressType) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_sharedDevices); // RAII-style acquire and relinquish via destructor
    return findDevice(hci, sharedDevices, address, addressType);
}

// *************************************************

void BTAdapter::removeDevice(BTDevice & device) noexcept {
    jau_WORDY_PRINT("DBTAdapter::removeDevice: Start %s", toString());
    removeAllStatusListener(device);

    const HCIStatusCode status = device.disconnect(HCIStatusCode::REMOTE_USER_TERMINATED_CONNECTION);
    jau_WORDY_PRINT("BTAdapter::removeDevice: disconnect %s, %s", status, toString());
    unlockConnect(device);
    removeConnectedDevice(device); // usually done in BTAdapter::mgmtEvDeviceDisconnectedHCI
    removeDiscoveredDevice(device.addressAndType); // usually done in BTAdapter::mgmtEvDeviceDisconnectedHCI
    removeDevicePausingDiscovery(device);
    if( device.getAvailableSMPKeys(false /* responder */) == SMPKeyType::NONE ) {
        // Only remove from shared device list if not an initiator (LL master) having paired keys!
        removeSharedDevice(device);
    }

    if( _print_device_lists || jau::environment::get().verbose ) {
        jau_PLAIN_PRINT(true, "BTAdapter::removeDevice: End %s, %s", device.getAddressAndType(), toString());
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
bool BTAdapter::removeSMPKeyBin(key_list_t & keys, BDAddressAndType const & remoteAddress, const bool remove_file, const std::string& key_path_) noexcept {
    for (auto it = keys.begin(); it != keys.end(); ++it) {
        const SMPKeyBinRef& k = *it;
        if ( nullptr != k && remoteAddress == k->getRemoteAddrAndType() ) {
            jau_DBG_PRINT("BTAdapter::removeSMPKeyBin(file %d): %s", remove_file, k->toString());
            if( remove_file && key_path_.size() > 0 ) {
                if( !k->remove(key_path_) ) {
                    jau_WARN_PRINT("Failed removal of SMPKeyBin file: %s", k->getFilename(key_path_));
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
bool BTAdapter::addSMPKeyBin(const SMPKeyBinRef& key, const bool write_file) {
    const std::lock_guard<std::mutex> lock(mtx_keys); // RAII-style acquire and relinquish via destructor
    removeSMPKeyBin(key_list, key->getRemoteAddrAndType(), write_file /* remove_file */, key_path);
    if( jau::environment::get().debug ) {
        key->setVerbose(true);
        jau_DBG_PRINT("BTAdapter::addSMPKeyBin(file %d): %s", write_file, key->toString());
    }
    key_list.push_back( key );
    if( write_file && key_path.size() > 0 ) {
        if( !key->write(key_path, true /* overwrite */) ) {
            jau_WARN_PRINT("Failed write of SMPKeyBin file: %s", key->getFilename(key_path));
        }
    }
    return true;
}
bool BTAdapter::removeSMPKeyBin(BDAddressAndType const & remoteAddress, const bool remove_file) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_keys); // RAII-style acquire and relinquish via destructor
    return removeSMPKeyBin(key_list, remoteAddress, remove_file, key_path);
}

// *************************************************

HCIStatusCode BTAdapter::startAdvertising(const DBGattServerRef& gattServerData_,
                               EInfoReport& eir, EIRDataType adv_mask, EIRDataType scanrsp_mask,
                               const uint16_t adv_interval_min, const uint16_t adv_interval_max,
                               const AD_PDU_Type adv_type,
                               const uint8_t adv_chan_map,
                               const uint8_t filter_policy) {
    if( !isPowered() ) { // isValid() && hci.isOpen() && POWERED
        poweredOff(false /* active */, "startAdvertising.np");
        return HCIStatusCode::NOT_POWERED;
    }

    if( isDiscovering() ) {
        jau_WARN_PRINT("Not allowed (scan enabled): %s", toString(true));
        return HCIStatusCode::COMMAND_DISALLOWED;
    }
    const jau::nsize_t connCount = getConnectedDeviceCount();
    if( 0 < connCount ) { // FIXME: May shall not be a restriction
        jau_WARN_PRINT("Not allowed (%zu connections open/pending): %s", connCount, toString(true));
        printDeviceLists();
        return HCIStatusCode::COMMAND_DISALLOWED;
    }
    if( jau::environment::get().debug ) {
        std::vector<MgmtDefaultParam> params = mgmt->readDefaultSysParam(dev_id);
        jau_DBG_PRINT("BTAdapter::startAdvertising[%u]: SysParam: %zu", dev_id, params.size());
        for(size_t i=0; i<params.size(); ++i) {
            jau_PLAIN_PRINT(true, "[%2.2zu]: %s", i, params[i]);
        }
    }
    if constexpr ( USE_LINUX_BT_SECURITY ) {
        SMPIOCapability pre_io_cap { SMPIOCapability::UNSET };
        SMPIOCapability new_io_cap = SMPIOCapability::UNSET != io_cap_server ? io_cap_server : SMPIOCapability::NO_INPUT_NO_OUTPUT;
        const bool res_iocap = mgmt->setIOCapability(dev_id, new_io_cap, pre_io_cap);
        jau_DBG_PRINT("BTAdapter::startAdvertising: dev_id %u, setIOCapability[%s -> %s]: result %d",
            dev_id, pre_io_cap, new_io_cap, res_iocap);
    }
    jau_DBG_PRINT("BTAdapter::startAdvertising.1: dev_id %u, %s", dev_id, toString());
    l2cap_service.stop();
    l2cap_service.start();

    if( !l2cap_att_srv.is_open() ) {
        jau_ERR_PRINT("l2cap_service failed: %s", toString(true));
        l2cap_service.stop();
        return HCIStatusCode::INTERNAL_FAILURE;
    }

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
    const HCILEOwnAddressType own_mac_type=visibleMACType;
    const HCILEOwnAddressType peer_mac_type=HCILEOwnAddressType::PUBLIC;

    HCIStatusCode status = hci.le_start_adv(eir, adv_mask, scanrsp_mask,
                                            peer_bdaddr, own_mac_type, peer_mac_type,
                                            adv_interval_min, adv_interval_max, adv_type, adv_chan_map, filter_policy);
    if( HCIStatusCode::SUCCESS != status ) {
        jau_ERR_PRINT("le_start_adv failed: %s - %s", status, toString(true));
        gattServerData = nullptr;
        l2cap_service.stop();
    } else {
        gattServerData = gattServerData_;
        btRole = BTRole::Slave;
        jau_DBG_PRINT("BTAdapter::startAdvertising.OK: dev_id %u, %s", dev_id, toString());
    }
    return status;
}

HCIStatusCode BTAdapter::startAdvertising(const DBGattServerRef& gattServerData_,
                               const uint16_t adv_interval_min, const uint16_t adv_interval_max,
                               const AD_PDU_Type adv_type,
                               const uint8_t adv_chan_map,
                               const uint8_t filter_policy) {
    EInfoReport eir;
    EIRDataType adv_mask = EIRDataType::FLAGS | EIRDataType::SERVICE_UUID;
    EIRDataType scanrsp_mask = EIRDataType::NAME | EIRDataType::CONN_IVAL;

    eir.setFlags(GAPFlags::LE_Gen_Disc);
    eir.setName(getName());
    eir.setConnInterval(10, 24); // default
    if( nullptr != gattServerData_ ) {
        for(const DBGattServiceRef& s : gattServerData_->getServices()) {
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
HCIStatusCode BTAdapter::stopAdvertising() {
    if( !isPowered() ) { // isValid() && hci.isOpen() && POWERED
        poweredOff(false /* active */, "stopAdvertising.np");
        return HCIStatusCode::NOT_POWERED;
    }

    l2cap_service.stop();

    HCIStatusCode status = hci.le_enable_adv(false /* enable */);
    if( HCIStatusCode::SUCCESS != status ) {
        jau_ERR_PRINT("le_enable_adv failed: %s", status);
    }
    return status;
}

// *************************************************

std::string BTAdapter::toString(bool includeDiscoveredDevices) const noexcept {
    std::string out = jau_format_string("Adapter[%d, BT %d, BTMode %s, %s, %s",
                                        dev_id, getBTMajorVersion(), getBTMode(), getRole(), adapterInfo.addressAndType);

    if (adapterInfo.addressAndType != visibleAddressAndType) {
        jau_append_string(out, " (%s)", visibleAddressAndType);
    }

    jau_append_string(out, " '%s', curSettings%s, valid %s, adv %s, scanType[native %s, meta %s], open[mgmt, %s, hci %s], %s, %s]",
        getName(), adapterInfo.getCurrentSettingMask(), isValid(), hci.isAdvertising(), hci.getCurrentScanType(),
        currentMetaScanType, mgmt->isOpen(), hci.isOpen(), l2cap_att_srv, javaObjectToString());

    if( includeDiscoveredDevices ) {
        device_list_t devices = getDiscoveredDevices();
        if( devices.size() > 0 ) {
            jau::append_string(out, "\n");
            for(const auto& p : devices) {
                 if( nullptr != p ) {
                    jau_append_string(out, "  %s\n", p->toString());
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
    size_t i=0;
    jau::for_each_fidelity(statusListenerList, [&](StatusListenerPair &p) {
        try {
            p.listener->adapterSettingsChanged(*this, old_settings_, current_settings, changes, timestampMS);
        } catch (std::exception &e) {
            jau_ERR_PRINT("BTAdapter:CB:NewSettings-CBs %zu/%zu: %s of %s: Caught exception %s",
                i+1, statusListenerList.size(), p.listener->toString(), toString(), e.what());
        }
        i++;
    });
}

void BTAdapter::sendAdapterSettingsInitial(AdapterStatusListener & asl, const uint64_t timestampMS) noexcept
{
    const AdapterSetting current_settings = adapterInfo.getCurrentSettingMask();
    jau_COND_PRINT(debug_event, "BTAdapter::sendAdapterSettingsInitial: NONE -> %s, changes NONE: %s", current_settings, toString() );
    try {
        asl.adapterSettingsChanged(*this, AdapterSetting::NONE, current_settings, AdapterSetting::NONE, timestampMS);
    } catch (std::exception &e) {
        jau_ERR_PRINT("BTAdapter::sendAdapterSettingsChanged-CB: %s of %s: Caught exception %s", asl, toString(), e.what());
    }
}

void BTAdapter::sendDeviceUpdated(std::string cause, BTDeviceRef device, uint64_t timestamp, EIRDataType updateMask) noexcept {
    size_t i=0;
    jau::for_each_fidelity(statusListenerList, [&](StatusListenerPair &p) {
        try {
            if( p.match(device) ) {
                p.listener->deviceUpdated(device, updateMask, timestamp);
            }
        } catch (std::exception &e) {
            jau_ERR_PRINT("BTAdapter::sendDeviceUpdated-CBs (%s) %zu/%zu: %s of %s: Caught exception %s",
                cause, i+1, statusListenerList.size(), p.listener->toString(), device->toString(), e.what());
        }
        i++;
    });
}

// *************************************************

void BTAdapter::mgmtEvHCIAnyHCI(const MgmtEvent& e) noexcept {
    jau_DBG_PRINT("BTAdapter:hci::Any: %s", e);
    (void)e;
}
void BTAdapter::mgmtEvMgmtAnyMgmt(const MgmtEvent& e) noexcept {
    jau_DBG_PRINT("BTAdapter:mgmt:Any: %s", e);
    (void)e;
}

void BTAdapter::mgmtEvDeviceDiscoveringHCI(const MgmtEvent& e) noexcept {
    // jau_INFO_PRINT("BTAdapter:hci:DeviceDiscovering: %s", e);
    const MgmtEvtDiscovering &event = *static_cast<const MgmtEvtDiscovering *>(&e);
    mgmtEvDeviceDiscoveringAny(event.getScanType(), event.getEnabled(), event.getTimestamp(), EventSource::hci);
}

void BTAdapter::mgmtEvDeviceDiscoveringMgmt(const MgmtEvent& e) noexcept {
    // jau_INFO_PRINT("BTAdapter:mgmt:DeviceDiscovering: %s", e);
    const MgmtEvtDiscovering &event = *static_cast<const MgmtEvtDiscovering *>(&e);
    mgmtEvDeviceDiscoveringAny(event.getScanType(), event.getEnabled(), event.getTimestamp(), EventSource::mgmt);
}

void BTAdapter::updateDeviceDiscoveringState(const ScanType eventScanType, const bool eventEnabled) noexcept {
    mgmtEvDeviceDiscoveringAny(eventScanType, eventEnabled, jau::getCurrentMilliseconds(), EventSource::dbt);
}

void BTAdapter::mgmtEvDeviceDiscoveringAny(const ScanType eventScanType_, const bool eventEnabled, const uint64_t eventTimestamp,
                                           const EventSource source) noexcept {
    const std::lock_guard<std::mutex> lock(mtx_discoveringEvt); // RAII-style acquire and relinquish via destructor
    ScanType currentNativeScanType = hci.getCurrentScanType();

    ScanType eventScanType = eventScanType_;
    if( EventSource::mgmt == source && ScanType::NONE == eventScanType) {
        // Fix Kernel BlueZ MGMT bug, passing zero for Address_Type
        eventScanType = currentNativeScanType;
    }

    // FIXME: Respect BTAdapter::btMode, i.e. BTMode::BREDR, BTMode::LE or BTMode::DUAL to setup BREDR, LE or DUAL scanning!
    //
    // Also catches case where discovery changes w/o user interaction [start/stop]Discovery(..)
    // if sourced from mgmt channel (!hciSourced)

    const ScanType locCurrentMetaScanType = currentMetaScanType;
    ScanType nextMetaScanType;
    if( eventEnabled ) {
        // enabled eventScanType
        nextMetaScanType = changeScanType(locCurrentMetaScanType, eventScanType, true);
    } else {
        // disabled eventScanType
        if( is_set(eventScanType, ScanType::LE) && DiscoveryPolicy::AUTO_OFF != discovery_policy ) {
            // Unchanged meta for disabled-LE && keep_le_scan_alive
            nextMetaScanType = locCurrentMetaScanType;
        } else {
            nextMetaScanType = changeScanType(locCurrentMetaScanType, eventScanType, false);
        }
    }

    if( EventSource::hci != source ) {
        // update HCIHandler's currentNativeScanType from other source
        const ScanType nextNativeScanType = changeScanType(currentNativeScanType, eventScanType, eventEnabled);
        jau_DBG_PRINT("BTAdapter:%s:DeviceDiscovering: dev_id %u, policy %s: scanType[event[%s -> %s, enabled %s], native %s -> %s, meta %s -> %s])",
            source, dev_id, discovery_policy, eventScanType_, eventScanType, eventEnabled,
            currentNativeScanType, nextNativeScanType, locCurrentMetaScanType, nextMetaScanType);
        currentNativeScanType = nextNativeScanType;
        hci.setCurrentScanType(currentNativeScanType);
    } else {
        jau_DBG_PRINT("BTAdapter:%s:DeviceDiscovering: dev_id %u, policy %s: scanType[event[%s, enabled %s], native %s, meta %s -> %s])",
            source, dev_id, discovery_policy, eventScanType, eventEnabled,
            currentNativeScanType, locCurrentMetaScanType, nextMetaScanType);
    }
    currentMetaScanType = nextMetaScanType;
    if( isDiscovering() ) {
        btRole = BTRole::Master;
    }

    checkDiscoveryState();

    size_t i=0;
    jau::for_each_fidelity(statusListenerList, [&](StatusListenerPair &p) {
        try {
            p.listener->discoveringChanged(*this, currentMetaScanType, eventScanType, eventEnabled, discovery_policy, eventTimestamp);
        } catch (std::exception &except) {
            jau_ERR_PRINT("BTAdapter:%s:DeviceDiscovering-CBs %zu/%zu: %s of %s: Caught exception %s",
                source, i+1, statusListenerList.size(), p.listener->toString(), toString(), except.what());
        }
        i++;
    });

    if( !is_set(currentNativeScanType, ScanType::LE) &&
        DiscoveryPolicy::AUTO_OFF != discovery_policy &&
        0 == getDevicesPausingDiscoveryCount() )
    {
        discovery_service.start();
    }
}

void BTAdapter::mgmtEvNewSettingsMgmt(const MgmtEvent& e) noexcept {
    jau_COND_PRINT(debug_event, "BTAdapter:mgmt:NewSettings: %s", e);
    const MgmtEvtNewSettings &event = *static_cast<const MgmtEvtNewSettings *>(&e);
    const AdapterSetting new_settings = adapterInfo.setCurrentSettingMask(event.getSettings()); // probably done by mgmt callback already

    updateAdapterSettings(true /* off_thread */, new_settings, true /* sendEvent */, event.getTimestamp());
}

void BTAdapter::updateAdapterSettings(const bool off_thread, const AdapterSetting new_settings, const bool sendEvent, const uint64_t timestamp) noexcept {
    const AdapterSetting old_settings_ = old_settings;

    const AdapterSetting changes = getAdapterSettingMaskDiff(new_settings, old_settings_);

    const bool justPoweredOn = isAdapterSettingBitSet(changes, AdapterSetting::POWERED) &&
                               isAdapterSettingBitSet(new_settings, AdapterSetting::POWERED);

    const bool justPoweredOff = isAdapterSettingBitSet(changes, AdapterSetting::POWERED) &&
                                !isAdapterSettingBitSet(new_settings, AdapterSetting::POWERED);

    old_settings = new_settings;

    jau_COND_PRINT(debug_event, "BTAdapter::updateAdapterSettings: %s -> %s, changes %s: %s, sendEvent %d, offThread %d",
        old_settings_, new_settings, changes, toString(), sendEvent, off_thread ); // NOLINT(clang-analyzer-optin.cplusplus.VirtualCall)

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
            std::thread bg(&BTAdapter::poweredOff, this, false, "adapter_settings.0"); // @suppress("Invalid arguments")
            bg.detach();
        } else {
            poweredOff(false, "powered_off.1");
        }
    }
}

void BTAdapter::mgmtEvLocalNameChangedMgmt(const MgmtEvent& e) noexcept {
    jau_COND_PRINT(debug_event, "BTAdapter:mgmt:LocalNameChanged: %s", e);
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
    jau_COND_PRINT(debug_event, "BTAdapter:mgmt:LocalNameChanged: Local name: %d: '%s' -> '%s'; short_name: %d: '%s' -> '%s'",
        nameChanged, old_name, getName(), shortNameChanged, old_shortName, getShortName());
}

void BTAdapter::l2capServerInit(jau::service_runner& sr0) noexcept {
    (void)sr0;

    l2cap_att_srv.set_interrupted_query( jau::bind_member(&l2cap_service, &jau::service_runner::shall_stop2) );

    if( !l2cap_att_srv.open() ) {
        jau_ERR_PRINT("Adapter[%d]: L2CAP ATT open failed: %s", dev_id, l2cap_att_srv);
    }
}

void BTAdapter::l2capServerEnd(jau::service_runner& sr) noexcept {
    (void)sr;
    if( !l2cap_att_srv.close() ) {
        jau_ERR_PRINT("Adapter[%d]: L2CAP ATT close failed: %s", dev_id, l2cap_att_srv);
    }
}

void BTAdapter::l2capServerWork(jau::service_runner& sr) noexcept {
    (void)sr;
    std::unique_ptr<L2CAPClient> l2cap_att_ = l2cap_att_srv.accept();
    if( BTRole::Slave == getRole() && nullptr != l2cap_att_ && l2cap_att_->getRemoteAddressAndType().isLEAddress() ) {
        jau_DBG_PRINT("L2CAP-ACCEPT: BTAdapter::l2capServer connected.1: (public) %s", l2cap_att_->toString());

        std::unique_lock<std::mutex> lock(mtx_l2cap_att); // RAII-style acquire and relinquish via destructor
        l2cap_att = std::move( l2cap_att_ );
        lock.unlock(); // unlock mutex before notify_all to avoid pessimistic re-block of notified wait() thread.
        cv_l2cap_att.notify_all(); // notify waiting getter

    } else if( nullptr != l2cap_att_ ) {
        jau_DBG_PRINT("L2CAP-ACCEPT: BTAdapter::l2capServer connected.2: (ignored) %s", l2cap_att_->toString());
    } else {
        jau_DBG_PRINT("L2CAP-ACCEPT: BTAdapter::l2capServer connected.0: nullptr");
    }
}

std::unique_ptr<L2CAPClient> BTAdapter::get_l2cap_connection(const std::shared_ptr<BTDevice>& device) {
    if( BTRole::Slave != getRole() ) {
        jau_DBG_PRINT("L2CAP-ACCEPT: BTAdapter:get_l2cap_connection(dev_id %u): Not in server mode", dev_id);
        return nullptr;
    }
    const jau::fraction_i64 timeout = L2CAP_CLIENT_CONNECT_TIMEOUT_MS;

    std::unique_lock<std::mutex> lock(mtx_l2cap_att); // RAII-style acquire and relinquish via destructor
    const jau::fraction_timespec timeout_time = jau::getMonotonicTime() + jau::fraction_timespec(timeout);
    while( device->getConnected() && ( nullptr == l2cap_att || l2cap_att->getRemoteAddressAndType() != device->getAddressAndType() ) ) {
        std::cv_status s = wait_until(cv_l2cap_att, lock, timeout_time);
        if( std::cv_status::timeout == s && ( nullptr == l2cap_att || l2cap_att->getRemoteAddressAndType() != device->getAddressAndType() ) ) {
            jau_DBG_PRINT("L2CAP-ACCEPT: BTAdapter:get_l2cap_connection(dev_id %u): l2cap_att TIMEOUT", dev_id);
            return nullptr;
        }
    }
    if( nullptr != l2cap_att && l2cap_att->getRemoteAddressAndType() == device->getAddressAndType() ) {
        std::unique_ptr<L2CAPClient> l2cap_att_ = std::move( l2cap_att );
        jau_DBG_PRINT("L2CAP-ACCEPT: BTAdapter:get_l2cap_connection(dev_id %u): Accept: l2cap_att %s", dev_id, l2cap_att_->toString());
        return l2cap_att_; // copy elision
    } else if( nullptr != l2cap_att ) {
        std::unique_ptr<L2CAPClient> l2cap_att_ = std::move( l2cap_att );
        jau_DBG_PRINT("L2CAP-ACCEPT: BTAdapter:get_l2cap_connection(dev_id %u): Ignore: l2cap_att %s", dev_id, l2cap_att_->toString());
        return nullptr;
    } else {
        jau_DBG_PRINT("L2CAP-ACCEPT: BTAdapter:get_l2cap_connection(dev_id %u): Null: Might got disconnected", dev_id);
        return nullptr;
    }
}

jau::fraction_i64 BTAdapter::smp_timeoutfunc(jau::simple_timer& timer) {
    if( timer.shall_stop() ) {
        return 0_s;
    }
    device_list_t failed_devices;
    {
        const bool useServerAuth = BTRole::Slave == getRole() &&
                                   BTSecurityLevel::ENC_AUTH <= sec_level_server &&
                                   hasSMPIOCapabilityAnyIO(io_cap_server);

        const std::lock_guard<std::mutex> lock(mtx_connectedDevices); // RAII-style acquire and relinquish via destructor
        jau::for_each_fidelity(connectedDevices, [&](BTDeviceRef& device) {
            if( device->isValidInstance() && device->getConnected() &&
                !useServerAuth &&
                BTSecurityLevel::NONE < device->getConnSecurityLevel() &&
                SMPPairingState::KEY_DISTRIBUTION == device->pairing_data.state )
                // isSMPPairingActive( device->pairing_data.state ) &&
                // !isSMPPairingUserInteraction( device->pairing_data.state ) )
            {
                // actively within SMP negotiations, excluding user interaction
                const uint32_t smp_events = device->smp_events;
                if( 0 == smp_events ) {
                    jau_DBG_PRINT("BTAdapter::smp_timeoutfunc(dev_id %u): SMP Timeout: Pairing-Failed %u: %s", dev_id, smp_events, device->toString());
                    failed_devices.push_back(device);
                } else {
                    jau_DBG_PRINT("BTAdapter::smp_timeoutfunc(dev_id %u): SMP Timeout: Ignore-2 %u -> 0: %s", dev_id, smp_events, device->toString());
                    device->smp_events = 0;
                }
            } else {
                const uint32_t smp_events = device->smp_events;
                if( 0 < smp_events ) {
                    jau_DBG_PRINT("BTAdapter::smp_timeoutfunc(dev_id %u): SMP Timeout: Ignore-1 %u: %s", dev_id, smp_events, device->toString());
                    device->smp_events = 0;
                }
            }
        });
    }
    jau::for_each_fidelity(failed_devices, [&](BTDeviceRef& device) {
        const bool smp_auto = device->isConnSecurityAutoEnabled();
        jau_IRQ_PRINT("BTAdapter(dev_id %u): SMP Timeout: Start: smp_auto %d, %s", dev_id, smp_auto, device->toString());
        const SMPPairFailedMsg msg(SMPPairFailedMsg::ReasonCode::UNSPECIFIED_REASON);
        const L2CapFrame source(device->getConnectionHandle(),
                L2CapFrame::PBFlag::START_NON_AUTOFLUSH_HOST, 0 /* bc_flag */,
                L2CAP_CID::SMP, L2CAP_PSM::UNDEFINED, 0 /* len */);
        // BTAdapter::sendDevicePairingState() will delete device-key if server and issue disconnect if !smp_auto
        device->hciSMPMsgCallback(device, msg, source);
        jau_DBG_PRINT("BTAdapter::smp_timeoutfunc(dev_id %u): SMP Timeout: Done: smp_auto %d, %s", dev_id, smp_auto, device->toString());
    });
    return timer.shall_stop() ? 0_s : SMP_NEXT_EVENT_TIMEOUT_MS; // keep going until BTAdapter closes
}

void BTAdapter::mgmtEvDeviceConnectedHCI(const MgmtEvent& e) {
    const MgmtEvtDeviceConnected &event = *static_cast<const MgmtEvtDeviceConnected *>(&e);
    EInfoReport ad_report;
    {
        ad_report.setSource(EInfoReport::Source::EIR, false);
        ad_report.setTimestamp(event.getTimestamp());
        ad_report.setAddressType(event.getAddressType());
        ad_report.setAddress( event.getAddress() );
        ad_report.read_data(event.getData(), event.getDataSize());
    }
    jau_DBG_PRINT("BTAdapter::mgmtEvDeviceConnectedHCI(dev_id %u): Event %s, AD EIR %s", dev_id, e, ad_report.toString(true));

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

    jau_DBG_PRINT("BTAdapter:hci:DeviceConnected(dev_id %u): state[role %s, new %d, discovered %d, unpair %d, has_keys %d], %s: %s",
        dev_id, getRole(), new_connect, device_discovered, slave_unpair, has_smp_keys, e, ad_report);

    if( slave_unpair ) {
        /**
         * Without unpair in SC mode (or key pre-pairing), the peripheral fails the DHKey Check.
         */
        if( !has_smp_keys ) {
            // No pre-pairing -> unpair
            HCIStatusCode  res = mgmt->unpairDevice(dev_id, device->getAddressAndType(), false /* disconnect */);
            if( HCIStatusCode::SUCCESS != res && HCIStatusCode::NOT_PAIRED != res ) {
                jau_WARN_PRINT("(dev_id %u, new_connect %d): Unpair device failed %s of %s",
                    dev_id, new_connect, res, device->getAddressAndType());
            }
        }
    }

    const SMPIOCapability io_cap_has = mgmt->getIOCapability(dev_id);

    EIRDataType updateMask = device->update(ad_report);
    if( 0 == new_connect ) {
        jau_WARN_PRINT("(dev_id %u, already connected, updated %s): %s, handle %#x -> %#x,\n    %s,\n    -> %s",
            dev_id, updateMask, event, device->getConnectionHandle(), event.getHCIHandle(), ad_report, device->toString());
    } else {
        addConnectedDevice(device); // track device, if not done yet
        jau_COND_PRINT(debug_event, "BTAdapter::hci:DeviceConnected(dev_id %u, new_connect %d, updated %s): %s, handle %#x -> %#x,\n    %s,\n    -> %s",
            dev_id, new_connect, updateMask, event, device->getConnectionHandle(), event.getHCIHandle(), ad_report, device->toString());
    }

    if( BTRole::Slave == getRole() ) {
        // filters and sets device->pairing_data.{sec_level_user, ioCap_user}
        device->setConnSecurity(sec_level_server, io_cap_server);
    }
    device->notifyConnected(device, event.getHCIHandle(), io_cap_has);

    if( device->isConnSecurityAutoEnabled() ) {
        new_connect = 0; // disable deviceConnected() events for BTRole::Master for SMP-Auto
    }

    size_t i=0;
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
            jau_ERR_PRINT("BTAdapter::hci:DeviceConnected-CBs %zu/%zu: %s of %s: Caught exception %s",
                i+1, statusListenerList.size(), p.listener->toString(), device->toString(), except.what());
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
}
void BTAdapter::mgmtEvDeviceConnectedMgmt(const MgmtEvent& e) noexcept {
    const MgmtEvtDeviceConnected &event = *static_cast<const MgmtEvtDeviceConnected *>(&e);
    EInfoReport ad_report;
    {
        ad_report.setSource(EInfoReport::Source::EIR, false);
        ad_report.setTimestamp(event.getTimestamp());
        ad_report.setAddressType(event.getAddressType());
        ad_report.setAddress( event.getAddress() );
        ad_report.read_data(event.getData(), event.getDataSize());
    }
    jau_DBG_PRINT("BTAdapter::mgmtEvDeviceConnectedMgmt(dev_id %u): Event %s, AD EIR %s", dev_id, e, ad_report.toString(true));
}

void BTAdapter::mgmtEvConnectFailedHCI(const MgmtEvent& e) noexcept {
    const MgmtEvtDeviceConnectFailed &event = *static_cast<const MgmtEvtDeviceConnectFailed *>(&e);

    BTDeviceRef device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr != device ) {
        const uint16_t handle = device->getConnectionHandle();
        jau_DBG_PRINT("BTAdapter::hci:ConnectFailed(dev_id %u): %s, handle %s -> zero,\n    -> %s",
            dev_id, event, jau::toHexString(handle), device->toString());

        unlockConnect(*device);
        device->notifyDisconnected();
        removeConnectedDevice(*device);

        if( !device->isConnSecurityAutoEnabled() ) {
            size_t i=0;
            jau::for_each_fidelity(statusListenerList, [&](StatusListenerPair &p) {
                try {
                    if( p.match(device) ) {
                        p.listener->deviceDisconnected(device, event.getHCIStatus(), handle, event.getTimestamp());
                    }
                } catch (std::exception &except) {
                    jau_ERR_PRINT("BTAdapter::hci:DeviceDisconnected-CBs %zu/%zu: %s of %s: Caught exception %s",
                        i+1, statusListenerList.size(), p.listener->toString(), device->toString(), except.what());
                }
                i++;
            });
            device->clearData();
            removeDiscoveredDevice(device->addressAndType); // ensure device will cause a deviceFound event after disconnect
        }
    } else {
        jau_WORDY_PRINT("BTAdapter::hci:DeviceDisconnected(dev_id %u): Device not tracked: %s", dev_id, event);
    }
}

void BTAdapter::mgmtEvHCILERemoteUserFeaturesHCI(const MgmtEvent& e) {
    const MgmtEvtHCILERemoteFeatures &event = *static_cast<const MgmtEvtHCILERemoteFeatures *>(&e);

    BTDeviceRef device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr != device ) {
        jau_COND_PRINT(debug_event, "BTAdapter::hci:LERemoteUserFeatures(dev_id %u): %s, %s", dev_id, event, device->toString());

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
        jau_WORDY_PRINT("BTAdapter::hci:LERemoteUserFeatures(dev_id %u): Device not tracked: %s", dev_id, event);
    }
}

void BTAdapter::mgmtEvHCILEPhyUpdateCompleteHCI(const MgmtEvent& e) noexcept {
    const MgmtEvtHCILEPhyUpdateComplete &event = *static_cast<const MgmtEvtHCILEPhyUpdateComplete *>(&e);

    BTDeviceRef device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr != device ) {
        jau_COND_PRINT(debug_event, "BTAdapter::hci:LEPhyUpdateComplete(dev_id %u): %s, %s", dev_id, event, device->toString());

        device->notifyLEPhyUpdateComplete(event.getHCIStatus(), event.getTx(), event.getRx());
    } else {
        jau_WORDY_PRINT("BTAdapter::hci:LEPhyUpdateComplete(dev_id %u): Device not tracked: %s", dev_id, event);
    }
}

void BTAdapter::mgmtEvDeviceDisconnectedHCI(const MgmtEvent& e) noexcept {
    const MgmtEvtDeviceDisconnected &event = *static_cast<const MgmtEvtDeviceDisconnected *>(&e);

    BTDeviceRef device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr != device ) {
        if( device->getConnectionHandle() != event.getHCIHandle() ) {
            jau_WORDY_PRINT("BTAdapter::hci:DeviceDisconnected(dev_id %u): ConnHandle mismatch %s\n    -> %s",
                dev_id, event, device->toString());
            return;
        }
        jau_DBG_PRINT("BTAdapter::hci:DeviceDisconnected(dev_id %u): %s, handle %#x -> zero,\n    -> %s",
            dev_id, event, event.getHCIHandle(), device->toString());

        unlockConnect(*device);
        device->notifyDisconnected(); // -> unpair()
        removeConnectedDevice(*device);
        if( BTRole::Slave == btRole ) {
            // Keep valid in LL master discovery mode (client).
            gattServerData = nullptr;
        }

        if( !device->isConnSecurityAutoEnabled() ) {
            size_t i=0;
            jau::for_each_fidelity(statusListenerList, [&](StatusListenerPair &p) {
                try {
                    if( p.match(device) ) {
                        p.listener->deviceDisconnected(device, event.getHCIReason(), event.getHCIHandle(), event.getTimestamp());
                    }
                } catch (std::exception &except) {
                    jau_ERR_PRINT("BTAdapter::hci:DeviceDisconnected-CBs %zu/%zu: %s of %s: Caught exception %s",
                        i+1, statusListenerList.size(), p.listener->toString(), device->toString(), except.what());
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
                            jau_WARN_PRINT("(dev_id %u): Unpair device failed %s of %s",
                                    dev_id, res, device->getAddressAndType());
                        }
                        res = device->uploadKeys(*key, BTSecurityLevel::NONE);
                    #else
                        res = device->uploadKeys(*key, BTSecurityLevel::NONE);
                    #endif
                    if( HCIStatusCode::SUCCESS != res ) {
                        jau_WARN_PRINT("(dev_id %u): Upload SMPKeyBin failed %s, %s (removing file)", dev_id, res, key->toString());
                        removeSMPKeyBin(device->getAddressAndType(), true /* remove_file */);
                    }
                }
            }
        }
        removeDevicePausingDiscovery(*device);
    } else {
        jau_DBG_PRINT("BTAdapter::hci:DeviceDisconnected(dev_id %u): Device not connected: %s", dev_id, event);
        if( _print_device_lists || jau::environment::get().verbose ) {
            printDeviceLists();
        }
        device = findDevicePausingDiscovery(event.getAddress(), event.getAddressType());
        if( nullptr != device ) {
            removeDevicePausingDiscovery(*device);
        }
    }
}

// Local BTRole::Slave
void BTAdapter::mgmtEvLELTKReqEventHCI(const MgmtEvent& e) noexcept {
    const MgmtEvtHCILELTKReq &event = *static_cast<const MgmtEvtHCILELTKReq *>(&e);

    BTDeviceRef device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr != device ) {
        // BT Core Spec v5.2: Vol 4, Part E HCI: 7.7.65.5 LE Long Term Key Request event
        device->updatePairingState(device, e, HCIStatusCode::SUCCESS, SMPPairingState::COMPLETED);
    } else {
        jau_WORDY_PRINT("BTAdapter::hci:LE_LTK_Request(dev_id %u): Device not tracked: %s", dev_id, event);
    }
}
void BTAdapter::mgmtEvLELTKReplyAckCmdHCI(const MgmtEvent& e) noexcept {
    const MgmtEvtHCILELTKReplyAckCmd &event = *static_cast<const MgmtEvtHCILELTKReplyAckCmd *>(&e);

    BTDeviceRef device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr != device ) {
        // BT Core Spec v5.2: Vol 4, Part E HCI: 7.8.25 LE Long Term Key Request Reply command
        device->updatePairingState(device, e, HCIStatusCode::SUCCESS, SMPPairingState::COMPLETED);
    } else {
        jau_WORDY_PRINT("BTAdapter::hci:LE_LTK_REPLY_ACK(dev_id %u): Device not tracked: %s", dev_id, event);
    }
}
void BTAdapter::mgmtEvLELTKReplyRejCmdHCI(const MgmtEvent& e) noexcept {
    const MgmtEvtHCILELTKReplyRejCmd &event = *static_cast<const MgmtEvtHCILELTKReplyRejCmd *>(&e);

    BTDeviceRef device = findConnectedDevice(event.getAddress(), event.getAddressType());
    jau_DBG_PRINT("BTAdapter::hci:LE_LTK_REPLY_REJ(dev_id %u): Ignored: %s (tracked %s)", dev_id, event, (nullptr!=device));
}

// Local BTRole::Master
void BTAdapter::mgmtEvLEEnableEncryptionCmdHCI(const MgmtEvent& e) noexcept {
    const MgmtEvtHCILEEnableEncryptionCmd &event = *static_cast<const MgmtEvtHCILEEnableEncryptionCmd *>(&e);

    BTDeviceRef device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr != device ) {
        // BT Core Spec v5.2: Vol 4, Part E HCI: 7.8.24 LE Enable Encryption command
        device->updatePairingState(device, e, HCIStatusCode::SUCCESS, SMPPairingState::COMPLETED);
    } else {
        jau_WORDY_PRINT("BTAdapter::hci:LE_ENABLE_ENC(dev_id %u): Device not tracked: %s", dev_id, event);
    }
}
// On BTRole::Master (reply to MgmtEvtHCILEEnableEncryptionCmd) and BTRole::Slave
void BTAdapter::mgmtEvHCIEncryptionChangedHCI(const MgmtEvent& e) noexcept {
    const MgmtEvtHCIEncryptionChanged &event = *static_cast<const MgmtEvtHCIEncryptionChanged *>(&e);

    BTDeviceRef device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr != device ) {
        // BT Core Spec v5.2: Vol 4, Part E HCI: 7.7.8 HCIEventType::ENCRYPT_CHANGE
        const HCIStatusCode evtStatus = event.getHCIStatus();
        const bool ok = HCIStatusCode::SUCCESS == evtStatus && 0 != event.getEncEnabled();
        const SMPPairingState pstate = ok ? SMPPairingState::COMPLETED : SMPPairingState::FAILED;
        device->updatePairingState(device, e, evtStatus, pstate);
    } else {
        jau_WORDY_PRINT("BTAdapter::hci:ENC_CHANGED(dev_id %u): Device not tracked: %s", dev_id, event);
    }
}
// On BTRole::Master (reply to MgmtEvtHCILEEnableEncryptionCmd) and BTRole::Slave
void BTAdapter::mgmtEvHCIEncryptionKeyRefreshCompleteHCI(const MgmtEvent& e) noexcept {
    const MgmtEvtHCIEncryptionKeyRefreshComplete &event = *static_cast<const MgmtEvtHCIEncryptionKeyRefreshComplete *>(&e);

    BTDeviceRef device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr != device ) {
        // BT Core Spec v5.2: Vol 4, Part E HCI: 7.7.39 HCIEventType::ENCRYPT_KEY_REFRESH_COMPLETE
        const HCIStatusCode evtStatus = event.getHCIStatus();
        const bool ok = HCIStatusCode::SUCCESS == evtStatus;
        const SMPPairingState pstate = ok ? SMPPairingState::COMPLETED : SMPPairingState::FAILED;
        device->updatePairingState(device, e, evtStatus, pstate);
    } else {
        jau_WORDY_PRINT("BTAdapter::hci:ENC_KEY_REFRESH_COMPLETE(dev_id %u): Device not tracked: %s", dev_id, event);
    }
}

void BTAdapter::mgmtEvPairDeviceCompleteMgmt(const MgmtEvent& e) noexcept {
    const MgmtEvtPairDeviceComplete &event = *static_cast<const MgmtEvtPairDeviceComplete *>(&e);

    BTDeviceRef device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr != device ) {
        const HCIStatusCode evtStatus = to_HCIStatusCode( event.getStatus() );
        const bool ok = HCIStatusCode::ALREADY_PAIRED == evtStatus;
        const SMPPairingState pstate = ok ? SMPPairingState::COMPLETED : SMPPairingState::NONE;
        device->updatePairingState(device, e, evtStatus, pstate);
    } else {
        jau_WORDY_PRINT("BTAdapter::mgmt:PairDeviceComplete(dev_id %u): Device not tracked: %s", dev_id, event);
    }
}

void BTAdapter::mgmtEvNewLongTermKeyMgmt(const MgmtEvent& e) noexcept {
    const MgmtEvtNewLongTermKey& event = *static_cast<const MgmtEvtNewLongTermKey *>(&e);
    const MgmtLongTermKey& ltk_info = event.getLongTermKey();
    BTDeviceRef device = findConnectedDevice(ltk_info.address, ltk_info.address_type);
    if( nullptr != device ) {
        const bool ok = ltk_info.enc_size > 0 && ltk_info.key_type != MgmtLTKType::NONE;
        if( ok ) {
            device->updatePairingState(device, e, HCIStatusCode::SUCCESS, SMPPairingState::COMPLETED);
        } else {
            jau_WORDY_PRINT("BTAdapter::mgmt:NewLongTermKey(dev_id %u): Invalid LTK: %s", dev_id, event);
        }
    } else {
        jau_WORDY_PRINT("BTAdapter::mgmt:NewLongTermKey(dev_id %u): Device not tracked: %s", dev_id, event);
    }
}

void BTAdapter::mgmtEvNewLinkKeyMgmt(const MgmtEvent& e) noexcept {
    const MgmtEvtNewLinkKey& event = *static_cast<const MgmtEvtNewLinkKey *>(&e);
    const MgmtLinkKeyInfo& lk_info = event.getLinkKey();
    // lk_info.address_type might be wrongly reported by mgmt, i.e. BDADDR_BREDR, use any.
    BTDeviceRef device = findConnectedDevice(lk_info.address, BDAddressType::BDADDR_UNDEFINED);
    if( nullptr != device ) {
        const bool ok = lk_info.key_type != MgmtLinkKeyType::NONE;
        if( ok ) {
            device->updatePairingState(device, e, HCIStatusCode::SUCCESS, SMPPairingState::COMPLETED);
        } else {
            jau_WORDY_PRINT("BTAdapter::mgmt:NewLinkKey(dev_id %u): Invalid LK: %s",
                dev_id, event);
        }
    } else {
        jau_WORDY_PRINT("BTAdapter::mgmt:NewLinkKey(dev_id %u): Device not tracked: %s",
            dev_id, event);
    }
}

void BTAdapter::mgmtEvNewIdentityResolvingKeyMgmt(const MgmtEvent& e) noexcept {
    const MgmtEvtNewIdentityResolvingKey& event = *static_cast<const MgmtEvtNewIdentityResolvingKey *>(&e);
    const EUI48& randomAddress = event.getRandomAddress();
    const MgmtIdentityResolvingKey& irk = event.getIdentityResolvingKey();
    if( adapterInfo.addressAndType.address == irk.address && adapterInfo.addressAndType.type == irk.address_type ) {
        // setPrivacy ...
        visibleAddressAndType.address = randomAddress;
        visibleAddressAndType.type = BDAddressType::BDADDR_LE_RANDOM;
        visibleMACType = HCILEOwnAddressType::RESOLVABLE_OR_RANDOM;
        privacyIRK = irk;
        jau_WORDY_PRINT("BTAdapter::mgmt:NewIdentityResolvingKey(dev_id %u): Host Adapter: %s", dev_id, event);
    } else {
        BTDeviceRef device = findConnectedDevice(randomAddress, BDAddressType::BDADDR_UNDEFINED);
        if( nullptr != device ) {
            // Handled via SMP
            jau_WORDY_PRINT("BTAdapter::mgmt:NewIdentityResolvingKey(dev_id %u): Device found (Resolvable): %s, %s", dev_id, event, device->toString());
        } else {
            jau_WORDY_PRINT("BTAdapter::mgmt:NewIdentityResolvingKey(dev_id %u): Device not tracked: %s", dev_id, event);
        }
    }
}

void BTAdapter::mgmtEvDeviceFoundHCI(const MgmtEvent& e) {
    // jau_COND_PRINT(debug_event, "BTAdapter:hci:DeviceFound(dev_id %u): %s", dev_id, e);
    const MgmtEvtDeviceFound &deviceFoundEvent = *static_cast<const MgmtEvtDeviceFound *>(&e);

    const EInfoReport* eir = deviceFoundEvent.getEIR();
    if( nullptr == eir ) {
        // Sourced from Linux Mgmt, which we don't support
        jau_ABORT("BTAdapter:hci:DeviceFound: Not sourced from LE_ADVERTISING_REPORT: %s", deviceFoundEvent);
        return; // unreachable
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
        jau_DBG_PRINT("BTAdapter:hci:DeviceFound(1.0, dev_id %u): Discovered but already connected %s [discovered %s, shared %s] -> Drop(1) %s",
            dev_id, dev_connected->getAddressAndType(), (nullptr != dev_discovered), (nullptr != dev_shared), eir->toString());
        if( _print_device_lists || jau::environment::get().verbose ) {
            printDeviceLists();
        }
    } else if( nullptr == dev_discovered ) { // nullptr == dev_connected && nullptr == dev_discovered
        if( nullptr == dev_shared ) {
            //
            // All new discovered device
            //
            dev_shared = BTDevice::make_shared(*this, *eir);
            addDiscoveredDevice(dev_shared);
            addSharedDevice(dev_shared);
            jau_DBG_PRINT("BTAdapter:hci:DeviceFound(1.1, dev_id %u): New undiscovered/unshared %s -> deviceFound(..) %s",
                dev_id, dev_shared->getAddressAndType(), eir->toString());
            if( _print_device_lists || jau::environment::get().verbose ) {
                printDeviceLists();
            }

            {
                const HCIStatusCode res = mgmt->unpairDevice(dev_id, dev_shared->getAddressAndType(), false /* disconnect */);
                if( HCIStatusCode::SUCCESS != res && HCIStatusCode::NOT_PAIRED != res ) {
                    jau_WARN_PRINT("(dev_id %u): Unpair device failed %s of %s", dev_id, res, dev_shared->getAddressAndType());
                }
            }
            size_t i=0;
            bool device_used = false;
            jau::for_each_fidelity(statusListenerList, [&](StatusListenerPair &p) {
                try {
                    if( p.match(dev_shared) ) {
                        device_used = p.listener->deviceFound(dev_shared, eir->getTimestamp()) || device_used;
                    }
                } catch (std::exception &except) {
                    jau_ERR_PRINT("BTAdapter:hci:DeviceFound-CBs %zu/%zu: %s of %s: Caught exception %s",
                        i+1, statusListenerList.size(), p.listener->toString(), dev_shared->toString(), except.what());
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
            jau_DBG_PRINT("BTAdapter:hci:DeviceFound(1.2, dev_id %u): Undiscovered but shared %s -> deviceFound(..) [deviceUpdated(..)] %s",
                dev_id, dev_shared->getAddressAndType(), eir->toString());
            if( _print_device_lists || jau::environment::get().verbose ) {
                printDeviceLists();
            }

            if( !dev_shared->isPrePaired() ) {
                HCIStatusCode res = dev_shared->unpair();
                if( HCIStatusCode::SUCCESS != res && HCIStatusCode::NOT_PAIRED != res ) {
                    jau_WARN_PRINT("(dev_id %u): Unpair device failed: %s, %s", dev_id, res, dev_shared->getAddressAndType());
                }
            }
            size_t i=0;
            bool device_used = false;
            jau::for_each_fidelity(statusListenerList, [&](StatusListenerPair &p) {
                try {
                    if( p.match(dev_shared) ) {
                        device_used = p.listener->deviceFound(dev_shared, eir->getTimestamp()) || device_used;
                    }
                } catch (std::exception &except) {
                    jau_ERR_PRINT("BTAdapter:hci:DeviceFound: %zu/%zu: %s of %s: Caught exception %s",
                        i+1, statusListenerList.size(), p.listener->toString(), dev_shared->toString(), except.what());
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
                jau_DBG_PRINT("BTAdapter:hci:DeviceFound(2.1.1, dev_id %u): Discovered but unshared %s, name changed %s -> deviceFound(..) %s",
                    dev_id, dev_discovered->getAddressAndType(), updateMask, eir->toString());
                addSharedDevice(dev_discovered); // re-add to shared devices!
                if( _print_device_lists || jau::environment::get().verbose ) {
                    printDeviceLists();
                }
                size_t i=0;
                bool device_used = false;
                jau::for_each_fidelity(statusListenerList, [&](StatusListenerPair &p) {
                    try {
                        if( p.match(dev_discovered) ) {
                            device_used = p.listener->deviceFound(dev_discovered, eir->getTimestamp()) || device_used;
                        }
                    } catch (std::exception &except) {
                        jau_ERR_PRINT("BTAdapter:hci:DeviceFound: %zu/%zu: %s of %s: Caught exception %s",
                            i+1, statusListenerList.size(), p.listener->toString(), dev_discovered->toString(), except.what());
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
                jau_COND_PRINT(debug_event, "BTAdapter:hci:DeviceFound(2.1.2, dev_id %u): Discovered but unshared %s, no name change -> Drop(2) %s",
                    dev_id, dev_discovered->getAddressAndType(), eir->toString());
            }
        } else { // nullptr != dev_shared
            //
            // Discovered and shared device,
            // i.e. at least one deviceFound(..) returned true - interest/picked.
            //
            if( EIRDataType::NONE != updateMask ) {
                if( debug_event ) {
                    jau_PLAIN_PRINT(true, "BTAdapter:hci:DeviceFound(2.2.1, dev_id %u): Discovered and shared %s, updated %s -> deviceUpdated(..) %s",
                        dev_id, dev_shared->getAddressAndType(), updateMask, eir->toString());
                    if( _print_device_lists || jau::environment::get().verbose ) {
                        printDeviceLists();
                    }
                }
                sendDeviceUpdated("DiscoveredDeviceFound", dev_shared, eir->getTimestamp(), updateMask);
            } else {
                // Drop: No update
                if( debug_event ) {
                    jau_PLAIN_PRINT(true, "BTAdapter:hci:DeviceFound(2.2.2, dev_id %u): Discovered and shared %s, not-updated -> Drop(3) %s",
                        dev_id, dev_shared->getAddressAndType(), eir->toString());
                    if( _print_device_lists || jau::environment::get().verbose ) {
                        printDeviceLists();
                    }
                }
            }
        }
    }
}

void BTAdapter::mgmtEvDeviceUnpairedMgmt(const MgmtEvent& e) noexcept {
    const MgmtEvtDeviceUnpaired &event = *static_cast<const MgmtEvtDeviceUnpaired *>(&e);
    jau_DBG_PRINT("BTAdapter:mgmt:DeviceUnpaired: %s", event);
}
void BTAdapter::mgmtEvPinCodeRequestMgmt(const MgmtEvent& e) noexcept {
    const MgmtEvtPinCodeRequest &event = *static_cast<const MgmtEvtPinCodeRequest *>(&e);

    BTDeviceRef device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr == device ) {
        jau_WORDY_PRINT("BTAdapter:hci:SMP: dev_id %u: Device not tracked: address[%s, %s], %s",
            dev_id, event.getAddress(), event.getAddressType(), event);
        return;
    }
    jau_DBG_PRINT("BTAdapter:mgmt:PinCodeRequest: %s", event);
    // FIXME: device->updatePairingState(device, e, HCIStatusCode::SUCCESS, SMPPairingState::PASSKEY_EXPECTED);
}
void BTAdapter::mgmtEvAuthFailedMgmt(const MgmtEvent& e) noexcept {
    const MgmtEvtAuthFailed &event = *static_cast<const MgmtEvtAuthFailed *>(&e);

    BTDeviceRef device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr == device ) {
        jau_WORDY_PRINT("BTAdapter:hci:SMP: dev_id %u: Device not tracked: address[%s, %s], %s",
            dev_id, event.getAddress(), event.getAddressType(), event);
        return;
    }
    const HCIStatusCode evtStatus = to_HCIStatusCode( event.getStatus() );
    device->updatePairingState(device, e, evtStatus, SMPPairingState::FAILED);
}
void BTAdapter::mgmtEvUserConfirmRequestMgmt(const MgmtEvent& e) noexcept {
    const MgmtEvtUserConfirmRequest &event = *static_cast<const MgmtEvtUserConfirmRequest *>(&e);

    BTDeviceRef device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr == device ) {
        jau_WORDY_PRINT("BTAdapter:hci:SMP: dev_id %u: Device not tracked: address[%s, %s], %s",
            dev_id, event.getAddress(), event.getAddressType(), event);
        return;
    }
    // FIXME: Pass confirm_hint and value?
    jau_DBG_PRINT("BTAdapter:mgmt:UserConfirmRequest: %s", event);
    device->updatePairingState(device, e, HCIStatusCode::SUCCESS, SMPPairingState::NUMERIC_COMPARE_EXPECTED);
}
void BTAdapter::mgmtEvUserPasskeyRequestMgmt(const MgmtEvent& e) noexcept {
    const MgmtEvtUserPasskeyRequest &event = *static_cast<const MgmtEvtUserPasskeyRequest *>(&e);

    BTDeviceRef device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr == device ) {
        jau_WORDY_PRINT("BTAdapter:hci:SMP: dev_id %u: Device not tracked: address[%s, %s], %s",
            dev_id, event.getAddress(), event.getAddressType(), event);
        return;
    }
    jau_DBG_PRINT("BTAdapter:mgmt:UserPasskeyRequest: %s", event);
    device->updatePairingState(device, e, HCIStatusCode::SUCCESS, SMPPairingState::PASSKEY_EXPECTED);
}

void BTAdapter::mgmtEvPasskeyNotifyMgmt(const MgmtEvent& e) noexcept {
    const MgmtEvtPasskeyNotify &event = *static_cast<const MgmtEvtPasskeyNotify *>(&e);

    BTDeviceRef device = findConnectedDevice(event.getAddress(), event.getAddressType());
    if( nullptr == device ) {
        jau_WORDY_PRINT("BTAdapter:hci:SMP: dev_id %u: Device not tracked: address[%s, %s], %s",
            dev_id, event.getAddress(), event.getAddressType(), event);
        return;
    }
    jau_DBG_PRINT("BTAdapter:mgmt:PasskeyNotify: %s", event);
    device->updatePairingState(device, e, HCIStatusCode::SUCCESS, SMPPairingState::PASSKEY_NOTIFY);
}

void BTAdapter::hciSMPMsgCallback(const BDAddressAndType & addressAndType,
                                   const SMPPDUMsg& msg, const L2CapFrame& source) noexcept {
    BTDeviceRef device = findConnectedDevice(addressAndType.address, addressAndType.type);
    if( nullptr == device ) {
        jau_WORDY_PRINT("BTAdapter:hci:SMP: dev_id %u: Device not tracked: address%s: %s, %s",
            dev_id, addressAndType, msg, source);
        return;
    }
    if( device->getConnectionHandle() != source.handle ) {
        jau_WORDY_PRINT("BTAdapter:hci:SMP: dev_id %u: ConnHandle mismatch address%s: %s, %s\n    -> %s",
            dev_id, addressAndType, msg, source, device->toString());
        return;
    }

    device->hciSMPMsgCallback(device, msg, source);
}

void BTAdapter::sendDevicePairingState(const BTDeviceRef& device, const SMPPairingState state, const PairingMode mode, uint64_t timestamp)
{
    if( BTRole::Slave == getRole() ) {
        // PERIPHERAL_ADAPTER_MANAGES_SMP_KEYS
        if( SMPPairingState::COMPLETED == state ) {
            // Pairing completed
            if( PairingMode::PRE_PAIRED != mode ) {
                // newly paired -> store keys
                SMPKeyBin key = SMPKeyBin::create(*device);
                if( key.isValid() ) {
                    jau_DBG_PRINT("sendDevicePairingState (dev_id %u): created SMPKeyBin: %s", dev_id, key);
                    addSMPKeyBin( std::make_shared<SMPKeyBin>(key), true /* write_file */ );
                } else {
                    jau_WARN_PRINT("(dev_id %u): created SMPKeyBin invalid: %s", dev_id, key);
                }
            } else {
                // pre-paired, refresh PairingData of BTDevice (perhaps a new instance)
                const SMPKeyBinRef key = findSMPKeyBin( device->getAddressAndType() );
                if( nullptr != key ) {
                    bool res = device->setSMPKeyBin(*key);
                    if( !res ) {
                        jau_WARN_PRINT("(dev_id %u): device::setSMPKeyBin() failed %d, %s", dev_id, res, key->toString());
                    }
                }
            }
        } else if( SMPPairingState::FAILED == state ) {
            // Pairing failed on this server side
            removeSMPKeyBin(device->getAddressAndType(), true /* remove_file */);
        }
    }
    size_t i=0;
    jau::for_each_fidelity(statusListenerList, [&](StatusListenerPair &p) {
        try {
            if( p.match(device) ) {
                p.listener->devicePairingState(device, state, mode, timestamp);
            }
        } catch (std::exception &except) {
            jau_ERR_PRINT("BTAdapter::sendDevicePairingState: %zu/%zu: %s of %s: Caught exception %s",
                i+1, statusListenerList.size(), p.listener->toString(), device->toString(), except.what());
        }
        i++;
    });
    if( SMPPairingState::FAILED == state && !device->isConnSecurityAutoEnabled() ) {
        // Don't rely on receiving a disconnect
        std::thread dc(&BTDevice::disconnect, device.get(), HCIStatusCode::AUTHENTICATION_FAILURE);
        dc.detach();
    }
}

void BTAdapter::notifyPairingStageDone(const BTDeviceRef& device, uint64_t timestamp) noexcept {
    if( DiscoveryPolicy::PAUSE_CONNECTED_UNTIL_PAIRED == discovery_policy ) {
        removeDevicePausingDiscovery(*device);
    }
    (void)timestamp;
}

void BTAdapter::sendDeviceReady(BTDeviceRef device, uint64_t timestamp) noexcept {
    if( DiscoveryPolicy::PAUSE_CONNECTED_UNTIL_READY == discovery_policy ) {
        removeDevicePausingDiscovery(*device);
    }
    size_t i=0;
    jau::for_each_fidelity(statusListenerList, [&](StatusListenerPair &p) {
        try {
            // Only issue if valid && received connected confirmation (HCI) && not have called disconnect yet.
            if( device->isValidInstance() && device->getConnected() && device->allowDisconnect ) {
                if( p.match(device) ) {
                    p.listener->deviceReady(device, timestamp);
                }
            }
        } catch (std::exception &except) {
            jau_ERR_PRINT("BTAdapter::sendDeviceReady: %zu/%zu: %s of %s: Caught exception %s",
                i+1, statusListenerList.size(), p.listener->toString(), device->toString(), except.what());
        }
        i++;
    });
}
