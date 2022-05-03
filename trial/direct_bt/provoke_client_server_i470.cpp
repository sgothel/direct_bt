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

#define CATCH_CONFIG_RUNNER
// #define CATCH_CONFIG_MAIN
#include <catch2/catch_amalgamated.hpp>
#include <jau/test/catch2_ext.hpp>

#include "dbt_base_client_server.hpp"

#include "dbt_server01.hpp"
#include "dbt_client01.hpp"

using namespace direct_bt;

// Singleton test framework, alive until test program ends
static BaseDBTClientServer& base_test_framework = BaseDBTClientServer::get();

/**
 * Testing w/o client filtering processing device and hence not blocking deviceFound.
 *
 * In other words, relying on BTAdapter to filter out:
 * - already discovered devices
 * - already connected devices
 */
class TestDBTClientServer_i470 {
    private:
        std::mutex mtx_sync;
        BTDeviceRef lastCompletedDevice = nullptr;
        PairingMode lastCompletedDevicePairingMode = PairingMode::NONE;
        BTSecurityLevel lastCompletedDeviceSecurityLevel = BTSecurityLevel::NONE;
        EInfoReport lastCompletedDeviceEIR;

    public:
        void test_i470() {
            const bool serverSC = true;
            const std::string suffix = "i470";
            const int protocolSessionCount = 2;
            const bool server_client_order = true;
            const BTSecurityLevel secLevelServer = BTSecurityLevel::ENC_ONLY;
            const BTSecurityLevel secLevelClient = BTSecurityLevel::ENC_ONLY;

            base_test_framework.setupTest( 20_s );
            {

                const jau::fraction_timespec t0 = jau::getMonotonicTime();

                BTManager & manager = BTManager::get();
                {
                    jau::darray<BTAdapterRef> adapters = manager.getAdapters();
                    jau::fprintf_td(stderr, "Adapter: Count %u\n", adapters.size());

                    for(jau::nsize_t i=0; i<adapters.size(); i++) {
                        jau::fprintf_td(stderr, "%u: %s\n", i, adapters[i]->toString().c_str());
                    }
                    REQUIRE( adapters.size() >= 2 );
                }

                std::shared_ptr<DBTServer01> server = std::make_shared<DBTServer01>("S-"+suffix, EUI48::ALL_DEVICE, BTMode::DUAL, serverSC, secLevelServer);
                server->servingProtocolSessionsLeft = 2* protocolSessionCount;

                std::shared_ptr<DBTClient01> client = std::make_shared<DBTClient01>("C-"+suffix, EUI48::ALL_DEVICE, BTMode::DUAL);

                ChangedAdapterSetCallback myChangedAdapterSetFunc = DBTEndpoint::initChangedAdapterSetListener(manager,
                        server_client_order ? std::vector<DBTEndpointRef>{ server, client } : std::vector<DBTEndpointRef>{ client, server } );

                const std::string serverName = server->getName();
                BTDeviceRegistry::addToWaitForDevices( serverName );
                {
                    BTSecurityRegistry::Entry * sec = BTSecurityRegistry::getOrCreate(serverName);
                    sec->sec_level = secLevelClient;
                }
                client->KEEP_CONNECTED = false; // default, auto-disconnect after work is done
                client->REMOVE_DEVICE = false; // default, test side-effects
                client->measurementsLeft = protocolSessionCount;
                client->discoveryPolicy = DiscoveryPolicy::PAUSE_CONNECTED_UNTIL_DISCONNECTED;

                {
                    const std::lock_guard<std::mutex> lock(mtx_sync); // RAII-style acquire and relinquish via destructor
                    lastCompletedDevice = nullptr;
                    lastCompletedDevicePairingMode = PairingMode::NONE;
                    lastCompletedDeviceSecurityLevel = BTSecurityLevel::NONE;
                    lastCompletedDeviceEIR.clear();
                }
                class MyAdapterStatusListener : public AdapterStatusListener {
                  private:
                    TestDBTClientServer_i470& parent;
                  public:
                    MyAdapterStatusListener(TestDBTClientServer_i470& p) : parent(p) {}

                    void deviceReady(BTDeviceRef device, const uint64_t timestamp) override {
                        (void)timestamp;
                        const std::lock_guard<std::mutex> lock(parent.mtx_sync); // RAII-style acquire and relinquish via destructor
                        parent.lastCompletedDevice = device;
                        parent.lastCompletedDevicePairingMode = device->getPairingMode();
                        parent.lastCompletedDeviceSecurityLevel = device->getConnSecurityLevel();
                        parent.lastCompletedDeviceEIR = *device->getEIR();
                        fprintf_td(stderr, "XXXXXX Client Ready: %s\n", device->toString(true).c_str());
                    }

                    std::string toString() const override { return "DBTClientServer1x::Client"; }
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
                        done = ! ( protocolSessionCount > server->servedProtocolSessionsTotal ||
                                   protocolSessionCount > client->completedMeasurements ||
                                   nullptr == lastCompletedDevice ||
                                   lastCompletedDevice->getConnected() );
                    }
                    max_connections_hit = ( protocolSessionCount * DBTConstants::max_connections_per_session ) <= server->disconnectCount;
                    test_duration = ( jau::getMonotonicTime() - t0 ).to_fraction_i64();
                    timeout = 0_s < timeout_value && timeout_value <= test_duration + 500_ms; // let's timeout here before our timeout timer
                    if( !done && !max_connections_hit && !timeout ) {
                        jau::sleep_for( 88_ms );
                    }
                } while( !done && !max_connections_hit && !timeout );
                test_duration = ( jau::getMonotonicTime() - t0 ).to_fraction_i64();

                fprintf_td(stderr, "\n\n");
                fprintf_td(stderr, "****** Test Stats: duration %" PRIi64 " ms, timeout[hit %d, value %s sec], max_connections hit %d\n",
                        test_duration.to_ms(), timeout, timeout_value.to_string(true).c_str(), max_connections_hit);
                fprintf_td(stderr, "  Server ProtocolSessions[success %d/%d total, requested %d], disconnects %d of %d max\n",
                        server->servedProtocolSessionsSuccess.load(), server->servedProtocolSessionsTotal.load(), protocolSessionCount,
                        server->disconnectCount.load(), ( protocolSessionCount * DBTConstants::max_connections_per_session ));
                fprintf_td(stderr, "  Client ProtocolSessions success %d \n",
                        client->completedMeasurements.load());
                fprintf_td(stderr, "\n\n");

                REQUIRE( false == max_connections_hit );
                REQUIRE( false == timeout );
                {
                    const std::lock_guard<std::mutex> lock(mtx_sync); // RAII-style acquire and relinquish via destructor
                    REQUIRE( protocolSessionCount == server->servedProtocolSessionsTotal );
                    REQUIRE( protocolSessionCount == server->servedProtocolSessionsSuccess );
                    REQUIRE( protocolSessionCount == client->completedMeasurements );
                    REQUIRE( nullptr != lastCompletedDevice );
                    REQUIRE( EIRDataType::NONE != lastCompletedDeviceEIR.getEIRDataMask() );
                    REQUIRE( false == lastCompletedDevice->getConnected() );
                    REQUIRE( ( protocolSessionCount * DBTConstants::max_connections_per_session ) > server->disconnectCount );
                }

                //
                // Client stop
                //
                DBTClientTest::stopDiscovery(client, true /* current_exp_discovering_state */, "test"+suffix+"_stopDiscovery");
                client->close("test"+suffix+"_close");

                //
                // Server stop
                //
                DBTServerTest::stop(server, true /* current_exp_advertising_state */, "test"+suffix+"_stopAdvertising");
                server->close("test"+suffix+"_close");

                //
                // Validating Security Mode
                //
                SMPKeyBin clientKeys = SMPKeyBin::read(DBTConstants::CLIENT_KEY_PATH, *lastCompletedDevice, true /* verbose */);
                REQUIRE( true == clientKeys.isValid() );
                const BTSecurityLevel clientKeysSecLevel = clientKeys.getSecLevel();
                REQUIRE( secLevelClient == clientKeysSecLevel);
                {
                    if( PairingMode::PRE_PAIRED == lastCompletedDevicePairingMode ) {
                        // Using encryption: pre-paired
                        REQUIRE( BTSecurityLevel::ENC_ONLY == lastCompletedDeviceSecurityLevel ); // pre-paired fixed level, no auth
                    } else if( BTSecurityLevel::NONE < secLevelClient ) {
                        // Using encryption: Newly paired
                        REQUIRE( PairingMode::PRE_PAIRED != lastCompletedDevicePairingMode );
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

                const int count = manager.removeChangedAdapterSetCallback(myChangedAdapterSetFunc);
                fprintf_td(stderr, "****** EOL Removed ChangedAdapterSetCallback %d\n", count);
            }
            base_test_framework.cleanupTest();
        }
};

METHOD_AS_TEST_CASE( TestDBTClientServer_i470::test_i470, "ClientServer i470 Trial", "[trial][i470]");
