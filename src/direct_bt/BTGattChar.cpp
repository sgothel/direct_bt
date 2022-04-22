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

#include <jau/dfa_utf8_decode.hpp>
#include <jau/debug.hpp>

#include "BTDevice.hpp"
#include "BTGattService.hpp"
#include "BTGattChar.hpp"
#include "BTGattHandler.hpp"

using namespace direct_bt;
using namespace jau;

/**
 * Simple access and provision of a typename string representation
 * at compile time like RTTI via jau::type_name_cue.
 */
JAU_TYPENAME_CUE(direct_bt::BTGattCharListener)

/**
 * Simple access and provision of a typename string representation
 * at compile time like RTTI via jau::type_name_cue.
 */
JAU_TYPENAME_CUE(direct_bt::AssociatedBTGattCharListener)

const char * BTGattCharListener::type_name() const noexcept { return jau::type_name_cue<BTGattCharListener>::name(); }

const char * AssociatedBTGattCharListener::type_name() const noexcept { return jau::type_name_cue<AssociatedBTGattCharListener>::name(); }


#define CHAR_DECL_PROPS_ENUM(X) \
        X(BTGattChar,Broadcast,broadcast) \
        X(BTGattChar,Read,read) \
        X(BTGattChar,WriteNoAck,write-noack) \
        X(BTGattChar,WriteWithAck,write-ack) \
        X(BTGattChar,Notify,notify) \
        X(BTGattChar,Indicate,indicate) \
        X(BTGattChar,AuthSignedWrite,authenticated-signed-writes) \
        X(BTGattChar,ExtProps,extended-properties)

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

#define CASE2_TO_STRING2(U,V,W) case U::V: return #W;

static std::string _getPropertyBitValStr(const BTGattChar::PropertyBitVal prop) noexcept {
    switch(prop) {
        CHAR_DECL_PROPS_ENUM(CASE2_TO_STRING2)
        default: ; // fall through intended
    }
    return "Unknown property";
}

std::string direct_bt::to_string(const BTGattChar::PropertyBitVal mask) noexcept {
    const BTGattChar::PropertyBitVal none = static_cast<BTGattChar::PropertyBitVal>(0);
    const uint8_t one = 1;
    bool has_pre = false;
    std::string out("[");
    for(int i=0; i<8; i++) {
        const BTGattChar::PropertyBitVal propertyBit = static_cast<BTGattChar::PropertyBitVal>( one << i );
        if( none != ( mask & propertyBit ) ) {
            if( has_pre ) { out.append(", "); }
            out.append(_getPropertyBitValStr(propertyBit));
            has_pre = true;
        }
    }
    out.append("]");
    return out;
}

std::shared_ptr<BTGattDesc> BTGattChar::findGattDesc(const jau::uuid_t& desc_uuid) noexcept {
    const size_t descriptors_size = descriptorList.size();
    for(size_t j = 0; j < descriptors_size; j++) {
        direct_bt::BTGattDescRef descriptor = descriptorList[j];
        if( nullptr != descriptor && desc_uuid == *(descriptor->type) ) {
            return descriptor;
        }
    }
    return nullptr;
}

std::string BTGattChar::toString() const noexcept {
    std::string char_name = "";
    std::string desc_str;

    if( uuid_t::TypeSize::UUID16_SZ == value_type->getTypeSize() ) {
        const uint16_t uuid16 = (static_cast<const uuid16_t*>(value_type.get()))->value;
        char_name = ", "+GattCharacteristicTypeToString(static_cast<GattCharacteristicType>(uuid16));
    }
    {
        BTGattDescRef ud = getUserDescription();
        if( nullptr != ud ) {
            char_name.append( ", '" + dfa_utf8_decode( ud->value.get_ptr(), ud->value.size() ) + "'");
        }
    }
    if( 0 < descriptorList.size() ) {
        bool comma = false;
        desc_str = ", descr[";
        for(size_t i=0; i<descriptorList.size(); i++) {
            const BTGattDescRef cd = descriptorList[i];
            if( comma ) {
                desc_str += ", ";
            }
            desc_str += "handle "+to_hexstring(cd->handle);
            comma = true;
        }
        desc_str += "]";
    }

    std::string notify_str;
    if( hasProperties(BTGattChar::PropertyBitVal::Notify) || hasProperties(BTGattChar::PropertyBitVal::Indicate) ) {
        notify_str = ", enabled[notify "+std::to_string(enabledNotifyState)+", indicate "+std::to_string(enabledIndicateState)+"]";
    }
    return "Char[handle "+to_hexstring(handle)+", props "+to_hexstring(properties)+" "+to_string(properties)+
            char_name+desc_str+", ccd-idx "+std::to_string(clientCharConfigIndex)+notify_str+
           ", value[type 0x"+value_type->toString()+", handle "+to_hexstring(value_handle)+
           "]]";
}

std::string BTGattChar::toShortString() const noexcept {
    std::string char_name;

    if( uuid_t::TypeSize::UUID16_SZ == value_type->getTypeSize() ) {
        const uint16_t uuid16 = (static_cast<const uuid16_t*>(value_type.get()))->value;
        char_name = ", "+GattCharacteristicTypeToString(static_cast<GattCharacteristicType>(uuid16));
    }
    {
        BTGattDescRef ud = getUserDescription();
        if( nullptr != ud ) {
            char_name.append( ", '" + dfa_utf8_decode( ud->value.get_ptr(), ud->value.size() ) + "'");
        }
    }
    std::string notify_str;
    if( hasProperties(BTGattChar::PropertyBitVal::Notify) || hasProperties(BTGattChar::PropertyBitVal::Indicate) ) {
        notify_str = ", enabled[notify "+std::to_string(enabledNotifyState)+", indicate "+std::to_string(enabledIndicateState)+"]";
    }
    return "Char[handle "+to_hexstring(handle)+", props "+to_hexstring(properties)+" "+to_string(properties)+
            char_name+", value[handle "+to_hexstring(value_handle)+
           "], ccd-idx "+std::to_string(clientCharConfigIndex)+notify_str+"]";
}

std::shared_ptr<BTGattHandler> BTGattChar::getGattHandlerUnchecked() const noexcept {
    std::shared_ptr<BTGattService> s = getServiceUnchecked();
    if( nullptr != s ) {
        return s->getGattHandlerUnchecked();
    }
    return nullptr;
}

std::shared_ptr<BTDevice> BTGattChar::getDeviceUnchecked() const noexcept {
    std::shared_ptr<BTGattService> s = getServiceUnchecked();
    if( nullptr != s ) {
        return s->getDeviceUnchecked();
    }
    return nullptr;
}

bool BTGattChar::configNotificationIndication(const bool enableNotification, const bool enableIndication, bool enabledState[2]) noexcept {
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
        } else {
            ERR_PRINT("Characteristic's device GATTHandle not connected: %s", toShortString().c_str());
        }
        return false;
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

bool BTGattChar::enableNotificationOrIndication(bool enabledState[2]) noexcept {
    const bool hasEnableNotification = hasProperties(BTGattChar::PropertyBitVal::Notify);
    const bool hasEnableIndication = hasProperties(BTGattChar::PropertyBitVal::Indicate);

    const bool enableNotification = hasEnableNotification;
    const bool enableIndication = !enableNotification && hasEnableIndication;

    return configNotificationIndication(enableNotification, enableIndication, enabledState);
}

bool BTGattChar::disableIndicationNotification() noexcept {
    bool enabledState[2];
    return configNotificationIndication(false, false, enabledState);
}

class DelegatedBTGattCharListener : public BTGattCharListener {
    private:
        const BTGattChar * associatedChar;
        std::shared_ptr<BTGattChar::Listener> delegate;

    public:
        DelegatedBTGattCharListener(const BTGattChar * characteristicMatch, std::shared_ptr<BTGattChar::Listener> l) noexcept
        : associatedChar(characteristicMatch), delegate(l) { }

        const char * type_name() const noexcept override;

        bool match(const BTGattChar & characteristic) noexcept override {
            if( nullptr == associatedChar ) {
                return true;
            }
            return *associatedChar == characteristic;
        }

        void notificationReceived(BTGattCharRef charDecl,
                                  const TROOctets& charValue, const uint64_t timestamp) override {
            delegate->notificationReceived(charDecl, charValue, timestamp);
        }

        void indicationReceived(BTGattCharRef charDecl,
                                const TROOctets& charValue, const uint64_t timestamp,
                                const bool confirmationSent) override {
            delegate->indicationReceived(charDecl, charValue, timestamp, confirmationSent);
        }

        std::string toString() override {
            return std::string(type_name())+"["+jau::to_string(this)+", delegate "+jau::to_string(delegate.get())+"]";
        }

        /**
         * Comparison operator merely testing for same memory reference of the delegate.
         */
        bool operator==(const BTGattCharListener& rhs) const noexcept override;

        bool operator!=(const DelegatedBTGattCharListener& rhs) const noexcept
        { return !(*this == rhs); }
};

/**
 * Simple access and provision of a typename string representation
 * at compile time like RTTI via jau::type_name_cue.
 */
JAU_TYPENAME_CUE(DelegatedBTGattCharListener)

const char * DelegatedBTGattCharListener::type_name() const noexcept { return jau::type_name_cue<DelegatedBTGattCharListener>::name(); }

bool DelegatedBTGattCharListener::operator==(const BTGattCharListener& rhs) const noexcept
{
    if( 0 != strcmp(rhs.type_name(), jau::type_name_cue<DelegatedBTGattCharListener>::name()) ) {
        return false;
    }
    const DelegatedBTGattCharListener& rhs2 = static_cast<const DelegatedBTGattCharListener&>(rhs);
    return delegate.get() == rhs2.delegate.get();
}

bool BTGattChar::addCharListener(std::shared_ptr<BTGattChar::Listener> l) noexcept {
    std::shared_ptr<BTDevice> device = getDeviceUnchecked();
    if( nullptr == device ) {
        ERR_PRINT("Characteristic's device null: %s", toShortString().c_str());
        return false;
    }
    return device->addCharListener( std::make_shared<DelegatedBTGattCharListener>( this, l ) );
}

bool BTGattChar::addCharListener(std::shared_ptr<BTGattChar::Listener> l, bool enabledState[2]) noexcept {
    if( !enableNotificationOrIndication(enabledState) ) {
        return false;
    }
    return addCharListener(l);
}

bool BTGattChar::removeCharListener(std::shared_ptr<Listener> l) noexcept {
    std::shared_ptr<BTDevice> device = getDeviceUnchecked();
    if( nullptr == device ) {
        ERR_PRINT("Characteristic's device null: %s", toShortString().c_str());
        return false;
    }
    return device->removeCharListener( std::make_shared<DelegatedBTGattCharListener>( this, l ) );
}

int BTGattChar::removeAllAssociatedCharListener(bool shallDisableIndicationNotification) noexcept {
    if( shallDisableIndicationNotification ) {
        disableIndicationNotification();
    }
    std::shared_ptr<BTDevice> device = getDeviceUnchecked();
    if( nullptr == device ) {
        ERR_PRINT("Characteristic's device null: %s", toShortString().c_str());
        return 0;
    }
    return device->removeAllAssociatedCharListener(this);
}

bool BTGattChar::readValue(POctets & res, int expectedLength) noexcept {
    std::shared_ptr<BTDevice> device = getDeviceUnchecked();
    if( nullptr == device ) {
        ERR_PRINT("Characteristic's device null: %s", toShortString().c_str());
        return false;
    }
    std::shared_ptr<BTGattHandler> gatt = device->getGattHandler();
    if( nullptr == gatt ) {
        ERR_PRINT("Characteristic's device GATTHandle not connected: %s", toShortString().c_str());
        return false;
    }
    return gatt->readCharacteristicValue(*this, res, expectedLength);
}
/**
 * BT Core Spec v5.2: Vol 3, Part G GATT: 4.9.3 Write Characteristic Value
 */
bool BTGattChar::writeValue(const TROOctets & value) noexcept {
    std::shared_ptr<BTDevice> device = getDeviceUnchecked();
    if( nullptr == device ) {
        ERR_PRINT("Characteristic's device null: %s", toShortString().c_str());
        return false;
    }
    std::shared_ptr<BTGattHandler> gatt = device->getGattHandler();
    if( nullptr == gatt ) {
        ERR_PRINT("Characteristic's device GATTHandle not connected: %s", toShortString().c_str());
        return false;
    }
    return gatt->writeCharacteristicValue(*this, value);
}

/**
 * BT Core Spec v5.2: Vol 3, Part G GATT: 4.9.1 Write Characteristic Value Without Response
 */
bool BTGattChar::writeValueNoResp(const TROOctets & value) noexcept {
    std::shared_ptr<BTDevice> device = getDeviceUnchecked();
    if( nullptr == device ) {
        ERR_PRINT("Characteristic's device null: %s", toShortString().c_str());
        return false;
    }
    std::shared_ptr<BTGattHandler> gatt = device->getGattHandler();
    if( nullptr == gatt ) {
        ERR_PRINT("Characteristic's device GATTHandle not connected: %s", toShortString().c_str());
        return false;
    }
    return gatt->writeCharacteristicValueNoResp(*this, value);
}
