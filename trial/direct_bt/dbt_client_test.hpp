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

#ifndef DBT_CLIENT_TEST_HPP_
#define DBT_CLIENT_TEST_HPP_

#include "dbt_endpoint.hpp"

class DBTClientTest;
typedef std::shared_ptr<DBTClientTest> DBTClientTestRef;

class DBTClientTest : public DBTEndpoint {

    public:
        /**
         * Set DiscoveryPolicy
         *
         * Default is {@link DiscoveryPolicy#PAUSE_CONNECTED_UNTIL_READY}
         */
        virtual void setDiscoveryPolicy(const DiscoveryPolicy v) = 0;

        /**
         * Set disconnect after processing.
         *
         * Default is `false`.
         */
        virtual void setKeepConnected(const bool v) = 0;

        /**
         * Set remove device when disconnecting.
         *
         * This removes the device from all instances within adapter
         * and hence all potential side-effects of the current instance.
         *
         * Default is `false`, since it is good to test whether such side-effects exists.
         */
        virtual void setRemoveDevice(const bool v) = 0;

        virtual HCIStatusCode startDiscovery(const std::string& msg) = 0;

        virtual HCIStatusCode stopDiscovery(const std::string& msg) = 0;

        static void startDiscovery(DBTClientTestRef client, const bool current_exp_discovering_state, const std::string& msg) {
            BTAdapterRef adapter = client->getAdapter();
            REQUIRE( false == adapter->isAdvertising() );
            REQUIRE( current_exp_discovering_state == adapter->isDiscovering() );

            REQUIRE( HCIStatusCode::SUCCESS == client->startDiscovery(msg) );
            while( !adapter->isDiscovering() ) { // pending action
                jau::sleep_for( 100_ms );
            }
            REQUIRE( false == adapter->isAdvertising() );
            REQUIRE( true == adapter->isDiscovering() );
            REQUIRE( BTRole::Master == adapter->getRole() );
        }

        static void stopDiscovery(DBTClientTestRef client, const bool current_exp_discovering_state, const std::string& msg) {
            BTAdapterRef adapter = client->getAdapter();
            REQUIRE( false == adapter->isAdvertising() );
            REQUIRE( current_exp_discovering_state == adapter->isDiscovering() );
            REQUIRE( BTRole::Master == adapter->getRole() );

            REQUIRE( HCIStatusCode::SUCCESS == client->stopDiscovery(msg) );
            while( adapter->isDiscovering() ) { // pending action
                jau::sleep_for( 100_ms );
            }
            REQUIRE( false == adapter->isAdvertising() );
            REQUIRE( false == adapter->isDiscovering() );
            REQUIRE( BTRole::Master == adapter->getRole() );
        }
};

#endif /* #define DBT_CLIENT_TEST_HPP_ */
