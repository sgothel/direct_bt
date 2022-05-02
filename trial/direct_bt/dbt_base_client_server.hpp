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

#include "dbt_server00.hpp"
#include "dbt_client00.hpp"

class BaseDBTClientServer {
    private:
        static constexpr const bool DEBUG = true;

        BaseDBTClientServer() noexcept {
            if( DEBUG ) {
                setenv("direct_bt.debug", "true", 1 /* overwrite */);
                // setenv("direct_bt.debug", "true,gatt.data", 1 /* overwrite */);
            }

            REQUIRE( DBTUtils::rmKeyFolder() );
            REQUIRE( DBTUtils::mkdirKeyFolder() );
        }

        void close() {
            BTManager& manager = BTManager::get();
            if( nullptr != manager ) {
                jau::darray<BTAdapterRef> adapters = manager.getAdapters();
                for(BTAdapterRef a : adapters) {
                    a->stopAdvertising();
                    a->stopDiscovery();
                    a->setPowered(false);
                }
                // All implicit via destructor or shutdown hook!
                manager.close(); /* implies: adapter.close(); */
            }
        }

    public:
        static BaseDBTClientServer& get() {
            static BaseDBTClientServer s;
            return s;
        }

        /**
         * Ensure
         * - all adapter are powered off
         * - manager being shutdown
         */
        ~BaseDBTClientServer() noexcept { close(); }

        /**
         * Ensure
         * - all adapter are powered off
         */
        void setupTest() {
            BTManager& manager = BTManager::get();
            if( nullptr != manager ) {
                jau::darray<BTAdapterRef> adapters = manager.getAdapters();
                for(BTAdapterRef a : adapters) {
                    a->stopAdvertising();
                    a->stopDiscovery();
                    REQUIRE( a->setPowered(false) );
                }
            }
            BTDeviceRegistry::clearWaitForDevices();
            BTDeviceRegistry::clearProcessedDevices();
            BTDeviceRegistry::clearProcessingDevices();
            BTSecurityRegistry::clear();
        }

        /**
         * Ensure
         * - remove all status listener from all adapter
         * - all adapter are powered off
         * - clear BTDeviceRegistry
         * - clear BTSecurityRegistry
         */
        void cleanupTest() {
            BTManager& manager = BTManager::get();
            if( nullptr != manager ) {
                jau::darray<BTAdapterRef> adapters = manager.getAdapters();
                for(BTAdapterRef a : adapters) {
                    { // if( false ) {
                        a->removeAllStatusListener();
                    }
                    a->stopAdvertising();
                    a->stopDiscovery();
                    REQUIRE( a->setPowered(false) );
                }
            }
            BTDeviceRegistry::clearWaitForDevices();
            BTDeviceRegistry::clearProcessedDevices();
            BTDeviceRegistry::clearProcessingDevices();
            BTSecurityRegistry::clear();
        }
};

#endif /* DBT_BASE_CLIENY_SERVER_HPP_ */
