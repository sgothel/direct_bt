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

#include <iostream>
#include <cassert>
#include <cinttypes>
#include <cstring>

#include "dbt_base_client_server.hpp"

#include "dbt_server01.hpp"
#include "dbt_client01.hpp"

using namespace direct_bt;

// Singleton test framework, alive until test program ends
static BaseDBTClientServer& base_test_framework = BaseDBTClientServer::get();

/**
 * Testing a full Bluetooth server and client lifecycle of operations, requiring two BT adapter:
 * - start server advertising
 * - start client discovery and connect to server when discovered
 * - client/server processing of connection when ready
 * - client disconnect
 * - server stop advertising
 * - security-level: NONE, ENC_ONLY freshly-paired and ENC_ONLY pre-paired
 * - reuse server-adapter for client-mode discovery (just toggle on/off)
 */
class DBTClientServer1x {
  private:
    // timeout check: timeout_value < test_duration + timeout_preempt_diff; // let's timeout here before our timeout timer
    static constexpr const fraction_i64 timeout_preempt_diff = 500_ms;

    std::mutex mtx_sync;
    BTDeviceRef lastCompletedDevice = nullptr;
    PairingMode lastCompletedDevicePairingMode = PairingMode::NONE;
    BTSecurityLevel lastCompletedDeviceSecurityLevel = BTSecurityLevel::NONE;
    EInfoReport lastCompletedDeviceEIR;
    int client_power_down_count = 0;
    int client_power_up_count = 0;
    bool client_reset_at_ready = false;
    bool server_reset_at_ready = false;
    bool client_reset_test = false;
    bool server_reset_test = false;

  public:
    void set_client_reset_at_ready(const bool v) noexcept { client_reset_at_ready = v; client_reset_test = v; }
    void set_server_reset_at_ready(const bool v) noexcept { server_reset_at_ready = v; server_reset_test = v; }

    void test8x_fullCycle(const std::string& suffix, const int protocolSessionCount, const bool server_client_order,
                          const bool serverSC, const BTSecurityLevel secLevelServer, const ExpectedPairing serverExpPairing,
                          const BTSecurityLevel secLevelClient, const ExpectedPairing clientExpPairing)
    {
        std::shared_ptr<DBTServer01> server = std::make_shared<DBTServer01>("S-"+suffix, EUI48::ALL_DEVICE, BTMode::DUAL,
                                                                            serverSC, secLevelServer);
        std::shared_ptr<DBTClient01> client = std::make_shared<DBTClient01>("C-"+suffix, EUI48::ALL_DEVICE, BTMode::DUAL);

        server->setProtocolSessionsLeft( protocolSessionCount );

        client->setProtocolSessionsLeft( protocolSessionCount );
        client->setDisconnectDevice( true ); // default, auto-disconnect after work is done
        client->setRemoveDevice( false ); // default, test side-effects
        client->setDiscoveryPolicy( DiscoveryPolicy::PAUSE_CONNECTED_UNTIL_DISCONNECTED );

        test8x_fullCycle(suffix,
                         DBTConstants::max_connections_per_session, true /* expSuccess */,
                         server_client_order,
                         server, secLevelServer, serverExpPairing,
                         client, secLevelClient, clientExpPairing);
    }

    void test8x_fullCycle(const std::string& suffix,
                          const int max_connections_per_session, const bool expSuccess,
                          const bool server_client_order,
                          const std::shared_ptr<DBTServerTest>& server, const BTSecurityLevel secLevelServer, const ExpectedPairing serverExpPairing,
                          const std::shared_ptr<DBTClientTest>& client, const BTSecurityLevel secLevelClient, const ExpectedPairing clientExpPairing)
    {
        (void)secLevelServer;
        (void)serverExpPairing;

        const int protocolSessionCount = std::min(server->getProtocolSessionsLeft(), client->getProtocolSessionsLeft());
        const jau::fraction_timespec t0 = jau::getMonotonicTime();

        std::shared_ptr<BTManager> manager = BTManager::get();
        {
            jau::darray<BTAdapterRef> adapters = manager->getAdapters();
            jau::fprintf_td(stderr, "Adapter: Count %u\n", adapters.size());

            for(jau::nsize_t i=0; i<adapters.size(); i++) {
                jau::fprintf_td(stderr, "%u: %s\n", i, adapters[i]->toString().c_str());
            }
            REQUIRE( adapters.size() >= 2 );
        }

        ChangedAdapterSetCallback myChangedAdapterSetFunc = DBTEndpoint::initChangedAdapterSetListener(manager,
                server_client_order ? std::vector<DBTEndpointRef>{ server, client } : std::vector<DBTEndpointRef>{ client, server } );

        const std::string serverName = server->getName();
        BTDeviceRegistry::addToWaitForDevices( serverName );
        {
            BTSecurityRegistry::Entry * sec = BTSecurityRegistry::getOrCreate(serverName);
            sec->sec_level = secLevelClient;
        }

        {
            const std::lock_guard<std::mutex> lock(mtx_sync); // RAII-style acquire and relinquish via destructor
            lastCompletedDevice = nullptr;
            lastCompletedDevicePairingMode = PairingMode::NONE;
            lastCompletedDeviceSecurityLevel = BTSecurityLevel::NONE;
            lastCompletedDeviceEIR.clear();
        }
        class MyAdapterStatusListener : public AdapterStatusListener {
          private:
            DBTClientServer1x& parent;
          public:
            MyAdapterStatusListener(DBTClientServer1x& p) : parent(p) {}

            void adapterSettingsChanged(BTAdapter &a, const AdapterSetting oldmask, const AdapterSetting newmask,
                                        const AdapterSetting changedmask, const uint64_t timestamp) override {
                const bool initialSetting = AdapterSetting::NONE == oldmask;
                if( !initialSetting && isAdapterSettingBitSet(changedmask, AdapterSetting::POWERED) ) {
                    const std::lock_guard<std::mutex> lock(parent.mtx_sync); // RAII-style acquire and relinquish via destructor
                    if( isAdapterSettingBitSet(newmask, AdapterSetting::POWERED) ) {
                        ++parent.client_power_up_count;
                    } else {
                        ++parent.client_power_down_count;
                    }
                }
                (void)timestamp;
                (void)a;
            }

            void deviceReady(const BTDeviceRef& device, const uint64_t timestamp) override {
                (void)timestamp;
                const std::lock_guard<std::mutex> lock(parent.mtx_sync); // RAII-style acquire and relinquish via destructor
                parent.lastCompletedDevice = device;
                parent.lastCompletedDevicePairingMode = device->getPairingMode();
                parent.lastCompletedDeviceSecurityLevel = device->getConnSecurityLevel();
                parent.lastCompletedDeviceEIR = *device->getEIR();
                fprintf_td(stderr, "XXXXXX Client Ready: %s\n", device->toString(true).c_str());
                if( parent.client_reset_at_ready ) {
                    parent.client_reset_at_ready = false;
                    fprintf_td(stderr, "XXXXXX Client Reset.0: %s\n", device->toString(true).c_str());
                    const HCIStatusCode rr = device->getAdapter().reset();
                    fprintf_td(stderr, "XXXXXX Client Reset.X: %s: %s\n", direct_bt::to_string(rr).c_str(), device->toString(true).c_str());
                }
            }

            std::string toString() const noexcept override { return "DBTClientServer1x::Client"; }
        };
        std::shared_ptr<AdapterStatusListener> clientAdapterStatusListener = std::make_shared<MyAdapterStatusListener>(*this);
        REQUIRE( true == client->getAdapter()->addStatusListener(clientAdapterStatusListener) );

        //
        // Server start
        //
        DBTEndpoint::checkInitializedState(server);
        DBTServerTest::startAdvertising(server, false /* current_exp_advertising_state */, "test"+suffix+"_startAdvertising");

        //
        // Client start
        //
        DBTEndpoint::checkInitializedState(client);
        DBTClientTest::startDiscovery(client, false /* current_exp_discovering_state */, "test"+suffix+"_startDiscovery");

        BaseDBTClientServer& framework = BaseDBTClientServer::get();
        const jau::fraction_i64 timeout_value = framework.get_timeout_value();
        jau::fraction_i64 test_duration = 0_s;
        bool done = false;
        bool timeout = false;
        bool max_connections_hit = false;
        do {
            {
                const std::lock_guard<std::mutex> lock(mtx_sync); // RAII-style acquire and relinquish via destructor
                done = ! ( protocolSessionCount > server->getProtocolSessionsDoneSuccess() ||
                           protocolSessionCount > client->getProtocolSessionsDoneSuccess() ||
                           nullptr == lastCompletedDevice ||
                           lastCompletedDevice->getConnected() );
            }
            max_connections_hit = ( protocolSessionCount * max_connections_per_session ) <= server->getDisconnectCount();
            test_duration = ( jau::getMonotonicTime() - t0 ).to_fraction_i64();
            timeout = framework.is_timedout() ||
                      ( 0_s < timeout_value && timeout_value <= test_duration + timeout_preempt_diff ); // let's timeout here before our timeout timer
            if( !done && !max_connections_hit && !timeout ) {
                jau::sleep_for( 88_ms );
            }
        } while( !done && !max_connections_hit && !timeout );
        test_duration = ( jau::getMonotonicTime() - t0 ).to_fraction_i64();

        fprintf_td(stderr, "\n\n");
        fprintf_td(stderr, "****** Test Stats: duration %" PRIi64 " ms, timeout[hit %d, value %s sec], max_connections hit %d\n",
                test_duration.to_ms(), timeout, timeout_value.to_string(true).c_str(), max_connections_hit);
        fprintf_td(stderr, "  Server ProtocolSessions[success %d/%d total, requested %d], disconnects %d of %d max\n",
                server->getProtocolSessionsDoneSuccess(), server->getProtocolSessionsDoneTotal(), protocolSessionCount,
                server->getDisconnectCount(), ( protocolSessionCount * max_connections_per_session ));
        fprintf_td(stderr, "  Client ProtocolSessions[success %d/%d total, requested %d], disconnects %d of %d max, power[down %d, up %d]\n",
                client->getProtocolSessionsDoneSuccess(), client->getProtocolSessionsDoneTotal(), protocolSessionCount,
                client->getDisconnectCount(), ( protocolSessionCount * max_connections_per_session ),
                client_power_down_count, client_power_up_count);
        fprintf_td(stderr, "\n\n");

        if( expSuccess ) {
            REQUIRE( false == max_connections_hit );
            REQUIRE( false == timeout );
            {
                const std::lock_guard<std::mutex> lock(mtx_sync); // RAII-style acquire and relinquish via destructor
                REQUIRE( protocolSessionCount <= server->getProtocolSessionsDoneTotal() );
                REQUIRE( protocolSessionCount == server->getProtocolSessionsDoneSuccess() );
                REQUIRE( protocolSessionCount <= client->getProtocolSessionsDoneTotal() );
                REQUIRE( protocolSessionCount == client->getProtocolSessionsDoneSuccess() );
                REQUIRE( nullptr != lastCompletedDevice );
                REQUIRE( EIRDataType::NONE != lastCompletedDeviceEIR.getEIRDataMask() );
                REQUIRE( false == lastCompletedDevice->getConnected() );
                REQUIRE( ( protocolSessionCount * max_connections_per_session ) > server->getDisconnectCount() );
                if( client_reset_test ) {
                    REQUIRE( 1 == client_power_down_count );
                    REQUIRE( 1 == client_power_up_count );
                } else {
                    REQUIRE( 0 == client_power_down_count );
                    REQUIRE( 0 == client_power_up_count );
                }
            }
        }

        //
        // Client stop
        //
        const bool current_exp_discovering_state = expSuccess ? true : client->getAdapter()->isDiscovering();
        DBTClientTest::stopDiscovery(client, current_exp_discovering_state, "test"+suffix+"_stopDiscovery");
        client->close("test"+suffix+"_close");

        //
        // Server stop
        //
        DBTServerTest::stop(server, "test"+suffix+"_stop");

        if( expSuccess ) {
            //
            // Validating Security Mode
            //
            SMPKeyBin clientKeys = SMPKeyBin::read(DBTConstants::CLIENT_KEY_PATH, *lastCompletedDevice, true /* verbose */);
            REQUIRE( true == clientKeys.isValid() );
            const BTSecurityLevel clientKeysSecLevel = clientKeys.getSecLevel();
            REQUIRE( secLevelClient == clientKeysSecLevel);
            {
                if( ExpectedPairing::PREPAIRED == clientExpPairing && BTSecurityLevel::NONE < secLevelClient ) {
                    // Using encryption: pre-paired
                    REQUIRE( PairingMode::PRE_PAIRED == lastCompletedDevicePairingMode );
                    REQUIRE( BTSecurityLevel::ENC_ONLY == lastCompletedDeviceSecurityLevel ); // pre-paired fixed level, no auth
                } else if( ExpectedPairing::NEW_PAIRING == clientExpPairing && BTSecurityLevel::NONE < secLevelClient ) {
                    // Using encryption: Newly paired
                    REQUIRE( PairingMode::PRE_PAIRED != lastCompletedDevicePairingMode );
                    REQUIRE_MSG( "PairingMode client "+to_string(lastCompletedDevicePairingMode)+" not > NONE", PairingMode::NONE < lastCompletedDevicePairingMode );
                    REQUIRE_MSG( "SecurityLevel client "+to_string(lastCompletedDeviceSecurityLevel)+" not >= "+to_string(secLevelClient), secLevelClient <= lastCompletedDeviceSecurityLevel );
                } else if( ExpectedPairing::DONT_CARE == clientExpPairing && BTSecurityLevel::NONE < secLevelClient ) {
                    // Any encryption, any pairing
                    REQUIRE_MSG( "PairingMode client "+to_string(lastCompletedDevicePairingMode)+" not > NONE", PairingMode::NONE < lastCompletedDevicePairingMode );
                    REQUIRE_MSG( "SecurityLevel client "+to_string(lastCompletedDeviceSecurityLevel)+" not >= "+to_string(secLevelClient), secLevelClient <= lastCompletedDeviceSecurityLevel );
                } else {
                    // No encryption: No pairing
                    REQUIRE( PairingMode::NONE == lastCompletedDevicePairingMode );
                    REQUIRE( BTSecurityLevel::NONE == lastCompletedDeviceSecurityLevel );
                }
            }

            //
            // Validating EIR
            //
            {
                const std::lock_guard<std::mutex> lock(mtx_sync); // RAII-style acquire and relinquish via destructor
                fprintf_td(stderr, "lastCompletedDevice.connectedEIR: %s\n", lastCompletedDeviceEIR.toString().c_str());
                REQUIRE( EIRDataType::NONE != lastCompletedDeviceEIR.getEIRDataMask() );
                REQUIRE( true == lastCompletedDeviceEIR.isSet(EIRDataType::FLAGS) );
                REQUIRE( true == lastCompletedDeviceEIR.isSet(EIRDataType::SERVICE_UUID) );
                REQUIRE( true == lastCompletedDeviceEIR.isSet(EIRDataType::NAME) );
                REQUIRE( true == lastCompletedDeviceEIR.isSet(EIRDataType::CONN_IVAL) );
                REQUIRE( serverName == lastCompletedDeviceEIR.getName() );
                {
                    const EInfoReport eir = *lastCompletedDevice->getEIR();
                    fprintf_td(stderr, "lastCompletedDevice.currentEIR: %s\n", eir.toString().c_str());
                    REQUIRE( EIRDataType::NONE == eir.getEIRDataMask() );
                    REQUIRE( 0 == eir.getName().length());
                }
            }

            //
            // Now reuse adapter for client mode -> Start discovery + Stop Discovery
            //
            {
                const BTAdapterRef adapter = server->getAdapter();
                { // if( false ) {
                    adapter->removeAllStatusListener();
                }

                DBTEndpoint::startDiscovery(adapter, false /* current_exp_discovering_state */);

                DBTEndpoint::stopDiscovery(adapter, true /* current_exp_discovering_state */);
            }
        }
        const BTManager::size_type count = manager->removeChangedAdapterSetCallback(myChangedAdapterSetFunc);
        fprintf_td(stderr, "****** EOL Removed ChangedAdapterSetCallback %zu\n", (size_t)count);
    }
};
