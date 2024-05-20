/*
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2021 Gothel Software e.K.
 * Copyright (c) 2021 ZAFENA AB
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

#include <cstring>
#include <string>
#include <memory>
#include <cstdint>
#include <vector>
#include <cstdio>

#include  <algorithm>

#include <jau/debug.hpp>

#include "DBGattServer.hpp"

using namespace direct_bt;

bool DBGattDesc::setValue(const uint8_t* source, const jau::nsize_t source_len, const jau::nsize_t dest_pos) noexcept {
    if( hasVariableLength() ) {
        if( value.capacity() < dest_pos + source_len ) {
            return false;
        }
        if( value.size() != dest_pos + source_len ) {
            value.resize( dest_pos + source_len );
        }
    } else {
        if( value.size() < dest_pos + source_len ) {
            return false;
        }
    }
    value.put_bytes_nc(dest_pos, source, source_len);
    return true;
}

bool DBGattChar::setValue(const uint8_t* source, const jau::nsize_t source_len, const jau::nsize_t dest_pos) noexcept {
    if( hasVariableLength() ) {
        if( value.capacity() < dest_pos + source_len ) {
            return false;
        }
        if( value.size() != dest_pos + source_len ) {
            value.resize( dest_pos + source_len );
        }
    } else {
        if( value.size() < dest_pos + source_len ) {
            return false;
        }
    }
    value.put_bytes_nc(dest_pos, source, source_len);
    return true;
}

std::string direct_bt::to_string(const DBGattServer::Mode m) noexcept {
    switch(m) {
        case DBGattServer::Mode::NOP: return "nop";
        case DBGattServer::Mode::DB:  return "db";
        case DBGattServer::Mode::FWD: return "fwd";
    }
    return "Unknown mode";
}

static jau::cow_darray<DBGattServer::ListenerRef>::equal_comparator _listenerRefEqComparator =
        [](const DBGattServer::ListenerRef &a, const DBGattServer::ListenerRef &b) -> bool { return *a == *b; };

bool DBGattServer::addListener(const ListenerRef& l) {
    if( nullptr == l ) {
        throw jau::IllegalArgumentError("Listener ref is null", E_FILE_LINE);
    }
    return listenerList.push_back_unique(l, _listenerRefEqComparator);
}

bool DBGattServer::removeListener(const ListenerRef& l) {
    if( nullptr == l ) {
        ERR_PRINT("Listener ref is null");
        return false;
    }
    const auto count = listenerList.erase_matching(l, false /* all_matching */, _listenerRefEqComparator);
    return count > 0;
}

std::string DBGattServer::toString() const noexcept {
    return "DBSrv[mode "+to_string(mode)+", max mtu "+std::to_string(max_att_mtu)+", "+std::to_string(services.size())+" services, "+javaObjectToString()+"]";
}

