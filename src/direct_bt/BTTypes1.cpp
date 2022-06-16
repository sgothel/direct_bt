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

#include <cstring>
#include <string>
#include <memory>
#include <cstdint>
#include <vector>
#include <cstdio>

#include <algorithm>

// #define PERF_PRINT_ON 1
// #define VERBOSE_ON 1
#include <jau/debug.hpp>

#include "BTTypes1.hpp"

extern "C" {
    #include <inttypes.h>
    #include <unistd.h>
}

using namespace direct_bt;

// *************************************************
// *************************************************
// *************************************************

template<typename T>
static void append_bitstr(std::string& out, T mask, T bit, const std::string& bitstr, bool& comma) {
    if( bit == ( mask & bit ) ) {
        if( comma ) { out.append(", "); }
        out.append(bitstr); comma = true;
    }
}
#define APPEND_BITSTR(U,V,M) append_bitstr(out, M, U::V, #V, comma);

#define SETTING_ENUM(X,M) \
    X(AdapterSetting,NONE,M) \
    X(AdapterSetting,POWERED,M) \
    X(AdapterSetting,CONNECTABLE,M) \
    X(AdapterSetting,FAST_CONNECTABLE,M) \
    X(AdapterSetting,DISCOVERABLE,M) \
    X(AdapterSetting,BONDABLE,M) \
    X(AdapterSetting,LINK_SECURITY,M) \
    X(AdapterSetting,SSP,M) \
    X(AdapterSetting,BREDR,M) \
    X(AdapterSetting,HS,M) \
    X(AdapterSetting,LE,M) \
    X(AdapterSetting,ADVERTISING,M) \
    X(AdapterSetting,SECURE_CONN,M) \
    X(AdapterSetting,DEBUG_KEYS,M) \
    X(AdapterSetting,PRIVACY,M) \
    X(AdapterSetting,CONFIGURATION,M) \
    X(AdapterSetting,STATIC_ADDRESS,M) \
    X(AdapterSetting,PHY_CONFIGURATION,M)

std::string direct_bt::to_string(const AdapterSetting mask) noexcept {
    std::string out("[");
    bool comma = false;
    SETTING_ENUM(APPEND_BITSTR,mask)
    out.append("]");
    return out;
}

BTMode direct_bt::getAdapterSettingsBTMode(const AdapterSetting settingMask) noexcept {
    const bool isBREDR = isAdapterSettingBitSet(settingMask, AdapterSetting::BREDR);
    const bool isLE = isAdapterSettingBitSet(settingMask, AdapterSetting::LE);
    if( isBREDR && isLE ) {
        return BTMode::DUAL;
    } else if( isBREDR ) {
        return BTMode::BREDR;
    } else if( isLE ) {
        return BTMode::LE;
    } else {
        return BTMode::NONE;
    }
}
