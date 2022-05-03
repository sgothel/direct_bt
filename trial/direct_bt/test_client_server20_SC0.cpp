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

#include "dbt_client_server1x.hpp"

using namespace direct_bt;

// Singleton test framework, alive until test program ends
static BaseDBTClientServer& base_test_framework = BaseDBTClientServer::get();

/**
 * Testing a full Bluetooth server and client lifecycle of operations, requiring two BT adapter:
 * - operating in legacy non SC mode
 * - start server advertising
 * - start client discovery and connect to server when discovered
 * - client/server processing of connection when ready
 * - client disconnect
 * - server stop advertising
 * - security-level: NONE, ENC_ONLY freshly-paired and ENC_ONLY pre-paired
 * - reuse server-adapter for client-mode discovery (just toggle on/off)
 */
class TestDBTClientServer20_SC0 : public DBTClientServer1x {
    private:
        static constexpr const bool serverSC = false;

    public:
        void test10_FullCycle_EncOnlyNo1() {
            base_test_framework.setupTest( 40_s );
            {
                const bool serverShallHaveKeys = false;
                const bool clientShallHaveKeys = false;
                test8x_fullCycle("20", 1 /* protocolSessionCount */, true /* server_client_order */, serverSC,
                                 BTSecurityLevel::ENC_ONLY, serverShallHaveKeys, BTSecurityLevel::ENC_ONLY, clientShallHaveKeys);
            }
            base_test_framework.cleanupTest();
        }

        void test20_FullCycle_EncOnlyNo2() {
            base_test_framework.setupTest( 40_s );
            {
                const bool serverShallHaveKeys = true;
                const bool clientShallHaveKeys = true;
                test8x_fullCycle("21", 2 /* protocolSessionCount */, true /* server_client_order */, serverSC,
                                 BTSecurityLevel::ENC_ONLY, serverShallHaveKeys, BTSecurityLevel::ENC_ONLY, clientShallHaveKeys);
            }
            base_test_framework.cleanupTest();
        }

};

METHOD_AS_TEST_CASE( TestDBTClientServer20_SC0::test10_FullCycle_EncOnlyNo1, "ClientServer 20 SC0 EncOnly Trial", "[trial][serverclient][fullcycle][sc0][enconly][newpairing]");
METHOD_AS_TEST_CASE( TestDBTClientServer20_SC0::test20_FullCycle_EncOnlyNo2, "ClientServer 21 SC0 EncOnly Trial", "[trial][serverclient][fullcycle][sc0][enconly][prepaired]");
