/*
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2020-2026 Gothel Software e.K.
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
 * This _dbt_scanner10_ C++ scanner ::BTRole::Master GATT client example uses an event driven workflow
 * and multithreading, i.e. one thread processes each found device when notified.
 *
 * _dbt_scanner10_ represents the recommended utilization of Direct-BT.
 *
 * ### dbt_scanner10 Invocation Examples:
 * Using `scripts/run-dbt_scanner10.sh` from `dist` directory:
 *
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
 * * Read device C0:26:DA:01:DA:B1  (using default auto-sec w/ keyboard iocap) from adapter 01:02:03:04:05:06
 *   ~~~
 *   ../scripts/run-dbt_scanner10.sh -adapter adapter 01:02:03:04:05:06 -dev C0:26:DA:01:DA:B1
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

// Default from dbt_peripheral00.cpp or DBTPeripheral00.java
static std::unique_ptr<uuid_t> cmd_uuid = jau::uuid_t::create(std::string("d0ca6bf3-3d52-4760-98e5-fc5883e93712"));
static std::unique_ptr<uuid_t> cmd_rsp_uuid = jau::uuid_t::create(std::string("d0ca6bf3-3d53-4760-98e5-fc5883e93712"));
static uint8_t cmd_arg = 0x44;

static bool SHOW_UPDATE_EVENTS = false;
static bool QUIET = false;

static void connectDiscoveredDevice(BTDeviceRef device);

static void processReadyDevice(const BTDeviceRef& device);

static void removeDevice(BTDeviceRef device);
static void resetAdapter(BTAdapter *a, int mode);
static bool startDiscovery(BTAdapter *a, const std::string& msg);

class MyAdapterStatusListener : public AdapterStatusListener {

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
            std::thread sd(::startDiscovery, &a, "powered-on"); // @suppress("Invalid arguments")
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

        if( BTDeviceRegistry::isWaitingForAnyDevice() ||
            ( BTDeviceRegistry::isWaitingForDevice(device->getAddressAndType().address, device->getName()) &&
              ( 0 < MULTI_MEASUREMENTS || !BTDeviceRegistry::isDeviceProcessed(device->getAddressAndType()) )
            )
          )
        {
            jau_fprintf_td(stderr, "****** FOUND__-0: Connecting %s\n", device->toString(true));
            {
                const uint64_t td = jau::getCurrentMilliseconds() - timestamp_t0; // adapter-init -> now
                jau_fprintf_td(stderr, "PERF: adapter-init -> FOUND__-0  %" PRIu64 " ms\n", td);
            }
            std::thread dc(::connectDiscoveredDevice, device); // @suppress("Invalid arguments")
            dc.detach();
            return true;
        } else {
            if( !QUIET ) {
                jau_fprintf_td(stderr, "****** FOUND__-1: NOP %s\n", device->toString(true));
            }
            return false;
        }
    }

    void deviceUpdated(const BTDeviceRef& device, const EIRDataType updateMask, const uint64_t timestamp) override {
        if( !QUIET && SHOW_UPDATE_EVENTS ) {
            jau_fprintf_td(stderr, "****** UPDATED: %s of %s\n", updateMask, device->toString(true));
        }
        (void)timestamp;
    }

    void deviceConnected(const BTDeviceRef& device, const bool discovered, const uint64_t timestamp) override {
        jau_fprintf_td(stderr, "****** CONNECTED (discovered %d): %s\n", discovered, device->toString(true));
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
                const bool res  = SMPKeyBin::remove(CLIENT_KEY_PATH, *device);
                jau_fprintf_td(stderr, "****** PAIRING_STATE: state %s; Remove key file %s, res %d\n",
                        state, SMPKeyBin::getFilename(CLIENT_KEY_PATH, *device), res);
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
                    std::thread dc(&BTDevice::setPairingPasskey, device, 0);
                    // 3s disconnect: std::thread dc(&BTDevice::setPairingPasskeyNegative, device);
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
        deviceReadyCount++;
        jau_fprintf_td(stderr, "****** READY-0: Processing[%d] %s\n", deviceReadyCount.load(), device->toString(true));
        processReadyDevice(device); // AdapterStatusListener::deviceReady() explicitly allows prolonged and complex code execution!
    }

    void deviceDisconnected(const BTDeviceRef& device, const HCIStatusCode reason, const uint16_t handle, const uint64_t timestamp) override {
        jau_fprintf_td(stderr, "****** DISCONNECTED: Reason 0x%X (%s), old handle %s: %s\n",
                static_cast<uint8_t>(reason), reason,
                toHexString(handle), device->toString(true));
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
        return "MyAdapterStatusListener[this "+toHexString(this)+"]";
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
        jau_fprintf_td(stderr, "**[%2.2zu.%2.2zu] Characteristic-Notify: UUID %s, td %" PRIu64 " ******\n",
                i, j, charDecl->value_type->toUUID128String(), (tR-timestamp));
        jau_fprintf_td(stderr, "**[%2.2zu.%2.2zu]     Characteristic: %s ******\n", i, j, charDecl->toString());
        if( _TEMPERATURE_MEASUREMENT == *charDecl->value_type ) {
            std::shared_ptr<GattTemperatureMeasurement> temp = GattTemperatureMeasurement::get(char_value);
            if( nullptr != temp ) {
                jau_fprintf_td(stderr, "**[%2.2zu.%2.2zu]     Value T: %s ******\n", i, j, temp->toString());
            }
            jau_fprintf_td(stderr, "**[%2.2zu.%2.2zu]     Value R: %s ******\n", i, j, char_value);
        } else {
            jau_fprintf_td(stderr, "**[%2.2zu.%2.2zu]     Value R: %s ******\n", i, j, char_value);
            jau_fprintf_td(stderr, "**[%2.2zu.%2.2zu]     Value S: %s ******\n", i, j, jau::dfa_utf8_decode(char_value.get_ptr(), char_value.size()));
        }
    }

    void indicationReceived(BTGattCharRef charDecl,
                            const TROOctets& char_value, const uint64_t timestamp,
                            const bool confirmationSent) override
    {
        const uint64_t tR = jau::getCurrentMilliseconds();
        jau_fprintf_td(stderr, "**[%2.2zu.%2.2zu] Characteristic-Indication: UUID %s, td %" PRIu64 ", confirmed %d ******\n",
                i, j, charDecl->value_type->toUUID128String(), (tR-timestamp), confirmationSent);
        jau_fprintf_td(stderr, "**[%2.2zu.%2.2zu]     Characteristic: %s ******\n", i, j, charDecl->toString());
        if( _TEMPERATURE_MEASUREMENT == *charDecl->value_type ) {
            std::shared_ptr<GattTemperatureMeasurement> temp = GattTemperatureMeasurement::get(char_value);
            if( nullptr != temp ) {
                jau_fprintf_td(stderr, "**[%2.2zu.%2.2zu]     Value T: %s ******\n", i, j, temp->toString());
            }
            jau_fprintf_td(stderr, "**[%2.2zu.%2.2zu]     Value R: %s ******\n", i, j, char_value);
        } else {
            jau_fprintf_td(stderr, "**[%2.2zu.%2.2zu]     Value R: %s ******\n", i, j, char_value);
            jau_fprintf_td(stderr, "**[%2.2zu.%2.2zu]     Value S: %s ******\n", i, j, jau::dfa_utf8_decode(char_value.get_ptr(), char_value.size()));
        }
    }
};

static void connectDiscoveredDevice(BTDeviceRef device) { // NOLINT(performance-unnecessary-value-param): Pass-by-value out-of-thread
    jau_fprintf_td(stderr, "****** Connecting Device: Start %s\n", device->toString());

    const BTSecurityRegistry::Entry* sec = BTSecurityRegistry::getStartOf(device->getAddressAndType().address, device->getName());
    if( nullptr != sec ) {
        jau_fprintf_td(stderr, "****** Connecting Device: Found SecurityDetail %s for %s\n", sec->toString(), device->toString());
    } else {
        jau_fprintf_td(stderr, "****** Connecting Device: No SecurityDetail for %s\n", device->toString());
    }
    const BTSecurityLevel req_sec_level = nullptr != sec ? sec->getSecLevel() : BTSecurityLevel::UNSET;
    HCIStatusCode res = device->uploadKeys(CLIENT_KEY_PATH, req_sec_level, true /* verbose_ */);
    jau_fprintf_td(stderr, "****** Connecting Device: BTDevice::uploadKeys(...) result %s\n", res);
    if( HCIStatusCode::SUCCESS != res ) {
        if( nullptr != sec ) {
            if( sec->isSecurityAutoEnabled() ) {
                bool r = device->setConnSecurityAuto( sec->getSecurityAutoIOCap() );
                jau_fprintf_td(stderr, "****** Connecting Device: Using SecurityDetail.SEC AUTO %s, set OK %d\n", sec->toString(), r);
            } else if( sec->isSecLevelOrIOCapSet() ) {
                bool r = device->setConnSecurity( sec->getSecLevel(), sec->getIOCap() );
                jau_fprintf_td(stderr, "****** Connecting Device: Using SecurityDetail.Level+IOCap %s, set OK %d\n", sec->toString(), r);
            } else {
                bool r = device->setConnSecurityAuto( SMPIOCapability::KEYBOARD_ONLY );
                jau_fprintf_td(stderr, "****** Connecting Device: Setting SEC AUTO security detail w/ KEYBOARD_ONLY (%s) -> set OK %d\n", sec->toString(), r);
            }
        } else {
            bool r = device->setConnSecurityAuto( SMPIOCapability::KEYBOARD_ONLY );
            jau_fprintf_td(stderr, "****** Connecting Device: Setting SEC AUTO security detail w/ KEYBOARD_ONLY -> set OK %d\n", r);
        }
    }
    std::shared_ptr<const EInfoReport> eir = device->getEIR();
    jau_fprintf_td(stderr, "EIR-1 %s\n", device->getEIRInd()->toString());
    jau_fprintf_td(stderr, "EIR-2 %s\n", device->getEIRScanRsp()->toString());
    jau_fprintf_td(stderr, "EIR-+ %s\n", eir->toString());

    uint16_t conn_interval_min  = (uint16_t)8;  // 10ms
    uint16_t conn_interval_max  = (uint16_t)12; // 15ms
    const uint16_t conn_latency  = (uint16_t)0;
    if( eir->isSet(EIRDataType::CONN_IVAL) ) {
        eir->getConnInterval(conn_interval_min, conn_interval_max);
    }
    const uint16_t supervision_timeout = getHCIConnSupervisorTimeout(conn_latency, (int) ( conn_interval_max * 1.25 ) /* ms */);
    res = device->connectLE(le_scan_interval, le_scan_window, conn_interval_min, conn_interval_max, conn_latency, supervision_timeout);
    jau_fprintf_td(stderr, "****** Connecting Device: End result %s of %s\n", res, device->toString());
}

static void processReadyDevice(const BTDeviceRef& device) {
    jau_fprintf_td(stderr, "****** Processing Ready Device: Start %s\n", device->toString());

    const uint64_t t1 = jau::getCurrentMilliseconds();

    SMPKeyBin::createAndWrite(*device, CLIENT_KEY_PATH, true /* verbose */);

    const uint64_t t2 = jau::getCurrentMilliseconds();

    bool success = false;

    if( device->getAdapter().getBTMajorVersion() > 4 ) {
        LE_PHYs Tx { LE_PHYs::LE_2M }, Rx { LE_PHYs::LE_2M };
        HCIStatusCode res = device->setConnectedLE_PHY(Tx, Rx);
        jau_fprintf_td(stderr, "****** Set Connected LE PHY: status %s: Tx %s, Rx %s\n", res, Tx, Rx);
    }
    {
        LE_PHYs resTx, resRx;
        HCIStatusCode res = device->getConnectedLE_PHY(resTx, resRx);
        jau_fprintf_td(stderr, "****** Got Connected LE PHY: status %s: Tx %s, Rx %s\n", res, resTx, resRx);
    }

    //
    // GATT Service Processing
    //
    jau_fprintf_td(stderr, "****** Processing Ready Device: GATT start: %s\n", device->getAddressAndType());
    if( !QUIET ) {
        device->getAdapter().printDeviceLists();
    }
    const uint64_t t3 = jau::getCurrentMilliseconds();

    try {
        jau::darray<BTGattServiceRef> primServices = device->getGattServices();
        if( 0 == primServices.size() ) {
            jau_fprintf_td(stderr, "****** Processing Ready Device: getServices() failed %s\n", device->toString());
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
            jau_fprintf_td(stderr, "\n\n\n");
            jau_fprintf_td(stderr, "PERF: GATT primary-services completed\n"
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

        if( nullptr != cmd_uuid ) {
            BTGattCmd cmd = nullptr != cmd_rsp_uuid ? BTGattCmd(*device, "TestCmd", *cmd_uuid, *cmd_rsp_uuid, 256)
                                                    : BTGattCmd(*device, "TestCmd", *cmd_uuid);
            cmd.setVerbose(true);
            const bool cmd_resolved = cmd.isResolved();
            jau_fprintf_td(stderr, "Command test: %s, resolved %d\n", cmd, cmd_resolved);
            POctets cmd_data(1, lb_endian_t::little);
            cmd_data.put_uint8_nc(0, cmd_arg);
            const HCIStatusCode cmd_res = cmd.send(true /* prefNoAck */, cmd_data, 3_s);
            if( HCIStatusCode::SUCCESS == cmd_res ) {
                if( cmd.hasResponseSet() ) {
                    const jau::TROOctets& resp = cmd.getResponse();
                    if( 1 == resp.size() && resp.get_uint8_nc(0) == cmd_arg ) {
                        jau_fprintf_td(stderr, "Success: %s -> %s (echo response)\n", cmd, resp);
                    } else {
                        jau_fprintf_td(stderr, "Success: %s -> %s (different response)\n", cmd, resp);
                    }
                } else {
                    jau_fprintf_td(stderr, "Success: %s -> no response\n", cmd);
                }
            } else {
                jau_fprintf_td(stderr, "Failure: %s -> %s\n", cmd, cmd_res);
            }
        }

        std::shared_ptr<GattGenericAccessSvc> ga = device->getGattGenericAccess();
        if( nullptr != ga && !QUIET ) {
            jau_fprintf_td(stderr, "  GenericAccess: %s\n\n", ga->toString());
        }
        {
            std::shared_ptr<BTGattHandler> gatt = device->getGattHandler();
            if( nullptr != gatt && gatt->isConnected() ) {
                std::shared_ptr<GattDeviceInformationSvc> di = gatt->getDeviceInformation(primServices);
                if( nullptr != di && !QUIET ) {
                    jau_fprintf_td(stderr, "  DeviceInformation: %s\n\n", di->toString());
                }
            }
        }


        for(size_t i=0; i<primServices.size(); i++) {
            BTGattService & primService = *primServices.at(i);
            {
                jau_fprintf_td(stderr, "  [%2.2zu] Service UUID %s (%s)\n", i,
                        primService.type->toUUID128String(),
                        primService.type->getTypeSizeString());
                jau_fprintf_td(stderr, "  [%2.2zu]         %s\n", i, primService);
            }
            jau::darray<BTGattCharRef> & serviceCharacteristics = primService.characteristicList;
            for(size_t j=0; j<serviceCharacteristics.size(); j++) {
                BTGattCharRef & serviceChar = serviceCharacteristics.at(j);
                {
                    jau_fprintf_td(stderr, "  [%2.2zu.%2.2zu] Characteristic: UUID %s (%s)\n", i, j,
                            serviceChar->value_type->toUUID128String(),
                            serviceChar->value_type->getTypeSizeString());
                    jau_fprintf_td(stderr, "  [%2.2zu.%2.2zu]     %s\n", i, j, serviceChar->toString());
                }
                if( serviceChar->hasProperties(BTGattChar::PropertyBitVal::Read) ) {
                    POctets value(BTGattHandler::number(BTGattHandler::Defaults::MAX_ATT_MTU), 0, jau::lb_endian_t::little);
                    if( serviceChar->readValue(value) ) {
                        std::string sval = dfa_utf8_decode(value.get_ptr(), value.size());
                        {
                            jau_fprintf_td(stderr, "  [%2.2zu.%2.2zu]     value: %s ('%s')\n", (int)i, (int)j, value, sval);
                        }
                    }
                }
                jau::darray<BTGattDescRef> & charDescList = serviceChar->descriptorList;
                for(size_t k=0; k<charDescList.size(); k++) {
                    BTGattDesc & charDesc = *charDescList.at(k);
                    {
                        jau_fprintf_td(stderr, "  [%2.2zu.%2.2zu.%2.2zu] Descriptor: UUID %s (%s)\n", i, j, k,
                                charDesc.type->toUUID128String(),
                                charDesc.type->getTypeSizeString());
                        jau_fprintf_td(stderr, "  [%2.2zu.%2.2zu.%2.2zu]     %s\n", i, j, k, charDesc);
                    }
                }
                bool cccdEnableResult[2];
                if( serviceChar->enableNotificationOrIndication( cccdEnableResult ) ) {
                    // ClientCharConfigDescriptor (CCD) is available
                    bool clAdded = serviceChar->addCharListener( std::make_shared<MyGATTEventListener>(i, j) );
                    {
                        jau_fprintf_td(stderr, "  [%2.2zu.%2.2zu] Characteristic-Listener: Notification(%d), Indication(%d): Added %d\n",
                                (int)i, (int)j, cccdEnableResult[0], cccdEnableResult[1], clAdded);
                        jau_fprintf_td(stderr, "\n");
                    }
                }
            }
            jau_fprintf_td(stderr, "\n");
        }
        // FIXME sleep 1s for potential callbacks ..
        jau::sleep_for( 1_s );
        success = true;
    } catch ( std::exception & e ) {
        jau_fprintf_td(stderr, "****** Processing Ready Device: Exception caught for %s: %s\n", device->toString(), e.what());
    }

exit:
    jau_fprintf_td(stderr, "****** Processing Ready Device: End-1: Success %d on %s\n", success, device->toString());

    if( DiscoveryPolicy::PAUSE_CONNECTED_UNTIL_DISCONNECTED == discoveryPolicy ) {
        device->getAdapter().removeDevicePausingDiscovery(*device);
    }

    if( KEEP_CONNECTED && GATT_PING_ENABLED && success ) {
        while( device->pingGATT() ) {
            jau_fprintf_td(stderr, "****** Processing Ready Device: pingGATT OK: %s\n", device->getAddressAndType());
            jau::sleep_for( 1_s );
        }
        jau_fprintf_td(stderr, "****** Processing Ready Device: pingGATT failed, waiting for disconnect: %s\n", device->getAddressAndType());
        // Even w/ GATT_PING_ENABLED, we utilize disconnect event to clean up -> remove
    }

    if( !QUIET ) {
        device->getAdapter().printDeviceLists();
    }

    jau_fprintf_td(stderr, "****** Processing Ready Device: End-2: Success %d on %s\n", success, device->toString());

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
        jau_fprintf_td(stderr, "****** Processing Ready Device: MULTI_MEASUREMENTS left %d: %s\n", MULTI_MEASUREMENTS.load(), device->getAddressAndType());
    }
}

static void removeDevice(BTDeviceRef device) { // NOLINT(performance-unnecessary-value-param): Pass-by-value out-of-thread
    jau_fprintf_td(stderr, "****** Remove Device: removing: %s\n", device->getAddressAndType());

    device->remove();
}

static void resetAdapter(BTAdapter *a, int mode) {
    jau_fprintf_td(stderr, "****** Reset Adapter: reset[%d] start: %s\n", mode, a->toString());
    HCIStatusCode res = a->reset();
    jau_fprintf_td(stderr, "****** Reset Adapter: reset[%d] end: %s, %s\n", mode, res, a->toString());
}

static bool startDiscovery(BTAdapter *a, const std::string& msg) {
    if( useAdapter != EUI48::ALL_DEVICE && useAdapter != a->getAddressAndType().address ) {
        jau_fprintf_td(stderr, "****** Start discovery (%s): Adapter not selected: %s\n", msg, a->toString());
        return false;
    }
    HCIStatusCode status = a->startDiscovery( nullptr, discoveryPolicy, le_scan_active, le_scan_interval, le_scan_window, filter_policy, filter_dup );
    jau_fprintf_td(stderr, "****** Start discovery (%s) result: %s: %s\n", msg, status, a->toString());
    return HCIStatusCode::SUCCESS == status;
}

static bool initAdapter(std::shared_ptr<BTAdapter>& adapter) {
    if( useAdapter != EUI48::ALL_DEVICE && useAdapter != adapter->getAddressAndType().address ) {
        jau_fprintf_td(stderr, "initAdapter: Adapter not selected: %s\n", adapter->toString());
        return false;
    }
    // Initialize with defaults and power-on
    if( !adapter->isInitialized() ) {
        HCIStatusCode status = adapter->initialize( btMode, false );
        if( HCIStatusCode::SUCCESS != status ) {
            jau_fprintf_td(stderr, "initAdapter: Adapter initialization failed: %s: %s\n",
                    status, adapter->toString());
            return false;
        }
        adapter->setPrivacy(use_privacy);
    }
    if( !adapter->setPowered( true ) ) {
        jau_fprintf_td(stderr, "initAdapter: Adapter power-on failed:: %s\n", adapter->toString());
        return false;
    }
    // adapter is powered-on
    jau_fprintf_td(stderr, "initAdapter: %s\n", adapter->toString());
    {
        const LE_Features le_feats = adapter->getLEFeatures();
        jau_fprintf_td(stderr, "initAdapter: LE_Features %s\n", le_feats);
    }
    if( adapter->getBTMajorVersion() > 4 ) {
        LE_PHYs Tx { LE_PHYs::LE_2M }, Rx { LE_PHYs::LE_2M };
        HCIStatusCode res = adapter->setDefaultLE_PHY(Tx, Rx);
        jau_fprintf_td(stderr, "initAdapter: Set Default LE PHY: status %s: Tx %s, Rx %s\n", res, Tx, Rx);
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
                jau_fprintf_td(stderr, "****** Adapter ADDED__: InitOK: %s\n", adapter->toString());
            } else {
                jau_fprintf_td(stderr, "****** Adapter ADDED__: Ignored: %s\n", adapter->toString());
            }
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
    bool done = false;

    timestamp_t0 = jau::getCurrentMilliseconds();

    const std::shared_ptr<BTManager>& mngr = BTManager::get();
    mngr->addChangedAdapterSetCallback(myChangedAdapterSetFunc);

    while( !done ) {
        if( 0 == MULTI_MEASUREMENTS ||
            ( -1 == MULTI_MEASUREMENTS && !BTDeviceRegistry::isWaitingForAnyDevice() && BTDeviceRegistry::areAllDevicesProcessed() )
          )
        {
            jau_fprintf_td(stderr, "****** EOL Test MULTI_MEASUREMENTS left %d, processed %zu/%zu\n",
                    MULTI_MEASUREMENTS.load(), BTDeviceRegistry::getProcessedDeviceCount(), BTDeviceRegistry::getWaitForDevicesCount());
            jau_fprintf_td(stderr, "****** WaitForDevice %s\n", BTDeviceRegistry::getWaitForDevicesString());
            jau_fprintf_td(stderr, "****** DevicesProcessed %s\n", BTDeviceRegistry::getProcessedDevicesString());
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
        jau_fprintf_td(stderr, "****** EOL Adapter's Devices - pre close: %s\n", adapter->toString());
        adapter->printDeviceLists();
    });
    {
        BTManager::size_type count = mngr->removeChangedAdapterSetCallback(myChangedAdapterSetFunc);
        jau_fprintf_td(stderr, "****** EOL Removed ChangedAdapterSetCallback %zu\n", (size_t)count);

        mngr->close();
    }
    jau::for_each_const(adapterList, [](const std::shared_ptr<BTAdapter>& adapter) {
        jau_fprintf_td(stderr, "****** EOL Adapter's Devices - post close: %s\n", adapter->toString());
        adapter->printDeviceLists();
    });
}

#include <cstdio>

int main(int argc, char *argv[])
{
    bool waitForEnter=false;

    jau_fprintf_td(stderr, "Direct-BT Native Version %s (API %s)\n", DIRECT_BT_VERSION, DIRECT_BT_VERSION_API);

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
        } else if( !strcmp("-privacy", argv[i]) ) {
            use_privacy = true;
        } else if( !strcmp("-dev", argv[i]) && argc > (i+1) ) {
            std::string addrOrNameSub = std::string(argv[++i]);
            BTDeviceRegistry::addToWaitForDevices( addrOrNameSub );
        } else if( !strcmp("-passkey", argv[i]) && argc > (i+2) ) {
            const std::string addrOrNameSub(argv[++i]);
            BTSecurityRegistry::Entry* sec = BTSecurityRegistry::getOrCreate(addrOrNameSub);
            sec->passkey = atoi(argv[++i]);
            jau_fprintf(stderr, "Set passkey in %s\n", sec->toString());
        } else if( !strcmp("-seclevel", argv[i]) && argc > (i+2) ) {
            const std::string addrOrNameSub(argv[++i]);
            BTSecurityRegistry::Entry* sec = BTSecurityRegistry::getOrCreate(addrOrNameSub);
            sec->sec_level = to_BTSecurityLevel(atoi(argv[++i]));
            jau_fprintf(stderr, "Set sec_level in %s\n", sec->toString());
        } else if( !strcmp("-iocap", argv[i]) && argc > (i+2) ) {
            const std::string addrOrNameSub(argv[++i]);
            BTSecurityRegistry::Entry* sec = BTSecurityRegistry::getOrCreate(addrOrNameSub);
            sec->io_cap = to_SMPIOCapability(atoi(argv[++i]));
            jau_fprintf(stderr, "Set io_cap in %s\n", sec->toString());
        } else if( !strcmp("-secauto", argv[i]) && argc > (i+2) ) {
            const std::string addrOrNameSub(argv[++i]);
            BTSecurityRegistry::Entry* sec = BTSecurityRegistry::getOrCreate(addrOrNameSub);
            sec->io_cap_auto = to_SMPIOCapability(atoi(argv[++i]));
            jau_fprintf(stderr, "Set SEC AUTO security io_cap in %s\n", sec->toString());
        } else if( !strcmp("-cmd", argv[i]) && argc > (i+1) ) {
            cmd_uuid = jau::uuid_t::create((std::string)argv[++i]);
        } else if( !strcmp("-cmdrsp", argv[i]) && argc > (i+1) ) {
            cmd_rsp_uuid = jau::uuid_t::create((std::string)argv[++i]);
        } else if( !strcmp("-cmdarg", argv[i]) && argc > (i+1) ) {
            cmd_arg = (uint8_t)atoi(argv[++i]);
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
    jau_fprintf_td(stderr, "pid %d\n", getpid());

    jau_fprintf_td(stderr, "Run with '[-btmode LE|BREDR|DUAL] "
                    "[-disconnect] [-enableGATTPing] [-count <number>] [-single] [-show_update_events] [-quiet] "
                    "[-discoveryPolicy <0-4>] "
                    "[-scanPassive] "
                    "[-resetEachCon connectionCount] "
                    "[-adapter <adapter_address>] "
                    "[-privacy] "
                    "(-dev <device_[address|name]_sub>)* "
                    "(-seclevel <device_[address|name]_sub> <int_sec_level>)* "
                    "(-iocap <device_[address|name]_sub> <int_iocap>)* "
                    "(-secauto <device_[address|name]_sub> <int_iocap>)* "
                    "(-passkey <device_[address|name]_sub> <digits>)* "
                    "[-cmd <uuid>] [-cmdrsp <uuid>] [-cmdarg <byte-val>] "
                    "[-dbt_verbose true|false] "
                    "[-dbt_debug true|false|adapter.event,gatt.data,hci.event,hci.scan_ad_eir,mgmt.event] "
                    "[-dbt_mgmt cmd.timeout=3000,ringsize=64,...] "
                    "[-dbt_hci cmd.complete.timeout=10000,cmd.status.timeout=3000,ringsize=64,...] "
                    "[-dbt_gatt cmd.read.timeout=500,cmd.write.timeout=500,cmd.init.timeout=2500,ringsize=128,...] "
                    "[-dbt_l2cap reader.timeout=10000,restart.count=0,...] "
                    "\n");

    jau_fprintf_td(stderr, "MULTI_MEASUREMENTS %d\n", MULTI_MEASUREMENTS.load());
    jau_fprintf_td(stderr, "KEEP_CONNECTED %d\n", KEEP_CONNECTED);
    jau_fprintf_td(stderr, "RESET_ADAPTER_EACH_CONN %d\n", RESET_ADAPTER_EACH_CONN);
    jau_fprintf_td(stderr, "GATT_PING_ENABLED %d\n", GATT_PING_ENABLED);
    jau_fprintf_td(stderr, "REMOVE_DEVICE %d\n", REMOVE_DEVICE);
    jau_fprintf_td(stderr, "SHOW_UPDATE_EVENTS %d\n", SHOW_UPDATE_EVENTS);
    jau_fprintf_td(stderr, "QUIET %d\n", QUIET);
    jau_fprintf_td(stderr, "adapter %s, privacy %d\n", useAdapter, use_privacy);
    jau_fprintf_td(stderr, "btmode %s\n", btMode);
    jau_fprintf_td(stderr, "discoveryPolicy %s\n", discoveryPolicy);
    jau_fprintf_td(stderr, "scanActive %s\n", le_scan_active);
    jau_fprintf_td(stderr, "Command: cmd %s, arg 0x%X\n         rsp %s\n",
            nullptr != cmd_uuid ? cmd_uuid->toString() : "n/a", cmd_arg,
            nullptr != cmd_rsp_uuid ? cmd_rsp_uuid->toString() : "n/a");
    jau_fprintf_td(stderr, "security-details: %s\n", BTSecurityRegistry::allToString());
    jau_fprintf_td(stderr, "waitForDevice: %s\n", BTDeviceRegistry::getWaitForDevicesString());

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
    if( true ) {
        // Just for testing purpose, i.e. triggering BTManager::close() within the test controlled app,
        // instead of program shutdown.
        jau_fprintf_td(stderr, "****** Manager close start\n");
        const std::shared_ptr<BTManager>& mngr = BTManager::get(); // already existing
        mngr->close();
        jau_fprintf_td(stderr, "****** Manager close end\n");
    }
}
