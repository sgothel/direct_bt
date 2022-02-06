/*
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

static void connectToDiscoveredServer(BTDeviceRef device);
static void processReadyToServer(BTDeviceRef device);
static void removeDeviceToServer(BTDeviceRef device);
static bool startDiscoveryToServer(BTAdapter *a, std::string msg);

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

static bool startAdvertisingToClient(BTAdapterRef a, std::string msg);
static bool stopAdvertisingToClient(BTAdapterRef a, std::string msg);
static void processDisconnectedDeviceToClient(BTDeviceRef device);

static bool QUIET = false;

//
// To Server Settings (acting as client)
//

class AdapterToServerStatusListener : public AdapterStatusListener {

    void adapterSettingsChanged(BTAdapter &a, const AdapterSetting oldmask, const AdapterSetting newmask,
                                const AdapterSetting changedmask, const uint64_t timestamp) override {
        const bool initialSetting = AdapterSetting::NONE == oldmask;
        if( initialSetting ) {
            fprintf_td(stderr, "****** To Server: SETTINGS_INITIAL: %s -> %s, changed %s\n", to_string(oldmask).c_str(),
                    to_string(newmask).c_str(), to_string(changedmask).c_str());
        } else {
            fprintf_td(stderr, "****** To Server: SETTINGS_CHANGED: %s -> %s, changed %s\n", to_string(oldmask).c_str(),
                    to_string(newmask).c_str(), to_string(changedmask).c_str());
        }
        fprintf_td(stderr, "To Server: Status BTAdapter:\n");
        fprintf_td(stderr, "%s\n", a.toString().c_str());
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
        fprintf_td(stderr, "****** To Server: DISCOVERING: meta %s, changed[%s, enabled %d, policy %s]: %s\n",
                to_string(currentMeta).c_str(), to_string(changedType).c_str(), changedEnabled, to_string(policy).c_str(), a.toString().c_str());
        (void)timestamp;
    }

    bool deviceFound(BTDeviceRef device, const uint64_t timestamp) override {
        (void)timestamp;

        if( !BTDeviceRegistry::isDeviceProcessing( device->getAddressAndType() ) &&
            ( !BTDeviceRegistry::isWaitingForAnyDevice() ||
              BTDeviceRegistry::isWaitingForDevice(device->getAddressAndType().address, device->getName())
            )
          )
        {
            fprintf_td(stderr, "****** To Server: FOUND__-0: Connecting %s\n", device->toString(true).c_str());
            {
                const uint64_t td = getCurrentMilliseconds() - timestamp_t0; // adapter-init -> now
                fprintf_td(stderr, "PERF: adapter-init -> FOUND__-0  %" PRIu64 " ms\n", td);
            }
            std::thread dc(::connectToDiscoveredServer, device); // @suppress("Invalid arguments")
            dc.detach();
            return true;
        } else {
            if( !QUIET ) {
                fprintf_td(stderr, "****** To Server: FOUND__-1: NOP %s\n", device->toString(true).c_str());
            }
            return false;
        }
    }

    void deviceConnected(BTDeviceRef device, const uint16_t handle, const uint64_t timestamp) override {
        fprintf_td(stderr, "****** To Server: CONNECTED: %s\n", device->toString(true).c_str());
        (void)handle;
        (void)timestamp;
    }

    void devicePairingState(BTDeviceRef device, const SMPPairingState state, const PairingMode mode, const uint64_t timestamp) override {
        fprintf_td(stderr, "****** To Server: PAIRING STATE: state %s, mode %s, %s\n",
            to_string(state).c_str(), to_string(mode).c_str(), device->toString().c_str());
        (void)timestamp;
        switch( state ) {
            case SMPPairingState::NONE:
                // next: deviceReady(..)
                break;
            case SMPPairingState::FAILED: {
                const bool res  = SMPKeyBin::remove(CLIENT_KEY_PATH, *device);
                fprintf_td(stderr, "****** To Server: PAIRING_STATE: state %s; Remove key file %s, res %d\n",
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
            serverDeviceReadyCount++;
            fprintf_td(stderr, "****** To Server: READY-0: Processing[%d] %s\n", serverDeviceReadyCount.load(), device->toString(true).c_str());
            BTDeviceRegistry::addToProcessingDevices(device->getAddressAndType(), device->getName());
            processReadyToServer(device); // AdapterStatusListener::deviceReady() explicitly allows prolonged and complex code execution!
        } else {
            fprintf_td(stderr, "****** To Server: READY-1: NOP %s\n", device->toString(true).c_str());
        }
    }

    void deviceDisconnected(BTDeviceRef device, const HCIStatusCode reason, const uint16_t handle, const uint64_t timestamp) override {
        fprintf_td(stderr, "****** To Server: DISCONNECTED: Reason 0x%X (%s), old handle %s: %s\n",
                static_cast<uint8_t>(reason), to_string(reason).c_str(),
                to_hexstring(handle).c_str(), device->toString(true).c_str());
        (void)timestamp;
        {
            jau::sc_atomic_critical sync(sync_data);
            connectedDeviceToServer = nullptr;
        }
        std::thread dc(::removeDeviceToServer, device); // @suppress("Invalid arguments")
        dc.detach();
    }

    std::string toString() const override {
        return "MyAdapterClientStatusListener[this "+to_hexstring(this)+"]";
    }

};

class NativeGattToServerCharListener : public BTGattHandler::NativeGattCharListener {
  public:

    NativeGattToServerCharListener() {}

    BTDeviceRef getToClient() noexcept {
        jau::sc_atomic_critical sync(sync_data);
        return connectedDeviceToClient;
    }

    void notificationReceived(BTDeviceRef source, const uint16_t char_handle,
                              const TROOctets& char_value, const uint64_t timestamp) override
    {
        (void)timestamp;
        BTDeviceRef devToClient = getToClient();
        std::string devToClientS = nullptr != devToClient ? devToClient->getAddressAndType().address.toString() : "nil";
        std::string devFromServerS = source->getAddressAndType().address.toString();

        fprintf_td(stderr, "%s*  -> %s : Notify: handle %s\n",
                devFromServerS.c_str(), devToClientS.c_str(), jau::to_hexstring(char_handle).c_str());
        fprintf_td(stderr, "    raw : %s\n", char_value.toString().c_str());
        fprintf_td(stderr, "    utf8: %s\n", jau::dfa_utf8_decode(char_value.get_ptr(), char_value.size()).c_str());
        fprintf_td(stderr, "\n");
        std::shared_ptr<BTGattHandler> gh = nullptr != devToClient ? devToClient->getGattHandler() : nullptr;
        if( nullptr != gh ) {
            gh->sendNotification(char_handle, char_value);
        }
    }

    void indicationReceived(BTDeviceRef source, const uint16_t char_handle,
                            const TROOctets& char_value, const uint64_t timestamp,
                            const bool confirmationSent) override
    {
        (void)timestamp;
        BTDeviceRef devToClient = getToClient();
        std::string devToClientS = nullptr != devToClient ? devToClient->getAddressAndType().address.toString() : "nil";
        std::string devFromServerS = source->getAddressAndType().address.toString();

        fprintf_td(stderr, "%s*  -> %s : Indication: handle %s, confirmed %d\n",
                devFromServerS.c_str(), devToClientS.c_str(), jau::to_hexstring(char_handle).c_str(), confirmationSent);
        fprintf_td(stderr, "    raw : %s\n", char_value.toString().c_str());
        fprintf_td(stderr, "    utf8: %s\n", jau::dfa_utf8_decode(char_value.get_ptr(), char_value.size()).c_str());
        fprintf_td(stderr, "\n");
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
                     BTDeviceRef serverReplier,
                     BTDeviceRef clientRequester) override {
        std::string serverReplierS = serverReplier->getAddressAndType().address.toString();
        std::string clientRequesterS = nullptr != clientRequester ? clientRequester->getAddressAndType().address.toString() : "nil";

        fprintf_td(stderr, "%s  <-> %s*: MTU: client %u -> %s, server %u -> used %u\n",
                clientRequesterS.c_str(), serverReplierS.c_str(),
                clientMTU, AttErrorRsp::getErrorCodeString(error_reply).c_str(), serverMTU, usedMTU);
        if( AttErrorRsp::ErrorCode::NO_ERROR != error_reply ) {
            fprintf_td(stderr, "    pdu : %s\n", pduReply.toString().c_str());
        }
        fprintf_td(stderr, "\n");
    }

    void writeRequest(const uint16_t handle,
                      const jau::TROOctets& data,
                      const jau::darray<Section>& sections,
                      const bool with_response,
                      BTDeviceRef serverDest,
                      BTDeviceRef clientSource) override {
        std::string serverDestS = serverDest->getAddressAndType().address.toString();
        std::string clientSourceS = nullptr != clientSource ? clientSource->getAddressAndType().address.toString() : "nil";

        fprintf_td(stderr, "%s   -> %s*: Write-Req: handle %s, with_response %d\n",
                clientSourceS.c_str(), serverDestS.c_str(), jau::to_hexstring(handle).c_str(), with_response);
        fprintf_td(stderr, "    raw : %s\n", data.toString().c_str());
        fprintf_td(stderr, "    utf8: %s\n", jau::dfa_utf8_decode(data.get_ptr(), data.size()).c_str());
        fprintf_td(stderr, "    sections: ");
        for(Section s : sections) {
            fprintf(stderr, "%s, ", s.toString().c_str());
        }
        fprintf(stderr, "\n");
        fprintf_td(stderr, "\n");
    }

    void writeResponse(const AttPDUMsg& pduReply,
                       const AttErrorRsp::ErrorCode error_code,
                       BTDeviceRef serverSource,
                       BTDeviceRef clientDest) override {
        std::string serverSourceS = serverSource->getAddressAndType().address.toString();
        std::string clientDestS = nullptr != clientDest ? clientDest->getAddressAndType().address.toString() : "nil";

        fprintf_td(stderr, "%s*  -> %s : Write-Rsp: %s\n",
                serverSourceS.c_str(), clientDestS.c_str(), AttErrorRsp::getErrorCodeString(error_code).c_str());
        fprintf_td(stderr, "    pdu : %s\n", pduReply.toString().c_str());
        fprintf_td(stderr, "\n");
    }


    void readResponse(const uint16_t handle,
                      const uint16_t value_offset,
                      const AttPDUMsg& pduReply,
                      const AttErrorRsp::ErrorCode error_reply,
                      const jau::TROOctets& data_reply,
                      BTDeviceRef serverReplier,
                      BTDeviceRef clientRequester) override {
        std::string serverReplierS = serverReplier->getAddressAndType().address.toString();
        std::string clientRequesterS = nullptr != clientRequester ? clientRequester->getAddressAndType().address.toString() : "nil";

        fprintf_td(stderr, "%s  <-> %s*: Read: handle %s, value_offset %d -> %s\n",
                clientRequesterS.c_str(), serverReplierS.c_str(),
                jau::to_hexstring(handle).c_str(), value_offset, AttErrorRsp::getErrorCodeString(error_reply).c_str());
        if( 0 < data_reply.size() ) {
            fprintf_td(stderr, "    raw : %s\n", data_reply.toString().c_str());
            fprintf_td(stderr, "    utf8: %s\n", jau::dfa_utf8_decode(data_reply.get_ptr(), data_reply.size()).c_str());
        } else {
            fprintf_td(stderr, "    pdu : %s\n", pduReply.toString().c_str());
        }
        fprintf_td(stderr, "\n");
    }

};

static void connectToDiscoveredServer(BTDeviceRef device) {
    fprintf_td(stderr, "****** To Server: Connecting Device: Start %s\n", device->toString().c_str());

    const BTSecurityRegistry::Entry* sec = BTSecurityRegistry::getStartOf(device->getAddressAndType().address, device->getName());
    if( nullptr != sec ) {
        fprintf_td(stderr, "****** To Server: Connecting Device: Found SecurityDetail %s for %s\n", sec->toString().c_str(), device->toString().c_str());
    } else {
        fprintf_td(stderr, "****** To Server: Connecting Device: No SecurityDetail for %s\n", device->toString().c_str());
    }
    const BTSecurityLevel req_sec_level = nullptr != sec ? sec->getSecLevel() : BTSecurityLevel::UNSET;
    HCIStatusCode res = device->uploadKeys(CLIENT_KEY_PATH, req_sec_level, true /* verbose_ */);
    fprintf_td(stderr, "****** Connecting Device: BTDevice::uploadKeys(...) result %s\n", to_string(res).c_str());
    if( HCIStatusCode::SUCCESS != res ) {
        if( nullptr != sec ) {
            if( sec->isSecurityAutoEnabled() ) {
                bool r = device->setConnSecurityAuto( sec->getSecurityAutoIOCap() );
                fprintf_td(stderr, "****** To Server: Connecting Device: Using SecurityDetail.SEC AUTO %s, set OK %d\n", sec->toString().c_str(), r);
            } else if( sec->isSecLevelOrIOCapSet() ) {
                bool r = device->setConnSecurity( sec->getSecLevel(), sec->getIOCap() );
                fprintf_td(stderr, "****** To Server: Connecting Device: Using SecurityDetail.Level+IOCap %s, set OK %d\n", sec->toString().c_str(), r);
            } else {
                bool r = device->setConnSecurityAuto( SMPIOCapability::KEYBOARD_ONLY );
                fprintf_td(stderr, "****** To Server: Connecting Device: Setting SEC AUTO security detail w/ KEYBOARD_ONLY (%s) -> set OK %d\n", sec->toString().c_str(), r);
            }
        } else {
            bool r = device->setConnSecurityAuto( SMPIOCapability::KEYBOARD_ONLY );
            fprintf_td(stderr, "****** To Server: Connecting Device: Setting SEC AUTO security detail w/ KEYBOARD_ONLY -> set OK %d\n", r);
        }
    }
    std::shared_ptr<const EInfoReport> eir = device->getEIR();
    fprintf_td(stderr, "To Server: Using EIR %s\n", eir->toString().c_str());

    uint16_t conn_interval_min  = (uint16_t)12;
    uint16_t conn_interval_max  = (uint16_t)12;
    const uint16_t conn_latency  = (uint16_t)0;
    if( eir->isSet(EIRDataType::CONN_IVAL) ) {
        eir->getConnInterval(conn_interval_min, conn_interval_max);
    }
    const uint16_t supervision_timeout = (uint16_t) getHCIConnSupervisorTimeout(conn_latency, (int) ( conn_interval_max * 1.25 ) /* ms */);
    res = device->connectLE(le_scan_interval, le_scan_window, conn_interval_min, conn_interval_max, conn_latency, supervision_timeout);
    fprintf_td(stderr, "****** To Server: Connecting Device: End result %s of %s\n", to_string(res).c_str(), device->toString().c_str());
}

static void processReadyToServer(BTDeviceRef device) {
    fprintf_td(stderr, "****** To Server: Processing Ready Device: Start %s\n", device->toString().c_str());

    SMPKeyBin::createAndWrite(*device, CLIENT_KEY_PATH, true /* verbose */);

    bool success = false;

    {
        LE_PHYs Tx { LE_PHYs::LE_2M }, Rx { LE_PHYs::LE_2M };
        HCIStatusCode res = device->setConnectedLE_PHY(Tx, Rx);
        fprintf_td(stderr, "****** To Server: Set Connected LE PHY: status %s: Tx %s, Rx %s\n",
                to_string(res).c_str(), to_string(Tx).c_str(), to_string(Rx).c_str());
    }
    {
        LE_PHYs resTx, resRx;
        HCIStatusCode res = device->getConnectedLE_PHY(resTx, resRx);
        fprintf_td(stderr, "****** To Server: Got Connected LE PHY: status %s: Tx %s, Rx %s\n",
                to_string(res).c_str(), to_string(resTx).c_str(), to_string(resRx).c_str());
    }

    //
    // GATT Service Processing
    //
    fprintf_td(stderr, "****** To Server: Processing Ready Device: GATT start: %s\n", device->getAddressAndType().toString().c_str());
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
        fprintf_td(stderr, "****** To Server: Processing Ready Device: Exception caught for %s: %s\n", device->toString().c_str(), e.what());
    }

    fprintf_td(stderr, "****** To Server: Processing Ready Device: End-1: Success %d on %s; devInProc %zu\n",
            success, device->toString().c_str(), BTDeviceRegistry::getProcessingDeviceCount());

    BTDeviceRegistry::removeFromProcessingDevices(device->getAddressAndType());

    if( success ) {
        BTDeviceRegistry::addToProcessedDevices(device->getAddressAndType(), device->getName());
    }

}

static void removeDeviceToServer(BTDeviceRef device) {
    fprintf_td(stderr, "****** To Server: Remove Device: %s\n", device->getAddressAndType().toString().c_str());

    BTDeviceRegistry::removeFromProcessingDevices(device->getAddressAndType());

    {
        stopAdvertisingToClient(adapterToClient, "removeDeviceToServer");
        jau::sc_atomic_critical sync(sync_data);
        if( nullptr != connectedDeviceToClient ) {
            connectedDeviceToClient->disconnect(HCIStatusCode::CONNECTION_TERMINATED_BY_LOCAL_HOST);
        }
    }
    device->remove();
}

static void resetConnectionToServer(BTDeviceRef device) {
    fprintf_td(stderr, "****** To Server: Disconnected: %s\n", device->toString().c_str());
    device->disconnect(HCIStatusCode::DISCONNECTED);

    BTAdapter& a = device->getAdapter();
    fprintf_td(stderr, "****** To Server: Power off: %s\n", a.toString().c_str());
    if( a.setPowered(false) ) {
        fprintf_td(stderr, "****** To Server: Power on: %s\n", a.toString().c_str());
        if( a.setPowered(true) ) {
            startDiscoveryToServer(&a, "resetConnectionToServer");
        }
    }
}

static bool startDiscoveryToServer(BTAdapter *a, std::string msg) {
    if( adapterToServerAddr != EUI48::ALL_DEVICE && adapterToServerAddr != a->getAddressAndType().address ) {
        fprintf_td(stderr, "****** To Server: Start discovery (%s): Adapter not selected: %s\n", msg.c_str(), a->toString().c_str());
        return false;
    }
    HCIStatusCode status = a->startDiscovery( discoveryPolicy, le_scan_active, le_scan_interval, le_scan_window, filter_policy );
    fprintf_td(stderr, "****** To Server: Start discovery (%s) result: %s: %s\n", msg.c_str(), to_string(status).c_str(), a->toString().c_str());
    return HCIStatusCode::SUCCESS == status;
}

static bool initAdapterToServer(std::shared_ptr<BTAdapter>& adapter) {
    if( adapterToServerAddr != EUI48::ALL_DEVICE && adapterToServerAddr != adapter->getAddressAndType().address ) {
        fprintf_td(stderr, "initAdapterToServer: Adapter not selected: %s\n", adapter->toString().c_str());
        return false;
    }
    // Initialize with defaults and power-on
    if( !adapter->isInitialized() ) {
        HCIStatusCode status = adapter->initialize( btMode );
        if( HCIStatusCode::SUCCESS != status ) {
            fprintf_td(stderr, "initAdapterToServer: Adapter initialization failed: %s: %s\n",
                    to_string(status).c_str(), adapter->toString().c_str());
            return false;
        }
    } else if( !adapter->setPowered( true ) ) {
        fprintf_td(stderr, "initAdapterToServer: Already initialized adapter power-on failed:: %s\n", adapter->toString().c_str());
        return false;
    }
    // adapter is powered-on
    fprintf_td(stderr, "initAdapterToServer: %s\n", adapter->toString().c_str());
    {
        const LE_Features le_feats = adapter->getLEFeatures();
        fprintf_td(stderr, "initAdapterToServer: LE_Features %s\n", to_string(le_feats).c_str());
    }
    {
        LE_PHYs Tx { LE_PHYs::LE_2M }, Rx { LE_PHYs::LE_2M };
        HCIStatusCode res = adapter->setDefaultLE_PHY(Tx, Rx);
        fprintf_td(stderr, "initAdapterToServer: Set Default LE PHY: status %s: Tx %s, Rx %s\n",
                to_string(res).c_str(), to_string(Tx).c_str(), to_string(Rx).c_str());
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
            fprintf_td(stderr, "****** To Client: SETTINGS_INITIAL: %s -> %s, changed %s\n", to_string(oldmask).c_str(),
                    to_string(newmask).c_str(), to_string(changedmask).c_str());
        } else {
            fprintf_td(stderr, "****** To Client: SETTINGS_CHANGED: %s -> %s, changed %s\n", to_string(oldmask).c_str(),
                    to_string(newmask).c_str(), to_string(changedmask).c_str());
        }
        fprintf_td(stderr, "To Client: Status BTAdapter:\n");
        fprintf_td(stderr, "%s\n", a.toString().c_str());
        (void)timestamp;
    }

    void discoveringChanged(BTAdapter &a, const ScanType currentMeta, const ScanType changedType, const bool changedEnabled, const DiscoveryPolicy policy, const uint64_t timestamp) override {
        fprintf_td(stderr, "****** To Client: DISCOVERING: meta %s, changed[%s, enabled %d, policy %s]: %s\n",
                to_string(currentMeta).c_str(), to_string(changedType).c_str(), changedEnabled, to_string(policy).c_str(), a.toString().c_str());
        (void)timestamp;
    }

    bool deviceFound(BTDeviceRef device, const uint64_t timestamp) override {
        (void)timestamp;

        fprintf_td(stderr, "****** To Client: FOUND__-1: NOP %s\n", device->toString(true).c_str());
        return false;
    }

    void deviceConnected(BTDeviceRef device, const uint16_t handle, const uint64_t timestamp) override {
        fprintf_td(stderr, "****** To Client: CONNECTED: %s\n", device->toString(true).c_str());

        (void)handle;
        (void)timestamp;
    }

    void devicePairingState(BTDeviceRef device, const SMPPairingState state, const PairingMode mode, const uint64_t timestamp) override {
        fprintf_td(stderr, "****** To Client: PAIRING STATE: state %s, mode %s, %s\n",
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
        {
            jau::sc_atomic_critical sync(sync_data);
            connectedDeviceToClient = device;
        }
        fprintf_td(stderr, "****** To Client: READY-0: Processing %s\n", device->toString(true).c_str());
        BTDeviceRegistry::addToProcessingDevices(device->getAddressAndType(), device->getName());
    }

    void deviceDisconnected(BTDeviceRef device, const HCIStatusCode reason, const uint16_t handle, const uint64_t timestamp) override {
        servedClientConnections = servedClientConnections + 1;
        fprintf_td(stderr, "****** DISCONNECTED (count %zu): Reason 0x%X (%s), old handle %s: %s\n",
                servedClientConnections.load(), static_cast<uint8_t>(reason), to_string(reason).c_str(),
                to_hexstring(handle).c_str(), device->toString(true).c_str());

        {
            jau::sc_atomic_critical sync(sync_data);
            connectedDeviceToClient = nullptr;
        }
        std::thread sd(::processDisconnectedDeviceToClient, device); // @suppress("Invalid arguments")
        sd.detach();
        (void)timestamp;
    }

    std::string toString() const override {
        return "MyAdapterServerStatusListener[this "+to_hexstring(this)+"]";
    }

};

static void processDisconnectedDeviceToClient(BTDeviceRef device) {
    fprintf_td(stderr, "****** To Client: Disconnected Device (count %zu): Start %s\n",
            servedClientConnections.load(), device->toString().c_str());

    // already unpaired
    stopAdvertisingToClient(adapterToClient, "processDisconnectedDeviceToClient");
    BTDeviceRegistry::removeFromProcessingDevices(device->getAddressAndType());
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // wait a little (FIXME: Fast restart of advertising error)

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

    fprintf_td(stderr, "****** To Client: Disonnected Device: End %s\n", device->toString().c_str());
}

static bool startAdvertisingToClient(BTAdapterRef a, std::string msg) {
    BTDeviceRef devToServer;
    {
        jau::sc_atomic_critical sync(sync_data);
        devToServer = connectedDeviceToServer;
    }
    if( nullptr == devToServer ) {
        fprintf_td(stderr, "To Client: Start advertising: Skipped, not connected to server\n");
        return false;
    }

    EInfoReport eir = *devToServer->getEIR();
    EIRDataType adv_mask = EIRDataType::FLAGS | EIRDataType::SERVICE_UUID;
    EIRDataType scanrsp_mask = EIRDataType::NAME | EIRDataType::CONN_IVAL;

    DBGattServerRef dbGattServer( new DBGattServer( devToServer ) );
    fprintf_td(stderr, "To Client: Start advertising: GattServer %s\n", dbGattServer->toString().c_str());

    DBGattCharRef gattDevNameChar = dbGattServer->findGattChar( jau::uuid16_t(GattServiceType::GENERIC_ACCESS),
                                                                jau::uuid16_t(GattCharacteristicType::DEVICE_NAME) );
    if( nullptr != gattDevNameChar ) {
        std::string aname = a->getName();
        gattDevNameChar->setValue(reinterpret_cast<uint8_t*>(aname.data()), aname.size(), 0);
    }

    fprintf_td(stderr, "****** To Client: Start advertising (%s): EIR %s\n", msg.c_str(), eir.toString().c_str());
    fprintf_td(stderr, "****** To Client: Start advertising (%s): adv %s, scanrsp %s\n", msg.c_str(), to_string(adv_mask).c_str(), to_string(scanrsp_mask).c_str());

    HCIStatusCode status = a->startAdvertising(dbGattServer, eir, adv_mask, scanrsp_mask,
                                               adv_interval_min, adv_interval_max,
                                               adv_type, adv_chan_map, filter_policy);
    fprintf_td(stderr, "****** To Client: Start advertising (%s) result: %s: %s\n", msg.c_str(), to_string(status).c_str(), a->toString().c_str());
    fprintf_td(stderr, "%s", dbGattServer->toFullString().c_str());
    return HCIStatusCode::SUCCESS == status;
}

static bool stopAdvertisingToClient(BTAdapterRef a, std::string msg) {
    HCIStatusCode status = a->stopAdvertising();
    fprintf_td(stderr, "****** To Client: Stop advertising (%s) result: %s: %s\n", msg.c_str(), to_string(status).c_str(), a->toString().c_str());
    return HCIStatusCode::SUCCESS == status;
}

static bool initAdapterToClient(std::shared_ptr<BTAdapter>& adapter) {
    if( adapterToClientAddr != EUI48::ALL_DEVICE && adapterToClientAddr != adapter->getAddressAndType().address ) {
        fprintf_td(stderr, "initAdapterToClient: Adapter not selected: %s\n", adapter->toString().c_str());
        return false;
    }
    if( !adapter->isInitialized() ) {
        // Initialize with defaults and power-on
        const HCIStatusCode status = adapter->initialize( btMode );
        if( HCIStatusCode::SUCCESS != status ) {
            fprintf_td(stderr, "initAdapterToClient: initialize failed: %s: %s\n",
                    to_string(status).c_str(), adapter->toString().c_str());
            return false;
        }
    } else if( !adapter->setPowered( true ) ) {
        fprintf_td(stderr, "initAdapterToClient: setPower.1 on failed: %s\n", adapter->toString().c_str());
        return false;
    }
    // adapter is powered-on
    fprintf_td(stderr, "initAdapterToClient.1: %s\n", adapter->toString().c_str());

    if( adapter->setPowered(false) ) {
        HCIStatusCode status = adapter->setName(adapterToClientName, adapterToClientShortName);
        if( HCIStatusCode::SUCCESS == status ) {
            fprintf_td(stderr, "initAdapterToClient: setLocalName OK: %s\n", adapter->toString().c_str());
        } else {
            fprintf_td(stderr, "initAdapterToClient: setLocalName failed: %s\n", adapter->toString().c_str());
            return false;
        }

        status = adapter->setSecureConnections( adapterToClientUseSC );
        if( HCIStatusCode::SUCCESS == status ) {
            fprintf_td(stderr, "initAdapterToClient: setSecureConnections OK: %s\n", adapter->toString().c_str());
        } else {
            fprintf_td(stderr, "initAdapterToClient: setSecureConnections failed: %s\n", adapter->toString().c_str());
            return false;
        }

        const uint16_t conn_min_interval = 8;  // 10ms
        const uint16_t conn_max_interval = 40; // 50ms
        const uint16_t conn_latency = 0;
        const uint16_t supervision_timeout = 50; // 500ms
        status = adapter->setDefaultConnParam(conn_min_interval, conn_max_interval, conn_latency, supervision_timeout);
        if( HCIStatusCode::SUCCESS == status ) {
            fprintf_td(stderr, "initAdapterToClient: setDefaultConnParam OK: %s\n", adapter->toString().c_str());
        } else {
            fprintf_td(stderr, "initAdapterToClient: setDefaultConnParam failed: %s\n", adapter->toString().c_str());
            return false;
        }

        if( !adapter->setPowered( true ) ) {
            fprintf_td(stderr, "initAdapterToClient: setPower.2 on failed: %s\n", adapter->toString().c_str());
            return false;
        }
    } else {
        fprintf_td(stderr, "initAdapterToClient: setPowered.2 off failed: %s\n", adapter->toString().c_str());
        return false;
    }
    fprintf_td(stderr, "initAdapterToClient.2: %s\n", adapter->toString().c_str());

    {
        const LE_Features le_feats = adapter->getLEFeatures();
        fprintf_td(stderr, "initAdapterToClient: LE_Features %s\n", to_string(le_feats).c_str());
    }
    {
        LE_PHYs Tx { LE_PHYs::LE_2M }, Rx { LE_PHYs::LE_2M };
        HCIStatusCode res = adapter->setDefaultLE_PHY(Tx, Rx);
        fprintf_td(stderr, "initAdapterToClient: Set Default LE PHY: status %s: Tx %s, Rx %s\n",
                to_string(res).c_str(), to_string(Tx).c_str(), to_string(Rx).c_str());
    }
    adapter->setSMPKeyPath(SERVER_KEY_PATH);

    std::shared_ptr<AdapterStatusListener> asl( std::make_shared<AdapterToClientStatusListener>() );
    adapter->addStatusListener( asl );
    // Flush discovered devices after registering our status listener.
    // This avoids discovered devices before we have registered!
    adapter->removeDiscoveredDevices();

    adapter->setServerConnSecurity(adapterToClientSecLevel, SMPIOCapability::UNSET);

    return true;
}

//
// Common: To Server and Client
//

static bool myChangedAdapterSetFunc(const bool added, std::shared_ptr<BTAdapter>& adapter) {
    if( added ) {
        if( nullptr == adapterToServer ) {
            if( initAdapterToServer( adapter ) ) {
                adapterToServer = adapter;
                fprintf_td(stderr, "****** AdapterToServer ADDED__: InitOK: %s\n", adapter->toString().c_str());
                return true;
            }
        }
        if( nullptr == adapterToClient ) {
            if( initAdapterToClient( adapter ) ) {
                adapterToClient = adapter;
                fprintf_td(stderr, "****** AdapterToClient ADDED__: InitOK: %s\n", adapter->toString().c_str());
                return true;
            }
        }
        fprintf_td(stderr, "****** Adapter ADDED__: Ignored: %s\n", adapter->toString().c_str());
    } else {
        if( nullptr != adapterToServer && adapter == adapterToServer ) {
            adapterToServer = nullptr;
            fprintf_td(stderr, "****** AdapterToServer REMOVED: %s\n", adapter->toString().c_str());
            return true;
        }
        if( nullptr != adapterToClient && adapter == adapterToClient ) {
            adapterToClient = nullptr;
            fprintf_td(stderr, "****** AdapterToClient REMOVED: %s\n", adapter->toString().c_str());
            return true;
        }
        fprintf_td(stderr, "****** Adapter REMOVED: Ignored %s\n", adapter->toString().c_str());
    }
    return true;
}

void test() {
    timestamp_t0 = getCurrentMilliseconds();

    BTManager & mngr = BTManager::get();
    mngr.addChangedAdapterSetCallback(myChangedAdapterSetFunc);

    while( 0 == MAX_SERVED_CONNECTIONS || MAX_SERVED_CONNECTIONS > servedClientConnections ) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }
    adapterToServer = nullptr;
    adapterToClient = nullptr;

    //
    // just a manually controlled pull down to show status, not required
    //
    jau::darray<std::shared_ptr<BTAdapter>> adapterList = mngr.getAdapters();

    jau::for_each_const(adapterList, [](const std::shared_ptr<BTAdapter>& adapter) {
        fprintf_td(stderr, "****** EOL Adapter's Devices - pre close: %s\n", adapter->toString().c_str());
        adapter->printDeviceLists();
    });
    {
        int count = mngr.removeChangedAdapterSetCallback(myChangedAdapterSetFunc);
        fprintf_td(stderr, "****** EOL Removed ChangedAdapterSetCallback %d\n", count);

        mngr.close();
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

    fprintf_td(stderr, "DirectBT Native Version %s (API %s)\n", DIRECT_BT_VERSION, DIRECT_BT_VERSION_API);

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
            fprintf(stderr, "Set sec_level 2 client %s\n", to_string(adapterToClientSecLevel).c_str());
        } else if( !strcmp("-adapterToServer", argv[i]) && argc > (i+1) ) {
            adapterToServerAddr = EUI48( std::string(argv[++i]) );
        } else if( !strcmp("-server", argv[i]) && argc > (i+1) ) {
            std::string addrOrNameSub = std::string(argv[++i]);
            BTDeviceRegistry::addToWaitForDevices( addrOrNameSub );
        } else if( !strcmp("-passkeyToServer", argv[i]) && argc > (i+2) ) {
            const std::string addrOrNameSub(argv[++i]);
            BTSecurityRegistry::Entry* sec = BTSecurityRegistry::getOrCreate(addrOrNameSub);
            sec->passkey = atoi(argv[++i]);
            fprintf(stderr, "Set passkey to server in %s\n", sec->toString().c_str());
        } else if( !strcmp("-seclevelToServer", argv[i]) && argc > (i+2) ) {
            const std::string addrOrNameSub(argv[++i]);
            BTSecurityRegistry::Entry* sec = BTSecurityRegistry::getOrCreate(addrOrNameSub);
            sec->sec_level = to_BTSecurityLevel(atoi(argv[++i]));
            fprintf(stderr, "Set sec_level to server in %s\n", sec->toString().c_str());
        } else if( !strcmp("-iocapToServer", argv[i]) && argc > (i+2) ) {
            const std::string addrOrNameSub(argv[++i]);
            BTSecurityRegistry::Entry* sec = BTSecurityRegistry::getOrCreate(addrOrNameSub);
            sec->io_cap = to_SMPIOCapability(atoi(argv[++i]));
            fprintf(stderr, "Set io_cap to server in %s\n", sec->toString().c_str());
        } else if( !strcmp("-secautoToServer", argv[i]) && argc > (i+2) ) {
            const std::string addrOrNameSub(argv[++i]);
            BTSecurityRegistry::Entry* sec = BTSecurityRegistry::getOrCreate(addrOrNameSub);
            sec->io_cap_auto = to_SMPIOCapability(atoi(argv[++i]));
            fprintf(stderr, "Set SEC AUTO security io_cap to server in %s\n", sec->toString().c_str());
        } else if( !strcmp("-count", argv[i]) && argc > (i+1) ) {
            MAX_SERVED_CONNECTIONS = atoi(argv[++i]);
        }
    }
    fprintf_td(stderr, "pid %d\n", getpid());

    fprintf_td(stderr, "Run with '[-btmode LE|BREDR|DUAL] [-use_sc 0|1] [-count <connection_number>] [-quiet] "
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

    fprintf_td(stderr, "btmode %s\n", to_string(btMode).c_str());
    fprintf_td(stderr, "MAX_SERVED_CONNECTIONS %d\n", MAX_SERVED_CONNECTIONS);
    fprintf_td(stderr, "To Client Settings (acting as server):\n");
    fprintf_td(stderr, "- adapter %s\n", adapterToClientAddr.toString().c_str());
    fprintf_td(stderr, "- SC %s\n", to_string(adapterToClientUseSC).c_str());
    fprintf_td(stderr, "- name %s (short %s)\n", adapterToClientName.c_str(), adapterToClientShortName.c_str());
    fprintf_td(stderr, "- mtu %d\n", (int)max_att_mtu_to_client);
    fprintf_td(stderr, "- sec_level %s\n", to_string(adapterToClientSecLevel).c_str());
    fprintf_td(stderr, "To Server Settings (acting as client):\n");
    fprintf_td(stderr, "- adapter %s\n", adapterToServerAddr.toString().c_str());
    fprintf_td(stderr, "- discoveryPolicy %s\n", to_string(discoveryPolicy).c_str());
    fprintf_td(stderr, "- security-details client: %s\n", BTSecurityRegistry::allToString().c_str());
    fprintf_td(stderr, "- server to connect to: %s\n", BTDeviceRegistry::getWaitForDevicesString().c_str());


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
        BTManager & mngr = BTManager::get(); // already existing
        mngr.close();
        fprintf_td(stderr, "****** Manager close end\n");
    }
}
