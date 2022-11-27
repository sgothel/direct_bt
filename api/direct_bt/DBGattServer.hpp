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
#include "jau/int_types.hpp"
// #include "BTGattService.hpp"

// #define JAU_TRACE_DBGATT 1
#ifdef JAU_TRACE_DBGATT
    #define JAU_TRACE_DBGATT_PRINT(...) { fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); fflush(stderr); }
#else
    #define JAU_TRACE_DBGATT_PRINT(...)
#endif

/**
 * - - - - - - - - - - - - - - -
 *
 * Module DBGattServer covering the GattServer elements:
 *
 * - BT Core Spec v5.2: Vol 3, Part G Generic Attribute Protocol (GATT)
 * - BT Core Spec v5.2: Vol 3, Part G GATT: 2.6 GATT Profile Hierarchy
 */
namespace direct_bt {

    class BTDevice; // forward
    typedef std::shared_ptr<BTDevice> BTDeviceRef;

    /** @defgroup DBTUserServerAPI Direct-BT Peripheral-Server User Level API
     *  User level Direct-BT API types and functionality addressing the peripheral-server ::GATTRole::Server perspective,
     *  , [see Direct-BT Overview](namespacedirect__bt.html#details).
     *
     *  @{
     */

    class DBGattService; // fwd

    /**
     * Representing a Gatt Characteristic Descriptor object from the ::GATTRole::Server perspective.
     *
     * A list of shared DBGattDesc instances are passed at DBGattChar construction
     * and are retrievable via DBGattChar::getDescriptors().
     *
     * See [Direct-BT Overview](namespacedirect__bt.html#details).
     *
     * BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3 Characteristic Descriptor
     *
     * @since 2.4.0
     */
    class DBGattDesc : public jau::jni::JavaUplink {
        private:
            friend DBGattService;

            uint16_t handle;

            std::shared_ptr<const jau::uuid_t> type;

            jau::POctets value;

            bool variable_length;

        public:
            /**
             * Characteristic Descriptor Handle
             * <p>
             * Attribute handles are unique for each device (server) (BT Core Spec v5.2: Vol 3, Part F Protocol..: 3.2.2 Attribute Handle).
             * </p>
             */
            uint16_t getHandle() const noexcept { return handle; }

            /** Type of descriptor */
            std::shared_ptr<const jau::uuid_t>& getType() noexcept { return type; }

            /** Type of descriptor */
            std::shared_ptr<const jau::uuid_t> getType() const noexcept { return type; }

            /**
             * True if value is of variable length, otherwise fixed length.
             */
            bool hasVariableLength() const noexcept { return variable_length; }

            /**
             * Get reference of this characteristic descriptor's value.
             *
             * Its capacity defines the maximum writable variable length
             * and its size defines the maximum writable fixed length.
             */
            jau::POctets& getValue() noexcept { return value; }

            /**
             * Set this characteristic descriptor's value.
             *
             * Methods won't exceed the value's capacity if it is of hasVariableLength()
             * or the value's size otherwise.
             *
             * @param source data to be written to this value
             * @param source_len length of the source data to be written
             * @param dest_pos position where the source data shall be written to the value
             * @return true if successful, otherwise false for exceeding the value's limits.
             */
            bool setValue(const uint8_t* source, const jau::nsize_t source_len, const jau::nsize_t dest_pos) noexcept;

            static std::string java_class() noexcept {
                return std::string(JAVA_MAIN_PACKAGE "DBGattDesc");
            }

            std::string get_java_class() const noexcept override {
                return java_class();
            }

            ~DBGattDesc() noexcept override {
                JAU_TRACE_DBGATT_PRINT("DBGattDesc dtor0: %p", this);
            }

            /**
             *
             * @param type_
             * @param value_ the data, which length defines the maximum writable fixed length if variable_length is false.
             *        If variable length is true, its capacity limits the maximum writable length.
             *        Forced to false if isExtendedProperties() or isClientCharConfig().
             * @param variable_length_ defaults to false.
             */
            DBGattDesc(std::shared_ptr<const jau::uuid_t> type_,
                       jau::POctets && value_, bool variable_length_=false) noexcept
            : handle(0), type(std::move(type_)), value( std::move( value_ ) ), variable_length(variable_length_)
            {
                if( variable_length && ( isExtendedProperties() || isClientCharConfig() ) ) {
                    variable_length = false;
                }
                JAU_TRACE_DBGATT_PRINT("DBGattDesc ctor0, value-move: %p", this);
            }

            /**
             * Copy constructor preserving the capacity of the value
             * @param o
             */
            DBGattDesc(const DBGattDesc &o)
            : jau::jni::JavaUplink(o),
              handle(o.handle), type(o.type),
              value(o.value, o.value.capacity()),
              variable_length(o.variable_length)
            {
                JAU_TRACE_DBGATT_PRINT("DBGattDesc ctor-cpy0: %p -> %p", &o, this);
            }

            /**
             * Move constructor
             * @param o POctet source to be taken over
             */
            DBGattDesc(DBGattDesc &&o) noexcept
            : handle(std::move(o.handle)), type(std::move(o.type)),
              value(std::move(o.value)),
              variable_length(std::move(o.variable_length))
            {
                JAU_TRACE_DBGATT_PRINT("DBGattDesc ctor-move0: %p -> %p", &o, this);
            }

            /**
             * Return a newly constructed Client Characteristic Configuration
             * with a zero uint16_t value of fixed length.
             * @see isClientCharConfig()
             */
            static std::shared_ptr<DBGattDesc> createClientCharConfig() {
                jau::POctets p( 2, jau::endian::little);
                p.put_uint16_nc(0, 0);
                return std::make_shared<DBGattDesc>( BTGattDesc::TYPE_CCC_DESC, std::move(p), false /* variable_length */ );
            }

            /** Fill value with zero bytes. */
            void bzero() noexcept {
                value.bzero();
            }

            /** Value is uint16_t bitfield */
            bool isExtendedProperties() const noexcept { return *BTGattDesc::TYPE_EXT_PROP == *type; }

            /* BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.3 Client Characteristic Configuration (Characteristic Descriptor, optional, single, uint16_t bitfield) */
            bool isClientCharConfig() const noexcept{ return *BTGattDesc::TYPE_CCC_DESC == *type; }

            /* BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.2 Characteristic User Description */
            bool isUserDescription() const noexcept{ return *BTGattDesc::TYPE_USER_DESC == *type; }

            std::string toString() const noexcept override {
                const std::string len = variable_length ? "var" : "fixed";
                return "Desc[type 0x"+type->toString()+", handle "+jau::to_hexstring(handle)+
                       ", value[len "+len+
                       ", "+value.toString()+
                       " '" + jau::dfa_utf8_decode( value.get_ptr(), value.size() ) + "'"+
                       "], "+javaObjectToString()+"]";
            }
    };
    inline bool operator==(const DBGattDesc& lhs, const DBGattDesc& rhs) noexcept
    { return lhs.getHandle() == rhs.getHandle(); /** unique attribute handles */ }

    inline bool operator!=(const DBGattDesc& lhs, const DBGattDesc& rhs) noexcept
    { return !(lhs == rhs); }

    typedef std::shared_ptr<DBGattDesc> DBGattDescRef;

    /**
     * Representing a Gatt Characteristic object from the ::GATTRole::Server perspective.
     *
     * A list of shared DBGattChar instances are passed at DBGattService construction
     * and are retrievable via DBGattService::getCharacteristics().
     *
     * See [Direct-BT Overview](namespacedirect__bt.html#details).
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
    class DBGattChar : public jau::jni::JavaUplink {
        private:
            friend DBGattService;

            bool enabledNotifyState = false;
            bool enabledIndicateState = false;

            uint16_t handle;

            uint16_t end_handle;

            uint16_t value_handle;

            std::shared_ptr<const jau::uuid_t> value_type;

            BTGattChar::PropertyBitVal properties;

            jau::darray<DBGattDescRef> descriptors;

            jau::POctets value;

            bool variable_length;

            int clientCharConfigIndex;

            int userDescriptionIndex;

        public:
            /**
             * Characteristic Handle of this instance.
             * <p>
             * Attribute handles are unique for each device (server) (BT Core Spec v5.2: Vol 3, Part F Protocol..: 3.2.2 Attribute Handle).
             * </p>
             */
            uint16_t getHandle() const noexcept { return handle; }

            /**
             * Characteristic end handle, inclusive.
             * <p>
             * Attribute handles are unique for each device (server) (BT Core Spec v5.2: Vol 3, Part F Protocol..: 3.2.2 Attribute Handle).
             * </p>
             */
            uint16_t getEndHandle() const noexcept { return end_handle; }

            /**
             * Characteristics Value Handle.
             * <p>
             * Attribute handles are unique for each device (server) (BT Core Spec v5.2: Vol 3, Part F Protocol..: 3.2.2 Attribute Handle).
             * </p>
             */
            uint16_t getValueHandle() const noexcept { return value_handle; }

            /* Characteristics Value Type UUID */
            std::shared_ptr<const jau::uuid_t>& getValueType() noexcept { return value_type; }

            /* Characteristics Value Type UUID */
            std::shared_ptr<const jau::uuid_t> getValueType() const noexcept { return value_type; }

            /* Characteristics Property */
            BTGattChar::PropertyBitVal getProperties() const noexcept { return properties; }

            /** List of Characteristic Descriptions. */
            jau::darray<DBGattDescRef>& getDescriptors() noexcept { return descriptors; }

            /**
             * True if value is of variable length, otherwise fixed length.
             */
            bool hasVariableLength() const noexcept { return variable_length; }

            /**
             * Get reference of this characteristics's value.
             *
             * Its capacity defines the maximum writable variable length
             * and its size defines the maximum writable fixed length.
             */
            jau::POctets& getValue() noexcept { return value; }

            /**
             * Set this characteristics's value.
             *
             * Methods won't exceed the value's capacity if it is of hasVariableLength()
             * or the value's size otherwise.
             *
             * @param source data to be written to this value
             * @param source_len length of the source data to be written
             * @param dest_pos position where the source data shall be written to the value
             * @return true if successful, otherwise false for exceeding the value's limits.
             */
            bool setValue(const uint8_t* source, const jau::nsize_t source_len, const jau::nsize_t dest_pos) noexcept;

            /* Optional Client Characteristic Configuration index within descriptorList */
            int getClientCharConfigIndex() const noexcept { return clientCharConfigIndex; }

            /* Optional Characteristic User Description index within descriptorList */
            int getUserDescriptionIndex() const noexcept { return userDescriptionIndex; }

            static std::string java_class() noexcept {
                return std::string(JAVA_MAIN_PACKAGE "DBGattChar");
            }

            std::string get_java_class() const noexcept override {
                return java_class();
            }

            ~DBGattChar() noexcept override {
                JAU_TRACE_DBGATT_PRINT("DBGattChar dtor0: %p", this);
            }

            /**
             *
             * @param value_type_
             * @param properties_
             * @param descriptors_
             * @param value_ the data, which length defines the maximum writable fixed length if variable_length is false.
             *        If variable length is true, its capacity limits the maximum writable length.
             * @param variable_length_ defaults to false.
             *
             */
            DBGattChar(const std::shared_ptr<const jau::uuid_t>& value_type_,
                       const BTGattChar::PropertyBitVal properties_,
                       jau::darray<DBGattDescRef> && descriptors_,
                       jau::POctets && value_, bool variable_length_=false) noexcept
            : handle(0), end_handle(0), value_handle(0),
              value_type(value_type_),
              properties(properties_),
              descriptors( std::move( descriptors_ ) ),
              value( std::move( value_ ) ), variable_length(variable_length_),
              clientCharConfigIndex(-1),
              userDescriptionIndex(-1)
            {
                int i=0;
                // C++11: Range-based for loop: [begin, end[
                for(const DBGattDescRef& d : descriptors) {
                    if( 0 > clientCharConfigIndex && d->isClientCharConfig() ) {
                        clientCharConfigIndex=i;
                    } else if( 0 > userDescriptionIndex && d->isUserDescription() ) {
                        userDescriptionIndex=i;
                    }
                    ++i;
                }
                JAU_TRACE_DBGATT_PRINT("DBGattChar: ctor0, descr-move: %p", this);
            }

            /**
             * Copy constructor preserving the capacity of the value
             * @param o
             */
            DBGattChar(const DBGattChar &o)
            : jau::jni::JavaUplink(o),
              handle(o.handle), end_handle(o.end_handle),
              value_handle(o.value_handle), value_type(o.value_type),
              properties(o.properties),
              descriptors(o.descriptors),
              value(o.value, o.value.capacity()),
              variable_length(o.variable_length),
              clientCharConfigIndex(o.clientCharConfigIndex),
              userDescriptionIndex(o.userDescriptionIndex)
            {
                JAU_TRACE_DBGATT_PRINT("DBGattChar: ctor-copy0: %p -> %p", &o, this);
            }

            /**
             * Move constructor
             * @param o POctet source to be taken over
             */
            DBGattChar(DBGattChar &&o) noexcept
            : handle(std::move(o.handle)), end_handle(std::move(o.end_handle)),
              value_handle(std::move(o.value_handle)), value_type(std::move(o.value_type)),
              properties(std::move(o.properties)),
              descriptors(std::move(o.descriptors)),
              value(std::move(o.value)),
              variable_length(std::move(o.variable_length)),
              clientCharConfigIndex(std::move(o.clientCharConfigIndex)),
              userDescriptionIndex(std::move(o.userDescriptionIndex))
            {
                JAU_TRACE_DBGATT_PRINT("DBGattChar: ctor-move0: %p -> %p", &o, this);
            }

            bool hasProperties(const BTGattChar::PropertyBitVal v) const noexcept { return v == ( properties & v ); }

            /** Fill value with zero bytes. */
            void bzero() noexcept {
                value.bzero();
            }

            const DBGattDescRef getClientCharConfig() const noexcept {
                if( 0 > clientCharConfigIndex ) {
                    return nullptr;
                }
                return descriptors.at(static_cast<size_t>(clientCharConfigIndex)); // abort if out of bounds
            }

            const DBGattDescRef getUserDescription() const noexcept {
                if( 0 > userDescriptionIndex ) {
                    return nullptr;
                }
                return descriptors.at(static_cast<size_t>(userDescriptionIndex)); // abort if out of bounds
            }
            DBGattDescRef getClientCharConfig() noexcept {
                if( 0 > clientCharConfigIndex ) {
                    return nullptr;
                }
                return descriptors.at(static_cast<size_t>(clientCharConfigIndex)); // abort if out of bounds
            }

            DBGattDescRef getUserDescription() noexcept {
                if( 0 > userDescriptionIndex ) {
                    return nullptr;
                }
                return descriptors.at(static_cast<size_t>(userDescriptionIndex)); // abort if out of bounds
            }

            std::string toString() const noexcept override {
                std::string char_name, notify_str;
                {
                    const DBGattDescRef ud = getUserDescription();
                    if( nullptr != ud ) {
                        char_name = ", '" + jau::dfa_utf8_decode( ud->getValue().get_ptr(), ud->getValue().size() ) + "'";
                    }
                }
                if( hasProperties(BTGattChar::PropertyBitVal::Notify) || hasProperties(BTGattChar::PropertyBitVal::Indicate) ) {
                    notify_str = ", enabled[notify "+std::to_string(enabledNotifyState)+", indicate "+std::to_string(enabledIndicateState)+"]";
                }
                const std::string len = variable_length ? "var" : "fixed";
                return "Char[handle ["+jau::to_hexstring(handle)+".."+jau::to_hexstring(end_handle)+
                       "], props "+jau::to_hexstring(properties)+" "+to_string(properties)+
                       char_name+", value[type 0x"+value_type->toString()+", handle "+jau::to_hexstring(value_handle)+", len "+len+
                       ", "+value.toString()+
                       " '" + jau::dfa_utf8_decode( value.get_ptr(), value.size() ) + "'"+
                       "], ccd-idx "+std::to_string(clientCharConfigIndex)+notify_str+
                       ", "+javaObjectToString()+"]";
            }
    };
    inline bool operator==(const DBGattChar& lhs, const DBGattChar& rhs) noexcept
    { return lhs.getHandle() == rhs.getHandle() && lhs.getEndHandle() == rhs.getEndHandle(); /** unique attribute handles */ }

    inline bool operator!=(const DBGattChar& lhs, const DBGattChar& rhs) noexcept
    { return !(lhs == rhs); }

    typedef std::shared_ptr<DBGattChar> DBGattCharRef;

    /**
     * Representing a Gatt Service object from the ::GATTRole::Server perspective.
     *
     * A list of shared DBGattService instances are passed at DBGattServer construction
     * and are retrievable via DBGattServer::getServices().
     *
     * See [Direct-BT Overview](namespacedirect__bt.html#details).
     *
     * BT Core Spec v5.2: Vol 3, Part G GATT: 3.1 Service Definition
     *
     * Includes a complete [Primary] Service Declaration
     * including its list of Characteristic Declarations,
     * which also may include its client config if available.
     *
     * @since 2.4.0
     */
    class DBGattService : public jau::jni::JavaUplink {
        private:
            bool primary;

            uint16_t handle;

            uint16_t end_handle;

            std::shared_ptr<const jau::uuid_t> type;

            jau::darray<DBGattCharRef> characteristics;

        public:
            /**
             * Indicate whether this service is a primary service.
             */
            bool isPrimary() const noexcept { return primary; }

            /**
             * Service start handle
             * <p>
             * Attribute handles are unique for each device (server) (BT Core Spec v5.2: Vol 3, Part F Protocol..: 3.2.2 Attribute Handle).
             * </p>
             */
            uint16_t getHandle() const noexcept { return handle; }

            /**
             * Service end handle, inclusive.
             * <p>
             * Attribute handles are unique for each device (server) (BT Core Spec v5.2: Vol 3, Part F Protocol..: 3.2.2 Attribute Handle).
             * </p>
             */
            uint16_t getEndHandle() const noexcept { return end_handle; }

            /** Service type UUID */
            std::shared_ptr<const jau::uuid_t>& getType() noexcept { return type; }

            /** Service type UUID */
            std::shared_ptr<const jau::uuid_t> getType() const noexcept { return type; }

            /** List of Characteristic Declarations. */
            jau::darray<DBGattCharRef>& getCharacteristics() noexcept { return characteristics; }

            static std::string java_class() noexcept {
                return std::string(JAVA_MAIN_PACKAGE "DBGattService");
            }

            std::string get_java_class() const noexcept override {
                return java_class();
            }

            ~DBGattService() noexcept override {
                JAU_TRACE_DBGATT_PRINT("DBGattService dtor0: %p", this);
            }

            DBGattService(const bool primary_,
                          const std::shared_ptr<const jau::uuid_t>& type_,
                          jau::darray<DBGattCharRef> && characteristics_)
            : primary(primary_), handle(0), end_handle(0),
              type(type_),
              characteristics( std::move( characteristics_ ) )
            { }

            DBGattService(const DBGattService &o) = default;
            DBGattService(DBGattService &&o) noexcept = default;

            DBGattCharRef findGattChar(const jau::uuid_t& char_uuid) noexcept {
                for(DBGattCharRef& c : characteristics) {
                    if( char_uuid.equivalent( *c->value_type ) ) {
                        return c;
                    }
                }
                return nullptr;
            }
            DBGattCharRef findGattCharByValueHandle(const uint16_t char_value_handle) noexcept {
                for(DBGattCharRef& c : characteristics) {
                    if( char_value_handle == c->value_handle ) {
                        return c;
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
                for(DBGattCharRef& c : characteristics) {
                    c->handle = h++;
                    c->value_handle = h++;
                    for(DBGattDescRef& d : c->descriptors) {
                        d->handle = h++;
                    }
                    c->end_handle = h-1;
                }
                end_handle = h-1;
                return ( end_handle - handle ) + 1;
            }

            std::string toString() const noexcept override {
                return "Srvc[type 0x"+type->toString()+", handle ["+jau::to_hexstring(handle)+".."+jau::to_hexstring(end_handle)+"], "+
                       std::to_string(characteristics.size())+" chars, "+javaObjectToString()+"]";

            }
    };
    inline bool operator==(const DBGattService& lhs, const DBGattService& rhs) noexcept
    { return lhs.getHandle() == rhs.getHandle() && lhs.getEndHandle() == rhs.getEndHandle(); /** unique attribute handles */ }

    inline bool operator!=(const DBGattService& lhs, const DBGattService& rhs) noexcept
    { return !(lhs == rhs); }

    typedef std::shared_ptr<DBGattService> DBGattServiceRef;

    /**
     * Representing a complete list of Gatt Service objects from the ::GATTRole::Server perspective,
     * i.e. the Gatt Server database.
     *
     * One instance shall be attached to BTAdapter when advertising via BTAdapter::startAdvertising(),
     * changing its operating mode to Gatt Server mode, i.e. ::GATTRole::Server.
     *
     * The instance can also be retrieved via BTAdapter::getGATTServerData().
     *
     * See [Direct-BT Overview](namespacedirect__bt.html#details).
     *
     * This class is not thread safe and only intended to be prepared
     * by the user at startup and processed by the Gatt Server facility.
     *
     * @since 2.4.0
     */
    class DBGattServer : public jau::jni::JavaUplink {
        public:
            /**
             * Operating mode of a DBGattServer instance.
             */
            enum class Mode : uint8_t {
                /** No operation mode, i.e. w/o any DBGattService. */
                NOP = 0,

                /** Database mode, the default operating on given list of DBGattService. */
                DB = 1,

                /** Forward mode, acting as a proxy forwarding all requests to a 3rd remote GATT server BTDevice. */
                FWD = 2
            };
            static std::string toModeString(const Mode m) noexcept;

            /**
             * Listener to remote master device's operations on the local GATT-Server.
             *
             * All methods shall return as soon as possible to not block GATT event processing.
             */
            class Listener {
                public:
                    virtual ~Listener() = default;

                    /**
                     * Notification that device got connected.
                     *
                     * Convenient user entry, allowing to setup resources.
                     *
                     * @param device the connected device
                     * @param initialMTU initial used minimum MTU until negotiated.
                     */
                    virtual void connected(BTDeviceRef device, const uint16_t initialMTU) = 0;

                    /**
                     * Notification that device got disconnected.
                     *
                     * Convenient user entry, allowing to clean up resources.
                     *
                     * @param device the disconnected device.
                     */
                    virtual void disconnected(BTDeviceRef device) = 0;

                    /**
                     * Notification that the MTU has changed.
                     *
                     * @param device the device for which the MTU has changed
                     * @param mtu the new negotiated MTU
                     */
                    virtual void mtuChanged(BTDeviceRef device, const uint16_t mtu) = 0;

                    /**
                     * Signals attempt to read a value.
                     *
                     * Callee shall accept the read request by returning true, otherwise false.
                     *
                     * @param device
                     * @param s
                     * @param c
                     * @return true if master read has been accepted by GATT-Server listener, otherwise false. Only if all listener return true, the read action will be allowed.
                     */
                    virtual bool readCharValue(BTDeviceRef device, DBGattServiceRef s, DBGattCharRef c) = 0;

                    /**
                     * Signals attempt to read a value.
                     *
                     * Callee shall accept the read request by returning true, otherwise false.
                     *
                     * @param device
                     * @param s
                     * @param c
                     * @param d
                     * @return true if master read has been accepted by GATT-Server listener, otherwise false. Only if all listener return true, the read action will be allowed.
                     */
                    virtual bool readDescValue(BTDeviceRef device, DBGattServiceRef s, DBGattCharRef c, DBGattDescRef d) = 0;

                    /**
                     * Signals attempt to write a single or bulk (prepare) value.
                     *
                     * Callee shall accept the write request by returning true, otherwise false.
                     *
                     * @param device
                     * @param s
                     * @param c
                     * @param value
                     * @param value_offset
                     * @return true if master write has been accepted by GATT-Server listener, otherwise false. Only if all listener return true, the write action will be allowed.
                     * @see writeCharValueDone()
                     */
                    virtual bool writeCharValue(BTDeviceRef device, DBGattServiceRef s, DBGattCharRef c,
                                                const jau::TROOctets & value, const uint16_t value_offset) = 0;

                    /**
                     * Notifies completion of single or bulk writeCharValue() after having accepted and performed all write requests.
                     *
                     * @param device
                     * @param s
                     * @param c
                     * @see writeCharValue()
                     */
                    virtual void writeCharValueDone(BTDeviceRef device, DBGattServiceRef s, DBGattCharRef c) = 0;

                    /**
                     * Signals attempt to write a single or bulk (prepare) value.
                     *
                     * Callee shall accept the write request by returning true, otherwise false.
                     *
                     * @param device
                     * @param s
                     * @param c
                     * @param d
                     * @param value
                     * @param value_offset
                     * @return true if master write has been accepted by GATT-Server listener, otherwise false. Only if all listener return true, the write action will be allowed.
                     * @see writeDescValueDone()
                     */
                    virtual bool writeDescValue(BTDeviceRef device, DBGattServiceRef s, DBGattCharRef c, DBGattDescRef d,
                                                const jau::TROOctets & value, const uint16_t value_offset) = 0;

                    /**
                     * Notifies completion of single or bulk writeCharValue() after having accepted and performed all write requests.
                     *
                     * @param device
                     * @param s
                     * @param c
                     * @param d
                     * @see writeDescValue()
                     */
                    virtual void writeDescValueDone(BTDeviceRef device, DBGattServiceRef s, DBGattCharRef c, DBGattDescRef d) = 0;

                    /**
                     * Notifies a change of the Client Characteristic Configuration Descriptor (CCCD) value.
                     *
                     * @param device
                     * @param s
                     * @param c
                     * @param d
                     * @param notificationEnabled
                     * @param indicationEnabled
                     */
                    virtual void clientCharConfigChanged(BTDeviceRef device, DBGattServiceRef s, DBGattCharRef c, DBGattDescRef d,
                                                         const bool notificationEnabled, const bool indicationEnabled) = 0;

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

            typedef jau::nsize_t size_type;
            typedef jau::cow_darray<ListenerRef, size_type> ListenerList_t;
            typedef jau::darray<DBGattServiceRef, size_type> GattServiceList_t;

        private:            
            ListenerList_t listenerList;

            uint16_t max_att_mtu;

            GattServiceList_t services;

            BTDeviceRef fwdServer; // FWD mode

            Mode mode;

        public:
            /** Used maximum server Rx ATT_MTU, defaults to 512+1. */
            uint16_t getMaxAttMTU() const noexcept { return max_att_mtu; }

            /**
             * Set maximum server Rx ATT_MTU, defaults to 512+1 limit.
             *
             * Method can only be issued before passing instance to BTAdapter::startAdvertising()
             * @see BTAdapter::startAdvertising()
             */
            void setMaxAttMTU(const uint16_t v) noexcept { max_att_mtu = std::min<uint16_t>(512+1, v); }

            /** List of Services */
            jau::darray<DBGattServiceRef>& getServices() noexcept { return services; }

            Mode getMode() const noexcept { return mode; }

            BTDeviceRef getFwdServer() noexcept { return fwdServer; }

            static std::string java_class() noexcept {
                return std::string(JAVA_MAIN_PACKAGE "DBGattServer");
            }

            std::string get_java_class() const noexcept override {
                return java_class();
            }

            ~DBGattServer() noexcept override { // NOLINT(modernize-use-equals-default)
                #ifdef JAU_TRACE_DBGATT
                    JAU_TRACE_DBGATT_PRINT("DBGattServer dtor0: %p", this);
                    jau::print_backtrace(true);
                #endif
            }

            /**
             * DBGattServer in Mode::NOP mode.
             */
            DBGattServer()
            : max_att_mtu(512+1),
              services( ),
              fwdServer( nullptr ),
              mode(Mode::NOP)
            { }

            /**
             * DBGattServer in Mode::DB mode if given services_ are not empty, otherwise in Mode::NOP mode.
             * @param max_att_mtu_
             * @param services_
             */
            DBGattServer(uint16_t max_att_mtu_, jau::darray<DBGattServiceRef> && services_)
            : max_att_mtu(std::min<uint16_t>(512+1, max_att_mtu_)),
              services( std::move( services_ ) ),
              fwdServer( nullptr ),
              mode( services.size() > 0 ? Mode::DB : Mode::NOP )
            { }

            /**
             * DBGattServer in Mode::DB mode if given services_ are not empty, otherwise in Mode::NOP mode.
             *
             * Ctor using default maximum ATT_MTU of 512+1
             * @param services_
             */
            DBGattServer(jau::darray<DBGattServiceRef> && services_)
            : max_att_mtu(512+1),
              services( std::move( services_ ) ),
              fwdServer( nullptr ),
              mode( services.size() > 0 ? Mode::DB : Mode::NOP )
            { }

            /**
             * DBGattServer in Mode::FWD to the given forward BTDevice.
             *
             * @param fwdServer_
             */
            DBGattServer(BTDeviceRef fwdServer_)
            : max_att_mtu(512+1),
              services( ),
              fwdServer( fwdServer_ ),
              mode( Mode::FWD )
            { }

            DBGattServiceRef findGattService(const jau::uuid_t& type) noexcept {
                for(DBGattServiceRef& s : services) {
                    if( type.equivalent( *s->getType() ) ) {
                        return s;
                    }
                }
                return nullptr;
            }
            DBGattCharRef findGattChar(const jau::uuid_t&  service_uuid, const jau::uuid_t& char_uuid) noexcept {
                DBGattServiceRef service = findGattService(service_uuid);
                if( nullptr == service ) {
                    return nullptr;
                }
                return service->findGattChar(char_uuid);
            }
            DBGattDescRef findGattClientCharConfig(const jau::uuid_t&  service_uuid, const jau::uuid_t& char_uuid) noexcept {
                DBGattCharRef c = findGattChar(service_uuid, char_uuid);
                if( nullptr == c ) {
                    return nullptr;
                }
                return c->getClientCharConfig();
            }
            bool resetGattClientCharConfig(const jau::uuid_t&  service_uuid, const jau::uuid_t& char_uuid) noexcept {
                DBGattDescRef d = findGattClientCharConfig(service_uuid, char_uuid);
                if( nullptr == d ) {
                    return false;
                }
                d->bzero();
                return true;
            }

            DBGattCharRef findGattCharByValueHandle(const uint16_t char_value_handle) noexcept {
                for(DBGattServiceRef& s : services) {
                    DBGattCharRef r = s->findGattCharByValueHandle(char_value_handle);
                    if( nullptr != r ) {
                        return r;
                    }
                }
                return nullptr;
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
            size_type setServicesHandles() {
                size_type c = 0;
                uint16_t h = 1;
                for(DBGattServiceRef& s : services) {
                    int l = s->setHandles(h);
                    c += l;
                    h += l; // end + 1 for next service
                }
                return c;
            }

            bool addListener(ListenerRef l);
            bool removeListener(ListenerRef l);
            ListenerList_t& listener() { return listenerList; }

            std::string toFullString() {
                std::string res = toString()+"\n";
                for(DBGattServiceRef& s : services) {
                    res.append("  ").append(s->toString()).append("\n");
                    for(DBGattCharRef& c : s->getCharacteristics()) {
                        res.append("    ").append(c->toString()).append("\n");
                        for(DBGattDescRef& d : c->getDescriptors()) {
                            res.append("      ").append(d->toString()).append("\n");
                        }
                    }
                }
                return res;
            }

            std::string toString() const noexcept override;
    };
    typedef std::shared_ptr<DBGattServer> DBGattServerRef;

    std::string to_string(const DBGattServer::Mode m) noexcept;

    /**@}*/

} // namespace direct_bt

#endif /* DB_GATT_SERVER_HPP_ */
