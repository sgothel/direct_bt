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

#include <cinttypes>

#include <jau/dfa_utf8_decode.hpp>

#include <direct_bt/DirectBT.hpp>

extern "C" {
    #include <unistd.h>
}

using namespace direct_bt;
using namespace jau;

/**
 * This C++ direct_bt scanner example is a TinyB backward compatible and not fully event driven.
 * It uses a more fine grained control via GATTHandler.
 * <p>
 * For a more user convenient and readable approach see dbt_scanner00.cpp or dbt_scanner10.cpp!
 * </p>
 * <p>
 * This example does not represent the recommended utilization of Direct-BT.
 * </p>
 */

std::shared_ptr<DBTDevice> deviceFound = nullptr;
std::mutex mtxDeviceFound;
std::condition_variable cvDeviceFound;

class MyAdapterStatusListener : public AdapterStatusListener {
    void adapterSettingsChanged(DBTAdapter &a, const AdapterSetting oldmask, const AdapterSetting newmask,
                                const AdapterSetting changedmask, const uint64_t timestamp) override {
        fprintf(stderr, "****** Native Adapter SETTINGS_CHANGED: %s -> %s, changed %s\n",
                getAdapterSettingMaskString(oldmask).c_str(),
                getAdapterSettingMaskString(newmask).c_str(),
                getAdapterSettingMaskString(changedmask).c_str());
        fprintf(stderr, "Status DBTAdapter:\n");
        fprintf(stderr, "%s\n", a.toString().c_str());
        (void)timestamp;
    }

    void discoveringChanged(DBTAdapter &a, const ScanType currentMeta, const ScanType changedType, const bool changedEnabled, const bool keepAlive, const uint64_t timestamp) override {
        fprintf(stderr, "****** DISCOVERING: meta %s, changed[%s, enabled %d, keepAlive %d]: %s\n",
                getScanTypeString(currentMeta).c_str(), getScanTypeString(changedType).c_str(), changedEnabled, keepAlive, a.toString().c_str());
        (void)timestamp;
    }

    void deviceFound(std::shared_ptr<DBTDevice> device, const uint64_t timestamp) override {
        fprintf(stderr, "****** FOUND__: %s\n", device->toString(true).c_str());
        fprintf(stderr, "Status Adapter:\n");
        fprintf(stderr, "%s\n", device->getAdapter().toString().c_str());
        {
            std::unique_lock<std::mutex> lockRead(mtxDeviceFound); // RAII-style acquire and relinquish via destructor
            ::deviceFound = device;
            cvDeviceFound.notify_all(); // notify waiting getter
        }
        (void)timestamp;
    }
    void deviceUpdated(std::shared_ptr<DBTDevice> device, const EIRDataType updateMask, const uint64_t timestamp) override {
        fprintf(stderr, "****** UPDATED: %s of %s\n", getEIRDataMaskString(updateMask).c_str(), device->toString(true).c_str());
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
    }
    void deviceReady(std::shared_ptr<DBTDevice> device, const uint64_t timestamp) override {
        fprintf(stderr, "****** READY: %s\n", device->toString().c_str());
        (void)timestamp;
    }
    void deviceDisconnected(std::shared_ptr<DBTDevice> device, const HCIStatusCode reason, const uint16_t handle, const uint64_t timestamp) override {
        fprintf(stderr, "****** DISCONNECTED: Reason 0x%X (%s), old handle %s: %s\n",
                static_cast<uint8_t>(reason), getHCIStatusCodeString(reason).c_str(),
                uint16HexString(handle).c_str(), device->toString(true).c_str());
        (void)handle;
        (void)timestamp;
    }

    std::string toString() const override {
        return "MyAdapterStatusListener[this "+aptrHexString(this)+"]";
    }
};

static const uuid16_t _TEMPERATURE_MEASUREMENT(GattCharacteristicType::TEMPERATURE_MEASUREMENT);

class MyGATTEventListener : public GATTCharacteristicListener {

    void notificationReceived(GATTCharacteristicRef charDecl,
                             const TROOctets& charValue, const uint64_t timestamp) override {
        const std::shared_ptr<DBTDevice> dev = charDecl->getDeviceChecked();
        const uint64_t tR = getCurrentMilliseconds();
        fprintf(stderr, "****** GATT Notify (td %" PRIu64 " ms, dev-discovered %" PRIu64 " ms): From %s\n",
                (tR-timestamp), (tR-dev->ts_creation), dev->toString().c_str());
        if( nullptr != charDecl ) {
            fprintf(stderr, "****** decl %s\n", charDecl->toString().c_str());
        }
        fprintf(stderr, "****** rawv %s\n", charValue.toString().c_str());
    }

    void indicationReceived(GATTCharacteristicRef charDecl,
                            const TROOctets& charValue, const uint64_t timestamp,
                            const bool confirmationSent) override
    {
        const std::shared_ptr<DBTDevice> dev = charDecl->getDeviceChecked();
        const uint64_t tR = getCurrentMilliseconds();
        fprintf(stderr, "****** GATT Indication (confirmed %d, td(msg %" PRIu64 " ms, dev-discovered %" PRIu64 " ms): From %s\n",
                confirmationSent, (tR-timestamp), (tR-dev->ts_creation), dev->toString().c_str());
        if( nullptr != charDecl ) {
            fprintf(stderr, "****** decl %s\n", charDecl->toString().c_str());
            if( _TEMPERATURE_MEASUREMENT == *charDecl->value_type ) {
                std::shared_ptr<GattTemperatureMeasurement> temp = GattTemperatureMeasurement::get(charValue);
                if( nullptr != temp ) {
                    fprintf(stderr, "****** valu %s\n", temp->toString().c_str());
                }
            }
        }
        fprintf(stderr, "****** rawv %s\n", charValue.toString().c_str());
    }
};

// #define SHOW_STATIC_SERVICE_CHARACTERISTIC_COMPOSITION 1

int main(int argc, char *argv[])
{
    bool ok = true, foundDevice=false;
    int dev_id = 0; // default
    bool waitForEnter=false;
    BDAddressAndType waitForDevice = BDAddressAndType::ANY_DEVICE;
    bool forever = false;

    /**
     * BT Core Spec v5.2:  Vol 3, Part A L2CAP Spec: 7.9 PRIORITIZING DATA OVER HCI
     *
     * In order for guaranteed channels to meet their guarantees,
     * L2CAP should prioritize traffic over the HCI transport in devices that support HCI.
     * Packets for Guaranteed channels should receive higher priority than packets for Best Effort channels.
     * ...
     * I have noticed that w/o HCI le_connect, overall communication takes twice as long!!!
     */
    bool doHCI_Connect = true;

    for(int i=1; i<argc; i++) {
        if( !strcmp("-wait", argv[i]) ) {
            waitForEnter = true;
        } else if( !strcmp("-forever", argv[i]) ) {
            forever = true;
        } else if( !strcmp("-dev_id", argv[i]) && argc > (i+1) ) {
            dev_id = atoi(argv[++i]);
        } else if( !strcmp("-skipConnect", argv[i]) ) {
            doHCI_Connect = false;
        } else if( !strcmp("-mac", argv[i]) && argc > (i+1) ) {
            std::string macstr = std::string(argv[++i]);
            waitForDevice = BDAddressAndType(EUI48(macstr), BDAddressType::BDADDR_UNDEFINED);
        }
    }
    fprintf(stderr, "dev_id %d\n", dev_id);
    fprintf(stderr, "doHCI_Connect %d\n", doHCI_Connect);
    fprintf(stderr, "waitForDevice: %s\n", waitForDevice.toString().c_str());

    if( waitForEnter ) {
        fprintf(stderr, "Press ENTER to continue\n");
        getchar();
    }

    DBTAdapter adapter(dev_id);
    if( !adapter.hasDevId() ) {
        fprintf(stderr, "Default adapter not available.\n");
        exit(1);
    }
    if( !adapter.isValid() ) {
        fprintf(stderr, "Adapter invalid.\n");
        exit(1);
    }
    if( !adapter.isPowered() ) {
        fprintf(stderr, "Adapter not powered: device %s, address %s: %s\n",
                adapter.getName().c_str(), adapter.getAddressString().c_str(), adapter.toString().c_str());
        exit(1);
    }
    fprintf(stderr, "Using adapter: device %s, address %s: %s\n",
        adapter.getName().c_str(), adapter.getAddressString().c_str(), adapter.toString().c_str());

    adapter.addStatusListener(std::shared_ptr<AdapterStatusListener>(new MyAdapterStatusListener()));

    const uint64_t t0 = getCurrentMilliseconds();

    while( ok && ( forever || !foundDevice ) ) {
        ok = HCIStatusCode::SUCCESS == adapter.startDiscovery(true /* keepAlive */);
        if( !ok) {
            perror("Adapter start discovery failed");
            goto out;
        }

        std::shared_ptr<DBTDevice> device = nullptr;
        {
            std::unique_lock<std::mutex> lockRead(mtxDeviceFound); // RAII-style acquire and relinquish via destructor
            while( nullptr == device ) { // FIXME deadlock, waiting forever!
                cvDeviceFound.wait(lockRead);
                if( nullptr != deviceFound ) {
                    foundDevice = deviceFound->getAddressAndType().matches(waitForDevice); // match
                    if( foundDevice || ( BDAddressAndType::ANY_DEVICE == waitForDevice && deviceFound->getAddressAndType().isLEAddress() ) ) {
                        // match or any LE device
                        device.swap(deviceFound); // take over deviceFound
                    }
                }
            }
        }
        adapter.stopDiscovery();

        if( ok && nullptr != device ) {
            const uint64_t t1 = getCurrentMilliseconds();

            //
            // HCI LE-Connect
            // (Without: Overall communication takes ~twice as long!!!)
            //
            if( doHCI_Connect ) {
                HCIStatusCode res;
                if( ( res = device->connectDefault() ) != HCIStatusCode::SUCCESS ) {
                    fprintf(stderr, "Connect: Failed res %s, %s\n", getHCIStatusCodeString(res).c_str(), device->toString().c_str());
                    // we tolerate the failed immediate connect, as it might happen at a later time
                } else {
                    fprintf(stderr, "Connect: Success\n");
                }
            } else {
                fprintf(stderr, "Connect: Skipped %s\n", device->toString().c_str());
            }
            const uint64_t t3 = getCurrentMilliseconds();
            const uint64_t td03 = t3 - t0;
            const uint64_t td13 = t3 - t1;
            const uint64_t td01 = t1 - t0;
            fprintf(stderr, "  discovery-only %" PRIu64 " ms,\n"
                            "  connect-only %" PRIu64 " ms,\n"
                            "  discovered to hci-connected %" PRIu64 " ms,\n"
                            "  total %" PRIu64 " ms,\n",
                            td01, td13, (t3 - device->getCreationTimestamp()), td03);

            //
            // GATT Processing
            //
            const uint64_t t4 = getCurrentMilliseconds();
            // let's check further for full GATT
            std::shared_ptr<GATTHandler> gatt = device->getGATTHandler();
            if( nullptr != gatt ) {
                fprintf(stderr, "GATT usedMTU %d (server) -> %d (used)\n", gatt->getServerMTU(), gatt->getUsedMTU());

                gatt->addCharacteristicListener( std::shared_ptr<GATTCharacteristicListener>( new MyGATTEventListener() ) );

                jau::darray<GATTServiceRef> & primServices = gatt->discoverCompletePrimaryServices(gatt);
                const uint64_t t5 = getCurrentMilliseconds();
                {
                    const uint64_t td45 = t5 - t4; // connect -> gatt complete
                    const uint64_t td05 = t5 - t0; // total
                    fprintf(stderr, "\n\n\n");
                    fprintf(stderr, "GATT primary-services completed\n");
                    fprintf(stderr, "  gatt connect -> gatt complete %" PRIu64 " ms,\n"
                                    "  discovered to gatt complete %" PRIu64 " ms,\n"
                                    "  total %" PRIu64 " ms\n\n",
                                    td45, (t5 - device->getCreationTimestamp()), td05);
                }
                std::shared_ptr<GattGenericAccessSvc> ga = device->getGATTGenericAccess();
                if( nullptr != ga ) {
                    fprintf(stderr, "  GenericAccess: %s\n\n", ga->toString().c_str());
                }
                if( nullptr != gatt && gatt->isConnected() ) {
                    std::shared_ptr<GattDeviceInformationSvc> di = gatt->getDeviceInformation(primServices);
                    if( nullptr != di ) {
                        fprintf(stderr, "  DeviceInformation: %s\n\n", di->toString().c_str());
                    }
                }

                for(size_t i=0; i<primServices.size() && gatt->isConnected(); i++) {
                    GATTService & primService = *primServices.at(i);
                    fprintf(stderr, "  [%2.2d] Service %s\n", (int)i, primService.toString().c_str());
                    fprintf(stderr, "  [%2.2d] Service Characteristics\n", (int)i);
                    jau::darray<GATTCharacteristicRef> & serviceCharacteristics = primService.characteristicList;
                    for(size_t j=0; j<serviceCharacteristics.size() && gatt->isConnected(); j++) {
                        GATTCharacteristic & serviceChar = *serviceCharacteristics.at(j);
                        fprintf(stderr, "  [%2.2d.%2.2d] Decla: %s\n", (int)i, (int)j, serviceChar.toString().c_str());
                        if( serviceChar.hasProperties(GATTCharacteristic::PropertyBitVal::Read) ) {
                            POctets value(GATTHandler::number(GATTHandler::Defaults::MAX_ATT_MTU), 0);
                            if( gatt->readCharacteristicValue(serviceChar, value) ) {
                                fprintf(stderr, "  [%2.2d.%2.2d] Value: %s\n", (int)i, (int)j, value.toString().c_str());
                            }
                        }
                        GATTDescriptorRef cccd = serviceChar.getClientCharacteristicConfig();
                        if( nullptr != cccd ) {
                            const bool enableNotification = serviceChar.hasProperties(GATTCharacteristic::PropertyBitVal::Notify);
                            const bool enableIndication = serviceChar.hasProperties(GATTCharacteristic::PropertyBitVal::Indicate);
                            if( enableNotification || enableIndication ) {
                                bool res = gatt->configNotificationIndication(*cccd, enableNotification, enableIndication);
                                fprintf(stderr, "  [%2.2d.%2.2d] Config Notification(%d), Indication(%d): Result %d\n",
                                        (int)i, (int)j, enableNotification, enableIndication, res);
                            }
                        }
                    }
                }
                // FIXME sleep 1s for potential callbacks ..
                sleep(1);
            } else {
                fprintf(stderr, "GATT connect failed: %s\n", gatt->getStateString().c_str());
            }
            device->disconnect(); // OK if not connected, also issues device->disconnectGATT() -> gatt->disconnect()
            device->remove(); // implicit disconnect as well
        } // if( ok && nullptr != device )
    }

#ifdef SHOW_STATIC_SERVICE_CHARACTERISTIC_COMPOSITION
    //
    // Show static composition of Services and Characteristics
    //
    for(size_t i=0; i<GATT_SERVICES.size(); i++) {
        const GattServiceCharacteristic * gsc = GATT_SERVICES.at(i);
        fprintf(stderr, "GattServiceCharacteristic %d: %s\n", (int)i, gsc->toString().c_str());
    }
#endif /* SHOW_STATIC_SERVICE_CHARACTERISTIC_COMPOSITION */

out:
    return 0;
}

