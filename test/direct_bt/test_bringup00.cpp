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

#include <direct_bt/DirectBT.hpp>

using namespace direct_bt;

void resetStates() {
    std::shared_ptr<BTManager> mngr = BTManager::get();
    jau::darray<BTAdapterRef> adapters = mngr->getAdapters();
    for(BTAdapterRef a : adapters) {
        a->removeAllStatusListener();
        // test runs w/o elevated permissions
        // a->stopAdvertising();
        // a->stopDiscovery();
        // REQUIRE( a->setPowered(false) );
    }
    mngr->removeAllChangedAdapterSetCallbacks();
    BTDeviceRegistry::clearWaitForDevices();
    BTDeviceRegistry::clearProcessedDevices();
    BTSecurityRegistry::clear();
}

/**
 * Testing BTManager bring up:
 * - test loading native libraries
 * - show all installed adapter
 * - no extra permissions required
 */
TEST_CASE( "BTManager Bringup Test 00", "[test][BTManager][bringup]" ) {
    {
        // setenv("direct_bt.debug", "true", 1 /* overwrite */);
    }
    jau::fprintf_td(stderr, "Direct-BT Native Version %s (API %s)\n", DIRECT_BT_VERSION, DIRECT_BT_VERSION_API);
    resetStates();

    BTManagerRef manager = BTManager::get();

    jau::darray<BTAdapterRef> adapters = manager->getAdapters();
    jau::fprintf_td(stderr, "Adapter: Count %u\n", adapters.size());

    for(jau::nsize_t i=0; i<adapters.size(); i++) {
        jau::fprintf_td(stderr, "%u: %s\n", i, adapters[i]->toString().c_str());
    }
    jau::fprintf_td(stderr, "Adapter: Status Checks\n");
    for(BTAdapterRef a : adapters) {
        // test runs w/o elevated permissions
        REQUIRE( false == a->isInitialized() );
        // REQUIRE( false == a->isPowered() );
        REQUIRE( BTRole::Master == a->getRole() ); // default role
        REQUIRE( 4 <= a->getBTMajorVersion() );
    }

    jau::fprintf_td(stderr, "Manager: Closing\n");
    adapters.clear();
    resetStates();
    manager->close(); /* implies: adapter.close(); */

    jau::fprintf_td(stderr, "Test: Done\n");
}
