/*
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2021 Gothel Software e.K.
 * Copyright (c) 2021 ZAFENA AB
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
#include <fstream>
#include <iostream>

#include <cinttypes>

#include <pthread.h>
#include <csignal>

#include <jau/cpp_lang_util.hpp>
#include <jau/dfa_utf8_decode.hpp>
#include <jau/basic_algos.hpp>
#include <jau/darray.hpp>

#include <direct_bt/DirectBT.hpp>

extern "C" {
    #include <unistd.h>
}

#include "dbt_constants.hpp"

using namespace direct_bt;
using namespace jau;

/** \file
 * _dbt_test_gattcharlifecycle01_ is a test program
 * to validate the proper lifecycle of BTGattHandler and BTGattChar notify/indication listener
 * including their off-thread usage within BTGattHandler::l2capReaderThreadImpl().
 *
 * This shall become a life BT unit test with full BTRole::Slave support.
 */

static uint64_t timestamp_t0;

static EUI48 useAdapter = EUI48::ALL_DEVICE;
static BTMode btMode = BTMode::DUAL;
static std::shared_ptr<BTAdapter> chosenAdapter = nullptr;

static std::atomic<int> deviceReadyCount = 0;

static std::atomic<int> MULTI_MEASUREMENTS = 8;

static void connectDiscoveredDevice(std::shared_ptr<BTDevice> device);

static void processReadyDevice(std::shared_ptr<BTDevice> device);

static bool startDiscovery(BTAdapter *a, std::string msg);

class MyAdapterStatusListener : public AdapterStatusListener {

    void adapterSettingsChanged(BTAdapter &a, const AdapterSetting oldmask, const AdapterSetting newmask,
                                const AdapterSetting changedmask, const uint64_t timestamp) override {
        const bool initialSetting = AdapterSetting::NONE == oldmask;
        if( initialSetting ) {
            fprintf_td(stderr, "****** SETTINGS_INITIAL: %s -> %s, changed %s\n", to_string(oldmask).c_str(),
                    to_string(newmask).c_str(), to_string(changedmask).c_str());
        } else {
            fprintf_td(stderr, "****** SETTINGS_CHANGED: %s -> %s, changed %s\n", to_string(oldmask).c_str(),
                    to_string(newmask).c_str(), to_string(changedmask).c_str());
        }
        fprintf_td(stderr, "Status BTAdapter:\n");
        fprintf_td(stderr, "%s\n", a.toString().c_str());
        (void)timestamp;

        if( !initialSetting &&
            isAdapterSettingBitSet(changedmask, AdapterSetting::POWERED) &&
            isAdapterSettingBitSet(newmask, AdapterSetting::POWERED) )
        {
            std::thread sd(::startDiscovery, &a, "powered-on"); // @suppress("Invalid arguments")
            sd.detach();
        }
    }

    void discoveringChanged(BTAdapter &a, const ScanType currentMeta, const ScanType changedType, const bool changedEnabled, const bool keepAlive, const uint64_t timestamp) override {
        fprintf_td(stderr, "****** DISCOVERING: meta %s, changed[%s, enabled %d, keepAlive %d]: %s\n",
                to_string(currentMeta).c_str(), to_string(changedType).c_str(), changedEnabled, keepAlive, a.toString().c_str());
        (void)timestamp;
    }

    bool deviceFound(std::shared_ptr<BTDevice> device, const uint64_t timestamp) override {
        (void)timestamp;

        if( !BTDeviceRegistry::isDeviceProcessing( device->getAddressAndType() ) &&
            ( !BTDeviceRegistry::isWaitingForAnyDevice() ||
              ( BTDeviceRegistry::isWaitingForDevice(device->getAddressAndType().address, device->getName()) &&
                ( 0 < MULTI_MEASUREMENTS || !BTDeviceRegistry::isDeviceProcessed(device->getAddressAndType()) )
              )
            )
          )
        {
            fprintf_td(stderr, "****** FOUND__-0: Connecting %s\n", device->toString(true).c_str());
            {
                const uint64_t td = getCurrentMilliseconds() - timestamp_t0; // adapter-init -> now
                fprintf_td(stderr, "PERF: adapter-init -> FOUND__-0  %" PRIu64 " ms\n", td);
            }
            std::thread dc(::connectDiscoveredDevice, device); // @suppress("Invalid arguments")
            dc.detach();
            return true;
        } else {
            fprintf_td(stderr, "****** FOUND__-1: NOP %s\n", device->toString(true).c_str());
            return false;
        }
    }

    void deviceUpdated(std::shared_ptr<BTDevice> device, const EIRDataType updateMask, const uint64_t timestamp) override {
        fprintf_td(stderr, "****** UPDATED: %s of %s\n", to_string(updateMask).c_str(), device->toString(true).c_str());
        (void)timestamp;
    }

    void deviceConnected(std::shared_ptr<BTDevice> device, const uint16_t handle, const uint64_t timestamp) override {
        fprintf_td(stderr, "****** CONNECTED: %s\n", device->toString(true).c_str());
        (void)handle;
        (void)timestamp;
    }

    void devicePairingState(std::shared_ptr<BTDevice> device, const SMPPairingState state, const PairingMode mode, const uint64_t timestamp) override {
        fprintf_td(stderr, "****** PAIRING STATE: state %s, mode %s, %s\n",
            to_string(state).c_str(), to_string(mode).c_str(), device->toString().c_str());
        (void)timestamp;
        switch( state ) {
            case SMPPairingState::NONE:
                // next: deviceReady(..)
                break;
            case SMPPairingState::FAILED: {
                const bool res  = SMPKeyBin::remove(KEY_PATH, *device);
                fprintf_td(stderr, "****** PAIRING_STATE: state %s; Remove key file %s, res %d\n",
                        to_string(state).c_str(), SMPKeyBin::getFilename(KEY_PATH, *device).c_str(), res);
                // next: deviceReady() or deviceDisconnected(..)
            } break;
            case SMPPairingState::REQUESTED_BY_RESPONDER:
                // next: FEATURE_EXCHANGE_STARTED
                break;
            case SMPPairingState::FEATURE_EXCHANGE_STARTED:
                // next: FEATURE_EXCHANGE_COMPLETED
                break;
            case SMPPairingState::FEATURE_EXCHANGE_COMPLETED:
                // next: PASSKEY_EXPECTED... or KEY_DISTRIBUTION
                break;
            case SMPPairingState::PASSKEY_EXPECTED: {
                const BTSecurityRegistry::Entry* sec = BTSecurityRegistry::getStartOf(device->getAddressAndType().address, "");
                if( nullptr != sec && sec->getPairingPasskey() != BTSecurityRegistry::Entry::NO_PASSKEY ) {
                    std::thread dc(&BTDevice::setPairingPasskey, device, static_cast<uint32_t>( sec->getPairingPasskey() ));
                    dc.detach();
                } else {
                    std::thread dc(&BTDevice::setPairingPasskey, device, 0);
                    // 3s disconnect: std::thread dc(&BTDevice::setPairingPasskeyNegative, device);
                    dc.detach();
                }
                // next: KEY_DISTRIBUTION or FAILED
              } break;
            case SMPPairingState::NUMERIC_COMPARE_EXPECTED: {
                const BTSecurityRegistry::Entry* sec = BTSecurityRegistry::getStartOf(device->getAddressAndType().address, "");
                if( nullptr != sec ) {
                    std::thread dc(&BTDevice::setPairingNumericComparison, device, sec->getPairingNumericComparison());
                    dc.detach();
                } else {
                    std::thread dc(&BTDevice::setPairingNumericComparison, device, false);
                    dc.detach();
                }
                // next: KEY_DISTRIBUTION or FAILED
              } break;
            case SMPPairingState::OOB_EXPECTED:
                // FIXME: ABORT
                break;
            case SMPPairingState::KEY_DISTRIBUTION:
                // next: COMPLETED or FAILED
                break;
            case SMPPairingState::COMPLETED:
                // next: deviceReady(..)
                break;
            default: // nop
                break;
        }
    }

    void deviceReady(std::shared_ptr<BTDevice> device, const uint64_t timestamp) override {
        (void)timestamp;
        if( !BTDeviceRegistry::isDeviceProcessing( device->getAddressAndType() ) &&
            ( !BTDeviceRegistry::isWaitingForAnyDevice() ||
              ( BTDeviceRegistry::isWaitingForDevice(device->getAddressAndType().address, device->getName()) &&
                ( 0 < MULTI_MEASUREMENTS || !BTDeviceRegistry::isDeviceProcessed(device->getAddressAndType()) )
              )
            )
          )
        {
            deviceReadyCount++;
            fprintf_td(stderr, "****** READY-0: Processing[%d] %s\n", deviceReadyCount.load(), device->toString(true).c_str());
            BTDeviceRegistry::addToProcessingDevices(device->getAddressAndType(), device->getName());
            processReadyDevice(device); // AdapterStatusListener::deviceReady() explicitly allows prolonged and complex code execution!
        } else {
            fprintf_td(stderr, "****** READY-1: NOP %s\n", device->toString(true).c_str());
        }
    }

    void deviceDisconnected(std::shared_ptr<BTDevice> device, const HCIStatusCode reason, const uint16_t handle, const uint64_t timestamp) override {
        fprintf_td(stderr, "****** DISCONNECTED: Reason 0x%X (%s), old handle %s: %s\n",
                static_cast<uint8_t>(reason), to_string(reason).c_str(),
                to_hexstring(handle).c_str(), device->toString(true).c_str());
        (void)timestamp;

        BTDeviceRegistry::removeFromProcessingDevices(device->getAddressAndType());
    }

    std::string toString() const override {
        return "MyAdapterStatusListener[this "+to_hexstring(this)+"]";
    }

};

class MyGATTEventListener : public BTGattChar::Listener {
  private:
    int i, j;
    std::shared_ptr<std::vector<uint8_t>> sink_ref;

  public:

    MyGATTEventListener(int i_, int j_, std::shared_ptr<std::vector<uint8_t>>& sink_ref_)
    : i(i_), j(j_), sink_ref(sink_ref_) {}

    void notificationReceived(BTGattCharRef charDecl, const TROOctets& char_value, const uint64_t timestamp) override {
        const uint64_t tR = getCurrentMilliseconds();
        fprintf_td(stderr, "**[%2.2d.%2.2d] Characteristic-Notify: UUID %s, td %" PRIu64 " ******\n",
                i, j, charDecl->value_type->toUUID128String().c_str(), (tR-timestamp));
        fprintf_td(stderr, "**[%2.2d.%2.2d]     Characteristic: %s ******\n", i, j, charDecl->toString().c_str());
        fprintf_td(stderr, "**[%2.2d.%2.2d]     Value R: %s ******\n", i, j, char_value.toString().c_str());
        sink_ref->insert(sink_ref->end(), char_value.get_ptr(), char_value.get_ptr()+char_value.size());
    }

    void indicationReceived(BTGattCharRef charDecl,
                            const TROOctets& char_value, const uint64_t timestamp,
                            const bool confirmationSent) override
    {
        const uint64_t tR = getCurrentMilliseconds();
        fprintf_td(stderr, "**[%2.2d.%2.2d] Characteristic-Indication: UUID %s, td %" PRIu64 ", confirmed %d ******\n",
                i, j, charDecl->value_type->toUUID128String().c_str(), (tR-timestamp), confirmationSent);
        fprintf_td(stderr, "**[%2.2d.%2.2d]     Characteristic: %s ******\n", i, j, charDecl->toString().c_str());
        fprintf_td(stderr, "**[%2.2d.%2.2d]     Value R: %s ******\n", i, j, char_value.toString().c_str());
        sink_ref->insert(sink_ref->end(), char_value.get_ptr(), char_value.get_ptr()+char_value.size());
    }
};

static void connectDiscoveredDevice(std::shared_ptr<BTDevice> device) {
    fprintf_td(stderr, "****** Connecting Device: Start %s\n", device->toString().c_str());

    // Testing listener lifecycle @ device dtor
    class TempAdapterStatusListener : public AdapterStatusListener {
        void deviceUpdated(std::shared_ptr<BTDevice> device, const EIRDataType updateMask, const uint64_t timestamp) override {
            fprintf_td(stderr, "****** UPDATED(2): %s of %s\n", to_string(updateMask).c_str(), device->toString(true).c_str());
            (void)timestamp;
        }
        void deviceConnected(std::shared_ptr<BTDevice> device, const uint16_t handle, const uint64_t timestamp) override {
            fprintf_td(stderr, "****** CONNECTED(2): %s\n", device->toString(true).c_str());
            (void)handle;
            (void)timestamp;
        }
        std::string toString() const override {
            return "TempAdapterStatusListener[this "+to_hexstring(this)+"]";
        }
    };
    device->addStatusListener(std::shared_ptr<AdapterStatusListener>(new TempAdapterStatusListener()));

    {
        const HCIStatusCode r = device->getAdapter().stopDiscovery();
        fprintf_td(stderr, "****** Connecting Device: stopDiscovery result %s\n", to_string(r).c_str());
    }

    const BTSecurityRegistry::Entry* sec = BTSecurityRegistry::getStartOf(device->getAddressAndType().address, device->getName());
    if( nullptr != sec ) {
        fprintf_td(stderr, "****** Connecting Device: Found SecurityDetail %s for %s\n", sec->toString().c_str(), device->toString().c_str());
    } else {
        fprintf_td(stderr, "****** Connecting Device: No SecurityDetail for %s\n", device->toString().c_str());
    }
    const BTSecurityLevel req_sec_level = nullptr != sec ? sec->getSecLevel() : BTSecurityLevel::UNSET;
    HCIStatusCode res = device->uploadKeys(KEY_PATH, req_sec_level, true /* verbose_ */);
    fprintf_td(stderr, "****** Connecting Device: BTDevice::uploadKeys(...) result %s\n", to_string(res).c_str());
    if( HCIStatusCode::SUCCESS != res ) {
        if( nullptr != sec ) {
            if( sec->isSecurityAutoEnabled() ) {
                bool r = device->setConnSecurityAuto( sec->getSecurityAutoIOCap() );
                fprintf_td(stderr, "****** Connecting Device: Using SecurityDetail.SEC AUTO %s, set OK %d\n", sec->toString().c_str(), r);
            } else if( sec->isSecLevelOrIOCapSet() ) {
                bool r = device->setConnSecurityBest( sec->getSecLevel(), sec->getIOCap() );
                fprintf_td(stderr, "****** Connecting Device: Using SecurityDetail.Level+IOCap %s, set OK %d\n", sec->toString().c_str(), r);
            } else {
                bool r = device->setConnSecurityAuto( SMPIOCapability::KEYBOARD_ONLY );
                fprintf_td(stderr, "****** Connecting Device: Setting SEC AUTO security detail w/ KEYBOARD_ONLY (%s) -> set OK %d\n", sec->toString().c_str(), r);
            }
        } else {
            bool r = device->setConnSecurityAuto( SMPIOCapability::KEYBOARD_ONLY );
            fprintf_td(stderr, "****** Connecting Device: Setting SEC AUTO security detail w/ KEYBOARD_ONLY -> set OK %d\n", r);
        }
    }

    res = device->connectDefault();
    fprintf_td(stderr, "****** Connecting Device: End result %s of %s\n", to_string(res).c_str(), device->toString().c_str());

    if( 0 == BTDeviceRegistry::getProcessingDeviceCount() && HCIStatusCode::SUCCESS != res ) {
        startDiscovery(&device->getAdapter(), "post-connect");
    }
}

// #define VERBOSE_DEBUG 1

struct GattCharAndListener {
    BTGattCharRef gattCharRef;
    std::shared_ptr<BTGattChar::Listener> listenerRef;
    std::shared_ptr<std::vector<uint8_t>> sinkRef;
};

static int addGattCharListener(std::shared_ptr<BTDevice> device, std::vector<GattCharAndListener>& gattCharAndListenerList) {
#if VERBOSE_DEBUG
    jau::INFO_PRINT("addGattCharListener: Start");
    device->getGattHandler()->printCharListener();
#endif

    int count = 0;
    try {
        jau::darray<BTGattServiceRef> primServices = device->getGattServices();
        if( 0 == primServices.size() ) {
            fprintf_td(stderr, "****** addGattCharListener(): getServices() failed %s\n", device->toString().c_str());
            return 0;
        }
        for(size_t i=0; i<primServices.size(); i++) {
            BTGattService & primService = *primServices.at(i);
            jau::darray<BTGattCharRef> & serviceCharacteristics = primService.characteristicList;
            for(size_t j=0; j<serviceCharacteristics.size(); j++) {
                BTGattCharRef & serviceChar = serviceCharacteristics.at(j);
                bool cccdEnableResult[2];
                std::shared_ptr<std::vector<uint8_t>> sinkRef = std::make_shared<std::vector<uint8_t>>();
                sinkRef->reserve(4096);
                std::shared_ptr<BTGattChar::Listener> cl = std::make_shared<MyGATTEventListener>(i, j, sinkRef);
                bool cccdRet = serviceChar->addCharListener( cl, cccdEnableResult );
                if( cccdRet ) {
#if VERBOSE_DEBUG
                    fprintf_td(stderr, "  [%2.2d] Service UUID %s\n", i, primService.type->toUUID128String().c_str());
                    fprintf_td(stderr, "  [%2.2d]         %s\n", i, primService.toString().c_str());
                    fprintf_td(stderr, "  [%2.2d.%2.2d] Characteristic: UUID %s\n", i, j, serviceChar->value_type->toUUID128String().c_str());
                    fprintf_td(stderr, "  [%2.2d.%2.2d]     %s\n", i, j, serviceChar->toString().c_str());
                    fprintf_td(stderr, "  [%2.2d.%2.2d] Characteristic-Listener(%zd): Notification(%d), Indication(%d): Added %d; %u charListener\n",
                            (int)i, (int)j, count, cccdEnableResult[0], cccdEnableResult[1], cccdRet, device->getGattHandler()->getCharListenerCount());
#endif
                    // Validate consistency of GATTHandler [add|remove]CharListener()
                    jau::INFO_PRINT("  [%2.2d.%2.2d] added %d: %p", i, j, count, cl.get());
                    gattCharAndListenerList.push_back( GattCharAndListener{ serviceChar, cl, sinkRef } );
                    count++;
                }
            }
        }
    } catch ( std::exception & e ) {
        fprintf_td(stderr, "****** addGattCharListener(): Exception caught for %s: %s\n", device->toString().c_str(), e.what());
    }

#if VERBOSE_DEBUG
    jau::INFO_PRINT("addGattCharListener: End");
    device->getGattHandler()->printCharListener();
#endif

    // Validate consistency of GATTHandler [add|remove]CharListener()
    const size_t totalCharListener = device->getGattHandler()->getCharListenerCount();
    if( totalCharListener != gattCharAndListenerList.size() ) {
        ERR_PRINT("Char-Listener %zu actual != %zu cached\n", totalCharListener, gattCharAndListenerList.size());
    }

    return count;
}

static int removeGattCharListener(std::shared_ptr<BTDevice> device, std::vector<GattCharAndListener>& gattCharAndListenerList) {
#if VERBOSE_DEBUG
    jau::INFO_PRINT("removeGattCharListener: Start");
    device->getGattHandler()->printCharListener();
#endif

    const int pre_count = device->getGattHandler()->getCharListenerCount();
    int count = 0;
    while( gattCharAndListenerList.size() > 0 ) {
        GattCharAndListener gcl = gattCharAndListenerList.back();
        jau::INFO_PRINT("[%d] remove: %p", count, gcl.listenerRef.get());
        gcl.gattCharRef->removeCharListener(gcl.listenerRef);
        gattCharAndListenerList.pop_back();
        count++;
    }
#if VERBOSE_DEBUG
    jau::INFO_PRINT("removeGattCharListener: End");
    device->getGattHandler()->printCharListener();
#endif

    // Validate consistency of GATTHandler [add|remove]CharListener()
    const size_t totalCharListener = device->getGattHandler()->getCharListenerCount();
    const size_t cached_count = gattCharAndListenerList.size();
    if( totalCharListener != cached_count ) {
        ERR_PRINT("Char-Listener (now) %zu actual (%zu pre) != %zu cached (%zu removed)\n",
                totalCharListener, pre_count, cached_count, count);
    }
    if( pre_count != count ) {
        ERR_PRINT("Char-Listener (rem) %zu pre != %zu removed\n", pre_count, count);
    }
    return count;
}

static void processReadyDevice(std::shared_ptr<BTDevice> device) {
    fprintf_td(stderr, "****** Processing Ready Device: Start %s\n", device->toString().c_str());
    device->getAdapter().stopDiscovery(); // make sure for pending connections on failed connect*(..) command

    SMPKeyBin::createAndWrite(*device, KEY_PATH, true /* verbose */);

    {
        LE_PHYs Tx { LE_PHYs::LE_2M }, Rx { LE_PHYs::LE_2M };
        HCIStatusCode res = device->setConnectedLE_PHY(Tx, Rx);
        fprintf_td(stderr, "****** Set Connected LE PHY: status %s: Tx %s, Rx %s\n",
                to_string(res).c_str(), to_string(Tx).c_str(), to_string(Rx).c_str());
    }
    {
        LE_PHYs resTx, resRx;
        HCIStatusCode res = device->getConnectedLE_PHY(resTx, resRx);
        fprintf_td(stderr, "****** Got Connected LE PHY: status %s: Tx %s, Rx %s\n",
                to_string(res).c_str(), to_string(resTx).c_str(), to_string(resRx).c_str());
    }

    // Validate consistency of GATTHandler [add|remove]CharListener()
    std::vector<GattCharAndListener> gattCharAndListenerList;

    while( 0 < MULTI_MEASUREMENTS ) {
        //
        // GATT Service Processing
        //
        int countCL_add = addGattCharListener(device, gattCharAndListenerList);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        int countCL_rem = removeGattCharListener(device, gattCharAndListenerList);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        --MULTI_MEASUREMENTS;

        fprintf_td(stderr, "****** Processing Ready Device: Left %d: GATT listener: %d added, %d removed, %s\n",
                MULTI_MEASUREMENTS.load(), countCL_add, countCL_rem, device->getAddressAndType().toString().c_str());
    }

    // Validate consistency of GATTHandler [add|remove]CharListener()
    const size_t totalCharListener = device->getGattHandler()->getCharListenerCount();
    if( totalCharListener != gattCharAndListenerList.size() ) {
        ERR_PRINT("Char-Listener %zu actual != %zu cached\n", totalCharListener, gattCharAndListenerList.size());
    }

    BTDeviceRegistry::removeFromProcessingDevices(device->getAddressAndType());

    if( 0 == BTDeviceRegistry::getProcessingDeviceCount() ) {
        startDiscovery(&device->getAdapter(), "post-processing-1");
    }

    fprintf_td(stderr, "****** Processing Ready Device: End-2: %s; devInProc %zu\n",
            device->toString().c_str(), BTDeviceRegistry::getProcessingDeviceCount());

    BTDeviceRegistry::addToProcessedDevices(device->getAddressAndType(), device->getName());

    // Validate consistency of GATTHandler [add|remove]CharListener()
    if( 0 != gattCharAndListenerList.size() ) {
        ERR_PRINT("Char-Listener not zero but %zu\n", gattCharAndListenerList.size());
    }
}

static bool le_scan_active = true; // default value
static const uint16_t le_scan_interval = 24; // default value
static const uint16_t le_scan_window = 24; // default value
static const uint8_t filter_policy = 0; // default value

static bool startDiscovery(BTAdapter *a, std::string msg) {
    if( useAdapter != EUI48::ALL_DEVICE && useAdapter != a->getAddressAndType().address ) {
        fprintf_td(stderr, "****** Start discovery (%s): Adapter not selected: %s\n", msg.c_str(), a->toString().c_str());
        return false;
    }
    HCIStatusCode status = a->startDiscovery( true, le_scan_active, le_scan_interval, le_scan_window, filter_policy );
    fprintf_td(stderr, "****** Start discovery (%s) result: %s: %s\n", msg.c_str(), to_string(status).c_str(), a->toString().c_str());
    return HCIStatusCode::SUCCESS == status;
}

static bool initAdapter(std::shared_ptr<BTAdapter>& adapter) {
    if( useAdapter != EUI48::ALL_DEVICE && useAdapter != adapter->getAddressAndType().address ) {
        fprintf_td(stderr, "initAdapter: Adapter not selected: %s\n", adapter->toString().c_str());
        return false;
    }
    // Initialize with defaults and power-on
    if( !adapter->isInitialized() ) {
        HCIStatusCode status = adapter->initialize( btMode );
        if( HCIStatusCode::SUCCESS != status ) {
            fprintf_td(stderr, "initAdapter: Adapter initialization failed: %s: %s\n",
                    to_string(status).c_str(), adapter->toString().c_str());
            return false;
        }
    } else if( !adapter->setPowered( true ) ) {
        fprintf_td(stderr, "initAdapter: Already initialized adapter power-on failed:: %s\n", adapter->toString().c_str());
        return false;
    }
    // adapter is powered-on
    fprintf_td(stderr, "initAdapter: %s\n", adapter->toString().c_str());
    {
        const LE_Features le_feats = adapter->getLEFeatures();
        fprintf_td(stderr, "initAdapter: LE_Features %s\n", to_string(le_feats).c_str());
    }
    {
        LE_PHYs Tx { LE_PHYs::LE_2M }, Rx { LE_PHYs::LE_2M };
        HCIStatusCode res = adapter->setDefaultLE_PHY(Tx, Rx);
        fprintf_td(stderr, "initAdapter: Set Default LE PHY: status %s: Tx %s, Rx %s\n",
                to_string(res).c_str(), to_string(Tx).c_str(), to_string(Rx).c_str());
    }
    std::shared_ptr<AdapterStatusListener> asl(new MyAdapterStatusListener());
    adapter->addStatusListener( asl );
    // Flush discovered devices after registering our status listener.
    // This avoids discovered devices before we have registered!
    adapter->removeDiscoveredDevices();

    if( !startDiscovery(adapter.get(), "initAdapter") ) {
        adapter->removeStatusListener( asl );
        return false;
    }
    return true;
}

static bool myChangedAdapterSetFunc(const bool added, std::shared_ptr<BTAdapter>& adapter) {
    if( added ) {
        if( nullptr == chosenAdapter ) {
            if( initAdapter( adapter ) ) {
                chosenAdapter = adapter;
                fprintf_td(stderr, "****** Adapter ADDED__: InitOK: %s\n", adapter->toString().c_str());
            } else {
                fprintf_td(stderr, "****** Adapter ADDED__: Ignored: %s\n", adapter->toString().c_str());
            }
        } else {
            fprintf_td(stderr, "****** Adapter ADDED__: Ignored (other): %s\n", adapter->toString().c_str());
        }
    } else {
        if( nullptr != chosenAdapter && adapter == chosenAdapter ) {
            chosenAdapter = nullptr;
            fprintf_td(stderr, "****** Adapter REMOVED: %s\n", adapter->toString().c_str());
        } else {
            fprintf_td(stderr, "****** Adapter REMOVED (other): %s\n", adapter->toString().c_str());
        }
    }
    return true;
}

void test() {
    bool done = false;

    fprintf_td(stderr, "DirectBT Native Version %s (API %s)\n", DIRECT_BT_VERSION, DIRECT_BT_VERSION_API);

    timestamp_t0 = getCurrentMilliseconds();

    BTManager & mngr = BTManager::get();
    mngr.addChangedAdapterSetCallback(myChangedAdapterSetFunc);

    while( !done ) {
        if( 0 == MULTI_MEASUREMENTS ) {
            fprintf_td(stderr, "****** EOL Test MULTI_MEASUREMENTS left %d, processed %zu/%zu\n",
                    MULTI_MEASUREMENTS.load(), BTDeviceRegistry::getProcessedDeviceCount(), BTDeviceRegistry::getWaitForDevicesCount());
            fprintf_td(stderr, "****** WaitForDevice %s\n", BTDeviceRegistry::getWaitForDevicesString().c_str());
            fprintf_td(stderr, "****** DevicesProcessed %s\n", BTDeviceRegistry::getProcessedDevicesString().c_str());
            done = true;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        }
    }
    chosenAdapter = nullptr;

    //
    // just a manually controlled pull down to show status, not required
    //
    jau::darray<std::shared_ptr<BTAdapter>> adapterList = mngr.getAdapters();

    jau::for_each_const(adapterList, [](const std::shared_ptr<BTAdapter>& adapter) {
        fprintf_td(stderr, "****** EOL Adapter's Devices - pre close: %s\n", adapter->toString().c_str());
        adapter->printDeviceLists();
    });
    {
        int count = mngr.removeChangedAdapterSetCallback(myChangedAdapterSetFunc);
        fprintf_td(stderr, "****** EOL Removed ChangedAdapterSetCallback %d\n", count);

        mngr.close();
    }
    jau::for_each_const(adapterList, [](const std::shared_ptr<BTAdapter>& adapter) {
        fprintf_td(stderr, "****** EOL Adapter's Devices - post close: %s\n", adapter->toString().c_str());
        adapter->printDeviceLists();
    });
}

#include <cstdio>

int main(int argc, char *argv[])
{
    bool waitForEnter=false;

    for(int i=1; i<argc; i++) {
        if( !strcmp("-dbt_debug", argv[i]) && argc > (i+1) ) {
            setenv("direct_bt.debug", argv[++i], 1 /* overwrite */);
        } else if( !strcmp("-dbt_verbose", argv[i]) && argc > (i+1) ) {
            setenv("direct_bt.verbose", argv[++i], 1 /* overwrite */);
        } else if( !strcmp("-dbt_gatt", argv[i]) && argc > (i+1) ) {
            setenv("direct_bt.gatt", argv[++i], 1 /* overwrite */);
        } else if( !strcmp("-dbt_l2cap", argv[i]) && argc > (i+1) ) {
            setenv("direct_bt.l2cap", argv[++i], 1 /* overwrite */);
        } else if( !strcmp("-dbt_hci", argv[i]) && argc > (i+1) ) {
            setenv("direct_bt.hci", argv[++i], 1 /* overwrite */);
        } else if( !strcmp("-dbt_mgmt", argv[i]) && argc > (i+1) ) {
            setenv("direct_bt.mgmt", argv[++i], 1 /* overwrite */);
        } else if( !strcmp("-wait", argv[i]) ) {
            waitForEnter = true;
        } else if( !strcmp("-scanPassive", argv[i]) ) {
            le_scan_active = false;
        } else if( !strcmp("-btmode", argv[i]) && argc > (i+1) ) {
            btMode = to_BTMode(argv[++i]);
        } else if( !strcmp("-adapter", argv[i]) && argc > (i+1) ) {
            useAdapter = EUI48( std::string(argv[++i]) );
        } else if( !strcmp("-dev", argv[i]) && argc > (i+1) ) {
            std::string addrOrNameSub = std::string(argv[++i]);
            BTDeviceRegistry::addToWaitForDevices( addrOrNameSub );
        } else if( !strcmp("-passkey", argv[i]) && argc > (i+2) ) {
            const std::string addrOrNameSub(argv[++i]);
            BTSecurityRegistry::Entry* sec = BTSecurityRegistry::getOrCreate(addrOrNameSub);
            sec->passkey = atoi(argv[++i]);
            fprintf(stderr, "Set passkey in %s\n", sec->toString().c_str());
        } else if( !strcmp("-seclevel", argv[i]) && argc > (i+2) ) {
            const std::string addrOrNameSub(argv[++i]);
            BTSecurityRegistry::Entry* sec = BTSecurityRegistry::getOrCreate(addrOrNameSub);
            sec->sec_level = to_BTSecurityLevel(atoi(argv[++i]));
            fprintf(stderr, "Set sec_level in %s\n", sec->toString().c_str());
        } else if( !strcmp("-iocap", argv[i]) && argc > (i+2) ) {
            const std::string addrOrNameSub(argv[++i]);
            BTSecurityRegistry::Entry* sec = BTSecurityRegistry::getOrCreate(addrOrNameSub);
            sec->io_cap = to_SMPIOCapability(atoi(argv[++i]));
            fprintf(stderr, "Set io_cap in %s\n", sec->toString().c_str());
        } else if( !strcmp("-secauto", argv[i]) && argc > (i+2) ) {
            const std::string addrOrNameSub(argv[++i]);
            BTSecurityRegistry::Entry* sec = BTSecurityRegistry::getOrCreate(addrOrNameSub);
            sec->io_cap_auto = to_SMPIOCapability(atoi(argv[++i]));
            fprintf(stderr, "Set SEC AUTO security io_cap in %s\n", sec->toString().c_str());
        } else if( !strcmp("-count", argv[i]) && argc > (i+1) ) {
            MULTI_MEASUREMENTS = atoi(argv[++i]);
        } else if( !strcmp("-single", argv[i]) ) {
            MULTI_MEASUREMENTS = 1;
        }
    }
    fprintf(stderr, "pid %d\n", getpid());

    fprintf(stderr, "Run with '[-btmode LE|BREDR|DUAL] "
                    "[-disconnect] [-enableGATTPing] [-count <number>] [-single] [-show_update_events] [-quiet] "
                    "[-scanPassive]"
                    "[-resetEachCon connectionCount] "
                    "[-adapter <adapter_address>] "
                    "(-dev <device_[address|name]_sub>)* (-wl <device_address>)* "
                    "(-seclevel <device_[address|name]_sub> <int_sec_level>)* "
                    "(-iocap <device_[address|name]_sub> <int_iocap>)* "
                    "(-secauto <device_[address|name]_sub> <int_iocap>)* "
                    "(-passkey <device_[address|name]_sub> <digits>)* "
                    "[-unpairPre] [-unpairPost] "
                    "[-dbt_verbose true|false] "
                    "[-dbt_debug true|false|adapter.event,gatt.data,hci.event,hci.scan_ad_eir,mgmt.event] "
                    "[-dbt_mgmt cmd.timeout=3000,ringsize=64,...] "
                    "[-dbt_hci cmd.complete.timeout=10000,cmd.status.timeout=3000,ringsize=64,...] "
                    "[-dbt_gatt cmd.read.timeout=500,cmd.write.timeout=500,cmd.init.timeout=2500,ringsize=128,...] "
                    "[-dbt_l2cap reader.timeout=10000,restart.count=0,...] "
                    "\n");

    fprintf(stderr, "MULTI_MEASUREMENTS %d\n", MULTI_MEASUREMENTS.load());
    fprintf(stderr, "adapter %s\n", useAdapter.toString().c_str());
    fprintf(stderr, "btmode %s\n", to_string(btMode).c_str());
    fprintf(stderr, "scanActive %s\n", to_string(le_scan_active).c_str());

    fprintf(stderr, "security-details: %s\n", BTSecurityRegistry::allToString().c_str());
    fprintf(stderr, "waitForDevice: %s\n", BTDeviceRegistry::getWaitForDevicesString().c_str());

    if( waitForEnter ) {
        fprintf(stderr, "Press ENTER to continue\n");
        getchar();
    }
    fprintf(stderr, "****** TEST start\n");
    test();
    fprintf(stderr, "****** TEST end\n");
    if( true ) {
        // Just for testing purpose, i.e. triggering BTManager::close() within the test controlled app,
        // instead of program shutdown.
        fprintf(stderr, "****** Manager close start\n");
        BTManager & mngr = BTManager::get(); // already existing
        mngr.close();
        fprintf(stderr, "****** Manager close end\n");
    }
}
