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

#ifndef BT_GATT_DESCRIPTOR_HPP_
#define BT_GATT_DESCRIPTOR_HPP_

#include <jau/octets.hpp>
#include <jau/uuid.hpp>
#include <cstring>
#include <string>
#include <memory>
#include <cstdint>

#include <mutex>
#include <atomic>

#include "BTTypes0.hpp"
#include "ATTPDUTypes.hpp"

#include "BTTypes1.hpp"

/**
 * - - - - - - - - - - - - - - -
 *
 * Module GATTDescriptor:
 *
 * - BT Core Spec v5.2: Vol 3, Part G Generic Attribute Protocol (GATT)
 * - BT Core Spec v5.2: Vol 3, Part G GATT: 2.6 GATT Profile Hierarchy
 */
namespace direct_bt {

    class BTDevice; // forward
    class BTGattHandler; // forward
    class BTGattChar; // forward
    typedef std::shared_ptr<BTGattChar> BTGattCharRef;

    /** \addtogroup DBTUserClientAPI
     *
     *  @{
     */

    /**
     * Representing a Gatt Characteristic Descriptor object from the ::GATTRole::Client perspective.
     *
     * A list of shared BTGattDesc instances is available from BTGattChar
     * via BTGattChar::descriptorList.
     *
     * See [Direct-BT Overview](namespacedirect__bt.html#details).
     *
     * BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3 Characteristic Descriptor
     */
    class BTGattDesc : public BTObject {
        private:
            /** Descriptor's characteristic weak back-reference */
            std::weak_ptr<BTGattChar> wbr_char;

            std::string toShortString() const noexcept;

        public:
            static const std::shared_ptr<jau::uuid_t> TYPE_EXT_PROP;
            static const std::shared_ptr<jau::uuid_t> TYPE_USER_DESC;
            static const std::shared_ptr<jau::uuid_t> TYPE_CCC_DESC;

            /**
             * Following UUID16 GATT profile attribute types are listed under:
             * BT Core Spec v5.2: Vol 3, Part G GATT: 3.4 Summary of GATT Profile Attribute Types
             *
             * See GattAttributeType for further non BTGattDesc related declarations.
             */
            enum Type : uint16_t {
                /* BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.1 Characteristic Extended Properties */
                CHARACTERISTIC_EXTENDED_PROPERTIES          = 0x2900,
                /* BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.2 Characteristic User Description (Characteristic Descriptor, optional, single, string) */
                CHARACTERISTIC_USER_DESCRIPTION             = 0x2901,
                /* BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.3 Client Characteristic Configuration (Characteristic Descriptor, optional, single, uint16_t bitfield) */
                CLIENT_CHARACTERISTIC_CONFIGURATION         = 0x2902,
                /* BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.4 Server Characteristic Configuration (Characteristic Descriptor, optional, single, bitfield) */
                SERVER_CHARACTERISTIC_CONFIGURATION         = 0x2903,
                /* BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.5 Characteristic Presentation Format (Characteristic Descriptor, optional, single, complex) */
                CHARACTERISTIC_PRESENTATION_FORMAT          = 0x2904,
                CHARACTERISTIC_AGGREGATE_FORMAT             = 0x2905,

                /** Our identifier to mark a custom vendor Characteristic Descriptor */
                CUSTOM_CHARACTERISTIC_DESCRIPTION           = 0x8888
            };

            /** Type of descriptor */
            std::unique_ptr<const jau::uuid_t> type;

            /**
             * Characteristic Descriptor Handle
             * <p>
             * Attribute handles are unique for each device (server) (BT Core Spec v5.2: Vol 3, Part F Protocol..: 3.2.2 Attribute Handle).
             * </p>
             */
            const uint16_t handle;

            /* Characteristics Descriptor's Value */
            jau::POctets value;

            BTGattDesc(const BTGattCharRef & characteristic, std::unique_ptr<const jau::uuid_t> && type_,
                           const uint16_t handle_) noexcept
            : wbr_char(characteristic), type(std::move(type_)), handle(handle_),
              value(jau::lb_endian::little /* intentional zero sized */)
            { }

            std::string get_java_class() const noexcept override {
                return java_class();
            }
            static std::string java_class() noexcept {
                return std::string(JAVA_DBT_PACKAGE "DBTGattDesc");
            }

            std::shared_ptr<BTGattChar> getGattCharUnchecked() const noexcept { return wbr_char.lock(); }
            std::shared_ptr<BTGattChar> getGattCharChecked() const;
            std::shared_ptr<BTGattHandler> getGattHandlerUnchecked() const noexcept;
            std::shared_ptr<BTDevice> getDeviceUnchecked() const noexcept;

            std::string toString() const noexcept override;

            /** Value is uint16_t bitfield */
            bool isExtendedProperties() const noexcept { return *TYPE_EXT_PROP == *type; }

            /* BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.3 Client Characteristic Configuration (Characteristic Descriptor, optional, single, uint16_t bitfield) */
            bool isClientCharConfig() const noexcept{ return *TYPE_CCC_DESC == *type; }

            /* BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.2 Characteristic User Description */
            bool isUserDescription() const noexcept{ return *TYPE_USER_DESC == *type; }

            /**
             * BT Core Spec v5.2: Vol 3, Part G GATT: 4.12.1 Read Characteristic Descriptor
             * <p>
             * BT Core Spec v5.2: Vol 3, Part G GATT: 4.12.2 Read Long Characteristic Descriptor
             * </p>
             * <p>
             * If expectedLength = 0, then only one ATT_READ_REQ/RSP will be used.
             * </p>
             * <p>
             * If expectedLength < 0, then long values using multiple ATT_READ_BLOB_REQ/RSP will be used until
             * the response returns zero. This is the default parameter.
             * </p>
             * <p>
             * If expectedLength > 0, then long values using multiple ATT_READ_BLOB_REQ/RSP will be used
             * if required until the response returns zero.
             * </p>
             * <p>
             * Convenience delegation call to BTGattHandler via BTDevice
             * If the BTDevice's BTGattHandler is null, i.e. not connected, false is returned.
             * </p>
             */
            bool readValue(int expectedLength=-1) noexcept;

            /**
             * BT Core Spec v5.2: Vol 3, Part G GATT: 4.12.3 Write Characteristic Descriptors
             * <p>
             * BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3 Characteristic Descriptor
             * </p>
             * <p>
             * BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.3 Client Characteristic Configuration
             * </p>
             * <p>
             * Convenience delegation call to BTGattHandler via BTDevice
             * If the BTDevice's BTGattHandler is null, i.e. not connected, false is returned.
             * </p>
             */
            bool writeValue() noexcept;
    };
    typedef std::shared_ptr<BTGattDesc> BTGattDescRef;

    inline bool operator==(const BTGattDesc& lhs, const BTGattDesc& rhs) noexcept
    { return lhs.handle == rhs.handle; /** unique attribute handles */ }

    inline bool operator!=(const BTGattDesc& lhs, const BTGattDesc& rhs) noexcept
    { return !(lhs == rhs); }

    /**@}*/

} // namespace direct_bt

#endif /* BT_GATT_DESCRIPTOR_HPP_ */
