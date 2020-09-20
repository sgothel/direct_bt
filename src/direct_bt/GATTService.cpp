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

#include <dbt_debug.hpp>
#include <cstring>
#include <string>
#include <memory>
#include <cstdint>
#include <vector>
#include <cstdio>

#include  <algorithm>

#include "DBTDevice.hpp"
#include "GATTHandler.hpp"
#include "GATTService.hpp"
#include "GATTNumbers.hpp"

using namespace direct_bt;

std::shared_ptr<GATTHandler> GATTService::getGATTHandlerChecked() const {
    std::shared_ptr<GATTHandler> ref = wbr_handler.lock();
    if( nullptr == ref ) {
        throw IllegalStateException("GATTService's GATTHandler already destructed: "+toShortString(), E_FILE_LINE);
    }
    return ref;
}

std::shared_ptr<DBTDevice> GATTService::getDeviceUnchecked() const noexcept {
    std::shared_ptr<GATTHandler> h = getGATTHandlerUnchecked();
    if( nullptr != h ) {
        return h->getDeviceUnchecked();
    }
    return nullptr;
}

std::shared_ptr<DBTDevice> GATTService::getDeviceChecked() const {
    return getGATTHandlerChecked()->getDeviceChecked();
}

std::string GATTService::toString() const noexcept {
    std::string name = "";
    if( uuid_t::TypeSize::UUID16_SZ == type->getTypeSize() ) {
        const uint16_t uuid16 = (static_cast<const uuid16_t*>(type.get()))->value;
        name = " - "+GattServiceTypeToString(static_cast<GattServiceType>(uuid16));
    }
    return "type 0x"+type->toString()+", handle ["+uint16HexString(startHandle, true)+".."+uint16HexString(endHandle, true)+"]"+
                name+", "+std::to_string(characteristicList.size())+" characteristics";
}

std::string GATTService::toShortString() const noexcept {
    std::string name = "";
    if( uuid_t::TypeSize::UUID16_SZ == type->getTypeSize() ) {
        const uint16_t uuid16 = (static_cast<const uuid16_t*>(type.get()))->value;
        name = " - "+GattServiceTypeToString(static_cast<GattServiceType>(uuid16));
    }
    return "handle ["+uint16HexString(startHandle, true)+".."+uint16HexString(endHandle, true)+"]"+
                name+", "+std::to_string(characteristicList.size())+" characteristics";
}
