/*
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2020 Gothel Software e.K.
 * Copyright (c) 2020 ZAFENA AB
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

#include <BTSecurityRegistry.hpp>

#include <jau/darray.hpp>

using namespace direct_bt;

namespace direct_bt::BTSecurityRegistry {

    static jau::darray<Entry> devicesSecDetails;

    Entry* get(const EUI48& addr, const std::string& name, AddressNameEntryMatchFunc m) {
        auto first = devicesSecDetails.begin();
        auto last = devicesSecDetails.end();
        for (; first != last; ++first) {
            if( m(addr, name, *first) ) {
                return &(*first);
            }
        }
        return nullptr;
    }
    Entry* get(const EUI48Sub& addrSub, const std::string& name, AddressSubNameEntryMatchFunc m) {
        auto first = devicesSecDetails.begin();
        auto last = devicesSecDetails.end();
        for (; first != last; ++first) {
            if( m(addrSub, name, *first) ) {
                return &(*first);
            }
        }
        return nullptr;
    }
    Entry* get(const std::string& name, NameEntryMatchFunc m) {
        auto first = devicesSecDetails.begin();
        auto last = devicesSecDetails.end();
        for (; first != last; ++first) {
            if( m(name, *first) ) {
                return &(*first);
            }
        }
        return nullptr;
    }

    jau::darray<Entry>& getEntries() {
        return devicesSecDetails;
    }

    Entry* getOrCreate(const std::string& addrOrNameSub) {
        EUI48Sub addr1;
        std::string errmsg;
        Entry* sec = nullptr;
        if( EUI48Sub::scanEUI48Sub(addrOrNameSub, addr1, errmsg) ) {
            sec = getEqual(addr1, "");
            if( nullptr == sec ) {
                Entry& r = devicesSecDetails.emplace_back( addr1 );
                sec = &r;
            }
        } else {
            sec = getEqual(addrOrNameSub);
            if( nullptr == sec ) {
                Entry& r = devicesSecDetails.emplace_back( addrOrNameSub );
                sec = &r;
            }
        }
        return sec;
    }
    void clear() {
        devicesSecDetails.clear();
    }
    std::string allToString() {
        std::string res;
        int i=0;
        for(auto iter = devicesSecDetails.cbegin(); iter != devicesSecDetails.cend(); ++iter, ++i) {
            const Entry& sec = *iter;
            if( 0 < i ) {
                res += ", ";
            }
            res += sec.toString();
        }
        return res;
    }

} // namespace direct_bt::BTSecurityRegistry
