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

#include <jau/test/catch2_ext.hpp>

#include "dbt_client_server1x.hpp"

using namespace direct_bt;

/**
 * Testing a full Bluetooth server and client lifecycle of operations including adapter reset, requiring two BT adapter:
 * - trigger client adapter reset
 * - operating w/o encryption
 * - start server advertising
 * - start client discovery and connect to server when discovered
 * - client/server processing of connection when ready
 * - client disconnect
 * - server stop advertising
 * - security-level: NONE, ENC_ONLY freshly-paired and ENC_ONLY pre-paired
 * - reuse server-adapter for client-mode discovery (just toggle on/off)
 */
class TestDBTClientServer40_Reset : public DBTClientServer1x {
    private:
        static constexpr const bool serverSC = true;

    public:
        void test40_ClientReset01() {
            base_test_framework.setupTest( 20_s );
            {
                const std::string suffix = "40";
                const int protocolSessionCount = 1;
                const int max_connections_per_session = DBTConstants::max_connections_per_session;
                const bool expSuccess = true;
                const bool server_client_order = true;
                const BTSecurityLevel secLevelServer = BTSecurityLevel::NONE;
                const BTSecurityLevel secLevelClient = BTSecurityLevel::NONE;
                const ExpectedPairing serverExpPairing = ExpectedPairing::DONT_CARE;
                const ExpectedPairing clientExpPairing = ExpectedPairing::DONT_CARE;

                std::shared_ptr<DBTServerTest> server = std::make_shared<DBTServer01>("S-"+suffix, EUI48::ALL_DEVICE, BTMode::DUAL, serverSC, secLevelServer);
                std::shared_ptr<DBTClientTest> client = std::make_shared<DBTClient01>("C-"+suffix, EUI48::ALL_DEVICE, BTMode::DUAL);

                server->setProtocolSessionsLeft( protocolSessionCount );

                client->setProtocolSessionsLeft( protocolSessionCount );
                client->setDisconnectDevice( true ); // default, auto-disconnect after work is done
                client->setRemoveDevice( false ); // default, test side-effects
                client->setDiscoveryPolicy( DiscoveryPolicy::PAUSE_CONNECTED_UNTIL_DISCONNECTED );

                set_client_reset_at_ready(true);

                test8x_fullCycle(suffix,
                                 max_connections_per_session, expSuccess,
                                 server_client_order,
                                 server, secLevelServer, serverExpPairing,
                                 client, secLevelClient, clientExpPairing);
            }
            base_test_framework.cleanupTest();
        }

};

METHOD_AS_TEST_CASE( TestDBTClientServer40_Reset::test40_ClientReset01, "ClientServer 40 Reset Trial", "[trial][serverclient][reset]");
