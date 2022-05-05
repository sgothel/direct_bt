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
 * - operating w/o encryption
 * - start server advertising
 * - start client discovery and connect to server when discovered
 * - client/server processing of connection when ready
 * - client disconnect
 * - server stop advertising
 * - security-level: NONE, ENC_ONLY freshly-paired and ENC_ONLY pre-paired
 * - reuse server-adapter for client-mode discovery (just toggle on/off)
 */
class TestDBTClientServer10_NoEnc : public DBTClientServer1x {
    private:
        static constexpr const bool serverSC = true;

    public:
        void test00_FullCycle_EncNone() {
            base_test_framework.setupTest( 20_s );
            {
                const ExpectedPairing serverExpPairing = ExpectedPairing::DONT_CARE;
                const ExpectedPairing clientExpPairing = ExpectedPairing::DONT_CARE;
                test8x_fullCycle("10", 1 /* protocolSessionCount */, true /* server_client_order */, serverSC,
                                 BTSecurityLevel::NONE, serverExpPairing, BTSecurityLevel::NONE, clientExpPairing);
            }
            base_test_framework.cleanupTest();
        }

        void test01_FullCycle_EncNone() {
            base_test_framework.setupTest( 30_s );
            {
                const ExpectedPairing serverExpPairing = ExpectedPairing::DONT_CARE;
                const ExpectedPairing clientExpPairing = ExpectedPairing::DONT_CARE;
                test8x_fullCycle("11", 2 /* protocolSessionCount */, true /* server_client_order */, serverSC,
                                 BTSecurityLevel::NONE, serverExpPairing, BTSecurityLevel::NONE, clientExpPairing);
            }
            base_test_framework.cleanupTest();
        }

};

METHOD_AS_TEST_CASE( TestDBTClientServer10_NoEnc::test00_FullCycle_EncNone, "ClientServer 10 NoEnc Trial", "[trial][serverclient][fullcycle][noenc]");
METHOD_AS_TEST_CASE( TestDBTClientServer10_NoEnc::test01_FullCycle_EncNone, "ClientServer 11 NoEnc Trial", "[trial][serverclient][fullcycle][noenc]");
