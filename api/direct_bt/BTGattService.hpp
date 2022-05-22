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

#ifndef BT_GATT_SERVICE_HPP_
#define BT_GATT_SERVICE_HPP_

#include <cstring>
#include <string>
#include <memory>
#include <cstdint>

#include <mutex>
#include <atomic>

#include <jau/java_uplink.hpp>
#include <jau/darray.hpp>
#include <jau/octets.hpp>
#include <jau/uuid.hpp>

#include "BTTypes0.hpp"
#include "ATTPDUTypes.hpp"

#include "BTTypes1.hpp"

#include "BTGattChar.hpp"

/**
 * - - - - - - - - - - - - - - -
 *
 * Module GATTService:
 *
 * - BT Core Spec v5.2: Vol 3, Part G Generic Attribute Protocol (GATT)
 * - BT Core Spec v5.2: Vol 3, Part G GATT: 2.6 GATT Profile Hierarchy
 */
namespace direct_bt {

    class BTGattHandler; // forward
    typedef std::shared_ptr<BTGattHandler> BTGattHandlerRef;

    class BTDevice; // forward
    typedef std::shared_ptr<BTDevice> BTDeviceRef;

    /** \addtogroup DBTUserClientAPI
     *
     *  @{
     */

    /**
     * Representing a Gatt Service object from the ::GATTRole::Client perspective.
     *
     * A list of shared BTGattService instances can be retrieved from BTDevice
     * after successful connection and optional pairing via BTDevice::getGattServices().
     *
     * See [Direct-BT Overview](namespacedirect__bt.html#details).
     *
     * BT Core Spec v5.2: Vol 3, Part G GATT: 3.1 Service Definition
     *
     * Includes a complete [Primary] Service Declaration
     * including its list of Characteristic Declarations,
     * which also may include its client config if available.
     */
    class BTGattService : public BTObject {
        private:
            /** Service's GATTHandler weak back-reference */
            std::weak_ptr<BTGattHandler> wbr_handler;

            std::string toShortString() const noexcept;

        public:
            const bool primary;

            /**
             * Service start handle
             * <p>
             * Attribute handles are unique for each device (server) (BT Core Spec v5.2: Vol 3, Part F Protocol..: 3.2.2 Attribute Handle).
             * </p>
             */
            const uint16_t handle;

            /**
             * Service end handle, inclusive.
             * <p>
             * Attribute handles are unique for each device (server) (BT Core Spec v5.2: Vol 3, Part F Protocol..: 3.2.2 Attribute Handle).
             * </p>
             */
            const uint16_t end_handle;

            /** Service type UUID */
            std::unique_ptr<const jau::uuid_t> type;

            /** List of Characteristic Declarations as shared reference */
            jau::darray<BTGattCharRef> characteristicList;

            BTGattService(const std::shared_ptr<BTGattHandler> &handler_, const bool isPrimary_,
                        const uint16_t startHandle_, const uint16_t endHandle_, std::unique_ptr<const jau::uuid_t> && type_) noexcept
            : wbr_handler(handler_), primary(isPrimary_), handle(startHandle_), end_handle(endHandle_), type(std::move(type_)), characteristicList() {
                characteristicList.reserve(10);
            }

            std::string get_java_class() const noexcept override {
                return java_class();
            }
            static std::string java_class() noexcept {
                return std::string(JAVA_DBT_PACKAGE "DBTGattService");
            }

            BTGattHandlerRef getGattHandlerUnchecked() const noexcept { return wbr_handler.lock(); }
            BTGattHandlerRef getGattHandlerChecked() const;

            BTDeviceRef getDeviceUnchecked() const noexcept;
            BTDeviceRef getDeviceChecked() const;

            /**
             * Find a BTGattChar by its char_uuid.
             *
             * @parameter char_uuid the jau::uuid_t of the desired BTGattChar, within this BTGattService.
             * @return The matching characteristic or null if not found
             */
            BTGattCharRef findGattChar(const jau::uuid_t& char_uuid) noexcept;

            /**
             * Find a BTGattChar by itself, i.e. mapping BTGattChar instance to BTGattCharRef.
             *
             * @parameter characteristic the desired BTGattChar, within this BTGattService.
             * @return The matching characteristic or null if not found
             */
            BTGattCharRef findGattChar(const BTGattChar& characteristic) noexcept;

            std::string toString() const noexcept override;
    };
    typedef std::shared_ptr<BTGattService> BTGattServiceRef;

    inline bool operator==(const BTGattService& lhs, const BTGattService& rhs) noexcept
    { return lhs.handle == rhs.handle && lhs.end_handle == rhs.end_handle; /** unique attribute handles */ }

    inline bool operator!=(const BTGattService& lhs, const BTGattService& rhs) noexcept
    { return !(lhs == rhs); }

    /**@}*/

} // namespace direct_bt

#endif /* BT_GATT_SERVICE_HPP_ */
