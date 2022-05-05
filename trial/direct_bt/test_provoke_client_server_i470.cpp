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

// #include "dbt_base_client_server.hpp"
#include "dbt_client_server1x.hpp"

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
 *
 * Further, the server will issue a disconnect once only 300 ms after 1st MTU exchange,
 * disrupting the client's getGATTServices().
 */
class TestDBTClientServer_i470 : public DBTClientServer1x {
    public:
        void test_i470() {
            base_test_framework.setupTest( 20_s );
            {
                const bool serverSC = true;
                const std::string suffix = "i470";
                const int protocolSessionCount = 2;
                const bool server_client_order = true;
                const BTSecurityLevel secLevelServer = BTSecurityLevel::ENC_ONLY;
                const BTSecurityLevel secLevelClient = BTSecurityLevel::ENC_ONLY;
                const ExpectedPairing serverExpPairing = ExpectedPairing::DONT_CARE;
                const ExpectedPairing clientExpPairing = ExpectedPairing::DONT_CARE;

                // std::shared_ptr<DBTServerTest> server = std::make_shared<DBTServer01>("S-"+suffix, EUI48::ALL_DEVICE, BTMode::DUAL, serverSC, secLevelServer);
                std::shared_ptr<DBTServerTest> server = std::make_shared<DBTServer00>("S-"+suffix, EUI48::ALL_DEVICE, BTMode::DUAL, serverSC, secLevelServer);
                std::shared_ptr<DBTClientTest> client = std::make_shared<DBTClient01>("C-"+suffix, EUI48::ALL_DEVICE, BTMode::DUAL);
                test8x_fullCycle(suffix, protocolSessionCount, server_client_order,
                                 server, secLevelServer, serverExpPairing,
                                 client, secLevelClient, clientExpPairing);
            }
            base_test_framework.cleanupTest();
        }
};

METHOD_AS_TEST_CASE( TestDBTClientServer_i470::test_i470, "ClientServer i470 Trial", "[trial][i470]");
