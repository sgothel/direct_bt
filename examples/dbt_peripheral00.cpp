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
 * This _dbt_peripheral00__ C++ peripheral ::BTRole::Slave GATT server example uses an event driven workflow.
 *
 * ### dbt_peripheral00 Invocation Examples:
 * Using `scripts/run-dbt_peripheral00.sh` from `dist` directory:
 *
 * * Serving clients as `TestDevice001` using adapter 00:1A:7D:DA:71:03; Using ENC_ONLY (JUST_WORKS) encryption.
 *   ~~~
 *   ../scripts/run-dbt_peripheral00.sh -adapter 00:1A:7D:DA:71:03 -name TestDevice001 -seclevel 2
 *   ~~~
 */

static EUI48 useAdapter = EUI48::ALL_DEVICE;
static BTMode btMode = BTMode::DUAL;
static bool use_SC = true;
static std::string adapter_name = "TestDev001_N"; // NOLINT(bugprone-throwing-static-initialization)
static std::string adapter_short_name = "TDev001N"; // NOLINT(bugprone-throwing-static-initialization)
static std::shared_ptr<BTAdapter> chosenAdapter = nullptr;
static BTSecurityLevel adapter_sec_level = BTSecurityLevel::UNSET;
static SMPIOCapability adapter_sec_io_cap = SMPIOCapability::UNSET;
static bool SHOW_UPDATE_EVENTS = false;
static bool RUN_ONLY_ONCE = false;
static jau::sc_atomic_bool sync_data;
static BTDeviceRef connectedDevice = nullptr;

static jau::relaxed_atomic_nsize_t servedConnections = 0;

static bool startAdvertising(BTAdapter *a, std::string msg); // NOLINT(performance-unnecessary-value-param): Pass-by-value out-of-thread
static bool stopAdvertising(BTAdapter *a, std::string msg); // NOLINT(performance-unnecessary-value-param): Pass-by-value out-of-thread
static void processDisconnectedDevice(BTDeviceRef device); // NOLINT(performance-unnecessary-value-param): Pass-by-value out-of-thread

static void setDevice(const BTDeviceRef& cd) {
    jau::sc_atomic_critical sync(sync_data);
    connectedDevice = cd;
}

static BTDeviceRef getDevice() {
    jau::sc_atomic_critical sync(sync_data);
    return connectedDevice;
}

static bool matches(const BTDeviceRef& device) {
    const BTDeviceRef d = getDevice();
    return nullptr != d ? (*d) == *device : false;
}

// NOLINTBEGIN(bugprone-throwing-static-initialization)
static const jau::uuid128_t DataServiceUUID = jau::uuid128_t("d0ca6bf3-3d50-4760-98e5-fc5883e93712");
static const jau::uuid128_t StaticDataUUID  = jau::uuid128_t("d0ca6bf3-3d51-4760-98e5-fc5883e93712");
static const jau::uuid128_t CommandUUID     = jau::uuid128_t("d0ca6bf3-3d52-4760-98e5-fc5883e93712");
static const jau::uuid128_t ResponseUUID    = jau::uuid128_t("d0ca6bf3-3d53-4760-98e5-fc5883e93712");
static const jau::uuid128_t PulseDataUUID   = jau::uuid128_t("d0ca6bf3-3d54-4760-98e5-fc5883e93712");
// NOLINTEND(bugprone-throwing-static-initialization)

// DBGattServerRef dbGattServer = std::make_shared<DBGattServer>(
static DBGattServerRef dbGattServer( new DBGattServer( // NOLINT(bugprone-throwing-static-initialization)
        /* services: */
        jau::make_darray( // DBGattService
          std::make_shared<DBGattService> ( true /* primary */,
              std::make_unique<const jau::uuid16_t>(GattServiceType::GENERIC_ACCESS) /* type_ */,
              jau::make_darray ( // DBGattChar
                  std::make_shared<DBGattChar>( std::make_unique<const jau::uuid16_t>(GattCharacteristicType::DEVICE_NAME) /* value_type_ */,
                              BTGattChar::PropertyBitVal::Read,
                              jau::darray<DBGattDescRef>() /* intentionally empty */,
                              make_gvalue(adapter_name, 128) /* value */, true /* variable_length */ ),
                  std::make_shared<DBGattChar>( std::make_unique<const jau::uuid16_t>(GattCharacteristicType::APPEARANCE) /* value_type_ */,
                              BTGattChar::PropertyBitVal::Read,
                              jau::darray<DBGattDescRef>() /* intentionally empty */,
                              make_gvalue((uint16_t)0) /* value */ )
              ) ),
          std::make_shared<DBGattService> ( true /* primary */,
              std::make_unique<const jau::uuid16_t>(GattServiceType::DEVICE_INFORMATION) /* type_ */,
              jau::make_darray ( // DBGattChar
                  std::make_shared<DBGattChar>( std::make_unique<const jau::uuid16_t>(GattCharacteristicType::MANUFACTURER_NAME_STRING) /* value_type_ */,
                              BTGattChar::PropertyBitVal::Read,
                              jau::darray<DBGattDescRef>() /* intentionally empty */,
                              make_gvalue("Gothel Software") /* value */ ),
                  std::make_shared<DBGattChar>( std::make_unique<const jau::uuid16_t>(GattCharacteristicType::MODEL_NUMBER_STRING) /* value_type_ */,
                              BTGattChar::PropertyBitVal::Read,
                              jau::darray<DBGattDescRef>() /* intentionally empty */,
                              make_gvalue("2.4.0-pre") /* value */ ),
                  std::make_shared<DBGattChar>( std::make_unique<const jau::uuid16_t>(GattCharacteristicType::SERIAL_NUMBER_STRING) /* value_type_ */,
                              BTGattChar::PropertyBitVal::Read,
                              jau::darray<DBGattDescRef>() /* intentionally empty */,
                              make_gvalue("sn:0123456789") /* value */ ),
                  std::make_shared<DBGattChar>( std::make_unique<const jau::uuid16_t>(GattCharacteristicType::HARDWARE_REVISION_STRING) /* value_type_ */,
                              BTGattChar::PropertyBitVal::Read,
                              jau::darray<DBGattDescRef>() /* intentionally empty */,
                              make_gvalue("hw:0123456789") /* value */ ),
                  std::make_shared<DBGattChar>( std::make_unique<const jau::uuid16_t>(GattCharacteristicType::FIRMWARE_REVISION_STRING) /* value_type_ */,
                              BTGattChar::PropertyBitVal::Read,
                              jau::darray<DBGattDescRef>() /* intentionally empty */,
                              make_gvalue("fw:0123456789") /* value */ ),
                  std::make_shared<DBGattChar>( std::make_unique<const jau::uuid16_t>(GattCharacteristicType::SOFTWARE_REVISION_STRING) /* value_type_ */,
                              BTGattChar::PropertyBitVal::Read,
                              jau::darray<DBGattDescRef>() /* intentionally empty */,
                              make_gvalue("sw:0123456789") /* value */ )
              ) ),
          std::make_shared<DBGattService> ( true /* primary */,
              std::make_unique<const jau::uuid128_t>(DataServiceUUID) /* type_ */,
              jau::make_darray ( // DBGattChar
                  std::make_shared<DBGattChar>( std::make_unique<const jau::uuid128_t>(StaticDataUUID) /* value_type_ */,
                              BTGattChar::PropertyBitVal::Read,
                              jau::make_darray ( // DBGattDesc
                                  std::make_shared<DBGattDesc>( BTGattDesc::TYPE_USER_DESC, make_gvalue("DATA_STATIC") )
                              ),
                            make_gvalue("Proprietary Static Data 0x00010203") /* value */ ),
                  std::make_shared<DBGattChar>( std::make_unique<const jau::uuid128_t>(CommandUUID) /* value_type_ */,
                              BTGattChar::PropertyBitVal::WriteNoAck | BTGattChar::PropertyBitVal::WriteWithAck,
                              jau::make_darray ( // DBGattDesc
                                  std::make_shared<DBGattDesc>( BTGattDesc::TYPE_USER_DESC, make_gvalue("COMMAND") )
                              ),
                              make_gvalue(128, 64) /* value */, true /* variable_length */ ),
                  std::make_shared<DBGattChar>( std::make_unique<const jau::uuid128_t>(ResponseUUID) /* value_type_ */,
                              BTGattChar::PropertyBitVal::Notify | BTGattChar::PropertyBitVal::Indicate,
                              jau::make_darray ( // DBGattDesc
                                  std::make_shared<DBGattDesc>( BTGattDesc::TYPE_USER_DESC, make_gvalue("RESPONSE") ),
                                  DBGattDesc::createClientCharConfig()
                              ),
                              make_gvalue((uint16_t)0) /* value */ ),
                  std::make_shared<DBGattChar>( std::make_unique<const jau::uuid128_t>(PulseDataUUID) /* value_type_ */,
                              BTGattChar::PropertyBitVal::Notify | BTGattChar::PropertyBitVal::Indicate,
                              jau::make_darray ( // DBGattDesc
                                  std::make_shared<DBGattDesc>( BTGattDesc::TYPE_USER_DESC, make_gvalue("DATA_PULSE") ),
                                  DBGattDesc::createClientCharConfig()
                              ),
                              make_gvalue("Synthethic Sensor 01") /* value */ )
              ) )
        ) ) );


class MyAdapterStatusListener : public AdapterStatusListener { // NOLINT(misc-use-internal-linkage)
  public:
    void adapterSettingsChanged(BTAdapter &a, const AdapterSetting oldmask, const AdapterSetting newmask,
                                const AdapterSetting changedmask, const uint64_t timestamp) override {
        const bool initialSetting = AdapterSetting::NONE == oldmask;
        if( initialSetting ) {
            jau_fprintf_td(stderr, "****** SETTINGS_INITIAL: %s -> %s, changed %s\n", oldmask,
                    newmask, changedmask);
        } else {
            jau_fprintf_td(stderr, "****** SETTINGS_CHANGED: %s -> %s, changed %s\n", oldmask,
                    newmask, changedmask);
        }
        jau_fprintf_td(stderr, "Status BTAdapter:\n");
        jau_fprintf_td(stderr, "%s\n", a);
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
        jau_fprintf_td(stderr, "****** DISCOVERING: meta %s, changed[%s, enabled %d, policy %s]: %s\n",
                currentMeta, changedType, changedEnabled, policy, a);
        (void)timestamp;
    }

    bool deviceFound(const BTDeviceRef& device, const uint64_t timestamp) override {
        (void)timestamp;

        jau_fprintf_td(stderr, "****** FOUND__-1: NOP %s\n", device->toString(true));
        return false;
    }

    void deviceUpdated(const BTDeviceRef& device, const EIRDataType updateMask, const uint64_t timestamp) override {
        if( is_set(updateMask, EIRDataType::BDADDR)) {
            jau_fprintf_td(stderr, "****** UPDATED (ADDR-RESOLVED): %s of %s\n", updateMask, device->toString(true));
        } else if( SHOW_UPDATE_EVENTS ) {
            jau_fprintf_td(stderr, "****** UPDATED: %s of %s\n", updateMask, device->toString(true));
        }
        (void)timestamp;
    }

    void deviceConnected(const BTDeviceRef& device, const bool discovered, const uint64_t timestamp) override {
        jau_fprintf_td(stderr, "****** CONNECTED (discovered %d): %s\n", discovered, device->toString(true));
        const bool available = nullptr == getDevice();
        if( available ) {
            setDevice(device);
        }
        (void)discovered;
        (void)timestamp;
    }

    void devicePairingState(const BTDeviceRef& device, const SMPPairingState state, const PairingMode mode, const uint64_t timestamp) override {
        jau_fprintf_td(stderr, "****** PAIRING STATE: state %s, mode %s, %s\n",
            state, mode, device->toString());
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
                    std::thread dc(&BTDevice::setPairingNumericComparison, device, true); // FIXME
                    dc.detach();
                }
                // next: KEY_DISTRIBUTION or FAILED
              } break;
            case SMPPairingState::PASSKEY_NOTIFY: {
                jau_fprintf_td(stderr, "****** \n");
                jau_fprintf_td(stderr, "****** \n");
                jau_fprintf_td(stderr, "****** Confirm on your device %s\n", device->getName());
                jau_fprintf_td(stderr, "****** PassKey: %s\n", device->getResponderSMPPassKeyString());
                jau_fprintf_td(stderr, "****** \n");
                jau_fprintf_td(stderr, "****** \n");
                // next: KEY_DISTRIBUTION or FAILED
              } break;
            case SMPPairingState::OOB_EXPECTED:
                // FIXME: jau_ABORT
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

    void deviceReady(const BTDeviceRef& device, const uint64_t timestamp) override {
        (void)timestamp;
        jau_fprintf_td(stderr, "****** READY-1: NOP %s\n", device->toString(true));
    }

    void deviceDisconnected(const BTDeviceRef& device, const HCIStatusCode reason, const uint16_t handle, const uint64_t timestamp) override {
        servedConnections = servedConnections + 1;
        jau_fprintf_td(stderr, "****** DISCONNECTED (count %zu): Reason 0x%X (%s), old handle %s: %s\n",
                servedConnections.load(), static_cast<uint8_t>(reason), reason,
                toHexString(handle), device->toString(true));

        const bool match = matches(device);
        if( match ) {
            setDevice(nullptr);
        }
        std::thread sd(::processDisconnectedDevice, device); // @suppress("Invalid arguments")
        sd.detach();
        (void)timestamp;
    }

    std::string toString() const noexcept override {
        return "MyAdapterStatusListener[this "+toHexString(this)+"]";
    }

};

class MyGATTServerListener : public DBGattServer::Listener { // NOLINT(misc-use-internal-linkage)
    private:
        jau::service_runner pulse_service;

        jau::sc_atomic_uint16 handlePulseDataNotify = 0;
        jau::sc_atomic_uint16 handlePulseDataIndicate = 0;
        jau::sc_atomic_uint16 handleResponseDataNotify = 0;
        jau::sc_atomic_uint16 handleResponseDataIndicate = 0;

        uint16_t usedMTU = BTGattHandler::number(BTGattHandler::Defaults::MIN_ATT_MTU);

        void clear() {
            jau::sc_atomic_critical sync(sync_data);

            handlePulseDataNotify = 0;
            handlePulseDataIndicate = 0;
            handleResponseDataNotify = 0;
            handleResponseDataIndicate = 0;

            dbGattServer->resetGattClientCharConfig(DataServiceUUID, PulseDataUUID);
            dbGattServer->resetGattClientCharConfig(DataServiceUUID, ResponseUUID);
        }

        void pulse_worker_init(jau::service_runner& sr) noexcept {
            (void)sr;
            const BTDeviceRef connectedDevice_ = getDevice();
            const std::string connectedDeviceStr = nullptr != connectedDevice_ ? connectedDevice_->toString() : "n/a";
            jau_fprintf_td(stderr, "****** Server GATT::PULSE Start %s\n", connectedDeviceStr);
        }
        void pulse_worker(jau::service_runner& sr) {
            BTDeviceRef connectedDevice_ = getDevice();
            if( nullptr != connectedDevice_ && connectedDevice_->getConnected() ) {
                if( 0 != handlePulseDataNotify || 0 != handlePulseDataIndicate ) {
                    std::string data( "Dynamic Data Example. Elapsed Milliseconds: "+jau::to_decstring(environment::getElapsedMillisecond(), ',', 9) );
                    jau::POctets v(data.size()+1, jau::lb_endian_t::little);
                    v.put_string_nc(0, data, v.size(), true /* includeEOS */);
                    if( 0 != handlePulseDataNotify ) {
                        const bool res = connectedDevice_->sendNotification(handlePulseDataNotify, v);
                        jau_fprintf_td(stderr, "****** GATT::sendNotification: PULSE (res %d) to %s\n", res, connectedDevice_->toString());
                    }
                    if( 0 != handlePulseDataIndicate ) {
                        const bool res = connectedDevice_->sendIndication(handlePulseDataIndicate, v);
                        jau_fprintf_td(stderr, "****** GATT::sendIndication: PULSE (res %d) to %s\n", res, connectedDevice_->toString());
                    }
                }
            }
            if( !sr.shall_stop() ) {
                jau::sleep_for( 500_ms );
            }
        }
        void pulse_worker_end(jau::service_runner& sr) noexcept {
            (void)sr;
            const BTDeviceRef connectedDevice_ = getDevice();
            const std::string connectedDeviceStr = nullptr != connectedDevice_ ? connectedDevice_->toString() : "n/a";
            jau_fprintf_td(stderr, "****** Server GATT::PULSE End %s\n", connectedDeviceStr);
        }

        void sendResponse(jau::POctets data) { // NOLINT(performance-unnecessary-value-param): Pass-by-value out-of-thread
            BTDeviceRef connectedDevice_ = getDevice();
            if( nullptr != connectedDevice_ && connectedDevice_->getConnected() ) {
                if( 0 != handleResponseDataNotify || 0 != handleResponseDataIndicate ) {
                    if( 0 != handleResponseDataNotify ) {
                        const bool res = connectedDevice_->sendNotification(handleResponseDataNotify, data);
                        jau_fprintf_td(stderr, "****** GATT::sendNotification (res %d): %s to %s\n",
                                res, data, connectedDevice_->toString());
                    }
                    if( 0 != handleResponseDataIndicate ) {
                        const bool res = connectedDevice_->sendIndication(handleResponseDataIndicate, data);
                        jau_fprintf_td(stderr, "****** GATT::sendIndication (res %d): %s to %s\n",
                                res, data, connectedDevice_->toString());
                    }
                }
            }
        }

    public:
        MyGATTServerListener()
        : pulse_service("MyGATTServerListener::pulse", THREAD_SHUTDOWN_TIMEOUT_MS,
                             jau::bind_member(this, &MyGATTServerListener::pulse_worker),
                             jau::bind_member(this, &MyGATTServerListener::pulse_worker_init),
                             jau::bind_member(this, &MyGATTServerListener::pulse_worker_end))
        {
            pulse_service.start();
        }

        ~MyGATTServerListener() noexcept override {
            pulse_service.stop();
        }

        void close() noexcept {
            pulse_service.stop();
            clear();
        }

        void connected(const BTDeviceRef& device, const uint16_t initialMTU) override {
            jau::sc_atomic_critical sync(sync_data);
            const bool match = matches(device);
            jau_fprintf_td(stderr, "****** GATT::connected(match %d): initMTU %d, %s\n",
                    match, (int)initialMTU, device->toString());
            if( match ) {
                usedMTU = initialMTU;
            }
        }

        void disconnected(const BTDeviceRef& device) override {
            jau::sc_atomic_critical sync(sync_data);
            const bool match = matches(device);
            jau_fprintf_td(stderr, "****** GATT::disconnected(match %d): %s\n", match, device->toString());
            if( match ) {
                clear();
            }
        }

        void mtuChanged(const BTDeviceRef& device, const uint16_t mtu) override {
            const bool match = matches(device);
            jau_fprintf_td(stderr, "****** GATT::mtuChanged(match %d): %d -> %d, %s\n",
                    match, match ? (int)usedMTU : 0, (int)mtu, device->toString());
            if( match ) {
                usedMTU = mtu;
            }
        }

        bool readCharValue(const BTDeviceRef& device, const DBGattServiceRef& s, const DBGattCharRef& c) override {
            const bool match = matches(device);
            jau_fprintf_td(stderr, "****** GATT::readCharValue(match %d): to %s, from\n  %s\n    %s\n",
                    match, device->toString(), s->toString(), c->toString());
            return match;
        }

        bool readDescValue(const BTDeviceRef& device, const DBGattServiceRef& s, const DBGattCharRef& c, const DBGattDescRef& d) override {
            const bool match = matches(device);
            jau_fprintf_td(stderr, "****** GATT::readDescValue(match %d): to %s, from\n  %s\n    %s\n      %s\n",
                    match, device->toString(), s->toString(), c->toString(), d->toString());
            return match;
        }

        bool writeCharValue(const BTDeviceRef& device, const DBGattServiceRef& s, const DBGattCharRef& c, const jau::TROOctets & value, const uint16_t value_offset) override {
            const bool match = matches(device);
            jau_fprintf_td(stderr, "****** GATT::writeCharValue(match %d): %s '%s' @ %u from %s, to\n  %s\n    %s\n",
                    match, value, jau::dfa_utf8_decode( value.get_ptr(), value.size() ),
                    value_offset,
                    device->toString(), s->toString(), c->toString());
            return match;
        }
        void writeCharValueDone(const BTDeviceRef& device, const DBGattServiceRef& s, const DBGattCharRef& c) override {
            const bool match = matches(device);
            const jau::TROOctets& value = c->getValue();
            jau_fprintf_td(stderr, "****** GATT::writeCharValueDone(match %d): From %s, to\n  %s\n    %s\n    Char-Value: %s\n",
                    match, device->toString(), s->toString(), c->toString(), value);

            if( match &&
                c->getValueType()->equivalent( CommandUUID ) &&
                ( 0 != handleResponseDataNotify || 0 != handleResponseDataIndicate ) )
            {
                jau::POctets value2(value);
                std::thread senderThread(&MyGATTServerListener::sendResponse, this, value2); // @suppress("Invalid arguments")
                senderThread.detach();
            }
        }

        bool writeDescValue(const BTDeviceRef& device, const DBGattServiceRef& s, const DBGattCharRef& c, const DBGattDescRef& d, const jau::TROOctets & value, const uint16_t value_offset) override {
            const bool match = matches(device);
            jau_fprintf_td(stderr, "****** GATT::writeDescValue(match %d): %s '%s' @ %u from %s\n  %s\n    %s\n      %s\n",
                    match, value, jau::dfa_utf8_decode( value.get_ptr(), value.size() ),
                    value_offset,
                    device->toString(), s->toString(), c->toString(), d->toString());
            return match;
        }
        void writeDescValueDone(const BTDeviceRef& device, const DBGattServiceRef& s, const DBGattCharRef& c, const DBGattDescRef& d) override {
            const bool match = matches(device);
            const jau::TROOctets& value = d->getValue();
            jau_fprintf_td(stderr, "****** GATT::writeDescValueDone(match %d): From %s\n  %s\n    %s\n      %s\n    Desc-Value: %s\n",
                    match, device->toString(), s->toString(), c->toString(), d->toString(), value);
        }

        void clientCharConfigChanged(const BTDeviceRef& device, const DBGattServiceRef& s, const DBGattCharRef& c, const DBGattDescRef& d, const bool notificationEnabled, const bool indicationEnabled) override {
            const bool match = matches(device);
            const jau::TROOctets& value = d->getValue();
            jau_fprintf_td(stderr, "****** GATT::clientCharConfigChanged(match %d): notify %d, indicate %d from %s\n  %s\n    %s\n      %s\n    Desc-Value: %s\n",
                    match, notificationEnabled, indicationEnabled,
                    device->toString(), s->toString(), c->toString(), d->toString(), value);

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

static const uint16_t adv_interval_min=160; // x0.625 = 100ms
static const uint16_t adv_interval_max=480; // x0.625 = 300ms
static const AD_PDU_Type adv_type=AD_PDU_Type::ADV_IND;
static const uint8_t adv_chan_map=0x07;
static const uint8_t filter_policy=0x00;

static bool startAdvertising(BTAdapter *a, std::string msg) { // NOLINT(performance-unnecessary-value-param): Pass-by-value out-of-thread
    if( useAdapter != EUI48::ALL_DEVICE && useAdapter != a->getAddressAndType().address ) {
        jau_fprintf_td(stderr, "****** Start advertising (%s): Adapter not selected: %s\n", msg, a->toString());
        return false;
    }

    {
        const LE_Features le_feats = a->getLEFeatures();
        jau_fprintf_td(stderr, "startAdvertising: LE_Features %s\n", le_feats);
    }
    if( a->getBTMajorVersion() > 4 ) {
        LE_PHYs Tx { LE_PHYs::LE_2M }, Rx { LE_PHYs::LE_2M };
        HCIStatusCode res = a->setDefaultLE_PHY(Tx, Rx);
        jau_fprintf_td(stderr, "startAdvertising: Set Default LE PHY: status %s: Tx %s, Rx %s\n", res, Tx, Rx);
    }

    a->setServerConnSecurity(adapter_sec_level, adapter_sec_io_cap);

    EInfoReport eir;
    EIRDataType adv_mask = EIRDataType::FLAGS | EIRDataType::SERVICE_UUID;
    EIRDataType scanrsp_mask = EIRDataType::NAME | EIRDataType::CONN_IVAL;

    eir.addFlags(GAPFlags::LE_Gen_Disc);
    eir.addFlags(GAPFlags::BREDR_UNSUP);

    eir.addService(DataServiceUUID);
    eir.setServicesComplete(false);

    eir.setName(a->getName());
    eir.setConnInterval(8, 12); // 10ms - 15ms

    DBGattCharRef gattDevNameChar = dbGattServer->findGattChar( jau::uuid16_t(GattServiceType::GENERIC_ACCESS),
                                                                jau::uuid16_t(GattCharacteristicType::DEVICE_NAME) );
    if( nullptr != gattDevNameChar ) {
        std::string aname = a->getName();
        gattDevNameChar->setValue(reinterpret_cast<uint8_t*>(aname.data()), aname.size(), 0);
    }

    jau_fprintf_td(stderr, "****** Start advertising (%s): EIR %s\n", msg, eir);
    jau_fprintf_td(stderr, "****** Start advertising (%s): adv %s, scanrsp %s\n", msg, adv_mask, scanrsp_mask);

    HCIStatusCode status = a->startAdvertising(dbGattServer, eir, adv_mask, scanrsp_mask,
                                               adv_interval_min, adv_interval_max,
                                               adv_type, adv_chan_map, filter_policy);
    jau_fprintf_td(stderr, "****** Start advertising (%s) result: %s: %s\n", msg, status, a->toString());
    jau_fprintf_td(stderr, "%s", dbGattServer->toFullString());
    return HCIStatusCode::SUCCESS == status;
}

static bool stopAdvertising(BTAdapter *a, std::string msg) { // NOLINT(performance-unnecessary-value-param): Pass-by-value out-of-thread
    if( useAdapter != EUI48::ALL_DEVICE && useAdapter != a->getAddressAndType().address ) {
        jau_fprintf_td(stderr, "****** Stop advertising (%s): Adapter not selected: %s\n", msg, a->toString());
        return false;
    }
    HCIStatusCode status = a->stopAdvertising();
    jau_fprintf_td(stderr, "****** Stop advertising (%s) result: %s: %s\n", msg, status, a->toString());
    return HCIStatusCode::SUCCESS == status;
}

static void processDisconnectedDevice(BTDeviceRef device) { // NOLINT(performance-unnecessary-value-param): Pass-by-value out-of-thread
    jau_fprintf_td(stderr, "****** Disconnected Device (count %zu): Start %s\n",
            servedConnections.load(), device->toString());

    // already unpaired
    stopAdvertising(&device->getAdapter(), "device-disconnected");
    device->remove();

    jau::sleep_for( 100_ms ); // wait a little (FIXME: Fast restart of advertising error)

    if( !RUN_ONLY_ONCE ) {
        startAdvertising(&device->getAdapter(), "device-disconnected");
    }

    jau_fprintf_td(stderr, "****** Disonnected Device: End %s\n", device->toString());
}

static bool initAdapter(std::shared_ptr<BTAdapter>& adapter) {
    if( useAdapter != EUI48::ALL_DEVICE && useAdapter != adapter->getAddressAndType().address ) {
        jau_fprintf_td(stderr, "initAdapter: Adapter not selected: %s\n", adapter->toString());
        return false;
    }
    if( !adapter->isInitialized() ) {
        // Initialize with defaults and power-on
        const HCIStatusCode status = adapter->initialize( btMode, false );
        if( HCIStatusCode::SUCCESS != status ) {
            jau_fprintf_td(stderr, "initAdapter: initialize failed: %s: %s\n",
                    status, adapter->toString());
            return false;
        }
    } else if( !adapter->setPowered( false ) ) {
        jau_fprintf_td(stderr, "initAdapter: setPower.1 off failed: %s\n", adapter->toString());
        return false;
    }
    // adapter is powered-off
    jau_fprintf_td(stderr, "initAdapter.1: %s\n", adapter->toString());

    {
        HCIStatusCode status = adapter->setName(adapter_name, adapter_short_name);
        if( HCIStatusCode::SUCCESS == status ) {
            jau_fprintf_td(stderr, "initAdapter: setLocalName OK: %s\n", adapter->toString());
        } else {
            jau_fprintf_td(stderr, "initAdapter: setLocalName failed: %s\n", adapter->toString());
            return false;
        }

        status = adapter->setSecureConnections( use_SC );
        if( HCIStatusCode::SUCCESS == status ) {
            jau_fprintf_td(stderr, "initAdapter: setSecureConnections OK: %s\n", adapter->toString());
        } else {
            jau_fprintf_td(stderr, "initAdapter: setSecureConnections failed: %s\n", adapter->toString());
            return false;
        }

        const uint16_t conn_min_interval = 8;  // 10ms
        const uint16_t conn_max_interval = 40; // 50ms
        const uint16_t conn_latency = 0;
        const uint16_t supervision_timeout = 50; // 500ms
        status = adapter->setDefaultConnParam(conn_min_interval, conn_max_interval, conn_latency, supervision_timeout);
        if( HCIStatusCode::SUCCESS == status ) {
            jau_fprintf_td(stderr, "initAdapter: setDefaultConnParam OK: %s\n", adapter->toString());
        } else if( HCIStatusCode::UNKNOWN_COMMAND == status ) {
            jau_fprintf_td(stderr, "initAdapter: setDefaultConnParam UNKNOWN_COMMAND (ignored): %s\n", adapter->toString());
        } else {
            jau_fprintf_td(stderr, "initAdapter: setDefaultConnParam failed: %s, %s\n", status, adapter->toString());
            return false;
        }

        if( !adapter->setPowered( true ) ) {
            jau_fprintf_td(stderr, "initAdapter: setPower.2 on failed: %s\n", adapter->toString());
            return false;
        }
    }
    jau_fprintf_td(stderr, "initAdapter.2: %s\n", adapter->toString());

    adapter->setSMPKeyPath(SERVER_KEY_PATH);

    std::shared_ptr<AdapterStatusListener> asl( std::make_shared<MyAdapterStatusListener>() );
    adapter->addStatusListener( asl );
    if( !startAdvertising(adapter.get(), "initAdapter") ) {
        adapter->removeStatusListener( asl );
        return false;
    }
    return true;
}

static void myChangedAdapterSetFunc(const bool added, std::shared_ptr<BTAdapter>& adapter) {
    if( added ) {
        if( nullptr == chosenAdapter ) {
            if( initAdapter( adapter ) ) {
                chosenAdapter = adapter;
                jau_fprintf_td(stderr, "****** Adapter ADDED__: InitOK: %s\n", adapter->toString());
            } else {
                jau_fprintf_td(stderr, "****** Adapter ADDED__: Ignored: %s\n", adapter->toString());
            }
            jau_fprintf_td(stderr, "****** Adapter Features: %s\n", adapter->getLEFeatures());
        } else {
            jau_fprintf_td(stderr, "****** Adapter ADDED__: Ignored (other): %s\n", adapter->toString());
        }
    } else {
        if( nullptr != chosenAdapter && adapter == chosenAdapter ) {
            chosenAdapter = nullptr;
            jau_fprintf_td(stderr, "****** Adapter REMOVED: %s\n", adapter->toString());
        } else {
            jau_fprintf_td(stderr, "****** Adapter REMOVED (other): %s\n", adapter->toString());
        }
    }
}

static void test() {
    const std::shared_ptr<BTManager>& mngr = BTManager::get();

    jau_fprintf_td(stderr, "****** Test Start\n");

    std::shared_ptr<MyGATTServerListener> listener = std::make_shared<MyGATTServerListener>();
    dbGattServer->addListener( listener );

    mngr->addChangedAdapterSetCallback(myChangedAdapterSetFunc);

    while( !RUN_ONLY_ONCE || 0 == servedConnections ) {
        jau::sleep_for( 2_s );
    }

    jau_fprintf_td(stderr, "****** Test Shutdown.01 (DBGattServer.remove-listener)\n");
    dbGattServer->removeListener( listener );

    jau_fprintf_td(stderr, "****** Test Shutdown.02 (listener.close)\n");
    listener->close();

    jau_fprintf_td(stderr, "****** Test Shutdown.03 (DBGattServer.close := nullptr)\n");
    dbGattServer = nullptr;

    chosenAdapter = nullptr;

    jau_fprintf_td(stderr, "****** Test End\n");
}

#include <cstdio>

int main(int argc, char *argv[])
{
    bool waitForEnter=false;

    jau_fprintf_td(stderr, "Direct-BT Native Version %s (API %s)\n", DIRECT_BT_VERSION, DIRECT_BT_VERSION_API);

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
            int v = 0;
            jau::fromIntString(v, argv[++i]);
            use_SC = 0 != v;
        } else if( !strcmp("-adapter", argv[i]) && argc > (i+1) ) {
            useAdapter = EUI48( std::string(argv[++i]) );
        } else if( !strcmp("-name", argv[i]) && argc > (i+1) ) {
            adapter_name = std::string(argv[++i]);
        } else if( !strcmp("-short_name", argv[i]) && argc > (i+1) ) {
            adapter_short_name = std::string(argv[++i]);
        } else if( !strcmp("-mtu", argv[i]) && argc > (i+1) ) {
            uint16_t v = 512+1;
            jau::fromIntString(v, argv[++i]);
            dbGattServer->setMaxAttMTU( v );
        } else if( !strcmp("-seclevel", argv[i]) && argc > (i+1) ) {
            jau::fromIntString(number_ref(adapter_sec_level), argv[++i]);
        } else if( !strcmp("-iocap", argv[i]) && argc > (i+1) ) {
            jau::fromIntString(number_ref(adapter_sec_io_cap), argv[++i]);
        } else if( !strcmp("-once", argv[i]) ) {
            RUN_ONLY_ONCE = true;
        }
    }
    jau_fprintf_td(stderr, "pid %d\n", getpid());

    jau_fprintf_td(stderr, "Run with '[-btmode LE|BREDR|DUAL] [-use_sc 0|1] "
                    "[-adapter <adapter_address>] "
                    "[-name <adapter_name>] "
                    "[-short_name <adapter_short_name>] "
                    "[-mtu <max att_mtu>] "
                    "[-seclevel <int_sec_level>]* "
                    "[-iocap <int_iocap>]* "
                    "[-once] "
                    "[-dbt_verbose true|false] "
                    "[-dbt_debug true|false|adapter.event,gatt.data,hci.event,hci.scan_ad_eir,mgmt.event] "
                    "[-dbt_mgmt cmd.timeout=3000,ringsize=64,...] "
                    "[-dbt_hci cmd.complete.timeout=10000,cmd.status.timeout=3000,ringsize=64,...] "
                    "[-dbt_gatt cmd.read.timeout=500,cmd.write.timeout=500,cmd.init.timeout=2500,ringsize=128,...] "
                    "[-dbt_l2cap reader.timeout=10000,restart.count=0,...] "
                    "\n");

    jau_fprintf_td(stderr, "SHOW_UPDATE_EVENTS %d\n", SHOW_UPDATE_EVENTS);
    jau_fprintf_td(stderr, "adapter %s\n", useAdapter);
    jau_fprintf_td(stderr, "adapter btmode %s\n", btMode);
    jau_fprintf_td(stderr, "adapter SC %s\n", use_SC);
    jau_fprintf_td(stderr, "adapter name %s (short %s)\n", adapter_name, adapter_short_name);
    jau_fprintf_td(stderr, "adapter mtu %d\n", (int)dbGattServer->getMaxAttMTU());
    jau_fprintf_td(stderr, "adapter sec_level %s\n", adapter_sec_level);
    jau_fprintf_td(stderr, "adapter io_cap %s\n", adapter_sec_io_cap);
    jau_fprintf_td(stderr, "once %d\n", (int)RUN_ONLY_ONCE);
    jau_fprintf_td(stderr, "GattServer %s\n", dbGattServer->toString());
    jau_fprintf_td(stderr, "GattServer.services: %s\n", dbGattServer->getServices().getInfo());
    jau_fprintf_td(stderr, "GattService.characteristics: %s\n", dbGattServer->getServices()[0]->getCharacteristics().getInfo());

    if( waitForEnter ) {
        jau_fprintf_td(stderr, "Press ENTER to continue\n");
        getchar();
    }
    jau_fprintf_td(stderr, "****** TEST start\n");
    try {
        test();
    } catch (...) {
        jau::fput_exception(stderr, std::current_exception(), E_FILE_LINE);
        jau_ERR_PRINT3("Exception caught while testing");
    }
    jau_fprintf_td(stderr, "****** TEST end\n");
}
