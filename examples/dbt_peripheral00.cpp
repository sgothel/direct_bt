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
#include <direct_bt/GattNumbers.hpp>
#include <direct_bt/DBGattServer.hpp>

extern "C" {
    #include <unistd.h>
}

#include "dbt_constants.hpp"

using namespace direct_bt;
using namespace jau;

/** \file
 * This _dbt_peripheral00__ C++ peripheral ::BTRole::Slave example uses the Direct-BT fully event driven workflow.
 */

static uint64_t timestamp_t0;

static EUI48 useAdapter = EUI48::ALL_DEVICE;
static BTMode btMode = BTMode::DUAL;
static bool use_SC = true;
static std::string adapter_name = "TestDev001_N";
static std::string adapter_short_name = "TDev001N";
static std::shared_ptr<BTAdapter> chosenAdapter = nullptr;

static bool SHOW_UPDATE_EVENTS = false;

static bool startAdvertising(BTAdapter *a, std::string msg);
static bool stopAdvertising(BTAdapter *a, std::string msg);
static void processConnectedDevice(std::shared_ptr<BTDevice> device);
static void processReadyDevice(std::shared_ptr<BTDevice> device);
static void processDisconnectedDevice(std::shared_ptr<BTDevice> device);

static jau::POctets make_poctets(const char* name) {
    return jau::POctets( (const uint8_t*)name, (nsize_t)strlen(name), endian::little );
}
static jau::POctets make_poctets(const uint16_t v) {
    jau::POctets p(2, endian::little);
    p.put_uint16_nc(0, v);
    return p;
}
static jau::POctets make_poctets(const jau::nsize_t capacity, const jau::nsize_t size) {
    return jau::POctets(capacity, size, endian::little);
}

// static const jau::uuid128_t DataServiceUUID =  jau::uuid128_t("d0ca6bf3-3d50-4760-98e5-fc5883e93712");
static const jau::uuid128_t CommandUUID      = jau::uuid128_t("d0ca6bf3-3d52-4760-98e5-fc5883e93712");
static const jau::uuid128_t ResponseUUID     = jau::uuid128_t("d0ca6bf3-3d53-4760-98e5-fc5883e93712");
static const jau::uuid128_t PulseDataUUID =    jau::uuid128_t("d0ca6bf3-3d54-4760-98e5-fc5883e93712");

// DBGattServerRef dbGattServer = std::make_shared<DBGattServer>(
DBGattServerRef dbGattServer( new DBGattServer(
        /* services: */
        jau::make_darray( // DBGattService
          DBGattService ( true /* primary */,
              std::make_unique<const jau::uuid16_t>(GattServiceType::GENERIC_ACCESS) /* type_ */,
              jau::make_darray ( // DBGattChar
                  DBGattChar( std::make_unique<const jau::uuid16_t>(GattCharacteristicType::DEVICE_NAME) /* value_type_ */,
                              BTGattChar::PropertyBitVal::Read,
                              /* descriptors: */ { /* intentionally w/o Desc */ },
                              make_poctets("Synthethic Sensor 01") /* value */ ),
                  DBGattChar( std::make_unique<const jau::uuid16_t>(GattCharacteristicType::APPEARANCE) /* value_type_ */,
                              BTGattChar::PropertyBitVal::Read,
                              /* descriptors: */ { /* intentionally w/o Desc */ },
                              make_poctets((uint16_t)0) /* value */
              ) ) ),
          DBGattService ( true /* primary */,
              std::make_unique<const jau::uuid16_t>(GattServiceType::DEVICE_INFORMATION) /* type_ */,
              jau::make_darray ( // DBGattChar
                  DBGattChar( std::make_unique<const jau::uuid16_t>(GattCharacteristicType::MANUFACTURER_NAME_STRING) /* value_type_ */,
                              BTGattChar::PropertyBitVal::Read,
                              /* descriptors: */ { /* intentionally w/o Desc */ },
                              make_poctets("Gothel Software") /* value */ ),
                  DBGattChar( std::make_unique<const jau::uuid16_t>(GattCharacteristicType::MODEL_NUMBER_STRING) /* value_type_ */,
                              BTGattChar::PropertyBitVal::Read,
                              /* descriptors: */ { /* intentionally w/o Desc */ },
                              make_poctets("2.4.0-pre") /* value */ ),
                  DBGattChar( std::make_unique<const jau::uuid16_t>(GattCharacteristicType::SERIAL_NUMBER_STRING) /* value_type_ */,
                              BTGattChar::PropertyBitVal::Read,
                              /* descriptors: */ { /* intentionally w/o Desc */ },
                              make_poctets("sn:0123456789") /* value */ ),
                  DBGattChar( std::make_unique<const jau::uuid16_t>(GattCharacteristicType::HARDWARE_REVISION_STRING) /* value_type_ */,
                              BTGattChar::PropertyBitVal::Read,
                              /* descriptors: */ { /* intentionally w/o Desc */ },
                              make_poctets("hw:0123456789") /* value */ ),
                  DBGattChar( std::make_unique<const jau::uuid16_t>(GattCharacteristicType::FIRMWARE_REVISION_STRING) /* value_type_ */,
                              BTGattChar::PropertyBitVal::Read,
                              /* descriptors: */ { /* intentionally w/o Desc */ },
                              make_poctets("fw:0123456789") /* value */ ),
                  DBGattChar( std::make_unique<const jau::uuid16_t>(GattCharacteristicType::SOFTWARE_REVISION_STRING) /* value_type_ */,
                              BTGattChar::PropertyBitVal::Read,
                              /* descriptors: */ { /* intentionally w/o Desc */ },
                              make_poctets("sw:0123456789") /* value */
              ) ) ),
          DBGattService ( true /* primary */,
              std::make_unique<const jau::uuid128_t>("d0ca6bf3-3d50-4760-98e5-fc5883e93712") /* type_ */,
              jau::make_darray ( // DBGattChar
                  DBGattChar( std::make_unique<const jau::uuid128_t>("d0ca6bf3-3d51-4760-98e5-fc5883e93712") /* value_type_ */,
                              BTGattChar::PropertyBitVal::Read,
                              jau::make_darray ( // DBGattDesc
                                  DBGattDesc( BTGattDesc::TYPE_USER_DESC, make_poctets("DATA STATIC") )
                              ),
                            make_poctets("Proprietary Static Data 0x00010203") /* value */ ),
                  DBGattChar( std::make_unique<const jau::uuid128_t>(CommandUUID) /* value_type_ */,
                              BTGattChar::PropertyBitVal::WriteNoAck | BTGattChar::PropertyBitVal::WriteWithAck,
                              jau::make_darray ( // DBGattDesc
                                  DBGattDesc( BTGattDesc::TYPE_USER_DESC, make_poctets("COMMAND") )
                              ),
                              make_poctets(128, 64) /* value */ ),
                  DBGattChar( std::make_unique<const jau::uuid128_t>(ResponseUUID) /* value_type_ */,
                              BTGattChar::PropertyBitVal::Notify | BTGattChar::PropertyBitVal::Indicate,
                              jau::make_darray ( // DBGattDesc
                                  DBGattDesc( BTGattDesc::TYPE_USER_DESC, make_poctets("RESPONSE") ),
                                  DBGattDesc::createClientCharConfig()
                              ),
                              make_poctets((uint16_t)0) /* value */ ),
                  DBGattChar( std::make_unique<const jau::uuid128_t>(PulseDataUUID) /* value_type_ */,
                              BTGattChar::PropertyBitVal::Notify | BTGattChar::PropertyBitVal::Indicate,
                              jau::make_darray ( // DBGattDesc
                                  DBGattDesc( BTGattDesc::TYPE_USER_DESC, make_poctets("DATA PULSE") ),
                                  DBGattDesc::createClientCharConfig()
                              ),
                              make_poctets("Synthethic Sensor 01") /* value */
              ) ) )
        ) ) );


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
            std::thread sd(::startAdvertising, &a, "powered-on"); // @suppress("Invalid arguments")
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

        fprintf_td(stderr, "****** FOUND__-1: NOP %s\n", device->toString(true).c_str());
        return false;
    }

    void deviceUpdated(std::shared_ptr<BTDevice> device, const EIRDataType updateMask, const uint64_t timestamp) override {
        if( SHOW_UPDATE_EVENTS ) {
            fprintf_td(stderr, "****** UPDATED: %s of %s\n", to_string(updateMask).c_str(), device->toString(true).c_str());
        }
        (void)timestamp;
    }

    void deviceConnected(std::shared_ptr<BTDevice> device, const uint16_t handle, const uint64_t timestamp) override {
        fprintf_td(stderr, "****** CONNECTED: %s\n", device->toString(true).c_str());

        std::thread sd(::processConnectedDevice, device); // @suppress("Invalid arguments")
        sd.detach();

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
              BTDeviceRegistry::isWaitingForDevice(device->getAddressAndType().address, device->getName())
            )
          )
        {
            fprintf_td(stderr, "****** READY-0: Processing %s\n", device->toString(true).c_str());
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

        std::thread sd(::processDisconnectedDevice, device); // @suppress("Invalid arguments")
        sd.detach();
        (void)timestamp;
    }

    std::string toString() const override {
        return "MyAdapterStatusListener[this "+to_hexstring(this)+"]";
    }

};

class MyGATTServerListener : public DBGattServer::Listener {
    private:
        mutable jau::sc_atomic_bool sync_data;
        std::thread pulseSenderThread;
        jau::sc_atomic_bool stopPulseSender = false;

        jau::sc_atomic_uint16 handlePulseDataNotify = 0;
        jau::sc_atomic_uint16 handlePulseDataIndicate = 0;
        jau::sc_atomic_uint16 handleResponseDataNotify = 0;
        jau::sc_atomic_uint16 handleResponseDataIndicate = 0;

        std::shared_ptr<BTDevice> sendToDevice;

        void pulseSender() {
            jau::sc_atomic_critical sync(sync_data);
            while( !stopPulseSender ) {
                if( nullptr != sendToDevice && sendToDevice->getConnected() ) {
                    if( 0 != handlePulseDataNotify || 0 != handlePulseDataIndicate ) {
                        std::string data( "Dynamic Data Example. Elapsed Milliseconds: "+jau::to_decstring(environment::getElapsedMillisecond(), ',', 9) );
                        jau::POctets v(data.size()+1, jau::endian::little);
                        v.put_string_nc(0, data, v.size(), true /* includeEOS */);
                        if( 0 != handlePulseDataNotify ) {
                            sendToDevice->sendNotification(handlePulseDataNotify, v);
                        }
                        if( 0 != handlePulseDataIndicate ) {
                            sendToDevice->sendIndication(handlePulseDataNotify, v);
                        }
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }

        void sendResponse(jau::POctets data) {
            if( nullptr != sendToDevice && sendToDevice->getConnected() ) {
                if( 0 != handleResponseDataNotify || 0 != handleResponseDataIndicate ) {
                    if( 0 != handleResponseDataNotify ) {
                        sendToDevice->sendNotification(handleResponseDataNotify, data);
                    }
                    if( 0 != handleResponseDataIndicate ) {
                        sendToDevice->sendIndication(handleResponseDataNotify, data);
                    }
                }
            }
        }

    public:
        MyGATTServerListener()
        : pulseSenderThread(&MyGATTServerListener::pulseSender, this)
        { }

        ~MyGATTServerListener() {
            {
                jau::sc_atomic_critical sync(sync_data);
                stopPulseSender = true;
                sendToDevice = nullptr;
            }
            if( pulseSenderThread.joinable() ) {
                pulseSenderThread.join();
            }
        }

        bool readCharValue(std::shared_ptr<BTDevice> device, DBGattService& s, DBGattChar& c) override {
            fprintf_td(stderr, "GATT::readCharValue: to %s, from\n  %s\n    %s\n",
                    device->toString().c_str(), s.toString().c_str(), c.toString().c_str());
            return true;
        }

        bool readDescValue(std::shared_ptr<BTDevice> device, DBGattService& s, DBGattChar& c, DBGattDesc& d) override {
            fprintf_td(stderr, "GATT::readDescValue: to %s, from\n  %s\n    %s\n      %s\n",
                    device->toString().c_str(), s.toString().c_str(), c.toString().c_str(), d.toString().c_str());
            return true;
        }

        bool writeCharValue(std::shared_ptr<BTDevice> device, DBGattService& s, DBGattChar& c, const jau::TROOctets & value, const uint16_t value_offset) override {
            fprintf_td(stderr, "GATT::writeCharValue: %s @ %s from %s, to\n  %s\n    %s\n",
                    value.toString().c_str(), jau::to_hexstring(value_offset).c_str(),
                    device->toString().c_str(), s.toString().c_str(), c.toString().c_str());

            if( c.value_type->equivalent( CommandUUID ) &&
                ( 0 != handleResponseDataNotify || 0 != handleResponseDataIndicate ) )
            {
                jau::POctets value2(value);
                std::thread senderThread(&MyGATTServerListener::sendResponse, this, value2); // @suppress("Invalid arguments")
                senderThread.detach();
            }
            return true;
        }

        bool writeDescValue(std::shared_ptr<BTDevice> device, DBGattService& s, DBGattChar& c, DBGattDesc& d, const jau::TROOctets & value, const uint16_t value_offset) override {
            fprintf_td(stderr, "GATT::writeDescValue: %s @ %s from %s\n  %s\n    %s\n      %s\n",
                    value.toString().c_str(), jau::to_hexstring(value_offset).c_str(),
                    device->toString().c_str(), s.toString().c_str(), c.toString().c_str(), d.toString().c_str());
            return true;
        }

        void clientCharConfigChanged(std::shared_ptr<BTDevice> device, DBGattService& s, DBGattChar& c, DBGattDesc& d, const bool notificationEnabled, const bool indicationEnabled) override {
            fprintf_td(stderr, "GATT::clientCharConfigChanged: notify %d, indicate %d from %s\n  %s\n    %s\n      %s\n",
                    notificationEnabled, indicationEnabled,
                    device->toString().c_str(), s.toString().c_str(), c.toString().c_str(), d.toString().c_str());

            {
                jau::sc_atomic_critical sync(sync_data);
                sendToDevice = device;
                if( c.value_type->equivalent( PulseDataUUID ) ) {
                    handlePulseDataNotify = notificationEnabled ? c.value_handle : 0;
                    handlePulseDataIndicate = indicationEnabled ? c.value_handle : 0;
                } else if( c.value_type->equivalent( ResponseUUID ) ) {
                    handleResponseDataNotify = notificationEnabled ? c.value_handle : 0;
                    handleResponseDataIndicate = indicationEnabled ? c.value_handle : 0;
                }
            }
        }
};

static const uint16_t adv_interval_min=0x0800;
static const uint16_t adv_interval_max=0x0800;
static const AD_PDU_Type adv_type=AD_PDU_Type::ADV_IND;
static const uint8_t adv_chan_map=0x07;
static const uint8_t filter_policy=0x00;

static bool startAdvertising(BTAdapter *a, std::string msg) {
    if( useAdapter != EUI48::ALL_DEVICE && useAdapter != a->getAddressAndType().address ) {
        fprintf_td(stderr, "****** Start advertising (%s): Adapter not selected: %s\n", msg.c_str(), a->toString().c_str());
        return false;
    }
    HCIStatusCode status = a->startAdvertising(dbGattServer,
                                               adv_interval_min, adv_interval_max,
                                               adv_type, adv_chan_map, filter_policy);
    fprintf_td(stderr, "****** Start advertising (%s) result: %s: %s\n", msg.c_str(), to_string(status).c_str(), a->toString().c_str());
    fprintf_td(stderr, "%s", dbGattServer->toFullString().c_str());
    return HCIStatusCode::SUCCESS == status;
}

static bool stopAdvertising(BTAdapter *a, std::string msg) {
    if( useAdapter != EUI48::ALL_DEVICE && useAdapter != a->getAddressAndType().address ) {
        fprintf_td(stderr, "****** Stop advertising (%s): Adapter not selected: %s\n", msg.c_str(), a->toString().c_str());
        return false;
    }
    HCIStatusCode status = a->stopAdvertising();
    fprintf_td(stderr, "****** Stop advertising (%s) result: %s: %s\n", msg.c_str(), to_string(status).c_str(), a->toString().c_str());
    return HCIStatusCode::SUCCESS == status;
}

static void processConnectedDevice(std::shared_ptr<BTDevice> device) {
    fprintf_td(stderr, "****** Connected Device: Start %s\n", device->toString().c_str());

    // Already Connected - Can't Do!
    // HCIStatusCode res = SMPKeyBin::readAndApply(KEY_PATH, *device, BTSecurityLevel::UNSET, true /* verbose */);
    // fprintf_td(stderr, "****** CONNECTED: SMPKeyBin::readAndApply(..) result %s\n", to_string(res).c_str());

    fprintf_td(stderr, "****** Connected Device: End %s\n", device->toString().c_str());
}

static void processDisconnectedDevice(std::shared_ptr<BTDevice> device) {
    fprintf_td(stderr, "****** Disconnected Device: Start %s\n", device->toString().c_str());

    stopAdvertising(&device->getAdapter(), "device-disconnected");
    {
        const HCIStatusCode r = device->unpair();
        fprintf_td(stderr, "****** Disconnect Device: Unpair-Pre result: %s\n", to_string(r).c_str());
    }
    BTDeviceRegistry::removeFromProcessingDevices(device->getAddressAndType());
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    startAdvertising(&device->getAdapter(), "device-disconnected");

    fprintf_td(stderr, "****** Disonnected Device: End %s\n", device->toString().c_str());
}

static void processReadyDevice(std::shared_ptr<BTDevice> device) {
    fprintf_td(stderr, "****** Processing Ready Device: Start %s\n", device->toString().c_str());

    SMPKeyBin::createAndWrite(*device, KEY_PATH, false /* overwrite */, true /* verbose */);

    fprintf_td(stderr, "****** Processing Ready Device: End %s\n", device->toString().c_str());
}

static bool initAdapter(std::shared_ptr<BTAdapter>& adapter) {
    if( useAdapter != EUI48::ALL_DEVICE && useAdapter != adapter->getAddressAndType().address ) {
        fprintf_td(stderr, "initAdapter: Adapter not selected: %s\n", adapter->toString().c_str());
        return false;
    }
    if( !adapter->isInitialized() ) {
        // setName(..) ..
        if( adapter->setPowered(false) ) {
            if( adapter->setSecureConnections( use_SC ) ) {
                fprintf_td(stderr, "initAdapter: setSecureConnections OK: %s\n", adapter->toString().c_str());
            } else {
                fprintf_td(stderr, "initAdapter: setSecureConnections failed: %s\n", adapter->toString().c_str());
            }
            const HCIStatusCode status = adapter->setName(adapter_name, adapter_short_name);
            if( HCIStatusCode::SUCCESS == status ) {
                fprintf_td(stderr, "initAdapter: setLocalName OK: %s\n", adapter->toString().c_str());
            } else {
                fprintf_td(stderr, "initAdapter: setLocalName failed: %s\n", adapter->toString().c_str());
            }
        } else {
            fprintf_td(stderr, "initAdapter: setPowered failed: %s\n", adapter->toString().c_str());
        }
        // Initialize with defaults and power-on
        const HCIStatusCode status = adapter->initialize( btMode );
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

    std::shared_ptr<AdapterStatusListener> asl( std::make_shared<MyAdapterStatusListener>() );
    adapter->addStatusListener( asl );
    // Flush discovered devices after registering our status listener.
    // This avoids discovered devices before we have registered!
    adapter->removeDiscoveredDevices();

    if( !startAdvertising(adapter.get(), "initAdapter") ) {
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
            fprintf_td(stderr, "****** Adapter Features: %s\n", direct_bt::to_string(adapter->getLEFeatures()).c_str());
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

    dbGattServer->addListener( std::make_shared<MyGATTServerListener>() );

    BTManager & mngr = BTManager::get();
    mngr.addChangedAdapterSetCallback(myChangedAdapterSetFunc);

    while( !done ) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }
    chosenAdapter = nullptr;
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
        } else if( !strcmp("-show_update_events", argv[i]) ) {
            SHOW_UPDATE_EVENTS = true;
        } else if( !strcmp("-btmode", argv[i]) && argc > (i+1) ) {
            btMode = to_BTMode(argv[++i]);
        } else if( !strcmp("-use_sc", argv[i]) && argc > (i+1) ) {
            use_SC = 0 != atoi(argv[++i]);
        } else if( !strcmp("-adapter", argv[i]) && argc > (i+1) ) {
            useAdapter = EUI48( std::string(argv[++i]) );
        } else if( !strcmp("-name", argv[i]) && argc > (i+1) ) {
            adapter_name = std::string(argv[++i]);
        } else if( !strcmp("-short_name", argv[i]) && argc > (i+1) ) {
            adapter_short_name = std::string(argv[++i]);
        } else if( !strcmp("-mtu", argv[i]) && argc > (i+1) ) {
            dbGattServer->att_mtu = atoi(argv[++i]);
        }
    }
    fprintf(stderr, "pid %d\n", getpid());

    fprintf(stderr, "Run with '[-btmode LE|BREDR|DUAL] [-use_sc 0|1] "
                    "[-adapter <adapter_address>] "
                    "[-name <adapter_name>] "
                    "[-short_name <adapter_short_name>] "
                    "[-mtu <max att_mtu>"
                    "[-dbt_verbose true|false] "
                    "[-dbt_debug true|false|adapter.event,gatt.data,hci.event,hci.scan_ad_eir,mgmt.event] "
                    "[-dbt_mgmt cmd.timeout=3000,ringsize=64,...] "
                    "[-dbt_hci cmd.complete.timeout=10000,cmd.status.timeout=3000,ringsize=64,...] "
                    "[-dbt_gatt cmd.read.timeout=500,cmd.write.timeout=500,cmd.init.timeout=2500,ringsize=128,...] "
                    "[-dbt_l2cap reader.timeout=10000,restart.count=0,...] "
                    "\n");

    fprintf(stderr, "SHOW_UPDATE_EVENTS %d\n", SHOW_UPDATE_EVENTS);
    fprintf(stderr, "adapter %s\n", useAdapter.toString().c_str());
    fprintf(stderr, "btmode %s\n", to_string(btMode).c_str());
    fprintf(stderr, "name %s (short %s)\n", adapter_name.c_str(), adapter_short_name.c_str());
    fprintf(stderr, "GattServer %s\n", dbGattServer->toString().c_str());
    fprintf(stderr, "GattServer.services: %s\n", dbGattServer->services.get_info().c_str());
    fprintf(stderr, "GattService.characteristics: %s\n", dbGattServer->services[0].characteristics.get_info().c_str());

    if( waitForEnter ) {
        fprintf(stderr, "Press ENTER to continue\n");
        getchar();
    }
    fprintf(stderr, "****** TEST start\n");
    test();
    fprintf(stderr, "****** TEST end\n");
}
