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
#include <cstdio>

#include <jau/debug.hpp>

#include "BTDevice.hpp"
#include "BTGattDesc.hpp"
#include "BTGattHandler.hpp"

using namespace direct_bt;

const std::shared_ptr<jau::uuid_t> BTGattDesc::TYPE_EXT_PROP( std::make_shared<jau::uuid16_t>(Type::CHARACTERISTIC_EXTENDED_PROPERTIES) );
const std::shared_ptr<jau::uuid_t> BTGattDesc::TYPE_USER_DESC( std::make_shared<jau::uuid16_t>(Type::CHARACTERISTIC_USER_DESCRIPTION) );
const std::shared_ptr<jau::uuid_t> BTGattDesc::TYPE_CCC_DESC( std::make_shared<jau::uuid16_t>(Type::CLIENT_CHARACTERISTIC_CONFIGURATION) );

std::shared_ptr<BTGattChar> BTGattDesc::getGattCharChecked() const {
    std::shared_ptr<BTGattChar> ref = wbr_char.lock();
    if( nullptr == ref ) {
        throw jau::IllegalStateError("GATTDescriptor's characteristic already destructed: "+toShortString(), E_FILE_LINE);
    }
    return ref;
}

std::shared_ptr<BTGattHandler> BTGattDesc::getGattHandlerUnchecked() const noexcept {
    std::shared_ptr<BTGattChar> c = getGattCharUnchecked();
    if( nullptr != c ) {
        return c->getGattHandlerUnchecked();
    }
    return nullptr;
}

std::shared_ptr<BTDevice> BTGattDesc::getDeviceUnchecked() const noexcept {
    std::shared_ptr<BTGattChar> c = getGattCharUnchecked();
    if( nullptr != c ) {
        return c->getDeviceUnchecked();
    }
    return nullptr;
}

bool BTGattDesc::readValue(int expectedLength) noexcept {
    std::shared_ptr<BTDevice> device = getDeviceUnchecked();
    if( nullptr == device ) {
        ERR_PRINT("Descriptor's device null: %s", toShortString().c_str());
        return false;
    }
    std::shared_ptr<BTGattHandler> gatt = device->getGattHandler();
    if( nullptr == gatt ) {
        ERR_PRINT("Descriptor's device GATTHandle not connected: %s", toShortString().c_str());
        return false;
    }
    return gatt->readDescriptorValue(*this, expectedLength);
}

bool BTGattDesc::writeValue() noexcept {
    std::shared_ptr<BTDevice> device = getDeviceUnchecked();
    if( nullptr == device ) {
        ERR_PRINT("Descriptor's device null: %s", toShortString().c_str());
        return false;
    }
    std::shared_ptr<BTGattHandler> gatt = device->getGattHandler();
    if( nullptr == gatt ) {
        ERR_PRINT("Descriptor's device GATTHandle not connected: %s", toShortString().c_str());
        return false;
    }
    return gatt->writeDescriptorValue(*this);
}

std::string BTGattDesc::toString() const noexcept {
    return "Desc[type 0x"+type->toString()+", handle "+jau::to_hexstring(handle)+
           ", value["+value.toString()+
           " '" + jau::dfa_utf8_decode( value.get_ptr(), value.size() ) + "'"+
           "]]";
}

std::string BTGattDesc::toShortString() const noexcept {
    return "Desc[handle "+jau::to_hexstring(handle)+", value["+value.toString()+"]]";
}
