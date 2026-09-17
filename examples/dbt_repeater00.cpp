/*
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2022-2026 Gothel Software e.K.
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

/** \file
 * This _dbt_repeater00_ C++ repeater example implementing a GATT repeater,
 * i.e. forwarding client requests to a GATT server and passing the results back.
 *
 * The repeater can be used in between an existing Bluetooth LE client and server,
 * acting as a forwarder and to analyze the GATT client/server protocol.
 *
 * ### dbt_repeater00 Invocation Examples:
 * Using `scripts/run-dbt_repeater00.sh` from `dist` directory:
 *
 * * Connection to server `TAIDOC TD1107` using adapter `DC:FB:48:00:90:19`; Serving client as `TAIDOC TD1108` using adapter `00:1A:7D:DA:71:03`; Using ENC_ONLY (JUST_WORKS) encryption.
 *   ~~~
 *   ../scripts/run-dbt_repeater00.sh -adapterToServer DC:FB:48:00:90:19 -adapterToClient 00:1A:7D:DA:71:03 -server 'TAIDOC TD1107' -nameToClient 'TAIDOC TD1108' -seclevelToServer 'TAIDOC TD1107' 2 -seclevelToClient 2 -quiet
 *   ~~~
 */

static uint64_t timestamp_t0;

static jau::sc_atomic_bool sync_data;
static BTMode btMode = BTMode::DUAL;

//
// To Server Settings (acting as client)
//
static EUI48 adapterToServerAddr = EUI48::ALL_DEVICE;
static BTAdapterRef adapterToServer = nullptr;
static BTDeviceRef connectedDeviceToServer = nullptr;
static std::atomic<int> serverDeviceReadyCount = 0;

static void connectToDiscoveredServer(BTDeviceRef device); // NOLINT(performance-unnecessary-value-param): Pass-by-value out-of-thread
static void processReadyToServer(const BTDeviceRef& device);
static void removeDeviceToServer(BTDeviceRef device); // NOLINT(performance-unnecessary-value-param): Pass-by-value out-of-thread
static bool startDiscoveryToServer(BTAdapter *a, std::string msg); // NOLINT(performance-unnecessary-value-param): Pass-by-value out-of-thread

static DiscoveryPolicy discoveryPolicy = DiscoveryPolicy::PAUSE_CONNECTED_UNTIL_DISCONNECTED;
static const bool le_scan_active = true; // default value
static const uint16_t le_scan_interval = 24; // default value
static const uint16_t le_scan_window = 24; // default value
static const uint8_t filter_policy = 0; // default value
static const uint16_t adv_interval_min=640;
static const uint16_t adv_interval_max=640;
static const AD_PDU_Type adv_type=AD_PDU_Type::ADV_IND;
static const uint8_t adv_chan_map=0x07;

//
// To Client Settings (acting as server)
//
static EUI48 adapterToClientAddr = EUI48::ALL_DEVICE;
static bool adapterToClientUseSC = true;
static std::string adapterToClientName = "repeater0";
static std::string adapterToClientShortName = "repeater0";
static uint16_t max_att_mtu_to_client = 512+1;
static BTSecurityLevel adapterToClientSecLevel = BTSecurityLevel::UNSET;
static BTAdapterRef adapterToClient = nullptr;
static jau::relaxed_atomic_nsize_t servedClientConnections = 0;
static jau::nsize_t MAX_SERVED_CONNECTIONS = 0; // unlimited
static BTDeviceRef connectedDeviceToClient = nullptr;

static bool startAdvertisingToClient(const BTAdapterRef& a, const std::string& msg);
static bool stopAdvertisingToClient(const BTAdapterRef& a, const std::string& msg);
static void processDisconnectedDeviceToClient(BTDeviceRef device); // NOLINT(performance-unnecessary-value-param): Pass-by-value out-of-thread

static bool QUIET = false;

//
// To Server Settings (acting as client)
//

class AdapterToServerStatusListener : public AdapterStatusListener {

    void adapterSettingsChanged(BTAdapter &a, const AdapterSetting oldmask, const AdapterSetting newmask,
                                const AdapterSetting changedmask, const uint64_t timestamp) override {
        const bool initialSetting = AdapterSetting::NONE == oldmask;
        if( initialSetting ) {
            jau_fprintf_td(stderr, "****** To Server: SETTINGS_INITIAL: %s -> %s, changed %s\n", oldmask,
                    newmask, changedmask);
        } else {
            jau_fprintf_td(stderr, "****** To Server: SETTINGS_CHANGED: %s -> %s, changed %s\n", oldmask,
                    newmask, changedmask);
        }
        jau_fprintf_td(stderr, "To Server: Status BTAdapter:\n");
        jau_fprintf_td(stderr, "%s\n", a);
        (void)timestamp;

        if( !initialSetting &&
            isAdapterSettingBitSet(changedmask, AdapterSetting::POWERED) &&
            isAdapterSettingBitSet(newmask, AdapterSetting::POWERED) )
        {
            std::thread sd(::startDiscoveryToServer, &a, "powered-on"); // @suppress("Invalid arguments")
            sd.detach();
        }
    }

    void discoveringChanged(BTAdapter &a, const ScanType currentMeta, const ScanType changedType, const bool changedEnabled, const DiscoveryPolicy policy, const uint64_t timestamp) override {
        jau_fprintf_td(stderr, "****** To Server: DISCOVERING: meta %s, changed[%s, enabled %d, policy %s]: %s\n",
                currentMeta, changedType, changedEnabled, policy, a);
        (void)timestamp;
    }

    bool deviceFound(const BTDeviceRef& device, const uint64_t timestamp) override {
        (void)timestamp;

        if( BTDeviceRegistry::isWaitingForAnyDevice() ||
            BTDeviceRegistry::isWaitingForDevice(device->getAddressAndType().address, device->getName())
          )
        {
            jau_fprintf_td(stderr, "****** To Server: FOUND__-0: Connecting %s\n", device->toString(true));
            {
                const uint64_t td = getCurrentMilliseconds() - timestamp_t0; // adapter-init -> now
                jau_fprintf_td(stderr, "PERF: adapter-init -> FOUND__-0  %" PRIu64 " ms\n", td);
            }
            std::thread dc(::connectToDiscoveredServer, device); // @suppress("Invalid arguments")
            dc.detach();
            return true;
        } else {
            if( !QUIET ) {
                jau_fprintf_td(stderr, "****** To Server: FOUND__-1: NOP %s\n", device->toString(true));
            }
            return false;
        }
    }

    void deviceConnected(const BTDeviceRef& device, const bool discovered, const uint64_t timestamp) override {
        jau_fprintf_td(stderr, "****** To Server: CONNECTED (discovered %d): %s\n", discovered, device->toString(true));
        (void)discovered;
        (void)timestamp;
    }

    void devicePairingState(const BTDeviceRef& device, const SMPPairingState state, const PairingMode mode, const uint64_t timestamp) override {
        jau_fprintf_td(stderr, "****** To Server: PAIRING STATE: state %s, mode %s, %s\n",
            state, mode, device->toString());
        (void)timestamp;
        switch( state ) {
            case SMPPairingState::NONE:
                // next: deviceReady(..)
                break;
            case SMPPairingState::FAILED: {
                const bool res  = SMPKeyBin::remove(CLIENT_KEY_PATH, *device);
                jau_fprintf_td(stderr, "****** To Server: PAIRING_STATE: state %s; Remove key file %s, res %d\n",
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
        if( BTDeviceRegistry::isWaitingForAnyDevice() ||
            BTDeviceRegistry::isWaitingForDevice(device->getAddressAndType().address, device->getName())
          )
        {
            serverDeviceReadyCount++;
            jau_fprintf_td(stderr, "****** To Server: READY-0: Processing[%d] %s\n", serverDeviceReadyCount.load(), device->toString(true));
            processReadyToServer(device); // AdapterStatusListener::deviceReady() explicitly allows prolonged and complex code execution!
        } else {
            jau_fprintf_td(stderr, "****** To Server: READY-1: NOP %s\n", device->toString(true));
        }
    }

    void deviceDisconnected(const BTDeviceRef& device, const HCIStatusCode reason, const uint16_t handle, const uint64_t timestamp) override {
        jau_fprintf_td(stderr, "****** To Server: DISCONNECTED: Reason 0x%X (%s), old handle %s: %s\n",
                static_cast<uint8_t>(reason), reason,
                toHexString(handle), device->toString(true));
        (void)timestamp;
        {
            jau::sc_atomic_critical sync(sync_data);
            connectedDeviceToServer = nullptr;
        }
        std::thread dc(::removeDeviceToServer, device); // @suppress("Invalid arguments")
        dc.detach();
    }

    std::string toString() const noexcept override {
        return "MyAdapterClientStatusListener[this "+toHexString(this)+"]";
    }

};

class NativeGattToServerCharListener : public BTGattHandler::NativeGattCharListener {
  public:

    NativeGattToServerCharListener() = default;

    BTDeviceRef getToClient() noexcept {
        jau::sc_atomic_critical sync(sync_data);
        return connectedDeviceToClient;
    }

    void notificationReceived(const BTDeviceRef& source, const uint16_t char_handle,
                              const TROOctets& char_value, const uint64_t timestamp) override
    {
        (void)timestamp;
        BTDeviceRef devToClient = getToClient();
        std::string devToClientS = nullptr != devToClient ? devToClient->getAddressAndType().address.toString() : "nil";
        std::string devFromServerS = source->getAddressAndType().address.toString();

        jau_fprintf_td(stderr, "%s*  -> %s : Notify: handle %s\n",
                devFromServerS, devToClientS, jau::toHexString(char_handle));
        jau_fprintf_td(stderr, "    raw : %s\n", char_value);
        jau_fprintf_td(stderr, "    utf8: %s\n", jau::dfa_utf8_decode(char_value.get_ptr(), char_value.size()));
        jau_fprintf_td(stderr, "\n");
        std::shared_ptr<BTGattHandler> gh = nullptr != devToClient ? devToClient->getGattHandler() : nullptr;
        if( nullptr != gh ) {
            gh->sendNotification(char_handle, char_value);
        }
    }

    void indicationReceived(const BTDeviceRef& source, const uint16_t char_handle,
                            const TROOctets& char_value, const uint64_t timestamp,
                            const bool confirmationSent) override
    {
        (void)timestamp;
        BTDeviceRef devToClient = getToClient();
        std::string devToClientS = nullptr != devToClient ? devToClient->getAddressAndType().address.toString() : "nil";
        std::string devFromServerS = source->getAddressAndType().address.toString();

        jau_fprintf_td(stderr, "%s*  -> %s : Indication: handle %s, confirmed %d\n",
                devFromServerS, devToClientS, jau::toHexString(char_handle), confirmationSent);
        jau_fprintf_td(stderr, "    raw : %s\n", char_value);
        jau_fprintf_td(stderr, "    utf8: %s\n", jau::dfa_utf8_decode(char_value.get_ptr(), char_value.size()));
        jau_fprintf_td(stderr, "\n");
        std::shared_ptr<BTGattHandler> gh = nullptr != devToClient ? devToClient->getGattHandler() : nullptr;
        if( nullptr != gh ) {
            gh->sendIndication(char_handle, char_value);
        }
    }

    void mtuResponse(const uint16_t clientMTU,
                     const AttPDUMsg& pduReply,
                     const AttErrorRsp::ErrorCode error_reply,
                     const uint16_t serverMTU,
                     const uint16_t usedMTU,
                     const BTDeviceRef& serverReplier,
                     const BTDeviceRef& clientRequester) override {
        std::string serverReplierS = serverReplier->getAddressAndType().address.toString();
        std::string clientRequesterS = nullptr != clientRequester ? clientRequester->getAddressAndType().address.toString() : "nil";

        jau_fprintf_td(stderr, "%s  <-> %s*: MTU: client %u -> %s, server %u -> used %u\n",
                clientRequesterS, serverReplierS,
                clientMTU, AttErrorRsp::getErrorCodeString(error_reply), serverMTU, usedMTU);
        if( AttErrorRsp::ErrorCode::NO_ERROR != error_reply ) {
            jau_fprintf_td(stderr, "    pdu : %s\n", pduReply);
        }
        jau_fprintf_td(stderr, "\n");
    }

    void writeRequest(const uint16_t handle,
                      const jau::TROOctets& data,
                      const jau::darray<Section>& sections,
                      const bool with_response,
                      const BTDeviceRef& serverDest,
                      const BTDeviceRef& clientSource) override {
        std::string serverDestS = serverDest->getAddressAndType().address.toString();
        std::string clientSourceS = nullptr != clientSource ? clientSource->getAddressAndType().address.toString() : "nil";

        jau_fprintf_td(stderr, "%s   -> %s*: Write-Req: handle %s, with_response %d\n",
                clientSourceS, serverDestS, jau::toHexString(handle), with_response);
        jau_fprintf_td(stderr, "    raw : %s\n", data);
        jau_fprintf_td(stderr, "    utf8: %s\n", jau::dfa_utf8_decode(data.get_ptr(), data.size()));
        jau_fprintf_td(stderr, "    sections: ");
        for(Section s : sections) {
            jau_fprintf(stderr, "%s, ", s.toString());
        }
        jau_fprintf(stderr, "\n");
        jau_fprintf_td(stderr, "\n");
    }

    void writeResponse(const AttPDUMsg& pduReply,
                       const AttErrorRsp::ErrorCode error_code,
                       const BTDeviceRef& serverSource,
                       const BTDeviceRef& clientDest) override {
        std::string serverSourceS = serverSource->getAddressAndType().address.toString();
        std::string clientDestS = nullptr != clientDest ? clientDest->getAddressAndType().address.toString() : "nil";

        jau_fprintf_td(stderr, "%s*  -> %s : Write-Rsp: %s\n",
                serverSourceS, clientDestS, AttErrorRsp::getErrorCodeString(error_code));
        jau_fprintf_td(stderr, "    pdu : %s\n", pduReply);
        jau_fprintf_td(stderr, "\n");
    }


    void readResponse(const uint16_t handle,
                      const uint16_t value_offset,
                      const AttPDUMsg& pduReply,
                      const AttErrorRsp::ErrorCode error_reply,
                      const jau::TROOctets& data_reply,
                      const BTDeviceRef& serverReplier,
                      const BTDeviceRef& clientRequester) override {
        std::string serverReplierS = serverReplier->getAddressAndType().address.toString();
        std::string clientRequesterS = nullptr != clientRequester ? clientRequester->getAddressAndType().address.toString() : "nil";

        jau_fprintf_td(stderr, "%s  <-> %s*: Read: handle %s, value_offset %d -> %s\n",
                clientRequesterS, serverReplierS,
                jau::toHexString(handle), value_offset, AttErrorRsp::getErrorCodeString(error_reply));
        if( 0 < data_reply.size() ) {
            jau_fprintf_td(stderr, "    raw : %s\n", data_reply);
            jau_fprintf_td(stderr, "    utf8: %s\n", jau::dfa_utf8_decode(data_reply.get_ptr(), data_reply.size()));
        } else {
            jau_fprintf_td(stderr, "    pdu : %s\n", pduReply);
        }
        jau_fprintf_td(stderr, "\n");
    }

};

static void connectToDiscoveredServer(BTDeviceRef device) { // NOLINT(performance-unnecessary-value-param): Pass-by-value out-of-thread
    jau_fprintf_td(stderr, "****** To Server: Connecting Device: Start %s\n", device->toString());

    const BTSecurityRegistry::Entry* sec = BTSecurityRegistry::getStartOf(device->getAddressAndType().address, device->getName());
    if( nullptr != sec ) {
        jau_fprintf_td(stderr, "****** To Server: Connecting Device: Found SecurityDetail %s for %s\n", sec->toString(), device->toString());
    } else {
        jau_fprintf_td(stderr, "****** To Server: Connecting Device: No SecurityDetail for %s\n", device->toString());
    }
    const BTSecurityLevel req_sec_level = nullptr != sec ? sec->getSecLevel() : BTSecurityLevel::UNSET;
    HCIStatusCode res = device->uploadKeys(CLIENT_KEY_PATH, req_sec_level, true /* verbose_ */);
    jau_fprintf_td(stderr, "****** Connecting Device: BTDevice::uploadKeys(...) result %s\n", res);
    if( HCIStatusCode::SUCCESS != res ) {
        if( nullptr != sec ) {
            if( sec->isSecurityAutoEnabled() ) {
                bool r = device->setConnSecurityAuto( sec->getSecurityAutoIOCap() );
                jau_fprintf_td(stderr, "****** To Server: Connecting Device: Using SecurityDetail.SEC AUTO %s, set OK %d\n", sec->toString(), r);
            } else if( sec->isSecLevelOrIOCapSet() ) {
                bool r = device->setConnSecurity( sec->getSecLevel(), sec->getIOCap() );
                jau_fprintf_td(stderr, "****** To Server: Connecting Device: Using SecurityDetail.Level+IOCap %s, set OK %d\n", sec->toString(), r);
            } else {
                bool r = device->setConnSecurityAuto( SMPIOCapability::KEYBOARD_ONLY );
                jau_fprintf_td(stderr, "****** To Server: Connecting Device: Setting SEC AUTO security detail w/ KEYBOARD_ONLY (%s) -> set OK %d\n", sec->toString(), r);
            }
        } else {
            bool r = device->setConnSecurityAuto( SMPIOCapability::KEYBOARD_ONLY );
            jau_fprintf_td(stderr, "****** To Server: Connecting Device: Setting SEC AUTO security detail w/ KEYBOARD_ONLY -> set OK %d\n", r);
        }
    }
    std::shared_ptr<const EInfoReport> eir = device->getEIR();
    jau_fprintf_td(stderr, "To Server: EIR-1 %s\n", device->getEIRInd()->toString());
    jau_fprintf_td(stderr, "To Server: EIR-2 %s\n", device->getEIRScanRsp()->toString());
    jau_fprintf_td(stderr, "To Server: EIR-+ %s\n", eir->toString());

    uint16_t conn_interval_min  = (uint16_t)12;
    uint16_t conn_interval_max  = (uint16_t)12;
    const uint16_t conn_latency  = (uint16_t)0;
    if( eir->isSet(EIRDataType::CONN_IVAL) ) {
        eir->getConnInterval(conn_interval_min, conn_interval_max);
    }
    const uint16_t supervision_timeout = getHCIConnSupervisorTimeout(conn_latency, (int) ( conn_interval_max * 1.25 ) /* ms */);
    res = device->connectLE(le_scan_interval, le_scan_window, conn_interval_min, conn_interval_max, conn_latency, supervision_timeout);
    jau_fprintf_td(stderr, "****** To Server: Connecting Device: End result %s of %s\n", res, device->toString());
}

static void processReadyToServer(const BTDeviceRef& device) {
    jau_fprintf_td(stderr, "****** To Server: Processing Ready Device: Start %s\n", device->toString());

    SMPKeyBin::createAndWrite(*device, CLIENT_KEY_PATH, true /* verbose */);

    bool success = false;

    if( device->getAdapter().getBTMajorVersion() > 4 ) {
        LE_PHYs Tx { LE_PHYs::LE_2M }, Rx { LE_PHYs::LE_2M };
        HCIStatusCode res = device->setConnectedLE_PHY(Tx, Rx);
        jau_fprintf_td(stderr, "****** To Server: Set Connected LE PHY: status %s: Tx %s, Rx %s\n", res, Tx, Rx);
    }
    {
        LE_PHYs resTx, resRx;
        HCIStatusCode res = device->getConnectedLE_PHY(resTx, resRx);
        jau_fprintf_td(stderr, "****** To Server: Got Connected LE PHY: status %s: Tx %s, Rx %s\n", res, resTx, resRx);
    }

    //
    // GATT Service Processing
    //
    jau_fprintf_td(stderr, "****** To Server: Processing Ready Device: GATT start: %s\n", device->getAddressAndType());
    try {
        std::shared_ptr<BTGattHandler> gh = device->getGattHandler();
        gh->addCharListener( std::make_shared<NativeGattToServerCharListener>() );

        if( nullptr != adapterToClient ) {
            jau::sc_atomic_critical sync(sync_data);
            connectedDeviceToServer = device;
            if( !startAdvertisingToClient(adapterToClient, "processReadyToServer") ) {
                device->disconnect(HCIStatusCode::REMOTE_USER_TERMINATED_CONNECTION);
            } else {
                success = true;
            }
        }
    } catch ( std::exception & e ) {
        jau_fprintf_td(stderr, "****** To Server: Processing Ready Device: Exception caught for %s: %s\n", device->toString(), e.what());
    }

    jau_fprintf_td(stderr, "****** To Server: Processing Ready Device: End-1: Success %d on %s\n", success, device->toString());

    if( success ) {
        BTDeviceRegistry::addToProcessedDevices(device->getAddressAndType(), device->getName());
    }

}

static void removeDeviceToServer(BTDeviceRef device) { // NOLINT(performance-unnecessary-value-param): Pass-by-value out-of-thread
    jau_fprintf_td(stderr, "****** To Server: Remove Device: %s\n", device->getAddressAndType());

    {
        stopAdvertisingToClient(adapterToClient, "removeDeviceToServer");
        jau::sc_atomic_critical sync(sync_data);
        if( nullptr != connectedDeviceToClient ) {
            connectedDeviceToClient->disconnect(HCIStatusCode::CONNECTION_TERMINATED_BY_LOCAL_HOST);
        }
    }
    device->remove();
}

static void resetConnectionToServer(BTDeviceRef device) { // NOLINT(performance-unnecessary-value-param): Pass-by-value out-of-thread
    jau_fprintf_td(stderr, "****** To Server: Disconnected: %s\n", device->toString());
    device->disconnect(HCIStatusCode::DISCONNECTED);

    BTAdapter& a = device->getAdapter();
    jau_fprintf_td(stderr, "****** To Server: Power off: %s\n", a);
    if( a.setPowered(false) ) {
        jau_fprintf_td(stderr, "****** To Server: Power on: %s\n", a);
        if( a.setPowered(true) ) {
            startDiscoveryToServer(&a, "resetConnectionToServer");
        }
    }
}

static bool startDiscoveryToServer(BTAdapter *a, std::string msg) { // NOLINT(performance-unnecessary-value-param): Pass-by-value out-of-thread
    if( adapterToServerAddr != EUI48::ALL_DEVICE && adapterToServerAddr != a->getAddressAndType().address ) {
        jau_fprintf_td(stderr, "****** To Server: Start discovery (%s): Adapter not selected: %s\n", msg, a->toString());
        return false;
    }
    HCIStatusCode status = a->startDiscovery( nullptr, discoveryPolicy, le_scan_active, le_scan_interval, le_scan_window, filter_policy );
    jau_fprintf_td(stderr, "****** To Server: Start discovery (%s) result: %s: %s\n", msg, status, a->toString());
    return HCIStatusCode::SUCCESS == status;
}

static bool initAdapterToServer(std::shared_ptr<BTAdapter>& adapter) {
    if( adapterToServerAddr != EUI48::ALL_DEVICE && adapterToServerAddr != adapter->getAddressAndType().address ) {
        jau_fprintf_td(stderr, "initAdapterToServer: Adapter not selected: %s\n", adapter->toString());
        return false;
    }
    // Initialize with defaults and power-on
    if( !adapter->isInitialized() ) {
        HCIStatusCode status = adapter->initialize( btMode, true );
        if( HCIStatusCode::SUCCESS != status ) {
            jau_fprintf_td(stderr, "initAdapterToServer: Adapter initialization failed: %s: %s\n",
                    status, adapter->toString());
            return false;
        }
    } else if( !adapter->setPowered( true ) ) {
        jau_fprintf_td(stderr, "initAdapterToServer: Already initialized adapter power-on failed:: %s\n", adapter->toString());
        return false;
    }
    // adapter is powered-on
    jau_fprintf_td(stderr, "initAdapterToServer: %s\n", adapter->toString());
    {
        const LE_Features le_feats = adapter->getLEFeatures();
        jau_fprintf_td(stderr, "initAdapterToServer: LE_Features %s\n", le_feats);
    }
    if( adapter->getBTMajorVersion() > 4 ) {
        LE_PHYs Tx { LE_PHYs::LE_2M }, Rx { LE_PHYs::LE_2M };
        HCIStatusCode res = adapter->setDefaultLE_PHY(Tx, Rx);
        jau_fprintf_td(stderr, "initAdapterToServer: Set Default LE PHY: status %s: Tx %s, Rx %s\n", res, Tx, Rx);
    }
    std::shared_ptr<AdapterStatusListener> asl(new AdapterToServerStatusListener());
    adapter->addStatusListener( asl );

    if( !startDiscoveryToServer(adapter.get(), "initAdapterToServer") ) {
        adapter->removeStatusListener( asl );
        return false;
    }
    return true;
}

//
// To Client Settings (acting as server)
//

class AdapterToClientStatusListener : public AdapterStatusListener {

    void adapterSettingsChanged(BTAdapter &a, const AdapterSetting oldmask, const AdapterSetting newmask,
                                const AdapterSetting changedmask, const uint64_t timestamp) override {
        const bool initialSetting = AdapterSetting::NONE == oldmask;
        if( initialSetting ) {
            jau_fprintf_td(stderr, "****** To Client: SETTINGS_INITIAL: %s -> %s, changed %s\n", oldmask,
                    newmask, changedmask);
        } else {
            jau_fprintf_td(stderr, "****** To Client: SETTINGS_CHANGED: %s -> %s, changed %s\n", oldmask,
                    newmask, changedmask);
        }
        jau_fprintf_td(stderr, "To Client: Status BTAdapter:\n");
        jau_fprintf_td(stderr, "%s\n", a);
        (void)timestamp;
    }

    void discoveringChanged(BTAdapter &a, const ScanType currentMeta, const ScanType changedType, const bool changedEnabled, const DiscoveryPolicy policy, const uint64_t timestamp) override {
        jau_fprintf_td(stderr, "****** To Client: DISCOVERING: meta %s, changed[%s, enabled %d, policy %s]: %s\n",
                currentMeta, changedType, changedEnabled, policy, a);
        (void)timestamp;
    }

    bool deviceFound(const BTDeviceRef& device, const uint64_t timestamp) override {
        (void)timestamp;

        jau_fprintf_td(stderr, "****** To Client: FOUND__-1: NOP %s\n", device->toString(true));
        return false;
    }

    void deviceConnected(const BTDeviceRef& device, const bool discovered, const uint64_t timestamp) override {
        jau_fprintf_td(stderr, "****** To Client: CONNECTED (discovered %d): %s\n", discovered, device->toString(true));
        (void)discovered;
        (void)timestamp;
    }

    void devicePairingState(const BTDeviceRef& device, const SMPPairingState state, const PairingMode mode, const uint64_t timestamp) override {
        jau_fprintf_td(stderr, "****** To Client: PAIRING STATE: state %s, mode %s, %s\n",
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
        {
            jau::sc_atomic_critical sync(sync_data);
            connectedDeviceToClient = device;
        }
        jau_fprintf_td(stderr, "****** To Client: READY-0: Processing %s\n", device->toString(true));
    }

    void deviceDisconnected(const BTDeviceRef& device, const HCIStatusCode reason, const uint16_t handle, const uint64_t timestamp) override {
        servedClientConnections = servedClientConnections + 1;
        jau_fprintf_td(stderr, "****** DISCONNECTED (count %zu): Reason 0x%X (%s), old handle %s: %s\n",
                servedClientConnections.load(), static_cast<uint8_t>(reason), reason,
                toHexString(handle), device->toString(true));

        {
            jau::sc_atomic_critical sync(sync_data);
            connectedDeviceToClient = nullptr;
        }
        std::thread sd(::processDisconnectedDeviceToClient, device); // @suppress("Invalid arguments")
        sd.detach();
        (void)timestamp;
    }

    std::string toString() const noexcept override {
        return "MyAdapterServerStatusListener[this "+toHexString(this)+"]";
    }

};

static void processDisconnectedDeviceToClient(BTDeviceRef device) { // NOLINT(performance-unnecessary-value-param): Pass-by-value out-of-thread
    jau_fprintf_td(stderr, "****** To Client: Disconnected Device (count %zu): Start %s\n",
            servedClientConnections.load(), device->toString());

    // already unpaired
    stopAdvertisingToClient(adapterToClient, "processDisconnectedDeviceToClient");

    jau::sleep_for( 100_ms ); // wait a little (FIXME: Fast restart of advertising error)

    BTDeviceRef devToServer;
    {
        jau::sc_atomic_critical sync(sync_data);
        devToServer = connectedDeviceToServer;
    }
    if( nullptr != devToServer ) {
        std::thread sd(::resetConnectionToServer, devToServer); // @suppress("Invalid arguments")
        sd.detach();
    } else {
        startAdvertisingToClient(adapterToClient, "processDisconnectedDeviceToClient");
    }

    jau_fprintf_td(stderr, "****** To Client: Disonnected Device: End %s\n", device->toString());
}

static bool startAdvertisingToClient(const BTAdapterRef& a, const std::string& msg) {
    BTDeviceRef devToServer;
    {
        jau::sc_atomic_critical sync(sync_data);
        devToServer = connectedDeviceToServer;
    }
    if( nullptr == devToServer ) {
        jau_fprintf_td(stderr, "To Client: Start advertising: Skipped, not connected to server\n");
        return false;
    }

    EInfoReport eir = *devToServer->getEIR();
    const EIRDataType ind_mask = EIR_DATA_TYPE_MASK & devToServer->getEIRInd()->getEIRDataMask();
    const EIRDataType scanrsp_mask = EIR_DATA_TYPE_MASK & devToServer->getEIRScanRsp()->getEIRDataMask();

    DBGattServerRef dbGattServer( new DBGattServer( devToServer ) );
    jau_fprintf_td(stderr, "To Client: Start advertising: GattServer %s\n", dbGattServer->toString());

    DBGattCharRef gattDevNameChar = dbGattServer->findGattChar( jau::uuid16_t(GattServiceType::GENERIC_ACCESS),
                                                                jau::uuid16_t(GattCharacteristicType::DEVICE_NAME) );
    if( nullptr != gattDevNameChar ) {
        std::string aname = a->getName();
        gattDevNameChar->setValue(reinterpret_cast<uint8_t*>(aname.data()), aname.size(), 0);
    }

    jau_fprintf_td(stderr, "****** To Client: Start advertising (%s): EIR %s\n", msg, eir);
    jau_fprintf_td(stderr, "****** To Client: Start advertising (%s): adv %s, scanrsp %s\n", msg, ind_mask, scanrsp_mask);

    HCIStatusCode status = a->startAdvertising(dbGattServer, eir, ind_mask, scanrsp_mask,
                                               adv_interval_min, adv_interval_max,
                                               adv_type, adv_chan_map, filter_policy);
    jau_fprintf_td(stderr, "****** To Client: Start advertising (%s) result: %s: %s\n", msg, status, a->toString());
    jau_fprintf_td(stderr, "%s", dbGattServer->toFullString());
    return HCIStatusCode::SUCCESS == status;
}

static bool stopAdvertisingToClient(const BTAdapterRef& a, const std::string& msg) {
    HCIStatusCode status = a->stopAdvertising();
    jau_fprintf_td(stderr, "****** To Client: Stop advertising (%s) result: %s: %s\n", msg, status, a->toString());
    return HCIStatusCode::SUCCESS == status;
}

static bool initAdapterToClient(std::shared_ptr<BTAdapter>& adapter) {
    if( adapterToClientAddr != EUI48::ALL_DEVICE && adapterToClientAddr != adapter->getAddressAndType().address ) {
        jau_fprintf_td(stderr, "initAdapterToClient: Adapter not selected: %s\n", adapter->toString());
        return false;
    }
    if( !adapter->isInitialized() ) {
        // Initialize with defaults and power-on
        const HCIStatusCode status = adapter->initialize( btMode, false );
        if( HCIStatusCode::SUCCESS != status ) {
            jau_fprintf_td(stderr, "initAdapterToClient: initialize failed: %s: %s\n",
                    status, adapter->toString());
            return false;
        }
    } else if( !adapter->setPowered( false ) ) {
        jau_fprintf_td(stderr, "initAdapterToClient: setPower.1 off failed: %s\n", adapter->toString());
        return false;
    }
    // adapter is powered-off
    jau_fprintf_td(stderr, "initAdapterToClient.1: %s\n", adapter->toString());

    {
        HCIStatusCode status = adapter->setName(adapterToClientName, adapterToClientShortName);
        if( HCIStatusCode::SUCCESS == status ) {
            jau_fprintf_td(stderr, "initAdapterToClient: setLocalName OK: %s\n", adapter->toString());
        } else {
            jau_fprintf_td(stderr, "initAdapterToClient: setLocalName failed: %s\n", adapter->toString());
            return false;
        }

        status = adapter->setSecureConnections( adapterToClientUseSC );
        if( HCIStatusCode::SUCCESS == status ) {
            jau_fprintf_td(stderr, "initAdapterToClient: setSecureConnections OK: %s\n", adapter->toString());
        } else {
            jau_fprintf_td(stderr, "initAdapterToClient: setSecureConnections failed: %s\n", adapter->toString());
            return false;
        }

        const uint16_t conn_min_interval = 8;  // 10ms
        const uint16_t conn_max_interval = 40; // 50ms
        const uint16_t conn_latency = 0;
        const uint16_t supervision_timeout = 50; // 500ms
        status = adapter->setDefaultConnParam(conn_min_interval, conn_max_interval, conn_latency, supervision_timeout);
        if( HCIStatusCode::SUCCESS == status ) {
            jau_fprintf_td(stderr, "initAdapterToClient: setDefaultConnParam OK: %s\n", adapter->toString());
        } else if( HCIStatusCode::UNKNOWN_COMMAND == status ) {
            jau_fprintf_td(stderr, "initAdapterToClient: setDefaultConnParam UNKNOWN_COMMAND (ignored): %s\n", adapter->toString());
        } else {
            jau_fprintf_td(stderr, "initAdapterToClient: setDefaultConnParam failed: %s, %s\n", status, adapter->toString());
            return false;
        }

        if( !adapter->setPowered( true ) ) {
            jau_fprintf_td(stderr, "initAdapterToClient: setPower.2 on failed: %s\n", adapter->toString());
            return false;
        }
    }
    jau_fprintf_td(stderr, "initAdapterToClient.2: %s\n", adapter->toString());

    {
        const LE_Features le_feats = adapter->getLEFeatures();
        jau_fprintf_td(stderr, "initAdapterToClient: LE_Features %s\n", le_feats);
    }
    if( adapter->getBTMajorVersion() > 4 ) {
        LE_PHYs Tx { LE_PHYs::LE_2M }, Rx { LE_PHYs::LE_2M };
        HCIStatusCode res = adapter->setDefaultLE_PHY(Tx, Rx);
        jau_fprintf_td(stderr, "initAdapterToClient: Set Default LE PHY: status %s: Tx %s, Rx %s\n", res, Tx, Rx);
    }
    adapter->setSMPKeyPath(SERVER_KEY_PATH);

    std::shared_ptr<AdapterStatusListener> asl( std::make_shared<AdapterToClientStatusListener>() );
    adapter->addStatusListener( asl );

    adapter->setServerConnSecurity(adapterToClientSecLevel, SMPIOCapability::UNSET);

    return true;
}

//
// Common: To Server and Client
//

static void myChangedAdapterSetFunc(const bool added, std::shared_ptr<BTAdapter>& adapter) {
    if( added ) {
        if( nullptr == adapterToServer ) {
            if( initAdapterToServer( adapter ) ) {
                adapterToServer = adapter;
                jau_fprintf_td(stderr, "****** AdapterToServer ADDED__: InitOK: %s\n", adapter->toString());
                return;
            }
        }
        if( nullptr == adapterToClient ) {
            if( initAdapterToClient( adapter ) ) {
                adapterToClient = adapter;
                jau_fprintf_td(stderr, "****** AdapterToClient ADDED__: InitOK: %s\n", adapter->toString());
                return;
            }
        }
        jau_fprintf_td(stderr, "****** Adapter ADDED__: Ignored: %s\n", adapter->toString());
    } else {
        if( nullptr != adapterToServer && adapter == adapterToServer ) {
            adapterToServer = nullptr;
            jau_fprintf_td(stderr, "****** AdapterToServer REMOVED: %s\n", adapter->toString());
            return;
        }
        if( nullptr != adapterToClient && adapter == adapterToClient ) {
            adapterToClient = nullptr;
            jau_fprintf_td(stderr, "****** AdapterToClient REMOVED: %s\n", adapter->toString());
            return;
        }
        jau_fprintf_td(stderr, "****** Adapter REMOVED: Ignored %s\n", adapter->toString());
    }
}

static void test() {
    timestamp_t0 = getCurrentMilliseconds();

    const std::shared_ptr<BTManager>& mngr = BTManager::get();
    mngr->addChangedAdapterSetCallback(myChangedAdapterSetFunc);

    while( 0 == MAX_SERVED_CONNECTIONS || MAX_SERVED_CONNECTIONS > servedClientConnections ) {
        jau::sleep_for( 2_s );
    }
    adapterToServer = nullptr;
    adapterToClient = nullptr;

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
        } else if( !strcmp("-quiet", argv[i]) ) {
            QUIET = true;
        } else if( !strcmp("-discoveryPolicy", argv[i]) ) {
            discoveryPolicy = to_DiscoveryPolicy(atoi(argv[++i]));
        } else if( !strcmp("-btmode", argv[i]) && argc > (i+1) ) {
            btMode = to_BTMode(argv[++i]);
        } else if( !strcmp("-use_sc", argv[i]) && argc > (i+1) ) {
            adapterToClientUseSC = 0 != atoi(argv[++i]);
        } else if( !strcmp("-adapterToClient", argv[i]) && argc > (i+1) ) {
            adapterToClientAddr = EUI48( std::string(argv[++i]) );
        } else if( !strcmp("-nameToClient", argv[i]) && argc > (i+1) ) {
            adapterToClientName = std::string(argv[++i]);
        } else if( !strcmp("-mtuToClient", argv[i]) && argc > (i+1) ) {
            max_att_mtu_to_client = atoi(argv[++i]);
        } else if( !strcmp("-seclevelToClient", argv[i]) && argc > (i+1) ) {
            adapterToClientSecLevel = to_BTSecurityLevel(atoi(argv[++i]));
            jau_fprintf(stderr, "Set sec_level 2 client %s\n", adapterToClientSecLevel);
        } else if( !strcmp("-adapterToServer", argv[i]) && argc > (i+1) ) {
            adapterToServerAddr = EUI48( std::string(argv[++i]) );
        } else if( !strcmp("-server", argv[i]) && argc > (i+1) ) {
            std::string addrOrNameSub = std::string(argv[++i]);
            BTDeviceRegistry::addToWaitForDevices( addrOrNameSub );
        } else if( !strcmp("-passkeyToServer", argv[i]) && argc > (i+2) ) {
            const std::string addrOrNameSub(argv[++i]);
            BTSecurityRegistry::Entry* sec = BTSecurityRegistry::getOrCreate(addrOrNameSub);
            sec->passkey = atoi(argv[++i]);
            jau_fprintf(stderr, "Set passkey to server in %s\n", sec->toString());
        } else if( !strcmp("-seclevelToServer", argv[i]) && argc > (i+2) ) {
            const std::string addrOrNameSub(argv[++i]);
            BTSecurityRegistry::Entry* sec = BTSecurityRegistry::getOrCreate(addrOrNameSub);
            sec->sec_level = to_BTSecurityLevel(atoi(argv[++i]));
            jau_fprintf(stderr, "Set sec_level to server in %s\n", sec->toString());
        } else if( !strcmp("-iocapToServer", argv[i]) && argc > (i+2) ) {
            const std::string addrOrNameSub(argv[++i]);
            BTSecurityRegistry::Entry* sec = BTSecurityRegistry::getOrCreate(addrOrNameSub);
            sec->io_cap = to_SMPIOCapability(atoi(argv[++i]));
            jau_fprintf(stderr, "Set io_cap to server in %s\n", sec->toString());
        } else if( !strcmp("-secautoToServer", argv[i]) && argc > (i+2) ) {
            const std::string addrOrNameSub(argv[++i]);
            BTSecurityRegistry::Entry* sec = BTSecurityRegistry::getOrCreate(addrOrNameSub);
            sec->io_cap_auto = to_SMPIOCapability(atoi(argv[++i]));
            jau_fprintf(stderr, "Set SEC AUTO security io_cap to server in %s\n", sec->toString());
        } else if( !strcmp("-count", argv[i]) && argc > (i+1) ) {
            MAX_SERVED_CONNECTIONS = atoi(argv[++i]);
        }
    }
    jau_fprintf_td(stderr, "pid %d\n", getpid());

    jau_fprintf_td(stderr, "Run with '[-btmode LE|BREDR|DUAL] [-use_sc 0|1] [-count <connection_number>] [-quiet] "
                    "[-discoveryPolicy <0-4>] "
                    "[-adapterToClient <adapter_address>] "
                    "[-nameToClient <adapter_name>] "
                    "[-mtuToClient <max att_mtu>] "
                    "[-seclevelToClient <int_sec_level>]* "
                    "[-adapterToServer <adapter_address>] "
                    "(-server <device_[address|name]_sub>)* "
                    "(-seclevelToServer <device_[address|name]_sub> <int_sec_level>)* "
                    "(-iocapToServer <device_[address|name]_sub> <int_iocap>)* "
                    "(-secautoToServer <device_[address|name]_sub> <int_iocap>)* "
                    "(-passkeyToServer <device_[address|name]_sub> <digits>)* "
                    "[-dbt_verbose true|false] "
                    "[-dbt_debug true|false|adapter.event,gatt.data,hci.event,hci.scan_ad_eir,mgmt.event] "
                    "[-dbt_mgmt cmd.timeout=3000,ringsize=64,...] "
                    "[-dbt_hci cmd.complete.timeout=10000,cmd.status.timeout=3000,ringsize=64,...] "
                    "[-dbt_gatt cmd.read.timeout=500,cmd.write.timeout=500,cmd.init.timeout=2500,ringsize=128,...] "
                    "[-dbt_l2cap reader.timeout=10000,restart.count=0,...] "
                    "\n");

    jau_fprintf_td(stderr, "btmode %s\n", btMode);
    jau_fprintf_td(stderr, "MAX_SERVED_CONNECTIONS %zu\n", MAX_SERVED_CONNECTIONS);
    jau_fprintf_td(stderr, "To Client Settings (acting as server):\n");
    jau_fprintf_td(stderr, "- adapter %s\n", adapterToClientAddr);
    jau_fprintf_td(stderr, "- SC %s\n", adapterToClientUseSC);
    jau_fprintf_td(stderr, "- name %s (short %s)\n", adapterToClientName, adapterToClientShortName);
    jau_fprintf_td(stderr, "- mtu %d\n", (int)max_att_mtu_to_client);
    jau_fprintf_td(stderr, "- sec_level %s\n", adapterToClientSecLevel);
    jau_fprintf_td(stderr, "To Server Settings (acting as client):\n");
    jau_fprintf_td(stderr, "- adapter %s\n", adapterToServerAddr);
    jau_fprintf_td(stderr, "- discoveryPolicy %s\n", discoveryPolicy);
    jau_fprintf_td(stderr, "- security-details client: %s\n", BTSecurityRegistry::allToString());
    jau_fprintf_td(stderr, "- server to connect to: %s\n", BTDeviceRegistry::getWaitForDevicesString());


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
