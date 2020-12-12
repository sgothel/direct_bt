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
#include <fstream>
#include <iostream>

#include <cinttypes>

#include <pthread.h>

#include <jau/dfa_utf8_decode.hpp>

#include <direct_bt/DirectBT.hpp>

extern "C" {
    #include <unistd.h>
}

using namespace direct_bt;
using namespace jau;


/**
 * This C++ scanner example uses the Direct-BT fully event driven workflow
 * and adds multithreading, i.e. one thread processes each found device found
 * as notified via the event listener.
 * <p>
 * This example represents the recommended utilization of Direct-BT.
 * </p>
 */

static uint64_t timestamp_t0;


static int RESET_ADAPTER_EACH_CONN = 0;
static std::atomic<int> deviceReadyCount = 0;

static std::atomic<int> MULTI_MEASUREMENTS = 8;

static bool KEEP_CONNECTED = true;
static bool UNPAIR_DEVICE_PRE = false;
static bool UNPAIR_DEVICE_POST = false;
static bool GATT_PING_ENABLED = false;
static bool REMOVE_DEVICE = true;

static bool USE_WHITELIST = false;
static std::vector<BDAddressAndType> WHITELIST;

static std::string charIdentifier = "";
static int charValue = 0;

static bool SHOW_UPDATE_EVENTS = false;
static bool QUIET = false;

static std::vector<BDAddressAndType> waitForDevices;

const static int NO_PASSKEY = -1;
static int pairing_passkey = NO_PASSKEY;
static BTSecurityLevel sec_level = BTSecurityLevel::UNSET;
static SMPIOCapability io_capabilities = SMPIOCapability::UNSET;

static void connectDiscoveredDevice(std::shared_ptr<DBTDevice> device);

static void processReadyDevice(std::shared_ptr<DBTDevice> device);

static void removeDevice(std::shared_ptr<DBTDevice> device);
static void resetAdapter(DBTAdapter *a, int mode);
static bool startDiscovery(DBTAdapter *a, std::string msg);

static std::vector<BDAddressAndType> devicesInProcessing;
static std::recursive_mutex mtx_devicesProcessing;

static std::recursive_mutex mtx_devicesProcessed;
static std::vector<BDAddressAndType> devicesProcessed;

bool matches(std::vector<BDAddressAndType> &cont, const BDAddressAndType &mac) {
    return cont.end() != find_if(cont.begin(), cont.end(), [&](const BDAddressAndType & it)->bool {
       return it.matches(mac);
    });
}

bool contains(std::vector<BDAddressAndType> &cont, const BDAddressAndType &mac) {
    return cont.end() != find(cont.begin(), cont.end(), mac);
}

void printList(const std::string &msg, std::vector<BDAddressAndType> &cont) {
    fprintf(stderr, "%s ", msg.c_str());
    std::for_each(cont.begin(), cont.end(),
            [](const BDAddressAndType &mac) { fprintf(stderr, "%s, ", mac.toString().c_str()); });
    fprintf(stderr, "\n");
}

static void addToDevicesProcessed(const BDAddressAndType &a) {
    const std::lock_guard<std::recursive_mutex> lock(mtx_devicesProcessed); // RAII-style acquire and relinquish via destructor
    devicesProcessed.push_back(a);
}
static bool isDeviceProcessed(const BDAddressAndType & a) {
    const std::lock_guard<std::recursive_mutex> lock(mtx_devicesProcessed); // RAII-style acquire and relinquish via destructor
    return contains(devicesProcessed, a);
}
static size_t getDeviceProcessedCount() {
    const std::lock_guard<std::recursive_mutex> lock(mtx_devicesProcessed); // RAII-style acquire and relinquish via destructor
    return devicesProcessed.size();
}
static bool allDevicesProcessed(std::vector<BDAddressAndType> &cont) {
    const std::lock_guard<std::recursive_mutex> lock(mtx_devicesProcessed); // RAII-style acquire and relinquish via destructor
    for (auto it = cont.begin(); it != cont.end(); ++it) {
        if( !contains(devicesProcessed, *it) ) {
            return false;
        }
    }
    return true;
}
static void printDevicesProcessed(const std::string &msg) {
    const std::lock_guard<std::recursive_mutex> lock(mtx_devicesProcessed); // RAII-style acquire and relinquish via destructor
    printList(msg, devicesProcessed);
}

static void addToDevicesProcessing(const BDAddressAndType &a) {
    const std::lock_guard<std::recursive_mutex> lock(mtx_devicesProcessing); // RAII-style acquire and relinquish via destructor
    devicesInProcessing.push_back(a);
}
static bool removeFromDevicesProcessing(const BDAddressAndType &a) {
    const std::lock_guard<std::recursive_mutex> lock(mtx_devicesProcessing); // RAII-style acquire and relinquish via destructor
    for (auto it = devicesInProcessing.begin(); it != devicesInProcessing.end(); ++it) {
        if ( a == *it ) {
            devicesInProcessing.erase(it);
            return true;
        }
    }
    return false;
}
static bool isDeviceProcessing(const BDAddressAndType & a) {
    const std::lock_guard<std::recursive_mutex> lock(mtx_devicesProcessing); // RAII-style acquire and relinquish via destructor
    return contains(devicesInProcessing, a);
}
static size_t getDeviceProcessingCount() {
    const std::lock_guard<std::recursive_mutex> lock(mtx_devicesProcessing); // RAII-style acquire and relinquish via destructor
    return devicesInProcessing.size();
}

struct MyLongTermKeyInfo {
    BDAddressAndType address_and_type;
    SMPLongTermKeyInfo smp_ltk;

    bool write(const std::string filename) {
        if( !smp_ltk.isValid() ) {
            fprintf(stderr, "****** WRITE LTK [%s, invalid, skipped]: %s\n",
                    address_and_type.toString().c_str(),
                    smp_ltk.toString().c_str());
            return false;
        }
        std::ofstream file(filename, std::ios::binary | std::ios::trunc);
        file.write((char*)&address_and_type.address, sizeof(address_and_type.address));
        file.write((char*)&address_and_type.type, sizeof(address_and_type.type));
        file.write((char*)&smp_ltk, sizeof(smp_ltk));
        file.close();
        fprintf(stderr, "****** WRITE LTK [%s, valid, written]: %s\n",
                address_and_type.toString().c_str(),
                smp_ltk.toString().c_str());
        return true;
    }

    bool read(const std::string filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open() ) {
            return false;
        }
        file.read((char*)&address_and_type.address, sizeof(address_and_type.address));
        file.read((char*)&address_and_type.type, sizeof(address_and_type.type));
        file.read((char*)&smp_ltk, sizeof(smp_ltk));
        file.close();
        address_and_type.clearHash();
        fprintf(stderr, "****** READ LTK [%s, valid %d]: %s\n",
                address_and_type.toString().c_str(),
                smp_ltk.isValid(), smp_ltk.toString().c_str());
        return smp_ltk.isValid();
    }
};

struct MySignatureResolvingKeyInfo {
    BDAddressAndType address_and_type;
    SMPSignatureResolvingKeyInfo smp_csrk;

    bool write(const std::string filename) {
        std::ofstream file(filename, std::ios::binary | std::ios::trunc);
        file.write((char*)&address_and_type.address, sizeof(address_and_type.address));
        file.write((char*)&address_and_type.type, sizeof(address_and_type.type));
        file.write((char*)&smp_csrk, sizeof(smp_csrk));
        file.close();
        fprintf(stderr, "****** WRITE CSRK [%s, written]: %s\n",
                address_and_type.toString().c_str(),
                smp_csrk.toString().c_str());
        return true;
    }

    bool read(const std::string filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open() ) {
            return false;
        }
        file.read((char*)&address_and_type.address, sizeof(address_and_type.address));
        file.read((char*)&address_and_type.type, sizeof(address_and_type.type));
        file.read((char*)&smp_csrk, sizeof(smp_csrk));
        file.close();
        address_and_type.clearHash();
        fprintf(stderr, "****** READ CSRK %s: %s\n",
                address_and_type.toString().c_str(),
                smp_csrk.toString().c_str());
        return true;
    }
};

class MyAdapterStatusListener : public AdapterStatusListener {

    void adapterSettingsChanged(DBTAdapter &a, const AdapterSetting oldmask, const AdapterSetting newmask,
                                const AdapterSetting changedmask, const uint64_t timestamp) override {
        const bool initialSetting = AdapterSetting::NONE == oldmask;
        if( initialSetting ) {
            fprintf(stderr, "****** SETTINGS_INITIAL: %s -> %s, changed %s\n", getAdapterSettingMaskString(oldmask).c_str(),
                    getAdapterSettingMaskString(newmask).c_str(), getAdapterSettingMaskString(changedmask).c_str());
        } else {
            fprintf(stderr, "****** SETTINGS_CHANGED: %s -> %s, changed %s\n", getAdapterSettingMaskString(oldmask).c_str(),
                    getAdapterSettingMaskString(newmask).c_str(), getAdapterSettingMaskString(changedmask).c_str());
        }
        fprintf(stderr, "Status DBTAdapter:\n");
        fprintf(stderr, "%s\n", a.toString().c_str());
        (void)timestamp;

        if( !initialSetting &&
            isAdapterSettingBitSet(changedmask, AdapterSetting::POWERED) &&
            isAdapterSettingBitSet(newmask, AdapterSetting::POWERED) )
        {
            std::thread sd(::startDiscovery, &a, "powered-on"); // @suppress("Invalid arguments")
            sd.detach();
        }
    }

    void discoveringChanged(DBTAdapter &a, const ScanType currentMeta, const ScanType changedType, const bool changedEnabled, const bool keepAlive, const uint64_t timestamp) override {
        fprintf(stderr, "****** DISCOVERING: meta %s, changed[%s, enabled %d, keepAlive %d]: %s\n",
                getScanTypeString(currentMeta).c_str(), getScanTypeString(changedType).c_str(), changedEnabled, keepAlive, a.toString().c_str());
        (void)timestamp;
    }

    void deviceFound(std::shared_ptr<DBTDevice> device, const uint64_t timestamp) override {
        (void)timestamp;

        if( BDAddressType::BDADDR_LE_PUBLIC != device->getAddressAndType().type
            && BLERandomAddressType::STATIC_PUBLIC != device->getAddressAndType().getBLERandomAddressType() ) {
            // Requires BREDR or LE Secure Connection support: WIP
            fprintf(stderr, "****** FOUND__-2: Skip non 'public LE' and non 'random static public LE' %s\n", device->toString(true).c_str());
            return;
        }
        if( !isDeviceProcessing( device->getAddressAndType() ) &&
            ( waitForDevices.empty() ||
              ( matches(waitForDevices, device->getAddressAndType()) &&
                ( 0 < MULTI_MEASUREMENTS || !isDeviceProcessed(device->getAddressAndType()) )
              )
            )
          )
        {
            fprintf(stderr, "****** FOUND__-0: Connecting %s\n", device->toString(true).c_str());
            {
                const uint64_t td = getCurrentMilliseconds() - timestamp_t0; // adapter-init -> now
                fprintf(stderr, "PERF: adapter-init -> FOUND__-0  %" PRIu64 " ms\n", td);
            }
            std::thread dc(::connectDiscoveredDevice, device); // @suppress("Invalid arguments")
            dc.detach();
        } else {
            fprintf(stderr, "****** FOUND__-1: NOP %s\n", device->toString(true).c_str());
        }
    }

    void deviceUpdated(std::shared_ptr<DBTDevice> device, const EIRDataType updateMask, const uint64_t timestamp) override {
        if( SHOW_UPDATE_EVENTS ) {
            fprintf(stderr, "****** UPDATED: %s of %s\n", getEIRDataMaskString(updateMask).c_str(), device->toString(true).c_str());
        }
        (void)timestamp;
    }

    void deviceConnected(std::shared_ptr<DBTDevice> device, const uint16_t handle, const uint64_t timestamp) override {
        fprintf(stderr, "****** CONNECTED: %s\n", device->toString(true).c_str());
        (void)handle;
        (void)timestamp;
    }

    void devicePairingState(std::shared_ptr<DBTDevice> device, const SMPPairingState state, const PairingMode mode, const uint64_t timestamp) override {
        fprintf(stderr, "****** PAIRING STATE: state %s, mode %s, %s\n",
            getSMPPairingStateString(state).c_str(), getPairingModeString(mode).c_str(), device->toString().c_str());
        (void)timestamp;
        switch( state ) {
            case SMPPairingState::NONE:
                // next: deviceReady(..)
                break;
            case SMPPairingState::FAILED:
                // next: deviceReady() or deviceDisconnected(..)
                break;
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
                if( pairing_passkey != NO_PASSKEY ) {
                    std::thread dc(&DBTDevice::setPairingPasskey, device, static_cast<uint32_t>(pairing_passkey)); // @suppress("Invalid arguments")
                    dc.detach();
                } /* else {
                    std::thread dc(&DBTDevice::setPairingPasskeyNegative, device); // @suppress("Invalid arguments")
                    dc.detach();
                } */
                // next: KEY_DISTRIBUTION or FAILED
              } break;
            case SMPPairingState::NUMERIC_COMPARE_EXPECTED: {
                std::thread dc(&DBTDevice::setPairingNumericComparison, device, true); // @suppress("Invalid arguments")
                dc.detach();
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

    void deviceReady(std::shared_ptr<DBTDevice> device, const uint64_t timestamp) override {
        (void)timestamp;
        if( !isDeviceProcessing( device->getAddressAndType() ) &&
            ( waitForDevices.empty() ||
              ( matches(waitForDevices, device->getAddressAndType()) &&
                ( 0 < MULTI_MEASUREMENTS || !isDeviceProcessed(device->getAddressAndType()) )
              )
            )
          )
        {
            deviceReadyCount++;
            fprintf(stderr, "****** READY-0: Processing[%d] %s\n", deviceReadyCount.load(), device->toString(true).c_str());
            addToDevicesProcessing(device->getAddressAndType());
            processReadyDevice(device); // AdapterStatusListener::deviceReady() explicitly allows prolonged and complex code execution!
        } else {
            fprintf(stderr, "****** READY-1: NOP %s\n", device->toString(true).c_str());
        }
    }

    void deviceDisconnected(std::shared_ptr<DBTDevice> device, const HCIStatusCode reason, const uint16_t handle, const uint64_t timestamp) override {
        fprintf(stderr, "****** DISCONNECTED: Reason 0x%X (%s), old handle %s: %s\n",
                static_cast<uint8_t>(reason), getHCIStatusCodeString(reason).c_str(),
                uint16HexString(handle).c_str(), device->toString(true).c_str());
        (void)timestamp;

        if( REMOVE_DEVICE ) {
            std::thread dc(::removeDevice, device); // @suppress("Invalid arguments")
            dc.detach();
        } else {
            removeFromDevicesProcessing(device->getAddressAndType());
        }
        if( 0 < RESET_ADAPTER_EACH_CONN && 0 == deviceReadyCount % RESET_ADAPTER_EACH_CONN ) {
            std::thread dc(::resetAdapter, &device->getAdapter(), 1); // @suppress("Invalid arguments")
            dc.detach();
        }
    }

    std::string toString() const override {
        return "MyAdapterStatusListener[this "+aptrHexString(this)+"]";
    }

};

static const uuid16_t _TEMPERATURE_MEASUREMENT(GattCharacteristicType::TEMPERATURE_MEASUREMENT);

class MyGATTEventListener : public AssociatedGATTCharacteristicListener {
  public:

    MyGATTEventListener(const GATTCharacteristic * characteristicMatch)
    : AssociatedGATTCharacteristicListener(characteristicMatch) {}

    void notificationReceived(GATTCharacteristicRef charDecl, std::shared_ptr<TROOctets> char_value, const uint64_t timestamp) override {
        const std::shared_ptr<DBTDevice> dev = charDecl->getDeviceChecked();
        const uint64_t tR = getCurrentMilliseconds();
        fprintf(stderr, "****** GATT Notify (td %" PRIu64 " ms, dev-discovered %" PRIu64 " ms): From %s\n",
                (tR-timestamp), (tR-dev->getLastDiscoveryTimestamp()), dev->toString().c_str());
        if( nullptr != charDecl ) {
            fprintf(stderr, "****** decl %s\n", charDecl->toString().c_str());
        }
        fprintf(stderr, "****** rawv %s\n", char_value->toString().c_str());
    }

    void indicationReceived(GATTCharacteristicRef charDecl,
                            std::shared_ptr<TROOctets> char_value, const uint64_t timestamp,
                            const bool confirmationSent) override
    {
        const std::shared_ptr<DBTDevice> dev = charDecl->getDeviceChecked();
        const uint64_t tR = getCurrentMilliseconds();
        fprintf(stderr, "****** GATT Indication (confirmed %d, td(msg %" PRIu64 " ms, dev-discovered %" PRIu64 " ms): From %s\n",
                confirmationSent, (tR-timestamp), (tR-dev->getLastDiscoveryTimestamp()), dev->toString().c_str());
        if( nullptr != charDecl ) {
            fprintf(stderr, "****** decl %s\n", charDecl->toString().c_str());
            if( _TEMPERATURE_MEASUREMENT == *charDecl->value_type ) {
                std::shared_ptr<GattTemperatureMeasurement> temp = GattTemperatureMeasurement::get(*char_value);
                if( nullptr != temp ) {
                    fprintf(stderr, "****** valu %s\n", temp->toString().c_str());
                }
            }
        }
        fprintf(stderr, "****** rawv %s\n", char_value->toString().c_str());
    }
};

static void connectDiscoveredDevice(std::shared_ptr<DBTDevice> device) {
    fprintf(stderr, "****** Connecting Device: Start %s\n", device->toString().c_str());

    if( UNPAIR_DEVICE_PRE ) {
        const HCIStatusCode unpair_res = device->unpair();
        fprintf(stderr, "****** Connecting Device: Unpair-Pre result: %s\n", getHCIStatusCodeString(unpair_res).c_str());
    }

    device->getAdapter().stopDiscovery();

    {
        MyLongTermKeyInfo my_ltk_resp;
        MyLongTermKeyInfo my_ltk_init;
        if( my_ltk_init.read(device->getAddressAndType().toString()+".init.ltk") &&
            my_ltk_resp.read(device->getAddressAndType().toString()+".resp.ltk") &&
            HCIStatusCode::SUCCESS == device->setLongTermKeyInfo(my_ltk_init.smp_ltk) &&
            HCIStatusCode::SUCCESS == device->setLongTermKeyInfo(my_ltk_resp.smp_ltk) ) {
            fprintf(stderr, "****** Connecting Device: Loaded LTKs from file successfully\n");
        } else {
            fprintf(stderr, "****** Connecting Device: Error loading LTKs from file\n");
        }
    }

    device->setConnSecurityBest(sec_level, io_capabilities);

    HCIStatusCode res;
    if( !USE_WHITELIST ) {
        res = device->connectDefault();
    } else {
        res = HCIStatusCode::SUCCESS;
    }

    fprintf(stderr, "****** Connecting Device: End result %s of %s\n", getHCIStatusCodeString(res).c_str(), device->toString().c_str());
    if( !USE_WHITELIST && 0 == getDeviceProcessingCount() && HCIStatusCode::SUCCESS != res ) {
        startDiscovery(&device->getAdapter(), "post-connect");
    }
}

static void processReadyDevice(std::shared_ptr<DBTDevice> device) {
    fprintf(stderr, "****** Processing Ready Device: Start %s\n", device->toString().c_str());
    device->getAdapter().stopDiscovery(); // make sure for pending connections on failed connect*(..) command
    const uint64_t t1 = getCurrentMilliseconds();
    bool success = false;

    {
        const SMPPairingState pstate = device->getPairingState();
        const PairingMode pmode = device->getPairingMode(); // Skip PairingMode::PRE_PAIRED (write again)
        if( SMPPairingState::COMPLETED == pstate && PairingMode::PRE_PAIRED != pmode ) {
            const SMPKeyType keys_resp = device->getAvailableSMPKeys(true /* responder */);
            const SMPKeyType keys_init = device->getAvailableSMPKeys(false /* responder */);

            if( ( SMPKeyType::ENC_KEY & keys_init ) != SMPKeyType::NONE ) {
                MyLongTermKeyInfo my_ltk { device->getAddressAndType(),
                                           device->getLongTermKeyInfo(false /* responder */) };
                my_ltk.write(my_ltk.address_and_type.toString()+".init.ltk");
            }
            if( ( SMPKeyType::ENC_KEY & keys_resp ) != SMPKeyType::NONE ) {
                MyLongTermKeyInfo my_ltk { device->getAddressAndType(),
                                           device->getLongTermKeyInfo(true /* responder */) };
                my_ltk.write(my_ltk.address_and_type.toString()+".resp.ltk");
            }

            if( ( SMPKeyType::SIGN_KEY & keys_init ) != SMPKeyType::NONE ) {
                MySignatureResolvingKeyInfo my_csrk { device->getAddressAndType(),
                                                      device->getSignatureResolvingKeyInfo(false /* responder */) };
                my_csrk.write(my_csrk.address_and_type.toString()+".init.csrk");
            }
            if( ( SMPKeyType::SIGN_KEY & keys_resp ) != SMPKeyType::NONE ) {
                MySignatureResolvingKeyInfo my_csrk { device->getAddressAndType(),
                                                      device->getSignatureResolvingKeyInfo(true /* responder */) };
                my_csrk.write(my_csrk.address_and_type.toString()+".resp.csrk");
            }
        }
    }
#if 1
    //
    // GATT Service Processing
    //
    fprintf(stderr, "****** Processing Ready Device: GATT start: %s\n", device->getAddressAndType().toString().c_str());
    if( !QUIET ) {
        device->getAdapter().printSharedPtrListOfDevices();
    }
    try {
        std::vector<GATTServiceRef> primServices = device->getGATTServices();
        if( 0 == primServices.size() ) {
            fprintf(stderr, "****** Processing Ready Device: getServices() failed %s\n", device->toString().c_str());
            goto exit;
        }

        const uint64_t t5 = getCurrentMilliseconds();
        if( !QUIET ) {
            const uint64_t td01 = t1 - timestamp_t0; // adapter-init -> processing-start
            const uint64_t td15 = t5 - t1; // get-gatt-services
            const uint64_t tdc5 = t5 - device->getLastDiscoveryTimestamp(); // discovered to gatt-complete
            const uint64_t td05 = t5 - timestamp_t0; // adapter-init -> gatt-complete
            fprintf(stderr, "\n\n\n");
            fprintf(stderr, "PERF: GATT primary-services completed\n");
            fprintf(stderr, "PERF:  adapter-init to processing-start %" PRIu64 " ms,\n"
                            "PERF:  get-gatt-services %" PRIu64 " ms,\n"
                            "PERF:  discovered to gatt-complete %" PRIu64 " ms (connect %" PRIu64 " ms),\n"
                            "PERF:  adapter-init to gatt-complete %" PRIu64 " ms\n\n",
                            td01, td15, tdc5, (tdc5 - td15), td05);
        }
#if 0
        {
            // WIP: Implement a simple Characteristic ping-pong writeValue <-> notify transmission for stress testing.
            DBTManager & manager = device->getAdapter().getManager();
            if( nullptr != charIdentifier && charIdentifier.length() > 0 ) {
                GATTCharacteristic * char2 = (GATTCharacteristic*) nullptr;
                        // manager.find(BluetoothType.GATT_CHARACTERISTIC, null, charIdentifier, device);
                fprintf(stderr, "Char UUID %s\n", charIdentifier.c_str());
                fprintf(stderr, "  over device : %s\n", char2->toString().c_str());
                if( nullptr != char2 ) {
                    bool cccdEnableResult[2];
                    bool cccdRet = char2->addCharacteristicListener( std::shared_ptr<GATTCharacteristicListener>( new MyGATTEventListener(char2) ),
                                                                          cccdEnableResult );
                    if( !QUIET ) {
                        fprintf(stderr, "Added CharPingPongListenerRes Notification(%d), Indication(%d): Result %d\n",
                                cccdEnableResult[0], cccdEnableResult[1], cccdRet);
                    }
                    if( cccdRet ) {
                        uint8_t cmd[] { (uint8_t)charValue }; // request device model
                        bool wres = char2->writeValueNoResp(cmd);
                        if( !QUIET ) {
                            fprintf(stderr, "Write response: "+wres);
                        }
                    }
                }
            }
        }
#endif

        std::shared_ptr<GattGenericAccessSvc> ga = device->getGATTGenericAccess();
        if( nullptr != ga && !QUIET ) {
            fprintf(stderr, "  GenericAccess: %s\n\n", ga->toString().c_str());
        }
        {
            std::shared_ptr<GATTHandler> gatt = device->getGATTHandler();
            if( nullptr != gatt && gatt->isConnected() ) {
                std::shared_ptr<GattDeviceInformationSvc> di = gatt->getDeviceInformation(primServices);
                if( nullptr != di && !QUIET ) {
                    fprintf(stderr, "  DeviceInformation: %s\n\n", di->toString().c_str());
                }
            }
        }

        for(size_t i=0; i<primServices.size(); i++) {
            GATTService & primService = *primServices.at(i);
            if( !QUIET ) {
                fprintf(stderr, "  [%2.2d] Service %s\n", (int)i, primService.toString().c_str());
                fprintf(stderr, "  [%2.2d] Service Characteristics\n", (int)i);
            }
            std::vector<GATTCharacteristicRef> & serviceCharacteristics = primService.characteristicList;
            for(size_t j=0; j<serviceCharacteristics.size(); j++) {
                GATTCharacteristic & serviceChar = *serviceCharacteristics.at(j);
                if( !QUIET ) {
                    fprintf(stderr, "  [%2.2d.%2.2d] CharDef: %s\n", (int)i, (int)j, serviceChar.toString().c_str());
                }
                if( serviceChar.hasProperties(GATTCharacteristic::PropertyBitVal::Read) ) {
                    POctets value(GATTHandler::number(GATTHandler::Defaults::MAX_ATT_MTU), 0);
                    if( serviceChar.readValue(value) ) {
                        std::string sval = dfa_utf8_decode(value.get_ptr(), value.getSize());
                        if( !QUIET ) {
                            fprintf(stderr, "  [%2.2d.%2.2d] CharVal: %s ('%s')\n", (int)i, (int)j, value.toString().c_str(), sval.c_str());
                        }
                    }
                }
                std::vector<GATTDescriptorRef> & charDescList = serviceChar.descriptorList;
                for(size_t k=0; k<charDescList.size(); k++) {
                    GATTDescriptor & charDesc = *charDescList.at(k);
                    if( !QUIET ) {
                        fprintf(stderr, "  [%2.2d.%2.2d.%2.2d] Desc: %s\n", (int)i, (int)j, (int)k, charDesc.toString().c_str());
                    }
                }
                bool cccdEnableResult[2];
                bool cccdRet = serviceChar.addCharacteristicListener( std::shared_ptr<GATTCharacteristicListener>( new MyGATTEventListener(&serviceChar) ),
                                                                      cccdEnableResult );
                if( !QUIET ) {
                    fprintf(stderr, "  [%2.2d.%2.2d] addCharacteristicListener Notification(%d), Indication(%d): Result %d\n",
                            (int)i, (int)j, cccdEnableResult[0], cccdEnableResult[1], cccdRet);
                }
            }
        }
        // FIXME sleep 1s for potential callbacks ..
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        success = true;
    } catch ( std::exception & e ) {
        fprintf(stderr, "****** Processing Ready Device: Exception caught for %s: %s\n", device->toString().c_str(), e.what());
    }

exit:
    if( !USE_WHITELIST && 0 == getDeviceProcessingCount() ) {
        startDiscovery(&device->getAdapter(), "post-processing-1");
    }

    if( KEEP_CONNECTED && GATT_PING_ENABLED && success ) {
        while( device->pingGATT() ) {
            fprintf(stderr, "****** Processing Ready Device: pingGATT OK: %s\n", device->getAddressAndType().toString().c_str());
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        fprintf(stderr, "****** Processing Ready Device: pingGATT failed, waiting for disconnect: %s\n", device->getAddressAndType().toString().c_str());
        // Even w/ GATT_PING_ENABLED, we utilize disconnect event to clean up -> remove
    }

    if( !QUIET ) {
        device->getAdapter().printSharedPtrListOfDevices();
    }

#endif

    fprintf(stderr, "****** Processing Ready Device: End: Success %d on %s; devInProc %zu\n",
            success, device->toString().c_str(), getDeviceProcessingCount());

    if( success ) {
        addToDevicesProcessed(device->getAddressAndType());
    }

    if( !KEEP_CONNECTED ) {
        removeFromDevicesProcessing(device->getAddressAndType());

        if( UNPAIR_DEVICE_POST ) {
            const HCIStatusCode unpair_res = device->unpair();
            fprintf(stderr, "****** Processing Ready Device: Unpair-Post result: %s\n", getHCIStatusCodeString(unpair_res).c_str());
        }

        device->remove();

        if( 0 < RESET_ADAPTER_EACH_CONN && 0 == deviceReadyCount % RESET_ADAPTER_EACH_CONN ) {
            resetAdapter(&device->getAdapter(), 2);
        } else if( !USE_WHITELIST && 0 == getDeviceProcessingCount() ) {
            startDiscovery(&device->getAdapter(), "post-processing-2");
        }
    }

    if( 0 < MULTI_MEASUREMENTS ) {
        MULTI_MEASUREMENTS--;
        fprintf(stderr, "****** Processing Ready Device: MULTI_MEASUREMENTS left %d: %s\n", MULTI_MEASUREMENTS.load(), device->getAddressAndType().toString().c_str());
    }
}

static void removeDevice(std::shared_ptr<DBTDevice> device) {
    fprintf(stderr, "****** Remove Device: removing: %s\n", device->getAddressAndType().toString().c_str());
    device->getAdapter().stopDiscovery();

    removeFromDevicesProcessing(device->getAddressAndType());

    device->remove();

    if( !USE_WHITELIST && 0 == getDeviceProcessingCount() ) {
        startDiscovery(&device->getAdapter(), "post-remove-device");
    }
}

static void resetAdapter(DBTAdapter *a, int mode) {
    fprintf(stderr, "****** Reset Adapter: reset[%d] start: %s\n", mode, a->toString().c_str());
    HCIStatusCode res = a->reset();
    fprintf(stderr, "****** Reset Adapter: reset[%d] end: %s, %s\n", mode, getHCIStatusCodeString(res).c_str(), a->toString().c_str());
}

static bool startDiscovery(DBTAdapter *a, std::string msg) {
    HCIStatusCode status = a->startDiscovery( true );
    fprintf(stderr, "****** Start discovery (%s) result: %s\n", msg.c_str(), getHCIStatusCodeString(status).c_str());
    return HCIStatusCode::SUCCESS == status;
}

static std::shared_ptr<DBTAdapter> createAdapter(const int dev_id0, const bool validate_dev_id) {
    // pre-validate dev_id availability
    int dev_id;
    if( validate_dev_id ) {
        DBTManager & mngr = DBTManager::get();
        if( 0 > dev_id0 ) {
            dev_id = mngr.getDefaultAdapterDevID();
        } else if( nullptr != mngr.getAdapterInfo(dev_id0) ) {
            dev_id = dev_id0;
        } else {
            dev_id = -1;
        }
    } else {
        dev_id = dev_id0;
    }
    if( 0 > dev_id ) {
        fprintf(stderr, "Adapter not available (1): Request %d, deduced %d\n", dev_id0, dev_id);
        return nullptr;
    }

    std::shared_ptr<DBTAdapter> adapter(new DBTAdapter(dev_id)); // given dev_id >= 0 or default adapter (1st powered)
    if( !adapter->hasDevId() ) {
        fprintf(stderr, "Adapter not available (2): %d\n", dev_id); // should have been covered above
        return nullptr;
    }
    if( !adapter->isPowered() ) { // should have been covered above
        fprintf(stderr, "Adapter not powered (2): %s\n", adapter->toString().c_str());
        return nullptr;
    }

    adapter->addStatusListener(std::shared_ptr<AdapterStatusListener>(new MyAdapterStatusListener()));

    if( USE_WHITELIST ) {
        for (auto it = WHITELIST.begin(); it != WHITELIST.end(); ++it) {
            bool res = adapter->addDeviceToWhitelist(*it, HCIWhitelistConnectType::HCI_AUTO_CONN_ALWAYS);
            fprintf(stderr, "Added to WHITELIST: res %d, address %s\n", res, it->toString().c_str());
        }
    } else {
        if( !startDiscovery(adapter.get(), "kick-off") ) {
            return nullptr;
        }
    }
    return adapter;
}

static jau::cow_vector<std::shared_ptr<DBTAdapter>> adapterList;

static std::shared_ptr<DBTAdapter> getAdapter(const uint16_t dev_id) {
    std::shared_ptr<std::vector<std::shared_ptr<DBTAdapter>>> snapshot = adapterList.get_snapshot();
    auto begin = snapshot->begin();
    auto it = std::find_if(begin, snapshot->end(), [&](std::shared_ptr<DBTAdapter> const& p) -> bool {
        return p->dev_id == dev_id;
    });
    if ( it == std::end(*snapshot) ) {
        return nullptr;
    } else {
        return *it;
    }
}
static std::shared_ptr<DBTAdapter> removeAdapter(const uint16_t dev_id) {
    std::shared_ptr<DBTAdapter> res = nullptr;
    const std::lock_guard<std::recursive_mutex> lock(adapterList.get_write_mutex());
    std::shared_ptr<std::vector<std::shared_ptr<DBTAdapter>>> store = adapterList.copy_store();
    for(auto it = store->begin(); it != store->end(); ) {
        if ( (*it)->dev_id == dev_id ) {
            res = *it;
            it = store->erase(it);
            adapterList.set_store(std::move(store));
            return res;
        } else {
            ++it;
        }
    }
    return nullptr;
}

static bool myChangedAdapterSetFunc(const bool added, const AdapterInfo& adapterInfo) {
    if( added ) {
        std::shared_ptr<DBTAdapter> pre = getAdapter(adapterInfo.dev_id);
        if( nullptr != pre ) {
            fprintf(stderr, "****** Adapter ADDED__: Not new %s\n", pre->toString().c_str());
        } else {
            if( adapterInfo.isCurrentSettingBitSet(AdapterSetting::POWERED) ) {
                std::shared_ptr<DBTAdapter> adapter = createAdapter(adapterInfo.dev_id, false /* validate_dev_id */);
                if( nullptr != adapter ) {
                    adapterList.push_back(adapter);
                    fprintf(stderr, "****** Adapter ADDED__: Created %s\n", adapter->toString().c_str());
                }
            } else {
                fprintf(stderr, "****** Adapter ADDED__: Ignored %s\n", adapterInfo.toString().c_str());
            }
        }
    } else {
        std::shared_ptr<DBTAdapter> removed = removeAdapter(adapterInfo.dev_id);
        if( nullptr != removed ) {
            fprintf(stderr, "****** Adapter REMOVED: %s\n", removed->toString().c_str());
            removed->close();
        } else {
            fprintf(stderr, "****** Adapter REMOVED: Not found %s\n", adapterInfo.toString().c_str());
        }
    }
    return true;
}

void test() {
    bool done = false;

    timestamp_t0 = getCurrentMilliseconds();

    DBTManager & mngr = DBTManager::get();
    mngr.addChangedAdapterSetCallback(myChangedAdapterSetFunc);

    while( !done ) {
        if( 0 == MULTI_MEASUREMENTS ||
            ( -1 == MULTI_MEASUREMENTS && !waitForDevices.empty() && allDevicesProcessed(waitForDevices) )
          )
        {
            fprintf(stderr, "****** EOL Test MULTI_MEASUREMENTS left %d, processed %zu/%zu\n",
                    MULTI_MEASUREMENTS.load(), getDeviceProcessedCount(), waitForDevices.size());
            printList("****** WaitForDevice ", waitForDevices);
            printDevicesProcessed("****** DevicesProcessed ");
            done = true;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        }
    }

    jau::for_each_cow(adapterList, [](std::shared_ptr<DBTAdapter>& adapter) {
        fprintf(stderr, "****** EOL Adapter's Devices - pre close: %s\n", adapter->toString().c_str());
        adapter->printSharedPtrListOfDevices();
        adapter->close();
        fprintf(stderr, "****** EOL Adapter's Devices - post close\n");
        adapter->printSharedPtrListOfDevices();
    });
    fprintf(stderr, "****** EOL Adapter's Devices - post close all\n");

    {
        int count = mngr.removeChangedAdapterSetCallback(myChangedAdapterSetFunc);
        fprintf(stderr, "****** EOL Removed ChangedAdapterSetCallback %d\n", count);
    }
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
            btMode = getBTMode(argv[++i]);
            if( BTMode::NONE != btMode ) {
                setenv("direct_bt.mgmt.btmode", getBTModeString(btMode).c_str(), 1 /* overwrite */);
            }
        } else if( !strcmp("-wait", argv[i]) ) {
            waitForEnter = true;
        } else if( !strcmp("-show_update_events", argv[i]) ) {
            SHOW_UPDATE_EVENTS = true;
        } else if( !strcmp("-quiet", argv[i]) ) {
            QUIET = true;
        } else if( !strcmp("-mac", argv[i]) && argc > (i+1) ) {
            std::string macstr = std::string(argv[++i]);
            BDAddressAndType mac(EUI48(macstr), BDAddressType::BDADDR_UNDEFINED);
            waitForDevices.push_back( mac );
        } else if( !strcmp("-wl", argv[i]) && argc > (i+1) ) {
            std::string macstr = std::string(argv[++i]);
            BDAddressAndType wle(EUI48(macstr), BDAddressType::BDADDR_LE_PUBLIC);
            fprintf(stderr, "Whitelist + %s\n", wle.toString().c_str());
            WHITELIST.push_back( wle );
            USE_WHITELIST = true;
        } else if( !strcmp("-passkey", argv[i]) && argc > (i+1) ) {
            pairing_passkey = atoi(argv[++i]);
        } else if( !strcmp("-seclevel", argv[i]) && argc > (i+1) ) {
            sec_level = getBTSecurityLevel(atoi(argv[++i]));
        } else if( !strcmp("-iocap", argv[i]) && argc > (i+1) ) {
            io_capabilities = getSMPIOCapability(atoi(argv[++i]));
        } else if( !strcmp("-unpairPre", argv[i]) ) {
            UNPAIR_DEVICE_PRE = true;
        } else if( !strcmp("-unpairPost", argv[i]) ) {
            UNPAIR_DEVICE_POST = true;
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
                    "[-resetEachCon connectionCount] "
                    "(-mac <device_address>)* (-wl <device_address>)* "
                    "[-seclevel <int>] [-iocap <int>] [-passkey <digits>] "
                    "[-unpairPre] [-unpairPost] "
                    "[-charid <uuid>] [-charval <byte-val>] "
                    "[-dbt_verbose true|false] "
                    "[-dbt_debug true|false|adapter.event,gatt.data,hci.event,mgmt.event] "
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
    fprintf(stderr, "btmode %s\n", getBTModeString(btMode).c_str());
    fprintf(stderr, "passkey %d\n", pairing_passkey);
    fprintf(stderr, "seclevel %s\n", getBTSecurityLevelString(sec_level).c_str());
    fprintf(stderr, "iocap %s\n", getSMPIOCapabilityString(io_capabilities).c_str());
    fprintf(stderr, "UNPAIR_DEVICE_PRE %d\n", UNPAIR_DEVICE_PRE);
    fprintf(stderr, "UNPAIR_DEVICE_POST %d\n", UNPAIR_DEVICE_POST);
    fprintf(stderr, "characteristic-id: %s\n", charIdentifier.c_str());
    fprintf(stderr, "characteristic-value: %d\n", charValue);

    printList( "waitForDevice: ", waitForDevices);

    if( waitForEnter ) {
        fprintf(stderr, "Press ENTER to continue\n");
        getchar();
    }
    fprintf(stderr, "****** TEST start\n");
    test();
    fprintf(stderr, "****** TEST end\n");
    if( true ) {
        // Just for testing purpose, i.e. triggering DBTManager::close() within the test controlled app,
        // instead of program shutdown.
        fprintf(stderr, "****** Manager close start\n");
        DBTManager & mngr = DBTManager::get(); // already existing
        mngr.close();
        fprintf(stderr, "****** Manager close end\n");
    }
}
