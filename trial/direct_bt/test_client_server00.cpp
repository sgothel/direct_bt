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

#define CATCH_CONFIG_RUNNER
// #define CATCH_CONFIG_MAIN
#include <catch2/catch_amalgamated.hpp>
#include <jau/test/catch2_ext.hpp>

#include "dbt_base_client_server.hpp"

#include "dbt_server00.hpp"

using namespace direct_bt;

// Singleton test framework, alive until test program ends
static BaseDBTClientServer& base_test_framework = BaseDBTClientServer::get();

/**
 * Testing BTManager bring up:
 * - test loading native libraries
 * - test that at least one adapter are present
 * - validating basic default adapter status
 */
TEST_CASE( "BTManager Bringup Trial 00", "[trial][BTManager][bringup]" ) {
    base_test_framework.setupTest( 5_s );

    jau::fprintf_td(stderr, "Direct-BT Native Version %s (API %s)\n", DIRECT_BT_VERSION, DIRECT_BT_VERSION_API);

    BTManager & manager = BTManager::get();
    jau::darray<BTAdapterRef> adapters = manager.getAdapters();
    {
        jau::fprintf_td(stderr, "Adapter: Count %u\n", adapters.size());

        for(jau::nsize_t i=0; i<adapters.size(); i++) {
            jau::fprintf_td(stderr, "%u: %s\n", i, adapters[i]->toString().c_str());
        }
        REQUIRE( adapters.size() >= 1 );
    }

    jau::fprintf_td(stderr, "Adapter: Status Checks\n");
    for(BTAdapterRef a : adapters) {
        REQUIRE( false == a->isInitialized() );
        REQUIRE( false == a->isPowered() );
        REQUIRE( BTRole::Master == a->getRole() ); // default role
        REQUIRE( 4 <= a->getBTMajorVersion() );
    }

    base_test_framework.cleanupTest();
}

/**
 * Testing BTManager bring up:
 * - test loading native libraries
 * - test that at least one adapter are present
 * - validating basic default adapter status
 */
TEST_CASE( "Server StartStop and SwitchRole Trial 10", "[trial][startstop][switchrole]" ) {
    base_test_framework.setupTest( 5_s );

    BTManager & manager = BTManager::get();
    {
        jau::darray<BTAdapterRef> adapters = manager.getAdapters();
        jau::fprintf_td(stderr, "Adapter: Count %u\n", adapters.size());

        for(jau::nsize_t i=0; i<adapters.size(); i++) {
            jau::fprintf_td(stderr, "%u: %s\n", i, adapters[i]->toString().c_str());
        }
        REQUIRE( adapters.size() >= 1 );
    }
    REQUIRE( manager.getAdapterCount() >= 1 );

    const std::string serverName = "TestDBTCS00-S-T10";
    std::shared_ptr<DBTServer00> server = std::make_shared<DBTServer00>(serverName, EUI48::ALL_DEVICE, BTMode::DUAL, true /* SC */, BTSecurityLevel::NONE);
    server->setProtocolSessionsLeft(1);

    ChangedAdapterSetCallback myChangedAdapterSetFunc = DBTEndpoint::initChangedAdapterSetListener(manager, { server });

    //
    // Server start
    //
    DBTEndpoint::checkInitializedState(server);
    DBTServerTest::startAdvertising(server, false /* current_exp_advertising_state */, "test10_startAdvertising");

    //
    // Server stop
    //
    DBTServerTest::stop(server, true /* current_exp_advertising_state */, "test10_stopAdvertising");
    server->close("test10_close");

    //
    // Now reuse adapter for client mode -> Start discovery + Stop Discovery
    //
    {
        BTAdapterRef adapter = server->getAdapter();
        { // if( false ) {
            adapter->removeAllStatusListener();
        }

        DBTEndpoint::startDiscovery(adapter, false /* current_exp_discovering_state */);

        DBTEndpoint::stopDiscovery(adapter, true /* current_exp_discovering_state */);
    }

    REQUIRE( 1 == manager.removeChangedAdapterSetCallback(myChangedAdapterSetFunc) );

    base_test_framework.cleanupTest();
}

