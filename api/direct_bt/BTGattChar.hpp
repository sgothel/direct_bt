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

#ifndef BT_GATT_CHARACTERISTIC_HPP_
#define BT_GATT_CHARACTERISTIC_HPP_

#include <cstring>
#include <string>
#include <memory>
#include <cstdint>

#include <mutex>
#include <atomic>

#include <jau/java_uplink.hpp>
#include <jau/octets.hpp>
#include <jau/uuid.hpp>

#include "BTTypes0.hpp"
#include "ATTPDUTypes.hpp"

#include "BTTypes1.hpp"

#include "BTGattDesc.hpp"
#include "jau/int_types.hpp"


/**
 * - - - - - - - - - - - - - - -
 *
 * Module BTGattChar:
 *
 * - BT Core Spec v5.2: Vol 3, Part G Generic Attribute Protocol (GATT)
 * - BT Core Spec v5.2: Vol 3, Part G GATT: 2.6 GATT Profile Hierarchy
 */
namespace direct_bt {

    class BTGattHandler; // forward
    typedef std::shared_ptr<BTGattHandler> BTGattHandlerRef;

    class BTDevice; // forward
    typedef std::shared_ptr<BTDevice> BTDeviceRef;

    class BTGattService; // forward
    typedef std::shared_ptr<BTGattService> BTGattServiceRef;

    class BTGattCharListener; // forward
    typedef std::shared_ptr<BTGattCharListener> BTGattCharListenerRef;

    /** @defgroup DBTUserClientAPI Direct-BT Central-Client User Level API
     *  User level Direct-BT API types and functionality addressing the central-client ::GATTRole::Client perspective,
     *  [see Direct-BT Overview](namespacedirect__bt.html#details).
     *
     *  @{
     */

    /**
     * Representing a Gatt Characteristic object from the ::GATTRole::Client perspective.
     *
     * A list of shared BTGattChar instances is available from BTGattService
     * via BTGattService::characteristicList.
     *
     * See [Direct-BT Overview](namespacedirect__bt.html#details).
     *
     * BT Core Spec v5.2: Vol 3, Part G GATT: 3.3 Characteristic Definition
     *
     * BT Core Spec v5.2: Vol 3, Part G GATT: 4.6.1 Discover All Characteristics of a Service
     *
     * The handle represents a service's characteristics-declaration
     * and the value the Characteristics Property, Characteristics Value Handle _and_ Characteristics UUID.
     */
    class BTGattChar : public BTObject {
        private:
            /** Characteristics's service weak back-reference */
            std::weak_ptr<BTGattService> wbr_service;
            bool enabledNotifyState = false;
            bool enabledIndicateState = false;

            std::string toShortString() const noexcept;

        public:
            /** BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.1.1 Characteristic Properties */
            enum PropertyBitVal : uint8_t {
                NONE            = 0,
                Broadcast       = (1 << 0),
                Read            = (1 << 1),
                WriteNoAck      = (1 << 2),
                WriteWithAck    = (1 << 3),
                Notify          = (1 << 4),
                Indicate        = (1 << 5),
                AuthSignedWrite = (1 << 6),
                ExtProps        = (1 << 7)
            };

            typedef jau::nsize_t size_type;
            typedef jau::snsize_t ssize_type;

            /**
             * Characteristic Handle of this instance.
             * <p>
             * Attribute handles are unique for each device (server) (BT Core Spec v5.2: Vol 3, Part F Protocol..: 3.2.2 Attribute Handle).
             * </p>
             */
            const uint16_t handle;

            /* Characteristics Property */
            const PropertyBitVal properties;

            /**
             * Characteristics Value Handle.
             * <p>
             * Attribute handles are unique for each device (server) (BT Core Spec v5.2: Vol 3, Part F Protocol..: 3.2.2 Attribute Handle).
             * </p>
             */
            const uint16_t value_handle;

            /* Characteristics Value Type UUID */
            std::unique_ptr<const jau::uuid_t> value_type;

            /** List of Characteristic Descriptions as shared reference */
            jau::darray<BTGattDescRef> descriptorList;

            /* Optional Client Characteristic Configuration index within descriptorList */
            ssize_type clientCharConfigIndex = -1;

            /* Optional Characteristic User Description index within descriptorList */
            ssize_type userDescriptionIndex = -1;

            BTGattChar(const BTGattServiceRef & service_, const uint16_t handle_,
                       const PropertyBitVal properties_, const uint16_t value_handle_, std::unique_ptr<const jau::uuid_t> && value_type_) noexcept
            : wbr_service(service_), handle(handle_),
              properties(properties_), value_handle(value_handle_), value_type(std::move(value_type_)) {}

            std::string get_java_class() const noexcept override {
                return java_class();
            }
            static std::string java_class() noexcept {
                return std::string(JAVA_DBT_PACKAGE "DBTGattChar");
            }

            BTGattServiceRef getServiceUnchecked() const noexcept { return wbr_service.lock(); }
            BTGattHandlerRef getGattHandlerUnchecked() const noexcept;
            BTDeviceRef getDeviceUnchecked() const noexcept;

            bool hasProperties(const PropertyBitVal v) const noexcept { return v == ( properties & v ); }

            /**
             * Find a BTGattDesc by its desc_uuid.
             *
             * @parameter desc_uuid the UUID of the desired BTGattDesc
             * @return The matching descriptor or null if not found
             */
            std::shared_ptr<BTGattDesc> findGattDesc(const jau::uuid_t& desc_uuid) noexcept;

            std::string toString() const noexcept override;

            void clearDescriptors() noexcept {
                descriptorList.clear();
                clientCharConfigIndex = -1;
                userDescriptionIndex = -1;
            }

            /**
             * Return the Client Characteristic Configuration BTGattDescRef if available or nullptr.
             *
             * The BTGattDesc::Type::CLIENT_CHARACTERISTIC_CONFIGURATION has been indexed while
             * retrieving the GATT database from the server.
             */
            BTGattDescRef getClientCharConfig() const noexcept {
                if( 0 > clientCharConfigIndex ) {
                    return nullptr;
                }
                return descriptorList.at(static_cast<size_t>(clientCharConfigIndex)); // abort if out of bounds
            }

            /**
             * Return the User Description BTGattDescRef if available or nullptr.
             *
             * The BTGattDesc::Type::CHARACTERISTIC_USER_DESCRIPTION has been indexed while
             * retrieving the GATT database from the server.
             */
            BTGattDescRef getUserDescription() const noexcept {
                if( 0 > userDescriptionIndex ) {
                    return nullptr;
                }
                return descriptorList.at(static_cast<size_t>(userDescriptionIndex)); // abort if out of bounds
            }

            /**
             * BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.3 Client Characteristic Configuration
             *
             * Method enables notification and/or indication for this characteristic at BLE level.
             *
             * Implementation masks this Characteristic properties PropertyBitVal::Notify and PropertyBitVal::Indicate
             * with the respective user request parameters, hence removes unsupported requests.
             *
             * Notification and/or indication configuration is only performed per characteristic if changed.
             *
             * It is recommended to utilize notification over indication, as its link-layer handshake
             * and higher potential bandwidth may deliver material higher performance.
             *
             * @param enableNotification
             * @param enableIndication
             * @param enabledState array of size 2, holding the resulting enabled state for notification and indication.
             * @return false if this characteristic has no PropertyBitVal::Notify or PropertyBitVal::Indication present,
             * or there is no BTGattDesc of type ClientCharacteristicConfiguration, or if the operation has failed.
             * Otherwise returns true.
             *
             * @see enableNotificationOrIndication()
             * @see disableIndicationNotification()
             */
            bool configNotificationIndication(const bool enableNotification, const bool enableIndication, bool enabledState[2]) noexcept;

            /**
             * BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.3 Client Characteristic Configuration
             *
             * Method will attempt to enable notification on the BLE level, if available,
             * otherwise indication if available.
             *
             * Notification and/or indication configuration is only performed per characteristic if changed.
             *
             * It is recommended to utilize notification over indication, as its link-layer handshake
             * and higher potential bandwidth may deliver material higher performance.
             *
             * @param enabledState array of size 2, holding the resulting enabled state for notification and indication.
             * @return false if this characteristic has no PropertyBitVal::Notify or PropertyBitVal::Indication present,
             * or there is no BTGattDesc of type ClientCharacteristicConfiguration, or if the operation has failed.
             * Otherwise returns true.
             *
             * @see configNotificationIndication()
             * @see disableIndicationNotification()
             */
            bool enableNotificationOrIndication(bool enabledState[2]) noexcept;

            /**
             * BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.3 Client Characteristic Configuration
             *
             * Method will attempt to disable notification and indication on the BLE level.
             *
             * Notification and/or indication configuration is only performed per characteristic if changed.
             *
             * @return false if this characteristic has no PropertyBitVal::Notify or PropertyBitVal::Indication present,
             * or there is no BTGattDesc of type ClientCharacteristicConfiguration, or if the operation has failed.
             * Otherwise returns true.
             *
             * @see configNotificationIndication()
             * @see enableNotificationOrIndication()
             * @since 2.4.0
             */
            bool disableIndicationNotification() noexcept;

            /**
             * Add the given BTGattCharListener to the listener list if not already present.
             *
             * Occurring notifications and indications for this characteristic,
             * if enabled via configNotificationIndication(bool, bool, bool[]) or enableNotificationOrIndication(bool[]),
             * will call the respective BTGattCharListener callback method.
             *
             * Returns true if the given listener is not element of the list and has been newly added,
             * otherwise false.
             *
             * Convenience delegation call to BTGattHandler via BTDevice
             *
             * @see BTGattChar::disableIndicationNotification()
             * @see BTGattChar::enableNotificationOrIndication()
             * @see BTGattChar::configNotificationIndication()
             * @see BTGattChar::addCharListener()
             * @see BTGattChar::removeCharListener()
             * @see BTGattChar::removeAllAssociatedCharListener()
             */
            bool addCharListener(const BTGattCharListenerRef& l) noexcept;

            /**
             * Add the given BTGattCharListener to the listener list if not already present
             * and if enabling the notification <i>or</i> indication for this characteristic at BLE level was successful.<br>
             * Notification and/or indication configuration is only performed per characteristic if changed.
             *
             * Implementation will enable notification if available,
             * otherwise indication will be enabled if available. <br>
             * Implementation uses enableNotificationOrIndication(bool[]) to enable either.
             *
             * Occurring notifications and indications for this characteristic
             * will call the respective BTGattCharListener callback method.
             *
             * Returns true if enabling the notification and/or indication was successful
             * and if the given listener is not element of the list and has been newly added,
             * otherwise false.
             *
             * @param enabledState array of size 2, holding the resulting enabled state for notification and indication
             * using enableNotificationOrIndication(bool[])
             *
             * @see BTGattChar::disableIndicationNotification()
             * @see BTGattChar::enableNotificationOrIndication()
             * @see BTGattChar::configNotificationIndication()
             * @see BTGattChar::addCharListener()
             * @see BTGattChar::removeCharListener()
             * @see BTGattChar::removeAllAssociatedCharListener()
             */
            bool addCharListener(const BTGattCharListenerRef& l, bool enabledState[2]) noexcept;

            /**
             * Remove the given associated BTGattCharListener from the listener list if present.
             *
             * To disables the notification and/or indication for this characteristic at BLE level
             * use disableIndicationNotification() when desired.
             *
             * @param l
             * @return
             *
             * @see BTGattChar::disableIndicationNotification()
             * @see BTGattChar::enableNotificationOrIndication()
             * @see BTGattChar::configNotificationIndication()
             * @see BTGattChar::addCharListener()
             * @see BTGattChar::removeCharListener()
             * @see BTGattChar::removeAllAssociatedCharListener()
             * @since 2.4.0
             */
            bool removeCharListener(const BTGattCharListenerRef& l) noexcept;

            /**
             * Removes all associated BTGattCharListener and and {@link BTGattCharListener} from the listener list.
             *
             * Also disables the notification and/or indication for this characteristic at BLE level
             * if `disableIndicationNotification == true`.
             *
             * Returns the number of removed event listener.
             *
             * If the BTDevice's BTGattHandler is null, i.e. not connected, `zero` is being returned.
             *
             * @param shallDisableIndicationNotification if true, disables the notification and/or indication for this characteristic
             * using {@link #disableIndicationNotification()
             * @return
             * @see BTGattChar::disableIndicationNotification()
             * @see BTGattChar::configNotificationIndication()
             * @see BTGattChar::addCharListener()
             * @see BTGattChar::removeCharListener()
             * @see BTGattChar::removeAllAssociatedCharListener()
             */
            size_type removeAllAssociatedCharListener(bool shallDisableIndicationNotification) noexcept;

            /**
             * BT Core Spec v5.2: Vol 3, Part G GATT: 4.8.1 Read Characteristic Value
             * <p>
             * BT Core Spec v5.2: Vol 3, Part G GATT: 4.8.3 Read Long Characteristic Value
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
             * <p>
             * </p>
             * If the BTDevice's BTGattHandler is null, i.e. not connected, false is returned.
             * </p>
             */
            bool readValue(jau::POctets & res, int expectedLength=-1) noexcept;

            /**
             * BT Core Spec v5.2: Vol 3, Part G GATT: 4.9.3 Write Characteristic Value
             * <p>
             * Convenience delegation call to BTGattHandler via BTDevice
             * <p>
             * </p>
             * If the BTDevice's BTGattHandler is null, i.e. not connected, false is returned.
             * </p>
             */
            bool writeValue(const jau::TROOctets & value) noexcept;

            /**
             * BT Core Spec v5.2: Vol 3, Part G GATT: 4.9.1 Write Characteristic Value Without Response
             * <p>
             * Convenience delegation call to BTGattHandler via BTDevice
             * <p>
             * </p>
             * If the BTDevice's BTGattHandler is null, i.e. not connected, false is returned.
             * </p>
             */
            bool writeValueNoResp(const jau::TROOctets & value) noexcept;
    };
    typedef std::shared_ptr<BTGattChar> BTGattCharRef;

    inline bool operator==(const BTGattChar& lhs, const BTGattChar& rhs) noexcept
    { return lhs.handle == rhs.handle; /** unique attribute handles */ }

    inline bool operator!=(const BTGattChar& lhs, const BTGattChar& rhs) noexcept
    { return !(lhs == rhs); }

    constexpr uint8_t number(const BTGattChar::PropertyBitVal rhs) noexcept {
        return static_cast<uint8_t>(rhs);
    }
    constexpr BTGattChar::PropertyBitVal operator |(const BTGattChar::PropertyBitVal lhs, const BTGattChar::PropertyBitVal rhs) noexcept {
        return static_cast<BTGattChar::PropertyBitVal> ( number(lhs) | number(rhs) );
    }
    constexpr BTGattChar::PropertyBitVal operator &(const BTGattChar::PropertyBitVal lhs, const BTGattChar::PropertyBitVal rhs) noexcept {
        return static_cast<BTGattChar::PropertyBitVal> ( number(lhs) & number(rhs) );
    }
    constexpr bool operator ==(const BTGattChar::PropertyBitVal lhs, const BTGattChar::PropertyBitVal rhs) noexcept {
        return number(lhs) == number(rhs);
    }
    constexpr bool operator !=(const BTGattChar::PropertyBitVal lhs, const BTGattChar::PropertyBitVal rhs) noexcept {
        return !( lhs == rhs );
    }
    std::string to_string(const BTGattChar::PropertyBitVal mask) noexcept;

    /**
     * {@link BTGattChar} event listener for notification and indication events.
     * <p>
     * A listener instance may be attached to a BTGattChar instance via
     * {@link BTGattChar::addCharListener(BTGattCharListenerRef)} to listen to its events.
     * </p>
     * <p>
     * A listener instance may be attached to a BTGattHandler via
     * {@link BTGattHandler::addCharListener(BTGattCharListenerRef)}
     * to listen to all events of the device or the matching filtered events.
     * </p>
     * <p>
     * The listener manager maintains a unique set of listener instances without duplicates.
     * </p>
     */
    class BTGattCharListener : public jau::jni::JavaUplink {
        public:
            /**
             * Returns a unique string denominating the type of this instance.
             *
             * Simple access and provision of a typename string representation
             * at compile time like RTTI via jau::type_name_cue.
             */
            virtual const char * type_name() const noexcept;

            /**
             * Called from native BLE stack, initiated by a received notification associated
             * with the given {@link BTGattChar}.
             * @param charDecl {@link BTGattChar} related to this notification
             * @param charValue the notification value
             * @param timestamp monotonic timestamp at reception, see jau::getCurrentMilliseconds()
             */
            virtual void notificationReceived(BTGattCharRef charDecl,
                                              const jau::TROOctets& charValue, const uint64_t timestamp) = 0;

            /**
             * Called from native BLE stack, initiated by a received indication associated
             * with the given {@link BTGattChar}.
             * @param charDecl {@link BTGattChar} related to this indication
             * @param charValue the indication value
             * @param timestamp monotonic timestamp at reception, see jau::getCurrentMilliseconds()
             * @param confirmationSent if true, the native stack has sent the confirmation, otherwise user is required to do so.
             */
            virtual void indicationReceived(BTGattCharRef charDecl,
                                            const jau::TROOctets& charValue, const uint64_t timestamp,
                                            const bool confirmationSent) = 0;

            ~BTGattCharListener() noexcept override = default;

            /** Return a simple description about this instance. */
            std::string toString() const noexcept override {
                return std::string(type_name())+"["+jau::to_string(this)+"]";
            }

            std::string get_java_class() const noexcept override {
                return java_class();
            }
            static std::string java_class() noexcept {
                return std::string(JAVA_MAIN_PACKAGE "BTGattCharListener");
            }

            /**
             * Default comparison operator, merely testing for same memory reference.
             * <p>
             * Specializations may override.
             * </p>
             */
            virtual bool operator==(const BTGattCharListener& rhs) const noexcept
            { return this == &rhs; }

            bool operator!=(const BTGattCharListener& rhs) const noexcept
            { return !(*this == rhs); }
    };
    typedef std::shared_ptr<BTGattCharListener> BTGattCharListenerRef;

    /**@}*/

} // namespace direct_bt

#endif /* BT_GATT_CHARACTERISTIC_HPP_ */
