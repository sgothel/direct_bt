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

#include <direct_bt/BTDeviceRegistry.hpp>
#include <direct_bt/BTSecurityRegistry.hpp>

extern "C" {
    #include <unistd.h>
}

using namespace direct_bt;
using namespace jau;

/** \file
 * This _dbt_scanner10_ C++ scanner example uses the Direct-BT fully event driven workflow
 * and adds multithreading, i.e. one thread processes each found device found
 * as notified via the event listener.
 *
 * _dbt_scanner10_ represents the recommended utilization of Direct-BT.
 *
 * ### dbt_scanner10 Invocation Examples:
 * Using `scripts/run-dbt_scanner10.sh` from `dist` directory:
 * * Scan and read all devices (using default auto-sec w/ keyboard iocap)
 *   ~~~
 *   ../scripts/run-dbt_scanner10.sh
 *   ~~~
 *
 * * Read device C0:26:DA:01:DA:B1  (using default auto-sec w/ keyboard iocap)
 *   ~~~
 *   ../scripts/run-dbt_scanner10.sh -dev C0:26:DA:01:DA:B1
 *   ~~~
 *
 * * Read device C0:26:DA:01:DA:B1  (enforcing no security)
 *   ~~~
 *   ../scripts/run-dbt_scanner10.sh -dev C0:26:DA:01:DA:B1 -seclevel C0:26:DA:01:DA:B1 1
 *   ~~~
 *
 * * Read any device containing C0:26:DA  (enforcing no security)
 *   ~~~
 *   ../scripts/run-dbt_scanner10.sh -dev C0:26:DA -seclevel C0:26:DA 1
 *   ~~~
 *
 * * Read any device containing name `TAIDOC` (enforcing no security)
 *   ~~~
 *   ../scripts/run-dbt_scanner10.sh -dev 'TAIDOC' -seclevel 'TAIDOC' 1
 *   ~~~
 *
 * * Read device C0:26:DA:01:DA:B1, basic debug flags enabled (using default auto-sec w/ keyboard iocap)
 *   ~~~
 *   ../scripts/run-dbt_scanner10.sh -dev C0:26:DA:01:DA:B1 -dbt_debug true
 *   ~~~
 *
 * * Read device C0:26:DA:01:DA:B1, all debug flags enabled (using default auto-sec w/ keyboard iocap)
 *   ~~~
 *   ../scripts/run-dbt_scanner10.sh -dev C0:26:DA:01:DA:B1 -dbt_debug adapter.event,gatt.data,hci.event,hci.scan_ad_eir,mgmt.event
 *   ~~~
 *
 * ## Special Actions
 * * To do a BT adapter removal/add via software, assuming the device is '1-4' (Bus 1.Port 4):
 *   ~~~
 *   echo '1-4' > /sys/bus/usb/drivers/usb/unbind
 *   echo '1-4' > /sys/bus/usb/drivers/usb/bind
 *   ~~~
 */

const static std::string KEY_PATH = "keys";

static uint64_t timestamp_t0;

static int RESET_ADAPTER_EACH_CONN = 0;
static std::atomic<int> deviceReadyCount = 0;

static std::atomic<int> MULTI_MEASUREMENTS = 8;

static bool KEEP_CONNECTED = true;
static bool GATT_PING_ENABLED = false;
static bool REMOVE_DEVICE = true;

static bool USE_WHITELIST = false;
static jau::darray<BDAddressAndType> WHITELIST;

static std::string charIdentifier = "";
static int charValue = 0;

static bool SHOW_UPDATE_EVENTS = false;
static bool QUIET = false;

static void connectDiscoveredDevice(std::shared_ptr<BTDevice> device);

static void processReadyDevice(std::shared_ptr<BTDevice> device);

static void removeDevice(std::shared_ptr<BTDevice> device);
static void resetAdapter(BTAdapter *a, int mode);
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

        if( BDAddressType::BDADDR_LE_PUBLIC != device->getAddressAndType().type
            && BLERandomAddressType::STATIC_PUBLIC != device->getAddressAndType().getBLERandomAddressType() ) {
            // Requires BREDR or LE Secure Connection support: WIP
            fprintf_td(stderr, "****** FOUND__-2: Skip non 'public LE' and non 'random static public LE' %s\n", device->toString(true).c_str());
            return false;
        }
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
        if( SHOW_UPDATE_EVENTS ) {
            fprintf_td(stderr, "****** UPDATED: %s of %s\n", to_string(updateMask).c_str(), device->toString(true).c_str());
        }
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
                const bool res  = SMPKeyBin::remove(KEY_PATH, device->getAddressAndType());
                fprintf_td(stderr, "****** PAIRING_STATE: state %s; Remove key file %s, res %d\n",
                        to_string(state).c_str(), SMPKeyBin::getFilename(KEY_PATH, device->getAddressAndType()).c_str(), res);
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

        if( REMOVE_DEVICE ) {
            std::thread dc(::removeDevice, device); // @suppress("Invalid arguments")
            dc.detach();
        } else {
            BTDeviceRegistry::removeFromProcessingDevices(device->getAddressAndType());
        }
        if( 0 < RESET_ADAPTER_EACH_CONN && 0 == deviceReadyCount % RESET_ADAPTER_EACH_CONN ) {
            std::thread dc(::resetAdapter, &device->getAdapter(), 1); // @suppress("Invalid arguments")
            dc.detach();
        }
    }

    std::string toString() const override {
        return "MyAdapterStatusListener[this "+to_hexstring(this)+"]";
    }

};

static const uuid16_t _TEMPERATURE_MEASUREMENT(GattCharacteristicType::TEMPERATURE_MEASUREMENT);

class MyGATTEventListener : public BTGattChar::Listener {
  private:
    int i, j;

  public:

    MyGATTEventListener(int i_, int j_) : i(i_), j(j_) {}

    void notificationReceived(BTGattCharRef charDecl, const TROOctets& char_value, const uint64_t timestamp) override {
        const uint64_t tR = getCurrentMilliseconds();
        fprintf_td(stderr, "**[%2.2d.%2.2d] Characteristic-Notify: UUID %s, td %" PRIu64 " ******\n",
                i, j, charDecl->value_type->toUUID128String().c_str(), (tR-timestamp));
        fprintf_td(stderr, "**[%2.2d.%2.2d]     Characteristic: %s ******\n", i, j, charDecl->toString().c_str());
        if( _TEMPERATURE_MEASUREMENT == *charDecl->value_type ) {
            std::shared_ptr<GattTemperatureMeasurement> temp = GattTemperatureMeasurement::get(char_value);
            if( nullptr != temp ) {
                fprintf_td(stderr, "**[%2.2d.%2.2d]     Value T: %s ******\n", i, j, temp->toString().c_str());
            }
        }
        fprintf_td(stderr, "**[%2.2d.%2.2d]     Value R: %s ******\n", i, j, char_value.toString().c_str());
    }

    void indicationReceived(BTGattCharRef charDecl,
                            const TROOctets& char_value, const uint64_t timestamp,
                            const bool confirmationSent) override
    {
        const uint64_t tR = getCurrentMilliseconds();
        fprintf_td(stderr, "**[%2.2d.%2.2d] Characteristic-Indication: UUID %s, td %" PRIu64 ", confirmed %d ******\n",
                i, j, charDecl->value_type->toUUID128String().c_str(), (tR-timestamp), confirmationSent);
        fprintf_td(stderr, "**[%2.2d.%2.2d]     Characteristic: %s ******\n", i, j, charDecl->toString().c_str());
        if( _TEMPERATURE_MEASUREMENT == *charDecl->value_type ) {
            std::shared_ptr<GattTemperatureMeasurement> temp = GattTemperatureMeasurement::get(char_value);
            if( nullptr != temp ) {
                fprintf_td(stderr, "**[%2.2d.%2.2d]     Value T: %s ******\n", i, j, temp->toString().c_str());
            }
        }
        fprintf_td(stderr, "**[%2.2d.%2.2d]     Value R: %s ******\n", i, j, char_value.toString().c_str());
    }
};

static void connectDiscoveredDevice(std::shared_ptr<BTDevice> device) {
    fprintf_td(stderr, "****** Connecting Device: Start %s\n", device->toString().c_str());

    // Testing listener lifecycle @ device dtor
    class TempAdapterStatusListener : public AdapterStatusListener {
        void deviceUpdated(std::shared_ptr<BTDevice> device, const EIRDataType updateMask, const uint64_t timestamp) override {
            if( SHOW_UPDATE_EVENTS ) {
                fprintf_td(stderr, "****** UPDATED(2): %s of %s\n", to_string(updateMask).c_str(), device->toString(true).c_str());
            }
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
        const HCIStatusCode r = device->unpair();
        fprintf_td(stderr, "****** Connecting Device: Unpair-Pre result: %s\n", to_string(r).c_str());
    }

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
    HCIStatusCode res = SMPKeyBin::readAndApply(KEY_PATH, *device, req_sec_level, true /* verbose */);
    fprintf_td(stderr, "****** Connecting Device: SMPKeyBin::readAndApply(..) result %s\n", to_string(res).c_str());
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

    if( !USE_WHITELIST ) {
        res = device->connectDefault();
    } else {
        res = HCIStatusCode::SUCCESS;
    }
    fprintf_td(stderr, "****** Connecting Device: End result %s of %s\n", to_string(res).c_str(), device->toString().c_str());

    if( !USE_WHITELIST && 0 == BTDeviceRegistry::getProcessingDeviceCount() && HCIStatusCode::SUCCESS != res ) {
        startDiscovery(&device->getAdapter(), "post-connect");
    }
}

static void processReadyDevice(std::shared_ptr<BTDevice> device) {
    fprintf_td(stderr, "****** Processing Ready Device: Start %s\n", device->toString().c_str());
    device->getAdapter().stopDiscovery(); // make sure for pending connections on failed connect*(..) command

    const uint64_t t1 = getCurrentMilliseconds();

    SMPKeyBin::createAndWrite(*device, KEY_PATH, false /* overwrite */, true /* verbose */);

    bool success = false;

    {
        LE_PHYs resRx, resTx;
        HCIStatusCode res = device->getConnectedLE_PHY(resRx, resTx);
        fprintf_td(stderr, "****** Connected LE PHY: status %s: RX %s, TX %s\n",
                to_string(res).c_str(), to_string(resRx).c_str(), to_string(resTx).c_str());
    }

    //
    // GATT Service Processing
    //
    fprintf_td(stderr, "****** Processing Ready Device: GATT start: %s\n", device->getAddressAndType().toString().c_str());
    if( !QUIET ) {
        device->getAdapter().printDeviceLists();
    }
    try {
        jau::darray<BTGattServiceRef> primServices = device->getGattServices();
        if( 0 == primServices.size() ) {
            fprintf_td(stderr, "****** Processing Ready Device: getServices() failed %s\n", device->toString().c_str());
            goto exit;
        }

        const uint64_t t5 = getCurrentMilliseconds();
        if( !QUIET ) {
            const uint64_t td01 = t1 - timestamp_t0; // adapter-init -> processing-start
            const uint64_t td15 = t5 - t1; // get-gatt-services
            const uint64_t tdc5 = t5 - device->getLastDiscoveryTimestamp(); // discovered to gatt-complete
            const uint64_t td05 = t5 - timestamp_t0; // adapter-init -> gatt-complete
            fprintf_td(stderr, "\n\n\n");
            fprintf_td(stderr, "PERF: GATT primary-services completed\n");
            fprintf_td(stderr, "PERF:  adapter-init to processing-start %" PRIu64 " ms,\n"
                            "PERF:  get-gatt-services %" PRIu64 " ms,\n"
                            "PERF:  discovered to gatt-complete %" PRIu64 " ms (connect %" PRIu64 " ms),\n"
                            "PERF:  adapter-init to gatt-complete %" PRIu64 " ms\n\n",
                            td01, td15, tdc5, (tdc5 - td15), td05);
        }
#if 0
        {
            // WIP: Implement a simple Characteristic ping-pong writeValue <-> notify transmission for stress testing.
            BTManager & manager = device->getAdapter().getManager();
            if( nullptr != charIdentifier && charIdentifier.length() > 0 ) {
                BTGattChar * char2 = (BTGattChar*) nullptr;
                        // manager.find(BluetoothType.GATT_CHARACTERISTIC, null, charIdentifier, device);
                fprintf_td(stderr, "Char UUID %s\n", charIdentifier.c_str());
                fprintf_td(stderr, "  over device : %s\n", char2->toString().c_str());
                if( nullptr != char2 ) {
                    bool cccdEnableResult[2];
                    bool cccdRet = char2->addCharListener( std::shared_ptr<BTGattCharListener>( new MyGATTEventListener(char2) ),
                                                                          cccdEnableResult );
                    if( !QUIET ) {
                        fprintf_td(stderr, "Added CharPingPongListenerRes Notification(%d), Indication(%d): Result %d\n",
                                cccdEnableResult[0], cccdEnableResult[1], cccdRet);
                    }
                    if( cccdRet ) {
                        uint8_t cmd[] { (uint8_t)charValue }; // request device model
                        bool wres = char2->writeValueNoResp(cmd);
                        if( !QUIET ) {
                            fprintf_td(stderr, "Write response: "+wres);
                        }
                    }
                }
            }
        }
#endif

        std::shared_ptr<GattGenericAccessSvc> ga = device->getGattGenericAccess();
        if( nullptr != ga && !QUIET ) {
            fprintf_td(stderr, "  GenericAccess: %s\n\n", ga->toString().c_str());
        }
        {
            std::shared_ptr<BTGattHandler> gatt = device->getGattHandler();
            if( nullptr != gatt && gatt->isConnected() ) {
                std::shared_ptr<GattDeviceInformationSvc> di = gatt->getDeviceInformation(primServices);
                if( nullptr != di && !QUIET ) {
                    fprintf_td(stderr, "  DeviceInformation: %s\n\n", di->toString().c_str());
                }
            }
        }

        for(size_t i=0; i<primServices.size(); i++) {
            BTGattService & primService = *primServices.at(i);
            if( !QUIET ) {
                fprintf_td(stderr, "  [%2.2d] Service UUID %s\n", i, primService.type->toUUID128String().c_str());
                fprintf_td(stderr, "  [%2.2d]         %s\n", i, primService.toString().c_str());
            }
            jau::darray<BTGattCharRef> & serviceCharacteristics = primService.characteristicList;
            for(size_t j=0; j<serviceCharacteristics.size(); j++) {
                BTGattChar & serviceChar = *serviceCharacteristics.at(j);
                if( !QUIET ) {
                    fprintf_td(stderr, "  [%2.2d.%2.2d] Characteristic: UUID %s\n", i, j, serviceChar.value_type->toUUID128String().c_str());
                    fprintf_td(stderr, "  [%2.2d.%2.2d]     %s\n", i, j, serviceChar.toString().c_str());
                }
                if( serviceChar.hasProperties(BTGattChar::PropertyBitVal::Read) ) {
                    POctets value(BTGattHandler::number(BTGattHandler::Defaults::MAX_ATT_MTU), 0);
                    if( serviceChar.readValue(value) ) {
                        std::string sval = dfa_utf8_decode(value.get_ptr(), value.getSize());
                        if( !QUIET ) {
                            fprintf_td(stderr, "  [%2.2d.%2.2d]     value: %s ('%s')\n", (int)i, (int)j, value.toString().c_str(), sval.c_str());
                        }
                    }
                }
                jau::darray<BTGattDescRef> & charDescList = serviceChar.descriptorList;
                for(size_t k=0; k<charDescList.size(); k++) {
                    BTGattDesc & charDesc = *charDescList.at(k);
                    if( !QUIET ) {
                        fprintf_td(stderr, "  [%2.2d.%2.2d.%2.2d] Descriptor: UUID %s\n", i, j, k, charDesc.type->toUUID128String().c_str());
                        fprintf_td(stderr, "  [%2.2d.%2.2d.%2.2d]     %s\n", i, j, k, charDesc.toString().c_str());
                    }
                }
                bool cccdEnableResult[2];
                bool cccdRet = serviceChar.addCharListener( std::shared_ptr<BTGattChar::Listener>( new MyGATTEventListener(i, j) ),
                                                            cccdEnableResult );
                if( !QUIET ) {
                    fprintf_td(stderr, "  [%2.2d.%2.2d] Characteristic-Listener: Notification(%d), Indication(%d): Added %d\n",
                            (int)i, (int)j, cccdEnableResult[0], cccdEnableResult[1], cccdRet);
                    fprintf_td(stderr, "\n");
                }
            }
            fprintf_td(stderr, "\n");
        }
        // FIXME sleep 1s for potential callbacks ..
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        success = true;
    } catch ( std::exception & e ) {
        fprintf_td(stderr, "****** Processing Ready Device: Exception caught for %s: %s\n", device->toString().c_str(), e.what());
    }

exit:
    fprintf_td(stderr, "****** Processing Ready Device: End-1: Success %d on %s; devInProc %zu\n",
            success, device->toString().c_str(), BTDeviceRegistry::getProcessingDeviceCount());

    BTDeviceRegistry::removeFromProcessingDevices(device->getAddressAndType());

    if( !USE_WHITELIST && 0 == BTDeviceRegistry::getProcessingDeviceCount() ) {
        startDiscovery(&device->getAdapter(), "post-processing-1");
    }

    if( KEEP_CONNECTED && GATT_PING_ENABLED && success ) {
        while( device->pingGATT() ) {
            fprintf_td(stderr, "****** Processing Ready Device: pingGATT OK: %s\n", device->getAddressAndType().toString().c_str());
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        fprintf_td(stderr, "****** Processing Ready Device: pingGATT failed, waiting for disconnect: %s\n", device->getAddressAndType().toString().c_str());
        // Even w/ GATT_PING_ENABLED, we utilize disconnect event to clean up -> remove
    }

    if( !QUIET ) {
        device->getAdapter().printDeviceLists();
    }

    fprintf_td(stderr, "****** Processing Ready Device: End-2: Success %d on %s; devInProc %zu\n",
            success, device->toString().c_str(), BTDeviceRegistry::getProcessingDeviceCount());

    if( success ) {
        BTDeviceRegistry::addToProcessedDevices(device->getAddressAndType(), device->getName());
    }
    device->removeAllCharListener();

    if( !KEEP_CONNECTED ) {
        {
            const HCIStatusCode unpair_res = device->unpair();
            fprintf_td(stderr, "****** Processing Ready Device: Unpair-Post result: %s\n", to_string(unpair_res).c_str());
        }

        device->remove();

        if( 0 < RESET_ADAPTER_EACH_CONN && 0 == deviceReadyCount % RESET_ADAPTER_EACH_CONN ) {
            resetAdapter(&device->getAdapter(), 2);
        } else if( !USE_WHITELIST && 0 == BTDeviceRegistry::getProcessingDeviceCount() ) {
            startDiscovery(&device->getAdapter(), "post-processing-2");
        }
    }

    if( 0 < MULTI_MEASUREMENTS ) {
        MULTI_MEASUREMENTS--;
        fprintf_td(stderr, "****** Processing Ready Device: MULTI_MEASUREMENTS left %d: %s\n", MULTI_MEASUREMENTS.load(), device->getAddressAndType().toString().c_str());
    }
}

static void removeDevice(std::shared_ptr<BTDevice> device) {
    fprintf_td(stderr, "****** Remove Device: removing: %s\n", device->getAddressAndType().toString().c_str());
    device->getAdapter().stopDiscovery();

    BTDeviceRegistry::removeFromProcessingDevices(device->getAddressAndType());

    device->remove();

    if( !USE_WHITELIST && 0 == BTDeviceRegistry::getProcessingDeviceCount() ) {
        startDiscovery(&device->getAdapter(), "post-remove-device");
    }
}

static void resetAdapter(BTAdapter *a, int mode) {
    fprintf_td(stderr, "****** Reset Adapter: reset[%d] start: %s\n", mode, a->toString().c_str());
    HCIStatusCode res = a->reset();
    fprintf_td(stderr, "****** Reset Adapter: reset[%d] end: %s, %s\n", mode, to_string(res).c_str(), a->toString().c_str());
}

static bool le_scan_active = true; // default value
static const uint16_t le_scan_interval = 24; // default value
static const uint16_t le_scan_window = 24; // default value
static const uint8_t filter_policy = 0; // default value

static bool startDiscovery(BTAdapter *a, std::string msg) {
    HCIStatusCode status = a->startDiscovery( true, le_scan_active, le_scan_interval, le_scan_window, filter_policy );
    fprintf_td(stderr, "****** Start discovery (%s) result: %s\n", msg.c_str(), to_string(status).c_str());
    return HCIStatusCode::SUCCESS == status;
}

static bool initAdapter(std::shared_ptr<BTAdapter>& adapter) {
    // Even if adapter is not yet powered, listen to it and act when it gets powered-on
    adapter->addStatusListener(std::shared_ptr<AdapterStatusListener>(new MyAdapterStatusListener()));
    // Flush discovered devices after registering our status listener.
    // This avoids discovered devices before we have registered!
    adapter->removeDiscoveredDevices();

    if( !adapter->isPowered() ) { // should have been covered above
        fprintf_td(stderr, "Adapter not powered (2): %s\n", adapter->toString().c_str());
        return false;
    }

    if( USE_WHITELIST ) {
        for (auto it = WHITELIST.begin(); it != WHITELIST.end(); ++it) {
            bool res = adapter->addDeviceToWhitelist(*it, HCIWhitelistConnectType::HCI_AUTO_CONN_ALWAYS);
            fprintf_td(stderr, "Added to WHITELIST: res %d, address %s\n", res, it->toString().c_str());
        }
    } else {
        if( !startDiscovery(adapter.get(), "kick-off") ) {
            return false;
        }
    }
    return true;
}

static bool myChangedAdapterSetFunc(const bool added, std::shared_ptr<BTAdapter>& adapter) {
    if( added ) {
        if( initAdapter( adapter ) ) {
            fprintf_td(stderr, "****** Adapter ADDED__: InitOK. %s\n", adapter->toString().c_str());
        } else {
            fprintf_td(stderr, "****** Adapter ADDED__: Ignored %s\n", adapter->toString().c_str());
        }
        fprintf_td(stderr, "****** Adapter Features: %s\n", direct_bt::to_string(adapter->getLEFeatures()).c_str());
    } else {
        fprintf_td(stderr, "****** Adapter REMOVED: %s\n", adapter->toString().c_str());
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
        if( 0 == MULTI_MEASUREMENTS ||
            ( -1 == MULTI_MEASUREMENTS && BTDeviceRegistry::isWaitingForAnyDevice() && BTDeviceRegistry::areAllDevicesProcessed() )
          )
        {
            fprintf_td(stderr, "****** EOL Test MULTI_MEASUREMENTS left %d, processed %zu/%zu\n",
                    MULTI_MEASUREMENTS.load(), BTDeviceRegistry::getProcessedDeviceCount(), BTDeviceRegistry::getWaitForDevicesCount());
            fprintf_td(stderr, "****** WaitForDevice %s\n", BTDeviceRegistry::getWaitForDevicesString().c_str());
            fprintf_td(stderr, "****** DevicesProcessed %s\n", BTDeviceRegistry::getProcessedDevicesString().c_str());
            done = true;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        }
    }

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
    BTMode btMode = BTMode::DUAL;
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
        } else if( !strcmp("-btmode", argv[i]) && argc > (i+1) ) {
            btMode = to_BTMode(argv[++i]);
            if( BTMode::NONE != btMode ) {
                setenv("direct_bt.mgmt.btmode", to_string(btMode).c_str(), 1 /* overwrite */);
            }
        } else if( !strcmp("-wait", argv[i]) ) {
            waitForEnter = true;
        } else if( !strcmp("-show_update_events", argv[i]) ) {
            SHOW_UPDATE_EVENTS = true;
        } else if( !strcmp("-quiet", argv[i]) ) {
            QUIET = true;
        } else if( !strcmp("-scanPassive", argv[i]) ) {
            le_scan_active = false;
        } else if( !strcmp("-dev", argv[i]) && argc > (i+1) ) {
            std::string addrOrNameSub = std::string(argv[++i]);
            BTDeviceRegistry::addToWaitForDevices( addrOrNameSub );
        } else if( !strcmp("-wl", argv[i]) && argc > (i+1) ) {
            std::string macstr = std::string(argv[++i]);
            BDAddressAndType wle(EUI48(macstr), BDAddressType::BDADDR_LE_PUBLIC);
            fprintf(stderr, "Whitelist + %s\n", wle.toString().c_str());
            WHITELIST.push_back( wle );
            USE_WHITELIST = true;
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
        } else if( !strcmp("-charid", argv[i]) && argc > (i+1) ) {
            charIdentifier = std::string(argv[++i]);
        } else if( !strcmp("-charval", argv[i]) && argc > (i+1) ) {
            charValue = atoi(argv[++i]);
        } else if( !strcmp("-disconnect", argv[i]) ) {
            KEEP_CONNECTED = false;
        } else if( !strcmp("-enableGATTPing", argv[i]) ) {
            GATT_PING_ENABLED = true;
        } else if( !strcmp("-keepDevice", argv[i]) ) {
            REMOVE_DEVICE = false;
        } else if( !strcmp("-count", argv[i]) && argc > (i+1) ) {
            MULTI_MEASUREMENTS = atoi(argv[++i]);
        } else if( !strcmp("-single", argv[i]) ) {
            MULTI_MEASUREMENTS = -1;
        } else if( !strcmp("-resetEachCon", argv[i]) && argc > (i+1) ) {
            RESET_ADAPTER_EACH_CONN = atoi(argv[++i]);
        }
    }
    fprintf(stderr, "pid %d\n", getpid());

    fprintf(stderr, "Run with '[-btmode LE|BREDR|DUAL] "
                    "[-disconnect] [-enableGATTPing] [-count <number>] [-single] [-show_update_events] [-quiet] "
                    "[-scanPassive]"
                    "[-resetEachCon connectionCount] "
                    "(-dev <device_[address|name]_sub>)* (-wl <device_address>)* "
                    "(-seclevel <device_[address|name]_sub> <int_sec_level>)* "
                    "(-iocap <device_[address|name]_sub> <int_iocap>)* "
                    "(-secauto <device_[address|name]_sub> <int_iocap>)* "
                    "(-passkey <device_[address|name]_sub> <digits>)* "
                    "[-unpairPre] [-unpairPost] "
                    "[-charid <uuid>] [-charval <byte-val>] "
                    "[-dbt_verbose true|false] "
                    "[-dbt_debug true|false|adapter.event,gatt.data,hci.event,hci.scan_ad_eir,mgmt.event] "
                    "[-dbt_mgmt cmd.timeout=3000,ringsize=64,...] "
                    "[-dbt_hci cmd.complete.timeout=10000,cmd.status.timeout=3000,ringsize=64,...] "
                    "[-dbt_gatt cmd.read.timeout=500,cmd.write.timeout=500,cmd.init.timeout=2500,ringsize=128,...] "
                    "[-dbt_l2cap reader.timeout=10000,restart.count=0,...] "
                    "\n");

    fprintf(stderr, "MULTI_MEASUREMENTS %d\n", MULTI_MEASUREMENTS.load());
    fprintf(stderr, "KEEP_CONNECTED %d\n", KEEP_CONNECTED);
    fprintf(stderr, "RESET_ADAPTER_EACH_CONN %d\n", RESET_ADAPTER_EACH_CONN);
    fprintf(stderr, "GATT_PING_ENABLED %d\n", GATT_PING_ENABLED);
    fprintf(stderr, "REMOVE_DEVICE %d\n", REMOVE_DEVICE);
    fprintf(stderr, "USE_WHITELIST %d\n", USE_WHITELIST);
    fprintf(stderr, "SHOW_UPDATE_EVENTS %d\n", SHOW_UPDATE_EVENTS);
    fprintf(stderr, "QUIET %d\n", QUIET);
    fprintf(stderr, "btmode %s\n", to_string(btMode).c_str());
    fprintf(stderr, "scanActive %s\n", to_string(le_scan_active).c_str());
    fprintf(stderr, "characteristic-id: %s\n", charIdentifier.c_str());
    fprintf(stderr, "characteristic-value: %d\n", charValue);

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
