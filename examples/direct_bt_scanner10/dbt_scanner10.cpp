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
#include <unordered_set>
#include <unordered_map>

#include <cinttypes>

#include <pthread.h>
#include <csignal>

#include <jau/cpp_lang_macros.hpp>
#include <jau/dfa_utf8_decode.hpp>
#include <jau/basic_algos.hpp>
#include <jau/darray.hpp>

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

const static int NO_PASSKEY = -1;
const static std::string KEY_PATH = "keys";

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
static jau::darray<BDAddressAndType> WHITELIST;

static std::string charIdentifier = "";
static int charValue = 0;

static bool SHOW_UPDATE_EVENTS = false;
static bool QUIET = false;

static jau::darray<BDAddressAndType> waitForDevices;

static void connectDiscoveredDevice(std::shared_ptr<BTDevice> device);

static void processReadyDevice(std::shared_ptr<BTDevice> device);

static void removeDevice(std::shared_ptr<BTDevice> device);
static void resetAdapter(BTAdapter *a, int mode);
static bool startDiscovery(BTAdapter *a, std::string msg);


static std::unordered_set<BDAddressAndType> devicesInProcessing;
static std::recursive_mutex mtx_devicesProcessing;

static std::recursive_mutex mtx_devicesProcessed;
static std::unordered_set<BDAddressAndType> devicesProcessed;

bool matches(jau::darray<BDAddressAndType> &cont, const BDAddressAndType &mac) {
    return cont.end() != jau::find_if(cont.begin(), cont.end(), [&](const BDAddressAndType & it)->bool {
       return it.matches(mac);
    });
}

void printList(const std::string &msg, jau::darray<BDAddressAndType> &cont) {
    fprintf(stderr, "%s ", msg.c_str());
    jau::for_each(cont.begin(), cont.end(),
            [](const BDAddressAndType &mac) { fprintf(stderr, "%s, ", mac.toString().c_str()); });
    fprintf(stderr, "\n");
}

void printList(const std::string &msg, std::unordered_set<BDAddressAndType> &cont) {
    fprintf(stderr, "%s ", msg.c_str());
    jau::for_each(cont.begin(), cont.end(),
            [](const BDAddressAndType &mac) { fprintf(stderr, "%s, ", mac.toString().c_str()); });
    fprintf(stderr, "\n");
}

static void addToDevicesProcessed(const BDAddressAndType &a) {
    const std::lock_guard<std::recursive_mutex> lock(mtx_devicesProcessed); // RAII-style acquire and relinquish via destructor
    devicesProcessed.insert(devicesProcessed.end(), a);
}
static bool isDeviceProcessed(const BDAddressAndType & a) {
    const std::lock_guard<std::recursive_mutex> lock(mtx_devicesProcessed); // RAII-style acquire and relinquish via destructor
    return devicesProcessed.end() != devicesProcessed.find(a);
}
static size_t getDeviceProcessedCount() {
    const std::lock_guard<std::recursive_mutex> lock(mtx_devicesProcessed); // RAII-style acquire and relinquish via destructor
    return devicesProcessed.size();
}
static bool allDevicesProcessed(jau::darray<BDAddressAndType> &cont) {
    const std::lock_guard<std::recursive_mutex> lock(mtx_devicesProcessed); // RAII-style acquire and relinquish via destructor
    for (auto it = cont.begin(); it != cont.end(); ++it) {
        if( devicesProcessed.end() == devicesProcessed.find(*it) ) {
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
    devicesInProcessing.insert(devicesInProcessing.end(), a);
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
    return devicesInProcessing.end() != devicesInProcessing.find(a);
}
static size_t getDeviceProcessingCount() {
    const std::lock_guard<std::recursive_mutex> lock(mtx_devicesProcessing); // RAII-style acquire and relinquish via destructor
    return devicesInProcessing.size();
}

struct MyBTSecurityDetail {
    BDAddressAndType addrAndType;

    BTSecurityLevel sec_level { BTSecurityLevel::UNSET };
    SMPIOCapability io_cap { SMPIOCapability::UNSET };
    int passkey = NO_PASSKEY;

    MyBTSecurityDetail(const BDAddressAndType& addrAndType_)
    : addrAndType(addrAndType_) {}

    const BTSecurityLevel& getSecLevel() { return sec_level; }

    const SMPIOCapability& getIOCapability() { return io_cap; }

    int getPairingPasskey() { return passkey; }

    int getPairingNumericComparison() { return 1; }

    std::string toString() const noexcept {
        return "MyBTSecurityDetail["+addrAndType.toString()+", lvl "+
                getBTSecurityLevelString(sec_level)+", io "+getSMPIOCapabilityString(io_cap)+
                ", passkey "+std::to_string(passkey)+"]";
    }

  private:
    static std::unordered_map<BDAddressAndType, MyBTSecurityDetail> devicesSecDetail;

  public:

    static MyBTSecurityDetail* get(const BDAddressAndType& addrAndType) {
        auto s = devicesSecDetail.find(addrAndType);
        if( s != devicesSecDetail.end() ) {
            return &(s->second);
        } else {
            return nullptr;
        }
    }
    static MyBTSecurityDetail* getOrCreate(const BDAddressAndType& addrAndType) {
        MyBTSecurityDetail* sec_detail = get(addrAndType);
        if( nullptr != sec_detail ) {
            return sec_detail;
        } else {
            auto i = devicesSecDetail.insert( devicesSecDetail.end(), { addrAndType, MyBTSecurityDetail(addrAndType) } );
            return &(i->second);
        }
    }
    static std::string allToString() {
        std::string res;
        int i=0;
        for(auto iter = devicesSecDetail.begin(); iter != devicesSecDetail.end(); ++iter, ++i) {
            const MyBTSecurityDetail& sec = iter->second;
            if( 0 < i ) {
                res += ", ";
            }
            res += sec.toString();
        }
        return res;
    }
};
std::unordered_map<BDAddressAndType, MyBTSecurityDetail> MyBTSecurityDetail::devicesSecDetail;

struct MyLongTermKeyInfo {
    constexpr static const uint16_t VERSION = (uint16_t)0b0101010101010101 + (uint16_t)0; // bitpattern + version
    uint16_t version;             //  2
    BDAddressAndType addrAndType; //  7
    BTSecurityLevel sec_level;    //  1
    SMPIOCapability io_cap;       //  1
    SMPLongTermKeyInfo smp_ltk;   // 28

    // 39 bytes

    MyLongTermKeyInfo(const BDAddressAndType& addrAndType_,
                      const BTSecurityLevel sec_level_, const SMPIOCapability io_cap_,
                      const SMPLongTermKeyInfo& smp_ltk_)
    : version(VERSION),
      addrAndType(addrAndType_), sec_level(sec_level_), io_cap(io_cap_),
      smp_ltk(smp_ltk_)
    {}

    MyLongTermKeyInfo()
    : version(VERSION),
      addrAndType(), sec_level(BTSecurityLevel::UNSET), io_cap(SMPIOCapability::UNSET),
      smp_ltk()
    { }

    constexpr_cxx20 std::string toString() const noexcept {
        return "LTKInfo["+addrAndType.toString()+", sec "+getBTSecurityLevelString(sec_level)+
                ", io "+getSMPIOCapabilityString(io_cap)+
                ", "+smp_ltk.toString()+", ver["+jau::uint16HexString(version)+", ok "+std::to_string(VERSION==version)+"]]";
    }

    constexpr bool isValid() const noexcept {
        return VERSION == version && BTSecurityLevel::ENC_ONLY <= sec_level && smp_ltk.isValid();
    }

    bool isResponder() const noexcept { return smp_ltk.isResponder(); }

    constexpr_cxx20 std::string getFilename() const noexcept {
        const std::string role = isResponder() ? "resp" : "init";
        return "bt_sec."+addrAndType.address.toString()+":"+std::to_string(number(addrAndType.type))+".ltk."+role+".bin";
    }
    static std::string getFilename(const BDAddressAndType& addrAndType_, const bool isResponder_) {
        const std::string role = isResponder_ ? "resp" : "init";
        return "bt_sec."+addrAndType_.address.toString()+":"+std::to_string(number(addrAndType_.type))+".ltk."+role+".bin";
    }

    bool write(const std::string path) {
        if( !isValid() ) {
            fprintf(stderr, "****** WRITE LTK: Invalid (skipped) %s\n", toString().c_str());
            return false;
        }
        std::ofstream file(path+"/"+getFilename(), std::ios::binary | std::ios::trunc);
        uint8_t buffer[2];
        jau::put_uint16(buffer, 0, version, true /* littleEndian */);
        file.write((char*)buffer, sizeof(version));
        file.write((char*)&addrAndType.address, sizeof(addrAndType.address));
        file.write((char*)&addrAndType.type, sizeof(addrAndType.type));
        file.write((char*)&sec_level, sizeof(sec_level));
        file.write((char*)&io_cap, sizeof(io_cap));
        file.write((char*)&smp_ltk, sizeof(smp_ltk));
        file.close();
        fprintf(stderr, "****** WRITE LTK: Stored %s\n", toString().c_str());
        return true;
    }

    bool read(const std::string path, const BDAddressAndType& addrAndType_, const bool isResponder_) {
        const std::string filename = path+"/"+getFilename(addrAndType_, isResponder_);
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open() ) {
            fprintf(stderr, "****** READ LTK failed: %s\n", filename.c_str());
            return false;
        }
        uint8_t buffer[2];
        file.read((char*)buffer, sizeof(version));
        version = jau::get_uint16(buffer, 0, true /* littleEndian */);
        file.read((char*)&addrAndType.address, sizeof(addrAndType.address));
        file.read((char*)&addrAndType.type, sizeof(addrAndType.type));
        file.read((char*)&sec_level, sizeof(sec_level));
        file.read((char*)&io_cap, sizeof(io_cap));
        file.read((char*)&smp_ltk, sizeof(smp_ltk));
        file.close();
        addrAndType.clearHash();
        fprintf(stderr, "****** READ LTK: %s\n", toString().c_str());
        return isValid();
    }
};

struct MySignatureResolvingKeyInfo {
    BDAddressAndType addrAndType;
    SMPSignatureResolvingKeyInfo smp_csrk;

    bool isResponder() const noexcept { return smp_csrk.isResponder(); }

    constexpr_cxx20 std::string getFilename() const noexcept {
        const std::string role = isResponder() ? "resp" : "init";
        return "bt_sec."+addrAndType.address.toString()+":"+std::to_string(number(addrAndType.type))+".csrk."+role+".bin";
    }
    static std::string getFilename(const BDAddressAndType& addrAndType_, const bool isResponder_) {
        const std::string role = isResponder_ ? "resp" : "init";
        return "bt_sec."+addrAndType_.address.toString()+":"+std::to_string(number(addrAndType_.type))+".csrk."+role+".bin";
    }

    bool write(const std::string path) {
        std::ofstream file(path+"/"+getFilename(), std::ios::binary | std::ios::trunc);
        file.write((char*)&addrAndType.address, sizeof(addrAndType.address));
        file.write((char*)&addrAndType.type, sizeof(addrAndType.type));
        file.write((char*)&smp_csrk, sizeof(smp_csrk));
        file.close();
        fprintf(stderr, "****** WRITE CSRK [%s, written]: %s\n",
                addrAndType.toString().c_str(),
                smp_csrk.toString().c_str());
        return true;
    }

    bool read(const std::string path, const BDAddressAndType& addrAndType_, const bool isResponder_) {
        const std::string filename = path+"/"+getFilename(addrAndType_, isResponder_);
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open() ) {
            fprintf(stderr, "****** READ CSRK [%s] failed\n", filename.c_str());
            return false;
        }
        file.read((char*)&addrAndType.address, sizeof(addrAndType.address));
        file.read((char*)&addrAndType.type, sizeof(addrAndType.type));
        file.read((char*)&smp_csrk, sizeof(smp_csrk));
        file.close();
        addrAndType.clearHash();
        fprintf(stderr, "****** READ CSRK %s: %s\n",
                addrAndType.toString().c_str(),
                smp_csrk.toString().c_str());
        return true;
    }
};

class MyAdapterStatusListener : public AdapterStatusListener {

    void adapterSettingsChanged(BTAdapter &a, const AdapterSetting oldmask, const AdapterSetting newmask,
                                const AdapterSetting changedmask, const uint64_t timestamp) override {
        const bool initialSetting = AdapterSetting::NONE == oldmask;
        if( initialSetting ) {
            fprintf(stderr, "****** SETTINGS_INITIAL: %s -> %s, changed %s\n", getAdapterSettingMaskString(oldmask).c_str(),
                    getAdapterSettingMaskString(newmask).c_str(), getAdapterSettingMaskString(changedmask).c_str());
        } else {
            fprintf(stderr, "****** SETTINGS_CHANGED: %s -> %s, changed %s\n", getAdapterSettingMaskString(oldmask).c_str(),
                    getAdapterSettingMaskString(newmask).c_str(), getAdapterSettingMaskString(changedmask).c_str());
        }
        fprintf(stderr, "Status BTAdapter:\n");
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

    void discoveringChanged(BTAdapter &a, const ScanType currentMeta, const ScanType changedType, const bool changedEnabled, const bool keepAlive, const uint64_t timestamp) override {
        fprintf(stderr, "****** DISCOVERING: meta %s, changed[%s, enabled %d, keepAlive %d]: %s\n",
                getScanTypeString(currentMeta).c_str(), getScanTypeString(changedType).c_str(), changedEnabled, keepAlive, a.toString().c_str());
        (void)timestamp;
    }

    bool deviceFound(std::shared_ptr<BTDevice> device, const uint64_t timestamp) override {
        (void)timestamp;

        if( BDAddressType::BDADDR_LE_PUBLIC != device->getAddressAndType().type
            && BLERandomAddressType::STATIC_PUBLIC != device->getAddressAndType().getBLERandomAddressType() ) {
            // Requires BREDR or LE Secure Connection support: WIP
            fprintf(stderr, "****** FOUND__-2: Skip non 'public LE' and non 'random static public LE' %s\n", device->toString(true).c_str());
            return false;
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
            return true;
        } else {
            fprintf(stderr, "****** FOUND__-1: NOP %s\n", device->toString(true).c_str());
            return false;
        }
    }

    void deviceUpdated(std::shared_ptr<BTDevice> device, const EIRDataType updateMask, const uint64_t timestamp) override {
        if( SHOW_UPDATE_EVENTS ) {
            fprintf(stderr, "****** UPDATED: %s of %s\n", getEIRDataMaskString(updateMask).c_str(), device->toString(true).c_str());
        }
        (void)timestamp;
    }

    void deviceConnected(std::shared_ptr<BTDevice> device, const uint16_t handle, const uint64_t timestamp) override {
        fprintf(stderr, "****** CONNECTED: %s\n", device->toString(true).c_str());
        (void)handle;
        (void)timestamp;
    }

    void devicePairingState(std::shared_ptr<BTDevice> device, const SMPPairingState state, const PairingMode mode, const uint64_t timestamp) override {
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
                const MyBTSecurityDetail* sec = MyBTSecurityDetail::get(device->getAddressAndType());
                if( nullptr != sec && sec->passkey != NO_PASSKEY ) {
                    std::thread dc(&BTDevice::setPairingPasskey, device, static_cast<uint32_t>(sec->passkey)); // @suppress("Invalid arguments")
                    dc.detach();
                } /* else {
                    std::thread dc(&BTDevice::setPairingPasskeyNegative, device); // @suppress("Invalid arguments")
                    dc.detach();
                } */
                // next: KEY_DISTRIBUTION or FAILED
              } break;
            case SMPPairingState::NUMERIC_COMPARE_EXPECTED: {
                std::thread dc(&BTDevice::setPairingNumericComparison, device, true); // @suppress("Invalid arguments")
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

    void deviceReady(std::shared_ptr<BTDevice> device, const uint64_t timestamp) override {
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

    void deviceDisconnected(std::shared_ptr<BTDevice> device, const HCIStatusCode reason, const uint16_t handle, const uint64_t timestamp) override {
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

class MyGATTEventListener : public AssociatedBTGattCharListener {
  public:

    MyGATTEventListener(const BTGattChar * characteristicMatch)
    : AssociatedBTGattCharListener(characteristicMatch) {}

    void notificationReceived(BTGattCharRef charDecl, const TROOctets& char_value, const uint64_t timestamp) override {
        const std::shared_ptr<BTDevice> dev = charDecl->getDeviceChecked();
        const uint64_t tR = getCurrentMilliseconds();
        fprintf(stderr, "****** GATT Notify (td %" PRIu64 " ms, dev-discovered %" PRIu64 " ms): From %s\n",
                (tR-timestamp), (tR-dev->getLastDiscoveryTimestamp()), dev->toString().c_str());
        if( nullptr != charDecl ) {
            fprintf(stderr, "****** decl %s\n", charDecl->toString().c_str());
        }
        fprintf(stderr, "****** rawv %s\n", char_value.toString().c_str());
    }

    void indicationReceived(BTGattCharRef charDecl,
                            const TROOctets& char_value, const uint64_t timestamp,
                            const bool confirmationSent) override
    {
        const std::shared_ptr<BTDevice> dev = charDecl->getDeviceChecked();
        const uint64_t tR = getCurrentMilliseconds();
        fprintf(stderr, "****** GATT Indication (confirmed %d, td(msg %" PRIu64 " ms, dev-discovered %" PRIu64 " ms): From %s\n",
                confirmationSent, (tR-timestamp), (tR-dev->getLastDiscoveryTimestamp()), dev->toString().c_str());
        if( nullptr != charDecl ) {
            fprintf(stderr, "****** decl %s\n", charDecl->toString().c_str());
            if( _TEMPERATURE_MEASUREMENT == *charDecl->value_type ) {
                std::shared_ptr<GattTemperatureMeasurement> temp = GattTemperatureMeasurement::get(char_value);
                if( nullptr != temp ) {
                    fprintf(stderr, "****** valu %s\n", temp->toString().c_str());
                }
            }
        }
        fprintf(stderr, "****** rawv %s\n", char_value.toString().c_str());
    }
};

static void connectDiscoveredDevice(std::shared_ptr<BTDevice> device) {
    fprintf(stderr, "****** Connecting Device: Start %s\n", device->toString().c_str());

    if( UNPAIR_DEVICE_PRE ) {
        const HCIStatusCode unpair_res = device->unpair();
        fprintf(stderr, "****** Connecting Device: Unpair-Pre result: %s\n", getHCIStatusCodeString(unpair_res).c_str());
    }

    device->getAdapter().stopDiscovery();

    bool useStoredLTKInfo = false;
    {
        MyLongTermKeyInfo my_ltk_resp;
        MyLongTermKeyInfo my_ltk_init;
        if( my_ltk_init.read(KEY_PATH, device->getAddressAndType(), false /* responder */) &&
            my_ltk_resp.read(KEY_PATH, device->getAddressAndType(), true /* responder */) &&
            device->setConnSecurity(my_ltk_init.sec_level, my_ltk_init.io_cap) &&
            HCIStatusCode::SUCCESS == device->setLongTermKeyInfo(my_ltk_init.smp_ltk) &&
            HCIStatusCode::SUCCESS == device->setLongTermKeyInfo(my_ltk_resp.smp_ltk) ) {
            fprintf(stderr, "****** Connecting Device: Loaded LTKs from file successfully\n");
            fprintf(stderr, "- init %s\n", my_ltk_init.toString().c_str());
            fprintf(stderr, "- resp %s\n", my_ltk_resp.toString().c_str());
            useStoredLTKInfo = true;
        }
    }
    if( !useStoredLTKInfo ) {
        const MyBTSecurityDetail* sec = MyBTSecurityDetail::get(device->getAddressAndType());
        if( nullptr != sec ) {
            bool res = device->setConnSecurityBest(sec->sec_level, sec->io_cap);
            fprintf(stderr, "****** Connecting Device: Using SecurityDetail %s, set OK %d\n", sec->toString().c_str(), res);
        } else {
            fprintf(stderr, "****** Connecting Device: No SecurityDetail for %s\n", device->getAddressAndType().toString().c_str());
        }
    }


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

static void processReadyDevice(std::shared_ptr<BTDevice> device) {
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
                MyLongTermKeyInfo my_ltk (device->getAddressAndType(),
                                          device->getConnSecurityLevel(), device->getConnIOCapability(),
                                          device->getLongTermKeyInfo(false /* responder */));
                my_ltk.write(KEY_PATH);
            }
            if( ( SMPKeyType::ENC_KEY & keys_resp ) != SMPKeyType::NONE ) {
                MyLongTermKeyInfo my_ltk (device->getAddressAndType(),
                                          device->getConnSecurityLevel(), device->getConnIOCapability(),
                                          device->getLongTermKeyInfo(true /* responder */));
                my_ltk.write(KEY_PATH);
            }

            if( ( SMPKeyType::SIGN_KEY & keys_init ) != SMPKeyType::NONE ) {
                MySignatureResolvingKeyInfo my_csrk { device->getAddressAndType(),
                                                      device->getSignatureResolvingKeyInfo(false /* responder */) };
                my_csrk.write(KEY_PATH);
            }
            if( ( SMPKeyType::SIGN_KEY & keys_resp ) != SMPKeyType::NONE ) {
                MySignatureResolvingKeyInfo my_csrk { device->getAddressAndType(),
                                                      device->getSignatureResolvingKeyInfo(true /* responder */) };
                my_csrk.write(KEY_PATH);
            }
        }
    }

    //
    // GATT Service Processing
    //
    fprintf(stderr, "****** Processing Ready Device: GATT start: %s\n", device->getAddressAndType().toString().c_str());
    if( !QUIET ) {
        device->getAdapter().printSharedPtrListOfDevices();
    }
    try {
        jau::darray<BTGattServiceRef> primServices = device->getGattServices();
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
            BTManager & manager = device->getAdapter().getManager();
            if( nullptr != charIdentifier && charIdentifier.length() > 0 ) {
                BTGattChar * char2 = (BTGattChar*) nullptr;
                        // manager.find(BluetoothType.GATT_CHARACTERISTIC, null, charIdentifier, device);
                fprintf(stderr, "Char UUID %s\n", charIdentifier.c_str());
                fprintf(stderr, "  over device : %s\n", char2->toString().c_str());
                if( nullptr != char2 ) {
                    bool cccdEnableResult[2];
                    bool cccdRet = char2->addCharListener( std::shared_ptr<BTGattCharListener>( new MyGATTEventListener(char2) ),
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

        std::shared_ptr<GattGenericAccessSvc> ga = device->getGattGenericAccess();
        if( nullptr != ga && !QUIET ) {
            fprintf(stderr, "  GenericAccess: %s\n\n", ga->toString().c_str());
        }
        {
            std::shared_ptr<BTGattHandler> gatt = device->getGattHandler();
            if( nullptr != gatt && gatt->isConnected() ) {
                std::shared_ptr<GattDeviceInformationSvc> di = gatt->getDeviceInformation(primServices);
                if( nullptr != di && !QUIET ) {
                    fprintf(stderr, "  DeviceInformation: %s\n\n", di->toString().c_str());
                }
            }
        }

        for(size_t i=0; i<primServices.size(); i++) {
            BTGattService & primService = *primServices.at(i);
            if( !QUIET ) {
                // fprintf(stderr, "  [%2.2d] Service %s\n", (int)i, primService.toString().c_str());
                fprintf(stderr, "  [%2.2d] Service Characteristics\n", (int)i);
            }
            jau::darray<BTGattCharRef> & serviceCharacteristics = primService.characteristicList;
            for(size_t j=0; j<serviceCharacteristics.size(); j++) {
                BTGattChar & serviceChar = *serviceCharacteristics.at(j);
                if( !QUIET ) {
                    fprintf(stderr, "  [%2.2d.%2.2d] CharDef: %s\n", (int)i, (int)j, serviceChar.toString().c_str());
                }
                if( serviceChar.hasProperties(BTGattChar::PropertyBitVal::Read) ) {
                    POctets value(BTGattHandler::number(BTGattHandler::Defaults::MAX_ATT_MTU), 0);
                    if( serviceChar.readValue(value) ) {
                        std::string sval = dfa_utf8_decode(value.get_ptr(), value.getSize());
                        if( !QUIET ) {
                            fprintf(stderr, "  [%2.2d.%2.2d] CharVal: %s ('%s')\n", (int)i, (int)j, value.toString().c_str(), sval.c_str());
                        }
                    }
                }
                jau::darray<BTGattDescRef> & charDescList = serviceChar.descriptorList;
                for(size_t k=0; k<charDescList.size(); k++) {
                    BTGattDesc & charDesc = *charDescList.at(k);
                    if( !QUIET ) {
                        fprintf(stderr, "  [%2.2d.%2.2d.%2.2d] Desc: %s\n", (int)i, (int)j, (int)k, charDesc.toString().c_str());
                    }
                }
                bool cccdEnableResult[2];
                bool cccdRet = serviceChar.addCharListener( std::shared_ptr<BTGattCharListener>( new MyGATTEventListener(&serviceChar) ),
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
    fprintf(stderr, "****** Processing Ready Device: End-1: Success %d on %s; devInProc %zu\n",
            success, device->toString().c_str(), getDeviceProcessingCount());

    removeFromDevicesProcessing(device->getAddressAndType());

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

    fprintf(stderr, "****** Processing Ready Device: End-2: Success %d on %s; devInProc %zu\n",
            success, device->toString().c_str(), getDeviceProcessingCount());

    if( success ) {
        addToDevicesProcessed(device->getAddressAndType());
    }

    if( !KEEP_CONNECTED ) {

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

static void removeDevice(std::shared_ptr<BTDevice> device) {
    fprintf(stderr, "****** Remove Device: removing: %s\n", device->getAddressAndType().toString().c_str());
    device->getAdapter().stopDiscovery();

    removeFromDevicesProcessing(device->getAddressAndType());

    device->remove();

    if( !USE_WHITELIST && 0 == getDeviceProcessingCount() ) {
        startDiscovery(&device->getAdapter(), "post-remove-device");
    }
}

static void resetAdapter(BTAdapter *a, int mode) {
    fprintf(stderr, "****** Reset Adapter: reset[%d] start: %s\n", mode, a->toString().c_str());
    HCIStatusCode res = a->reset();
    fprintf(stderr, "****** Reset Adapter: reset[%d] end: %s, %s\n", mode, getHCIStatusCodeString(res).c_str(), a->toString().c_str());
}

static bool startDiscovery(BTAdapter *a, std::string msg) {
    HCIStatusCode status = a->startDiscovery( true );
    fprintf(stderr, "****** Start discovery (%s) result: %s\n", msg.c_str(), getHCIStatusCodeString(status).c_str());
    return HCIStatusCode::SUCCESS == status;
}

static bool initAdapter(std::shared_ptr<BTAdapter>& adapter) {
    if( !adapter->isPowered() ) { // should have been covered above
        fprintf(stderr, "Adapter not powered (2): %s\n", adapter->toString().c_str());
        return false;
    }
    adapter->addStatusListener(std::shared_ptr<AdapterStatusListener>(new MyAdapterStatusListener()));

    // Flush discovered devices after registering our status listener.
    // This avoids discovered devices before we have registered!
    if( 0 == waitForDevices.size() ) {
        // we accept all devices, so flush all discovered devices
        adapter->removeDiscoveredDevices();
    } else {
        // only flush discovered devices we intend to listen to
        jau::for_each(waitForDevices.begin(), waitForDevices.end(), [&adapter](const BDAddressAndType &mac) {
            adapter->removeDiscoveredDevice(mac);
        });
    }

    if( USE_WHITELIST ) {
        for (auto it = WHITELIST.begin(); it != WHITELIST.end(); ++it) {
            bool res = adapter->addDeviceToWhitelist(*it, HCIWhitelistConnectType::HCI_AUTO_CONN_ALWAYS);
            fprintf(stderr, "Added to WHITELIST: res %d, address %s\n", res, it->toString().c_str());
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
            fprintf(stderr, "****** Adapter ADDED__: InitOK. %s\n", adapter->toString().c_str());
        } else {
            fprintf(stderr, "****** Adapter ADDED__: Ignored %s\n", adapter->toString().c_str());
        }
    } else {
        fprintf(stderr, "****** Adapter REMOVED: %s\n", adapter->toString().c_str());
    }
    return true;
}

void test() {
    bool done = false;

    timestamp_t0 = getCurrentMilliseconds();

    BTManager & mngr = BTManager::get();
    mngr.addChangedAdapterSetCallback(myChangedAdapterSetFunc);

    while( !done ) {
        if( 0 == MULTI_MEASUREMENTS ||
            ( -1 == MULTI_MEASUREMENTS && !waitForDevices.empty() && allDevicesProcessed(waitForDevices) )
          )
        {
            fprintf(stderr, "****** EOL Test MULTI_MEASUREMENTS left %d, processed %zu/%zu\n",
                    MULTI_MEASUREMENTS.load(), getDeviceProcessedCount(), (size_t)waitForDevices.size());
            printList("****** WaitForDevice ", waitForDevices);
            printDevicesProcessed("****** DevicesProcessed ");
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
        fprintf(stderr, "****** EOL Adapter's Devices - pre close: %s\n", adapter->toString().c_str());
        adapter->printSharedPtrListOfDevices();
    });
    {
        int count = mngr.removeChangedAdapterSetCallback(myChangedAdapterSetFunc);
        fprintf(stderr, "****** EOL Removed ChangedAdapterSetCallback %d\n", count);

        mngr.close();
    }
    jau::for_each_const(adapterList, [](const std::shared_ptr<BTAdapter>& adapter) {
        fprintf(stderr, "****** EOL Adapter's Devices - post close: %s\n", adapter->toString().c_str());
        adapter->printSharedPtrListOfDevices();
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
        } else if( !strcmp("-passkey", argv[i]) && argc > (i+3) ) {
            const char* mac = argv[++i];
            const uint8_t atype = (uint8_t) ( atoi(argv[++i]) & 0xff );
            const BDAddressAndType macAndType(EUI48(mac), getBDAddressType(atype));
            MyBTSecurityDetail* sec = MyBTSecurityDetail::getOrCreate(macAndType);
            sec->passkey = atoi(argv[++i]);
            fprintf(stderr, "Set passkey in %s\n", sec->toString().c_str());
        } else if( !strcmp("-seclevel", argv[i]) && argc > (i+3) ) {
            const char* mac = argv[++i];
            const uint8_t atype = (uint8_t) ( atoi(argv[++i]) & 0xff );
            const BDAddressAndType macAndType(EUI48(mac), getBDAddressType(atype));
            MyBTSecurityDetail* sec = MyBTSecurityDetail::getOrCreate(macAndType);
            sec->sec_level = getBTSecurityLevel(atoi(argv[++i]));
            fprintf(stderr, "Set sec_level in %s\n", sec->toString().c_str());
        } else if( !strcmp("-iocap", argv[i]) && argc > (i+3) ) {
            const char* mac = argv[++i];
            const uint8_t atype = (uint8_t) ( atoi(argv[++i]) & 0xff );
            const BDAddressAndType macAndType(EUI48(mac), getBDAddressType(atype));
            MyBTSecurityDetail* sec = MyBTSecurityDetail::getOrCreate(macAndType);
            sec->io_cap = getSMPIOCapability(atoi(argv[++i]));
            fprintf(stderr, "Set io_cap in %s\n", sec->toString().c_str());
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
                    "[-seclevel <device_address> <(int)address_type> <int>] "
                    "[-iocap <device_address> <(int)address_type> <int>] "
                    "[-passkey <device_address> <(int)address_type> <digits>] "
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
    fprintf(stderr, "UNPAIR_DEVICE_PRE %d\n", UNPAIR_DEVICE_PRE);
    fprintf(stderr, "UNPAIR_DEVICE_POST %d\n", UNPAIR_DEVICE_POST);
    fprintf(stderr, "characteristic-id: %s\n", charIdentifier.c_str());
    fprintf(stderr, "characteristic-value: %d\n", charValue);

    fprintf(stderr, "security-details: %s\n", MyBTSecurityDetail::allToString().c_str());
    printList( "waitForDevice: ", waitForDevices);


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
