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
    class BTGattService; // forward
    typedef std::shared_ptr<BTGattService> BTGattServiceRef;

    /**
     * Representing a Gatt Characteristic object from the ::GATTRole::Client perspective.
     *
     * BT Core Spec v5.2: Vol 3, Part G GATT: 3.3 Characteristic Definition
     *
     * handle -> CDAV value
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
            /**
             * Returns string values as defined in <https://git.kernel.org/pub/scm/bluetooth/bluez.git/tree/doc/gatt-api.txt>
             * <pre>
             * org.bluez.GattCharacteristic1 :: array{string} Flags [read-only]
             * </pre>
             */
            static std::string getPropertiesString(const PropertyBitVal properties) noexcept;
            static jau::darray<std::unique_ptr<std::string>> getPropertiesStringList(const PropertyBitVal properties) noexcept;

            /**
             * {@link BTGattChar} event listener for notification and indication events.
             * <p>
             * This listener instance is attached to a BTGattChar via
             * {@link BTGattChar::addCharListener(std::shared_ptr<BTGattChar::Listener>)} or
             * {@link BTGattChar::addCharListener(std::shared_ptr<BTGattChar::Listener>, bool[])}
             * to listen to events associated with the BTGattChar instance.
             * </p>
             * <p>
             * The listener manager maintains a unique set of listener instances without duplicates.
             * </p>
             * <p>
             * Implementation will utilize a BTGattCharListener instance for the listener manager,
             * delegating matching BTGattChar events to this instance.
             * </p>
             */
            class Listener {
                public:
                    /**
                     * Called from native BLE stack, initiated by a received notification associated
                     * with the given {@link BTGattChar}.
                     * @param charDecl {@link BTGattChar} related to this notification
                     * @param charValue the notification value
                     * @param timestamp monotonic timestamp at reception, jau::getCurrentMilliseconds()
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

                    virtual ~Listener() noexcept {}

                    /**
                     * Default comparison operator, merely testing for same memory reference.
                     * <p>
                     * Specializations may override.
                     * </p>
                     */
                    virtual bool operator==(const Listener& rhs) const noexcept
                    { return this == &rhs; }

                    bool operator!=(const Listener& rhs) const noexcept
                    { return !(*this == rhs); }
            };

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
            int clientCharConfigIndex = -1;

            /* Optional Characteristic User Description index within descriptorList */
            int userDescriptionIndex = -1;

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

            std::shared_ptr<BTGattService> getServiceUnchecked() const noexcept { return wbr_service.lock(); }
            std::shared_ptr<BTGattService> getServiceChecked() const;
            std::shared_ptr<BTGattHandler> getGattHandlerUnchecked() const noexcept;
            std::shared_ptr<BTGattHandler> getGattHandlerChecked() const;
            std::shared_ptr<BTDevice> getDeviceUnchecked() const noexcept;
            std::shared_ptr<BTDevice> getDeviceChecked() const;

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

            BTGattDescRef getClientCharConfig() const noexcept {
                if( 0 > clientCharConfigIndex ) {
                    return nullptr;
                }
                return descriptorList.at(static_cast<size_t>(clientCharConfigIndex)); // abort if out of bounds
            }

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
             * @throws IllegalStateException if notification or indication is set to be enabled
             * and the {@link BTDevice's}'s {@link BTGattHandler} is null, i.e. not connected
             *
             * @see enableNotificationOrIndication()
             * @see disableIndicationNotification()
             */
            bool configNotificationIndication(const bool enableNotification, const bool enableIndication, bool enabledState[2]);

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
             * @throws IllegalStateException if notification or indication is set to be enabled
             * and the {@link BTDevice's}'s {@link BTGattHandler} is null, i.e. not connected
             *
             * @see configNotificationIndication()
             * @see disableIndicationNotification()
             */
            bool enableNotificationOrIndication(bool enabledState[2]);

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
             * @throws IllegalStateException if notification or indication is set to be enabled
             * and the {@link BTDevice's}'s {@link BTGattHandler} is null, i.e. not connected
             *
             * @see configNotificationIndication()
             * @see enableNotificationOrIndication()
             * @since 2.4.0
             */
            bool disableIndicationNotification();

            /**
             * Add the given BTGattChar::Listener to the listener list if not already present.
             *
             * Occurring notifications and indications for this characteristic,
             * if enabled via configNotificationIndication(bool, bool, bool[]) or enableNotificationOrIndication(bool[]),
             * will call the respective BTGattChar::Listener callback method.
             *
             * Returns true if the given listener is not element of the list and has been newly added,
             * otherwise false.
             *
             * Implementation wraps given BTGattChar::Listener into an AssociatedBTGattCharListener
             * to restrict the listener to listen only to this BTGattChar instance.
             *
             * Convenience delegation call to BTGattHandler via BTDevice
             *
             * @throws IllegalStateException if the {@link BTDevice's}'s {@link BTGattHandler} is null, i.e. not connected
             *
             * @see BTGattChar::disableIndicationNotification()
             * @see BTGattChar::enableNotificationOrIndication()
             * @see BTGattChar::configNotificationIndication()
             * @see BTGattChar::addCharListener()
             * @see BTGattChar::removeCharListener()
             * @see BTGattChar::removeAllAssociatedCharListener()
             */
            bool addCharListener(std::shared_ptr<Listener> l);

            /**
             * Add the given BTGattChar::Listener to the listener list if not already present
             * and if enabling the notification <i>or</i> indication for this characteristic at BLE level was successful.<br>
             * Notification and/or indication configuration is only performed per characteristic if changed.
             *
             * Implementation will enable notification if available,
             * otherwise indication will be enabled if available. <br>
             * Implementation uses enableNotificationOrIndication(bool[]) to enable either.
             *
             * Occurring notifications and indications for this characteristic
             * will call the respective BTGattChar::Listener callback method.
             *
             * Returns true if enabling the notification and/or indication was successful
             * and if the given listener is not element of the list and has been newly added,
             * otherwise false.
             *
             * Implementation wraps given BTGattChar::Listener into an AssociatedBTGattCharListener
             * to restrict the listener to listen only to this BTGattChar instance.
             *
             * @param enabledState array of size 2, holding the resulting enabled state for notification and indication
             * using enableNotificationOrIndication(bool[])
             *
             * @throws IllegalStateException if the {@link BTDevice's}'s {@link BTGattHandler} is null, i.e. not connected
             *
             * @see BTGattChar::disableIndicationNotification()
             * @see BTGattChar::enableNotificationOrIndication()
             * @see BTGattChar::configNotificationIndication()
             * @see BTGattChar::addCharListener()
             * @see BTGattChar::removeCharListener()
             * @see BTGattChar::removeAllAssociatedCharListener()
             */
            bool addCharListener(std::shared_ptr<Listener> l, bool enabledState[2]);

            /**
             * Remove the given associated BTGattChar::Listener from the listener list if present.
             *
             * To disables the notification and/or indication for this characteristic at BLE level
             * use disableIndicationNotification() when desired.
             *
             * @param l
             * @return
             *
             * @throws IllegalStateException if the {@link BTDevice's}'s {@link BTGattHandler} is null, i.e. not connected
             *
             * @see BTGattChar::disableIndicationNotification()
             * @see BTGattChar::enableNotificationOrIndication()
             * @see BTGattChar::configNotificationIndication()
             * @see BTGattChar::addCharListener()
             * @see BTGattChar::removeCharListener()
             * @see BTGattChar::removeAllAssociatedCharListener()
             * @since 2.4.0
             */
            bool removeCharListener(std::shared_ptr<Listener> l);

            /**
             * Removes all associated BTGattChar::Listener and and {@link BTGattCharListener} from the listener list.
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
            int removeAllAssociatedCharListener(bool shallDisableIndicationNotification);

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
            bool readValue(jau::POctets & res, int expectedLength=-1);

            /**
             * BT Core Spec v5.2: Vol 3, Part G GATT: 4.9.3 Write Characteristic Value
             * <p>
             * Convenience delegation call to BTGattHandler via BTDevice
             * <p>
             * </p>
             * If the BTDevice's BTGattHandler is null, i.e. not connected, an IllegalStateException is thrown.
             * </p>
             */
            bool writeValue(const jau::TROOctets & value);

            /**
             * BT Core Spec v5.2: Vol 3, Part G GATT: 4.9.1 Write Characteristic Value Without Response
             * <p>
             * Convenience delegation call to BTGattHandler via BTDevice
             * <p>
             * </p>
             * If the BTDevice's BTGattHandler is null, i.e. not connected, an IllegalStateException is thrown.
             * </p>
             */
            bool writeValueNoResp(const jau::TROOctets & value);
    };
    typedef std::shared_ptr<BTGattChar> BTGattCharRef;

    inline bool operator==(const BTGattChar& lhs, const BTGattChar& rhs) noexcept
    { return lhs.handle == rhs.handle; /** unique attribute handles */ }

    inline bool operator!=(const BTGattChar& lhs, const BTGattChar& rhs) noexcept
    { return !(lhs == rhs); }

    /**
     * {@link BTGattChar} event listener for notification and indication events.
     * <p>
     * A listener instance may be attached to a BTGattChar instance via
     * {@link BTGattChar::addCharListener(std::shared_ptr<BTGattChar::Listener>)} to listen to its events.
     * </p>
     * <p>
     * A listener instance may be attached to a BTGattHandler via
     * {@link BTGattHandler::addCharListener(std::shared_ptr<BTGattCharListener>)}
     * to listen to all events of the device or the matching filtered events.
     * </p>
     * <p>
     * User may utilize {@link AssociatedBTGattCharListener} to listen to only one {@link BTGattChar}.
     * </p>
     * <p>
     * The listener manager maintains a unique set of listener instances without duplicates.
     * </p>
     */
    class BTGattCharListener {
        public:
            /**
             * Returns a unique string denominating the type of this instance.
             *
             * Simple access and provision of a typename string representation
             * at compile time like RTTI via jau::type_name_cue.
             */
            virtual const char * type_name() const noexcept;

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

            virtual ~BTGattCharListener() noexcept {}

            /** Return a simple description about this instance. */
            virtual std::string toString() {
                return std::string(type_name())+"["+jau::to_string(this)+"]";
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

    class AssociatedBTGattCharListener : public BTGattCharListener {
        private:
            const BTGattChar * associatedChar;

        public:
            /**
             * Passing the associated BTGattChar to filter out non matching events.
             */
            AssociatedBTGattCharListener(const BTGattChar * characteristicMatch) noexcept
            : associatedChar(characteristicMatch) { }

            const char * type_name() const noexcept override;

            bool match(const BTGattChar & characteristic) noexcept override {
                if( nullptr == associatedChar ) {
                    return true;
                }
                return *associatedChar == characteristic;
            }
    };

} // namespace direct_bt

#endif /* BT_GATT_CHARACTERISTIC_HPP_ */
