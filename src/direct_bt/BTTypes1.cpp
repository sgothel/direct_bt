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
#include <cstdio>

// #define PERF_PRINT_ON 1
// #define VERBOSE_ON 1
#include <jau/debug.hpp>

#include "BTTypes1.hpp"

extern "C" {
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

namespace direct_bt {
    JAU_MAKE_BITFIELD_ENUM_STRING_CODE(AdapterSetting, POWERED, CONNECTABLE, FAST_CONNECTABLE, DISCOVERABLE, BONDABLE, LINK_SECURITY,
                                                       SSP, BREDR, HS, LE, ADVERTISING, SECURE_CONN,
                                                       DEBUG_KEYS, PRIVACY, CONFIGURATION, STATIC_ADDRESS, PHY_CONFIGURATION);
}

std::string direct_bt::ConnectionInfo::toString() const noexcept {
    return jau_format_string("address %s, addressType %s, rssi %d, tx_power[set %d, max %d]",
        getAddress(), getAddressType(), rssi, tx_power, max_tx_power);
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

std::string direct_bt::AdapterInfo::toString() const noexcept {
    return jau_format_string("AdapterInfo[id %u, address %s, version %u, manuf %u, settings[sup %s, cur %s], name '%s', shortName '%s']",
        dev_id, addressAndType, version, manufacturer, supported_setting, current_setting, name, short_name);
}
