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

// #include "dbt_server01.hpp"
// #include "dbt_client01.hpp"

using namespace direct_bt;

/**
 * Testing w/o client filtering processing device and hence not blocking deviceFound.
 *
 * In other words, relying on BTAdapter to filter out:
 * - already discovered devices
 * - already connected devices
 *
 * Further, the server will issue a disconnect once only 300 ms after 1st MTU exchange,
 * disrupting the client's getGATTServices().
 */
class TestDBTClientServer_i470 : public DBTClientServer1x {
    public:
        void test_i470_a() {
            base_test_framework.setupTest( 10_s );
            {
                const bool serverSC = true;
                const std::string suffix = "i470_a";
                const int protocolSessionCount = 10;
                const int max_connections_per_session = 200;
                const bool expSuccess = false;
                const bool server_client_order = true;
                const BTSecurityLevel secLevelServer = BTSecurityLevel::ENC_ONLY;
                const BTSecurityLevel secLevelClient = BTSecurityLevel::ENC_ONLY;
                const ExpectedPairing serverExpPairing = ExpectedPairing::DONT_CARE;
                const ExpectedPairing clientExpPairing = ExpectedPairing::DONT_CARE;
                const bool client_do_disconnect = true;
                const bool server_do_disconnect = false;

                std::shared_ptr<DBTServerTest> server = std::make_shared<DBTServer01>("S-"+suffix, EUI48::ALL_DEVICE, BTMode::DUAL, serverSC, secLevelServer, server_do_disconnect);
                std::shared_ptr<DBTClientTest> client = std::make_shared<DBTClient01>("C-"+suffix, EUI48::ALL_DEVICE, BTMode::DUAL, client_do_disconnect);

                test8x_fullCycle(suffix,
                                 protocolSessionCount, max_connections_per_session, expSuccess,
                                 server_client_order,
                                 server, secLevelServer, serverExpPairing,
                                 client, secLevelClient, clientExpPairing);
            }
            base_test_framework.cleanupTest();
        }

        void test_i470_b() {
            base_test_framework.setupTest( 10_s );
            {
                const bool serverSC = true;
                const std::string suffix = "i470_b";
                const int protocolSessionCount = 10;
                const int max_connections_per_session = 200;
                const bool expSuccess = false;
                const bool server_client_order = true;
                const BTSecurityLevel secLevelServer = BTSecurityLevel::ENC_ONLY;
                const BTSecurityLevel secLevelClient = BTSecurityLevel::ENC_ONLY;
                const ExpectedPairing serverExpPairing = ExpectedPairing::DONT_CARE;
                const ExpectedPairing clientExpPairing = ExpectedPairing::DONT_CARE;
                const bool client_do_disconnect = false;
                const bool server_do_disconnect = true;

                std::shared_ptr<DBTServerTest> server = std::make_shared<DBTServer01>("S-"+suffix, EUI48::ALL_DEVICE, BTMode::DUAL, serverSC, secLevelServer, server_do_disconnect);
                std::shared_ptr<DBTClientTest> client = std::make_shared<DBTClient01>("C-"+suffix, EUI48::ALL_DEVICE, BTMode::DUAL, client_do_disconnect);

                test8x_fullCycle(suffix,
                                 protocolSessionCount, max_connections_per_session, expSuccess,
                                 server_client_order,
                                 server, secLevelServer, serverExpPairing,
                                 client, secLevelClient, clientExpPairing);
            }
            base_test_framework.cleanupTest();
        }
};

METHOD_AS_TEST_CASE( TestDBTClientServer_i470::test_i470_a, "ClientServer i470 Trial a", "[trial][i470]");
METHOD_AS_TEST_CASE( TestDBTClientServer_i470::test_i470_b, "ClientServer i470 Trial b", "[trial][i470]");
