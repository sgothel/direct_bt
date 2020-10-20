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

#ifndef DBT_TYPES_HPP_
#define DBT_TYPES_HPP_

#include <mutex>
#include <atomic>

#include <jau/java_uplink.hpp>

#include "UUID.hpp"
#include "BTAddress.hpp"
#include "BTTypes.hpp"

#define JAVA_DBT_PACKAGE "direct_bt/tinyb/"
#define JAVA_MAIN_PACKAGE "org/tinyb"
#define JAVA_HCI_PACKAGE "tinyb/hci"

namespace direct_bt {

    class DBTAdapter; // forward
    class DBTDevice; // forward

    class DBTObject : public jau::JavaUplink
    {
        protected:
            std::atomic_bool valid;
            std::mutex lk;

            DBTObject() noexcept : valid(true) {}

            bool lock() noexcept {
                 if (valid) {
                     lk.lock();
                     return true;
                 } else {
                     return false;
                 }
             }

             void unlock() noexcept {
                 lk.unlock();
             }

        public:
            virtual std::string toString() const noexcept override { return "DBTObject["+aptrHexString(this)+"]"; }

            virtual ~DBTObject() noexcept {
                valid = false;
            }

            inline bool isValid() const noexcept { return valid.load(); }

            /**
             * Throws an IllegalStateException if isValid() == false
             */
            void checkValid() const override {
                if( !isValid() ) {
                    throw jau::IllegalStateException("DBTObject state invalid: "+aptrHexString(this), E_FILE_LINE);
                }
            }
    };

    /**
     * mgmt_addr_info { EUI48, uint8_t type },
     * int8_t rssi,
     * int8_t tx_power,
     * int8_t max_tx_power;
     */
    class ConnectionInfo
    {
        private:
            EUI48 address;
            BDAddressType addressType;
            int8_t rssi;
            int8_t tx_power;
            int8_t max_tx_power;

        public:
            static size_t minimumDataSize() noexcept { return 6 + 1 + 1 + 1 + 1; }

            ConnectionInfo(const EUI48 &address_, BDAddressType addressType_, int8_t rssi_, int8_t tx_power_, int8_t max_tx_power_) noexcept
            : address(address_), addressType(addressType_), rssi(rssi_), tx_power(tx_power_), max_tx_power(max_tx_power_) {}

            const EUI48 getAddress() const noexcept { return address; }
            BDAddressType getAddressType() const noexcept { return addressType; }
            int8_t getRSSI() const noexcept { return rssi; }
            int8_t getTxPower() const noexcept { return tx_power; }
            int8_t getMaxTxPower() const noexcept { return max_tx_power; }

            std::string toString() const noexcept {
                return "address="+getAddress().toString()+", addressType "+getBDAddressTypeString(getAddressType())+
                       ", rssi "+std::to_string(rssi)+
                       ", tx_power[set "+std::to_string(tx_power)+", max "+std::to_string(tx_power)+"]";
            }
    };

    class NameAndShortName
    {
        friend class DBTManager; // top manager
        friend class DBTAdapter; // direct manager

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

    inline void setAdapterSettingMaskBit(AdapterSetting &mask, const AdapterSetting bit) noexcept { mask = mask | bit; }
    std::string getAdapterSettingBitString(const AdapterSetting settingBit) noexcept;
    std::string getAdapterSettingMaskString(const AdapterSetting settingBitMask) noexcept;

    /** Maps the given {@link AdapterSetting} to {@link BTMode} */
    BTMode getAdapterSettingsBTMode(const AdapterSetting settingMask) noexcept;

    class AdapterInfo
    {
        friend class DBTManager; // top manager
        friend class DBTAdapter; // direct manager

        public:
            const int dev_id;
            const EUI48 address;
            const uint8_t version;
            const uint16_t manufacturer;
            const AdapterSetting supported_setting;

        private:
            std::atomic<AdapterSetting> current_setting;
            uint32_t dev_class;
            std::string name;
            std::string short_name;

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
            void setDevClass(const uint32_t v) noexcept { dev_class = v; }
            void setName(const std::string v) noexcept { name = v; }
            void setShortName(const std::string v) noexcept { short_name = v; }

        public:
            AdapterInfo(const int dev_id_, const EUI48 & address_,
                        const uint8_t version_, const uint16_t manufacturer_,
                        const AdapterSetting supported_setting_, const AdapterSetting current_setting_,
                        const uint32_t dev_class_, const std::string & name_, const std::string & short_name_) noexcept
            : dev_id(dev_id_), address(address_), version(version_),
              manufacturer(manufacturer_), supported_setting(supported_setting_),
              current_setting(current_setting_), dev_class(dev_class_),
              name(name_), short_name(short_name_)
            { }

            bool isSettingMaskSupported(const AdapterSetting setting) const noexcept {
                return setting == ( setting & supported_setting );
            }
            AdapterSetting getCurrentSettingMask() const noexcept { return current_setting; }
            bool isCurrentSettingBitSet(const AdapterSetting bit) noexcept { return AdapterSetting::NONE != ( current_setting & bit ); }

            /** Map {@link #getCurrentSettingMask()} to {@link BTMode} */
            BTMode getCurrentBTMode() const noexcept { return getAdapterSettingsBTMode(current_setting); }

            uint32_t getDevClass() const noexcept { return dev_class; }
            std::string getName() const noexcept { return name; }
            std::string getShortName() const noexcept { return short_name; }

            std::string toString() const noexcept {
                return "Adapter[id "+std::to_string(dev_id)+", address "+address.toString()+", version "+std::to_string(version)+
                        ", manuf "+std::to_string(manufacturer)+
                        ", settings[sup "+getAdapterSettingMaskString(supported_setting)+", cur "+getAdapterSettingMaskString(current_setting)+
                        "], name '"+name+"', shortName '"+short_name+"']";
            }
    };

} // namespace direct_bt

#endif /* DBT_TYPES_HPP_ */
