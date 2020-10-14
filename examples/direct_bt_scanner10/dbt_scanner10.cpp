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

#include <cinttypes>

#include <pthread.h>

#include <direct_bt/DirectBT.hpp>

#include "direct_bt/dfa_utf8_decode.hpp"

extern "C" {
    #include <unistd.h>
}

using namespace direct_bt;

/**
 * This C++ scanner example uses the Direct-BT fully event driven workflow
 * and adds multithreading, i.e. one thread processes each found device found
 * as notified via the event listener.
 * <p>
 * This example represents the recommended utilization of Direct-BT.
 * </p>
 */

static int64_t timestamp_t0;


static int RESET_ADAPTER_EACH_CONN = 0;
static std::atomic<int> connectionCount = 0;

static std::atomic<int> MULTI_MEASUREMENTS = 8;

static bool KEEP_CONNECTED = true;
static bool GATT_PING_ENABLED = false;
static bool REMOVE_DEVICE = true;

static bool USE_WHITELIST = false;
static std::vector<EUI48> WHITELIST;

static bool SHOW_UPDATE_EVENTS = false;
static bool QUIET = false;

static std::vector<EUI48> waitForDevices;

static void connectDiscoveredDevice(std::shared_ptr<DBTDevice> device);

static void processConnectedDevice(std::shared_ptr<DBTDevice> device);

static void removeDevice(std::shared_ptr<DBTDevice> device);
static void resetAdapter(DBTAdapter *a, int mode);
static bool startDiscovery(DBTAdapter *a, std::string msg);

static std::vector<EUI48> devicesInProcessing;
static std::recursive_mutex mtx_devicesProcessing;

static std::recursive_mutex mtx_devicesProcessed;
static std::vector<EUI48> devicesProcessed;

bool contains(std::vector<EUI48> &cont, const EUI48 &mac) {
    return cont.end() != find(cont.begin(), cont.end(), mac);
}

void printList(const std::string &msg, std::vector<EUI48> &cont) {
    fprintf(stderr, "%s ", msg.c_str());
    std::for_each(cont.begin(), cont.end(),
            [](const EUI48 &mac) { fprintf(stderr, "%s, ", mac.toString().c_str()); });
    fprintf(stderr, "\n");
}

static void addToDevicesProcessed(const EUI48 &a) {
    const std::lock_guard<std::recursive_mutex> lock(mtx_devicesProcessed); // RAII-style acquire and relinquish via destructor
    devicesProcessed.push_back(a);
}
static bool isDeviceProcessed(const EUI48 & a) {
    const std::lock_guard<std::recursive_mutex> lock(mtx_devicesProcessed); // RAII-style acquire and relinquish via destructor
    return contains(devicesProcessed, a);
}
static size_t getDeviceProcessedCount() {
    const std::lock_guard<std::recursive_mutex> lock(mtx_devicesProcessed); // RAII-style acquire and relinquish via destructor
    return devicesProcessed.size();
}
static bool allDevicesProcessed(std::vector<EUI48> &cont) {
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

static void addToDevicesProcessing(const EUI48 &a) {
    const std::lock_guard<std::recursive_mutex> lock(mtx_devicesProcessing); // RAII-style acquire and relinquish via destructor
    devicesInProcessing.push_back(a);
}
static bool removeFromDevicesProcessing(const EUI48 &a) {
    const std::lock_guard<std::recursive_mutex> lock(mtx_devicesProcessing); // RAII-style acquire and relinquish via destructor
    for (auto it = devicesInProcessing.begin(); it != devicesInProcessing.end(); ++it) {
        if ( a == *it ) {
            devicesInProcessing.erase(it);
            return true;
        }
    }
    return false;
}
static bool isDeviceProcessing(const EUI48 & a) {
    const std::lock_guard<std::recursive_mutex> lock(mtx_devicesProcessing); // RAII-style acquire and relinquish via destructor
    return contains(devicesInProcessing, a);
}
static size_t getDeviceProcessingCount() {
    const std::lock_guard<std::recursive_mutex> lock(mtx_devicesProcessing); // RAII-style acquire and relinquish via destructor
    return devicesInProcessing.size();
}

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

    void discoveringChanged(DBTAdapter &a, const bool enabled, const bool keepAlive, const uint64_t timestamp) override {
        fprintf(stderr, "****** DISCOVERING: enabled %d, keepAlive %d: %s\n", enabled, keepAlive, a.toString().c_str());
        (void)timestamp;
    }

    void deviceFound(std::shared_ptr<DBTDevice> device, const uint64_t timestamp) override {
        (void)timestamp;

        if( BDAddressType::BDADDR_LE_PUBLIC != device->getAddressType()
            && BLERandomAddressType::STATIC_PUBLIC != device->getBLERandomAddressType() ) {
            // Requires BREDR or LE Secure Connection support: WIP
            fprintf(stderr, "****** FOUND__-2: Skip non 'public LE' and non 'random static public LE' %s\n", device->toString(true).c_str());
            return;
        }
        if( !isDeviceProcessing( device->getAddress() ) &&
            ( waitForDevices.empty() ||
              ( contains(waitForDevices, device->getAddress()) &&
                ( 0 < MULTI_MEASUREMENTS || !isDeviceProcessed(device->getAddress()) )
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
        (void)handle;
        (void)timestamp;

        if( !isDeviceProcessing( device->getAddress() ) &&
            ( waitForDevices.empty() ||
              ( contains(waitForDevices, device->getAddress()) &&
                ( 0 < MULTI_MEASUREMENTS || !isDeviceProcessed(device->getAddress()) )
              )
            )
          )
        {
            connectionCount++;
            fprintf(stderr, "****** CONNECTED-0: Processing[%d] %s\n", connectionCount.load(), device->toString(true).c_str());
            {
                const uint64_t td = getCurrentMilliseconds() - timestamp_t0; // adapter-init -> now
                fprintf(stderr, "PERF: adapter-init -> CONNECTED-0  %" PRIu64 " ms\n", td);
            }
            addToDevicesProcessing(device->getAddress());
            std::thread dc(::processConnectedDevice, device); // @suppress("Invalid arguments")
            dc.detach();
        } else {
            fprintf(stderr, "****** CONNECTED-1: NOP %s\n", device->toString(true).c_str());
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
            removeFromDevicesProcessing(device->getAddress());
        }
        if( 0 < RESET_ADAPTER_EACH_CONN && 0 == connectionCount % RESET_ADAPTER_EACH_CONN ) {
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

    void notificationReceived(GATTCharacteristicRef charDecl, std::shared_ptr<TROOctets> charValue, const uint64_t timestamp) override {
        const std::shared_ptr<DBTDevice> dev = charDecl->getDeviceChecked();
        const int64_t tR = getCurrentMilliseconds();
        fprintf(stderr, "****** GATT Notify (td %" PRIu64 " ms, dev-discovered %" PRIu64 " ms): From %s\n",
                (tR-timestamp), (tR-dev->getLastDiscoveryTimestamp()), dev->toString().c_str());
        if( nullptr != charDecl ) {
            fprintf(stderr, "****** decl %s\n", charDecl->toString().c_str());
        }
        fprintf(stderr, "****** rawv %s\n", charValue->toString().c_str());
    }

    void indicationReceived(GATTCharacteristicRef charDecl,
                            std::shared_ptr<TROOctets> charValue, const uint64_t timestamp,
                            const bool confirmationSent) override
    {
        const std::shared_ptr<DBTDevice> dev = charDecl->getDeviceChecked();
        const int64_t tR = getCurrentMilliseconds();
        fprintf(stderr, "****** GATT Indication (confirmed %d, td(msg %" PRIu64 " ms, dev-discovered %" PRIu64 " ms): From %s\n",
                confirmationSent, (tR-timestamp), (tR-dev->getLastDiscoveryTimestamp()), dev->toString().c_str());
        if( nullptr != charDecl ) {
            fprintf(stderr, "****** decl %s\n", charDecl->toString().c_str());
            if( _TEMPERATURE_MEASUREMENT == *charDecl->value_type ) {
                std::shared_ptr<GattTemperatureMeasurement> temp = GattTemperatureMeasurement::get(*charValue);
                if( nullptr != temp ) {
                    fprintf(stderr, "****** valu %s\n", temp->toString().c_str());
                }
            }
        }
        fprintf(stderr, "****** rawv %s\n", charValue->toString().c_str());
    }
};

static void connectDiscoveredDevice(std::shared_ptr<DBTDevice> device) {
    fprintf(stderr, "****** Connecting Device: Start %s\n", device->toString().c_str());
    device->getAdapter().stopDiscovery();
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

static void processConnectedDevice(std::shared_ptr<DBTDevice> device) {
    fprintf(stderr, "****** Processing Device: Start %s\n", device->toString().c_str());
    device->getAdapter().stopDiscovery(); // make sure for pending connections on failed connect*(..) command
    const uint64_t t1 = getCurrentMilliseconds();
    bool success = false;

    // Secure Pairing
    {
        std::vector<PairingMode> spm = device->getSupportedPairingModes();
        if( !QUIET ) {
            fprintf(stderr, "Supported Secure Pairing Modes: ");
            std::for_each(spm.begin(), spm.end(), [](PairingMode pm) { fprintf(stderr, "%s, ", getPairingModeString(pm).c_str()); } );
            fprintf(stderr, "\n");
        }

        std::vector<PairingMode> rpm = device->getRequiredPairingModes();
        if( !QUIET ) {
            fprintf(stderr, "Required Secure Pairing Modes: ");
            std::for_each(rpm.begin(), rpm.end(), [](PairingMode pm) { fprintf(stderr, "%s, ", getPairingModeString(pm).c_str()); } );
            fprintf(stderr, "\n");
        }

        if( spm.end() != std::find (spm.begin(), spm.end(), PairingMode::JUST_WORKS) ) {
            const std::vector<int> passkey; // empty for JustWorks
            HCIStatusCode res = device->pair(""); // empty for JustWorks
            fprintf(stderr, "Secure Pairing Just Works result %s of %s\n", getHCIStatusCodeString(res).c_str(), device->toString().c_str());
        } else if( spm.end() != std::find (spm.begin(), spm.end(), PairingMode::PASSKEY_ENTRY) ) {
            HCIStatusCode res = device->pair("111111"); // PasskeyEntry
            fprintf(stderr, "Secure Pairing Passkey Entry result %s of %s\n", getHCIStatusCodeString(res).c_str(), device->toString().c_str());
        } else if( !QUIET ) {
            fprintf(stderr, "Secure Pairing JUST_WORKS or PASSKEY_ENTRY not supported %s\n", device->toString().c_str());
        }
    }

    //
    // GATT Service Processing
    //
    fprintf(stderr, "****** Processing Device: GATT start: %s\n", device->getAddressString().c_str());
    if( !QUIET ) {
        device->getAdapter().printSharedPtrListOfDevices();
    }
    try {
        std::vector<GATTServiceRef> primServices = device->getGATTServices(); // implicit GATT connect...
        if( 0 == primServices.size() ) {
            fprintf(stderr, "****** Processing Device: getServices() failed %s\n", device->toString().c_str());
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
        fprintf(stderr, "****** Processing Device: Exception caught for %s: %s\n", device->toString().c_str(), e.what());
    }

exit:
    if( !USE_WHITELIST && 0 == getDeviceProcessingCount() ) {
        startDiscovery(&device->getAdapter(), "post-processing-1");
    }

    if( KEEP_CONNECTED && GATT_PING_ENABLED && success ) {
        while( device->pingGATT() ) {
            fprintf(stderr, "****** Processing Device: pingGATT OK: %s\n", device->getAddressString().c_str());
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        fprintf(stderr, "****** Processing Device: pingGATT failed, waiting for disconnect: %s\n", device->getAddressString().c_str());
        // Even w/ GATT_PING_ENABLED, we utilize disconnect event to clean up -> remove
    }

    if( !QUIET ) {
        device->getAdapter().printSharedPtrListOfDevices();
    }

    fprintf(stderr, "****** Processing Device: End: Success %d on %s; devInProc %zd\n",
            success, device->toString().c_str(), getDeviceProcessingCount());

    if( success ) {
        addToDevicesProcessed(device->getAddress());
    }

    if( !KEEP_CONNECTED ) {
        removeFromDevicesProcessing(device->getAddress());

        device->remove();

        if( 0 < RESET_ADAPTER_EACH_CONN && 0 == connectionCount % RESET_ADAPTER_EACH_CONN ) {
            resetAdapter(&device->getAdapter(), 2);
        } else if( !USE_WHITELIST && 0 == getDeviceProcessingCount() ) {
            startDiscovery(&device->getAdapter(), "post-processing-2");
        }
    }

    if( 0 < MULTI_MEASUREMENTS ) {
        MULTI_MEASUREMENTS--;
        fprintf(stderr, "****** Processing Device: MULTI_MEASUREMENTS left %d: %s\n", MULTI_MEASUREMENTS.load(), device->getAddressString().c_str());
    }
}

static void removeDevice(std::shared_ptr<DBTDevice> device) {
    fprintf(stderr, "****** Remove Device: removing: %s\n", device->getAddressString().c_str());
    device->getAdapter().stopDiscovery();

    removeFromDevicesProcessing(device->getAddress());

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

void test(int dev_id) {
    bool done = false;

    timestamp_t0 = getCurrentMilliseconds();

    DBTAdapter adapter(dev_id);
    if( !adapter.hasDevId() ) {
        fprintf(stderr, "Default adapter not available.\n");
        exit(1);
    }
    if( !adapter.isValid() ) {
        fprintf(stderr, "Adapter invalid.\n");
        exit(1);
    }
    if( !adapter.isEnabled() ) {
        fprintf(stderr, "Adapter not enabled: device %s, address %s: %s\n",
                adapter.getName().c_str(), adapter.getAddressString().c_str(), adapter.toString().c_str());
        exit(1);
    }
    fprintf(stderr, "Using adapter: device %s, address %s: %s\n",
        adapter.getName().c_str(), adapter.getAddressString().c_str(), adapter.toString().c_str());

    adapter.addStatusListener(std::shared_ptr<AdapterStatusListener>(new MyAdapterStatusListener()));

    if( USE_WHITELIST ) {
        for (auto it = WHITELIST.begin(); it != WHITELIST.end(); ++it) {
            bool res = adapter.addDeviceToWhitelist(*it, BDAddressType::BDADDR_LE_PUBLIC, HCIWhitelistConnectType::HCI_AUTO_CONN_ALWAYS);
            fprintf(stderr, "Added to WHITELIST: res %d, address %s\n", res, it->toString().c_str());
        }
    } else {
        if( !startDiscovery(&adapter, "kick-off") ) {
            done = true;
        }
    }

    while( !done ) {
        if( 0 == MULTI_MEASUREMENTS ||
            ( -1 == MULTI_MEASUREMENTS && !waitForDevices.empty() && allDevicesProcessed(waitForDevices) )
          )
        {
            fprintf(stderr, "****** EOL Test MULTI_MEASUREMENTS left %d, processed %zd/%zd\n",
                    MULTI_MEASUREMENTS.load(), getDeviceProcessedCount(), waitForDevices.size());
            printList("****** WaitForDevice ", waitForDevices);
            printDevicesProcessed("****** DevicesProcessed ");
            done = true;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        }
    }
    fprintf(stderr, "****** EOL Adapter's Devices\n");
    adapter.printSharedPtrListOfDevices();
}

#include <cstdio>

int main(int argc, char *argv[])
{
    int dev_id = 0; // default
    BTMode btMode = BTMode::NONE;
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
        } else if( !strcmp("-dev_id", argv[i]) && argc > (i+1) ) {
            dev_id = atoi(argv[++i]);
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
            waitForDevices.push_back( EUI48(macstr) );
        } else if( !strcmp("-wl", argv[i]) && argc > (i+1) ) {
            std::string macstr = std::string(argv[++i]);
            EUI48 wlmac(macstr);
            fprintf(stderr, "Whitelist + %s\n", wlmac.toString().c_str());
            WHITELIST.push_back( wlmac );
            USE_WHITELIST = true;
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

    fprintf(stderr, "Run with '[-dev_id <adapter-index>] [-btmode LE|BREDR|DUAL] "
                    "[-disconnect] [-enableGATTPing] [-count <number>] [-single] [-show_update_events] [-quiet] "
                    "[-resetEachCon connectionCount] "
                    "(-mac <device_address>)* (-wl <device_address>)* "
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
    fprintf(stderr, "dev_id %d\n", dev_id);
    fprintf(stderr, "btmode %s\n", getBTModeString(btMode).c_str());

    printList( "waitForDevice: ", waitForDevices);

    if( waitForEnter ) {
        fprintf(stderr, "Press ENTER to continue\n");
        getchar();
    }
    fprintf(stderr, "****** TEST start\n");
    test(dev_id);
    fprintf(stderr, "****** TEST end\n");
    if( true ) {
        // Just for testing purpose, i.e. triggering DBTManager::close() within the test controlled app,
        // instead of program shutdown.
        fprintf(stderr, "****** Manager close start\n");
        DBTManager & mngr = DBTManager::get(BTMode::NONE); // already existing
        mngr.close();
        fprintf(stderr, "****** Manager close end\n");
    }
}
