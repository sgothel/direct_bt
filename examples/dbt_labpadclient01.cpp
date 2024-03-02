/*
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2020-2023 Gothel Software e.K.
 * Copyright (c) 2020-2023 ZAFENA AB
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
using namespace jau::fractions_i64_literals;

/** \file
 * The example demonstrates a client connecting to `Avalun's LabPad device`.
 *
 * It differs from _dbt_scanner10_ as follows:
 *
 * * Employs a minimal GattServer supplying `Generic Access` service
 *
 * * Performing one simple Gatt write and indication listener test
 *
 * * Uses pre-set `-dev LabPad` device name and SMPIOCapability::KEYBOARD_ONLY + BTSecurityLevel::ENC_AUTH
 *
 * * Commandline `-passkey <int>` uses `LabPad` implicitly, i.e. user only needs to pass the integer w/o device name.
 *
 * Other than that, please refer to _dbt_scanner10_ as a general example.
 */

static uint64_t timestamp_t0;

static EUI48 useAdapter = EUI48::ALL_DEVICE;
static BTMode btMode = BTMode::DUAL;
static bool use_privacy = false;

static DiscoveryPolicy discoveryPolicy = DiscoveryPolicy::PAUSE_CONNECTED_UNTIL_READY; // default value
static bool le_scan_active = true; // default value
static const uint16_t le_scan_interval = 24; // default value
static const uint16_t le_scan_window = 24; // default value
static const uint8_t filter_policy = 0; // default value
static const bool filter_dup = true; // default value

static std::shared_ptr<BTAdapter> chosenAdapter = nullptr;

static int RESET_ADAPTER_EACH_CONN = 0;
static std::atomic<int> deviceReadyCount = 0;

static std::atomic<int> MULTI_MEASUREMENTS = 8;

static bool KEEP_CONNECTED = true;
static bool GATT_PING_ENABLED = false;
static bool REMOVE_DEVICE = true;

// Avalun's LabPad Comman + Event UUID
static std::unique_ptr<uuid_t> cmd_req_uuid = jau::uuid_t::create(std::string("2c1b2472-4a5f-11e5-9595-0002a5d5c51b"));
static std::unique_ptr<uuid_t> cmd_rsp_uuid = jau::uuid_t::create(std::string("2c1b2473-4a5f-11e5-9595-0002a5d5c51b"));

static const POctets cmd_data1( { 0x00 /* cmd-idx-0 */,
                                  0x14, 0x00, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00,
                                  0x01, 0x5E, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00,
                                  0x9B, 0x23, 0x84 }, lb_endian::little);

static const POctets cmd_data2( { 0x01 /* cmd-idx-1 */,
                                  0xB8 }, lb_endian::little);

static const POctets resp_exp(  { 0x00 /* rsp-idx-0 */,
                                  0x14, 0x00, 0x00, 0x00, 0x01, 0x10, 0x00, 0x00,
                                  0x01, 0x89, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                  0xf6, 0x64, 0x17,
                                  0x01 /* rsp-idx-1 */, 0xed }, lb_endian::little );

static bool SHOW_UPDATE_EVENTS = false;
static bool QUIET = false;

static void connectDiscoveredDevice(BTDeviceRef device);

static void processReadyDevice(const BTDeviceRef& device);

static void removeDevice(BTDeviceRef device);
static void resetAdapter(BTAdapter *a, int mode);
static bool startDiscovery(BTAdapter *a, const std::string& msg);

DBGattServerRef dbGattServer( new DBGattServer(
        /* services: */
        jau::make_darray( // DBGattService
          std::make_shared<DBGattService> ( true /* primary */,
              std::make_unique<const jau::uuid16_t>(GattServiceType::GENERIC_ACCESS) /* type_ */,
              jau::make_darray ( // DBGattChar
                  std::make_shared<DBGattChar>( std::make_unique<const jau::uuid16_t>(GattCharacteristicType::DEVICE_NAME) /* value_type_ */,
                              BTGattChar::PropertyBitVal::Read,
                              jau::darray<DBGattDescRef>() /* intentionally empty */,
                              make_gvalue("Jausoft_Dev", 128) /* value */, true /* variable_length */ ),
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
            std::thread sd(::startDiscovery, &a, "powered-on"); // @suppress("Invalid arguments")
            sd.detach();
        }
    }

    void discoveringChanged(BTAdapter &a, const ScanType currentMeta, const ScanType changedType, const bool changedEnabled, const DiscoveryPolicy policy, const uint64_t timestamp) override {
        fprintf_td(stderr, "****** DISCOVERING: meta %s, changed[%s, enabled %d, policy %s]: %s\n",
                to_string(currentMeta).c_str(), to_string(changedType).c_str(), changedEnabled, to_string(policy).c_str(), a.toString().c_str());
        (void)timestamp;
    }

    bool deviceFound(const BTDeviceRef& device, const uint64_t timestamp) override {
        (void)timestamp;

        if( BTDeviceRegistry::isWaitingForAnyDevice() ||
            ( BTDeviceRegistry::isWaitingForDevice(device->getAddressAndType().address, device->getName()) &&
              ( 0 < MULTI_MEASUREMENTS || !BTDeviceRegistry::isDeviceProcessed(device->getAddressAndType()) )
            )
          )
        {
            fprintf_td(stderr, "****** FOUND__-0: Connecting %s\n", device->toString(true).c_str());
            {
                const uint64_t td = jau::getCurrentMilliseconds() - timestamp_t0; // adapter-init -> now
                fprintf_td(stderr, "PERF: adapter-init -> FOUND__-0  %" PRIu64 " ms\n", td);
            }
            std::thread dc(::connectDiscoveredDevice, device); // @suppress("Invalid arguments")
            dc.detach();
            return true;
        } else {
            if( !QUIET ) {
                fprintf_td(stderr, "****** FOUND__-1: NOP %s\n", device->toString(true).c_str());
            }
            return false;
        }
    }

    void deviceUpdated(const BTDeviceRef& device, const EIRDataType updateMask, const uint64_t timestamp) override {
        if( !QUIET && SHOW_UPDATE_EVENTS ) {
            fprintf_td(stderr, "****** UPDATED: %s of %s\n", to_string(updateMask).c_str(), device->toString(true).c_str());
        }
        (void)timestamp;
    }

    void deviceConnected(const BTDeviceRef& device, const bool discovered, const uint64_t timestamp) override {
        fprintf_td(stderr, "****** CONNECTED (discovered %d): %s\n", discovered, device->toString(true).c_str());
        (void)discovered;
        (void)timestamp;
    }

    void devicePairingState(const BTDeviceRef& device, const SMPPairingState state, const PairingMode mode, const uint64_t timestamp) override {
        fprintf_td(stderr, "****** PAIRING STATE: state %s, mode %s, %s\n",
            to_string(state).c_str(), to_string(mode).c_str(), device->toString().c_str());
        (void)timestamp;
        switch( state ) {
            case SMPPairingState::NONE:
                // next: deviceReady(..)
                break;
            case SMPPairingState::FAILED: {
                const bool res  = SMPKeyBin::remove(CLIENT_KEY_PATH, *device);
                fprintf_td(stderr, "****** PAIRING_STATE: state %s; Remove key file %s, res %d\n",
                        to_string(state).c_str(), SMPKeyBin::getFilename(CLIENT_KEY_PATH, *device).c_str(), res);
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
                const BTSecurityRegistry::Entry* sec = BTSecurityRegistry::getStartOf(device->getAddressAndType().address, device->getName());
                if( nullptr != sec && sec->getPairingPasskey() != BTSecurityRegistry::Entry::NO_PASSKEY ) {
                    std::thread dc(&BTDevice::setPairingPasskey, device, static_cast<uint32_t>( sec->getPairingPasskey() ));
                    dc.detach();
                } else {
                    // std::thread dc(&BTDevice::setPairingPasskey, device, 0);
                    // 3s disconnect:
                    std::thread dc(&BTDevice::setPairingPasskeyNegative, device);
                    dc.detach();
                }
                // next: KEY_DISTRIBUTION or FAILED
              } break;
            case SMPPairingState::NUMERIC_COMPARE_EXPECTED: {
                const BTSecurityRegistry::Entry* sec = BTSecurityRegistry::getStartOf(device->getAddressAndType().address, device->getName());
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
        deviceReadyCount++;
        fprintf_td(stderr, "****** READY-0: Processing[%d] %s\n", deviceReadyCount.load(), device->toString(true).c_str());
        processReadyDevice(device); // AdapterStatusListener::deviceReady() explicitly allows prolonged and complex code execution!
    }

    void deviceDisconnected(const BTDeviceRef& device, const HCIStatusCode reason, const uint16_t handle, const uint64_t timestamp) override {
        fprintf_td(stderr, "****** DISCONNECTED: Reason 0x%X (%s), old handle %s: %s\n",
                static_cast<uint8_t>(reason), to_string(reason).c_str(),
                to_hexstring(handle).c_str(), device->toString(true).c_str());
        (void)timestamp;

        if( REMOVE_DEVICE ) {
            std::thread dc(::removeDevice, device); // @suppress("Invalid arguments")
            dc.detach();
        }
        if( 0 < RESET_ADAPTER_EACH_CONN && 0 == deviceReadyCount % RESET_ADAPTER_EACH_CONN ) {
            std::thread dc(::resetAdapter, &device->getAdapter(), 1); // @suppress("Invalid arguments")
            dc.detach();
        }
    }

    std::string toString() const noexcept override {
        return "MyAdapterStatusListener[this "+to_hexstring(this)+"]";
    }

};

static const uuid16_t _TEMPERATURE_MEASUREMENT(GattCharacteristicType::TEMPERATURE_MEASUREMENT);

class MyGATTEventListener : public BTGattCharListener {
  private:
    int i, j;

  public:

    MyGATTEventListener(int i_, int j_) : i(i_), j(j_) {}

    void notificationReceived(BTGattCharRef charDecl, const TROOctets& char_value, const uint64_t timestamp) override {
        const uint64_t tR = jau::getCurrentMilliseconds();
        fprintf_td(stderr, "**[%2.2d.%2.2d] Characteristic-Notify: UUID %s, td %" PRIu64 " ******\n",
                i, j, charDecl->value_type->toUUID128String().c_str(), (tR-timestamp));
        fprintf_td(stderr, "**[%2.2d.%2.2d]     Characteristic: %s ******\n", i, j, charDecl->toString().c_str());
        if( _TEMPERATURE_MEASUREMENT == *charDecl->value_type ) {
            std::shared_ptr<GattTemperatureMeasurement> temp = GattTemperatureMeasurement::get(char_value);
            if( nullptr != temp ) {
                fprintf_td(stderr, "**[%2.2d.%2.2d]     Value T: %s ******\n", i, j, temp->toString().c_str());
            }
            fprintf_td(stderr, "**[%2.2d.%2.2d]     Value R: %s ******\n", i, j, char_value.toString().c_str());
        } else {
            fprintf_td(stderr, "**[%2.2d.%2.2d]     Value R: %s ******\n", i, j, char_value.toString().c_str());
            fprintf_td(stderr, "**[%2.2d.%2.2d]     Value S: %s ******\n", i, j, jau::dfa_utf8_decode(char_value.get_ptr(), char_value.size()).c_str());
        }
    }

    void indicationReceived(BTGattCharRef charDecl,
                            const TROOctets& char_value, const uint64_t timestamp,
                            const bool confirmationSent) override
    {
        const uint64_t tR = jau::getCurrentMilliseconds();
        fprintf_td(stderr, "**[%2.2d.%2.2d] Characteristic-Indication: UUID %s, td %" PRIu64 ", confirmed %d ******\n",
                i, j, charDecl->value_type->toUUID128String().c_str(), (tR-timestamp), confirmationSent);
        fprintf_td(stderr, "**[%2.2d.%2.2d]     Characteristic: %s ******\n", i, j, charDecl->toString().c_str());
        if( _TEMPERATURE_MEASUREMENT == *charDecl->value_type ) {
            std::shared_ptr<GattTemperatureMeasurement> temp = GattTemperatureMeasurement::get(char_value);
            if( nullptr != temp ) {
                fprintf_td(stderr, "**[%2.2d.%2.2d]     Value T: %s ******\n", i, j, temp->toString().c_str());
            }
            fprintf_td(stderr, "**[%2.2d.%2.2d]     Value R: %s ******\n", i, j, char_value.toString().c_str());
        } else {
            fprintf_td(stderr, "**[%2.2d.%2.2d]     Value R: %s ******\n", i, j, char_value.toString().c_str());
            fprintf_td(stderr, "**[%2.2d.%2.2d]     Value S: %s ******\n", i, j, jau::dfa_utf8_decode(char_value.get_ptr(), char_value.size()).c_str());
        }
    }
};

static void connectDiscoveredDevice(BTDeviceRef device) { // NOLINT(performance-unnecessary-value-param): Pass-by-value out-of-thread
    fprintf_td(stderr, "****** Connecting Device: Start %s\n", device->toString().c_str());

    const BTSecurityRegistry::Entry* sec = BTSecurityRegistry::getStartOf(device->getAddressAndType().address, device->getName());
    if( nullptr != sec ) {
        fprintf_td(stderr, "****** Connecting Device: Found SecurityDetail %s for %s\n", sec->toString().c_str(), device->toString().c_str());
    } else {
        fprintf_td(stderr, "****** Connecting Device: No SecurityDetail for %s\n", device->toString().c_str());
    }
    const BTSecurityLevel req_sec_level = nullptr != sec ? sec->getSecLevel() : BTSecurityLevel::UNSET;
    HCIStatusCode res = device->uploadKeys(CLIENT_KEY_PATH, req_sec_level, true /* verbose_ */);
    fprintf_td(stderr, "****** Connecting Device: BTDevice::uploadKeys(...) result %s\n", to_string(res).c_str());
    if( HCIStatusCode::SUCCESS != res ) {
        if( nullptr != sec ) {
            if( sec->isSecurityAutoEnabled() ) {
                bool r = device->setConnSecurityAuto( sec->getSecurityAutoIOCap() );
                fprintf_td(stderr, "****** Connecting Device: Using SecurityDetail.SEC AUTO %s, set OK %d\n", sec->toString().c_str(), r);
            } else if( sec->isSecLevelOrIOCapSet() ) {
                bool r = device->setConnSecurity( sec->getSecLevel(), sec->getIOCap() );
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
    std::shared_ptr<const EInfoReport> eir = device->getEIR();
    fprintf_td(stderr, "EIR-1 %s\n", device->getEIRInd()->toString().c_str());
    fprintf_td(stderr, "EIR-2 %s\n", device->getEIRScanRsp()->toString().c_str());
    fprintf_td(stderr, "EIR-+ %s\n", eir->toString().c_str());

    uint16_t conn_interval_min  = (uint16_t)8;  // 10ms
    uint16_t conn_interval_max  = (uint16_t)12; // 15ms
    const uint16_t conn_latency  = (uint16_t)0;
    if( eir->isSet(EIRDataType::CONN_IVAL) ) {
        eir->getConnInterval(conn_interval_min, conn_interval_max);
    }
    const uint16_t supervision_timeout = (uint16_t) getHCIConnSupervisorTimeout(conn_latency, (int) ( conn_interval_max * 1.25 ) /* ms */);
    res = device->connectLE(le_scan_interval, le_scan_window, conn_interval_min, conn_interval_max, conn_latency, supervision_timeout);
    fprintf_td(stderr, "****** Connecting Device: End result %s of %s\n", to_string(res).c_str(), device->toString().c_str());
}

static void processReadyDevice(const BTDeviceRef& device) {
    fprintf_td(stderr, "****** Processing Ready Device: Start %s\n", device->toString().c_str());

    const uint64_t t1 = jau::getCurrentMilliseconds();

    SMPKeyBin::createAndWrite(*device, CLIENT_KEY_PATH, true /* verbose */);

    const uint64_t t2 = jau::getCurrentMilliseconds();

    bool success = false;

    if( device->getAdapter().getBTMajorVersion() > 4 ) {
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

    //
    // GATT Service Processing
    //
    fprintf_td(stderr, "****** Processing Ready Device: GATT start: %s\n", device->getAddressAndType().toString().c_str());
    if( !QUIET ) {
        device->getAdapter().printDeviceLists();
    }
    const uint64_t t3 = jau::getCurrentMilliseconds();

    try {
        jau::darray<BTGattServiceRef> primServices = device->getGattServices();
        if( 0 == primServices.size() ) {
            fprintf_td(stderr, "****** Processing Ready Device: getServices() failed %s\n", device->toString().c_str());
            goto exit;
        }

        const uint64_t t5 = jau::getCurrentMilliseconds();
        {
            const uint64_t td00 = device->getLastDiscoveryTimestamp() - timestamp_t0; // adapter-init to discovered
            const uint64_t td01 = t1 - timestamp_t0; // adapter-init to processing-start
            const uint64_t td05 = t5 - timestamp_t0; // adapter-init -> gatt-complete
            const uint64_t tdc1 = t1 - device->getLastDiscoveryTimestamp(); // discovered to processing-start
            const uint64_t tdc5 = t5 - device->getLastDiscoveryTimestamp(); // discovered to gatt-complete
            const uint64_t td12 = t2 - t1; // SMPKeyBin
            const uint64_t td23 = t3 - t2; // LE_PHY
            const uint64_t td13 = t3 - t1; // SMPKeyBin + LE_PHY
            const uint64_t td35 = t5 - t3; // get-gatt-services
            fprintf_td(stderr, "\n\n\n");
            fprintf_td(stderr, "PERF: GATT primary-services completed\n"
                               "PERF:  adapter-init to discovered %" PRIu64 " ms,\n"
                               "PERF:  adapter-init to processing-start %" PRIu64 " ms,\n"
                               "PERF:  adapter-init to gatt-complete %" PRIu64 " ms\n"
                               "PERF:  discovered to processing-start %" PRIu64 " ms,\n"
                               "PERF:  discovered to gatt-complete %" PRIu64 " ms,\n"
                               "PERF:  SMPKeyBin + LE_PHY %" PRIu64 " ms (SMPKeyBin %" PRIu64 " ms, LE_PHY %" PRIu64 " ms),\n"
                               "PERF:  get-gatt-services %" PRIu64 " ms,\n\n",
                               td00, td01, td05,
                               tdc1, tdc5,
                               td13, td12, td23, td35);
        }

        {
            const jau::nsize_t response_size = resp_exp.size();
            int fail_point = 0;
            BTGattCmd cmd = BTGattCmd(*device, "TestCmd", *cmd_req_uuid, *cmd_rsp_uuid, 256);
            cmd.setVerbose(true);
            HCIStatusCode cmd_res = cmd.isResolved() ? HCIStatusCode::SUCCESS : HCIStatusCode::INTERNAL_FAILURE;
            if( HCIStatusCode::SUCCESS != cmd_res ) {
                fail_point = 1;
            } else {
                cmd.setResponseMinSize(response_size);
                cmd.setDataCallback( [](BTGattCharRef, const jau::TROOctets& char_value, const uint64_t) {
                    fprintf_td(stderr, "Received: %s\n", char_value.toString().c_str());
                });
                fprintf_td(stderr, "Command test: %s\n", cmd.toString().c_str());
                cmd_res = cmd.sendOnly(true, cmd_data1);
            }
            if( HCIStatusCode::SUCCESS != cmd_res ) {
                fail_point = 2;
            } else {
                cmd_res = cmd.send(true, cmd_data2, 3_s);
            }
            if( HCIStatusCode::SUCCESS != cmd_res ) {
                fail_point = 3;
            } else {
                if( !cmd.hasResponseSet() ) {
                    fail_point = 4;
                } else {
                    const jau::TROOctets& resp = cmd.getResponse();
                    if( response_size != resp.size() ) {
                        fail_point = 5;
                        fprintf_td(stderr, "Failure: %s -> %s (response size)\n", cmd.toString().c_str(), resp.toString().c_str());
                    } else if( resp_exp != resp ) {
                        fail_point = 6;
                        fprintf_td(stderr, "Failure: %s (response content)\n", cmd.toString().c_str());
                        fprintf_td(stderr, "- exp %s\n", resp_exp.toString().c_str());
                        fprintf_td(stderr, "- has %s\n", resp.toString().c_str());
                    } else {
                        fprintf_td(stderr, "Success: %s -> %s\n", cmd.toString().c_str(), resp.toString().c_str());
                    }
                }
            }
            if( 0 != fail_point ) {
                fprintf_td(stderr, "Failure: point %u: %s -> %s\n", fail_point, cmd.toString().c_str(), to_string(cmd_res).c_str());
            }
        }

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
            {
                fprintf_td(stderr, "  [%2.2d] Service UUID %s (%s)\n", i,
                        primService.type->toUUID128String().c_str(),
                        primService.type->getTypeSizeString().c_str());
                fprintf_td(stderr, "  [%2.2d]         %s\n", i, primService.toString().c_str());
            }
            jau::darray<BTGattCharRef> & serviceCharacteristics = primService.characteristicList;
            for(size_t j=0; j<serviceCharacteristics.size(); j++) {
                BTGattCharRef & serviceChar = serviceCharacteristics.at(j);
                {
                    fprintf_td(stderr, "  [%2.2d.%2.2d] Characteristic: UUID %s (%s)\n", i, j,
                            serviceChar->value_type->toUUID128String().c_str(),
                            serviceChar->value_type->getTypeSizeString().c_str());
                    fprintf_td(stderr, "  [%2.2d.%2.2d]     %s\n", i, j, serviceChar->toString().c_str());
                }
                if( serviceChar->hasProperties(BTGattChar::PropertyBitVal::Read) ) {
                    POctets value(BTGattHandler::number(BTGattHandler::Defaults::MAX_ATT_MTU), 0, jau::lb_endian::little);
                    if( serviceChar->readValue(value) ) {
                        std::string sval = dfa_utf8_decode(value.get_ptr(), value.size());
                        {
                            fprintf_td(stderr, "  [%2.2d.%2.2d]     value: %s ('%s')\n", (int)i, (int)j, value.toString().c_str(), sval.c_str());
                        }
                    }
                }
                jau::darray<BTGattDescRef> & charDescList = serviceChar->descriptorList;
                for(size_t k=0; k<charDescList.size(); k++) {
                    BTGattDesc & charDesc = *charDescList.at(k);
                    {
                        fprintf_td(stderr, "  [%2.2d.%2.2d.%2.2d] Descriptor: UUID %s (%s)\n", i, j, k,
                                charDesc.type->toUUID128String().c_str(),
                                charDesc.type->getTypeSizeString().c_str());
                        fprintf_td(stderr, "  [%2.2d.%2.2d.%2.2d]     %s\n", i, j, k, charDesc.toString().c_str());
                    }
                }
                bool cccdEnableResult[2];
                if( serviceChar->enableNotificationOrIndication( cccdEnableResult ) ) {
                    // ClientCharConfigDescriptor (CCD) is available
                    bool clAdded = serviceChar->addCharListener( std::make_shared<MyGATTEventListener>(i, j) );
                    {
                        fprintf_td(stderr, "  [%2.2d.%2.2d] Characteristic-Listener: Notification(%d), Indication(%d): Added %d\n",
                                (int)i, (int)j, cccdEnableResult[0], cccdEnableResult[1], clAdded);
                        fprintf_td(stderr, "\n");
                    }
                }
            }
            fprintf_td(stderr, "\n");
        }
        // FIXME sleep 1s for potential callbacks ..
        jau::sleep_for( 1_s );
        success = true;
    } catch ( std::exception & e ) {
        fprintf_td(stderr, "****** Processing Ready Device: Exception caught for %s: %s\n", device->toString().c_str(), e.what());
    }

exit:
    fprintf_td(stderr, "****** Processing Ready Device: End-1: Success %d on %s\n", success, device->toString().c_str());

    if( DiscoveryPolicy::PAUSE_CONNECTED_UNTIL_DISCONNECTED == discoveryPolicy ) {
        device->getAdapter().removeDevicePausingDiscovery(*device);
    }

    if( KEEP_CONNECTED && GATT_PING_ENABLED && success ) {
        while( device->pingGATT() ) {
            fprintf_td(stderr, "****** Processing Ready Device: pingGATT OK: %s\n", device->getAddressAndType().toString().c_str());
            jau::sleep_for( 1_s );
        }
        fprintf_td(stderr, "****** Processing Ready Device: pingGATT failed, waiting for disconnect: %s\n", device->getAddressAndType().toString().c_str());
        // Even w/ GATT_PING_ENABLED, we utilize disconnect event to clean up -> remove
    }

    if( !QUIET ) {
        device->getAdapter().printDeviceLists();
    }

    fprintf_td(stderr, "****** Processing Ready Device: End-2: Success %d on %s\n", success, device->toString().c_str());

    if( success ) {
        BTDeviceRegistry::addToProcessedDevices(device->getAddressAndType(), device->getName());
    }

    if( !KEEP_CONNECTED ) {
        device->removeAllCharListener();

        device->remove();

        if( 0 < RESET_ADAPTER_EACH_CONN && 0 == deviceReadyCount % RESET_ADAPTER_EACH_CONN ) {
            resetAdapter(&device->getAdapter(), 2);
        }
    }

    if( 0 < MULTI_MEASUREMENTS ) {
        MULTI_MEASUREMENTS--;
        fprintf_td(stderr, "****** Processing Ready Device: MULTI_MEASUREMENTS left %d: %s\n", MULTI_MEASUREMENTS.load(), device->getAddressAndType().toString().c_str());
    }
}

static void removeDevice(BTDeviceRef device) { // NOLINT(performance-unnecessary-value-param): Pass-by-value out-of-thread
    fprintf_td(stderr, "****** Remove Device: removing: %s\n", device->getAddressAndType().toString().c_str());

    device->remove();
}

static void resetAdapter(BTAdapter *a, int mode) {
    fprintf_td(stderr, "****** Reset Adapter: reset[%d] start: %s\n", mode, a->toString().c_str());
    HCIStatusCode res = a->reset();
    fprintf_td(stderr, "****** Reset Adapter: reset[%d] end: %s, %s\n", mode, to_string(res).c_str(), a->toString().c_str());
}

static bool startDiscovery(BTAdapter *a, const std::string& msg) {
    if( useAdapter != EUI48::ALL_DEVICE && useAdapter != a->getAddressAndType().address ) {
        fprintf_td(stderr, "****** Start discovery (%s): Adapter not selected: %s\n", msg.c_str(), a->toString().c_str());
        return false;
    }

    if( false ) {
        DBGattCharRef gattDevNameChar = dbGattServer->findGattChar( jau::uuid16_t(GattServiceType::GENERIC_ACCESS),
                                                                    jau::uuid16_t(GattCharacteristicType::DEVICE_NAME) );
        if( nullptr != gattDevNameChar ) {
            std::string aname = a->getName();
            gattDevNameChar->setValue(reinterpret_cast<uint8_t*>(aname.data()), aname.size(), 0);
        }
    }

    HCIStatusCode status = a->startDiscovery( dbGattServer, discoveryPolicy, le_scan_active, le_scan_interval, le_scan_window, filter_policy, filter_dup );
    fprintf_td(stderr, "****** Start discovery (%s) result: %s: %s\n", msg.c_str(), to_string(status).c_str(), a->toString().c_str());
    fprintf_td(stderr, "%s", dbGattServer->toFullString().c_str());
    return HCIStatusCode::SUCCESS == status;
}

static bool initAdapter(std::shared_ptr<BTAdapter>& adapter) {
    if( useAdapter != EUI48::ALL_DEVICE && useAdapter != adapter->getAddressAndType().address ) {
        fprintf_td(stderr, "initAdapter: Adapter not selected: %s\n", adapter->toString().c_str());
        return false;
    }
    // Initialize with defaults and power-on
    if( !adapter->isInitialized() ) {
        HCIStatusCode status = adapter->initialize( btMode, false );
        if( HCIStatusCode::SUCCESS != status ) {
            fprintf_td(stderr, "initAdapter: Adapter initialization failed: %s: %s\n",
                    to_string(status).c_str(), adapter->toString().c_str());
            return false;
        }
        adapter->setPrivacy(use_privacy);
    }
    if( !adapter->setPowered( true ) ) {
        fprintf_td(stderr, "initAdapter: Adapter power-on failed:: %s\n", adapter->toString().c_str());
        return false;
    }
    // adapter is powered-on
    fprintf_td(stderr, "initAdapter: %s\n", adapter->toString().c_str());
    {
        const LE_Features le_feats = adapter->getLEFeatures();
        fprintf_td(stderr, "initAdapter: LE_Features %s\n", to_string(le_feats).c_str());
    }
    if( adapter->getBTMajorVersion() > 4 ) {
        LE_PHYs Tx { LE_PHYs::LE_2M }, Rx { LE_PHYs::LE_2M };
        HCIStatusCode res = adapter->setDefaultLE_PHY(Tx, Rx);
        fprintf_td(stderr, "initAdapter: Set Default LE PHY: status %s: Tx %s, Rx %s\n",
                to_string(res).c_str(), to_string(Tx).c_str(), to_string(Rx).c_str());
    }
    std::shared_ptr<AdapterStatusListener> asl(new MyAdapterStatusListener());
    adapter->addStatusListener( asl );

    if( !startDiscovery(adapter.get(), "initAdapter") ) {
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
}

void test() {
    bool done = false;

    timestamp_t0 = jau::getCurrentMilliseconds();

    std::shared_ptr<BTManager> mngr = BTManager::get();
    mngr->addChangedAdapterSetCallback(myChangedAdapterSetFunc);

    while( !done ) {
        if( 0 == MULTI_MEASUREMENTS ||
            ( -1 == MULTI_MEASUREMENTS && !BTDeviceRegistry::isWaitingForAnyDevice() && BTDeviceRegistry::areAllDevicesProcessed() )
          )
        {
            fprintf_td(stderr, "****** EOL Test MULTI_MEASUREMENTS left %d, processed %zu/%zu\n",
                    MULTI_MEASUREMENTS.load(), BTDeviceRegistry::getProcessedDeviceCount(), BTDeviceRegistry::getWaitForDevicesCount());
            fprintf_td(stderr, "****** WaitForDevice %s\n", BTDeviceRegistry::getWaitForDevicesString().c_str());
            fprintf_td(stderr, "****** DevicesProcessed %s\n", BTDeviceRegistry::getProcessedDevicesString().c_str());
            done = true;
        } else {
            jau::sleep_for( 2_s );
        }
    }
    chosenAdapter = nullptr;

    //
    // just a manually controlled pull down to show status, not required
    //
    jau::darray<std::shared_ptr<BTAdapter>> adapterList = mngr->getAdapters();

    jau::for_each_const(adapterList, [](const std::shared_ptr<BTAdapter>& adapter) {
        fprintf_td(stderr, "****** EOL Adapter's Devices - pre close: %s\n", adapter->toString().c_str());
        adapter->printDeviceLists();
    });
    {
        BTManager::size_type count = mngr->removeChangedAdapterSetCallback(myChangedAdapterSetFunc);
        fprintf_td(stderr, "****** EOL Removed ChangedAdapterSetCallback %zu\n", (size_t)count);

        mngr->close();
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

    fprintf_td(stderr, "Direct-BT Native Version %s (API %s)\n", DIRECT_BT_VERSION, DIRECT_BT_VERSION_API);

    // Add defaults for Avalun's LabPad device, announcing its device name as 'LabPad[0-9]+'
    std::string dev_name_prefix = "LabPad";
    {
        BTDeviceRegistry::addToWaitForDevices( dev_name_prefix );

        BTSecurityRegistry::Entry* sec = BTSecurityRegistry::getOrCreate(dev_name_prefix);
        sec->io_cap = SMPIOCapability::KEYBOARD_ONLY;
        sec->sec_level = BTSecurityLevel::ENC_AUTH;
    }

    for(int i=1; i<argc; i++) {
        fprintf(stderr, "arg[%d/%d]: '%s'\n", i, argc, argv[i]);

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
        } else if( !strcmp("-quiet", argv[i]) ) {
            QUIET = true;
        } else if( !strcmp("-discoveryPolicy", argv[i]) ) {
            discoveryPolicy = to_DiscoveryPolicy(atoi(argv[++i]));
        } else if( !strcmp("-scanPassive", argv[i]) ) {
            le_scan_active = false;
        } else if( !strcmp("-btmode", argv[i]) && argc > (i+1) ) {
            btMode = to_BTMode(argv[++i]);
        } else if( !strcmp("-adapter", argv[i]) && argc > (i+1) ) {
            useAdapter = EUI48( std::string(argv[++i]) );
        } else if( !strcmp("-passkey", argv[i]) && argc > (i+1) ) {
            BTSecurityRegistry::Entry* sec = BTSecurityRegistry::getOrCreate(dev_name_prefix);
            sec->passkey = atoi(argv[++i]);
            fprintf(stderr, "Set passkey in %s\n", sec->toString().c_str());
        }
    }
    fprintf_td(stderr, "pid %d\n", getpid());

    fprintf_td(stderr, "Run with '[-btmode LE|BREDR|DUAL] "
                    "[-disconnect] [-show_update_events] [-quiet] "
                    "[-discoveryPolicy <0-4>] "
                    "[-scanPassive] "
                    "[-adapter <adapter_address>] "
                    "(-passkey <digits>)* "
                    "[-dbt_verbose true|false] "
                    "[-dbt_debug true|false|adapter.event,gatt.data,hci.event,hci.scan_ad_eir,mgmt.event] "
                    "[-dbt_mgmt cmd.timeout=3000,ringsize=64,...] "
                    "[-dbt_hci cmd.complete.timeout=10000,cmd.status.timeout=3000,ringsize=64,...] "
                    "[-dbt_gatt cmd.read.timeout=500,cmd.write.timeout=500,cmd.init.timeout=2500,ringsize=128,...] "
                    "[-dbt_l2cap reader.timeout=10000,restart.count=0,...] "
                    "\n");

    fprintf_td(stderr, "MULTI_MEASUREMENTS %d\n", MULTI_MEASUREMENTS.load());
    fprintf_td(stderr, "KEEP_CONNECTED %d\n", KEEP_CONNECTED);
    fprintf_td(stderr, "RESET_ADAPTER_EACH_CONN %d\n", RESET_ADAPTER_EACH_CONN);
    fprintf_td(stderr, "GATT_PING_ENABLED %d\n", GATT_PING_ENABLED);
    fprintf_td(stderr, "REMOVE_DEVICE %d\n", REMOVE_DEVICE);
    fprintf_td(stderr, "SHOW_UPDATE_EVENTS %d\n", SHOW_UPDATE_EVENTS);
    fprintf_td(stderr, "QUIET %d\n", QUIET);
    fprintf_td(stderr, "adapter %s, privacy %d\n", useAdapter.toString().c_str(), use_privacy);
    fprintf_td(stderr, "btmode %s\n", to_string(btMode).c_str());
    fprintf_td(stderr, "discoveryPolicy %s\n", to_string(discoveryPolicy).c_str());
    fprintf_td(stderr, "scanActive %s\n", to_string(le_scan_active).c_str());
    fprintf_td(stderr, "security-details: %s\n", BTSecurityRegistry::allToString().c_str());
    fprintf_td(stderr, "waitForDevice: %s\n", BTDeviceRegistry::getWaitForDevicesString().c_str());

    if( waitForEnter ) {
        fprintf_td(stderr, "Press ENTER to continue\n");
        getchar();
    }
    fprintf_td(stderr, "****** TEST start\n");
    test();
    fprintf_td(stderr, "****** TEST end\n");
    if( true ) {
        // Just for testing purpose, i.e. triggering BTManager::close() within the test controlled app,
        // instead of program shutdown.
        fprintf_td(stderr, "****** Manager close start\n");
        std::shared_ptr<BTManager> mngr = BTManager::get(); // already existing
        mngr->close();
        fprintf_td(stderr, "****** Manager close end\n");
    }
}
