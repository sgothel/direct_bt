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

using namespace direct_bt;

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
    std::mutex mtx_sync;
    BTDeviceRef lastCompletedDevice = nullptr;
    PairingMode lastCompletedDevicePairingMode = PairingMode::NONE;
    BTSecurityLevel lastCompletedDeviceSecurityLevel = BTSecurityLevel::NONE;
    EInfoReport lastCompletedDeviceEIR;

  public:
    void test8x_fullCycle(const std::string& suffix, const bool server_client_order,
                          const bool serverSC, const BTSecurityLevel secLevelServer, const bool serverShallHaveKeys,
                          const BTSecurityLevel secLevelClient, const bool clientShallHaveKeys)
    {
        (void)serverShallHaveKeys;

        BTManager & manager = BTManager::get();
        {
            jau::darray<BTAdapterRef> adapters = manager.getAdapters();
            jau::fprintf_td(stderr, "Adapter: Count %u\n", adapters.size());

            for(jau::nsize_t i=0; i<adapters.size(); i++) {
                jau::fprintf_td(stderr, "%u: %s\n", i, adapters[i]->toString().c_str());
            }
            REQUIRE( adapters.size() >= 2 );
        }

        std::shared_ptr<DBTServer00> server = std::make_shared<DBTServer00>("S-"+suffix, EUI48::ALL_DEVICE, BTMode::DUAL, serverSC, secLevelServer);
        server->servingConnectionsLeft = 1;

        std::shared_ptr<DBTClient00> client = std::make_shared<DBTClient00>("C-"+suffix, EUI48::ALL_DEVICE, BTMode::DUAL);

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
        client->measurementsLeft = 1;
        client->discoveryPolicy = DiscoveryPolicy::PAUSE_CONNECTED_UNTIL_DISCONNECTED;

        lastCompletedDevice = nullptr;
        lastCompletedDevicePairingMode = PairingMode::NONE;
        lastCompletedDeviceSecurityLevel = BTSecurityLevel::NONE;
        lastCompletedDeviceEIR.clear();
        class MyAdapterStatusListener : public AdapterStatusListener {
          private:
            DBTClientServer1x& parent;
          public:
            MyAdapterStatusListener(DBTClientServer1x& p) : parent(p) {}

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

        bool done = false;
        do {
            {
                const std::lock_guard<std::mutex> lock(mtx_sync); // RAII-style acquire and relinquish via destructor
                done = ! ( 1 > server->servedConnections ||
                           1 > client->completedMeasurements ||
                           nullptr == lastCompletedDevice ||
                           lastCompletedDevice->getConnected() );
            }
            if( !done ) {
                jau::sleep_for( 88_ms );
            }

        } while( !done );
        {
            const std::lock_guard<std::mutex> lock(mtx_sync); // RAII-style acquire and relinquish via destructor
            REQUIRE( 1 == server->servedConnections );
            REQUIRE( 1 == client->completedMeasurements );
            REQUIRE( nullptr != lastCompletedDevice );
            REQUIRE( EIRDataType::NONE != lastCompletedDeviceEIR.getEIRDataMask() );
            REQUIRE( false == lastCompletedDevice->getConnected() );
        }

        //
        // Client stop
        //
        DBTClientTest::stopDiscovery(client, true /* current_exp_discovering_state */, "test"+suffix+"_stopDiscovery");
        client->close("test"+suffix+"_close");

        //
        // Server stop
        //
        DBTServerTest::stop(server, false /* current_exp_advertising_state */, "test"+suffix+"_stopAdvertising");
        server->close("test"+suffix+"_close");

        //
        // Validating Security Mode
        //
        SMPKeyBin clientKeys = SMPKeyBin::read(DBTConstants::CLIENT_KEY_PATH, *lastCompletedDevice, true /* verbose */);
        REQUIRE( true == clientKeys.isValid() );
        const BTSecurityLevel clientKeysSecLevel = clientKeys.getSecLevel();
        REQUIRE( secLevelClient == clientKeysSecLevel);
        {
            if( clientShallHaveKeys ) {
                // Using encryption: pre-paired
                REQUIRE( PairingMode::PRE_PAIRED == lastCompletedDevicePairingMode );
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

        const int count = manager.removeChangedAdapterSetCallback(myChangedAdapterSetFunc);
        fprintf_td(stderr, "****** EOL Removed ChangedAdapterSetCallback %d\n", count);
    }
};
