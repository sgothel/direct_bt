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

static jau::cow_darray<DBGattServer::ListenerRef>::equal_comparator _listenerRefEqComparator =
        [](const DBGattServer::ListenerRef &a, const DBGattServer::ListenerRef &b) -> bool { return *a == *b; };

bool DBGattServer::addListener(ListenerRef l) {
    if( nullptr == l ) {
        throw jau::IllegalArgumentException("Listener ref is null", E_FILE_LINE);
    }
    return listenerList.push_back_unique(l, _listenerRefEqComparator);
}

bool DBGattServer::removeListener(ListenerRef l) {
    if( nullptr == l ) {
        ERR_PRINT("Listener ref is null");
        return false;
    }
    const int count = listenerList.erase_matching(l, false /* all_matching */, _listenerRefEqComparator);
    return count > 0;
}

