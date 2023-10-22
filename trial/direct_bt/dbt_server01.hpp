/**
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2022 Gothel Software e.K.
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

#ifndef DBT_SERVER01_HPP_
#define DBT_SERVER01_HPP_

#include <random>

#include "dbt_constants.hpp"

#include "dbt_server_test.hpp"

#include <jau/latch.hpp>

class DBTServer01;
typedef std::shared_ptr<DBTServer01> DBTServer01Ref;

using namespace jau;

/**
 * This peripheral BTRole::Slave test participant works with DBTClient00.
 */
class DBTServer01 : public DBTServerTest {
    private:
        static jau::POctets make_poctets(const char* name) {
            return jau::POctets( (const uint8_t*)name, (nsize_t)strlen(name), endian::little );
        }
        static jau::POctets make_poctets(const char* name, const jau::nsize_t capacity) {
            const nsize_t name_len = (nsize_t)strlen(name);
            jau::POctets p( std::max<nsize_t>(capacity, name_len), name_len, endian::little );
            p.bzero();
            p.put_bytes_nc(0, reinterpret_cast<const uint8_t*>(name), name_len);
            return p;
        }
        static jau::POctets make_poctets(const uint16_t v) {
            jau::POctets p(2, endian::little);
            p.put_uint16_nc(0, v);
            return p;
        }
        static jau::POctets make_poctets(const jau::nsize_t capacity, const jau::nsize_t size) {
            jau::POctets p(capacity, size, endian::little);
            p.bzero();
            return p;
        }

        static const bool GATT_VERBOSE = false;
        static const bool SHOW_UPDATE_EVENTS = false;

        const std::string adapterShortName = "TDev1Srv";
        std::string adapterName = "TestDev1_Srv";
        jau::EUI48 useAdapter = jau::EUI48::ALL_DEVICE;
        BTMode btMode = BTMode::DUAL;
        bool use_SC = true;
        BTSecurityLevel adapterSecurityLevel = BTSecurityLevel::UNSET;

        jau::latch running_threads = jau::latch(0);
        jau::sc_atomic_int disconnectCount = 0;
        jau::sc_atomic_int servedProtocolSessionsTotal = 0;
        jau::sc_atomic_int servedProtocolSessionsSuccess = 0;
        jau::sc_atomic_int servingProtocolSessionsLeft = 1;

        bool do_disconnect_randomly = false;

        DBGattServerRef dbGattServer = std::make_shared<DBGattServer>(
                /* services: */
                jau::make_darray( // DBGattService
                  std::make_shared<DBGattService> ( true /* primary */,
                      std::make_unique<const jau::uuid16_t>(GattServiceType::GENERIC_ACCESS) /* type_ */,
                      jau::make_darray ( // DBGattChar
                          std::make_shared<DBGattChar>( std::make_unique<const jau::uuid16_t>(GattCharacteristicType::DEVICE_NAME) /* value_type_ */,
                                      BTGattChar::PropertyBitVal::Read,
                                      jau::darray<DBGattDescRef>() /* intentionally empty */,
                                      make_poctets(adapterName.c_str(), 128) /* value */, true /* variable_length */ ),
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
                      std::make_unique<const jau::uuid128_t>(DBTConstants::DataServiceUUID) /* type_ */,
                      jau::make_darray ( // DBGattChar
                          std::make_shared<DBGattChar>( std::make_unique<const jau::uuid128_t>(DBTConstants::StaticDataUUID) /* value_type_ */,
                                      BTGattChar::PropertyBitVal::Read,
                                      jau::make_darray ( // DBGattDesc
                                          std::make_shared<DBGattDesc>( BTGattDesc::TYPE_USER_DESC, make_poctets("DATA_STATIC") )
                                      ),
                                    make_poctets("Proprietary Static Data 0x00010203") /* value */ ),
                          std::make_shared<DBGattChar>( std::make_unique<const jau::uuid128_t>(DBTConstants::CommandUUID) /* value_type_ */,
                                      BTGattChar::PropertyBitVal::WriteNoAck | BTGattChar::PropertyBitVal::WriteWithAck,
                                      jau::make_darray ( // DBGattDesc
                                          std::make_shared<DBGattDesc>( BTGattDesc::TYPE_USER_DESC, make_poctets("COMMAND") )
                                      ),
                                      make_poctets(128, 64) /* value */, true /* variable_length */ ),
                          std::make_shared<DBGattChar>( std::make_unique<const jau::uuid128_t>(DBTConstants::ResponseUUID) /* value_type_ */,
                                      BTGattChar::PropertyBitVal::Notify | BTGattChar::PropertyBitVal::Indicate,
                                      jau::make_darray ( // DBGattDesc
                                          std::make_shared<DBGattDesc>( BTGattDesc::TYPE_USER_DESC, make_poctets("RESPONSE") ),
                                          DBGattDesc::createClientCharConfig()
                                      ),
                                      make_poctets((uint16_t)0) /* value */ ),
                          std::make_shared<DBGattChar>( std::make_unique<const jau::uuid128_t>(DBTConstants::PulseDataUUID) /* value_type_ */,
                                      BTGattChar::PropertyBitVal::Notify | BTGattChar::PropertyBitVal::Indicate,
                                      jau::make_darray ( // DBGattDesc
                                          std::make_shared<DBGattDesc>( BTGattDesc::TYPE_USER_DESC, make_poctets("DATA_PULSE") ),
                                          DBGattDesc::createClientCharConfig()
                                      ),
                                      make_poctets("Synthethic Sensor 01") /* value */ )
                      ) )
                ) );


        std::mutex mtx_sync;
        BTDeviceRef connectedDevice;

        void setDevice(const BTDeviceRef& cd) {
            const std::lock_guard<std::mutex> lock(mtx_sync); // RAII-style acquire and relinquish via destructor
            connectedDevice = cd;
        }

        BTDeviceRef getDevice() {
            const std::lock_guard<std::mutex> lock(mtx_sync); // RAII-style acquire and relinquish via destructor
            return connectedDevice;
        }

        bool matches(const BTDeviceRef& device) {
            const BTDeviceRef d = getDevice();
            return nullptr != d ? (*d) == *device : false;
        }

        class MyAdapterStatusListener : public AdapterStatusListener {
          public:
            DBTServer01& parent;

            MyAdapterStatusListener(DBTServer01& p) : parent(p) {}

            void adapterSettingsChanged(BTAdapter &a, const AdapterSetting oldmask, const AdapterSetting newmask,
                                        const AdapterSetting changedmask, const uint64_t timestamp) override {
                const bool initialSetting = AdapterSetting::NONE == oldmask;
                if( initialSetting ) {
                    fprintf_td(stderr, "****** Server SETTINGS_INITIAL: %s -> %s, changed %s\n", to_string(oldmask).c_str(),
                            to_string(newmask).c_str(), to_string(changedmask).c_str());
                } else {
                    fprintf_td(stderr, "****** Server SETTINGS_CHANGED: %s -> %s, changed %s\n", to_string(oldmask).c_str(),
                            to_string(newmask).c_str(), to_string(changedmask).c_str());

                    const bool justPoweredOn = isAdapterSettingBitSet(changedmask, AdapterSetting::POWERED) &&
                                               isAdapterSettingBitSet(newmask, AdapterSetting::POWERED);
                    if( justPoweredOn && *parent.serverAdapter == a ) {
                        parent.startAdvertising("powered_on");
                    }
                }
                fprintf_td(stderr, "Server Status BTAdapter:\n");
                fprintf_td(stderr, "%s\n", a.toString().c_str());
                (void)timestamp;
            }

            void discoveringChanged(BTAdapter &a, const ScanType currentMeta, const ScanType changedType, const bool changedEnabled, const DiscoveryPolicy policy, const uint64_t timestamp) override {
                fprintf_td(stderr, "****** Server DISCOVERING: meta %s, changed[%s, enabled %d, policy %s]: %s\n",
                        to_string(currentMeta).c_str(), to_string(changedType).c_str(), changedEnabled, to_string(policy).c_str(), a.toString().c_str());
                (void)timestamp;
            }

            bool deviceFound(const BTDeviceRef& device, const uint64_t timestamp) override {
                (void)timestamp;

                fprintf_td(stderr, "****** Server FOUND__-1: NOP %s\n", device->toString(true).c_str());
                return false;
            }

            void deviceUpdated(const BTDeviceRef& device, const EIRDataType updateMask, const uint64_t timestamp) override {
                if( SHOW_UPDATE_EVENTS ) {
                    fprintf_td(stderr, "****** Server UPDATED: %s of %s\n", to_string(updateMask).c_str(), device->toString(true).c_str());
                }
                (void)timestamp;
            }

            void deviceConnected(const BTDeviceRef& device, const bool discovered, const uint64_t timestamp) override {
                fprintf_td(stderr, "****** Server CONNECTED (discovered %d): %s\n", discovered, device->toString(true).c_str());
                const bool available = nullptr == parent.getDevice();
                if( available ) {
                    parent.setDevice(device);
                }
                (void)discovered;
                (void)timestamp;
            }

            void devicePairingState(const BTDeviceRef& device, const SMPPairingState state, const PairingMode mode, const uint64_t timestamp) override {
                fprintf_td(stderr, "****** Server PAIRING STATE: state %s, mode %s, %s\n",
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

            void deviceReady(const BTDeviceRef& device, const uint64_t timestamp) override {
                (void)timestamp;
                fprintf_td(stderr, "****** Server READY-1: NOP %s\n", device->toString(true).c_str());
            }

            void deviceDisconnected(const BTDeviceRef& device, const HCIStatusCode reason, const uint16_t handle, const uint64_t timestamp) override {
                fprintf_td(stderr, "****** Server DISCONNECTED (count %zu): Reason 0x%X (%s), old handle %s: %s\n",
                        1+parent.disconnectCount.load(), static_cast<uint8_t>(reason), to_string(reason).c_str(),
                        to_hexstring(handle).c_str(), device->toString(true).c_str());

                const bool match = parent.matches(device);
                if( match ) {
                    parent.setDevice(nullptr);
                }
                parent.running_threads.count_up();
                std::thread sd(&DBTServer01::processDisconnectedDevice, &parent, device); // @suppress("Invalid arguments")
                sd.detach();
                (void)timestamp;
            }

            std::string toString() const noexcept override {
                return "Server MyAdapterStatusListener[this "+to_hexstring(this)+"]";
            }

        };

        class MyGATTServerListener : public DBGattServer::Listener {
            private:
                DBTServer01& parent;
                jau::service_runner pulse_service;

                jau::sc_atomic_uint16 handlePulseDataNotify = 0;
                jau::sc_atomic_uint16 handlePulseDataIndicate = 0;
                jau::sc_atomic_uint16 handleResponseDataNotify = 0;
                jau::sc_atomic_uint16 handleResponseDataIndicate = 0;

                uint16_t usedMTU = BTGattHandler::number(BTGattHandler::Defaults::MIN_ATT_MTU);

                void pulse_worker_init(jau::service_runner& sr) noexcept {
                    (void)sr;
                    const BTDeviceRef connectedDevice_ = parent.getDevice();
                    const std::string connectedDeviceStr = nullptr != connectedDevice_ ? connectedDevice_->toString() : "n/a";
                    fprintf_td(stderr, "****** Server GATT::PULSE Start %s\n", connectedDeviceStr.c_str());
                }
                void pulse_worker(jau::service_runner& sr) noexcept {
                    BTDeviceRef connectedDevice_ = parent.getDevice();
                    if( nullptr != connectedDevice_ && connectedDevice_->getConnected() ) {
                        if( 0 != handlePulseDataNotify || 0 != handlePulseDataIndicate ) {
                            std::string data( "Dynamic Data Example. Elapsed Milliseconds: "+jau::to_decstring(environment::getElapsedMillisecond(), ',', 9) );
                            jau::POctets v(data.size()+1, jau::endian::little);
                            v.put_string_nc(0, data, v.size(), true /* includeEOS */);
                            if( 0 != handlePulseDataNotify ) {
                                if( GATT_VERBOSE ) {
                                    fprintf_td(stderr, "****** Server GATT::sendNotification: PULSE to %s\n", connectedDevice_->toString().c_str());
                                }
                                connectedDevice_->sendNotification(handlePulseDataNotify, v);
                            }
                            if( 0 != handlePulseDataIndicate ) {
                                if( GATT_VERBOSE ) {
                                    fprintf_td(stderr, "****** Server GATT::sendIndication: PULSE to %s\n", connectedDevice_->toString().c_str());
                                }
                                connectedDevice_->sendIndication(handlePulseDataIndicate, v);
                            }
                        }
                    }
                    if( !sr.shall_stop() ) {
                        jau::sleep_for( 100_ms );
                    }
                }
                void pulse_worker_end(jau::service_runner& sr) noexcept {
                    (void)sr;
                    const BTDeviceRef connectedDevice_ = parent.getDevice();
                    const std::string connectedDeviceStr = nullptr != connectedDevice_ ? connectedDevice_->toString() : "n/a";
                    fprintf_td(stderr, "****** Server GATT::PULSE End %s\n", connectedDeviceStr.c_str());
                }

                void sendResponse(jau::POctets data) { // NOLINT(performance-unnecessary-value-param): Pass-by-value out-of-thread
                    BTDeviceRef connectedDevice_ = parent.getDevice();
                    if( nullptr != connectedDevice_ && connectedDevice_->getConnected() ) {
                        if( 0 != handleResponseDataNotify || 0 != handleResponseDataIndicate ) {
                            if( 0 != handleResponseDataNotify ) {
                                if( GATT_VERBOSE ) {
                                    fprintf_td(stderr, "****** GATT::sendNotification: %s to %s\n",
                                            data.toString().c_str(), connectedDevice_->toString().c_str());
                                }
                                connectedDevice_->sendNotification(handleResponseDataNotify, data);
                            }
                            if( 0 != handleResponseDataIndicate ) {
                                if( GATT_VERBOSE ) {
                                    fprintf_td(stderr, "****** GATT::sendIndication: %s to %s\n",
                                            data.toString().c_str(), connectedDevice_->toString().c_str());
                                }
                                connectedDevice_->sendIndication(handleResponseDataIndicate, data);
                            }
                        }
                    }
                    parent.running_threads.count_down();
                }

                void disconnectDeviceRandomly() {
                    // sleep range: 100 - 1500 ms
                    // sleep range: 100 - 1500 ms
                    static const int sleep_min = 100;
                    static const int sleep_max = 1500;
                    static std::random_device rng_hw;;
                    static std::uniform_int_distribution<int> rng_dist(sleep_min, sleep_max);
                    const int64_t sleep_dur = rng_dist(rng_hw);
                    jau::sleep_for( sleep_dur * 1_ms );
                    BTDeviceRef connectedDevice_ = parent.getDevice();
                    if( nullptr != connectedDevice_ ) {
                        fprintf_td(stderr, "****** Server i470 disconnectDevice(delayed %d ms): client %s\n", sleep_dur, connectedDevice_->toString().c_str());
                        connectedDevice_->disconnect();
                    } else {
                        fprintf_td(stderr, "****** Server i470 disconnectDevice(delayed %d ms): client null\n", sleep_dur);
                    }
                    parent.running_threads.count_down();
                }

            public:

                MyGATTServerListener(DBTServer01& p)
                : parent(p),
                  pulse_service("MyGATTServerListener::pulse", THREAD_SHUTDOWN_TIMEOUT_MS,
                                       jau::bind_member(this, &MyGATTServerListener::pulse_worker),
                                       jau::bind_member(this, &MyGATTServerListener::pulse_worker_init),
                                       jau::bind_member(this, &MyGATTServerListener::pulse_worker_end))
                {
                    pulse_service.start();
                }

                ~MyGATTServerListener() noexcept override {
                    pulse_service.stop();
                }

                void clear() {
                    const std::lock_guard<std::mutex> lock(parent.mtx_sync); // RAII-style acquire and relinquish via destructor

                    handlePulseDataNotify = 0;
                    handlePulseDataIndicate = 0;
                    handleResponseDataNotify = 0;
                    handleResponseDataIndicate = 0;

                    parent.dbGattServer->resetGattClientCharConfig(DBTConstants::DataServiceUUID, DBTConstants::PulseDataUUID);
                    parent.dbGattServer->resetGattClientCharConfig(DBTConstants::DataServiceUUID, DBTConstants::ResponseUUID);
                }

                void close() noexcept {
                    pulse_service.stop();
                    clear();
                }

                void connected(const BTDeviceRef& device, const uint16_t initialMTU) override {
                    const bool match = parent.matches(device);
                    fprintf_td(stderr, "****** Server GATT::connected(match %d): initMTU %d, %s\n",
                            match, (int)initialMTU, device->toString().c_str());
                    if( match ) {
                        const std::lock_guard<std::mutex> lock(parent.mtx_sync); // RAII-style acquire and relinquish via destructor
                        usedMTU = initialMTU;
                    }
                }

                void disconnected(const BTDeviceRef& device) override {
                    const bool match = parent.matches(device);
                    fprintf_td(stderr, "****** Server GATT::disconnected(match %d): %s\n", match, device->toString().c_str());
                    if( match ) {
                        clear();
                    }
                }

                void mtuChanged(const BTDeviceRef& device, const uint16_t mtu) override {
                    const bool match = parent.matches(device);
                    const uint16_t usedMTU_old = usedMTU;
                    if( match ) {
                        const std::lock_guard<std::mutex> lock(parent.mtx_sync); // RAII-style acquire and relinquish via destructor
                        usedMTU = mtu;
                    }
                    fprintf_td(stderr, "****** Server GATT::mtuChanged(match %d, served %zu, left %zu): %d -> %d, %s\n",
                            match, parent.servedProtocolSessionsTotal.load(), parent.servingProtocolSessionsLeft.load(),
                            match ? (int)usedMTU_old : 0, (int)mtu, device->toString().c_str());
                    if( parent.do_disconnect_randomly ) {
                        parent.running_threads.count_up();
                        std::thread disconnectThread(&MyGATTServerListener::disconnectDeviceRandomly, this);
                        disconnectThread.detach();
                    }
                }

                bool readCharValue(const BTDeviceRef& device, const DBGattServiceRef& s, const DBGattCharRef& c) override {
                    const bool match = parent.matches(device);
                    if( GATT_VERBOSE ) {
                        fprintf_td(stderr, "****** Server GATT::readCharValue(match %d): to %s, from\n  %s\n    %s\n",
                                match, device->toString().c_str(), s->toString().c_str(), c->toString().c_str());
                    }
                    return match;
                }

                bool readDescValue(const BTDeviceRef& device, const DBGattServiceRef& s, const DBGattCharRef& c, const DBGattDescRef& d) override {
                    const bool match = parent.matches(device);
                    if( GATT_VERBOSE ) {
                        fprintf_td(stderr, "****** Server GATT::readDescValue(match %d): to %s, from\n  %s\n    %s\n      %s\n",
                                match, device->toString().c_str(), s->toString().c_str(), c->toString().c_str(), d->toString().c_str());
                    }
                    return match;
                }

                bool writeCharValue(const BTDeviceRef& device, const DBGattServiceRef& s, const DBGattCharRef& c, const jau::TROOctets & value, const uint16_t value_offset) override {
                    const bool match = parent.matches(device);
                    if( GATT_VERBOSE ) {
                        fprintf_td(stderr, "****** Server GATT::writeCharValue(match %d): %s '%s' @ %u from %s, to\n  %s\n    %s\n",
                                match, value.toString().c_str(), jau::dfa_utf8_decode( value.get_ptr(), value.size() ).c_str(),
                                value_offset,
                                device->toString().c_str(), s->toString().c_str(), c->toString().c_str());
                    }
                    return match;
                }

                void writeCharValueDone(const BTDeviceRef& device, const DBGattServiceRef& s, const DBGattCharRef& c) override {
                    const bool match = parent.matches(device);
                    const jau::TROOctets& value = c->getValue();
                    bool isFinalHandshake = false;
                    bool isFinalHandshakeSuccess = false;

                    if( match &&
                        c->getValueType()->equivalent( DBTConstants::CommandUUID ) &&
                        ( 0 != handleResponseDataNotify || 0 != handleResponseDataIndicate ) )
                    {
                        isFinalHandshakeSuccess = DBTConstants::SuccessHandshakeCommandData.size() == value.size() &&
                                                  0 == ::memcmp(DBTConstants::SuccessHandshakeCommandData.data(), value.get_ptr(), value.size());
                        isFinalHandshake = isFinalHandshakeSuccess ||
                                           ( DBTConstants::FailHandshakeCommandData.size() == value.size() &&
                                             0 == ::memcmp(DBTConstants::FailHandshakeCommandData.data(), value.get_ptr(), value.size()) );

                        if( isFinalHandshake ) {
                            if( isFinalHandshakeSuccess ) {
                                parent.servedProtocolSessionsSuccess++;
                            }
                            parent.servedProtocolSessionsTotal++;
                            if( parent.servingProtocolSessionsLeft > 0 ) {
                                parent.servingProtocolSessionsLeft--;
                            }
                        }
                        parent.running_threads.count_up();
                        jau::POctets value2(value);
                        std::thread senderThread(&MyGATTServerListener::sendResponse, this, value2);
                        senderThread.detach();
                    }
                    if( GATT_VERBOSE || isFinalHandshake ) {
                        fprintf_td(stderr, "****** Server GATT::writeCharValueDone(match %d, finalCmd %d, sessions [%d ok / %d total], left %d): From %s, to\n  %s\n    %s\n    Char-Value: %s\n",
                                match, isFinalHandshake, parent.servedProtocolSessionsSuccess.load(), parent.servedProtocolSessionsTotal.load(), parent.servingProtocolSessionsLeft.load(),
                                device->toString().c_str(), s->toString().c_str(), c->toString().c_str(), value.toString().c_str());
                    }
                }

                bool writeDescValue(const BTDeviceRef& device, const DBGattServiceRef& s, const DBGattCharRef& c, const DBGattDescRef& d, const jau::TROOctets & value, const uint16_t value_offset) override {
                    const bool match = parent.matches(device);
                    if( GATT_VERBOSE ) {
                        fprintf_td(stderr, "****** Server GATT::writeDescValue(match %d): %s '%s' @ %u from %s\n  %s\n    %s\n      %s\n",
                                match, value.toString().c_str(), jau::dfa_utf8_decode( value.get_ptr(), value.size() ).c_str(),
                                value_offset,
                                device->toString().c_str(), s->toString().c_str(), c->toString().c_str(), d->toString().c_str());
                    }
                    return match;
                }
                void writeDescValueDone(const BTDeviceRef& device, const DBGattServiceRef& s, const DBGattCharRef& c, const DBGattDescRef& d) override {
                    if( GATT_VERBOSE ) {
                        const bool match = parent.matches(device);
                        const jau::TROOctets& value = d->getValue();
                        fprintf_td(stderr, "****** Server GATT::writeDescValueDone(match %d): From %s\n  %s\n    %s\n      %s\n    Desc-Value: %s\n",
                                match, device->toString().c_str(), s->toString().c_str(), c->toString().c_str(), d->toString().c_str(), value.toString().c_str());
                    }
                }

                void clientCharConfigChanged(const BTDeviceRef& device, const DBGattServiceRef& s, const DBGattCharRef& c, const DBGattDescRef& d, const bool notificationEnabled, const bool indicationEnabled) override {
                    const bool match = parent.matches(device);
                    if( GATT_VERBOSE ) {
                        const jau::TROOctets& value = d->getValue();
                        fprintf_td(stderr, "****** GATT::clientCharConfigChanged(match %d): notify %d, indicate %d from %s\n  %s\n    %s\n      %s\n    Desc-Value: %s\n",
                                match, notificationEnabled, indicationEnabled,
                                device->toString().c_str(), s->toString().c_str(), c->toString().c_str(), d->toString().c_str(), value.toString().c_str());
                    }
                    if( match ) {
                        if( c->getValueType()->equivalent( DBTConstants::PulseDataUUID ) ) {
                            const std::lock_guard<std::mutex> lock(parent.mtx_sync); // RAII-style acquire and relinquish via destructor
                            handlePulseDataNotify = notificationEnabled ? c->getValueHandle() : 0;
                            handlePulseDataIndicate = indicationEnabled ? c->getValueHandle() : 0;
                        } else if( c->getValueType()->equivalent( DBTConstants::ResponseUUID ) ) {
                            const std::lock_guard<std::mutex> lock(parent.mtx_sync); // RAII-style acquire and relinquish via destructor
                            handleResponseDataNotify = notificationEnabled ? c->getValueHandle() : 0;
                            handleResponseDataIndicate = indicationEnabled ? c->getValueHandle() : 0;
                        }
                    }
                }
        };

        std::shared_ptr<MyGATTServerListener> gattServerListener = std::make_shared<MyGATTServerListener>(*this);
        std::shared_ptr<AdapterStatusListener> myAdapterStatusListener = std::make_shared<MyAdapterStatusListener>(*this);

        BTAdapterRef serverAdapter = nullptr;

    public:

        DBTServer01(const std::string& adapterName_, const jau::EUI48& useAdapter_, const BTMode btMode_,
                    const bool use_SC_, const BTSecurityLevel adapterSecurityLevel_, const bool do_disconnect_randomly_=false)
        {
            this->adapterName = adapterName_;
            this->useAdapter = useAdapter_;
            this->btMode = btMode_;
            this->use_SC = use_SC_;
            this->adapterSecurityLevel = adapterSecurityLevel_;
            this->do_disconnect_randomly = do_disconnect_randomly_;

            dbGattServer->addListener( gattServerListener );
        }

        ~DBTServer01() override {
            fprintf_td(stderr, "****** Server dtor: running_threads %zu\n", running_threads.value());
            running_threads.wait_for( 10_s );
        }

        std::string getName() override { return adapterName; }

        BTSecurityLevel getSecurityLevel() override { return adapterSecurityLevel; }

        void setAdapter(BTAdapterRef serverAdapter_) override {
            this->serverAdapter = serverAdapter_;
        }

        BTAdapterRef getAdapter() override { return serverAdapter; }

    private:
        static const uint16_t adv_interval_min=160; // x0.625 = 100ms
        static const uint16_t adv_interval_max=480; // x0.625 = 300ms
        static const AD_PDU_Type adv_type=AD_PDU_Type::ADV_IND;
        static const uint8_t adv_chan_map=0x07;
        static const uint8_t filter_policy=0x00;

    public:

        void close(const std::string& msg) override {
            fprintf_td(stderr, "****** Server Close.0: %s\n", msg.c_str());
            REQUIRE( true == serverAdapter->removeStatusListener( myAdapterStatusListener ) );
            {
                stopAdvertising(msg);
                BTDeviceRef connectedDevice_ = getDevice();
                if( nullptr != connectedDevice_ ) {
                    setDevice(nullptr);
                    connectedDevice_->disconnect();
                }
            }
            gattServerListener->close();
            fprintf_td(stderr, "****** Server close: running_threads %zu\n", running_threads.value());
            running_threads.wait_for( 10_s );

            // dbGattServer = nullptr; // keep alive
            stopAdvertising(msg); // try once more in case of already started AdapterStatusListener
            fprintf_td(stderr, "****** Server Close.X: %s\n", msg.c_str());
        }

        void setProtocolSessionsLeft(const int v) override {
            servingProtocolSessionsLeft = v;
        }
        int getProtocolSessionsLeft() override {
            return servingProtocolSessionsLeft;
        }
        int getProtocolSessionsDoneTotal() override {
            return servedProtocolSessionsTotal;
        }
        int getProtocolSessionsDoneSuccess() override {
            return servedProtocolSessionsSuccess;
        }
        int getDisconnectCount() override {
            return disconnectCount;
        }

    private:
        HCIStatusCode stopAdvertising(const std::string& msg) {
            HCIStatusCode status = serverAdapter->stopAdvertising();
            fprintf_td(stderr, "****** Server Stop advertising (%s) result: %s: %s\n", msg.c_str(), to_string(status).c_str(), serverAdapter->toString().c_str());
            return status;
        }

    public:

        HCIStatusCode startAdvertising(const std::string& msg) override {
            EInfoReport eir;
            EIRDataType adv_mask = EIRDataType::FLAGS | EIRDataType::SERVICE_UUID;
            EIRDataType scanrsp_mask = EIRDataType::NAME | EIRDataType::CONN_IVAL;

            eir.addFlags(GAPFlags::LE_Gen_Disc);
            eir.addFlags(GAPFlags::BREDR_UNSUP);

            eir.addService(DBTConstants::DataServiceUUID);
            eir.setServicesComplete(false);

            eir.setName(serverAdapter->getName());
            eir.setConnInterval(8, 12); // 10ms - 15ms

            DBGattCharRef gattDevNameChar = dbGattServer->findGattChar( jau::uuid16_t(GattServiceType::GENERIC_ACCESS),
                                                                        jau::uuid16_t(GattCharacteristicType::DEVICE_NAME) );
            if( nullptr != gattDevNameChar ) {
                std::string aname = serverAdapter->getName();
                gattDevNameChar->setValue(reinterpret_cast<uint8_t*>(aname.data()), aname.size(), 0);
            }

            fprintf_td(stderr, "****** Start advertising (%s): EIR %s\n", msg.c_str(), eir.toString().c_str());
            fprintf_td(stderr, "****** Start advertising (%s): adv %s, scanrsp %s\n", msg.c_str(), to_string(adv_mask).c_str(), to_string(scanrsp_mask).c_str());

            HCIStatusCode status = serverAdapter->startAdvertising(dbGattServer, eir, adv_mask, scanrsp_mask,
                                                       adv_interval_min, adv_interval_max,
                                                       adv_type, adv_chan_map, filter_policy);
            fprintf_td(stderr, "****** Server Start advertising (%s) result: %s: %s\n", msg.c_str(), to_string(status).c_str(), serverAdapter->toString().c_str());
            if( GATT_VERBOSE ) {
                fprintf_td(stderr, "%s", dbGattServer->toFullString().c_str());
            }
            return status;
        }

    private:
        void processDisconnectedDevice(BTDeviceRef device) { // NOLINT(performance-unnecessary-value-param): Pass-by-value out-of-thread
            fprintf_td(stderr, "****** Server Disconnected Device (count %zu, served %zu, left %zu): Start %s\n",
                    1+disconnectCount.load(), servedProtocolSessionsTotal.load(), servingProtocolSessionsLeft.load(), device->toString().c_str());

            // already unpaired
            stopAdvertising("device-disconnected");
            device->remove();

            disconnectCount++;

            jau::sleep_for( 100_ms ); // wait a little (FIXME: Fast restart of advertising error)

            if( servingProtocolSessionsLeft  > 0 ) {
                startAdvertising("device-disconnected");
            }

            fprintf_td(stderr, "****** Server Disonnected Device: End %s\n", device->toString().c_str());
            running_threads.count_down();
        }

    public:

        bool initAdapter(BTAdapterRef adapter) override {
            if( useAdapter != EUI48::ALL_DEVICE && useAdapter != adapter->getAddressAndType().address ) {
                fprintf_td(stderr, "initServerAdapter: Adapter not selected: %s\n", adapter->toString().c_str());
                return false;
            }
            adapterName = adapterName + "-" + adapter->getAddressAndType().address.toString();
            {
                auto it = std::remove( adapterName.begin(), adapterName.end(), ':');
                adapterName.erase(it, adapterName.end());
            }

            if( !adapter->isInitialized() ) {
                // Initialize with defaults and power-on
                const HCIStatusCode status = adapter->initialize( btMode, false );
                if( HCIStatusCode::SUCCESS != status ) {
                    fprintf_td(stderr, "initServerAdapter: initialize failed: %s: %s\n",
                            to_string(status).c_str(), adapter->toString().c_str());
                    return false;
                }
            } else if( !adapter->setPowered( false ) ) {
                fprintf_td(stderr, "initServerAdapter: setPower.1 off failed: %s\n", adapter->toString().c_str());
                return false;
            }
            // adapter is powered-off
            fprintf_td(stderr, "initServerAdapter.1: %s\n", adapter->toString().c_str());

            {
                HCIStatusCode status = adapter->setName(adapterName, adapterShortName);
                if( HCIStatusCode::SUCCESS == status ) {
                    fprintf_td(stderr, "initServerAdapter: setLocalName OK: %s\n", adapter->toString().c_str());
                } else {
                    fprintf_td(stderr, "initServerAdapter: setLocalName failed: %s\n", adapter->toString().c_str());
                    return false;
                }

                status = adapter->setSecureConnections( use_SC );
                if( HCIStatusCode::SUCCESS == status ) {
                    fprintf_td(stderr, "initServerAdapter: setSecureConnections OK: %s\n", adapter->toString().c_str());
                } else {
                    fprintf_td(stderr, "initServerAdapter: setSecureConnections failed: %s\n", adapter->toString().c_str());
                    return false;
                }

                const uint16_t conn_min_interval = 8;  // 10ms
                const uint16_t conn_max_interval = 40; // 50ms
                const uint16_t conn_latency = 0;
                const uint16_t supervision_timeout = 50; // 500ms
                status = adapter->setDefaultConnParam(conn_min_interval, conn_max_interval, conn_latency, supervision_timeout);
                if( HCIStatusCode::SUCCESS == status ) {
                    fprintf_td(stderr, "initServerAdapter: setDefaultConnParam OK: %s\n", adapter->toString().c_str());
                } else if( HCIStatusCode::UNKNOWN_COMMAND == status ) {
                    fprintf_td(stderr, "initServerAdapter: setDefaultConnParam UNKNOWN_COMMAND (ignored): %s\n", adapter->toString().c_str());
                } else {
                    fprintf_td(stderr, "initServerAdapter: setDefaultConnParam failed: %s, %s\n", to_string(status).c_str(), adapter->toString().c_str());
                    return false;
                }

                if( !adapter->setPowered( true ) ) {
                    fprintf_td(stderr, "initServerAdapter: setPower.2 on failed: %s\n", adapter->toString().c_str());
                    return false;
                }
            }
            // adapter is powered-on
            fprintf_td(stderr, "initServerAdapter.2: %s\n", adapter->toString().c_str());

            {
                const LE_Features le_feats = adapter->getLEFeatures();
                fprintf_td(stderr, "initServerAdapter: LE_Features %s\n", to_string(le_feats).c_str());
            }
            if( adapter->getBTMajorVersion() > 4 ) {
                LE_PHYs Tx { LE_PHYs::LE_2M }, Rx { LE_PHYs::LE_2M };
                HCIStatusCode res = adapter->setDefaultLE_PHY(Tx, Rx);
                fprintf_td(stderr, "initServerAdapter: Set Default LE PHY: status %s: Tx %s, Rx %s\n",
                        to_string(res).c_str(), to_string(Tx).c_str(), to_string(Rx).c_str());
            }
            adapter->setSMPKeyPath(DBTConstants::SERVER_KEY_PATH);

            REQUIRE( true == adapter->addStatusListener( myAdapterStatusListener ) );

            adapter->setServerConnSecurity(adapterSecurityLevel, SMPIOCapability::UNSET);

            return true;
        }
};

#endif // DBT_SERVER01_HPP_
