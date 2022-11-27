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

#include <cstdint>
#include <cinttypes>
#include <cmath>

#include <algorithm>

#include <jau/basic_types.hpp>

#include "ieee11073/DataTypes.hpp"

using namespace ieee11073;

AbsoluteTime::AbsoluteTime(const uint8_t * data_le, const int size) {
    if( nullptr == data_le || 0 == size ) {
        return;
    }
    int i=0;
    if( 2 > size ) return;
    year = (int16_t) ( data_le[i+0] | data_le[i+1] << 8 );
    i+=2;
    if( 3 > size ) return;
    month = static_cast<int8_t>(data_le[i++]);
    if( 4 > size ) return;
    day = static_cast<int8_t>(data_le[i++]);
    if( 5 > size ) return;
    hour = static_cast<int8_t>(data_le[i++]);
    if( 6 > size ) return;
    minute = static_cast<int8_t>(data_le[i++]);
    if( 7 > size ) return;
    second = static_cast<int8_t>(data_le[i++]);
    if( 8 > size ) return;
    second_fractions = static_cast<int8_t>(data_le[i++]);
}

std::string AbsoluteTime::toString() const {
    char cbuf[24]; // '2020-04-04 10:58:59' 19 + second_fractions
    const size_t count = snprintf(cbuf, sizeof(cbuf), "%4.4d-%2.2d-%2.2d %2.2d:%2.2d:%2.2d",
                               (int)year, (int)month, (int)day, (int)hour, (int)minute, (int)second);
    if( count >= sizeof(cbuf) ) { // truncated?
        throw jau::InternalError("snprintf date-format truncated "+std::to_string(count)+" >= "+std::to_string(sizeof(cbuf)), E_FILE_LINE);
    }
    std::string res(cbuf);
    if( 0 != second_fractions ) {
        res += "."+std::to_string(second_fractions);
    }
    return res;
}

static const int32_t FIRST_RESERVED_VALUE = FloatTypes::ReservedFloatValues::MDER_POSITIVE_INFINITY;
static const uint32_t FIRST_S_RESERVED_VALUE = FloatTypes::ReservedSFloatValues::MDER_S_POSITIVE_INFINITY;

static const float reserved_float_values[5] = {INFINITY, NAN, NAN, NAN, -INFINITY};

using namespace jau::int_literals;

float FloatTypes::float16_IEEE11073_to_IEEE754(const uint16_t raw_bt_float16_le) {
    uint16_t mantissa = raw_bt_float16_le & 0x0FFF;
    int8_t exponent = (int8_t) ( ( raw_bt_float16_le >> 12 ) & 0xFF );

    if( exponent >= 0x0008) {
        exponent = (int8_t) ( - ( (0x000F + 1) - (int)exponent ) );
    }

    if( mantissa >= FIRST_S_RESERVED_VALUE &&
        mantissa <= ReservedSFloatValues::MDER_S_NEGATIVE_INFINITY ) {
        return reserved_float_values[mantissa - FIRST_S_RESERVED_VALUE];
    }
    if ( mantissa >= 0x0800 ) {
        mantissa = - ( ( 0x0FFF + 1 ) - mantissa );
    }
    return (float)mantissa * ::powf(10.0f, (float)exponent);
}

float FloatTypes::float32_IEEE11073_to_IEEE754(const uint32_t raw_bt_float32_le) {
    int32_t mantissa = (int32_t) ( raw_bt_float32_le & 0xFFFFFF );
    int8_t exponent = (int8_t) ( ( raw_bt_float32_le >> 24 ) & 0xFF );

    if( mantissa >= FIRST_RESERVED_VALUE &&
        mantissa <= ReservedFloatValues::MDER_NEGATIVE_INFINITY ) {
        return reserved_float_values[mantissa - FIRST_RESERVED_VALUE];
    }
    if( mantissa >= 0x800000 ) {
        mantissa = - ( ( 0xFFFFFF + 1 ) - mantissa );
    }
    return (float)mantissa * ::powf(10.0f, (float)exponent);
}
