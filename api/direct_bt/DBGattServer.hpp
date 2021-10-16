/*
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2021 Gothel Software e.K.
 * Copyright (c) 2021 ZAFENA AB
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

#ifndef DB_GATT_SERVER_HPP_
#define DB_GATT_SERVER_HPP_

#include <cstring>
#include <string>
#include <memory>
#include <cstdint>

#include <mutex>
#include <atomic>

#include <jau/java_uplink.hpp>
#include <jau/octets.hpp>
#include <jau/darray.hpp>
#include <jau/cow_darray.hpp>
#include <jau/uuid.hpp>
#include <jau/dfa_utf8_decode.hpp>

#include "BTTypes0.hpp"
#include "ATTPDUTypes.hpp"

#include "BTTypes1.hpp"

// #include "BTGattDesc.hpp"
#include "BTGattChar.hpp"
// #include "BTGattService.hpp"


/**
 * - - - - - - - - - - - - - - -
 *
 * Module DBGattServer covering the GattServer elements:
 *
 * - BT Core Spec v5.2: Vol 3, Part G Generic Attribute Protocol (GATT)
 * - BT Core Spec v5.2: Vol 3, Part G GATT: 2.6 GATT Profile Hierarchy
 */
namespace direct_bt {

    /**
     * Representing a Gatt Characteristic Descriptor object from the ::GATTRole::Server perspective.
     *
     * BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3 Characteristic Descriptor
     *
     * @since 2.4.0
     */
    class DBGattDesc {
        public:
            /**
             * Characteristic Descriptor Handle
             * <p>
             * Attribute handles are unique for each device (server) (BT Core Spec v5.2: Vol 3, Part F Protocol..: 3.2.2 Attribute Handle).
             * </p>
             */
            uint16_t handle;

            /** Type of descriptor */
            std::shared_ptr<const jau::uuid_t> type;

            /* Characteristics Descriptor's Value */
            jau::POctets value;

            DBGattDesc(const std::shared_ptr<const jau::uuid_t>& type_,
                       const jau::TROOctets& value_) noexcept
            : handle(0), type(type_), value(value_) {}

            std::string toString() const noexcept {
                return "Desc[type 0x"+type->toString()+", handle "+jau::to_hexstring(handle)+", value["+value.toString()+"]]";
            }

            /** Value is uint16_t bitfield */
            bool isExtendedProperties() const noexcept { return *BTGattDesc::TYPE_EXT_PROP == *type; }

            /* BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.3 Client Characteristic Configuration (Characteristic Descriptor, optional, single, uint16_t bitfield) */
            bool isClientCharConfig() const noexcept{ return *BTGattDesc::TYPE_CCC_DESC == *type; }

            /* BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.2 Characteristic User Description */
            bool isUserDescription() const noexcept{ return *BTGattDesc::TYPE_USER_DESC == *type; }
    };
    inline bool operator==(const DBGattDesc& lhs, const DBGattDesc& rhs) noexcept
    { return lhs.handle == rhs.handle; /** unique attribute handles */ }

    inline bool operator!=(const DBGattDesc& lhs, const DBGattDesc& rhs) noexcept
    { return !(lhs == rhs); }

    /**
     * Representing a Gatt Characteristic object from the ::GATTRole::Server perspective.
     *
     * BT Core Spec v5.2: Vol 3, Part G GATT: 3.3 Characteristic Definition
     *
     * handle -> CDAV value
     *
     * BT Core Spec v5.2: Vol 3, Part G GATT: 4.6.1 Discover All Characteristics of a Service
     *
     * The handle represents a service's characteristics-declaration
     * and the value the Characteristics Property, Characteristics Value Handle _and_ Characteristics UUID.
     *
     * @since 2.4.0
     */
    class DBGattChar {
        private:
            bool enabledNotifyState = false;
            bool enabledIndicateState = false;

        public:
            /**
             * Characteristic Handle of this instance.
             * <p>
             * Attribute handles are unique for each device (server) (BT Core Spec v5.2: Vol 3, Part F Protocol..: 3.2.2 Attribute Handle).
             * </p>
             */
            uint16_t handle;

            /**
             * Characteristic end handle, inclusive.
             * <p>
             * Attribute handles are unique for each device (server) (BT Core Spec v5.2: Vol 3, Part F Protocol..: 3.2.2 Attribute Handle).
             * </p>
             */
            uint16_t end_handle;

            /**
             * Characteristics Value Handle.
             * <p>
             * Attribute handles are unique for each device (server) (BT Core Spec v5.2: Vol 3, Part F Protocol..: 3.2.2 Attribute Handle).
             * </p>
             */
            uint16_t value_handle;

            /* Characteristics Value Type UUID */
            std::shared_ptr<const jau::uuid_t> value_type;

            /* Characteristics Property */
            BTGattChar::PropertyBitVal properties;

            /** List of Characteristic Descriptions. */
            jau::darray<DBGattDesc> descriptors;

            /* Characteristics's Value */
            jau::POctets value;

            /* Optional Client Characteristic Configuration index within descriptorList */
            int clientCharConfigIndex;

            /* Optional Characteristic User Description index within descriptorList */
            int userDescriptionIndex;

            DBGattChar(const std::shared_ptr<const jau::uuid_t>& value_type_,
                       const BTGattChar::PropertyBitVal properties_,
                       const jau::darray<DBGattDesc>& descriptors_,
                       const jau::TROOctets & value_) noexcept
            : handle(0), end_handle(0), value_handle(0),
              value_type(value_type_),
              properties(properties_),
              descriptors(descriptors_),
              value(value_),
              clientCharConfigIndex(-1),
              userDescriptionIndex(-1)
            {
                int i=0;
                // C++11: Range-based for loop: [begin, end[
                for(DBGattDesc& d : descriptors) {
                    if( 0 > clientCharConfigIndex && d.isClientCharConfig() ) {
                        clientCharConfigIndex=i;
                    } else if( 0 > userDescriptionIndex && d.isUserDescription() ) {
                        userDescriptionIndex=i;
                    }
                    ++i;
                }
            }

            bool hasProperties(const BTGattChar::PropertyBitVal v) const noexcept { return v == ( properties & v ); }

            std::string toString() const noexcept {
                std::string char_name, notify_str;
                {
                    const DBGattDesc* ud = getUserDescription();
                    if( nullptr != ud ) {
                        char_name = ", '" + jau::dfa_utf8_decode( ud->value.get_ptr(), ud->value.size() ) + "'";
                    }
                }
                if( hasProperties(BTGattChar::PropertyBitVal::Notify) || hasProperties(BTGattChar::PropertyBitVal::Indicate) ) {
                    notify_str = ", enabled[notify "+std::to_string(enabledNotifyState)+", indicate "+std::to_string(enabledIndicateState)+"]";
                }
                return "Char[handle ["+jau::to_hexstring(handle)+".."+jau::to_hexstring(end_handle)+
                       "], props "+jau::to_hexstring(properties)+" "+to_string(properties)+
                       char_name+", value[type 0x"+value_type->toString()+", handle "+jau::to_hexstring(value_handle)+", "+value.toString()+
                       "], ccd-idx "+std::to_string(clientCharConfigIndex)+notify_str+"]";
            }

            const DBGattDesc* getClientCharConfig() const noexcept {
                if( 0 > clientCharConfigIndex ) {
                    return nullptr;
                }
                return &descriptors.at(static_cast<size_t>(clientCharConfigIndex)); // abort if out of bounds
            }

            const DBGattDesc* getUserDescription() const noexcept {
                if( 0 > userDescriptionIndex ) {
                    return nullptr;
                }
                return &descriptors.at(static_cast<size_t>(userDescriptionIndex)); // abort if out of bounds
            }
    };
    inline bool operator==(const DBGattChar& lhs, const DBGattChar& rhs) noexcept
    { return lhs.handle == rhs.handle; /** unique attribute handles */ }

    inline bool operator!=(const DBGattChar& lhs, const DBGattChar& rhs) noexcept
    { return !(lhs == rhs); }

    /**
     * Representing a Gatt Service object from the ::GATTRole::Server perspective.
     *
     * BT Core Spec v5.2: Vol 3, Part G GATT: 3.1 Service Definition
     *
     * Includes a complete [Primary] Service Declaration
     * including its list of Characteristic Declarations,
     * which also may include its client config if available.
     *
     * @since 2.4.0
     */
    class DBGattService {
        public:
            /**
             * Indicate whether this service is a primary service.
             */
            bool primary;

            /**
             * Service start handle
             * <p>
             * Attribute handles are unique for each device (server) (BT Core Spec v5.2: Vol 3, Part F Protocol..: 3.2.2 Attribute Handle).
             * </p>
             */
            uint16_t handle;

            /**
             * Service end handle, inclusive.
             * <p>
             * Attribute handles are unique for each device (server) (BT Core Spec v5.2: Vol 3, Part F Protocol..: 3.2.2 Attribute Handle).
             * </p>
             */
            uint16_t end_handle;

            /** Service type UUID */
            std::shared_ptr<const jau::uuid_t> type;

            /** List of Characteristic Declarations. */
            jau::darray<DBGattChar> characteristics;

            DBGattService(const bool primary_,
                          const std::shared_ptr<const jau::uuid_t>& type_,
                          const jau::darray<DBGattChar>& characteristics_) noexcept
            : primary(primary_), handle(0), end_handle(0),
              type(type_),
              characteristics(characteristics_)
            { }

            DBGattChar* findGattChar(const jau::uuid_t& char_uuid) noexcept {
                for(DBGattChar& c : characteristics) {
                    if( char_uuid.equivalent( *c.value_type ) ) {
                        return &c;
                    }
                }
                return nullptr;
            }

            /**
             * Sets all handles of this service instance and all its owned childs,
             * i.e. DBGattChars elements and its DBGattDesc elements.
             *
             * @param start_handle a valid and unique start handle number > 0
             * @return number of set handles, i.e. `( end_handle - handle ) + 1`
             */
            int setHandles(const uint16_t start_handle) {
                if( 0 == start_handle ) {
                    handle = 0;
                    end_handle = 0;
                    return 0;
                }
                uint16_t h = start_handle;
                handle = h++;
                for(DBGattChar& c : characteristics) {
                    c.handle = h++;
                    c.value_handle = h++;
                    for(DBGattDesc& d : c.descriptors) {
                        d.handle = h++;
                    }
                    c.end_handle = h-1;
                }
                end_handle = h-1;
                return ( end_handle - handle ) + 1;
            }
            std::string toString() const noexcept {
                return "Srvc[type 0x"+type->toString()+", handle ["+jau::to_hexstring(handle)+".."+jau::to_hexstring(end_handle)+"], "+
                       std::to_string(characteristics.size())+" chars]";

            }
    };
    inline bool operator==(const DBGattService& lhs, const DBGattService& rhs) noexcept
    { return lhs.handle == rhs.handle && lhs.end_handle == rhs.end_handle; /** unique attribute handles */ }

    inline bool operator!=(const DBGattService& lhs, const DBGattService& rhs) noexcept
    { return !(lhs == rhs); }

    /**
     * Representing a complete list of Gatt Service objects from the ::GATTRole::Server perspective,
     * i.e. the Gatt Server database.
     *
     * One instance shall be attached to BTAdapter and hence BTGattHandler
     * when operating in Gatt Server mode, i.e. ::GATTRole::Server.
     *
     * This class is not thread safe and only intended to be prepared
     * by the user at startup and processed by the Gatt Server facility.
     *
     * @since 2.4.0
     */
    class DBGattServer {
        public:
            /**
             * Listener to remote master device's operations on the local GATT-Server.
             */
            class Listener {
                public:
                    virtual ~Listener() {}

                    /**
                     *
                     * @param device
                     * @param s
                     * @param c
                     * @return true if master read has been accepted by GATT-Server listener, otherwise false. Only if all listener return true, the read action will be allowed.
                     */
                    virtual bool readCharValue(std::shared_ptr<BTDevice> device, DBGattService& s, DBGattChar& c) = 0;

                    /**
                     *
                     * @param device
                     * @param s
                     * @param c
                     * @param d
                     * @return true if master read has been accepted by GATT-Server listener, otherwise false. Only if all listener return true, the read action will be allowed.
                     */
                    virtual bool readDescValue(std::shared_ptr<BTDevice> device, DBGattService& s, DBGattChar& c, DBGattDesc& d) = 0;

                    /**
                     *
                     * @param device
                     * @param s
                     * @param c
                     * @param value
                     * @return true if master write has been accepted by GATT-Server listener, otherwise false. Only if all listener return true, the write action will be allowed.
                     */
                    virtual bool writeCharValue(std::shared_ptr<BTDevice> device, DBGattService& s, DBGattChar& c, const jau::TROOctets & value) = 0;

                    /**
                     *
                     * @param device
                     * @param s
                     * @param c
                     * @param d
                     * @param value
                     * @return true if master write has been accepted by GATT-Server listener, otherwise false. Only if all listener return true, the write action will be allowed.
                     */
                    virtual bool writeDescValue(std::shared_ptr<BTDevice> device, DBGattService& s, DBGattChar& c, DBGattDesc& d, const jau::TROOctets & value) = 0;

                    /**
                     * Default comparison operator, merely testing for same memory reference.
                     * <p>
                     * Specializations may override.
                     * </p>
                     */
                    virtual bool operator==(const Listener& rhs) const
                    { return this == &rhs; }

                    bool operator!=(const Listener& rhs) const
                    { return !(*this == rhs); }
            };
            typedef std::shared_ptr<Listener> ListenerRef;

        private:
            typedef jau::cow_darray<ListenerRef> ListenerList_t;
            ListenerList_t listenerList;

        public:
            /** List of Services */
            jau::darray<DBGattService> services;

            DBGattServer()
            : services()
            { }

            DBGattServer(jau::darray<DBGattService>& service_)
            : services(service_)
            { }

            DBGattServer(std::initializer_list<DBGattService> initlist)
            : services(initlist)
            { }

            DBGattService* findGattService(const jau::uuid_t& type) noexcept {
                for(DBGattService& s : services) {
                    if( type.equivalent( *s.type ) ) {
                        return &s;
                    }
                }
                return nullptr;
            }
            DBGattChar* findGattChar(const jau::uuid_t&  service_uuid, const jau::uuid_t& char_uuid) noexcept {
                DBGattService* service = findGattService(service_uuid);
                if( nullptr == service ) {
                    return nullptr;
                }
                return service->findGattChar(char_uuid);
            }

            bool addService(DBGattService& s) noexcept {
                if( nullptr != findGattService(*s.type) ) {
                    // already shared
                    return false;
                }
                services.push_back(s);
                return true;
            }

            /**
             * Sets all handles of all service instances and all its owned childs,
             * i.e. DBGattChars elements and its DBGattDesc elements.
             *
             * Start handle is `1`.
             *
             * Method is being called by BTAdapter when advertising is enabled
             * via BTAdapter::startAdvertising().
             *
             * @return number of set handles, i.e. `( end_handle - handle ) + 1`
             * @see BTAdapter::startAdvertising()
             */
            int setServicesHandles() {
                int c = 0;
                uint16_t h = 1;
                for(DBGattService& s : services) {
                    int l = s.setHandles(h);
                    c += l;
                    h += l; // end + 1 for next service
                }
                return c;
            }

            bool addListener(ListenerRef l);
            bool removeListener(ListenerRef l);
            bool removeListener(const Listener * l);
            jau::cow_darray<ListenerRef>& listener() { return listenerList; }

            std::string toFullString() {
                std::string res = toString()+"\n";
                for(DBGattService& s : services) {
                    res.append("  ").append(s.toString()).append("\n");
                    for(DBGattChar& c : s.characteristics) {
                        res.append("    ").append(c.toString()).append("\n");
                        for(DBGattDesc& d : c.descriptors) {
                            res.append("      ").append(d.toString()).append("\n");
                        }
                    }
                }
                return res;
            }
            std::string toString() const noexcept {
                return "DBSrv["+std::to_string(services.size())+" services]";

            }
    };
    typedef std::shared_ptr<DBGattServer> DBGattServerRef;

} // namespace direct_bt

#endif /* DB_GATT_SERVER_HPP_ */
