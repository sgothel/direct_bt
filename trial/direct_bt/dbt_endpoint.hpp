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

#ifndef DBT_ENDPOINT_HPP_
#define DBT_ENDPOINT_HPP_

#include <iostream>
#include <cassert>
#include <cinttypes>
#include <cstring>

#include <catch2/catch_amalgamated.hpp>
#include <jau/test/catch2_ext.hpp>

#include <direct_bt/DirectBT.hpp>

using namespace direct_bt;

class DBTEndpoint;
typedef std::shared_ptr<DBTEndpoint> DBTEndpointRef;

class DBTEndpoint {

    public:
        virtual ~DBTEndpoint() {}

        /**
         * Return name of this endpoint,
         * which becomes the adapter's name.
         */
        virtual std::string getName() = 0;

        /**
         * Set the server adapter for this endpoint.
         *
         * This is done in {@link ChangedAdapterSetListener#adapterAdded(BTAdapter)}
         * if {@link #initAdapter(BTAdapter)} returned true.
         *
         * @param a the associate adapter for this endpoint.
         */
        virtual void setAdapter(BTAdapterRef a) = 0;

        /**
         * Return the adapter for this endpoint.
         */
        virtual BTAdapterRef getAdapter() = 0;

        virtual void close(const std::string& msg) = 0;

        virtual void setProtocolSessionsLeft(const int v) = 0;
        virtual int getProtocolSessionsLeft() = 0;
        virtual int getProtocolSessionsDoneTotal() = 0;
        virtual int getProtocolSessionsDoneSuccess() = 0;
        virtual int getDisconnectCount() = 0;

        /**
         * Initialize the given adapter for this endpoint.
         *
         * The matching and successfully initialized adapter
         * will become this endpoint's associated adapter via {@link #setAdapter(BTAdapter)},
         * as performed in in {@link ChangedAdapterSetListener#adapterAdded(BTAdapter)}.
         *
         * @param adapter the potential associated adapter for this endpoint.
         * @return true if successful and associated
         */
        virtual bool initAdapter(BTAdapterRef adapter) = 0;

        static void checkInitializedState(const DBTEndpointRef endp) {
            BTAdapterRef adapter = endp->getAdapter();
            REQUIRE( true == adapter->isInitialized() );
            REQUIRE( true == adapter->isPowered() );
            REQUIRE( BTRole::Master == adapter->getRole() );
            REQUIRE( 4 <= adapter->getBTMajorVersion() );
        }

        static std::mutex mtx_cas_endpts;
        static std::vector<DBTEndpointRef> cas_endpts;

        static bool myChangedAdapterSetFunc(const bool added, BTAdapterRef& adapter) {
            if( added ) {
                for(DBTEndpointRef endpt : cas_endpts ) {
                    if( nullptr == endpt->getAdapter() ) {
                        if( endpt->initAdapter( adapter ) ) {
                            endpt->setAdapter(adapter);
                            jau::fprintf_td(stderr, "****** Adapter ADDED__: InitOK: %s\n", adapter->toString().c_str());
                            return true;
                        }
                    }
                }
                jau::fprintf_td(stderr, "****** Adapter ADDED__: Ignored: %s\n", adapter->toString().c_str());
            } else {
                for(DBTEndpointRef endpt : cas_endpts ) {
                    if( nullptr != endpt->getAdapter() && *adapter == *endpt->getAdapter() ) {
                        endpt->setAdapter(nullptr);
                        jau::fprintf_td(stderr, "****** Adapter REMOVED: %s\n", adapter->toString().c_str());
                        return true;
                    }
                }
                jau::fprintf_td(stderr, "****** Adapter REMOVED: Ignored: %s\n", adapter->toString().c_str());
            }
            return true;
        }

        static ChangedAdapterSetCallback initChangedAdapterSetListener(BTManager& manager, std::vector<DBTEndpointRef> endpts) {
            const std::lock_guard<std::mutex> lock(mtx_cas_endpts); // RAII-style acquire and relinquish via destructor
            cas_endpts = std::move( endpts );
            ChangedAdapterSetCallback casc = jau::bindPlainFunc(&DBTEndpoint::myChangedAdapterSetFunc);
            manager.addChangedAdapterSetCallback(casc);
            for(DBTEndpointRef endpt : cas_endpts ) {
                REQUIRE( nullptr != endpt->getAdapter() );
            }
            return casc;
        }

        static void startDiscovery(BTAdapterRef adapter, const bool current_exp_discovering_state) {
            REQUIRE( false == adapter->isAdvertising() );
            REQUIRE( current_exp_discovering_state == adapter->isDiscovering());

            REQUIRE( HCIStatusCode::SUCCESS == adapter->startDiscovery() );
            while( !adapter->isDiscovering() ) { // pending action
                jau::sleep_for( 100_ms );
            }
            REQUIRE( false == adapter->isAdvertising() );
            REQUIRE( true == adapter->isDiscovering() );
            REQUIRE( BTRole::Master == adapter->getRole() );
        }

        static void stopDiscovery(BTAdapterRef adapter, const bool current_exp_discovering_state) {
            REQUIRE( false == adapter->isAdvertising() );
            REQUIRE( current_exp_discovering_state == adapter->isDiscovering() );
            REQUIRE( BTRole::Master == adapter->getRole() );

            REQUIRE( HCIStatusCode::SUCCESS == adapter->stopDiscovery() ); // pending action
            while( adapter->isDiscovering() ) { // pending action
                jau::sleep_for( 100_ms );
            }
            REQUIRE( false == adapter->isAdvertising() );
            REQUIRE( false == adapter->isDiscovering() );
            REQUIRE( BTRole::Master == adapter->getRole() );
        }
};

std::mutex DBTEndpoint::mtx_cas_endpts;
std::vector<DBTEndpointRef> DBTEndpoint::cas_endpts;

#endif /* DBT_ENDPOINT_HPP_ */
