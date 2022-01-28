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
static BTSecurityLevel adapter_sec_level = BTSecurityLevel::UNSET;
static bool SHOW_UPDATE_EVENTS = false;
static bool RUN_ONLY_ONCE = false;
static jau::relaxed_atomic_nsize_t servedConnections = 0;

static bool startAdvertising(BTAdapter *a, std::string msg);
static bool stopAdvertising(BTAdapter *a, std::string msg);
static void processReadyDevice(BTDeviceRef device);
static void processDisconnectedDevice(BTDeviceRef device);

static jau::POctets make_poctets(const char* name) {
    return jau::POctets( (const uint8_t*)name, (nsize_t)strlen(name), endian::little );
}
static jau::POctets make_poctets(const char* name, const jau::nsize_t capacity) {
    const nsize_t name_len = (nsize_t)strlen(name);
    jau::POctets p( std::max<nsize_t>(capacity, name_len), name_len, endian::little );
    p.put_bytes_nc(0, reinterpret_cast<const uint8_t*>(name), name_len);
    return p;
}
static jau::POctets make_poctets(const uint16_t v) {
    jau::POctets p(2, endian::little);
    p.put_uint16_nc(0, v);
    return p;
}
static jau::POctets make_poctets(const jau::nsize_t capacity, const jau::nsize_t size) {
    return jau::POctets(capacity, size, endian::little);
}

static const jau::uuid128_t DataServiceUUID = jau::uuid128_t("d0ca6bf3-3d50-4760-98e5-fc5883e93712");
static const jau::uuid128_t StaticDataUUID  = jau::uuid128_t("d0ca6bf3-3d51-4760-98e5-fc5883e93712");
static const jau::uuid128_t CommandUUID     = jau::uuid128_t("d0ca6bf3-3d52-4760-98e5-fc5883e93712");
static const jau::uuid128_t ResponseUUID    = jau::uuid128_t("d0ca6bf3-3d53-4760-98e5-fc5883e93712");
static const jau::uuid128_t PulseDataUUID   = jau::uuid128_t("d0ca6bf3-3d54-4760-98e5-fc5883e93712");

// DBGattServerRef dbGattServer = std::make_shared<DBGattServer>(
DBGattServerRef dbGattServer( new DBGattServer(
        /* services: */
        jau::make_darray( // DBGattService
          std::make_shared<DBGattService> ( true /* primary */,
              std::make_unique<const jau::uuid16_t>(GattServiceType::GENERIC_ACCESS) /* type_ */,
              jau::make_darray ( // DBGattChar
                  std::make_shared<DBGattChar>( std::make_unique<const jau::uuid16_t>(GattCharacteristicType::DEVICE_NAME) /* value_type_ */,
                              BTGattChar::PropertyBitVal::Read,
                              jau::darray<DBGattDescRef>() /* intentionally empty */,
                              make_poctets(adapter_name.c_str(), 128) /* value */, true /* variable_length */ ),
                  std::make_shared<DBGattChar>( std::make_unique<const jau::uuid16_t>(GattCharacteristicType::APPEARANCE) /* value_type_ */,
                              BTGattChar::PropertyBitVal::Read,
                              jau::darray<DBGattDescRef>() /* intentionally empty */,
                              make_poctets((uint16_t)0) /* value */ )
              ) ),
          std::make_shared<DBGattService> ( true /* primary */,
              std::make_unique<const jau::uuid16_t>(GattServiceType::DEVICE_INFORMATION) /* type_ */,
              jau::make_darray ( // DBGattChar
                  std::make_shared<DBGattChar>( std::make_unique<const jau::uuid16_t>(GattCharacteristicType::MANUFACTURER_NAME_STRING) /* value_type_ */,
                              BTGattChar::PropertyBitVal::Read,
                              jau::darray<DBGattDescRef>() /* intentionally empty */,
                              make_poctets("Gothel Software") /* value */ ),
                  std::make_shared<DBGattChar>( std::make_unique<const jau::uuid16_t>(GattCharacteristicType::MODEL_NUMBER_STRING) /* value_type_ */,
                              BTGattChar::PropertyBitVal::Read,
                              jau::darray<DBGattDescRef>() /* intentionally empty */,
                              make_poctets("2.4.0-pre") /* value */ ),
                  std::make_shared<DBGattChar>( std::make_unique<const jau::uuid16_t>(GattCharacteristicType::SERIAL_NUMBER_STRING) /* value_type_ */,
                              BTGattChar::PropertyBitVal::Read,
                              jau::darray<DBGattDescRef>() /* intentionally empty */,
                              make_poctets("sn:0123456789") /* value */ ),
                  std::make_shared<DBGattChar>( std::make_unique<const jau::uuid16_t>(GattCharacteristicType::HARDWARE_REVISION_STRING) /* value_type_ */,
                              BTGattChar::PropertyBitVal::Read,
                              jau::darray<DBGattDescRef>() /* intentionally empty */,
                              make_poctets("hw:0123456789") /* value */ ),
                  std::make_shared<DBGattChar>( std::make_unique<const jau::uuid16_t>(GattCharacteristicType::FIRMWARE_REVISION_STRING) /* value_type_ */,
                              BTGattChar::PropertyBitVal::Read,
                              jau::darray<DBGattDescRef>() /* intentionally empty */,
                              make_poctets("fw:0123456789") /* value */ ),
                  std::make_shared<DBGattChar>( std::make_unique<const jau::uuid16_t>(GattCharacteristicType::SOFTWARE_REVISION_STRING) /* value_type_ */,
                              BTGattChar::PropertyBitVal::Read,
                              jau::darray<DBGattDescRef>() /* intentionally empty */,
                              make_poctets("sw:0123456789") /* value */ )
              ) ),
          std::make_shared<DBGattService> ( true /* primary */,
              std::make_unique<const jau::uuid128_t>(DataServiceUUID) /* type_ */,
              jau::make_darray ( // DBGattChar
                  std::make_shared<DBGattChar>( std::make_unique<const jau::uuid128_t>(StaticDataUUID) /* value_type_ */,
                              BTGattChar::PropertyBitVal::Read,
                              jau::make_darray ( // DBGattDesc
                                  std::make_shared<DBGattDesc>( BTGattDesc::TYPE_USER_DESC, make_poctets("DATA_STATIC") )
                              ),
                            make_poctets("Proprietary Static Data 0x00010203") /* value */ ),
                  std::make_shared<DBGattChar>( std::make_unique<const jau::uuid128_t>(CommandUUID) /* value_type_ */,
                              BTGattChar::PropertyBitVal::WriteNoAck | BTGattChar::PropertyBitVal::WriteWithAck,
                              jau::make_darray ( // DBGattDesc
                                  std::make_shared<DBGattDesc>( BTGattDesc::TYPE_USER_DESC, make_poctets("COMMAND") )
                              ),
                              make_poctets(128, 64) /* value */, true /* variable_length */ ),
                  std::make_shared<DBGattChar>( std::make_unique<const jau::uuid128_t>(ResponseUUID) /* value_type_ */,
                              BTGattChar::PropertyBitVal::Notify | BTGattChar::PropertyBitVal::Indicate,
                              jau::make_darray ( // DBGattDesc
                                  std::make_shared<DBGattDesc>( BTGattDesc::TYPE_USER_DESC, make_poctets("RESPONSE") ),
                                  DBGattDesc::createClientCharConfig()
                              ),
                              make_poctets((uint16_t)0) /* value */ ),
                  std::make_shared<DBGattChar>( std::make_unique<const jau::uuid128_t>(PulseDataUUID) /* value_type_ */,
                              BTGattChar::PropertyBitVal::Notify | BTGattChar::PropertyBitVal::Indicate,
                              jau::make_darray ( // DBGattDesc
                                  std::make_shared<DBGattDesc>( BTGattDesc::TYPE_USER_DESC, make_poctets("DATA_PULSE") ),
                                  DBGattDesc::createClientCharConfig()
                              ),
                              make_poctets("Synthethic Sensor 01") /* value */ )
              ) )
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

    void discoveringChanged(BTAdapter &a, const ScanType currentMeta, const ScanType changedType, const bool changedEnabled, const DiscoveryPolicy policy, const uint64_t timestamp) override {
        fprintf_td(stderr, "****** DISCOVERING: meta %s, changed[%s, enabled %d, policy %s]: %s\n",
                to_string(currentMeta).c_str(), to_string(changedType).c_str(), changedEnabled, to_string(policy).c_str(), a.toString().c_str());
        (void)timestamp;
    }

    bool deviceFound(BTDeviceRef device, const uint64_t timestamp) override {
        (void)timestamp;

        fprintf_td(stderr, "****** FOUND__-1: NOP %s\n", device->toString(true).c_str());
        return false;
    }

    void deviceUpdated(BTDeviceRef device, const EIRDataType updateMask, const uint64_t timestamp) override {
        if( SHOW_UPDATE_EVENTS ) {
            fprintf_td(stderr, "****** UPDATED: %s of %s\n", to_string(updateMask).c_str(), device->toString(true).c_str());
        }
        (void)timestamp;
    }

    void deviceConnected(BTDeviceRef device, const uint16_t handle, const uint64_t timestamp) override {
        fprintf_td(stderr, "****** CONNECTED: %s\n", device->toString(true).c_str());

        (void)handle;
        (void)timestamp;
    }

    void devicePairingState(BTDeviceRef device, const SMPPairingState state, const PairingMode mode, const uint64_t timestamp) override {
        fprintf_td(stderr, "****** PAIRING STATE: state %s, mode %s, %s\n",
            to_string(state).c_str(), to_string(mode).c_str(), device->toString().c_str());
        (void)timestamp;
        switch( state ) {
            case SMPPairingState::NONE:
                // next: deviceReady(..)
                break;
            case SMPPairingState::FAILED: {
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

    void deviceReady(BTDeviceRef device, const uint64_t timestamp) override {
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

    void deviceDisconnected(BTDeviceRef device, const HCIStatusCode reason, const uint16_t handle, const uint64_t timestamp) override {
        servedConnections = servedConnections + 1;
        fprintf_td(stderr, "****** DISCONNECTED (count %zu): Reason 0x%X (%s), old handle %s: %s\n",
                servedConnections.load(), static_cast<uint8_t>(reason), to_string(reason).c_str(),
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

        BTDeviceRef connectedDevice;
        uint16_t usedMTU = BTGattHandler::number(BTGattHandler::Defaults::MIN_ATT_MTU);

        bool matches(const BTDeviceRef& device) const {
            jau::sc_atomic_critical sync(sync_data);
            return nullptr != connectedDevice ? *connectedDevice == *device : false;
        }

        void clear() {
            jau::sc_atomic_critical sync(sync_data);

            handlePulseDataNotify = 0;
            handlePulseDataIndicate = 0;
            handleResponseDataNotify = 0;
            handleResponseDataIndicate = 0;
            connectedDevice = nullptr;

            dbGattServer->resetGattClientCharConfig(DataServiceUUID, PulseDataUUID);
            dbGattServer->resetGattClientCharConfig(DataServiceUUID, ResponseUUID);
        }

        void pulseSender() {
            while( !stopPulseSender ) {
                {
                    jau::sc_atomic_critical sync(sync_data);
                    if( nullptr != connectedDevice && connectedDevice->getConnected() ) {
                        if( 0 != handlePulseDataNotify || 0 != handlePulseDataIndicate ) {
                            std::string data( "Dynamic Data Example. Elapsed Milliseconds: "+jau::to_decstring(environment::getElapsedMillisecond(), ',', 9) );
                            jau::POctets v(data.size()+1, jau::endian::little);
                            v.put_string_nc(0, data, v.size(), true /* includeEOS */);
                            if( 0 != handlePulseDataNotify ) {
                                fprintf_td(stderr, "****** GATT::sendNotification: PULSE to %s\n", connectedDevice->toString().c_str());
                                connectedDevice->sendNotification(handlePulseDataNotify, v);
                            }
                            if( 0 != handlePulseDataIndicate ) {
                                fprintf_td(stderr, "****** GATT::sendIndication: PULSE to %s\n", connectedDevice->toString().c_str());
                                connectedDevice->sendIndication(handlePulseDataIndicate, v);
                            }
                        }
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }

        void sendResponse(jau::POctets data) {
            if( nullptr != connectedDevice && connectedDevice->getConnected() ) {
                if( 0 != handleResponseDataNotify || 0 != handleResponseDataIndicate ) {
                    if( 0 != handleResponseDataNotify ) {
                        fprintf_td(stderr, "****** GATT::sendNotification: %s to %s\n",
                                data.toString().c_str(), connectedDevice->toString().c_str());
                        connectedDevice->sendNotification(handleResponseDataNotify, data);
                    }
                    if( 0 != handleResponseDataIndicate ) {
                        fprintf_td(stderr, "****** GATT::sendIndication: %s to %s\n",
                                data.toString().c_str(), connectedDevice->toString().c_str());
                        connectedDevice->sendIndication(handleResponseDataIndicate, data);
                    }
                }
            }
        }

    public:
        MyGATTServerListener()
        : pulseSenderThread(&MyGATTServerListener::pulseSender, this)
        { }

        ~MyGATTServerListener() noexcept { close(); }

        void close() noexcept {
            {
                jau::sc_atomic_critical sync(sync_data);
                stopPulseSender = true;
                connectedDevice = nullptr;
            }
            if( pulseSenderThread.joinable() ) {
                pulseSenderThread.join();
            }
        }

        void connected(BTDeviceRef device, const uint16_t initialMTU) override {
            jau::sc_atomic_critical sync(sync_data);
            const bool available = nullptr == connectedDevice;
            fprintf_td(stderr, "****** GATT::connected(available %d): initMTU %d, %s\n",
                    available, (int)initialMTU, device->toString().c_str());
            if( available ) {
                connectedDevice = device;
                usedMTU = initialMTU;
            }
        }

        void disconnected(BTDeviceRef device) override {
            jau::sc_atomic_critical sync(sync_data);
            const bool match = nullptr != connectedDevice ? *connectedDevice == *device : false;
            fprintf_td(stderr, "****** GATT::disconnected(match %d): %s\n", match, device->toString().c_str());
            if( match ) {
                clear();
            }
        }

        void mtuChanged(BTDeviceRef device, const uint16_t mtu) override {
            const bool match = matches(device);
            fprintf_td(stderr, "****** GATT::mtuChanged(match %d): %d -> %d, %s\n",
                    match, match ? (int)usedMTU : 0, (int)mtu, device->toString().c_str());
            if( match ) {
                usedMTU = mtu;
            }
        }

        bool readCharValue(BTDeviceRef device, DBGattServiceRef s, DBGattCharRef c) override {
            const bool match = matches(device);
            fprintf_td(stderr, "****** GATT::readCharValue(match %d): to %s, from\n  %s\n    %s\n",
                    match, device->toString().c_str(), s->toString().c_str(), c->toString().c_str());
            return match;
        }

        bool readDescValue(BTDeviceRef device, DBGattServiceRef s, DBGattCharRef c, DBGattDescRef d) override {
            const bool match = matches(device);
            fprintf_td(stderr, "****** GATT::readDescValue(match %d): to %s, from\n  %s\n    %s\n      %s\n",
                    match, device->toString().c_str(), s->toString().c_str(), c->toString().c_str(), d->toString().c_str());
            return match;
        }

        bool writeCharValue(BTDeviceRef device, DBGattServiceRef s, DBGattCharRef c, const jau::TROOctets & value, const uint16_t value_offset) override {
            const bool match = matches(device);
            fprintf_td(stderr, "****** GATT::writeCharValue(match %d): %s '%s' @ %u from %s, to\n  %s\n    %s\n",
                    match, value.toString().c_str(), jau::dfa_utf8_decode( value.get_ptr(), value.size() ).c_str(),
                    value_offset,
                    device->toString().c_str(), s->toString().c_str(), c->toString().c_str());
            return match;
        }
        void writeCharValueDone(BTDeviceRef device, DBGattServiceRef s, DBGattCharRef c) override {
            const bool match = matches(device);
            const jau::TROOctets& value = c->getValue();
            fprintf_td(stderr, "****** GATT::writeCharValueDone(match %d): From %s, to\n  %s\n    %s\n    Char-Value: %s\n",
                    match, device->toString().c_str(), s->toString().c_str(), c->toString().c_str(), value.toString().c_str());

            if( match &&
                c->getValueType()->equivalent( CommandUUID ) &&
                ( 0 != handleResponseDataNotify || 0 != handleResponseDataIndicate ) )
            {
                jau::POctets value2(value);
                std::thread senderThread(&MyGATTServerListener::sendResponse, this, value2); // @suppress("Invalid arguments")
                senderThread.detach();
            }
        }

        bool writeDescValue(BTDeviceRef device, DBGattServiceRef s, DBGattCharRef c, DBGattDescRef d, const jau::TROOctets & value, const uint16_t value_offset) override {
            const bool match = matches(device);
            fprintf_td(stderr, "****** GATT::writeDescValue(match %d): %s '%s' @ %u from %s\n  %s\n    %s\n      %s\n",
                    match, value.toString().c_str(), jau::dfa_utf8_decode( value.get_ptr(), value.size() ).c_str(),
                    value_offset,
                    device->toString().c_str(), s->toString().c_str(), c->toString().c_str(), d->toString().c_str());
            return match;
        }
        void writeDescValueDone(BTDeviceRef device, DBGattServiceRef s, DBGattCharRef c, DBGattDescRef d) override {
            const bool match = matches(device);
            const jau::TROOctets& value = d->getValue();
            fprintf_td(stderr, "****** GATT::writeDescValueDone(match %d): From %s\n  %s\n    %s\n      %s\n    Desc-Value: %s\n",
                    match, device->toString().c_str(), s->toString().c_str(), c->toString().c_str(), d->toString().c_str(), value.toString().c_str());
        }

        void clientCharConfigChanged(BTDeviceRef device, DBGattServiceRef s, DBGattCharRef c, DBGattDescRef d, const bool notificationEnabled, const bool indicationEnabled) override {
            const bool match = matches(device);
            const jau::TROOctets& value = d->getValue();
            fprintf_td(stderr, "****** GATT::clientCharConfigChanged(match %d): notify %d, indicate %d from %s\n  %s\n    %s\n      %s\n    Desc-Value: %s\n",
                    match, notificationEnabled, indicationEnabled,
                    device->toString().c_str(), s->toString().c_str(), c->toString().c_str(), d->toString().c_str(), value.toString().c_str());

            if( match ) {
                jau::sc_atomic_critical sync(sync_data);
                if( c->getValueType()->equivalent( PulseDataUUID ) ) {
                    handlePulseDataNotify = notificationEnabled ? c->getValueHandle() : 0;
                    handlePulseDataIndicate = indicationEnabled ? c->getValueHandle() : 0;
                } else if( c->getValueType()->equivalent( ResponseUUID ) ) {
                    handleResponseDataNotify = notificationEnabled ? c->getValueHandle() : 0;
                    handleResponseDataIndicate = indicationEnabled ? c->getValueHandle() : 0;
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
    EInfoReport eir;
    EIRDataType adv_mask = EIRDataType::FLAGS | EIRDataType::SERVICE_UUID;
    EIRDataType scanrsp_mask = EIRDataType::NAME | EIRDataType::CONN_IVAL;

    eir.addFlags(GAPFlags::LE_Gen_Disc);
    eir.addFlags(GAPFlags::BREDR_UNSUP);

    eir.addService(DataServiceUUID);
    eir.setServicesComplete(false);

    eir.setName(a->getName());
    eir.setConnInterval(10, 24);

    DBGattCharRef gattDevNameChar = dbGattServer->findGattChar( jau::uuid16_t(GattServiceType::GENERIC_ACCESS),
                                                                jau::uuid16_t(GattCharacteristicType::DEVICE_NAME) );
    if( nullptr != gattDevNameChar ) {
        std::string aname = a->getName();
        gattDevNameChar->setValue(reinterpret_cast<uint8_t*>(aname.data()), aname.size(), 0);
    }

    fprintf_td(stderr, "****** Start advertising (%s): EIR %s\n", msg.c_str(), eir.toString().c_str());
    fprintf_td(stderr, "****** Start advertising (%s): adv %s, scanrsp %s\n", msg.c_str(), to_string(adv_mask).c_str(), to_string(scanrsp_mask).c_str());

    HCIStatusCode status = a->startAdvertising(dbGattServer, eir, adv_mask, scanrsp_mask,
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

static void processDisconnectedDevice(BTDeviceRef device) {
    fprintf_td(stderr, "****** Disconnected Device (count %zu): Start %s\n",
            servedConnections.load(), device->toString().c_str());

    // already unpaired
    stopAdvertising(&device->getAdapter(), "device-disconnected");
    BTDeviceRegistry::removeFromProcessingDevices(device->getAddressAndType());
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // wait a little (FIXME: Fast restart of advertising error)

    if( !RUN_ONLY_ONCE ) {
        startAdvertising(&device->getAdapter(), "device-disconnected");
    }

    fprintf_td(stderr, "****** Disonnected Device: End %s\n", device->toString().c_str());
}

static void processReadyDevice(BTDeviceRef device) {
    fprintf_td(stderr, "****** Processing Ready Device: Start %s\n", device->toString().c_str());

    fprintf_td(stderr, "****** Processing Ready Device: End %s\n", device->toString().c_str());
}

static bool initAdapter(std::shared_ptr<BTAdapter>& adapter) {
    if( useAdapter != EUI48::ALL_DEVICE && useAdapter != adapter->getAddressAndType().address ) {
        fprintf_td(stderr, "initAdapter: Adapter not selected: %s\n", adapter->toString().c_str());
        return false;
    }
    if( !adapter->isInitialized() ) {
        // Initialize with defaults and power-on
        const HCIStatusCode status = adapter->initialize( btMode );
        if( HCIStatusCode::SUCCESS != status ) {
            fprintf_td(stderr, "initAdapter: initialize failed: %s: %s\n",
                    to_string(status).c_str(), adapter->toString().c_str());
            return false;
        }
    } else if( !adapter->setPowered( true ) ) {
        fprintf_td(stderr, "initAdapter: setPower.1 on failed: %s\n", adapter->toString().c_str());
        return false;
    }
    // adapter is powered-on
    fprintf_td(stderr, "initAdapter.1: %s\n", adapter->toString().c_str());

    if( adapter->setPowered(false) ) {
        HCIStatusCode status = adapter->setName(adapter_name, adapter_short_name);
        if( HCIStatusCode::SUCCESS == status ) {
            fprintf_td(stderr, "initAdapter: setLocalName OK: %s\n", adapter->toString().c_str());
        } else {
            fprintf_td(stderr, "initAdapter: setLocalName failed: %s\n", adapter->toString().c_str());
            return false;
        }

        status = adapter->setSecureConnections( use_SC );
        if( HCIStatusCode::SUCCESS == status ) {
            fprintf_td(stderr, "initAdapter: setSecureConnections OK: %s\n", adapter->toString().c_str());
        } else {
            fprintf_td(stderr, "initAdapter: setSecureConnections failed: %s\n", adapter->toString().c_str());
            return false;
        }

        const uint16_t conn_min_interval = 8;  // 10ms
        const uint16_t conn_max_interval = 40; // 50ms
        const uint16_t conn_latency = 0;
        const uint16_t supervision_timeout = 50; // 500ms
        status = adapter->setDefaultConnParam(conn_min_interval, conn_max_interval, conn_latency, supervision_timeout);
        if( HCIStatusCode::SUCCESS == status ) {
            fprintf_td(stderr, "initAdapter: setDefaultConnParam OK: %s\n", adapter->toString().c_str());
        } else {
            fprintf_td(stderr, "initAdapter: setDefaultConnParam failed: %s\n", adapter->toString().c_str());
            return false;
        }

        if( !adapter->setPowered( true ) ) {
            fprintf_td(stderr, "initAdapter: setPower.2 on failed: %s\n", adapter->toString().c_str());
            return false;
        }
    } else {
        fprintf_td(stderr, "initAdapter: setPowered.2 off failed: %s\n", adapter->toString().c_str());
        return false;
    }
    fprintf_td(stderr, "initAdapter.2: %s\n", adapter->toString().c_str());

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
    adapter->setSMPKeyPath(SERVER_KEY_PATH);

    std::shared_ptr<AdapterStatusListener> asl( std::make_shared<MyAdapterStatusListener>() );
    adapter->addStatusListener( asl );
    // Flush discovered devices after registering our status listener.
    // This avoids discovered devices before we have registered!
    adapter->removeDiscoveredDevices();

    adapter->setServerConnSecurity(adapter_sec_level, SMPIOCapability::UNSET);

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
    timestamp_t0 = getCurrentMilliseconds();

    fprintf_td(stderr, "****** Test Start\n");

    std::shared_ptr<MyGATTServerListener> listener = std::make_shared<MyGATTServerListener>();
    dbGattServer->addListener( listener );

    BTManager & mngr = BTManager::get();
    mngr.addChangedAdapterSetCallback(myChangedAdapterSetFunc);

    while( !RUN_ONLY_ONCE || 0 == servedConnections ) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }

    fprintf_td(stderr, "****** Test Shutdown.01 (DBGattServer.remove-listener)\n");
    dbGattServer->removeListener( listener );

    fprintf_td(stderr, "****** Test Shutdown.02 (listener.close)\n");
    listener->close();

    fprintf_td(stderr, "****** Test Shutdown.03 (DBGattServer.close := nullptr)\n");
    dbGattServer = nullptr;

    chosenAdapter = nullptr;

    fprintf_td(stderr, "****** Test End\n");
}

#include <cstdio>

int main(int argc, char *argv[])
{
    bool waitForEnter=false;

    fprintf_td(stderr, "DirectBT Native Version %s (API %s)\n", DIRECT_BT_VERSION, DIRECT_BT_VERSION_API);

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
            dbGattServer->setMaxAttMTU( atoi(argv[++i]) );
        } else if( !strcmp("-seclevel", argv[i]) && argc > (i+1) ) {
            adapter_sec_level = to_BTSecurityLevel(atoi(argv[++i]));
            fprintf(stderr, "Set adapter sec_level %s\n", to_string(adapter_sec_level).c_str());
        } else if( !strcmp("-once", argv[i]) ) {
            RUN_ONLY_ONCE = true;
        }
    }
    fprintf_td(stderr, "pid %d\n", getpid());

    fprintf_td(stderr, "Run with '[-btmode LE|BREDR|DUAL] [-use_sc 0|1] "
                    "[-adapter <adapter_address>] "
                    "[-name <adapter_name>] "
                    "[-short_name <adapter_short_name>] "
                    "[-mtu <max att_mtu>] "
                    "[-seclevel <int_sec_level>]* "
                    "[-once] "
                    "[-dbt_verbose true|false] "
                    "[-dbt_debug true|false|adapter.event,gatt.data,hci.event,hci.scan_ad_eir,mgmt.event] "
                    "[-dbt_mgmt cmd.timeout=3000,ringsize=64,...] "
                    "[-dbt_hci cmd.complete.timeout=10000,cmd.status.timeout=3000,ringsize=64,...] "
                    "[-dbt_gatt cmd.read.timeout=500,cmd.write.timeout=500,cmd.init.timeout=2500,ringsize=128,...] "
                    "[-dbt_l2cap reader.timeout=10000,restart.count=0,...] "
                    "\n");

    fprintf_td(stderr, "SHOW_UPDATE_EVENTS %d\n", SHOW_UPDATE_EVENTS);
    fprintf_td(stderr, "adapter %s\n", useAdapter.toString().c_str());
    fprintf_td(stderr, "adapter btmode %s\n", to_string(btMode).c_str());
    fprintf_td(stderr, "adapter SC %s\n", to_string(use_SC).c_str());
    fprintf_td(stderr, "adapter name %s (short %s)\n", adapter_name.c_str(), adapter_short_name.c_str());
    fprintf_td(stderr, "adapter mtu %d\n", (int)dbGattServer->getMaxAttMTU());
    fprintf_td(stderr, "adapter sec_level %s\n", to_string(adapter_sec_level).c_str());
    fprintf_td(stderr, "once %d\n", (int)RUN_ONLY_ONCE);
    fprintf_td(stderr, "GattServer %s\n", dbGattServer->toString().c_str());
    fprintf_td(stderr, "GattServer.services: %s\n", dbGattServer->getServices().get_info().c_str());
    fprintf_td(stderr, "GattService.characteristics: %s\n", dbGattServer->getServices()[0]->getCharacteristics().get_info().c_str());

    if( waitForEnter ) {
        fprintf_td(stderr, "Press ENTER to continue\n");
        getchar();
    }
    fprintf_td(stderr, "****** TEST start\n");
    test();
    fprintf_td(stderr, "****** TEST end\n");
}
