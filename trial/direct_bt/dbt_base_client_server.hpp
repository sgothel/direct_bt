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

#ifndef DBT_BASE_CLIENY_SERVER_HPP_
#define DBT_BASE_CLIENY_SERVER_HPP_

#include "dbt_utils.hpp"

#include "dbt_constants.hpp"

#include <direct_bt/DirectBT.hpp>

#include <jau/simple_timer.hpp>

using namespace direct_bt;

class BaseDBTClientServer {
    private:
        static constexpr const bool debug = false;

        BTManagerRef manager;

        jau::fraction_i64 test_timeout = 0_s;

        jau::simple_timer timeout_timer = jau::simple_timer("DBTTrial-Timeout", 1_s /* shutdown timeout */);
        jau::sc_atomic_bool timedout = false;

        jau::fraction_i64 timeout_func(jau::simple_timer& timer) {
            if( !timer.shall_stop() ) {
                fprintf(stderr, "\n***** DBTTrial Error: Timeout %s sec -> abort *****\n\n", test_timeout.to_string(true).c_str());
                timedout = true;
            }
            return 0_s;
        }

        BaseDBTClientServer(const bool btmanager_hold_and_close) noexcept {
            if( debug ) {
                setenv("direct_bt.debug", "true", 1 /* overwrite */);
                // setenv("direct_bt.debug", "true,gatt.data", 1 /* overwrite */);
            }

            DBTUtils::rmKeyFolder();
            DBTUtils::mkdirKeyFolder();

            if( btmanager_hold_and_close ) {
                manager = BTManager::get();
            }
        }

        void close() {
            timeout_timer.stop();
            if( nullptr != manager ) {
                manager->close();
            }
        }

    public:
        static BaseDBTClientServer& get(const bool btmanager_hold_and_close = true) {
            MgmtEnv::get(); // initialize the environment first for 'direct_bt' key
            static BaseDBTClientServer s(btmanager_hold_and_close);
            return s;
        }

        jau::fraction_i64 get_timeout_value() const { return test_timeout; }

        bool is_timedout() const noexcept { return timedout; }

        /**
         * Ensure
         * - all adapter are powered off
         * - manager being shutdown
         */
        ~BaseDBTClientServer() noexcept { close(); }

    private:
        void resetStates() {
            std::shared_ptr<BTManager> mngr = BTManager::get();
            jau::darray<BTAdapterRef> adapters = mngr->getAdapters();
            for(BTAdapterRef a : adapters) {
                a->removeAllStatusListener();
                a->stopAdvertising();
                a->stopDiscovery();
                REQUIRE( a->setPowered(false) );
            }
            mngr->removeAllChangedAdapterSetCallbacks();
            BTDeviceRegistry::clearWaitForDevices();
            BTDeviceRegistry::clearProcessedDevices();
            BTSecurityRegistry::clear();
        }

    public:
        /**
         * Ensure
         * - all adapter are powered off
         */
        void setupTest(const jau::fraction_i64 timeout = 0_s) {
            timeout_timer.stop();
            test_timeout = timeout;
            resetStates();
            if( !timeout.is_zero() ) {
                timeout_timer.start(timeout, jau::bind_member(this, &BaseDBTClientServer::timeout_func));
            }
        }

        /**
         * Ensure
         * - remove all status listener from all adapter
         * - all adapter are powered off
         * - clear BTDeviceRegistry
         * - clear BTSecurityRegistry
         */
        void cleanupTest() {
            timeout_timer.stop();
            test_timeout = 0_s;
            resetStates();
        }

        void closeBTManager() {
            std::shared_ptr<BTManager> mngr = BTManager::get();
            mngr->close();
        }
};

#endif /* DBT_BASE_CLIENY_SERVER_HPP_ */
