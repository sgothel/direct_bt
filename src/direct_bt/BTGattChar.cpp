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

#include  <algorithm>

#include <jau/debug.hpp>

#include "BTDevice.hpp"
#include "BTGattChar.hpp"
#include "BTGattHandler.hpp"

using namespace direct_bt;
using namespace jau;

#define CHAR_DECL_PROPS_ENUM(X) \
        X(Broadcast,broadcast) \
        X(Read,read) \
        X(WriteNoAck,write-without-response) \
        X(WriteWithAck,write) \
        X(Notify,notify) \
        X(Indicate,indicate) \
        X(AuthSignedWrite,authenticated-signed-writes) \
        X(ExtProps,extended-properties)

/**
        "reliable-write"
        "writable-auxiliaries"
        "encrypt-read"
        "encrypt-write"
        "encrypt-authenticated-read"
        "encrypt-authenticated-write"
        "secure-read" (Server only)
        "secure-write" (Server only)
        "authorize"
 */

#define CASE_TO_STRING2(V,S) case V: return #S;

static std::string _getPropertyBitValStr(const BTGattChar::PropertyBitVal prop) noexcept {
    switch(prop) {
        CHAR_DECL_PROPS_ENUM(CASE_TO_STRING2)
        default: ; // fall through intended
    }
    return "Unknown property";
}

std::string BTGattChar::getPropertiesString(const PropertyBitVal properties) noexcept {
    const PropertyBitVal none = static_cast<PropertyBitVal>(0);
    const uint8_t one = 1;
    bool has_pre = false;
    std::string out("[");
    for(int i=0; i<8; i++) {
        const PropertyBitVal propertyBit = static_cast<PropertyBitVal>( one << i );
        if( none != ( properties & propertyBit ) ) {
            if( has_pre ) { out.append(", "); }
            out.append(_getPropertyBitValStr(propertyBit));
            has_pre = true;
        }
    }
    out.append("]");
    return out;
}

jau::darray<std::unique_ptr<std::string>> BTGattChar::getPropertiesStringList(const PropertyBitVal properties) noexcept {
    jau::darray<std::unique_ptr<std::string>> out;
    const PropertyBitVal none = static_cast<PropertyBitVal>(0);
    const uint8_t one = 1;
    for(int i=0; i<8; i++) {
        const PropertyBitVal propertyBit = static_cast<PropertyBitVal>( one << i );
        if( none != ( properties & propertyBit ) ) {
            out.push_back( std::unique_ptr<std::string>( new std::string( _getPropertyBitValStr(propertyBit) ) ) );
        }
    }
    return out;
}

std::string BTGattChar::toString() const noexcept {
    uint16_t service_handle_end = 0xffff;
    BTGattServiceRef serviceRef = getServiceUnchecked();
    std::string service_uuid_str = "";
    std::string service_name = "";
    std::string char_name = "";
    std::string desc_str = ", descr[ ";

    if( nullptr != serviceRef ) {
        std::unique_ptr<const uuid_t> & service_uuid = serviceRef->type;
        service_uuid_str = service_uuid->toString();
        service_handle_end = serviceRef->endHandle;

        if( uuid_t::TypeSize::UUID16_SZ == service_uuid->getTypeSize() ) {
            const uint16_t uuid16 = (static_cast<const uuid16_t*>(service_uuid.get()))->value;
            service_name = ", "+GattServiceTypeToString(static_cast<GattServiceType>(uuid16));
        }
    }
    if( uuid_t::TypeSize::UUID16_SZ == value_type->getTypeSize() ) {
        const uint16_t uuid16 = (static_cast<const uuid16_t*>(value_type.get()))->value;
        char_name = ", "+GattCharacteristicTypeToString(static_cast<GattCharacteristicType>(uuid16));
    }
    for(size_t i=0; i<descriptorList.size(); i++) {
        const BTGattDescRef cd = descriptorList[i];
        desc_str += cd->toString() + ", ";
    }
    desc_str += " ]";
    return "[handle "+to_hexstring(handle)+", props "+to_hexstring(properties)+" "+getPropertiesString(properties)+
           ", value[type 0x"+value_type->toString()+", handle "+to_hexstring(value_handle)+char_name+desc_str+
           "], service[type 0x"+service_uuid_str+
           ", handle[ "+to_hexstring(service_handle)+".."+to_hexstring(service_handle_end)+" ]"+
           service_name+", enabled[notify "+std::to_string(enabledNotifyState)+", indicate "+std::to_string(enabledIndicateState)+"] ] ]";
}

std::string BTGattChar::toShortString() const noexcept {
    std::string char_name = "";

    if( uuid_t::TypeSize::UUID16_SZ == value_type->getTypeSize() ) {
        const uint16_t uuid16 = (static_cast<const uuid16_t*>(value_type.get()))->value;
        char_name = ", "+GattCharacteristicTypeToString(static_cast<GattCharacteristicType>(uuid16));
    }
    return "[handle "+to_hexstring(handle)+", props "+to_hexstring(properties)+" "+getPropertiesString(properties)+
           ", value[handle "+to_hexstring(value_handle)+char_name+
           "], service["+
           ", handle[ "+to_hexstring(service_handle)+".. ]"+
           ", enabled[notify "+std::to_string(enabledNotifyState)+", indicate "+std::to_string(enabledIndicateState)+"] ] ]";
}

std::shared_ptr<BTGattService> BTGattChar::getServiceChecked() const {
    std::shared_ptr<BTGattService> ref = wbr_service.lock();
    if( nullptr == ref ) {
        throw IllegalStateException("GATTCharacteristic's service already destructed: "+toShortString(), E_FILE_LINE);
    }
    return ref;
}

std::shared_ptr<BTGattHandler> BTGattChar::getGattHandlerUnchecked() const noexcept {
    std::shared_ptr<BTGattService> s = getServiceUnchecked();
    if( nullptr != s ) {
        return s->getGattHandlerUnchecked();
    }
    return nullptr;
}

std::shared_ptr<BTGattHandler> BTGattChar::getGattHandlerChecked() const {
    return getServiceChecked()->getGattHandlerChecked();
}

std::shared_ptr<BTDevice> BTGattChar::getDeviceUnchecked() const noexcept {
    std::shared_ptr<BTGattService> s = getServiceUnchecked();
    if( nullptr != s ) {
        return s->getDeviceUnchecked();
    }
    return nullptr;
}

std::shared_ptr<BTDevice> BTGattChar::getDeviceChecked() const {
    return getServiceChecked()->getDeviceChecked();
}

bool BTGattChar::configNotificationIndication(const bool enableNotification, const bool enableIndication, bool enabledState[2]) {
    enabledState[0] = false;
    enabledState[1] = false;

    const bool hasEnableNotification = hasProperties(BTGattChar::PropertyBitVal::Notify);
    const bool hasEnableIndication = hasProperties(BTGattChar::PropertyBitVal::Indicate);
    if( !hasEnableNotification && !hasEnableIndication ) {
        DBG_PRINT("Characteristic has neither Notify nor Indicate property present: %s", toString().c_str());
        return false;
    }

    std::shared_ptr<BTDevice> device = getDeviceUnchecked();
    std::shared_ptr<BTGattHandler> gatt = nullptr != device ? device->getGattHandler() : nullptr;
    if( nullptr == gatt ) {
        if( !enableNotification && !enableIndication ) {
            // OK to have GATTHandler being shutdown @ disable
            DBG_PRINT("Characteristic's device GATTHandle not connected: %s", toShortString().c_str());
            return false;
        }
        throw IllegalStateException("Characteristic's device GATTHandle not connected: "+
                toString(), E_FILE_LINE);
    }
    const bool resEnableNotification = hasEnableNotification && enableNotification;
    const bool resEnableIndication = hasEnableIndication && enableIndication;

    if( resEnableNotification == enabledNotifyState &&
        resEnableIndication == enabledIndicateState )
    {
        enabledState[0] = resEnableNotification;
        enabledState[1] = resEnableIndication;
        DBG_PRINT("GATTCharacteristic::configNotificationIndication: Unchanged: notification[shall %d, has %d: %d == %d], indication[shall %d, has %d: %d == %d]",
                enableNotification, hasEnableNotification, enabledNotifyState, resEnableNotification,
                enableIndication, hasEnableIndication, enabledIndicateState, resEnableIndication);
        return true;
    }

    BTGattDescRef cccd = this->getClientCharConfig();
    if( nullptr == cccd ) {
        DBG_PRINT("Characteristic has no ClientCharacteristicConfig descriptor: %s", toString().c_str());
        return false;
    }
    bool res = gatt->configNotificationIndication(*cccd, resEnableNotification, resEnableIndication);
    DBG_PRINT("GATTCharacteristic::configNotificationIndication: res %d, notification[shall %d, has %d: %d -> %d], indication[shall %d, has %d: %d -> %d]",
            res,
            enableNotification, hasEnableNotification, enabledNotifyState, resEnableNotification,
            enableIndication, hasEnableIndication, enabledIndicateState, resEnableIndication);
    if( res ) {
        enabledNotifyState = resEnableNotification;
        enabledIndicateState = resEnableIndication;
        enabledState[0] = resEnableNotification;
        enabledState[1] = resEnableIndication;
    }
    return res;
}

bool BTGattChar::enableNotificationOrIndication(bool enabledState[2]) {
    const bool hasEnableNotification = hasProperties(BTGattChar::PropertyBitVal::Notify);
    const bool hasEnableIndication = hasProperties(BTGattChar::PropertyBitVal::Indicate);

    const bool enableNotification = hasEnableNotification;
    const bool enableIndication = !enableNotification && hasEnableIndication;

    return configNotificationIndication(enableNotification, enableIndication, enabledState);
}

bool BTGattChar::addCharListener(std::shared_ptr<BTGattCharListener> l) {
    return getDeviceChecked()->addCharListener(l);
}

bool BTGattChar::addCharListener(std::shared_ptr<BTGattCharListener> l, bool enabledState[2]) {
    if( !enableNotificationOrIndication(enabledState) ) {
        return false;
    }
    return addCharListener(l);
}

bool BTGattChar::removeCharListener(std::shared_ptr<BTGattCharListener> l, bool disableIndicationNotification) {
    if( disableIndicationNotification ) {
        bool enabledState[2];
        configNotificationIndication(false, false, enabledState);
    }
    return getDeviceChecked()->removeCharListener(l);
}

int BTGattChar::removeAllCharListener(bool disableIndicationNotification) {
    if( disableIndicationNotification ) {
        bool enabledState[2];
        configNotificationIndication(false, false, enabledState);
    }
    return getDeviceChecked()->removeAllCharListener();
}

bool BTGattChar::readValue(POctets & res, int expectedLength) {
    std::shared_ptr<BTDevice> device = getDeviceChecked();
    std::shared_ptr<BTGattHandler> gatt = device->getGattHandler();
    if( nullptr == gatt ) {
        throw IllegalStateException("Characteristic's device GATTHandle not connected: "+toShortString(), E_FILE_LINE);
    }
    return gatt->readCharacteristicValue(*this, res, expectedLength);
}
/**
 * BT Core Spec v5.2: Vol 3, Part G GATT: 4.9.3 Write Characteristic Value
 */
bool BTGattChar::writeValue(const TROOctets & value) {
    std::shared_ptr<BTDevice> device = getDeviceChecked();
    std::shared_ptr<BTGattHandler> gatt = device->getGattHandler();
    if( nullptr == gatt ) {
        throw IllegalStateException("Characteristic's device GATTHandle not connected: "+toShortString(), E_FILE_LINE);
    }
    return gatt->writeCharacteristicValue(*this, value);
}

/**
 * BT Core Spec v5.2: Vol 3, Part G GATT: 4.9.1 Write Characteristic Value Without Response
 */
bool BTGattChar::writeValueNoResp(const TROOctets & value) {
    std::shared_ptr<BTDevice> device = getDeviceChecked();
    std::shared_ptr<BTGattHandler> gatt = device->getGattHandler();
    if( nullptr == gatt ) {
        throw IllegalStateException("Characteristic's device GATTHandle not connected: "+toShortString(), E_FILE_LINE);
    }
    return gatt->writeCharacteristicValueNoResp(*this, value);
}
