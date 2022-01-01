/*
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2021 Gothel Software e.K.
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

#ifndef BT_ADDRESS_HPP_
#define BT_ADDRESS_HPP_

#include <cstring>
#include <string>
#include <cstdint>
#include <functional>

#include <jau/packed_attribute.hpp>
#include <jau/ordered_atomic.hpp>
#include <jau/eui48.hpp>

using jau::EUI48;
using jau::EUI48Sub;

namespace direct_bt {

    /**
     * BT Core Spec v5.2:  Vol 3, Part C Generic Access Profile (GAP): 15.1.1.1 Public Bluetooth address
     * <pre>
     * 1) BT public address used as BD_ADDR for BR/EDR physical channel is defined in Vol 2, Part B 1.2
     * - EUI-48 or MAC (6 octets)
     *
     * 2) BT public address used as BD_ADDR for the LE physical channel is defined in Vol 6, Part B 1.3
     *    BT Core Spec v5.2:  Vol 3, Part C Generic Access Profile (GAP): 15.1.1.2 Random Bluetooth address
     *
     * 3) BT random address used as BD_ADDR on the LE physical channel is defined in Vol 3, Part C 10.8
     * </pre>
     */
    enum class BDAddressType : uint8_t {
        /** Bluetooth BREDR address */
        BDADDR_BREDR      = 0x00,
        /** Bluetooth LE public address */
        BDADDR_LE_PUBLIC  = 0x01,
        /** Bluetooth LE random address, see {@link BLERandomAddressType} */
        BDADDR_LE_RANDOM  = 0x02,
        /** Undefined */
        BDADDR_UNDEFINED  = 0xff
    };
    constexpr BDAddressType to_BDAddressType(const uint8_t v) noexcept {
        if( v <= 2 ) {
            return static_cast<BDAddressType>(v);
        }
        return BDAddressType::BDADDR_UNDEFINED;
    }
    constexpr uint8_t number(const BDAddressType rhs) noexcept {
        return static_cast<uint8_t>(rhs);
    }
    std::string to_string(const BDAddressType type) noexcept;

    /**
     * BT Core Spec v5.2:  Vol 6 LE, Part B Link Layer Specification: 1.3 Device Address
     * <p>
     * BT Core Spec v5.2:  Vol 6 LE, Part B Link Layer Specification: 1.3.2 Random device Address
     * </p>
     * <p>
     * Table 1.2, address bits [47:46]
     * </p>
     * <p>
     * If {@link BDAddressType} is {@link BDAddressType::BDADDR_LE_RANDOM},
     * its value shall be different than {@link BLERandomAddressType::UNDEFINED}.
     * </p>
     * <p>
     * If {@link BDAddressType} is not {@link BDAddressType::BDADDR_LE_RANDOM},
     * its value shall be {@link BLERandomAddressType::UNDEFINED}.
     * </p>
     */
    enum class BLERandomAddressType : uint8_t {
        /** Non-resolvable private random device address 0b00 */
        UNRESOLVABLE_PRIVAT = 0x00,
        /**
         * Resolvable private random device address 0b01.
         *
         * Requires Local Identity Resolving Key (IRK) or the Peer Identity Resolving Key (IRK).
         *
         * EUI48: 24 bits hash = ag(IRK, prand), 24 bits prand.
         */
        RESOLVABLE_PRIVAT   = 0x01,
        /** Reserved for future use 0b10 */
        RESERVED            = 0x02,
        /** Static public 'random' device address 0b11. Not changing between power-cycles. */
        STATIC_PUBLIC       = 0x03,
        /** Undefined, e.g. address not of type {@link BDAddressType::BDADDR_LE_RANDOM} */
        UNDEFINED           = 0xff
    };
    constexpr uint8_t number(const BLERandomAddressType rhs) noexcept { return static_cast<uint8_t>(rhs); }
    std::string to_string(const BLERandomAddressType type) noexcept;

    /**
     * HCI LE Address-Type is PUBLIC: 0x00, RANDOM: 0x01
     * <p>
     * BT Core Spec v5.2:  Vol 4, Part E Host Controller Interface (HCI) Functionality:
     * <pre>
     * > 7.8.5: LE Set Advertising Parameters command
     * -- Own_Address_Type: public: 0x00 (default), random: 0x01, resolvable-1: 0x02, resolvable-2: 0x03
     * > 7.8.10: LE Set Scan Parameters command
     * -- Own_Address_Type: public: 0x00 (default), random: 0x01, resolvable-1: 0x02, resolvable-2: 0x03
     * > 7.8.12: LE Create Connection command
     * -- Own_Address_Type: public: 0x00 (default), random: 0x01,
     *    Public Identity Address (resolvable-1, any not supporting LE_Set_Privacy_Mode command): 0x02,
     *    Random (static) Identity Address (resolvable-2, any not supporting LE_Set_Privacy_Mode command): 0x03
     * </pre>
     * </p>
     */
    enum class HCILEPeerAddressType : uint8_t {
        /** Public Device Address */
        PUBLIC = 0x00,
        /** Random Device Address */
        RANDOM = 0x01,
        /** Public Resolved Identity Address */
        PUBLIC_IDENTITY = 0x02,
        /** Resolved Random (Static) Identity Address */
        RANDOM_STATIC_IDENTITY = 0x03,
        UNDEFINED = 0xff /**< HCIADDR_UNDEFINED */
    };
    constexpr uint8_t number(const HCILEPeerAddressType rhs) noexcept { return static_cast<uint8_t>(rhs); }
    BDAddressType to_BDAddressType(const HCILEPeerAddressType hciPeerAddrType) noexcept;
    std::string to_string(const HCILEPeerAddressType type) noexcept;

    enum class HCILEOwnAddressType : uint8_t {
        /** Public Device Address */
        PUBLIC = 0x00,
        /** Random Device Address */
        RANDOM = 0x01,
        /** Controller Resolved Private Address or Public Address */
        RESOLVABLE_OR_PUBLIC = 0x02,
        /** Controller Resolved Private Address or Random Address */
        RESOLVABLE_OR_RANDOM = 0x03,
        UNDEFINED = 0xff
    };
    constexpr uint8_t number(const HCILEOwnAddressType rhs) noexcept { return static_cast<uint8_t>(rhs); }
    BDAddressType to_BDAddressType(const HCILEOwnAddressType hciOwnAddrType) noexcept;
    HCILEOwnAddressType to_HCILEOwnAddressType(const BDAddressType addrType) noexcept;
    std::string to_string(const HCILEOwnAddressType type) noexcept;


    /**
     * Unique Bluetooth EUI48 address and ::BDAddressType tuple.
     * <p>
     * Bluetooth EUI48 address itself is not unique as it requires the ::BDAddressType bits.<br>
     * E.g. there could be two devices with the same EUI48 address, one using ::BDAddressType::BDADDR_LE_PUBLIC
     * and one using ::BDAddressType::BDADDR_LE_RANDOM being a ::BLERandomAddressType::RESOLVABLE_PRIVAT.
     * </p>
     */
    class BDAddressAndType {
        public:
            /** Using EUI48::ANY_DEVICE and ::BDAddressType::BDADDR_BREDR to match any BREDR device. */
            static const BDAddressAndType ANY_BREDR_DEVICE;

            /**
             * Using EUI48::ANY_DEVICE and ::BDAddressType::BDADDR_UNDEFINED to match any device.<br>
             * This constant is suitable to {@link #matches(BDAddressAndType) any device.
             */
            static const BDAddressAndType ANY_DEVICE;

            jau::EUI48 address;
            BDAddressType type;

        private:
            jau::relaxed_atomic_size_t hash = 0; // default 0, cache

        public:
            BDAddressAndType(const jau::EUI48 & address_, BDAddressType type_)
            : address(address_), type(type_) {}

            constexpr BDAddressAndType() noexcept : address(), type{BDAddressType::BDADDR_UNDEFINED} { }
            BDAddressAndType(const BDAddressAndType &o) noexcept : address(o.address), type(o.type) { }
            BDAddressAndType(BDAddressAndType &&o) noexcept {
                address = std::move(o.address);
                type = std::move(o.type);
            }
            constexpr BDAddressAndType& operator=(const BDAddressAndType &o) noexcept {
                address = o.address;
                type = o.type;
                return *this;
            }
            BDAddressAndType& operator=(BDAddressAndType &&o) noexcept {
                address = std::move(o.address);
                type = std::move(o.type);
                return *this;
            }

            /**
             * Returns true if the BDAddressType is a LE address type.
             */
            constexpr bool isLEAddress() const noexcept {
                return BDAddressType::BDADDR_LE_PUBLIC == type || BDAddressType::BDADDR_LE_RANDOM == type;
            }

            /**
             * Returns true if the BDAddressType is a BREDR address type.
             */
            constexpr bool isBREDRAddress() const noexcept { return BDAddressType::BDADDR_BREDR == type; }

            /**
             * Returns the BLERandomAddressType.
             * <p>
             * If ::BDAddressType is ::BDAddressType::BDADDR_LE_RANDOM,
             * method shall return a valid value other than ::BLERandomAddressType::UNDEFINED.
             * </p>
             * <p>
             * If BDAddressType is not ::BDAddressType::BDADDR_LE_RANDOM,
             * method shall return ::BLERandomAddressType::UNDEFINED.
             * </p>
             * @since 2.2.0
             */
            static BLERandomAddressType getBLERandomAddressType(const jau::EUI48& address, const BDAddressType addressType) noexcept;

            /**
             * Returns the BLERandomAddressType std::string representation.
             * <p>
             * If ::BDAddressType is ::BDAddressType::BDADDR_LE_RANDOM,
             * method shall return the given prefix and a valid string value other than ::BLERandomAddressType::UNDEFINED.
             * </p>
             * <p>
             * If BDAddressType is not ::BDAddressType::BDADDR_LE_RANDOM,
             * method shall return an empty string.
             * </p>
             * @since 2.5.3
             */
            static std::string getBLERandomAddressTypeString(const jau::EUI48& address, const BDAddressType addressType, const std::string& prefix) noexcept;

            /**
             * Returns the BLERandomAddressType.
             * <p>
             * If type is ::BDAddressType::BDADDR_LE_RANDOM},
             * method shall return a valid value other than ::BLERandomAddressType::UNDEFINED.
             * </p>
             * <p>
             * If type is not ::BDAddressType::BDADDR_LE_RANDOM,
             * method shall return ::BLERandomAddressType::UNDEFINED.
             * </p>
             * @since 2.0.0
             */
            BLERandomAddressType getBLERandomAddressType() const noexcept {
                return getBLERandomAddressType(address, type);
            }


            /**
             * Returns true if both devices match, i.e. equal address
             * and equal type or at least one type is {@link BDAddressType#BDADDR_UNDEFINED}.
             */
            bool matches(const BDAddressAndType & o) const noexcept {
                if(this == &o) {
                    return true;
                }
                return address == o.address &&
                       ( type == o.type ||
                         BDAddressType::BDADDR_UNDEFINED == type ||
                         BDAddressType::BDADDR_UNDEFINED == o.type );
            }

            /**
             * Implementation uses a lock-free volatile cache.
             */
            std::size_t hash_code() const noexcept {
                std::size_t h = hash;
                if( 0 == h ) {
                    // 31 * x == (x << 5) - x
                    h = 31 + address.hash_code();
                    h = ((h << 5) - h) + number(type);
                    const_cast<BDAddressAndType *>(this)->hash = h;
                }
                return h;
            }

            /**
             * Method clears the cached hash value.
             * @see #clear()
             */
            void clearHash() { hash = 0; }

            /**
             * Method clears the underlying byte array {@link #b} and cached hash value.
             * @see #clearHash()
             */
            void clear() {
                hash = 0;
                address.clear();
                type = BDAddressType::BDADDR_UNDEFINED;
            }

            std::string toString() const noexcept;
    };
    inline bool operator==(const BDAddressAndType& lhs, const BDAddressAndType& rhs) noexcept {
        if( &lhs == &rhs ) {
            return true;
        }
        return lhs.address == rhs.address &&
               lhs.type == rhs.type;
    }
    inline bool operator!=(const BDAddressAndType& lhs, const BDAddressAndType& rhs) noexcept
    { return !(lhs == rhs); }

    inline std::string to_string(const BDAddressAndType& a) noexcept { return a.toString(); }

} // namespace direct_bt

// injecting specialization of std::hash to namespace std of our types above
namespace std
{
    template<> struct hash<direct_bt::BDAddressAndType> {
        std::size_t operator()(direct_bt::BDAddressAndType const& a) const noexcept {
            return a.hash_code();
        }
    };
}

#endif /* BT_ADDRESS_HPP_ */
