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
#include <cstdio>

#include <jau/debug.hpp>

#include "GattNumbers.hpp"

#include "BTDevice.hpp"
#include "BTGattHandler.hpp"
#include "BTGattService.hpp"

using namespace direct_bt;
using namespace jau;

BTGattHandlerRef BTGattService::getGattHandlerChecked() const {
    std::shared_ptr<BTGattHandler> ref = wbr_handler.lock();
    if( nullptr == ref ) {
        throw IllegalStateError("GATTService's GATTHandler already destructed: "+toShortString(), E_FILE_LINE);
    }
    return ref;
}

BTDeviceRef BTGattService::getDeviceUnchecked() const noexcept {
    std::shared_ptr<BTGattHandler> h = getGattHandlerUnchecked();
    if( nullptr != h ) {
        return h->getDeviceUnchecked();
    }
    return nullptr;
}

BTDeviceRef BTGattService::getDeviceChecked() const {
    return getGattHandlerChecked()->getDeviceChecked();
}

BTGattCharRef BTGattService::findGattChar(const jau::uuid_t& char_uuid) noexcept {
    for(BTGattCharRef& c : characteristicList) {
        if( nullptr != c && char_uuid.equivalent( *(c->value_type) ) ) {
            return c;
        }
    }
    return nullptr;
}

BTGattCharRef BTGattService::findGattChar(const BTGattChar& characteristic) noexcept {
    for(BTGattCharRef& c : characteristicList) {
        if( nullptr != c && characteristic == *c ) {
            return c;
        }
    }
    return nullptr;
}

std::string BTGattService::toString() const noexcept {
    std::string name = "";
    if( uuid_t::TypeSize::UUID16_SZ == type->getTypeSize() ) {
        const uint16_t uuid16 = (static_cast<const uuid16_t*>(type.get()))->value;
        name = " - "+GattServiceTypeToString(static_cast<GattServiceType>(uuid16));
    }
    return "Srvc[type 0x"+type->toString()+", handle ["+to_hexstring(handle)+".."+to_hexstring(end_handle)+"]"+
                name+", "+std::to_string(characteristicList.size())+" chars]";
}

std::string BTGattService::toShortString() const noexcept {
    std::string name = "";
    if( uuid_t::TypeSize::UUID16_SZ == type->getTypeSize() ) {
        const uint16_t uuid16 = (static_cast<const uuid16_t*>(type.get()))->value;
        name = " - "+GattServiceTypeToString(static_cast<GattServiceType>(uuid16));
    }
    return "Srvc[handle ["+to_hexstring(handle)+".."+to_hexstring(end_handle)+"]"+
                name+", "+std::to_string(characteristicList.size())+" characteristics]";
}
