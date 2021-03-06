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

#include "UUID.hpp"
#include "BTTypes0.hpp"
#include "OctetTypes.hpp"
#include "ATTPDUTypes.hpp"

#include "BTTypes1.hpp"

#include "BTGattDesc.hpp"


/**
 * - - - - - - - - - - - - - - -
 *
 * Module BTGattChar:
 *
 * - BT Core Spec v5.2: Vol 3, Part G Generic Attribute Protocol (GATT)
 * - BT Core Spec v5.2: Vol 3, Part G GATT: 2.6 GATT Profile Hierarchy
 */
namespace direct_bt {

    class BTGattCharListener; // forward

    class BTGattHandler; // forward
    class BTGattService; // forward
    typedef std::shared_ptr<BTGattService> BTGattServiceRef;

    /**
     * <p>
     * BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.1 Characteristic Declaration Attribute Value
     * </p>
     * handle -> CDAV value
     * <p>
     * BT Core Spec v5.2: Vol 3, Part G GATT: 4.6.1 Discover All Characteristics of a Service
     *
     * Here the handle is a service's characteristics-declaration
     * and the value the Characteristics Property, Characteristics Value Handle _and_ Characteristics UUID.
     * </p>
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
                Broadcast = 0x01,
                Read = 0x02,
                WriteNoAck = 0x04,
                WriteWithAck = 0x08,
                Notify = 0x10,
                Indicate = 0x20,
                AuthSignedWrite = 0x40,
                ExtProps = 0x80
            };
            /**
             * Returns string values as defined in <https://git.kernel.org/pub/scm/bluetooth/bluez.git/tree/doc/gatt-api.txt>
             * <pre>
             * org.bluez.GattCharacteristic1 :: array{string} Flags [read-only]
             * </pre>
             */
            static std::string getPropertiesString(const PropertyBitVal properties) noexcept;
            static jau::darray<std::unique_ptr<std::string>> getPropertiesStringList(const PropertyBitVal properties) noexcept;

            /**
             * Characteristics's Service Handle - key to service's handle range, retrieved from Characteristics data.
             * <p>
             * Attribute handles are unique for each device (server) (BT Core Spec v5.2: Vol 3, Part F Protocol..: 3.2.2 Attribute Handle).
             * </p>
             */
            const uint16_t service_handle;

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
            std::unique_ptr<const uuid_t> value_type;

            /** List of Characteristic Descriptions as shared reference */
            jau::darray<BTGattDescRef> descriptorList;

            /* Optional Client Characteristic Configuration index within descriptorList */
            int clientCharConfigIndex = -1;

            BTGattChar(const BTGattServiceRef & service_, const uint16_t service_handle_, const uint16_t handle_,
                               const PropertyBitVal properties_, const uint16_t value_handle_, std::unique_ptr<const uuid_t> && value_type_) noexcept
            : wbr_service(service_), service_handle(service_handle_), handle(handle_),
              properties(properties_), value_handle(value_handle_), value_type(std::move(value_type_)) {}

            std::string get_java_class() const noexcept override {
                return java_class();
            }
            static std::string java_class() noexcept {
                return std::string(JAVA_DBT_PACKAGE "DBTGattChar");
            }

            std::shared_ptr<BTGattService> getServiceUnchecked() const noexcept { return wbr_service.lock(); }
            std::shared_ptr<BTGattService> getServiceChecked() const;
            std::shared_ptr<BTGattHandler> getGattHandlerUnchecked() const noexcept;
            std::shared_ptr<BTGattHandler> getGattHandlerChecked() const;
            std::shared_ptr<BTDevice> getDeviceUnchecked() const noexcept;
            std::shared_ptr<BTDevice> getDeviceChecked() const;

            bool hasProperties(const PropertyBitVal v) const noexcept { return v == ( properties & v ); }

            std::string toString() const noexcept override;

            void clearDescriptors() noexcept {
                descriptorList.clear();
                clientCharConfigIndex = -1;
            }

            BTGattDescRef getClientCharConfig() noexcept {
                if( 0 > clientCharConfigIndex ) {
                    return nullptr;
                }
                return descriptorList.at(static_cast<size_t>(clientCharConfigIndex)); // abort if out of bounds
            }

            /**
             * BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.3 Client Characteristic Configuration
             * <p>
             * Method enables notification and/or indication for this characteristic at BLE level.
             * </p>
             * <p>
             * Implementation masks this Characteristic properties PropertyBitVal::Notify and PropertyBitVal::Indicate
             * with the respective user request parameters, hence removes unsupported requests.
             * </p>
             * <p>
             * Notification and/or indication configuration is only performed per characteristic if changed.
             * </p>
             * <p>
             * It is recommended to utilize notification over indication, as its link-layer handshake
             * and higher potential bandwidth may deliver material higher performance.
             * </p>
             * @param enableNotification
             * @param enableIndication
             * @param enabledState array of size 2, holding the resulting enabled state for notification and indication.
             * @return false if this characteristic has no PropertyBitVal::Notify or PropertyBitVal::Indication present,
             * or there is no BTGattDesc of type ClientCharacteristicConfiguration, or if the operation has failed.
             * Otherwise returns true.
             * @throws IllegalStateException if notification or indication is set to be enabled
             * and the {@link BTDevice's}'s {@link BTGattHandler} is null, i.e. not connected
             */
            bool configNotificationIndication(const bool enableNotification, const bool enableIndication, bool enabledState[2]);

            /**
             * BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.3 Client Characteristic Configuration
             * <p>
             * Method will attempt to enable notification on the BLE level, if available,
             * otherwise indication if available.
             * </p>
             * <p>
             * Notification and/or indication configuration is only performed per characteristic if changed.
             * </p>
             * <p>
             * It is recommended to utilize notification over indication, as its link-layer handshake
             * and higher potential bandwidth may deliver material higher performance.
             * </p>
             * @param enabledState array of size 2, holding the resulting enabled state for notification and indication.
             * @return false if this characteristic has no PropertyBitVal::Notify or PropertyBitVal::Indication present,
             * or there is no BTGattDesc of type ClientCharacteristicConfiguration, or if the operation has failed.
             * Otherwise returns true.
             * @throws IllegalStateException if notification or indication is set to be enabled
             * and the {@link BTDevice's}'s {@link BTGattHandler} is null, i.e. not connected
             */
            bool enableNotificationOrIndication(bool enabledState[2]);

            /**
             * Add the given BTGattCharListener to the listener list if not already present.
             * <p>
             * Occurring notifications and indications, if enabled via configNotificationIndication(bool, bool, bool[])
             * or enableNotificationOrIndication(bool[]),
             * will call the respective BTGattCharListener callback method.
             * </p>
             * <p>
             * Returns true if the given listener is not element of the list and has been newly added,
             * otherwise false.
             * </p>
             * <p>
             * Convenience delegation call to BTGattHandler via BTDevice
             * </p>
             * <p>
             * To restrict the listener to listen only to this BTGattChar instance,
             * user has to implement BTGattCharListener::match(BTGattCharRef) accordingly.
             * <br>
             * For this purpose, use may derive from AssociatedBTGattCharListener,
             * which provides these simple matching filter facilities.
             * </p>
             * @throws IllegalStateException if the {@link BTDevice's}'s {@link BTGattHandler} is null, i.e. not connected
             */
            bool addCharListener(std::shared_ptr<BTGattCharListener> l);

            /**
             * Add the given BTGattCharListener to the listener list if not already present
             * and if enabling the notification <i>or</i> indication for this characteristic at BLE level was successful.<br>
             * Notification and/or indication configuration is only performed per characteristic if changed.
             * <p>
             * Implementation will attempt to enable notification only, if available,
             * otherwise indication if available. <br>
             * Implementation uses enableNotificationOrIndication(bool[]) to enable either.
             * </p>
             * <p>
             * Occurring notifications and indications will call the respective BTGattCharListener
             * callback method.
             * </p>
             * <p>
             * Returns true if enabling the notification and/or indication was successful
             * and if the given listener is not element of the list and has been newly added,
             * otherwise false.
             * </p>
             * <p>
             * To restrict the listener to listen only to this BTGattChar instance,
             * user has to implement BTGattCharListener::match(BTGattCharRef) accordingly.
             * <br>
             * For this purpose, use may derive from AssociatedBTGattCharListener,
             * which provides these simple matching filter facilities.
             * </p>
             * @param enabledState array of size 2, holding the resulting enabled state for notification and indication
             * using enableNotificationOrIndication(bool[])
             * @throws IllegalStateException if the {@link BTDevice's}'s {@link BTGattHandler} is null, i.e. not connected
             */
            bool addCharListener(std::shared_ptr<BTGattCharListener> l, bool enabledState[2]);

            /**
             * Disables the notification and/or indication for this characteristic at BLE level
             * if `disableIndicationNotification == true`
             * and removes the given {@link BTGattCharListener} from the listener list.
             * <p>
             * Returns true if the given listener is an element of the list and has been removed,
             * otherwise false.
             * </p>
             * <p>
             * Convenience delegation call to BTGattHandler via BTDevice
             * performing addCharListener(..)
             * and {@link #configNotificationIndication(bool, bool, bool[]) if `disableIndicationNotification == true`
             * </p>
             * <p>
             * If the BTDevice's BTGattHandler is null, i.e. not connected, `false` is being returned.
             * </p>
             * @param l
             * @param disableIndicationNotification if true, disables the notification and/or indication for this characteristic
             * using {@link #configNotificationIndication(bool, bool, bool[])
             * @return
             */
            bool removeCharListener(std::shared_ptr<BTGattCharListener> l, bool disableIndicationNotification);

            /**
             * Disables the notification and/or indication for this characteristic at BLE level
             * if `disableIndicationNotification == true`
             * and removes all {@link BTGattCharListener} from the listener list.
             * <p>
             * Returns the number of removed event listener.
             * </p>
             * <p>
             * Convenience delegation call to BTGattHandler via BTDevice
             * performing addCharListener(..)
             * and configNotificationIndication(..) if `disableIndicationNotification == true`.
             * </p>
             * <p>
             * If the BTDevice's BTGattHandler is null, i.e. not connected, `zero` is being returned.
             * </p>
             * @param disableIndicationNotification if true, disables the notification and/or indication for this characteristic
             * using {@link #configNotificationIndication(bool, bool, bool[])
             * @return
             */
            int removeAllCharListener(bool disableIndicationNotification);

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
             * If the BTDevice's BTGattHandler is null, i.e. not connected, an IllegalStateException is thrown.
             * </p>
             */
            bool readValue(POctets & res, int expectedLength=-1);

            /**
             * BT Core Spec v5.2: Vol 3, Part G GATT: 4.9.3 Write Characteristic Value
             * <p>
             * Convenience delegation call to BTGattHandler via BTDevice
             * <p>
             * </p>
             * If the BTDevice's BTGattHandler is null, i.e. not connected, an IllegalStateException is thrown.
             * </p>
             */
            bool writeValue(const TROOctets & value);

            /**
             * BT Core Spec v5.2: Vol 3, Part G GATT: 4.9.1 Write Characteristic Value Without Response
             * <p>
             * Convenience delegation call to BTGattHandler via BTDevice
             * <p>
             * </p>
             * If the BTDevice's BTGattHandler is null, i.e. not connected, an IllegalStateException is thrown.
             * </p>
             */
            bool writeValueNoResp(const TROOctets & value);
    };
    typedef std::shared_ptr<BTGattChar> BTGattCharRef;

    inline bool operator==(const BTGattChar& lhs, const BTGattChar& rhs) noexcept
    { return lhs.handle == rhs.handle; /** unique attribute handles */ }

    inline bool operator!=(const BTGattChar& lhs, const BTGattChar& rhs) noexcept
    { return !(lhs == rhs); }

    /**
     * {@link BTGattChar} event listener for notification and indication events.
     * <p>
     * A listener instance may be attached to a {@link BTGattChar} via
     * {@link BTGattChar::addCharListener(std::shared_ptr<BTGattCharListener>)} to listen to events,
     * see method's API doc for {@link BTGattChar} filtering.
     * </p>
     * <p>
     * User may utilize {@link AssociatedBTGattCharListener} to listen to only one {@link BTGattChar}.
     * </p>
     * <p>
     * A listener instance may be attached to a {@link BTGattHandler} via
     * {@link BTGattHandler::addCharListener(std::shared_ptr<BTGattCharListener>)}
     * to listen to all events of the device or the matching filtered events.
     * </p>
     * <p>
     * The listener receiver maintains a unique set of listener instances without duplicates.
     * </p>
     */
    class BTGattCharListener {
        public:
            /**
             * Custom filter for all event methods,
             * which will not be called if this method returns false.
             * <p>
             * User may override this method to test whether the methods shall be called
             * for the given BTGattChar.
             * </p>
             * <p>
             * Defaults to true;
             * </p>
             */
            virtual bool match(const BTGattChar & characteristic) noexcept {
                (void)characteristic;
                return true;
            }

            /**
             * Called from native BLE stack, initiated by a received notification associated
             * with the given {@link BTGattChar}.
             * @param charDecl {@link BTGattChar} related to this notification
             * @param charValue the notification value
             * @param timestamp the indication monotonic timestamp, see getCurrentMilliseconds()
             */
            virtual void notificationReceived(BTGattCharRef charDecl,
                                              const TROOctets& charValue, const uint64_t timestamp) = 0;

            /**
             * Called from native BLE stack, initiated by a received indication associated
             * with the given {@link BTGattChar}.
             * @param charDecl {@link BTGattChar} related to this indication
             * @param charValue the indication value
             * @param timestamp the indication monotonic timestamp, see {@link BluetoothUtils#getCurrentMilliseconds()}
             * @param confirmationSent if true, the native stack has sent the confirmation, otherwise user is required to do so.
             */
            virtual void indicationReceived(BTGattCharRef charDecl,
                                            const TROOctets& charValue, const uint64_t timestamp,
                                            const bool confirmationSent) = 0;

            virtual ~BTGattCharListener() noexcept {}

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

    class AssociatedBTGattCharListener : public BTGattCharListener{
        private:
            const BTGattChar * associatedChar;

        public:
            /**
             * Passing the associated BTGattChar to filter out non matching events.
             */
            AssociatedBTGattCharListener(const BTGattChar * characteristicMatch) noexcept
            : associatedChar(characteristicMatch) { }

            bool match(const BTGattChar & characteristic) noexcept override {
                if( nullptr == associatedChar ) {
                    return true;
                }
                return *associatedChar == characteristic;
            }
    };

} // namespace direct_bt

#endif /* BT_GATT_CHARACTERISTIC_HPP_ */
