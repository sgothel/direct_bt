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

#ifndef BT_TYPES1_HPP_
#define BT_TYPES1_HPP_

#include <mutex>
#include <atomic>

#include <jau/java_uplink.hpp>
#include <jau/basic_types.hpp>
#include <jau/uuid.hpp>

#include "BTAddress.hpp"
#include "BTTypes0.hpp"

namespace direct_bt {

    class BTAdapter; // forward
    class BTDevice; // forward

    /** \addtogroup DBTUserAPI
     *
     *  @{
     */

    class BTObject : public jau::jni::JavaUplink
    {
        protected:
            std::atomic_bool instance_valid;

            BTObject() noexcept : instance_valid(true) {}

        public:
            std::string toString() const noexcept override { return "BTObject["+jau::to_hexstring(this)+"]"; }

            ~BTObject() noexcept override {
                instance_valid = false;
            }

            /**
             * Returns whether the object's reference is valid and in a general operational state.
             */
            inline bool isValidInstance() const noexcept { return instance_valid.load(); }

            void checkValidInstance() const override {
                if( !isValidInstance() ) {
                    throw jau::IllegalStateException("BTObject::checkValidInstance: Invalid object: "+jau::to_hexstring(this), E_FILE_LINE);
                }
            }
    };
    inline std::string to_string(const BTObject& o) noexcept { return o.toString(); }

    /**
     * mgmt_addr_info { EUI48, uint8_t type },
     * int8_t rssi,
     * int8_t tx_power,
     * int8_t max_tx_power;
     */
    class ConnectionInfo
    {
        private:
            jau::EUI48 address;
            BDAddressType addressType;
            int8_t rssi;
            int8_t tx_power;
            int8_t max_tx_power;

        public:
            static jau::nsize_t minimumDataSize() noexcept { return 6 + 1 + 1 + 1 + 1; }

            ConnectionInfo(const jau::EUI48 &address_, BDAddressType addressType_, int8_t rssi_, int8_t tx_power_, int8_t max_tx_power_) noexcept
            : address(address_), addressType(addressType_), rssi(rssi_), tx_power(tx_power_), max_tx_power(max_tx_power_) {}

            const jau::EUI48 getAddress() const noexcept { return address; }
            BDAddressType getAddressType() const noexcept { return addressType; }
            int8_t getRSSI() const noexcept { return rssi; }
            int8_t getTxPower() const noexcept { return tx_power; }
            int8_t getMaxTxPower() const noexcept { return max_tx_power; }

            std::string toString() const noexcept {
                return "address="+getAddress().toString()+", addressType "+to_string(getAddressType())+
                       ", rssi "+std::to_string(rssi)+
                       ", tx_power[set "+std::to_string(tx_power)+", max "+std::to_string(tx_power)+"]";
            }
    };

    class NameAndShortName
    {
        friend class BTManager; // top manager
        friend class BTAdapter; // direct manager

        private:
            std::string name;
            std::string short_name;

        protected:
            void setName(const std::string v) noexcept { name = v; }
            void setShortName(const std::string v) noexcept { short_name = v; }

        public:
            NameAndShortName() noexcept
            : name(), short_name() {}

            NameAndShortName(const std::string & name_, const std::string & short_name_) noexcept
            : name(name_), short_name(short_name_) {}

            std::string getName() const noexcept { return name; }
            std::string getShortName() const noexcept { return short_name; }

            std::string toString() const noexcept {
                return "name '"+getName()+"', shortName '"+getShortName()+"'";
            }
    };

    /**
     * Adapter Setting Bits.
     * <p>
     * Used to denote specific bits or as a bit-mask.
     * </p>
     */
    enum class AdapterSetting : uint32_t {
        NONE               =          0,
        POWERED            = 0x00000001,
        CONNECTABLE        = 0x00000002,
        FAST_CONNECTABLE   = 0x00000004,
        DISCOVERABLE       = 0x00000008,
        BONDABLE           = 0x00000010,
        LINK_SECURITY      = 0x00000020,
        SSP                = 0x00000040,
        BREDR              = 0x00000080,
        HS                 = 0x00000100,
        LE                 = 0x00000200,
        ADVERTISING        = 0x00000400,
        SECURE_CONN        = 0x00000800,
        DEBUG_KEYS         = 0x00001000,
        PRIVACY            = 0x00002000,
        CONFIGURATION      = 0x00004000,
        STATIC_ADDRESS     = 0x00008000,
        PHY_CONFIGURATION  = 0x00010000
    };
    constexpr AdapterSetting operator ~(const AdapterSetting rhs) noexcept {
        return static_cast<AdapterSetting> ( ~static_cast<uint32_t>(rhs) );
    }
    constexpr AdapterSetting operator ^(const AdapterSetting lhs, const AdapterSetting rhs) noexcept {
        return static_cast<AdapterSetting> ( static_cast<uint32_t>(lhs) ^ static_cast<uint32_t>(rhs) );
    }
    constexpr AdapterSetting operator |(const AdapterSetting lhs, const AdapterSetting rhs) noexcept {
        return static_cast<AdapterSetting> ( static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs) );
    }
    constexpr AdapterSetting operator &(const AdapterSetting lhs, const AdapterSetting rhs) noexcept {
        return static_cast<AdapterSetting> ( static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs) );
    }
    constexpr bool operator ==(const AdapterSetting lhs, const AdapterSetting rhs) noexcept {
        return static_cast<uint32_t>(lhs) == static_cast<uint32_t>(rhs);
    }
    constexpr bool operator !=(const AdapterSetting lhs, const AdapterSetting rhs) noexcept {
        return !( lhs == rhs );
    }

    constexpr AdapterSetting getAdapterSettingMaskDiff(const AdapterSetting setting_a, const AdapterSetting setting_b) noexcept { return setting_a ^ setting_b; }

    constexpr bool isAdapterSettingBitSet(const AdapterSetting mask, const AdapterSetting bit) noexcept { return AdapterSetting::NONE != ( mask & bit ); }

    constexpr void setAdapterSettingMaskBit(AdapterSetting &mask, const AdapterSetting bit) noexcept { mask = mask | bit; }
    constexpr void clrAdapterSettingMaskBit(AdapterSetting &mask, const AdapterSetting bit) noexcept { mask = mask & ~bit; }

    std::string to_string(const AdapterSetting settingBitMask) noexcept;

    /** Maps the given {@link AdapterSetting} to {@link BTMode} */
    BTMode getAdapterSettingsBTMode(const AdapterSetting settingMask) noexcept;

    class AdapterInfo
    {
        public:
            const uint16_t dev_id;
            /**
             * The adapter's address initially reported by the system is always its public address, i.e. BDAddressType::BDADDR_LE_PUBLIC.
             * <p>
             * Subsequent adapter setup using BDAddressType::BDADDR_LE_RANDOM must be handled within BTAdapter
             * and is not reflected in AdapterInfo.
             * </p>
             */
            const BDAddressAndType addressAndType;
            const uint8_t version;
            const uint16_t manufacturer;

        private:
            AdapterSetting supported_setting;
            std::atomic<AdapterSetting> current_setting;
            uint32_t dev_class;
            std::string name;
            std::string short_name;

        public:
            AdapterInfo(const uint16_t dev_id_, const BDAddressAndType & addressAndType_,
                        const uint8_t version_, const uint16_t manufacturer_,
                        const AdapterSetting supported_setting_, const AdapterSetting current_setting_,
                        const uint32_t dev_class_, const std::string & name_, const std::string & short_name_) noexcept
            : dev_id(dev_id_), addressAndType(addressAndType_), version(version_),
              manufacturer(manufacturer_),
              supported_setting(supported_setting_),
              current_setting(current_setting_), dev_class(dev_class_),
              name(name_), short_name(short_name_)
            { }

            AdapterInfo(const AdapterInfo &o) noexcept
            : dev_id(o.dev_id), addressAndType(o.addressAndType), version(o.version),
              manufacturer(o.manufacturer),
              supported_setting(o.supported_setting),
              current_setting(o.current_setting.load()), dev_class(o.dev_class),
              name(o.name), short_name(o.short_name)
            { }
            AdapterInfo& operator=(const AdapterInfo &o) {
                if( this != &o ) {
                    if( dev_id != o.dev_id || addressAndType != o.addressAndType ) {
                        throw jau::IllegalArgumentException("Can't assign different device id's or address "+o.toString()+" -> "+toString(), E_FILE_LINE);
                    }
                    supported_setting   = o.supported_setting;
                    current_setting     = o.current_setting.load();
                    dev_class           = o.dev_class;
                    name                = o.name;
                    short_name          = o.short_name;
                }
                return *this;
            }
            AdapterInfo(AdapterInfo&& o) noexcept
            : dev_id(std::move(o.dev_id)), addressAndType(std::move(o.addressAndType)), version(std::move(o.version)),
              manufacturer(std::move(o.manufacturer)),
              supported_setting(std::move(o.supported_setting)),
              current_setting(o.current_setting.load()), dev_class(std::move(o.dev_class)),
              name(std::move(o.name)), short_name(std::move(o.short_name))
            { }
            AdapterInfo& operator=(AdapterInfo &&o) noexcept = delete;

            /**
             * Assigns the given 'new_setting & supported_setting' to the current_setting.
             * @param new_setting assigned to current_setting after masking with supported_setting.
             * @return 'new_setting & supported_setting', i.e. the new current_setting.
             */
            AdapterSetting setCurrentSettingMask(const AdapterSetting new_setting) noexcept {
                const AdapterSetting _current_setting = new_setting & supported_setting;
                current_setting = _current_setting;
                return _current_setting;
            }
            void setSettingMasks(const AdapterSetting supported_setting_, const AdapterSetting current_setting_) noexcept {
                supported_setting = supported_setting_;
                current_setting = current_setting_;
            }
            void setDevClass(const uint32_t v) noexcept { dev_class = v; }
            void setName(const std::string v) noexcept { name = v; }
            void setShortName(const std::string v) noexcept { short_name = v; }

            constexpr const AdapterSetting& get_supportedSetting() const noexcept { return supported_setting; }

            bool isSettingMaskSupported(const AdapterSetting setting) const noexcept {
                return setting == ( setting & supported_setting );
            }
            AdapterSetting getCurrentSettingMask() const noexcept { return current_setting; }
            bool isCurrentSettingBitSet(const AdapterSetting bit) const noexcept { return AdapterSetting::NONE != ( current_setting & bit ); }

            /** Map {@link #getCurrentSettingMask()} to {@link BTMode} */
            BTMode getCurrentBTMode() const noexcept { return getAdapterSettingsBTMode(current_setting); }

            uint32_t getDevClass() const noexcept { return dev_class; }
            std::string getName() const noexcept { return name; }
            std::string getShortName() const noexcept { return short_name; }

            std::string toString() const noexcept {
                return "AdapterInfo[id "+std::to_string(dev_id)+", address "+addressAndType.toString()+", version "+std::to_string(version)+
                        ", manuf "+std::to_string(manufacturer)+
                        ", settings[sup "+to_string(supported_setting)+", cur "+to_string(current_setting)+
                        "], name '"+name+"', shortName '"+short_name+"']";
            }
    };
    inline std::string to_string(const AdapterInfo& a) noexcept { return a.toString(); }

    /**@}*/

} // namespace direct_bt

#endif /* BT_TYPES1_HPP_ */
