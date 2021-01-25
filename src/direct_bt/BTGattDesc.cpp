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

#include <BTDevice.hpp>
#include <BTGattDesc.hpp>
#include <BTGattHandler.hpp>
#include <cstring>
#include <string>
#include <memory>
#include <cstdint>
#include <vector>
#include <cstdio>

#include  <algorithm>

#include <jau/debug.hpp>


using namespace direct_bt;
using namespace jau;

const uuid16_t BTGattDesc::TYPE_EXT_PROP(Type::CHARACTERISTIC_EXTENDED_PROPERTIES);
const uuid16_t BTGattDesc::TYPE_USER_DESC(Type::CHARACTERISTIC_USER_DESCRIPTION);
const uuid16_t BTGattDesc::TYPE_CCC_DESC(Type::CLIENT_CHARACTERISTIC_CONFIGURATION);

std::shared_ptr<BTGattChar> BTGattDesc::getGattCharChecked() const {
    std::shared_ptr<BTGattChar> ref = wbr_char.lock();
    if( nullptr == ref ) {
        throw IllegalStateException("GATTDescriptor's characteristic already destructed: "+toShortString(), E_FILE_LINE);
    }
    return ref;
}

std::shared_ptr<BTGattHandler> BTGattDesc::getGattHandlerChecked() const {
    return getGattCharChecked()->getGattHandlerChecked();
}

std::shared_ptr<BTDevice> BTGattDesc::getDeviceChecked() const {
    return getGattCharChecked()->getDeviceChecked();
}

bool BTGattDesc::readValue(int expectedLength) {
    std::shared_ptr<BTDevice> device = getDeviceChecked();
    std::shared_ptr<BTGattHandler> gatt = device->getGattHandler();
    if( nullptr == gatt ) {
        throw IllegalStateException("Descriptor's device GATTHandle not connected: "+toShortString(), E_FILE_LINE);
    }
    return gatt->readDescriptorValue(*this, expectedLength);
}

bool BTGattDesc::writeValue() {
    std::shared_ptr<BTDevice> device = getDeviceChecked();
    std::shared_ptr<BTGattHandler> gatt = device->getGattHandler();
    if( nullptr == gatt ) {
        throw IllegalStateException("Descriptor's device GATTHandle not connected: "+toShortString(), E_FILE_LINE);
    }
    return gatt->writeDescriptorValue(*this);
}

std::string BTGattDesc::toString() const noexcept {
    return "[type 0x"+type->toString()+", handle "+uint16HexString(handle)+", value["+value.toString()+"] ]";
}

std::string BTGattDesc::toShortString() const noexcept {
    return "[handle "+uint16HexString(handle)+", value["+value.toString()+"] ]";
}
