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

#ifndef DBT_SERVER_TEST_HPP_
#define DBT_SERVER_TEST_HPP_

#include "dbt_endpoint.hpp"

class DBTServerTest;
typedef std::shared_ptr<DBTServerTest> DBTServerTestRef;

class DBTServerTest : public DBTEndpoint {

    public:
        virtual BTSecurityLevel getSecurityLevel() = 0;

        virtual HCIStatusCode startAdvertising(const std::string& msg) = 0;

        static void startAdvertising(const DBTServerTestRef& server, const bool current_exp_advertising_state, const std::string& msg) {
            BTAdapterRef adapter = server->getAdapter();
            REQUIRE( current_exp_advertising_state == adapter->isAdvertising() );
            REQUIRE( false == adapter->isDiscovering() );

            REQUIRE( HCIStatusCode::SUCCESS == server->startAdvertising(msg) );
            REQUIRE( true == adapter->isAdvertising() );
            REQUIRE( false == adapter->isDiscovering() );
            REQUIRE( BTRole::Slave == adapter->getRole() );
            REQUIRE( server->getName() == adapter->getName() );
        }

        static void stop(const DBTServerTestRef& server, const std::string& msg) {
            BTAdapterRef adapter = server->getAdapter();
            REQUIRE( false == adapter->isDiscovering() );
            REQUIRE( BTRole::Slave == adapter->getRole() ); // kept

            // Stopping advertising and serving even if stopped must be OK!
            server->close(msg);
            REQUIRE( false == adapter->isAdvertising() );
            REQUIRE( false == adapter->isDiscovering() );
            REQUIRE( BTRole::Slave == adapter->getRole() ); // kept
        }
};

#endif // DBT_SERVER_TEST_HPP_
